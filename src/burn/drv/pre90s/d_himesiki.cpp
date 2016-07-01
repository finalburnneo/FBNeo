// FB Alpha Himeshikibu & Android driver module
// Based on MAME driver by Uki and David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "8255ppi.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 nDrvZ80Bank;
static UINT8 scrolly;
static UINT16 scrollx;
static UINT8 soundlatch;
static UINT8 flipscreen;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo HimesikiInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 3,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Himesiki)

static struct BurnInputInfo AndroidpInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 3,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Androidp)

static struct BurnDIPInfo HimesikiDIPList[]=
{
	{0x14, 0xff, 0xff, 0xfe, NULL		},
	{0x15, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x01, 0x01, "Off"		},
	{0x14, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "1-2"		},
	{0x14, 0x01, 0x02, 0x02, "Off"		},
	{0x14, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "1-3"		},
	{0x14, 0x01, 0x04, 0x04, "Off"		},
	{0x14, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x08, 0x08, "Upright"		},
	{0x14, 0x01, 0x08, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "1-5"		},
	{0x14, 0x01, 0x10, 0x10, "Off"		},
	{0x14, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "1-6"		},
	{0x14, 0x01, 0x20, 0x20, "Off"		},
	{0x14, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x40, 0x40, "Off"		},
	{0x14, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"		},
	{0x14, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "2-1"		},
	{0x15, 0x01, 0x01, 0x01, "Off"		},
	{0x15, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "2-2"		},
	{0x15, 0x01, 0x02, 0x02, "Off"		},
	{0x15, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "2-3"		},
	{0x15, 0x01, 0x04, 0x04, "Off"		},
	{0x15, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "2-4"		},
	{0x15, 0x01, 0x08, 0x08, "Off"		},
	{0x15, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "2-5"		},
	{0x15, 0x01, 0x10, 0x10, "Off"		},
	{0x15, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "2-6"		},
	{0x15, 0x01, 0x20, 0x20, "Off"		},
	{0x15, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "2-7"		},
	{0x15, 0x01, 0x40, 0x40, "Off"		},
	{0x15, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "2-8"		},
	{0x15, 0x01, 0x80, 0x80, "Off"		},
	{0x15, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Himesiki)

static struct BurnDIPInfo AndroidpoDIPList[]=
{
	{0x14, 0xff, 0xff, 0xfb, NULL		},
	{0x15, 0xff, 0xff, 0xfc, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x0c, 0x0c, "1"		},
	{0x14, 0x01, 0x0c, 0x04, "2"		},
	{0x14, 0x01, 0x0c, 0x08, "3"		},
	{0x14, 0x01, 0x0c, 0x00, "4"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x15, 0x01, 0x07, 0x00, "6 Coins 1 Credit"		},
	{0x15, 0x01, 0x07, 0x01, "5 Coins 1 Credit"		},
	{0x15, 0x01, 0x07, 0x02, "4 Coins 1 Credit"		},
	{0x15, 0x01, 0x07, 0x04, "1 Coin  1 Credit"		},
	{0x15, 0x01, 0x07, 0x05, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x07, 0x06, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  4 Credits"		},
	{0x15, 0x01, 0x07, 0x03, "Invalid"		},
};

STDDIPINFO(Androidpo)

static struct BurnDIPInfo AndroidpDIPList[]=
{
	{0x14, 0xff, 0xff, 0xf6, NULL		},
	{0x15, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x01, 0x01, "Cocktail"		},
	{0x14, 0x01, 0x01, 0x00, "Upright"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x0c, 0x08, "1"		},
	{0x14, 0x01, 0x0c, 0x0c, "2"		},
	{0x14, 0x01, 0x0c, 0x04, "3"		},
	{0x14, 0x01, 0x0c, 0x00, "4"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x40, 0x00, "No"		},
	{0x14, 0x01, 0x40, 0x40, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x80, 0x00, "Off"		},
	{0x14, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x15, 0x01, 0x07, 0x00, "4 Coins 1 Credit"		},
	{0x15, 0x01, 0x07, 0x01, "3 Coins 1 Credit"		},
	{0x15, 0x01, 0x07, 0x02, "2 Coins 1 Credit"		},
	{0x15, 0x01, 0x07, 0x07, "1 Coin  1 Credit"		},
	{0x15, 0x01, 0x07, 0x03, "2 Coins 3 Credits"		},
	{0x15, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x15, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x15, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
};

STDDIPINFO(Androidp)


static void palette_update_one(INT32 offset)
{
	INT32 p = (DrvPalRAM[offset+0] | (DrvPalRAM[offset+1] << 8));

	INT32 r = (p >> 10) & 0x1f;
	INT32 g = (p >>  5) & 0x1f;
	INT32 b = (p & 0x1f);

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r,g,b,0);
}

static void __fastcall himesiki_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xa800) {
		DrvPalRAM[address & 0x7ff] = data;
		palette_update_one(address & 0x7fe);
		return;
	}
}

static void __fastcall himesiki_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			ppi8255_w((port/4) & 1, port & 3, data);
		return;

