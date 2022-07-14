// FB Neo Exerion driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "resnet.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvGfxROM[4];
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM[2];
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;

static UINT8 *DrvTransTab; // spr transtab

static INT32 Scanline;     // background synth
static INT32 Scanline_last;
static UINT16 *Background;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles[2];

static INT32 vblank;
static INT32 sublatch;
static INT32 flipscreen;
static INT32 previous_coin;
static INT32 portA;
static INT32 char_palette;
static INT32 char_bank;
static INT32 sprite_palette;
static UINT8 *background_latches;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[1];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo ExerionInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Exerion)

static struct BurnDIPInfo ExerionDIPList[]=
{
	{0x10, 0xff, 0xff, 0xa2, NULL					},
	{0x11, 0xff, 0xff, 0xf0, NULL					},

	{0   , 0xfe, 0   ,    8, "Lives"				},
	{0x10, 0x01, 0x07, 0x00, "1"					},
	{0x10, 0x01, 0x07, 0x01, "2"					},
	{0x10, 0x01, 0x07, 0x02, "3"					},
	{0x10, 0x01, 0x07, 0x03, "4"					},
	{0x10, 0x01, 0x07, 0x04, "5"					},
	{0x10, 0x01, 0x07, 0x05, "5"					},
	{0x10, 0x01, 0x07, 0x06, "5"					},
	{0x10, 0x01, 0x07, 0x07, "254 (Cheat)"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x10, 0x01, 0x18, 0x00, "10000"				},
	{0x10, 0x01, 0x18, 0x08, "20000"				},
	{0x10, 0x01, 0x18, 0x10, "30000"				},
	{0x10, 0x01, 0x18, 0x18, "40000"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x10, 0x01, 0x60, 0x00, "Easy"					},
	{0x10, 0x01, 0x60, 0x20, "Medium"				},
	{0x10, 0x01, 0x60, 0x40, "Hard"					},
	{0x10, 0x01, 0x60, 0x60, "Hardest"				},
	
	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x10, 0x01, 0x80, 0x00, "Upright"				},
	{0x10, 0x01, 0x80, 0x80, "Cocktail"				},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x11, 0x01, 0x0e, 0x0e, "5 Coins 1 Credit"		},
	{0x11, 0x01, 0x0e, 0x0a, "4 Coins 1 Credit"		},
	{0x11, 0x01, 0x0e, 0x06, "3 Coins 1 Credit"		},
	{0x11, 0x01, 0x0e, 0x02, "2 Coins 1 Credit"		},
	{0x11, 0x01, 0x0e, 0x00, "1 Coin  1 Credit"		},
	{0x11, 0x01, 0x0e, 0x04, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x0e, 0x08, "1 Coin  3 Credits"	},
	{0x11, 0x01, 0x0e, 0x0c, "1 Coin  4 Credits"	},
};

STDDIPINFO(Exerion)

static UINT8 protection_read(INT32 offset)
{
	if (ZetGetPC(-1) == 0x4143)
		return DrvZ80ROM[0][0x33c0 + (DrvZ80RAM[0][0xd] << 2) + offset];
	else
		return DrvZ80RAM[0][0x8 + offset];
}

static void cpu_sync()
{
	INT32 cyc = ZetTotalCycles(0) - ZetTotalCycles(1);
	if (cyc > 0) {
		ZetRun(1, cyc);
	}
}

static void __fastcall exerion_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
			flipscreen 		= (data & 0x01);
			char_palette 	= (data & 0x06) >> 1;
			char_bank 		= (data & 0x08) >> 3;
			sprite_palette	= (data & 0xc0) >> 6;
		return;

		case 0xc800:
			cpu_sync();
			sublatch = data;
		return;

		case 0xd000:
		case 0xd001:
		case 0xd800:
		case 0xd801:
			AY8910Write((address >> 11) & 1, address & 1, data);
		return;
	}
}

