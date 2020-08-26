// FinalBurn Neo Konami Hot Chase and WEC Le Mans 24 driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "m6809_intf.h"
#include "burn_ym2151.h"
#include "k007232.h"
#include "bitswap.h"
#include "konamiic.h"
#include "burn_pal.h"
#include "burn_led.h"
#include "burn_shift.h"

static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *Drv68KROM[2];
static UINT8 *DrvM6809ROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[4];
static UINT8 *DrvSndROM[3];
static UINT8 *Drv68KRAM[3];
static UINT8 *DrvPalRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvPageRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvRoadRAM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 selected_ip;
static UINT16 irq_control;
static UINT16 protection_state;
static UINT16 protection_ram[3];
static UINT16 blitter_regs[16];
static UINT8 multiply_reg[2];
static UINT8 soundbank;
static UINT8 soundlatch;
static UINT8 sound_status;
static INT32 irq_timer;

static INT32 pages[2][4];

struct sprite_t
{
	UINT8 *pen_data;
	INT32 line_offset;

	UINT32 pal_base;

	INT32 x_offset;
	INT32 y_offset;
	INT32 tile_width;
	INT32 tile_height;
	INT32 total_width;
	INT32 total_height;
	INT32 x;
	INT32 y;
	INT32 shadow_mode;
	INT32 flags;
};

static struct sprite_t sprite_list[256];
static INT32 spr_idx_list[256];
static INT32 spr_pri_list[256];
static struct sprite_t *spr_ptr_list[256];
static INT32 spr_count;

static INT32 game_select = 0; // 0 - wecleman / 1 = hotchase
static INT32 spr_color_offs;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;

static INT16 Analog0;
static INT16 Analog1;

static INT32 scanline;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d} // A - for analog happytime
static struct BurnInputInfo WeclemanInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	A("P1 Steering",	BIT_ANALOG_REL,	&Analog0,		"p1 x-axis"	),
	A("P1 Accelerator",	BIT_ANALOG_REL,	&Analog1,		"p1 fire 1"	),
	{"P1 Brake",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	{"P1 Gear Shift",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"service"	},
	{"Service 4",		BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},

	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 2,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};
#undef A
STDINPUTINFO(Wecleman)

static struct BurnDIPInfo WeclemanDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xff, NULL					},
	{0x0d, 0xff, 0xff, 0xdf, NULL					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x0c, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x0c, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x0c, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x0c, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x0c, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x0c, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x0c, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x0c, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x0c, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x0c, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x0c, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x0c, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x0c, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x0c, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x0c, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x0c, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x0c, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x0c, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x0c, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x0c, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x0c, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x0c, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x0c, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x0c, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x0c, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x0c, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x0c, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x0c, 0x01, 0xf0, 0x00, "No Coin B"			},

	{0   , 0xfe, 0   ,    2, "Speed Unit"			},
	{0x0d, 0x01, 0x01, 0x01, "km/h"					},
	{0x0d, 0x01, 0x01, 0x00, "mph"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0d, 0x01, 0x18, 0x18, "Easy"					},
	{0x0d, 0x01, 0x18, 0x10, "Normal"				},
	{0x0d, 0x01, 0x18, 0x08, "Hard"					},
	{0x0d, 0x01, 0x18, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0d, 0x01, 0x20, 0x20, "Off"					},
	{0x0d, 0x01, 0x20, 0x00, "On"					},
};

STDDIPINFO(Wecleman)

static struct BurnDIPInfo HotchaseDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x7f, NULL					},
	{0x0d, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Speed Unit"			},
	{0x0c, 0x01, 0x01, 0x01, "KM"					},
	{0x0c, 0x01, 0x01, 0x00, "M.P.H."				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0c, 0x01, 0x80, 0x80, "Off"					},
	{0x0c, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x0d, 0x01, 0x0f, 0x02, "5 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0f, 0x04, "4 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0f, 0x07, "3 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0a, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0f, 0x01, "5 Coins 3 Credits"	},
	{0x0d, 0x01, 0x0f, 0x06, "3 Coins 2 Credits"	},
	{0x0d, 0x01, 0x0f, 0x03, "4 Coins 3 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x0f, 0x05, "3 Coins 4 Credits"	},
	{0x0d, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0x0f, 0x08, "2 Coins 5 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x0d, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    15, "Coin B"				},
	{0x0d, 0x01, 0xf0, 0x20, "5 Coins 1 Credits"	},
	{0x0d, 0x01, 0xf0, 0x70, "3 Coins 1 Credits"	},
	{0x0d, 0x01, 0xf0, 0xa0, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0xf0, 0x10, "5 Coins 3 Credits"	},
	{0x0d, 0x01, 0xf0, 0x60, "3 Coins 2 Credits"	},
	{0x0d, 0x01, 0xf0, 0x30, "4 Coins 3 Credits"	},
	{0x0d, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0xf0, 0x50, "3 Coins 4 Credits"	},
	{0x0d, 0x01, 0xf0, 0x90, "2 Coins 3 Credits"	},
	{0x0d, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0xf0, 0x80, "2 Coins 5 Credits"	},
	{0x0d, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x0d, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x0d, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x0d, 0x01, 0xf0, 0x00, "1 Coin/99 Credits"	},
};

STDDIPINFO(Hotchase)

#define BLITTER_DEBUG 0

static void blitter_write()
{
	INT32 minterm  = blitter_regs[0] >> 8;
	INT32 list_len = blitter_regs[0] & 0xFF;
	INT32 src  = (blitter_regs[2] << 16) + (blitter_regs[3] & 0xfffe);
	INT32 list = (blitter_regs[4] << 16) + (blitter_regs[5] & 0xfffe);
	INT32 dest = (blitter_regs[6] << 16) + (blitter_regs[7] & 0xfffe);
	INT32 size =  blitter_regs[8] & 0xff;

#if BLITTER_DEBUG
	bprintf(0, _T("\n\---\nminterm %02x\n"), minterm);
	bprintf(0, _T("size: %02x\n"), size);
	bprintf(0, _T("list_len: %02x\n"), list_len);
#endif

	if (minterm != 2)
	{
		for ( ; size > 0 ; size--)
		{
			SekWriteWord(dest, SekReadWord(src));
			src += 2;
			dest += 2;
		}
	}
	else
	{
		for ( ; list_len > 0 ; list_len-- )
		{
			INT32 i = src + SekReadWord(list+2);
			INT32 j = i + (size << 1);
			INT32 destptr = dest;

			for (; i<j; destptr+=2, i+=2)
				SekWriteWord(destptr, SekReadWord(i));

			destptr = dest + 14;
			i = SekReadWord(list) + spr_color_offs;
			SekWriteWord(destptr, i);

			dest += 16;
			list += 4;
		}

		SekWriteWord(dest, 0xffff);
	}
}

static void __fastcall wecleman_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffe0) == 0x080000) {
#if BLITTER_DEBUG
		bprintf(0, _T("blitter_w.w %x      %x            \tframe %d  cyc %d   scanline %d\n"), address, data, nCurrentFrame, SekTotalCycles(), scanline/8);
#endif
		blitter_regs[(address & 0x1e) / 2] = data;
		if (address == 0x080010) blitter_write();
		return;
	}

	if ((address & 0xffd000) == 0x100000) {
		K051316Write((address / 0x2000) & 1, (address / 2) & 0x7ff, data);
		return;
	}

	if ((address & 0xffdfe0) == 0x101000) {
		K051316WriteCtrl((address / 0x2000) & 1, (address / 2) & 0x0f, data);
		return;
	}

	switch (address)
	{
		case 0x060004:
			protection_state = data & 0x2000;
		case 0x060000:
		case 0x060002:
			if (protection_state == 0) protection_ram[(address / 2) & 3] = data;
		return;

		case 0x140000:
		case 0x140001:
			soundlatch = data;
		return;

		case 0x140002:
		case 0x140003:
			selected_ip = (data >> 5) & 3;
		return;

		case 0x140004:
		case 0x140005:
			{
				if ((irq_control & 1) && (~data & 1)) {
					INT32 cyc = (SekTotalCycles(0) - SekTotalCycles(1));
					if (cyc > 0) {
						SekRun(1, cyc);
					}
					SekSetIRQLine(1, 4, CPU_IRQSTATUS_AUTO);
				}
				if ((irq_control & 4) && (~data & 4)) {
					if (game_select) {
						M6809SetIRQLine(0, 0, CPU_IRQSTATUS_HOLD);
						sound_status = 0;
					} else {
						ZetSetIRQLine(0, 0, CPU_IRQSTATUS_HOLD);
					}
				}
				if ((irq_control & 8) && (~data & 8) && game_select) {
					M6809Reset(0);
				}
				SekSetRESETLine(1, ~data & 0x002);
				irq_control = data;
			}
		return;

		case 0x140006:
		case 0x140007:
		return; // nop

		case 0x140020:
		case 0x140021:
		return; // nop

		case 0x140030:
		case 0x140031:
		return; // nop
	}
}

static void __fastcall wecleman_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffffe0) == 0x080000) {
#if BLITTER_DEBUG
		bprintf(0, _T("blitter_w.b %x      %x            \tframe %d  cyc %d   scanline %d\n"), address, data, nCurrentFrame, SekTotalCycles(), scanline/8);
#endif
		UINT8 *bregs = (UINT8*)blitter_regs;

#ifdef LSB_FIRST
		bregs[(address & 0x1f) ^ 1] = data;
#else
		bregs[(address & 0x1f)] = data;
#endif			
		if (address == 0x080010) blitter_write();
		return;
	}

	if ((address & 0xffd000) == 0x100000) {
		K051316Write((address / 0x2000) & 1, (address / 2) & 0x7ff, data);
		return;
	}

	if ((address & 0xffdfe0) == 0x101000) {
		K051316WriteCtrl((address / 0x2000) & 1, (address / 2) & 0x0f, data);
		return;
	}

	switch (address)
	{
		case 0x140001:
			soundlatch = data;
		return;

		case 0x140003:
			selected_ip = (data >> 5) & 3;
		return;

		case 0x140005:
			wecleman_main_write_word(address, data); // irq stuff, see above
		return;

		case 0x140007:
		return; // nop - watchdog?

		case 0x140021:
		return; // adc
	}
}

