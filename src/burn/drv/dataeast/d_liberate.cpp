// FB Alpha Data East Liberation driver module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "ay8910.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSoundROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvScrRAM;
static UINT8 *DrvSoundRAM;
static UINT8 *DrvIORAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 background_color;
static INT32 background_disable;
static INT32 flipscreen;
static INT32 soundlatch;
static INT32 irq_latch;
static INT32 input_bank;

static UINT8 *vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static struct BurnInputInfo LiberateInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy4 + 1,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Liberate)

static struct BurnInputInfo KamikcabInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Kamikcab)

static struct BurnDIPInfo LiberateDIPList[]=
{
	{0x13, 0xff, 0xff, 0x8f, NULL										},
	{0x14, 0xff, 0xff, 0x3f, NULL										},

	{0   , 0xfe, 0   ,    4, "Coin A"									},
	{0x13, 0x01, 0x03, 0x00, "2 Coins 1 Credits"						},
	{0x13, 0x01, 0x03, 0x03, "1 Coin  1 Credits"						},
	{0x13, 0x01, 0x03, 0x02, "1 Coin  2 Credits"						},
	{0x13, 0x01, 0x03, 0x01, "1 Coin  3 Credits"						},

	{0   , 0xfe, 0   ,    4, "Coin B"									},
	{0x13, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"						},
	{0x13, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"						},
	{0x13, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"						},
	{0x13, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"									},
	{0x13, 0x01, 0x40, 0x00, "Upright"									},
	{0x13, 0x01, 0x40, 0x40, "Cocktail"									},

	{0   , 0xfe, 0   ,    1, "Manufacturer"								},
//	{0x13, 0x01, 0x80, 0x00, "(INVALID) Data East USA (Dual Assault)"	},
	{0x13, 0x01, 0x80, 0x80, "Data East Corporation (Liberation)"		},

	{0   , 0xfe, 0   ,    4, "Lives"									},
	{0x14, 0x01, 0x03, 0x01, "5"										},
	{0x14, 0x01, 0x03, 0x02, "4"										},
	{0x14, 0x01, 0x03, 0x03, "3"										},
	{0x14, 0x01, 0x03, 0x00, "Infinite"									},

	{0   , 0xfe, 0   ,    4, "Bonus"									},
	{0x14, 0x01, 0x0c, 0x00, "20000 only"								},
	{0x14, 0x01, 0x0c, 0x0c, "20000, every 40000 after"					},
	{0x14, 0x01, 0x0c, 0x08, "30000, every 50000 after"					},
	{0x14, 0x01, 0x0c, 0x04, "50000, every 70000 after"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"								},
	{0x14, 0x01, 0x30, 0x30, "Easy"										},
	{0x14, 0x01, 0x30, 0x20, "Normal"									},
	{0x14, 0x01, 0x30, 0x10, "Hard"										},
	{0x14, 0x01, 0x30, 0x00, "Hardest"									},
};

STDDIPINFO(Liberate)

static struct BurnDIPInfo BoomrangDIPList[]=
{
	{0x13, 0xff, 0xff, 0x0f, NULL										},
	{0x14, 0xff, 0xff, 0xbf, NULL										},

	{0   , 0xfe, 0   ,    4, "Coin A"									},
	{0x13, 0x01, 0x03, 0x00, "2 Coins 1 Credits"						},
	{0x13, 0x01, 0x03, 0x03, "1 Coin  1 Credits"						},
	{0x13, 0x01, 0x03, 0x02, "1 Coin  2 Credits"						},
	{0x13, 0x01, 0x03, 0x01, "1 Coin  3 Credits"						},

	{0   , 0xfe, 0   ,    4, "Coin B"									},
	{0x13, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"						},
	{0x13, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"						},
	{0x13, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"						},
	{0x13, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"									},
	{0x13, 0x01, 0x40, 0x00, "Upright"									},
	{0x13, 0x01, 0x40, 0x40, "Cocktail"									},

	{0   , 0xfe, 0   ,    2, "Manufacturer"								},
	{0x13, 0x01, 0x80, 0x00, "Data East USA"							},
	{0x13, 0x01, 0x80, 0x80, "Data East Corporation"					},

	{0   , 0xfe, 0   ,    4, "Lives"									},
	{0x14, 0x01, 0x03, 0x01, "5"										},
	{0x14, 0x01, 0x03, 0x02, "4"										},
	{0x14, 0x01, 0x03, 0x03, "3"										},
	{0x14, 0x01, 0x03, 0x00, "Infinite"									},

	{0   , 0xfe, 0   ,    4, "Bonus"									},
	{0x14, 0x01, 0x0c, 0x00, "20000 only"								},
	{0x14, 0x01, 0x0c, 0x0c, "20000, every 40000 after"					},
	{0x14, 0x01, 0x0c, 0x08, "30000, every 50000 after"					},
	{0x14, 0x01, 0x0c, 0x04, "50000, every 70000 after"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"								},
	{0x14, 0x01, 0x30, 0x30, "Easy"										},
	{0x14, 0x01, 0x30, 0x20, "Normal"									},
	{0x14, 0x01, 0x30, 0x10, "Hard"										},
	{0x14, 0x01, 0x30, 0x00, "Hardest"									},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"								},
	{0x14, 0x01, 0x40, 0x40, "Off"										},
	{0x14, 0x01, 0x40, 0x00, "On"									    },

	{0   , 0xfe, 0   ,    2, "Invincibility"							},
	{0x14, 0x01, 0x80, 0x80, "Off"										},
	{0x14, 0x01, 0x80, 0x00, "On"									    },
};

STDDIPINFO(Boomrang)

static struct BurnDIPInfo KamikcabDIPList[]=
{
	{0x12, 0xff, 0xff, 0x0f, NULL										},
	{0x13, 0xff, 0xff, 0xbf, NULL										},

	{0   , 0xfe, 0   ,    4, "Coin A"									},
	{0x12, 0x01, 0x03, 0x00, "2 Coins 1 Credits"						},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"						},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"						},
	{0x12, 0x01, 0x03, 0x01, "1 Coin  3 Credits"						},

	{0   , 0xfe, 0   ,    4, "Coin B"									},
	{0x12, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"						},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"						},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"						},
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"									},
	{0x12, 0x01, 0x40, 0x00, "Upright"									},
	{0x12, 0x01, 0x40, 0x40, "Cocktail"									},

	{0   , 0xfe, 0   ,    2, "Manufacturer"								},
	{0x12, 0x01, 0x80, 0x00, "Data East USA"							},
	{0x12, 0x01, 0x80, 0x80, "Data East Corporation"					},

	{0   , 0xfe, 0   ,    4, "Lives"									},
	{0x13, 0x01, 0x03, 0x01, "5"										},
	{0x13, 0x01, 0x03, 0x02, "2"										},
	{0x13, 0x01, 0x03, 0x03, "3"										},
	{0x13, 0x01, 0x03, 0x00, "Infinite"									},

	{0   , 0xfe, 0   ,    4, "Bonus"									},
	{0x13, 0x01, 0x0c, 0x00, "20000 only"								},
	{0x13, 0x01, 0x0c, 0x0c, "20000, every 30000 after"					},
	{0x13, 0x01, 0x0c, 0x08, "30000, every 40000 after"					},
	{0x13, 0x01, 0x0c, 0x04, "40000, every 50000 after"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"								},
	{0x13, 0x01, 0x30, 0x30, "Easy"										},
	{0x13, 0x01, 0x30, 0x20, "Normal"									},
	{0x13, 0x01, 0x30, 0x10, "Hard"										},
	{0x13, 0x01, 0x30, 0x00, "Hardest"									},

	{0   , 0xfe, 0   ,    2, "Invincibility (Cheat)"					},
	{0x13, 0x01, 0x80, 0x80, "Off"										},
	{0x13, 0x01, 0x80, 0x00, "On"										},
};

STDDIPINFO(Kamikcab)

static struct BurnDIPInfo DualasltDIPList[] = {
	{0x13, 0xFF, 0xFF, 0x0f, NULL										},

	{0   , 0xfe, 0   ,    2, "Manufacturer"								},
	{0x13, 0x01, 0x80, 0x00, "Data East USA (Dual Assault)"	},
//	{0x13, 0x01, 0x80, 0x80, "(INVALID) Data East Corporation (Liberation)"		},
};

STDDIPINFOEXT(Dualaslt, Liberate, Dualaslt )

static void bankswitch(INT32 data)
{
	input_bank = data;

	if (input_bank) {
		M6502MapMemory(DrvMainROM + 0x4000,		0x4000, 0x7fff, MAP_ROM);
		M6502MapMemory(DrvMainROM + 0x0000,		0x8000, 0x80ff, MAP_ROM);
	} else {
		M6502MapMemory(NULL,					0x4000, 0x7fff, MAP_RAM);
		M6502MapMemory(DrvColRAM,				0x4000, 0x43ff, MAP_RAM);
		M6502MapMemory(DrvVidRAM,				0x4400, 0x47ff, MAP_RAM);
		M6502MapMemory(DrvSprRAM,				0x4800, 0x4fff, MAP_RAM);
		M6502MapMemory(DrvScrRAM,				0x6200, 0x67ff, MAP_RAM);
		M6502MapMemory(DrvMainROM + 0x8000,		0x8000, 0x80ff, MAP_ROM);
	}
}

static void liberate_main_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("MW: %4.4x, %2.2x\n"), address, data);

	if ((address & 0xfff0) == 0x8000)
	{
		address &= 0xf;
		DrvIORAM[address] = data;

		switch (address)
		{
			case 6:
				background_color = (data >> 4) & 3;
				background_disable = data & 4;
				flipscreen = data & 1;
			return;

			case 8:
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return;

			case 9:
				soundlatch = data;
				M6502SetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
			return;
		}
	}
}

static UINT8 liberate_main_read(UINT16 address)
{
//	bprintf (0, _T("MR: %4.4x\n"), address);

	if ((address & 0xfff0) == 0x8000 && input_bank)
	{
		address &= 0xf;

		if (address == 0) return DrvInputs[0]; /* Player 1 controls */
		if (address == 1) return DrvInputs[1]; /* Player 2 controls */
		if (address == 2) return (DrvInputs[2] & 0x7f) | (*vblank & 0x80); /* Vblank, coins */
		if (address == 3) return DrvDips[0]; /* Dip 1 */
		if (address == 4) return DrvDips[1]; /* Dip 2 */

		return 0xff;
	}

	if ((address & 0xff00) == 0x8000) {
		return DrvMainROM[address];
	}

	return 0;
}

static void liberate_main_write_port(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("PW: %4.4x, %2.2x\n"), address, data);
	switch (address)
	{
		case 0x00:
			bankswitch(data);
		return;
	}
}

static UINT8 liberate_main_read_port(UINT16 address)
{
//	bprintf (0, _T("PR: %4.4x\n"), address);

	switch (address)
	{
		case 0x00:
			return *vblank;

		case 0x01:
			return DrvInputs[3];
	}

	return 0;
}

static void liberate_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x1000:
		return; // nop

		case 0x3000:
			AY8910Write(0, 1, data);
		return;

		case 0x4000:
			AY8910Write(0, 0, data);
		return;

		case 0x7000:
			AY8910Write(1, 1, data);
		return;

		case 0x8000:
			AY8910Write(1, 0, data);
		return;
	}
}

static UINT8 liberate_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xb000:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static tilemap_scan( bg )
{
	return ((row & 0xf)) + ((15 - (col & 0xf)) << 4) + ((row & 0x10) << 5) + ((col & 0x10) << 4);
}

static tilemap_scan( tx )
{
	return (row & 0x1f) + ((31 - (col & 0x1f)) << 5);
}

static tilemap_callback( bg )
{
	offs = (offs & 0xff) + (DrvIORAM[2 + ((offs & 0x100) / 0x80) + ((offs & 0x200) / 0x200)] << 8);

	TILE_SET_INFO(0, DrvMainROM[0x4000 + offs], background_color, 0);
}

static tilemap_callback( tx )
{
	INT32 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] + (attr << 8);

	TILE_SET_INFO(1, code, attr >> 4, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	memcpy (DrvMainROM, DrvMainROM + 0x8000, 0x100);
	memset (DrvMainROM, 0xff, 0x10);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	M6502Close();

	AY8910Reset(0);
	AY8910Reset(1);

	background_color = 0;
	background_disable = 0;
	flipscreen = 0;
	soundlatch = 0;
	irq_latch = 0;
	input_bank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x010000;
	DrvSoundROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x030000;
	DrvGfxROM1		= Next; Next += 0x030000;
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000020;

	DrvPalette		= (UINT32*)Next; Next += 0x0021 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x001000;
	DrvColRAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvScrRAM		= Next; Next += 0x000600;
	DrvSoundRAM		= Next; Next += 0x000200;
	DrvIORAM		= Next; Next += 0x000010;

	vblank			= Next; Next += 0x000008; // 1 byte

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3]  = { 0x6000*8*2, 0x6000*8*1, 0x6000*8*0 };
	INT32 XOffs0[16] = { STEP8(128,1), STEP8(0,1) };
	INT32 YOffs0[16] = { STEP16(0,8) };

	INT32 Plane1[3]  = { 4, 0, 0x4000*8+4 };
	INT32 Plane2[3]  = { 0x2000*8+4, 0x2000*8+0, 0x4000*8 };
	INT32 XOffs1[16] = { STEP4(24,1), STEP4(16,1), STEP4(8,1), STEP4(0,1) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x12000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x12000);

	GfxDecode(0x0C00, 3,  8,  8, Plane0, XOffs0 + 8, YOffs0, 0x040, tmp, DrvGfxROM0);
	GfxDecode(0x0300, 3, 16, 16, Plane0, XOffs0 + 0, YOffs0, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x08000);

	GfxDecode(0x0080, 3, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM2 + 0x0000);
	GfxDecode(0x0080, 3, 16, 16, Plane2, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM2 + 0x8000);

	BurnFree(tmp);

	return 0;
}

static void DrvLiberateDecode()
{
	UINT8 tmp[0x100];

	for (INT32 A = 0; A < 0x100; A++) {
		tmp[A] = BITSWAP08(A, 1, 2, 3, 4, 5, 6, 7, 0);
	}

	M6502Open(0);
	M6502SetOpcodeDecode(tmp);
	M6502Close();
}

static void DrvNibbleSwap()
{
	UINT8 *rom = DrvMainROM;

	for (INT32 i = 0; i < 0x10000; i++) {
		if (i >= 0x4000 && i < 0x8000) continue;

		rom[i] = (rom[i] << 4) | (rom[i] >> 4);
	}
}

static void DrvCommonInit(INT32 gfxsize, INT32 bgsplit)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return;
	memset(AllMem, 0, nLen);
	MemIndex();

	M6502Init(0, TYPE_DECO16);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,				0x0000, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvMainROM + 0x1000,		0x1000, 0x3fff, MAP_ROM);

	// reads are switchable between data & RAM
	M6502MapMemory(DrvColRAM,				0x4000, 0x43ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,				0x4400, 0x47ff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,				0x4800, 0x4fff, MAP_RAM);
	M6502MapMemory(DrvScrRAM,				0x6200, 0x67ff, MAP_RAM);

	M6502MapMemory(DrvMainROM + 0x8000,		0x8000, 0xffff, MAP_ROM); // skip first 0x100 bytes for banking
	M6502SetWriteHandler(liberate_main_write);
	M6502SetReadHandler(liberate_main_read);
	M6502SetReadPortHandler(liberate_main_read_port);
	M6502SetWritePortHandler(liberate_main_write_port);
	M6502Close();

	M6502Init(1, TYPE_DECO222);
	M6502Open(1);
	M6502MapMemory(DrvSoundRAM,				0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvSoundROM + 0xc000,	0xc000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(liberate_sound_write);
	M6502SetReadHandler(liberate_sound_read);
	M6502Close();

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.16, BURN_SND_ROUTE_BOTH);
    AY8910SetBuffered(M6502TotalCycles, 1500000);

	GenericTilesInit();
	GenericTilemapInit(0, bg_map_scan, bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, tx_map_scan, tx_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM2, 3, 16, 16, 0x10000, 0, 3);
	GenericTilemapSetGfx(1, DrvGfxROM0, 3,  8,  8, gfxsize, 0, 3);
	GenericTilemapSetGfx(2, DrvGfxROM1, 3, 16, 16, gfxsize, 0, 3);
	GenericTilemapCategoryConfig(0, 3);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransMask(0, 2, 0x0001);

	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -8);
}

static INT32 LiberateInit()
{
	DrvCommonInit(0x30000, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvMainROM + 0x00000,  k,   1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x08000,  k++, 1)) return 1; // reload
		if (BurnLoadRom(DrvMainROM + 0x0c000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvSoundROM + 0xe000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x06000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0a000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvMainROM + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  k++, 1)) return 1;

