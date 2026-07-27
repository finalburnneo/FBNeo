// FinalBurn Neo Super Pinball Action driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "msm6295.h"
#include "burn_pal.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM[4];
static UINT8 *Drv68KRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvExtraRAM;
static UINT8 *DrvZ80RAM[2];

static UINT32 *DrvPalette32;

static UINT16 scrollx;
static UINT16 scrolly;
static UINT8 soundlatch;
static UINT16 extra_latch;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[3];
static UINT8 DrvReset;

static INT32 nCyclesExtra;

static struct BurnInputInfo SpbactnInputList[] = {
	{"Coin 1",								BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"		},
	{"Coin 2",								BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"		},
	{"Start",								BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"		},
	{"Left Flippers",						BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"		},
	{"Right Flippers",						BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"		},
	{"Launch Ball / Shake (Right side)",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 3"		},
	{"Launch Ball / Shake (Left side)",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 4"		},

	{"Reset",								BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dips A",								BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dips B",								BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
};

STDINPUTINFO(Spbactn)

static struct BurnInputInfo SpbactnpInputList[] = {
	{"Coin 1",								BIT_DIGITAL,	DrvJoy1 + 13,	"p1 coin"		},
	{"Coin 2",								BIT_DIGITAL,	DrvJoy1 + 14,	"p2 coin"		},
	{"Start",								BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"		},
	{"Left Flippers",						BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"		},
	{"Right Flippers",						BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 2"		},
	{"Launch Ball / Shake",					BIT_DIGITAL,	DrvJoy2 + 14,	"p1 fire 3"		},

	{"Reset",								BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dips A",								BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dips B",								BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
};

STDINPUTINFO(Spbactnp)

static struct BurnDIPInfo SpbactnDIPList[]=
{
	DIP_OFFSET(0x08)
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x00, 0x01, 0x07, 0x02, "4 Coins/1 Credit"			},
	{0x00, 0x01, 0x07, 0x03, "3 Coins/1 Credit"			},
	{0x00, 0x01, 0x07, 0x04, "2 Coins/1 Credit"			},
	{0x00, 0x01, 0x07, 0x01, "2 Coins/1 Credit 3/2"		},
	{0x00, 0x01, 0x07, 0x07, "1 Coin/1 Credit"			},
	{0x00, 0x01, 0x07, 0x05, "1 Coin/1 Credit 2/3"		},
	{0x00, 0x01, 0x07, 0x06, "1 Coin/2 Credits"			},
	{0x00, 0x01, 0x07, 0x00, "1 Coin/1 Credit 5/6"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x00, 0x01, 0x38, 0x10, "4 Coins/1 Credit"			},
	{0x00, 0x01, 0x38, 0x18, "3 Coins/1 Credit"			},
	{0x00, 0x01, 0x38, 0x20, "2 Coins/1 Credit"			},
	{0x00, 0x01, 0x38, 0x08, "2 Coins/1 Credit 3/2"		},
	{0x00, 0x01, 0x38, 0x38, "1 Coin/1 Credit"			},
	{0x00, 0x01, 0x38, 0x28, "1 Coin/1 Credit 2/3"		},
	{0x00, 0x01, 0x38, 0x30, "1 Coin/2 Credits"			},
	{0x00, 0x01, 0x38, 0x00, "1 Coin/1 Credit 5/6"		},

	{0   , 0xfe, 0   ,    4, "Balls"					},
	{0x00, 0x01, 0xc0, 0x00, "2"						},
	{0x00, 0x01, 0xc0, 0xc0, "3"						},
	{0x00, 0x01, 0xc0, 0x80, "4"						},
	{0x00, 0x01, 0xc0, 0x40, "5"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0x03, 0x02, "Easy"						},
	{0x01, 0x01, 0x03, 0x03, "Normal"					},
	{0x01, 0x01, 0x03, 0x01, "Hard"						},
	{0x01, 0x01, 0x03, 0x00, "Very Hard"				},

	{0   , 0xfe, 0   ,    4, "Extra Ball"				},
	{0x01, 0x01, 0x0c, 0x04, "100k and 500k"			},
	{0x01, 0x01, 0x0c, 0x0c, "200k and 800k"			},
	{0x01, 0x01, 0x0c, 0x08, "200k"						},
	{0x01, 0x01, 0x0c, 0x00, "None"						},

	{0   , 0xfe, 0   ,    2, "Hit Difficulty"			},
	{0x01, 0x01, 0x10, 0x10, "Normal"					},
	{0x01, 0x01, 0x10, 0x00, "Difficult"				},

	{0   , 0xfe, 0   ,    2, "Display Instructions"		},
	{0x01, 0x01, 0x20, 0x00, "No"						},
	{0x01, 0x01, 0x20, 0x20, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x01, 0x01, 0x40, 0x00, "Off"						},
	{0x01, 0x01, 0x40, 0x40, "On"						},

	{0   , 0xfe, 0   ,    2, "Match"					},
	{0x01, 0x01, 0x80, 0x80, "1/20"						},
	{0x01, 0x01, 0x80, 0x00, "1/40"						},
};

STDDIPINFO(Spbactn)

static struct BurnDIPInfo SpbactnpDIPList[]=
{
	DIP_OFFSET(0x07)
	{0x00, 0xff, 0xff, 0xff, NULL						},
	{0x01, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Balls"					},
	{0x00, 0x01, 0x03, 0x00, "2"						},
	{0x00, 0x01, 0x03, 0x03, "3"						},
	{0x00, 0x01, 0x03, 0x01, "4"						},
	{0x00, 0x01, 0x03, 0x02, "5"						},

	{0   , 0xfe, 0   ,    8, "Coin A"					},
	{0x00, 0x01, 0xe0, 0x40, "4 Coins/1 Credit"			},
	{0x00, 0x01, 0xe0, 0xc0, "3 Coins/1 Credit"			},
	{0x00, 0x01, 0xe0, 0x20, "2 Coins/1 Credit"			},
	{0x00, 0x01, 0xe0, 0x80, "2 Coins/1 Credit 3/2"		},
	{0x00, 0x01, 0xe0, 0xe0, "1 Coin/1 Credit"			},
	{0x00, 0x01, 0xe0, 0xa0, "1 Coin/1 Credit 2/3"		},
	{0x00, 0x01, 0xe0, 0x60, "1 Coin/2 Credits"			},
	{0x00, 0x01, 0xe0, 0x00, "1 Coin/1 Credit 5/6"		},

	{0   , 0xfe, 0   ,    8, "Coin B"					},
	{0x00, 0x01, 0x1c, 0x08, "4 Coins/1 Credit"			},
	{0x00, 0x01, 0x1c, 0x18, "3 Coins/1 Credit"			},
	{0x00, 0x01, 0x1c, 0x04, "2 Coins/1 Credit"			},
	{0x00, 0x01, 0x1c, 0x10, "2 Coins/1 Credit 3/2"		},
	{0x00, 0x01, 0x1c, 0x1c, "1 Coin/1 Credit"			},
	{0x00, 0x01, 0x1c, 0x14, "1 Coin/1 Credit 2/3"		},
	{0x00, 0x01, 0x1c, 0x0c, "1 Coin/2 Credits"			},
	{0x00, 0x01, 0x1c, 0x00, "1 Coin/1 Credit 5/6"		},

	{0   , 0xfe, 0   ,    2, "Match"					},
	{0x01, 0x01, 0x01, 0x01, "1/20"						},
	{0x01, 0x01, 0x01, 0x00, "1/40"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x01, 0x01, 0x02, 0x00, "Off"						},
	{0x01, 0x01, 0x02, 0x02, "On"						},

	{0   , 0xfe, 0   ,    2, "Display Instructions"		},
	{0x01, 0x01, 0x04, 0x00, "No"						},
	{0x01, 0x01, 0x04, 0x04, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Hit Difficulty"			},
	{0x01, 0x01, 0x08, 0x08, "Normal"					},
	{0x01, 0x01, 0x08, 0x00, "Difficult"				},

	{0   , 0xfe, 0   ,    4, "Extra Ball"				},
	{0x01, 0x01, 0x30, 0x20, "100k and 500k"			},
	{0x01, 0x01, 0x30, 0x30, "200k and 800k"			},
	{0x01, 0x01, 0x30, 0x10, "200k"						},
	{0x01, 0x01, 0x30, 0x00, "None"						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x01, 0x01, 0xc0, 0x80, "Easy"						},
	{0x01, 0x01, 0xc0, 0xc0, "Normal"					},
	{0x01, 0x01, 0xc0, 0x40, "Hard"						},
	{0x01, 0x01, 0xc0, 0x00, "Very Hard"				},
};

STDDIPINFO(Spbactnp)

static UINT16 __fastcall spbactn_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x90000:
			return DrvInputs[0];

		case 0x90010:
		case 0x90002: // spbactnp
			return DrvInputs[1];

		case 0x90020:
			return DrvInputs[2]; // sys

		case 0x90006: // spbactnp
			return DrvDips[0] + (DrvDips[1] << 8);

		case 0x90030:
			return DrvDips[1];

		case 0x90040:
			return DrvDips[0];
	}

	return 0;
}

static UINT8 __fastcall spbactn_main_read_byte(UINT32 address)
{
	return SekReadWord(address & ~1) >> ((~address & 1) << 3);
}

static void sync(INT32 which)
{
	ZetOpen(which);
	INT32 cycles = (SekTotalCycles() / 3) - ZetTotalCycles();
	if (cycles > 0) {
		ZetRun(cycles);
	}
	ZetClose();
}

static void __fastcall spbactn_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x90020:
		case 0x90002: // spbactnp
			SekSetIRQLine(3, CPU_IRQSTATUS_NONE);
		return;

		case 0x90124: // spbactnp
			scrolly = data;
		return;

		case 0x9012c: // spbactnp
			scrollx = data;
		return;

		case 0x9000e: // spbactnp
			sync(1);
			extra_latch = data;
			ZetNmi(1);
		return;

		case 0x90010:
		case 0x90006: // spbactnp
			sync(0);
			soundlatch = data;
			ZetNmi(0);
		return;
	}
}

static void __fastcall spbactn_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x90011:
		case 0x90007: // spbactnp
			sync(0);
			soundlatch = data;
			ZetNmi(0);
		return;

		case 0x90020:
		case 0x90021:
		case 0x90002: // spbactnp
		case 0x90003: // spbactnp
			SekSetIRQLine(3, CPU_IRQSTATUS_NONE);
		return;
	}
}

static void __fastcall spbactn_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf800:
			MSM6295Write(0, data);
		return;

		case 0xf810:
		case 0xf811:
			BurnYM3812Write(0, address & 1, data);
		return;
	}
}

static UINT8 __fastcall spbactn_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			return MSM6295Read(0);

		case 0xfc20:
			return soundlatch;
	}

	return 0;
}

static UINT8 __fastcall spbactnp_extra_read(UINT16 address)
{
	switch (address)
	{
		case 0xd800:
			return extra_latch;

		case 0xd801:
			return extra_latch >> 8;
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 color =(*((UINT16*)(DrvBgRAM + 0x0000 + offs * 2)) & 0xf0) >> 4;
	INT32 code  = *((UINT16*)(DrvBgRAM + 0x4000 + offs * 2));

	TILE_SET_INFO(1, code, color, 0);
}

static tilemap_callback( fg )
{
	INT32 attr  = *((UINT16*)(DrvFgRAM + 0x0000 + offs * 2));
	INT32 code  = *((UINT16*)(DrvFgRAM + 0x4000 + offs * 2));
	INT32 color = ((attr & 0xf0) >> 4) | ((attr & 0x08) << 1);

	TILE_SET_INFO(0, code, color, 0);
}

static tilemap_callback( extra )
{
	UINT16 code = DrvExtraRAM[offs] | (DrvExtraRAM[offs + 0x800] << 8);

	TILE_SET_INFO(3, code, code >> 12, 0);
}

static void TecmoFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 TecmoSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(double)ZetTotalCycles() * nSoundRate / 4000000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	MSM6295Reset();
	BurnYM3812Reset();
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	soundlatch = 0;
	scrollx = 0;
	scrolly = 0;

	nCyclesExtra = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x040000;
	DrvZ80ROM[0]		= Next; Next += 0x010000;
	DrvZ80ROM[1]		= Next; Next += 0x010000;

	DrvSndROM			= Next; Next += 0x040000;

	DrvGfxROM[0]		= Next; Next += 0x100000;
	DrvGfxROM[1]		= Next; Next += 0x100000;
	DrvGfxROM[2]		= Next; Next += 0x100000;
	DrvGfxROM[3]		= Next; Next += 0x040000;

	BurnPalette			= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);
	DrvPalette32		= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);