static UINT16 wecleman_protection_r()
{
	INT32 blend, data0, data1, r0, g0, b0, r1, g1, b1;

	data0 = protection_ram[0];
	blend = protection_ram[2];
	data1 = protection_ram[1];
	blend &= 0x3ff;

	r0 = data0;  g0 = data0;  b0 = data0;
	r0 &= 0xf;   g0 &= 0xf0;  b0 &= 0xf00;
	r1 = data1;  g1 = data1;  b1 = data1;
	r1 &= 0xf;   g1 &= 0xf0;  b1 &= 0xf00;
	r1 -= r0;    g1 -= g0;    b1 -= b0;
	r1 *= blend; g1 *= blend; b1 *= blend;
	r1 >>= 10;   g1 >>= 10;   b1 >>= 10;
	r0 += r1;    g0 += g1;    b0 += b1;
	g0 &= 0xf0;  b0 &= 0xf00;

	r0 |= g0;
	r0 |= b0;

	return r0;
}

static UINT8 steer_nice()
{
	// logarithmic curve for nicer steering - dink (June 05, 2020)

	const UINT8 curve[] = {
		0x00, 0x2c, 0x52, 0x56, 0x59, 0x5b, 0x5d, 0x5f, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
		0x68, 0x69, 0x6a, 0x6a, 0x6b, 0x6b, 0x6c, 0x6d, 0x6d, 0x6d, 0x6e, 0x6e, 0x6f, 0x6f, 0x70, 0x70,
		0x70, 0x71, 0x71, 0x71, 0x72, 0x72, 0x72, 0x73, 0x73, 0x73, 0x73, 0x74, 0x74, 0x74, 0x75, 0x75,
		0x75, 0x75, 0x76, 0x76, 0x76, 0x76, 0x76, 0x77, 0x77, 0x77, 0x77, 0x77, 0x78, 0x78, 0x78, 0x78,
		0x78, 0x79, 0x79, 0x79, 0x79, 0x79, 0x79, 0x7a, 0x7a, 0x7a, 0x7a, 0x7a, 0x7a, 0x7b, 0x7b, 0x7b,
		0x7b, 0x7b, 0x7b, 0x7b, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7c, 0x7d, 0x7d, 0x7d, 0x7d,
		0x7d, 0x7d, 0x7d, 0x7d, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7e, 0x7f, 0x7f, 0x7f,
		0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
		0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
		0x81, 0x81, 0x81, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x82, 0x83, 0x83, 0x83, 0x83,
		0x83, 0x83, 0x83, 0x83, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x84, 0x85, 0x85, 0x85, 0x85,
		0x85, 0x85, 0x85, 0x86, 0x86, 0x86, 0x86, 0x86, 0x86, 0x87, 0x87, 0x87, 0x87, 0x87, 0x87, 0x88,
		0x88, 0x88, 0x88, 0x88, 0x89, 0x89, 0x89, 0x89, 0x89, 0x8a, 0x8a, 0x8a, 0x8a, 0x8a, 0x8b, 0x8b,
		0x8b, 0x8b, 0x8c, 0x8c, 0x8c, 0x8d, 0x8d, 0x8d, 0x8d, 0x8e, 0x8e, 0x8e, 0x8f, 0x8f, 0x8f, 0x90,
		0x90, 0x90, 0x91, 0x91, 0x92, 0x92, 0x93, 0x93, 0x93, 0x94, 0x95, 0x95, 0x96, 0x96, 0x97, 0x98,
		0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa1, 0xa3, 0xa5, 0xa7, 0xaa, 0xae, 0xe4, 0xff
	};

	return curve[ProcessAnalog(Analog0, 0, INPUT_DEADZONE, 0x00, 0xff)];
}

static UINT16 __fastcall wecleman_main_read_word(UINT32 address)
{
	if ((address & 0xffffe0) == 0x080000) {
		return blitter_regs[(address & 0x1e) / 2];
	}

	if ((address & 0xffd000) == 0x100000) {
		return K051316Read((address / 0x2000) & 1, (address / 2) & 0x7ff);
	}

	if ((address & 0xffdfe0) == 0x101000) {
		return K051316ReadCtrl((address / 0x2000) & 1, (address / 2) & 0x0f);
	}

	switch (address)
	{
		case 0x060006:
			return wecleman_protection_r();

		case 0x140010:
			return DrvInputs[0]; // coins + brake + gear

		case 0x140012: {
			UINT8 ret = DrvInputs[1];
			if (game_select && sound_status) ret |= 0x10;
			return ret;
		}

		case 0x140014:
			return DrvDips[0];

		case 0x140016:
			return DrvDips[1];

		case 0x140020:
			switch (selected_ip) {
				case 0: return ProcessAnalog(Analog1, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0x80);
				case 2: return steer_nice();
				default: return 0xffff;
			}
			break;
	}

	return 0;
}

static UINT8 __fastcall wecleman_main_read_byte(UINT32 address)
{
	if ((address & 0xffd000) == 0x100000) {
		return K051316Read((address / 0x2000) & 1, (address / 2) & 0x7ff);
	}

	if ((address & 0xffdfe0) == 0x101000) {
		return K051316ReadCtrl((address / 0x2000) & 1, (address / 2) & 0x0f);
	}

	switch (address)
	{
		case 0x140006:
		case 0x140007:
			return 0; // watchdog

		case 0x140011:
			return DrvInputs[0]; // coins + brake + gear

		case 0x140013: {
			UINT8 ret = DrvInputs[1];
			if (game_select && sound_status) ret |= 0x10;
			return ret; // in1
		}

		case 0x140015:
			return DrvDips[0];

		case 0x140016:
		case 0x140017:
			return DrvDips[1];

		case 0x140021:
			switch (selected_ip) {
				case 0: return ProcessAnalog(Analog1, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0x80);
				case 2: return steer_nice();
				default: return 0xff;
			}
			break;
	}

	return 0;
}

static void bankswitch(INT32 data)
{
	soundbank = data & 1;

	k007232_set_bank(0, 0, ~data & 1);
}

static void __fastcall wecleman_sound_write(UINT16 address, UINT8 data)
{
	if (address >= 0xb000 && address <= 0xb00d) {
		K007232WriteReg(0, address & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0x8500:
		return; // nop

		case 0x9000:
		case 0x9001:
			multiply_reg[address & 1] = data;
		return;

		case 0x9006:
		return; // nop

		case 0xc000:
		case 0xc001:
			BurnYM2151Write(address, data);
		return;

		case 0xf000:
			bankswitch(data);
		return;
	}
}

static UINT8 __fastcall wecleman_sound_read(UINT16 address)
{
	if (address >= 0xb000 && address <= 0xb00d) {
		return K007232ReadReg(0, address & 0xf);
	}

	switch (address)
	{
		case 0x9000:
			return multiply_reg[0] * multiply_reg[1];

		case 0xa000:
			if (soundlatch == 0) {
				K007232Reset(0);
			}
			return soundlatch;

		case 0xc000:
		case 0xc001:
			return BurnYM2151Read();
	}

	return 0;
}

static void hotchase_sound_write(UINT16 address, UINT8 data)
{
	if (address >= 0x1000 && address <= 0x3fff) {
		if ((address & 0xfff) > 0x00d) return;
		K007232WriteReg((address - 0x1000) >> 12, (address & 0xf) ^ 1, data);
		return;
	}

	switch (address)
	{
		case 0x4000:
		case 0x4001:
		case 0x4002:
		case 0x4003:
		case 0x4004:
		case 0x4005:
			K007232SetVolume((address & 7) / 2, address & 1, (data&0x0f) * 0x08, (data>>4) * 0x08);
		return;
		case 0x4006:
			k007232_set_bank(0, (data >> 1) & 1, (data >> 3) & 1);
			k007232_set_bank(1, (data >> 2) & 1, (data >> 4) & 1);
		return;
		case 0x4007:
			k007232_set_bank(2, (data >> 0) & 7, (data >> 3) & 7);
		return;

		case 0x7000:
			sound_status = 1;
		return;
	}
}

static UINT8 hotchase_sound_read(UINT16 address)
{
	if (address >= 0x1000 && address <= 0x3fff) {
		if ((address & 0xfff) > 0x00d) return 0;
		return K007232ReadReg((address - 0x1000) >> 12, address & 0xf);
	}

	switch (address)
	{
		case 0x6000:
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 page = pages[0][((offs & 0x40) >> 6) | ((offs & 0x1000) >> 11)];
	INT32 code = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvPageRAM + ((offs & 0x3f) + ((offs & 0xf80) >> 1) + (page << 11)) * 2)));
	
	TILE_SET_INFO(1, code, ((code & 0xf00) >> 5) + (code >> 12), 0);
}

static tilemap_callback( fg )
{
	INT32 page = pages[1][((offs & 0x40) >> 6) | ((offs & 0x1000) >> 11)];
	INT32 code = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvPageRAM + ((offs & 0x3f) + ((offs & 0xf80) >> 1) + (page << 11)) * 2)));

	if (!code || code == 0xffff) code = 0x20; // blank space!!

	TILE_SET_INFO(1, code, ((code & 0xf00) >> 5) + (code >> 12), 0);
}

static tilemap_callback( tx )
{
	INT32 code = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvTxtRAM + offs * 2)));

	TILE_SET_INFO(1, code, ((code & 0xf00) >> 5) + (code >> 12), 0);
}

static void hotchase_zoom_callback_1(INT32 *code, INT32 *color, INT32 *)
{
	*code |= (*color & 0x03) << 8;
	*color = (*color & 0xfc) >> 2;
}