static UINT8 __fastcall exerion_main_read(UINT16 address)
{
	if ((address & 0xf000) == 0x6000) {
		if (address >= 0x6008 && address <= 0x600b) {
			return protection_read(address - 0x6008);
		}
		return DrvZ80RAM[0][address & 0xfff];
	}

	switch (address)
	{
		case 0xa000:
			return (DrvInputs[0] & 0xc0) | (DrvInputs[1 + flipscreen] & 0x3f);

		case 0xa800:
			return DrvDips[0];

		case 0xb000:
			return (DrvDips[1] & 0xfe) | vblank;

		case 0xd802:
			return AY8910Read(1);
	}

	return 0;
}

static void do_background(INT32 y);

static void __fastcall exerion_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0x8002:
		case 0x8003:
		case 0x8004:
		case 0x8005:
		case 0x8006:
		case 0x8007:
		case 0x8008:
		case 0x8009:
		case 0x800a:
		case 0x800b:
		case 0x800c:
			if (Scanline > 0) do_background(Scanline - 1);
			background_latches[address & 0xf] = data;
		return;
	}
}

static UINT8 __fastcall exerion_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
			return sublatch;

		case 0xa000: {
			INT32 xpos = ((ZetTotalCycles() * 2) % 424) + 88;
			INT32 n = 1;

			if (xpos >= 384 && !vblank)
				n = (~xpos & 0x40) >> 6;

			return (vblank << 1) | n;
		}
	}

	return 0;
}

static UINT8 AY8910_1_portA_read(UINT32)
{
	portA ^= 0x40;
	return portA;
}

static void AY8910_1_portB_write(UINT32,UINT32)
{
	portA = DrvZ80ROM[0][0x5f76];
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1);

	AY8910Reset(0);
	AY8910Reset(1);

	sublatch = 0;
	flipscreen = 0;
	previous_coin = 0;
	portA = 0;
	char_palette = 0;
	char_bank = 0;
	sprite_palette = 0;

	nExtraCycles[0] = nExtraCycles[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x010000;
	DrvZ80ROM[1]		= Next; Next += 0x010000;

	DrvGfxROM[0]		= Next; Next += 0x008000;
	DrvGfxROM[1]		= Next; Next += 0x010000; // 16x16 sprites
	DrvGfxROM[3]		= Next; Next += 0x040000; // 32x32 sprites (doubled above)
	DrvGfxROM[2]		= Next; Next += 0x008000;

	DrvColPROM			= Next; Next += 0x000620;

	DrvPalette			= (UINT32*)Next; Next += 0x300 * sizeof(UINT32);

	DrvTransTab         = (UINT8*)Next; Next += 0x200;

	AllRam				= Next;

	DrvZ80RAM[0]		= Next; Next += 0x001000;
	DrvZ80RAM[1]		= Next; Next += 0x000800;

	DrvSprRAM			= Next; Next += 0x000400;
	DrvVidRAM			= Next; Next += 0x000800;

	background_latches	= Next; Next += 0x000010;

	RamEnd				= Next;

	Background          = (UINT16*)Next; Next += 512*256*sizeof(UINT16);

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2]   = { 0, 4 };
	INT32 XOffs[16]  = { STEP4(3,-1), STEP4(8+3,-1), STEP4(16+3,-1), STEP4(24+3,-1) };
	INT32 YOffs0[8]  = { STEP8(0,16) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs, YOffs0, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x4000);

	GfxDecode(0x0100, 2, 16, 16, Plane, XOffs, YOffs1, 0x200, tmp, DrvGfxROM[1]);

	INT32 XOffsDouble[32];
	INT32 YOffsDouble[32];
	for (INT32 i = 0; i < 32; i++) {
		XOffsDouble[i] = XOffs[i / 2];
		YOffsDouble[i] = YOffs1[i / 2];
	}

	GfxDecode(0x0100, 2, 32, 32, Plane, XOffsDouble, YOffsDouble, 0x200, tmp, DrvGfxROM[3]);

	BurnFree(tmp);

	return 0;
}

