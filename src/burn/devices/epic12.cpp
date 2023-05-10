/* ported from mame 0.155 */
/*
Copyright (c) 1997-2015 Nicola Salmoria and the MAME team
Redistributions may not be sold, nor may they be used in a commercial product or activity.
*/
/* emulation of Altera Cyclone EPIC12 FPGA programmed as a blitter */
/* code by David Haywood, Luca Elia, MetalliC */

#include "burnint.h"
#include "tiles_generic.h"
#include "sh4_intf.h"
#include "thready.h"
#include "rectangle.h"

static UINT16* m_ram16;
static UINT32 m_gfx_addr;
static UINT32 m_gfx_scroll_0_x, m_gfx_scroll_0_y;
static UINT32 m_gfx_scroll_1_x, m_gfx_scroll_1_y;

static int m_gfx_size;
static UINT32 *m_bitmaps;
static rectangle m_clip;

static UINT64 epic12_device_blit_delay;
static int m_blitter_busy;

static UINT16* m_use_ram;
static int m_main_ramsize; // type D has double the main ram
static int m_main_rammask;

static int m_delay_scale;
static int m_burn_cycles;

static UINT8 epic12_device_colrtable[0x20][0x40];
static UINT8 epic12_device_colrtable_rev[0x20][0x40];
static UINT8 epic12_device_colrtable_add[0x20][0x20];

static UINT16 *pal16 = NULL; // palette lut for 16bpp video emulation

#include "epic12.h"

struct _clr_t
{
	UINT8 b,g,r,t;
};

typedef struct _clr_t clr_t;

union colour_t
{
	clr_t trgb;
	UINT32 u32;
};

typedef const void (*epic12_device_blitfunction)(
						const rectangle *,
						UINT32 *, /* gfx */
						int , /* src_x */
						int , /* src_y */
						const int , /* dst_x_start */
						const int , /* dst_y_start */
						int , /* dimx */
						int , /* dimy */
						const int , /* flipy */
						const UINT8 , /* s_alpha */
						const UINT8 , /* d_alpha */
						//int , /* tint */
						const clr_t * );

#define BLIT_PARAMS const rectangle *clip, UINT32 *gfx, int src_x, int src_y, const int dst_x_start, const int dst_y_start, int dimx, int dimy, const int flipy, const UINT8 s_alpha, const UINT8 d_alpha, const clr_t *tint_clr

	static inline void pen_to_clr(UINT32 pen, clr_t *clr)
	{
	// --t- ---- rrrr r--- gggg g--- bbbb b---  format
		clr->r = (pen >> (16+3));// & 0x1f;
		clr->g = (pen >>  (8+3));// & 0x1f;
		clr->b = (pen >>   3);// & 0x1f;

	// --t- ---- ---r rrrr ---g gggg ---b bbbb  format
	//  clr->r = (pen >> 16) & 0x1f;
	//  clr->g = (pen >> 8) & 0x1f;
	//  clr->b = (pen >> 0) & 0x1f;

	}


	// convert separate r,g,b biases (0..80..ff) to clr_t (-1f..0..1f)
	static inline void tint_to_clr(UINT8 r, UINT8 g, UINT8 b, clr_t *clr)
	{
		clr->r  =   r>>2;
		clr->g  =   g>>2;
		clr->b  =   b>>2;
	}

	// clr_t to r5g5b5
	static inline UINT32 clr_to_pen(const clr_t *clr)
	{
	// --t- ---- rrrr r--- gggg g--- bbbb b---  format
		return (clr->r << (16+3)) | (clr->g << (8+3)) | (clr->b << 3);

	// --t- ---- ---r rrrr ---g gggg ---b bbbb  format
	//  return (clr->r << (16)) | (clr->g << (8)) | (clr->b);
	}


	static inline void clr_add_with_clr_mul_fixed(clr_t *clr, const clr_t *clr0, const UINT8 mulfixed_val, const clr_t *mulfixed_clr0)
	{
		clr->r = epic12_device_colrtable_add[clr0->r][epic12_device_colrtable[(mulfixed_clr0->r)][mulfixed_val]];
		clr->g = epic12_device_colrtable_add[clr0->g][epic12_device_colrtable[(mulfixed_clr0->g)][mulfixed_val]];
		clr->b = epic12_device_colrtable_add[clr0->b][epic12_device_colrtable[(mulfixed_clr0->b)][mulfixed_val]];
	}

	static inline  void clr_add_with_clr_mul_3param(clr_t *clr, const clr_t *clr0, const clr_t *clr1, const clr_t *clr2)
	{
		clr->r = epic12_device_colrtable_add[clr0->r][epic12_device_colrtable[(clr2->r)][(clr1->r)]];
		clr->g = epic12_device_colrtable_add[clr0->g][epic12_device_colrtable[(clr2->g)][(clr1->g)]];
		clr->b = epic12_device_colrtable_add[clr0->b][epic12_device_colrtable[(clr2->b)][(clr1->b)]];
	}

	static inline  void clr_add_with_clr_square(clr_t *clr, const clr_t *clr0, const clr_t *clr1)
	{
		clr->r = epic12_device_colrtable_add[clr0->r][epic12_device_colrtable[(clr1->r)][(clr1->r)]];
		clr->g = epic12_device_colrtable_add[clr0->r][epic12_device_colrtable[(clr1->g)][(clr1->g)]];
		clr->b = epic12_device_colrtable_add[clr0->r][epic12_device_colrtable[(clr1->b)][(clr1->b)]];
	}

	static inline  void clr_add_with_clr_mul_fixed_rev(clr_t *clr, const clr_t *clr0, const UINT8 val, const clr_t *clr1)
	{
		clr->r =  epic12_device_colrtable_add[clr0->r][epic12_device_colrtable_rev[val][(clr1->r)]];
		clr->g =  epic12_device_colrtable_add[clr0->g][epic12_device_colrtable_rev[val][(clr1->g)]];
		clr->b =  epic12_device_colrtable_add[clr0->b][epic12_device_colrtable_rev[val][(clr1->b)]];
	}

	static inline  void clr_add_with_clr_mul_rev_3param(clr_t *clr, const clr_t *clr0, const clr_t *clr1, const clr_t *clr2)
	{
		clr->r =  epic12_device_colrtable_add[clr0->r][epic12_device_colrtable_rev[(clr2->r)][(clr1->r)]];
		clr->g =  epic12_device_colrtable_add[clr0->g][epic12_device_colrtable_rev[(clr2->g)][(clr1->g)]];
		clr->b =  epic12_device_colrtable_add[clr0->b][epic12_device_colrtable_rev[(clr2->b)][(clr1->b)]];
	}

	static inline  void clr_add_with_clr_mul_rev_square(clr_t *clr, const clr_t *clr0, const clr_t *clr1)
	{
		clr->r =  epic12_device_colrtable_add[clr0->r][epic12_device_colrtable_rev[(clr1->r)][(clr1->r)]];
		clr->g =  epic12_device_colrtable_add[clr0->g][epic12_device_colrtable_rev[(clr1->g)][(clr1->g)]];
		clr->b =  epic12_device_colrtable_add[clr0->b][epic12_device_colrtable_rev[(clr1->b)][(clr1->b)]];
	}


	static inline  void clr_add(clr_t *clr, const clr_t *clr0, const clr_t *clr1)
	{
	/*
	    clr->r = clr0->r + clr1->r;
	    clr->g = clr0->g + clr1->g;
	    clr->b = clr0->b + clr1->b;
	*/
		// use pre-clamped lookup table
		clr->r =  epic12_device_colrtable_add[clr0->r][clr1->r];
		clr->g =  epic12_device_colrtable_add[clr0->g][clr1->g];
		clr->b =  epic12_device_colrtable_add[clr0->b][clr1->b];

	}


	static inline void clr_mul(clr_t *clr0, const clr_t *clr1)
	{
		clr0->r = epic12_device_colrtable[(clr0->r)][(clr1->r)];
		clr0->g = epic12_device_colrtable[(clr0->g)][(clr1->g)];
		clr0->b = epic12_device_colrtable[(clr0->b)][(clr1->b)];
	}

	static inline void clr_square(clr_t *clr0, const clr_t *clr1)
	{
		clr0->r = epic12_device_colrtable[(clr1->r)][(clr1->r)];
		clr0->g = epic12_device_colrtable[(clr1->g)][(clr1->g)];
		clr0->b = epic12_device_colrtable[(clr1->b)][(clr1->b)];
	}

	static inline void clr_mul_3param(clr_t *clr0, const clr_t *clr1, const clr_t *clr2)
	{
		clr0->r = epic12_device_colrtable[(clr2->r)][(clr1->r)];
		clr0->g = epic12_device_colrtable[(clr2->g)][(clr1->g)];
		clr0->b = epic12_device_colrtable[(clr2->b)][(clr1->b)];
	}

	static inline void clr_mul_rev(clr_t *clr0, const clr_t *clr1)
	{
		clr0->r = epic12_device_colrtable_rev[(clr0->r)][(clr1->r)];
		clr0->g = epic12_device_colrtable_rev[(clr0->g)][(clr1->g)];
		clr0->b = epic12_device_colrtable_rev[(clr0->b)][(clr1->b)];
	}

	static inline void clr_mul_rev_square(clr_t *clr0, const clr_t *clr1)
	{
		clr0->r = epic12_device_colrtable_rev[(clr1->r)][(clr1->r)];
		clr0->g = epic12_device_colrtable_rev[(clr1->g)][(clr1->g)];
		clr0->b = epic12_device_colrtable_rev[(clr1->b)][(clr1->b)];
	}


	static inline void clr_mul_rev_3param(clr_t *clr0, const clr_t *clr1, const clr_t *clr2)
	{
		clr0->r = epic12_device_colrtable_rev[(clr2->r)][(clr1->r)];
		clr0->g = epic12_device_colrtable_rev[(clr2->g)][(clr1->g)];
		clr0->b = epic12_device_colrtable_rev[(clr2->b)][(clr1->b)];
	}

	static inline void clr_mul_fixed(clr_t *clr, const UINT8 val, const clr_t *clr0)
	{
		clr->r = epic12_device_colrtable[val][(clr0->r)];
		clr->g = epic12_device_colrtable[val][(clr0->g)];
		clr->b = epic12_device_colrtable[val][(clr0->b)];
	}

	static inline void clr_mul_fixed_rev(clr_t *clr, const UINT8 val, const clr_t *clr0)
	{
		clr->r = epic12_device_colrtable_rev[val][(clr0->r)];
		clr->g = epic12_device_colrtable_rev[val][(clr0->g)];
		clr->b = epic12_device_colrtable_rev[val][(clr0->b)];
	}

	static inline void clr_copy(clr_t *clr, const clr_t *clr0)
	{
		clr->r = clr0->r;
		clr->g = clr0->g;
		clr->b = clr0->b;
	}



	// (1|s|d) * s_factor * s + (1|s|d) * d_factor * d
	// 0: +alpha
	// 1: +source
	// 2: +dest
	// 3: *
	// 4: -alpha
	// 5: -source
	// 6: -dest
	// 7: *