static void hotchase_zoom_callback_2(INT32 *code, INT32 *color, INT32 *)
{
	*color = ((*color & 0x3f) << 1) | ((*code & 0x80) >> 7);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	for (INT32 i = 0; i < 0x2000; i++) {
		DrvPalRAM[i] = 0xff; // white
	}

	SekOpen(0);
	SekReset();
	SekClose();

	SekOpen(1);
	SekReset();
	SekClose();

	if (game_select)
	{
		K051316Reset();
		K051316WrapEnable(0, 1);

		M6809Open(0);
		M6809Reset();
		M6809Close();

		K007232Reset(0);
		K007232Reset(1);
		K007232Reset(2);
	}
	else
	{
		ZetOpen(0);
		ZetReset();
		ZetClose();

		BurnYM2151Reset();
		K007232Reset(0);
	}

	BurnLEDReset();
	BurnShiftReset();

	memset (protection_ram, 0, sizeof(protection_ram));
	memset (blitter_regs, 0, sizeof(blitter_regs));
	memset (multiply_reg, 0, sizeof(multiply_reg));

	soundbank = 0;
	selected_ip = 0;
	irq_control = 0;
	protection_state = 0;
	soundlatch = 0;
	sound_status = 0;
	irq_timer = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM[0]	= Next; Next += 0x040000;
	Drv68KROM[1]	= Next; Next += 0x020000;

	DrvM6809ROM		= Next;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM[0]	= Next; Next += 0x800000; // sprites
	DrvGfxROM[1]	= Next; Next += 0x080000; // layers / roz (hotchase)
	DrvGfxROM[2]	= Next; Next += 0x080000; // roz (hotchase)
	DrvGfxROM[3]	= Next; Next += 0x080000; // road

	DrvSndROM[0]	= Next; Next += 0x040000;
	DrvSndROM[1]	= Next; Next += 0x040000;
	DrvSndROM[2]	= Next; Next += 0x100000;

	DrvPalette		= (UINT32*)Next; Next += 0x1002 * sizeof(UINT32); // + black & white + gray

	AllRam			= Next;

	DrvPalRAM		= Next; Next += 0x002000;
	Drv68KRAM[0]	= Next; Next += 0x004000;
	Drv68KRAM[1]	= Next; Next += 0x002000;
	Drv68KRAM[2]	= Next; Next += 0x001400;
	DrvTxtRAM		= Next; Next += 0x001000;
	DrvPageRAM		= Next; Next += 0x004000;
	DrvShareRAM		= Next; Next += 0x004000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvRoadRAM		= Next; Next += 0x001000;

	DrvM6809RAM		= Next;
	DrvZ80RAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void bitswap(UINT8 *src, INT32 len, INT32 _14,INT32 _13,INT32 _12,INT32 _11,INT32 _10,INT32 _f,INT32 _e,INT32 _d,INT32 _c,INT32 _b,INT32 _a,INT32 _9,INT32 _8,INT32 _7,INT32 _6,INT32 _5,INT32 _4,INT32 _3,INT32 _2,INT32 _1,INT32 _0)
{
	UINT8 *tmp = (UINT8*)BurnMalloc(len);

	memcpy(tmp, src, len);

	for (INT32 i = 0;i < len;i++)
	{
		src[i] = tmp[BITSWAP24(i,23,22,21,_14,_13,_12,_11,_10,_f,_e,_d,_c,_b,_a,_9,_8,_7,_6,_5,_4,_3,_2,_1,_0)];
		src[i] = (src[i] >> 7) | (src[i] << 1); // rotate - tiles are 0,7,6,5,4,3,2,1
	}

	BurnFree(tmp);
}

static void decode_and_unpack_sprites()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);

	memcpy (tmp, DrvGfxROM[0], 0x200000);

	for (INT32 i = 0; i < 0x200000; i++)
	{
		UINT8 data = BITSWAP08(tmp[BITSWAP24(i,23,22,21,0,1,20,19,18,17,14,9,16,6,4,7,8,15,10,11,13,5,12,3,2)],7,0,1,2,3,4,5,6);

		if ((data & 0xf0) == 0xf0) data &= 0x0f;
		if ((data & 0x0f) == 0x0f) data &= 0xf0;

		DrvGfxROM[0][i*2+1] = data & 0xf;
		DrvGfxROM[0][i*2+0] = data >> 4;
	}

	BurnFree(tmp);
}

static void hotchase_sprite_decode()
{
	INT32 num16_banks = 3;
	INT32 bank_size = 0x80000*2;
	UINT8 *base = DrvGfxROM[0]; // sprites
	UINT8 *temp = (UINT8*)BurnMalloc(bank_size);

	for (INT32 i = num16_banks; i > 0; i-- ){
		UINT8 *finish   = base + 2*bank_size*i;
		UINT8 *dest     = finish - 2*bank_size;

		UINT8 *p1 = &temp[0];
		UINT8 *p2 = &temp[bank_size/2];

		UINT8 data;

		memcpy (&temp[0], base+bank_size*(i-1), bank_size);

		do {
			data = *p1++;
			if( (data&0xf0) == 0xf0 ) data &= 0x0f;
			if( (data&0x0f) == 0x0f ) data &= 0xf0;
			*dest++ = data >> 4;
			*dest++ = data & 0xF;

			data = *p1++;
			if( (data&0xf0) == 0xf0 ) data &= 0x0f;
			if( (data&0x0f) == 0x0f ) data &= 0xf0;
			*dest++ = data >> 4;
			*dest++ = data & 0xF;

			data = *p2++;
			if( (data&0xf0) == 0xf0) data &= 0x0f;
			if( (data&0x0f) == 0x0f) data &= 0xf0;
			*dest++ = data >> 4;
			*dest++ = data & 0xF;

			data = *p2++;
			if( (data&0xf0) == 0xf0 ) data &= 0x0f;
			if( (data&0x0f) == 0x0f ) data &= 0xf0;
			*dest++ = data >> 4;
			*dest++ = data & 0xF;
		} while( dest<finish );
	}

	BurnFree(temp);
}

static INT32 WeclemanGfxDecode()
{
	INT32 Plane0[3] = { 0x08000*8*0, 0x08000*8*1, 0x08000*8*2 };
	INT32 Plane1[3] = { 0x04000*8*2, 0x04000*8*1, 0x04000*8*0 };
	INT32 XOffs[64] = { STEP8(7,-1), STEP8(15,-1), STEP8(23,-1), STEP8(31,-1), STEP8(39,-1), STEP8(47,-1), STEP8(55,-1), STEP8(63,-1) };
	INT32 YOffs[8]  = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x18000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[1], 0x018000);

	GfxDecode(0x1000, 3,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[3], 0x00c000);

	GfxDecode(0x0800, 3, 64,  1, Plane1, XOffs, YOffs, 0x040, tmp, DrvGfxROM[3]);

	BurnFree (tmp);

	return 0;
}

static INT32 HotchaseRoadDecode() // expand to 4bpp & double each pixel (wide)
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);

	memcpy (tmp, DrvGfxROM[3], 0x20000);

	INT32 XOffs[64] = {
		0*4,0*4,1*4,1*4,2*4,2*4,3*4,3*4,4*4,4*4,5*4,5*4,6*4,6*4,7*4,7*4,
		8*4,8*4,9*4,9*4,10*4,10*4,11*4,11*4,12*4,12*4,13*4,13*4,14*4,14*4,15*4,15*4,
		16*4,16*4,17*4,17*4,18*4,18*4,19*4,19*4,20*4,20*4,21*4,21*4,22*4,22*4,23*4,23*4,
		24*4,24*4,25*4,25*4,26*4,26*4,27*4,27*4,28*4,28*4,29*4,29*4,30*4,30*4,31*4,31*4
	};
	INT32 YOffs[1] = { 0 };
	INT32 Plane[4] = { 0, 1, 2, 3 };

	GfxDecode(0x2000, 4,  64,  1, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM[3]);

	BurnFree(tmp);

	return 0;
}

static INT32 WeclemanInit()
{
	game_select = 0;

	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM[0] + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM[0] + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM[0] + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM[0] + 0x020000, k++, 2)) return 1;

		if (BurnLoadRom(Drv68KROM[1] + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM[1] + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0c0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0e0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x100000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x120000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x140000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x160000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x180000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1c0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1e0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x008000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x010000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[3] + 0x000000, k++, 1)) return 1; // road
		if (BurnLoadRom(DrvGfxROM[3] + 0x008000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[0] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[0] + 0x020000, k++, 1)) return 1;

		decode_and_unpack_sprites();
		bitswap(DrvGfxROM[1], 0x18000, 20,19,18,17,16,15,12,7,14,4,2,5,6,13,8,9,11,3,10,1,0);
		bitswap(DrvGfxROM[3], 0x0c000, 20,19,18,17,16,15,14,7,12,4,2,5,6,13,8,9,11,3,10,1,0);
		WeclemanGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM[0],			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM[0],			0x040000, 0x043fff, MAP_RAM); // videostatus = 40494
	SekMapMemory(DrvPageRAM,			0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvTxtRAM,				0x108000, 0x108fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,				0x110000, 0x110fff, MAP_RAM);
	SekMapMemory(DrvShareRAM,			0x124000, 0x127fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,				0x130000, 0x130fff, MAP_RAM);
	SekSetWriteWordHandler(0,			wecleman_main_write_word);
	SekSetWriteByteHandler(0,			wecleman_main_write_byte);
	SekSetReadWordHandler(0,			wecleman_main_read_word);
	SekSetReadByteHandler(0,			wecleman_main_read_byte);

	SekInit(1, 0x68000); // no handlers
	SekOpen(1);
	SekMapMemory(Drv68KROM[1],			0x000000, 0x00ffff, MAP_RAM);
	SekMapMemory(DrvRoadRAM,			0x060000, 0x060fff, MAP_RAM);
	SekMapMemory(DrvShareRAM,			0x070000, 0x073fff, MAP_RAM);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0x8000, 0x83ff, MAP_RAM);
	ZetSetWriteHandler(wecleman_sound_write);
	ZetSetReadHandler(wecleman_sound_read);
	ZetClose();

	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.15, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.15, BURN_SND_ROUTE_RIGHT);

	K007232Init(0, 3579545, DrvSndROM[0], 0x40000);
	K007232PCMSetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 128, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 128, 64);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, tx_map_callback, 8, 8,  64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8, 8, 0x400000, 0, 0xff); // sprite
	GenericTilemapSetGfx(1, DrvGfxROM[1], 3,  8, 8, 0x040000, 0, 0xff); // layer
	GenericTilemapSetGfx(3, DrvGfxROM[3], 3, 64, 1, 0x020000, 0, 0xff); // road (3)!
	GenericTilemapSetScrollRows(0, 512);
	GenericTilemapSetScrollRows(1, 512);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);

	BurnBitmapAllocate(1, nScreenWidth, nScreenHeight, true);

	BurnLEDInit(1, LED_POSITION_TOP_LEFT, LED_SIZE_4x4, LED_COLOR_GREEN, 65);
	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_GREEN, 80);

	spr_color_offs = 0x40;

	DrvDoReset();

	return 0;
}

