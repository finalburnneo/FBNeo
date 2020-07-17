// FB Alpha X-Men driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "konamiic.h"
#include "burn_ym2151.h"
#include "k054539.h"
#include "eeprom.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROMExp0;
static UINT8 *DrvGfxROMExp1;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvEEPROM;

static UINT32 *DrvBitmaps[3];

static UINT8 *DrvSprRAM[2];
static UINT8 *DrvTMapRAM[4];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *soundlatch;
static UINT8 *soundlatch2;
static UINT8 *nDrvZ80Bank;

static INT32 interrupt_enable;

static INT32 sprite_colorbase;
static INT32 bg_colorbase;
static INT32 layerpri[3];
static INT32 layer_colorbase[3];
static INT32 tilemap_select;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvReset;
static UINT16 DrvInputs[4];

static struct BurnInputInfo XmenInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy2 + 15,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 10,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p3 fire 3"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy1 + 15,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy3 + 11,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p4 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Diagnostics",		BIT_DIGITAL,	DrvJoy3 + 14,	"diag"		},
};

STDINPUTINFO(Xmen)

static struct BurnInputInfo Xmen2pInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Diagnostics",		BIT_DIGITAL,	DrvJoy3 + 14,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"service2"	},
};

STDINPUTINFO(Xmen2p)

static struct BurnInputInfo Xmen6pInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy2 + 15,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 10,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p3 fire 3"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy1 + 15,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy3 + 11,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p4 fire 3"	},

	{"P5 Coin",			BIT_DIGITAL,	DrvJoy4 + 7,	"p5 coin"	},
	{"P5 Start",		BIT_DIGITAL,	DrvJoy3 + 12,	"p5 start"	},
	{"P5 Up",			BIT_DIGITAL,	DrvJoy4 + 0,	"p5 up"		},
	{"P5 Down",			BIT_DIGITAL,	DrvJoy4 + 1,	"p5 down"	},
	{"P5 Left",			BIT_DIGITAL,	DrvJoy4 + 2,	"p5 left"	},
	{"P5 Right",		BIT_DIGITAL,	DrvJoy4 + 3,	"p5 right"	},
	{"P5 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p5 fire 1"	},
	{"P5 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p5 fire 2"	},
	{"P5 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p5 fire 3"	},

	{"P6 Coin",			BIT_DIGITAL,	DrvJoy4 + 15,	"p6 coin"	},
	{"P6 Start",		BIT_DIGITAL,	DrvJoy3 + 13,	"p6 start"	},
	{"P6 Up",			BIT_DIGITAL,	DrvJoy4 + 8,	"p6 up"		},
	{"P6 Down",			BIT_DIGITAL,	DrvJoy4 + 9,	"p6 down"	},
	{"P6 Left",			BIT_DIGITAL,	DrvJoy4 + 10,	"p6 left"	},
	{"P6 Right",		BIT_DIGITAL,	DrvJoy4 + 11,	"p6 right"	},
	{"P6 Button 1",		BIT_DIGITAL,	DrvJoy4 + 12,	"p6 fire 1"	},
	{"P6 Button 2",		BIT_DIGITAL,	DrvJoy4 + 13,	"p6 fire 2"	},
	{"P6 Button 3",		BIT_DIGITAL,	DrvJoy4 + 14,	"p6 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Diagnostics",		BIT_DIGITAL,	DrvJoy3 + 14,	"diag"		},
};

STDINPUTINFO(Xmen6p)

static void __fastcall xmen_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x108000:
			K052109RMRDLine = data & 0x02;
			K053246_set_OBJCHA_line(data & 0x01);
		return;

		case 0x108001:
			tilemap_select = (data & 0x80) >> 6; // bit7 hi = tilemap 2,3 select, bit7 lo = tilemap 0,1 -dink July 2020
			EEPROMWrite(data & 0x08, data & 0x10, data & 0x04);
		return;

		case 0x10804d:
			*soundlatch = data;
		return;

		case 0x10804e:
		case 0x10804f:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x10a000: // nop?
		return;

		case 0x10a001:
			// watchdog
		return;

		case 0x18fa01:
			interrupt_enable = data & 0x04;
		return;
	}

	if (address >= 0x18c000 && address <= 0x197fff) {
		if (address & 1) K052109Write((address - 0x18c000) >> 1, data);
		return;
	}

	if ((address & 0xfff000) == 0x100000) {
		K053247Write((address & 0xfff) ^ 1, data);
		return;
	}

	if ((address & 0xfffff8) == 0x108020) {
		K053246Write((address & 0x007)^1, data);
		return;
	}

	if ((address & 0xffffe0) == 0x108060) {
		if (address & 1) K053251Write((address >> 1) & 0x0f, data);
		return;
	}
}

