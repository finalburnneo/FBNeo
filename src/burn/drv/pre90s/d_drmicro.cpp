// Dr. Micro emu-layer for FB Alpha by dink, based on the MAME driver by Uki.

#include "tiles_generic.h"
#include "driver.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "sn76496.h"
#include "msm5205.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColorPROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvChar4GFX;
static UINT8 *DrvChar8GFX;
static UINT8 *DrvSprite4GFX;
static UINT8 *DrvSprite8GFX;

static INT32 DrvGfxROM0Len;

static UINT8 *nmi_mask;
static INT32 pcm_adr;

static UINT8 flipscreen = 0;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2] = { 0, 0 };
static UINT8 DrvInput[2];
static UINT8 DrvReset;

static INT32 nCyclesTotal = 3072000;


static struct BurnInputInfo DrmicroInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 6,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Drmicro)


static struct BurnDIPInfo DrmicroDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x4d, NULL		},
	{0x10, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x03, 0x00, "2"		},
	{0x0f, 0x01, 0x03, 0x01, "3"		},
	{0x0f, 0x01, 0x03, 0x02, "4"		},
	{0x0f, 0x01, 0x03, 0x03, "5"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0f, 0x01, 0x04, 0x00, "Off"		},
	{0x0f, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0f, 0x01, 0x18, 0x00, "30000 100000"		},
	{0x0f, 0x01, 0x18, 0x08, "50000 150000"		},
	{0x0f, 0x01, 0x18, 0x10, "70000 200000"		},
	{0x0f, 0x01, 0x18, 0x18, "100000 300000"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0f, 0x01, 0x20, 0x00, "Off"		},
	{0x0f, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0f, 0x01, 0x40, 0x40, "Upright"		},
	{0x0f, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x0f, 0x01, 0x80, 0x00, "Off"		},
	{0x0f, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x10, 0x01, 0x07, 0x07, "4 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x06, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x05, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x00, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x07, 0x01, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x07, 0x02, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x07, 0x03, "1 Coin  4 Credits"		},
	{0x10, 0x01, 0x07, 0x04, "1 Coin  5 Credits"		},
};

STDDIPINFO(Drmicro)

static void DrvPaletteInit()
{
	const UINT8 *color_prom = DrvColorPROM;

	INT32 pal[0x100];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0, bit1, bit2, r, g, b;

		bit0 = (color_prom[i] >> 0) & 0x01;
		bit1 = (color_prom[i] >> 1) & 0x01;
		bit2 = (color_prom[i] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (color_prom[i] >> 3) & 0x01;
		bit1 = (color_prom[i] >> 4) & 0x01;
		bit2 = (color_prom[i] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = 0;
		bit1 = (color_prom[i] >> 6) & 0x01;
		bit2 = (color_prom[i] >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		pal[i] = BurnHighCol(r, g, b, 0);
	}

	color_prom += 0x20;

	for (INT32 i = 0; i < 0x200; i++)
	{
		DrvPalette[i] = pal[color_prom[i] & 0x0f];
	}
}

inline static INT32 DrvMSM5205SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)ZetTotalCycles() * nSoundRate / nCyclesTotal);
}

static void pcm_w()
{
	UINT8 *PCM = DrvSndROM;

	int data = PCM[pcm_adr / 2];

	if (data != 0x70)
	{
		if (~pcm_adr & 1)
			data >>= 4;

		MSM5205DataWrite(0, data & 0x0f);
		MSM5205ResetWrite(0, 0);
		MSM5205VCLKWrite(0, 1);
		MSM5205VCLKWrite(0, 0);

		pcm_adr = (pcm_adr + 1) & 0x7fff;
	}
	else
	{
		MSM5205ResetWrite(0, 1);
	}

}

static void __fastcall main_out(UINT16 port, UINT8 data)
{
	port &= 0xff;
	switch (port)
	{
		case 0x00:
		case 0x01:
		case 0x02:
			SN76496Write(port & 1, data);
		return;

		case 0x03:
			pcm_adr = ((data & 0x3f) << 9);
			pcm_w();
		return;

		case 0x04:
			*nmi_mask = data & 1;
			flipscreen = (data & 2) ? 1 : 0;
		return;
	}

	return;
}

static UINT8 __fastcall main_in(UINT16 port)
{
	port &= 0xff;
	switch (port)
	{
		case 0x00:
			return DrvInput[0];
		case 0x01:
			return DrvInput[1];
		case 0x03:
			return DrvDips[0];
		case 0x04:
			return DrvDips[1];
	}

	return 0;
}


