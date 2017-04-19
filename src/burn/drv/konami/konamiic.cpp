#include "tiles_generic.h"
#include "konamiic.h"

UINT32 KonamiIC_K051960InUse = 0;
UINT32 KonamiIC_K052109InUse = 0;
UINT32 KonamiIC_K051316InUse = 0;
UINT32 KonamiIC_K053245InUse = 0;
UINT32 KonamiIC_K053247InUse = 0;
UINT32 KonamiIC_K053936InUse = 0;
UINT32 KonamiIC_K053250InUse = 0;
UINT32 KonamiIC_K055555InUse = 0;
UINT32 KonamiIC_K054338InUse = 0;
UINT32 KonamiIC_K056832InUse = 0;

UINT32 *konami_bitmap32 = NULL;
UINT8  *konami_priority_bitmap = NULL;
UINT32 *konami_palette32;

static INT32 previous_depth = 0;
static UINT16 *palette_lut = NULL;

//static UINT16 *konami_blendpal16;

static INT32 highlight_mode = 0;  // set in driver init.
static INT32 highlight_over_sprites_mode = 0; // ""

void konami_sortlayers3( int *layer, int *pri )
{
#define SWAP(a,b) \
	if (pri[a] < pri[b]) \
	{ \
		int t; \
		t = pri[a]; pri[a] = pri[b]; pri[b] = t; \
		t = layer[a]; layer[a] = layer[b]; layer[b] = t; \
	}

	SWAP(0,1)
	SWAP(0,2)
	SWAP(1,2)
#undef  SWAP
}

void konami_sortlayers4( int *layer, int *pri )
{
#define SWAP(a,b) \
	if (pri[a] <= pri[b]) \
	{ \
		int t; \
		t = pri[a]; pri[a] = pri[b]; pri[b] = t; \
		t = layer[a]; layer[a] = layer[b]; layer[b] = t; \
	}

	SWAP(0, 1)
	SWAP(0, 2)
	SWAP(0, 3)
	SWAP(1, 2)
	SWAP(1, 3)
	SWAP(2, 3)
#undef  SWAP
}

void konami_sortlayers5( int *layer, int *pri )
{
#define SWAP(a,b) \
	if (pri[a] <= pri[b]) \
	{ \
		int t; \
		t = pri[a]; pri[a] = pri[b]; pri[b] = t; \
		t = layer[a]; layer[a] = layer[b]; layer[b] = t; \
	}

	SWAP(0, 1)
	SWAP(0, 2)
	SWAP(0, 3)
	SWAP(0, 4)
	SWAP(1, 2)
	SWAP(1, 3)
	SWAP(1, 4)
	SWAP(2, 3)
	SWAP(2, 4)
	SWAP(3, 4)
#undef  SWAP
}

static void shuffle(UINT16 *buf, INT32 len)
{
	if (len == 2 || len & 3) return;

	len >>= 1;

	for (INT32 i = 0; i < len/2; i++)
	{
		INT32 t = buf[len/2 + i];
		buf[len/2 + i] = buf[len + i];
		buf[len + i] = t;
	}

	shuffle(buf,       len);
	shuffle(buf + len, len);
}

void konami_rom_deinterleave_2(UINT8 *src, INT32 len)
{
	shuffle((UINT16*)src,len/2);
}

void konami_rom_deinterleave_4(UINT8 *src, INT32 len)
{
	konami_rom_deinterleave_2(src, len);
	konami_rom_deinterleave_2(src, len);
}

