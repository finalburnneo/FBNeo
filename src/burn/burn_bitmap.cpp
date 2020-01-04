// FB Alpha Bitmap Management System

#include "tiles_generic.h"

#define MAX_BITMAPS			32			// maximum number of 

#define FLAG_INITIALIZED	0x01		// flag to ensure this bitmap has been initialized
#define FLAG_HAS_PRIMAP		0x02		// flat to mark this bitmap has a priority map

static struct bitmap_struct bitmaps[MAX_BITMAPS];

// allocate bitmap and optionally allocate priority map - memory allicated is nWidth * nHeight * sizeof(UINT16)
void BurnBitmapAllocate(INT32 nBitmapNumber, INT32 nWidth, INT32 nHeight, bool use_primap)
{
#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if (nBitmapNumber >= MAX_BITMAPS) {
		bprintf (0, _T("BurnBitmapAllocate(UINT16 **, INT32, INT32) too many bitmaps allocated %d, max: (^d)\n"), nBitmapNumber, MAX_BITMAPS);
		return;
	}

	// verify width and height are sane numbers
	if (nWidth > 0x10000 || nHeight > 0x10000) {
		bprintf (0, _T("BurnBitmapAllocate(UINT16 **, INT32, INT32) (%d) has extremely large bitmap size. Width %d, Height: %d\n"), nBitmapNumber, nWidth, nHeight);
	}
#endif

	bitmap_struct *ptr = &bitmaps[nBitmapNumber];

#if defined FBNEO_DEBUG
	// verify this entry in the struct hasn't been initialized already!
	if (ptr->nFlags & FLAG_INITIALIZED)
	{
		bprintf (0, _T("BurnBitmapAllocate(UINT16 **, INT32, INT32) attempting to allocate bitmap number that is already allocated (%d)!\n"), nBitmapNumber);
		return;
	}
#endif

	// allocate bitmap
	ptr->pBitmap = (UINT16 *)BurnMalloc(nWidth * nHeight * sizeof(UINT16));

	// allocate priority bitmap
	if (use_primap) {
		ptr->pPrimap = (UINT8 *)BurnMalloc(nWidth * nHeight * sizeof(UINT8));
	}

	// set flags - always set initialized, only set primap if primap is allocated
	ptr->nFlags = FLAG_INITIALIZED | (use_primap ? FLAG_HAS_PRIMAP : 0);

	// set width and height
	ptr->nWidth = nWidth;
	ptr->nHeight = nHeight;

	// set default clip dimensions (full bitmap size)
	ptr->clipdims.nMinx = 0;
	ptr->clipdims.nMaxx = nWidth;
	ptr->clipdims.nMiny = 0;
	ptr->clipdims.nMaxy = nHeight;
}

// get pointer to clip dimensions
clip_struct *BurnBitmapClipDims(INT32 nBitmapNumber)
{
#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if (nBitmapNumber >= MAX_BITMAPS) {
		bprintf (0, _T("BurnBitmapClipdims(%d) called with invalid bitmap number. Max (%d)\n"), nBitmapNumber, MAX_BITMAPS); 
		return NULL;
	}
#endif

	bitmap_struct *ptr = &bitmaps[nBitmapNumber];

#if defined FBNEO_DEBUG
	// verify this entry in the struct has been initialized!
	if ((ptr->nFlags & FLAG_INITIALIZED) == 0) {
		bprintf (0, _T("BurnBitmapClipDims(%d) called without itialized bitmap!\n"), nBitmapNumber); 
		return NULL;
	}
#endif

	return &ptr->clipdims;
}

// set clip dimensions
void BurnBitmapSetClipDims(INT32 nBitmapNumber, INT32 nMinx, INT32 nMaxx, INT32 nMiny, INT32 nMaxy)
{
#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if (nBitmapNumber >= MAX_BITMAPS) {
		bprintf (0, _T("BurnBitmapSetClipDims(%d, INT32, INT32, INT32, INT32) called with invalid bitmap number. Max (%d)\n"), nBitmapNumber, MAX_BITMAPS); 
		return;
	}
#endif

	bitmap_struct *ptr = &bitmaps[nBitmapNumber];

