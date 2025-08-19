/*
    render.c --
    Display rendering.
*/

#include "smsshared.h"

static UINT8 sms_cram_expand_table[4];
static UINT8 gg_cram_expand_table[16];

/* Background drawing function */
void (*render_bg)(INT16 line) = NULL;
void (*render_obj)(INT16 line) = NULL;
static void parse_satb(INT16 line);

/* Pointer to output buffer */
UINT8 *linebuf;

/* Internal buffer for drawing non 8-bit displays */
static UINT8 internal_buffer[0x200];

/* Precalculated pixel table */
static UINT32 pixel[PALETTE_SIZE];

/* Dirty pattern info */
UINT8 bg_name_dirty[0x200];     /* 1= This pattern is dirty */
UINT16 bg_name_list[0x200];     /* List of modified pattern indices */
UINT16 bg_list_index;           /* # of modified patterns in list */
UINT8 bg_pattern_cache[0x20000];/* Cached and flipped patterns */
UINT16 smsvdp_ntmask;

/* Pixel look-up table */
static UINT8 lut[0x10000];

/* Attribute expansion table */
static const UINT32 atex[4] =
{
	0x00000000,
	0x10101010,
	0x20202020,
	0x30303030,
};

/* Bitplane to packed pixel LUT */
static UINT32 bp_lut[0x10000];

static struct
{
  UINT16 yrange;
  UINT16 xpos;
  UINT16 attr;
} object_info[64];

static UINT8 object_index_count;

UINT32 gg_overscanmode;

/* Macros to access memory 32-bits at a time (from MAME's drawgfx.c) */

#ifdef ALIGN_DWORD

static __inline__ UINT32 read_dword(void *address)
{
	if ((UINT32)address & 3)
	{
#ifdef LSB_FIRST  /* little endian version */
		return ( *((UINT8 *)address) +
				(*((UINT8 *)address+1) << 8)  +
				(*((UINT8 *)address+2) << 16) +
				(*((UINT8 *)address+3) << 24) );
#else             /* big endian version */
		return ( *((UINT8 *)address+3) +
				(*((UINT8 *)address+2) << 8)  +
				(*((UINT8 *)address+1) << 16) +
				(*((UINT8 *)address)   << 24) );
#endif
	}
	else
		return *(UINT32 *)address;
}


static __inline__ void write_dword(void *address, UINT32 data)
{
	if ((UINT32)address & 3)
	{
#ifdef LSB_FIRST
		*((UINT8 *)address) =    data;
		*((UINT8 *)address+1) = (data >> 8);
		*((UINT8 *)address+2) = (data >> 16);
		*((UINT8 *)address+3) = (data >> 24);
#else
		*((UINT8 *)address+3) =  data;
		*((UINT8 *)address+2) = (data >> 8);
		*((UINT8 *)address+1) = (data >> 16);
		*((UINT8 *)address)   = (data >> 24);
#endif
		return;
	}
	else
		*(UINT32 *)address = data;
}
#else
#define read_dword(address) *(UINT32 *)address
#define write_dword(address,data) *(UINT32 *)address=data
#endif


/****************************************************************************/


void render_shutdown(void)
{
}