static void exerion_decode()
{
	UINT8 *src = (UINT8*)BurnMalloc(0x10000);
	UINT8 *dst = DrvGfxROM[0];

	memcpy(src, dst, 0x2000);

	for (INT32 i = 0; i < 0x2000; i++)
	{
		dst[(i & 0x1f01) | ((i << 3) & 0xf0) | ((i >> 4) & 0xe)] = src[i];
	}

	dst = DrvGfxROM[1];
	memcpy(src, dst, 0x4000);

	for (INT32 i = 0; i < 0x4000; i++)
	{
		INT32 j = ((i << 1) & 0x3c00) | ((i >> 4) & 0x0200) | ((i << 4) & 0x01c0) | ((i >> 3) & 0x003c) | (i & 0xc003);
		dst[j] = src[i];
	}

	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "irion")) {
		for (INT32 i = 0; i < 0x4000; i += 0x400) {
			for (INT32 j = 0; j < 0x200; j++) {
				UINT8 temp = dst[i + j];
				dst[i + j] = dst[i + j + 0x200];
				dst[i + j + 0x200] = temp;
			}
		}
	}

	BurnFree(src);
}

static void build_bitmaps()
{
	for (INT32 i = 0; i < 4; i++)
	{
		UINT8 *src = DrvGfxROM[2] + i * 0x2000;
		UINT16 *dst = BurnBitmapGetBitmap(i+1);

		for (INT32 y = 0; y < 0x100; y++)
		{
			for (INT32 x = 0; x < 0x80; x += 4)
			{
				UINT8 data = *src++;

				UINT16 val = ((data >> 3) & 2) | ((data >> 0) & 1);
				if (val) val |= 0x100 >> i;
				*dst++ = val << (2 * i);

				val = ((data >> 4) & 2) | ((data >> 1) & 1);
				if (val) val |= 0x100 >> i;
				*dst++ = val << (2 * i);

				val = ((data >> 5) & 2) | ((data >> 2) & 1);
				if (val) val |= 0x100 >> i;
				*dst++ = val << (2 * i);

				val = ((data >> 6) & 2) | ((data >> 3) & 1);
				if (val) val |= 0x100 >> i;
				*dst++ = val << (2 * i);
			}

			for (INT32 x = 0x80; x < 0x100; x++)
				*dst++ = 0;
		}
	}
}

