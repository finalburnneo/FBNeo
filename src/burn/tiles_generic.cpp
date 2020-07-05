/*================================================================================================
Generic Tile Rendering Module - Uses the Colour-Depth Independent Image Transfer Method

Supports 8 x 8, 16 x 16 and 32 x 32 with or without masking and with full flipping. The functions fully
support varying colour-depths and palette offsets as well as all the usual variables.

Call GenericTilesInit() in the driver Init function to store the drivers screen size for clipping.
This function also calls BurnTransferInit().

Call GenericTilesExit() in the driver Exit function to clear the screen size variables.
Again, this function also calls BurnTransferExit().

Otherwise, use the Transfer code as usual.
================================================================================================*/

#include "tiles_generic.h"

UINT8* pTileData;
INT32 nScreenWidth, nScreenHeight;
static INT32 nScreenWidthMax, nScreenHeightMax, nScreenWidthMin, nScreenHeightMin;

UINT8 GenericTilesPRIMASK = 0x00;

INT32 GenericTilesInit()
{
	Debug_GenericTilesInitted = 1;

	INT32 nRet, xAspect, yAspect;

	BurnDrvGetAspect(&xAspect, &yAspect);

	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		BurnDrvGetVisibleSize(&nScreenHeight, &nScreenWidth);
	} else {
		BurnDrvGetVisibleSize(&nScreenWidth, &nScreenHeight);
	}

	nScreenWidthMax = nScreenWidth;
	nScreenHeightMax = nScreenHeight;
	nScreenHeightMin = nScreenWidthMin = 0;

	GenericTilesPRIMASK = 0x00;

	nRet = BurnTransferInit();

	GenericTilemapExit();

	return nRet;
}

INT32 GenericTilesExit()
{
	nScreenWidth = nScreenHeight = 0;
	nScreenWidthMax = nScreenHeightMax = 0;
	nScreenHeightMin = nScreenWidthMin = 0;

	BurnTransferExit();

	Debug_GenericTilesInitted = 0;

	GenericTilemapExit();

	return 0;
}

void GenericTilesSetClip(INT32 nMinx, INT32 nMaxx, INT32 nMiny, INT32 nMaxy)
{
	if (nMinx < 0) nMinx = 0;
	if (nMiny < 0) nMiny = 0;

	if (nMaxx > nScreenWidth) nMaxx = nScreenWidth;
	if (nMaxy > nScreenHeight) nMaxy = nScreenHeight;

	if (nMinx > -1) nScreenWidthMin = nMinx;
	if (nMaxx > -1) nScreenWidthMax = nMaxx;
	if (nMiny > -1) nScreenHeightMin = nMiny;
	if (nMaxy > -1) nScreenHeightMax = nMaxy;
}

void GenericTilesGetClip(INT32 *nMinx, INT32 *nMaxx, INT32 *nMiny, INT32 *nMaxy)
{
	*nMinx = nScreenWidthMin;
	*nMaxx = nScreenWidthMax;
	*nMiny = nScreenHeightMin;
	*nMaxy = nScreenHeightMax;
}

void GenericTilesClearClip()
{
	nScreenWidthMin = 0;
	nScreenWidthMax = nScreenWidth;
	nScreenHeightMin = 0;
	nScreenHeightMax = nScreenHeight;
}

void GenericTilesSetClipRaw(INT32 nMinx, INT32 nMaxx, INT32 nMiny, INT32 nMaxy)
{
	nScreenWidthMin = nMinx;
	nScreenWidthMax = nMaxx;
	nScreenHeightMin = nMiny;
	nScreenHeightMax = nMaxy;

	nScreenWidth = nMaxx;
	nScreenHeight = nMaxy;
}

void GenericTilesClearClipRaw()
{
	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		BurnDrvGetVisibleSize(&nScreenHeight, &nScreenWidth);
	} else {
		BurnDrvGetVisibleSize(&nScreenWidth, &nScreenHeight);
	}

	nScreenWidthMax = nScreenWidth;
	nScreenHeightMax = nScreenHeight;
	nScreenHeightMin = nScreenWidthMin = 0;
}

void GenericTilesSetScanline(INT32 nScanline)
{
	if (nScanline < 0 || nScanline == nScreenHeight) {
		return;
	}

	nScreenHeightMin = nScanline;
	nScreenHeightMax = nScanline + 1;
}

// ----------------------------------------------------------------------------
// Generic Tile Handling

void GenericTilesSetGfx(INT32 nNum, UINT8 *GfxBase, INT32 nDepth, INT32 nTileWidth, INT32 nTileHeight, INT32 nGfxLen, UINT32 nColorOffset, UINT32 nColorMask)
{
	GenericTilemapSetGfx(nNum, GfxBase, nDepth, nTileWidth, nTileHeight, nGfxLen, nColorOffset, nColorMask);
}

// ----------------------------------------------------------------------------
// Colour-depth independant image transfer

UINT16* pTransDraw = NULL;
UINT8 *pPrioDraw = NULL;

static INT32 nTransWidth, nTransHeight;

void BurnTransferClear()
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnTransferInitted) bprintf(PRINT_ERROR, _T("BurnTransferClear called without init\n"));
#endif

	memset((void*)pTransDraw, 0, nTransWidth * nTransHeight * sizeof(UINT16));

	memset(pPrioDraw, 0, nTransWidth * nTransHeight);
}

void BurnPrioClear()
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnTransferInitted) bprintf(PRINT_ERROR, _T("BurnPrioClear called without init\n"));
#endif

	memset(pPrioDraw, 0, nTransWidth * nTransHeight);
}

void BurnTransferClear(UINT16 nFillPattern)
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnTransferInitted) bprintf(PRINT_ERROR, _T("BurnTransferClear called without init\n"));
#endif

	for (INT32 i = 0; i < nTransWidth * nTransHeight; i++) {
		pTransDraw[i] = nFillPattern;
		pPrioDraw[i] = 0;
	}
}

INT32 BurnTransferCopy(UINT32* pPalette)
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnTransferInitted) bprintf(PRINT_ERROR, _T("BurnTransferCopy called without init\n"));
#endif

	UINT16* pSrc = pTransDraw;
	UINT8* pDest = pBurnDraw;

	pBurnDrvPalette = pPalette;

	switch (nBurnBpp) {
		case 2: {
			for (INT32 y = 0; y < nTransHeight; y++, pSrc += nTransWidth, pDest += nBurnPitch) {
				for (INT32 x = 0; x < nTransWidth; x ++) {
					((UINT16*)pDest)[x] = pPalette[pSrc[x]];
				}
			}
			break;
		}
		case 3: {
			for (INT32 y = 0; y < nTransHeight; y++, pSrc += nTransWidth, pDest += nBurnPitch) {
				for (INT32 x = 0; x < nTransWidth; x++) {
					UINT32 c = pPalette[pSrc[x]];
					*(pDest + (x * 3) + 0) = c & 0xFF;
					*(pDest + (x * 3) + 1) = (c >> 8) & 0xFF;
					*(pDest + (x * 3) + 2) = c >> 16;

				}
			}
			break;
		}
		case 4: {
			for (INT32 y = 0; y < nTransHeight; y++, pSrc += nTransWidth, pDest += nBurnPitch) {
				for (INT32 x = 0; x < nTransWidth; x++) {
					((UINT32*)pDest)[x] = pPalette[pSrc[x]];
				}
			}
			break;
		}
	}

	return 0;
}

#define nTransOverflow 16 // 16 lines of overflow, some games spill past the end of the allocated height causing heap corruption.

void BurnTransferExit()
{
#if defined FBNEO_DEBUG
	if (!Debug_BurnTransferInitted) bprintf(PRINT_ERROR, _T("BurnTransferExit called without init\n"));
#endif

	if (Debug_BurnTransferInitted)
	{ // pTransDraw spill detector v.0001.01 - handy for driver development!

		INT32 uhoh_spill = 0;

		for (INT32 y = nTransHeight; y < nTransHeight + nTransOverflow; y++) {
			for (INT32 x = 0; x < nTransWidth; x++) {
				if (pTransDraw[y * nTransWidth + x]) uhoh_spill = 1;
			}
		}
		if (uhoh_spill) {
			bprintf(PRINT_ERROR, _T("!!! BurnTransferExit(): Game wrote past pTransDraw's allocated dimensions!\n"));
		}
	}

	BurnBitmapExit();
	pTransDraw = NULL;
	pPrioDraw = NULL;
//	BurnFree(pTransDraw);
//	BurnFree(pPrioDraw);

	Debug_BurnTransferInitted = 0;
}

INT32 BurnTransferInit()
{
	Debug_BurnTransferInitted = 1;

	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		BurnDrvGetVisibleSize(&nTransHeight, &nTransWidth);
	} else {
		BurnDrvGetVisibleSize(&nTransWidth, &nTransHeight);
	}

	BurnBitmapAllocate(0, nTransWidth, nTransHeight + nTransOverflow, true);

	pTransDraw = BurnBitmapGetBitmap(0);
	pPrioDraw = BurnBitmapGetPriomap(0);

	BurnTransferClear();

	return 0;
}

void BurnTransferFlip(INT32 bFlipX, INT32 bFlipY)
{
	if (bFlipX) {
		UINT16 *tmp = (UINT16*)pBurnDraw; // :D
		for (INT32 y = 0; y < nScreenHeight; y++)
		{
			UINT16 *line = pTransDraw + y * nScreenWidth;
			for (INT32 x = 0; x < nScreenWidth; x++)
			{
				tmp[(nScreenWidth - 1) - x] = line[x];
			}
			memcpy (line, tmp, nScreenWidth * 2);
		}
	}
	if (bFlipY) {
		UINT16 *tmp = (UINT16*)pBurnDraw;
		UINT16 *src1 = pTransDraw;
		UINT16 *src2 = pTransDraw + (nScreenHeight-1) * nScreenWidth;

		for (INT32 y = 0; y < nScreenHeight / 2; y++) {
			memcpy (tmp,  src1, nScreenWidth * 2);
			memcpy (src1, src2, nScreenWidth * 2);
			memcpy (src2, tmp,  nScreenWidth * 2);
			src1 += nScreenWidth;
			src2 -= nScreenWidth;
		}
	}
}

/*================================================================================================
Graphics Decoding
================================================================================================*/

inline static INT32 readbit(const UINT8 *src, INT32 bitnum)
{
	return src[bitnum / 8] & (0x80 >> (bitnum % 8));
}

void GfxDecode(INT32 num, INT32 numPlanes, INT32 xSize, INT32 ySize, INT32 planeoffsets[], INT32 xoffsets[], INT32 yoffsets[], INT32 modulo, UINT8 *pSrc, UINT8 *pDest)
{
	INT32 c;

	for (c = 0; c < num; c++) {
		INT32 plane, x, y;

		UINT8 *dp = pDest + (c * xSize * ySize);
		memset(dp, 0, xSize * ySize);

		for (plane = 0; plane < numPlanes; plane++) {
			INT32 planebit = 1 << (numPlanes - 1 - plane);
			INT32 planeoffs = (c * modulo) + planeoffsets[plane];

			for (y = 0; y < ySize; y++) {
				INT32 yoffs = planeoffs + yoffsets[y];
				dp = pDest + (c * xSize * ySize) + (y * xSize);

				for (x = 0; x < xSize; x++) {
					if (readbit(pSrc, yoffs + xoffsets[x])) dp[x] |= planebit;
				}
			}
		}
	}
}

void GfxDecodeSingle(INT32 which, INT32 numPlanes, INT32 xSize, INT32 ySize, INT32 planeoffsets[], INT32 xoffsets[], INT32 yoffsets[], INT32 modulo, UINT8 *pSrc, UINT8 *pDest)
{
	INT32 plane, x, y;

	UINT8 *dp = pDest + (which * xSize * ySize);
	memset(dp, 0, xSize * ySize);

	for (plane = 0; plane < numPlanes; plane++) {
		INT32 planebit = 1 << (numPlanes - 1 - plane);
		INT32 planeoffs = (which * modulo) + planeoffsets[plane];

		for (y = 0; y < ySize; y++) {
			INT32 yoffs = planeoffs + yoffsets[y];
			dp = pDest + (which * xSize * ySize) + (y * xSize);

			for (x = 0; x < xSize; x++) {
				if (readbit(pSrc, yoffs + xoffsets[x])) dp[x] |= planebit;
			}
		}
	}
}

//================================================================================================

#define PLOTPIXEL_PRIO(x) { pPixel[x] = nPalette + pTileData[x]; pPri[x] = nPriority | (pPri[x] & GenericTilesPRIMASK); }
#define PLOTPIXEL_PRIO_FLIPX(x, a) { pPixel[x] = nPalette + pTileData[a]; pPri[x] = nPriority | (pPri[x] & GenericTilesPRIMASK); }
#define PLOTPIXEL_PRIO_MASK(x, mc) if (pTileData[x] != mc) { pPixel[x] = nPalette + pTileData[x]; pPri[x] = nPriority | (pPri[x] & GenericTilesPRIMASK); }
#define PLOTPIXEL_PRIO_MASK_FLIPX(x, a, mc) if (pTileData[a] != mc) { pPixel[x] = nPalette + pTileData[a]; pPri[x] = nPriority | (pPri[x] & GenericTilesPRIMASK); }

#define PLOTPIXEL(x) pPixel[x] = nPalette + pTileData[x];
#define PLOTPIXEL_FLIPX(x, a) pPixel[x] = nPalette + pTileData[a];
#define PLOTPIXEL_MASK(x, mc) if (pTileData[x] != mc) {pPixel[x] = nPalette + pTileData[x];}
#define PLOTPIXEL_MASK_FLIPX(x, a, mc) if (pTileData[a] != mc) {pPixel[x] = nPalette + pTileData[a] ;}

#define PLOTPIXEL_PRIO_TRANSMASK(x) if (pTransTable[pTileData[x]] == 0) { pPixel[x] = nPalette + pTileData[x]; pPri[x] = nPriority | (pPri[x] & GenericTilesPRIMASK); }
#define PLOTPIXEL_PRIO_TRANSMASK_FLIPX(x, a) if (pTransTable[pTileData[a]] == 0) { pPixel[x] = nPalette + pTileData[a]; pPri[x] = nPriority | (pPri[x] & GenericTilesPRIMASK); }
#define PLOTPIXEL_TRANSMASK(x) if (pTransTable[pTileData[x]] == 0) { pPixel[x] = nPalette + pTileData[x];}
#define PLOTPIXEL_TRANSMASK_FLIPX(x, a) if (pTransTable[pTileData[a]] == 0) { pPixel[x] = nPalette + pTileData[a] ;}

#define CLIPPIXEL(x, sx, a) if ((sx + x) >= nScreenWidthMin && (sx + x) < nScreenWidthMax) { a; };

/*================================================================================================
8 x 8 Functions
================================================================================================*/

void Render8x8Tile(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth ) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pTileData += 8) {
		PLOTPIXEL(0);
		PLOTPIXEL(1);
		PLOTPIXEL(2);
		PLOTPIXEL(3);
		PLOTPIXEL(4);
		PLOTPIXEL(5);
		PLOTPIXEL(6);
		PLOTPIXEL(7);
	}
}

void Render8x8Tile_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(0, StartX, PLOTPIXEL(0));
		CLIPPIXEL(1, StartX, PLOTPIXEL(1));
		CLIPPIXEL(2, StartX, PLOTPIXEL(2));
		CLIPPIXEL(3, StartX, PLOTPIXEL(3));
		CLIPPIXEL(4, StartX, PLOTPIXEL(4));
		CLIPPIXEL(5, StartX, PLOTPIXEL(5));
		CLIPPIXEL(6, StartX, PLOTPIXEL(6));
		CLIPPIXEL(7, StartX, PLOTPIXEL(7));
	}
}

void Render8x8Tile_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pTileData += 8) {
		PLOTPIXEL_FLIPX(7, 0);
		PLOTPIXEL_FLIPX(6, 1);
		PLOTPIXEL_FLIPX(5, 2);
		PLOTPIXEL_FLIPX(4, 3);
		PLOTPIXEL_FLIPX(3, 4);
		PLOTPIXEL_FLIPX(2, 5);
		PLOTPIXEL_FLIPX(1, 6);
		PLOTPIXEL_FLIPX(0, 7);
	}
}

void Render8x8Tile_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(7, StartX, PLOTPIXEL_FLIPX(7, 0));
		CLIPPIXEL(6, StartX, PLOTPIXEL_FLIPX(6, 1));
		CLIPPIXEL(5, StartX, PLOTPIXEL_FLIPX(5, 2));
		CLIPPIXEL(4, StartX, PLOTPIXEL_FLIPX(4, 3));
		CLIPPIXEL(3, StartX, PLOTPIXEL_FLIPX(3, 4));
		CLIPPIXEL(2, StartX, PLOTPIXEL_FLIPX(2, 5));
		CLIPPIXEL(1, StartX, PLOTPIXEL_FLIPX(1, 6));
		CLIPPIXEL(0, StartX, PLOTPIXEL_FLIPX(0, 7));
	}
}

void Render8x8Tile_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 8) {
		PLOTPIXEL(0);
		PLOTPIXEL(1);
		PLOTPIXEL(2);
		PLOTPIXEL(3);
		PLOTPIXEL(4);
		PLOTPIXEL(5);
		PLOTPIXEL(6);
		PLOTPIXEL(7);
	}
}

void Render8x8Tile_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(0, StartX, PLOTPIXEL(0));
		CLIPPIXEL(1, StartX, PLOTPIXEL(1));
		CLIPPIXEL(2, StartX, PLOTPIXEL(2));
		CLIPPIXEL(3, StartX, PLOTPIXEL(3));
		CLIPPIXEL(4, StartX, PLOTPIXEL(4));
		CLIPPIXEL(5, StartX, PLOTPIXEL(5));
		CLIPPIXEL(6, StartX, PLOTPIXEL(6));
		CLIPPIXEL(7, StartX, PLOTPIXEL(7));
	}
}

void Render8x8Tile_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 8) {
		PLOTPIXEL_FLIPX(7, 0);
		PLOTPIXEL_FLIPX(6, 1);
		PLOTPIXEL_FLIPX(5, 2);
		PLOTPIXEL_FLIPX(4, 3);
		PLOTPIXEL_FLIPX(3, 4);
		PLOTPIXEL_FLIPX(2, 5);
		PLOTPIXEL_FLIPX(1, 6);
		PLOTPIXEL_FLIPX(0, 7);
	}
}

void Render8x8Tile_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(7, StartX, PLOTPIXEL_FLIPX(7, 0));
		CLIPPIXEL(6, StartX, PLOTPIXEL_FLIPX(6, 1));
		CLIPPIXEL(5, StartX, PLOTPIXEL_FLIPX(5, 2));
		CLIPPIXEL(4, StartX, PLOTPIXEL_FLIPX(4, 3));
		CLIPPIXEL(3, StartX, PLOTPIXEL_FLIPX(3, 4));
		CLIPPIXEL(2, StartX, PLOTPIXEL_FLIPX(2, 5));
		CLIPPIXEL(1, StartX, PLOTPIXEL_FLIPX(1, 6));
		CLIPPIXEL(0, StartX, PLOTPIXEL_FLIPX(0, 7));
	}
}

/*================================================================================================
8 x 8 Functions with Masking
================================================================================================*/