/* Initialize the rendering data */
void render_init(void)
{
	INT32 i, j;
	INT32 bx, sx, b, s, bp, bf, sf, c;

	make_tms_tables();
	memset(&lut, 0, sizeof(lut));
	memset(&bp_lut, 0, sizeof(bp_lut));

	/* Generate 64k of data for the look up table */
	for(bx = 0; bx < 0x100; bx++)
	{
		for(sx = 0; sx < 0x100; sx++)
		{
			/* Background pixel */
			b  = (bx & 0x0F);

			/* Background priority */
			bp = (bx & 0x20) ? 1 : 0;

			/* Full background pixel + priority + sprite marker */
			bf = (bx & 0x7F);

			/* Sprite pixel */
			s  = (sx & 0x0F);

			/* Full sprite pixel, w/ palette and marker bits added */
			sf = (sx & 0x0F) | 0x10 | 0x40;

			/* Overwriting a sprite pixel ? */
			if(bx & 0x40)
			{
				/* Return the input */
				c = bf;
			}
			else
			{
				/* Work out priority and transparency for both pixels */
				if(bp)
				{
					/* Underlying pixel is high priority */
					if(b)
					{
						c = bf | 0x40;
					}
					else
					{

						if(s)
						{
							c = sf;
						}
						else
						{
							c = bf;
						}
					}
				}
				else
				{
					/* Underlying pixel is low priority */
					if(s)
					{
						c = sf;
					}
					else
					{
						c = bf;
					}
				}
			}

			/* Store result */
			lut[(bx << 8) | (sx)] = c;
		}
	}

	/* Make bitplane to pixel lookup table */
	for(i = 0; i < 0x100; i++)
		for(j = 0; j < 0x100; j++)
		{
			INT32 x;
			UINT32 out = 0;
			for(x = 0; x < 8; x++)
			{
				out |= (j & (0x80 >> x)) ? (UINT32)(8 << (x << 2)) : 0;
				out |= (i & (0x80 >> x)) ? (UINT32)(4 << (x << 2)) : 0;
			}
#ifdef LSB_FIRST
			bp_lut[(j << 8) | (i)] = out;
#else
			bp_lut[(i << 8) | (j)] = out;
#endif
		}

	for(i = 0; i < 4; i++)
	{
		UINT8 c2 = i << 6 | i << 4 | i << 2 | i;
		sms_cram_expand_table[i] = c2;
	}

	for(i = 0; i < 16; i++)
	{
		UINT8 c2 = i << 4 | i;
		gg_cram_expand_table[i] = c2;
	}

	render_reset();

}


/* Reset the rendering data */
void render_reset(void)
{
	INT32 i;

	/* Clear palette */
	for(i = 0; i < PALETTE_SIZE; i++)
	{
		palette_sync(i, 1);
	}

	/* Invalidate pattern cache */
	memset(&bg_name_dirty, 0, sizeof(bg_name_dirty));
	memset(&bg_name_list, 0, sizeof(bg_name_list));
	bg_list_index = 0;
	memset(&bg_pattern_cache, 0, sizeof(bg_pattern_cache));
	memset(&internal_buffer, 0, sizeof(internal_buffer));
	/* Pick render routine */
	render_bg = render_bg_sms;
	render_obj = render_obj_sms;
}


/* Draw a line of the display */
void render_line(INT16 line)
{
	static INT32 last_line = -1;

	if (line >= vdp.lpf || line == last_line)
		return;

	last_line = line;

	/* Point to current line in output buffer */
	linebuf = &internal_buffer[0];

	memset(linebuf, BACKDROP_COLOR, bitmap.width);

	/* Sprite limit flag is set at the beginning of the line */
	if (vdp.spr_ovr)
	{
		vdp.spr_ovr = 0;
		vdp.status |= 0x40;
	}

	INT32 extend = (vdp.extended && IS_GG) ? 16 : 0; // GG: in extended mode, the screen starts 16pixels lower than usual.
	INT32 offset = 0;

	if((IS_GG) && ((!gg_overscanmode && ((line < 24 + extend) || (line > 167 + extend))) || (gg_overscanmode && line < 9))) {
		// GG: Blank top and bottom borders.
		// GG: If overscan mode is enabled, only blank the top 8 lines (which are the most corrupted).
	} else
		if(vdp.reg[1] & 0x40) // Display enable
		{
			/* Update pattern cache */
			update_bg_pattern_cache();

			/* Draw background */
			if(render_bg != NULL && (nBurnLayer & 1))
				render_bg(line);

			/* Draw sprites */
			if(render_obj != NULL && (nSpriteEnable & 1))
				render_obj(line);

			/* Blank leftmost column of display & center screen */
			if(vdp.reg[0] & 0x20)
			{
				if (IS_GG) {
					bitmap.viewport.x = 44; // fixes crap on the right side of the screen in Devilish, Yuyu & Codemasters games
					offset = 4;
				}
				memset(linebuf, BACKDROP_COLOR, 8);
				// center the screen
				memmove(linebuf+4, linebuf+8, bitmap.viewport.w + bitmap.viewport.x);
				if (!IS_GG) {
					memset(linebuf+bitmap.viewport.w + bitmap.viewport.x - 4, BACKDROP_COLOR, 4);
				}
			} else {
				if (IS_GG) {
					bitmap.viewport.x = 48;
				}
			}
		}

	/* Parse Sprites for next line */
	if (vdp.mode > 7)
		parse_satb(line);
	else
		parse_line(line);

	if (line < vdp.height) blit_linebuf(line, extend, offset);
}