	AllRam				= Next;

	Drv68KRAM			= Next; Next += 0x004000;
	BurnPalRAM			= Next; Next += 0x002a00;
	DrvSprRAM			= Next; Next += 0x001000;
	DrvFgRAM			= Next; Next += 0x008000;
	DrvBgRAM			= Next; Next += 0x008000;
	DrvExtraRAM			= Next; Next += 0x001000;
	DrvZ80RAM[0]		= Next; Next += 0x000800;
	DrvZ80RAM[1]		= Next; Next += 0x000800;

	RamEnd				= Next;
	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes[4] = { STEP4(0,1) };
	INT32 XOffs[16] = { STEP8(0,4), STEP8(256,4) };
	INT32 YOffs[8]  = { STEP8(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x80000);

	GfxDecode(0x2000, 4, 16,  8, Planes, XOffs, YOffs, 0x200, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x80000);

	GfxDecode(0x2000, 4, 16,  8, Planes, XOffs, YOffs, 0x200, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x80000);

	GfxDecode(0x4000, 4,  8,  8, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM[2]);

	memcpy (tmp, DrvGfxROM[3], 0x20000);

	GfxDecode(0x1000, 4,  8,  8, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM[3]);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM + 0x00001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x00000, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 2)) return 1; // fg
		if (BurnLoadRom(DrvGfxROM[0] + 0x00001, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x00001, k++, 2)) return 1; // bg
		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 2)) return 1;
		for (INT32 i = 0; i < 0x80000; i++) DrvGfxROM[1][i] = BITSWAP08(DrvGfxROM[1][i], 0,1,2,3,4,5,6,7);

		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 2)) return 1; // sp
		if (BurnLoadRom(DrvGfxROM[2] + 0x00001, k++, 2)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x00000, 0x3ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x40000, 0x43fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x50000, 0x50fff, MAP_RAM);
	SekMapMemory(DrvFgRAM,			0x60000, 0x67fff, MAP_RAM);
	SekMapMemory(DrvBgRAM,			0x70000, 0x77fff, MAP_RAM);
	SekMapMemory(BurnPalRAM,		0x80000, 0x827ff, MAP_RAM);
	SekSetWriteWordHandler(0,		spbactn_main_write_word);
	SekSetWriteByteHandler(0,		spbactn_main_write_byte);
	SekSetReadWordHandler(0,		spbactn_main_read_word);
	SekSetReadByteHandler(0,		spbactn_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],		0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[0],		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(spbactn_sound_write);
	ZetSetReadHandler(spbactn_sound_read);
	ZetClose();

	// fake
	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[0],		0x0000, 0xffff, MAP_ROM);
	ZetSetHALT(1); // not used by this hw
	ZetClose();

	BurnYM3812Init(1, 4000000, &TecmoFMIRQHandler, &TecmoSynchroniseStream, 0);
	BurnTimerAttach(&ZetConfig, 4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetBank(0, DrvSndROM, 0x00000, 0x3ffff);
	MSM6295SetRoute(0, 0.85, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 8, 64, 128);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 16, 8, 64, 128);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4, 16,  8, 0x100000, 0x000, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16,  8, 0x100000, 0x000, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4,  8,  8, 0x100000, 0x000, 0x3ff);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	BurnBitmapAllocate(1, nScreenWidth, nScreenHeight, true);
	BurnBitmapAllocate(2, nScreenWidth, nScreenHeight, true);
	BurnBitmapAllocate(3, nScreenWidth, nScreenHeight, true);

	DrvDoReset();

	return 0;
}

