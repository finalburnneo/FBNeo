// FB Neo Lady Frog driver module
// Based on MAME driver by Tomasz Slanina

#include "tiles_generic.h"
#include "z80_intf.h"
#include "msm5232.h"
#include "ay8910.h"
#include "dac.h"
#include "biquad.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 palette_bank;
static INT32 tile_bank;
static INT32 sound_data;
static INT32 sound_flag;
static INT32 sound_latch;
static INT32 sound_nmi_pending;
static INT32 sound_nmi_enabled;
static INT32 sound_cpu_halted;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT32 nCyclesExtra[2];

static BIQSTEREO biquad;

static struct BurnInputInfo LadyfrogInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ladyfrog)

static struct BurnDIPInfo LadyfrogDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0xda, NULL								},
	{0x01, 0xff, 0xff, 0xf5, NULL								},

	{0   , 0xfe, 0   ,    4, "Lives"							},
	{0x00, 0x01, 0x03, 0x00, "4"								},
	{0x00, 0x01, 0x03, 0x02, "3"								},
	{0x00, 0x01, 0x03, 0x01, "2"								},
	{0x00, 0x01, 0x03, 0x03, "1"								},

	{0   , 0xfe, 0   ,    2, "Clear 'doors' after life lost"	},
	{0x00, 0x01, 0x04, 0x00, "Off"								},
	{0x00, 0x01, 0x04, 0x04, "On"								},

	{0   , 0xfe, 0   ,    2, "Free Play"						},
	{0x00, 0x01, 0x10, 0x10, "Off"								},
	{0x00, 0x01, 0x10, 0x00, "On"								},

	{0   , 0xfe, 0   ,    2, "Allow Continue"					},
	{0x00, 0x01, 0x20, 0x20, "No"								},
	{0x00, 0x01, 0x20, 0x00, "Yes"								},

	{0   , 0xfe, 0   ,    4, "Coin A"							},
	{0x00, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"				},
	{0x00, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"				},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"				},
	{0x00, 0x01, 0xc0, 0x00, "1 Coin  3 Credits"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"						},
	{0x01, 0x01, 0x03, 0x03, "Easy"								},
	{0x01, 0x01, 0x03, 0x01, "Normal"							},
	{0x01, 0x01, 0x03, 0x02, "Hard"								},
	{0x01, 0x01, 0x03, 0x00, "Hardest"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"						},
	{0x01, 0x01, 0x08, 0x08, "Off"								},
	{0x01, 0x01, 0x08, 0x00, "On"								},
};

STDDIPINFO(Ladyfrog)

static struct BurnDIPInfo TouchemeDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0xfa, NULL								},
	{0x01, 0xff, 0xff, 0xff, NULL								},

	{0   , 0xfe, 0   ,    4, "Lives"							},
	{0x00, 0x01, 0x03, 0x00, "4"								},
	{0x00, 0x01, 0x03, 0x02, "3"								},
	{0x00, 0x01, 0x03, 0x01, "2"								},
	{0x00, 0x01, 0x03, 0x03, "1"								},

	{0   , 0xfe, 0   ,    2, "Allow Continue"					},
	{0x00, 0x01, 0x04, 0x04, "No"								},
	{0x00, 0x01, 0x04, 0x00, "Yes"								},

	{0   , 0xfe, 0   ,    2, "Free Play"						},
	{0x00, 0x01, 0x10, 0x10, "Off"								},
	{0x00, 0x01, 0x10, 0x00, "On"								},

	{0   , 0xfe, 0   ,    4, "Coin A"							},
	{0x00, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"				},
	{0x00, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"				},
	{0x00, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"				},
	{0x00, 0x01, 0xc0, 0x00, "1 Coin  4 Credits"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"						},
	{0x01, 0x01, 0x02, 0x00, "Off"								},
	{0x01, 0x01, 0x02, 0x02, "On"								},
};

STDDIPINFO(Toucheme)

static void palette_bankswitch(INT32 data)
{
	palette_bank = data;
	data = (data >> 5) & 1;

	ZetMapMemory(DrvPalRAM + 0x000 + (data * 0x100), 0xdd00, 0xddff, MAP_RAM);
	ZetMapMemory(DrvPalRAM + 0x200 + (data * 0x100), 0xde00, 0xdeff, MAP_RAM);
}

static void __fastcall ladyfrog_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd000:
			tile_bank = (~data & 0x18) >> 3;
		return;

		case 0xd400:
			sound_latch = data;
			if (sound_nmi_enabled) {
				ZetNmi(1);
			} else {
				sound_nmi_pending = 1;
			}
		return;

		case 0xd403:
			sound_cpu_halted = data & 1;
			if (sound_cpu_halted) {
				ZetReset(1);
			}
		return;

		case 0xdf03:
			palette_bankswitch(data);
		return;
	}
}

static UINT8 __fastcall ladyfrog_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xd400:
			sound_flag = 0;
			return sound_data;

		case 0xd401:
			return sound_flag ? 0xff : 0xfd;

		case 0xd800:
			return DrvDips[0];

		case 0xd801:
			return DrvDips[1];

		case 0xd804:
			return DrvInputs[0];

		case 0xd806:
			return DrvInputs[1];

		case 0xd0d0:
			return 0; // nop
	}

	return 0;
}

