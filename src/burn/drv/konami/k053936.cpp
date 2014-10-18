#include "tiles_generic.h"
#include "konamiic.h"

#define MAX_K053936	2

static INT32 nRamLen[MAX_K053936] = { 0, 0 };
static INT32 nWidth[MAX_K053936] = { 0, 0 };
static INT32 nHeight[MAX_K053936] = { 0, 0 };
static UINT16 *tscreen[MAX_K053936] = { NULL, NULL };
static UINT8 *ramptr[MAX_K053936] = { NULL, NULL };
static UINT8 *rambuf[MAX_K053936] = { NULL, NULL };

static INT32 K053936Wrap[MAX_K053936] = { 0, 0 };
static INT32 K053936Offset[MAX_K053936][2] = { { 0, 0 }, { 0, 0 } };

static void (*pTileCallback0)(INT32 offset, UINT16 *ram, INT32 *code, INT32 *color, INT32 *sx, INT32 *sy, INT32 *fx, INT32 *fy);
static void (*pTileCallback1)(INT32 offset, UINT16 *ram, INT32 *code, INT32 *color, INT32 *sx, INT32 *sy, INT32 *fx, INT32 *fy);

void K053936Reset()
{
	for (INT32 i = 0; i < MAX_K053936; i++) {
		if (rambuf[i]) {
			memset (rambuf[i], 0, nRamLen[i]);
		}
	}
}

void K053936Init(INT32 chip, UINT8 *ram, INT32 len, INT32 w, INT32 h, void (*pCallback)(INT32 offset, UINT16 *ram, INT32 *code, INT32 *color, INT32 *sx, INT32 *sy, INT32 *fx, INT32 *fy))
{
	ramptr[chip] = ram;

	nRamLen[chip] = len;

	if (rambuf[chip] == NULL) {
		rambuf[chip] = (UINT8*)BurnMalloc(len);
	}

	nWidth[chip] = w;
	nHeight[chip] = h;

	if (tscreen[chip] == NULL) {
		tscreen[chip] = (UINT16*)BurnMalloc(w * h * 2);
	}

	if (chip == 0) {
		pTileCallback0 = pCallback;
	}
	if (chip == 1) {
		pTileCallback1 = pCallback;
	}

	KonamiAllocateBitmaps();

	KonamiIC_K053936InUse = 1;
}

void K053936Exit()
{
	for (INT32 i = 0; i < MAX_K053936; i++) {
		nRamLen[i] = 0;
		nWidth[i] = 0;
		nHeight[i] = 0;
		BurnFree (tscreen[i]);
		ramptr[i] = NULL;
		BurnFree (rambuf[i]);
		K053936Wrap[i] = 0;
		K053936Offset[i][0] = K053936Offset[i][1] = 0;
	}
}

void K053936PredrawTiles2(INT32 chip, UINT8 *gfx)
{
	UINT16 *ram = (UINT16*)ramptr[chip];
	UINT16 *buf = (UINT16*)rambuf[chip];

	for (INT32 i = 0; i < (nWidth[chip] / 16) * (nHeight[chip] / 16); i++)
	{
		if (ram[(i*2)+0] != buf[(i*2)+0] || ram[(i*2)+1] != buf[(i*2)+1]) 
		{
			INT32 sx = (i % (nWidth[chip]/16)) * 16;
			INT32 sy = i / (nWidth[chip]/16) * 16;
			INT32 code = 0;
			INT32 color = 0;
			INT32 flipx = 0;
			INT32 flipy = 0;

			if (chip) {
				pTileCallback1(i, ram, &code, &color, &sx, &sy, &flipx, &flipy);
			} else {
				pTileCallback0(i, ram, &code, &color, &sx, &sy, &flipx, &flipy);
			}

			// draw tile
			{
				INT32 flip = 0;
				if (flipx) flip  = 0x0f;
				if (flipy) flip |= 0xf0;
			
				UINT8 *src = gfx + (code * 16 * 16);
				UINT16 *dst = tscreen[chip] + (sy * nWidth[chip]) + sx;
	
				for (INT32 y = 0; y < 16; y++) {
					for (INT32 x = 0; x < 16; x++) {
						INT32 pxl = src[((y << 4) | x) ^ flip];
						if (pxl == 0) pxl |= 0x8000;
						dst[x] = (pxl | color);
					}
					dst += nWidth[chip];
				}
			}
		}
		buf[(i*2)+0] = ram[(i*2)+0];
		buf[(i*2)+1] = ram[(i*2)+1];
	}
}