static INT32 DrvLoadRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[8] = { DrvZ80ROM[0], DrvZ80ROM[1] };
	UINT8 *gLoad[8] = { DrvGfxROM[0], DrvGfxROM[1], DrvGfxROM[2], DrvColPROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		INT32 id = ri.nType & 7;

		if ((ri.nType & BRF_PRG) && (id >= 1 && id <= 2)) // PRG
		{
			if (bLoad) {
				//bprintf (0, _T("PRG%d: %5.5x, %d\n"), id, pLoad[id-1] - DrvZ80ROM[id-1], i);
				if (BurnLoadRom(pLoad[id-1], i, 1)) return 1;
			}
			pLoad[id-1] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (id >= 1 && id <= 3)) // GFX
		{
			if (bLoad) {
				//bprintf (0, _T("GFX%d: %5.5x, %d\n"), id, gLoad[id-1] - DrvGfxROM[id-1], i);
				if (BurnLoadRom(gLoad[id-1], i, 1)) return 1;
			}
			gLoad[id-1] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_GRA) && id == 4) // PROMS
		{
			if (bLoad) {
				//bprintf (0, _T("GFX PROMS%d: %5.5x, %d\n"), id, gLoad[id-1] - DrvColPROM, i);
				if (BurnLoadRom(gLoad[id-1], i, 1)) return 1;
			}
			gLoad[id-1] += ri.nLen;
			continue;
		}
	}

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms(1)) return 1;
	exerion_decode();
	DrvGfxDecode();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0], 			0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[0],			0x6000, 0x6fff, MAP_WRITE | MAP_FETCH);
	ZetMapMemory(DrvZ80RAM[0] + 0x100,	0x6100, 0x6fff, MAP_READ);
	ZetMapMemory(DrvVidRAM,				0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,				0x8800, 0x8bff, MAP_RAM); // 0-7f
	ZetSetWriteHandler(exerion_main_write);
	ZetSetReadHandler(exerion_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],			0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],			0x4000, 0x47ff, MAP_RAM);
	ZetSetWriteHandler(exerion_sub_write);
	ZetSetReadHandler(exerion_sub_read);
	ZetClose();

	AY8910Init(0, 1664000, 0);
	AY8910Init(1, 1664000, 1);
	AY8910SetPorts(1, &AY8910_1_portA_read, NULL, NULL, &AY8910_1_portB_write);
	AY8910SetBuffered(ZetTotalCycles, 3328000);
	AY8910SetAllRoutes(0, 0.33, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.33, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	BurnBitmapAllocate(1, 256, 256, false);
	BurnBitmapAllocate(2, 256, 256, false);
	BurnBitmapAllocate(3, 256, 256, false);
	BurnBitmapAllocate(4, 256, 256, false);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 2,  8,  8, 0x08000, 0x000, 0x3f);
	build_bitmaps();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 tmp[0x20];

	INT32 resistances_rg[3] = { 1000, 470, 220 };
	INT32 resistances_b [2] = { 470, 220 };
	double rweights[3], gweights[3], bweights[2];

	compute_resistor_weights(0, 255, -1.0,
							 3, &resistances_rg[0], rweights, 0, 0,
							 3, &resistances_rg[0], gweights, 0, 0,
							 2, &resistances_b[0],  bweights, 0, 0);

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 1;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 1;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 1;
		INT32 r = combine_3_weights(rweights, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >> 3) & 1;
		bit1 = (DrvColPROM[i] >> 4) & 1;
		bit2 = (DrvColPROM[i] >> 5) & 1;
		INT32 g = combine_3_weights(gweights, bit0, bit1, bit2);

		bit0 = (DrvColPROM[i] >> 6) & 1;
		bit1 = (DrvColPROM[i] >> 7) & 1;
		INT32 b = combine_2_weights(bweights, bit0, bit1);

		tmp[i] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		DrvPalette[i] = tmp[0x10 | (DrvColPROM[0x20 + ((i & 0x1c0) | ((i & 3) << 4) | ((i >> 2) & 0x0f))] & 0x0f)];
		DrvTransTab[i] = 0x10 | (DrvColPROM[0x20 + ((i & 0x1c0) | ((i & 3) << 4) | ((i >> 2) & 0x0f))] & 0x0f);
	}

	for (INT32 i = 0x200; i < 0x300; i++)
	{
		DrvPalette[i] = tmp[DrvColPROM[0x20 + i] & 0x0f];
	}
}

