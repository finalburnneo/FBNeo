#include "burnint.h"
#include "burn_pal.h"

UINT32 *BurnPalette = NULL;
UINT8 *BurnPalRAM = NULL;

//-------------------------------------------------------------------------------------

static inline UINT32 PaletteWrite4Bit(INT32 offset, INT32 rshift, INT32 gshift, INT32 bshift)
{
	if (BurnPalRAM == NULL) return 0;

	UINT16 *pal = (UINT16*)BurnPalRAM;
	UINT16 p = BURN_ENDIAN_SWAP_INT16(pal[offset]);

	UINT8 r = (p >> rshift) & 0xf;
	UINT8 g = (p >> gshift) & 0xf;
	UINT8 b = (p >> bshift) & 0xf;

	return BurnHighCol(r+(r*16), b+(b*16), g+(g*16), 0);	
}

static inline void PaletteUpdate4Bit(INT32 rshift, INT32 gshift, INT32 bshift)
{
	if (BurnPalette == NULL) return;

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		BurnPalette[i] = PaletteWrite4Bit(i, rshift, gshift,  bshift);
	}
}

void BurnPaletteUpdate_xxxxBBBBGGGGRRRR()
{
	PaletteUpdate4Bit(0, 4, 8);
}

void BurnPaletteUpdate_xxxxBBBBRRRRGGGG()
{
	PaletteUpdate4Bit(4, 0, 8);
}

void BurnPaletteUpdate_xxxxRRRRGGGGBBBB()
{
	PaletteUpdate4Bit(8, 4, 0);
}

void BurnPaletteWrite_xxxxBBBBGGGGRRRR(INT32 offset)
{
	offset /= 2;

	BurnPalette[offset] = PaletteWrite4Bit(offset, 0, 4, 8);
}

void BurnPaletteWrite_xxxxBBBBRRRRGGGG(INT32 offset)
{
	offset /= 2;

	BurnPalette[offset] = PaletteWrite4Bit(offset, 4, 0, 8);
}

void BurnPaletteWrite_xxxxRRRRGGGGBBBB(INT32 offset)
{
	offset /= 2;

	BurnPalette[offset] = PaletteWrite4Bit(offset, 8, 4, 0);
}

//-------------------------------------------------------------------------------------

static inline UINT32 PaletteWrite5Bit(INT32 offset, INT32 rshift, INT32 gshift, INT32 bshift)
{
	if (BurnPalRAM == NULL) return 0;

	UINT16 *pal = (UINT16*)BurnPalRAM;
	UINT16 p = BURN_ENDIAN_SWAP_INT16(pal[offset]);

	UINT8 r = (p >> rshift) & 0x1f;
	UINT8 g = (p >> gshift) & 0x1f;
	UINT8 b = (p >> bshift) & 0x1f;

	r = (r * 8) + (r / 4);
	g = (g * 8) + (g / 4);
	b = (b * 8) + (b / 4);

	return BurnHighCol(r, g, b, 0);
}

static inline void PaletteUpdate5Bit(INT32 rshift, INT32 gshift, INT32 bshift)
{
	if (BurnPalette == NULL) return;

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		BurnPalette[i] = PaletteWrite5Bit(i, rshift, gshift,  bshift);
	}
}

void BurnPaletteUpdate_xRRRRRGGGGGBBBBB()
{
	PaletteUpdate5Bit(10, 5, 0);
}

void BurnPaletteUpdate_xBBBBBGGGGGRRRRR()
{
	PaletteUpdate5Bit(0, 5, 10);
}

void BurnPaletteUpdate_xGGGGGBBBBBRRRRR()
{
	PaletteUpdate5Bit(0, 10, 5);
}

void BurnPaletteUpdate_xGGGGGRRRRRBBBBB()
{
	PaletteUpdate5Bit(5, 10, 0);
}

void BurnPaletteUpdate_GGGGGRRRRRBBBBBx()
{
	PaletteUpdate5Bit(6, 11, 1);
}

void BurnPaletteWrite_xRRRRRGGGGGBBBBB(INT32 offset)
{
	offset /= 2;

	if (BurnPalette) {
		BurnPalette[offset] = PaletteWrite5Bit(offset, 10, 5, 0);
	}
}

void BurnPaletteWrite_xBBBBBGGGGGRRRRR(INT32 offset)
{
	offset /= 2;

	if (BurnPalette) {
		BurnPalette[offset] = PaletteWrite5Bit(offset, 0, 5, 10);
	}
}

void BurnPaletteWrite_xGGGGGBBBBBRRRRR(INT32 offset)
{
	offset /= 2;

	if (BurnPalette) {
		BurnPalette[offset] = PaletteWrite5Bit(offset, 0, 10, 5);
	}
}

void BurnPaletteWrite_xGGGGGRRRRRBBBBB(INT32 offset)
{
	offset /= 2;

	if (BurnPalette) {
		BurnPalette[offset] = PaletteWrite5Bit(offset, 5, 10, 0);
	}
}

void BurnPaletteWrite_GGGGGRRRRRBBBBBx(INT32 offset)
{
	offset /= 2;

	if (BurnPalette) {
		BurnPalette[offset] = PaletteWrite5Bit(offset, 6, 11, 1);
	}
}

