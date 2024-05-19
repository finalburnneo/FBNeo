// FB Alpha Arabian driver module
// Based on MAME driver by Jarek Burczynski, Phil Bennett and various others

#include "tiles_generic.h"
#include "z80_intf.h"
#include "mb88xx_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvBlitRAM;
static UINT8 *DrvTempBmp;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *arabian_color;
static UINT8 *flipscreen;

static UINT8 *mcu_port_o;
static UINT16 *mcu_port_p;
static UINT8 *mcu_port_r;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvJoy7[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[8];
static UINT8 DrvReset;

static INT32 nExtraCycles;

static struct BurnInputInfo ArabianInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy4 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy4 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy4 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy6 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy6 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy6 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy6 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy7 + 0,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Arabian)

static struct BurnDIPInfo ArabianDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0x06, NULL				},
	{0x01, 0xff, 0xff, 0x0c, NULL				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x00, 0x01, 0x01, 0x00, "3"				},
	{0x00, 0x01, 0x01, 0x01, "5"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x00, 0x01, 0x02, 0x02, "Upright"			},
	{0x00, 0x01, 0x02, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x00, 0x01, 0x04, 0x04, "Off"				},
	{0x00, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x00, 0x01, 0x08, 0x00, "Easy"				},
	{0x00, 0x01, 0x08, 0x08, "Hard"				},

	{0   , 0xfe, 0   ,    16, "Coinage"			},
	{0x00, 0x01, 0xf0, 0x10, "A 2/1 B 2/1"		},
	{0x00, 0x01, 0xf0, 0x20, "A 2/1 B 1/3"		},
	{0x00, 0x01, 0xf0, 0x00, "A 1/1 B 1/1"		},
	{0x00, 0x01, 0xf0, 0x30, "A 1/1 B 1/2"		},
	{0x00, 0x01, 0xf0, 0x40, "A 1/1 B 1/3"		},
	{0x00, 0x01, 0xf0, 0x50, "A 1/1 B 1/4"		},
	{0x00, 0x01, 0xf0, 0x60, "A 1/1 B 1/5"		},
	{0x00, 0x01, 0xf0, 0x70, "A 1/1 B 1/6"		},
	{0x00, 0x01, 0xf0, 0x80, "A 1/2 B 1/2"		},
	{0x00, 0x01, 0xf0, 0x90, "A 1/2 B 1/4"		},
	{0x00, 0x01, 0xf0, 0xa0, "A 1/2 B 1/5"		},
	{0x00, 0x01, 0xf0, 0xe0, "A 1/2 B 1/6"		},
	{0x00, 0x01, 0xf0, 0xb0, "A 1/2 B 1/10"		},
	{0x00, 0x01, 0xf0, 0xc0, "A 1/2 B 1/11"		},
	{0x00, 0x01, 0xf0, 0xd0, "A 1/2 B 1/12"		},
	{0x00, 0x01, 0xf0, 0xf0, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Coin Counters"	},
	{0x01, 0x01, 0x01, 0x01, "1"				},
	{0x01, 0x01, 0x01, 0x00, "2"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x01, 0x01, 0x02, 0x02, "Off"				},
	{0x01, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x01, 0x01, 0x0c, 0x0c, "30k 70k 40k+"		},
	{0x01, 0x01, 0x0c, 0x04, "20k only"			},
	{0x01, 0x01, 0x0c, 0x08, "40k only"			},
	{0x01, 0x01, 0x0c, 0x00, "None"				},
};

STDDIPINFO(Arabian)

