// FB Neo Capcom Baseball driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "eeprom.h"
#include "burn_ym2413.h"
#include "msm6295.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80OPS;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvSndROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvTextRAM;
static UINT8 *DrvScrollRAM;
static UINT8 *DrvSprRAM;

static UINT8 *scroll;

static INT32 flipscreen;
static INT32 tilebank;
static INT32 spritebank;
static INT32 text_on;
static INT32 bg_on;
static INT32 obj_on;
static INT32 bankdata;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 nCyclesExtra;

static struct BurnInputInfo CbasebalInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"		},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"		},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"		},

	{"Service",			BIT_DIGITAL,	DrvJoy3 + 2,	"service"		},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dips A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
};

STDINPUTINFO(Cbasebal)

static struct BurnDIPInfo CbasebalDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0x08, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x08, 0x08, "Off"				},
	{0x00, 0x01, 0x08, 0x00, "On"				},
};

STDDIPINFO(Cbasebal)

// kabuki.cpp
extern void kabuki_decode(UINT8 *src,UINT8 *dest_op,UINT8 *dest_data,
		INT32 base_addr,INT32 length,INT32 swap_key1,INT32 swap_key2,INT32 addr_key,INT32 xor_key);

static void bankswitch(INT32 data)
{
	UINT8 *ram[4] = { DrvScrollRAM, BurnPalRAM, DrvTextRAM, DrvScrollRAM };

	bankdata = data;
	INT32 bank = 0x10000 + (bankdata & 0x1f) * 0x4000;

	ZetMapMemory(DrvZ80ROM + bank,		0x8000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80OPS + bank,		0x8000, 0xbfff, MAP_FETCHOP);
	ZetMapMemory(ram[(data & 0xc0) >> 6],	0xc000, 0xcfff, MAP_RAM);
}

static void __fastcall cbasebal_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			bankswitch(data);
		return;

		case 0x01:
			EEPROMSetCSLine(data ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
		return;

		case 0x02:
			EEPROMSetClockLine(data ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
		return;

		case 0x03:
			EEPROMWriteBit(data);
		return;

		case 0x05:
			MSM6295Write(0, data);
		return;

		case 0x06:
		case 0x07:
			BurnYM2413Write(port & 1, data);
		return;

		case 0x08: // x low
		case 0x09: // x high
		case 0x0a: // y low
		case 0x0b: // y high
			scroll[port & 3] = data;
		return;

		case 0x13:
			flipscreen 	=  data & 0x02;
			tilebank 	= (data & 0x08) << 8;
			spritebank	= (data & 0x10) >> 4;
			text_on		= ~data & 0x20;
			bg_on		= ~data & 0x40;
			obj_on		= ~data & 0x80;
		return;

		case 0x14:
			// coin counter - data & 3
			// coin lockout - ~data & 0xc
		return;
	}
}

static UINT8 __fastcall cbasebal_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x10:
			return DrvInputs[0];

		case 0x11:
			return DrvInputs[1];

		case 0x12:
			return ((DrvInputs[2] ^ (vblank ? 0x40 : 0)) & ~0x88) | (DrvDips[0] & 0x08) | (EEPROMRead() ? 0x80 : 0);
	}

	return 0;
}

static tilemap_callback( bg )
{
	UINT16 attr = DrvScrollRAM[offs * 2 + 1];
	UINT16 code = DrvScrollRAM[offs * 2 + 0] | ((attr & 7) << 8) | tilebank;

	TILE_SET_INFO(1, code, attr >> 4, (attr & 0x08) ? TILE_FLIPX : 0);
}

