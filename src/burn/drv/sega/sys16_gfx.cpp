#include "sys16.h"
#include "thready.h"

INT32 System16SpriteBanks[16];
INT32 System16TileBanks[8];
INT32 System16OldTileBanks[8];
INT32 System16Page[4];
INT32 System16OldPage[4];
UINT8 BootlegFgPage[4];
UINT8 BootlegBgPage[4];
INT32 System16ScrollX[4] = {0, 0, 0, 0};
INT32 System16ScrollY[4] = {0, 0, 0, 0};
INT32 System16VideoEnable;
INT32 System16AVideoEnableDelayed; // for 16a
INT32 System18VdpEnable;
INT32 System18VdpMixing;
INT32 System16ScreenFlip = 0;
INT32 System16ScreenFlipXoffs = 0; // 16B
INT32 System16ScreenFlipYoffs = 0; // 16B
INT32 System16SpriteShadow;
INT32 System16SpriteXOffset = 0;
INT32 System16SpriteYOffset = 0; // only sys16b/c (fantznps2), only positive #'s!
INT32 System16ColScroll = 0;
INT32 System16RowScroll = 0;
INT32 System16RoadControl = 0;
INT32 System16RoadColorOffset1 = 0;
INT32 System16RoadColorOffset2 = 0;
INT32 System16RoadColorOffset3 = 0;
INT32 System16RoadXOffset = 0;
INT32 System16RoadPriority = 0;
INT32 System16PaletteEntries = 0;
INT32 System16SpritePalOffset = 0x400;
INT32 System16TilemapColorOffset = 0;
INT32 System16TileBankSize = 0;
INT32 System16RecalcBgTileMap = 0;
INT32 System16RecalcBgAltTileMap = 0;
INT32 System16RecalcFgTileMap = 0;
INT32 System16RecalcFgAltTileMap = 0;
INT32 System16CreateOpaqueTileMaps = 0;
INT32 System16IgnoreVideoEnable = 0;

bool bSystem16BootlegRender;

UINT16 *pTempDraw = NULL;
static UINT16 *pSys16BgTileMapOpaque = NULL;
static UINT16 *pSys16BgAltTileMapOpaque = NULL;
static UINT16 *pSys16BgTileMapPri0 = NULL;
static UINT16 *pSys16BgTileMapPri1 = NULL;
static UINT16 *pSys16FgTileMapPri0 = NULL;
static UINT16 *pSys16FgTileMapPri1 = NULL;
static UINT16 *pSys16BgAltTileMapPri0 = NULL;
static UINT16 *pSys16BgAltTileMapPri1 = NULL;
static UINT16 *pSys16FgAltTileMapPri0 = NULL;
static UINT16 *pSys16FgAltTileMapPri1 = NULL;

static UINT16 *pSpriteBitmap = NULL;

static UINT8 System16PaletteNormal[32];
static UINT8 System16PaletteShadow[32];
static UINT8 System16PaletteHilight[32];

static INT32 AltModeKludge = 0; // a couple System 16B games have slightly different rendering (dunkshot, timescan)

/*====================================================
Scan Function
====================================================*/

void System16GfxScan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(System16VideoEnable);
		SCAN_VAR(System16AVideoEnableDelayed);

		if (nAction & ACB_WRITE) {
			if (((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM16A) || ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_HANGON))  {
				System16RecalcBgTileMap = 1;
				System16RecalcFgTileMap = 1;
			}

			if (((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM16B) || ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM18) || ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_OUTRUN) || ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEMX)) {
				System16RecalcBgTileMap = 1;
				System16RecalcBgAltTileMap = 1;
				System16RecalcFgTileMap = 1;
				System16RecalcFgAltTileMap = 1;
			}
		}
	}
}

/*====================================================
Decode Functions
====================================================*/

void System16Decode8x8Tiles(UINT8 *pTile, INT32 Num, INT32 offs1, INT32 offs2, INT32 offs3)
{
	INT32 c, y, x, Dat1, Dat2, Dat3, Col;

	for (c = 0; c < Num; c++) {
		for (y = 0; y < 8; y++) {
			Dat1 = System16TempGfx[offs1 + (c * 8) + y];
			Dat2 = System16TempGfx[offs2 + (c * 8) + y];
			Dat3 = System16TempGfx[offs3 + (c * 8) + y];
			for (x = 0; x < 8; x++) {
				Col = 0;
				if (Dat1 & 1) { Col |= 4;}
				if (Dat2 & 1) { Col |= 2;}
				if (Dat3 & 1) { Col |= 1;}
				pTile[(c * 64) + (y * 8) + (7 - x)] = Col;
				Dat1 >>= 1;
				Dat2 >>= 1;
				Dat3 >>= 1;
			}
		}
	}
}

void OutrunDecodeRoad()
{
	for (INT32 y = 0; y < 256 * 2; y++) {
		UINT8 *src = System16TempGfx + ((y & 0xff) * 0x40 + (y >> 8) * 0x8000) % System16RoadRomSize;
		UINT8 *dst = System16Roads + y * 512;

		/* loop over columns */
		for (INT32 x = 0; x < 512; x++)
		{
			dst[x] = (((src[x/8] >> (~x & 7)) & 1) << 0) | (((src[x/8 + 0x4000] >> (~x & 7)) & 1) << 1);

			/* pre-mark road data in the "stripe" area with a high bit */
			if (x >= 256-8 && x < 256 && dst[x] == 3)
				dst[x] |= 4;
		}
	}

	/* set up a dummy road in the last entry */
	memset(System16Roads + 256 * 2 * 512, 3, 512);
}

void HangonDecodeRoad()
{
	INT32 x, y;

	/* loop over rows */
	for (y = 0; y < 256; y++)
	{
		UINT8 *src = System16TempGfx + ((y & 0xff) * 0x40) % System16RoadRomSize;
		UINT8 *dst = System16Roads + y * 512;

		/* loop over columns */
		for (x = 0; x < 512; x++)
			dst[x] = (((src[x/8] >> (~x & 7)) & 1) << 0) | (((src[x/8 + 0x4000] >> (~x & 7)) & 1) << 1);
	}
}

/*====================================================
Tile Layer Rendering
====================================================*/

#define PLOTPIXEL(x, po) pPixel[x] = nPalette | pTileData[x] | po;
#define PLOTPIXEL_MASK(x, mc, po) if (pTileData[x] != mc) {pPixel[x] = nPalette | pTileData[x] | po;}

static inline void RenderTile(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
	UINT32 nPalette = nTilePalette << nColourDepth;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * 1024) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += 1024, pTileData += 8) {
		PLOTPIXEL(0, nPaletteOffset);
		PLOTPIXEL(1, nPaletteOffset);
		PLOTPIXEL(2, nPaletteOffset);
		PLOTPIXEL(3, nPaletteOffset);
		PLOTPIXEL(4, nPaletteOffset);
		PLOTPIXEL(5, nPaletteOffset);
		PLOTPIXEL(6, nPaletteOffset);
		PLOTPIXEL(7, nPaletteOffset);
	}
}

static void RenderTile_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
	UINT32 nPalette = nTilePalette << nColourDepth;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * 1024) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += 1024, pTileData += 8) {
		PLOTPIXEL_MASK(0, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(1, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(2, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(3, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(4, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(5, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(6, nMaskColour, nPaletteOffset);
		PLOTPIXEL_MASK(7, nMaskColour, nPaletteOffset);
	}
}

#undef PLOTPIXEL
#undef PLOTPIXEL_MASK

void System16ATileMapsInit(INT32 bOpaque)
{
	if (bOpaque) {
		pSys16BgTileMapOpaque = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
	}
	
	pSys16BgTileMapPri0 = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
	pSys16BgTileMapPri1 = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
	pSys16FgTileMapPri0 = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
	pSys16FgTileMapPri1 = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));

	pSpriteBitmap = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
}

void System16BTileMapsInit(INT32 bOpaque)
{
	if (bOpaque) {
		pSys16BgTileMapOpaque = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
		pSys16BgAltTileMapOpaque = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
	}
	
	pSys16BgTileMapPri0 = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
	pSys16BgTileMapPri1 = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
	pSys16FgTileMapPri0 = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
	pSys16FgTileMapPri1 = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
	
	pSys16BgAltTileMapPri0 = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
	pSys16BgAltTileMapPri1 = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
	pSys16FgAltTileMapPri0 = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
	pSys16FgAltTileMapPri1 = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));

	pSpriteBitmap = (UINT16*)BurnMalloc(1024 * 512 * sizeof(UINT16));
}

void System16TileMapsExit()
{
	BurnFree(pSys16BgTileMapOpaque);
	BurnFree(pSys16BgAltTileMapOpaque);
	BurnFree(pSys16BgTileMapPri0);
	BurnFree(pSys16BgTileMapPri1);
	BurnFree(pSys16FgTileMapPri0);
	BurnFree(pSys16FgTileMapPri1);
	BurnFree(pSys16BgAltTileMapPri0);
	BurnFree(pSys16BgAltTileMapPri1);
	BurnFree(pSys16FgAltTileMapPri0);
	BurnFree(pSys16FgAltTileMapPri1);

	BurnFree(pSpriteBitmap);

	AltModeKludge = 0;
}

static void System16ACreateBgTileMaps()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex, Priority, ColourOff, TilePage, i, xOffs, yOffs;
	UINT16 *VideoRam = (UINT16*)System16TileRam;
	UINT16 EffPage, ActPage;
	UINT16 *pDest = NULL;
	
	if (System16CreateOpaqueTileMaps) memset(pSys16BgTileMapOpaque, 0, 1024 * 512 * sizeof(UINT16));
	EffPage = System16Page[1];
	EffPage = ((EffPage >> 4) & 0x0707) | ((EffPage << 4) & 0x7070);
	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_HANGON) EffPage &= 0x3333;
	for (i = 0; i < 2; i++) {
		if (i == 0) pDest = pSys16BgTileMapPri0;
		if (i == 1) pDest = pSys16BgTileMapPri1;
		memset(pDest, 0, 1024 * 512 * sizeof(UINT16));
		for (TilePage = 0; TilePage < 4; TilePage++) {
			ActPage = (EffPage >> 0) & 0x0f;
			xOffs = 0;
			yOffs = 0;
			if (TilePage == 1) { ActPage = (EffPage >> 4) & 0x0f; xOffs = 512; }
			if (TilePage == 2) { ActPage = (EffPage >> 8) & 0x0f; yOffs = 256; }
			if (TilePage == 3) { ActPage = (EffPage >> 12) & 0x0f; xOffs = 512; yOffs = 256; }
			for (my = 0; my < 32; my++) {
				y = 8 * my;
				y += yOffs;
				for (mx = 0; mx < 64; mx++) {
					TileIndex = (ActPage * 64 * 32) + (my * 64) + mx;
						
					Attr = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
					Priority = (Attr >> 12) & 1;
			
					if (Priority != i) continue;
					
					x = 8 * mx;
					x += xOffs;
								
					Code = ((Attr >> 1) & 0x1000) | (Attr & 0xfff);
					Code &= (System16NumTiles - 1);
					Colour = (Attr >> 5) & 0x7f;
				
					ColourOff = System16TilemapColorOffset;
					if (Colour >= 0x20) ColourOff = 0x100 | System16TilemapColorOffset;
					if (Colour >= 0x40) ColourOff = 0x200 | System16TilemapColorOffset;
					if (Colour >= 0x60) ColourOff = 0x300 | System16TilemapColorOffset;
			
					if (System16CreateOpaqueTileMaps) RenderTile(pSys16BgTileMapOpaque, Code, x, y, Colour, 3, ColourOff, System16Tiles);
					RenderTile_Mask(pDest, Code, x, y, Colour, 3, 0, ColourOff, System16Tiles);
				}
			}
		}
	}
}

static void System16ACreateFgTileMaps()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex, Priority, ColourOff, TilePage, xOffs, yOffs;
	UINT16 *VideoRam = (UINT16*)System16TileRam;
	UINT16 EffPage, ActPage;
	
	memset(pSys16FgTileMapPri0, 0, 1024 * 512 * sizeof(UINT16));
	memset(pSys16FgTileMapPri1, 0, 1024 * 512 * sizeof(UINT16));
		
	EffPage = System16Page[0];
	EffPage = ((EffPage >> 4) & 0x0707) | ((EffPage << 4) & 0x7070);
	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_HANGON) EffPage &= 0x3333;
	for (TilePage = 0; TilePage < 4; TilePage++) {
		ActPage = (EffPage >> 0) & 0x0f;
		xOffs = 0;
		yOffs = 0;
		if (TilePage == 1) { ActPage = (EffPage >> 4) & 0x0f; xOffs = 512; }
		if (TilePage == 2) { ActPage = (EffPage >> 8) & 0x0f; yOffs = 256; }
		if (TilePage == 3) { ActPage = (EffPage >> 12) & 0x0f; xOffs = 512; yOffs = 256; }
		for (my = 0; my < 32; my++) {
			y = 8 * my;
			y += yOffs;
			for (mx = 0; mx < 64; mx++) {
				TileIndex = (ActPage * 64 * 32) + (my * 64) + mx;
					
				Attr = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
				Priority = (Attr >> 12) & 1;
			
				x = 8 * mx;
				x += xOffs;
			
				Code = ((Attr >> 1) & 0x1000) | (Attr & 0xfff);
				Code &= (System16NumTiles - 1);
				Colour = (Attr >> 5) & 0x7f;
				
				ColourOff = System16TilemapColorOffset;
				if (Colour >= 0x20) ColourOff = 0x100 | System16TilemapColorOffset;
				if (Colour >= 0x40) ColourOff = 0x200 | System16TilemapColorOffset;
				if (Colour >= 0x60) ColourOff = 0x300 | System16TilemapColorOffset;
		
				if (Priority == 0) RenderTile_Mask(pSys16FgTileMapPri0, Code, x, y, Colour, 3, 0, ColourOff, System16Tiles);
				if (Priority == 1) RenderTile_Mask(pSys16FgTileMapPri1, Code, x, y, Colour, 3, 0, ColourOff, System16Tiles);
			}
		}
	}
}

static void System16BCreateBgTileMaps()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex, Priority, ColourOff, TilePage, i, xOffs, yOffs;
	UINT16 *VideoRam = (UINT16*)System16TileRam;
	UINT16 EffPage, ActPage;
	UINT16 *pDest = NULL;
	
	INT32 ColourDepth = 3;
	INT32 ColourOffsMultiplier = 1;
	if (Lockonph) {
		ColourDepth = 4;
		ColourOffsMultiplier = 2;
	}
		
	if (System16CreateOpaqueTileMaps) memset(pSys16BgTileMapOpaque, 0, 1024 * 512 * sizeof(UINT16));
	EffPage = System16Page[1];
	for (i = 0; i < 2; i++) {
		if (i == 0) pDest = pSys16BgTileMapPri0;
		if (i == 1) pDest = pSys16BgTileMapPri1;
		memset(pDest, 0, 1024 * 512 * sizeof(UINT16));
		for (TilePage = 0; TilePage < 4; TilePage++) {
			ActPage = (EffPage >> 0) & 0x0f;
			xOffs = 0;
			yOffs = 0;
			if (TilePage == 1) { ActPage = (EffPage >> 4) & 0x0f; xOffs = 512; }
			if (TilePage == 2) { ActPage = (EffPage >> 8) & 0x0f; yOffs = 256; }
			if (TilePage == 3) { ActPage = (EffPage >> 12) & 0x0f; xOffs = 512; yOffs = 256; }
			for (my = 0; my < 32; my++) {
				y = 8 * my;
				y += yOffs;
				for (mx = 0; mx < 64; mx++) {
					TileIndex = (ActPage * 64 * 32) + (my * 64) + mx;
						
					Attr = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
					Priority = (Attr >> 15) & 1;
			
					if (Priority != i) continue;
					
					x = 8 * mx;
					x += xOffs;
								
					Code = Attr & 0x1fff;
					Code = System16TileBanks[Code / System16TileBankSize] * System16TileBankSize + Code % System16TileBankSize;
					Code &= (System16NumTiles - 1);
					Colour = (Attr >> 6) & 0x7f;
				
					ColourOff = System16TilemapColorOffset;
					if (Colour >= 0x20) ColourOff = (0x100 * ColourOffsMultiplier) | System16TilemapColorOffset;
					if (Colour >= 0x40) ColourOff = (0x200 * ColourOffsMultiplier) | System16TilemapColorOffset;
					if (Colour >= 0x60) ColourOff = (0x300 * ColourOffsMultiplier) | System16TilemapColorOffset;
			
					if (System16CreateOpaqueTileMaps) RenderTile(pSys16BgTileMapOpaque, Code, x, y, Colour, ColourDepth, ColourOff, System16Tiles);
					RenderTile_Mask(pDest, Code, x, y, Colour, ColourDepth, 0, ColourOff, System16Tiles);
				}
			}
		}
	}
}

static void System16BCreateBgAltTileMaps()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex, Priority, ColourOff, TilePage, i, xOffs, yOffs;
	UINT16 *VideoRam = (UINT16*)System16TileRam;
	UINT16 EffPage, ActPage;
	UINT16 *pDest = NULL;
	
	INT32 ColourDepth = 3;
	INT32 ColourOffsMultiplier = 1;
	if (Lockonph) {
		ColourDepth = 4;
		ColourOffsMultiplier = 2;
	}
	
	if (System16CreateOpaqueTileMaps) memset(pSys16BgAltTileMapOpaque, 0, 1024 * 512 * sizeof(UINT16));
	EffPage = System16Page[3];
	for (i = 0; i < 2; i++) {
		if (i == 0) pDest = pSys16BgAltTileMapPri0;
		if (i == 1) pDest = pSys16BgAltTileMapPri1;
		if (System16RecalcBgAltTileMap) memset(pDest, 0,1024 * 512 * sizeof(UINT16));
		for (TilePage = 0; TilePage < 4; TilePage++) {
			ActPage = (EffPage >> 0) & 0x0f;
			xOffs = 0;
			yOffs = 0;
			if (TilePage == 1) { ActPage = (EffPage >> 4) & 0x0f; xOffs = 512; }
			if (TilePage == 2) { ActPage = (EffPage >> 8) & 0x0f; yOffs = 256; }
			if (TilePage == 3) { ActPage = (EffPage >> 12) & 0x0f; xOffs = 512; yOffs = 256; }
			for (my = 0; my < 32; my++) {
				y = 8 * my;
				y += yOffs;
				for (mx = 0; mx < 64; mx++) {
					TileIndex = (ActPage * 64 * 32) + (my * 64) + mx;
						
					Attr = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
					Priority = (Attr >> 15) & 1;
			
					if (Priority != i) continue;
					
					x = 8 * mx;
					x += xOffs;
			
					Code = Attr & 0x1fff;
					Code = System16TileBanks[Code / System16TileBankSize] * System16TileBankSize + Code % System16TileBankSize;
					Code &= (System16NumTiles - 1);
					Colour = (Attr >> 6) & 0x7f;
				
					ColourOff = System16TilemapColorOffset;
					if (Colour >= 0x20) ColourOff = (0x100 * ColourOffsMultiplier) | System16TilemapColorOffset;
					if (Colour >= 0x40) ColourOff = (0x200 * ColourOffsMultiplier) | System16TilemapColorOffset;
					if (Colour >= 0x60) ColourOff = (0x300 * ColourOffsMultiplier) | System16TilemapColorOffset;
			
					if (System16CreateOpaqueTileMaps) RenderTile(pSys16BgAltTileMapOpaque, Code, x, y, Colour, ColourDepth, ColourOff, System16Tiles);
					if (System16RecalcBgAltTileMap) RenderTile_Mask(pDest, Code, x, y, Colour, ColourDepth, 0, ColourOff, System16Tiles);
				}
			}
		}
	}
}

static void System16BCreateFgTileMaps()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex, Priority, ColourOff, TilePage, xOffs, yOffs;
	UINT16 *VideoRam = (UINT16*)System16TileRam;
	UINT16 EffPage, ActPage;
	
	INT32 ColourDepth = 3;
	INT32 ColourOffsMultiplier = 1;
	if (Lockonph) {
		ColourDepth = 4;
		ColourOffsMultiplier = 2;
	}
	
	memset(pSys16FgTileMapPri0, 0, 1024 * 512 * sizeof(UINT16));
	memset(pSys16FgTileMapPri1, 0, 1024 * 512 * sizeof(UINT16));
		
	EffPage = System16Page[0];
	for (TilePage = 0; TilePage < 4; TilePage++) {
		ActPage = (EffPage >> 0) & 0x0f;
		xOffs = 0;
		yOffs = 0;
		if (TilePage == 1) { ActPage = (EffPage >> 4) & 0x0f; xOffs = 512; }
		if (TilePage == 2) { ActPage = (EffPage >> 8) & 0x0f; yOffs = 256; }
		if (TilePage == 3) { ActPage = (EffPage >> 12) & 0x0f; xOffs = 512; yOffs = 256; }
		for (my = 0; my < 32; my++) {
			y = 8 * my;
			y += yOffs;
			for (mx = 0; mx < 64; mx++) {
				TileIndex = (ActPage * 64 * 32) + (my * 64) + mx;
					
				Attr = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
				Priority = (Attr >> 15) & 1;
			
				x = 8 * mx;
				x += xOffs;
			
				Code = Attr & 0x1fff;
				Code = System16TileBanks[Code / System16TileBankSize] * System16TileBankSize + Code % System16TileBankSize;
				Code &= (System16NumTiles - 1);
				Colour = (Attr >> 6) & 0x7f;

				ColourOff = System16TilemapColorOffset;
				if (Colour >= 0x20) ColourOff = (0x100 * ColourOffsMultiplier) | System16TilemapColorOffset;
				if (Colour >= 0x40) ColourOff = (0x200 * ColourOffsMultiplier) | System16TilemapColorOffset;
				if (Colour >= 0x60) ColourOff = (0x300 * ColourOffsMultiplier) | System16TilemapColorOffset;
		
				if (Priority == 0) RenderTile_Mask(pSys16FgTileMapPri0, Code, x, y, Colour, ColourDepth, 0, ColourOff, System16Tiles);
				if (Priority == 1) RenderTile_Mask(pSys16FgTileMapPri1, Code, x, y, Colour, ColourDepth, 0, ColourOff, System16Tiles);
			}
		}
	}
}

static void System16BCreateFgAltTileMaps()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex, Priority, ColourOff, TilePage, xOffs, yOffs;
	UINT16 *VideoRam = (UINT16*)System16TileRam;
	UINT16 EffPage, ActPage;
	
	INT32 ColourDepth = 3;
	INT32 ColourOffsMultiplier = 1;
	if (Lockonph) {
		ColourDepth = 4;
		ColourOffsMultiplier = 2;
	}
	
	memset(pSys16FgAltTileMapPri0, 0, 1024 * 512 * sizeof(UINT16));
	memset(pSys16FgAltTileMapPri1, 0, 1024 * 512 * sizeof(UINT16));
		
	EffPage = System16Page[2];
	for (TilePage = 0; TilePage < 4; TilePage++) {
		ActPage = (EffPage >> 0) & 0x0f;
		xOffs = 0;
		yOffs = 0;
		if (TilePage == 1) { ActPage = (EffPage >> 4) & 0x0f; xOffs = 512; }
		if (TilePage == 2) { ActPage = (EffPage >> 8) & 0x0f; yOffs = 256; }
		if (TilePage == 3) { ActPage = (EffPage >> 12) & 0x0f; xOffs = 512; yOffs = 256; }
		for (my = 0; my < 32; my++) {
			y = 8 * my;
			y += yOffs;
			for (mx = 0; mx < 64; mx++) {
				TileIndex = (ActPage * 64 * 32) + (my * 64) + mx;
					
				Attr = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
				Priority = (Attr >> 15) & 1;
		
				x = 8 * mx;
				x += xOffs;
			
				Code = Attr & 0x1fff;
				Code = System16TileBanks[Code / System16TileBankSize] * System16TileBankSize + Code % System16TileBankSize;
				Code &= (System16NumTiles - 1);
				Colour = (Attr >> 6) & 0x7f;
			
				ColourOff = System16TilemapColorOffset;
				if (Colour >= 0x20) ColourOff = (0x100 * ColourOffsMultiplier) | System16TilemapColorOffset;
				if (Colour >= 0x40) ColourOff = (0x200 * ColourOffsMultiplier) | System16TilemapColorOffset;
				if (Colour >= 0x60) ColourOff = (0x300 * ColourOffsMultiplier) | System16TilemapColorOffset;
		
				if (Priority == 0) RenderTile_Mask(pSys16FgAltTileMapPri0, Code, x, y, Colour, ColourDepth, 0, ColourOff, System16Tiles);
				if (Priority == 1) RenderTile_Mask(pSys16FgAltTileMapPri1, Code, x, y, Colour, ColourDepth, 0, ColourOff, System16Tiles);
			}
		}
	}
}

static void System16BAltCreateBgTileMaps()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex, Priority, ColourOff, TilePage, i, xOffs, yOffs;
	UINT16 *VideoRam = (UINT16*)System16TileRam;
	UINT16 EffPage, ActPage;
	UINT16 *pDest = NULL;
		
	if (System16CreateOpaqueTileMaps) memset(pSys16BgTileMapOpaque, 0, 1024 * 512 * sizeof(UINT16));
	EffPage = System16Page[1];
	for (i = 0; i < 2; i++) {
		if (i == 0) pDest = pSys16BgTileMapPri0;
		if (i == 1) pDest = pSys16BgTileMapPri1;
		memset(pDest, 0, 1024 * 512 * sizeof(UINT16));
		for (TilePage = 0; TilePage < 4; TilePage++) {
			ActPage = (EffPage >> 0) & 0x0f;
			xOffs = 0;
			yOffs = 0;
			if (TilePage == 1) { ActPage = (EffPage >> 4) & 0x0f; xOffs = 512; }
			if (TilePage == 2) { ActPage = (EffPage >> 8) & 0x0f; yOffs = 256; }
			if (TilePage == 3) { ActPage = (EffPage >> 12) & 0x0f; xOffs = 512; yOffs = 256; }
			for (my = 0; my < 32; my++) {
				y = 8 * my;
				y += yOffs;
				for (mx = 0; mx < 64; mx++) {
					TileIndex = (ActPage * 64 * 32) + (my * 64) + mx;
						
					Attr = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
					Priority = (Attr >> 15) & 1;
			
					if (Priority != i) continue;
					
					x = 8 * mx;
					x += xOffs;
								
					Code = Attr & 0x1fff;
					Code = System16TileBanks[Code / System16TileBankSize] * System16TileBankSize + Code % System16TileBankSize;
					Code &= (System16NumTiles - 1);
					Colour = (Attr >> 5) & 0x7f;
				
					ColourOff = System16TilemapColorOffset;
					if (Colour >= 0x20) ColourOff = 0x100 | System16TilemapColorOffset;
					if (Colour >= 0x40) ColourOff = 0x200 | System16TilemapColorOffset;
					if (Colour >= 0x60) ColourOff = 0x300 | System16TilemapColorOffset;
			
					if (System16CreateOpaqueTileMaps) RenderTile(pSys16BgTileMapOpaque, Code, x, y, Colour, 3, ColourOff, System16Tiles);
					RenderTile_Mask(pDest, Code, x, y, Colour, 3, 0, ColourOff, System16Tiles);
				}
			}
		}
	}
}

static void System16BAltCreateBgAltTileMaps()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex, Priority, ColourOff, TilePage, i, xOffs, yOffs;
	UINT16 *VideoRam = (UINT16*)System16TileRam;
	UINT16 EffPage, ActPage;
	UINT16 *pDest = NULL;
	
	if (System16CreateOpaqueTileMaps) memset(pSys16BgAltTileMapOpaque, 0, 1024 * 512 * sizeof(UINT16));
	EffPage = System16Page[3];
	for (i = 0; i < 2; i++) {
		if (i == 0) pDest = pSys16BgAltTileMapPri0;
		if (i == 1) pDest = pSys16BgAltTileMapPri1;
		if (System16RecalcBgAltTileMap) memset(pDest, 0,1024 * 512 * sizeof(UINT16));
		for (TilePage = 0; TilePage < 4; TilePage++) {
			ActPage = (EffPage >> 0) & 0x0f;
			xOffs = 0;
			yOffs = 0;
			if (TilePage == 1) { ActPage = (EffPage >> 4) & 0x0f; xOffs = 512; }
			if (TilePage == 2) { ActPage = (EffPage >> 8) & 0x0f; yOffs = 256; }
			if (TilePage == 3) { ActPage = (EffPage >> 12) & 0x0f; xOffs = 512; yOffs = 256; }
			for (my = 0; my < 32; my++) {
				y = 8 * my;
				y += yOffs;
				for (mx = 0; mx < 64; mx++) {
					TileIndex = (ActPage * 64 * 32) + (my * 64) + mx;
						
					Attr = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
					Priority = (Attr >> 15) & 1;
			
					if (Priority != i) continue;
					
					x = 8 * mx;
					x += xOffs;
			
					Code = Attr & 0x1fff;
					Code = System16TileBanks[Code / System16TileBankSize] * System16TileBankSize + Code % System16TileBankSize;
					Code &= (System16NumTiles - 1);
					Colour = (Attr >> 5) & 0x7f;
				
					ColourOff = System16TilemapColorOffset;
					if (Colour >= 0x20) ColourOff = 0x100 | System16TilemapColorOffset;
					if (Colour >= 0x40) ColourOff = 0x200 | System16TilemapColorOffset;
					if (Colour >= 0x60) ColourOff = 0x300 | System16TilemapColorOffset;
			
					if (System16CreateOpaqueTileMaps) RenderTile(pSys16BgAltTileMapOpaque, Code, x, y, Colour, 3, ColourOff, System16Tiles);
					if (System16RecalcBgAltTileMap) RenderTile_Mask(pDest, Code, x, y, Colour, 3, 0, ColourOff, System16Tiles);
				}
			}
		}
	}
}

static void System16BAltCreateFgTileMaps()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex, Priority, ColourOff, TilePage, xOffs, yOffs;
	UINT16 *VideoRam = (UINT16*)System16TileRam;
	UINT16 EffPage, ActPage;
	
	memset(pSys16FgTileMapPri0, 0, 1024 * 512 * sizeof(UINT16));
	memset(pSys16FgTileMapPri1, 0, 1024 * 512 * sizeof(UINT16));
		
	EffPage = System16Page[0];
	for (TilePage = 0; TilePage < 4; TilePage++) {
		ActPage = (EffPage >> 0) & 0x0f;
		xOffs = 0;
		yOffs = 0;
		if (TilePage == 1) { ActPage = (EffPage >> 4) & 0x0f; xOffs = 512; }
		if (TilePage == 2) { ActPage = (EffPage >> 8) & 0x0f; yOffs = 256; }
		if (TilePage == 3) { ActPage = (EffPage >> 12) & 0x0f; xOffs = 512; yOffs = 256; }
		for (my = 0; my < 32; my++) {
			y = 8 * my;
			y += yOffs;
			for (mx = 0; mx < 64; mx++) {
				TileIndex = (ActPage * 64 * 32) + (my * 64) + mx;
					
				Attr = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
				Priority = (Attr >> 15) & 1;
			
				x = 8 * mx;
				x += xOffs;
			
				Code = Attr & 0x1fff;
				Code = System16TileBanks[Code / System16TileBankSize] * System16TileBankSize + Code % System16TileBankSize;
				Code &= (System16NumTiles - 1);
				Colour = (Attr >> 5) & 0x7f;
			
				ColourOff = System16TilemapColorOffset;
				if (Colour >= 0x20) ColourOff = 0x100 | System16TilemapColorOffset;
				if (Colour >= 0x40) ColourOff = 0x200 | System16TilemapColorOffset;
				if (Colour >= 0x60) ColourOff = 0x300 | System16TilemapColorOffset;
		
				if (Priority == 0) RenderTile_Mask(pSys16FgTileMapPri0, Code, x, y, Colour, 3, 0, ColourOff, System16Tiles);
				if (Priority == 1) RenderTile_Mask(pSys16FgTileMapPri1, Code, x, y, Colour, 3, 0, ColourOff, System16Tiles);
			}
		}
	}
}

static void System16BAltCreateFgAltTileMaps()
{
	INT32 mx, my, Attr, Code, Colour, x, y, TileIndex, Priority, ColourOff, TilePage, xOffs, yOffs;
	UINT16 *VideoRam = (UINT16*)System16TileRam;
	UINT16 EffPage, ActPage;
	
	memset(pSys16FgAltTileMapPri0, 0, 1024 * 512 * sizeof(UINT16));
	memset(pSys16FgAltTileMapPri1, 0, 1024 * 512 * sizeof(UINT16));
		
	EffPage = System16Page[2];
	for (TilePage = 0; TilePage < 4; TilePage++) {
		ActPage = (EffPage >> 0) & 0x0f;
		xOffs = 0;
		yOffs = 0;
		if (TilePage == 1) { ActPage = (EffPage >> 4) & 0x0f; xOffs = 512; }
		if (TilePage == 2) { ActPage = (EffPage >> 8) & 0x0f; yOffs = 256; }
		if (TilePage == 3) { ActPage = (EffPage >> 12) & 0x0f; xOffs = 512; yOffs = 256; }
		for (my = 0; my < 32; my++) {
			y = 8 * my;
			y += yOffs;
			for (mx = 0; mx < 64; mx++) {
				TileIndex = (ActPage * 64 * 32) + (my * 64) + mx;
					
				Attr = BURN_ENDIAN_SWAP_INT16(VideoRam[TileIndex]);
				Priority = (Attr >> 15) & 1;
		
				x = 8 * mx;
				x += xOffs;
			
				Code = Attr & 0x1fff;
				Code = System16TileBanks[Code / System16TileBankSize] * System16TileBankSize + Code % System16TileBankSize;
				Code &= (System16NumTiles - 1);
				Colour = (Attr >> 5) & 0x7f;
			
				ColourOff = System16TilemapColorOffset;
				if (Colour >= 0x20) ColourOff = 0x100 | System16TilemapColorOffset;
				if (Colour >= 0x40) ColourOff = 0x200 | System16TilemapColorOffset;
				if (Colour >= 0x60) ColourOff = 0x300 | System16TilemapColorOffset;
		
				if (Priority == 0) RenderTile_Mask(pSys16FgAltTileMapPri0, Code, x, y, Colour, 3, 0, ColourOff, System16Tiles);
				if (Priority == 1) RenderTile_Mask(pSys16FgAltTileMapPri1, Code, x, y, Colour, 3, 0, ColourOff, System16Tiles);
			}
		}
	}
}

static void System16ACreateTileMaps()
{
	if (System16RecalcBgTileMap) {
		System16ACreateBgTileMaps();
		System16RecalcBgTileMap = 0;
	}
	
	if (System16RecalcFgTileMap) {
		System16ACreateFgTileMaps();
		System16RecalcFgTileMap = 0;
	}
}

static void System16BCreateTileMaps()
{
	UINT16 *TextRam = (UINT16*)System16TextRam;
	INT32 i;
	
	// Check if we need the alt tilemaps
	if (System16RecalcFgAltTileMap) {
		System16RecalcFgAltTileMap = 0;
		for (i = 0xf80/2 + 0 * 0x40/2; i < 0xf80/2 + 0 * 0x40/2 + 224/8; i++) {
			if (BURN_ENDIAN_SWAP_INT16(TextRam[i]) & 0x8000) System16RecalcFgAltTileMap = 1;
		}
	}
	
	if (System16RecalcBgAltTileMap) {
		System16RecalcBgAltTileMap = 0;
		for (i = 0xf80/2 + 1 * 0x40/2; i < 0xf80/2 + 1 * 0x40/2 + 224/8; i++) {
			if (BURN_ENDIAN_SWAP_INT16(TextRam[i]) & 0x8000) System16RecalcBgAltTileMap = 1;
		}
	}

	if (System16RecalcBgTileMap) {
		System16BCreateBgTileMaps();
		System16RecalcBgTileMap = 0;
	}
	
	if (System16RecalcBgAltTileMap) {
		System16BCreateBgAltTileMaps();
		System16RecalcBgAltTileMap = 0;
	}
	
	if (System16RecalcFgTileMap) {
		System16BCreateFgTileMaps();
		System16RecalcFgTileMap = 0;
	}
	
	if (System16RecalcFgAltTileMap) {
		System16BCreateFgAltTileMaps();
		System16RecalcFgAltTileMap = 0;
	}
}

static void System16BAltCreateTileMaps()
{
	UINT16 *TextRam = (UINT16*)System16TextRam;
	INT32 i;
	
	// Check if we need the alt tilemaps
	if (System16RecalcFgAltTileMap) {
		System16RecalcFgAltTileMap = 0;
		for (i = 0xf80/2 + 0 * 0x40/2; i < 0xf80/2 + 0 * 0x40/2 + 224/8; i++) {
			if (BURN_ENDIAN_SWAP_INT16(TextRam[i]) & 0x8000) System16RecalcFgAltTileMap = 1;
		}
	}
	
	if (System16RecalcBgAltTileMap) {
		System16RecalcBgAltTileMap = 0;
		for (i = 0xf80/2 + 1 * 0x40/2; i < 0xf80/2 + 1 * 0x40/2 + 224/8; i++) {
			if (BURN_ENDIAN_SWAP_INT16(TextRam[i]) & 0x8000) System16RecalcBgAltTileMap = 1;
		}
	}

	if (System16RecalcBgTileMap) {
		System16BAltCreateBgTileMaps();
		System16RecalcBgTileMap = 0;
	}
	
	if (System16RecalcBgAltTileMap) {
		System16BAltCreateBgAltTileMaps();
		System16RecalcBgAltTileMap = 0;
	}
	
	if (System16RecalcFgTileMap) {
		System16BAltCreateFgTileMaps();
		System16RecalcFgTileMap = 0;
	}
	
	if (System16RecalcFgAltTileMap) {
		System16BAltCreateFgAltTileMaps();
		System16RecalcFgAltTileMap = 0;
	}
}

void System16ATileWordWrite(UINT32 Offset, UINT16 d)
{
	UINT16 *TileRam = (UINT16*)System16TileRam;
	
	UINT32 FgPage1Addr = (((System16Page[0] >> 0) & 0xf) * (32 * 64)) << 1;
	UINT32 FgPage2Addr = (((System16Page[0] >> 4) & 0xf) * (32 * 64)) << 1;
	UINT32 FgPage3Addr = (((System16Page[0] >> 8) & 0xf) * (32 * 64)) << 1;
	UINT32 FgPage4Addr = (((System16Page[0] >> 12) & 0xf) * (32 * 64)) << 1;
	
	UINT32 BgPage1Addr = (((System16Page[1] >> 0) & 0xf) * (32 * 64)) << 1;
	UINT32 BgPage2Addr = (((System16Page[1] >> 4) & 0xf) * (32 * 64)) << 1;
	UINT32 BgPage3Addr = (((System16Page[1] >> 8) & 0xf) * (32 * 64)) << 1;
	UINT32 BgPage4Addr = (((System16Page[1] >> 12) & 0xf) * (32 * 64)) << 1;
	
	if ((Offset >= FgPage1Addr && Offset <= (FgPage1Addr + 0xfff)) || (Offset >= FgPage2Addr && Offset <= (FgPage2Addr + 0xfff)) || (Offset >= FgPage3Addr && Offset <= (FgPage3Addr + 0xfff)) || (Offset >= FgPage4Addr && Offset <= (FgPage4Addr + 0xfff))) {
		if (BURN_ENDIAN_SWAP_INT16(TileRam[Offset >> 1]) != d) System16RecalcFgTileMap = 1;
	}
	
	if ((Offset >= BgPage1Addr && Offset <= (BgPage1Addr + 0xfff)) || (Offset >= BgPage2Addr && Offset <= (BgPage2Addr + 0xfff)) || (Offset >= BgPage3Addr && Offset <= (BgPage3Addr + 0xfff)) || (Offset >= BgPage4Addr && Offset <= (BgPage4Addr + 0xfff))) {
		if (BURN_ENDIAN_SWAP_INT16(TileRam[Offset >> 1]) != d) System16RecalcBgTileMap = 1;
	}
	
	TileRam[Offset >> 1] = BURN_ENDIAN_SWAP_INT16(d);
}

void System16ATileByteWrite(UINT32 Offset, UINT8 d)
{
	UINT32 FgPage1Addr = (((System16Page[0] >> 0) & 0xf) * (32 * 64)) << 1;
	UINT32 FgPage2Addr = (((System16Page[0] >> 4) & 0xf) * (32 * 64)) << 1;
	UINT32 FgPage3Addr = (((System16Page[0] >> 8) & 0xf) * (32 * 64)) << 1;
	UINT32 FgPage4Addr = (((System16Page[0] >> 12) & 0xf) * (32 * 64)) << 1;
	
	UINT32 BgPage1Addr = (((System16Page[1] >> 0) & 0xf) * (32 * 64)) << 1;
	UINT32 BgPage2Addr = (((System16Page[1] >> 4) & 0xf) * (32 * 64)) << 1;
	UINT32 BgPage3Addr = (((System16Page[1] >> 8) & 0xf) * (32 * 64)) << 1;
	UINT32 BgPage4Addr = (((System16Page[1] >> 12) & 0xf) * (32 * 64)) << 1;
	
	if ((Offset >= FgPage1Addr && Offset <= (FgPage1Addr + 0xfff)) || (Offset >= FgPage2Addr && Offset <= (FgPage2Addr + 0xfff)) || (Offset >= FgPage3Addr && Offset <= (FgPage3Addr + 0xfff)) || (Offset >= FgPage4Addr && Offset <= (FgPage4Addr + 0xfff))) {
		if (System16TileRam[Offset] != d) System16RecalcFgTileMap = 1;
	}
	
	if ((Offset >= BgPage1Addr && Offset <= (BgPage1Addr + 0xfff)) || (Offset >= BgPage2Addr && Offset <= (BgPage2Addr + 0xfff)) || (Offset >= BgPage3Addr && Offset <= (BgPage3Addr + 0xfff)) || (Offset >= BgPage4Addr && Offset <= (BgPage4Addr + 0xfff))) {
		if (System16TileRam[Offset] != d) System16RecalcBgTileMap = 1;
	}
	
	System16TileRam[Offset] = d;
}

void System16BTileWordWrite(UINT32 Offset, UINT16 d)
{
	UINT16 *TileRam = (UINT16*)System16TileRam;
	
	UINT32 FgPage1Addr = (((System16Page[0] >> 0) & 0xf) * (32 * 64)) << 1;
	UINT32 FgPage2Addr = (((System16Page[0] >> 4) & 0xf) * (32 * 64)) << 1;
	UINT32 FgPage3Addr = (((System16Page[0] >> 8) & 0xf) * (32 * 64)) << 1;
	UINT32 FgPage4Addr = (((System16Page[0] >> 12) & 0xf) * (32 * 64)) << 1;
	
	UINT32 BgPage1Addr = (((System16Page[1] >> 0) & 0xf) * (32 * 64)) << 1;
	UINT32 BgPage2Addr = (((System16Page[1] >> 4) & 0xf) * (32 * 64)) << 1;
	UINT32 BgPage3Addr = (((System16Page[1] >> 8) & 0xf) * (32 * 64)) << 1;
	UINT32 BgPage4Addr = (((System16Page[1] >> 12) & 0xf) * (32 * 64)) << 1;
	
	UINT32 FgAltPage1Addr = (((System16Page[2] >> 0) & 0xf) * (32 * 64)) << 1;
	UINT32 FgAltPage2Addr = (((System16Page[2] >> 4) & 0xf) * (32 * 64)) << 1;
	UINT32 FgAltPage3Addr = (((System16Page[2] >> 8) & 0xf) * (32 * 64)) << 1;
	UINT32 FgAltPage4Addr = (((System16Page[2] >> 12) & 0xf) * (32 * 64)) << 1;
	
	UINT32 BgAltPage1Addr = (((System16Page[3] >> 0) & 0xf) * (32 * 64)) << 1;
	UINT32 BgAltPage2Addr = (((System16Page[3] >> 4) & 0xf) * (32 * 64)) << 1;
	UINT32 BgAltPage3Addr = (((System16Page[3] >> 8) & 0xf) * (32 * 64)) << 1;
	UINT32 BgAltPage4Addr = (((System16Page[3] >> 12) & 0xf) * (32 * 64)) << 1;
	
	if ((Offset >= FgPage1Addr && Offset <= (FgPage1Addr + 0xfff)) || (Offset >= FgPage2Addr && Offset <= (FgPage2Addr + 0xfff)) || (Offset >= FgPage3Addr && Offset <= (FgPage3Addr + 0xfff)) || (Offset >= FgPage4Addr && Offset <= (FgPage4Addr + 0xfff))) {
		if (BURN_ENDIAN_SWAP_INT16(TileRam[Offset >> 1]) != d) System16RecalcFgTileMap = 1;
	}
	
	if ((Offset >= BgPage1Addr && Offset <= (BgPage1Addr + 0xfff)) || (Offset >= BgPage2Addr && Offset <= (BgPage2Addr + 0xfff)) || (Offset >= BgPage3Addr && Offset <= (BgPage3Addr + 0xfff)) || (Offset >= BgPage4Addr && Offset <= (BgPage4Addr + 0xfff))) {
		if (BURN_ENDIAN_SWAP_INT16(TileRam[Offset >> 1]) != d) System16RecalcBgTileMap = 1;
	}
	
	if ((Offset >= FgAltPage1Addr && Offset <= (FgAltPage1Addr + 0xfff)) || (Offset >= FgAltPage2Addr && Offset <= (FgAltPage2Addr + 0xfff)) || (Offset >= FgAltPage3Addr && Offset <= (FgAltPage3Addr + 0xfff)) || (Offset >= FgAltPage4Addr && Offset <= (FgAltPage4Addr + 0xfff))) {
		if (BURN_ENDIAN_SWAP_INT16(TileRam[Offset >> 1]) != d) System16RecalcFgAltTileMap = 1;
	}
	
	if ((Offset >= BgAltPage1Addr && Offset <= (BgAltPage1Addr + 0xfff)) || (Offset >= BgAltPage2Addr && Offset <= (BgAltPage2Addr + 0xfff)) || (Offset >= BgAltPage3Addr && Offset <= (BgAltPage3Addr + 0xfff)) || (Offset >= BgAltPage4Addr && Offset <= (BgAltPage4Addr + 0xfff))) {
		if (BURN_ENDIAN_SWAP_INT16(TileRam[Offset >> 1]) != d) System16RecalcBgAltTileMap = 1;
	}	
	
	TileRam[Offset >> 1] = BURN_ENDIAN_SWAP_INT16(d);
}

void System16BTileByteWrite(UINT32 Offset, UINT8 d)
{
	UINT32 FgPage1Addr = (((System16Page[0] >> 0) & 0xf) * (32 * 64)) << 1;
	UINT32 FgPage2Addr = (((System16Page[0] >> 4) & 0xf) * (32 * 64)) << 1;
	UINT32 FgPage3Addr = (((System16Page[0] >> 8) & 0xf) * (32 * 64)) << 1;
	UINT32 FgPage4Addr = (((System16Page[0] >> 12) & 0xf) * (32 * 64)) << 1;
	
	UINT32 BgPage1Addr = (((System16Page[1] >> 0) & 0xf) * (32 * 64)) << 1;
	UINT32 BgPage2Addr = (((System16Page[1] >> 4) & 0xf) * (32 * 64)) << 1;
	UINT32 BgPage3Addr = (((System16Page[1] >> 8) & 0xf) * (32 * 64)) << 1;
	UINT32 BgPage4Addr = (((System16Page[1] >> 12) & 0xf) * (32 * 64)) << 1;
	
	UINT32 FgAltPage1Addr = (((System16Page[2] >> 0) & 0xf) * (32 * 64)) << 1;
	UINT32 FgAltPage2Addr = (((System16Page[2] >> 4) & 0xf) * (32 * 64)) << 1;
	UINT32 FgAltPage3Addr = (((System16Page[2] >> 8) & 0xf) * (32 * 64)) << 1;
	UINT32 FgAltPage4Addr = (((System16Page[2] >> 12) & 0xf) * (32 * 64)) << 1;
	
	UINT32 BgAltPage1Addr = (((System16Page[3] >> 0) & 0xf) * (32 * 64)) << 1;
	UINT32 BgAltPage2Addr = (((System16Page[3] >> 4) & 0xf) * (32 * 64)) << 1;
	UINT32 BgAltPage3Addr = (((System16Page[3] >> 8) & 0xf) * (32 * 64)) << 1;
	UINT32 BgAltPage4Addr = (((System16Page[3] >> 12) & 0xf) * (32 * 64)) << 1;
	
	if ((Offset >= FgPage1Addr && Offset <= (FgPage1Addr + 0xfff)) || (Offset >= FgPage2Addr && Offset <= (FgPage2Addr + 0xfff)) || (Offset >= FgPage3Addr && Offset <= (FgPage3Addr + 0xfff)) || (Offset >= FgPage4Addr && Offset <= (FgPage4Addr + 0xfff))) {
		if (System16TileRam[Offset] != d) System16RecalcFgTileMap = 1;
	}
	
	if ((Offset >= BgPage1Addr && Offset <= (BgPage1Addr + 0xfff)) || (Offset >= BgPage2Addr && Offset <= (BgPage2Addr + 0xfff)) || (Offset >= BgPage3Addr && Offset <= (BgPage3Addr + 0xfff)) || (Offset >= BgPage4Addr && Offset <= (BgPage4Addr + 0xfff))) {
		if (System16TileRam[Offset] != d) System16RecalcBgTileMap = 1;
	}
	
	if ((Offset >= FgAltPage1Addr && Offset <= (FgAltPage1Addr + 0xfff)) || (Offset >= FgAltPage2Addr && Offset <= (FgAltPage2Addr + 0xfff)) || (Offset >= FgAltPage3Addr && Offset <= (FgAltPage3Addr + 0xfff)) || (Offset >= FgAltPage4Addr && Offset <= (FgAltPage4Addr + 0xfff))) {
		if (System16TileRam[Offset] != d) System16RecalcFgAltTileMap = 1;
	}
	
	if ((Offset >= BgAltPage1Addr && Offset <= (BgAltPage1Addr + 0xfff)) || (Offset >= BgAltPage2Addr && Offset <= (BgAltPage2Addr + 0xfff)) || (Offset >= BgAltPage3Addr && Offset <= (BgAltPage3Addr + 0xfff)) || (Offset >= BgAltPage4Addr && Offset <= (BgAltPage4Addr + 0xfff))) {
		if (System16TileRam[Offset] != d) System16RecalcBgAltTileMap = 1;
	}	
	
	System16TileRam[Offset] = d;
}

static void System16ARenderTileLayer(INT32 Page, INT32 PriorityDraw, INT32 Transparent, INT32 Priority)
{
	INT32 xScroll, yScroll, x, y;
	UINT16 *TextRam = (UINT16*)System16TextRam;
	
	xScroll = System16ScrollX[Page];
	yScroll = System16ScrollY[Page];
	UINT16 Pix;
	
	UINT16 *pTileMapSrc = NULL;
	UINT16 *pTileMapDest = NULL;
	INT32 xSrcOff, ySrcOff, RowScrollIndex, xEffScroll, yEffScroll;
	
	if (System16ColScroll && System16RowScroll) {
		for (y = 0; y < nScreenHeight; y++) {
			pTileMapDest = pTransDraw + (y * nScreenWidth);
			UINT8 *pPrio = pPrioDraw + (y * nScreenWidth);

			RowScrollIndex = y / 8;
			if (System16ScreenFlip) RowScrollIndex = (216 - y) / 8;

			xEffScroll = BURN_ENDIAN_SWAP_INT16(TextRam[0xf80/2 + RowScrollIndex * 2 + Page]) & 0x1ff;
			if (System16ScreenFlip) xEffScroll += 17;

			for (x = 0; x < nScreenWidth; x++) {
				yEffScroll = BURN_ENDIAN_SWAP_INT16(TextRam[0xf30/2 + (x/16) * 2 + Page]) & 0xff;

				ySrcOff = (y + yEffScroll) & 0x1ff;
				if (System16ScreenFlip) ySrcOff = ((216 - y) + yEffScroll) & 0x1ff;
				
				if (Page == 0 && PriorityDraw == 0) pTileMapSrc = pSys16FgTileMapPri0 + (ySrcOff * 1024);
				if (Page == 0 && PriorityDraw == 1) pTileMapSrc = pSys16FgTileMapPri1 + (ySrcOff * 1024);
				if (Page == 1 && PriorityDraw == 0 && Transparent == 1) pTileMapSrc = pSys16BgTileMapPri0 + (ySrcOff * 1024);
				if (Page == 1 && PriorityDraw == 1 && Transparent == 1) pTileMapSrc = pSys16BgTileMapPri1 + (ySrcOff * 1024);
				if (Page == 1 && Transparent == 0) pTileMapSrc = pSys16BgTileMapOpaque + (ySrcOff * 1024);
							
				xSrcOff = (x - xEffScroll + 200) & 0x3ff;
				if (System16ScreenFlip) xSrcOff = ((312 - x) - xEffScroll + 200) & 0x3ff;
				
				Pix = pTileMapSrc[xSrcOff];

				if (Transparent) {
					if (Pix) {
						pTileMapDest[x] = Pix;
						pPrio[x] |= Priority;
					}
				} else {
					pTileMapDest[x] = Pix;
					pPrio[x] |= Priority;
				}
			}
		}
	} else {
		if (System16ColScroll) {
			for (y = 0; y < nScreenHeight; y++) {
				pTileMapDest = pTransDraw + (y * nScreenWidth);
				UINT8 *pPrio = pPrioDraw + (y * nScreenWidth);

				xEffScroll = xScroll;
				if (System16ScreenFlip) xEffScroll += 17;

				for (x = 0; x < nScreenWidth; x++) {
					yEffScroll = BURN_ENDIAN_SWAP_INT16(TextRam[0xf30/2 + (x/16) * 2 + Page]) & 0xff;

					ySrcOff = (y + yEffScroll) & 0x1ff;
					if (System16ScreenFlip) ySrcOff = ((216 - y) + yEffScroll) & 0x1ff;
					
					if (Page == 0 && PriorityDraw == 0) pTileMapSrc = pSys16FgTileMapPri0 + (ySrcOff * 1024);
					if (Page == 0 && PriorityDraw == 1) pTileMapSrc = pSys16FgTileMapPri1 + (ySrcOff * 1024);
					if (Page == 1 && PriorityDraw == 0 && Transparent == 1) pTileMapSrc = pSys16BgTileMapPri0 + (ySrcOff * 1024);
					if (Page == 1 && PriorityDraw == 1 && Transparent == 1) pTileMapSrc = pSys16BgTileMapPri1 + (ySrcOff * 1024);
					if (Page == 1 && Transparent == 0) pTileMapSrc = pSys16BgTileMapOpaque + (ySrcOff * 1024);
							
					xSrcOff = (x - xEffScroll + 200) & 0x3ff;
					if (System16ScreenFlip) xSrcOff = ((312 - x) - xEffScroll + 200) & 0x3ff;
					
					Pix = pTileMapSrc[xSrcOff];

					if (Transparent) {
						if (Pix) {
							pTileMapDest[x] = Pix;
							pPrio[x] |= Priority;
						}
					} else {
						pTileMapDest[x] = Pix;
						pPrio[x] |= Priority;
					}
				}
			}
		} else {
			if (System16RowScroll) {
				for (y = 0; y < nScreenHeight; y++) {
					pTileMapDest = pTransDraw + (y * nScreenWidth);
					UINT8 *pPrio = pPrioDraw + (y * nScreenWidth);

					RowScrollIndex = y / 8;
					if (System16ScreenFlip) RowScrollIndex = (216 - y) / 8;
		
					xEffScroll = BURN_ENDIAN_SWAP_INT16(TextRam[0xf80/2 + RowScrollIndex * 2 + Page]) & 0x1ff;
					if (System16ScreenFlip) xEffScroll += 17;
			
					for (x = 0; x < nScreenWidth; x++) {
						yEffScroll = yScroll;
		
						ySrcOff = (y + yEffScroll) & 0x1ff;
						if (System16ScreenFlip) ySrcOff = ((216 - y) + yEffScroll) & 0x1ff;
						
						if (Page == 0 && PriorityDraw == 0) pTileMapSrc = pSys16FgTileMapPri0 + (ySrcOff * 1024);
						if (Page == 0 && PriorityDraw == 1) pTileMapSrc = pSys16FgTileMapPri1 + (ySrcOff * 1024);
						if (Page == 1 && PriorityDraw == 0 && Transparent == 1) pTileMapSrc = pSys16BgTileMapPri0 + (ySrcOff * 1024);
						if (Page == 1 && PriorityDraw == 1 && Transparent == 1) pTileMapSrc = pSys16BgTileMapPri1 + (ySrcOff * 1024);
						if (Page == 1 && Transparent == 0) pTileMapSrc = pSys16BgTileMapOpaque + (ySrcOff * 1024);
							
						xSrcOff = (x - xEffScroll + 200) & 0x3ff;
						if (System16ScreenFlip) xSrcOff = ((312 - x) - xEffScroll + 200) & 0x3ff;
						
						Pix = pTileMapSrc[xSrcOff];

						if (Transparent) {
							if (Pix) {
								pTileMapDest[x] = Pix;
								pPrio[x] |= Priority;
							}
						} else {
							pTileMapDest[x] = Pix;
							pPrio[x] |= Priority;
						}
					}
				}	
			} else {
				for (y = 0; y < nScreenHeight; y++) {
					pTileMapDest = pTransDraw + (y * nScreenWidth);
					UINT8 *pPrio = pPrioDraw + (y * nScreenWidth);

					if (System16ScreenFlip) xScroll += 17;
			
					ySrcOff = (y + yScroll) & 0x1ff;
					if (System16ScreenFlip) ySrcOff = ((216 - y) + yScroll) & 0x1ff;
					
					if (Page == 0 && PriorityDraw == 0) pTileMapSrc = pSys16FgTileMapPri0 + (ySrcOff * 1024);
					if (Page == 0 && PriorityDraw == 1) pTileMapSrc = pSys16FgTileMapPri1 + (ySrcOff * 1024);
					if (Page == 1 && PriorityDraw == 0) pTileMapSrc = pSys16BgTileMapPri0 + (ySrcOff * 1024);
					if (Page == 1 && PriorityDraw == 1) pTileMapSrc = pSys16BgTileMapPri1 + (ySrcOff * 1024);
					if (Page == 1 && Transparent == 0) pTileMapSrc = pSys16BgTileMapOpaque + (ySrcOff * 1024);
		
					for (x = 0; x < nScreenWidth; x++) {	
						xSrcOff = (x - xScroll + 200) & 0x3ff;
						if (System16ScreenFlip) xSrcOff = ((312 - x) - xScroll + 200) & 0x3ff;
						
						Pix = pTileMapSrc[xSrcOff];

						if (Transparent) {
							if (Pix) {
								pTileMapDest[x] = Pix;
								pPrio[x] |= Priority;
							}
						} else {
							pTileMapDest[x] = Pix;
							pPrio[x] |= Priority;
						}
					}
				}
			}
		}
	}
}

static void System16BRenderTileLayer(INT32 Page, INT32 PriorityDraw, INT32 Transparent)
{
	INT32 xScroll, yScroll, x, y;
	UINT16 *TextRam = (UINT16*)System16TextRam;
	
	xScroll = System16ScrollX[Page];
	yScroll = System16ScrollY[Page];
	UINT16 Pix;
	
	UINT16 *pTileMapSrc = NULL;
	UINT16 *pTileMapDest = NULL;
	INT32 xSrcOff, ySrcOff, RowScrollIndex, RowScroll, xEffScroll, yEffScroll;
	
	if (yScroll & 0x8000) {
		for (y = 0; y < nScreenHeight; y++) {
			pTileMapDest = pTransDraw + (y * nScreenWidth);
		
			RowScrollIndex = y / 8;
			if (System16ScreenFlip) RowScrollIndex = (216 - y) / 8;
		
			RowScroll = BURN_ENDIAN_SWAP_INT16(TextRam[0xf80/2 + 0x40/2 * Page + RowScrollIndex]);
			xEffScroll = (xScroll & 0x8000) ? RowScroll : xScroll;

			for (x = 0; x < nScreenWidth; x++) {
				yEffScroll = BURN_ENDIAN_SWAP_INT16(TextRam[0xf16/2 + 0x40/2 * Page + (x+9)/16]);
		
				if (RowScroll & 0x8000) {
					xEffScroll = System16ScrollX[Page + 2];
					yEffScroll = System16ScrollY[Page + 2];
					
					ySrcOff = (y + yEffScroll) & 0x1ff;
					if (System16ScreenFlip) ySrcOff = (((216 + System16ScreenFlipYoffs) - y) + yEffScroll) & 0x1ff;
					
					if (Page == 0 && PriorityDraw == 0) pTileMapSrc = pSys16FgAltTileMapPri0 + (ySrcOff * 1024);
					if (Page == 0 && PriorityDraw == 1) pTileMapSrc = pSys16FgAltTileMapPri1 + (ySrcOff * 1024);
					if (Page == 1 && PriorityDraw == 0) pTileMapSrc = pSys16BgAltTileMapPri0 + (ySrcOff * 1024);
					if (Page == 1 && PriorityDraw == 1) pTileMapSrc = pSys16BgAltTileMapPri1 + (ySrcOff * 1024);
					if (Page == 1 && Transparent == 0) pTileMapSrc = pSys16BgAltTileMapOpaque + (ySrcOff * 1024);
				} else {
					ySrcOff = (y + yEffScroll) & 0x1ff;
					if (System16ScreenFlip) ySrcOff = (((216 + System16ScreenFlipYoffs) - y) + yEffScroll) & 0x1ff;
					
					if (Page == 0 && PriorityDraw == 0) pTileMapSrc = pSys16FgTileMapPri0 + (ySrcOff * 1024);
					if (Page == 0 && PriorityDraw == 1) pTileMapSrc = pSys16FgTileMapPri1 + (ySrcOff * 1024);
					if (Page == 1 && PriorityDraw == 0 && Transparent == 1) pTileMapSrc = pSys16BgTileMapPri0 + (ySrcOff * 1024);
					if (Page == 1 && PriorityDraw == 1 && Transparent == 1) pTileMapSrc = pSys16BgTileMapPri1 + (ySrcOff * 1024);
					if (Page == 1 && Transparent == 0) pTileMapSrc = pSys16BgTileMapOpaque + (ySrcOff * 1024);
				}
							
				xSrcOff = (x - xEffScroll + 192) & 0x3ff;

				if (System16ScreenFlip) xSrcOff = (((312 + System16ScreenFlipXoffs) - x) - xEffScroll + 192) & 0x3ff;
				
				Pix = pTileMapSrc[xSrcOff];

				if (Transparent) {
					if (Pix) pTileMapDest[x] = Pix;
				} else {
					pTileMapDest[x] = Pix;
				}
			}
		}
	} else {
		for (y = 0; y < nScreenHeight; y++) {
			pTileMapDest = pTransDraw + (y * nScreenWidth);
		
			RowScrollIndex = y / 8;
			if (System16ScreenFlip) RowScrollIndex = (216 - y) / 8;
		
			RowScroll = BURN_ENDIAN_SWAP_INT16(TextRam[0xf80/2 + 0x40/2 * Page + RowScrollIndex]);
			xEffScroll = (xScroll & 0x8000) ? RowScroll : xScroll;
			yEffScroll = yScroll;
		
			if (RowScroll & 0x8000) {
				xEffScroll = System16ScrollX[Page + 2];
				yEffScroll = System16ScrollY[Page + 2];
				
				ySrcOff = (y + yEffScroll) & 0x1ff;
				if (System16ScreenFlip) ySrcOff = (((216 + System16ScreenFlipYoffs) - y) + yEffScroll) & 0x1ff;
				
				if (Page == 0 && PriorityDraw == 0) pTileMapSrc = pSys16FgAltTileMapPri0 + (ySrcOff * 1024);
				if (Page == 0 && PriorityDraw == 1) pTileMapSrc = pSys16FgAltTileMapPri1 + (ySrcOff * 1024);
				if (Page == 1 && PriorityDraw == 0) pTileMapSrc = pSys16BgAltTileMapPri0 + (ySrcOff * 1024);
				if (Page == 1 && PriorityDraw == 1) pTileMapSrc = pSys16BgAltTileMapPri1 + (ySrcOff * 1024);
				if (Page == 1 && Transparent == 0) pTileMapSrc = pSys16BgAltTileMapOpaque + (ySrcOff * 1024);
			} else {
				ySrcOff = (y + yEffScroll) & 0x1ff;
				if (System16ScreenFlip) ySrcOff = (((216 + System16ScreenFlipYoffs) - y) + yEffScroll) & 0x1ff;
				
				if (Page == 0 && PriorityDraw == 0) pTileMapSrc = pSys16FgTileMapPri0 + (ySrcOff * 1024);
				if (Page == 0 && PriorityDraw == 1) pTileMapSrc = pSys16FgTileMapPri1 + (ySrcOff * 1024);
				if (Page == 1 && PriorityDraw == 0) pTileMapSrc = pSys16BgTileMapPri0 + (ySrcOff * 1024);
				if (Page == 1 && PriorityDraw == 1) pTileMapSrc = pSys16BgTileMapPri1 + (ySrcOff * 1024);
				if (Page == 1 && Transparent == 0) pTileMapSrc = pSys16BgTileMapOpaque + (ySrcOff * 1024);
			}
		
			for (x = 0; x < nScreenWidth; x++) {
				xSrcOff = (x - xEffScroll + 192) & 0x3ff;

				if (System16ScreenFlip) xSrcOff = (((312 + System16ScreenFlipXoffs) - x) - xEffScroll + 192) & 0x3ff;
				
				Pix = pTileMapSrc[xSrcOff];

				if (Transparent) {
					if (Pix) pTileMapDest[x] = Pix;
				} else {
					pTileMapDest[x] = Pix;
				}
			}
		}
	}
}

static void BootlegRenderTileLayer(INT32 Page, INT32 PriorityDraw, INT32 Transparent)
{
	INT32 mx, my, Code, Colour, x, y, Priority, TileIndex, ColourOff;
	
	UINT16 EffPage, ActPage = 0, xScroll, yScroll;
	
	EffPage = System16Page[Page];
	xScroll = System16ScrollX[Page];
	yScroll = System16ScrollY[Page];
	
	INT32 yStartPosArray[28 * 21];
	
	float yPos = float(yScroll % 8);
	INT32 yStartPos = INT32(yPos) * -1;

	if (yScroll & 0x8000) {
		for (y = 0; y < 224; y += 8) {
			for (x = -8; x < 320; x += 16) {
				INT32 yColScrollTemp = (System16TextRam[0xf16 + (0x40 * Page) +  (2 * (x + 8) / 16) + 1] << 8) | System16TextRam[0xf16 + (0x40 * Page) +  (2 * (x + 8) / 16) + 0];
				float yTempPos = float(yColScrollTemp % 8);
				INT32 yTempStartPos = INT32(yTempPos) * -1;
				yStartPosArray[(y / 8) + (28 * ((x + 8) / 16))] = yTempStartPos + y;
			}
		}
		
		for (INT32 ny = 0; ny < 28; ny++) {
			INT32 RowScrollIndex = ny;
			
			INT32 RowScroll = (System16TextRam[0xf80 + (0x40 * Page) +  (2 * RowScrollIndex) + 1] << 8) | System16TextRam[0xf80 + (0x40 * Page) +  (2 * RowScrollIndex) + 0];
			
			INT32 xScrollRow = ((xScroll & 0x8000) ? RowScroll : xScroll);
			
			INT32 yScrollCol;
			
			if (RowScroll & 0x8000) {
				EffPage = System16Page[Page + 2];
				xScrollRow = System16ScrollX[Page + 2];
				yScrollCol = System16ScrollY[Page + 2];
			}
				
			float xPos = float(xScrollRow % 8);
			INT32 xStartPos = (INT32)xPos;
			
			if (xStartPos == 1) xStartPos = -7;
			if (xStartPos == 2) xStartPos = -6;
			if (xStartPos == 3) xStartPos = -5;
			if (xStartPos == 4) xStartPos = -4;
			if (xStartPos == 5) xStartPos = -3;
			if (xStartPos == 6) xStartPos = -2;
			if (xStartPos == 7) xStartPos = -1;
			if (xStartPos == 0) xStartPos = 0;
			
			for (x = -8; x < 320; x += 16) {
				INT32 x2 = xStartPos + x;
				
				yScrollCol = (System16TextRam[0xf16 + (0x40 * Page) +  (2 * (x + 8) / 16) + 1] << 8) | System16TextRam[0xf16 + (0x40 * Page) +  (2 * (x + 8) / 16) + 0];
				
				my = ((ny * 8) + (yScrollCol & 0x1ff)) / 8;
		
				if (my > 63) my -= 64;
	
				y = yStartPosArray[ny + (28 * ((x + 8) / 16))];
					
				mx = (x2 - (xScrollRow & 0x3ff) + 192) / 8;
				if (mx < 0) mx += 128;
				if (Page == 1) {
					if (my < 32 && mx < 64) ActPage = BootlegBgPage[0];
					if (my < 32 && mx >= 64) ActPage = BootlegBgPage[1];
					if (my >= 32 && mx < 64) ActPage = BootlegBgPage[2];
					if (my >= 32 && mx >= 64) ActPage = BootlegBgPage[3];
				}
				if (Page == 0) {
					if (my < 32 && mx < 64) ActPage = BootlegFgPage[0];
					if (my < 32 && mx >= 64) ActPage = BootlegFgPage[1];
					if (my >= 32 && mx < 64) ActPage = BootlegFgPage[2];
					if (my >= 32 && mx >= 64) ActPage = BootlegFgPage[3];
				}
				
				TileIndex = 64 * 32 * 2 * ActPage + ((2 * 64 * my) & 0xfff) + ((2 * mx) & 0x7f);
				INT32 Data = (System16TileRam[TileIndex + 1] << 8) | System16TileRam[TileIndex + 0];
				INT32 Data2 = (System16TileRam[TileIndex + 3] << 8) | System16TileRam[TileIndex + 2];
				
				Priority = (Data >> 15) & 1;
				
				if (Priority == PriorityDraw) {
					Code = Data & 0x1fff;
					Code = System16TileBanks[Code / System16TileBankSize] * System16TileBankSize + Code % System16TileBankSize;
					Code &= (System16NumTiles - 1);
					Colour = (Data >> 6) & 0x7f;
			
					ColourOff = System16TilemapColorOffset;
					if (Colour >= 0x20) ColourOff = 0x100 | System16TilemapColorOffset;
					if (Colour >= 0x40) ColourOff = 0x200 | System16TilemapColorOffset;
					if (Colour >= 0x60) ColourOff = 0x300 | System16TilemapColorOffset;
			
					if (Transparent) {
						if (x2 > 7 && x2 < 312 && y > 7 && y < 216) {
							Render8x8Tile_Mask(pTransDraw, Code, x2, y, Colour, 3, 0, ColourOff, System16Tiles);
						} else {
							Render8x8Tile_Mask_Clip(pTransDraw, Code, x2, y, Colour, 3, 0, ColourOff, System16Tiles);
						}
					} else {
						if (x2 > 7 && x2 < 312 && y > 7 && y < 216) {
							Render8x8Tile(pTransDraw, Code, x2, y, Colour, 3, ColourOff, System16Tiles);
						} else {
							Render8x8Tile_Clip(pTransDraw, Code, x2, y, Colour, 3, ColourOff, System16Tiles);
						}
					}
					
					Code = Data2 & 0x1fff;
					Code = System16TileBanks[Code / System16TileBankSize] * System16TileBankSize + Code % System16TileBankSize;
					Code &= (System16NumTiles - 1);
					Colour = (Data2 >> 6) & 0x7f;
			
					ColourOff = System16TilemapColorOffset;
					if (Colour >= 0x20) ColourOff = 0x100 | System16TilemapColorOffset;
					if (Colour >= 0x40) ColourOff = 0x200 | System16TilemapColorOffset;
					if (Colour >= 0x60) ColourOff = 0x300 | System16TilemapColorOffset;
					
					INT32 x1 = x2 + 8;
					
					if (Transparent) {
						if (x1 > 7 && x1 < 312 && y > 7 && y < 216) {
							Render8x8Tile_Mask(pTransDraw, Code, x1, y, Colour, 3, 0, ColourOff, System16Tiles);
						} else {
							Render8x8Tile_Mask_Clip(pTransDraw, Code, x1, y, Colour, 3, 0, ColourOff, System16Tiles);
						}
					} else {
						if (x1 > 7 && x1 < 312 && y > 7 && y < 216) {
							Render8x8Tile(pTransDraw, Code, x1, y, Colour, 3, ColourOff, System16Tiles);
						} else {
							Render8x8Tile_Clip(pTransDraw, Code, x1, y, Colour, 3, ColourOff, System16Tiles);
						}
					}
				}
			}
		}		
	} else {
		for (y = yStartPos; y < 224; y += 8) {
			INT32 RowScrollIndex = y / 8;
			INT32 RowScroll = (System16TextRam[0xf80 + (0x40 * Page) +  (2 * RowScrollIndex) + 1] << 8) | System16TextRam[0xf80 + (0x40 * Page) +  (2 * RowScrollIndex) + 0];
		
			INT32 xScrollRow = ((xScroll & 0x8000) ? RowScroll : xScroll);
		
			if (RowScroll & 0x8000) {
				EffPage = System16Page[Page + 2];
				xScrollRow = System16ScrollX[Page + 2];
				yScroll = System16ScrollY[Page + 2];
			}
		
			float xPos = float(xScrollRow % 8);
			INT32 xStartPos = (INT32)xPos;
		
			if (xStartPos == 1) xStartPos = -7;
			if (xStartPos == 2) xStartPos = -6;
			if (xStartPos == 3) xStartPos = -5;
			if (xStartPos == 4) xStartPos = -4;
			if (xStartPos == 5) xStartPos = -3;
			if (xStartPos == 6) xStartPos = -2;
			if (xStartPos == 7) xStartPos = -1;
			
			my = (y + (yScroll & 0x1ff)) / 8;
			
			if (my > 63) my -= 64;
		
			for (x = xStartPos; x < 320; x += 8) {
				mx = (x - (xScrollRow & 0x3ff) + 192) / 8;
				if (mx < 0) mx += 128;
				if (Page == 1) {
					if (my < 32 && mx < 64) ActPage = BootlegBgPage[0];
					if (my < 32 && mx >= 64) ActPage = BootlegBgPage[1];
					if (my >= 32 && mx < 64) ActPage = BootlegBgPage[2];
					if (my >= 32 && mx >= 64) ActPage = BootlegBgPage[3];
				}
				if (Page == 0) {
					if (my < 32 && mx < 64) ActPage = BootlegFgPage[0];
					if (my < 32 && mx >= 64) ActPage = BootlegFgPage[1];
					if (my >= 32 && mx < 64) ActPage = BootlegFgPage[2];
					if (my >= 32 && mx >= 64) ActPage = BootlegFgPage[3];
				}
				
				TileIndex = 64 * 32 * 2 * ActPage + ((2 * 64 * my) & 0xfff) + ((2 * mx) & 0x7f);
				INT32 Data = (System16TileRam[TileIndex + 1] << 8) | System16TileRam[TileIndex + 0];
			
				Priority = (Data >> 15) & 1;
			
				if (Priority == PriorityDraw) {
					Code = Data & 0x1fff;
					Code = System16TileBanks[Code / System16TileBankSize] * System16TileBankSize + Code % System16TileBankSize;
					Code &= (System16NumTiles - 1);
					Colour = (Data >> 6) & 0x7f;
			
					ColourOff = System16TilemapColorOffset;
					if (Colour >= 0x20) ColourOff = 0x100 | System16TilemapColorOffset;
					if (Colour >= 0x40) ColourOff = 0x200 | System16TilemapColorOffset;
					if (Colour >= 0x60) ColourOff = 0x300 | System16TilemapColorOffset;
			
					if (Transparent) {
						if (x > 7 && x < 312 && y > 7 && y < 216) {
							Render8x8Tile_Mask(pTransDraw, Code, x, y, Colour, 3, 0, ColourOff, System16Tiles);
						} else {
							Render8x8Tile_Mask_Clip(pTransDraw, Code, x, y, Colour, 3, 0, ColourOff, System16Tiles);
						}
					} else {
						if (x > 7 && x < 312 && y > 7 && y < 216) {
							Render8x8Tile(pTransDraw, Code, x, y, Colour, 3, ColourOff, System16Tiles);
						} else {
							Render8x8Tile_Clip(pTransDraw, Code, x, y, Colour, 3, ColourOff, System16Tiles);
						}
					}
				}
			}
		}
	}
}

static void System16ARenderTextLayer(INT32 PriorityDraw, INT32 tpri)
{
	INT32 mx, my, Code, Colour, x, y, Priority, TileIndex = 0;

	for (my = 0; my < 32; my++) {
		for (mx = 0; mx < 64; mx++) { 
			Code = (System16TextRam[TileIndex + 1] << 8) | System16TextRam[TileIndex + 0];
			
			Priority = (Code >> 11) & 1;
			
			if (Priority == PriorityDraw) {
				Colour = (Code >> 8) & 0x07;
				Code &= 0xff;
			
				x = 8 * mx;
				y = 8 * my;
				
				x -= 192;
				
				if (System16ScreenFlip) {
					x = 312 - x;
					y = 216 - y;

					if (x > 7 && x < 312 && y > 7 && y < 216) {
						Render8x8Tile_Prio_Mask_FlipXY(pTransDraw, Code, x, y, Colour, 3, 0, System16TilemapColorOffset, tpri, System16Tiles);
					} else {
						Render8x8Tile_Prio_Mask_FlipXY_Clip(pTransDraw, Code, x, y, Colour, 3, 0, System16TilemapColorOffset, tpri, System16Tiles);
					}
				} else {
					if (x > 7 && x < 312 && y > 7 && y < 216) {
						Render8x8Tile_Prio_Mask(pTransDraw, Code, x, y, Colour, 3, 0, System16TilemapColorOffset, tpri, System16Tiles);
					} else {
						Render8x8Tile_Prio_Mask_Clip(pTransDraw, Code, x, y, Colour, 3, 0, System16TilemapColorOffset, tpri, System16Tiles);
					}
				}
			}

			TileIndex += 2;
		}
	}
}

static void System16BRenderTextLayer(INT32 PriorityDraw)
{
	INT32 mx, my, Code, Colour, x, y, Priority, TileIndex = 0;
	
	INT32 ColourDepth = 3;
	if (Lockonph) ColourDepth = 4;

	for (my = 0; my < 32; my++) {
		for (mx = 0; mx < 64; mx++) {
			Code = (System16TextRam[TileIndex + 1] << 8) | System16TextRam[TileIndex + 0];
			Priority = (Code >> 15) & 1;
			
			if (Priority == PriorityDraw) {			
				Colour = (Code >> 9) & 0x07;
				Code &= 0x1ff;
			
				Code += System16TileBanks[0] * System16TileBankSize;
				
				Code &= (System16NumTiles - 1);
			
				x = 8 * mx;
				y = 8 * my;
			
				x -= 192;

				if (System16ScreenFlip) {
					x = 312 - x;
					y = 216 - y;
					
					if (x > 7 && x < 312 && y > 7 && y < 216) {
						Render8x8Tile_Mask_FlipXY(pTransDraw, Code, x, y, Colour, ColourDepth, 0, System16TilemapColorOffset, System16Tiles);
					} else {
						Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, Code, x, y, Colour, ColourDepth, 0, System16TilemapColorOffset, System16Tiles);
					}
				} else {
					if (x > 7 && x < 312 && y > 7 && y < 216) {
						Render8x8Tile_Mask(pTransDraw, Code, x, y, Colour, ColourDepth, 0, System16TilemapColorOffset, System16Tiles);
					} else {
						Render8x8Tile_Mask_Clip(pTransDraw, Code, x, y, Colour, ColourDepth, 0, System16TilemapColorOffset, System16Tiles);
					}
				}
			}

			TileIndex += 2;
		}
	}
}

static void System16BAltRenderTextLayer(INT32 PriorityDraw)
{
	INT32 mx, my, Code, Colour, x, y, Priority, TileIndex = 0;

	for (my = 0; my < 32; my++) {
		for (mx = 0; mx < 64; mx++) {
			Code = (System16TextRam[TileIndex + 1] << 8) | System16TextRam[TileIndex + 0];
			Priority = (Code >> 15) & 1;
			
			if (Priority == PriorityDraw) {			
				Colour = (Code >> 8) & 0x07;
				Code &= 0xff;
			
				Code += System16TileBanks[0] * System16TileBankSize;
				
				Code &= (System16NumTiles - 1);
			
				x = 8 * mx;
				y = 8 * my;
			
				x -= 192;
				
				if (System16ScreenFlip) {
					x = 312 - x;
					y = 216 - y;
					
					if (x > 7 && x < 312 && y > 7 && y < 216) {
						Render8x8Tile_Mask_FlipXY(pTransDraw, Code, x, y, Colour, 3, 0, System16TilemapColorOffset, System16Tiles);
					} else {
						Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, Code, x, y, Colour, 3, 0, System16TilemapColorOffset, System16Tiles);
					}
				} else {
					if (x > 7 && x < 312 && y > 7 && y < 216) {
						Render8x8Tile_Mask(pTransDraw, Code, x, y, Colour, 3, 0, System16TilemapColorOffset, System16Tiles);
					} else {
						Render8x8Tile_Mask_Clip(pTransDraw, Code, x, y, Colour, 3, 0, System16TilemapColorOffset, System16Tiles);
					}
				}
			}

			TileIndex +=2 ;
		}
	}
}

/*====================================================
Sprite Rendering
====================================================*/

inline static void System16ADrawPixel(INT32 x, INT32 pix, INT32 colour, UINT16* pPixel)
{
	x += System16SpriteXOffset;

	if (x >= 0 && x <= 319 && pix != 0 && pix != 15) {
		pPixel[x] = pix | colour;
	}
}

static void System16ARenderSpriteLayer()
{
	UINT8 numbanks = System16SpriteRomSize / 0x10000;

	const UINT16 *spritebase = (const UINT16*)System16Sprites;
	UINT16 *data;

#if 0
	// debug - dink
	INT32 pos = 0;

	UINT16 *sf = (UINT16*)System16SpriteRam;
	sf += 0x10;
	extern int counter;

	if (counter) {
		bprintf(0, _T("--- spr start \n"));
		bprintf(0, _T("%04x  %04x  %04x  %04x  %04x\n\n"), sf[0], sf[1], sf[2], sf[3], sf[4]);
	}
#endif

	for (data = (UINT16*)System16SpriteRam; data < (UINT16*)System16SpriteRam + System16SpriteRamSize / 2; data += 8/*,pos+=8 dinkdebug*/) {
		if ((BURN_ENDIAN_SWAP_INT16(data[0]) >> 8) > 0xf0) break;
		
		INT32 bottom  = (BURN_ENDIAN_SWAP_INT16(data[0]) >> 8) + 1;
		INT32 top     = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0xff) + 1;
		INT32 xpos    = (BURN_ENDIAN_SWAP_INT16(data[1]) & 0x1ff) - 0xbd;
		INT32 pitch   = (INT16)BURN_ENDIAN_SWAP_INT16(data[2]);
		UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[3]);
		INT32 color   = ((BURN_ENDIAN_SWAP_INT16(data[4]) >> 4) & 0x3f0) | ((BURN_ENDIAN_SWAP_INT16(data[4]) & 3) << 10);
		INT32 bank    = System16SpriteBanks[(BURN_ENDIAN_SWAP_INT16(data[4]) >> 4) & 0x7];
		const UINT16 *spritedata;
		INT32 x, y, pix, xdelta = 1;
		
		/* initialize the end address to the start address */
		//data[7] = addr;
		UINT16 d7 = addr; // data[7] cache

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if ((top >= bottom) || bank == 255) {
			data[7] = BURN_ENDIAN_SWAP_INT16(d7); // un-cache d7
			continue;
		}

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x8000 * bank;

#if 0
		if (counter) {
			bprintf(0, _T("%x  %x  %x   pri %x  pos %x\n"), addr, bank, color, color >> 10, pos);
		}
#endif
		if (System16ScreenFlip) {
			INT32 temp = top;
			top = 224 - bottom;
			bottom = 224 - temp;
			xpos = 320 - xpos;
			xdelta = -1;
		}
		
		for (y = top; y < bottom; y++) {
			addr += pitch;
			if (y >= 0 && y <= 223) {
				UINT16* pPixel = pSpriteBitmap + (y * 320);
				if (!(addr & 0x8000)) {
					/* start at the word before because we preincrement below */
					//data[7] = addr - 1;
					d7 = addr - 1;
					for (x = xpos; ((xpos - x) & 0x1ff) != 1; ) {
						UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[++d7 & 0x7fff]);
				
						pix = (pixels >> 12) & 0xf; System16ADrawPixel(x, pix, color, pPixel); x += xdelta;
						pix = (pixels >>  8) & 0xf; System16ADrawPixel(x, pix, color, pPixel); x += xdelta;
						pix = (pixels >>  4) & 0xf; System16ADrawPixel(x, pix, color, pPixel); x += xdelta;
						pix = (pixels >>  0) & 0xf; System16ADrawPixel(x, pix, color, pPixel); x += xdelta;
				
						if (pix == 15) break;
					}
				} else {
					//data[7] = addr + 1;
					d7 = addr + 1;
					for (x = xpos; ((xpos - x) & 0x1ff) != 1; ) {
						UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[--d7 & 0x7fff]);

						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; System16ADrawPixel(x, pix, color, pPixel); x += xdelta;
						pix = (pixels >>  4) & 0xf; System16ADrawPixel(x, pix, color, pPixel); x += xdelta;
						pix = (pixels >>  8) & 0xf; System16ADrawPixel(x, pix, color, pPixel); x += xdelta;
						pix = (pixels >> 12) & 0xf; System16ADrawPixel(x, pix, color, pPixel); x += xdelta;

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15) break;
					}
				}
			}
		}
		data[7] = BURN_ENDIAN_SWAP_INT16(d7); // un-cache d7
	}
}

inline static void System16BDrawPixel(INT32 x, INT32 pix, INT32 colour, UINT16* pPixel, UINT16* PalRAM)
{
	x += System16SpriteXOffset;
	if (x >= 0 && x <= 319 && pix != 0 && pix != 15) {
		if (colour == (0x3f << 4)) {
			pPixel[x] &= (System16PaletteEntries - 1);
			pPixel[x] += (BURN_ENDIAN_SWAP_INT16(PalRAM[pPixel[x]]) & 0x8000) ? (System16PaletteEntries * 2) : System16PaletteEntries;
		} else {
			pPixel[x] = (pix | colour | System16SpritePalOffset) & (System16PaletteEntries - 1);
		}
	}
}

static void System16BRenderSpriteLayer(INT32 Priority)
{
	UINT8 numbanks;
	const UINT16 *spritebase;
  	UINT16 *data;
	
	UINT16 *PalRAM = (UINT16*)System16PaletteRam;

	spritebase = (const UINT16 *)System16Sprites;
	numbanks = System16SpriteRomSize / 0x20000;

	for (data = (UINT16*)System16SpriteRam; data < (UINT16*)System16SpriteRam + System16SpriteRamSize / 2; data += 8) {
		if (BURN_ENDIAN_SWAP_INT16(data[2]) & 0x8000) break;
		INT32 sprpri  = 1 << ((BURN_ENDIAN_SWAP_INT16(data[4]) >> 6) & 0x3);

		if (Priority && sprpri != Priority) continue;
		
		INT32 bottom  = BURN_ENDIAN_SWAP_INT16(data[0]) >> 8;
		INT32 top     = BURN_ENDIAN_SWAP_INT16(data[0]) & 0xff;
		INT32 xpos    = (BURN_ENDIAN_SWAP_INT16(data[1]) & 0x1ff) - 0xb8;
		INT32 hide    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x4000;
		INT32 flip    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x100;
		INT32 pitch   = (INT8)(BURN_ENDIAN_SWAP_INT16(data[2]) & 0xff);
		UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[3]);
		INT32 bank    = System16SpriteBanks[(BURN_ENDIAN_SWAP_INT16(data[4]) >> 8) & 0xf];
		INT32 color   = ((BURN_ENDIAN_SWAP_INT16(data[4]) & 0x3f) << 4);
		INT32 vzoom   = (BURN_ENDIAN_SWAP_INT16(data[5]) >> 5) & 0x1f;
		INT32 hzoom   = BURN_ENDIAN_SWAP_INT16(data[5]) & 0x1f;
		const UINT16 *spritedata;
		INT32 x, y, pix, xdelta = 1;

		/* initialize the end address to the start address */
		//data[7] = addr;
		UINT16 d7 = addr; // data[7] cache

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (hide || (top >= bottom) || bank == 255) {
			data[7] = BURN_ENDIAN_SWAP_INT16(d7); // un-cache d7
			continue;
		}

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x10000 * bank;

		/* reset the yzoom counter */
		data[5] &= BURN_ENDIAN_SWAP_INT16(0x03ff);
		
		if (System16ScreenFlip) {
			INT32 temp = top;
			top = 224 - bottom;
			bottom = 224 - temp;
			xpos = 320 - xpos;
			xdelta = -1;
		}
		
		/* loop from top to bottom */
		for (y = top; y < bottom; y++) {
			/* advance a row */
			addr += pitch;

			/* accumulate zoom factors; if we carry into the high bit, skip an extra row */
			data[5] += BURN_ENDIAN_SWAP_INT16(vzoom << 10);
			if (data[5] & BURN_ENDIAN_SWAP_INT16(0x8000)) {
				addr += pitch;
				data[5] &= BURN_ENDIAN_SWAP_INT16(~0x8000);
			}
			
			/* skip drawing if not within the cliprect */
			if (y >= 0 && y <= 223) {
				UINT16* pPixel = pTransDraw + (y * 320);
				int xacc;

				/* compute the initial X zoom accumulator; this is verified on the real PCB */
				xacc = 4 * hzoom;

				/* non-flipped case */
				if (!flip) {
					/* start at the word before because we preincrement below */
					//data[7] = addr - 1;
					d7 = addr - 1;
					for (x = xpos; ((xpos - x) & 0x1ff) != 1; ) {
						UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[++d7]);

						/* draw four pixels */
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System16BDrawPixel(x, pix, color, pPixel, PalRAM); x += xdelta; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System16BDrawPixel(x, pix, color, pPixel, PalRAM); x += xdelta; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System16BDrawPixel(x, pix, color, pPixel, PalRAM); x += xdelta; }
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System16BDrawPixel(x, pix, color, pPixel, PalRAM); x += xdelta; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15) break;
					}
				} else {
					/* start at the word after because we predecrement below */
					//data[7] = addr + 1;
					d7 = addr + 1;
					for (x = xpos; ((xpos - x) & 0x1ff) != 1; ) {
						UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[--d7]);

						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System16BDrawPixel(x, pix, color, pPixel, PalRAM); x += xdelta; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System16BDrawPixel(x, pix, color, pPixel, PalRAM); x += xdelta; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System16BDrawPixel(x, pix, color, pPixel, PalRAM); x += xdelta; }
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System16BDrawPixel(x, pix, color, pPixel, PalRAM); x += xdelta; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15) break;
					}
				}
			}
		}
	}
}

inline static void OutrunDrawPixel(INT32 x, INT32 pix, INT32 colour, INT32 shadow, UINT16* pPixel, UINT16* PalRAM)
{
	if (x >= 0 && x <= 319 && pix != 0 && pix != 15) {
		if (shadow && pix == 0xa) {
			pPixel[x] &= (System16PaletteEntries - 1);
			pPixel[x] += (BURN_ENDIAN_SWAP_INT16(PalRAM[pPixel[x]]) & 0x8000) ? (System16PaletteEntries * 2) : System16PaletteEntries;
		} else {
			pPixel[x] = (pix | colour | 0x800) & (System16PaletteEntries - 1);
		}
	}
}

static void OutrunRenderSpriteLayer(INT32 Priority)
{
	UINT8 numbanks = System16SpriteRomSize / 0x40000;
	const UINT32 *spritebase = (const UINT32 *)System16Sprites;
	UINT16 *data;
	
	UINT16 *PalRAM = (UINT16*)System16PaletteRam;

	for (data = (UINT16*)System16SpriteRamBuff; data < (UINT16*)System16SpriteRamBuff + System16SpriteRamSize / 2; data += 8) {
		if (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x8000) break;
		INT32 sprpri  = 1 << ((BURN_ENDIAN_SWAP_INT16(data[3]) >> 12) & 3);
				
		if (sprpri != Priority) continue;
		
		INT32 hide    = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x5000);
		INT32 bank    = (BURN_ENDIAN_SWAP_INT16(data[0]) >> 9) & 7;
		INT32 top     = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x1ff) - 0x100;
		UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[1]);
		INT32 pitch   = (INT16)((BURN_ENDIAN_SWAP_INT16(data[2]) >> 1) | ((BURN_ENDIAN_SWAP_INT16(data[4]) & 0x1000) << 3)) >> 8;
		INT32 xpos    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x1ff;
		INT32 shadow  = (BURN_ENDIAN_SWAP_INT16(data[3]) >> 14) & 1;
		INT32 vzoom   = BURN_ENDIAN_SWAP_INT16(data[3]) & 0x7ff;
		INT32 ydelta  = (BURN_ENDIAN_SWAP_INT16(data[4]) & 0x8000) ? 1 : -1;
		INT32 flip    = (~(BURN_ENDIAN_SWAP_INT16(data[4])) >> 14) & 1;
		INT32 xdelta  = (BURN_ENDIAN_SWAP_INT16(data[4]) & 0x2000) ? 1 : -1;
		INT32 hzoom   = BURN_ENDIAN_SWAP_INT16(data[4]) & 0x7ff;
		INT32 height  = (BURN_ENDIAN_SWAP_INT16(data[5]) >> 8) + 1;
		INT32 color   = (BURN_ENDIAN_SWAP_INT16(data[5]) & 0x7f) << 4;
		INT32 x, y, ytarget, yacc = 0, pix;
		const UINT32 *spritedata;

		/* adjust X coordinate */
		/* note: the threshhold below is a guess. If it is too high, rachero will draw garbage */
		/* If it is too low, smgp won't draw the bottom part of the road */
		if (xpos < 0x80 && xdelta < 0)
			xpos += 0x200;
		xpos -= 0xbe;

		/* initialize the end address to the start address */
		//data[7] = BURN_ENDIAN_SWAP_INT16(addr);
		UINT16 d7 = addr; // data[7] cache

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (hide || height == 0) {
			data[7] = BURN_ENDIAN_SWAP_INT16(d7); // un-cache d7
			continue;
		}

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x10000 * bank;

		/* clamp to a maximum of 8x (not 100% confirmed) */
		if (vzoom < 0x40) vzoom = 0x40;
		if (hzoom < 0x40) hzoom = 0x40;

		/* loop from top to bottom */
		ytarget = top + ydelta * height;
		for (y = top; y != ytarget; y += ydelta)
		{
			/* skip drawing if not within the cliprect */
			if (y >= 0 && y <= 223) {
				UINT16* pPixel = pTransDraw + (y * 320);
				INT32 xacc = 0;

				/* non-flipped case */
				if (!flip)
				{
					/* start at the word before because we preincrement below */
					//data[7] = addr - 1;
					d7 = addr - 1;
					for (x = xpos; (xdelta > 0 && x <= 319) || (xdelta < 0 && x >= 0); )
					{
						UINT32 pixels = BURN_ENDIAN_SWAP_INT32(spritedata[++d7]);

						/* draw four pixels */
						pix = (pixels >> 28) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 24) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 20) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 16) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 12) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  8) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  4) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  0) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;

						/* stop if the second-to-last pixel in the group was 0xf */
						if ((pixels & 0x000000f0) == 0x000000f0)
							break;
					}
				}

				/* flipped case */
				else
				{
					/* start at the word after because we predecrement below */
					//data[7] = addr + 1;
					d7 = addr + 1;
					for (x = xpos; (xdelta > 0 && x <= 319) || (xdelta < 0 && x >= 0); )
					{
						UINT32 pixels = BURN_ENDIAN_SWAP_INT32(spritedata[--d7]);

						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  4) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  8) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 12) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 16) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 20) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 24) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 28) & 0xf; while (xacc < 0x200) { OutrunDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;

						/* stop if the second-to-last pixel in the group was 0xf */
						if ((pixels & 0x0f000000) == 0x0f000000)
							break;
					}
				}
			}

			/* accumulate zoom factors; if we carry into the high bit, skip an extra row */
			yacc += vzoom;
			addr += pitch * (yacc >> 9);
			yacc &= 0x1ff;
		}
	}
}

inline static void HangonDrawPixel(INT32 x, INT32 pix, INT32 colour, INT32 shadow, UINT16* pPixel, UINT16* PalRAM)
{
	if (x >= 0 && x <= 319 && pix != 0 && pix != 15) {
		if (shadow && pix == 0xa) {
			pPixel[x] &= (System16PaletteEntries - 1);
			pPixel[x] += (BURN_ENDIAN_SWAP_INT16(PalRAM[pPixel[x]]) & 0x8000) ? (System16PaletteEntries * 2) : System16PaletteEntries;
		} else {
			pPixel[x] = (pix | colour | 0x400) & (System16PaletteEntries - 1);
		}
	}
}

static void HangonRenderSpriteLayer(INT32 Priority)
{
	UINT8 numbanks = System16SpriteRomSize / 0x20000;
	const UINT32 *spritebase = (UINT32*)System16Sprites;
	const UINT8 *zoom = System16Prom;
	UINT16 *data;
	
	UINT16 *PalRAM = (UINT16*)System16PaletteRam;

	for (data = (UINT16*)System16SpriteRam; data < (UINT16*)System16SpriteRam + System16SpriteRamSize / 2; data += 8) {
		if ((BURN_ENDIAN_SWAP_INT16(data[0]) >> 8) > 0xf0) break;	
		INT32 sprpri  = ((BURN_ENDIAN_SWAP_INT16(data[2]) >> 14) & 1) ? (1<<3) : (1<<1);
		
		if (Priority && sprpri != Priority) continue;
		
		INT32 bottom  = (BURN_ENDIAN_SWAP_INT16(data[0]) >> 8) + 1;
		INT32 top     = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0xff) + 1;
		INT32 bank    = System16SpriteBanks[(BURN_ENDIAN_SWAP_INT16(data[1]) >> 12) & 0x7];
		INT32 xpos    = (BURN_ENDIAN_SWAP_INT16(data[1]) & 0x1ff) - 0xbd;
		INT32 shadow  = (~(BURN_ENDIAN_SWAP_INT16(data[2])) >> 15) & 1;
		INT32 color   = (((BURN_ENDIAN_SWAP_INT16(data[2]) >> 8) & 0x3f) << 4);
		INT32 pitch   = (INT16)(BURN_ENDIAN_SWAP_INT16(data[2]) << 9) >> 9;
		UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[3]);
		INT32 hzoom   = ((BURN_ENDIAN_SWAP_INT16(data[4]) >> 8) & 0x3f) << 1;
		INT32 vzoom   = (BURN_ENDIAN_SWAP_INT16(data[4]) >> 0) & 0x3f;
		INT32 x, y, pix, zaddr, zmask;
		const UINT32 *spritedata;
		
		/* initialize the end address to the start address */
		//data[7] = addr;
		UINT16 d7 = addr; // data[7] cache

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if ((top >= bottom) || bank == 255) {
			data[7] = BURN_ENDIAN_SWAP_INT16(d7); // un-cache d7
			continue;
		}

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x8000 * bank;
		
		/* determine the starting zoom address and mask */
		zaddr = (vzoom & 0x38) << 5;
		zmask = 1 << (vzoom & 7);
		
		for (y = top; y < bottom; y++) {
			addr += pitch;
			
			/* if the zoom bit says so, add pitch a second time */
			if (zoom[zaddr++] & zmask)
				addr += pitch;
				
			if (y >= 0 && y <= 223) {
				UINT16* pPixel = pTransDraw + (y * 320);
				INT32 xacc = 0x00;
				
				if (!(addr & 0x8000)) {
					/* start at the word before because we preincrement below */
					//data[7] = addr - 1;
					d7 = addr - 1;
					for (x = xpos; x <= 319; ) {
						UINT32 pixels = BURN_ENDIAN_SWAP_INT32(spritedata[++d7 & 0x7fff]);
					
						pix = (pixels >> 28) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >> 24) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >> 20) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >> 16) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
					
						if (pix == 15) break;
					}
				} else {
					//data[7] = addr + 1;
					d7 = addr + 1;
					for (x = xpos; x <= 319; ) {
						UINT32 pixels = BURN_ENDIAN_SWAP_INT32(spritedata[--d7 & 0x7fff]);

						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >> 16) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >> 20) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >> 24) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }
						pix = (pixels >> 28) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x++; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15) break;
					}
				}
			}
		}
	}
}

inline static void HangonAltDrawPixel(INT32 x, INT32 pix, INT32 colour, UINT16* pPixel)
{
	if (x >= 0 && x <= 319 && pix != 0 && pix != 15) {
		if (colour == (0x3f << 4)) {
			pPixel[x] &= (System16PaletteEntries - 1);
			pPixel[x] += ((System16SpriteShadow) ? System16PaletteEntries * 2 : System16PaletteEntries);
		} else {
			pPixel[x] = (pix | colour | 0x400) & (System16PaletteEntries - 1);
		}
	}
}

static void HangonAltRenderSpriteLayer(INT32 Priority)
{
	UINT8 numbanks = System16SpriteRomSize / 0x10000;
	const UINT16 *spritebase = (UINT16*)System16Sprites;
	const UINT8 *zoom = System16Prom;
	UINT16 *data;
	
	for (data = (UINT16*)System16SpriteRam; data < (UINT16*)System16SpriteRam + System16SpriteRamSize / 2; data += 8) {
		if ((BURN_ENDIAN_SWAP_INT16(data[0]) >> 8) > 0xf0) break;
		INT32 sprpri  = 1 << ((BURN_ENDIAN_SWAP_INT16(data[4]) >> 0) & 0x3);
		
		if (Priority && sprpri != Priority) continue;
		
		INT32 bottom  = (BURN_ENDIAN_SWAP_INT16(data[0]) >> 8) + 1;
		INT32 top     = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0xff) + 1;
		INT32 bank    = System16SpriteBanks[(BURN_ENDIAN_SWAP_INT16(data[1]) >> 12) & 0xf];
		INT32 xpos    = (BURN_ENDIAN_SWAP_INT16(data[1]) & 0x1ff) - 0xbd;
		INT32 pitch   = (INT16)BURN_ENDIAN_SWAP_INT16(data[2]);
		UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[3]);
		INT32 color   = (((BURN_ENDIAN_SWAP_INT16(data[4]) >> 8) & 0x3f) << 4);
		INT32 vzoom   = (BURN_ENDIAN_SWAP_INT16(data[4]) >> 2) & 0x3f;
		INT32 hzoom   = vzoom << 1;
		
		INT32 x, y, pix, zaddr, zmask;
		const UINT16 *spritedata;

		/* initialize the end address to the start address */
		//data[7] = addr;
		UINT16 d7 = addr; // data[7] cache

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if ((top >= bottom) || bank == 255) {
			data[7] = BURN_ENDIAN_SWAP_INT16(d7); // un-cache d7
			continue;
		}

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x8000 * bank;

		/* determine the starting zoom address and mask */
		zaddr = (vzoom & 0x38) << 5;
		zmask = 1 << (vzoom & 7);

		/* loop from top to bottom */
		for (y = top; y < bottom; y++)
		{
			/* advance a row */
			addr += pitch;

			/* if the zoom bit says so, add pitch a second time */
			if (zoom[zaddr++] & zmask)
				addr += pitch;

			/* skip drawing if not within the cliprect */
			if (y >= 0 && y <= 223)
			{
				UINT16* pPixel = pTransDraw + (y * 320);
				INT32 xacc = 0x00;

				/* note that the System 16A sprites have a design flaw that allows the address */
				/* to carry into the flip flag, which is the topmost bit -- it is very important */
				/* to emulate this as the games compensate for it */

				/* non-flipped case */
				if (!(addr & 0x8000))
				{
					/* start at the word before because we preincrement below */
					//data[7] = addr - 1;
					d7 = addr - 1;
					for (x = xpos; x <= 319; )
					{
						UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[++d7 & 0x7fff]);

						/* draw four pixels */
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonAltDrawPixel(x, pix, color, pPixel); x++; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonAltDrawPixel(x, pix, color, pPixel); x++; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonAltDrawPixel(x, pix, color, pPixel); x++; }
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonAltDrawPixel(x, pix, color, pPixel); x++; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15)
							break;
					}
				}

				/* flipped case */
				else
				{
					/* start at the word after because we predecrement below */
					//data[7] = addr + 1;
					d7 = addr + 1;
					for (x = xpos; x <= 319; )
					{
						UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[--d7 & 0x7fff]);

						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonAltDrawPixel(x, pix, color, pPixel); x++; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonAltDrawPixel(x, pix, color, pPixel); x++; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonAltDrawPixel(x, pix, color, pPixel); x++; }
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0xff) + hzoom; if (xacc < 0x100) { if (x >= 0) HangonAltDrawPixel(x, pix, color, pPixel); x++; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15)
							break;
					}
				}
			}
		}
	}
}

inline static void BoardXDrawPixel(INT32 x, INT32 pix, INT32 colour, INT32 shadow, UINT16* pPixel, UINT16* PalRAM)
{
	if (x >= 0 && x <= 319 && pix != 0 && pix != 15) {
		if (shadow && pix == 0xa) {
			pPixel[x] &= (System16PaletteEntries - 1);
			pPixel[x] += (BURN_ENDIAN_SWAP_INT16(PalRAM[pPixel[x]]) & 0x8000) ? (System16PaletteEntries * 2) : System16PaletteEntries;
		} else {
			pPixel[x] = (pix | colour) & (System16PaletteEntries - 1);
		}
	}
}

static void XBoardRenderSpriteLayer(INT32 Priority)
{
	UINT8 numbanks = System16SpriteRomSize / 0x40000;
	const UINT32 *spritebase = (const UINT32 *)System16Sprites;
	UINT16 *data;
	
	UINT16 *PalRAM = (UINT16*)System16PaletteRam;

	for (data = (UINT16*)System16SpriteRamBuff; data < (UINT16*)System16SpriteRamBuff + System16SpriteRamSize / 2; data += 8) {
		if (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x8000) break;
		INT32 sprpri  = 1 << ((BURN_ENDIAN_SWAP_INT16(data[3]) >> 12) & 3);
		
		if (sprpri != Priority) continue;
		
		INT32 hide    = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x5000);
		INT32 bank    = (BURN_ENDIAN_SWAP_INT16(data[0]) >> 9) & 7;
		INT32 top     = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x1ff) - 0x100;
		UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[1]);
		INT32 pitch   = (INT16)((BURN_ENDIAN_SWAP_INT16(data[2]) >> 1) | ((BURN_ENDIAN_SWAP_INT16(data[4]) & 0x1000) << 3)) >> 8;
		INT32 xpos    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x1ff;
		INT32 shadow  = (BURN_ENDIAN_SWAP_INT16(data[3]) >> 14) & 1;
		INT32 vzoom   = BURN_ENDIAN_SWAP_INT16(data[3]) & 0x7ff;
		INT32 ydelta  = (BURN_ENDIAN_SWAP_INT16(data[4]) & 0x8000) ? 1 : -1;
		INT32 flip    = (~(BURN_ENDIAN_SWAP_INT16(data[4])) >> 14) & 1;
		INT32 xdelta  = (BURN_ENDIAN_SWAP_INT16(data[4]) & 0x2000) ? 1 : -1;
		INT32 hzoom   = BURN_ENDIAN_SWAP_INT16(data[4]) & 0x7ff;
		INT32 height  = (BURN_ENDIAN_SWAP_INT16(data[5]) & 0xfff) + 1;
		INT32 color   = (BURN_ENDIAN_SWAP_INT16(data[6]) & 0xff) << 4;
		INT32 x, y, ytarget, yacc = 0, pix;
		const UINT32 *spritedata;

		/* adjust X coordinate */
		/* note: the threshhold below is a guess. If it is too high, rachero will draw garbage */
		/* If it is too low, smgp won't draw the bottom part of the road */
		if (xpos < 0x80 && xdelta < 0)
			xpos += 0x200;
		xpos -= 0xbe;

		/* initialize the end address to the start address */
		//data[7] = addr;
		UINT16 d7 = addr; // data[7] cache

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (hide || height == 0) {
			data[7] = BURN_ENDIAN_SWAP_INT16(d7); // un-cache d7
			continue;
		}

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x10000 * bank;

		/* clamp to a maximum of 8x (not 100% confirmed) */
		if (vzoom < 0x40) vzoom = 0x40;
		if (hzoom < 0x40) hzoom = 0x40;

		/* loop from top to bottom */
		ytarget = top + ydelta * height;
		for (y = top; y != ytarget; y += ydelta)
		{
			/* skip drawing if not within the cliprect */
			if (y >= 0 && y <= 223) {
				UINT16* pPixel = pTransDraw + (y * 320);
				INT32 xacc = 0;

				/* non-flipped case */
				if (!flip)
				{
					/* start at the word before because we preincrement below */
					//data[7] = addr - 1;
					d7 = addr - 1;
					for (x = xpos; (xdelta > 0 && x <= 319) || (xdelta < 0 && x >= 0); )
					{
						UINT32 pixels = BURN_ENDIAN_SWAP_INT32(spritedata[++d7]);
						
						/* draw four pixels */
						pix = (pixels >> 28) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 24) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 20) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 16) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 12) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  8) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  4) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  0) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;

						/* stop if the second-to-last pixel in the group was 0xf */
						if ((pixels & 0x000000f0) == 0x000000f0)
							break;
					}
				}

				/* flipped case */
				else
				{
					/* start at the word after because we predecrement below */
					//data[7] = addr + 1;
					d7 = addr + 1;
					for (x = xpos; (xdelta > 0 && x <= 319) || (xdelta < 0 && x >= 0); )
					{
						UINT32 pixels = BURN_ENDIAN_SWAP_INT32(spritedata[--d7]);
						
						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  4) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >>  8) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 12) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 16) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 20) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 24) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;
						pix = (pixels >> 28) & 0xf; while (xacc < 0x200) { BoardXDrawPixel(x, pix, color, shadow, pPixel, PalRAM); x += xdelta; xacc += hzoom; } xacc -= 0x200;

						/* stop if the second-to-last pixel in the group was 0xf */
						if ((pixels & 0x0f000000) == 0x0f000000)
							break;
					}
				}
			}

			/* accumulate zoom factors; if we carry into the high bit, skip an extra row */
			yacc += vzoom;
			addr += pitch * (yacc >> 9);
			yacc &= 0x1ff;
		}
	}
}

inline static void YBoardSystem16BDrawPixel(INT32 x, INT32 pix, INT32 Priority, INT32 colour, UINT16* pPixel, UINT8* pPriorityMap, UINT16* PalRAM)
{
	if (x >= 0 && x <= 319 && pix != 0 && pix != 15) {
		UINT16 Pixel = pPixel[x];
		if (Pixel != 0xffff) {
			if (Priority < (pPriorityMap[x] & 0x1f)) {
				if (pix == 14) {
					pPixel[x] &= (System16PaletteEntries - 1);
					pPixel[x] += (BURN_ENDIAN_SWAP_INT16(PalRAM[pPixel[x]]) & 0x8000) ? (System16PaletteEntries * 2) : System16PaletteEntries;
				} else {
					pPixel[x] = (pix | colour | 0x800) & (System16PaletteEntries - 1);
				}
			}
		}
	}
}

static void YBoardSystem16BRenderSpriteLayer()
{
	UINT8 numbanks;
	const UINT16 *spritebase;
  	UINT16 *data;

	spritebase = (const UINT16 *)System16Sprites;
	numbanks = System16SpriteRomSize / 0x20000;
	
	UINT16 *PalRAM = (UINT16*)System16PaletteRam;

	for (data = (UINT16*)System16SpriteRam; data < (UINT16*)System16SpriteRam + System16SpriteRamSize / 2; data += 8) {
		if (BURN_ENDIAN_SWAP_INT16(data[2]) & 0x8000) break;
		INT32 sprpri  = (BURN_ENDIAN_SWAP_INT16(data[1]) >> 8) & 0x1e;
				
		INT32 bottom  = BURN_ENDIAN_SWAP_INT16(data[0]) >> 8;
		INT32 top     = BURN_ENDIAN_SWAP_INT16(data[0]) & 0xff;
		INT32 xpos    = (BURN_ENDIAN_SWAP_INT16(data[1]) & 0x1ff) - 0xb8;
		INT32 hide    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x4000;
		INT32 flip    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x100;
		INT32 pitch   = (INT8)(BURN_ENDIAN_SWAP_INT16(data[2]) & 0xff);
		UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[3]);
		INT32 bank    = System16SpriteBanks[(BURN_ENDIAN_SWAP_INT16(data[4]) >> 8) & 0xf];
		INT32 color   = ((BURN_ENDIAN_SWAP_INT16(data[4]) & 0x7f) << 4);
		INT32 vzoom   = (BURN_ENDIAN_SWAP_INT16(data[5]) >> 5) & 0x1f;
		INT32 hzoom   = BURN_ENDIAN_SWAP_INT16(data[5]) & 0x1f;
		const UINT16 *spritedata;
		INT32 x, y, pix, xdelta = 1;
		
		/* initialize the end address to the start address */
		//data[7] = BURN_ENDIAN_SWAP_INT16(addr);
		UINT16 d7 = addr;

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (hide || (top >= bottom) || bank == 255) {
			data[7] = BURN_ENDIAN_SWAP_INT16(d7); // un-cache d7
			continue;
		}

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x10000 * bank;

		/* reset the yzoom counter */
		data[5] &= BURN_ENDIAN_SWAP_INT16(0x03ff);

		/* loop from top to bottom */
		for (y = top; y < bottom; y++) {
			/* advance a row */
			addr += pitch;

			/* accumulate zoom factors; if we carry into the high bit, skip an extra row */
			data[5] += BURN_ENDIAN_SWAP_INT16(vzoom << 10);
			if (data[5] & BURN_ENDIAN_SWAP_INT16(0x8000)) {
				addr += pitch;
				data[5] &= BURN_ENDIAN_SWAP_INT16(~0x8000);
			}

			/* skip drawing if not within the cliprect */
			if (y >= 0 && y <= 223) {
				UINT16* pPixel = pTransDraw + (y * 320);
				UINT8* pPrio = System16PriorityMap + (y * 320);
				INT32 xacc;

				/* compute the initial X zoom accumulator; this is verified on the real PCB */
				xacc = 4 * hzoom;

				/* non-flipped case */
				if (!flip) {
					/* start at the word before because we preincrement below */
					//data[7] = addr - 1;
					d7 = addr - 1;
					for (x = xpos; ((xpos - x) & 0x1ff) != 1; ) {
						UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[++d7]);

						/* draw four pixels */
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { YBoardSystem16BDrawPixel(x, pix, sprpri, color, pPixel, pPrio, PalRAM); x += xdelta; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { YBoardSystem16BDrawPixel(x, pix, sprpri, color, pPixel, pPrio, PalRAM); x += xdelta; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { YBoardSystem16BDrawPixel(x, pix, sprpri, color, pPixel, pPrio, PalRAM); x += xdelta; }
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { YBoardSystem16BDrawPixel(x, pix, sprpri, color, pPixel, pPrio, PalRAM); x += xdelta; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15) break;
					}
				} else {
					/* start at the word after because we predecrement below */
					//data[7] = addr + 1;
					d7 = addr + 1;
					for (x = xpos; ((xpos - x) & 0x1ff) != 1; ) {
						UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[--d7]);

						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { YBoardSystem16BDrawPixel(x, pix, sprpri, color, pPixel, pPrio, PalRAM); x += xdelta; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { YBoardSystem16BDrawPixel(x, pix, sprpri, color, pPixel, pPrio, PalRAM); x += xdelta; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { YBoardSystem16BDrawPixel(x, pix, sprpri, color, pPixel, pPrio, PalRAM); x += xdelta; }
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { YBoardSystem16BDrawPixel(x, pix, sprpri, color, pPixel, pPrio, PalRAM); x += xdelta; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15) break;
					}
				}
			}
		}
	}
}

// High resolution version of YBoardSystem16BRenderSpriteLayer() for
// System16ResScale > 1 (HUD, sky and backdrop sprites).
//
// The native zoom drops source pixels/rows: horizontally, after q source
// pixels a row has drawn N(q) = ceil((q * (64 - hzoom) - c) / 64) pixels,
// with c = (4 * hzoom) & 0x3f (the hardware's initial accumulator), and
// vertically screen row r shows source row (r + 1) * (32 + vzoom) / 32 past
// addr. Here both are evaluated at S times finer resolution: N_S(q) uses
// S * (64 - hzoom) and S * c, and scaled row R shows source row
// (R + S) * (32 + vzoom) / (32 * S). With S = 1 these are exactly the native
// formulas; with zoom 0 every source pixel becomes an S x S block. Where a
// row ends, and the zoom counter written back to sprite RAM (data[5]), are
// computed exactly as the native renderer does.

inline static void YBoardSystem16BDrawPixelScaled(INT32 x, INT32 w, INT32 pix, INT32 Priority, INT32 colour, UINT16* pPixel, UINT8* pPriorityMap, UINT16* PalRAM)
{
	if (x >= 0 && x < w && pix != 0 && pix != 15) {
		UINT16 Pixel = pPixel[x];
		if (Pixel != 0xffff) {
			if (Priority < (pPriorityMap[x] & 0x1f)) {
				if (pix == 14) {
					pPixel[x] &= (System16PaletteEntries - 1);
					pPixel[x] += (BURN_ENDIAN_SWAP_INT16(PalRAM[pPixel[x]]) & 0x8000) ? (System16PaletteEntries * 2) : System16PaletteEntries;
				} else {
					pPixel[x] = (pix | colour | 0x800) & (System16PaletteEntries - 1);
				}
			}
		}
	}
}

struct YBoard16BRowState {
	INT32 S, w;					// scale, scaled screen width
	INT32 xpos, hzoom, c;		// native start x, horizontal zoom, initial accumulator
	INT32 xacc, x;				// native accumulator and x (decide where the row ends)
	INT32 q, drawn;				// source pixels consumed, scaled pixels drawn
	INT32 sprpri, color;
	UINT16 *pPixel, *PalRAM;
	UINT8 *pPrio;
};

inline static void YBoard16BScaledPixel(YBoard16BRowState &r, INT32 pix)
{
	r.xacc = (r.xacc & 0x3f) + r.hzoom;
	if (r.xacc < 0x40) r.x++;
	r.q++;

	const INT32 target = (r.q * r.S * (64 - r.hzoom) - r.c * r.S + 63) >> 6;	// ceil(N_S(q))
	while (r.drawn < target) {
		YBoardSystem16BDrawPixelScaled(r.xpos * r.S + r.drawn, r.w, pix, r.sprpri, r.color, r.pPixel, r.pPrio, r.PalRAM);
		r.drawn++;
	}
}

static void YBoardSystem16BRenderSpriteLayerScaled()
{
	const INT32 S = System16ResScale;
	const INT32 w = 320 * S;
	const INT32 h = 224 * S;
	UINT8 numbanks;
	const UINT16 *spritebase;
	UINT16 *data;

	spritebase = (const UINT16 *)System16Sprites;
	numbanks = System16SpriteRomSize / 0x20000;

	UINT16 *PalRAM = (UINT16*)System16PaletteRam;

	for (data = (UINT16*)System16SpriteRam; data < (UINT16*)System16SpriteRam + System16SpriteRamSize / 2; data += 8) {
		if (BURN_ENDIAN_SWAP_INT16(data[2]) & 0x8000) break;
		INT32 sprpri  = (BURN_ENDIAN_SWAP_INT16(data[1]) >> 8) & 0x1e;

		INT32 bottom  = BURN_ENDIAN_SWAP_INT16(data[0]) >> 8;
		INT32 top     = BURN_ENDIAN_SWAP_INT16(data[0]) & 0xff;
		INT32 xpos    = (BURN_ENDIAN_SWAP_INT16(data[1]) & 0x1ff) - 0xb8;
		INT32 hide    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x4000;
		INT32 flip    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x100;
		INT32 pitch   = (INT8)(BURN_ENDIAN_SWAP_INT16(data[2]) & 0xff);
		UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[3]);
		INT32 bank    = System16SpriteBanks[(BURN_ENDIAN_SWAP_INT16(data[4]) >> 8) & 0xf];
		INT32 color   = ((BURN_ENDIAN_SWAP_INT16(data[4]) & 0x7f) << 4);
		INT32 vzoom   = (BURN_ENDIAN_SWAP_INT16(data[5]) >> 5) & 0x1f;
		INT32 hzoom   = BURN_ENDIAN_SWAP_INT16(data[5]) & 0x1f;
		const UINT16 *spritedata;

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (hide || (top >= bottom) || bank == 255) {
			data[7] = BURN_ENDIAN_SWAP_INT16(addr); // un-cache d7
			continue;
		}

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x10000 * bank;

		/* the Y zoom counter in sprite RAM, updated exactly as the native renderer does */
		data[5] &= BURN_ENDIAN_SWAP_INT16(0x03ff);
		for (INT32 y = top; y < bottom; y++) {
			data[5] += BURN_ENDIAN_SWAP_INT16(vzoom << 10);
			if (data[5] & BURN_ENDIAN_SWAP_INT16(0x8000)) {
				data[5] &= BURN_ENDIAN_SWAP_INT16(~0x8000);
			}
		}

		YBoard16BRowState r;
		r.S = S; r.w = w; r.xpos = xpos; r.hzoom = hzoom; r.c = (4 * hzoom) & 0x3f;
		r.sprpri = sprpri; r.color = color; r.PalRAM = PalRAM;

		/* The last native row (bottom - top - 1) shows source row
		   (bottom - top) * (32 + vzoom) / 32, the last the hardware ever reads.
		   The finer rows below could step past it, into data that isn't part of
		   this sprite, so they are clamped to it. */
		const INT32 lastsrcrow = ((bottom - top) * (32 + vzoom)) / 32;

		/* loop over scaled rows from top to bottom */
		for (INT32 y = top * S; y < bottom * S; y++) {
			/* skip drawing if not within the cliprect */
			if (y < 0 || y >= h) continue;

			/* source row for this scaled row (see the comment above) */
			INT32 srcrow = ((y - top * S + S) * (32 + vzoom)) / (32 * S);
			if (srcrow > lastsrcrow) srcrow = lastsrcrow;
			UINT16 rowaddr = addr + pitch * srcrow;

			r.pPixel = pTransDraw + y * w;
			r.pPrio = System16PriorityMap + y * w;
			r.xacc = 4 * hzoom; /* initial X zoom accumulator; this is verified on the real PCB */
			r.x = xpos;
			r.q = 0;
			r.drawn = 0;

			INT32 pix = 0;
			UINT16 d7;

			/* non-flipped case */
			if (!flip) {
				/* start at the word before because we preincrement below */
				d7 = rowaddr - 1;
				while (((xpos - r.x) & 0x1ff) != 1) {
					UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[++d7]);

					/* draw four pixels */
					pix = (pixels >> 12) & 0xf; YBoard16BScaledPixel(r, pix);
					pix = (pixels >>  8) & 0xf; YBoard16BScaledPixel(r, pix);
					pix = (pixels >>  4) & 0xf; YBoard16BScaledPixel(r, pix);
					pix = (pixels >>  0) & 0xf; YBoard16BScaledPixel(r, pix);

					/* stop if the last pixel in the group was 0xf */
					if (pix == 15) break;
				}
			} else {
				/* start at the word after because we predecrement below */
				d7 = rowaddr + 1;
				while (((xpos - r.x) & 0x1ff) != 1) {
					UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[--d7]);

					/* draw four pixels */
					pix = (pixels >>  0) & 0xf; YBoard16BScaledPixel(r, pix);
					pix = (pixels >>  4) & 0xf; YBoard16BScaledPixel(r, pix);
					pix = (pixels >>  8) & 0xf; YBoard16BScaledPixel(r, pix);
					pix = (pixels >> 12) & 0xf; YBoard16BScaledPixel(r, pix);

					/* stop if the last pixel in the group was 0xf */
					if (pix == 15) break;
				}
			}
		}
	}
}

inline static void YBoardDrawPixel(INT32 x, UINT32 ind, UINT32 colourpri, UINT16* pPixel)
{
	if (x >= 0 && x <= 511 && ind < 0x1fe) {
		pPixel[x] = (ind | colourpri)/* & (System16PaletteEntries - 1)*/;
	}
}

static void YBoardRenderSpriteLayer()
{
	UINT8 numbanks = System16Sprite2RomSize / 0x80000;
	const UINT64 *spritebase = (const UINT64 *)System16Sprites2;
	const UINT16 *rotatebase = (UINT16*)System16RotateRamBuff;
	UINT8 visited[0x1000];
	UINT16 *data;
	INT32 next = 0;

	/* reset the visited list */
	memset(visited, 0, sizeof(visited));

	for (INT32 y = 0; y <= 511; y++)
		if (!(BURN_ENDIAN_SWAP_INT16(rotatebase[y & ~1]) & 0xc000))
			memset(pTempDraw + (y * 512), 0xff, 512 * sizeof(UINT16));

	for (data = (UINT16*)System16SpriteRam2; !(BURN_ENDIAN_SWAP_INT16(data[0]) & 0x8000) && !visited[next]; data = (UINT16*)System16SpriteRam2 + next * 8)
	{
		INT32 hide    = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x5000);
		UINT16 *indirect = (UINT16*)System16SpriteRam2 + ((BURN_ENDIAN_SWAP_INT16(data[0]) & 0x7ff) << 4);
		INT32 bank    = ((BURN_ENDIAN_SWAP_INT16(data[1]) >> 8) & 0x10) | ((BURN_ENDIAN_SWAP_INT16(data[2]) >> 12) & 0x0f);
		INT32 xpos    = (BURN_ENDIAN_SWAP_INT16(data[1]) & 0xfff) - 0x600;
		INT32 top     = (BURN_ENDIAN_SWAP_INT16(data[2]) & 0xfff) - 0x600;
		UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[3]);
		INT32 height  = BURN_ENDIAN_SWAP_INT16(data[4]);
		INT32 ydelta  = (BURN_ENDIAN_SWAP_INT16(data[5]) & 0x4000) ? 1 : -1;
		INT32 flip    = (~(BURN_ENDIAN_SWAP_INT16(data[5])) >> 13) & 1;
		INT32 xdelta  = (BURN_ENDIAN_SWAP_INT16(data[5]) & 0x1000) ? 1 : -1;
		INT32 zoom    = BURN_ENDIAN_SWAP_INT16(data[5]) & 0x7ff;
		INT32 colorpri= (BURN_ENDIAN_SWAP_INT16(data[6]) << 1) & 0xfe00;
		INT32 pitch   = (INT8)BURN_ENDIAN_SWAP_INT16(data[6]);
		INT32 x, y, ytarget, yacc = 0, pix, ind;
		const UINT64 *spritedata;
		UINT16 offs;

		/* note that we've visited this entry and get the offset of the next one */
		visited[next] = 1;
		next = BURN_ENDIAN_SWAP_INT16(data[7]) & 0xfff;
		
		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (hide || height == 0)
			continue;

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x10000 * bank;

		/* clamp to a maximum of 8x (not 100% confirmed) */
		if (zoom == 0) zoom = 1;
		
		/* loop from top to bottom */
		ytarget = top + ydelta * height;
		for (y = top; y != ytarget; y += ydelta)
		{
			/* skip drawing if not within the cliprect */
			if (y >= 0 && y <= 511)
			{
				//UINT16* pPixel = pTransDraw + (y * 320);
				UINT16* pPixel = pTempDraw + (y * 512);
				INT32 minx = BURN_ENDIAN_SWAP_INT16(rotatebase[y & ~1]);
				INT32 maxx = BURN_ENDIAN_SWAP_INT16(rotatebase[y |  1]);
				INT32 xacc = 0;

				/* bit 0x8000 from rotate RAM means that Y is above the top of the screen */
				if ((minx & 0x8000) && ydelta < 0)
					break;

				/* bit 0x4000 from rotate RAM means that Y is below the bottom of the screen */
				if ((minx & 0x4000) && ydelta > 0)
					break;

				/* if either bit is set, skip the rest for this scanline */
				if (!(minx & 0xc000))
				{
					/* clamp min/max to the cliprect */
					minx -= 0x600;
					maxx -= 0x600;
					if (minx < 0)
						minx = 0;
					if (maxx > 511)
						maxx = 511;

					/* non-flipped case */
					if (!flip)
					{
						/* start at the word before because we preincrement below */
						offs = addr - 1;
						for (x = xpos; (xdelta > 0 && x <= maxx) || (xdelta < 0 && x >= minx); )
						{
							UINT64 pixels = spritedata[++offs];
#ifndef LSB_FIRST
							pixels = BURN_ENDIAN_SWAP_INT64(pixels);
#endif

							/* draw four pixels */
							pix = (pixels >> 60) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 56) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 52) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 48) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 44) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 40) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 36) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 32) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 28) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 24) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 20) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 16) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 12) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >>  8) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >>  4) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >>  0) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							
							/* stop if the second-to-last pixel in the group was 0xf */
							if (pix == 0x0f)
								break;
						}
					}

					/* flipped case */
					else
					{
						/* start at the word after because we predecrement below */
						offs = addr + 1;
						for (x = xpos; (xdelta > 0 && x <= maxx) || (xdelta < 0 && x >= minx); )
						{
							UINT64 pixels = spritedata[--offs];
#ifndef LSB_FIRST
							pixels = BURN_ENDIAN_SWAP_INT64(pixels);
#endif

							/* draw four pixels */
							pix = (pixels >>  0) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >>  4) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >>  8) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 12) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 16) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 20) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 24) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 28) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 32) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 36) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 40) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 44) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 48) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 52) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 56) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							pix = (pixels >> 60) & 0xf; ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]); while (xacc < 0x200) { YBoardDrawPixel(x, ind, colorpri, pPixel); x += xdelta; xacc += zoom; } xacc -= 0x200;
							
							/* stop if the second-to-last pixel in the group was 0xf */
							if (pix == 0x0f)
								break;
						}
					}
				}
			}

			/* accumulate zoom factors; if we carry into the high bit, skip an extra row */
			yacc += zoom;
			addr += pitch * (yacc >> 9);
			yacc &= 0x1ff;
		}
	}
}

// ---------------------------------------------------------------------------
// Level-of-detail art for the high resolution renderer.
//
// Distant objects are drawn by the game from smaller copies of their art. In
// sprite ROM each smaller copy is stored immediately after its larger image,
// as an exact 2:1 point sample of it: half the rows and half the row width
// (rounded up to whole 64-bit words, the reduction centred in the row), and
// every row has a 0xf marker nibble at both ends. Wide images are often made
// of vertical strips (a marker at both ends of every strip), and the game draws
// each strip as its own sprite. When a sprite is drawn at 2x/4x,
// YBoardFindLargerArt() works out which image (and which strip of it) the
// sprite shows, looks for a larger image just before it, verifies the markers
// and the 2:1 match, repeats for further levels, and returns how to draw the
// matching part of the largest one in its place. Results are cached. Nothing
// is game specific: if a game's art isn't laid out like this, nothing matches
// and sprites are drawn as they are.
// ---------------------------------------------------------------------------

struct YBoardLargerArt {
	INT32 levels;		// 0 = none found; otherwise each level doubles the size
	INT32 start;		// first word of the largest image (top row, leftmost word)
	INT32 width;		// its row width in words
	INT32 rows;			// its row count
	INT32 C, D;			// largest-image pixel/row = 2^levels * (drawn-image pixel/row) + C/D
	INT32 childwidth;	// the drawn image: row width in words
	INT32 childrows;	// the drawn image: row count
	INT32 childstart;	// the drawn image: first word (top row, leftmost word)
	INT32 pos;			// which word of its row the sprite's addr is
	INT32 skip;			// rows the sprite reads before the image starts, in reading order (< 0: the image starts earlier)
	INT32 gapped;		// a link of the chain skips gap rows (see YBoardLargerChainSearch): loses ties
};

struct YBoardLodCacheEntry {
	INT32 used;
	INT32 bank, addr, pitch, flip, rowsread;	// the sprite as the game draws it
	YBoardLargerArt art;						// what to draw instead (levels = 0: nothing)
};

#define YBOARD_LOD_CACHE_SIZE	8192		// power of 2
static YBoardLodCacheEntry YBoardLodCache[YBOARD_LOD_CACHE_SIZE];
static const UINT8 *YBoardLodCacheRom = NULL;

// the same image is often drawn by many sprites (rows of trees, road patches)
// with different zooms, so the search result per image is cached as well
struct YBoardLodImageCacheEntry {
	const UINT64 *spritedata;		// NULL = unused
	INT32 start, width, rows;
	YBoardLargerArt art;
};
static YBoardLodImageCacheEntry YBoardLodImageCache[YBOARD_LOD_CACHE_SIZE];

// YBoardLargerChain() is asked about the same image many times (from each
// row count tried for the image below it, from each sprite size), so its
// results are kept as well
struct YBoardLodChainCacheEntry {
	const UINT64 *spritedata;		// NULL = unused
	INT32 start, width, rows, depth;
	INT32 levels, strong;
	YBoardLargerArt top;
};
static YBoardLodChainCacheEntry YBoardLodChainCache[YBOARD_LOD_CACHE_SIZE];

// The caches are open-addressed with up to 16 probes. When all of them are
// taken, the first one is replaced, so the caches never stop working however
// long the game runs (the results are deterministic, so this only costs time).
#define YBOARD_LOD_PROBES	16

static inline INT32 YBoardSpriteNibble(const UINT64 *spritedata, INT32 rowstart, INT32 x)
{
	UINT64 pixels = spritedata[(rowstart + (x >> 4)) & 0xffff];
#ifndef LSB_FIRST
	pixels = BURN_ENDIAN_SWAP_INT64(pixels);
#endif
	return (INT32)((pixels >> (60 - 4 * (x & 15))) & 0xf);
}

// every row starts with a 0xf nibble and ends with one
// Rows marked other than at both ends of the pitch (all seen in Rail Chase).
// A row shorter than the pitch, the rest empty words the hardware never reads
// (the fallen enemy 13:52f2), always counts: its markers are at the start of
// its first and the end of its last non-empty word. An empty row with a marker
// ending some word and one starting some word (the title enemy 12:728a, whose
// top rows are marked only in the middle) only counts in a second search, made
// when the first finds no larger art for a sprite (YBoardFindLargerArt): in
// Power Drift such rows also complete misaligned blocks of other art.
static INT32 YBoardMarkRelax = 0;		// the second search is being made
static bool YBoardRowEmpty(const UINT64 *spritedata, INT32 rowstart, INT32 width);
static bool YBoardRowMarkedOtherwise(const UINT64 *spritedata, INT32 rowstart, INT32 width)
{
	INT32 w0 = 0, w1 = width - 1;
	while (w0 < width && spritedata[(rowstart + w0) & 0xffff] == 0) w0++;
	while (w1 > w0 && spritedata[(rowstart + w1) & 0xffff] == 0) w1--;
	if (w0 < width && (w0 > 0 || w1 < width - 1) &&
		YBoardSpriteNibble(spritedata, rowstart + w0, 0) == 0xf && YBoardSpriteNibble(spritedata, rowstart + w1, 15) == 0xf)
		return true;
	if (!YBoardMarkRelax) return false;
	bool fwd = false, bwd = false;
	for (INT32 w = 0; w < width; w++) {
		if (YBoardSpriteNibble(spritedata, rowstart + w, 15) == 0xf) fwd = true;
		if (YBoardSpriteNibble(spritedata, rowstart + w, 0) == 0xf) bwd = true;
	}
	return fwd && bwd && YBoardRowEmpty(spritedata, rowstart, width);
}

// Does every row have its 0xf markers (see YBoardRowMarkedOtherwise too)?
static bool YBoardImageHasMarkers(const UINT64 *spritedata, INT32 start, INT32 width, INT32 rows)
{
	for (INT32 r = 0; r < rows; r++) {
		const INT32 rowstart = start + r * width;
		if (YBoardSpriteNibble(spritedata, rowstart, 0) == 0xf && YBoardSpriteNibble(spritedata, rowstart, width * 16 - 1) == 0xf)
			continue;
		if (!YBoardRowMarkedOtherwise(spritedata, rowstart, width)) return false;
	}
	return true;
}

// One of the two markers of a row (see YBoardLargerChainSearch).
static bool YBoardRowHasAMarker(const UINT64 *spritedata, INT32 rowstart, INT32 width)
{
	return YBoardSpriteNibble(spritedata, rowstart, 0) == 0xf ||
		YBoardSpriteNibble(spritedata, rowstart, width * 16 - 1) == 0xf;
}

// The smaller copies are mostly exact 2:1 samples, but some were retouched by
// hand (the title logo: 89%; a tree whose copy drops the edge column: 79%).
// A candidate must also have the exact size and position and the 0xf markers
// on every row, which unrelated art practically never has, so 75% is enough.
#define YBOARD_LOD_MIN_MATCH	750		// per mille
// In the second search (see YBoardRowMarkedOtherwise), made only when the first
// found nothing: Rail Chase's bat 10:66e4 has a retouched 2:1 parent 10:66d1
// that matches 70%.
#define YBOARD_LOD_SECOND_MIN_MATCH	700	// per mille
#define YBOARD_MIN_MATCH_NOW	(YBoardMarkRelax ? YBOARD_LOD_SECOND_MIN_MATCH : YBOARD_LOD_MIN_MATCH)
#define YBOARD_LOD_WEAK_MATCH	600		// per mille, for look-alikes (see YBoardLargerChain)

// An image unpacked to one byte per pixel for YBoardMatchHalf (0xf markers
// as 0), with its non-empty pixel counts: suf[0/1][y] = rows y, y + 2, y + 4..
// counting only pixels at even/odd x, suf[2][y] = rows y, y + 1.. (all pixels).
// The parent is stored as two planes, its even and its odd columns, so the
// pixels compared with a child row are next to each other.
// The last child and parent are kept: the same image is often matched again.
struct YBoardLodUnpacked {
	const UINT64 *spritedata;		// NULL = nothing unpacked
	INT32 start, width, rows;
	bool planar;
	INT32 done;						// rows unpacked so far (rows are unpacked when first needed)
	UINT8 *pixels;
	INT32 *suf[3];
	INT32 size, sufsize;			// allocated pixels, counts
};
static YBoardLodUnpacked YBoardLodChild, YBoardLodParent;

static void YBoardLodFree(YBoardLodUnpacked &u)
{
	if (u.pixels) free(u.pixels);
	if (u.suf[0]) free(u.suf[0]);
	memset(&u, 0, sizeof(u));
}

static inline UINT64 YBoardLodWord(const YBoardLodUnpacked &u, INT32 i)
{
	UINT64 pixels = u.spritedata[(u.start + i) & 0xffff];
#ifndef LSB_FIRST
	pixels = BURN_ENDIAN_SWAP_INT64(pixels);
#endif
	return pixels;
}

// unpack the rows up to and including row
static inline void YBoardLodRows(YBoardLodUnpacked &u, INT32 row)
{
	if (row < u.done) return;

	// a byte holds two pixels (high nibble first)
	static UINT8 pair[256][2];
	static bool table = false;
	if (!table) {
		for (INT32 b = 0; b < 256; b++) {
			pair[b][0] = ((b >> 4) == 0xf) ? 0 : (b >> 4);
			pair[b][1] = ((b & 0xf) == 0xf) ? 0 : (b & 0xf);
		}
		table = true;
	}

	const INT32 hw = u.width * 8;
	for (INT32 y = u.done; y <= row; y++) {
		UINT8 *out = u.pixels + y * 2 * hw;
		UINT8 *even = u.pixels + y * hw, *odd = u.pixels + (u.rows + y) * hw;
		for (INT32 i = 0; i < u.width; i++) {
			const UINT64 pixels = YBoardLodWord(u, y * u.width + i);
			for (INT32 k = 56; k >= 0; k -= 8) {
				const INT32 b = (INT32)(pixels >> k) & 0xff;
				if (u.planar) { *even++ = pair[b][0]; *odd++ = pair[b][1]; }
				else { *out++ = pair[b][0]; *out++ = pair[b][1]; }
			}
		}
	}
	u.done = row + 1;
}

static bool YBoardLodUnpack(YBoardLodUnpacked &u, const UINT64 *spritedata, INT32 start, INT32 width, INT32 rows, bool planar)
{
	if (u.spritedata == spritedata && u.start == start && u.width == width && u.rows == rows) return true;

	const INT32 w = width * 16;
	if (w * rows > u.size || 3 * (rows + 2) > u.sufsize) {
		YBoardLodFree(u);
		u.pixels = (UINT8*)malloc(w * rows);
		u.suf[0] = (INT32*)malloc(3 * (rows + 2) * sizeof(INT32));
		if (u.pixels == NULL || u.suf[0] == NULL) { YBoardLodFree(u); return false; }
		u.size = w * rows; u.sufsize = 3 * (rows + 2);
	}
	u.suf[1] = u.suf[0] + (rows + 2);
	u.suf[2] = u.suf[1] + (rows + 2);
	u.spritedata = spritedata; u.start = start; u.width = width; u.rows = rows;
	u.planar = planar; u.done = 0;

	// non-empty pixels (not 0, not 0xf) per row, at even and odd x, 16 at a time
	const UINT64 ones = ((UINT64)0x01010101 << 32) | 0x01010101, nib = ones * 0x11;
	for (INT32 k = 0; k < 3; k++) u.suf[k][rows] = u.suf[k][rows + 1] = 0;
	for (INT32 y = rows - 1; y >= 0; y--) {
		INT32 n0 = 0, n1 = 0;
		for (INT32 i = 0; i < width; i++) {
			const UINT64 v = YBoardLodWord(u, y * width + i);
			const UINT64 any = (v | (v >> 1) | (v >> 2) | (v >> 3)) & nib;	// bit 0 of each nibble: not 0
			const UINT64 all = (v & (v >> 1) & (v >> 2) & (v >> 3)) & nib;	// bit 0 of each nibble: 0xf
			const UINT64 ne = any & ~all;
			n0 += (INT32)(((((ne >> 4) & ones) * ones) >> 56) & 0xff);		// high nibbles: even x
			n1 += (INT32)((((ne & ones) * ones) >> 56) & 0xff);				// low nibbles: odd x
		}
		u.suf[0][y] = u.suf[0][y + 2] + n0;
		u.suf[1][y] = u.suf[1][y + 2] + n1;
		u.suf[2][y] = u.suf[2][y + 1] + n0 + n1;
	}
	return true;
}

// Is the image at pstart (pwidth x prows) the 2:1 original of the child image?
// Child pixel (x, y) = parent pixel (2 * (x - offx) + ox, 2 * (y - offy) + oy).
// Returns the best match fraction over non-empty pixels (marker nibbles count as empty),
// or -1 if there are too few of them to tell.
static INT32 YBoardMatchHalf(const UINT64 *spritedata, INT32 cstart, INT32 cwidth, INT32 crows,
	INT32 pstart, INT32 pwidth, INT32 prows, INT32 *best_dx, INT32 *best_dy, INT32 minimum = YBOARD_LOD_MIN_MATCH)
{
	INT32 best = -1;
	INT32 need = minimum - 10;		// an alignment scoring below this is dropped early
	const INT32 cw = cwidth * 16;
	const INT32 centre = 8 * cwidth - 4 * pwidth;	// horizontal centring of the reduction, in child pixels

	if (!YBoardLodUnpack(YBoardLodChild, spritedata, cstart, cwidth, crows, false)) return -1;
	if (!YBoardLodUnpack(YBoardLodParent, spritedata, pstart, pwidth, prows, true)) return -1;
	const UINT8 *child = YBoardLodChild.pixels;
	const UINT8 *plane[2] = { YBoardLodParent.pixels, YBoardLodParent.pixels + prows * pwidth * 8 };
	const INT32 hw = pwidth * 8;					// plane width
	// 8 pixels at a time: a byte's top bit set if it is non-zero / zero
	const UINT64 ones = ((UINT64)0x01010101 << 32) | 0x01010101, low7 = ones * 0x7f, top = ones * 0x80;
	const INT32 *csuf = YBoardLodChild.suf[2];
	INT32 *const *psuf = YBoardLodParent.suf;

	// the natural alignment (centred, top rows aligned) is tried first; another
	// one is only taken if it matches clearly better
	static const INT32 order[3] = { 0, -1, 1 };
	for (INT32 i = 0; i < 3; i++) {
		const INT32 offx = centre + order[i];
		for (INT32 j = 0; j < 3; j++) {
			const INT32 offy = order[j];
			for (INT32 ox = 0; ox < 2; ox++) {
				// child pixels x0 <= x < x1 lie on the parent (0 <= px < pw):
				// px = 2 * (x - offx) + ox, which is plane ox, column x - offx
				INT32 x0 = (offx > 0) ? offx : 0;
				INT32 x1 = offx + hw;
				if (x1 > cw) x1 = cw;
				if (x1 < 0) x1 = 0;
				if (x0 > x1) x0 = x1;
				for (INT32 oy = 0; oy < 2; oy++) {
					INT32 same = 0, total = 0;
					for (INT32 y = 0; y < crows; y++) {
						// give up on this alignment once it can't reach the minimum any
						// more (less 1%; the result is the same: such a score is never used).
						// Once one alignment has scored, another is only taken if it scores more
						// than 10 above it, so it can be dropped as soon as it can't.
						const INT32 py = 2 * (y - offy) + oy;
						const INT32 pfrom = (py < oy) ? oy : py;		// first parent row still to come
						const INT32 most = total + csuf[y] + ((pfrom < prows) ? psuf[ox][pfrom] : 0);	// largest possible final total
						if ((total - same) * 1000 > (1000 - need) * most) { total = 0; break; }
						if (py < 0 || py >= prows) {
							total += csuf[y] - csuf[y + 1];		// the row's non-empty pixels
							continue;
						}
						YBoardLodRows(YBoardLodChild, y);
						YBoardLodRows(YBoardLodParent, py);
						const UINT8 *c = child + y * cw;
						const UINT8 *p = plane[ox] + py * hw - offx;
						for (INT32 x = 0; x < x0; x++) total += (c[x] != 0);
						for (INT32 x = x1; x < cw; x++) total += (c[x] != 0);
						INT32 x = x0;
						for (; x + 8 <= x1; x += 8) {
							UINT64 cv, pv;
							memcpy(&cv, c + x, 8);
							memcpy(&pv, p + x, 8);
							const UINT64 nz = ((cv | pv) + low7) & top;			// pixel values are 0..14
							const UINT64 eq = ~((cv ^ pv) + low7) & top;
							total += (INT32)((((nz >> 7) * ones) >> 56) & 0xff);
							same += (INT32)(((((nz & eq) >> 7) * ones) >> 56) & 0xff);
						}
						for (; x < x1; x++) {
							const INT32 cv = c[x], pv = p[x];
							const INT32 nz = (cv | pv) != 0;
							total += nz;
							same += nz & (cv == pv);
						}
					}
					// tiny images (the farthest copies are a few pixels) must match exactly
					if (total >= 16 || (total >= 2 && same == total)) {
						const INT32 score = (same * 1000) / total;	// per mille
						if (best < 0 || score > best + 10) {
							best = score;
							if (need < best + 10) need = best + 10;
							*best_dx = ox - 2 * offx;
							*best_dy = oy - 2 * offy;
						}
					}
				}
			}
		}
	}
	return best;
}

// A row's layout at a given width: -1 if it lacks the 0xf marker at either
// end, otherwise a bit for each inner word boundary that has markers on both
// sides (images made of strips have them; others don't).
static INT32 YBoardRowLayout(const UINT64 *spritedata, INT32 rowstart, INT32 width)
{
	if (rowstart < 0 || rowstart + width > 0x10000) return -1;
	if (YBoardSpriteNibble(spritedata, rowstart, 0) != 0xf || YBoardSpriteNibble(spritedata, rowstart, width * 16 - 1) != 0xf) return -1;
	INT32 layout = 0;
	for (INT32 w = 1; w < width && w < 31; w++)
		if (YBoardSpriteNibble(spritedata, rowstart, w * 16 - 1) == 0xf && YBoardSpriteNibble(spritedata, rowstart, w * 16) == 0xf)
			layout |= 1 << w;
	return layout;
}

// Is the row empty: every pixel 0 or a 0xf marker?
static bool YBoardRowEmpty(const UINT64 *spritedata, INT32 rowstart, INT32 width)
{
	for (INT32 w = 0; w < width; w++) {
		UINT64 v = spritedata[(rowstart + w) & 0xffff];
		for (INT32 n = 0; n < 16; n++, v >>= 4)
			if ((v & 0xf) != 0 && (v & 0xf) != 0xf) return false;
	}
	return true;
}

// Does an image start at start (width words, rows rows)? Its first two rows
// have the same layout, and the row before it doesn't (or has no markers).
static bool YBoardStartsImage(const UINT64 *spritedata, INT32 start, INT32 width, INT32 rows)
{
	const INT32 first = YBoardRowLayout(spritedata, start, width);
	if (rows >= 2 && YBoardRowLayout(spritedata, start + width, width) != first) return false;
	return YBoardRowLayout(spritedata, start - width, width) != first;
}

// Do two images look alike: about the same share of non-empty pixels, and
// mostly the same colours? Used only for candidates whose size and position
// in ROM already fit (see YBoardLargerChain).
static bool YBoardSimilarArt(const UINT64 *spritedata, INT32 cstart, INT32 cwidth, INT32 crows,
	INT32 pstart, INT32 pwidth, INT32 prows)
{
	INT32 ch[16] = { 0 }, ph[16] = { 0 };
	INT32 cn = 0, pn = 0;
	for (INT32 y = 0; y < crows; y++)
		for (INT32 x = 0; x < cwidth * 16; x++) {
			const INT32 c = YBoardSpriteNibble(spritedata, cstart + y * cwidth, x);
			if (c != 0 && c != 0xf) { ch[c]++; cn++; }
		}
	for (INT32 y = 0; y < prows; y++)
		for (INT32 x = 0; x < pwidth * 16; x++) {
			const INT32 p = YBoardSpriteNibble(spritedata, pstart + y * pwidth, x);
			if (p != 0 && p != 0xf) { ph[p]++; pn++; }
		}
	if (pn < 16) return false;

	// The farthest copies can be a handful of pixels (a car seen from behind:
	// 2 rows), too few for the tests below: then every colour of the copy
	// must be in the larger image.
	if (cn < 16) {
		if (cn < 2) return false;
		for (INT32 i = 1; i < 15; i++)
			if (ch[i] && !ph[i]) return false;
		return true;
	}

	// share of non-empty pixels within a factor of 1.5
	const INT64 cd = (INT64)cn * pwidth * prows, pd = (INT64)pn * cwidth * crows;
	if (2 * cd > 3 * pd || 2 * pd > 3 * cd) return false;

	// colour histograms overlap by at least 75%
	INT64 overlap = 0;
	for (INT32 i = 1; i < 15; i++) {
		const INT64 a = (INT64)ch[i] * pn, b = (INT64)ph[i] * cn;
		overlap += (a < b) ? a : b;
	}
	return overlap * 4 >= (INT64)cn * pn * 3;
}

// Find the chain of larger images above the image at start (width x rows,
// markers already checked), at most depth levels. Returns the number of levels;
// *top gets the largest image and C/D relative to this one (see YBoardLargerArt).
// Sometimes more than one block passes the 2:1 check (e.g. a parent with a
// row more or less, when the reduction isn't exact), but only the real image
// has larger images of its own above it, so the candidate whose chain goes on
// longest is taken. On a tie the first one is kept unless another matches
// clearly better, or about as well and starts where the image before it ends.
static INT32 YBoardLargerChainSearch(const UINT64 *spritedata, INT32 start, INT32 width, INT32 rows, INT32 depth, YBoardLargerArt *top, INT32 *strong);

static INT32 YBoardLargerChain(const UINT64 *spritedata, INT32 start, INT32 width, INT32 rows, INT32 depth, YBoardLargerArt *top, INT32 *strong)
{
	*strong = 0;
	top->start = start; top->width = width; top->rows = rows; top->C = 0; top->D = 0; top->gapped = 0;
	if (depth <= 0) return 0;

	const INT32 dkey = depth + (YBoardMarkRelax << 8);		/* (results differ in the second search) */
	UINT32 h = (UINT32)(start * 0x9e3779b1u) ^ (UINT32)(width * 0x85ebca6bu) ^ (UINT32)(rows * 0xc2b2ae35u) ^ (UINT32)(dkey * 0x165667b1u) ^ (UINT32)(((size_t)spritedata >> 3) * 0x27d4eb2fu);
	h ^= h >> 15;
	YBoardLodChainCacheEntry *slot = NULL;
	for (INT32 probe = 0; probe < YBOARD_LOD_PROBES; probe++) {
		YBoardLodChainCacheEntry &e = YBoardLodChainCache[(h + probe) & (YBOARD_LOD_CACHE_SIZE - 1)];
		if (e.spritedata == NULL) { slot = &e; break; }
		if (e.spritedata == spritedata && e.start == start && e.width == width && e.rows == rows && e.depth == dkey) {
			*top = e.top; *strong = e.strong;
			return e.levels;
		}
	}
	if (slot == NULL) slot = &YBoardLodChainCache[h & (YBOARD_LOD_CACHE_SIZE - 1)];

	const INT32 levels = YBoardLargerChainSearch(spritedata, start, width, rows, depth, top, strong);
	slot->spritedata = spritedata; slot->start = start; slot->width = width; slot->rows = rows; slot->depth = dkey;
	slot->levels = levels; slot->strong = *strong; slot->top = *top;
	return levels;
}

static INT32 YBoardLargerChainSearch(const UINT64 *spritedata, INT32 start, INT32 width, INT32 rows, INT32 depth, YBoardLargerArt *top, INT32 *strong)
{
	INT32 blevels = 0, bscore = 0, bbounded = 0, bgapped = 0;

	// The larger image has 2W or 2W - 1 words per row and 2R, 2R - 1 or 2R + 1
	// rows (an odd row count is halved either way); full size first. R is
	// only known from the rows the game draws, though, and a tall image (a
	// tower drawn as one sprite) may be drawn with fewer or more rows than it
	// has. So the rows with markers just before this image are counted too,
	// and that count is tried as well if it is within 25% of 2R.
	// Some small copies keep a wider row than they need (a car 4 words wide
	// whose half-size copy has 3-word rows), so 2W - 2 words is tried last.
	INT32 run[3] = { 0, 0, 0 };		// marker rows just before start, for 2W, 2W - 1 and 2W - 2 words
	for (INT32 i = 0; i < 3; i++) {
		const INT32 pwidth = 2 * width - i, most = 2 * rows + rows / 2 + 1;
		if (pwidth < 1) continue;
		while (run[i] < most && start - pwidth * (run[i] + 1) >= 0 &&
			YBoardImageHasMarkers(spritedata, start - pwidth * (run[i] + 1), pwidth, 1))
			run[i]++;
	}

	// The last rows of a larger image that the game never draws in full may
	// have only one of their markers (Rail Chase's fighter 13:4c9d, whose two
	// bottom rows only have the one at their end; Power Drift's START banner
	// 6:d240, whose empty last row lacks its end marker). If nothing else is
	// found, up to two such rows right before start are skipped: the candidates
	// then end before them (gap rows). Only for images of 8 rows or more: a
	// tiny image matches some block after a gap by chance.
	INT32 gap[3] = { 0, 0, 0 }, gaprun[3] = { 0, 0, 0 };
	for (INT32 i = 0; i < 3; i++) {
		const INT32 pwidth = 2 * width - i, most = 2 * rows + rows / 2 + 1;
		if (pwidth < 1 || rows < 8) continue;
		while (gap[i] < 2 && start - pwidth * (gap[i] + 1) >= 0 &&
			!YBoardImageHasMarkers(spritedata, start - pwidth * (gap[i] + 1), pwidth, 1) &&
			YBoardRowHasAMarker(spritedata, start - pwidth * (gap[i] + 1), pwidth))
			gap[i]++;
		if (gap[i] == 0) continue;
		const INT32 end = start - pwidth * gap[i];
		while (gaprun[i] < most && end - pwidth * (gaprun[i] + 1) >= 0 &&
			YBoardImageHasMarkers(spritedata, end - pwidth * (gaprun[i] + 1), pwidth, 1))
			gaprun[i]++;
	}

	// Pass 0 wants the 2:1 match. Some copies were redrawn rather than reduced
	// (a repeating tower pattern with a different period), so if nothing
	// matches, pass 1 accepts a candidate that has the right size and position,
	// starts at an image boundary and looks alike (YBoardSimilarArt), but only
	// below a real 2:1 match: the candidate must have a 2:1 parent itself.
	// (*strong: the first link of the chain returned is a 2:1 match.)
	for (INT32 withgap = 0; withgap < 2 && blevels == 0; withgap++)
	for (INT32 pass = 0; pass < 2 && blevels == 0; pass++)
	for (INT32 ci = 0; ci < 12; ci++) {
		const INT32 i = (ci < 8) ? (ci & 1) : 2;	// 2W, then 2W - 1 words; 2W - 2 last
		const INT32 pwidth = 2 * width - i;
		if (i == 2 && width < 2) continue;
		if (withgap && gap[i] == 0) continue;
		const INT32 skip = withgap ? gap[i] : 0;		// gap rows before start
		const INT32 avail = withgap ? gaprun[i] : run[i];	// marker rows before them
		INT32 prows;
		static const INT32 rowsplus[3] = { 0, -1, 1 };
		if (ci < 6) {
			prows = 2 * rows + rowsplus[ci >> 1];
		} else if (ci >= 8 && ci < 11) {
			prows = 2 * rows + rowsplus[ci - 8];
		} else {
			prows = avail;
			if (prows >= 2 * rows - 1 && prows <= 2 * rows + 1) continue;	// tried already
			if (4 * prows < 3 * 2 * rows || 4 * prows > 5 * 2 * rows) continue;
		}
		{
			const INT32 pstart = start - pwidth * (prows + skip);
			if (pwidth < 1 || prows < 1 || pstart < 0) continue;
			if (prows > avail) continue;			// some row lacks its markers

			// does the candidate start right where the image starts? A block that
			// starts a row into the real image, or a row early (taking in the last
			// row of the image before it), matches about as well, but doesn't.
			const INT32 bounded = YBoardStartsImage(spritedata, pstart, pwidth, prows);

			INT32 dx = 0, dy = 0, score = 0, weak = 0;
			if (pass == 0) {
				score = YBoardMatchHalf(spritedata, start, width, rows, pstart, pwidth, prows, &dx, &dy, YBOARD_MIN_MATCH_NOW);
				if (score < YBOARD_MIN_MATCH_NOW) continue;
			} else {
				if (!bounded || !YBoardSimilarArt(spritedata, start, width, rows, pstart, pwidth, prows)) continue;
				dx = -2 * (8 * width - 4 * pwidth);		// centred, top rows aligned
				// Copies of grainy textures (road surfaces) are often re-dithered rather
				// than reduced: they still match 2:1 well short of the minimum, but
				// clearly better than chance; such a link may stand without a 2:1
				// parent above it, placed where it matches best.
				INT32 wdx = 0, wdy = 0;
				if (YBoardMatchHalf(spritedata, start, width, rows, pstart, pwidth, prows, &wdx, &wdy, YBOARD_LOD_WEAK_MATCH) >= YBOARD_LOD_WEAK_MATCH) {
					weak = 1; dx = wdx; dy = wdy;
				}
			}

			YBoardLargerArt above;
			INT32 abovestrong = 0;
			const INT32 levels = 1 + YBoardLargerChain(spritedata, pstart, pwidth, prows, depth - 1, &above, &abovestrong);
			if (pass == 1 && !abovestrong && !weak) continue;
			const INT32 gapped = withgap || above.gapped;
			if (levels > blevels || (levels == blevels && (gapped < bgapped || (gapped == bgapped && (score > bscore + 10 ||
				(score >= bscore - 10 && bounded && !bbounded)))))) {
				blevels = levels; bscore = score; bbounded = bounded; bgapped = gapped; *strong = (pass == 0);
				top->start = above.start; top->width = above.width; top->rows = above.rows; top->gapped = gapped;
				top->C = (dx << (levels - 1)) + above.C;
				top->D = (dy << (levels - 1)) + above.D;
			}
		}
	}

	return blevels;
}

static YBoardLargerArt YBoardFindLargerArtSearch(const UINT64 *spritedata, INT32 start, INT32 width, INT32 rows);

static YBoardLargerArt YBoardFindLargerArtUncached(const UINT64 *spritedata, INT32 start, INT32 width, INT32 rows)
{
	const INT32 rkey = rows + (YBoardMarkRelax << 16);		/* (results differ in the second search) */
	UINT32 h = (UINT32)(start * 0x9e3779b1u) ^ (UINT32)(width * 0x85ebca6bu) ^ (UINT32)(rkey * 0xc2b2ae35u) ^ (UINT32)(((size_t)spritedata >> 3) * 0x27d4eb2fu);
	h ^= h >> 15;
	YBoardLodImageCacheEntry *slot = NULL;
	for (INT32 probe = 0; probe < YBOARD_LOD_PROBES; probe++) {
		YBoardLodImageCacheEntry &e = YBoardLodImageCache[(h + probe) & (YBOARD_LOD_CACHE_SIZE - 1)];
		if (e.spritedata == NULL) { slot = &e; break; }
		if (e.spritedata == spritedata && e.start == start && e.width == width && e.rows == rkey)
			return e.art;
	}
	if (slot == NULL) slot = &YBoardLodImageCache[h & (YBOARD_LOD_CACHE_SIZE - 1)];
	slot->spritedata = spritedata; slot->start = start; slot->width = width; slot->rows = rkey;
	slot->art = YBoardFindLargerArtSearch(spritedata, start, width, rows);
	return slot->art;
}

static YBoardLargerArt YBoardFindLargerArtSearch(const UINT64 *spritedata, INT32 start, INT32 width, INT32 rows)
{
	YBoardLargerArt art = { 0, start, width, rows, 0, 0, width, rows, start, 0, 0, 0 };

	if (!YBoardImageHasMarkers(spritedata, start, width, rows)) return art;

	YBoardLargerArt top;
	INT32 strong = 0;
	art.levels = YBoardLargerChain(spritedata, start, width, rows, 4, &top, &strong);
	if (art.levels > 0) {
		art.start = top.start; art.width = top.width; art.rows = top.rows;
		art.C = top.C; art.D = top.D; art.gapped = top.gapped;
	}

	return art;
}

// The sprite as the game draws it: the first drawn row is read from addr
// onward (backward when flipped) until the end marker, rows are pitch words
// apart, and rowsread rows are read. addr may be any word of the image's row
// (a strip of a wider image), so every position is tried; only the right one
// passes the marker and 2:1 checks. The image may have a row or two more than
// were read when the game shrinks it, so a few row counts are tried as well.
// Tall images (towers) may also be read past their first or last row, into
// the art next to them; then the image is the longest run of read rows that
// have markers, if that is most of them, and skip says how many rows were
// read before it. They may also be drawn from their middle, so the whole run
// of marker rows around the rows read is tried too (skip < 0: the image
// starts before the first row read).
// A wrong row count can still pass the 2:1 check against a block shifted by a
// row, but then the chain of larger images breaks off early, so the choice
// that reaches the most levels wins (the first one found on a tie).
static YBoardLargerArt YBoardFindLargerArtForSprite(const UINT64 *spritedata, UINT16 addr, INT32 pitch, INT32 flip, INT32 rowsread, INT32 zoom)
{
	YBoardLargerArt best = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	const INT32 width = (pitch < 0) ? -pitch : pitch;
	if (width == 0 || width > 64 || rowsread < 1 || rowsread > 1024) return best;

	const INT32 extra = (zoom >> 9) + 1;

	for (INT32 pos = 0; pos < width; pos++) {			// addr's word within its row
		const INT32 rowstart = addr - pos;

		// the longest run of read rows with markers: [a, b)
		INT32 a = 0, b = 0;
		for (INT32 i = 0; i < rowsread; ) {
			if (!YBoardImageHasMarkers(spritedata, (rowstart + pitch * i) & 0xffff, width, 1)) { i++; continue; }
			INT32 j = i + 1;
			while (j < rowsread && YBoardImageHasMarkers(spritedata, (rowstart + pitch * j) & 0xffff, width, 1)) j++;
			if (j - i > b - a) { a = i; b = j; }
			i = j;
		}
		if (b - a < rowsread && 4 * (b - a) < 3 * rowsread) continue;	// mostly not one image

		// The run may also be only part of a taller image (a tower drawn from
		// its middle): then the image is the whole run of marker rows around it,
		// also before the first row read. Reading indexes s < 0 are before it.
		INT32 s = a, e = b;
		#define YBOARD_LOD_ROW_OK(i)	(rowstart + pitch * (i) >= 0 && rowstart + pitch * (i) < 0x10000 && \
			YBoardImageHasMarkers(spritedata, rowstart + pitch * (i), width, 1))
		while (e - s < 1024 && YBOARD_LOD_ROW_OK(s - 1)) s--;
		if (b == rowsread) while (e - s < 1024 && YBOARD_LOD_ROW_OK(e)) e++;
		#undef YBOARD_LOD_ROW_OK

		// That run can go on into the next images (a tower drawn shortened, from
		// its top: the rows after it are the next, smaller copy). So the run
		// around it whose rows also have the same layout of markers as the first
		// row read (YBoardRowLayout) is tried as well: [s2, e2).
		INT32 s2 = a, e2 = b;
		{
			const INT32 layout = YBoardRowLayout(spritedata, (rowstart + pitch * a) & 0xffff, width);
			// (a row with nothing but its markers is padding after an image, not part of it)
			#define YBOARD_LOD_ROW_SAME(i)	(rowstart + pitch * (i) >= 0 && rowstart + pitch * (i) < 0x10000 && \
				YBoardRowLayout(spritedata, rowstart + pitch * (i), width) == layout && \
				!YBoardRowEmpty(spritedata, rowstart + pitch * (i), width))
			while (e2 - s2 < 1024 && s2 > s && YBOARD_LOD_ROW_SAME(s2 - 1)) s2--;
			while (e2 - s2 < 1024 && e2 < e && YBOARD_LOD_ROW_SAME(e2)) e2++;
			#undef YBOARD_LOD_ROW_SAME
		}

		// (first, rows) to try: the run, a few more rows if it reaches the last
		// row read, and the whole run of marker rows around it
		const INT32 lo = b - a, hi = (b == rowsread) ? (rowsread - a + extra) : lo;
		for (INT32 rows = lo; rows <= hi + 2; rows++) {
			INT32 first = a, n = rows;
			if (rows == hi + 1) {
				if (s == a && e - a <= hi) continue;	// nothing more around it
				first = s; n = e - s;
			}
			if (rows == hi + 2) {
				if ((s2 == a && e2 - a <= hi) || (s2 == s && e2 == e)) break;	// tried already
				first = s2; n = e2 - s2;
			}
			const INT32 start = (pitch > 0) ? (rowstart + width * first) : (rowstart - width * (first + n - 1));
			if (start < 0 || start + width * n > 0x10000) continue;

			YBoardLargerArt art = YBoardFindLargerArtUncached(spritedata, start, width, n);
			if (art.levels > best.levels || (art.levels == best.levels && art.levels > 0 && best.gapped && !art.gapped)) {
				art.pos = pos;
				art.skip = first;
				best = art;
			}
		}
	}

	return best;
}

static bool YBoardFindLargerArt(const UINT64 *spritedata, INT32 bank, UINT16 addr, INT32 pitch, INT32 flip, INT32 rowsread, INT32 zoom, YBoardLargerArt *art)
{
	if (YBoardLodCacheRom != System16Sprites2) {
		memset(YBoardLodCache, 0, sizeof(YBoardLodCache));
		memset(YBoardLodImageCache, 0, sizeof(YBoardLodImageCache));
		memset(YBoardLodChainCache, 0, sizeof(YBoardLodChainCache));
		YBoardLodChild.spritedata = YBoardLodParent.spritedata = NULL;
		YBoardLodCacheRom = System16Sprites2;
	}

	// the extra rows tried depend on the zoom, so it's part of the key (as zoom >> 9)
	const INT32 zkey = zoom >> 9;
	UINT32 h = (UINT32)bank * 0x9e3779b1u ^ (UINT32)addr * 0x85ebca6bu ^ (UINT32)(pitch & 0xff) * 0xc2b2ae35u ^ (UINT32)(rowsread * 2 + flip) * 0x27d4eb2fu ^ (UINT32)zkey * 0x165667b1u;
	h ^= h >> 15;
	YBoardLodCacheEntry *slot = NULL;
	for (INT32 probe = 0; probe < YBOARD_LOD_PROBES; probe++) {
		YBoardLodCacheEntry &e = YBoardLodCache[(h + probe) & (YBOARD_LOD_CACHE_SIZE - 1)];
		if (!e.used) { slot = &e; break; }
		if (e.bank == bank && e.addr == addr && e.pitch == pitch && e.flip == flip && e.rowsread == rowsread * 16 + zkey) {
			*art = e.art;
			return art->levels > 0;
		}
	}
	if (slot == NULL) slot = &YBoardLodCache[h & (YBOARD_LOD_CACHE_SIZE - 1)];
	slot->used = 1; slot->bank = bank; slot->addr = addr; slot->pitch = pitch; slot->flip = flip; slot->rowsread = rowsread * 16 + zkey;
	slot->art = YBoardFindLargerArtForSprite(spritedata, addr, pitch, flip, rowsread, zoom);
	if (slot->art.levels == 0) {
		/* the second search: one more row form (YBoardRowMarkedOtherwise), and a
		   lower 2:1 match minimum */
		YBoardMarkRelax = 1;
		slot->art = YBoardFindLargerArtForSprite(spritedata, addr, pitch, flip, rowsread, zoom);
		YBoardMarkRelax = 0;
	}
	*art = slot->art;
	return art->levels > 0;
}

// A sprite row is read until a word ends with the 0xf marker. When the game
// reads a row or two past the end of an image, into art that has no marker
// where the row should end, the hardware keeps reading and drawing the
// following words (the next rows' data) far to the side: on some courses a
// car shadow is followed by such art and leaves stray dots beside the car.
// Some sprites run on past the row end on purpose, though (the horizon sea
// strip), so at high resolution a row is only cut at its pitch when what lies
// beyond is nearly empty: fewer than 1 visible pixel in 16. Returns how many
// words to read at most.
static INT32 YBoardRowWordLimit(const UINT64 *spritedata, UINT16 rowaddr, INT32 flip, INT32 rowwords, const UINT16 *indirect)
{
	if (rowwords == 0) return 0x10000;
	INT32 visible = 0, beyond = 0;
	for (INT32 n = 0; n < 256; n++) {
		UINT64 pixels = spritedata[(UINT16)(flip ? (rowaddr - n) : (rowaddr + n))];
#ifndef LSB_FIRST
		pixels = BURN_ENDIAN_SWAP_INT64(pixels);
#endif
		if (n >= rowwords) {
			for (INT32 shift = 0; shift <= 60; shift += 4)
				if (BURN_ENDIAN_SWAP_INT16(indirect[(pixels >> shift) & 0xf]) < 0x1fe) visible++;
			beyond += 16;
		}
		if (((flip ? (pixels >> 60) : pixels) & 0xf) == 0xf) break;	/* the nibble read last */
	}
	if (beyond == 0) return 0x10000;
	return (visible * 16 < beyond) ? rowwords : 0x10000;
}

// E(p) = ceil(p * one / step): the number of framebuffer pixels drawn for the
// first p source pixels when every source pixel advances the zoom accumulator
// by step and a framebuffer pixel is drawn each time it stays below one.
static inline INT32 YBoardRunStart(INT32 p, INT32 one, INT32 step)
{
	return (INT32)(((INT64)p * one + step - 1) / step);
}

// Source pixel p of a sprite row as drawn: the p-th pixel read from rowaddr
// (backward when flipped), or of the large image's columns [u0, u1); returns
// its framebuffer value, or 0xffff if transparent.
static inline UINT32 YBoardSourcePixel(const UINT64 *spritedata, bool larger, INT32 lrow, INT32 u0, INT32 u1,
	UINT16 rowaddr, INT32 flip, INT32 p, const UINT16 *indirect, INT32 colorpri)
{
	INT32 pix;
	if (larger) {
		pix = YBoardSpriteNibble(spritedata, lrow, flip ? (u1 - 1 - p) : (u0 + p));
	} else {
		UINT64 pixels = spritedata[(UINT16)(flip ? (rowaddr - (p >> 4)) : (rowaddr + (p >> 4)))];
#ifndef LSB_FIRST
		pixels = BURN_ENDIAN_SWAP_INT64(pixels);
#endif
		pix = (INT32)((pixels >> (flip ? (4 * (p & 15)) : (60 - 4 * (p & 15)))) & 0xf);
	}
	const UINT32 ind = BURN_ENDIAN_SWAP_INT16(indirect[pix]);
	return (ind < 0x1fe) ? (ind | colorpri) : 0xffff;
}

// Which part of each row of the high resolution sprite framebuffer has been
// drawn since the row was last cleared (lo > hi: none), so that clearing only
// touches that part.
static INT16 YBoardDirtyLo[512 * 4], YBoardDirtyHi[512 * 4];
static UINT16 *YBoardDirtyBuffer = NULL;
static INT32 YBoardDirtyScale = 0;

// The rectangle of the high resolution sprite framebuffer that
// System16RotateDrawScaled() will read this frame (inclusive bounds), from the
// same rotate RAM values, with a margin. Pixels outside it can't be seen, so
// they needn't be drawn. If the area read wraps around the framebuffer edge,
// the whole width (or height) is returned. Rows the game marks as off-screen
// keep their contents into the next frame and are never drawn into, so not
// drawing the unseen pixels of the other rows can't show up later either.
static void YBoardRotateView(INT32 s, INT32 *x0, INT32 *x1, INT32 *y0, INT32 *y1)
{
	const UINT16 *pRotateBuff = (UINT16*)System16RotateRamBuff;
	const INT32 W = 512 * s, margin = 2 * s;
	*x0 = 0; *x1 = W - 1; *y0 = 0; *y1 = W - 1;

	INT32 currx = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f0]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f1]);
	INT32 curry = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f2]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f3]);
	const INT32 dyy = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f4]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f5]);
	const INT32 dxx = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f6]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f7]);
	const INT32 dxy = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f8]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f9]);
	const INT32 dyx = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3fa]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3fb]);
	currx += dxx * 27;
	curry += dyx * 27;

	/* the corners, exactly as System16RotateDrawScaled() steps (positions times s) */
	const INT64 w1 = 320 * s - 1, h1 = 224 * s - 1;
	INT64 cx[4], cy[4];
	for (INT32 c = 0; c < 4; c++) {
		const INT64 xx = (c & 1) ? w1 : 0, yy = (c & 2) ? h1 : 0;
		cx[c] = ((INT64)currx * s + yy * dxy + xx * dxx) >> 14;
		cy[c] = ((INT64)curry * s + yy * dyy + xx * dyx) >> 14;
	}
	INT64 lo, hi;
	lo = hi = cx[0];
	for (INT32 c = 1; c < 4; c++) { if (cx[c] < lo) lo = cx[c]; if (cx[c] > hi) hi = cx[c]; }
	lo -= margin; hi += margin;
	if (hi - lo + 1 < W) {
		const INT32 a = (INT32)(lo & (W - 1)), b = a + (INT32)(hi - lo);
		if (b < W) { *x0 = a; *x1 = b; }
	}
	lo = hi = cy[0];
	for (INT32 c = 1; c < 4; c++) { if (cy[c] < lo) lo = cy[c]; if (cy[c] > hi) hi = cy[c]; }
	lo -= margin; hi += margin;
	if (hi - lo + 1 < W) {
		const INT32 a = (INT32)(lo & (W - 1)), b = a + (INT32)(hi - lo);
		if (b < W) { *y0 = a; *y1 = b; }
	}
}

// The rectangle of the framebuffer the rotation stage shows this frame, and
// each sprite's larger art (by sprite list entry), set before drawing.
static INT32 YBoardViewX0, YBoardViewX1, YBoardViewY0, YBoardViewY1;
static YBoardLargerArt YBoardFrameArt[0x1000];
static UINT8 YBoardFrameSkip[0x1000];		/* left out: see YBoardVisiblePixels */

// A second thread for the high resolution renderer, via FBNeo's thready
// (Windows, pthreads, macOS; elsewhere everything runs on the calling thread). It
// draws half of the sprite framebuffer's rows, or the lower half of the
// rotated screen, while the calling thread does the rest.

// which of parts threads draws sprite framebuffer row y
#define YBOARD_ROW_PART(y, parts)	(((y) >> 4) % (parts))
static void YBoardDrawSpriteBand(INT32 part, INT32 parts);
static void System16RotateDrawScaledRows(INT32 y0, INT32 y1);

#define YBOARD_JOB_SPRITES	1
#define YBOARD_JOB_ROTATE	2
static volatile INT32 YBoardJob = 0, YBoardJobY0 = 0, YBoardJobY1 = 0;
static INT32 YBoardThreadsOn = 0;

static void YBoardThreadJob()
{
	if (YBoardJob == YBOARD_JOB_SPRITES)
		YBoardDrawSpriteBand(YBoardJobY0, YBoardJobY1);		/* part, parts */
	else if (YBoardJob == YBOARD_JOB_ROTATE)
		System16RotateDrawScaledRows(YBoardJobY0, YBoardJobY1);
}

// Start the job (rows [y0, y1), or part y0 of y1 for the sprites) on the
// second thread; false if there is none.
static bool YBoardThreadsStart(INT32 job, INT32 y0, INT32 y1)
{
#if (THREADY == THREADY_PTHREAD) || (THREADY == THREADY_PTHREADAPL) || (THREADY == THREADY_WINDOWS) || (THREADY == THREADY_WINDOWSVC)
	if (!YBoardThreadsOn) {
		thready.init(YBoardThreadJob);
		YBoardThreadsOn = 1;
	}
	YBoardJob = job; YBoardJobY0 = y0; YBoardJobY1 = y1;
	thready.notify();		/* (runs the job right here if the thread couldn't be made) */
	return true;
#else
	(void)job; (void)y0; (void)y1;
	return false;
#endif
}

static void YBoardThreadsWait()
{
	thready.notify_wait();
}

static void OutrunScaledExit();

// Called from System16Exit().
void System16GfxThreadsExit()
{
	OutrunScaledExit();
	if (YBoardThreadsOn) {
		thready.exit();
		YBoardThreadsOn = 0;
	}
	YBoardLodFree(YBoardLodChild);
	YBoardLodFree(YBoardLodParent);

	/* the next game's buffers and ROMs may be allocated at the same addresses
	   as this game's: what was known about them must not carry over */
	YBoardDirtyBuffer = NULL;
	YBoardDirtyScale = 0;
	YBoardLodCacheRom = NULL;
}

// The visible pixels a sprite shows in all the rows it reads (rows read as the
// hardware does, cut by YBoardRowWordLimit), counted up to most + 1. A sprite
// with only one or two visible pixels in all is the game drawing art that isn't
// there: e.g. a distant car's shadow pointed into unrelated art (2:c009) shows
// as a lone white pixel or two (the shadow's colour table maps 0xc to white).
// Real objects that small are drawn from their larger art at high resolution
// (see YBoardFindLargerArt), so such sprites are left out there.
static INT32 YBoardVisiblePixels(const UINT64 *spritedata, UINT16 addr, INT32 pitch, INT32 flip, INT32 rows, const UINT16 *indirect, INT32 most)
{
	const INT32 rowwords = (pitch < 0) ? -pitch : pitch;
	INT32 visible = 0;
	for (INT32 r = 0; r < rows; r++) {
		const UINT16 rowaddr = addr + pitch * r;
		const INT32 maxwords = YBoardRowWordLimit(spritedata, rowaddr, flip, rowwords, indirect);
		for (INT32 n = 0; n < 256 && n < maxwords; n++) {
			UINT64 pixels = spritedata[(UINT16)(flip ? (rowaddr - n) : (rowaddr + n))];
#ifndef LSB_FIRST
			pixels = BURN_ENDIAN_SWAP_INT64(pixels);
#endif
			for (INT32 shift = 0; shift <= 60; shift += 4)
				if (BURN_ENDIAN_SWAP_INT16(indirect[(pixels >> shift) & 0xf]) < 0x1fe) visible++;
			if (visible > most) return visible;
			if (((flip ? (pixels >> 60) : pixels) & 0xf) == 0xf) break;	/* the nibble read last */
		}
	}
	return visible;
}

// The rows added above/below a sprite drawn from larger art (see below) are
// outside what the game drew. If later sprites hide all of the sprite's own
// pixels, the game shows nothing of it, but the added rows could still show
// (e.g. a distant sign placed just behind the edge of the sea: its added top
// border peeked out above the sea). So the added pixels and the sprite's own
// pixels are recorded, and after all sprites are drawn, the added pixels of a
// sprite none of whose own pixels are still visible are taken out again.
// (Only sprites with up to YBOARD_OWN_MAX own pixels are checked: those are
// the distant ones; a big sprite is never hidden that exactly.)
// Each of the two drawing threads (see YBoardDrawSpriteBand) records the
// pixels in its own rows; the check runs once both are done.
struct YBoardAddedPixel {
	INT32 pos;					// offset in pTempDraw
	UINT16 old, value;			// before and after the added pixel was drawn
};
struct YBoardOwnPixel {
	INT32 pos;
	UINT16 value;
};
struct YBoardTrackedSprite {
	INT32 added0, added1;		// its added pixels [added0, added1)
	INT32 own0, own1;			// its own pixels recorded [own0, own1)
	INT32 owntotal;				// its own pixels drawn (recorded or not)
	INT32 on;					// tracked this frame (it has added rows)
};
#define YBOARD_ADDED_MAX	0x10000		/* per thread; beyond these, added pixels are just drawn */
#define YBOARD_OWN_MAX		1024		/* own pixels per sprite */
static YBoardAddedPixel YBoardAdded[2][YBOARD_ADDED_MAX];
static YBoardOwnPixel YBoardOwn[2][YBOARD_ADDED_MAX];
static YBoardTrackedSprite YBoardTracked[2][0x1000];	/* by thread and sprite entry */
static INT32 YBoardAddedCount[2] = { 0, 0 }, YBoardOwnCount[2] = { 0, 0 };

// draw a pixel of a tracked sprite (t: the sprite's record in this thread)
static inline void YBoardTrackPixel(INT32 part, YBoardTrackedSprite &t, bool added, UINT16 *p, UINT16 value)
{
	const INT32 pos = (INT32)(p - pTempDraw);
	if (added) {
		if (YBoardAddedCount[part] < YBOARD_ADDED_MAX) {
			YBoardAddedPixel &e = YBoardAdded[part][YBoardAddedCount[part]++];
			e.pos = pos; e.old = *p; e.value = value;
		}
	} else {
		if (t.owntotal < YBOARD_OWN_MAX && YBoardOwnCount[part] < YBOARD_ADDED_MAX) {
			YBoardOwnPixel &e = YBoardOwn[part][YBoardOwnCount[part]++];
			e.pos = pos; e.value = value;
		} else
			t.owntotal = YBOARD_OWN_MAX + 1;	/* too many to check (or no room) */
		t.owntotal++;
	}
	*p = value;
}

// High resolution version of YBoardRenderSpriteLayer() for System16ResScale > 1.
// The sprite framebuffer pTempDraw is (512 * S) x (512 * S). Positions are
// multiplied by S and the zoom accumulators count to 0x200 * S instead of 0x200,
// so every source pixel produces S times as many framebuffer pixels in each
// direction. Shrunk sprites therefore skip fewer source pixels and keep more
// detail; at 1:1 zoom each source pixel becomes an exact S x S block. The rotate
// RAM clip values and flags are looked up by native row and scaled.
// per thread: which source pixel each framebuffer pixel of a sprite's row
// shows, and the source pixels of the row being drawn
#define YBOARD_ROW_MAX	4096
static INT16 YBoardPixelSource[2][YBOARD_ROW_MAX];
static UINT16 YBoardRowSource[2][YBOARD_ROW_MAX];

static void YBoardDrawSpriteBand(INT32 part, INT32 parts)
{
	const INT32 S = System16ResScale;
	const INT32 W = 512 * S;		// framebuffer width and height
	const INT32 ONE = 0x200 * S;	// zoom accumulator unit (0x200 = one source pixel natively)
	const INT32 onebits = (S == 4) ? 11 : 10;	// ONE = 1 << onebits (S is 2 or 4)

	UINT8 numbanks = System16Sprite2RomSize / 0x80000;
	const UINT64 *spritebase = (const UINT64 *)System16Sprites2;
	const UINT16 *rotatebase = (UINT16*)System16RotateRamBuff;
	UINT8 visited[0x1000];
	UINT16 *data;
	INT32 next = 0;

	/* reset the visited list */
	memset(visited, 0, sizeof(visited));

	/* the part of the framebuffer the rotation stage will show this frame */
	const INT32 viewX0 = YBoardViewX0, viewX1 = YBoardViewX1, viewY0 = YBoardViewY0, viewY1 = YBoardViewY1;

	/* only the part of each row that was drawn since it was last cleared needs clearing */
	for (INT32 y = 0; y <= 511; y++)
		if (!(BURN_ENDIAN_SWAP_INT16(rotatebase[y & ~1]) & 0xc000))
			for (INT32 ys = y * S; ys < y * S + S; ys++)
				if (YBOARD_ROW_PART(ys, parts) == part && YBoardDirtyLo[ys] <= YBoardDirtyHi[ys]) {
					memset(pTempDraw + ys * W + YBoardDirtyLo[ys], 0xff, (YBoardDirtyHi[ys] - YBoardDirtyLo[ys] + 1) * sizeof(UINT16));
					YBoardDirtyLo[ys] = (INT16)W;
					YBoardDirtyHi[ys] = -1;
				}

	for (data = (UINT16*)System16SpriteRam2; !(BURN_ENDIAN_SWAP_INT16(data[0]) & 0x8000) && !visited[next]; data = (UINT16*)System16SpriteRam2 + next * 8)
	{
		INT32 hide    = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x5000);
		UINT16 *indirect = (UINT16*)System16SpriteRam2 + ((BURN_ENDIAN_SWAP_INT16(data[0]) & 0x7ff) << 4);
		INT32 bank    = ((BURN_ENDIAN_SWAP_INT16(data[1]) >> 8) & 0x10) | ((BURN_ENDIAN_SWAP_INT16(data[2]) >> 12) & 0x0f);
		INT32 xpos    = (BURN_ENDIAN_SWAP_INT16(data[1]) & 0xfff) - 0x600;
		INT32 top     = (BURN_ENDIAN_SWAP_INT16(data[2]) & 0xfff) - 0x600;
		UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[3]);
		INT32 height  = BURN_ENDIAN_SWAP_INT16(data[4]);
		INT32 ydelta  = (BURN_ENDIAN_SWAP_INT16(data[5]) & 0x4000) ? 1 : -1;
		INT32 flip    = (~(BURN_ENDIAN_SWAP_INT16(data[5])) >> 13) & 1;
		INT32 xdelta  = (BURN_ENDIAN_SWAP_INT16(data[5]) & 0x1000) ? 1 : -1;
		INT32 zoom    = BURN_ENDIAN_SWAP_INT16(data[5]) & 0x7ff;
		INT32 colorpri= (BURN_ENDIAN_SWAP_INT16(data[6]) << 1) & 0xfe00;
		INT32 pitch   = (INT8)BURN_ENDIAN_SWAP_INT16(data[6]);
		const UINT64 *spritedata;

		/* note that we've visited this entry and get the offset of the next one */
		const INT32 entry = next;
		visited[next] = 1;
		next = BURN_ENDIAN_SWAP_INT16(data[7]) & 0xfff;

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (hide || height == 0 || YBoardFrameSkip[entry])
			continue;

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x10000 * bank;

		/* clamp to a maximum of 8x (not 100% confirmed) */
		if (zoom == 0) zoom = 1;

		/* native rows top .. top + ydelta * (height - 1) cover these scaled rows */
		INT32 y = (ydelta > 0) ? (top * S) : (top * S + S - 1);
		const INT32 xstart = (xdelta > 0) ? (xpos * S) : (xpos * S + S - 1);

		/* Native row i shows source row (i * zoom) >> 9, so the last source row the
		   hardware ever reads is ((height - 1) * zoom) >> 9. The finer rows here
		   step in between and could step past it on the last native row, into
		   data that isn't part of this sprite, so they are clamped to it. */
		const INT32 lastsrcrow = (INT32)(((INT64)(height - 1) * zoom) >> 9);

		const INT32 rowwords = (pitch < 0) ? -pitch : pitch;

		/* If this is a reduced copy of larger art, draw the larger art instead
		   (see YBoardFindLargerArt): 2^levels times the pixels, so 2^levels
		   times the zoom. Large-image pixel u = 2^levels * (drawn-image pixel) + C,
		   and the same for rows with D. For each row, the span of the large
		   image that the drawn sprite's row came from is drawn pixel by pixel:
		   where the sprite's row ends (its marker) can differ from row to row,
		   and the span can cover several of the large image's strips. */
		INT32 drzoom = zoom, drlastrow = lastsrcrow;
		INT32 k = 1;
		INT64 rowbias = 0;		/* subtracted from r * drzoom before dividing by ONE */
		/* (looked up for all sprites before the bands are drawn) */
		const YBoardLargerArt &art = YBoardFrameArt[entry];
		const bool larger = art.levels > 0;
		if (larger) {
			k = 1 << art.levels;
			drzoom = zoom << art.levels;
			drlastrow = art.rows - 1;

			/* (the sprite may read art.skip rows before the image starts) */
			const INT64 tnum = (pitch > 0) ? (INT64)(-art.D) : ((INT64)art.childrows * k - art.rows + art.D);
			rowbias = (tnum + (INT64)art.skip * k) * ONE;
		}

		/* Where the sprite shows the whole small image, draw the large image's
		   rows just outside it too: if the rows read include the small image's
		   first (last) row, the large rows before (after) the matching one are
		   drawn; otherwise a frame or border the small copy leaves out (it
		   samples only every 2nd row) would be cut off. */
		/* At most k - 1 large rows are added at each end: that's all a 2:1
		   reduction can leave out, and the large image found may start or end
		   with rows of a neighbouring image that must not be drawn. */
		INT32 rbeg = 0, rend = height * S;
		INT32 extlo = 0, exthi = 0;		/* large rows allowed in the added rows */
		if (larger) {
			const INT64 ext = ((INT64)(k - 1) * ONE + drzoom - 1) / drzoom;		/* k - 1 large rows, in r (rounded up; limited exactly below) */
			if (art.skip >= 0) {
				INT64 rA = (rowbias > 0) ? ((rowbias + drzoom - 1) / drzoom) : -((-rowbias) / drzoom);	/* large row 0 */
				const INT64 rc = ((INT64)art.skip * ONE + zoom - 1) / zoom;		/* small image row 0 */
				if (rA < rc - ext) rA = rc - ext;
				if (rA < 0) rbeg = (INT32)rA;
			}
			if (art.skip + art.childrows <= lastsrcrow + 1) {
				const INT64 num = (INT64)(drlastrow + 1) * ONE + rowbias;
				INT64 rB = (num > 0) ? ((num + drzoom - 1) / drzoom) : 0;			/* past the last large row */
				const INT64 rc = ((INT64)(art.skip + art.childrows) * ONE + zoom - 1) / zoom;	/* past the small image */
				if (rB > rc + ext) rB = rc + ext;
				if (rB > rend) rend = (INT32)rB;
			}
			y += ydelta * rbeg;

			/* the large rows of the small image's first and last rows, +- (k - 1) */
			const INT64 rc0 = ((INT64)art.skip * ONE + zoom - 1) / zoom;
			const INT64 rc1 = ((INT64)(art.skip + art.childrows) * ONE + zoom - 1) / zoom - 1;
			const INT64 n0 = rc0 * drzoom - rowbias, n1 = rc1 * drzoom - rowbias;
			extlo = (INT32)((n0 >= 0) ? (n0 / ONE) : -((-n0 + ONE - 1) / ONE)) - (k - 1);
			exthi = (INT32)((n1 >= 0) ? (n1 / ONE) : -((-n1 + ONE - 1) / ONE)) + (k - 1);
		}

		/* record the added pixels and the sprite's own ones (see YBoardAddedPixel) */
		YBoardTrackedSprite &trk = YBoardTracked[part][entry];
		bool track = larger && (rbeg < 0 || rend > height * S);
		trk.on = track;
		trk.added0 = trk.added1 = YBoardAddedCount[part];
		trk.own0 = trk.own1 = YBoardOwnCount[part];
		trk.owntotal = 0;

		/* what the last row drawn depended on, and how many source pixels it read */
		bool keyvalid = false;
		INT32 kA = 0, kB = 0, kC = 0, kD = 0, kMin = 0, kMax = 0, kPend = 0;
		/* framebuffer value of each pen (0xffff: transparent) */
		UINT16 lut[16];
		for (INT32 i = 0; i < 16; i++) {
			const UINT32 ind = BURN_ENDIAN_SWAP_INT16(indirect[i]);
			lut[i] = (UINT16)((ind < 0x1fe) ? (ind | colorpri) : 0xffff);
		}
		INT32 pidxN = 0;		/* YBoardPixelSource[part][j] is set for j < pidxN */

		for (INT32 r = rbeg; r < rend; r++, y += ydelta)
		{
			/* nothing is drawn outside the framebuffer; a row marked as above
			   (below) the screen ends the sprite, whichever band it is in */
			if (y < 0 || y >= W)
				continue;
			{
				const INT32 frow = y / S;
				const INT32 flags = BURN_ENDIAN_SWAP_INT16(rotatebase[frow & ~1]);
				if ((flags & 0x8000) && ydelta < 0)
					break;
				if ((flags & 0x4000) && ydelta > 0)
					break;
			}

			/* only this part's rows, and only those the rotation stage shows */
			if (YBOARD_ROW_PART(y, parts) != part || y < viewY0 || y > viewY1)
				continue;

			INT32 srcrow;
			bool rowvalid = true;
			const bool addedrow = r < 0 || r >= height * S;
			/* a sprite with more own pixels than are checked is never taken out,
			   so the rest of it needn't be recorded */
			if (track && trk.owntotal > YBOARD_OWN_MAX) track = false;
			if (larger) {
				const INT64 num = (INT64)r * drzoom - rowbias;
				srcrow = (INT32)((num >= 0) ? (num / ONE) : -((-num + ONE - 1) / ONE));
				if (srcrow < 0 || srcrow > drlastrow) rowvalid = false;
				if ((r < 0 || r >= height * S) && (srcrow < extlo || srcrow > exthi)) rowvalid = false;
			} else {
				srcrow = (INT32)(((INT64)r * zoom) / ONE);
				if (srcrow > lastsrcrow) srcrow = lastsrcrow;
			}
			const UINT16 rowaddr = addr + pitch * srcrow;
			/* large image: first word of the row (rows are counted in drawing order) */
			const INT32 lrow = larger ? (art.start + art.width * ((pitch > 0) ? srcrow : (art.rows - 1 - srcrow))) : 0;

			/* large image: the columns [u0, u1) to draw on this row, and how far
			   along xdelta the first of them goes (scaled pixels) */
			INT32 u0 = 0, u1 = 0, xshift = 0;
			if (larger && rowvalid) {
				/* the drawn sprite's row that this one corresponds to */
				INT32 crow = (INT32)(((INT64)r * zoom) / ONE) - art.skip;
				if (crow < 0) crow = 0;
				if (crow > art.childrows - 1) crow = art.childrows - 1;
				const INT32 cstart = art.childstart + art.childwidth * ((pitch > 0) ? crow : (art.childrows - 1 - crow));

				/* where the hardware would stop reading it: the strip [w0, w1) in words */
				INT32 w0 = art.pos, w1 = art.pos + 1;
				for (INT32 n = 0; n < art.childwidth; n++) {
					const INT32 w = flip ? (art.pos - n) : (art.pos + n);
					if (w < 0 || w >= art.childwidth) break;
					if (flip) w0 = w; else w1 = w + 1;
					if (YBoardSpriteNibble(spritedata, cstart, w * 16 + (flip ? 0 : 15)) == 0xf) break;
				}

				const INT32 a0 = k * w0 * 16 + art.C;	/* the strip's edges in large-image pixels */
				const INT32 a1 = k * w1 * 16 + art.C;
				u0 = (a0 < 0) ? 0 : a0;
				u1 = (a1 > 16 * art.width) ? 16 * art.width : a1;
				/* a strip edge that is the small image's edge: draw the large image
				   up to k - 1 pixels further, as for the rows */
				if (w0 == 0) { u0 = a0 - (k - 1); if (u0 < 0) u0 = 0; }
				if (w1 == art.childwidth) { u1 = a1 + (k - 1); if (u1 > 16 * art.width) u1 = 16 * art.width; }

				/* large-image pixels between the strip's first drawn edge and the first one drawn */
				const INT64 lead = flip ? (INT64)(a1 - u1) : (INT64)(u0 - a0);
				xshift = (INT32)((lead * ONE + drzoom / 2) / drzoom);
			}

			/* skip drawing if not within the cliprect */
			if (y >= 0 && y < W)
			{
				const INT32 row = y / S;	/* native framebuffer row */
				INT32 minx = BURN_ENDIAN_SWAP_INT16(rotatebase[row & ~1]);
				INT32 maxx = BURN_ENDIAN_SWAP_INT16(rotatebase[row |  1]);

				/* bit 0x8000 from rotate RAM means that Y is above the top of the screen */
				if ((minx & 0x8000) && ydelta < 0)
					break;

				/* bit 0x4000 from rotate RAM means that Y is below the bottom of the screen */
				if ((minx & 0x4000) && ydelta > 0)
					break;

				/* if either bit is set, skip the rest for this scanline (and rows
				   the rotation stage doesn't show) */
				if (!(minx & 0xc000) && rowvalid && y >= viewY0 && y <= viewY1)
				{
					/* clamp min/max to the cliprect, then scale */
					minx -= 0x600;
					maxx -= 0x600;
					if (minx < 0)
						minx = 0;
					if (maxx > 511)
						maxx = 511;
					minx = minx * S;
					maxx = maxx * S + S - 1;

					/* Source pixel p (the p-th pixel read) is drawn on framebuffer
					   pixels x0 + xdelta * j for j in [E(p), E(p + 1)), E(p) =
					   ceil(p * ONE / step): exactly what the hardware's zoom
					   accumulator does. So framebuffer pixel j shows source pixel
					   (j * step) / ONE (ONE is a power of 2). First work out how many
					   source pixels are read (pend), then draw the framebuffer pixels
					   from them, skipping source pixels that aren't shown at all. */
					const INT32 x0 = xstart + xdelta * xshift;
					const INT32 step = larger ? drzoom : zoom;
					INT32 pend = 0;

					/* the next scaled rows often show the same source row with the same
					   clip: then pend is the same too */
					const INT32 key = larger ? lrow : (INT32)rowaddr;
					if (keyvalid && key == kA && u0 == kB && u1 == kC && xshift == kD && minx == kMin && maxx == kMax)
						pend = kPend;
					else if (larger)
					{
						/* Like the hardware, the clip limit is only checked before each word
						   of the drawn sprite (a word that's started is drawn to its end),
						   i.e. after the large-image pixels next to C + 16k * n. */
						const INT32 wordpx = 16 * k;
						const INT32 count = u1 - u0;
						if ((xdelta > 0) ? (xstart <= maxx) : (xstart >= minx)) {
							pend = count;
							/* the first i whose pixel ends a word, then every wordpx */
							const INT32 e0 = (flip ? (u1 - 1) : (u0 + 1)) - art.C;	/* edge value at i = 0 */
							const INT32 e0mod = ((e0 % wordpx) + wordpx) % wordpx;
							for (INT32 i = flip ? e0mod : ((wordpx - e0mod) % wordpx); i < count; i += wordpx) {
								const INT32 xe = x0 + xdelta * YBoardRunStart(i + 1, ONE, step);	/* x after pixel i */
								if ((xdelta > 0) ? (xe > maxx) : (xe < minx)) { pend = i + 1; break; }
							}
						}
					}
					else
					{
						/* words are read from rowaddr on (backward when flipped) until one
						   ends with 0xf, while the clip limit hasn't been passed */
						const INT32 maxwords = YBoardRowWordLimit(spritedata, rowaddr, flip, rowwords, indirect);
						for (INT32 w = 0; w < maxwords; w++) {
							const INT32 xw = x0 + xdelta * YBoardRunStart(16 * w, ONE, step);	/* x at the word's first pixel */
							if ((xdelta > 0) ? (xw > maxx) : (xw < minx)) break;
							pend = 16 * (w + 1);
							UINT64 pixels = spritedata[(UINT16)(flip ? (rowaddr - w) : (rowaddr + w))];
#ifndef LSB_FIRST
							pixels = BURN_ENDIAN_SWAP_INT64(pixels);
#endif
							if (((flip ? (pixels >> 60) : pixels) & 0xf) == 0xf) break;	/* its last pixel is 0xf */
						}
					}

					keyvalid = true;
					kA = key; kB = u0; kC = u1; kD = xshift; kMin = minx; kMax = maxx; kPend = pend;

					/* framebuffer pixels j = 0 .. E(pend) - 1, within the framebuffer */
					INT32 j0 = 0, j1 = YBoardRunStart(pend, ONE, step);
					/* (and within the part the rotation stage shows) */
					if (xdelta > 0) { if (j0 < viewX0 - x0) j0 = viewX0 - x0; if (j1 > viewX1 + 1 - x0) j1 = viewX1 + 1 - x0; }
					else { if (j0 < x0 - viewX1) j0 = x0 - viewX1; if (j1 > x0 - viewX0 + 1) j1 = x0 - viewX0 + 1; }

					if (j0 < j1)
					{
						UINT16 *pRow = pTempDraw + (y * W);
						INT32 drawn0 = W, drawn1 = -1;		/* first and last j drawn */
						INT16 *pidx = YBoardPixelSource[part];
						UINT16 *src = YBoardRowSource[part];
						if (step < ONE && !track && j1 <= YBOARD_ROW_MAX && (INT32)(((INT64)(j1 - 1) * step) >> onebits) < YBOARD_ROW_MAX)
						{
							/* enlarged: framebuffer pixel j shows source pixel pidx[j] (the
							   same on every row of the sprite), so the source pixels shown
							   are read into src[] first, then the framebuffer pixels are
							   filled from them */
							for (; pidxN < j1; pidxN++) pidx[pidxN] = (INT16)(((INT64)pidxN * step) >> onebits);
							const INT32 pa = pidx[j0], pb = pidx[j1 - 1];
							INT32 first = -1, last = -1;		/* the first and last of them not transparent */
							if (larger) {
								/* large image: its pixels are read one after the other */
								const INT32 cdir = flip ? -1 : 1;
								INT32 c = flip ? (u1 - 1 - pa) : (u0 + pa);
								INT32 cword = -1;
								UINT64 cbits = 0;
								for (INT32 p = pa; p <= pb; p++, c += cdir) {
									if ((c >> 4) != cword) {
										cword = c >> 4;
										cbits = spritedata[(lrow + cword) & 0xffff];
#ifndef LSB_FIRST
										cbits = BURN_ENDIAN_SWAP_INT64(cbits);
#endif
									}
									const UINT16 value = lut[(cbits >> (60 - 4 * (c & 15))) & 0xf];
									src[p] = value;
									if (value != 0xffff) { if (first < 0) first = p; last = p; }
								}
							} else {
								for (INT32 p = pa; p <= pb; p++) {
									const UINT16 value = (UINT16)YBoardSourcePixel(spritedata, larger, lrow, u0, u1, rowaddr, flip, p, indirect, colorpri);
									src[p] = value;
									if (value != 0xffff) { if (first < 0) first = p; last = p; }
								}
							}
							if (first >= 0) {
								INT32 ja = YBoardRunStart(first, ONE, step), jb = YBoardRunStart(last + 1, ONE, step);
								if (ja < j0) ja = j0;
								if (jb > j1) jb = j1;
								if (xdelta > 0) { UINT16 *d = pRow + x0; for (INT32 j = ja; j < jb; j++) { const UINT16 v = src[pidx[j]]; if (v != 0xffff) d[j] = v; } }
								else { UINT16 *d = pRow + x0; for (INT32 j = ja; j < jb; j++) { const UINT16 v = src[pidx[j]]; if (v != 0xffff) d[-j] = v; } }
								drawn0 = ja; drawn1 = jb - 1;
							}
						}
						else if (step < ONE)
						{
							/* enlarged, one run of framebuffer pixels per source pixel (the
							   pixels drawn are recorded): xacc is the hardware's accumulator,
							   E(p) * step - p * ONE, always in [0, step), so each run is q or
							   q + 1 long */
							const INT32 q = ONE / step, qstep = q * step;
							INT32 p = (INT32)(((INT64)j0 * step) >> onebits);
							INT32 e = YBoardRunStart(p, ONE, step);
							INT32 xacc = (INT32)((INT64)e * step - ((INT64)p << onebits));
							while (e < j1) {
								INT32 n = q;
								xacc += qstep;
								if (xacc < ONE) { xacc += step; n++; }
								xacc -= ONE;
								INT32 ja = e, jb = e + n;		/* the run [ja, jb) */
								e = jb;
								if (ja < j0) ja = j0;
								if (jb > j1) jb = j1;
								if (ja < jb) {
									const UINT32 value = YBoardSourcePixel(spritedata, larger, lrow, u0, u1, rowaddr, flip, p, indirect, colorpri);
									if (value != 0xffff) {
										if (track) { for (INT32 j = ja; j < jb; j++) YBoardTrackPixel(part, trk, addedrow, pRow + x0 + xdelta * j, (UINT16)value); }
										else if (xdelta > 0) { UINT16 *d = pRow + x0 + ja; for (INT32 j = ja; j < jb; j++) *d++ = (UINT16)value; }
										else { UINT16 *d = pRow + x0 - ja; for (INT32 j = ja; j < jb; j++) *d-- = (UINT16)value; }
										if (ja < drawn0) drawn0 = ja;
										drawn1 = jb - 1;
									}
								}
								p++;
							}
						}
						else
						{
							/* shrunk: most framebuffer pixels show a different source pixel */
							INT32 lastp = -1;
							UINT32 value = 0xffff;
							INT64 acc = (INT64)j0 * step;
							INT32 cword = -1;
							UINT64 cbits = 0;
							for (INT32 j = j0; j < j1; j++, acc += step) {
								const INT32 p = (INT32)(acc >> onebits);
								if (p != lastp) {
									lastp = p;
									if (larger) {
										/* large image: the word holding column c is kept */
										const INT32 c = flip ? (u1 - 1 - p) : (u0 + p);
										if ((c >> 4) != cword) {
											cword = c >> 4;
											cbits = spritedata[(lrow + cword) & 0xffff];
#ifndef LSB_FIRST
											cbits = BURN_ENDIAN_SWAP_INT64(cbits);
#endif
										}
										const UINT32 ind = BURN_ENDIAN_SWAP_INT16(indirect[(cbits >> (60 - 4 * (c & 15))) & 0xf]);
										value = (ind < 0x1fe) ? (ind | colorpri) : 0xffff;
									}
									else
										value = YBoardSourcePixel(spritedata, larger, lrow, u0, u1, rowaddr, flip, p, indirect, colorpri);
								}
								if (value != 0xffff) {
									if (track) YBoardTrackPixel(part, trk, addedrow, pRow + x0 + xdelta * j, (UINT16)value);
									else pRow[x0 + xdelta * j] = (UINT16)value;
									if (j < drawn0) drawn0 = j;
									drawn1 = j;
								}
							}
						}
						if (drawn1 >= 0) {
							INT32 lo = x0 + xdelta * drawn0, hi = x0 + xdelta * drawn1;
							if (lo > hi) { const INT32 t = lo; lo = hi; hi = t; }
							if (lo < YBoardDirtyLo[y]) YBoardDirtyLo[y] = (INT16)lo;
							if (hi > YBoardDirtyHi[y]) YBoardDirtyHi[y] = (INT16)hi;
						}
					}
				}
			}
		}

		trk.added1 = YBoardAddedCount[part];
		trk.own1 = YBoardOwnCount[part];
	}
}


// The Y-Board sprite list and the rotate RAM are only read while the sprites
// are drawn, so two threads can draw the framebuffer at the same time: each
// draws every sprite in list order, but only into its own rows, so the result
// is the same as drawing it all at once. Anything one would change that the
// other reads is prepared beforehand: the larger-art lookups (their caches)
// and the view rectangle.
static void YBoardRenderSpriteLayerScaled()
{
	const INT32 S = System16ResScale;
	const INT32 W = 512 * S;

	YBoardRotateView(S, &YBoardViewX0, &YBoardViewX1, &YBoardViewY0, &YBoardViewY1);

	if (YBoardDirtyBuffer != pTempDraw || YBoardDirtyScale != S) {
		for (INT32 y = 0; y < W; y++) { YBoardDirtyLo[y] = 0; YBoardDirtyHi[y] = (INT16)(W - 1); }
		YBoardDirtyBuffer = pTempDraw;
		YBoardDirtyScale = S;
	}

	/* look up the larger art of every sprite drawn, in list order */
	INT32 order[0x1000], orders = 0;		/* the sprites drawn, in list order */
	{
		UINT8 numbanks = System16Sprite2RomSize / 0x80000;
		const UINT64 *spritebase = (const UINT64 *)System16Sprites2;
		UINT8 visited[0x1000];
		UINT16 *data;
		INT32 next = 0;
		memset(visited, 0, sizeof(visited));
		for (data = (UINT16*)System16SpriteRam2; !(BURN_ENDIAN_SWAP_INT16(data[0]) & 0x8000) && !visited[next]; data = (UINT16*)System16SpriteRam2 + next * 8)
		{
			const INT32 entry = next;
			visited[next] = 1;
			next = BURN_ENDIAN_SWAP_INT16(data[7]) & 0xfff;
			YBoardFrameArt[entry].levels = 0;
			YBoardFrameSkip[entry] = 0;
			YBoardTracked[0][entry].on = YBoardTracked[1][entry].on = 0;

			const INT32 hide = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x5000);
			const INT32 height = BURN_ENDIAN_SWAP_INT16(data[4]);
			if (hide || height == 0)
				continue;
			INT32 bank = ((BURN_ENDIAN_SWAP_INT16(data[1]) >> 8) & 0x10) | ((BURN_ENDIAN_SWAP_INT16(data[2]) >> 12) & 0x0f);
			if (numbanks)
				bank %= numbanks;
			INT32 zoom = BURN_ENDIAN_SWAP_INT16(data[5]) & 0x7ff;
			if (zoom == 0) zoom = 1;
			const INT32 flip  = (~(BURN_ENDIAN_SWAP_INT16(data[5])) >> 13) & 1;
			const INT32 pitch = (INT8)BURN_ENDIAN_SWAP_INT16(data[6]);
			const UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[3]);
			const INT32 lastsrcrow = (INT32)(((INT64)(height - 1) * zoom) >> 9);
			YBoardFindLargerArt(spritebase + 0x10000 * bank, bank, addr, pitch, flip, lastsrcrow + 1, zoom, &YBoardFrameArt[entry]);
			if (YBoardFrameArt[entry].levels == 0) {
				const UINT16 *indirect = (UINT16*)System16SpriteRam2 + ((BURN_ENDIAN_SWAP_INT16(data[0]) & 0x7ff) << 4);
				if (YBoardVisiblePixels(spritebase + 0x10000 * bank, addr, pitch, flip, lastsrcrow + 1, indirect, 2) <= 2)
					YBoardFrameSkip[entry] = 1;		/* see YBoardVisiblePixels */
			}
			order[orders++] = entry;
		}
	}
	YBoardAddedCount[0] = YBoardAddedCount[1] = 0;
	YBoardOwnCount[0] = YBoardOwnCount[1] = 0;

	/* the rows are dealt out to the two threads in alternate groups of 16, so
	   both get about the same amount of work wherever the sprites are */
	if (YBoardThreadsStart(YBOARD_JOB_SPRITES, 1, 2)) {
		YBoardDrawSpriteBand(0, 2);
		YBoardThreadsWait();
	} else {
		YBoardDrawSpriteBand(0, 1);
	}

	/* take out the added pixels of sprites that are hidden entirely, last drawn first */
	for (INT32 i = orders - 1; i >= 0; i--) {
		const INT32 entry = order[i];
		const YBoardTrackedSprite &t0 = YBoardTracked[0][entry], &t1 = YBoardTracked[1][entry];
		const bool added = (t0.on && t0.added1 > t0.added0) || (t1.on && t1.added1 > t1.added0);
		if (!added) continue;
		if ((t0.on ? t0.owntotal : 0) + (t1.on ? t1.owntotal : 0) > YBOARD_OWN_MAX) continue;	/* too many to check */
		bool visible = false;
		for (INT32 part = 0; part < 2 && !visible; part++) {
			const YBoardTrackedSprite &t = YBoardTracked[part][entry];
			if (!t.on) continue;
			for (INT32 j = t.own0; j < t.own1 && !visible; j++)
				if (pTempDraw[YBoardOwn[part][j].pos] == YBoardOwn[part][j].value) visible = true;
		}
		if (visible) continue;
		for (INT32 part = 0; part < 2; part++) {
			const YBoardTrackedSprite &t = YBoardTracked[part][entry];
			if (!t.on) continue;
			for (INT32 j = t.added1 - 1; j >= t.added0; j--)
				if (pTempDraw[YBoardAdded[part][j].pos] == YBoardAdded[part][j].value) pTempDraw[YBoardAdded[part][j].pos] = YBoardAdded[part][j].old;
		}
	}
}
/*====================================================
Road Rendering
====================================================*/

static void OutrunRenderRoadBackgroundLayer()
{
	UINT16 *roadram = (UINT16*)System16RoadRam;
	INT32 x, y;
	
	for (y = 0; y < 224; y++) {
		UINT16* pPixel = pTransDraw + (y * 320);
		INT32 data0 = BURN_ENDIAN_SWAP_INT16(roadram[0x000 + y]);
		INT32 data1 = BURN_ENDIAN_SWAP_INT16(roadram[0x100 + y]);
		
		INT32 color = -1;
		
		switch (System16RoadControl & 3) {
			case 0: {
				if (data0 & 0x800) color = data0 & 0x7f;
				break;
			}

			case 1: {
				if (data0 & 0x800) color = data0 & 0x7f;
				else if (data1 & 0x800) color = data1 & 0x7f;
				break;
			}

			case 2: {
				if (data1 & 0x800) color = data1 & 0x7f;
				else if (data0 & 0x800)	color = data0 & 0x7f;
				break;
			}

			case 3: {
				if (data1 & 0x800) color = data1 & 0x7f;
				break;
			}
		}

		if (color != -1) {
			for (x = 0; x < 320; x++) {
				pPixel[x] = color | System16RoadColorOffset3;
			}
		}
	}
}

static void OutrunRenderRoadForegroundLayer()
{
	UINT16 *roadram = (UINT16*)System16RoadRam;
	INT32 x, y;
	
	for (y = 0; y < 224; y++) {
		static const UINT8 priority_map[2][8] =	{
			{ 0x80,0x81,0x81,0x87,0,0,0,0x00 },
			{ 0x81,0x81,0x81,0x8f,0,0,0,0x80 }
		};
	
		UINT16* pPixel = pTransDraw + (y * 320);
		INT32 data0 = BURN_ENDIAN_SWAP_INT16(roadram[0x000 + y]);
		INT32 data1 = BURN_ENDIAN_SWAP_INT16(roadram[0x100 + y]);
		
		/* if both roads are low priority, skip */
		if ((data0 & 0x800) && (data1 & 0x800))	continue;
		
		INT32 hpos0, hpos1, color0, color1;
		INT32 control = System16RoadControl & 3;
		UINT16 color_table[32];
		UINT8 *src0, *src1;
		UINT8 bgcolor;

		/* get road 0 data */
		src0 = (data0 & 0x800) ? System16Roads + 256 * 2 * 512 : (System16Roads + (0x000 + ((data0 >> 1) & 0xff)) * 512);
		hpos0 = BURN_ENDIAN_SWAP_INT16((roadram[0x200 + ((System16RoadControl & 4) ? y : (data0 & 0x1ff))])) & 0xfff;
		color0 = BURN_ENDIAN_SWAP_INT16(roadram[0x600 + ((System16RoadControl & 4) ? y : (data0 & 0x1ff))]);

		/* get road 1 data */
		src1 = (data1 & 0x800) ? System16Roads + 256 * 2 * 512 : (System16Roads + (0x100 + ((data1 >> 1) & 0xff)) * 512);
		hpos1 = BURN_ENDIAN_SWAP_INT16((roadram[0x400 + ((System16RoadControl & 4) ? (0x100 + y) : (data1 & 0x1ff))])) & 0xfff;
		color1 = BURN_ENDIAN_SWAP_INT16(roadram[0x600 + ((System16RoadControl & 4) ? (0x100 + y) : (data1 & 0x1ff))]);

		/* determine the 5 colors for road 0 */
		color_table[0x00] = System16RoadColorOffset1 ^ 0x00 ^ ((color0 >> 0) & 1);
		color_table[0x01] = System16RoadColorOffset1 ^ 0x02 ^ ((color0 >> 1) & 1);
		color_table[0x02] = System16RoadColorOffset1 ^ 0x04 ^ ((color0 >> 2) & 1);
		bgcolor = (color0 >> 8) & 0xf;
		color_table[0x03] = (data0 & 0x200) ? color_table[0x00] : (System16RoadColorOffset2 ^ 0x00 ^ bgcolor);
		color_table[0x07] = System16RoadColorOffset1 ^ 0x06 ^ ((color0 >> 3) & 1);

		/* determine the 5 colors for road 1 */
		color_table[0x10] = System16RoadColorOffset1 ^ 0x08 ^ ((color1 >> 4) & 1);
		color_table[0x11] = System16RoadColorOffset1 ^ 0x0a ^ ((color1 >> 5) & 1);
		color_table[0x12] = System16RoadColorOffset1 ^ 0x0c ^ ((color1 >> 6) & 1);
		bgcolor = (color1 >> 8) & 0xf;
		color_table[0x13] = (data1 & 0x200) ? color_table[0x10] : (System16RoadColorOffset2 ^ 0x10 ^ bgcolor);
		color_table[0x17] = System16RoadColorOffset1 ^ 0x0e ^ ((color1 >> 7) & 1);

		/* draw the road */
		switch (control) {
			case 0: {
				if (data0 & 0x800) continue;
				hpos0 = (hpos0 - (0x5f8 + System16RoadXOffset)) & 0xfff;
				for (x = 0; x < 320; x++) {
					INT32 pix0 = (hpos0 < 0x200) ? src0[hpos0] : 3;
					pPixel[x] = color_table[0x00 + pix0];
					hpos0 = (hpos0 + 1) & 0xfff;
				}
				break;
			}

			case 1: {
				hpos0 = (hpos0 - (0x5f8 + System16RoadXOffset)) & 0xfff;
				hpos1 = (hpos1 - (0x5f8 + System16RoadXOffset)) & 0xfff;
				for (x = 0; x < 320; x++) {
					INT32 pix0 = (hpos0 < 0x200) ? src0[hpos0] : 3;
					INT32 pix1 = (hpos1 < 0x200) ? src1[hpos1] : 3;
					if ((priority_map[0][pix0] >> pix1) & 1) {
						pPixel[x] = color_table[0x10 + pix1];
					} else {
						pPixel[x] = color_table[0x00 + pix0];
					}
					hpos0 = (hpos0 + 1) & 0xfff;
					hpos1 = (hpos1 + 1) & 0xfff;
				}
				break;
			}

			case 2: {
				hpos0 = (hpos0 - (0x5f8 + System16RoadXOffset)) & 0xfff;
				hpos1 = (hpos1 - (0x5f8 + System16RoadXOffset)) & 0xfff;
				for (x = 0; x < 320; x++) {
					INT32 pix0 = (hpos0 < 0x200) ? src0[hpos0] : 3;
					INT32 pix1 = (hpos1 < 0x200) ? src1[hpos1] : 3;
					if ((priority_map[1][pix0] >> pix1) & 1) {
						pPixel[x] = color_table[0x10 + pix1];
					} else {
						pPixel[x] = color_table[0x00 + pix0];
					}
					hpos0 = (hpos0 + 1) & 0xfff;
					hpos1 = (hpos1 + 1) & 0xfff;
				}
				break;
			}

			case 3: {
				if (data1 & 0x800) continue;
				hpos1 = (hpos1 - (0x5f8 + System16RoadXOffset)) & 0xfff;
				for (x = 0; x < 320; x++) {
					INT32 pix1 = (hpos1 < 0x200) ? src1[hpos1] : 3;
					pPixel[x] = color_table[0x10 + pix1];
					hpos1 = (hpos1 + 1) & 0xfff;
				}
				break;
			}
		}
	}	
}

static void HangonRenderRoadLayer(INT32 priority)
{
	UINT16 *roadram = (UINT16*)System16RoadRam;
	INT32 x, y;
	
	for (y = 0; y <= 223; y++)
	{
		UINT16* pPixel = pTransDraw + (y * 320);
		INT32 control = BURN_ENDIAN_SWAP_INT16(roadram[0x000 + y]);
		INT32 ff9j1 = 0, ff9j2 = 0, ctr9m = 0, ctr9n9p = 0, ctr9n9p_ena = 0, ss8j = 0, plycont = 0;
		
		/* the PLYCONT signal controls the road layering */
		plycont = (control >> 10) & 3;
		
		if (plycont != priority) continue;
		
		INT32 hpos = BURN_ENDIAN_SWAP_INT16(roadram[0x100 + (control & 0xff)]);
		INT32 color0 = BURN_ENDIAN_SWAP_INT16(roadram[0x200 + (control & 0xff)]);
		INT32 color1 = BURN_ENDIAN_SWAP_INT16(roadram[0x300 + (control & 0xff)]);
		UINT8 *src;

		/* compute the offset of the road graphics for this line */
		src = System16Roads + (0x000 + (control & 0xff)) * 512;

		/* initialize the 4-bit counter at 9M, which counts bits within each road byte */
		ctr9m = hpos & 7;

		/* initialize the two 4-bit counters at 9P (low) and 9N (high), which count road data bytes */
		ctr9n9p = (hpos >> 3) & 0xff;

		/* initialize the flip-flop at 9J (lower half), which controls the counting direction */
		ff9j1 = (hpos >> 11) & 1;

		/* initialize the flip-flop at 9J (upper half), which controls the background color */
		ff9j2 = 1;

		/* initialize the serial shifter at 8S, which delays several signals after we flip */
		ss8j = 0;

		/* draw this scanline from the beginning */
		for (x = -24; x <= 319; x++)
		{
			INT32 md, color, select;

			/* ---- the following logic all happens constantly ---- */

			/* the enable is controlled by the value in the counter at 9M */
			ctr9n9p_ena = (ctr9m == 7);

			/* if we carried out of the 9P/9N counters, we will forcibly clear the flip-flop at 9J (lower half) */
			if ((ctr9n9p & 0xff) == 0xff)
				ff9j1 = 0;

			/* if the control word bit 8 is clear, we will forcibly set the flip-flop at 9J (lower half) */
			if (!(control & 0x100))
				ff9j1 = 1;

			/* for the Hang On/Super Hang On case only: if the control word bit 9 is clear, we will forcibly */
			/* set the flip-flip at 9J (upper half) */
			if (Hangon && !(control & 0x200))
				ff9j2 = 1;

			/* ---- now process the pixel ---- */
			md = 3;

			/* the Space Harrier/Enduro Racer hardware has a tweak that maps the control word bit 9 to the */
			/* /CE line on the road ROM; use this to effectively disable the road data */
			if (Hangon || !(control & 0x200))

				/* the /OE line on the road ROM is linked to the AND of bits 2 & 3 of the counter at 9N */
				if ((ctr9n9p & 0xc0) == 0xc0)
				{
					/* note that the pixel logic is hidden in a custom at 9S; this is just a guess */
					if (ss8j & 1)
						md = src[((ctr9n9p & 0x3f) << 3) | ctr9m];
					else
						md = src[((ctr9n9p & 0x3f) << 3) | (ctr9m ^ 7)];
				}

			/* "select" is a made-up signal that comes from bit 3 of the serial shifter and is */
			/* used in several places for color selection */
			select = (ss8j >> 3) & 1;

			/* check the flip-flop at 9J (upper half) to determine if we should use the background color; */
			/* the output of this is ANDed with M0 and M1 so it only affects pixels with a value of 3; */
			/* this is done by the AND gates at 9L and 7K */
			if (ff9j2 && md == 3)
			{
				/* in this case, the "select" signal is used to select which background color to use */
				/* since the color0 control word contains two selections */
				color = (color0 >> (select ? 0 : 8)) & 0x3f;
				color |= 0x7c0;
			}

			/* if we're not using the background color, we select pixel data from an alternate path */
			else
			{
				/* the AND gates at 7L, 9K, and 7K clamp the pixel value to 0 if bit 7 of the color 1 */
				/* signal is 1 and if the pixel value is 3 (both M0 and M1 == 1) */
				if ((color1 & 0x80) && md == 3)
					md = 0;

				/* the pixel value plus the "select" line combine to form a mux into the low 8 bits of color1 */
				color = (color1 >> ((md << 1) | select)) & 1;

				/* this value becomes the low bit of the final color; the "select" line itself and the pixel */
				/* value form the other bits */
				color |= select << 3;
				color |= md << 1;
				color |= 0x038;
			}

			/* write the pixel if we're past the minimum clip */
			if (x >= 0)
				pPixel[x] = color;

			/* ---- the following logic all happens on the 6M clock ---- */

			/* clock the counter at 9M */
			ctr9m = (ctr9m + 1) & 7;

			/* if enabled, clock on the two cascaded 4-bit counters at 9P and 9N */
			if (ctr9n9p_ena)
			{
				if (ff9j1)
					ctr9n9p++;
				else
					ctr9n9p--;
			}

			/* clock the flip-flop at 9J (upper half) */
			ff9j2 = !(!ff9j1 && (ss8j & 0x80));

			/* clock the serial shift register at 8J */
			ss8j = (ss8j << 1) | ff9j1;
		}
	}
}

/*====================================================
Rotation Layer Rendering
====================================================*/

void System16RotateDraw()
{
	UINT16 *pRotateBuff = (UINT16*)System16RotateRamBuff;

	INT32 currx = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f0]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f1]);
	INT32 curry = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f2]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f3]);
	INT32 dyy = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f4]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f5]);
	INT32 dxx = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f6]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f7]);
	INT32 dxy = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f8]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f9]);
	INT32 dyx = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3fa]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3fb]);
	INT32 x, y;

	/* advance forward based on the clip rect */
	currx += dxx * (0 + 27) + dxy * 0;
	curry += dyx * (0 + 27) + dyy * 0;

	/* loop over screen Y coordinates */
	for (y = 0; y <= 223; y++)
	{
		UINT16* pPixel = pTransDraw + (y * 320);
		UINT16* pSrc = pTempDraw;
		UINT8* pPri = System16PriorityMap + (y * 320);
		INT32 tx = currx;
		INT32 ty = curry;

		/* loop over screen X coordinates */
		for (x = 0; x <= 319; x++)
		{
			/* fetch the pixel from the source bitmap */
			INT32 sx = (tx >> 14) & 0x1ff;
			INT32 sy = (ty >> 14) & 0x1ff;
			INT32 pix = pSrc[sy * 512 + (sx & 0x1ff)];

			/* non-zero pixels get written; everything else is the scanline color */
			if (pix != 0xffff)
			{
				*pPixel++ = ((pix & 0x1ff) | ((pix >> 6) & 0x200) | ((pix >> 3) & 0xc00) | 0x1000) & (System16PaletteEntries - 1);
				*pPri++ = (pix >> 8) | 1;
			}
			else
			{
				*pPixel++ = /*info->colorbase +*/ sy;
				*pPri++ = 0xff;
			}

			/* advance the source X/Y pointers */
			tx += dxx;
			ty += dyx;
		}

		/* advance the source X/Y pointers */
		currx += dxy;
		curry += dyy;
	}
}

// High resolution version of System16RotateDraw() for System16ResScale > 1.
// It walks the same affine transform, but in steps of 1/S of a native pixel,
// and samples the (512 * S) x (512 * S) sprite framebuffer drawn by
// YBoardRenderSpriteLayerScaled(). Positions are kept multiplied by S in 64
// bits, so (S * v) >> 14 is the scaled framebuffer coordinate; only its low
// 9 + k bits are used (S = 1 << k), so the 32-bit wrap-around of the original
// accumulators doesn't matter.
static void System16RotateDrawScaledRows(INT32 y0, INT32 y1)
{
	UINT16 *pRotateBuff = (UINT16*)System16RotateRamBuff;

	const INT32 s = System16ResScale;
	const INT32 k = (s == 4) ? 2 : 1;	/* s = 1 << k */
	const INT32 fbmask = 512 * s - 1;
	const INT32 w = 320 * s;
	const INT32 h = 224 * s;

	INT32 currx = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f0]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f1]);
	INT32 curry = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f2]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f3]);
	INT32 dyy = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f4]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f5]);
	INT32 dxx = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f6]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f7]);
	INT32 dxy = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f8]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3f9]);
	INT32 dyx = (BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3fa]) << 16) | BURN_ENDIAN_SWAP_INT16(pRotateBuff[0x3fb]);

	/* advance forward based on the clip rect */
	currx += dxx * (0 + 27) + dxy * 0;
	curry += dyx * (0 + 27) + dyy * 0;

	INT64 rowx = (INT64)currx * s + (INT64)y0 * dxy;
	INT64 rowy = (INT64)curry * s + (INT64)y0 * dyy;

	if (y1 > h) y1 = h;
	for (INT32 y = y0; y < y1; y++)
	{
		UINT16* pPixel = pTransDraw + (y * w);
		UINT8* pPri = System16PriorityMap + (y * w);
		INT64 tx = rowx;
		INT64 ty = rowy;

		if (dyx == 0 && dxx == 0x4000)
		{
			/* no rotation or horizontal zoom (the usual case): the row reads one
			   framebuffer row, one pixel per pixel (wrapping at its end) */
			const INT32 sy = (INT32)(ty >> 14) & fbmask;
			const UINT16 *pSrcRow = pTempDraw + sy * (fbmask + 1);
			const UINT16 scanline = (UINT16)(sy >> k);	/* scanline colour = native framebuffer row */
			INT32 sx = (INT32)(tx >> 14) & fbmask;
			for (INT32 x = 0; x < w; )
			{
				INT32 n = fbmask + 1 - sx;		/* pixels before the framebuffer row wraps */
				if (n > w - x) n = w - x;
				const UINT16 *pSrc = pSrcRow + sx;
				for (INT32 i = 0; i < n; i++)
				{
					const INT32 pix = pSrc[i];
					if (pix != 0xffff)
					{
						*pPixel++ = ((pix & 0x1ff) | ((pix >> 6) & 0x200) | ((pix >> 3) & 0xc00) | 0x1000) & (System16PaletteEntries - 1);
						*pPri++ = (pix >> 8) | 1;
					}
					else
					{
						*pPixel++ = scanline;
						*pPri++ = 0xff;
					}
				}
				x += n;
				sx = 0;
			}

			rowx += dxy;
			rowy += dyy;
			continue;
		}

		for (INT32 x = 0; x < w; x++)
		{
			INT32 sx = (INT32)(tx >> 14) & fbmask;
			INT32 sy = (INT32)(ty >> 14) & fbmask;
			INT32 pix = pTempDraw[sy * (fbmask + 1) + sx];

			if (pix != 0xffff)
			{
				*pPixel++ = ((pix & 0x1ff) | ((pix >> 6) & 0x200) | ((pix >> 3) & 0xc00) | 0x1000) & (System16PaletteEntries - 1);
				*pPri++ = (pix >> 8) | 1;
			}
			else
			{
				*pPixel++ = sy >> k;	/* scanline colour = native framebuffer row */
				*pPri++ = 0xff;
			}

			tx += dxx;
			ty += dyx;
		}

		rowx += dxy;
		rowy += dyy;
	}
}

// The screen rows are independent, so the lower half is done on the second
// thread (see YBoardThreadsStart) while this one does the upper half.
static void System16RotateDrawScaled()
{
	const INT32 h = 224 * System16ResScale;
	if (YBoardThreadsStart(YBOARD_JOB_ROTATE, h / 2, h)) {
		System16RotateDrawScaledRows(0, h / 2);
		YBoardThreadsWait();
	} else {
		System16RotateDrawScaledRows(0, h);
	}
}

/*====================================================
Genesis VDP Rendering
====================================================*/

static void System18DrawVDP(INT32 prio)
{
	INT32 x, y;

	for (y = 0; y < 224; y++)
	{
		UINT16 *src = pTempDraw + (y * 512);
		UINT16 *dst = pTransDraw + (y * 320);
		UINT8  *pri = pPrioDraw + (y * 320);

		for (x = 0; x < 320; x++)
		{
			UINT16 pix = src[x];
			if (pix != 0xffff)
			{
				dst[x] = pix;
				pri[x] |= prio;
			}
		}
	}
}

void UpdateSystem18VDP()
{
	for (INT32 y = 0; y < 224; y++) {
		UINT16* pPixel = pTempDraw + (y * 512);
		vdp_drawline(pPixel, y, 0xffff);
	}
}

/*====================================================
Palette Generation
====================================================*/

static INT32 rgbclamp(INT32 in)
{
	if (in > 255) in = 255;
	if (in < 0) in = 0;

	return in;
}

void System16PaletteInit()
{
	static const INT32 resistances_normal[6] = { 3900, 2000, 1000, 1000/2, 1000/4, 0 };
	double weights_normal[6];
	compute_resistor_weights(0, 255, -1.0, 6, resistances_normal, weights_normal, 0, 0, 0, NULL, NULL, 0, 0, 0, NULL, NULL, 0, 0);

	// compute weight table for shadow/hilight palette entries
	static const INT32 resistances_sh[6]     = { 3900, 2000, 1000, 1000/2, 1000/4, 470 };
	double weights_sh[6];
	compute_resistor_weights(0, 255, -1.0, 6, resistances_sh, weights_sh, 0, 0, 0, NULL, NULL, 0, 0, 0, NULL, NULL, 0, 0);

	// compute R, G, B for each weight
	for (INT32 value = 0; value < 32; value++) {
		INT32 i4 = (value >> 4) & 1;
		INT32 i3 = (value >> 3) & 1;
		INT32 i2 = (value >> 2) & 1;
		INT32 i1 = (value >> 1) & 1;
		INT32 i0 = (value >> 0) & 1;
		System16PaletteNormal[value] = combine_6_weights(weights_normal, i0, i1, i2, i3, i4, 0);

		// Out Run hw gets slightly darker shadows, to match pcb
		INT32 c = combine_6_weights(weights_sh, i0, i1, i2, i3, i4, ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_OUTRUN) ? -0.30 : 0);
		System16PaletteShadow[value] = rgbclamp(c);
		System16PaletteHilight[value] = combine_6_weights(weights_sh, i0, i1, i2, i3, i4, 1);
	}
}

void System16PaletteExit()
{
	memset(System16PaletteNormal, 0, 32 * sizeof(UINT8));
	memset(System16PaletteShadow, 0, 32 * sizeof(UINT8));
	memset(System16PaletteHilight, 0, 32 * sizeof(UINT8));
}

static INT32 System16CalcPalette()
{
	INT32 i;

	for (i = 0; i < System16PaletteEntries * 2; i +=2) {
		INT32 r, g, b;
		INT32 nColour = (System16PaletteRam[i + 1] << 8) | System16PaletteRam[i + 0];
	
		r = ((nColour >> 12) & 0x01) | ((nColour << 1) & 0x1e);
		g = ((nColour >> 13) & 0x01) | ((nColour >> 3) & 0x1e);
		b = ((nColour >> 14) & 0x01) | ((nColour >> 7) & 0x1e);
		
		System16Palette[i / 2] = BurnHighCol(System16PaletteNormal[r], System16PaletteNormal[g], System16PaletteNormal[b], 0);
		System16Palette[(i / 2) + System16PaletteEntries] = BurnHighCol(System16PaletteShadow[r], System16PaletteShadow[g], System16PaletteShadow[b], 0);
		System16Palette[(i / 2) + (System16PaletteEntries * 2)] = BurnHighCol(System16PaletteHilight[r], System16PaletteHilight[g], System16PaletteHilight[b], 0);
	}
	
	return 0;
}

static INT32 PhilkoCalcPalette()
{
	INT32 i;

	for (i = 0; i < System16PaletteEntries * 2; i +=2) {
		INT32 r, g, b;
		INT32 nColour = (System16PaletteRam[i + 1] << 8) | System16PaletteRam[i + 0];
	
		b = (nColour >> 0) & 0x1f;
		g = (nColour >> 5) & 0x1f;
		r = (nColour >> 10) & 0x1f;
		
		System16Palette[i / 2] = BurnHighCol(System16PaletteNormal[r], System16PaletteNormal[g], System16PaletteNormal[b], 0);
		System16Palette[(i / 2) + System16PaletteEntries] = BurnHighCol(System16PaletteShadow[r], System16PaletteShadow[g], System16PaletteShadow[b], 0);
		System16Palette[(i / 2) + (System16PaletteEntries * 2)] = BurnHighCol(System16PaletteHilight[r], System16PaletteHilight[g], System16PaletteHilight[b], 0);
	}
	
	return 0;
}

/*====================================================
Frame Rendering
====================================================*/

inline static void System16AUpdateTileValues()
{
	UINT16 *TextRam = (UINT16*)System16TextRam;
	INT32 i;
	
	for (i = 0; i < 2; i++) {
		System16OldPage[i] = System16Page[i];
		System16Page[i] = (System16ScreenFlip) ? BURN_ENDIAN_SWAP_INT16(TextRam[0xe8e/2 - i]) : BURN_ENDIAN_SWAP_INT16(TextRam[0xe9e/2 - i]);
		System16ScrollX[i] = BURN_ENDIAN_SWAP_INT16(TextRam[0xff8/2 + i]) & 0x1ff;
		System16ScrollY[i] = BURN_ENDIAN_SWAP_INT16(TextRam[0xf24/2 + i]) & 0x0ff;
	}
	
	if (System16OldPage[0] != System16Page[0]) {
		System16RecalcFgTileMap = 1;
	}
	
	if (System16OldPage[1] != System16Page[1]) {
		System16RecalcBgTileMap = 1;
	}
}

INT32 System16ARender()
{
	// clear sprite mixer buffer
	memset(pSpriteBitmap, 0xff, nScreenHeight * nScreenWidth * sizeof(UINT16));

	// tiles_generic priority mixer: mix-all
	GenericTilesPRIMASK = 0xff;

	BurnTransferClear(System16PaletteEntries * 3); // BLACK

	if (!System16AVideoEnableDelayed) {
		BurnTransferCopy(System16Palette);
		return 0;
	}
	
	System16AUpdateTileValues();
	System16ACreateTileMaps();
	
	System16CalcPalette();

	System16ARenderTileLayer(1, 0, 0, 0);
	System16ARenderTileLayer(1, 0, 1, 0);

	System16ARenderTileLayer(1, 0, 1, 1);
	System16ARenderTileLayer(1, 1, 1, 2);

	System16ARenderTileLayer(0, 0, 1, 2);
	System16ARenderTileLayer(0, 1, 1, 4);

	System16ARenderTextLayer(0, 4);
	System16ARenderTextLayer(1, 8);

	if (nSpriteEnable & 1) System16ARenderSpriteLayer();

	for (INT32 y = 0; y < nScreenHeight; y++) {
		for (INT32 x = 0; x < nScreenWidth; x++) {
			UINT16* pPixel = pSpriteBitmap + (y * 320);
			UINT8*    pPri = pPrioDraw  + (y * nScreenWidth);
			UINT16*  pDest = pTransDraw + (y * 320);

			UINT16 pix = pPixel[x];
			INT32 pri = (pix >> 10) & 3;

			if (pix != 0xffff) {
				if ((1<<pri) > pPri[x]) {

					if ((pix & 0x3f0) == 0x3f0) {
						pDest[x] += System16PaletteEntries;
					} else {
						pDest[x] = ((pix & 0x3ff) | System16SpritePalOffset);
					}
				}
			}
		}
	}

	BurnTransferCopy(System16Palette);

	return 0;
}

INT32 System16BootlegRender()
{
	if (!System16VideoEnable) {
		BurnTransferClear();
		return 0;
	}

	System16CalcPalette();
	BootlegRenderTileLayer(1, 0, 0);
	BootlegRenderTileLayer(1, 1, 0);
	System16BRenderSpriteLayer(1);
	BootlegRenderTileLayer(1, 0, 1);
	System16BRenderSpriteLayer(2);
	BootlegRenderTileLayer(1, 1, 1);
	BootlegRenderTileLayer(0, 0, 1);
	System16BRenderSpriteLayer(4);
	BootlegRenderTileLayer(0, 1, 1);
	System16BRenderTextLayer(0);
	System16BRenderSpriteLayer(8);
	System16BRenderTextLayer(1);
	BurnTransferCopy(System16Palette);

	return 0;
}

inline static void System16BUpdateTileValues()
{
	UINT16 *TextRam = (UINT16*)System16TextRam;
	
	for (INT32 i = 0; i < 4; i++) {
		System16OldPage[i] = System16Page[i];
		System16Page[i] = BURN_ENDIAN_SWAP_INT16(TextRam[(0xe80 >> 1) + i]);
		System16ScrollX[i] = BURN_ENDIAN_SWAP_INT16(TextRam[(0xe98 >> 1) + i]);
		System16ScrollY[i] = BURN_ENDIAN_SWAP_INT16(TextRam[(0xe90 >> 1) + i]);
	}
	
	if (System16OldPage[0] != System16Page[0]) {
		System16RecalcFgTileMap = 1;
	}
	
	if (System16OldPage[1] != System16Page[1]) {
		System16RecalcBgTileMap = 1;
	}
	
	if (System16OldPage[2] != System16Page[2]) {
		System16RecalcFgAltTileMap = 1;
	}
	
	if (System16OldPage[3] != System16Page[3]) {
		System16RecalcBgAltTileMap = 1;
	}
}

// System 18 / 16B v dink.01
static void System18RenderTileLayer(INT32 Page, INT32 PriorityDraw, INT32 Transparent, INT32 Priority)
{
	INT32 xScroll, yScroll, x, y;
	UINT16 *TextRam = (UINT16*)System16TextRam;

	xScroll = System16ScrollX[Page];
	yScroll = System16ScrollY[Page];
	UINT16 Pix;

	UINT16 *pTileMapSrc = NULL;
	UINT16 *pTileMapDest = NULL;
	INT32 xSrcOff, ySrcOff, RowScrollIndex, RowScroll, xEffScroll, yEffScroll;

	if (yScroll & 0x8000) {
		for (y = 0; y < nScreenHeight; y++) {
			pTileMapDest = pTransDraw + (y * nScreenWidth);
			UINT8 *pPrio = pPrioDraw + (y * nScreenWidth);

			RowScrollIndex = y / 8;
			if (System16ScreenFlip) RowScrollIndex = (216 - y) / 8;

			RowScroll = BURN_ENDIAN_SWAP_INT16(TextRam[0xf80/2 + 0x40/2 * Page + RowScrollIndex]);
			xEffScroll = (xScroll & 0x8000) ? RowScroll : xScroll;

			for (x = 0; x < nScreenWidth; x++) {
				yEffScroll = BURN_ENDIAN_SWAP_INT16(TextRam[0xf16/2 + 0x40/2 * Page + (x+9)/16]);

				if (RowScroll & 0x8000) {
					xEffScroll = System16ScrollX[Page + 2];
					yEffScroll = System16ScrollY[Page + 2];

					ySrcOff = (y + yEffScroll) & 0x1ff;
					if (System16ScreenFlip) ySrcOff = (((216 + System16ScreenFlipYoffs) - y) + yEffScroll) & 0x1ff;

					if (Page == 0 && PriorityDraw == 0) pTileMapSrc = pSys16FgAltTileMapPri0 + (ySrcOff * 1024);
					if (Page == 0 && PriorityDraw == 1) pTileMapSrc = pSys16FgAltTileMapPri1 + (ySrcOff * 1024);
					if (Page == 1 && PriorityDraw == 0) pTileMapSrc = pSys16BgAltTileMapPri0 + (ySrcOff * 1024);
					if (Page == 1 && PriorityDraw == 1) pTileMapSrc = pSys16BgAltTileMapPri1 + (ySrcOff * 1024);
					if (Page == 1 && Transparent == 0) pTileMapSrc = pSys16BgAltTileMapOpaque + (ySrcOff * 1024);
				} else {
					ySrcOff = (y + yEffScroll) & 0x1ff;
					if (System16ScreenFlip) ySrcOff = (((216 + System16ScreenFlipYoffs) - y) + yEffScroll) & 0x1ff;

					if (Page == 0 && PriorityDraw == 0) pTileMapSrc = pSys16FgTileMapPri0 + (ySrcOff * 1024);
					if (Page == 0 && PriorityDraw == 1) pTileMapSrc = pSys16FgTileMapPri1 + (ySrcOff * 1024);
					if (Page == 1 && PriorityDraw == 0 && Transparent == 1) pTileMapSrc = pSys16BgTileMapPri0 + (ySrcOff * 1024);
					if (Page == 1 && PriorityDraw == 1 && Transparent == 1) pTileMapSrc = pSys16BgTileMapPri1 + (ySrcOff * 1024);
					if (Page == 1 && Transparent == 0) pTileMapSrc = pSys16BgTileMapOpaque + (ySrcOff * 1024);
				}

				xSrcOff = (x - xEffScroll + 192) & 0x3ff;

				if (System16ScreenFlip) xSrcOff = (((312 + System16ScreenFlipXoffs) - x) - xEffScroll + 192) & 0x3ff;

				Pix = pTileMapSrc[xSrcOff];

				if (Transparent) {
					if (Pix) {
						pTileMapDest[x] = Pix;
						pPrio[x] |= Priority;
					}
				} else {
					pTileMapDest[x] = Pix;
					pPrio[x] |= Priority;
				}
			}
		}
	} else {
		for (y = 0; y < nScreenHeight; y++) {
			pTileMapDest = pTransDraw + (y * nScreenWidth);
			UINT8 *pPrio = pPrioDraw + (y * nScreenWidth);

			RowScrollIndex = y / 8;
			if (System16ScreenFlip) RowScrollIndex = (216 - y) / 8;

			RowScroll = BURN_ENDIAN_SWAP_INT16(TextRam[0xf80/2 + 0x40/2 * Page + RowScrollIndex]);
			xEffScroll = (xScroll & 0x8000) ? RowScroll : xScroll;
			yEffScroll = yScroll;

			if (RowScroll & 0x8000) {
				xEffScroll = System16ScrollX[Page + 2];
				yEffScroll = System16ScrollY[Page + 2];

				ySrcOff = (y + yEffScroll) & 0x1ff;
				if (System16ScreenFlip) ySrcOff = (((216 + System16ScreenFlipYoffs) - y) + yEffScroll) & 0x1ff;

				if (Page == 0 && PriorityDraw == 0) pTileMapSrc = pSys16FgAltTileMapPri0 + (ySrcOff * 1024);
				if (Page == 0 && PriorityDraw == 1) pTileMapSrc = pSys16FgAltTileMapPri1 + (ySrcOff * 1024);
				if (Page == 1 && PriorityDraw == 0) pTileMapSrc = pSys16BgAltTileMapPri0 + (ySrcOff * 1024);
				if (Page == 1 && PriorityDraw == 1) pTileMapSrc = pSys16BgAltTileMapPri1 + (ySrcOff * 1024);
				if (Page == 1 && Transparent == 0) pTileMapSrc = pSys16BgAltTileMapOpaque + (ySrcOff * 1024);
			} else {
				ySrcOff = (y + yEffScroll) & 0x1ff;
				if (System16ScreenFlip) ySrcOff = (((216 + System16ScreenFlipYoffs) - y) + yEffScroll) & 0x1ff;

				if (Page == 0 && PriorityDraw == 0) pTileMapSrc = pSys16FgTileMapPri0 + (ySrcOff * 1024);
				if (Page == 0 && PriorityDraw == 1) pTileMapSrc = pSys16FgTileMapPri1 + (ySrcOff * 1024);
				if (Page == 1 && PriorityDraw == 0) pTileMapSrc = pSys16BgTileMapPri0 + (ySrcOff * 1024);
				if (Page == 1 && PriorityDraw == 1) pTileMapSrc = pSys16BgTileMapPri1 + (ySrcOff * 1024);
				if (Page == 1 && Transparent == 0) pTileMapSrc = pSys16BgTileMapOpaque + (ySrcOff * 1024);
			}

			for (x = 0; x < nScreenWidth; x++) {
				xSrcOff = (x - xEffScroll + 192) & 0x3ff;

				if (System16ScreenFlip) xSrcOff = (((312 + System16ScreenFlipXoffs) - x) - xEffScroll + 192) & 0x3ff;

				Pix = pTileMapSrc[xSrcOff];

				if (Transparent) {
					if (Pix) {
						pTileMapDest[x] = Pix;
						pPrio[x] |= Priority;
					}
				} else {
					pTileMapDest[x] = Pix;
					pPrio[x] |= Priority;
				}
			}
		}
	}
}

static void System18RenderTextLayer(INT32 PriorityDraw, INT32 tpri)
{
	INT32 mx, my, Code, Colour, x, y, Priority, TileIndex = 0;

	INT32 ColourDepth = 3;
	if (Lockonph) ColourDepth = 4;

	for (my = 0; my < 32; my++) {
		for (mx = 0; mx < 64; mx++) {
			Code = (System16TextRam[TileIndex + 1] << 8) | System16TextRam[TileIndex + 0];
			Priority = (Code >> 15) & 1;

			if (Priority == PriorityDraw) {
				Colour = (Code >> ((AltModeKludge) ? 8 : 9)) & 0x07;
				Code &= (AltModeKludge) ? 0xff : 0x1ff;

				Code += System16TileBanks[0] * System16TileBankSize;

				Code &= (System16NumTiles - 1);

				x = 8 * mx;
				y = 8 * my;

				x -= 192;

				if (System16ScreenFlip) {
					x = 312 - x;
					y = 216 - y;

					if (x > 7 && x < 312 && y > 7 && y < 216) {
						Render8x8Tile_Prio_Mask_FlipXY(pTransDraw, Code, x, y, Colour, ColourDepth, 0, System16TilemapColorOffset, tpri, System16Tiles);
					} else {
						Render8x8Tile_Prio_Mask_FlipXY_Clip(pTransDraw, Code, x, y, Colour, ColourDepth, 0, System16TilemapColorOffset, tpri, System16Tiles);
					}
				} else {
					if (x > 7 && x < 312 && y > 7 && y < 216) {
						Render8x8Tile_Prio_Mask(pTransDraw, Code, x, y, Colour, ColourDepth, 0, System16TilemapColorOffset, tpri, System16Tiles);
					} else {
						Render8x8Tile_Prio_Mask_Clip(pTransDraw, Code, x, y, Colour, ColourDepth, 0, System16TilemapColorOffset, tpri, System16Tiles);
					}
				}
			}

			TileIndex += 2;
		}
	}
}

inline static void System18DrawPixel(INT32 x, INT32 pix, INT32 colour, UINT16* pPixel)
{
	x += System16SpriteXOffset;
	if (x >= 0 && x <= 319 && pix != 0 && pix != 15) {
		pPixel[x] = pix | colour;
	}
}

static void System18RenderSpriteLayer()
{
	UINT8 numbanks;
	const UINT16 *spritebase;
	UINT16 *data;

	spritebase = (const UINT16 *)System16Sprites;
	numbanks = System16SpriteRomSize / 0x20000;

	for (data = (UINT16*)System16SpriteRam; data < (UINT16*)System16SpriteRam + System16SpriteRamSize / 2; data += 8) {
		if (BURN_ENDIAN_SWAP_INT16(data[2]) & 0x8000) break;
		INT32 bottom  = BURN_ENDIAN_SWAP_INT16(data[0]) >> 8;
		INT32 top     = BURN_ENDIAN_SWAP_INT16(data[0]) & 0xff;
		INT32 xpos    = (BURN_ENDIAN_SWAP_INT16(data[1]) & 0x1ff) - 0xb8;
		INT32 hide    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x4000;
		INT32 flip    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x100;
		INT32 pitch   = (INT8)(BURN_ENDIAN_SWAP_INT16(data[2]) & 0xff);
		UINT16 addr   = BURN_ENDIAN_SWAP_INT16(data[3]);
		INT32 bank    = System16SpriteBanks[(BURN_ENDIAN_SWAP_INT16(data[4]) >> 8) & 0xf];
		INT32 color   = ((BURN_ENDIAN_SWAP_INT16(data[4]) & 0xff) << 4);// | ((BURN_ENDIAN_SWAP_INT16(data[1]) & 0x1e00) << 3); <- this is for yboard, only

		INT32 vzoom   = (BURN_ENDIAN_SWAP_INT16(data[5]) >> 5) & 0x1f;
		INT32 hzoom   = BURN_ENDIAN_SWAP_INT16(data[5]) & 0x1f;
		const UINT16 *spritedata;
		INT32 x, y, pix, xdelta = 1;

		/* initialize the end address to the start address */
		//data[7] = addr;
		UINT16 d7 = addr; // data[7] cache

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (hide || (top >= bottom) || bank == 255) {
			data[7] = BURN_ENDIAN_SWAP_INT16(d7); // un-cache d7
			continue;
		}

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x10000 * bank;

		/* reset the yzoom counter */
		data[5] &= BURN_ENDIAN_SWAP_INT16(0x03ff);

		if (System16ScreenFlip) {
			INT32 temp = top;
			top = 224 - bottom;
			bottom = 224 - temp;
			xpos = 320 - xpos;
			xdelta = -1;
		}

		/* loop from top to bottom */
		for (y = top; y < bottom; y++) {
			/* advance a row */
			addr += pitch;

			/* accumulate zoom factors; if we carry into the high bit, skip an extra row */
			data[5] += BURN_ENDIAN_SWAP_INT16(vzoom << 10);
			if (data[5] & BURN_ENDIAN_SWAP_INT16(0x8000)) {
				addr += pitch;
				data[5] &= BURN_ENDIAN_SWAP_INT16(~0x8000);
			}

			/* skip drawing if not within the cliprect */
			if (y >= 0 && y <= 223) {
				UINT16* pPixel = pSpriteBitmap + (y * 320);
				int xacc;

				/* compute the initial X zoom accumulator; this is verified on the real PCB */
				xacc = 4 * hzoom;

				/* non-flipped case */
				if (!flip) {
					/* start at the word before because we preincrement below */
					//data[7] = addr - 1;
					d7 = addr - 1;
					for (x = xpos; ((xpos - x) & 0x1ff) != 1; ) {
						UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[++d7]);

						/* draw four pixels */
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System18DrawPixel(x, pix, color, pPixel); x += xdelta; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System18DrawPixel(x, pix, color, pPixel); x += xdelta; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System18DrawPixel(x, pix, color, pPixel); x += xdelta; }
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System18DrawPixel(x, pix, color, pPixel); x += xdelta; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15) break;
					}
				} else {
					/* start at the word after because we predecrement below */
					//data[7] = addr + 1;
					d7 = addr + 1;
					for (x = xpos; ((xpos - x) & 0x1ff) != 1; ) {
						UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[--d7]);

						/* draw four pixels */
						pix = (pixels >>  0) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System18DrawPixel(x, pix, color, pPixel); x += xdelta; }
						pix = (pixels >>  4) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System18DrawPixel(x, pix, color, pPixel); x += xdelta; }
						pix = (pixels >>  8) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System18DrawPixel(x, pix, color, pPixel); x += xdelta; }
						pix = (pixels >> 12) & 0xf; xacc = (xacc & 0x3f) + hzoom; if (xacc < 0x40) { System18DrawPixel(x, pix, color, pPixel); x += xdelta; }

						/* stop if the last pixel in the group was 0xf */
						if (pix == 15) break;
					}
				}
			}
		}
	}
}


INT32 System18Render()
{
	BurnTransferClear();

	// clear sprite mixer buffer
	memset(pSpriteBitmap, 0xff, nScreenHeight * nScreenWidth * sizeof(UINT16));

	// tiles_generic priority mixer: mix-all
	GenericTilesPRIMASK = 0xff;

	if (!System16VideoEnable) {
		return 0;
	}

	INT32 VDPLayer = (System18VdpMixing >> 1) & 3;
	INT32 VDPPri = (System18VdpMixing & 1) ? (1 << VDPLayer) : 0;

	System16BUpdateTileValues();
	System16BCreateTileMaps();

	System16CalcPalette();
	GenesisPaletteRecalc();

	INT32 VDP_Enable = System18VdpEnable;
	if (~nSpriteEnable & 0x80) VDP_Enable = 0;

	if (VDP_Enable) UpdateSystem18VDP();

	if (nBurnLayer & 1) System18RenderTileLayer(1, 0, 0, 0);
	if (nBurnLayer & 1) System18RenderTileLayer(1, 1, 0, 0);
	if (VDP_Enable && VDPLayer == 0) System18DrawVDP(VDPPri);

	if (nBurnLayer & 2) System18RenderTileLayer(1, 0, 1, 1);
	if (nBurnLayer & 4) System18RenderTileLayer(1, 1, 1, 2);
	if (VDP_Enable && VDPLayer == 1) System18DrawVDP(VDPPri);

	if (nBurnLayer & 8)    System18RenderTileLayer(0, 0, 1, 2);
	if (nSpriteEnable & 1) System18RenderTileLayer(0, 1, 1, 4);
	if (VDP_Enable && VDPLayer == 2) System18DrawVDP(VDPPri);

	if (nSpriteEnable & 2) System18RenderTextLayer(0, 4);
	if (nSpriteEnable & 4) System18RenderTextLayer(1, 8);
	if (VDP_Enable && VDPLayer == 3) System18DrawVDP(VDPPri);

	if (nSpriteEnable & 8) System18RenderSpriteLayer();

	for (INT32 y = 0; y < nScreenHeight; y++) {
		for (INT32 x = 0; x < nScreenWidth; x++) {
			UINT16* pPixel = pSpriteBitmap + (y * 320);
			UINT8*    pPri = pPrioDraw  + (y * nScreenWidth);
			UINT16*  pDest = pTransDraw + (y * 320);
			UINT16 *PalRAM = (UINT16*)System16PaletteRam;

			UINT16 pix = pPixel[x];
			INT32 pri = (pix >> 10) & 3;

			if (pix != 0xffff) {
				if ((1<<pri) > pPri[x]) {

					if ((pix & 0x3f0) == 0x3f0) {
						pDest[x] += (BURN_ENDIAN_SWAP_INT16(PalRAM[pPixel[x]]) & 0x8000) ? (System16PaletteEntries * 2) : System16PaletteEntries;
					} else {
						pDest[x] = ((pix & 0x3ff) | System16SpritePalOffset);
					}
				}
			}
		}
	}

	BurnTransferCopy(System16Palette);

	BurnGunDrawTargets();

	return 0;
}

INT32 System16BAltRender()
{
	AltModeKludge = 1;
	return System16BRender();
}

INT32 System16BRender()
{
	BurnTransferClear();

	// clear sprite mixer buffer
	memset(pSpriteBitmap, 0xff, nScreenHeight * nScreenWidth * sizeof(UINT16));

	// tiles_generic priority mixer: mix-all
	GenericTilesPRIMASK = 0xff;

	if (!System16IgnoreVideoEnable) {
		if (!System16VideoEnable) {
			return 0;
		}
	}
	System16BUpdateTileValues();

	if (AltModeKludge) {
		System16BAltCreateTileMaps();
	} else {
		System16BCreateTileMaps();
	}

	if (Lockonph || ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_DFJAIL))) {
		PhilkoCalcPalette();
	} else {
		System16CalcPalette();
	}

	if (nBurnLayer & 1) System18RenderTileLayer(1, 0, 0, 0);
	if (nBurnLayer & 1) System18RenderTileLayer(1, 1, 0, 0);

	if (nBurnLayer & 2) System18RenderTileLayer(1, 0, 1, 1);
	if (nBurnLayer & 4) System18RenderTileLayer(1, 1, 1, 2);

	if (nBurnLayer & 8)    System18RenderTileLayer(0, 0, 1, 2);
	if (nSpriteEnable & 1) System18RenderTileLayer(0, 1, 1, 4);

	if (nSpriteEnable & 2) System18RenderTextLayer(0, 4);
	if (nSpriteEnable & 4) System18RenderTextLayer(1, 8);

	if (nSpriteEnable & 8) System18RenderSpriteLayer();

	for (INT32 y = System16SpriteYOffset; y < (nScreenHeight - System16SpriteYOffset); y++) {
		for (INT32 x = 0; x < nScreenWidth; x++) {
			UINT16* pPixel = pSpriteBitmap + ((y - System16SpriteYOffset) * 320);
			UINT8*    pPri = pPrioDraw  + (y * nScreenWidth);
			UINT16*  pDest = pTransDraw + (y * 320);

			UINT16 pix = pPixel[x];
			INT32 pri = (pix >> 10) & 3;

			if (pix != 0xffff) {
				if ((1<<pri) > pPri[x]) {
					if ((pix & 0x3f0) == 0x3f0) {
						pDest[x] += System16PaletteEntries;
					} else {
						pDest[x] = ((pix & 0x3ff) | System16SpritePalOffset);
					}
				}
			}
		}
	}

	BurnTransferCopy(System16Palette);

	AltModeKludge = 0;

	return 0;
}

INT32 HangonRender()
{
	if (!System16VideoEnable) {
		BurnTransferClear();
		return 0;
	}
	
	System16CalcPalette();
	
	System16AUpdateTileValues();
	System16ACreateTileMaps();
	
	HangonRenderRoadLayer(0);
	HangonRenderSpriteLayer(1);
	System16ARenderTileLayer(1, 0, 1, 0);
	HangonRenderSpriteLayer(2);
	System16ARenderTileLayer(1, 1, 1, 0);
	System16ARenderTileLayer(0, 0, 1, 0);
	HangonRenderSpriteLayer(4);
	System16ARenderTileLayer(0, 1, 1, 0);
	HangonRenderRoadLayer(1);
	HangonRenderSpriteLayer(8);
	HangonRenderSpriteLayer(0);
	System16ARenderTextLayer(0, 0);
	System16ARenderTextLayer(1, 0);
	BurnTransferCopy(System16Palette);

	return 0;
}

INT32 HangonAltRender()
{
	if (!System16VideoEnable) {
		BurnTransferClear();
		return 0;
	}
	
	System16CalcPalette();
	
	System16AUpdateTileValues();
	System16ACreateTileMaps();
	
	HangonRenderRoadLayer(0);
	HangonAltRenderSpriteLayer(1);
	System16ARenderTileLayer(1, 0, 1, 0);
	HangonAltRenderSpriteLayer(2);
	System16ARenderTileLayer(1, 1, 1, 0);
	System16ARenderTileLayer(0, 0, 1, 0);
	HangonAltRenderSpriteLayer(4);
	System16ARenderTileLayer(0, 1, 1, 0);
	HangonRenderRoadLayer(1);
	HangonAltRenderSpriteLayer(8);
	HangonAltRenderSpriteLayer(0);
	System16ARenderTextLayer(0, 0);
	System16ARenderTextLayer(1, 0);
	BurnTransferCopy(System16Palette);

	return 0;
}

/*====================================================
Out Run board high resolution (System16ResScale > 1)
====================================================*/

// Out Run, Turbo Out Run, Super Hang-On and the X-Board games (After Burner,
// Thunder Blade, Super Monaco GP ...: the same road, sprite and tile chips)
// drawn at S = System16ResScale times
// the native resolution (pTransDraw is 320S x 224S). The native functions above
// are not changed and are used as they are when S is 1.
// - Tile and text layers: the native functions draw into a 320x224 scratch
//   buffer, and every pixel they wrote becomes an S x S block (OutrunNativeLayer*).
// - Road: drawn per output line from the road ROM rows at S times the width
//   where the ROM has them (OutrunRoad*).
// - Sprites: positioned and zoomed at S times finer steps; where the sprite ROM
//   holds a larger copy of the image (2:1, 4:1 ...), that copy is drawn
//   (OutrunLod*).

/* ---- tile and text layers ---- */

// (the frame's parts: see "threads" below)
#define OUTRUN_BAND_SHIFT	4			/* bands of 16 lines */
#define OUTRUN_PARTS		2
#define OUTRUN_PART(Y)		(((Y) >> OUTRUN_BAND_SHIFT) & (OUTRUN_PARTS - 1))
static void OutrunRunParts(INT32 job);
#define OUTRUN_JOB_SPRITES	1
#define OUTRUN_JOB_ROAD		2
#define OUTRUN_JOB_EXPAND	3
static void OutrunExpandPart(INT32 part);

#define OUTRUN_NATIVE_EMPTY		0xffff		/* not a pixel value any layer writes */

static UINT16 *OutrunNativeLayer = NULL;	/* 320x224 */
static UINT16 *OutrunNativeSaveDraw = NULL;
static INT32 OutrunNativeSaveW, OutrunNativeSaveH, OutrunNativeSaveClip[4];

// Redirects drawing to the native size scratch buffer, emptied.
static void OutrunNativeLayerBegin()
{
	if (OutrunNativeLayer == NULL) OutrunNativeLayer = (UINT16*)BurnMalloc(320 * 224 * sizeof(UINT16));
	memset(OutrunNativeLayer, 0xff, 320 * 224 * sizeof(UINT16));

	OutrunNativeSaveDraw = pTransDraw;
	OutrunNativeSaveW = nScreenWidth;
	OutrunNativeSaveH = nScreenHeight;
	GenericTilesGetClip(&OutrunNativeSaveClip[0], &OutrunNativeSaveClip[1], &OutrunNativeSaveClip[2], &OutrunNativeSaveClip[3]);

	pTransDraw = OutrunNativeLayer;
	nScreenWidth = 320;
	nScreenHeight = 224;
	GenericTilesSetClipRaw(0, 320, 0, 224);
}

// Restores drawing to the screen and draws there every pixel written to the
// scratch buffer, as an S x S block.
static void OutrunNativeLayerEnd()
{
	pTransDraw = OutrunNativeSaveDraw;
	nScreenWidth = OutrunNativeSaveW;
	nScreenHeight = OutrunNativeSaveH;
	GenericTilesSetClipRaw(OutrunNativeSaveClip[0], OutrunNativeSaveClip[1], OutrunNativeSaveClip[2], OutrunNativeSaveClip[3]);

	OutrunRunParts(OUTRUN_JOB_EXPAND);
}

// The native scratch's pixels as S x S blocks, the lines of one part.
static void OutrunExpandPart(INT32 part)
{
	const INT32 S = System16ResScale;
	const INT32 W = 320 * S;
	for (INT32 y = 0; y < 224; y++) {
		if (OUTRUN_PART(y * S) != part) continue;		/* (S divides the band) */
		const UINT16 *src = OutrunNativeLayer + y * 320;
		UINT16 *dst = pTransDraw + (y * S) * W;
		for (INT32 x = 0; x < 320; x++) {
			const UINT16 v = src[x];
			if (v == OUTRUN_NATIVE_EMPTY) continue;
			UINT16 *d = dst + x * S;
			for (INT32 j = 0; j < S; j++, d += W)
				for (INT32 i = 0; i < S; i++) d[i] = v;
		}
	}
}

/* ---- threads ---- */

// The frame is drawn by two threads where they help: the calling thread and
// a second one (FBNeo's thready; Windows, pthreads, macOS), each drawing the
// finer lines of its own bands of OUTRUN_BAND lines, so every pixel is written
// by one thread in the same order as by one thread alone (the same picture).
// Searches for larger art and anything else that changes shared state are done
// first, on the calling thread.


static volatile INT32 OutrunJob = 0;
static INT32 OutrunThreadsOn = 0;

static void OutrunDrawSpritesPart(INT32 part);
static void OutrunDrawRoadPart(INT32 part);

static void OutrunRunPart(INT32 job, INT32 part)
{
	if (job == OUTRUN_JOB_SPRITES) OutrunDrawSpritesPart(part);
	else if (job == OUTRUN_JOB_ROAD) OutrunDrawRoadPart(part);
	else if (job == OUTRUN_JOB_EXPAND) OutrunExpandPart(part);
}

static void OutrunThreadJob()
{
	OutrunRunPart(OutrunJob, 1);
}

// Runs a job's two parts, the second on the other thread where there is one.
static void OutrunRunParts(INT32 job)
{
#if (THREADY == THREADY_PTHREAD) || (THREADY == THREADY_PTHREADAPL) || (THREADY == THREADY_WINDOWS) || (THREADY == THREADY_WINDOWSVC)
	if (!OutrunThreadsOn) {
		thready.init(OutrunThreadJob);
		OutrunThreadsOn = 1;
	}
	OutrunJob = job;
	thready.notify();		/* (runs the job right here if the thread couldn't be made) */
	OutrunRunPart(job, 0);
	thready.notify_wait();
#else
	OutrunRunPart(job, 0);
	OutrunRunPart(job, 1);
#endif
}

static void OutrunThreadsExit()
{
	if (OutrunThreadsOn) {
		thready.exit();
		OutrunThreadsOn = 0;
	}
}

/* ---- road ---- */

// The road ROM has 256 rows per road (two roads), each a 512 pixel image of
// the road at one width, all centred on about the same pixel, with pixels
// outside the road 3; the width grows evenly with the row number (row r is
// about twice as wide as row r / 2). A line on screen shows one row, shifted
// by hpos. Here the road's width and centre are taken as straight-line fits
// over the rows, a line's road is drawn S times finer from the row that is
// (nearly) S times as wide, scaled back down, and the S - 1 finer lines between
// two native lines take the width and position in between, where the next
// line continues the same road. The road RAM gives each line's row in half
// rows (bit 0 is not used by the native renderer); that is used too.

struct OutrunRoadRow {
	INT16 a, b;			/* the road: pixels a..b-1 are not 3 (a == b: none) */
	UINT8 usable;		/* a whole road image, with lines on it */
	UINT8 ok;			/* drawn finer at the current scale (see OutrunRoadBuild) */
};

static OutrunRoadRow OutrunRoadRows[512];
static INT64 OutrunRoadLenA[2], OutrunRoadLenB[2];		/* road width = A * row + B (1/65536 pixels) */
static INT64 OutrunRoadCenA[2], OutrunRoadCenB[2];		/* road centre = A * row + B (1/65536 pixels) */
static const UINT8 *OutrunRoadRom = NULL;
static INT32 OutrunRoadScale = 0;

#define OUTRUN_ROAD_MIN_MATCH	800		/* per mille of the native road's pixels, and of its lines */

// Width and centre (1/65536 pixels) of the road in row 'r256' (1/256 rows) of a road.
static inline INT64 OutrunRoadLen(INT32 road, INT64 r256) { return (OutrunRoadLenA[road] * r256 >> 8) + OutrunRoadLenB[road]; }
static inline INT64 OutrunRoadCen(INT32 road, INT64 r256) { return (OutrunRoadCenA[road] * r256 >> 8) + OutrunRoadCenB[road]; }

// The row of a road (0-255) to draw a road of width len (1/65536 pixels) from:
// the widest usable one not wider (no pixel of it is skipped), else the narrowest.
static INT32 OutrunRoadWideRow(INT32 road, INT64 len)
{
	INT32 best = -1;
	for (INT32 r = 255; r >= 0; r--) {
		if (!OutrunRoadRows[road * 256 + r].usable) continue;
		if (OutrunRoadLen(road, (INT64)r << 8) <= len) return r;
		best = r;
	}
	return best;
}

// The wider row's pixel for position u (1/65536 pixels) in a road of width
// len and centre cen, drawn from row q.
static inline UINT8 OutrunRoadWidePixel(const UINT8 *wide, INT64 lq, INT64 cq, INT64 len, INT64 cen, INT64 u)
{
	const INT64 qp = (cq + (u - cen) * lq / len) >> 16;
	return (qp >= 0 && qp < 512) ? wide[qp] : 3;
}

static void OutrunRoadBuild(INT32 S)
{
	for (INT32 r = 0; r < 512; r++) {
		const UINT8 *row = System16Roads + r * 512;
		INT32 a = 0, b = 512;
		while (a < 512 && row[a] == 3) a++;
		while (b > a && row[b - 1] == 3) b--;
		OutrunRoadRow &R = OutrunRoadRows[r];
		R.a = a;
		R.b = (a < 512) ? b : a;
		R.ok = 0;
		/* a whole road image (not cut off by the row's ends) with more than one colour on it */
		R.usable = 0;
		if (a > 0 && b < 512 && b - a >= 2)
			for (INT32 p = a + 1; p < b; p++) if (row[p] != row[a]) { R.usable = 1; break; }
	}

	/* straight-line fits of width and centre over the usable rows (the few
	   narrowest are left out: they are all about the same) */
	for (INT32 road = 0; road < 2; road++) {
		INT64 n = 0, sr = 0, srr = 0, sl = 0, srl = 0, sc = 0, src = 0;
		for (INT32 r = 8; r < 256; r++) {
			const OutrunRoadRow &R = OutrunRoadRows[road * 256 + r];
			if (!R.usable) continue;
			const INT64 l = R.b - R.a, c2 = R.a + R.b;
			n++; sr += r; srr += r * r; sl += l; srl += r * l; sc += c2; src += r * c2;
		}
		const INT64 den = n * srr - sr * sr;
		if (n < 2 || den == 0) {
			OutrunRoadLenA[road] = 0; OutrunRoadLenB[road] = 0;
			OutrunRoadCenA[road] = 0; OutrunRoadCenB[road] = 0;
			continue;
		}
		OutrunRoadLenA[road] = ((n * srl - sr * sl) << 16) / den;
		OutrunRoadLenB[road] = ((sl << 16) - OutrunRoadLenA[road] * sr) / n;
		OutrunRoadCenA[road] = ((n * src - sr * sc) << 15) / den;			/* c2 is twice the centre */
		OutrunRoadCenB[road] = ((sc << 15) - OutrunRoadCenA[road] * sr) / n;
	}

	/* a row is drawn finer only if its wider art, scaled down the same way,
	   is mostly the row itself: the road surface (the commonest pixel) and the
	   rest (lines, edges) counted apart (rules out the narrowest rows, which
	   are all the same few pixels, and rows that aren't road images) */
	for (INT32 r = 0; r < 512; r++) {
		OutrunRoadRow &R = OutrunRoadRows[r];
		const INT32 road = r >> 8;
		const INT32 a = R.a, b = R.b;
		if (b - a < 2 || OutrunRoadLenA[road] <= 0) continue;
		const INT64 len = OutrunRoadLen(road, (INT64)(r & 0xff) << 8);
		const INT64 cen = OutrunRoadCen(road, (INT64)(r & 0xff) << 8);
		if (len < 65536) continue;
		/* the row's road must have about the fitted width */
		const INT64 dl = ((INT64)(b - a) << 16) - len;
		if (dl > (len >> 4) + 4 * 65536 || -dl > (len >> 4) + 4 * 65536) continue;
		const INT32 q = OutrunRoadWideRow(road, S * len);
		if (q < 0) continue;
		const INT64 lq = OutrunRoadLen(road, (INT64)q << 8), cq = OutrunRoadCen(road, (INT64)q << 8);
		const UINT8 *row = System16Roads + r * 512, *wide = System16Roads + (road * 256 + q) * 512;

		INT32 count[16];
		memset(count, 0, sizeof(count));
		for (INT32 p = a; p < b; p++) count[row[p] & 0xf]++;
		INT32 surface = 0;
		for (INT32 v = 1; v < 16; v++) if (count[v] > count[surface]) surface = v;

		INT32 match = 0, lines = 0, linematch = 0;
		for (INT32 p = a; p < b; p++) {
			/* about the centre of native pixel p (within a native pixel either
			   way: thin lines needn't line up exactly) */
			bool same = false;
			for (INT32 d = -2; d <= 2 && !same; d++)
				same = (OutrunRoadWidePixel(wide, lq, cq, len, cen, ((INT64)p << 16) + 32768 + d * 32768) == row[p]);
			if (same) match++;
			if (row[p] != surface) {
				lines++;
				if (same) linematch++;
			}
		}
		R.ok = lines && (match * 1000 >= OUTRUN_ROAD_MIN_MATCH * (b - a)) && (linematch * 1000 >= OUTRUN_ROAD_MIN_MATCH * lines);
	}

	OutrunRoadRom = System16Roads;
	OutrunRoadScale = S;
}

static void OutrunRenderRoadBackgroundLayerScaled()
{
	UINT16 *roadram = (UINT16*)System16RoadRam;
	const INT32 S = System16ResScale;
	const INT32 W = 320 * S;

	for (INT32 y = 0; y < 224; y++) {
		INT32 data0 = BURN_ENDIAN_SWAP_INT16(roadram[0x000 + y]);
		INT32 data1 = BURN_ENDIAN_SWAP_INT16(roadram[0x100 + y]);

		INT32 color = -1;

		switch (System16RoadControl & 3) {
			case 0: if (data0 & 0x800) color = data0 & 0x7f; break;
			case 1: if (data0 & 0x800) color = data0 & 0x7f; else if (data1 & 0x800) color = data1 & 0x7f; break;
			case 2: if (data1 & 0x800) color = data1 & 0x7f; else if (data0 & 0x800) color = data0 & 0x7f; break;
			case 3: if (data1 & 0x800) color = data1 & 0x7f; break;
		}

		if (color != -1) {
			UINT16 *pPixel = pTransDraw + (y * S) * W;
			for (INT32 x = 0; x < W * S; x++) pPixel[x] = color | System16RoadColorOffset3;
		}
	}
}

// One native road line's data, as OutrunRenderRoadForegroundLayer() reads it.
struct OutrunRoadLine {
	INT32 data0, data1;
	const UINT8 *src0, *src1;
	INT32 row0, row1;			/* ROM row, -1 for the "no road" row */
	INT32 hpos0, hpos1;			/* start position, as drawn */
	INT32 color0, color1;
};

static void OutrunRoadReadLine(INT32 y, OutrunRoadLine &l)
{
	UINT16 *roadram = (UINT16*)System16RoadRam;
	l.data0 = BURN_ENDIAN_SWAP_INT16(roadram[0x000 + y]);
	l.data1 = BURN_ENDIAN_SWAP_INT16(roadram[0x100 + y]);

	l.row0 = (l.data0 & 0x800) ? -1 : (0x000 + ((l.data0 >> 1) & 0xff));
	l.src0 = (l.data0 & 0x800) ? System16Roads + 256 * 2 * 512 : (System16Roads + (0x000 + ((l.data0 >> 1) & 0xff)) * 512);
	l.hpos0 = BURN_ENDIAN_SWAP_INT16((roadram[0x200 + ((System16RoadControl & 4) ? y : (l.data0 & 0x1ff))])) & 0xfff;
	l.color0 = BURN_ENDIAN_SWAP_INT16(roadram[0x600 + ((System16RoadControl & 4) ? y : (l.data0 & 0x1ff))]);

	l.row1 = (l.data1 & 0x800) ? -1 : (0x100 + ((l.data1 >> 1) & 0xff));
	l.src1 = (l.data1 & 0x800) ? System16Roads + 256 * 2 * 512 : (System16Roads + (0x100 + ((l.data1 >> 1) & 0xff)) * 512);
	l.hpos1 = BURN_ENDIAN_SWAP_INT16((roadram[0x400 + ((System16RoadControl & 4) ? (0x100 + y) : (l.data1 & 0x1ff))])) & 0xfff;
	l.color1 = BURN_ENDIAN_SWAP_INT16(roadram[0x600 + ((System16RoadControl & 4) ? (0x100 + y) : (l.data1 & 0x1ff))]);

	l.hpos0 = (l.hpos0 - (0x5f8 + System16RoadXOffset)) & 0xfff;
	l.hpos1 = (l.hpos1 - (0x5f8 + System16RoadXOffset)) & 0xfff;
}

// One road's pixels for one output line, out[0 .. 320S-1], from the model:
// road row r256 (1/256 rows) and start position h256 (1/256 pixels), drawn
// from the wider art for row qrow256 (the native line's own row, so all the
// finer lines of a native line use the same art and thin lines keep their width).
static void OutrunRoadModelPixels(UINT8 *out, INT32 S, INT32 road, INT64 r256, INT64 h256, INT64 qrow256)
{
	const INT32 W = 320 * S;

	INT64 len = OutrunRoadLen(road, r256);
	const INT64 cen = OutrunRoadCen(road, r256);
	if (len < 256) len = 256;
	INT32 q = OutrunRoadWideRow(road, S * OutrunRoadLen(road, qrow256));
	if (q < 0) q = 0;
	const UINT8 *wide = System16Roads + (road * 256 + q) * 512;
	const INT64 lq = OutrunRoadLen(road, (INT64)q << 8), cq = OutrunRoadCen(road, (INT64)q << 8);

	/* position in the native row at the centre of output pixel X, in 1/65536
	   pixels: (h256 << 8) + (X + 0.5) * 65536 / S, wrapped like hpos; in the
	   wider row: cq + (u - cen) * lq / len, stepped along */
	const INT64 ratio = (lq << 16) / len;					/* 1/65536 */
	const INT32 step = 65536 / S;
	INT64 u = (h256 << 8) + step / 2;
	for (INT32 X = 0; X < W; X++, u += step) {
		const INT64 uw = u & ((INT64)0x1000 * 65536 - 1);
		const INT64 qp = (cq + (((uw - cen) * ratio) >> 16)) >> 16;
		out[X] = (qp >= 0 && qp < 512 && uw < (INT64)0x200 * 65536) ? wide[qp] : 3;
	}
}

// One road's pixels for one output line as the native line shows them, S wide.
static void OutrunRoadNativePixels(UINT8 *out, INT32 S, const UINT8 *src, INT32 hpos)
{
	for (INT32 x = 0; x < 320; x++) {
		const INT32 p = (hpos + x) & 0xfff;
		const UINT8 v = (p < 0x200) ? src[p] : 3;
		for (INT32 k = 0; k < S; k++) *out++ = v;
	}
}

// Whether a road row can be drawn from the model: a row found to match its
// wider art, or one of the narrowest rows (all about the same few pixels, the
// road far away), which is then drawn as narrow as the model has it.
static bool OutrunRoadModelRow(INT32 row)
{
	if (row < 0) return false;
	const OutrunRoadRow &R = OutrunRoadRows[row];
	if (R.ok) return true;
	const INT32 road = row >> 8;
	if ((row & 0xff) >= 16 || R.b <= R.a || OutrunRoadLenA[road] <= 0) return false;
	const INT64 cen = OutrunRoadCen(road, (INT64)(row & 0xff) << 8);
	const INT64 d = ((INT64)(R.a + R.b) << 15) - cen;
	return d >= -4 * 65536 && d <= 4 * 65536;
}

#define OUTRUN_ROAD_SMOOTH		3		/* lines either side averaged */

// Per native line and road, for the model: drawn from it, the row (1/256 rows)
// and start position (1/256 pixels) as the road RAM gives them, whether the
// next line continues the road, and both smoothed along the road.
static UINT8 OutrunRoadOn[2][224], OutrunRoadCont[2][224];
static INT64 OutrunRoadR[2][224], OutrunRoadH[2][224], OutrunRoadSR[2][224], OutrunRoadSH[2][224];

static void OutrunRoadPrepare()
{
	OutrunRoadLine l;
	for (INT32 y = 0; y < 224; y++) {
		OutrunRoadReadLine(y, l);
		const INT32 row[2] = { l.row0, l.row1 }, data[2] = { l.data0, l.data1 }, hpos[2] = { l.hpos0, l.hpos1 };
		for (INT32 k = 0; k < 2; k++) {
			OutrunRoadOn[k][y] = OutrunRoadModelRow(row[k]);
			OutrunRoadR[k][y] = (INT64)(data[k] & 0x1ff) << 7;		/* bits 1-8 the row, bit 0 half a row */
			OutrunRoadH[k][y] = (INT64)hpos[k] << 8;
			OutrunRoadCont[k][y] = 0;
		}
	}

	/* the next line continues the road: drawn from the model too, about the
	   same width and position (hpos taken across its wrap) */
	for (INT32 k = 0; k < 2; k++) {
		for (INT32 y = 0; y < 223; y++) {
			if (!OutrunRoadOn[k][y] || !OutrunRoadOn[k][y + 1]) continue;
			const INT64 dr = OutrunRoadR[k][y + 1] - OutrunRoadR[k][y];
			INT32 dh = (INT32)((OutrunRoadH[k][y + 1] - OutrunRoadH[k][y]) >> 8) & 0xfff;
			if (dh >= 0x800) dh -= 0x1000;
			if (dr < -16 * 256 || dr > 16 * 256 || dh < -16 || dh > 16) continue;
			OutrunRoadCont[k][y] = 1;
			OutrunRoadH[k][y + 1] = OutrunRoadH[k][y] + ((INT64)dh << 8);	/* unwrapped along the road */
		}

		/* the road RAM holds whole rows and pixels, rounded from a smooth road:
		   each line is averaged with up to OUTRUN_ROAD_SMOOTH lines either
		   side (as many each side) along the same stretch of road */
		for (INT32 y = 0; y < 224; y++) {
			INT32 n = 0;
			while (n < OUTRUN_ROAD_SMOOTH && y - n - 1 >= 0 && y + n + 1 < 224 &&
				OutrunRoadCont[k][y - n - 1] && OutrunRoadCont[k][y + n]) n++;
			INT64 sr = 0, sh = 0;
			for (INT32 i = y - n; i <= y + n; i++) { sr += OutrunRoadR[k][i]; sh += OutrunRoadH[k][i]; }
			OutrunRoadSR[k][y] = sr / (2 * n + 1);
			OutrunRoadSH[k][y] = sh / (2 * n + 1);
		}
	}
}

static void OutrunDrawRoadPart(INT32 part)
{
	static const UINT8 priority_map[2][8] =	{
		{ 0x80,0x81,0x81,0x87,0,0,0,0x00 },
		{ 0x81,0x81,0x81,0x8f,0,0,0,0x80 }
	};
	const INT32 S = System16ResScale;
	const INT32 W = 320 * S;
	static UINT8 pixparts[OUTRUN_PARTS][2][320 * 4];
	UINT8 (*pix)[320 * 4] = pixparts[part];

	OutrunRoadLine l;
	for (INT32 y = 0; y < 224; y++) {
		OutrunRoadReadLine(y, l);

		/* if both roads are low priority, skip */
		if ((l.data0 & 0x800) && (l.data1 & 0x800)) continue;

		const INT32 control = System16RoadControl & 3;
		if (control == 0 && (l.data0 & 0x800)) continue;
		if (control == 3 && (l.data1 & 0x800)) continue;

		UINT16 color_table[32];
		UINT8 bgcolor;

		/* determine the 5 colors for road 0 */
		color_table[0x00] = System16RoadColorOffset1 ^ 0x00 ^ ((l.color0 >> 0) & 1);
		color_table[0x01] = System16RoadColorOffset1 ^ 0x02 ^ ((l.color0 >> 1) & 1);
		color_table[0x02] = System16RoadColorOffset1 ^ 0x04 ^ ((l.color0 >> 2) & 1);
		bgcolor = (l.color0 >> 8) & 0xf;
		color_table[0x03] = (l.data0 & 0x200) ? color_table[0x00] : (System16RoadColorOffset2 ^ 0x00 ^ bgcolor);
		color_table[0x07] = System16RoadColorOffset1 ^ 0x06 ^ ((l.color0 >> 3) & 1);

		/* determine the 5 colors for road 1 */
		color_table[0x10] = System16RoadColorOffset1 ^ 0x08 ^ ((l.color1 >> 4) & 1);
		color_table[0x11] = System16RoadColorOffset1 ^ 0x0a ^ ((l.color1 >> 5) & 1);
		color_table[0x12] = System16RoadColorOffset1 ^ 0x0c ^ ((l.color1 >> 6) & 1);
		bgcolor = (l.color1 >> 8) & 0xf;
		color_table[0x13] = (l.data1 & 0x200) ? color_table[0x10] : (System16RoadColorOffset2 ^ 0x10 ^ bgcolor);
		color_table[0x17] = System16RoadColorOffset1 ^ 0x0e ^ ((l.color1 >> 7) & 1);

		const UINT8 *src[2] = { l.src0, l.src1 };
		const INT32 hpos[2] = { l.hpos0, l.hpos1 };

		for (INT32 j = 0; j < S; j++) {
			if (OUTRUN_PART(y * S + j) != part) continue;
			UINT16 *pPixel = pTransDraw + (y * S + j) * W;

			for (INT32 k = 0; k < 2; k++) {
				if ((k == 0 && control == 3) || (k == 1 && control == 0)) continue;
				if (!OutrunRoadOn[k][y]) {
					OutrunRoadNativePixels(pix[k], S, src[k], hpos[k]);
					continue;
				}
				/* finer line j: between this line and the next where it continues the road */
				INT64 r256 = OutrunRoadSR[k][y], h256 = OutrunRoadSH[k][y];
				if (j && OutrunRoadCont[k][y]) {
					r256 += ((OutrunRoadSR[k][y + 1] - r256) * j) / S;
					h256 += ((OutrunRoadSH[k][y + 1] - h256) * j) / S;
				}
				OutrunRoadModelPixels(pix[k], S, k, r256, h256 & ((INT64)0x1000 * 256 - 1), OutrunRoadR[k][y]);
			}

			switch (control) {
				case 0:
					for (INT32 x = 0; x < W; x++) pPixel[x] = color_table[0x00 + pix[0][x]];
					break;
				case 1:
				case 2: {
					const UINT8 *pm = priority_map[control - 1];
					for (INT32 x = 0; x < W; x++) {
						if ((pm[pix[0][x]] >> pix[1][x]) & 1)
							pPixel[x] = color_table[0x10 + pix[1][x]];
						else
							pPixel[x] = color_table[0x00 + pix[0][x]];
					}
					break;
				}
				case 3:
					for (INT32 x = 0; x < W; x++) pPixel[x] = color_table[0x10 + pix[1][x]];
					break;
			}
		}
	}
}

static void OutrunRenderRoadForegroundLayerScaled()
{
	const INT32 S = System16ResScale;
	if (OutrunRoadRom != System16Roads || OutrunRoadScale != S) OutrunRoadBuild(S);
	OutrunRoadPrepare();
	OutrunRunParts(OUTRUN_JOB_ROAD);
}

/* ---- larger sprite art ---- */

// Sprite rows end at a marker pixel (0xf) at a fixed place in a ROM word, which
// depends on the chip and the direction the row is read:
// - Out Run chip: 32-bit words of 8 pixels; reading forward, a row ends after
//   the word whose second-to-last pixel is 0xf; backward, the word whose second
//   pixel is 0xf.
// - System 16B chip (Super Hang-On): 16-bit words of 4 pixels; forward, the
//   word whose last pixel is 0xf; backward, whose first pixel is 0xf.
// Smaller copies of an image (about half as wide and tall) follow the larger
// image in the ROM, as on the Y-Board. OutrunLodFind looks for the larger copy
// of the image a sprite shows, and that copy's own larger copy, and so on: it
// tries images just before the sprite's art, of about twice its pitch and
// rows, and takes one whose every other pixel of every other row (with a small
// offset) is mostly the sprite's art.

#define OUTRUN_LOD_FMT_OUTRUN	0
#define OUTRUN_LOD_FMT_16B		1

#define OUTRUN_LOD_LEVELS		3			/* up to 8x larger */
#define OUTRUN_LOD_ROW_WORDS	64			/* a longer row is not looked at */
#define OUTRUN_LOD_ROW_PIXELS	(OUTRUN_LOD_ROW_WORDS * 8)
#define OUTRUN_LOD_MIN_MATCH	750			/* per mille of the compared pixels */
#define OUTRUN_LOD_TRIES		8			/* searches per image, as more of it is drawn */
#define OUTRUN_LOD_MIN_PIXELS	8			/* visible pixels a sprite needs to be looked up */
#define OUTRUN_LOD_MIN_COMPARED	8			/* pixels compared, at least; below ... */
#define OUTRUN_LOD_TINY_PIXELS	12			/* ... this many, every one must match */
#define OUTRUN_LOD_SMALL_PIXELS	24			/* compared pixels below which ... */
#define OUTRUN_LOD_SMALL_MATCH	850			/* ... the match must be this close (per mille) */

struct OutrunLodLevel {
	UINT16 addr;			/* where the hardware would read row 0 */
	INT16 bank;				/* the sprite ROM bank it is in (-1: the sprite's own) */
	INT16 pitch;
	INT16 offset;			/* pixel j of this level = pixel (j - offset) / 2 of the level below */
	INT16 voff;				/* row i of this level = row (i - voff) / 2 of the level below */
};

struct OutrunLodArt {
	INT32 levels;			/* 0: none */
	INT32 validrow;			/* the sprite's rows after this one are drawn from its own art */
	OutrunLodLevel level[OUTRUN_LOD_LEVELS + 1];	/* [0] = the sprite's own */
};

// Reads one row the way the hardware does, from word addr, into pix (up to
// OUTRUN_LOD_ROW_PIXELS). Returns the pixels read, or -1 if no end marker came
// within OUTRUN_LOD_ROW_WORDS words.
// The searches' work, counted the same on every machine: pixels read and
// compared (so a frame's searches can be limited, see OutrunLodFind).
static UINT32 OutrunLodWork = 0;
static INT32 OutrunLodCounting = 0;		/* set while a search runs (drawing, on two threads, reads rows too) */

static INT32 OutrunLodReadRow(INT32 fmt, const void *bankdata, UINT16 addr, INT32 backward, UINT8 *pix, INT32 maxwords = OUTRUN_LOD_ROW_WORDS)
{
	INT32 n = 0;
	if (maxwords > OUTRUN_LOD_ROW_WORDS) maxwords = OUTRUN_LOD_ROW_WORDS;
	if (fmt == OUTRUN_LOD_FMT_OUTRUN) {
		const UINT32 *d = (const UINT32*)bankdata;
		for (INT32 w = 0; w < maxwords; w++) {
			if (OutrunLodCounting) OutrunLodWork += 8;
			const UINT32 pixels = BURN_ENDIAN_SWAP_INT32(d[(UINT16)(backward ? addr - w : addr + w)]);
			if (!backward) {
				for (INT32 s = 28; s >= 0; s -= 4) pix[n++] = (pixels >> s) & 0xf;
				if ((pixels & 0x000000f0) == 0x000000f0) return n;
			} else {
				for (INT32 s = 0; s <= 28; s += 4) pix[n++] = (pixels >> s) & 0xf;
				if ((pixels & 0x0f000000) == 0x0f000000) return n;
			}
		}
	} else {
		const UINT16 *d = (const UINT16*)bankdata;
		for (INT32 w = 0; w < maxwords; w++) {
			if (OutrunLodCounting) OutrunLodWork += 4;
			const UINT16 pixels = BURN_ENDIAN_SWAP_INT16(d[(UINT16)(backward ? addr - w : addr + w)]);
			if (!backward) {
				for (INT32 s = 12; s >= 0; s -= 4) pix[n++] = (pixels >> s) & 0xf;
				if ((pixels & 0x000f) == 0x000f) return n;
			} else {
				for (INT32 s = 0; s <= 12; s += 4) pix[n++] = (pixels >> s) & 0xf;
				if ((pixels & 0xf000) == 0xf000) return n;
			}
		}
	}
	return -1;
}

static inline bool OutrunLodVisible(UINT8 p) { return p != 0 && p != 15; }

// Row lengths, per bank and direction: the words OutrunLodReadRow reads from
// each address (1 .. OUTRUN_LOD_ROW_WORDS; 0: no end marker in reach), so the
// searches can tell which candidate rows have the right length without
// reading them.
#define OUTRUN_LOD_LEN_TABLES	32

static const void *OutrunLodLenBank[OUTRUN_LOD_LEN_TABLES];
static INT32 OutrunLodLenDir[OUTRUN_LOD_LEN_TABLES];
static UINT8 *OutrunLodLen[OUTRUN_LOD_LEN_TABLES];

static void OutrunLodLenFree()
{
	for (INT32 i = 0; i < OUTRUN_LOD_LEN_TABLES; i++) {
		BurnFree(OutrunLodLen[i]);
		OutrunLodLenBank[i] = NULL;
	}
}

static const UINT8 *OutrunLodRowLengths(INT32 fmt, const void *bankdata, INT32 backward)
{
	INT32 slot = -1;
	for (INT32 i = 0; i < OUTRUN_LOD_LEN_TABLES; i++) {
		if (OutrunLodLen[i] != NULL && OutrunLodLenBank[i] == bankdata && OutrunLodLenDir[i] == backward) return OutrunLodLen[i];
		if (OutrunLodLen[i] == NULL && slot < 0) slot = i;
	}
	if (slot < 0) return NULL;

	UINT8 *len = (UINT8*)BurnMalloc(0x10000);
	if (len == NULL) return NULL;
	OutrunLodWork += 8 * 0x10000;		/* (about the time it takes, as counted elsewhere) */
	/* twice round, so the rows that wrap past the end are counted too */
	INT32 r = 0;
	for (INT32 i = 0; i < 2 * 0x10000; i++) {
		const UINT16 a = (UINT16)(backward ? i : (0xffff - i));
		bool marker;
		if (fmt == OUTRUN_LOD_FMT_OUTRUN) {
			const UINT32 pixels = BURN_ENDIAN_SWAP_INT32(((const UINT32*)bankdata)[a]);
			marker = backward ? ((pixels & 0x0f000000) == 0x0f000000) : ((pixels & 0x000000f0) == 0x000000f0);
		} else {
			const UINT16 pixels = BURN_ENDIAN_SWAP_INT16(((const UINT16*)bankdata)[a]);
			marker = backward ? ((pixels & 0xf000) == 0xf000) : ((pixels & 0x000f) == 0x000f);
		}
		r = marker ? 1 : ((r > 0 && r < OUTRUN_LOD_ROW_WORDS) ? r + 1 : 0);
		len[a] = (UINT8)r;
	}
	OutrunLodLen[slot] = len;
	OutrunLodLenBank[slot] = bankdata;
	OutrunLodLenDir[slot] = backward;
	return len;
}

#define OUTRUN_LOD_CACHE_SIZE	4096		/* power of 2 */

// The kinds of search for a larger copy, stricter first (see OutrunLodFindParents).
#define OUTRUN_LOD_PASS_EXACT		0
#define OUTRUN_LOD_PASS_RELAXED		1
#define OUTRUN_LOD_PASS_REDRAWN		2
#define OUTRUN_LOD_PASS_ADJACENT	3

// A search for the larger copy of an image (OutrunLodFindParents), which may
// go on over several frames.
struct OutrunLodParentSearch {
	INT32 dn;				/* the candidates' row distance to go on from (0: start) */
	INT32 maxpass, top;
	INT32 bestscore[OUTRUN_LOD_PASS_ADJACENT + 1], bestdist[OUTRUN_LOD_PASS_ADJACENT + 1], founddn[OUTRUN_LOD_PASS_ADJACENT + 1];
	bool found[OUTRUN_LOD_PASS_ADJACENT + 1];
	OutrunLodLevel level[OUTRUN_LOD_PASS_ADJACENT + 1];
};

// A candidate of the search for a redrawn copy (OutrunLodFarStep).
#define OUTRUN_FAR_TOP	4
struct OutrunLodFarCand {
	INT32 score;			/* -1: none */
	UINT16 addr;
	INT16 pitch, offset;
	INT16 ok;				/* alike enough to be taken (else only a rival) */
};

struct OutrunLodCacheEntry {
	UINT32 key;				/* 0: empty */
	INT16 k0, k1;			/* the rows looked at (by the search under way) */
	INT16 tries;			/* searches made */
	INT8 searching;			/* a search is under way (it goes on in later frames) */
	INT8 started;			/* its current attempt has begun (a0, a1 set) */
	INT16 trim;				/* its attempt: rows left off at the end */
	INT16 a0, a1;			/* the rows of its current level used */
	INT16 shift;			/* the next row shift to look from (see OutrunLodSearchStep) */
	INT16 bestpass;			/* the best found for the current level so far */
	INT16 bestshift;
	OutrunLodLevel bestfrom, bestnext;
	OutrunLodParentSearch ps;	/* the search from the current shift (ps.dn > 0: under way) */
	INT16 xbank;			/* the search in all banks (OutrunLodSearchBank): the next bank, -1 not under way */
	INT16 xdone;			/* it was made, per level (bit) */
	INT16 xfound;			/* the levels found by a search of every bank or the bank (bit) */
	INT32 xscore;			/* its best so far (-1 none) */
	OutrunLodLevel xbest;
	INT32 fnext;			/* the search for a redrawn copy before the image (OutrunLodFarStep): the next candidate, -1 not under way */
	INT16 fdone;			/* it was made */
	OutrunLodFarCand ftop[OUTRUN_FAR_TOP];	/* its best distinct candidates so far */
	INT8 mirrored;			/* (flipped images) its twin's larger copies were looked at (OutrunLodFind) */
	OutrunLodArt art;		/* the best found so far (drawn) */
	OutrunLodArt work;		/* the search's own so far */
};

static OutrunLodCacheEntry *OutrunLodCache = NULL;

#define OUTRUN_LOD_FRAME_WORK	600000		/* search work per frame (see OutrunLodWork): about 1-2 ms */
static UINT32 OutrunLodFrameWork = 0;		/* done this frame */
static const void *OutrunLodCacheRom = NULL;

static UINT8 OutrunLodChildRows[2][OUTRUN_LOD_ROW_PIXELS];

#define OUTRUN_LOD_MAX_ROWS		512			/* a taller image is not looked up */
#define OUTRUN_LOD_SAMPLES		6			/* rows compared first */
#define OUTRUN_LOD_MAX_OFFSET	12			/* pixels the larger image may be shifted by */

static UINT8 OutrunLodCRow[OUTRUN_LOD_MAX_ROWS][OUTRUN_LOD_ROW_PIXELS];	/* the child image's rows */
static INT16 OutrunLodCLen[OUTRUN_LOD_MAX_ROWS];

// Compares child row k (in OutrunLodCRow) with a parent row, at pixel offset
// 'offset': adds the pixels visible in either and those equal.
static void OutrunLodCompareRow(INT32 k, const UINT8 *ppix, INT32 pn, INT32 offset, INT32 *same, INT32 *total)
{
	const UINT8 *cpix = OutrunLodCRow[k];
	const INT32 cn = OutrunLodCLen[k];
	OutrunLodWork += cn;
	for (INT32 i = 0; i < cn; i++) {
		const INT32 j = 2 * i + offset;
		const UINT8 c = cpix[i];
		const UINT8 p = (j >= 0 && j < pn) ? ppix[j] : 0;
		const bool cv = OutrunLodVisible(c), pv = OutrunLodVisible(p);
		if (!cv && !pv) continue;
		(*total)++;
		if (cv && pv && c == p) (*same)++;
	}
}

// The relaxed comparison (second search): child row k against parent rows
// 2k and 2k+1: visible where either is, and child pixels found in the 2x2
// parent pixels they cover (redrawn copies of textured art keep the texture's
// period instead of sampling it).
static void OutrunLodCompareRelaxed(INT32 k, const UINT8 *p0, INT32 n0, const UINT8 *p1, INT32 n1, INT32 offset,
	INT32 *maskunion, INT32 *maskboth, INT32 *visible, INT32 *found)
{
	const UINT8 *cpix = OutrunLodCRow[k];
	const INT32 cn = OutrunLodCLen[k];
	OutrunLodWork += 2 * cn;
	for (INT32 i = 0; i < cn; i++) {
		const INT32 j = 2 * i + offset;
		const UINT8 c = cpix[i];
		UINT8 q[4];
		q[0] = (j >= 0 && j < n0) ? p0[j] : 0;
		q[1] = (j + 1 >= 0 && j + 1 < n0) ? p0[j + 1] : 0;
		q[2] = (j >= 0 && j < n1) ? p1[j] : 0;
		q[3] = (j + 1 >= 0 && j + 1 < n1) ? p1[j + 1] : 0;
		const bool cv = OutrunLodVisible(c), pv = OutrunLodVisible(q[0]);
		if (cv || pv) (*maskunion)++;
		if (cv && pv) (*maskboth)++;
		if (cv) {
			(*visible)++;
			if (c == q[0] || c == q[1] || c == q[2] || c == q[3]) (*found)++;
		}
	}
}

// The redrawn comparison (third search): child pixels found among the 4x4
// parent pixels around the 2x2 they cover (parent rows 2k-1 .. 2k+2): sizes
// of an image drawn one by one by an artist, not scaled.
static void OutrunLodCompareRedrawn(INT32 k, const UINT8 *const *p, const INT32 *n, INT32 offset, INT32 *found)
{
	const UINT8 *cpix = OutrunLodCRow[k];
	const INT32 cn = OutrunLodCLen[k];
	OutrunLodWork += 4 * cn;
	for (INT32 i = 0; i < cn; i++) {
		const UINT8 c = cpix[i];
		if (!OutrunLodVisible(c)) continue;
		const INT32 j = 2 * i + offset;
		bool hit = false;
		for (INT32 r = 0; r < 4 && !hit; r++)
			for (INT32 jj = j - 1; jj <= j + 2; jj++)
				if (jj >= 0 && jj < n[r] && p[r][jj] == c) { hit = true; break; }
		if (hit) (*found)++;
	}
}

#define OUTRUN_LOD_RELAX_MASK		900		/* per mille: visible in both, of visible in either */
#define OUTRUN_LOD_RELAX_FOUND		950		/* per mille of the child's visible pixels */
#define OUTRUN_LOD_RELAX_EXACT		500		/* per mille, the plain comparison */
#define OUTRUN_LOD_REDRAWN_MASK		880		/* the same, third search */
#define OUTRUN_LOD_REDRAWN_FOUND	900
#define OUTRUN_LOD_ADJACENT_MASK	800		/* the same, fourth search (adjacent images only) */
#define OUTRUN_LOD_ADJACENT_FOUND	750
#define OUTRUN_LOD_ADJACENT_EXACT	400


// A candidate parent image's rows, read once for all the offsets tried:
// row r (paddr + r * ppitch) as OutrunLodReadRow would read it with up to lim
// words; returns its pixels' count, or -1.
#define OUTRUN_LOD_PROWS	(2 * OUTRUN_LOD_MAX_ROWS + 3)

static UINT8 OutrunLodPRow[OUTRUN_LOD_PROWS][OUTRUN_LOD_ROW_PIXELS];
static UINT32 OutrunLodPRowStamp[OUTRUN_LOD_PROWS];
static UINT32 OutrunLodPStamp = 0;			/* the candidate being compared */

static INT32 OutrunLodParentRow(INT32 fmt, const void *bank, INT32 backward, const UINT8 *rowlen, UINT16 paddr, INT32 ppitch,
	INT32 r, INT32 rbase, INT32 lim, const UINT8 **pix)
{
	const UINT16 a = (UINT16)(paddr + r * ppitch);
	const INT32 i = r - rbase;
	*pix = OutrunLodPRow[i];
	if (rowlen == NULL) return OutrunLodReadRow(fmt, bank, a, backward, OutrunLodPRow[i], lim);
	const INT32 w = rowlen[a];
	if (w == 0 || w > lim || w > OUTRUN_LOD_ROW_WORDS) return -1;
	if (OutrunLodPRowStamp[i] != OutrunLodPStamp) {
		OutrunLodReadRow(fmt, bank, a, backward, OutrunLodPRow[i], w);
		OutrunLodPRowStamp[i] = OutrunLodPStamp;
	}
	return w * ((fmt == OUTRUN_LOD_FMT_OUTRUN) ? 8 : 4);
}

// The search for the larger copy, all kinds at once (passes OUTRUN_LOD_PASS_EXACT
// .. maxpass, stricter first): for each, the best candidate (found[pass]), as
// separate searches would give, for the strictest pass that finds one (looser
// ones stop being compared once a stricter one has found one). The candidates
// are the same for all, so they are read once.
static bool OutrunLodFindParents(INT32 fmt, const void *bank, INT32 backward, UINT16 addr, INT32 pitch,
	INT32 k0, INT32 k1, OutrunLodParentSearch &ps)
{
	static UINT8 ppix[OUTRUN_LOD_SAMPLES][OUTRUN_LOD_ROW_PIXELS];
	INT32 pn[OUTRUN_LOD_SAMPLES], sample[OUTRUN_LOD_SAMPLES];
	const UINT8 *rowlen = OutrunLodRowLengths(fmt, bank, backward);
	const UINT32 work0 = OutrunLodWork;
	bool *found = ps.found;
	OutrunLodLevel *level = ps.level;
	INT32 *bestscore = ps.bestscore, *bestdist = ps.bestdist, *founddn = ps.founddn;

	INT32 maxpass = ps.maxpass;
	if (maxpass >= OUTRUN_LOD_PASS_ADJACENT && pitch < 0) maxpass = OUTRUN_LOD_PASS_REDRAWN;
	if (ps.dn == 0) {
		for (INT32 p = 0; p <= OUTRUN_LOD_PASS_ADJACENT; p++) { found[p] = false; bestscore[p] = -1; bestdist[p] = 0; founddn[p] = -1; }
		ps.top = maxpass;		/* the loosest pass still of use */
	}
	if (pitch == 0) return true;
	const INT32 rows = k1 - k0 + 1;
	if (rows < 2) return true;

	INT32 samples = (rows < OUTRUN_LOD_SAMPLES) ? rows : OUTRUN_LOD_SAMPLES;
	for (INT32 s = 0; s < samples; s++) sample[s] = k0 + (samples > 1 ? (s * (rows - 1)) / (samples - 1) : 0);

	const INT32 ppw = (fmt == OUTRUN_LOD_FMT_OUTRUN) ? 8 : 4;	/* pixels per word */
	static const INT32 dp[5] = { 0, -1, 1, -2, 2 };
	INT32 nmin[5];
	for (INT32 di = 0; di < 5; di++) {
		const INT32 ppitch = 2 * pitch + ((pitch > 0) ? dp[di] : -dp[di]);
		/* the parent's rows 0 .. 2 * k1 + 1 end about where the child's row k0
		   begins (the larger image is stored first) */
		nmin[di] = (ppitch != 0) ? (2 * k1 + 2) - (k0 * pitch) / ppitch - 2 : 0;
	}

	/* per pass: the first candidates found and those up to four rows further:
	   the same image lined up a row or a word differently can match as well,
	   and the one with the smallest pixel offset reads whole rows */
	const INT32 nrange = rows / 4 + 8;
	INT32 top = ps.top;
	for (INT32 dn = ps.dn; dn <= nrange; dn++) {
		/* done when the strictest pass has found one and seen its window (a
		   looser pass's find still leaves the stricter ones looking further) */
		if (top == OUTRUN_LOD_PASS_EXACT && founddn[top] >= 0 && dn > founddn[top] + 4) break;

		for (INT32 di = 0; di < 5; di++) {
			const INT32 ppitch = 2 * pitch + ((pitch > 0) ? dp[di] : -dp[di]);
			if (ppitch == 0 || (ppitch > 0) != (pitch > 0)) continue;
			const INT32 n = nmin[di] + dn;
			const INT32 subs = (ppitch > 0) ? ppitch : -ppitch;
			/* the larger image's rows needn't line up with the smaller one's */
			for (INT32 e = 0; e < subs; e++) {
			const UINT16 paddr = (UINT16)(addr - n * ppitch - e);

			/* the passes this candidate is tried for (the window of each pass
			   that found one counts from its own first find) */
			INT32 maxp = top;
			bool adjacent = false;
			if (maxp == OUTRUN_LOD_PASS_ADJACENT) {
				/* only an image stored right before this one (rows of ppitch
				   words, the last ending where this image's first row's words
				   begin), starting up to three rows before paddr */
				const UINT16 end = backward ? (UINT16)(addr - pitch + ppitch) : addr;
				for (INT32 v = 0; v <= 3 && !adjacent; v++) {
					const INT32 d = (UINT16)(end - (UINT16)(paddr - v * ppitch));
					const INT32 R = d / ppitch;
					if (d % ppitch == 0 && R >= 2 * (k1 + 1) - 2 && R <= 4 * (k1 + 1) + 8) adjacent = true;
				}
				if (!adjacent) maxp = OUTRUN_LOD_PASS_REDRAWN;
			}

			/* the sample rows: same width (about twice) */
			OutrunLodWork += 4;
			bool fits = true;
			if (rowlen != NULL) {
				/* told by the lengths alone first (as read below) */
				for (INT32 s = 0; s < samples && fits; s++) {
					const INT32 cn = OutrunLodCLen[sample[s] - k0];
					const INT32 maxw = (2 * cn + 32) / ppw + 1;
					const INT32 w = rowlen[(UINT16)(paddr + 2 * sample[s] * ppitch)];
					if (w == 0 || w > maxw || w * ppw < 2 * cn - 32 || w * ppw > 2 * cn + 32) fits = false;
				}
				if (!fits) continue;
			}
			for (INT32 s = 0; s < samples && fits; s++) {
				const INT32 cn = OutrunLodCLen[sample[s] - k0];
				pn[s] = OutrunLodReadRow(fmt, bank, (UINT16)(paddr + 2 * sample[s] * ppitch), backward, ppix[s], (2 * cn + 32) / ppw + 1);
				if (pn[s] < 0 || pn[s] < 2 * cn - 32 || pn[s] > 2 * cn + 32) fits = false;
			}
			if (!fits) continue;

			/* the offsets that line up the first or last visible pixels of the
			   sample rows, give or take a few */
			UINT8 tryoff[2 * OUTRUN_LOD_MAX_OFFSET + 1];
			memset(tryoff, 0, sizeof(tryoff));
			INT32 tries = 0;
			for (INT32 s = 0; s < samples; s++) {
				const UINT8 *cp = OutrunLodCRow[sample[s] - k0];
				const INT32 cn = OutrunLodCLen[sample[s] - k0];
				INT32 fc = 0, fp = 0, lc = cn - 1, lp = pn[s] - 1;
				while (fc < cn && !OutrunLodVisible(cp[fc])) fc++;
				while (fp < pn[s] && !OutrunLodVisible(ppix[s][fp])) fp++;
				while (lc >= 0 && !OutrunLodVisible(cp[lc])) lc--;
				while (lp >= 0 && !OutrunLodVisible(ppix[s][lp])) lp--;
				if (fc >= cn || fp >= pn[s]) continue;
				const INT32 est[2] = { fp - 2 * fc, lp - 2 * lc - 1 };
				for (INT32 e2 = 0; e2 < 2; e2++)
					for (INT32 o = est[e2] - 2; o <= est[e2] + 2; o++)
						if (o >= -OUTRUN_LOD_MAX_OFFSET && o <= OUTRUN_LOD_MAX_OFFSET && !tryoff[o + OUTRUN_LOD_MAX_OFFSET]) {
							tryoff[o + OUTRUN_LOD_MAX_OFFSET] = 1;
							tries++;
						}
			}
			if (tries == 0) continue;
			OutrunLodPStamp++;		/* a new candidate: its rows are read anew */
			if (OutrunLodPStamp == 0) {
				memset(OutrunLodPRowStamp, 0, sizeof(OutrunLodPRowStamp));
				OutrunLodPStamp = 1;
			}
			for (INT32 offset = -OUTRUN_LOD_MAX_OFFSET; offset <= OUTRUN_LOD_MAX_OFFSET; offset++) {
				if (!tryoff[offset + OUTRUN_LOD_MAX_OFFSET]) continue;
				INT32 same = 0, total = 0;
				for (INT32 s = 0; s < samples; s++) OutrunLodCompareRow(sample[s] - k0, ppix[s], pn[s], offset, &same, &total);
				/* the loosest pass tried decides what is read */
				const INT32 minexact = (maxp == OUTRUN_LOD_PASS_EXACT) ? OUTRUN_LOD_MIN_MATCH : (maxp == OUTRUN_LOD_PASS_ADJACENT) ? OUTRUN_LOD_ADJACENT_EXACT : OUTRUN_LOD_RELAX_EXACT;
				if (total == 0 || same * 1000 < (minexact - 100) * total) continue;
				const bool relax = (maxp != OUTRUN_LOD_PASS_EXACT);

				/* all rows */
				same = total = 0;
				INT32 maskunion = 0, maskboth = 0, visible = 0, nfound = 0, nearfound = 0;
				bool ok = true;
				for (INT32 k = k0; k <= k1 && ok; k++) {
					const INT32 lim = (2 * OutrunLodCLen[k - k0] + 32) / ppw + 1;
					const UINT8 *pr[4];
					INT32 nn[4];
					nn[1] = OutrunLodParentRow(fmt, bank, backward, rowlen, paddr, ppitch, 2 * k, 2 * k0 - 1, lim, &pr[1]);
					if (nn[1] < 0) { ok = false; break; }
					OutrunLodCompareRow(k - k0, pr[1], nn[1], offset, &same, &total);
					if (relax) {
						nn[2] = OutrunLodParentRow(fmt, bank, backward, rowlen, paddr, ppitch, 2 * k + 1, 2 * k0 - 1, lim, &pr[2]);
						if (nn[2] < 0) nn[2] = 0;
						OutrunLodCompareRelaxed(k - k0, pr[1], nn[1], pr[2], nn[2], offset, &maskunion, &maskboth, &visible, &nfound);
						if (maxp >= OUTRUN_LOD_PASS_REDRAWN) {
							nn[0] = OutrunLodParentRow(fmt, bank, backward, rowlen, paddr, ppitch, 2 * k - 1, 2 * k0 - 1, lim, &pr[0]);
							nn[3] = OutrunLodParentRow(fmt, bank, backward, rowlen, paddr, ppitch, 2 * k + 2, 2 * k0 - 1, lim, &pr[3]);
							if (nn[0] < 0) nn[0] = 0;
							if (nn[3] < 0) nn[3] = 0;
							OutrunLodCompareRedrawn(k - k0, pr, nn, offset, &nearfound);
						}
					}
				}
				if (!ok || total < OUTRUN_LOD_MIN_COMPARED) continue;
				const INT32 exact = (same * 1000) / total;
				const INT32 dist = (offset < 0 ? -offset : offset) * 8 + di;
				for (INT32 pass = OUTRUN_LOD_PASS_EXACT; pass <= maxp; pass++) {
					if (founddn[pass] >= 0 && dn > founddn[pass] + 4) continue;	/* this pass's window is over */
					INT32 score = exact;
					if (pass == OUTRUN_LOD_PASS_EXACT) {
						if (exact < OUTRUN_LOD_MIN_MATCH) continue;
						if (total < OUTRUN_LOD_SMALL_PIXELS && exact < OUTRUN_LOD_SMALL_MATCH) continue;
						if (total < OUTRUN_LOD_TINY_PIXELS && same < total) continue;
					} else if (pass == OUTRUN_LOD_PASS_RELAXED) {
						if (total < OUTRUN_LOD_SMALL_PIXELS || exact < OUTRUN_LOD_RELAX_EXACT) continue;
						if (maskboth * 1000 < OUTRUN_LOD_RELAX_MASK * maskunion) continue;
						if (visible == 0 || nfound * 1000 < OUTRUN_LOD_RELAX_FOUND * visible) continue;
						score = (nfound * 1000) / visible;
					} else if (pass == OUTRUN_LOD_PASS_REDRAWN) {
						if (total < OUTRUN_LOD_SMALL_PIXELS || exact < OUTRUN_LOD_RELAX_EXACT) continue;
						if (maskboth * 1000 < OUTRUN_LOD_REDRAWN_MASK * maskunion) continue;
						if (visible == 0 || nearfound * 1000 < OUTRUN_LOD_REDRAWN_FOUND * visible) continue;
						score = (maskboth * 1000) / maskunion + (nearfound * 1000) / visible;
					} else {
						if (total < OUTRUN_LOD_SMALL_PIXELS || exact < OUTRUN_LOD_ADJACENT_EXACT) continue;
						if (maskboth * 1000 < OUTRUN_LOD_ADJACENT_MASK * maskunion) continue;
						if (visible == 0 || nearfound * 1000 < OUTRUN_LOD_ADJACENT_FOUND * visible) continue;
						score = (maskboth * 1000) / maskunion + (nearfound * 1000) / visible;
					}
					if (score > bestscore[pass] || (score == bestscore[pass] && dist < bestdist[pass])) {
						bestscore[pass] = score; bestdist[pass] = dist;
						level[pass].addr = paddr; level[pass].pitch = ppitch; level[pass].offset = offset; level[pass].voff = 0;
						found[pass] = true;
						if (founddn[pass] < 0) founddn[pass] = dn;
					}
				}
			}
			}
		}
		/* passes looser than one that found one are of no use any more */
		for (INT32 p = 0; p < top; p++) if (found[p]) { top = p; break; }

		/* the frame's work done: the rest in a later frame */
		if (dn < nrange && OutrunLodFrameWork + (OutrunLodWork - work0) >= OUTRUN_LOD_FRAME_WORK) {
			ps.dn = dn + 1;
			ps.top = top;
			return false;
		}
	}
	return true;
}

// Reads rows k0..k1 of an image into OutrunLodCRow; false if a row has no end
// marker in reach or there are too many rows. Counts the visible pixels.
static bool OutrunLodReadImage(INT32 fmt, const void *bank, INT32 backward, UINT16 addr, INT32 pitch, INT32 k0, INT32 k1, INT32 *visible)
{
	if (k1 - k0 + 1 > OUTRUN_LOD_MAX_ROWS) return false;
	*visible = 0;
	for (INT32 k = k0; k <= k1; k++) {
		const INT32 n = OutrunLodReadRow(fmt, bank, (UINT16)(addr + k * pitch), backward, OutrunLodCRow[k - k0]);
		if (n < 0) return false;
		OutrunLodCLen[k - k0] = n;
		for (INT32 i = 0; i < n; i++) if (OutrunLodVisible(OutrunLodCRow[k - k0][i])) (*visible)++;
	}
	return true;
}

/* ---- the larger copy elsewhere in the ROM ---- */

// Some games keep an image's larger copy in another part of the sprite ROM
// (Racing Hero: the receding motorcycle 4:28ef, its 2:1 copy at 5:c69e), out
// of the search's reach. When the search finds none for the sprite's own image
// (a big enough one), every bank is looked through by an index: for each ROM
// address, a fingerprint of the row read from there, 8 of every other pixel
// from its first visible one (or the one after); a row of the image with the
// same 8 pixels from its first visible one gives the candidates, which are
// compared in full (the strict comparison only, held to more: these
// candidates are not near the image). Out Run's sprite format only.

#define OUTRUN_LINK_MATCH		850			/* per mille, exact */
#define OUTRUN_LINK_PIXELS		48			/* compared, at least */
#define OUTRUN_LINK_MIN_ROWS	16			/* images this tall at least, */
#define OUTRUN_LINK_MIN_VISIBLE	96			/* with this many visible pixels */
#define OUTRUN_LINK_FP			8			/* pixels in a fingerprint */
#define OUTRUN_LINK_INDEXES		16			/* banks x directions */
#define OUTRUN_LINK_TRIED		128			/* candidates compared, at most, per bank */
#define OUTRUN_LINK_MAX_HITS	16			/* a fingerprint found this often or less is used */

static const UINT32 *OutrunLodSpriteBase = NULL;	/* (Out Run format) */

static INT32 OutrunLodNumBanks = 0;

static const void *OutrunLodLevelData(const OutrunLodLevel &l, const void *bankdata)
{
	return (l.bank >= 0) ? (const void*)(OutrunLodSpriteBase + 0x10000 * l.bank) : bankdata;
}

#define OUTRUN_LINK_HASH		4096		/* index chains */
#define OUTRUN_LINK_BUILD		0x2000		/* addresses indexed a step */

struct OutrunLodIndex {
	const void *data;
	INT32 backward;
	INT32 built;			/* addresses indexed so far (0x10000: all) */
	INT32 n;
	INT32 *head;			/* per hash, the last entry (-1 none) */
	INT32 *next;			/* per entry, the one before with the same hash */
	UINT16 *fp, *addr;		/* per entry: fingerprint, address */
};
static OutrunLodIndex OutrunLodIndexes[OUTRUN_LINK_INDEXES];

static void OutrunLodIndexFree()
{
	for (INT32 i = 0; i < OUTRUN_LINK_INDEXES; i++) {
		OutrunLodIndex &x = OutrunLodIndexes[i];
		BurnFree(x.head); BurnFree(x.next); BurnFree(x.fp); BurnFree(x.addr);
		x.data = NULL; x.n = 0; x.built = 0;
	}
}

// 16-bit fingerprint of OUTRUN_LINK_FP pixels, every 'step'-th from pix[0].
static inline UINT32 OutrunLodFingerprint(const UINT8 *pix, INT32 step)
{
	UINT32 h = 2166136261u;
	for (INT32 i = 0; i < OUTRUN_LINK_FP; i++) {
		const UINT8 p = pix[i * step];
		h = (h ^ (OutrunLodVisible(p) ? p : 0)) * 16777619u;
	}
	return (h ^ (h >> 16)) & 0xffff;
}

// The index of a bank and reading direction, built OUTRUN_LINK_BUILD addresses
// a call (complete when built is 0x10000). NULL if there is no room.
static OutrunLodIndex *OutrunLodGetIndex(const void *data, INT32 backward)
{
	static UINT8 pix[OUTRUN_LOD_ROW_PIXELS];
	OutrunLodIndex *x = NULL;
	for (INT32 i = 0; i < OUTRUN_LINK_INDEXES && x == NULL; i++)
		if (OutrunLodIndexes[i].head && OutrunLodIndexes[i].data == data && OutrunLodIndexes[i].backward == backward) x = &OutrunLodIndexes[i];
	if (x == NULL) {
		for (INT32 i = 0; i < OUTRUN_LINK_INDEXES && x == NULL; i++) if (OutrunLodIndexes[i].head == NULL) x = &OutrunLodIndexes[i];
		if (x == NULL) return NULL;
		x->head = (INT32*)BurnMalloc(OUTRUN_LINK_HASH * sizeof(INT32));
		x->next = (INT32*)BurnMalloc(2 * 0x10000 * sizeof(INT32));
		x->fp = (UINT16*)BurnMalloc(2 * 0x10000 * sizeof(UINT16));
		x->addr = (UINT16*)BurnMalloc(2 * 0x10000 * sizeof(UINT16));
		if (!x->head || !x->next || !x->fp || !x->addr) {
			BurnFree(x->head); BurnFree(x->next); BurnFree(x->fp); BurnFree(x->addr);
			return NULL;
		}
		for (INT32 h = 0; h < OUTRUN_LINK_HASH; h++) x->head[h] = -1;
		x->data = data; x->backward = backward; x->built = 0; x->n = 0;
	}
	if (x->built < 0x10000) {
		const UINT8 *rowlen = OutrunLodRowLengths(OUTRUN_LOD_FMT_OUTRUN, data, backward);
		if (rowlen == NULL) return NULL;
		const INT32 end = x->built + OUTRUN_LINK_BUILD;
		OutrunLodWork += 24 * OUTRUN_LINK_BUILD;	/* (about the time it takes, as counted elsewhere) */
		for (INT32 a = x->built; a < end; a++) {
			const INT32 w = rowlen[a];
			if (w == 0) continue;
			const INT32 pn = OutrunLodReadRow(OUTRUN_LOD_FMT_OUTRUN, data, (UINT16)a, backward, pix, w);
			INT32 f = 0;
			while (f < pn && !OutrunLodVisible(pix[f])) f++;
			for (INT32 q = 0; q < 2; q++) {
				if (f + q + 2 * (OUTRUN_LINK_FP - 1) >= pn) continue;
				const UINT32 fp = OutrunLodFingerprint(pix + f + q, 2);
				const INT32 h = fp & (OUTRUN_LINK_HASH - 1);
				x->fp[x->n] = (UINT16)fp; x->addr[x->n] = (UINT16)a;
				x->next[x->n] = x->head[h]; x->head[h] = x->n;
				x->n++;
			}
		}
		x->built = end;
	}
	return x;
}

static bool OutrunLodLinkCompare(const void *data, INT32 backward, UINT16 addr, INT32 ppitch, INT32 k0, INT32 k1,
	INT32 *bestscore, UINT16 *bestaddr, INT32 *bestoffset);

// The candidates in one bank for the image whose rows k0..k1 are in
// OutrunLodCRow (Out Run format): rows of it with the fingerprint of one of
// the image's rows; each compared in full. Updates the best.
static void OutrunLodSearchBank(INT32 bank, INT32 backward, INT32 pitch, INT32 k0, INT32 k1, INT32 *bestscore, OutrunLodLevel *best)
{
	static UINT16 triedaddr[OUTRUN_LINK_TRIED];
	static INT16 triedpitch[OUTRUN_LINK_TRIED];
	const void *data = OutrunLodSpriteBase + 0x10000 * bank;
	const OutrunLodIndex *x = OutrunLodGetIndex(data, backward);
	if (x == NULL || x->built < 0x10000 || pitch == 0) return;
	INT32 tried = 0;

	const INT32 rows = k1 - k0 + 1;
	static const INT32 dp[5] = { 0, -1, 1, -2, 2 };
	for (INT32 si = 0; si < 8; si++) {
		const INT32 k = k0 + (si * (rows - 1)) / 7;
		const UINT8 *cp = OutrunLodCRow[k - k0];
		const INT32 cn = OutrunLodCLen[k - k0];
		INT32 f = 0;
		while (f < cn && !OutrunLodVisible(cp[f])) f++;
		if (f + OUTRUN_LINK_FP > cn) continue;
		/* (8 pixels of three colours at least: plain runs are everywhere) */
		INT32 colours = 0;
		UINT32 seen = 0;
		for (INT32 i = 0; i < OUTRUN_LINK_FP; i++) {
			const UINT8 p = OutrunLodVisible(cp[f + i]) ? cp[f + i] : 0;
			if (!(seen & (1 << p))) { seen |= 1 << p; colours++; }
		}
		if (colours < 3) continue;
		const UINT32 fp = OutrunLodFingerprint(cp + f, 1);

		/* the index entries with this fingerprint (a common one is no help) */
		INT32 hits = 0;
		for (INT32 i = x->head[fp & (OUTRUN_LINK_HASH - 1)]; i >= 0; i = x->next[i]) hits += (x->fp[i] == fp);
		OutrunLodWork += 4 * hits + 64;
		if (hits > OUTRUN_LINK_MAX_HITS) continue;
		for (INT32 i = x->head[fp & (OUTRUN_LINK_HASH - 1)]; i >= 0; i = x->next[i]) {
			if (x->fp[i] != fp) continue;
			const UINT16 a = x->addr[i];
			for (INT32 di = 0; di < 5; di++) {
				const INT32 ppitch = 2 * pitch + ((pitch > 0) ? dp[di] : -dp[di]);
				if (ppitch == 0 || (ppitch > 0) != (pitch > 0)) continue;
				for (INT32 r = 2 * k; r <= 2 * k + 1; r++) {
					const UINT16 paddr = (UINT16)(a - r * ppitch);
					bool seen = false;
					for (INT32 t = 0; t < tried && !seen; t++) seen = (triedaddr[t] == paddr && triedpitch[t] == ppitch);
					if (seen) continue;
					if (tried >= OUTRUN_LINK_TRIED) return;
					triedaddr[tried] = paddr; triedpitch[tried] = (INT16)ppitch; tried++;
					OutrunLodWork += 256;
					UINT16 addr = 0;
					INT32 offset = best->offset;
					if (OutrunLodLinkCompare(data, backward, paddr, ppitch, k0, k1, bestscore, &addr, &offset)) {
						best->addr = addr; best->bank = (INT16)bank; best->pitch = (INT16)ppitch; best->offset = (INT16)offset; best->voff = 0;
					}
				}
			}
		}
	}
}

// Compares the image whose rows k0..k1 are in OutrunLodCRow with the 2:1
// candidate of pitch ppitch at addr in 'data' (Out Run format). Updates the
// best (score, address, offset); true if it is better.
static bool OutrunLodLinkCompare(const void *data, INT32 backward, UINT16 addr, INT32 ppitch, INT32 k0, INT32 k1,
	INT32 *bestscore, UINT16 *bestaddr, INT32 *bestoffset)
{
	bool better = false;
	static UINT8 ppix[OUTRUN_LOD_SAMPLES][OUTRUN_LOD_ROW_PIXELS], prow[OUTRUN_LOD_ROW_PIXELS];
	INT32 pn[OUTRUN_LOD_SAMPLES], sample[OUTRUN_LOD_SAMPLES];
	const UINT8 *rowlen = OutrunLodRowLengths(OUTRUN_LOD_FMT_OUTRUN, data, backward);
	const INT32 rows = k1 - k0 + 1;
	if (rowlen == NULL || rows < 2) return false;
	const INT32 samples = (rows < OUTRUN_LOD_SAMPLES) ? rows : OUTRUN_LOD_SAMPLES;
	for (INT32 s = 0; s < samples; s++) sample[s] = k0 + (samples > 1 ? (s * (rows - 1)) / (samples - 1) : 0);

	do {
		const UINT16 paddr = addr;

		/* every row's length (about twice: 2 words either way) */
		bool fits = true;
		for (INT32 k = k0; k <= k1 && fits; k++) {
			const INT32 cn = OutrunLodCLen[k - k0];
			const INT32 w = rowlen[(UINT16)(paddr + 2 * k * ppitch)];
			if (w == 0 || w * 8 < 2 * cn - 16 || w * 8 > 2 * cn + 16) fits = false;
		}
		if (!fits) continue;

		for (INT32 s = 0; s < samples; s++)
			pn[s] = OutrunLodReadRow(OUTRUN_LOD_FMT_OUTRUN, data, (UINT16)(paddr + 2 * sample[s] * ppitch), backward, ppix[s]);

		/* the offsets that line up the first or last visible pixels */
		UINT8 tryoff[2 * OUTRUN_LOD_MAX_OFFSET + 1];
		memset(tryoff, 0, sizeof(tryoff));
		for (INT32 s = 0; s < samples; s++) {
			const UINT8 *cp = OutrunLodCRow[sample[s] - k0];
			const INT32 cn = OutrunLodCLen[sample[s] - k0];
			INT32 fc = 0, fp = 0, lc = cn - 1, lp = pn[s] - 1;
			while (fc < cn && !OutrunLodVisible(cp[fc])) fc++;
			while (fp < pn[s] && !OutrunLodVisible(ppix[s][fp])) fp++;
			while (lc >= 0 && !OutrunLodVisible(cp[lc])) lc--;
			while (lp >= 0 && !OutrunLodVisible(ppix[s][lp])) lp--;
			if (fc >= cn || fp >= pn[s]) continue;
			const INT32 est[2] = { fp - 2 * fc, lp - 2 * lc - 1 };
			for (INT32 e2 = 0; e2 < 2; e2++)
				for (INT32 o = est[e2] - 2; o <= est[e2] + 2; o++)
					if (o >= -OUTRUN_LOD_MAX_OFFSET && o <= OUTRUN_LOD_MAX_OFFSET) tryoff[o + OUTRUN_LOD_MAX_OFFSET] = 1;
		}
		for (INT32 offset = -OUTRUN_LOD_MAX_OFFSET; offset <= OUTRUN_LOD_MAX_OFFSET; offset++) {
			if (!tryoff[offset + OUTRUN_LOD_MAX_OFFSET]) continue;
			INT32 same = 0, total = 0;
			for (INT32 s = 0; s < samples; s++) OutrunLodCompareRow(sample[s] - k0, ppix[s], pn[s], offset, &same, &total);
			if (total == 0 || same * 1000 < (OUTRUN_LINK_MATCH - 100) * total) continue;

			same = total = 0;
			for (INT32 k = k0; k <= k1; k++) {
				const INT32 n2 = OutrunLodReadRow(OUTRUN_LOD_FMT_OUTRUN, data, (UINT16)(paddr + 2 * k * ppitch), backward, prow);
				OutrunLodCompareRow(k - k0, prow, (n2 < 0) ? 0 : n2, offset, &same, &total);
			}
			if (total < OUTRUN_LINK_PIXELS) continue;
			const INT32 score = (same * 1000) / total;
			if (score < OUTRUN_LINK_MATCH) continue;
			/* the best: by score, then the smaller offset, then the first found */
			const INT32 ao = (offset < 0) ? -offset : offset, abo = (*bestoffset < 0) ? -*bestoffset : *bestoffset;
			if (score > *bestscore || (score == *bestscore && ao < abo)) {
				*bestscore = score; *bestaddr = paddr; *bestoffset = offset;
				better = true;
			}
		}
	} while (0);
	return better;
}


/* ---- a redrawn copy further before the image ---- */

// A small image may be the game's own smaller drawing of a larger one, not
// scaled from it (Racing Hero: the far motorcycle 4:3ca4, 9 rows, drawn by
// hand from 4:3670, 19 rows, 0x634 words before it): its pixels are not every
// other pixel of the larger one, only near them. When nothing else is found
// for a small image, the OUTRUN_FAR_RANGE words before it (where the game's
// larger images were seen to be, further than the near search reaches) are
// looked through: images about twice as
// wide and tall (by the first and last visible pixel of each row, from a
// table), whose 2x2 pixels over each pixel of the small image are visible
// where it is and hold its colour. The best is taken if it is clearly better
// than any other image there: small images are alike more often.

#define OUTRUN_FAR_MIN_ROWS		3			/* images this tall at least ... */
#define OUTRUN_FAR_MIN_VISIBLE	24			/* ... with this many visible pixels */
#define OUTRUN_FAR_MASK			750			/* per mille: visible in both, of visible in either */
#define OUTRUN_FAR_FOUND		950			/* per mille of the small image's pixels found in their 2x2 */
#define OUTRUN_FAR_MARGIN		100			/* better than any other image by this much (per mille) */
#define OUTRUN_FAR_MAX_WIDTH	48			/* ... and this wide at most */
#define OUTRUN_FAR_RANGE		0x2000		/* words before the image looked through */
#define OUTRUN_FAR_STEP_WORK	200000		/* work a step (see OutrunLodWork) */
#define OUTRUN_FAR_NONE			0xffff

// The first and last visible pixel of the row read from each address (as
// OutrunLodReadRow reads it), per bank and direction; OUTRUN_FAR_NONE if none.
static const void *OutrunLodExtBank[OUTRUN_LOD_LEN_TABLES];
static INT32 OutrunLodExtDir[OUTRUN_LOD_LEN_TABLES];
static UINT16 *OutrunLodExtFirst[OUTRUN_LOD_LEN_TABLES];
static UINT16 *OutrunLodExtLast[OUTRUN_LOD_LEN_TABLES];

static void OutrunLodExtFree()
{
	for (INT32 i = 0; i < OUTRUN_LOD_LEN_TABLES; i++) {
		BurnFree(OutrunLodExtFirst[i]);
		BurnFree(OutrunLodExtLast[i]);
		OutrunLodExtBank[i] = NULL;
	}
}

static bool OutrunLodRowExtents(const void *bankdata, INT32 backward, const UINT16 **first, const UINT16 **last)
{
	INT32 slot = -1;
	for (INT32 i = 0; i < OUTRUN_LOD_LEN_TABLES; i++) {
		if (OutrunLodExtFirst[i] != NULL && OutrunLodExtBank[i] == bankdata && OutrunLodExtDir[i] == backward) {
			*first = OutrunLodExtFirst[i]; *last = OutrunLodExtLast[i];
			return true;
		}
		if (OutrunLodExtFirst[i] == NULL && slot < 0) slot = i;
	}
	if (slot < 0) return false;

	UINT16 *f = (UINT16*)BurnMalloc(0x10000 * sizeof(UINT16));
	UINT16 *l = (UINT16*)BurnMalloc(0x10000 * sizeof(UINT16));
	if (f == NULL || l == NULL) { BurnFree(f); BurnFree(l); return false; }
	OutrunLodWork += 16 * 0x10000;		/* (about the time it takes, as counted elsewhere) */
	/* from the end of each row back (twice round, as the row lengths) */
	UINT16 nf = OUTRUN_FAR_NONE, nl = OUTRUN_FAR_NONE;	/* those of the address read next */
	for (INT32 i = 0; i < 2 * 0x10000; i++) {
		const UINT16 a = (UINT16)(backward ? i : (0xffff - i));
		const UINT32 pixels = BURN_ENDIAN_SWAP_INT32(((const UINT32*)bankdata)[a]);
		const bool marker = backward ? ((pixels & 0x0f000000) == 0x0f000000) : ((pixels & 0x000000f0) == 0x000000f0);
		INT32 wf = -1, wl = -1;
		for (INT32 j = 0; j < 8; j++) {
			const UINT8 p = (pixels >> (backward ? 4 * j : 28 - 4 * j)) & 0xf;
			if (OutrunLodVisible(p)) { if (wf < 0) wf = j; wl = j; }
		}
		UINT16 cf, cl;
		if (marker) {
			cf = (wf >= 0) ? (UINT16)wf : OUTRUN_FAR_NONE;
			cl = (wl >= 0) ? (UINT16)wl : OUTRUN_FAR_NONE;
		} else {
			cf = (wf >= 0) ? (UINT16)wf : (nf != OUTRUN_FAR_NONE && nf < OUTRUN_LOD_ROW_PIXELS) ? (UINT16)(nf + 8) : OUTRUN_FAR_NONE;
			cl = (nl != OUTRUN_FAR_NONE && nl < OUTRUN_LOD_ROW_PIXELS) ? (UINT16)(nl + 8) : (wl >= 0) ? (UINT16)wl : OUTRUN_FAR_NONE;
		}
		f[a] = cf; l[a] = cl;
		nf = cf; nl = cl;
	}
	OutrunLodExtFirst[slot] = f; OutrunLodExtLast[slot] = l;
	OutrunLodExtBank[slot] = bankdata; OutrunLodExtDir[slot] = backward;
	*first = f; *last = l;
	return true;
}

// The visible width of the image whose rows are in OutrunLodCRow.
static INT32 OutrunLodVisibleWidth(INT32 rows)
{
	INT32 x0 = OUTRUN_LOD_ROW_PIXELS, x1 = -1;
	for (INT32 k = 0; k < rows; k++)
		for (INT32 i = 0; i < OutrunLodCLen[k]; i++)
			if (OutrunLodVisible(OutrunLodCRow[k][i])) { if (i < x0) x0 = i; if (i > x1) x1 = i; }
	return (x1 < 0) ? 0 : x1 - x0 + 1;
}

// Whether two candidates are (parts of) the same image: their rows overlap.
static inline bool OutrunLodFarOverlap(const OutrunLodFarCand &a, const OutrunLodFarCand &b, INT32 rows)
{
	const INT32 la = a.pitch * rows, lb = b.pitch * rows;
	const INT32 d = (INT16)(UINT16)(b.addr - a.addr);	/* (within one bank: the nearer way round) */
	return d > -lb && d < la;
}

static void OutrunLodFarAdd(OutrunLodFarCand *top, const OutrunLodFarCand &c, INT32 rows)
{
	/* one per image: the better of two that overlap */
	for (INT32 i = 0; i < OUTRUN_FAR_TOP; i++) {
		if (top[i].score >= 0 && OutrunLodFarOverlap(top[i], c, rows)) {
			if (c.score <= top[i].score) return;
			for (INT32 j = i; j < OUTRUN_FAR_TOP - 1; j++) top[j] = top[j + 1];
			top[OUTRUN_FAR_TOP - 1].score = -1;
			i--;
		}
	}
	for (INT32 i = 0; i < OUTRUN_FAR_TOP; i++) {
		if (top[i].score < c.score) {
			for (INT32 j = OUTRUN_FAR_TOP - 1; j > i; j--) top[j] = top[j - 1];
			top[i] = c;
			return;
		}
	}
}

// One step of the search for a redrawn copy of the image at addr whose rows
// k0..k1 are in OutrunLodCRow (Out Run format, positive pitch): the next
// candidates (each address in the range, at each pitch about twice the
// image's), OUTRUN_FAR_STEP_WORK of work. Returns true when the
// bank has been looked through.
static bool OutrunLodFarStep(const void *data, INT32 backward, UINT16 addr, INT32 pitch, INT32 k0, INT32 k1, OutrunLodCacheEntry &e)
{
	static UINT8 prow[2 * OUTRUN_LINK_MIN_ROWS][OUTRUN_LOD_ROW_PIXELS];
	INT32 pn[2 * OUTRUN_LINK_MIN_ROWS];
	const UINT16 *first, *last;
	const UINT8 *rowlen = OutrunLodRowLengths(OUTRUN_LOD_FMT_OUTRUN, data, backward);
	if (rowlen == NULL || !OutrunLodRowExtents(data, backward, &first, &last)) return true;
	const INT32 rows = k1 - k0 + 1;

	/* the image's visible extent */
	INT32 cx0 = OUTRUN_LOD_ROW_PIXELS, cx1 = -1, cn = 0;
	for (INT32 k = 0; k < rows; k++)
		for (INT32 i = 0; i < OutrunLodCLen[k]; i++)
			if (OutrunLodVisible(OutrunLodCRow[k][i])) { if (i < cx0) cx0 = i; if (i > cx1) cx1 = i; }
	for (INT32 k = 0; k < rows; k++) if (OutrunLodCLen[k] > cn) cn = OutrunLodCLen[k];
	if (cx1 < 0) return true;
	const INT32 cw = cx1 - cx0 + 1;
	/* each row's visible extent (cf > cl: none), and the row with the most
	   visible pixels (km, compared first) */
	INT32 cf[OUTRUN_LINK_MIN_ROWS], cl[OUTRUN_LINK_MIN_ROWS], km = 0, kmvis = 0;
	if (rows > OUTRUN_LINK_MIN_ROWS) return true;
	for (INT32 k = 0; k < rows; k++) {
		INT32 v = 0;
		cf[k] = OUTRUN_LOD_ROW_PIXELS; cl[k] = -1;
		for (INT32 i = 0; i < OutrunLodCLen[k]; i++)
			if (OutrunLodVisible(OutrunLodCRow[k][i])) { if (i < cf[k]) cf[k] = i; cl[k] = i; v++; }
		if (v > kmvis) { kmvis = v; km = k; }
	}

	static const INT32 dp[5] = { 0, -1, 1, -2, 2 };
	const INT32 end = 5 * OUTRUN_FAR_RANGE;
	INT32 n = e.fnext;
	const UINT32 work0 = OutrunLodWork;
	for (; n < end && OutrunLodWork - work0 < OUTRUN_FAR_STEP_WORK; n++) {
		const INT32 ppitch = 2 * pitch + dp[n / OUTRUN_FAR_RANGE];
		if (ppitch <= 0) continue;
		const UINT16 paddr = (UINT16)(addr - OUTRUN_FAR_RANGE + n % OUTRUN_FAR_RANGE);

		/* the extent of rows 2k0 .. 2k1+1: about twice the image's, and
		   something shown where the image shows something (one row off at most) */
		INT32 px0 = OUTRUN_LOD_ROW_PIXELS, px1 = -1, off = 0;
		INT32 pf[OUTRUN_LINK_MIN_ROWS], pl[OUTRUN_LINK_MIN_ROWS];
		bool fits = true;
		for (INT32 k = k0; k <= k1 && fits; k++) {
			pf[k - k0] = OUTRUN_LOD_ROW_PIXELS; pl[k - k0] = -1;
			for (INT32 r = 2 * k; r <= 2 * k + 1 && fits; r++) {
				const UINT16 a = (UINT16)(paddr + r * ppitch);
				OutrunLodWork += 2;
				if (rowlen[a] == 0 || rowlen[a] * 8 > 2 * cn + 32) { fits = false; break; }
				if (first[a] == OUTRUN_FAR_NONE) continue;
				if (first[a] < pf[k - k0]) pf[k - k0] = first[a];
				if (last[a] > pl[k - k0]) pl[k - k0] = last[a];
			}
			if (!fits) break;
			if (pl[k - k0] >= 0) {
				if (pf[k - k0] < px0) px0 = pf[k - k0];
				if (pl[k - k0] > px1) px1 = pl[k - k0];
				if (px1 - px0 + 1 > 2 * cw + 3) fits = false;
			}
			if ((pl[k - k0] >= 0) != (cl[k - k0] >= 0) && ++off > 1) fits = false;
		}
		if (!fits || px1 < 0 || px1 - px0 + 1 < 2 * cw - 3) continue;
		/* each row about where twice the image's is (most rows) */
		INT32 closerows = 0, shown = 0;
		for (INT32 k = 0; k < rows; k++) {
			if (cl[k] < 0 || pl[k] < 0) continue;
			shown++;
			const INT32 df = pf[k] - (px0 + 2 * (cf[k] - cx0)), dl = pl[k] - (px0 + 2 * (cl[k] - cx0) + 1);
			if (df >= -3 && df <= 3 && dl >= -3 && dl <= 3) closerows++;
		}
		if (closerows * 10 < shown * 7) continue;

		/* the 2x2 pixels over each pixel of the image: first those of row km
		   (half of its pixels found at least, either way) */
		{
			pn[2 * km] = OutrunLodReadRow(OUTRUN_LOD_FMT_OUTRUN, data, (UINT16)(paddr + (2 * (k0 + km)) * ppitch), backward, prow[2 * km]);
			pn[2 * km + 1] = OutrunLodReadRow(OUTRUN_LOD_FMT_OUTRUN, data, (UINT16)(paddr + (2 * (k0 + km) + 1) * ppitch), backward, prow[2 * km + 1]);
			INT32 best = 0;
			for (INT32 ph = 0; ph < 2; ph++) {
				const INT32 offset = px0 - 2 * cx0 - ph;
				INT32 found = 0;
				for (INT32 i = cf[km]; i <= cl[km]; i++) {
					const UINT8 c = OutrunLodCRow[km][i];
					if (!OutrunLodVisible(c)) continue;
					const INT32 j = 2 * i + offset;
					for (INT32 r = 2 * km; r <= 2 * km + 1; r++)
						if ((j >= 0 && j < pn[r] && prow[r][j] == c) || (j + 1 >= 0 && j + 1 < pn[r] && prow[r][j + 1] == c)) { found++; break; }
				}
				if (found > best) best = found;
			}
			OutrunLodWork += 4 * cw;
			if (best * 2 < kmvis) continue;
		}
		for (INT32 r = 0; r < 2 * rows; r++)
			pn[r] = OutrunLodReadRow(OUTRUN_LOD_FMT_OUTRUN, data, (UINT16)(paddr + (2 * k0 + r) * ppitch), backward, prow[r]);
		for (INT32 ph = 0; ph < 2; ph++) {
			const INT32 offset = px0 - 2 * cx0 - ph;
			INT32 maskunion = 0, maskboth = 0, visible = 0, found = 0;
			for (INT32 k = k0; k <= k1; k++) {
				const UINT8 *p0 = prow[2 * (k - k0)], *p1 = prow[2 * (k - k0) + 1];
				const INT32 n0 = pn[2 * (k - k0)], n1 = pn[2 * (k - k0) + 1];
				const UINT8 *cpix = OutrunLodCRow[k - k0];
				OutrunLodWork += 4 * cw;
				for (INT32 i = cx0; i <= cx1; i++) {
					const INT32 j = 2 * i + offset;
					const UINT8 c = (i < OutrunLodCLen[k - k0]) ? cpix[i] : 0;
					const UINT8 q[4] = { (UINT8)((j >= 0 && j < n0) ? p0[j] : 0), (UINT8)((j + 1 >= 0 && j + 1 < n0) ? p0[j + 1] : 0),
						(UINT8)((j >= 0 && j < n1) ? p1[j] : 0), (UINT8)((j + 1 >= 0 && j + 1 < n1) ? p1[j + 1] : 0) };
					bool pv = false, hit = false;
					for (INT32 t = 0; t < 4; t++) if (OutrunLodVisible(q[t])) { pv = true; if (q[t] == c) hit = true; }
					const bool cv = OutrunLodVisible(c);
					if (cv || pv) maskunion++;
					if (cv && pv) maskboth++;
					if (cv) { visible++; if (hit) found++; }
				}
			}
			if (maskunion == 0 || visible == 0) continue;
			const INT32 mask = (maskboth * 1000) / maskunion, fnd = (found * 1000) / visible;
			OutrunLodFarCand c;
			c.score = (mask * fnd) / 1000;
			c.addr = paddr; c.pitch = (INT16)ppitch; c.offset = (INT16)offset;
			c.ok = (mask >= OUTRUN_FAR_MASK && fnd >= OUTRUN_FAR_FOUND);
			OutrunLodFarAdd(e.ftop, c, 2 * rows);
		}
	}
	e.fnext = n;
	return n >= end;
}

#define OUTRUN_LOD_TRIM_ROWS	16		/* images up to this tall are looked up again without their last rows */

// One step of a search (OutrunLodFind): begins an attempt, or looks for the
// next level's larger copy from one row shift. Returns true when the search is
// over.
//  - Only images with enough to see are looked up; none found, again without
//    the last row or two (the hardware reads on into the next image at some
//    zooms, and that row is not in the larger copy), if the image is small.
//  - A larger image is found with its rows lined up with the smaller one's,
//    which may put its first row one off from where the image begins: its own
//    larger image is looked for from there too. The strictest kind of search
//    that finds one wins, the first shift among those; the loosest only for
//    the sprite's own image (its larger copies are matched with the stricter
//    ones).
// The cache entry of a key, NULL if none.
static OutrunLodCacheEntry *OutrunLodCacheLookup(UINT32 key)
{
	if (OutrunLodCache == NULL) return NULL;
	const UINT32 h = (key * 2654435761u) >> 20;
	for (INT32 probe = 0; probe < 16; probe++) {
		OutrunLodCacheEntry &c = OutrunLodCache[(h + probe) & (OUTRUN_LOD_CACHE_SIZE - 1)];
		if (c.key == key) return &c;
		if (c.key == 0) return NULL;
	}
	return NULL;
}

// The image just found as the current level may have been looked up itself
// (drawn by the game at another size): its larger copies, as found then, are
// taken (and the search ends there). True if taken.
static bool OutrunLodTakeCached(OutrunLodCacheEntry &e)
{
	OutrunLodArt &w = e.work;
	const OutrunLodLevel &cur = w.level[w.levels];
	const INT32 bank = (cur.bank >= 0) ? cur.bank : (INT32)((e.key >> 27) & 0xf);
	const UINT32 key = 0x80000000 | ((UINT32)(bank & 0xf) << 27) | (e.key & (1u << 26)) | ((UINT32)(cur.pitch & 0x3ff) << 16) | cur.addr;
	const UINT32 h = (key * 2654435761u) >> 20;
	for (INT32 probe = 0; probe < 16; probe++) {
		const OutrunLodCacheEntry &c = OutrunLodCache[(h + probe) & (OUTRUN_LOD_CACHE_SIZE - 1)];
		if (c.key == 0) return false;
		if (c.key != key) continue;
		/* (a finished search, over the rows needed) */
		if (&c == &e || c.searching || c.art.levels == 0 || c.k0 > e.a0 || c.k1 < e.a1) return false;
		for (INT32 l = 1; l <= c.art.levels && w.levels < OUTRUN_LOD_LEVELS; l++) {
			OutrunLodLevel n = c.art.level[l];
			if (n.bank < 0) n.bank = (INT16)bank;
			w.level[++w.levels] = n;
		}
		return true;
	}
	return false;
}

// Begins the search of every bank for the current level's image (rows a0..a1),
// once per level, if it is big enough (Out Run format). False if not begun.
static bool OutrunLodStartBanks(INT32 fmt, const void *bankdata, INT32 backward, OutrunLodCacheEntry &e)
{
	const OutrunLodArt &w = e.work;
	const OutrunLodLevel &cur = w.level[w.levels];
	const INT32 rows = e.a1 - e.a0 + 1;
	INT32 visible = 0;
	if (fmt != OUTRUN_LOD_FMT_OUTRUN || OutrunLodSpriteBase == NULL || OutrunLodNumBanks <= 0 || (e.xdone & (1 << w.levels)) ||
		rows < OUTRUN_LINK_MIN_ROWS || rows > OUTRUN_LOD_MAX_ROWS ||
		!OutrunLodReadImage(fmt, OutrunLodLevelData(cur, bankdata), backward, cur.addr, cur.pitch, e.a0, e.a1, &visible) || visible < OUTRUN_LINK_MIN_VISIBLE) return false;
	e.xdone |= 1 << w.levels;
	e.xbank = 0;
	e.xscore = -1;
	e.xbest.offset = 0;
	return true;
}

static bool OutrunLodSearchStep(INT32 fmt, const void *bankdata, INT32 backward, OutrunLodCacheEntry &e)
{
	OutrunLodArt &w = e.work;
	INT32 visible = 0;

	/* none near the current level's image: every bank, one a step */
	if (e.xbank >= 0) {
		/* (the bank's index is built over several steps first) */
		OutrunLodIndex *x = OutrunLodGetIndex(OutrunLodSpriteBase + 0x10000 * e.xbank, backward);
		if (x && x->built < 0x10000) return false;
		const OutrunLodLevel &cur = w.level[w.levels];
		if (!OutrunLodReadImage(fmt, OutrunLodLevelData(cur, bankdata), backward, cur.addr, cur.pitch, e.a0, e.a1, &visible)) return true;
		OutrunLodSearchBank(e.xbank, backward, cur.pitch, e.a0, e.a1, &e.xscore, &e.xbest);
		e.xbank++;
		if (e.xbank < OutrunLodNumBanks) return false;
		e.xbank = -1;
		if (e.xscore < 0) return true;
		if (w.levels == 0) w.validrow = e.k1;
		w.level[++w.levels] = e.xbest;
		e.xfound |= 1 << w.levels;
		e.a0 = 2 * e.a0; e.a1 = 2 * e.a1 + 1;
		e.started = 1;
		e.shift = 0;
		return OutrunLodTakeCached(e) || w.levels >= OUTRUN_LOD_LEVELS;
	}

	/* nothing for a small image: a redrawn copy further before it, some
	   addresses a step */
	if (e.fnext >= 0) {
		if (!OutrunLodReadImage(fmt, bankdata, backward, w.level[0].addr, w.level[0].pitch, e.k0, e.k1, &visible)) return true;
		if (!OutrunLodFarStep(bankdata, backward, w.level[0].addr, w.level[0].pitch, e.k0, e.k1, e)) return false;
		e.fnext = -1;
		/* the best, if alike enough and clearly better than any other image */
		const OutrunLodFarCand &b = e.ftop[0];
		if (b.score < 0 || !b.ok || (e.ftop[1].score >= 0 && b.score - e.ftop[1].score < OUTRUN_FAR_MARGIN)) return true;
		OutrunLodLevel l;
		l.addr = b.addr; l.bank = -1; l.pitch = b.pitch; l.offset = b.offset; l.voff = 0;
		w.validrow = e.k1;
		w.level[++w.levels] = l;
		e.xfound |= 1 << w.levels;
		e.a0 = 2 * e.k0; e.a1 = 2 * e.k1 + 1;
		e.started = 1;
		e.shift = 0;
		return OutrunLodTakeCached(e) || w.levels >= OUTRUN_LOD_LEVELS;
	}

	if (!e.started) {
		const INT32 t1 = e.k1 - e.trim;
		/* (a taller image is matched by the share of its pixels alike: a row
		   more or less hardly changes that) */
		if (e.trim > ((e.k1 - e.k0 + 1 <= OUTRUN_LOD_TRIM_ROWS) ? 2 : 0) || t1 <= e.k0) {
			/* none near: every bank (once; the rows not trimmed, a big enough image) */
			e.a0 = e.k0; e.a1 = e.k1;
			if (OutrunLodStartBanks(fmt, bankdata, backward, e)) return false;
			/* a small one: a redrawn copy in its bank (once) */
			if (fmt == OUTRUN_LOD_FMT_OUTRUN && !e.fdone && w.level[0].pitch > 0 &&
				e.k1 - e.k0 + 1 >= OUTRUN_FAR_MIN_ROWS && e.k1 - e.k0 + 1 < OUTRUN_LINK_MIN_ROWS &&
				OutrunLodReadImage(fmt, bankdata, backward, w.level[0].addr, w.level[0].pitch, e.k0, e.k1, &visible) && visible >= OUTRUN_FAR_MIN_VISIBLE &&
				OutrunLodVisibleWidth(e.k1 - e.k0 + 1) <= OUTRUN_FAR_MAX_WIDTH) {
				e.fdone = 1;
				e.fnext = 0;
				for (INT32 t = 0; t < OUTRUN_FAR_TOP; t++) e.ftop[t].score = -1;
				return false;
			}
			return true;
		}
		if (!OutrunLodReadImage(fmt, bankdata, backward, w.level[0].addr, w.level[0].pitch, e.k0, t1, &visible) || visible < OUTRUN_LOD_MIN_PIXELS) {
			e.trim++;
			return false;
		}
		w.validrow = t1;
		e.a0 = e.k0; e.a1 = t1;
		e.started = 1;
		/* (the first level is looked for in the next step) */
		return false;
	}

	/* one row shift per step */
	OutrunLodLevel &cur = w.level[w.levels];
	const void *data = OutrunLodLevelData(cur, bankdata);	/* (the level's bank) */
	static const INT32 shifts[3] = { 0, 1, -1 };
	const INT32 nshifts = (w.levels == 0) ? 1 : 3;
	const INT32 lastpass = (w.levels == 0) ? OUTRUN_LOD_PASS_ADJACENT : OUTRUN_LOD_PASS_REDRAWN;
	if (e.shift == 0) e.bestpass = lastpass + 1;
	const INT32 si = e.shift;
	OutrunLodLevel alt = cur;
	alt.addr = (UINT16)(cur.addr - shifts[si] * cur.pitch);
	alt.voff = cur.voff + shifts[si];
	if (OutrunLodReadImage(fmt, data, backward, alt.addr, alt.pitch, e.a0 + shifts[si], e.a1 + shifts[si], &visible)) {
		if (e.ps.dn == 0) e.ps.maxpass = e.bestpass - 1;
		if (!OutrunLodFindParents(fmt, data, backward, alt.addr, alt.pitch, e.a0 + shifts[si], e.a1 + shifts[si], e.ps)) return false;	/* goes on later */
		e.ps.dn = 0;
		for (INT32 pass = OUTRUN_LOD_PASS_EXACT; pass < e.bestpass; pass++) {
			if (!e.ps.found[pass]) continue;
			e.bestpass = pass;
			e.bestnext = e.ps.level[pass];
			e.bestnext.bank = cur.bank;		/* (in the same bank) */
			e.bestfrom = alt;
			e.bestshift = shifts[si];
			break;
		}
	}
	e.shift++;
	if (e.shift < nshifts && e.bestpass > OUTRUN_LOD_PASS_EXACT) return false;	/* the next shift in the next step */
	e.shift = 0;

	if (e.bestpass > lastpass) {
		/* none near a level found far from the one below: every bank (its
		   own larger copy may be far from it too) */
		if (w.levels > 0) return !((e.xfound >> w.levels) & 1) || !OutrunLodStartBanks(fmt, bankdata, backward, e);
		e.trim++;				/* none for the sprite's own image: the next attempt */
		e.started = 0;
		return false;
	}
	cur = e.bestfrom;
	e.a0 += e.bestshift; e.a1 += e.bestshift;
	w.level[++w.levels] = e.bestnext;
	e.a0 = 2 * e.a0; e.a1 = 2 * e.a1 + 1;
	return OutrunLodTakeCached(e) || w.levels >= OUTRUN_LOD_LEVELS;
}

// The parts of a cache key.
static inline INT32 OutrunLodKeyBank(UINT32 key) { return (key >> 27) & 0xf; }
static inline INT32 OutrunLodKeyBackward(UINT32 key) { return (key >> 26) & 1; }
static inline INT32 OutrunLodKeyPitch(UINT32 key) { const INT32 p = (key >> 16) & 0x3ff; return (p & 0x200) ? p - 0x400 : p; }
static inline UINT16 OutrunLodKeyAddr(UINT32 key) { return (UINT16)key; }

// The larger copies of a sprite's image, rows k0..k1 read. Each image is
// looked up once (and again when more of it is drawn than before: a car first
// drawn cut off by a hill shows only its top rows, unless all levels were
// found, or it was looked at often enough) and kept. The searches a frame
// makes are limited by their work (counted the same on every machine, see
// OutrunLodWork): past that, a sprite is drawn with what has been found so
// far, and its search goes on in a later frame.
static void OutrunLodFindOne(INT32 fmt, const void *spritebase, INT32 bank, const void *bankdata, UINT16 addr, INT32 pitch,
	INT32 backward, INT32 k0, INT32 k1, OutrunLodArt *art)
{
	art->levels = 0;
	art->validrow = k1;
	art->level[0].addr = addr;
	art->level[0].bank = -1;
	art->level[0].pitch = pitch;
	art->level[0].offset = 0;
	art->level[0].voff = 0;

	if (OutrunLodCache == NULL) {
		OutrunLodCache = (OutrunLodCacheEntry*)BurnMalloc(OUTRUN_LOD_CACHE_SIZE * sizeof(OutrunLodCacheEntry));
		if (OutrunLodCache == NULL) return;
		memset(OutrunLodCache, 0, OUTRUN_LOD_CACHE_SIZE * sizeof(OutrunLodCacheEntry));
	}
	if (OutrunLodCacheRom != spritebase) {
		memset(OutrunLodCache, 0, OUTRUN_LOD_CACHE_SIZE * sizeof(OutrunLodCacheEntry));
		OutrunLodCacheRom = spritebase;
		OutrunLodLenFree();
		OutrunLodIndexFree();
		OutrunLodExtFree();
	}

	const UINT32 key = 0x80000000 | ((UINT32)(bank & 0xf) << 27) | ((UINT32)(backward & 1) << 26) | ((UINT32)(pitch & 0x3ff) << 16) | addr;
	const UINT32 h = (key * 2654435761u) >> 20;
	OutrunLodCacheEntry *e = NULL;
	for (INT32 probe = 0; probe < 16; probe++) {
		OutrunLodCacheEntry &c = OutrunLodCache[(h + probe) & (OUTRUN_LOD_CACHE_SIZE - 1)];
		if (c.key == key) { e = &c; break; }
		if (c.key == 0 || probe == 15) {
			/* a new one (the last place taken over when all are in use) */
			e = &c;
			e->key = key;
			e->tries = 0;
			e->searching = 0;
			e->art = *art;
			e->k0 = k0; e->k1 = k1;
			e->k1 = -1;			/* (looked up below) */
			break;
		}
	}

	if (!e->searching) {
		if (e->art.levels >= OUTRUN_LOD_LEVELS || e->tries >= OUTRUN_LOD_TRIES || (e->k1 >= 0 && k0 >= e->k0 && k1 <= e->k1)) { *art = e->art; return; }
		/* a new search, over all the rows seen */
		if (e->k1 >= 0) {
			if (e->k0 < k0) k0 = e->k0;
			if (e->k1 > k1) k1 = e->k1;
		}
		e->k0 = k0; e->k1 = k1;
		e->tries++;
		e->searching = 1;
		e->started = 0;
		e->trim = 0;
		e->shift = 0;
		e->ps.dn = 0;
		e->xbank = -1;
		e->xdone = 0;
		e->xfound = 0;
		e->fnext = -1;
		e->fdone = 0;
		e->mirrored = 0;
		e->work = *art;
		e->work.validrow = k1;
	}

	while (e->searching && OutrunLodFrameWork < OUTRUN_LOD_FRAME_WORK) {
		/* (the long search for a redrawn copy takes what the others leave) */
		if (e->fnext >= 0 && OutrunLodFrameWork >= OUTRUN_LOD_FRAME_WORK / 3) break;
		const UINT32 work0 = OutrunLodWork;
		OutrunLodCounting = 1;
		const bool done = OutrunLodSearchStep(fmt, bankdata, backward, *e);
		OutrunLodCounting = 0;
		OutrunLodFrameWork += OutrunLodWork - work0;
		if (done) {
			e->searching = 0;
			if (e->work.levels >= e->art.levels) e->art = e->work;	/* else keep the better result */
		}
	}

	*art = (e->searching && e->work.levels > e->art.levels) ? e->work : e->art;
}

// A flipped sprite (Out Run format: its rows read backward, from their last
// word) whose rows are the same words as an image read forward (its twin) is
// that image mirrored, and so are their larger copies. A small image's copy
// is often found only one way: its every other pixel is the larger one's from
// one end, not from the other (Out Run: the far roadside tree 0:c13a, its
// 2:1 copy 0:c123 found read forward only). So when none is found for a
// small flipped image, its twin's are looked for and taken, mirrored: each read
// backward from the word as far after its rows' starts as the longest row
// reaches, if that reads every row's pixels, mirrored, and nothing else.

// Rows 0..rows-1 of the image read forward from f and backward from b (both
// pitch): the same pixels, mirrored (forward pixel j = backward pixel
// 8 * (b - f) + 7 - j)? The backward rows may read more before f (a larger
// copy found may begin a word into its rows: read backward, the rows are
// whole).
static bool OutrunLodMirrored(const void *data, UINT16 f, UINT16 b, INT32 pitch, INT32 rows)
{
	static UINT8 fp[OUTRUN_LOD_ROW_PIXELS], bp[OUTRUN_LOD_ROW_PIXELS];
	const INT32 m = 8 * (INT32)(UINT16)(b - f) + 7;
	for (INT32 r = 0; r < rows; r++) {
		const INT32 nf = OutrunLodReadRow(OUTRUN_LOD_FMT_OUTRUN, data, (UINT16)(f + r * pitch), 0, fp);
		const INT32 nb = OutrunLodReadRow(OUTRUN_LOD_FMT_OUTRUN, data, (UINT16)(b + r * pitch), 1, bp);
		if (nf < 0 || nb < 0) return false;
		for (INT32 i = 0; i < nb; i++) {
			const INT32 j = m - i;
			if (j < 0) continue;		/* (before f: the row's own start, which the forward read leaves out) */
			const UINT8 q = (j < nf) ? fp[j] : 0;
			if (OutrunLodVisible(bp[i]) != OutrunLodVisible(q) || (OutrunLodVisible(q) && q != bp[i])) return false;
		}
		/* (and nothing of the forward row past what is read backward) */
		for (INT32 j = 0; j < nf; j++) {
			const INT32 i = m - j;
			if ((i < 0 || i >= nb) && OutrunLodVisible(fp[j])) return false;
		}
	}
	return true;
}

// The levels of a flipped image from its twin's (twin; art->level[0] set, and
// m0: the backward pixel over forward pixel 0 of level 0). Stops at a level
// that is not its forward one mirrored.
static void OutrunLodMirror(const OutrunLodArt &twin, const void *bankdata, INT32 rows, INT32 m0, OutrunLodArt *art)
{
	INT32 mprev = m0;
	for (INT32 l = 1; l <= twin.levels; l++) {
		const OutrunLodLevel &f = twin.level[l];
		const void *data = OutrunLodLevelData(f, bankdata);
		const UINT8 *len = OutrunLodRowLengths(OUTRUN_LOD_FMT_OUTRUN, data, 0);
		const INT32 lrows = rows << l;
		if (len == NULL || f.pitch <= 0 || lrows > 2 * OUTRUN_LOD_MAX_ROWS) return;
		/* read backward from the last word of the longest row */
		INT32 d = 0;
		for (INT32 r = 0; r < lrows; r++) {
			const INT32 w = len[(UINT16)(f.addr + r * f.pitch)];
			if (w == 0) return;
			if (w - 1 > d) d = w - 1;
		}
		const UINT16 b = (UINT16)(f.addr + d);
		if (!OutrunLodMirrored(data, f.addr, b, f.pitch, lrows)) return;
		/* the 2x2 pixels over pixel j of the level below begin at 2j + offset:
		   mirrored, at 2j' + (m - 2 * mprev - offset - 1) */
		const INT32 m = 8 * d + 7;
		OutrunLodLevel n = f;
		n.addr = b;
		n.offset = (INT16)(m - 2 * mprev - f.offset - 1);
		art->level[++art->levels] = n;
		mprev = m;
	}
}

#define OUTRUN_MIRROR_MAX_VISIBLE	48		/* flipped images with fewer visible pixels */

static void OutrunLodFind(INT32 fmt, const void *spritebase, INT32 bank, const void *bankdata, UINT16 addr, INT32 pitch,
	INT32 backward, INT32 k0, INT32 k1, OutrunLodArt *art)
{
	OutrunLodFindOne(fmt, spritebase, bank, bankdata, addr, pitch, backward, k0, k1, art);
	if (fmt != OUTRUN_LOD_FMT_OUTRUN || !backward || pitch <= 0 || art->levels > 0 || k0 != 0 || k1 + 1 > OUTRUN_LOD_MAX_ROWS) return;

	/* (once per search of the image: kept in its entry) */
	const UINT32 key = 0x80000000 | ((UINT32)(bank & 0xf) << 27) | (1u << 26) | ((UINT32)(pitch & 0x3ff) << 16) | addr;
	OutrunLodCacheEntry *e = OutrunLodCacheLookup(key);
	if (e == NULL || e->searching || e->mirrored) return;

	/* its twin: read forward from the first word of its longest row */
	const INT32 rows = k1 + 1;
	const UINT8 *len = OutrunLodRowLengths(fmt, bankdata, 1);
	if (len == NULL) return;
	INT32 d = 0;
	for (INT32 r = 0; r < rows; r++) {
		const INT32 w = len[(UINT16)(addr + r * pitch)];
		if (w == 0) return;
		if (w - 1 > d) d = w - 1;
	}
	const UINT16 taddr = (UINT16)(addr - d);
	/* (a small image only: a larger one's copies are matched more loosely,
	   either way; and its twin) */
	INT32 visible = 0;
	if (!OutrunLodReadImage(fmt, bankdata, 1, addr, pitch, 0, k1, &visible) || visible >= OUTRUN_MIRROR_MAX_VISIBLE ||
		!OutrunLodMirrored(bankdata, taddr, addr, pitch, rows)) { e->mirrored = 1; return; }
	OutrunLodArt twin;
	OutrunLodFindOne(fmt, spritebase, bank, bankdata, taddr, pitch, 0, k0, k1, &twin);
	const OutrunLodCacheEntry *t = OutrunLodCacheLookup(0x80000000 | ((UINT32)(bank & 0xf) << 27) | ((UINT32)(pitch & 0x3ff) << 16) | taddr);
	if (t == NULL || t->searching) return;		/* (its search goes on) */
	e = OutrunLodCacheLookup(key);			/* (the twin's entry may have taken its place) */
	if (e == NULL) return;
	e->mirrored = 1;
	if (twin.levels <= art->levels) return;

	OutrunLodArt m;
	m.levels = 0;
	m.validrow = twin.validrow;
	m.level[0] = art->level[0];
	OutrunLodMirror(twin, bankdata, rows, 8 * d + 7, &m);
	if (m.levels > art->levels) {
		*art = m;
		e->art = m;
	}
}

// Row i of level 'lev' for row position y (in 1/65536 of the sprite's own rows).
static inline INT32 OutrunLodRowIndex(const OutrunLodArt &art, INT32 lev, INT64 y)
{
	INT64 i = y;
	for (INT32 l = 1; l <= lev; l++) i = 2 * i + ((INT64)art.level[l].voff << 16);
	return (INT32)(i >> 16);
}

// Pixel j of level 'lev' for pixel position t (in 1/65536 of the sprite's own pixels).
static inline INT32 OutrunLodPixelIndex(const OutrunLodArt &art, INT32 lev, INT64 t)
{
	/* level l pixel = 2 * (level l-1 pixel) + offset(l) */
	INT64 j = t;
	for (INT32 l = 1; l <= lev; l++) j = 2 * j + ((INT64)art.level[l].offset << 16);
	return (INT32)(j >> 16);
}

/* ---- Out Run sprites ---- */

// The level of larger art to draw a sprite from: the smallest at least as
// fine as the screen (zoom 0x200 = 1:1 at native resolution).
static INT32 OutrunLodChooseLevel(const OutrunLodArt &art, INT32 S, INT32 zoom)
{
	INT32 lev = 0;
	while (lev < art.levels && (zoom << lev) < 0x200 * S) lev++;
	return lev;
}

// One sprite of a layer, ready to draw.
struct OutrunSpriteDraw {
	const UINT32 *spritedata;
	const UINT8 *rowlen;		/* row lengths (OutrunLodRowLengths), or NULL */
	UINT16 addr;
	INT32 pitch, top, height, ydelta, xdelta, xpos, flip, hzoom, vzoom, shadow, color, lev;
	INT64 yspan;				/* rows x 512, << 16 */
	OutrunLodArt art;
};

static OutrunSpriteDraw OutrunSprites[0x1000 / 16];
static INT32 OutrunSpriteCount = 0;
static UINT8 OutrunRowPix[OUTRUN_PARTS][OUTRUN_LOD_ROW_PIXELS];

// Reads the layer's sprites (Priority) and finds their larger art. Also
// X-Board's sprites (xboard): the same chip, with the height and colour in
// other bits and no palette offset.
static void OutrunPrepareSprites(INT32 Priority, bool xboard)
{
	const INT32 S = System16ResScale;
	const INT32 H = 224 * S;
	UINT8 numbanks = System16SpriteRomSize / 0x40000;
	const UINT32 *spritebase = (const UINT32 *)System16Sprites;
	UINT16 *data;

	OutrunSpriteCount = 0;
	OutrunLodSpriteBase = spritebase;
	OutrunLodNumBanks = (numbanks > 8) ? 8 : numbanks;
	for (data = (UINT16*)System16SpriteRamBuff; data < (UINT16*)System16SpriteRamBuff + System16SpriteRamSize / 2; data += 8) {
		if (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x8000) break;
		INT32 sprpri  = 1 << ((BURN_ENDIAN_SWAP_INT16(data[3]) >> 12) & 3);

		if (sprpri != Priority) continue;

		INT32 hide    = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x5000);
		INT32 bank    = (BURN_ENDIAN_SWAP_INT16(data[0]) >> 9) & 7;
		INT32 top     = (BURN_ENDIAN_SWAP_INT16(data[0]) & 0x1ff) - 0x100;
		UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[1]);
		INT32 pitch   = (INT16)((BURN_ENDIAN_SWAP_INT16(data[2]) >> 1) | ((BURN_ENDIAN_SWAP_INT16(data[4]) & 0x1000) << 3)) >> 8;
		INT32 xpos    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x1ff;
		INT32 shadow  = (BURN_ENDIAN_SWAP_INT16(data[3]) >> 14) & 1;
		INT32 vzoom   = BURN_ENDIAN_SWAP_INT16(data[3]) & 0x7ff;
		INT32 ydelta  = (BURN_ENDIAN_SWAP_INT16(data[4]) & 0x8000) ? 1 : -1;
		INT32 flip    = (~(BURN_ENDIAN_SWAP_INT16(data[4])) >> 14) & 1;
		INT32 xdelta  = (BURN_ENDIAN_SWAP_INT16(data[4]) & 0x2000) ? 1 : -1;
		INT32 hzoom   = BURN_ENDIAN_SWAP_INT16(data[4]) & 0x7ff;
		INT32 height, color;
		if (xboard) {
			height    = (BURN_ENDIAN_SWAP_INT16(data[5]) & 0xfff) + 1;
			color     = (BURN_ENDIAN_SWAP_INT16(data[6]) & 0xff) << 4;
		} else {
			height    = (BURN_ENDIAN_SWAP_INT16(data[5]) >> 8) + 1;
			color     = ((BURN_ENDIAN_SWAP_INT16(data[5]) & 0x7f) << 4) | 0x800;
		}

		/* adjust X coordinate (as the native renderer) */
		if (xpos < 0x80 && xdelta < 0)
			xpos += 0x200;
		xpos -= 0xbe;

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (hide || height == 0) {
			data[7] = BURN_ENDIAN_SWAP_INT16(addr); // un-cache d7
			continue;
		}

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		const UINT32 *spritedata = spritebase + 0x10000 * bank;

		/* clamp to a maximum of 8x (not 100% confirmed) */
		if (vzoom < 0x40) vzoom = 0x40;
		if (hzoom < 0x40) hzoom = 0x40;

		/* the rows the hardware reads: native line i shows row (i * vzoom) >> 9 */
		const INT32 lastrow = ((height - 1) * vzoom) >> 9;
		INT64 yspan = (INT64)height * vzoom;								/* rows x 512 */
		if (yspan > (INT64)(lastrow + 1) * 512) yspan = (INT64)(lastrow + 1) * 512;
		yspan <<= 16;

		OutrunSpriteDraw &d = OutrunSprites[OutrunSpriteCount];
		OutrunLodFind(OUTRUN_LOD_FMT_OUTRUN, spritebase, bank, spritedata, addr, pitch, flip, 0, lastrow, &d.art);

		/* nothing on screen: not drawn (its lines all above or below) */
		const INT32 ylast = top + ydelta * (height - 1);
		if (((top < ylast) ? ylast : top) < 0 || ((top < ylast) ? top : ylast) * S >= H) continue;

		const INT32 hlev = OutrunLodChooseLevel(d.art, S, hzoom);
		const INT32 vlev = OutrunLodChooseLevel(d.art, S, vzoom);
		d.lev = (hlev > vlev) ? hlev : vlev;
		d.spritedata = spritedata;
		d.rowlen = OutrunLodRowLengths(OUTRUN_LOD_FMT_OUTRUN, spritedata, flip);
		d.addr = addr; d.pitch = pitch; d.top = top; d.height = height; d.ydelta = ydelta; d.xdelta = xdelta;
		d.xpos = xpos; d.flip = flip; d.hzoom = hzoom; d.vzoom = vzoom; d.shadow = shadow; d.color = color;
		d.yspan = yspan;
		OutrunSpriteCount++;
	}
}

// Draws the prepared sprites' finer lines in the bands of the given part.
static void OutrunDrawSpritesPart(INT32 part)
{
	const INT32 S = System16ResScale;
	const INT32 W = 320 * S;
	const INT32 H = 224 * S;
	const INT32 mask = System16PaletteEntries - 1;
	UINT8 *rowpix = OutrunRowPix[part];
	UINT16 *PalRAM = (UINT16*)System16PaletteRam;

	for (INT32 si = 0; si < OutrunSpriteCount; si++) {
		const OutrunSpriteDraw &d = OutrunSprites[si];
		const OutrunLodArt &art = d.art;
		const INT32 height = d.height, hzoom = d.hzoom, xdelta = d.xdelta, flip = d.flip;
		const UINT32 *spritedata = d.spritedata;

		/* finer lines: line I (of S * height) shows the sprite's row I * vzoom / (512 S),
		   pixel N of a line its pixel N * hzoom / (512 S), taken from the larger
		   art 2^lev times as fine */
		INT32 lastrowread = -0x7fffffff, lastlev = -1, lastn = 0;		/* the level row in rowpix */
		INT32 lastchild = -0x7fffffff, childn = 0;		/* the sprite's own row's length */
		const INT32 X0 = d.xpos * S + ((xdelta > 0) ? 0 : S - 1);
		for (INT32 I = 0; I < S * height; I++) {
			const INT32 i = I / S;
			const INT32 y = d.top + d.ydelta * i;
			const INT32 Y = y * S + ((d.ydelta > 0) ? (I % S) : (S - 1 - (I % S)));
			if (Y < 0 || Y >= H || OUTRUN_PART(Y) != part) continue;

			/* the sprite's own row: where it ends */
			/* row position (1/65536 rows): native line i shows row i * vzoom / 512;
			   the finer lines spread over the rows the native lines show, up to
			   the last one (never past it: rows beyond are another image) */
			const INT64 yc = (INT64)I * d.yspan / (512 * S * height);
			const INT32 crow = (INT32)(yc >> 16);
			if (crow != lastchild) {
				const UINT16 ra = (UINT16)(d.addr + crow * d.pitch);
				if (d.rowlen) {
					const INT32 w = d.rowlen[ra];
					childn = w ? w * 8 : -1;
				} else {
					childn = OutrunLodReadRow(OUTRUN_LOD_FMT_OUTRUN, spritedata, ra, flip, rowpix);
					lastrowread = -0x7fffffff;		/* (rowpix used) */
				}
				lastchild = crow;
			}
			INT32 NE;
			if (childn < 0) {
				NE = 320 * S + S;						/* no end marker: up to the edge */
			} else {
				NE = (INT32)(((INT64)childn * 512 * S + hzoom - 1) / hzoom);
			}

			/* the level's row (rows past those the larger art was matched on: the sprite's own) */
			const INT32 rl = (crow > art.validrow) ? 0 : d.lev;
			const OutrunLodLevel &L = art.level[rl];
			const INT32 lrow = OutrunLodRowIndex(art, rl, yc);
			if (lrow != lastrowread || rl != lastlev) {
				lastn = OutrunLodReadRow(OUTRUN_LOD_FMT_OUTRUN, (const UINT32*)OutrunLodLevelData(L, spritedata), (UINT16)(L.addr + lrow * L.pitch), flip, rowpix);
				if (lastn < 0) lastn = OUTRUN_LOD_ROW_PIXELS;	/* drawn up to NE */
				lastrowread = lrow;
				lastlev = rl;
			}

			UINT16 *pPixel = pTransDraw + Y * W;
			/* pixel N of the line: the sprite's pixel t = N * hzoom / (512 S),
			   the level's pixel (t << lev) + offsets (all in 1/65536) */
			const INT64 tstep = ((INT64)hzoom << (16 + rl)) / (512 * S);
			/* the part of the line on screen */
			INT32 N0 = 0, N1 = NE;
			if (xdelta > 0) {
				if (X0 < 0) N0 = -X0;
				if (N1 > W - X0) N1 = W - X0;
			} else {
				if (X0 >= W) N0 = X0 - W + 1;
				if (N1 > X0 + 1) N1 = X0 + 1;
			}
			INT64 tl = ((INT64)OutrunLodPixelIndex(art, rl, 0) << 16) + tstep * N0;
			UINT16 *p = pPixel + X0 + xdelta * N0;
			for (INT32 N = N0; N < N1; N++, tl += tstep, p += xdelta) {
				const INT32 j = (INT32)(tl >> 16);
				INT32 pix;
				if (j < 0) continue;
				if (j >= lastn) {
					if (rl == 0 && childn < 0) {
						/* beyond the words read: read the ROM directly, like the hardware */
						const INT32 w = j >> 3;
						const UINT32 pixels = BURN_ENDIAN_SWAP_INT32(spritedata[(UINT16)(flip ? (d.addr + crow * d.pitch - w) : (d.addr + crow * d.pitch + w))]);
						pix = flip ? (pixels >> (4 * (j & 7))) & 0xf : (pixels >> (28 - 4 * (j & 7))) & 0xf;
					} else {
						break;
					}
				} else {
					pix = rowpix[j];
				}
				if (pix == 0 || pix == 15) continue;
				if (d.shadow && pix == 0xa) {
					*p &= mask;
					*p += (BURN_ENDIAN_SWAP_INT16(PalRAM[*p]) & 0x8000) ? (System16PaletteEntries * 2) : System16PaletteEntries;
				} else {
					*p = (pix | d.color) & mask;
				}
			}
		}
	}
}

static void OutrunRenderSpriteLayerScaled(INT32 Priority, bool xboard)
{
	OutrunPrepareSprites(Priority, xboard);
	if (OutrunSpriteCount) OutrunRunParts(OUTRUN_JOB_SPRITES);
}

/* ---- System 16B sprites (Super Hang-On) ---- */

inline static void Shangon16BDrawPixelScaled(INT32 x, INT32 W, INT32 pix, INT32 colour, UINT16* pPixel, UINT16* PalRAM)
{
	if (x >= 0 && x < W && pix != 0 && pix != 15) {
		if (colour == (0x3f << 4)) {
			pPixel[x] &= (System16PaletteEntries - 1);
			pPixel[x] += (BURN_ENDIAN_SWAP_INT16(PalRAM[pPixel[x]]) & 0x8000) ? (System16PaletteEntries * 2) : System16PaletteEntries;
		} else {
			pPixel[x] = (pix | colour | System16SpritePalOffset) & (System16PaletteEntries - 1);
		}
	}
}

// As System16BRenderSpriteLayer(0) at S times the resolution. The native zoom
// drops source pixels and skips rows; after q source pixels a row has drawn
// ceil((q * (64 - hzoom) - c) / 64) pixels (c = (4 * hzoom) & 0x3f, the initial
// accumulator), so pixel n shows source pixel (64 n + c) / (64 - hzoom), and
// line r shows row (r + 1) * (32 + vzoom) / 32 past addr. Both are evaluated at
// S times finer steps (see also YBoardSystem16BRenderSpriteLayerScaled).
static void Shangon16BRenderSpriteLayerScaled()
{
	const INT32 S = System16ResScale;
	const INT32 W = 320 * S;
	const INT32 H = 224 * S;
	UINT8 numbanks;
	const UINT16 *spritebase;
	UINT16 *data;
	static UINT8 rowpix[OUTRUN_LOD_ROW_PIXELS];

	UINT16 *PalRAM = (UINT16*)System16PaletteRam;

	spritebase = (const UINT16 *)System16Sprites;
	numbanks = System16SpriteRomSize / 0x20000;

	for (data = (UINT16*)System16SpriteRam; data < (UINT16*)System16SpriteRam + System16SpriteRamSize / 2; data += 8) {
		if (BURN_ENDIAN_SWAP_INT16(data[2]) & 0x8000) break;

		INT32 bottom  = BURN_ENDIAN_SWAP_INT16(data[0]) >> 8;
		INT32 top     = BURN_ENDIAN_SWAP_INT16(data[0]) & 0xff;
		INT32 xpos    = (BURN_ENDIAN_SWAP_INT16(data[1]) & 0x1ff) - 0xb8;
		INT32 hide    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x4000;
		INT32 flip    = BURN_ENDIAN_SWAP_INT16(data[2]) & 0x100;
		INT32 pitch   = (INT8)(BURN_ENDIAN_SWAP_INT16(data[2]) & 0xff);
		UINT16 addr = BURN_ENDIAN_SWAP_INT16(data[3]);
		INT32 bank    = System16SpriteBanks[(BURN_ENDIAN_SWAP_INT16(data[4]) >> 8) & 0xf];
		INT32 color   = ((BURN_ENDIAN_SWAP_INT16(data[4]) & 0x3f) << 4);
		INT32 vzoom   = (BURN_ENDIAN_SWAP_INT16(data[5]) >> 5) & 0x1f;
		INT32 hzoom   = BURN_ENDIAN_SWAP_INT16(data[5]) & 0x1f;
		const UINT16 *spritedata;
		INT32 xdelta = 1;

		/* if hidden, or top greater than/equal to bottom, or invalid bank, punt */
		if (hide || (top >= bottom) || bank == 255) {
			data[7] = BURN_ENDIAN_SWAP_INT16(addr); // un-cache d7
			continue;
		}

		/* clamp to within the memory region size */
		if (numbanks)
			bank %= numbanks;
		spritedata = spritebase + 0x10000 * bank;

		/* the Y zoom counter in sprite RAM, updated exactly as the native renderer does */
		data[5] &= BURN_ENDIAN_SWAP_INT16(0x03ff);
		for (INT32 y = top; y < bottom; y++) {
			data[5] += BURN_ENDIAN_SWAP_INT16(vzoom << 10);
			if (data[5] & BURN_ENDIAN_SWAP_INT16(0x8000)) {
				data[5] &= BURN_ENDIAN_SWAP_INT16(~0x8000);
			}
		}

		if (System16ScreenFlip) {
			INT32 temp = top;
			top = 224 - bottom;
			bottom = 224 - temp;
			xpos = 320 - xpos;
			xdelta = -1;
		}
		xpos += System16SpriteXOffset;

		/* the rows read: native line r (0 = top) shows row (r + 1) * (32 + vzoom) / 32 */
		const INT32 firstrow = (32 + vzoom) / 32;
		const INT32 lastrow = ((bottom - top) * (32 + vzoom)) / 32;
		const INT32 c = (4 * hzoom) & 0x3f;

		OutrunLodArt art;
		OutrunLodFind(OUTRUN_LOD_FMT_16B, spritebase, bank, spritedata, addr, pitch, flip ? 1 : 0, firstrow, lastrow, &art);
		/* zoom as in Out Run terms (0x200 = 1:1, larger = smaller) */
		const INT32 hz = (0x200 * 64) / (64 - hzoom), vz = (0x200 * (32 + vzoom)) / 32;
		const INT32 hlev = OutrunLodChooseLevel(art, S, hz);
		const INT32 vlev = OutrunLodChooseLevel(art, S, vz);
		const INT32 lev = (hlev > vlev) ? hlev : vlev;

		INT32 lastrowread = -0x7fffffff, lastlev = -1, lastn = 0;
		INT32 lastchild = -0x7fffffff, childn = 0;
		for (INT32 R = 0; R < S * (bottom - top); R++) {
			const INT32 Y = top * S + R;
			if (Y < 0 || Y >= H) continue;

			/* the sprite's own row */
			/* row position (1/65536 rows): native line r shows row (r + 1) * (32 + vzoom) / 32;
			   the finer lines spread from the first row the native lines show to
			   the end of the last one (rows beyond are not this image's, and the
			   last could otherwise be repeated) */
			const INT64 y0 = ((INT64)(32 + vzoom) << 16) / 32;
			const INT64 yc = y0 + (((INT64)(lastrow + 1) << 16) - y0) * R / (S * (bottom - top));
			INT32 crow = (INT32)(yc >> 16);
			if (crow > lastrow) crow = lastrow;
			if (crow != lastchild) {
				childn = OutrunLodReadRow(OUTRUN_LOD_FMT_16B, spritedata, (UINT16)(addr + crow * pitch), flip ? 1 : 0, OutrunLodChildRows[1]);
				lastchild = crow;
			}
			/* where the native row ends (no end marker: 512 pixels drawn) */
			const INT32 q = (childn < 0) ? 0x7fff : childn;
			INT32 NE = (INT32)(((INT64)q * S * (64 - hzoom) - c * S + 63) / 64);
			if (NE > 512 * S) NE = 512 * S;

			/* the level's row (rows past those the larger art was matched on: the sprite's own) */
			const INT32 rl = (crow > art.validrow) ? 0 : lev;
			const OutrunLodLevel &L = art.level[rl];
			const INT32 lrow = OutrunLodRowIndex(art, rl, yc);
			if (lrow != lastrowread || rl != lastlev) {
				lastn = OutrunLodReadRow(OUTRUN_LOD_FMT_16B, spritedata, (UINT16)(L.addr + lrow * L.pitch), flip ? 1 : 0, rowpix);
				if (lastn < 0) lastn = -1;
				lastrowread = lrow;
				lastlev = rl;
			}

			UINT16 *pPixel = pTransDraw + Y * W;
			const INT32 X0 = xpos * S + ((xdelta > 0) ? 0 : S - 1);
			for (INT32 N = 0; N < NE; N++) {
				const INT32 X = X0 + xdelta * N;
				if (xdelta > 0 ? X >= W : X < 0) break;
				const INT64 t = (((INT64)64 * N + c * S) << 16) / ((64 - hzoom) * S);
				const INT32 j = OutrunLodPixelIndex(art, rl, t);
				INT32 pix;
				if (j < 0) continue;
				if (lastn >= 0 && j < lastn) {
					pix = rowpix[j];
				} else if (lastn < 0 && rl == 0) {
					/* no end marker in reach: read the ROM like the hardware */
					const INT32 w = j >> 2;
					const UINT16 pixels = BURN_ENDIAN_SWAP_INT16(spritedata[(UINT16)(flip ? (L.addr + lrow * L.pitch - w) : (L.addr + lrow * L.pitch + w))]);
					pix = flip ? (pixels >> (4 * (j & 3))) & 0xf : (pixels >> (12 - 4 * (j & 3))) & 0xf;
				} else {
					break;
				}
				Shangon16BDrawPixelScaled(X, W, pix, color, pPixel, PalRAM);
			}
		}
	}
}

/* ---- frames ---- */

static void OutrunRenderScaled()
{
	OutrunLodFrameWork = 0;
	System16BUpdateTileValues();
	System16BCreateTileMaps();

	System16CalcPalette();
	OutrunRenderRoadBackgroundLayerScaled();
	OutrunRenderSpriteLayerScaled(1, false);
	OutrunNativeLayerBegin();
	System16BRenderTileLayer(1, 0, 1);
	OutrunNativeLayerEnd();
	OutrunRenderSpriteLayerScaled(2, false);
	OutrunNativeLayerBegin();
	System16BRenderTileLayer(1, 1, 1);
	System16BRenderTileLayer(0, 0, 1);
	OutrunNativeLayerEnd();
	OutrunRenderSpriteLayerScaled(4, false);
	OutrunNativeLayerBegin();
	System16BRenderTileLayer(0, 1, 1);
	OutrunNativeLayerEnd();
	OutrunRenderRoadForegroundLayerScaled();
	OutrunNativeLayerBegin();
	System16BRenderTextLayer(0);
	OutrunNativeLayerEnd();
	OutrunRenderSpriteLayerScaled(8, false);
	OutrunNativeLayerBegin();
	System16BRenderTextLayer(1);
	OutrunNativeLayerEnd();
	BurnTransferCopy(System16Palette);

	if (System16HasGears) BurnShiftRenderScaled(System16ResScale);
}

static void XBoardRenderScaled()
{
	OutrunLodFrameWork = 0;
	System16BUpdateTileValues();
	System16BCreateTileMaps();

	System16CalcPalette();
	OutrunRenderRoadBackgroundLayerScaled();
	if (!System16RoadPriority) OutrunRenderRoadForegroundLayerScaled();
	OutrunRenderSpriteLayerScaled(1, true);
	OutrunNativeLayerBegin();
	System16BRenderTileLayer(1, 0, 1);
	OutrunNativeLayerEnd();
	OutrunRenderSpriteLayerScaled(2, true);
	OutrunNativeLayerBegin();
	System16BRenderTileLayer(1, 1, 1);
	System16BRenderTileLayer(0, 0, 1);
	OutrunNativeLayerEnd();
	OutrunRenderSpriteLayerScaled(4, true);
	OutrunNativeLayerBegin();
	System16BRenderTileLayer(0, 1, 1);
	OutrunNativeLayerEnd();
	if (System16RoadPriority) OutrunRenderRoadForegroundLayerScaled();
	OutrunNativeLayerBegin();
	System16BRenderTextLayer(0);
	OutrunNativeLayerEnd();
	OutrunRenderSpriteLayerScaled(8, true);
	OutrunNativeLayerBegin();
	System16BRenderTextLayer(1);
	OutrunNativeLayerEnd();
	BurnTransferCopy(System16Palette);

	/* (the guns work in the larger screen's coordinates, see XBoardSetResolution) */
	BurnGunDrawTargets();
}

static void ShangonRenderScaled()
{
	OutrunLodFrameWork = 0;
	BurnTransferClear();

	System16BUpdateTileValues();
	System16BAltCreateTileMaps();

	System16CalcPalette();
	OutrunRenderRoadBackgroundLayerScaled();
	OutrunNativeLayerBegin();
	System16BRenderTileLayer(1, 0, 1);
	System16BRenderTileLayer(1, 1, 1);
	System16BRenderTileLayer(0, 0, 1);
	System16BRenderTileLayer(0, 1, 1);
	OutrunNativeLayerEnd();
	OutrunRenderRoadForegroundLayerScaled();
	Shangon16BRenderSpriteLayerScaled();
	OutrunNativeLayerBegin();
	System16BAltRenderTextLayer(0);
	System16BAltRenderTextLayer(1);
	OutrunNativeLayerEnd();
	BurnTransferCopy(System16Palette);
}

// Called from System16GfxThreadsExit().
static void OutrunScaledExit()
{
	BurnFree(OutrunNativeLayer);
	OutrunThreadsExit();
	BurnFree(OutrunLodCache);
	OutrunLodLenFree();
	OutrunLodIndexFree();
	OutrunLodExtFree();
	OutrunLodCacheRom = NULL;
	OutrunRoadRom = NULL;
	OutrunRoadScale = 0;
}

INT32 OutrunRender()
{
	if (!System16VideoEnable) {
		BurnTransferClear();
		return 0;
	}

	if (System16ResScale > 1) {
		OutrunRenderScaled();
		return 0;
	}

	System16BUpdateTileValues();
	System16BCreateTileMaps();
	
	System16CalcPalette();
	OutrunRenderRoadBackgroundLayer();
	OutrunRenderSpriteLayer(1);
	System16BRenderTileLayer(1, 0, 1);
	OutrunRenderSpriteLayer(2);
	System16BRenderTileLayer(1, 1, 1);
	System16BRenderTileLayer(0, 0, 1);
	OutrunRenderSpriteLayer(4);
	System16BRenderTileLayer(0, 1, 1);
	OutrunRenderRoadForegroundLayer();
	System16BRenderTextLayer(0);
	OutrunRenderSpriteLayer(8);
	System16BRenderTextLayer(1);
	BurnTransferCopy(System16Palette);

	if (System16HasGears) BurnShiftRender();

	return 0;
}

INT32 ShangonRender()
{
	if (System16ResScale > 1) {
		ShangonRenderScaled();
		return 0;
	}

	// System16VideoEnable is ignored for Shangon, enabling it will break attract mode.
	BurnTransferClear();

	System16BUpdateTileValues();
	System16BAltCreateTileMaps();
	
	System16CalcPalette();
	OutrunRenderRoadBackgroundLayer();
	//System16BRenderSpriteLayer(1);
	System16BRenderTileLayer(1, 0, 1);
	//System16BRenderSpriteLayer(2);
	System16BRenderTileLayer(1, 1, 1);
	System16BRenderTileLayer(0, 0, 1);
	//System16BRenderSpriteLayer(4);
	System16BRenderTileLayer(0, 1, 1);
	OutrunRenderRoadForegroundLayer();
	//System16BRenderSpriteLayer(8);
	System16BRenderSpriteLayer(0);
	System16BAltRenderTextLayer(0);
	System16BAltRenderTextLayer(1);
	BurnTransferCopy(System16Palette);

	return 0;
}

INT32 XBoardRender()
{
	if (!System16VideoEnable) {
		BurnTransferClear();
		return 0;
	}

	if (System16ResScale > 1) {
		XBoardRenderScaled();
		return 0;
	}

	System16BUpdateTileValues();
	System16BCreateTileMaps();
	
	System16CalcPalette();
	OutrunRenderRoadBackgroundLayer();
	if (!System16RoadPriority) OutrunRenderRoadForegroundLayer();
	XBoardRenderSpriteLayer(1);
	System16BRenderTileLayer(1, 0, 1);
	XBoardRenderSpriteLayer(2);
	System16BRenderTileLayer(1, 1, 1);
	System16BRenderTileLayer(0, 0, 1);
	XBoardRenderSpriteLayer(4);
	System16BRenderTileLayer(0, 1, 1);
	if (System16RoadPriority) OutrunRenderRoadForegroundLayer();
	System16BRenderTextLayer(0);
	XBoardRenderSpriteLayer(8);
	System16BRenderTextLayer(1);
	BurnTransferCopy(System16Palette);
	
	BurnGunDrawTargets();

	return 0;
}

INT32 YBoardRender()
{
	if (!System16VideoEnable) {
		BurnTransferClear();
		return 0;
	}
	
	System16CalcPalette();
	if (System16ResScale > 1) {
		YBoardRenderSpriteLayerScaled();
		System16RotateDrawScaled();
		YBoardSystem16BRenderSpriteLayerScaled();
	} else {
		YBoardRenderSpriteLayer();
		System16RotateDraw();
		YBoardSystem16BRenderSpriteLayer();
	}
	BurnTransferCopy(System16Palette);
	
	BurnGunDrawTargets();

	if (System16HasGears) {
		if (System16ResScale > 1) BurnShiftRenderScaled(System16ResScale);
		else BurnShiftRender();
	}

	return 0;
}