static INT32 SpbactnpInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM + 0x00001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x00000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x20001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x20000, k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		
		if (BurnLoadRom(DrvSndROM + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 2)) return 1; // fg
		if (BurnLoadRom(DrvGfxROM[0] + 0x00001, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 2)) return 1; // bg
		if (BurnLoadRom(DrvGfxROM[1] + 0x00001, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1; // sp
		if (BurnLoadRom(DrvGfxROM[2] + 0x20000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[3] + 0x00000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,				0x00000, 0x3ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,				0x40000, 0x43fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,				0x50000, 0x50fff, MAP_RAM);
	SekMapMemory(DrvFgRAM,				0x60000, 0x67fff, MAP_RAM);
	SekMapMemory(DrvBgRAM,				0x70000, 0x77fff, MAP_RAM);
	SekMapMemory(BurnPalRAM,			0x80000, 0x827ff, MAP_RAM);
	SekSetWriteWordHandler(0,			spbactn_main_write_word);
	SekSetWriteByteHandler(0,			spbactn_main_write_byte);
	SekSetReadWordHandler(0,			spbactn_main_read_word);
	SekSetReadByteHandler(0,			spbactn_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],			0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[0],			0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(spbactn_sound_write);
	ZetSetReadHandler(spbactn_sound_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],			0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],			0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvExtraRAM,			0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(BurnPalRAM + 0x2800,	0xd000, 0xd1ff, MAP_RAM);
	ZetSetReadHandler(spbactnp_extra_read);
	ZetClose();

	BurnYM3812Init(1, 4000000, &TecmoFMIRQHandler, &TecmoSynchroniseStream, 0);
	BurnTimerAttach(&ZetConfig, 4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetBank(0, DrvSndROM, 0x00000, 0x3ffff);
	MSM6295SetRoute(0, 0.85, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 8, 64, 128);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 16, 8, 64, 128);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, extra_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4, 16,  8, 0x100000, 0x0000, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16,  8, 0x100000, 0x0000, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4,  8,  8, 0x080000, 0x0000, 0x3ff);
	GenericTilemapSetGfx(3, DrvGfxROM[3], 4,  8,  8, 0x040000, 0x1400, 0x0f);
	GenericTilemapSetOffsets(0, 224, 0);
	GenericTilemapSetOffsets(1, 224, 0);
	GenericTilemapSetOffsets(2, 0, 0);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	BurnBitmapAllocate(1, nScreenWidth, nScreenHeight, true);
	BurnBitmapAllocate(2, nScreenWidth, nScreenHeight, true);
	BurnBitmapAllocate(3, nScreenWidth, nScreenHeight, true);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM3812Exit();
	MSM6295Exit();

	SekExit();
	ZetExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites()
{
	INT32 screenxstart = (nScreenWidth > 512) ? 224 : 0;
	INT32 screenystart = (nScreenWidth > 512) ? 0 : 16;
	UINT16 *source = (UINT16*)DrvSprRAM;

	for (INT32 i = 0; i < 0x1000/2; i+=8)
	{
		INT32 attr = source[i + 0];

		if (attr & 4)
		{
			INT32 code  = source[i + 1];
			INT32 color = source[i + 2];
			INT32 ypos  = source[i + 3] & 0x1ff;
			INT32 xpos  = (source[i + 4]) & 0x3ff;

			INT32 flipx = attr & 1;
			INT32 flipy = attr & 2;
			INT32 size = 1 << (color & 3);

			code &= ~((size * size) - 1);
			color = (color >> 4) & 0x0f;
			color |= attr & 0x3f0; // blend bit and priority bits

			if (xpos >= 512) xpos -= 1024;	// wraparound
			if (ypos >= 256) ypos -=  512;

			for (INT32 row = 0; row < size; row++)
			{
				for (INT32 col = 0; col < size; col++)
				{
					INT32 sx = xpos + 8 * (flipx ? (size - 1 - col) : col);
					INT32 sy = ypos + 8 * (flipy ? (size - 1 - row) : row);

					INT32 tile = code + (col & 1) + ((col & 2) << 1) + ((col & 4) << 2) + ((row & 1) << 1) + ((row & 2) << 2) + ((row & 4) << 3);

					DrawGfxMaskTile(3, 2, tile, sx + screenxstart, sy - screenystart, flipx, flipy, color, 0);
				}
			}
		}
	}
}

static UINT16 sum_colors(INT32 c1_idx, INT32 c2_idx)
{
	UINT32 c1 = DrvPalette32[c1_idx];
	UINT32 c2 = DrvPalette32[c2_idx];

	UINT8 c1_r = c1 >> 16;
	UINT8 c1_g = c1 >> 8;
	UINT8 c1_b = c1;

	UINT8 c2_r = c2 >> 16;
	UINT8 c2_g = c2 >> 8;
	UINT8 c2_b = c2;

	UINT8 r = (c1_r + c2_r) >> 3;
	UINT8 g = (c1_g + c2_g) >> 3;
	UINT8 b = (c1_b + c2_b) >> 3;
	if (r > 0x1f) r = 0x1f; // clamp
	if (g > 0x1f) g = 0x1f;
	if (b > 0x1f) b = 0x1f;

	return 0x8000 | (r << 10) | (g << 5) | b;
}

static void mix_bitmaps(INT32 bitmap_bg, INT32 bitmap_fg, INT32 bitmap_sp, INT32 m_sprpri_shift, INT32 m_sprbln_shift, INT32 m_sprcol_shift)
{
	INT32 screenxstart = (nScreenWidth > 512) ? 224 : 0;
	INT32 screenystart = (nScreenWidth > 512) ? 16 : 0;

	for (int y = screenystart; y < nScreenHeight - screenystart; y++)
	{
		UINT16 *dd  = BurnBitmapGetPosition(0, 0, y);
		UINT16 *sd2 = BurnBitmapGetPosition(bitmap_sp, 0, y);
		UINT16 *fg  = BurnBitmapGetPosition(bitmap_fg, 0, y);
		UINT16 *bg  = BurnBitmapGetPosition(bitmap_bg, 0, y);

		for (int x = screenxstart; x < nScreenWidth; x++)
		{
			UINT16 sprpixel = sd2[x];

			UINT16 m_sprpri = (sprpixel >> m_sprpri_shift) & 0x3;
			UINT16 m_sprbln = (sprpixel >> m_sprbln_shift) & 0x1;
			UINT16 m_sprcol = (sprpixel >> m_sprcol_shift) & 0xf;

			sprpixel = (sprpixel & 0xf) | (m_sprcol << 4);

			UINT8 bgpixel = bg[x];
			UINT16 fgpixel = fg[x];
			UINT16 fgbln = fgpixel & 0x100;
			fgpixel &= 0xff;

			if (sprpixel & 0xf)
			{
				if (m_sprpri == 0) // behind all
				{
					if (fgpixel & 0xf)
					{
						dd[x] = fgpixel  + 0xa00;
					}
					else if (bgpixel & 0xf)
					{
						dd[x] = bgpixel  + 0xb00;
					}
					else
					{
						dd[x] = sprpixel + 0x800;
					}
				}
				else if (m_sprpri == 1)
				{
					if (fgpixel & 0xf)
					{
						if (fgbln)
						{
							if (m_sprbln)
							{
								dd[x] = sum_colors(bgpixel + 0x0300, sprpixel + 0x1000); // WRONG??
							}
							else
							{
								dd[x] = sum_colors(fgpixel + 0x1100, sprpixel + 0x0000);
							}
						}
						else
						{
							dd[x] = fgpixel + 0xa00;
						}
					}
					else
					{
						if (m_sprbln)
						{
							if (bgpixel & 0xf)
							{
								dd[x] = sum_colors(bgpixel + 0x300, sprpixel + 0x1000);
							}
							else
							{
								dd[x] = sum_colors(          0x300, sprpixel + 0x1000);
							}
						}
						else
						{
							dd[x] = sprpixel + 0x800;
						}
					}
				}
				else if (m_sprpri == 2) // above bg,fg, behind tx
				{
					if (m_sprbln)
					{
						if (fgpixel & 0xf) // is the fg used?
						{
							dd[x] = sum_colors(fgpixel + 0x200, sprpixel + 0x1000);
						}
						else if (bgpixel & 0xf)
						{
							dd[x] = sum_colors(bgpixel + 0x300, sprpixel + 0x1000);
						}
						else
						{
							dd[x] = sum_colors(          0x300, sprpixel + 0x1000);
						}
					}
					else
					{
						dd[x] = sprpixel + 0x800;
					}
				}
				else if (m_sprpri == 3) // above all?
				{
					if (m_sprbln)
					{
						if (fgpixel & 0xf) // is the fg used?
						{
							dd[x] = sum_colors(fgpixel + 0x200, sprpixel + 0x1000);
						}
						else if (bgpixel & 0xf)
						{
							dd[x] = sum_colors(bgpixel + 0x300, sprpixel + 0x1000);
						}
						else
						{
							dd[x] = sum_colors(          0x300, sprpixel + 0x1000);
						}
					}
					else
					{
						dd[x] = sprpixel + 0x800;
					}

				}
			}
			else // NON SPRITE CASES
			{
				if (fgpixel & 0xf)
				{
					if (fgbln)
					{
						if (bgpixel & 0xf)
						{
							dd[x] = sum_colors(fgpixel + 0x1100, bgpixel + 0x300);
						}
						else
						{
							dd[x] = sum_colors(fgpixel + 0x1100,           0x300);
						}
					}
					else
					{
						dd[x] = fgpixel + 0xa00;
					}
				}
				else if (bgpixel & 0xf)
				{
					dd[x] = bgpixel + 0xb00;
				}
				else
				{
					dd[x] = 0xb00;
				}
			}
		}
	}
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)BurnPalRAM;

	for (INT32 i = 0; i < 0x2800/2; i++)
	{
		UINT8 r = pal4bit(p[i] & 0xf);
		UINT8 g = pal4bit((p[i] & 0xf0) >> 4);
		UINT8 b = pal4bit((p[i] & 0xf00) >> 8);

		BurnPalette[i] = BurnHighCol(r,g,b,0);
		DrvPalette32[i] = (r << 16) | (g << 8) | b;
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		for (INT32 i = 0; i < 0x8000; i++) {
			BurnPalette[0x8000 + i] = BurnHighCol(pal5bit(i >> 10), pal5bit((i >> 5) & 0x1f), pal5bit(i & 0x1f),0);
			DrvPalette32[0x8000 + i] = (pal5bit(i >> 10) << 16) | (pal5bit((i >> 5) & 0x1f) << 8) | pal5bit(i & 0x1f);
		}
		BurnRecalc = 0;
	}

	DrvPaletteUpdate();

	BurnBitmapFill(0, 0); // main
	BurnBitmapFill(1, 0); // bg
	BurnBitmapFill(2, 0); // fg
	BurnBitmapFill(3, 0); // sprite

	if (nBurnLayer & 1) GenericTilemapDraw(0, 1, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, 2, 0);

	if (nSpriteEnable & 1) draw_sprites();

	mix_bitmaps(1,2,3, 8,10,4);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static void SpbactnpPaletteUpdate()
{
	UINT16 *p = (UINT16*)BurnPalRAM;

	for (INT32 i = 0; i < 0x2800/2; i++) // xBRG
	{
		UINT16 c = p[i];
		UINT8 r = pal4bit((c & 0xf0) >> 4);
		UINT8 g = pal4bit(c & 0xf);
		UINT8 b = pal4bit((c & 0xf00) >> 8);

		BurnPalette[i] = BurnHighCol(r,g,b,0);
		DrvPalette32[i] = (r << 16) | (g << 8) | b;
	}

	for (INT32 i = 0x2800/2; i < 0x2a00/2; i++) // xBRG
	{
		UINT16 c = (p[i] << 8) | (p[i] >> 8);
		UINT8 r = pal4bit((c & 0xf0) >> 4);
		UINT8 g = pal4bit(c & 0xf);
		UINT8 b = pal4bit((c & 0xf00) >> 8);

		BurnPalette[i] = BurnHighCol(r,g,b,0);
		DrvPalette32[i] = (r << 16) | (g << 8) | b;
	}
}

static INT32 SpbactnpDraw()
{
	if (BurnRecalc) {
		for (INT32 i = 0; i < 0x8000; i++) {
			BurnPalette[0x8000 + i] = BurnHighCol(pal5bit(i >> 10), pal5bit((i >> 5) & 0x1f), pal5bit(i & 0x1f),0);
			DrvPalette32[0x8000 + i] = (pal5bit(i >> 10) << 16) | (pal5bit((i >> 5) & 0x1f) << 8) | pal5bit(i & 0x1f);
		}
		BurnRecalc = 0;
	}

	SpbactnpPaletteUpdate();

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapSetScrollY(0, scrolly);

	BurnBitmapFill(0, 0); // main
	BurnBitmapFill(1, 0); // bg
	BurnBitmapFill(2, 0); // fg
	BurnBitmapFill(3, 0); // sprite

	GenericTilesSetClip(224, 736, 16, 224);

	if (nBurnLayer & 1) GenericTilemapDraw(0, 1, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, 2, 0);

	if (nSpriteEnable & 1) draw_sprites();

	mix_bitmaps(1,2,3, 12,14,8);

	GenericTilesSetClip(0, 256, 0, 256);

	if (nBurnLayer & 4) GenericTilemapDraw(2, 1, 0);

	GenericTilesClearClip();

	// rotate top tilemap
	for (INT32 y = 16; y < 240; y++)
	{
		for (INT32 x = 0; x < 256; x++)
		{
			UINT16 *dest = pTransDraw + ((x ^ 0xff) * nScreenWidth) + (y - 16);
			UINT16 *src = BurnBitmapGetPosition(1, x, y);
			dest[0] = src[0];
		}
	}

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));
		DrvInputs[2] &= ~3; // bits 0,1 active hi

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= DrvJoy1[i] << i;
			DrvInputs[1] ^= DrvJoy2[i] << i;
			DrvInputs[2] ^= DrvJoy3[i] << i;
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[3] = { 12000000 / 60, 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { nCyclesExtra, 0, 0 }; // sound is tracked by timer, "extra screen proc" tracking is not needed

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == 239) SekSetIRQLine(3, CPU_IRQSTATUS_ACK);

		ZetOpen(0);
		CPU_RUN_TIMER(1);
		ZetClose();

		ZetOpen(1);
		CPU_RUN_SYNCINT(2, Zet);
		if (i == 239) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	SekClose();

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		SekScan(nAction);
		ZetScan(nAction);

		BurnYM3812Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(extra_latch);
		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// Super Pinball Action (US)

