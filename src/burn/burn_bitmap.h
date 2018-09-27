// FB Alpha Bitmap Management System

#ifndef BURNBITMAP

#define BURNBITMAP

struct clip_struct
{
	INT32 nMinx;						// minimum X value
	INT32 nMaxx;						// maximum X value
	INT32 nMiny;						// minimum Y value
	INT32 nMaxy;						// maximum Y value
};

struct bitmap_struct
{
	UINT16 *pBitmap;					// 16-bit indexed bitmap
	UINT8 *pPrimap;						// 8-bit priority map
	INT32 nWidth;						// bitmap width
	INT32 nHeight;						// bitmap height
	UINT8 nFlags;						// flags/settings for bitmap

	clip_struct clipdims;
};


// Get pointer to this bitmap
UINT16 *BurnBitmapGetBitmap(INT32 nBitmapNumber);

// Get pointer to this priority map
UINT8 *BurnBitmapGetPriomap(INT32 nBitmapNumber);

// Get dimensions of this bitmap
void BurnBitmapGetDimensions(INT32 nBitmapNumber, INT32 *nWidth, INT32 *nHeight);

// Get pointer to this bitmap at nStartX and nStartY
UINT16 *BurnBitmapGetPosition(INT32 nBitmapNumber, INT32 nStartX, INT32 nStartY);

// Get pointer to this priority map at nStartX and nStartY
UINT8 *BurnBitmapGetPrimapPosition(INT32 nBitmapNumber, INT32 nStartX, INT32 nStartY);

// Fill this bitmap with nFillValue (clear)
void BurnBitmapFill(INT32 nBitmapNumber, INT32 nFillValue);

// Fill this priority map with 0-value
void BurnBitmapPrimapClear(INT32 nBitmapNumber);

// Get pointer to clip dimensions
clip_struct *BurnBitmapClipDims(INT32 nBitmapNumber);

// Set clip dimensions
void BurnBitmapSetClipDims(INT32 nBitmapNumber, INT32 nMinx, INT32 nMaxx, INT32 nMiny, INT32 nMaxy);

// Get bitmap clips
void BurnBitmapGetClipDims(INT32 nBitmapNumber, INT32 *nMinx, INT32 *nMaxx, INT32 *nMiny, INT32 *nMaxy);

// Copy this bitmap to pDest and priority map to pPrio with scrolling and transparency
// use nPixelMask with nTransColor to set up transparency - if ((nPixel & nPixelMask) == nTransColor) continue;
// Please note that this function respects clipping!
void BurnBitmapCopy(INT32 nBitmapNumber, UINT16 *pDest, UINT8 *pPrio, INT32 nScrollX, INT32 nScrollY, INT32 nPixelMask, INT32 nTransColor);

// Allocate bitmap and priority map (optional)
// Note--NEVER USE BITMAP #0! This is reserved for pTransDraw!!!!
void BurnBitmapAllocate(INT32 nBitmapNumber, INT32 nWidth, INT32 nHeight, bool use_primap);

// Exit bitmap functions and free memory (Call in GenericTilesExit())
void BurnBitmapExit();

#endif

