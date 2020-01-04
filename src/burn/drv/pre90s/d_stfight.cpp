// FB Alpha 1942 driver module
// Based on MAME driver by Mark McDougall

#include "tiles_generic.h"
#include "z80_intf.h"
#include "taito_m68705.h"
#include "burn_ym2203.h"
#include "msm5205.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80OPS0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvGfxROM5;
static UINT8 *DrvGfxROM6;
static UINT8 *DrvGfxROM7;
static UINT8 *DrvGfxROM8;
static UINT8 *DrvGfxROM9;
static UINT8 *DrvSndROM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvMCURAM;

static UINT16 *DrvBitmap[2];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles[3];

static UINT8 cpu_to_mcu_data;
static INT32 cpu_to_mcu_empty;
static UINT16 sprite_base;
static UINT8 coin_state;
static UINT8 soundlatch;
static UINT8 video_regs[10];

static INT32 adpcm_reset;
static UINT16 adpcm_data_off;
static UINT8 vck2;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static struct BurnInputInfo StfightInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Stfight)

static struct BurnDIPInfo StfightDIPList[]=
{
	{0x11, 0xff, 0xff, 0x3f, NULL					},
	{0x12, 0xff, 0xff, 0x4d, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x11, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x04, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x07, 0x03, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x11, 0x01, 0x18, 0x10, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x18, 0x18, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x18, 0x00, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0x18, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x20, 0x20, "Off"					},
	{0x11, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x11, 0x01, 0x40, 0x40, "No"					},
	{0x11, 0x01, 0x40, 0x00, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Bullet Color"		    },
	{0x11, 0x01, 0x80, 0x00, "Red"					},
	{0x11, 0x01, 0x80, 0x80, "Blue"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x12, 0x01, 0x01, 0x01, "Upright"				},
	{0x12, 0x01, 0x01, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x06, 0x06, "Easy"					},
	{0x12, 0x01, 0x06, 0x04, "Normal"				},
	{0x12, 0x01, 0x06, 0x02, "Hard"					},
	{0x12, 0x01, 0x06, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x12, 0x01, 0x18, 0x18, "1"					},
	{0x12, 0x01, 0x18, 0x10, "2"					},
	{0x12, 0x01, 0x18, 0x08, "3"					},
	{0x12, 0x01, 0x18, 0x00, "4"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x12, 0x01, 0x60, 0x60, "10000 30000"			},
	{0x12, 0x01, 0x60, 0x40, "20000 40000"			},
	{0x12, 0x01, 0x60, 0x20, "30000 60000"			},
	{0x12, 0x01, 0x60, 0x00, "40000 80000"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x80, 0x80, "Off"					},
	{0x12, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Stfight)

static void __fastcall stfight_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc500:
			soundlatch = 0x80 | data;
		return;

		case 0xc600:
			cpu_to_mcu_data = data & 0x0f;
			cpu_to_mcu_empty = 0;
		return;

		case 0xc700:
			coin_state |= ~data & 3;
		return;

		case 0xc804:
			// coin counters
		return;

		case 0xc806:
		return;	// nop

		case 0xc807:
			sprite_base = ((data & 0x04) << 7) | ((data & 0x01) << 8);
		return;

		case 0xd800:
		case 0xd801:
		case 0xd802:
		case 0xd803:
		case 0xd804:
		case 0xd805:
		case 0xd806:
		case 0xd807:
		case 0xd808:
			video_regs[address & 0xf] = data;
		return;
	}
}

static UINT8 __fastcall stfight_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xc200:
			return DrvInputs[0];

		case 0xc201:
			return DrvInputs[1];

		case 0xc202:
			return DrvInputs[2];

		case 0xc203:
			return DrvDips[0];

		case 0xc204:
			return DrvDips[1];

		case 0xc205:
			return coin_state;
	}

	return 0;
}

static void __fastcall stfight_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
		case 0xc800:
		case 0xc801:
			BurnYM2203Write((address / 0x800) & 1, address & 1, data);
		return;

		case 0xd800:
		case 0xe800:
		return; // nop
	}
}

static UINT8 __fastcall stfight_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
		case 0xc800:
		case 0xc801:
			return BurnYM2203Read((address / 0x800) & 1, address & 1);

		case 0xd000:
			return 0; // nop

		case 0xf000:
			UINT8 ret = soundlatch;
			soundlatch &= 0x7f;
			return ret;
	}

	return 0;
}

static void stfight_m68705_portB_out(UINT8 *data)
{
	if ((*data & 0x20) == 0) {
		cpu_to_mcu_empty = 1;
	}
}

