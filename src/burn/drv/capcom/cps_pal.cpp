#include "cps.h"
#include "bitswap.h"

// CPS (palette)

static bool CpsXmvPaletteRestored = false;	// xmvsf3v3: CpsPalVisualScan() restored CpsPal
static UINT8* CpsPalSrc = NULL;			// Copy of current input palette
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
	if (CpsXmvSixEnabled) memset(CpsPal, 0, nLen);
	CpsXmvPaletteRestored = false;

	return 0;
}

INT32 CpsPalExit()
{
	BurnFree(CpsPal);
	BurnFree(CpsPalSrc);
	return 0;
}

// Update CpsPal with the new palette at pNewPal (length 0xc00 bytes)
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
			for (INT32 Offset = 0; Offset < 0x200; ++Offset) {
				INT32 Palette = BURN_ENDIAN_SWAP_INT16(*(PaletteRAM++));
				INT32 r, g, b, Bright;
				
				Bright = 0x0f + ((Palette >> 12) << 1);

				r = ((Palette >> 8) & 0x0f) * 0x11 * Bright / 0x2d;
				g = ((Palette >> 4) & 0x0f) * 0x11 * Bright / 0x2d;
				b = ((Palette >> 0) & 0x0f) * 0x11 * Bright / 0x2d;
				
				CpsPal[(0x200 * nPage) + (Offset ^ 15)] = BurnHighCol(r, g, b, 0);
			}
		} else {
			if (PaletteRAM != (UINT16*)CpsPalSrc) {
				PaletteRAM += 0x200;
			}
		}
	}

	return 0;
}

// xmvsf3v3: the effective colours keep pages the game disabled between palette DMAs, which
// rereading palette RAM after a load can't rebuild; the pixel format must not change in between
INT32 CpsPalVisualScan(INT32 nAction)
{
	if (!CpsXmvSixEnabled || !(nAction & ACB_DRIVER_DATA)) return 0;

	struct BurnArea ba;
	memset(&ba, 0, sizeof(ba));
	ba.Data = CpsPalSrc;	ba.nLen = 0xc00 * sizeof(UINT16);	ba.szName = "XMvsSF 3v3 palette DMA input";	BurnAcb(&ba);
	ba.Data = CpsPal;		ba.nLen = 0xc00 * sizeof(UINT32);	ba.szName = "XMvsSF 3v3 effective palette";	BurnAcb(&ba);

	if (nAction & ACB_WRITE) CpsXmvPaletteRestored = true;

	return 0;
}

INT32 CpsPalConsumeRestoredCache()
{
	if (!CpsXmvSixEnabled || !CpsXmvPaletteRestored) return 0;

	CpsXmvPaletteRestored = false;

	return 1;
}