static struct BurnDIPInfo ArabianaDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0x06, NULL				},
	{0x01, 0xff, 0xff, 0x0d, NULL				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x00, 0x01, 0x01, 0x00, "3"				},
	{0x00, 0x01, 0x01, 0x01, "5"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x00, 0x01, 0x02, 0x02, "Upright"			},
	{0x00, 0x01, 0x02, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x00, 0x01, 0x04, 0x04, "Off"				},
	{0x00, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x00, 0x01, 0x08, 0x00, "Easy"				},
	{0x00, 0x01, 0x08, 0x08, "Hard"				},

	{0   , 0xfe, 0   ,    16, "Coinage"			},
	{0x00, 0x01, 0xf0, 0x10, "A 2/1 B 2/1"		},
	{0x00, 0x01, 0xf0, 0x20, "A 2/1 B 1/3"		},
	{0x00, 0x01, 0xf0, 0x00, "A 1/1 B 1/1"		},
	{0x00, 0x01, 0xf0, 0x30, "A 1/1 B 1/2"		},
	{0x00, 0x01, 0xf0, 0x40, "A 1/1 B 1/3"		},
	{0x00, 0x01, 0xf0, 0x50, "A 1/1 B 1/4"		},
	{0x00, 0x01, 0xf0, 0x60, "A 1/1 B 1/5"		},
	{0x00, 0x01, 0xf0, 0x70, "A 1/1 B 1/6"		},
	{0x00, 0x01, 0xf0, 0x80, "A 1/2 B 1/2"		},
	{0x00, 0x01, 0xf0, 0x90, "A 1/2 B 1/4"		},
	{0x00, 0x01, 0xf0, 0xa0, "A 1/2 B 1/5"		},
	{0x00, 0x01, 0xf0, 0xe0, "A 1/2 B 1/6"		},
	{0x00, 0x01, 0xf0, 0xb0, "A 1/2 B 1/10"		},
	{0x00, 0x01, 0xf0, 0xc0, "A 1/2 B 1/11"		},
	{0x00, 0x01, 0xf0, 0xd0, "A 1/2 B 1/12"		},
	{0x00, 0x01, 0xf0, 0xf0, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Coin Counters"	},
	{0x01, 0x01, 0x01, 0x01, "1"				},
	{0x01, 0x01, 0x01, 0x00, "2"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x01, 0x01, 0x02, 0x02, "Off"				},
	{0x01, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x01, 0x01, 0x0c, 0x0c, "20k 50k 150k 100k+" },
	{0x01, 0x01, 0x0c, 0x04, "20k only"			},
	{0x01, 0x01, 0x0c, 0x08, "40k only"			},
	{0x01, 0x01, 0x0c, 0x00, "None"				},
};

STDDIPINFO(Arabiana)

static void blit_area(UINT8 plane, UINT16 src, UINT8 x, UINT8 y, UINT8 sx, UINT8 sy)
{
	UINT8 *srcdata = DrvGfxROM + src * 4;

	for (INT32 i = 0; i <= sx; i++, x += 4)
	{
		for (INT32 j = 0; j <= sy; j++)
		{
			UINT8 p1 = *srcdata++;
			UINT8 p2 = *srcdata++;
			UINT8 p3 = *srcdata++;
			UINT8 p4 = *srcdata++;
			UINT8 *base = DrvTempBmp + (((y + j) & 0xff) * 256 + (x & 0xff));

			if (plane & 0x01)
			{
				if (p4 != 8) base[0] = (base[0] & ~0xf0) | (p4 << 4);
				if (p3 != 8) base[1] = (base[1] & ~0xf0) | (p3 << 4);
				if (p2 != 8) base[2] = (base[2] & ~0xf0) | (p2 << 4);
				if (p1 != 8) base[3] = (base[3] & ~0xf0) | (p1 << 4);
			}

			if (plane & 0x04)
			{
				if (p4 != 8) base[0] = (base[0] & ~0x0f) | p4;
				if (p3 != 8) base[1] = (base[1] & ~0x0f) | p3;
				if (p2 != 8) base[2] = (base[2] & ~0x0f) | p2;
				if (p1 != 8) base[3] = (base[3] & ~0x0f) | p1;
			}
		}
	}
}

