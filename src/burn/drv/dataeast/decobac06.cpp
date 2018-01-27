#include "tiles_generic.h"

int bac06_depth = 4;
int bac06_yadjust = 0;

void bac06_draw_layer(UINT8 *vram, UINT16 control[2][4], UINT8 *rsram, UINT8 *csram, UINT8 *gfx8, INT32 col8, INT32 mask8, UINT8 *gfx16, INT32 col16, INT32 mask16, INT32 widetype, INT32 opaque)
{
	const INT32 dims[4][3][2] = {
		{ { 128,  32 }, {  64, 64 }, { 32, 128 } },
		{ {  64,  16 }, {  32, 32 }, { 16,  64 } },
		{ { 128,  16 }, {  64, 32 }, { 32,  64 } },
		{ { 256,  16 }, { 128, 32 }, { 64,  64 } }
	};

	INT32 bank = ( control[0][2] & 1) * 0x1000;
	INT32 size = (~control[0][0] & 1);
	INT32 shape =  control[0][3] & 3;
	if (shape == 3) shape = 1;

	INT32 tsize = (8 << size);
	INT32 wide = dims[(size) ? (widetype + 1) : 0][shape][0];
	INT32 high = dims[(size) ? (widetype + 1) : 0][shape][1];
	INT32 bsize = dims[(size) ? (widetype + 1) : 0][0][1];

	UINT16 *ram = (UINT16*)vram;

	INT32 scrollx  = (control[1][0]) & ((wide * tsize) - 1);
	INT32 scrolly  = (control[1][1] + bac06_yadjust) & ((high * tsize) - 1);
	INT32 rsenable = (control[0][0] & 0x04) && (rsram != NULL);
	INT32 csenable = (control[0][0] & 0x08) && (csram != NULL);

	if (rsenable || csenable)
	{
		UINT16 *scrx = (UINT16*)rsram;
		UINT16 *scry = (UINT16*)csram;
		INT32 mwidth  = (wide * tsize)-1;
		INT32 mheight = (high * tsize)-1;
		INT32 colbase = size ? col16 : col8;
		INT32 gfxmask = size ? mask16 : mask8;

		UINT16 *dst = pTransDraw;
		UINT8 *gfxbase = size ? gfx16 : gfx8;

		for (INT32 sy = 0; sy < nScreenHeight; sy++)
		{
			INT32 syy = (sy + scrolly) & mheight;
			if (csenable) syy = (syy + scry[syy]) & mheight;
			INT32 row = syy / tsize;
			INT32 zy = syy & (tsize - 1);

			for (INT32 sx = 0; sx < nScreenWidth; sx++)
			{
				INT32 sxx = (sx + scrollx) & mwidth;
				if (rsenable) sxx = (sxx + scrx[syy]) & mwidth;
				INT32 col = sxx / tsize;
				INT32 zx = sxx & (tsize - 1);

				INT32 offs = (col & (bsize - 1)) + (row * bsize) + ((col & ~(bsize - 1)) * high);

				INT32 code  = ram[offs];
				INT32 color = ((code >> 12) << bac06_depth) | colbase;

				code = (((code & 0xfff) + bank) & gfxmask) * (tsize * tsize);

				INT32 pxl = gfxbase[code + (zy * tsize) + zx];

				if (pxl | opaque)
					dst[sx] = pxl + color;
			}

			dst += nScreenWidth;
		}
	} else {
		for (INT32 row = 0; row < high; row++)
		{
			INT32 sy = (row * tsize) - scrolly;
			if (sy <= -tsize) sy += high * tsize;
			if (sy >= nScreenHeight) continue;

			for (INT32 col = 0; col < wide; col++)
			{
				INT32 sx = (col * tsize) - scrollx;		
				if (sx <= -tsize) sx += wide * tsize;
				if (sx >= nScreenWidth) continue;

				INT32 offs = (col & (bsize - 1)) + (row * bsize) + ((col & ~(bsize - 1)) * high);

				INT32 code  = ram[offs];
				INT32 color = (code >> 12);	
				code = (code & 0xfff) + bank;

				if (opaque)
				{
					if (size)
					{
						Render16x16Tile_Clip(pTransDraw, code & mask16, sx, sy, color, bac06_depth, col16, gfx16);
		
					}
					else
					{
						Render8x8Tile_Clip(pTransDraw, code & mask8, sx, sy, color, bac06_depth, col8, gfx8);
					}
				}
				else
				{
					if (size)
					{
						Render16x16Tile_Mask_Clip(pTransDraw, code & mask16, sx, sy, color, bac06_depth, 0, col16, gfx16);
		
					}
					else
					{
						Render8x8Tile_Mask_Clip(pTransDraw, code & mask8, sx, sy, color, bac06_depth, 0, col8, gfx8);
					}
				}
			}
		}
	}
}
