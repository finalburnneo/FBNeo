// FB Alpa Dragon Ball Z driver module
// Based on MAME driver by David Haywood, R. Belmont and Pierpaolo Prazzoli

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "konamiic.h"
#include "burn_ym2151.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROMExp0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROMExp1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROMExp2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROMExp3;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM0;
static UINT8 *Drv68KRAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvBg2RAM;
static UINT8 *DrvBg1RAM;
static UINT8 *Drvk053936RAM1;
static UINT8 *Drvk053936RAM2;
static UINT8 *DrvK053936Ctrl1;
static UINT8 *DrvK053936Ctrl2;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 layerpri[8];
static INT32 layer_colorbase[8];
static INT32 sprite_colorbase;

static UINT8 *soundlatch;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvReset;
static UINT16 DrvInputs[3];
static UINT8 DrvDips[3];

static UINT16 control_data;

static struct BurnInputInfo DbzInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 4"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Dbz)

static struct BurnDIPInfo DbzDIPList[]=
{
	{0x16, 0xff, 0xff, 0x08, NULL			},
	{0x17, 0xff, 0xff, 0x3f, NULL			},
	{0x18, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x08, 0x08, "Off"			},
	{0x16, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x17, 0x01, 0x03, 0x01, "Easy"			},
	{0x17, 0x01, 0x03, 0x03, "Normal"		},
	{0x17, 0x01, 0x03, 0x02, "Hard"			},
	{0x17, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x17, 0x01, 0x08, 0x08, "Off"			},
	{0x17, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x17, 0x01, 0x20, 0x20, "Off"			},
	{0x17, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x17, 0x01, 0x40, 0x00, "English"		},
	{0x17, 0x01, 0x40, 0x40, "Japanese"		},

	{0   , 0xfe, 0   ,    2, "Mask ROM Test"	},
	{0x17, 0x01, 0x80, 0x00, "Off"			},
	{0x17, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x18, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x18, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x18, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x18, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x18, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x18, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x18, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x18, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x18, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x18, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x18, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x18, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x18, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x18, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x18, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x18, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x18, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x18, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x18, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x18, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x18, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x18, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x18, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x18, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x18, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x18, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x18, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x18, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x18, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x18, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x18, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x18, 0x01, 0xf0, 0x00, "No Coin B"		},
};

STDDIPINFO(Dbz)

static struct BurnDIPInfo Dbz2DIPList[]=
{
	{0x16, 0xff, 0xff, 0x08, NULL			},
	{0x17, 0xff, 0xff, 0x3f, NULL			},
	{0x18, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x16, 0x01, 0x08, 0x08, "Off"			},
	{0x16, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x17, 0x01, 0x03, 0x01, "Easy"			},
	{0x17, 0x01, 0x03, 0x03, "Normal"		},
	{0x17, 0x01, 0x03, 0x02, "Hard"			},
	{0x17, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x17, 0x01, 0x04, 0x04, "Off"			},
	{0x17, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x17, 0x01, 0x08, 0x08, "Off"			},
	{0x17, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Level Select"		},
	{0x17, 0x01, 0x10, 0x10, "Off"			},
	{0x17, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x17, 0x01, 0x20, 0x20, "Off"			},
	{0x17, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x17, 0x01, 0x40, 0x00, "English"		},
	{0x17, 0x01, 0x40, 0x40, "Japanese"		},

	{0   , 0xfe, 0   ,    2, "Mask ROM Test"	},
	{0x17, 0x01, 0x80, 0x00, "Off"			},
	{0x17, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x18, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x18, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x18, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x18, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x18, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x18, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x18, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x18, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x18, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x18, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x18, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x18, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x18, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x18, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x18, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x18, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x18, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x18, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x18, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x18, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x18, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x18, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x18, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x18, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x18, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x18, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x18, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x18, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x18, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x18, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x18, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x18, 0x01, 0xf0, 0x00, "No Coin B"		},
};

STDDIPINFO(Dbz2)

static void __fastcall dbz_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffc000) == 0x490000) {
		K056832RamWriteWord(address & 0x1fff, data);
		return;
	}

	if ((address & 0xffbff8) == 0x4c0000) {
		K053246Write((address & 0x06) + 0, data >> 8);
		K053246Write((address & 0x06) + 1, data&0xff);
		return;
	}

	if ((address & 0xfffff8) == 0x4c8000) {
		return;	// regsb
	}
		
	if ((address & 0xffffc0) == 0x4cc000) {
		K056832WordWrite(address & 0x3e, data);
		return;
	}

	if ((address & 0xffffe0) == 0x4f8000) {
	//	k053252
		return;
	}

	switch (address)
	{
		case 0x4ec000:
			control_data = data;
			K053246_set_OBJCHA_line(data & 0x0400);
		return;

		case 0x4f0000:
			*soundlatch = data;
		return;

		case 0x4f4000:
			ZetNmi();
		return;
	}

