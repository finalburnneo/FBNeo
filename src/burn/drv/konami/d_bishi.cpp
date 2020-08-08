// FB Alpha Bishi Bashi driver module
// Based on MAME driver by R. Belmont

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "konamiic.h"
#include "ymz280b.h"

static UINT8 *AllMem;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvGfxROMExp;
static UINT8 *AllRam;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 layerpri[4];
static INT32 layer_colorbase[4];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];
static UINT8 DrvDips[2];

static UINT16 control_data = 0;
static UINT16 control_data2 = 0;

static struct BurnInputInfo BishiInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 11,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 11,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 10,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 9,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 8,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 15,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 14,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 3"	},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p3 start"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p3 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 6,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bishi)

static struct BurnInputInfo Bishi2pInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 11,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 11,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 10,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 9,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 8,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 6,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bishi2p)

static struct BurnDIPInfo BishiDIPList[]=
{
	{0x10, 0xff, 0xff, 0xec, NULL			},
	{0x11, 0xff, 0xff, 0x3f, NULL			},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x10, 0x01, 0x07, 0x07, "Easiest"		},
	{0x10, 0x01, 0x07, 0x06, "Very Easy"		},
	{0x10, 0x01, 0x07, 0x05, "Easy"			},
	{0x10, 0x01, 0x07, 0x04, "Medium"		},
	{0x10, 0x01, 0x07, 0x03, "Medium Hard"		},
	{0x10, 0x01, 0x07, 0x02, "Hard"			},
	{0x10, 0x01, 0x07, 0x01, "Very Hard"		},
	{0x10, 0x01, 0x07, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    8, "Lives"		},
	{0x10, 0x01, 0x38, 0x38, "1"			},
	{0x10, 0x01, 0x38, 0x30, "2"			},
	{0x10, 0x01, 0x38, 0x28, "3"			},
	{0x10, 0x01, 0x38, 0x20, "4"			},
	{0x10, 0x01, 0x38, 0x18, "5"			},
	{0x10, 0x01, 0x38, 0x10, "6"			},
	{0x10, 0x01, 0x38, 0x08, "7"			},
	{0x10, 0x01, 0x38, 0x00, "8"			},

	{0   , 0xfe, 0   ,    4, "Demo Sounds"		},
	{0x10, 0x01, 0xc0, 0xc0, "All The Time"		},
	{0x10, 0x01, 0xc0, 0x80, "Loop At 2 Times"	},
	{0x10, 0x01, 0xc0, 0x40, "Loop At 4 Times"	},
	{0x10, 0x01, 0xc0, 0x00, "No Sounds"		},

	{0   , 0xfe, 0   ,    16, "Coinage"		},
	{0x11, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x11, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x11, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x11, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x11, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x0f, 0x00, "4 Coins 5 Credits"	},
	{0x11, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x11, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x11, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x11, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x11, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x11, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x11, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x11, 0x01, 0x10, 0x10, "No"			},
	{0x11, 0x01, 0x10, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Slack Difficulty"	},
	{0x11, 0x01, 0x20, 0x20, "Off"			},
	{0x11, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Title Demo"		},
	{0x11, 0x01, 0x40, 0x40, "At 1 Loop"		},
	{0x11, 0x01, 0x40, 0x00, "At Every Gamedemo"	},

	{0   , 0xfe, 0   ,    2, "Gamedemo"		},
	{0x11, 0x01, 0x80, 0x80, "4 Kinds"		},
	{0x11, 0x01, 0x80, 0x00, "7 Kinds"		},
};

STDDIPINFO(Bishi)

static struct BurnDIPInfo Bishi2pDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xec, NULL			},
	{0x0d, 0xff, 0xff, 0x3f, NULL			},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x0c, 0x01, 0x07, 0x07, "Easiest"		},
	{0x0c, 0x01, 0x07, 0x06, "Very Easy"		},
	{0x0c, 0x01, 0x07, 0x05, "Easy"			},
	{0x0c, 0x01, 0x07, 0x04, "Medium"		},
	{0x0c, 0x01, 0x07, 0x03, "Medium Hard"		},
	{0x0c, 0x01, 0x07, 0x02, "Hard"			},
	{0x0c, 0x01, 0x07, 0x01, "Very Hard"		},
	{0x0c, 0x01, 0x07, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    8, "Lives"		},
	{0x0c, 0x01, 0x38, 0x38, "1"			},
	{0x0c, 0x01, 0x38, 0x30, "2"			},
	{0x0c, 0x01, 0x38, 0x28, "3"			},
	{0x0c, 0x01, 0x38, 0x20, "4"			},
	{0x0c, 0x01, 0x38, 0x18, "5"			},
	{0x0c, 0x01, 0x38, 0x10, "6"			},
	{0x0c, 0x01, 0x38, 0x08, "7"			},
	{0x0c, 0x01, 0x38, 0x00, "8"			},

	{0   , 0xfe, 0   ,    4, "Demo Sounds"		},
	{0x0c, 0x01, 0xc0, 0xc0, "All The Time"		},
	{0x0c, 0x01, 0xc0, 0x80, "Loop At 2 Times"	},
	{0x0c, 0x01, 0xc0, 0x40, "Loop At 4 Times"	},
	{0x0c, 0x01, 0xc0, 0x00, "No Sounds"		},

	{0   , 0xfe, 0   ,    16, "Coinage"		},
	{0x0d, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x0d, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x0f, 0x00, "4 Coins 5 Credits"	},
	{0x0d, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x0d, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x0d, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x0d, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0d, 0x01, 0x10, 0x10, "No"			},
	{0x0d, 0x01, 0x10, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Slack Difficulty"	},
	{0x0d, 0x01, 0x20, 0x20, "Off"			},
	{0x0d, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Title Demo"		},
	{0x0d, 0x01, 0x40, 0x40, "At 1 Loop"		},
	{0x0d, 0x01, 0x40, 0x00, "At Every Gamedemo"	},

	{0   , 0xfe, 0   ,    2, "Gamedemo"		},
	{0x0d, 0x01, 0x80, 0x80, "4 Kinds"		},
	{0x0d, 0x01, 0x80, 0x00, "7 Kinds"		},
};

STDDIPINFO(Bishi2p)

static void __fastcall bishi_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffc0) == 0x830000) {
		K056832WordWrite(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0x840000) {
		return; // regb
	}

	if ((address & 0xffffe0) == 0x850000) {
		K054338WriteWord(address, data);
		return;
	}

	if ((address & 0xffff00) == 0x870000) {
		K055555WordWrite(address, data);
		return;
	}

	if ((address & 0xffe000) == 0xa00000) {
		K056832RamWriteWord(address & 0x1fff, data);
		return;
	}

	switch (address)
	{
		case 0x800000:
			control_data = data;
		return;

		case 0x810000:
			control_data2 = data;
		return;

		case 0x880000:
		case 0x880002:
			YMZ280BWrite((address/2)&1, data);
		return;
	}
}

