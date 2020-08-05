// FinalBurn Neo Oriental Soft G-Stream driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "e132xs_intf.h"
#include "msm6295.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvBootROM;
static UINT8 *DrvMainROM;
static UINT8 *DrvGfxROM[4];
static UINT8 *DrvSndROM[2];
static UINT8 *DrvNVRAM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 okibank;
static INT32 scrollx[3];
static INT32 scrolly[3];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT16 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo GstreamInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 9,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 11,	"service"	},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 15,	"diag"		},
};

STDINPUTINFO(Gstream)

static void gstream_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x4f000000:
			scrollx[2] = data;
		return;

		case 0x4f200000:
			scrolly[2] = data;
		return;

		case 0x4f800000:
			scrollx[0] = data;
		return;

		case 0x4fa00000:
			scrolly[0] = data;
		return;

		case 0x4fc00000:
			scrollx[1] = data;
		return;

		case 0x4fe00000:
			scrolly[1] = data;
		return;
	}
}

static void set_okibank(UINT8 data)
{
	if (data != okibank) {
		okibank = data;

		INT32 bank0 = ((BIT(data, 6) & ~BIT(data, 7)) << 1) | (BIT(data, 2) & BIT(data, 3));
		INT32 bank1 = ((BIT(data, 4) & ~BIT(data, 5)) << 1) | (BIT(data, 0) & BIT(data, 1));

		MSM6295SetBank(0, DrvSndROM[0] + (bank0 * 0x40000), 0, 0x3ffff);
		MSM6295SetBank(1, DrvSndROM[1] + (bank1 * 0x40000), 0, 0x3ffff);
	}
}

static void gstream_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x4030:
			set_okibank(data);
		return;

		case 0x4040:
		return; // nop

		case 0x4050:
			MSM6295Write(0, data);
		return;

		case 0x4060:
			MSM6295Write(1, data);
		return;
	}
}

static UINT32 gstream_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x4000:
			return DrvInputs[0] + (DrvInputs[0] << 16);

		case 0x4010:
			return DrvInputs[1] + (DrvInputs[1] << 16);

		case 0x4020:
			return DrvInputs[2] + (DrvInputs[2] << 16);

		case 0x4050:
			return MSM6295Read(0);

		case 0x4060:
			return MSM6295Read(1);
	}

	return 0;
}

static UINT32 gstream_read_long(UINT32 address)
{
	if (address < 0x400000) {
		if (address == 0xd1ee0) {
			if (E132XSGetPC(0) == 0xc0001592) {
				E132XSBurnCycles(50);
			}
		}
		UINT32 ret = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvMainRAM + address)));
		return (ret << 16) | (ret >> 16);
	}

	return 0;
}

static UINT16 gstream_read_word(UINT32 address)
{
	if (address < 0x400000) {
		if (address == 0xd1ee0) {
			if (E132XSGetPC(0) == 0xc0001592) {
				E132XSBurnCycles(50);
			}
		}
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvMainRAM + address)));
	}

	return 0;
}

static UINT8 gstream_read_byte(UINT32 address)
{
	if (address < 0x400000) {
		if (address == 0xd1ee0) {
			if (E132XSGetPC(0) == 0xc0001592) {
				E132XSBurnCycles(50);
			}
		}
		return DrvMainRAM[address^1];
	}

	return 0;
}

static tilemap_callback( layer2 )
{
	UINT32 attr = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvVidRAM + 0x800 + offs * 4)));

	TILE_SET_INFO(2, attr, attr >> 14, 0);
}

static tilemap_callback( layer1 )
{
	UINT32 attr = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvVidRAM + 0x400 + offs * 4)));

	TILE_SET_INFO(1, attr, attr >> 14, 0);
}