		case 0x08:
			scrolly = data;
		return;

		case 0x09:
			scrollx = ((data & 1) << 8) | (scrollx & 0xff);
		return;

		case 0x0a:
			scrollx = (scrollx & 0x100) | data;
		return;

		case 0x0b:
			soundlatch = data;
			ZetClose();
			ZetOpen(1);
			ZetNmi();
			ZetClose();
			ZetOpen(0);
		return;
	}
}

static UINT8 __fastcall himesiki_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			return ppi8255_r((port/4) & 1, port & 3);
	}

	return 0;
}

static void __fastcall himesiki_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM2203Write(0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall himesiki_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM2203Read(0, port & 1);

		case 0x04:
			return soundlatch;
	}

	return 0;
}

static UINT8 ppi8255_0_portA_r() { return DrvInputs[0]; }
static UINT8 ppi8255_0_portB_r() { return DrvInputs[1]; }
static UINT8 ppi8255_0_portC_r() { return DrvInputs[2]; }
static UINT8 ppi8255_1_portA_r() { return DrvDips[0]; }
static UINT8 ppi8255_1_portB_r() { return DrvDips[1]; }

static void ppi8255_1_portC_w(UINT8 data)
{
	nDrvZ80Bank = data;
//	flipscreen = data & 0x10;

	INT32 bank = ((data >> 2) & 3) * 0x4000;

	ZetMapMemory(DrvZ80ROM0 + 0x10000 + bank, 0xc000, 0xffff, MAP_ROM);
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 4000000;
}

static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 4000000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ppi8255_1_portC_w(0);
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	//nDrvZ80Bank = 0;
	soundlatch = 0;
	flipscreen = 0;
	scrolly = 0;
	scrollx = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x020000;
	DrvZ80ROM1	= Next; Next += 0x008000;

	DrvGfxROM0	= Next; Next += 0x040000;
	DrvGfxROM1	= Next; Next += 0x080000;
	DrvGfxROM2	= Next; Next += 0x100000;

	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM0	= Next; Next += 0x002000;
	DrvZ80RAM1	= Next; Next += 0x000800;

	DrvPalRAM	= Next; Next += 0x000800;
	DrvBgRAM	= Next; Next += 0x001000;
	DrvSprRAM	= Next; Next += 0x000800;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes[4]  = { STEP4(0,1) };
	INT32 XOffs[32]  = { 4,0,12,8,20,16,28,24,36,32,44,40,52,48,60,56,68,64,76,72,84,80,92,88,100,96,108,104,116,112,124,120 };
	INT32 YOffs0[8]  = { STEP8(0,32) };
	INT32 YOffs1[16] = { STEP16(0,64) };
	INT32 YOffs2[32] = { STEP32(0,128) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x20000);

	GfxDecode(0x1000, 4,  8,  8, Planes, XOffs, YOffs0, 0x0100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Planes, XOffs, YOffs1, 0x0400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x80000);

	GfxDecode(0x0400, 4, 32, 32, Planes, XOffs, YOffs2, 0x1000, tmp, DrvGfxROM2);

	BurnFree (tmp);
	
	return 0;
}