static void __fastcall bishi_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffffc0) == 0x830000) {
		K056832ByteWrite(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0x840000) {
		return; // regb
	}

	if ((address & 0xffffe0) == 0x850000) {
		K054338WriteByte(address, data);
		return;
	}

	if ((address & 0xffff00) == 0x870000) {
		K055555ByteWrite(address, data);
		return;
	}

	if ((address & 0xffe000) == 0xa00000) {
		K056832RamWriteByte(address & 0x1fff, data);
		return;
	}

	switch (address)
	{
		case 0x800000:
			control_data = (control_data & 0x00ff) | (data << 8);
		return;

		case 0x800001:
			control_data = (control_data & 0xff00) | (data << 0);
		return;

		case 0x810000:
		case 0x810001:
			control_data2 = data;
		return;

		case 0x880000:
		case 0x880002:
			YMZ280BWrite((address/2)&1, data);
		return;
	}
}

static UINT16 __fastcall bishi_read_word(UINT32 address)
{
	if ((address & 0xffe000) == 0xa00000) {
		return K056832RamReadWord(address & 0x1fff);
	}

	switch (address)
	{
		case 0x800000:
		case 0x800001:
			return control_data;

		case 0x800004:
		case 0x800005:
			return (DrvDips[1] << 8) + DrvDips[0];

		case 0x800006:
		case 0x800007:
			return DrvInputs[0];

		case 0x800008:
		case 0x800009:
			return DrvInputs[1];

		case 0x880000:
		case 0x880002:
			return YMZ280BRead((address/2)&1);
	}

	return 0;
}