/* Draw the Master System background */
void render_bg_sms(INT16 line)
{
	INT32 locked = 0;
	INT32 yscroll_mask = (vdp.extended) ? 256 : 224;
	INT32 v_line = (line + vdp.reg[9]) % yscroll_mask;
	INT32 v_row  = (v_line & 7) << 3;
	INT32 hscroll = ((vdp.reg[0] & 0x40) && (line < 0x10)) ? 0 : (0x100 - vdp.reg[8]);
	INT32 column = 0;
	UINT16 attr;
	UINT16 *nt = (UINT16 *)&vdp.vram[(vdp.ntab + ((v_line >> 3) << 6)) & smsvdp_ntmask];
	INT32 nt_scroll = (hscroll >> 3);
	INT32 shift = (hscroll & 7);
	UINT32 atex_mask;
	UINT32 *cache_ptr;
	UINT32 *linebuf_ptr = (UINT32 *)&linebuf[0 - shift];

	/* Draw first column (clipped) */
	if(shift)
	{
		INT32 x;

		for(x = shift; x < 8; x++)
			linebuf[(0 - shift) + (x)] = 0;

		column++;
	}

	/* Draw a line of the background */
	for(; column < 32; column++)
	{
		/* Stop vertical scrolling for leftmost eight columns */
		if((vdp.reg[0] & 0x80) && (!locked) && (column >= 24))
		{
			locked = 1;
			v_row = (line & 7) << 3;
			nt = (UINT16 *)&vdp.vram[((vdp.reg[2] << 10) & 0x3800) + ((line >> 3) << 6)];
		}

		/* Get name table attribute word */
		attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
		attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
		/* Expand priority and palette bits */
		atex_mask = atex[(attr >> 11) & 3];

		/* Point to a line of pattern data in cache */
		cache_ptr = (UINT32 *)&bg_pattern_cache[((attr & 0x7FF) << 6) | (v_row)];

		/* Copy the left half, adding the attribute bits in */
		write_dword( &linebuf_ptr[(column << 1)] , read_dword( &cache_ptr[0] ) | (atex_mask));

		/* Copy the right half, adding the attribute bits in */
		write_dword( &linebuf_ptr[(column << 1) | (1)], read_dword( &cache_ptr[1] ) | (atex_mask));
	}

	/* Draw last column (clipped) */
	if(shift)
	{
		INT32 x, c, a;

		UINT8 *p = &linebuf[(0 - shift)+(column << 3)];

		attr = nt[(column + nt_scroll) & 0x1F];

#ifndef LSB_FIRST
		attr = (((attr & 0xFF) << 8) | ((attr & 0xFF00) >> 8));
#endif
		a = (attr >> 7) & 0x30;

		for(x = 0; x < shift; x++)
		{
			c = bg_pattern_cache[((attr & 0x7FF) << 6) | (v_row) | (x)];
			p[x] = ((c) | (a));
		}
	}
}


