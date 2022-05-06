// FB Alpha Vendetta / Crime Fighters 2 / Escape Kids driver module
// Based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "konami_intf.h"
#include "konamiic.h"
#include "k053260.h"
#include "eeprom.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvKonROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROMExp0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROMExp1;
static UINT8 *DrvSndROM;
static UINT8 *DrvKonRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DefaultEEPROM = NULL;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *nDrvBank;

static INT32 videobank;
static INT32 irq_enabled;
static INT32 vblank = 0;
static INT32 bankoffset;

static INT32 bg_colorbase;
static INT32 sprite_colorbase;
static INT32 layer_colorbase[3];
static INT32 layerpri[3];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6;
static UINT8 DrvReset;
static UINT8 DrvInputs[5];

static INT32 nCyclesDone[2];

static struct BurnInputInfo Vendet4pInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 coin"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy4 + 7,	"p4 coin"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 2,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 1,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 0,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Diagnostics",		BIT_DIGITAL,	&DrvJoy6,	"diag"		},
};

STDINPUTINFO(Vendet4p)

static struct BurnInputInfo VendettaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Diagnostics",		BIT_DIGITAL,	&DrvJoy6,	"diag"		},
	{"Service",		BIT_DIGITAL,	DrvJoy5 + 4,	"service"	},
};

STDINPUTINFO(Vendetta)

static struct BurnInputInfo EsckidsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy5 + 2,	"p3 start"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy4 + 7,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy5 + 3,	"p4 start"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 2,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 1,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 0,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Diagnostics",		BIT_DIGITAL,	&DrvJoy6,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy5 + 5,	"service2"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy5 + 6,	"service3"	},
	{"Service 4",		BIT_DIGITAL,	DrvJoy5 + 7,	"service4"	},
};

STDINPUTINFO(Esckids)

static struct BurnInputInfo EsckidsjInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy5 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Diagnostics",		BIT_DIGITAL,	&DrvJoy6,	"diag"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy5 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy5 + 5,	"service2"	},
};

STDINPUTINFO(Esckidsj)

void vendetta_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x5fe0:
			K052109RMRDLine = data & 0x08;
			K053246_set_OBJCHA_line(data & 0x20);
		return;

		case 0x5fe2:
		{
			if (data == 0xff) return;

			EEPROMWrite(data & 0x10, data & 0x08, data & 0x20);

			irq_enabled = (data >> 6) & 1;

			videobank = data & 1;
		}
		return;

		case 0x5fe4:
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x5fe6:
		case 0x5fe7:
			K053260Write(0, address & 1, data);
		return;
	}

	if ((address & 0xffe0) == 0x5f80) {
		K054000Write(address, data);
		return;
	}

	if ((address & 0xfff0) == 0x5fa0) {
		K053251Write(address & 0x0f, data);
		return;
	}

	if ((address & 0xfff8) == 0x5fb0) {
		K053246Write(address & 7, data);
		return;
	}

	if (videobank) {
		if ((address & 0xf000) == 0x4000) {
			address ^= 1;
			K053247Write(address & 0x0fff, data);
			return;
		}

		if ((address & 0xf000) == 0x6000) {
			DrvPalRAM[address & 0xfff] = data;
			return;
		}
	}

	if ((address & 0xc000) == 0x4000) {
		K052109Write(address & 0x3fff, data);
		return;
	}
}

UINT8 vendetta_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x5fc0:
			return DrvInputs[0];

		case 0x5fc1:
			return DrvInputs[1];

		case 0x5fc2:
			return DrvInputs[2];

		case 0x5fc3:
			return DrvInputs[3];

		case 0x5fd0:
			return (EEPROMRead() & 1) | vblank | ((DrvJoy6 << 2) ^ 0xf6);

		case 0x5fd1:
			return DrvInputs[4];

		case 0x5fe4:
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			return 0;

		case 0x5fe6:
		case 0x5fe7:
			return K053260Read(0, (address & 1)+2);

		case 0x5fe8:
		case 0x5fe9:
			return K053246Read(address & 1);

		case 0x5fea:
			return 0; // watchdog
	}

	if ((address & 0xffe0) == 0x5f80) {
		return K054000Read(address);
	}

	if (videobank) {
		if ((address & 0xf000) == 0x4000) {
			address ^= 1;
			return K053247Read(address & 0x0fff);
		}

		if ((address & 0xf000) == 0x6000) {
			return DrvPalRAM[address & 0x0fff];
		}
	}

	if ((address & 0xc000) == 0x4000) {
		return K052109Read(address & 0x3fff);
	}

	return 0;
}

void esckids_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3fd0:
			K052109RMRDLine = data & 0x08;
			K053246_set_OBJCHA_line(data & 0x20);
		return;

		case 0x3fd2:
		{
			if (data == 0xff) return;

			EEPROMWrite(data & 0x10, data & 0x08, data & 0x20);

			irq_enabled = (data >> 6) & 1;

			videobank = data & 1;
		}
		return;

		case 0x3fd4:
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x3fd6:
		case 0x3fd7:
			K053260Write(0, address & 1, data);
		return;
	}

	if ((address & 0xfff0) == 0x3fb0) {
		K053251Write(address & 0x0f, data);
		return;
	}

	if ((address & 0xfff8) == 0x3fa0) {
		K053246Write(address & 7, data);
		return;
	}

	if (videobank) {
		if ((address & 0xf000) == 0x2000) {
			address ^= 1;
			K053247Write(address & 0x0fff, data);
			return;
		}

		if ((address & 0xf000) == 0x4000) {
			DrvPalRAM[address & 0xfff] = data;
			return;
		}
	}

	if (address >= 0x2000 && address <= 0x5fff) {
		K052109Write(address - 0x2000, data);
		return;
	}
}

