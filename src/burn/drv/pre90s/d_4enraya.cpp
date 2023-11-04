// FB Neo 4 Enraya driver module
// Based on MAME driver by Tomasz Slanina

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 soundcontrol;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static UINT8 sound_bit;

static struct BurnInputInfo Enraya4InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Enraya4)

static struct BurnInputInfo UnkpacgInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"Coin 3",			BIT_DIGITAL,	DrvJoy1 + 2,	"p3 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 1,	"dips"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 0,	"dips"		},
};

STDINPUTINFO(Unkpacg)

static struct BurnInputInfo UnksigInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"Coin 3",			BIT_DIGITAL,	DrvJoy1 + 2,	"p3 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Deal",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 deal"   },
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Fire",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 1,	"dips"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 0,	"dips"		},
};

STDINPUTINFO(Unksig)

static struct BurnDIPInfo Enraya4DIPList[]=
{
	{0x0d, 0xff, 0xff, 0xfd, NULL			},
	{0x0e, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0d, 0x01, 0x01, 0x01, "Easy"			},
	{0x0d, 0x01, 0x01, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0d, 0x01, 0x02, 0x02, "Off"			},
	{0x0d, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Pieces"		},
	{0x0d, 0x01, 0x04, 0x04, "30"			},
	{0x0d, 0x01, 0x04, 0x00, "16"			},

	{0   , 0xfe, 0   ,    2, "Speed"		},
	{0x0d, 0x01, 0x08, 0x08, "Slow"			},
	{0x0d, 0x01, 0x08, 0x00, "Fast"			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0d, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x0d, 0x01, 0x30, 0x10, "1 Coin  3 Credits"	},
	{0x0d, 0x01, 0x30, 0x20, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x0d, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x0d, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
};

STDDIPINFO(Enraya4)

static struct BurnDIPInfo UnkpacgDIPList[]=
{
	{0x09, 0xff, 0xff, 0x00, NULL			},
	{0x0a, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "0-8"			},
	{0x09, 0x01, 0x80, 0x00, "Off"			},
	{0x09, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    2, "1-0"			},
	{0x0a, 0x01, 0x01, 0x00, "Off"			},
	{0x0a, 0x01, 0x01, 0x01, "On"			},

	{0   , 0xfe, 0   ,    2, "Gambling Game"},
	{0x0a, 0x01, 0x02, 0x00, "Yes"			},
	{0x0a, 0x01, 0x02, 0x02, "No"			},

	{0   , 0xfe, 0   ,    2, "1-2"			},
	{0x0a, 0x01, 0x04, 0x00, "Off"			},
	{0x0a, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    2, "1-3"			},
	{0x0a, 0x01, 0x08, 0x00, "Off"			},
	{0x0a, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "1-4"			},
	{0x0a, 0x01, 0x10, 0x00, "Off"			},
	{0x0a, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "1-5"			},
	{0x0a, 0x01, 0x20, 0x00, "Off"			},
	{0x0a, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Gambling Game Credits Value"			},
	{0x0a, 0x01, 0x40, 0x40, "1 Credit/Point = 100"			},
	{0x0a, 0x01, 0x40, 0x00, "1 Credit/Point = 500"			},

	{0   , 0xfe, 0   ,    2, "Clear NVRAM (On, reset, Off, reset)" },
	{0x0a, 0x01, 0x80, 0x00, "Off"			},
	{0x0a, 0x01, 0x80, 0x80, "On"			},
};

STDDIPINFO(Unkpacg)

static struct BurnDIPInfo UnksigDIPList[] =
{
	DIP_OFFSET(0x01)
};

STDDIPINFOEXT(Unksig, Unkpacg, Unksig)

static void __fastcall enraya4_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0xd000 || (address & 0xf000) == 0x7000) {
		DrvVidRAM[((address & 0x3ff) << 1) + 0] = data;
		DrvVidRAM[((address & 0x3ff) << 1) + 1] = (address >> 10) & 3;
		return;
	}
}

static void sound_control(UINT8 data)
{
	if ((soundcontrol & sound_bit) == sound_bit && (data & sound_bit) == 0) {
		AY8910Write(0, ~soundcontrol & 1, soundlatch);
	}

	soundcontrol = data;
}

static void __fastcall enraya4_out_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x17:
			AY8910Write(0, 1, data);
		break;