//	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall dbz_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffc000) == 0x490000) {
		K056832RamWriteByte(address & 0x1fff, data);
		return;
	}

	if ((address & 0xffbff8) == 0x4c0000) {
		K053246Write((address & 0x07) ^ 0, data);
		return;
	}

	if ((address & 0xfffff8) == 0x4c8000) {
		return;	// regsb
	}

	if ((address & 0xffffc0) == 0x4cc000) {
		K056832ByteWrite(address & 0x3f, data);
		return;
	}

	if ((address & 0xffffe1) == 0x4fc001) {
		K053251Write((address / 2) & 0xf, data);
		return;
	}

	if ((address & 0xffffe0) == 0x4f8000) {
	//	k053252Write
		return;
	}

	switch (address)
	{
		case 0x4e8000:
		return;		// nop

		case 0x4ec000:
			control_data = (control_data & 0x00ff) + (data << 8);
			K053246_set_OBJCHA_line(data & 0x04);
		return;

		case 0x4ec001:
			control_data = (control_data & 0xff00) + data;
		return;

		case 0x4f0000:
		case 0x4f0001:
			*soundlatch = data;
		return;

		case 0x4f4000:
		case 0x4f4001:
			ZetNmi();
		return;
	}

//	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall dbz_main_read_word(UINT32 address)
{
	if ((address & 0xffc000) == 0x490000) {
		return K056832RamReadWord(address & 0x1fff);
	}

	if ((address & 0xff8000) == 0x498000) {
		return K056832RomWord8000Read(address);
	}

	if ((address & 0xffffe0) == 0x4f8000) {
		return 0; // k053252
	}

	switch (address)
	{
		case 0x4c0000:
			return K053246Read(1) + (K053246Read(0) << 8);

		case 0x4e0000:
			return DrvInputs[0];

		case 0x4e0002:
			return DrvInputs[1];

		case 0x4e4000:
			return DrvInputs[2];
	}

//	bprintf (0, _T("RW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall dbz_main_read_byte(UINT32 address)
{
	if ((address & 0xffc000) == 0x490000) {
		return K056832RamReadByte(address & 0x1fff);
	}

	if ((address & 0xff8000) == 0x498000) {
		return K056832RomWord8000Read(address) >> ((~address & 1) * 8);
	}

	if ((address & 0xffffe0) == 0x4f8000) {
		return 0; // k053252
	}

	switch (address)
	{
		case 0x4c0000:
		case 0x4c0001:
			return K053246Read(address & 1);

		case 0x4e0000:
			return DrvInputs[0] >> 8;

		case 0x4e0001:
			return DrvInputs[0];

		case 0x4e0002:
			return DrvInputs[1] >> 8;

		case 0x4e0003:
			return DrvInputs[1];

		case 0x4e4000:
			return DrvInputs[2] >> 8;

		case 0x4e4001:
			return DrvInputs[2];
	}

//	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static void __fastcall dbz_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
			BurnYM2151SelectRegister(data);
		return;

		case 0xc001:
			BurnYM2151WriteRegister(data);
		return;

		case 0xd000:
		case 0xd002:
		case 0xd001:
			MSM6295Command(0, data);
		return;
	}
}

