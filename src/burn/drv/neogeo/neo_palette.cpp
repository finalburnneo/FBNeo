#include "neogeo.h"
// Neo Geo -- palette functions

UINT8* NeoPalSrc[2];				// Pointer to input palettes
UINT32* NeoPalette;
INT32 nNeoPaletteBank;				// Selected palette bank
INT32 bNeoDarkenPalette;			// Screen Brightness mode

static UINT32* NeoPaletteData[2] = {NULL, NULL};
static UINT16* NeoPaletteCopy[2] = {NULL, NULL};

UINT8 NeoRecalcPalette;

INT32 NeoInitPalette()
{
	for (INT32 i = 0; i < 2; i++) {
		if (NeoPaletteData[i]) {
			BurnFree(NeoPaletteData[i]);
		}
		if (NeoPaletteCopy[i]) {
			BurnFree(NeoPaletteCopy[i]);
		}
		NeoPaletteData[i] = (UINT32*)BurnMalloc(4096 * sizeof(UINT32));
		NeoPaletteCopy[i] = (UINT16*)BurnMalloc(4096 * sizeof(UINT16));
	}

	NeoRecalcPalette = 1;
	bNeoDarkenPalette = 0;

	return 0;
}

void NeoExitPalette()
{
	for (INT32 i = 0; i < 2; i++) {
		BurnFree(NeoPaletteData[i]);
		BurnFree(NeoPaletteCopy[i]);
	}
}

inline static UINT32 CalcCol(UINT16 nColour)
{
	INT32 r = (nColour & 0x0F00) >> 4;	// Red
	r |= (nColour >> 11) & 8;
	r |= (nColour >> 13) & 4;			// Red: add "dark" bit
	INT32 g = (nColour & 0x00F0);		// Green
	g |= (nColour >> 10) & 8;
	g |= (nColour >> 13) & 4;
	INT32 b = (nColour & 0x000F) << 4;	// Blue
	b |= (nColour >> 9) & 8;
	b |= (nColour >> 13) & 4;

	// our current color mask is 0xfc (6 bits)
	// shift down and OR to fill in the low 2 bits
	r |= r >> 6;
	g |= g >> 6;
	b |= b >> 6;

	if (bNeoDarkenPalette) {
		r >>= 1;
		g >>= 1;
		b >>= 1;
	}

	return BurnHighCol(r, g, b, 0);
}

INT32 NeoUpdatePalette()
{
	if (NeoRecalcPalette) {
		INT32 i;
		UINT16* ps;
		UINT16* pc;
		UINT32* pd;

		// Update both palette banks
		for (INT32 j = 0; j < 2; j++) {
			for (i = 0, ps = (UINT16*)NeoPalSrc[j], pc = NeoPaletteCopy[j], pd = NeoPaletteData[j]; i < 4096; i++, ps++, pc++, pd++) {
				*pc = *ps;
				*pd = CalcCol(BURN_ENDIAN_SWAP_INT16(*ps));
			}
		}

		NeoRecalcPalette = 0;

	}

	return 0;
}

void NeoSetPalette()
{
	NeoPalette = NeoPaletteData[nNeoPaletteBank];
	pBurnDrvPalette = NeoPalette;
}

// Update the PC copy of the palette on writes to the palette memory
void __fastcall NeoPalWriteByte(UINT32 nAddress, UINT8 byteValue)
{
	nAddress &= 0x1FFF;
	nAddress ^= 1;

	NeoPalSrc[nNeoPaletteBank][nAddress] = byteValue;							// write byte

	if (((UINT8*)NeoPaletteCopy[nNeoPaletteBank])[nAddress] != byteValue) {
		((UINT8*)NeoPaletteCopy[nNeoPaletteBank])[nAddress]  = byteValue;
		NeoPaletteData[nNeoPaletteBank][nAddress >> 1] = CalcCol(*(UINT16*)(NeoPalSrc[nNeoPaletteBank] + (nAddress & ~0x01)));
	}
}

void __fastcall NeoPalWriteWord(UINT32 nAddress, UINT16 wordValue)
{
	nAddress &= 0x1FFF;
	nAddress >>= 1;

	((UINT16*)NeoPalSrc[nNeoPaletteBank])[nAddress] = BURN_ENDIAN_SWAP_INT16(wordValue);		// write word

	if (NeoPaletteCopy[nNeoPaletteBank][nAddress] != BURN_ENDIAN_SWAP_INT16(wordValue)) {
		NeoPaletteCopy[nNeoPaletteBank][nAddress] = BURN_ENDIAN_SWAP_INT16(wordValue);
		NeoPaletteData[nNeoPaletteBank][nAddress] = CalcCol(wordValue);
	}
}