void KonamiRecalcPalette(UINT8 *src, UINT32 *dst, INT32 len)
{
	konami_palette32 = dst;

	UINT8 r,g,b;
	UINT16 *p = (UINT16*)src;
	for (INT32 i = 0; i < len / 2; i++) {
		UINT16 d = BURN_ENDIAN_SWAP_INT16((p[i] << 8) | (p[i] >> 8));

		r = (d >>  0) & 0x1f;
		g = (d >>  5) & 0x1f;
		b = (d >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		dst[i] = (r << 16) | (g << 8) | b; // 32-bit colors
	}
}

void KonamiICReset()
{
	if (KonamiIC_K051960InUse) K051960Reset();
	if (KonamiIC_K052109InUse) K052109Reset();
	if (KonamiIC_K051316InUse) K051316Reset();
	if (KonamiIC_K053245InUse) K053245Reset();
	if (KonamiIC_K053247InUse) K053247Reset();
	if (KonamiIC_K053936InUse) K053936Reset();
	if (KonamiIC_K053250InUse) K053250Reset();
	if (KonamiIC_K055555InUse) K055555Reset();
	if (KonamiIC_K054338InUse) K054338Reset();
	if (KonamiIC_K056832InUse) K056832Reset();

	K053251Reset();
	K054000Reset();
	K051733Reset();
}

void KonamiICExit()
{
	if (konami_bitmap32) {
		BurnFree (konami_bitmap32);
		konami_bitmap32 = NULL;
	}

	if (konami_priority_bitmap) {
		BurnFree(konami_priority_bitmap);
		konami_priority_bitmap = NULL;
	}

	previous_depth = 0;
	if (palette_lut) {
		BurnFree(palette_lut);
	}
	palette_lut = NULL;

	if (KonamiIC_K051960InUse) K051960Exit();
	if (KonamiIC_K052109InUse) K052109Exit();
	if (KonamiIC_K051316InUse) K051316Exit();
	if (KonamiIC_K053245InUse) K053245Exit();
	if (KonamiIC_K053247InUse) K053247Exit();
	if (KonamiIC_K053936InUse) K053936Exit();
	if (KonamiIC_K053250InUse) K053250Exit();
	if (KonamiIC_K055555InUse) K055555Exit();
	if (KonamiIC_K054338InUse) K054338Exit();
	if (KonamiIC_K056832InUse) K056832Exit();

	KonamiIC_K051960InUse = 0;
	KonamiIC_K052109InUse = 0;
	KonamiIC_K051316InUse = 0;
	KonamiIC_K053245InUse = 0;
	KonamiIC_K053247InUse = 0;
	KonamiIC_K053250InUse = 0;
	KonamiIC_K055555InUse = 0;
	KonamiIC_K054338InUse = 0;
	KonamiIC_K056832InUse = 0;

	highlight_over_sprites_mode = 0;
	highlight_mode = 0;

	K05324xZRejection = -1;
}

void KonamiICScan(INT32 nAction)
{
	if (KonamiIC_K051960InUse) K051960Scan(nAction);
	if (KonamiIC_K052109InUse) K052109Scan(nAction);
	if (KonamiIC_K051316InUse) K051316Scan(nAction);
	if (KonamiIC_K053245InUse) K053245Scan(nAction);
	if (KonamiIC_K053247InUse) K053247Scan(nAction);
	if (KonamiIC_K053936InUse) K053936Scan(nAction);
	if (KonamiIC_K053250InUse) K053250Scan(nAction);
	if (KonamiIC_K055555InUse) K055555Scan(nAction);
	if (KonamiIC_K054338InUse) K054338Scan(nAction);
	if (KonamiIC_K056832InUse) K056832Scan(nAction);

	K053251Scan(nAction);
	K054000Scan(nAction);
	K051733Scan(nAction);
}

void KonamiAllocateBitmaps()
{
	INT32 width, height;
	BurnDrvGetVisibleSize(&width, &height);

	if (konami_bitmap32 == NULL) {
		konami_bitmap32 = (UINT32*)BurnMalloc(width * height * sizeof(INT32));
	}

	if (konami_priority_bitmap == NULL) {
		konami_priority_bitmap = (UINT8*)BurnMalloc(width * height * sizeof(INT8));
	}
}

void KonamiClearBitmaps(UINT32 color)
{
	if (konami_priority_bitmap && konami_bitmap32) {
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			konami_priority_bitmap[i] = 0;
			konami_bitmap32[i] = color;
		}
	}
}

