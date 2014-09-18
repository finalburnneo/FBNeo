#include "tiles_generic.h"
#include "konamiic.h"

UINT32 KonamiIC_K051960InUse = 0;
UINT32 KonamiIC_K052109InUse = 0;
UINT32 KonamiIC_K051316InUse = 0;
UINT32 KonamiIC_K053245InUse = 0;
UINT32 KonamiIC_K053247InUse = 0;
UINT32 KonamiIC_K053936InUse = 0;

UINT32 *konami_temp_screen = NULL;
INT32 K05324xZRejection = -1;

UINT32 *konami_palette32;

UINT16 *konami_priority_bitmap = NULL;

void K05324xSetZRejection(INT32 z)
{
	K05324xZRejection = z;
}

UINT8 K052109_051960_r(INT32 offset)
{
	if (K052109RMRDLine == 0)
	{
		if (offset >= 0x3800 && offset < 0x3808)
			return K051937Read(offset - 0x3800);
		else if (offset < 0x3c00)
			return K052109Read(offset);
		else
			return K051960Read(offset - 0x3c00);
	}
	else return K052109Read(offset);
}

void K052109_051960_w(INT32 offset, INT32 data)
{
	if (offset >= 0x3800 && offset < 0x3808)
		K051937Write(offset - 0x3800,data);
	else if (offset < 0x3c00)
		K052109Write(offset,         data);
	else
		K051960Write(offset - 0x3c00,data);
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

// xbbbbbgggggrrrrr (used mostly by Konami-custom cpu games)
void KonamiRecalcPal(UINT8 *src, UINT32 *dst, INT32 len)
{
	UINT8 r,g,b;
	UINT16 *p = (UINT16*)src;
	for (INT32 i = 0; i < len / 2; i++) {
		UINT16 d = BURN_ENDIAN_SWAP_INT16((p[i] << 8) | (p[i] >> 8));

		b = (d >> 10) & 0x1f;
		g = (d >>  5) & 0x1f;
		r = (d >>  0) & 0x1f;

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

	// No init's, so always reset these
	K053251Reset();
	K054000Reset();
	K051733Reset();
}

void KonamiICExit()
{
	if (konami_temp_screen) {
		BurnFree (konami_temp_screen);
		konami_temp_screen = NULL;
	}

	if (konami_priority_bitmap) {
		BurnFree(konami_priority_bitmap);
		konami_priority_bitmap = NULL;
	}

	if (KonamiIC_K051960InUse) K051960Exit();
	if (KonamiIC_K052109InUse) K052109Exit();
	if (KonamiIC_K051316InUse) K051316Exit();
	if (KonamiIC_K053245InUse) K053245Exit();
	if (KonamiIC_K053247InUse) K053247Exit();
	if (KonamiIC_K053936InUse) K053936Exit();

	KonamiIC_K051960InUse = 0;
	KonamiIC_K052109InUse = 0;
	KonamiIC_K051316InUse = 0;
	KonamiIC_K053245InUse = 0;
	KonamiIC_K053247InUse = 0;

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

	K053251Scan(nAction);
	K054000Scan(nAction);
	K051733Scan(nAction);
}


void konami_allocate_bitmaps()
{
	INT32 width, height;
	BurnDrvGetVisibleSize(&width, &height);

	if (konami_temp_screen == NULL) {
		konami_temp_screen = (UINT32*)BurnMalloc(width * height * sizeof(INT32));
	}

	if (konami_priority_bitmap == NULL) {
		konami_priority_bitmap = (UINT16*)BurnMalloc(width * height * sizeof(INT16));
	}
}


void KonamiBlendCopy(UINT32 *pPalette)
{
	pBurnDrvPalette = pPalette;

	UINT32 *bmp = konami_temp_screen;

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		PutPix(pBurnDraw + (i * nBurnBpp), BurnHighCol(bmp[i]>>16, (bmp[i]>>8)&0xff, bmp[i]&0xff, 0));
	}
}

void konami_draw_16x16_priozoom_tile(UINT8 *gfx, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, UINT32 priority)
{
	// Based on MAME sources for tile zooming
	UINT8 *gfx_base = gfx + (code * width * height);
	int dh = (zoomy * height + 0x8000) / 0x10000;
	int dw = (zoomx * width + 0x8000) / 0x10000;

	priority |= 1<<31; // always on!

	UINT32 *pal = konami_palette32 + color;

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
			UINT8 *src = gfx_base + (y_index / 0x10000) * width;
			UINT32 *dst = konami_temp_screen + y * nScreenWidth;
			UINT16 *prio = konami_priority_bitmap + y * nScreenWidth;

			if (y >= 0 && y < nScreenHeight) 
			{
				for (INT32 x = sx, x_index = x_index_base; x < ex; x++)
				{
					if ((priority & (1 << (prio[x] & 0x1f)))==0) {

						if (x >= 0 && x < nScreenWidth) {
							INT32 pxl = src[x_index>>16];

							if (pxl != t) {
								dst[x] = pal[pxl];
								prio[x] = 0x1f;
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

void konami_draw_16x16_zoom_tile(UINT8 *gfxbase, INT32 code, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy)
{
	// Based on MAME sources for tile zooming
	UINT8 *gfx_base = gfxbase + (code * width * height);
	int dh = (zoomy * height + 0x8000) / 0x10000;
	int dw = (zoomx * width + 0x8000) / 0x10000;

	UINT32 *pal = konami_palette32 + color;

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
			UINT8 *src = gfx_base + (y_index / 0x10000) * width;
			UINT32 *dst = konami_temp_screen + y * nScreenWidth;

			if (y >= 0 && y < nScreenHeight) 
			{
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

void konami_draw_16x16_prio_tile(UINT8 *gfxbase, INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, UINT32 priority)
{
	INT32 flip = 0;
	if (flipx) flip |= 0x0f;
	if (flipy) flip |= 0xf0;

	UINT8 *gfx = gfxbase + code * 0x100;

	UINT16 *pri = konami_priority_bitmap + (sy * nScreenWidth) + sx;
	UINT32 *dst = konami_temp_screen + (sy * nScreenWidth) + sx;
	UINT32 *pal = konami_palette32 + color;

	priority |= 1 << 31; // always on!

	for (INT32 y = 0; y < 16; y++, sy++)
	{
		if (sy >= 0 && sy < nScreenHeight)
		{
			for (INT32 x = 0; x < 16; x++)
			{
				if ((sx+x) >=0 && (sx+x) < nScreenWidth)
				{
					INT32 pxl = gfx[((y*16)+x)^flip];

					if (pxl) {
						if ((priority & (1 << (pri[x] & 0x1f)))==0) {
							dst[x] = pal[pxl];
							pri[x] = 0x1f;
						}
					}
				}
			}
		}

		pri += nScreenWidth;
		dst += nScreenWidth;
	}
}

void konami_draw_16x16_tile(UINT8 *gfxbase, INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	INT32 flip = 0;
	if (flipx) flip |= 0x0f;
	if (flipy) flip |= 0xf0;

	UINT8 *gfx = gfxbase + code * 0x100;

	UINT32 *pal = konami_palette32 + color;
	UINT32 *dst = konami_temp_screen + (sy * nScreenWidth) + sx;

	for (INT32 y = 0; y < 16; y++, sy++)
	{
		if (sy >= 0 && sy < nScreenHeight)
		{
			for (INT32 x = 0; x < 16; x++)
			{
				if ((sx+x) >=0 && (sx+x) < nScreenWidth)
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

static inline UINT32 shadow_blend(UINT32 d)
{
	return ((((d & 0xff00ff) * 0x9d) & 0xff00ff00) + (((d & 0x00ff00) * 0x9d) & 0x00ff0000)) / 0x100;
}


/*
// Correct?
static inline UINT32 highlight_blend(UINT32 d)
{
	return (((0xA857A857 + ((d & 0xff00ff) * 0x56)) & 0xff00ff00) + ((0x00A85700 + ((d & 0x00ff00) * 0x56)) & 0x00ff0000)) / 0x100;
}
*/

void konami_render_zoom_shadow_tile(UINT8 *gfxbase, INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, UINT32 priority, INT32 /*highlight*/)
{
	INT32 h = ((zoomy << 4) + 0x8000) >> 16;
	INT32 w = ((zoomx << 4) + 0x8000) >> 16;

	if (!h || !w || sx + w < 0 || sy + h < 0 || sx >= nScreenWidth || sy >= nScreenHeight) return;

	if (fy) fy  = (height-1)*width;
	if (fx) fy |= (width-1);

	INT32 hz = (height << 12) / h;
	INT32 wz = (width << 12) / w;

	INT32 starty = 0, startx = 0, endy = h, endx = w;
	if (sy < 0) starty = 0 - sy;
	if (sx < 0) startx = 0 - sx;
	if (sy + h >= nScreenHeight) endy -= (h + sy) - nScreenHeight;
	if (sx + w >= nScreenWidth ) endx -= (w + sx) - nScreenWidth;

	UINT8  *src = gfxbase + (code * width * height);
	UINT32 *dst = konami_temp_screen + (sy + starty) * nScreenWidth + sx;
	UINT16 *pri = konami_priority_bitmap + (sy + starty) * nScreenWidth + sx;
	UINT32 *pal = konami_palette32 + color;

	if (priority == 0xffffffff)
	{
		for (INT32 y = starty; y < endy; y++)
		{
			if (y >= 0 && y < nScreenHeight)
			{
				INT32 zy = ((y * hz) >> 12) * width;
		
				for (INT32 x = startx; x < endx; x++)
				{
					if ((sx+x) < 0 || (sx+x) >= nScreenWidth) continue;

					INT32 pxl = src[(zy + ((x * wz) >> 12)) ^ fy];
		
					if (pxl) {
						if (pxl == 0x0f) {
							dst[x] = shadow_blend(dst[x]);
						} else {
							dst[x] = pal[pxl];
						}
					} 
				}
			}

			dst += nScreenWidth;
		}
	} else {
		priority |= 1<<31; // always on!

		for (INT32 y = starty; y < endy; y++)
		{
			if (y >= 0 && y < nScreenHeight)
			{	
				INT32 zy = ((y * hz) >> 12) * width;

				for (INT32 x = startx; x < endx; x++)
				{
					if ((sx+x) < 0 || (sx+x) >= nScreenWidth) continue;

					INT32 pxl = src[(zy + ((x * wz) >> 12)) ^ fy];
		
					if (pxl) {
						if (pxl == 0x0f) {
							if ((priority & (1 << (pri[x]&0x1f)))==0 && (pri[x] & 0x80) == 0) {
								//if (highlight) {
								//	dst[x] = highlight_blend(dst[x]);
								//} else {
									dst[x] = shadow_blend(dst[x]);
								//}
								pri[x] |= 0x80;
							}
						} else {
							if ((priority & (1 << (pri[x]&0x1f)))==0) {
								dst[x] = pal[pxl];
								pri[x] = 0x1f;
							}
						}
					} 
				}
			}

			pri += nScreenWidth;
			dst += nScreenWidth;
		}
	}
}

