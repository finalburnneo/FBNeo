// FB Alpha Shanghai Kid / Chinese Hero / Dynamic Ski driver module
// Based on MAME driver by Phil Stroffolino

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "dac.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[3];
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *video_regs;

static INT32 soundlatch;
static INT32 bankdata;
static INT32 soundbank;

static INT32 irq[2];
static INT32 nmi[2];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 is_game; // 0 chinhero, 1 shangkid

static struct BurnInputInfo DynamskiInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Dynamski)

static struct BurnInputInfo ShangkidInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Shangkid)

static struct BurnInputInfo ChinheroInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Chinhero)

static struct BurnDIPInfo DynamskiDIPList[]=
{
	DIP_OFFSET(0x0e)
	{0x00, 0xff, 0xff, 0x05, NULL					},

	{0   , 0xfe, 0   ,    3, "Unknown"				},
	{0x00, 0x01, 0x03, 0x01, "A"					},
	{0x00, 0x01, 0x03, 0x02, "B"					},
	{0x00, 0x01, 0x03, 0x03, "C"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x04, 0x04, "Upright"				},
	{0x00, 0x01, 0x04, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x00, 0x01, 0x18, 0x18, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x18, 0x10, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x18, 0x00, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x18, 0x08, "1 Coin  2 Credits"	},
};

STDDIPINFO(Dynamski)

static struct BurnDIPInfo ChinheroDIPList[]=
{
	DIP_OFFSET(0x16)
	{0x00, 0xff, 0xff, 0x01, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x03, 0x01, "3"					},
	{0x00, 0x01, 0x03, 0x02, "4"					},
	{0x00, 0x01, 0x03, 0x03, "5"					},
	{0x00, 0x01, 0x03, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x00, 0x01, 0x04, 0x00, "Off"					},
	{0x00, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x00, 0x01, 0x18, 0x18, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x18, 0x10, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x18, 0x00, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x18, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x20, 0x20, "Off"					},
	{0x00, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0xc0, 0x00, "Easy"					},
	{0x00, 0x01, 0xc0, 0x40, "Medium"				},
	{0x00, 0x01, 0xc0, 0x80, "Hard"					},
	{0x00, 0x01, 0xc0, 0xc0, "Hardest"				},
};

STDDIPINFO(Chinhero)

static struct BurnDIPInfo ShangkidDIPList[]=
{
	DIP_OFFSET(0x12)
	{0x00, 0xff, 0xff, 0x06, NULL					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x02, 0x02, "Upright"				},
	{0x00, 0x01, 0x02, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x00, 0x01, 0x1c, 0x10, "4 Coins 1 Credits"	},
	{0x00, 0x01, 0x1c, 0x0c, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x1c, 0x08, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x1c, 0x00, "1 Coin/1 Credit without coin counter"	},
	{0x00, 0x01, 0x1c, 0x04, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x1c, 0x14, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x1c, 0x18, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x1c, 0x1c, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x20, 0x20, "Off"					},
	{0x00, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0xc0, 0x00, "Easy"					},
	{0x00, 0x01, 0xc0, 0x40, "Medium"				},
	{0x00, 0x01, 0xc0, 0x80, "Hard"					},
	{0x00, 0x01, 0xc0, 0xc0, "Hardest"				},
};

STDDIPINFO(Shangkid)

static void bankswitch(INT32 data)
{
	bankdata = data;

	ZetMapMemory(DrvZ80ROM[0] + 0x8000 + (data & 1) * 0x2000, 0x8000, 0x9fff, MAP_ROM);
}

static void __fastcall chinhero_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			if (nmi[0]) {
				ZetSetIRQLine(0, 0x20, CPU_IRQSTATUS_ACK);
			}
		return;

		case 0xa800:
			if (nmi[1]) {
				ZetSetIRQLine(1, 0x20, CPU_IRQSTATUS_ACK);
			}
		return;

		case 0xb000:
			ZetSetRESETLine(1, (data ? 0 : 1));
		return;

		case 0xb001:
			ZetSetRESETLine(2, (data ? 0 : 1));
		return;

		case 0xb002:
			irq[0] = data;
			if (!data) {
				ZetSetIRQLine(0, 0, CPU_IRQSTATUS_NONE);
			}
		return;

		case 0xb003:
			irq[1] = data;
			if (!data) {
				ZetSetIRQLine(1, 0, CPU_IRQSTATUS_NONE);
			}
		return;

		case 0xb004:
			nmi[0] = data;
			if (!data) {
				ZetSetIRQLine(0, 0x20, CPU_IRQSTATUS_NONE);
			}
		return;

		case 0xb005:
			nmi[1] = data;
			if (!data) {
				ZetSetIRQLine(1, 0x20, CPU_IRQSTATUS_NONE);
			}
		return;

		case 0xb006:
			// nop
		return;

		case 0xb007: // shangkid (cpu#0 only)
			if (is_game == 1) {
				bankswitch(data & 1);
			}
		return;

		case 0xc000:
		case 0xc001:
		case 0xc002:
			video_regs[address & 3] = data;
		return;
	}
}

static UINT8 __fastcall chinhero_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xb800:
			return DrvDips[0];

		case 0xb801:
			return DrvInputs[0] | ((is_game == 1) ? 0x80 : 0x00); // sys -shangkid needs this bit set

		case 0xb802:
			return DrvInputs[2]; // p2

		case 0xb803:
			return DrvInputs[1]; // p1
	}

	return 0;
}

static void __fastcall chinhero_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			AY8910Write(0, port & 1, data);
		return;
	}
}

static void __fastcall dynamski_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			irq[0] = data;
			if (!data) {
				ZetSetIRQLine(0, 0, CPU_IRQSTATUS_NONE);
			}
		return;

		case 0xe001:
		case 0xe002: // flipsc?
			video_regs[address - 0xe001] = data;
			return;
	}
}