static INT32 HotchaseCommonInit(INT32 alt_set)
{
	game_select = 1;

	BurnAllocMemIndex();

	if (alt_set)
	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM[0] + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM[0] + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM[0] + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM[0] + 0x020000, k++, 2)) return 1;

		if (BurnLoadRom(Drv68KROM[1] + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM[1] + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6809ROM  + 0x008000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x020000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x040000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x060000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x060001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x080000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x080001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0a0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0a0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0c0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0c0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0e0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x0e0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x100000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x100001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x120000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x120001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x140000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x140001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x160000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x160001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x180000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x180001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1a0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1a0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1c0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1c0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1e0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x1e0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x200000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x200001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x220000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x220001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x240000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x240001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x260000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x260001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x280000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x280001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x2a0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x2a0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x2c0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x2c0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x2e0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x2e0001, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x000000, k++, 1)) return 1;
		memcpy (DrvGfxROM[2], DrvGfxROM[2] + 0x8000, 0x8000); // first half empty

		if (BurnLoadRom(DrvGfxROM[3] + 0x000000, k++, 1)) return 1; // road

		if (BurnLoadRom(DrvSndROM[0] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[0] + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[0] + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[0] + 0x030000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[1] + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[1] + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[1] + 0x030000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[2] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[2] + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[2] + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[2] + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[2] + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[2] + 0x0a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[2] + 0x0c0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[2] + 0x0e0000, k++, 1)) return 1;

		hotchase_sprite_decode();	// sprites (0)
		HotchaseRoadDecode();	// road (3)
	}
	else
	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM[0] + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM[0] + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM[0] + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM[0] + 0x020000, k++, 2)) return 1;

		if (BurnLoadRom(Drv68KROM[1] + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM[1] + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6809ROM  + 0x008000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x100000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x180000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x200000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x280000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x000000, k++, 1)) return 1;
		memcpy (DrvGfxROM[2], DrvGfxROM[2] + 0x8000, 0x8000); // first half empty

		if (BurnLoadRom(DrvGfxROM[3] + 0x000000, k++, 1)) return 1; // road

		if (BurnLoadRom(DrvSndROM[0] + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1] + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[2] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[2] + 0x080000, k++, 1)) return 1;

		BurnByteswap(DrvGfxROM[0], 0x300000); // word_swap
		hotchase_sprite_decode();	// sprites (0)
		HotchaseRoadDecode();	// road (3)
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM[0],			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM[1],			0x040000, 0x041fff, MAP_RAM);
	SekMapMemory(Drv68KRAM[0],			0x060000, 0x063fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,				0x110000, 0x111fff, MAP_RAM);
	SekMapMemory(DrvShareRAM,			0x120000, 0x123fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,				0x130000, 0x130fff, MAP_RAM);
	SekSetWriteWordHandler(0,			wecleman_main_write_word);
	SekSetWriteByteHandler(0,			wecleman_main_write_byte);
	SekSetReadWordHandler(0,			wecleman_main_read_word);
	SekSetReadByteHandler(0,			wecleman_main_read_byte);
	SekClose();

	SekInit(1, 0x68000); // no handlers
	SekOpen(1);
	SekMapMemory(Drv68KROM[1],			0x000000, 0x01ffff, MAP_RAM);
	SekMapMemory(DrvRoadRAM,			0x020000, 0x020fff, MAP_RAM);
	SekMapMemory(DrvShareRAM,			0x040000, 0x043fff, MAP_RAM);
	SekMapMemory(Drv68KRAM[2],			0x060000, 0x0613ff, MAP_RAM);
	SekClose();

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,			0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0x8000,0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(hotchase_sound_write);
	M6809SetReadHandler(hotchase_sound_read);
	M6809Close();

	K007232Init(0, 3579545, DrvSndROM[0], 0x040000);
	K007232Init(1, 3579545, DrvSndROM[1], 0x040000);
	K007232Init(2, 3579545, DrvSndROM[2], 0x100000);
	K007232PCMSetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	K007232PCMSetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);
	K007232PCMSetAllRoutes(2, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4,  8, 8, 0x600000, 0, 0xff); // sprite
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,  8, 8, 0x080000, 0, 0xff); // roz
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4,  8, 8, 0x080000, 0, 0xff); // roz
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4, 64, 1, 0x040000, 0, 0xff); // road (3)!

	K051316Init(0, DrvGfxROM[1], DrvGfxROM[1], 0x3ffff, hotchase_zoom_callback_1, 4, 0);
	K051316Init(1, DrvGfxROM[2], DrvGfxROM[2], 0x07fff, hotchase_zoom_callback_2, 4, 0);
	K051316SetOffset(0, -0xb0 / 2, -16);
	K051316SetOffset(1, -0xb0 / 2, -16);

	BurnLEDInit(1, LED_POSITION_TOP_LEFT, LED_SIZE_4x4, LED_COLOR_GREEN, 65);
	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_GREEN, 80);

	spr_color_offs = 0;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	K007232Exit();
	KonamiICExit();

	SekExit();

	if (game_select) {
		M6809Exit();
		K051316Exit();
	} else {
		ZetExit();
		BurnYM2151Exit();
	}

	BurnLEDExit();
	BurnShiftExit();

	BurnFreeMemIndex();

	return 0;
}

static void WeclemanPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x800; i++)
	{
		UINT8 r = pal4bit(BURN_ENDIAN_SWAP_INT16(p[i]) >> 0);
		UINT8 g = pal4bit(BURN_ENDIAN_SWAP_INT16(p[i]) >> 4);
		UINT8 b = pal4bit(BURN_ENDIAN_SWAP_INT16(p[i]) >> 8);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
		DrvPalette[i + 0x800] = BurnHighCol(r >> 1, g >> 1, b >> 1, 0);
	}

	DrvPalette[0x1000] = 0;
	DrvPalette[0x1001] = BurnHighCol(0xff, 0xff, 0xff, 0);
}

static void wecleman_draw_road(INT32 priority)
{
// must be powers of 2
#define XSIZE 512
#define YSIZE 256

#define YMASK (YSIZE-1)

#define DST_WIDTH 320
#define DST_HEIGHT 224

#define MIDCURB_DY 5
#define TOPCURB_DY 7

	static UINT32 road_color[48] =
	{
		0x03f1, 0x03f3, 0x03f5, 0x03fd, 0x03fd, 0x03fb, 0x03fd, 0x07ff,    // road color 0
		0x03f0, 0x03f2, 0x03f4, 0x03fc, 0x03fc, 0x03fb, 0x03fc, 0x07fe,    // road color 1
		0x1001, 0x1001, 0x1001, 0x03f9, 0x03f9, 0x1001, 0x1001, 0x1001,    // midcurb color 0
		0x1001, 0x1001, 0x1001, 0x03f8, 0x03f8, 0x1001, 0x1001, 0x1001,    // midcurb color 1
		0x1001, 0x1001, 0x1001, 0x03f7, 0x1001, 0x1001, 0x1001, 0x1001,    // topcurb color 0
		0x1001, 0x1001, 0x1001, 0x03f6, 0x1001, 0x1001, 0x1001, 0x1001     // topcutb color 1
	};

	UINT16 *roadram = (UINT16*)DrvRoadRAM;

	INT32 scrollx, sy, sx;

	if (priority == 0x02)
	{
		for (sy=0; sy<nScreenHeight; sy++)
		{
			UINT16 road = BURN_ENDIAN_SWAP_INT16(roadram[sy]);
			if ((road>>8) != 0x02) continue;
			UINT16 *dst = pTransDraw + sy * nScreenWidth;

			for (sx = 0; sx < 512; sx++)
				if (sx < nScreenWidth) dst[sx] = (BURN_ENDIAN_SWAP_INT16(roadram[sy+(YSIZE*2)]) & 0xf) + 0x7f0;
		}
	}
	else if (priority == 0x04)
	{
		for (sy=0; sy<nScreenHeight; sy++)
		{
			UINT16 *dst = pTransDraw + sy * nScreenWidth;

			UINT16 road = BURN_ENDIAN_SWAP_INT16(roadram[sy]);
			if ((road>>8) != 0x04) continue;
			road &= YMASK;

			UINT8 *src_ptr = DrvGfxROM[3] + (((road * 8) * 64) & 0x1ffff);
			INT32 mdy = ((road * MIDCURB_DY) >> 8) * nScreenWidth;
			INT32 tdy = ((road * TOPCURB_DY) >> 8) * nScreenWidth;

			scrollx = BURN_ENDIAN_SWAP_INT16(roadram[sy+YSIZE]) + (0x18 - 0xe00);

			UINT32 *pal_ptr = road_color + ((BURN_ENDIAN_SWAP_INT16(roadram[sy+(YSIZE*2)])<<3) & 8);

			for (sx = 0; sx < DST_WIDTH; sx++, scrollx++)
			{
				if (scrollx >= 0 && scrollx < 512)
				{
					UINT32 pix = src_ptr[scrollx];
					if (sx < nScreenWidth) dst[sx] = pal_ptr[pix];

					UINT32 temp = pal_ptr[pix + 16];
					if (temp != 0x1001 && (sx - mdy) >= 0) dst[sx - mdy] = temp;

					temp = pal_ptr[pix + 32];
					if (temp != 0x1001 && (sx - tdy) >= 0) dst[sx - tdy] = temp;
				}
				else
					dst[sx] = pal_ptr[7];
			}
		}
	}
#undef YSIZE
#undef XSIZE
}

static void get_sprite_info(INT32 spr_offsx, INT32 spr_offsy)
{
#define SPRITE_FLIPX    0x01
#define SPRITE_FLIPY    0x02
#define NUM_SPRITES     256

	GenericTilesGfx *pgfx	= &GenericGfxData[0];
	INT32 gfx_max     		= pgfx->gfx_len;

	UINT16 *source = (UINT16*)DrvSprRAM;

	sprite_t *sprite = sprite_list;
	sprite_t *const finish = sprite + NUM_SPRITES;

	INT32 bank, code, gfx, zoom;

	static const INT32 banks[2][0x40] = {
		{ 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7,
		8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15,
		0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7,
		8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15 },

		{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
		32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 }
	};

	for (spr_count = 0; sprite < finish; source += 0x10/2, sprite++)
	{
		if (BURN_ENDIAN_SWAP_INT16(source[0x00/2]) == 0xffff) break;

		sprite->y = BURN_ENDIAN_SWAP_INT16(source[0x00/2]) & 0xff;
		sprite->total_height = (BURN_ENDIAN_SWAP_INT16(source[0x00/2]) >> 8) - sprite->y;
		if (sprite->total_height < 1) continue;

		sprite->x = BURN_ENDIAN_SWAP_INT16(source[0x02/2]) & 0x1ff;
		bank = BURN_ENDIAN_SWAP_INT16(source[0x02/2]) >> 10;
		if (bank == 0x3f) continue;

		sprite->tile_width = BURN_ENDIAN_SWAP_INT16(source[0x04/2]) & 0xff;
		if (sprite->tile_width < 1) continue;

		sprite->shadow_mode = BURN_ENDIAN_SWAP_INT16(source[0x04/2]) & 0x4000;

		code = BURN_ENDIAN_SWAP_INT16(source[0x06/2]);
		zoom = BURN_ENDIAN_SWAP_INT16(source[0x08/2]);

		sprite->pal_base = (BURN_ENDIAN_SWAP_INT16(source[0x0e/2]) & 0xff) << 4;

		gfx = ((banks[game_select][bank & 0x3f]) << 15) + (code & 0x7fff);

		sprite->flags = 0;
		if (code & 0x8000) { sprite->flags |= SPRITE_FLIPX; gfx += 1-sprite->tile_width; }
		if (BURN_ENDIAN_SWAP_INT16(source[0x02/2]) & 0x0200) sprite->flags |= SPRITE_FLIPY;

		gfx <<= 3;
		sprite->tile_width <<= 3;
		sprite->tile_height = (sprite->total_height * 0x80) / (0x80 - (zoom >> 8)); // needs work

		if ((gfx + sprite->tile_width * sprite->tile_height - 1) >= gfx_max) continue;

		sprite->pen_data = pgfx->gfxbase + gfx;
		sprite->line_offset = sprite->tile_width;
		sprite->total_width = sprite->tile_width - (sprite->tile_width * (zoom & 0xff)) / 0x80;
		sprite->total_height += 1;
		sprite->x += spr_offsx;
		sprite->y += spr_offsy;

		if (game_select == 0) // wecleman
		{
			spr_idx_list[spr_count] = spr_count;
			spr_pri_list[spr_count] = BURN_ENDIAN_SWAP_INT16(source[0x0e/2]) >> 8;
		}

		spr_ptr_list[spr_count] = sprite;
		spr_count++;
	}
}

