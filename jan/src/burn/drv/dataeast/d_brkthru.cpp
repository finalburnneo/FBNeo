// FB Alpha Break Thru driver module
// Based on MAME driver by Phil Stroffolino

// Tofix:
//   background layer in Breakthru has the wrong colors.

// Notes:
//  Due to our 6809 core being very cycle-inaccurate, our cpu's need an extra
//  1.5mhz tacked on to match the same performance as MAME running the same
//  game.  Todo: remove o/c when core is updated.

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "burn_ym2203.h"
#include "burn_ym3526.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvM6809RAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;
static UINT16 bgscroll;
static UINT8 bgbasecolor;
static UINT8 soundlatch;
static INT32 rombank;
static UINT8 nmi_mask;
static UINT8 vblank;
static UINT8 prevcoin;

static INT32 darwin = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo BrkthruInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 7,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Brkthru)


static struct BurnDIPInfo BrkthruDIPList[]=
{
	{0x12, 0xff, 0xff, 0x3f, NULL			},
	{0x13, 0xff, 0xff, 0x1f, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Enemy Vehicles"	},
	{0x12, 0x01, 0x10, 0x10, "Slow"			},
	{0x12, 0x01, 0x10, 0x00, "Fast"			},

	{0   , 0xfe, 0   ,    2, "Enemy Bullets"	},
	{0x12, 0x01, 0x20, 0x20, "Slow"			},
	{0x12, 0x01, 0x20, 0x00, "Fast"			},

	{0   , 0xfe, 0   ,    2, "Control Panel"	},
	{0x12, 0x01, 0x40, 0x40, "1 Player"		},
	{0x12, 0x01, 0x40, 0x00, "2 Players"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x80, 0x00, "Upright"		},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x00, "99 (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x0c, 0x00, "20000 Points Only"	},
	{0x13, 0x01, 0x0c, 0x04, "10000/20000 Points"	},
	{0x13, 0x01, 0x0c, 0x0c, "20000/30000 Points"	},
	{0x13, 0x01, 0x0c, 0x08, "20000/40000 Points"	},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x10, 0x00, "No"			},
	{0x13, 0x01, 0x10, 0x10, "Yes"			},
};

STDDIPINFO(Brkthru)


static struct BurnDIPInfo BrkthrujDIPList[]=
{
	{0x12, 0xff, 0xff, 0x3f, NULL			},
	{0x13, 0xff, 0xff, 0x1f, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Enemy Vehicles"	},
	{0x12, 0x01, 0x10, 0x10, "Slow"			},
	{0x12, 0x01, 0x10, 0x00, "Fast"			},

	{0   , 0xfe, 0   ,    2, "Enemy Bullets"	},
	{0x12, 0x01, 0x20, 0x20, "Slow"			},
	{0x12, 0x01, 0x20, 0x00, "Fast"			},

	{0   , 0xfe, 0   ,    2, "Control Panel"	},
	{0x12, 0x01, 0x40, 0x40, "1 Player"		},
	{0x12, 0x01, 0x40, 0x00, "2 Players"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x80, 0x00, "Upright"		},
	{0x12, 0x01, 0x80, 0x80, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x02, "2"			},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x00, "99 (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x0c, 0x00, "20000 Points Only"	},
	{0x13, 0x01, 0x0c, 0x04, "10000/20000 Points"	},
	{0x13, 0x01, 0x0c, 0x0c, "20000/30000 Points"	},
	{0x13, 0x01, 0x0c, 0x08, "20000/40000 Points"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x10, 0x10, "Off"			},
	{0x13, 0x01, 0x10, 0x00, "On"			},
};

STDDIPINFO(Brkthruj)

static struct BurnDIPInfo DarwinDIPList[]=
{
	{0x12, 0xff, 0xff, 0x4f, NULL			},
	{0x13, 0xff, 0xff, 0x1f, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x20, 0x00, "Upright"		},
	{0x12, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x40, 0x00, "Off"			},
	{0x12, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x13, 0x01, 0x01, 0x01, "3"			},
	{0x13, 0x01, 0x01, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x13, 0x01, 0x02, 0x02, "20k, 50k and every 50k"},
	{0x13, 0x01, 0x02, 0x00, "30k, 80k and every 80k"},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x0c, 0x0c, "Easy"			},
	{0x13, 0x01, 0x0c, 0x08, "Medium"		},
	{0x13, 0x01, 0x0c, 0x04, "Hard"			},
	{0x13, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x10, 0x00, "No"			},
	{0x13, 0x01, 0x10, 0x10, "Yes"			},
};