static UINT8 __fastcall dynamski_read(UINT16 address)
{
	switch (address)
	{
		case 0xe800:
			return DrvInputs[0]; // sys

		case 0xe801:
			return DrvInputs[1]; // p1

		case 0xe802:
			return DrvInputs[2]; // p2

		case 0xe803:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall dynamski_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			AY8910Write(0, ~port & 1, data);
		return;
	}
}

static void __fastcall chinhero_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0:
			DACSignedWrite(0, data);
		return;
	}
}

static UINT8 __fastcall chinhero_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0:
			ZetSetIRQLine(2, 0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static void sound_bankswitch(INT32 data)
{
	soundbank = data;

	ZetMapMemory(DrvZ80ROM[2] + (soundbank * 0x10000), 0x0000, 0xdfff, MAP_ROM);
}

static void ay8910_portA_write(UINT32 /*addr*/, UINT32 data)
{
	if (data & 1) ZetSetIRQLine(2, 0, CPU_IRQSTATUS_HOLD);

	if (is_game == 1) { // shangkid
		ZetCPUPush(2);
		sound_bankswitch((data & ~1) ? 0 : 1);
		ZetCPUPop();
	}
}

static void ay8910_portB_write(UINT32 /*addr*/, UINT32 data)
{
	soundlatch = data;
}

static tilemap_callback( skbg )
{
	INT32 attr  = DrvVidRAM[offs + 0x800];
	INT32 code  = DrvVidRAM[offs] + attr * 256;
	INT32 color = ((attr >> 3) & 3) | ((attr >> 2) & 0x38);
	INT32 group = TILE_GROUP((DrvColPROM[0x800 + color * 4] == 2) ? 1 : 0);

	TILE_SET_INFO(0, code, color, ((attr & 0x04) ? TILE_FLIPX : 0) | group);
}

static tilemap_callback( chbg )
{
	INT32 attr  = DrvVidRAM[offs + 0x800];
	INT32 code  = DrvVidRAM[offs] + attr * 256;
	INT32 color = (attr >> 2) & 0x1f;
	INT32 group = TILE_GROUP((DrvColPROM[0x800 + color * 4] == 2) ? 1 : 0);

	TILE_SET_INFO(0, code, color, ((attr & 0x80) ? TILE_FLIPX : 0) | group);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetSetRESETLine(1, 1);
	ZetSetRESETLine(2, 1);

	AY8910Reset(0);
	DACReset();

	soundlatch = 0;

	irq[0] = irq[1] = 0;
	nmi[0] = nmi[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x010000;
	DrvZ80ROM[1]		= Next; Next += 0x010000;
	DrvZ80ROM[2]		= Next; Next += 0x020000;

	DrvGfxROM0			= Next; Next += 0x010000;
	DrvGfxROM1			= Next; Next += 0x060000;
	DrvGfxROM2			= Next; Next += 0x040000; // temp

	DrvColPROM			= Next; Next += 0x000b00;

	DrvPalette			= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam				= Next;

	DrvShareRAM			= Next; Next += 0x002e00;
	DrvVidRAM			= Next; Next += 0x001000;
	DrvSprRAM			= Next; Next += 0x001000; // 0x200 shangkid,chinhero, 0xc00 dynamski
	DrvZ80RAM			= Next; Next += 0x001000;

	video_regs			= Next; Next += 0x000004;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode(UINT8 *src, UINT8 *dst, INT32 len, INT32 num, INT32 type)
{
	INT32 Plane[4][3] = { { 0, 4 }, { 0, 4 }, { 0x8000*8+4, 0, 4 }, { 0x8000*8, 0x4000*8+0, 0x4000*8+4 } };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(8,1), STEP4(128,1), STEP4(128+8,1) };
	INT32 YOffs[16] = { STEP8(0,16), STEP8(256,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < len; i++) tmp[i] = src[i] ^ 0xff;

	GfxDecode(num, (type < 2) ? 2 : 3, (type > 0) ? 16 : 8, (type > 0) ? 16 : 8, Plane[type], XOffs, YOffs, (type > 0) ? 0x200 : 0x80, tmp, dst);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvGfxDecodeCH(UINT8 *src, UINT8 *dst, INT32 num)
{
	INT32 Plane[2][3] = { { 0x4000*8+4, 0, 4 }, { 0x4000*8, 0x2000*8+0, 0x2000*8+4 } };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(8,1), STEP4(128,1), STEP4(128+8,1) };
	INT32 YOffs[16] = { STEP8(0,16), STEP8(256,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x20000; i++) tmp[i] = src[i] ^ 0xff;

	GfxDecode(num, 3, 16, 16, Plane[0], XOffs, YOffs, 0x200, tmp + 0x00000, dst + (16*16*num*0));
	GfxDecode(num, 3, 16, 16, Plane[1], XOffs, YOffs, 0x200, tmp + 0x00000, dst + (16*16*num*2));

	GfxDecode(num, 3, 16, 16, Plane[0], XOffs, YOffs, 0x200, tmp + 0x10000, dst + (16*16*num*1));
	GfxDecode(num, 3, 16, 16, Plane[1], XOffs, YOffs, 0x200, tmp + 0x10000, dst + (16*16*num*3));

	BurnFree(tmp);

	return 0;
}

static void maincpu_init(INT32 cpu)
{
	ZetInit(cpu);
	ZetOpen(cpu);
	ZetMapMemory(DrvZ80ROM[cpu],	0x0000, 0x9fff, MAP_ROM);
	// 8000-9fff banked in shangkid (main cpu)
	ZetMapMemory(DrvVidRAM,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xe000, 0xfdff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xfe00, 0xffff, MAP_RAM);
	ZetSetWriteHandler(chinhero_main_write);
	ZetSetReadHandler(chinhero_main_read);
	if (cpu == 1) {
		ZetSetOutHandler(chinhero_main_write_port);
	}
	ZetClose();
}

static INT32 DrvInit(INT32 game, INT32 load_type)
{
	BurnAllocMemIndex();

	GenericTilesInit();

	is_game = game;

	INT32 k = 0;
	if (game == 0 && load_type == 0) // chinhero
	{
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x06000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x02000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, k++, 1)) return 1; // load temp
		if (BurnLoadRom(DrvGfxROM2 + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x12000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x14000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00100, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00200, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00300, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00400, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00600, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00800, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00900, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00a00, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00a20, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00a40, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00a60, k++, 1)) return 1;
	}
	else if (game == 0 && load_type == 1) // chinhero
	{
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000, k++, 1)) return 1; // load temp
		if (BurnLoadRom(DrvGfxROM2 + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x12000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x14000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00300, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00400, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00600, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00800, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00900, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00a00, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00a20, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00a40, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00a60, k++, 1)) return 1;
	}
	else if (game == 1 && load_type == 0) // shangkid
	{
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x0a000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x14000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x1c000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x14000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00300, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00400, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00600, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00800, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00900, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00a00, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00a20, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00a40, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00a60, k++, 1)) return 1;
	}
	else if (game == 1 && load_type == 1)
	{
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x0a000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x14000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[2] + 0x1c000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x14000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00300, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00400, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00600, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00800, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00900, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00a00, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00a20, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00a40, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00a60, k++, 1)) return 1;
	}

	if (game == 0) { // chinhero
		DrvGfxDecode(DrvGfxROM0 + 0x000000, DrvGfxROM0, 0x4000, 0x400, 0);
		DrvGfxDecodeCH(DrvGfxROM2 + 0x00000, DrvGfxROM1, 0x80);

		GenericTilemapInit(0, TILEMAP_SCAN_ROWS, chbg_map_callback, 8, 8, 64, 32);
		GenericTilemapSetGfx(0, DrvGfxROM0, 2,  8,  8, 0x10000, 0, 0x3f);
		GenericTilemapSetGfx(1, DrvGfxROM1, 3, 16, 16, 0x8000*4, 0, 0x1f);
		GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	} else if (game == 1) { // shangkid
		DrvGfxDecode(DrvGfxROM0 + 0x000000, DrvGfxROM0, 0x04000, 0x400, 0);
		DrvGfxDecode(DrvGfxROM1 + 0x000000, DrvGfxROM1, 0x18000, 0x600, 1);

		GenericTilemapInit(0, TILEMAP_SCAN_ROWS, skbg_map_callback, 8, 8, 64, 32);
		GenericTilemapSetGfx(0, DrvGfxROM0, 2,  8,  8, 0x10000, 0, 0x3f);
		GenericTilemapSetGfx(1, DrvGfxROM1, 2, 16, 16, 0x60000, 0, 0x3f);
		GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	}

	maincpu_init(0);
	maincpu_init(1);

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM[2],	0x0000, 0xdfff, MAP_ROM); // banked on shangkid
	ZetMapMemory(DrvZ80RAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0xe800, 0xefff, MAP_RAM); // mirror
	ZetSetOutHandler(chinhero_sound_write_port);
	ZetSetInHandler(chinhero_sound_read_port);
	ZetClose();

	AY8910Init(0, 1536000, 0);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetPorts(0, NULL, NULL, ay8910_portA_write, ay8910_portB_write);
	AY8910SetBuffered(ZetTotalCycles, 3072000);

	DACInit(0, 0, 1, ZetTotalCycles, 3072000);
	DACSetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