#if defined FBNEO_DEBUG
	// verify this entry in the struct has been initialized!
	if ((ptr->nFlags & FLAG_INITIALIZED) == 0) {
		bprintf (0, _T("BurnBitmapSetClipDims(%d, INT32, INT32, INT32, INT32) called without itialized bitmap!\n"), nBitmapNumber); 
		return;
	}
#endif

	// make sure the clips are within the bounds of the bitmap
	if (nMinx < 0) nMinx = 0;
	if (nMaxx >= ptr->nWidth) nMaxx = ptr->nWidth;
	if (nMiny < 0) nMiny = 0;
	if (nMaxy >= ptr->nHeight) nMaxy = ptr->nHeight;

	ptr->clipdims.nMinx = nMinx;
	ptr->clipdims.nMaxx = nMaxx;
	ptr->clipdims.nMiny = nMiny;
	ptr->clipdims.nMaxy = nMaxy;
}

// get bitmap clips
void BurnBitmapGetClipDims(INT32 nBitmapNumber, INT32 *nMinx, INT32 *nMaxx, INT32 *nMiny, INT32 *nMaxy)
{
#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if (nBitmapNumber >= MAX_BITMAPS) {
		bprintf (0, _T("BurnBitmapGetClipDims(%d, INT32*, INT32*, INT32* INT32*) called with invalid bitmap number. Max (%d)\n"), nBitmapNumber, MAX_BITMAPS); 
		return;
	}
#endif

	bitmap_struct *ptr = &bitmaps[nBitmapNumber];

#if defined FBNEO_DEBUG
	// verify this entry in the struct has been initialized!
	if ((ptr->nFlags & FLAG_INITIALIZED) == 0) {
		bprintf (0, _T("BurnBitmapGetClipDims(%d, INT32*, INT32*, INT32* INT32*) called without itialized bitmap!\n"), nBitmapNumber); 
		return;
	}
#endif

	*nMinx = ptr->clipdims.nMinx;
	*nMaxx = ptr->clipdims.nMaxx;
	*nMiny = ptr->clipdims.nMiny;
	*nMaxy = ptr->clipdims.nMaxy;
}

// get pointer to bitmap memory
UINT16 *BurnBitmapGetBitmap(INT32 nBitmapNumber)
{
#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if (nBitmapNumber >= MAX_BITMAPS) {
		bprintf (0, _T("BurnBitmapGetBitmap(%d) called with invalid bitmap number. Max (%d)\n"), nBitmapNumber, MAX_BITMAPS); 
		return NULL;
	}
#endif

	bitmap_struct *ptr = &bitmaps[nBitmapNumber];

#if defined FBNEO_DEBUG
	// verify this entry in the struct has been initialized!
	if ((ptr->nFlags & FLAG_INITIALIZED) == 0) {
		bprintf (0, _T("BurnBitmapGetBitmap(%d) called without itialized bitmap!\n"), nBitmapNumber); 
		return NULL;
	}
#endif

	// return pointer to bitmap
	return ptr->pBitmap;
}

// get pointer to priority map memory
UINT8 *BurnBitmapGetPriomap(INT32 nBitmapNumber)
{
#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if (nBitmapNumber >= MAX_BITMAPS) {
		bprintf (0, _T("BurnBitmapGetPriomap(%d) called with invalid bitmap number. Max (%d)\n"), nBitmapNumber, MAX_BITMAPS); 
		return NULL;
	}
#endif

	bitmap_struct *ptr = &bitmaps[nBitmapNumber];

#if defined FBNEO_DEBUG
	// verify this bitmap has a priority map
	if ((ptr->nFlags & FLAG_HAS_PRIMAP) == 0) {
		bprintf (0, _T("BurnBitmapGetPriomap(%d) called without initialized Primap!\n"), nBitmapNumber);
		return NULL;
	}
#endif

	// return pointer to priority map
	return ptr->pPrimap;
}

// get dimensions of the bitmap
void BurnBitmapGetDimensions(INT32 nBitmapNumber, INT32 *nWidth, INT32 *nHeight)
{
#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if (nBitmapNumber >= MAX_BITMAPS) {
		bprintf (0, _T("BurnBitmapGetDimensions(%d, INT32, INT32) called with invalid bitmap number. Max (%d)\n"), nBitmapNumber, MAX_BITMAPS); 
		return;
	}