/* Draw sprites */
void render_obj_sms(INT16 line)
{
	INT32 start, end, xp, yp, n;
	UINT8 sp, bg;
	UINT8 *linebuf_ptr;
	UINT8 *cache_ptr;

	INT32 width = 8;
	INT32 max = (vdp.extended) ? 256 : 224;
	if ((line >= max) && (line < 240)) return;

	/* Adjust dimensions for double size sprites */
	if(vdp.reg[1] & 0x01)
		width *= 2;

	/* Draw sprites in front-to-back order */
	for(INT32 i = 0; i < object_index_count; i++)
	{
		/* Width of sprite */
		start = 0;
		end = width;

		/* Sprite X position */
		xp = object_info[i].xpos;

		/* Sprite Y range */
		yp = object_info[i].yrange;

		/* Pattern name */
		n = object_info[i].attr;

		/* X position shift */
		if(vdp.reg[0] & 0x08) xp -= 8;

		/* Add MSB of pattern name */
		if(vdp.reg[6] & 0x04) n |= 0x0100;

		/* Mask LSB for 8x16 sprites */
		if(vdp.reg[1] & 0x02) n &= 0x01FE;

		/* Point to offset in line buffer */
		linebuf_ptr = (UINT8 *)&linebuf[xp];

		/* Clip sprites on left edge */
		if(xp < 0)
			start = (0 - xp);

		/* Clip sprites on right edge */
		if((xp + width) > 256)
			end = (256 - xp);

		/* Draw double size sprite */
		if(vdp.reg[1] & 0x01)
		{
			/* Retrieve tile data from cached nametable */
			cache_ptr = (UINT8 *)&bg_pattern_cache[(n << 6) | ((yp >> 1) << 3)];

			/* Draw sprite line (at 1/2 dot rate) */
			for(INT32 x = start; x < end; x+=2)
			{
				/* Source pixel from cache */
				sp = cache_ptr[(x >> 1)];

				/* Only draw opaque sprite pixels */
				if(sp)
				{
					/* Background pixel from line buffer */
					bg = linebuf_ptr[x];

					/* Look up result */
					linebuf_ptr[x] = linebuf_ptr[x+1] = lut[(bg << 8) | (sp)];

					/* Check sprite collision */
					if ((bg & 0x40) && !(vdp.status & 0x20))
					{
						/* pixel-accurate SPR_COL flag */
						vdp.status |= 0x20;
						vdp.spr_col = (line << 8) | ((xp + x + 13) >> 1);
					}
				}
			}
		}
		else /* Regular size sprite (8x8 / 8x16) */
		{
			/* Retrieve tile data from cached nametable */
			cache_ptr = (UINT8 *)&bg_pattern_cache[(n << 6) | (yp << 3)];

			/* Draw sprite line */
			for(INT32 x = start; x < end; x++)
			{
				/* Source pixel from cache */
				sp = cache_ptr[x];

				/* Only draw opaque sprite pixels */
				if(sp)
				{
					/* Background pixel from line buffer */
					bg = linebuf_ptr[x];

					/* Look up result */
					linebuf_ptr[x] = lut[(bg << 8) | (sp)];

					/* Check sprite collision */
					if ((bg & 0x40) && !(vdp.status & 0x20))
					{
						/* pixel-accurate SPR_COL flag */
						vdp.status |= 0x20;
						vdp.spr_col = (line << 8) | ((xp + x + 13) >> 1);
					}
				}
			}
		}
	}
}

static void parse_satb(INT16 line)
{
	/* Pointer to sprite attribute table */
	UINT8 *st = (UINT8 *)&vdp.vram[vdp.satb];

	/* Sprite counter (64 max.) */
	INT32 i = 0;

	/* Line counter value */
	INT32 vc = vc_table[sms.display][vdp.extended][line];

	/* Sprite height (8x8 by default) */
	INT32 yp;
	INT32 height = 8;

	/* Adjust height for 8x16 sprites */
	if(vdp.reg[1] & 0x02)
		height <<= 1;

	/* Adjust height for zoomed sprites */
	if(vdp.reg[1] & 0x01)
		height <<= 1;

	/* Sprite count for current line (8 max.) */
	object_index_count = 0;

	for(i = 0; i < 64; i++)
	{
		/* Sprite Y position */
		yp = st[i];

		/* Found end of sprite list marker for non-extended modes? */
		if(vdp.extended == 0 && yp == 208)
			return;

		/* Wrap Y coordinate for sprites > 240 */
		if(yp > 240) yp -= 256;

		/* Compare sprite position with current line counter */
		yp = vc - yp;

		/* Sprite is within vertical range? */
		if((yp >= 0) && (yp < height))
		{
			/* Sprite limit reached? */
			if (object_index_count == 8)
			{
				/* Flag is set only during active area */
				if (line < vdp.height)
					vdp.spr_ovr = 1;

				/* End of sprite parsing */
				if (!vdp.no_spr_limit)
					return;
			}

			/* Store sprite attributes for later processing */
			object_info[object_index_count].yrange = yp;
			object_info[object_index_count].xpos = st[0x80 + (i << 1)];
			object_info[object_index_count].attr = st[0x81 + (i << 1)];

			/* Increment Sprite count for current line */
			++object_index_count;
		}
	}
}

void invalidate_bg_pattern_cache(void)
{
	if (!smsvdp_tmsmode) {
		/* Force full pattern cache update when not in a TMS9918 mode */
		bg_list_index = 0x200;
		for(INT32 i = 0; i < 0x200; i++) {
			bg_name_list[i] = i;
			bg_name_dirty[i] = (UINT8)-1;
		}
	}
}

