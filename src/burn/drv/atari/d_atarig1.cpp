// FB Alpha Atari G1 system driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "atariic.h"
#include "atarijsa.h"
#include "atarirle.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvRLERAM;
static UINT8 *DrvPfRAM;
static UINT8 *DrvAlphaRAM;
static UINT8 *Drv68KRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 a2d_select;
static INT32 video_int_state;
static INT32 pf_tile_bank;

static INT32 scanline;
static INT32 vblank;
static INT32 pitfight = 0;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT16 DrvInputs[5];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;
static INT16 DrvAnalogPort2 = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo HydraInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"Coin 3",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},
	A("P1 Stick X",     BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Stick Y",     BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),
	A("P1 Accelerator", BIT_ANALOG_REL, &DrvAnalogPort2,"p1 fire 1"),
	{"P1 Right Trigger",BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Right Thumb",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 3"	},
	{"P1 Left Trigger",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 5"	},
	{"P1 Left Thumb",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 6"	},
	{"P1 Boost",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};
#undef A

STDINPUTINFO(Hydra)

static struct BurnInputInfo PitfightInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 8,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 11,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 1,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 0,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p3 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Pitfight)

static struct BurnDIPInfo HydraDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x40, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x0c, 0x01, 0x40, 0x40, "Off"			},
	{0x0c, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Hydra)

static struct BurnDIPInfo PitfightDIPList[]=
{
	{0x19, 0xff, 0xff, 0x40, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x19, 0x01, 0x40, 0x40, "Off"			},
	{0x19, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Pitfight)

static void update_interrupts()
{
	INT32 state = 0;
	if (video_int_state) state = 1;
	if (atarijsa_int_state) state = 2;

	if (state)
		SekSetIRQLine(state, CPU_IRQSTATUS_ACK);
	else
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
}

static void __fastcall atarig1_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xff8000) == 0xf88000) {
		AtariEEPROMUnlockWrite();
		return;
	}

	if (address >= 0xff0000 && address <= 0xff3000) {
		*((UINT16*)(DrvRLERAM + (address & 0x3ffe))) = data;
		INT32 moaddress = (address & 0x3fff) / 2;
		if (moaddress < 0x800)
			atarirle_0_spriteram_w(moaddress);

		if (address != 0xff2000) return;
	}

	switch (address)
	{
		case 0xf80000:
			BurnWatchdogWrite();
		return;

		case 0xf90000:
			AtariJSAWrite(data);
		return;

		case 0xf98000:
			AtariJSAResetWrite(data);
		return;

		case 0xfa0000:
		case 0xfa0001:
			atarirle_control_w(0,data,scanline);
		return;

		case 0xfb0000:
			video_int_state = 0;
			update_interrupts();
		return;

		case 0xfc8000:
		case 0xfc8002:
		case 0xfc8004:
		case 0xfc8006:
			a2d_select = (address & 0x6) >> 1;
		return;

		case 0xff2000:
			atarirle_command_w(0, (pitfight != 0 && data == 0) ? ATARIRLE_COMMAND_CHECKSUM : ATARIRLE_COMMAND_DRAW);
		return;
	}
}

static void __fastcall atarig1_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xff8000) == 0xf88000) {
		AtariEEPROMUnlockWrite();
		return;
	}

	if (address >= 0xff0000 && address <= 0xff3000) {
		INT32 moaddress = address & 0x3fff;
		DrvRLERAM[moaddress^1] = data;
		if (moaddress < 0x1000)
			atarirle_0_spriteram_w(moaddress/2);

		if (address != 0xff2000 && address != 0xff2001) return;
	}

	switch (address)
	{
		case 0xf80000:
		case 0xf80001:
			BurnWatchdogWrite();
		return;

		case 0xf90000:
		case 0xf90001:
			AtariJSAWrite(data);
		return;

		case 0xf98000:
		case 0xf98001:
			AtariJSAResetWrite(data);
		return;

		case 0xfa0000:
		case 0xfa0001:
			atarirle_control_w(0,data,scanline);
		return;

		case 0xfb0000:
		case 0xfb0001:
			video_int_state = 0;
			update_interrupts();
		return;

		case 0xfc8000:
		case 0xfc8001:
		case 0xfc8002:
		case 0xfc8003:
		case 0xfc8004:
		case 0xfc8005:
		case 0xfc8006:
		case 0xfc8007:
			a2d_select = (address & 0x6) >> 1;
		return;

		case 0xff2000:
		case 0xff2001:
			atarirle_command_w(0, (pitfight != 0 && data == 0) ? ATARIRLE_COMMAND_CHECKSUM : ATARIRLE_COMMAND_DRAW);
		return;
	}
}

