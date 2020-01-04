// FB Alpha Tank Busters driver module
// Based on MAME driver by Jarek Burczynski

// Notes:
//  1) game has stuck sprite issues.  might be due to a unknown bad dump or
//  unhandled emulation of the device that sits at 0xe803.
//  2) *game seems to act more like PCB when e803 is used to return a random#
//  3) considering the tricks used in bagman (also Valadon) w/ the pal, its
//  safe to assume there is possibly something similar being used here.

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *e00x_data;

static UINT16 scrollx;
static UINT8 scrolly;
static UINT8 soundlatch;
static UINT8 sound_timer;
static UINT8 irq_mask;
static UINT8 variable_data;
static UINT8 bankdata;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo TankbustInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Tankbust)

static struct BurnDIPInfo TankbustDIPList[]=
{
	{0x09, 0xff, 0xff, 0x6b, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x09, 0x01, 0x03, 0x03, "Easy"						},
	{0x09, 0x01, 0x03, 0x02, "Hard"						},
	{0x09, 0x01, 0x03, 0x01, "Normal"					},
	{0x09, 0x01, 0x03, 0x00, "Very Hard"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x09, 0x01, 0x04, 0x04, "Off"						},
	{0x09, 0x01, 0x04, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Language"					},
	{0x09, 0x01, 0x08, 0x08, "English"					},
	{0x09, 0x01, 0x08, 0x00, "French"					},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x09, 0x01, 0x10, 0x10, "No Bonus"					},
	{0x09, 0x01, 0x10, 0x00, "60000"					},

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x09, 0x01, 0x20, 0x20, "1C/1C 1C/2C 1C/6C 1C/14C"	},
	{0x09, 0x01, 0x20, 0x00, "2C/1C 1C/1C 1C/3C 1C/7C"	},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x09, 0x01, 0xc0, 0xc0, "1"						},
	{0x09, 0x01, 0xc0, 0x80, "2"						},
	{0x09, 0x01, 0xc0, 0x40, "3"						},
	{0x09, 0x01, 0xc0, 0x00, "4"						},
};

STDDIPINFO(Tankbust)

static void bankswitch(INT32 data)
{
	bankdata = data & 1;

	ZetMapMemory(DrvZ80ROM0 + 0x10000 + bankdata * 0x4000,		0x6000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM0 + 0x18000 + bankdata * 0x2000,		0xa000, 0xbfff, MAP_ROM);
}

static void __fastcall tankbust_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0xe000) {
		e00x_data[address & 7] = data;
	//	return;
	}

	switch (address)
	{
		case 0xe000:
			irq_mask = data & 1;
		return;

		case 0xe001:
			if ((data & 1) == 0) {
				ZetSetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
			}
		return;

		case 0xe002: // coin counter
		case 0xe003: // nop
		case 0xe004: // nop
		case 0xe005: // nop
		case 0xe006: // nop
		return;

		case 0xe007:
			bankswitch(data);
		return;

		case 0xe800:
			scrolly = data;
		return;

		case 0xe801:
			scrollx = (scrollx & 0xff00) | (data << 0);
		return;

		case 0xe802:
			scrollx = (scrollx & 0x00ff) | (data << 8);
		return;

		case 0xe803:
			soundlatch = data;
		return;

		case 0xe804:
			// nop
		return;
	}
}

static UINT8 __fastcall tankbust_main_read(UINT16 address)
{	
	if ((address & 0xfff8) == 0xe000) {
		return e00x_data[address & 7];
	}

	switch (address)
	{
		case 0xe800:
			return DrvInputs[0];

		case 0xe801:
			return DrvInputs[1];

		case 0xe802:
			return DrvDips[0];

		case 0xe803:
			variable_data += 8;
			return variable_data;
	}

	return 0;
}

static void __fastcall tankbust_sound_write(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x10:
			AY8910Write(1, 1, data);
		return;

		case 0x30:
			AY8910Write(1, 0, data);
		return;

		case 0x40:
			AY8910Write(0, 1, data);
		return;

		case 0xc0:
			AY8910Write(0, 0, data);
		return;
	}
}

