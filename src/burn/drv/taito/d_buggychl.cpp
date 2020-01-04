// FB Alpha Taito Buggy Challenge driver module
// Based on MAME driver by Ernesto Corvi and Nicola Salmoria

#include "tiles_generic.h"
#include "taito_m68705.h"
#include "z80_intf.h"
#include "watchdog.h"
#include "msm5232.h"
#include "ay8910.h"
#include "burn_gun.h"
#include "burn_shift.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvMCUROM;
static UINT8 *DrvCharROM;
static UINT8 *DrvSprROM;
static UINT8 *DrvZoomROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvMCURAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVScrRAM;
static UINT8 *DrvHScrRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvSprLutRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 bankdata;
static INT32 spritelut_bank;
static INT32 soundlatch[2]; // relies on this being at least 16 bits!
static INT32 flipscreen;
static INT32 bgclip_on;
static INT32 sprite_color_base;
static INT32 sky_on;
static INT32 bg_scrollx;
static INT32 sndbyte_4830;

static INT32 ta7630_vol_ctrl[16];
static UINT8 ta7630_snd_ctrl0;
static UINT8 ta7630_snd_ctrl1;
static UINT8 ta7630_snd_ctrl2;

static INT32 sound_enabled;
static INT32 nmi_enabled;
static INT32 nmi_pending;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT16 AnalogPort0;
static INT16 AnalogPort1;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo BuggychlInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Gear Shift",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},

	A("Left/Right",		BIT_ANALOG_REL, &AnalogPort0,	"p1 x-axis"  ),
	A("Throttle",		BIT_ANALOG_REL, &AnalogPort1,	"p1 z-axis"  ),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Test Button",		BIT_DIGITAL,	DrvJoy1 + 4,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 3,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};
#undef A
STDINPUTINFO(Buggychl)

