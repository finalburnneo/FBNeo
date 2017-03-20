// license:BSD-3-Clause
// copyright-holders:David Haywood

// k053247

#include "tiles_generic.h"
#include "konamiic.h"

#define K053247_CUSTOMSHADOW	0x20000000
#define K053247_SHDSHIFT		20

static UINT8  K053246Regs[8];
static UINT8  K053246_OBJCHA_line;
UINT8 *K053247Ram;
static UINT16 K053247Regs[16];

static UINT8 *K053246Gfx;
static UINT32   K053246Mask;
static UINT8 *K053246GfxExp;
static UINT32   K053246MaskExp;

static INT32 K053247_dx;
static INT32 K053247_dy;
static INT32 K053247_wraparound;

static INT32 nBpp = 4;

static INT32 K053247Flags;

void (*K053247Callback)(INT32 *code, INT32 *color, INT32 *priority);

void K053247Reset()
{
	memset (K053247Ram,  0, 0x1000);
	memset (K053247Regs, 0, 16 * sizeof (UINT16));
	memset (K053246Regs, 0, 8);

	K053246_OBJCHA_line = 0; // clear
}

void K053247Scan(INT32 nAction)
{
	struct BurnArea ba;
	
	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = K053247Ram;
		ba.nLen	  = 0x1000;
		ba.szName = "K053247 Ram";
		BurnAcb(&ba);

		ba.Data	  = K053247Regs;
		ba.nLen	  = 0x0010 * sizeof(UINT16);
		ba.szName = "K053247 Regs";
		BurnAcb(&ba);

		ba.Data	  = K053246Regs;
		ba.nLen	  = 0x0008;
		ba.szName = "K053246 Regs";
		BurnAcb(&ba);

		SCAN_VAR(K053246_OBJCHA_line);
		SCAN_VAR(K053247_wraparound);
	}
}

void K053247Init(UINT8 *gfxrom, UINT8 *gfxromexp, INT32 gfxlen, void (*Callback)(INT32 *code, INT32 *color, INT32 *priority), INT32 flags)
{
	K053247Ram = (UINT8*)BurnMalloc(0x1000);

	K053246Gfx = gfxrom;
	K053246Mask = gfxlen;

	K053246GfxExp = gfxromexp;
	K053246MaskExp = ((gfxlen * 2) + 1) / 0x100;

	K053247Callback = Callback;

	K053247_dx = 0;
	K053247_dy = 0;
	K053247_wraparound = 1;

	KonamiAllocateBitmaps();

	K053247Flags = flags; // 0x02 highlight, 0x01 shadow

	KonamiIC_K053247InUse = 1;

	nBpp = 4;
}

void K053247SetBpp(INT32 bpp)
{
	nBpp = bpp;
}

void K053247Exit()
{
	BurnFree (K053247Ram);

	K053247Flags = 0;

	memset (K053247Regs, 0, 16 * sizeof(UINT16));
}

void K053247Export(UINT8 **ram, UINT8 **gfx, void (**callback)(INT32 *, INT32 *, INT32 *), INT32 *dx, INT32 *dy)
{
	if (ram) *ram = K053247Ram;
	if (gfx) *gfx = K053246Gfx;

	if (dx)	*dx = K053247_dx;
	if (dy)	*dy = K053247_dy;

	if(callback) *callback = K053247Callback;
}

void K053247GfxDecode(UINT8 *src, UINT8 *dst, INT32 len) // 16x16
{
	for (INT32 i = 0; i < len; i++)
	{
		INT32 t = src[i^1];
		dst[(i << 1) + 0] = t >> 4;
		dst[(i << 1) + 1] = t & 0x0f;
	}
}

void K053247SetSpriteOffset(INT32 offsx, INT32 offsy)
{
	K053247_dx = offsx;
	K053247_dy = offsy;
}

void K053247WrapEnable(INT32 status)
{
	K053247_wraparound = status;
}

UINT8 K053247Read(INT32 offset)
{
	return K053247Ram[offset & 0xfff];
}

UINT16 K053247ReadWord(INT32 offset)
{
	return *((UINT16*)(K053247Ram + (offset & 0xffe)));
}

void K053247WriteWord(INT32 offset, UINT16 data)
{
	*((UINT16*)(K053247Ram + (offset & 0xffe))) = BURN_ENDIAN_SWAP_INT16(data);
}

void K053247Write(INT32 offset, INT32 data)
{
	if (data & 0x10000) { // use word
		*((UINT16*)(K053247Ram + (offset & 0xffe))) = BURN_ENDIAN_SWAP_INT16(data);
	} else {
		K053247Ram[offset & 0xfff] = data;
	}
}

void K053247WriteRegsByte(INT32 offset, UINT8 data)
{
	UINT8 *regs = (UINT8*)&K053247Regs;

	regs[(offset & 0x1f)^1] = data;
}

void K053247WriteRegsWord(INT32 offset, UINT16 data)
{
	K053247Regs[(offset & 0x1e) / 2] = data;
}

