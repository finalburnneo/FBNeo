// FB Alpha Ping Pong driver module
// Based on MAME driver by Jarek Parchanski

#include "tiles_generic.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "sn76496.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvBankROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvTransTab;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvNVRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 nmi_enable;
static UINT8 irq_enable;
static UINT8 question_addr_low_data;
static UINT8 question_addr_high_data;
static UINT32 question_addr_high;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];

static UINT8 DrvInp[5];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static INT32 watchdog;
static INT32 nNMIMask;
static INT32 cashquiz = 0;

static struct BurnInputInfo PingpongInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvInputs + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvInputs + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvInputs + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvInputs + 3,	"dip"		},
};

STDINPUTINFO(Pingpong)

static struct BurnInputInfo MerlinmmInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvInputs + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvInputs + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvInputs + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvInputs + 3,	"dip"		},
	{"Dip E",		BIT_DIPSWITCH,	DrvInputs + 4,	"dip"		},
};

STDINPUTINFO(Merlinmm)

static struct BurnInputInfo CashquizInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 5"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvInputs + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvInputs + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvInputs + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvInputs + 3,	"dip"		},
	{"Dip E",		BIT_DIPSWITCH,	DrvInputs + 4,	"dip"		},
};

STDINPUTINFO(Cashquiz)

static struct BurnDIPInfo PingpongDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL			},
	{0x0f, 0xff, 0xff, 0xff, NULL			},
	{0x10, 0xff, 0xff, 0xff, NULL			},
	{0x11, 0xff, 0xff, 0xfa, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x10, 0x01, 0x0f, 0x04, "4 Coins 1 Credits"	},
	{0x10, 0x01, 0x0f, 0x0a, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0x0f, 0x01, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0x0f, 0x02, "3 Coins 2 Credits"	},
	{0x10, 0x01, 0x0f, 0x08, "4 Coins 3 Credits"	},
	{0x10, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0x0f, 0x0c, "3 Coins 4 Credits"	},
	{0x10, 0x01, 0x0f, 0x0e, "2 Coins 3 Credits"	},
	{0x10, 0x01, 0x0f, 0x07, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x10, 0x01, 0x0f, 0x0b, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0x0f, 0x0d, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x10, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x10, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x10, 0x01, 0xf0, 0x40, "4 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0xa0, "3 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x10, "2 Coins 1 Credits"	},
	{0x10, 0x01, 0xf0, 0x20, "3 Coins 2 Credits"	},
	{0x10, 0x01, 0xf0, 0x80, "4 Coins 3 Credits"	},
	{0x10, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x10, 0x01, 0xf0, 0xc0, "3 Coins 4 Credits"	},
	{0x10, 0x01, 0xf0, 0xe0, "2 Coins 3 Credits"	},
	{0x10, 0x01, 0xf0, 0x70, "1 Coin  2 Credits"	},
	{0x10, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x10, 0x01, 0xf0, 0xb0, "1 Coin  3 Credits"	},
	{0x10, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},
	{0x10, 0x01, 0xf0, 0xd0, "1 Coin  5 Credits"	},
	{0x10, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},
	{0x10, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x10, 0x01, 0xf0, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x01, 0x01, "Off"			},
	{0x11, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x06, 0x06, "Easy"			},
	{0x11, 0x01, 0x06, 0x02, "Normal"		},
	{0x11, 0x01, 0x06, 0x04, "Difficult"		},
	{0x11, 0x01, 0x06, 0x00, "Very Difficult"	},
};

STDDIPINFO(Pingpong)