#include "epic12_blit0.inc"
#include "epic12_blit1.inc"
#include "epic12_blit2.inc"
#include "epic12_blit3.inc"
#include "epic12_blit4.inc"
#include "epic12_blit5.inc"
#include "epic12_blit6.inc"
#include "epic12_blit7.inc"
#include "epic12_blit8.inc"

static UINT8 *dips; // pointer to cv1k's dips

static void blitter_delay_callback(int)
{
	m_blitter_busy = 0;
	//bprintf(0, _T("cyc @ blitdelay callback: %d\n"), Sh3TotalCycles());
}

static void gfx_exec(); // forward

static void run_blitter_cb()
{
	epic12_device_blit_delay = 0;
	gfx_exec();
}

void epic12_exit()
{
	thready.exit();

	BurnFree(m_bitmaps);

	if (pal16) {
		BurnFree(pal16);
		pal16 = NULL;
	}
}

void epic12_init(INT32 ram_size, UINT16 *ram, UINT8 *dippy)
{
	m_main_ramsize = ram_size;
	m_main_rammask = ram_size - 1;

	m_use_ram = m_ram16 = ram;

	dips = dippy;

	m_gfx_size = 0x2000 * 0x1000;
	m_bitmaps = (UINT32*)BurnMalloc (0x2000 * 0x1000 * 4);

	m_clip.set(0, 0x2000-1, 0, 0x1000-1);

	m_delay_scale = 50;
	m_blitter_busy = 0;
	m_gfx_addr = 0;
	m_gfx_scroll_0_x = 0;
	m_gfx_scroll_0_y = 0;
	m_gfx_scroll_1_x = 0;
	m_gfx_scroll_1_y = 0;
	epic12_device_blit_delay = 0;

	thready.init(run_blitter_cb);

	sh4_set_cave_blitter_delay_func(blitter_delay_callback);
}

void epic12_set_blitterthreading(INT32 value)
{
	thready.set_threading(value);
}

void epic12_set_blitterdelay(INT32 delay, INT32 burn_cycles)
{
	m_delay_scale = delay;
	m_burn_cycles = burn_cycles;
}

void epic12_reset()
{
	// cache table to avoid divides in blit code, also pre-clamped
	int x,y;
	for (y=0;y<0x40;y++)
	{
		for (x=0;x<0x20;x++)
		{
			epic12_device_colrtable[x][y] = (x*y) / 0x1f;
			if (epic12_device_colrtable[x][y]>0x1f) epic12_device_colrtable[x][y] = 0x1f;

			epic12_device_colrtable_rev[x^0x1f][y] = (x*y) / 0x1f;
			if (epic12_device_colrtable_rev[x^0x1f][y]>0x1f) epic12_device_colrtable_rev[x^0x1f][y] = 0x1f;
		}
	}

	// preclamped add table
	for (y=0;y<0x20;y++)
	{
		for (x=0;x<0x20;x++)
		{
			epic12_device_colrtable_add[x][y] = (x+y);
			if (epic12_device_colrtable_add[x][y]>0x1f) epic12_device_colrtable_add[x][y] = 0x1f;
		}
	}

	m_blitter_busy = 0;
	m_gfx_addr = 0;
	m_gfx_scroll_0_x = 0;
	m_gfx_scroll_0_y = 0;
	m_gfx_scroll_1_x = 0;
	m_gfx_scroll_1_y = 0;
	epic12_device_blit_delay = 0;

	thready.reset();
}

static UINT16 READ_NEXT_WORD(UINT32 *addr)
{
	UINT16 data = m_use_ram[((*addr & m_main_rammask) >> 1)];

	*addr += 2;

	return data;
}