static tilemap_callback( fg )
{
	UINT16 attr = DrvTextRAM[offs + 0x800];
	UINT16 code = DrvTextRAM[offs] | ((attr & 0xf0) << 4);

	TILE_SET_INFO(0, code, attr, (attr & 0x08) ? TILE_FLIPX : 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetClose();

	BurnYM2413Reset();
	MSM6295Reset();

	flipscreen = 0;
	tilebank = 0;
	spritebank = 0;
	text_on = 0;
	bg_on = 0;
	obj_on = 0;

	nCyclesExtra = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x090000;
	DrvZ80OPS		= Next; Next += 0x090000;

	DrvGfxROM[0]	= Next; Next += 0x040000;
	DrvGfxROM[1]	= Next; Next += 0x100000;
	DrvGfxROM[2]	= Next; Next += 0x100000;

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x040000;

	BurnPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x001e00;
	DrvSprRAM		= Next; Next += 0x000200;
	BurnPalRAM		= Next; Next += 0x001000;
	DrvTextRAM		= Next; Next += 0x001000;
	DrvScrollRAM	= Next; Next += 0x001000;

	scroll			= Next; Next += 0x000004;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 0,4 };
	INT32 XOffs0[8]  = { STEP4(8+3,-1), STEP4(3,-1) };
	INT32 Plane1[4]  = { 4096*64*8+4, 4096*64*8+0,4, 0 };
	INT32 XOffs1[16] = { STEP4(0,1), STEP4(8,1), STEP4(256,1), STEP4(256+8,1) };
	INT32 YOffs[16]  = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x10000);

	GfxDecode(0x1000, 2,  8,  8, Plane0, XOffs0, YOffs, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x80000);

	GfxDecode(0x1000, 4, 16, 16, Plane1, XOffs1, YOffs, 0x200, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x80000);

	GfxDecode(0x1000, 4, 16, 16, Plane1, XOffs1, YOffs, 0x200, tmp, DrvGfxROM[2]);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM    + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x30000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x50000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x40000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x60000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x40000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x60000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM    + 0x00000, k++, 1)) return 1;

		DrvGfxDecode();

		kabuki_decode(DrvZ80ROM, DrvZ80OPS, DrvZ80ROM, 0, 0x8000, 0x01234567, 0x76543210, 0x6548, 0x24);
		for (INT32 i = 0; i < 0x20; i++)
			kabuki_decode(DrvZ80ROM + 0x10000 + i * 0x4000, DrvZ80OPS + 0x10000 + i * 0x4000, DrvZ80ROM + 0x10000 + i * 0x4000, 0x8000, 0x4000, 0x01234567, 0x76543210, 0x6548, 0x24);
	}

 	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80OPS,				0x0000, 0x7fff, MAP_FETCHOP);
	ZetMapMemory(DrvZ80RAM,				0xe000, 0xfdff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0xfe00, 0xffff, MAP_RAM);
	ZetSetOutHandler(cbasebal_write_port);
	ZetSetInHandler(cbasebal_read_port);
	ZetClose();

	EEPROMInit(&eeprom_interface_93C46);

	BurnYM2413Init(16000000 / 4);
	BurnYM2413SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 16000000 / 16 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback,  8,  8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 2,  8,  8, 0x040000, 0x100, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x100000, 0x300, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, 0x100000, 0x200, 0x07);
	GenericTilemapSetTransparent(1, 0x03);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -64, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	EEPROMExit();
	MSM6295Exit();
	BurnYM2413Exit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites()
{
	for (INT32 offs = 0x200 - 8; offs >= 0; offs -= 4)
	{
		INT32 attr  = DrvSprRAM[offs + 1];
		INT32 code  = DrvSprRAM[offs] | ((attr & 0xe0) << 3) | (spritebank << 11);
		INT32 sx    = DrvSprRAM[offs + 3] | ((attr & 0x10) << 4);
		INT32 sy   =((DrvSprRAM[offs + 2] + 8) & 0xff) - 8;
		INT32 color = attr & 0x07;
		INT32 flipx = attr & 0x08;

		if (flipscreen)
		{
			sx = 496 - sx;
			sy = 240 - sy;
			flipx = !flipx;
		}

		DrawGfxMaskTile(0, 2, code, sx - 64, sy - 16, flipx, flipscreen, color, 0xf);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		BurnPaletteUpdate_xxxxBBBBRRRRGGGG();
		BurnRecalc = 1;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? TMAP_FLIPXY : 0);
	GenericTilemapSetScrollX(0, scroll[0] | (scroll[1] << 8));
	GenericTilemapSetScrollY(0, scroll[2] | (scroll[3] << 8));

	BurnTransferClear(0x300);

	if ((nBurnLayer & 1) && bg_on) GenericTilemapDraw(0, pTransDraw, 0);
	if ((nSpriteEnable & 1) && obj_on) draw_sprites();
	if ((nBurnLayer & 2) && text_on) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 16000000 / 2 / 60 };
	INT32 nCyclesDone[1] = { nCyclesExtra };

	ZetOpen(0);

//	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Zet);
		if (i == 16) {
			vblank = 0;
		}
		if (i == nInterleave-1) {
			vblank = 1;
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
	}

	if (pBurnSoundOut) {
		BurnYM2413Render(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		ZetScan(nAction);
		BurnYM2413Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(tilebank);
		SCAN_VAR(spritebank);
		SCAN_VAR(text_on);
		SCAN_VAR(bg_on);
		SCAN_VAR(obj_on);
		SCAN_VAR(bankdata);
		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	EEPROMScan(nAction, pnMin);

	return 0;
}


// Capcom Baseball (Japan)

static struct BurnRomInfo cbasebalRomDesc[] = {
	{ "cbj10.11j",		0x008000, 0xbbff0acc,  1 | BRF_PRG | BRF_ESS },	//  0 Z80 Code (Encrypted)
	{ "cbj07.16f",		0x020000, 0x8111d13f,  1 | BRF_PRG | BRF_ESS },	//  1 
	{ "cbj06.14f",		0x020000, 0x9aaa0e37,  1 | BRF_PRG | BRF_ESS },	//  2 
	{ "cbj05.13f",		0x020000, 0xd0089f37,  1 | BRF_PRG | BRF_ESS },	//  3 

	{ "cbj13.16m",		0x010000, 0x2359fa0a,  2 | BRF_GRA },			//  4 Characters

	{ "cbj02.1f",		0x020000, 0xd6740535,  3 | BRF_GRA },			//  5 Background Tiles
	{ "cbj03.2f",		0x020000, 0x88098dcd,  3 | BRF_GRA },			//  6 
	{ "cbj08.1j",		0x020000, 0x5f3344bf,  3 | BRF_GRA },			//  7 
	{ "cbj09.2j",		0x020000, 0xaafffdae,  3 | BRF_GRA },			//  8 

	{ "cbj11.1m",		0x020000, 0xbdc1507d,  4 | BRF_GRA },			//  9 Sprites
	{ "cbj12.2m",		0x020000, 0x973f3efe,  4 | BRF_GRA },			// 10 
	{ "cbj14.1n",		0x020000, 0x765dabaa,  4 | BRF_GRA },			// 11 
	{ "cbj15.2n",		0x020000, 0x74756de5,  4 | BRF_GRA },			// 12 

	{ "cbj01.1e",		0x020000, 0x1d8968bd,  5 | BRF_SND },			// 13 Samples
};

STD_ROM_PICK(cbasebal)
STD_ROM_FN(cbasebal)

struct BurnDriver BurnDrvCbasebal = {
	"cbasebal", NULL, NULL, NULL, "1989",
	"Capcom Baseball (Japan)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_SPORTSMISC, 0,
	NULL, cbasebalRomInfo, cbasebalRomName, NULL, NULL, NULL, NULL, CbasebalInputInfo, CbasebalDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x400,
	384, 224, 4, 3
};