static INT32 DynamskiInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x01000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x03000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x05000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x06000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x07000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x04000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00040, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00140, k++, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0 + 0x000000, DrvGfxROM0, 0x04000, 0x400, 0);
		DrvGfxDecode(DrvGfxROM1 + 0x000000, DrvGfxROM1, 0x06000, 0x180, 1);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,			0xc000, 0xcbff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xd000, 0xdbff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,			0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(dynamski_write);
	ZetSetReadHandler(dynamski_read);
	ZetSetOutHandler(dynamski_main_write_port);
	ZetClose();

	ZetInit(1); // not on this hardware
	ZetInit(2); // not on this hardware

	AY8910Init(0, 2000000, 0);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3000000);

	DACInit(0, 0, 1, ZetTotalCycles, 3000000); // not on this hardware
	DACSetRoute(0, 0.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	DACExit();

	BurnFreeMemIndex();

	is_game = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 2*0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 2*0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 2*0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 2*0x100] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprite(INT32 sprite_type)
{
	GenericTilesGfx *gfx = &GenericGfxData[1];

	INT32 transparent_pen = (1 << gfx->depth) - 1;
	INT32 color_shift = gfx->depth - 2;

	UINT8 *finish = DrvSprRAM;
	UINT8 *source = DrvSprRAM + 0x200;

	while (source > finish) {
		source -= 8;

		INT32 ypos        = 209 - source[0];
		INT32 tile        = source[1]&0x3f;
		INT32 xflip       = (source[1]&0x40) >> 6;
		INT32 yflip       = (source[1]&0x80) >> 7;
		INT32 bank        = source[2]&0x3f;
		INT32 xsize       = (source[2]&0x40) >> 6;
		INT32 ysize       = (source[2]&0x80) >> 7;
		INT32 height      = ((source[3]&0x07) + 1) << 1;
		INT32 xpos        = ((source[4]+source[5]*0x100)&0x1ff)-23;
		INT32 color       = (source[6]&0x3f) >> color_shift;
		INT32 width       = ((source[7]&0x07) + 1) << 1;

		if (sprite_type == 1) {
			INT32 t[4] = { bank & 0xf, bank & 0xf, 0x10 | (bank & 0x3), 0x14 | (bank & 0x3) };

			tile += t[bank >> 4 & 0x3] * 0x40;
		} else {
			if (color == 0) continue;
			tile += (bank >> 4) * 0x80 + (bank & 0x01) * 0x40;
		}

		color = ((color & gfx->color_mask) << gfx->depth) + gfx->color_offset;

		if (xsize == 0 && xflip) xpos -= 16;
		if (ysize == 0 && yflip == 0) ypos += 16;

		xpos += (16-width)*(xsize+1)/2;
		ypos += (16-height)*(ysize+1)/2;

		for (INT32 r = 0; r <= ysize; r++) {
			for (INT32 c = 0; c <= xsize; c++) {
				INT32 sx = xpos+(c^xflip)*width;
				INT32 sy = ypos+(r^yflip)*height;

				RenderZoomedTile(pTransDraw, gfx->gfxbase, (tile+c*8+r) % gfx->code_mask, color, transparent_pen, sx - 16, sy, xflip, yflip, gfx->width, gfx->height, (width<<16)/16, (height<<16)/16);
				RenderZoomedTile(pTransDraw, gfx->gfxbase, (tile+c*8+r) % gfx->code_mask, color, transparent_pen, sx - 16, sy + 256, xflip, yflip, gfx->width, gfx->height, (width<<16)/16, (height<<16)/16);
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

	BurnTransferClear(0);

	INT32 flipscreen = video_regs[1] & 0x80;

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? (TMAP_FLIPX | TMAP_FLIPY) : 0);
	GenericTilemapSetScrollX(0, video_regs[0] - 24);
	GenericTilemapSetScrollY(0, video_regs[2]);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprite(is_game);

	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));

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
		DrvInputs[0] = 0;
		DrvInputs[1] = 0;
		DrvInputs[2] = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] =  { 3072000 / 60, 3072000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 223 && irq[0]) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == 223 && irq[1]) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		ZetClose();

		ZetOpen(2);
		CPU_RUN(2, Zet);
		ZetClose();

		if (i == 223 && pBurnDraw) {
			BurnDrvRedraw();
		}
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static void DynamskiPaletteInit()
{
	UINT32 pal[32];

	for (int i = 0; i < 32; i++)
	{
		UINT32 data = (DrvColPROM[i | 0x20] << 8) | DrvColPROM[i];

		UINT8 r = (data >> 1) & 0x1f;
		UINT8 g = (data >> 6) & 0x1f;
		UINT8 b = (data >> 11) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x40; i++)
	{
		DrvPalette[0x00 + i] = pal[(DrvColPROM[i + 0x040] & 0x0f)];
		DrvPalette[0x40 + i] = pal[(DrvColPROM[i + 0x140] & 0x0f) | 0x10];
	}
}