void Render8x8Tile_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Mask called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pTileData += 8) {
		PLOTPIXEL_MASK(0, nMaskColour);
		PLOTPIXEL_MASK(1, nMaskColour);
		PLOTPIXEL_MASK(2, nMaskColour);
		PLOTPIXEL_MASK(3, nMaskColour);
		PLOTPIXEL_MASK(4, nMaskColour);
		PLOTPIXEL_MASK(5, nMaskColour);
		PLOTPIXEL_MASK(6, nMaskColour);
		PLOTPIXEL_MASK(7, nMaskColour);
	}
}

void Render8x8Tile_Mask_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Mask_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(0, StartX, PLOTPIXEL_MASK(0, nMaskColour));
		CLIPPIXEL(1, StartX, PLOTPIXEL_MASK(1, nMaskColour));
		CLIPPIXEL(2, StartX, PLOTPIXEL_MASK(2, nMaskColour));
		CLIPPIXEL(3, StartX, PLOTPIXEL_MASK(3, nMaskColour));
		CLIPPIXEL(4, StartX, PLOTPIXEL_MASK(4, nMaskColour));
		CLIPPIXEL(5, StartX, PLOTPIXEL_MASK(5, nMaskColour));
		CLIPPIXEL(6, StartX, PLOTPIXEL_MASK(6, nMaskColour));
		CLIPPIXEL(7, StartX, PLOTPIXEL_MASK(7, nMaskColour));
	}
}

void Render8x8Tile_Mask_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Mask_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pTileData += 8) {
		PLOTPIXEL_MASK_FLIPX(7, 0, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(6, 1, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(5, 2, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(4, 3, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(3, 4, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(2, 5, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(1, 6, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(0, 7, nMaskColour);
	}
}

void Render8x8Tile_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Mask_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(7, StartX, PLOTPIXEL_MASK_FLIPX(7, 0, nMaskColour));
		CLIPPIXEL(6, StartX, PLOTPIXEL_MASK_FLIPX(6, 1, nMaskColour));
		CLIPPIXEL(5, StartX, PLOTPIXEL_MASK_FLIPX(5, 2, nMaskColour));
		CLIPPIXEL(4, StartX, PLOTPIXEL_MASK_FLIPX(4, 3, nMaskColour));
		CLIPPIXEL(3, StartX, PLOTPIXEL_MASK_FLIPX(3, 4, nMaskColour));
		CLIPPIXEL(2, StartX, PLOTPIXEL_MASK_FLIPX(2, 5, nMaskColour));
		CLIPPIXEL(1, StartX, PLOTPIXEL_MASK_FLIPX(1, 6, nMaskColour));
		CLIPPIXEL(0, StartX, PLOTPIXEL_MASK_FLIPX(0, 7, nMaskColour));
	}
}

void Render8x8Tile_Mask_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Mask_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 8) {
		PLOTPIXEL_MASK(0, nMaskColour);
		PLOTPIXEL_MASK(1, nMaskColour);
		PLOTPIXEL_MASK(2, nMaskColour);
		PLOTPIXEL_MASK(3, nMaskColour);
		PLOTPIXEL_MASK(4, nMaskColour);
		PLOTPIXEL_MASK(5, nMaskColour);
		PLOTPIXEL_MASK(6, nMaskColour);
		PLOTPIXEL_MASK(7, nMaskColour);
	}
}

void Render8x8Tile_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Mask_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(0, StartX, PLOTPIXEL_MASK(0, nMaskColour));
		CLIPPIXEL(1, StartX, PLOTPIXEL_MASK(1, nMaskColour));
		CLIPPIXEL(2, StartX, PLOTPIXEL_MASK(2, nMaskColour));
		CLIPPIXEL(3, StartX, PLOTPIXEL_MASK(3, nMaskColour));
		CLIPPIXEL(4, StartX, PLOTPIXEL_MASK(4, nMaskColour));
		CLIPPIXEL(5, StartX, PLOTPIXEL_MASK(5, nMaskColour));
		CLIPPIXEL(6, StartX, PLOTPIXEL_MASK(6, nMaskColour));
		CLIPPIXEL(7, StartX, PLOTPIXEL_MASK(7, nMaskColour));
	}
}

void Render8x8Tile_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Mask_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 8) {
		PLOTPIXEL_MASK_FLIPX(7, 0, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(6, 1, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(5, 2, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(4, 3, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(3, 4, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(2, 5, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(1, 6, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(0, 7, nMaskColour);
	}
}

void Render8x8Tile_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Mask_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(7, StartX, PLOTPIXEL_MASK_FLIPX(7, 0, nMaskColour));
		CLIPPIXEL(6, StartX, PLOTPIXEL_MASK_FLIPX(6, 1, nMaskColour));
		CLIPPIXEL(5, StartX, PLOTPIXEL_MASK_FLIPX(5, 2, nMaskColour));
		CLIPPIXEL(4, StartX, PLOTPIXEL_MASK_FLIPX(4, 3, nMaskColour));
		CLIPPIXEL(3, StartX, PLOTPIXEL_MASK_FLIPX(3, 4, nMaskColour));
		CLIPPIXEL(2, StartX, PLOTPIXEL_MASK_FLIPX(2, 5, nMaskColour));
		CLIPPIXEL(1, StartX, PLOTPIXEL_MASK_FLIPX(1, 6, nMaskColour));
		CLIPPIXEL(0, StartX, PLOTPIXEL_MASK_FLIPX(0, 7, nMaskColour));
	}
}

/*================================================================================================
16 x 16 Functions
================================================================================================*/

void Render16x16Tile(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pTileData += 16) {
		PLOTPIXEL( 0);
		PLOTPIXEL( 1);
		PLOTPIXEL( 2);
		PLOTPIXEL( 3);
		PLOTPIXEL( 4);
		PLOTPIXEL( 5);
		PLOTPIXEL( 6);
		PLOTPIXEL( 7);
		PLOTPIXEL( 8);
		PLOTPIXEL( 9);
		PLOTPIXEL(10);
		PLOTPIXEL(11);
		PLOTPIXEL(12);
		PLOTPIXEL(13);
		PLOTPIXEL(14);
		PLOTPIXEL(15);
	}
}

void Render16x16Tile_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL( 0));
		CLIPPIXEL( 1, StartX, PLOTPIXEL( 1));
		CLIPPIXEL( 2, StartX, PLOTPIXEL( 2));
		CLIPPIXEL( 3, StartX, PLOTPIXEL( 3));
		CLIPPIXEL( 4, StartX, PLOTPIXEL( 4));
		CLIPPIXEL( 5, StartX, PLOTPIXEL( 5));
		CLIPPIXEL( 6, StartX, PLOTPIXEL( 6));
		CLIPPIXEL( 7, StartX, PLOTPIXEL( 7));
		CLIPPIXEL( 8, StartX, PLOTPIXEL( 8));
		CLIPPIXEL( 9, StartX, PLOTPIXEL( 9));
		CLIPPIXEL(10, StartX, PLOTPIXEL(10));
		CLIPPIXEL(11, StartX, PLOTPIXEL(11));
		CLIPPIXEL(12, StartX, PLOTPIXEL(12));
		CLIPPIXEL(13, StartX, PLOTPIXEL(13));
		CLIPPIXEL(14, StartX, PLOTPIXEL(14));
		CLIPPIXEL(15, StartX, PLOTPIXEL(15));
	}
}

void Render16x16Tile_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pTileData += 16) {
		PLOTPIXEL_FLIPX(15,  0);
		PLOTPIXEL_FLIPX(14,  1);
		PLOTPIXEL_FLIPX(13,  2);
		PLOTPIXEL_FLIPX(12,  3);
		PLOTPIXEL_FLIPX(11,  4);
		PLOTPIXEL_FLIPX(10,  5);
		PLOTPIXEL_FLIPX( 9,  6);
		PLOTPIXEL_FLIPX( 8,  7);
		PLOTPIXEL_FLIPX( 7,  8);
		PLOTPIXEL_FLIPX( 6,  9);
		PLOTPIXEL_FLIPX( 5, 10);
		PLOTPIXEL_FLIPX( 4, 11);
		PLOTPIXEL_FLIPX( 3, 12);
		PLOTPIXEL_FLIPX( 2, 13);
		PLOTPIXEL_FLIPX( 1, 14);
		PLOTPIXEL_FLIPX( 0, 15);
	}
}

void Render16x16Tile_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(15, StartX, PLOTPIXEL_FLIPX(15,  0));
		CLIPPIXEL(14, StartX, PLOTPIXEL_FLIPX(14,  1));
		CLIPPIXEL(13, StartX, PLOTPIXEL_FLIPX(13,  2));
		CLIPPIXEL(12, StartX, PLOTPIXEL_FLIPX(12,  3));
		CLIPPIXEL(11, StartX, PLOTPIXEL_FLIPX(11,  4));
		CLIPPIXEL(10, StartX, PLOTPIXEL_FLIPX(10,  5));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_FLIPX( 9,  6));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_FLIPX( 8,  7));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_FLIPX( 7,  8));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_FLIPX( 6,  9));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_FLIPX( 5, 10));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_FLIPX( 4, 11));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_FLIPX( 3, 12));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_FLIPX( 2, 13));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_FLIPX( 1, 14));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_FLIPX( 0, 15));
	}
}

void Render16x16Tile_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 16) {
		PLOTPIXEL( 0);
		PLOTPIXEL( 1);
		PLOTPIXEL( 2);
		PLOTPIXEL( 3);
		PLOTPIXEL( 4);
		PLOTPIXEL( 5);
		PLOTPIXEL( 6);
		PLOTPIXEL( 7);
		PLOTPIXEL( 8);
		PLOTPIXEL( 9);
		PLOTPIXEL(10);
		PLOTPIXEL(11);
		PLOTPIXEL(12);
		PLOTPIXEL(13);
		PLOTPIXEL(14);
		PLOTPIXEL(15);
	}
}

void Render16x16Tile_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL( 0));
		CLIPPIXEL( 1, StartX, PLOTPIXEL( 1));
		CLIPPIXEL( 2, StartX, PLOTPIXEL( 2));
		CLIPPIXEL( 3, StartX, PLOTPIXEL( 3));
		CLIPPIXEL( 4, StartX, PLOTPIXEL( 4));
		CLIPPIXEL( 5, StartX, PLOTPIXEL( 5));
		CLIPPIXEL( 6, StartX, PLOTPIXEL( 6));
		CLIPPIXEL( 7, StartX, PLOTPIXEL( 7));
		CLIPPIXEL( 8, StartX, PLOTPIXEL( 8));
		CLIPPIXEL( 9, StartX, PLOTPIXEL( 9));
		CLIPPIXEL(10, StartX, PLOTPIXEL(10));
		CLIPPIXEL(11, StartX, PLOTPIXEL(11));
		CLIPPIXEL(12, StartX, PLOTPIXEL(12));
		CLIPPIXEL(13, StartX, PLOTPIXEL(13));
		CLIPPIXEL(14, StartX, PLOTPIXEL(14));
		CLIPPIXEL(15, StartX, PLOTPIXEL(15));
	}
}

void Render16x16Tile_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 16) {
		PLOTPIXEL_FLIPX(15,  0);
		PLOTPIXEL_FLIPX(14,  1);
		PLOTPIXEL_FLIPX(13,  2);
		PLOTPIXEL_FLIPX(12,  3);
		PLOTPIXEL_FLIPX(11,  4);
		PLOTPIXEL_FLIPX(10,  5);
		PLOTPIXEL_FLIPX( 9,  6);
		PLOTPIXEL_FLIPX( 8,  7);
		PLOTPIXEL_FLIPX( 7,  8);
		PLOTPIXEL_FLIPX( 6,  9);
		PLOTPIXEL_FLIPX( 5, 10);
		PLOTPIXEL_FLIPX( 4, 11);
		PLOTPIXEL_FLIPX( 3, 12);
		PLOTPIXEL_FLIPX( 2, 13);
		PLOTPIXEL_FLIPX( 1, 14);
		PLOTPIXEL_FLIPX( 0, 15);
	}
}

void Render16x16Tile_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(15, StartX, PLOTPIXEL_FLIPX(15,  0));
		CLIPPIXEL(14, StartX, PLOTPIXEL_FLIPX(14,  1));
		CLIPPIXEL(13, StartX, PLOTPIXEL_FLIPX(13,  2));
		CLIPPIXEL(12, StartX, PLOTPIXEL_FLIPX(12,  3));
		CLIPPIXEL(11, StartX, PLOTPIXEL_FLIPX(11,  4));
		CLIPPIXEL(10, StartX, PLOTPIXEL_FLIPX(10,  5));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_FLIPX( 9,  6));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_FLIPX( 8,  7));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_FLIPX( 7,  8));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_FLIPX( 6,  9));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_FLIPX( 5, 10));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_FLIPX( 4, 11));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_FLIPX( 3, 12));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_FLIPX( 2, 13));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_FLIPX( 1, 14));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_FLIPX( 0, 15));
	}
}

/*================================================================================================
16 x 16 Functions with Masking
================================================================================================*/

void Render16x16Tile_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Mask called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pTileData += 16) {
		PLOTPIXEL_MASK( 0, nMaskColour);
		PLOTPIXEL_MASK( 1, nMaskColour);
		PLOTPIXEL_MASK( 2, nMaskColour);
		PLOTPIXEL_MASK( 3, nMaskColour);
		PLOTPIXEL_MASK( 4, nMaskColour);
		PLOTPIXEL_MASK( 5, nMaskColour);
		PLOTPIXEL_MASK( 6, nMaskColour);
		PLOTPIXEL_MASK( 7, nMaskColour);
		PLOTPIXEL_MASK( 8, nMaskColour);
		PLOTPIXEL_MASK( 9, nMaskColour);
		PLOTPIXEL_MASK(10, nMaskColour);
		PLOTPIXEL_MASK(11, nMaskColour);
		PLOTPIXEL_MASK(12, nMaskColour);
		PLOTPIXEL_MASK(13, nMaskColour);
		PLOTPIXEL_MASK(14, nMaskColour);
		PLOTPIXEL_MASK(15, nMaskColour);
	}
}