static UINT8 __fastcall bishi_read_byte(UINT32 address)
{
	if ((address & 0xffe000) == 0xa00000) {
		return K056832RamReadByte(address & 0x1fff);
	}

	switch (address)
	{
		case 0x800000:
		case 0x800001:
			return control_data >> ((~address & 1) * 8);

		case 0x800004:
		case 0x800005:
			return DrvDips[(address & 1)];

		case 0x800006:
			return DrvInputs[0] >> 8;

		case 0x800007:
			return DrvInputs[0];

		case 0x800008:
			return DrvInputs[1] >> 8;

		case 0x800009:
			return DrvInputs[1];

		case 0x880000:
		case 0x880001:
		case 0x880002:
		case 0x880003:
			return YMZ280BRead((address/2)&1);
	}
	
	return 0;
}

static void bishi_tile_callback(INT32 layer, INT32 *code, INT32 *color, INT32 */*flags*/)
{
	*color = layer_colorbase[layer] + ((*color & 0xf0));
	*code &= 0x7fff;
}

static void bishi_sound_irq(INT32 status)
{
	SekSetIRQLine(1, (status) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	KonamiICReset();

	YMZ280BReset();

	for (INT32 i = 0; i < 4; i++) 
		layer_colorbase[i] = i << 6;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x100000;

	DrvGfxROM		= Next; Next += 0x200000;
	DrvGfxROMExp		= Next; Next += 0x200000;

	YMZ280BROM		= Next; Next += 0x200000;

	konami_palette32	= (UINT32*)Next;
	DrvPalette		= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x008000;
	DrvPalRAM		= Next; Next += 0x004000;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[8] = { 8*7,8*3,8*5,8*1,8*6,8*2,8*4,8*0 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,64) };

	GfxDecode(0x8000, 8,  8,  8, Plane, XOffs, YOffs, 8*8*8, DrvGfxROM, DrvGfxROMExp);

	return 0;
}

static INT32 DrvInit()
{
	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000000,  0, 1)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x080000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM  + 0x000000,  2, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x000001,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x100000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x100001,  5, 2)) return 1;

		if (BurnLoadRom(YMZ280BROM + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(YMZ280BROM + 0x080000,  7, 1)) return 1;
		if (BurnLoadRom(YMZ280BROM + 0x100000,  8, 1)) return 1;
		if (BurnLoadRom(YMZ280BROM + 0x180000,  9, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x400000, 0x407fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0xb00000, 0xb03fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0xb04000, 0xb047ff, MAP_ROM);
	SekSetWriteWordHandler(0,	bishi_write_word);
	SekSetWriteByteHandler(0,	bishi_write_byte);
	SekSetReadWordHandler(0,	bishi_read_word);
	SekSetReadByteHandler(0,	bishi_read_byte);
	SekClose();

	K055555Init();
	K054338Init();

	K056832Init(DrvGfxROM, DrvGfxROMExp, 0x200000, bishi_tile_callback);
	K056832SetGlobalOffsets(29, 16);
	K056832SetLayerOffsets(0, -2, 0);
	K056832SetLayerOffsets(1,  2, 0);
	K056832SetLayerOffsets(2,  4, 0);
	K056832SetLayerOffsets(3,  6, 0);
	K056832SetLayerAssociation(0);

	YMZ280BInit(16934400, bishi_sound_irq);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	KonamiICExit();

	SekExit();

	YMZ280BExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteRecalc()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x4000/2; i+=2) {
		UINT32 p = (BURN_ENDIAN_SWAP_INT16(pal[i]) << 16) | (BURN_ENDIAN_SWAP_INT16(pal[i+1]));

		UINT8 r = p & 0xff;
		UINT8 g = p >> 8;
		UINT8 b = p >> 16;

		DrvPalette[i/2] = (r << 16) | (g << 8) | b;
	}
}