static void do_background(INT32 y)
{
	if (y == Scanline_last) return;

	INT32 min_x = 12 * 8;
	INT32 max_x = 52 * 8;

	UINT16 *src0 = &BurnBitmapGetBitmap(1)[background_latches[1] * 256];
	UINT16 *src1 = &BurnBitmapGetBitmap(2)[background_latches[3] * 256];
	UINT16 *src2 = &BurnBitmapGetBitmap(3)[background_latches[5] * 256];
	UINT16 *src3 = &BurnBitmapGetBitmap(4)[background_latches[7] * 256];
	INT32 xoffs0 = background_latches[0];
	INT32 xoffs1 = background_latches[2];
	INT32 xoffs2 = background_latches[4];
	INT32 xoffs3 = background_latches[6];
	INT32 start0 = background_latches[8] & 0x0f;
	INT32 start1 = background_latches[9] & 0x0f;
	INT32 start2 = background_latches[10] & 0x0f;
	INT32 start3 = background_latches[11] & 0x0f;
	INT32 stop0 = background_latches[8] >> 4;
	INT32 stop1 = background_latches[9] >> 4;
	INT32 stop2 = background_latches[10] >> 4;
	INT32 stop3 = background_latches[11] >> 4;
	UINT8 *mixer = &DrvColPROM[0x320 + ((background_latches[12] << 4) & 0xf0)];
	UINT16 scanline[512];
	UINT32 pen_base = 0x200 + ((background_latches[12] >> 4) << 4);

	for (INT32 x = 32; x < min_x; x++)
	{
		if (!flipscreen)
		{
			if (!(++xoffs0 & 0x1f)) start0++, stop0++;
			if (!(++xoffs1 & 0x1f)) start1++, stop1++;
			if (!(++xoffs2 & 0x1f)) start2++, stop2++;
			if (!(++xoffs3 & 0x1f)) start3++, stop3++;
		} else {
			if (!(xoffs0-- & 0x1f)) start0++, stop0++;
			if (!(xoffs1-- & 0x1f)) start1++, stop1++;
			if (!(xoffs2-- & 0x1f)) start2++, stop2++;
			if (!(xoffs3-- & 0x1f)) start3++, stop3++;
		}
	}

	for (INT32 x = min_x; x < max_x; x++)
	{
		UINT16 combined = 0;
		UINT8 lookupval;

		if ((start0 ^ stop0) & 0x10) combined |= src0[xoffs0 & 0xff];
		if ((start1 ^ stop1) & 0x10) combined |= src1[xoffs1 & 0xff];
		if ((start2 ^ stop2) & 0x10) combined |= src2[xoffs2 & 0xff];
		if ((start3 ^ stop3) & 0x10) combined |= src3[xoffs3 & 0xff];

		lookupval = mixer[combined >> 8] & 3;

		scanline[x] = pen_base | (lookupval << 2) | ((combined >> (2 * lookupval)) & 3);

		if (!flipscreen)
		{
			if (!(++xoffs0 & 0x1f)) start0++, stop0++;
			if (!(++xoffs1 & 0x1f)) start1++, stop1++;
			if (!(++xoffs2 & 0x1f)) start2++, stop2++;
			if (!(++xoffs3 & 0x1f)) start3++, stop3++;
		} else {
			if (!(xoffs0-- & 0x1f)) start0++, stop0++;
			if (!(xoffs1-- & 0x1f)) start1++, stop1++;
			if (!(xoffs2-- & 0x1f)) start2++, stop2++;
			if (!(xoffs3-- & 0x1f)) start3++, stop3++;
		}
	}

	memcpy (Background + y * nScreenWidth, &scanline[min_x], nScreenWidth * sizeof(UINT16)); // 2 * for int16

	Scanline_last = y;
}

static void clear_background()
{
	Scanline_last = -1;

	memset(Background, 0, 512*256*sizeof(UINT16));
}