static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	pcm_adr = 0;
	flipscreen = 0;

	MSM5205Reset();
	SN76496Reset();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x10000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);
	DrvChar4GFX      = Next; Next += 0x10000;
	DrvChar8GFX     = Next; Next += 0x10000;
	DrvSprite4GFX    = Next; Next += 0x10000;
	DrvSprite8GFX    = Next; Next += 0x10000;
	DrvColorPROM    = Next; Next += 0x00400;
	DrvSndROM       = Next; Next += 0x100000;
	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x01000;
	DrvZ80RAM1		= Next; Next += 0x01000;
	DrvVidRAM		= Next; Next += 0x01000;

	nmi_mask        = Next; Next += 0x00001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 GetRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *Load0 = DrvZ80ROM; //    1 main
	UINT8 *Loadg4 = DrvChar4GFX; //  2 gfx4
	UINT8 *Loadg8 = DrvChar8GFX; //  3 gfx8
	UINT8 *Loads = DrvSndROM; //   4 sound samples rom
	UINT8 *Loadc = DrvColorPROM; //   5 prom

	DrvGfxROM0Len = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 7) == 1) {
			if (BurnLoadRom(Load0, i, 1)) return 1;
			Load0 += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 2) {
			if (BurnLoadRom(Loadg4, i, 1)) return 1;
			Loadg4 += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 3) {
			if (BurnLoadRom(Loadg8, i, 1)) return 1;
			Loadg8 += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 4) {
			if (BurnLoadRom(Loads, i, 1)) return 1;
			Loads += ri.nLen;

			continue;
		}

		if ((ri.nType & 7) == 5) {
			if (BurnLoadRom(Loadc, i, 1)) return 1;
			Loadc += ri.nLen;

			continue;
		}
	}

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 charPlane4[2] =   { 0,0x2000*8 };
	INT32 charPlane8[3] =   { 0x2000*16,0x2000*8,0 };
	INT32 spritePlane4[2] = { 0,0x2000*8 };
	INT32 spritePlane8[3] = { 0x2000*16,0x2000*8,0 };

	INT32 sprXOffs[16] = { STEP8(7,-1),STEP8(71,-1) };
	INT32 sprYOffs[16] = { STEP8(0,8),STEP8(128,8) };

	INT32 charXOffs[8] = { STEP8(7,-1) };
	INT32 charYOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)malloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}
	memset(tmp, 0, 0x20000);
	memcpy(tmp, DrvChar4GFX, 0x4000);

	GfxDecode(0x100, 2, 16, 16, spritePlane4, sprXOffs, sprYOffs, 0x100, tmp + 0x0000, DrvSprite4GFX);
	GfxDecode(0x400, 2, 8, 8, charPlane4, charXOffs, charYOffs, 0x040, tmp + 0x0000, DrvChar4GFX);

	memset(tmp, 0, 0x20000);
	memcpy(tmp, DrvChar8GFX, 0x6000);

	GfxDecode(0x100, 3, 16, 16, spritePlane8, sprXOffs, sprYOffs, 0x100, tmp + 0x0000, DrvSprite8GFX);
	GfxDecode(0x400, 3, 8, 8, charPlane8, charXOffs, charYOffs, 0x040, tmp + 0x0000, DrvChar8GFX);

	free (tmp);

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
		if (GetRoms()) return 1;

		DrvGfxDecode();

		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		    0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		    0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1,		0xf000, 0xffff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		    0xe000, 0xefff, MAP_RAM);
	ZetSetOutHandler(main_out);
	ZetSetInHandler(main_in);
	ZetClose();

	SN76496Init(0, 4608000, 0);
	SN76496SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	SN76496Init(1, 4608000, 1);
	SN76496SetRoute(1, 0.50, BURN_SND_ROUTE_BOTH);
	SN76496Init(2, 4608000, 1);
	SN76496SetRoute(2, 0.50, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, DrvMSM5205SynchroniseStream, 384000, pcm_w, MSM5205_S64_4B, 1);
	MSM5205SetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}


static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	SN76496Exit();
	MSM5205Exit();

	BurnFree(AllMem);

	return 0;
}

static void draw_bg(INT32 layer)
{
	UINT8 bits = (layer == 0) ? 2 : 3;
	INT32 palind = (layer == 0) ? 0 : 0x100;

	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		sy -= 16; //offsets
		if (sy < -7) sy += 256;
		if (sx < -7) sx += 256;

		INT32 flipy = 0;
		INT32 flipx = 0;

		INT32 code = DrvVidRAM[offs + 0x0800];
		INT32 color = DrvVidRAM[offs + 0x0c00];

		if (layer == 1) {
			code = DrvVidRAM[offs + 0x0000];
			color = DrvVidRAM[offs + 0x0400];
		}

		code += (color & 0xc0) << 2;
		code &= 0x3ff;
		flipy = color & 0x20;
		flipx = color & 0x10;
		color &= 0x0f;

		if (sx > nScreenWidth || sy > nScreenHeight) continue;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, bits, 0, palind, (layer == 0) ? DrvChar4GFX : DrvChar8GFX);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, bits, 0, palind, (layer == 0) ? DrvChar4GFX : DrvChar8GFX);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, bits, 0, palind, (layer == 0) ? DrvChar4GFX : DrvChar8GFX);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, bits, 0, palind, (layer == 0) ? DrvChar4GFX : DrvChar8GFX);
			}
		}
	}
}