static void sortsprite(INT32 *idx_array, INT32 *key_array, INT32 size)
{
	INT32 i, j, tgt_val, low_val, low_pos, src_idx, tgt_idx, hi_idx;

	idx_array += size;

	for (j=-size; j<-1; j++)
	{
		src_idx = idx_array[j];
		low_pos = j;
		low_val = key_array[src_idx];
		hi_idx = src_idx;
		for (i=j+1; i; i++)
		{
			tgt_idx = idx_array[i];
			tgt_val = key_array[tgt_idx];
			if (low_val > tgt_val)
				{ low_val = tgt_val; low_pos = i; }
			else if ((low_val == tgt_val) && (hi_idx <= tgt_idx))
				{ hi_idx = tgt_idx; low_pos = i; }
		}
		low_val = idx_array[low_pos];
		idx_array[low_pos] = src_idx;
		idx_array[j] = low_val;
	}
}

static void do_blit_zoom32(const sprite_t &sprite)
{
#define PRECISION_X 20
#define PRECISION_Y 20
#define FPY_HALF (1<<(PRECISION_Y-1))

	INT32 min_x = 0;
	INT32 min_y = 0;
	INT32 max_x = nScreenWidth - 1;
	INT32 max_y = nScreenHeight - 1;

	UINT32 pal_base;
	INT32 src_f0y, src_fdy, src_f0x, src_fdx, src_fpx;
	INT32 x1, x2, y1, y2, dx, dy, sx, sy;
	INT32 xcount0=0, ycount0=0;

	if (sprite.flags & SPRITE_FLIPX)
	{
		x2 = sprite.x;
		x1 = x2 + sprite.total_width;
		dx = -1;
		if (x2 < min_x) x2 = min_x;
		if (x1 > max_x )
		{
			xcount0 = x1 - max_x;
			x1 = max_x;
		}
		if (x2 >= x1) return;
		x1--; x2--;
	}
	else
	{
		x1 = sprite.x;
		x2 = x1 + sprite.total_width;
		dx = 1;
		if (x1 < min_x )
		{
			xcount0 = min_x - x1;
			x1 = min_x;
		}
		if (x2 > max_x ) x2 = max_x;
		if (x1 >= x2) return;
	}

	if (sprite.flags & SPRITE_FLIPY)
	{
		y2 = sprite.y;
		y1 = y2 + sprite.total_height;
		dy = -1;
		if (y2 < min_y ) y2 = min_y;
		if (y1 > max_y )
		{
			ycount0 = max_y;
			y1 = max_y;
		}
		if (y2 >= y1) return;
		y1--; y2--;
	}
	else
	{
		y1 = sprite.y;
		y2 = y1 + sprite.total_height;
		dy = 1;
		if (y1 < min_y )
		{
			ycount0 = min_y - y1;
			y1 = min_y;
		}
		if (y2 > max_y) y2 = max_y;
		if (y1 >= y2) return;
	}

	// calculate entry point decimals
	src_fdy = (sprite.tile_height<<PRECISION_Y) / sprite.total_height;
	src_f0y = src_fdy * ycount0 + FPY_HALF;

	src_fdx = (sprite.tile_width<<PRECISION_X) / sprite.total_width;
	src_f0x = src_fdx * xcount0;

	// pre-loop assignments and adjustments
	pal_base = sprite.pal_base;

	if (x1 > min_x)
	{
		x1 -= dx;
		x2 -= dx;
	}

	for (sy = y1; sy != y2; sy += dy)
	{
		UINT8 *row_base = sprite.pen_data + (src_f0y>>PRECISION_Y) * sprite.line_offset;
		src_fpx = src_f0x;
		UINT16 *dst_ptr = pTransDraw + sy * nScreenWidth;

		if (!sprite.shadow_mode)
		{
			for (sx = x1; sx != x2; sx += dx)
			{
				INT32 pix = row_base[src_fpx >> PRECISION_X];
				if (pix & 0x80) break;
				if (pix)
					dst_ptr[sx] = pal_base + pix;
				src_fpx += src_fdx;
			}
		}
		else
		{
			for (sx = x1; sx != x2; sx += dx)
			{
				INT32 pix = row_base[src_fpx >> PRECISION_X];
				if (pix & 0x80) break;
				if (pix)
				{
					if (pix != 0xa)
						dst_ptr[sx] = pal_base + pix;
					else
						dst_ptr[sx] |=0x800;
				}
				src_fpx += src_fdx;
			}
		}

		src_f0y += src_fdy;
	}
}

static void sprite_draw_wecleman()
{
	sortsprite(spr_idx_list, spr_pri_list, spr_count);

	for (INT32 i = 0; i < spr_count; i++) {
		do_blit_zoom32(*spr_ptr_list[spr_idx_list[i]]);
	}
}

static void sprite_draw_hotchase()
{
	for (INT32 i = 0; i < spr_count; i++) {
		do_blit_zoom32(*spr_ptr_list[i]);
	}
}

static void HotchasePaletteUpdate()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x1000/2; i++)
	{
		UINT8 r = ((BURN_ENDIAN_SWAP_INT16(pal[i]) << 1) & 0x1e) | ((BURN_ENDIAN_SWAP_INT16(pal[i]) >> 12) & 0x01);
		UINT8 g = ((BURN_ENDIAN_SWAP_INT16(pal[i]) >> 3) & 0x1e) | ((BURN_ENDIAN_SWAP_INT16(pal[i]) >> 13) & 0x01);
		UINT8 b = ((BURN_ENDIAN_SWAP_INT16(pal[i]) >> 7) & 0x1e) | ((BURN_ENDIAN_SWAP_INT16(pal[i]) >> 14) & 0x01);

		DrvPalette[i + 0x000] = BurnHighCol(pal5bit(r), pal5bit(g), pal5bit(b), 0);
		DrvPalette[i + 0x800] = BurnHighCol(pal5bit(r >> 1), pal5bit(g >> 1), pal5bit(b >> 1), 0);
	}

	DrvPalette[0x1000] = 0;
	DrvPalette[0x1001] = BurnHighCol(0xff,0xff,0xff,0);
}

static void hotchase_draw_road()
{
	UINT16 *roadram = (UINT16*)DrvRoadRAM;

	for (INT32 sy = 0; sy < nScreenHeight; sy++)
	{
		INT32 code    = BURN_ENDIAN_SWAP_INT16(roadram[sy*2+1]) + (BURN_ENDIAN_SWAP_INT16(roadram[sy*2+0]) << 16);
		INT32 color   = ((code & 0x00f00000) >> 20) + 0x70;
		INT32 scrollx = ((code & 0x0007fc00) >> 10) * 2;

		code = (code & 0x1ff) * (512 / 32);

		for (INT32 sx = 0; sx < 1024; sx += 64)
		{
			DrawGfxMaskTile(0, 3, code++, ((sx-scrollx)&0x3ff)-(384-32), sy, 0, 0, color, 0);
		}
	}
}

static INT32 WeclemanDraw()
{
	if (DrvRecalc) {
		WeclemanPaletteUpdate();
		DrvRecalc = 1;
	}

	UINT16 *regs = (UINT16*)DrvTxtRAM;

	INT32 video_on = irq_control & 0x40;
	UINT16 fgvalue = BURN_ENDIAN_SWAP_INT16(regs[0xefc/2]);
	UINT16 bgvalue = BURN_ENDIAN_SWAP_INT16(regs[0xefe/2]);

	pages[0][0] = (bgvalue >>  4) & 3;
	pages[0][1] = (bgvalue >>  0) & 3;
	pages[0][2] = (bgvalue >> 12) & 3;
	pages[0][3] = (bgvalue >>  8) & 3;
	pages[1][0] = (fgvalue >>  4) & 3;
	pages[1][1] = (fgvalue >>  0) & 3;
	pages[1][2] = (fgvalue >> 12) & 3;
	pages[1][3] = (fgvalue >>  8) & 3;

	BurnLEDSetStatus(0, (selected_ip & 0x04)); // Start lamp

	INT32 fg_y = BURN_ENDIAN_SWAP_INT16(regs[0x0f24/2]) & 0x1ff;
	INT32 bg_y = BURN_ENDIAN_SWAP_INT16(regs[0x0f26/2]) & 0x1ff;

	GenericTilemapSetScrollY(0, bg_y - 0);
	GenericTilemapSetScrollY(1, fg_y - 0);

	GenericTilemapSetScrollX(2, 512-320-16 - 8);
	GenericTilemapSetScrollY(2, 0);

	for (INT32 i = 0; i < (28 << 2); i += 4)
	{
		INT32 fg_x = BURN_ENDIAN_SWAP_INT16(regs[(i + 0xf80) / 2]) + (0xb0 - 8);
		INT32 bg_x = BURN_ENDIAN_SWAP_INT16(regs[(i + 0xf82) / 2]) + (0xb0 - 8);

		INT32 k = i << 1;

		for (INT32 j = 0; j < 8; j++)
		{
			GenericTilemapSetScrollRow(1, (fg_y + k + j) & 0x1ff, fg_x);
			GenericTilemapSetScrollRow(0, (bg_y + k + j) & 0x1ff, bg_x);
		}
	}

	get_sprite_info(-0xbc, 1);

	BurnTransferClear(0x1000);

	if (video_on)
	{
		if (nSpriteEnable & 1) wecleman_draw_road(0x02);

		if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);

		if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);

		if (nSpriteEnable & 2) wecleman_draw_road(0x04);

		if (nSpriteEnable & 4) sprite_draw_wecleman();

		if (nBurnLayer & 4) GenericTilemapDraw(2, 0, 0);
	}

	BurnTransferCopy(DrvPalette);

	BurnLEDRender();
	BurnShiftRender();

	return 0;
}