		DrvLiberateDecode();
		DrvGfxDecode();
	}

	DrvDoReset();

	return 0;
}

static INT32 DualasltInit()
{
	DrvCommonInit(0x30000, 0);

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvMainROM + 0x00000,  k,   1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x08000,  k++, 1)) return 1; // reload
		if (BurnLoadRom(DrvMainROM + 0x0c000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvSoundROM + 0xe000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x06000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0a000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x02000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvMainROM + 0x04000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x06000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  k++, 1)) return 1;

		DrvLiberateDecode();
		DrvGfxDecode();
	}

	DrvDoReset();

	return 0;
}

static INT32 BoomrangInit()
{
	DrvCommonInit(0x20000,1);

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvMainROM + 0x00000,  k,   1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x08000,  k++, 1)) return 1; // reload
		if (BurnLoadRom(DrvMainROM + 0x0c000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvSoundROM + 0xc000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x06000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvMainROM + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  k++, 1)) return 1;

		DrvNibbleSwap();
		DrvGfxDecode();
	}

	DrvDoReset();

	return 0;
}

static INT32 BoomrangaInit()
{
	DrvCommonInit(0x20000,1);

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvMainROM + 0x00000,  k,   1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x08000,  k++, 1)) return 1; // reload
		if (BurnLoadRom(DrvMainROM + 0x02000,  k,   1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0a000,  k++, 1)) return 1; // reload
		if (BurnLoadRom(DrvMainROM + 0x0c000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0e000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvSoundROM + 0xc000,  k,   1)) return 1;
		if (BurnLoadRom(DrvSoundROM + 0xe000,  k++, 1)) return 1; // reload

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x06000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0e000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x02000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvMainROM + 0x04000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x06000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  k++, 1)) return 1;

		DrvNibbleSwap();
		DrvGfxDecode();
	}

	DrvDoReset();

	return 0;
}

