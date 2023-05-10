// FinalBurn Neo Disco Boy driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "msm5205.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvZ80RAM[4];
static UINT8 *DrvAttrRAM;

static UINT8 bankdata[4];
static UINT8 soundlatch;
static UINT8 gfxbank;
static INT32 adpcm_toggle;
static UINT8 adpcm_data;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static HoldCoin<2> hold_coin;

static struct BurnInputInfo DiscoboyInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Discoboy)

static struct BurnDIPInfo DiscoboyDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x7f, NULL					},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x00, 0x01, 0x07, 0x00, "4 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x01, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x07, 0x02, "2 Coins 3 Credits"	},
	{0x00, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x00, 0x01, 0x08, 0x08, "Every 150000"			},
	{0x00, 0x01, 0x08, 0x00, "Every 300000"			},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x00, 0x01, 0x10, 0x10, "3"					},
	{0x00, 0x01, 0x10, 0x00, "4"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x00, 0x01, 0x60, 0x00, "Easy"					},
	{0x00, 0x01, 0x60, 0x60, "Normal"				},
	{0x00, 0x01, 0x60, 0x40, "Hard"					},
	{0x00, 0x01, 0x60, 0x20, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    0, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Discoboy)

static void rombank_write(UINT8 data)
{
	bankdata[0] = data;
	gfxbank = data & 0xf0;

	ZetMapMemory(DrvZ80ROM[0] + 0x10000 + (data & 7) * 0x4000, 0x8000, 0xbfff, MAP_ROM);
}

static void rambank0_write(UINT8 data)
{
	bankdata[1] = data;
	ZetMapMemory(BurnPalRAM + ((data & 0x20) << 6), 0xc000, 0xc7ff, MAP_RAM);
}

static void rambank1_write(UINT8 data)
{
	bankdata[2] = data;
	if (data < 2) {
		ZetMapMemory(DrvZ80RAM[1 + (data & 1)], 0xd000, 0xdfff, MAP_RAM);
	}
}

static void __fastcall discoboy_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			rambank1_write(data);
		return;

		case 0x01:
			rombank_write(data);
		return;

		case 0x03:
			soundlatch = data;
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_HOLD);
		return;

		case 0x06: // nop
		return;

		case 0x07:
			rambank0_write(data);
		return;
	}
}

static UINT8 __fastcall discoboy_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return DrvDips[0];

		case 0x01: // sys
		case 0x02: // p1
		case 0x03: // p2
			return DrvInputs[(port - 1) & 3];

		case 0x04:
			return DrvDips[1];

		case 0x06:
			return 0; // ok!
	}

	return 0;
}

static void sound_bankswitch(UINT8 data)
{
	bankdata[3] = data;
	ZetMapMemory(DrvZ80ROM[1] + (data & 7) * 0x4000, 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall discoboy_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			MSM5205ResetWrite(0, (data >> 3) & 1);
			sound_bankswitch(data);
		return;

		case 0xe400:
			adpcm_data = data;
		return;

		case 0xec00:
		case 0xec01:
			BurnYM3812Write(0, address & 1, data);
		return;
	}
}

static UINT8 __fastcall discoboy_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf800:
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 code = DrvZ80RAM[1][offs * 2] + (DrvZ80RAM[1][offs * 2 + 1] << 8);
	INT32 color = DrvAttrRAM[offs];
	INT32 bank = 1;

	if (code > 0x2000) {
		bank++;
		code = (code & 0x1fff) + (gfxbank & 0xc0) * 0x80;
	}

	TILE_SET_INFO(bank, code, color, 0);
}

static void DrvMSM5205Int()
{
	MSM5205DataWrite(0, adpcm_data & 0xf);
	adpcm_data >>= 4;
	adpcm_toggle ^= 1;

	if (adpcm_toggle) ZetNmi();
}

