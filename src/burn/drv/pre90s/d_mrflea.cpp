// FB Alpha Amazing Adventures of Mr. F. Lea driver module
// Based on MAME driver by Phil Stroffolino

#include "tiles_generic.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT16 *DrvVidRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 mrflea_io;
static INT32 mrflea_main;
static INT32 mrflea_status;
static UINT8 mrflea_select[4];
static UINT8 gfx_bank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"Start 1",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"Start 2",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},

	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 4, 	"p1 right"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5, 	"p1 left"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 down"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip 1",			BIT_DIPSWITCH,	DrvDips+0,		"dip"		},
	{"Dip 2",			BIT_DIPSWITCH,	DrvDips+1,		"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x09, 0xff, 0xff, 0xff, NULL                     },
	{0x0a, 0xff, 0xff, 0xff, NULL                     },

	{0   , 0xfe, 0   , 4   , "Bonus"                  },
	{0x09, 0x01, 0x03, 0x03, "A"       		  },
	{0x09, 0x01, 0x03, 0x02, "B"       		  },
	{0x09, 0x01, 0x03, 0x01, "C"       		  },
	{0x09, 0x01, 0x03, 0x00, "D"       		  },

	{0   , 0xfe, 0   , 4   , "Coinage" 	          },
	{0x0a, 0x01, 0x03, 0x02, "2C 1C"       		  },
	{0x0a, 0x01, 0x03, 0x03, "1C 1C"       		  },
	{0x0a, 0x01, 0x03, 0x00, "2C 3C"       		  },
	{0x0a, 0x01, 0x03, 0x01, "1C 2C"       		  },

	{0   , 0xfe, 0   , 4   , "Lives"                  },
	{0x0a, 0x01, 0x0c, 0x0c, "3"       		  },
	{0x0a, 0x01, 0x0c, 0x08, "4"       		  },
	{0x0a, 0x01, 0x0c, 0x04, "5"       		  },
	{0x0a, 0x01, 0x0c, 0x00, "7"       		  },

	{0   , 0xfe, 0   , 4   , "Difficulty" 	          },
	{0x0a, 0x01, 0x30, 0x30, "Easy"       		  },
	{0x0a, 0x01, 0x30, 0x20, "Medium"       	  },
	{0x0a, 0x01, 0x30, 0x10, "Hard"       		  },
	{0x0a, 0x01, 0x30, 0x00, "Hardest"     		  },
};

STDDIPINFO(Drv)

static void palette_write(INT32 offs)
{
	UINT8 r = (DrvPalRAM[offs | 1] & 0x0f) | (DrvPalRAM[offs | 1] << 4);
	UINT8 g = (DrvPalRAM[offs] & 0xf0) | (DrvPalRAM[offs] >> 4);
	UINT8 b = (DrvPalRAM[offs] & 0x0f) | (DrvPalRAM[offs] << 4);

	DrvPalette[offs/2] = BurnHighCol(r,g,b,0);
}

static void __fastcall mrflea_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xe000)
	{
		DrvVidRAM[address & 0x3ff] = data | ((address & 0x400) >> 2);
		return;
	}

	if ((address & 0xffc0) == 0xe800)
	{
		INT32 offs = address &= 0x3f;
		DrvPalRAM[offs] = data;
		palette_write(offs & ~1);
		return;
	}

	if ((address & 0xff00) == 0xec00)
	{
		address &= 0xff;

		if (address & 2) { // tile number
			DrvSprRAM[address | 1] = address & 1;
			address &= 0xfe;
		}

		DrvSprRAM[address] = data;
		return;
	}
}

static void __fastcall mrflea_out_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x40:
		{
			mrflea_status |= 0x08;
			mrflea_io = data;

			ZetClose();
			ZetOpen(1);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
			ZetOpen(0);
		}
		return;

		case 0x60:
			gfx_bank = data;
		return;
	}
}

static UINT8 __fastcall mrflea_in_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x41:
			mrflea_status &= ~0x01;
			return mrflea_main;

		case 0x42:
			return mrflea_status ^ 0x08;
	}

	return 0;
}