static UINT8 __fastcall dbz_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
			return BurnYM2151ReadStatus();

		case 0xd000:
		case 0xd002:
		case 0xd001:
			return MSM6295ReadStatus(0);

		case 0xe000:
		case 0xe001:
			return *soundlatch;
	}

	return 0;
}

static void dbz_sprite_callback(INT32 */*code*/, INT32 *color, INT32 *priority)
{
	INT32 pri = (*color & 0x3c0) >> 5;

	if (pri <= layerpri[3])					*priority = 0xff00;
	else if (pri > layerpri[3] && pri <= layerpri[2])	*priority = 0xfff0;
	else if (pri > layerpri[2] && pri <= layerpri[1])	*priority = 0xfffc;
	else							*priority = 0xfffe;

	*color = (sprite_colorbase << 1) + (*color & 0x1f);
}

static void dbz_tile_callback(INT32 layer, INT32 */*code*/, INT32 *color, INT32 */*flags*/)
{
	*color = (layer_colorbase[layer] << 1) + ((*color & 0x3c) >> 2);
}

static void dbz_K053936_callback1(INT32 offset, UINT16 *ram, INT32 *code, INT32 *color, INT32 *, INT32 *, INT32 *flipx, INT32 *)
{
	*code  =  ram[(offset * 2) + 1] & 0x7fff;
	*color = ((ram[(offset * 2) + 0] & 0x000f) + (layer_colorbase[4] << 1)) << 4;
	*color &= 0x1ff0;
	*flipx =  ram[(offset * 2) + 0] & 0x0080;
}

static void dbz_K053936_callback2(INT32 offset, UINT16 *ram, INT32 *code, INT32 *color, INT32 *, INT32 *, INT32 *flipx, INT32 *)
{
	*code  =  ram[(offset * 2) + 1] & 0x7fff;
	*color = ((ram[(offset * 2) + 0] & 0x000f) + (layer_colorbase[5] << 1)) << 4;
	*color &= 0x1ff0;
	*flipx =  ram[(offset * 2) + 0] & 0x0080;
}

