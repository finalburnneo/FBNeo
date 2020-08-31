// FinalBurn Neo Hyper Duel / Magical Error wo Sagase driver module
// Based on MAME driver by Lucia Elia / Hau

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "i4x00.h"
#include "burn_ym2151.h"
#include "burn_ym2413.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM[1];
static UINT8 *DrvGfxROM[4];
static UINT8 *DrvSndROM;
static UINT8 *DrvShareRAM[3];

static INT32 cpu_trigger;
static INT32 requested_int;
static INT32 vblank_end_timer;

static INT32 game_select = 0;
static INT32 int_num;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[3];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;

static INT32 nExtraCycles[2];

static struct BurnInputInfo HyprduelInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Hyprduel)

static struct BurnDIPInfo HyprduelDIPList[]=
{
	{0x15, 0xff, 0xff, 0x80, NULL						},
	{0x16, 0xff, 0xff, 0xbf, NULL						},
	{0x17, 0xff, 0xff, 0x01, NULL						},

	{0   , 0xfe, 0   ,    2, "Show Warning"				},
	{0x15, 0x01, 0x40, 0x40, "Off"						},
	{0x15, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Test Mode"				},
	{0x15, 0x01, 0x80, 0x80, "Off"						},
	{0x15, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x16, 0x01, 0x07, 0x01, "4 Coins 1 Credits"		},
	{0x16, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x16, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x16, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x16, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x16, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x16, 0x01, 0x07, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x16, 0x01, 0x38, 0x08, "4 Coins 1 Credits"		},
	{0x16, 0x01, 0x38, 0x10, "3 Coins 1 Credits"		},
	{0x16, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x16, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x16, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x16, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x16, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x16, 0x01, 0x38, 0x00, "Free Play"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x16, 0x01, 0x40, 0x40, "Off"						},
	{0x16, 0x01, 0x40, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x16, 0x01, 0x80, 0x80, "Off"						},
	{0x16, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x17, 0x01, 0x01, 0x01, "Off"						},
	{0x17, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x17, 0x01, 0x0c, 0x08, "Easy"						},
	{0x17, 0x01, 0x0c, 0x0c, "Normal"					},
	{0x17, 0x01, 0x0c, 0x04, "Hard"						},
	{0x17, 0x01, 0x0c, 0x00, "Very Hard"				},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x17, 0x01, 0x30, 0x20, "2"						},
	{0x17, 0x01, 0x30, 0x30, "3"						},
	{0x17, 0x01, 0x30, 0x10, "4"						},
	{0x17, 0x01, 0x30, 0x00, "5"						},
};

STDDIPINFO(Hyprduel)

static void update_irq_state()
{
	INT32 irq = requested_int & (~i4x00_irq_enable) & int_num;
	if (irq) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
}

static void subcpu_control_write(INT32 data)
{
	switch (data)
	{
		case 0x0d:
		case 0x0f:
		case 0x01:
			if (SekGetRESETLine(1) == 0) {
				SekSetRESETLine(1, 1);
			}
		break;

		case 0x00:
			if (SekGetRESETLine(1) != 0) {
				SekSetRESETLine(1, 0);
			}
			SekBurnUntilInt();
		break;

		case 0x0c:
		case 0x80:
			SekSetVIRQLine(1, 2, CPU_IRQSTATUS_AUTO);
		break;
	}
}

static void __fastcall hyperduel_main_write_word(UINT32 address, UINT16 data)
{	
	switch (address)
	{
		case 0x400000: // magerror
		case 0x800000: // hyprduel
			subcpu_control_write(data);
		return;

		case 0xe00000:
		return; // nop
	}
	
	bprintf (0, _T("Missed write (word) %5.5x\n"), address);
}

static void __fastcall hyperduel_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0xe00001:
		return; // nop
	}
	
	bprintf (0, _T("Missed write (byte) %5.5x\n"), address);
}