static void gfx_upload(UINT32 *addr)
{
	UINT32 x,y, dst_p,dst_x_start,dst_y_start, dimx,dimy;
	UINT32 *dst;

	// 0x20000000
	READ_NEXT_WORD(addr);
	READ_NEXT_WORD(addr);

	// 0x99999999
	READ_NEXT_WORD(addr);
	READ_NEXT_WORD(addr);

	dst_x_start = READ_NEXT_WORD(addr);
	dst_y_start = READ_NEXT_WORD(addr);

	dst_p = 0;
	dst_x_start &= 0x1fff;
	dst_y_start &= 0x0fff;

	dimx = (READ_NEXT_WORD(addr) & 0x1fff) + 1;
	dimy = (READ_NEXT_WORD(addr) & 0x0fff) + 1;

	//bprintf(0, _T("GFX COPY: DST %02X,%02X,%03X DIM %02X,%03X\n"), dst_p,dst_x_start,dst_y_start, dimx,dimy);

	for (y = 0; y < dimy; y++)
	{
		//dst = &m_bitmaps->pix(dst_y_start + y, 0);
		dst = m_bitmaps + (dst_y_start + y) * 0x2000;

		dst += dst_x_start;

		for (x = 0; x < dimx; x++)
		{
			UINT16 pendat = READ_NEXT_WORD(addr);
			// real hw would upload the gfxword directly, but our VRAM is 32-bit, so convert it.
			//dst[dst_x_start + x] = pendat;
			*dst++ = ((pendat&0x8000)<<14) | ((pendat&0x7c00)<<9) | ((pendat&0x03e0)<<6) | ((pendat&0x001f)<<3);  // --t- ---- rrrr r--- gggg g--- bbbb b---  format
			//dst[dst_x_start + x] = ((pendat&0x8000)<<14) | ((pendat&0x7c00)<<6) | ((pendat&0x03e0)<<3) | ((pendat&0x001f)<<0);  // --t- ---- ---r rrrr ---g gggg ---b bbbb  format


		}
	}
}

#define draw_params &m_clip, m_bitmaps,src_x,src_y, x,y, dimx,dimy, flipy, s_alpha, d_alpha, &tint_clr



static epic12_device_blitfunction epic12_device_f0_ti1_tr1_blit_funcs[] =
{
	draw_sprite_f0_ti1_tr1_s0_d0, draw_sprite_f0_ti1_tr1_s1_d0, draw_sprite_f0_ti1_tr1_s2_d0, draw_sprite_f0_ti1_tr1_s3_d0, draw_sprite_f0_ti1_tr1_s4_d0, draw_sprite_f0_ti1_tr1_s5_d0, draw_sprite_f0_ti1_tr1_s6_d0, draw_sprite_f0_ti1_tr1_s7_d0,
	draw_sprite_f0_ti1_tr1_s0_d1, draw_sprite_f0_ti1_tr1_s1_d1, draw_sprite_f0_ti1_tr1_s2_d1, draw_sprite_f0_ti1_tr1_s3_d1, draw_sprite_f0_ti1_tr1_s4_d1, draw_sprite_f0_ti1_tr1_s5_d1, draw_sprite_f0_ti1_tr1_s6_d1, draw_sprite_f0_ti1_tr1_s7_d1,
	draw_sprite_f0_ti1_tr1_s0_d2, draw_sprite_f0_ti1_tr1_s1_d2, draw_sprite_f0_ti1_tr1_s2_d2, draw_sprite_f0_ti1_tr1_s3_d2, draw_sprite_f0_ti1_tr1_s4_d2, draw_sprite_f0_ti1_tr1_s5_d2, draw_sprite_f0_ti1_tr1_s6_d2, draw_sprite_f0_ti1_tr1_s7_d2,
	draw_sprite_f0_ti1_tr1_s0_d3, draw_sprite_f0_ti1_tr1_s1_d3, draw_sprite_f0_ti1_tr1_s2_d3, draw_sprite_f0_ti1_tr1_s3_d3, draw_sprite_f0_ti1_tr1_s4_d3, draw_sprite_f0_ti1_tr1_s5_d3, draw_sprite_f0_ti1_tr1_s6_d3, draw_sprite_f0_ti1_tr1_s7_d3,
	draw_sprite_f0_ti1_tr1_s0_d4, draw_sprite_f0_ti1_tr1_s1_d4, draw_sprite_f0_ti1_tr1_s2_d4, draw_sprite_f0_ti1_tr1_s3_d4, draw_sprite_f0_ti1_tr1_s4_d4, draw_sprite_f0_ti1_tr1_s5_d4, draw_sprite_f0_ti1_tr1_s6_d4, draw_sprite_f0_ti1_tr1_s7_d4,
	draw_sprite_f0_ti1_tr1_s0_d5, draw_sprite_f0_ti1_tr1_s1_d5, draw_sprite_f0_ti1_tr1_s2_d5, draw_sprite_f0_ti1_tr1_s3_d5, draw_sprite_f0_ti1_tr1_s4_d5, draw_sprite_f0_ti1_tr1_s5_d5, draw_sprite_f0_ti1_tr1_s6_d5, draw_sprite_f0_ti1_tr1_s7_d5,
	draw_sprite_f0_ti1_tr1_s0_d6, draw_sprite_f0_ti1_tr1_s1_d6, draw_sprite_f0_ti1_tr1_s2_d6, draw_sprite_f0_ti1_tr1_s3_d6, draw_sprite_f0_ti1_tr1_s4_d6, draw_sprite_f0_ti1_tr1_s5_d6, draw_sprite_f0_ti1_tr1_s6_d6, draw_sprite_f0_ti1_tr1_s7_d6,
	draw_sprite_f0_ti1_tr1_s0_d7, draw_sprite_f0_ti1_tr1_s1_d7, draw_sprite_f0_ti1_tr1_s2_d7, draw_sprite_f0_ti1_tr1_s3_d7, draw_sprite_f0_ti1_tr1_s4_d7, draw_sprite_f0_ti1_tr1_s5_d7, draw_sprite_f0_ti1_tr1_s6_d7, draw_sprite_f0_ti1_tr1_s7_d7,
};

static epic12_device_blitfunction epic12_device_f0_ti1_tr0_blit_funcs[] =
{
	draw_sprite_f0_ti1_tr0_s0_d0, draw_sprite_f0_ti1_tr0_s1_d0, draw_sprite_f0_ti1_tr0_s2_d0, draw_sprite_f0_ti1_tr0_s3_d0, draw_sprite_f0_ti1_tr0_s4_d0, draw_sprite_f0_ti1_tr0_s5_d0, draw_sprite_f0_ti1_tr0_s6_d0, draw_sprite_f0_ti1_tr0_s7_d0,
	draw_sprite_f0_ti1_tr0_s0_d1, draw_sprite_f0_ti1_tr0_s1_d1, draw_sprite_f0_ti1_tr0_s2_d1, draw_sprite_f0_ti1_tr0_s3_d1, draw_sprite_f0_ti1_tr0_s4_d1, draw_sprite_f0_ti1_tr0_s5_d1, draw_sprite_f0_ti1_tr0_s6_d1, draw_sprite_f0_ti1_tr0_s7_d1,
	draw_sprite_f0_ti1_tr0_s0_d2, draw_sprite_f0_ti1_tr0_s1_d2, draw_sprite_f0_ti1_tr0_s2_d2, draw_sprite_f0_ti1_tr0_s3_d2, draw_sprite_f0_ti1_tr0_s4_d2, draw_sprite_f0_ti1_tr0_s5_d2, draw_sprite_f0_ti1_tr0_s6_d2, draw_sprite_f0_ti1_tr0_s7_d2,
	draw_sprite_f0_ti1_tr0_s0_d3, draw_sprite_f0_ti1_tr0_s1_d3, draw_sprite_f0_ti1_tr0_s2_d3, draw_sprite_f0_ti1_tr0_s3_d3, draw_sprite_f0_ti1_tr0_s4_d3, draw_sprite_f0_ti1_tr0_s5_d3, draw_sprite_f0_ti1_tr0_s6_d3, draw_sprite_f0_ti1_tr0_s7_d3,
	draw_sprite_f0_ti1_tr0_s0_d4, draw_sprite_f0_ti1_tr0_s1_d4, draw_sprite_f0_ti1_tr0_s2_d4, draw_sprite_f0_ti1_tr0_s3_d4, draw_sprite_f0_ti1_tr0_s4_d4, draw_sprite_f0_ti1_tr0_s5_d4, draw_sprite_f0_ti1_tr0_s6_d4, draw_sprite_f0_ti1_tr0_s7_d4,
	draw_sprite_f0_ti1_tr0_s0_d5, draw_sprite_f0_ti1_tr0_s1_d5, draw_sprite_f0_ti1_tr0_s2_d5, draw_sprite_f0_ti1_tr0_s3_d5, draw_sprite_f0_ti1_tr0_s4_d5, draw_sprite_f0_ti1_tr0_s5_d5, draw_sprite_f0_ti1_tr0_s6_d5, draw_sprite_f0_ti1_tr0_s7_d5,
	draw_sprite_f0_ti1_tr0_s0_d6, draw_sprite_f0_ti1_tr0_s1_d6, draw_sprite_f0_ti1_tr0_s2_d6, draw_sprite_f0_ti1_tr0_s3_d6, draw_sprite_f0_ti1_tr0_s4_d6, draw_sprite_f0_ti1_tr0_s5_d6, draw_sprite_f0_ti1_tr0_s6_d6, draw_sprite_f0_ti1_tr0_s7_d6,
	draw_sprite_f0_ti1_tr0_s0_d7, draw_sprite_f0_ti1_tr0_s1_d7, draw_sprite_f0_ti1_tr0_s2_d7, draw_sprite_f0_ti1_tr0_s3_d7, draw_sprite_f0_ti1_tr0_s4_d7, draw_sprite_f0_ti1_tr0_s5_d7, draw_sprite_f0_ti1_tr0_s6_d7, draw_sprite_f0_ti1_tr0_s7_d7,
};