static void dbzYM2151IrqHandler(INT32 status)
{
	ZetSetIRQLine(0, (status) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
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

	KonamiICReset();

	MSM6295Reset(0);
	BurnYM2151Reset();

	control_data = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x100000;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x400000;
	DrvGfxROMExp0		= Next; Next += 0x800000;
	DrvGfxROM1		= Next; Next += 0x800000;
	DrvGfxROMExp1		= Next; Next += 0x1000000;
	DrvGfxROM2		= Next; Next += 0x400000;
	DrvGfxROMExp2		= Next; Next += 0x800000;
	DrvGfxROM3		= Next; Next += 0x400000;
	DrvGfxROMExp3		= Next; Next += 0x800000;

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x040000;

	konami_palette32	= (UINT32*)Next;
	DrvPalette		= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM0		= Next; Next += 0x010000;
	Drv68KRAM1		= Next; Next += 0x004000;
	DrvPalRAM		= Next; Next += 0x004000;

	DrvBg2RAM		= Next; Next += 0x002000;
	DrvBg1RAM		= Next; Next += 0x002000;

	DrvK053936Ctrl1		= Next; Next += 0x000400;
	DrvK053936Ctrl2		= Next; Next += 0x000400;

	Drvk053936RAM1		= Next; Next += 0x004000;
	Drvk053936RAM2		= Next; Next += 0x004000;

	DrvZ80RAM		= Next; Next += 0x004000;

	soundlatch		= Next; Next += 0x000001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvExpandGfx(UINT8 *dst, UINT8 *src, INT32 len, INT32 bs)
{
	for (INT32 i = (len - 1); i >= 0; i--) {
		dst[i*2+0] = src[i^bs] >> 4;
		dst[i*2+1] = src[i^bs] & 0xf;
	}
}

static void dbz_patch()
{
	UINT16 *ROM = (UINT16 *)Drv68KROM;

	// to avoid crash during loop at 0x00076e after D4 > 0x80 (reading tiles region out of bounds)
	ROM[0x76c/2] = 0x007f;    /* 0x00ff */
	// nop out dbz1's mask rom test
	// tile ROM test
	ROM[0x7b0/2] = 0x4e71;    /* 0x0c43 - cmpi.w  #-$1e0d, D3 */
	ROM[0x7b2/2] = 0x4e71;    /* 0xe1f3 */
	ROM[0x7b4/2] = 0x4e71;    /* 0x6600 - bne     $7d6 */
	ROM[0x7b6/2] = 0x4e71;    /* 0x0020 */
	ROM[0x7c0/2] = 0x4e71;    /* 0x0c45 - cmpi.w  #-$7aad, D5 */
	ROM[0x7c2/2] = 0x4e71;    /* 0x8553 */
	ROM[0x7c4/2] = 0x4e71;    /* 0x6600 - bne     $7d6 */
	ROM[0x7c6/2] = 0x4e71;    /* 0x0010 */
	// PSAC2 ROM test (A and B)
	ROM[0x9a8/2] = 0x4e71;    /* 0x0c43 - cmpi.w  #$43c0, D3 */
	ROM[0x9aa/2] = 0x4e71;    /* 0x43c0 */
	ROM[0x9ac/2] = 0x4e71;    /* 0x6600 - bne     $a00 */
	ROM[0x9ae/2] = 0x4e71;    /* 0x0052 */
	ROM[0x9ea/2] = 0x4e71;    /* 0x0c44 - cmpi.w  #-$13de, D4 */
	ROM[0x9ec/2] = 0x4e71;    /* 0xec22 */
	ROM[0x9ee/2] = 0x4e71;    /* 0x6600 - bne     $a00 */
	ROM[0x9f0/2] = 0x4e71;    /* 0x0010 */
	// prog ROM test
	ROM[0x80c/2] = 0x4e71;    /* 0xb650 - cmp.w   (A0), D3 */
	ROM[0x80e/2] = 0x4e71;    /* 0x6600 - bne     $820 */
	ROM[0x810/2] = 0x4e71;    /* 0x005e */
}

static void dbza_patch()
{
	UINT16 *ROM = (UINT16 *)Drv68KROM;

	// nop out dbz1's mask rom test
	// tile ROM test
	ROM[0x78c/2] = 0x4e71;    /* 0x0c43 - cmpi.w  #-$1236, D3 */
	ROM[0x78e/2] = 0x4e71;    /* 0x0010 */
	ROM[0x790/2] = 0x4e71;    /* 0x6600 - bne     $7a2 */
	ROM[0x792/2] = 0x4e71;    /* 0x0010 */
	// PSAC2 ROM test
	ROM[0x982/2] = 0x4e71;    /* 0x0c43 - cmpi.w  #$437e, D3 */
	ROM[0x984/2] = 0x4e71;    /* 0x437e */
	ROM[0x986/2] = 0x4e71;    /* 0x6600 - bne     $9a0 */
	ROM[0x988/2] = 0x4e71;    /* 0x0018 */
	ROM[0x98a/2] = 0x4e71;    /* 0x0c44 - cmpi.w  #$65e8, D4 */
	ROM[0x98c/2] = 0x4e71;    /* 0x65e8 */
	ROM[0x98e/2] = 0x4e71;    /* 0x6600 - bne     $9a0 */
	ROM[0x990/2] = 0x4e71;    /* 0x0010 */
}

static void dbz2_patch()
{
	UINT16 *ROM = (UINT16 *)Drv68KROM;

	// to avoid crash during loop at 0x000a4a after D4 > 0x80 (reading tiles region out of bounds)
	ROM[0xa48/2] = 0x007f;    /* 0x00ff */
	// nop out dbz1's mask rom test
	// tile ROM test
	ROM[0xa88/2] = 0x4e71;    /* 0x0c43 - cmpi.w  #$e58, D3 */
	ROM[0xa8a/2] = 0x4e71;    /* 0x0e58 */
	ROM[0xa8c/2] = 0x4e71;    /* 0x6600 - bne     $aae */
	ROM[0xa8e/2] = 0x4e71;    /* 0x0020 */
	ROM[0xa98/2] = 0x4e71;    /* 0x0c45 - cmpi.w  #-$3d20, D5 */
	ROM[0xa9a/2] = 0x4e71;    /* 0xc2e0 */
	ROM[0xa9c/2] = 0x4e71;    /* 0x6600 - bne     $aae */
	ROM[0xa9e/2] = 0x4e71;    /* 0x0010 */
	// PSAC2 ROM test (0 to 3)
	ROM[0xc66/2] = 0x4e71;    /* 0xb549 - cmpm.w  (A1)+, (A2)+ */
	ROM[0xc68/2] = 0x4e71;    /* 0x6600 - bne     $cc8 */
	ROM[0xc6a/2] = 0x4e71;    /* 0x005e */
	ROM[0xc7c/2] = 0x4e71;    /* 0xb549 - cmpm.w  (A1)+, (A2)+ */
	ROM[0xc7e/2] = 0x4e71;    /* 0x6600 - bne     $cc8 */
	ROM[0xc80/2] = 0x4e71;    /* 0x0048 */
	ROM[0xc9e/2] = 0x4e71;    /* 0xb549 - cmpm.w  (A1)+, (A2)+ */
	ROM[0xca0/2] = 0x4e71;    /* 0x6600 - bne     $cc8 */
	ROM[0xca2/2] = 0x4e71;    /* 0x0026 */
	ROM[0xcb4/2] = 0x4e71;    /* 0xb549 - cmpm.w  (A1)+, (A2)+ */
	ROM[0xcb6/2] = 0x4e71;    /* 0x6600 - bne     $cc8 */
	ROM[0xcb8/2] = 0x4e71;    /* 0x0010 */
	// prog ROM test
	ROM[0xae4/2] = 0x4e71;    /* 0xb650 - cmp.w   (A0), D3 */
	ROM[0xae6/2] = 0x4e71;    /* 0x6600 - bne     $af8 */
	ROM[0xae8/2] = 0x4e71;    /* 0x005e */
}

static INT32 DrvInit(INT32 nGame)
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

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  3, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  4, 4, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  5, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  6, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004,  7, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006,  8, 8, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  9, 1)) return 1;

		if (nGame != 2) // dbz, dbza
		{
			if (BurnLoadRom(DrvGfxROM3 + 0x000000, 10, 1)) return 1;

			if (BurnLoadRom(DrvSndROM  + 0x000000, 11, 1)) return 1;
		}
		else // dbz2
		{
			if (BurnLoadRom(DrvGfxROM2 + 0x200000, 10, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM3 + 0x000000, 11, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM3 + 0x200000, 12, 1)) return 1;

			if (BurnLoadRom(DrvSndROM  + 0x000000, 13, 1)) return 1;
		}

		DrvExpandGfx(DrvGfxROMExp0, DrvGfxROM0, 0x400000, 1);
		DrvExpandGfx(DrvGfxROMExp1, DrvGfxROM1, 0x800000, 1);
		DrvExpandGfx(DrvGfxROMExp2, DrvGfxROM2, 0x400000, 0);
		DrvExpandGfx(DrvGfxROMExp3, DrvGfxROM3, 0x400000, 0);

		if (nGame == 0) dbz_patch();
		if (nGame == 1) dbza_patch();
		if (nGame == 2) dbz2_patch();
	}

	K053936Init(0, DrvBg1RAM, 0x4000, 1024, 512, dbz_K053936_callback1);
	K053936Init(1, DrvBg2RAM, 0x4000, 1024, 512, dbz_K053936_callback2);
	K053936EnableWrap(0,1);
	K053936EnableWrap(1,1);
	K053936SetOffset(0, -46, -16);
	K053936SetOffset(1, -46, -16);

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x400000, dbz_tile_callback);
	K056832SetGlobalOffsets(0, 0);
	K056832SetLayerOffsets(0, (nGame == 2) ? -34 : -35, -16);
	K056832SetLayerOffsets(1, -31, -16);
	K056832SetLayerOffsets(2,   0,   0);
	K056832SetLayerOffsets(3, -31, -16);

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x7fffff, dbz_sprite_callback, 1);
	K053247SetSpriteOffset(-87, -32);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM0,	0x480000, 0x48ffff, MAP_RAM);
	SekMapMemory(K053247Ram,	0x4a0000, 0x4a0fff, MAP_RAM);
	SekMapMemory(Drv68KRAM1,	0x4a1000, 0x4a3fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x4a8000, 0x4abfff, MAP_RAM);
	SekMapMemory(DrvK053936Ctrl1,	0x4d0000, 0x4d03ff, MAP_RAM);
	SekMapMemory(DrvK053936Ctrl2,	0x4d4000, 0x4d43ff, MAP_RAM);
	SekMapMemory(DrvBg2RAM,		0x500000, 0x501fff, MAP_RAM);
	SekMapMemory(DrvBg1RAM,		0x508000, 0x509fff, MAP_RAM);
	SekMapMemory(Drvk053936RAM1,	0x510000, 0x513fff, MAP_RAM);
	SekMapMemory(Drvk053936RAM2,	0x518000, 0x51bfff, MAP_RAM);
	SekSetWriteWordHandler(0,	dbz_main_write_word);
	SekSetWriteByteHandler(0,	dbz_main_write_byte);
	SekSetReadWordHandler(0,	dbz_main_read_word);
	SekSetReadByteHandler(0,	dbz_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x8000, 0xbfff, MAP_RAM);
	ZetSetWriteHandler(dbz_sound_write);
	ZetSetReadHandler(dbz_sound_read);
	ZetClose();

	BurnYM2151Init(4000000);
	BurnYM2151SetIrqHandler(&dbzYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 1056000 / 132, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

static INT32 dbzInit()
{
	return DrvInit(0);
}

static INT32 dbzaInit()
{
	return DrvInit(1);
}

static INT32 dbz2Init()
{
	return DrvInit(2);
}

static INT32 DrvExit()
{
	GenericTilesExit();

	KonamiICExit();

	BurnYM2151Exit();
	MSM6295Exit(0);

	SekExit();
	ZetExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteRecalc()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x4000/2; i++)
	{
		INT32 r = (pal[i] >> 10 & 0x1f);
		INT32 g = (pal[i] >> 5) & 0x1f;
		INT32 b = (pal[i]) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2); 

		DrvPalette[i] = (r << 16) + (g << 8) + b;
	}
}