static void draw_sprites()
{
	for (INT32 g = 0; g < 2; g++)
	{
		UINT8 bits = (g != 0) ? 2 : 3;
		INT32 palind = (g != 0) ? 0 : 0x100;
		INT32 adr = 0x800 * g;

		for (INT32 offs = 0; offs < 0x20; offs += 4)
		{
			INT32 sx = DrvVidRAM[offs + adr + 3];
			INT32 sy = DrvVidRAM[offs + adr + 0];
			INT32 attr = DrvVidRAM[offs + adr + 2];
			INT32 code = DrvVidRAM[offs + adr + 1];

			INT32 flipx = (code & 0x01) ^ flipscreen;
			INT32 flipy = ((code & 0x02) >> 1) ^ flipscreen;

			code = (code >> 2) | (attr & 0xc0);

			INT32 color = (attr & 0x0f) + 0x00;

			if (!flipscreen)
				sy = (240 - sy) & 0xff;
			else
				sx = (240 - sx) & 0xff;

			sy -= 16; // offsets

			if (sy < -15) sy += 256;
			if (sx < -15) sx += 256;

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, bits, 0, palind, (g == 0) ? DrvSprite8GFX : DrvSprite4GFX);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, bits, 0, palind, (g == 0) ? DrvSprite8GFX : DrvSprite4GFX);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, bits, 0, palind, (g == 0) ? DrvSprite8GFX : DrvSprite4GFX);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, bits, 0, palind, (g == 0) ? DrvSprite8GFX : DrvSprite4GFX);
				}
			}

			if (sx > 240) {
				sx -= 256;
				if (flipy) { // again! yay!
					if (flipx) {
						Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, bits, 0, palind, (g == 0) ? DrvSprite8GFX : DrvSprite4GFX);
					} else {
						Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, bits, 0, palind, (g == 0) ? DrvSprite8GFX : DrvSprite4GFX);
					}
				} else {
					if (flipx) {
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, bits, 0, palind, (g == 0) ? DrvSprite8GFX : DrvSprite4GFX);
					} else {
						Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, bits, 0, palind, (g == 0) ? DrvSprite8GFX : DrvSprite4GFX);
					}
				}
			}
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

	if (nBurnLayer & 1) draw_bg(0);
	if (nBurnLayer & 2) draw_bg(1);
	if (nBurnLayer & 4) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void DrvMakeInputs()
{
	// Reset Inputs (all active HIGH)
	DrvInput[0] = 0;
	DrvInput[1] = 0;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << i;
		DrvInput[1] |= (DrvJoy2[i] & 1) << i;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	DrvMakeInputs();

	INT32 nInterleave = MSM5205CalcInterleave(0, nCyclesTotal);
	ZetNewFrame();
	ZetOpen(0);

	INT32 nSoundBufferPos = 0;

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetRun(nCyclesTotal / 60 / nInterleave);

		if (*nmi_mask && i == (nInterleave - 1))
			ZetNmi();

		MSM5205Update();

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			SN76496Update(1, pSoundBuf, nSegmentLength);
			SN76496Update(2, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			SN76496Update(0, pSoundBuf, nSegmentLength);
			SN76496Update(1, pSoundBuf, nSegmentLength);
			SN76496Update(2, pSoundBuf, nSegmentLength);
		}
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
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

	if (pnMin) {
		*pnMin = 0x029735;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		SN76496Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(pcm_adr);
	}

	return 0;
}

// Dr. Micro

static struct BurnRomInfo drmicroRomDesc[] = {
	{ "dm-00.13b",	0x2000, 0x270f2145, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "dm-01.14b",	0x2000, 0xbba30c80, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "dm-02.15b",	0x2000, 0xd9e4ca6b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "dm-03.13d",	0x2000, 0xb7bcb45b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "dm-04.14d",	0x2000, 0x071db054, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "dm-05.15d",	0x2000, 0xf41b8d8a, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "dm-23.5l",	0x2000, 0x279a76b8, 2 | BRF_GRA },           //  6 gfx1
	{ "dm-24.5n",	0x2000, 0xee8ed1ec, 2 | BRF_GRA },           //  7

	{ "dm-20.4a",	0x2000, 0x6f5dbf22, 3 | BRF_GRA },           //  8 gfx2
	{ "dm-21.4c",	0x2000, 0x8b17ff47, 3 | BRF_GRA },           //  9
	{ "dm-22.4d",	0x2000, 0x84daf771, 3 | BRF_GRA },           // 10

	{ "dm-40.12m",	0x2000, 0x3d080af9, 4 | BRF_SND },           // 11 adpcm
	{ "dm-41.13m",	0x2000, 0xddd7bda2, 4 | BRF_SND },           // 12

	{ "dm-62.9h",	0x0020, 0xe3e36eaf, 5 | BRF_GRA },           // 13 proms
	{ "dm-61.4m",	0x0100, 0x0dd8e365, 5 | BRF_GRA },           // 14
	{ "dm-60.6e",	0x0100, 0x540a3953, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(drmicro)
STD_ROM_FN(drmicro)

struct BurnDriver BurnDrvDrmicro = {
	"drmicro", NULL, NULL, NULL, "1983",
	"Dr. Micro\0", NULL, "Sanritsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, drmicroRomInfo, drmicroRomName, NULL, NULL, DrmicroInputInfo, DrmicroDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