UINT8 esckids_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x3f80:
			return DrvInputs[0];

		case 0x3f81:
			return DrvInputs[1];

		case 0x3f82:
			return DrvInputs[2];

		case 0x3f83:
			return DrvInputs[3];

		case 0x3f92:
			return (EEPROMRead() & 1) | vblank | ((DrvJoy6 << 2) ^ 0xf6);

		case 0x3f93:
			return DrvInputs[4];

		case 0x3fd4:
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			return 0;

		case 0x3fd6:
		case 0x3fd7:
			return K053260Read(0, (address & 1)+2);

		case 0x3fd8:
		case 0x3fd9:
			return K053246Read(address & 1);

	}

	if (videobank) {
		if ((address & 0xf000) == 0x2000) {
			address ^= 1;
			return K053247Read(address & 0x0fff);
		}

		if ((address & 0xf000) == 0x4000) {
			return DrvPalRAM[address & 0x0fff];
		}
	}

	if (address >= 0x2000 && address <= 0x5fff) {
		return K052109Read(address - 0x2000);
	}

	return 0;
}

void __fastcall vendetta_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf800:
			BurnYM2151SelectRegister(data);
		return;

		case 0xf801:
			BurnYM2151WriteRegister(data);
		return;

		case 0xfa00:
			nCyclesDone[1] += ZetRun(100);
			ZetNmi();
		return;
	}

	if (address >= 0xfc00 && address < 0xfc30) {
		K053260Write(0, address & 0xff, data);
		return;
	}
}

UINT8 __fastcall vendetta_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			return 0xff;
		case 0xf801:
			return BurnYM2151Read();
	}

	if (address >= 0xfc00 && address < 0xfc30) {
		if ((address & 0x3f) == 0x01) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);

		return K053260Read(0, address & 0xff);
	}

	return 0;
}

static void vendetta_set_lines(INT32 lines)
{
	nDrvBank[0] = lines;

	if (lines < 0x1c) {
		konamiMapMemory(DrvKonROM + 0x10000 + (lines * 0x2000), 0x0000 | bankoffset, 0x1fff | bankoffset, MAP_ROM);
	}
}

static void K052109Callback(INT32 layer, INT32 bank, INT32 *code, INT32 *color, INT32 *, INT32 *)
{
	*code |= ((*color & 0x03) << 8) | ((*color & 0x30) << 6) | ((*color & 0x0c) << 10) | (bank << 14);
	*color = layer_colorbase[layer] + ((*color & 0xc0) >> 6);
}

static void EsckidsK052109Callback(INT32 layer, INT32 bank, INT32 *code, INT32 *color, INT32 *, INT32 *)
{
	*code |= ((*color & 0x03) << 8) | ((*color & 0x10) << 6) | ((*color & 0x0c) <<  9) | (bank << 13);
	*color = layer_colorbase[layer] + ((*color & 0xe0) >>  5);
}

static void VendettaK053247Callback(INT32 *code, INT32 *color, INT32 *priority)
{
	INT32 pri = (*color & 0x03e0) >> 4;
	if (pri <= layerpri[2])					*priority = 0;
	else if (pri > layerpri[2] && pri <= layerpri[1])	*priority = 0xf0;
	else if (pri > layerpri[1] && pri <= layerpri[0])	*priority = 0xf0 | 0xcc;
	else							*priority = 0xf0 | 0xcc | 0xaa;

	*code &= 0x7fff;

	*color = sprite_colorbase + (*color & 0x001f);
}