inline static INT32 DrvMSM5205SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)ZetTotalCycles() * nSoundRate / 5000000);
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)ZetTotalCycles() * nSoundRate / 5000000);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	rombank_write(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	sound_bankswitch(0);
	ZetReset();
	BurnYM3812Reset();
	MSM5205Reset();
	ZetClose();

	gfxbank = 0;
	soundlatch = 0;
	adpcm_toggle = 0;
	adpcm_data = 0;

	hold_coin.reset();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x030000;
	DrvZ80ROM[1]		= Next; Next += 0x020000;

	DrvGfxROM[0]		= Next; Next += 0x200000;
	DrvGfxROM[1]		= Next; Next += 0x080000;
	DrvGfxROM[2]		= Next; Next += 0x400000;

	BurnPalette			= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam				= Next;

	BurnPalRAM			= Next; Next += 0x001000;
	DrvAttrRAM			= Next; Next += 0x000800;
	DrvZ80RAM[0]		= Next; Next += 0x002000;
	DrvZ80RAM[1]		= Next; Next += 0x001000;
	DrvZ80RAM[2]		= Next; Next += 0x001000;
	DrvZ80RAM[3]		= Next; Next += 0x000800;

	RamEnd				= Next;
	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0x80000*8+4, 0x80000*8, 4, 0 };
	INT32 Plane1[4]  = { 0x10000*8, 0, 0x30000*8, 0x20000*8 };
	INT32 Plane2[4]  = { 0x40000*8, 0, 0xc0000*8, 0x80000*8 };
	INT32 XOffs0[16] = { STEP4(0,1), STEP4(8,1), STEP4(256,1), STEP4(256+8,1) };
	INT32 YOffs0[16] = { STEP16(0,16) };
	INT32 XOffs1[8]  = { STEP8(0,1) };
	INT32 YOffs1[8]  = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane0, XOffs0, YOffs0, 0x200, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x040000);

	GfxDecode(0x2000, 4,  8,  8, Plane1, XOffs1, YOffs1, 0x040, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x100000);

	GfxDecode(0x10000, 4,  8,  8, Plane2, XOffs1, YOffs1, 0x040, tmp, DrvGfxROM[2]);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x10000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x10000, k++, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[0] + 0x00000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x10000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x80000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x90000, k++, 1, LD_INVERT)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[1] + 0x00000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x10000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x20000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[1] + 0x30000, k++, 1, LD_INVERT)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[2] + 0x00000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x40000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x80000, k++, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0xc0000, k++, 1, LD_INVERT)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0], 		0x0000, 0x7fff, MAP_ROM);
	// rom bank 8000-bfff
	// ram bank 1 c000-c7ff
	ZetMapMemory(DrvAttrRAM,		0xc800, 0xcfff, MAP_RAM);
	// rambank 2 d000-dfff
	ZetMapMemory(DrvZ80RAM[0],		0xe000, 0xffff, MAP_RAM);
	ZetSetOutHandler(discoboy_main_write_port);
	ZetSetInHandler(discoboy_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[3],		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(discoboy_sound_write);
	ZetSetReadHandler(discoboy_sound_read);
	ZetClose();

	BurnYM3812Init(1, 2500000, NULL, &DrvSynchroniseStream, 0);
	BurnTimerAttachYM3812(&ZetConfig, 5000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, DrvMSM5205SynchroniseStream, 400000, DrvMSM5205Int, MSM5205_S96_4B, 1);
	MSM5205SetRoute(0, 0.65, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4, 16, 16, 0x200000, 0, 0x7f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4,  8,  8, 0x080000, 0, 0x7f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4,  8,  8, 0x400000, 0, 0x7f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -64, -8);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	MSM5205Exit();
	BurnYM3812Exit();

	ZetExit();
	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x1000; i += 2)
	{
		UINT16 pal = BurnPalRAM[i] | (BurnPalRAM[i + 1] << 8);

		INT32 b = ((pal >> 0) & 0xf) << 4;
		INT32 g = ((pal >> 4) & 0xf) << 4;
		INT32 r = ((pal >> 8) & 0xf) << 4;

		BurnPalette[(i / 2)] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x1000 - 0x40; offs >= 0; offs -= 0x20)
	{
		INT32 code 	=   DrvZ80RAM[2][offs + 0];
		INT32 attr 	=   DrvZ80RAM[2][offs + 1];
		INT32 sx 	=   DrvZ80RAM[2][offs + 3] + ((attr & 0x10) << 4);
		INT32 sy 	= ((DrvZ80RAM[2][offs + 2] + 8) & 0xff) - 8;
		INT32 color	=   attr & 0x0f;

		code += (attr & 0xe0) << 3;
		if (code & 0x400) code += (gfxbank & 0x30) * 0x40;

		DrawGfxMaskTile(0, 0, code, sx - 64, sy - 8, 0, 0, color, 0xf);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteUpdate();
		BurnRecalc = 1;	// force update!
	}

	BurnTransferClear(0x3ff);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= DrvJoy1[i] << i;
			DrvInputs[1] ^= DrvJoy2[i] << i;
			DrvInputs[2] ^= DrvJoy3[i] << i;
		}

		hold_coin.checklow(0, DrvInputs[0], 1<<7, 1);
		hold_coin.checklow(1, DrvInputs[0], 1<<5, 1);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 5000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	MSM5205NewFrame(0, 5000000, nInterleave);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdateYM3812((i + 1) * (nCyclesTotal[1] / nInterleave));
		MSM5205UpdateScanline(i);
		ZetClose();
	}

	ZetOpen(1);

	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
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

		SCAN_VAR(bankdata);
		SCAN_VAR(soundlatch);
		SCAN_VAR(adpcm_toggle);
		SCAN_VAR(adpcm_data);

		hold_coin.scan();
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		rombank_write(bankdata[0]);
		rambank0_write(bankdata[1]);
		rambank1_write(bankdata[2]);
		ZetClose();

		ZetOpen(1);
		sound_bankswitch(bankdata[3]);
		ZetClose();
	}

	return 0;
}