void K053936PredrawTiles(INT32 chip, UINT8 *gfx, INT32 transparent, INT32 tcol)
{
	INT32 twidth = nWidth[chip];
	UINT16 *ram = (UINT16*)ramptr[chip];
	UINT16 *buf = (UINT16*)rambuf[chip];

	for (INT32 i = 0; i < nRamLen[chip] / 2; i++)
	{
		if (ram[i] != buf[i]) {
			INT32 sx = 0;
			INT32 sy = 0;
			INT32 code = 0;
			INT32 color = 0;
			INT32 fx = 0;
			INT32 fy = 0;
		
			if (chip) {
				pTileCallback1(i, ram, &code, &color, &sx, &sy, &fx, &fy);
			} else {
				pTileCallback0(i, ram, &code, &color, &sx, &sy, &fx, &fy);
			}

			if (code == -1) continue;
		
			// draw tile
			{
				if (fy) fy  = 0xf0;
				if (fx) fy |= 0x0f;
		
				UINT8 *src = gfx + (code * 16 * 16);
				UINT16 *sdst = tscreen[chip] + (sy * twidth) + sx;
		
				for (INT32 y = 0; y < 16; y++) {
					for (INT32 x = 0; x < 16; x++) {
						INT32 pxl = src[((y << 4) | x) ^ fy];
						if (transparent) {
							if (pxl == tcol) pxl |= 0x8000;
						}
		
						sdst[x] = pxl | color;
					}
					sdst += twidth;
				}
			}
		}
		buf[i] = ram[i];
	}
}

static inline void copy_roz(INT32 chip, INT32 minx, INT32 maxx, INT32 miny, INT32 maxy, UINT32 startx, UINT32 starty, INT32 incxx, INT32 incxy, INT32 incyx, INT32 incyy, INT32 transp, INT32 priority)
{
	if (incxx == (1 << 16) && incxy == 0 && incyx == 0 && incyy == (1 << 16) && K053936Wrap[chip])
	{
		INT32 scrollx = startx >> 16;
		INT32 scrolly = starty >> 16;

		for (INT32 sy = 0; sy < nScreenHeight; sy++) {
			UINT8  *pri = konami_priority_bitmap + (sy * nScreenWidth);
			UINT16 *src = tscreen[chip] + (((scrolly + sy) % nHeight[chip]) * nWidth[chip]);
			UINT32 *dst = konami_bitmap32 + (sy * nScreenWidth);

			for (INT32 sx = 0; sx < nScreenWidth; sx++) {
				INT32 pxl = src[(scrollx+sx)%nWidth[chip]];
				if ((pxl & 0x8000)!=0 && transp) continue;

				dst[sx] = konami_palette32[pxl & 0x7fff];
				pri[sx] = priority;
			}
		}
		return;
	}

	UINT8  *pri = konami_priority_bitmap;
	UINT32 *dst = konami_bitmap32;
	UINT32 *pal = konami_palette32;
	UINT16 *src = tscreen[chip];

	INT32 hmask = nHeight[chip] - 1;
	INT32 wmask = nWidth[chip] - 1;

	INT32 wrap = K053936Wrap[chip];

	for (INT32 sy = miny; sy < maxy; sy++, startx+=incyx, starty+=incyy)
	{
		UINT32 cx = startx;
		UINT32 cy = starty;

		if (transp) {
			if (wrap) {
				for (INT32 x = minx; x < maxx; x++, cx+=incxx, cy+=incxy, dst++, pri++)
				{
					INT32 pxl = src[(((cy >> 16) & hmask) << 10) + ((cx >> 16) & wmask)];
		
					if (!(pxl & 0x8000)) {
						*dst = pal[pxl];
						*pri = priority;
					}
				}
			} else {
				for (INT32 x = minx; x < maxx; x++, cx+=incxx, cy+=incxy, dst++, pri++)
				{
					INT32 yy = cy >> 16;
					if (yy > hmask || yy < 0) continue;
					INT32 xx = cx >> 16;
					if (xx > wmask || xx < 0) continue;

					INT32 pxl = src[(yy << 10) + xx];

					if (!(pxl & 0x8000)) {
						*dst = pal[pxl];
						*pri = priority;
					}
				}
			}
		} else {
			if (wrap) {
				for (INT32 x = minx; x < maxx; x++, cx+=incxx, cy+=incxy, dst++, pri++) {
					*dst = pal[src[(((cy >> 16) & hmask) << 10) + ((cx >> 16) & wmask)] & 0x7fff];
					*pri = priority;
				}
			} else {
				for (INT32 x = minx; x < maxx; x++, cx+=incxx, cy+=incxy, dst++, pri++) {
					INT32 yy = cy >> 16;
					if (yy > hmask || yy < 0) continue;
					INT32 xx = cx >> 16;
					if (xx > wmask || xx < 0) continue;

					*dst = pal[src[(yy << 10) + xx] & 0x7fff];
					*pri = priority;
				}
			}
		}
	}
}