static INT32 KamikcabInit()
{
	DrvCommonInit(0x20000,1);

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvMainROM + 0x00000,  k,   1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0c000,  k++, 1)) return 1; // reload

		if (BurnLoadRom(DrvSoundROM + 0xe000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x06000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvMainROM + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  k++, 1)) return 1;

		DrvNibbleSwap();
		DrvGfxDecode();
	}

	DrvDoReset();

	return 0;
}

static INT32 YellowcbInit()
{
	DrvCommonInit(0x20000,1);

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvMainROM + 0x00000,  k,   1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0c000,  k++, 1)) return 1; // reload
		if (BurnLoadRom(DrvMainROM + 0x02000,  k,   1)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0e000,  k++, 1)) return 1; // reload

		if (BurnLoadRom(DrvSoundROM + 0xe000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x06000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0e000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x02000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvMainROM + 0x04000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,  k++, 1)) return 1;

		DrvNibbleSwap();
		DrvGfxDecode();
	}

	vblank = &DrvMainROM[0xa000];

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	M6502Exit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 32; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (DrvColPROM[i] >> 6) & 0x01;
		bit2 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}

	DrvPalette[32] = 0; // black!
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x800; offs += 4)
	{
		int multi, fx, fy, sx, sy, sy2, code, color;

		code = DrvSprRAM[offs + 1] + ((DrvSprRAM[offs + 0] & 0x60) << 3);
		sx = 240 - DrvSprRAM[offs + 3];
		sy = 240 - DrvSprRAM[offs + 2];
		color = ((DrvSprRAM[offs + 1] & 0x08) >> 3); // ?

		fx = DrvSprRAM[offs + 0] & 0x04;
		fy = DrvSprRAM[offs + 0] & 0x02;
		multi = DrvSprRAM[offs + 0] & 0x10;

		if (multi && fy == 0)
			sy -= 16;

		if (flipscreen)
		{
			sy = 240 - sy;
			sx = 240 - sx;
			if (fy)
				sy2 = sy + 16;
			else
				sy2 = sy - 16;
			if (fx) fx = 0; else fx = 1;
			if (fy) fy = 0; else fy = 1;
		}
		else
		{
			if (fy)
				sy2 = sy - 16;
			else
				sy2 = sy + 16;
		}

		Draw16x16MaskTile(pTransDraw, code%0x300, sx, sy - 8, fx, fy, color, 3, 0, 0, DrvGfxROM1);

		if (multi)
			Draw16x16MaskTile(pTransDraw, (code+1)%0x300, sx, sy2 - 8, fx, fy, color, 3, 0, 0, DrvGfxROM1);
	}
}

