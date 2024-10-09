// FinalBurn Neo Cave-1000 System driver module
// Based on MAME driver by David Haywood, Luca Elia, MetalliC

#include "tiles_generic.h"
#include "sh4_intf.h"
#include "sh3comn.h"
#include "epic12.h"
#include "serflash.h"
#include "rtc9701.h"
#include "ymz770.h"

#define SH3_CLOCK (12800000 * 8)

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static UINT8 *DrvMainROM;
static UINT8 *DrvFlashROM;
static UINT8 *DrvSoundROM;

static UINT8 *DrvMainRAM;
static UINT8 *DrvCacheRAM;

static UINT8 DrvRecalc;

static INT32 nExtraCycles[1];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;
static HoldCoin<2> hold_coin;

static INT32 is_type_d;

// cpu speed changing
static INT32 DriverClock; // selected cpu clockrate
static INT32 nPrevBurnCPUSpeedAdjust;
static UINT8 nPrevCPUTenth;
static INT32 speedhack_burn; // 10ms @ cpu clock, calculated in DrvFrame

static struct BurnInputInfo Cv1kInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy4 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy4 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy4 + 7,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"S3 Test (Jamma)",	BIT_DIGITAL,	DrvJoy1 + 1,	"service2"	},
	{"S3 Test",			BIT_DIGITAL,	DrvJoy3 + 1,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Cv1k)

static struct BurnDIPInfo DefaultDIPList[]=
{
	{0   , 0xfe, 0   ,    2, "Thread Blitter"	},
	{0x01, 0x01, 0x01, 0x00, "Off"				},
	{0x01, 0x01, 0x01, 0x01, "On"				},

	{0   , 0xfe, 0   ,    2, "Speed Hacks"		},
	{0x01, 0x01, 0x02, 0x00, "Off"				},
	{0x01, 0x01, 0x02, 0x02, "On"				},

	{0   , 0xfe, 0   ,    2, "Thread Sync"		},
	{0x01, 0x01, 0x04, 0x00, "Before Exec"		},
	{0x01, 0x01, 0x04, 0x04, "Before Draw"		},

	{0   , 0xfe, 0   ,    2, "Blitter Timing"	},
	{0x02, 0x01, 0x20, 0x00, "Accurate (Buffis)"},
	{0x02, 0x01, 0x20, 0x20, "Antiquity"		},

	{0   , 0xfe, 0   ,    2, "Clip Margin (test)"},
	{0x02, 0x01, 0x40, 0x40, "0  (off)"			},
	{0x02, 0x01, 0x40, 0x00, "32 (on)"			},

	{0   , 0xfe, 0   ,    2, "Busy Sleep (test)"},
	{0x02, 0x01, 0x80, 0x80, "Off"				},
	{0x02, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   , 0x20, "Blitter Delay"	},
	{0x02, 0x01, 0x1f, 0x00, "Off"				},
	{0x02, 0x01, 0x1f, 0x01, "50"				},
	{0x02, 0x01, 0x1f, 0x02, "51"				},
	{0x02, 0x01, 0x1f, 0x03, "52"				},
	{0x02, 0x01, 0x1f, 0x04, "53"				},
	{0x02, 0x01, 0x1f, 0x05, "54"				},
	{0x02, 0x01, 0x1f, 0x06, "55"				},
	{0x02, 0x01, 0x1f, 0x07, "56"				},
	{0x02, 0x01, 0x1f, 0x08, "57"				},
	{0x02, 0x01, 0x1f, 0x09, "58"				},
	{0x02, 0x01, 0x1f, 0x0a, "59"				},
	{0x02, 0x01, 0x1f, 0x0b, "60"				},
	{0x02, 0x01, 0x1f, 0x0c, "61"				},
	{0x02, 0x01, 0x1f, 0x0d, "62"				},
	{0x02, 0x01, 0x1f, 0x0e, "63"				},
	{0x02, 0x01, 0x1f, 0x0f, "64"				},
	{0x02, 0x01, 0x1f, 0x10, "65"				},
	{0x02, 0x01, 0x1f, 0x11, "66"				},
	{0x02, 0x01, 0x1f, 0x12, "67"				},
	{0x02, 0x01, 0x1f, 0x13, "68"				},
	{0x02, 0x01, 0x1f, 0x14, "69"				},
	{0x02, 0x01, 0x1f, 0x15, "70"				},
	{0x02, 0x01, 0x1f, 0x16, "71"				},
	{0x02, 0x01, 0x1f, 0x17, "72"				},
	{0x02, 0x01, 0x1f, 0x18, "73"				},
	{0x02, 0x01, 0x1f, 0x19, "74"				},
	{0x02, 0x01, 0x1f, 0x1a, "75"				},
	{0x02, 0x01, 0x1f, 0x1b, "76"				},
	{0x02, 0x01, 0x1f, 0x1c, "77"				},
	{0x02, 0x01, 0x1f, 0x1d, "78"				},
	{0x02, 0x01, 0x1f, 0x1e, "79"				},
	{0x02, 0x01, 0x1f, 0x1f, "80"				},

	{0   , 0xfe, 0   ,   10, "el_rika's CPU Rate tenth-percent adjust"},
	{0x03, 0x01, 0x0f, 0x00, ".0"				},
	{0x03, 0x01, 0x0f, 0x01, ".1"				},
	{0x03, 0x01, 0x0f, 0x02, ".2"				},
	{0x03, 0x01, 0x0f, 0x03, ".3"				},
	{0x03, 0x01, 0x0f, 0x04, ".4"				},
	{0x03, 0x01, 0x0f, 0x05, ".5"				},
	{0x03, 0x01, 0x0f, 0x06, ".6"				},
	{0x03, 0x01, 0x0f, 0x07, ".7"				},
	{0x03, 0x01, 0x0f, 0x08, ".8"				},
	{0x03, 0x01, 0x0f, 0x09, ".9"				},
};

static struct BurnDIPInfo Cv1kDIPList[]=
{
	DIP_OFFSET(0x18)
	{0x00, 0xff, 0xff, 0x00, NULL				},
	{0x01, 0xff, 0xff, 0x07, NULL				},
	{0x02, 0xff, 0xff, 0x00, NULL				},
	{0x03, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    2, "S2:1"				},
	{0x00, 0x01, 0x01, 0x00, "Off"				},
	{0x00, 0x01, 0x01, 0x01, "On"				},

