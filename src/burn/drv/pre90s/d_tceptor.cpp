// FB Alpha Thunder Ceptor driver module
// Based on MAME driver by BUT

// to do:
//  right side background is jerky and scrolls kinda weird - fixed with kludge in tilemap_generic

// for later (dink):
//  audio cpu (hd63701) needs 100khz cycles p/s more otherwise it dies at 0xe in the soundtest.
//  -> verify the m6803_internal_registers & timers internal to the m680x in the core.

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "m6800_intf.h"
#include "m6502_intf.h"
#include "m68000_intf.h"
#include "namco_snd.h"
#include "burn_ym2151.h"
#include "dac.h"
#include "namco_c45.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6502ROM0;
static UINT8 *DrvM6502ROM1;
static UINT8 *Drv68KROM;
static UINT8 *DrvHD63701ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvM6809RAM0;
static UINT8 *DrvTileRAM;
static UINT8 *DrvTileAttrRAM;
static UINT8 *DrvBgRAM0;
static UINT8 *DrvBgRAM1;
static UINT8 *DrvM6502RAM0;
static UINT8 *DrvM6502RAM1;
static UINT8 *DrvShareRAM0;
static UINT8 *DrvShareRAM1;
static UINT8 *DrvShareRAM2;
static UINT8 *Drv68KRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvHD63701RAM0;
static UINT8 *DrvHD63701RAM1;
static UINT8 *DrvHD63701RAM2;
static UINT16 *DrvBitmap;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *sprite_mask_enable;

static UINT16 scroll[4];
static UINT8 m68000_irq_enable;
static UINT8 m6809_irq_enable;
static UINT8 mcu_irq_enable;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvInputs[2];
static UINT8 DrvDips[3];
static UINT8 DrvReset;

static INT32 DrvAnalogPort0 = 0;
static INT32 DrvAnalogPort1 = 0;
static INT32 DrvAnalogPort2 = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo TceptorInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"	},

	A("P1 X Axis",      BIT_ANALOG_REL, &DrvAnalogPort0, "mouse x-axis"),
	A("P1 Y Axis",      BIT_ANALOG_REL, &DrvAnalogPort1, "mouse y-axis"),
	A("P1 Accelerator", BIT_ANALOG_REL, &DrvAnalogPort2, "p1 x-axis"),

	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};
#undef A
STDINPUTINFO(Tceptor)

static struct BurnDIPInfo TceptorDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xff, NULL					},
	{0x0b, 0xff, 0xff, 0xfb, NULL					},
	{0x0c, 0xff, 0xff, 0x04, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x0a, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x0a, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0x07, 0x01, "3 Coins 2 Credits"	},
	{0x0a, 0x01, 0x07, 0x00, "4 Coins 3 Credits"	},
	{0x0a, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x0a, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x0a, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x0a, 0x01, 0x18, 0x10, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0x18, 0x08, "3 Coins 2 Credits"	},
	{0x0a, 0x01, 0x18, 0x00, "4 Coins 3 Credits"	},
	{0x0a, 0x01, 0x18, 0x18, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0a, 0x01, 0x20, 0x00, "Off"					},
	{0x0a, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{0x0a, 0x01, 0x40, 0x40, "Off"					},
	{0x0a, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0b, 0x01, 0x03, 0x02, "A"					},
	{0x0b, 0x01, 0x03, 0x03, "B"					},
	{0x0b, 0x01, 0x03, 0x01, "C"					},
	{0x0b, 0x01, 0x03, 0x00, "D"					},

	{0   , 0xfe, 0   ,    1, "MODE (tceptor2)"		},
	{0x0b, 0x01, 0x04, 0x00, "2D"					},
//	{0x0b, 0x01, 0x04, 0x04, "3D"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0c, 0x01, 0x04, 0x00, "On"					},
	{0x0c, 0x01, 0x04, 0x04, "Off"					},
};

STDDIPINFO(Tceptor)

static void tceptor_m6809_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x4000) {
		namcos1_custom30_write(address & 0x3ff, data);
		return;
	}

	switch (address)
	{
		case 0x4800:
		case 0x4f00:
		case 0x4f01:
		case 0x4f02:
		case 0x4f03:
		return;	// nop

		case 0x5000:
			scroll[0] = (scroll[0] & 0x00ff) | (data << 8);
		return;

		case 0x5001:
			scroll[0] = (scroll[0] & 0xff00) | data;
		return;

		case 0x5002:
			scroll[1] = data;
		return;

		case 0x5004:
			scroll[2] = (scroll[2] & 0x00ff) | (data << 8);
		return;

		case 0x5005:
			scroll[2] = (scroll[2] & 0xff00) | data;
		return;

		case 0x5006:
			scroll[3] = data;
		return;

		case 0x8000:
		case 0x8800:
			m6809_irq_enable = (address >> 11) & 1;
		return;
	}
}

static UINT8 tceptor_m6809_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x4000) {
		return namcos1_custom30_read(address & 0x3ff);
	}

	switch (address)
	{
		case 0x4f00:
			return 0; // nop

		case 0x4f01: // pedal (accel)
			if (DrvAnalogPort2 == 0xffff) DrvAnalogPort2 = 0xfc04; // digital button -> accel
			return ProcessAnalog(DrvAnalogPort2, 0, 1, 0x00, 0xd6);

		case 0x4f02: // x
			return ProcessAnalog(DrvAnalogPort0, 0, 1, 0x00, 0xfe);

		case 0x4f03: // y
			return ProcessAnalog(DrvAnalogPort1, 0, 1, 0x00, 0xfe);
	}

	return 0;
}

