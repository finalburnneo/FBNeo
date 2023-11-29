// FB Alpha Ajax driver module
// Based on MAME driver by Manuel Abadia

#include "tiles_generic.h"
#include "z80_intf.h"
#include "konami_intf.h"
#include "konamiic.h"
#include "burn_ym2151.h"
#include "k007232.h"
#include "m6809_intf.h"

static UINT8* AllMem;
static UINT8* MemEnd;
static UINT8* AllRam;
static UINT8* RamEnd;
static UINT8* DrvKonROM;
static UINT8* DrvM6809ROM;
static UINT8* DrvZ80ROM;
static UINT8* DrvGfxROM0;
static UINT8* DrvGfxROM1;
static UINT8* DrvGfxROM2;
static UINT8* DrvGfxROMExp0;
static UINT8* DrvGfxROMExp1;
static UINT8* DrvSndROM0;
static UINT8* DrvSndROM1;
static UINT8* DrvShareRAM;
static UINT8* DrvKonRAM;
static UINT8* DrvPalRAM;
static UINT8* DrvZ80RAM;
static UINT32* DrvPalette;
static UINT8 DrvRecalc;

static UINT8* soundlatch;
static UINT8* nDrvBankRom;

static UINT8 DrvInputs[3];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvReset;

static INT32 firq_enable;
static INT32 ajax_priority;

static struct BurnInputInfo AjaxInputList[] = {
	{"P1 Coin", BIT_DIGITAL, DrvJoy1 + 0, "p1 coin"},
	{"P1 Start", BIT_DIGITAL, DrvJoy1 + 3, "p1 start"},
	{"P1 Up", BIT_DIGITAL, DrvJoy2 + 2, "p1 up"},
	{"P1 Down", BIT_DIGITAL, DrvJoy2 + 3, "p1 down"},
	{"P1 Left", BIT_DIGITAL, DrvJoy2 + 0, "p1 left"},
	{"P1 Right", BIT_DIGITAL, DrvJoy2 + 1, "p1 right"},
	{"P1 Button 1", BIT_DIGITAL, DrvJoy2 + 4, "p1 fire 1"},
	{"P1 Button 2", BIT_DIGITAL, DrvJoy2 + 5, "p1 fire 2"},
	{"P1 Button 3", BIT_DIGITAL, DrvJoy2 + 6, "p1 fire 3"},

	{"P2 Coin", BIT_DIGITAL, DrvJoy1 + 1, "p2 coin"},
	{"P2 Start", BIT_DIGITAL, DrvJoy1 + 4, "p2 start"},
	{"P2 Up", BIT_DIGITAL, DrvJoy3 + 2, "p2 up"},
	{"P2 Down", BIT_DIGITAL, DrvJoy3 + 3, "p2 down"},
	{"P2 Left", BIT_DIGITAL, DrvJoy3 + 0, "p2 left"},
	{"P2 Right", BIT_DIGITAL, DrvJoy3 + 1, "p2 right"},
	{"P2 Button 1", BIT_DIGITAL, DrvJoy3 + 4, "p2 fire 1"},
	{"P2 Button 2", BIT_DIGITAL, DrvJoy3 + 5, "p2 fire 2"},
	{"P2 Button 3", BIT_DIGITAL, DrvJoy3 + 6, "p2 fire 3"},

	{"Reset", BIT_DIGITAL, &DrvReset, "reset"},
	{"Service", BIT_DIGITAL, DrvJoy1 + 2, "service"},
	{"Dip A", BIT_DIPSWITCH, DrvDips + 0, "dip"},
	{"Dip B", BIT_DIPSWITCH, DrvDips + 1, "dip"},
	{"Dip C", BIT_DIPSWITCH, DrvDips + 2, "dip"},
};

STDINPUTINFO(Ajax)

static struct BurnDIPInfo AjaxDIPList[] =
{
	{0x14, 0xff, 0xff, 0xff, nullptr},
	{0x15, 0xff, 0xff, 0x52, nullptr},
	{0x16, 0xff, 0xff, 0xff, nullptr},