static epic12_device_blitfunction epic12_device_f1_ti1_tr1_blit_funcs[] =
{
	draw_sprite_f1_ti1_tr1_s0_d0, draw_sprite_f1_ti1_tr1_s1_d0, draw_sprite_f1_ti1_tr1_s2_d0, draw_sprite_f1_ti1_tr1_s3_d0, draw_sprite_f1_ti1_tr1_s4_d0, draw_sprite_f1_ti1_tr1_s5_d0, draw_sprite_f1_ti1_tr1_s6_d0, draw_sprite_f1_ti1_tr1_s7_d0,
	draw_sprite_f1_ti1_tr1_s0_d1, draw_sprite_f1_ti1_tr1_s1_d1, draw_sprite_f1_ti1_tr1_s2_d1, draw_sprite_f1_ti1_tr1_s3_d1, draw_sprite_f1_ti1_tr1_s4_d1, draw_sprite_f1_ti1_tr1_s5_d1, draw_sprite_f1_ti1_tr1_s6_d1, draw_sprite_f1_ti1_tr1_s7_d1,
	draw_sprite_f1_ti1_tr1_s0_d2, draw_sprite_f1_ti1_tr1_s1_d2, draw_sprite_f1_ti1_tr1_s2_d2, draw_sprite_f1_ti1_tr1_s3_d2, draw_sprite_f1_ti1_tr1_s4_d2, draw_sprite_f1_ti1_tr1_s5_d2, draw_sprite_f1_ti1_tr1_s6_d2, draw_sprite_f1_ti1_tr1_s7_d2,
	draw_sprite_f1_ti1_tr1_s0_d3, draw_sprite_f1_ti1_tr1_s1_d3, draw_sprite_f1_ti1_tr1_s2_d3, draw_sprite_f1_ti1_tr1_s3_d3, draw_sprite_f1_ti1_tr1_s4_d3, draw_sprite_f1_ti1_tr1_s5_d3, draw_sprite_f1_ti1_tr1_s6_d3, draw_sprite_f1_ti1_tr1_s7_d3,
	draw_sprite_f1_ti1_tr1_s0_d4, draw_sprite_f1_ti1_tr1_s1_d4, draw_sprite_f1_ti1_tr1_s2_d4, draw_sprite_f1_ti1_tr1_s3_d4, draw_sprite_f1_ti1_tr1_s4_d4, draw_sprite_f1_ti1_tr1_s5_d4, draw_sprite_f1_ti1_tr1_s6_d4, draw_sprite_f1_ti1_tr1_s7_d4,
	draw_sprite_f1_ti1_tr1_s0_d5, draw_sprite_f1_ti1_tr1_s1_d5, draw_sprite_f1_ti1_tr1_s2_d5, draw_sprite_f1_ti1_tr1_s3_d5, draw_sprite_f1_ti1_tr1_s4_d5, draw_sprite_f1_ti1_tr1_s5_d5, draw_sprite_f1_ti1_tr1_s6_d5, draw_sprite_f1_ti1_tr1_s7_d5,
	draw_sprite_f1_ti1_tr1_s0_d6, draw_sprite_f1_ti1_tr1_s1_d6, draw_sprite_f1_ti1_tr1_s2_d6, draw_sprite_f1_ti1_tr1_s3_d6, draw_sprite_f1_ti1_tr1_s4_d6, draw_sprite_f1_ti1_tr1_s5_d6, draw_sprite_f1_ti1_tr1_s6_d6, draw_sprite_f1_ti1_tr1_s7_d6,
	draw_sprite_f1_ti1_tr1_s0_d7, draw_sprite_f1_ti1_tr1_s1_d7, draw_sprite_f1_ti1_tr1_s2_d7, draw_sprite_f1_ti1_tr1_s3_d7, draw_sprite_f1_ti1_tr1_s4_d7, draw_sprite_f1_ti1_tr1_s5_d7, draw_sprite_f1_ti1_tr1_s6_d7, draw_sprite_f1_ti1_tr1_s7_d7,
};