static void __fastcall xmen_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff000) == 0x100000) {
		K053247Write(address & 0xffe, data | 0x10000);
		return;
	}

	if ((address & 0xfffff8) == 0x108020) {
		K053246Write((address & 0x006), 0x10000|data);
		return;
	}
}

static UINT8 __fastcall xmen_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x108054:
		case 0x108055:
			return *soundlatch2;

		case 0x10a000:
		case 0x10a001:
			return DrvInputs[0] >> ((~address & 1) << 3);

		case 0x10a002:
		case 0x10a003:
			return DrvInputs[1] >> ((~address & 1) << 3);

		case 0x10a004:
			return ((DrvInputs[2] >> 8) & 0x7f) | ((GetCurrentFrame() & 1) << 7);

		case 0x10a005:
			return (DrvInputs[2] & 0xbf) | (EEPROMRead() << 6);

		case 0x10a006:
		case 0x10a007:
			return DrvInputs[3] >> ((~address & 1) << 3);

		case 0x10a00c:
		case 0x10a00d:
			return K053246Read(~address & 1);
	}

	if ((address & 0xfff000) == 0x100000) {
		return K053247Read((address & 0xfff)^1);
	}

	if (address >= 0x18c000 && address <= 0x197fff) {
		return K052109Read((address - 0x18c000) >> 1);
	}

	return 0;
}

static UINT16 __fastcall xmen_main_read_word(UINT32 address)
{
	switch (address) {
		case 0x10a00c:
			return K053246Read(1) | (K053246Read(0) << 8);
	}

	return 0;
}

static void bankswitch(INT32 bank)
{
	nDrvZ80Bank[0] = bank & 7;

	ZetMapMemory(DrvZ80ROM + nDrvZ80Bank[0] * 0x4000, 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall xmen_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe800:
		case 0xec00:
			BurnYM2151SelectRegister(data);
		return;

		case 0xe801:
		case 0xec01:
			BurnYM2151WriteRegister(data);
		return;

		case 0xf000:
			*soundlatch2 = data;
		return;

		case 0xf800:
			bankswitch(data);
		return;
	}

	if (address >= 0xe000 && address <= 0xe22f) {
		return K054539Write(0, address & 0x3ff, data);
	}
}

static UINT8 __fastcall xmen_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe800:
		case 0xe801:
		case 0xec00:
		case 0xec01:
			return BurnYM2151Read();

		case 0xf002:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch;
	}

	if (address >= 0xe000 && address <= 0xe22f) {
		return K054539Read(0, address & 0x3ff);
	}

	return 0;
}

static void K052109Callback(INT32 layer, INT32 , INT32 *, INT32 *color, INT32 *, INT32 *)
{
	if (layer == 0)
		*color = layer_colorbase[layer] + ((*color & 0xf0) >> 4);
	else
		*color = layer_colorbase[layer] + ((*color & 0x7c) >> 2);
}