static void stfight_m68705_portC_out(UINT8 *data)
{
	if ((portC_out & 1) && !(*data & 1)) coin_state &= ~1;
	if ((portC_out & 2) && !(*data & 2)) coin_state &= ~2;

	adpcm_reset = *data & 0x04;
	if (!adpcm_reset && (portC_out & 4))
		adpcm_data_off = portA_out << 9;

	MSM5205ResetWrite(0, adpcm_reset ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);

	ZetOpen(0);
	ZetSetIRQLine(0x20, (*data & 8) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
	ZetClose();
}

static void stfight_m68705_portB_in()
{
	portB_in = (DrvInputs[3] << 6) | (cpu_to_mcu_empty ? 0x10 : 0x00) | (cpu_to_mcu_data & 0x0f);
}

static void stfight_adpcm_int()
{
	m68705SetIrqLine(0, vck2 ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	vck2 = !vck2;

	if (!adpcm_reset)
	{
		UINT8 adpcm_data = DrvSndROM[(adpcm_data_off >> 1) & 0x7fff];

		if (~adpcm_data_off & 1)
			adpcm_data >>= 4;
		adpcm_data_off++;

		MSM5205DataWrite(0, adpcm_data & 0x0f);
	}
}

static m68705_interface stfight_m68705_interface = {
	NULL, stfight_m68705_portB_out, stfight_m68705_portC_out,
	NULL, NULL, NULL,
	NULL, stfight_m68705_portB_in, NULL
};

static tilemap_scan( bg )
{
	return ((col & 0x0e) >> 1) + ((row & 0x0f) << 3) + ((col & 0x70) << 3) +
			((row & 0x80) << 3) + ((row & 0x10) << 7) + ((col & 0x01) << 12) + ((row & 0x60) << 8);
}

static tilemap_scan( fg )
{
	return (col & 0x0f) + ((row & 0x0f) << 4) + ((col & 0x70) << 4) + ((row & 0xf0) << 7);
}

static tilemap_callback( bg )
{
	INT32 attr = DrvGfxROM5[offs + 0x8000];
	INT32 code = DrvGfxROM5[offs] + (((attr & 0x20) << 4) | ((attr & 0x80) << 1));

	TILE_SET_INFO(0, code, attr, 0);
}

static tilemap_callback( fg )
{
	INT32 attr = DrvGfxROM4[offs + 0x8000];
	INT32 code = DrvGfxROM4[offs] + (((attr & 0x20) << 3) | ((attr & 0x80) << 2));

	TILE_SET_INFO(1, code, attr, 0);
}

static tilemap_callback( tx )
{
	UINT8 attr = DrvTxtRAM[offs+0x400];

	TILE_SET_INFO(2, DrvTxtRAM[offs] + ((attr & 0x80) << 1), attr, TILE_FLIPYX((attr & 0x60) >> 5));
}

inline static INT32 DrvMSM5205SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)m6805TotalCycles() * nSoundRate / (3000000 / 4));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	BurnYM2203Write(0, 0, 0x2f); // set prescale (game doesn't do this)
	BurnYM2203Write(1, 0, 0x2f); // set prescale (game doesn't do this)
	ZetClose();

	MSM5205Reset();
	m67805_taito_reset();

	cpu_to_mcu_data = 0;
	cpu_to_mcu_empty = 1;
	sprite_base = 0;
	coin_state = 0;
	soundlatch = 0;
	memset (video_regs, 0, 10);

	adpcm_reset = 1;
	adpcm_data_off = 0;
	vck2 = 0;

	nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x010000;
	DrvZ80OPS0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x008000;

	DrvMCUROM		= Next; Next += 0x000800;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x040000;
	DrvGfxROM2		= Next; Next += 0x040000;
	DrvGfxROM3		= Next; Next += 0x040000;
	DrvGfxROM4		= Next; Next += 0x010000;
	DrvGfxROM5		= Next; Next += 0x010000;
	DrvGfxROM6		= Next; Next += 0x000100;
	DrvGfxROM7		= Next; Next += 0x000200;
	DrvGfxROM8		= Next; Next += 0x000200;
	DrvGfxROM9		= Next; Next += 0x000200;

	DrvSndROM		= Next; Next += 0x008000;

	DrvBitmap[0]	= (UINT16*)Next; Next += 256 * 256 * sizeof(UINT16);
	DrvBitmap[1]	= (UINT16*)Next; Next += 256 * 256 * sizeof(UINT16);

	DrvPalette		= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x001000;
	DrvTxtRAM		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000200;
	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x000800;

	DrvMCURAM		= Next; Next += 0x000080;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[ 2] = { 4, 0 };
	INT32 XOffs0[ 8] = { STEP4(0,1), STEP4(8,1) };
	INT32 YOffs0[ 8] = { STEP8(0,16) };

	INT32 Plane1[ 4] = { 0x80000, 0x80000+4, 0, 4 };
	INT32 XOffs1[16] = { STEP4(0,1), STEP4(8,1), STEP4(256,1), STEP4(256+8,1) };
	INT32 YOffs1[16] = { STEP16(0,16) };

	INT32 Plane2[ 4] = { 0x80000+4, 0x80000+0, 4, 0 };
	INT32 XOffs2[16] = { STEP4(0,1), STEP4(8,1), STEP4(512,1), STEP4(512+8,1) };
	INT32 YOffs2[16] = { STEP16(0,16) };

	INT32 Plane3[ 4] = { 0x80000, 0x80000+4, 0, 4 };
	INT32 XOffs3[16] = { STEP4(0,1), STEP4(8,1), STEP4(256,1), STEP4(256+8,1) };
	INT32 YOffs3[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x02000);

	GfxDecode(0x200, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp + 0x00000, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x20000);

	GfxDecode(0x400, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp + 0x00000, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x20000);

	GfxDecode(0x200, 4, 16, 16, Plane2, XOffs2, YOffs2, 0x400, tmp + 0x00000, DrvGfxROM2);
	GfxDecode(0x200, 4, 16, 16, Plane2, XOffs2, YOffs2, 0x400, tmp + 0x00020, DrvGfxROM2 + 0x20000);

	memcpy (tmp, DrvGfxROM3, 0x20000);

	GfxDecode(0x400, 4, 16, 16, Plane3, XOffs3, YOffs3, 0x200, tmp + 0x00000, DrvGfxROM3);

	BurnFree (tmp);

	return 0;
}