static void draw_background()
{
	for (INT32 y = 0; y < nScreenHeight; y++) {
		memcpy(pTransDraw + y * nScreenWidth, Background + (y * nScreenWidth), nScreenWidth * sizeof(UINT16));
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x80; i += 4)
	{
		INT32 flags = DrvSprRAM[i + 0];
		INT32 y = DrvSprRAM[i + 1] ^ 255;
		INT32 code = DrvSprRAM[i + 2];
		INT32 x = DrvSprRAM[i + 3] * 2 + 72 - ((flipscreen) ? -(12*8) : (12*8));

		INT32 xflip = flags & 0x80;
		INT32 yflip = flags & 0x40;
		INT32 doubled = flags & 0x10;
		INT32 wide = flags & 0x08;
		INT32 code2 = code;

		INT32 color = ((flags >> 1) & 0x03) | ((code >> 5) & 0x04) | (code & 0x08) | (sprite_palette * 16);

		INT32 px = (doubled) ? 32 : 16;

		if (flipscreen)
		{
			x = 64*8 - px - x;
			y = 32*8 - px - y;
			if (wide) y -= px;
			xflip = !xflip;
			yflip = !yflip;
		}

		if (wide)
		{
			if (yflip)
				code |= 0x10, code2 &= ~0x10;
			else
				code &= ~0x10, code2 |= 0x10;

			RenderTileTranstabOffset(pTransDraw, DrvGfxROM[(doubled) ? 3 : 1], code2, color << 2, 0x10, x, y + px - 16, xflip, yflip, px, px, DrvTransTab + 0x100, 0x100);
		}

		RenderTileTranstabOffset(pTransDraw, DrvGfxROM[(doubled) ? 3 : 1], code, color << 2, 0x10, x, y - 16, xflip, yflip, px, px, DrvTransTab + 0x100, 0x100);

		if (doubled) i += 4;
	}
}

static void draw_chars()
{
	for (INT32 sy = 0/8; sy < 240/8; sy++)
	{
		for (INT32 sx = 96/8; sx < 416/8; sx++)
		{
			INT32 x = (flipscreen) ? (63*8 - 8*sx) : 8*sx;
			INT32 y = (flipscreen) ? (31*8 - 8*sy) : 8*sy;
			INT32 offs = sy * 8 * 8 + sx;
			INT32 code = DrvVidRAM[offs] + 256 * char_bank;
			INT32 color = ((DrvVidRAM[offs] & 0xf0) >> 4) + char_palette * 16;

			DrawGfxMaskTile(0, 0, code, x - 96, y - 16, flipscreen, flipscreen, color, 0);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1)		draw_background();
	if (nSpriteEnable & 1)	draw_sprites();
	if (nBurnLayer & 2)		draw_chars();

	BurnTransferFlip(flipscreen, flipscreen); // unflip coctail for netplay/etc
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
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
		if ((DrvJoy4[0] & 1) && !previous_coin) {
			ZetNmi(0);
		}
		previous_coin = DrvJoy4[0] & 1;
	}

	clear_background();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { (3328000 * 100) / 6132, (3328000 * 100) / 6132 };
	INT32 nCyclesDone[2] = { nExtraCycles[0], nExtraCycles[1] };
	ZetIdle(0, nExtraCycles[0]); // _SYNCINTernally
	ZetIdle(1, nExtraCycles[1]);

	for (INT32 i = 0; i < nInterleave; i++) {
		if (i >= (16 - 1) && i <= 240) {
			vblank = 0;
			Scanline = i - 16;
		} else vblank = 1;

		ZetOpen(0);
		CPU_RUN_SYNCINT(0, Zet);
		ZetClose();

		ZetOpen(1);
		CPU_RUN_SYNCINT(1, Zet);
		ZetClose();
	}

	nExtraCycles[0] = ZetTotalCycles(0) - nCyclesTotal[0];
	nExtraCycles[1] = ZetTotalCycles(1) - nCyclesTotal[1];

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

		SCAN_VAR(sublatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(previous_coin);
		SCAN_VAR(portA);
		SCAN_VAR(char_palette);
		SCAN_VAR(char_bank);
		SCAN_VAR(sprite_palette);

		SCAN_VAR(nExtraCycles);
	}

	return 0;
}

// Exerion

static struct BurnRomInfo exerionRomDesc[] = {
	{ "exerion.07",		0x2000, 0x4c78d57d, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "exerion.08",		0x2000, 0xdcadc1df, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "exerion.09",		0x2000, 0x34cc4d14, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "exerion.05",		0x2000, 0x32f6bff5, 2 | BRF_PRG | BRF_ESS }, //  3 sub