static INT32 HotchaseDraw()
{
	if (DrvRecalc) {
		HotchasePaletteUpdate();
		DrvRecalc = 1; // force update
	}

	INT32 video_on = irq_control & 0x40;

	BurnLEDSetStatus(0, (selected_ip & 0x04)); // Start lamp

	get_sprite_info(-0xc0, 0);

	BurnTransferClear(0x1000);

	if (video_on)
	{
		K051316RedrawTiles(0);
		K051316RedrawTiles(1);

		if (nBurnLayer & 1) K051316_zoom_draw(0, 0x100); // 0x100 = index16

		if (nBurnLayer & 2) hotchase_draw_road();

		if (nSpriteEnable & 1) sprite_draw_hotchase();

		if (nBurnLayer & 4) K051316_zoom_draw(1, 0x100); // 0x100 = index16
	}

	BurnTransferCopy(DrvPalette);

	BurnLEDRender();
	BurnShiftRender();

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame(); // cpu sync

	{
		DrvInputs[0] = (game_select) ? 0xff : 0;
		DrvInputs[1] = (game_select) ? 0xe7 : 0xf7; // 0x10 - soundcpu to main on hotchase

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		{ // gear shifter
			BurnShiftInputCheckToggle(DrvJoy1[5]);

			DrvInputs[0] &= ~0x20;
			DrvInputs[0] |= (bBurnShiftStatus^game_select) << 5;
		}
	}

	INT32 nSegment;
	INT32 MULT = 8;
	INT32 nInterleave = 262*MULT;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[3] = { 10000000 / 60, 10000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	if (game_select == 1) nCyclesTotal[2] /= 2; // hotchase

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;
		SekOpen(0);
		if (game_select == 0 && (i & ((0x10 * MULT)-1)) == 0) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		if (i == 223*MULT) {
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}
		CPU_RUN(0, Sek);
		SekClose();

		SekOpen(1);
		CPU_RUN(1, Sek);
		SekClose();

		if (game_select == 0)
		{
			ZetOpen(0);
			CPU_RUN(2, Zet);
			ZetClose();

			if (pBurnSoundOut && (i&0xf) == 0xf) {
				nSegment = nBurnSoundLen / (nInterleave / (MULT*2));
				BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
				nSoundBufferPos += nSegment;
			}
		}
		else
		{
			M6809Open(0);
			CPU_RUN(2, M6809);
			if ((i & ((0x20 * MULT) - 1)) == 0) { // @ 256*8 interleave!
				M6809SetIRQLine(1, CPU_IRQSTATUS_HOLD);
			}
			M6809Close();
		}
	}

	if (pBurnSoundOut) {
		if (game_select == 0)
		{
			nSegment = nBurnSoundLen - nSoundBufferPos;
			if (nSegment > 0) {
				BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			}
			K007232Update(0, pBurnSoundOut, nBurnSoundLen);
		}
		else
		{
			BurnSoundClear();
			K007232Update(0, pBurnSoundOut, nBurnSoundLen);
			K007232Update(1, pBurnSoundOut, nBurnSoundLen);
			K007232Update(2, pBurnSoundOut, nBurnSoundLen);
		}
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029732;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		if (game_select) {
			M6809Scan(nAction);
		} else {
			ZetScan(nAction);
			BurnYM2151Scan(nAction, pnMin);
		}
		K007232Scan(nAction, pnMin);

		KonamiICScan(nAction);

		BurnLEDScan(nAction, pnMin);
		BurnShiftScan(nAction);

		SCAN_VAR(protection_ram);
		SCAN_VAR(blitter_regs);
		SCAN_VAR(multiply_reg);

		SCAN_VAR(soundbank);
		SCAN_VAR(selected_ip);
		SCAN_VAR(irq_control);
		SCAN_VAR(protection_state);
		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_status);
		SCAN_VAR(irq_timer);
	}

	if (nAction & ACB_WRITE) {
		if (game_select == 0) {
			bankswitch(soundbank);
		}
	}

	return 0;
}

// WEC Le Mans 24 (v2.01)

static struct BurnRomInfo weclemanRomDesc[] = {
	{ "602g08.17h",		0x10000, 0x2edf468c, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "602g11.23h",		0x10000, 0xca5ecb25, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "602a09.18h",		0x10000, 0x8a9d756f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "602a10.22h",		0x10000, 0x569f5001, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "602a06.18a",		0x08000, 0xe12c0d11, 2 | BRF_PRG | BRF_ESS }, //  4 68K #1 Code
	{ "602a07.20a",		0x08000, 0x47968e51, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "602a01.6d",		0x08000, 0xdeafe5f1, 3 | BRF_PRG | BRF_ESS }, //  6 Z80 Code

	{ "602a25.12e",		0x20000, 0x0eacf1f9, 1 | BRF_GRA },           //  7 Sprites
	{ "602a26.14e",		0x20000, 0x2182edaf, 1 | BRF_GRA },           //  8
	{ "602a27.15e",		0x20000, 0xb22f08e9, 1 | BRF_GRA },           //  9
	{ "602a28.17e",		0x20000, 0x5f6741fa, 1 | BRF_GRA },           // 10
	{ "602a21.6e",		0x20000, 0x8cab34f1, 1 | BRF_GRA },           // 11
	{ "602a22.7e",		0x20000, 0xe40303cb, 1 | BRF_GRA },           // 12
	{ "602a23.9e",		0x20000, 0x75077681, 1 | BRF_GRA },           // 13
	{ "602a24.10e",		0x20000, 0x583dadad, 1 | BRF_GRA },           // 14
	{ "602a17.12c",		0x20000, 0x31612199, 1 | BRF_GRA },           // 15
	{ "602a18.14c",		0x20000, 0x3f061a67, 1 | BRF_GRA },           // 16
	{ "602a19.15c",		0x20000, 0x5915dbc5, 1 | BRF_GRA },           // 17
	{ "602a20.17c",		0x20000, 0xf87e4ef5, 1 | BRF_GRA },           // 18
	{ "602a13.6c",		0x20000, 0x5d3589b8, 1 | BRF_GRA },           // 19
	{ "602a14.7c",		0x20000, 0xe3a75f6c, 1 | BRF_GRA },           // 20
	{ "602a15.9c",		0x20000, 0x0d493c9f, 1 | BRF_GRA },           // 21
	{ "602a16.10c",		0x20000, 0xb08770b3, 1 | BRF_GRA },           // 22

	{ "602a31.26g",		0x08000, 0x01fa40dd, 2 | BRF_GRA },           // 23 Layer Graphics
	{ "602a30.24g",		0x08000, 0xbe5c4138, 2 | BRF_GRA },           // 24
	{ "602a29.23g",		0x08000, 0xf1a8d33e, 2 | BRF_GRA },           // 25

	{ "602a04.11e",		0x08000, 0xade9f359, 4 | BRF_GRA },           // 26 Road Graphics
	{ "602a05.13e",		0x04000, 0xf22b7f2b, 4 | BRF_GRA },           // 27

	{ "602a03.10a",		0x20000, 0x31392b01, 1 | BRF_SND },           // 28 k007232 Chip #0 Samples
	{ "602a02.8a",		0x20000, 0xe2be10ae, 1 | BRF_SND },           // 29

	{ "602a12.1a",		0x04000, 0x77b9383d, 0 | BRF_OPT },           // 30 Unknown...
};

STD_ROM_PICK(wecleman)
STD_ROM_FN(wecleman)

struct BurnDriver BurnDrvWecleman = {
	"wecleman", NULL, NULL, NULL, "1986",
	"WEC Le Mans 24 (v2.01)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, weclemanRomInfo, weclemanRomName, NULL, NULL, NULL, NULL, WeclemanInputInfo, WeclemanDIPInfo,
	WeclemanInit, DrvExit, DrvFrame, WeclemanDraw, DrvScan, &DrvRecalc, 0x800,
	320, 224, 4, 3
};


// WEC Le Mans 24 (v2.00)

static struct BurnRomInfo weclemanaRomDesc[] = {
	{ "602f08.17h",		0x10000, 0x493b79d3, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "602f11.23h",		0x10000, 0x6bb4f1fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "602a09.18h",		0x10000, 0x8a9d756f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "602a10.22h",		0x10000, 0x569f5001, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "602a06.18a",		0x08000, 0xe12c0d11, 2 | BRF_PRG | BRF_ESS }, //  4 68K #1 Code
	{ "602a07.20a",		0x08000, 0x47968e51, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "602a01.6d",		0x08000, 0xdeafe5f1, 3 | BRF_PRG | BRF_ESS }, //  6 Z80 Code

	{ "602a25.12e",		0x20000, 0x0eacf1f9, 1 | BRF_GRA },           //  7 Sprites
	{ "602a26.14e",		0x20000, 0x2182edaf, 1 | BRF_GRA },           //  8
	{ "602a27.15e",		0x20000, 0xb22f08e9, 1 | BRF_GRA },           //  9
	{ "602a28.17e",		0x20000, 0x5f6741fa, 1 | BRF_GRA },           // 10
	{ "602a21.6e",		0x20000, 0x8cab34f1, 1 | BRF_GRA },           // 11
	{ "602a22.7e",		0x20000, 0xe40303cb, 1 | BRF_GRA },           // 12
	{ "602a23.9e",		0x20000, 0x75077681, 1 | BRF_GRA },           // 13
	{ "602a24.10e",		0x20000, 0x583dadad, 1 | BRF_GRA },           // 14
	{ "602a17.12c",		0x20000, 0x31612199, 1 | BRF_GRA },           // 15
	{ "602a18.14c",		0x20000, 0x3f061a67, 1 | BRF_GRA },           // 16
	{ "602a19.15c",		0x20000, 0x5915dbc5, 1 | BRF_GRA },           // 17
	{ "602a20.17c",		0x20000, 0xf87e4ef5, 1 | BRF_GRA },           // 18
	{ "602a13.6c",		0x20000, 0x5d3589b8, 1 | BRF_GRA },           // 19
	{ "602a14.7c",		0x20000, 0xe3a75f6c, 1 | BRF_GRA },           // 20
	{ "602a15.9c",		0x20000, 0x0d493c9f, 1 | BRF_GRA },           // 21
	{ "602a16.10c",		0x20000, 0xb08770b3, 1 | BRF_GRA },           // 22