STDDIPINFO(Darwin)

static void bankswitch(INT32 data)
{
	rombank = data & 7;
	INT32 bank = 0x10000 + rombank * 0x2000;

	M6809MapMemory(DrvM6809ROM0 + bank,	0x2000, 0x3fff, MAP_ROM);
}

static void brkthru_main_write(UINT16 address, UINT8 data)
{
	switch (address & ~0x1000)
	{
		case 0x0800:
			bgscroll = data | (bgscroll & 0x100);
		return;

		case 0x0801:
		{
			bankswitch(data & 7);
			bgbasecolor = (data >> 2) & 0x0e;
			flipscreen = data & 0x40;
			bgscroll = ((data & 0x80) << 1) | (bgscroll & 0xff);
		}
		return;

		case 0x0802:
			soundlatch = data;
			M6809Close();
			M6809Open(1);
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_ACK);
			M6809Close();
			M6809Open(0);
		return;

		case 0x0803:
			if (data & 0x02) M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			//nmi_mask = (~data & 1) ^ ((address & 0x800) >> 11); grr.
			if (address == 0x803) nmi_mask = data & 1; else nmi_mask = ~data & 1;
			//bprintf(0, _T("nmi mask %X. address %X\n"), nmi_mask, address);
		return;
	}
}

static UINT8 brkthru_main_read(UINT16 address)
{
	switch (address & ~0x1000)
	{
		case 0x0800:
			return DrvInputs[0];

		case 0x0801:
			return (DrvInputs[1] & 0x7f) | (vblank ? 0 : 0x80);

		case 0x0802:
			return DrvDips[0];

		case 0x0803:
			return (DrvDips[1] & 0x1f) | (DrvInputs[2] & 0xe0);
	}

	return 0;
}

static void brkthru_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
		case 0x2001:
			BurnYM3526Write(address & 1, data);
		return;

		case 0x6000:
		case 0x6001:
			BurnYM2203Write(0, address & 1, data);
		return;
	}
}

static UINT8 brkthru_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x4000:
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_NONE);

			return soundlatch;

		case 0x6000:
		case 0x6001:
			return BurnYM2203Read(0, address & 1);
	}

	return 0;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	M6809SetIRQLine(M6809_IRQ_LINE, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

inline static double DrvGetTime()
{
	return (double)M6809TotalCycles() / (1500000*2);
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)M6809TotalCycles() * nSoundRate / (1500000*2);
}