// Disco Boy

static struct BurnRomInfo discoboyRomDesc[] = {
	{ "u2",				0x10000, 0x44a4fefa, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "u18",			0x20000, 0x88d1282d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "2.u28",			0x10000, 0x7c2ed174, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "1.u45",			0x10000, 0xc266c6df, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "5.u94",			0x10000, 0xdbd20836, 3 | BRF_GRA },           //  4 Sprites
	{ "6.u124",			0x40000, 0xe20d41f8, 3 | BRF_GRA },           //  5
	{ "7.u95",			0x10000, 0x1d5617a2, 3 | BRF_GRA },           //  6
	{ "8.u125",			0x40000, 0x30be1340, 3 | BRF_GRA },           //  7

	{ "u80",			0x10000, 0x4cc642ae, 4 | BRF_GRA },           //  8 Background Tiles
	{ "u81",			0x10000, 0x9e04274e, 4 | BRF_GRA },           //  9
	{ "u78",			0x10000, 0x04571f70, 4 | BRF_GRA },           // 10
	{ "u79",			0x10000, 0x646f0f83, 4 | BRF_GRA },           // 11

	{ "u50",			0x20000, 0x1557ca92, 5 | BRF_GRA },           // 12 Banked Background Tiles
	{ "u5",				0x20000, 0xa07df669, 5 | BRF_GRA },           // 13
	{ "u46",			0x20000, 0x764ffde4, 5 | BRF_GRA },           // 14
	{ "u49",			0x20000, 0x0b6c0d8d, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(discoboy)
STD_ROM_FN(discoboy)

struct BurnDriver BurnDrvDiscoboy = {
	"discoboy", NULL, NULL, NULL, "1993",
	"Disco Boy\0", NULL, "Soft Art Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT | GBF_ACTION, 0,
	NULL, discoboyRomInfo, discoboyRomName, NULL, NULL, NULL, NULL, DiscoboyInputInfo, DiscoboyDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	240, 384, 3, 4
};


// Disco Boy (Promat license?)

static struct BurnRomInfo discoboypRomDesc[] = {
	{ "discob.u2",		0x10000, 0x7f07afd1, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "discob.u18",		0x20000, 0x05f0daaf, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "discob.u28",		0x10000, 0x7c2ed174, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "discob.u45",		0x10000, 0xc266c6df, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "discob.u94",		0x10000, 0xc436f1e5, 3 | BRF_GRA },           //  4 Sprites
	{ "discob.u124",	0x40000, 0x0b0bf653, 3 | BRF_GRA },           //  5
	{ "discob.u95",		0x10000, 0xddea540e, 3 | BRF_GRA },           //  6
	{ "discob.u125",	0x40000, 0xfcac2cb8, 3 | BRF_GRA },           //  7

	{ "discob.u80",		0x10000, 0x48a7ebdf, 4 | BRF_GRA },           //  8 Background Tiles
	{ "discob.u81",		0x10000, 0x9ca358e1, 4 | BRF_GRA },           //  9
	{ "discob.u78",		0x10000, 0x2b39eb08, 4 | BRF_GRA },           // 10
	{ "discob.u79",		0x10000, 0x77ffc6bf, 4 | BRF_GRA },           // 11

	{ "discob.u50",		0x40000, 0xea7231db, 5 | BRF_GRA },           // 12 Banked Background Tiles
	{ "discob.u5",		0x40000, 0xafeefecc, 5 | BRF_GRA },           // 13
	{ "discob.u46",		0x40000, 0x835c513b, 5 | BRF_GRA },           // 14
	{ "discob.u49",		0x40000, 0x9f884db4, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(discoboyp)
STD_ROM_FN(discoboyp)

struct BurnDriver BurnDrvDiscoboyp = {
	"discoboyp", "discoboy", NULL, NULL, "1993",
	"Disco Boy (Promat license?)\0", NULL, "Soft Art Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SHOOT | GBF_ACTION, 0,
	NULL, discoboypRomInfo, discoboypRomName, NULL, NULL, NULL, NULL, DiscoboyInputInfo, DiscoboyDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	240, 384, 3, 4
};
