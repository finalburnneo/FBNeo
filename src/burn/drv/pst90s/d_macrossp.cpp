// FB Alpha Macross Plus driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "es5506.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM0;
static UINT8 *Drv68KROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvTransTab[5];
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM2;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM[4];
static UINT8 *DrvZoomRAM[4];
static UINT8 *DrvVidReg[4];
static UINT8 *DrvPalRAM;
static UINT8 *Drv68KRAM0;
static UINT8 *Drv68KRAM1;
static UINT8 *DrvSprBuf[2];

static UINT16 *tilemaps[4];
static UINT8 *dirty_tiles[4];
static INT32 dirty_layer[4];

static UINT32 *Palette;
static UINT32 *DrvPalette;
static UINT8   DrvRecalc;

static UINT8 palette_fade;
static UINT16 soundlatch;
static UINT8 sound_pending;
static UINT8 sound_toggle;

static UINT8 DrvJoy1[32];
static UINT8 DrvJoy2[32];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT32 DrvInputs[2];

static INT32 color_mask[3];
static INT32 volume_mute = 0;

static struct BurnInputInfo MacrosspInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 22,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 23,	"p1 fire 4"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 30,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 31,	"p2 fire 4"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Macrossp)

static struct BurnDIPInfo MacrosspDIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL			},
	{0x17, 0xff, 0xff, 0xbf, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x16, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x16, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x16, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x16, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x16, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x16, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x16, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x16, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x16, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x16, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x16, 0x01, 0x0f, 0x09, "1 Coins/7 Credits"	},
	{0x16, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x16, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0xf0, 0x00, "5 Coins 3 Credits"	},
	{0x16, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x16, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x16, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x16, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x16, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x16, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x16, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x16, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x16, 0x01, 0xf0, 0x90, "1 Coins/7 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x17, 0x01, 0x03, 0x02, "Easy"			},
	{0x17, 0x01, 0x03, 0x03, "Normal"		},
	{0x17, 0x01, 0x03, 0x01, "Hard"			},
	{0x17, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x17, 0x01, 0x0c, 0x00, "2"			},
	{0x17, 0x01, 0x0c, 0x0c, "3"			},
	{0x17, 0x01, 0x0c, 0x08, "4"			},
	{0x17, 0x01, 0x0c, 0x04, "5"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x17, 0x01, 0x10, 0x00, "Off"			},
	{0x17, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x17, 0x01, 0x20, 0x20, "Off"			},
	{0x17, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x17, 0x01, 0x40, 0x40, "Japanese"		},
	{0x17, 0x01, 0x40, 0x00, "English"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x17, 0x01, 0x80, 0x80, "Off"			},
	{0x17, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Macrossp)

static void __fastcall macrossp_main_write_long(UINT32 /*address*/, UINT32 /*data*/)
{
//	if (address != 0xfe0000 && (address < 0x900000 || address >= 0x920000))
//		bprintf (0, _T("MWL: %6.6x, %8.8x\n"), address,data);
}

static void __fastcall macrossp_main_write_byte(UINT32 /*address*/, UINT8/* data*/)
{
//	if (address != 0x90000b)
//		bprintf (0, _T("MWB: %6.6x, %2.2x\n"), address,data);
}

static UINT32 __fastcall macrossp_main_read_long(UINT32 address)
{
//	if (address != 0xfe0000 && address != 0xb00000)
//		bprintf (0, _T("MRL: %6.6x\n"), address);

	switch (address)
	{
		case 0xb00000:
			return DrvInputs[0];
	}

	return 0;
}

static void __fastcall macrossp_main_write_word(UINT32 address, UINT16 data)
{
//	bprintf (0, _T("MWW: %6.6x, %4.4x\n"), address, data);

	switch (address)
	{
		case 0xb00010:
	//	case 0xb00012:
		{
			data &= 0xff;
			if (data != 0xff) {
				palette_fade = (UINT8)(0xff - ((data & 0xff) - 40) / 212.0 * 255.0);
				DrvRecalc = 1;
			}
		}
		return;

		case 0xc00000:
			soundlatch = data;
			sound_pending = 1;
			SekClose();
			SekOpen(1);
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
			SekClose();
			SekOpen(0);
		return;
	}
}

static UINT16 __fastcall macrossp_main_read_word(UINT32 address)
{
//	bprintf (0, _T("MRW: %6.6x\n"), address);

	switch (address)
	{
		case 0xb00000:
			return DrvInputs[0] >> 16;

		case 0xb00002:
			return DrvInputs[0];

		case 0xb00004:
			sound_toggle ^= 1;
			return sound_toggle + (sound_pending << 1);

		case 0xb0000c:
			return DrvDips[0] | (DrvDips[1] << 8);
	}

	return 0;
}

static UINT8 __fastcall macrossp_main_read_byte(UINT32 address)
{
//	bprintf (0, _T("MRB: %6.6x\n"), address);

	switch (address)
	{
		case 0xb00007:
			sound_toggle ^= 1;
			return sound_toggle + (sound_pending << 1);
	}

	return 0;
}

static void __fastcall macrossp_vidram_write_long(UINT32 address, UINT32 data)
{
	INT32 select = (address / 0x8000) & 3;
	address = (address & 0x3ffc) / 4;

	UINT32 *ram = (UINT32*)DrvVidRAM[select];

	data = (data << 16) | (data >> 16);

	if (ram[address] != data) {
		ram[address] = data;
		dirty_tiles[select][address/1] = 1;
		dirty_layer[select] = 1;
	}
}

static void __fastcall macrossp_vidram_write_word(UINT32 address, UINT16 data)
{
	INT32 select = (address / 0x8000) & 3;

	address = (address & 0x3ffe) / 2;

	UINT16 *ram = (UINT16*)DrvVidRAM[select];

	if (ram[address] != data) {
		ram[address] = data;
		dirty_tiles[select][address/2] = 1;
		dirty_layer[select] = 1;
	}
}


static void __fastcall macrossp_vidram_write_byte(UINT32 address, UINT8 data)
{
	INT32 select = (address / 0x8000) & 3;

	address &= 0x3fff;

	if (DrvVidRAM[select][address^1] != data) {
		DrvVidRAM[select][address^1] = data;
		dirty_tiles[select][address/4] = 1;
		dirty_layer[select] = 1;
	}
}

static inline void palette_write(INT32 offset)
{
	UINT32 p = *((UINT32*)(DrvPalRAM + offset));

	UINT8 r = p >>  8;
	UINT8 g = p >>  0;
	UINT8 b = p >> 24;

	r = (r * palette_fade) / 255;
	g = (g * palette_fade) / 255;
	b = (b * palette_fade) / 255;

	Palette[offset/4] = (r * 0x10000) + (g * 0x100) + b;
	DrvPalette[offset/4] = BurnHighCol(r,g,b,0);
}

static void __fastcall macrossp_palette_write_long(UINT32 address, UINT32 data)
{
	*((UINT32*)(DrvPalRAM + (address & 0x3ffc))) = (data << 16) | (data >> 16);
	palette_write(address & 0x3ffc);

}

static void __fastcall macrossp_palette_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(DrvPalRAM + (address & 0x3ffe))) = data;
	palette_write(address & 0x3ffc);
}

