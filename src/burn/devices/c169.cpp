// Based on MAME sources by Phil Stroffolino

#include "tiles_generic.h"

static UINT32 size;
static INT32 color;
static INT32 priority;

static INT32 left;
static INT32 top;
static INT32 incxx;
static INT32 incxy;
static INT32 incyx;
static INT32 incyy;

static INT32 startx=0;
static INT32 starty=0;

static INT32 clip_min_y;
static INT32 clip_max_y;
static INT32 clip_min_x;
static INT32 clip_max_x;

static INT32 global_priority = 0;

static UINT8 *roz_ram;
static UINT8 *roz_ctrl;
static UINT16 *roz_bitmap;

void c169_roz_init(UINT8 *ram, UINT8 *control, UINT16 *bitmap)
{
	roz_ram = ram;
	roz_ctrl = control;
	roz_bitmap = bitmap;
}	

static void c169_roz_unpack_params(const UINT16 *source)
{
	const INT32 xoffset = 36, yoffset = 3;

	UINT16 temp = source[1];
	size = 512 << ((temp & 0x0300) >> 8);
	color = (temp & 0x000f) * 256;
	priority = (temp & 0x00f0) >> 4;

	temp = source[2];
	left = (temp & 0x7000) >> 3;
	if (temp & 0x8000) temp |= 0xf000; else temp &= 0x0fff; // sign extend
	incxx = (INT16)temp;

	temp = source[3];
	top = (temp&0x7000)>>3;
	if (temp & 0x8000) temp |= 0xf000; else temp &= 0x0fff; // sign extend
	incxy = (INT16)temp;

	temp = source[4];
	if (temp & 0x8000) temp |= 0xf000; else temp &= 0x0fff; // sign extend
	incyx = (INT16)temp;

	temp = source[5];
	if (temp & 0x8000) temp |= 0xf000; else temp &= 0x0fff; // sign extend
	incyy = (INT16)temp;

	startx = (INT16)source[6];
	starty = (INT16)source[7];
	startx <<= 4;
	starty <<= 4;

	startx += xoffset * incxx + yoffset * incyx;
	starty += xoffset * incxy + yoffset * incyy;

	startx <<= 8;
	starty <<= 8;
	incxx <<= 8;
	incxy <<= 8;
	incyx <<= 8;
	incyy <<= 8;
}

static void c169_roz_draw_helper()
{
	UINT32 size_mask = size - 1;
	UINT16 *srcbitmap = roz_bitmap;
	UINT32 hstartx = startx + clip_min_x * incxx + clip_min_y * incyx;
	UINT32 hstarty = starty + clip_min_x * incxy + clip_min_y * incyy;
	INT32 sx = clip_min_x;
	INT32 sy = clip_min_y;
	while (sy <= clip_max_y)
	{
		INT32 x = sx;
		UINT32 cx = hstartx;
		UINT32 cy = hstarty;
		UINT16 *dest = pTransDraw + (sy * nScreenWidth) + sx;
		UINT8 *prio = pPrioDraw + (sy * nScreenWidth) + sx;
		while (x <= clip_max_x)
		{
			UINT32 xpos = (((cx >> 16) & size_mask) + left) & 0xfff;
			UINT32 ypos = (((cy >> 16) & size_mask) + top) & 0xfff;
			INT32 pxl = srcbitmap[(ypos * 0x1000) + xpos];
			if ((pxl & 0x8000) == 0) {
				*dest = srcbitmap[(ypos * 0x1000) + xpos] + color;
				*prio = global_priority;
			}
			cx += incxx;
			cy += incxy;
			x++;
			dest++;
			prio++;
		}
		hstartx += incyx;
		hstarty += incyy;
		sy++;
	}
}

static void c169_roz_draw_scanline(INT32 line, INT32 pri)
{
	if (line >= 0 && line <= clip_max_y) // namco's clipping is 1 less for max_*
	{
		INT32 row = line / 8;
		INT32 offs = row * 0x100 + (line & 7) * 0x10 + 0xe080;
		UINT16 *source = (UINT16*)(roz_ram + offs);

		if ((source[1] & 0x8000) == 0)
		{
			if (pri == priority)
			{
				c169_roz_unpack_params(source);

				INT32 oldmin = clip_min_y;
				INT32 oldmax = clip_max_y;

				clip_min_y = line;
				clip_max_y = line+1;

				c169_roz_draw_helper();

				clip_min_y = oldmin;
				clip_max_y = oldmax;
			}
		}
	}
}

void c169_roz_draw(INT32 pri, INT32 line)
{
	GenericTilesGetClip(&clip_min_x, &clip_max_x, &clip_min_y, &clip_max_y);

	if (line != -1) {
		if (line >= clip_min_y && line <= clip_max_y) {
			clip_min_y = line;
			clip_max_y = line+1;
		} else {
			return; // nothing to draw due to clipping
		}
	}

	const UINT16 *source = (UINT16*)roz_ctrl;

	INT32 mode = source[0]; // 0x8000 or 0x1000

	global_priority = pri;

	for (INT32 which = 1; which >= 0; which--)
	{
		UINT16 attrs = source[1 + (which*8)];
		if ((attrs & 0x8000) == 0)
		{
			if (which == 1 && mode == 0x8000)
			{
				for (INT32 scanline = clip_min_y; scanline <= clip_max_y; scanline++)
					c169_roz_draw_scanline(scanline, pri);
			}
			else
			{
				c169_roz_unpack_params(source + (which*8));
				if (priority == pri)
					c169_roz_draw_helper();
			}
		}
	}
}