		case 0x37:
			AY8910Write(0, 0, data);
		break;

		case 0x20:
		case 0x23:
			soundlatch = data;
		break;

		case 0x30:
		case 0x33:
			sound_control(data);
		break;
	}
}

static UINT8 __fastcall enraya4_in_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvDips[0];

		case 0x01:
			return DrvInputs[0];
	
		case 0x02:
			return DrvInputs[1];

		case 0x27:
			return AY8910Read(0);
	}

	return 0;
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvVidRAM[offs * 2] + (DrvVidRAM[offs * 2 + 1] * 256), 0, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);

	HiscoreReset();

	soundlatch   = 0;
	soundcontrol = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;	// unkpacga (0x8000 * 2)

	DrvGfxROM		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0008 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x001000;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { 0x10000, 0x20000, 0x00000 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x6000);

	GfxDecode(0x0400, 3, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static void DrvPrgDecode()
{
	for (INT32 i = 0x8000; i < 0x10000; i++) {
		DrvZ80ROM[i] = (DrvZ80ROM[i] & 0xfc) | ((DrvZ80ROM[i] & 1) << 1) | ((DrvZ80ROM[i] >> 1) & 1);
	}
}

static UINT8 ay_read_A(UINT32)
{
	return DrvDips[1];
}

static INT32 DrvInit(INT32 game, INT32 sbit)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		//---------------------------------------------------------
		// Prg

		if (7 == game) {	// unksiga
			UINT8* tmp = (UINT8*)BurnMalloc(0x10000 * 2);
			if (NULL == tmp) return 1;

			for (INT32 i = 0, j = 0; i < 2; i++, j += 0x8000) {
				if (BurnLoadRom(tmp + i * 0x10000, i, 1)) {
					BurnFree(tmp);
					return 1;
				}
				memcpy(DrvZ80ROM + j, tmp + i * 0x10000, 0x2000);
			}
			BurnFree(tmp);
		} else {	// unkpacg
			if (BurnLoadRom(DrvZ80ROM + 0x00000, 0, 1)) return 1;
			if (8 != game)	//unksigb
				if (BurnLoadRom(DrvZ80ROM + 0x08000, 1, 1)) return 1;
		}
		if (6 == game) {	// unksig
			for (INT32 i = 0; i < 0x8000 * 2; i += 0x8000) {
				memmove(DrvZ80ROM + i, DrvZ80ROM + i + 0x6000, 0x2000);
				memset(DrvZ80ROM + i + 0x2000, 0, 0x6000);
			}
		}
		if (8 == game) {	// unksigb
			memset(DrvZ80ROM + 0x4000, 0, 0xc000);
			memmove(DrvZ80ROM + 0x8000, DrvZ80ROM + 0x0000, 0x2000);
			memmove(DrvZ80ROM + 0x0000, DrvZ80ROM + 0x2000, 0x2000);
		}

		//---------------------------------------------------------
		// Gfx

		if (0 == game || 5 == game) {	// 4enraya / unkpacgd
			if (BurnLoadRom(DrvGfxROM + 0x00000, 2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x02000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x04000, 4, 1)) return 1;
		} else if (3 == game || 4 == game || 6 == game || 7 == game || 8 == game) {	// unkpacga / unkpacgb / unksig /unksiga /unksigb
			UINT32 nRomLen = (7 == game) ? 0x10000 : 0x8000;
			UINT32 nOffset = (6 == game || 8 == game) ? 0x6000 : ((7 == game) ? 0x8000 : 0x2000);
			INT32 nNumber = (8 == game) ? 2 : 3;
			INT32 nIndex = (8 == game) ? 1 : 2;
			INT32 i, j, k;

			UINT8* tmp = (UINT8*)BurnMalloc(nRomLen * nNumber);
			if (NULL == tmp) return 1;

			for (i = 0, j = nIndex, k = 0; i < nRomLen * nNumber; i += nRomLen, j++, k += 0x2000) {
				if (BurnLoadRom(tmp + i, j, 1)) {
					BurnFree(tmp);
					return 1;
				}
				memcpy(DrvGfxROM + k, tmp + i + nOffset, 0x2000);
			}
			BurnFree(tmp);

			if (8 == game) {	//unksigb
				if (BurnLoadRom(DrvGfxROM + k, j, 1)) return 1;
			}
		} else {	// unkpacg
			if (BurnLoadRom(DrvGfxROM + 0x02000, 2, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x04000, 3, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM + 0x00000, 4, 1)) return 1;
		}

		//---------------------------------------------------------
		// Crypt

		switch (game) {
			case 1:	// unkpacg
			case 3:	// unkpacga
			case 4:	// unkpacgb
			case 6:	// unksig
			case 7:	// unksiga
				DrvPrgDecode();
				break;
			default:
				break;
		}
		DrvGfxDecode();	// 0x10000 Max (0x400 * 8 * 8)
	}

	//---------------------------------------------------------
	// Map & Other

	ZetInit(0);
	ZetOpen(0);

	if (0 == game) {		// 4enraya
		ZetMapMemory(DrvZ80ROM,          0x0000, 0xbfff, MAP_ROM);
		ZetMapMemory(DrvZ80RAM,          0xc000, 0xcfff, MAP_RAM);
	//	ZetMapMemory(DrvVidRAM,          0xd000, 0xdfff, MAP_WRITE);
	} else if (3 == game) {	// unkpacga
		ZetMapMemory(DrvZ80ROM + 0x6000, 0x0000, 0x1fff, MAP_ROM);
		ZetMapMemory(DrvNVRAM,           0x6000, 0x6fff, MAP_RAM);
		ZetMapMemory(DrvZ80ROM + 0x8000, 0x8000, 0xffff, MAP_ROM);
	} else {				// unkpacg
		ZetMapMemory(DrvZ80ROM,          0x0000, 0x1fff, MAP_ROM);
		ZetMapMemory(DrvNVRAM,           0x6000, 0x6fff, MAP_RAM);
	//	ZetMapMemory(DrvVidRAM,          0x7000, 0x7fff, MAP_WRITE);
		ZetMapMemory(DrvZ80ROM + 0x8000, 0x8000, 0x9fff, MAP_ROM);
	}

	ZetSetOutHandler(enraya4_out_port);
	ZetSetInHandler(enraya4_in_port);
	ZetSetWriteHandler(enraya4_write);
	ZetClose();

	AY8910Init(0, 2000000, 0);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	if (game) { // unkpacg
		AY8910SetPorts(0, &ay_read_A, NULL, NULL, NULL);
	}

	sound_bit = sbit;

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 3, 8, 8, 0x10000, 0, 0);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 8; i++) {
		DrvPalette[i] = BurnHighCol((i&1)?0xff:0, (i&2)?0xff:0, (i&4)?0xff:0, 0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		if (sound_bit == 2) {
			DrvInputs[0] &= ~0x80; // sound hw bit for unkpacg
		}
	}

	INT32 nInterleave = 4;
	INT32 nCyclesTotal[1] = { 4000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Zet);

		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}

	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
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

		SCAN_VAR(soundlatch);
		SCAN_VAR(soundcontrol);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x01000;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// 4 En Raya (set 1)