static INT32 DrvDraw()
{
	DrvPaletteRecalc();

	INT32 layers[4];
	static const INT32 pris[4] = { K55_PRIINP_0, K55_PRIINP_3, K55_PRIINP_6, K55_PRIINP_7 };
	static const INT32 enables[4] = { K55_INP_VRAM_A, K55_INP_VRAM_B, K55_INP_VRAM_C, K55_INP_VRAM_D };

	KonamiClearBitmaps(0);

	K054338_fill_solid_bg();

	for (INT32 i = 0; i < 4; i++)
	{
		layers[i] = i;
		layerpri[i] = K055555ReadRegister(pris[i]);
	}

	konami_sortlayers4(layers, layerpri);

	for (INT32 i = 0; i < 4; i++)
	{
		if (K055555ReadRegister(K55_INPUT_ENABLES) & enables[layers[i]])
		{
			if (nBurnLayer & (1 << i)) K056832Draw(layers[i], 0, 1 << i);
		}
	}

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 2 * sizeof(INT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 12000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += SekRun(nCyclesTotal[0] / nInterleave);

		if (control_data & 0x800) {
			if (i == 0) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
			if (i == 240) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);

		}
	}

	if (pBurnSoundOut) {
		YMZ280BRender(pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
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

		YMZ280BScan(nAction, pnMin);

		KonamiICScan(nAction);

		SCAN_VAR(control_data);
		SCAN_VAR(control_data2);
	}

	return 0;
}


// Bishi Bashi Championship Mini Game Senshuken (ver JAA, 3 Players)

static struct BurnRomInfo bishiRomDesc[] = {
	{ "575jaa05.12e",	0x80000, 0x7d354567, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "575jaa06.15e",	0x80000, 0x9b2f7fbb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "575jaa07.14n",	0x80000, 0x37bbf387, 2 | BRF_GRA },           //  2 K056832 Characters
	{ "575jaa08.17n",	0x80000, 0x47ecd559, 2 | BRF_GRA },           //  3
	{ "575jaa09.19n",	0x80000, 0xc1db6e68, 2 | BRF_GRA },           //  4
	{ "575jaa10.22n",	0x80000, 0xc8b145d6, 2 | BRF_GRA },           //  5

	{ "575jaa01.2f",	0x80000, 0xe1e9f7b2, 3 | BRF_SND },           //  6 YMZ280b Samples
	{ "575jaa02.4f",	0x80000, 0xd228eb06, 3 | BRF_SND },           //  7
	{ "575jaa03.6f",	0x80000, 0x9ec0321f, 3 | BRF_SND },           //  8
	{ "575jaa04.8f",	0x80000, 0x0120967f, 3 | BRF_SND },           //  9
};

STD_ROM_PICK(bishi)
STD_ROM_FN(bishi)

struct BurnDriver BurnDrvBishi = {
	"bishi", NULL, NULL, NULL, "1996",
	"Bishi Bashi Championship Mini Game Senshuken (ver JAA, 3 Players)\0", "Imperfect gfx (one gfx rom bad, bad priorities)", "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_MINIGAMES, 0,
	NULL, bishiRomInfo, bishiRomName, NULL, NULL, NULL, NULL, BishiInputInfo, BishiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Super Bishi Bashi Championship (ver JAA, 2 Players)

static struct BurnRomInfo sbishiRomDesc[] = {
	{ "675jaa05.12e",	0x80000, 0x28a09c01, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "675jaa06.15e",	0x80000, 0xe4998b33, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "675jaa07.14n",	0x80000, 0x6fe7c658, 2 | BRF_GRA },           //  2 K056832 Characters
	{ "675jaa08.17n",	0x80000, 0xc230afc9, 2 | BRF_GRA },           //  3
	{ "675jaa09.19n",	0x80000, 0x63fe85a5, 2 | BRF_GRA },           //  4
	{ "675jaa10.22n",	0x80000, 0x703ac462, 2 | BRF_GRA },           //  5