	{0   , 0xfe, 0   ,    2, "S2:2"				},
	{0x00, 0x01, 0x02, 0x00, "Off"				},
	{0x00, 0x01, 0x02, 0x02, "On"				},

	{0   , 0xfe, 0   ,    2, "S2:3"				},
	{0x00, 0x01, 0x04, 0x00, "Off"				},
	{0x00, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    2, "S2:4"				},
	{0x00, 0x01, 0x08, 0x00, "Off"				},
	{0x00, 0x01, 0x08, 0x08, "On"				},
};

static struct BurnDIPInfo Cv1ksDIPList[]=
{
	DIP_OFFSET(0x18)
	{0x00, 0xff, 0xff, 0x00, NULL				},
	{0x01, 0xff, 0xff, 0x07, NULL				},
	{0x02, 0xff, 0xff, 0x00, NULL				},
	{0x03, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x01, 0x00, "Off"				},
	{0x00, 0x01, 0x01, 0x01, "On"				},

	{0   , 0xfe, 0   ,    2, "Special Mode"		},
	{0x00, 0x01, 0x02, 0x00, "Off"				},
	{0x00, 0x01, 0x02, 0x02, "On"				},
};

STDDIPINFOEXT(Cv1k, Cv1k, Default)
STDDIPINFOEXT(Cv1ks, Cv1ks, Default)

static void __fastcall main_write_long(UINT32 offset, UINT32 data)
{
	if ((offset & 0xffffff80) == 0x18000000) { // 0x18000000 - 0x18000057
		epic12_blitter_write(offset & 0xff, data);
		return;
	}

	if (offset > 0x03fffff) bprintf(0, _T("mwl %x  %x\n"), offset, data);
}
static void __fastcall main_write_word(UINT32 offset, UINT16 data)
{
	if (offset > 0x03fffff) bprintf(0, _T("mww %x  %x\n"), offset, data);
}
static void __fastcall main_write_byte(UINT32 offset, UINT8 data)
{
	if ((offset & 0xffffff80) == 0x18000000) {
		epic12_blitter_write(offset & 0xff, data);
		return;
	}

	switch (offset) {
		case 0x10000000: serflash_data_write(data); return;
		case 0x10000001: serflash_cmd_write(data); return;
		case 0x10000002: serflash_addr_write(data); return;

		case 0x10400000: case 0x10400001: case 0x10400002: case 0x10400003:
		case 0x10400004: case 0x10400005: case 0x10400006: case 0x10400007:
			ymz770_write(offset & 7, data);
			return;


		case 0x10c00001: {
			rtc9701_write_bit(data & 1);
			rtc9701_set_clock_line((data & 2) >> 1);
			rtc9701_set_cs_line((~data & 4) >> 2);
			return;
		}
		case 0x10c00002: /* ?? */ return;
		case 0x10c00003: serflash_enab_write(data); return;
	}

	if (offset > 0x03fffff) bprintf(0, _T("mwb %x  %x\n"), offset, data);
}

static UINT32 __fastcall main_read_long(UINT32 offset)
{
	if ((offset & 0xffffff80) == 0x18000000) {
		return epic12_blitter_read(offset & 0xff);
	}

	//bprintf(0, _T("mrl %x\n"), offset);

	return 0;
}
static UINT16 __fastcall main_read_word(UINT32 offset)
{
	if ((offset & 0xffffff80) == 0x18000000) {
		return epic12_blitter_read(offset & 0xff);
	}

	bprintf(0, _T("mrw %x\n"), offset);

	return 0;
}
static UINT8 __fastcall main_read_byte(UINT32 offset)
{
	if ((offset & 0xffffff80) == 0x18000000) {
		return epic12_blitter_read(offset & 0xff);
	}

	switch (offset) {
		case 0x10000000: return serflash_io_read();
		case 0x10000001: case 0x10000002: case 0x10000003:
		case 0x10000004: case 0x10000005: case 0x10000006:
		case 0x10000007: return 0xff;

		case 0x10c00001: return (rtc9701_read_bit() & 1) | 0xfe;
	}

	bprintf(0, _T("mrb %x\n"), offset);

	return 0;
}

static UINT32 __fastcall main_read_port(UINT32 offset)
{
	//bprintf(0, _T("before_mrp %x\n"), offset);
	switch (offset & ~7) {
		case SH3_PORT_C: return DrvInputs[0];
		case SH3_PORT_D: return DrvInputs[1];
		case SH3_PORT_E: return 0xdf | 0x20; // flash ready
		case SH3_PORT_F: return DrvInputs[2];
		case SH3_PORT_L: return DrvInputs[3];
		case SH3_PORT_J: return 0xff; // blitter(?)
	}

	bprintf(0, _T("mrp %x\n"), offset);
	return 0;
}
static void __fastcall main_write_port(UINT32 offset, UINT32 data)
{
	switch (offset & ~7) {
		case SH3_PORT_E: return; // "not, sure"
		case SH3_PORT_J: return; // blitter(?)
	}

	bprintf(0, _T("mwp %x, %x\n"), offset, data);
}

// hacky speedhack handler
static UINT32 hacky_idle_ram;
static UINT32 hacky_idle_pc;

static UINT32 __fastcall speedhack_read_long(UINT32 offset)
{
	UINT32 pc = Sh3GetPC(-1);
	if ( offset == hacky_idle_ram && (pc == hacky_idle_pc || pc == hacky_idle_pc+2)) {
		//bprintf(0, _T("l"));
		Sh3BurnCycles(speedhack_burn);
	}
	UINT32 V = *((UINT32 *)(DrvMainRAM + (offset & 0xfffffc)));
	V = (V << 16) | (V >> 16);
	return V;
}

static UINT16 __fastcall speedhack_read_word(UINT32 offset)
{
#if 0
	// speedhack for all games uses long handler, just passthru for word/byte
	// reads since this is a high-traffic area. (0xc000000 - 0xc00ffff)
	UINT32 pc = Sh3GetPC(-1);
	if ( offset == hacky_idle_ram && (pc == hacky_idle_pc || pc == hacky_idle_pc+2)) {
		//bprintf(0, _T("w"));
		Sh3BurnCycles(speedhack_burn);
	}
#endif
	return *((UINT16 *)(DrvMainRAM + (offset & 0xfffffe)));
}

