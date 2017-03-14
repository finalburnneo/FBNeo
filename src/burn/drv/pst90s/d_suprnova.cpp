// FB Alpha Super Kaneko Nova System driver module by iq_132, fixups by dink
// Based on MAME driver by Sylvain Glaize and David Haywood
//
// notes: vblokbr/sarukani prefers 60hz w/the speedhack, or it gets stuck in testmode.
//

#include "tiles_generic.h"
#include "ymz280b.h"
#include "sknsspr.h"
#include "sh2_intf.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvSh2BIOS;
static UINT8 *DrvSh2ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvNvRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvLineRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvGfxRAM;
static UINT8 *DrvSh2RAM;
static UINT8 *DrvCacheRAM;
static UINT8 *DrvV3Regs;
static UINT8 *DrvSprRegs;
static UINT8 *DrvPalRegs;

static UINT32 *DrvPalette;

static UINT8 *DrvTmpScreenBuf;
static UINT16 *DrvTmpScreenA;
static UINT16 *DrvTmpScreenB;
static UINT16 *DrvTmpScreenA2;
static UINT16 *DrvTmpScreenB2;
static UINT16 *DrvTmpScreenC;
static UINT8 *DrvTmpFlagA;
static UINT8 *DrvTmpFlagB;
static UINT8 *DrvTmpFlagA2;
static UINT8 *DrvTmpFlagB2;
static UINT32 *DrvTmpDraw; // main drawing surface
static UINT32 *pDrvTmpDraw;
static UINT8 *olddepths;

static struct {
	UINT16 x1p, y1p, z1p, x1s, y1s, z1s;
	UINT16 x2p, y2p, z2p, x2s, y2s, z2s;
	UINT16 org;

	UINT16 x1_p1, x1_p2, y1_p1, y1_p2, z1_p1, z1_p2;
	UINT16 x2_p1, x2_p2, y2_p1, y2_p2, z2_p1, z2_p2;
	UINT16 x1tox2, y1toy2, z1toz2;
	INT16 x_in, y_in, z_in;
	UINT16 flag;

	UINT8 disconnect;
} hit;

static INT32 sprite_kludge_x;
static INT32 sprite_kludge_y;

static UINT8 DrvJoy1[32];
static UINT8 DrvDips[2];
static UINT32 DrvInputs[3];
static INT32 DrvAnalogPort0 = 0;
static INT32 DrvAnalogPort1 = 0;
static UINT8 DrvReset;

static INT32 sixtyhz = 0;

static INT32 nGfxLen0 = 0;
static INT32 nRedrawTiles = 0;
static UINT32 speedhack_address = ~0;
static UINT32 speedhack_pc[2] = { 0, 0 };
static UINT8 m_region = 0; /* 0 Japan, 1 Europe, 2 Asia, 3 USA, 4 Korea */
static UINT32 Vblokbrk = 0;
static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};
#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static struct BurnInputInfo VblokbrkInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 24,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 25,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 26,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 30,	"p1 fire 3"},
	A("P1 Paddle",           BIT_ANALOG_REL, &DrvAnalogPort0,"p1 z-axis"),

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 16,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 17,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 18,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 22,	"p2 fire 3"},
	A("P2 Paddle",          BIT_ANALOG_REL, &DrvAnalogPort1,"p2 z-axis"),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 13,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

#undef A

STDINPUTINFO(Vblokbrk)

static struct BurnInputInfo SknsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 24,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 25,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 26,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 30,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 16,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 17,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 18,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 22,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 13,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Skns)