static void XmenK053247Callback(INT32 *code, INT32 *color, INT32 *priority_mask)
{
	INT32 pri = (*color & 0x00e0) >> 4;
	if (pri <= layerpri[2])					*priority_mask = 0x00;
	else if (pri > layerpri[2] && pri <= layerpri[1])	*priority_mask = 0xf0;
	else if (pri > layerpri[1] && pri <= layerpri[0])	*priority_mask = 0xfc;
	else 							*priority_mask = 0xfe;

	*color = (sprite_colorbase + (*color & 0x001f)) & 0x7f;
	*code &= 0x7fff;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnYM2151Reset();
	K054539Reset(0);

	KonamiICReset();

	EEPROMReset();

	if (EEPROMAvailable() == 0) {
		EEPROMFill(DrvEEPROM, 0, 0x80);
	}

	interrupt_enable = 0;
	tilemap_select = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x100000;
	DrvZ80ROM			= Next; Next += 0x020000;

	DrvGfxROM0			= Next; Next += 0x200000;
	DrvGfxROMExp0		= Next; Next += 0x400000;
	DrvGfxROM1			= Next; Next += 0x400000;
	DrvGfxROMExp1		= Next; Next += 0x800000;

	DrvSndROM			= Next; Next += 0x200000;
	konami_palette32	= (UINT32*)Next;
	DrvPalette			= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	DrvEEPROM			= Next; Next += 0x0000100;

	AllRam				= Next;

	if (nScreenWidth != 288)
	{
		DrvSprRAM[0]	= Next; Next += 0x002000;
		DrvSprRAM[1]	= Next; Next += 0x002000;
		DrvTMapRAM[0]	= Next; Next += 0x00c000;
		DrvTMapRAM[1]	= Next; Next += 0x00c000;
		DrvTMapRAM[2]	= Next; Next += 0x00c000;
		DrvTMapRAM[3]	= Next; Next += 0x00c000;
	}

	DrvPalRAM			= Next; Next += 0x001000;
	Drv68KRAM			= Next; Next += 0x005000;

	DrvZ80RAM			= Next; Next += 0x002000;

	soundlatch			= Next; Next += 0x000001;
	soundlatch2			= Next; Next += 0x000001;

	nDrvZ80Bank			= Next; Next += 0x000001;

	RamEnd				= Next;

	DrvBitmaps[0]		= (UINT32*)Next; Next += 512 * 256 * sizeof(UINT32);
	DrvBitmaps[1]		= (UINT32*)Next; Next += 512 * 256 * sizeof(UINT32);

	MemEnd				= Next;

	return 0;
}

static const eeprom_interface xmen_eeprom_intf =
{
	7,		 // address bits
	8,		 // data bits
	"011000",	 // read command
	"011100",	 // write command
	0,		 // erase command
	"0100000000000", // lock command
	"0100110000000", // unlock command
	0,
	0
};

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
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x080001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x080000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  5, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  6, 4, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  7, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  8, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004,  9, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006, 10, 8, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 11, 1)) return 1;

		if (BurnLoadRom(DrvEEPROM  + 0x000000, 12, 1)) return 1;

		K052109GfxDecode(DrvGfxROM0, DrvGfxROMExp0, 0x200000);
		K053247GfxDecode(DrvGfxROM1, DrvGfxROMExp1, 0x400000);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,				0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,				0x104000, 0x104fff, MAP_RAM);
	SekMapMemory(Drv68KRAM + 0x01000,	0x110000, 0x113fff, MAP_RAM);

	if (nScreenWidth != 288)
	{
		SekMapMemory(DrvSprRAM[0],			0x100000, 0x101fff, MAP_RAM); // only 0-fff used!!
		SekMapMemory(DrvSprRAM[1],			0x102000, 0x103fff, MAP_RAM); // only 0-fff used!!
		SekMapMemory(DrvTMapRAM[0],			0x18c000, 0x197fff, MAP_RAM);
		SekMapMemory(DrvTMapRAM[1],			0x1ac000, 0x1b7fff, MAP_RAM);
		SekMapMemory(DrvTMapRAM[2],			0x1cc000, 0x1d7fff, MAP_RAM);
		SekMapMemory(DrvTMapRAM[3],			0x1ec000, 0x1f7fff, MAP_RAM);
	} else {
		SekMapMemory(Drv68KRAM + 0x00000,	0x101000, 0x101fff, MAP_RAM);
	}

	SekSetWriteByteHandler(0,			xmen_main_write_byte);
	SekSetWriteWordHandler(0,			xmen_main_write_word);
	SekSetReadByteHandler(0,			xmen_main_read_byte);
	SekSetReadWordHandler(0,			xmen_main_read_word);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(xmen_sound_write);
	ZetSetReadHandler(xmen_sound_read);
	ZetClose();

	EEPROMInit(&xmen_eeprom_intf);

	K052109Init(DrvGfxROM0, DrvGfxROMExp0, 0x1fffff);
	K052109SetCallback(K052109Callback);
	K052109AdjustScroll(8, 0);

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x3fffff, XmenK053247Callback, 1);
	K053247SetSpriteOffset(514, -158);

	BurnYM2151Init(4000000);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.20, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.20, BURN_SND_ROUTE_RIGHT);

	K054539Init(0, 48000, DrvSndROM, 0x200000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	DrvBitmaps[2] = konami_bitmap32;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	KonamiICExit();

	SekExit();
	ZetExit();

	BurnYM2151Exit();
	K054539Exit();

	EEPROMExit();

	BurnFree (AllMem);

	return 0;
}