static UINT8 __fastcall speedhack_read_byte(UINT32 offset)
{
#if 0
	UINT32 pc = Sh3GetPC(-1);
	if ( offset == hacky_idle_ram && (pc == hacky_idle_pc || pc == hacky_idle_pc+2)) {
		//bprintf(0, _T("b"));
		Sh3BurnCycles(speedhack_burn);
	}
#endif
	return DrvMainRAM[(offset & 0xffffff) ^ 1];
}

static void speedhack_set(UINT32 ram, UINT32 pc)
{
	hacky_idle_ram = ram;
	hacky_idle_pc = pc;

	Sh3MapHandler(1, 0xc000000, 0xc00ffff, MAP_READ);
	Sh3SetReadByteHandler (1, speedhack_read_byte);
	Sh3SetReadWordHandler (1, speedhack_read_word);
	Sh3SetReadLongHandler (1, speedhack_read_long);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	Sh3Open(0);
	Sh3Reset();
	Sh3Close();

	epic12_reset();
	serflash_reset();
	rtc9701_reset();
	ymz770_reset();

	nExtraCycles[0] = 0;

	nPrevBurnCPUSpeedAdjust = -1;
	nPrevCPUTenth = 0xff;

	hold_coin.reset();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x0400100; // +0x100 overdump crud
	DrvFlashROM		= Next; Next += 0x8400000;
	DrvSoundROM		= Next; Next += 0x0800100; // +0x100 overdump crud

	AllRam			= Next;

	if (is_type_d) {
		DrvMainRAM		= Next; Next += 0x1000000;
	} else {
		DrvMainRAM		= Next; Next +=  0x800000;
	}
	DrvCacheRAM		= Next; Next += 0x0004000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoadRoms()
{
	struct BurnRomInfo ri;
	BurnDrvGetRomInfo(&ri, 0);

	if (BurnLoadRom(DrvMainROM,  0, 1)) return 1;
	if (ri.nLen == 0x200000) memcpy (DrvMainROM + 0x200000, DrvMainROM, 0x200000);
	//if (ri.nLen >= 0x400000) type_d = 1;

	if (BurnLoadRom(DrvFlashROM, 1, 1)) return 1;

	if (BurnLoadRom(DrvSoundROM + 0x000000, 2, 1)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0x400000, 3, 1)) return 1;
	BurnByteswap(DrvSoundROM, 0x800000);

	return 0;
}

struct speedy_s {
	char set[16][16];
	UINT32 pc;
	UINT32 ram;
};

static speedy_s gamelist[] = {
	{ {"mushisam", "mushisamb", "\0", }, 				0xc04a2aa, 0xc0024d8 },
	{ {"ibara", "ibarao", "mushisama", "\0", }, 		0xc04a0aa, 0xc0022f0 },
	{ {"espgal2", "espgal2a", "espgal2b", "\0", }, 		0xc05177a, 0xc002310 },
	{ {"mushitam", "mushitama", "\0", }, 				0xc04a0da, 0xc0022f0 },
	{ {"ibarablk", "ibarablka", "\0", }, 				0xc05176a, 0xc002310 },
	{ {"futari15", "futari15a", "futari10", "\0", }, 	0xc05176a, 0xc002310 },
	{ {"futaribl", "futariblj", "mmpork", "\0", }, 		0xc05176a, 0xc002310 },
	{ {"pinkswts", "pinkswtsa", "pinkswtsb",
	   "pinkswtsx", "pinkswtssc", "\0", }, 				0xc05176a, 0xc002310 },
	{ {"deathsml", "\0", }, 							0xc0519a2, 0xc002310 },
	{ {"dsmbl", "ddpdfk", "ddpdfk10", "dfkbl",
	   "akatana", "ddpsdoj", "sdojak", "\0", },			0xc1d1346, 0xc002310 },
	{ {"\0", }, 0, 0 },
};

static void init_speedhack()
{
	UINT32 hack_ram = 0;
	UINT32 hack_pc = 0;

	for (INT32 i = 0; gamelist[i].pc != 0; i++) {
		for (INT32 setnum = 0; gamelist[i].set[setnum][0] != '\0'; setnum++) {
			if (!strcmp(BurnDrvGetTextA(DRV_NAME), gamelist[i].set[setnum])) {
				bprintf(0, _T("*** found speedhack for %S\n"),gamelist[i].set[setnum]);
				hack_ram = gamelist[i].ram;
				hack_pc = gamelist[i].pc;
				break;
			}
		}
	}

	if (hack_ram && hack_pc) {
		bprintf(0, _T("hack_ram: %x  hack_pc: %x\n"), hack_ram, hack_pc);
	} else {
		bprintf(0, _T("*** UHOH!  Speedhack not found!  ***\n"));
	}

	speedhack_set(hack_ram, hack_pc);
}

static INT32 DrvInit()
{
	struct BurnRomInfo ri;
	BurnDrvGetRomInfo(&ri, 0);
	if (ri.nLen >= 0x400000) is_type_d = 1;

	BurnAllocMemIndex();
	GenericTilesInit();

	if (DrvLoadRoms()) { return 1; }

	Sh3Init(0, SH3_CLOCK, 0, 0, 0, 0, 0, 1, 0, 1, 0);
	Sh3Open(0);

	Sh3MapMemory(DrvMainROM,		0x0000000, 	0x03fffff,	MAP_ROM);
	if (is_type_d == 0) {
		Sh3MapMemory(DrvMainRAM,	0xc000000, 	0xc7fffff,	MAP_RAM);
		Sh3MapMemory(DrvMainRAM,	0xc800000, 	0xcffffff,	MAP_RAM);
	} else {
		Sh3MapMemory(DrvMainRAM,	0xc000000, 	0xcffffff,	MAP_RAM);
	}
	Sh3MapMemory(DrvCacheRAM,		0xf0000000,	0xf0003fff,	MAP_RAM); // this 16k cache area used as ram

	Sh3SetReadByteHandler (0, main_read_byte);
	Sh3SetReadWordHandler (0, main_read_word);
	Sh3SetReadLongHandler (0, main_read_long);
	Sh3SetWriteByteHandler(0, main_write_byte);
	Sh3SetWriteWordHandler(0, main_write_word);
	Sh3SetWriteLongHandler(0, main_write_long);

	Sh3SetReadPortHandler(main_read_port);
	Sh3SetWritePortHandler(main_write_port);

	init_speedhack(); // install the hacky speedhack handler

	Sh3Close();

	epic12_init((is_type_d) ? 0x1000000 : 0x0800000, (UINT16*)DrvMainRAM, &DrvDips[0]);
	serflash_init(DrvFlashROM, 0x8400000);
	rtc9701_init();

	ymz770_init(DrvSoundROM, 0x800000);

	ymz770_set_buffered(Sh3TotalCycles, SH3_CLOCK);
	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	Sh3Exit();

	epic12_exit();
	serflash_exit();
	rtc9701_exit();
	ymz770_exit();

	BurnFreeMemIndex();
	GenericTilesExit();

	is_type_d = 0;

	return 0;
}