static void DrvZ80Decode()
{
	for (INT32 i = 0; i < 0x8000; i++) {
		UINT8 src = DrvZ80ROM0[i];

		DrvZ80OPS0[i] = (src & 0xa6) |	((((src << 2) ^ src) << 3) & 0x40) |
				(~((src ^ (i >> 1)) >> 2) & 0x10) | (~(((src << 1) ^ i) << 2) & 0x08) | (((src ^ (src >> 3)) >> 1) & 0x01);

		DrvZ80ROM0[i] =	(src & 0xa6) | (~((src ^ (src << 1)) << 5) & 0x40) |
			(((src ^ (i << 3)) << 1) & 0x10) |	(((src ^ i) >> 1) & 0x08) |	(~((src >> 6) ^ i) & 0x01);
	}
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
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM  + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x18000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x10000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x18000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000, 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x10000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x18000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x08000, 16, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x00000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x08000, 18, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM5 + 0x00000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM5 + 0x08000, 20, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM6 + 0x00000, 21, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM7 + 0x00000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM7 + 0x00100, 23, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM8 + 0x00000, 24, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM8 + 0x00100, 25, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM9 + 0x00000, 26, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM9 + 0x00100, 27, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x00000, 29, 1)) return 1;

		for (INT32 i = 0; i < 0x100; i++) { // merge nibbles
			DrvGfxROM7[i] = (DrvGfxROM7[i] << 4) | (DrvGfxROM7[i + 0x100] & 0xf);
			DrvGfxROM8[i] = (DrvGfxROM8[i] << 4) | (DrvGfxROM8[i + 0x100] & 0xf);
			DrvGfxROM9[i] = (DrvGfxROM9[i] << 4) | (DrvGfxROM9[i + 0x100] & 0xf);
		}

		DrvGfxDecode();
		DrvZ80Decode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,			0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80OPS0,			0x0000, 0x7fff, MAP_FETCHOP); // decrypted opcodes
	ZetMapMemory(DrvPalRAM,				0xc000, 0xc1ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,				0xd000, 0xd7ff, MAP_RAM); // stfight
	ZetMapMemory(DrvZ80RAM0,			0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0xf000, 0xffff, MAP_RAM); // stfight
	ZetSetWriteHandler(stfight_main_write);
	ZetSetReadHandler(stfight_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,			0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(stfight_sound_write);
	ZetSetReadHandler(stfight_sound_read);
	ZetClose();

	m67805_taito_init(DrvMCUROM, DrvMCURAM, &stfight_m68705_interface);

	MSM5205Init(0, DrvMSM5205SynchroniseStream, 384000, stfight_adpcm_int, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	BurnYM2203Init(2, 1500000, NULL, 0);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE,   0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, bg_map_scan, bg_map_callback, 16, 16, 128, 256);
	GenericTilemapInit(1, fg_map_scan, fg_map_callback, 16, 16, 128, 256);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, tx_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM2, 4, 16, 16, 0x40000, 0, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x40000, 0, 0x07);
	GenericTilemapSetGfx(2, DrvGfxROM0, 2,  8,  8, 0x08000, 0, 0x0f);
	GenericTilemapSetOffsets(0, 0, -16);
	GenericTilemapSetOffsets(1, 0, -16);
	GenericTilemapSetOffsets(2, 0, -16);
	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	m67805_taito_exit();

	BurnYM2203Exit();
	MSM5205Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r = DrvPalRAM[0x000 + i] >> 4;
		UINT8 g = DrvPalRAM[0x000 + i] & 0x0f;
		UINT8 b = DrvPalRAM[0x100 + i] & 0x0f;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x1000 - 0x20; offs >= 0; offs -= 0x20)
	{
		INT32 sy = DrvSprRAM[offs + 2];
		if (sy == 0) continue;

		INT32 attr  = DrvSprRAM[offs + 1];
		INT32 flipx = attr & 0x10;
		INT32 flipy = 0;
		INT32 color =(attr & 0x0f) | (attr & 0x20) >> 1; // mix in priority bit

		INT32 sx = DrvSprRAM[offs + 3];
		if ((sx >= 0xf0) && (attr & 0x80)) sx -= 0x100;

		Draw16x16MaskTile(DrvBitmap[0], DrvSprRAM[offs] + sprite_base, sx, sy-16, flipx, flipy, color, 4, 0xf, 0, DrvGfxROM3);
	}
}

static void layer_mixer(UINT16 *srcbitmap, UINT8 *clut, INT32 base, INT32 mask, INT32 condition, INT32 realcheck)
{
	for (INT32 y = 0; y < 224; y++)
	{
		UINT16 *dest = pTransDraw + y * nScreenWidth;
		UINT16 *src = srcbitmap + y * nScreenWidth;

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			if (src[x] == -1)
				continue;

			if ((src[x] & mask) == condition)
			{
				UINT8 pix = src[x] & 0xff;
				UINT8 real = clut[pix];

				if (realcheck) // the text layer transparency appears to be 0xf *after* lookup
				{
					if ((real & 0x0f) != 0x0f)
					{
						dest[x] = (real & 0x3f) + base;
					}
				}
				else if ((src[x] & 0xf) != 0xf) // other layers it's pre-lookup
				{
					dest[x] = (real & 0x3f) + base;
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	BurnTransferClear();

	INT32 foreground_enable = video_regs[7] & 0x10;
	INT32 background_enable = video_regs[7] & 0x20;
	INT32 sprite_enable = video_regs[7] & 0x40;
	INT32 text_enable = video_regs[7] & 0x80;

	if (sprite_enable)
	{
		memset (DrvBitmap[0], 0xff, 256 * 256 * sizeof(UINT16));

		draw_sprites();
	}

	if (background_enable && nBurnLayer&1)
	{
		GenericTilemapSetScrollX(0, video_regs[4] + (video_regs[5] * 256));
		GenericTilemapSetScrollY(0, video_regs[6] + (video_regs[8] * 256));

		GenericTilemapDraw(0, DrvBitmap[1], 0);
		layer_mixer(DrvBitmap[1], DrvGfxROM8, 0x00, 0x000, 0x000, 0);
	}
	else
	{
		BurnTransferClear();
	}

	if (sprite_enable && nSpriteEnable&1)
	{
		layer_mixer(DrvBitmap[0], DrvGfxROM9, 0x80, 0x100, 0x100, 0);
	}

	if (foreground_enable && nBurnLayer&2)
	{
		GenericTilemapSetScrollX(1, video_regs[0] + (video_regs[1] * 256));
		GenericTilemapSetScrollY(1, video_regs[2] + (video_regs[3] * 256));

		GenericTilemapDraw(1, DrvBitmap[1], 0);

		layer_mixer(DrvBitmap[1], DrvGfxROM7, 0x40, 0x000, 0x000, 0);
	}

	if (sprite_enable && nSpriteEnable&2)
	{
		layer_mixer(DrvBitmap[0], DrvGfxROM9, 0x80, 0x100, 0x000, 0);
	}

	if (text_enable && nBurnLayer&4)
	{
		GenericTilemapDraw(2, DrvBitmap[1], 0);
		layer_mixer(DrvBitmap[1], DrvGfxROM6, 0xc0, 0x000, 0x000, 1);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 3000000 / 60, 3000000 / 60, 3000000 / 4 / 60 };
	INT32 nCyclesDone[3] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2] };

	ZetNewFrame();
	m6805NewFrame();
	MSM5205NewFrame(0, 3000000 / 4, nInterleave);

	m6805Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);

		if (i == 240) {
			ZetSetVector(0xcf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		if (i == 255 || i == 127) {
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));

		if (i == 127 || i == 255) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		nCyclesDone[2] += m6805Run(((i + 1) * nCyclesTotal[2] / nInterleave) - nCyclesDone[2]);
		MSM5205UpdateScanline(i);
	}

	ZetOpen(1);

	BurnTimerEndFrame(nCyclesTotal[1]);
	
	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	m6805Close();
	ZetClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = 0; // sound cpu, not critical
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];

	if (pBurnDraw) {
		DrvDraw();
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
		BurnYM2203Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);
		m68705_taito_scan(nAction);

		SCAN_VAR(cpu_to_mcu_data);
		SCAN_VAR(cpu_to_mcu_empty);
		SCAN_VAR(sprite_base);
		SCAN_VAR(coin_state);
		SCAN_VAR(soundlatch);
		SCAN_VAR(video_regs);
		SCAN_VAR(adpcm_reset);
		SCAN_VAR(adpcm_data_off);
		SCAN_VAR(vck2);

		SCAN_VAR(nExtraCycles);
	}

	return 0;
}