static void dynamski_draw_background(INT32 pri)
{
	for (INT32 i = 0; i < 0x400; i++)
	{
		INT32 sx = (i & 0x1f) * 8;
		INT32 sy = (i / 0x20) * 8;
		INT32 code  = DrvVidRAM[i];
		INT32 color = DrvVidRAM[i + 0x400];

		if (sy < 16)
		{
			INT32 temp = sx;
			sx = sy+256+16;
			sy = temp;
		}
		else if (sy >= (256-16))
		{
			INT32 temp = sx;
			sx = sy-256+16;
			sy = temp;
		}
		else
		{
			sx+=16;
		}

		if (pri == 0 || (color >> 7) == pri)
		{
			code += (color & 0x60) << 3;

			Draw8x8MaskTile(pTransDraw, code, sx, sy - 16, 0, 0, color & 0xf, 2, pri ? 3 : -1, 0, DrvGfxROM0);
		}
	}
}

static void dynamski_draw_sprites()
{
	for (INT32 i = 0x7e; i >= 0x00; i -= 2)	{
		INT32 bank = DrvSprRAM[0xb80 + i];
		INT32 attr = DrvSprRAM[0xb81 + i];

		INT32 code = (DrvVidRAM[0xb80 + i] & 0x3f) + (bank * 0x40);
		INT32 color = DrvVidRAM[0xb81 + i] & 0xf;
		INT32 flipx = DrvVidRAM[0xb80 + i] & 0x80;
		INT32 flipy = DrvVidRAM[0xb80 + i] & 0x40;
		INT32 sy = 240-DrvSprRAM[0x380 + i];
		INT32 sx = (DrvSprRAM[0x381 + i] - 64 + 8 + 16) + ((attr & 1) * 0x100);

		Draw16x16MaskTile(pTransDraw, code % 0x180, sx, sy - 16, flipx, flipy, color, 2, 3, 0x40, DrvGfxROM1);
	}
}
static INT32 DynamskiDraw()
{
	if (DrvRecalc) {
		DynamskiPaletteInit();
		DrvRecalc = 1;
	}

	BurnTransferClear(0);

	if (nBurnLayer & 1) dynamski_draw_background(0);
	if (nSpriteEnable & 1) dynamski_draw_sprites();
	if (nBurnLayer & 2) dynamski_draw_background(1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DynamskiFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		DrvInputs[0] = 0;
		DrvInputs[1] = 0;
		DrvInputs[2] = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] =  { 3000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 223 && irq[0]) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		ZetClose();

		if (i == 223 && pBurnDraw) {
			BurnDrvRedraw();
		}
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
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

		AY8910Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(irq);
		SCAN_VAR(nmi);
		SCAN_VAR(soundlatch);

		if (is_game == 1) { // shangkid
			SCAN_VAR(bankdata);
			SCAN_VAR(soundbank);
		}
	}

	if (nAction & ACB_WRITE)
	{
		if (is_game == 1) { // shangkid
			ZetOpen(0);
			bankswitch(bankdata);
			ZetClose();

			ZetOpen(2);
			sound_bankswitch(soundbank);
			ZetClose();
		}
	}

	return 0;
}

// Chinese Hero

static struct BurnRomInfo chinheroRomDesc[] = {
	{ "ic2.1",				0x2000, 0x8974bac4, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "ic3.2",				0x2000, 0x9b7a02fe, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ic4.3",				0x2000, 0xe86d4195, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ic5.4",				0x2000, 0x2b629d2c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ic6.5",				0x2000, 0x35bf4a4f, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ic31.6",				0x2000, 0x7c56927b, 2 | BRF_PRG | BRF_ESS }, //  5 Sub Z80 COde
	{ "ic32.7",				0x2000, 0xd67b8045, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "ic47.8",				0x2000, 0x3c396062, 3 | BRF_PRG | BRF_ESS }, //  7 Sound Z80 Code
	{ "ic48.9",				0x2000, 0xb14f2bab, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "ic49.10",			0x2000, 0x8c0e43d1, 3 | BRF_PRG | BRF_ESS }, //  9