static void boomrang_draw_sprites(INT32 pri)
{
	for (INT32 offs = 0; offs < 0x800; offs += 4)
	{
		int multi, fx, fy, sx, sy, sy2, code, code2, color;

		if ((DrvSprRAM[offs + 0] & 1) != 1)
			continue;
		if ((DrvSprRAM[offs + 0] & 0x8) != pri)
			continue;

		code = DrvSprRAM[offs + 1] + ((DrvSprRAM[offs + 0] & 0xe0) << 3);
		code2 = code + 1;

		multi = DrvSprRAM[offs + 0] & 0x10;

		sy=DrvSprRAM[offs + 2];
		if (multi)
			sy += 16;
		sx = (240 - DrvSprRAM[offs + 3]);
		sy = 240 - sy;

		color = (DrvSprRAM[offs + 0] & 0x4) >> 2;

		fx = 0;
		fy = DrvSprRAM[offs + 0] & 0x02;
		multi = DrvSprRAM[offs + 0] & 0x10;

		if (fy && multi) { code2 = code; code++; }

		if (flipscreen)
		{
			sy = 240 - sy;
			sx = 240 - sx;
			if (fx) fx = 0; else fx = 1;
			if (fy) fy = 0; else fy = 1;
			sy2 = sy - 16;
		}
		else
		{
			sy2 = sy + 16;
		}

		Draw16x16MaskTile(pTransDraw, code & 0x1ff, sx, sy - 8, fx, fy, color, 3, 0, 0, DrvGfxROM1);

		if (multi)
			Draw16x16MaskTile(pTransDraw, code2 & 0x1ff, sx, sy2 - 8, fx, fy, color, 3, 0, 0, DrvGfxROM1);
	}
}