static UINT16 __fastcall hyperduel_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xe00000:
			return ((DrvDips[0] | 0x3f) << 8) | 0xff;

		case 0xe00002:
			return ((DrvDips[2] | 0xc2) << 8) | (DrvDips[1] << 0);

		case 0xe00004:
			return DrvInputs[0];

		case 0xe00006:
			return DrvInputs[1];
	}
	
	bprintf (0, _T("Missed read %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall hyperduel_main_read_byte(UINT32 address)
{
	return SekReadWord(address & ~1) >> ((~address & 1) * 8);
}

static void __fastcall hyperduel_main_sync_write_word(UINT32 address, UINT16 data)
{
	// handler#, address, data (below)
	SEK_DEF_WRITE_WORD(1, address, data)
}

// saving just in-case -dink
// #define DEBUG_SYNC

static void __fastcall hyperduel_main_sync_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xff8000) == 0xc00000) {
		address &= 0x7fff;
		UINT16 *ram = (UINT16*)DrvShareRAM[0];
		DrvShareRAM[0][address ^ 1] = data;

		if (address >= 0x040e && address <= 0x0411)
		{
			if (BURN_ENDIAN_SWAP_INT16(ram[0x40e/2]) + BURN_ENDIAN_SWAP_INT16(ram[0x410/2]))
			{
				if (cpu_trigger == 0 && SekGetRESETLine(1) == 0)
				{
#ifdef DEBUG_SYNC
					bprintf(0, _T("SP1. "));
#endif
					SekSetHALT(0, 1);
					cpu_trigger = 1;
				}
			}

			return;
		}
		if (address == 0x0408)
		{
			if (cpu_trigger == 0 && SekGetRESETLine(1) == 0)
			{
#ifdef DEBUG_SYNC
				bprintf(0, _T("SP2. "));
#endif
				SekSetHALT(0, 1);
				cpu_trigger = 2;
			}

			return;
		}
	}
}

static UINT16 __fastcall hyperduel_sub_sync_read_word(UINT32 address)
{
	SEK_DEF_READ_WORD(1, address)
}

static UINT8 __fastcall hyperduel_sub_sync_read_byte(UINT32 address)
{
	if ((address & 0xfffc00) == 0xc00400)
	{
		if ((address & ~1) == 0xc00408)
		{
			if (cpu_trigger == 1)
			{
#ifdef DEBUG_SYNC
				bprintf(0, _T("sp1. "));
#endif
				SekSetHALT(0, 0);
				cpu_trigger = 0;
			}


		}
		return DrvShareRAM[0][(address & 0x7fff) ^ 1];
	}

	if ((address & 0xfffc00) == 0xfff000)
	{
		if ((address & ~1) == 0xfff34c)
		{
			if (cpu_trigger == 2)
			{
#ifdef DEBUG_SYNC
				bprintf(0, _T("sp2. "));
#endif
				SekSetHALT(0, 0);
				cpu_trigger = 0;
			}
		}
		return DrvShareRAM[2][(address - 0xfe4000) ^ 1];
	}

	return 0;
}

static void __fastcall hyperduel_sub_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
// hyprduel
		case 0x400000:
		case 0x400002:
			if (game_select == 0) BurnYM2151Write((address / 2) & 1, data);
		return;

// magerror
		case 0x800000:
		case 0x800002:
			if (game_select == 1) BurnYM2413Write((address / 2) & 1, data);
		return;

// both
		case 0x400004:
		case 0x800004:
			MSM6295Write(0, data & 0xff);
		return;
	}
}

static void __fastcall hyperduel_sub_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x400000:
		case 0x400001:
		case 0x400002:
		case 0x400003:
			if (game_select == 0) BurnYM2151Write((address / 2) & 1, data);
		return;
		
		case 0x800000:
		case 0x800001:
		case 0x800002:
		case 0x800003:
			if (game_select == 1) BurnYM2413Write((address / 2) & 1, data);
		return;
	// both
		case 0x400004:
		case 0x400005:
		case 0x800004:
		case 0x800005:
			MSM6295Write(0, data & 0xff);
		return;
	}
}

static UINT16 __fastcall hyperduel_sub_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
		case 0x400002:
			return (game_select == 0) ? BurnYM2151Read() : 0;
			
		case 0x800000:
		case 0x800002:
			return 0;

		case 0x400004:
		case 0x800004:
			return MSM6295Read(0);
	}

	return 0;
}

static UINT8 __fastcall hyperduel_sub_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
		case 0x400001:
		case 0x400002:
		case 0x400003:
			return (game_select == 0) ? BurnYM2151Read() : 0;

		case 0x800000:
		case 0x800001:
		case 0x800002:
		case 0x800003:
			return 0;

		case 0x400004:
		case 0x400005:
		case 0x800004:
		case 0x800005:
			return MSM6295Read(0);
	}

	return 0;
}

static void irq_cause_write(UINT16 data)
{
	if (data == int_num)
		requested_int &= ~(int_num & ~i4x00_irq_enable);
	else
		requested_int &= ~(data & i4x00_irq_enable);

	update_irq_state();
}

static UINT16 irq_cause_read()
{
	return requested_int;
}