static inline void DrvRecalcPalette()
{
	UINT8 r,g,b;
	UINT16 *p = (UINT16*)DrvPalRAM;
	for (INT32 i = 0; i < 0x1000 / 2; i++) {
		INT32 d = BURN_ENDIAN_SWAP_INT16(p[i]);

		r = (d >>  0) & 0x1f;
		g = (d >>  5) & 0x1f;
		b = (d >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = (r << 16) | (g << 8) | b;
	}
}

static INT32 draw_side(INT32 side)
{
	INT32 width = nScreenWidth;
	UINT8 temp_reg = 0;

	if (width != 288)
	{
		// Every other frame xmen6p sets the lsb of the sprite xoffset to 3,
		// this causes the sprites to jiggle when we update both screens per frame.
		// this is a hack so we can update both screens per frame.
		temp_reg = K053246ReadRegs(1);
		K053246Write(1, 0);

		K053247SetSpriteOffset((side) ? (228-5) : 511, -158);
		K052109AdjustScroll((side) ? 24 : -8, 0);

		nScreenWidth = 512;
		konami_bitmap32 = DrvBitmaps[side];

		memcpy (K053247Ram, DrvSprRAM[side], 0x1000);

		for (INT32 i = 0; i < 0xc000/2; i++)
		{
			// tilemap 0 (@ 0x18c000) is master, only it can control certain registers. -dink july 2020
			if ((side == 1 || tilemap_select) && (i != 0x1c80 && i != 0x1e80)) {
				K052109Write(i, *((UINT16*)(DrvTMapRAM[side+tilemap_select] + i * 2)) & 0xff);
			} else if (side == 0) {
				K052109Write(i, *((UINT16*)(DrvTMapRAM[side] + i * 2)) & 0xff);
			}
		}
	}

	K052109UpdateScroll();

	INT32 layer[3];

	bg_colorbase       = K053251GetPaletteIndex(4);
	sprite_colorbase   = K053251GetPaletteIndex(1);
	layer_colorbase[0] = K053251GetPaletteIndex(3);
	layer_colorbase[1] = K053251GetPaletteIndex(0);
	layer_colorbase[2] = K053251GetPaletteIndex(2);

	layerpri[0] = K053251GetPriority(3);
	layerpri[1] = K053251GetPriority(0);
	layerpri[2] = K053251GetPriority(2);
	layer[0] = 0;
	layer[1] = 1;
	layer[2] = 2;

	konami_sortlayers3(layer,layerpri);

	KonamiClearBitmaps(DrvPalette[16 * bg_colorbase+1]);

	if (nBurnLayer & 1) K052109RenderLayer(layer[0], 0, 1);
	if (nBurnLayer & 2) K052109RenderLayer(layer[1], 0, 2);
	if (nBurnLayer & 4) K052109RenderLayer(layer[2], 0, 4);

	if (nSpriteEnable & 1) K053247SpritesRender();

	if (width != 288)
	{
		nScreenWidth = 288 * 2;
		konami_bitmap32 = DrvBitmaps[2];

		{
			INT32 woffs = (side & 1) * 288;
			UINT32 *src = DrvBitmaps[side & 1];
			UINT32 *dst = konami_bitmap32 + woffs;

			for (INT32 y = 0; y < nScreenHeight; y++)
			{
				for (INT32 x = 0; x < 288; x++)
				{
					dst[x] = src[x];
				}
				dst += 288 * 2;
				src += 512;
			}
		}
		K053246Write(1, temp_reg);
	}

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
	}

	if (nScreenWidth != 288) {
		draw_side(1);
	}
	draw_side(0);

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			//DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;   // handled below..
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		{ // start button frame-round-robin - prevent more than 1 start
			// from falling on any given frame (causes crash in-game)

			INT32 button = (nCurrentFrame % 24) / 4;
			DrvInputs[2] ^= (DrvJoy3[8+button] & 1) << (8+button);
			DrvInputs[2] ^= (DrvJoy3[14] & 1) << 14; // f2/service mode
		}

	 	// Clear Opposites
		if ((DrvInputs[0] & 0x000c) == 0) DrvInputs[0] |= 0x000c;
		if ((DrvInputs[0] & 0x0003) == 0) DrvInputs[0] |= 0x0003;
		if ((DrvInputs[0] & 0x0c00) == 0) DrvInputs[0] |= 0x0c00;
		if ((DrvInputs[0] & 0x0300) == 0) DrvInputs[0] |= 0x0300;
		if ((DrvInputs[1] & 0x000c) == 0) DrvInputs[1] |= 0x000c;
		if ((DrvInputs[1] & 0x0003) == 0) DrvInputs[1] |= 0x0003;
		if ((DrvInputs[1] & 0x0c00) == 0) DrvInputs[1] |= 0x0c00;
		if ((DrvInputs[1] & 0x0300) == 0) DrvInputs[1] |= 0x0300;
	}

	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 16000000 / 60, 8000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, Sek);

		if (i == 239) {
			if (nScreenWidth != 288) interrupt_enable = *((UINT16*)(DrvTMapRAM[0] + 0x3a00)) & 0x0004;
			if (interrupt_enable) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
		}
		if (i == 255) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);

		CPU_RUN(1, Zet);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
		}
		K054539Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029705;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		ZetScan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		K054539Scan(nAction, pnMin);

		KonamiICScan(nAction);
		EEPROMScan(nAction, pnMin);

		SCAN_VAR(interrupt_enable);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(nDrvZ80Bank[0]);
		ZetClose();
	}

	EEPROMScan(nAction, pnMin);

	return 0;
}