static void __fastcall mrflea_cpu1_out_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x21:
			mrflea_status |= 0x01;
			mrflea_main = data;
		return;

		case 0x40:
			AY8910Write(0, 0, mrflea_select[0]);
			AY8910Write(0, 1, data);
		return;

		case 0x44:
			AY8910Write(1, 0, mrflea_select[2]);
			AY8910Write(1, 1, data);
		return;

		case 0x46:
			AY8910Write(2, 0, mrflea_select[3]);
			AY8910Write(2, 1, data);
		return;

		case 0x41:
		case 0x43:
		case 0x45:
		case 0x47:
			mrflea_select[(port >> 1) & 3] = data;
		return;
	}
}

static UINT8 __fastcall mrflea_cpu1_in_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x10:
			return (mrflea_status & 0x08) ? 0x00 : 0x01;

		case 0x20:
			mrflea_status &= ~0x08;
			return mrflea_io;

		case 0x22:
			return mrflea_status ^ 0x01;

		case 0x40:
			if ((mrflea_select[0] & 0x0e) == 0x0e) {
				return DrvInputs[~mrflea_select[0] & 1];
			}
			return 0;

		case 0x44:
			if ((mrflea_select[2] & 0x0e) == 0x0e) return 0xff;
			return 0;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);
	AY8910Reset(2);

	memset (mrflea_select, 0, 4);

	mrflea_io = 0;
	mrflea_main = 0;
	mrflea_status = 0;
	gfx_bank = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x00c000;
	DrvZ80ROM1		= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvPalette		= (UINT32*)Next; Next += 0x0020 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x000200;
	DrvVidRAM		= (UINT16*)Next; Next += 0x000400 * sizeof(UINT16);
	DrvPalRAM		= Next; Next += 0x000100;
	DrvSprRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0, 0x4000*8*1, 0x4000*8*2, 0x4000*8*3 };
	INT32 XOffs0[16] = { STEP16(0,1) };
	INT32 YOffs0[16] = { STEP16(0,16) };
	INT32 Plane1[4]  = { STEP4(0,1) };
	INT32 XOffs1[8]  = { STEP8(0,4) };
	INT32 YOffs1[8]  = { STEP8(0,32) };
	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x10000);

	GfxDecode(0x0200, 4, 16, 16, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x10000);

	GfxDecode(0x0800, 4,  8,  8, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x8000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0xa000,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x2000,  7, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x3000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x4000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x6000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x8000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0xa000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0xc000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0xe000, 16, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x6000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x8000, 21, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0xa000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0xc000, 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0xe000, 24, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xc000, 0xcfff, MAP_RAM);
	ZetSetInHandler(mrflea_in_port);
	ZetSetOutHandler(mrflea_out_port);
	ZetSetWriteHandler(mrflea_write);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x8000, 0x80ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1 + 0x100,0x9000, 0x90ff, MAP_RAM);
	ZetSetInHandler(mrflea_cpu1_in_port);
	ZetSetOutHandler(mrflea_cpu1_out_port);
	ZetClose();

	AY8910Init(0, 2000000, 0);
	AY8910Init(1, 2000000, 1);
	AY8910Init(2, 2000000, 1);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(2, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 6000000);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);
	AY8910Exit(2);

	GenericTilesExit();

	BurnFree(AllMem);

	return 0;
}

static void draw_layer()
{
	INT32 base = ((gfx_bank & 0x04) << 8) | ((gfx_bank & 0x10) << 5);

	for (INT32 i = 0; i < 0x400 - 32; i++)
	{
		INT32 sy = (i >> 2) & 0xf8;
		INT32 sx = (i << 3) & 0xf8;

		Render8x8Tile(pTransDraw, base + DrvVidRAM[i], sx, sy, 0, 4, 0, DrvGfxROM1);
	}
}