static INT32 LiberateDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear(32);

	if (background_disable) {
		BurnTransferClear(32);
	} else {
		GenericTilemapSetScrollY(0,  DrvIORAM[1]);
		GenericTilemapSetScrollX(0, -DrvIORAM[0]);

		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	}

	draw_sprites();

	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 BoomrangDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollY(0,  DrvIORAM[1]);
	GenericTilemapSetScrollX(0, -DrvIORAM[0]);

	BurnTransferClear(32);

	if (background_disable) {
		BurnTransferClear(32);
	} else {
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	}

	boomrang_draw_sprites(8);

	if (background_disable == 0) {
		if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_DRAWLAYER1);
	}

	boomrang_draw_sprites(0);

	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void take_interrupt()
{
	INT32 p = ~DrvInputs[2] & 0x43; // coin & service buttons

	*vblank = 0xff;
	DrvMainROM[2] |= 0x80;

	if (p != 0 && irq_latch == 0)
	{
		M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		irq_latch = 1;
	}
	else
	{
		if (p == 0)
			irq_latch = 0;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xff;
		DrvInputs[3] = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

    M6502NewFrame();

	DrvMainROM[0] = DrvInputs[0]; /* Player 1 controls */
	DrvMainROM[1] = DrvInputs[1]; /* Player 2 controls */
	DrvMainROM[2] = DrvInputs[2] & 0x7f; /* Vblank, coins */
	DrvMainROM[3] = DrvDips[0]; /* Dip 1 */
	DrvMainROM[4] = DrvDips[1]; /* Dip 2 */

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 2000000 / 60, 1500000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	*vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6502Open(0);
		CPU_RUN(0, M6502);
		if (i == 240) take_interrupt();
		M6502Close();

		M6502Open(1);
		CPU_RUN(1, M6502);

		if ((i & 0xf) == 0xf)
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // 16x per frame

		M6502Close();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(background_color);
		SCAN_VAR(background_disable);
		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(irq_latch);
		SCAN_VAR(input_bank);
	}

	return 0;
}


