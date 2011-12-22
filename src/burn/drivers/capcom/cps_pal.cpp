#include "cps.h"
// CPS (palette)

static UINT8* CpsPalSrc = NULL;			// Copy of current input palette

UINT32* CpsPal = NULL;					// Hicolor version of palette
UINT32* CpsObjPal = NULL;

INT32 nLagObjectPalettes;

inline static UINT32 CalcColCPS1(UINT16 a)
{
	INT32 r, g, b, f;
	const INT32 F_OFFSET = 0x0F;

	// Format is FFFF RRRR GGGG BBBB
	f = (a & 0xF000) >> 12;

	r = (a & 0x0F00) >> 4;	  // Red
	r |= r >> 4;
	g = (a & 0x00F0);		  // Green
	g |= g >> 4;
	b = (a & 0x000F);		  // Blue
	b |= b << 4;

	f += F_OFFSET;
	r *= f; r /= F_OFFSET + 0x0F;
	g *= f; g /= F_OFFSET + 0x0F;
	b *= f; b /= F_OFFSET + 0x0F;

	return BurnHighCol(r, g, b, 0);
}

static UINT32 CalcColCPS2(UINT16 a)
{
	INT32 r, g, b, f;
	const INT32 F_OFFSET = 0x0F;

	// Format is FFFF RRRR GGGG BBBB
	f = (a & 0xF000) >> 12;

	r = (a & 0x0F00) >> 4;	  // Red
	r |= r >> 4;
	g = (a & 0x00F0);		  // Green
	g |= g >> 4;
	b = (a & 0x000F);		  // Blue
	b |= b << 4;

	f += F_OFFSET;
	r *= f; r /= F_OFFSET + 0x0F;
	g *= f; g /= F_OFFSET + 0x0F;
	b *= f; b /= F_OFFSET + 0x0F;

	return BurnHighCol(r, g, b, 0);
}

static INT32 CalcAll()
{
	UINT16* ps;

	if (Cps == 2) {
		if (nLagObjectPalettes) {
			ps = (UINT16*)CpsPalSrc + 0x0C00;
			for (INT32 i = 0x0C00; i < 0x0E00; i++, ps++) {
				CpsPal[i ^ 15] = CalcColCPS2(*ps);
			}
			ps = (UINT16*)CpsPalSrc + 0x0200;
			for (INT32 i = 0x0200; i < 0x0800; i++, ps++) {
				CpsPal[i ^ 15] = CalcColCPS2(*ps);
			}

			memcpy(CpsPal + 0x0E00, CpsPal + 0x0C00, 0x0200 << 2);
		} else {
			ps = (UINT16*)CpsPalSrc;
			for (INT32 i = 0x0000; i < 0x0800; i++, ps++) {
				CpsPal[i ^ 15] = CalcColCPS2(*ps);
			}
		}

	} else {
		ps = (UINT16*)CpsPalSrc;
		for (INT32 i = 0x0000; i < 0x0c00; i++, ps++) {
			CpsPal[i ^ 15] = CalcColCPS1(*ps);
		}
	}

	return 0;
}

static void CalcAllStar(INT32 nLayer)
{
	UINT16* ps = (UINT16*)CpsPalSrc;
	INT32 nOffset = 0x0800 + (nLayer << 9);
	
	for (INT32 i = 0; i < 128; i++, ps++) {
		CpsPal[(i + nOffset) ^ 15] = CalcColCPS1(*(ps + nOffset));
	}
}

INT32 CpsPalInit()
{
	INT32 nLen = 0;

	nLen = 0x1000 * sizeof(UINT16);
	CpsPalSrc = (UINT8*)BurnMalloc(nLen);
	if (CpsPalSrc == NULL) {
		return 1;
	}
	memset(CpsPalSrc, 0, nLen);

	// The star layer palettes are at the end of the normal palette, so double the size
	nLen = 0x1000 * sizeof(UINT32);
	CpsPal = (UINT32*)BurnMalloc(nLen);
	if (CpsPal == NULL) {
		return 1;
	}

	// Set CpsPal to initial values
	CalcAll();
	if (CpsStar) {
		CalcAllStar(0);
		CalcAllStar(1);
	}

	if (nLagObjectPalettes) {
		CpsObjPal = CpsPal + 0x0C00;
	} else {
		CpsObjPal = CpsPal;
	}

	return 0;
}

INT32 CpsPalExit()
{
	BurnFree(CpsPal);
	BurnFree(CpsPalSrc);
	return 0;
}