void Render16x16Tile_Mask_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Mask_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL_MASK( 0, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_MASK( 1, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_MASK( 2, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_MASK( 3, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_MASK( 4, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_MASK( 5, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_MASK( 6, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_MASK( 7, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_MASK( 8, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_MASK( 9, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_MASK(10, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_MASK(11, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_MASK(12, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_MASK(13, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_MASK(14, nMaskColour));
		CLIPPIXEL(15, StartX, PLOTPIXEL_MASK(15, nMaskColour));
	}
}

void Render16x16Tile_Mask_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Mask_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pTileData += 16) {
		PLOTPIXEL_MASK_FLIPX(15,  0, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(14,  1, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(13,  2, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(12,  3, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(11,  4, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(10,  5, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 9,  6, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 8,  7, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 7,  8, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 6,  9, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 5, 10, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 4, 11, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 3, 12, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 2, 13, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 1, 14, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 0, 15, nMaskColour);
	}
}

void Render16x16Tile_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Mask_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(15, StartX, PLOTPIXEL_MASK_FLIPX(15,  0, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_MASK_FLIPX(14,  1, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_MASK_FLIPX(13,  2, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_MASK_FLIPX(12,  3, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_MASK_FLIPX(11,  4, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_MASK_FLIPX(10,  5, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_MASK_FLIPX( 9,  6, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_MASK_FLIPX( 8,  7, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_MASK_FLIPX( 7,  8, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_MASK_FLIPX( 6,  9, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_MASK_FLIPX( 5, 10, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_MASK_FLIPX( 4, 11, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_MASK_FLIPX( 3, 12, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_MASK_FLIPX( 2, 13, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_MASK_FLIPX( 1, 14, nMaskColour));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_MASK_FLIPX( 0, 15, nMaskColour));
	}
}

void Render16x16Tile_Mask_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Mask_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 16) {
		PLOTPIXEL_MASK( 0, nMaskColour);
		PLOTPIXEL_MASK( 1, nMaskColour);
		PLOTPIXEL_MASK( 2, nMaskColour);
		PLOTPIXEL_MASK( 3, nMaskColour);
		PLOTPIXEL_MASK( 4, nMaskColour);
		PLOTPIXEL_MASK( 5, nMaskColour);
		PLOTPIXEL_MASK( 6, nMaskColour);
		PLOTPIXEL_MASK( 7, nMaskColour);
		PLOTPIXEL_MASK( 8, nMaskColour);
		PLOTPIXEL_MASK( 9, nMaskColour);
		PLOTPIXEL_MASK(10, nMaskColour);
		PLOTPIXEL_MASK(11, nMaskColour);
		PLOTPIXEL_MASK(12, nMaskColour);
		PLOTPIXEL_MASK(13, nMaskColour);
		PLOTPIXEL_MASK(14, nMaskColour);
		PLOTPIXEL_MASK(15, nMaskColour);
	}
}

void Render16x16Tile_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Mask_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL_MASK( 0, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_MASK( 1, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_MASK( 2, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_MASK( 3, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_MASK( 4, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_MASK( 5, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_MASK( 6, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_MASK( 7, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_MASK( 8, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_MASK( 9, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_MASK(10, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_MASK(11, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_MASK(12, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_MASK(13, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_MASK(14, nMaskColour));
		CLIPPIXEL(15, StartX, PLOTPIXEL_MASK(15, nMaskColour));
	}
}

void Render16x16Tile_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Mask_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 16) {
		PLOTPIXEL_MASK_FLIPX(15,  0, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(14,  1, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(13,  2, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(12,  3, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(11,  4, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(10,  5, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 9,  6, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 8,  7, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 7,  8, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 6,  9, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 5, 10, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 4, 11, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 3, 12, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 2, 13, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 1, 14, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 0, 15, nMaskColour);
	}
}

void Render16x16Tile_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Mask_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(15, StartX, PLOTPIXEL_MASK_FLIPX(15,  0, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_MASK_FLIPX(14,  1, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_MASK_FLIPX(13,  2, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_MASK_FLIPX(12,  3, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_MASK_FLIPX(11,  4, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_MASK_FLIPX(10,  5, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_MASK_FLIPX( 9,  6, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_MASK_FLIPX( 8,  7, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_MASK_FLIPX( 7,  8, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_MASK_FLIPX( 6,  9, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_MASK_FLIPX( 5, 10, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_MASK_FLIPX( 4, 11, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_MASK_FLIPX( 3, 12, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_MASK_FLIPX( 2, 13, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_MASK_FLIPX( 1, 14, nMaskColour));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_MASK_FLIPX( 0, 15, nMaskColour));
	}
}

/*================================================================================================
32 x 32 Functions
================================================================================================*/

void Render32x32Tile(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pTileData += 32) {
		PLOTPIXEL( 0);
		PLOTPIXEL( 1);
		PLOTPIXEL( 2);
		PLOTPIXEL( 3);
		PLOTPIXEL( 4);
		PLOTPIXEL( 5);
		PLOTPIXEL( 6);
		PLOTPIXEL( 7);
		PLOTPIXEL( 8);
		PLOTPIXEL( 9);
		PLOTPIXEL(10);
		PLOTPIXEL(11);
		PLOTPIXEL(12);
		PLOTPIXEL(13);
		PLOTPIXEL(14);
		PLOTPIXEL(15);
		PLOTPIXEL(16);
		PLOTPIXEL(17);
		PLOTPIXEL(18);
		PLOTPIXEL(19);
		PLOTPIXEL(20);
		PLOTPIXEL(21);
		PLOTPIXEL(22);
		PLOTPIXEL(23);
		PLOTPIXEL(24);
		PLOTPIXEL(25);
		PLOTPIXEL(26);
		PLOTPIXEL(27);
		PLOTPIXEL(28);
		PLOTPIXEL(29);
		PLOTPIXEL(30);
		PLOTPIXEL(31);
	}
}

void Render32x32Tile_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL( 0));
		CLIPPIXEL( 1, StartX, PLOTPIXEL( 1));
		CLIPPIXEL( 2, StartX, PLOTPIXEL( 2));
		CLIPPIXEL( 3, StartX, PLOTPIXEL( 3));
		CLIPPIXEL( 4, StartX, PLOTPIXEL( 4));
		CLIPPIXEL( 5, StartX, PLOTPIXEL( 5));
		CLIPPIXEL( 6, StartX, PLOTPIXEL( 6));
		CLIPPIXEL( 7, StartX, PLOTPIXEL( 7));
		CLIPPIXEL( 8, StartX, PLOTPIXEL( 8));
		CLIPPIXEL( 9, StartX, PLOTPIXEL( 9));
		CLIPPIXEL(10, StartX, PLOTPIXEL(10));
		CLIPPIXEL(11, StartX, PLOTPIXEL(11));
		CLIPPIXEL(12, StartX, PLOTPIXEL(12));
		CLIPPIXEL(13, StartX, PLOTPIXEL(13));
		CLIPPIXEL(14, StartX, PLOTPIXEL(14));
		CLIPPIXEL(15, StartX, PLOTPIXEL(15));
		CLIPPIXEL(16, StartX, PLOTPIXEL(16));
		CLIPPIXEL(17, StartX, PLOTPIXEL(17));
		CLIPPIXEL(18, StartX, PLOTPIXEL(18));
		CLIPPIXEL(19, StartX, PLOTPIXEL(19));
		CLIPPIXEL(20, StartX, PLOTPIXEL(20));
		CLIPPIXEL(21, StartX, PLOTPIXEL(21));
		CLIPPIXEL(22, StartX, PLOTPIXEL(22));
		CLIPPIXEL(23, StartX, PLOTPIXEL(23));
		CLIPPIXEL(24, StartX, PLOTPIXEL(24));
		CLIPPIXEL(25, StartX, PLOTPIXEL(25));
		CLIPPIXEL(26, StartX, PLOTPIXEL(26));
		CLIPPIXEL(27, StartX, PLOTPIXEL(27));
		CLIPPIXEL(28, StartX, PLOTPIXEL(28));
		CLIPPIXEL(29, StartX, PLOTPIXEL(29));
		CLIPPIXEL(30, StartX, PLOTPIXEL(30));
		CLIPPIXEL(31, StartX, PLOTPIXEL(31));
	}
}

void Render32x32Tile_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pTileData += 32) {
		PLOTPIXEL_FLIPX(31,  0);
		PLOTPIXEL_FLIPX(30,  1);
		PLOTPIXEL_FLIPX(29,  2);
		PLOTPIXEL_FLIPX(28,  3);
		PLOTPIXEL_FLIPX(27,  4);
		PLOTPIXEL_FLIPX(26,  5);
		PLOTPIXEL_FLIPX(25,  6);
		PLOTPIXEL_FLIPX(24,  7);
		PLOTPIXEL_FLIPX(23,  8);
		PLOTPIXEL_FLIPX(22,  9);
		PLOTPIXEL_FLIPX(21, 10);
		PLOTPIXEL_FLIPX(20, 11);
		PLOTPIXEL_FLIPX(19, 12);
		PLOTPIXEL_FLIPX(18, 13);
		PLOTPIXEL_FLIPX(17, 14);
		PLOTPIXEL_FLIPX(16, 15);
		PLOTPIXEL_FLIPX(15, 16);
		PLOTPIXEL_FLIPX(14, 17);
		PLOTPIXEL_FLIPX(13, 18);
		PLOTPIXEL_FLIPX(12, 19);
		PLOTPIXEL_FLIPX(11, 20);
		PLOTPIXEL_FLIPX(10, 21);
		PLOTPIXEL_FLIPX( 9, 22);
		PLOTPIXEL_FLIPX( 8, 23);
		PLOTPIXEL_FLIPX( 7, 24);
		PLOTPIXEL_FLIPX( 6, 25);
		PLOTPIXEL_FLIPX( 5, 26);
		PLOTPIXEL_FLIPX( 4, 27);
		PLOTPIXEL_FLIPX( 3, 28);
		PLOTPIXEL_FLIPX( 2, 29);
		PLOTPIXEL_FLIPX( 1, 30);
		PLOTPIXEL_FLIPX( 0, 31);
	}
}

void Render32x32Tile_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(31, StartX, PLOTPIXEL_FLIPX(31,  0));
		CLIPPIXEL(30, StartX, PLOTPIXEL_FLIPX(30,  1));
		CLIPPIXEL(29, StartX, PLOTPIXEL_FLIPX(29,  2));
		CLIPPIXEL(28, StartX, PLOTPIXEL_FLIPX(28,  3));
		CLIPPIXEL(27, StartX, PLOTPIXEL_FLIPX(27,  4));
		CLIPPIXEL(26, StartX, PLOTPIXEL_FLIPX(26,  5));
		CLIPPIXEL(25, StartX, PLOTPIXEL_FLIPX(25,  6));
		CLIPPIXEL(24, StartX, PLOTPIXEL_FLIPX(24,  7));
		CLIPPIXEL(23, StartX, PLOTPIXEL_FLIPX(23,  8));
		CLIPPIXEL(22, StartX, PLOTPIXEL_FLIPX(22,  9));
		CLIPPIXEL(21, StartX, PLOTPIXEL_FLIPX(21, 10));
		CLIPPIXEL(20, StartX, PLOTPIXEL_FLIPX(20, 11));
		CLIPPIXEL(19, StartX, PLOTPIXEL_FLIPX(19, 12));
		CLIPPIXEL(18, StartX, PLOTPIXEL_FLIPX(18, 13));
		CLIPPIXEL(17, StartX, PLOTPIXEL_FLIPX(17, 14));
		CLIPPIXEL(16, StartX, PLOTPIXEL_FLIPX(16, 15));
		CLIPPIXEL(15, StartX, PLOTPIXEL_FLIPX(15, 16));
		CLIPPIXEL(14, StartX, PLOTPIXEL_FLIPX(14, 17));
		CLIPPIXEL(13, StartX, PLOTPIXEL_FLIPX(13, 18));
		CLIPPIXEL(12, StartX, PLOTPIXEL_FLIPX(12, 19));
		CLIPPIXEL(11, StartX, PLOTPIXEL_FLIPX(11, 20));
		CLIPPIXEL(10, StartX, PLOTPIXEL_FLIPX(10, 21));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_FLIPX( 9, 22));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_FLIPX( 8, 23));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_FLIPX( 7, 24));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_FLIPX( 6, 25));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_FLIPX( 5, 26));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_FLIPX( 4, 27));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_FLIPX( 3, 28));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_FLIPX( 2, 29));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_FLIPX( 1, 30));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_FLIPX( 0, 31));
	}
}

void Render32x32Tile_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 32) {
		PLOTPIXEL( 0);
		PLOTPIXEL( 1);
		PLOTPIXEL( 2);
		PLOTPIXEL( 3);
		PLOTPIXEL( 4);
		PLOTPIXEL( 5);
		PLOTPIXEL( 6);
		PLOTPIXEL( 7);
		PLOTPIXEL( 8);
		PLOTPIXEL( 9);
		PLOTPIXEL(10);
		PLOTPIXEL(11);
		PLOTPIXEL(12);
		PLOTPIXEL(13);
		PLOTPIXEL(14);
		PLOTPIXEL(15);
		PLOTPIXEL(16);
		PLOTPIXEL(17);
		PLOTPIXEL(18);
		PLOTPIXEL(19);
		PLOTPIXEL(20);
		PLOTPIXEL(21);
		PLOTPIXEL(22);
		PLOTPIXEL(23);
		PLOTPIXEL(24);
		PLOTPIXEL(25);
		PLOTPIXEL(26);
		PLOTPIXEL(27);
		PLOTPIXEL(28);
		PLOTPIXEL(29);
		PLOTPIXEL(30);
		PLOTPIXEL(31);
	}
}

void Render32x32Tile_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL( 0));
		CLIPPIXEL( 1, StartX, PLOTPIXEL( 1));
		CLIPPIXEL( 2, StartX, PLOTPIXEL( 2));
		CLIPPIXEL( 3, StartX, PLOTPIXEL( 3));
		CLIPPIXEL( 4, StartX, PLOTPIXEL( 4));
		CLIPPIXEL( 5, StartX, PLOTPIXEL( 5));
		CLIPPIXEL( 6, StartX, PLOTPIXEL( 6));
		CLIPPIXEL( 7, StartX, PLOTPIXEL( 7));
		CLIPPIXEL( 8, StartX, PLOTPIXEL( 8));
		CLIPPIXEL( 9, StartX, PLOTPIXEL( 9));
		CLIPPIXEL(10, StartX, PLOTPIXEL(10));
		CLIPPIXEL(11, StartX, PLOTPIXEL(11));
		CLIPPIXEL(12, StartX, PLOTPIXEL(12));
		CLIPPIXEL(13, StartX, PLOTPIXEL(13));
		CLIPPIXEL(14, StartX, PLOTPIXEL(14));
		CLIPPIXEL(15, StartX, PLOTPIXEL(15));
		CLIPPIXEL(16, StartX, PLOTPIXEL(16));
		CLIPPIXEL(17, StartX, PLOTPIXEL(17));
		CLIPPIXEL(18, StartX, PLOTPIXEL(18));
		CLIPPIXEL(19, StartX, PLOTPIXEL(19));
		CLIPPIXEL(20, StartX, PLOTPIXEL(20));
		CLIPPIXEL(21, StartX, PLOTPIXEL(21));
		CLIPPIXEL(22, StartX, PLOTPIXEL(22));
		CLIPPIXEL(23, StartX, PLOTPIXEL(23));
		CLIPPIXEL(24, StartX, PLOTPIXEL(24));
		CLIPPIXEL(25, StartX, PLOTPIXEL(25));
		CLIPPIXEL(26, StartX, PLOTPIXEL(26));
		CLIPPIXEL(27, StartX, PLOTPIXEL(27));
		CLIPPIXEL(28, StartX, PLOTPIXEL(28));
		CLIPPIXEL(29, StartX, PLOTPIXEL(29));
		CLIPPIXEL(30, StartX, PLOTPIXEL(30));
		CLIPPIXEL(31, StartX, PLOTPIXEL(31));
	}
}

void Render32x32Tile_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 32) {
		PLOTPIXEL_FLIPX(31,  0);
		PLOTPIXEL_FLIPX(30,  1);
		PLOTPIXEL_FLIPX(29,  2);
		PLOTPIXEL_FLIPX(28,  3);
		PLOTPIXEL_FLIPX(27,  4);
		PLOTPIXEL_FLIPX(26,  5);
		PLOTPIXEL_FLIPX(25,  6);
		PLOTPIXEL_FLIPX(24,  7);
		PLOTPIXEL_FLIPX(23,  8);
		PLOTPIXEL_FLIPX(22,  9);
		PLOTPIXEL_FLIPX(21, 10);
		PLOTPIXEL_FLIPX(20, 11);
		PLOTPIXEL_FLIPX(19, 12);
		PLOTPIXEL_FLIPX(18, 13);
		PLOTPIXEL_FLIPX(17, 14);
		PLOTPIXEL_FLIPX(16, 15);
		PLOTPIXEL_FLIPX(15, 16);
		PLOTPIXEL_FLIPX(14, 17);
		PLOTPIXEL_FLIPX(13, 18);
		PLOTPIXEL_FLIPX(12, 19);
		PLOTPIXEL_FLIPX(11, 20);
		PLOTPIXEL_FLIPX(10, 21);
		PLOTPIXEL_FLIPX( 9, 22);
		PLOTPIXEL_FLIPX( 8, 23);
		PLOTPIXEL_FLIPX( 7, 24);
		PLOTPIXEL_FLIPX( 6, 25);
		PLOTPIXEL_FLIPX( 5, 26);
		PLOTPIXEL_FLIPX( 4, 27);
		PLOTPIXEL_FLIPX( 3, 28);
		PLOTPIXEL_FLIPX( 2, 29);
		PLOTPIXEL_FLIPX( 1, 30);
		PLOTPIXEL_FLIPX( 0, 31);
	}
}

void Render32x32Tile_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(31, StartX, PLOTPIXEL_FLIPX(31,  0));
		CLIPPIXEL(30, StartX, PLOTPIXEL_FLIPX(30,  1));
		CLIPPIXEL(29, StartX, PLOTPIXEL_FLIPX(29,  2));
		CLIPPIXEL(28, StartX, PLOTPIXEL_FLIPX(28,  3));
		CLIPPIXEL(27, StartX, PLOTPIXEL_FLIPX(27,  4));
		CLIPPIXEL(26, StartX, PLOTPIXEL_FLIPX(26,  5));
		CLIPPIXEL(25, StartX, PLOTPIXEL_FLIPX(25,  6));
		CLIPPIXEL(24, StartX, PLOTPIXEL_FLIPX(24,  7));
		CLIPPIXEL(23, StartX, PLOTPIXEL_FLIPX(23,  8));
		CLIPPIXEL(22, StartX, PLOTPIXEL_FLIPX(22,  9));
		CLIPPIXEL(21, StartX, PLOTPIXEL_FLIPX(21, 10));
		CLIPPIXEL(20, StartX, PLOTPIXEL_FLIPX(20, 11));
		CLIPPIXEL(19, StartX, PLOTPIXEL_FLIPX(19, 12));
		CLIPPIXEL(18, StartX, PLOTPIXEL_FLIPX(18, 13));
		CLIPPIXEL(17, StartX, PLOTPIXEL_FLIPX(17, 14));
		CLIPPIXEL(16, StartX, PLOTPIXEL_FLIPX(16, 15));
		CLIPPIXEL(15, StartX, PLOTPIXEL_FLIPX(15, 16));
		CLIPPIXEL(14, StartX, PLOTPIXEL_FLIPX(14, 17));
		CLIPPIXEL(13, StartX, PLOTPIXEL_FLIPX(13, 18));
		CLIPPIXEL(12, StartX, PLOTPIXEL_FLIPX(12, 19));
		CLIPPIXEL(11, StartX, PLOTPIXEL_FLIPX(11, 20));
		CLIPPIXEL(10, StartX, PLOTPIXEL_FLIPX(10, 21));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_FLIPX( 9, 22));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_FLIPX( 8, 23));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_FLIPX( 7, 24));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_FLIPX( 6, 25));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_FLIPX( 5, 26));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_FLIPX( 4, 27));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_FLIPX( 3, 28));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_FLIPX( 2, 29));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_FLIPX( 1, 30));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_FLIPX( 0, 31));
	}
}

/*================================================================================================
32 x 32 Functions with Masking
================================================================================================*/

void Render32x32Tile_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Mask called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pTileData += 32) {
		PLOTPIXEL_MASK( 0, nMaskColour);
		PLOTPIXEL_MASK( 1, nMaskColour);
		PLOTPIXEL_MASK( 2, nMaskColour);
		PLOTPIXEL_MASK( 3, nMaskColour);
		PLOTPIXEL_MASK( 4, nMaskColour);
		PLOTPIXEL_MASK( 5, nMaskColour);
		PLOTPIXEL_MASK( 6, nMaskColour);
		PLOTPIXEL_MASK( 7, nMaskColour);
		PLOTPIXEL_MASK( 8, nMaskColour);
		PLOTPIXEL_MASK( 9, nMaskColour);
		PLOTPIXEL_MASK(10, nMaskColour);
		PLOTPIXEL_MASK(11, nMaskColour);
		PLOTPIXEL_MASK(12, nMaskColour);
		PLOTPIXEL_MASK(13, nMaskColour);
		PLOTPIXEL_MASK(14, nMaskColour);
		PLOTPIXEL_MASK(15, nMaskColour);
		PLOTPIXEL_MASK(16, nMaskColour);
		PLOTPIXEL_MASK(17, nMaskColour);
		PLOTPIXEL_MASK(18, nMaskColour);
		PLOTPIXEL_MASK(19, nMaskColour);
		PLOTPIXEL_MASK(20, nMaskColour);
		PLOTPIXEL_MASK(21, nMaskColour);
		PLOTPIXEL_MASK(22, nMaskColour);
		PLOTPIXEL_MASK(23, nMaskColour);
		PLOTPIXEL_MASK(24, nMaskColour);
		PLOTPIXEL_MASK(25, nMaskColour);
		PLOTPIXEL_MASK(26, nMaskColour);
		PLOTPIXEL_MASK(27, nMaskColour);
		PLOTPIXEL_MASK(28, nMaskColour);
		PLOTPIXEL_MASK(29, nMaskColour);
		PLOTPIXEL_MASK(30, nMaskColour);
		PLOTPIXEL_MASK(31, nMaskColour);
	}
}

void Render32x32Tile_Mask_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Mask_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL_MASK( 0, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_MASK( 1, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_MASK( 2, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_MASK( 3, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_MASK( 4, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_MASK( 5, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_MASK( 6, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_MASK( 7, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_MASK( 8, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_MASK( 9, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_MASK(10, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_MASK(11, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_MASK(12, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_MASK(13, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_MASK(14, nMaskColour));
		CLIPPIXEL(15, StartX, PLOTPIXEL_MASK(15, nMaskColour));
		CLIPPIXEL(16, StartX, PLOTPIXEL_MASK(16, nMaskColour));
		CLIPPIXEL(17, StartX, PLOTPIXEL_MASK(17, nMaskColour));
		CLIPPIXEL(18, StartX, PLOTPIXEL_MASK(18, nMaskColour));
		CLIPPIXEL(19, StartX, PLOTPIXEL_MASK(19, nMaskColour));
		CLIPPIXEL(20, StartX, PLOTPIXEL_MASK(20, nMaskColour));
		CLIPPIXEL(21, StartX, PLOTPIXEL_MASK(21, nMaskColour));
		CLIPPIXEL(22, StartX, PLOTPIXEL_MASK(22, nMaskColour));
		CLIPPIXEL(23, StartX, PLOTPIXEL_MASK(23, nMaskColour));
		CLIPPIXEL(24, StartX, PLOTPIXEL_MASK(24, nMaskColour));
		CLIPPIXEL(25, StartX, PLOTPIXEL_MASK(25, nMaskColour));
		CLIPPIXEL(26, StartX, PLOTPIXEL_MASK(26, nMaskColour));
		CLIPPIXEL(27, StartX, PLOTPIXEL_MASK(27, nMaskColour));
		CLIPPIXEL(28, StartX, PLOTPIXEL_MASK(28, nMaskColour));
		CLIPPIXEL(29, StartX, PLOTPIXEL_MASK(29, nMaskColour));
		CLIPPIXEL(30, StartX, PLOTPIXEL_MASK(30, nMaskColour));
		CLIPPIXEL(31, StartX, PLOTPIXEL_MASK(31, nMaskColour));
	}
}

void Render32x32Tile_Mask_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Mask_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pTileData += 32) {
		PLOTPIXEL_MASK_FLIPX(31,  0, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(30,  1, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(29,  2, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(28,  3, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(27,  4, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(26,  5, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(25,  6, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(24,  7, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(23,  8, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(22,  9, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(21, 10, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(20, 11, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(19, 12, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(18, 13, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(17, 14, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(16, 15, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(15, 16, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(14, 17, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(13, 18, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(12, 19, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(11, 20, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(10, 21, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 9, 22, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 8, 23, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 7, 24, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 6, 25, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 5, 26, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 4, 27, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 3, 28, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 2, 29, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 1, 30, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 0, 31, nMaskColour);
	}
}

void Render32x32Tile_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Mask_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(31, StartX, PLOTPIXEL_MASK_FLIPX(31,  0, nMaskColour));
		CLIPPIXEL(30, StartX, PLOTPIXEL_MASK_FLIPX(30,  1, nMaskColour));
		CLIPPIXEL(29, StartX, PLOTPIXEL_MASK_FLIPX(29,  2, nMaskColour));
		CLIPPIXEL(28, StartX, PLOTPIXEL_MASK_FLIPX(28,  3, nMaskColour));
		CLIPPIXEL(27, StartX, PLOTPIXEL_MASK_FLIPX(27,  4, nMaskColour));
		CLIPPIXEL(26, StartX, PLOTPIXEL_MASK_FLIPX(26,  5, nMaskColour));
		CLIPPIXEL(25, StartX, PLOTPIXEL_MASK_FLIPX(25,  6, nMaskColour));
		CLIPPIXEL(24, StartX, PLOTPIXEL_MASK_FLIPX(24,  7, nMaskColour));
		CLIPPIXEL(23, StartX, PLOTPIXEL_MASK_FLIPX(23,  8, nMaskColour));
		CLIPPIXEL(22, StartX, PLOTPIXEL_MASK_FLIPX(22,  9, nMaskColour));
		CLIPPIXEL(21, StartX, PLOTPIXEL_MASK_FLIPX(21, 10, nMaskColour));
		CLIPPIXEL(20, StartX, PLOTPIXEL_MASK_FLIPX(20, 11, nMaskColour));
		CLIPPIXEL(19, StartX, PLOTPIXEL_MASK_FLIPX(19, 12, nMaskColour));
		CLIPPIXEL(18, StartX, PLOTPIXEL_MASK_FLIPX(18, 13, nMaskColour));
		CLIPPIXEL(17, StartX, PLOTPIXEL_MASK_FLIPX(17, 14, nMaskColour));
		CLIPPIXEL(16, StartX, PLOTPIXEL_MASK_FLIPX(16, 15, nMaskColour));
		CLIPPIXEL(15, StartX, PLOTPIXEL_MASK_FLIPX(15, 16, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_MASK_FLIPX(14, 17, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_MASK_FLIPX(13, 18, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_MASK_FLIPX(12, 19, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_MASK_FLIPX(11, 20, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_MASK_FLIPX(10, 21, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_MASK_FLIPX( 9, 22, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_MASK_FLIPX( 8, 23, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_MASK_FLIPX( 7, 24, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_MASK_FLIPX( 6, 25, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_MASK_FLIPX( 5, 26, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_MASK_FLIPX( 4, 27, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_MASK_FLIPX( 3, 28, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_MASK_FLIPX( 2, 29, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_MASK_FLIPX( 1, 30, nMaskColour));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_MASK_FLIPX( 0, 31, nMaskColour));
	}
}

void Render32x32Tile_Mask_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Mask_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 32) {
		PLOTPIXEL_MASK( 0, nMaskColour);
		PLOTPIXEL_MASK( 1, nMaskColour);
		PLOTPIXEL_MASK( 2, nMaskColour);
		PLOTPIXEL_MASK( 3, nMaskColour);
		PLOTPIXEL_MASK( 4, nMaskColour);
		PLOTPIXEL_MASK( 5, nMaskColour);
		PLOTPIXEL_MASK( 6, nMaskColour);
		PLOTPIXEL_MASK( 7, nMaskColour);
		PLOTPIXEL_MASK( 8, nMaskColour);
		PLOTPIXEL_MASK( 9, nMaskColour);
		PLOTPIXEL_MASK(10, nMaskColour);
		PLOTPIXEL_MASK(11, nMaskColour);
		PLOTPIXEL_MASK(12, nMaskColour);
		PLOTPIXEL_MASK(13, nMaskColour);
		PLOTPIXEL_MASK(14, nMaskColour);
		PLOTPIXEL_MASK(15, nMaskColour);
		PLOTPIXEL_MASK(16, nMaskColour);
		PLOTPIXEL_MASK(17, nMaskColour);
		PLOTPIXEL_MASK(18, nMaskColour);
		PLOTPIXEL_MASK(19, nMaskColour);
		PLOTPIXEL_MASK(20, nMaskColour);
		PLOTPIXEL_MASK(21, nMaskColour);
		PLOTPIXEL_MASK(22, nMaskColour);
		PLOTPIXEL_MASK(23, nMaskColour);
		PLOTPIXEL_MASK(24, nMaskColour);
		PLOTPIXEL_MASK(25, nMaskColour);
		PLOTPIXEL_MASK(26, nMaskColour);
		PLOTPIXEL_MASK(27, nMaskColour);
		PLOTPIXEL_MASK(28, nMaskColour);
		PLOTPIXEL_MASK(29, nMaskColour);
		PLOTPIXEL_MASK(30, nMaskColour);
		PLOTPIXEL_MASK(31, nMaskColour);
	}
}

void Render32x32Tile_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Mask_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL_MASK( 0, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_MASK( 1, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_MASK( 2, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_MASK( 3, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_MASK( 4, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_MASK( 5, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_MASK( 6, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_MASK( 7, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_MASK( 8, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_MASK( 9, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_MASK(10, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_MASK(11, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_MASK(12, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_MASK(13, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_MASK(14, nMaskColour));
		CLIPPIXEL(15, StartX, PLOTPIXEL_MASK(15, nMaskColour));
		CLIPPIXEL(16, StartX, PLOTPIXEL_MASK(16, nMaskColour));
		CLIPPIXEL(17, StartX, PLOTPIXEL_MASK(17, nMaskColour));
		CLIPPIXEL(18, StartX, PLOTPIXEL_MASK(18, nMaskColour));
		CLIPPIXEL(19, StartX, PLOTPIXEL_MASK(19, nMaskColour));
		CLIPPIXEL(20, StartX, PLOTPIXEL_MASK(20, nMaskColour));
		CLIPPIXEL(21, StartX, PLOTPIXEL_MASK(21, nMaskColour));
		CLIPPIXEL(22, StartX, PLOTPIXEL_MASK(22, nMaskColour));
		CLIPPIXEL(23, StartX, PLOTPIXEL_MASK(23, nMaskColour));
		CLIPPIXEL(24, StartX, PLOTPIXEL_MASK(24, nMaskColour));
		CLIPPIXEL(25, StartX, PLOTPIXEL_MASK(25, nMaskColour));
		CLIPPIXEL(26, StartX, PLOTPIXEL_MASK(26, nMaskColour));
		CLIPPIXEL(27, StartX, PLOTPIXEL_MASK(27, nMaskColour));
		CLIPPIXEL(28, StartX, PLOTPIXEL_MASK(28, nMaskColour));
		CLIPPIXEL(29, StartX, PLOTPIXEL_MASK(29, nMaskColour));
		CLIPPIXEL(30, StartX, PLOTPIXEL_MASK(30, nMaskColour));
		CLIPPIXEL(31, StartX, PLOTPIXEL_MASK(31, nMaskColour));
	}
}

void Render32x32Tile_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Mask_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 32) {
		PLOTPIXEL_MASK_FLIPX(31,  0, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(30,  1, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(29,  2, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(28,  3, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(27,  4, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(26,  5, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(25,  6, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(24,  7, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(23,  8, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(22,  9, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(21, 10, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(20, 11, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(19, 12, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(18, 13, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(17, 14, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(16, 15, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(15, 16, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(14, 17, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(13, 18, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(12, 19, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(11, 20, nMaskColour);
		PLOTPIXEL_MASK_FLIPX(10, 21, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 9, 22, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 8, 23, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 7, 24, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 6, 25, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 5, 26, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 4, 27, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 3, 28, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 2, 29, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 1, 30, nMaskColour);
		PLOTPIXEL_MASK_FLIPX( 0, 31, nMaskColour);
	}
}

void Render32x32Tile_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Mask_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(31, StartX, PLOTPIXEL_MASK_FLIPX(31,  0, nMaskColour));
		CLIPPIXEL(30, StartX, PLOTPIXEL_MASK_FLIPX(30,  1, nMaskColour));
		CLIPPIXEL(29, StartX, PLOTPIXEL_MASK_FLIPX(29,  2, nMaskColour));
		CLIPPIXEL(28, StartX, PLOTPIXEL_MASK_FLIPX(28,  3, nMaskColour));
		CLIPPIXEL(27, StartX, PLOTPIXEL_MASK_FLIPX(27,  4, nMaskColour));
		CLIPPIXEL(26, StartX, PLOTPIXEL_MASK_FLIPX(26,  5, nMaskColour));
		CLIPPIXEL(25, StartX, PLOTPIXEL_MASK_FLIPX(25,  6, nMaskColour));
		CLIPPIXEL(24, StartX, PLOTPIXEL_MASK_FLIPX(24,  7, nMaskColour));
		CLIPPIXEL(23, StartX, PLOTPIXEL_MASK_FLIPX(23,  8, nMaskColour));
		CLIPPIXEL(22, StartX, PLOTPIXEL_MASK_FLIPX(22,  9, nMaskColour));
		CLIPPIXEL(21, StartX, PLOTPIXEL_MASK_FLIPX(21, 10, nMaskColour));
		CLIPPIXEL(20, StartX, PLOTPIXEL_MASK_FLIPX(20, 11, nMaskColour));
		CLIPPIXEL(19, StartX, PLOTPIXEL_MASK_FLIPX(19, 12, nMaskColour));
		CLIPPIXEL(18, StartX, PLOTPIXEL_MASK_FLIPX(18, 13, nMaskColour));
		CLIPPIXEL(17, StartX, PLOTPIXEL_MASK_FLIPX(17, 14, nMaskColour));
		CLIPPIXEL(16, StartX, PLOTPIXEL_MASK_FLIPX(16, 15, nMaskColour));
		CLIPPIXEL(15, StartX, PLOTPIXEL_MASK_FLIPX(15, 16, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_MASK_FLIPX(14, 17, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_MASK_FLIPX(13, 18, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_MASK_FLIPX(12, 19, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_MASK_FLIPX(11, 20, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_MASK_FLIPX(10, 21, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_MASK_FLIPX( 9, 22, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_MASK_FLIPX( 8, 23, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_MASK_FLIPX( 7, 24, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_MASK_FLIPX( 6, 25, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_MASK_FLIPX( 5, 26, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_MASK_FLIPX( 4, 27, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_MASK_FLIPX( 3, 28, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_MASK_FLIPX( 2, 29, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_MASK_FLIPX( 1, 30, nMaskColour));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_MASK_FLIPX( 0, 31, nMaskColour));
	}
}

/*================================================================================================
Custom Height and Width Functions
================================================================================================*/

void RenderCustomTile(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth ) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL(x);
		}
	}
}

void RenderCustomTile_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(x, StartX, PLOTPIXEL(x));
		}
	}
}

void RenderCustomTile_FlipX(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_FLIPX(nWidth - x - 1, x);
		}
	}
}

void RenderCustomTile_FlipX_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(nWidth - x - 1, StartX, PLOTPIXEL_FLIPX(nWidth - x - 1, x));
		}
	}
}

void RenderCustomTile_FlipY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL(x);
		}
	}
}

void RenderCustomTile_FlipY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(x, StartX, PLOTPIXEL(x));
		}
	}
}

void RenderCustomTile_FlipXY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_FLIPX(nWidth - x - 1, x);
		}
	}
}

void RenderCustomTile_FlipXY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(nWidth - x - 1, StartX, PLOTPIXEL_FLIPX(nWidth - x - 1, x));
		}
	}
}

/*================================================================================================
Custom Height and Width Functions with Masking
================================================================================================*/

void RenderCustomTile_Mask(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Mask called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_MASK(x, nMaskColour);
		}
	}
}

void RenderCustomTile_Mask_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Mask_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(x, StartX, PLOTPIXEL_MASK(x, nMaskColour));
		}
	}
}

void RenderCustomTile_Mask_FlipX(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Mask_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_MASK_FLIPX(nWidth - x - 1, x, nMaskColour);
		}
	}
}

void RenderCustomTile_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Mask_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(nWidth - x - 1, StartX, PLOTPIXEL_MASK_FLIPX(nWidth - x - 1, x, nMaskColour));
		}
	}
}

void RenderCustomTile_Mask_FlipY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Mask_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_MASK(x, nMaskColour);
		}
	}
}

void RenderCustomTile_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Mask_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(x, StartX, PLOTPIXEL_MASK(x, nMaskColour));
		}
	}
}

void RenderCustomTile_Mask_FlipXY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Mask_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_MASK_FLIPX(nWidth - x - 1, x, nMaskColour);
		}
	}
}

void RenderCustomTile_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Mask_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(nWidth - x - 1, StartX, PLOTPIXEL_MASK_FLIPX(nWidth - x - 1, x, nMaskColour));
		}
	}
}



void Render8x8Tile_Prio(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth ) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 8) {
		PLOTPIXEL_PRIO(0);
		PLOTPIXEL_PRIO(1);
		PLOTPIXEL_PRIO(2);
		PLOTPIXEL_PRIO(3);
		PLOTPIXEL_PRIO(4);
		PLOTPIXEL_PRIO(5);
		PLOTPIXEL_PRIO(6);
		PLOTPIXEL_PRIO(7);
	}
}

void Render8x8Tile_Prio_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(0, StartX, PLOTPIXEL_PRIO(0));
		CLIPPIXEL(1, StartX, PLOTPIXEL_PRIO(1));
		CLIPPIXEL(2, StartX, PLOTPIXEL_PRIO(2));
		CLIPPIXEL(3, StartX, PLOTPIXEL_PRIO(3));
		CLIPPIXEL(4, StartX, PLOTPIXEL_PRIO(4));
		CLIPPIXEL(5, StartX, PLOTPIXEL_PRIO(5));
		CLIPPIXEL(6, StartX, PLOTPIXEL_PRIO(6));
		CLIPPIXEL(7, StartX, PLOTPIXEL_PRIO(7));
	}
}

void Render8x8Tile_Prio_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 8) {
		PLOTPIXEL_PRIO_FLIPX(7, 0);
		PLOTPIXEL_PRIO_FLIPX(6, 1);
		PLOTPIXEL_PRIO_FLIPX(5, 2);
		PLOTPIXEL_PRIO_FLIPX(4, 3);
		PLOTPIXEL_PRIO_FLIPX(3, 4);
		PLOTPIXEL_PRIO_FLIPX(2, 5);
		PLOTPIXEL_PRIO_FLIPX(1, 6);
		PLOTPIXEL_PRIO_FLIPX(0, 7);
	}
}

void Render8x8Tile_Prio_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(7, StartX, PLOTPIXEL_PRIO_FLIPX(7, 0));
		CLIPPIXEL(6, StartX, PLOTPIXEL_PRIO_FLIPX(6, 1));
		CLIPPIXEL(5, StartX, PLOTPIXEL_PRIO_FLIPX(5, 2));
		CLIPPIXEL(4, StartX, PLOTPIXEL_PRIO_FLIPX(4, 3));
		CLIPPIXEL(3, StartX, PLOTPIXEL_PRIO_FLIPX(3, 4));
		CLIPPIXEL(2, StartX, PLOTPIXEL_PRIO_FLIPX(2, 5));
		CLIPPIXEL(1, StartX, PLOTPIXEL_PRIO_FLIPX(1, 6));
		CLIPPIXEL(0, StartX, PLOTPIXEL_PRIO_FLIPX(0, 7));
	}
}

void Render8x8Tile_Prio_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 8) {
		PLOTPIXEL_PRIO(0);
		PLOTPIXEL_PRIO(1);
		PLOTPIXEL_PRIO(2);
		PLOTPIXEL_PRIO(3);
		PLOTPIXEL_PRIO(4);
		PLOTPIXEL_PRIO(5);
		PLOTPIXEL_PRIO(6);
		PLOTPIXEL_PRIO(7);
	}
}

void Render8x8Tile_Prio_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(0, StartX, PLOTPIXEL_PRIO(0));
		CLIPPIXEL(1, StartX, PLOTPIXEL_PRIO(1));
		CLIPPIXEL(2, StartX, PLOTPIXEL_PRIO(2));
		CLIPPIXEL(3, StartX, PLOTPIXEL_PRIO(3));
		CLIPPIXEL(4, StartX, PLOTPIXEL_PRIO(4));
		CLIPPIXEL(5, StartX, PLOTPIXEL_PRIO(5));
		CLIPPIXEL(6, StartX, PLOTPIXEL_PRIO(6));
		CLIPPIXEL(7, StartX, PLOTPIXEL_PRIO(7));
	}
}

void Render8x8Tile_Prio_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 8) {
		PLOTPIXEL_PRIO_FLIPX(7, 0);
		PLOTPIXEL_PRIO_FLIPX(6, 1);
		PLOTPIXEL_PRIO_FLIPX(5, 2);
		PLOTPIXEL_PRIO_FLIPX(4, 3);
		PLOTPIXEL_PRIO_FLIPX(3, 4);
		PLOTPIXEL_PRIO_FLIPX(2, 5);
		PLOTPIXEL_PRIO_FLIPX(1, 6);
		PLOTPIXEL_PRIO_FLIPX(0, 7);
	}
}

void Render8x8Tile_Prio_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw+ ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(7, StartX, PLOTPIXEL_PRIO_FLIPX(7, 0));
		CLIPPIXEL(6, StartX, PLOTPIXEL_PRIO_FLIPX(6, 1));
		CLIPPIXEL(5, StartX, PLOTPIXEL_PRIO_FLIPX(5, 2));
		CLIPPIXEL(4, StartX, PLOTPIXEL_PRIO_FLIPX(4, 3));
		CLIPPIXEL(3, StartX, PLOTPIXEL_PRIO_FLIPX(3, 4));
		CLIPPIXEL(2, StartX, PLOTPIXEL_PRIO_FLIPX(2, 5));
		CLIPPIXEL(1, StartX, PLOTPIXEL_PRIO_FLIPX(1, 6));
		CLIPPIXEL(0, StartX, PLOTPIXEL_PRIO_FLIPX(0, 7));
	}
}

/*================================================================================================
8 x 8 Functions with Masking
================================================================================================*/

void Render8x8Tile_Prio_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_Mask called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 8) {
		PLOTPIXEL_PRIO_MASK(0, nMaskColour);
		PLOTPIXEL_PRIO_MASK(1, nMaskColour);
		PLOTPIXEL_PRIO_MASK(2, nMaskColour);
		PLOTPIXEL_PRIO_MASK(3, nMaskColour);
		PLOTPIXEL_PRIO_MASK(4, nMaskColour);
		PLOTPIXEL_PRIO_MASK(5, nMaskColour);
		PLOTPIXEL_PRIO_MASK(6, nMaskColour);
		PLOTPIXEL_PRIO_MASK(7, nMaskColour);
	}
}

void Render8x8Tile_Prio_Mask_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_Mask_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(0, StartX, PLOTPIXEL_PRIO_MASK(0, nMaskColour));
		CLIPPIXEL(1, StartX, PLOTPIXEL_PRIO_MASK(1, nMaskColour));
		CLIPPIXEL(2, StartX, PLOTPIXEL_PRIO_MASK(2, nMaskColour));
		CLIPPIXEL(3, StartX, PLOTPIXEL_PRIO_MASK(3, nMaskColour));
		CLIPPIXEL(4, StartX, PLOTPIXEL_PRIO_MASK(4, nMaskColour));
		CLIPPIXEL(5, StartX, PLOTPIXEL_PRIO_MASK(5, nMaskColour));
		CLIPPIXEL(6, StartX, PLOTPIXEL_PRIO_MASK(6, nMaskColour));
		CLIPPIXEL(7, StartX, PLOTPIXEL_PRIO_MASK(7, nMaskColour));
	}
}

void Render8x8Tile_Prio_Mask_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_Mask_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 8) {
		PLOTPIXEL_PRIO_MASK_FLIPX(7, 0, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(6, 1, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(5, 2, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(4, 3, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(3, 4, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(2, 5, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(1, 6, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(0, 7, nMaskColour);
	}
}

void Render8x8Tile_Prio_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_Mask_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 8; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(7, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(7, 0, nMaskColour));
		CLIPPIXEL(6, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(6, 1, nMaskColour));
		CLIPPIXEL(5, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(5, 2, nMaskColour));
		CLIPPIXEL(4, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(4, 3, nMaskColour));
		CLIPPIXEL(3, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(3, 4, nMaskColour));
		CLIPPIXEL(2, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(2, 5, nMaskColour));
		CLIPPIXEL(1, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(1, 6, nMaskColour));
		CLIPPIXEL(0, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(0, 7, nMaskColour));
	}
}

void Render8x8Tile_Prio_Mask_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_Mask_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 8) {
		PLOTPIXEL_PRIO_MASK(0, nMaskColour);
		PLOTPIXEL_PRIO_MASK(1, nMaskColour);
		PLOTPIXEL_PRIO_MASK(2, nMaskColour);
		PLOTPIXEL_PRIO_MASK(3, nMaskColour);
		PLOTPIXEL_PRIO_MASK(4, nMaskColour);
		PLOTPIXEL_PRIO_MASK(5, nMaskColour);
		PLOTPIXEL_PRIO_MASK(6, nMaskColour);
		PLOTPIXEL_PRIO_MASK(7, nMaskColour);
	}
}

void Render8x8Tile_Prio_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_Mask_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(0, StartX, PLOTPIXEL_PRIO_MASK(0, nMaskColour));
		CLIPPIXEL(1, StartX, PLOTPIXEL_PRIO_MASK(1, nMaskColour));
		CLIPPIXEL(2, StartX, PLOTPIXEL_PRIO_MASK(2, nMaskColour));
		CLIPPIXEL(3, StartX, PLOTPIXEL_PRIO_MASK(3, nMaskColour));
		CLIPPIXEL(4, StartX, PLOTPIXEL_PRIO_MASK(4, nMaskColour));
		CLIPPIXEL(5, StartX, PLOTPIXEL_PRIO_MASK(5, nMaskColour));
		CLIPPIXEL(6, StartX, PLOTPIXEL_PRIO_MASK(6, nMaskColour));
		CLIPPIXEL(7, StartX, PLOTPIXEL_PRIO_MASK(7, nMaskColour));
	}
}

void Render8x8Tile_Prio_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_Mask_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 8) {
		PLOTPIXEL_PRIO_MASK_FLIPX(7, 0, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(6, 1, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(5, 2, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(4, 3, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(3, 4, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(2, 5, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(1, 6, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(0, 7, nMaskColour);
	}
}

void Render8x8Tile_Prio_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render8x8Tile_Prio_Mask_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 6);

	UINT16* pPixel = pDestDraw + ((StartY + 7) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 7) * nScreenWidth) + StartX;

	for (INT32 y = 7; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 8) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(7, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(7, 0, nMaskColour));
		CLIPPIXEL(6, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(6, 1, nMaskColour));
		CLIPPIXEL(5, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(5, 2, nMaskColour));
		CLIPPIXEL(4, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(4, 3, nMaskColour));
		CLIPPIXEL(3, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(3, 4, nMaskColour));
		CLIPPIXEL(2, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(2, 5, nMaskColour));
		CLIPPIXEL(1, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(1, 6, nMaskColour));
		CLIPPIXEL(0, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(0, 7, nMaskColour));
	}
}

/*================================================================================================
16 x 16 Functions
================================================================================================*/

void Render16x16Tile_Prio(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 16) {
		PLOTPIXEL_PRIO( 0);
		PLOTPIXEL_PRIO( 1);
		PLOTPIXEL_PRIO( 2);
		PLOTPIXEL_PRIO( 3);
		PLOTPIXEL_PRIO( 4);
		PLOTPIXEL_PRIO( 5);
		PLOTPIXEL_PRIO( 6);
		PLOTPIXEL_PRIO( 7);
		PLOTPIXEL_PRIO( 8);
		PLOTPIXEL_PRIO( 9);
		PLOTPIXEL_PRIO(10);
		PLOTPIXEL_PRIO(11);
		PLOTPIXEL_PRIO(12);
		PLOTPIXEL_PRIO(13);
		PLOTPIXEL_PRIO(14);
		PLOTPIXEL_PRIO(15);
	}
}

void Render16x16Tile_Prio_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO( 0));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO( 1));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO( 2));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO( 3));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO( 4));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO( 5));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO( 6));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO( 7));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO( 8));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO( 9));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO(10));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO(11));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO(12));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO(13));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO(14));
		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO(15));
	}
}

void Render16x16Tile_Prio_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 16) {
		PLOTPIXEL_PRIO_FLIPX(15,  0);
		PLOTPIXEL_PRIO_FLIPX(14,  1);
		PLOTPIXEL_PRIO_FLIPX(13,  2);
		PLOTPIXEL_PRIO_FLIPX(12,  3);
		PLOTPIXEL_PRIO_FLIPX(11,  4);
		PLOTPIXEL_PRIO_FLIPX(10,  5);
		PLOTPIXEL_PRIO_FLIPX( 9,  6);
		PLOTPIXEL_PRIO_FLIPX( 8,  7);
		PLOTPIXEL_PRIO_FLIPX( 7,  8);
		PLOTPIXEL_PRIO_FLIPX( 6,  9);
		PLOTPIXEL_PRIO_FLIPX( 5, 10);
		PLOTPIXEL_PRIO_FLIPX( 4, 11);
		PLOTPIXEL_PRIO_FLIPX( 3, 12);
		PLOTPIXEL_PRIO_FLIPX( 2, 13);
		PLOTPIXEL_PRIO_FLIPX( 1, 14);
		PLOTPIXEL_PRIO_FLIPX( 0, 15);
	}
}

void Render16x16Tile_Prio_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO_FLIPX(15,  0));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO_FLIPX(14,  1));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO_FLIPX(13,  2));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO_FLIPX(12,  3));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO_FLIPX(11,  4));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO_FLIPX(10,  5));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO_FLIPX( 9,  6));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO_FLIPX( 8,  7));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO_FLIPX( 7,  8));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO_FLIPX( 6,  9));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO_FLIPX( 5, 10));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO_FLIPX( 4, 11));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO_FLIPX( 3, 12));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO_FLIPX( 2, 13));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO_FLIPX( 1, 14));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO_FLIPX( 0, 15));
	}
}