static void arabian_blitter_w(INT32 offset, UINT8 data)
{
	DrvBlitRAM[offset] = data;

	if (offset == 0x0006)
	{
		INT32 plane = DrvBlitRAM[offset - 6];
		INT32 src   = DrvBlitRAM[offset - 5] | (DrvBlitRAM[offset - 4] << 8);
		INT32 x     = DrvBlitRAM[offset - 2] << 2;
		INT32 y     = DrvBlitRAM[offset - 3];
		INT32 sx    = DrvBlitRAM[offset - 0];
		INT32 sy    = DrvBlitRAM[offset - 1];

		blit_area(plane, src, x, y, sx, sy);
	}
}

static void __fastcall arabian_videoram_w(UINT16 offset, UINT8 data)
{
	UINT8 *base;
	UINT8 x, y;

	x = (offset >> 8) << 2;
	y = offset & 0xff;

	DrvVidRAM[offset & 0x3fff] = data;

	base = &DrvTempBmp[y * 256 + x];

	if (DrvBlitRAM[0] & 0x08)
	{
		base[0] = (base[0] & ~0x03) | ((data & 0x10) >> 3) | ((data & 0x01) >> 0);
		base[1] = (base[1] & ~0x03) | ((data & 0x20) >> 4) | ((data & 0x02) >> 1);
		base[2] = (base[2] & ~0x03) | ((data & 0x40) >> 5) | ((data & 0x04) >> 2);
		base[3] = (base[3] & ~0x03) | ((data & 0x80) >> 6) | ((data & 0x08) >> 3);
	}

	if (DrvBlitRAM[0] & 0x04)
	{
		base[0] = (base[0] & ~0x0c) | ((data & 0x10) >> 1) | ((data & 0x01) << 2);
		base[1] = (base[1] & ~0x0c) | ((data & 0x20) >> 2) | ((data & 0x02) << 1);
		base[2] = (base[2] & ~0x0c) | ((data & 0x40) >> 3) | ((data & 0x04) << 0);
		base[3] = (base[3] & ~0x0c) | ((data & 0x80) >> 4) | ((data & 0x08) >> 1);
	}

	if (DrvBlitRAM[0] & 0x02)
	{
		base[0] = (base[0] & ~0x30) | ((data & 0x10) << 1) | ((data & 0x01) << 4);
		base[1] = (base[1] & ~0x30) | ((data & 0x20) << 0) | ((data & 0x02) << 3);
		base[2] = (base[2] & ~0x30) | ((data & 0x40) >> 1) | ((data & 0x04) << 2);
		base[3] = (base[3] & ~0x30) | ((data & 0x80) >> 2) | ((data & 0x08) << 1);
	}

	if (DrvBlitRAM[0] & 0x01)
	{
		base[0] = (base[0] & ~0xc0) | ((data & 0x10) << 3) | ((data & 0x01) << 6);
		base[1] = (base[1] & ~0xc0) | ((data & 0x20) << 2) | ((data & 0x02) << 5);
		base[2] = (base[2] & ~0xc0) | ((data & 0x40) << 1) | ((data & 0x04) << 4);
		base[3] = (base[3] & ~0xc0) | ((data & 0x80) << 0) | ((data & 0x08) << 3);
	}
}

static void sync_mcu()
{
	INT32 cyc = ((INT64)ZetTotalCycles(0) * (2000000 / 6) / 3000000) - mb88xxTotalCycles();

	if (cyc > 0) {
		mb88xxRun(cyc);
	}
}

static void __fastcall arabian_write(UINT16 address, UINT8 data)
{
	if ((address & 0xc000) == 0x8000) {
		arabian_videoram_w(address & 0x3fff, data);
		return;
	}

	if ((address & 0xf000) == 0xe000) {
		arabian_blitter_w(address & 0x0007, data);
		return;
	}
}

static UINT8 __fastcall arabian_read(UINT16 address)
{
	if ((address & 0xfe00) == 0xc000) {
		return DrvInputs[0];
	}

	if ((address & 0xfe00) == 0xc200) {
		return DrvDips[0];
	}

	return 0;
}