// Empire City: 1931 (bootleg?)

static struct BurnRomInfo empcityRomDesc[] = {
	{ "ec_01.rom",			0x8000, 0xfe01d9b1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "ec_02.rom",			0x8000, 0xb3cf1ef7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ec_04.rom",			0x8000, 0xaa3e7d1e, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "empcityu_68705.3j",	0x0800, 0x182f7616, 3 | BRF_PRG | BRF_ESS }, //  3 M68705 MCU Code

	{ "17.2n",				0x2000, 0x1b3706b5, 4 | BRF_GRA },           //  4 Character Tiles

	{ "7.4c",				0x8000, 0x2c6caa5f, 5 | BRF_GRA },           //  5 Foreground Tiles
	{ "8.5c",				0x8000, 0xe11ded31, 5 | BRF_GRA },           //  6
	{ "5.2c",				0x8000, 0x0c099a31, 5 | BRF_GRA },           //  7
	{ "6.3c",				0x8000, 0x3cc77c31, 5 | BRF_GRA },           //  8

	{ "13.4c",				0x8000, 0x0ae48dd3, 6 | BRF_GRA },           //  9 Background Tiles
	{ "14.5j",				0x8000, 0xdebf5d76, 6 | BRF_GRA },           // 10
	{ "11.2j",				0x8000, 0x8261ecfe, 6 | BRF_GRA },           // 11
	{ "12.3j",				0x8000, 0x71137301, 6 | BRF_GRA },           // 12

	{ "20.8w",				0x8000, 0x8299f247, 7 | BRF_GRA },           // 13 Sprites
	{ "21.9w",				0x8000, 0xb57dc037, 7 | BRF_GRA },           // 14
	{ "18.6w",				0x8000, 0x68acd627, 7 | BRF_GRA },           // 15
	{ "19.7w",				0x8000, 0x5170a057, 7 | BRF_GRA },           // 16

	{ "9.7c",				0x8000, 0x8ceaf4fe, 8 | BRF_GRA },           // 17 Foreground Tilemap
	{ "10.8c",				0x8000, 0x5a1a227a, 8 | BRF_GRA },           // 18

	{ "15.7j",				0x8000, 0x27a310bc, 9 | BRF_GRA },           // 19 Background Tilemap
	{ "16.8j",				0x8000, 0x3d19ce18, 9 | BRF_GRA },           // 20

	{ "82s129.006",			0x0100, 0xf9424b5b, 10 | BRF_GRA },          // 21 Text Layer Color Look-up Table

	{ "82s129.002",			0x0100, 0xc883d49b, 11 | BRF_GRA },          // 22 Foreground Layer Color Look-up Table
	{ "82s129.003",			0x0100, 0xaf81882a, 11 | BRF_GRA },          // 23

	{ "82s129.004",			0x0100, 0x1831ce7c, 12 | BRF_GRA },          // 24 Background Layer Color Look-up Table
	{ "82s129.005",			0x0100, 0x96cb6293, 12 | BRF_GRA },          // 25

	{ "82s129.052",			0x0100, 0x3d915ffc, 13 | BRF_GRA },          // 26 Sprite Color Look-up Table
	{ "82s129.066",			0x0100, 0x51e8832f, 13 | BRF_GRA },          // 27

	{ "82s129.015",			0x0100, 0x0eaf5158, 14 | BRF_OPT },          // 28 Unknown PROM

	{ "5j",					0x8000, 0x1b8d0c07, 15 | BRF_SND },          // 29 MSM5205 Samples
};

STD_ROM_PICK(empcity)
STD_ROM_FN(empcity)

struct BurnDriver BurnDrvEmpcity = {
	"empcity", NULL, NULL, NULL, "1986",
	"Empire City: 1931 (bootleg?)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, empcityRomInfo, empcityRomName, NULL, NULL, NULL, NULL, StfightInputInfo, StfightDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};

// Empire City: 1931 (US)

static struct BurnRomInfo empcityuRomDesc[] = {
	{ "cpu.4u",		        0x8000, 0xe2c40ea3, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "cpu.2u",		        0x8000, 0x96ee8b81, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ec_04.rom",		    0x8000, 0xaa3e7d1e, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "empcityu_68705.3j",	0x0800, 0x182f7616, 3 | BRF_PRG | BRF_ESS }, //  3 mcu

