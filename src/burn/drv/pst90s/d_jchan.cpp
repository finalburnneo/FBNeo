// FB Alpha Jackie Chan driver module
// Based on MAME driver by David Haywood

// Note: a version of sknssprite device is included in this file. it's kept
// separate because it causes bugs w/supernova*, yet fixes zooming issues with
// jchan bg's.  go figure...
// * sauro kani, watch for lines in non-zoomed images

#include "tiles_generic.h"
#include "m68000_intf.h"
//#include "sknsspr.h" - see note above..
#include "kaneko_tmap.h"
#include "ymz280b.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM0;
static UINT8 *Drv68KROM1;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvTransTab;
static UINT8 *DrvNVRAM;
static UINT8 *Drv68KRAM0;
static UINT8 *Drv68KRAM1;
static UINT8 *DrvMCURAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprReg0;
static UINT8 *DrvSprRAM1;
static UINT8 *DrvSprReg1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvVidRegs;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 *mcu_com;

static UINT16 enable_sub_irq;
static INT32 watchdog;

static INT32 nExtraCycles[2];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[4];
static UINT8 DrvReset;

static struct BurnInputInfo JchanInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 10,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy4 + 8,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 11,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy4 + 10,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 14,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 13,	"tilt"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Jchan)

static struct BurnDIPInfo JchanDIPList[]=
{
	{0x17, 0xff, 0xff, 0xdb, NULL				},
	{0x18, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x17, 0x01, 0x01, 0x01, "Off"				},
	{0x17, 0x01, 0x01, 0x00, "On"				},

//	Not yet.
	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x17, 0x01, 0x02, 0x02, "Off"				},
	{0x17, 0x01, 0x02, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x17, 0x01, 0x04, 0x04, "Off"				},
	{0x17, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Sound"			},
	{0x17, 0x01, 0x08, 0x00, "Mono"				},
	{0x17, 0x01, 0x08, 0x08, "Stereo"			},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x17, 0x01, 0x10, 0x10, "Off"				},
	{0x17, 0x01, 0x10, 0x00, "On"				},
	
	{0   , 0xfe, 0   ,    2, "Blood Mode"			},
	{0x17, 0x01, 0x20, 0x20, "Normal"			},
	{0x17, 0x01, 0x20, 0x00, "High"				},

	{0   , 0xfe, 0   ,    2, "Special Prize Available"	},
	{0x17, 0x01, 0x40, 0x40, "No"				},
	{0x17, 0x01, 0x40, 0x00, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Buttons Layout"		},
	{0x17, 0x01, 0x80, 0x80, "3+1"				},
	{0x17, 0x01, 0x80, 0x00, "2+2"				},
};

STDDIPINFO(Jchan)

static struct BurnDIPInfo Jchan2DIPList[]=
{
	{0   , 0xfe, 0   ,    2, "Unused"			},
	{0x17, 0x01, 0x04, 0x04, "Off"				},
	{0x17, 0x01, 0x04, 0x00, "On"				},
};

STDDIPINFOEXT(Jchan2, Jchan, Jchan2)

static void toxboy_handle_04_subcommand(UINT8 mcu_subcmd, UINT16 *mcu_ram)
{
	INT32 offs = (mcu_subcmd & 0x3f) * 8;

	UINT8 *mcu_rom = DrvMCUROM + 0x10000;

	UINT16 romstart  = mcu_rom[offs+2] | (mcu_rom[offs+3]<<8);
	UINT16 romlength = mcu_rom[offs+4] | (mcu_rom[offs+5]<<8);
	UINT16 ramdest   = BURN_ENDIAN_SWAP_INT16(mcu_ram[0x0012/2]);

	for (INT32 x = 0; x < romlength; x++) {
		DrvMCURAM[(ramdest+x)] = mcu_rom[(romstart+x)];
	}
}

static void jchan_mcu_run()
{
	UINT16 *mcu_ram = (UINT16*)DrvMCURAM;
	UINT16 mcu_command = BURN_ENDIAN_SWAP_INT16(mcu_ram[0x0010/2]);		// command nb
	UINT16 mcu_offset  = BURN_ENDIAN_SWAP_INT16(mcu_ram[0x0012/2]) / 2;	// offset in shared RAM where MCU will write
	UINT16 mcu_subcmd  = BURN_ENDIAN_SWAP_INT16(mcu_ram[0x0014/2]);		// sub-command parameter, happens only for command #4

	switch (mcu_command >> 8)
	{
		case 0x04:
			 toxboy_handle_04_subcommand(mcu_subcmd, mcu_ram);
		break;

		case 0x03:
			mcu_ram[mcu_offset] = BURN_ENDIAN_SWAP_INT16((DrvDips[1] << 8) | (DrvDips[0] << 0));
		break;

		case 0x02:
			memcpy (DrvMCURAM + mcu_offset, DrvNVRAM, 0x80);
		break;

		case 0x42:
			memcpy (DrvNVRAM, DrvMCURAM + mcu_offset, 0x80);
		break;
	}
}

static void __fastcall jchan_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x330000:
		case 0x340000:
		case 0x350000:
		case 0x360000:
			mcu_com[(address - 0x330000) >> 16] = BURN_ENDIAN_SWAP_INT16(data);
			if (mcu_com[0] == 0xffff && mcu_com[1] == 0xffff && mcu_com[2] == 0xffff && mcu_com[3] == 0xffff) {
				memset (mcu_com, 0, 4 * sizeof(UINT16));
				jchan_mcu_run();
			}
		return;

		case 0xf00000:
			enable_sub_irq = data & 0x8000;
		return;

		case 0xf80000:
			watchdog = 0;
		return;
	}
}

static void __fastcall jchan_main_write_byte(UINT32 /*address*/, UINT8 /*data*/)
{

}

static UINT16 __fastcall jchan_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x370000:
			return 0; // mcu status
	}

	return 0;
}

