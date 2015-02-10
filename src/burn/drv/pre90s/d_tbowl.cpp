// FB Alpha Tecmo Bowl driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "msm5205.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSndROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvBgRAM2;
static UINT8 *DrvBgRAM;
static UINT8 *DrvTxRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;

static UINT32  *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *DrvBank;
static UINT8 *DrvScroll;

static INT32 adpcm_data[2];
static INT32 adpcm_pos[2];
static INT32 adpcm_end[2];

static UINT8 *soundlatch;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT8 DrvInputs[5];

static struct BurnInputInfo TbowlInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy5 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy5 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy5 + 2,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 start"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy5 + 3,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p4 start"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 2,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 1,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 0,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service (General)",	BIT_DIGITAL,	DrvJoy5 + 4,	"service"	},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Service 4",		BIT_DIGITAL,	DrvJoy4 + 6,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Tbowl)

static struct BurnDIPInfo TbowlDIPList[]=
{
	{0x26, 0xff, 0xff, 0xbf, NULL			},
	{0x27, 0xff, 0xff, 0xff, NULL			},
	{0x28, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x26, 0x01, 0x07, 0x00, "8 Coins 1 Credits"	},
	{0x26, 0x01, 0x07, 0x01, "7 Coins 1 Credits"	},
	{0x26, 0x01, 0x07, 0x02, "6 Coins 1 Credits"	},
	{0x26, 0x01, 0x07, 0x03, "5 Coins 1 Credits"	},
	{0x26, 0x01, 0x07, 0x04, "4 Coins 1 Credits"	},
	{0x26, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x26, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x26, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,   31, "Time (Players)"	},
	{0x26, 0x01, 0xf8, 0x00, "7:00"			},
	{0x26, 0x01, 0xf8, 0x08, "6:00"			},
	{0x26, 0x01, 0xf8, 0x10, "5:00"			},
	{0x26, 0x01, 0xf8, 0x18, "4:30"			},
	{0x26, 0x01, 0xf8, 0x20, "3:40"			},
	{0x26, 0x01, 0xf8, 0x28, "3:20"			},
	{0x26, 0x01, 0xf8, 0x30, "3:00"			},
	{0x26, 0x01, 0xf8, 0x38, "2:50"			},
	{0x26, 0x01, 0xf8, 0x40, "2:40"			},
	{0x26, 0x01, 0xf8, 0x48, "2:30"			},
	{0x26, 0x01, 0xf8, 0x50, "2:20"			},
	{0x26, 0x01, 0xf8, 0x58, "2:10"			},
	{0x26, 0x01, 0xf8, 0x60, "2:00"			},
	{0x26, 0x01, 0xf8, 0x68, "1:55"			},
	{0x26, 0x01, 0xf8, 0x70, "1:50"			},
	{0x26, 0x01, 0xf8, 0x78, "1:45"			},
	{0x26, 0x01, 0xf8, 0x80, "1:40"			},
	{0x26, 0x01, 0xf8, 0x88, "1:35"			},
	{0x26, 0x01, 0xf8, 0x90, "1:25"			},
	{0x26, 0x01, 0xf8, 0x98, "1:20"			},
	{0x26, 0x01, 0xf8, 0xa0, "1:15"			},
	{0x26, 0x01, 0xf8, 0xa8, "1:10"			},
	{0x26, 0x01, 0xf8, 0xb0, "1:05"			},
	{0x26, 0x01, 0xf8, 0xb8, "1:00"			},
	{0x26, 0x01, 0xf8, 0xc0, "0:55"			},
	{0x26, 0x01, 0xf8, 0xc8, "0:50"			},
	{0x26, 0x01, 0xf8, 0xd0, "0:45"			},
	{0x26, 0x01, 0xf8, 0xd8, "0:40"			},
	{0x26, 0x01, 0xf8, 0xe0, "0:35"			},
	{0x26, 0x01, 0xf8, 0xe8, "0:30"			},
	{0x26, 0x01, 0xf8, 0xf0, "0:25"			},