static INT32 DrvDoReset()
{
	memset (AllRam,	0, RamEnd - AllRam);

	M6809Open(0);
	bankswitch(0);
	M6809Reset();
	BurnYM3526Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	BurnYM2203Reset();
	M6809Close();

	DrvInputs[2] = 0xff;
	bgscroll = 0;
	soundlatch = 0;
	flipscreen = 0;
	nmi_mask = 0;
	bgbasecolor = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0		= Next; Next += 0x020000;
	DrvM6809ROM1		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x040000;
	DrvGfxROM2		= Next; Next += 0x040000;

	DrvColPROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x000800;
	DrvBgRAM		= Next; Next += 0x000400;
	DrvFgRAM		= Next; Next += 0x000c00;
	DrvM6809RAM1		= Next; Next += 0x002000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3]  = { 0x8004, 0, 4 };
	INT32 XOffs0[8]  = { 256*8*8+0, 256*8*8+1, 256*8*8+2, 256*8*8+3, 0, 1, 2, 3 };//STEP4(0x4000, 1), STEP4(0,1) };
	INT32 YOffs[16]  = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 };//STEP16(0,8) };

	INT32 Plane1[3]  = { 0x20004, 0, 4 };
	INT32 Plane2[3]  = { 0x18000, 0, 4 };
	INT32 XOffs1[16] = { 0, 1, 2, 3, 1024*8*8+0, 1024*8*8+1, 1024*8*8+2, 1024*8*8+3, 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+1024*8*8+0, 16*8+1024*8*8+1, 16*8+1024*8*8+2, 16*8+1024*8*8+3 };//STEP4(0,1), STEP4(0x10000,1), STEP4(0x80,1), STEP4(0x10080, 1) };

	INT32 Plane3[3]  = { 0x80000, 0x40000, 0 };
	INT32 XOffs3[16] = { 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7, 0, 1, 2, 3, 4, 5, 6, 7 };//STEP8(128,1), STEP8(0,1) };


	UINT8 *tmp = (UINT8*)BurnMalloc(0x60000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0100, 3, 8, 8, Plane0, XOffs0, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x20000);

	GfxDecode(0x0080, 3, 16, 16, Plane1, XOffs1, YOffs, 0x100, tmp + 0x00000, DrvGfxROM1 + 0x00000);
	GfxDecode(0x0080, 3, 16, 16, Plane2, XOffs1, YOffs, 0x100, tmp + 0x01000, DrvGfxROM1 + 0x08000);
	GfxDecode(0x0080, 3, 16, 16, Plane1, XOffs1, YOffs, 0x100, tmp + 0x08000, DrvGfxROM1 + 0x10000);
	GfxDecode(0x0080, 3, 16, 16, Plane2, XOffs1, YOffs, 0x100, tmp + 0x09000, DrvGfxROM1 + 0x18000);
	GfxDecode(0x0080, 3, 16, 16, Plane1, XOffs1, YOffs, 0x100, tmp + 0x10000, DrvGfxROM1 + 0x20000);
	GfxDecode(0x0080, 3, 16, 16, Plane2, XOffs1, YOffs, 0x100, tmp + 0x11000, DrvGfxROM1 + 0x28000);
	GfxDecode(0x0080, 3, 16, 16, Plane1, XOffs1, YOffs, 0x100, tmp + 0x18000, DrvGfxROM1 + 0x30000);
	GfxDecode(0x0080, 3, 16, 16, Plane2, XOffs1, YOffs, 0x100, tmp + 0x19000, DrvGfxROM1 + 0x38000);

	memcpy (tmp, DrvGfxROM2, 0x18000);

	GfxDecode(0x0400, 3, 16, 16, Plane3, XOffs3, YOffs, 0x100, tmp, DrvGfxROM2);

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
		if (BurnLoadRom(DrvM6809ROM0 + 0x04000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x10000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x18000,  3, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x08000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2   + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x08000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x10000,  8, 1)) return 1;

		memcpy (DrvGfxROM1 + 0x00000, DrvGfxROM2 + 0x00000, 0x4000);
		memcpy (DrvGfxROM1 + 0x08000, DrvGfxROM2 + 0x04000, 0x4000);
		memcpy (DrvGfxROM1 + 0x10000, DrvGfxROM2 + 0x08000, 0x4000);
		memcpy (DrvGfxROM1 + 0x18000, DrvGfxROM2 + 0x0c000, 0x4000);
		memcpy (DrvGfxROM1 + 0x04000, DrvGfxROM2 + 0x10000, 0x1000);
		memcpy (DrvGfxROM1 + 0x06000, DrvGfxROM2 + 0x11000, 0x1000);
		memcpy (DrvGfxROM1 + 0x0c000, DrvGfxROM2 + 0x12000, 0x1000);
		memcpy (DrvGfxROM1 + 0x0e000, DrvGfxROM2 + 0x13000, 0x1000);
		memcpy (DrvGfxROM1 + 0x14000, DrvGfxROM2 + 0x14000, 0x1000);
		memcpy (DrvGfxROM1 + 0x16000, DrvGfxROM2 + 0x15000, 0x1000);
		memcpy (DrvGfxROM1 + 0x1c000, DrvGfxROM2 + 0x16000, 0x1000);
		memcpy (DrvGfxROM1 + 0x1e000, DrvGfxROM2 + 0x17000, 0x1000);

		if (BurnLoadRom(DrvGfxROM2   + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x08000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x10000, 11, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00100, 13, 1)) return 1;

		DrvGfxDecode();
	}

	M6809Init(2); // this init is only called once with the maximum cpus to init.
	M6809Open(0);
	if (darwin) {
		M6809MapMemory(DrvSprRAM,		0x0000, 0x00ff, MAP_RAM); // 0-ff
		M6809MapMemory(DrvFgRAM,		0x1000, 0x1bff, MAP_RAM); // 0-3ff
		M6809MapMemory(DrvBgRAM,		0x1c00, 0x1fff, MAP_RAM);
	} else {
		M6809MapMemory(DrvFgRAM,		0x0000, 0x0bff, MAP_RAM); // 0-3ff
		M6809MapMemory(DrvBgRAM,		0x0c00, 0x0fff, MAP_RAM);
		M6809MapMemory(DrvSprRAM,		0x1000, 0x17ff, MAP_RAM); // 0-ff
	}
	M6809MapMemory(DrvM6809ROM0 + 0x4000,		0x4000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(brkthru_main_write);
	M6809SetReadHandler(brkthru_main_read);
	M6809Close();

	//M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809RAM1,			0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM1 + 0x8000,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(brkthru_sound_write);
	M6809SetReadHandler(brkthru_sound_read);
	M6809Close();

	BurnYM2203Init(1, 1500000, NULL, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachM6809(1500000*2);
	BurnYM2203SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.10);

	BurnYM3526Init(3000000, &DrvFMIRQHandler, &DrvSynchroniseStream, 1);
	BurnTimerAttachM6809YM3526(1500000*2);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	darwin = 0;
	GenericTilesExit();

	M6809Exit();
	BurnYM2203Exit();
	BurnYM3526Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	const UINT8 tab[16] = {
		0x00, 0x0e, 0x1f, 0x2d, 0x43, 0x51, 0x62, 0x70, 
		0x8f, 0x9d, 0xae, 0xbc, 0xd2, 0xe0, 0xf1, 0xff
	};

	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r = tab[DrvColPROM[i] & 0xf];
		UINT8 g = tab[DrvColPROM[i] >> 4];
		UINT8 b = tab[DrvColPROM[i+0x100] & 0xf];

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		sy -= 8;
		sx -= 8;

		INT32 code = DrvFgRAM[offs];

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, 0, 3, 0, 0, DrvGfxROM0);
	}
}

