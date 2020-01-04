// FB Alpha Jaleco D-Day driver module
// Based on MAME driver by Pierpaolo Prazzoli, Tomasz Slanina and Angelo Salese

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bank_address;
static UINT8 char_bank;
static UINT8 dma_data[4][2];
static UINT8 dma_flip[4];
static UINT8 nmi_enable[2];
static UINT8 soundlatch;
static UINT8 prot_addr;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo DdayjlcInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 4,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ddayjlc)

static struct BurnDIPInfo DdayjlcDIPList[]=
{
	{0x0c, 0xff, 0xff, 0x00, NULL					},
	{0x0d, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0c, 0x01, 0x03, 0x00, "3"					},
	{0x0c, 0x01, 0x03, 0x01, "4"					},
	{0x0c, 0x01, 0x03, 0x02, "5"					},
	{0x0c, 0x01, 0x03, 0x03, "6"					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x0c, 0x01, 0x1c, 0x0c, "4 Coins 1 Credits"	},
	{0x0c, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x0c, 0x01, 0x1c, 0x04, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0x1c, 0x18, "2 Coins 3 Credits"	},
	{0x0c, 0x01, 0x1c, 0x10, "1 Coin  2 Credits"	},
	{0x0c, 0x01, 0x1c, 0x14, "1 Coin  3 Credits"	},
	{0x0c, 0x01, 0x1c, 0x1c, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Extend"				},
	{0x0c, 0x01, 0x20, 0x00, "30K"					},
	{0x0c, 0x01, 0x20, 0x20, "50K"					},
	
	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x0c, 0x01, 0x40, 0x00, "Easy"					},
	{0x0c, 0x01, 0x40, 0x40, "Hard"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0c, 0x01, 0x80, 0x00, "Upright"				},
	{0x0c, 0x01, 0x80, 0x80, "Cocktail"				},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x0d, 0x01, 0x07, 0x03, "4 Coins 1 Credits"	},
	{0x0d, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x0d, 0x01, 0x07, 0x01, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x07, 0x06, "2 Coins 3 Credits"	},
	{0x0d, 0x01, 0x07, 0x04, "1 Coin  2 Credits"	},
	{0x0d, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x0d, 0x01, 0x07, 0x07, "Free Play"			},
};

STDDIPINFO(Ddayjlc)

static void bankswitch()
{
	ZetMapMemory(DrvZ80ROM0 + 0x10000 + (bank_address * 0x4000), 0xa000, 0xdfff, MAP_ROM);
}

static void dma_start(UINT8 data)
{
	if (data == 0)
	{
		UINT16 src  = (dma_data[0][1] * 256) + dma_data[0][0];
		UINT16 size = (dma_data[1][1] * 256) + dma_data[1][0];
		UINT16 dst  = (dma_data[2][1] * 256) + dma_data[2][0];

		for (INT32 i = 0; i <= (size & 0x3ff); i++) // yes, size + 1
		{
			ZetWriteByte(dst++, ZetReadByte(src++));
		}

		memset (dma_flip, 0, sizeof(dma_flip));
	}
}

static void __fastcall ddayjc_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0xa000 && address <= 0xdfff) return; // nop

	switch (address)
	{
		case 0xe000:
		case 0xe001:
		case 0xe002:
		case 0xe003:
			dma_data[address & 3][dma_flip[address & 3]] = data;
			dma_flip[address & 3] ^= 1;
		return;

		case 0xe008:
		return; // nop

		case 0xf000:
			soundlatch = data;
			ZetSetVector(1, 0xff);
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_HOLD);
		return;

		case 0xf080:
			char_bank = data;
		return;

		case 0xf081:
		return; // nop

		case 0xf083:
			dma_start(data);
		return;

		case 0xf084:
			bank_address = (bank_address & 0xfe) | (data & 1);
		return;

		case 0xf085:
			bank_address = (bank_address & 0xfd) | ((data & 1) << 1);
		return;

		case 0xf086:
			bank_address = (bank_address & 0xfb) | ((data & 1) << 2);
			if (bank_address > 2) bank_address = 0;
			bankswitch();
		return;

		case 0xf101:
			nmi_enable[0] = data;
		return;

		case 0xf102:
		case 0xf103:
		case 0xf104:
		case 0xf105:
			prot_addr = (prot_addr & (~(1 << (address - 0xf102)))) | ((data & 1) << (address - 0xf102));
		return;
	}
}

static UINT8 __fastcall ddayjc_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xf000:
			return DrvInputs[0];

		case 0xf100:
		{
			static const UINT8 prot_data[0x10] = {
				0x02, 0x02, 0x02, 0x02,	0x02, 0x00, 0x02, 0x00, 0x02, 0x02, 0x02, 0x00,	0x03, 0x01, 0x00, 0x03
			};

			return (DrvInputs[1] & 0x9f) | ((prot_data[prot_addr] << 5) & 0x60);
		}

		case 0xf180:
			return DrvDips[0];

		case 0xf200:
			return DrvDips[1];
	}

	return 0;
}