UINT16 K053247ReadRegs(INT32 offset)
{
	return K053247Regs[offset & 0xf];
}

UINT16 K053246ReadRegs(INT32 offset)
{
	return K053246Regs[offset & 7];
}

UINT8 K053246Read(INT32 offset)
{
	if (K053246_OBJCHA_line) // assert_line
	{
		INT32 addr;

		addr = (K053246Regs[6] << 17) | (K053246Regs[7] << 9) | (K053246Regs[4] << 1) | ((offset & 1) ^ 1);
		addr &= K053246Mask;

		return K053246Gfx[addr];
	}
	else
	{
		return 0;
	}
}

void K053246Write(INT32 offset, INT32 data)
{
	if (data & 0x10000) { // handle it as a word
		*((UINT16*)(K053246Regs + (offset & 6))) = BURN_ENDIAN_SWAP_INT16(data);
	} else {
		K053246Regs[offset & 7] = data;
	}
}

void K053246_set_OBJCHA_line(INT32 state)
{
	K053246_OBJCHA_line = state;
}

INT32 K053246_is_IRQ_enabled()
{
	return K053246Regs[5] & 0x10;
}

void K053247SpritesRender()
{
#define NUM_SPRITES 256


	UINT8 dtable[256];
	UINT8 stable[256];
	UINT8 *wtable;

	memset(dtable, 1, 256);
	dtable[0] = 0;
	memset(stable, 2, 256);
	stable[0] = 0;

	static const INT32 xoffset[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };
	static const INT32 yoffset[8] = { 0, 2, 8, 10, 32, 34, 40, 42 };

	INT32 sortedlist[NUM_SPRITES];
	INT32 offs,zcode;
	INT32 ox,oy,color,code,size,w,h,x,y,xa,ya,flipx,flipy,mirrorx,mirrory,shadow,zoomx,zoomy,primask;
	INT32 nozoom,count,temp,shdmask;

	INT32 flipscreenx = K053246Regs[5] & 0x01;
	INT32 flipscreeny = K053246Regs[5] & 0x02;
	INT32 offx = (INT16)((K053246Regs[0] << 8) | K053246Regs[1]);
	INT32 offy = (INT16)((K053246Regs[2] << 8) | K053246Regs[3]);

	UINT16 *SprRam = (UINT16*)K053247Ram;

	UINT8 *gfxbase = K053246GfxExp;

	INT32 screen_width = nScreenWidth-1;

	if (K053247Flags & 1) {
		if (K053247Flags & 2) {
			shdmask = 3;
		} else {
			shdmask = 0;
		}
	} else {
		shdmask = -1;
	}

	// Prebuild a sorted table by descending Z-order.
	zcode = K05324xZRejection;
	offs = count = 0;

	if (zcode == -1)
	{
		for (; offs<0x800; offs+=8)
			if (BURN_ENDIAN_SWAP_INT16(SprRam[offs]) & 0x8000) sortedlist[count++] = offs;
	}
	else
	{
		for (; offs<0x800; offs+=8)
			if ((BURN_ENDIAN_SWAP_INT16(SprRam[offs]) & 0x8000) && ((BURN_ENDIAN_SWAP_INT16(SprRam[offs]) & 0xff) != zcode)) sortedlist[count++] = offs;
	}

	w = count;
	count--;
	h = count;

	if (!(K053247Regs[0xc/2] & 0x10))
	{
		// sort objects in decending order(smaller z closer) when OPSET PRI is clear
		for (y=0; y<h; y++)
		{
			offs = sortedlist[y];
			zcode = BURN_ENDIAN_SWAP_INT16(SprRam[offs]) & 0xff;
			for (x=y+1; x<w; x++)
			{
				temp = sortedlist[x];
				code = BURN_ENDIAN_SWAP_INT16(SprRam[temp]) & 0xff;
				if (zcode <= code) { zcode = code; sortedlist[x] = offs; sortedlist[y] = offs = temp; }
			}
		}
	}
	else
	{
		// sort objects in ascending order(bigger z closer) when OPSET PRI is set
		for (y=0; y<h; y++)
		{
			offs = sortedlist[y];
			zcode = BURN_ENDIAN_SWAP_INT16(SprRam[offs]) & 0xff;
			for (x=y+1; x<w; x++)
			{
				temp = sortedlist[x];
				code = BURN_ENDIAN_SWAP_INT16(SprRam[temp]) & 0xff;
				if (zcode >= code) { zcode = code; sortedlist[x] = offs; sortedlist[y] = offs = temp; }
			}
		}
	}

	for (INT32 i = count; i >= 0; i--)
	{
		offs = sortedlist[i];

		code = BURN_ENDIAN_SWAP_INT16(SprRam[offs+1]);
		shadow = color = BURN_ENDIAN_SWAP_INT16(SprRam[offs+6]);
		primask = 0;

		(*K053247Callback)(&code,&color,&primask);

		temp = BURN_ENDIAN_SWAP_INT16(SprRam[offs]);

		size = (temp & 0x0f00) >> 8;
		w = 1 << (size & 0x03);
		h = 1 << ((size >> 2) & 0x03);

		/* the sprite can start at any point in the 8x8 grid. We have to */
		/* adjust the offsets to draw it correctly. Simpsons does this all the time. */
		xa = 0;
		ya = 0;
		if (code & 0x01) xa += 1;
		if (code & 0x02) ya += 1;
		if (code & 0x04) xa += 2;
		if (code & 0x08) ya += 2;
		if (code & 0x10) xa += 4;
		if (code & 0x20) ya += 4;
		code &= ~0x3f;

		oy = (INT16)BURN_ENDIAN_SWAP_INT16(SprRam[offs+2]);
		ox = (INT16)BURN_ENDIAN_SWAP_INT16(SprRam[offs+3]);

		ox += K053247_dx;
		oy -= K053247_dy;

		if (K053247_wraparound)
		{
			offx &= 0x3ff;
			offy &= 0x3ff;
			oy &= 0x3ff;
			ox &= 0x3ff;
		}

		y = zoomy = BURN_ENDIAN_SWAP_INT16(SprRam[offs+4]) & 0x3ff;
		if (zoomy) zoomy = (0x400000+(zoomy>>1)) / zoomy; else zoomy = 0x800000;
		if (!(temp & 0x4000))
		{
			x = zoomx = BURN_ENDIAN_SWAP_INT16(SprRam[offs+5]) & 0x3ff;
			if (zoomx) zoomx = (0x400000+(zoomx>>1)) / zoomx;
			else zoomx = 0x800000;
		}
		else { zoomx = zoomy; x = y; }

		if ( K053246Regs[5] & 0x08 ) // Check only "Bit #3 is '1'?" (NOTE: good guess)
		{
			zoomx >>= 1;		// Fix sprite width to HALF size
			ox = (ox >> 1) + 1;	// Fix sprite draw position
			if (flipscreenx) ox += screen_width;
			nozoom = 0;
		}
		else
			nozoom = (x == 0x40 && y == 0x40);

		flipx = temp & 0x1000;
		flipy = temp & 0x2000;
		mirrorx = shadow & 0x4000;
		if (mirrorx) flipx = 0; // documented and confirmed
		mirrory = shadow & 0x8000;

		INT32 highlight = 0;

		wtable = dtable;

		if (color == -1)
		{
			// drop the entire sprite to shadow unconditionally
			if (shdmask < 0) continue;
			color = 0;
			shadow = -1;

			wtable = stable;
		}
		else
		{
			if (shdmask >= 0)
			{
				shadow = (color & 0x20000000) ? (color >> 20) : (shadow >> 10);

				if (shadow &= 3) {
					if (((shadow-1) & shdmask) == 1) highlight = 1;
				}
			}
			else
				shadow = 0;
		}

		color &= 0xffff; // strip attribute flags

		if (flipscreenx)
		{
			ox = -ox;
			if (!mirrorx) flipx = !flipx;
		}
		if (flipscreeny)
		{
			oy = -oy;
			if (!mirrory) flipy = !flipy;
		}

		// apply wrapping and global offsets
		if (K053247_wraparound)
		{
			ox = ( ox - offx) & 0x3ff;
			oy = (-oy - offy) & 0x3ff;
			if (ox >= 0x300) ox -= 0x400;
			if (oy >= 0x280) oy -= 0x400;
		}
		else
		{
			ox =  ox - offx;
			oy = -oy - offy;
		}

		// apply global and display window offsets

		/* the coordinates given are for the *center* of the sprite */
		ox -= (zoomx * w) >> 13;
		oy -= (zoomy * h) >> 13;

		dtable[15] = shadow ? 2 : 1;

		for (y = 0;y < h;y++)
		{
			INT32 sx,sy,zw,zh;

			sy = oy + ((zoomy * y + (1<<11)) >> 12);
			zh = (oy + ((zoomy * (y+1) + (1<<11)) >> 12)) - sy;

			for (x = 0;x < w;x++)
			{
				INT32 c,fx,fy;

				sx = ox + ((zoomx * x + (1<<11)) >> 12);
				zw = (ox + ((zoomx * (x+1) + (1<<11)) >> 12)) - sx;
				c = code;
				if (mirrorx)
				{
					if ((flipx == 0) ^ ((x<<1) < w))
					{
						/* mirror left/right */
						c += xoffset[(w-1-x+xa)&7];
						fx = 1;
					}
					else
					{
						c += xoffset[(x+xa)&7];
						fx = 0;
					}
				}
				else
				{
					if (flipx) c += xoffset[(w-1-x+xa)&7];
					else c += xoffset[(x+xa)&7];
					fx = flipx;
				}

				if (mirrory)
				{
					if ((flipy == 0) ^ ((y<<1) >= h))
					{
						/* mirror top/bottom */
						c += yoffset[(h-1-y+ya)&7];
						fy = 1;
					}
					else
					{
						c += yoffset[(y+ya)&7];
						fy = 0;
					}
				}
				else
				{
					if (flipy) c += yoffset[(h-1-y+ya)&7];
					else c += yoffset[(y+ya)&7];
					fy = flipy;
				}

				c &= K053246MaskExp;

				if (shadow || wtable == stable) {
					if (mirrory && h == 1)
						konami_render_zoom_shadow_tile(gfxbase, c, nBpp, color, sx, sy, fx, !fy, 16, 16, zw << 12, zh << 12, primask, highlight);

					konami_render_zoom_shadow_tile(gfxbase, c, nBpp, color, sx, sy, fx, fy, 16, 16, zw << 12, zh << 12, primask, highlight);
					continue;
				}

				if (mirrory && h == 1)
				{
					if (nozoom) {
						konami_draw_16x16_prio_tile(gfxbase, c, nBpp, color, sx, sy, fx, !fy, primask);
					} else {
						konami_draw_16x16_priozoom_tile(gfxbase, c, nBpp, color, 0, sx, sy, fx, !fy, 16, 16, zw<<12, zh<<12, primask);
					}
				}

				if (nozoom) {
					konami_draw_16x16_prio_tile(gfxbase, c, nBpp, color, sx, sy, fx, fy, primask);
				} else {
					konami_draw_16x16_priozoom_tile(gfxbase, c, nBpp, color, 0, sx, sy, fx, fy, 16, 16, zw<<12, zh<<12, primask);
				}
			} // end of X loop
		} // end of Y loop

	} // end of sprite-list loop
#undef NUM_SPRITES
}