static struct BurnDIPInfo MerlinmmDIPList[]=
{
	{0x07, 0xff, 0xff, 0xff, NULL		},
	{0x08, 0xff, 0xff, 0xff, NULL		},
	{0x09, 0xff, 0xff, 0xff, NULL		},
	{0x0a, 0xff, 0xff, 0xff, NULL		},
	{0x0b, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    2, "Bank 3-3"	},
	{0x07, 0x01, 0x01, 0x01, "Off"		},
	{0x07, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Bank 3-2"	},
	{0x07, 0x01, 0x02, 0x02, "Off"		},
	{0x07, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Bank 3-1"	},
	{0x07, 0x01, 0x04, 0x04, "Off"		},
	{0x07, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Door Close"	},
	{0x07, 0x01, 0x10, 0x10, "Off"		},
	{0x07, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Door Open"	},
	{0x07, 0x01, 0x20, 0x20, "Off"		},
	{0x07, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Stake"	},
	{0x09, 0x01, 0x02, 0x02, "10p"		},
	{0x09, 0x01, 0x02, 0x00, "20p"		},

	{0   , 0xfe, 0   ,    2, "Bank 1-6"	},
	{0x09, 0x01, 0x04, 0x04, "Off"		},
	{0x09, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Bank 1-5"	},
	{0x09, 0x01, 0x08, 0x08, "Off"		},
	{0x09, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "10p Enabled"	},
	{0x09, 0x01, 0x10, 0x10, "No"		},
	{0x09, 0x01, 0x10, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    2, "20p Enabled"	},
	{0x09, 0x01, 0x20, 0x20, "No"		},
	{0x09, 0x01, 0x20, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    2, "50p Enabled"	},
	{0x09, 0x01, 0x40, 0x40, "No"		},
	{0x09, 0x01, 0x40, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    2, "100p Enabled"	},
	{0x09, 0x01, 0x80, 0x80, "No"		},
	{0x09, 0x01, 0x80, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Bank 2-8"	},
	{0x0a, 0x01, 0x01, 0x01, "Off"		},
	{0x0a, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Bank 2-7"	},
	{0x0a, 0x01, 0x02, 0x02, "Off"		},
	{0x0a, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Bank 2-6"	},
	{0x0a, 0x01, 0x04, 0x04, "Off"		},
	{0x0a, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Bank 2-5"	},
	{0x0a, 0x01, 0x08, 0x08, "Off"		},
	{0x0a, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Bank 2-4"	},
	{0x0a, 0x01, 0x10, 0x10, "Off"		},
	{0x0a, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Bank 2-3"	},
	{0x0a, 0x01, 0x20, 0x20, "Off"		},
	{0x0a, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Bank 2-2"	},
	{0x0a, 0x01, 0x40, 0x40, "Off"		},
	{0x0a, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Bank 2-1"	},
	{0x0a, 0x01, 0x80, 0x80, "Off"		},
	{0x0a, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "10P Level"	},
	{0x0b, 0x01, 0x01, 0x01, "Low"		},
	{0x0b, 0x01, 0x01, 0x00, "High"		},

	{0   , 0xfe, 0   ,    2, "20P Level"	},
	{0x0b, 0x01, 0x02, 0x02, "Low"		},
	{0x0b, 0x01, 0x02, 0x00, "High"		},

	{0   , 0xfe, 0   ,    2, "50P Level"	},
	{0x0b, 0x01, 0x04, 0x04, "Low"		},
	{0x0b, 0x01, 0x04, 0x00, "High"		},

	{0   , 0xfe, 0   ,    2, "100P Level"	},
	{0x0b, 0x01, 0x08, 0x08, "Low"		},
	{0x0b, 0x01, 0x08, 0x00, "High"		},
};

STDDIPINFO(Merlinmm)

static struct BurnDIPInfo CashquizDIPList[]=
{
	{0x0d, 0xff, 0xff, 0xff, NULL		},
	{0x0e, 0xff, 0xff, 0xff, NULL		},
	{0x0f, 0xff, 0xff, 0xff, NULL		},
	{0x10, 0xff, 0xff, 0xff, NULL		},
	{0x11, 0xff, 0xff, 0xff, NULL		},
};

STDDIPINFO(Cashquiz)

static void cashquiz_question_bank_high_write(UINT8 data)
{
	if (data != 0xff)
	{
		question_addr_high_data = data;

		for (INT32 i = 0; i < 8; i++)
		{
			if ((data ^ 0xff) == (1 << i))
			{
				question_addr_high = i * 0x8000;
				return;
			}
		}
	}
}

static void cashquiz_question_bank_low_write(UINT8 data)
{
	if (data >= 0x60 && data <= 0xdf)
	{
		question_addr_low_data = data;

		INT32 bank = (question_addr_low_data & 7) * 0x100;
		INT32 bankaddr = question_addr_high + (question_addr_low_data - 0x60) * 0x100;

		ZetMapMemory(DrvBankROM + bankaddr, 0x5000 + bank, 0x50ff + bank, MAP_ROM);
	}
}

static void __fastcall pingpong_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
			if (cashquiz) cashquiz_question_bank_high_write(data);
		return;

		case 0x4001:
			if (cashquiz) cashquiz_question_bank_low_write(data);
		return;

		case 0xa000:
			irq_enable = data & 0x04;
			nmi_enable = data & 0x08;
			// coin counter 1 and 2 - data & 1, data & 2
		return;

		case 0xa200:
		return; // nop

		case 0xa400:
			SN76496Write(0, data);
		return;

		case 0xa600:
			watchdog = 0;
		return;
	}
}

static UINT8 __fastcall pingpong_read(UINT16 address)
{
	switch (address & ~0x800)
	{
		case 0x7000:	// merlinmm
			return DrvInp[4];

		case 0xa000:
		case 0xa080:
		case 0xa100:
		case 0xa180:
			return DrvInp[(address >> 7) & 3];
	}

	return 0;
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	if (cashquiz) ZetMapMemory(DrvBankROM, 0x5000, 0x57ff, MAP_ROM);
	ZetClose();

	question_addr_low_data = 0;
	question_addr_high_data = 0;
	question_addr_high = 0;

	irq_enable = 0;
	nmi_enable = 0;

	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x008000;

	if (cashquiz) {
		DrvBankROM	= Next; Next += 0x040000;
	}

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000220;

	DrvTransTab		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x00200 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000400;

	AllRam			= Next;

	DrvColRAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvZ80RAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes[2] = { 4, 0 };
	INT32 XOffs0[8] = { STEP4(3,-1), STEP4(8*8+3,-1) };
	INT32 XOffs1[16]= { STEP4(12*16+3,-1), STEP4(8*16+3,-1), STEP4(4*16+3,-1), STEP4(0*16+3,-1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(32*8,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Planes, XOffs0, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x2000);

	GfxDecode(0x0080, 2, 16, 16, Planes, XOffs1, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static void DrvPrgDecode(UINT8 *ROM, INT32 len)
{
	for (INT32 i = 0; i < len; i++) {
		ROM[i] = BITSWAP08(ROM[i],0,1,2,3,4,5,6,7);
	}
}

static INT32 PingpongInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  3, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020,  5, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0120,  6, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvColRAM,		0x8000, 0x83ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0x8400, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0x9000, 0x97ff, MAP_RAM);
	ZetSetWriteHandler(pingpong_write);
	ZetSetReadHandler(pingpong_read);
	ZetClose();

	SN76496Init(0, 18432000/8, 0);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	nNMIMask = 0x1f;

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 MerlinmmInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  2, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  3, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020,  4, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0120,  5, 1)) return 1;

		DrvPrgDecode(DrvZ80ROM, 0x04000);
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvNVRAM,		0x5000, 0x53ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0x5400, 0x57ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,		0x8000, 0x83ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0x8400, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0x9000, 0x97ff, MAP_RAM);
	ZetSetWriteHandler(pingpong_write);
	ZetSetReadHandler(pingpong_read);
	ZetClose();

	SN76496Init(0, 18432000/8, 0);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	nNMIMask = 0x1ff;

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 CashquizInit()
{
	cashquiz = 1;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;

		UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);

		for (INT32 i = 0; i < 8; i++) {
			if (BurnLoadRom(tmp, i+1, 1)) return 1;
			memcpy (DrvBankROM + i * 0x8000 + 0x2000, tmp + 0x0000, 0x6000);
			memcpy (DrvBankROM + i * 0x8000 + 0x0000, tmp + 0x6000, 0x2000);
		}

		BurnFree(tmp);

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  9, 1)) return 1;
		memcpy (DrvGfxROM0, DrvGfxROM0 + 0x2000, 0x2000);

	//	if (BurnLoadRom(DrvGfxROM1 + 0x0000,  10, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000,  11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020,  12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0120,  13, 1)) return 1;

		DrvPrgDecode(DrvZ80ROM,  0x04000);
		DrvPrgDecode(DrvBankROM, 0x40000);

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvBankROM,	0x5000, 0x57ff, MAP_ROM);
	ZetMapMemory(DrvColRAM,		0x8000, 0x83ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0x8400, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0x9000, 0x97ff, MAP_RAM);
	ZetSetWriteHandler(pingpong_write);
	ZetSetReadHandler(pingpong_read);
	ZetClose();

	SN76496Init(0, 18432000/8, 0);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	nNMIMask = 0x1ff;

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	SN76496Exit();

	BurnFree(AllMem);

	cashquiz = 0;

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 t[0x20];

	for (INT32 i = 0; i < 0x20; i++)
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

		t[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvPalette[i+0x000] = t[(DrvColPROM[i+0x020] & 0x0f) | 0x10];
		DrvPalette[i+0x100] = t[BITSWAP08(DrvColPROM[i+0x120], 7,6,5,4,0,1,2,3)];
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		DrvTransTab[i] = DrvPalette[i] ? 1 : 0;
	}
}

static void draw_layer()
{
	for (INT32 offs = 2 * 32; offs < 32 * 32 - 2 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy =((offs / 0x20) * 8) - 16;

		INT32 attr = DrvColRAM[offs];
		INT32 code = DrvVidRAM[offs] + ((attr & 0x20) * 8);
		INT32 color = attr & 0x1f;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY(pTransDraw, code, sx, sy, color, 2, 0, DrvGfxROM0);	
			} else {
				Render8x8Tile_FlipY(pTransDraw, code, sx, sy, color, 2, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX(pTransDraw, code, sx, sy, color, 2, 0, DrvGfxROM0);	
			} else {
				Render8x8Tile(pTransDraw, code, sx, sy, color, 2, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_sprites()
{
	GenericTilesSetClip(0, 256, 32 - 16, 232 - 16);

	UINT8 *spr = DrvSprRAM + 3;

	for (INT32 offs = 0x50; offs >= 0; offs -= 4)
	{
		INT32 attr = spr[offs + 0];
		INT32 sx   = spr[offs + 3];
		INT32 sy   = 241 - spr[offs + 1];
		INT32 code = spr[offs + 2] & 0x7f;

		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;
		INT32 color = attr & 0x1f;

		RenderTileTranstab(pTransDraw, DrvGfxROM1, code, color*4+0x100, 0, sx, sy - 16, flipx, flipy, 16, 16, DrvTransTab);
	}

	GenericTilesClearClip();
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_layer();
	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog > 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memcpy (DrvInp, DrvInputs, 5);

		for (INT32 i = 0; i < 8; i++) {
			DrvInp[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInp[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInp[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInp[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInp[4] ^= (DrvJoy5[i] & 1) << i;
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal = 18432000 / 6 / 60;
	INT32 nCyclesDone  = 0;

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone += ZetRun(nCyclesTotal / nInterleave);

		if (i == 240 && irq_enable) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		if ((i & nNMIMask) == 0 && nmi_enable) ZetNmi();
	}

	ZetClose();

	if (pBurnSoundOut) {
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_VOLATILE) {		
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		SN76496Scan(nAction, pnMin);

		SCAN_VAR(irq_enable);
		SCAN_VAR(nmi_enable);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00400;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if ((nAction & ACB_WRITE) && cashquiz) {
		ZetOpen(0);
		cashquiz_question_bank_high_write(question_addr_high_data);
		cashquiz_question_bank_low_write(question_addr_low_data);
		ZetClose();
	}

	return 0;
}


// Konami's Ping-Pong

static struct BurnRomInfo pingpongRomDesc[] = {
	{ "pp_e04.rom",		0x4000, 0x18552f8f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 code
	{ "pp_e03.rom",		0x4000, 0xae5f01e8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pp_e01.rom",		0x2000, 0xd1d6f090, 2 | BRF_GRA },           //  2 Background tiles

	{ "pp_e02.rom",		0x2000, 0x33c687e0, 3 | BRF_GRA },           //  3 Sprites

	{ "pingpong.3j",	0x0020, 0x3e04f06e, 4 | BRF_GRA },           //  4 Color data
	{ "pingpong.5h",	0x0100, 0x8456046a, 4 | BRF_GRA },           //  5
	{ "pingpong.11j",	0x0100, 0x09d96b08, 4 | BRF_GRA },           //  6
};

STD_ROM_PICK(pingpong)
STD_ROM_FN(pingpong)

struct BurnDriver BurnDrvPingpong = {
	"pingpong", NULL, NULL, NULL, "1985",
	"Konami's Ping-Pong\0", NULL, "Konami", "GX555",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_MISC, 0,
	NULL, pingpongRomInfo, pingpongRomName, NULL, NULL, PingpongInputInfo, PingpongDIPInfo,
	PingpongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Merlins Money Maze

static struct BurnRomInfo merlinmmRomDesc[] = {
	{ "merlinmm.ic2",	0x4000, 0xea5b6590, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 code

	{ "merlinmm.7h",	0x2000, 0xf7d535aa, 2 | BRF_GRA },           //  1 Background tiles

	{ "merl_sp.12c",	0x2000, 0x517ecd57, 3 | BRF_GRA },           //  2 Sprites

	{ "merlinmm.3j",	0x0020, 0xd56e91f4, 4 | BRF_GRA },           //  3 Color data
	{ "pingpong.5h",	0x0100, 0x8456046a, 4 | BRF_GRA },           //  4
	{ "pingpong.11j",	0x0100, 0x09d96b08, 4 | BRF_GRA },           //  5
};

STD_ROM_PICK(merlinmm)
STD_ROM_FN(merlinmm)

struct BurnDriver BurnDrvMerlinmm = {
	"merlinmm", NULL, NULL, NULL, "1986",
	"Merlins Money Maze\0", NULL, "Zilec-Zenitone", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 1, HARDWARE_PREFIX_KONAMI, GBF_MISC, 0,
	NULL, merlinmmRomInfo, merlinmmRomName, NULL, NULL, MerlinmmInputInfo, MerlinmmDIPInfo,
	MerlinmmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Cash Quiz (Type B, Version 5)

static struct BurnRomInfo cashquizRomDesc[] = {
	{ "cashqcv5.ic3",	0x4000, 0x8e9e2bed, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 code

	{ "q30_soaps.ic1",	0x8000, 0xb35a30ac, 2 | BRF_GRA },           //  1 Question data
	{ "q10.ic2",		0x8000, 0x54962e11, 2 | BRF_GRA },           //  2
	{ "q29_newsoccrick.ic3",0x8000, 0x03d47262, 2 | BRF_GRA },           //  3
	{ "q28_sportstime.ic4",	0x8000, 0x2bd00476, 2 | BRF_GRA },           //  4
	{ "q20_mot.ic5",	0x8000, 0x17a38baf, 2 | BRF_GRA },           //  5
	{ "q14_popmusic2.ic6",	0x8000, 0xe486d6ee, 2 | BRF_GRA },           //  6
	{ "q26_screenent.ic7",	0x8000, 0x9d130515, 2 | BRF_GRA },           //  7
	{ "q19.ic8",		0x8000, 0x9f3f77e6, 2 | BRF_GRA },           //  8

	{ "cashq.7h",		0x4000, 0x44b72a4f, 3 | BRF_GRA },           //  9 Background tiles

	{ "cashq.12c",		0x2000, 0x00000000, 4 | BRF_NODUMP },        // 10 Sprites

	{ "cashquiz.3j",	0x0020, 0xdc70e23b, 5 | BRF_GRA },           // 11 Color data
	{ "pingpong.5h",	0x0100, 0x8456046a, 5 | BRF_GRA },           // 12
	{ "pingpong.11j",	0x0100, 0x09d96b08, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(cashquiz)
STD_ROM_FN(cashquiz)

struct BurnDriver BurnDrvCashquiz = {
	"cashquiz", NULL, NULL, NULL, "1986",
	"Cash Quiz (Type B, Version 5)\0", "Missing graphics due to undumped ROM!", "Zilec-Zenitone", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_QUIZ, 0,
	NULL, cashquizRomInfo, cashquizRomName, NULL, NULL, CashquizInputInfo, CashquizDIPInfo,
	CashquizInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};