static void draw_bg_layer(INT32 opaque)
{
	for (INT32 offs = 0; offs < 32 * 16; offs++)
	{
		INT32 sy = (offs & 0xf) * 16;
		INT32 sx = (offs / 0x10) * 16;

		sx -= bgscroll;
		sy -= 8;
		sx -= 8;
		if (sx < -15) sx += 512;
		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = DrvBgRAM[offs*2+1];
		INT32 code  = DrvBgRAM[offs*2] + ((attr & 3) * 256);
		INT32 color = ((attr >> 2) & 1)+bgbasecolor;

		if (opaque) {
			Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 3, 0x80, DrvGfxROM1);
		} else {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x80, DrvGfxROM1);
		}
	}
}

static inline void draw_single_sprite(INT32 code, INT32 sx, INT32 sy, INT32 color)
{
	if (flipscreen) {
		Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
	} else {
		Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
	}
}

static void draw_sprites(INT32 prio)
{
	for (INT32 offs = 0;offs < 0x100; offs += 4)
	{
		if ((DrvSprRAM[offs] & 0x01) == 0) continue;

		if (((DrvSprRAM[offs] & 0x08) >> 3) == prio)
		{
			INT32 sx = 240 - DrvSprRAM[offs + 3];
			sx -= 8;
			if (sx < -15) sx += 256;

			INT32 sy = 240 - DrvSprRAM[offs + 2];
			sy -= 8;
			INT32 code = DrvSprRAM[offs + 1] + (128 * (DrvSprRAM[offs] & 0x06));
			INT32 color = (DrvSprRAM[offs] & 0xe0) >> 5;

			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
			}

			if (DrvSprRAM[offs] & 0x10)
			{
				draw_single_sprite(code & ~1, sx, (flipscreen ? sy + 16 : sy - 16), color);
				draw_single_sprite(code |  1, sx, sy, color);

				draw_single_sprite(code & ~1, sx, (flipscreen ? sy + 16 : sy - 16) + 256, color);
				draw_single_sprite(code |  1, sx, sy + 256, color);
			}
			else
			{
				draw_single_sprite(code, sx, sy, color);
				draw_single_sprite(code, sx, sy + 256, color);
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

	BurnTransferClear();
	if (nBurnLayer & 1) draw_bg_layer(1);
	if (nBurnLayer & 2) draw_sprites(0);
	if (nBurnLayer & 1) draw_bg_layer(0);
	if (nBurnLayer & 2) draw_sprites(1);
	if (nBurnLayer & 4) draw_fg_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if ((prevcoin == 0xff) && (((DrvInputs[2] & 0x20) == 0) || ((DrvInputs[2] & 0x40) == 0))) { // coin!
			M6809Open(0);
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6809Close();
		}
		prevcoin = DrvInputs[2];
	}

	INT32 nInterleave = 272;
	INT32 nCyclesTotal[2] = { 1500000*2 / 60, 1500000*2 / 60 };

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		BurnTimerUpdate((i+1) * (nCyclesTotal[0] / nInterleave));
		if (i == 248) vblank = 1;
		if (i == 248 && nmi_mask) M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		M6809Close();

		M6809Open(1);
		BurnTimerUpdateYM3526((i+1) * (nCyclesTotal[1] / nInterleave));
		M6809Close();
	}

	M6809Open(0);
	BurnTimerEndFrame(nCyclesTotal[0]);
	M6809Close();
	
	M6809Open(1);
	BurnTimerEndFrameYM3526(nCyclesTotal[1]);
	M6809Close();

	if (pBurnSoundOut) {
		M6809Open(0);
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		M6809Close();
		M6809Open(1);
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
		M6809Close();
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
		*pnMin = 0x029706;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);

		M6809Open(0);
		BurnYM2203Scan(nAction, pnMin);
		M6809Close();
		M6809Open(1);
		BurnYM3526Scan(nAction, pnMin);
		M6809Close();

		SCAN_VAR(flipscreen);
		SCAN_VAR(bgscroll);
		SCAN_VAR(bgbasecolor);
		SCAN_VAR(soundlatch);
		SCAN_VAR(rombank);
		SCAN_VAR(nmi_mask);
	}
	
	if (nAction & ACB_WRITE) {
		M6809Open(0);
		bankswitch(rombank);
		M6809Close();
	}

	return 0;
}