// Liberation

static struct BurnRomInfo liberateRomDesc[] = {
	{ "bt12-2.bin",	0x4000, 0xa0079ffd, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "bt13-2.bin",	0x4000, 0x19f8485c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bt11.bin",	0x2000, 0xb549ccaa, 2 | BRF_PRG | BRF_ESS }, //  2 M6502 #1 Code

	{ "bt04.bin",	0x4000, 0x96e48d72, 3 | BRF_GRA },           //  3 Characters & Sprites
	{ "bt03.bin",	0x2000, 0x29ad1b59, 3 | BRF_GRA },           //  4
	{ "bt06.bin",	0x4000, 0x7bed1497, 3 | BRF_GRA },           //  5
	{ "bt05.bin",	0x2000, 0xa8896c20, 3 | BRF_GRA },           //  6
	{ "bt08.bin",	0x4000, 0x828ef78d, 3 | BRF_GRA },           //  7
	{ "bt07.bin",	0x2000, 0xf919e8e2, 3 | BRF_GRA },           //  8

	{ "bt02.bin",	0x4000, 0x7169f7bb, 4 | BRF_GRA },           //  9 Background Tiles
	{ "bt00.bin",	0x2000, 0xb744454d, 4 | BRF_GRA },           // 10

	{ "bt10.bin",	0x4000, 0xee335397, 5 | BRF_GRA },           // 11 Background Map / M6502 #0 Data

	{ "bt14.bin",	0x0020, 0x20281d61, 6 | BRF_GRA },           // 12 Color Data
};

STD_ROM_PICK(liberate)
STD_ROM_FN(liberate)

struct BurnDriver BurnDrvLiberate = {
	"liberate", NULL, NULL, NULL, "1984",
	"Liberation\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, liberateRomInfo, liberateRomName, NULL, NULL, NULL, NULL, LiberateInputInfo, LiberateDIPInfo,
	LiberateInit, DrvExit, DrvFrame, LiberateDraw, DrvScan, &DrvRecalc, 0x21,
	240, 256, 3, 4
};


// Dual Assault

static struct BurnRomInfo dualasltRomDesc[] = {
	{ "bt12",		0x4000, 0x1434ee46, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "bt13",		0x4000, 0x38e0ffa4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bt11.bin",	0x2000, 0xb549ccaa, 2 | BRF_PRG | BRF_ESS }, //  2 M6502 #1 Code

	{ "bt04-5",		0x4000, 0x159a3e85, 3 | BRF_GRA },           //  3 Characters & Sprites
	{ "bt03.bin",	0x2000, 0x29ad1b59, 3 | BRF_GRA },           //  4
	{ "bt06-5",		0x4000, 0x3b5a80c8, 3 | BRF_GRA },           //  5
	{ "bt05.bin",	0x2000, 0xa8896c20, 3 | BRF_GRA },           //  6
	{ "bt08-5",		0x4000, 0xb0cebde8, 3 | BRF_GRA },           //  7
	{ "bt07.bin",	0x2000, 0xf919e8e2, 3 | BRF_GRA },           //  8

	{ "bt01",		0x2000, 0xc0ddbeb5, 4 | BRF_GRA },           //  9 Background Tiles
	{ "bt02a",		0x2000, 0x846d9d24, 4 | BRF_GRA },           // 10
	{ "bt00.bin",	0x2000, 0xb744454d, 4 | BRF_GRA },           // 11

	{ "bt09",		0x2000, 0x2ea31472, 5 | BRF_GRA },           // 12 Background Map / M6502 #0 Data
	{ "bt10a",		0x2000, 0x69d9aa8d, 5 | BRF_GRA },           // 13

	{ "bt14.bin",	0x0020, 0x20281d61, 6 | BRF_GRA },           // 14 Color Data
};

STD_ROM_PICK(dualaslt)
STD_ROM_FN(dualaslt)

struct BurnDriver BurnDrvDualaslt = {
	"dualaslt", "liberate", NULL, NULL, "1984",
	"Dual Assault\0", NULL, "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, dualasltRomInfo, dualasltRomName, NULL, NULL, NULL, NULL, LiberateInputInfo, DualasltDIPInfo,
	DualasltInit, DrvExit, DrvFrame, LiberateDraw, DrvScan, &DrvRecalc, 0x21,
	240, 256, 3, 4
};


// Boomer Rang'r / Genesis (set 1)

static struct BurnRomInfo boomrangRomDesc[] = {
	{ "bp13.9k",	0x4000, 0xb70439b1, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "bp14.11k",	0x4000, 0x98050e13, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bp11.11f",	0x4000, 0xd6106f00, 2 | BRF_PRG | BRF_ESS }, //  2 M6502 #1 Code