// Update CpsPal with the new palette at pNewPal (length 0x1000 bytes)
INT32 CpsPalUpdate(UINT8* pNewPal, INT32 bRecalcAll)
{
	INT32 i;
	UINT16 *ps, *pn;

	// If we are recalculating the whole palette, just copy to CpsPalSrc
	// and recalculate it all
	if (bRecalcAll) {
		ps = (UINT16*)CpsPalSrc;
		pn = (UINT16*)pNewPal;

		if (nLagObjectPalettes) {
			INT32 nBuffer = 0x0C00 + ((GetCurrentFrame() & 1) << 9);

			memcpy(ps + 0x0200, pn + 0x0200, 0x0600 << 1);
			memcpy(ps + nBuffer, pn, 0x0200 << 1);
			memcpy(ps + 0x0E00, pn, 0x0200 << 1);

			CpsObjPal = CpsPal + nBuffer;
		} else {
			memcpy(ps, pn, 0x0c00 << 1);
		}

		CalcAll();

		return 0;
	}

	if (Cps == 2) {
		if (nLagObjectPalettes) {
			INT32 nBuffer = 0x0C00 + ((GetCurrentFrame() & 1) << 9);

			ps = (UINT16*)CpsPalSrc + (nBuffer ^ 0x0200);
			pn = (UINT16*)pNewPal;
			CpsObjPal = CpsPal + (nBuffer ^ 0x0200);

			for (i = 0; i < 0x0200; i++, ps++, pn++) {
				UINT16 n;
				n = *pn;
				if (*ps == n) {
					continue;								// Colour hasn't changed - great!
				}

				*ps = n;									// Update our copy of the palette

				CpsObjPal[i ^ 15] = CalcColCPS2(n);
			}

			ps = (UINT16*)CpsPalSrc + 0x0200;
			pn = (UINT16*)pNewPal + 0x0200;

			for (i = 0x0200; i < 0x0800; i++, ps++, pn++) {
				UINT16 n;
				n = *pn;
				if (*ps == n) {
					continue;								// Colour hasn't changed - great!
				}

				*ps = n;									// Update our copy of the palette

				CpsPal[i ^ 15] = CalcColCPS2(n);
			}

			CpsObjPal = CpsPal + nBuffer;
		} else {
			ps = (UINT16*)CpsPalSrc;
			pn = (UINT16*)pNewPal;

			for (i = 0x0000; i < 0x0800; i++, ps++, pn++) {
				UINT16 n = *pn;
				if (*ps == n) {
					continue;								// Colour hasn't changed - great!
				}

				*ps = n;									// Update our copy of the palette

				CpsPal[i ^ 15] = CalcColCPS2(n);
			}
		}
	} else {
		ps = (UINT16*)CpsPalSrc;
		pn = (UINT16*)pNewPal;

		for (i = 0x0000; i < 0x0c00; i++, ps++, pn++) {
			UINT16 n = *pn;
			if (*ps == n) {
                continue;								// Colour hasn't changed - great!
			}

			*ps = n;									// Update our copy of the palette

			CpsPal[i ^ 15] = CalcColCPS1(n);
		}
	}

	return 0;
}

INT32 CpsStarPalUpdate(UINT8* pNewPal, INT32 nLayer, INT32 bRecalcAll)
{
	INT32 i;
	UINT16 *ps, *pn;

	if (nLayer == 0) {
		ps = (UINT16*)CpsPalSrc + 0x0800;
		pn = (UINT16*)pNewPal + 0x0800;

		if (bRecalcAll) {
			memcpy(ps, pn, 256);
			CalcAllStar(nLayer);
			return 0;
		}

		// Star layer 0
		for (i = 0x0800; i < 0x0880; i++, ps++, pn++) {
			UINT16 n = *pn;
			if (*ps == n) {
				   continue;					// Colour hasn't changed - great!
			}

			*ps = n;							// Update our copy of the palette

			CpsPal[i ^ 15] = CalcColCPS1(n);
		}
	} else {
		ps = (UINT16*)CpsPalSrc + 0x0A00;
		pn = (UINT16*)pNewPal + 0x0A00;

		if (bRecalcAll) {
			memcpy(ps, pn, 256);
			CalcAllStar(nLayer);
			return 0;
		}

		// Star layer 1
		for (i = 0x0A00; i < 0x0A80; i++, ps++, pn++) {
			UINT16 n = *pn;
			if (*ps == n) {
				   continue;					// Colour hasn't changed - great!
			}

			*ps = n;							// Update our copy of the palette

			CpsPal[i ^ 15] = CalcColCPS1(n);
		}
	}

	return 0;
}