	{ "ic21.11",			0x2000, 0x3a37fb45, 4 | BRF_GRA },           // 10 Background Tiles
	{ "ic22.12",			0x2000, 0xbc21c002, 4 | BRF_GRA },           // 11

	{ "ic114.18",			0x2000, 0xfc4183a8, 5 | BRF_GRA },           // 12 Sprites
	{ "ic113.17",			0x2000, 0xd713d7fe, 5 | BRF_GRA },           // 13
	{ "ic99.13",			0x2000, 0xa8e2a3f4, 5 | BRF_GRA },           // 14
	{ "ic112.16",			0x2000, 0xdd5170ca, 6 | BRF_GRA },           // 15
	{ "ic111.15",			0x2000, 0x20f6052e, 6 | BRF_GRA },           // 16
	{ "ic110.14",			0x2000, 0x9bc2d568, 6 | BRF_GRA },           // 17

	{ "v_ic36_r",			0x0100, 0x16ae1692, 7 | BRF_GRA },           // 18 PROMs
	{ "v_ic35_g",			0x0100, 0xb3d0a074, 7 | BRF_GRA },           // 19
	{ "v_ic27_b",			0x0100, 0x353a2d11, 7 | BRF_GRA },           // 20
	{ "v_ic28_m",			0x0100, 0x7ca273c1, 7 | BRF_GRA },           // 21
	{ "v_ic69",				0x0200, 0x410d6f86, 7 | BRF_GRA },           // 22
	{ "v_ic108",			0x0200, 0xd33c02ae, 7 | BRF_GRA },           // 23
	{ "v_ic12",				0x0100, 0x0de07e89, 7 | BRF_GRA },           // 24
	{ "v_ic15_p",			0x0100, 0x7e0a0581, 7 | BRF_GRA },           // 25
	{ "v_ic8",				0x0020, 0x4c62974d, 7 | BRF_GRA },           // 26
	{ "ic8",				0x0020, 0x84bcd9af, 7 | BRF_GRA },           // 27
	{ "ic22",				0x0020, 0x84bcd9af, 7 | BRF_GRA },           // 28
	{ "ic42",				0x0020, 0x2ccfe10a, 7 | BRF_GRA },           // 29
};

STD_ROM_PICK(chinhero)
STD_ROM_FN(chinhero)

static INT32 ChinheroInit()
{
	return DrvInit(0, 0);
}

struct BurnDriver BurnDrvChinhero = {
	"chinhero", NULL, NULL, NULL, "1984",
	"Chinese Hero\0", NULL, "Taiyo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, chinheroRomInfo, chinheroRomName, NULL, NULL, NULL, NULL, ChinheroInputInfo, ChinheroDIPInfo,
	ChinheroInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Chinese Hero (older, set 1)

static struct BurnRomInfo chinhero2RomDesc[] = {
	{ "1.128",				0x4000, 0x68e247aa, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "2.128",				0x4000, 0x0346d8c9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.128",				0x4000, 0xa78b8d78, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "4.128",				0x4000, 0x6ab2e836, 2 | BRF_PRG | BRF_ESS }, //  3 Sub Z80 Code

	{ "5.128",				0x4000, 0x4e4f3f92, 3 | BRF_PRG | BRF_ESS }, //  4 Sound Z80 Code
	{ "ic49.10",			0x2000, 0x8c0e43d1, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "ic21.11",			0x2000, 0x3a37fb45, 4 | BRF_GRA },           //  6 Background Tiles
	{ "ic22.12",			0x2000, 0xbc21c002, 4 | BRF_GRA },           //  7

	{ "ic114.18",			0x2000, 0xfc4183a8, 5 | BRF_GRA },           //  8 Sprites
	{ "ic113.17",			0x2000, 0xd713d7fe, 5 | BRF_GRA },           //  9
	{ "ic99.13",			0x2000, 0xa8e2a3f4, 5 | BRF_GRA },           // 10
	{ "ic112.16",			0x2000, 0xdd5170ca, 6 | BRF_GRA },           // 11
	{ "ic111.15",			0x2000, 0x20f6052e, 6 | BRF_GRA },           // 12
	{ "ic110.14",			0x2000, 0x9bc2d568, 6 | BRF_GRA },           // 13

	{ "v_ic36_r",			0x0100, 0x16ae1692, 7 | BRF_GRA },           // 14 PROMs
	{ "v_ic35_g",			0x0100, 0xb3d0a074, 7 | BRF_GRA },           // 15
	{ "v_ic27_b",			0x0100, 0x353a2d11, 7 | BRF_GRA },           // 16
	{ "v_ic28_m",			0x0100, 0x7ca273c1, 7 | BRF_GRA },           // 17
	{ "v_ic69",				0x0200, 0x410d6f86, 7 | BRF_GRA },           // 18
	{ "v_ic108",			0x0200, 0xd33c02ae, 7 | BRF_GRA },           // 19
	{ "v_ic12",				0x0100, 0x0de07e89, 7 | BRF_GRA },           // 20
	{ "v_ic15_p",			0x0100, 0x7e0a0581, 7 | BRF_GRA },           // 21
	{ "v_ic8",				0x0020, 0x4c62974d, 7 | BRF_GRA },           // 22
	{ "ic8",				0x0020, 0x84bcd9af, 7 | BRF_GRA },           // 23
	{ "ic22",				0x0020, 0x84bcd9af, 7 | BRF_GRA },           // 24
	{ "ic42",				0x0020, 0x2ccfe10a, 7 | BRF_GRA },           // 25
};

STD_ROM_PICK(chinhero2)
STD_ROM_FN(chinhero2)

static INT32 Chinhero2Init()
{
	return DrvInit(0, 1);
}

struct BurnDriver BurnDrvChinhero2 = {
	"chinhero2", "chinhero", NULL, NULL, "1984",
	"Chinese Hero (older, set 1)\0", NULL, "Taiyo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, chinhero2RomInfo, chinhero2RomName, NULL, NULL, NULL, NULL, ChinheroInputInfo, ChinheroDIPInfo,
	Chinhero2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Chinese Hero (older, set 2)

static struct BurnRomInfo chinhero3RomDesc[] = {
	{ "1-11-22.ic2",		0x4000, 0x9b24f886, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "2-11-22.ic3",		0x4000, 0x39c66686, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3-11-22.ic4",		0x2000, 0x2d51135e, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "4-11-22.ic31",		0x4000, 0x6ab2e836, 2 | BRF_PRG | BRF_ESS }, //  3 Sub Z80 Code