void KonamiBlendCopy(UINT32 *pPalette)
{
	pBurnDrvPalette = pPalette;

	UINT32 *bmp = konami_bitmap32;

	if (previous_depth != 2 && nBurnBpp == 2) {
		if (palette_lut == NULL) {
			palette_lut = (UINT16*)BurnMalloc((1 << 24) * 2);

			for (INT32 i = 0; i < (1 << 24); i++) {
				palette_lut[i] = BurnHighCol(i / 0x10000, (i / 0x100) & 0xff, i & 0xff, 0);
			}
		}
	}

	previous_depth = nBurnBpp;

	switch (nBurnBpp)
	{
		case 4:
		{
			memcpy (pBurnDraw, bmp, nScreenWidth * nScreenHeight * sizeof(INT32));
		}
		break;

		case 2:
		{
			UINT16 *dst = (UINT16*)pBurnDraw;
			for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++, dst++, bmp++) {
				*dst = palette_lut[*bmp];
			}
		}
		break;

		case 3:
		{
			UINT8 *dst = pBurnDraw;
			for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++, dst+=3, bmp++) {
				dst[0] = *bmp;
				dst[1] = *bmp / 0x100;
				dst[2] = *bmp / 0x10000;
			}
		}
		break;
#if 0
		case 2:
		{
			UINT16 *dst = (UINT16*)pBurnDraw;
			for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++, dst++, bmp++) {
				*dst = *bmp / 0x10000;
				*dst += (*bmp / 0x400) & 0x3f;
				*dst += (*bmp / 8) & 0x1f;
			}	

		}
		break;
#endif
	    default:
			for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
				PutPix(pBurnDraw + (i * nBurnBpp), BurnHighCol((bmp[i]>>16)&0xff, (bmp[i]>>8)&0xff, bmp[i]&0xff, 0));
			}
			// bprintf (0, _T("Unsupported KonamiBlendCopy bit depth! %d\n"), nBurnBpp);
			break;
	}
}

void konami_draw_16x16_priozoom_tile(UINT8 *gfx, INT32 code, INT32 bpp, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, UINT32 priority)
{
	// Based on MAME sources for tile zooming
	UINT8 *gfx_base = gfx + (code * width * height);
	int dh = (zoomy * height + 0x8000) / 0x10000;
	int dw = (zoomx * width + 0x8000) / 0x10000;

	priority |= 1<<31; // always on!

	UINT32 *pal = konami_palette32 + (color << bpp);

	if (dw && dh)
	{
		int dx = (width * 0x10000) / dw;
		int dy = (height * 0x10000) / dh;
		int ex = sx + dw;
		int ey = sy + dh;
		int x_index_base = 0;
		int y_index = 0;

		if (fx) {
			x_index_base = (dw - 1) * dx;
			dx = -dx;
		}

		if (fy) {
			y_index = (dh - 1) * dy;
			dy = -dy;
		}

		for (INT32 y = sy; y < ey; y++)
		{
			if (y >= 0 && y < nScreenHeight)
			{
				UINT8 *src = gfx_base + (y_index / 0x10000) * width;
				UINT32 *dst = konami_bitmap32 + y * nScreenWidth;
				UINT8 *prio = konami_priority_bitmap + y * nScreenWidth;

				for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
				{
					if (x >= 0 && x < nScreenWidth) {
						INT32 pxl = src[x_index>>16];

						if (pxl != t) {
							if ((priority & (1 << (prio[x]&0x1f)))==0) {
								dst[x] = pal[pxl];
							}
							prio[x] |= 0x1f;
						}
					}
					x_index += dx;
				}
			}

			y_index += dy;
		}
	}
}

