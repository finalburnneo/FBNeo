// FB Alpha Metal Freezer driver module
// Based on MAME driver by Angelo Salese (Kale)

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "t5182.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80ROMDec;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvScrollRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 z80_bank;
static UINT8 fg_tilebank;
static UINT8 rowscroll_enable;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[2];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static struct BurnInputInfo MetlfrzrInputList[] = {
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 start"	},
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 start"	},
	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Metlfrzr)

static struct BurnDIPInfo MetlfrzrDIPList[]=
{
	{0x11, 0xff, 0xff, 0xcf, NULL			},
	{0x12, 0xff, 0xff, 0xfe, NULL			},
	{0x13, 0xff, 0xff, 0x40, NULL			},

	{0   , 0xfe, 0   ,    2, "2-0"			},
	{0x11, 0x01, 0x01, 0x01, "No"			},
	{0x11, 0x01, 0x01, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "2-1"			},
	{0x11, 0x01, 0x02, 0x02, "No"			},
	{0x11, 0x01, 0x02, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "2-2"			},
	{0x11, 0x01, 0x04, 0x04, "No"			},
	{0x11, 0x01, 0x04, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "2-2"			},
	{0x11, 0x01, 0x08, 0x08, "No"			},
	{0x11, 0x01, 0x08, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "2-6"			},
	{0x11, 0x01, 0x40, 0x40, "No"			},
	{0x11, 0x01, 0x40, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "2-7"			},
	{0x11, 0x01, 0x80, 0x80, "No"			},
	{0x11, 0x01, 0x80, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x01, 0x00, "Upright"		},
	{0x12, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x02, 0x02, "Off"			},
	{0x12, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x04, 0x04, "Off"			},
	{0x12, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Coin A"		},
	{0x12, 0x01, 0x38, 0x00, "5 Coins 1 Credit"	},
	{0x12, 0x01, 0x38, 0x20, "4 Coins 1 Credit"	},
	{0x12, 0x01, 0x30, 0x10, "3 Coins 1 Credit"	},
	{0x12, 0x01, 0x30, 0x30, "2 Coins 1 Credit"	},
	{0x12, 0x01, 0x30, 0x38, "1 Coin  1 Credit"	},
	{0x12, 0x01, 0x30, 0x18, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x30, 0x28, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x30, 0x08, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0xc0, 0x80, "2 Coins 1 Credit"	},
	{0x12, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  1 Credit"	},
	{0x12, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x03, 0x02, "Off"			},
	{0x13, 0x01, 0x03, 0x03, "On"			},
	{0x13, 0x01, 0x03, 0x01, "Off"			},
	{0x13, 0x01, 0x03, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x0c, 0x08, "20k/50k/100k"		},
	{0x13, 0x01, 0x0c, 0x0c, "30k/80k/150k"		},
	{0x13, 0x01, 0x0c, 0x04, "50k/100k/200k"	},
	{0x13, 0x01, 0x0c, 0x00, "100k/200k/400k"	},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x30, 0x20, "1"			},
	{0x13, 0x01, 0x30, 0x10, "2"			},
	{0x13, 0x01, 0x30, 0x30, "3"			},
	{0x13, 0x01, 0x30, 0x00, "4"			},

	{0   , 0xfe, 0   ,    2, "Level Select"		},
	{0x13, 0x01, 0x40, 0x40, "Off"			},
	{0x13, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Metlfrzr)

static void bankswitch(INT32 data)
{
	z80_bank = data;

	INT32 nBank = ((data & 0x0c) >> 2) * 0x4000 + 0x10000;

	ZetMapMemory(DrvZ80ROM + nBank, 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall metlfrzr_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff80) == 0xd400) {
		t5182SharedRAM[address & 0x7f] = data;
		return;
	}

	switch (address)
	{
		case 0xd700:
			fg_tilebank = (data >> 4) & 1;
			rowscroll_enable = data & 0x02;
			bankswitch(data);
		return;

		case 0xd710:
			ZetClose();
			ZetOpen(1);
			t5182_setirq_callback(CPU_ASSERT);
			ZetClose();
			ZetOpen(0);
		return;

		case 0xd712:
		case 0xd713:
			t5182_semaphore_main = address & 1;
		return;
	}
}

static UINT8 __fastcall metlfrzr_main_read(UINT16 address)
{
	if ((address & 0xff80) == 0xd400) {
		return t5182SharedRAM[address & 0x7f];
	}

	switch (address)
	{
		case 0xd600:
			return DrvInputs[0];

		case 0xd601:
			return DrvInputs[1];

		case 0xd602:
			return (DrvInputs[2] & 0x30) | (DrvDips[0] & ~0x30);

		case 0xd603:
			return DrvDips[1];

		case 0xd604:
			return DrvDips[2];

		case 0xd711:
			return t5182_semaphore_snd;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	bankswitch(0);
	ZetClose();

	t5182Reset();

	fg_tilebank = 0;
	rowscroll_enable = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM	= Next; Next += 0x020000;
	DrvZ80ROMDec	= DrvZ80ROM + 0x008000;

	t5182ROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x080000;
	DrvGfxROM1	= Next; Next += 0x080000;

	DrvPalette	= (UINT32*)Next; Next += 0x0201 * sizeof(UINT32);

	AllRam		= Next;

	DrvPalRAM	= Next; Next += 0x000400;
	DrvScrollRAM	= Next; Next += 0x000100;
	DrvZ80RAM	= Next; Next += 0x002800;
	DrvVidRAM	= Next; Next += 0x001000;

	t5182SharedRAM	= Next; Next += 0x000100;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes[4]  = { STEP4(0,4) };
	INT32 XOffs0[8]  = { STEP4(16+3,-1), STEP4(3,-1) };
	INT32 XOffs1[16] = { STEP4(0,1), STEP4(16,1), STEP4(64*8,1), STEP4(64*8+16,1) };
	INT32 YOffs[16]  = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x40000);

	GfxDecode(0x2000, 4,  8,  8, Planes, XOffs0, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Planes, XOffs1, YOffs, 0x400, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static void DrvProgDecrypt()
{
	for (INT32 A = 0; A < 0x8000; A++)
	{
		DrvZ80ROMDec[A] = DrvZ80ROM[A];

		if (BIT(A,5) && !BIT(A,3))
			DrvZ80ROMDec[A] ^= 0x40;

		if (BIT(A,10) && !BIT(A,9) && BIT(A,3))
			DrvZ80ROMDec[A] ^= 0x20;

		if ((BIT(A,10) ^ BIT(A,9)) && BIT(A,1))
			DrvZ80ROMDec[A] ^= 0x02;

		if (BIT(A,9) || !BIT(A,5) || BIT(A,3))
			DrvZ80ROMDec[A] = BITSWAP08(DrvZ80ROMDec[A],7,6,1,4,3,2,5,0);

		/* decode the data */
		if (BIT(A,5))
			DrvZ80ROM[A] ^= 0x40;

		if (BIT(A,9) || !BIT(A,5))
			DrvZ80ROM[A] = BITSWAP08(DrvZ80ROM[A],7,6,1,4,3,2,5,0);
	}
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
		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x10000,  1, 1)) return 1;

		if (BurnLoadRom(t5182ROM   + 0x00000,  2, 1)) return 1;
		if (BurnLoadRom(t5182ROM   + 0x08000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00001,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x20001,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x20000,  7, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00001,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20001, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000, 11, 2)) return 1;

		DrvProgDecrypt();
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROMDec,	0x0000, 0x7fff, MAP_FETCHOP);
	ZetMapMemory(DrvPalRAM,		0xd000, 0xd3ff, MAP_RAM);
	ZetMapMemory(DrvScrollRAM,	0xd600, 0xd6ff, MAP_WRITE);
	ZetMapMemory(DrvVidRAM,		0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0xd800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(metlfrzr_main_write);
	ZetSetReadHandler(metlfrzr_main_read);
	ZetClose();

	t5182Init(1, 14318180/4);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	t5182Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x200; i++) {
		INT32 b = DrvPalRAM[i+0x200] & 0x0f;
		INT32 g = DrvPalRAM[i+0x000] >> 4;
		INT32 r = DrvPalRAM[i+0x000] & 0x0f;

		DrvPalette[i] = BurnHighCol(r+(r*16), g+(g*16), b+(b*16), 0);
	}