	{ "exerion.06",		0x2000, 0x435a85a4, 1 | BRF_GRA },           //  4 fgchars

	{ "exerion.11",		0x2000, 0xf0633a09, 2 | BRF_GRA },           //  5 sprites
	{ "exerion.10",		0x2000, 0x80312de0, 2 | BRF_GRA },           //  6

	{ "exerion.03",		0x2000, 0x790595b8, 3 | BRF_GRA },           //  7 bgdata
	{ "exerion.04",		0x2000, 0xd7abd0b9, 3 | BRF_GRA },           //  8
	{ "exerion.01",		0x2000, 0x5bb755cb, 3 | BRF_GRA },           //  9
	{ "exerion.02",		0x2000, 0xa7ecbb70, 3 | BRF_GRA },           // 10

	{ "exerion.e1",		0x0020, 0x2befcc20, 4 | BRF_GRA },           // 11 proms
	{ "exerion.i8",		0x0100, 0x31db0e08, 4 | BRF_GRA },           // 12
	{ "exerion.h10",	0x0100, 0x63b4c555, 4 | BRF_GRA },           // 13
	{ "exerion.i3",		0x0100, 0xfe72ab79, 4 | BRF_GRA },           // 14
	{ "exerion.k4",		0x0100, 0xffc2ba43, 4 | BRF_GRA },           // 15
};

STD_ROM_PICK(exerion)
STD_ROM_FN(exerion)

struct BurnDriver BurnDrvExerion = {
	"exerion", NULL, NULL, NULL, "1983",
	"Exerion\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, exerionRomInfo, exerionRomName, NULL, NULL, NULL, NULL, ExerionInputInfo, ExerionDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 320, 3, 4
};

// Exerion (Taito)

static struct BurnRomInfo exeriontRomDesc[] = {
	{ "prom5.4p",		0x4000, 0x58b4dc1b, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "prom6.4s",		0x2000, 0xfca18c2d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "exerion.05",		0x2000, 0x32f6bff5, 2 | BRF_PRG | BRF_ESS }, //  2 sub

	{ "exerion.06",		0x2000, 0x435a85a4, 1 | BRF_GRA },           //  3 gfx1

	{ "exerion.11",		0x2000, 0xf0633a09, 2 | BRF_GRA },           //  4 gfx2
	{ "exerion.10",		0x2000, 0x80312de0, 2 | BRF_GRA },           //  5

	{ "exerion.03",		0x2000, 0x790595b8, 3 | BRF_GRA },           //  6 gfx3
	{ "exerion.04",		0x2000, 0xd7abd0b9, 3 | BRF_GRA },           //  7
	{ "exerion.01",		0x2000, 0x5bb755cb, 3 | BRF_GRA },           //  8
	{ "exerion.02",		0x2000, 0xa7ecbb70, 3 | BRF_GRA },           //  9