static tilemap_callback( layer0 )
{
	UINT32 attr = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvVidRAM + 0x000 + offs * 4)));

	TILE_SET_INFO(0, attr, attr >> 14, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	E132XSOpen(0);
	E132XSReset();
	E132XSClose();

	set_okibank(0);
	MSM6295Reset(0);
	MSM6295Reset(1);

	memset (scrollx, 0, sizeof(scrollx));
	memset (scrolly, 0, sizeof(scrolly));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvBootROM		= Next; Next += 0x080000;
	DrvMainROM		= Next; Next += 0x200000;

	DrvGfxROM[0]	= Next; Next += 0x1000000;
	DrvGfxROM[1]	= Next; Next += 0x400000;
	DrvGfxROM[2]	= Next; Next += 0x400000;
	DrvGfxROM[3]	= Next; Next += 0x400000;

	MSM6295ROM		= Next;
	DrvSndROM[0]	= Next; Next += 0x100000;
	DrvSndROM[1]	= Next; Next += 0x100000;

	DrvNVRAM		= Next; Next += 0x002000;

	DrvPalette		= (UINT32*)Next; Next += 0x1c00 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x400000;
	DrvVidRAM		= Next; Next += 0x004000;
	DrvPalRAM		= Next; Next += 0x007000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRomExt(DrvBootROM + 0x000000, k++, 1, LD_BYTESWAP)) return 1;

		if (BurnLoadRomExt(DrvMainROM + 0x000000, k++, 1, 0)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[0] + 0x000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x000002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x400000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x400002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x800000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0x800002, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0xc00000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[0] + 0xc00002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x200000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x200000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[3] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[3] + 0x200000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[0] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[0] + 0x080000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM[1] + 0x080000, k++, 1)) return 1;

		if (BurnLoadRom(DrvNVRAM + 0x000000, k++, 1)) return 1;
	}

	E132XSInit(0, TYPE_E132XT, 64000000);
	E132XSOpen(0);
	E132XSMapMemory(DrvMainRAM,			0x00000000, 0x003fffff, MAP_RAM);
	E132XSMapMemory(DrvMainROM,			0x4e000000, 0x4e1fffff, MAP_ROM);
	E132XSMapMemory(DrvPalRAM,			0x4f400000, 0x4f406fff, MAP_RAM);
	E132XSMapMemory(DrvVidRAM,			0x80000000, 0x80003fff, MAP_RAM);
	E132XSMapMemory(DrvNVRAM,			0xffc00000, 0xffc01fff, MAP_RAM);
	E132XSMapMemory(DrvBootROM,			0xfff80000, 0xffffffff, MAP_ROM);
	E132XSSetWriteWordHandler(gstream_write_word);
	E132XSSetIOWriteHandler(gstream_io_write);
	E132XSSetIOReadHandler(gstream_io_read);

	E132XSMapMemory(NULL,				0x000d1000, 0x000d1fff, MAP_ROM); // unmap for speed hack
	E132XSSetReadLongHandler(gstream_read_long); // for speed hack
	E132XSSetReadWordHandler(gstream_read_word);
	E132XSSetReadByteHandler(gstream_read_byte);
	E132XSClose();

	MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, 0);
	MSM6295Init(1, 1000000 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 32, 32, 16, 16);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 32, 32, 16, 16);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, layer2_map_callback, 32, 32, 16, 16);
	GenericTilemapSetGfx(0, DrvGfxROM[1], 8, 32, 32, 0x0400000, 0x1000, 0x03);
	GenericTilemapSetGfx(1, DrvGfxROM[2], 8, 32, 32, 0x0400000, 0x1400, 0x03);
	GenericTilemapSetGfx(2, DrvGfxROM[3], 8, 32, 32, 0x0400000, 0x1800, 0x03);
	GenericTilemapSetGfx(3, DrvGfxROM[0], 8, 16, 16, 0x1000000, 0x0000, 0x1f);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapBuildSkipTable(0, 0, 0);
	GenericTilemapBuildSkipTable(1, 1, 0);
	GenericTilemapBuildSkipTable(2, 2, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit();
	E132XSExit();

	BurnFreeMemIndex();

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x1c00; i++) // BGR_565
	{
		UINT8 r = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 0) & 0x1f;
		UINT8 g = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 5) & 0x3f;
		UINT8 b = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 11) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 2) | (g >> 4);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	UINT32 *vram = (UINT32*)DrvVidRAM;

	for (INT32 i = 0; i < 0x1000; i+=4)
	{
		UINT32 code  = BURN_ENDIAN_SWAP_INT32(vram[i + 0]) >> 16;
		UINT32 sx    =(BURN_ENDIAN_SWAP_INT32(vram[i + 1]) >> 16) & 0x1ff;
		UINT32 sy    =(BURN_ENDIAN_SWAP_INT32(vram[i + 2]) >> 16) & 0xff;
		UINT32 color = BURN_ENDIAN_SWAP_INT32(vram[i + 3]) >> 16;

		DrawGfxMaskTile(0, 3, code, sx - 2,       sy,       0, 0, color, 0);
		DrawGfxMaskTile(0, 3, code, sx - 2,       sy - 256, 0, 0, color, 0);
		DrawGfxMaskTile(0, 3, code, sx - 2 - 512, sy,       0, 0, color, 0);
		DrawGfxMaskTile(0, 3, code, sx - 2 - 512, sy - 256, 0, 0, color, 0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force update
	}

	GenericTilemapSetScrollX(0, scrollx[0]);
	GenericTilemapSetScrollY(0, scrolly[0]);
	GenericTilemapSetScrollX(1, scrollx[1]);
	GenericTilemapSetScrollY(1, scrolly[1]);
	GenericTilemapSetScrollX(2, scrollx[2]);
	GenericTilemapSetScrollY(2, scrolly[2]);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(2, 0, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);
	if (nBurnLayer & 4) GenericTilemapDraw(0, 0, 0);

	if (nSpriteEnable & 1) draw_sprites();

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

		DrvInputs[1]  = (DrvInputs[1] & 0x7fff) | (~DrvInputs[0] & 0x8000);
		DrvInputs[2]  = (DrvInputs[2] & 0xffb0);
		DrvInputs[2] |=((DrvInputs[0] & 0x8200) >> 9);
		DrvInputs[2] |=((DrvInputs[1] & 0x0200) >> 8);
		DrvInputs[2] |=((DrvInputs[0] & 0x0400) >> 8);
		DrvInputs[2] |=((DrvInputs[1] & 0x0400) >> 7);
	}

	E132XSOpen(0);
	E132XSRun(64000000 / 60);
	E132XSSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	E132XSClose();

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		E132XSScan(nAction);

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(okibank);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
	}

	if (nAction & ACB_WRITE) {
		set_okibank(okibank);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvNVRAM;
		ba.nLen	  = 0x2000;
		ba.szName = "NV Ram";
		BurnAcb(&ba);
	}

	return 0;
}