	{ "vid.2p",		        0x2000, 0x15593793, 4 | BRF_GRA },           //  4 stfight_vid:tx_gfx

	{ "7.4c",		        0x8000, 0x2c6caa5f, 5 | BRF_GRA },           //  5 stfight_vid:fg_gfx
	{ "8.5c",		        0x8000, 0xe11ded31, 5 | BRF_GRA },           //  6
	{ "5.2c",		        0x8000, 0x0c099a31, 5 | BRF_GRA },           //  7
	{ "6.3c",		        0x8000, 0x3cc77c31, 5 | BRF_GRA },           //  8

	{ "13.4c",		        0x8000, 0x0ae48dd3, 6 | BRF_GRA },           //  9 stfight_vid:bg_gfx
	{ "14.5j",		        0x8000, 0xdebf5d76, 6 | BRF_GRA },           // 10
	{ "11.2j",		        0x8000, 0x8261ecfe, 6 | BRF_GRA },           // 11
	{ "12.3j",		        0x8000, 0x71137301, 6 | BRF_GRA },           // 12

	{ "20.8w",		        0x8000, 0x8299f247, 7 | BRF_GRA },           // 13 stfight_vid:spr_gfx
	{ "21.9w",		        0x8000, 0xb57dc037, 7 | BRF_GRA },           // 14
	{ "18.6w",		        0x8000, 0x68acd627, 7 | BRF_GRA },           // 15
	{ "19.7w",		        0x8000, 0x5170a057, 7 | BRF_GRA },           // 16

	{ "9.7c",		        0x8000, 0x8ceaf4fe, 8 | BRF_GRA },           // 17 stfight_vid:fg_map
	{ "10.8c",		        0x8000, 0x5a1a227a, 8 | BRF_GRA },           // 18

	{ "15.7j",		        0x8000, 0x27a310bc, 9 | BRF_GRA },           // 19 stfight_vid:bg_map
	{ "16.8j",		        0x8000, 0x3d19ce18, 9 | BRF_GRA },           // 20

	{ "82s129.006",		    0x0100, 0xf9424b5b, 10 | BRF_GRA },           // 21 stfight_vid:tx_clut

	{ "82s129.002",		    0x0100, 0xc883d49b, 11 | BRF_GRA },           // 22 stfight_vid:fg_clut
	{ "82s129.003",		    0x0100, 0xaf81882a, 11 | BRF_GRA },           // 23

	{ "82s129.004",		    0x0100, 0x1831ce7c, 12 | BRF_GRA },           // 24 stfight_vid:bg_clut
	{ "82s129.005",		    0x0100, 0x96cb6293, 12 | BRF_GRA },           // 25

	{ "82s129.052",		    0x0100, 0x3d915ffc, 13 | BRF_GRA },           // 26 stfight_vid:spr_clut
	{ "82s129.066",		    0x0100, 0x51e8832f, 13 | BRF_GRA },           // 27

	{ "82s129.015",		    0x0100, 0x0eaf5158, 14 | BRF_GRA },           // 28 proms

	{ "5j",			        0x8000, 0x1b8d0c07, 15 | BRF_SND },           // 29 adpcm

	{ "82s123.a7",		    0x0020, 0x93e2d292, 16 | BRF_GRA },           // 30 user1
};

STD_ROM_PICK(empcityu)
STD_ROM_FN(empcityu)

struct BurnDriver BurnDrvEmpcityu = {
	"empcityu", "empcity", NULL, NULL, "1986",
	"Empire City: 1931 (US)\0", NULL, "Seibu Kaihatsu (Taito / Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, empcityuRomInfo, empcityuRomName, NULL, NULL, NULL, NULL, StfightInputInfo, StfightDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};

// Empire City: 1931 (Japan)

static struct BurnRomInfo empcityjRomDesc[] = {
	{ "1.bin",		        0x8000, 0x8162331c, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "2.bin",		        0x8000, 0x960edea6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ec_04.rom",		    0x8000, 0xaa3e7d1e, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "empcityj_68705.3j",	0x0800, 0x19bdb0a9, 3 | BRF_PRG | BRF_ESS }, //  3 mcu

	{ "17.2n",		        0x2000, 0x1b3706b5, 4 | BRF_GRA },           //  4 stfight_vid:tx_gfx

	{ "7.4c",		        0x8000, 0x2c6caa5f, 5 | BRF_GRA },           //  5 stfight_vid:fg_gfx
	{ "8.5c",		        0x8000, 0xe11ded31, 5 | BRF_GRA },           //  6
	{ "5.2c",		        0x8000, 0x0c099a31, 5 | BRF_GRA },           //  7
	{ "6.3c",		        0x8000, 0x3cc77c31, 5 | BRF_GRA },           //  8

	{ "13.4c",		        0x8000, 0x0ae48dd3, 6 | BRF_GRA },           //  9 stfight_vid:bg_gfx
	{ "14.5j",		        0x8000, 0xdebf5d76, 6 | BRF_GRA },           // 10
	{ "11.2j",		        0x8000, 0x8261ecfe, 6 | BRF_GRA },           // 11
	{ "12.3j",		        0x8000, 0x71137301, 6 | BRF_GRA },           // 12

	{ "20.8w",		        0x8000, 0x8299f247, 7 | BRF_GRA },           // 13 stfight_vid:spr_gfx
	{ "21.9w",		        0x8000, 0xb57dc037, 7 | BRF_GRA },           // 14
	{ "18.6w",		        0x8000, 0x68acd627, 7 | BRF_GRA },           // 15
	{ "19.7w",		        0x8000, 0x5170a057, 7 | BRF_GRA },           // 16

	{ "9.7c",		        0x8000, 0x8ceaf4fe, 8 | BRF_GRA },           // 17 stfight_vid:fg_map
	{ "10.8c",		        0x8000, 0x5a1a227a, 8 | BRF_GRA },           // 18

