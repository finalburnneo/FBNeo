// FB Alpha Mat Mania driver module
// Based on MAME driver by Brad Oliver

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "m6809_intf.h"
#include "taito_m68705.h"
#include "ay8910.h"
#include "burn_ym3526.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvSndRAM;
static UINT8 *DrvMCURAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvVidRAM2;
static UINT8 *DrvColRAM0;
static UINT8 *DrvColRAM1;
static UINT8 *DrvColRAM2;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 pageselect;
static UINT8 scroll;
static UINT8 soundlatch;

static INT32 vblank;
static INT32 maniach;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo MatmaniaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Matmania)

static struct BurnDIPInfo MatmaniaDIPList[]=
{
	{0x11, 0xff, 0xff, 0x5f, NULL					},
	{0x12, 0xff, 0xff, 0xfe, NULL					},
	
	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x11, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x11, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x10, 0x00, "Off"					},
	{0x11, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x11, 0x01, 0x20, 0x00, "Upright"				},
	{0x11, 0x01, 0x20, 0x20, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x40, 0x40, "Off"					},
	{0x11, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x03, "Easy"					},
	{0x12, 0x01, 0x03, 0x02, "Medium"				},
	{0x12, 0x01, 0x03, 0x01, "Hard"					},
	{0x12, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Tournament Time"		},
	{0x12, 0x01, 0x0c, 0x00, "2:12"					},
	{0x12, 0x01, 0x0c, 0x04, "2:24"					},
	{0x12, 0x01, 0x0c, 0x08, "2:30"					},
	{0x12, 0x01, 0x0c, 0x0c, "2:36"					},
};

STDDIPINFO(Matmania)

static struct BurnDIPInfo ManiachDIPList[]=
{
	{0x11, 0xff, 0xff, 0x5f, NULL					},
	{0x12, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x11, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x11, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x10, 0x00, "Off"					},
	{0x11, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x11, 0x01, 0x20, 0x00, "Upright"				},
	{0x11, 0x01, 0x20, 0x20, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x40, 0x40, "Off"					},
	{0x11, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x03, 0x03, "Easy"					},
	{0x12, 0x01, 0x03, 0x02, "Medium"				},
	{0x12, 0x01, 0x03, 0x01, "Hard"					},
	{0x12, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Tournament Time"		},
	{0x12, 0x01, 0x0c, 0x00, "2:12"					},
	{0x12, 0x01, 0x0c, 0x04, "2:24"					},
	{0x12, 0x01, 0x0c, 0x08, "2:30"					},
	{0x12, 0x01, 0x0c, 0x0c, "2:36"					},
};

STDDIPINFO(Maniach)

static void matmania_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0x3050 && address <= 0x307f) {
		DrvPalRAM[address - 0x3050] = data;
		return;
	}

	switch (address)
	{
		case 0x3000:
			pageselect = data & 1;
		return;

		case 0x3010:
			soundlatch = data;
			if (maniach) {
				M6809Open(0);
				M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);
				M6809Close();
			} else {
				M6502Close();
				M6502Open(1);
				M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
				M6502Close();
				M6502Open(0);
			}
		return;

		case 0x3020:
			scroll = data;
		return;

		case 0x3030:
		return;	// nop

		case 0x3040:
			from_main = data;
			main_sent = 1;
		return;
	}
}

static UINT8 matmania_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			return DrvInputs[0];

		case 0x3010:
			return DrvInputs[1];

		case 0x3020:
			return DrvDips[1];

		case 0x3030:
			return (DrvDips[0] & 0x7f) | (vblank ? 0x80 : 0);

		case 0x3040:
			return standard_taito_mcu_read();

		case 0x3041:
			UINT8 res = 0;
			if (!mcu_sent) res |= 0x01;
			if (!main_sent) res |= 0x02;
			return res;
	}

	return 0;
}

static void matmania_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
			AY8910Write(0, 1, data);
		return;

		case 0x2001:
			AY8910Write(0, 0, data);
		return;

		case 0x2002:
			AY8910Write(1, 1, data);
		return;

		case 0x2003:
			AY8910Write(1, 0, data);
		return;

		case 0x2004:
			DACSignedWrite(0, data);
		return;
	}
}

static void maniach_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
		case 0x2001:
			BurnYM3526Write(address & 1, data);
		return;

		case 0x2002:
			DACSignedWrite(0, data);
		return;
	}
}

static UINT8 matmania_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x2004:
		case 0x2007:
			return soundlatch;
	}

	return 0;
}