static struct BurnRomInfo enraya4RomDesc[] = {
	{ "5.bin",   0x8000, 0xcf1cd151, BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "4.bin",   0x4000, 0xf9ec1be7, BRF_ESS | BRF_PRG }, //  1

	{ "1.bin",   0x2000, 0x0e5072fd, BRF_GRA },	      //  2 Graphics
	{ "2.bin",   0x2000, 0x2b0a3793, BRF_GRA },	      //  3
	{ "3.bin",   0x2000, 0xf6940836, BRF_GRA },	      //  4

	{ "1.bpr",   0x0020, 0xdcbd2352, BRF_GRA },	      //  5 Address control prom - not used
};

STD_ROM_PICK(enraya4)
STD_ROM_FN(enraya4)

static INT32 enraya4Init()
{
	return DrvInit(0, 4);
}

struct BurnDriver BurnDrvEnraya4 = {
	"4enraya", NULL, NULL, NULL, "1990",
	"4 En Raya (set 1)\0", NULL, "IDSA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, enraya4RomInfo, enraya4RomName, NULL, NULL, NULL, NULL, Enraya4InputInfo, Enraya4DIPInfo,
	enraya4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8,
	256, 224, 4, 3
};


// 4 En Raya (set 2)

static struct BurnRomInfo enrayaa4RomDesc[] = {
	{ "5.bin",   0x8000, 0x76e8656c, BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "4.bin",   0x4000, 0xf9ec1be7, BRF_ESS | BRF_PRG }, //  1

