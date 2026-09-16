// FinalBurn Neo Dark Seal driver module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "h6280_intf.h"
#include "bitswap.h"
#include "deco16ic.h"
#include "burn_ym2203.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "timer.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvHucROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *Drv68KRAM;
static UINT8 *DrvHucRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc = 0;

static INT32 vblank = 0;
static INT32 nCyclesExtra;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDip[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static struct BurnInputInfo DarksealInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDip + 0,		"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDip + 1,		"dip"		},
};

STDINPUTINFO(Darkseal)

static struct BurnDIPInfo DarksealDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x7f, NULL					},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0x07, 0x00, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x07, 0x01, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x07, 0x07, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x07, 0x02, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x00, 0x01, 0x38, 0x00, "3 Coins 1 Credit"		},
	{0x00, 0x01, 0x38, 0x08, "2 Coins 1 Credit"		},
	{0x00, 0x01, 0x38, 0x38, "1 Coin  1 Credit"		},
	{0x00, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x38, 0x18, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x38, 0x10, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x01, 0x01, 0x03, 0x00, "1"					},
	{0x01, 0x01, 0x03, 0x01, "2"					},
	{0x01, 0x01, 0x03, 0x03, "3"					},
	{0x01, 0x01, 0x03, 0x02, "4"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x0c, 0x08, "Easy"					},
	{0x01, 0x01, 0x0c, 0x0c, "Normal"				},
	{0x01, 0x01, 0x0c, 0x04, "Hard"					},
	{0x01, 0x01, 0x0c, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Energy"				},
	{0x01, 0x01, 0x30, 0x00, "2"					},
	{0x01, 0x01, 0x30, 0x10, "2.5"					},
	{0x01, 0x01, 0x30, 0x30, "3"					},
	{0x01, 0x01, 0x30, 0x20, "4"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x01, 0x01, 0x40, 0x00, "No"					},
	{0x01, 0x01, 0x40, 0x40, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x80, "Off"					},
	{0x01, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Darkseal)

static inline void palette_write(INT32 offset)
{
	UINT16 *data = (UINT16*)(DrvPalRAM + offset);

	UINT8 r = (BURN_ENDIAN_SWAP_INT16(data[0x0000/2]) >> 0) & 0xff;
	UINT8 g = (BURN_ENDIAN_SWAP_INT16(data[0x0000/2]) >> 8) & 0xff;
	UINT8 b = (BURN_ENDIAN_SWAP_INT16(data[0x1000/2]) >> 0) & 0xff;

	DrvPalette[offset/2] = BurnHighCol(r, g, b, 0);
}

static void __fastcall darkseal_write_byte(UINT32 address, UINT8 data)
{
	deco16_write_control_byte(1, address, 0x240000, data)
	deco16_write_control_byte(0, address, 0x2a0000, data)

	switch (address & ~1)
	{
		case 0x180006:
			memcpy (DrvSprBuf, DrvSprRAM, 0x800);
		return;

		case 0x180008:
			deco16_soundlatch = data & 0xff;
			h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x18000a:
			SekSetIRQLine(6, CPU_IRQSTATUS_NONE);
		return;
	}


	bprintf(0, _T("wb %x  %x\n"), address, data);
}

static void __fastcall darkseal_write_word(UINT32 address, UINT16 data)
{
	deco16_write_control_word(1, address, 0x240000, data)
	deco16_write_control_word(0, address, 0x2a0000, data)

	switch (address)
	{
		case 0x180006:
			memcpy (DrvSprBuf, DrvSprRAM, 0x800);
		return;

		case 0x180008:
			deco16_soundlatch = data & 0xff;
			h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x18000a:
			SekSetIRQLine(6, CPU_IRQSTATUS_NONE);
		return;
	}

	bprintf(0, _T("ww %x  %x\n"), address, data);
}

static UINT8 __fastcall darkseal_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x180000:
			return DrvDip[1];

		case 0x180001:
			return DrvDip[0];

		case 0x180002:
			return DrvInputs[0] >> 8;

		case 0x180003:
			return DrvInputs[0];

		case 0x180004:
			return 0xff;

		case 0x180005:
			return (DrvInputs[1] & ~8) | vblank;
	}

	bprintf(0, _T("rb %x\n"), address);

	return 0;
}