static epic12_device_blitfunction epic12_device_f1_ti1_tr0_blit_funcs[] =
{
	draw_sprite_f1_ti1_tr0_s0_d0, draw_sprite_f1_ti1_tr0_s1_d0, draw_sprite_f1_ti1_tr0_s2_d0, draw_sprite_f1_ti1_tr0_s3_d0, draw_sprite_f1_ti1_tr0_s4_d0, draw_sprite_f1_ti1_tr0_s5_d0, draw_sprite_f1_ti1_tr0_s6_d0, draw_sprite_f1_ti1_tr0_s7_d0,
	draw_sprite_f1_ti1_tr0_s0_d1, draw_sprite_f1_ti1_tr0_s1_d1, draw_sprite_f1_ti1_tr0_s2_d1, draw_sprite_f1_ti1_tr0_s3_d1, draw_sprite_f1_ti1_tr0_s4_d1, draw_sprite_f1_ti1_tr0_s5_d1, draw_sprite_f1_ti1_tr0_s6_d1, draw_sprite_f1_ti1_tr0_s7_d1,
	draw_sprite_f1_ti1_tr0_s0_d2, draw_sprite_f1_ti1_tr0_s1_d2, draw_sprite_f1_ti1_tr0_s2_d2, draw_sprite_f1_ti1_tr0_s3_d2, draw_sprite_f1_ti1_tr0_s4_d2, draw_sprite_f1_ti1_tr0_s5_d2, draw_sprite_f1_ti1_tr0_s6_d2, draw_sprite_f1_ti1_tr0_s7_d2,
	draw_sprite_f1_ti1_tr0_s0_d3, draw_sprite_f1_ti1_tr0_s1_d3, draw_sprite_f1_ti1_tr0_s2_d3, draw_sprite_f1_ti1_tr0_s3_d3, draw_sprite_f1_ti1_tr0_s4_d3, draw_sprite_f1_ti1_tr0_s5_d3, draw_sprite_f1_ti1_tr0_s6_d3, draw_sprite_f1_ti1_tr0_s7_d3,
	draw_sprite_f1_ti1_tr0_s0_d4, draw_sprite_f1_ti1_tr0_s1_d4, draw_sprite_f1_ti1_tr0_s2_d4, draw_sprite_f1_ti1_tr0_s3_d4, draw_sprite_f1_ti1_tr0_s4_d4, draw_sprite_f1_ti1_tr0_s5_d4, draw_sprite_f1_ti1_tr0_s6_d4, draw_sprite_f1_ti1_tr0_s7_d4,
	draw_sprite_f1_ti1_tr0_s0_d5, draw_sprite_f1_ti1_tr0_s1_d5, draw_sprite_f1_ti1_tr0_s2_d5, draw_sprite_f1_ti1_tr0_s3_d5, draw_sprite_f1_ti1_tr0_s4_d5, draw_sprite_f1_ti1_tr0_s5_d5, draw_sprite_f1_ti1_tr0_s6_d5, draw_sprite_f1_ti1_tr0_s7_d5,
	draw_sprite_f1_ti1_tr0_s0_d6, draw_sprite_f1_ti1_tr0_s1_d6, draw_sprite_f1_ti1_tr0_s2_d6, draw_sprite_f1_ti1_tr0_s3_d6, draw_sprite_f1_ti1_tr0_s4_d6, draw_sprite_f1_ti1_tr0_s5_d6, draw_sprite_f1_ti1_tr0_s6_d6, draw_sprite_f1_ti1_tr0_s7_d6,
	draw_sprite_f1_ti1_tr0_s0_d7, draw_sprite_f1_ti1_tr0_s1_d7, draw_sprite_f1_ti1_tr0_s2_d7, draw_sprite_f1_ti1_tr0_s3_d7, draw_sprite_f1_ti1_tr0_s4_d7, draw_sprite_f1_ti1_tr0_s5_d7, draw_sprite_f1_ti1_tr0_s6_d7, draw_sprite_f1_ti1_tr0_s7_d7,
};



static epic12_device_blitfunction epic12_device_f0_ti0_tr1_blit_funcs[] =
{
	draw_sprite_f0_ti0_tr1_s0_d0, draw_sprite_f0_ti0_tr1_s1_d0, draw_sprite_f0_ti0_tr1_s2_d0, draw_sprite_f0_ti0_tr1_s3_d0, draw_sprite_f0_ti0_tr1_s4_d0, draw_sprite_f0_ti0_tr1_s5_d0, draw_sprite_f0_ti0_tr1_s6_d0, draw_sprite_f0_ti0_tr1_s7_d0,
	draw_sprite_f0_ti0_tr1_s0_d1, draw_sprite_f0_ti0_tr1_s1_d1, draw_sprite_f0_ti0_tr1_s2_d1, draw_sprite_f0_ti0_tr1_s3_d1, draw_sprite_f0_ti0_tr1_s4_d1, draw_sprite_f0_ti0_tr1_s5_d1, draw_sprite_f0_ti0_tr1_s6_d1, draw_sprite_f0_ti0_tr1_s7_d1,
	draw_sprite_f0_ti0_tr1_s0_d2, draw_sprite_f0_ti0_tr1_s1_d2, draw_sprite_f0_ti0_tr1_s2_d2, draw_sprite_f0_ti0_tr1_s3_d2, draw_sprite_f0_ti0_tr1_s4_d2, draw_sprite_f0_ti0_tr1_s5_d2, draw_sprite_f0_ti0_tr1_s6_d2, draw_sprite_f0_ti0_tr1_s7_d2,
	draw_sprite_f0_ti0_tr1_s0_d3, draw_sprite_f0_ti0_tr1_s1_d3, draw_sprite_f0_ti0_tr1_s2_d3, draw_sprite_f0_ti0_tr1_s3_d3, draw_sprite_f0_ti0_tr1_s4_d3, draw_sprite_f0_ti0_tr1_s5_d3, draw_sprite_f0_ti0_tr1_s6_d3, draw_sprite_f0_ti0_tr1_s7_d3,
	draw_sprite_f0_ti0_tr1_s0_d4, draw_sprite_f0_ti0_tr1_s1_d4, draw_sprite_f0_ti0_tr1_s2_d4, draw_sprite_f0_ti0_tr1_s3_d4, draw_sprite_f0_ti0_tr1_s4_d4, draw_sprite_f0_ti0_tr1_s5_d4, draw_sprite_f0_ti0_tr1_s6_d4, draw_sprite_f0_ti0_tr1_s7_d4,
	draw_sprite_f0_ti0_tr1_s0_d5, draw_sprite_f0_ti0_tr1_s1_d5, draw_sprite_f0_ti0_tr1_s2_d5, draw_sprite_f0_ti0_tr1_s3_d5, draw_sprite_f0_ti0_tr1_s4_d5, draw_sprite_f0_ti0_tr1_s5_d5, draw_sprite_f0_ti0_tr1_s6_d5, draw_sprite_f0_ti0_tr1_s7_d5,
	draw_sprite_f0_ti0_tr1_s0_d6, draw_sprite_f0_ti0_tr1_s1_d6, draw_sprite_f0_ti0_tr1_s2_d6, draw_sprite_f0_ti0_tr1_s3_d6, draw_sprite_f0_ti0_tr1_s4_d6, draw_sprite_f0_ti0_tr1_s5_d6, draw_sprite_f0_ti0_tr1_s6_d6, draw_sprite_f0_ti0_tr1_s7_d6,
	draw_sprite_f0_ti0_tr1_s0_d7, draw_sprite_f0_ti0_tr1_s1_d7, draw_sprite_f0_ti0_tr1_s2_d7, draw_sprite_f0_ti0_tr1_s3_d7, draw_sprite_f0_ti0_tr1_s4_d7, draw_sprite_f0_ti0_tr1_s5_d7, draw_sprite_f0_ti0_tr1_s6_d7, draw_sprite_f0_ti0_tr1_s7_d7,
};