static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset (AllRam, 0, RamEnd - AllRam);

	konamiOpen(0);
	konamiReset();
	konamiClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnYM2151Reset();

	KonamiICReset();

	K053260Reset(0);

	EEPROMReset();

	videobank = 0;

	irq_enabled = 0;
	videobank = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvKonROM		= Next; Next += 0x050000;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x100000;
	DrvGfxROMExp0		= Next; Next += 0x200000;
	DrvGfxROM1		= Next; Next += 0x400000;
	DrvGfxROMExp1		= Next; Next += 0x800000;

	DrvSndROM		= Next; Next += 0x100000;

	DefaultEEPROM	= Next; Next += 0x000080;

	DrvPalette		= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;

	DrvKonRAM		= Next; Next += 0x002000;
	DrvPalRAM		= Next; Next += 0x001000;

	nDrvBank		= Next; Next += 0x000001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static const eeprom_interface vendetta_eeprom_intf =
{
	7,			// address bits
	8,			// data bits
	"011000",		//  read command
	"011100",		// write command
	0,			// erase command
	"0100000000000",	// lock command
	"0100110000000",	// unlock command
	0,
	0
};

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
		if (BurnLoadRom(DrvKonROM  + 0x010000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  1, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  2, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  3, 4, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  4, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  5, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004,  6, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006,  7, 8, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000,  8, 1)) return 1;

		if (BurnLoadRom(DefaultEEPROM, 9, 1)) return 1;

		K052109GfxDecode(DrvGfxROM0, DrvGfxROMExp0, 0x100000);
		K053247GfxDecode(DrvGfxROM1, DrvGfxROMExp1, 0x400000);
	}

	if (nGame) // escape kids
	{
		memcpy (DrvKonROM + 0x08000, DrvKonROM + 0x28000, 0x8000);

		konamiInit(0);
		konamiOpen(0);
		konamiMapMemory(DrvKonRAM,	     0x0000, 0x1fff, MAP_RAM);
		konamiMapMemory(DrvKonROM + 0x10000, 0x6000, 0x7fff, MAP_ROM);
		konamiMapMemory(DrvKonROM + 0x08000, 0x8000, 0xffff, MAP_ROM);
		konamiSetWriteHandler(esckids_main_write);
		konamiSetReadHandler(esckids_main_read);
		konamiSetlinesCallback(vendetta_set_lines);
		konamiClose();

		K052109Init(DrvGfxROM0, DrvGfxROMExp0, 0x0fffff);
		K052109SetCallback(EsckidsK052109Callback);
		K052109AdjustScroll(8, -8);

		K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x3fffff, VendettaK053247Callback, 1);
		K053247SetSpriteOffset(-20, -14);

		bankoffset = 0x6000;
	} else {
		memcpy (DrvKonROM + 0x08000, DrvKonROM + 0x48000, 0x8000);

		konamiInit(0);
		konamiOpen(0);
		konamiMapMemory(DrvKonROM + 0x10000, 0x0000, 0x1fff, MAP_ROM);
		konamiMapMemory(DrvKonRAM,	     0x2000, 0x3fff, MAP_RAM);
		konamiMapMemory(DrvKonROM + 0x08000, 0x8000, 0xffff, MAP_ROM);
		konamiSetWriteHandler(vendetta_main_write);
		konamiSetReadHandler(vendetta_main_read);
		konamiSetlinesCallback(vendetta_set_lines);
		konamiClose();

		K052109Init(DrvGfxROM0, DrvGfxROMExp0, 0x0fffff);
		K052109SetCallback(K052109Callback);
		K052109AdjustScroll(0, 0);

		K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x3fffff, VendettaK053247Callback, 1);
		K053247SetSpriteOffset(-53+2, -22);

		bankoffset = 0;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xefff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0xefff, 2, DrvZ80ROM);
	ZetMapArea(0xf000, 0xf7ff, 0, DrvZ80RAM);
	ZetMapArea(0xf000, 0xf7ff, 1, DrvZ80RAM);
	ZetMapArea(0xf000, 0xf7ff, 2, DrvZ80RAM);
	ZetSetWriteHandler(vendetta_sound_write);
	ZetSetReadHandler(vendetta_sound_read);
	ZetClose();

	EEPROMInit(&vendetta_eeprom_intf);
	if (!EEPROMAvailable()) EEPROMFill(DefaultEEPROM,0, 0x80);

	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	K053260Init(0, 3579545, DrvSndROM, 0x100000 >> nGame);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_1, 0.75, BURN_SND_ROUTE_LEFT);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_2, 0.75, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	KonamiICExit();

	konamiExit();
	ZetExit();

	EEPROMExit();

	BurnYM2151Exit();
	K053260Exit();

	BurnFree (AllMem);

	return 0;
}

static INT32 DrvDraw()
{
	KonamiRecalcPalette(DrvPalRAM, DrvPalette, 0x1000);
	KonamiClearBitmaps(0);

	K052109UpdateScroll();

	INT32 layer[3];

	bg_colorbase       = K053251GetPaletteIndex(0);
	sprite_colorbase   = K053251GetPaletteIndex(1);
	layer_colorbase[0] = K053251GetPaletteIndex(2);
	layer_colorbase[1] = K053251GetPaletteIndex(3);
	layer_colorbase[2] = K053251GetPaletteIndex(4);

	layerpri[0] = K053251GetPriority(2);
	layerpri[1] = K053251GetPriority(3);
	layerpri[2] = K053251GetPriority(4);
	layer[0] = 0;
	layer[1] = 1;
	layer[2] = 2;

	konami_sortlayers3(layer,layerpri);

	if (nBurnLayer & 1) K052109RenderLayer(layer[0], K052109_OPAQUE, 1);
	if (nBurnLayer & 2) K052109RenderLayer(layer[1], 0, 2);
	if (nBurnLayer & 4) K052109RenderLayer(layer[2], 0, 4);

	if (nSpriteEnable & 1) K053247SpritesRender();

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();
	konamiNewFrame();

	{
		memset (DrvInputs, 0xff, 5);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}

	  // Clear Opposites
		if ((DrvInputs[0] & 0x0c) == 0) DrvInputs[0] |= 0x0c;
		if ((DrvInputs[0] & 0x03) == 0) DrvInputs[0] |= 0x03;
		if ((DrvInputs[1] & 0x0c) == 0) DrvInputs[1] |= 0x0c;
		if ((DrvInputs[1] & 0x03) == 0) DrvInputs[1] |= 0x03;
		if ((DrvInputs[2] & 0x0c) == 0) DrvInputs[2] |= 0x0c;
		if ((DrvInputs[2] & 0x03) == 0) DrvInputs[2] |= 0x03;
		if ((DrvInputs[3] & 0x0c) == 0) DrvInputs[3] |= 0x0c;
		if ((DrvInputs[3] & 0x03) == 0) DrvInputs[3] |= 0x03;
	}

	INT32 nInterleave = 100;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 3579545 / 60 };
	
	nCyclesDone[0] = nCyclesDone[1] = 0;
	
	ZetOpen(0);
	konamiOpen(0);

	vblank = 8;

	INT32 trigger_vblank = (nInterleave / 256) * 240;

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nNext, nCyclesSegment;

		nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[0];
		nCyclesSegment = konamiRun(nCyclesSegment);
		nCyclesDone[0] += nCyclesSegment;

		nNext = (i + 1) * nCyclesTotal[1] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[1];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[1] += nCyclesSegment;

		if (i == trigger_vblank) vblank = 0; // or 8?

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (irq_enabled) konamiSetIrqLine(KONAMI_IRQ_LINE, CPU_IRQSTATUS_AUTO);

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
		}
	}

	konamiClose();
	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
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

		konamiCpuScan(nAction);
		ZetScan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		K053260Scan(nAction, pnMin);

		KonamiICScan(nAction);

		EEPROMScan(nAction, pnMin);

		SCAN_VAR(irq_enabled);
		SCAN_VAR(videobank);
		SCAN_VAR(nCyclesDone[1]);
	}

	if (nAction & ACB_WRITE) {
		konamiOpen(0);
		vendetta_set_lines(nDrvBank[0]);
		konamiClose();
	}

	return 0;
}


