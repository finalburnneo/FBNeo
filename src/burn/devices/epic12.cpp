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
#include <math.h> // floor()

static const int EP1C_VRAM_CLK_NANOSEC = 13;
static const int EP1C_SRAM_CLK_NANOSEC = 20;
static const int EP1C_VRAM_H_LINE_PERIOD_NANOSEC = 63600;
static const int EP1C_VRAM_H_LINE_DURATION_NANOSEC = 2160;
static const int EP1C_FRAME_DURATION_NANOSEC = 16666666;
static const int EP1C_DRAW_OPERATION_SIZE_BYTES = 20;
static const int EP1C_CLIP_OPERATION_SIZE_BYTES = 2;

// When looking at VRAM viewer in Special mode in Muchi Muchi Pork, draws 32 pixels outside of
// the "clip area" is visible. This is likely why the frame buffers will have at least a 32 pixel offset
// from the VRAM borders or other buffers in all games.
static int EP1C_CLIP_MARGIN = 32;
// note: configurable via dipsetting  -dink may 2023

// Number of bytes that are read each time Blitter fetches operations from SRAM.
static const int OPERATION_CHUNK_SIZE_BYTES = 64;

// Approximate time it takes to fetch a chunk of operations.
// This is composed of the time that the Blitter holds the Bus Request (BREQ) signal
// of the SH-3, as well as the overhead between requests.
static const int OPERATION_READ_CHUNK_INTERVAL_NS = 700;

static UINT64 m_blit_delay_ns = 0;
static UINT16 m_blit_idle_op_bytes = 0;

static void idle_blitter(UINT8 operation_size_bytes)
{
	m_blit_idle_op_bytes += operation_size_bytes;
	if (m_blit_idle_op_bytes >= OPERATION_CHUNK_SIZE_BYTES)
	{
		m_blit_idle_op_bytes -= OPERATION_CHUNK_SIZE_BYTES;
		m_blit_delay_ns += OPERATION_READ_CHUNK_INTERVAL_NS;
	}
}

static UINT16* m_ram16;
static UINT16* m_ram16_copy;
static UINT32 m_gfx_addr;
static UINT32 m_gfx_addr_shadowcopy;
static UINT32 m_gfx_scroll_x, m_gfx_scroll_y;
static UINT32 m_gfx_clip_x, m_gfx_clip_y;
static UINT32 m_gfx_clip_x_shadowcopy, m_gfx_clip_y_shadowcopy;

static int m_gfx_size;
static UINT32 *m_bitmaps;
static rectangle m_clip;

static UINT64 epic12_device_blit_delay;
static int m_blitter_busy;

static int m_main_ramsize; // type D has double the main ram
static int m_main_rammask;

static int m_delay_method;
static int m_delay_scale;
static int m_burn_cycles;

static INT32 sleep_on_busy = 1;

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
	BurnFree(m_ram16_copy);

	if (pal16) {
		BurnFree(pal16);
		pal16 = NULL;
	}
}