void K053936Draw(INT32 chip, UINT16 *ctrl, UINT16 *linectrl, INT32 flags)
{
	INT32 transp = flags & 0xff;
	INT32 priority = (flags >> 8) & 0xff;

	if (ctrl[0x07] & 0x0040 && linectrl)	// Super!
	{
		UINT32 startx,starty;
		INT32 incxx,incxy;

		INT32 minx, maxx, maxy, miny, y;

		if ((ctrl[0x07] & 0x0002) && ctrl[0x09])	// glfgreat
		{
			minx = ctrl[0x08] + K053936Offset[chip][0]+2;
			maxx = ctrl[0x09] + K053936Offset[chip][0]+2 - 1;
			if (minx < 0) minx = 0;
			if (maxx > nScreenWidth) maxx = nScreenWidth;

			y = ctrl[0x0a] + K053936Offset[chip][1]-2;
			if (y < 0) y = 0;
			maxy = ctrl[0x0b] + K053936Offset[chip][1]-2 - 1;
			if (maxy > nScreenHeight) maxy = nScreenHeight;
		}
		else
		{
			minx = 0;
			maxx = nScreenWidth;

			y = 0;
			maxy = nScreenHeight;
		}

		while (y <= maxy)
		{
			UINT16 *lineaddr = linectrl + 4*((y - K053936Offset[chip][1]) & 0x1ff);
			miny = maxy = y;

			startx = 256 * (INT16)(lineaddr[0] + ctrl[0x00]);
			starty = 256 * (INT16)(lineaddr[1] + ctrl[0x01]);
			incxx  =       (INT16)(lineaddr[2]);
			incxy  =       (INT16)(lineaddr[3]);

			if (ctrl[0x06] & 0x8000) incxx *= 256;
			if (ctrl[0x06] & 0x0080) incxy *= 256;

			startx -= K053936Offset[chip][0] * incxx;
			starty -= K053936Offset[chip][0] * incxy;

			copy_roz(chip, minx, maxx, miny, maxy, startx << 5, starty << 5, incxx << 5, incxy << 5, 0, 0, transp, priority);

			y++;
		}
	}
	else	// simple
	{
		UINT32 startx, starty;
		INT32 incxx, incxy, incyx, incyy;

		startx = 256 * (INT16)(ctrl[0x00]);
		starty = 256 * (INT16)(ctrl[0x01]);
		incyx  =       (INT16)(ctrl[0x02]);
		incyy  =       (INT16)(ctrl[0x03]);
		incxx  =       (INT16)(ctrl[0x04]);
		incxy  =       (INT16)(ctrl[0x05]);

		if (ctrl[0x06] & 0x4000)
		{
			incyx *= 256;
			incyy *= 256;
		}

		if (ctrl[0x06] & 0x0040)
		{
			incxx *= 256;
			incxy *= 256;
		}

		startx -= K053936Offset[chip][1] * incyx;
		starty -= K053936Offset[chip][1] * incyy;

		startx -= K053936Offset[chip][0] * incxx;
		starty -= K053936Offset[chip][0] * incxy;

		copy_roz(chip, 0, nScreenWidth, 0, nScreenHeight, startx << 5, starty << 5, incxx << 5, incxy << 5, incyx << 5, incyy << 5, transp, priority);
	}
}