static void __fastcall ddayjc_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3000:
			AY8910Write(0, 1, data);
		return;

		case 0x4000:
			AY8910Write(0, 0, data);
		return;
			
		case 0x5000:
			AY8910Write(1, 1, data);
		return;

		case 0x6000:
			AY8910Write(1, 0, data);
		return;

		case 0x7000:
			nmi_enable[1] = data;
		return;
	}
}

static UINT8 __fastcall ddayjc_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			return AY8910Read(0);

		case 0x5000:
			return AY8910Read(1);
	}

	return 0;
}

static tilemap_callback( bg )
{
	UINT8 attr = DrvBgRAM[offs + 0x400];
	UINT16 code = DrvBgRAM[offs] + ((attr & 0x08) << 5);

	TILE_SET_INFO(0, code, (attr & 0x07) | ((attr >> 3) & 8), 0);
}

static tilemap_callback( fg )
{
	UINT8 code = DrvVidRAM[offs];

	INT32 flags = 0;
	INT32 sx = offs & 0x1f;
	if (sx < 2 || sx >= 30) flags = TILE_OPAQUE;

	TILE_SET_INFO(1, code + (char_bank * 0x100), 2, flags);
}

static UINT8 AY8910_0_portA(UINT32)
{
	return soundlatch;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bank_address = 0;
	bankswitch();
	ZetReset();
	ZetClose();

	ZetReset(1);

	AY8910Reset(0);
	AY8910Reset(1);

	char_bank = 0;
	memset (dma_data, 0, sizeof(dma_data));
	memset (dma_flip, 0, sizeof(dma_flip));
	memset (nmi_enable, 0, sizeof(nmi_enable));
	soundlatch = 0;
	prot_addr = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x01c000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x008000;
	DrvGfxROM2		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000400;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvBgRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2]  = { 0, 0x4000*8 };
	INT32 XOffs[16] = { STEP8(64,1), STEP8(0,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x8000; i++)
	{
		tmp[(i & 0x4007) | ((i & 0x2000) >> 10) | ((i & 0x1ff8) << 1)] = DrvGfxROM0[i];
	}

	GfxDecode(0x0200, 2, 16, 16, Plane, XOffs + 0, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x8000);

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs + 8, YOffs, 0x040, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x8000);

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs + 8, YOffs, 0x040, tmp, DrvGfxROM2);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x06000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x12000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x14000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x16000,  7, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x18000,  8, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x1a000,  9, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x06000, 14, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x04000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 16, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x04000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 18, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 19, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 20, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 21, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00300, 22, 1)) return 1;
	//	if (BurnLoadRom(DrvColPROM + 0x00400, 23, 1)) return 1; // not used

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0x9400, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,			0x9800, 0x9fff, MAP_RAM);
//	bank a000-dfff - mapped in DrvDoReset()
	ZetSetWriteHandler(ddayjc_main_write);
	ZetSetReadHandler(ddayjc_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x2000, 0x23ff, MAP_RAM);
	ZetSetWriteHandler(ddayjc_sound_write);
	ZetSetReadHandler(ddayjc_sound_read);
	ZetClose();

	AY8910Init(0, 2000000, 0);
	AY8910Init(1, 2000000, 1);
	AY8910SetPorts(0, &AY8910_0_portA, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.35, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.35, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM2, 2, 8, 8, 0x8000, 0x100, 0xf);
	GenericTilemapSetGfx(1, DrvGfxROM1, 2, 8, 8, 0x8000, 0x080, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x200; i++)
	{
		UINT8 d = (DrvColPROM[i]) | (DrvColPROM[i + 0x200] * 16);

		INT32 bit0 = (d >> 0) & 0x01;
		INT32 bit1 = (d >> 1) & 0x01;
		INT32 bit2 = (d >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (d >> 3) & 0x01;
		bit1 = (d >> 4) & 0x01;
		bit2 = (d >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (d >> 6) & 0x01;
		bit2 = (d >> 7) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x400; i += 4)
	{
		INT32 sy    = 256 - DrvSprRAM[i + 0] - 24;
		INT32 attr  = DrvSprRAM[i + 2];
		INT32 code  = DrvSprRAM[i + 1];
		INT32 sx    = DrvSprRAM[i + 3] - 16;
		INT32 flipx = attr & 0x80;
		INT32 flipy = code & 0x80;
		INT32 color = attr & 0xf;

		code = (code & 0x7f) | ((attr & 0x30) << 3);

		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 2, 0, 0, DrvGfxROM0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	GenericTilemapSetScrollX(0, DrvBgRAM[0] + 8);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0;
		DrvInputs[1] = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		ProcessJoystick(&DrvInputs[0], 0, 0,1,3,2, INPUT_4WAY);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (nmi_enable[0] && i == 240) ZetNmi();
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if (nmi_enable[1] && i == 240) ZetNmi();
		ZetClose();
	}

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

		SCAN_VAR(bank_address);
		SCAN_VAR(char_bank);
		SCAN_VAR(dma_data);
		SCAN_VAR(dma_flip);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(prot_addr);
	}

	return 0;
}


// D-Day (Jaleco set 1)