inline static void DrvYM3526IRQHandler(INT32, INT32 nStatus)
{
	M6809SetIRQLine(1/*FIRQ*/, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static tilemap_callback( maniach_bg0 )
{
	offs ^= 0x1e0;
	INT32 attr = DrvColRAM0[offs];
	INT32 code = DrvVidRAM0[offs] + ((attr & 0x03) << 8);
	INT32 color = attr >> 4; // & 3
	INT32 flipy = offs & 0x10;

	TILE_SET_INFO(0, code, color, flipy ? TILE_FLIPY : 0);
}

static tilemap_callback( maniach_bg1 )
{
	offs ^= 0x1e0;
	INT32 attr = DrvColRAM2[offs];
	INT32 code = DrvVidRAM2[offs] + ((attr & 0x03) << 8);
	INT32 color = attr >> 4; // & 3
	INT32 flipy = offs & 0x10;

	TILE_SET_INFO(0, code, color, flipy ? TILE_FLIPY : 0);
}

static tilemap_callback( bg0 )
{
	offs ^= 0x1e0;
	INT32 attr = DrvColRAM0[offs];
	INT32 code = DrvVidRAM0[offs] + ((attr & 0x08) << 5);
	INT32 color = attr >> 4; // & 3
	INT32 flipy = offs & 0x10;

	TILE_SET_INFO(0, code, color, flipy ? TILE_FLIPY : 0);
}

static tilemap_callback( bg1 )
{
	offs ^= 0x1e0;
	INT32 attr = DrvColRAM2[offs];
	INT32 code = DrvVidRAM2[offs] + ((attr & 0x08) << 5);
	INT32 color = attr >> 4; // & 3
	INT32 flipy = offs & 0x10;

	TILE_SET_INFO(0, code, color, flipy ? TILE_FLIPY : 0);
}

static tilemap_callback( fg )
{
	offs ^= 0x3e0;
	INT32 attr = DrvColRAM1[offs];
	INT32 code = DrvVidRAM1[offs] + ((attr & 0x07) << 8);
	INT32 color = attr >> 4; // & 3

	TILE_SET_INFO(1, code, color, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	M6502Close();

	M6809Open(0);
	M6809Reset();
	BurnYM3526Reset();
	M6809Close();

	m67805_taito_reset();

	AY8910Reset(0);
	AY8910Reset(1);
	DACReset();

	pageselect = 0;
	scroll = 0;
	soundlatch = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x010000;
	DrvSndROM		= Next; Next += 0x010000;
	DrvMCUROM		= Next; Next += 0x000800;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x040000;
	DrvGfxROM2		= Next; Next += 0x100000;

	DrvColPROM		= Next; Next += 0x0000c0;

	DrvPalette		= (UINT32*)Next; Next += 0x0050 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x000800;
	DrvSndRAM		= Next; Next += 0x001000;
	DrvMCURAM		= Next; Next += 0x000800;
	DrvVidRAM0		= Next; Next += 0x000200;
	DrvVidRAM1		= Next; Next += 0x000400;
	DrvVidRAM2		= Next; Next += 0x000200;
	DrvColRAM0		= Next; Next += 0x000200;
	DrvColRAM1		= Next; Next += 0x000400;
	DrvColRAM2		= Next; Next += 0x000200;
	DrvPalRAM		= Next; Next += 0x000030;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3] = { 0x020000, 0x010000, 0 };
	INT32 Plane1[3] = { 0x080000, 0x040000, 0 };
	INT32 Plane2[3] = { 0x1c0000, 0x0e0000, 0 };
	INT32 XOffs[16] = { STEP8(128,1), STEP8(0,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x54000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x06000);

	GfxDecode(0x0400, 3,  8,  8, Plane0, XOffs + 8, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x18000);

	GfxDecode(0x0400, 3, 16, 16, Plane1, XOffs + 0, YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x54000);

	GfxDecode(0x0e00, 3, 16, 16, Plane2, XOffs + 0, YOffs, 0x100, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static void maniach_m68705_portB_out(UINT8 *data)
{
	if ((ddrB & 0x02) && (~*data & 0x02) && (portB_out & 0x02))
	{
		portA_in = from_main;
		main_sent = 0;
	}
	if ((ddrB & 0x04) && (*data & 0x04) && (~portB_out & 0x04))
	{
		from_mcu = portA_out;
		mcu_sent = 1;
	}
}

static m68705_interface maniach_m68705_interface = {
	NULL, maniach_m68705_portB_out, NULL,
	NULL, NULL, NULL,
	NULL, NULL, standard_m68705_portC_in
};

static INT32 DrvInit(INT32 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	maniach = game;

	if (game == 0) // matmania
	{
		if (BurnLoadRom(DrvMainROM + 0x04000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0c000,  2, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x08000,  3, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x0c000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0c000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x14000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x18000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x1c000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x24000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x28000, 21, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2c000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x30000, 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x34000, 24, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x38000, 25, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x3c000, 26, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40000, 27, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x44000, 28, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x48000, 29, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4c000, 30, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x50000, 31, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 32, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020, 33, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00080, 34, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000a0, 35, 1)) return 1;

		DrvGfxDecode();

		for (INT32 i = 0; i < 0x40; i++) {
			DrvColPROM[0x40 + i] = DrvColPROM[i] >> 4;
		}
	}
	else // maniach
	{
		if (BurnLoadRom(DrvMainROM + 0x04000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0c000,  2, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x04000,  3, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x08000,  4, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x0c000,  5, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM  + 0x00000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000, 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x08000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0c000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x14000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x18000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x1c000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 21, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x24000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x28000, 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2c000, 24, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x30000, 25, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x34000, 26, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x38000, 27, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x3c000, 28, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40000, 29, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x44000, 30, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x48000, 31, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4c000, 32, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x50000, 33, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 34, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020, 35, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00080, 36, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x000a0, 37, 1)) return 1;

		DrvGfxDecode();

		for (INT32 i = 0; i < 0x40; i++) {
			DrvColPROM[0x40 + i] = DrvColPROM[i] >> 4;
		}
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,			0x0000, 0x07ff, MAP_RAM); // spriteram = 0x780 - 0x7df
	M6502MapMemory(DrvVidRAM1,			0x1000, 0x13ff, MAP_RAM);
	M6502MapMemory(DrvColRAM1,			0x1400, 0x17ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM0,			0x2000, 0x21ff, MAP_RAM);
	M6502MapMemory(DrvColRAM0,			0x2200, 0x23ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM2,			0x2400, 0x25ff, MAP_RAM);
	M6502MapMemory(DrvColRAM2,			0x2600, 0x27ff, MAP_RAM);
	M6502MapMemory(DrvMainROM + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(matmania_main_write);
	M6502SetReadHandler(matmania_main_read);
	M6502Close();
	
	// matmania
	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvSndRAM,			0x0000, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvSndROM + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(matmania_sound_write);
	M6502SetReadHandler(matmania_sound_read);
	M6502Close();

	// matmania
	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.30, BURN_SND_ROUTE_BOTH);

	// maniach
	m67805_taito_init(DrvMCUROM, DrvMCURAM, &maniach_m68705_interface);

	// maniach
	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvSndRAM,			0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvSndROM + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(maniach_sound_write);
	M6809SetReadHandler(matmania_sound_read);
	M6809Close();

	// maniach
	BurnYM3526Init(3600000, DrvYM3526IRQHandler, 0);
	BurnTimerAttachYM3526(&M6809Config, 1500000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	// both
	DACInit(0, 0, 1, (maniach) ? M6809TotalCycles : M6502TotalCycles, (maniach) ? 1500000 : 1200000);
	DACSetRoute(0, 0.40, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, (maniach) ? maniach_bg0_map_callback : bg0_map_callback, 16, 16, 16, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, (maniach) ? maniach_bg1_map_callback : bg1_map_callback, 16, 16, 16, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_COLS, fg_map_callback,   8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 3, 16, 16, 0x20000 << maniach, 0x20, 3);
	GenericTilemapSetGfx(1, DrvGfxROM0, 3,  8,  8, 0x10000, 0x00, 3);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -8);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();
	M6809Exit();
	m67805_taito_exit();

	AY8910Exit(0);
	AY8910Exit(1);
	BurnYM3526Exit();
	DACExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit(UINT8 *source, INT32 length, INT32 base)
{
	for (INT32 i = 0; i < length; i++)
	{
		INT32 bit0 = (source[i] >> 0) & 1;
		INT32 bit1 = (source[i] >> 1) & 1;
		INT32 bit2 = (source[i] >> 2) & 1;
		INT32 bit3 = (source[i] >> 3) & 1;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (source[i + (length * 1)] >> 0) & 1;
		bit1 = (source[i + (length * 1)] >> 1) & 1;
		bit2 = (source[i + (length * 1)] >> 2) & 1;
		bit3 = (source[i + (length * 1)] >> 3) & 1;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (source[i + (length * 2)] >> 0) & 1;
		bit1 = (source[i + (length * 2)] >> 1) & 1;
		bit2 = (source[i + (length * 2)] >> 2) & 1;
		bit3 = (source[i + (length * 2)] >> 3) & 1;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i + base] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	UINT8 *spriteram = DrvMainRAM + 0x780;

	for (INT32 offs = 0; offs < 0x60; offs += 4)
	{
		if (spriteram[offs] & 0x01)
		{
			INT32 attr  = spriteram[offs + 0];
			INT32 code  = spriteram[offs + 1] + ((attr & 0xf0) << 4);
			if (code >= 0xe00) continue;
			INT32 sx    = 239 - spriteram[offs + 3];
			INT32 sy    = (240 - spriteram[offs + 2]) & 0xff;
			INT32 color = (attr >> 3) & 1;
			INT32 flipx = attr & 0x04;
			INT32 flipy = attr & 0x02;

			Draw16x16MaskTile(pTransDraw, code, sx, sy-8, flipx, flipy, color, 3, 0, 0x40, DrvGfxROM2);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit(DrvColPROM, 0x40, 0);
		DrvRecalc = 0;
	}

	DrvPaletteInit(DrvPalRAM, 0x10, 0x40);

	if (nBurnLayer & 1)
	{
		GenericTilemapSetScrollY(pageselect, scroll);
		GenericTilemapDraw(pageselect, pTransDraw, 0);
	}
	else
	{
		BurnTransferClear();
	}

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(2, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();
	M6809NewFrame();

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 1500000 / 60, 1200000 / 60, 3000000 / 4 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };
	INT32 nSoundBufferPos = 0;

	vblank = 1;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 7) vblank = 0;
		M6502Open(0);
		nCyclesDone[0] += M6502Run(nCyclesTotal[0] / nInterleave);
		if (i == nInterleave-1) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
			vblank = 1;
		}
		M6502Close();

		if (maniach)
		{
			M6809Open(0);
			BurnTimerUpdateYM3526((i + 1) * (nCyclesTotal[0] / nInterleave));

			M6809Close();

			m6805Open(0);
			nCyclesDone[2] += m6805Run(nCyclesTotal[2] / nInterleave);
			m6805Close();
		}
		else
		{
			M6502Open(1);
			nCyclesDone[1] += M6502Run(nCyclesTotal[1] / nInterleave);
			if ((i % 17) == 0) M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
			M6502Close();

			// Render Sound Segment
			if (pBurnSoundOut && (i%8)==7) {
				INT32 nSegmentLength = nBurnSoundLen / (nInterleave/8);
				INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
				AY8910Render(pSoundBuf, nSegmentLength);
				nSoundBufferPos += nSegmentLength;
			}
		}
	}
	if (maniach) {
		M6809Open(0);
		BurnTimerEndFrameYM3526(nCyclesTotal[0]);
		M6809Close();
	}

	if (pBurnSoundOut) {
		if (maniach) {
			M6809Open(0);
			BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
			DACUpdate(pBurnSoundOut, nBurnSoundLen);
			M6809Close();
		} else {
			// Make sure the buffer is entirely filled.
			if (pBurnSoundOut) {
				INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
				INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
				if (nSegmentLength) {
					AY8910Render(pSoundBuf, nSegmentLength);
				}
			}
			DACUpdate(pBurnSoundOut, nBurnSoundLen);
		}
	}

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

		M6502Scan(nAction);
		M6809Scan(nAction);
		m68705_taito_scan(nAction);
		AY8910Scan(nAction, pnMin);
		DACScan(nAction,pnMin);
		BurnYM3526Scan(nAction,pnMin);

		SCAN_VAR(pageselect);
		SCAN_VAR(scroll);
		SCAN_VAR(soundlatch);
	}

	return 0;
}


// Mat Mania

static struct BurnRomInfo matmaniaRomDesc[] = {
	{ "k0-03",			0x4000, 0x314ab8a4, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "k1-03",			0x4000, 0x3b3c3f08, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "k2-03",			0x4000, 0x286c0917, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "k4-0",			0x4000, 0x86dab489, 2 | BRF_PRG | BRF_ESS }, //  3 M6502 #1 Code
	{ "k5-0",			0x4000, 0x4c41cdba, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "ku-02",			0x2000, 0x613c8698, 3 | BRF_GRA },           //  5 Characters
	{ "kv-02",			0x2000, 0x274ce14b, 3 | BRF_GRA },           //  6
	{ "kw-02",			0x2000, 0x7588a9c4, 3 | BRF_GRA },           //  7

	{ "kt-02",			0x4000, 0x5d817c70, 4 | BRF_GRA },           //  8 Tiles
	{ "ks-02",			0x4000, 0x2e9f3ba0, 4 | BRF_GRA },           //  9
	{ "kr-02",			0x4000, 0xb057d3e3, 4 | BRF_GRA },           // 10

	{ "k6-00",			0x4000, 0x294d0878, 5 | BRF_GRA },           // 11 Sprites
	{ "k7-00",			0x4000, 0x0908c2f5, 5 | BRF_GRA },           // 12
	{ "k8-00",			0x4000, 0xae8341e1, 5 | BRF_GRA },           // 13
	{ "k9-00",			0x4000, 0x752ac2c6, 5 | BRF_GRA },           // 14
	{ "ka-00",			0x4000, 0x46a9cb16, 5 | BRF_GRA },           // 15
	{ "kb-00",			0x4000, 0xbf016772, 5 | BRF_GRA },           // 16
	{ "kc-00",			0x4000, 0x8d08bce7, 5 | BRF_GRA },           // 17
	{ "kd-00",			0x4000, 0xaf1d6a60, 5 | BRF_GRA },           // 18
	{ "ke-00",			0x4000, 0x614f19b0, 5 | BRF_GRA },           // 19
	{ "kf-00",			0x4000, 0xbdf58c18, 5 | BRF_GRA },           // 20
	{ "kg-00",			0x4000, 0x2189f5cf, 5 | BRF_GRA },           // 21
	{ "kh-00",			0x4000, 0x6b11ed1f, 5 | BRF_GRA },           // 22
	{ "ki-00",			0x4000, 0xd7ac4ec5, 5 | BRF_GRA },           // 23
	{ "kj-00",			0x4000, 0x2caee05d, 5 | BRF_GRA },           // 24
	{ "kk-00",			0x4000, 0xeb54f010, 5 | BRF_GRA },           // 25
	{ "kl-00",			0x4000, 0xfa4c7e0c, 5 | BRF_GRA },           // 26
	{ "km-00",			0x4000, 0x6d2369b6, 5 | BRF_GRA },           // 27
	{ "kn-00",			0x4000, 0xc55733e2, 5 | BRF_GRA },           // 28
	{ "ko-00",			0x4000, 0xed3c3476, 5 | BRF_GRA },           // 29
	{ "kp-00",			0x4000, 0x9c84a969, 5 | BRF_GRA },           // 30
	{ "kq-00",			0x4000, 0xfa2f0003, 5 | BRF_GRA },           // 31

	{ "matmania.1",		0x0020, 0x1b58f01f, 6 | BRF_GRA },           // 32 Color Data
	{ "matmania.5",		0x0020, 0x2029f85f, 6 | BRF_GRA },           // 33
	{ "matmania.2",		0x0020, 0xb6ac1fd5, 6 | BRF_GRA },           // 34
	{ "matmania.16",	0x0020, 0x09325dc2, 6 | BRF_GRA },           // 35
};

STD_ROM_PICK(matmania)
STD_ROM_FN(matmania)

static INT32 MatmaniaInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvMatmania = {
	"matmania", NULL, NULL, NULL, "1985",
	"Mat Mania\0", NULL, "Technos Japan (Taito America license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VSFIGHT, 0,
	NULL, matmaniaRomInfo, matmaniaRomName, NULL, NULL, NULL, NULL, MatmaniaInputInfo, MatmaniaDIPInfo,
	MatmaniaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x50,
	240, 256, 3, 4
};


// Exciting Hour

static struct BurnRomInfo excthourRomDesc[] = {
	{ "e29",			0x4000, 0xc453e855, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "e28",			0x4000, 0x17b63708, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "e27",			0x4000, 0x269ab3bc, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "k4-0",			0x4000, 0x86dab489, 2 | BRF_PRG | BRF_ESS }, //  3 M6502 #1 Code
	{ "k5-0",			0x4000, 0x4c41cdba, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "e30",			0x2000, 0xb2875329, 3 | BRF_GRA },           //  5 Characters
	{ "e31",			0x2000, 0xc9506de8, 3 | BRF_GRA },           //  6
	{ "e32",			0x2000, 0x00d1635f, 3 | BRF_GRA },           //  7

	{ "e5",				0x4000, 0x0604dc55, 4 | BRF_GRA },           //  8 Tiles
	{ "ks-02",			0x4000, 0x2e9f3ba0, 4 | BRF_GRA },           //  9
	{ "e3",				0x4000, 0xebd273c6, 4 | BRF_GRA },           // 10

	{ "k6-00",			0x4000, 0x294d0878, 5 | BRF_GRA },           // 11 Sprites
	{ "k7-00",			0x4000, 0x0908c2f5, 5 | BRF_GRA },           // 12
	{ "k8-00",			0x4000, 0xae8341e1, 5 | BRF_GRA },           // 13
	{ "k9-00",			0x4000, 0x752ac2c6, 5 | BRF_GRA },           // 14
	{ "ka-00",			0x4000, 0x46a9cb16, 5 | BRF_GRA },           // 15
	{ "kb-00",			0x4000, 0xbf016772, 5 | BRF_GRA },           // 16
	{ "kc-00",			0x4000, 0x8d08bce7, 5 | BRF_GRA },           // 17
	{ "kd-00",			0x4000, 0xaf1d6a60, 5 | BRF_GRA },           // 18
	{ "ke-00",			0x4000, 0x614f19b0, 5 | BRF_GRA },           // 19
	{ "kf-00",			0x4000, 0xbdf58c18, 5 | BRF_GRA },           // 20
	{ "kg-00",			0x4000, 0x2189f5cf, 5 | BRF_GRA },           // 21
	{ "kh-00",			0x4000, 0x6b11ed1f, 5 | BRF_GRA },           // 22
	{ "ki-00",			0x4000, 0xd7ac4ec5, 5 | BRF_GRA },           // 23
	{ "kj-00",			0x4000, 0x2caee05d, 5 | BRF_GRA },           // 24
	{ "kk-00",			0x4000, 0xeb54f010, 5 | BRF_GRA },           // 25
	{ "kl-00",			0x4000, 0xfa4c7e0c, 5 | BRF_GRA },           // 26
	{ "km-00",			0x4000, 0x6d2369b6, 5 | BRF_GRA },           // 27
	{ "kn-00",			0x4000, 0xc55733e2, 5 | BRF_GRA },           // 28
	{ "ko-00",			0x4000, 0xed3c3476, 5 | BRF_GRA },           // 29
	{ "kp-00",			0x4000, 0x9c84a969, 5 | BRF_GRA },           // 30
	{ "kq-00",			0x4000, 0xfa2f0003, 5 | BRF_GRA },           // 31

	{ "matmania.1",		0x0020, 0x1b58f01f, 6 | BRF_GRA },           // 32 Color Data
	{ "matmania.5",		0x0020, 0x2029f85f, 6 | BRF_GRA },           // 33
	{ "matmania.2",		0x0020, 0xb6ac1fd5, 6 | BRF_GRA },           // 34
	{ "matmania.16",	0x0020, 0x09325dc2, 6 | BRF_GRA },           // 35
};

STD_ROM_PICK(excthour)
STD_ROM_FN(excthour)

struct BurnDriver BurnDrvExcthour = {
	"excthour", "matmania", NULL, NULL, "1985",
	"Exciting Hour\0", NULL, "Technos Japan (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VSFIGHT, 0,
	NULL, excthourRomInfo, excthourRomName, NULL, NULL, NULL, NULL, MatmaniaInputInfo, ManiachDIPInfo,
	MatmaniaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x50,
	240, 256, 3, 4
};


// Mania Challenge (set 1)

static struct BurnRomInfo maniachRomDesc[] = {
	{ "mc-mb2.bin",		0x4000, 0xa6da1ba8, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "mc-ma2.bin",		0x4000, 0x84583323, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mc-m92.bin",		0x4000, 0xe209a500, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mc-m50.bin",		0x4000, 0xba415d68, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #0 Code
	{ "mc-m40.bin",		0x4000, 0x2a217ed0, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mc-m30.bin",		0x4000, 0x95af1723, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "01",				0x0800, 0x00c7f80c, 3 | BRF_GRA },           //  6 M68705 #0 Code

	{ "mc-m60.bin",		0x2000, 0x1cdbb117, 4 | BRF_GRA },           //  7 Characters
	{ "mc-m70.bin",		0x2000, 0x553f0780, 4 | BRF_GRA },           //  8
	{ "mc-m80.bin",		0x2000, 0x9392ecb7, 4 | BRF_GRA },           //  9

	{ "mc-m01.bin",		0x8000, 0xda558e4d, 5 | BRF_GRA },           // 10 Tiles
	{ "mc-m10.bin",		0x8000, 0x619a02f8, 5 | BRF_GRA },           // 11
	{ "mc-m20.bin",		0x8000, 0xa617c6c1, 5 | BRF_GRA },           // 12
	
	{ "mc-mq0.bin",		0x4000, 0x102a1666, 6 | BRF_GRA },           // 13 Sprites
	{ "mc-mr0.bin",		0x4000, 0x1e854453, 6 | BRF_GRA },           // 14
	{ "mc-ms0.bin",		0x4000, 0x7bc9d878, 6 | BRF_GRA },           // 15
	{ "mc-mt0.bin",		0x4000, 0x09cea985, 6 | BRF_GRA },           // 16
	{ "mc-mu0.bin",		0x4000, 0x5421769e, 6 | BRF_GRA },           // 17
	{ "mc-mv0.bin",		0x4000, 0x36fc3e2d, 6 | BRF_GRA },           // 18
	{ "mc-mw0.bin",		0x4000, 0x135dce4c, 6 | BRF_GRA },           // 19
	{ "mc-mj0.bin",		0x4000, 0x7e73895b, 6 | BRF_GRA },           // 20
	{ "mc-mk0.bin",		0x4000, 0x66c8bf75, 6 | BRF_GRA },           // 21
	{ "mc-ml0.bin",		0x4000, 0x88138a1d, 6 | BRF_GRA },           // 22
	{ "mc-mm0.bin",		0x4000, 0xa1a4260d, 6 | BRF_GRA },           // 23
	{ "mc-mn0.bin",		0x4000, 0x6bc61b58, 6 | BRF_GRA },           // 24
	{ "mc-mo0.bin",		0x4000, 0xf96ef600, 6 | BRF_GRA },           // 25
	{ "mc-mp0.bin",		0x4000, 0x1259618e, 6 | BRF_GRA },           // 26
	{ "mc-mc0.bin",		0x4000, 0x133d644f, 6 | BRF_GRA },           // 27
	{ "mc-md0.bin",		0x4000, 0xe387b036, 6 | BRF_GRA },           // 28
	{ "mc-me0.bin",		0x4000, 0xb36b1283, 6 | BRF_GRA },           // 29
	{ "mc-mf0.bin",		0x4000, 0x2584d8a9, 6 | BRF_GRA },           // 30
	{ "mc-mg0.bin",		0x4000, 0xcf31a714, 6 | BRF_GRA },           // 31
	{ "mc-mh0.bin",		0x4000, 0x6292d589, 6 | BRF_GRA },           // 32
	{ "mc-mi0.bin",		0x4000, 0xee2e06e3, 6 | BRF_GRA },           // 33

	{ "prom.2",			0x0020, 0x32db2cf4, 7 | BRF_GRA },           // 34 Color Data
	{ "prom.16",		0x0020, 0x18836d26, 7 | BRF_GRA },           // 35
	{ "prom.3",			0x0020, 0xc7925311, 7 | BRF_GRA },           // 36
	{ "prom.17",		0x0020, 0x41f51d49, 7 | BRF_GRA },           // 37

	{ "pal10l8.51",		0x002c, 0x424547af, 8 | BRF_OPT },           // 38 PLDs
	{ "pal10l8.56",		0x002c, 0x5f6fdf22, 8 | BRF_OPT },           // 39
	{ "pal16r4a.117",	0x0104, 0x76640daa, 8 | BRF_OPT },           // 40
	{ "pal16r4a.118",	0x0104, 0xbca7cae2, 8 | BRF_OPT },           // 41
};

STD_ROM_PICK(maniach)
STD_ROM_FN(maniach)

static INT32 ManiachInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvManiach = {
	"maniach", NULL, NULL, NULL, "1986",
	"Mania Challenge (set 1)\0", NULL, "Technos Japan (Taito America license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VSFIGHT, 0,
	NULL, maniachRomInfo, maniachRomName, NULL, NULL, NULL, NULL, MatmaniaInputInfo, ManiachDIPInfo,
	ManiachInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x50,
	240, 256, 3, 4
};


// Mania Challenge (set 2)

static struct BurnRomInfo maniach2RomDesc[] = {
	{ "ic40-mb1",		0x4000, 0xb337a867, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "ic41-ma1",		0x4000, 0x85ec8279, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic42-m91",		0x4000, 0xa14b86dd, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mc-m50.bin",		0x4000, 0xba415d68, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #0 Code
	{ "mc-m40.bin",		0x4000, 0x2a217ed0, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mc-m30.bin",		0x4000, 0x95af1723, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "01",				0x0800, 0x00c7f80c, 3 | BRF_GRA },           //  6 M68705 #0 Code

	{ "mc-m60.bin",		0x2000, 0x1cdbb117, 4 | BRF_GRA },           //  7 Characters
	{ "mc-m70.bin",		0x2000, 0x553f0780, 4 | BRF_GRA },           //  8
	{ "mc-m80.bin",		0x2000, 0x9392ecb7, 4 | BRF_GRA },           //  9

	{ "mc-m01.bin",		0x8000, 0xda558e4d, 5 | BRF_GRA },           // 10 Tiles
	{ "mc-m10.bin",		0x8000, 0x619a02f8, 5 | BRF_GRA },           // 11
	{ "mc-m20.bin",		0x8000, 0xa617c6c1, 5 | BRF_GRA },           // 12

	{ "mc-mq0.bin",		0x4000, 0x102a1666, 6 | BRF_GRA },           // 13 Sprites
	{ "mc-mr0.bin",		0x4000, 0x1e854453, 6 | BRF_GRA },           // 14
	{ "mc-ms0.bin",		0x4000, 0x7bc9d878, 6 | BRF_GRA },           // 15
	{ "mc-mt0.bin",		0x4000, 0x09cea985, 6 | BRF_GRA },           // 16
	{ "mc-mu0.bin",		0x4000, 0x5421769e, 6 | BRF_GRA },           // 17
	{ "mc-mv0.bin",		0x4000, 0x36fc3e2d, 6 | BRF_GRA },           // 18
	{ "mc-mw0.bin",		0x4000, 0x135dce4c, 6 | BRF_GRA },           // 19
	{ "mc-mj0.bin",		0x4000, 0x7e73895b, 6 | BRF_GRA },           // 20
	{ "mc-mk0.bin",		0x4000, 0x66c8bf75, 6 | BRF_GRA },           // 21
	{ "mc-ml0.bin",		0x4000, 0x88138a1d, 6 | BRF_GRA },           // 22
	{ "mc-mm0.bin",		0x4000, 0xa1a4260d, 6 | BRF_GRA },           // 23
	{ "mc-mn0.bin",		0x4000, 0x6bc61b58, 6 | BRF_GRA },           // 24
	{ "mc-mo0.bin",		0x4000, 0xf96ef600, 6 | BRF_GRA },           // 25
	{ "mc-mp0.bin",		0x4000, 0x1259618e, 6 | BRF_GRA },           // 26
	{ "mc-mc0.bin",		0x4000, 0x133d644f, 6 | BRF_GRA },           // 27
	{ "mc-md0.bin",		0x4000, 0xe387b036, 6 | BRF_GRA },           // 28
	{ "mc-me0.bin",		0x4000, 0xb36b1283, 6 | BRF_GRA },           // 29
	{ "mc-mf0.bin",		0x4000, 0x2584d8a9, 6 | BRF_GRA },           // 30
	{ "mc-mg0.bin",		0x4000, 0xcf31a714, 6 | BRF_GRA },           // 31
	{ "mc-mh0.bin",		0x4000, 0x6292d589, 6 | BRF_GRA },           // 32
	{ "mc-mi0.bin",		0x4000, 0xee2e06e3, 6 | BRF_GRA },           // 33

	{ "prom.2",			0x0020, 0x32db2cf4, 7 | BRF_GRA },           // 34 Color Data
	{ "prom.16",		0x0020, 0x18836d26, 7 | BRF_GRA },           // 35
	{ "prom.3",			0x0020, 0xc7925311, 7 | BRF_GRA },           // 36
	{ "prom.17",		0x0020, 0x41f51d49, 7 | BRF_GRA },           // 37

	{ "pal10l8.51",		0x002c, 0x424547af, 8 | BRF_OPT },           // 38 PLDs
	{ "pal10l8.56",		0x002c, 0x5f6fdf22, 8 | BRF_OPT },           // 39
	{ "pal16r4a.117",	0x0104, 0x76640daa, 8 | BRF_OPT },           // 40
	{ "pal16r4a.118",	0x0104, 0xbca7cae2, 8 | BRF_OPT },           // 41
};

STD_ROM_PICK(maniach2)
STD_ROM_FN(maniach2)

struct BurnDriver BurnDrvManiach2 = {
	"maniach2", "maniach", NULL, NULL, "1986",
	"Mania Challenge (set 2)\0", NULL, "Technos Japan (Taito America license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VSFIGHT, 0,
	NULL, maniach2RomInfo, maniach2RomName, NULL, NULL, NULL, NULL, MatmaniaInputInfo, ManiachDIPInfo,
	ManiachInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x50,
	240, 256, 3, 4
};