static inline UINT16 special_read()
{
	UINT16 ret = DrvInputs[0];
	if (atarigen_cpu_to_sound_ready) ret ^= 0x1000;
	ret ^= 0x2000;
	if (vblank) ret ^= 0x8000;
	return ret;
}

static UINT8 read_analog(INT32 port)
{
	UINT8 ret = 0;

	switch (port) {
		case 0: ret = ProcessAnalog(DrvAnalogPort0, 0, INPUT_DEADZONE, 0x00, 0xfe); break;
		case 1: ret = ProcessAnalog(DrvAnalogPort1, 0, INPUT_DEADZONE, 0x00, 0xfe); break;
		case 2: ret = ProcessAnalog(DrvAnalogPort2, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff); break;
	}

	return ret;
}

static UINT16 __fastcall atarig1_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xfc0000:
			return special_read();

		case 0xfc8000:
		case 0xfc8002:
		case 0xfc8004:
		case 0xfc8006:
			if (pitfight) return DrvInputs[1];
			return read_analog(a2d_select) << 8;

		case 0xfd0000:
			return 0xff | (AtariJSARead() << 8);
	}

	return 0;
}

static UINT8 __fastcall atarig1_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xfc0000:
		case 0xfc0001:
			return special_read() >> ((~address & 1) * 8);

		case 0xfc8000:
		case 0xfc8001:
		case 0xfc8002:
		case 0xfc8003:
		case 0xfc8004:
		case 0xfc8005:
		case 0xfc8006:
		case 0xfc8007:
			if (pitfight) return (DrvInputs[1] >> ((~address & 1) * 8));
			return ((read_analog(a2d_select) << 8) >> ((~address & 1) * 8));

		case 0xfd0000:
			return AtariJSARead();
		case 0xfd0001:
			return 0xff;
	}

	return 0;
}

static tilemap_callback( bg )
{
	UINT16 data = *((UINT16*)(DrvPfRAM + (offs * 2)));

	INT32 code = (pf_tile_bank << 12) | (data & 0xfff);

	TILE_SET_INFO(0, code, data >> 12, TILE_FLIPYX(data >> 15));
}