	{ "5.128",				0x4000, 0x4e4f3f92, 3 | BRF_PRG | BRF_ESS }, //  4 Sound Z80 Code
	{ "ic49.10",			0x2000, 0x8c0e43d1, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "ic21.11",			0x2000, 0x3a37fb45, 4 | BRF_GRA },           //  6 Background Tiles
	{ "ic22.12",			0x2000, 0xbc21c002, 4 | BRF_GRA },           //  7

	{ "ic114.18",			0x2000, 0xfc4183a8, 5 | BRF_GRA },           //  8 Sprites
	{ "ic113.17",			0x2000, 0xd713d7fe, 5 | BRF_GRA },           //  9
	{ "ic99.13",			0x2000, 0xa8e2a3f4, 5 | BRF_GRA },           // 10
	{ "ic112.16",			0x2000, 0xdd5170ca, 6 | BRF_GRA },           // 11
	{ "ic111.15",			0x2000, 0x20f6052e, 6 | BRF_GRA },           // 12
	{ "ic110.14",			0x2000, 0x9bc2d568, 6 | BRF_GRA },           // 13

	{ "v_ic36_r",			0x0100, 0x16ae1692, 7 | BRF_GRA },           // 14 PROMs
	{ "v_ic35_g",			0x0100, 0xb3d0a074, 7 | BRF_GRA },           // 15
	{ "v_ic27_b",			0x0100, 0x353a2d11, 7 | BRF_GRA },           // 16
	{ "v_ic28_m",			0x0100, 0x7ca273c1, 7 | BRF_GRA },           // 17
	{ "v_ic69",				0x0200, 0x410d6f86, 7 | BRF_GRA },           // 18
	{ "v_ic108",			0x0200, 0xd33c02ae, 7 | BRF_GRA },           // 19
	{ "v_ic12",				0x0100, 0x0de07e89, 7 | BRF_GRA },           // 20
	{ "v_ic15_p",			0x0100, 0x7e0a0581, 7 | BRF_GRA },           // 21
	{ "v_ic8",				0x0020, 0x4c62974d, 7 | BRF_GRA },           // 22
	{ "ic8",				0x0020, 0x84bcd9af, 7 | BRF_GRA },           // 23
	{ "ic22",				0x0020, 0x84bcd9af, 7 | BRF_GRA },           // 24
	{ "ic42",				0x0020, 0x2ccfe10a, 7 | BRF_GRA },           // 25
};

STD_ROM_PICK(chinhero3)
STD_ROM_FN(chinhero3)

struct BurnDriver BurnDrvChinhero3 = {
	"chinhero3", "chinhero", NULL, NULL, "1984",
	"Chinese Hero (older, set 2)\0", NULL, "Taiyo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, chinhero3RomInfo, chinhero3RomName, NULL, NULL, NULL, NULL, ChinheroInputInfo, ChinheroDIPInfo,
	Chinhero2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Chinese Heroe (Taito)

static struct BurnRomInfo chinherotRomDesc[] = {
	{ "chtaito1.bin",		0x4000, 0x2bd64de0, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "chtaito2.bin",		0x4000, 0x8fd04da9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "chtaito3.bin",		0x2000, 0xcd6908e7, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "chtaito4.bin",		0x4000, 0x2a012614, 2 | BRF_PRG | BRF_ESS }, //  3 Sub Z80 Code

	{ "chtaito5.bin",		0x4000, 0x4e4f3f92, 3 | BRF_PRG | BRF_ESS }, //  4 Sound Z80 Code
	{ "chtaito6.bin",		0x2000, 0x8c0e43d1, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "chtaito11.bin",		0x2000, 0x3a37fb45, 4 | BRF_GRA },           //  6 Background Tiles
	{ "chtaito12.bin",		0x2000, 0xbc21c002, 4 | BRF_GRA },           //  7

	{ "chtaito18.bin",		0x2000, 0xfc4183a8, 5 | BRF_GRA },           //  8 Sprites
	{ "chtaito17.bin",		0x2000, 0xd713d7fe, 5 | BRF_GRA },           //  9
	{ "chtaito13.bin",		0x2000, 0xa8e2a3f4, 5 | BRF_GRA },           // 10
	{ "chtaito16.bin",		0x2000, 0xdd5170ca, 6 | BRF_GRA },           // 11
	{ "chtaito15.bin",		0x2000, 0x20f6052e, 6 | BRF_GRA },           // 12
	{ "chtaito14.bin",		0x2000, 0x9bc2d568, 6 | BRF_GRA },           // 13