void konami_draw_16x16_zoom_tile(UINT8 *gfxbase, INT32 code, INT32 bpp, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy)
{
	UINT8 *gfx_base = gfxbase + (code * width * height);
	int dh = (zoomy * height + 0x8000) / 0x10000;
	int dw = (zoomx * width + 0x8000) / 0x10000;

	UINT32 *pal = konami_palette32 + (color << bpp);

	if (dw && dh)
	{
		int dx = (width * 0x10000) / dw;
		int dy = (height * 0x10000) / dh;
		int ex = sx + dw;
		int ey = sy + dh;
		int x_index_base = 0;
		int y_index = 0;

		if (fx) {
			x_index_base = (dw - 1) * dx;
			dx = -dx;
		}

		if (fy) {
			y_index = (dh - 1) * dy;
			dy = -dy;
		}

		for (INT32 y = sy; y < ey; y++)
		{
			if (y >= 0 && y < nScreenHeight)
			{
				UINT8 *src = gfx_base + (y_index / 0x10000) * width;
				UINT32 *dst = konami_bitmap32 + y * nScreenWidth;

				for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
				{
					if (x >= 0 && x < nScreenWidth) {
						INT32 pxl = src[x_index>>16];

						if (pxl != t) {
							dst[x] = pal[pxl];
						}
					}

					x_index += dx;
				}
			}

			y_index += dy;
		}
	}
}

void konami_set_highlight_mode(INT32 mode)
{
	highlight_mode = mode;
}

void konami_set_highlight_over_sprites_mode(INT32 mode)
{
	highlight_over_sprites_mode = mode;
}

static inline UINT32 shadow_blend(UINT32 d)
{
	return ((((d & 0xff00ff) * 0x9d) & 0xff00ff00) + (((d & 0x00ff00) * 0x9d) & 0x00ff0000)) / 0x100;
}

static inline UINT32 highlight_blend(UINT32 d)
{
	INT32 r = ((d&0xff0000)+0x220000);
	if (r > 0xff0000) r = 0xff0000;

	INT32 g = ((d&0x00ff00)+0x002200);
	if (g > 0x00ff00) g = 0x00ff00;

	INT32 b = ((d&0x0000ff)+0x000022);
	if (b > 0x0000ff) b = 0x0000ff;
	return r|g|b;
}

void konami_draw_16x16_prio_tile(UINT8 *gfxbase, INT32 code, INT32 bpp, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, UINT32 priority)
{
	INT32 flip = 0;
	if (flipx) flip |= 0x0f;
	if (flipy) flip |= 0xf0;

	UINT8 *gfx = gfxbase + code * 0x100;

	UINT8 *pri = konami_priority_bitmap + (sy * nScreenWidth) + sx;
	UINT32 *dst = konami_bitmap32 + (sy * nScreenWidth) + sx;
	UINT32 *pal = konami_palette32 + (color << bpp);

	priority |= 1 << 31; // always on!

	for (INT32 y = 0; y < 16; y++, sy++)
	{
		if (sy >= 0 && sy < nScreenHeight)
		{
			for (INT32 x = 0; x < 16; x++)
			{
				if ((sx+x) >= 0 && (sx+x) < nScreenWidth)
				{
					INT32 pxl = gfx[((y*16)+x)^flip];

					if (pxl) {
						if ((priority & (1 << (pri[x]&0x1f)))==0) {
							if (pri[x] & 0x20) {
								dst[x] = highlight_mode ? highlight_blend(pal[pxl]) : shadow_blend(pal[pxl]);//pal[pxl];
							} else {
								dst[x] = pal[pxl];
							}
						}
						pri[x] |= 0x1f;
					}
				}
			}
		}

		pri += nScreenWidth;
		dst += nScreenWidth;
	}
}

void konami_draw_16x16_tile(UINT8 *gfxbase, INT32 code, INT32 bpp, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	INT32 flip = 0;
	if (flipx) flip |= 0x0f;
	if (flipy) flip |= 0xf0;

	UINT8 *gfx = gfxbase + code * 0x100;

	UINT32 *pal = konami_palette32 + (color << bpp);
	UINT32 *dst = konami_bitmap32 + (sy * nScreenWidth) + sx;

	for (INT32 y = 0; y < 16; y++, sy++)
	{
		if (sy >= 0 && sy < nScreenHeight)
		{
			for (INT32 x = 0; x < 16; x++)
			{
				if ((sx+x) >= 0 && (sx+x) < nScreenWidth)
				{
					INT32 pxl = gfx[((y*16)+x)^flip];

					if (pxl) {
						dst[x] = pal[pxl];
					}
				}
			}
		}

		dst += nScreenWidth;
	}
}