// Vendetta (World, 4 Players ver. T)

static struct BurnRomInfo vendettaRomDesc[] = {
	{ "081t01.17c",	0x040000, 0xe76267f5, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code

	{ "081b02",		0x010000, 0x4c604d9b, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "081a09",		0x080000, 0xb4c777a9, 3 | BRF_GRA },           //  2 K052109 Tiles
	{ "081a08",		0x080000, 0x272ac8d9, 3 | BRF_GRA },           //  3

	{ "081a04",		0x100000, 0x464b9aa4, 4 | BRF_GRA },           //  4 K053247 Tiles
	{ "081a05",		0x100000, 0x4e173759, 4 | BRF_GRA },           //  5
	{ "081a06",		0x100000, 0xe9fe6d80, 4 | BRF_GRA },           //  6
	{ "081a07",		0x100000, 0x8a22b29a, 4 | BRF_GRA },           //  7

	{ "081a03",		0x100000, 0x14b6baea, 5 | BRF_SND },           //  8 K053260 Samples

	{ "vendetta.nv",  0x000080, 0xfbac4e30, BRF_ESS | BRF_PRG },   //  9 EEPROM

	{ "p1-pal16l8acn.17e",	0x000117, 0xeae70da3, 7 | BRF_OPT },   // 10 PLDs (BAD_DUMP / Bruteforced)
	{ "p2-pal16l8acn.14e",	0x000117, 0xb84abb7d, 7 | BRF_OPT },   // 11      (BAD_DUMP / Bruteforced)
};

STD_ROM_PICK(vendetta)
STD_ROM_FN(vendetta)

static INT32 VendettaInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvVendetta = {
	"vendetta", NULL, NULL, NULL, "1991",
	"Vendetta (World, 4 Players ver. T)\0", NULL, "Konami", "GX081",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, vendettaRomInfo, vendettaRomName, NULL, NULL, NULL, NULL, Vendet4pInputInfo, NULL,
	VendettaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	304, 224, 4, 3
};


// Vendetta (US, 4 Players ver. R)

static struct BurnRomInfo vendettarRomDesc[] = {
	{ "081r01.17c",	0x040000, 0x84796281, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code

	{ "081b02",		0x010000, 0x4c604d9b, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "081a09",		0x080000, 0xb4c777a9, 3 | BRF_GRA },           //  2 K052109 Tiles
	{ "081a08",		0x080000, 0x272ac8d9, 3 | BRF_GRA },           //  3

	{ "081a04",		0x100000, 0x464b9aa4, 4 | BRF_GRA },           //  4 K053247 Tiles
	{ "081a05",		0x100000, 0x4e173759, 4 | BRF_GRA },           //  5
	{ "081a06",		0x100000, 0xe9fe6d80, 4 | BRF_GRA },           //  6
	{ "081a07",		0x100000, 0x8a22b29a, 4 | BRF_GRA },           //  7

	{ "081a03",		0x100000, 0x14b6baea, 5 | BRF_SND },           //  8 K053260 Samples

	{ "vendettar.nv",  0x000080, 0xec3f0449, BRF_ESS | BRF_PRG },  //  9 EEPROM

	{ "p1-pal16l8acn.17e",	0x000117, 0xeae70da3, 7 | BRF_OPT },   // 10 PLDs (BAD_DUMP / Bruteforced)
	{ "p2-pal16l8acn.14e",	0x000117, 0xb84abb7d, 7 | BRF_OPT },   // 11      (BAD_DUMP / Bruteforced)
};

STD_ROM_PICK(vendettar)
STD_ROM_FN(vendettar)

struct BurnDriver BurnDrvVendettar = {
	"vendettar", "vendetta", NULL, NULL, "1991",
	"Vendetta (US, 4 Players ver. R)\0", NULL, "Konami", "GX081",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, vendettarRomInfo, vendettarRomName, NULL, NULL, NULL, NULL, Vendet4pInputInfo, NULL,
	VendettaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	304, 224, 4, 3
};


// Vendetta (Asia, 4 Players ver. Z)

static struct BurnRomInfo vendettazRomDesc[] = {
	{ "081z01.17c",	0x040000, 0x4d225a8d, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code