	{ "15.7j",		        0x8000, 0x27a310bc, 9 | BRF_GRA },           // 19 stfight_vid:bg_map
	{ "16.8j",		        0x8000, 0x3d19ce18, 9 | BRF_GRA },           // 20

	{ "82s129.006",		    0x0100, 0xf9424b5b, 10 | BRF_GRA },           // 21 stfight_vid:tx_clut

	{ "82s129.002",		    0x0100, 0xc883d49b, 11 | BRF_GRA },           // 22 stfight_vid:fg_clut
	{ "82s129.003",		    0x0100, 0xaf81882a, 11 | BRF_GRA },           // 23

	{ "82s129.004",		    0x0100, 0x1831ce7c, 12 | BRF_GRA },           // 24 stfight_vid:bg_clut
	{ "82s129.005",		    0x0100, 0x96cb6293, 12 | BRF_GRA },           // 25

	{ "82s129.052",		    0x0100, 0x3d915ffc, 13 | BRF_GRA },           // 26 stfight_vid:spr_clut
	{ "82s129.066",		    0x0100, 0x51e8832f, 13 | BRF_GRA },           // 27

	{ "82s129.015",		    0x0100, 0x0eaf5158, 14 | BRF_GRA },           // 28 proms

	{ "5j",			        0x8000, 0x1b8d0c07, 15 | BRF_SND },           // 29 adpcm
};

STD_ROM_PICK(empcityj)
STD_ROM_FN(empcityj)

struct BurnDriver BurnDrvEmpcityj = {
	"empcityj", "empcity", NULL, NULL, "1986",
	"Empire City: 1931 (Japan)\0", NULL, "Seibu Kaihatsu (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, empcityjRomInfo, empcityjRomName, NULL, NULL, NULL, NULL, StfightInputInfo, StfightDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};

// Empire City: 1931 (Italy)

static struct BurnRomInfo empcityiRomDesc[] = {
	{ "1.bin",		        0x8000, 0x32378e47, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "2.bin",		        0x8000, 0xd20010c6, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sf03.bin",		    0x8000, 0x6a8cb7a6, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "empcityi_68705.3j",	0x0800, 0xb1817d44, 3 | BRF_PRG | BRF_ESS }, //  3 mcu

	{ "17.2n",		        0x2000, 0x1b3706b5, 4 | BRF_GRA },           //  4 stfight_vid:tx_gfx

	{ "7.4c",		        0x8000, 0x2c6caa5f, 5 | BRF_GRA },           //  5 stfight_vid:fg_gfx
	{ "8.5c",		        0x8000, 0xe11ded31, 5 | BRF_GRA },           //  6
	{ "5.2c",		        0x8000, 0x0c099a31, 5 | BRF_GRA },           //  7
	{ "6.3c",		        0x8000, 0x3cc77c31, 5 | BRF_GRA },           //  8

	{ "13.4c",		        0x8000, 0x0ae48dd3, 6 | BRF_GRA },           //  9 stfight_vid:bg_gfx
	{ "14.5j",		        0x8000, 0xdebf5d76, 6 | BRF_GRA },           // 10
	{ "11.2j",		        0x8000, 0x8261ecfe, 6 | BRF_GRA },           // 11
	{ "12.3j",		        0x8000, 0x71137301, 6 | BRF_GRA },           // 12

	{ "20.8w",		        0x8000, 0x8299f247, 7 | BRF_GRA },           // 13 stfight_vid:spr_gfx
	{ "21.9w",		        0x8000, 0xb57dc037, 7 | BRF_GRA },           // 14
	{ "18.6w",		        0x8000, 0x68acd627, 7 | BRF_GRA },           // 15
	{ "19.7w",		        0x8000, 0x5170a057, 7 | BRF_GRA },           // 16

	{ "9.7c",		        0x8000, 0x8ceaf4fe, 8 | BRF_GRA },           // 17 stfight_vid:fg_map
	{ "10.8c",		        0x8000, 0x5a1a227a, 8 | BRF_GRA },           // 18

	{ "15.7j",		        0x8000, 0x27a310bc, 9 | BRF_GRA },           // 19 stfight_vid:bg_map
	{ "16.8j",		        0x8000, 0x3d19ce18, 9 | BRF_GRA },           // 20

	{ "82s129.006",		    0x0100, 0xf9424b5b, 10 | BRF_GRA },           // 21 stfight_vid:tx_clut

	{ "82s129.002",		    0x0100, 0xc883d49b, 11 | BRF_GRA },           // 22 stfight_vid:fg_clut
	{ "82s129.003",		    0x0100, 0xaf81882a, 11 | BRF_GRA },           // 23

	{ "82s129.004",		    0x0100, 0x1831ce7c, 12 | BRF_GRA },           // 24 stfight_vid:bg_clut
	{ "82s129.005",		    0x0100, 0x96cb6293, 12 | BRF_GRA },           // 25

	{ "82s129.052",		    0x0100, 0x3d915ffc, 13 | BRF_GRA },           // 26 stfight_vid:spr_clut
	{ "82s129.066",		    0x0100, 0x51e8832f, 13 | BRF_GRA },           // 27

	{ "82s129.015",		    0x0100, 0x0eaf5158, 14 | BRF_GRA },           // 28 proms

	{ "5j",			        0x8000, 0x1b8d0c07, 15 | BRF_SND },           // 29 adpcm
};

STD_ROM_PICK(empcityi)
STD_ROM_FN(empcityi)

struct BurnDriver BurnDrvEmpcityi = {
	"empcityi", "empcity", NULL, NULL, "1986",
	"Empire City: 1931 (Italy)\0", NULL, "Seibu Kaihatsu (Eurobed license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, empcityiRomInfo, empcityiRomName, NULL, NULL, NULL, NULL, StfightInputInfo, StfightDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};

// Street Fight (Germany)

static struct BurnRomInfo stfightRomDesc[] = {
	{ "a-1.4q",		        0x8000, 0xff83f316, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "sf02.bin",		    0x8000, 0xe626ce9e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sf03.bin",		    0x8000, 0x6a8cb7a6, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "stfight_68705.3j",	0x0800, 0xf4cc50d6, 3 | BRF_PRG | BRF_ESS }, //  3 mcu