//-------------------------------------------------------------------------------------

void BurnPaletteUpdate_RRRRGGGGBBBBRGBx()
{
	if (BurnPalRAM == NULL || BurnPalette == NULL) return;

	UINT16 *pal = (UINT16*)BurnPalRAM;

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		UINT16 p = BURN_ENDIAN_SWAP_INT16(pal[i]);

		UINT8 r = ((p >>  11) & 0x1e) | ((p >> 3) & 0x01);
		UINT8 g = ((p >>   7) & 0x1e) | ((p >> 2) & 0x01);
		UINT8 b = ((p >>   3) & 0x1e) | ((p >> 1) & 0x01);

		r = (r * 8) + (r / 4);
		g = (g * 8) + (g / 4);
		b = (b * 8) + (b / 4);

		BurnPalette[i] = BurnHighCol(r, g, b, 0);
	}	
}

void BurnPaletteWrite_RRRRGGGGBBBBRGBx(INT32 offset)
{
	if (BurnPalRAM == NULL || BurnPalette == NULL) return;

	offset /= 2;

	UINT16 *pal = (UINT16*)BurnPalRAM;
	UINT16 p = BURN_ENDIAN_SWAP_INT16(pal[offset]);

	UINT8 r = ((p >>  11) & 0x1e) | ((p >> 3) & 0x01);
	UINT8 g = ((p >>   7) & 0x1e) | ((p >> 2) & 0x01);
	UINT8 b = ((p >>   3) & 0x1e) | ((p >> 1) & 0x01);

	r = (r * 8) + (r / 4);
	g = (g * 8) + (g / 4);
	b = (b * 8) + (b / 4);

	BurnPalette[offset] = BurnHighCol(r, g, b, 0);
}

//-------------------------------------------------------------------------------------

static inline void palette_update_8bit(INT32 r_mask, INT32 g_mask, INT32 b_mask, INT32 r_shift, INT32 g_shift, INT32 b_shift, INT32 invert)
{
	if (BurnPalRAM == NULL || BurnPalette == NULL) return;

	r_mask = (1 << r_mask) - 1;
	g_mask = (1 << g_mask) - 1;
	b_mask = (1 << b_mask) - 1;
	invert = (invert) ? 0xff : 0;

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		UINT8 p = BurnPalRAM[i] ^ invert;
		UINT8 r = (p >> r_shift) & r_mask;
		UINT8 g = (p >> g_shift) & g_mask;
		UINT8 b = (p >> b_shift) & b_mask;

		if (r_mask == 3) r = pal2bit(r);
		if (r_mask == 7) r = pal3bit(r);

		if (g_mask == 3) g = pal2bit(r);
		if (g_mask == 7) g = pal3bit(r);

		if (b_mask == 3) b = pal2bit(r);
		if (b_mask == 7) b = pal3bit(r);

		BurnPalette[i] = BurnHighCol(r,g,b,0);
	}
}

void BurnPaletteUpdate_BBGGGRRR()
{
	palette_update_8bit(3, 3, 2, 0, 3, 6, 0);
}

void BurnPaletteUpdate_RRRGGGBB()
{
	palette_update_8bit(3, 3, 2, 5, 2, 0, 0);
}

void BurnPaletteUpdate_BBGGGRRR_inverted()
{
	palette_update_8bit(3, 3, 2, 0, 3, 6, 1);
}

void BurnPaletteUpdate_RRRGGGBB_inverted()
{
	palette_update_8bit(3, 3, 2, 5, 2, 0, 1);
}

static inline void palette_write_8bit(INT32 offset, INT32 r_shift, INT32 g_shift, INT32 b_shift, INT32 r_mask, INT32 g_mask, INT32 b_mask, INT32 invert)
{
	if (BurnPalRAM == NULL || BurnPalette == NULL) return;

	UINT8 p = BurnPalRAM[offset] ^ invert;
	UINT8 r = (p >> r_shift) & r_mask;
	UINT8 g = (p >> g_shift) & g_mask;
	UINT8 b = (p >> b_shift) & b_mask;

	if (r_mask == 3) r = pal2bit(r);
	if (r_mask == 7) r = pal3bit(r);

	if (g_mask == 3) g = pal2bit(r);
	if (g_mask == 7) g = pal3bit(r);

	if (b_mask == 3) b = pal2bit(r);
	if (b_mask == 7) b = pal3bit(r);

	BurnPalette[offset] = BurnHighCol(r,g,b,0);
}

void BurnPaletteWrite_BBGGGRRR(INT32 offset)
{
	palette_write_8bit(offset, 3, 3, 2, 0, 3, 6, 0);
}

void BurnPaletteWrite_RRRGGGBB(INT32 offset)
{
	palette_write_8bit(offset, 3, 3, 2, 5, 2, 0, 0);
}

void BurnPaletteWrite_BBGGGRRR_inverted(INT32 offset)
{
	palette_write_8bit(offset, 3, 3, 2, 0, 3, 6, 1);
}

void BurnPaletteWrite_RRRGGGBB_inverted(INT32 offset)
{
	palette_write_8bit(offset, 3, 3, 2, 5, 2, 0, 1);
}