static struct BurnDIPInfo BuggychlDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x7f, NULL					},
	{0x0b, 0xff, 0xff, 0x00, NULL					},
	{0x0c, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Game Over Bonus"		},
	{0x0a, 0x01, 0x03, 0x03, "2000/1000/50"			},
	{0x0a, 0x01, 0x03, 0x02, "1000/500/30"			},
	{0x0a, 0x01, 0x03, 0x01, "500/200/10"			},
	{0x0a, 0x01, 0x03, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x0a, 0x01, 0x04, 0x04, "Off"					},
	{0x0a, 0x01, 0x04, 0x00, "On"					},
	
	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0a, 0x01, 0x18, 0x18, "Easy"					},
	{0x0a, 0x01, 0x18, 0x10, "Normal"				},
	{0x0a, 0x01, 0x18, 0x08, "Hard"					},
	{0x0a, 0x01, 0x18, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0a, 0x01, 0x20, 0x20, "Off"					},
	{0x0a, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0a, 0x01, 0x40, 0x40, "Off"					},
	{0x0a, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    16, "Coin A"				},
	{0x0b, 0x01, 0x0f, 0x0f, "9 Coins 1 Credits"	},
	{0x0b, 0x01, 0x0f, 0x0e, "8 Coins 1 Credits"	},
	{0x0b, 0x01, 0x0f, 0x0d, "7 Coins 1 Credits"	},
	{0x0b, 0x01, 0x0f, 0x0c, "6 Coins 1 Credits"	},
	{0x0b, 0x01, 0x0f, 0x0b, "5 Coins 1 Credits"	},
	{0x0b, 0x01, 0x0f, 0x0a, "4 Coins 1 Credits"	},
	{0x0b, 0x01, 0x0f, 0x09, "3 Coins 1 Credits"	},
	{0x0b, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x0b, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x0b, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x0b, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x0b, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x0b, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x0b, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"	},
	{0x0b, 0x01, 0x0f, 0x07, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    16, "Coin B"				},
	{0x0b, 0x01, 0xf0, 0xf0, "9 Coins 1 Credits"	},
	{0x0b, 0x01, 0xf0, 0xe0, "8 Coins 1 Credits"	},
	{0x0b, 0x01, 0xf0, 0xd0, "7 Coins 1 Credits"	},
	{0x0b, 0x01, 0xf0, 0xc0, "6 Coins 1 Credits"	},
	{0x0b, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"	},
	{0x0b, 0x01, 0xf0, 0xa0, "4 Coins 1 Credits"	},
	{0x0b, 0x01, 0xf0, 0x90, "3 Coins 1 Credits"	},
	{0x0b, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x0b, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"	},
	{0x0b, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"	},
	{0x0b, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},
	{0x0b, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"	},
	{0x0b, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x0b, 0x01, 0xf0, 0x60, "1 Coin  7 Credits"	},
	{0x0b, 0x01, 0xf0, 0x70, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    2, "Start button needed"	},
	{0x0c, 0x01, 0x01, 0x00, "No"					},
	{0x0c, 0x01, 0x01, 0x01, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Fuel loss"			},
	{0x0c, 0x01, 0x04, 0x04, "Normal"				},
	{0x0c, 0x01, 0x04, 0x00, "Crash only"			},

	{0   , 0xfe, 0   ,    2, "Coinage Display"		},
	{0x0c, 0x01, 0x10, 0x00, "No"					},
	{0x0c, 0x01, 0x10, 0x10, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Year Display"			},
	{0x0c, 0x01, 0x20, 0x00, "No"					},
	{0x0c, 0x01, 0x20, 0x20, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x0c, 0x01, 0x40, 0x40, "Off"					},
	{0x0c, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Coin Slots"			},
	{0x0c, 0x01, 0x80, 0x00, "1"					},
	{0x0c, 0x01, 0x80, 0x80, "2"					},
};

STDDIPINFO(Buggychl)

static void bankswitch(INT32 data)
{
	bankdata = data & 7;

	ZetMapMemory(DrvZ80ROM0 + 0x10000 + (bankdata * 0x2000),	0xa000, 0xbfff, MAP_ROM);
}

static void spritelut_bankswitch(INT32 data)
{
	spritelut_bank = (data & 0x10) >> 4;

	ZetMapMemory(DrvSprLutRAM + (spritelut_bank * 0x1000),		0x9000, 0x9fff, MAP_RAM);
}

static inline void character_expand(INT32 offset)
{
	offset &= 0x7ff;

	UINT8 d0 = DrvCharRAM[offset + 0x0000];
	UINT8 d1 = DrvCharRAM[offset + 0x0800];
	UINT8 d2 = DrvCharRAM[offset + 0x1000];
	UINT8 d3 = DrvCharRAM[offset + 0x1800];
	UINT8 *d = DrvCharROM + (offset * 8);

	for (INT32 i = 0; i < 8; i++)
	{
		*d = (d0 & 1) | ((d1 & 1) << 1) | ((d2 & 1) << 2) | ((d3 & 1) << 3);

		d0 >>= 1;
		d1 >>= 1;
		d2 >>= 1;
		d3 >>= 1;
		d++;
	}
}

static void sync_cpus()
{
	INT32 cyc = ZetTotalCycles() * 4 / 6;
	ZetClose();
	ZetOpen(1);
	cyc = cyc - ZetTotalCycles();
	if (cyc > 0) {
		ZetRun(cyc);
	}
	ZetClose();
	ZetOpen(0);
}

static void __fastcall buggychl_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff00) == 0xda00) return; // nop

	if ((address & 0xe000) == 0xa000) {
		if (DrvCharRAM[address & 0x1fff] != data) {
			DrvCharRAM[address & 0x1fff] = data;
			character_expand(address);
		}
		return;
	}

	if (address == 0xd000) return; // nop

	if ((address & 0xff00) == 0xd100) {
		flipscreen = data & 3; // x,y
		bgclip_on = data & 4;
		sky_on = data & 8;
		sprite_color_base = (data & 0x10) ? 16 : 48;
	//	coin_lockout = data & 0x40
	//	led = data & 0x80
		return;
	}

	if ((address & 0xff00) == 0xd200) {
		bankswitch(data);
		return;
	}

	if ((address & 0xff07) == 0xd300) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0xff07) == 0xd303) {
		spritelut_bankswitch(data);
		return;
	}

	if ((address & 0xff04) == 0xd304) return; // nop

	if ((address & 0xff03) == 0xd400) {
		standard_taito_mcu_write(data);
		return;
	}

	if ((address & 0xff1b) == 0xd610) {
		sync_cpus();
		if (nmi_enabled) {
			ZetNmi(1);
		} else nmi_pending = 1;

		soundlatch[0] = data | 0x100;
		return;
	}

	if ((address & 0xff18) == 0xd618) return; // nop
	if (address == 0xdc04) return; // nop

	if (address == 0xdc06) {
		bg_scrollx = data - 0x12;
		return;
	}
}

static UINT8 __fastcall buggychl_main_read(UINT16 address)
{
//	if ((address & 0xff00) == 0xd400) address &= ~0xfc;
//	if ((address & 0xff00) == 0xd600) address &= ~0xe4;

	switch (address)
	{
		case 0xd400:
			return standard_taito_mcu_read();

		case 0xd401:
		{
			INT32 res = 0;
			if (!main_sent) res |= 0x01;
			if (mcu_sent) res |= 0x02;
			return res;
		}

		case 0xd600:
			return DrvDips[0];

		case 0xd601:
			return DrvDips[1];

		case 0xd602:
			return DrvDips[2];

		case 0xd603:
			return DrvInputs[0];

		case 0xd608:
			BurnTrackballUpdate(0);
			return BurnTrackballRead(0, 0);

		case 0xd609:
			return (DrvInputs[1] & 0x0f) | (ProcessAnalog(AnalogPort1, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0xff) & 0xf0); // analog (pedal)

		case 0xd60a:
		case 0xd60b:
			return 0;

		case 0xd610:
			soundlatch[1] &= 0xff;
			return soundlatch[1];

		case 0xd611:
			return (soundlatch[1] >> 8) | ((soundlatch[0] >> 8) << 1);
	}

	return 0;
}

static void sound_control_0_w(UINT8 data)
{
	ta7630_snd_ctrl0 = data & 0xff;

	double vol = ta7630_vol_ctrl[(ta7630_snd_ctrl0 >> 4) & 15] / 100.0;

	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_0);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_1);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_2);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_3);
}