static UINT8 __fastcall jchan_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xf00000:
			return DrvInputs[0] >> 8;

		case 0xf00002:
			return DrvInputs[1] >> 8;

		case 0xf00004:
			return DrvInputs[2] >> 8;

		case 0xf00006:
			return DrvInputs[3] >> 8;
	}

	return 0;
}

static void __fastcall jchan_sub_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x800000:
			YMZ280BWrite(0, data & 0xff);
		return;

		case 0x800002:
			YMZ280BWrite(1, data & 0xff);
		return;

		case 0xa00000:
			watchdog = 0;
		return;
	}
}

static void __fastcall jchan_sub_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x800001:
			YMZ280BWrite(0, data);
		return;

		case 0x800003:
			YMZ280BWrite(1, data);
		return;
	}
}

static UINT16 __fastcall jchan_sub_read_word(UINT32 /*address*/)
{
	return 0;
}

static UINT8 __fastcall jchan_sub_read_byte(UINT32 /*address*/)
{
	return 0;
}

static void __fastcall jchan_main_command_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvShareRAM + (address & 0x3ffe))) = BURN_ENDIAN_SWAP_INT16(data);

	if (address == 0x403ffe) {
		SekSetIRQLine(1, 4, CPU_IRQSTATUS_AUTO);
	}
}

static void __fastcall jchan_main_command_write_byte(UINT32 address, UINT8 data)
{
	DrvShareRAM[(address & 0x3fff) ^ 1] = data;
}

static void __fastcall jchan_sub_command_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvShareRAM + (address & 0x3ffe))) = BURN_ENDIAN_SWAP_INT16(data);

	if (address == 0x400000) { // not used?
		SekSetIRQLine(0, 3, CPU_IRQSTATUS_AUTO);
	}
}

static void __fastcall jchan_sub_command_write_byte(UINT32 address, UINT8 data)
{
	DrvShareRAM[(address & 0x3fff) ^ 1] = data;
}

static inline void palette_update(UINT16 offset)
{
	INT32 p = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvPalRAM + offset)));

	INT32 r = (p >>  5) & 0x1f;
	INT32 g = (p >> 10) & 0x1f;
	INT32 b = (p >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r, g, b, 0);
}

static void __fastcall jchan_palette_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvPalRAM + (address & 0xfffe))) = BURN_ENDIAN_SWAP_INT16(data);

	palette_update(address);
}

