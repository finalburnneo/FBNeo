// see tait_f3.cpp for credits

#include "tiles_generic.h"
#include "taitof3_video.h"
#include "taito.h"

UINT32 sprite_lag;
UINT32 extended_layers;

UINT8 *TaitoF3CtrlRAM;
UINT8 *TaitoF3PfRAM;
UINT8 *TaitoF3LineRAM;

void (*pPaletteUpdateCallback)(UINT16) = NULL;
UINT8 TaitoF3PalRecalc;

static UINT16 *pal16 = NULL; // fast palette for 16bpp video emulation

UINT16 *bitmap_layer[10];
UINT8 *bitmap_flags[10];
INT32 bitmap_width[8];
UINT32 *output_bitmap;

UINT8 *tile_opaque_sp;
UINT8 *tile_opaque_pf[8];
UINT8 *dirty_tiles;
UINT8 dirty_tile_count[10];

static INT32 flipscreen = 0;
// set each frame
static UINT32 sprite_pen_mask;
static UINT32 sprite_pri_usage;
static INT32 sprite_extra_planes = 0;

struct tempsprite
{
	INT32 code, color;
	INT32 flipx, flipy;
	INT32 x, y;
	INT32 zoomx, zoomy;
	INT32 pri;
	INT32 rampos;
};

static struct tempsprite *m_spritelist;
static const struct tempsprite *m_sprite_end;

void TaitoF3VideoReset()
{
	flipscreen = 0;

	sprite_pen_mask = 0;
	sprite_pri_usage = 0;
	sprite_extra_planes = 0;
}



static INT32 min_x = 0;
static INT32 max_x = 512;
static INT32 min_y = 0;
static INT32 max_y = 256;


static INT32 m_width_mask=0x3ff;
static INT32 m_twidth_mask=0x7f;
static INT32 m_twidth_mask_bit=7;

#define ORIENTATION_FLIP_Y	1


static UINT16 *m_src0;
static UINT16 *m_src_s0;
static UINT16 *m_src_e0;
static UINT16 m_clip_al0;
static UINT16 m_clip_ar0;
static UINT16 m_clip_bl0;
static UINT16 m_clip_br0;
static UINT8 *m_tsrc0;
static UINT8 *m_tsrc_s0;
static UINT32 m_x_count0;
static UINT32 m_x_zoom0;
static UINT16 *m_src1;
static UINT16 *m_src_s1;
static UINT16 *m_src_e1;
static UINT16 m_clip_al1;
static UINT16 m_clip_ar1;
static UINT16 m_clip_bl1;
static UINT16 m_clip_br1;
static UINT8 *m_tsrc1;
static UINT8 *m_tsrc_s1;
static UINT32 m_x_count1;
static UINT32 m_x_zoom1;
static UINT16 *m_src2;
static UINT16 *m_src_s2;
static UINT16 *m_src_e2;
static UINT16 m_clip_al2;
static UINT16 m_clip_ar2;
static UINT16 m_clip_bl2;
static UINT16 m_clip_br2;
static UINT8 *m_tsrc2;
static UINT8 *m_tsrc_s2;
static UINT32 m_x_count2;
static UINT32 m_x_zoom2;
static UINT16 *m_src3;
static UINT16 *m_src_s3;
static UINT16 *m_src_e3;
static UINT16 m_clip_al3;
static UINT16 m_clip_ar3;
static UINT16 m_clip_bl3;
static UINT16 m_clip_br3;
static UINT8 *m_tsrc3;
static UINT8 *m_tsrc_s3;
static UINT32 m_x_count3;
static UINT32 m_x_zoom3;
static UINT16 *m_src4;
static UINT16 *m_src_s4;
static UINT16 *m_src_e4;
static UINT16 m_clip_al4;
static UINT16 m_clip_ar4;
static UINT16 m_clip_bl4;
static UINT16 m_clip_br4;
static UINT8 *m_tsrc4;
static UINT8 *m_tsrc_s4;
static UINT32 m_x_count4;
static UINT32 m_x_zoom4;

static UINT8 m_add_sat[256][256];

static INT32 m_f3_alpha_level_2as;
static INT32 m_f3_alpha_level_2ad;
static INT32 m_f3_alpha_level_3as;
static INT32 m_f3_alpha_level_3ad;
static INT32 m_f3_alpha_level_2bs;
static INT32 m_f3_alpha_level_2bd;
static INT32 m_f3_alpha_level_3bs;
static INT32 m_f3_alpha_level_3bd;
static INT32 m_alpha_level_last;

static INT32 m_alpha_s_1_1;
static INT32 m_alpha_s_1_2;
static INT32 m_alpha_s_1_4;
static INT32 m_alpha_s_1_5;
static INT32 m_alpha_s_1_6;
static INT32 m_alpha_s_1_8;
static INT32 m_alpha_s_1_9;
static INT32 m_alpha_s_1_a;
static INT32 m_alpha_s_2a_0;
static INT32 m_alpha_s_2a_4;
static INT32 m_alpha_s_2a_8;
static INT32 m_alpha_s_2b_0;
static INT32 m_alpha_s_2b_4;
static INT32 m_alpha_s_2b_8;
static INT32 m_alpha_s_3a_0;
static INT32 m_alpha_s_3a_1;
static INT32 m_alpha_s_3a_2;
static INT32 m_alpha_s_3b_0;
static INT32 m_alpha_s_3b_1;
static INT32 m_alpha_s_3b_2;
static UINT32 m_dval;
static UINT8 m_pval;
static UINT8 m_tval;
static UINT8 m_pdest_2a;
static UINT8 m_pdest_2b;
static INT32 m_tr_2a;
static INT32 m_tr_2b;
static UINT8 m_pdest_3a;
static UINT8 m_pdest_3b;
static INT32 m_tr_3a;
static INT32 m_tr_3b;

#define BYTE4_XOR_LE(x)	x


struct f3_playfield_line_inf
{
	INT32 alpha_mode[256];
	INT32 pri[256];

	/* use for draw_scanlines */
	UINT16 *src[256],*src_s[256],*src_e[256];
	UINT8 *tsrc[256],*tsrc_s[256];
	INT32 x_count[256];
	UINT32 x_zoom[256];
	UINT32 clip0[256];
	UINT32 clip1[256];
};

struct f3_spritealpha_line_inf
{
	UINT16 alpha_level[256];
	UINT16 spri[256];
	UINT16 sprite_alpha[256];
	UINT32 sprite_clip0[256];
	UINT32 sprite_clip1[256];
	INT16 clip0_l[256];
	INT16 clip0_r[256];
	INT16 clip1_l[256];
	INT16 clip1_r[256];
};



static struct f3_playfield_line_inf m_pf_line_inf[5];
static struct f3_spritealpha_line_inf m_sa_line_inf[1];


static void clear_f3_stuff()
{
	memset(&m_pf_line_inf, 0, sizeof(m_pf_line_inf));
	memset(&m_sa_line_inf, 0, sizeof(m_sa_line_inf));
}


static INT32 (*m_dpix_n[8][16])(UINT32 s_pix);
static INT32 (**m_dpix_lp[5])(UINT32 s_pix);
static INT32 (**m_dpix_sp[9])(UINT32 s_pix);




static void draw_pf_layer(INT32 layer)
{
	INT32 offset = (layer * (0x1000 << extended_layers));

	UINT16 *ram = (UINT16*)(TaitoF3PfRAM + offset);

	INT32 width = extended_layers ? 1024 : 512;
	INT32 wide = width / 16;

	// was this layer written at all? skip!
	if (extended_layers) {
		if (dirty_tile_count[layer*2+0] == 0 && dirty_tile_count[layer*2+1] == 0) {
			return;
		}
		dirty_tile_count[layer*2+0] = dirty_tile_count[layer*2+1] = 0;
	} else {
		if (dirty_tile_count[layer] == 0) {
			return;
		}
		dirty_tile_count[layer] = 0;
	}

	for (INT32 offs = 0; offs < wide * 32; offs++)
	{
		if (dirty_tiles[((offs * 4) + offset) / 4] == 0) continue;
		dirty_tiles[((offs * 4) + offset) / 4] = 0;

		INT32 sx = (offs % wide) * 16;
		INT32 sy = (offs / wide) * 16;

		UINT16 tile = ram[offs * 2 + 0];
		UINT16 code = (ram[offs * 2 + 1] & 0xffff) % TaitoCharModulo;

		UINT8 category = (tile >> 9) & 1;

		INT32 explane = (tile >> 10) & 3;
		INT32 flipx   = (tile >> 14) & 1;
		INT32 flipy   = (tile >> 15) & 1;
		INT32 color   = ((tile & 0x1ff) & (~explane)) << 4;

		INT32 penmask = (explane << 4) | 0x0f;

		if (flipscreen) {
			sx = (width - 16) - sx;
			sy = (512 - 16) - sy;
			flipy ^= 1;
			flipx ^= 1;
		}

		{
			INT32 flip = (flipy * 0xf0) + (flipx * 0x0f);

			UINT8 *gfx = TaitoChars + (code * 0x100);
			UINT16 *dst = bitmap_layer[layer] + (sy * width) + sx;
			UINT8 *flagptr = bitmap_flags[layer] + (sy * width) + sx;

			for (INT32 y = 0; y < 16; y++, sy++, dst += width, flagptr += width)
			{
				for (INT32 x = 0; x < 16; x++)
				{
					INT32 pxl = gfx[((y*16)+x)^flip] & penmask;

					dst[x] = pxl + color;

					if (pxl == 0) {
						flagptr[x] = category;
					} else {
						flagptr[x] = category | 0x10;
					}
				}
			}
		}
	}
}

static void draw_vram_layer()
{
	UINT16 *ram = (UINT16*)TaitoVideoRam;

	for (INT32 offs = 0; offs < 64 * 64; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		INT32 tile = ram[offs] & 0xffff;

		INT32 code = tile & 0x00ff;

		INT32 flipx = tile & 0x0100;
		INT32 flipy = tile & 0x8000;

		if (flipscreen) {
			sx = (512 - 8) - sx;
			sy = (512 - 8) - sy;
			flipx ^= 0x100;
			flipy ^= 0x8000;
		}

		INT32 color = (tile >> 9) & 0x3f;

		{
			color *= 16;
			UINT8 *gfx = TaitoCharsB + (code * 8 * 8);

			INT32 flip = ((flipy ? 0x38 : 0) | (flipx ? 7 : 0));

			UINT16 *dst = bitmap_layer[8] + (sy * 512) + sx;
			UINT8 *flagptr = bitmap_flags[8] + (sy * 512) + sx;

			for (INT32 y = 0; y < 8; y++)
			{
				for (INT32 x = 0; x < 8; x++)
				{
					INT32 pxl = gfx[((y*8)+x)^flip];

					dst[x] = pxl + color;

					if (pxl == 0) {
						flagptr[x] = /*category | */ 0;
					} else {
						flagptr[x] = /*category | */0x10;
					}
				}

				dst += 512;
				flagptr += 512;
			}
		}
	}
}

static void draw_pixel_layer()
{
	// was this written? skip!
	if (dirty_tile_count[9] == 0) {
	//	bprintf (0, _T("Skip pixel layer!\n"));
		return;
	}
	dirty_tile_count[9] = 0;

	UINT16 *ram = (UINT16*)TaitoVideoRam;

	UINT16 y_offs = *((UINT16*)(TaitoF3CtrlRAM + 0x1a)) & 0x1ff;
	if (flipscreen) y_offs += 0x100;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs / 0x20) * 8;
		INT32 sy = (offs & 0x1f) * 8;

		INT32 col_off = ((offs & 0x1f) * 0x40) + ((offs & 0xfe0) >> 5);

		if ((((offs & 0x1f) * 8 + y_offs) & 0x1ff) > 0xff)
			col_off += 0x800;

		INT32 tile = ram[col_off];

		INT32 code = offs;

		INT32 flipx = tile & 0x0100;
		INT32 flipy = tile & 0x8000;

		INT32 color = (tile >> 9) & 0x3f;

		if (flipscreen)
		{
			flipx ^= 0x0100;
			flipy ^= 0x8000;
			sy = (256 - 8) - sy;
			sx = (512 - 8) - sx;
		}

		{
			color *= 16;
			UINT8 *gfx = TaitoCharsPivot + (code * 8 * 8);

			INT32 flip = ((flipy ? 0x38 : 0) | (flipx ? 7 : 0));

			UINT16 *dst = bitmap_layer[9] + (sy * 512) + sx;
			UINT8 *flagptr = bitmap_flags[9] + (sy * 512) + sx;

			for (INT32 y = 0; y < 8; y++)
			{
				for (INT32 x = 0; x < 8; x++)
				{
					INT32 pxl = gfx[((y*8)+x)^flip];

					dst[x] = pxl + color;

					if (pxl == 0) {
						flagptr[x] = /*category | */ 0;
					} else {
						flagptr[x] = /*category | */0x10;
					}
				}

				dst += 512;
				flagptr += 512;
			}
		}
	}
}




#define PSET_T					\
	c = *source & sprite_pen_mask;	\
	if(c)						\
	{							\
		p=*pri;					\
		if(!p || p==0xff)		\
		{						\
			*dest = pal[c];		\
			*pri = pri_dst;		\
		}						\
	}

#define PSET_O					\
	p=*pri;						\
	if(!p || p==0xff)			\
	{							\
		*dest = pal[(*source & sprite_pen_mask)];	\
		*pri = pri_dst;			\
	}

#define NEXT_P					\
	source += dx;				\
	dest++;						\
	pri++;