static void __fastcall buggychl_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4800:
		case 0x4801:
		case 0x4802:
		case 0x4803:
			AY8910Write((address >> 1) & 1, address & 1, data);
		return;

		case 0x4810:
		case 0x4811:
		case 0x4812:
		case 0x4813:
		case 0x4814:
		case 0x4815:
		case 0x4816:
		case 0x4817:
		case 0x4818:
		case 0x4819:
		case 0x481a:
		case 0x481b:
		case 0x481c:
		case 0x481d:
			MSM5232Write(address, data);
		return;

		case 0x4820:
			sound_control_0_w(data);
		return;

		case 0x4830:
			sndbyte_4830 = data;
		return;

		case 0x5000:
			soundlatch[1] = data | 0x100;
		return;

		case 0x5001:
			nmi_enabled = 1;
			if (nmi_pending) {
				nmi_pending = 0;
				ZetNmi();
			}
		return;

		case 0x5002:
			nmi_enabled = 0;
		return;

		case 0x5003:
			sound_enabled = data & 1;
		return;
	}
}

static UINT8 __fastcall buggychl_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x4830:
			return sndbyte_4830;

		case 0x5000:
			soundlatch[0] &= 0xff;
			return soundlatch[0];

		case 0x5001:
			return (soundlatch[0] >> 8) | ((soundlatch[1] >> 8) << 1);
	}

	return 0;
}

static void ta7630_init()
{
	double db           = 0.0;
	double db_step      = 1.50;
	double db_step_inc  = 0.125;

	for (INT32 i = 0; i < 16; i++)
	{
		double max = 100.0 / pow(10.0, db/20.0 );
		ta7630_vol_ctrl[15 - i] = (INT32)max;

		db += db_step;
		db_step += db_step_inc;
	}
}

static void AY0_PortA_write(UINT32 /*addr*/, UINT32 data)
{
	if (data == 0xff) return; // ignore ay-init
	ta7630_snd_ctrl1 = data & 0xff;
	AY8910SetAllRoutes(0, ta7630_vol_ctrl[(ta7630_snd_ctrl1 >> 4) & 15] / 200.0, BURN_SND_ROUTE_BOTH);
}