static UINT8 __fastcall tankbust_sound_read(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x30:
			return AY8910Read(1);

		case 0xc0:
			return AY8910Read(0);
	}

	return 0;
}

static UINT8 AY8910_0_portA(UINT32)
{
	ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
	return soundlatch;
}

static UINT8 AY8910_0_portB(UINT32)
{
	sound_timer++;
	return sound_timer;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvColRAM[offs];
	INT32 code = DrvVidRAM[offs] + ((attr & 7) * 256);

	TILE_SET_INFO(0, code, ((attr & 0x40) >> 4) | ((attr & 0x20) >> 5) | ((attr & 0x10) >> 3), TILE_GROUP((attr >> 3) & 1));
}

static tilemap_callback( tx )
{
	INT32 code = DrvTxtRAM[offs];

	TILE_SET_INFO(1, code, (code >> 7) | ((code >> 5) & 2), TILE_FLIPY);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	scrollx = 0;
	scrolly = 0;
	soundlatch = 0;
	sound_timer = 0;
	irq_mask = 0;
	variable_data = 0x11;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x01c000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x002000;

	DrvColPROM		= Next; Next += 0x000080;

	DrvPalette		= (UINT32*)Next; Next += 0x80 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvColRAM		= Next; Next += 0x000800;
	DrvTxtRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;
	
	e00x_data		= Next; Next += 0x000008;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { STEP4(0, 0x10000) };
	INT32 Plane1[3] = { 0x20000*0, 0x20000*1, 0x20000*2 };
	INT32 Plane2[1] = { 0 };
	INT32 XOffs[32] = { STEP8(0,1), STEP8(64,1), STEP8(256,1), STEP8(256+64,1) };
	INT32 YOffs[32] = { STEP8(7*8,-8), STEP8(23*8,-8), STEP8(71*8,-8), STEP8(87*8,-8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xC000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x8000);

	GfxDecode(0x0040, 4, 32, 32, Plane0, XOffs, YOffs, 0x400, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0xc000);

	GfxDecode(0x0800, 3,  8,  8, Plane1, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x2000);

	GfxDecode(0x0040, 1,  8,  8, Plane2, XOffs, YOffs, 0x040, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x18000,  2, 1)) return 1;
		memcpy (DrvZ80ROM0 + 0x12000, DrvZ80ROM0 + 0x18000, 0x02000);
		memcpy (DrvZ80ROM0 + 0x10000, DrvZ80ROM0 + 0x1a000, 0x02000);
		if (BurnLoadRom(DrvZ80ROM0 + 0x18000,  3, 1)) return 1;
		memcpy (DrvZ80ROM0 + 0x16000, DrvZ80ROM0 + 0x18000, 0x02000);
		memcpy (DrvZ80ROM0 + 0x14000, DrvZ80ROM0 + 0x1a000, 0x02000);
		if (BurnLoadRom(DrvZ80ROM0 + 0x18000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x06000,  9, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x00000, 10, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x04000, 11, 1, LD_INVERT)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x08000, 12, 1, LD_INVERT)) return 1;
		
		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 13, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00020, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00040, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00060, 17, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,			0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,				0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvColRAM,				0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,				0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0xd800, 0xd8ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,			0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(tankbust_main_write);
	ZetSetReadHandler(tankbust_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,			0x8000, 0x87ff, MAP_RAM);
	ZetSetOutHandler(tankbust_sound_write);
	ZetSetInHandler(tankbust_sound_read);
	ZetClose();

	AY8910Init(0, 894886, 0);
	AY8910Init(1, 894886, 1);
	AY8910SetPorts(0, &AY8910_0_portA, &AY8910_0_portB, NULL, NULL);
	AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.10, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, tx_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 3, 8, 8, 0x20000, 0x20, 0x7);
	GenericTilemapSetGfx(1, DrvGfxROM2, 1, 8, 8, 0x01000, 0x60, 0xf);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -128, -8);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 128; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 r = 0x55 * bit0 + 0xaa * bit1;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		INT32 code  = DrvSprRAM[offs] & 0x3f;
		INT32 flipy = DrvSprRAM[offs] & 0x40;
		INT32 flipx = DrvSprRAM[offs] & 0x80;
		INT32 sy    = (240- DrvSprRAM[offs+1]) - 14;
		INT32 sx    = (DrvSprRAM[offs+2] & 0x01) * 256 + DrvSprRAM[offs+3] - 7;

		if (sy == 222) continue; // junk sprite

		Draw32x32MaskTile(pTransDraw, code, sx - 128, sy - 8, flipx, flipy, 0, 4, 0, 0, DrvGfxROM0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	INT32 xscroll = scrollx & 0x1ff;
	if (xscroll >= 0x100) xscroll -= 0x200;

	INT32 yscroll = scrolly;
	if (yscroll & 0x80) yscroll -= 0x100;

	GenericTilemapSetScrollX(0, xscroll);
	GenericTilemapSetScrollY(0, yscroll);

	if (~nBurnLayer & 1) BurnTransferClear();

	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if ( nSpriteEnable & 1) draw_sprites();

	if ( nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));

	if ( nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 100;
	INT32 nTotalCycles[2] = { 7159090 / 60, 4772726 / 60 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		ZetRun(nTotalCycles[0] / nInterleave);
		if (i == nInterleave - 1 && irq_mask) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		ZetRun(nTotalCycles[1] / nInterleave);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029735;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_timer);
		SCAN_VAR(irq_mask);
		SCAN_VAR(variable_data);
		SCAN_VAR(bankdata);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}