	DrvPalette[0x200] = 0;
}

static void draw_bg_layer() // Ripped directly from MAME, very ugly!
{
	INT32 x_scroll_shift;

	UINT16 x_scroll_value = DrvScrollRAM[0x17] + ((DrvScrollRAM[0x06] & 1) << 8);
	INT32 x_scroll_base = (x_scroll_value >> 3) * 32;

	for (INT32 count = 0; count < 32*33; count++)
	{
		INT32 tile_base = count;
		INT32 y = (count % 32);
		if(y > 7 || rowscroll_enable == 0) // TODO: this condition breaks on level 5 halfway thru.
		{
			tile_base+= x_scroll_base;
			x_scroll_shift = (x_scroll_value & 7);
		}
		else
			x_scroll_shift = 0;
		tile_base &= 0x7ff;
		INT32 x = (count / 32);

		UINT16 tile = DrvVidRAM[tile_base*2+0] + ((DrvVidRAM[tile_base*2+1] & 0xf0) << 4) + (fg_tilebank * 0x1000);
		UINT8 color = DrvVidRAM[tile_base*2+1] & 0xf;

		Render8x8Tile_Mask_Clip(pTransDraw, tile, x*8-x_scroll_shift, y*8 - 16, color, 4, 0xf, 0x100, DrvGfxROM0);
	}
}