	{ "081b02",		0x010000, 0x4c604d9b, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "081a09",		0x080000, 0xb4c777a9, 3 | BRF_GRA },           //  2 K052109 Tiles
	{ "081a08",		0x080000, 0x272ac8d9, 3 | BRF_GRA },           //  3

	{ "081a04",		0x100000, 0x464b9aa4, 4 | BRF_GRA },           //  4 K053247 Tiles
	{ "081a05",		0x100000, 0x4e173759, 4 | BRF_GRA },           //  5
	{ "081a06",		0x100000, 0xe9fe6d80, 4 | BRF_GRA },           //  6
	{ "081a07",		0x100000, 0x8a22b29a, 4 | BRF_GRA },           //  7

	{ "081a03",		0x100000, 0x14b6baea, 5 | BRF_SND },           //  8 K053260 Samples

	{ "vendetta.nv",  0x000080, 0xfbac4e30, BRF_ESS | BRF_PRG },   //  9 EEPROM

	{ "p1-pal16l8acn.17e",	0x000117, 0xeae70da3, 7 | BRF_OPT },   // 10 PLDs (BAD_DUMP / Bruteforced)
	{ "p2-pal16l8acn.14e",	0x000117, 0xb84abb7d, 7 | BRF_OPT },   // 11      (BAD_DUMP / Bruteforced)
};

STD_ROM_PICK(vendettaz)
STD_ROM_FN(vendettaz)

struct BurnDriver BurnDrvVendettaz = {
	"vendettaz", "vendetta", NULL, NULL, "1991",
	"Vendetta (Asia, 4 Players ver. Z)\0", NULL, "Konami", "GX081",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, vendettazRomInfo, vendettazRomName, NULL, NULL, NULL, NULL, Vendet4pInputInfo, NULL,
	VendettaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	304, 224, 4, 3
};


// Vendetta (World, 4 Players, ver. ?)

static struct BurnRomInfo vendettaunRomDesc[] = {
	/* program rom labeled as 1 */
	{ "1.17c",	    0x040000, 0x1a7ceb1b, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code

	{ "081b02",		0x010000, 0x4c604d9b, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "081a09",		0x080000, 0xb4c777a9, 3 | BRF_GRA },           //  2 K052109 Tiles
	{ "081a08",		0x080000, 0x272ac8d9, 3 | BRF_GRA },           //  3

	{ "081a04",		0x100000, 0x464b9aa4, 4 | BRF_GRA },           //  4 K053247 Tiles
	{ "081a05",		0x100000, 0x4e173759, 4 | BRF_GRA },           //  5
	{ "081a06",		0x100000, 0xe9fe6d80, 4 | BRF_GRA },           //  6
	{ "081a07",		0x100000, 0x8a22b29a, 4 | BRF_GRA },           //  7

	{ "081a03",		0x100000, 0x14b6baea, 5 | BRF_SND },           //  8 K053260 Samples

	{ "vendetta.nv",  0x000080, 0xfbac4e30, BRF_ESS | BRF_PRG },   //  9 EEPROM

	{ "p1-pal16l8acn.17e",	0x000117, 0xeae70da3, 7 | BRF_OPT },   // 10 PLDs (BAD_DUMP / Bruteforced)
	{ "p2-pal16l8acn.14e",	0x000117, 0xb84abb7d, 7 | BRF_OPT },   // 11      (BAD_DUMP / Bruteforced)
};

STD_ROM_PICK(vendettaun)
STD_ROM_FN(vendettaun)

struct BurnDriver BurnDrvVendettaun = {
	"vendettaun", "vendetta", NULL, NULL, "1991",
	"Vendetta (World, 4 Players, ver. ?)\0", NULL, "Konami", "GX081",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, vendettaunRomInfo, vendettaunRomName, NULL, NULL, NULL, NULL, Vendet4pInputInfo, NULL,
	VendettaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	304, 224, 4, 3
};


// Vendetta (World, 2 Players ver. W)

static struct BurnRomInfo vendetta2pwRomDesc[] = {
	{ "081w01.17c",	0x040000, 0xcee57132, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code

	{ "081b02",		0x010000, 0x4c604d9b, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "081a09",		0x080000, 0xb4c777a9, 3 | BRF_GRA },           //  2 K052109 Tiles
	{ "081a08",		0x080000, 0x272ac8d9, 3 | BRF_GRA },           //  3

	{ "081a04",		0x100000, 0x464b9aa4, 4 | BRF_GRA },           //  4 K053247 Tiles
	{ "081a05",		0x100000, 0x4e173759, 4 | BRF_GRA },           //  5
	{ "081a06",		0x100000, 0xe9fe6d80, 4 | BRF_GRA },           //  6
	{ "081a07",		0x100000, 0x8a22b29a, 4 | BRF_GRA },           //  7

	{ "081a03",		0x100000, 0x14b6baea, 5 | BRF_SND },           //  8 K053260 Samples

	{ "vendetta.nv",  0x000080, 0xfbac4e30, BRF_ESS | BRF_PRG },   //  9 EEPROM

