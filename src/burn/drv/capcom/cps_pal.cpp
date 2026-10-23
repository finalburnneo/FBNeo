#include "cps.h"
#include "bitswap.h"

// CPS (palette)

static bool CpsPaletteRestored = false;	// CpsPalVisualScan() restored CpsPal
static UINT8* CpsPalSrc = NULL;			// Effective hardware palette words
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
	if (CpsXmvSixEnabled || Cps2Plus) memset(CpsPal, 0, nLen);
	CpsPaletteRestored = false;

	return 0;
}

INT32 CpsPalExit()
{
	BurnFree(CpsPal);
	BurnFree(CpsPalSrc);
	return 0;
}

static inline UINT32 CpsPalColour(UINT16 nRaw)
{
	INT32 Palette = BURN_ENDIAN_SWAP_INT16(nRaw);
	INT32 Bright = 0x0f + ((Palette >> 12) << 1);
	INT32 r = ((Palette >> 8) & 0x0f) * 0x11 * Bright / 0x2d;
	INT32 g = ((Palette >> 4) & 0x0f) * 0x11 * Bright / 0x2d;
	INT32 b = ((Palette >> 0) & 0x0f) * 0x11 * Bright / 0x2d;
	return BurnHighCol(r, g, b, 0);
}

// Update the enabled palette pages from the DMA source.
INT32 CpsPalUpdate(UINT8* pNewPal)
{
	UINT16* ps = (UINT16*)CpsPalSrc;
	UINT16* pn = (UINT16*)pNewPal;
	UINT16* PaletteRAM = pn;
	INT32 nCtrl = CpsReg[nCpsPalCtrlReg];

	for (INT32 nPage = 0; nPage < 6; nPage++) {
		if (BIT(nCtrl, nPage)) {
			for (INT32 Offset = 0; Offset < 0x200; ++Offset) {
				UINT16 nRaw = *PaletteRAM++;
				// Disabled destination pages retain their previous hardware colours.
				ps[(0x200 * nPage) + Offset] = nRaw;
				CpsPal[(0x200 * nPage) + (Offset ^ 15)] = CpsPalColour(nRaw);
			}
		} else {
			if (PaletteRAM != pn) PaletteRAM += 0x200;
		}
	}

	return 0;
}

// Palette RAM can already contain the next DMA's colours. Save the effective
// palette instead; hardware words can be converted to the current pixel format.
INT32 CpsPalVisualScan(INT32 nAction)
{
	if (!(nAction & ACB_DRIVER_DATA)) return 0;

	struct BurnArea ba;
	memset(&ba, 0, sizeof(ba));
	if (CpsXmvSixEnabled) {
		// Preserve this driver's existing converted-palette state layout.
		ba.Data = CpsPal;
		ba.nLen = 0xc00 * sizeof(UINT32);
		ba.szName = "XMvsSF 3v3 effective palette";
	} else {
		ba.Data = CpsPalSrc;
		ba.nLen = 0xc00 * sizeof(UINT16);
		ba.szName = "Effective palette";
	}
	BurnAcb(&ba);

	if (nAction & ACB_WRITE) {
		if (!CpsXmvSixEnabled) {
			UINT16* ps = (UINT16*)CpsPalSrc;
			for (INT32 i = 0; i < 0xc00; i++) CpsPal[i ^ 15] = CpsPalColour(ps[i]);
		}
		CpsPaletteRestored = true;
	}

	return 0;
}

INT32 CpsPalConsumeRestoredCache()
{
	if (!CpsPaletteRestored) return 0;

	CpsPaletteRestored = false;

	return 1;
}