static struct BurnRomInfo spbactnRomDesc[] = {
	{ "rom1.bin",							0x020000, 0x6741bd3f,  1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "rom2.bin",							0x020000, 0x488cc511,  1 | BRF_PRG | BRF_ESS },	//  1 

	{ "a-u14.3",							0x010000, 0x57f4c503,  2 | BRF_PRG | BRF_ESS },	//  2 Z80 Code

	{ "a-u19",								0x020000, 0x87427d7d,  3 | BRF_SND },			//  3 Samples

	{ "b-u98",								0x040000, 0x315eab4d,  4 | BRF_GRA },			//  4 Foreground Tiles
	{ "b-u99",								0x040000, 0x7b76efd9,  4 | BRF_GRA },			//  5 

	{ "b-u104",								0x040000, 0xb648a40a,  5 | BRF_GRA },			//  6 Background Tiles
	{ "b-u105",								0x040000, 0x0172d79a,  5 | BRF_GRA },			//  7 

	{ "b-u110",								0x040000, 0x862ebacd,  6 | BRF_GRA },			//  8 Sprites
	{ "b-u111",								0x040000, 0x1cc1379a,  6 | BRF_GRA },			//  9 
};

STD_ROM_PICK(spbactn)
STD_ROM_FN(spbactn)