	{ "bp04.7b",	0x4000, 0x5d4b12eb, 3 | BRF_GRA },           //  3 Characters & Sprites
	{ "bp06.10b",	0x4000, 0x5a18296e, 3 | BRF_GRA },           //  4
	{ "bp08.13b",	0x4000, 0x4cdb30d9, 3 | BRF_GRA },           //  5

	{ "bp02.4b",	0x4000, 0xf3c2b84f, 4 | BRF_GRA },           //  6 Background Tiles
	{ "bp00.1b",	0x4000, 0x3370cf6e, 4 | BRF_GRA },           //  7

	{ "bp10.10a",	0x4000, 0xdd18a96f, 5 | BRF_GRA },           //  8 Background Map / M6502 #0 Data

	{ "82s123.5l",	0x0020, 0xa71e19ff, 6 | BRF_GRA },           //  9 Color Data
};

STD_ROM_PICK(boomrang)
STD_ROM_FN(boomrang)

struct BurnDriver BurnDrvBoomrang = {
	"boomrang", NULL, NULL, NULL, "1983",
	"Boomer Rang'r / Genesis (set 1)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, boomrangRomInfo, boomrangRomName, NULL, NULL, NULL, NULL, LiberateInputInfo, BoomrangDIPInfo,
	BoomrangInit, DrvExit, DrvFrame, BoomrangDraw, DrvScan, &DrvRecalc, 0x21,
	240, 256, 3, 4
};


// Boomer Rang'r / Genesis (set 2)

static struct BurnRomInfo boomrangaRomDesc[] = {
	{ "bp12-2",		0x2000, 0x87fc2f0b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "bp13-2",		0x2000, 0x8e864764, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bp14-",		0x2000, 0x0a64018a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bp15-",		0x2000, 0xd23a5c31, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bp11-",		0x2000, 0xbbafe1ff, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 #1 Code

	{ "bp03-",		0x2000, 0x33565e00, 3 | BRF_GRA },           //  5 Characters & Sprites
	{ "bp04-",		0x2000, 0xab3ccae2, 3 | BRF_GRA },           //  6
	{ "bp05-",		0x2000, 0x8a8d25fa, 3 | BRF_GRA },           //  7
	{ "bp06-",		0x2000, 0xaa64bacd, 3 | BRF_GRA },           //  8
	{ "bp07-",		0x2000, 0x6c7370aa, 3 | BRF_GRA },           //  9
	{ "bp08-",		0x2000, 0x348bd0cc, 3 | BRF_GRA },           // 10

	{ "bp01-",		0x2000, 0xb4000aff, 4 | BRF_GRA },           // 11 Background Tiles
	{ "bp02-",		0x2000, 0x90044512, 4 | BRF_GRA },           // 12
	{ "bp00-",		0x2000, 0xe33564e5, 4 | BRF_GRA },           // 13

	{ "bp10-",		0x2000, 0xcb3b0f60, 5 | BRF_GRA },           // 14 Background Map / M6502 #0 Data
	{ "bp09-",		0x2000, 0xa64ac71d, 5 | BRF_GRA },           // 15

	{ "ap-16.5l",	0x0020, 0xa71e19ff, 6 | BRF_GRA },           // 16 Color Data
};

STD_ROM_PICK(boomranga)
STD_ROM_FN(boomranga)

struct BurnDriver BurnDrvBoomranga = {
	"boomranga", "boomrang", NULL, NULL, "1983",
	"Boomer Rang'r / Genesis (set 2)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, boomrangaRomInfo, boomrangaRomName, NULL, NULL, NULL, NULL, LiberateInputInfo, BoomrangDIPInfo,
	BoomrangaInit, DrvExit, DrvFrame, BoomrangDraw, DrvScan, &DrvRecalc, 0x21,
	240, 256, 3, 4
};


// Kamikaze Cabbie

static struct BurnRomInfo kamikcabRomDesc[] = {
	{ "bp11",		0x4000, 0xa69e5580, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code

	{ "bp09",		0x2000, 0x16b13676, 2 | BRF_PRG | BRF_ESS }, //  1 M6502 #1 Code

	{ "bp04",		0x4000, 0xb471542d, 3 | BRF_GRA },           //  2 Characters & Sprites
	{ "bp06",		0x4000, 0x4bf96d0d, 3 | BRF_GRA },           //  3
	{ "bp08",		0x4000, 0xb4756bed, 3 | BRF_GRA },           //  4

	{ "bp02",		0x4000, 0x77299e6e, 4 | BRF_GRA },           //  5 Background Tiles
	{ "bp00",		0x2000, 0xc20ca7ca, 4 | BRF_GRA },           //  6

	{ "bp12",		0x4000, 0x8c8f5d35, 5 | BRF_GRA },           //  7 Background Map / M6502 #0 Data