static void DrvYM2151IrqHandler(INT32 state)
{
	SekSetVIRQLine(1, (state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekSetHALT(0);
	SekClose();

	SekOpen(1);
	SekReset();
	SekSetRESETLine(1); // start in reset
	SekClose();
	
	i4x00_reset();

	MSM6295Reset(0);

	if (game_select) {
		BurnYM2413Reset();
	} else {
		BurnYM2151Reset();
	}

	cpu_trigger = 0;
	requested_int = 0;
	vblank_end_timer = -1;

	nExtraCycles[0] = nExtraCycles[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM[0]		= Next; Next += 0x080000;

	DrvGfxROM[0]		= Next; Next += 0x410000;
	DrvGfxROM[1]		= Next; Next += 0x800000;
	DrvGfxROM[2]		= Next; Next += 0x000400;

	MSM6295ROM			= Next;
	DrvSndROM			= Next; Next += 0x040000;

	AllRam				= Next;

	DrvShareRAM[0]		= Next; Next += 0x020000;
	DrvShareRAM[1]		= Next; Next += 0x004000;
	DrvShareRAM[2]		= Next; Next += 0x01c000;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 HyprduelInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRomExt(Drv68KROM[0] + 0x0000001, k++, 2, 0)) return 1;
		if (BurnLoadRomExt(Drv68KROM[0] + 0x0000000, k++, 2, 0)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[0] + 0x0000000, k++, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x0000002, k++, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x0000004, k++, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x0000006, k++, 8, LD_GROUP(2))) return 1;
		
		memset (DrvGfxROM[0] + 0x400000, 0xff, 0x10000);

		if (BurnLoadRomExt(DrvSndROM    + 0x0000000, k++, 1, 0)) return 1;

		BurnNibbleExpand(DrvGfxROM[0], DrvGfxROM[1], 0x400000, 1, 0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM[0],				0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KROM[0],				0x080000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvShareRAM[0],			0xc00000, 0xc07fff, MAP_RAM);
	SekMapMemory(DrvShareRAM[1],			0xfe0000, 0xfe3fff, MAP_RAM);
	SekMapMemory(DrvShareRAM[2],			0xfe4000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,				hyperduel_main_write_word);
	SekSetWriteByteHandler(0,				hyperduel_main_write_byte);
	SekSetReadWordHandler(0,				hyperduel_main_read_word);
	SekSetReadByteHandler(0,				hyperduel_main_read_byte);

	SekMapHandler(1, 						0xc00400, 0xc007ff, MAP_WRITE);
	SekSetWriteWordHandler(1,				hyperduel_main_sync_write_word);
	SekSetWriteByteHandler(1,				hyperduel_main_sync_write_byte);
	
	i4x00_init(0x400000, DrvGfxROM[0], DrvGfxROM[1], 0x400000, irq_cause_write, irq_cause_read, NULL, 1, 0);

	SekClose();

	SekInit(1, 0x68000);
	SekOpen(1);
	SekMapMemory(DrvShareRAM[0],			0x000000, 0x003fff, MAP_RAM);
	SekMapMemory(DrvShareRAM[2],			0x004000, 0x007fff, MAP_ROM); // read only
	SekMapMemory(DrvShareRAM[0],			0xc00000, 0xc07fff, MAP_RAM);
	SekMapMemory(DrvShareRAM[1],			0xfe0000, 0xfe3fff, MAP_RAM);
	SekMapMemory(DrvShareRAM[2],			0xfe4000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,				hyperduel_sub_write_word);
	SekSetWriteByteHandler(0,				hyperduel_sub_write_byte);
	SekSetReadWordHandler(0,				hyperduel_sub_read_word);
	SekSetReadByteHandler(0,				hyperduel_sub_read_byte);
	
	SekMapHandler(1, 						0xc00400, 0xc007ff, MAP_READ | MAP_FETCH);
	SekMapHandler(1, 						0xfff000, 0xfff3ff, MAP_READ | MAP_FETCH);
	SekSetReadWordHandler(1,				hyperduel_sub_sync_read_word);
	SekSetReadByteHandler(1,				hyperduel_sub_sync_read_byte);

	SekClose();

	int_num = 0x02;

	BurnYM2151Init(4000000, 1);
	BurnTimerAttachSek(10000000);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.45, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.45, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 2062500 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 MagerrorInit()
{
	game_select = 1;
	
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRomExt(Drv68KROM[0] + 0x0000001, k++, 2, 0)) return 1;
		if (BurnLoadRomExt(Drv68KROM[0] + 0x0000000, k++, 2, 0)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[0] + 0x0000000, k++, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x0000002, k++, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x0000004, k++, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x0000006, k++, 8, LD_GROUP(2))) return 1;
		
		memset (DrvGfxROM[0] + 0x400000, 0xff, 0x10000);

		if (BurnLoadRomExt(DrvSndROM    + 0x0000000, k++, 1, 0)) return 1;

		BurnNibbleExpand(DrvGfxROM[0], DrvGfxROM[1], 0x400000, 1, 0);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM[0],				0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM[0],			0xc00000, 0xc1ffff, MAP_RAM);
	SekMapMemory(DrvShareRAM[1],			0xfe0000, 0xfe3fff, MAP_RAM);
	SekMapMemory(DrvShareRAM[2],			0xfe4000, 0xffffff, MAP_RAM);
	
	SekSetWriteWordHandler(0,				hyperduel_main_write_word);
	SekSetWriteByteHandler(0,				hyperduel_main_write_byte);
	SekSetReadWordHandler(0,				hyperduel_main_read_word);
	SekSetReadByteHandler(0,				hyperduel_main_read_byte);
	
	i4x00_init(0x800000, DrvGfxROM[0], DrvGfxROM[1], 0x400000, irq_cause_write, irq_cause_read, NULL, 1, 0);
	
	SekClose();

	SekInit(1, 0x68000);
	SekOpen(1);
	SekMapMemory(DrvShareRAM[0],			0x000000, 0x003fff, MAP_RAM);
	SekMapMemory(DrvShareRAM[2],			0x004000, 0x007fff, MAP_ROM); // read only
	SekMapMemory(DrvShareRAM[0],			0xc00000, 0xc1ffff, MAP_RAM);
	SekMapMemory(DrvShareRAM[1],			0xfe0000, 0xfe3fff, MAP_RAM);
	SekMapMemory(DrvShareRAM[2],			0xfe4000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,				hyperduel_sub_write_word);
	SekSetWriteByteHandler(0,				hyperduel_sub_write_byte);
	SekSetReadWordHandler(0,				hyperduel_sub_read_word);
	SekSetReadByteHandler(0,				hyperduel_sub_read_byte);
	SekClose();

	int_num = 0x01;

	BurnYM2413Init(3579545);
	BurnYM2413SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 2062500 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	i4x00_exit();

	if (game_select) {
		BurnYM2413Exit();
	} else {
		BurnYM2151Exit();
	}
	MSM6295Exit(0);
	SekExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;
	game_select = 0;

	return 0;
}