static void __fastcall macrossp_palette_write_byte(UINT32 address, UINT8 data)
{
	DrvPalRAM[(address & 0x3fff)^1] = data;
	palette_write(address & 0x3ffc);
}

static UINT8 __fastcall macrossp_sound_read_byte(UINT32 /*address*/)
{
//	bprintf (0, _T("SRB: %6.6x\n"), address);

	return 0;
}

static void __fastcall macrossp_sound_write_byte(UINT32 /*ddress*/, UINT8 /*data*/)
{
//	bprintf (0, _T("SWB: %6.6x, %2.2x\n"), address, data);
}

static void __fastcall macrossp_sound_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff80) == 0x400000) {
		ES5506Write((address / 2) & 0x3f, data);
	}
}

static UINT16 __fastcall macrossp_sound_read_word(UINT32 address)
{
	if ((address & 0xffff80) == 0x400000) {
		return ES5506Read((address / 2) & 0x3f);
	}

	switch (address)
	{
		case 0x600000:
			sound_pending = 0;
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( text )
{
	UINT32 *ram = (UINT32*)(DrvVidRAM[3]);

	UINT16 code = ram[offs] >> 16;
	INT32 color = (ram[offs] >> 1) & 0x7f;

	TILE_SET_INFO(4, code, color, DrvTransTab[4][code] ? TILE_SKIP : 0);
}

static tilemap_callback( scra )
{
	UINT32 *ram = (UINT32*)(DrvVidRAM[0]);

	UINT16 attr = ram[offs];
	UINT16 code = ram[offs] >> 16;

	INT32 color;
	if (color_mask[0] == 7)
		color = (attr << 1) & 0x1c;
	else
		color = (attr >> 1) & 0x1f;

	INT32 flags = TILE_FLIPYX(attr >> 14);

	flags |= DrvTransTab[1][code] ? TILE_SKIP : 0;

	TILE_SET_INFO(1, code, color, flags);
}

static tilemap_callback( scrb )
{
	UINT32 *ram = (UINT32*)(DrvVidRAM[1]);

	UINT16 attr = ram[offs];
	UINT16 code = ram[offs] >> 16;

	INT32 color;
	if (color_mask[1] == 7)
		color = (attr << 1) & 0x1c;
	else
		color = (attr >> 1) & 0x1f;

	INT32 flags = TILE_FLIPYX(attr >> 14);

	flags |= DrvTransTab[2][code] ? TILE_SKIP : 0;

	TILE_SET_INFO(2, code, color, flags);	
}

static tilemap_callback( scrc )
{
	UINT32 *ram = (UINT32*)(DrvVidRAM[2]);

	UINT16 attr = ram[offs];
	UINT16 code = ram[offs] >> 16;

	INT32 color;
	if (color_mask[2] == 7)
		color = (attr << 1) & 0x1c;
	else
		color = (attr >> 1) & 0x1f;

	INT32 flags = TILE_FLIPYX(attr >> 14);

	flags |= DrvTransTab[3][code] ? TILE_SKIP : 0;

	TILE_SET_INFO(3, code, color, flags);	
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	memset (dirty_tiles[0], 1, 64*64);
	memset (dirty_tiles[1], 1, 64*64);
	memset (dirty_tiles[2], 1, 64*64);
	dirty_layer[0] = dirty_layer[1] = dirty_layer[2] = 1;

	SekOpen(0);
	SekReset();
	SekClose();

	SekOpen(1);
	SekReset();
	SekClose();

	ES5506Reset();

	volume_mute = 60;
	ES5506SetRoute(0, 0.00, BURN_SND_ES5506_ROUTE_BOTH);

	palette_fade = 0xff; // quizmoon doesn't initialize!
	soundlatch = 0;
	sound_pending = 0;
	sound_toggle = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM0		= Next; Next += 0x400000;
	Drv68KROM1		= Next; Next += 0x100000;

	DrvGfxROM0		= Next; Next += 0x1000000;
	DrvGfxROM1		= Next; Next += 0x800000;
	DrvGfxROM2		= Next; Next += 0x800000;
	DrvGfxROM3		= Next; Next += 0x800000;
	DrvGfxROM4		= Next; Next += 0x100000;

	DrvTransTab[0]		= Next; Next += 0x1000000 / 0x100;
	DrvTransTab[1]		= Next; Next += 0x0800000 / 0x100;
	DrvTransTab[2]		= Next; Next += 0x0800000 / 0x100;
	DrvTransTab[3]		= Next; Next += 0x0800000 / 0x100;
	DrvTransTab[4]		= Next; Next += 0x0100000 / 0x100;

	DrvSndROM0		= Next; Next += 0x800000; // ES5506 Banks 0 & 1
	DrvSndROM2		= Next; Next += 0x800000; // 2 & 3

	Palette			= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);
	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x003000;
	DrvSprBuf[0]		= Next; Next += 0x003000;
	DrvSprBuf[1]		= Next; Next += 0x003000;
	DrvVidRAM[0]		= Next; Next += 0x004000;
	DrvZoomRAM[0]		= Next; Next += 0x000400;
	DrvVidReg[0]		= Next; Next += 0x000400;
	DrvVidRAM[1]		= Next; Next += 0x004000;
	DrvZoomRAM[1]		= Next; Next += 0x000400;
	DrvVidReg[1]		= Next; Next += 0x000400;
	DrvVidRAM[2]		= Next; Next += 0x004000;
	DrvZoomRAM[2]		= Next; Next += 0x000400;
	DrvVidReg[2]		= Next; Next += 0x000400;
	DrvVidRAM[3]		= Next; Next += 0x004000;
	DrvZoomRAM[3]		= Next; Next += 0x000400;
	DrvVidReg[3]		= Next; Next += 0x000400;
	DrvPalRAM		= Next; Next += 0x004000;
	Drv68KRAM0		= Next; Next += 0x020000;
	Drv68KRAM1		= Next; Next += 0x008000;

	RamEnd			= Next;

	tilemaps[0]		= (UINT16*)Next; Next += (16 * 64) * (16 * 64) * 2;
	tilemaps[1]		= (UINT16*)Next; Next += (16 * 64) * (16 * 64) * 2;
	tilemaps[2]		= (UINT16*)Next; Next += (16 * 64) * (16 * 64) * 2;

	dirty_tiles[0]		= Next; Next += 64 * 64;
	dirty_tiles[1]		= Next; Next += 64 * 64;
	dirty_tiles[2]		= Next; Next += 64 * 64;

	MemEnd			= Next;

	return 0;
}

static void DrvBuildTransTab(INT32 tab, UINT8 *rom, INT32 len)
{
	for (INT32 i = 0; i < len; i+= 16*16)
	{
		DrvTransTab[tab][len/(16*16)] = 1;

		for (INT32 j = 0; j < 16*16; j++) {
			if (rom[i+j]) {
				DrvTransTab[tab][len/(16*16)] = 0;
				break;
			}
		}
	}
}

static void DrvGfxExpand(UINT8 *rom, INT32 len)
{
	for (INT32 i = (len - 1) * 2; i >= 0; i -= 2)
	{
		rom[i+0] = rom[i/2] >> 4;
		rom[i+1] = rom[i/2] & 0xf;
	}
}

static INT32 DrvInit(INT32 select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (select == 0) // macrossp
	{
		if (BurnLoadRom(Drv68KROM0 + 0x000002,  0, 4)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x000003,  1, 4)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x000000,  2, 4)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x000001,  3, 4)) return 1;

		if (BurnLoadRom(Drv68KROM1 + 0x000000,  4, 2)) return 1;
		if (BurnLoadRom(Drv68KROM1 + 0x000001,  5, 2)) return 1;

	//	if (BurnLoadRom(Drv68KROM0 + 0x000000,  6, 4)) return 1; // unused bios?

		if (BurnLoadRom(DrvGfxROM0 + 0x000003,  7, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000002,  8, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  9, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000, 10, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x400000, 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x400000, 14, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x400000, 16, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x000000, 17, 1)) return 1;

		memset (DrvSndROM0, 0xff, 0x800000);
		if (BurnLoadRom(DrvSndROM0 + 0x000001, 18, 2)) return 1;

		DrvGfxExpand(DrvGfxROM4, 0x80000);
	}
	else
	{
		if (BurnLoadRom(Drv68KROM0 + 0x000002,  0, 4)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x000003,  1, 4)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x000000,  2, 4)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x000001,  3, 4)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x200002,  4, 4)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x200003,  5, 4)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x200000,  6, 4)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x200001,  7, 4)) return 1;

		if (BurnLoadRom(Drv68KROM1 + 0x000000,  8, 2)) return 1;
		if (BurnLoadRom(Drv68KROM1 + 0x000001,  9, 2)) return 1;

	//	if (BurnLoadRom(Drv68KROM0 + 0x000000, 10, 4)) return 1; // unused bios?

		if (BurnLoadRom(DrvGfxROM0 + 0x000003, 11, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000002, 12, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001, 13, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000, 14, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, 15, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 16, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000, 17, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000001, 18, 2)) return 1;
		if (BurnLoadRom(DrvSndROM0 + 0x000000, 19, 2)) return 1;

		if (BurnLoadRom(DrvSndROM2 + 0x000001, 20, 2)) return 1;
		if (BurnLoadRom(DrvSndROM2 + 0x000000, 21, 2)) return 1;
	}

	DrvBuildTransTab(0, DrvGfxROM0, 0x1000000);
	DrvBuildTransTab(1, DrvGfxROM1, 0x0800000);
	DrvBuildTransTab(2, DrvGfxROM2, 0x0800000);
	DrvBuildTransTab(3, DrvGfxROM3, 0x0800000);
	DrvBuildTransTab(4, DrvGfxROM4, 0x0100000);

	SekInit(0, 0x68ec020);
	SekOpen(0);
	SekMapMemory(Drv68KROM0,		0x000000, 0x3fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,			0x800000, 0x802fff, MAP_RAM);
	SekMapMemory(DrvVidRAM[0],		0x900000, 0x903fff, MAP_RAM); // handler
	SekMapMemory(DrvZoomRAM[0],		0x904000, 0x9043ff, MAP_RAM); // + 0x200
	SekMapMemory(DrvVidReg[0],		0x905000, 0x9053ff, MAP_RAM); // 0-b
	SekMapMemory(DrvVidRAM[1],		0x908000, 0x90bfff, MAP_RAM); // handler
	SekMapMemory(DrvZoomRAM[1],		0x90c000, 0x90c3ff, MAP_RAM); // + 0x200
	SekMapMemory(DrvVidReg[1],		0x90d000, 0x90d3ff, MAP_RAM); // 0-b
	SekMapMemory(DrvVidRAM[1],		0x910000, 0x913fff, MAP_RAM); // handler
	SekMapMemory(DrvZoomRAM[2],		0x914000, 0x9143ff, MAP_RAM); // + 0x200
	SekMapMemory(DrvVidReg[2],		0x915000, 0x9153ff, MAP_RAM); // 0-b
	SekMapMemory(DrvVidRAM[3],		0x918000, 0x91bfff, MAP_RAM);
	SekMapMemory(DrvZoomRAM[3],		0x91c000, 0x91c3ff, MAP_RAM); // + 0x200
	SekMapMemory(DrvVidReg[3],		0x91d000, 0x91d3ff, MAP_RAM); // 0-b
	SekMapMemory(DrvPalRAM,			0xa00000, 0xa03fff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,		0xf00000, 0xf1ffff, MAP_RAM);
	SekSetWriteLongHandler(0,		macrossp_main_write_long);
	SekSetWriteWordHandler(0,		macrossp_main_write_word);
	SekSetWriteByteHandler(0,		macrossp_main_write_byte);
	SekSetReadLongHandler(0,		macrossp_main_read_long);
	SekSetReadWordHandler(0,		macrossp_main_read_word);
	SekSetReadByteHandler(0,		macrossp_main_read_byte);

	SekMapHandler(1,			0x900000, 0x903fff, MAP_WRITE);
	SekSetWriteLongHandler(1,		macrossp_vidram_write_long);
	SekSetWriteWordHandler(1,		macrossp_vidram_write_word);
	SekSetWriteByteHandler(1,		macrossp_vidram_write_byte);

	SekMapHandler(2,			0x908000, 0x90bfff, MAP_WRITE);
	SekSetWriteLongHandler(2,		macrossp_vidram_write_long);
	SekSetWriteWordHandler(2,		macrossp_vidram_write_word);
	SekSetWriteByteHandler(2,		macrossp_vidram_write_byte);

	SekMapHandler(3,			0x910000, 0x913fff, MAP_WRITE);
	SekSetWriteLongHandler(3,		macrossp_vidram_write_long);
	SekSetWriteWordHandler(3,		macrossp_vidram_write_word);
	SekSetWriteByteHandler(3,		macrossp_vidram_write_byte);

	SekMapHandler(4,			0xa00000, 0xa03fff, MAP_WRITE);
	SekSetWriteLongHandler(4,		macrossp_palette_write_long);
	SekSetWriteWordHandler(4,		macrossp_palette_write_word);
	SekSetWriteByteHandler(4,		macrossp_palette_write_byte);
	SekClose();

	SekInit(1, 0x68000);
	SekOpen(1);
	SekMapMemory(Drv68KROM1,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM1,		0x200000, 0x207fff, MAP_RAM);
	SekSetWriteWordHandler(0,		macrossp_sound_write_word);
	SekSetWriteByteHandler(0,		macrossp_sound_write_byte);
	SekSetReadWordHandler(0,		macrossp_sound_read_word);
	SekSetReadByteHandler(0,		macrossp_sound_read_byte);
	SekClose();

	ES5506Init(16000000, DrvSndROM0, DrvSndROM0 + 0x400000, DrvSndROM2, DrvSndROM2 + 0x400000, NULL);
	ES5506SetRoute(0, 0.00, BURN_SND_ES5506_ROUTE_BOTH); // start at 0 because loud BRRRRPPPP! at reset.

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, text_map_callback, 16, 16, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, scra_map_callback, 16, 16, 64, 64);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, scrb_map_callback, 16, 16, 64, 64);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, scrc_map_callback, 16, 16, 64, 64);
	GenericTilemapSetGfx(1, DrvGfxROM1, 6, 16, 16, 0x800000, 0x800, 0x1f); // actually 8bpp, not 6pp! this is fine!
	GenericTilemapSetGfx(2, DrvGfxROM2, 6, 16, 16, 0x800000, 0x800, 0x1f); // actually 8bpp, not 6pp! this is fine!
	GenericTilemapSetGfx(3, DrvGfxROM3, 6, 16, 16, 0x800000, 0x800, 0x1f); // actually 8bpp, not 6pp! this is fine!
	GenericTilemapSetGfx(4, DrvGfxROM4, 4, 16, 16, 0x100000, 0x800, 0x7f);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapSetTransparent(3, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	
	ES5506Exit();

	BurnFree(AllMem);

	return 0;
}