// Break Thru (US)

static struct BurnRomInfo brkthruRomDesc[] = {
	{ "brkthru.1",		0x4000, 0xcfb4265f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "brkthru.2",		0x8000, 0xfa8246d9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "brkthru.4",		0x8000, 0x8cabf252, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "brkthru.3",		0x8000, 0x2f2c40c2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "brkthru.5",		0x8000, 0xc309435f, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code

	{ "brkthru.12",		0x2000, 0x58c0b29b, 3 | BRF_GRA },           //  5 Characters

	{ "brkthru.7",		0x8000, 0x920cc56a, 4 | BRF_GRA },           //  6 Background Layer
	{ "brkthru.6",		0x8000, 0xfd3cee40, 4 | BRF_GRA },           //  7
	{ "brkthru.8",		0x8000, 0xf67ee64e, 4 | BRF_GRA },           //  8

	{ "brkthru.9",		0x8000, 0xf54e50a7, 5 | BRF_GRA },           //  9 Sprites
	{ "brkthru.10",		0x8000, 0xfd156945, 5 | BRF_GRA },           // 10
	{ "brkthru.11",		0x8000, 0xc152a99b, 5 | BRF_GRA },           // 11

	{ "brkthru.13",		0x0100, 0xaae44269, 6 | BRF_GRA },           // 12 Color data
	{ "brkthru.14",		0x0100, 0xf2d4822a, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(brkthru)
STD_ROM_FN(brkthru)

struct BurnDriver BurnDrvBrkthru = {
	"brkthru", NULL, NULL, NULL, "1986",
	"Break Thru (US)\0", NULL, "Data East USA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_HORSHOOT, 0,
	NULL, brkthruRomInfo, brkthruRomName, NULL, NULL, BrkthruInputInfo, BrkthruDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 240, 4, 3
};


// Kyohkoh-Toppa (Japan)

static struct BurnRomInfo brkthrujRomDesc[] = {
	{ "1",			0x4000, 0x09bd60ee, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "2",			0x8000, 0xf2b2cd1c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4",			0x8000, 0xb42b3359, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "brkthru.3",		0x8000, 0x2f2c40c2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "brkthru.5",		0x8000, 0xc309435f, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code

	{ "12",			0x2000, 0x3d9a7003, 3 | BRF_GRA },           //  5 Characters

	{ "brkthru.7",		0x8000, 0x920cc56a, 4 | BRF_GRA },           //  6 Background Layer
	{ "6",			0x8000, 0xcb47b395, 4 | BRF_GRA },           //  7
	{ "8",			0x8000, 0x5e5a2cd7, 4 | BRF_GRA },           //  8

	{ "brkthru.9",		0x8000, 0xf54e50a7, 5 | BRF_GRA },           //  9 Sprites
	{ "brkthru.10",		0x8000, 0xfd156945, 5 | BRF_GRA },           // 10
	{ "brkthru.11",		0x8000, 0xc152a99b, 5 | BRF_GRA },           // 11

	{ "brkthru.13",		0x0100, 0xaae44269, 6 | BRF_GRA },           // 12 Color data
	{ "brkthru.14",		0x0100, 0xf2d4822a, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(brkthruj)
STD_ROM_FN(brkthruj)

struct BurnDriver BurnDrvBrkthruj = {
	"brkthruj", "brkthru", NULL, NULL, "1986",
	"Kyohkoh-Toppa (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_HORSHOOT, 0,
	NULL, brkthrujRomInfo, brkthrujRomName, NULL, NULL, BrkthruInputInfo, BrkthrujDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 240, 4, 3
};


// Force Break (bootleg)

static struct BurnRomInfo forcebrkRomDesc[] = {
	{ "1",			0x4000, 0x09bd60ee, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "2",			0x8000, 0xf2b2cd1c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "forcebrk4",		0x8000, 0xb4838c19, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "brkthru.3",		0x8000, 0x2f2c40c2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "brkthru.5",		0x8000, 0xc309435f, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code

	{ "12",			0x2000, 0x3d9a7003, 3 | BRF_GRA },           //  5 Characters

	{ "brkthru.7",		0x8000, 0x920cc56a, 4 | BRF_GRA },           //  6 Background Layer
	{ "forcebrk6",		0x8000, 0x08bca16a, 4 | BRF_GRA },           //  7
	{ "forcebrk8",		0x8000, 0xa3a1131e, 4 | BRF_GRA },           //  8

	{ "brkthru.9",		0x8000, 0xf54e50a7, 5 | BRF_GRA },           //  9 Sprites
	{ "brkthru.10",		0x8000, 0xfd156945, 5 | BRF_GRA },           // 10
	{ "brkthru.11",		0x8000, 0xc152a99b, 5 | BRF_GRA },           // 11

	{ "brkthru.13",		0x0100, 0xaae44269, 6 | BRF_GRA },           // 12 Color data
	{ "brkthru.14",		0x0100, 0xf2d4822a, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(forcebrk)
STD_ROM_FN(forcebrk)

struct BurnDriver BurnDrvForcebrk = {
	"forcebrk", "brkthru", NULL, NULL, "1986",
	"Force Break (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_DATAEAST, GBF_HORSHOOT, 0,
	NULL, forcebrkRomInfo, forcebrkRomName, NULL, NULL, BrkthruInputInfo, BrkthruDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 240, 4, 3
};


// Darwin 4078 (Japan)

static struct BurnRomInfo darwinRomDesc[] = {
	{ "darw_04.rom",	0x4000, 0x0eabf21c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "darw_05.rom",	0x8000, 0xe771f864, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "darw_07.rom",	0x8000, 0x97ac052c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "darw_06.rom",	0x8000, 0x2a9fb208, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "darw_08.rom",	0x8000, 0x6b580d58, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code

	{ "darw_09.rom",	0x2000, 0x067b4cf5, 3 | BRF_GRA },           //  5 Characters

	{ "darw_03.rom",	0x8000, 0x57d0350d, 4 | BRF_GRA },           //  6 Background Layer
	{ "darw_02.rom",	0x8000, 0x559a71ab, 4 | BRF_GRA },           //  7
	{ "darw_01.rom",	0x8000, 0x15a16973, 4 | BRF_GRA },           //  8

	{ "darw_10.rom",	0x8000, 0x487a014c, 5 | BRF_GRA },           //  9 Sprites
	{ "darw_11.rom",	0x8000, 0x548ce2d1, 5 | BRF_GRA },           // 10
	{ "darw_12.rom",	0x8000, 0xfaba5fef, 5 | BRF_GRA },           // 11

	{ "df.12",		0x0100, 0x89b952ef, 6 | BRF_GRA },           // 12 Color data
	{ "df.13",		0x0100, 0xd595e91d, 6 | BRF_GRA },           // 13
};

STD_ROM_PICK(darwin)
STD_ROM_FN(darwin)

static INT32 darwinInit()
{
	darwin = 1;
	return DrvInit();
}

struct BurnDriver BurnDrvDarwin = {
	"darwin", NULL, NULL, NULL, "1986",
	"Darwin 4078 (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, darwinRomInfo, darwinRomName, NULL, NULL, BrkthruInputInfo, DarwinDIPInfo,
	darwinInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 240, 3, 4
};