static void AY1_PortA_write(UINT32 /*addr*/, UINT32 data)
{
	if (data == 0xff) return; // ignore ay-init
	ta7630_snd_ctrl2 = data & 0xff;
	AY8910SetAllRoutes(1, ta7630_vol_ctrl[(ta7630_snd_ctrl2 >> 4) & 15] / 200.0, BURN_SND_ROUTE_BOTH);
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(1, DrvVidRAM[offs + 0x400], 0, 0);
}

static tilemap_callback( fg )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], 0, 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	bankswitch(0);
	spritelut_bankswitch(0);
	ZetReset();
	ZetClose();

	ZetReset(1);

	m67805_taito_reset();

	BurnWatchdogReset();

	AY8910Reset(0);
	AY8910Reset(1);
	ta7630_init();
	MSM5232Reset();

	BurnShiftReset();

	soundlatch[0] = 0;
	soundlatch[1] = 0;
	flipscreen = 0;
	bgclip_on = 0;
	sprite_color_base = 0;
	sky_on = 0;
	bg_scrollx = 0;
	sndbyte_4830 = 0;

	ta7630_snd_ctrl0 = 0;
	ta7630_snd_ctrl1 = 0;
	ta7630_snd_ctrl2 = 0;

	sound_enabled = 0;
	nmi_enabled = 0;
	nmi_pending = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x020000;
	DrvZ80ROM1		= Next; Next += 0x010000;
	DrvMCUROM		= Next; Next += 0x000800;

	DrvCharROM		= Next; Next += 0x010000;
	DrvSprROM		= Next; Next += 0x040000;
	DrvZoomROM		= Next; Next += 0x004000;

	DrvPalette		= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvMCURAM		= Next; Next += 0x000080;
	DrvVidRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x000100;

	DrvPalRAM		= Next; Next += 0x000100;
	DrvVScrRAM		= Next; Next += 0x000100;
	DrvHScrRAM		= Next; Next += 0x000100;
	DrvCharRAM		= Next; Next += 0x002000;
	DrvSprLutRAM	= Next; Next += 0x002000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { (0x20000/4)*8*3, (0x20000/4)*8*2, (0x20000/4)*8*1, (0x20000/4)*8*0 };
	INT32 XOffs[16] = { STEP8((0x20000/8)*8+7,-1), STEP8(7,-1) };
	INT32 YOffs[1]  = { 0 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvSprROM, 0x20000);

	GfxDecode(0x4000, 4, 16, 1, Plane, XOffs, YOffs, 0x008, tmp, DrvSprROM);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x14000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x18000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM  + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSprROM  + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x14000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSprROM  + 0x1c000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZoomROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZoomROM + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZoomROM + 0x03000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,			0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvSprLutRAM,			0x9000, 0x9fff, MAP_RAM);	// sprite lut - banked!
//	ZetMapMemory(DrvCharRAM,			0xa000, 0xbfff, MAP_WRITE); // rom is banked here too!!
	ZetMapMemory(DrvVidRAM,				0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0xd500, 0xd5ff, MAP_WRITE);	// write only!
	ZetMapMemory(DrvPalRAM,				0xd700, 0xd7ff, MAP_WRITE);	// write only!
	ZetMapMemory(DrvVScrRAM,			0xd800, 0xd8ff, MAP_RAM);	// 40-5f
	ZetMapMemory(DrvHScrRAM,			0xdb00, 0xdbff, MAP_RAM);
	ZetSetWriteHandler(buggychl_main_write);
	ZetSetReadHandler(buggychl_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,			0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,			0x4000, 0x47ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM1 + 0xe000,	0xe000, 0xefff, MAP_ROM);
	ZetSetWriteHandler(buggychl_sound_write);
	ZetSetReadHandler(buggychl_sound_read);
	ZetClose();

	m67805_taito_init(DrvMCUROM, DrvMCURAM, &standard_m68705_interface);

	BurnWatchdogInit(DrvDoReset, 180);

	AY8910Init(0, 2000000, 0);
	AY8910Init(1, 2000000, 0);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetPorts(0, NULL, NULL, AY0_PortA_write, NULL);
	AY8910SetPorts(1, NULL, NULL, AY1_PortA_write, NULL);

	MSM5232Init(2000000, 1);
	MSM5232SetCapacitors(0.39e-6, 0.39e-6, 0.39e-6, 0.39e-6, 0.39e-6, 0.39e-6, 0.39e-6, 0.39e-6);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_0);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_1);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_2);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_3);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_4);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_5);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_6);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_7);

	ta7630_init();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvCharROM, 4, 8, 8, 0x10000, 0x00, 0);
	GenericTilemapSetGfx(1, DrvCharROM, 4, 8, 8, 0x10000, 0x20, 0);