// X-Men (4 Players ver EBA)

static struct BurnRomInfo xmenRomDesc[] = {
	{ "065-eba04.10d",	0x020000, 0x3588c5ec, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-eba05.10f",	0x020000, 0x79ce32f8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.9d",		0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.9f",		0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.6f",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.15l",	0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.16l",	0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.2h",		0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.2l",		0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.1h",		0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.1l",		0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1f",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_eba.nv",    0x000080, 0x37f8e77a, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmen)
STD_ROM_FN(xmen)

struct BurnDriver BurnDrvXmen = {
	"xmen", NULL, NULL, NULL, "1992",
	"X-Men (4 Players ver EBA)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmenRomInfo, xmenRomName, NULL, NULL, NULL, NULL, XmenInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// X-Men (4 Players ver UBB)

static struct BurnRomInfo xmenuRomDesc[] = {
	{ "065-ubb04.10d",	0x020000, 0xf896c93b, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-ubb05.10f",	0x020000, 0xe02e5d64, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.9d",		0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.9f",		0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.6f",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.15l",	0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.16l",	0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.2h",		0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.2l",		0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.1h",		0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.1l",		0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1f",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_ubb.nv",    0x000080, 0x52f334ba, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmenu)
STD_ROM_FN(xmenu)

struct BurnDriver BurnDrvXmenu = {
	"xmenu", "xmen", NULL, NULL, "1992",
	"X-Men (4 Players ver UBB)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmenuRomInfo, xmenuRomName, NULL, NULL, NULL, NULL, XmenInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// X-Men (4 Players ver UEB)

static struct BurnRomInfo xmenuaRomDesc[] = {
	{ "065-ueb04.10d",	0x020000, 0xeee4e7ef, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-ueb05.10f",	0x020000, 0xc3b2ffde, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.9d",		0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.9f",		0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.6f",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.15l",	0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.16l",	0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.2h",		0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.2l",		0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.1h",		0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.1l",		0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1f",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_ueb.nv",    0x000080, 0xdb85fef4, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmenua)
STD_ROM_FN(xmenua)

struct BurnDriver BurnDrvXmenua = {
	"xmenua", "xmen", NULL, NULL, "1992",
	"X-Men (4 Players ver UEB)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmenuaRomInfo, xmenuaRomName, NULL, NULL, NULL, NULL, XmenInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// X-Men (4 Players ver AEA)

static struct BurnRomInfo xmenaRomDesc[] = {
	{ "065-aea04.10d",	0x020000, 0x0e8d2e98, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-aea05.10f",	0x020000, 0x0b742a4e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.9d",		0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.9f",		0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.6f",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.15l",	0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.16l",	0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.2h",		0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.2l",		0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.1h",		0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.1l",		0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1f",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_aea.nv",    0x000080, 0xd73d4f20, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmena)
STD_ROM_FN(xmena)

struct BurnDriver BurnDrvXmena = {
	"xmena", "xmen", NULL, NULL, "1992",
	"X-Men (4 Players ver AEA)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmenaRomInfo, xmenaRomName, NULL, NULL, NULL, NULL, XmenInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// X-Men (4 Players ver ADA)

static struct BurnRomInfo xmenaaRomDesc[] = {
	{ "065-ada04.10d",	0x020000, 0xb8276624, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-ada05.10f",	0x020000, 0xc68582ad, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.9d",		0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.9f",		0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.6f",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.15l",	0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.16l",	0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.2h",		0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.2l",		0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.1h",		0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.1l",		0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1f",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_ada.nv",    0x000080, 0xa77a3891, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmenaa)
STD_ROM_FN(xmenaa)

struct BurnDriver BurnDrvXmenaa = {
	"xmenaa", "xmen", NULL, NULL, "1992",
	"X-Men (4 Players ver ADA)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmenaaRomInfo, xmenaaRomName, NULL, NULL, NULL, NULL, XmenInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// X-Men (4 Players ver JBA)

static struct BurnRomInfo xmenjRomDesc[] = {
	{ "065-jba04.10d",	0x020000, 0xd86cf5eb, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-jba05.10f",	0x020000, 0xabbc8126, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.9d",		0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.9f",		0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.6f",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.15l",	0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.16l",	0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.2h",		0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.2l",		0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.1h",		0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.1l",		0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1f",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_jba.nv",    0x000080, 0x7439cea7, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmenj)
STD_ROM_FN(xmenj)

struct BurnDriver BurnDrvXmenj = {
	"xmenj", "xmen", NULL, NULL, "1992",
	"X-Men (4 Players ver JBA)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmenjRomInfo, xmenjRomName, NULL, NULL, NULL, NULL, XmenInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// X-Men (4 Players ver JEA)

static struct BurnRomInfo xmenjaRomDesc[] = {
	{ "065-jea04.10d",	0x020000, 0x655a61d6, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-jea05.10f",	0x020000, 0x7ea9fc84, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.9d",		0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.9f",		0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.6f",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.15l",	0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.16l",	0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.2h",		0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.2l",		0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.1h",		0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.1l",		0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1f",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_jea.nv",    0x000080, 0xdf5b6bc6, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmenja)
STD_ROM_FN(xmenja)

struct BurnDriver BurnDrvXmenja = {
	"xmenja", "xmen", NULL, NULL, "1992",
	"X-Men (4 Players ver JEA)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmenjaRomInfo, xmenjaRomName, NULL, NULL, NULL, NULL, XmenInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// X-Men (2 Players ver EAA)

static struct BurnRomInfo xmen2peRomDesc[] = {
	{ "065-eaa04.10d",	0x020000, 0x502861e7, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-eaa05.10f",	0x020000, 0xca6071bf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.9d",		0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.9f",		0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.6f",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.15l",	0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.16l",	0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.2h",		0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.2l",		0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.1h",		0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.1l",		0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1f",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_eaa.nv",    0x000080, 0x1cbcb653, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmen2pe)
STD_ROM_FN(xmen2pe)

struct BurnDriver BurnDrvXmen2pe = {
	"xmen2pe", "xmen", NULL, NULL, "1992",
	"X-Men (2 Players ver EAA)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmen2peRomInfo, xmen2peRomName, NULL, NULL, NULL, NULL, Xmen2pInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// X-Men (2 Players ver UAB)

static struct BurnRomInfo xmen2puRomDesc[] = {
	{ "065-uab04.10d",	0x020000, 0xff003db1, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-uab05.10f",	0x020000, 0x4e99a943, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.9d",		0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.9f",		0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.6f",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.15l",	0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.16l",	0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.2h",		0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.2l",		0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.1h",		0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.1l",		0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1f",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_uab.nv",    0x000080, 0x79b76593, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmen2pu)
STD_ROM_FN(xmen2pu)

struct BurnDriver BurnDrvXmen2pu = {
	"xmen2pu", "xmen", NULL, NULL, "1992",
	"X-Men (2 Players ver UAB)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmen2puRomInfo, xmen2puRomName, NULL, NULL, NULL, NULL, Xmen2pInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// X-Men (2 Players ver AAA)

static struct BurnRomInfo xmen2paRomDesc[] = {
	{ "065-aaa04.10d",	0x020000, 0x7f8b27c2, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-aaa05.10f",	0x020000, 0x841ed636, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.9d",		0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.9f",		0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.6f",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.15l",	0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.16l",	0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.2h",		0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.2l",		0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.1h",		0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.1l",		0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1f",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_aaa.nv",    0x000080, 0x750fd447, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmen2pa)
STD_ROM_FN(xmen2pa)

struct BurnDriver BurnDrvXmen2pa = {
	"xmen2pa", "xmen", NULL, NULL, "1992",
	"X-Men (2 Players ver AAA)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmen2paRomInfo, xmen2paRomName, NULL, NULL, NULL, NULL, Xmen2pInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// X-Men (2 Players ver JAA)

static struct BurnRomInfo xmen2pjRomDesc[] = {
	{ "065-jaa04.10d",	0x020000, 0x66746339, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-jaa05.10f",	0x020000, 0x1215b706, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.9d",		0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.9f",		0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.6f",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.15l",	0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.16l",	0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.2h",		0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.2l",		0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.1h",		0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.1l",		0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1f",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_jaa.nv",    0x000080, 0x849a9e19, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmen2pj)
STD_ROM_FN(xmen2pj)

struct BurnDriver BurnDrvXmen2pj = {
	"xmen2pj", "xmen", NULL, NULL, "1992",
	"X-Men (2 Players ver JAA)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmen2pjRomInfo, xmen2pjRomName, NULL, NULL, NULL, NULL, Xmen2pInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// X-Men (6 Players ver ECB)

static struct BurnRomInfo xmen6pRomDesc[] = {
	{ "065-ecb04.18g",	0x020000, 0x258eb21f, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-ecb05.18j",	0x020000, 0x25997bcd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.17g",	0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.17j",	0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.7b",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.1l",		0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.1h",		0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.12l",	0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.17l",	0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.22h",	0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.22l",	0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1d",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_ecb.nv",    0x000080, 0x462c6e1a, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmen6p)
STD_ROM_FN(xmen6p)

struct BurnDriver BurnDrvXmen6p = {
	"xmen6p", "xmen", NULL, NULL, "1992",
	"X-Men (6 Players ver ECB)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 6, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmen6pRomInfo, xmen6pRomName, NULL, NULL, NULL, NULL, Xmen6pInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288*2, 224, 8, 3
};


// X-Men (6 Players ver UCB)

static struct BurnRomInfo xmen6puRomDesc[] = {
	{ "065-ucb04.18g",	0x020000, 0x0f09b8e0, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "065-ucb05.18j",	0x020000, 0x867becbf, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "065-a02.17g",	0x040000, 0xb31dc44c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "065-a03.17j",	0x040000, 0x13842fe6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "065-a01.7b",		0x020000, 0x147d3a4d, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "065-a08.1l",		0x100000, 0x6b649aca, 3 | BRF_GRA },           //  5 Background Tiles
	{ "065-a07.1h",		0x100000, 0xc5dc8fc4, 3 | BRF_GRA },           //  6

	{ "065-a09.12l",	0x100000, 0xea05d52f, 4 | BRF_GRA },           //  7 Sprites
	{ "065-a10.17l",	0x100000, 0x96b91802, 4 | BRF_GRA },           //  8
	{ "065-a12.22h",	0x100000, 0x321ed07a, 4 | BRF_GRA },           //  9
	{ "065-a11.22l",	0x100000, 0x46da948e, 4 | BRF_GRA },           // 10

	{ "065-a06.1d",		0x200000, 0x5adbcee0, 5 | BRF_SND },           // 11 K054539 Samples

	{ "xmen_ucb.nv",    0x000080, 0xf3d0f682, 6 | BRF_PRG | BRF_ESS }, // 12 Default Settings
};

STD_ROM_PICK(xmen6pu)
STD_ROM_FN(xmen6pu)

struct BurnDriver BurnDrvXmen6pu = {
	"xmen6pu", "xmen", NULL, NULL, "1992",
	"X-Men (6 Players ver UCB)\0", NULL, "Konami", "GX065",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 6, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, xmen6puRomInfo, xmen6puRomName, NULL, NULL, NULL, NULL, Xmen6pInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288*2, 224, 8, 3
};