static void __fastcall jchan_palette_write_byte(UINT32 address, UINT8 data)
{
	DrvPalRAM[(address & 0xffff)^1] = data;

	palette_update(address & ~1);
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekReset(0);
	SekReset(1);

	YMZ280BReset();

	enable_sub_irq = 0;
	watchdog = 0;

	nExtraCycles[0] = nExtraCycles[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM0		= Next; Next += 0x200000;
	Drv68KROM1		= Next; Next += 0x200000;

	DrvMCUROM		= Next; Next += 0x020000;

	DrvGfxROM0		= Next; Next += 0x200000;
	DrvGfxROM1		= Next; Next += 0x2000000;
	DrvGfxROM2		= Next; Next += 0x1000000;

	DrvTransTab		= Next; Next += 0x200000 / 0x100;

	YMZ280BROM		= Next; Next += 0x1000000;

	DrvNVRAM		= Next; Next += 0x000080;

	DrvPalette		= (UINT32*)Next; Next += 0x8001 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM0		= Next; Next += 0x010000;
	Drv68KRAM1		= Next; Next += 0x010000;
	DrvMCURAM		= Next; Next += 0x010000;
	DrvShareRAM		= Next; Next += 0x004000;
	DrvSprRAM0		= Next; Next += 0x004000;
	DrvSprReg0		= Next; Next += 0x000400;
	DrvSprRAM1		= Next; Next += 0x004000;
	DrvSprReg1		= Next; Next += 0x000400;
	DrvPalRAM		= Next; Next += 0x010000;

	DrvVidRAM		= Next; Next += 0x004000;
	DrvVidRegs		= Next; Next += 0x000400;

	mcu_com			= (UINT16*)Next; Next += 0x00004 * sizeof(UINT16);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvDecodeMcuData()
{
	static const UINT8 toybox_mcu_decryption_table[0x100] = {
		0x7b,0x82,0xf0,0xbc,0x7f,0x1d,0xa2,0xc5,0x2a,0xfa,0x55,0xee,0x1a,0xd0,0x59,0x76,
		0x5e,0x75,0x79,0x16,0xa5,0xf6,0x84,0xed,0x0f,0x2e,0xf2,0x36,0x61,0xac,0xcd,0xab,
		0x01,0x3b,0x01,0x87,0x73,0xab,0xce,0x5d,0xd4,0x1d,0x68,0x2a,0x35,0xea,0x13,0x27,
		0x00,0xaa,0x46,0x36,0x6e,0x65,0x80,0x7e,0x19,0xe2,0x96,0xab,0xac,0xa5,0x6c,0x63,
		0x4a,0x6f,0x87,0xf6,0x6a,0xac,0x38,0xe2,0x1f,0x87,0xf9,0xaa,0xf5,0x41,0x60,0xa6,
		0x42,0xb9,0x30,0xf2,0xc3,0x1c,0x4e,0x4b,0x08,0x10,0x42,0x32,0xbf,0xb2,0xc5,0x0f,
		0x7a,0xab,0x97,0xf6,0xe7,0xb3,0x46,0xf8,0xec,0x2b,0x7d,0x5f,0xb1,0x10,0x03,0xe4,
		0x0f,0x22,0xdf,0x8d,0x10,0x66,0xa7,0x7e,0x96,0xbd,0x5a,0xaf,0xaa,0x43,0xdf,0x10,
		0x7c,0x04,0xe2,0x9d,0x66,0xd7,0xf0,0x02,0x58,0x8a,0x55,0x17,0x16,0xe2,0xe2,0x52,
		0xaf,0xd9,0xf9,0x0d,0x59,0x70,0x86,0x3c,0x05,0xd1,0x52,0xa7,0xf0,0xbf,0x17,0xd0,
		0x23,0x15,0xfe,0x23,0xf2,0x80,0x60,0x6f,0x95,0x89,0x67,0x65,0xc9,0x0e,0xfc,0x16,
		0xd6,0x8a,0x9f,0x25,0x2c,0x0f,0x2d,0xe4,0x51,0xb2,0xa8,0x18,0x3a,0x5d,0x66,0xa0,
		0x9f,0xb0,0x58,0xea,0x78,0x72,0x08,0x6a,0x90,0xb6,0xa4,0xf5,0x08,0x19,0x60,0x4e,
		0x92,0xbd,0xf1,0x05,0x67,0x4f,0x24,0x99,0x69,0x1d,0x0c,0x6d,0xe7,0x74,0x88,0x22,
		0x2d,0x15,0x7a,0xa2,0x37,0xa9,0xa0,0xb0,0x2c,0xfb,0x27,0xe5,0x4f,0xb6,0xcd,0x75,
		0xdc,0x39,0xce,0x6f,0x1f,0xfe,0xcc,0xb5,0xe6,0xda,0xd8,0xee,0x85,0xee,0x2f,0x04,
	};

	for (INT32 i = 0; i < 0x20000; i++) {
		DrvMCUROM[i] = DrvMCUROM[i] + toybox_mcu_decryption_table[(i^1) & 0xff];
	}
}

static void DrvFillTransTable()
{
	memset (DrvTransTab, 0, 0x2000);

	for (INT32 i = 0; i < 0x200000; i+= 0x100) {
		DrvTransTab[i/0x100] = 1; // transparent

		for (INT32 j = 0; j < 0x100; j++) {
			if (DrvGfxROM0[j + i]) {
				DrvTransTab[i/0x100] = 0;
				break;
			}
		}
	}
}

static INT32 DrvGfxDecode()
{
	static INT32 Planes[4] = { STEP4(0, 1) };
	static INT32 XOffs[16] = { 4, 0, 12, 8, 20, 16, 28, 24, 8*32+4, 8*32+0, 8*32+12, 8*32+8, 8*32+20, 8*32+16, 8*32+28, 8*32+24 };
	static INT32 YOffs[16] = { STEP8(0, 32), STEP8(16*32, 32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x100000);

	GfxDecode(0x02000, 4, 16, 16, Planes, XOffs, YOffs, 0x400, tmp, DrvGfxROM0);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM0 + 0x0000000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0000001,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0100000,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x0100001,  3, 2)) return 1;

		if (BurnLoadRom(Drv68KROM1 + 0x0000000,  4, 2)) return 1;
		if (BurnLoadRom(Drv68KROM1 + 0x0000001,  5, 2)) return 1;

		if (BurnLoadRom(DrvMCUROM  + 0x0000000,  6, 1)) return 1;
		BurnByteswap (DrvMCUROM, 0x20000);

		if (BurnLoadRom(DrvGfxROM0 + 0x0000000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0400000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0800000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1200000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1400000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1600000, 15, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1600001, 16, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0200000, 18, 1)) return 1;

		if (BurnLoadRom(YMZ280BROM + 0x0000000, 19, 1)) return 1;
		if (BurnLoadRom(YMZ280BROM + 0x0100000, 19, 1)) return 1; // reload
		if (BurnLoadRom(YMZ280BROM + 0x0200000, 20, 1)) return 1;
		if (BurnLoadRom(YMZ280BROM + 0x0400000, 21, 1)) return 1;

		DrvGfxDecode();
		DrvDecodeMcuData();
		DrvFillTransTable();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM0,	0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM0,	0x200000, 0x20ffff, MAP_RAM);
	SekMapMemory(DrvMCURAM,		0x300000, 0x30ffff, MAP_RAM);
	SekMapMemory(DrvShareRAM,	0x400000, 0x403fff, MAP_RAM);
	SekMapMemory(DrvSprRAM0,	0x500000, 0x503fff, MAP_RAM);
	SekMapMemory(DrvSprReg0,	0x600000, 0x60003f|0x3ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x700000, 0x70ffff, MAP_RAM);
	SekSetWriteWordHandler(0,	jchan_main_write_word);
	SekSetWriteByteHandler(0,	jchan_main_write_byte);
	SekSetReadWordHandler(0,	jchan_main_read_word);
	SekSetReadByteHandler(0,	jchan_main_read_byte);

	SekMapHandler(1,		0x403c00, 0x403fff, MAP_WRITE);
	SekSetWriteWordHandler(1,	jchan_main_command_write_word);
	SekSetWriteByteHandler(1,	jchan_main_command_write_byte);

	SekMapHandler(2,		0x700000, 0x70ffff, MAP_WRITE);
	SekSetWriteWordHandler(2,	jchan_palette_write_word);
	SekSetWriteByteHandler(2,	jchan_palette_write_byte);
	SekClose();

	SekInit(1, 0x68000);
	SekOpen(1);
	SekMapMemory(Drv68KROM1,	0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM1,	0x100000, 0x10ffff, MAP_RAM);
	SekMapMemory(DrvShareRAM,	0x400000, 0x403fff, MAP_RAM);
	SekMapMemory(DrvVidRAM,		0x500000, 0x503fff, MAP_RAM);
	SekMapMemory(DrvVidRegs,	0x600000, 0x60001f|0x3ff, MAP_RAM);
	SekMapMemory(DrvSprRAM1,	0x700000, 0x703fff, MAP_RAM);
	SekMapMemory(DrvSprReg1,	0x780000, 0x78003f|0x3ff, MAP_RAM);
	SekSetWriteWordHandler(0,	jchan_sub_write_word);
	SekSetWriteByteHandler(0,	jchan_sub_write_byte);
	SekSetReadWordHandler(0,	jchan_sub_read_word);
	SekSetReadByteHandler(0,	jchan_sub_read_byte);

	SekMapHandler(1,		0x400000, 0x4003ff, MAP_WRITE);
	SekSetWriteWordHandler(1,	jchan_sub_command_write_word);
	SekSetWriteByteHandler(1,	jchan_sub_command_write_byte);
	SekClose();

	YMZ280BInit(16000000, NULL);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	kaneko_view2_init(0, DrvVidRAM, DrvVidRegs, DrvGfxROM0, 0, DrvTransTab, 25, 0);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	YMZ280BExit();
	YMZ280BROM = NULL;

	kaneko_view2_exit();

	BurnFreeMemIndex();

	return 0;
}