//	GenericTilemapSetScrollCols(0, 32);
//	GenericTilemapSetScrollRows(0, 256);
//	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	BurnBitmapAllocate(1, 256, 256, true);
	BurnBitmapAllocate(2, 256, 256, true);

	BurnTrackballInit(1);
	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_GREEN, 80);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	m67805_taito_exit();
	AY8910Exit(0);
	AY8910Exit(1);
	MSM5232Exit();
	BurnWatchdogExit();
	BurnTrackballExit();
	BurnShiftExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x100; i+=2)
	{
		UINT8 r = DrvPalRAM[i + 0] & 0xf;
		UINT8 g = DrvPalRAM[i + 1] >> 4;
		UINT8 b = DrvPalRAM[i + 1] & 0xf;

		DrvPalette[(i / 2) + 0x00] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);

		r = 0;
		g = 240 - (i / 2);
		b = 255;

		DrvPalette[(i / 2) + 0x80] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sky()
{
	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		UINT16 *dst = pTransDraw + y * nScreenWidth;

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			dst[x] = 0x80 + (x / 2);
		}
	}
}

static void draw_sprites()
{
	const UINT8 *gfx = DrvZoomROM;
	UINT8 *spriteram = DrvSprRAM;
	UINT8 *sprite_lookup = DrvSprLutRAM;
	INT32 flip_screen_y = flipscreen & 1;
	INT32 flip_screen_x = flipscreen & 2;

	for (INT32 offs = 0; offs < 0x80; offs += 4)
	{
		INT32 sx, sy, flipy, zoom, ch, x, px, y;
		const UINT8 *lookup;
		const UINT8 *zoomx_rom, *zoomy_rom;

		sx = spriteram[offs + 3] - ((spriteram[offs + 2] & 0x80) << 1);
		sy = (256 - 64 - spriteram[offs] + ((spriteram[offs + 1] & 0x80) << 1)) - 16;
		flipy = spriteram[offs + 1] & 0x40;
		zoom = spriteram[offs + 1] & 0x3f;
		zoomy_rom = gfx + (zoom << 6);
		zoomx_rom = gfx + 0x2000 + (zoom << 3);

		lookup = sprite_lookup + ((spriteram[offs + 2] & 0x7f) << 6);

		for (y = 0; y < 64; y++)
		{
			int dy = flip_screen_y ? (255 - sy - y) : (sy + y);

			if ((dy & ~0xff) == 0)
			{
				int charline, base_pos;

				charline = zoomy_rom[y] & 0x07;
				base_pos = zoomy_rom[y] & 0x38;
				if (flipy)
					base_pos ^= 0x38;

				px = 0;
				for (ch = 0; ch < 4; ch++)
				{
					int pos, code, realflipy;
					const UINT8 *pendata;

					pos = base_pos + 2 * ch;
					code = 8 * (lookup[pos] | ((lookup[pos + 1] & 0x07) << 8));
					realflipy = (lookup[pos + 1] & 0x80) ? !flipy : flipy;
					code += (realflipy ? (charline ^ 7) : charline);
					pendata = DrvSprROM + (code * 16);

					for (x = 0; x < 16; x++)
					{
						int col = pendata[x];
						if (col)
						{
							int dx = flip_screen_x ? (255 - sx - px) : (sx + px);
							if ((dx & ~0xff) == 0) {
								if (dx >= 0 && dx < nScreenWidth && dy >= 0 && dy < nScreenHeight) {
									pTransDraw[dy * nScreenWidth + dx] = sprite_color_base + col;
								}
							}
						}

						/* the following line is almost certainly wrong */
						if (zoomx_rom[7 - (2 * ch + x / 8)] & (1 << (x & 7)))
							px++;
					}
				}
			}
		}
	}
}