void K053936EnableWrap(INT32 chip, INT32 status)
{
	K053936Wrap[chip] = status;
}

void K053936SetOffset(INT32 chip, INT32 xoffs, INT32 yoffs)
{
	K053936Offset[chip][0] = xoffs;
	K053936Offset[chip][1] = yoffs;
}

void K053936Scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(K053936Wrap[0]);
		SCAN_VAR(K053936Wrap[1]);
	}
}


// this is not safe to hook up to any other driver than mystwarr!!!!!!!!

static INT32 K053936_clip_enabled[2];
static INT32 K053936_cliprect[2][4];
static INT32 K053936_offset[2][2];
static INT32 K053936_enable[2] = { 0, 0 };
static INT32 K053936_color[2] = { 0, 0 };
UINT16 *m_k053936_0_ctrl_16 = NULL;
UINT16 *m_k053936_0_linectrl_16 = NULL;
UINT16 *m_k053936_0_ctrl = NULL;
UINT16 *m_k053936_0_linectrl = NULL;
UINT16 *K053936_external_bitmap = NULL;

void K053936GpInit()
{

}

void K053936GPExit()
{
	K053936_clip_enabled[0] = K053936_clip_enabled[1] = 0;
	K053936_enable[0] = K053936_enable[1] = 0;
	K053936_color[0] = K053936_color[1] = 0;
	memset(K053936_cliprect,0,sizeof(K053936_cliprect)); //2*4*sizeof(INT32));
	memset(K053936_offset,0,sizeof(K053936_offset)); //2*2*sizeof(INT32));
	m_k053936_0_ctrl_16 = NULL;
	m_k053936_0_linectrl_16 = NULL;
	m_k053936_0_ctrl = NULL;
	m_k053936_0_linectrl = NULL;
	K053936_external_bitmap = NULL;
}

static inline UINT32 alpha_blend(UINT32 d, UINT32 s, UINT32 p)
{
	if (p == 0) return d;

	INT32 a = 256 - p;

	return (((((s & 0xff00ff) * p) + ((d & 0xff00ff) * a)) & 0xff00ff00) |
		((((s & 0x00ff00) * p) + ((d & 0x00ff00) * a)) & 0x00ff0000)) >> 8;
}

void K053936GP_set_colorbase(INT32 chip, INT32 color_base)
{
	K053936_color[chip] = color_base;
}

void K053936GP_enable(int chip, int enable)
{
	K053936_enable[chip] = enable;
}

void K053936GP_set_offset(int chip, int xoffs, int yoffs) { K053936_offset[chip][0] = xoffs; K053936_offset[chip][1] = yoffs; }

void K053936GP_clip_enable(int chip, int status) { K053936_clip_enabled[chip] = status; }

void K053936GP_set_cliprect(int chip, int minx, int maxx, int miny, int maxy)
{
	K053936_cliprect[chip][0] = minx;
	K053936_cliprect[chip][1] = maxx;
	K053936_cliprect[chip][2] = miny;
	K053936_cliprect[chip][3] = maxy;	
}