void epic12_init(INT32 ram_size, UINT16 *ram, UINT8 *dippy)
{
	m_main_ramsize = ram_size;
	m_main_rammask = ram_size - 1;

	m_ram16 = ram;

	m_ram16_copy = (UINT16*)BurnMalloc(ram_size);

	dips = dippy;

	m_gfx_size = 0x2000 * 0x1000;
	m_bitmaps = (UINT32*)BurnMalloc (m_gfx_size * 4);

	m_clip.set(0, 0x2000-1, 0, 0x1000-1);

	m_delay_method = 0; // accurate
	m_delay_scale = 50;
	m_blitter_busy = 0;
	m_gfx_addr = 0;
	m_gfx_addr_shadowcopy = 0;
	m_gfx_scroll_x = 0;
	m_gfx_scroll_y = 0;
	m_gfx_clip_x = 0;
	m_gfx_clip_y = 0;
	m_gfx_clip_x_shadowcopy = 0;
	m_gfx_clip_y_shadowcopy = 0;
	epic12_device_blit_delay = 0;

	m_blit_delay_ns = 0;
	m_blit_idle_op_bytes = 0;

	epic12_set_blitter_clipping_margin(1);
	epic12_set_blitter_sleep_on_busy(1);

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

void epic12_set_blitterdelay_method(INT32 delay_method) // 0 = accurate, !0 = ancient
{
	m_delay_method = delay_method;
}

void epic12_set_blitter_clipping_margin(INT32 c_margin_on) // ??? not sure.
{
	EP1C_CLIP_MARGIN = (c_margin_on) ? 32 : 0;
}

void epic12_set_blitter_sleep_on_busy(INT32 busysleep_on)
{
	sleep_on_busy = (busysleep_on) ? 1 : 0;
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
	m_gfx_addr_shadowcopy = 0;
	m_gfx_scroll_x = 0;
	m_gfx_scroll_y = 0;
	m_gfx_clip_x = 0;
	m_gfx_clip_y = 0;
	m_gfx_clip_x_shadowcopy = 0;
	m_gfx_clip_y_shadowcopy = 0;
	epic12_device_blit_delay = 0;
	m_blit_delay_ns = 0;
	m_blit_idle_op_bytes = 0;

	thready.reset();
}

static inline UINT16 READ_NEXT_WORD(UINT32 *addr)
{
	const UINT16 data = m_ram16_copy[((*addr & m_main_rammask) >> 1)];

	*addr += 2;

	return data;
}

static inline UINT16 COPY_NEXT_WORD(UINT32 *addr)
{
	const UINT16 data = m_ram16[((*addr & m_main_rammask) >> 1)];
	m_ram16_copy[((*addr & m_main_rammask) >> 1)] = data;

	*addr += 2;

	return data;
}

static void gfx_upload_shadow_copy(UINT32 *addr)
{
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);

	const UINT32 dimx = (COPY_NEXT_WORD(addr) & 0x1fff) + 1;
	const UINT32 dimy = (COPY_NEXT_WORD(addr) & 0x0fff) + 1;

	for (UINT32 y = 0; y < dimy; y++)
	{
		for (UINT32 x = 0; x < dimx; x++)
		{
			COPY_NEXT_WORD(addr);
		}
	}

	// Time spent on uploads is mostly due to Main RAM accesses.
	// The Blitter will send BREQ requests to the SH-3, to access Main RAM
	// and then write it to VRAM.
	// The number of bytes to read are the sum of a 16b fixed header and the pixel
	// data (2 byte per pixel). RAM accesses are 32bit, so divide by four for clocks.
	//
	// TODO: There's additional overhead to these request thats are not included. The BREQ
	// assertion also puts CPU into WAIT, if it needs uncached RAM accesses.
	int num_sram_clk = (16 + dimx * dimy * 2 ) / 4;
	m_blit_delay_ns += num_sram_clk * EP1C_SRAM_CLK_NANOSEC;
	m_blit_idle_op_bytes = 0;
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

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/*
    Calculate number of VRAM row accesses a draw will perform.
    Source data will typically be aligned well with VRAM, but this is not the case for the destination.
    As an example, drawing a 64x32 pixel image will usually read from two VRAM rows for source data,
    but if the destination start coordinate is (x=10, y=10), each of the 32x32px chunks of source data will
    touch 4 rows of destination VRAM, leading to a total of 8 destination VRAM accesses.
*/
inline UINT16 calculate_vram_accesses(UINT16 start_x, UINT16 start_y, UINT16 dimx, UINT16 dimy)
{
	int x_rows = 0;
	int num_vram_rows = 0;
	for (int x_pixels = dimx; x_pixels > 0; x_pixels -= 32)
	{
		x_rows++;
		if (((start_x & 31) + MIN(32, x_pixels)) > 32)
			x_rows++;  // Drawing across multiple horizontal VRAM row boundaries.
	}
	for (int y_pixels = dimy; y_pixels > 0; y_pixels -= 32)
	{
		num_vram_rows += x_rows;
		if (((start_y & 31) + MIN(32, y_pixels)) > 32)
			num_vram_rows += x_rows;  // Drawing across multiple vertical VRAM row boundaries.	
	}
	return num_vram_rows;
}

static void gfx_draw_shadow_copy(UINT32 *addr)
{
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);
	UINT16 src_x_start = COPY_NEXT_WORD(addr);
	UINT16 src_y_start = COPY_NEXT_WORD(addr);
	UINT16 dst_x_start = COPY_NEXT_WORD(addr);
	UINT16 dst_y_start = COPY_NEXT_WORD(addr);
	UINT16 src_dimx = (COPY_NEXT_WORD(addr) & 0x1fff) + 1;
	UINT16 src_dimy = (COPY_NEXT_WORD(addr) & 0x0fff) + 1;
	COPY_NEXT_WORD(addr);
	COPY_NEXT_WORD(addr);

	// Calculate Blitter delay for the Draw operation.
	// On real hardware, the Blitter will read operations into a FIFO queue
	// by asserting BREQ on the SH3 and then reading from Main RAM.
	// Since the reads are done concurrently to executions of operations, its
	// ok to estimate the delay all at once instead for emulation purposes.
	
	UINT16 dst_x_end = dst_x_start + src_dimx - 1;
	UINT16 dst_y_end = dst_y_start + src_dimy - 1;

	// Sprites fully outside of clipping area should not be drawn.
	if (dst_x_start > m_clip.max_x || dst_x_end < m_clip.min_x || dst_y_start > m_clip.max_y || dst_y_end < m_clip.min_y)
	{
		idle_blitter(EP1C_DRAW_OPERATION_SIZE_BYTES);
		return;
	}

	// Clip the blitter operations, to have the calculations only respect the area being written.
	// It's not 100% clear this is how this is performed, but it is clear that there should be some amount of clipping
	// applied here to match the hardware. This way seems most likely, and maps well to the delays seen on hardware.
	// One example of this being utilized heavily is the transparent fog in Mushihimesama Futari Stage 1. This is drawn as
	// 256x256 sprites, with large parts clipped away.

	dst_x_start = MAX(dst_x_start, (UINT16)m_clip.min_x);
	dst_y_start = MAX(dst_y_start, (UINT16)m_clip.min_y);
	dst_x_end = MIN(dst_x_end, (UINT16)m_clip.max_x);
	dst_y_end = MIN(dst_y_end, (UINT16)m_clip.max_y);
	src_dimx = dst_x_end - dst_x_start + 1;
	src_dimy = dst_y_end - dst_y_start + 1;

	m_blit_idle_op_bytes = 0;  // Blitter no longer idle.

	// VRAM data is laid out in 32x32 pixel rows. Calculate amount of rows accessed. 
	int src_num_vram_rows = calculate_vram_accesses(src_x_start, src_y_start, src_dimx, src_dimy);
	int dst_num_vram_rows = calculate_vram_accesses(dst_x_start, dst_y_start, src_dimx, src_dimy);

	// Since draws are done 4 pixels at the time, extend the draw area to coordinates aligned for this.
	// Doing this after VRAM calculations simplify things a bit, and these extensions will never make the
	// destination area span additional VRAM rows. 
	dst_x_start -= dst_x_start & 3;
	dst_x_end += (4 - ((dst_x_end + 1) & 3)) & 3;
	UINT16 dst_dimx = dst_x_end - dst_x_start + 1;
	UINT16 dst_dimy = dst_y_end - dst_y_start + 1;

	// Number of VRAM CLK cycles needed to draw a sprite is sum of:
	// - Number of pixels read from source divided by 4 (Each CLK reads 4 pixels, since 32bit DDR).
	// - Number of pixels read from destination divided by 4.
	// - Pixels written to destination divided by 4.
	// - VRAM access overhead:
	//   - 6 CLK of overhead after each read from a source VRAM row.
	//   - 20 CLK of overhead between read and write of each destination VRAM row.
	//   - 11 CLK of overhead after each write to a destination VRAM row.
	// - 12 CLK of additional overhead per sprite at the end of writing.
	// Note: Details are from https://buffis.com/docs/CV1000_Blitter_Research_by_buffi.pdf
	//       There may be mistakes.	
	UINT32 num_vram_clk = src_dimx * src_dimy / 4 + dst_dimx * dst_dimy / 2 + src_num_vram_rows * 6 + dst_num_vram_rows * (20 + 11) + 12;
	m_blit_delay_ns += num_vram_clk * EP1C_VRAM_CLK_NANOSEC;
}

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