	{ "1.bin",   0x2000, 0x0e5072fd, BRF_GRA },	      //  2 Graphics
	{ "2.bin",   0x2000, 0x2b0a3793, BRF_GRA },	      //  3
	{ "3.bin",   0x2000, 0xf6940836, BRF_GRA },	      //  4

	{ "1.bpr",   0x0020, 0xdcbd2352, BRF_GRA },	      //  5 Address control prom - not used
};

STD_ROM_PICK(enrayaa4)
STD_ROM_FN(enrayaa4)

struct BurnDriver BurnDrvEnrayaa4 = {
	"4enrayaa", "4enraya", NULL, NULL, "1990",
	"4 En Raya (set 2)\0", NULL, "IDSA", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PUZZLE, 0,
	NULL, enrayaa4RomInfo, enrayaa4RomName, NULL, NULL, NULL, NULL, Enraya4InputInfo, Enraya4DIPInfo,
	enraya4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8,
	256, 224, 4, 3
};


// unknown 'Pac-Man' gambling game (set 1)

static struct BurnRomInfo unkpacgRomDesc[] = {
	{ "1.u14",   0x2000, 0x848c4143, BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "2.u46",   0x2000, 0x9e6e0bd3, BRF_ESS | BRF_PRG }, //  1

	{ "3.u20",   0x2000, 0xd00b04ea, BRF_GRA },           //  2 Graphics
	{ "4.u19",   0x2000, 0x4a123a3d, BRF_GRA },           //  3
	{ "5.u18",   0x2000, 0x44f272d2, BRF_GRA },           //  4
};

STD_ROM_PICK(unkpacg)
STD_ROM_FN(unkpacg)

static INT32 unkpacgInit()
{
	return DrvInit(1, 2);
}

struct BurnDriver BurnDrvUnkpacg = {
	"unkpacg", NULL, NULL, NULL, "199?",
	"unknown 'Pac-Man' gambling game (set 1)\0", NULL, "<unknown>", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, unkpacgRomInfo, unkpacgRomName, NULL, NULL, NULL, NULL, UnkpacgInputInfo, UnkpacgDIPInfo,
	unkpacgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8,
	256, 224, 4, 3
};


// Pucman

static struct BurnRomInfo unkpacgaRomDesc[] = {
	// first 0x5fff are 0xff filled
	{ "p1.bin",	0x8000, 0x386bd2da, BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "p2.bin",	0x8000, 0x7878d7f3, BRF_ESS | BRF_PRG }, //  1

	// 1ST AND 2ND HALF IDENTICAL
	{ "r.bin",	0x8000, 0xb0d7b67a, BRF_GRA },           //  2 Graphics
	{ "b.bin",	0x8000, 0x5b26dce5, BRF_GRA },           //  3
	{ "g.bin",	0x8000, 0xe12d34e0, BRF_GRA },           //  4
};

STD_ROM_PICK(unkpacga)
STD_ROM_FN(unkpacga)

static INT32 unkpacgaInit()
{
	return DrvInit(3, 2);
}

struct BurnDriver BurnDrvUnkpacga = {
	"unkpacga", "unkpacg", NULL, NULL, "199?",
	"Pucman\0", NULL, "IDI SRL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, unkpacgaRomInfo, unkpacgaRomName, NULL, NULL, NULL, NULL, UnkpacgInputInfo, UnkpacgDIPInfo,
	unkpacgaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8,
	256, 224, 4, 3
};


// unknown 'Pac-Man' gambling game (set 2)