	{ "17.2n",		        0x2000, 0x1b3706b5, 4 | BRF_GRA },           //  4 stfight_vid:tx_gfx

	{ "7.4c",		        0x8000, 0x2c6caa5f, 5 | BRF_GRA },           //  5 stfight_vid:fg_gfx
	{ "8.5c",		        0x8000, 0xe11ded31, 5 | BRF_GRA },           //  6
	{ "5.2c",		        0x8000, 0x0c099a31, 5 | BRF_GRA },           //  7
	{ "6.3c",		        0x8000, 0x3cc77c31, 5 | BRF_GRA },           //  8

	{ "13.4c",		        0x8000, 0x0ae48dd3, 6 | BRF_GRA },           //  9 stfight_vid:bg_gfx
	{ "14.5j",		        0x8000, 0xdebf5d76, 6 | BRF_GRA },           // 10
	{ "11.2j",		        0x8000, 0x8261ecfe, 6 | BRF_GRA },           // 11
	{ "12.3j",		        0x8000, 0x71137301, 6 | BRF_GRA },           // 12

	{ "20.8w",		        0x8000, 0x8299f247, 7 | BRF_GRA },           // 13 stfight_vid:spr_gfx
	{ "21.9w",		        0x8000, 0xb57dc037, 7 | BRF_GRA },           // 14
	{ "18.6w",		        0x8000, 0x68acd627, 7 | BRF_GRA },           // 15
	{ "19.7w",		        0x8000, 0x5170a057, 7 | BRF_GRA },           // 16

	{ "9.7c",		        0x8000, 0x8ceaf4fe, 8 | BRF_GRA },           // 17 stfight_vid:fg_map
	{ "10.8c",		        0x8000, 0x5a1a227a, 8 | BRF_GRA },           // 18

	{ "15.7j",		        0x8000, 0x27a310bc, 9 | BRF_GRA },           // 19 stfight_vid:bg_map
	{ "16.8j",		        0x8000, 0x3d19ce18, 9 | BRF_GRA },           // 20

	{ "82s129.006",		    0x0100, 0xf9424b5b, 10 | BRF_GRA },           // 21 stfight_vid:tx_clut

	{ "82s129.002",		    0x0100, 0xc883d49b, 11 | BRF_GRA },           // 22 stfight_vid:fg_clut
	{ "82s129.003",		    0x0100, 0xaf81882a, 11 | BRF_GRA },           // 23

	{ "82s129.004",		    0x0100, 0x1831ce7c, 12 | BRF_GRA },           // 24 stfight_vid:bg_clut
	{ "82s129.005",		    0x0100, 0x96cb6293, 12 | BRF_GRA },           // 25

	{ "82s129.052",		    0x0100, 0x3d915ffc, 13 | BRF_GRA },           // 26 stfight_vid:spr_clut
	{ "82s129.066",		    0x0100, 0x51e8832f, 13 | BRF_GRA },           // 27

	{ "82s129.015",		    0x0100, 0x0eaf5158, 14 | BRF_GRA },           // 28 proms

	{ "5j",			        0x8000, 0x1b8d0c07, 15 | BRF_SND },           // 29 adpcm
};

STD_ROM_PICK(stfight)
STD_ROM_FN(stfight)

struct BurnDriver BurnDrvStfight = {
	"stfight", "empcity", NULL, NULL, "1986",
	"Street Fight (Germany)\0", NULL, "Seibu Kaihatsu (Tuning license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, stfightRomInfo, stfightRomName, NULL, NULL, NULL, NULL, StfightInputInfo, StfightDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};

// Street Fight (bootleg?)

static struct BurnRomInfo stfightaRomDesc[] = {
	{ "sfight2.bin",	    0x8000, 0x8fb4dfc9, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "sfight1.bin",	    0x8000, 0x983ce746, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sf03.bin",		    0x8000, 0x6a8cb7a6, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "stfight_68705.3j",	0x0800, 0xf4cc50d6, 3 | BRF_PRG | BRF_ESS }, //  3 mcu

	{ "17.2n",		        0x2000, 0x1b3706b5, 4 | BRF_GRA },           //  4 stfight_vid:tx_gfx

	{ "7.4c",		        0x8000, 0x2c6caa5f, 5 | BRF_GRA },           //  5 stfight_vid:fg_gfx
	{ "8.5c",		        0x8000, 0xe11ded31, 5 | BRF_GRA },           //  6
	{ "5.2c",		        0x8000, 0x0c099a31, 5 | BRF_GRA },           //  7
	{ "6.3c",		        0x8000, 0x3cc77c31, 5 | BRF_GRA },           //  8

	{ "13.4c",		        0x8000, 0x0ae48dd3, 6 | BRF_GRA },           //  9 stfight_vid:bg_gfx
	{ "14.5j",		        0x8000, 0xdebf5d76, 6 | BRF_GRA },           // 10
	{ "11.2j",		        0x8000, 0x8261ecfe, 6 | BRF_GRA },           // 11
	{ "12.3j",		        0x8000, 0x71137301, 6 | BRF_GRA },           // 12

	{ "20.8w",		        0x8000, 0x8299f247, 7 | BRF_GRA },           // 13 stfight_vid:spr_gfx
	{ "21.9w",		        0x8000, 0xb57dc037, 7 | BRF_GRA },           // 14
	{ "18.6w",		        0x8000, 0x68acd627, 7 | BRF_GRA },           // 15
	{ "19.7w",		        0x8000, 0x5170a057, 7 | BRF_GRA },           // 16

	{ "9.7c",		        0x8000, 0x8ceaf4fe, 8 | BRF_GRA },           // 17 stfight_vid:fg_map
	{ "10.8c",		        0x8000, 0x5a1a227a, 8 | BRF_GRA },           // 18