// Tank Busters

static struct BurnRomInfo tankbustRomDesc[] = {
	{ "a-s4-6.bin",		0x4000, 0x8ebe7317, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a-s7-9.bin",		0x2000, 0x047aee33, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a-s5_7.bin",		0x4000, 0xdd4800ca, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a-s6-8.bin",		0x4000, 0xf8801238, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a-s8-10.bin",	0x4000, 0x9e826faa, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "a-b3-1.bin",		0x2000, 0xb0f56102, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 #1 Code

	{ "a-d5-2.bin",		0x2000, 0x0bbf3fdb, 3 | BRF_GRA },           //  6 Sprites
	{ "a-d6-3.bin",		0x2000, 0x4398dc21, 3 | BRF_GRA },           //  7
	{ "a-d7-4.bin",		0x2000, 0xaca197fc, 3 | BRF_GRA },           //  8
	{ "a-d8-5.bin",		0x2000, 0x1e6edc17, 3 | BRF_GRA },           //  9

	{ "b-m4-11.bin",	0x4000, 0xeb88ee1f, 4 | BRF_GRA },           // 10 Background Tiles
	{ "b-m5-12.bin",	0x4000, 0x4c65f399, 4 | BRF_GRA },           // 11
	{ "b-m6-13.bin",	0x4000, 0xa5baa413, 4 | BRF_GRA },           // 12

	{ "b-r3-14.bin",	0x2000, 0x4310a815, 5 | BRF_GRA },           // 13 Characters

	{ "tb-prom.1s8",	0x0020, 0xdfaa086c, 6 | BRF_GRA },           // 14 Color Data
	{ "tb-prom.2r8",	0x0020, 0xec50d674, 6 | BRF_GRA },           // 15
	{ "tb-prom.3p8",	0x0020, 0x3e70eafd, 6 | BRF_GRA },           // 16
	{ "tb-prom.4k8",	0x0020, 0x624f40d2, 6 | BRF_GRA },           // 17
};

STD_ROM_PICK(tankbust)
STD_ROM_FN(tankbust)

struct BurnDriver BurnDrvTankbust = {
	"tankbust", NULL, NULL, NULL, "1985",
	"Tank Busters\0", "Graphics and stuck sprite issues", "Valadon Automation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, tankbustRomInfo, tankbustRomName, NULL, NULL, NULL, NULL, TankbustInputInfo, TankbustDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 128,
	240, 320, 3, 4
};