static epic12_device_blitfunction epic12_device_f0_ti0_tr0_blit_funcs[] =
{
	draw_sprite_f0_ti0_tr0_s0_d0, draw_sprite_f0_ti0_tr0_s1_d0, draw_sprite_f0_ti0_tr0_s2_d0, draw_sprite_f0_ti0_tr0_s3_d0, draw_sprite_f0_ti0_tr0_s4_d0, draw_sprite_f0_ti0_tr0_s5_d0, draw_sprite_f0_ti0_tr0_s6_d0, draw_sprite_f0_ti0_tr0_s7_d0,
	draw_sprite_f0_ti0_tr0_s0_d1, draw_sprite_f0_ti0_tr0_s1_d1, draw_sprite_f0_ti0_tr0_s2_d1, draw_sprite_f0_ti0_tr0_s3_d1, draw_sprite_f0_ti0_tr0_s4_d1, draw_sprite_f0_ti0_tr0_s5_d1, draw_sprite_f0_ti0_tr0_s6_d1, draw_sprite_f0_ti0_tr0_s7_d1,
	draw_sprite_f0_ti0_tr0_s0_d2, draw_sprite_f0_ti0_tr0_s1_d2, draw_sprite_f0_ti0_tr0_s2_d2, draw_sprite_f0_ti0_tr0_s3_d2, draw_sprite_f0_ti0_tr0_s4_d2, draw_sprite_f0_ti0_tr0_s5_d2, draw_sprite_f0_ti0_tr0_s6_d2, draw_sprite_f0_ti0_tr0_s7_d2,
	draw_sprite_f0_ti0_tr0_s0_d3, draw_sprite_f0_ti0_tr0_s1_d3, draw_sprite_f0_ti0_tr0_s2_d3, draw_sprite_f0_ti0_tr0_s3_d3, draw_sprite_f0_ti0_tr0_s4_d3, draw_sprite_f0_ti0_tr0_s5_d3, draw_sprite_f0_ti0_tr0_s6_d3, draw_sprite_f0_ti0_tr0_s7_d3,
	draw_sprite_f0_ti0_tr0_s0_d4, draw_sprite_f0_ti0_tr0_s1_d4, draw_sprite_f0_ti0_tr0_s2_d4, draw_sprite_f0_ti0_tr0_s3_d4, draw_sprite_f0_ti0_tr0_s4_d4, draw_sprite_f0_ti0_tr0_s5_d4, draw_sprite_f0_ti0_tr0_s6_d4, draw_sprite_f0_ti0_tr0_s7_d4,
	draw_sprite_f0_ti0_tr0_s0_d5, draw_sprite_f0_ti0_tr0_s1_d5, draw_sprite_f0_ti0_tr0_s2_d5, draw_sprite_f0_ti0_tr0_s3_d5, draw_sprite_f0_ti0_tr0_s4_d5, draw_sprite_f0_ti0_tr0_s5_d5, draw_sprite_f0_ti0_tr0_s6_d5, draw_sprite_f0_ti0_tr0_s7_d5,
	draw_sprite_f0_ti0_tr0_s0_d6, draw_sprite_f0_ti0_tr0_s1_d6, draw_sprite_f0_ti0_tr0_s2_d6, draw_sprite_f0_ti0_tr0_s3_d6, draw_sprite_f0_ti0_tr0_s4_d6, draw_sprite_f0_ti0_tr0_s5_d6, draw_sprite_f0_ti0_tr0_s6_d6, draw_sprite_f0_ti0_tr0_s7_d6,
	draw_sprite_f0_ti0_tr0_s0_d7, draw_sprite_f0_ti0_tr0_s1_d7, draw_sprite_f0_ti0_tr0_s2_d7, draw_sprite_f0_ti0_tr0_s3_d7, draw_sprite_f0_ti0_tr0_s4_d7, draw_sprite_f0_ti0_tr0_s5_d7, draw_sprite_f0_ti0_tr0_s6_d7, draw_sprite_f0_ti0_tr0_s7_d7,
};

static epic12_device_blitfunction epic12_device_f1_ti0_tr1_blit_funcs[] =
{
	draw_sprite_f1_ti0_tr1_s0_d0, draw_sprite_f1_ti0_tr1_s1_d0, draw_sprite_f1_ti0_tr1_s2_d0, draw_sprite_f1_ti0_tr1_s3_d0, draw_sprite_f1_ti0_tr1_s4_d0, draw_sprite_f1_ti0_tr1_s5_d0, draw_sprite_f1_ti0_tr1_s6_d0, draw_sprite_f1_ti0_tr1_s7_d0,
	draw_sprite_f1_ti0_tr1_s0_d1, draw_sprite_f1_ti0_tr1_s1_d1, draw_sprite_f1_ti0_tr1_s2_d1, draw_sprite_f1_ti0_tr1_s3_d1, draw_sprite_f1_ti0_tr1_s4_d1, draw_sprite_f1_ti0_tr1_s5_d1, draw_sprite_f1_ti0_tr1_s6_d1, draw_sprite_f1_ti0_tr1_s7_d1,
	draw_sprite_f1_ti0_tr1_s0_d2, draw_sprite_f1_ti0_tr1_s1_d2, draw_sprite_f1_ti0_tr1_s2_d2, draw_sprite_f1_ti0_tr1_s3_d2, draw_sprite_f1_ti0_tr1_s4_d2, draw_sprite_f1_ti0_tr1_s5_d2, draw_sprite_f1_ti0_tr1_s6_d2, draw_sprite_f1_ti0_tr1_s7_d2,
	draw_sprite_f1_ti0_tr1_s0_d3, draw_sprite_f1_ti0_tr1_s1_d3, draw_sprite_f1_ti0_tr1_s2_d3, draw_sprite_f1_ti0_tr1_s3_d3, draw_sprite_f1_ti0_tr1_s4_d3, draw_sprite_f1_ti0_tr1_s5_d3, draw_sprite_f1_ti0_tr1_s6_d3, draw_sprite_f1_ti0_tr1_s7_d3,
	draw_sprite_f1_ti0_tr1_s0_d4, draw_sprite_f1_ti0_tr1_s1_d4, draw_sprite_f1_ti0_tr1_s2_d4, draw_sprite_f1_ti0_tr1_s3_d4, draw_sprite_f1_ti0_tr1_s4_d4, draw_sprite_f1_ti0_tr1_s5_d4, draw_sprite_f1_ti0_tr1_s6_d4, draw_sprite_f1_ti0_tr1_s7_d4,
	draw_sprite_f1_ti0_tr1_s0_d5, draw_sprite_f1_ti0_tr1_s1_d5, draw_sprite_f1_ti0_tr1_s2_d5, draw_sprite_f1_ti0_tr1_s3_d5, draw_sprite_f1_ti0_tr1_s4_d5, draw_sprite_f1_ti0_tr1_s5_d5, draw_sprite_f1_ti0_tr1_s6_d5, draw_sprite_f1_ti0_tr1_s7_d5,
	draw_sprite_f1_ti0_tr1_s0_d6, draw_sprite_f1_ti0_tr1_s1_d6, draw_sprite_f1_ti0_tr1_s2_d6, draw_sprite_f1_ti0_tr1_s3_d6, draw_sprite_f1_ti0_tr1_s4_d6, draw_sprite_f1_ti0_tr1_s5_d6, draw_sprite_f1_ti0_tr1_s6_d6, draw_sprite_f1_ti0_tr1_s7_d6,
	draw_sprite_f1_ti0_tr1_s0_d7, draw_sprite_f1_ti0_tr1_s1_d7, draw_sprite_f1_ti0_tr1_s2_d7, draw_sprite_f1_ti0_tr1_s3_d7, draw_sprite_f1_ti0_tr1_s4_d7, draw_sprite_f1_ti0_tr1_s5_d7, draw_sprite_f1_ti0_tr1_s6_d7, draw_sprite_f1_ti0_tr1_s7_d7,
};