#endif

	bitmap_struct *ptr = &bitmaps[nBitmapNumber];

#if defined FBNEO_DEBUG
	// verify this entry in the struct has been initialized!
	if ((ptr->nFlags & FLAG_INITIALIZED) == 0) {
		bprintf (0, _T("BurnBitmapGetDimensions(%d, INT32 *, INT32 *) called without itialized bitmap!\n"), nBitmapNumber); 
		return;
	}
#endif

	// pass width and height into variables
	*nWidth = ptr->nWidth;
	*nHeight = ptr->nHeight;
}

// get bitmap pointer at a set position, has bounds checking
UINT16 *BurnBitmapGetPosition(INT32 nBitmapNumber, INT32 nStartX, INT32 nStartY)
{
#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if (nBitmapNumber >= MAX_BITMAPS) {
		bprintf (0, _T("BurnBitmapGetPosition(%d, INT32, INT32) called with invalid bitmap number. Max (%d)\n"), nBitmapNumber, MAX_BITMAPS); 
		return NULL;
	}
#endif

	bitmap_struct *ptr = &bitmaps[nBitmapNumber];

#if defined FBNEO_DEBUG
	// verify this entry in the struct has been initialized!
	if ((ptr->nFlags & FLAG_INITIALIZED) == 0) {
		bprintf (0, _T("BurnBitmapGetPosition(%d, INT32, INT32) called without itialized bitmap!\n"), nBitmapNumber); 
		return NULL;
	}
#endif

	// keep position within bounds
	nStartY %= ptr->nHeight;
	nStartX %= ptr->nWidth;

	// return pointer to bitmap + position
	return ptr->pBitmap + (nStartY * ptr->nWidth) + nStartX;
}

// get bitmap pointer at a set position, has bounds checking
UINT8 *BurnBitmapGetPrimapPosition(INT32 nBitmapNumber, INT32 nStartX, INT32 nStartY)
{
#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if (nBitmapNumber >= MAX_BITMAPS) {
		bprintf (0, _T("BurnBitmapGetPrimapPosition(%d, INT32, INT32) called with invalid bitmap number. Max (%d)\n"), nBitmapNumber, MAX_BITMAPS); 
		return NULL;
	}
#endif

	bitmap_struct *ptr = &bitmaps[nBitmapNumber];

#if defined FBNEO_DEBUG
	// verify this bitmap has a priority map
	if ((ptr->nFlags & FLAG_HAS_PRIMAP) == 0) {
		bprintf (0, _T("BurnBitmapGetPrimapPosition(%d, INT32, INT32) called without initialized Primap!\n"), nBitmapNumber);
		return NULL;
	}
#endif

	// keep position within bounds
	nStartY %= ptr->nHeight;
	nStartX %= ptr->nWidth;

	// return pointer to priority bitmap + position
	return ptr->pPrimap + (nStartY * ptr->nWidth) + nStartX;
}

// fill bitmap with a value (use for clearing)
void BurnBitmapFill(INT32 nBitmapNumber, INT32 nFillValue)
{
#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if (nBitmapNumber >= MAX_BITMAPS) {
		bprintf (0, _T("BurnBitmapFill(%d, INT32) called with invalid bitmap number. Max (%d)\n"), nBitmapNumber, MAX_BITMAPS); 
		return;
	}
#endif

	bitmap_struct *ptr = &bitmaps[nBitmapNumber];

#if defined FBNEO_DEBUG
	if ((ptr->nFlags & FLAG_INITIALIZED) == 0) {
		bprintf (0, _T("BurnBitmapFill(%d, INT32) called without itialized bitmap!\n"), nBitmapNumber); 
		return;
	}
#endif

	for (INT32 i = 0; i < ptr->nWidth * ptr->nHeight; i++)
	{
		ptr->pBitmap[i] = nFillValue;
	}
}

// zero-fill priority bitmap
void BurnBitmapPrimapClear(INT32 nBitmapNumber)
{
#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if (nBitmapNumber >= MAX_BITMAPS) {
		bprintf (0, _T("BurnBitmapPrimapClear(%d) called with invalid bitmap number. Max (%d)\n"), nBitmapNumber, MAX_BITMAPS); 
		return;
	}