static struct BurnRomInfo unkpacgbRomDesc[] = {
	{ "p1.bin",	0x8000, 0x5cc6b5e1, BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "p2.bin",	0x8000, 0x06b42740, BRF_ESS | BRF_PRG }, //  1

	// 1ST AND 2ND HALF IDENTICAL
	{ "r.bin",	0x8000, 0xb0d7b67a, BRF_GRA },           //  2 Graphics
	{ "b.bin",	0x8000, 0x5b26dce5, BRF_GRA },           //  3
	{ "g.bin",	0x8000, 0xe12d34e0, BRF_GRA },           //  4
};

STD_ROM_PICK(unkpacgb)
STD_ROM_FN(unkpacgb)

static INT32 unkpacgbInit()
{
	return DrvInit(4, 2);
}

struct BurnDriver BurnDrvUnkpacgb = {
	"unkpacgb", "unkpacg", NULL, NULL, "199?",
	"unknown 'Pac-Man' gambling game (set 2)\0", NULL, "<unknown>", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, unkpacgbRomInfo, unkpacgbRomName, NULL, NULL, NULL, NULL, UnkpacgInputInfo, UnkpacgDIPInfo,
	unkpacgbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8,
	256, 224, 4, 3
};


// Coco Louco

static struct BurnRomInfo unkpacgcRomDesc[] = {
	{ "4",	0x2000, 0x9f620694, BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "5",	0x2000, 0xb107ad7e, BRF_ESS | BRF_PRG }, //  1

	{ "1",	0x2000, 0xd00b04ea, BRF_GRA },           //  2 Graphics
	{ "2",	0x2000, 0x4a123a3d, BRF_GRA },           //  3
	{ "3",	0x2000, 0xf7cd9de0, BRF_GRA },           //  4
};

STD_ROM_PICK(unkpacgc)
STD_ROM_FN(unkpacgc)

static INT32 unkpacgcInit()
{
	return DrvInit(2, 2);
}

struct BurnDriver BurnDrvUnkpacgc = {
	"unkpacgc", "unkpacg", NULL, NULL, "1988",
	"Coco Louco\0", NULL, "<unknown>", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, unkpacgcRomInfo, unkpacgcRomName, NULL, NULL, NULL, NULL, UnkpacgInputInfo, UnkpacgDIPInfo,
	unkpacgcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8,
	256, 224, 4, 3
};


// unknown 'Pac Man with cars' gambling game

static struct BurnRomInfo unkpacgdRomDesc[] = {
	// only the first program ROM differs from unkpacgc and only slightly
	{ "2.bin",	0x2000, 0x4a545bf6, BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "1.bin",	0x2000, 0xb107ad7e, BRF_ESS | BRF_PRG }, //  1

	// these are different: they change the characters from pacman related to car racing related
	{ "3.bin",	0x2000, 0xce47a9da, BRF_GRA },           //  2 Graphics
	{ "4.bin",	0x2000, 0x9a404e8c, BRF_GRA },           //  3
	{ "5.bin",	0x2000, 0x32d8d105, BRF_GRA },           //  4
};

STD_ROM_PICK(unkpacgd)
STD_ROM_FN(unkpacgd)

static INT32 unkpacgdInit()
{
	return DrvInit(5, 2);
}

struct BurnDriver BurnDrvUnkpacgd = {
	"unkpacgd", "unkpacg", NULL, NULL, "1988",
	"unknown 'Pac Man with cars' gambling game\0", NULL, "<unknown>", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, unkpacgdRomInfo, unkpacgdRomName, NULL, NULL, NULL, NULL, UnkpacgInputInfo, UnkpacgDIPInfo,
	unkpacgdInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8,
	256, 224, 4, 3
};


// unknown 'Space Invaders' gambling game (encrypted, set 1)
// All ROMs are 0x8000 but only the last 0x2000 of each is used.

static struct BurnRomInfo unksigRomDesc[] = {
	{ "i.bin",	0x8000, 0x902efc27, BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "ii.bin",	0x8000, 0x855c1ea3, BRF_ESS | BRF_PRG }, //  1