	{ "exerion.e1",		0x0020, 0x2befcc20, 4 | BRF_GRA },           // 10 proms
	{ "exerion.i8",		0x0100, 0x31db0e08, 4 | BRF_GRA },           // 11
	{ "exerion.h10",	0x0100, 0x63b4c555, 4 | BRF_GRA },           // 12
	{ "exerion.i3",		0x0100, 0xfe72ab79, 4 | BRF_GRA },           // 13
	{ "exerion.k4",		0x0100, 0xffc2ba43, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(exeriont)
STD_ROM_FN(exeriont)

struct BurnDriver BurnDrvExeriont = {
	"exeriont", "exerion", NULL, NULL, "1983",
	"Exerion (Taito)\0", NULL, "Jaleco (Taito America license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, exeriontRomInfo, exeriontRomName, NULL, NULL, NULL, NULL, ExerionInputInfo, ExerionDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 320, 3, 4
};


// Exerion (bootleg)

static struct BurnRomInfo exerionbRomDesc[] = {
	{ "eb5.bin",		0x4000, 0xda175855, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "eb6.bin",		0x2000, 0x0dbe2eff, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "exerion.05",		0x2000, 0x32f6bff5, 2 | BRF_PRG | BRF_ESS }, //  2 sub

	{ "exerion.06",		0x2000, 0x435a85a4, 1 | BRF_GRA },           //  3 gfx1

	{ "exerion.11",		0x2000, 0xf0633a09, 2 | BRF_GRA },           //  4 gfx2
	{ "exerion.10",		0x2000, 0x80312de0, 2 | BRF_GRA },           //  5

	{ "exerion.03",		0x2000, 0x790595b8, 3 | BRF_GRA },           //  6 gfx3
	{ "exerion.04",		0x2000, 0xd7abd0b9, 3 | BRF_GRA },           //  7
	{ "exerion.01",		0x2000, 0x5bb755cb, 3 | BRF_GRA },           //  8
	{ "exerion.02",		0x2000, 0xa7ecbb70, 3 | BRF_GRA },           //  9

	{ "exerion.e1",		0x0020, 0x2befcc20, 4 | BRF_GRA },           // 10 proms
	{ "exerion.i8",		0x0100, 0x31db0e08, 4 | BRF_GRA },           // 11
	{ "exerion.h10",	0x0100, 0x63b4c555, 4 | BRF_GRA },           // 12
	{ "exerion.i3",		0x0100, 0xfe72ab79, 4 | BRF_GRA },           // 13
	{ "exerion.k4",		0x0100, 0xffc2ba43, 4 | BRF_GRA },           // 14
};

STD_ROM_PICK(exerionb)
STD_ROM_FN(exerionb)

static INT32 ExerionbInit()
{
	INT32 rc = DrvInit();

	if (!rc) {
		for (INT32 i = 0; i < 0x6000; i++) {
			DrvZ80ROM[0][i] = (DrvZ80ROM[0][i] & 0xf9) | ((DrvZ80ROM[0][i] & 2) << 1) | ((DrvZ80ROM[0][i] & 4) >> 1);
		}
	}

	return rc;
}

struct BurnDriver BurnDrvExerionb = {
	"exerionb", "exerion", NULL, NULL, "1983",
	"Exerion (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, exerionbRomInfo, exerionbRomName, NULL, NULL, NULL, NULL, ExerionInputInfo, ExerionDIPInfo,
	ExerionbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 320, 3, 4
};


// Irion

static struct BurnRomInfo irionRomDesc[] = {
	{ "2.bin",		0x2000, 0xbf55324e, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "3.bin",		0x2000, 0x0625bb49, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "4.bin",		0x2000, 0x918a9b1d, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "1.bin",		0x2000, 0x56cd5ebf, 1 | BRF_GRA },           //  3 gfx1

	{ "5.bin",		0x2000, 0x80312de0, 2 | BRF_GRA },           //  4 gfx2
	{ "6.bin",		0x2000, 0xf0633a09, 2 | BRF_GRA },           //  5

	{ "exerion.e1",		0x0020, 0x2befcc20, 4 | BRF_GRA },           //  6 proms
	{ "exerion.i8",		0x0100, 0x31db0e08, 4 | BRF_GRA },           //  7
	{ "exerion.h10",	0x0100, 0x63b4c555, 4 | BRF_GRA },           //  8
	{ "exerion.i3",		0x0100, 0xfe72ab79, 4 | BRF_GRA },           //  9
	{ "exerion.k4",		0x0100, 0xffc2ba43, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(irion)
STD_ROM_FN(irion)

struct BurnDriver BurnDrvIrion = {
	"irion", "exerion", NULL, NULL, "1983",
	"Irion\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, irionRomInfo, irionRomName, NULL, NULL, NULL, NULL, ExerionInputInfo, ExerionDIPInfo,
	ExerionbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	224, 320, 3, 4
};