static INT32 DrvInit(INT32 nGame)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (nGame == 0) // himesiki
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  1, 1)) return 1;
		memcpy (DrvZ80ROM0 + 0x18000, DrvZ80ROM0 + 0x10000, 0x04000);

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00001,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20001,  8, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20001, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40000, 13, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40001, 14, 2)) return 1;
		memset (DrvGfxROM1 + 0x60000, 0xff, 0x20000);
	}
	else if (nGame == 1) // androidpo
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;
		memcpy (DrvZ80ROM0 + 0x10000, DrvZ80ROM1 + 0x00000, 0x04000);
		memcpy (DrvZ80ROM0 + 0x18000, DrvZ80ROM1 + 0x04000, 0x04000);
		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;
		memcpy (DrvZ80ROM0 + 0x14000, DrvZ80ROM1 + 0x00000, 0x04000);
		memcpy (DrvZ80ROM0 + 0x1c000, DrvZ80ROM1 + 0x04000, 0x04000);

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00001,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20001,  6, 2)) return 1;

		memset (DrvGfxROM2, 0xff, 0x80000); // not present
	} else if (nGame == 2) // androidp
	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;
		memcpy (DrvZ80ROM0 + 0x10000, DrvZ80ROM1 + 0x00000, 0x04000);
		memcpy (DrvZ80ROM0 + 0x18000, DrvZ80ROM1 + 0x04000, 0x04000);

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00001,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20001,  5, 2)) return 1;

		memset (DrvGfxROM2, 0xff, 0x80000); // not present
	}

	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xa000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,		0xa800, 0xafff, MAP_ROM); // written in handler
	ZetMapMemory(DrvBgRAM,		0xb000, 0xbfff, MAP_RAM);
	ZetSetWriteHandler(himesiki_main_write);
	ZetSetOutHandler(himesiki_main_write_port);
	ZetSetInHandler(himesiki_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xf800, 0xffff, MAP_RAM);
	ZetSetOutHandler(himesiki_sound_write_port);
	ZetSetInHandler(himesiki_sound_read_port);
	ZetClose();

	ppi8255_init(2);
	PPI0PortReadA = ppi8255_0_portA_r;
	PPI0PortReadB = ppi8255_0_portB_r;
	PPI0PortReadC = ppi8255_0_portC_r;
	PPI1PortReadA = ppi8255_1_portA_r;
	PPI1PortReadB = ppi8255_1_portB_r;
	PPI1PortWriteC = ppi8255_1_portC_w;

	BurnYM2203Init(1, 3000000, &DrvFMIRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2203SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.05);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	BurnYM2203Exit();
	ZetExit();
	ppi8255_exit();
	GenericTilesExit();

	BurnFree (AllMem);

	return 0;
}

static void draw_layer()
{
	INT32 yscroll = scrolly;
	INT32 xscroll = scrollx;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		sx -= xscroll;
		if (sx < -7) sx += 512;
		sy -= yscroll;
		if (sy < -7) sy += 256;

		INT32 code = (DrvBgRAM[offs * 2] + (DrvBgRAM[offs * 2 + 1] << 8));
		INT32 color = code >> 12;

		Render8x8Tile_Clip(pTransDraw, code & 0xfff, sx, sy, color, 4, 0, DrvGfxROM0);
	}
}