static void draw_sprites()
{
	UINT8 *ram = DrvZ80RAM + 0x2600;

	for (INT32 count = 0x200 - 4; count >- 1; count -= 4)
	{
		INT32 code  = ram[count+0] + ((ram[count+1] & 0x70) << 4);
		UINT8 color = ram[count+1] & 0xf;
		INT32 sy    = ram[count+2];
		INT32 sx    = ram[count+3];
		if (ram[count+1] & 0x80) sx -= 256; // wrap

		Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 4, 0xf, 0, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
	//	DrvRecalc = 0;
		DrvPaletteUpdate();
	//}

	for (INT32 offs = 0; offs < nScreenWidth * nScreenHeight; offs++) {
		pTransDraw[offs] = 0x200;
	}

	draw_bg_layer();
	draw_sprites();

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
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		t5182_coin_input = (DrvJoy4[0] << 0) | (DrvJoy4[1] << 1);
	}

	INT32 nSegment;
	INT32 nInterleave = 16;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 6000000 / 60, 63079 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);

		nSegment = ((nCyclesTotal[0] / nInterleave) * (i + 1)) - ZetTotalCycles();

		nCyclesDone[0] += ZetRun(nSegment);

		if (i == 0) {
			ZetSetVector(0x08);
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}

		if (i == 15) {
			ZetSetVector(0x10);
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}

		ZetClose();

		ZetOpen(1); // t5182

		nSegment = ((nCyclesTotal[1] / nInterleave) * (i + 1)) - ZetTotalCycles();

		nCyclesDone[1] += ZetRun(nSegment);

		if (pBurnSoundOut) {
			nSegment = nBurnSoundLen / nInterleave;

			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);

			nSoundBufferPos += nSegment;
		}

		ZetClose();
	}

	ZetOpen(1); // t5182

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029729;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);

		t5182Scan(nAction);


		SCAN_VAR(z80_bank);
		SCAN_VAR(fg_tilebank);

	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(z80_bank);
		ZetClose();
	}

	return 0;
}


// Metal Freezer

static struct BurnRomInfo metlfrzrRomDesc[] = {
	{ "1.15j",		0x08000, 0xf59b5fa2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2.14j",		0x10000, 0x21ecc248, 1 | BRF_PRG | BRF_ESS }, //  1

#if !defined (ROM_VERIFY)
	{ "t5182.rom",		0x02000, 0xd354c8fc, 2 | BRF_PRG | BRF_ESS }, //  2 t5182 Code (Z80)
#endif
	{ "3.4h",		0x08000, 0x36f88e54, 2 | BRF_GRA },           //  3 t5182 External

	{ "10.5a",		0x10000, 0x3313e74a, 3 | BRF_GRA },           //  4 Background Tiles
	{ "12.7a",		0x10000, 0x6da5fda9, 3 | BRF_GRA },           //  5
	{ "11.6a",		0x10000, 0xfa6490b8, 3 | BRF_GRA },           //  6
	{ "13.9a",		0x10000, 0xa4f689ec, 3 | BRF_GRA },           //  7

	{ "14.13a",		0x10000, 0xa9cd5225, 4 | BRF_GRA },           //  8 Sprites
	{ "16.11a",		0x10000, 0x92f2cb49, 4 | BRF_GRA },           //  9
	{ "15.12a",		0x10000, 0xce5c4c8b, 4 | BRF_GRA },           // 10
	{ "17.10a",		0x10000, 0x3fec33f7, 4 | BRF_GRA },           // 11

	{ "n8s129a.7f",		0x00100, 0xc849d60b, 5 | BRF_OPT },           // 12 Proms (unused)
	{ "n82s135n.9c",	0x00100, 0x7bbd52db, 5 | BRF_OPT },           // 13

	{ "pld3.14h.bin",	0x00149, 0x8183f7f0, 6 | BRF_OPT },           // 14 PLDs
	{ "pld8.4d.bin",	0x00149, 0xf1e35034, 6 | BRF_OPT },           // 15
};

STD_ROM_PICK(metlfrzr)
STD_ROM_FN(metlfrzr)

struct BurnDriver BurnDrvMetlfrzr = {
	"metlfrzr", NULL, NULL, NULL, "1989",
	"Metal Freezer\0", NULL, "Seibu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, metlfrzrRomInfo, metlfrzrRomName, NULL, NULL, MetlfrzrInputInfo, MetlfrzrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