void Render16x16Tile_Prio_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 16) {
		PLOTPIXEL_PRIO( 0);
		PLOTPIXEL_PRIO( 1);
		PLOTPIXEL_PRIO( 2);
		PLOTPIXEL_PRIO( 3);
		PLOTPIXEL_PRIO( 4);
		PLOTPIXEL_PRIO( 5);
		PLOTPIXEL_PRIO( 6);
		PLOTPIXEL_PRIO( 7);
		PLOTPIXEL_PRIO( 8);
		PLOTPIXEL_PRIO( 9);
		PLOTPIXEL_PRIO(10);
		PLOTPIXEL_PRIO(11);
		PLOTPIXEL_PRIO(12);
		PLOTPIXEL_PRIO(13);
		PLOTPIXEL_PRIO(14);
		PLOTPIXEL_PRIO(15);
	}
}

void Render16x16Tile_Prio_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO( 0));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO( 1));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO( 2));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO( 3));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO( 4));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO( 5));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO( 6));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO( 7));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO( 8));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO( 9));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO(10));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO(11));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO(12));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO(13));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO(14));
		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO(15));
	}
}

void Render16x16Tile_Prio_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 16) {
		PLOTPIXEL_PRIO_FLIPX(15,  0);
		PLOTPIXEL_PRIO_FLIPX(14,  1);
		PLOTPIXEL_PRIO_FLIPX(13,  2);
		PLOTPIXEL_PRIO_FLIPX(12,  3);
		PLOTPIXEL_PRIO_FLIPX(11,  4);
		PLOTPIXEL_PRIO_FLIPX(10,  5);
		PLOTPIXEL_PRIO_FLIPX( 9,  6);
		PLOTPIXEL_PRIO_FLIPX( 8,  7);
		PLOTPIXEL_PRIO_FLIPX( 7,  8);
		PLOTPIXEL_PRIO_FLIPX( 6,  9);
		PLOTPIXEL_PRIO_FLIPX( 5, 10);
		PLOTPIXEL_PRIO_FLIPX( 4, 11);
		PLOTPIXEL_PRIO_FLIPX( 3, 12);
		PLOTPIXEL_PRIO_FLIPX( 2, 13);
		PLOTPIXEL_PRIO_FLIPX( 1, 14);
		PLOTPIXEL_PRIO_FLIPX( 0, 15);
	}
}