	{ "bp15",		0x0020, 0x30d3acce, 6 | BRF_GRA },           //  8 Color Data
};

STD_ROM_PICK(kamikcab)
STD_ROM_FN(kamikcab)

struct BurnDriver BurnDrvKamikcab = {
	"kamikcab", NULL, NULL, NULL, "1984",
	"Kamikaze Cabbie\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, kamikcabRomInfo, kamikcabRomName, NULL, NULL, NULL, NULL, KamikcabInputInfo, KamikcabDIPInfo,
	KamikcabInit, DrvExit, DrvFrame, BoomrangDraw, DrvScan, &DrvRecalc, 0x21,
	240, 256, 3, 4
};


// Yellow Cab (Japan)

static struct BurnRomInfo yellowcbjRomDesc[] = {
	{ "bp10.bin",	0x2000, 0x1024f2f1, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "bp11.bin",	0x2000, 0x63cdee83, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bp09",		0x2000, 0x16b13676, 2 | BRF_PRG | BRF_ESS }, //  2 M6502 #1 Code

	{ "bp03.bin",	0x2000, 0x6761d767, 3 | BRF_GRA },           //  3 Characters & Sprites
	{ "bp04.bin",	0x2000, 0x55517196, 3 | BRF_GRA },           //  4
	{ "bp05.rom",	0x2000, 0x33658fd9, 3 | BRF_GRA },           //  5
	{ "bp06.rom",	0x2000, 0xfbc20f07, 3 | BRF_GRA },           //  6
	{ "bp07.rom",	0x2000, 0x061f9e54, 3 | BRF_GRA },           //  7
	{ "bp08.rom",	0x2000, 0x2ace626d, 3 | BRF_GRA },           //  8

	{ "bp01.bin",	0x2000, 0x8e92a253, 4 | BRF_GRA },           //  9 Background Tiles
	{ "bp02.bin",	0x2000, 0x47ada1bb, 4 | BRF_GRA },           // 10
	{ "bp00.bin",	0x2000, 0x9ead0da1, 4 | BRF_GRA },           // 11

	{ "bp12",		0x4000, 0x8c8f5d35, 5 | BRF_GRA },           // 12 Background Map / M6502 #0 Data

	{ "bp15",		0x0020, 0x30d3acce, 6 | BRF_GRA },           // 13 Color Data
};

STD_ROM_PICK(yellowcbj)
STD_ROM_FN(yellowcbj)

struct BurnDriver BurnDrvYellowcbj = {
	"yellowcbj", "kamikcab", NULL, NULL, "1984",
	"Yellow Cab (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, yellowcbjRomInfo, yellowcbjRomName, NULL, NULL, NULL, NULL, KamikcabInputInfo, KamikcabDIPInfo,
	YellowcbInit, DrvExit, DrvFrame, BoomrangDraw, DrvScan, &DrvRecalc, 0x21,
	240, 256, 3, 4
};


// Yellow Cab (bootleg)

static struct BurnRomInfo yellowcbbRomDesc[] = {
	{ "rom11.rom",	0x2000, 0xaf97d530, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "rom10.rom",	0x2000, 0x33c3e9b9, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bp09",		0x2000, 0x16b13676, 2 | BRF_PRG | BRF_ESS }, //  2 M6502 #1 Code

	{ "rom3.rom",	0x2000, 0x6761d767, 3 | BRF_GRA },           //  3 Characters & Sprites
	{ "rom4.rom",	0x2000, 0x55517196, 3 | BRF_GRA },           //  4
	{ "rom5.rom",	0x2000, 0x33658fd9, 3 | BRF_GRA },           //  5
	{ "rom6.rom",	0x2000, 0xfbc20f07, 3 | BRF_GRA },           //  6
	{ "rom7.rom",	0x2000, 0x061f9e54, 3 | BRF_GRA },           //  7
	{ "rom8.rom",	0x2000, 0x2ace626d, 3 | BRF_GRA },           //  8

	{ "rom1.rom",	0x2000, 0x8e92a253, 4 | BRF_GRA },           //  9 Background Tiles
	{ "rom2.rom",	0x2000, 0x47ada1bb, 4 | BRF_GRA },           // 10
	{ "rom0.rom",	0x2000, 0x9ead0da1, 4 | BRF_GRA },           // 11

	{ "bp12",		0x4000, 0x8c8f5d35, 5 | BRF_GRA },           // 12 Background Map / M6502 #0 Data

	{ "bp15",		0x0020, 0x30d3acce, 6 | BRF_GRA },           // 13 Color Data
};

STD_ROM_PICK(yellowcbb)
STD_ROM_FN(yellowcbb)

struct BurnDriver BurnDrvYellowcbb = {
	"yellowcbb", "kamikcab", NULL, NULL, "1984",
	"Yellow Cab (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_BOOTLEG, 2, HARDWARE_PREFIX_DATAEAST, GBF_MISC, 0,
	NULL, yellowcbbRomInfo, yellowcbbRomName, NULL, NULL, NULL, NULL, KamikcabInputInfo, KamikcabDIPInfo,
	YellowcbInit, DrvExit, DrvFrame, BoomrangDraw, DrvScan, &DrvRecalc, 0x21,
	240, 256, 3, 4
};