	{0   , 0xfe, 0   ,    4, "Difficulty (unused ?)"},
	{0x27, 0x01, 0x03, 0x00, "0x00"			},
	{0x27, 0x01, 0x03, 0x01, "0x01"			},
	{0x27, 0x01, 0x03, 0x02, "0x02"			},
	{0x27, 0x01, 0x03, 0x03, "0x03"			},

	{0   , 0xfe, 0   ,    4, "Extra Time (Players)"	},
	{0x27, 0x01, 0x0c, 0x00, "0:30"			},
	{0x27, 0x01, 0x0c, 0x04, "0:20"			},
	{0x27, 0x01, 0x0c, 0x08, "0:10"			},
	{0x27, 0x01, 0x0c, 0x0c, "None"			},

	{0   , 0xfe, 0   ,    4, "Timer Speed"		},
	{0x27, 0x01, 0x30, 0x00, "56/60"		},
	{0x27, 0x01, 0x30, 0x10, "51/60"		},
	{0x27, 0x01, 0x30, 0x30, "47/60"		},
	{0x27, 0x01, 0x30, 0x20, "42/60"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x27, 0x01, 0x40, 0x00, "Off"			},
	{0x27, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Hi-Score Reset"	},
	{0x27, 0x01, 0x80, 0x00, "Off"			},
	{0x27, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    4, "Time (Quarter)"	},
	{0x28, 0x01, 0x03, 0x00, "8:00"			},
	{0x28, 0x01, 0x03, 0x01, "5:00"			},
	{0x28, 0x01, 0x03, 0x03, "4:00"			},
	{0x28, 0x01, 0x03, 0x02, "3:00"			},
};

STDDIPINFO(Tbowl)

static void bankswitch(INT32 n, INT32 data)
{
	DrvBank[n] = data;

	INT32 bank = 0x10000 + (data & 0xf8) * 256;

	ZetMapMemory((n ? DrvZ80ROM1 : DrvZ80ROM0) + bank, 0xf000, 0xf7ff, MAP_ROM);
}

static void __fastcall tbowl_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xfc00:
			bankswitch(0, data);
		return;

		case 0xfc03: // coin counter
		return;

		case 0xfc0d:
			*soundlatch = data;
			ZetClose();
			ZetOpen(2);
			ZetNmi();
			ZetClose();
			ZetOpen(0);
		return;
	}

	if ((address & 0xfff8) == 0xfc10) {
		DrvScroll[address & 0x07] = data;
		return;
	}
}

static UINT8 __fastcall tbowl_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xfc00:
			return DrvInputs[0];

		case 0xfc01:
			return DrvInputs[1];

		case 0xfc02:
			return DrvInputs[2];

		case 0xfc03:
			return DrvInputs[3];

		case 0xfc07:
			return DrvInputs[4];

		case 0xfc08:
			return DrvDips[0];

		case 0xfc09:
			return DrvDips[1];

		case 0xfc0a:
			return DrvDips[2];
	}

	return 0;
}

static void __fastcall tbowl_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xfc00:
			bankswitch(1, data);
		return;

		case 0xfc02:
			ZetClose();
			ZetOpen(0);
			ZetNmi();
			ZetClose();
			ZetOpen(1);
		return;
	}
}

static void __fastcall tbowl_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd000:
		case 0xd001:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0xd800:
		case 0xd801:
			BurnYM3812Write(1, address & 1, data);
		return;

		case 0xe000:
		case 0xe001:
			adpcm_end[address & 1] = (data + 1) << 8;
		return;

		case 0xe002:
		case 0xe003:
			adpcm_pos[address & 1] = data << 8;
			MSM5205ResetWrite(address & 1, 0);
		return;

		case 0xe004:
		case 0xe005:
			data &= 0x7f;
			MSM5205SetRoute(address & 1, (double)data / 0x7f, BURN_SND_ROUTE_BOTH);
		return;
	}
}

static UINT8 __fastcall tbowl_sound_read(UINT16 address)
{
	if (address == 0xe010) return *soundlatch;

	return 0;
}