static tilemap_callback( alpha )
{
	UINT16 data = *((UINT16*)(DrvAlphaRAM + (offs * 2)));

	TILE_SET_INFO(1, data, data >> 12, (data >> 15) ? TILE_OPAQUE : 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	AtariSlapsticReset();
	AtariEEPROMReset();
	AtariJSAReset();

	BurnWatchdogReset();

	a2d_select = 0;
	video_int_state = 0;
	pf_tile_bank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;
	DrvM6502ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x100000;
	DrvGfxROM1		= Next; Next += 0x040000;
	DrvGfxROM2		= Next; Next += 0x200000;

	DrvSndROM		= Next; Next += 0x080000;

	DrvPalette		= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam			= Next;

	DrvPalRAM		= Next; Next += 0x000c00;

	Drv68KRAM		= Next;
	DrvRLERAM		= Next; Next += 0x4000; // 0 - fff
	DrvPfRAM		= Next; Next += 0x2000; // 4000 - 5fff
	DrvAlphaRAM		= Next; Next += 0x1000; // 6000 - 6fff
					  Next += 0x9000; // 7000 - ffff (Drv68KRAM)

	atarirle_0_spriteram = (UINT16*)DrvRLERAM;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { STEP4(0,1) };
	INT32 XOffs0[8] = { STEP8(0,4) };
	INT32 YOffs0[8] = { STEP8(0,32) };

	INT32 Plane1[1] = { 0 };
	INT32 XOffs1[8] = { STEP8(0,1) };
	INT32 YOffs1[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xa0000);
	if (tmp == NULL) {
		return 1;
	}

	UINT8 *extra = (UINT8*)BurnMalloc(0x100000);

	memcpy (tmp, DrvGfxROM0, 0x0a0000);

	GfxDecode(0x4000, 4, 8, 8, Plane0, XOffs0, YOffs0, 0x100, tmp + 0x00000, DrvGfxROM0);
	GfxDecode(0x4000, 1, 8, 8, Plane1, XOffs1, YOffs1, 0x040, tmp + 0x80000, extra);

	for (INT32 i = 0; i < 0x100000; i++) {
		DrvGfxROM0[i] = (DrvGfxROM0[i] & 0xf) | ((extra[i] & 0x01) << 4);
	}

	BurnFree (extra);

	memcpy (tmp, DrvGfxROM1, 0x020000);

	GfxDecode(0x1000, 4, 8, 8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 game, INT32 slapstic)
{
	static const struct atarirle_desc modesc_hydra =
	{
		0,							// region where the GFX data lives
		256,						// number of entries in sprite RAM
		0,							// left clip coordinate
		255,						// right clip coordinate

		0x200,						// base palette entry
		0x100,						// maximum number of colors

		{{ 0x7fff,0,0,0,0,0,0,0 }},	// mask for the code index
		{{ 0,0x00f0,0,0,0,0,0,0 }},	// mask for the color
		{{ 0,0,0xffc0,0,0,0,0,0 }},	// mask for the X position
		{{ 0,0,0,0xffc0,0,0,0,0 }},	// mask for the Y position
		{{ 0,0,0,0,0xffff,0,0,0 }},	// mask for the scale factor
		{{ 0x8000,0,0,0,0,0,0,0 }},	// mask for the horizontal flip
		{{ 0,0,0,0,0,0x00ff,0,0 }},	// mask for the order
		{{ 0 }},					// mask for the priority
		{{ 0 }}						// mask for the VRAM target
	};

	static const struct atarirle_desc modesc_pitfight =
	{
		0,							// region where the GFX data lives
		256,						// number of entries in sprite RAM
		40,							// left clip coordinate
		295,						// right clip coordinate

		0x200,						// base palette entry
		0x100,						// maximum number of colors

		{{ 0x7fff,0,0,0,0,0,0,0 }},	// mask for the code index
		{{ 0,0x00f0,0,0,0,0,0,0 }},	// mask for the color
		{{ 0,0,0xffc0,0,0,0,0,0 }},	// mask for the X position
		{{ 0,0,0,0xffc0,0,0,0,0 }},	// mask for the Y position
		{{ 0,0,0,0,0xffff,0,0,0 }},	// mask for the scale factor
		{{ 0x8000,0,0,0,0,0,0,0 }},	// mask for the horizontal flip
		{{ 0,0,0,0,0,0,0x00ff,0 }},	// mask for the order
		{{ 0 }},					// mask for the priority
		{{ 0 }}						// mask for the VRAM target
	};

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (game == 0) // hydra
	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x060001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x060000, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6502ROM+ 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000001, k++, 2)) return 1; // 4bp section
		if (BurnLoadRom(DrvGfxROM0 + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000, k++, 1)) return 1; // 1 bpp portion
		if (BurnLoadRom(DrvGfxROM0 + 0x090000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 1)) return 1;

		// byteswapped compared to mame
		if (BurnLoadRom(DrvGfxROM2 + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x020000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x040000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x060000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x060001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0a0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0a0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0c0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0c0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0e0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0e0001, k++, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x060000, k++, 1)) return 1;

		DrvGfxDecode();
	}
	else // pitfight
	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020000, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6502ROM+ 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000001, k++, 2)) return 1; // 4bp section
		if (BurnLoadRom(DrvGfxROM0 + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000, k++, 1)) return 1; // 1 bpp portion

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 1)) return 1;

		// byteswapped compared with mame
		if (BurnLoadRom(DrvGfxROM2 + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x040000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0c0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0c0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x100001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x140000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x140001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x180000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x180001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x1c0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x1c0001, k++, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x070000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM + 0x000000,		0x000000, 0x03ffff, MAP_ROM);
	if (game == 1 && slapstic != -1)
	{
		SekMapMemory(Drv68KROM + 0x038000,	0x038000, 0x03ffff, MAP_ROM); // pitfight slapstic
	}
	SekMapMemory(Drv68KROM + 0x040000,		0x040000, 0x077fff, MAP_ROM);
	if (game == 0 && slapstic != -1)
	{
		SekMapMemory(Drv68KROM + 0x078000,	0x078000, 0x07ffff, MAP_ROM); // hydra slapstic
	}
	SekMapMemory(DrvPalRAM,					0xfe8000, 0xfe8bff, MAP_RAM); // 0-9ff
	SekMapMemory(Drv68KRAM,					0xff0000, 0xffffff, MAP_RAM);

	SekMapHandler(0,						0xff0000, 0xff23ff, MAP_WRITE); // unmap this part

	SekSetWriteWordHandler(0,				atarig1_main_write_word);
	SekSetWriteByteHandler(0,				atarig1_main_write_byte);
	SekSetReadWordHandler(0,				atarig1_main_read_word);
	SekSetReadByteHandler(0,				atarig1_main_read_byte);

	if (slapstic != -1)
	{
		INT32 addr = (game == 0) ? 0x78000 : 0x38000;

		AtariSlapsticInit(Drv68KROM + addr, slapstic);
		AtariSlapsticInstallMap(1, 			addr);
	}

	AtariEEPROMInit(0x8000);
	AtariEEPROMInstallMap(3, 				0xfd8000, 0xfdffff);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	AtariJSAInit(DrvM6502ROM, &update_interrupts, DrvSndROM, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback,    8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, alpha_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 5, 8, 8, 0x100000, 0x300, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 8, 0x040000, 0x100, 0x0f);
	GenericTilemapSetTransparent(1, 0);

	atarirle_init(0, game ? &modesc_pitfight : &modesc_hydra, DrvGfxROM2, game ? 0x200000 : 0x100000);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	atarirle_exit();

	AtariJSAExit();
	AtariSlapsticExit();
	AtariEEPROMExit();

	BurnFree(AllMem);

	pitfight = 0;

	return 0;
}