static void __fastcall arabian_out(UINT16 port, UINT8 data)
{
	switch (port)
	{
		case 0xc800:
			AY8910Write(0, 0, data);
		return;

		case 0xca00:
			AY8910Write(0, 1, data);
		return;
	}
}

static UINT8 mcu_port_r0_read()
{
	return mcu_port_r[0] | 0x04;
}

static UINT8 mcu_port_r1_read()
{
	return mcu_port_r[1];
}

static UINT8 mcu_port_r2_read()
{
	return mcu_port_r[2];
}

static UINT8 mcu_port_r3_read()
{
	return mcu_port_r[3];
}

static void mcu_port_r0_write(UINT8 data)
{
	if (~data & 2) {
		DrvZ80RAM[*mcu_port_p | *mcu_port_o] = 0xf0 | mcu_port_r[3];
	}

	*flipscreen = data & 8;

	mcu_port_r[0] = data & 0x0f;
}

static void mcu_port_r1_write(UINT8 data)
{
	mcu_port_r[1] = data & 0x0f;
}

static void mcu_port_r2_write(UINT8 data)
{
	mcu_port_r[2] = data & 0x0f;
}

static void mcu_port_r3_write(UINT8 data)
{
	mcu_port_r[3] = data & 0x0f;
}

static UINT8 mcu_port_k_read()
{
	if (~mcu_port_r[0] & 1) {
		return DrvZ80RAM[*mcu_port_p | *mcu_port_o] & 0x0f;
	} else {
		for (INT32 i = 0; i < 6; i++) {
			if (~(((mcu_port_r[2] << 4) | mcu_port_r[1]) & 0x3f) & (1 << i)) {
				return DrvInputs[(i & 7)+2] & 0x0f;
			}
		}
	}

	return 0x0f;
}

static void mcu_port_o_write(UINT8 data)
{
	*mcu_port_o = (*mcu_port_o & (0x0f << ((~data & 0x10) >> 2))) | ((data & 0x0f) << ((data & 0x10) >> 2));
}

static void mcu_port_p_write(UINT8 data)
{
	*mcu_port_p = (data & 0x07) << 8;
}

static void ay8910_porta_w(UINT32, UINT32 data)
{
	*arabian_color = data >> 3;
}