static struct BurnDIPInfo SknsDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL		},
	{0x16, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x15, 0x01, 0x01, 0x01, "Off"		},
	{0x15, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"	},
	{0x15, 0x01, 0x02, 0x02, "Off"		},
	{0x15, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Use Backup Ram"},
	{0x15, 0x01, 0x40, 0x00, "No"		},
	{0x15, 0x01, 0x40, 0x40, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Freeze"	},
	{0x15, 0x01, 0x80, 0x00, "Freezes the game"},
	{0x15, 0x01, 0x80, 0x80, "No"	},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"},
	{0x16, 0x01, 0x01, 0x00, "No"		},
	{0x16, 0x01, 0x01, 0x01, "Yes"		},
};

STDDIPINFO(Skns)

static struct BurnDIPInfo SknsNoSpeedhackDIPList[]=
{
	{0x15, 0xff, 0xff, 0xff, NULL		},
	{0x16, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x15, 0x01, 0x01, 0x01, "Off"		},
	{0x15, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"	},
	{0x15, 0x01, 0x02, 0x02, "Off"		},
	{0x15, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Use Backup Ram"},
	{0x15, 0x01, 0x40, 0x00, "No"		},
	{0x15, 0x01, 0x40, 0x40, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Freeze"	},
	{0x15, 0x01, 0x80, 0x00, "Freezes the game"},
	{0x15, 0x01, 0x80, 0x80, "No"	},
};

STDDIPINFO(SknsNoSpeedhack)

static struct BurnDIPInfo VblokbrkDIPList[]=
{
	{0x17, 0xff, 0xff, 0xff, NULL		},
	{0x18, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x17, 0x01, 0x01, 0x01, "Off"		},
	{0x17, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"	},
	{0x17, 0x01, 0x02, 0x02, "Off"		},
	{0x17, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Use Backup Ram"},
	{0x17, 0x01, 0x40, 0x00, "No"		},
	{0x17, 0x01, 0x40, 0x40, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Freeze"	},
	{0x17, 0x01, 0x80, 0x00, "Freezes the game"},
	{0x17, 0x01, 0x80, 0x80, "No"	},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"},
	{0x18, 0x01, 0x01, 0x00, "No"		},
	{0x18, 0x01, 0x01, 0x01, "Yes"		},
};

STDDIPINFO(Vblokbrk)

static struct BurnInputInfo CyvernInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 24,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 25,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 26,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 16,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 17,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 18,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 13,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Cyvern)


static struct BurnDIPInfo CyvernDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL		},
	{0x14, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    2, "Service Mode" },
	{0x13, 0x01, 0x01, 0x01, "Off"		},
	{0x13, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"	},
	{0x13, 0x01, 0x02, 0x02, "Off"		},
	{0x13, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Use Backup Ram"},
	{0x13, 0x01, 0x40, 0x00, "No"		},
	{0x13, 0x01, 0x40, 0x40, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Freeze"	},
	{0x13, 0x01, 0x80, 0x00, "Freezes the game"},
	{0x13, 0x01, 0x80, 0x80, "No"	},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"},
	{0x14, 0x01, 0x01, 0x00, "No"		},
	{0x14, 0x01, 0x01, 0x01, "Yes"		},
};

STDDIPINFO(Cyvern)

static struct BurnDIPInfo CyvernNoSpeedhackDIPList[]= // gals panic 4 (galpani4)
{
	{0x13, 0xff, 0xff, 0xff, NULL		},
	{0x14, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    2, "Service Mode" },
	{0x13, 0x01, 0x01, 0x01, "Off"		},
	{0x13, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"	},
	{0x13, 0x01, 0x02, 0x02, "Off"		},
	{0x13, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Use Backup Ram"},
	{0x13, 0x01, 0x40, 0x00, "No"		},
	{0x13, 0x01, 0x40, 0x40, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Freeze"	},
	{0x13, 0x01, 0x80, 0x00, "Freezes the game"},
	{0x13, 0x01, 0x80, 0x80, "No"	},
};

STDDIPINFO(CyvernNoSpeedhack)

static void hit_calc_orig(UINT16 p, UINT16 s, UINT16 org, UINT16 *l, UINT16 *r)
{
	switch(org & 3) {
	case 0:
		*l = p;
		*r = p+s;
	break;
	case 1:
		*l = p-s/2;
		*r = *l+s;
	break;
	case 2:
		*l = p-s;
		*r = p;
	break;
	case 3:
		*l = p-s;
		*r = p+s;
	break;
	}
}

static void hit_calc_axis(UINT16 x1p, UINT16 x1s, UINT16 x2p, UINT16 x2s, UINT16 org,
			  UINT16 *x1_p1, UINT16 *x1_p2, UINT16 *x2_p1, UINT16 *x2_p2,
			  INT16 *x_in, UINT16 *x1tox2)
{
	UINT16 x1l=0, x1r=0, x2l=0, x2r=0;
	hit_calc_orig(x1p, x1s, org,      &x1l, &x1r);
	hit_calc_orig(x2p, x2s, org >> 8, &x2l, &x2r);

	*x1tox2 = x2p-x1p;
	*x1_p1 = x1p;
	*x2_p1 = x2p;
	*x1_p2 = x1r;
	*x2_p2 = x2l;
	*x_in = x1r-x2l;
}

static void hit_recalc()
{
	hit_calc_axis(hit.x1p, hit.x1s, hit.x2p, hit.x2s, hit.org,
		&hit.x1_p1, &hit.x1_p2, &hit.x2_p1, &hit.x2_p2,
		&hit.x_in, &hit.x1tox2);
	hit_calc_axis(hit.y1p, hit.y1s, hit.y2p, hit.y2s, hit.org,
		&hit.y1_p1, &hit.y1_p2, &hit.y2_p1, &hit.y2_p2,
		&hit.y_in, &hit.y1toy2);
	hit_calc_axis(hit.z1p, hit.z1s, hit.z2p, hit.z2s, hit.org,
		&hit.z1_p1, &hit.z1_p2, &hit.z2_p1, &hit.z2_p2,
		&hit.z_in, &hit.z1toz2);

	hit.flag = 0;
	hit.flag |= hit.y2p > hit.y1p ? 0x8000 : hit.y2p == hit.y1p ? 0x4000 : 0x2000;
	hit.flag |= hit.y_in >= 0 ? 0 : 0x1000;
	hit.flag |= hit.x2p > hit.x1p ? 0x0800 : hit.x2p == hit.x1p ? 0x0400 : 0x0200;
	hit.flag |= hit.x_in >= 0 ? 0 : 0x0100;
	hit.flag |= hit.z2p > hit.z1p ? 0x0080 : hit.z2p == hit.z1p ? 0x0040 : 0x0020;
	hit.flag |= hit.z_in >= 0 ? 0 : 0x0010;
	hit.flag |= hit.x_in >= 0 && hit.y_in >= 0 && hit.z_in >= 0 ? 8 : 0;
	hit.flag |= hit.z_in >= 0 && hit.x_in >= 0                  ? 4 : 0;
	hit.flag |= hit.y_in >= 0 && hit.z_in >= 0                  ? 2 : 0;
	hit.flag |= hit.x_in >= 0 && hit.y_in >= 0                  ? 1 : 0;
}

static void skns_hit_w(UINT32 adr, UINT32 data)
{
	switch(adr & ~3) {
	case 0x00:
	case 0x28:
		hit.x1p = data;
	break;
	case 0x08:
	case 0x30:
		hit.y1p = data;
	break;
	case 0x38:
	case 0x50:
		hit.z1p = data;
	break;
	case 0x04:
	case 0x2c:
		hit.x1s = data;
	break;
	case 0x0c:
	case 0x34:
		hit.y1s = data;
	break;
	case 0x3c:
	case 0x54:
		hit.z1s = data;
	break;
	case 0x10:
	case 0x58:
		hit.x2p = data;
	break;
	case 0x18:
	case 0x60:
		hit.y2p = data;
	break;
	case 0x20:
	case 0x68:
		hit.z2p = data;
	break;
	case 0x14:
	case 0x5c:
		hit.x2s = data;
	break;
	case 0x1c:
	case 0x64:
		hit.y2s = data;
	break;
	case 0x24:
	case 0x6c:
		hit.z2s = data;
	break;
	case 0x70:
		hit.org = data;
	break;
	default:
	break;
	}
	hit_recalc();
}

static UINT32 skns_hit_r(UINT32 adr)
{
	if(hit.disconnect)
		return 0x0000;

	switch(adr & 0xfc) {
	case 0x28:
	case 0x2a:
		return (Sh2TotalCycles() ^ (Sh2TotalCycles() >> 16)) & 0xffff;
	case 0x00:
	case 0x10:
		return (UINT16)hit.x_in;
	case 0x04:
	case 0x14:
		return (UINT16)hit.y_in;
	case 0x18:
		return (UINT16)hit.z_in;
	case 0x08:
	case 0x1c:
		return hit.flag;
	case 0x40:
		return hit.x1p;
	case 0x48:
		return hit.y1p;
	case 0x50:
		return hit.z1p;
	case 0x44:
		return hit.x1s;
	case 0x4c:
		return hit.y1s;
	case 0x54:
		return hit.z1s;
	case 0x58:
		return hit.x2p;
	case 0x60:
		return hit.y2p;
	case 0x68:
		return hit.z2p;
	case 0x5c:
		return hit.x2s;
	case 0x64:
		return hit.y2s;
	case 0x6c:
		return hit.z2s;
	case 0x70:
		return hit.org;
	case 0x80:
		return hit.x1tox2;
	case 0x84:
		return hit.y1toy2;
	case 0x88:
		return hit.z1toz2;
	case 0x90:
		return hit.x1_p1;
	case 0xa0:
		return hit.y1_p1;
	case 0xb0:
		return hit.z1_p1;
	case 0x98:
		return hit.x1_p2;
	case 0xa8:
		return hit.y1_p2;
	case 0xb8:
		return hit.z1_p2;
	case 0x94:
		return hit.x2_p1;
	case 0xa4:
		return hit.y2_p1;
	case 0xb4:
		return hit.z2_p1;
	case 0x9c:
		return hit.x2_p2;
	case 0xac:
		return hit.y2_p2;
	case 0xbc:
		return hit.z2_p2;
	default:
		return 0;
	}
}


static UINT32 skns_msm6242_r(UINT32 offset)
{
	time_t nLocalTime = time(NULL); // ripped from pgm_run.cpp
	tm* tmLocalTime = localtime(&nLocalTime);

	UINT32 value = 0;

	switch ((offset >> 2) & 3)
	{
		case 0:
			value  = (tmLocalTime->tm_sec % 10)<<24;
			value |= (tmLocalTime->tm_sec / 10)<<16;
			value |= (tmLocalTime->tm_min % 10)<<8;
			value |= (tmLocalTime->tm_min / 10);
			break;
		case 1:
			value  = (tmLocalTime->tm_hour % 10)<<24;
			value |= (tmLocalTime->tm_hour / 10)<<16;
			value |= (tmLocalTime->tm_mday % 10)<<8;
			value |= (tmLocalTime->tm_mday / 10);
			break;
		case 2:
			value  = ((tmLocalTime->tm_mon + 1) % 10) << 24;
			value |= ((tmLocalTime->tm_mon + 1) / 10) << 16;
			value |=  (tmLocalTime->tm_year % 10) << 8;
			value |= ((tmLocalTime->tm_year / 10) % 10);
			break;
		case 3:
			value  = (tmLocalTime->tm_wday)<<24;
			value |= (1)<<16;
			value |= (6)<<8;
			value |= (4);
			break;
	}
	return value;
}

static UINT8 __fastcall suprnova_read_byte(UINT32 address)
{
	address &= 0xc7ffffff;

	if ((address & 0xfffffff0) == 0x01000000) {
		return skns_msm6242_r(address) >> ((~address & 3) << 3);
	}

	if ((address & 0xffffff00) == 0x02f00000) {
		return skns_hit_r(address) >> ((~address & 3) << 3);
	}

	switch (address)
	{
		case 0x00400000:
		case 0x00400001:
		case 0x00400002:
		case 0x00400003:
			return DrvInputs[0] >> ((~address & 3) << 3); // 400000

		case 0x00400004:
		case 0x00400005:
		case 0x00400006:
		case 0x00400007:
			return DrvInputs[1] >> ((~address & 3) << 3); // 400004

		case 0x0040000c:
		case 0x0040000d:
		case 0x0040000e:
		case 0x0040000f:
			return DrvInputs[2] >> ((~address & 3) << 3); // 40000c

		case 0x00c00000:
		case 0x00c00001:
		case 0x00c00002:return 0;
		case 0x00c00003:
			return YMZ280BReadStatus();
	}
	bprintf(0, _T("rb %X. "), address);

	return 0;
}

static UINT16 __fastcall suprnova_read_word(UINT32 address)
{
	address &= 0xc7fffffe;

	if ((address & 0xfffffff0) == 0x01000000) {
		return skns_msm6242_r(address) >> ((~address & 2) << 3);
	}

	if ((address & 0xffffff00) == 0x02f00000) {
		return skns_hit_r(address) >> ((~address & 2) << 3);
	}

	switch (address)
	{
		case 0x00400000:
		case 0x00400001:
			return DrvInputs[0] >> 16;

		case 0x00400002:
		case 0x00400003:
			return DrvInputs[0]; // 400000

		case 0x00400004:
		case 0x00400005:
			return DrvInputs[1] >> 16;

		case 0x00400006:
		case 0x00400007:
			return DrvInputs[1]; // 400004

		case 0x0040000c:
		case 0x0040000d:
			return DrvInputs[2] >> 16;

		case 0x0040000e:
		case 0x0040000f:
			return DrvInputs[2]; // 40000c
	}
	bprintf(0, _T("rw %X. "), address);

	return 0;
}

static UINT32 __fastcall suprnova_read_long(UINT32 address)
{
	address &= 0xc7fffffc;

	if ((address & 0xfffffff0) == 0x01000000) {
		return skns_msm6242_r(address);
	}

	if ((address & 0xffffff00) == 0x02f00000) {
		return skns_hit_r(address);
	}

	switch (address)
	{
		case 0x00400000:
			return DrvInputs[0];

		case 0x00400004:
			return DrvInputs[1];

		case 0x0040000c:
			return DrvInputs[2];
	}

	return 0;
}

static INT32 suprnova_alt_enable_sprites = 0;
static INT32 bright_spc_g_trans = 0;
static INT32 bright_spc_r_trans = 0;
static INT32 bright_spc_b_trans = 0;
static INT32 bright_spc_g = 0;
static INT32 bright_spc_r = 0;
static INT32 bright_spc_b = 0;
static INT32 suprnova_alt_enable_background = 0;
static INT32 bright_v3_g = 0;
static INT32 bright_v3_r = 0;
static INT32 bright_v3_b = 0;
static INT32 use_spc_bright = 1;
static INT32 use_v3_bright = 1;

static void skns_pal_regs_w(UINT32 offset)
{
	UINT32 data = *((UINT32*)(DrvPalRegs + (offset & 0x1c)));
	offset = (offset >> 2) & 7;

	switch ( offset )
	{
		case (0x00/4): // RWRA0
			use_spc_bright = data&1;
			suprnova_alt_enable_sprites = (data>>8)&1;
			break;

		case (0x04/4): // RWRA1
			bright_spc_g = data&0xff;
			bright_spc_g_trans = (data>>8) &0xff;
			break;

		case (0x08/4): // RWRA2
			bright_spc_r = data&0xff;
			bright_spc_r_trans = (data>>8) &0xff;
			break;

		case (0x0C/4): // RWRA3
			bright_spc_b = data&0xff;
			bright_spc_b_trans = (data>>8)&0xff;
			break;

		case (0x10/4): // RWRB0
			use_v3_bright = data&1;
			suprnova_alt_enable_background = (data>>8)&1;
			break;

		case (0x14/4): // RWRB1
			bright_v3_g = data&0xff;
		//	bright_v3_g_trans = (data>>8)&0xff;
			break;

		case (0x18/4): // RWRB2
			bright_v3_r = data&0xff;
		//	bright_v3_r_trans = (data>>8)&0xff;
			break;

		case (0x1C/4): // RWRB3
			bright_v3_b = data&0xff;
		//	bright_v3_b_trans = (data>>8)&0xff;
			break;
	}
}

static inline void decode_graphics_ram(UINT32 offset)
{
	offset &= 0x3fffc;
	UINT32 p = *((UINT32*)(DrvGfxRAM + offset));

	if ( (DrvGfxROM2[offset + 0] == p >> 24) &&
		 (DrvGfxROM2[offset + 1] == p >> 16) &&
		 (DrvGfxROM2[offset + 2] == p >>  8) &&
		 (DrvGfxROM2[offset + 3] == p >>  0) ) return;

	nRedrawTiles = 1;

	DrvGfxROM2[offset + 0] = p >> 24;
	DrvGfxROM2[offset + 1] = p >> 16;
	DrvGfxROM2[offset + 2] = p >>  8;
	DrvGfxROM2[offset + 3] = p >>  0;
}

static void __fastcall suprnova_write_byte(UINT32 address, UINT8 data)
{
	address &= 0xc7ffffff;

	if ((address & 0xfffc0000) == 0x4800000) {
		DrvGfxRAM[(address & 0x3ffff) ^ 3] = data;
		decode_graphics_ram(address);
		return;

	} //04800000, 0x0483ffff

	switch (address)
	{
		case 0x00c00000:
			YMZ280BSelectRegister(data);
		return;

		case 0x00c00001:
			YMZ280BWriteRegister(data);
		return;

		case 0x01800000:
			//  case 0x01800001:
			//  case 0x01800002:
			//  case 0x01800003:// sengeki writes here... puzzloop complains (security...)
			{
				hit.disconnect=1; /* hit2 stuff */
				switch (m_region) /* 0 Japan, 1 Europe, 2 Asia, 3 USA, 4 Korea */
				{
					case 0:
						if (data == 0) hit.disconnect= 0;
						break;
					case 3:
						if (data == 1) hit.disconnect= 0;
						break;
                    case 4: // korea
						if (data == 2) hit.disconnect= 0;
						break;
                    case 1:
						if (data == 3) hit.disconnect= 0;
						break;
                    case 2:
						if (data < 2) hit.disconnect= 0;
						break;
						// unknown country id, unlock per default
                    default:
						hit.disconnect= 0;
						break;
				}
			}
			return;
	}

	if ((address & 0xffffffe0) == 0x02a00000) {
		DrvPalRegs[(address & 0x1f) ^ 3] = data;
		skns_pal_regs_w(address);
		return;
	}

	// skns_msm6242_w -- not used
	if ((address & ~0x0f)  == 0x1000000) return;

	// skns io -- not used
	if ((address & ~0x0f) == 0x00400000) {
		if ((Sh2GetPC(0) == 0x04013B42 + 2) && Vblokbrk) { // speedhack for Vblokbrk / Saru Kani
			//Sh2BurnUntilInt(0); // this breaks sound in vblokbrk...
		}
		return;
	}
}

static void __fastcall suprnova_write_word(UINT32 address, UINT16 data)
{
	address &= 0xc7fffffe;

	if ((address & 0xfffc0000) == 0x4800000) {
		*((UINT16*)(DrvGfxRAM + ((address & 0x3fffe) ^ 2))) = data;
		decode_graphics_ram(address);
		return;

	} //04800000, 0x0483ffff
}

static void __fastcall suprnova_write_long(UINT32 address, UINT32 data)
{
	address &= 0xc7fffffc;

	if ((address & 0xfffc0000) == 0x4800000) {
		*((UINT32*)(DrvGfxRAM + (address & 0x3fffc))) = data;
		decode_graphics_ram(address);
		return;

	} //04800000, 0x0483ffff

	if ((address & 0xffffffe0) == 0x02a00000) {
		*((UINT32*)(DrvPalRegs + (address & 0x1c))) = data;
		skns_pal_regs_w(address);
		return;
	}

	if ((address & 0xffffff00) == 0x02f00000) {
		skns_hit_w(address & 0xff, data);
		return;
	}

	if (address == 0x05000000) return; // vsblock
}

static inline void suprnova_speedhack(UINT32 a)
{
	UINT32 b  = a & ~3;
	UINT32 pc = Sh2GetPC(0);

	if (b == speedhack_address) {
		if (pc == speedhack_pc[0]) {
			Sh2BurnUntilInt(0);
		}
	}
}

static UINT32 __fastcall suprnova_hack_read_long(UINT32 a)
{
	suprnova_speedhack(a);

	a &= 0xffffc;

	return *((UINT32*)(DrvSh2RAM + a));
}

static UINT16 __fastcall suprnova_hack_read_word(UINT32 a)
{
	suprnova_speedhack(a);

	return *((UINT16 *)(DrvSh2RAM + ((a & 0xffffe) ^ 2)));
}

static UINT8 __fastcall suprnova_hack_read_byte(UINT32 a)
{
	suprnova_speedhack(a);

	return DrvSh2RAM[(a & 0xfffff) ^ 3];
}

static void BurnSwapEndian(UINT8 *src, INT32 len)
{
	for (INT32 i = 0; i < len; i+=4) {
		INT32 t = src[i + 0];
		src[i + 0] = src[i+3];
		src[i + 3] = t;
		t = src[i + 1];
		src[i + 1] = src[i + 2];
		src[i + 2] = t;
	}
}

static INT32 MemIndex(INT32 gfxlen0)
{
	UINT8 *Next; Next = AllMem;

	DrvSh2BIOS		= Next; Next += 0x0080000;
	DrvSh2ROM		= Next; Next += 0x0400000;

	YMZ280BROM		= Next; Next += 0x0500000;

	DrvGfxROM0		= Next; Next += gfxlen0;
	DrvGfxROM1		= Next; Next += 0x0800000;
	DrvGfxROM2		= Next; Next += 0x0800000;

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x010000;
	DrvNvRAM		= Next; Next += 0x010000;
	DrvSprRAM		= Next; Next += 0x010000;
	DrvLineRAM		= Next; Next += 0x010000;
	DrvPalRAM		= Next; Next += 0x020000;
	DrvGfxRAM		= Next; Next += 0x040000;
	DrvSh2RAM		= Next; Next += 0x100000;
	DrvCacheRAM		= Next; Next += 0x010000;
	DrvV3Regs		= Next; Next += 0x010100;
	DrvSprRegs		= Next; Next += 0x010100;
	DrvPalRegs		= Next; Next += 0x010020;

	RamEnd			= Next;

	DrvTmpScreenBuf		= Next; Next += 0x10000;

	DrvTmpScreenA		= (UINT16*)Next; Next += 1024 * 1024 * sizeof(INT16);
	DrvTmpScreenB		= (UINT16*)Next; Next += 1024 * 1024 * sizeof(INT16);
	DrvTmpScreenC		= (UINT16*)Next; Next += 320 * 240 * sizeof(INT16);
	DrvTmpScreenA2		= (UINT16*)Next; Next += 320 * 240 * sizeof(INT16);
	DrvTmpScreenB2		= (UINT16*)Next; Next += 320 * 240 * sizeof(INT16);
	pDrvTmpDraw		= (UINT32*)Next;
	DrvTmpDraw		= (UINT32*)Next; Next += 320 * 240 * sizeof(INT32);

	DrvTmpFlagA		= Next; Next += 1024 * 1024;
	DrvTmpFlagB		= Next; Next += 1024 * 1024;

	DrvTmpFlagA2		= Next; Next += 320 * 240;
	DrvTmpFlagB2		= Next; Next += 320 * 240;

	DrvPalette		= (UINT32*)Next; Next += 0x10000 * sizeof(INT32);

	olddepths       = Next; Next += 2 * sizeof(UINT8);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	memset (DrvTmpScreenBuf, 0xff, 0x8000);
	memset (&hit, 0, sizeof(hit));

	Sh2Open(0);
	if (Vblokbrk) {
		Sh2Reset(); // VS Block Breaker / Saru Kani must run through the Super Kaneko BIOS for nvram to work!
	} else { // Run everything else directly, bypassing the bios.
		Sh2Reset( *(UINT32 *)(DrvSh2ROM + 0), *(UINT32 *)(DrvSh2ROM + 4) );
		if (sprite_kludge_y == -272) // sengekistriker
			Sh2SetVBR(0x6000000);
		else Sh2SetVBR(0x4000000);
	}
	Sh2Close();

	YMZ280BReset();

	hit.disconnect = (m_region != 2) ? 1 : 0;

	suprnova_alt_enable_sprites = 0;
	bright_spc_g_trans = bright_spc_r_trans = bright_spc_b_trans = 0;
	bright_spc_g = bright_spc_r = bright_spc_b = 0;
	//suprnova_alt_enable_background = 0; set in init, and by game
	bright_v3_g = bright_v3_r = bright_v3_b = 0;
	use_spc_bright = 1;
	use_v3_bright = 1;

	nRedrawTiles = 1;
	olddepths[0] = olddepths[1] = 0xff;

	HiscoreReset();

	sh2_suprnova_speedhack = (DrvDips[1] & 1);

 	return 0;
}

static INT32 DrvLoad(INT32 nLoadRoms)
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *LoadPr = DrvSh2ROM;
	UINT8 *LoadSp = DrvGfxROM0;
	UINT8 *LoadBg = DrvGfxROM1;
	UINT8 *LoadFg = DrvGfxROM2 + 0x400000;
	UINT8 *LoadYM = YMZ280BROM;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (nLoadRoms) {
				if (BurnLoadRom(LoadPr + 0, i+0, 2)) return 1;
				if (BurnLoadRom(LoadPr + 1, i+1, 2)) return 1;
			}
			LoadPr += ri.nLen * 2;
			i++; 

			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (nLoadRoms) {
				if (BurnLoadRom(LoadSp, i, 1)) return 1;
			}
			LoadSp += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 3) {
			if (nLoadRoms) {
				if (BurnLoadRom(LoadBg, i, 1)) return 1;
			}
			LoadBg += ri.nLen;

			continue;
		}
		
		if ((ri.nType & 7) == 4) {
			if (nLoadRoms) {
				if (BurnLoadRom(LoadFg, i, 1)) return 1;
			}
			LoadFg += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 5) {
			if (nLoadRoms) {
				if (BurnLoadRom(LoadYM, i, 1)) return 1;
			}
			LoadYM += ri.nLen;
			continue;
		}
	}

	if (!nLoadRoms) {
		for (nGfxLen0 = 1; nGfxLen0 < (LoadSp - DrvGfxROM0); nGfxLen0 <<= 1) {}
	}

	return 0;
}

static INT32 DrvInit(INT32 bios)
{
	AllMem = NULL;
	DrvLoad(0);
	MemIndex(nGfxLen0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(nGfxLen0);

	{
		if (DrvLoad(1)) return 1;

		if (BurnLoadRom(DrvSh2BIOS, 0x00080 + bios, 1)) return 1;	// bios
		m_region = bios;
		BurnSwapEndian(DrvSh2BIOS, 0x80000);
		BurnSwapEndian(DrvSh2ROM, 0x200000);
	}

	Sh2Init(1);
	Sh2Open(0);

	Sh2MapMemory(DrvSh2BIOS,		0x00000000, 0x0007ffff, MAP_ROM);
	Sh2MapMemory(DrvNvRAM,			0x00800000, 0x00801fff, MAP_RAM);
	Sh2MapMemory(DrvSprRAM,			0x02000000, 0x02003fff, MAP_RAM);
	Sh2MapMemory(DrvSprRegs,		0x02100000, 0x0210003f, MAP_RAM); // sprite regs
	Sh2MapMemory(DrvV3Regs,			0x02400000, 0x0240007f, MAP_RAM);
	Sh2MapMemory(DrvVidRAM,			0x02500000, 0x02507fff, MAP_RAM); //0-4000, 4000-7fff A, B
	Sh2MapMemory(DrvLineRAM,		0x02600000, 0x02607fff, MAP_RAM);
	Sh2MapMemory(DrvPalRegs,		0x02a00000, 0x02a0001f, MAP_ROM);
	Sh2MapMemory(DrvPalRAM,			0x02a40000, 0x02a5ffff, MAP_RAM);
	Sh2MapMemory(DrvSh2ROM,			0x04000000, 0x041fffff, MAP_ROM);
	Sh2MapMemory(DrvGfxRAM,			0x04800000, 0x0483ffff, MAP_ROM); // tilemap B, graphics tiles
	Sh2MapMemory(DrvSh2RAM,			0x06000000, 0x060fffff, MAP_RAM);
	Sh2MapMemory(DrvCacheRAM,		0xc0000000, 0xc0000fff, MAP_RAM);

	Sh2SetReadByteHandler (0,		suprnova_read_byte);
	Sh2SetReadWordHandler (0,		suprnova_read_word);
	Sh2SetReadLongHandler (0,		suprnova_read_long);
	Sh2SetWriteByteHandler(0,		suprnova_write_byte);
	Sh2SetWriteWordHandler(0,		suprnova_write_word);
	Sh2SetWriteLongHandler(0,		suprnova_write_long);

	Sh2MapHandler(1,			0x06000000, 0x060fffff, MAP_ROM);
	Sh2SetReadByteHandler (1,		suprnova_hack_read_byte);
	Sh2SetReadWordHandler (1,		suprnova_hack_read_word);
	Sh2SetReadLongHandler (1,		suprnova_hack_read_long);

	if (!strncmp(BurnDrvGetTextA(DRV_NAME), "galpanis", 8)) {
		bprintf(0, _T("Note (soundfix): switching Busy Loop Speedhack to mode #2 for galpanis*.\n"));
		sh2_busyloop_speedhack_mode2 = 1;
	}

	if (!sixtyhz) BurnSetRefreshRate(59.5971);

	YMZ280BInit(16666666, NULL);

	skns_init();
	skns_sprite_kludge(sprite_kludge_x, sprite_kludge_y);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	skns_exit();

	Sh2Exit();
	YMZ280BExit();
	YMZ280BROM = NULL;

	free(AllMem);
	AllMem = NULL;

	suprnova_alt_enable_background = 0;
	Vblokbrk = 0;
	nGfxLen0 = 0;

	sixtyhz = 0;

	speedhack_address = ~0;
	memset (speedhack_pc, 0, 2 * sizeof(INT32));

	return 0;
}

static void draw_layer(UINT8 *source, UINT8 *previous, UINT16 *dest, UINT8 *prid, UINT8 *gfxbase, INT32 layer)
{
	UINT32 *prev = (UINT32*)previous;
	UINT32 *vram = (UINT32*)source;

	UINT8 depthchanged[2] = { 0, 0 };
	UINT32 depth = *((UINT32*)(DrvV3Regs + 0x0c));
	if (layer) depth >>= 8;
	depth &= 1;

	if (depth != olddepths[layer]) {
		depthchanged[layer] = 1;
		olddepths[layer] = depth;
	}

	for (INT32 offs = 0; offs < 64 * 64; offs++)
	{
		// dirty tile speed hack. nRedrawTiles true if ram-based graphics changed.
		if (layer == 1) {
			if (!depthchanged[layer] && !nRedrawTiles && vram[offs] == prev[offs]) {
				continue;
			}
		} else {
			if (!depthchanged[layer] && vram[offs] == prev[offs]) {
				continue;
			}
		}
		prev[offs] = vram[offs];

		INT32 sx = (offs & 0x3f) << 4;
		INT32 sy = (offs >> 6) << 4;

		INT32 attr  = vram[offs];
		INT32 code  = attr & 0x001fffff;
		INT32 color =((attr & 0x3f000000) >> 24) | 0x40;
		INT32 prio  =(attr & 0x00e00000) >> 21;

		INT32 flipx = (attr >> 31) & 1;
		INT32 flipy = (attr >> 30) & 1;
	
		color <<= 8;
		UINT8 *pri = prid + sy * 1024 + sx;
		UINT16 *dst = dest + sy * 1024 + sx;

		if (depth) {	// 4bpp

			code &= 0x0FFFF;

			if (flipy) flipy = 0x78;
			if (flipx) flipy |=0x07;

			UINT8 *gfx = gfxbase + (code << 7);

			for (INT32 y = 0; y < 16; y++) {
				for (INT32 x = 0; x < 16; x+=2) {
					INT32 c = gfx[((y << 3) | (x >> 1)) ^ flipy];

					dst[x+0] = (c & 0x0f) + color;
					dst[x+1] = (c >> 4) + color;
					pri[x+0] = pri[x+1] = prio;
				}

				dst += 1024;
				pri += 1024;
			}
		} else {	// 8bpp
			code &= 0x7FFF;

			UINT8 *gfx = gfxbase + (code << 8);
			if (flipy) gfx += 0xf0;
			INT32 inc = flipy ? -16 : 16;

			for (INT32 y = 0; y < 16 * 16; y+=16, gfx += inc) {
				if (flipx) {
					dst[ 0] = gfx[15] + color;
					dst[ 1] = gfx[14] + color;
					dst[ 2] = gfx[13] + color;
					dst[ 3] = gfx[12] + color;
					dst[ 4] = gfx[11] + color;
					dst[ 5] = gfx[10] + color;
					dst[ 6] = gfx[ 9] + color;
					dst[ 7] = gfx[ 8] + color;
					dst[ 8] = gfx[ 7] + color;
					dst[ 9] = gfx[ 6] + color;
					dst[10] = gfx[ 5] + color;
					dst[11] = gfx[ 4] + color;
					dst[12] = gfx[ 3] + color;
					dst[13] = gfx[ 2] + color;
					dst[14] = gfx[ 1] + color;
					dst[15] = gfx[ 0] + color;
				} else {
					dst[ 0] = gfx[ 0] + color;
					dst[ 1] = gfx[ 1] + color;
					dst[ 2] = gfx[ 2] + color;
					dst[ 3] = gfx[ 3] + color;
					dst[ 4] = gfx[ 4] + color;
					dst[ 5] = gfx[ 5] + color;
					dst[ 6] = gfx[ 6] + color;
					dst[ 7] = gfx[ 7] + color;
					dst[ 8] = gfx[ 8] + color;
					dst[ 9] = gfx[ 9] + color;
					dst[10] = gfx[10] + color;
					dst[11] = gfx[11] + color;
					dst[12] = gfx[12] + color;
					dst[13] = gfx[13] + color;
					dst[14] = gfx[14] + color;
					dst[15] = gfx[15] + color;
				}

				pri[ 0] = prio;
				pri[ 1] = prio;
				pri[ 2] = prio;
				pri[ 3] = prio;
				pri[ 4] = prio;
				pri[ 5] = prio;
				pri[ 6] = prio;
				pri[ 7] = prio;
				pri[ 8] = prio;
				pri[ 9] = prio;
				pri[10] = prio;
				pri[11] = prio;
				pri[12] = prio;
				pri[13] = prio;
				pri[14] = prio;
				pri[15] = prio;

				dst += 1024;
				pri += 1024;
			}
		}
	}
}


static void suprnova_draw_roz(UINT16 *source, UINT8 *flags, UINT16 *ddest, UINT8 *dflags, UINT32 startx, UINT32 starty, INT32 incxx, INT32 incxy, INT32 incyx, INT32 incyy, INT32 wraparound, INT32 columnscroll, UINT32* scrollram)
{
	const INT32 xmask = 0x3ff;
	const INT32 ymask = 0x3ff;
	const UINT32 widthshifted = 1024 << 16;
	const UINT32 heightshifted = 1024 << 16;
	UINT32 cx;
	UINT32 cy;
	INT32 x;
	INT32 sx;
	INT32 sy;
	INT32 ex;
	INT32 ey;
	UINT16 *dest;
	UINT8* destflags;

	/* pre-advance based on the cliprect */
	startx += 0 * incxx + 0 * incyx;
	starty += 0 * incxy + 0 * incyy;

	/* extract start/end points */
	sx = 0;
	sy = 0;
	ex = nScreenWidth-1;
	ey = nScreenHeight-1;

	{
		/* loop over rows */
		while (sy <= ey)
		{

			/* initialize X counters */
			x = sx;
			cx = startx;
			cy = starty;

			/* get dest and priority pointers */
			dest = ddest + (sy * nScreenWidth) + sx;
			destflags = dflags + (sy * nScreenWidth) + sx;

			/* loop over columns */
			while (x <= ex)
			{
				if ((wraparound) || (cx < widthshifted && cy < heightshifted)) // not sure how this will cope with no wraparound, but row/col scroll..
				{
					if (columnscroll)
					{
						INT32 offset = (((cy >> 16) - scrollram[(cx>>16)&0x3ff]) & ymask) * 1024 + ((cx >> 16) & xmask);
						offset &= 0xfffff;
						dest[0]     = source[offset];
						destflags[0] = flags[offset];
					}
					else
					{
						INT32 offset = ((cy >> 16) & ymask) * 1024 + (((cx >> 16) - scrollram[(cy>>16)&0x3ff]) & xmask);
						offset &= 0xfffff;
;						dest[0] =     source[offset];
						destflags[0] = flags[offset];
					}
				}

				/* advance in X */
				cx += incxx;
				cy += incxy;
				x++;
				dest++;
				destflags++;
			}

			/* advance in Y */
			startx += incyx;
			starty += incyy;
			sy++;
		}
	}
}

static void supernova_draw(INT32 *offs, UINT16 *bitmap, UINT8 *flags, UINT16 *dbitmap, UINT8 *dflags, INT32 layer)
{
	UINT32 *vreg = (UINT32*)DrvV3Regs;
	UINT32 *line = (UINT32*)DrvLineRAM;

	INT32 enable = (vreg[offs[0]] >> 0) & 0x0001;
	INT32 nowrap = (vreg[offs[0]] >> 0) & 0x0004;

	UINT32 startx,starty;
	INT32 incxx,incxy,incyx,incyy;
	INT32 columnscroll;

	if (enable && suprnova_alt_enable_background)
	{
		if (layer == 0) draw_layer(DrvVidRAM + 0x0000, DrvTmpScreenBuf + 0x0000, DrvTmpScreenA, DrvTmpFlagA, DrvGfxROM1, 0);
		if (layer == 1) draw_layer(DrvVidRAM + 0x4000, DrvTmpScreenBuf + 0x4000, DrvTmpScreenB, DrvTmpFlagB, DrvGfxROM2, 1);

		startx = vreg[offs[1]];
		incyy  = vreg[offs[2]]&0x7ffff;
		if (incyy&0x40000) incyy = incyy-0x80000; // level 3 boss in sengekis
		incyx  = vreg[offs[3]];
		starty = vreg[offs[4]];
		incxy  = vreg[offs[5]];
		incxx  = vreg[offs[6]]&0x7ffff;
		if (incxx&0x40000) incxx = incxx-0x80000;

		columnscroll = (vreg[0x0c/4] >> offs[7]) & 0x0001;

		// iq_132 complete hack for now....
		if ((incyy|incyx|incxy|incxx)==0) {
			incyy=1<<8;
			incxx=1<<8;
		}

		if (nBurnLayer & (layer+1)) suprnova_draw_roz(bitmap,flags,dbitmap,dflags,startx << 8,starty << 8,	incxx << 8,incxy << 8,incyx << 8,incyy << 8, !nowrap, columnscroll, &line[offs[8]]);
	}
}

static void DrvRecalcPalette()
{
	INT32 use_bright, brightness_r, brightness_g, brightness_b;
	INT32 r,g,b;
	UINT32 *p = (UINT32*)DrvPalRAM;
	for (INT32 i = 0; i < 0x20000 / 4; i++) {
		r = (p[i] >> 10) & 0x1f;
		g = (p[i] >>  5) & 0x1f;
		b = (p[i] >>  0) & 0x1f;

		if (i < 0x4000) { // 1st half is for Sprites
			use_bright = use_spc_bright;
			brightness_b = bright_spc_b;
			brightness_g = bright_spc_g;
			brightness_r = bright_spc_r;
		} else { // V3 bg's
			use_bright = use_v3_bright;
			brightness_b = bright_v3_b;
			brightness_g = bright_v3_g;
			brightness_r = bright_v3_r;
		}

		if(use_bright) {
			if(brightness_b) b = ((b<<3) * (brightness_b+1))>>8;
			else b = 0;
			if(brightness_g) g = ((g<<3) * (brightness_g+1))>>8;
			else g = 0;
			if(brightness_r) r = ((r<<3) * (brightness_r+1))>>8;
			else r = 0;
		} else {
			r <<= 3;
			g <<= 3;
			b <<= 3;
		}

		DrvPalette[i] = (r << 16) | (g << 8) | b;
	}
}


static void render_and_copy_layers()
{
	UINT32 *vreg = (UINT32*)DrvV3Regs;

	INT32 offs[2][9] = {
		{ 0x10 / 4, 0x1c / 4, 0x30 / 4, 0x2c / 4, 0x20 / 4, 0x28 / 4, 0x24 / 4, 1, 0x0000 },
		{ 0x34 / 4, 0x40 / 4, 0x54 / 4, 0x50 / 4, 0x44 / 4, 0x4c / 4, 0x48 / 4, 9, 0x1000 / 4 }
	};

	{
		INT32 supernova_pri_a = (vreg[0x10/4] & 0x0002)>>1;
		INT32 supernova_pri_b = (vreg[0x34/4] & 0x0002)>>1;

		supernova_draw(offs[1], DrvTmpScreenB, DrvTmpFlagB, DrvTmpScreenB2, DrvTmpFlagB2, 1);
		supernova_draw(offs[0], DrvTmpScreenA, DrvTmpFlagA, DrvTmpScreenA2, DrvTmpFlagA2, 0);

		{
			INT32 x,y;
			UINT8* srcflags, *src2flags;
			UINT16* src, *src2, *src3;
			UINT32* dst;
			UINT16 pri, pri2, pri3;
			UINT16 bgpri;

			UINT32 *clut = DrvPalette;

			for (y=0;y<240;y++)
			{
				src = DrvTmpScreenB2 + y * nScreenWidth; //BITMAP_ADDR16(tilemap_bitmap_lower, y, 0);
				srcflags = DrvTmpFlagB2 + y * nScreenWidth; //BITMAP_ADDR8(tilemap_bitmapflags_lower, y, 0);

				src2 = DrvTmpScreenA2 + y * nScreenWidth; //BITMAP_ADDR16(tilemap_bitmap_higher, y, 0);
				src2flags = DrvTmpFlagA2 + y * nScreenWidth; //BITMAP_ADDR8(tilemap_bitmapflags_higher, y, 0);

				src3 = DrvTmpScreenC + y * nScreenWidth; //BITMAP_ADDR16(sprite_bitmap, y, 0);

				dst = DrvTmpDraw + y * nScreenWidth; //BITMAP_ADDR32(bitmap, y, 0);

				for (x=0;x<320;x++)
				{
					UINT16 pendata  = src[x]&0x7fff;
					UINT16 pendata2 = src2[x]&0x7fff;
					UINT16 bgpendata;
					UINT16 pendata3 = src3[x]&0x3fff;

					UINT32 coldat;

					pri = ((srcflags[x] & 0x07)<<1) | (supernova_pri_b);
					pri2= ((src2flags[x] & 0x07)<<1) | (supernova_pri_a);
					pri3 = ((src3[x]&0xc000)>>12)+3;

					if (pri<=pri2) // <= is good for last level of cyvern.. < seem better for galpanis kaneko logo
					{
						if (pendata2&0xff)
						{
							bgpendata = pendata2&0x7fff;
							bgpri = pri2;
						}
						else if (pendata&0xff)
						{
							bgpendata = pendata&0x7fff;
							bgpri = pri;
						}
						else
						{
							bgpendata = pendata2&0x7fff;
							bgpri = 0;
						}
					}
					else
					{
						if (pendata&0xff)
						{
							bgpendata = pendata&0x7fff;
							bgpri = pri;
						}
						else if (pendata2&0xff)
						{
							bgpendata = pendata2&0x7fff;
							bgpri = pri2;
						}
						else
						{
							bgpendata = 0;
							bgpri = 0;
						}
					}

					// if the sprites are higher than the bg pixel
					if (pri3 > bgpri)
					{
						if (pendata3&0xff)
						{
							UINT16 palvalue = *((UINT32*)(DrvPalRAM + (pendata3 * 4)));

							coldat = clut[pendata3];

							if (palvalue&0x8000) // iq_132
							{
								UINT32 srccolour = clut[bgpendata&0x7fff];
								UINT32 dstcolour = clut[pendata3&0x3fff];

								INT32 r,g,b;
								INT32 r2,g2,b2;

								r = (srccolour & 0x000000ff)>> 0;
								g = (srccolour & 0x0000ff00)>> 8;
								b = (srccolour & 0x00ff0000)>> 16;

								r2 = (dstcolour & 0x000000ff)>> 0;
								g2 = (dstcolour & 0x0000ff00)>> 8;
								b2 = (dstcolour & 0x00ff0000)>> 16;

								r2 = (r2 * bright_spc_r_trans) >> 8;
								g2 = (g2 * bright_spc_g_trans) >> 8;
								b2 = (b2 * bright_spc_b_trans) >> 8;

								r = (r+r2);
								if (r>255) r = 255;

								g = (g+g2);
								if (g>255) g = 255;

								b = (b+b2);
								if (b>255) b = 255;

								dst[x] = (r << 16) | (g << 8) | (b << 0);
							}

							else
							{
								coldat = clut[pendata3];
								dst[x] = coldat;
							}
						}
						else
						{
							coldat = clut[bgpendata];
							dst[x] = coldat;
						}
					}
					else
					{
						coldat = clut[bgpendata];
						dst[x] = coldat;
					}

				}
			}
		}
	}
}


static INT32 DrvDraw()
{
	DrvRecalcPalette();

	if (nBurnBpp == 4) { // 32bpp rendered directly
		DrvTmpDraw = (UINT32*)pBurnDraw;
	} else {
		DrvTmpDraw = pDrvTmpDraw;
	}

	memset (DrvTmpScreenA2, 0, nScreenWidth * nScreenHeight * 2);
	memset (DrvTmpScreenB2, 0, nScreenWidth * nScreenHeight * 2);

	render_and_copy_layers();

	// mix sprites next frame (necessary 1frame sprite lag)
	memset (DrvTmpScreenC,  0, nScreenWidth * nScreenHeight * 2);
	if (nSpriteEnable & 1) skns_draw_sprites(DrvTmpScreenC, (UINT32*)DrvSprRAM, 0x4000, DrvGfxROM0, nGfxLen0, (UINT32*)DrvSprRegs, 0);

	if (nBurnBpp != 4) {
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			INT32 d = DrvTmpDraw[i];
			PutPix(pBurnDraw + i * nBurnBpp, BurnHighCol(d>>16, d>>8, d, 0));
		}
	}

	nRedrawTiles = 0;

	return 0;
}

static UINT32 scalerange_skns(UINT32 x, UINT32 in_min, UINT32 in_max, UINT32 out_min, UINT32 out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static UINT8 Paddle_X = 0;

static UINT8 Paddle_incdec(UINT32 PaddlePortnum) {
	UINT8 Temp;

	Temp = 0x7f + (PaddlePortnum >> 4);
	if (Temp < 0x01) Temp = 0x01;
	if (Temp > 0xfe) Temp = 0xfe;
	Temp = scalerange_skns(Temp, 0x3f, 0xbe, 0x01, 0xfe);
	if (Temp > 0x90) Paddle_X-=15;
	if (Temp < 0x70) Paddle_X+=15;
	return Paddle_X;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = ~0;
		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

		DrvInputs[1] = 0x0000ff00 | DrvDips[0];
		DrvInputs[1] |= Paddle_incdec(DrvAnalogPort0) << 24;
		DrvInputs[2] = 0xffffffff;
	}

	UINT32 nTotalCycles = (sixtyhz) ? (28638000 / 60) : (INT32)(28638000 / 59.5971);
	INT32 nCyclesDone = 0;
	INT32 nInterleave = 262;

	for (INT32 i = 0; i < nInterleave; i++) {
		//Sh2Run(nTotalCycles / nInterleave);
		nCyclesDone += Sh2Run(((i + 1) * nTotalCycles / nInterleave) - nCyclesDone);

		if (i == 1) {
			Sh2SetIRQLine(1, CPU_IRQSTATUS_AUTO);
		} else if (i == 240) {
			Sh2SetIRQLine(5, CPU_IRQSTATUS_AUTO);
		}
		{ // fire irq9 every interleave iteration.
			Sh2SetIRQLine(9, CPU_IRQSTATUS_AUTO);
			if (i%125==0 && i!=0) { //125 = every 8 ms (per 261 interleave)
				Sh2SetIRQLine(11, CPU_IRQSTATUS_AUTO);
			}
			if (i%31==0 && i!=0) { //31=every 2 ms
				Sh2SetIRQLine(15, CPU_IRQSTATUS_AUTO);
			}
		}
	}

	if (pBurnSoundOut) {
		YMZ280BRender(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}


static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin =  0x029707;
	}
	
	if (nAction & ACB_MEMORY_RAM) {
		ba.Data		= AllRam;
		ba.nLen		= RamEnd - AllRam;
		ba.nAddress = 0;
		ba.szName	= "All RAM";
		BurnAcb(&ba);

		ba.Data		= DrvGfxROM2;
		ba.nLen		= 0x40000;
		ba.nAddress = 0;
		ba.szName	= "RAM Tiles";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_DRIVER_DATA) {
		Sh2Scan(nAction);
		YMZ280BScan();

		SCAN_VAR(hit);
		SCAN_VAR(suprnova_alt_enable_sprites);
		SCAN_VAR(bright_spc_g_trans);
		SCAN_VAR(bright_spc_r_trans);
		SCAN_VAR(bright_spc_b_trans);
		SCAN_VAR(bright_spc_g);
		SCAN_VAR(bright_spc_r);
		SCAN_VAR(bright_spc_b);
		SCAN_VAR(suprnova_alt_enable_background);
		SCAN_VAR(bright_v3_g);
		SCAN_VAR(bright_v3_r);
		SCAN_VAR(bright_v3_b);
		SCAN_VAR(use_spc_bright);
		SCAN_VAR(use_v3_bright);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNvRAM;
		ba.nLen		= 0x02000;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE) {
		nRedrawTiles = 1;
		olddepths[0] = olddepths[1] = 0xff;
	}

	return 0;
}


// Super Kaneko Nova System BIOS

static struct BurnRomInfo sknsRomDesc[] = {
	{ "sknsj1.u10",		0x80000, 0x7e2b836c, BRF_BIOS}, //  0 Japan
	{ "sknse2.u10",		0x80000, 0xe2b9d7d1, BRF_BIOS}, //  1 Europe
	{ "sknsa1.u10",		0x80000, 0x745e5212, BRF_BIOS}, //  2 Asia
	{ "sknsu1.u10",		0x80000, 0x384d21ec, BRF_BIOS}, //  3 USA
	{ "sknsk1.u10",		0x80000, 0xff1c9f79, BRF_BIOS}, //  4 Korea
	
#if defined (ROM_VERIFY)
	{ "supernova_modbios-japan.u10", 0x080000, 0xb8d3190c, BRF_OPT },
	{ "supernova-modbios-korea.u10", 0x080000, 0x1d90517c, BRF_OPT },
#endif
};

STD_ROM_PICK(skns)
STD_ROM_FN(skns)

static INT32 SknsInit() {
	return 1;
}

struct BurnDriver BurnDrvSkns = {
	"skns", NULL, NULL, NULL, "1996",
	"Super Kaneko Nova System BIOS\0", "BIOS only", "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_KANEKO_SKNS, GBF_BIOS, 0,
	NULL, sknsRomInfo, sknsRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo,
	SknsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,  0x8000,
	320, 240, 4, 3
};


// Cyvern (US)

static struct BurnRomInfo cyvernRomDesc[] = {
	{ "cv-usa.u10",	        0x100000, 0x1023ddca, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "cv-usa.u8",		0x100000, 0xf696f6be, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cv100-00.u24",	0x400000, 0xcd4ae88a, 2 | BRF_GRA},            //  2 Sprites
	{ "cv101-00.u20",	0x400000, 0xa6cb3f0b, 2 | BRF_GRA},            //  3

	{ "cv200-00.u16",	0x400000, 0xddc8c67e, 3 | BRF_GRA},            //  4 Background Tiles
	{ "cv201-00.u13",	0x400000, 0x65863321, 3 | BRF_GRA},            //  5

	{ "cv210-00.u18",	0x400000, 0x7486bf3a, 4 | BRF_GRA},            //  6 Foreground Tiles

	{ "cv300-00.u4",	0x400000, 0xfbeda465, 5 | BRF_SND},            //  7 YMZ280b Samples
};

STDROMPICKEXT(cyvern, cyvern, skns)
STD_ROM_FN(cyvern)

static INT32 CyvernInit()
{
	sprite_kludge_x = 0;
	sprite_kludge_y = 2;
	speedhack_address = 0x604d3c8;
	speedhack_pc[0] = 0x402ebd4;

	return DrvInit(3 /* USA */);
}

struct BurnDriver BurnDrvCyvern = {
	"cyvern", NULL, "skns", NULL, "1998",
	"Cyvern (US)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_VERSHOOT, 0,
	NULL, cyvernRomInfo, cyvernRomName, NULL, NULL, CyvernInputInfo, CyvernDIPInfo,
	CyvernInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,  0x8000,
	240, 320, 3, 4
};


// Cyvern (Japan)

static struct BurnRomInfo cyvernjRomDesc[] = {
	{ "cvj-even.u10",	0x100000, 0x802fadb4, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "cvj-odd.u8",		0x100000, 0xf8a0fbdd, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "cv100-00.u24",	0x400000, 0xcd4ae88a, 2 | BRF_GRA},            //  2 Sprites
	{ "cv101-00.u20",	0x400000, 0xa6cb3f0b, 2 | BRF_GRA},            //  3

	{ "cv200-00.u16",	0x400000, 0xddc8c67e, 3 | BRF_GRA},            //  4 Background Tiles
	{ "cv201-00.u13",	0x400000, 0x65863321, 3 | BRF_GRA},            //  5

	{ "cv210-00.u18",	0x400000, 0x7486bf3a, 4 | BRF_GRA},            //  6 Foreground Tiles

	{ "cv300-00.u4",	0x400000, 0xfbeda465, 5 | BRF_SND},            //  7 YMZ280b Samples
};

STDROMPICKEXT(cyvernj, cyvernj, skns)
STD_ROM_FN(cyvernj)

static INT32 CyvernJInit()
{
	sprite_kludge_x = 0;
	sprite_kludge_y = 2;
	speedhack_address = 0x604d3c8;
	speedhack_pc[0] = 0x402ebd4;

	return DrvInit(0 /* Japan */);
}

struct BurnDriver BurnDrvCyvernJ = {
	"cyvernj", "cyvern", "skns", NULL, "1998",
	"Cyvern (Japan)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_VERSHOOT, 0,
	NULL, cyvernjRomInfo, cyvernjRomName, NULL, NULL, CyvernInputInfo, CyvernDIPInfo,
	CyvernJInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,  0x8000,
	240, 320, 3, 4
};

// Guts'n (Japan)

static struct BurnRomInfo gutsnRomDesc[] = {
	{ "gts000j0.u6",	0x080000, 0x8ee91310, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "gts001j0.u4",	0x080000, 0x80b8ee66, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gts10000.u24",	0x400000, 0x1959979e, 2 | BRF_GRA },           //  2 Sprites

	{ "gts20000.u16",	0x400000, 0xc443aac3, 3 | BRF_GRA },           //  3 Background Tiles

	{ "gts30000.u4",	0x400000, 0x8c169141, 5 | BRF_SND },           //  4 YMZ280b Samples
};

STDROMPICKEXT(gutsn, gutsn, skns)
STD_ROM_FN(gutsn)

static INT32 GutsnInit()
{
	sprite_kludge_x = 0;
	sprite_kludge_y = 0;
	speedhack_address = 0x600c780;
	speedhack_pc[0] = 0x4022070; //number from mame + 0x02

	return DrvInit(0 /*japan*/);
}

struct BurnDriver BurnDrvGutsn = {
	"gutsn", NULL, "skns", NULL, "2000",
	"Guts'n (Japan)\0", NULL, "Kaneko / Kouyousha", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, gutsnRomInfo, gutsnRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo,
	GutsnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,  0x8000,
	320, 240, 4, 3
};


// Sengeki Striker (Asia)

static struct BurnRomInfo sengekisRomDesc[] = {
	{ "ss01a.u6",		0x080000, 0x962fe857, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "ss01a.u4",		0x080000, 0xee853c23, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ss100-00.u21",	0x400000, 0xbc7b3dfa, 2 | BRF_GRA},            //  2 Sprites
	{ "ss101-00.u20",	0x400000, 0xab2df280, 2 | BRF_GRA},            //  3
	{ "ss102-00.u8",	0x400000, 0x0845eafe, 2 | BRF_GRA},            //  4
	{ "ss103-00.u32",	0x400000, 0xee451ac9, 2 | BRF_GRA},            //  5

	{ "ss200-00.u17",	0x400000, 0xcd773976, 3 | BRF_GRA},            //  6 Background Tiles
	{ "ss201-00.u9",	0x400000, 0x301fad4c, 3 | BRF_GRA},            //  7

	{ "ss210-00.u3",	0x200000, 0xc3697805, 4 | BRF_GRA},            //  8 Foreground Tiles

	{ "ss300-00.u1",	0x400000, 0x35b04b18, 5 | BRF_SND},            //  9 YMZ280b Samples
};

STDROMPICKEXT(sengekis, sengekis, skns)
STD_ROM_FN(sengekis)

static INT32 SengekisInit()
{
	sprite_kludge_x = -192;
	sprite_kludge_y = -272;

	speedhack_address = 0x60b74bc;
	speedhack_pc[0] = 0x60006ec + 2;
	sixtyhz = 1;

	return DrvInit(2 /*asia*/);
}

struct BurnDriver BurnDrvSengekis = {
	"sengekis", NULL, "skns", NULL, "1997",
	"Sengeki Striker (Asia)\0", NULL, "Kaneko / Warashi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_VERSHOOT, 0,
	NULL, sengekisRomInfo, sengekisRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo,
	SengekisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,  0x8000,
	240, 320, 3, 4
};

// Sengeki Striker (Japan)

static struct BurnRomInfo sengekisjRomDesc[] = {
	{ "ss01j.u6",		0x080000, 0x9efdcd5a, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "ss01j.u4",		0x080000, 0x92c3f45e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ss100-00.u21",	0x400000, 0xbc7b3dfa, 2 | BRF_GRA },           //  2 Sprites
	{ "ss101-00.u20",	0x400000, 0xab2df280, 2 | BRF_GRA },           //  3
	{ "ss102-00.u8",	0x400000, 0x0845eafe, 2 | BRF_GRA },           //  4
	{ "ss103-00.u32",	0x400000, 0xee451ac9, 2 | BRF_GRA },           //  5

	{ "ss200-00.u17",	0x400000, 0xcd773976, 3 | BRF_GRA },           //  6 Background Tiles
	{ "ss201-00.u9",	0x400000, 0x301fad4c, 3 | BRF_GRA },           //  7

	{ "ss210-00.u3",	0x200000, 0xc3697805, 4 | BRF_GRA },           //  8 Foreground Tiles

	{ "ss300-00.u1",	0x400000, 0x35b04b18, 5 | BRF_SND },           //  9 YMZ280b Samples
};

STDROMPICKEXT(sengekisj, sengekisj, skns)
STD_ROM_FN(sengekisj)

static INT32 SengekisjInit()
{
	sprite_kludge_x = -192;
	sprite_kludge_y = -272;

	speedhack_address = 0x60b7380;
	speedhack_pc[0] = 0x60006ec + 2;
	sixtyhz = 1;

	return DrvInit(0 /*japan*/);
}

struct BurnDriver BurnDrvSengekisj = {
	"sengekisj", "sengekis", "skns", NULL, "1997",
	"Sengeki Striker (Japan)\0", NULL, "Kaneko / Warashi", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_VERSHOOT, 0,
	NULL, sengekisjRomInfo, sengekisjRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo,
	SengekisjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,  0x8000,
	240, 320, 3, 4
};


// Puzz Loop (Europe, v0.94)

static struct BurnRomInfo puzzloopRomDesc[] = {
	{ "pl00e4.u6",		0x080000, 0x7d3131a5, 1 | BRF_PRG | BRF_ESS }, //  1 SH2 Code
	{ "pl00e4.u4",		0x080000, 0x40dc3291, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pzl10000.u24",	0x400000, 0x35bf6897, 2 | BRF_GRA },           //  3 Sprites

	{ "pzl20000.u16",	0x400000, 0xff558e68, 3 | BRF_GRA },           //  4 Background Tiles

	{ "pzl21000.u18",	0x400000, 0xc8b3be64, 4 | BRF_GRA },           //  5 Foreground Tiles

	{ "pzl30000.u4",	0x400000, 0x38604b8d, 5 | BRF_SND },           //  6 YMZ280b Samples
};

STDROMPICKEXT(puzzloop, puzzloop, skns)
STD_ROM_FN(puzzloop)

static INT32 PuzzloopInit()
{
	sprite_kludge_x = -9;
	sprite_kludge_y = -1;

	speedhack_address = 0x6081d38;
	speedhack_pc[0] = 0x401dab0 + 2;

	return DrvInit(1 /*europe*/);
}

struct BurnDriver BurnDrvPuzzloop = {
	"puzzloop", NULL, "skns", NULL, "1998",
	"Puzz Loop (Europe, v0.94)\0", NULL, "Mitchell", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, puzzloopRomInfo, puzzloopRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //PuzzloopInputInfo, PuzzloopDIPInfo,
	PuzzloopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Puzz Loop (Europe, v0.93)

static struct BurnRomInfo puzzloopeRomDesc[] = {
	{ "pl00e1.u6",		0x080000, 0x273adc38, 1 | BRF_PRG | BRF_ESS }, //  1 SH2 Code
	{ "pl00e1.u4",		0x080000, 0x14ac2870, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pzl10000.u24",	0x400000, 0x35bf6897, 2 | BRF_GRA },           //  3 Sprites

	{ "pzl20000.u16",	0x400000, 0xff558e68, 3 | BRF_GRA },           //  4 Background Tiles

	{ "pzl21000.u18",	0x400000, 0xc8b3be64, 4 | BRF_GRA },           //  5 Foreground Tiles

	{ "pzl30000.u4",	0x400000, 0x38604b8d, 5 | BRF_SND },           //  6 YMZ280b Samples
};

STDROMPICKEXT(puzzloope, puzzloope, skns)
STD_ROM_FN(puzzloope)

struct BurnDriver BurnDrvPuzzloope = {
	"puzzloope", "puzzloop", "skns", NULL, "1998",
	"Puzz Loop (Europe, v0.93)\0", NULL, "Mitchell", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, puzzloopeRomInfo, puzzloopeRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //PuzzloopInputInfo, PuzzloopDIPInfo,
	PuzzloopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Puzz Loop (Japan)

static struct BurnRomInfo puzzloopjRomDesc[] = {
	{ "pl0j2.u6",		0x080000, 0x23c3bf97, 1 | BRF_PRG | BRF_ESS }, //  1 SH2 Code
	{ "pl0j2.u4",		0x080000, 0x55b2a3cb, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pzl10000.u24",	0x400000, 0x35bf6897, 2 | BRF_GRA },           //  3 Sprites

	{ "pzl20000.u16",	0x400000, 0xff558e68, 3 | BRF_GRA },           //  4 Background Tiles

	{ "pzl21000.u18",	0x400000, 0xc8b3be64, 4 | BRF_GRA },           //  5 Foreground Tiles

	{ "pzl30000.u4",	0x400000, 0x38604b8d, 5 | BRF_SND },           //  6 YMZ280b Samples
};

STDROMPICKEXT(puzzloopj, puzzloopj, skns)
STD_ROM_FN(puzzloopj)

static INT32 PuzzloopjInit()
{
	sprite_kludge_x = -9;
	sprite_kludge_y = -1;

	speedhack_address = 0x6086714;
	speedhack_pc[0] = 0x401dca0 + 2;

	return DrvInit(0 /*japan*/);
}

struct BurnDriver BurnDrvPuzzloopj = {
	"puzzloopj", "puzzloop", "skns", NULL, "1998",
	"Puzz Loop (Japan)\0", NULL, "Mitchell", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, puzzloopjRomInfo, puzzloopjRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //PuzzloopInputInfo, PuzzloopDIPInfo,
	PuzzloopjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Puzz Loop (Asia)

static struct BurnRomInfo puzzloopaRomDesc[] = {
	{ "pl0a3.u6",		0x080000, 0x4e8673b8, 1 | BRF_PRG | BRF_ESS }, //  1 SH2 Code
	{ "pl0a3.u4",		0x080000, 0xe08a1a07, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pzl10000.u24",	0x400000, 0x35bf6897, 2 | BRF_GRA },           //  3 Sprites

	{ "pzl20000.u16",	0x400000, 0xff558e68, 3 | BRF_GRA },           //  4 Background Tiles

	{ "pzl21000.u18",	0x400000, 0xc8b3be64, 4 | BRF_GRA },           //  5 Foreground Tiles

	{ "pzl30000.u4",	0x400000, 0x38604b8d, 5 | BRF_SND },           //  6 YMZ280b Samples
};

STDROMPICKEXT(puzzloopa, puzzloopa, skns)
STD_ROM_FN(puzzloopa)

static INT32 PuzzloopaInit()
{
	sprite_kludge_x = -9;
	sprite_kludge_y = -1;

	speedhack_address = 0x6085bcc;
	speedhack_pc[0] = 0x401d9d4 + 2;

	return DrvInit(2 /*asia*/);
}

struct BurnDriver BurnDrvPuzzloopa = {
	"puzzloopa", "puzzloop", "skns", NULL, "1998",
	"Puzz Loop (Asia)\0", NULL, "Mitchell", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, puzzloopaRomInfo, puzzloopaRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //PuzzloopInputInfo, PuzzloopDIPInfo,
	PuzzloopaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Puzz Loop (Korea)

static struct BurnRomInfo puzzloopkRomDesc[] = {
	{ "pl0k4.u6",		0x080000, 0x8d81f20c, 1 | BRF_PRG | BRF_ESS }, //  1 SH2 Code
	{ "pl0k4.u4",		0x080000, 0x17c78e41, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pzl10000.u24",	0x400000, 0x35bf6897, 2 | BRF_GRA },           //  3 Sprites

	{ "pzl20000.u16",	0x400000, 0xff558e68, 3 | BRF_GRA },           //  4 Background Tiles

	{ "pzl21000.u18",	0x400000, 0xc8b3be64, 4 | BRF_GRA },           //  5 Foreground Tiles

	{ "pzl30000.u4",	0x400000, 0x38604b8d, 5 | BRF_SND },           //  6 YMZ280b Samples
};

STDROMPICKEXT(puzzloopk, puzzloopk, skns)
STD_ROM_FN(puzzloopk)

static INT32 PuzzloopkInit()
{
	sprite_kludge_x = -9;
	sprite_kludge_y = -1;

//	speedhack_address = 0x6081d38;
//	speedhack_pc[0] = 0x401dab0 + 2;

	return DrvInit(4 /*korea*/);
}

struct BurnDriver BurnDrvPuzzloopk = {
	"puzzloopk", "puzzloop", "skns", NULL, "1998",
	"Puzz Loop (Korea)\0", NULL, "Mitchell", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, puzzloopkRomInfo, puzzloopkRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //PuzzloopInputInfo, PuzzloopDIPInfo,
	PuzzloopkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Puzz Loop (USA)

static struct BurnRomInfo puzzloopuRomDesc[] = {
	{ "plue5.u6",		0x080000, 0xe6f3f82f, 1 | BRF_PRG | BRF_ESS }, //  1 SH2 Code
	{ "plue5.u4",		0x080000, 0x0d081d30, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "pzl10000.u24",	0x400000, 0x35bf6897, 2 | BRF_GRA },           //  3 Sprites

	{ "pzl20000.u16",	0x400000, 0xff558e68, 3 | BRF_GRA },           //  4 Background Tiles

	{ "pzl21000.u18",	0x400000, 0xc8b3be64, 4 | BRF_GRA },           //  5 Foreground Tiles

	{ "pzl30000.u4",	0x400000, 0x38604b8d, 5 | BRF_SND },           //  6 YMZ280b Samples
};

STDROMPICKEXT(puzzloopu, puzzloopu, skns)
STD_ROM_FN(puzzloopu)

static INT32 PuzzloopuInit()
{
	sprite_kludge_x = -9;
	sprite_kludge_y = -1;

	speedhack_address = 0x6085cec;
	speedhack_pc[0] = 0x401dab0 + 2;

	return DrvInit(3 /*usa*/);
}

struct BurnDriver BurnDrvPuzzloopu = {
	"puzzloopu", "puzzloop", "skns", NULL, "1998",
	"Puzz Loop (USA)\0", NULL, "Mitchell", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, puzzloopuRomInfo, puzzloopuRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //PuzzloopInputInfo, PuzzloopDIPInfo,
	PuzzloopuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Tel Jan

static struct BurnRomInfo teljanRomDesc[] = {
	{ "tel1j.u10",		0x080000, 0x09b552fe, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "tel1j.u8",		0x080000, 0x070b4345, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "tj100-00.u24",	0x400000, 0x810144f1, 2 | BRF_GRA },           //  2 Sprites
	{ "tj101-00.u20",	0x400000, 0x82f570e1, 2 | BRF_GRA },           //  3
	{ "tj102-00.u17",	0x400000, 0xace875dc, 2 | BRF_GRA },           //  4

	{ "tj200-00.u16",	0x400000, 0xbe0f90b2, 3 | BRF_GRA },           //  5 Background Tiles

	{ "tj300-00.u4",	0x400000, 0x685495c4, 5 | BRF_SND },           //  6 YMZ280b Samples
};

STDROMPICKEXT(teljan, teljan, skns)
STD_ROM_FN(teljan)

static INT32 TeljanInit()
{
	sprite_kludge_x = 5;
	sprite_kludge_y = 1;

	speedhack_address = 0x6002fb4;
	speedhack_pc[0] = 0x401ba32 + 2;

	return DrvInit(0 /*japan*/);
}

struct BurnDriver BurnDrvTeljan = {
	"teljan",  NULL, "skns",NULL, "1999",
	"Tel Jan\0", NULL, "Electro Design", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_KANEKO_SKNS, GBF_MAHJONG, 0,
	NULL, teljanRomInfo, teljanRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //Skns_1pInputInfo, Skns_1pDIPInfo,
	TeljanInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Panic Street (Japan)

static struct BurnRomInfo panicstrRomDesc[] = {
	{ "ps1000j0.u10",	0x100000, 0x59645f89, 1 | BRF_PRG | BRF_ESS }, //  1 SH2 Code
	{ "ps1001j0.u8",	0x100000, 0xc4722be9, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "ps-10000.u24",	0x400000, 0x294b2f14, 2 | BRF_GRA },           //  3 Sprites
	{ "ps110100.u20",	0x400000, 0xe292f393, 2 | BRF_GRA },           //  4

	{ "ps120000.u16",	0x400000, 0xd772ac15, 3 | BRF_GRA },           //  5 Background Tiles

	{ "ps-30000.u4",	0x400000, 0x2262e263, 5 | BRF_SND },           //  6 YMZ280b Samples
};

STDROMPICKEXT(panicstr, panicstr, skns)
STD_ROM_FN(panicstr)

static INT32 PanicstrInit()
{
	sprite_kludge_x = -1;
	sprite_kludge_y = -1;

	speedhack_address = 0x60f19e4;
	speedhack_pc[0] = 0x404e68a + 2;

	return DrvInit(0 /*japan*/);
}

struct BurnDriver BurnDrvPanicstr = {
	"panicstr", NULL, "skns", NULL, "1999",
	"Panic Street (Japan)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, panicstrRomInfo, panicstrRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //GalpanisInputInfo, GalpanisDIPInfo,
	PanicstrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Gals Panic 4 (Japan)

static struct BurnRomInfo galpani4RomDesc[] = {
	{ "gp4j1.u10",		0x080000, 0x919a3893, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "gp4j1.u8",		0x080000, 0x94cb1fb7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gp4-100-00.u24",	0x200000, 0x1df61f01, 2 | BRF_GRA },           //  2 Sprites
	{ "gp4-101-00.u20",	0x100000, 0x8e2c9349, 2 | BRF_GRA },           //  3

	{ "gp4-200-00.u16",	0x200000, 0xf0781376, 3 | BRF_GRA },           //  4 Background Tiles
	{ "gp4-201-00.u18",	0x200000, 0x10c4b183, 3 | BRF_GRA },           //  5

	{ "gp4-300-00.u4",	0x200000, 0x8374663a, 5 | BRF_SND },           //  6 YMZ280b Samples
};

STDROMPICKEXT(galpani4, galpani4, skns)
STD_ROM_FN(galpani4)

static INT32 Galpani4Init()
{
	sprite_kludge_x = -5;
	sprite_kludge_y = -1;

	return DrvInit(0 /*Japan*/);
}

struct BurnDriver BurnDrvGalpani4 = {
	"galpani4", NULL, "skns", NULL, "1996",
	"Gals Panic 4 (Japan)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, galpani4RomInfo, galpani4RomName, NULL, NULL, CyvernInputInfo, CyvernNoSpeedhackDIPInfo,
	Galpani4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Gals Panic 4 (Korea)

static struct BurnRomInfo galpani4kRomDesc[] = {
	{ "gp4k1.u10",		0x080000, 0xcbd5c3a0, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "gp4k1.u8",		0x080000, 0x7a95bfe2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gp4-100-00.u24",	0x200000, 0x1df61f01, 2 | BRF_GRA },           //  2 Sprites
	{ "gp4-101-00.u20",	0x100000, 0x8e2c9349, 2 | BRF_GRA },           //  3

	{ "gp4-200-00.u16",	0x200000, 0xf0781376, 3 | BRF_GRA },           //  4 Background Tiles
	{ "gp4-201-00.u18",	0x200000, 0x10c4b183, 3 | BRF_GRA },           //  5

	{ "gp4-300-00.u4",	0x200000, 0x8374663a, 5 | BRF_SND },           //  6 YMZ280b Samples
	{ "gp4-301-01.u7",	0x200000, 0x886ef77f, 5 | BRF_SND },           //  7
};

STDROMPICKEXT(galpani4k, galpani4k, skns)
STD_ROM_FN(galpani4k)

static INT32 Galpani4kInit()
{
	sprite_kludge_x = -5;
	sprite_kludge_y = -1;

	return DrvInit(4 /*Korea*/);
}

struct BurnDriver BurnDrvGalpani4k = {
	"galpani4k", "galpani4", "skns", NULL, "1996",
	"Gals Panic 4 (Korea)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, galpani4kRomInfo, galpani4kRomName, NULL, NULL, CyvernInputInfo, CyvernNoSpeedhackDIPInfo,
	Galpani4kInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Gals Panic S - Extra Edition (Europe)

static struct BurnRomInfo galpanisRomDesc[] = {
	{ "gps-000-e1.u10",	0x100000, 0xb9ea3c44, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "gps-001-e1.u8",	0x100000, 0xded57bd0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gps10000.u24",	0x400000, 0xa1a7acf2, 2 | BRF_GRA },           //  2 Sprites
	{ "gps10100.u20",	0x400000, 0x49f764b6, 2 | BRF_GRA },           //  3
	{ "gps10200.u17",	0x400000, 0x51980272, 2 | BRF_GRA },           //  4

	{ "gps20000.u16",	0x400000, 0xc146a09e, 3 | BRF_GRA },           //  5 Background Tiles
	{ "gps20100.u13",	0x400000, 0x9dfa2dc6, 3 | BRF_GRA },           //  6

	{ "gps30000.u4",	0x400000, 0x9e4da8e3, 5 | BRF_SND },           //  7 YMZ280b Samples
};

STDROMPICKEXT(galpanis, galpanis, skns)
STD_ROM_FN(galpanis)

static INT32 GalpanisInit()
{
	sprite_kludge_x = -5;
	sprite_kludge_y = -1;

	return DrvInit(1 /*Europe*/);
}

struct BurnDriver BurnDrvGalpanis = {
	"galpanis", NULL, "skns", NULL, "1997",
	"Gals Panic S - Extra Edition (Europe)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, galpanisRomInfo, galpanisRomName, NULL, NULL, SknsInputInfo, SknsNoSpeedhackDIPInfo,
	GalpanisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Gals Panic S - Extra Edition (Japan)

static struct BurnRomInfo galpanisjRomDesc[] = {
	{ "gps-000-j1.u10",	0x100000, 0xc6938c3f, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "gps-001-j1.u8",	0x100000, 0xe764177a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gps10000.u24",	0x400000, 0xa1a7acf2, 2 | BRF_GRA },           //  2 Sprites
	{ "gps10100.u20",	0x400000, 0x49f764b6, 2 | BRF_GRA },           //  3
	{ "gps10200.u17",	0x400000, 0x51980272, 2 | BRF_GRA },           //  4

	{ "gps20000.u16",	0x400000, 0xc146a09e, 3 | BRF_GRA },           //  5 Background Tiles
	{ "gps20100.u13",	0x400000, 0x9dfa2dc6, 3 | BRF_GRA },           //  6

	{ "gps30000.u4",	0x400000, 0x9e4da8e3, 5 | BRF_SND },           //  7 YMZ280b Samples
};

STDROMPICKEXT(galpanisj, galpanisj, skns)
STD_ROM_FN(galpanisj)

static INT32 GalpanisjInit()
{
	sprite_kludge_x = -5;
	sprite_kludge_y = -1;

	return DrvInit(0 /*Japan*/);
}

struct BurnDriver BurnDrvGalpanisj = {
	"galpanisj", "galpanis", "skns", NULL, "1997",
	"Gals Panic S - Extra Edition (Japan)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, galpanisjRomInfo, galpanisjRomName, NULL, NULL, SknsInputInfo, SknsNoSpeedhackDIPInfo,
	GalpanisjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Gals Panic S - Extra Edition (Korea)

static struct BurnRomInfo galpaniskRomDesc[] = {
	{ "gps-000-k1.u10",	0x100000, 0xc9ff3d8a, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "gps-001-k1.u8",	0x100000, 0x354e601d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gps10000.u24",	0x400000, 0xa1a7acf2, 2 | BRF_GRA },           //  2 Sprites
	{ "gps10100.u20",	0x400000, 0x49f764b6, 2 | BRF_GRA },           //  3
	{ "gps10200.u17",	0x400000, 0x51980272, 2 | BRF_GRA },           //  4

	{ "gps20000.u16",	0x400000, 0xc146a09e, 3 | BRF_GRA },           //  5 Background Tiles
	{ "gps20100.u13",	0x400000, 0x9dfa2dc6, 3 | BRF_GRA },           //  6

	{ "gps30000.u4",	0x400000, 0x9e4da8e3, 5 | BRF_SND },           //  7 YMZ280b Samples
};

STDROMPICKEXT(galpanisk, galpanisk, skns)
STD_ROM_FN(galpanisk)

static INT32 GalpaniskInit()
{
	sprite_kludge_x = -5;
	sprite_kludge_y = -1;

	return DrvInit(4 /*Korea*/);
}

struct BurnDriver BurnDrvGalpanisk = {
	"galpanisk", "galpanis", "skns", NULL, "1997",
	"Gals Panic S - Extra Edition (Korea)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, galpaniskRomInfo, galpaniskRomName, NULL, NULL, SknsInputInfo, SknsNoSpeedhackDIPInfo,
	GalpaniskInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Gals Panic S2 (Europe)
// only the 2 program ROMs were dumped, but mask ROMs are supposed to match.

static struct BurnRomInfo galpans2RomDesc[] = {
	{ "GPS2E_U6__Ver.3.u6",		0x100000, 0x72fff5d1, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "GPS2E_U4__Ver.3.u4",		0x100000, 0x95061601, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gs210000.u21",	0x400000, 0x294b2f14, 2 | BRF_GRA },           //  2 Sprites
	{ "gs210100.u20",	0x400000, 0xf75c5a9a, 2 | BRF_GRA },           //  3
	{ "gs210200.u8",	0x400000, 0x25b4f56b, 2 | BRF_GRA },           //  4
	{ "gs210300.u32",	0x400000, 0xdb6d4424, 2 | BRF_GRA },           //  5

	{ "gs220000.u17",	0x400000, 0x5caae1c0, 3 | BRF_GRA },           //  6 Background Tiles
	{ "gs220100.u9",	0x400000, 0x8d51f197, 3 | BRF_GRA },           //  7

	{ "gs221000.u3",	0x400000, 0x58800a18, 4 | BRF_GRA },           //  8 Foreground Tiles

	{ "gs230000.u1",	0x400000, 0x0348e8e1, 5 | BRF_SND },           //  9 YMZ280b Samples
};

STDROMPICKEXT(galpans2, galpans2, skns)
STD_ROM_FN(galpans2)

static INT32 Galpans2Init()
{
	sprite_kludge_x = -1;
	sprite_kludge_y = -1;

	speedhack_address = 0x60fb6bc;
	speedhack_pc[0] = 0x4049ae2 + 2;

	return DrvInit(1 /*Europe*/);
}

struct BurnDriver BurnDrvGalpans2 = {
	"galpans2", NULL, "skns", NULL, "1999",
	"Gals Panic S2 (Europe)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, galpans2RomInfo, galpans2RomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //GalpanisInputInfo, GalpanisDIPInfo,
	Galpans2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Gals Panic S2 (Japan)

static struct BurnRomInfo galpans2jRomDesc[] = {
	{ "gps2j.u6",		0x100000, 0x6e74005b, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "gps2j.u4",		0x100000, 0x9b4b2304, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gs210000.u21",	0x400000, 0x294b2f14, 2 | BRF_GRA },           //  2 Sprites
	{ "gs210100.u20",	0x400000, 0xf75c5a9a, 2 | BRF_GRA },           //  3
	{ "gs210200.u8",	0x400000, 0x25b4f56b, 2 | BRF_GRA },           //  4
	{ "gs210300.u32",	0x400000, 0xdb6d4424, 2 | BRF_GRA },           //  5

	{ "gs220000.u17",	0x400000, 0x5caae1c0, 3 | BRF_GRA },           //  6 Background Tiles
	{ "gs220100.u9",	0x400000, 0x8d51f197, 3 | BRF_GRA },           //  7

	{ "gs221000.u3",	0x400000, 0x58800a18, 4 | BRF_GRA },           //  8 Foreground Tiles

	{ "gs230000.u1",	0x400000, 0x0348e8e1, 5 | BRF_SND },           //  9 YMZ280b Samples
};

STDROMPICKEXT(galpans2j, galpans2j, skns)
STD_ROM_FN(galpans2j)

static INT32 Galpans2jInit()
{
	sprite_kludge_x = -1;
	sprite_kludge_y = -1;

	speedhack_address = 0x60fb6bc;
	speedhack_pc[0] = 0x4049ae2 + 2;

	return DrvInit(0 /*Japan*/);
}

struct BurnDriver BurnDrvGalpans2j = {
	"galpans2j", "galpans2", "skns", NULL, "1999",
	"Gals Panic S2 (Japan)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, galpans2jRomInfo, galpans2jRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //GalpanisInputInfo, GalpanisDIPInfo,
	Galpans2jInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Gals Panic S2 (Asia)

static struct BurnRomInfo galpans2aRomDesc[] = {
	{ "gps2av11.u6",	0x100000, 0x61c05d5f, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "gps2av11.u4",	0x100000, 0x2e8c0ac2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gs210000.u21",	0x400000, 0x294b2f14, 2 | BRF_GRA },           //  2 Sprites
	{ "gs210100.u20",	0x400000, 0xf75c5a9a, 2 | BRF_GRA },           //  3
	{ "gs210200.u8",	0x400000, 0x25b4f56b, 2 | BRF_GRA },           //  4
	{ "gs210300.u32",	0x400000, 0xdb6d4424, 2 | BRF_GRA },           //  5

	{ "gs220000.u17",	0x400000, 0x5caae1c0, 3 | BRF_GRA },           //  6 Background Tiles
	{ "gs220100.u9",	0x400000, 0x8d51f197, 3 | BRF_GRA },           //  7

	{ "gs221000.u3",	0x400000, 0x58800a18, 4 | BRF_GRA },           //  8 Foreground Tiles

	{ "gs230000.u1",	0x400000, 0x0348e8e1, 5 | BRF_SND },           //  9 YMZ280b Samples
};

STDROMPICKEXT(galpans2a, galpans2a, skns)
STD_ROM_FN(galpans2a)

static INT32 Galpans2aInit()
{
	sprite_kludge_x = -1;
	sprite_kludge_y = -1;

	speedhack_address = 0x60fb6bc;
	speedhack_pc[0] = 0x4049ae2 + 2;

	return DrvInit(2 /*Asia*/);
}

struct BurnDriver BurnDrvGalpans2a = {
	"galpans2a", "galpans2", "skns", NULL, "1999",
	"Gals Panic S2 (Asia)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, galpans2aRomInfo, galpans2aRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //GalpanisInputInfo, GalpanisDIPInfo,
	Galpans2aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Gals Panic SU (Korea)

static struct BurnRomInfo galpansuRomDesc[] = {
	{ "su.u10",		0x100000, 0x5ae66218, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "su.u8",		0x100000, 0x10977a03, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "24",			0x400000, 0x294b2f14, 2 | BRF_GRA },           //  2 Sprites
	{ "20",			0x400000, 0xf75c5a9a, 2 | BRF_GRA },           //  3
	{ "17",			0x400000, 0x25b4f56b, 2 | BRF_GRA },           //  4
	{ "32",			0x400000, 0xdb6d4424, 2 | BRF_GRA },           //  5

	{ "16",			0x400000, 0x5caae1c0, 3 | BRF_GRA },           //  6 Background Tiles
	{ "13",			0x400000, 0x8d51f197, 3 | BRF_GRA },           //  7

	{ "7",			0x400000, 0x58800a18, 4 | BRF_GRA },           //  8 Foreground Tiles

	{ "4",			0x400000, 0x0348e8e1, 5 | BRF_SND },           //  9 YMZ280b Samples

#if !defined (ROM_VERIFY)
	{ "bios.u10",		0x80000, 0x161fb79e, BRF_BIOS | BRF_OPT},      //  10 Korea Bios
#endif
};

STDROMPICKEXT(galpansu, galpansu, skns)
STD_ROM_FN(galpansu)

static INT32 GalpansuInit()
{
	sprite_kludge_x = -1;
	sprite_kludge_y = -1;

	speedhack_address = 0x60fb6bc;
	speedhack_pc[0] = 0x4049ae2 + 2;

	return DrvInit(4 /*Korea*/);
}

struct BurnDriver BurnDrvGalpansu = {
	"galpansu", "galpans2", "skns", NULL, "1999",
	"Gals Panic SU (Korea)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, galpansuRomInfo, galpansuRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //GalpanisInputInfo, GalpanisDIPInfo,
	GalpansuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Gals Panic S3 (Japan)

static struct BurnRomInfo galpans3RomDesc[] = {
	{ "gpss3.u10",		0x100000, 0xc1449a72, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "gpss3.u8",		0x100000, 0x11eb44cf, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u24.bin",		0x800000, 0x70613168, 2 | BRF_GRA },           //  2 Sprites

	{ "u16.bin",		0x800000, 0xa96daf2a, 3 | BRF_GRA },           //  3 Background Tiles
 
	{ "u4.bin",		0x400000, 0xbf5736c6, 5 | BRF_SND },           //  4 YMZ280b Samples
};

STDROMPICKEXT(galpans3, galpans3, skns)
STD_ROM_FN(galpans3)

static INT32 Galpans3Init()
{
	sprite_kludge_x = -1;
	sprite_kludge_y = -1;

	return DrvInit(0 /*Japan*/);
}

struct BurnDriver BurnDrvGalpans3 = {
	"galpans3", NULL, "skns", NULL, "2002",
	"Gals Panic S3 (Japan)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, galpans3RomInfo, galpans3RomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //GalpanisInputInfo, GalpanisDIPInfo,
	Galpans3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Jan Jan Paradise

static struct BurnRomInfo jjparadsRomDesc[] = {
	{ "jp1j1.u10",		0x080000, 0xde2fb669, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "jp1j1.u8",		0x080000, 0x7276efb1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "jp100-00.u24",	0x400000, 0xf31b2e95, 2 | BRF_GRA },           //  2 Sprites
	{ "jp101-00.u20",	0x400000, 0x70cc8c24, 2 | BRF_GRA },           //  3
	{ "jp102-00.u17",	0x400000, 0x35401c1e, 2 | BRF_GRA },           //  4

	{ "jp200-00.u16",	0x200000, 0x493d63db, 3 | BRF_GRA },           //  5 Background Tiles

	{ "jp300-00.u4",	0x200000, 0x7023fe46, 5 | BRF_SND },           //  6 YMZ280b Samples
};

STDROMPICKEXT(jjparads, jjparads, skns)
STD_ROM_FN(jjparads)

static INT32 JjparadsInit()
{
	sprite_kludge_x = 5;
	sprite_kludge_y = 1;

//	speedhack_address = 0x6000994;
//	speedhack_pc[0] = 0x4015e84 + 2;

	return DrvInit(0 /*Japan*/);
}

struct BurnDriver BurnDrvJjparads = {
	"jjparads", NULL, "skns", NULL, "1996",
	"Jan Jan Paradise\0", NULL, "Electro Design", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_KANEKO_SKNS, GBF_MAHJONG, 0,
	NULL, jjparadsRomInfo, jjparadsRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //Skns_1pInputInfo, Skns_1pDIPInfo,
	JjparadsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Jan Jan Paradise 2

static struct BurnRomInfo jjparad2RomDesc[] = {
	{ "jp2000j1.u6",	0x080000, 0x5d75e765, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "jp2001j1.u4",	0x080000, 0x1771910a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "jp210000.u21",	0x400000, 0x79a7e3d7, 2 | BRF_GRA },           //  2 Sprites
	{ "jp210100.u20",	0x400000, 0x42415e0c, 2 | BRF_GRA },           //  3
	{ "jp210200.u8",	0x400000, 0x26731745, 2 | BRF_GRA },           //  4

	{ "jp220000.u17",	0x400000, 0xd0e71873, 3 | BRF_GRA },           //  5 Background Tiles
	{ "jp220100.u9",	0x400000, 0x4c7d964d, 3 | BRF_GRA },           //  6

	{ "jp230000.u1",	0x400000, 0x73e30d7f, 5 | BRF_SND },           //  7 YMZ280b Samples
};

STDROMPICKEXT(jjparad2, jjparad2, skns)
STD_ROM_FN(jjparad2)

static INT32 Jjparad2Init()
{
	sprite_kludge_x = 5;
	sprite_kludge_y = 1;

//	speedhack_address = 0x6000994;
//	speedhack_pc[0] = 0x401620a + 2;

	return DrvInit(0 /*Japan*/);
}

struct BurnDriver BurnDrvJjparad2 = {
	"jjparad2", NULL, "skns", NULL, "1997",
	"Jan Jan Paradise 2\0", NULL, "Electro Design", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_KANEKO_SKNS, GBF_MAHJONG, 0,
	NULL, jjparad2RomInfo, jjparad2RomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //Skns_1pInputInfo, Skns_1pDIPInfo,
	Jjparad2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Sen-Know (Japan)

static struct BurnRomInfo senknowRomDesc[] = {
	{ "snw000j1.u6",	0x080000, 0x0d6136f6, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "snw001j1.u4",	0x080000, 0x4a10ec3d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "snw10000.u21",	0x400000, 0x5133c69c, 2 | BRF_GRA },           //  2 Sprites
	{ "snw10100.u20",	0x400000, 0x9dafe03f, 2 | BRF_GRA },           //  3

	{ "snw20000.u17",	0x400000, 0xd5fe5f8c, 3 | BRF_GRA },           //  4 Background Tiles
	{ "snw20100.u9",	0x400000, 0xc0037846, 3 | BRF_GRA },           //  5

	{ "snw21000.u3",	0x400000, 0xf5c23e79, 4 | BRF_GRA },           //  6 Foreground Tiles

	{ "snw30000.u1",	0x400000, 0xec9eef40, 5 | BRF_SND },           //  7 YMZ280b Samples
};

STDROMPICKEXT(senknow, senknow, skns)
STD_ROM_FN(senknow)

static INT32 SenknowInit()
{
	sprite_kludge_x = 1;
	sprite_kludge_y = 1;

	speedhack_address = 0x60000dc;
	speedhack_pc[0] = 0x4017dce + 2;

	return DrvInit(0 /*Japan*/);
}

struct BurnDriver BurnDrvSenknow = {
	"senknow", NULL, "skns", NULL, "1999",
	"Sen-Know (Japan)\0", NULL, "Kaneko / Kouyousha", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_PUZZLE, 0,
	NULL, senknowRomInfo, senknowRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //SknsInputInfo, SknsDIPInfo,
	SenknowInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// VS Mahjong Otome Ryouran

static struct BurnRomInfo ryouranRomDesc[] = {
	{ "or-000-j2.u10",	0x080000, 0xcba8ca4e, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "or-001-j2.u8",	0x080000, 0x8e79c6b7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "or100-00.u24",	0x400000, 0xe9c7695b, 2 | BRF_GRA },           //  2 Sprites
	{ "or101-00.u20",	0x400000, 0xfe06bf12, 2 | BRF_GRA },           //  3
	{ "or102-00.u17",	0x400000, 0xf2a5237b, 2 | BRF_GRA },           //  4

	{ "or200-00.u16",	0x400000, 0x4c4701a8, 3 | BRF_GRA },           //  5 Background Tiles
	{ "or201-00.u13",	0x400000, 0xa94064aa, 3 | BRF_GRA },           //  6

	{ "or300-00.u4",	0x400000, 0xa3f64b79, 5 | BRF_SND },           //  7 YMZ280b Samples
};

STDROMPICKEXT(ryouran, ryouran, skns)
STD_ROM_FN(ryouran)

static INT32 RyouranInit()
{
	sprite_kludge_x = 5;
	sprite_kludge_y = 1;

	speedhack_address = 0x6000a14;
	speedhack_pc[0] = 0x40182ce + 2;

	return DrvInit(0 /*Japan*/);
}

struct BurnDriver BurnDrvRyouran = {
	"ryouran", NULL, "skns", NULL, "1998",
	"VS Mahjong Otome Ryouran\0", NULL, "Electro Design", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_KANEKO_SKNS, GBF_MAHJONG, 0,
	NULL, ryouranRomInfo, ryouranRomName, NULL, NULL, SknsInputInfo, SknsDIPInfo, //Skns_1pInputInfo, Skns_1pDIPInfo,
	RyouranInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// VS Block Breaker (Europe)

static struct BurnRomInfo vblokbrkRomDesc[] = {
	{ "sk000e2-e.u10",	0x080000, 0x5a278f10, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "sk000e-o.u8",	0x080000, 0xaecf0647, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sk-100-00.u24",	0x200000, 0x151dd88a, 2 | BRF_GRA },           //  2 Sprites
	{ "sk-101.u20",		0x100000, 0x779cce23, 2 | BRF_GRA },           //  3

	{ "sk-200-00.u16",	0x200000, 0x2e297c61, 3 | BRF_GRA },           //  4 Background Tiles

	{ "sk-300-00.u4",	0x200000, 0xe6535c05, 5 | BRF_SND },           //  5 YMZ280b Samples
};

STDROMPICKEXT(vblokbrk, vblokbrk, skns)
STD_ROM_FN(vblokbrk)

static INT32 VblokbrkInit()
{
	sprite_kludge_x = -1;
	sprite_kludge_y = -1;
	suprnova_alt_enable_background = 1;
	Vblokbrk = 1;
	sixtyhz = 1;

	return DrvInit(1 /*Europe*/);
}

struct BurnDriver BurnDrvVblokbrk = {
	"vblokbrk", NULL, "skns", NULL, "1997",
	"VS Block Breaker (Europe)\0", NULL, "Kaneko / Mediaworks", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_BALLPADDLE, 0,
	NULL, vblokbrkRomInfo, vblokbrkRomName, NULL, NULL, VblokbrkInputInfo, VblokbrkDIPInfo,
	VblokbrkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// VS Block Breaker (Asia)

static struct BurnRomInfo vblokbrkaRomDesc[] = {
	{ "sk01a.u10",		0x080000, 0x4d1be53e, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "sk01a.u8",		0x080000, 0x461e0197, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sk-100-00.u24",	0x200000, 0x151dd88a, 2 | BRF_GRA },           //  2 Sprites
	{ "sk-101.u20",		0x100000, 0x779cce23, 2 | BRF_GRA },           //  3

	{ "sk-200-00.u16",	0x200000, 0x2e297c61, 3 | BRF_GRA },           //  4 Background Tiles

	{ "sk-300-00.u4",	0x200000, 0xe6535c05, 5 | BRF_SND },           //  5 YMZ280b Samples
};

STDROMPICKEXT(vblokbrka, vblokbrka, skns)
STD_ROM_FN(vblokbrka)

static INT32 VblokbrkaInit()
{
	sprite_kludge_x = -1;
	sprite_kludge_y = -1;
	suprnova_alt_enable_background = 1;
	Vblokbrk = 1;
	sixtyhz = 1;

	return DrvInit(2 /*Asia*/);
}

struct BurnDriver BurnDrvVblokbrka = {
	"vblokbrka", "vblokbrk", "skns", NULL, "1997",
	"VS Block Breaker (Asia)\0", NULL, "Kaneko / Mediaworks", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_BALLPADDLE, 0,
	NULL, vblokbrkaRomInfo, vblokbrkaRomName, NULL, NULL, VblokbrkInputInfo, VblokbrkDIPInfo,
	VblokbrkaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};


// Saru-Kani-Hamu-Zou (Japan)

static struct BurnRomInfo sarukaniRomDesc[] = {
	{ "sk1j1.u10",		0x080000, 0xfcc131b6, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "sk1j1.u8",		0x080000, 0x3b6aa343, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "sk-100-00.u24",	0x200000, 0x151dd88a, 2 | BRF_GRA },           //  2 Sprites
	{ "sk-101.u20",		0x100000, 0x779cce23, 2 | BRF_GRA },           //  3

	{ "sk-200-00.u16",	0x200000, 0x2e297c61, 3 | BRF_GRA },           //  4 Background Tiles

	{ "sk-300-00.u4",	0x200000, 0xe6535c05, 5 | BRF_SND },           //  5 YMZ280b Samples
};

STDROMPICKEXT(sarukani, sarukani, skns)
STD_ROM_FN(sarukani)

static INT32 SarukaniInit()
{
	sprite_kludge_x = -1;
	sprite_kludge_y = -1;
	suprnova_alt_enable_background = 1;
	Vblokbrk = 1;
	sixtyhz = 1;

	return DrvInit(0 /*Japan*/);
}

struct BurnDriver BurnDrvSarukani = {
	"sarukani", "vblokbrk", "skns", NULL, "1997",
	"Saru-Kani-Hamu-Zou (Japan)\0", NULL, "Kaneko / Mediaworks", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_KANEKO_SKNS, GBF_BALLPADDLE, 0,
	NULL, sarukaniRomInfo, sarukaniRomName, NULL, NULL, VblokbrkInputInfo, VblokbrkDIPInfo,
	SarukaniInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL, 0x8000,
	320, 240, 4, 3
};