static void interrupt_callback(INT32 line)
{
	if (line == 224)
	{
		requested_int |= 0x01; // vblank
		requested_int |= 0x20;
		vblank_end_timer = (10000000 / 1000000) * 2500;
		SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		update_irq_state();
	}
	else if (line < 224)
	{
		requested_int |= 0x12; // hsync
		update_irq_state();
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nSegment;
	INT32 nInterleave = 256 * 2;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 10000000 / 60, 10000000 / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles[0], nExtraCycles[1] };

	i4x00_draw_begin();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		INT32 cycles = SekTotalCycles();

		CPU_RUN(0, Sek);
		if (i & 1) interrupt_callback(i / 2);

		if (i4x00_blitter_timer > 0) {
			i4x00_blitter_timer -= SekTotalCycles() - cycles;
			if (i4x00_blitter_timer < 0) {
				requested_int |= 1 << 2; //blitter_bit;
				update_irq_state();
			}
		}

		if (vblank_end_timer > 0) {
			vblank_end_timer -= SekTotalCycles() - cycles;
			if (vblank_end_timer < 0) {
				requested_int &= ~0x20;
			}
		}
		SekClose();

		SekOpen(1);
		if (game_select == 0) {
			BurnTimerUpdate((nCyclesTotal[1] * (i + 1)) / nInterleave);
		} else {
			CPU_RUN(1, Sek);
			if ((i & 0x1f) == 0x1f) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		}

		if (pBurnSoundOut && (i & 3) == 3 && game_select == 0) {
			nSegment = nBurnSoundLen / (nInterleave / 4);
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			MSM6295Render(0, pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}
		SekClose();

		if (i4x00_raster_update && (i & 1)) {
			i4x00_draw_scanline(i / 2);
			i4x00_raster_update = 0;
		}
	}

	i4x00_draw_end();

	SekOpen(1);
	if (game_select == 0) {
		BurnTimerEndFrame(nCyclesTotal[1]);
		if (pBurnSoundOut) {
			nSegment = nBurnSoundLen - nSoundBufferPos;
			if (nSegment > 0) {
				BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
				MSM6295Render(0, pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			}
		}
	}
	if (pBurnSoundOut && game_select == 1) {
		BurnYM2413Render(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}
	
	SekClose();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		
		i4x00_scan(nAction, pnMin);

		if (game_select == 1) {
			BurnYM2413Scan(nAction, pnMin);
		} else {
			BurnYM2151Scan(nAction, pnMin);
		}
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(cpu_trigger);
		SCAN_VAR(requested_int);
		SCAN_VAR(vblank_end_timer);
	}

	return 0;
}


// Hyper Duel (Japan set 1)

static struct BurnRomInfo hyprduelRomDesc[] = {
	{ "24.u24",			0x040000, 0xc7402722, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "23.u23",			0x040000, 0xd8297c2b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ts_hyper-1.u74",	0x100000, 0x4b3b2d3c, 2 | BRF_GRA },           //  2 Graphics
	{ "ts_hyper-2.u75",	0x100000, 0xdc230116, 2 | BRF_GRA },           //  3
	{ "ts_hyper-3.u76",	0x100000, 0x2d770dd0, 2 | BRF_GRA },           //  4
	{ "ts_hyper-4.u77",	0x100000, 0xf88c6d33, 2 | BRF_GRA },           //  5

	{ "97.u97",			0x040000, 0xbf3f8574, 3 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(hyprduel)
STD_ROM_FN(hyprduel)

struct BurnDriver BurnDrvHyprduel = {
	"hyprduel", NULL, NULL, NULL, "1993",
	"Hyper Duel (Japan set 1)\0", NULL, "Technosoft", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, hyprduelRomInfo, hyprduelRomName, NULL, NULL, NULL, NULL, HyprduelInputInfo, HyprduelDIPInfo,
	HyprduelInit, DrvExit, DrvFrame, i4x00_draw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Hyper Duel (Japan set 2)

static struct BurnRomInfo hyprduel2RomDesc[] = {
	{ "24a.u24",		0x040000, 0x2458f91d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "23a.u23",		0x040000, 0x98aedfca, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ts_hyper-1.u74",	0x100000, 0x4b3b2d3c, 2 | BRF_GRA },           //  2 Graphics
	{ "ts_hyper-2.u75",	0x100000, 0xdc230116, 2 | BRF_GRA },           //  3
	{ "ts_hyper-3.u76",	0x100000, 0x2d770dd0, 2 | BRF_GRA },           //  4
	{ "ts_hyper-4.u77",	0x100000, 0xf88c6d33, 2 | BRF_GRA },           //  5

	{ "97.u97",			0x040000, 0xbf3f8574, 3 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(hyprduel2)
STD_ROM_FN(hyprduel2)

struct BurnDriver BurnDrvHyprduel2 = {
	"hyprduel2", "hyprduel", NULL, NULL, "1993",
	"Hyper Duel (Japan set 2)\0", NULL, "Technosoft", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_HORSHOOT, 0,
	NULL, hyprduel2RomInfo, hyprduel2RomName, NULL, NULL, NULL, NULL, HyprduelInputInfo, HyprduelDIPInfo,
	HyprduelInit, DrvExit, DrvFrame, i4x00_draw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};


// Magical Error wo Sagase

static struct BurnRomInfo magerrorRomDesc[] = {
	{ "24.u24",			0x040000, 0x5e78027f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "23.u23",			0x040000, 0x7271ec70, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mr93046-02.u74",	0x100000, 0xf7ba06fb, 2 | BRF_GRA },           //  2 Graphics
	{ "mr93046-04.u75",	0x100000, 0x8c114d15, 2 | BRF_GRA },           //  3
	{ "mr93046-01.u76",	0x100000, 0x6cc3b928, 2 | BRF_GRA },           //  4
	{ "mr93046-03.u77",	0x100000, 0x6b1eb0ea, 2 | BRF_GRA },           //  5

	{ "97.u97",			0x040000, 0x2e62bca8, 3 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(magerror)
STD_ROM_FN(magerror)

struct BurnDriver BurnDrvMagerror = {
	"magerror", NULL, NULL, NULL, "1994",
	"Magical Error wo Sagase\0", NULL, "Technosoft / Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, magerrorRomInfo, magerrorRomName, NULL, NULL, NULL, NULL, HyprduelInputInfo, HyprduelDIPInfo,
	MagerrorInit, DrvExit, DrvFrame, i4x00_draw, DrvScan, &DrvRecalc, 0x1000,
	320, 224, 4, 3
};