static void ay8910_portb_w(UINT32, UINT32 data)
{
	sync_mcu();
	mb88xxSetRESETLine(~data & 0x10);
	mb88xxSetIRQLine(0, (data & 0x20) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	mb88xxReset();

	AY8910Reset(0);

	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x008000;
	DrvMCUROM		= Next; Next += 0x000800;

	DrvGfxROM		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x004000;
	DrvBlitRAM		= Next; Next += 0x001000;

	DrvTempBmp		= Next; Next += 256 * 256;

	flipscreen		= Next; Next += 0x000001;
	arabian_color	= Next; Next += 0x000001;
	mcu_port_o		= Next; Next += 0x000001;
	mcu_port_p		= (UINT16*)Next; Next += 0x000002;
	mcu_port_r		= Next; Next += 0x000004;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 tpal[0x40];

	for (INT32 i = 0; i < 64; i++)
	{
		INT32 r, b, g;
		r = ((i >> 5) & 1) * (153*192/255) + ((i >> 4) & 1) * (102*192/255) + ((i & 0x30) ? 63 : 0);
		g = ((i >> 3) & 1) * (156*192/255) + ((i >> 2) & 1) * (99*192/255) + ((i & 0x0c) ? 63 : 0);
		b = ((i >> 1) & 1) * 192 + ((i >> 0) & 1) * 63;
		tpal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < (1 << 13); i++)
	{
		INT32 ena = (i >> 12) & 1;
		INT32 enb = (i >> 11) & 1;
		INT32 abhf = (~i >> 10) & 1;
		INT32 aghf = (~i >> 9) & 1;
		INT32 arhf = (~i >> 8) & 1;
		INT32 az = (i >> 7) & 1;
		INT32 ar = (i >> 6) & 1;
		INT32 ag = (i >> 5) & 1;
		INT32 ab = (i >> 4) & 1;
		INT32 bz = (i >> 3) & 1;
		INT32 br = (i >> 2) & 1;
		INT32 bg = (i >> 1) & 1;
		INT32 bb = (i >> 0) & 1;

		INT32 planea = (az | ar | ag | ab) & ena;

		INT32 rhi = planea ? ar : enb ? bz : 0;
		INT32 rlo = planea ? (((!arhf) & az) ? 0 : ar) : enb ? br : 0;

		INT32 ghi = planea ? ag : enb ? bb : 0;
		INT32 glo = planea ? (((!aghf) & az) ? 0 : ag) : enb ? bg : 0;

		INT32 bhi = ab;
		INT32 bbase = ((!abhf) & az) ? 0 : ab;

		INT32 t = (rhi << 5) | (rlo << 4) | (ghi << 3) | (glo << 2) | (bhi << 1) | bbase;

		DrvPalette[i] = tpal[t];
	}
}

static void DrvGfxDecode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);

	for (INT32 offs = 0; offs < 0x4000 * 4; offs+=4)
	{
		INT32 v1 = DrvGfxROM[(offs / 4) + 0x0000];
		INT32 v2 = DrvGfxROM[(offs / 4) + 0x4000];

		for (INT32 j = 0; j < 4; j++, v1>>=1, v2>>=1) {
			tmp[offs + 3 - j] = (v1 & 1) | ((v1 >> 3) & 2) | ((v2 << 2) & 4) | ((v2 >> 1) & 8);
		}
	}

	memcpy (DrvGfxROM, tmp, 0x10000);
	BurnFree (tmp);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM + 0x0000, 0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2000, 1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x4000, 2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x6000, 3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x0000, 4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x2000, 5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x4000, 6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x6000, 7, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM + 0x0000, 8, 1)) return 1;

		DrvPaletteInit();
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0xd800, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(arabian_write);
	ZetSetReadHandler(arabian_read);
	ZetSetOutHandler(arabian_out);
	ZetClose();

	mb88xxInit(0, 8841, DrvMCUROM);
	mb88xxSetReadWriteFunction(mcu_port_r0_write, mcu_port_r0_read, 'r', 0);
	mb88xxSetReadWriteFunction(mcu_port_r1_write, mcu_port_r1_read, 'r', 1);
	mb88xxSetReadWriteFunction(mcu_port_r2_write, mcu_port_r2_read, 'r', 2);
	mb88xxSetReadWriteFunction(mcu_port_r3_write, mcu_port_r3_read, 'r', 3);
	mb88xxSetReadWriteFunction(NULL, mcu_port_k_read, 'k', 0);
	mb88xxSetReadWriteFunction(mcu_port_o_write, NULL, 'o', 0);
	mb88xxSetReadWriteFunction(mcu_port_p_write, NULL, 'p', 0);

	AY8910Init(0, 1500000, 0);
	AY8910SetPorts(0, NULL, NULL, ay8910_porta_w, ay8910_portb_w);
	AY8910SetAllRoutes(0, 0.40, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3000000);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	mb88xxExit();

	AY8910Exit(0);

	BurnFreeMemIndex();

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	UINT16 *pDst = pTransDraw;
	UINT8 *pSrc = DrvTempBmp + (11 * 256);

	*flipscreen = 0; // don't flip coctail, for 2p / 2joy

	if (*flipscreen) {
		pDst += nScreenWidth * (nScreenHeight - 1);
		for (INT32 y = 11; y < 256-11; y++) {
			for (INT32 x = 0; x < 256; x++) {
				pDst[x] = pSrc[x ^ 0xff];
			}
			pDst -= nScreenWidth;
			pSrc += 256;
		}
	} else {
		for (INT32 y = 11; y < 256-11; y++) {
			for (INT32 x = 0; x < 256; x++) {
				pDst[x] = pSrc[x];
			}
			pDst += nScreenWidth;
			pSrc += 256;
		}
	}

	BurnTransferCopy(DrvPalette + (*arabian_color << 8));

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();
	mb88xxNewFrame();

	{
		memset (DrvInputs, 0, 8);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
			DrvInputs[6] ^= (DrvJoy7[i] & 1) << i;
		}
		DrvInputs[1] = DrvDips[0];
		DrvInputs[2] |= 1;
		DrvInputs[7] = DrvDips[1];
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 2000000 / 6 / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Zet);
		sync_mcu();

		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}

	ZetClose();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

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

		ZetScan(nAction);
		mb88xxScan(nAction);

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(nExtraCycles);
	}

	return 0;
}