static void tbowl_adpcm_int(INT32 num)
{
	if (adpcm_pos[num] >= adpcm_end[num] || adpcm_pos[num] >= 0x10000)
	{
		MSM5205ResetWrite(num, 1);
	}
	else if (adpcm_data[num] != -1)
	{
		MSM5205DataWrite(num, adpcm_data[num] & 0x0f);
		adpcm_data[num] = -1;
	}
	else
	{
		UINT8 *ROM = DrvSndROM + 0x10000 * num;

		adpcm_data[num] = ROM[adpcm_pos[num]++ & 0xffff];
		MSM5205DataWrite(num, adpcm_data[num] >> 4);
	}
}

static void tbowl_vclk_0()
{
	tbowl_adpcm_int(0);
}

static void tbowl_vclk_1()
{
	tbowl_adpcm_int(1);
}

static void DrvFMIRQHandler(int, INT32 nStatus)
{
	ZetSetIRQLine(0, ((nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE));
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 4000000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	bankswitch(0, 0);
	ZetClose();

	ZetOpen(1);
	ZetReset();
	bankswitch(1, 0);
	ZetClose();

	ZetOpen(2);
	ZetReset();
	BurnYM3812Reset();
	MSM5205Reset();
	ZetClose();

	adpcm_pos[0] = adpcm_pos[1] = 0;
	adpcm_end[0] = adpcm_end[1] = 0;
	adpcm_data[0] = adpcm_data[1] = -1;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x020000;
	DrvZ80ROM1	= Next; Next += 0x020000;
	DrvZ80ROM2	= Next; Next += 0x008000;

	DrvGfxROM0	= Next; Next += 0x020000;
	DrvGfxROM1	= Next; Next += 0x100000;
	DrvGfxROM2	= Next; Next += 0x100000;
	DrvGfxROM3	= Next; Next += 0x100000;

	DrvSndROM	= Next; Next += 0x020000;

	DrvPalette	= (UINT32*)Next; Next += 0x00800 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM0	= Next; Next += 0x02000;
	DrvZ80RAM1	= Next; Next += 0x01800;
	DrvZ80RAM2	= Next; Next += 0x00800;

	DrvBgRAM2	= Next; Next += 0x02000;
	DrvBgRAM	= Next; Next += 0x02000;
	DrvTxRAM	= Next; Next += 0x01000;
	DrvShareRAM	= Next; Next += 0x00400;

	DrvPalRAM	= Next; Next += 0x01000;
	DrvSprRAM	= Next; Next += 0x00800;

	DrvBank		= Next; Next += 0x00002;
	DrvScroll	= Next; Next += 0x00008;

	soundlatch	= Next; Next += 0x00001;

	RamEnd		= Next;
	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	UINT8 *tmp = (UINT8*)malloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	static INT32 Planes[4] = { STEP4(0,1) };
	static INT32 XOffs[16] = { STEP8(0,4), STEP8(256,4) };
	static INT32 YOffs[16] = { STEP8(0,32), STEP8(512,32) };

	memcpy (tmp, DrvGfxROM0, 0x10000);

	GfxDecode(0x0800, 4,  8,  8, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x80000);

	GfxDecode(0x1000, 4, 16, 16, Planes, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x80000);

	GfxDecode(0x4000, 4,  8,  8, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM2);

	free (tmp);

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x10000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x00001,  6, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x40001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40000,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00001,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x60001, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x60000, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20001, 13, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000, 14, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x60001, 15, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x60000, 16, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40001, 17, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40000, 18, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20001, 19, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 20, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001, 21, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 22, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x00000, 23, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x10000, 24, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvBgRAM2,		0xa000, 0xbfff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,		0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvTxRAM,		0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xf800, 0xfbff, MAP_RAM);
	ZetSetWriteHandler(tbowl_main_write);
	ZetSetReadHandler(tbowl_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0xc000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,		0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xf800, 0xfbff, MAP_RAM);
	ZetSetWriteHandler(tbowl_sub_write);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(tbowl_sound_write);
	ZetSetReadHandler(tbowl_sound_read);
	ZetClose();

	BurnYM3812Init(2, 4000000, &DrvFMIRQHandler, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3812(4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 0.80, BURN_SND_ROUTE_BOTH);
	BurnYM3812SetRoute(1, BURN_SND_YM3812_ROUTE, 0.80, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, DrvSynchroniseStream, 384000, tbowl_vclk_0, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	MSM5205Init(1, DrvSynchroniseStream, 384000, tbowl_vclk_1, MSM5205_S48_4B, 1);
	MSM5205SetRoute(1, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM5205Exit();
	BurnYM3812Exit();

	ZetExit();

	free (AllMem);
	AllMem = NULL;

	return 0;
}

static inline void DrvRecalcPalette()
{
	UINT8 r,g,b;
	UINT16 *p = (UINT16*)DrvPalRAM;
	for (INT32 i = 0; i < 0x1000 / 2; i++) {
		UINT16 d = (p[i] << 8) | (p[i] >> 8);

		g = (d >> 0) & 0x0f;
		r = (d >> 4) & 0x0f;
		b = (d >> 8) & 0x0f;

		DrvPalette[i] = BurnHighCol((r << 4) | r, (g << 4) | g, (b << 4) | b, 0);
	}
}

static void draw_background_layer(UINT8 *src, INT32 xr, INT32 coloff, UINT8 *scroll)
{
	INT32 scrollx = ((scroll[1] << 8) | scroll[0]) & 0x7ff;
	INT32 scrolly = ((scroll[3] << 8) | scroll[2]) & 0x1ff;

	for (INT32 offs = 0; offs < 128 * 32; offs++) {
		INT32 sx = (offs & 0x7f) << 4;
		INT32 sy = (offs >> 7) << 4;

		sx -= scrollx;
		if (sx < -15) sx += 0x800;
		sy -= scrolly + 16;
		if (sy < -15) sy += 0x200;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr = src[offs + 0x1000];
		INT32 code = src[offs] + ((attr & 0x0f) << 8);
		INT32 color= attr >> 4;

		if (!code) continue;

		Render16x16Tile_Mask_Clip(pTransDraw, code ^ xr, sx, sy, color, 4, 0, coloff, DrvGfxROM1);
	}
}

static void draw_text_layer()
{
	for (INT32 offs = 0x80; offs < 0x780; offs++) {
		INT32 sx = (offs & 0x3f) << 3;
		INT32 sy = (offs >> 6) << 3;

		INT32 attr = DrvTxRAM[offs + 0x800];
		INT32 code = DrvTxRAM[offs] | ((attr & 0x07) << 8);
		INT32 color= attr >> 4;

		if (!code) continue;

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0, 0x100, DrvGfxROM0);
	}
}

static inline INT32 calc_sprite_offset(INT32 code, INT32 x, INT32 y)
{
	INT32 ofst = ((y & 4) << 3) | ((x & 4) << 2) | ((y & 2) << 2) | ((x & 2) << 1) | ((y & 1) << 1) | (x & 1);
	return (ofst + code) & 0x3fff;
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x800; offs+=8)
	{
		if (DrvSprRAM[offs+0] & 0x80)
		{
			INT32 code  = DrvSprRAM[offs + 2] | (DrvSprRAM[offs + 1] << 8);
			INT32 color = DrvSprRAM[offs + 3] & 0x1f;
			INT32 sizex = 1 << ((DrvSprRAM[offs] & 0x03));
			INT32 sizey = 1 << ((DrvSprRAM[offs] & 0x0c) >> 2);

			INT32 flipx = DrvSprRAM[offs] & 0x20;
			INT32 xpos  = DrvSprRAM[offs + 6] | ((DrvSprRAM[offs + 4] & 0x03) << 8);
			INT32 ypos  = DrvSprRAM[offs + 5] | ((DrvSprRAM[offs + 4] & 0x10) << 4);

			for (INT32 y = 0; y < sizey; y++)
			{
				for (INT32 x = 0; x < sizex; x++)
				{
					INT32 sx = xpos + 8 * (flipx ? (sizex - 1 - x) : x);
					INT32 sy = (ypos + 8 * y) - 16;

					INT32 tile = calc_sprite_offset(code, x, y);

					if (flipx) {
						Render8x8Tile_Mask_FlipX_Clip(pTransDraw, tile, sx,         sy,         color, 4, 0, 0, DrvGfxROM2);
						Render8x8Tile_Mask_FlipX_Clip(pTransDraw, tile, sx,         sy - 0x200, color, 4, 0, 0, DrvGfxROM2);
						Render8x8Tile_Mask_FlipX_Clip(pTransDraw, tile, sx - 0x400, sy,         color, 4, 0, 0, DrvGfxROM2);
						Render8x8Tile_Mask_FlipX_Clip(pTransDraw, tile, sx - 0x400, sy - 0x200, color, 4, 0, 0, DrvGfxROM2);
					}
					else
					{
						Render8x8Tile_Mask_Clip(pTransDraw, tile, sx,         sy,         color, 4, 0, 0, DrvGfxROM2);
						Render8x8Tile_Mask_Clip(pTransDraw, tile, sx,         sy - 0x200, color, 4, 0, 0, DrvGfxROM2);
						Render8x8Tile_Mask_Clip(pTransDraw, tile, sx - 0x400, sy,         color, 4, 0, 0, DrvGfxROM2);
						Render8x8Tile_Mask_Clip(pTransDraw, tile, sx - 0x400, sy - 0x200, color, 4, 0, 0, DrvGfxROM2);
					}
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
	}

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x0100;
	}

	draw_background_layer(DrvBgRAM,  0x000, 0x300, DrvScroll + 4);
	draw_sprites();
	draw_background_layer(DrvBgRAM2, 0x400, 0x200, DrvScroll + 0);
	draw_text_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 5);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}
	}

	ZetNewFrame();

	INT32 nSegment;
	INT32 nInterleave = MSM5205CalcInterleave(0, 4000000);
	INT32 nTotalCycles[3] = { 8000000 / 60, 8000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = (nTotalCycles[0] - nCyclesDone[0]) / (nInterleave - i);

		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == (nInterleave-1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		nSegment = (nTotalCycles[1] - nCyclesDone[1]) / (nInterleave - i);

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nSegment);
		if (i == (nInterleave-1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		ZetOpen(2);
		BurnTimerUpdateYM3812((i + 1) * (nTotalCycles[2] / nInterleave));
		MSM5205Update();
		ZetClose();
	}

	ZetOpen(2);

	BurnTimerEndFrameYM3812(nTotalCycles[2]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(1, pBurnSoundOut, nBurnSoundLen);
	}

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
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {		
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		BurnYM3812Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(adpcm_data);
		SCAN_VAR(adpcm_pos);
		SCAN_VAR(adpcm_end);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(0, DrvBank[0]);
		ZetClose();

		ZetOpen(1);
		bankswitch(1, DrvBank[1]);
		ZetClose();
	}

	return 0;
}


// Tecmo Bowl (World?)

static struct BurnRomInfo tbowlRomDesc[] = {
	{ "4.11b",	0x08000, 0xdb8a4f5d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "6206b.5",	0x10000, 0x133c5c11, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "6206c.24",	0x10000, 0x040c8138, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "6206c.25",	0x10000, 0x92c3cef5, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "6206a.1",	0x08000, 0x4370207b, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "14.13l",	0x08000, 0xf9cf60b9, 4 | BRF_GRA },           //  5 Characters
	{ "15.15l",	0x08000, 0xa23f6c53, 4 | BRF_GRA },           //  6

	{ "6206b.6",	0x10000, 0xb9615ffa, 5 | BRF_GRA },           //  7 Tiles
	{ "6206b.8",	0x10000, 0x6389c719, 5 | BRF_GRA },           //  8
	{ "6206b.7",	0x10000, 0xd139c397, 5 | BRF_GRA },           //  9
	{ "6206b.9",	0x10000, 0x975ded4c, 5 | BRF_GRA },           // 10
	{ "6206b.10",	0x10000, 0x9b4fa82e, 5 | BRF_GRA },           // 11
	{ "6206b.12",	0x10000, 0x7d0030f6, 5 | BRF_GRA },           // 12
	{ "6206b.11",	0x10000, 0x06bf07bb, 5 | BRF_GRA },           // 13
	{ "6206b.13",	0x10000, 0x4ad72c16, 5 | BRF_GRA },           // 14

	{ "6206c.16",	0x10000, 0x1a2fb925, 6 | BRF_GRA },           // 15 Sprites
	{ "6206c.20",	0x10000, 0x70bb38a3, 6 | BRF_GRA },           // 16
	{ "6206c.17",	0x10000, 0xde16bc10, 6 | BRF_GRA },           // 17
	{ "6206c.21",	0x10000, 0x41b2a910, 6 | BRF_GRA },           // 18
	{ "6206c.18",	0x10000, 0x0684e188, 6 | BRF_GRA },           // 19
	{ "6206c.22",	0x10000, 0xcf660ebc, 6 | BRF_GRA },           // 20
	{ "6206c.19",	0x10000, 0x71795604, 6 | BRF_GRA },           // 21
	{ "6206c.23",	0x10000, 0x97fba168, 6 | BRF_GRA },           // 22

	{ "6206a.3",	0x10000, 0x3aa24744, 7 | BRF_SND },           // 23 Samples
	{ "6206a.2",	0x10000, 0x1e9e5936, 7 | BRF_SND },           // 24
};

STD_ROM_PICK(tbowl)
STD_ROM_FN(tbowl)

struct BurnDriver BurnDrvTbowl = {
	"tbowl", NULL, NULL, NULL, "1987",
	"Tecmo Bowl (World?)\0", NULL, "Tecmo", "hardware",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, tbowlRomInfo, tbowlRomName, NULL, NULL, TbowlInputInfo, TbowlDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 224, 16, 9
};


// Tecmo Bowl (Japan)

static struct BurnRomInfo tbowljRomDesc[] = {
	{ "6206b.4",	0x08000, 0x7ed3eff7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "6206b.5",	0x10000, 0x133c5c11, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "6206c.24",	0x10000, 0x040c8138, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "6206c.25",	0x10000, 0x92c3cef5, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "6206a.1",	0x08000, 0x4370207b, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "6206b.14",	0x08000, 0xcf99d0bf, 4 | BRF_GRA },           //  5 Characters
	{ "6206b.15",	0x08000, 0xd69248cf, 4 | BRF_GRA },           //  6

	{ "6206b.6",	0x10000, 0xb9615ffa, 5 | BRF_GRA },           //  7 Tiles
	{ "6206b.8",	0x10000, 0x6389c719, 5 | BRF_GRA },           //  8
	{ "6206b.7",	0x10000, 0xd139c397, 5 | BRF_GRA },           //  9
	{ "6206b.9",	0x10000, 0x975ded4c, 5 | BRF_GRA },           // 10
	{ "6206b.10",	0x10000, 0x9b4fa82e, 5 | BRF_GRA },           // 11
	{ "6206b.12",	0x10000, 0x7d0030f6, 5 | BRF_GRA },           // 12
	{ "6206b.11",	0x10000, 0x06bf07bb, 5 | BRF_GRA },           // 13
	{ "6206b.13",	0x10000, 0x4ad72c16, 5 | BRF_GRA },           // 14

	{ "6206c.16",	0x10000, 0x1a2fb925, 6 | BRF_GRA },           // 15 Sprites
	{ "6206c.20",	0x10000, 0x70bb38a3, 6 | BRF_GRA },           // 16
	{ "6206c.17",	0x10000, 0xde16bc10, 6 | BRF_GRA },           // 17
	{ "6206c.21",	0x10000, 0x41b2a910, 6 | BRF_GRA },           // 18
	{ "6206c.18",	0x10000, 0x0684e188, 6 | BRF_GRA },           // 19
	{ "6206c.22",	0x10000, 0xcf660ebc, 6 | BRF_GRA },           // 20
	{ "6206c.19",	0x10000, 0x71795604, 6 | BRF_GRA },           // 21
	{ "6206c.23",	0x10000, 0x97fba168, 6 | BRF_GRA },           // 22

	{ "6206a.3",	0x10000, 0x3aa24744, 7 | BRF_SND },           // 23 Samples
	{ "6206a.2",	0x10000, 0x1e9e5936, 7 | BRF_SND },           // 24
};

STD_ROM_PICK(tbowlj)
STD_ROM_FN(tbowlj)

struct BurnDriver BurnDrvTbowlj = {
	"tbowlj", "tbowl", NULL, NULL, "1987",
	"Tecmo Bowl (Japan)\0", NULL, "Tecmo", "hardware",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, tbowljRomInfo, tbowljRomName, NULL, NULL, TbowlInputInfo, TbowlDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	512, 224, 16, 9
};