static void draw_sprites()
{
	UINT8 *spriteram = DrvSprRAM + 0x100;

	for (INT32 offs = 0x00; offs < 0x60; offs += 4)
	{
		INT32 attr = spriteram[offs + 1];
		INT32 code = spriteram[offs + 0] | (attr & 3) << 8;
		code &= 0x2ff;
		INT32 x = spriteram[offs + 3] | (attr & 8) << 5;
		INT32 y = spriteram[offs + 2];

		INT32 color = (attr & 0xf0) >> 4;
		color &= 0x3ff;
		INT32 fx = attr & 4;
		INT32 fy = 0;

		if (x > 0x1e0)
			x -= 0x200;

		if (flipscreen)
		{
			y = (y - 31) & 0xff;
			x = 224 - x;
			fx ^= 4;
			fy = 1;
		}
		else
		{
			y = 257 - y;
			if (y > 0xc0)
				y -= 0x100;
		}

		if (fy) {
			if (fx) {
				Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, x, y, color, 4, 0xf, 0x200, DrvGfxROM2);
			} else {
				Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, x, y, color, 4, 0xf, 0x200, DrvGfxROM2);
			}
		} else {
			if (fx) {
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, x, y, color, 4, 0xf, 0x200, DrvGfxROM2);
			} else {
				Render32x32Tile_Mask_Clip(pTransDraw, code, x, y, color, 4, 0xf, 0x200, DrvGfxROM2);
			}
		}
	}

	spriteram = DrvSprRAM;

	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		if ((spriteram[offs + 0] == 0x00) && 
	        (spriteram[offs + 1] == 0x00) &&
		    (spriteram[offs + 2] == 0x00) &&
		    (spriteram[offs + 3] == 0x00))
		 	 continue;

		INT32 attr = spriteram[offs + 1];
		INT32 code = spriteram[offs + 0] | (attr & 7) << 8;
		INT32 x = spriteram[offs + 3] | (attr & 8) << 5;
		INT32 y = spriteram[offs + 2];

		INT32 color = (attr & 0xf0) >> 4;
		INT32 f = 0;

		if (x > 0x1e0)
			x -= 0x200;

		if (flipscreen)
		{
			y = (y - 15) &0xff;
			x = 240 - x;
			f = 1;
		}
		else
			y = 257 - y;

		y &= 0xff;
		if (y > 0xf0)
			y -= 0x100;

		if (f) {
			if (f) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, x, y, color, 4, 0xf, 0x200, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, x, y, color, 4, 0xf, 0x200, DrvGfxROM1);
			}
		} else {
			if (f) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, x, y, color, 4, 0xf, 0x200, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, x, y, color, 4, 0xf, 0x200, DrvGfxROM1);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x800; i+=2) {
			palette_update_one(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();
	if (nBurnLayer & 1) draw_layer();
	if (nBurnLayer & 2) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256*2;
	INT32 nCyclesTotal[2] = { 8000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	
	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext, nCyclesSegment;

		// Run Z80 #1
		nCurrentCPU = 0;
		ZetOpen(nCurrentCPU);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += ZetRun(nCyclesSegment);
		if (i == 239*2) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		// Run Z80 #2
		nCurrentCPU = 1;
		ZetOpen(nCurrentCPU);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[nCurrentCPU] / nInterleave));
		ZetClose();
	}

	ZetOpen(1);
	BurnTimerEndFrame(nCyclesTotal[1]);
	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}
	ZetClose();
	
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

		ppi8255_scan();

		SCAN_VAR(nDrvZ80Bank);
		SCAN_VAR(scrolly);
		SCAN_VAR(scrollx);

		if (nAction & ACB_WRITE) {
			ZetOpen(0);
			ppi8255_1_portC_w(nDrvZ80Bank);
			ZetClose();
		}
	}

	return 0;
}


// Himeshikibu (Japan)

static struct BurnRomInfo himesikiRomDesc[] = {
	{ "1.1k",					0x08000, 0xfb4604b3, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "2.1g",					0x08000, 0x0c30ded1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "5.6n",					0x08000, 0xb1214ac7, 2 | BRF_PRG | BRF_ESS }, //  2 - Z80 #1 Code

	{ "3.5f",					0x10000, 0x73843e60, 3 | BRF_GRA },           //  3 - Background Tiles
	{ "4.5d",					0x10000, 0x443a3164, 3 | BRF_GRA },           //  4

	{ "13.9e",					0x10000, 0x43102682, 4 | BRF_GRA },           //  5 - 16x16 Sprites
	{ "12.9c",					0x10000, 0x19c8f9f4, 4 | BRF_GRA },           //  6
	{ "15.8e",					0x10000, 0x2630d394, 4 | BRF_GRA },           //  7
	{ "14.8c",					0x10000, 0x8103a207, 4 | BRF_GRA },           //  8