static INT32 DrvDraw()
{
	epic12_draw_screen(DrvRecalc);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	// check if cpu rate changes here..
	if (nPrevBurnCPUSpeedAdjust != nBurnCPUSpeedAdjust || DrvDips[3] != nPrevCPUTenth) {
		bprintf(0, _T("Setting CPU Clock selection.\n"));
		nPrevBurnCPUSpeedAdjust = nBurnCPUSpeedAdjust;
		nPrevCPUTenth = DrvDips[3];

		INT32 i_percent = ((double)100 * nBurnCPUSpeedAdjust / 256) + 0.5; // whole percent comes from UI
		double dPercent = i_percent + (0.1 * (DrvDips[3] & 0xf)); // .x tenth percent comes from DIPS

		DriverClock = (INT32)((INT64)SH3_CLOCK * dPercent / 100);
		speedhack_burn = (double)((double)DriverClock / 1000000) * 10; // 10us

		Sh3SetClockCV1k(DriverClock);
		ymz770_set_buffered(Sh3TotalCycles, DriverClock);

		bprintf(0, _T("Main Clock %d  at %0.1f%%  Adjusted Clock %d\n"), SH3_CLOCK, dPercent, DriverClock);
	}
	// set blitter delay, blitter threading & speedhack via dip setting
	{
		INT32 delay = DrvDips[2] & 0x1f;

		epic12_set_blitterdelay_method(DrvDips[2] & 0x20);
		epic12_set_blitterdelay((delay) ? ((delay - 1) + 50) : 0, speedhack_burn);
		epic12_set_blitterthreading(DrvDips[1] & 1);
		Sh3SetTimerGranularity(DrvDips[1] & 2);

		// test stuff for el_rika
		epic12_set_blitter_clipping_margin(!(DrvDips[2] & 0x40));
		epic12_set_blitter_sleep_on_busy(!(DrvDips[2] & 0x80));
	}

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xff;
		DrvInputs[3] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		hold_coin.checklow(0, DrvInputs[0], 1<<2, 2);
		hold_coin.checklow(1, DrvInputs[0], 1<<3, 2);
	}

	Sh3NewFrame();

	INT32 nInterleave = 240;
	INT32 nCyclesTotal[1] = { DriverClock / 60 };
	INT32 nCyclesDone[1] = { nExtraCycles[0] };

	Sh3Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sh3);
	}

	Sh3SetIRQLine(2, CPU_IRQSTATUS_HOLD);

	if (pBurnSoundOut) {
		ymz770_update(pBurnSoundOut, nBurnSoundLen);
	}

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];

	Sh3Close();

	rtc9701_once_per_frame();

	if (DrvDips[1] & 4) { // Thread Sync: Before Draw
		epic12_wait_blitterthread();
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All RAM");
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		Sh3Scan(nAction);

		ymz770_scan(nAction, pnMin);
		epic12_scan(nAction, pnMin);

		SCAN_VAR(nExtraCycles);

		hold_coin.scan();
	}

	serflash_scan(nAction, pnMin); // writes back to flash (hiscores ddpdfk, etc) here
	rtc9701_scan(nAction, pnMin); // eeprom (nvram) lives here

	return 0;
}

// Mushihime-Sama (2004/10/12.MASTER VER.)