static void draw_background()
{
	UINT16 *alpha = (UINT16*)DrvAlphaRAM;

	for (INT32 i = 0; i < 240; i++)
	{
		INT32 offs = ((i / 8) * 64) + 48 + ((i & 7) * 2);

		UINT16 word0 = alpha[offs+0];
		UINT16 word1 = alpha[offs+1];

		if (word0 & 0x8000)
		{
			GenericTilemapSetScrollX(0, (word0 >> 6) + ((pitfight) ? 2 : 0)); // +2 offset for pitfight?
		}

		if (word1 & 0x8000)
		{	
			GenericTilemapSetScrollY(0, (word1 >> 6) - i);
			pf_tile_bank = word1 & 0x0007;
		}

		GenericTilesSetClip(-1, -1, i, i+1);
		GenericTilemapDraw(0, pTransDraw, 0);
		GenericTilesClearClip();
	}
}

static void copy_sprites()
{
	UINT16 *b1 = BurnBitmapGetBitmap(1);

	for (INT32 z = 0; z < nScreenWidth * nScreenHeight; z++)
	{
		if (nSpriteEnable & 1) if (b1[z]) pTransDraw[z] = b1[z] & 0x3ff;
	}
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0xc00/2; i++)
	{
		INT32 n = p[i] >> 15;
		UINT8 r = ((p[i] >> 9) & 0x3e) | n;
		UINT8 g = ((p[i] >> 4) & 0x3e) | n;
		UINT8 b = ((p[i] << 1) & 0x3e) | n;

		r = (r << 2) | (r >> 4);
		g = (g << 2) | (g >> 4);
		b = (b << 2) | (b >> 4);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force!!
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_background();

	copy_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	atarirle_eof();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	SekNewFrame();
	M6502NewFrame();

	{
		DrvInputs[0] = 0x1fff | ((DrvDips[0] & 0x40) << 8);
		DrvInputs[1] = 0xffff;
		DrvInputs[2] = 0x0040;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		atarijsa_input_port = DrvInputs[2];
		atarijsa_test_mask = 0x40;
		atarijsa_test_port = DrvDips[0] & atarijsa_test_mask;
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { (INT32)(14318180 / 59.92), (INT32)(1789773 / 59.92) };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	M6502Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		nCyclesDone[1] += M6502Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);

		if (i == 239) {
			vblank = 1;

			video_int_state = 1;
			update_interrupts();

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		AtariJSAInterruptUpdate(nInterleave);

		if (pBurnSoundOut && i&1) {
			INT32 nSegment = nBurnSoundLen / (nInterleave / 2);
			AtariJSAUpdate(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			AtariJSAUpdate(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	SekClose();
	M6502Close();

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

		SekScan(nAction);

		atarirle_scan(nAction, pnMin);
		AtariJSAScan(nAction, pnMin);
		AtariSlapsticScan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		SCAN_VAR(a2d_select);
		SCAN_VAR(pf_tile_bank);
		SCAN_VAR(video_int_state);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Hydra

static struct BurnRomInfo hydraRomDesc[] = {
	{ "136079-3028.bin",			0x10000, 0x43475f73, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136079-3029.bin",			0x10000, 0x886e1de8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136079-3034.bin",			0x10000, 0x5115aa36, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136079-3035.bin",			0x10000, 0xa28ba44b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136079-1032.bin",			0x10000, 0xecd1152a, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136079-1033.bin",			0x10000, 0x2ebe1939, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136079-1030.bin",			0x10000, 0xb31fd41f, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136079-1031.bin",			0x10000, 0x453d076f, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "hydraa0.bin",				0x10000, 0x619d7319, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code
	
	{ "136079-1017.bin",			0x10000, 0xbd77b747, 3 | BRF_GRA },           //  9 Background Tiles
	{ "136079-1018.bin",			0x10000, 0x7c24e637, 3 | BRF_GRA },           // 10
	{ "136079-1019.bin",			0x10000, 0xaa2fb07b, 3 | BRF_GRA },           // 11
	{ "136079-1020.bin",			0x10000, 0x906ccd98, 3 | BRF_GRA },           // 12
	{ "136079-1021.bin",			0x10000, 0xf88cdac2, 3 | BRF_GRA },           // 13
	{ "136079-1022.bin",			0x10000, 0xa9c612ff, 3 | BRF_GRA },           // 14
	{ "136079-1023.bin",			0x10000, 0xb706aa6e, 3 | BRF_GRA },           // 15
	{ "136079-1024.bin",			0x10000, 0xc49eac53, 3 | BRF_GRA },           // 16
	{ "136079-1025.bin",			0x10000, 0x98b5b1a1, 3 | BRF_GRA },           // 17
	{ "136079-1026.bin",			0x10000, 0xd68d44aa, 3 | BRF_GRA },           // 18

	{ "136079-1027.bin",			0x20000, 0xf9135b9b, 4 | BRF_GRA },           // 19 Alpha Tiles

	{ "136079-1001.bin",			0x10000, 0x3f757a53, 5 | BRF_GRA },           // 20 RLE-Compressed Sprites
	{ "136079-1002.bin",			0x10000, 0xa1169469, 5 | BRF_GRA },           // 21
	{ "136079-1003.bin",			0x10000, 0xaa21ec33, 5 | BRF_GRA },           // 22
	{ "136079-1004.bin",			0x10000, 0xc0a2be66, 5 | BRF_GRA },           // 23
	{ "136079-1005.bin",			0x10000, 0x80c285b3, 5 | BRF_GRA },           // 24
	{ "136079-1006.bin",			0x10000, 0xad831c59, 5 | BRF_GRA },           // 25
	{ "136079-1007.bin",			0x10000, 0xe0688cc0, 5 | BRF_GRA },           // 26
	{ "136079-1008.bin",			0x10000, 0xe6827f6b, 5 | BRF_GRA },           // 27
	{ "136079-1009.bin",			0x10000, 0x33624d07, 5 | BRF_GRA },           // 28
	{ "136079-1010.bin",			0x10000, 0x9de4c689, 5 | BRF_GRA },           // 29
	{ "136079-1011.bin",			0x10000, 0xd55c6e49, 5 | BRF_GRA },           // 30
	{ "136079-1012.bin",			0x10000, 0x43af45d0, 5 | BRF_GRA },           // 31
	{ "136079-1013.bin",			0x10000, 0x2647a82b, 5 | BRF_GRA },           // 32
	{ "136079-1014.bin",			0x10000, 0x8897d5e9, 5 | BRF_GRA },           // 33
	{ "136079-1015.bin",			0x10000, 0xcf7f69fd, 5 | BRF_GRA },           // 34
	{ "136079-1016.bin",			0x10000, 0x61aaf14f, 5 | BRF_GRA },           // 35

	{ "136079-1037.bin",			0x10000, 0xb974d3d0, 6 | BRF_GRA },           // 36 Samples
	{ "136079-1038.bin",			0x10000, 0xa2eda15b, 6 | BRF_GRA },           // 37
	{ "136079-1039.bin",			0x10000, 0xeb9eaeb7, 6 | BRF_GRA },           // 38

	{ "136079-1040.bin",			0x00200, 0x43d6f3d4, 7 | BRF_GRA },           // 39 RLE-Control PROMs
	{ "136079-1041.bin",			0x00200, 0x341dc4bb, 7 | BRF_GRA },           // 40
	{ "136079-1042.bin",			0x00200, 0x2e49b52e, 7 | BRF_GRA },           // 41
};

STD_ROM_PICK(hydra)
STD_ROM_FN(hydra)

static INT32 HydraInit()
{
	return DrvInit(0, 116);
}

struct BurnDriver BurnDrvHydra = {
	"hydra", NULL, NULL, NULL, "1990",
	"Hydra\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hydraRomInfo, hydraRomName, NULL, NULL, NULL, NULL, HydraInputInfo, HydraDIPInfo,
	HydraInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	336, 240, 4, 3
};


// Hydra (prototype 5/14/90)

static struct BurnRomInfo hydrapRomDesc[] = {
	{ "hydhi0.bin",					0x10000, 0xdab2e8a2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "hydlo0.bin",					0x10000, 0xc18d4f16, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "hydhi1.bin",					0x10000, 0x50c12bb9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "hydlo1.bin",					0x10000, 0x5ee0a846, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "hydhi2.bin",					0x10000, 0x436a6d81, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "hydlo2.bin",					0x10000, 0x182bfd6a, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "hydhi3.bin",					0x10000, 0x29e9e03e, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "hydlo3.bin",					0x10000, 0x7b5047f0, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "hydraa0.bin",				0x10000, 0x619d7319, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code

	{ "136079-1017.bin",			0x10000, 0xbd77b747, 3 | BRF_GRA },           //  9 Background Tiles
	{ "136079-1018.bin",			0x10000, 0x7c24e637, 3 | BRF_GRA },           // 10
	{ "136079-1019.bin",			0x10000, 0xaa2fb07b, 3 | BRF_GRA },           // 11
	{ "hydpl03.bin",				0x10000, 0x1f0dfe60, 3 | BRF_GRA },           // 12
	{ "136079-1021.bin",			0x10000, 0xf88cdac2, 3 | BRF_GRA },           // 13
	{ "136079-1022.bin",			0x10000, 0xa9c612ff, 3 | BRF_GRA },           // 14
	{ "136079-1023.bin",			0x10000, 0xb706aa6e, 3 | BRF_GRA },           // 15
	{ "hydphi3.bin",				0x10000, 0x917e250c, 3 | BRF_GRA },           // 16
	{ "136079-1025.bin",			0x10000, 0x98b5b1a1, 3 | BRF_GRA },           // 17
	{ "hydpl41.bin",				0x10000, 0x85f9afa6, 3 | BRF_GRA },           // 18

	{ "hydalph.bin",				0x20000, 0x7dd2b062, 4 | BRF_GRA },           // 19 Alpha Tiles

	{ "hydmhi0.bin",				0x10000, 0x3c83b42d, 5 | BRF_GRA },           // 20 RLE-Compressed Sprites
	{ "hydmlo0.bin",				0x10000, 0x6d49650c, 5 | BRF_GRA },           // 21
	{ "hydmhi1.bin",				0x10000, 0x689b3376, 5 | BRF_GRA },           // 22
	{ "hydmlo1.bin",				0x10000, 0xc81a4e88, 5 | BRF_GRA },           // 23
	{ "hydmhi2.bin",				0x10000, 0x77098e14, 5 | BRF_GRA },           // 24
	{ "hydmlo2.bin",				0x10000, 0x40015d9d, 5 | BRF_GRA },           // 25
	{ "hydmhi3.bin",				0x10000, 0xdfebdcbd, 5 | BRF_GRA },           // 26
	{ "hydmlo3.bin",				0x10000, 0x213c407c, 5 | BRF_GRA },           // 27
	{ "hydmhi4.bin",				0x10000, 0x2897765f, 5 | BRF_GRA },           // 28
	{ "hydmlo4.bin",				0x10000, 0x730157f3, 5 | BRF_GRA },           // 29
	{ "hydmhi5.bin",				0x10000, 0xecd061ae, 5 | BRF_GRA },           // 30
	{ "hydmlo5.bin",				0x10000, 0xa5a08c53, 5 | BRF_GRA },           // 31
	{ "hydmhi6.bin",				0x10000, 0xaa3f2903, 5 | BRF_GRA },           // 32
	{ "hydmlo6.bin",				0x10000, 0xdb8ea56f, 5 | BRF_GRA },           // 33
	{ "hydmhi7.bin",				0x10000, 0x71fc3e43, 5 | BRF_GRA },           // 34
	{ "hydmlo7.bin",				0x10000, 0x7960b0c2, 5 | BRF_GRA },           // 35

	{ "136079-1037.bin",			0x10000, 0xb974d3d0, 6 | BRF_GRA },           // 36 Samples
	{ "136079-1038.bin",			0x10000, 0xa2eda15b, 6 | BRF_GRA },           // 37
	{ "136079-1039.bin",			0x10000, 0xeb9eaeb7, 6 | BRF_GRA },           // 38

	{ "136079-1040.bin",			0x00200, 0x43d6f3d4, 7 | BRF_GRA },           // 39 RLE-Control PROMs
	{ "136079-1041.bin",			0x00200, 0x341dc4bb, 7 | BRF_GRA },           // 40
	{ "136079-1042.bin",			0x00200, 0x2e49b52e, 7 | BRF_GRA },           // 41
};

STD_ROM_PICK(hydrap)
STD_ROM_FN(hydrap)

static INT32 HydrapInit()
{
	return DrvInit(0, -1);
}

struct BurnDriver BurnDrvHydrap = {
	"hydrap", "hydra", NULL, NULL, "1990",
	"Hydra (prototype 5/14/90)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, hydrapRomInfo, hydrapRomName, NULL, NULL, NULL, NULL, HydraInputInfo, HydraDIPInfo,
	HydrapInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	336, 240, 4, 3
};


// Pit Fighter (rev 9)

static struct BurnRomInfo pitfightRomDesc[] = {
	{ "136081-9028.05d",			0x10000, 0x427713e9, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136081-9029.05b",			0x10000, 0x2cdeaeba, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136081-9030.15d",			0x10000, 0x3bace9ef, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136081-9031.15b",			0x10000, 0xc717f011, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136081-1060.1b",				0x10000, 0x231d71d7, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "136081-1017.130m",			0x10000, 0xad3cfea5, 3 | BRF_GRA },           //  5 Background Tiles
	{ "136081-1018.120m",			0x10000, 0x1a0f8bcf, 3 | BRF_GRA },           //  6
	{ "136081-1021.90m",			0x10000, 0x777efee3, 3 | BRF_GRA },           //  7
	{ "136081-1022.75m",			0x10000, 0x524319d0, 3 | BRF_GRA },           //  8
	{ "136081-1025.45m",			0x10000, 0xfc41691a, 3 | BRF_GRA },           //  9

	{ "136081-1027.15l",			0x10000, 0xa59f381d, 4 | BRF_GRA },           // 10 Alpha Tiles

	{ "136081-1001.65r",			0x20000, 0x3af31444, 5 | BRF_GRA },           // 11 RLE-Compressed Sprites
	{ "136081-1002.65n",			0x20000, 0xf1d76a4c, 5 | BRF_GRA },           // 12
	{ "136081-1003.70r",			0x20000, 0x28c41c2a, 5 | BRF_GRA },           // 13
	{ "136081-1004.70n",			0x20000, 0x977744da, 5 | BRF_GRA },           // 14
	{ "136081-1005.75r",			0x20000, 0xae59aef2, 5 | BRF_GRA },           // 15
	{ "136081-1006.75n",			0x20000, 0xb6ccd77e, 5 | BRF_GRA },           // 16
	{ "136081-1007.90r",			0x20000, 0xba33b0c0, 5 | BRF_GRA },           // 17
	{ "136081-1008.90n",			0x20000, 0x09bd047c, 5 | BRF_GRA },           // 18
	{ "136081-1009.100r",			0x20000, 0xab85b00b, 5 | BRF_GRA },           // 19
	{ "136081-1010.100n",			0x20000, 0xeca94bdc, 5 | BRF_GRA },           // 20
	{ "136081-1011.115r",			0x20000, 0xa86582fd, 5 | BRF_GRA },           // 21
	{ "136081-1012.115n",			0x20000, 0xefd1152d, 5 | BRF_GRA },           // 22
	{ "136081-1013.120r",			0x20000, 0xa141379e, 5 | BRF_GRA },           // 23
	{ "136081-1014.120n",			0x20000, 0x93bfcc15, 5 | BRF_GRA },           // 24
	{ "136081-1015.130r",			0x20000, 0x9378ad0b, 5 | BRF_GRA },           // 25
	{ "136081-1016.130n",			0x20000, 0x19c3fbe0, 5 | BRF_GRA },           // 26

	{ "136081-1061.7k",				0x10000, 0x5b0468c6, 6 | BRF_GRA },           // 27 Samples
	{ "136081-1062.7j",				0x10000, 0xf73fe3cb, 6 | BRF_GRA },           // 28
	{ "136081-1063.7e",				0x10000, 0xaa93421d, 6 | BRF_GRA },           // 29
	{ "136081-1064.7d",				0x10000, 0x33f045d5, 6 | BRF_GRA },           // 30

	{ "136081-1040.30s",			0x00200, 0x9b0f8b95, 7 | BRF_GRA },           // 31  RLE-Control PROMs
	{ "136081-1041.25s",			0x00200, 0xf7ba6153, 7 | BRF_GRA },           // 32
	{ "136081-1042.20s",			0x00200, 0x3572fe68, 7 | BRF_GRA },           // 33

	{ "gal16v8a-136081-1043.15e",	0x00117, 0x649d9c0d, 8 | BRF_OPT },           // 34 PLDs
	{ "gal16v8a-136079-1045.40p",	0x00117, 0x535c4d2e, 8 | BRF_OPT },           // 35
	{ "gal16v8a-136079-1046.65d",	0x00117, 0x38b22b2c, 8 | BRF_OPT },           // 36
	{ "gal16v8a-136079-1047.40s",	0x00117, 0x20839904, 8 | BRF_OPT },           // 37
	{ "gal16v8a-136079-1048.125f",	0x00117, 0xd16e8d6e, 8 | BRF_OPT },           // 38
	{ "gal16v8a-136079-1049.120f",	0x00117, 0x68440ab1, 8 | BRF_OPT },           // 39
	{ "gal16v8a-136079-1050.115f",	0x00117, 0x20dc296a, 8 | BRF_OPT },           // 40
	{ "gal16v8a-136079-1051.105f",	0x00117, 0x45f0cb5f, 8 | BRF_OPT },           // 41
	{ "gal16v8a-136079-1052.30n",	0x00117, 0x6b3d2669, 8 | BRF_OPT },           // 42
};

STD_ROM_PICK(pitfight)
STD_ROM_FN(pitfight)

static INT32 PitfightInit()
{
	pitfight = 1;

	return DrvInit(1, 114);
}

struct BurnDriver BurnDrvPitfight = {
	"pitfight", NULL, NULL, NULL, "1990",
	"Pit Fighter (rev 9)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, pitfightRomInfo, pitfightRomName, NULL, NULL, NULL, NULL, PitfightInputInfo, PitfightDIPInfo,
	PitfightInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x600,
	336, 240, 4, 3
};