void konami_render_zoom_shadow_tile(UINT8 *gfxbase, INT32 code, INT32 bpp, INT32 color, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, UINT32 priority, INT32 /*highlight*/)
{
	// Based on MAME sources for tile zooming
	UINT8 *gfx_base = gfxbase + (code * width * height);
	int dh = (zoomy * height + 0x8000) / 0x10000;
	int dw = (zoomx * width + 0x8000) / 0x10000;

	INT32 shadow_color = (1 << bpp) - 1;

	UINT32 *pal = konami_palette32 + (color << bpp);

	if (dw && dh)
	{
		int dx = (width * 0x10000) / dw;
		int dy = (height * 0x10000) / dh;
		int ex = sx + dw;
		int ey = sy + dh;
		int x_index_base = 0;
		int y_index = 0;

		if (fx) {
			x_index_base = (dw - 1) * dx;
			dx = -dx;
		}

		if (fy) {
			y_index = (dh - 1) * dy;
			dy = -dy;
		}

		if (priority == 0xffffffff)
		{
			for (INT32 y = sy; y < ey; y++)
			{
				if (y >= 0 && y < nScreenHeight)
				{
					UINT8 *src = gfx_base + (y_index / 0x10000) * width;
					UINT32 *dst = konami_bitmap32 + y * nScreenWidth;
					UINT8 *pri = konami_priority_bitmap + y * nScreenWidth;

					for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
					{
						if (x >= 0 && x < nScreenWidth) {
							INT32 pxl = src[x_index>>16];

							if (pxl) {
								if (pxl == shadow_color) {
									dst[x] = highlight_mode ? highlight_blend(dst[x]) : shadow_blend(dst[x]);
									if (highlight_over_sprites_mode)
										pri[x] |= 0x20;
								} else {
										if (pri[x] & 0x20) {
											dst[x] = highlight_mode ? highlight_blend(dst[x]) : shadow_blend(dst[x]);//pal[pxl];
										} else {
											dst[x] = pal[pxl];
										}
								}
							}
						}

						x_index += dx;
					}
				}

				y_index += dy;
			}
		} else {
			priority |= 1<<31; // always on!

			for (INT32 y = sy; y < ey; y++)
			{
				if (y >= 0 && y < nScreenHeight)
				{
					UINT8 *src = gfx_base + (y_index / 0x10000) * width;
					UINT32 *dst = konami_bitmap32 + y * nScreenWidth;
					UINT8 *pri = konami_priority_bitmap + y * nScreenWidth;

					for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
					{
						if (x >= 0 && x < nScreenWidth) {
							INT32 pxl = src[x_index>>16];

							if (pxl) {
								if (pxl == shadow_color) {
									if ((priority & (1 << (pri[x]&0x1f)))==0 && (pri[x] & 0x80) == 0) {
										dst[x] = highlight_mode ? highlight_blend(dst[x]) : shadow_blend(dst[x]);
										pri[x] = 0x80;
										if (highlight_over_sprites_mode)
											pri[x] |= 0x20;
									}
								} else {
									if ((priority & (1 << (pri[x]&0x1f)))==0) {
										if (pri[x] & 0x20) {
											dst[x] = highlight_mode ? highlight_blend(dst[x]) : shadow_blend(dst[x]);//pal[pxl];
										} else {
											dst[x] = pal[pxl];
										}
										pri[x] = (pri[x]&0x80)|0x1f;
									}
								}
							}
						}

						x_index += dx;
					}
				}

				y_index += dy;
			}
		}
	}
}