static inline void K053936GP_copyroz32clip(INT32 chip, UINT16 *src_bitmap, INT32 *my_clip, UINT32 _startx,UINT32 _starty,int _incxx,int _incxy,int _incyx,int _incyy,
		int tilebpp, int blend, int alpha, int clip, int pixeldouble_output)
{
	static const int colormask[8]={1,3,7,0xf,0x1f,0x3f,0x7f,0xff};
	int cy, cx;
	int ecx;
	int src_pitch, incxy, incxx;
	int src_minx, src_maxx, src_miny, src_maxy, cmask;
	UINT16 *src_base;
	INT32 src_size;

	const UINT32 *pal_base;
	int dst_ptr;
	int dst_size;
	int dst_base2;

	int tx, dst_pitch;
	UINT32 *dst_base;
	int starty, incyy, startx, incyx, ty, sx, sy;

	incxy = _incxy; incxx = _incxx; incyy = _incyy; incyx = _incyx;
	starty = _starty; startx = _startx;

	INT32 color_base = K053936_color[chip];

	if (clip) // set source clip range to some extreme values when disabled
	{
		src_minx = K053936_cliprect[chip][0];
		src_maxx = K053936_cliprect[chip][1];
		src_miny = K053936_cliprect[chip][2];
		src_maxy = K053936_cliprect[chip][3];
	}
	// this simply isn't safe to do!
	else { src_minx = src_miny = -0x10000; src_maxx = src_maxy = 0x10000; }

	// set target clip range
	sx = my_clip[0];
	tx = my_clip[1] - sx + 1;
	sy = my_clip[2];
	ty = my_clip[3] - sy + 1;

	startx += sx * incxx + sy * incyx;
	starty += sx * incxy + sy * incyy;

	// adjust entry points and other loop constants
	dst_pitch = nScreenWidth;
	dst_base = konami_bitmap32;
	dst_base2 = sy * dst_pitch + sx + tx;
	ecx = tx = -tx;

	tilebpp = (tilebpp-1) & 7;
	pal_base = konami_palette32;
	cmask = colormask[tilebpp];

	src_pitch = 0x2000;
	src_base = src_bitmap;
	src_size = 0x2000 * 0x2000;
	dst_size = nScreenWidth * nScreenHeight;
	dst_ptr = 0;//dst_base;
	cy = starty;
	cx = startx;

	if (blend > 0)
	{
		dst_base += dst_pitch;      // draw blended
		starty += incyy;
		startx += incyx;

		do {
			do {
				int srcx = (cx >> 16) & 0x1fff;
				int srcy = (cy >> 16) & 0x1fff;
				int pixel;
				UINT32 offs;
				offs = srcy * src_pitch + srcx;

				cx += incxx;
				cy += incxy;

				if (offs>=src_size||offs<0)
					continue;

				if (srcx < src_minx || srcx > src_maxx || srcy < src_miny || srcy > src_maxy)
					continue;

				pixel = src_base[offs]|color_base;
				if (!(pixel & cmask))
					continue;
// this one below is fucked.
				if ((dst_ptr+ecx+dst_base2+dst_pitch)<dst_size) dst_base[dst_ptr+ecx+dst_base2] = alpha_blend(pal_base[pixel], dst_base[dst_ptr+ecx+dst_base2], alpha);

				if (pixeldouble_output)
				{
					ecx++;
					if ((dst_ptr+ecx+dst_base2+dst_pitch)<dst_size) dst_base[dst_ptr+ecx+dst_base2] = alpha_blend(pal_base[pixel], dst_base[dst_ptr+ecx+dst_base2], alpha);
				}
			}
			while (++ecx < 0);

			ecx = tx;
			dst_ptr += dst_pitch;
			cy = starty; starty += incyy;
			cx = startx; startx += incyx;
		} while (--ty);
	}
	else    //  draw solid
	{
		if (blend == 0)
		{
			dst_ptr += dst_pitch;
			starty += incyy;
			startx += incyx;
		}
		else
		{
			if ((sy & 1) ^ (blend & 1))
			{
				if (ty <= 1) return;

				dst_ptr += dst_pitch;
				cy += incyy;
				cx += incyx;
			}

			if (ty > 1)
			{
				ty >>= 1;
				dst_pitch <<= 1;
				incyy <<= 1;
				incyx <<= 1;

				dst_ptr += dst_pitch;
				starty = cy + incyy;
				startx = cx + incyx;
			}
		}

		do {
			do {
				int srcx = (cx >> 16) & 0x1fff;
				int srcy = (cy >> 16) & 0x1fff;
				int pixel;
				UINT32 offs;

				offs = srcy * src_pitch + srcx;

				cx += incxx;
				cy += incxy;

				if (offs>=src_size||offs<0)
					continue;

				if (srcx < src_minx || srcx > src_maxx || srcy < src_miny || srcy > src_maxy)
					continue;

				pixel = src_base[offs]|color_base;
				if (!(pixel & cmask))
					continue;

				if ((dst_ptr+ecx+dst_base2)<dst_size) dst_base[dst_ptr+ecx+dst_base2] = pal_base[pixel];

				if (pixeldouble_output)
				{
					ecx++;
					if ((dst_ptr+ecx+dst_base2)<dst_size) dst_base[dst_ptr+ecx+dst_base2] = pal_base[pixel];
				}
			}
			while (++ecx < 0);

			ecx = tx;
			dst_ptr += dst_pitch;
			cy = starty; starty += incyy;
			cx = startx; startx += incyx;
		} while (--ty);
	}
}

