// FB Alpha Atari Akka Arrh driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "pokey.h"
#include "earom.h"
#include "watchdog.h"
#include "burn_gun.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 vblank;

static INT32 nExtraCycles;
static UINT8 video_mirror;

static INT16 DrvAnalogPortX = 0;
static INT16 DrvAnalogPortY = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo DrvInputList[] = {
	{"Coin 1",				BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"Coin 2",				BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"Coin 3",				BIT_DIGITAL,	DrvJoy2 + 2,	"p3 coin"	},
	{"Start 1",				BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"	},
	{"Start 2",				BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P1 Fire",				BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Power Blaster",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},
	{"P1 Zoom",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},

	A("P1 Trackball X", 	BIT_ANALOG_REL, &DrvAnalogPortX,"p1 x-axis"),
	A("P1 Trackball Y", 	BIT_ANALOG_REL, &DrvAnalogPortY,"p1 y-axis"),

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
};

STDINPUTINFO(Drv)
#undef A

static void akkaarrh_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x7010) {
		pokey_write(0, address & 0xf, data);
		return;
	}

	if ((address & 0xfff0) == 0x7020) {
		pokey_write(1, address & 0xf, data);
		return;
	}

	if ((address & 0xffc0) == 0x7040) {
		earom_write(address & 0x3f, data);
		return;
	}

	switch (address)
	{
		case 0x4000:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x5000:
			BurnWatchdogWrite();
		return;

		case 0x6000:
			video_mirror = data & 1;
		return;

		case 0x70c0:
		case 0x70c1:
		case 0x70c2:
		case 0x70c3:
            //bprintf(0, _T("??: %X %x\n"), address, data);
		return;

		case 0x70c7:
			earom_ctrl_write(0, data);
		return;
	}
}