// G-Stream G2020

static struct BurnRomInfo gstreamRomDesc[] = {
	{ "gs_prg_01.u56",	0x080000, 0x0d0c6a38, 1 | BRF_PRG | BRF_ESS }, //  0 Boot ROM

	{ "gs_prg_02.u197",	0x200000, 0x2f8a6bea, 2 | BRF_PRG | BRF_ESS }, //  1 Main Program ROM

	{ "gs_gr_07.u107",	0x200000, 0x84e66fe1, 3 | BRF_GRA },           //  2 Sprites
	{ "gs_gr_11.u108",	0x200000, 0x946d71d1, 3 | BRF_GRA },           //  3
	{ "gs_gr_08.u109",	0x200000, 0xabd0d6aa, 3 | BRF_GRA },           //  4
	{ "gs_gr_12.u110",	0x200000, 0x94b56e4e, 3 | BRF_GRA },           //  5
	{ "gs_gr_09.u180",	0x200000, 0xf2c4fd77, 3 | BRF_GRA },           //  6
	{ "gs_gr_13.u181",	0x200000, 0x7daaeff0, 3 | BRF_GRA },           //  7
	{ "gs_gr_10.u182",	0x200000, 0xd696d15d, 3 | BRF_GRA },           //  8
	{ "gs_gr_14.u183",	0x200000, 0x6bd2a1e1, 3 | BRF_GRA },           //  9

	{ "gs_gr_01.u120",	0x200000, 0xb82cfab8, 4 | BRF_GRA },           // 10 Background Layer 0 Tiles
	{ "gs_gr_02.u121",	0x200000, 0x37e19cbd, 4 | BRF_GRA },           // 11

	{ "gs_gr_03.u125",	0x200000, 0x1a3b2b11, 5 | BRF_GRA },           // 12 Background Layer 1 Tiles
	{ "gs_gr_04.u126",	0x200000, 0xa4e8906c, 5 | BRF_GRA },           // 13

	{ "gs_gr_05.u174",	0x200000, 0xef283a73, 6 | BRF_GRA },           // 14 Background Layer 2 Tiles
	{ "gs_gr_06.u175",	0x200000, 0xd4e3a2b2, 6 | BRF_GRA },           // 15

	{ "gs_snd_01.u192",	0x080000, 0x79b64d3f, 7 | BRF_SND },           // 16 OKI #0 Samples
	{ "gs_snd_02.u194",	0x080000, 0xe49ed92c, 7 | BRF_SND },           // 17

	{ "gs_snd_03.u191",	0x080000, 0x2bfff4ac, 8 | BRF_SND },           // 18 OKI #1 Samples
	{ "gs_snd_04.u193",	0x080000, 0xb259de3b, 8 | BRF_SND },           // 19

	{ "gstream.nv",		0x002000, 0x895d724b, 9 | BRF_PRG },           // 20 NV RAM Default Data
};

STD_ROM_PICK(gstream)
STD_ROM_FN(gstream)

struct BurnDriver BurnDrvGstream = {
	"gstream", NULL, NULL, NULL, "2002",
	"G-Stream G2020\0", NULL, "Oriental Soft Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, gstreamRomInfo, gstreamRomName, NULL, NULL, NULL, NULL, GstreamInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1c00,
	240, 320, 3, 4
};