	{ "6.1a",					0x10000, 0x14989c22, 5 | BRF_GRA },           //  9 - 32x32 Sprites
	{ "7.1c",					0x10000, 0xcec56e16, 5 | BRF_GRA },           // 10
	{ "8.2a",					0x10000, 0x44ba127e, 5 | BRF_GRA },           // 11
	{ "9.2c",					0x10000, 0x0dda724a, 5 | BRF_GRA },           // 12
	{ "10.4a",					0x10000, 0x0adda8d1, 5 | BRF_GRA },           // 13
	{ "11.4c",					0x10000, 0xaa032946, 5 | BRF_GRA },           // 14
};

STD_ROM_PICK(himesiki)
STD_ROM_FN(himesiki)

static INT32 himesikiInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvHimesiki = {
	"himesiki", NULL, NULL, NULL, "1989",
	"Himeshikibu (Japan)\0", NULL, "Hi-Soft", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAHJONG, 0,
	NULL, himesikiRomInfo, himesikiRomName, NULL, NULL, HimesikiInputInfo, HimesikiDIPInfo,
	himesikiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	192, 256, 3, 4
};


// Android (early build?)

static struct BurnRomInfo androidpoRomDesc[] = {
	{ "MITSUBISHI__AD1__M5L27256K.toppcb.k1",		0x08000, 0x25ab85eb, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "MITSUBISHI__AD-3__M5L27256K.toppcb.g1",		0x08000, 0x6cf5f48a, 1 | BRF_PRG | BRF_ESS }, //  1 
	{ "MITSUBISHI__AD2__M5L27256K.toppcb.j1",		0x08000, 0xe41426be, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "MITSUBISHI__AD-4__M5L27256K.toppcb.n6",		0x08000, 0x13c38fe4, 2 | BRF_PRG | BRF_ESS }, //  3 - Z80 #1 Code

	{ "MITSUBISHI__AD-5__M5L27512K.toppcb.f5",		0x10000, 0x4c72a930, 3 | BRF_GRA },           //  4 - Background Tiles	

	{ "MITSUBISHI__AD-6__M5L27512K.botpcb.def9",	0x10000, 0x5e42984e, 4 | BRF_GRA },           //  5 - 16x16 Sprites
	{ "MITSUBISHI__AD-7__M5L27512K.botpcb.bc9",		0x10000, 0x611ff400, 4 | BRF_GRA },           //  6
	
	{ "RICOH_7A2_19__EPL10P8BP_JAPAN_M.j3.jed",		0x00473, 0x807d1553, 0 | BRF_OPT },           //  7
	{ "RICOH_7A2_19__EPL10P8BP_JAPAN_I.f1.jed",		0x00473, 0xc5e51ea2, 0 | BRF_OPT },           //  8
};

STD_ROM_PICK(androidpo)
STD_ROM_FN(androidpo)

static INT32 androidpoInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvAndroidpo = {
	"androidpo", "androidp", NULL, NULL, "198?",
	"Android (prototype, early build)\0", NULL, "Nasco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, androidpoRomInfo, androidpoRomName, NULL, NULL, AndroidpInputInfo, AndroidpoDIPInfo,
	androidpoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	192, 256, 3, 4
};

// Android (later build?)

static struct BurnRomInfo androidpRomDesc[] = {
	{ "ANDR1.BIN", 0x08000, 0xfff04130, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "ANDR3.BIN", 0x08000, 0x112d5123, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ANDR4.BIN", 0x08000, 0x65f5e98b, 2 | BRF_PRG | BRF_ESS }, //  2 - Z80 #1 Code
   
	{ "ANDR5.BIN", 0x10000, 0x0a0b44c0, 3 | BRF_GRA },           //  3 - Background Tiles
   
	{ "ANDR6.BIN", 0x10000, 0x122b7dd1, 4 | BRF_GRA },           //  4 - 16x16 Sprites
	{ "ANDR7.BIN", 0x10000, 0xfc0f9234, 4 | BRF_GRA },           //  5
};

STD_ROM_PICK(androidp)
STD_ROM_FN(androidp)

static INT32 androidpInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvAndroidp = {
	"androidp", NULL, NULL, NULL, "198?",
	"Android (prototype, later build)\0", NULL, "Nasco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, androidpRomInfo, androidpRomName, NULL, NULL, AndroidpInputInfo, AndroidpDIPInfo,
	androidpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	192, 256, 3, 4
};