#define cliprect_min_y 0
#define cliprect_max_y (nScreenHeight-1)
#define cliprect_min_x 0
#define cliprect_max_x (nScreenWidth - 1)

#define SUPRNOVA_DECODE_BUFFER_SIZE	0x2000

static INT32 sprite_kludge_x = 0, sprite_kludge_y = 0;
static UINT8 decodebuffer[0x2000];

static INT32 skns_rle_decode ( INT32 romoffset, INT32 size, UINT8*gfx_source, INT32 gfx_length )
{
	UINT8 *src = gfx_source;
	INT32 srcsize = gfx_length;
	UINT8 *dst = decodebuffer;
	INT32 decodeoffset = 0;

	while(size>0) {
		UINT8 code = src[(romoffset++)%srcsize];
		size -= (code & 0x7f) + 1;
		if(code & 0x80) { /* (code & 0x7f) normal values will follow */
			code &= 0x7f;
			do {
				dst[(decodeoffset++)%SUPRNOVA_DECODE_BUFFER_SIZE] = src[(romoffset++)%srcsize];
				code--;
			} while(code != 0xff);
		} else {  /* repeat next value (code & 0x7f) times */
			UINT8 val = src[(romoffset++)%srcsize];
			do {
				dst[(decodeoffset++)%SUPRNOVA_DECODE_BUFFER_SIZE] = val;
				code--;
			} while(code != 0xff);
		}
	}
	return &src[romoffset%srcsize]-gfx_source;
}

/* Zooming blitter, zoom is by way of both source and destination offsets */
/* We are working in .6 fixed point if you hadn't guessed */

#define z_decls(step)				\
	UINT32 zxs = 0x10000-(zx_m);			\
	UINT32 zxd = 0x10000-(zx_s);		\
	UINT32 zys = 0x10000-(zy_m);			\
	UINT32 zyd = 0x10000-(zy_s);		\
	INT32 xs, ys, xd, yd, old, old2;		\
	INT32 step_spr = step;				\
	INT32 bxs = 0, bys = 0;				\
	INT32 clip_min_x = cliprect_min_x<<16;		\
	INT32 clip_max_x = (cliprect_max_x+1)<<16;	\
	INT32 clip_min_y = cliprect_min_y<<16;		\
	INT32 clip_max_y = (cliprect_max_y+1)<<16;	\
	sx <<= 16;					\
	sy <<= 16;					\
	x <<= 10;					\
	y <<= 10;

#define z_clamp_x_min()			\
	if(x < clip_min_x) {					\
		do {					\
			bxs += zxs;				\
			x += zxd;					\
		} while(x < clip_min_x);				\
	}

#define z_clamp_x_max()			\
	if(x > clip_max_x) {				\
		do {					\
			bxs += zxs;				\
			x -= zxd;					\
		} while(x > clip_max_x);				\
	}

#define z_clamp_y_min()			\
	if(y < clip_min_y) {					\
		do {					\
			bys += zys;				\
			y += zyd;					\
		} while(y < clip_min_y);				\
		src += (bys>>16)*step_spr;			\
	}

#define z_clamp_y_max()			\
	if(y > clip_max_y) {				\
		do {					\
			bys += zys;				\
			y -= zyd;					\
		} while(y > clip_max_y);				\
		src += (bys>>16)*step_spr;			\
	}

#define z_loop_x()			\
	xs = bxs;					\
	xd = x;					\
	while(xs < sx && xd <= clip_max_x)

#define z_loop_x_flip()			\
	xs = bxs;					\
	xd = x;					\
	while(xs < sx && xd >= clip_min_x)

#define z_loop_y()			\
	ys = bys;					\
	yd = y;					\
	while(ys < sy && yd <= clip_max_y)

#define z_loop_y_flip()			\
	ys = bys;					\
	yd = y;					\
	while(ys < sy && yd >= clip_min_y)

#define z_draw_pixel()				\
	UINT8 val = src[xs >> 16];			\
	if(val)					\
		if ((yd>>16) < nScreenHeight && (xd>>16) < nScreenWidth)	\
			bitmap[(yd>>16) * nScreenWidth + (xd>>16)] = val + colour;

#define z_x_dst(op)			\
	old = xd;					\
	do {						\
		xs += zxs;					\
		xd op zxd;					\
	} while(!((xd^old) & ~0xffff));

#define z_y_dst(op)			\
	old = yd;					\
	old2 = ys;					\
	do {						\
		ys += zys;					\
		yd op zyd;					\
	} while(!((yd^old) & ~0xffff));			\
	while((ys^old2) & ~0xffff) {			\
		src += step_spr;				\
		old2 += 0x10000;				\
	}

static void blit_nf_z(UINT16 *bitmap, const UINT8 *src, INT32 x, INT32 y, INT32 sx, INT32 sy, UINT16 zx_m, UINT16 zx_s, UINT16 zy_m, UINT16 zy_s, INT32 colour)
{
	z_decls(sx);
	z_clamp_x_min();
	z_clamp_y_min();
	z_loop_y() {
		z_loop_x() {
			z_draw_pixel();
			z_x_dst(+=);
		}
		z_y_dst(+=);
	}
}