static void f3_drawgfx(
		INT32 code,
		INT32 color,
		INT32 flipx,INT32 flipy,
		INT32 sx,INT32 sy,
		UINT8 pri_dst)
{
	pri_dst=1<<pri_dst;

	/* KW 991012 -- Added code to force clip to bitmap boundary */
//	myclip = clip;
//	myclip &= dest_bmp.cliprect();

	{
		const UINT32 *pal = TaitoPalette + (0x1000 + ((color & 0xff) << 4));
		const UINT8 *code_base = TaitoSpritesA + (code * 0x100);

		{
			/* compute sprite increment per screen pixel */
			INT32 dx = 1;
			INT32 dy = 1;

			INT32 ex = sx+16;
			INT32 ey = sy+16;

			INT32 x_index_base;
			INT32 y_index;

			if( flipx )
			{
				x_index_base = 15;
				dx = -1;
			}
			else
			{
				x_index_base = 0;
			}

			if( flipy )
			{
				y_index = 15;
				dy = -1;
			}
			else
			{
				y_index = 0;
			}

			if(sx < min_x)
			{ /* clip left */
				INT32 pixels = min_x-sx;
				sx += pixels;
				x_index_base += pixels*dx;
			}
			if( sy < min_y )
			{ /* clip top */
				INT32 pixels = min_y-sy;
				sy += pixels;
				y_index += pixels*dy;
			}
			/* NS 980211 - fixed incorrect clipping */
			if( ex > max_x+1 )
			{ /* clip right */
				INT32 pixels = ex-max_x-1;
				ex -= pixels;
			}
			if( ey > max_y+1 )
			{ /* clip bottom */
				INT32 pixels = ey-max_y-1;
				ey -= pixels;
			}

			if( ex>sx && ey>sy)
			{ /* skip if inner loop doesn't draw anything */
				{
					INT32 y=ey-sy;
					INT32 x=(ex-sx-1)|(tile_opaque_sp[code]<<4);
					const UINT8 *source0 = code_base + y_index * 16 + x_index_base;
					UINT32 *dest0 = output_bitmap + sy * 512 + sx;
					UINT8 *pri0   = TaitoPriorityMap + sy * 1024 + sx;

					INT32 yadv = 512;
					INT32 yadvp = 1024;

					dy=dy*16;

					while(1)
					{
						const UINT8 *source = source0;
						UINT32 *dest = dest0;
						UINT8 *pri = pri0;

						switch(x)
						{
							INT32 c;
							UINT8 p;
							case 31: PSET_O NEXT_P
							case 30: PSET_O NEXT_P
							case 29: PSET_O NEXT_P
							case 28: PSET_O NEXT_P
							case 27: PSET_O NEXT_P
							case 26: PSET_O NEXT_P
							case 25: PSET_O NEXT_P
							case 24: PSET_O NEXT_P
							case 23: PSET_O NEXT_P
							case 22: PSET_O NEXT_P
							case 21: PSET_O NEXT_P
							case 20: PSET_O NEXT_P
							case 19: PSET_O NEXT_P
							case 18: PSET_O NEXT_P
							case 17: PSET_O NEXT_P
							case 16: PSET_O break;

							case 15: PSET_T NEXT_P
							case 14: PSET_T NEXT_P
							case 13: PSET_T NEXT_P
							case 12: PSET_T NEXT_P
							case 11: PSET_T NEXT_P
							case 10: PSET_T NEXT_P
							case  9: PSET_T NEXT_P
							case  8: PSET_T NEXT_P
							case  7: PSET_T NEXT_P
							case  6: PSET_T NEXT_P
							case  5: PSET_T NEXT_P
							case  4: PSET_T NEXT_P
							case  3: PSET_T NEXT_P
							case  2: PSET_T NEXT_P
							case  1: PSET_T NEXT_P
							case  0: PSET_T
						}

						if(!(--y)) break;
						source0 += dy;
						dest0+=yadv;
						pri0+=yadvp;
					}
				}
			}
		}
	}
}
#undef PSET_T
#undef PSET_O
#undef NEXT_P


static void f3_drawgfxzoom(
		INT32 code,
		INT32 color,
		INT32 flipx,INT32 flipy,
		INT32 sx,INT32 sy,
		INT32 scalex, INT32 scaley,
		UINT8 pri_dst)
{
	pri_dst=1<<pri_dst;

	/* KW 991012 -- Added code to force clip to bitmap boundary */
//	myclip = clip;
//	myclip &= dest_bmp.cliprect();


	{
		const UINT32 *pal = TaitoPalette + (0x1000 + ((color & 0xff) << 4));
		const UINT8 *code_base = TaitoSpritesA + code * 0x100;

		{
			/* compute sprite increment per screen pixel */
			INT32 dx = (16<<16)/scalex;
			INT32 dy = (16<<16)/scaley;

			INT32 ex = sx+scalex;
			INT32 ey = sy+scaley;

			INT32 x_index_base;
			INT32 y_index;

			if( flipx )
			{
				x_index_base = (scalex-1)*dx;
				dx = -dx;
			}
			else
			{
				x_index_base = 0;
			}

			if( flipy )
			{
				y_index = (scaley-1)*dy;
				dy = -dy;
			}
			else
			{
				y_index = 0;
			}

			if(sx < min_x)
			{ /* clip left */
				INT32 pixels = min_x-sx;
				sx += pixels;
				x_index_base += pixels*dx;
			}
			if( sy < min_y )
			{ /* clip top */
				INT32 pixels = min_y-sy;
				sy += pixels;
				y_index += pixels*dy;
			}
			/* NS 980211 - fixed incorrect clipping */
			if( ex > max_x+1 )
			{ /* clip right */
				INT32 pixels = ex-max_x-1;
				ex -= pixels;
			}
			if( ey > max_y+1 )
			{ /* clip bottom */
				INT32 pixels = ey-max_y-1;
				ey -= pixels;
			}

			if( ex>sx )
			{ /* skip if inner loop doesn't draw anything */
				{
					INT32 y;
					for( y=sy; y<ey; y++ )
					{
						const UINT8 *source = code_base + (y_index>>16) * 16;
						UINT32 *dest = output_bitmap + y * 512;
						UINT8 *pri = TaitoPriorityMap + y * 1024;

						INT32 x, x_index = x_index_base;
						for( x=sx; x<ex; x++ )
						{
							INT32 c = source[x_index>>16] & sprite_pen_mask;
							if(c)
							{
								UINT8 p=pri[x];
								if (p == 0 || p == 0xff)
								{
									dest[x] = pal[c];
									pri[x] = pri_dst;
								}
							}
							x_index += dx;
						}
						y_index += dy;
					}
				}
			}
		}
	}
}


#define CALC_ZOOM(p)	{										\
	p##_addition = 0x100 - block_zoom_##p + p##_addition_left;	\
	p##_addition_left = p##_addition & 0xf;						\
	p##_addition = p##_addition >> 4;							\
	/*zoom##p = p##_addition << 12;*/							\
}

static void get_sprite_info(UINT16 *spriteram16_ptr)
{
#define DARIUSG_KLUDGE

	INT32 offs,spritecont,flipx,flipy,/*old_x,*/color,x,y;
	INT32 sprite,global_x=0,global_y=0,subglobal_x=0,subglobal_y=0;
	INT32 block_x=0, block_y=0;
	INT32 last_color=0,last_x=0,last_y=0,block_zoom_x=0,block_zoom_y=0;
	INT32 this_x,this_y;
	INT32 y_addition=16, x_addition=16;
	INT32 multi=0;
	INT32 sprite_top;

	INT32 x_addition_left = 8, y_addition_left = 8;

	struct tempsprite *sprite_ptr = m_spritelist;

	INT32 total_sprites=0;
	INT32 jumpcnt = 0;

	color=0;
	flipx=flipy=0;
	//old_x=0;
	y=x=0;

	sprite_top=0x2000;
	for (offs = 0; offs < sprite_top && (total_sprites < 0x400); offs += 8)
	{
		const INT32 current_offs=offs; /* Offs can change during loop, current_offs cannot */

		/* Check if the sprite list jump command bit is set */
		if ((spriteram16_ptr[current_offs+6+0]) & 0x8000) {
			UINT32 jump = (spriteram16_ptr[current_offs+6+0])&0x3ff;

			INT32 new_offs=((offs&0x4000)|((jump<<4)/2));
			if (new_offs==offs || jumpcnt > 250)
				break;
			offs=new_offs - 8;
			jumpcnt++;
		}

		/* Check if special command bit is set */
		if (spriteram16_ptr[current_offs+2+1] & 0x8000) {
			UINT32 cntrl=(spriteram16_ptr[current_offs+4+1])&0xffff;
			flipscreen=cntrl&0x2000;

			/*  cntrl&0x1000 = disabled?  (From F2 driver, doesn't seem used anywhere)
			    cntrl&0x0010 = ???
			    cntrl&0x0020 = ???
			*/

			sprite_extra_planes = (cntrl & 0x0300) >> 8;   // 0 = 4bpp, 1 = 5bpp, 2 = unused?, 3 = 6bpp
			sprite_pen_mask = (sprite_extra_planes << 4) | 0x0f;

			/* Sprite bank select */
			if (cntrl&1) {
				offs=offs|0x4000;
				sprite_top=sprite_top|0x4000;
			}
		}

		/* Set global sprite scroll */
		if (((spriteram16_ptr[current_offs+2+0]) & 0xf000) == 0xa000) {
			global_x = (spriteram16_ptr[current_offs+2+0]) & 0xfff;
			if (global_x >= 0x800) global_x -= 0x1000;
			global_y = spriteram16_ptr[current_offs+2+1] & 0xfff;
			if (global_y >= 0x800) global_y -= 0x1000;
		}

		/* And sub-global sprite scroll */
		if (((spriteram16_ptr[current_offs+2+0]) & 0xf000) == 0x5000) {
			subglobal_x = (spriteram16_ptr[current_offs+2+0]) & 0xfff;
			if (subglobal_x >= 0x800) subglobal_x -= 0x1000;
			subglobal_y = spriteram16_ptr[current_offs+2+1] & 0xfff;
			if (subglobal_y >= 0x800) subglobal_y -= 0x1000;
		}

		if (((spriteram16_ptr[current_offs+2+0]) & 0xf000) == 0xb000) {
			subglobal_x = (spriteram16_ptr[current_offs+2+0]) & 0xfff;
			if (subglobal_x >= 0x800) subglobal_x -= 0x1000;
			subglobal_y = spriteram16_ptr[current_offs+2+1] & 0xfff;
			if (subglobal_y >= 0x800) subglobal_y -= 0x1000;
			global_y=subglobal_y;
			global_x=subglobal_x;
		}

		/* A real sprite to process! */
		sprite = (spriteram16_ptr[current_offs+0+0]) | ((spriteram16_ptr[current_offs+4+1]&1)<<16);
		spritecont = spriteram16_ptr[current_offs+4+0]>>8;

/* These games either don't set the XY control bits properly (68020 bug?), or
    have some different mode from the others */
#ifdef DARIUSG_KLUDGE
		if (f3_game==DARIUSG || f3_game==GEKIRIDO || f3_game==CLEOPATR || f3_game==RECALH)
			multi=spritecont&0xf0;
#endif

		/* Check if this sprite is part of a continued block */
		if (multi) {
			/* Bit 0x4 is 'use previous colour' for this block part */
			if (spritecont&0x4) color=last_color;
			else color=(spriteram16_ptr[current_offs+4+0])&0xff;

#ifdef DARIUSG_KLUDGE
			if (f3_game==DARIUSG || f3_game==GEKIRIDO || f3_game==CLEOPATR || f3_game==RECALH) {
				/* Adjust X Position */
				if ((spritecont & 0x40) == 0) {
					if (spritecont & 0x4) {
						x = block_x;
					} else {
						this_x = spriteram16_ptr[current_offs+2+0];
						if (this_x&0x800) this_x= 0 - (0x800 - (this_x&0x7ff)); else this_x&=0x7ff;

						if ((spriteram16_ptr[current_offs+2+0])&0x8000) {
							this_x+=0;
						} else if ((spriteram16_ptr[current_offs+2+0])&0x4000) {
							/* Ignore subglobal (but apply global) */
							this_x+=global_x;
						} else { /* Apply both scroll offsets */
							this_x+=global_x+subglobal_x;
						}

						x = block_x = this_x;
					}
					x_addition_left = 8;
					CALC_ZOOM(x)
				}
				else if ((spritecont & 0x80) != 0) {
					x = last_x+x_addition;
					CALC_ZOOM(x)
				}

				/* Adjust Y Position */
				if ((spritecont & 0x10) == 0) {
					if (spritecont & 0x4) {
						y = block_y;
					} else {
						this_y = spriteram16_ptr[current_offs+2+1]&0xffff;
						if (this_y&0x800) this_y= 0 - (0x800 - (this_y&0x7ff)); else this_y&=0x7ff;

						if ((spriteram16_ptr[current_offs+2+0])&0x8000) {
							this_y+=0;
						} else if ((spriteram16_ptr[current_offs+2+0])&0x4000) {
							/* Ignore subglobal (but apply global) */
							this_y+=global_y;
						} else { /* Apply both scroll offsets */
							this_y+=global_y+subglobal_y;
						}

						y = block_y = this_y;
					}
					y_addition_left = 8;
					CALC_ZOOM(y)
				}
				else if ((spritecont & 0x20) != 0) {
					y = last_y+y_addition;
					CALC_ZOOM(y)
				}
			} else
#endif
			{
				/* Adjust X Position */
				if ((spritecont & 0x40) == 0) {
					x = block_x;
					x_addition_left = 8;
					CALC_ZOOM(x)
				}
				else if ((spritecont & 0x80) != 0) {
					x = last_x+x_addition;
					CALC_ZOOM(x)
				}
				/* Adjust Y Position */
				if ((spritecont & 0x10) == 0) {
					y = block_y;
					y_addition_left = 8;
					CALC_ZOOM(y)
				}
				else if ((spritecont & 0x20) != 0) {
					y = last_y+y_addition;
					CALC_ZOOM(y)
				}
				/* Both zero = reread block latch? */
			}
		}
		/* Else this sprite is the possible start of a block */
		else {
			color = (spriteram16_ptr[current_offs+4+0])&0xff;
			last_color=color;

			/* Sprite positioning */
			this_y = spriteram16_ptr[current_offs+2+1]&0xffff;
			this_x = spriteram16_ptr[current_offs+2+0]&0xffff;
			if (this_y&0x800) this_y= 0 - (0x800 - (this_y&0x7ff)); else this_y&=0x7ff;
			if (this_x&0x800) this_x= 0 - (0x800 - (this_x&0x7ff)); else this_x&=0x7ff;

			/* Ignore both scroll offsets for this block */
			if ((spriteram16_ptr[current_offs+2+0])&0x8000) {
				this_x+=0;
				this_y+=0;
			} else if ((spriteram16_ptr[current_offs+2+0])&0x4000) {
				/* Ignore subglobal (but apply global) */
				this_x+=global_x;
				this_y+=global_y;
			} else { /* Apply both scroll offsets */
				this_x+=global_x+subglobal_x;
				this_y+=global_y+subglobal_y;
			}

			block_y = y = this_y;
			block_x = x = this_x;

			block_zoom_x=spriteram16_ptr[current_offs+0+1];
			block_zoom_y=(block_zoom_x>>8)&0xff;
			block_zoom_x&=0xff;

			x_addition_left = 8;
			CALC_ZOOM(x)

			y_addition_left = 8;
			CALC_ZOOM(y)
		}

		/* These features are common to sprite and block parts */
		flipx = spritecont&0x1;
		flipy = spritecont&0x2;
		multi = spritecont&0x8;
		last_x=x;
		last_y=y;

		if (!sprite) continue;
		if (!x_addition || !y_addition) continue;

		if (flipscreen)
		{
			INT32 tx,ty;

			tx = 512-x_addition-x;
			ty = y; //256-y_addition-y;

			if (tx+x_addition<=min_x || tx>max_x || ty+y_addition<=min_y || ty>max_y) continue;
			sprite_ptr->x = tx;
			sprite_ptr->y = ty;
			sprite_ptr->flipx = !flipx;
			sprite_ptr->flipy = flipy;
		}
		else
		{
			if (x+x_addition<=min_x || x>max_x || y+y_addition<=min_y || y>max_y) continue;
			sprite_ptr->x = x;
			sprite_ptr->y = y;
			sprite_ptr->flipx = flipx;
			sprite_ptr->flipy = flipy;
		}

		sprite_ptr->code = sprite % TaitoSpriteAModulo;
		sprite_ptr->color = color;
		sprite_ptr->zoomx = x_addition;
		sprite_ptr->zoomy = y_addition;
		sprite_ptr->pri = (color & 0xc0) >> 6;
		sprite_ptr->rampos = current_offs & 0x1fff;
		//bprintf(0, _T("%X, "), current_offs);
		sprite_ptr++;
		total_sprites++;
	}

	if (f3_game == GSEEKER && TaitoDip[0] & 1)
	{ // gseeker st.5 boss spriteram overflow corruption fix.
		if (total_sprites > 1) {
			sprite_ptr--;
			INT32 i = 0x1ff8;
			while ((sprite_ptr->rampos == i || sprite_ptr->rampos >= i-0x400) && sprite_ptr != m_spritelist) {
				i -= 8;
				sprite_ptr--;
			}
			//bprintf(0, _T("last good: %X."), sprite_ptr->rampos);
			if (sprite_ptr != m_spritelist) sprite_ptr++; // always one empty sprite at the end.
		}
	}

	//bprintf(0, _T("\n"));
	if (jumpcnt>150) bprintf(0, _T("Sprite Jumps: %d. \n"), jumpcnt);
	m_sprite_end = sprite_ptr;
}
#undef CALC_ZOOM