static struct BurnRomInfo ddayjlcRomDesc[] = {
	{ "1",		0x2000, 0xdbfb8772, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2",		0x2000, 0xf40ea53e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3",		0x2000, 0x0780ef60, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4",		0x2000, 0x75991a24, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5",		0x2000, 0x299b05f2, 1 | BRF_PRG | BRF_ESS }, //  4 Bank Data
	{ "6",		0x2000, 0x38ae2616, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7",		0x2000, 0x4210f6ef, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8",		0x2000, 0x91a32130, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "9",		0x2000, 0xccb82f09, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "10",		0x2000, 0x5452aba1, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "11",		0x2000, 0xfe4de019, 2 | BRF_PRG | BRF_ESS }, // 10 Z80 #1 Code

	{ "16",		0x2000, 0xa167fe9a, 3 | BRF_GRA },           // 11 Sprites
	{ "17",		0x2000, 0x13ffe662, 3 | BRF_GRA },           // 12
	{ "18",		0x2000, 0xdebe6531, 3 | BRF_GRA },           // 13
	{ "19",		0x2000, 0x5816f947, 3 | BRF_GRA },           // 14

	{ "14",		0x1000, 0x2c0e9bbe, 4 | BRF_GRA },           // 15 Foreground Tiles
	{ "15",		0x1000, 0xa6eeaa50, 4 | BRF_GRA },           // 16

	{ "12",		0x1000, 0x7f7afe80, 5 | BRF_GRA },           // 17 Background Tiles
	{ "13",		0x1000, 0xf169b93f, 5 | BRF_GRA },           // 18

	{ "4l.bin",	0x0100, 0x2c3fa534, 6 | BRF_GRA },           // 19 Color Data
	{ "5p.bin",	0x0100, 0x4fd96b26, 6 | BRF_GRA },           // 20
	{ "4m.bin",	0x0100, 0xe0ab9a8f, 6 | BRF_GRA },           // 21
	{ "5n.bin",	0x0100, 0x61d85970, 6 | BRF_GRA },           // 22

	{ "3l.bin",	0x0100, 0xda6fe846, 0 | BRF_OPT },           // 23 Unused?
};

STD_ROM_PICK(ddayjlc)
STD_ROM_FN(ddayjlc)

struct BurnDriver BurnDrvDdayjlc = {
	"ddayjlc", NULL, NULL, NULL, "1984",
	"D-Day (Jaleco set 1)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, ddayjlcRomInfo, ddayjlcRomName, NULL, NULL, NULL, NULL, DdayjlcInputInfo, DdayjlcDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// D-Day (Jaleco set 2)

static struct BurnRomInfo ddayjlcaRomDesc[] = {
	{ "1a",		0x2000, 0xd8e4f3d4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2",		0x2000, 0xf40ea53e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3",		0x2000, 0x0780ef60, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4",		0x2000, 0x75991a24, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5",		0x2000, 0x299b05f2, 1 | BRF_PRG | BRF_ESS }, //  4 Bank Data
	{ "6",		0x2000, 0x38ae2616, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "7",		0x2000, 0x4210f6ef, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "8",		0x2000, 0x91a32130, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "9",		0x2000, 0xccb82f09, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "10",		0x2000, 0x5452aba1, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "11",		0x2000, 0xfe4de019, 2 | BRF_PRG | BRF_ESS }, // 10 Z80 #1 Code

	{ "16",		0x2000, 0xa167fe9a, 3 | BRF_GRA },           // 11 Sprites
	{ "17",		0x2000, 0x13ffe662, 3 | BRF_GRA },           // 12
	{ "18",		0x2000, 0xdebe6531, 3 | BRF_GRA },           // 13
	{ "19",		0x2000, 0x5816f947, 3 | BRF_GRA },           // 14

	{ "14",		0x1000, 0x2c0e9bbe, 4 | BRF_GRA },           // 15 Foreground Tiles
	{ "15",		0x1000, 0xa6eeaa50, 4 | BRF_GRA },           // 16

	{ "12",		0x1000, 0x7f7afe80, 5 | BRF_GRA },           // 17 Background Tiles
	{ "13",		0x1000, 0xf169b93f, 5 | BRF_GRA },           // 18

	{ "4l.bin",	0x0100, 0x2c3fa534, 6 | BRF_GRA },           // 19 Color Data
	{ "5p.bin",	0x0100, 0x4fd96b26, 6 | BRF_GRA },           // 20
	{ "4m.bin",	0x0100, 0xe0ab9a8f, 6 | BRF_GRA },           // 21
	{ "5n.bin",	0x0100, 0x61d85970, 6 | BRF_GRA },           // 22

	{ "3l.bin",	0x0100, 0xda6fe846, 0 | BRF_OPT },           // 23 Unused?
};

STD_ROM_PICK(ddayjlca)
STD_ROM_FN(ddayjlca)

struct BurnDriver BurnDrvDdayjlca = {
	"ddayjlca", "ddayjlc", NULL, NULL, "1984",
	"D-Day (Jaleco set 2)\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, ddayjlcaRomInfo, ddayjlcaRomName, NULL, NULL, NULL, NULL, DdayjlcInputInfo, DdayjlcDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