static UINT8 akkaarrh_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x7010) {
		return pokey_read(0, address & 0xf);
	}

	if ((address & 0xfff0) == 0x7020) {
		return pokey_read(1, address & 0xf);
	}

	if ((address & 0xffc0) == 0x7040) {
		return 0; // nop
	}

	switch (address)
	{
		case 0x7080:
			return DrvInputs[0];

		case 0x7081:
			return DrvInputs[1];

		case 0x7082:
			return DrvInputs[2] | (BurnTrackballRead(0, 0) & 0xf);

		case 0x7083:
			return DrvInputs[3] | (vblank << 6) | (BurnTrackballRead(0, 1) & 0xf);

		case 0x7087:
			return earom_read(0);
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[offs + 0x400];
	INT32 code = DrvVidRAM[offs];

	TILE_SET_INFO(0, code, attr, TILE_FLIPYX(attr >> 6));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	M6502Reset();
	M6502Close();

	BurnWatchdogReset();
	earom_reset();

	video_mirror = 0;

    nExtraCycles = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x008000;

	DrvPalette		= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvPalRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { STEP4(0x1800*8, -(0x800*8)) };
	INT32 XOffs0[8]  = { STEP8(0,1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };

	INT32 Plane1[4]  = { STEP4(0x3000*8,-(0x1000*8)) };
	INT32 XOffs1[16] = { STEP16(0,1) };
	INT32 YOffs1[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0100, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x0080, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 port1_read(INT32 offset)
{
	return 0x00;
}

static INT32 port2_read(INT32 offset)
{
	return 0x00;
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
		INT32 k = 0;
		if (BurnLoadRom(DrvM6502ROM + 0x8000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x9000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xa000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xb000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xc000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xd000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xe000, k  , 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xf000, k++, 1)) return 1; // reload

		if (BurnLoadRom(DrvGfxROM0  + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x0800, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x1800, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x3000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,				0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,				0x1000, 0x10ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,				0x2000, 0x27ff, MAP_RAM);
	M6502MapMemory(DrvPalRAM,				0x3000, 0x30ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(akkaarrh_write);
	M6502SetReadHandler(akkaarrh_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180);

	earom_init();

	PokeyInit(1250000, 2, 0.50, 0);
    PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyPotCallback(0, 0, port1_read);
	PokeyPotCallback(0, 1, port1_read);
	PokeyPotCallback(0, 2, port1_read);
	PokeyPotCallback(0, 3, port1_read);
	PokeyPotCallback(0, 4, port1_read);
	PokeyPotCallback(0, 5, port1_read);
	PokeyPotCallback(0, 6, port1_read);
	PokeyPotCallback(0, 7, port1_read);

	PokeyPotCallback(1, 0, port2_read);
	PokeyPotCallback(1, 1, port2_read);
	PokeyPotCallback(1, 2, port2_read);
	PokeyPotCallback(1, 3, port2_read);
	PokeyPotCallback(1, 4, port2_read);
	PokeyPotCallback(1, 5, port2_read);
	PokeyPotCallback(1, 6, port2_read);
	PokeyPotCallback(1, 7, port2_read);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 30);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x4000, 0, 0xf);
    //	GenericTilemapSetOffsets(0, 0, -16);

	//GenericTilemapSetScrollCols(0, 36); - to test/fix fliping and clipping in
	// the other mode

    BurnTrackballInit(2);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	M6502Exit();
	PokeyExit();
	earom_exit();
	BurnTrackballExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		UINT8 r = pal3bit(DrvPalRAM[i] >> 5);
		UINT8 g = pal2bit(DrvPalRAM[i] >> 3);
		UINT8 b = pal3bit(DrvPalRAM[i] >> 0);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		INT32 code	= DrvSprRAM[offs + 1];
		INT32 color	= DrvSprRAM[offs] & 0xf;
		INT32 sx	= DrvSprRAM[offs + 3];
		INT32 sy	= 240 - DrvSprRAM[offs + 2];

		Draw16x16MaskTile(pTransDraw, code, sx, sy, 0, 0, color, 4, 0, 0, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	DrvPaletteUpdate();

    BurnTransferClear();

	if (video_mirror) // mirror top left quadrant
    {
		GenericTilemapSetFlip(0, 0);
		GenericTilesSetClip(0, 128, 0, 120);
		if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
		GenericTilesClearClip();

		GenericTilemapSetFlip(0, TMAP_FLIPX);
		GenericTilesSetClip(128, 256, 0, 120);
		if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, 0);
		GenericTilesClearClip();

		GenericTilemapSetFlip(0, TMAP_FLIPY);
		GenericTilesSetClip(0, 128, 120, 240);
		if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, 0);
		GenericTilesClearClip();

		GenericTilemapSetFlip(0, TMAP_FLIPX | TMAP_FLIPY);
		GenericTilesSetClip(128, 256, 120, 240);
        if (nBurnLayer & 8) GenericTilemapDraw(0, pTransDraw, 0);
		GenericTilesClearClip();

		GenericTilemapSetFlip(0, 0);
		GenericTilesClearClip();
    } else {
        if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
    }

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xf0;
		DrvInputs[3] = 0xb0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		{
			BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
			BurnTrackballFrame(0, DrvAnalogPortX, DrvAnalogPortY, 0x01, 0x03);
			BurnTrackballUpdate(0);
		}
	}

	INT32 nInterleave = 20;
	INT32 nCyclesTotal[1] = { 1512000 / 60 };
	INT32 nCyclesDone[1] = { nExtraCycles };
	INT32 nSoundBufferPos = 0;

	M6502Open(0);
    vblank = 0;

    for (INT32 i = 0; i < nInterleave; i++)
    {
        if (i == 15) vblank = 1;

		CPU_RUN(0, M6502);

        if ((i % 5) == 4) {
            M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
        }

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			pokey_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	M6502Close();

	nExtraCycles = nCyclesDone - nCyclesTotal;

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			pokey_update(pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029727;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);
		pokey_scan(nAction, pnMin);

		BurnTrackballScan();
		BurnWatchdogScan(nAction);

        SCAN_VAR(video_mirror);

        SCAN_VAR(nExtraCycles);
	}

	earom_scan(nAction, pnMin);

	return 0;
}


// Akka Arrh (prototype)

static struct BurnRomInfo akkaarrhRomDesc[] = {
	{ "akka_8000.p1",	0x1000, 0x578bb162, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "akka_9000.m1",	0x1000, 0x837fa612, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "akka_a000.l1",	0x1000, 0x13c769e9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "akka_b000.j1",	0x1000, 0x35c04f28, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "akka_c000.h1",	0x1000, 0x17e85ac4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "akka_d000.e1",	0x1000, 0x03fb4143, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "akka_e000.f1",	0x1000, 0x8d3e671c, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "akka_pf0.l6",	0x0800, 0x5c10b63e, 2 | BRF_GRA },           //  7 Background Tiles
	{ "akka_pf1.j6",	0x0800, 0x636fd64c, 2 | BRF_GRA },           //  8
	{ "akka_pf2.p6",	0x0800, 0xa5f25d69, 2 | BRF_GRA },           //  9
	{ "akka_pf3.m6",	0x0800, 0xa3449469, 2 | BRF_GRA },           // 10

	{ "akka_mo0.f11",	0x1000, 0x71bd1bc6, 3 | BRF_GRA },           // 11 Sprites
	{ "akka_mo1.d11",	0x1000, 0xa5ee8ecc, 3 | BRF_GRA },           // 12
	{ "akka_mo2.a11",	0x1000, 0x11cec4d9, 3 | BRF_GRA },           // 13
	{ "akka_mo3.b11",	0x1000, 0xadcf6a36, 3 | BRF_GRA },           // 14
};

STD_ROM_PICK(akkaarrh)
STD_ROM_FN(akkaarrh)

struct BurnDriver BurnDrvAkkaarrh = {
	"akkaarrh", NULL, NULL, NULL, "1982",
	"Akka Arrh (prototype)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, akkaarrhRomInfo, akkaarrhRomName, NULL, NULL, NULL, NULL, DrvInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 240, 4, 3
};