static struct BurnRomInfo mushisamRomDesc[] = {
	{ "mushisam_u4",	0x0200000, 0x15321b30, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "mushisam_u2",	0x8400000, 0x4f0a842a, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x138e2050, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xe3d05c9f, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(mushisam)
STD_ROM_FN(mushisam)

struct BurnDriver BurnDrvMushisam = {
	"mushisam", NULL, NULL, NULL, "2004",
	"Mushihime-Sama (2004/10/12.MASTER VER.)\0", NULL, "Cave (AMI license)", "CA011",
	L"Mushihime-Sama\0\u866b\u59eb\u3055\u307e (2004/10/12.MASTER VER.)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, mushisamRomInfo, mushisamRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Mushihime-Sama (2004/10/12 MASTER VER.)

static struct BurnRomInfo mushisamaRomDesc[] = {
	{ "mushisama_u4",	0x0200000, 0x0b5b30b2, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "mushisam_u2",	0x8400000, 0x4f0a842a, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x138e2050, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xe3d05c9f, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(mushisama)
STD_ROM_FN(mushisama)

struct BurnDriver BurnDrvMushisama = {
	"mushisama", "mushisam", NULL, NULL, "2004",
	"Mushihime-Sama (2004/10/12 MASTER VER.)\0", NULL, "Cave (AMI license)", "CA011",
	L"Mushihime-Sama\0\u866b\u59eb\u3055\u307e (2004/10/12 MASTER VER.)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, mushisamaRomInfo, mushisamaRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Mushihime-Sama (2004/10/12 MASTER VER)

static struct BurnRomInfo mushisambRomDesc[] = {
	{ "mushisamb_u4",	0x0200000, 0x9f1c7f51, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "mushisam_u2",	0x8400000, 0x4f0a842a, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x138e2050, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xe3d05c9f, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(mushisamb)
STD_ROM_FN(mushisamb)

struct BurnDriver BurnDrvMushisamb = {
	"mushisamb", "mushisam", NULL, NULL, "2004",
	"Mushihime-Sama (2004/10/12 MASTER VER)\0", NULL, "Cave (AMI license)", "CA011",
	L"Mushihime-Sama\0\u866b\u59eb\u3055\u307e (2004/10/12 MASTER VER)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, mushisambRomInfo, mushisambRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Espgaluda II (2005/11/14.MASTER VER.)

static struct BurnRomInfo espgal2RomDesc[] = {
	{ "espgal2_u4",	0x0200000, 0x2cb37c03, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "u2",			0x8400000, 0x222f58c7, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",		0x0400000, 0xb9a10c22, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",		0x0400000, 0xc76b1ec4, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(espgal2)
STD_ROM_FN(espgal2)

struct BurnDriver BurnDrvEspgal2 = {
	"espgal2", NULL, NULL, NULL, "2005",
	"Espgaluda II (2005/11/14.MASTER VER.)\0", NULL, "Cave (AMI license)", "CA013",
	L"Espgaluda II\0\u30a8\u30b9\u30d7\u30ac\u30eb\u30fc\u30c0II (2005/11/14.MASTER VER.)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, espgal2RomInfo, espgal2RomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Espgaluda II (2005/11/14 MASTER VER, newer CV1000-B PCB)

static struct BurnRomInfo espgal2aRomDesc[] = {
	{ "espgal2a_u4",	0x0200000, 0x843608b8, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "u2",				0x8400000, 0x222f58c7, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0xb9a10c22, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xc76b1ec4, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(espgal2a)
STD_ROM_FN(espgal2a)

struct BurnDriver BurnDrvEspgal2a = {
	"espgal2a", "espgal2", NULL, NULL, "2005",
	"Espgaluda II (2005/11/14 MASTER VER, newer CV1000-B PCB)\0", NULL, "Cave (AMI license)", "CA013",
	L"Espgaluda II\0\u30a8\u30b9\u30d7\u30ac\u30eb\u30fc\u30c0II (2005/11/14 MASTER VER, newer CV1000-B PCB)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, espgal2aRomInfo, espgal2aRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Espgaluda II (2005/11/14 MASTER VER, original CV1000-B PCB)

static struct BurnRomInfo espgal2bRomDesc[] = {
	{ "espgal2b_u4",	0x0200000, 0x09c908bb, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "u2",				0x8400000, 0x222f58c7, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0xb9a10c22, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xc76b1ec4, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(espgal2b)
STD_ROM_FN(espgal2b)

struct BurnDriver BurnDrvEspgal2b = {
	"espgal2b", "espgal2", NULL, NULL, "2005",
	"Espgaluda II (2005/11/14 MASTER VER, original CV1000-B PCB)\0", NULL, "Cave (AMI license)", "CA013",
	L"Espgaluda II\0\u30a8\u30b9\u30d7\u30ac\u30eb\u30fc\u30c0II (2005/11/14 MASTER VER, original CV1000-B PCB)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, espgal2bRomInfo, espgal2bRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Puzzle! Mushihime-Tama (2005/09/09.MASTER VER)

static struct BurnRomInfo mushitamRomDesc[] = {
	{ "mushitam_u4",	0x0200000, 0xc49eb6b1, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "mushitam_u2",	0x8400000, 0x8ba498ab, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x701a912a, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0x6feeb9a1, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(mushitam)
STD_ROM_FN(mushitam)

struct BurnDriver BurnDrvMushitam = {
	"mushitam", NULL, NULL, NULL, "2005",
	"Puzzle! Mushihime-Tama (2005/09/09.MASTER VER)\0", NULL, "Cave (AMI license)", "CA???",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CAVE_CV1000, GBF_PUZZLE, 0,
	NULL, mushitamRomInfo, mushitamRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Puzzle! Mushihime-Tama (2005/09/09 MASTER VER)

static struct BurnRomInfo mushitamaRomDesc[] = {
	{ "mushitama_u4",	0x0200000, 0x4a23e6c8, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "mushitam_u2",	0x8400000, 0x8ba498ab, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x701a912a, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0x6feeb9a1, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(mushitama)
STD_ROM_FN(mushitama)

struct BurnDriver BurnDrvMushitama = {
	"mushitama", "mushitam", NULL, NULL, "2005",
	"Puzzle! Mushihime-Tama (2005/09/09 MASTER VER)\0", NULL, "Cave (AMI license)", "CA???",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CAVE_CV1000, GBF_PUZZLE, 0,
	NULL, mushitamaRomInfo, mushitamaRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Mushihime-Sama Futari Ver 1.5 (2006/12/8.MASTER VER. 1.54.)

static struct BurnRomInfo futari15RomDesc[] = {
	{ "futari15_u4",	0x0200000, 0xe8c5f128, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "futari15_u2",	0x8400000, 0xb9eae1fc, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x39f1e1f4, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xc631a766, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(futari15)
STD_ROM_FN(futari15)

struct BurnDriver BurnDrvFutari15 = {
	"futari15", NULL, NULL, NULL, "2006",
	"Mushihime-Sama Futari Ver 1.5 (2006/12/8.MASTER VER. 1.54.)\0", NULL, "Cave (AMI license)", "CA015",
	L"Mushihime-Sama Futari Ver 1.5\0\u866b\u59eb\u3055\u307e\u3075\u305f\u308a Ver 1.5 (2006/12/8.MASTER VER. 1.54.)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, futari15RomInfo, futari15RomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Mushihime-Sama Futari Ver 1.5 (2006/12/8 MASTER VER 1.54)

static struct BurnRomInfo futari15aRomDesc[] = {
	{ "futari15a_u4",	0x0200000, 0xa609cf89, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "futari15_u2",	0x8400000, 0xb9eae1fc, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x39f1e1f4, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xc631a766, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(futari15a)
STD_ROM_FN(futari15a)

struct BurnDriver BurnDrvFutari15a = {
	"futari15a", "futari15", NULL, NULL, "2006",
	"Mushihime-Sama Futari Ver 1.5 (2006/12/8 MASTER VER 1.54)\0", NULL, "Cave (AMI license)", "CA015",
	L"Mushihime-Sama Futari Ver 1.5\0\u866b\u59eb\u3055\u307e\u3075\u305f\u308a Ver 1.5 (2006/12/8 MASTER VER 1.54)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, futari15aRomInfo, futari15aRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Mushihime-Sama Futari Ver 1.0 (2006/10/23 MASTER VER.)

static struct BurnRomInfo futari10RomDesc[] = {
	{ "futari10_u4",	0x0200000, 0xb127dca7, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "futari10_u2",	0x8400000, 0x78ffcd0c, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x39f1e1f4, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xc631a766, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(futari10)
STD_ROM_FN(futari10)

struct BurnDriver BurnDrvFutari10 = {
	"futari10", "futari15", NULL, NULL, "2006",
	"Mushihime-Sama Futari Ver 1.0 (2006/10/23 MASTER VER.)\0", NULL, "Cave (AMI license)", "CA015",
	L"Mushihime-Sama Futari Ver 1.0\0\u866b\u59eb\u3055\u307e\u3075\u305f\u308a Ver 1.0 (2006/10/23 MASTER VER.)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, futari10RomInfo, futari10RomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Mushihime-Sama Futari Black Label - Another Ver (2009/11/27 INTERNATIONAL BL)

static struct BurnRomInfo futariblRomDesc[] = {
	{ "futaribli_u4",	0x0200000, 0x1971dd16, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "futariblk_u2",	0x8400000, 0x08c6fd62, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x39f1e1f4, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xc631a766, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(futaribl)
STD_ROM_FN(futaribl)

struct BurnDriver BurnDrvFutaribl = {
	"futaribl", NULL, NULL, NULL, "2007",
	"Mushihime-Sama Futari Black Label - Another Ver (2009/11/27 INTERNATIONAL BL)\0", NULL, "Cave (AMI license)", "CA015B",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, futariblRomInfo, futariblRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Mushihime-Sama Futari Black Label (2007/12/11 BLACK LABEL VER)

static struct BurnRomInfo futaribljRomDesc[] = {
	{ "futariblk_u4",	0x0200000, 0xb9467b6d, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "futariblk_u2",	0x8400000, 0x08c6fd62, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x39f1e1f4, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xc631a766, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(futariblj)
STD_ROM_FN(futariblj)

struct BurnDriver BurnDrvFutariblj = {
	"futariblj", "futaribl", NULL, NULL, "2007",
	"Mushihime-Sama Futari Black Label (2007/12/11 BLACK LABEL VER)\0", NULL, "Cave (AMI license)", "CA015B",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, futaribljRomInfo, futaribljRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Ibara (2005/03/22 MASTER VER.., '06. 3. 7 ver.)

static struct BurnRomInfo ibaraRomDesc[] = {
	{ "ibara_u4",		0x0200000, 0xd5fb6657, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "u2",				0x8400000, 0x55840976, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0xee5e585d, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xf0aa3cb6, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(ibara)
STD_ROM_FN(ibara)

struct BurnDriver BurnDrvIbara = {
	"ibara", NULL, NULL, NULL, "2005",
	"Ibara (2005/03/22 MASTER VER.., '06. 3. 7 ver.)\0", NULL, "Cave (AMI license)", "CA012",
	L"Ibara\0\u92f3\u8594\u8587 (2005/03/22 MASTER VER.., '06. 3. 7 ver.)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, ibaraRomInfo, ibaraRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1ksDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Ibara (2005/03/22 MASTER VER..)

static struct BurnRomInfo ibaraoRomDesc[] = {
	{ "ibarao_u4",		0x0200000, 0x8e6c155d, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "u2",				0x8400000, 0x55840976, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0xee5e585d, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xf0aa3cb6, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(ibarao)
STD_ROM_FN(ibarao)

struct BurnDriver BurnDrvIbarao = {
	"ibarao", "ibara", NULL, NULL, "2005",
	"Ibara (2005/03/22 MASTER VER..)\0", NULL, "Cave (AMI license)", "CA012",
	L"Ibara\0\u92f3\u8594\u8587 (2005/03/22 MASTER VER..)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, ibaraoRomInfo, ibaraoRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1ksDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Ibara Kuro Black Label (2006/02/06. MASTER VER.)

static struct BurnRomInfo ibarablkRomDesc[] = {
	{ "ibarablk_u4",	0x0200000, 0xee1f1f77, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "ibarablk_u2",	0x8400000, 0x5e46be44, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0xa436bb22, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xd11ab6b6, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(ibarablk)
STD_ROM_FN(ibarablk)

struct BurnDriver BurnDrvIbarablk = {
	"ibarablk", NULL, NULL, NULL, "2006",
	"Ibara Kuro Black Label (2006/02/06. MASTER VER.)\0", NULL, "Cave (AMI license)", "CA012B",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, ibarablkRomInfo, ibarablkRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1ksDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Ibara Kuro Black Label (2006/02/06 MASTER VER.)

static struct BurnRomInfo ibarablkaRomDesc[] = {
	{ "ibarablka_u4",	0x0200000, 0xa9d43839, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "ibarablka_u2",	0x8400000, 0x33400d96, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0xa436bb22, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xd11ab6b6, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(ibarablka)
STD_ROM_FN(ibarablka)

struct BurnDriver BurnDrvIbarablka = {
	"ibarablka", "ibarablk", NULL, NULL, "2006",
	"Ibara Kuro Black Label (2006/02/06 MASTER VER.)\0", NULL, "Cave (AMI license)", "CA012B",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, ibarablkaRomInfo, ibarablkaRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1ksDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Deathsmiles (2007/10/09 MASTER VER)

static struct BurnRomInfo deathsmlRomDesc[] = {
	{ "u4",				0x0200000, 0x1a7b98bf, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "u2",				0x8400000, 0x59ef5d78, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0xaab718c8, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0x83881d84, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(deathsml)
STD_ROM_FN(deathsml)

struct BurnDriver BurnDrvDeathsml = {
	"deathsml", NULL, NULL, NULL, "2007",
	"Deathsmiles (2007/10/09 MASTER VER)\0", NULL, "Cave (AMI license)", "CA017",
	L"Deathsmiles\0\uc730\ub930\ub930\ua430\ueb30\uba30 (2007/10/09 MASTER VER)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_HORSHOOT, 0,
	NULL, deathsmlRomInfo, deathsmlRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Muchi Muchi Pork! (2007/ 4/17 MASTER VER.)

static struct BurnRomInfo mmporkRomDesc[] = {
	{ "u4",				0x0200000, 0xd06cfa42, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "u2",				0x8400000, 0x1ee961b8, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x4a4b36df, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xce83d07b, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(mmpork)
STD_ROM_FN(mmpork)

struct BurnDriver BurnDrvMmpork = {
	"mmpork", NULL, NULL, NULL, "2007",
	"Muchi Muchi Pork! (2007/ 4/17 MASTER VER.)\0", NULL, "Cave (AMI license)", "CA016",
	L"Muchi Muchi Pork!\0\u3080\u3061\u3080\u3061\u30dd\u30fc\u30af! (2007/ 4/17 MASTER VER.)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, mmporkRomInfo, mmporkRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1ksDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Medal Mahjong Moukari Bancho (2007/06/05 MASTER VER.)

static struct BurnRomInfo mmmbancRomDesc[] = {
	{ "u4",				0x0200000, 0x5589d8c6, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "u2",				0x8400000, 0x2e38965a, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x4caaa1bf, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0x8e3a51ba, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(mmmbanc)
STD_ROM_FN(mmmbanc)

struct BurnDriverD BurnDrvMmmbanc = {
	"mmmbanc", NULL, NULL, NULL, "2007",
	"Medal Mahjong Moukari Bancho (2007/06/05 MASTER VER.)\0", NULL, "Cave (AMI license)", "CMDL01",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_CAVE_CV1000, GBF_MISC, 0,
	NULL, mmmbancRomInfo, mmmbancRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// Pink Sweets: Ibara Sorekara (2006/04/06 MASTER VER....)

static struct BurnRomInfo pinkswtsRomDesc[] = {
	{ "pinkswts_u4",	0x0200000, 0x5d812c9e, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "pinkswts_u2",	0x8400000, 0xa2fa5363, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x4b82d250, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xe93f0627, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(pinkswts)
STD_ROM_FN(pinkswts)

struct BurnDriver BurnDrvPinkswts = {
	"pinkswts", NULL, NULL, NULL, "2006",
	"Pink Sweets: Ibara Sorekara (2006/04/06 MASTER VER....)\0", NULL, "Cave (AMI license)", "CA014",
	L"Pink Sweets: Ibara Sorekara\0\u30d4\u30f3\u30af\u30b9\u30a6\u30a3\u30fc\u30c4 \uff5e\u92f3\u8594\u8587\u305d\u308c\u304b\u3089\uff5e (2006/04/06 MASTER VER....)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, pinkswtsRomInfo, pinkswtsRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1ksDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Pink Sweets: Ibara Sorekara (2006/04/06 MASTER VER...)

static struct BurnRomInfo pinkswtsaRomDesc[] = {
	{ "pnkswtsa_u4",	0x0200000, 0xee3339b2, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "pnkswtsa_u2",	0x8400000, 0x829a862e, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x4b82d250, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xe93f0627, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(pinkswtsa)
STD_ROM_FN(pinkswtsa)

struct BurnDriver BurnDrvPinkswtsa = {
	"pinkswtsa", "pinkswts", NULL, NULL, "2006",
	"Pink Sweets: Ibara Sorekara (2006/04/06 MASTER VER...)\0", NULL, "Cave (AMI license)", "CA014",
	L"Pink Sweets: Ibara Sorekara\0\u30d4\u30f3\u30af\u30b9\u30a6\u30a3\u30fc\u30c4 \uff5e\u92f3\u8594\u8587\u305d\u308c\u304b\u3089\uff5e (2006/04/06 MASTER VER...)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, pinkswtsaRomInfo, pinkswtsaRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1ksDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Pink Sweets: Ibara Sorekara (2006/04/06 MASTER VER.)

static struct BurnRomInfo pinkswtsbRomDesc[] = {
	{ "pnkswtsb_u4",	0x0200000, 0x68bcc009, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "pnkswtsx_u2",	0x8400000, 0x91e4deb2, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x4b82d250, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xe93f0627, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(pinkswtsb)
STD_ROM_FN(pinkswtsb)

struct BurnDriver BurnDrvPinkswtsb = {
	"pinkswtsb", "pinkswts", NULL, NULL, "2006",
	"Pink Sweets: Ibara Sorekara (2006/04/06 MASTER VER.)\0", NULL, "Cave (AMI license)", "CA014",
	L"Pink Sweets: Ibara Sorekara\0\u30d4\u30f3\u30af\u30b9\u30a6\u30a3\u30fc\u30c4 \uff5e\u92f3\u8594\u8587\u305d\u308c\u304b\u3089\uff5e (2006/04/06 MASTER VER.)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, pinkswtsbRomInfo, pinkswtsbRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1ksDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Pink Sweets: Ibara Sorekara (2006/xx/xx MASTER VER.)

static struct BurnRomInfo pinkswtsxRomDesc[] = {
	{ "pnkswtsx_u4",	0x0200000, 0x8fe05bf0, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "pnkswtsx_u2",	0x8400000, 0x91e4deb2, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x4b82d250, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xe93f0627, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(pinkswtsx)
STD_ROM_FN(pinkswtsx)

struct BurnDriver BurnDrvPinkswtsx = {
	"pinkswtsx", "pinkswts", NULL, NULL, "2006",
	"Pink Sweets: Ibara Sorekara (2006/xx/xx MASTER VER.)\0", NULL, "Cave (AMI license)", "CA014",
	L"Pink Sweets: Ibara Sorekara\0\u30d4\u30f3\u30af\u30b9\u30a6\u30a3\u30fc\u30c4 \uff5e\u92f3\u8594\u8587\u305d\u308c\u304b\u3089\uff5e (2006/xx/xx MASTER VER.)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, pinkswtsxRomInfo, pinkswtsxRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1ksDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Pink Sweets: Suicide Club (2017/10/31 SUICIDECLUB VER., bootleg)

static struct BurnRomInfo pinkswtsscRomDesc[] = {
	{ "suicideclub.u4",	0x0200000, 0x5e03662f, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "suicideclub.u2",	0x8400000, 0x32324608, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x4b82d250, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xe93f0627, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(pinkswtssc)
STD_ROM_FN(pinkswtssc)

struct BurnDriver BurnDrvPinkswtssc = {
	"pinkswtssc", "pinkswts", NULL, NULL, "2017",
	"Pink Sweets: Suicide Club (2017/10/31 SUICIDECLUB VER., bootleg)\0", NULL, "bootleg (Four Horsemen)", "CA014",
	L"Pink Sweets: Suicide Club\0\u30d4\u30f3\u30af\u30b9\u30a6\u30a3\u30fc\u30c4 \uff5e\u92f3\u8594\u8587\u305d\u308c\u304b\u3089\uff5e (2017/10/31 SUICIDECLUB VER., bootleg)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, 0,
	NULL, pinkswtsscRomInfo, pinkswtsscRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1ksDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// DoDonPachi Dai-Fukkatsu Ver 1.5 (2008/06/23 MASTER VER 1.5)

static struct BurnRomInfo ddpdfkRomDesc[] = {
	{ "ddpdfk_u4",		0x0400000, 0x9976d699, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "ddpdfk_u2",		0x8400000, 0x84a51a4f, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x27032cde, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xa6178c2c, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(ddpdfk)
STD_ROM_FN(ddpdfk)

struct BurnDriver BurnDrvDdpdfk = {
	"ddpdfk", NULL, NULL, NULL, "2008",
	"DoDonPachi Dai-Fukkatsu Ver 1.5 (2008/06/23 MASTER VER 1.5)\0", NULL, "Cave (AMI license)", "CA019",
	L"DoDonPachi Dai-Fukkatsu Ver 1.5\0\u6012\u9996\u9818\u8702 \u5927\u5fa9\u6d3b Ver 1.5 (2008/06/23 MASTER VER 1.5)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddpdfkRomInfo, ddpdfkRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// DoDonPachi Dai-Fukkatsu Ver 1.0 (2008/05/16 MASTER VER)

static struct BurnRomInfo ddpdfk10RomDesc[] = {
	{ "ddpdfk10_u4",	0x0400000, 0xa3d650b2, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "ddpdfk10_u2",	0x8400000, 0xd349cb2a, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x27032cde, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0xa6178c2c, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(ddpdfk10)
STD_ROM_FN(ddpdfk10)

struct BurnDriver BurnDrvDdpdfk10 = {
	"ddpdfk10", "ddpdfk", NULL, NULL, "2008",
	"DoDonPachi Dai-Fukkatsu Ver 1.0 (2008/05/16 MASTER VER)\0", NULL, "Cave (AMI license)", "CA019",
	L"DoDonPachi Dai-Fukkatsu Ver 1.0\0\u6012\u9996\u9818\u8702 \u5927\u5fa9\u6d3b Ver 1.0 (2008/05/16 MASTER VER)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddpdfk10RomInfo, ddpdfk10RomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Deathsmiles MegaBlack Label (2008/10/06 MEGABLACK LABEL VER)

static struct BurnRomInfo dsmblRomDesc[] = {
	{ "u4",				0x0400000, 0x77fc5ad1, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "u2",				0x8400000, 0xd6b85b7a, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0xa9536a6a, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0x3b673326, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(dsmbl)
STD_ROM_FN(dsmbl)

struct BurnDriver BurnDrvDsmbl = {
	"dsmbl", NULL, NULL, NULL, "2008",
	"Deathsmiles MegaBlack Label (2008/10/06 MEGABLACK LABEL VER)\0", NULL, "Cave (AMI license)", "CA017B",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_HORSHOOT, 0,
	NULL, dsmblRomInfo, dsmblRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};


// DoDonPachi Dai-Fukkatsu Black Label (2010/1/18 BLACK LABEL)

static struct BurnRomInfo dfkblRomDesc[] = {
	{ "u4",				0x0400000, 0x8092ca9d, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "u2",				0x8400000, 0x29f9d73a, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x36d4093b, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0x31f9eb0a, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(dfkbl)
STD_ROM_FN(dfkbl)

struct BurnDriver BurnDrvDfkbl = {
	"dfkbl", NULL, NULL, NULL, "2010",
	"DoDonPachi Dai-Fukkatsu Black Label (2010/1/18 BLACK LABEL)\0", NULL, "Cave", "CA019B",
	L"DoDonPachi Dai-Fukkatsu Black Label\0\u6012\u9996\u9818\u8702 \u5927\u5fa9\u6d3b (2010/1/18 BLACK LABEL)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, dfkblRomInfo, dfkblRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// DoDonPachi SaiDaiOuJou (2012/ 4/20)

static struct BurnRomInfo ddpsdojRomDesc[] = {
	{ "u4",				0x0400100, 0xe2a4411c, 2 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "u2",				0x8400000, 0x668e4cd6, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400100, 0xac94801c, 4 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400100, 0xf593045b, 4 | BRF_SND },           //  3
};

STD_ROM_PICK(ddpsdoj)
STD_ROM_FN(ddpsdoj)

struct BurnDriver BurnDrvDdpsdoj = {
	"ddpsdoj", NULL, NULL, NULL, "2012",
	"DoDonPachi SaiDaiOuJou (2012/ 4/20)\0", NULL, "Cave", "CA???",
	L"DoDonPachi SaiDaiOuJou\0\u6012\u9996\u9818\u8702 \u6700\u5927\u5f80\u751f (2012/4/20)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, ddpsdojRomInfo, ddpsdojRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// DoDonPachi SaiDaiOuJou & Knuckles (CaveDwellers Hack)

static struct BurnRomInfo sdojakRomDesc[] = {
	{ "sdojak.u4",		0x0400000, 0xa878ff4c, 2 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "sdojak.u2",		0x8400000, 0x54353425, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x32b91544, 4 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0x7b9e749f, 4 | BRF_SND },           //  3
};

STD_ROM_PICK(sdojak)
STD_ROM_FN(sdojak)

struct BurnDriver BurnDrvsdojak = {
	"sdojak", "ddpsdoj", NULL, NULL, "2021",
	"DoDonPachi SaiDaiOuJou & Knuckles (CaveDwellers Hack)\0", NULL, "hack", "CA???",
	L"DoDonPachi SaiDaiOuJou & Knuckles (CaveDwellers Hack)\0\u6012\u9996\u9818\u8702 \u6700\u5927\u5f80\u751f & \u30ca\u30c3\u30af\u30eb\u30ba (CaveDwellers, 2021/12/01)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_CAVE_CV1000, GBF_VERSHOOT, FBF_DONPACHI,
	NULL, sdojakRomInfo, sdojakRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	240, 320, 3, 4
};


// Akai Katana (2010/ 8/13 MASTER VER.)

static struct BurnRomInfo akatanaRomDesc[] = {
	{ "u4",				0x0400000, 0x613fd380, 1 | BRF_PRG | BRF_ESS }, //  0 SH3 Code

	{ "u2",				0x8400000, 0x89a2e1a5, 2 | BRF_PRG | BRF_ESS }, //  1 Flash

	{ "u23",			0x0400000, 0x34a67e24, 3 | BRF_SND },           //  2 YMZ770 Samples
	{ "u24",			0x0400000, 0x10760fed, 3 | BRF_SND },           //  3
};

STD_ROM_PICK(akatana)
STD_ROM_FN(akatana)

struct BurnDriver BurnDrvAkatana = {
	"akatana", NULL, NULL, NULL, "2010",
	"Akai Katana (2010/ 8/13 MASTER VER.)\0", NULL, "Cave", "CA021",
	L"Akai Katana\0\u8d64\u3044\u5200 (2010/ 8/13 MASTER VER.)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAVE_CV1000, GBF_HORSHOOT, 0,
	NULL, akatanaRomInfo, akatanaRomName, NULL, NULL, NULL, NULL, Cv1kInputInfo, Cv1kDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	320, 240, 4, 3
};