static void K053936GP_zoom_draw(int chip, UINT16 *ctrl, UINT16 *linectrl, UINT16 *src_bitmap,
		int tilebpp, int blend, int alpha, int pixeldouble_output)
{
	UINT16 *lineaddr;

	UINT32 startx, starty;
	int incxx, incxy, incyx, incyy, y, maxy, clip;

	clip = K053936_clip_enabled[chip];

	INT32 my_clip[4];
	my_clip[0] = 0;
	my_clip[1] = nScreenWidth - 1;
	my_clip[2] = 0;
	my_clip[3] = nScreenHeight - 1;

	if (ctrl[0x07] & 0x0040)    /* "super" mode */
	{
		y = 0; //cliprect.min_y;
		maxy = my_clip[3];

		while (y <= maxy)
		{
			lineaddr = linectrl + ( ((y - K053936_offset[chip][1]) & 0x1ff) << 2);
			my_clip[2] = my_clip[3] = y;

			startx = (INT16)(lineaddr[0] + ctrl[0x00]) << 8;
			starty = (INT16)(lineaddr[1] + ctrl[0x01]) << 8;
			incxx  = (INT16)(lineaddr[2]);
			incxy  = (INT16)(lineaddr[3]);

			if (ctrl[0x06] & 0x8000) incxx <<= 8;
			if (ctrl[0x06] & 0x0080) incxy <<= 8;

			startx -= K053936_offset[chip][0] * incxx;
			starty -= K053936_offset[chip][0] * incxy;

			K053936GP_copyroz32clip(chip, src_bitmap, my_clip,
					startx<<5, starty<<5, incxx<<5, incxy<<5, 0, 0,
					tilebpp, blend, alpha, clip, pixeldouble_output);
			y++;
		}
	}
	else    /* "simple" mode */
	{
		startx = (INT16)(ctrl[0x00]) << 8;
		starty = (INT16)(ctrl[0x01]) << 8;
		incyx  = (INT16)(ctrl[0x02]);
		incyy  = (INT16)(ctrl[0x03]);
		incxx  = (INT16)(ctrl[0x04]);
		incxy  = (INT16)(ctrl[0x05]);

		if (ctrl[0x06] & 0x4000) { incyx <<= 8; incyy <<= 8; }
		if (ctrl[0x06] & 0x0040) { incxx <<= 8; incxy <<= 8; }

		startx -= K053936_offset[chip][1] * incyx;
		starty -= K053936_offset[chip][1] * incyy;

		startx -= K053936_offset[chip][0] * incxx;
		starty -= K053936_offset[chip][0] * incxy;

		K053936GP_copyroz32clip(chip, src_bitmap, my_clip,
				startx<<5, starty<<5, incxx<<5, incxy<<5, incyx<<5, incyy<<5,
				tilebpp, blend, alpha, clip, pixeldouble_output);
	}
}

void K053936GP_0_zoom_draw(UINT16 *bitmap, int tilebpp, int blend, int alpha, int pixeldouble_output, UINT16* temp_m_k053936_0_ctrl_16, UINT16* temp_m_k053936_0_linectrl_16,UINT16* temp_m_k053936_0_ctrl, UINT16* temp_m_k053936_0_linectrl)
{
	if (K053936_enable[0] == 0) return;

	if (temp_m_k053936_0_ctrl_16)
	{
		K053936GP_zoom_draw(0,temp_m_k053936_0_ctrl_16,temp_m_k053936_0_linectrl_16,bitmap,tilebpp,blend,alpha, pixeldouble_output);
	}
	else
	{
		K053936GP_zoom_draw(0,temp_m_k053936_0_ctrl,   temp_m_k053936_0_linectrl,   bitmap,tilebpp,blend,alpha, pixeldouble_output);
	}
}