	{ "602a31.26g",		0x08000, 0x01fa40dd, 2 | BRF_GRA },           // 23 Layer Graphics
	{ "602a30.24g",		0x08000, 0xbe5c4138, 2 | BRF_GRA },           // 24
	{ "602a29.23g",		0x08000, 0xf1a8d33e, 2 | BRF_GRA },           // 25

	{ "602a04.11e",		0x08000, 0xade9f359, 4 | BRF_GRA },           // 26 Road Graphics
	{ "602a05.13e",		0x04000, 0xf22b7f2b, 4 | BRF_GRA },           // 27

	{ "602a03.10a",		0x20000, 0x31392b01, 1 | BRF_SND },           // 28 k007232 Chip #0 Samples
	{ "602a02.8a",		0x20000, 0xe2be10ae, 1 | BRF_SND },           // 29

	{ "602a12.1a",		0x04000, 0x77b9383d, 8 | BRF_OPT },           // 30 Unknown...
};

STD_ROM_PICK(weclemana)
STD_ROM_FN(weclemana)

struct BurnDriver BurnDrvWeclemana = {
	"weclemana", "wecleman", NULL, NULL, "1986",
	"WEC Le Mans 24 (v2.00)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, weclemanaRomInfo, weclemanaRomName, NULL, NULL, NULL, NULL, WeclemanInputInfo, WeclemanDIPInfo,
	WeclemanInit, DrvExit, DrvFrame, WeclemanDraw, DrvScan, &DrvRecalc, 0x800,
	320, 224, 4, 3
};


// WEC Le Mans 24 (v2.00, hack)

static struct BurnRomInfo weclemanbRomDesc[] = {
	{ "602f08.17h",		0x10000, 0x43241265, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "602f11.23h",		0x10000, 0x3ea7dae0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "602a09.18h",		0x10000, 0x8a9d756f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "602a10.22h",		0x10000, 0x569f5001, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "602a06.18a",		0x08000, 0xe12c0d11, 2 | BRF_PRG | BRF_ESS }, //  4 68K #1 Code
	{ "602a07.20a",		0x08000, 0x47968e51, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "602a01.6d",		0x08000, 0xdeafe5f1, 3 | BRF_PRG | BRF_ESS }, //  6 Z80 Code

	{ "602a25.12e",		0x20000, 0x0eacf1f9, 1 | BRF_GRA },           //  7 Sprites
	{ "602a26.14e",		0x20000, 0x2182edaf, 1 | BRF_GRA },           //  8
	{ "602a27.15e",		0x20000, 0xb22f08e9, 1 | BRF_GRA },           //  9
	{ "602a28.17e",		0x20000, 0x5f6741fa, 1 | BRF_GRA },           // 10
	{ "602a21.6e",		0x20000, 0x8cab34f1, 1 | BRF_GRA },           // 11
	{ "602a22.7e",		0x20000, 0xe40303cb, 1 | BRF_GRA },           // 12
	{ "602a23.9e",		0x20000, 0x75077681, 1 | BRF_GRA },           // 13
	{ "602a24.10e",		0x20000, 0x583dadad, 1 | BRF_GRA },           // 14
	{ "602a17.12c",		0x20000, 0x31612199, 1 | BRF_GRA },           // 15
	{ "602a18.14c",		0x20000, 0x3f061a67, 1 | BRF_GRA },           // 16
	{ "602a19.15c",		0x20000, 0x5915dbc5, 1 | BRF_GRA },           // 17
	{ "602a20.17c",		0x20000, 0xf87e4ef5, 1 | BRF_GRA },           // 18
	{ "602a13.6c",		0x20000, 0x5d3589b8, 1 | BRF_GRA },           // 19
	{ "602a14.7c",		0x20000, 0xe3a75f6c, 1 | BRF_GRA },           // 20
	{ "602a15.9c",		0x20000, 0x0d493c9f, 1 | BRF_GRA },           // 21
	{ "602a16.10c",		0x20000, 0xb08770b3, 1 | BRF_GRA },           // 22

	{ "602a31.26g",		0x08000, 0x01fa40dd, 1 | BRF_GRA },           // 23 Layer Graphics
	{ "602a30.24g",		0x08000, 0xbe5c4138, 1 | BRF_GRA },           // 24
	{ "602a29.23g",		0x08000, 0xf1a8d33e, 1 | BRF_GRA },           // 25

	{ "602a04.11e",		0x08000, 0xade9f359, 3 | BRF_GRA },           // 26 Road Graphics
	{ "602a05.13e",		0x04000, 0xf22b7f2b, 3 | BRF_GRA },           // 27

	{ "602a03.10a",		0x20000, 0x31392b01, 1 | BRF_SND },           // 28 k007232 Chip #0 Samples
	{ "602a02.8a",		0x20000, 0xe2be10ae, 1 | BRF_SND },           // 29

	{ "602a12.1a",		0x04000, 0x77b9383d, 0 | BRF_OPT },           // 30 Unknown...
};

STD_ROM_PICK(weclemanb)
STD_ROM_FN(weclemanb)

struct BurnDriver BurnDrvWeclemanb = {
	"weclemanb", "wecleman", NULL, NULL, "1988",
	"WEC Le Mans 24 (v2.00, hack)\0", NULL, "hack", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, weclemanbRomInfo, weclemanbRomName, NULL, NULL, NULL, NULL, WeclemanInputInfo, WeclemanDIPInfo,
	WeclemanInit, DrvExit, DrvFrame, WeclemanDraw, DrvScan, &DrvRecalc, 0x800,
	320, 224, 4, 3
};


// WEC Le Mans 24 (v1.26)

static struct BurnRomInfo weclemancRomDesc[] = {
	{ "17h",			0x10000, 0x66901326, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "23h",			0x10000, 0xd9d492f4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "602a09.18h",		0x10000, 0x8a9d756f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "602a10.22h",		0x10000, 0x569f5001, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "602a06.18a",		0x08000, 0xe12c0d11, 2 | BRF_PRG | BRF_ESS }, //  4 68K #1 Code
	{ "602a07.20a",		0x08000, 0x47968e51, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "602a01.6d",		0x08000, 0xdeafe5f1, 3 | BRF_PRG | BRF_ESS }, //  6 Z80 Code

	{ "602a25.12e",		0x20000, 0x0eacf1f9, 1 | BRF_GRA },           //  7 Sprites
	{ "602a26.14e",		0x20000, 0x2182edaf, 1 | BRF_GRA },           //  8
	{ "602a27.15e",		0x20000, 0xb22f08e9, 1 | BRF_GRA },           //  9
	{ "602a28.17e",		0x20000, 0x5f6741fa, 1 | BRF_GRA },           // 10
	{ "602a21.6e",		0x20000, 0x8cab34f1, 1 | BRF_GRA },           // 11
	{ "602a22.7e",		0x20000, 0xe40303cb, 1 | BRF_GRA },           // 12
	{ "602a23.9e",		0x20000, 0x75077681, 1 | BRF_GRA },           // 13
	{ "602a24.10e",		0x20000, 0x583dadad, 1 | BRF_GRA },           // 14
	{ "602a17.12c",		0x20000, 0x31612199, 1 | BRF_GRA },           // 15
	{ "602a18.14c",		0x20000, 0x3f061a67, 1 | BRF_GRA },           // 16
	{ "602a19.15c",		0x20000, 0x5915dbc5, 1 | BRF_GRA },           // 17
	{ "602a20.17c",		0x20000, 0xf87e4ef5, 1 | BRF_GRA },           // 18
	{ "602a13.6c",		0x20000, 0x5d3589b8, 1 | BRF_GRA },           // 19
	{ "602a14.7c",		0x20000, 0xe3a75f6c, 1 | BRF_GRA },           // 20
	{ "602a15.9c",		0x20000, 0x0d493c9f, 1 | BRF_GRA },           // 21
	{ "602a16.10c",		0x20000, 0xb08770b3, 1 | BRF_GRA },           // 22

	{ "602a31.26g",		0x08000, 0x01fa40dd, 2 | BRF_GRA },           // 23 Layer Graphics
	{ "602a30.24g",		0x08000, 0xbe5c4138, 2 | BRF_GRA },           // 24
	{ "602a29.23g",		0x08000, 0xf1a8d33e, 2 | BRF_GRA },           // 25

	{ "602a04.11e",		0x08000, 0xade9f359, 3 | BRF_GRA },           // 26 Road Graphics
	{ "602a05.13e",		0x04000, 0xf22b7f2b, 3 | BRF_GRA },           // 27

	{ "602a03.10a",		0x20000, 0x31392b01, 1 | BRF_SND },           // 28 k007232 Chip #0 Samples
	{ "602a02.8a",		0x20000, 0xe2be10ae, 1 | BRF_SND },           // 29

	{ "602a12.1a",		0x04000, 0x77b9383d, 0 | BRF_OPT },           // 30 Unknown...
};

STD_ROM_PICK(weclemanc)
STD_ROM_FN(weclemanc)

struct BurnDriver BurnDrvWeclemanc = {
	"weclemanc", "wecleman", NULL, NULL, "1986",
	"WEC Le Mans 24 (v1.26)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, weclemancRomInfo, weclemancRomName, NULL, NULL, NULL, NULL, WeclemanInputInfo, WeclemanDIPInfo,
	WeclemanInit, DrvExit, DrvFrame, WeclemanDraw, DrvScan, &DrvRecalc, 0x800,
	320, 224, 4, 3
};


// Hot Chase (set 1)

static struct BurnRomInfo hotchaseRomDesc[] = {
	{ "763k05",			0x10000, 0xf34fef0b, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "763k04",			0x10000, 0x60f73178, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "763k03",			0x10000, 0x28e3a444, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "763k02",			0x10000, 0x9510f961, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "763k07",			0x10000, 0xae12fa90, 2 | BRF_PRG | BRF_ESS }, //  4 68K #1 Code
	{ "763k06",			0x10000, 0xb77e0c07, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "763f01",			0x08000, 0x4fddd061, 3 | BRF_PRG | BRF_ESS }, //  6 M6809 Code