static INT32 DrvDraw()
{
	static const INT32 K053251_CI[6] = { 3, 4, 4, 4, 2, 1 };
	DrvPaletteRecalc();
	KonamiClearBitmaps(0);

	INT32 layer[5], plane;

	sprite_colorbase = K053251GetPaletteIndex(0);

	for (plane = 0; plane < 6; plane++)
	{
		layer_colorbase[plane] = K053251GetPaletteIndex(K053251_CI[plane]);
	}

	K053936PredrawTiles2(0, DrvGfxROMExp3);
	K053936PredrawTiles2(1, DrvGfxROMExp2);

	layer[0] = 0;
	layerpri[0] = K053251GetPriority(3);
	layer[1] = 1;
	layerpri[1] = K053251GetPriority(4);
	layer[2] = 3;
	layerpri[2] = K053251GetPriority(0);
	layer[3] = 4;
	layerpri[3] = K053251GetPriority(2);
	layer[4] = 5;
	layerpri[4] = K053251GetPriority(1);

	konami_sortlayers5(layer, layerpri);

	for (plane = 0; plane < 5; plane++)
	{
		INT32 flag, pri;

		if (plane == 0)
		{
			flag = 0;
			pri = 0;
		}
		else
		{
			flag = 1;
			pri = 1 << (plane - 1);
		}

		if (layer[plane] == 4) {
			if (nBurnLayer & 1) K053936Draw(0, (UINT16*)DrvK053936Ctrl2, (UINT16*)Drvk053936RAM2, flag | (pri<<8));
		} else if(layer[plane] == 5) {
			if (nBurnLayer & 2) K053936Draw(1, (UINT16*)DrvK053936Ctrl1, (UINT16*)Drvk053936RAM1, flag | (pri<<8));
		} else {
			if (nSpriteEnable & 2) K056832Draw(layer[plane], flag ? 0 : K056832_LAYER_OPAQUE, pri);
		}
	}

	if (nSpriteEnable & 1) K053247SpritesRender();

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

		DrvInputs[1] = (DrvInputs[1] & ~0xff08) | (DrvDips[0] & 0x08) | (DrvDips[1] << 8);
		DrvInputs[2] = (DrvDips[2] << 8) | (DrvDips[2] << 0);
	}

	SekNewFrame();

	INT32 nInterleave = nBurnSoundLen;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 16000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nNext, nCyclesSegment;

		nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[0];
		nCyclesSegment = SekRun(nCyclesSegment);
		nCyclesDone[0] += nCyclesSegment;

		if (i == 0 && K053246_is_IRQ_enabled()) {
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}

		if (i == (nInterleave - 20)) {
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO); //CK);
		}

		nNext = (i + 1) * nCyclesTotal[1] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[1];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[1] += nCyclesSegment;

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}

	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
		}
	}

	ZetClose();
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
		ZetScan(nAction);

		BurnYM2151Scan(nAction);
		MSM6295Scan(0,nAction);

		KonamiICScan(nAction);

		SCAN_VAR(control_data);
	}

	return 0;
}