static void copyscrollbitmap(UINT16 *dest, UINT16 *src, INT32 rows, INT32 *scrollx, INT32 cols, INT32 *scrolly)
{
	for (INT32 y = 0; y < 256; y++)
	{
		for (INT32 x = 0; x < 256; x++)
		{
			INT32 sy = y;
			if (cols == 1) {
				sy -= *scrolly;
			} else if (cols > 1) {
				sy -= scrolly[x];
			}
			sy &= 0xff;

			INT32 sx = x;
			if (rows == 1) {
				sx -= *scrollx;
			} else if (rows > 1) {
				sx -= scrollx[y];
			}
			sx &= 0xff;

			dest[sy * 256 + sx] = src[y * 256 + x];
		}
	}
}

static void copy_bitmap(UINT16 *dest, UINT16 *src)
{
	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		for (INT32 x = 0; x < (nScreenWidth - ((bgclip_on) ? 64 : 0)); x++)
		{
			INT32 pxl = src[(y * 256) + x];
			if (pxl != 32)
			{
				dest[y * nScreenWidth + x] = pxl;
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear(0);

	if (nBurnLayer & 1)
	{
		if (sky_on) {
			draw_sky();
		} else {
			BurnTransferClear(0x0020);
		}
	}

	INT32 scroll[256];
	GenericTilemapDraw(0, 1, 0);

	for (INT32 i = 0; i < 256; i++) {
		scroll[i] = DrvVScrRAM[0x40 + (i/8)];
	}

	copyscrollbitmap(BurnBitmapGetBitmap(2), BurnBitmapGetBitmap(1), 1, &bg_scrollx, 256, scroll);

	for (INT32 i = 0; i < 256; i++) {
		scroll[(i - 16) & 0xff] = DrvHScrRAM[i];
	}

	copyscrollbitmap(BurnBitmapGetBitmap(1), BurnBitmapGetBitmap(2), 256, scroll, 0, NULL);

	copy_bitmap(pTransDraw, BurnBitmapGetBitmap(1));

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);
	BurnShiftRender();

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		DrvInputs[0] = 0x00;
		DrvInputs[1] = 0x00;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
		BurnTrackballFrame(0, AnalogPort0, 0, 2, 3);
		BurnTrackballUpdate(0);

		{ // gear shifter stuff
			BurnShiftInputCheckToggle(DrvJoy1[3]);

			DrvInputs[0] &= ~8;
			DrvInputs[0] |= (!bBurnShiftStatus) << 3;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] =  { 6000000 / 60, 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	m6805Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == nInterleave-1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == 127 || i == 255) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		CPU_RUN(2, m6805);
	}
	
	m6805Close();

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		MSM5232Update(pBurnSoundOut, nBurnSoundLen);

		if (!sound_enabled) BurnSoundClear();
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

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

		ZetScan(nAction);
		m68705_taito_scan(nAction);
		BurnWatchdogScan(nAction);
		BurnShiftScan(nAction);
		AY8910Scan(nAction, pnMin);
		MSM5232Scan(nAction, pnMin);

		SCAN_VAR(bankdata);
		SCAN_VAR(spritelut_bank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(bgclip_on);
		SCAN_VAR(sprite_color_base);
		SCAN_VAR(sky_on);
		SCAN_VAR(bg_scrollx);
		SCAN_VAR(sndbyte_4830);

		SCAN_VAR(ta7630_snd_ctrl0);
		SCAN_VAR(ta7630_snd_ctrl1);
		SCAN_VAR(ta7630_snd_ctrl2);

		SCAN_VAR(sound_enabled);
		SCAN_VAR(nmi_enabled);
		SCAN_VAR(nmi_pending);
	}

	if (nAction & ACB_WRITE)
	{
		for (INT32 i = 0; i < 0x800; i++)
		{
			character_expand(i);
		}

		ZetOpen(0);
		bankswitch(bankdata);
		spritelut_bankswitch(spritelut_bank << 4);
		ZetClose();
	}

	return 0;
}


// Buggy Challenge

static struct BurnRomInfo buggychlRomDesc[] = {
	{ "a22-04-2.23",	0x4000, 0x16445a6a, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a22-05-2.22",	0x4000, 0xd57430b2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a22-01.3",		0x4000, 0xaf3b7554, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a22-02.2",		0x4000, 0xb8a645fb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a22-03.1",		0x4000, 0x5f45d469, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "a22-24.28",		0x4000, 0x1e7f841f, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "a22-19.31",		0x0800, 0x06a71df0, 3 | BRF_PRG | BRF_ESS }, //  6 MCU Code