static void tceptor_m6502_0_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
		case 0x2001:
			BurnYM2151Write(address & 1, data);
		return;

		case 0x3c01:
			// nop
		return;
	}
}

static UINT8 tceptor_m6502_0_read(UINT16 address)
{
	switch (address)
	{
		case 0x2000:
		case 0x2001:
			return BurnYM2151Read();
	}

	return 0;
}

static void tceptor_m6502_1_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
			DACWrite16(0, (data ? (data + 1) * 0x100 : 0x8000) - 0x8000);
		return;

		case 0x5000:
			// nop
		return;
	}
}

static void __fastcall tceptor_68k_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffc000) == 0x700000) {
		DrvShareRAM0[(address / 2) & 0x1fff] = data;
		return;
	}

	switch (address)
	{
		case 0x300000:
		case 0x300001:
			// nop
		return;

		case 0x600000:
		case 0x600001:
			m68000_irq_enable = data ? 1 : 0;
		return;
	}
}

static void __fastcall tceptor_68k_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffc000) == 0x700000) { // probably not used
		DrvShareRAM0[(address / 2) & 0x1fff] = data;
		return;
	}

	switch (address)
	{
		case 0x300000:
		case 0x300001:
			// nop
		return;

		case 0x600000:
		case 0x600001:
			m68000_irq_enable = data ? 1 : 0;
		return;
	}
}

static UINT16 __fastcall tceptor_68k_read_word(UINT32 address)
{
	if ((address & 0xffc000) == 0x700000) {
		return DrvShareRAM0[(address / 2) & 0x1fff];
	}

	return 0;
}

static UINT8 __fastcall tceptor_68k_read_byte(UINT32 address)
{
	if ((address & 0xffc000) == 0x700000) { // probably not used
		return DrvShareRAM0[(address / 2) & 0x1fff];
	}

	return 0;
}

static void tceptor_mcu_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffe0) == 0x0000) {
		m6803_internal_registers_w(address & 0x1f, data);
		return;
	}

	if ((address & 0xff80) == 0x0080) {
		DrvHD63701RAM1[address & 0x7f] = data;
		return;
	}

	if ((address & 0xfc00) == 0x1000) {
		namcos1_custom30_write(address & 0x3ff, data);
		return;
	}

	switch (address)
	{
		case 0x8000:
		case 0x8800:
			mcu_irq_enable = (data >> 11) & 1;
		return;
	}
}

static UINT8 interleave_input0(UINT8 in1, UINT8 in2)
{
	UINT8 r = 0;

	if (in1 & 0x80) r |= 0x01;
	if (in1 & 0x20) r |= 0x02;
	if (in1 & 0x08) r |= 0x04;
	if (in1 & 0x02) r |= 0x08;
	if (in2 & 0x80) r |= 0x10;
	if (in2 & 0x20) r |= 0x20;
	if (in2 & 0x08) r |= 0x40;
	if (in2 & 0x02) r |= 0x80;

	return r;
}

static UINT8 interleave_input1(UINT8 in1, UINT8 in2)
{
	UINT8 r = 0;

	if (in1 & 0x40) r |= 0x01;
	if (in1 & 0x10) r |= 0x02;
	if (in1 & 0x04) r |= 0x04;
	if (in1 & 0x01) r |= 0x08;
	if (in2 & 0x40) r |= 0x10;
	if (in2 & 0x10) r |= 0x20;
	if (in2 & 0x04) r |= 0x40;
	if (in2 & 0x01) r |= 0x80;

	return r;
}

static UINT8 tceptor_mcu_read(UINT16 address)
{
	if ((address & 0xffe0) == 0x0000) {
		return m6803_internal_registers_r(address & 0x1f);
	}

	if ((address & 0xff80) == 0x0080) {
		return DrvHD63701RAM1[address & 0x7f];
	}

	if ((address & 0xfc00) == 0x1000) {
		return namcos1_custom30_read(address & 0x3ff);
	}

	switch (address)
	{
		case 0x2100:
			return interleave_input0(DrvDips[0], DrvDips[1]);

		case 0x2101:
			return interleave_input1(DrvDips[0], DrvDips[1]);

		case 0x2200:
			return interleave_input0(DrvInputs[0], DrvInputs[1]);

		case 0x2201:
			return interleave_input1(DrvInputs[0], DrvInputs[1]);
	}

	return 0;
}

static void tceptor_mcu_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0x1ff)
	{
		case HD63701_PORT1:
		case HD63701_PORT2:
			if (data) return; // silence warnings
		return;
	}
}

static UINT8 tceptor_mcu_read_port(UINT16 port)
{
	switch (port & 0x1ff)
	{
		case HD63701_PORT1:
		case HD63701_PORT2:
			return 0xff;
	}

	return 0;
}

inline INT32 get_tile_addr(INT32 tile_index)
{
	INT32 x = tile_index / 28;
	INT32 y = tile_index % 28;
	
	switch (x)
	{
		case 0:
			return 0x3e2 + y;

		case 33:
			return 2 + y;
	}

	return 64 + (x - 1) + y * 32;
}