// Dragonball Z (rev B)

static struct BurnRomInfo dbzRomDesc[] = {
	{ "222b11.9e",	0x080000, 0x4c6b75e9, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "222b12.9f",	0x080000, 0x48637fce, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "222a10.5e",	0x008000, 0x1c93e30a, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "222a01.27c",	0x200000, 0x9fce4ed4, 3 | BRF_GRA },           //  3 Background Tiles
	{ "222a02.27e",	0x200000, 0x651acaa5, 3 | BRF_GRA },           //  4

	{ "222a04.3j",	0x200000, 0x2533b95a, 4 | BRF_GRA },           //  5 Sprites
	{ "222a05.1j",	0x200000, 0x731b7f93, 4 | BRF_GRA },           //  6
	{ "222a06.3l",	0x200000, 0x97b767d3, 4 | BRF_GRA },           //  7
	{ "222a07.1l",	0x200000, 0x430bc873, 4 | BRF_GRA },           //  8

	{ "222a08.25k",	0x200000, 0x6410ee1b, 5 | BRF_GRA },           //  9 Zoom Layer Tiles #1

	{ "222a09.25l",	0x200000, 0xf7b3f070, 6 | BRF_GRA },           // 10 Zoom Layer Tiles #0

	{ "222a03.7c",	0x040000, 0x1924467b, 7 | BRF_SND },           // 11 Samples
};