static void draw_sprites()
{
	const struct tempsprite *sprite_ptr;
	sprite_ptr = m_sprite_end;
	sprite_pri_usage=0;

	// if sprites use more than 4bpp, the bottom bits of the color code must be masked out.
	// This fixes (at least) stage 1 battle ships and attract mode explosions in Ray Force.

	while (sprite_ptr != m_spritelist)
	{
		INT32 pri;
		sprite_ptr--;

		pri=sprite_ptr->pri;
		sprite_pri_usage|=1<<pri;

		if(sprite_ptr->zoomx==16 && sprite_ptr->zoomy==16)
			f3_drawgfx(
					sprite_ptr->code,
					sprite_ptr->color & (~sprite_extra_planes),
					sprite_ptr->flipx,sprite_ptr->flipy,
					sprite_ptr->x,sprite_ptr->y,
					pri);
		else
			f3_drawgfxzoom(
					sprite_ptr->code,
					sprite_ptr->color & (~sprite_extra_planes),
					sprite_ptr->flipx,sprite_ptr->flipy,
					sprite_ptr->x,sprite_ptr->y,
					sprite_ptr->zoomx,sprite_ptr->zoomy,
					pri);
	}
}




/*============================================================================*/

#define SET_ALPHA_LEVEL(d,s)            \
{                                       \
	INT32 level = s;                      \
	if(level == 0) level = -1;          \
	d = level+1;                        \
}

static inline void f3_alpha_set_level()
{
//  SET_ALPHA_LEVEL(m_alpha_s_1_1, m_f3_alpha_level_2ad)
	SET_ALPHA_LEVEL(m_alpha_s_1_1, 255-m_f3_alpha_level_2as)
//  SET_ALPHA_LEVEL(m_alpha_s_1_2, m_f3_alpha_level_2bd)
	SET_ALPHA_LEVEL(m_alpha_s_1_2, 255-m_f3_alpha_level_2bs)
	SET_ALPHA_LEVEL(m_alpha_s_1_4, m_f3_alpha_level_3ad)
//  SET_ALPHA_LEVEL(m_alpha_s_1_5, m_f3_alpha_level_3ad*m_f3_alpha_level_2ad/255)
	SET_ALPHA_LEVEL(m_alpha_s_1_5, m_f3_alpha_level_3ad*(255-m_f3_alpha_level_2as)/255)
//  SET_ALPHA_LEVEL(m_alpha_s_1_6, m_f3_alpha_level_3ad*m_f3_alpha_level_2bd/255)
	SET_ALPHA_LEVEL(m_alpha_s_1_6, m_f3_alpha_level_3ad*(255-m_f3_alpha_level_2bs)/255)
	SET_ALPHA_LEVEL(m_alpha_s_1_8, m_f3_alpha_level_3bd)
//  SET_ALPHA_LEVEL(m_alpha_s_1_9, m_f3_alpha_level_3bd*m_f3_alpha_level_2ad/255)
	SET_ALPHA_LEVEL(m_alpha_s_1_9, m_f3_alpha_level_3bd*(255-m_f3_alpha_level_2as)/255)
//  SET_ALPHA_LEVEL(m_alpha_s_1_a, m_f3_alpha_level_3bd*m_f3_alpha_level_2bd/255)
	SET_ALPHA_LEVEL(m_alpha_s_1_a, m_f3_alpha_level_3bd*(255-m_f3_alpha_level_2bs)/255)

	SET_ALPHA_LEVEL(m_alpha_s_2a_0, m_f3_alpha_level_2as)
	SET_ALPHA_LEVEL(m_alpha_s_2a_4, m_f3_alpha_level_2as*m_f3_alpha_level_3ad/255)
	SET_ALPHA_LEVEL(m_alpha_s_2a_8, m_f3_alpha_level_2as*m_f3_alpha_level_3bd/255)

	SET_ALPHA_LEVEL(m_alpha_s_2b_0, m_f3_alpha_level_2bs)
	SET_ALPHA_LEVEL(m_alpha_s_2b_4, m_f3_alpha_level_2bs*m_f3_alpha_level_3ad/255)
	SET_ALPHA_LEVEL(m_alpha_s_2b_8, m_f3_alpha_level_2bs*m_f3_alpha_level_3bd/255)

	SET_ALPHA_LEVEL(m_alpha_s_3a_0, m_f3_alpha_level_3as)
	SET_ALPHA_LEVEL(m_alpha_s_3a_1, m_f3_alpha_level_3as*m_f3_alpha_level_2ad/255)
	SET_ALPHA_LEVEL(m_alpha_s_3a_2, m_f3_alpha_level_3as*m_f3_alpha_level_2bd/255)

	SET_ALPHA_LEVEL(m_alpha_s_3b_0, m_f3_alpha_level_3bs)
	SET_ALPHA_LEVEL(m_alpha_s_3b_1, m_f3_alpha_level_3bs*m_f3_alpha_level_2ad/255)
	SET_ALPHA_LEVEL(m_alpha_s_3b_2, m_f3_alpha_level_3bs*m_f3_alpha_level_2bd/255)
}
#undef SET_ALPHA_LEVEL

#define COLOR1 BYTE4_XOR_LE(0)
#define COLOR2 BYTE4_XOR_LE(1)
#define COLOR3 BYTE4_XOR_LE(2)

static inline void f3_alpha_blend32_s(INT32 alphas, UINT32 s)
{
	UINT8 *sc = (UINT8 *)&s;
	UINT8 *dc = (UINT8 *)&m_dval;
	dc[COLOR1] = (alphas * sc[COLOR1]) >> 8;
	dc[COLOR2] = (alphas * sc[COLOR2]) >> 8;
	dc[COLOR3] = (alphas * sc[COLOR3]) >> 8;
}

static inline void f3_alpha_blend32_d(INT32 alphas, UINT32 s)
{
	UINT8 *sc = (UINT8 *)&s;
	UINT8 *dc = (UINT8 *)&m_dval;
	dc[COLOR1] = m_add_sat[dc[COLOR1]][(alphas * sc[COLOR1]) >> 8];
	dc[COLOR2] = m_add_sat[dc[COLOR2]][(alphas * sc[COLOR2]) >> 8];
	dc[COLOR3] = m_add_sat[dc[COLOR3]][(alphas * sc[COLOR3]) >> 8];
}

/*============================================================================*/

static inline void f3_alpha_blend_1_1(UINT32 s){f3_alpha_blend32_d(m_alpha_s_1_1,s);}
static inline void f3_alpha_blend_1_2(UINT32 s){f3_alpha_blend32_d(m_alpha_s_1_2,s);}
static inline void f3_alpha_blend_1_4(UINT32 s){f3_alpha_blend32_d(m_alpha_s_1_4,s);}
static inline void f3_alpha_blend_1_5(UINT32 s){f3_alpha_blend32_d(m_alpha_s_1_5,s);}
static inline void f3_alpha_blend_1_6(UINT32 s){f3_alpha_blend32_d(m_alpha_s_1_6,s);}
static inline void f3_alpha_blend_1_8(UINT32 s){f3_alpha_blend32_d(m_alpha_s_1_8,s);}
static inline void f3_alpha_blend_1_9(UINT32 s){f3_alpha_blend32_d(m_alpha_s_1_9,s);}
static inline void f3_alpha_blend_1_a(UINT32 s){f3_alpha_blend32_d(m_alpha_s_1_a,s);}

static inline void f3_alpha_blend_2a_0(UINT32 s){f3_alpha_blend32_s(m_alpha_s_2a_0,s);}
static inline void f3_alpha_blend_2a_4(UINT32 s){f3_alpha_blend32_d(m_alpha_s_2a_4,s);}
static inline void f3_alpha_blend_2a_8(UINT32 s){f3_alpha_blend32_d(m_alpha_s_2a_8,s);}

static inline void f3_alpha_blend_2b_0(UINT32 s){f3_alpha_blend32_s(m_alpha_s_2b_0,s);}
static inline void f3_alpha_blend_2b_4(UINT32 s){f3_alpha_blend32_d(m_alpha_s_2b_4,s);}
static inline void f3_alpha_blend_2b_8(UINT32 s){f3_alpha_blend32_d(m_alpha_s_2b_8,s);}

static inline void f3_alpha_blend_3a_0(UINT32 s){f3_alpha_blend32_s(m_alpha_s_3a_0,s);}
static inline void f3_alpha_blend_3a_1(UINT32 s){f3_alpha_blend32_d(m_alpha_s_3a_1,s);}
static inline void f3_alpha_blend_3a_2(UINT32 s){f3_alpha_blend32_d(m_alpha_s_3a_2,s);}

static inline void f3_alpha_blend_3b_0(UINT32 s){f3_alpha_blend32_s(m_alpha_s_3b_0,s);}
static inline void f3_alpha_blend_3b_1(UINT32 s){f3_alpha_blend32_d(m_alpha_s_3b_1,s);}
static inline void f3_alpha_blend_3b_2(UINT32 s){f3_alpha_blend32_d(m_alpha_s_3b_2,s);}

/*============================================================================*/

static INT32 dpix_1_noalpha(UINT32 s_pix) {m_dval = s_pix; return 1;}
static INT32 dpix_ret1(UINT32 /*s_pix*/) {return 1;}
static INT32 dpix_ret0(UINT32 /*s_pix*/) {return 0;}
static INT32 dpix_1_1(UINT32 s_pix) {if(s_pix) f3_alpha_blend_1_1(s_pix); return 1;}
static INT32 dpix_1_2(UINT32 s_pix) {if(s_pix) f3_alpha_blend_1_2(s_pix); return 1;}
static INT32 dpix_1_4(UINT32 s_pix) {if(s_pix) f3_alpha_blend_1_4(s_pix); return 1;}
static INT32 dpix_1_5(UINT32 s_pix) {if(s_pix) f3_alpha_blend_1_5(s_pix); return 1;}
static INT32 dpix_1_6(UINT32 s_pix) {if(s_pix) f3_alpha_blend_1_6(s_pix); return 1;}
static INT32 dpix_1_8(UINT32 s_pix) {if(s_pix) f3_alpha_blend_1_8(s_pix); return 1;}
static INT32 dpix_1_9(UINT32 s_pix) {if(s_pix) f3_alpha_blend_1_9(s_pix); return 1;}
static INT32 dpix_1_a(UINT32 s_pix) {if(s_pix) f3_alpha_blend_1_a(s_pix); return 1;}

static INT32 dpix_2a_0(UINT32 s_pix)
{
	if(s_pix) f3_alpha_blend_2a_0(s_pix);
	else      m_dval = 0;
	if(m_pdest_2a) {m_pval |= m_pdest_2a;return 0;}
	return 1;
}
static INT32 dpix_2a_4(UINT32 s_pix)
{
	if(s_pix) f3_alpha_blend_2a_4(s_pix);
	if(m_pdest_2a) {m_pval |= m_pdest_2a;return 0;}
	return 1;
}
static INT32 dpix_2a_8(UINT32 s_pix)
{
	if(s_pix) f3_alpha_blend_2a_8(s_pix);
	if(m_pdest_2a) {m_pval |= m_pdest_2a;return 0;}
	return 1;
}