static void blit_fy_z(UINT16 *bitmap, const UINT8 *src, INT32 x, INT32 y, INT32 sx, INT32 sy, UINT16 zx_m, UINT16 zx_s, UINT16 zy_m, UINT16 zy_s, INT32 colour)
{
	z_decls(sx);
	z_clamp_x_min();
	z_clamp_y_max();
	z_loop_y_flip() {
		z_loop_x() {
			z_draw_pixel();
			z_x_dst(+=);
		}
		z_y_dst(-=);
	}
}

static void blit_fx_z(UINT16 *bitmap, const UINT8 *src, INT32 x, INT32 y, INT32 sx, INT32 sy, UINT16 zx_m, UINT16 zx_s, UINT16 zy_m, UINT16 zy_s, INT32 colour)
{
	z_decls(sx);
	z_clamp_x_max();
	z_clamp_y_min();
	z_loop_y() {
		z_loop_x_flip() {
			z_draw_pixel();
			z_x_dst(-=);
		}
		z_y_dst(+=);
	}
}

static void blit_fxy_z(UINT16 *bitmap, const UINT8 *src, INT32 x, INT32 y, INT32 sx, INT32 sy, UINT16 zx_m, UINT16 zx_s, UINT16 zy_m, UINT16 zy_s, INT32 colour)
{
	z_decls(sx);
	z_clamp_x_max();
	z_clamp_y_max();
	z_loop_y_flip() {
		z_loop_x_flip() {
			z_draw_pixel();
			z_x_dst(-=);
		}
		z_y_dst(-=);
	}
}

static void (*const blit_z[4])(UINT16 *bitmap, const UINT8 *src, INT32 x, INT32 y, INT32 sx, INT32 sy, UINT16 zx_m, UINT16 zx_s, UINT16 zy_m, UINT16 zy_s, INT32 colour) = {
	blit_nf_z,
	blit_fy_z,
	blit_fx_z,
	blit_fxy_z,
};