static epic12_device_blitfunction epic12_device_f1_ti0_tr0_blit_funcs[] =
{
	draw_sprite_f1_ti0_tr0_s0_d0, draw_sprite_f1_ti0_tr0_s1_d0, draw_sprite_f1_ti0_tr0_s2_d0, draw_sprite_f1_ti0_tr0_s3_d0, draw_sprite_f1_ti0_tr0_s4_d0, draw_sprite_f1_ti0_tr0_s5_d0, draw_sprite_f1_ti0_tr0_s6_d0, draw_sprite_f1_ti0_tr0_s7_d0,
	draw_sprite_f1_ti0_tr0_s0_d1, draw_sprite_f1_ti0_tr0_s1_d1, draw_sprite_f1_ti0_tr0_s2_d1, draw_sprite_f1_ti0_tr0_s3_d1, draw_sprite_f1_ti0_tr0_s4_d1, draw_sprite_f1_ti0_tr0_s5_d1, draw_sprite_f1_ti0_tr0_s6_d1, draw_sprite_f1_ti0_tr0_s7_d1,
	draw_sprite_f1_ti0_tr0_s0_d2, draw_sprite_f1_ti0_tr0_s1_d2, draw_sprite_f1_ti0_tr0_s2_d2, draw_sprite_f1_ti0_tr0_s3_d2, draw_sprite_f1_ti0_tr0_s4_d2, draw_sprite_f1_ti0_tr0_s5_d2, draw_sprite_f1_ti0_tr0_s6_d2, draw_sprite_f1_ti0_tr0_s7_d2,
	draw_sprite_f1_ti0_tr0_s0_d3, draw_sprite_f1_ti0_tr0_s1_d3, draw_sprite_f1_ti0_tr0_s2_d3, draw_sprite_f1_ti0_tr0_s3_d3, draw_sprite_f1_ti0_tr0_s4_d3, draw_sprite_f1_ti0_tr0_s5_d3, draw_sprite_f1_ti0_tr0_s6_d3, draw_sprite_f1_ti0_tr0_s7_d3,
	draw_sprite_f1_ti0_tr0_s0_d4, draw_sprite_f1_ti0_tr0_s1_d4, draw_sprite_f1_ti0_tr0_s2_d4, draw_sprite_f1_ti0_tr0_s3_d4, draw_sprite_f1_ti0_tr0_s4_d4, draw_sprite_f1_ti0_tr0_s5_d4, draw_sprite_f1_ti0_tr0_s6_d4, draw_sprite_f1_ti0_tr0_s7_d4,
	draw_sprite_f1_ti0_tr0_s0_d5, draw_sprite_f1_ti0_tr0_s1_d5, draw_sprite_f1_ti0_tr0_s2_d5, draw_sprite_f1_ti0_tr0_s3_d5, draw_sprite_f1_ti0_tr0_s4_d5, draw_sprite_f1_ti0_tr0_s5_d5, draw_sprite_f1_ti0_tr0_s6_d5, draw_sprite_f1_ti0_tr0_s7_d5,
	draw_sprite_f1_ti0_tr0_s0_d6, draw_sprite_f1_ti0_tr0_s1_d6, draw_sprite_f1_ti0_tr0_s2_d6, draw_sprite_f1_ti0_tr0_s3_d6, draw_sprite_f1_ti0_tr0_s4_d6, draw_sprite_f1_ti0_tr0_s5_d6, draw_sprite_f1_ti0_tr0_s6_d6, draw_sprite_f1_ti0_tr0_s7_d6,
	draw_sprite_f1_ti0_tr0_s0_d7, draw_sprite_f1_ti0_tr0_s1_d7, draw_sprite_f1_ti0_tr0_s2_d7, draw_sprite_f1_ti0_tr0_s3_d7, draw_sprite_f1_ti0_tr0_s4_d7, draw_sprite_f1_ti0_tr0_s5_d7, draw_sprite_f1_ti0_tr0_s6_d7, draw_sprite_f1_ti0_tr0_s7_d7,
};