	{ "v_ic36_r",			0x0100, 0x16ae1692, 7 | BRF_GRA },           // 14 PROMs
	{ "v_ic35_g",			0x0100, 0xb3d0a074, 7 | BRF_GRA },           // 15
	{ "v_ic27_b",			0x0100, 0x353a2d11, 7 | BRF_GRA },           // 16
	{ "v_ic28_m",			0x0100, 0x7ca273c1, 7 | BRF_GRA },           // 17
	{ "v_ic69",				0x0200, 0x410d6f86, 7 | BRF_GRA },           // 18
	{ "v_ic108",			0x0200, 0xd33c02ae, 7 | BRF_GRA },           // 19
	{ "v_ic12",				0x0100, 0x0de07e89, 7 | BRF_GRA },           // 20
	{ "v_ic15_p",			0x0100, 0x7e0a0581, 7 | BRF_GRA },           // 21
	{ "v_ic8",				0x0020, 0x4c62974d, 7 | BRF_GRA },           // 22
	{ "ic8",				0x0020, 0x84bcd9af, 7 | BRF_GRA },           // 23
	{ "ic22",				0x0020, 0x84bcd9af, 7 | BRF_GRA },           // 24
	{ "ic42",				0x0020, 0x2ccfe10a, 7 | BRF_GRA },           // 25
};

STD_ROM_PICK(chinherot)
STD_ROM_FN(chinherot)

struct BurnDriver BurnDrvChinherot = {
	"chinherot", "chinhero", NULL, NULL, "1984",
	"Chinese Heroe (Taito)\0", NULL, "Taiyo (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, chinherotRomInfo, chinherotRomName, NULL, NULL, NULL, NULL, ChinheroInputInfo, ChinheroDIPInfo,
	Chinhero2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	224, 288, 3, 4
};


// Shanghai Kid

static struct BurnRomInfo shangkidRomDesc[] = {
	{ "cr00ic02.bin",		0x4000, 0x2e420377, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "cr01ic03.bin",		0x4000, 0x161cd358, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cr02ic04.bin",		0x2000, 0x85b6e455, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cr03ic05.bin",		0x2000, 0x3b383863, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bbx.bin",			0x2000, 0x560c0abd, 2 | BRF_PRG | BRF_ESS }, //  4 Sub Z80 Code
	{ "cr04ic31.bin",		0x2000, 0xcb207885, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "cr05ic32.bin",		0x4000, 0xcf3b8d55, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "cr06ic33.bin",		0x2000, 0x0f3bdbd8, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "cr11ic51.bin",		0x4000, 0x2e2d6afe, 3 | BRF_PRG | BRF_ESS }, //  8 Sound Z80 Code
	{ "cr12ic43.bin",		0x4000, 0xdd29a0c8, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "cr13ic44.bin",		0x4000, 0x879d0de0, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "cr07ic47.bin",		0x4000, 0x20540f7c, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "cr08ic48.bin",		0x2000, 0x392f24db, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "cr09ic49.bin",		0x4000, 0xd50c96a8, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "cr10ic50.bin",		0x2000, 0x873a5f2d, 3 | BRF_PRG | BRF_ESS }, // 14

	{ "cr20ic21.bin",		0x2000, 0xeb3cbb11, 4 | BRF_GRA },           // 15 Background Tiles
	{ "cr21ic22.bin",		0x2000, 0x7c6e75f4, 4 | BRF_GRA },           // 16

	{ "cr14i114.bin",		0x4000, 0xee1f348f, 5 | BRF_GRA },           // 17 Sprites
	{ "cr15i113.bin",		0x4000, 0xa46398bd, 5 | BRF_GRA },           // 18
	{ "cr16i112.bin",		0x4000, 0xcbed446c, 5 | BRF_GRA },           // 19
	{ "cr17i111.bin",		0x4000, 0xb0a44330, 5 | BRF_GRA },           // 20
	{ "cr18ic99.bin",		0x4000, 0xff7efd7c, 5 | BRF_GRA },           // 21
	{ "cr19i100.bin",		0x4000, 0xf948f829, 5 | BRF_GRA },           // 22