// disable_priority is a hack to make jchan drawing a bit quicker (rather than moving the sprites around different bitmaps and adding colors
static void jchanskns_draw_sprites(UINT16 *bitmap, UINT32* spriteram_source, INT32 spriteram_size, UINT8* gfx_source, INT32 gfx_length, UINT32* sprite_regs, INT32 disable_priority)
{
	/*- SPR RAM Format -**

      16 bytes per sprite

	0x00  --ss --SS  z--- ----  jjjg g-ff  ppcc cccc

      s = y size
      S = x size
      j = joint
      g = group sprite is part of (if groups are enabled)
      f = flip
      p = priority
      c = palette

	0x04  ---- -aaa  aaaa aaaa  aaaa aaaa  aaaa aaaa

      a = ROM address of sprite data

	0x08  ZZZZ ZZ--  zzzz zz--  xxxx xxxx  xx-- ----

      Z = horizontal zoom table
      z = horizontal zoom subtable
      x = x position

	0x0C  ZZZZ ZZ--  zzzz zz--  yyyy yyyy  yy-- ----

      Z = vertical zoom table
      z = vertical zoom subtable
      x = y position

	**- End of Comments -*/

	UINT32 *source = spriteram_source;
	UINT32 *finish = source + spriteram_size/4;

	INT16 group_x_offset[4];
	INT32 group_y_offset[4];
	INT32 group_enable;
	INT32 group_number;
	INT32 sprite_flip;
	INT16 sprite_x_scroll;
	INT16 sprite_y_scroll;
	INT32 disabled = sprite_regs[0x04/4] & 0x08; // RWR1
	INT32 xsize,ysize, size, pri=0, romoffset, colour=0, xflip,yflip, joint;
	INT32 sx,sy;
	INT32 endromoffs=0, gfxlen;
	INT32 grow;
	UINT16 zoomx_m, zoomx_s, zoomy_m, zoomy_s;
	INT16 xpos=0,ypos=0;

	if ((!disabled)){

		group_enable    = (sprite_regs[0x00/4] & 0x0040) >> 6; // RWR0

		/* Sengekis uses global flip */
		sprite_flip = (sprite_regs[0x04/4] & 0x03); // RWR1

		sprite_y_scroll = INT16((sprite_regs[0x08/4] & 0x7fff) << 1) >> 1; // RWR2
		sprite_x_scroll = INT16((sprite_regs[0x10/4] & 0x7fff) << 1) >> 1; // RWR4

		group_x_offset[0] = (sprite_regs[0x18/4] & 0xffff); // RWR6
		group_y_offset[0] = (sprite_regs[0x1c/4] & 0xffff); // RWR7

		group_x_offset[1] = (sprite_regs[0x20/4] & 0xffff); // RWR8
		group_y_offset[1] = (sprite_regs[0x24/4] & 0xffff); // RWR9

		group_x_offset[2] = (sprite_regs[0x28/4] & 0xffff); // RWR10
		group_y_offset[2] = (sprite_regs[0x2c/4] & 0xffff); // RWR11

		group_x_offset[3] = (sprite_regs[0x30/4] & 0xffff); // RWR12
		group_y_offset[3] = (sprite_regs[0x34/4] & 0xffff); // RWR13

		/* Seems that sprites are consistently off by a fixed no. of pixels in different games
           (Patterns emerge through Manufacturer/Date/Orientation) */
		sprite_x_scroll += sprite_kludge_x;
		sprite_y_scroll += sprite_kludge_y;

		gfxlen = gfx_length;
		while( source<finish )
		{
			xflip = (source[0] & 0x00000200) >> 9;
			yflip = (source[0] & 0x00000100) >> 8;

			ysize = (source[0] & 0x30000000) >> 28;
			xsize = (source[0] & 0x03000000) >> 24;
			xsize ++;
			ysize ++;

			xsize *= 16;
			ysize *= 16;

			size = xsize * ysize;

			joint = (source[0] & 0x0000e000) >> 13;

			if (!(joint & 1))
			{
				xpos =  (source[2] & 0x0000ffff);
				ypos =  (source[3] & 0x0000ffff);

				xpos += sprite_x_scroll; // Global offset
				ypos += sprite_y_scroll;

				if (group_enable)
				{
					group_number = (source[0] & 0x00001800) >> 11;

					/* the group positioning doesn't seem to be working as i'd expect,
					if I apply the x position the cursor on galpani4 ends up moving
					from the correct position to too far right, also the y offset
					seems to cause the position to be off by one in galpans2 even if
					it fixes the position in galpani4?

					even if I take into account the global sprite scroll registers
					it isn't right

					global offset kludged using game specific offset -pjp */

					xpos += group_x_offset[group_number];
					ypos += group_y_offset[group_number];
				}
			}
			else
			{
				xpos +=  (source[2] & 0x0000ffff);
				ypos +=  (source[3] & 0x0000ffff);
			}

			/* Local sprite offset (for taking flip into account and drawing offset) */
			sx = xpos;
			sy = ypos;

			/* Global Sprite Flip (sengekis) */
			if (sprite_flip&2)
			{
				xflip ^= 1;
				sx = (nScreenWidth << 6) - sx;
			}
			if (sprite_flip&1)
			{
				yflip ^= 1;
				sy = (nScreenHeight << 6) - sy;
			}

			/* Palette linking */
			if (!(joint & 2))
			{
				colour = (source[0] & 0x0000003f) >> 0;
			}

			/* Priority and Tile linking */
			if (!(joint & 4))
			{
				romoffset = (source[1] & 0x07ffffff) >> 0;
				pri = (source[0] & 0x000000c0) >> 6;
			} else {
				romoffset = endromoffs;
			}

			grow = (source[0]>>23) & 1;

			if (!grow)
			{
				zoomx_m = (source[2] & 0xff000000) >> 16;
				zoomx_s = (source[2] & 0x00ff0000) >> 8;
				zoomy_m = (source[3] & 0xff000000) >> 16;
				zoomy_s = (source[3] & 0x00ff0000) >> 8;
			}
			else
			{
				// sengekis uses this on sprites which are shrinking as they head towards the ground
				// it's also used on the input test of Gals Panic S2
				//
				// it appears to offer a higher precision 'shrink' mode (although I'm not entirely
				//  convinced this implementation is correct because we simply end up ignoring
				//  part of the data)
				zoomx_m = 0;
				zoomx_s = (source[2] & 0xffff0000) >> 16;
				zoomy_m = 0;
				zoomy_s = (source[3] & 0xffff0000) >> 16;
			}

			romoffset &= gfxlen-1;

			endromoffs = skns_rle_decode ( romoffset, size, gfx_source, gfx_length );

			// in Cyvern

			//  train in tunnel pri = 0x00
			//  nothing?         = 0x01
			//  players etc. pri = 0x02
			//  pickups etc. pri = 0x03

			{
				INT32 NewColour = (colour<<8);
				if (disable_priority) {
					NewColour += disable_priority; // jchan hack
				} else {
					NewColour += (pri << 14);
				}

				if(zoomx_m || zoomx_s || zoomy_m || zoomy_s)
				{
					blit_z[ (xflip<<1) | yflip ](bitmap, decodebuffer, sx, sy, xsize, ysize, zoomx_m, zoomx_s, zoomy_m, zoomy_s, NewColour);
				}
				else
				{
					sx >>= 6;
					sy >>= 6;
					if (!xflip && !yflip) {
						for (INT32 xx = 0; xx<xsize; xx++)
						{
							if ((sx+xx < (cliprect_max_x+1)) && (sx+xx >= cliprect_min_x))
							{
								for (INT32 yy = 0; yy<ysize; yy++)
								{
									if ((sy+yy < (cliprect_max_y+1)) && (sy+yy >= cliprect_min_y))
									{
										INT32 pix;
										pix = decodebuffer[xsize*yy+xx];
										if (pix)
											bitmap[(sy+yy) * nScreenWidth + (sx+xx)] = pix+ NewColour; // change later
									}
								}
							}
						}
					} else if (!xflip && yflip) {
						sy -= ysize;

						for (INT32 xx = 0; xx<xsize; xx++)
						{
							if ((sx+xx < (cliprect_max_x+1)) && (sx+xx >= cliprect_min_x))
							{
								for (INT32 yy = 0; yy<ysize; yy++)
								{
									if ((sy+(ysize-1-yy) < (cliprect_max_y+1)) && (sy+(ysize-1-yy) >= cliprect_min_y))
									{
										INT32 pix;
										pix = decodebuffer[xsize*yy+xx];
										if (pix)
											bitmap[(sy+(ysize-1-yy)) * nScreenWidth + (sx+xx)] = pix+ NewColour; // change later
									}
								}
							}
						}
					} else if (xflip && !yflip) {
						sx -= xsize;

						for (INT32 xx = 0; xx<xsize; xx++)
						{
							if ( (sx+(xsize-1-xx) < (cliprect_max_x+1)) && (sx+(xsize-1-xx) >= cliprect_min_x))
							{
								for (INT32 yy = 0; yy<ysize; yy++)
								{
									if ((sy+yy < (cliprect_max_y+1)) && (sy+yy >= cliprect_min_y))
									{
										INT32 pix;
										pix = decodebuffer[xsize*yy+xx];
										if (pix)
											bitmap[(sy+yy) * nScreenWidth + (sx+(xsize-1-xx))] = pix+ NewColour; // change later
									}
								}
							}
						}
					} else if (xflip && yflip) {
						sx -= xsize;
						sy -= ysize;

						for (INT32 xx = 0; xx<xsize; xx++)
						{
							if ((sx+(xsize-1-xx) < (cliprect_max_x+1)) && (sx+(xsize-1-xx) >= cliprect_min_x))
							{
								for (INT32 yy = 0; yy<ysize; yy++)
								{
									if ((sy+(ysize-1-yy) < (cliprect_max_y+1)) && (sy+(ysize-1-yy) >= cliprect_min_y))
									{
										INT32 pix;
										pix = decodebuffer[xsize*yy+xx];
										if (pix)
											bitmap[(sy+(ysize-1-yy)) * nScreenWidth + (sx+(xsize-1-xx))] = pix+ NewColour; // change later
									}
								}
							}
						}
					}
				}
			}

			source+=4;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc)
	{
		for (INT32 i = 0; i < 0x10000; i+=2) {
			palette_update(i);
		}

		DrvRecalc = 0;
	}

	BurnTransferClear(0x8000); // black

	for (INT32 i = 0; i < 8; i++) {
		if (nSpriteEnable & (1<<i)) kaneko_view2_draw_layer(0, 0, i);
		if (nSpriteEnable & (1<<i)) kaneko_view2_draw_layer(0, 1, i);
	}

	if (nBurnLayer & 1) jchanskns_draw_sprites(pTransDraw, (UINT32*)DrvSprRAM1, 0x4000, DrvGfxROM2, 0x1000000, (UINT32*)DrvSprReg1, 0x4000);
	if (nBurnLayer & 2) jchanskns_draw_sprites(pTransDraw, (UINT32*)DrvSprRAM0, 0x4000, DrvGfxROM1, 0x2000000, (UINT32*)DrvSprReg0, 0x4000);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, 4 * sizeof(UINT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 512;
	INT32 nCyclesTotal[2] = { 16000000 / 60, 16000000 / 60 };
	INT32 nCyclesDone[2]  = { nExtraCycles[0], nExtraCycles[1] };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		CPU_RUN(0, Sek);
		if (i ==  11) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		if (i == 240) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
		SekClose();

		SekOpen(1);
		CPU_RUN(1, Sek);
		if (enable_sub_irq) {
			if (i ==  11) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
			if (i == 240) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);
			if (i == 249) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		}
		SekClose();

		if (i == 240 && pBurnDraw) {
			DrvDraw();
		}
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		YMZ280BRender(pBurnSoundOut, nBurnSoundLen);
	}

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

		YMZ280BScan(nAction, pnMin);

		SCAN_VAR(enable_sub_irq);
		SCAN_VAR(watchdog);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvNVRAM;
		ba.nLen	  = 0x80;
		ba.szName = "NV Ram";
		BurnAcb(&ba);
	}

	return 0;
}