#endif

	bitmap_struct *ptr = &bitmaps[nBitmapNumber];

#if defined FBNEO_DEBUG
	if ((ptr->nFlags & FLAG_HAS_PRIMAP) == 0) {
		bprintf (0, _T("BurnBitmapPrimapClear(%d) called without initialized Primap!\n"), nBitmapNumber);
		return;
	}
#endif

	memset (ptr->pPrimap, 0, ptr->nWidth * ptr->nHeight);
}

// copy bitmap with scrolling and transparency
void BurnBitmapCopy(INT32 nBitmapNumber, UINT16 *pDest, UINT8 *pPrio, INT32 nScrollX, INT32 nScrollY, INT32 nPixelMask, INT32 nTransColor)
{
#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if (nBitmapNumber >= MAX_BITMAPS) {
		bprintf (0, _T("BurnBitmapCopy(%d, ...) called with invalid bitmap number. Max (%d)\n"), nBitmapNumber, MAX_BITMAPS); 
		return;
	}
#endif

	bitmap_struct *ptr = &bitmaps[nBitmapNumber];

#if defined FBNEO_DEBUG
	// verify nBitmapNumber is a sane number
	if ((ptr->nFlags & FLAG_INITIALIZED) == 0) {
		bprintf (0, _T("BurnBitmapFill(%d, INT32) called without itialized bitmap!\n"), nBitmapNumber); 
		return;
	}

	if (((ptr->nFlags & FLAG_HAS_PRIMAP) == 0) && pPrio != NULL) {
		bprintf (0, _T("BurnBitmapCopy(%d) called without initialized Primap!\n"), nBitmapNumber);
		return;
	}
#endif

	// get clipping dimensions
	INT32 minx = 0, maxx = nScreenWidth, miny = 0, maxy = nScreenHeight;
	GenericTilesSetClip(minx, maxx, miny, maxy);

	// does this have a priority map?
	INT32 use_prio = (pPrio != NULL) && (ptr->pPrimap != NULL);

	for (INT32 sy = miny; sy < maxy; sy++) // height
	{
		// get pointers to current screen vertical offset
		UINT16 *pSrc = BurnBitmapGetPosition(nBitmapNumber, 0, sy + nScrollY);
		UINT8 *pPri = (use_prio ? BurnBitmapGetPrimapPosition(nBitmapNumber, 0, sy + nScrollY) : NULL);

		for (INT32 sx = minx; sx < maxx; sx++) // width
		{
			UINT16 c = pSrc[(sx + nScrollX) % ptr->nWidth]; // get pixel (pixel + color)

			if (nTransColor == -1) // no transparency
			{
				pDest[sx] = c;	// destination bitmap set to pixel

				if (use_prio) { // destination priority map set to source priority
					pPrio[sx] = pPri[sx];
				}
			}
			else
			{
				if ((c & nPixelMask) != nTransColor) // mask pixel to remove color, check pixel against transparent entry
				{
					pDest[sx] = c;	// destination bitmap set to pixel

					if (use_prio) { // destination priority map set to source priority
						pPrio[sx] = pPri[sx];
					}
				}
			}
		}

		// advance pixel line (vertical)
		pDest += nScreenWidth;

		if (use_prio) {
			pPrio += nScreenWidth;
		}
	}
}

// exit bitmap manager
void BurnBitmapExit() // Call in GenericTilesExit()
{
	bitmap_struct *ptr = &bitmaps[0];

	// interate through all entries
	for (INT32 i = 0; i < MAX_BITMAPS; i++)
	{
		// checck to see if it is initialized
		if (ptr->nFlags & FLAG_INITIALIZED)
		{
			BurnFree(ptr->pBitmap); // free bitmap

			// check to see if priority map is present and free if it is
			if (ptr->nFlags & FLAG_HAS_PRIMAP) {
				BurnFree(ptr->pPrimap);
			}
		}

		// zero-out all configured data
		memset (ptr, 0, sizeof(bitmap_struct));

		ptr++;
	}
}