static void __fastcall ladyfrog_sound_write(UINT16 address, UINT8 data)
{
	if (address >= 0xc900 && address <= 0xc90d) {
		MSM5232Write(address & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0xc802:
		case 0xc803:
			AY8910Write(0, address & 1, data);
		return;

		case 0xd000:
			sound_data = data;
			sound_flag = 1;
		return;

		case 0xd200:
			sound_nmi_enabled = 1;
			if (sound_nmi_pending) {
				ZetNmi();
				sound_nmi_pending = 0;
			}
		return;

		case 0xd400:
			sound_nmi_enabled = 0;
		return;

		case 0xd600:
			DACSignedWrite(0, data);
		return;

		case 0xc800:
		case 0xc801: // nop
		case 0xca00:
		case 0xcb00:
		case 0xcc00: // nop
		return;
	}
}

static UINT8 __fastcall ladyfrog_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xd000:
			return sound_latch;
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[0x80 + (offs * 2) + 1];
	INT32 code = DrvVidRAM[0x80 + (offs * 2) + 0] | ((attr & 0xc0) << 2) | ((attr & 0x30) << 6);

	TILE_SET_INFO(0, code + (tile_bank * 0x1000), attr, TILE_FLIPY);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	palette_bankswitch(0);
	ZetReset();
	ZetClose();

	ZetReset(1);

	AY8910Reset(0);
	MSM5232Reset();
	DACReset();

	tile_bank = 0;
	sound_data = 0;
	sound_latch = 0;
	sound_nmi_pending = 0;
	sound_nmi_enabled = 0;
	sound_cpu_halted = 0;

	nCyclesExtra[0] = nCyclesExtra[1] = 0;

	biquad.reset();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x010000;
	DrvZ80ROM1		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x0c0000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x002000;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvVidRAM		= Next; Next += 0x000900;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 sprite_offset)
{
	INT32 Plane[4]  = { 0x30000*8, 0x30000*8+4, 0, 4 };
	INT32 XOffs[16] = { STEP4(3,-1), STEP4(8+3,-1), STEP4(128+3,-1), STEP4(128+8+3,-1) };
	INT32 YOffs[16] = { STEP8(0,16), STEP8(256,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x600000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x60000; i++) tmp[i] = DrvGfxROM0[i] ^ 0xff;

	GfxDecode(0x3000, 4,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);
	GfxDecode(0x0200, 4, 16, 16, Plane, XOffs, YOffs, 0x200, tmp + sprite_offset, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 sprite_offset)
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM0 + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x30000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x40000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x50000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x20000, k++, 1)) return 1;

		DrvGfxDecode(sprite_offset);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,			0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,				0xc000, 0xc8ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0xdc00, 0xdcff, MAP_RAM); // 0-9f spr, a0-bf scroll, c0-ff
	ZetMapMemory(DrvZ80RAM0,			0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(ladyfrog_main_write);
	ZetSetReadHandler(ladyfrog_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,			0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(ladyfrog_sound_write);
	ZetSetReadHandler(ladyfrog_sound_read);
	ZetClose();

	AY8910Init(0, 2000000, 0);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 4000000);

	MSM5232Init(2000000, 1);
	MSM5232SetCapacitors(1e-6, 1e-6, 1e-6, 1e-6, 1e-6, 1e-6, 1e-6, 1e-6);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_0);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_1);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_2);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_3);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_4);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_5);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_6);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_7);

	DACInit(0, 0, 1, ZetTotalCycles, 4000000);
	DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback,  8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4,  8,  8, 0xc0000, 0x000, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 16, 0x20000, 0x100, 0x0f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -1);
	GenericTilemapSetScrollCols(0, 32);

	// notch-filter out high-pitched sound which plays after jumping (ladyfrog)
	biquad.init(FILT_NOTCH, nBurnSoundRate, 7290, 5, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	biquad.exit();

	ZetExit();

	AY8910Exit(0);
	MSM5232Exit();
	DACExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x200; i++)
	{
		INT32 r = DrvPalRAM[0x000 + i] & 0xf;
		INT32 g = DrvPalRAM[0x000 + i] >> 4;
		INT32 b = DrvPalRAM[0x200 + i] & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static void draw_sprites()
{
	GenericTilesGfx *gfx = &GenericGfxData[1];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 offs  =(DrvSprRAM[0x9f - i] & 0x1f) * 4;
		INT32 sy    = DrvSprRAM[offs + 0];
		INT32 attr  = DrvSprRAM[offs + 1];
		INT32 code  = DrvSprRAM[offs + 2] | ((attr & 0x10) << 4);
		INT32 sx    = DrvSprRAM[offs + 3];
		INT32 color = attr & 0x0f;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;

		Draw16x16MaskTile(pTransDraw, code % gfx->code_mask, sx, (238 - sy) - 16, flipx, flipy, color, gfx->depth, 0xf, gfx->color_offset, gfx->gfxbase);

		if (sx > 240)
		{			
			Draw16x16MaskTile(pTransDraw, code, sx - 256, (238 - sy) - 16, flipx, flipy, color, gfx->depth, 0xf, gfx->color_offset, gfx->gfxbase);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	for (INT32 i = 0; i < 32; i++) {
		GenericTilemapSetScrollCol(0, i, DrvSprRAM[0xa0 + i]);
	}

	GenericTilemapDraw(0, pTransDraw, 0);

	draw_sprites();

	BurnTransferFlip(0, 1);
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
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { (INT32)(4000000 / 59.871277), (INT32)(4000000 / 59.871277) };
	INT32 nCyclesDone[2]  = { nCyclesExtra[0], nCyclesExtra[1] };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == nInterleave-1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == nInterleave/2 || i == nInterleave-1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		biquad.filter_buffer(pBurnSoundOut, nBurnSoundLen);
		MSM5232Update(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

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

		AY8910Scan(nAction, pnMin);
		MSM5232Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(palette_bank);
		SCAN_VAR(tile_bank);
		SCAN_VAR(sound_data);
		SCAN_VAR(sound_flag);
		SCAN_VAR(sound_latch);
		SCAN_VAR(sound_nmi_pending);
		SCAN_VAR(sound_nmi_enabled);
		SCAN_VAR(sound_cpu_halted);

		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		palette_bankswitch(palette_bank);
		ZetClose();
	}

	return 0;
}


// Lady Frog

static struct BurnRomInfo ladyfrogRomDesc[] = {
	{ "2.107",		0x10000, 0xfa4466e6, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "1.115",		0x08000, 0xb0932498, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "3.32",		0x10000, 0x8a27fc0a, 3 | BRF_GRA },           //  2 Graphics
	{ "4.33",		0x10000, 0xe1a137d3, 3 | BRF_GRA },           //  3
	{ "5.34",		0x10000, 0x7816925f, 3 | BRF_GRA },           //  4
	{ "6.8",		0x10000, 0x61b3baaa, 3 | BRF_GRA },           //  5
	{ "7.9",		0x10000, 0x88aaff58, 3 | BRF_GRA },           //  6
	{ "8.10",		0x10000, 0x8c73baa1, 3 | BRF_GRA },           //  7
};

STD_ROM_PICK(ladyfrog)
STD_ROM_FN(ladyfrog)

static INT32 LadyfrogInit()
{
	return DrvInit(0x20000);
}

struct BurnDriver BurnDrvLadyfrog = {
	"ladyfrog", NULL, NULL, NULL, "1990",
	"Lady Frog\0", NULL, "Mondial Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, ladyfrogRomInfo, ladyfrogRomName, NULL, NULL, NULL, NULL, LadyfrogInputInfo, LadyfrogDIPInfo,
	LadyfrogInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	208, 256, 3, 4
};


// Touche Me (set 1)

static struct BurnRomInfo touchemeRomDesc[] = {
	{ "2.ic107",	0x10000, 0x26f4580b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "1.ic115",	0x08000, 0x902589aa, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "3.ic32",		0x10000, 0x223b4435, 3 | BRF_GRA },           //  2 Graphics
	{ "4.ic33",		0x10000, 0x96dcc2f3, 3 | BRF_GRA },           //  3
	{ "5.ic34",		0x10000, 0xb8667a6b, 3 | BRF_GRA },           //  4
	{ "6.ic8",		0x10000, 0xd257382f, 3 | BRF_GRA },           //  5
	{ "7.ic9",		0x10000, 0xfeb1b974, 3 | BRF_GRA },           //  6
	{ "8.ic10",		0x10000, 0xfc6808bf, 3 | BRF_GRA },           //  7
};

STD_ROM_PICK(toucheme)
STD_ROM_FN(toucheme)

static INT32 TouchemeInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvToucheme = {
	"toucheme", NULL, NULL, NULL, "19??",
	"Touche Me (set 1)\0", NULL, "<unknown>", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, touchemeRomInfo, touchemeRomName, NULL, NULL, NULL, NULL, LadyfrogInputInfo, TouchemeDIPInfo,
	TouchemeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	208, 256, 3, 4
};


// Touche Me (set 2, harder)

static struct BurnRomInfo touchemeaRomDesc[] = {
	{ "2.ic107",	0x10000, 0x4e72312d, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "1.ic115",	0x08000, 0x902589aa, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code

	{ "3.ic32",		0x10000, 0x223b4435, 3 | BRF_GRA },           //  2 Graphics
	{ "4.ic33",		0x10000, 0x96dcc2f3, 3 | BRF_GRA },           //  3
	{ "5.ic34",		0x10000, 0xb8667a6b, 3 | BRF_GRA },           //  4
	{ "6.ic8",		0x10000, 0xd257382f, 3 | BRF_GRA },           //  5
	{ "7.ic9",		0x10000, 0xfeb1b974, 3 | BRF_GRA },           //  6
	{ "8.ic10",		0x10000, 0xfc6808bf, 3 | BRF_GRA },           //  7
};

STD_ROM_PICK(touchemea)
STD_ROM_FN(touchemea)

struct BurnDriver BurnDrvTouchemea = {
	"touchemea", "toucheme", NULL, NULL, "19??",
	"Touche Me (set 2, harder)\0", NULL, "<unknown>", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, touchemeaRomInfo, touchemeaRomName, NULL, NULL, NULL, NULL, LadyfrogInputInfo, TouchemeDIPInfo,
	TouchemeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	208, 256, 3, 4
};