static void gfx_create_shadow_copy()
{
	UINT32 addr = m_gfx_addr & 0x1fffffff;

	m_clip.set(m_gfx_clip_x_shadowcopy - EP1C_CLIP_MARGIN, m_gfx_clip_x_shadowcopy + 320 - 1 + EP1C_CLIP_MARGIN,
			   m_gfx_clip_y_shadowcopy - EP1C_CLIP_MARGIN, m_gfx_clip_y_shadowcopy + 240 - 1 + EP1C_CLIP_MARGIN);

	while (1)
	{
		// request commands from main CPU RAM
		const UINT16 data = COPY_NEXT_WORD(&addr);

		switch (data & 0xf000)
		{
			case 0x0000:
			case 0xf000:
				return;

			case 0xc000:
				if (COPY_NEXT_WORD(&addr)) // cliptype
					m_clip.set(m_gfx_clip_x_shadowcopy - EP1C_CLIP_MARGIN, m_gfx_clip_x_shadowcopy + 320 - 1 + EP1C_CLIP_MARGIN,
							   m_gfx_clip_y_shadowcopy - EP1C_CLIP_MARGIN, m_gfx_clip_y_shadowcopy + 240 - 1 + EP1C_CLIP_MARGIN);
				else
					m_clip.set(0, 0x2000 - 1, 0, 0x1000 - 1);
				idle_blitter(EP1C_CLIP_OPERATION_SIZE_BYTES);
				break;

			case 0x2000:
				addr -= 2;
				gfx_upload_shadow_copy(&addr);
				break;

			case 0x1000:
				addr -= 2;
				gfx_draw_shadow_copy(&addr);
				break;

			default:
				//popmessage("GFX op = %04X", data);
				return;
		}
	}
}


