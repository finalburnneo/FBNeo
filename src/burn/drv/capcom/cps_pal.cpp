#include "cps.h"
#include "bitswap.h"

// CPS (palette)

static UINT8* CpsPalSrc = NULL;			// Copy of current input palette
CpsPalUpdateCallback CpsPalUpdateCallbackFunction = NULL;
UINT32* CpsPal = NULL;					// Hicolor version of palette
INT32 nCpsPalCtrlReg;
INT32 bCpsUpdatePalEveryFrame = 0;		// Some of the hacks need this as they don't write to CpsReg 0x0a

INT32 CpsPalInit()
{
	INT32 nLen = 0;

	nLen = 0xc00 * sizeof(UINT16);
	CpsPalSrc = (UINT8*)BurnMalloc(nLen);
	if (CpsPalSrc == NULL) {
		return 1;
	}
	memset(CpsPalSrc, 0, nLen);

	nLen = 0xc00 * sizeof(UINT32);
	CpsPal = (UINT32*)BurnMalloc(nLen);
	if (CpsPal == NULL) {
		return 1;
	}

	return 0;
}

INT32 CpsPalExit()
{
	BurnFree(CpsPal);
	BurnFree(CpsPalSrc);
	CpsPalUpdateCallbackFunction = NULL;
	return 0;
}

static inline UINT32 CpsPalColor(UINT16 nRaw)
{
	INT32 Palette = BURN_ENDIAN_SWAP_INT16(nRaw);
	INT32 Bright = 0x0f + ((Palette >> 12) << 1);
	INT32 r = ((Palette >> 8) & 0x0f) * 0x11 * Bright / 0x2d;
	INT32 g = ((Palette >> 4) & 0x0f) * 0x11 * Bright / 0x2d;
	INT32 b = ((Palette >> 0) & 0x0f) * 0x11 * Bright / 0x2d;
	return BurnHighCol(r, g, b, 0);
}

// Update CpsPal with the new palette at pNewPal (length 0xc00 words)
INT32 CpsPalUpdate(UINT8* pNewPal)
{
	UINT16 *ps, *pn;

	ps = (UINT16*)CpsPalSrc;
	pn = (UINT16*)pNewPal;

	memcpy(ps, pn, 0xc00 * sizeof(UINT16));

	INT32 nCtrl = CpsReg[nCpsPalCtrlReg];
	UINT16 *PaletteRAM = (UINT16*)CpsPalSrc;

	for (INT32 nPage = 0; nPage < 6; nPage++) {
		if (BIT(nCtrl, nPage)) {
			if (CpsPalUpdateCallbackFunction) CpsPalUpdateCallbackFunction(nPage, PaletteRAM);
			for (INT32 Offset = 0; Offset < 0x200; ++Offset) {
				CpsPal[(0x200 * nPage) + (Offset ^ 15)] = CpsPalColor(*PaletteRAM++);
			}
		} else {
			if (PaletteRAM != (UINT16*)CpsPalSrc) {
				PaletteRAM += 0x200;
			}
		}
	}

	return 0;
}

void CpsPalRecalc(const UINT16* pRaw)
{
	for (INT32 i = 0; i < 0xc00; i++) CpsPal[i ^ 15] = CpsPalColor(pRaw[i]);
}