struct BurnDriver BurnDrvSpbactn = {
	"spbactn", NULL, NULL, NULL, "1991",
	"Super Pinball Action (US)\0", NULL, "Tecmo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_PINBALL, 0,
	NULL, spbactnRomInfo, spbactnRomName, NULL, NULL, NULL, NULL, SpbactnInputInfo, SpbactnDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x1400,
	224, 512, 3, 4
};


// Super Pinball Action (Japan)

static struct BurnRomInfo spbactnjRomDesc[] = {
	{ "a-u68.1",							0x020000, 0xb5b2d824,  1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "a-u67.2",							0x020000, 0x9577b48b,  1 | BRF_PRG | BRF_ESS },	//  1 

	{ "a-u14.3",							0x010000, 0x57f4c503,  2 | BRF_PRG | BRF_ESS },	//  2 Z80 Code

	{ "a-u19",								0x020000, 0x87427d7d,  3 | BRF_SND },			//  3 Samples

	{ "b-u98",								0x040000, 0x315eab4d,  4 | BRF_GRA },			//  4 Foreground Tiles
	{ "b-u99",								0x040000, 0x7b76efd9,  4 | BRF_GRA },			//  5 

	{ "b-u104",								0x040000, 0xb648a40a,  5 | BRF_GRA },			//  6 Background Tiles
	{ "b-u105",								0x040000, 0x0172d79a,  5 | BRF_GRA },			//  7 