	{ "763e17",			0x80000, 0x8db4e0aa, 1 | BRF_GRA },           //  7 Sprites
	{ "763e20",			0x80000, 0xa22c6fce, 1 | BRF_GRA },           //  8
	{ "763e18",			0x80000, 0x50920d01, 1 | BRF_GRA },           //  9
	{ "763e21",			0x80000, 0x77e0e93e, 1 | BRF_GRA },           // 10
	{ "763e19",			0x80000, 0xa2622e56, 1 | BRF_GRA },           // 11
	{ "763e22",			0x80000, 0x967c49d1, 1 | BRF_GRA },           // 12

	{ "763e14",			0x20000, 0x60392aa1, 2 | BRF_GRA },           // 13 k051316 Roz Chip #0 Graphics

	{ "763a13",			0x10000, 0x8bed8e0d, 3 | BRF_GRA },           // 14 k051316 Roz Chip #1 Graphics

	{ "763e15",			0x20000, 0x7110aa43, 4 | BRF_GRA },           // 15 Road Graphics

	{ "763e11",			0x40000, 0x9d99a5a7, 1 | BRF_SND },           // 16 k007232 Chip #0 Samples

	{ "763e10",			0x40000, 0xca409210, 2 | BRF_SND },           // 17 k007232 Chip #1 Samples

	{ "763e08",			0x80000, 0x054a9a63, 3 | BRF_SND },           // 18 k007232 Chip #2 Samples
	{ "763e09",			0x80000, 0xc39857db, 3 | BRF_SND },           // 19

	{ "763a12",			0x08000, 0x05f1e553, 0 | BRF_OPT },           // 20 Unknown...
};

STD_ROM_PICK(hotchase)
STD_ROM_FN(hotchase)

static INT32 HotchaseInit()
{
	return HotchaseCommonInit(0);
}

struct BurnDriver BurnDrvHotchase = {
	"hotchase", NULL, NULL, NULL, "1988",
	"Hot Chase (set 1)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, hotchaseRomInfo, hotchaseRomName, NULL, NULL, NULL, NULL, WeclemanInputInfo, HotchaseDIPInfo,
	HotchaseInit, DrvExit, DrvFrame, HotchaseDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Hot Chase (set 2)

static struct BurnRomInfo hotchaseaRomDesc[] = {
	{ "763r05.r16",		0x10000, 0xc880d5e4, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "763r04.p16",		0x10000, 0xb732ee2c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "763r03.r14",		0x10000, 0x13dd71de, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "763r02.p14",		0x10000, 0x6cd1a18e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "763d07.h18",		0x10000, 0xae12fa90, 2 | BRF_PRG | BRF_ESS }, //  4 68K #1 Code
	{ "763d06.e18",		0x10000, 0xb77e0c07, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "763p01.r10",		0x08000, 0x15dbca7b, 3 | BRF_PRG | BRF_ESS }, //  6 M6809 Code

	{ "763e17a",		0x10000, 0x8542d7d7, 1 | BRF_GRA },           //  7 Sprites
	{ "763e17e",		0x10000, 0x4b4d919c, 1 | BRF_GRA },           //  8
	{ "763e17b",		0x10000, 0xba9d7e72, 1 | BRF_GRA },           //  9
	{ "763e17f",		0x10000, 0x582400bb, 1 | BRF_GRA },           // 10
	{ "763e17c",		0x10000, 0x0ed292f8, 1 | BRF_GRA },           // 11
	{ "763e17g",		0x10000, 0x35b27ed7, 1 | BRF_GRA },           // 12
	{ "763e17d",		0x10000, 0x0166d00d, 1 | BRF_GRA },           // 13
	{ "763e17h",		0x10000, 0xe5b8e8e6, 1 | BRF_GRA },           // 14
	{ "763e20a",		0x10000, 0x256fe63c, 1 | BRF_GRA },           // 15
	{ "763e20e",		0x10000, 0xee8ca7e1, 1 | BRF_GRA },           // 16
	{ "763e20b",		0x10000, 0xb6714c24, 1 | BRF_GRA },           // 17
	{ "763e20f",		0x10000, 0x9dbc4b21, 1 | BRF_GRA },           // 18
	{ "763e20c",		0x10000, 0x5173ad9b, 1 | BRF_GRA },           // 19
	{ "763e20g",		0x10000, 0xb8c77f99, 1 | BRF_GRA },           // 20
	{ "763e20d",		0x10000, 0x4ebdba32, 1 | BRF_GRA },           // 21
	{ "763e20h",		0x10000, 0x0a428654, 1 | BRF_GRA },           // 22
	{ "763e18a",		0x10000, 0x09748099, 1 | BRF_GRA },           // 23
	{ "763e18e",		0x10000, 0x049d4fcf, 1 | BRF_GRA },           // 24
	{ "763e18b",		0x10000, 0xed0c3369, 1 | BRF_GRA },           // 25
	{ "763e18f",		0x10000, 0xb596a9ce, 1 | BRF_GRA },           // 26
	{ "763e18c",		0x10000, 0x5a278291, 1 | BRF_GRA },           // 27
	{ "763e18g",		0x10000, 0xaa7263cd, 1 | BRF_GRA },           // 28
	{ "763e18d",		0x10000, 0xb0b79a71, 1 | BRF_GRA },           // 29
	{ "763e18h",		0x10000, 0xa18b9127, 1 | BRF_GRA },           // 30
	{ "763e21a",		0x10000, 0x60788c29, 1 | BRF_GRA },           // 31
	{ "763e21e",		0x10000, 0x844799ff, 1 | BRF_GRA },           // 32
	{ "763e21b",		0x10000, 0x1eefed61, 1 | BRF_GRA },           // 33
	{ "763e21f",		0x10000, 0x3aacfb10, 1 | BRF_GRA },           // 34
	{ "763e21c",		0x10000, 0x97e48b37, 1 | BRF_GRA },           // 35
	{ "763e21g",		0x10000, 0x74fefb12, 1 | BRF_GRA },           // 36
	{ "763e21d",		0x10000, 0xdd41569e, 1 | BRF_GRA },           // 37
	{ "763e21h",		0x10000, 0x7ea52bf6, 1 | BRF_GRA },           // 38
	{ "763e19a",		0x10000, 0x8c912c46, 1 | BRF_GRA },           // 39
	{ "763e19e",		0x10000, 0x0eb34787, 1 | BRF_GRA },           // 40
	{ "763e19b",		0x10000, 0x79960729, 1 | BRF_GRA },           // 41
	{ "763e19f",		0x10000, 0x1764ec5f, 1 | BRF_GRA },           // 42
	{ "763e19c",		0x10000, 0xf13377ac, 1 | BRF_GRA },           // 43
	{ "763e19g",		0x10000, 0xf2102e89, 1 | BRF_GRA },           // 44
	{ "763e19d",		0x10000, 0x0b2a19f4, 1 | BRF_GRA },           // 45
	{ "763e19h",		0x10000, 0xcd6d08a5, 1 | BRF_GRA },           // 46
	{ "763e22a",		0x10000, 0x16eec250, 1 | BRF_GRA },           // 47
	{ "763e22e",		0x10000, 0xc184b1c0, 1 | BRF_GRA },           // 48
	{ "763e22b",		0x10000, 0x1afe4b0c, 1 | BRF_GRA },           // 49
	{ "763e22f",		0x10000, 0x61f27c98, 1 | BRF_GRA },           // 50
	{ "763e22c",		0x10000, 0xc19b4b63, 1 | BRF_GRA },           // 51
	{ "763e22g",		0x10000, 0x5bcbaf29, 1 | BRF_GRA },           // 52
	{ "763e22d",		0x10000, 0xfd5b669d, 1 | BRF_GRA },           // 53
	{ "763e22h",		0x10000, 0x9a9f45d8, 1 | BRF_GRA },           // 54

	{ "763a14",			0x20000, 0x60392aa1, 2 | BRF_GRA },           // 55 k051316 Roz Chip #0 Graphics

	{ "763a13",			0x10000, 0x8bed8e0d, 3 | BRF_GRA },           // 56 k051316 Roz Chip #1 Graphics

	{ "763a15",			0x20000, 0x7110aa43, 4 | BRF_GRA },           // 57 Road Graphics

	{ "763e11a",		0x10000, 0xa60a93c8, 1 | BRF_SND },           // 58 k007232 Chip #0 Samples
	{ "763e11b",		0x10000, 0x7750feb5, 1 | BRF_SND },           // 59
	{ "763e11c",		0x10000, 0x78b89bf8, 1 | BRF_SND },           // 60
	{ "763e11d",		0x10000, 0x5f38d054, 1 | BRF_SND },           // 61

	{ "763e10a",		0x10000, 0x2b1cbefc, 2 | BRF_SND },           // 62 k007232 Chip #1 Samples
	{ "763e10b",		0x10000, 0x8209c950, 2 | BRF_SND },           // 63
	{ "763e10c",		0x10000, 0xb91d6c07, 2 | BRF_SND },           // 64
	{ "763e10d",		0x10000, 0x5b465d20, 2 | BRF_SND },           // 65

	{ "763e08a",		0x20000, 0x02e4e7ef, 3 | BRF_SND },           // 66 k007232 Chip #2 Samples
	{ "763e08b",		0x20000, 0x94edde2f, 3 | BRF_SND },           // 67
	{ "763e08c",		0x20000, 0xb1ab1529, 3 | BRF_SND },           // 68
	{ "763e08d",		0x20000, 0xee8d14db, 3 | BRF_SND },           // 69
	{ "763e09a",		0x20000, 0x1e6628ec, 3 | BRF_SND },           // 70
	{ "763e09b",		0x20000, 0xf0c2feb8, 3 | BRF_SND },           // 71
	{ "763e09c",		0x20000, 0xa0ade3e4, 3 | BRF_SND },           // 72
	{ "763e09d",		0x20000, 0xc74e484d, 3 | BRF_SND },           // 73

	{ "763a12",			0x08000, 0x05f1e553, 0 | BRF_GRA },           // 74 Unknown...
	{ "763a23.b17",		0x00200, 0x81c30352, 0 | BRF_GRA },           // 75
};

STD_ROM_PICK(hotchasea)
STD_ROM_FN(hotchasea)

static INT32 HotchaseaInit()
{
	return HotchaseCommonInit(1);
}

struct BurnDriver BurnDrvHotchasea = {
	"hotchasea", "hotchase", NULL, NULL, "1988",
	"Hot Chase (set 2)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, hotchaseaRomInfo, hotchaseaRomName, NULL, NULL, NULL, NULL, WeclemanInputInfo, HotchaseDIPInfo,
	HotchaseaInit, DrvExit, DrvFrame, HotchaseDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};