	{ "15.7j",		        0x8000, 0x27a310bc, 9 | BRF_GRA },           // 19 stfight_vid:bg_map
	{ "16.8j",		        0x8000, 0x3d19ce18, 9 | BRF_GRA },           // 20

	{ "82s129.006",		    0x0100, 0xf9424b5b, 10 | BRF_GRA },           // 21 stfight_vid:tx_clut

	{ "82s129.002",		    0x0100, 0xc883d49b, 11 | BRF_GRA },           // 22 stfight_vid:fg_clut
	{ "82s129.003",		    0x0100, 0xaf81882a, 11 | BRF_GRA },           // 23

	{ "82s129.004",		    0x0100, 0x1831ce7c, 12 | BRF_GRA },           // 24 stfight_vid:bg_clut
	{ "82s129.005",		    0x0100, 0x96cb6293, 12 | BRF_GRA },           // 25

	{ "82s129.052",		    0x0100, 0x3d915ffc, 13 | BRF_GRA },           // 26 stfight_vid:spr_clut
	{ "82s129.066",		    0x0100, 0x51e8832f, 13 | BRF_GRA },           // 27

	{ "82s129.015",		    0x0100, 0x0eaf5158, 14 | BRF_GRA },           // 28 proms

	{ "5j",			        0x8000, 0x1b8d0c07, 15 | BRF_SND },           // 29 adpcm
};

STD_ROM_PICK(stfighta)
STD_ROM_FN(stfighta)

struct BurnDriver BurnDrvStfighta = {
	"stfighta", "empcity", NULL, NULL, "1986",
	"Street Fight (bootleg?)\0", NULL, "Seibu Kaihatsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, stfightaRomInfo, stfightaRomName, NULL, NULL, NULL, NULL, StfightInputInfo, StfightDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};

// Street Fight (Germany - Benelux)

static struct BurnRomInfo stfightgbRomDesc[] = {
	{ "1.4t",		        0x8000, 0x0ce5ca11, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "2.2t",		        0x8000, 0x936ba873, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "c5",			        0x8000, 0x6a8cb7a6, 2 | BRF_PRG | BRF_ESS }, //  2 audiocpu

	{ "stfightgb_68705.3j",	0x0800, 0x3b1b2660, 3 | BRF_PRG | BRF_ESS }, //  3 mcu

	{ "17.2n",		        0x2000, 0x1b3706b5, 4 | BRF_GRA },           //  4 stfight_vid:tx_gfx

	{ "7.4c",		        0x8000, 0x2c6caa5f, 5 | BRF_GRA },           //  5 stfight_vid:fg_gfx
	{ "8.5c",		        0x8000, 0xe11ded31, 5 | BRF_GRA },           //  6
	{ "5.2c",		        0x8000, 0x0c099a31, 5 | BRF_GRA },           //  7
	{ "6.3c",		        0x8000, 0x3cc77c31, 5 | BRF_GRA },           //  8

	{ "13.4c",		        0x8000, 0x0ae48dd3, 6 | BRF_GRA },           //  9 stfight_vid:bg_gfx
	{ "14.5j",		        0x8000, 0xdebf5d76, 6 | BRF_GRA },           // 10
	{ "11.2j",		        0x8000, 0x8261ecfe, 6 | BRF_GRA },           // 11
	{ "12.3j",		        0x8000, 0x71137301, 6 | BRF_GRA },           // 12

	{ "20.8w",		        0x8000, 0x8299f247, 7 | BRF_GRA },           // 13 stfight_vid:spr_gfx
	{ "21.9w",		        0x8000, 0xb57dc037, 7 | BRF_GRA },           // 14
	{ "18.6w",		        0x8000, 0x68acd627, 7 | BRF_GRA },           // 15
	{ "19.7w",		        0x8000, 0x5170a057, 7 | BRF_GRA },           // 16

	{ "9.7c",		        0x8000, 0x8ceaf4fe, 8 | BRF_GRA },           // 17 stfight_vid:fg_map
	{ "10.8c",		        0x8000, 0x5a1a227a, 8 | BRF_GRA },           // 18

	{ "15.7j",		        0x8000, 0x27a310bc, 9 | BRF_GRA },           // 19 stfight_vid:bg_map
	{ "16.8j",		        0x8000, 0x3d19ce18, 9 | BRF_GRA },           // 20

	{ "82s129.006",		    0x0100, 0xf9424b5b, 10 | BRF_GRA },           // 21 stfight_vid:tx_clut

	{ "82s129.002",		    0x0100, 0xc883d49b, 11 | BRF_GRA },           // 22 stfight_vid:fg_clut
	{ "82s129.003",		    0x0100, 0xaf81882a, 11 | BRF_GRA },           // 23

	{ "82s129.004",		    0x0100, 0x1831ce7c, 12 | BRF_GRA },           // 24 stfight_vid:bg_clut
	{ "82s129.005",		    0x0100, 0x96cb6293, 12 | BRF_GRA },           // 25

	{ "82s129.052",		    0x0100, 0x3d915ffc, 13 | BRF_GRA },           // 26 stfight_vid:spr_clut
	{ "82s129.066",		    0x0100, 0x51e8832f, 13 | BRF_GRA },           // 27

	{ "82s129.015",		    0x0100, 0x0eaf5158, 14 | BRF_GRA },           // 28 proms

	{ "5j",			        0x8000, 0x1b8d0c07, 15 | BRF_SND },           // 29 adpcm
};

STD_ROM_PICK(stfightgb)
STD_ROM_FN(stfightgb)

struct BurnDriver BurnDrvStfightgb = {
	"stfightgb", "empcity", NULL, NULL, "1986",
	"Street Fight (Germany - Benelux)\0", NULL, "Seibu Kaihatsu (Tuning license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, stfightgbRomInfo, stfightgbRomName, NULL, NULL, NULL, NULL, StfightInputInfo, StfightDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};