static tilemap_callback( txt )
{
	offs = get_tile_addr(offs);

	INT32 code  = DrvTileRAM[offs];
	INT32 color = DrvTileAttrRAM[offs];

	TILE_SET_INFO(0, code, color, 0);
	*category = color;
}

static tilemap_callback( bg1 )
{
	UINT16 data = DrvBgRAM0[offs * 2] | (DrvBgRAM0[offs * 2 + 1] << 8);

	TILE_SET_INFO(1, data & 0x3ff, data >> 10, 0);
}

static tilemap_callback( bg2 )
{
	UINT16 data = DrvBgRAM1[offs * 2] | (DrvBgRAM1[offs * 2 + 1] << 8);
	
	TILE_SET_INFO(2, data & 0x3ff, data >> 10, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	DACReset();
	M6502Close();

	SekOpen(0);
	SekReset();
	SekClose();

	HD63701Open(0);
	HD63701Reset();
	HD63701Close();

	c45RoadReset();

	BurnYM2151Reset();
	NamcoSoundReset();

	memset (scroll, 0, 4 * sizeof(UINT16));
	m68000_irq_enable = 0;
	m6809_irq_enable = 0;
	mcu_irq_enable = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0		= Next; Next += 0x010000;
	DrvM6502ROM0		= Next; Next += 0x010000;
	DrvM6502ROM1		= Next; Next += 0x010000;
	Drv68KROM			= Next; Next += 0x110000;
	DrvHD63701ROM		= Next; Next += 0x010000;

	DrvGfxROM0			= Next; Next += 0x008000;
	DrvGfxROM1			= Next; Next += 0x020000;
	DrvGfxROM2			= Next; Next += 0x020000;
	DrvGfxROM3			= Next; Next += 0x100000;

	DrvColPROM			= Next; Next += 0x003500;

	sprite_mask_enable	= Next; Next += 0x000040;
	
	DrvNVRAM			= Next; Next += 0x001800;
	
	DrvPalette			= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);
	
	DrvBitmap			= (UINT16*)Next; Next += 320 * 256 * sizeof(INT16);

	AllRam				= Next;

	DrvM6809RAM0		= Next; Next += 0x001880;
	DrvTileRAM			= Next; Next += 0x000400;
	DrvTileAttrRAM		= Next; Next += 0x000400;
	DrvBgRAM0			= Next; Next += 0x001000;
	DrvBgRAM1			= Next; Next += 0x001000;

	DrvM6502RAM0		= Next; Next += 0x000300;
	DrvM6502RAM1		= Next; Next += 0x000100;

	DrvShareRAM0		= Next; Next += 0x002000;
	DrvShareRAM1		= Next; Next += 0x000100;
	DrvShareRAM2		= Next; Next += 0x000100;

	Drv68KRAM			= Next; Next += 0x004000;
	DrvSprRAM			= Next; Next += 0x000400;
	DrvSprBuf			= Next; Next += 0x000200;
	c45RoadRAM			= Next; Next += 0x020000;

	DrvHD63701RAM0		= Next; Next += 0x000800;
	DrvHD63701RAM1		= Next; Next += 0x000080;
	DrvHD63701RAM2		= Next; Next += 0x000400;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 0, 4 };
	INT32 XOffs0[8]  = { STEP4(64,1), STEP4(0,1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };
	INT32 Plane1[3]  = { 0x40004, 0, 4 };
	INT32 XOffs1[8]  = { STEP4(0,1), STEP4(8,1) };
	INT32 YOffs1[8]  = { STEP8(0,16) };
	INT32 Plane2[4]  = { 0, 4, 0x40000, 0x40004 };
	INT32 XOffs2[16] = { STEP4(0,1), STEP4(8,1), STEP4(16,1), STEP4(24,1) };
	INT32 YOffs2[16] = { STEP16(0,32) };
	INT32 Plane3[4]  = { 0, 4, 0x200000, 0x200004 };
	INT32 XOffs3[32] = { STEP4(0,1), STEP4(8,1), STEP4(16,1), STEP4(24,1),
						STEP4(32,1), STEP4(40,1), STEP4(48,1), STEP4(56,1) };
	INT32 YOffs3[32] = { STEP32(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0100, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	UINT8 *src = DrvGfxROM1 + 0x8000;
	for (INT32 i = 0x8000 - 2; i >= 0; i-=2)
	{
		src[i+1] = src[i/2] & 0x0f;
		src[i+0] = src[i/2] >> 4;
	}
	
	memcpy (tmp, DrvGfxROM1, 0x10000);

	GfxDecode(0x0800, 3,  8,  8, Plane1, XOffs1, YOffs1, 0x080, tmp, DrvGfxROM1);

	for (INT32 i = 0; i < 0x10000 / 0x100; i++)
	{
		for (INT32 y = 0; y < 16; y++)
		{
			memcpy(&tmp[(i*4 + 0) * 64 + y * 4], &DrvGfxROM2[i * 256 + y * 8 + 0x00], 4);
			memcpy(&tmp[(i*4 + 1) * 64 + y * 4], &DrvGfxROM2[i * 256 + y * 8 + 0x04], 4);
			memcpy(&tmp[(i*4 + 2) * 64 + y * 4], &DrvGfxROM2[i * 256 + y * 8 + 0x80], 4);
			memcpy(&tmp[(i*4 + 3) * 64 + y * 4], &DrvGfxROM2[i * 256 + y * 8 + 0x84], 4);
		}
	}

	GfxDecode(0x0200, 4, 16, 16, Plane2, XOffs2, YOffs2, 0x200, tmp, DrvGfxROM2);

	for (INT32 i = 0; i < 0x400; i++)
	{
		INT32 code = ((i & 0x07f) | ((i & 0x180) << 1) | 0x80) & ~((i & 0x200) >> 2);

		memcpy(&tmp[0x100 * (i + 0x000)], &DrvGfxROM3[0x100 * (code + 0x000)], 0x100);
		memcpy(&tmp[0x100 * (i + 0x400)], &DrvGfxROM3[0x100 * (code + 0x400)], 0x100);
	}

	GfxDecode(0x0400, 4, 32, 32, Plane3, XOffs3, YOffs3, 0x800, tmp, DrvGfxROM3);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	memset (DrvNVRAM, 0xff, 0x1800);

	{
		if (game == 0)
		{
			if (BurnLoadRom(DrvM6809ROM0  + 0x008000,  0, 1)) return 1;

			if (BurnLoadRom(DrvM6502ROM0  + 0x008000,  1, 1)) return 1;

			if (BurnLoadRom(DrvM6502ROM1  + 0x008000,  2, 1)) return 1;

			if (BurnLoadRom(Drv68KROM     + 0x000001,  3, 2)) return 1;
			if (BurnLoadRom(Drv68KROM     + 0x000000,  4, 2)) return 1;

			if (BurnLoadRom(DrvHD63701ROM + 0x008000,  5, 1)) return 1;
			if (BurnLoadRom(DrvHD63701ROM + 0x00f000,  6, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0    + 0x000000,  7, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1    + 0x000000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1    + 0x008000,  9, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2    + 0x000000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2    + 0x008000, 11, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM3    + 0x000000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3    + 0x010000, 13, 1)) return 1;
			memcpy (DrvGfxROM3 + 0x18000, DrvGfxROM3 + 0x10000, 0x08000);
			if (BurnLoadRom(DrvGfxROM3    + 0x020000, 14, 1)) return 1;
			memcpy (DrvGfxROM3 + 0x28000, DrvGfxROM3 + 0x20000, 0x08000);
			if (BurnLoadRom(DrvGfxROM3    + 0x030000, 15, 1)) return 1;
			memcpy (DrvGfxROM3 + 0x38000, DrvGfxROM3 + 0x30000, 0x08000);
			if (BurnLoadRom(DrvGfxROM3    + 0x040000, 16, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3    + 0x050000, 17, 1)) return 1;
			memcpy (DrvGfxROM3 + 0x58000, DrvGfxROM3 + 0x50000, 0x08000);
			if (BurnLoadRom(DrvGfxROM3    + 0x060000, 18, 1)) return 1;
			memcpy (DrvGfxROM3 + 0x68000, DrvGfxROM3 + 0x60000, 0x08000);
			if (BurnLoadRom(DrvGfxROM3    + 0x070000, 19, 1)) return 1;
			memcpy (DrvGfxROM3 + 0x78000, DrvGfxROM3 + 0x70000, 0x08000);

			if (BurnLoadRom(DrvColPROM    + 0x000000, 20, 1)) return 1;
			if (BurnLoadRom(DrvColPROM    + 0x000400, 21, 1)) return 1;
			if (BurnLoadRom(DrvColPROM    + 0x000800, 22, 1)) return 1;
			if (BurnLoadRom(DrvColPROM    + 0x000c00, 23, 1)) return 1;
			if (BurnLoadRom(DrvColPROM    + 0x001000, 24, 1)) return 1;
			if (BurnLoadRom(DrvColPROM    + 0x001400, 25, 1)) return 1;
			if (BurnLoadRom(DrvColPROM    + 0x001500, 26, 1)) return 1;
		}
		else
		{
			if (BurnLoadRom(DrvM6809ROM0  + 0x008000,  0, 1)) return 1;

			if (BurnLoadRom(DrvM6502ROM0  + 0x008000,  1, 1)) return 1;

			if (BurnLoadRom(DrvM6502ROM1  + 0x008000,  2, 1)) return 1;

			if (BurnLoadRom(Drv68KROM     + 0x000001,  3, 2)) return 1;
			if (BurnLoadRom(Drv68KROM     + 0x000000,  4, 2)) return 1;
			if (BurnLoadRom(Drv68KROM     + 0x100001,  5, 2)) return 1;
			if (BurnLoadRom(Drv68KROM     + 0x100000,  6, 2)) return 1;

			if (BurnLoadRom(DrvHD63701ROM + 0x008000,  7, 1)) return 1;
			if (BurnLoadRom(DrvHD63701ROM + 0x00f000,  8, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM0    + 0x000000,  9, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1    + 0x000000, 10, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1    + 0x008000, 11, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM2    + 0x000000, 12, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2    + 0x008000, 13, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM3    + 0x000000, 14, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3    + 0x010000, 15, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3    + 0x020000, 16, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3    + 0x030000, 17, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3    + 0x040000, 18, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3    + 0x050000, 19, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3    + 0x060000, 20, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3    + 0x070000, 21, 1)) return 1;

			if (BurnLoadRom(DrvColPROM    + 0x000000, 22, 1)) return 1;
			if (BurnLoadRom(DrvColPROM    + 0x000400, 23, 1)) return 1;
			if (BurnLoadRom(DrvColPROM    + 0x000800, 24, 1)) return 1;
			if (BurnLoadRom(DrvColPROM    + 0x000c00, 25, 1)) return 1;
			if (BurnLoadRom(DrvColPROM    + 0x001000, 26, 1)) return 1;
			if (BurnLoadRom(DrvColPROM    + 0x001400, 27, 1)) return 1;
			if (BurnLoadRom(DrvColPROM    + 0x001500, 28, 1)) return 1;
		}

		DrvGfxDecode();
	}

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM0,			0x0000, 0x17ff, MAP_RAM);
	M6809MapMemory(DrvTileRAM,				0x1800, 0x1bff, MAP_RAM);
	M6809MapMemory(DrvTileAttrRAM,			0x1c00, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvBgRAM0,				0x2000, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvBgRAM1,				0x3000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvShareRAM0,			0x6000, 0x7fff, MAP_RAM); // with 68k
	M6809MapMemory(DrvM6809ROM0 + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(tceptor_m6809_write);
	M6809SetReadHandler(tceptor_m6809_read);
	M6809Close();

	M6502Init(0, TYPE_M65C02);
	M6502Open(0);
	M6502MapMemory(DrvShareRAM1,			0x0000, 0x00ff, MAP_RAM);
	M6502MapMemory(DrvM6502RAM0,			0x0100, 0x03ff, MAP_RAM); // 100-30f
	M6502MapMemory(DrvShareRAM2,			0x3000, 0x30ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM0 + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(tceptor_m6502_0_write);
	M6502SetReadHandler(tceptor_m6502_0_read);
	M6502Close();

	M6502Init(1, TYPE_M65C02);
	M6502Open(1);
	M6502MapMemory(DrvShareRAM1,			0x0000, 0x00ff, MAP_RAM);
	M6502MapMemory(DrvM6502RAM1,			0x0100, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM1 + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(tceptor_m6502_1_write);
	// no reads
	M6502Close();

	c45RoadInit(0xfff, DrvColPROM + 0x1400); // call before mapping to 68k

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,					0x000000, 0x00ffff, MAP_ROM);
	SekMapMemory(Drv68KROM + 0x100000,		0x100000, 0x10ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,					0x200000, 0x203fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,					0x400000, 0x4003ff, MAP_RAM); // 0-1ff
//	SekMapMemory(c45RoadRAM,				0x500000, 0x51ffff, MAP_RAM); //0-ffff map, 10000-1ffff tile data
	SekSetWriteWordHandler(0,				tceptor_68k_write_word);
	SekSetWriteByteHandler(0,				tceptor_68k_write_byte);
	SekSetReadWordHandler(0,				tceptor_68k_read_word);
	SekSetReadByteHandler(0,				tceptor_68k_read_byte);
	c45RoadMap68k(0x500000); // road ram mapped here
	SekClose();

	HD63701Init(0);
	HD63701Open(0);
	HD63701MapMemory(DrvHD63701RAM2,			0x1400, 0x17ff, MAP_RAM);
	HD63701MapMemory(DrvShareRAM2,				0x2000, 0x20ff, MAP_RAM);
	HD63701MapMemory(DrvHD63701ROM + 0x8000,	0x8000, 0xbfff, MAP_ROM);
	HD63701MapMemory(DrvHD63701RAM0,			0xc000, 0xc7ff, MAP_RAM);
	HD63701MapMemory(DrvNVRAM,					0xc800, 0xdfff, MAP_RAM);
	HD63701MapMemory(DrvHD63701ROM + 0xf000,	0xf000, 0xffff, MAP_ROM);
	HD63701SetReadHandler(tceptor_mcu_read);
	HD63701SetWriteHandler(tceptor_mcu_write);
	HD63701SetReadPortHandler(tceptor_mcu_read_port);
	HD63701SetWritePortHandler(tceptor_mcu_write_port);
	HD63701Close();

	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.55, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.55, BURN_SND_ROUTE_RIGHT);

	NamcoSoundInit(24000, 8, 1);
	NacmoSoundSetAllRoutes(0.50, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6502TotalCycles, 2048000);
	DACSetRoute(0, 0.65, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, txt_map_callback,  8,  8, 34, 28);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg1_map_callback,  8,  8, 64, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, bg2_map_callback,  8,  8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0 + 0x00000, 2,  8,  8, 0x04000, 0x000, 0xff);
	GenericTilemapSetGfx(1, DrvGfxROM1 + 0x00000, 3,  8,  8, 0x10000, 0x800, 0x3f);
	GenericTilemapSetGfx(2, DrvGfxROM1 + 0x10000, 3,  8,  8, 0x10000, 0x800, 0x3f);
	GenericTilemapSetOffsets(1, -8, 0); // should be -8 for both, but causes continuity problems between the 2 halves.
	GenericTilemapSetOffsets(2, -8, 0);
	GenericTilemapCategoryConfig(0, 0x100);
	for (INT32 i = 0; i < 0x100 * 4; i++) {
		GenericTilemapSetCategoryEntry(0, i/4, i & 3, (DrvColPROM[0xc00 + i] == 7) ? 1 : 0);
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	M6502Exit();
	SekExit();
	HD63701Exit();

	NamcoSoundExit();
	BurnYM2151Exit();
	DACExit();

	c45RoadExit();
	
	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 tab[0x400];

	memset (sprite_mask_enable, 0, 0x40);
	
	for (INT32 i = 0; i < 0x400; i++)
	{
		INT32 r = DrvColPROM[i + 0x000] & 0xf;
		INT32 g = DrvColPROM[i + 0x400] & 0xf;
		INT32 b = DrvColPROM[i + 0x800] & 0xf;

		tab[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}

	for (INT32 i = 0; i < 0x0400; i++)
	{
		DrvPalette[i + 0x000] = tab[DrvColPROM[0x0c00 + i]];
		DrvPalette[i + 0x400] = tab[DrvColPROM[0x1000 + i] | 0x300];
		DrvPalette[i + 0x800] = tab[i & 0x1ff];
		DrvPalette[i + 0xc00] = tab[(i & 0xff) | 0x200]; // road

		if (DrvColPROM[0x1000 + i] == 0xfe) {
			sprite_mask_enable[i / 16] = 1;
		}
	}
}

static void draw_sprites(INT32 sprite_priority)
{
	UINT16 *mem1 = (UINT16*)(DrvSprBuf + 0x000);
	UINT16 *mem2 = (UINT16*)(DrvSprBuf + 0x100);
	INT32 need_mask = 0;

	for (INT32 i = 0x100-2; i >= 0; i -= 2)
	{
		INT32 scalex = (mem1[1 + i] & 0xfc00) << 1;
		INT32 scaley = (mem1[0 + i] & 0xfc00) << 1;
		INT32 pri = 7 - ((mem1[1 + i] & 0x3c0) >> 6);

		if (pri == sprite_priority && scalex && scaley)
		{
			INT32 x = mem2[1 + i] & 0x3ff;
			INT32 y = 512 - (mem2[0 + i] & 0x3ff);
			INT32 flipx = mem2[0 + i] & 0x4000;
			INT32 flipy = mem2[0 + i] & 0x8000;
			INT32 color = mem1[1 + i] & 0x3f;
			UINT8 *gfx;
			INT32 tsize;
			INT32 code;

			if (mem2[0 + i] & 0x2000)
			{
				gfx = DrvGfxROM3;
				tsize = 32;
				code = mem1[0 + i] & 0x3ff;
			}
			else
			{
				gfx = DrvGfxROM2;
				tsize = 16;
				code = mem1[0 + i] & 0x1ff;
				scaley *= 2;
			}

			if (sprite_mask_enable[color])
			{
				if (!need_mask) {
					// back up previous bitmap
					memcpy (DrvBitmap, pTransDraw, nScreenWidth * nScreenHeight * sizeof(UINT16));
				}
				need_mask = 1;
			}

			scalex += 0x800;
			scaley += 0x800;

			x -= (64+16);
			y -= 78;

			RenderZoomedPrioTranstabSprite(pTransDraw, gfx, code, (color*16)+0x400, 0xff, x, y, flipx, flipy, tsize, tsize, scalex, scaley, DrvColPROM + 0xc00, 1 << pri);
		}
	}

	if (need_mask)
	{
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			if (pTransDraw[i] == 0x063f) { //should be 0x3fe.  0x63f??
				pTransDraw[i] = DrvBitmap[i];
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

	INT32 bg_center = 144 - ((((scroll[0] + scroll[2]) & 0x1ff) - 288) / 2);

	if (bg_center == 288) bg_center = nScreenWidth;

	BurnTransferClear();

	GenericTilesSetClip(-1, ((bg_center + 8) < nScreenWidth) ? bg_center + 8 : bg_center, -1, -1); // hacky
	GenericTilemapSetScrollX(1, scroll[0] + 12);
	GenericTilemapSetScrollY(1, scroll[1] + 20);
	if (nBurnLayer & 1) GenericTilemapDraw(1, pTransDraw, 0);
	GenericTilesClearClip();

	GenericTilesSetClip((bg_center > 8) ? bg_center - 8 : 0, -1, -1, -1); // hacky sack
	GenericTilemapSetScrollX(2, scroll[2] + 20 - 4);
	GenericTilemapSetScrollY(2, scroll[3] + 20);
	if (nBurnLayer & 2) GenericTilemapDraw(2, pTransDraw, 0);
	GenericTilesClearClip();

	if (nBurnLayer & 4) {
		GenericTilesSetClip(-1, nScreenWidth-1, -1, -1); // c45 uses namcos2-style (0 based) clipping. we need to account for that.
		c45RoadDraw();
		GenericTilesClearClip();
	}

	for (INT32 i = 7; i >=0; i--) if (nSpriteEnable & (i << 1)) draw_sprites(i);
	
	if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 0);
	
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();
	M6502NewFrame();
	SekNewFrame();
	HD63701NewFrame();

	{
		memset (DrvInputs, 0xff, 2);

		DrvInputs[1] = (DrvInputs[1] & 0xfb) | (DrvDips[2] & 0x04);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[5] = { 1536000 / 60, 2048000 / 60, 2048000 / 60, 12288000 / 60, (1536000+100000) / 60 };
	INT32 nCyclesDone[5] = { 0, 0, 0, 0, 0 };

	M6809Open(0);
	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nNext = ((i + 1) * nCyclesTotal[0]) / nInterleave;
		nCyclesDone[0] += M6809Run(nNext - nCyclesDone[0]);
		if (i == (nInterleave - 1)) {
			if (m6809_irq_enable) {
				M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);
			} else {
				m6809_irq_enable = 1;
			}
		}

		M6502Open(0);
		nNext = ((i + 1) * nCyclesTotal[1]) / nInterleave;
		nCyclesDone[1] += M6502Run(nNext - nCyclesDone[1]);
		M6502Close();

		M6502Open(1);
		nNext = ((i + 1) * nCyclesTotal[2]) / nInterleave;
		nCyclesDone[2] += M6502Run(nNext - nCyclesDone[2]);
		M6502Close();

		nNext = ((i + 1) * nCyclesTotal[3]) / nInterleave;
		nCyclesDone[3] += SekRun(nNext - nCyclesDone[3]);
		if (i == (nInterleave - 1) && m68000_irq_enable) {
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		}

		HD63701Open(0);
		nNext = ((i + 1) * nCyclesTotal[4]) / nInterleave;
		nCyclesDone[4] += HD63701Run(nNext - nCyclesDone[4]);
		if (i == (nInterleave - 1)) {
			if (mcu_irq_enable) {
				HD63701SetIRQLine(0, CPU_IRQSTATUS_HOLD);
			} else {
				mcu_irq_enable = 1;
			}
		}
		HD63701Close();

		if (pBurnSoundOut && (i%4)==3) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 4);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			NamcoSoundUpdateStereo(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			NamcoSoundUpdateStereo(pSoundBuf, nSegmentLength);
		}
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();
	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x200);
	
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
		M6809Scan(nAction);
		M6502Scan(nAction);
		HD63701Scan(nAction);

		NamcoSoundScan(nAction, pnMin);
		BurnYM2151Scan(nAction, pnMin);
		DACScan(nAction,pnMin);

		c45RoadState(nAction);

		SCAN_VAR(scroll);
		SCAN_VAR(m68000_irq_enable);
		SCAN_VAR(m6809_irq_enable);
		SCAN_VAR(mcu_irq_enable);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x001800;
		ba.szName	= "NVRAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Thunder Ceptor

static struct BurnRomInfo tceptorRomDesc[] = {
	{ "tc1-1.10f",		0x08000, 0x4c6b063e,  1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code

	{ "tc1-21.1m",		0x08000, 0x2d0b2fa8,  2 | BRF_PRG | BRF_ESS }, //  1 M6502 Code (YM2151)

	{ "tc1-22.3m",		0x08000, 0x9f5a3e98,  3 | BRF_GRA },           //  2 M6502 Code (DAC)

	{ "tc1-4.8c",		0x08000, 0xae98b673,  4 | BRF_PRG | BRF_ESS }, //  3 M68K Code
	{ "tc1-3.10c",		0x08000, 0x779a4b25,  4 | BRF_PRG | BRF_ESS }, //  4

	{ "tc1-2.3a",		0x04000, 0xb6def610,  5 | BRF_PRG | BRF_ESS }, //  5 HD63701 Code
	{ "cus60-60a1.mcu",	0x01000, 0x076ea82a,  5 | BRF_PRG | BRF_ESS }, //  6

	{ "tc1-18.6b",		0x02000, 0x662b5650,  6 | BRF_GRA },           //  7 Characters

	{ "tc1-20.10e",		0x08000, 0x3e5054b7,  7 | BRF_GRA },           //  8 Background Tiles
	{ "tc1-19.10d",		0x04000, 0x7406e6e7,  7 | BRF_GRA },           //  9

	{ "tc1-16.8t",		0x08000, 0x7c72be33,  8 | BRF_GRA },           // 10 16x16 Sprites
	{ "tc1-15.10t",		0x08000, 0x51268075,  8 | BRF_GRA },           // 11

	{ "tc1-8.8m",		0x10000, 0x192a1f1f,  9 | BRF_GRA },           // 12 32x32 Sprites
	{ "tc1-10.8p",		0x08000, 0x7876bcef,  9 | BRF_GRA },           // 13
	{ "tc1-12.8r",		0x08000, 0xe8f55842,  9 | BRF_GRA },           // 14
	{ "tc1-14.8s",		0x08000, 0x723acf62,  9 | BRF_GRA },           // 15
	{ "tc1-7.10m",		0x10000, 0x828c80d5,  9 | BRF_GRA },           // 16
	{ "tc1-9.10p",		0x08000, 0x145cf59b,  9 | BRF_GRA },           // 17
	{ "tc1-11.10r",		0x08000, 0xad7c6c7e,  9 | BRF_GRA },           // 18
	{ "tc1-13.10s",		0x08000, 0xe67cef29,  9 | BRF_GRA },           // 19

	{ "tc1-3.1k",		0x00400, 0xfd2fcb57, 10 | BRF_GRA },           // 20 Color Data
	{ "tc1-1.1h",		0x00400, 0x0241cf67, 10 | BRF_GRA },           // 21
	{ "tc1-2.1j",		0x00400, 0xea9eb3da, 10 | BRF_GRA },           // 22
	{ "tc1-5.6a",		0x00400, 0xafa8eda8, 10 | BRF_GRA },           // 23
	{ "tc1-6.7s",		0x00400, 0x72707677, 10 | BRF_GRA },           // 24
	{ "tc1-4.2e",		0x00100, 0xa4e73d53, 10 | BRF_GRA },           // 25
	{ "tc1-17.7k",		0x02000, 0x90db1bf6, 10 | BRF_GRA },           // 26
};

STD_ROM_PICK(tceptor)
STD_ROM_FN(tceptor)

static INT32 TceptorInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvTceptor = {
	"tceptor", NULL, NULL, NULL, "1986",
	"Thunder Ceptor\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, tceptorRomInfo, tceptorRomName, NULL, NULL, NULL, NULL, TceptorInputInfo, TceptorDIPInfo,
	TceptorInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	272, 224, 4, 3
};


// Thunder Ceptor II

static struct BurnRomInfo tceptor2RomDesc[] = {
	{ "tc2-1.10f",		0x08000, 0xf953f153,  1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code

	{ "tc1-21.1m",		0x08000, 0x2d0b2fa8,  2 | BRF_PRG | BRF_ESS }, //  1 M6502 Code (YM2151)

	{ "tc1-22.3m",		0x08000, 0x9f5a3e98,  3 | BRF_GRA },           //  2 M6502 Code (DAC)

	{ "tc2-4.8c",		0x08000, 0x6c2efc04,  4 | BRF_PRG | BRF_ESS }, //  3 M68K Code
	{ "tc2-3.10c",		0x08000, 0x312b781a,  4 | BRF_PRG | BRF_ESS }, //  4
	{ "tc2-6.8d",		0x08000, 0x20711f14,  4 | BRF_PRG | BRF_ESS }, //  5
	{ "tc2-5.10d",		0x08000, 0x925f2560,  4 | BRF_PRG | BRF_ESS }, //  6

	{ "tc1-2.3a",		0x04000, 0xb6def610,  5 | BRF_PRG | BRF_ESS }, //  7 HD63701 Code
	{ "cus60-60a1.mcu",	0x01000, 0x076ea82a,  5 | BRF_PRG | BRF_ESS }, //  8

	{ "tc1-18.6b",		0x02000, 0x662b5650,  6 | BRF_GRA },           //  9 Characters

	{ "tc2-20.10e",		0x08000, 0xe72738fc,  7 | BRF_GRA },           // 10 Background Tiles
	{ "tc2-19.10d",		0x04000, 0x9c221e21,  7 | BRF_GRA },           // 11

	{ "tc2-16.8t",		0x08000, 0xdcf4da96,  8 | BRF_GRA },           // 12 16x16 Sprites
	{ "tc2-15.10t",		0x08000, 0xfb0a9f89,  8 | BRF_GRA },           // 13

	{ "tc2-8.8m",		0x10000, 0x03528d79,  9 | BRF_GRA },           // 14 32x32 Sprites
	{ "tc2-10.8p",		0x10000, 0x561105eb,  9 | BRF_GRA },           // 15
	{ "tc2-12.8r",		0x10000, 0x626ca8fb,  9 | BRF_GRA },           // 16
	{ "tc2-14.8s",		0x10000, 0xb9eec79d,  9 | BRF_GRA },           // 17
	{ "tc2-7.10m",		0x10000, 0x0e3523e0,  9 | BRF_GRA },           // 18
	{ "tc2-9.10p",		0x10000, 0xccfd9ff6,  9 | BRF_GRA },           // 19
	{ "tc2-11.10r",		0x10000, 0x40724380,  9 | BRF_GRA },           // 20
	{ "tc2-13.10s",		0x10000, 0x519ec7c1,  9 | BRF_GRA },           // 21

	{ "tc2-3.1k",		0x00400, 0xe3504f1a, 10 | BRF_GRA },           // 22 Color Data
	{ "tc2-1.1h",		0x00400, 0xe8a96fda, 10 | BRF_GRA },           // 23
	{ "tc2-2.1j",		0x00400, 0xc65eda61, 10 | BRF_GRA },           // 24
	{ "tc1-5.6a",		0x00400, 0xafa8eda8, 10 | BRF_GRA },           // 25
	{ "tc2-6.7s",		0x00400, 0xbadcda76, 10 | BRF_GRA },           // 26
	{ "tc2-4.2e",		0x00100, 0x6b49fc30, 10 | BRF_GRA },           // 27
	{ "tc1-17.7k",		0x02000, 0x90db1bf6, 10 | BRF_GRA },           // 28
};

STD_ROM_PICK(tceptor2)
STD_ROM_FN(tceptor2)

static INT32 Tceptor2Init()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvTceptor2 = {
	"tceptor2", "tceptor", NULL, NULL, "1986",
	"Thunder Ceptor II\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, tceptor2RomInfo, tceptor2RomName, NULL, NULL, NULL, NULL, TceptorInputInfo, TceptorDIPInfo,
	Tceptor2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	272, 224, 4, 3
};