static inline UINT32 alpha_blend_r32(UINT32 d, UINT32 s, UINT32 p)
{
	if (p == 0) return d;

	INT32 a = 256 - p;

	return (((((s & 0xff00ff) * p) + ((d & 0xff00ff) * a)) & 0xff00ff00) |
		((((s & 0x00ff00) * p) + ((d & 0x00ff00) * a)) & 0x00ff0000)) >> 8;
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

#define GX_ZBUFW     512
#define GX_ZBUFH     256

void zdrawgfxzoom32GP(UINT32 code, UINT32 color, INT32 flipx, INT32 flipy, INT32 sx, INT32 sy,
		INT32 scalex, INT32 scaley, INT32 alpha, INT32 drawmode, INT32 zcode, INT32 pri, UINT8* gx_objzbuf, UINT8* gx_shdzbuf)
{
#define FP     19
#define FPONE  (1<<FP)
#define FPHALF (1<<(FP-1))
#define FPENT  0

	// inner loop
	const UINT8  *src_ptr;
	INT32 src_x;
	INT32 eax, ecx;
	INT32 src_fx, src_fdx;
	INT32 shdpen;
	UINT8  z8 = 0, p8 = 0;
	UINT8  *ozbuf_ptr;
	UINT8  *szbuf_ptr;
	const UINT32 *pal_base;
	const UINT32 *shd_base;
	UINT32 *dst_ptr;

	// outter loop
	INT32 src_fby, src_fdy, src_fbx;
	const UINT8 *src_base;
	INT32 dst_w, dst_h;

	// one-time
	INT32 nozoom, granularity;
	INT32 src_fw, src_fh;
	INT32 dst_minx, dst_maxx, dst_miny, dst_maxy;
	INT32 dst_skipx, dst_skipy, dst_x, dst_y, dst_lastx, dst_lasty;
	INT32 src_pitch, dst_pitch;

	INT32 highlight_enable = (drawmode >> 4) && (K053247Flags & 2);// for fba
	//INT32 highlight_enable = drawmode >> 4;// for fba
	drawmode &= 0xf;

	// cull illegal and transparent objects
	if (!scalex || !scaley) return;

	// find shadow pens and cull invisible shadows
	granularity = shdpen = ((1 << nBpp) /*- 1*/);
	shdpen--;

	if (zcode >= 0)
	{
		if (drawmode == 5) { drawmode = 4; shdpen = 1; }
	}
	else
		if (drawmode >= 4) return;

	// alpha blend necessary?
	if (drawmode & 2)
	{
		if (alpha <= 0) return;
		if (alpha >= 255) drawmode &= ~2;
	}

	// fill internal data structure with default values
	ozbuf_ptr  = gx_objzbuf;
	szbuf_ptr  = gx_shdzbuf;

	src_pitch = 16;
	src_fw    = 16;
	src_fh    = 16;
	src_base  = K053246GfxExp + (code * 0x100);

	pal_base  = konami_palette32 + (color << nBpp);
	shd_base  = konami_palette32; // m_palette->shadow_table(); // iq_132

	dst_ptr   = konami_bitmap32;
	dst_pitch = nScreenWidth;
	dst_minx  = 0;
	dst_maxx  = (nScreenWidth - 1);
	dst_miny  = 0;
	dst_maxy  = (nScreenHeight - 1);
	dst_x     = sx;
	dst_y     = sy;

	// cull off-screen objects
	if (dst_x > dst_maxx || dst_y > dst_maxy) return;
	nozoom = (scalex == 0x10000 && scaley == 0x10000);
	if (nozoom)
	{
		dst_h = dst_w = 16;
		src_fdy = src_fdx = 1;
	}
	else
	{
		dst_w = ((scalex<<4)+0x8000)>>16;
		dst_h = ((scaley<<4)+0x8000)>>16;
		if (!dst_w || !dst_h) return;

		src_fw <<= FP;
		src_fh <<= FP;
		src_fdx = src_fw / dst_w;
		src_fdy = src_fh / dst_h;
	}
	dst_lastx = dst_x + dst_w - 1;
	if (dst_lastx < dst_minx) return;
	dst_lasty = dst_y + dst_h - 1;
	if (dst_lasty < dst_miny) return;

	// clip destination
	dst_skipx = 0;
	eax = dst_minx;  if ((eax -= dst_x) > 0) { dst_skipx = eax;  dst_w -= eax;  dst_x = dst_minx; }
	eax = dst_lastx; if ((eax -= dst_maxx) > 0) dst_w -= eax;
	dst_skipy = 0;
	eax = dst_miny;  if ((eax -= dst_y) > 0) { dst_skipy = eax;  dst_h -= eax;  dst_y = dst_miny; }
	eax = dst_lasty; if ((eax -= dst_maxy) > 0) dst_h -= eax;

	// calculate zoom factors and clip source
	if (nozoom)
	{
		if (!flipx) src_fbx = 0; else { src_fbx = src_fw - 1; src_fdx = -src_fdx; }
		if (!flipy) src_fby = 0; else { src_fby = src_fh - 1; src_fdy = -src_fdy; src_pitch = -src_pitch; }
	}
	else
	{
		if (!flipx) src_fbx = FPENT; else { src_fbx = src_fw - FPENT - 1; src_fdx = -src_fdx; }
		if (!flipy) src_fby = FPENT; else { src_fby = src_fh - FPENT - 1; src_fdy = -src_fdy; }
	}
	src_fbx += dst_skipx * src_fdx;
	src_fby += dst_skipy * src_fdy;

	// adjust insertion points and pre-entry constants
	eax = (dst_y - dst_miny) * GX_ZBUFW + (dst_x - dst_minx) + dst_w;
	z8 = (UINT8)zcode;
	p8 = (UINT8)pri;
	ozbuf_ptr += eax;
	szbuf_ptr += eax << 1;
	dst_ptr += dst_y * dst_pitch + dst_x + dst_w;
	dst_w = -dst_w;

	if (!nozoom)
	{
		ecx = src_fby;   src_fby += src_fdy;
		ecx >>= FP;      src_fx = src_fbx;
		src_x = src_fbx; src_fx += src_fdx;
		ecx <<= 4;       src_ptr = src_base;
		src_x >>= FP;    src_ptr += ecx;
		ecx = dst_w;

		if (zcode < 0) // no shadow and z-buffering
		{
			do {
				do {
					eax = src_ptr[src_x];
					src_x = src_fx;
					src_fx += src_fdx;
					src_x >>= FP;
					if (!eax || eax >= shdpen) continue;
					dst_ptr [ecx] = pal_base[eax];
				}
				while (++ecx);

				ecx = src_fby;   src_fby += src_fdy;
				dst_ptr += dst_pitch;
				ecx >>= FP;      src_fx = src_fbx;
				src_x = src_fbx; src_fx += src_fdx;
				ecx <<= 4;       src_ptr = src_base;
				src_x >>= FP;    src_ptr += ecx;
				ecx = dst_w;
			}
			while (--dst_h);
		}
		else
		{
			switch (drawmode)
			{
				case 0: // all pens solid
					do {
						do {
							eax = src_ptr[src_x];
							src_x = src_fx;
							src_fx += src_fdx;
							src_x >>= FP;
							if (!eax || ozbuf_ptr[ecx] < z8) continue;
							eax = pal_base[eax];
							ozbuf_ptr[ecx] = z8;
							dst_ptr [ecx] = eax;
						}
						while (++ecx);

						ecx = src_fby;   src_fby += src_fdy;
						ozbuf_ptr += GX_ZBUFW;
						dst_ptr += dst_pitch;
						ecx >>= FP;      src_fx = src_fbx;
						src_x = src_fbx; src_fx += src_fdx;
						ecx <<= 4;       src_ptr = src_base;
						src_x >>= FP;    src_ptr += ecx;
						ecx = dst_w;
					}
					while (--dst_h);
					break;

				case 1: // solid pens only
					do {
						do {
							eax = src_ptr[src_x];
							src_x = src_fx;
							src_fx += src_fdx;
							src_x >>= FP;
							if (!eax || eax >= shdpen || ozbuf_ptr[ecx] < z8) continue;
							eax = pal_base[eax];
							ozbuf_ptr[ecx] = z8;
							dst_ptr [ecx] = eax;
						}
						while (++ecx);

						ecx = src_fby;   src_fby += src_fdy;
						ozbuf_ptr += GX_ZBUFW;
						dst_ptr += dst_pitch;
						ecx >>= FP;      src_fx = src_fbx;
						src_x = src_fbx; src_fx += src_fdx;
						ecx <<= 4;       src_ptr = src_base;
						src_x >>= FP;    src_ptr += ecx;
						ecx = dst_w;
					}
					while (--dst_h);
					break;

				case 2: // all pens solid with alpha blending
					do {
						do {
							eax = src_ptr[src_x];
							src_x = src_fx;
							src_fx += src_fdx;
							src_x >>= FP;
							if (!eax || ozbuf_ptr[ecx] < z8) continue;
							ozbuf_ptr[ecx] = z8;

							dst_ptr[ecx] = alpha_blend_r32(pal_base[eax], dst_ptr[ecx], alpha);
						}
						while (++ecx);

						ecx = src_fby;   src_fby += src_fdy;
						ozbuf_ptr += GX_ZBUFW;
						dst_ptr += dst_pitch;
						ecx >>= FP;      src_fx = src_fbx;
						src_x = src_fbx; src_fx += src_fdx;
						ecx <<= 4;       src_ptr = src_base;
						src_x >>= FP;    src_ptr += ecx;
						ecx = dst_w;
					}
					while (--dst_h);
					break;

				case 3: // solid pens only with alpha blending
					do {
						do {
							eax = src_ptr[src_x];
							src_x = src_fx;
							src_fx += src_fdx;
							src_x >>= FP;
							if (!eax || eax >= shdpen || ozbuf_ptr[ecx] < z8) continue;
							ozbuf_ptr[ecx] = z8;

							dst_ptr[ecx] = alpha_blend_r32(pal_base[eax], dst_ptr[ecx], alpha);
						}
						while (++ecx);

						ecx = src_fby;   src_fby += src_fdy;
						ozbuf_ptr += GX_ZBUFW;
						dst_ptr += dst_pitch;
						ecx >>= FP;      src_fx = src_fbx;
						src_x = src_fbx; src_fx += src_fdx;
						ecx <<= 4;       src_ptr = src_base;
						src_x >>= FP;    src_ptr += ecx;
						ecx = dst_w;
					}
					while (--dst_h);
					break;

				case 4: // shadow pens only
					do {
						do {
							eax = src_ptr[src_x];
							src_x = src_fx;
							src_fx += src_fdx;
							src_x >>= FP;
							if (eax < shdpen || szbuf_ptr[ecx*2] < z8 || szbuf_ptr[ecx*2+1] <= p8) continue;
							//UINT32 pix = dst_ptr[ecx];
							szbuf_ptr[ecx*2] = z8;
							szbuf_ptr[ecx*2+1] = p8;

							// the shadow tables are 15-bit lookup tables which accept RGB15... lossy, nasty, yuck!
							if (highlight_enable) {
								dst_ptr[ecx] = highlight_blend(dst_ptr[ecx]); 
							} else {
								dst_ptr[ecx] = shadow_blend(dst_ptr[ecx]); //shd_base[pix.as_rgb15()];
							}
							//dst_ptr[ecx] =(eax>>3&0x001f);lend_r32( eax, 0x00000000, 128);
						}
						while (++ecx);

						ecx = src_fby;   src_fby += src_fdy;
						szbuf_ptr += (GX_ZBUFW<<1);
						dst_ptr += dst_pitch;
						ecx >>= FP;      src_fx = src_fbx;
						src_x = src_fbx; src_fx += src_fdx;
						ecx <<= 4;       src_ptr = src_base;
						src_x >>= FP;    src_ptr += ecx;
						ecx = dst_w;
					}
					while (--dst_h);
					break;
			}   // switch (drawmode)
		}   // if (zcode < 0)
	}   // if (!nozoom)
	else
	{
		src_ptr = src_base + (src_fby<<4) + src_fbx;
		src_fdy = src_fdx * dst_w + src_pitch;
		ecx = dst_w;

		if (zcode < 0) // no shadow and z-buffering
		{
			do {
				do {
					eax = *src_ptr;
					src_ptr += src_fdx;
					if (!eax || eax >= shdpen) continue;
					dst_ptr[ecx] = pal_base[eax];
				}
				while (++ecx);

				src_ptr += src_fdy;
				dst_ptr += dst_pitch;
				ecx = dst_w;
			}
			while (--dst_h);
		}
		else
		{
			switch (drawmode)
			{
				case 0: // all pens solid
					do {
						do {
							eax = *src_ptr;
							src_ptr += src_fdx;
							if (!eax || ozbuf_ptr[ecx] < z8) continue;
							eax = pal_base[eax];
							ozbuf_ptr[ecx] = z8;
							dst_ptr[ecx] = eax;
						}
						while (++ecx);

						src_ptr += src_fdy;
						ozbuf_ptr += GX_ZBUFW;
						dst_ptr += dst_pitch;
						ecx = dst_w;
					}
					while (--dst_h);
					break;

				case 1:  // solid pens only
					do {
						do {
							eax = *src_ptr;
							src_ptr += src_fdx;
							if (!eax || eax >= shdpen || ozbuf_ptr[ecx] < z8) continue;
							eax = pal_base[eax];
							ozbuf_ptr[ecx] = z8;
							dst_ptr[ecx] = eax;
						}
						while (++ecx);

						src_ptr += src_fdy;
						ozbuf_ptr += GX_ZBUFW;
						dst_ptr += dst_pitch;
						ecx = dst_w;
					}
					while (--dst_h);
					break;

				case 2: // all pens solid with alpha blending
					do {
						do {
							eax = *src_ptr;
							src_ptr += src_fdx;
							if (!eax || ozbuf_ptr[ecx] < z8) continue;
							ozbuf_ptr[ecx] = z8;

							dst_ptr[ecx] = alpha_blend_r32(pal_base[eax], dst_ptr[ecx], alpha);
						}
						while (++ecx);

						src_ptr += src_fdy;
						ozbuf_ptr += GX_ZBUFW;
						dst_ptr += dst_pitch;
						ecx = dst_w;
					}
					while (--dst_h);
					break;

				case 3: // solid pens only with alpha blending
					do {
						do {
							eax = *src_ptr;
							src_ptr += src_fdx;
							if (!eax || eax >= shdpen || ozbuf_ptr[ecx] < z8) continue;
							ozbuf_ptr[ecx] = z8;

							dst_ptr[ecx] = alpha_blend_r32(pal_base[eax], dst_ptr[ecx], alpha);
						}
						while (++ecx);

						src_ptr += src_fdy;
						ozbuf_ptr += GX_ZBUFW;
						dst_ptr += dst_pitch;
						ecx = dst_w;
					}
					while (--dst_h);
					break;

				case 4: // shadow pens only
					do {
						do {
							eax = *src_ptr;
							src_ptr += src_fdx;
							if (eax < shdpen || szbuf_ptr[ecx*2] < z8 || szbuf_ptr[ecx*2+1] <= p8) continue;
							//UINT32 pix = dst_ptr[ecx];
							szbuf_ptr[ecx*2] = z8;
							szbuf_ptr[ecx*2+1] = p8;

							// the shadow tables are 15-bit lookup tables which accept RGB15... lossy, nasty, yuck!
							if (highlight_enable) {
								dst_ptr[ecx] = highlight_blend(dst_ptr[ecx]); 
							} else {
								dst_ptr[ecx] = shadow_blend(dst_ptr[ecx]); //shd_base[pix.as_rgb15()];
							}
						}
						while (++ecx);

						src_ptr += src_fdy;
						szbuf_ptr += (GX_ZBUFW<<1);
						dst_ptr += dst_pitch;
						ecx = dst_w;
					}
					while (--dst_h);
					break;
			}
		}
	}
#undef FP
#undef FPONE
#undef FPHALF
#undef FPENT
}





void k053247_draw_yxloop_gx(
		INT32 code,
		INT32 color,
		INT32 height, INT32 width,
		INT32 zoomx, INT32 zoomy, INT32 flipx, INT32 flipy,
		INT32 ox, INT32 oy,
		INT32 xa, INT32 ya,
		INT32 mirrorx, INT32 mirrory,
		INT32 nozoom,
		/* gx specifics */
		INT32 pri,
		INT32 zcode, INT32 alpha, INT32 drawmode,
		UINT8* gx_objzbuf, UINT8* gx_shdzbuf,
		/* non-gx specifics */
		INT32 /*primask*/,
		UINT8* /*whichtable*/
		)
	{
		static const INT32 xoffset[8] = { 0, 1, 4, 5, 16, 17, 20, 21 };
		static const INT32 yoffset[8] = { 0, 2, 8, 10, 32, 34, 40, 42 };
		INT32 zw,zh;
		INT32  fx, fy, sx, sy;
		INT32 tempcode;

		for (INT32 y=0; y<height; y++)
		{
			sy = oy + ((zoomy * y + (1<<11)) >> 12);
			zh = (oy + ((zoomy * (y+1) + (1<<11)) >> 12)) - sy;

			for (INT32 x=0; x<width; x++)
			{
				sx = ox + ((zoomx * x + (1<<11)) >> 12);
				zw = (ox + ((zoomx * (x+1) + (1<<11)) >> 12)) - sx;
				tempcode = code;

				if (mirrorx)
				{
					if ((!flipx)^((x<<1)<width))
					{
						/* mirror left/right */
						tempcode += xoffset[(width-1-x+xa)&7];
						fx = 1;
					}
					else
					{
						tempcode += xoffset[(x+xa)&7];
						fx = 0;
					}
				}
				else
				{
					if (flipx) tempcode += xoffset[(width-1-x+xa)&7];
					else tempcode += xoffset[(x+xa)&7];
					fx = flipx;
				}

				if (mirrory)
				{
					if ((!flipy)^((y<<1)>=height))
					{
						/* mirror top/bottom */
						tempcode += yoffset[(height-1-y+ya)&7];
						fy = 1;
					}
					else
					{
						tempcode += yoffset[(y+ya)&7];
						fy = 0;
					}
				}
				else
				{
					if (flipy) tempcode += yoffset[(height-1-y+ya)&7];
					else tempcode += yoffset[(y+ya)&7];
					fy = flipy;
				}

				{
					if (nozoom) { zw = zh = 0x10; }

					zdrawgfxzoom32GP(
							tempcode,
							color,
							fx,fy,
							sx,sy,
							zw << 12, zh << 12, alpha, drawmode, zcode, pri,
							gx_objzbuf, gx_shdzbuf
							);

				}
			} // end of X loop
		} // end of Y loop
	}




void k053247_draw_single_sprite_gxcore(UINT8 *gx_objzbuf, UINT8 *gx_shdzbuf, INT32 code, UINT16 *gx_spriteram, INT32 offs,
		INT32 color, INT32 alpha, INT32 drawmode, INT32 zcode, INT32 pri,
		INT32 /*primask*/, INT32 /*shadow*/, UINT8 */*drawmode_table*/, UINT8 */*shadowmode_table*/, INT32 /*shdmask*/)
	{
		INT32 xa,ya,ox,oy,flipx,flipy,mirrorx,mirrory,zoomx,zoomy,scalex,scaley,nozoom;
		INT32 temp, temp4;
		INT32 flipscreenx = K053246Regs[5] & 0x01;
		INT32 flipscreeny = K053246Regs[5] & 0x02;

		xa = ya = 0;
		if (code & 0x01) xa += 1;
		if (code & 0x02) ya += 1;
		if (code & 0x04) xa += 2;
		if (code & 0x08) ya += 2;
		if (code & 0x10) xa += 4;
		if (code & 0x20) ya += 4;
		code &= ~0x3f;

		temp4 = gx_spriteram[offs];

		// mask off the upper 6 bits of coordinate and zoom registers
		oy = gx_spriteram[offs+2] & 0x3ff;
		ox = gx_spriteram[offs+3] & 0x3ff;

		scaley = zoomy = gx_spriteram[offs+4] & 0x3ff;
		if (zoomy) zoomy = (0x400000+(zoomy>>1)) / zoomy;
		else zoomy = 0x800000;
		if (!(temp4 & 0x4000))
		{
			scalex = zoomx = gx_spriteram[offs+5] & 0x3ff;
			if (zoomx) zoomx = (0x400000+(zoomx>>1)) / zoomx;
			else zoomx = 0x800000;
		}
		else { zoomx = zoomy; scalex = scaley; }

		nozoom = (scalex == 0x40 && scaley == 0x40);

		flipx = temp4 & 0x1000;
		flipy = temp4 & 0x2000;

		temp = gx_spriteram[offs+6];
		mirrorx = temp & 0x4000;
		if (mirrorx) flipx = 0; // only applies to x mirror, proven
		mirrory = temp & 0x8000;

		INT32 objset1 = K053246ReadRegs(5);
		// for Escape Kids (GX975)
		if ( objset1 & 8 ) // Check only "Bit #3 is '1'?"
		{
			INT32 screenwidth = nScreenWidth-1;

			zoomx = zoomx>>1; // Fix sprite width to HALF size
			ox = (ox>>1) + 1; // Fix sprite draw position

			if (flipscreenx) ox += screenwidth;
			nozoom = 0;
		}

		if (flipscreenx) { ox = -ox; if (!mirrorx) flipx = !flipx; }
		if (flipscreeny) { oy = -oy; if (!mirrory) flipy = !flipy; }

		INT32 k053247_opset = K053247ReadRegs(0xc/2);
		INT32 wrapsize, xwraplim, ywraplim;
		if (k053247_opset & 0x40)
		{
			wrapsize = 512;
			xwraplim = 512 - 64;
			ywraplim = 512 - 128;
		}
		else
		{
			wrapsize  = 1024;
			xwraplim  = 1024 - 384;
			ywraplim  = 1024 - 512;
		}

		// get "display window" offsets
		INT32 offx = (INT16)((K053246Regs[0] << 8) | K053246Regs[1]);
		INT32 offy = (INT16)((K053246Regs[2] << 8) | K053246Regs[3]);

		// apply wrapping and global offsets
		temp = wrapsize-1;

		ox += K053247_dx;
		oy -= K053247_dy;

		ox = ( ox - offx) & temp;
		oy = (-oy - offy) & temp;
		if (ox >= xwraplim) ox -= wrapsize;
		if (oy >= ywraplim) oy -= wrapsize;

		temp = temp4>>8 & 0x0f;
		INT32 width = 1 << (temp & 3);
		INT32 height = 1 << (temp>>2 & 3);

		ox -= (zoomx * width) >> 13;
		oy -= (zoomy * height) >> 13;

			k053247_draw_yxloop_gx(	code,
				color,
				height, width,
				zoomx, zoomy, flipx, flipy,
				ox, oy,
				xa, ya,
				mirrorx, mirrory,
				nozoom,
				pri,
				zcode, alpha, drawmode,
				gx_objzbuf, gx_shdzbuf,
				0,NULL
				);
	}