	{ "b-u110",								0x040000, 0x862ebacd,  6 | BRF_GRA },			//  8 Sprites
	{ "b-u111",								0x040000, 0x1cc1379a,  6 | BRF_GRA },			//  9 
};

STD_ROM_PICK(spbactnj)
STD_ROM_FN(spbactnj)

struct BurnDriver BurnDrvSpbactnj = {
	"spbactnj", "spbactn", NULL, NULL, "1991",
	"Super Pinball Action (Japan)\0", NULL, "Tecmo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_PINBALL, 0,
	NULL, spbactnjRomInfo, spbactnjRomName, NULL, NULL, NULL, NULL, SpbactnInputInfo, SpbactnDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x1400,
	224, 512, 3, 4
};


// Super Pinball Action (US, prototype, dual screen)

static struct BurnRomInfo spbactnpRomDesc[] = {
	{ "spa.18k",							0x010000, 0x40f6a1e6,  1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "spa.22k",							0x010000, 0xce31871e,  1 | BRF_PRG | BRF_ESS },	//  1 
	{ "spa.17k",							0x010000, 0xc9860ae9,  1 | BRF_PRG | BRF_ESS },	//  2 
	{ "spa.21k",							0x010000, 0x8226f644,  1 | BRF_PRG | BRF_ESS },	//  3 