	{ "a22-06.111",		0x4000, 0x1df91b17, 4 | BRF_GRA },           //  7 Sprites
	{ "a22-07.110",		0x4000, 0x2f0ab9b7, 4 | BRF_GRA },           //  8
	{ "a22-08.109",		0x4000, 0x49cb2134, 4 | BRF_GRA },           //  9
	{ "a22-09.108",		0x4000, 0xe682e200, 4 | BRF_GRA },           // 10
	{ "a22-10.107",		0x4000, 0x653b7e25, 4 | BRF_GRA },           // 11
	{ "a22-11.106",		0x4000, 0x8057b55c, 4 | BRF_GRA },           // 12
	{ "a22-12.105",		0x4000, 0x8b365b24, 4 | BRF_GRA },           // 13
	{ "a22-13.104",		0x4000, 0x2c6d68fe, 4 | BRF_GRA },           // 14

	{ "a22-14.59",		0x2000, 0xa450b3ef, 5 | BRF_GRA },           // 15 Sprite Zoom Look-up Tables
	{ "a22-15.115",		0x1000, 0x337a0c14, 5 | BRF_GRA },           // 16
	{ "a22-16.116",		0x1000, 0x337a0c14, 5 | BRF_GRA },           // 17
};

STD_ROM_PICK(buggychl)
STD_ROM_FN(buggychl)

struct BurnDriver BurnDrvBuggychl = {
	"buggychl", NULL, NULL, NULL, "1984",
	"Buggy Challenge\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, buggychlRomInfo, buggychlRomName, NULL, NULL, NULL, NULL, BuggychlInputInfo, BuggychlDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};


// Buggy Challenge (Tecfri)

static struct BurnRomInfo buggychltRomDesc[] = {
	{ "bu04.bin",	0x4000, 0xf90ab854, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "bu05.bin",	0x4000, 0x543d0949, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a22-01.3",	0x4000, 0xaf3b7554, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a22-02.2",	0x4000, 0xb8a645fb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a22-03.1",	0x4000, 0x5f45d469, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "a22-24.28",	0x4000, 0x1e7f841f, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "a22-19.31",	0x0800, 0x06a71df0, 3 | BRF_GRA },           //  6 MCU Code

	{ "a22-06.111",	0x4000, 0x1df91b17, 4 | BRF_GRA },           //  7 Sprites
	{ "a22-07.110",	0x4000, 0x2f0ab9b7, 4 | BRF_GRA },           //  8
	{ "a22-08.109",	0x4000, 0x49cb2134, 4 | BRF_GRA },           //  9
	{ "a22-09.108",	0x4000, 0xe682e200, 4 | BRF_GRA },           // 10
	{ "a22-10.107",	0x4000, 0x653b7e25, 4 | BRF_GRA },           // 11
	{ "a22-11.106",	0x4000, 0x8057b55c, 4 | BRF_GRA },           // 12
	{ "a22-12.105",	0x4000, 0x8b365b24, 4 | BRF_GRA },           // 13
	{ "a22-13.104",	0x4000, 0x2c6d68fe, 4 | BRF_GRA },           // 14

	{ "a22-14.59",	0x2000, 0xa450b3ef, 5 | BRF_GRA },           // 15 Sprite Zoom Look-up Tables
	{ "a22-15.115",	0x1000, 0x337a0c14, 5 | BRF_GRA },           // 16
	{ "a22-16.116",	0x1000, 0x337a0c14, 5 | BRF_GRA },           // 17
};

STD_ROM_PICK(buggychlt)
STD_ROM_FN(buggychlt)

struct BurnDriver BurnDrvBuggychlt = {
	"buggychlt", "buggychl", NULL, NULL, "1984",
	"Buggy Challenge (Tecfri)\0", NULL, "Taito Corporation (Tecfri license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, buggychltRomInfo, buggychltRomName, NULL, NULL, NULL, NULL, BuggychlInputInfo, BuggychlDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 256, 3, 4
};