	{0, 0xfe, 0, 16, "Coin A"},
	{0x14, 0x01, 0x0f, 0x02, "4 Coins 1 Credit"},
	{0x14, 0x01, 0x0f, 0x05, "3 Coins 1 Credit"},
	{0x14, 0x01, 0x0f, 0x08, "2 Coins 1 Credit"},
	{0x14, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"},
	{0x14, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"},
	{0x14, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"},
	{0x14, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"},
	{0x14, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"},
	{0x14, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"},
	{0x14, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"},
	{0x14, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"},
	{0x14, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"},
	{0x14, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"},
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"},
	{0x14, 0x01, 0x0f, 0x00, "Free Play"},

	{0, 0xfe, 0, 15, "Coin B"},
	{0x14, 0x01, 0xf0, 0x20, "4 Coins 1 Credit"},
	{0x14, 0x01, 0xf0, 0x50, "3 Coins 1 Credit"},
	{0x14, 0x01, 0xf0, 0x80, "2 Coins 1 Credit"},
	{0x14, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"},
	{0x14, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"},
	{0x14, 0x01, 0xf0, 0xf0, "1 Coin  1 Credit"},
	{0x14, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"},
	{0x14, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"},
	{0x14, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"},
	{0x14, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"},
	{0x14, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"},
	{0x14, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"},
	{0x14, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"},
	{0x14, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"},
	{0x14, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"},

	{0, 0xfe, 0, 4, "Lives"},
	{0x15, 0x01, 0x03, 0x03, "2"},
	{0x15, 0x01, 0x03, 0x02, "3"},
	{0x15, 0x01, 0x03, 0x01, "5"},
	{0x15, 0x01, 0x03, 0x00, "7"},

	//	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	//	{0x15, 0x01, 0x04, 0x00, "Upright"		},
	//	{0x15, 0x01, 0x04, 0x04, "Cocktail"		},

	{0, 0xfe, 0, 4, "Bonus Life"},
	{0x15, 0x01, 0x18, 0x18, "30000 150000"},
	{0x15, 0x01, 0x18, 0x10, "50000 200000"},
	{0x15, 0x01, 0x18, 0x08, "30000"},
	{0x15, 0x01, 0x18, 0x00, "50000"},

	{0, 0xfe, 0, 4, "Difficulty"},
	{0x15, 0x01, 0x60, 0x60, "Easy"},
	{0x15, 0x01, 0x60, 0x40, "Normal"},
	{0x15, 0x01, 0x60, 0x20, "Difficult"},
	{0x15, 0x01, 0x60, 0x00, "Very Difficult"},

	{0, 0xfe, 0, 2, "Demo Sounds"},
	{0x15, 0x01, 0x80, 0x80, "Off"},
	{0x15, 0x01, 0x80, 0x00, "On"},

	//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	//	{0x16, 0x01, 0x01, 0x01, "Off"			},
	//	{0x16, 0x01, 0x01, 0x00, "On"			},

	{0, 0xfe, 0, 2, "Upright Controls"},
	{0x16, 0x01, 0x02, 0x02, "Single"},
	{0x16, 0x01, 0x02, 0x00, "Dual"},

	{0, 0xfe, 0, 2, "Service Mode"},
	{0x16, 0x01, 0x04, 0x04, "Off"},
	{0x16, 0x01, 0x04, 0x00, "On"},

	{0, 0xfe, 0, 2, "Control in 3D Stages"},
	{0x16, 0x01, 0x08, 0x08, "Normal"},
	{0x16, 0x01, 0x08, 0x00, "Inverted"},
};

STDDIPINFO(Ajax)

static void ajax_main_bankswitch(INT32 data)
{
	nDrvBankRom[0] = data;

	INT32 nBank = 0x10000 + ((data & 0x80) << 9) + ((data & 7) << 13);

	ajax_priority = data & 0x08;

	konamiMapMemory(DrvKonROM + nBank, 0x6000, 0x7fff, MAP_ROM);
}

static void ajax_main_write(UINT16 address, UINT8 data)
{
	if (address <= 0x1c0)
	{
		switch ((address & 0x01c0) >> 6)
		{
		case 0x0000:
			if (address == 0 && firq_enable)
			{
				M6809SetIRQLine(1, CPU_IRQSTATUS_AUTO);
			}
			break;

		case 0x0001:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			break;

		case 0x0002:
			*soundlatch = data;
			break;

		case 0x0003:
			ajax_main_bankswitch(data);
			break;
		}
	}

	if ((address & 0xfff8) == 0x0800)
	{
		K051937Write(address & 7, data);
		return;
	}

	if ((address & 0xfc00) == 0x0c00)
	{
		K051960Write(address & 0x3ff, data);
	}
}

static UINT8 ajax_main_read(UINT16 address)
{
	if (address <= 0x01c0)
	{
		switch ((address & 0x1c0) >> 6)
		{
		case 0x0000:
			return konamiTotalCycles() & 0xff; // rand

		case 0x0004:
			return DrvInputs[2];

		case 0x0006:
			switch (address & 3)
			{
			case 0:
				return DrvInputs[0];

			case 1:
				return DrvInputs[1];

			case 2:
				return DrvDips[0];

			case 3:
				return DrvDips[1];
			}
			return 0;

		case 0x0007:
			return DrvDips[2];
		}
	}

	if ((address & 0xfff8) == 0x0800)
	{
		return K051937Read(address & 7);
	}

	if ((address & 0xfc00) == 0x0c00)
	{
		return K051960Read(address & 0x3ff);
	}

	return 0;
}

static void ajax_sub_bankswitch(UINT8 data)
{
	nDrvBankRom[1] = data;

	K052109RMRDLine = data & 0x40;

	K051316WrapEnable(0, data & 0x20);

	firq_enable = data & 0x10;

	INT32 nBank = ((data & 0x0f) << 13) + 0x10000;
	M6809MapMemory(DrvM6809ROM + nBank, 0x8000, 0x9fff, MAP_ROM);
}

static void ajax_sub_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0x0000)
	{
		K051316Write(0, address & 0x7ff, data);
		return;
	}

	if ((address & 0xfff0) == 0x0800)
	{
		K051316WriteCtrl(0, address & 0x0f, data);
		return;
	}

	if (address == 0x1800)
	{
		ajax_sub_bankswitch(data);
		return;
	}

	if ((address & 0xc000) == 0x4000)
	{
		K052109Write(address & 0x3fff, data);
	}
}

static UINT8 ajax_sub_read(UINT16 address)
{
	if ((address & 0xf800) == 0x0000)
	{
		return K051316Read(0, address & 0x7ff);
	}

	if ((address & 0xf800) == 0x1000)
	{
		return K051316ReadRom(0, address & 0x7ff);
	}

	if ((address & 0xc000) == 0x4000)
	{
		return K052109Read(address & 0x3fff);
	}

	return 0;
}

static void __fastcall ajax_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0xa000)
	{
		K007232WriteReg(0, address & 0x0f, data);
		return;
	}

	if ((address & 0xfff0) == 0xb000)
	{
		K007232WriteReg(1, address & 0x0f, data);
		return;
	}

	switch (address)
	{
	case 0x9000:
		k007232_set_bank(0, (data >> 1) & 1, (data >> 0) & 1);
		k007232_set_bank(1, (data >> 4) & 3, (data >> 2) & 3);
		return;

	case 0xb80c:
		K007232SetVolume(1, 0, (data & 0x0f) * 0x11 / 2, (data & 0x0f) * 0x11 / 2);
		return;

	case 0xc000:
		BurnYM2151SelectRegister(data);
		return;

	case 0xc001:
		BurnYM2151WriteRegister(data);
	}
}

static UINT8 __fastcall ajax_sound_read(UINT16 address)
{
	if ((address & 0xfff0) == 0xa000)
	{
		return K007232ReadReg(0, address & 0x0f);
	}

	if ((address & 0xfff0) == 0xb000)
	{
		return K007232ReadReg(1, address & 0x0f);
	}

	switch (address)
	{
	case 0xc000:
	case 0xc001:
		return BurnYM2151Read();

	case 0xe000:
		ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return *soundlatch;
	}

	return 0;
}

static void K052109Callback(INT32 layer, INT32 bank, INT32* code, INT32* color, INT32*, INT32*)
{
	INT32 layer_colorbase[3] = {64, 0, 32};

	*code |= ((*color & 0x0f) << 8) | (bank << 12);
	*code &= 0x3fff;
	*color = layer_colorbase[layer] + ((*color & 0xf0) >> 4);
}

static void K051960Callback(INT32* code, INT32* color, INT32* priority, INT32*)
{
	*priority = 0xff00;
	if (*color & 0x10) *priority |= 0xf0f0;
	if (~*color & 0x40) *priority |= 0xcccc;
	if (*color & 0x20) *priority |= 0xaaaa;
	*color = 16 + (*color & 0x0f);
	*code &= 0x1fff;
}

static void K051316Callback(INT32* code, INT32* color, INT32*)
{
	*code |= ((*color & 0x07) << 8);
	*code &= 0x7ff;
	*color = 6 + ((*color & 0x08) >> 3);
}

static void DrvK007232VolCallback0(INT32 v)
{
	K007232SetVolume(0, 0, (v >> 0x4) * 0x11, 0);
	K007232SetVolume(0, 1, 0, (v & 0x0f) * 0x11);
}

static void DrvK007232VolCallback1(INT32 v)
{
	K007232SetVolume(1, 1, (v & 0x0f) * 0x11 / 2, (v & 0x0f) * 0x11 / 2);
}

static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset(AllRam, 0, RamEnd - AllRam);

	konamiOpen(0);
	konamiReset();
	konamiClose();

	M6809Open(0);
	M6809Reset();
	M6809Close();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	K007232Reset(0);
	K007232Reset(1);
	BurnYM2151Reset();

	KonamiICReset();

	firq_enable = 0;
	ajax_priority = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8* Next;
	Next = AllMem;

	DrvKonROM = Next;
	Next += 0x030000;
	DrvM6809ROM = Next;
	Next += 0x030000;
	DrvZ80ROM = Next;
	Next += 0x010000;

	DrvGfxROM0 = Next;
	Next += 0x080000;
	DrvGfxROM1 = Next;
	Next += 0x100000;
	DrvGfxROM2 = Next;
	Next += 0x080000;
	DrvGfxROMExp0 = Next;
	Next += 0x100000;
	DrvGfxROMExp1 = Next;
	Next += 0x200000;

	DrvSndROM0 = Next;
	Next += 0x040000;
	DrvSndROM1 = Next;
	Next += 0x080000;

	DrvPalette = (UINT32*)Next;
	Next += 0x801 * sizeof(UINT32);

	AllRam = Next;

	DrvShareRAM = Next;
	Next += 0x002000;
	DrvKonRAM = Next;
	Next += 0x002000;
	DrvPalRAM = Next;
	Next += 0x001000 + 0x1000;

	DrvZ80RAM = Next;
	Next += 0x000800;

	soundlatch = Next;
	Next += 0x000001;
	nDrvBankRom = Next;
	Next += 0x000002;

	RamEnd = Next;
	MemEnd = Next;

	return 0;
}

static INT32 DrvInit(INT32 type)
{
	GenericTilesInit();

	AllMem = nullptr;
	MemIndex();
	INT32 nLen = MemEnd - static_cast<UINT8*>(nullptr);
	if ((AllMem = BurnMalloc(nLen)) == nullptr) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvKonROM + 0x020000, 0, 1)) return 1;
		if (BurnLoadRom(DrvKonROM + 0x010000, 1, 1)) return 1;
		memcpy(DrvKonROM + 0x08000, DrvKonROM + 0x28000, 0x8000);

		if (BurnLoadRom(DrvM6809ROM + 0x20000, 2, 1)) return 1;
		memcpy(DrvM6809ROM + 0xa000, DrvM6809ROM + 0x22000, 0x6000);
		if (BurnLoadRom(DrvM6809ROM + 0x10000, 3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x000000, 4, 1)) return 1;

		if (type)
		{
			if (BurnLoadRom(DrvGfxROM0 + 0x000000, 5, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x000001, 6, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x000002, 7, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x000003, 8, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x040000, 9, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x040001, 10, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x040002, 11, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x040003, 12, 4)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x000000, 13, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x000001, 14, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x000002, 15, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x000003, 16, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x040000, 17, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x040001, 18, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x040002, 19, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x040003, 20, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x080000, 21, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x080001, 22, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x080002, 23, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x080003, 24, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x0c0000, 25, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x0c0001, 26, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x0c0002, 27, 4)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x0c0003, 28, 4)) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x000000, 29, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x040000, 30, 1)) return 1;

			if (BurnLoadRom(DrvSndROM0 + 0x000000, 31, 1)) return 1;
			if (BurnLoadRom(DrvSndROM0 + 0x010000, 32, 1)) return 1;
			if (BurnLoadRom(DrvSndROM0 + 0x020000, 33, 1)) return 1;
			if (BurnLoadRom(DrvSndROM0 + 0x030000, 34, 1)) return 1;

			if (BurnLoadRom(DrvSndROM1 + 0x000000, 35, 1)) return 1;
			if (BurnLoadRom(DrvSndROM1 + 0x010000, 36, 1)) return 1;
			if (BurnLoadRom(DrvSndROM1 + 0x020000, 37, 1)) return 1;
			if (BurnLoadRom(DrvSndROM1 + 0x030000, 38, 1)) return 1;
			if (BurnLoadRom(DrvSndROM1 + 0x040000, 39, 1)) return 1;
			if (BurnLoadRom(DrvSndROM1 + 0x050000, 40, 1)) return 1;
			if (BurnLoadRom(DrvSndROM1 + 0x060000, 41, 1)) return 1;
			if (BurnLoadRom(DrvSndROM1 + 0x070000, 42, 1)) return 1;
		}
		else
		{
			if (BurnLoadRomExt(DrvGfxROM0 + 0x000000, 5, 4, LD_GROUP(2))) return 1;
			if (BurnLoadRomExt(DrvGfxROM0 + 0x000002, 6, 4, LD_GROUP(2))) return 1;

			if (BurnLoadRomExt(DrvGfxROM1 + 0x000000, 7, 4, LD_GROUP(2))) return 1;
			if (BurnLoadRomExt(DrvGfxROM1 + 0x000002, 8, 4, LD_GROUP(2))) return 1;

			if (BurnLoadRom(DrvGfxROM2 + 0x000000, 9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + 0x040000, 10, 1)) return 1;

			if (BurnLoadRom(DrvSndROM0 + 0x000000, 11, 1)) return 1;
			if (BurnLoadRom(DrvSndROM1 + 0x000000, 12, 1)) return 1;
		}

		K052109GfxDecode(DrvGfxROM0, DrvGfxROMExp0, 0x080000);
		K051960GfxDecode(DrvGfxROM1, DrvGfxROMExp1, 0x100000);
	}

	konamiInit(0);
	konamiOpen(0);
	konamiMapMemory(DrvPalRAM, 0x1000, 0x1fff, MAP_RAM);
	konamiMapMemory(DrvShareRAM, 0x2000, 0x3fff, MAP_RAM);
	konamiMapMemory(DrvKonRAM, 0x4000, 0x5fff, MAP_RAM);
	konamiMapMemory(DrvKonROM + 0x10000, 0x6000, 0x7fff, MAP_ROM);
	konamiMapMemory(DrvKonROM + 0x08000, 0x8000, 0xffff, MAP_ROM);
	konamiSetWriteHandler(ajax_main_write);
	konamiSetReadHandler(ajax_main_read);
	konamiClose();

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvShareRAM, 0x2000, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0x10000, 0x8000, 0x9fff, MAP_ROM);
	M6809MapMemory(DrvM6809ROM + 0x0a000, 0xa000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(ajax_sub_write);
	M6809SetReadHandler(ajax_sub_read);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80ROM);
	ZetMapArea(0x8000, 0x87ff, 0, DrvZ80RAM);
	ZetMapArea(0x8000, 0x87ff, 1, DrvZ80RAM);
	ZetMapArea(0x8000, 0x87ff, 2, DrvZ80RAM);
	ZetSetWriteHandler(ajax_sound_write);
	ZetSetReadHandler(ajax_sound_read);
	ZetClose();

	BurnYM2151InitBuffered(3579545, 1, nullptr, 0);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnTimerAttachZet(3579545);

	K007232Init(0, 3579545, DrvSndROM0, 0x40000);
	K007232SetPortWriteHandler(0, DrvK007232VolCallback0);
	K007232PCMSetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);

	K007232Init(1, 3579545, DrvSndROM1, 0x80000);
	K007232SetPortWriteHandler(1, DrvK007232VolCallback1);
	K007232SetRoute(1, BURN_SND_K007232_ROUTE_1, 0.40, BURN_SND_ROUTE_BOTH);
	K007232SetRoute(1, BURN_SND_K007232_ROUTE_2, 0.30, BURN_SND_ROUTE_BOTH);

	K052109Init(DrvGfxROM0, DrvGfxROMExp0, 0x7ffff);
	K052109SetCallback(K052109Callback);
	K052109AdjustScroll(4, 0);

	K051960Init(DrvGfxROM1, DrvGfxROMExp1, 0xfffff);
	K051960SetCallback(K051960Callback);
	K051960SetSpriteOffset(4, 0);

	K051316Init(0, DrvGfxROM2, DrvGfxROM2, 0x7ffff, K051316Callback, 7, 0);
	K051316SetOffset(0, -112 + 4, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	KonamiICExit();

	M6809Exit();
	konamiExit();
	ZetExit();

	K007232Exit();
	BurnYM2151Exit();

	BurnFree(AllMem);

	return 0;
}

static INT32 DrvDraw()
{
	KonamiRecalcPalette(DrvPalRAM, DrvPalette, 0x1000);

	K052109UpdateScroll();

	KonamiClearBitmaps(0);

	if (nBurnLayer & 1) K052109RenderLayer(2, 0, 1);

	if (ajax_priority)
	{
		if (nBurnLayer & 2) K051316_zoom_draw(0, 4);
		if (nBurnLayer & 4) K052109RenderLayer(1, 0, 2);
	}
	else
	{
		if (nBurnLayer & 4) K052109RenderLayer(1, 0, 2);
		if (nBurnLayer & 2) K051316_zoom_draw(0, 4);
	}

	if (nBurnLayer & 8) K052109RenderLayer(0, 0, 8);

	if (nSpriteEnable & 8) K051960SpritesRender(-1, -1);

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset)
	{
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset(DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		// Clear opposites
		if ((DrvInputs[1] & 0x03) == 0) DrvInputs[1] |= 0x03;
		if ((DrvInputs[1] & 0x0c) == 0) DrvInputs[1] |= 0x0c;
		if ((DrvInputs[2] & 0x03) == 0) DrvInputs[2] |= 0x03;
		if ((DrvInputs[2] & 0x0c) == 0) DrvInputs[2] |= 0x0c;
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[3] = {(((3000000 / 60) * 133) / 100) /* 33% overclock */, 3000000 / 60, 3579545 / 60};
	INT32 nCyclesDone[3] = {0, 0, 0};

	ZetOpen(0);
	M6809Open(0);
	konamiOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, konami);
		CPU_RUN(1, M6809);
		CPU_RUN_TIMER(2); // z80
	}

	if (K051960_irq_enabled) konamiSetIrqLine(KONAMI_IRQ_LINE, CPU_IRQSTATUS_AUTO);

	if (pBurnSoundOut)
	{
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		K007232Update(0, pBurnSoundOut, nBurnSoundLen);
		K007232Update(1, pBurnSoundOut, nBurnSoundLen);
	}

	konamiClose();
	M6809Close();
	ZetClose();

	if (pBurnDraw)
	{
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32* pnMin)
{
	struct BurnArea ba;

	if (pnMin)
	{
		*pnMin = 0x029705;
	}

	if (nAction & ACB_VOLATILE)
	{
		memset(&ba, 0, sizeof(ba));
		ba.Data = AllRam;
		ba.nLen = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		konamiCpuScan(nAction);
		M6809Scan(nAction);
		ZetScan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		K007232Scan(nAction, pnMin);

		KonamiICScan(nAction);

		SCAN_VAR(firq_enable);
		SCAN_VAR(ajax_priority);
	}

	if (nAction & ACB_WRITE)
	{
		konamiOpen(0);
		ajax_main_bankswitch(nDrvBankRom[0]);
		konamiClose();

		M6809Open(0);
		ajax_sub_bankswitch(nDrvBankRom[1]);
		M6809Close();
	}

	return 0;
}


// Ajax

static struct BurnRomInfo ajaxRomDesc[] = {
	{"770_m01.n11", 0x10000, 0x4a64e53a, 1 | BRF_PRG | BRF_ESS}, //  0 Konami Custom Code
	{"770_l02.n12", 0x10000, 0xad7d592b, 1 | BRF_PRG | BRF_ESS}, //  1

	{"770_l05.i16", 0x08000, 0xed64fbb2, 2 | BRF_PRG | BRF_ESS}, //  2 M6809 Code
	{"770_f04.g16", 0x10000, 0xe0e4ec9c, 2 | BRF_PRG | BRF_ESS}, //  3

	{"770_h03.f16", 0x08000, 0x2ffd2afc, 3 | BRF_PRG | BRF_ESS}, //  4 Z80 Code

	{"770c13-a.f3", 0x10000, 0x4ef6fff2, 4 | BRF_GRA}, //  5 K052109 Tiles
	{"770c13-c.f4", 0x10000, 0x97ffbab6, 4 | BRF_GRA}, //  6
	{"770c12-a.f5", 0x10000, 0x6c0ade68, 4 | BRF_GRA}, //  7
	{"770c12-c.f6", 0x10000, 0x61fc39cc, 4 | BRF_GRA}, //  8
	{"770c13-b.e3", 0x10000, 0x86fdd706, 4 | BRF_GRA}, //  9
	{"770c13-d.e4", 0x10000, 0x7d7acb2d, 4 | BRF_GRA}, // 10
	{"770c12-b.e5", 0x10000, 0x5f221cc6, 4 | BRF_GRA}, // 11
	{"770c12-d.e6", 0x10000, 0xf1edb2f4, 4 | BRF_GRA}, // 12

	{"770c09-a.f8", 0x10000, 0x76690fb8, 5 | BRF_GRA}, // 13 K051960 Tiles
	{"770c09-e.f9", 0x10000, 0x17b482c9, 5 | BRF_GRA}, // 14
	{"770c08-a.f10", 0x10000, 0xefd29a56, 5 | BRF_GRA}, // 15
	{"770c08-e.f11", 0x10000, 0x6d43afde, 5 | BRF_GRA}, // 16
	{"770c09-b.e8", 0x10000, 0xcd1709d1, 5 | BRF_GRA}, // 17
	{"770c09-f.e9", 0x10000, 0xcba4b47e, 5 | BRF_GRA}, // 18
	{"770c08-b.e10", 0x10000, 0xf3374014, 5 | BRF_GRA}, // 19
	{"770c08-f.e11", 0x10000, 0xf5ba59aa, 5 | BRF_GRA}, // 20
	{"770c09-c.d8", 0x10000, 0xbfd080b8, 5 | BRF_GRA}, // 21
	{"770c09-g.d9", 0x10000, 0x77d58ea0, 5 | BRF_GRA}, // 22
	{"770c08-c.d10", 0x10000, 0x28e7088f, 5 | BRF_GRA}, // 23
	{"770c08-g.d11", 0x10000, 0x17da8f6d, 5 | BRF_GRA}, // 24
	{"770c09-d.c8", 0x10000, 0x6f955600, 5 | BRF_GRA}, // 25
	{"770c09-h.c9", 0x10000, 0x494a9090, 5 | BRF_GRA}, // 26
	{"770c08-d.c10", 0x10000, 0x91591777, 5 | BRF_GRA}, // 27
	{"770c08-h.c11", 0x10000, 0xd97d4b15, 5 | BRF_GRA}, // 28

	{"770c06.f4", 0x40000, 0xd0c592ee, 6 | BRF_GRA}, // 29 K051960 Tiles
	{"770c07.h4", 0x40000, 0x0b399fb1, 6 | BRF_GRA}, // 30

	{"770c10-a.a7", 0x10000, 0xe45ec094, 7 | BRF_SND}, // 31 K007232 #0 Samples
	{"770c10-b.a6", 0x10000, 0x349db7d3, 7 | BRF_SND}, // 32
	{"770c10-c.a5", 0x10000, 0x71cb1f05, 7 | BRF_SND}, // 33
	{"770c10-d.a4", 0x10000, 0xe8ab1844, 7 | BRF_SND}, // 34

	{"770c11-a.c6", 0x10000, 0x8cccd9e0, 8 | BRF_SND}, // 35 K007232 #1 Samples
	{"770c11-b.c5", 0x10000, 0x0af2fedd, 8 | BRF_SND}, // 36
	{"770c11-c.c4", 0x10000, 0x7471f24a, 8 | BRF_SND}, // 37
	{"770c11-d.c3", 0x10000, 0xa58be323, 8 | BRF_SND}, // 38
	{"770c11-e.b7", 0x10000, 0xdd553541, 8 | BRF_SND}, // 39
	{"770c11-f.b6", 0x10000, 0x3f78bd0f, 8 | BRF_SND}, // 40
	{"770c11-g.b5", 0x10000, 0x078c51b2, 8 | BRF_SND}, // 41
	{"770c11-h.b4", 0x10000, 0x7300c2e1, 8 | BRF_SND}, // 42

	{"63s241.j11", 0x00200, 0x9bdd719f, 9 | BRF_OPT}, // 43 Timing Prom (unused)
};

STD_ROM_PICK(ajax)
STD_ROM_FN(ajax)

static INT32 ajaxInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvAjax = {
	"ajax", nullptr, nullptr, nullptr, "1987",
	"Ajax\0", nullptr, "Konami", "GX770",
	L"Ajax\0\u30A8\u30FC\u30B8\u30E3\u30C3\u30AF\u30B9\0", nullptr, nullptr, nullptr,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2,
	HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	nullptr, ajaxRomInfo, ajaxRomName, nullptr, nullptr, nullptr, nullptr, AjaxInputInfo, AjaxDIPInfo,
	ajaxInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 304, 3, 4
};


// Typhoon

static struct BurnRomInfo typhoonRomDesc[] = {
	{"770_k01.n11", 0x10000, 0x5ba74a22, 1 | BRF_PRG | BRF_ESS}, //  0 Konami Custom Code
	{"770_k02.n12", 0x10000, 0x3bcf782a, 1 | BRF_PRG | BRF_ESS}, //  1

	{"770_k05.i16", 0x08000, 0x0f1bebbb, 2 | BRF_PRG | BRF_ESS}, //  2 M6809 Code
	{"770_f04.g16", 0x10000, 0xe0e4ec9c, 2 | BRF_PRG | BRF_ESS}, //  3

	{"770_h03.f16", 0x08000, 0x2ffd2afc, 3 | BRF_PRG | BRF_ESS}, //  4 Z80 Code

	{"770c13.n22", 0x40000, 0xb859ca4e, 4 | BRF_GRA}, //  5 K052109 Tiles
	{"770c12.k22", 0x40000, 0x50d14b72, 4 | BRF_GRA}, //  6

	{"770c09.n4", 0x80000, 0x1ab4a7ff, 5 | BRF_GRA}, //  7 K051960 Tiles
	{"770c08.k4", 0x80000, 0xa8e80586, 5 | BRF_GRA}, //  8

	{"770c06.f4", 0x40000, 0xd0c592ee, 6 | BRF_GRA}, //  9 K051960 Tiles
	{"770c07.h4", 0x40000, 0x0b399fb1, 6 | BRF_GRA}, // 10

	{"770c10", 0x40000, 0x7fac825f, 7 | BRF_SND}, // 11 K007232 #0 Samples

	{"770c11", 0x80000, 0x299a615a, 8 | BRF_SND}, // 12 K007232 #1 Samples

	{"63s241.j11", 0x00200, 0x9bdd719f, 9 | BRF_OPT}, // 13 Timing Prom (unused)
};

STD_ROM_PICK(typhoon)
STD_ROM_FN(typhoon)

static INT32 typhoonInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvTyphoon = {
	"typhoon", "ajax", nullptr, nullptr, "1987",
	"Typhoon\0", nullptr, "Konami", "GX770",
	nullptr, nullptr, nullptr, nullptr,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2,
	HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	nullptr, typhoonRomInfo, typhoonRomName, nullptr, nullptr, nullptr, nullptr, AjaxInputInfo, AjaxDIPInfo,
	typhoonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 304, 3, 4
};


// Ajax (Japan)

static struct BurnRomInfo ajaxjRomDesc[] = {
	{"770_l01.n11", 0x10000, 0x7cea5274, 1 | BRF_PRG | BRF_ESS}, //  0 Konami Custom Code
	{"770_l02.n12", 0x10000, 0xad7d592b, 1 | BRF_PRG | BRF_ESS}, //  1

	{"770_l05.i16", 0x08000, 0xed64fbb2, 2 | BRF_PRG | BRF_ESS}, //  2 M6809 code
	{"770_f04.g16", 0x10000, 0xe0e4ec9c, 2 | BRF_PRG | BRF_ESS}, //  3

	{"770_f03.f16", 0x08000, 0x3fe914fd, 3 | BRF_PRG | BRF_ESS}, //  4 Z80 Code

	{"770c13.n22", 0x40000, 0xb859ca4e, 4 | BRF_GRA}, //  5 K052109 Tiles
	{"770c12.k22", 0x40000, 0x50d14b72, 4 | BRF_GRA}, //  6

	{"770c09.n4", 0x80000, 0x1ab4a7ff, 5 | BRF_GRA}, //  7 K051960 Tiles
	{"770c08.k4", 0x80000, 0xa8e80586, 5 | BRF_GRA}, //  8

	{"770c06.f4", 0x40000, 0xd0c592ee, 6 | BRF_GRA}, //  9 K051960 Tiles
	{"770c07.h4", 0x40000, 0x0b399fb1, 6 | BRF_GRA}, // 10

	{"770c10", 0x40000, 0x7fac825f, 7 | BRF_SND}, // 11 K007232 #0 Samples

	{"770c11", 0x80000, 0x299a615a, 8 | BRF_SND}, // 12 K007232 #1 Samples

	{"63s241.j11", 0x00200, 0x9bdd719f, 9 | BRF_OPT}, // 13 Timing Prom (unused)
};

STD_ROM_PICK(ajaxj)
STD_ROM_FN(ajaxj)

struct BurnDriver BurnDrvAjaxj = {
	"ajaxj", "ajax", nullptr, nullptr, "1987",
	"Ajax (Japan)\0", nullptr, "Konami", "GX770",
	L"Ajax\0\u30A8\u30FC\u30B8\u30E3\u30C3\u30AF\u30B9 (Japan)\0", nullptr, nullptr, nullptr,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2,
	HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	nullptr, ajaxjRomInfo, ajaxjRomName, nullptr, nullptr, nullptr, nullptr, AjaxInputInfo, AjaxDIPInfo,
	typhoonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	224, 304, 3, 4
};