// Jackie Chan - The Kung-Fu Master (rev 4?)

static struct BurnRomInfo jchanRomDesc[] = {
	{ "jm01x4.u67",		0x080000, 0xace80563, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "jm00x4.u68",		0x080000, 0x08172186, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jm11x3.u69",		0x080000, 0xd2e3f913, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jm10x3.u70",		0x080000, 0xee08fee1, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "jsp1x4.u86",		0x080000, 0x787939d0, 2 | BRF_PRG | BRF_ESS }, //  4 68K #1 Code
	{ "jsp0x4.u87",		0x080000, 0x1b27383e, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "jcd0x2.u13",		0x020000, 0x011dae3e, 3 | BRF_PRG | BRF_ESS }, //  6 MCU Data

	{ "jc-200.00",		0x100000, 0x1f30c24e, 4 | BRF_GRA },           //  7 Layer Tiles

	{ "jc-100-00.179",	0x400000, 0x578d928c, 5 | BRF_GRA },           //  8 Sprite "A" Graphics
	{ "jc-101-00.180",	0x400000, 0x7f5e1aca, 5 | BRF_GRA },           //  9
	{ "jc-102-00.181",	0x400000, 0x72caaa68, 5 | BRF_GRA },           // 10
	{ "jc-103-00.182",	0x400000, 0x4e9e9fc9, 5 | BRF_GRA },           // 11
	{ "jc-104-00.183",	0x200000, 0x6b2a2e93, 5 | BRF_GRA },           // 12
	{ "jc-105-00.184",	0x200000, 0x73cad1f0, 5 | BRF_GRA },           // 13
	{ "jc-108-00.185",	0x200000, 0x67dd1131, 5 | BRF_GRA },           // 14
	{ "jcs0x3.164",		0x040000, 0x9a012cbc, 5 | BRF_GRA },           // 15
	{ "jcs1x3.165",		0x040000, 0x57ae7c8d, 5 | BRF_GRA },           // 16

	{ "jc-106-00.171",	0x200000, 0xbc65661b, 6 | BRF_GRA },           // 17 Sprite "B" Graphics
	{ "jc-107-00.172",	0x200000, 0x92a86e8b, 6 | BRF_GRA },           // 18

	{ "jc-301-00.85",	0x100000, 0x9c5b3077, 7 | BRF_SND },           // 19 YMZ280b Samples
	{ "jc-300-00.84",	0x200000, 0x13d5b1eb, 7 | BRF_SND },           // 20
	{ "jcw0x0.u56",		0x040000, 0xbcf25c2a, 7 | BRF_SND },           // 21
};

STD_ROM_PICK(jchan)
STD_ROM_FN(jchan)

struct BurnDriver BurnDrvJchan = {
	"jchan", NULL, NULL, NULL, "1995",
	"Jackie Chan - The Kung-Fu Master (rev 4?)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_KANEKO_MISC, GBF_VSFIGHT, 0,
	NULL, jchanRomInfo, jchanRomName, NULL, NULL, NULL, NULL, JchanInputInfo, JchanDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};

// Jackie Chan - The Kung-Fu Master (rev 3?)