	{ "p1-pal16l8acn.17e",	0x000117, 0xeae70da3, 7 | BRF_OPT },   // 10 PLDs (BAD_DUMP / Bruteforced)
	{ "p2-pal16l8acn.14e",	0x000117, 0xb84abb7d, 7 | BRF_OPT },   // 11      (BAD_DUMP / Bruteforced)
};

STD_ROM_PICK(vendetta2pw)
STD_ROM_FN(vendetta2pw)

struct BurnDriver BurnDrvVendetta2pw = {
	"vendetta2pw", "vendetta", NULL, NULL, "1991",
	"Vendetta (World, 2 Players ver. W)\0", NULL, "Konami", "GX081",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, vendetta2pwRomInfo, vendetta2pwRomName, NULL, NULL, NULL, NULL, VendettaInputInfo, NULL,
	VendettaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	304, 224, 4, 3
};


// Vendetta (World, 2 Players ver. EB-A?)

static struct BurnRomInfo vendetta2pebaRomDesc[] = {
	{ "081-eb-a01.17c",    0x040000, 0x8430bb52, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code

	{ "081b02",            0x010000, 0x4c604d9b, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "081a09",            0x080000, 0xb4c777a9, 3 | BRF_GRA },           //  2 K052109 Tiles
	{ "081a08",            0x080000, 0x272ac8d9, 3 | BRF_GRA },           //  3

	{ "081a04",            0x100000, 0x464b9aa4, 4 | BRF_GRA },           //  4 K053247 Tiles
	{ "081a05",            0x100000, 0x4e173759, 4 | BRF_GRA },           //  5
	{ "081a06",            0x100000, 0xe9fe6d80, 4 | BRF_GRA },           //  6
	{ "081a07",            0x100000, 0x8a22b29a, 4 | BRF_GRA },           //  7

	{ "081a03",            0x100000, 0x14b6baea, 5 | BRF_SND },           //  8 K053260 Samples

	{ "vendetta.nv",    0x000080, 0xfbac4e30, BRF_ESS | BRF_PRG },        //  9 EEPROM

	{ "p1-pal16l8acn.17e",	0x000117, 0xeae70da3, 7 | BRF_OPT },          // 10 PLDs (BAD_DUMP / Bruteforced)
	{ "p2-pal16l8acn.14e",	0x000117, 0xb84abb7d, 7 | BRF_OPT },          // 11      (BAD_DUMP / Bruteforced)
};

STD_ROM_PICK(vendetta2peba)
STD_ROM_FN(vendetta2peba)

struct BurnDriver BurnDrvVendetta2peba = {
    "vendetta2peba", "vendetta", NULL, NULL, "1991",
    "Vendetta (World, 2 Players ver. EB-A?)\0", NULL, "Konami", "GX081",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
    NULL, vendetta2pebaRomInfo, vendetta2pebaRomName, NULL, NULL, NULL, NULL, VendettaInputInfo, NULL,
    VendettaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
    304, 224, 4, 3
};


// Vendetta (World, 2 Players ver. ?)

static struct BurnRomInfo vendetta2punRomDesc[] = {
	/* program rom labeled as 1 */
	{ "1.17c",		0x040000, 0xb4edde48, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code

	{ "081b02",		0x010000, 0x4c604d9b, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "081a09",		0x080000, 0xb4c777a9, 3 | BRF_GRA },           //  2 K052109 Tiles
	{ "081a08",		0x080000, 0x272ac8d9, 3 | BRF_GRA },           //  3

	{ "081a04",		0x100000, 0x464b9aa4, 4 | BRF_GRA },           //  4 K053247 Tiles
	{ "081a05",		0x100000, 0x4e173759, 4 | BRF_GRA },           //  5
	{ "081a06",		0x100000, 0xe9fe6d80, 4 | BRF_GRA },           //  6
	{ "081a07",		0x100000, 0x8a22b29a, 4 | BRF_GRA },           //  7

	{ "081a03",		0x100000, 0x14b6baea, 5 | BRF_SND },           //  8 K053260 Samples

	{ "vendetta.nv",  0x000080, 0xfbac4e30, BRF_ESS | BRF_PRG },   //  9 EEPROM

	{ "p1-pal16l8acn.17e",	0x000117, 0xeae70da3, 7 | BRF_OPT },   // 10 PLDs (BAD_DUMP / Bruteforced)
	{ "p2-pal16l8acn.14e",	0x000117, 0xb84abb7d, 7 | BRF_OPT },   // 11      (BAD_DUMP / Bruteforced)
};

STD_ROM_PICK(vendetta2pun)
STD_ROM_FN(vendetta2pun)

struct BurnDriver BurnDrvVendetta2pun = {
	"vendetta2pun", "vendetta", NULL, NULL, "1991",
	"Vendetta (World, 2 Players ver. ?)\0", NULL, "Konami", "GX081",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, vendetta2punRomInfo, vendetta2punRomName, NULL, NULL, NULL, NULL, VendettaInputInfo, NULL,
	VendettaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	304, 224, 4, 3
};


// Vendetta (Asia, 2 Players ver. U)

static struct BurnRomInfo vendetta2puRomDesc[] = {
	{ "081u01.17c",	0x040000, 0xb4d9ade5, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code

	{ "081b02",		0x010000, 0x4c604d9b, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "081a09",		0x080000, 0xb4c777a9, 3 | BRF_GRA },           //  2 K052109 Tiles
	{ "081a08",		0x080000, 0x272ac8d9, 3 | BRF_GRA },           //  3

	{ "081a04",		0x100000, 0x464b9aa4, 4 | BRF_GRA },           //  4 K053247 Tiles
	{ "081a05",		0x100000, 0x4e173759, 4 | BRF_GRA },           //  5
	{ "081a06",		0x100000, 0xe9fe6d80, 4 | BRF_GRA },           //  6
	{ "081a07",		0x100000, 0x8a22b29a, 4 | BRF_GRA },           //  7