static INT32 dpix_3a_0(UINT32 s_pix)
{
	if(s_pix) f3_alpha_blend_3a_0(s_pix);
	else      m_dval = 0;
	if(m_pdest_3a) {m_pval |= m_pdest_3a;return 0;}
	return 1;
}
static INT32 dpix_3a_1(UINT32 s_pix)
{
	if(s_pix) f3_alpha_blend_3a_1(s_pix);
	if(m_pdest_3a) {m_pval |= m_pdest_3a;return 0;}
	return 1;
}
static INT32 dpix_3a_2(UINT32 s_pix)
{
	if(s_pix) f3_alpha_blend_3a_2(s_pix);
	if(m_pdest_3a) {m_pval |= m_pdest_3a;return 0;}
	return 1;
}

static INT32 dpix_2b_0(UINT32 s_pix)
{
	if(s_pix) f3_alpha_blend_2b_0(s_pix);
	else      m_dval = 0;
	if(m_pdest_2b) {m_pval |= m_pdest_2b;return 0;}
	return 1;
}
static INT32 dpix_2b_4(UINT32 s_pix)
{
	if(s_pix) f3_alpha_blend_2b_4(s_pix);
	if(m_pdest_2b) {m_pval |= m_pdest_2b;return 0;}
	return 1;
}
static INT32 dpix_2b_8(UINT32 s_pix)
{
	if(s_pix) f3_alpha_blend_2b_8(s_pix);
	if(m_pdest_2b) {m_pval |= m_pdest_2b;return 0;}
	return 1;
}

static INT32 dpix_3b_0(UINT32 s_pix)
{
	if(s_pix) f3_alpha_blend_3b_0(s_pix);
	else      m_dval = 0;
	if(m_pdest_3b) {m_pval |= m_pdest_3b;return 0;}
	return 1;
}
static INT32 dpix_3b_1(UINT32 s_pix)
{
	if(s_pix) f3_alpha_blend_3b_1(s_pix);
	if(m_pdest_3b) {m_pval |= m_pdest_3b;return 0;}
	return 1;
}
static INT32 dpix_3b_2(UINT32 s_pix)
{
	if(s_pix) f3_alpha_blend_3b_2(s_pix);
	if(m_pdest_3b) {m_pval |= m_pdest_3b;return 0;}
	return 1;
}

static INT32 dpix_2_0(UINT32 s_pix)
{
	UINT8 tr2=m_tval&1;
	if(s_pix)
	{
		if(tr2==m_tr_2b)     {f3_alpha_blend_2b_0(s_pix);if(m_pdest_2b) m_pval |= m_pdest_2b;else return 1;}
		else if(tr2==m_tr_2a)    {f3_alpha_blend_2a_0(s_pix);if(m_pdest_2a) m_pval |= m_pdest_2a;else return 1;}
	}
	else
	{
		if(tr2==m_tr_2b)     {m_dval = 0;if(m_pdest_2b) m_pval |= m_pdest_2b;else return 1;}
		else if(tr2==m_tr_2a)    {m_dval = 0;if(m_pdest_2a) m_pval |= m_pdest_2a;else return 1;}
	}
	return 0;
}
static INT32 dpix_2_4(UINT32 s_pix)
{
	UINT8 tr2=m_tval&1;
	if(s_pix)
	{
		if(tr2==m_tr_2b)     {f3_alpha_blend_2b_4(s_pix);if(m_pdest_2b) m_pval |= m_pdest_2b;else return 1;}
		else if(tr2==m_tr_2a)    {f3_alpha_blend_2a_4(s_pix);if(m_pdest_2a) m_pval |= m_pdest_2a;else return 1;}
	}
	else
	{
		if(tr2==m_tr_2b)     {if(m_pdest_2b) m_pval |= m_pdest_2b;else return 1;}
		else if(tr2==m_tr_2a)    {if(m_pdest_2a) m_pval |= m_pdest_2a;else return 1;}
	}
	return 0;
}
static INT32 dpix_2_8(UINT32 s_pix)
{
	UINT8 tr2=m_tval&1;
	if(s_pix)
	{
		if(tr2==m_tr_2b)     {f3_alpha_blend_2b_8(s_pix);if(m_pdest_2b) m_pval |= m_pdest_2b;else return 1;}
		else if(tr2==m_tr_2a)    {f3_alpha_blend_2a_8(s_pix);if(m_pdest_2a) m_pval |= m_pdest_2a;else return 1;}
	}
	else
	{
		if(tr2==m_tr_2b)     {if(m_pdest_2b) m_pval |= m_pdest_2b;else return 1;}
		else if(tr2==m_tr_2a)    {if(m_pdest_2a) m_pval |= m_pdest_2a;else return 1;}
	}
	return 0;
}

static INT32 dpix_3_0(UINT32 s_pix)
{
	UINT8 tr2=m_tval&1;
	if(s_pix)
	{
		if(tr2==m_tr_3b)     {f3_alpha_blend_3b_0(s_pix);if(m_pdest_3b) m_pval |= m_pdest_3b;else return 1;}
		else if(tr2==m_tr_3a)    {f3_alpha_blend_3a_0(s_pix);if(m_pdest_3a) m_pval |= m_pdest_3a;else return 1;}
	}
	else
	{
		if(tr2==m_tr_3b)     {m_dval = 0;if(m_pdest_3b) m_pval |= m_pdest_3b;else return 1;}
		else if(tr2==m_tr_3a)    {m_dval = 0;if(m_pdest_3a) m_pval |= m_pdest_3a;else return 1;}
	}
	return 0;
}
static INT32 dpix_3_1(UINT32 s_pix)
{
	UINT8 tr2=m_tval&1;
	if(s_pix)
	{
		if(tr2==m_tr_3b)     {f3_alpha_blend_3b_1(s_pix);if(m_pdest_3b) m_pval |= m_pdest_3b;else return 1;}
		else if(tr2==m_tr_3a)    {f3_alpha_blend_3a_1(s_pix);if(m_pdest_3a) m_pval |= m_pdest_3a;else return 1;}
	}
	else
	{
		if(tr2==m_tr_3b)     {if(m_pdest_3b) m_pval |= m_pdest_3b;else return 1;}
		else if(tr2==m_tr_3a)    {if(m_pdest_3a) m_pval |= m_pdest_3a;else return 1;}
	}
	return 0;
}
static INT32 dpix_3_2(UINT32 s_pix)
{
	UINT8 tr2=m_tval&1;
	if(s_pix)
	{
		if(tr2==m_tr_3b)     {f3_alpha_blend_3b_2(s_pix);if(m_pdest_3b) m_pval |= m_pdest_3b;else return 1;}
		else if(tr2==m_tr_3a)    {f3_alpha_blend_3a_2(s_pix);if(m_pdest_3a) m_pval |= m_pdest_3a;else return 1;}
	}
	else
	{
		if(tr2==m_tr_3b)     {if(m_pdest_3b) m_pval |= m_pdest_3b;else return 1;}
		else if(tr2==m_tr_3a)    {if(m_pdest_3a) m_pval |= m_pdest_3a;else return 1;}
	}
	return 0;
}

static inline void dpix_1_sprite(UINT32 s_pix)
{
	if(s_pix)
	{
		UINT8 p1 = m_pval&0xf0;
		if     (p1==0x10)   f3_alpha_blend_1_1(s_pix);
		else if(p1==0x20)   f3_alpha_blend_1_2(s_pix);
		else if(p1==0x40)   f3_alpha_blend_1_4(s_pix);
		else if(p1==0x50)   f3_alpha_blend_1_5(s_pix);
		else if(p1==0x60)   f3_alpha_blend_1_6(s_pix);
		else if(p1==0x80)   f3_alpha_blend_1_8(s_pix);
		else if(p1==0x90)   f3_alpha_blend_1_9(s_pix);
		else if(p1==0xa0)   f3_alpha_blend_1_a(s_pix);
	}
}

static inline void dpix_bg(UINT32 bgcolor)
{
	UINT8 p1 = m_pval&0xf0;
	if(!p1)         m_dval = bgcolor;
	else if(p1==0x10)   f3_alpha_blend_1_1(bgcolor);
	else if(p1==0x20)   f3_alpha_blend_1_2(bgcolor);
	else if(p1==0x40)   f3_alpha_blend_1_4(bgcolor);
	else if(p1==0x50)   f3_alpha_blend_1_5(bgcolor);
	else if(p1==0x60)   f3_alpha_blend_1_6(bgcolor);
	else if(p1==0x80)   f3_alpha_blend_1_8(bgcolor);
	else if(p1==0x90)   f3_alpha_blend_1_9(bgcolor);
	else if(p1==0xa0)   f3_alpha_blend_1_a(bgcolor);
}

/******************************************************************************/

static INT32 alpha_blend_inited = 0;

static void init_alpha_blend_func()
{
	alpha_blend_inited = 1;

	m_dpix_n[0][0x0]=&dpix_1_noalpha;
	m_dpix_n[0][0x1]=&dpix_1_noalpha;
	m_dpix_n[0][0x2]=&dpix_1_noalpha;
	m_dpix_n[0][0x3]=&dpix_1_noalpha;
	m_dpix_n[0][0x4]=&dpix_1_noalpha;
	m_dpix_n[0][0x5]=&dpix_1_noalpha;
	m_dpix_n[0][0x6]=&dpix_1_noalpha;
	m_dpix_n[0][0x7]=&dpix_1_noalpha;
	m_dpix_n[0][0x8]=&dpix_1_noalpha;
	m_dpix_n[0][0x9]=&dpix_1_noalpha;
	m_dpix_n[0][0xa]=&dpix_1_noalpha;
	m_dpix_n[0][0xb]=&dpix_1_noalpha;
	m_dpix_n[0][0xc]=&dpix_1_noalpha;
	m_dpix_n[0][0xd]=&dpix_1_noalpha;
	m_dpix_n[0][0xe]=&dpix_1_noalpha;
	m_dpix_n[0][0xf]=&dpix_1_noalpha;

	m_dpix_n[1][0x0]=&dpix_1_noalpha;
	m_dpix_n[1][0x1]=&dpix_1_1;
	m_dpix_n[1][0x2]=&dpix_1_2;
	m_dpix_n[1][0x3]=&dpix_ret1;
	m_dpix_n[1][0x4]=&dpix_1_4;
	m_dpix_n[1][0x5]=&dpix_1_5;
	m_dpix_n[1][0x6]=&dpix_1_6;
	m_dpix_n[1][0x7]=&dpix_ret1;
	m_dpix_n[1][0x8]=&dpix_1_8;
	m_dpix_n[1][0x9]=&dpix_1_9;
	m_dpix_n[1][0xa]=&dpix_1_a;
	m_dpix_n[1][0xb]=&dpix_ret1;
	m_dpix_n[1][0xc]=&dpix_ret1;
	m_dpix_n[1][0xd]=&dpix_ret1;
	m_dpix_n[1][0xe]=&dpix_ret1;
	m_dpix_n[1][0xf]=&dpix_ret1;

	m_dpix_n[2][0x0]=&dpix_2a_0;
	m_dpix_n[2][0x1]=&dpix_ret0;
	m_dpix_n[2][0x2]=&dpix_ret0;
	m_dpix_n[2][0x3]=&dpix_ret0;
	m_dpix_n[2][0x4]=&dpix_2a_4;
	m_dpix_n[2][0x5]=&dpix_ret0;
	m_dpix_n[2][0x6]=&dpix_ret0;
	m_dpix_n[2][0x7]=&dpix_ret0;
	m_dpix_n[2][0x8]=&dpix_2a_8;
	m_dpix_n[2][0x9]=&dpix_ret0;
	m_dpix_n[2][0xa]=&dpix_ret0;
	m_dpix_n[2][0xb]=&dpix_ret0;
	m_dpix_n[2][0xc]=&dpix_ret0;
	m_dpix_n[2][0xd]=&dpix_ret0;
	m_dpix_n[2][0xe]=&dpix_ret0;
	m_dpix_n[2][0xf]=&dpix_ret0;

	m_dpix_n[3][0x0]=&dpix_3a_0;
	m_dpix_n[3][0x1]=&dpix_3a_1;
	m_dpix_n[3][0x2]=&dpix_3a_2;
	m_dpix_n[3][0x3]=&dpix_ret0;
	m_dpix_n[3][0x4]=&dpix_ret0;
	m_dpix_n[3][0x5]=&dpix_ret0;
	m_dpix_n[3][0x6]=&dpix_ret0;
	m_dpix_n[3][0x7]=&dpix_ret0;
	m_dpix_n[3][0x8]=&dpix_ret0;
	m_dpix_n[3][0x9]=&dpix_ret0;
	m_dpix_n[3][0xa]=&dpix_ret0;
	m_dpix_n[3][0xb]=&dpix_ret0;
	m_dpix_n[3][0xc]=&dpix_ret0;
	m_dpix_n[3][0xd]=&dpix_ret0;
	m_dpix_n[3][0xe]=&dpix_ret0;
	m_dpix_n[3][0xf]=&dpix_ret0;

	m_dpix_n[4][0x0]=&dpix_2b_0;
	m_dpix_n[4][0x1]=&dpix_ret0;
	m_dpix_n[4][0x2]=&dpix_ret0;
	m_dpix_n[4][0x3]=&dpix_ret0;
	m_dpix_n[4][0x4]=&dpix_2b_4;
	m_dpix_n[4][0x5]=&dpix_ret0;
	m_dpix_n[4][0x6]=&dpix_ret0;
	m_dpix_n[4][0x7]=&dpix_ret0;
	m_dpix_n[4][0x8]=&dpix_2b_8;
	m_dpix_n[4][0x9]=&dpix_ret0;
	m_dpix_n[4][0xa]=&dpix_ret0;
	m_dpix_n[4][0xb]=&dpix_ret0;
	m_dpix_n[4][0xc]=&dpix_ret0;
	m_dpix_n[4][0xd]=&dpix_ret0;
	m_dpix_n[4][0xe]=&dpix_ret0;
	m_dpix_n[4][0xf]=&dpix_ret0;

	m_dpix_n[5][0x0]=&dpix_3b_0;
	m_dpix_n[5][0x1]=&dpix_3b_1;
	m_dpix_n[5][0x2]=&dpix_3b_2;
	m_dpix_n[5][0x3]=&dpix_ret0;
	m_dpix_n[5][0x4]=&dpix_ret0;
	m_dpix_n[5][0x5]=&dpix_ret0;
	m_dpix_n[5][0x6]=&dpix_ret0;
	m_dpix_n[5][0x7]=&dpix_ret0;
	m_dpix_n[5][0x8]=&dpix_ret0;
	m_dpix_n[5][0x9]=&dpix_ret0;
	m_dpix_n[5][0xa]=&dpix_ret0;
	m_dpix_n[5][0xb]=&dpix_ret0;
	m_dpix_n[5][0xc]=&dpix_ret0;
	m_dpix_n[5][0xd]=&dpix_ret0;
	m_dpix_n[5][0xe]=&dpix_ret0;
	m_dpix_n[5][0xf]=&dpix_ret0;

	m_dpix_n[6][0x0]=&dpix_2_0;
	m_dpix_n[6][0x1]=&dpix_ret0;
	m_dpix_n[6][0x2]=&dpix_ret0;
	m_dpix_n[6][0x3]=&dpix_ret0;
	m_dpix_n[6][0x4]=&dpix_2_4;
	m_dpix_n[6][0x5]=&dpix_ret0;
	m_dpix_n[6][0x6]=&dpix_ret0;
	m_dpix_n[6][0x7]=&dpix_ret0;
	m_dpix_n[6][0x8]=&dpix_2_8;
	m_dpix_n[6][0x9]=&dpix_ret0;
	m_dpix_n[6][0xa]=&dpix_ret0;
	m_dpix_n[6][0xb]=&dpix_ret0;
	m_dpix_n[6][0xc]=&dpix_ret0;
	m_dpix_n[6][0xd]=&dpix_ret0;
	m_dpix_n[6][0xe]=&dpix_ret0;
	m_dpix_n[6][0xf]=&dpix_ret0;

	m_dpix_n[7][0x0]=&dpix_3_0;
	m_dpix_n[7][0x1]=&dpix_3_1;
	m_dpix_n[7][0x2]=&dpix_3_2;
	m_dpix_n[7][0x3]=&dpix_ret0;
	m_dpix_n[7][0x4]=&dpix_ret0;
	m_dpix_n[7][0x5]=&dpix_ret0;
	m_dpix_n[7][0x6]=&dpix_ret0;
	m_dpix_n[7][0x7]=&dpix_ret0;
	m_dpix_n[7][0x8]=&dpix_ret0;
	m_dpix_n[7][0x9]=&dpix_ret0;
	m_dpix_n[7][0xa]=&dpix_ret0;
	m_dpix_n[7][0xb]=&dpix_ret0;
	m_dpix_n[7][0xc]=&dpix_ret0;
	m_dpix_n[7][0xd]=&dpix_ret0;
	m_dpix_n[7][0xe]=&dpix_ret0;
	m_dpix_n[7][0xf]=&dpix_ret0;

	for(INT32 i = 0; i < 256; i++)
		for(INT32 j = 0; j < 256; j++)
			m_add_sat[i][j] = (i + j < 256) ? i + j : 255;
}