void Render16x16Tile_Prio_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO_FLIPX(15,  0));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO_FLIPX(14,  1));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO_FLIPX(13,  2));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO_FLIPX(12,  3));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO_FLIPX(11,  4));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO_FLIPX(10,  5));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO_FLIPX( 9,  6));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO_FLIPX( 8,  7));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO_FLIPX( 7,  8));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO_FLIPX( 6,  9));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO_FLIPX( 5, 10));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO_FLIPX( 4, 11));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO_FLIPX( 3, 12));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO_FLIPX( 2, 13));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO_FLIPX( 1, 14));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO_FLIPX( 0, 15));
	}
}

/*================================================================================================
16 x 16 Functions with Masking
================================================================================================*/

void Render16x16Tile_Prio_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_Mask called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 16) {
		PLOTPIXEL_PRIO_MASK( 0, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 1, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 2, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 3, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 4, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 5, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 6, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 7, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 8, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 9, nMaskColour);
		PLOTPIXEL_PRIO_MASK(10, nMaskColour);
		PLOTPIXEL_PRIO_MASK(11, nMaskColour);
		PLOTPIXEL_PRIO_MASK(12, nMaskColour);
		PLOTPIXEL_PRIO_MASK(13, nMaskColour);
		PLOTPIXEL_PRIO_MASK(14, nMaskColour);
		PLOTPIXEL_PRIO_MASK(15, nMaskColour);
	}
}

void Render16x16Tile_Prio_Mask_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_Mask_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO_MASK( 0, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO_MASK( 1, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO_MASK( 2, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO_MASK( 3, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO_MASK( 4, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO_MASK( 5, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO_MASK( 6, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO_MASK( 7, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO_MASK( 8, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO_MASK( 9, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO_MASK(10, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO_MASK(11, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO_MASK(12, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO_MASK(13, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO_MASK(14, nMaskColour));
		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO_MASK(15, nMaskColour));
	}
}

void Render16x16Tile_Prio_Mask_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_Mask_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 16) {
		PLOTPIXEL_PRIO_MASK_FLIPX(15,  0, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(14,  1, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(13,  2, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(12,  3, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(11,  4, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(10,  5, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 9,  6, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 8,  7, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 7,  8, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 6,  9, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 5, 10, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 4, 11, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 3, 12, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 2, 13, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 1, 14, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 0, 15, nMaskColour);
	}
}

void Render16x16Tile_Prio_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_Mask_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 16; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(15,  0, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(14,  1, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(13,  2, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(12,  3, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(11,  4, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(10,  5, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 9,  6, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 8,  7, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 7,  8, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 6,  9, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 5, 10, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 4, 11, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 3, 12, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 2, 13, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 1, 14, nMaskColour));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 0, 15, nMaskColour));
	}
}

void Render16x16Tile_Prio_Mask_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_Mask_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 16) {
		PLOTPIXEL_PRIO_MASK( 0, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 1, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 2, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 3, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 4, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 5, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 6, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 7, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 8, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 9, nMaskColour);
		PLOTPIXEL_PRIO_MASK(10, nMaskColour);
		PLOTPIXEL_PRIO_MASK(11, nMaskColour);
		PLOTPIXEL_PRIO_MASK(12, nMaskColour);
		PLOTPIXEL_PRIO_MASK(13, nMaskColour);
		PLOTPIXEL_PRIO_MASK(14, nMaskColour);
		PLOTPIXEL_PRIO_MASK(15, nMaskColour);
	}
}

void Render16x16Tile_Prio_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_Mask_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO_MASK( 0, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO_MASK( 1, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO_MASK( 2, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO_MASK( 3, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO_MASK( 4, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO_MASK( 5, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO_MASK( 6, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO_MASK( 7, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO_MASK( 8, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO_MASK( 9, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO_MASK(10, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO_MASK(11, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO_MASK(12, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO_MASK(13, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO_MASK(14, nMaskColour));
		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO_MASK(15, nMaskColour));
	}
}

void Render16x16Tile_Prio_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_Mask_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 16) {
		PLOTPIXEL_PRIO_MASK_FLIPX(15,  0, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(14,  1, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(13,  2, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(12,  3, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(11,  4, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(10,  5, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 9,  6, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 8,  7, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 7,  8, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 6,  9, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 5, 10, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 4, 11, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 3, 12, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 2, 13, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 1, 14, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 0, 15, nMaskColour);
	}
}

void Render16x16Tile_Prio_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render16x16Tile_Prio_Mask_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 8);

	UINT16* pPixel = pDestDraw + ((StartY + 15) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 15) * nScreenWidth) + StartX;

	for (INT32 y = 15; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 16) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(15,  0, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(14,  1, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(13,  2, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(12,  3, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(11,  4, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(10,  5, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 9,  6, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 8,  7, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 7,  8, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 6,  9, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 5, 10, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 4, 11, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 3, 12, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 2, 13, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 1, 14, nMaskColour));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 0, 15, nMaskColour));
	}
}

/*================================================================================================
32 x 32 Functions
================================================================================================*/

void Render32x32Tile_Prio(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 32) {
		PLOTPIXEL_PRIO( 0);
		PLOTPIXEL_PRIO( 1);
		PLOTPIXEL_PRIO( 2);
		PLOTPIXEL_PRIO( 3);
		PLOTPIXEL_PRIO( 4);
		PLOTPIXEL_PRIO( 5);
		PLOTPIXEL_PRIO( 6);
		PLOTPIXEL_PRIO( 7);
		PLOTPIXEL_PRIO( 8);
		PLOTPIXEL_PRIO( 9);
		PLOTPIXEL_PRIO(10);
		PLOTPIXEL_PRIO(11);
		PLOTPIXEL_PRIO(12);
		PLOTPIXEL_PRIO(13);
		PLOTPIXEL_PRIO(14);
		PLOTPIXEL_PRIO(15);
		PLOTPIXEL_PRIO(16);
		PLOTPIXEL_PRIO(17);
		PLOTPIXEL_PRIO(18);
		PLOTPIXEL_PRIO(19);
		PLOTPIXEL_PRIO(20);
		PLOTPIXEL_PRIO(21);
		PLOTPIXEL_PRIO(22);
		PLOTPIXEL_PRIO(23);
		PLOTPIXEL_PRIO(24);
		PLOTPIXEL_PRIO(25);
		PLOTPIXEL_PRIO(26);
		PLOTPIXEL_PRIO(27);
		PLOTPIXEL_PRIO(28);
		PLOTPIXEL_PRIO(29);
		PLOTPIXEL_PRIO(30);
		PLOTPIXEL_PRIO(31);
	}
}

void Render32x32Tile_Prio_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO( 0));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO( 1));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO( 2));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO( 3));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO( 4));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO( 5));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO( 6));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO( 7));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO( 8));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO( 9));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO(10));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO(11));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO(12));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO(13));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO(14));
		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO(15));
		CLIPPIXEL(16, StartX, PLOTPIXEL_PRIO(16));
		CLIPPIXEL(17, StartX, PLOTPIXEL_PRIO(17));
		CLIPPIXEL(18, StartX, PLOTPIXEL_PRIO(18));
		CLIPPIXEL(19, StartX, PLOTPIXEL_PRIO(19));
		CLIPPIXEL(20, StartX, PLOTPIXEL_PRIO(20));
		CLIPPIXEL(21, StartX, PLOTPIXEL_PRIO(21));
		CLIPPIXEL(22, StartX, PLOTPIXEL_PRIO(22));
		CLIPPIXEL(23, StartX, PLOTPIXEL_PRIO(23));
		CLIPPIXEL(24, StartX, PLOTPIXEL_PRIO(24));
		CLIPPIXEL(25, StartX, PLOTPIXEL_PRIO(25));
		CLIPPIXEL(26, StartX, PLOTPIXEL_PRIO(26));
		CLIPPIXEL(27, StartX, PLOTPIXEL_PRIO(27));
		CLIPPIXEL(28, StartX, PLOTPIXEL_PRIO(28));
		CLIPPIXEL(29, StartX, PLOTPIXEL_PRIO(29));
		CLIPPIXEL(30, StartX, PLOTPIXEL_PRIO(30));
		CLIPPIXEL(31, StartX, PLOTPIXEL_PRIO(31));
	}
}

void Render32x32Tile_Prio_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 32) {
		PLOTPIXEL_PRIO_FLIPX(31,  0);
		PLOTPIXEL_PRIO_FLIPX(30,  1);
		PLOTPIXEL_PRIO_FLIPX(29,  2);
		PLOTPIXEL_PRIO_FLIPX(28,  3);
		PLOTPIXEL_PRIO_FLIPX(27,  4);
		PLOTPIXEL_PRIO_FLIPX(26,  5);
		PLOTPIXEL_PRIO_FLIPX(25,  6);
		PLOTPIXEL_PRIO_FLIPX(24,  7);
		PLOTPIXEL_PRIO_FLIPX(23,  8);
		PLOTPIXEL_PRIO_FLIPX(22,  9);
		PLOTPIXEL_PRIO_FLIPX(21, 10);
		PLOTPIXEL_PRIO_FLIPX(20, 11);
		PLOTPIXEL_PRIO_FLIPX(19, 12);
		PLOTPIXEL_PRIO_FLIPX(18, 13);
		PLOTPIXEL_PRIO_FLIPX(17, 14);
		PLOTPIXEL_PRIO_FLIPX(16, 15);
		PLOTPIXEL_PRIO_FLIPX(15, 16);
		PLOTPIXEL_PRIO_FLIPX(14, 17);
		PLOTPIXEL_PRIO_FLIPX(13, 18);
		PLOTPIXEL_PRIO_FLIPX(12, 19);
		PLOTPIXEL_PRIO_FLIPX(11, 20);
		PLOTPIXEL_PRIO_FLIPX(10, 21);
		PLOTPIXEL_PRIO_FLIPX( 9, 22);
		PLOTPIXEL_PRIO_FLIPX( 8, 23);
		PLOTPIXEL_PRIO_FLIPX( 7, 24);
		PLOTPIXEL_PRIO_FLIPX( 6, 25);
		PLOTPIXEL_PRIO_FLIPX( 5, 26);
		PLOTPIXEL_PRIO_FLIPX( 4, 27);
		PLOTPIXEL_PRIO_FLIPX( 3, 28);
		PLOTPIXEL_PRIO_FLIPX( 2, 29);
		PLOTPIXEL_PRIO_FLIPX( 1, 30);
		PLOTPIXEL_PRIO_FLIPX( 0, 31);
	}
}

void Render32x32Tile_Prio_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(31, StartX, PLOTPIXEL_PRIO_FLIPX(31,  0));
		CLIPPIXEL(30, StartX, PLOTPIXEL_PRIO_FLIPX(30,  1));
		CLIPPIXEL(29, StartX, PLOTPIXEL_PRIO_FLIPX(29,  2));
		CLIPPIXEL(28, StartX, PLOTPIXEL_PRIO_FLIPX(28,  3));
		CLIPPIXEL(27, StartX, PLOTPIXEL_PRIO_FLIPX(27,  4));
		CLIPPIXEL(26, StartX, PLOTPIXEL_PRIO_FLIPX(26,  5));
		CLIPPIXEL(25, StartX, PLOTPIXEL_PRIO_FLIPX(25,  6));
		CLIPPIXEL(24, StartX, PLOTPIXEL_PRIO_FLIPX(24,  7));
		CLIPPIXEL(23, StartX, PLOTPIXEL_PRIO_FLIPX(23,  8));
		CLIPPIXEL(22, StartX, PLOTPIXEL_PRIO_FLIPX(22,  9));
		CLIPPIXEL(21, StartX, PLOTPIXEL_PRIO_FLIPX(21, 10));
		CLIPPIXEL(20, StartX, PLOTPIXEL_PRIO_FLIPX(20, 11));
		CLIPPIXEL(19, StartX, PLOTPIXEL_PRIO_FLIPX(19, 12));
		CLIPPIXEL(18, StartX, PLOTPIXEL_PRIO_FLIPX(18, 13));
		CLIPPIXEL(17, StartX, PLOTPIXEL_PRIO_FLIPX(17, 14));
		CLIPPIXEL(16, StartX, PLOTPIXEL_PRIO_FLIPX(16, 15));
		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO_FLIPX(15, 16));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO_FLIPX(14, 17));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO_FLIPX(13, 18));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO_FLIPX(12, 19));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO_FLIPX(11, 20));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO_FLIPX(10, 21));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO_FLIPX( 9, 22));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO_FLIPX( 8, 23));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO_FLIPX( 7, 24));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO_FLIPX( 6, 25));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO_FLIPX( 5, 26));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO_FLIPX( 4, 27));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO_FLIPX( 3, 28));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO_FLIPX( 2, 29));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO_FLIPX( 1, 30));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO_FLIPX( 0, 31));
	}
}