	{ "675jaa01.2f",	0x80000, 0x67910b15, 3 | BRF_SND },           //  6 YMZ280b Samples
	{ "675jaa02.4f",	0x80000, 0x3313a7ae, 3 | BRF_SND },           //  7
	{ "675jaa03.6f",	0x80000, 0xec977e6a, 3 | BRF_SND },           //  8
	{ "675jaa04.8f",	0x80000, 0x1d1de34e, 3 | BRF_SND },           //  9
};

STD_ROM_PICK(sbishi)
STD_ROM_FN(sbishi)

struct BurnDriver BurnDrvSbishi = {
	"sbishi", NULL, NULL, NULL, "1998",
	"Super Bishi Bashi Championship (ver JAA, 2 Players)\0", "Imperfect gfx (bad priorities)", "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_MINIGAMES, 0,
	NULL, sbishiRomInfo, sbishiRomName, NULL, NULL, NULL, NULL, Bishi2pInputInfo, Bishi2pDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Super Bishi Bashi Championship (ver KAB, 3 Players)

static struct BurnRomInfo sbishikRomDesc[] = {
	{ "kab05.12e",		0x80000, 0x749063ca, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "kab06.15e",		0x80000, 0x089e0f37, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "675kaa07.14n",	0x80000, 0x1177c1f8, 2 | BRF_GRA },           //  2 K056832 Characters
	{ "675kaa08.17n",	0x80000, 0x7117e9cd, 2 | BRF_GRA },           //  3
	{ "675kaa09.19n",	0x80000, 0x8d49c765, 2 | BRF_GRA },           //  4
	{ "675kaa10.22n",	0x80000, 0xc16acf32, 2 | BRF_GRA },           //  5

	{ "675kaa01.2f",	0x80000, 0x73ac6ae6, 3 | BRF_SND },           //  6 YMZ280b Samples
	{ "675kaa02.4f",	0x80000, 0x4c341e7c, 3 | BRF_SND },           //  7
	{ "675kaa03.6f",	0x80000, 0x83f91beb, 3 | BRF_SND },           //  8
	{ "675kaa04.8f",	0x80000, 0xebcbd813, 3 | BRF_SND },           //  9
};

STD_ROM_PICK(sbishik)
STD_ROM_FN(sbishik)

struct BurnDriver BurnDrvSbishik = {
	"sbishik", "sbishi", NULL, NULL, "1998",
	"Super Bishi Bashi Championship (ver KAB, 3 Players)\0", "Imperfect gfx (bad priorities)", "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_MINIGAMES, 0,
	NULL, sbishikRomInfo, sbishikRomName, NULL, NULL, NULL, NULL, BishiInputInfo, BishiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Super Bishi Bashi Championship (ver KAA, 3 Players)

static struct BurnRomInfo sbishikaRomDesc[] = {
	{ "675kaa05.12e",	0x80000, 0x23600e1d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "675kaa06.15e",	0x80000, 0xbd1091f5, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "675kaa07.14n",	0x80000, 0x1177c1f8, 2 | BRF_GRA },           //  2 K056832 Characters
	{ "675kaa08.17n",	0x80000, 0x7117e9cd, 2 | BRF_GRA },           //  3
	{ "675kaa09.19n",	0x80000, 0x8d49c765, 2 | BRF_GRA },           //  4
	{ "675kaa10.22n",	0x80000, 0xc16acf32, 2 | BRF_GRA },           //  5

	{ "675kaa01.2f",	0x80000, 0x73ac6ae6, 3 | BRF_SND },           //  6 YMZ280b Samples
	{ "675kaa02.4f",	0x80000, 0x4c341e7c, 3 | BRF_SND },           //  7
	{ "675kaa03.6f",	0x80000, 0x83f91beb, 3 | BRF_SND },           //  8
	{ "675kaa04.8f",	0x80000, 0xebcbd813, 3 | BRF_SND },           //  9
};

STD_ROM_PICK(sbishika)
STD_ROM_FN(sbishika)

struct BurnDriver BurnDrvSbishika = {
	"sbishika", "sbishi", NULL, NULL, "1998",
	"Super Bishi Bashi Championship (ver KAA, 3 Players)\0", "Imperfect gfx (bad priorities)", "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_MINIGAMES, 0,
	NULL, sbishikaRomInfo, sbishikaRomName, NULL, NULL, NULL, NULL, BishiInputInfo, BishiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};