static UINT16 __fastcall darkseal_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x180000:
			return (DrvDip[0] | (DrvDip[1]<<8));

		case 0x180002:
			return DrvInputs[0];

		case 0x180004:
			return (DrvInputs[1] & ~8) | vblank;
	}

	bprintf(0, _T("rw %x\n"), address);

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	deco16Reset();
	deco16SoundReset();

	nCyclesExtra = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x080000;

	DrvHucROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x040000;
	DrvGfxROM1	= Next; Next += 0x100000;
	DrvGfxROM2	= Next; Next += 0x100000;
	DrvGfxROM3	= Next; Next += 0x200000;

	MSM6295ROM	= Next; Next += 0x140000;

	DrvPalette	= (UINT32*)Next; Next += 0x00801 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x004000;
	DrvHucRAM	= Next; Next += 0x002000;
	DrvSprRAM	= Next; Next += 0x000800;
	DrvSprBuf	= Next; Next += 0x000800;
	DrvPalRAM	= Next; Next += 0x002000;

	RamEnd		= Next;
	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0x00000*8, 0x10000*8, 0x8000*8, 0x18000*8 };
	INT32 Plane1[4]  = { 8, 0, 0x40000*8+8, 0x40000*8 };
	INT32 Plane2[4]  = { 8, 0, 0x80000*8+8, 0x80000*8 };
	INT32 XOffs0[8]  = { STEP8(0,1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };
	INT32 XOffs1[16] = { STEP8(256,1), STEP8(0,1) };
	INT32 YOffs1[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x020000);

	GfxDecode(0x1000, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x080000);

	GfxDecode(0x1000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x080000);

	GfxDecode(0x1000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane2, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM3);

	BurnFree (tmp);

	return 0;
}