STD_ROM_PICK(dbz)
STD_ROM_FN(dbz)

struct BurnDriver BurnDrvDbz = {
	"dbz", NULL, NULL, NULL, "1993",
	"Dragonball Z (rev B)\0", NULL, "Banpresto", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_VSFIGHT, 0,
	NULL, dbzRomInfo, dbzRomName, NULL, NULL, DbzInputInfo, DbzDIPInfo,
	dbzInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 256, 4, 3
};

// Dragonball Z (rev A)

static struct BurnRomInfo dbzaRomDesc[] = {
	{ "222a11.9e",	0x080000, 0x60c7d9b2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "222a12.9f",	0x080000, 0x6ebc6853, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "222a10.5e",	0x008000, 0x1c93e30a, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "222a01.27c",	0x200000, 0x9fce4ed4, 3 | BRF_GRA },           //  3 Background Tiles
	{ "222a02.27e",	0x200000, 0x651acaa5, 3 | BRF_GRA },           //  4

	{ "222a04.3j",	0x200000, 0x2533b95a, 4 | BRF_GRA },           //  5 Sprites
	{ "222a05.1j",	0x200000, 0x731b7f93, 4 | BRF_GRA },           //  6
	{ "222a06.3l",	0x200000, 0x97b767d3, 4 | BRF_GRA },           //  7
	{ "222a07.1l",	0x200000, 0x430bc873, 4 | BRF_GRA },           //  8