void Render32x32Tile_Prio_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 32) {
		PLOTPIXEL_PRIO( 0);
		PLOTPIXEL_PRIO( 1);
		PLOTPIXEL_PRIO( 2);
		PLOTPIXEL_PRIO( 3);
		PLOTPIXEL_PRIO( 4);
		PLOTPIXEL_PRIO( 5);
		PLOTPIXEL_PRIO( 6);
		PLOTPIXEL_PRIO( 7);
		PLOTPIXEL_PRIO( 8);
		PLOTPIXEL_PRIO( 9);
		PLOTPIXEL_PRIO(10);
		PLOTPIXEL_PRIO(11);
		PLOTPIXEL_PRIO(12);
		PLOTPIXEL_PRIO(13);
		PLOTPIXEL_PRIO(14);
		PLOTPIXEL_PRIO(15);
		PLOTPIXEL_PRIO(16);
		PLOTPIXEL_PRIO(17);
		PLOTPIXEL_PRIO(18);
		PLOTPIXEL_PRIO(19);
		PLOTPIXEL_PRIO(20);
		PLOTPIXEL_PRIO(21);
		PLOTPIXEL_PRIO(22);
		PLOTPIXEL_PRIO(23);
		PLOTPIXEL_PRIO(24);
		PLOTPIXEL_PRIO(25);
		PLOTPIXEL_PRIO(26);
		PLOTPIXEL_PRIO(27);
		PLOTPIXEL_PRIO(28);
		PLOTPIXEL_PRIO(29);
		PLOTPIXEL_PRIO(30);
		PLOTPIXEL_PRIO(31);
	}
}

void Render32x32Tile_Prio_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO( 0));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO( 1));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO( 2));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO( 3));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO( 4));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO( 5));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO( 6));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO( 7));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO( 8));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO( 9));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO(10));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO(11));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO(12));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO(13));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO(14));
		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO(15));
		CLIPPIXEL(16, StartX, PLOTPIXEL_PRIO(16));
		CLIPPIXEL(17, StartX, PLOTPIXEL_PRIO(17));
		CLIPPIXEL(18, StartX, PLOTPIXEL_PRIO(18));
		CLIPPIXEL(19, StartX, PLOTPIXEL_PRIO(19));
		CLIPPIXEL(20, StartX, PLOTPIXEL_PRIO(20));
		CLIPPIXEL(21, StartX, PLOTPIXEL_PRIO(21));
		CLIPPIXEL(22, StartX, PLOTPIXEL_PRIO(22));
		CLIPPIXEL(23, StartX, PLOTPIXEL_PRIO(23));
		CLIPPIXEL(24, StartX, PLOTPIXEL_PRIO(24));
		CLIPPIXEL(25, StartX, PLOTPIXEL_PRIO(25));
		CLIPPIXEL(26, StartX, PLOTPIXEL_PRIO(26));
		CLIPPIXEL(27, StartX, PLOTPIXEL_PRIO(27));
		CLIPPIXEL(28, StartX, PLOTPIXEL_PRIO(28));
		CLIPPIXEL(29, StartX, PLOTPIXEL_PRIO(29));
		CLIPPIXEL(30, StartX, PLOTPIXEL_PRIO(30));
		CLIPPIXEL(31, StartX, PLOTPIXEL_PRIO(31));
	}
}

void Render32x32Tile_Prio_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 32) {
		PLOTPIXEL_PRIO_FLIPX(31,  0);
		PLOTPIXEL_PRIO_FLIPX(30,  1);
		PLOTPIXEL_PRIO_FLIPX(29,  2);
		PLOTPIXEL_PRIO_FLIPX(28,  3);
		PLOTPIXEL_PRIO_FLIPX(27,  4);
		PLOTPIXEL_PRIO_FLIPX(26,  5);
		PLOTPIXEL_PRIO_FLIPX(25,  6);
		PLOTPIXEL_PRIO_FLIPX(24,  7);
		PLOTPIXEL_PRIO_FLIPX(23,  8);
		PLOTPIXEL_PRIO_FLIPX(22,  9);
		PLOTPIXEL_PRIO_FLIPX(21, 10);
		PLOTPIXEL_PRIO_FLIPX(20, 11);
		PLOTPIXEL_PRIO_FLIPX(19, 12);
		PLOTPIXEL_PRIO_FLIPX(18, 13);
		PLOTPIXEL_PRIO_FLIPX(17, 14);
		PLOTPIXEL_PRIO_FLIPX(16, 15);
		PLOTPIXEL_PRIO_FLIPX(15, 16);
		PLOTPIXEL_PRIO_FLIPX(14, 17);
		PLOTPIXEL_PRIO_FLIPX(13, 18);
		PLOTPIXEL_PRIO_FLIPX(12, 19);
		PLOTPIXEL_PRIO_FLIPX(11, 20);
		PLOTPIXEL_PRIO_FLIPX(10, 21);
		PLOTPIXEL_PRIO_FLIPX( 9, 22);
		PLOTPIXEL_PRIO_FLIPX( 8, 23);
		PLOTPIXEL_PRIO_FLIPX( 7, 24);
		PLOTPIXEL_PRIO_FLIPX( 6, 25);
		PLOTPIXEL_PRIO_FLIPX( 5, 26);
		PLOTPIXEL_PRIO_FLIPX( 4, 27);
		PLOTPIXEL_PRIO_FLIPX( 3, 28);
		PLOTPIXEL_PRIO_FLIPX( 2, 29);
		PLOTPIXEL_PRIO_FLIPX( 1, 30);
		PLOTPIXEL_PRIO_FLIPX( 0, 31);
	}
}

void Render32x32Tile_Prio_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(31, StartX, PLOTPIXEL_PRIO_FLIPX(31,  0));
		CLIPPIXEL(30, StartX, PLOTPIXEL_PRIO_FLIPX(30,  1));
		CLIPPIXEL(29, StartX, PLOTPIXEL_PRIO_FLIPX(29,  2));
		CLIPPIXEL(28, StartX, PLOTPIXEL_PRIO_FLIPX(28,  3));
		CLIPPIXEL(27, StartX, PLOTPIXEL_PRIO_FLIPX(27,  4));
		CLIPPIXEL(26, StartX, PLOTPIXEL_PRIO_FLIPX(26,  5));
		CLIPPIXEL(25, StartX, PLOTPIXEL_PRIO_FLIPX(25,  6));
		CLIPPIXEL(24, StartX, PLOTPIXEL_PRIO_FLIPX(24,  7));
		CLIPPIXEL(23, StartX, PLOTPIXEL_PRIO_FLIPX(23,  8));
		CLIPPIXEL(22, StartX, PLOTPIXEL_PRIO_FLIPX(22,  9));
		CLIPPIXEL(21, StartX, PLOTPIXEL_PRIO_FLIPX(21, 10));
		CLIPPIXEL(20, StartX, PLOTPIXEL_PRIO_FLIPX(20, 11));
		CLIPPIXEL(19, StartX, PLOTPIXEL_PRIO_FLIPX(19, 12));
		CLIPPIXEL(18, StartX, PLOTPIXEL_PRIO_FLIPX(18, 13));
		CLIPPIXEL(17, StartX, PLOTPIXEL_PRIO_FLIPX(17, 14));
		CLIPPIXEL(16, StartX, PLOTPIXEL_PRIO_FLIPX(16, 15));
		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO_FLIPX(15, 16));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO_FLIPX(14, 17));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO_FLIPX(13, 18));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO_FLIPX(12, 19));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO_FLIPX(11, 20));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO_FLIPX(10, 21));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO_FLIPX( 9, 22));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO_FLIPX( 8, 23));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO_FLIPX( 7, 24));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO_FLIPX( 6, 25));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO_FLIPX( 5, 26));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO_FLIPX( 4, 27));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO_FLIPX( 3, 28));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO_FLIPX( 2, 29));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO_FLIPX( 1, 30));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO_FLIPX( 0, 31));
	}
}

/*================================================================================================
32 x 32 Functions with Masking
================================================================================================*/

void Render32x32Tile_Prio_Mask(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_Mask called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 32) {
		PLOTPIXEL_PRIO_MASK( 0, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 1, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 2, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 3, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 4, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 5, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 6, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 7, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 8, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 9, nMaskColour);
		PLOTPIXEL_PRIO_MASK(10, nMaskColour);
		PLOTPIXEL_PRIO_MASK(11, nMaskColour);
		PLOTPIXEL_PRIO_MASK(12, nMaskColour);
		PLOTPIXEL_PRIO_MASK(13, nMaskColour);
		PLOTPIXEL_PRIO_MASK(14, nMaskColour);
		PLOTPIXEL_PRIO_MASK(15, nMaskColour);
		PLOTPIXEL_PRIO_MASK(16, nMaskColour);
		PLOTPIXEL_PRIO_MASK(17, nMaskColour);
		PLOTPIXEL_PRIO_MASK(18, nMaskColour);
		PLOTPIXEL_PRIO_MASK(19, nMaskColour);
		PLOTPIXEL_PRIO_MASK(20, nMaskColour);
		PLOTPIXEL_PRIO_MASK(21, nMaskColour);
		PLOTPIXEL_PRIO_MASK(22, nMaskColour);
		PLOTPIXEL_PRIO_MASK(23, nMaskColour);
		PLOTPIXEL_PRIO_MASK(24, nMaskColour);
		PLOTPIXEL_PRIO_MASK(25, nMaskColour);
		PLOTPIXEL_PRIO_MASK(26, nMaskColour);
		PLOTPIXEL_PRIO_MASK(27, nMaskColour);
		PLOTPIXEL_PRIO_MASK(28, nMaskColour);
		PLOTPIXEL_PRIO_MASK(29, nMaskColour);
		PLOTPIXEL_PRIO_MASK(30, nMaskColour);
		PLOTPIXEL_PRIO_MASK(31, nMaskColour);
	}
}