static void gfx_exec()
{
	UINT32 addr = m_gfx_addr_shadowcopy & 0x1fffffff;
	m_clip.set(m_gfx_clip_x_shadowcopy - EP1C_CLIP_MARGIN, m_gfx_clip_x_shadowcopy + 320 - 1 + EP1C_CLIP_MARGIN,
			   m_gfx_clip_y_shadowcopy - EP1C_CLIP_MARGIN, m_gfx_clip_y_shadowcopy + 240 - 1 + EP1C_CLIP_MARGIN);

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
					m_clip.set(m_gfx_clip_x_shadowcopy - EP1C_CLIP_MARGIN, m_gfx_clip_x_shadowcopy + 320 - 1 + EP1C_CLIP_MARGIN,
							   m_gfx_clip_y_shadowcopy - EP1C_CLIP_MARGIN, m_gfx_clip_y_shadowcopy + 240 - 1 + EP1C_CLIP_MARGIN);
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


void epic12_wait_blitterthread()
{
	thready.notify_wait();
}

static void gfx_exec_write(UINT32 data)
{
//	if ( ACCESSING_BITS_0_7 )
	{
		if (data & 1)
		{
			thready.notify_wait();

			m_gfx_clip_x_shadowcopy = m_gfx_clip_x;
			m_gfx_clip_y_shadowcopy = m_gfx_clip_y;

			// Create a copy of the blit list so we can safely thread it.
			// Copying the Blitter operations will also estimate the delay needed for processing.
			m_blit_delay_ns = 0;
			gfx_create_shadow_copy();

			m_gfx_addr_shadowcopy = m_gfx_addr;

			if (m_delay_method) // old method
			{
				if (epic12_device_blit_delay && m_delay_scale)
				{
					m_blitter_busy = 1;
					int delay = epic12_device_blit_delay*(15 * m_delay_scale / 50);
					INT32 cycles = (INT32)((double)((double)delay / 1000000000) * sh4_get_cpu_speed());

					//bprintf(0, _T("old_blitter_delay  %d   cycles  %d\n"),delay, cycles);

					sh4_set_cave_blitter_delay_timer(cycles);
				}
				else
				{
					m_blitter_busy = 0;
				}
			} else {  // new method (buffis)
				// Every EP1C_VRAM_H_LINE_PERIOD_NANOSEC, the Blitter will block other operations, due
				// to fetching a horizontal line from VRAM for output.
				m_blit_delay_ns += floor( m_blit_delay_ns / EP1C_VRAM_H_LINE_PERIOD_NANOSEC ) * EP1C_VRAM_H_LINE_DURATION_NANOSEC;

				// Check if Blitter takes longer than a frame to render.
				// In practice, there's a bit less time than this to allow for lack of slowdown but
				// for debugging purposes this is an ok approximation.
				//if (m_blit_delay_ns > EP1C_FRAME_DURATION_NANOSEC)
				//	LOGDBG("Blitter delay! Blit duration %lld ns.\n", m_blit_delay_ns);

				m_blitter_busy = 1;

				INT32 cycles = (INT32)((double)((double)m_blit_delay_ns / 1000000000) * sh4_get_cpu_speed());

				//bprintf(0, _T("new blit_delay  %I64d   cycles  %d\n"),m_blit_delay_ns, cycles);

				sh4_set_cave_blitter_delay_timer(cycles);
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
	INT32 scrollx = -m_gfx_scroll_x;
	INT32 scrolly = -m_gfx_scroll_y;

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
	INT32 scrollx = -m_gfx_scroll_x;
	INT32 scrolly = -m_gfx_scroll_y;

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
			if (m_blitter_busy)
			{
				//m_maincpu->spin_until_time(attotime::from_usec(10));
				if (sleep_on_busy) {
					Sh3BurnCycles(m_burn_cycles); // 0x400 @ (12800000*8)
				}
				//bprintf(0, _T("%d frame - blitter busy read....."), nCurrentFrame);

				return 0x00000000;
			}
			else
				return 0x00000010;

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
			gfx_exec_write(data);
			break;

		case 0x08:
			m_gfx_addr = data & 0xffffff;
			break;

		case 0x14:
			m_gfx_scroll_x = data;
			break;

		case 0x18:
			m_gfx_scroll_y = data;
			break;

		case 0x40:
			m_gfx_clip_x = data;
			break;

		case 0x44:
			m_gfx_clip_y = data;
			break;
	}
}

void epic12_scan(INT32 nAction, INT32 *pnMin)
{
	SCAN_VAR(m_gfx_addr);
//	SCAN_VAR(m_gfx_addr_shadowcopy); // probably not needed!
	SCAN_VAR(m_gfx_scroll_x);
	SCAN_VAR(m_gfx_scroll_y);
	SCAN_VAR(m_gfx_clip_x);
	SCAN_VAR(m_gfx_clip_y);
//	SCAN_VAR(m_gfx_clip_x_shadowcopy);
//	SCAN_VAR(m_gfx_clip_y_shadowcopy);
	SCAN_VAR(epic12_device_blit_delay);
	SCAN_VAR(m_delay_scale);
	SCAN_VAR(m_blitter_busy);
	SCAN_VAR(m_blit_delay_ns);
	SCAN_VAR(m_blit_idle_op_bytes);

	if (~nAction & ACB_RUNAHEAD) {
		ScanVar(m_bitmaps, m_gfx_size * 4, "epic12 vram");
	}
	thready.scan();
}