	{ "spa.17g",							0x010000, 0x445fc2c5,  2 | BRF_PRG | BRF_ESS },	//  4 Z80 Code

	{ "spa_data_2-21-a10.8e",				0x020000, 0x87427d7d,  3 | BRF_SND },			//  5 Samples

	{ "spa_back0_split0_5-17-p-1.27b",		0x020000, 0x37922110,  4 | BRF_GRA },			//  6 Foreground Tiles
	{ "spa_back0_split1_5-17-p-1.27c",		0x020000, 0x9d6ef9ab,  4 | BRF_GRA },			//  7 

	{ "spa_back1_split0_3-14-a-11.26b",		0x020000, 0x6953fd62,  5 | BRF_GRA },			//  8 Background Tiles
	{ "spa_back1_split1_3-14-a-11.26c",		0x020000, 0xb4123511,  5 | BRF_GRA },			//  9 

	{ "spa_sp0_4-18-p-8.5m",				0x020000, 0xcd6ba360,  6 | BRF_GRA },			// 10 Sprites
	{ "spa_sp1_3-14-a-10.4m",				0x020000, 0x86406336,  6 | BRF_GRA },			// 11 

	{ "6204_6-6.29c",						0x010000, 0xe8250c26,  7 | BRF_PRG | BRF_ESS },	// 12 Z80 Code (Char Layer CPU)

	{ "spa.25c",							0x020000, 0x02b69ab9,  8 | BRF_GRA },			// 13 Char Layer Tiles

	{ "p109.18d",							0x000100, 0x2297a725,  0 | BRF_OPT },			// 14 Misc
	{ "pin.b.sub.23g",						0x000100, 0x3a0c70ed,  0 | BRF_OPT },			// 15 
	{ "tcm1.19g.bin",						0x000053, 0x2c54354a,  0 | BRF_OPT },			// 16 
};

STD_ROM_PICK(spbactnp)
STD_ROM_FN(spbactnp)

struct BurnDriver BurnDrvSpbactnp = {
	"spbactnp", "spbactn", NULL, NULL, "1989",
	"Super Pinball Action (US, prototype, dual screen)\0", NULL, "Tecmo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_PRE90S, GBF_PINBALL, 0,
	NULL, spbactnpRomInfo, spbactnpRomName, NULL, NULL, NULL, NULL, SpbactnpInputInfo, SpbactnpDIPInfo,
	SpbactnpInit, DrvExit, DrvFrame, SpbactnpDraw, DrvScan, &BurnRecalc, 0x1500,
	256, 736, 4, 7
};