void Render32x32Tile_Prio_Mask_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_Mask_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO_MASK( 0, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO_MASK( 1, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO_MASK( 2, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO_MASK( 3, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO_MASK( 4, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO_MASK( 5, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO_MASK( 6, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO_MASK( 7, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO_MASK( 8, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO_MASK( 9, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO_MASK(10, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO_MASK(11, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO_MASK(12, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO_MASK(13, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO_MASK(14, nMaskColour));
		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO_MASK(15, nMaskColour));
		CLIPPIXEL(16, StartX, PLOTPIXEL_PRIO_MASK(16, nMaskColour));
		CLIPPIXEL(17, StartX, PLOTPIXEL_PRIO_MASK(17, nMaskColour));
		CLIPPIXEL(18, StartX, PLOTPIXEL_PRIO_MASK(18, nMaskColour));
		CLIPPIXEL(19, StartX, PLOTPIXEL_PRIO_MASK(19, nMaskColour));
		CLIPPIXEL(20, StartX, PLOTPIXEL_PRIO_MASK(20, nMaskColour));
		CLIPPIXEL(21, StartX, PLOTPIXEL_PRIO_MASK(21, nMaskColour));
		CLIPPIXEL(22, StartX, PLOTPIXEL_PRIO_MASK(22, nMaskColour));
		CLIPPIXEL(23, StartX, PLOTPIXEL_PRIO_MASK(23, nMaskColour));
		CLIPPIXEL(24, StartX, PLOTPIXEL_PRIO_MASK(24, nMaskColour));
		CLIPPIXEL(25, StartX, PLOTPIXEL_PRIO_MASK(25, nMaskColour));
		CLIPPIXEL(26, StartX, PLOTPIXEL_PRIO_MASK(26, nMaskColour));
		CLIPPIXEL(27, StartX, PLOTPIXEL_PRIO_MASK(27, nMaskColour));
		CLIPPIXEL(28, StartX, PLOTPIXEL_PRIO_MASK(28, nMaskColour));
		CLIPPIXEL(29, StartX, PLOTPIXEL_PRIO_MASK(29, nMaskColour));
		CLIPPIXEL(30, StartX, PLOTPIXEL_PRIO_MASK(30, nMaskColour));
		CLIPPIXEL(31, StartX, PLOTPIXEL_PRIO_MASK(31, nMaskColour));
	}
}

void Render32x32Tile_Prio_Mask_FlipX(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_Mask_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 32) {
		PLOTPIXEL_PRIO_MASK_FLIPX(31,  0, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(30,  1, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(29,  2, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(28,  3, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(27,  4, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(26,  5, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(25,  6, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(24,  7, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(23,  8, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(22,  9, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(21, 10, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(20, 11, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(19, 12, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(18, 13, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(17, 14, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(16, 15, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(15, 16, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(14, 17, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(13, 18, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(12, 19, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(11, 20, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(10, 21, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 9, 22, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 8, 23, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 7, 24, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 6, 25, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 5, 26, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 4, 27, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 3, 28, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 2, 29, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 1, 30, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 0, 31, nMaskColour);
	}
}

void Render32x32Tile_Prio_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_Mask_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < 32; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(31, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(31,  0, nMaskColour));
		CLIPPIXEL(30, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(30,  1, nMaskColour));
		CLIPPIXEL(29, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(29,  2, nMaskColour));
		CLIPPIXEL(28, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(28,  3, nMaskColour));
		CLIPPIXEL(27, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(27,  4, nMaskColour));
		CLIPPIXEL(26, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(26,  5, nMaskColour));
		CLIPPIXEL(25, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(25,  6, nMaskColour));
		CLIPPIXEL(24, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(24,  7, nMaskColour));
		CLIPPIXEL(23, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(23,  8, nMaskColour));
		CLIPPIXEL(22, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(22,  9, nMaskColour));
		CLIPPIXEL(21, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(21, 10, nMaskColour));
		CLIPPIXEL(20, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(20, 11, nMaskColour));
		CLIPPIXEL(19, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(19, 12, nMaskColour));
		CLIPPIXEL(18, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(18, 13, nMaskColour));
		CLIPPIXEL(17, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(17, 14, nMaskColour));
		CLIPPIXEL(16, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(16, 15, nMaskColour));
		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(15, 16, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(14, 17, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(13, 18, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(12, 19, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(11, 20, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(10, 21, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 9, 22, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 8, 23, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 7, 24, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 6, 25, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 5, 26, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 4, 27, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 3, 28, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 2, 29, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 1, 30, nMaskColour));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 0, 31, nMaskColour));
	}
}

void Render32x32Tile_Prio_Mask_FlipY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_Mask_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 32) {
		PLOTPIXEL_PRIO_MASK( 0, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 1, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 2, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 3, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 4, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 5, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 6, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 7, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 8, nMaskColour);
		PLOTPIXEL_PRIO_MASK( 9, nMaskColour);
		PLOTPIXEL_PRIO_MASK(10, nMaskColour);
		PLOTPIXEL_PRIO_MASK(11, nMaskColour);
		PLOTPIXEL_PRIO_MASK(12, nMaskColour);
		PLOTPIXEL_PRIO_MASK(13, nMaskColour);
		PLOTPIXEL_PRIO_MASK(14, nMaskColour);
		PLOTPIXEL_PRIO_MASK(15, nMaskColour);
		PLOTPIXEL_PRIO_MASK(16, nMaskColour);
		PLOTPIXEL_PRIO_MASK(17, nMaskColour);
		PLOTPIXEL_PRIO_MASK(18, nMaskColour);
		PLOTPIXEL_PRIO_MASK(19, nMaskColour);
		PLOTPIXEL_PRIO_MASK(20, nMaskColour);
		PLOTPIXEL_PRIO_MASK(21, nMaskColour);
		PLOTPIXEL_PRIO_MASK(22, nMaskColour);
		PLOTPIXEL_PRIO_MASK(23, nMaskColour);
		PLOTPIXEL_PRIO_MASK(24, nMaskColour);
		PLOTPIXEL_PRIO_MASK(25, nMaskColour);
		PLOTPIXEL_PRIO_MASK(26, nMaskColour);
		PLOTPIXEL_PRIO_MASK(27, nMaskColour);
		PLOTPIXEL_PRIO_MASK(28, nMaskColour);
		PLOTPIXEL_PRIO_MASK(29, nMaskColour);
		PLOTPIXEL_PRIO_MASK(30, nMaskColour);
		PLOTPIXEL_PRIO_MASK(31, nMaskColour);
	}
}

void Render32x32Tile_Prio_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_Mask_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO_MASK( 0, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO_MASK( 1, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO_MASK( 2, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO_MASK( 3, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO_MASK( 4, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO_MASK( 5, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO_MASK( 6, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO_MASK( 7, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO_MASK( 8, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO_MASK( 9, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO_MASK(10, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO_MASK(11, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO_MASK(12, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO_MASK(13, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO_MASK(14, nMaskColour));
		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO_MASK(15, nMaskColour));
		CLIPPIXEL(16, StartX, PLOTPIXEL_PRIO_MASK(16, nMaskColour));
		CLIPPIXEL(17, StartX, PLOTPIXEL_PRIO_MASK(17, nMaskColour));
		CLIPPIXEL(18, StartX, PLOTPIXEL_PRIO_MASK(18, nMaskColour));
		CLIPPIXEL(19, StartX, PLOTPIXEL_PRIO_MASK(19, nMaskColour));
		CLIPPIXEL(20, StartX, PLOTPIXEL_PRIO_MASK(20, nMaskColour));
		CLIPPIXEL(21, StartX, PLOTPIXEL_PRIO_MASK(21, nMaskColour));
		CLIPPIXEL(22, StartX, PLOTPIXEL_PRIO_MASK(22, nMaskColour));
		CLIPPIXEL(23, StartX, PLOTPIXEL_PRIO_MASK(23, nMaskColour));
		CLIPPIXEL(24, StartX, PLOTPIXEL_PRIO_MASK(24, nMaskColour));
		CLIPPIXEL(25, StartX, PLOTPIXEL_PRIO_MASK(25, nMaskColour));
		CLIPPIXEL(26, StartX, PLOTPIXEL_PRIO_MASK(26, nMaskColour));
		CLIPPIXEL(27, StartX, PLOTPIXEL_PRIO_MASK(27, nMaskColour));
		CLIPPIXEL(28, StartX, PLOTPIXEL_PRIO_MASK(28, nMaskColour));
		CLIPPIXEL(29, StartX, PLOTPIXEL_PRIO_MASK(29, nMaskColour));
		CLIPPIXEL(30, StartX, PLOTPIXEL_PRIO_MASK(30, nMaskColour));
		CLIPPIXEL(31, StartX, PLOTPIXEL_PRIO_MASK(31, nMaskColour));
	}
}

void Render32x32Tile_Prio_Mask_FlipXY(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_Mask_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 32) {
		PLOTPIXEL_PRIO_MASK_FLIPX(31,  0, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(30,  1, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(29,  2, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(28,  3, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(27,  4, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(26,  5, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(25,  6, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(24,  7, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(23,  8, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(22,  9, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(21, 10, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(20, 11, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(19, 12, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(18, 13, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(17, 14, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(16, 15, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(15, 16, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(14, 17, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(13, 18, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(12, 19, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(11, 20, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX(10, 21, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 9, 22, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 8, 23, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 7, 24, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 6, 25, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 5, 26, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 4, 27, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 3, 28, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 2, 29, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 1, 30, nMaskColour);
		PLOTPIXEL_PRIO_MASK_FLIPX( 0, 31, nMaskColour);
	}
}

void Render32x32Tile_Prio_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("Render32x32Tile_Prio_Mask_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber << 10);

	UINT16* pPixel = pDestDraw + ((StartY + 31) * nScreenWidthMax) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + 31) * nScreenWidthMax) + StartX;

	for (INT32 y = 31; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += 32) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		CLIPPIXEL(31, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(31,  0, nMaskColour));
		CLIPPIXEL(30, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(30,  1, nMaskColour));
		CLIPPIXEL(29, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(29,  2, nMaskColour));
		CLIPPIXEL(28, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(28,  3, nMaskColour));
		CLIPPIXEL(27, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(27,  4, nMaskColour));
		CLIPPIXEL(26, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(26,  5, nMaskColour));
		CLIPPIXEL(25, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(25,  6, nMaskColour));
		CLIPPIXEL(24, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(24,  7, nMaskColour));
		CLIPPIXEL(23, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(23,  8, nMaskColour));
		CLIPPIXEL(22, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(22,  9, nMaskColour));
		CLIPPIXEL(21, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(21, 10, nMaskColour));
		CLIPPIXEL(20, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(20, 11, nMaskColour));
		CLIPPIXEL(19, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(19, 12, nMaskColour));
		CLIPPIXEL(18, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(18, 13, nMaskColour));
		CLIPPIXEL(17, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(17, 14, nMaskColour));
		CLIPPIXEL(16, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(16, 15, nMaskColour));
		CLIPPIXEL(15, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(15, 16, nMaskColour));
		CLIPPIXEL(14, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(14, 17, nMaskColour));
		CLIPPIXEL(13, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(13, 18, nMaskColour));
		CLIPPIXEL(12, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(12, 19, nMaskColour));
		CLIPPIXEL(11, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(11, 20, nMaskColour));
		CLIPPIXEL(10, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(10, 21, nMaskColour));
		CLIPPIXEL( 9, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 9, 22, nMaskColour));
		CLIPPIXEL( 8, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 8, 23, nMaskColour));
		CLIPPIXEL( 7, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 7, 24, nMaskColour));
		CLIPPIXEL( 6, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 6, 25, nMaskColour));
		CLIPPIXEL( 5, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 5, 26, nMaskColour));
		CLIPPIXEL( 4, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 4, 27, nMaskColour));
		CLIPPIXEL( 3, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 3, 28, nMaskColour));
		CLIPPIXEL( 2, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 2, 29, nMaskColour));
		CLIPPIXEL( 1, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 1, 30, nMaskColour));
		CLIPPIXEL( 0, StartX, PLOTPIXEL_PRIO_MASK_FLIPX( 0, 31, nMaskColour));
	}
}

/*================================================================================================
Custom Height and Width Functions
================================================================================================*/

void RenderCustomTile_Prio(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth ) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_PRIO(x);
		}
	}
}

void RenderCustomTile_Prio_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(x, StartX, PLOTPIXEL_PRIO(x));
		}
	}
}

void RenderCustomTile_Prio_FlipX(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_PRIO_FLIPX(nWidth - x - 1, x);
		}
	}
}

void RenderCustomTile_Prio_FlipX_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(nWidth - x - 1, StartX, PLOTPIXEL_PRIO_FLIPX(nWidth - x - 1, x));
		}
	}
}

void RenderCustomTile_Prio_FlipY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_PRIO(x);
		}
	}
}

void RenderCustomTile_Prio_FlipY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(x, StartX, PLOTPIXEL_PRIO(x));
		}
	}
}

void RenderCustomTile_Prio_FlipXY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_PRIO_FLIPX(nWidth - x - 1, x);
		}
	}
}

void RenderCustomTile_Prio_FlipXY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(nWidth - x - 1, StartX, PLOTPIXEL_PRIO_FLIPX(nWidth - x - 1, x));
		}
	}
}

/*================================================================================================
Custom Height and Width Functions with Masking
================================================================================================*/

void RenderCustomTile_Prio_Mask(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_PRIO_MASK(x, nMaskColour);
		}
	}
}

void RenderCustomTile_Prio_Mask_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(x, StartX, PLOTPIXEL_PRIO_MASK(x, nMaskColour));
		}
	}
}

void RenderCustomTile_Prio_Mask_FlipX(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_PRIO_MASK_FLIPX(nWidth - x - 1, x, nMaskColour);
		}
	}
}

void RenderCustomTile_Prio_Mask_FlipX_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(nWidth - x - 1, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(nWidth - x - 1, x, nMaskColour));
		}
	}
}

void RenderCustomTile_Prio_Mask_FlipY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_PRIO_MASK(x, nMaskColour);
		}
	}
}

void RenderCustomTile_Prio_Mask_FlipY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(x, StartX, PLOTPIXEL_PRIO_MASK(x, nMaskColour));
		}
	}
}

void RenderCustomTile_Prio_Mask_FlipXY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_PRIO_MASK_FLIPX(nWidth - x - 1, x, nMaskColour);
		}
	}
}

void RenderCustomTile_Prio_Mask_FlipXY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(nWidth - x - 1, StartX, PLOTPIXEL_PRIO_MASK_FLIPX(nWidth - x - 1, x, nMaskColour));
		}
	}
}









void RenderCustomTile_Prio_TransMask(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_PRIO_TRANSMASK(x);
		}
	}
}

void RenderCustomTile_Prio_TransMask_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(x, StartX, PLOTPIXEL_PRIO_TRANSMASK(x));
		}
	}
}

void RenderCustomTile_Prio_TransMask_FlipX(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_FlipX called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_PRIO_TRANSMASK_FLIPX(nWidth - x - 1, x);
		}
	}
}

void RenderCustomTile_Prio_TransMask_FlipX_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_FlipX_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + (StartY * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + (StartY * nScreenWidth) + StartX;

	for (INT32 y = 0; y < nHeight; y++, pPixel += nScreenWidth, pPri += nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(nWidth - x - 1, StartX, PLOTPIXEL_PRIO_TRANSMASK_FLIPX(nWidth - x - 1, x));
		}
	}
}

void RenderCustomTile_Prio_TransMask_FlipY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_FlipY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_PRIO_TRANSMASK(x);
		}
	}
}

void RenderCustomTile_Prio_TransMask_FlipY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_FlipY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(x, StartX, PLOTPIXEL_PRIO_TRANSMASK(x));
		}
	}
}

void RenderCustomTile_Prio_TransMask_FlipXY(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_FlipXY called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += nWidth) {
		for (INT32 x = 0; x < nWidth; x++) {
			PLOTPIXEL_PRIO_TRANSMASK_FLIPX(nWidth - x - 1, x);
		}
	}
}

void RenderCustomTile_Prio_TransMask_FlipXY_Clip(UINT16* pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 nTilePalette, INT32 nColourDepth, UINT8 *pTransTable, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderCustomTile_Prio_Mask_FlipXY_Clip called without init\n"));
#endif

	UINT32 nPalette = (nTilePalette << nColourDepth) + nPaletteOffset;
	pTileData = pTile + (nTileNumber * nWidth * nHeight);

	UINT16* pPixel = pDestDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;
	UINT8 *pPri = pPrioDraw + ((StartY + nHeight - 1) * nScreenWidth) + StartX;

	for (INT32 y = nHeight - 1; y >= 0; y--, pPixel -= nScreenWidth, pPri -= nScreenWidth, pTileData += nWidth) {
		if ((StartY + y) < nScreenHeightMin || (StartY + y) >= nScreenHeightMax) {
			continue;
		}

		for (INT32 x = 0; x < nWidth; x++) {
			CLIPPIXEL(nWidth - x - 1, StartX, PLOTPIXEL_PRIO_TRANSMASK_FLIPX(nWidth - x - 1, x));
		}
	}
}



#undef PLOTPIXEL_PRIO_TRANSMASK
#undef PLOTPIXEL_PRIO_MASK_FLIPX
#undef PLOTPIXEL_TRANSMASK
#undef PLOTPIXEL_TRANSMASK_FLIPX

#undef PLOTPIXEL
#undef PLOTPIXEL_FLIPX
#undef PLOTPIXEL_MASK
#undef CLIPPIXEL

#undef PLOTPIXEL_PRIO
#undef PLOTPIXEL_PRIO_FLIPX
#undef PLOTPIXEL_PRIO_MASK
#undef PLOTPIXEL_PRIO_MASK_FLIPX

/*================================================================================================
Merged tile functions (flipping/clipping merged)
================================================================================================*/


void Draw8x8Tile(UINT16 *pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - 8) || StartY <= (nScreenHeightMin - 8) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - 8) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - 8) ||
		(nScreenWidthMax - nScreenWidthMin) < 8 || (nScreenHeightMax - nScreenHeightMin) < 8)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render8x8Tile_FlipXY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				Render8x8Tile_FlipY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render8x8Tile_FlipX_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				Render8x8Tile_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render8x8Tile_FlipXY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				Render8x8Tile_FlipY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render8x8Tile_FlipX(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				Render8x8Tile(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
	}
}

void Draw8x8MaskTile(UINT16 *pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - 8) || StartY <= (nScreenHeightMin - 8) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - 8) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - 8) ||
		(nScreenWidthMax - nScreenWidthMin) < 8 || (nScreenHeightMax - nScreenHeightMin) < 8)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render8x8Tile_Mask_FlipXY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				Render8x8Tile_Mask_FlipY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render8x8Tile_Mask_FlipX_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				Render8x8Tile_Mask_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render8x8Tile_Mask_FlipXY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				Render8x8Tile_Mask_FlipY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render8x8Tile_Mask_FlipX(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				Render8x8Tile_Mask(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
	}
}

void Draw8x8PrioTile(UINT16 *pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - 8) || StartY <= (nScreenHeightMin - 8) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - 8) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - 8) ||
		(nScreenWidthMax - nScreenWidthMin) < 8 || (nScreenHeightMax - nScreenHeightMin) < 8)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render8x8Tile_Prio_FlipXY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render8x8Tile_Prio_FlipY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render8x8Tile_Prio_FlipX_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render8x8Tile_Prio_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render8x8Tile_Prio_FlipXY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render8x8Tile_Prio_FlipY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render8x8Tile_Prio_FlipX(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render8x8Tile_Prio(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
	}
}

void Draw8x8PrioMaskTile(UINT16 *pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - 8) || StartY <= (nScreenHeightMin - 8) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - 8) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - 8) ||
		(nScreenWidthMax - nScreenWidthMin) < 8 || (nScreenHeightMax - nScreenHeightMin) < 8)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render8x8Tile_Prio_Mask_FlipXY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render8x8Tile_Prio_Mask_FlipY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render8x8Tile_Prio_Mask_FlipX_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render8x8Tile_Prio_Mask_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render8x8Tile_Prio_Mask_FlipXY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render8x8Tile_Prio_Mask_FlipY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render8x8Tile_Prio_Mask_FlipX(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render8x8Tile_Prio_Mask(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
	}
}

void Draw16x16Tile(UINT16 *pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - 16) || StartY <= (nScreenHeightMin - 16) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - 16) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - 16) ||
		(nScreenWidthMax - nScreenWidthMin) < 16 || (nScreenHeightMax - nScreenHeightMin) < 16)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render16x16Tile_FlipXY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				Render16x16Tile_FlipY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render16x16Tile_FlipX_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				Render16x16Tile_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render16x16Tile_FlipXY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				Render16x16Tile_FlipY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render16x16Tile_FlipX(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				Render16x16Tile(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
	}
}

void Draw16x16MaskTile(UINT16 *pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - 16) || StartY <= (nScreenHeightMin - 16) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - 16) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - 16) ||
		(nScreenWidthMax - nScreenWidthMin) < 16 || (nScreenHeightMax - nScreenHeightMin) < 16)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render16x16Tile_Mask_FlipXY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				Render16x16Tile_Mask_FlipY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render16x16Tile_Mask_FlipX_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				Render16x16Tile_Mask_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render16x16Tile_Mask_FlipXY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				Render16x16Tile_Mask_FlipY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render16x16Tile_Mask_FlipX(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				Render16x16Tile_Mask(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
	}
}

void Draw16x16PrioTile(UINT16 *pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - 16) || StartY <= (nScreenHeightMin - 16) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - 16) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - 16) ||
		(nScreenWidthMax - nScreenWidthMin) < 16 || (nScreenHeightMax - nScreenHeightMin) < 16)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render16x16Tile_Prio_FlipXY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render16x16Tile_Prio_FlipY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render16x16Tile_Prio_FlipX_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render16x16Tile_Prio_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render16x16Tile_Prio_FlipXY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render16x16Tile_Prio_FlipY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render16x16Tile_Prio_FlipX(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render16x16Tile_Prio(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
	}
}

void Draw16x16PrioMaskTile(UINT16 *pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - 16) || StartY <= (nScreenHeightMin - 16) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - 16) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - 16) ||
		(nScreenWidthMax - nScreenWidthMin) < 16 || (nScreenHeightMax - nScreenHeightMin) < 16)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render16x16Tile_Prio_Mask_FlipXY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render16x16Tile_Prio_Mask_FlipY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render16x16Tile_Prio_Mask_FlipX_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render16x16Tile_Prio_Mask_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render16x16Tile_Prio_Mask_FlipXY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render16x16Tile_Prio_Mask_FlipY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render16x16Tile_Prio_Mask_FlipX(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render16x16Tile_Prio_Mask(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
	}
}

void Draw32x32Tile(UINT16 *pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - 32) || StartY <= (nScreenHeightMin - 32) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - 32) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - 32) ||
		(nScreenWidthMax - nScreenWidthMin) < 32 || (nScreenHeightMax - nScreenHeightMin) < 32)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render32x32Tile_FlipXY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				Render32x32Tile_FlipY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render32x32Tile_FlipX_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				Render32x32Tile_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render32x32Tile_FlipXY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				Render32x32Tile_FlipY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render32x32Tile_FlipX(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				Render32x32Tile(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
	}
}

void Draw32x32MaskTile(UINT16 *pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - 32) || StartY <= (nScreenHeightMin - 32) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - 32) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - 32) ||
		(nScreenWidthMax - nScreenWidthMin) < 32 || (nScreenHeightMax - nScreenHeightMin) < 32)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render32x32Tile_Mask_FlipXY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				Render32x32Tile_Mask_FlipY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render32x32Tile_Mask_FlipX_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				Render32x32Tile_Mask_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render32x32Tile_Mask_FlipXY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				Render32x32Tile_Mask_FlipY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render32x32Tile_Mask_FlipX(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				Render32x32Tile_Mask(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
	}
}

void Draw32x32PrioTile(UINT16 *pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - 32) || StartY <= (nScreenHeightMin - 32) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - 32) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - 32) ||
		(nScreenWidthMax - nScreenWidthMin) < 32 || (nScreenHeightMax - nScreenHeightMin) < 32)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render32x32Tile_Prio_FlipXY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render32x32Tile_Prio_FlipY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render32x32Tile_Prio_FlipX_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render32x32Tile_Prio_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render32x32Tile_Prio_FlipXY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render32x32Tile_Prio_FlipY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render32x32Tile_Prio_FlipX(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render32x32Tile_Prio(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
	}
}

void Draw32x32PrioMaskTile(UINT16 *pDestDraw, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - 32) || StartY <= (nScreenHeightMin - 32) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - 32) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - 32) ||
		(nScreenWidthMax - nScreenWidthMin) < 32 || (nScreenHeightMax - nScreenHeightMin) < 32)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render32x32Tile_Prio_Mask_FlipXY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render32x32Tile_Prio_Mask_FlipY_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render32x32Tile_Prio_Mask_FlipX_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render32x32Tile_Prio_Mask_Clip(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				Render32x32Tile_Prio_Mask_FlipXY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render32x32Tile_Prio_Mask_FlipY(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				Render32x32Tile_Prio_Mask_FlipX(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				Render32x32Tile_Prio_Mask(pDestDraw, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
	}
}

void DrawCustomTile(UINT16 *pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - nWidth) || StartY <= (nScreenHeightMin - nHeight) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - nWidth) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - nHeight) ||
		(nScreenWidthMax - nScreenWidthMin) < nWidth || (nScreenHeightMax - nScreenHeightMin) < nHeight)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				RenderCustomTile_FlipXY_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				RenderCustomTile_FlipY_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				RenderCustomTile_FlipX_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				RenderCustomTile_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				RenderCustomTile_FlipXY(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				RenderCustomTile_FlipY(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				RenderCustomTile_FlipX(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
			else
			{
				RenderCustomTile(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, pTile);
			}
		}
	}
}