	{ "081a03",		0x100000, 0x14b6baea, 5 | BRF_SND },           //  8 K053260 Samples

	{ "vendetta.nv",  0x000080, 0xfbac4e30, BRF_ESS | BRF_PRG },   //  9 EEPROM

	{ "p1-pal16l8acn.17e",	0x000117, 0xeae70da3, 7 | BRF_OPT },   // 10 PLDs (BAD_DUMP / Bruteforced)
	{ "p2-pal16l8acn.14e",	0x000117, 0xb84abb7d, 7 | BRF_OPT },   // 11      (BAD_DUMP / Bruteforced)
};

STD_ROM_PICK(vendetta2pu)
STD_ROM_FN(vendetta2pu)

struct BurnDriver BurnDrvVendetta2pu = {
	"vendetta2pu", "vendetta", NULL, NULL, "1991",
	"Vendetta (Asia, 2 Players ver. U)\0", NULL, "Konami", "GX081",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, vendetta2puRomInfo, vendetta2puRomName, NULL, NULL, NULL, NULL, VendettaInputInfo, NULL,
	VendettaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	304, 224, 4, 3
};


// Vendetta (Asia, 2 Players ver. D)

static struct BurnRomInfo vendetta2pdRomDesc[] = {
	{ "081d01.17c",	0x040000, 0x335da495, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code

	{ "081b02",		0x010000, 0x4c604d9b, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "081a09",		0x080000, 0xb4c777a9, 3 | BRF_GRA },           //  2 K052109 Tiles
	{ "081a08",		0x080000, 0x272ac8d9, 3 | BRF_GRA },           //  3

	{ "081a04",		0x100000, 0x464b9aa4, 4 | BRF_GRA },           //  4 K053247 Tiles
	{ "081a05",		0x100000, 0x4e173759, 4 | BRF_GRA },           //  5
	{ "081a06",		0x100000, 0xe9fe6d80, 4 | BRF_GRA },           //  6
	{ "081a07",		0x100000, 0x8a22b29a, 4 | BRF_GRA },           //  7

	{ "081a03",		0x100000, 0x14b6baea, 5 | BRF_SND },           //  8 K053260 Samples

	{ "vendetta.nv",  0x000080, 0xfbac4e30, BRF_ESS | BRF_PRG },   //  9 EEPROM

	{ "p1-pal16l8acn.17e",	0x000117, 0xeae70da3, 7 | BRF_OPT },   // 10 PLDs (BAD_DUMP / Bruteforced)
	{ "p2-pal16l8acn.14e",	0x000117, 0xb84abb7d, 7 | BRF_OPT },   // 11      (BAD_DUMP / Bruteforced)
};

STD_ROM_PICK(vendetta2pd)
STD_ROM_FN(vendetta2pd)

struct BurnDriver BurnDrvVendetta2pd = {
	"vendetta2pd", "vendetta", NULL, NULL, "1991",
	"Vendetta (Asia, 2 Players ver. D)\0", NULL, "Konami", "GX081",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, vendetta2pdRomInfo, vendetta2pdRomName, NULL, NULL, NULL, NULL, VendettaInputInfo, NULL,
	VendettaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	304, 224, 4, 3
};


// Crime Fighters 2 (Japan, 4 Players ver. N)

static struct BurnRomInfo vendettanRomDesc[] = {
	{ "081n01.17c",	0x040000, 0xfc766fab, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code

	{ "081b02",		0x010000, 0x4c604d9b, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "081a09",		0x080000, 0xb4c777a9, 3 | BRF_GRA },           //  2 K052109 Tiles
	{ "081a08",		0x080000, 0x272ac8d9, 3 | BRF_GRA },           //  3

	{ "081a04",		0x100000, 0x464b9aa4, 4 | BRF_GRA },           //  4 K053247 Tiles
	{ "081a05",		0x100000, 0x4e173759, 4 | BRF_GRA },           //  5
	{ "081a06",		0x100000, 0xe9fe6d80, 4 | BRF_GRA },           //  6
	{ "081a07",		0x100000, 0x8a22b29a, 4 | BRF_GRA },           //  7

	{ "081a03",		0x100000, 0x14b6baea, 5 | BRF_SND },           //  8 K053260 Samples

	{ "vendettaj.nv",  0x000080, 0x3550a54e, BRF_ESS | BRF_PRG },  //  9 EEPROM

	{ "p1-pal16l8acn.17e",	0x000117, 0xeae70da3, 7 | BRF_OPT },   // 10 PLDs (BAD_DUMP / Bruteforced)
	{ "p2-pal16l8acn.14e",	0x000117, 0xb84abb7d, 7 | BRF_OPT },   // 11      (BAD_DUMP / Bruteforced)
};

STD_ROM_PICK(vendettan)
STD_ROM_FN(vendettan)

struct BurnDriver BurnDrvVendettan = {
	"vendettan", "vendetta", NULL, NULL, "1991",
	"Crime Fighters 2 (Japan, 4 Players ver. N)\0", NULL, "Konami", "GX081",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, vendettanRomInfo, vendettanRomName, NULL, NULL, NULL, NULL, Vendet4pInputInfo, NULL,
	VendettaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	304, 224, 4, 3
};


// Crime Fighters 2 (Japan, 2 Players ver. P)

static struct BurnRomInfo vendetta2ppRomDesc[] = {
	{ "081p01.17c",	0x040000, 0x5fe30242, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code