static void palette_update()
{
	UINT32 *p = (UINT32*)DrvPalRAM;

	for (INT32 i = 0; i < 0x4000/4; i++)
	{
		UINT8 r = p[i] >>  8;
		UINT8 g = p[i] >>  0;
		UINT8 b = p[i] >> 24;

		r = (r * palette_fade) / 255;
		g = (g * palette_fade) / 255;
		b = (b * palette_fade) / 255;

		Palette[i] = (r * 0x10000) + (g * 0x100) + b;
		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void tilemap_update(INT32 map)
{
	if (dirty_layer[map] == 0) return;

	dirty_layer[map] = 0;

	UINT32 *ram = (UINT32*)DrvVidRAM[map];

	UINT8 *gfxbase[3] = { DrvGfxROM1, DrvGfxROM2, DrvGfxROM3 };

	INT32 fliptab[4] = { 0, 0x0f, 0xf0, 0xff };

	for (INT32 offs = 0; offs < 64 * 64; offs++)
	{
		if (dirty_tiles[map][offs] == 0) continue;
		dirty_tiles[map][offs] = 0;

		INT32 sx = (offs & 0x3f) * 16;
		INT32 sy = (offs / 0x40) * 16;

		UINT16 attr = ram[offs];
		UINT16 code = (ram[offs] >> 16) & 0x7fff;

		INT32 color;
		if (color_mask[map] == 7)
			color = (attr << 1) & 0x1c;
		else
			color = (attr >> 1) & 0x1f;

		color = (color * 0x40) + 0x800;
		UINT8 *gfx = gfxbase[map] + (code * 0x100);
		UINT16 *dst = tilemaps[map] + (sy * 0x400) + sx;

		INT32 flip = fliptab[(attr >> 14) & 3];

		for (INT32 y = 0; y < 16; y++)
		{
			for (INT32 x = 0; x < 16; x++)
			{
				INT32 pxl = gfx[((y*16)+x)^flip];

				dst[x] = pxl + color;
				if (pxl == 0) dst[x] |= 0x8000;
			}

			dst += 0x400;
		}
	}
}

static void draw_roz(UINT16 *sbitmap, INT32 sy, UINT32 startx, UINT32 starty, INT32 incxx, INT32 incyy, INT32 priority)
{
	starty += sy * incyy;
	UINT32 cx = startx;

	UINT16 *src = sbitmap + ((starty >> 16) & 0x3ff) * 0x400;
	UINT8 *pri = pPrioDraw + (sy * nScreenWidth);
	UINT16 *dest = pTransDraw + (sy * nScreenWidth);

	INT32 x = 0;

	while (x < nScreenWidth)
	{
		if ((src[(cx >> 16) & 0x3ff] & 0x8000) == 0)
		{
			*dest = src[(cx >> 16) & 0x3ff];
			*pri = priority;
		}

		cx += incxx;
		x++;
		++dest;
		pri++;
	}
}

static void draw_layer(INT32 layer, INT32 pri)
{
	UINT32 *reg = (UINT32*)DrvVidReg[layer];

	if ((reg[2] & 0xf000) == 0xe000)
	{
		UINT16 *map = (UINT16*)tilemaps[layer];
		UINT32 *lz = (UINT32*)(DrvZoomRAM[layer] + 0x200);

		tilemap_update(layer);

		INT32 startx = (reg[0] & 0x3ff0000);
		INT32 starty = (reg[0] & 0x00003ff) << 16;
		INT32 incy   = (reg[2] & 0x00001ff) << 10;
		INT32 sy     = starty - (240/2) * (incy - 0x10000);

		for (INT32 line = 0; line < 240; line++)
		{
			UINT32 incx;

			if (line & 1)
				incx = (lz[line/2] & 0xffff0000)>>6;
			else
				incx = (lz[line/2] & 0x0000ffff)<<10;
	
			INT32 sx = startx - (368/2) * (incx - 0x10000);

			draw_roz(map, line, sx, sy, incx, incy, 1 << pri);
		}
	}
	else
	{
		GenericTilemapSetScrollX(layer+1, reg[0] >> 16);
		GenericTilemapSetScrollY(layer+1, reg[0] & 0xffff);

		if (nBurnLayer & (1 << (layer+1))) GenericTilemapDraw(layer+1, pTransDraw, 1 << pri);
	}
}

static inline UINT32 alpha_blend(UINT32 d, UINT32 s)
{
	return (((((s & 0xff00ff) * 127) + ((d & 0xff00ff) * 128)) & 0xff00ff00) + ((((s & 0xff00) * 127) + ((d & 0xff00) * 128)) & 0xff0000)) / 0x100;
}

static void render_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, INT32 priority, INT32 alpha)
{
	if (DrvTransTab[0][code & 0xffff])
		return;

	UINT8 *dest = pBurnDraw;
	UINT16 *bg = pTransDraw;
	UINT8 *gfx = DrvGfxROM0;

	// Based on MAME sources for tile zooming
	UINT8 *gfx_base = gfx + (code * width * height);
	int dh = (zoomy * height + 0x8000) / 0x10000;
	int dw = (zoomx * width + 0x8000) / 0x10000;

	priority |= 1 << 31;

	if (dw && dh)
	{
		int dx = (width * 0x10000) / dw;
		int dy = (height * 0x10000) / dh;
		int ex = sx + dw;
		int ey = sy + dh;
		int x_index_base = 0;
		int y_index = 0;

		if (fx) {
			x_index_base = (dw - 1) * dx;
			dx = -dx;
		}

		if (fy) {
			y_index = (dh - 1) * dy;
			dy = -dy;
		}

		for (INT32 y = sy; y < ey; y++)
		{
			UINT8 *src = gfx_base + (y_index / 0x10000) * width;
			UINT8 *dst = dest + (y * nScreenWidth * nBurnBpp);
			UINT16 *bgs = bg + y * nScreenWidth;
			UINT8 *pri = pPrioDraw + y * nScreenWidth;

			if (y >= 0 && y < nScreenHeight) 
			{
				for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
				{
					if (x >= 0 && x < nScreenWidth) {
						INT32 pxl = src[x_index>>16];

						if (pxl) {
							if (alpha == 0x80) {
								if ((priority & (1 << pri[x])) == 0) {
									UINT32 pixel = alpha_blend(Palette[bgs[x]], Palette[pxl + color]);
									PutPix(dst + x * nBurnBpp, BurnHighCol(pixel/0x10000, pixel/0x100, pixel, 0));
								}
								pri[x] = 0x1f;
							} else {
								if ((priority & (1 << pri[x])) == 0) {
									PutPix(dst + x * nBurnBpp, DrvPalette[pxl + color]);
								}
								pri[x] = 0x1f;
							}
						}
					}
	
					x_index += dx;
				}
			}

			y_index += dy;
		}
	}
}

static void draw_sprites()
{
	UINT32 *spriteram = (UINT32*)DrvSprBuf[1];
	UINT32 *source = (spriteram + (0x3000/4)) - 3; /* buffered two frames */
	UINT32 *finish = spriteram;

	while (source >= finish)
	{
		INT32 wide = (source[0] >> 26) & 0xf;
		INT32 high = (source[0] >> 10) & 0xf;
		INT32 xpos = (source[0] >> 16) & 0x3ff;
		INT32 ypos = (source[0] >>  0) & 0x3ff;
		INT32 xzoom = (source[1] >> 16) & 0x3ff;
		INT32 yzoom = (source[1] >>  0) & 0x3ff;

		int col;
		INT32 tileno = (source[2] >> 16);
		INT32 flipx = (source[2] >> 14) & 1;
		INT32 flipy = (source[2] >> 15) & 1;
		INT32 alpha = (source[2] & 0x2000) ? 0x80 : 0xff;
		int loopno = 0;
		int xcnt, ycnt;
		int xoffset, yoffset;
		INT32 pri = (source[2] >> 10) & 3;
		int primask = 0;
		if(pri <= 0) primask |= 0xaaaa;
		if(pri <= 1) primask |= 0xcccc;
		if(pri <= 2) primask |= 0xf0f0;
		if(pri <= 3) primask |= 0xff00;

		switch (source[0] & 0xc0000000)
		{
			case 0x80000000:
				col = (source[2] & 0x38) >> 1;
				break;

			case 0x40000000:
				col = (source[2] & 0xf8) >> 3;
				break;

			default:
				col = rand();
				break;
		}

		if (xpos > 0x1ff) xpos -=0x400;
		if (ypos > 0x1ff) ypos -=0x400;

		/* loop params */
		int ymin = 0;
		int ymax = high+1;
		int yinc = 1;
		int yoffst = 0;
		if(flipy) {
			yoffst = (high * yzoom * 16);
			ymin = high;
			ymax = -1;
			yinc = -1;
		}

		int xmin = 0;
		int xmax = wide+1;
		int xinc = 1;
		int xoffst = 0;
		if(flipx) {
			xoffst = (wide * xzoom * 16);
			xmin = wide;
			xmax = -1;
			xinc = -1;
		}

		yoffset = yoffst;
		for (ycnt = ymin; ycnt != ymax; ycnt += yinc)
		{
			xoffset = xoffst;
			for (xcnt = xmin; xcnt != xmax; xcnt += xinc)
			{
				int fudged_xzoom = xzoom<<8;
				int fudged_yzoom = yzoom<<8;

				if(xzoom < 0x100) fudged_xzoom += 0x600;
				if(yzoom < 0x100) fudged_yzoom += 0x600;

				render_sprite(tileno+loopno, col<<6, xpos+(xoffset>>8), ypos+(yoffset>>8), flipx, flipy, 16, 16, fudged_xzoom, fudged_yzoom, primask, alpha);

				xoffset += ((xzoom*16) * xinc);
				loopno++;
			}
			yoffset += ((yzoom*16) * yinc);
		}

		source -= 3;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		palette_update();
		DrvRecalc = 0;	
	}

	UINT32 reg[3] = { *((UINT32*)DrvVidReg[0]), *((UINT32*)DrvVidReg[1]), *((UINT32*)DrvVidReg[2]) };

	UINT32 layerpri[3] = { (reg[0] >> 30) & 3, (reg[1] >> 30) & 3, (reg[2] >> 30) & 3 };

	INT32 pcmask[3];

	pcmask[0] = color_mask[0];
	pcmask[1] = color_mask[1];
	pcmask[2] = color_mask[2];

	color_mask[0] = (reg[0] & 0x8000000) ? 0x7 : 0x1f;
	color_mask[1] = (reg[1] & 0x8000000) ? 0x7 : 0x1f;
	color_mask[2] = (reg[2] & 0x8000000) ? 0x7 : 0x1f;

	for (INT32 i = 0; i < 3; i++) {
		if (color_mask[i] != pcmask[i]) {
			memset (dirty_tiles[i], 1, 64*64);
			dirty_layer[i] = 1;
		}
	}

	BurnTransferClear();

	for (INT32 pri = 0;  pri <= 3;  pri++)
	{
		for (INT32 layer = 2; layer >= 0; layer--)
		{
			if (layerpri[layer] == (UINT32)pri)
			{
				draw_layer(layer, pri);
			}
		}
	}

	if (nSpriteEnable & 2) GenericTilemapDraw(0, pTransDraw, 8);

	BurnTransferCopy(DrvPalette);

	if (nSpriteEnable & 1) draw_sprites();

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, 2 * sizeof(UINT32));
		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { 25000000 / 60, 16000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		nCyclesDone[0] += SekRun(nCyclesTotal[0] / nInterleave);
		if (i == 240) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
		SekClose();

		SekOpen(1);
		nCyclesDone[1] += SekRun(nCyclesTotal[1] / nInterleave);
		SekClose();
	}

	if (pBurnSoundOut) {
		if (volume_mute) volume_mute--;
		if (volume_mute == 1) {
			ES5506SetRoute(0, 3.00, BURN_SND_ES5506_ROUTE_BOTH);
		}
		ES5506Update(pBurnSoundOut, nBurnSoundLen);
	}
	
	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf[1], DrvSprBuf[0], 0x3000);
	memcpy (DrvSprBuf[0], DrvSprRAM,    0x3000);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029682;
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

		ES5506Scan(nAction, pnMin);

		SCAN_VAR(palette_fade);
		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_pending);
		SCAN_VAR(sound_toggle);
	}

	if (nAction & ACB_WRITE) {
		dirty_layer[0] = 1;
		dirty_layer[1] = 1;
		dirty_layer[2] = 1;
		memset (dirty_tiles[0], 1, 64*64);
		memset (dirty_tiles[1], 1, 64*64);
		memset (dirty_tiles[1], 1, 64*64);
		DrvRecalc = 1;
	}

	return 0;
}