	{ "r.bin",	0x8000, 0xf8a358fe, BRF_GRA },           //  2 Graphics
	{ "b.bin",	0x8000, 0x56ac5874, BRF_GRA },           //  3
	{ "v.bin",	0x8000, 0x4c5a7e43, BRF_GRA },           //  4
};

STD_ROM_PICK(unksig)
STD_ROM_FN(unksig)

static INT32 unksigInit()
{
	return DrvInit(6, 2);
}

struct BurnDriver BurnDrvUnksig = {
	"unksig", NULL, NULL, NULL, "199?",
	"unknown 'Space Invaders' gambling game (encrypted, set 1)\0", NULL, "<unknown>", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, unksigRomInfo, unksigRomName, NULL, NULL, NULL, NULL, UnksigInputInfo, UnksigDIPInfo,
	unksigInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8,
	256, 224, 4, 3
};


// unknown 'Space Invaders' gambling game (encrypted, set 2)
/*
  Unknown 'Space Invaders' gambling game
  All ROMs are 0x10000 but with a lot of addressing issues

  1.bin    BADADDR    ---xxxxxxxxxxxxx
  2.bin    BADADDR    ---xxxxxxxxxxxxx
  b.bin    BADADDR    x-xxxxxxxxxxxxxx
  r.bin    BADADDR    x-xxxxxxxxxxxxxx
  v.bin    BADADDR    x-xxxxxxxxxxxxxx

  The game has both (Space Invaders & Pac-Man) graphics sets.
  Maybe a leftover?...
*/

static struct BurnRomInfo unksigaRomDesc[] = {
	// Identical 0x2000 segments
	{ "1.bin",	0x10000, 0x089a4a63, BRF_ESS | BRF_PRG }, //  0 Z80 Code
	{ "2.bin",	0x10000, 0x970632fd, BRF_ESS | BRF_PRG }, //  1

	// tileset 0000-03ff = Space Invaders GFX.
	// tileset 0400-07ff = Pac-Man GFX.
	{ "b.bin",	0x10000, 0xdd257fb6, BRF_GRA },           //  2 Graphics
	{ "r.bin",	0x10000, 0x38e9feba, BRF_GRA },           //  3
	{ "v.bin",	0x10000, 0xcc597c7b, BRF_GRA },           //  4
};

STD_ROM_PICK(unksiga)
STD_ROM_FN(unksiga)

static INT32 unksigaInit()
{
	return DrvInit(7, 2);
}

struct BurnDriver BurnDrvUnksiga = {
	"unksiga", "unksig", NULL, NULL, "199?",
	"unknown 'Space Invaders' gambling game (encrypted, set 2)\0", NULL, "<unknown>", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, unksigaRomInfo, unksigaRomName, NULL, NULL, NULL, NULL, UnksigInputInfo, UnksigDIPInfo,
	unksigaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8,
	256, 224, 4, 3
};


// unknown 'Space Invaders' gambling game (unencrypted)
// This set has been found with GFX ROMs of different sizes, but same relevant data.
// Program ROM isn't encrypted and has further differences to the two other sets.

static struct BurnRomInfo unksigbRomDesc[] = {
	// 1ST AND 2ND HALF IDENTICAL
	{ "u144",	0x08000, 0x8f19f1d3, BRF_ESS | BRF_PRG }, //  0 Z80 Code

	{ "u172",	0x08000, 0xf8a358fe, BRF_GRA },           //  1 Graphics
	{ "u171",	0x08000, 0x56ac5874, BRF_GRA },           //  2
	{ "u170",	0x02000, 0xf9c686fc, BRF_GRA },           //  3
};

STD_ROM_PICK(unksigb)
STD_ROM_FN(unksigb)

static INT32 unksigbInit()
{
	return DrvInit(8, 2);
}

struct BurnDriver BurnDrvUnksigb = {
	"unksigb", "unksig", NULL, NULL, "199?",
	"unknown 'Space Invaders' gambling game (unencrypted)\0", NULL, "<unknown>", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, unksigbRomInfo, unksigbRomName, NULL, NULL, NULL, NULL, UnksigInputInfo, UnksigDIPInfo,
	unksigbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8,
	256, 224, 4, 3
};