static struct BurnRomInfo jchanaRomDesc[] = {
	{ "jm01x3.u67",		0x080000, 0xc0adb141, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "jm00x3.u68",		0x080000, 0xb1aadc5a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jm11x3.u69",		0x080000, 0xd2e3f913, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jm10x3.u70",		0x080000, 0xee08fee1, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "jsp1x3.u86",		0x080000, 0xd15d2b8e, 2 | BRF_PRG | BRF_ESS }, //  4 68K #1 Code
	{ "jsp0x3.u87",		0x080000, 0xebec50b1, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "jcd0x1.u13",		0x020000, 0x2a41da9c, 3 | BRF_PRG | BRF_ESS }, //  6 MCU Data

	{ "jc-200.00",		0x100000, 0x1f30c24e, 4 | BRF_GRA },           //  7 Layer Tiles

	{ "jc-100-00.179",	0x400000, 0x578d928c, 5 | BRF_GRA },           //  8 Sprite "A" Graphics
	{ "jc-101-00.180",	0x400000, 0x7f5e1aca, 5 | BRF_GRA },           //  9
	{ "jc-102-00.181",	0x400000, 0x72caaa68, 5 | BRF_GRA },           // 10
	{ "jc-103-00.182",	0x400000, 0x4e9e9fc9, 5 | BRF_GRA },           // 11
	{ "jc-104-00.183",	0x200000, 0x6b2a2e93, 5 | BRF_GRA },           // 12
	{ "jc-105-00.184",	0x200000, 0x73cad1f0, 5 | BRF_GRA },           // 13
	{ "jc-108-00.185",	0x200000, 0x67dd1131, 5 | BRF_GRA },           // 14
	{ "jcs0x3.164",		0x040000, 0x9a012cbc, 5 | BRF_GRA },           // 15
	{ "jcs1x3.165",		0x040000, 0x57ae7c8d, 5 | BRF_GRA },           // 16

	{ "jc-106-00.171",	0x200000, 0xbc65661b, 6 | BRF_GRA },           // 17 Sprite "B" Graphics
	{ "jc-107-00.172",	0x200000, 0x92a86e8b, 6 | BRF_GRA },           // 18

	{ "jc-301-00.85",	0x100000, 0x9c5b3077, 7 | BRF_SND },           // 19 YMZ280b Samples
	{ "jc-300-00.84",	0x200000, 0x13d5b1eb, 7 | BRF_SND },           // 20
	{ "jcw0x0.u56",		0x040000, 0xbcf25c2a, 7 | BRF_SND },           // 21
};

STD_ROM_PICK(jchana)
STD_ROM_FN(jchana)

struct BurnDriver BurnDrvJchana = {
	"jchana", "jchan", NULL, NULL, "1995",
	"Jackie Chan - The Kung-Fu Master (rev 3?)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_KANEKO_MISC, GBF_VSFIGHT, 0,
	NULL, jchanaRomInfo, jchanaRomName, NULL, NULL, NULL, NULL, JchanInputInfo, JchanDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};


// Jackie Chan in Fists of Fire

static struct BurnRomInfo jchan2RomDesc[] = {
	{ "j2p1x1.u67",		0x080000, 0x5448c4bc, 1 | BRF_PRG | BRF_ESS }, //  0 68K #0 Code
	{ "j2p1x2.u68",		0x080000, 0x52104ab9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "j2p1x3.u69",		0x080000, 0x8763ebca, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "j2p1x4.u70",		0x080000, 0x0f8e5e69, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "j2p1x5.u86",		0x080000, 0xdc897725, 2 | BRF_PRG | BRF_ESS }, //  4 68K #1 Code
	{ "j2p1x6.u87",		0x080000, 0x594224f9, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "j2d1x1.u13",		0x020000, 0xb2b7fc90, 3 | BRF_PRG | BRF_ESS }, //  6 MCU Data

	{ "jc-200.00",		0x100000, 0x1f30c24e, 4 | BRF_GRA },           //  7 Layer Tiles

	{ "jc-100-00.179",	0x400000, 0x578d928c, 5 | BRF_GRA },           //  8 Sprite "A" Graphics
	{ "jc-101-00.180",	0x400000, 0x7f5e1aca, 5 | BRF_GRA },           //  9
	{ "jc-102-00.181",	0x400000, 0x72caaa68, 5 | BRF_GRA },           // 10
	{ "jc-103-00.182",	0x400000, 0x4e9e9fc9, 5 | BRF_GRA },           // 11
	{ "jc-104-00.183",	0x200000, 0x6b2a2e93, 5 | BRF_GRA },           // 12
	{ "jc-105-00.184",	0x200000, 0x73cad1f0, 5 | BRF_GRA },           // 13
	{ "jc-108-00.185",	0x200000, 0x67dd1131, 5 | BRF_GRA },           // 14
	{ "j2g1x1.164",		0x080000, 0x66a7ea6a, 5 | BRF_GRA },           // 15
	{ "j2g1x2.165",		0x080000, 0x660e770c, 5 | BRF_GRA },           // 16

	{ "jc-106-00.171",	0x200000, 0xbc65661b, 6 | BRF_GRA },           // 17 Sprite "B" Graphics
	{ "jc-107-00.172",	0x200000, 0x92a86e8b, 6 | BRF_GRA },           // 18

	{ "jc-301-00.85",	0x100000, 0x9c5b3077, 7 | BRF_SND },           // 19 YMZ280b Samples
	{ "jc-300-00.84",	0x200000, 0x13d5b1eb, 7 | BRF_SND },           // 20
	{ "j2m1x1.u56",		0x040000, 0xbaf6e25e, 7 | BRF_SND },           // 21
};

STD_ROM_PICK(jchan2)
STD_ROM_FN(jchan2)

struct BurnDriver BurnDrvJchan2 = {
	"jchan2", NULL, NULL, NULL, "1995",
	"Jackie Chan in Fists of Fire\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_KANEKO_MISC, GBF_VSFIGHT, 0,
	NULL, jchan2RomInfo, jchan2RomName, NULL, NULL, NULL, NULL, JchanInputInfo, Jchan2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 240, 4, 3
};