// Macross Plus

static struct BurnRomInfo macrosspRomDesc[] = {
	{ "bp964a-c.u1",	0x080000, 0x39da35e7, 1 | BRF_PRG | BRF_ESS }, //  0 68ec20 code
	{ "bp964a-c.u2",	0x080000, 0x86d0ca6a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bp964a-c.u3",	0x080000, 0xfb895a7b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bp964a-c.u4",	0x080000, 0x8c8b966c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bp964a.u20",		0x080000, 0x12960cbb, 2 | BRF_PRG | BRF_ESS }, //  4 68k code
	{ "bp964a.u21",		0x080000, 0x87bdd2fc, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "bp964a.u49",		0x020000, 0xad203f76, 3 | BRF_OPT },           //  6 Bios? code

	{ "bp964a.u9",		0x400000, 0xbd51a70d, 4 | BRF_GRA },           //  7 Sprites
	{ "bp964a.u10",		0x400000, 0xab84bba7, 4 | BRF_GRA },           //  8
	{ "bp964a.u11",		0x400000, 0xb9ae1d0b, 4 | BRF_GRA },           //  9
	{ "bp964a.u12",		0x400000, 0x8dda1052, 4 | BRF_GRA },           // 10

	{ "bp964a.u13",		0x400000, 0xf4d3c5bf, 5 | BRF_GRA },           // 11 Layer 'A' tiles
	{ "bp964a.u14",		0x400000, 0x4f2dd1b2, 5 | BRF_GRA },           // 12

	{ "bp964a.u15",		0x400000, 0x5b97a870, 6 | BRF_GRA },           // 13 Layer 'B' tiles
	{ "bp964a.u16",		0x400000, 0xc8a0cd64, 6 | BRF_GRA },           // 14

	{ "bp964a.u17",		0x400000, 0xf2470876, 7 | BRF_GRA },           // 15 Layer 'C' tiles
	{ "bp964a.u18",		0x400000, 0x52ef21f3, 7 | BRF_GRA },           // 16

	{ "bp964a.u19",		0x080000, 0x19c7acd9, 8 | BRF_GRA },           // 17 Text tiles

	{ "bp964a.u24",		0x400000, 0x93f90336, 9 | BRF_SND },           // 18 Ensoniq samples, bank 0

	{ "u8.u8",		0x000117, 0x99bd3cc1, 0 | BRF_OPT },           // 19 Unused plds
	{ "u9.u9",		0x000117, 0x480f4860, 0 | BRF_OPT },           // 20
	{ "u200.u200",		0x000117, 0x9343ad76, 0 | BRF_OPT },           // 21
};

STD_ROM_PICK(macrossp)
STD_ROM_FN(macrossp)

static INT32 macrosspInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvMacrossp = {
	"macrossp", NULL, NULL, NULL, "1996",
	"Macross Plus\0", NULL, "MOSS / Banpresto", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, macrosspRomInfo, macrosspRomName, NULL, NULL, MacrosspInputInfo, MacrosspDIPInfo,
	macrosspInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	240, 384, 3, 4
};


// Quiz Bisyoujo Senshi Sailor Moon - Chiryoku Tairyoku Toki no Un

static struct BurnRomInfo quizmoonRomDesc[] = {
	{ "u1.bin",		0x020000, 0xea404553, 1 | BRF_PRG | BRF_ESS }, //  0 68ec20 code
	{ "u2.bin",		0x020000, 0x024eedff, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u3.bin",		0x020000, 0x545b1d17, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "u4.bin",		0x020000, 0x60b3d18c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "u5.bin",		0x080000, 0x4cc65f5e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "u6.bin",		0x080000, 0xd84b7c6c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "u7.bin",		0x080000, 0x656b2125, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "u8.bin",		0x080000, 0x944df309, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "u20.bin",		0x020000, 0xd7ad1ffb, 2 | BRF_PRG | BRF_ESS }, //  8 68k code
	{ "u21.bin",		0x020000, 0x6fc625c6, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "u49.bin",		0x020000, 0x1590ad81, 3 | BRF_GRA },           // 10 Bios? code

	{ "u9.bin",		0x400000, 0xaaaf2ca9, 4 | BRF_GRA },           // 11 Sprites
	{ "u10.bin",		0x400000, 0xf0349691, 4 | BRF_GRA },           // 12
	{ "u11.bin",		0x400000, 0x893ab178, 4 | BRF_GRA },           // 13
	{ "u12.bin",		0x400000, 0x39b731b8, 4 | BRF_GRA },           // 14

	{ "u13.bin",		0x400000, 0x3dcbb041, 5 | BRF_GRA },           // 15 Layer 'A' tiles

	{ "u15.bin",		0x400000, 0xb84224f0, 6 | BRF_GRA },           // 16 Layer 'B' tiles

	{ "u17.bin",		0x200000, 0xff93c949, 7 | BRF_GRA },           // 17 Layer 'C' tiles

	{ "u26.bin",		0x400000, 0x6c8f30d4, 8 | BRF_SND },           // 18 Ensoniq samples, bank 0
	{ "u24.bin",		0x400000, 0x5b12d0b1, 8 | BRF_SND },           // 19

	{ "u27.bin",		0x400000, 0xbd75d165, 9 | BRF_SND },           // 20 Ensoniq samples, bank 2
	{ "u25.bin",		0x400000, 0x3b9689bc, 9 | BRF_SND },           // 21
};

STD_ROM_PICK(quizmoon)
STD_ROM_FN(quizmoon)

static INT32 quizmoonInit()
{
	return DrvInit(1);
}

struct BurnDriverD BurnDrvQuizmoon = {
	"quizmoon", NULL, NULL, NULL, "1997",
	"Quiz Bisyoujo Senshi Sailor Moon - Chiryoku Tairyoku Toki no Un\0", NULL, "Banpresto", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, quizmoonRomInfo, quizmoonRomName, NULL, NULL, MacrosspInputInfo, MacrosspDIPInfo, //QuizmoonInputInfo, QuizmoonDIPInfo,
	quizmoonInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 224, 4, 3
};