	{ "cr31ic36.bin",		0x0100, 0x9439590b, 6 | BRF_GRA },           // 23 PROMs
	{ "cr30ic35.bin",		0x0100, 0x324e295e, 6 | BRF_GRA },           // 24
	{ "cr28ic27.bin",		0x0100, 0x375cba96, 6 | BRF_GRA },           // 25
	{ "cr29ic28.bin",		0x0100, 0x7ca273c1, 6 | BRF_GRA },           // 26
	{ "cr32ic69.bin",		0x0200, 0x410d6f86, 6 | BRF_GRA },           // 27
	{ "cr33-108.bin",		0x0200, 0xd33c02ae, 6 | BRF_GRA },           // 28
	{ "cr26ic12.bin",		0x0100, 0x85b5e958, 6 | BRF_GRA },           // 29
	{ "cr27ic15.bin",		0x0100, 0xf7a19fe2, 6 | BRF_GRA },           // 30
	{ "cr25ic8.bin",		0x0020, 0xc85e09ad, 6 | BRF_GRA },           // 31
	{ "cr22ic8.bin",		0x0020, 0x1a7e0b06, 6 | BRF_GRA },           // 32
	{ "cr23ic22.bin",		0x0020, 0xefb5f265, 6 | BRF_GRA },           // 33
	{ "cr24ic42.bin",		0x0020, 0x823878aa, 6 | BRF_GRA },           // 34
};

STD_ROM_PICK(shangkid)
STD_ROM_FN(shangkid)

static INT32 ShangkidInit()
{
	return DrvInit(1, 0);
}

struct BurnDriver BurnDrvShangkid = {
	"shangkid", NULL, NULL, NULL, "1985",
	"Shanghai Kid\0", NULL, "Taiyo (Data East license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VSFIGHT, 0,
	NULL, shangkidRomInfo, shangkidRomName, NULL, NULL, NULL, NULL, ShangkidInputInfo, ShangkidDIPInfo,
	ShangkidInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	288, 224, 4, 3
};


// Hokuha Syourin Hiryu no Ken

static struct BurnRomInfo hiryukenRomDesc[] = {
	{ "1.2",				0x4000, 0xc7af7f2e, 1 | BRF_PRG | BRF_ESS }, //  0 Main Z80 Code
	{ "2.3",				0x4000, 0x639afdb3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.4",				0x2000, 0xad210482, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.5",				0x2000, 0x6518943a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bbxj.bin",			0x2000, 0x8def4aaf, 2 | BRF_GRA },           //  4 Sub Z80 Code
	{ "5.31",				0x2000, 0x8ae37ce7, 2 | BRF_GRA },           //  5
	{ "6.32",				0x4000, 0xe835bb7f, 2 | BRF_GRA },           //  6
	{ "7.33",				0x2000, 0x3745ed36, 2 | BRF_GRA },           //  7

	{ "cr11ic51.bin",		0x4000, 0x2e2d6afe, 3 | BRF_PRG | BRF_ESS }, //  8 Sound Z80 Code
	{ "cr07ic47.bin",		0x4000, 0x20540f7c, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "9.48",				0x4000, 0x8da23cad, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "10.49",				0x4000, 0x52b82fee, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "cr10ic50.bin",		0x2000, 0x873a5f2d, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "21.21",				0x2000, 0xce20a1d4, 4 | BRF_GRA },           // 13 Background Tiles
	{ "22.22",				0x2000, 0x26fc88bf, 4 | BRF_GRA },           // 14

	{ "15.114",				0x4000, 0xed07854e, 5 | BRF_GRA },           // 15 Sprites
	{ "16.113",				0x4000, 0x85cf1939, 5 | BRF_GRA },           // 16
	{ "cr16i112.bin",		0x4000, 0xcbed446c, 5 | BRF_GRA },           // 17
	{ "cr17i111.bin",		0x4000, 0xb0a44330, 5 | BRF_GRA },           // 18
	{ "cr18ic99.bin",		0x4000, 0xff7efd7c, 5 | BRF_GRA },           // 19
	{ "20.100",				0x4000, 0x4bc77ca0, 5 | BRF_GRA },           // 20

	{ "r.36",				0x0100, 0x65dec63d, 6 | BRF_GRA },           // 21 PROMs
	{ "g.35",				0x0100, 0xe79de8cf, 6 | BRF_GRA },           // 22
	{ "b.27",				0x0100, 0xd6ab3448, 6 | BRF_GRA },           // 23
	{ "cr29ic28.bin",		0x0100, 0x7ca273c1, 6 | BRF_GRA },           // 24
	{ "cr32ic69.bin",		0x0200, 0x410d6f86, 6 | BRF_GRA },           // 25
	{ "cr33-108.bin",		0x0200, 0xd33c02ae, 6 | BRF_GRA },           // 26
	{ "cr26ic12.bin",		0x0100, 0x85b5e958, 6 | BRF_GRA },           // 27
	{ "cr27ic15.bin",		0x0100, 0xf7a19fe2, 6 | BRF_GRA },           // 28
	{ "cr25ic8.bin",		0x0020, 0xc85e09ad, 6 | BRF_GRA },           // 29
	{ "cr22ic8.bin",		0x0020, 0x1a7e0b06, 6 | BRF_GRA },           // 30
	{ "cr23ic22.bin",		0x0020, 0xefb5f265, 6 | BRF_GRA },           // 31
	{ "cr24ic42.bin",		0x0020, 0x823878aa, 6 | BRF_GRA },           // 32
};

STD_ROM_PICK(hiryuken)
STD_ROM_FN(hiryuken)

static INT32 HiryukenInit()
{
	return DrvInit(1, 1);
}

struct BurnDriver BurnDrvHiryuken = {
	"hiryuken", "shangkid", NULL, NULL, "1985",
	"Hokuha Syourin Hiryu no Ken\0", NULL, "Taiyo (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VSFIGHT, 0,
	NULL, hiryukenRomInfo, hiryukenRomName, NULL, NULL, NULL, NULL, ShangkidInputInfo, ShangkidDIPInfo,
	HiryukenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	288, 224, 4, 3
};


// Dynamic Ski

static struct BurnRomInfo dynamskiRomDesc[] = {
	{ "dynski.1",			0x1000, 0x30191160, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "dynski.2",			0x1000, 0x5e08a0b0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dynski.3",			0x1000, 0x29cfd740, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dynski.4",			0x1000, 0xe1d47776, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "dynski.5",			0x1000, 0xe39aba1b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "dynski.6",			0x1000, 0x95780608, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "dynski.7",			0x1000, 0xb88d328b, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "dynski.8",			0x1000, 0x8db5e691, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "dynski8.3e",			0x2000, 0x32c354dc, 2 | BRF_GRA },           //  8 Background Tiles
	{ "dynski9.2e",			0x2000, 0x80a6290c, 2 | BRF_GRA },           //  9

	{ "dynski5.14b",		0x2000, 0xaa4ac6e2, 3 | BRF_GRA },           // 10 Sprites
	{ "dynski6.15b",		0x2000, 0x47e76886, 3 | BRF_GRA },           // 11
	{ "dynski7.14d",		0x2000, 0xa153dfa9, 3 | BRF_GRA },           // 12

	{ "dynskic.15g",		0x0020, 0x9333a5e4, 4 | BRF_GRA },           // 13 PROMs
	{ "dynskic.15f",		0x0020, 0x3869514b, 4 | BRF_GRA },           // 14
	{ "dynski.4g",			0x0100, 0x761fe465, 4 | BRF_GRA },           // 15
	{ "dynski.11e",			0x0100, 0xe625aa09, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(dynamski)
STD_ROM_FN(dynamski)

struct BurnDriver BurnDrvDynamski = {
	"dynamski", NULL, NULL, NULL, "1984",
	"Dynamic Ski\0", NULL, "Taiyo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, dynamskiRomInfo, dynamskiRomName, NULL, NULL, NULL, NULL, DynamskiInputInfo, DynamskiDIPInfo,
	DynamskiInit, DrvExit, DynamskiFrame, DynamskiDraw, DrvScan, &DrvRecalc, 0x80,
	224, 288, 3, 4
};