	{ "081b02",		0x010000, 0x4c604d9b, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "081a09",		0x080000, 0xb4c777a9, 3 | BRF_GRA },           //  2 K052109 Tiles
	{ "081a08",		0x080000, 0x272ac8d9, 3 | BRF_GRA },           //  3

	{ "081a04",		0x100000, 0x464b9aa4, 4 | BRF_GRA },           //  4 K053247 Tiles
	{ "081a05",		0x100000, 0x4e173759, 4 | BRF_GRA },           //  5
	{ "081a06",		0x100000, 0xe9fe6d80, 4 | BRF_GRA },           //  6
	{ "081a07",		0x100000, 0x8a22b29a, 4 | BRF_GRA },           //  7

	{ "081a03",		0x100000, 0x14b6baea, 5 | BRF_SND },           //  8 K053260 Samples

	{ "vendettaj.nv",  0x000080, 0x3550a54e, BRF_ESS | BRF_PRG },  //  9 EEPROM

	{ "p1-pal16l8acn.17e",	0x000117, 0xeae70da3, 7 | BRF_OPT },   // 10 PLDs (BAD_DUMP / Bruteforced)
	{ "p2-pal16l8acn.14e",	0x000117, 0xb84abb7d, 7 | BRF_OPT },   // 11      (BAD_DUMP / Bruteforced)
};

STD_ROM_PICK(vendetta2pp)
STD_ROM_FN(vendetta2pp)

struct BurnDriver BurnDrvVendetta2pp = {
	"vendetta2pp", "vendetta", NULL, NULL, "1991",
	"Crime Fighters 2 (Japan, 2 Players ver. P)\0", NULL, "Konami", "GX081",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, vendetta2ppRomInfo, vendetta2ppRomName, NULL, NULL, NULL, NULL, VendettaInputInfo, NULL,
	VendettaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	304, 224, 4, 3
};


// Escape Kids (Asia, 4 Players)

static struct BurnRomInfo esckidsRomDesc[] = {
	{ "17c.bin", 0x020000, 0x9dfba99c, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code

	{ "975f02",	0x010000, 0x994fb229, 2 | BRF_PRG | BRF_ESS },  //  1 Z80 Code

	{ "975c09",	0x080000, 0xbc52210e, 3 | BRF_GRA },            //  2 K052109 Tiles
	{ "975c08",	0x080000, 0xfcff9256, 3 | BRF_GRA },            //  3

	{ "975c04",	0x100000, 0x15688a6f, 4 | BRF_GRA },            //  4 K053247 Tiles
	{ "975c05",	0x100000, 0x1ff33bb7, 4 | BRF_GRA },            //  5
	{ "975c06",	0x100000, 0x36d410f9, 4 | BRF_GRA },            //  6
	{ "975c07",	0x100000, 0x97ec541e, 4 | BRF_GRA },            //  7

	{ "975c03",	0x080000, 0xdc4a1707, 5 | BRF_SND },            //  8 K053260 Samples

	{ "esckids.nv",  0x000080, 0xa8522e1f, BRF_ESS | BRF_PRG }, //  9 EEPROM
};

STD_ROM_PICK(esckids)
STD_ROM_FN(esckids)

static INT32 EsckidsInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvEsckids = {
	"esckids", NULL, NULL, NULL, "1991",
	"Escape Kids (Asia, 4 Players)\0", NULL, "Konami", "GX975",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, esckidsRomInfo, esckidsRomName, NULL, NULL, NULL, NULL, EsckidsInputInfo, NULL,
	EsckidsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// Escape Kids (Japan, 2 Players)

static struct BurnRomInfo esckidsjRomDesc[] = {
	{ "975r01",	0x020000, 0x7b5c5572, 1 | BRF_PRG | BRF_ESS },   //  0 Konami Custom Code

	{ "975f02",	0x010000, 0x994fb229, 2 | BRF_PRG | BRF_ESS },   //  1 Z80 Code

	{ "975c09",	0x080000, 0xbc52210e, 3 | BRF_GRA },             //  2 K052109 Tiles
	{ "975c08",	0x080000, 0xfcff9256, 3 | BRF_GRA },             //  3

	{ "975c04",	0x100000, 0x15688a6f, 4 | BRF_GRA },             //  4 K053247 Tiles
	{ "975c05",	0x100000, 0x1ff33bb7, 4 | BRF_GRA },             //  5
	{ "975c06",	0x100000, 0x36d410f9, 4 | BRF_GRA },             //  6
	{ "975c07",	0x100000, 0x97ec541e, 4 | BRF_GRA },             //  7

	{ "975c03",	0x080000, 0xdc4a1707, 5 | BRF_SND },             //  8 K053260 Samples

	{ "esckidsj.nv",  0x000080, 0x985e2a2d, BRF_ESS | BRF_PRG }, //  9 EEPROM
};

STD_ROM_PICK(esckidsj)
STD_ROM_FN(esckidsj)

struct BurnDriver BurnDrvEsckidsj = {
	"esckidsj", "esckids", NULL, NULL, "1991",
	"Escape Kids (Japan, 2 Players)\0", NULL, "Konami", "GX975",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, esckidsjRomInfo, esckidsjRomName, NULL, NULL, NULL, NULL, EsckidsjInputInfo, NULL,
	EsckidsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 240, 4, 3
};