static void DrvPrgDecode()
{
	for (INT32 i = 0; i < 0x80000; i++)
		Drv68KROM[i] = BITSWAP08(Drv68KROM[i], 7, 1, 5, 4, 3, 2, 6, 0);
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(58.00);

	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x00001,	0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x00000,	1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x40001,	2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x40000,	3, 2)) return 1;

		if (BurnLoadRom(DrvHucROM + 0x00000,	4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,	5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,	6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,	7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,	8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,	9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x80000,  10, 1)) return 1;

		if (BurnLoadRom(MSM6295ROM + 0x000000, 11, 1)) return 1;
		if (BurnLoadRom(MSM6295ROM + 0x100000, 12, 1)) return 1;

		DrvPrgDecode();
		DrvGfxDecode();
	}

	deco16Init(0, 0, 1|4|8);
	deco16_set_global_offsets(0, 8);
	deco16_set_graphics(DrvGfxROM0, 0x40000, DrvGfxROM1, 0x100000, DrvGfxROM2, 0x100000);
	deco16_set_color_base(0, 0x300);
	deco16_set_color_base(1, 0x000);
	deco16_set_color_base(2, 0x000);
	deco16_set_color_base(3, 0x400);
	deco16_set_transparency_mask(0, 0xf);
	deco16_set_transparency_mask(1, 0xf);
	deco16_set_transparency_mask(2, 0xf);
	deco16_set_transparency_mask(3, 0xf);
	deco16_set_color_mask(0, 0xf);
	deco16_set_color_mask(1, 0xf);
	deco16_set_color_mask(2, 0xf);
	deco16_set_color_mask(3, 0xf);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,						0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,						0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,						0x120000, 0x1207ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,						0x140000, 0x141fff, MAP_RAM); // split
	SekMapMemory(deco16_pf_ram[2],				0x200000, 0x201fff, MAP_RAM);
	SekMapMemory(deco16_pf_ram[3],				0x202000, 0x203fff, MAP_RAM);
	SekMapMemory(deco16_pf_rowscroll[0],		0x220000, 0x220fff, MAP_RAM);
	SekMapMemory(deco16_pf_rowscroll[2],		0x222000, 0x222fff, MAP_RAM);
	SekMapMemory(deco16_pf_ram[0],				0x260000, 0x261fff, MAP_RAM);
	SekMapMemory(deco16_pf_ram[1],				0x262000, 0x263fff, MAP_RAM);
	SekSetWriteWordHandler(0,					darkseal_write_word);
	SekSetWriteByteHandler(0,					darkseal_write_byte);
	SekSetReadWordHandler(0,					darkseal_read_word);
	SekSetReadByteHandler(0,					darkseal_read_byte);
	SekClose();

	deco16SoundInit(DrvHucROM, DrvHucRAM, 8055000, 1, NULL, 0.55, 1006875, 1.00, 2013750, 0.60);
	BurnYM2203SetAllRoutes(0, 0.45, BURN_SND_ROUTE_BOTH);

	deco16_music_tempofix = 1;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	deco16Exit();
	deco16SoundExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites()
{
	UINT16 *SprRAM = (UINT16*)DrvSprBuf;

	for (INT32 offs = 0; offs < 0x400; offs += 4)
	{
		INT32 sprite = BURN_ENDIAN_SWAP_INT16(SprRAM[offs+1]) & 0x1fff;
		if (!sprite) continue;

		INT32 y = BURN_ENDIAN_SWAP_INT16(SprRAM[offs]);
		INT32 x = BURN_ENDIAN_SWAP_INT16(SprRAM[offs+2]);

		INT32 flash = ((y >> 12) & 1) & GetCurrentFrame();
		if (flash) continue;

		INT32 color = ((x >> 9) & 0x1f) + 0x10;

		INT32 fx = y & 0x2000;
		INT32 fy = y & 0x4000;
		INT32 multi = (1 << ((y & 0x0600) >> 9)) - 1;

		x &= 0x01ff;
		y &= 0x01ff;
		if (x > 255) x -= 512;
		if (y > 255) y -= 512;
		x =  240 - x;
		y = (240 - y) - 8;

		if (x > 256) continue;

		sprite &= ~multi;

		INT32 inc = -1;

		if (!fy) {
			sprite += multi;
			inc = 1;
		}

		while (multi >= 0)
		{
			Draw16x16MaskTile(pTransDraw, sprite - multi * inc, x, y+-16*multi, fx, fy, color, 4, 0, 0, DrvGfxROM3);

			multi--;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x1000; i+=2) {
			palette_write(i);
		}
		DrvPalette[0x800] = 0; // black
		DrvRecalc = 1;
	}

	memcpy(deco16_pf_rowscroll[1], deco16_pf_rowscroll[0], 0x2000);
	memcpy(deco16_pf_rowscroll[3], deco16_pf_rowscroll[2], 0x2000);

	deco16_pf12_update();
	deco16_pf34_update();

	BurnTransferClear(0x800);

	if (nBurnLayer & 1) deco16_draw_layer(2, pTransDraw, 0);
	if (nBurnLayer & 2) deco16_draw_layer(3, pTransDraw, 0);
	if (nBurnLayer & 4) deco16_draw_layer(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 8) deco16_draw_layer(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 12000000 / 58, 8055000 / 58 };
	INT32 nCyclesDone[2] = { nCyclesExtra, 0 };

	h6280NewFrame();

	SekOpen(0);
	h6280Open(0);

	vblank = 8;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		int scanline = (i + 248) % 256;
		if (scanline ==   0) vblank = 0;
		if (scanline == 248)
		{
			vblank = 8;

			SekSetIRQLine(6, CPU_IRQSTATUS_ACK);
		}

		CPU_RUN(0, Sek);
		CPU_RUN_TIMER(1);
	}

	h6280Close();
	SekClose();

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		deco16SoundUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin != NULL) {
		*pnMin = 0x029722;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(AllRam, RamEnd-AllRam, "All Ram");
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		deco16SoundScan(nAction, pnMin);

		deco16Scan();

		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// Dark Seal (World revision 3)

static struct BurnRomInfo darksealRomDesc[] = {
	{ "ga_04-3.j12",	0x20000, 0xbafad556, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ga_01-3.h14",	0x20000, 0xf409050e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ga_00.h12",		0x20000, 0xfbf3ac63, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ga_05.j14",		0x20000, 0xd5e3ae3f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fz_06-1.j15",	0x10000, 0xc4828a6d, 2 | BRF_PRG | BRF_ESS }, //  4 H6280 Code

	{ "fz_02.j1",		0x10000, 0x3c9c3012, 3 | BRF_GRA },           //  5 Text Tiles
	{ "fz_03.j2",		0x10000, 0x264b90ed, 3 | BRF_GRA },           //  6

	{ "mac-03.h3",		0x80000, 0x9996f3dc, 4 | BRF_GRA },           //  7 Foreground Tiles

	{ "mac-02.e20",		0x80000, 0x49504e89, 5 | BRF_GRA },           //  8 Background Tiles

	{ "mac-00.b1",		0x80000, 0x52acf1d6, 6 | BRF_GRA },           //  9 Sprite Tiles
	{ "mac-01.b3",		0x80000, 0xb28f7584, 6 | BRF_GRA },           // 10

	{ "fz_08.l17",		0x20000, 0xc9bf68e1, 7 | BRF_SND },           // 11 Oki6295 #0 Samples

	{ "fz_07.k14",		0x20000, 0x588dd3cb, 8 | BRF_SND },           // 12 Oki6295 #1 Samples
};

STD_ROM_PICK(darkseal)
STD_ROM_FN(darkseal)

struct BurnDriver BurnDrvDarkseal = {
	"darkseal", NULL, NULL, NULL, "1990",
	"Dark Seal (World revision 3)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAZE | GBF_SCRFIGHT, 0,
	NULL, darksealRomInfo, darksealRomName, NULL, NULL, NULL, NULL, DarksealInputInfo, DarksealDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};


// Dark Seal (World revision 1)

static struct BurnRomInfo darksea1RomDesc[] = {
	{ "fz_04-4.j12",	0x20000, 0xa1a985a9, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "fz_01-1.h14",	0x20000, 0x98bd2940, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fz_00-2.h12",	0x20000, 0xfbf3ac63, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fz_05-2.j14",	0x20000, 0xd5e3ae3f, 1 | BRF_PRG | BRF_ESS }, //  3
	
	{ "fz_06-1.j15",	0x10000, 0xc4828a6d, 2 | BRF_PRG | BRF_ESS }, //  4 H6280 Code

	{ "fz_02-1.j1",		0x10000, 0x3c9c3012, 3 | BRF_GRA },           //  5 Text Tiles
	{ "fz_03-1.j2",		0x10000, 0x264b90ed, 3 | BRF_GRA },           //  6

	{ "mac-03.h3",		0x80000, 0x9996f3dc, 4 | BRF_GRA },           //  7 Foreground Tiles

	{ "mac-02.e20",		0x80000, 0x49504e89, 5 | BRF_GRA },           //  8 Background Tiles

	{ "mac-00.b1",		0x80000, 0x52acf1d6, 6 | BRF_GRA },           //  9 Sprite Tiles
	{ "mac-01.b3",		0x80000, 0xb28f7584, 6 | BRF_GRA },           // 10

	{ "fz_08-1.k17",	0x20000, 0xc9bf68e1, 7 | BRF_SND },           // 11 Oki6295 #0 Samples

	{ "fz_07-.k14",		0x20000, 0x588dd3cb, 8 | BRF_SND },           // 12 Oki6295 #1 Samples 
};

STD_ROM_PICK(darksea1)
STD_ROM_FN(darksea1)

struct BurnDriver BurnDrvDarksea1 = {
	"darkseal1", "darkseal", NULL, NULL, "1990",
	"Dark Seal (World revision 1)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAZE | GBF_SCRFIGHT, 0,
	NULL, darksea1RomInfo, darksea1RomName, NULL, NULL, NULL, NULL, DarksealInputInfo, DarksealDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};


// Dark Seal (Japan revision 4)

static struct BurnRomInfo darkseajRomDesc[] = {
	{ "fz_04-4.j12",	0x20000, 0x817faa2c, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "fz_01-4.h14",	0x20000, 0x373caeee, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fz_00-2.h12",	0x20000, 0x1ab99aa7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fz_05-2.j14",	0x20000, 0x3374ef8c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fz_06-1.j15",	0x10000, 0xc4828a6d, 2 | BRF_PRG | BRF_ESS }, //  4 H6280 Code

	{ "fz_02.j1",		0x10000, 0x3c9c3012, 3 | BRF_GRA },           //  5 Text Tiles
	{ "fz_03.j2",		0x10000, 0x264b90ed, 3 | BRF_GRA },           //  6

	{ "mac-03.h3",		0x80000, 0x9996f3dc, 4 | BRF_GRA },           //  7 Foreground Tiles

	{ "mac-02.e20",		0x80000, 0x49504e89, 5 | BRF_GRA },           //  8 Background Tiles

	{ "mac-00.b1",		0x80000, 0x52acf1d6, 6 | BRF_GRA },           //  9 Sprite Tiles
	{ "mac-01.b3",		0x80000, 0xb28f7584, 6 | BRF_GRA },           // 10

	{ "fz_08.l17",		0x20000, 0xc9bf68e1, 7 | BRF_SND },           // 11 Oki6295 #0 Samples

	{ "fz_07.k14",		0x20000, 0x588dd3cb, 8 | BRF_SND },           // 12 Oki6295 #1 Samples
};

STD_ROM_PICK(darkseaj)
STD_ROM_FN(darkseaj)

struct BurnDriver BurnDrvDarkseaj = {
	"darksealj", "darkseal", NULL, NULL, "1990",
	"Dark Seal (Japan revision 4)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAZE | GBF_SCRFIGHT, 0,
	NULL, darkseajRomInfo, darkseajRomName, NULL, NULL, NULL, NULL, DarksealInputInfo, DarksealDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};


// Gate of Doom (US revision 4)

static struct BurnRomInfo gatedoomRomDesc[] = {
	{ "gb_04-4.j12",	0x20000, 0x8e3a0bfd, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "gb_01-4.h14",	0x20000, 0x8d0fd383, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gb_00.h12",		0x20000, 0xa88c16a1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gb_05.j14",		0x20000, 0x252d7e14, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fz_06-1.j15",	0x10000, 0xc4828a6d, 2 | BRF_PRG | BRF_ESS }, //  4 H6280 Code

	{ "fz_02.j1",		0x10000, 0x3c9c3012, 3 | BRF_GRA },           //  5 Text Tiles
	{ "fz_03.j2",		0x10000, 0x264b90ed, 3 | BRF_GRA },           //  6

	{ "mac-03.h3",		0x80000, 0x9996f3dc, 4 | BRF_GRA },           //  7 Foreground Tiles

	{ "mac-02.e20",		0x80000, 0x49504e89, 5 | BRF_GRA },           //  8 Background Tiles

	{ "mac-00.b1",		0x80000, 0x52acf1d6, 6 | BRF_GRA },           //  9 Sprite Tiles
	{ "mac-01.b3",		0x80000, 0xb28f7584, 6 | BRF_GRA },           // 10

	{ "fz_08.l17",		0x20000, 0xc9bf68e1, 7 | BRF_SND },           // 11 Oki6295 #0 Samples

	{ "fz_07.k14",		0x20000, 0x588dd3cb, 8 | BRF_SND },           // 12 Oki6295 #1 Samples
};

STD_ROM_PICK(gatedoom)
STD_ROM_FN(gatedoom)

struct BurnDriver BurnDrvGatedoom = {
	"gatedoom", "darkseal", NULL, NULL, "1990",
	"Gate of Doom (US revision 4)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAZE | GBF_SCRFIGHT, 0,
	NULL, gatedoomRomInfo, gatedoomRomName, NULL, NULL, NULL, NULL, DarksealInputInfo, DarksealDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};


// Gate of Doom (US revision 1)

static struct BurnRomInfo gatedom1RomDesc[] = {
	{ "gb_04.j12",		0x20000, 0x4c3bbd2b, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "gb_01.h14",		0x20000, 0x59e367f4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gb_00.h12",		0x20000, 0xa88c16a1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gb_05.j14",		0x20000, 0x252d7e14, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fz_06-1.j15",	0x10000, 0xc4828a6d, 2 | BRF_PRG | BRF_ESS }, //  4 H6280 Code

	{ "fz_02.j1",		0x10000, 0x3c9c3012, 3 | BRF_GRA },           //  5 Text Tiles
	{ "fz_03.j2",		0x10000, 0x264b90ed, 3 | BRF_GRA },           //  6

	{ "mac-03.h3",		0x80000, 0x9996f3dc, 4 | BRF_GRA },           //  7 Foreground Tiles

	{ "mac-02.e20",		0x80000, 0x49504e89, 5 | BRF_GRA },           //  8 Background Tiles

	{ "mac-00.b1",		0x80000, 0x52acf1d6, 6 | BRF_GRA },           //  9 Sprite Tiles
	{ "mac-01.b3",		0x80000, 0xb28f7584, 6 | BRF_GRA },           // 10

	{ "fz_08.l17",		0x20000, 0xc9bf68e1, 7 | BRF_SND },           // 11 Oki6295 #0 Samples

	{ "fz_07.k14",		0x20000, 0x588dd3cb, 8 | BRF_SND },           // 12 Oki6295 #1 Samples
};

STD_ROM_PICK(gatedom1)
STD_ROM_FN(gatedom1)

struct BurnDriver BurnDrvGatedom1 = {
	"gatedoom1", "darkseal", NULL, NULL, "1990",
	"Gate of Doom (US revision 1)\0", NULL, "Data East Corporation", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_MAZE | GBF_SCRFIGHT, 0,
	NULL, gatedom1RomInfo, gatedom1RomName, NULL, NULL, NULL, NULL, DarksealInputInfo, DarksealDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};