// Arabian

static struct BurnRomInfo arabianRomDesc[] = {
	{ "ic1rev2.87", 0x2000, 0x5e1c98b8, BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "ic2rev2.88", 0x2000, 0x092f587e, BRF_ESS | BRF_PRG }, //  1
	{ "ic3rev2.89", 0x2000, 0x15145f23, BRF_ESS | BRF_PRG }, //  2
	{ "ic4rev2.90", 0x2000, 0x32b77b44, BRF_ESS | BRF_PRG }, //  3

	{ "tvg-91.ic84",0x2000, 0xc4637822, BRF_GRA },		 //  4 Graphics
	{ "tvg-92.ic85",0x2000, 0xf7c6866d, BRF_GRA },		 //  5
	{ "tvg-93.ic86",0x2000, 0x71acd48d, BRF_GRA },		 //  6
	{ "tvg-94.ic87",0x2000, 0x82160b9a, BRF_GRA },		 //  7
	
	{ "sun-8212.ic3",0x0800, 0x8869611e, BRF_ESS | BRF_PRG },		 //  8 MCU
};

STD_ROM_PICK(arabian)
STD_ROM_FN(arabian)

struct BurnDriver BurnDrvarabian = {
	"arabian", NULL, NULL, NULL, "1983",
	"Arabian\0", NULL, "Sun Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, arabianRomInfo, arabianRomName, NULL, NULL, NULL, NULL, ArabianInputInfo, ArabianDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	234, 256, 3, 4
};

// Arabian (Atari)

static struct BurnRomInfo arabianaRomDesc[] = {
	{ "tvg-87.ic1", 0x2000, 0x51e9a6b1, BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "tvg-88.ic2", 0x2000, 0x1cdcc1ab, BRF_ESS | BRF_PRG }, //  1
	{ "tvg-89.ic3", 0x2000, 0xb7b7faa0, BRF_ESS | BRF_PRG }, //  2
	{ "tvg-90.ic4", 0x2000, 0xdbded961, BRF_ESS | BRF_PRG }, //  3

	{ "tvg-91.ic84",0x2000, 0xc4637822, BRF_GRA },		 //  4 Graphics
	{ "tvg-92.ic85",0x2000, 0xf7c6866d, BRF_GRA },		 //  5
	{ "tvg-93.ic86",0x2000, 0x71acd48d, BRF_GRA },		 //  6
	{ "tvg-94.ic87",0x2000, 0x82160b9a, BRF_GRA },		 //  7
	
	{ "sun-8212.ic3",0x0800, 0x8869611e, BRF_ESS | BRF_PRG },		 //  8 MCU
};

STD_ROM_PICK(arabiana)
STD_ROM_FN(arabiana)

struct BurnDriver BurnDrvarabiana = {
	"arabiana", "arabian", NULL, NULL, "1983",
	"Arabian (Atari)\0", NULL, "Sun Electronics (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, arabianaRomInfo, arabianaRomName, NULL, NULL, NULL, NULL, ArabianInputInfo, ArabianaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	234, 256, 3, 4
};