static void draw_sprites()
{
	GenericTilesSetClip(16, nScreenWidth-24, 0, nScreenHeight);

	for (INT32 i = 0; i < 0x100; i+=4)
	{
		INT32 sx = DrvSprRAM[i + 1] - 3;
		INT32 sy = DrvSprRAM[i + 0] - 13;

		INT32 code = DrvSprRAM[i + 2] + ((DrvSprRAM[i + 3] & 1) << 8);

		Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, 0, 4, 0, 0x10, DrvGfxROM0);
		Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy + 256, 0, 4, 0, 0x10, DrvGfxROM0);
	}

	GenericTilesClearClip();
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x100; i+=2) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) draw_layer();
	if (nBurnLayer & 2) draw_sprites();

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
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 200;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 6000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if ((i == (nInterleave/2) && (mrflea_status&0x08)) || i == (nInterleave-1))
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
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
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(mrflea_io);
		SCAN_VAR(mrflea_main);
		SCAN_VAR(mrflea_status);
		SCAN_VAR(gfx_bank);
		SCAN_VAR(mrflea_select);
	}

	return 0;
}


// The Amazing Adventures of Mr. F. Lea

static struct BurnRomInfo mrfleaRomDesc[] = {
	{ "cpu_d1",	0x2000, 0xd286217c, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 0 Code
	{ "cpu_d3",	0x2000, 0x95cf94bc, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "cpu_d5",	0x2000, 0x466ca77e, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "cpu_b1",	0x2000, 0x721477d6, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "cpu_b3",	0x2000, 0xf55b01e4, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "cpu_b5",	0x2000, 0x79f560aa, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "io_a11",	0x1000, 0x7a20c3ee, 2 | BRF_ESS | BRF_PRG }, //  6 Z80 1 Code
	{ "io_c11",	0x1000, 0x8d26e0c8, 2 | BRF_ESS | BRF_PRG }, //  7
	{ "io_d11",	0x1000, 0xabd9afc0, 2 | BRF_ESS | BRF_PRG }, //  8

	{ "vd_l10",	0x2000, 0x48b2adf9, 3 | BRF_GRA },	     //  9 Sprites
	{ "vd_l11",	0x2000, 0x2ff168c0, 3 | BRF_GRA },	     // 10
	{ "vd_l6",	0x2000, 0x100158ca, 3 | BRF_GRA },	     // 11
	{ "vd_l7",	0x2000, 0x34501577, 3 | BRF_GRA },	     // 12
	{ "vd_j10",	0x2000, 0x3f29b8c3, 3 | BRF_GRA },	     // 13
	{ "vd_j11",	0x2000, 0x39380bea, 3 | BRF_GRA },	     // 14
	{ "vd_j6",	0x2000, 0x2b4b110e, 3 | BRF_GRA },	     // 15
	{ "vd_j7",	0x2000, 0x3a3c8b1e, 3 | BRF_GRA },	     // 16

	{ "vd_k1",	0x2000, 0x7540e3a7, 4 | BRF_GRA },	     // 17 Characters
	{ "vd_k2",	0x2000, 0x6c688219, 4 | BRF_GRA },	     // 18
	{ "vd_k3",	0x2000, 0x15e96f3c, 4 | BRF_GRA },	     // 19
	{ "vd_k4",	0x2000, 0xfe5100df, 4 | BRF_GRA },	     // 20
	{ "vd_l1",	0x2000, 0xd1e3d056, 4 | BRF_GRA },	     // 21
	{ "vd_l2",	0x2000, 0x4d7fb925, 4 | BRF_GRA },	     // 22
	{ "vd_l3",	0x2000, 0x6d81588a, 4 | BRF_GRA },	     // 23
	{ "vd_l4",	0x2000, 0x423735a5, 4 | BRF_GRA },	     // 24
};

STD_ROM_PICK(mrflea)
STD_ROM_FN(mrflea)

struct BurnDriver BurnDrvmrflea = {
	"mrflea", NULL, NULL, NULL, "1982",
	"The Amazing Adventures of Mr. F. Lea\0", NULL, "Pacific Novelty", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 1, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, mrfleaRomInfo, mrfleaRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	248, 256, 3, 4
};