static void gfx_draw(UINT32 *addr)
{
	int x,y, dimx,dimy, flipx,flipy;//, src_p;
	int trans,blend, s_mode, d_mode;
	clr_t tint_clr;
	int tinted = 0;

	UINT16 attr     =   READ_NEXT_WORD(addr);
	UINT16 alpha    =   READ_NEXT_WORD(addr);
	UINT16 src_x    =   READ_NEXT_WORD(addr);
	UINT16 src_y    =   READ_NEXT_WORD(addr);
	UINT16 dst_x_start  =   READ_NEXT_WORD(addr);
	UINT16 dst_y_start  =   READ_NEXT_WORD(addr);
	UINT16 w        =   READ_NEXT_WORD(addr);
	UINT16 h        =   READ_NEXT_WORD(addr);
	UINT16 tint_r   =   READ_NEXT_WORD(addr);
	UINT16 tint_gb  =   READ_NEXT_WORD(addr);

	// 0: +alpha
	// 1: +source
	// 2: +dest
	// 3: *
	// 4: -alpha
	// 5: -source
	// 6: -dest
	// 7: *

	d_mode  =    attr & 0x0007;
	s_mode  =   (attr & 0x0070) >> 4;

	trans   =    attr & 0x0100;
	blend   =      attr & 0x0200;

	flipy   =    attr & 0x0400;
	flipx   =    attr & 0x0800;

	const UINT8 d_alpha =   ((alpha & 0x00ff)       )>>3;
	const UINT8 s_alpha =   ((alpha & 0xff00) >> 8  )>>3;

//  src_p   =   0;
	src_x   =   src_x & 0x1fff;
	src_y   =   src_y & 0x0fff;


	x       =   (dst_x_start & 0x7fff) - (dst_x_start & 0x8000);
	y       =   (dst_y_start & 0x7fff) - (dst_y_start & 0x8000);

	dimx    =   (w & 0x1fff) + 1;
	dimy    =   (h & 0x0fff) + 1;

	// convert parameters to clr


	tint_to_clr(tint_r & 0x00ff, (tint_gb >>  8) & 0xff, tint_gb & 0xff, &tint_clr);

	/* interestingly this gets set to 0x20 for 'normal' not 0x1f */

	if (tint_clr.r!=0x20)
		tinted = 1;

	if (tint_clr.g!=0x20)
		tinted = 1;

	if (tint_clr.b!=0x20)
		tinted = 1;


	// surprisingly frequent, need to verify if it produces a worthwhile speedup tho.
	if ((s_mode==0 && s_alpha==0x1f) && (d_mode==4 && d_alpha==0x1f))
		blend = 0;

	if (tinted)
	{
		if (!flipx)
		{
			if (trans)
			{
				if (!blend)
				{
					draw_sprite_f0_ti1_tr1_plain(draw_params);
				}
				else
				{
					epic12_device_f0_ti1_tr1_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
			else
			{
			if (!blend)
				{
					draw_sprite_f0_ti1_tr0_plain(draw_params);
				}
				else
				{
					epic12_device_f0_ti1_tr0_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
		}
		else // flipx
		{
			if (trans)
			{
				if (!blend)
				{
					draw_sprite_f1_ti1_tr1_plain(draw_params);
				}
				else
				{
					epic12_device_f1_ti1_tr1_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
			else
			{
			if (!blend)
				{
					draw_sprite_f1_ti1_tr0_plain(draw_params);
				}
				else
				{
					epic12_device_f1_ti1_tr0_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
		}
	}
	else
	{
		if (blend==0 && tinted==0)
		{
			if (!flipx)
			{
				if (trans)
				{
					draw_sprite_f0_ti0_tr1_simple(draw_params);
				}
				else
				{
					draw_sprite_f0_ti0_tr0_simple(draw_params);
				}
			}
			else
			{
				if (trans)
				{
					draw_sprite_f1_ti0_tr1_simple(draw_params);
				}
				else
				{
					draw_sprite_f1_ti0_tr0_simple(draw_params);
				}

			}

			return;
		}



		//printf("smode %d dmode %d\n", s_mode, d_mode);

		if (!flipx)
		{
			if (trans)
			{
				if (!blend)
				{
					draw_sprite_f0_ti0_plain(draw_params);
				}
				else
				{
					epic12_device_f0_ti0_tr1_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
			else
			{
			if (!blend)
				{
					draw_sprite_f0_ti0_tr0_plain(draw_params);
				}
				else
				{
					epic12_device_f0_ti0_tr0_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
		}
		else // flipx
		{
			if (trans)
			{
				if (!blend)
				{
					draw_sprite_f1_ti0_plain(draw_params);
				}
				else
				{
					epic12_device_f1_ti0_tr1_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
			else
			{
			if (!blend)
				{
					draw_sprite_f1_ti0_tr0_plain(draw_params);
				}
				else
				{
					epic12_device_f1_ti0_tr0_blit_funcs[s_mode | (d_mode<<3)](draw_params);
				}
			}
		}
	}
}


static void gfx_exec()
{
	UINT32 addr = m_gfx_addr & 0x1fffffff;
	m_clip.set(m_gfx_scroll_1_x, m_gfx_scroll_1_x + 320-1, m_gfx_scroll_1_y, m_gfx_scroll_1_y + 240-1);

//  logerror("GFX EXEC: %08X\n", addr);

	while (1)
	{
		UINT16 data = READ_NEXT_WORD(&addr);

		switch( data & 0xf000 )
		{
			case 0x0000:
			case 0xf000:
				return;

			case 0xc000:
				if (READ_NEXT_WORD(&addr)) // cliptype
					m_clip.set(m_gfx_scroll_1_x, m_gfx_scroll_1_x + 320-1, m_gfx_scroll_1_y, m_gfx_scroll_1_y + 240-1);
				else
					m_clip.set(0, 0x2000-1, 0, 0x1000-1);
				break;

			case 0x2000:
				addr -= 2;
				gfx_upload(&addr);
				break;

			case 0x1000:
				addr -= 2;
				gfx_draw(&addr);
				break;

			default:
				//popmessage("GFX op = %04X", data);
				return;
		}
	}
}



static UINT32 gfx_ready_read()
{
	if (m_blitter_busy)
	{
		//m_maincpu->spin_until_time(attotime::from_usec(10));
		Sh3BurnCycles(m_burn_cycles); // 0x400 @ (12800000*8)
		//bprintf(0, _T("%d frame - blitter busy read....."), nCurrentFrame);

		return 0x00000000;
	}
	else
		return 0x00000010;
}

void epic12_wait_blitterthread()
{
	thready.notify_wait();
}

static void gfx_exec_write(UINT32 offset, UINT32 data)
{
//	if ( ACCESSING_BITS_0_7 )
	{
		if (data & 1)
		{
			thready.notify_wait();

			if (epic12_device_blit_delay && m_delay_scale)
			{
				m_blitter_busy = 1;
				int delay = epic12_device_blit_delay*(15 * m_delay_scale / 50);
				INT32 cycles = (INT32)((double)((double)delay / 1000000000) * sh4_get_cpu_speed());

				sh4_set_cave_blitter_delay_timer(cycles);
			}
			else
			{
				m_blitter_busy = 0;
			}

			epic12_device_blit_delay = 0;
			thready.notify(); // run gfx_exec(), w/ threading if set (via dip option)
		}
	}
}

static void pal16_check_init()
{
	if (nBurnBpp < 3 && !pal16) {
		pal16 = (UINT16 *)BurnMalloc((1 << 24) * sizeof (UINT16));

		for (INT32 i = 0; i < (1 << 24); i++) {
			pal16[i] = BurnHighCol(i / 0x10000, (i / 0x100) & 0xff, i & 0xff, 0);
		}
	}
}

static void epic12_draw_screen16_24bpp()
{
	INT32 scrollx = -m_gfx_scroll_0_x;
	INT32 scrolly = -m_gfx_scroll_0_y;

	UINT8  *dst = (UINT8  *)pBurnDraw;
	UINT32 *src = (UINT32 *)m_bitmaps;
	const INT32 heightmask = 0x1000 - 1;
	const INT32 widthmask  = 0x2000 - 1;

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		UINT32 *s0 = &src[((y - scrolly) & heightmask) * 0x2000];
		INT32 sx;

		switch (nBurnBpp) {
			case 2: // 16bpp
				for (INT32 x = 0; x < nScreenWidth; x++, dst += nBurnBpp)
				{
					sx = x - scrollx;
					PutPix(dst, pal16[s0[sx & widthmask]&((1<<24)-1)]);
				}
				break;
			case 3: // 24bpp
				for (INT32 x = 0; x < nScreenWidth; x++, dst += nBurnBpp)
				{
					sx = x - scrollx;
					PutPix(dst, s0[sx & widthmask]);
				}
				break;
		}
	}
}

void epic12_draw_screen(UINT8 &recalc_palette)
{
	INT32 scrollx = -m_gfx_scroll_0_x;
	INT32 scrolly = -m_gfx_scroll_0_y;

	if (nBurnBpp != 4) {
		if (recalc_palette) {
			pal16_check_init();
			recalc_palette = 0;
		}

		epic12_draw_screen16_24bpp();
		return;
	}

	UINT32 *dst = (UINT32 *)pBurnDraw;
	UINT32 *src = (UINT32 *)m_bitmaps;
	const INT32 heightmask = 0x1000 - 1;
	const INT32 widthmask  = 0x2000 - 1;

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		UINT32 *s0 = &src[((y - scrolly) & heightmask) * 0x2000];
		UINT32 *d0 = dst + (y * nScreenWidth);
		INT32 sx;
		for (INT32 x = 0; x < nScreenWidth; x+=16)
		{
			sx = x - scrollx;
			d0[x + 0] = s0[((sx + 0)) & widthmask];
			d0[x + 1] = s0[((sx + 1)) & widthmask];
			d0[x + 2] = s0[((sx + 2)) & widthmask];
			d0[x + 3] = s0[((sx + 3)) & widthmask];
			d0[x + 4] = s0[((sx + 4)) & widthmask];
			d0[x + 5] = s0[((sx + 5)) & widthmask];
			d0[x + 6] = s0[((sx + 6)) & widthmask];
			d0[x + 7] = s0[((sx + 7)) & widthmask];
			d0[x + 8] = s0[((sx + 8)) & widthmask];
			d0[x + 9] = s0[((sx + 9)) & widthmask];
			d0[x +10] = s0[((sx +10)) & widthmask];
			d0[x +11] = s0[((sx +11)) & widthmask];
			d0[x +12] = s0[((sx +12)) & widthmask];
			d0[x +13] = s0[((sx +13)) & widthmask];
			d0[x +14] = s0[((sx +14)) & widthmask];
			d0[x +15] = s0[((sx +15)) & widthmask];
		}
	}
}



// 0x18000000 - 0x18000057

UINT32 epic12_blitter_read(UINT32 offset)
{
	switch (offset)
	{
		case 0x10:
			return gfx_ready_read();

		case 0x24:
			return 0xffffffff;

		case 0x28:
			return 0xffffffff;

		case 0x50:
			return *dips;

		default:
			//logerror("unknownblitter_r %08x %08x\n", offset*4, mem_mask);
			break;

	}
	return 0;
}


void epic12_blitter_write(UINT32 offset, UINT32 data)
{
	switch (offset)
	{
		case 0x04:
			gfx_exec_write(offset,data);
			break;

		case 0x08:
			m_gfx_addr = data & 0xffffff;
			break;

		case 0x14:
			m_gfx_scroll_0_x = data;
			break;

		case 0x18:
			m_gfx_scroll_0_y = data;
			break;

		case 0x40:
			m_gfx_scroll_1_x = data;
			break;

		case 0x44:
			m_gfx_scroll_1_y = data;
			break;
	}
}

void epic12_scan(INT32 nAction, INT32 *pnMin)
{
	SCAN_VAR(m_gfx_addr);
	SCAN_VAR(m_gfx_scroll_0_x);
	SCAN_VAR(m_gfx_scroll_0_y);
	SCAN_VAR(m_gfx_scroll_1_x);
	SCAN_VAR(m_gfx_scroll_1_y);
	SCAN_VAR(epic12_device_blit_delay);
	SCAN_VAR(m_delay_scale);
	SCAN_VAR(m_blitter_busy);
	if (~nAction & ACB_RUNAHEAD) {
		ScanVar(m_bitmaps, m_gfx_size * 4, "epic12 vram");
	}
	thready.scan();
}