void update_bg_pattern_cache(void)
{
	INT32 i;
	UINT8 x, y;
	UINT16 name;

	if(!bg_list_index) return;

	for(i = 0; i < bg_list_index; i++)
	{
		name = bg_name_list[i];
		bg_name_list[i] = 0;

		for(y = 0; y < 8; y++)
		{
			if(bg_name_dirty[name] & (1 << y))
			{
				UINT8 *dst = &bg_pattern_cache[name << 6];

				UINT16 *bp01 = (UINT16 *)&vdp.vram[(name << 5) | (y << 2) | (0)];
				UINT16 *bp23 = (UINT16 *)&vdp.vram[(name << 5) | (y << 2) | (2)];
				UINT32 temp = (bp_lut[*bp01] >> 2) | (bp_lut[*bp23]);

				for(x = 0; x < 8; x++)
				{
					UINT8 c = (temp >> (x << 2)) & 0x0F;
					dst[0x00000 | (y << 3) | (x)] = (c);
					dst[0x08000 | (y << 3) | (x ^ 7)] = (c);
					dst[0x10000 | ((y ^ 7) << 3) | (x)] = (c);
					dst[0x18000 | ((y ^ 7) << 3) | (x ^ 7)] = (c);
				}
			}
		}
		bg_name_dirty[name] = 0;
	}
	bg_list_index = 0;
}


/* Update a palette entry */
void palette_sync(INT16 index, INT16 force)
{
	INT16 r, g, b;

	// unless we are forcing an update,
	// if not in mode 4, exit

	if(IS_SMS && ((vdp.reg[0] & 4) == 0) )
	{
		/* Load TMS9918 palette */
		static UINT32 TMS9928A_palette[16] = {
			0x000000, 0x000000, 0x21c842, 0x5edc78, 0x5455ed, 0x7d76fc, 0xd4524d, 0x42ebf5,
			0xfc5554, 0xff7978, 0xd4c154, 0xe6ce80, 0x21b03b, 0xc95bba, 0xcccccc, 0xffffff
		};

		r = (TMS9928A_palette[index & 0x0f] >> 16) & 0xff;
		g = (TMS9928A_palette[index & 0x0f] >> 8) & 0xff;
		b = (TMS9928A_palette[index & 0x0f] >> 0) & 0xff;

		bitmap.pal.color[index][0] = r;
		bitmap.pal.color[index][1] = g;
		bitmap.pal.color[index][2] = b;

		pixel[index] = (r << 16) | (g << 8) | b;

		bitmap.pal.dirty[index] = bitmap.pal.update = 1;

		return;
	}

	if(IS_GG)
	{
		/* ----BBBBGGGGRRRR */
		r = (vdp.cram[(index << 1) | (0)] >> 0) & 0x0F;
		g = (vdp.cram[(index << 1) | (0)] >> 4) & 0x0F;
		b = (vdp.cram[(index << 1) | (1)] >> 0) & 0x0F;

		r = gg_cram_expand_table[r];
		g = gg_cram_expand_table[g];
		b = gg_cram_expand_table[b];
	}
	else
	{
		/* --BBGGRR */
		r = (vdp.cram[index] >> 0) & 3;
		g = (vdp.cram[index] >> 2) & 3;
		b = (vdp.cram[index] >> 4) & 3;

		r = sms_cram_expand_table[r];
		g = sms_cram_expand_table[g];
		b = sms_cram_expand_table[b];
	}

	bitmap.pal.color[index][0] = r;
	bitmap.pal.color[index][1] = g;
	bitmap.pal.color[index][2] = b;

	pixel[index] = (r << 16) | (g << 8) | b;

	bitmap.pal.dirty[index] = bitmap.pal.update = 1;
}

void blit_linebuf(INT16 line, INT16 extend, INT16 offs)
{
	line = line - extend;

	if (!pBurnDraw || line > nScreenHeight || line < 0) return;

	if (line == 0) { // clear screen
		BurnTransferClear(BACKDROP_COLOR);
	}

	// buffer palette for this line
	memcpy(SMSPalette + line * 0x20, pixel, PALETTE_SIZE * sizeof(UINT32));

	UINT16 *p = (UINT16 *)&bitmap.data[((line) * bitmap.pitch)];
	for (INT32 i = bitmap.viewport.x; i < bitmap.viewport.w + bitmap.viewport.x; i++)
	{
		p[i+offs] = (internal_buffer[i] & PIXEL_MASK) | (line * 0x20);
	}
}