	{ "222a08.25k",	0x200000, 0x6410ee1b, 5 | BRF_GRA },           //  9 Zoom Layer Tiles #1

	{ "222a09.25l",	0x200000, 0xf7b3f070, 6 | BRF_GRA },           // 10 Zoom Layer Tiles #0

	{ "222a03.7c",	0x040000, 0x1924467b, 7 | BRF_SND },           // 11 Samples
};

STD_ROM_PICK(dbza)
STD_ROM_FN(dbza)

struct BurnDriver BurnDrvDbza = {
	"dbza", "dbz", NULL, NULL, "1993",
	"Dragonball Z (rev A)\0", NULL, "Banpresto", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_VSFIGHT, 0,
	NULL, dbzaRomInfo, dbzaRomName, NULL, NULL, DbzInputInfo, DbzDIPInfo,
	dbzaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 256, 4, 3
};


// Dragonball Z 2 - Super Battle

static struct BurnRomInfo dbz2RomDesc[] = {
	{ "a9e.9e",	0x080000, 0xe6a142c9, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "a9f.9f",	0x080000, 0x76cac399, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "s-001.5e",	0x008000, 0x154e6d03, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ds-b01.27c",	0x200000, 0x8dc39972, 3 | BRF_GRA },           //  3 Background Tiles
	{ "ds-b02.27e",	0x200000, 0x7552f8cd, 3 | BRF_GRA },           //  4

	{ "ds-o01.3j",	0x200000, 0xd018531f, 4 | BRF_GRA },           //  5 Sprites
	{ "ds-o02.1j",	0x200000, 0x5a0f1ebe, 4 | BRF_GRA },           //  6
	{ "ds-o03.3l",	0x200000, 0xddc3bef1, 4 | BRF_GRA },           //  7
	{ "ds-o04.1l",	0x200000, 0xb5df6676, 4 | BRF_GRA },           //  8

	{ "ds-p01.25k",	0x200000, 0x1c7aad68, 5 | BRF_GRA },           //  9 Zoom Layer Tiles #1
	{ "ds-p02.27k",	0x200000, 0xe4c3a43b, 5 | BRF_GRA },           // 10

	{ "ds-p03.25l",	0x200000, 0x1eaa671b, 6 | BRF_GRA },           // 11 Zoom Layer Tiles #0
	{ "ds-p04.27l",	0x200000, 0x5845ff98, 6 | BRF_GRA },           // 12

	{ "pcm.7c",	0x040000, 0xb58c884a, 7 | BRF_SND },           // 13 Samples
};

STD_ROM_PICK(dbz2)
STD_ROM_FN(dbz2)

struct BurnDriver BurnDrvDbz2 = {
	"dbz2", NULL, NULL, NULL, "1994",
	"Dragonball Z 2 - Super Battle\0", NULL, "Banpresto", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_VSFIGHT, 0,
	NULL, dbz2RomInfo, dbz2RomName, NULL, NULL, DbzInputInfo, Dbz2DIPInfo,
	dbz2Init,DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 256, 4, 3
};