/******************************************************************************/

#define GET_PIXMAP_POINTER(pf_num) \
{ \
	const struct f3_playfield_line_inf *line_tmp=line_t[pf_num]; \
	m_src##pf_num=line_tmp->src[y]; \
	m_src_s##pf_num=line_tmp->src_s[y]; \
	m_src_e##pf_num=line_tmp->src_e[y]; \
	m_tsrc##pf_num=line_tmp->tsrc[y]; \
	m_tsrc_s##pf_num=line_tmp->tsrc_s[y]; \
	m_x_count##pf_num=line_tmp->x_count[y]; \
	m_x_zoom##pf_num=line_tmp->x_zoom[y]; \
	m_clip_al##pf_num=line_tmp->clip0[y]&0xffff; \
	m_clip_ar##pf_num=line_tmp->clip0[y]>>16; \
	m_clip_bl##pf_num=line_tmp->clip1[y]&0xffff; \
	m_clip_br##pf_num=line_tmp->clip1[y]>>16; \
}

#define CULC_PIXMAP_POINTER(pf_num) \
{ \
	m_x_count##pf_num += m_x_zoom##pf_num; \
	if(m_x_count##pf_num>>16) \
	{ \
		m_x_count##pf_num &= 0xffff; \
		m_src##pf_num++; \
		m_tsrc##pf_num++; \
		if(m_src##pf_num==m_src_e##pf_num) {m_src##pf_num=m_src_s##pf_num; m_tsrc##pf_num=m_tsrc_s##pf_num;} \
	} \
}

#define UPDATE_PIXMAP_SP(pf_num)	\
if(cx>=clip_als && cx<clip_ars && !(cx>=clip_bls && cx<clip_brs)) \
	{ \
		sprite_pri=sprite[pf_num]&m_pval; \
		if(sprite_pri) \
		{ \
			if(sprite[pf_num]&0x100) break; \
			if(!m_dpix_sp[sprite_pri]) \
			{ \
				if(!(m_pval&0xf0)) break; \
				else {dpix_1_sprite(*dsti);*dsti=m_dval;break;} \
			} \
			if((*m_dpix_sp[sprite_pri][m_pval>>4])(*dsti)) {*dsti=m_dval;break;} \
		} \
	}

#define UPDATE_PIXMAP_LP(pf_num) \
	if (cx>=m_clip_al##pf_num && cx<m_clip_ar##pf_num && !(cx>=m_clip_bl##pf_num && cx<m_clip_br##pf_num)) 	\
	{ \
		m_tval=*m_tsrc##pf_num; \
		if(m_tval&0xf0) \
			if((*m_dpix_lp[pf_num][m_pval>>4])(clut[*m_src##pf_num])) {*dsti=m_dval;break;} \
	}


static void draw_scanlines(INT32 xsize,INT16 *draw_line_num,
							const struct f3_playfield_line_inf **line_t,
							const INT32 *sprite,
							UINT32 orient,
							INT32 skip_layer_num)
{
	UINT32 *clut = TaitoPalette;
	UINT32 bgcolor=clut[0];
	INT32 length;

	const INT32 x=46;

	UINT16 clip_als=0, clip_ars=0, clip_bls=0, clip_brs=0;

	UINT8 *dstp0,*dstp;

	INT32 yadv = 512;
	INT32 yadvp = 1024;
	INT32 i=0,y=draw_line_num[0];
	INT32 ty = y;

#if 1
	if (orient & ORIENTATION_FLIP_Y)
	{
		ty = 512 - 1 - ty;
		yadv = -yadv;
		yadvp = -yadvp;
	}
#endif

	

	dstp0 = TaitoPriorityMap + (ty * 1024) + x;

	m_pdest_2a = m_f3_alpha_level_2ad ? 0x10 : 0;
	m_pdest_2b = m_f3_alpha_level_2bd ? 0x20 : 0;
	m_tr_2a =(m_f3_alpha_level_2as==0 && m_f3_alpha_level_2ad==255) ? -1 : 0;
	m_tr_2b =(m_f3_alpha_level_2bs==0 && m_f3_alpha_level_2bd==255) ? -1 : 1;
	m_pdest_3a = m_f3_alpha_level_3ad ? 0x40 : 0;
	m_pdest_3b = m_f3_alpha_level_3bd ? 0x80 : 0;
	m_tr_3a =(m_f3_alpha_level_3as==0 && m_f3_alpha_level_3ad==255) ? -1 : 0;
	m_tr_3b =(m_f3_alpha_level_3bs==0 && m_f3_alpha_level_3bd==255) ? -1 : 1;

	{
		UINT32 *dsti0,*dsti;
		dsti0 = output_bitmap + (ty * 512) + x;
		while(1)
		{
			INT32 cx=0;

			clip_als=m_sa_line_inf[0].sprite_clip0[y]&0xffff;
			clip_ars=m_sa_line_inf[0].sprite_clip0[y]>>16;
			clip_bls=m_sa_line_inf[0].sprite_clip1[y]&0xffff;
			clip_brs=m_sa_line_inf[0].sprite_clip1[y]>>16;

			length=xsize;
			dsti = dsti0;
			dstp = dstp0;

			switch(skip_layer_num)
			{
				case 0: GET_PIXMAP_POINTER(0)
				case 1: GET_PIXMAP_POINTER(1)
				case 2: GET_PIXMAP_POINTER(2)
				case 3: GET_PIXMAP_POINTER(3)
				case 4: GET_PIXMAP_POINTER(4)
			}

			while (1)
			{
				m_pval=*dstp;
				if (m_pval!=0xff)
				{
					UINT8 sprite_pri;
					switch(skip_layer_num)
					{
						case 0: UPDATE_PIXMAP_SP(0) UPDATE_PIXMAP_LP(0)
						case 1: UPDATE_PIXMAP_SP(1) UPDATE_PIXMAP_LP(1)
						case 2: UPDATE_PIXMAP_SP(2) UPDATE_PIXMAP_LP(2)
						case 3: UPDATE_PIXMAP_SP(3) UPDATE_PIXMAP_LP(3)
						case 4: UPDATE_PIXMAP_SP(4) UPDATE_PIXMAP_LP(4)
						case 5: UPDATE_PIXMAP_SP(5)
								if(!bgcolor) {if(!(m_pval&0xf0)) {*dsti=0;break;}}
								else dpix_bg(bgcolor);
								*dsti=m_dval;
					}
				}

				if(!(--length)) break;
				dsti++;
				dstp++;
				cx++;

				switch(skip_layer_num)
				{
					case 0: CULC_PIXMAP_POINTER(0)
					case 1: CULC_PIXMAP_POINTER(1)
					case 2: CULC_PIXMAP_POINTER(2)
					case 3: CULC_PIXMAP_POINTER(3)
					case 4: CULC_PIXMAP_POINTER(4)
				}
			}

			i++;
			if(draw_line_num[i]<0) break;
			if(draw_line_num[i]==y+1)
			{
				dsti0 += yadv;
				dstp0 += yadvp;
				y++;
				continue;
			}
			else
			{
				dsti0 += (draw_line_num[i]-y)*yadv;
				dstp0 += (draw_line_num[i]-y)*yadvp;
				y=draw_line_num[i];
			}
		}
	}
}
#undef GET_PIXMAP_POINTER
#undef CULC_PIXMAP_POINTER

static void visible_tile_check(
						struct f3_playfield_line_inf *line_t,
						INT32 line,
						UINT32 x_index_fx,UINT32 y_index,
						UINT16 *f3_pf_data_n)
{
	UINT16 *pf_base;
	INT32 i,trans_all,tile_index,tile_num;
	INT32 alpha_type,alpha_mode;
	INT32 opaque_all;
	INT32 total_elements;

	alpha_mode=line_t->alpha_mode[line];
	if(!alpha_mode) return;

	total_elements = TaitoCharModulo; // iq_132

	tile_index=x_index_fx>>16;
	tile_num=(((line_t->x_zoom[line]*320+(x_index_fx & 0xffff)+0xffff)>>16)+(tile_index%16)+15)/16;
	tile_index/=16;

	if (flipscreen)
	{
		pf_base=f3_pf_data_n+((31-(y_index/16))<<m_twidth_mask_bit);
		tile_index=(m_twidth_mask-tile_index)-tile_num+1;
	}
	else pf_base=f3_pf_data_n+((y_index/16)<<m_twidth_mask_bit);

	trans_all=1;
	opaque_all=1;
	alpha_type=0;
	for(i=0;i<tile_num;i++)
	{
		UINT32 tile=(pf_base[(tile_index*2+0)&m_twidth_mask]<<16)|(pf_base[(tile_index*2+1)&m_twidth_mask]);
		UINT8  extra_planes = (tile>>(16+10)) & 3;
		if(tile&0xffff)
		{
			trans_all=0;
			if(opaque_all)
			{
				if(tile_opaque_pf[extra_planes][(tile&0xffff)%total_elements]!=1) opaque_all=0;
			}

			if(alpha_mode==1)
			{
				if(!opaque_all) return;
			}
			else
			{
				if(alpha_type!=3)
				{
					if((tile>>(16+9))&1) alpha_type|=2;
					else                 alpha_type|=1;
				}
				else if(!opaque_all) break;
			}
		}
		else if(opaque_all) opaque_all=0;

		tile_index++;
	}

	if(trans_all)   {line_t->alpha_mode[line]=0;return;}

	if(alpha_mode>1)
	{
		line_t->alpha_mode[line]|=alpha_type<<4;
	}

	if(opaque_all)
		line_t->alpha_mode[line]|=0x80;
}

static void calculate_clip(INT32 y, UINT16 pri, UINT32* clip0, UINT32* clip1, int* line_enable)
{
	const struct f3_spritealpha_line_inf *sa_line_t=&m_sa_line_inf[0];

	switch (pri)
	{
	case 0x0100: /* Clip plane 1 enable */
		{
			if (sa_line_t->clip0_l[y] > sa_line_t->clip0_r[y])
				*line_enable=0;
			else
				*clip0=(sa_line_t->clip0_l[y]) | (sa_line_t->clip0_r[y]<<16);
			*clip1=0;
		}
		break;
	case 0x0110: /* Clip plane 1 enable, inverted */
		{
			*clip1=(sa_line_t->clip0_l[y]) | (sa_line_t->clip0_r[y]<<16);
			*clip0=0x7fff0000;
		}
		break;
	case 0x0200: /* Clip plane 2 enable */
		{
			if (sa_line_t->clip1_l[y] > sa_line_t->clip1_r[y])
				*line_enable=0;
			else
				*clip0=(sa_line_t->clip1_l[y]) | (sa_line_t->clip1_r[y]<<16);
			*clip1=0;
		}
		break;
	case 0x0220: /* Clip plane 2 enable, inverted */
		{
			*clip1=(sa_line_t->clip1_l[y]) | (sa_line_t->clip1_r[y]<<16);
			*clip0=0x7fff0000;
		}
		break;
	case 0x0300: /* Clip plane 1 & 2 enable */
		{
			INT32 clipl=0,clipr=0;

			if (sa_line_t->clip1_l[y] > sa_line_t->clip0_l[y])
				clipl=sa_line_t->clip1_l[y];
			else
				clipl=sa_line_t->clip0_l[y];

			if (sa_line_t->clip1_r[y] < sa_line_t->clip0_r[y])
				clipr=sa_line_t->clip1_r[y];
			else
				clipr=sa_line_t->clip0_r[y];

			if (clipl > clipr)
				*line_enable=0;
			else
				*clip0=(clipl) | (clipr<<16);
			*clip1=0;
		}
		break;
	case 0x0310: /* Clip plane 1 & 2 enable, plane 1 inverted */
		{
			if (sa_line_t->clip1_l[y] > sa_line_t->clip1_r[y])
				line_enable=NULL;
			else
				*clip0=(sa_line_t->clip1_l[y]) | (sa_line_t->clip1_r[y]<<16);

			*clip1=(sa_line_t->clip0_l[y]) | (sa_line_t->clip0_r[y]<<16);
		}
		break;
	case 0x0320: /* Clip plane 1 & 2 enable, plane 2 inverted */
		{
			if (sa_line_t->clip0_l[y] > sa_line_t->clip0_r[y])
				line_enable=NULL;
			else
				*clip0=(sa_line_t->clip0_l[y]) | (sa_line_t->clip0_r[y]<<16);

			*clip1=(sa_line_t->clip1_l[y]) | (sa_line_t->clip1_r[y]<<16);
		}
		break;
	case 0x0330: /* Clip plane 1 & 2 enable, both inverted */
		{
			INT32 clipl=0,clipr=0;

			if (sa_line_t->clip1_l[y] < sa_line_t->clip0_l[y])
				clipl=sa_line_t->clip1_l[y];
			else
				clipl=sa_line_t->clip0_l[y];

			if (sa_line_t->clip1_r[y] > sa_line_t->clip0_r[y])
				clipr=sa_line_t->clip1_r[y];
			else
				clipr=sa_line_t->clip0_r[y];

			if (clipl > clipr)
				*line_enable=0;
			else
				*clip1=(clipl) | (clipr<<16);
			*clip0=0x7fff0000;
		}
		break;
	default:
		// popmessage("Illegal clip mode");
		break;
	}
}

static void get_line_ram_info(INT32 which_map, INT32 sx, INT32 sy, INT32 pos, UINT16 *f3_pf_data_n)
{
	UINT16 *m_f3_line_ram = (UINT16*)TaitoF3LineRAM;
	struct f3_playfield_line_inf *line_t=&m_pf_line_inf[pos];

	INT32 y,y_start,y_end,y_inc;
	INT32 line_base,zoom_base,col_base,pri_base,inc;

	INT32 line_enable;
	INT32 colscroll=0,x_offset=0,line_zoom=0;
	UINT32 _y_zoom[256];
	UINT16 pri=0;
	INT32 bit_select=1<<pos;

	INT32 _colscroll[256];
	UINT32 _x_offset[256];
	INT32 y_index_fx;

	sx+=((46<<16));

	if (flipscreen)
	{
		line_base=0xa1fe + (pos*0x200);
		zoom_base=0x81fe;// + (pos*0x200);
		col_base =0x41fe + (pos*0x200);
		pri_base =0xb1fe + (pos*0x200);
		inc=-2;
		y_start=255;
		y_end=-1;
		y_inc=-1;

		if (extended_layers)    sx=-sx+(((188-512)&0xffff)<<16); else sx=-sx+(188<<16); /* Adjust for flipped scroll position */
		y_index_fx=-sy-(256<<16); /* Adjust for flipped scroll position */
	}
	else
	{
		line_base=0xa000 + (pos*0x200);
		zoom_base=0x8000;// + (pos*0x200);
		col_base =0x4000 + (pos*0x200);
		pri_base =0xb000 + (pos*0x200);
		inc=2;
		y_start=0;
		y_end=256;
		y_inc=1;

		y_index_fx=sy;
	}

	y=y_start;

	while(y!=y_end)
	{
		/* The zoom, column and row values can latch according to control ram */
		{
			if (m_f3_line_ram[0x600+(y)]&bit_select)
				x_offset=(m_f3_line_ram[line_base/2]&0xffff)<<10;
			if (m_f3_line_ram[0x700+(y)]&bit_select)
				pri=m_f3_line_ram[pri_base/2]&0xffff;

			// Zoom for playfields 1 & 3 is interleaved, as is the latch select
			switch (pos)
			{
			case 0:
				if (m_f3_line_ram[0x400+(y)]&bit_select)
					line_zoom=m_f3_line_ram[(zoom_base+0x000)/2]&0xffff;
				break;
			case 1:
				if (m_f3_line_ram[0x400+(y)]&0x2)
					line_zoom=((m_f3_line_ram[(zoom_base+0x200)/2]&0xffff)&0xff00) | (line_zoom&0x00ff);
				if (m_f3_line_ram[0x400+(y)]&0x8)
					line_zoom=((m_f3_line_ram[(zoom_base+0x600)/2]&0xffff)&0x00ff) | (line_zoom&0xff00);
				break;
			case 2:
				if (m_f3_line_ram[0x400+(y)]&bit_select)
					line_zoom=m_f3_line_ram[(zoom_base+0x400)/2]&0xffff;
				break;
			case 3:
				if (m_f3_line_ram[0x400+(y)]&0x8)
					line_zoom=((m_f3_line_ram[(zoom_base+0x600)/2]&0xffff)&0xff00) | (line_zoom&0x00ff);
				if (m_f3_line_ram[0x400+(y)]&0x2)
					line_zoom=((m_f3_line_ram[(zoom_base+0x200)/2]&0xffff)&0x00ff) | (line_zoom&0xff00);
				break;
			default:
				break;
			}

			// Column scroll only affects playfields 2 & 3
			if (pos>=2 && m_f3_line_ram[0x000+(y)]&bit_select)
				colscroll=(m_f3_line_ram[col_base/2]>> 0)&0x3ff;
		}

		if (!pri || (!flipscreen && y<24) || (flipscreen && y>231) ||
			(pri&0xc000)==0xc000 || !(pri&0x2000)/**/)
			line_enable=0;
		else if(pri&0x4000) //alpha1
			line_enable=2;
		else if(pri&0x8000) //alpha2
			line_enable=3;
		else
			line_enable=1;

		_colscroll[y]=colscroll;
		_x_offset[y]=(x_offset&0xffff0000) - (x_offset&0x0000ffff);
		_y_zoom[y] = (line_zoom&0xff) << 9;

		/* Evaluate clipping */
		if (pri&0x0800)
			line_enable=0;
		else if (pri&0x0330)
		{
			//fast path todo - remove line enable
			calculate_clip(y, pri&0x0330, &line_t->clip0[y], &line_t->clip1[y], &line_enable);
		}
		else
		{
			/* No clipping */
			line_t->clip0[y]=0x7fff0000;
			line_t->clip1[y]=0;
		}

		line_t->x_zoom[y]=0x10000 - (line_zoom&0xff00);
		line_t->alpha_mode[y]=line_enable;
		line_t->pri[y]=pri;

		zoom_base+=inc;
		line_base+=inc;
		col_base +=inc;
		pri_base +=inc;
		y +=y_inc;
	}

	UINT8 *pmap = bitmap_flags[which_map];
	UINT16 * tm = bitmap_layer[which_map];
	UINT16 * tmap = bitmap_layer[which_map];
	INT32 map_width = bitmap_width[which_map];

	y=y_start;
	while(y!=y_end)
	{
		UINT32 x_index_fx;
		UINT32 y_index;

		/* The football games use values in the range 0x200-0x3ff where the crowd should be drawn - !?

		   This appears to cause it to reference outside of the normal tilemap RAM area into the unused
		   area on the 32x32 tilemap configuration.. but exactly how isn't understood

		    Until this is understood we're creating additional tilemaps for the otherwise unused area of
		    RAM and forcing it to look up there.

		    the crowd area still seems to 'lag' behind the pitch area however.. but these are the values
		    in ram??
		*/
		INT32 cs = _colscroll[y];

		if (cs&0x200)
		{
			if (extended_layers == 0)
			{
				if (which_map == 2) {
					tmap = bitmap_layer[4]; //m_pf5_tilemap; // pitch -> crowd
					//pmap = bitmap_flags[4]; // breaks hthero & the footy games. keeping for reference. -dink
					//map_width = bitmap_width[4];
				}
				if (which_map == 3) {
					tmap = bitmap_layer[5]; //m_pf6_tilemap; // corruption on goals -> blank (hthero94)
					//pmap = bitmap_flags[5]; // "same as above"
					//map_width = bitmap_width[5];
				}
			}
		}
		else
		{
			tmap = tm;
			map_width = bitmap_width[which_map];
		}

		/* set pixmap pointer */
		UINT16 *srcbitmap = tmap;
		UINT8 *flagsbitmap = pmap;

//		bprintf (0, _T("%d, %d\n"), which_map, bitmap_width[which_map]);

		if(line_t->alpha_mode[y]!=0)
		{
			UINT16 *src_s;
			UINT8 *tsrc_s;

			x_index_fx = (sx+_x_offset[y]-(10*0x10000)+(10*line_t->x_zoom[y]))&((m_width_mask<<16)|0xffff);
			y_index = ((y_index_fx>>16)+_colscroll[y])&0x1ff;

			/* check tile status */
			visible_tile_check(line_t,y,x_index_fx,y_index,f3_pf_data_n);

			/* If clipping enabled for this line have to disable 'all opaque' optimisation */
			if (line_t->clip0[y]!=0x7fff0000 || line_t->clip1[y]!=0)
				line_t->alpha_mode[y]&=~0x80;

			/* set pixmap index */
			line_t->x_count[y]=x_index_fx & 0xffff; // Fractional part
			line_t->src_s[y]=src_s=srcbitmap + (y_index * map_width);
			line_t->src_e[y]=&src_s[m_width_mask+1];
			line_t->src[y]=&src_s[x_index_fx>>16];

			line_t->tsrc_s[y]=tsrc_s=flagsbitmap + (y_index * map_width);
			line_t->tsrc[y]=&tsrc_s[x_index_fx>>16];
		}

		y_index_fx += _y_zoom[y];
		y +=y_inc;
	}
}

static void get_vram_info(INT32 sx, INT32 sy)
{
	UINT16 *m_f3_line_ram = (UINT16*)TaitoF3LineRAM;

	const struct f3_spritealpha_line_inf *sprite_alpha_line_t=&m_sa_line_inf[0];
	struct f3_playfield_line_inf *line_t=&m_pf_line_inf[4];

	INT32 y,y_start,y_end,y_inc;
	INT32 pri_base,inc;

	INT32 line_enable;

	UINT16 pri=0;

	const INT32 vram_width_mask=0x3ff;

	if (flipscreen)
	{
		pri_base =0x73fe;
		inc=-2;
		y_start=255;
		y_end=-1;
		y_inc=-1;
	}
	else
	{
		pri_base =0x7200;
		inc=2;
		y_start=0;
		y_end=256;
		y_inc=1;
	}

	y=y_start;
	while(y!=y_end)
	{
		/* The zoom, column and row values can latch according to control ram */
		{
			if (m_f3_line_ram[(0x0600/2)+(y)]&0x2)
				pri=(m_f3_line_ram[pri_base/2]&0xffff);
		}

		if (!pri || (!flipscreen && y<24) || (flipscreen && y>231) ||
			(pri&0xc000)==0xc000 || !(pri&0x2000)/**/)
			line_enable=0;
		else if(pri&0x4000) //alpha1
			line_enable=2;
		else if(pri&0x8000) //alpha2
			line_enable=3;
		else
			line_enable=1;

		line_t->pri[y]=pri;

		/* Evaluate clipping */
		if (pri&0x0800)
			line_enable=0;
		else if (pri&0x0330)
		{
			//fast path todo - remove line enable
			calculate_clip(y, pri&0x0330, &line_t->clip0[y], &line_t->clip1[y], &line_enable);
		}
		else
		{
			/* No clipping */
			line_t->clip0[y]=0x7fff0000;
			line_t->clip1[y]=0;
		}

		line_t->x_zoom[y]=0x10000;
		line_t->alpha_mode[y]=line_enable;
		if (line_t->alpha_mode[y]>1)
			line_t->alpha_mode[y]|=0x10;

		pri_base +=inc;
		y +=y_inc;
	}

	sx&=0x1ff;

	/* set pixmap pointer */
	UINT16 *srcbitmap_pixel = bitmap_layer[9];
	UINT8 *flagsbitmap_pixel = bitmap_flags[9];
	UINT16 *srcbitmap_vram = bitmap_layer[8];
	UINT8 *flagsbitmap_vram = bitmap_flags[8];

	y=y_start;
	while(y!=y_end)
	{
		if(line_t->alpha_mode[y]!=0)
		{
			UINT16 *src_s;
			UINT8 *tsrc_s;

			// These bits in control ram indicate whether the line is taken from
			// the VRAM tilemap layer or pixel layer.
			const INT32 usePixelLayer=((sprite_alpha_line_t->sprite_alpha[y]&0xa000)==0xa000);

			/* set pixmap index */
			line_t->x_count[y]=0xffff;
			if (usePixelLayer)
				line_t->src_s[y]=src_s=srcbitmap_pixel + (sy&0xff) * 512;
			else
				line_t->src_s[y]=src_s=srcbitmap_vram + (sy&0x1ff) * 512;
			line_t->src_e[y]=&src_s[vram_width_mask+1];
			line_t->src[y]=&src_s[sx];

			if (usePixelLayer)
				line_t->tsrc_s[y]=tsrc_s=flagsbitmap_pixel + (sy&0xff) * 512;
			else
				line_t->tsrc_s[y]=tsrc_s=flagsbitmap_vram + (sy&0x1ff) * 512;
			line_t->tsrc[y]=&tsrc_s[sx];
		}

		sy++;
		y += y_inc;
	}
}


static void scanline_draw()
{
	INT32 i,j,y,ys,ye;
	INT32 y_start,y_end,y_start_next,y_end_next;
	UINT8 draw_line[256];
	INT16 draw_line_num[256];

	UINT32 rot=0;

	if (flipscreen)
	{
		rot=0;//ORIENTATION_FLIP_Y;
		ys=0;
		ye=232;
	}
	else
	{
		ys=24;
		ye=256;
	}

	y_start=ys;
	y_end=ye;
	memset(draw_line,0,256);

	while(1)
	{
		INT32 pos;
		INT32 pri[5],alpha_mode[5],alpha_mode_flag[5],alpha_level;
		UINT16 sprite_alpha;
		UINT8 sprite_alpha_check;
		UINT8 sprite_alpha_all_2a;
		INT32 spri;
		INT32 alpha;
		INT32 layer_tmp[5];
		struct f3_playfield_line_inf *pf_line_inf = m_pf_line_inf;
		struct f3_spritealpha_line_inf *sa_line_inf = m_sa_line_inf;
		INT32 count_skip_layer=0;
		INT32 sprite[6]={0,0,0,0,0,0};
		const struct f3_playfield_line_inf *line_t[5];


		/* find same status of scanlines */
		pri[0]=pf_line_inf[0].pri[y_start];
		pri[1]=pf_line_inf[1].pri[y_start];
		pri[2]=pf_line_inf[2].pri[y_start];
		pri[3]=pf_line_inf[3].pri[y_start];
		pri[4]=pf_line_inf[4].pri[y_start];
		alpha_mode[0]=pf_line_inf[0].alpha_mode[y_start];
		alpha_mode[1]=pf_line_inf[1].alpha_mode[y_start];
		alpha_mode[2]=pf_line_inf[2].alpha_mode[y_start];
		alpha_mode[3]=pf_line_inf[3].alpha_mode[y_start];
		alpha_mode[4]=pf_line_inf[4].alpha_mode[y_start];
		alpha_level=sa_line_inf[0].alpha_level[y_start];
		spri=sa_line_inf[0].spri[y_start];
		sprite_alpha=sa_line_inf[0].sprite_alpha[y_start];

		draw_line[y_start]=1;
		draw_line_num[i=0]=y_start;
		y_start_next=-1;
		y_end_next=-1;
		for(y=y_start+1;y<y_end;y++)
		{
			if(!draw_line[y])
			{
				if(pri[0]!=pf_line_inf[0].pri[y]) y_end_next=y+1;
				else if(pri[1]!=pf_line_inf[1].pri[y]) y_end_next=y+1;
				else if(pri[2]!=pf_line_inf[2].pri[y]) y_end_next=y+1;
				else if(pri[3]!=pf_line_inf[3].pri[y]) y_end_next=y+1;
				else if(pri[4]!=pf_line_inf[4].pri[y]) y_end_next=y+1;
				else if(alpha_mode[0]!=pf_line_inf[0].alpha_mode[y]) y_end_next=y+1;
				else if(alpha_mode[1]!=pf_line_inf[1].alpha_mode[y]) y_end_next=y+1;
				else if(alpha_mode[2]!=pf_line_inf[2].alpha_mode[y]) y_end_next=y+1;
				else if(alpha_mode[3]!=pf_line_inf[3].alpha_mode[y]) y_end_next=y+1;
				else if(alpha_mode[4]!=pf_line_inf[4].alpha_mode[y]) y_end_next=y+1;
				else if(alpha_level!=sa_line_inf[0].alpha_level[y]) y_end_next=y+1;
				else if(spri!=sa_line_inf[0].spri[y]) y_end_next=y+1;
				else if(sprite_alpha!=sa_line_inf[0].sprite_alpha[y]) y_end_next=y+1;
				else
				{
					draw_line[y]=1;
					draw_line_num[++i]=y;
					continue;
				}

				if(y_start_next<0) y_start_next=y;
			}
		}
		y_end=y_end_next;
		y_start=y_start_next;
		draw_line_num[++i]=-1;

		/* alpha blend */
		alpha_mode_flag[0]=alpha_mode[0]&~3;
		alpha_mode_flag[1]=alpha_mode[1]&~3;
		alpha_mode_flag[2]=alpha_mode[2]&~3;
		alpha_mode_flag[3]=alpha_mode[3]&~3;
		alpha_mode_flag[4]=alpha_mode[4]&~3;
		alpha_mode[0]&=3;
		alpha_mode[1]&=3;
		alpha_mode[2]&=3;
		alpha_mode[3]&=3;
		alpha_mode[4]&=3;
		if( alpha_mode[0]>1 ||
			alpha_mode[1]>1 ||
			alpha_mode[2]>1 ||
			alpha_mode[3]>1 ||
			alpha_mode[4]>1 ||
			(sprite_alpha&0xff) != 0xff  )
		{
			/* set alpha level */
			if(alpha_level!=m_alpha_level_last)
			{
				INT32 al_s,al_d;
				INT32 a=alpha_level;
				INT32 b=(a>>8)&0xf;
				INT32 c=(a>>4)&0xf;
				INT32 d=(a>>0)&0xf;
				a>>=12;

				/* b000 7000 */
				al_s = ( (15-d)*256) / 8;
				al_d = ( (15-b)*256) / 8;
				if(al_s>255) al_s = 255;
				if(al_d>255) al_d = 255;
				m_f3_alpha_level_3as = al_s;
				m_f3_alpha_level_3ad = al_d;
				m_f3_alpha_level_2as = al_d;
				m_f3_alpha_level_2ad = al_s;

				al_s = ( (15-c)*256) / 8;
				al_d = ( (15-a)*256) / 8;
				if(al_s>255) al_s = 255;
				if(al_d>255) al_d = 255;
				m_f3_alpha_level_3bs = al_s;
				m_f3_alpha_level_3bd = al_d;
				m_f3_alpha_level_2bs = al_d;
				m_f3_alpha_level_2bd = al_s;

				f3_alpha_set_level();
				m_alpha_level_last=alpha_level;
			}

			/* set sprite alpha mode */
			sprite_alpha_check=0;
			sprite_alpha_all_2a=1;
			m_dpix_sp[1]=NULL;
			m_dpix_sp[2]=NULL;
			m_dpix_sp[4]=NULL;
			m_dpix_sp[8]=NULL;
			for(i=0;i<4;i++)    /* i = sprite priority offset */
			{
				UINT8 sprite_alpha_mode=(sprite_alpha>>(i*2))&3;
				UINT8 sftbit=1<<i;
				if(sprite_pri_usage&sftbit)
				{
					if(sprite_alpha_mode==1)
					{
						if(m_f3_alpha_level_2as==0 && m_f3_alpha_level_2ad==255)
							sprite_pri_usage&=~sftbit;  // Disable sprite priority block
						else
						{
							m_dpix_sp[sftbit]=m_dpix_n[2];
							sprite_alpha_check|=sftbit;
						}
					}
					else if(sprite_alpha_mode==2)
					{
						if(sprite_alpha&0xff00)
						{
							if(m_f3_alpha_level_3as==0 && m_f3_alpha_level_3ad==255) sprite_pri_usage&=~sftbit;
							else
							{
								m_dpix_sp[sftbit]=m_dpix_n[3];
								sprite_alpha_check|=sftbit;
								sprite_alpha_all_2a=0;
							}
						}
						else
						{
							if(m_f3_alpha_level_3bs==0 && m_f3_alpha_level_3bd==255) sprite_pri_usage&=~sftbit;
							else
							{
								m_dpix_sp[sftbit]=m_dpix_n[5];
								sprite_alpha_check|=sftbit;
								sprite_alpha_all_2a=0;
							}
						}
					}
				}
			}


			/* check alpha level */
			for(i=0;i<5;i++)    /* i = playfield num (pos) */
			{
				INT32 alpha_type = (alpha_mode_flag[i]>>4)&3;

				if(alpha_mode[i]==2)
				{
					if(alpha_type==1)
					{
						if (m_f3_alpha_level_2as==0   && m_f3_alpha_level_2ad==255 && f3_game == GSEEKER) {
							alpha_mode[i]=3; alpha_mode_flag[i] |= 0x80;} /* will display continue screen in gseeker (mt 00026) */
						else if(m_f3_alpha_level_2as==0   && m_f3_alpha_level_2ad==255) alpha_mode[i]=0;
						else if(m_f3_alpha_level_2as==255 && m_f3_alpha_level_2ad==0  ) alpha_mode[i]=1;
					}
					else if(alpha_type==2)
					{
						if     (m_f3_alpha_level_2bs==0   && m_f3_alpha_level_2bd==255) alpha_mode[i]=0;
						else if(m_f3_alpha_level_2as==255 && m_f3_alpha_level_2ad==0 &&
								m_f3_alpha_level_2bs==255 && m_f3_alpha_level_2bd==0  ) alpha_mode[i]=1;
					}
					else if(alpha_type==3)
					{
						if     (m_f3_alpha_level_2as==0   && m_f3_alpha_level_2ad==255 &&
								m_f3_alpha_level_2bs==0   && m_f3_alpha_level_2bd==255) alpha_mode[i]=0;
						else if(m_f3_alpha_level_2as==255 && m_f3_alpha_level_2ad==0   &&
								m_f3_alpha_level_2bs==255 && m_f3_alpha_level_2bd==0  ) alpha_mode[i]=1;
					}
				}
				else if(alpha_mode[i]==3)
				{
					if(alpha_type==1)
					{
						if     (m_f3_alpha_level_3as==0   && m_f3_alpha_level_3ad==255) alpha_mode[i]=0;
						else if(m_f3_alpha_level_3as==255 && m_f3_alpha_level_3ad==0  ) alpha_mode[i]=1;
					}
					else if(alpha_type==2)
					{
						if     (m_f3_alpha_level_3bs==0   && m_f3_alpha_level_3bd==255) alpha_mode[i]=0;
						else if(m_f3_alpha_level_3as==255 && m_f3_alpha_level_3ad==0 &&
								m_f3_alpha_level_3bs==255 && m_f3_alpha_level_3bd==0  ) alpha_mode[i]=1;
					}
					else if(alpha_type==3)
					{
						if     (m_f3_alpha_level_3as==0   && m_f3_alpha_level_3ad==255 &&
								m_f3_alpha_level_3bs==0   && m_f3_alpha_level_3bd==255) alpha_mode[i]=0;
						else if(m_f3_alpha_level_3as==255 && m_f3_alpha_level_3ad==0   &&
								m_f3_alpha_level_3bs==255 && m_f3_alpha_level_3bd==0  ) alpha_mode[i]=1;
					}
				}
			}

			if (    (alpha_mode[0]==1 || alpha_mode[0]==2 || !alpha_mode[0]) &&
					(alpha_mode[1]==1 || alpha_mode[1]==2 || !alpha_mode[1]) &&
					(alpha_mode[2]==1 || alpha_mode[2]==2 || !alpha_mode[2]) &&
					(alpha_mode[3]==1 || alpha_mode[3]==2 || !alpha_mode[3]) &&
					(alpha_mode[4]==1 || alpha_mode[4]==2 || !alpha_mode[4]) &&
					sprite_alpha_all_2a                     )
			{
				INT32 alpha_type = (alpha_mode_flag[0] | alpha_mode_flag[1] | alpha_mode_flag[2] | alpha_mode_flag[3])&0x30;
				if(     (alpha_type==0x10 && m_f3_alpha_level_2as==255) ||
						(alpha_type==0x20 && m_f3_alpha_level_2as==255 && m_f3_alpha_level_2bs==255) ||
						(alpha_type==0x30 && m_f3_alpha_level_2as==255 && m_f3_alpha_level_2bs==255)  )
				{
					if(alpha_mode[0]>1) alpha_mode[0]=1;
					if(alpha_mode[1]>1) alpha_mode[1]=1;
					if(alpha_mode[2]>1) alpha_mode[2]=1;
					if(alpha_mode[3]>1) alpha_mode[3]=1;
					if(alpha_mode[4]>1) alpha_mode[4]=1;
					sprite_alpha_check=0;
					m_dpix_sp[1]=NULL;
					m_dpix_sp[2]=NULL;
					m_dpix_sp[4]=NULL;
					m_dpix_sp[8]=NULL;
				}
			}
		}
		else
		{
			sprite_alpha_check=0;
			m_dpix_sp[1]=NULL;
			m_dpix_sp[2]=NULL;
			m_dpix_sp[4]=NULL;
			m_dpix_sp[8]=NULL;
		}



		/* set scanline priority */
		{
			INT32 pri_max_opa=-1;
			for(i=0;i<5;i++)    /* i = playfield num (pos) */
			{
				INT32 p0=pri[i];
				INT32 pri_sl1=p0&0x0f;

				layer_tmp[i]=i + (pri_sl1<<3);

				if(!alpha_mode[i])
				{
					layer_tmp[i]|=0x80;
					count_skip_layer++;
				}
				else if(alpha_mode[i]==1 && (alpha_mode_flag[i]&0x80))
				{
					if(layer_tmp[i]>pri_max_opa) pri_max_opa=layer_tmp[i];
				}
			}

			if(pri_max_opa!=-1)
			{
				if(pri_max_opa>layer_tmp[0]) {layer_tmp[0]|=0x80;count_skip_layer++;}
				if(pri_max_opa>layer_tmp[1]) {layer_tmp[1]|=0x80;count_skip_layer++;}
				if(pri_max_opa>layer_tmp[2]) {layer_tmp[2]|=0x80;count_skip_layer++;}
				if(pri_max_opa>layer_tmp[3]) {layer_tmp[3]|=0x80;count_skip_layer++;}
				if(pri_max_opa>layer_tmp[4]) {layer_tmp[4]|=0x80;count_skip_layer++;}
			}
		}


		/* sort layer_tmp */
		for(i=0;i<4;i++)
		{
			for(j=i+1;j<5;j++)
			{
				if(layer_tmp[i]<layer_tmp[j])
				{
					INT32 temp = layer_tmp[i];
					layer_tmp[i] = layer_tmp[j];
					layer_tmp[j] = temp;
				}
			}
		}


		/* check sprite & layer priority */
		{
			INT32 l0,l1,l2,l3,l4;
			INT32 pri_sp[5];

			l0=layer_tmp[0]>>3;
			l1=layer_tmp[1]>>3;
			l2=layer_tmp[2]>>3;
			l3=layer_tmp[3]>>3;
			l4=layer_tmp[4]>>3;

			pri_sp[0]=spri&0xf;
			pri_sp[1]=(spri>>4)&0xf;
			pri_sp[2]=(spri>>8)&0xf;
			pri_sp[3]=spri>>12;

			for(i=0;i<4;i++)    /* i = sprite priority offset */
			{
				INT32 sp,sflg=1<<i;
				if(!(sprite_pri_usage & sflg)) continue;
				sp=pri_sp[i];

				/*
				    sprite priority==playfield priority
				        GSEEKER (plane leaving hangar) --> sprite
				        BUBSYMPH (title)       ---> sprite
				        DARIUSG (ZONE V' BOSS) ---> playfield
				*/

				if (f3_game == BUBSYMPH ) sp++;        //BUBSYMPH (title)
				if (f3_game == GSEEKER ) sp++;     //GSEEKER (plane leaving hangar)

						if(       sp>l0) sprite[0]|=sflg;
				else if(sp<=l0 && sp>l1) sprite[1]|=sflg;
				else if(sp<=l1 && sp>l2) sprite[2]|=sflg;
				else if(sp<=l2 && sp>l3) sprite[3]|=sflg;
				else if(sp<=l3 && sp>l4) sprite[4]|=sflg;
				else if(sp<=l4         ) sprite[5]|=sflg;
			}
		}


		/* draw scanlines */
		alpha=0;
		for(i=count_skip_layer;i<5;i++)
		{
			pos=layer_tmp[i]&7;
			line_t[i]=&pf_line_inf[pos];

			if(sprite[i]&sprite_alpha_check) alpha=1;
			else if(!alpha) sprite[i]|=0x100;

			if(alpha_mode[pos]>1)
			{
				INT32 alpha_type=(((alpha_mode_flag[pos]>>4)&3)-1)*2;
				m_dpix_lp[i]=m_dpix_n[alpha_mode[pos]+alpha_type];
				alpha=1;
			}
			else
			{
				if(alpha) m_dpix_lp[i]=m_dpix_n[1];
				else      m_dpix_lp[i]=m_dpix_n[0];
			}
		}
		if(sprite[5]&sprite_alpha_check) alpha=1;
		else if(!alpha) sprite[5]|=0x100;

		draw_scanlines(320,draw_line_num,line_t,sprite,rot,count_skip_layer);
		if(y_start<0) break;
	}
}


static void get_spritealphaclip_info()
{
	UINT16 *m_f3_line_ram = (UINT16*)TaitoF3LineRAM;
	struct f3_spritealpha_line_inf *line_t=&m_sa_line_inf[0];

	INT32 y,y_end,y_inc;

	INT32 spri_base,clip_base_low,clip_base_high,inc;

	UINT16 spri=0;
	UINT16 sprite_clip=0;
	UINT16 clip0_low=0, clip0_high=0, clip1_low=0;
	INT32 alpha_level=0;
	UINT16 sprite_alpha=0;

	if (flipscreen)
	{
		spri_base=0x77fe;
		clip_base_low=0x51fe;
		clip_base_high=0x45fe;
		inc=-2;
		y=255;
		y_end=-1;
		y_inc=-1;

	}
	else
	{
		spri_base=0x7600;
		clip_base_low=0x5000;
		clip_base_high=0x4400;
		inc=2;
		y=0;
		y_end=256;
		y_inc=1;
	}

	while(y!=y_end)
	{
		/* The zoom, column and row values can latch according to control ram */
		{
			if (m_f3_line_ram[0x100+(y)]&1)
				clip0_low=(m_f3_line_ram[clip_base_low/2]>> 0)&0xffff;
			if (m_f3_line_ram[0x000+(y)]&4)
				clip0_high=(m_f3_line_ram[clip_base_high/2]>> 0)&0xffff;
			if (m_f3_line_ram[0x100+(y)]&2)
				clip1_low=(m_f3_line_ram[(clip_base_low+0x200)/2]>> 0)&0xffff;

			if (m_f3_line_ram[(0x0600/2)+(y)]&0x8)
				spri=m_f3_line_ram[spri_base/2]&0xffff;
			if (m_f3_line_ram[(0x0600/2)+(y)]&0x4)
				sprite_clip=m_f3_line_ram[(spri_base-0x200)/2]&0xffff;
			if (m_f3_line_ram[(0x0400/2)+(y)]&0x1)
				sprite_alpha=m_f3_line_ram[(spri_base-0x1600)/2]&0xffff;
			if (m_f3_line_ram[(0x0400/2)+(y)]&0x2)
				alpha_level=m_f3_line_ram[(spri_base-0x1400)/2]&0xffff;
		}


		line_t->alpha_level[y]=alpha_level;
		line_t->spri[y]=spri;
		line_t->sprite_alpha[y]=sprite_alpha;
		line_t->clip0_l[y]=((clip0_low&0xff)|((clip0_high&0x1000)>>4)) - 47;
		line_t->clip0_r[y]=(((clip0_low&0xff00)>>8)|((clip0_high&0x2000)>>5)) - 47;
		line_t->clip1_l[y]=((clip1_low&0xff)|((clip0_high&0x4000)>>6)) - 47;
		line_t->clip1_r[y]=(((clip1_low&0xff00)>>8)|((clip0_high&0x8000)>>7)) - 47;
		if (line_t->clip0_l[y]<0) line_t->clip0_l[y]=0;
		if (line_t->clip0_r[y]<0) line_t->clip0_r[y]=0;
		if (line_t->clip1_l[y]<0) line_t->clip1_l[y]=0;
		if (line_t->clip1_r[y]<0) line_t->clip1_r[y]=0;

		/* Evaluate sprite clipping */
		if (sprite_clip&0x080)
		{
			line_t->sprite_clip0[y]=0x7fff7fff;
			line_t->sprite_clip1[y]=0;
		}
		else if (sprite_clip&0x33)
		{
			INT32 line_enable=1;
			calculate_clip(y, ((sprite_clip&0x33)<<4), &line_t->sprite_clip0[y], &line_t->sprite_clip1[y], &line_enable);
			if (line_enable==0)
				line_t->sprite_clip0[y]=0x7fff7fff;
		}
		else
		{
			line_t->sprite_clip0[y]=0x7fff0000;
			line_t->sprite_clip1[y]=0;
		}

		spri_base+=inc;
		clip_base_low+=inc;
		clip_base_high+=inc;
		y +=y_inc;
	}
}

void TaitoF3VideoInit()
{
	clear_f3_stuff();
	m_f3_alpha_level_2as=127;
	m_f3_alpha_level_2ad=127;
	m_f3_alpha_level_3as=127;
	m_f3_alpha_level_3ad=127;
	m_f3_alpha_level_2bs=127;
	m_f3_alpha_level_2bd=127;
	m_f3_alpha_level_3bs=127;
	m_f3_alpha_level_3bd=127;
	m_alpha_level_last = -1;

	m_pdest_2a = 0x10;
	m_pdest_2b = 0x20;
	m_tr_2a = 0;
	m_tr_2b = 1;
	m_pdest_3a = 0x40;
	m_pdest_3b = 0x80;
	m_tr_3a = 0;
	m_tr_3b = 1;

	m_width_mask=(extended_layers) ? 0x3ff : 0x1ff;
	m_twidth_mask=(extended_layers) ? 0x7f : 0x3f;
	m_twidth_mask_bit=(extended_layers) ? 7 : 6;

	m_spritelist = (struct tempsprite*)BurnMalloc(0x400 * sizeof(struct tempsprite));
	m_sprite_end = m_spritelist;


	init_alpha_blend_func();


}


static void pal16_check_init()
{
	if (nBurnBpp < 3 && !pal16) {
		pal16 = (UINT16 *)BurnMalloc((1<<24) * sizeof (UINT16));

		for (INT32 i = 0; i < (1 << 24); i++) {
			INT32 r = (i >> (16+3)) & 0x1f;
			INT32 g = (i >> (8+2)) & 0x3f;
			INT32 b = (i >> (0+3)) & 0x1f;
			pal16[i] = (r << 11) | (g << 5) | b;
		}
	}
}

void TaitoF3VideoExit()
{

	BurnFree (m_spritelist);

	if (pal16) {
		BurnFree(pal16);
		pal16 = NULL;
	}

}


void TaitoF3DrawCommon(INT32 scanline_start)
{
	if (TaitoF3PalRecalc) {
		pal16_check_init();

		for (INT32 i = 0; i < 0x8000; i+=4) {
			pPaletteUpdateCallback(i);
		}
		TaitoF3PalRecalc = 0;
	}

	UINT16 *m_f3_control_0 = (UINT16*)TaitoF3CtrlRAM;
	UINT16 *m_f3_control_1 = (UINT16*)(TaitoF3CtrlRAM + 0x10);

	UINT32 sy_fix[5],sx_fix[5];

	/* Setup scroll */
	sy_fix[0]=((m_f3_control_0[4]&0xffff)<< 9) + (1<<16);
	sy_fix[1]=((m_f3_control_0[5]&0xffff)<< 9) + (1<<16);
	sy_fix[2]=((m_f3_control_0[6]&0xffff)<< 9) + (1<<16);
	sy_fix[3]=((m_f3_control_0[7]&0xffff)<< 9) + (1<<16);
	sx_fix[0]=((m_f3_control_0[0]&0xffc0)<<10) - (6<<16);
	sx_fix[1]=((m_f3_control_0[1]&0xffc0)<<10) - (10<<16);
	sx_fix[2]=((m_f3_control_0[2]&0xffc0)<<10) - (14<<16);
	sx_fix[3]=((m_f3_control_0[3]&0xffc0)<<10) - (18<<16);
	sx_fix[4]=-(m_f3_control_1[4])+41;
	sy_fix[4]=-(m_f3_control_1[5]&0x1ff);

	sx_fix[0]-=((m_f3_control_0[0]&0x003f)<<10)+0x0400-0x10000;
	sx_fix[1]-=((m_f3_control_0[1]&0x003f)<<10)+0x0400-0x10000;
	sx_fix[2]-=((m_f3_control_0[2]&0x003f)<<10)+0x0400-0x10000;
	sx_fix[3]-=((m_f3_control_0[3]&0x003f)<<10)+0x0400-0x10000;

	if (flipscreen)
	{
		sy_fix[0]= 0x3000000-sy_fix[0];
		sy_fix[1]= 0x3000000-sy_fix[1];
		sy_fix[2]= 0x3000000-sy_fix[2];
		sy_fix[3]= 0x3000000-sy_fix[3];

		sx_fix[0]=-0x1a00000-sx_fix[0];
		sx_fix[1]=-0x1a00000-sx_fix[1];
		sx_fix[2]=-0x1a00000-sx_fix[2];
		sx_fix[3]=-0x1a00000-sx_fix[3];

		sx_fix[4]=-sx_fix[4] + 75;
		sy_fix[4]=-sy_fix[4];
	}

	memset (TaitoPriorityMap, 0, 1024 * 512);
	memset (output_bitmap, 0, 512 * 512 * sizeof(UINT32));

	switch (sprite_lag) {
		case 2: get_sprite_info((UINT16*)TaitoSpriteRamDelayed2); break;
		case 1: get_sprite_info((UINT16*)TaitoSpriteRamDelayed); break;
    		default: get_sprite_info((UINT16*)TaitoSpriteRam); 	break;
	}

	if (nSpriteEnable & 1) draw_sprites();

	get_spritealphaclip_info();

	for (INT32 i = 0; i < (8 >> extended_layers); i++) {
		if (nBurnLayer & 1) draw_pf_layer(i);
	}

	if (nBurnLayer & 2) draw_pixel_layer();
	if (nBurnLayer & 4) draw_vram_layer();

	{
		get_line_ram_info(0,sx_fix[0],sy_fix[0],0,(UINT16*)(TaitoF3PfRAM + (extended_layers ? 0x0000 : 0x0000)));

		get_line_ram_info(1,sx_fix[1],sy_fix[1],1,(UINT16*)(TaitoF3PfRAM + (extended_layers ? 0x2000 : 0x1000)));

		get_line_ram_info(2,sx_fix[2],sy_fix[2],2,(UINT16*)(TaitoF3PfRAM + (extended_layers ? 0x4000 : 0x2000)));

		get_line_ram_info(3,sx_fix[3],sy_fix[3],3,(UINT16*)(TaitoF3PfRAM + (extended_layers ? 0x6000 : 0x3000)));

		get_vram_info(sx_fix[4],sy_fix[4]);

		if (nBurnLayer & 8) scanline_draw();
	}

	// copy video to draw surface
	{
		if (flipscreen)
		{
			scanline_start = (scanline_start == 0x1234) ? 1 : 0; // super-kludge for gunlock. -dink

			UINT32 *src = output_bitmap + ((nScreenHeight + scanline_start - 1) * 512) + 46;
			UINT8 *dst = pBurnDraw;

			for (INT32 y = 0, i = 0; y < nScreenHeight; y++)
			{
				if (nBurnBpp == 2) { // 16bpp
					for (INT32 x = 0; x < nScreenWidth; x++, i++, dst += nBurnBpp)
					{
						//UINT32 pixel = src[x];
						PutPix(dst, pal16[src[x]]);
					}

					src -= 512;
				} else if (nBurnBpp == 4) { // quad block-32bit (fast)
					for (INT32 x = 0; x < nScreenWidth; x+=4, i++, dst += (nBurnBpp*4))
					{
						*((UINT32*)(dst + 0)) = src[x + 0];
						*((UINT32*)(dst + 4)) = src[x + 1];
						*((UINT32*)(dst + 8)) = src[x + 2];
						*((UINT32*)(dst + 12))= src[x + 3];
					}

					src -= 512;
				} else { // 24bit
					for (INT32 x = 0; x < nScreenWidth; x++, i++, dst += nBurnBpp)
					{
						PutPix(dst, src[x]);
					}

					src -= 512;
				}
			}
		}
		else
		{
			UINT32 *src = output_bitmap + (scanline_start * 512) + 46;
			UINT8 *dst = pBurnDraw;

			for (INT32 y = 0, i = 0; y < nScreenHeight; y++)
			{	
				if (nBurnBpp == 2) { // 16bpp
					for (INT32 x = 0; x < nScreenWidth; x++, i++, dst += nBurnBpp)
					{
						PutPix(dst, pal16[src[x]]);
					}

					src += 512;
				} else if (nBurnBpp == 4) { // quad block-32bit (fast)
					for (INT32 x = 0; x < nScreenWidth; x+=4, i++, dst += (nBurnBpp*4))
					{
						*((UINT32*)(dst + 0)) = src[x + 0];
						*((UINT32*)(dst + 4)) = src[x + 1];
						*((UINT32*)(dst + 8)) = src[x + 2];
						*((UINT32*)(dst + 12))= src[x + 3];
					}

					src += 512;
				} else { // 24bpp
					for (INT32 x = 0; x < nScreenWidth; x++, i++, dst += nBurnBpp)
					{
						PutPix(dst, src[x]);
					}

					src += 512;
				}
			}
		}
	}
}