void DrawCustomMaskTile(UINT16 *pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - nWidth) || StartY <= (nScreenHeightMin - nHeight) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - nWidth) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - nHeight) ||
		(nScreenWidthMax - nScreenWidthMin) < nWidth || (nScreenHeightMax - nScreenHeightMin) < nHeight)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				RenderCustomTile_Mask_FlipXY_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				RenderCustomTile_Mask_FlipY_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				RenderCustomTile_Mask_FlipX_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				RenderCustomTile_Mask_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				RenderCustomTile_Mask_FlipXY(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				RenderCustomTile_Mask_FlipY(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				RenderCustomTile_Mask_FlipX(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
			else
			{
				RenderCustomTile_Mask(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, pTile);
			}
		}
	}
}

void DrawCustomPrioTile(UINT16 *pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - nWidth) || StartY <= (nScreenHeightMin - nHeight) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - nWidth) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - nHeight) ||
		(nScreenWidthMax - nScreenWidthMin) < nWidth || (nScreenHeightMax - nScreenHeightMin) < nHeight)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				RenderCustomTile_Prio_FlipXY_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				RenderCustomTile_Prio_FlipY_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				RenderCustomTile_Prio_FlipX_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				RenderCustomTile_Prio_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				RenderCustomTile_Prio_FlipXY(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				RenderCustomTile_Prio_FlipY(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				RenderCustomTile_Prio_FlipX(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				RenderCustomTile_Prio(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nPaletteOffset, nPriority, pTile);
			}
		}
	}
}

void DrawCustomPrioMaskTile(UINT16 *pDestDraw, INT32 nWidth, INT32 nHeight, INT32 nTileNumber, INT32 StartX, INT32 StartY, INT32 FlipX, INT32 FlipY, INT32 nTilePalette, INT32 nColourDepth, INT32 nMaskColour, INT32 nPaletteOffset, INT32 nPriority, UINT8 *pTile)
{
	if (StartX <= (nScreenWidthMin - nWidth) || StartY <= (nScreenHeightMin - nHeight) || StartX >= nScreenWidthMax || StartY >= nScreenHeightMax)
	{
		return;
	}

	if (StartX < nScreenWidthMin || StartX > (nScreenWidthMax - nWidth) || StartY < nScreenHeightMin || StartY > (nScreenHeightMax - nHeight) ||
		(nScreenWidthMax - nScreenWidthMin) < nWidth || (nScreenHeightMax - nScreenHeightMin) < nHeight)
	{
		if (FlipY)
		{
			if (FlipX)
			{
				RenderCustomTile_Prio_Mask_FlipXY_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				RenderCustomTile_Prio_Mask_FlipY_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				RenderCustomTile_Prio_Mask_FlipX_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				RenderCustomTile_Prio_Mask_Clip(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
	}
	else
	{
		if (FlipY)
		{
			if (FlipX)
			{
				RenderCustomTile_Prio_Mask_FlipXY(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				RenderCustomTile_Prio_Mask_FlipY(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
		else
		{
			if (FlipX)
			{
				RenderCustomTile_Prio_Mask_FlipX(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
			else
			{
				RenderCustomTile_Prio_Mask(pDestDraw, nWidth, nHeight, nTileNumber, StartX, StartY, nTilePalette, nColourDepth, nMaskColour, nPaletteOffset, nPriority, pTile);
			}
		}
	}
}

void DrawGfxTile(INT32 nBitmap, INT32 nGfx, INT32 nTileNumber, INT32 nStartX, INT32 nStartY, INT32 nFlipx, INT32 nFlipy, INT32 nTilePalette)
{
	UINT16 *bitmap;
	if (nBitmap != 0)
	{
		bitmap = BurnBitmapGetBitmap(nBitmap);
		clip_struct *clip = BurnBitmapClipDims(nBitmap);
		BurnBitmapGetDimensions(nBitmap, &nScreenWidth, &nScreenHeight);

		GenericTilesSetClipRaw(clip->nMinx, clip->nMaxx, clip->nMiny, clip->nMaxy);
	}
	else
	{
		bitmap = pTransDraw;
	}

	GenericTilesGfx *gfx = &GenericGfxData[nGfx];

	DrawCustomTile(bitmap, gfx->width, gfx->height, nTileNumber % gfx->code_mask, nStartX, nStartY, nFlipx, nFlipy, nTilePalette & gfx->color_mask, gfx->depth, gfx->color_offset, gfx->gfxbase);

	if (nBitmap != 0)
	{
		GenericTilesClearClipRaw();
	}
}

void DrawGfxMaskTile(INT32 nBitmap, INT32 nGfx, INT32 nTileNumber, INT32 nStartX, INT32 nStartY, INT32 nFlipx, INT32 nFlipy, INT32 nTilePalette, INT32 nMaskColor)
{
	UINT16 *bitmap;
	if (nBitmap != 0)
	{
		bitmap = BurnBitmapGetBitmap(nBitmap);
		clip_struct *clip = BurnBitmapClipDims(nBitmap);
		BurnBitmapGetDimensions(nBitmap, &nScreenWidth, &nScreenHeight);

		GenericTilesSetClipRaw(clip->nMinx, clip->nMaxx, clip->nMiny, clip->nMaxy);
	}
	else
	{
		bitmap = pTransDraw;
	}

	GenericTilesGfx *gfx = &GenericGfxData[nGfx];

	DrawCustomMaskTile(bitmap, gfx->width, gfx->height, nTileNumber % gfx->code_mask, nStartX, nStartY, nFlipx, nFlipy, nTilePalette & gfx->color_mask, gfx->depth, nMaskColor, gfx->color_offset, gfx->gfxbase);

	if (nBitmap != 0)
	{
		GenericTilesClearClipRaw();
	}
}

void DrawGfxPrioTile(INT32 nBitmap, INT32 nGfx, INT32 nTileNumber, INT32 nStartX, INT32 nStartY, INT32 nFlipx, INT32 nFlipy, INT32 nTilePalette, INT32 nPriority)
{
	UINT16 *bitmap;
	if (nBitmap != 0)
	{
		bitmap = BurnBitmapGetBitmap(nBitmap);
		pPrioDraw = BurnBitmapGetPriomap(nBitmap);
		clip_struct *clip = BurnBitmapClipDims(nBitmap);
		BurnBitmapGetDimensions(nBitmap, &nScreenWidth, &nScreenHeight);

		GenericTilesSetClipRaw(clip->nMinx, clip->nMaxx, clip->nMiny, clip->nMaxy);
	}
	else
	{
		bitmap = pTransDraw;
	}

	GenericTilesGfx *gfx = &GenericGfxData[nGfx];

	DrawCustomPrioTile(bitmap, gfx->width, gfx->height, nTileNumber % gfx->code_mask, nStartX, nStartY, nFlipx, nFlipy, nTilePalette & gfx->color_mask, gfx->depth, gfx->color_offset, nPriority, gfx->gfxbase);

	if (nBitmap != 0)
	{
		pPrioDraw = BurnBitmapGetPriomap(0);
		GenericTilesClearClipRaw();
	}
}

void DrawGfxPrioMaskTile(INT32 nBitmap, INT32 nGfx, INT32 nTileNumber, INT32 nStartX, INT32 nStartY, INT32 nFlipx, INT32 nFlipy, INT32 nTilePalette, INT32 nMaskColor, INT32 nPriority)
{
	UINT16 *bitmap;
	if (nBitmap != 0)
	{
		bitmap = BurnBitmapGetBitmap(nBitmap);
		pPrioDraw = BurnBitmapGetPriomap(nBitmap);
		clip_struct *clip = BurnBitmapClipDims(nBitmap);
		BurnBitmapGetDimensions(nBitmap, &nScreenWidth, &nScreenHeight);

		GenericTilesSetClipRaw(clip->nMinx, clip->nMaxx, clip->nMiny, clip->nMaxy);
	}
	else
	{
		bitmap = pTransDraw;
	}

	GenericTilesGfx *gfx = &GenericGfxData[nGfx];

	DrawCustomPrioMaskTile(bitmap, gfx->width, gfx->height, nTileNumber % gfx->code_mask, nStartX, nStartY, nFlipx, nFlipy, nTilePalette & gfx->color_mask, gfx->depth, nMaskColor, gfx->color_offset, nPriority, gfx->gfxbase);

	if (nBitmap != 0)
	{
		pPrioDraw = BurnBitmapGetPriomap(0);
		GenericTilesClearClipRaw();
	}
}

/*================================================================================================
Zoomed Tile Functions
================================================================================================*/

void RenderZoomedTile(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderZoomedTile called without init\n"));
#endif

	// Based on MAME sources for tile zooming
	UINT8 *gfx_base = gfx + (code * width * height);
	int dh = (zoomy * height + 0x8000) / 0x10000;
	int dw = (zoomx * width + 0x8000) / 0x10000;

	if (dw && dh)
	{
		int dx = (width * 0x10000) / dw;
		int dy = (height * 0x10000) / dh;
		int ex = sx + dw;
		int ey = sy + dh;
		int x_index_base = 0;
		int y_index = 0;

		if (fx) {
			x_index_base = (dw - 1) * dx;
			dx = -dx;
		}

		if (fy) {
			y_index = (dh - 1) * dy;
			dy = -dy;
		}

		for (INT32 y = sy; y < ey; y++)
		{
			UINT8 *src = gfx_base + (y_index / 0x10000) * width;
			UINT16 *dst = dest + y * nScreenWidth;

			if (y >= nScreenHeightMin && y < nScreenHeightMax)
			{
				for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
				{
					if (x >= nScreenWidthMin && x < nScreenWidthMax) {
						INT32 pxl = src[x_index>>16];

						if (pxl != t)
							dst[x] = pxl + color;
					}

					x_index += dx;
				}
			}

			y_index += dy;
		}
	}
}

void RenderZoomedPrioTile(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, INT32 priority)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderZoomedPrioTile called without init\n"));
#endif

	// Based on MAME sources for tile zooming
	UINT8 *gfx_base = gfx + (code * width * height);
	int dh = (zoomy * height + 0x8000) / 0x10000;
	int dw = (zoomx * width + 0x8000) / 0x10000;

	if (dw && dh)
	{
		int dx = (width * 0x10000) / dw;
		int dy = (height * 0x10000) / dh;
		int ex = sx + dw;
		int ey = sy + dh;
		int x_index_base = 0;
		int y_index = 0;

		if (fx) {
			x_index_base = (dw - 1) * dx;
			dx = -dx;
		}

		if (fy) {
			y_index = (dh - 1) * dy;
			dy = -dy;
		}

		for (INT32 y = sy; y < ey; y++)
		{
			UINT8 *src = gfx_base + (y_index / 0x10000) * width;
			UINT16 *dst = dest + y * nScreenWidth;
			UINT8 *pri = pPrioDraw + y * nScreenWidth;

			if (y >= nScreenHeightMin && y < nScreenHeightMax)
			{
				for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
				{
					if (x >= nScreenWidthMin && x < nScreenWidthMax) {
						INT32 pxl = src[x_index>>16];

						if (pxl != t) {
							dst[x] = pxl + color;
							pri[x] = priority;
						}
					}

					x_index += dx;
				}
			}

			y_index += dy;
		}
	}
}

/*================================================================================================
Tile with Transparency Table Functions
================================================================================================*/

void RenderTileTranstabOffset(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *tab, UINT32 color_offset)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderTileTranstab called without init\n"));
#endif

	INT32 flip = 0;
	if (flipy) flip |= (height - 1) * width;
	if (flipx) flip |= width - 1;

	gfx += code * width * height;

	for (INT32 y = 0; y < height; y++, sy++) {
		if (sy < nScreenHeightMin || sy >= nScreenHeightMax) continue;

		for (INT32 x = 0; x < width; x++, sx++) {
			if (sx < nScreenWidthMin || sx >= nScreenWidthMax) continue;

			INT32 pxl = gfx[((y * width) + x) ^ flip] | color;

			if (tab[pxl] == trans_col) continue;

			dest[sy * nScreenWidth + sx] = pxl + color_offset;
		}

		sx -= width;
	}
}

void RenderTileTranstab(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *tab)
{
	RenderTileTranstabOffset(dest, gfx, code, color, trans_col, sx, sy, flipx, flipy, width, height, tab, 0x00);
}

void RenderTilePrioTranstabOffset(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *tab, UINT32 color_offset, INT32 priority)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderTilePrioTranstab called without init\n"));
#endif

	INT32 flip = 0;
	if (flipy) flip |= (height - 1) * width;
	if (flipx) flip |= width - 1;

	gfx += code * width * height;

	for (INT32 y = 0; y < height; y++, sy++) {
		if (sy < nScreenHeightMin || sy >= nScreenHeightMax) continue;

		for (INT32 x = 0; x < width; x++, sx++) {
			if (sx < nScreenWidthMin || sx >= nScreenWidthMax) continue;

			INT32 pxl = gfx[((y * width) + x) ^ flip] | color;

			if (tab[pxl] == trans_col) continue;

			dest[sy * nScreenWidth + sx] = pxl + color_offset;
			pPrioDraw[sy * nScreenWidth + sx] = priority;
		}

		sx -= width;
	}
}

void RenderTilePrioTranstab(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *tab, INT32 priority)
{
	RenderTilePrioTranstabOffset(dest, gfx, code, color, trans_col, sx, sy, flipx, flipy, width, height, tab, 0x00, priority);
}

void RenderPrioMaskTranstabSpriteOffset(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *tab, UINT32 color_offset, UINT32 priority)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderPrioMaskTranstabSprite called without init\n"));
#endif

	INT32 flip = 0;
	if (flipy) flip |= (height - 1) * width;
	if (flipx) flip |= width - 1;

	priority |= 1<<31;

	gfx += code * width * height;

	for (INT32 y = 0; y < height; y++, sy++) {
		if (sy < nScreenHeightMin || sy >= nScreenHeightMax) continue;

		for (INT32 x = 0; x < width; x++, sx++) {
			if (sx < nScreenWidthMin || sx >= nScreenWidthMax) continue;

			INT32 pxl = gfx[((y * width) + x) ^ flip] | color;

			if (tab[pxl] == trans_col) continue;

			if ((priority & (1 << pPrioDraw[sy * nScreenWidth + sx])) == 0) {
				dest[sy * nScreenWidth + sx] = pxl + color_offset;
			}
            pPrioDraw[sy * nScreenWidth + sx] = 0x1f;
		}

		sx -= width;
	}
}

void RenderPrioMaskTranstabSprite(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 trans_col, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, UINT8 *tab, UINT32 priority)
{
	RenderPrioMaskTranstabSpriteOffset(dest, gfx, code, color, trans_col, sx, sy, flipx, flipy, width, height, tab, 0x00, priority);
}

/*================================================================================================
Sprite tile with priorities
================================================================================================*/

void RenderZoomedPrioSprite(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, INT32 priority)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderZoomedPrioSprite called without init\n"));
#endif

	// Based on MAME sources for tile zooming
	UINT8 *gfx_base = gfx + (code * width * height);
	int dh = (zoomy * height + 0x8000) / 0x10000;
	int dw = (zoomx * width + 0x8000) / 0x10000;

	priority |= 1 << 31;

	if (dw && dh)
	{
		int dx = (width * 0x10000) / dw;
		int dy = (height * 0x10000) / dh;
		int ex = sx + dw;
		int ey = sy + dh;
		int x_index_base = 0;
		int y_index = 0;

		if (fx) {
			x_index_base = (dw - 1) * dx;
			dx = -dx;
		}

		if (fy) {
			y_index = (dh - 1) * dy;
			dy = -dy;
		}

		for (INT32 y = sy; y < ey; y++)
		{
			UINT8 *src = gfx_base + (y_index / 0x10000) * width;
			UINT16 *dst = dest + y * nScreenWidth;
			UINT8 *pri = pPrioDraw + y * nScreenWidth;

			if (y >= nScreenHeightMin && y < nScreenHeightMax)
			{
				for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
				{
					if (x >= nScreenWidthMin && x < nScreenWidthMax) {
						INT32 pxl = src[x_index>>16];

						if (pxl != t) {
							if ((priority & (1 << pri[x])) == 0) {
								dst[x] = pxl + color;
							}
							pri[x] = 0x1f;
						}
					}

					x_index += dx;
				}
			}

			y_index += dy;
		}
	}
}



void RenderPrioSprite(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 priority)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderPrioSprite called without init\n"));
#endif

	if (sx < (nScreenWidthMin - (width - 1)) || sy < (nScreenHeightMin - (height - 1)) || sx >= nScreenWidthMax || sy >= nScreenHeightMax) return;

	UINT8 *gfx_base = gfx + (code * width * height);

	priority |= 1 << 31;

	INT32 flipx = fx ? (width - 1) : 0;
	INT32 flipy = fy ? (height - 1) : 0;

	for (INT32 y = 0; y < height; y++, sy++)
	{
		if (sy < nScreenHeightMin || sy >= nScreenHeightMax) continue;

		UINT16 *dst = dest + sy * nScreenWidth + sx;
		UINT8 *pri = pPrioDraw + sy * nScreenWidth + sx;

		for (INT32 x = 0; x < width; x++, sx++)
		{
			if (sx < nScreenWidthMin || sx >= nScreenWidthMax) continue;

			INT32 pxl = gfx_base[(y ^ flipy) * width + (x ^ flipx)];

			if (pxl != t) {
				if ((priority & (1 << pri[x])) == 0) {
					dst[x] = pxl + color;
				}
				pri[x] = 0x1f;
			}
		}

		sx -= width;
	}
}

void RenderZoomedPrioTranstabSpriteOffset(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, UINT8 *tab, UINT32 color_offset, INT32 priority)
{
#if defined FBNEO_DEBUG
	if (!Debug_GenericTilesInitted) bprintf(PRINT_ERROR, _T("RenderZoomedPrioSprite called without init\n"));
#endif

	// Based on MAME sources for tile zooming
	UINT8 *gfx_base = gfx + (code * width * height);
	int dh = (zoomy * height + 0x8000) / 0x10000;
	int dw = (zoomx * width + 0x8000) / 0x10000;

	priority |= 1 << 31;

	if (dw && dh)
	{
		int dx = (width * 0x10000) / dw;
		int dy = (height * 0x10000) / dh;
		int ex = sx + dw;
		int ey = sy + dh;
		int x_index_base = 0;
		int y_index = 0;

		if (fx) {
			x_index_base = (dw - 1) * dx;
			dx = -dx;
		}

		if (fy) {
			y_index = (dh - 1) * dy;
			dy = -dy;
		}

		for (INT32 y = sy; y < ey; y++)
		{
			UINT8 *src = gfx_base + (y_index / 0x10000) * width;
			UINT16 *dst = dest + y * nScreenWidth;
			UINT8 *pri = pPrioDraw + y * nScreenWidth;

			if (y >= nScreenHeightMin && y < nScreenHeightMax)
			{
				for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
				{
					if (x >= nScreenWidthMin && x < nScreenWidthMax) {
						INT32 pxl = src[x_index>>16] + color;

						if (tab[pxl] != t) {
							if ((priority & (1 << pri[x])) == 0) {
								dst[x] = pxl + color_offset;
							}
							pri[x] = 0x1f;
						}
					}

					x_index += dx;
				}
			}

			y_index += dy;
		}
	}
}

void RenderZoomedPrioTranstabSprite(UINT16 *dest, UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, UINT8 *tab, INT32 priority)
{
	RenderZoomedPrioTranstabSpriteOffset(dest, gfx, code, color, t, sx, sy, fx, fy, width, height, zoomx, zoomy, tab, 0x00, priority);
}

