#include "neogeo.h"
#include "burn_pal.h"
#include "resnet.h"
#include "bitswap.h"
// Neo Geo -- palette functions

UINT8* NeoPalSrc[2];				// Pointer to input palettes
UINT32* NeoPalette;
INT32 nNeoPaletteBank;				// Selected palette bank
INT32 bNeoDarkenPalette;			// Screen Brightness mode

static UINT32* NeoPaletteData[2] = {NULL, NULL};
static UINT16* NeoPaletteCopy[2] = {NULL, NULL};

UINT8 NeoRecalcPalette;

static double rgbweights[6];
static double rgbweights_darken[6];

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

	// dinknotes:
	// https://wiki.neogeodev.org/images/3/32/Neogeo_aes_schematics_pal_2-page-002.jpg
	// https://wiki.neogeodev.org/images/7/72/Mv1fs-page3.jpg
	// https://wiki.neogeodev.org/images/8/8b/MV-4_Schematics_2.jpg
	// https://wiki.neogeodev.org/images/a/ab/MV-4F_Schematics_3.jpg
	static const int resistances[6] = { 8200, 3900, 2200, 1000, 470, 220 };

	compute_resistor_weights(0, 0xff, -1.0,
			6, &resistances[0], rgbweights, 0, 0,
			6, &resistances[0], rgbweights_darken, 150, 0, // "darken" (IO2 port 0x01 / 0x11) adds a 150ohm pulldown - marked "shadow" on schematics
			0, NULL, NULL, 0, 0);

	return 0;
}

void NeoExitPalette()
{
	for (INT32 i = 0; i < 2; i++) {
		BurnFree(NeoPaletteData[i]);
		BurnFree(NeoPaletteCopy[i]);
	}
}

static UINT8 get_weighty(UINT8 rgbval)
{
	rgbval >>= 2; // 0xfc [111111..] -> 0x3f [..111111] (for sanity)

	UINT32 v = combine_6_weights((bNeoDarkenPalette) ? rgbweights_darken : rgbweights,
								 BIT(rgbval, 0), BIT(rgbval, 1), BIT(rgbval, 2), BIT(rgbval, 3), BIT(rgbval, 4), BIT(rgbval, 5));
	return (v & 0xff);
}

inline static UINT32 CalcCol(UINT16 nColour)
{
	INT32 r = (nColour & 0x0F00) >> 4;	// Red
	r |= (nColour >> 11) & 8;
	r |= (~nColour >> 13) & 4;			// Red: add "dark" bit
	INT32 g = (nColour & 0x00F0);		// Green
	g |= (nColour >> 10) & 8;
	g |= (~nColour >> 13) & 4;
	INT32 b = (nColour & 0x000F) << 4;	// Blue
	b |= (nColour >> 9) & 8;
	b |= (~nColour >> 13) & 4;

	r = get_weighty(r);
	g = get_weighty(g);
	b = get_weighty(b);

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

