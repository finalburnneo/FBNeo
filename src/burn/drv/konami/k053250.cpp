// license:BSD-3-Clause
// copyright-holders:Olivier Galibert

#include "tiles_generic.h"
#include "konamiic.h"

static INT32 global_offx = 0;
static INT32 global_offy = 0;
static INT32 unpacked_size = 0;
static UINT8* k053250Rom;
static UINT8* k053250RomExp;

UINT16* K053250Ram;
static UINT8 regs[8];
static INT32 page = 0;

static UINT16* buffer[2];
static INT32 frame = -1;

#define ORIENTATION_FLIP_X              0x0001  /* mirror everything in the X direction */
#define ORIENTATION_FLIP_Y              0x0002  /* mirror everything in the Y direction */
#define ORIENTATION_SWAP_XY             0x0004  /* mirror along the top-left/bottom-right diagonal */

void K053250SetOffsets(INT32, int offx, int offy)
{
	global_offx = offx;
	global_offy = offy;
}

static void unpack_nibbles(INT32, UINT8* src, UINT8* dst, INT32 size)
{
	for (INT32 i = 0; i < size; i++)
	{
		dst[2 * i] = src[i] >> 4;
		dst[2 * i + 1] = src[i] & 15;
	}

	unpacked_size = 2 * size;
}

void K053250Init(INT32, UINT8* rom, UINT8* romexp, INT32 size)
{
	KonamiAllocateBitmaps();

	K053250Ram = (UINT16*)BurnMalloc(0x6000);
	buffer[0] = K053250Ram + 0x2000;
	buffer[1] = K053250Ram + 0x2800;

	k053250Rom = rom;
	k053250RomExp = romexp;

	unpack_nibbles(0, rom, romexp, size);

	KonamiIC_K053250InUse = 1;
}

void K053250Reset()
{
	page = 0;
	frame = -1;
	memset(regs, 0, 8);
}

void K053250Exit()
{
	BurnFree(K053250Ram);
}

void K053250Scan(INT32 nAction)
{
	struct BurnArea ba;

	if (nAction & ACB_MEMORY_RAM)
	{
		memset(&ba, 0, sizeof(ba));
		ba.Data = K053250Ram;
		ba.nLen = 0x6000;
		ba.szName = "K053250 Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		SCAN_VAR(regs);

		SCAN_VAR(page);
		SCAN_VAR(frame);
	}
}

// utility function to render a clipped scanline vertically or horizontally
static inline void pdraw_scanline32(UINT32* palette, UINT8* source, int linepos, int scroll, int zoom, UINT32 clipmask,
                                    UINT32 wrapmask, UINT32 orientation, UINT8 pri)
{
	// a sixteen-bit fixed point resolution should be adequate to our application
#define FIXPOINT_PRECISION      16
#define FIXPOINT_PRECISION_HALF (1<<(FIXPOINT_PRECISION-1))

	int end_pixel, flip, dst_min, dst_max, dst_start, dst_length;

	UINT32 src_wrapmask;
	UINT8* src_base;
	int src_fx, src_fdx;
	int pix_data, dst_offset;
	UINT32* pal_base;
	UINT8* pri_base = konami_priority_bitmap;
	UINT32* dst_base = konami_bitmap32;
	int dst_adv;

	// flip X and flip Y also switch role when the X Y coordinates are swapped
	if (!(orientation & ORIENTATION_SWAP_XY))
	{
		flip = orientation & ORIENTATION_FLIP_X;
		dst_min = 0; //cliprect.min_x;
		dst_max = nScreenWidth - 1; //cliprect.max_x;
	}
	else
	{
		flip = orientation & ORIENTATION_FLIP_Y;
		dst_min = 0; //cliprect.min_y;
		dst_max = nScreenHeight - 1; //cliprect.max_y;
	}

	if (clipmask)
	{
		// reject scanlines that are outside of the target bitmap's right(bottom) clip boundary
		dst_start = -scroll;
		if (dst_start > dst_max) return;

		// calculate target length
		dst_length = clipmask + 1;
		if (zoom) dst_length = (dst_length << 6) / zoom;

		// reject scanlines that are outside of the target bitmap's left(top) clip boundary
		end_pixel = dst_start + dst_length - 1;
		if (end_pixel < dst_min) return;

		// clip scanline tail
		if ((end_pixel -= dst_max) > 0) dst_length -= end_pixel;

		// reject 0-length scanlines
		if (dst_length <= 0) return;

		// calculate zoom factor
		src_fdx = zoom << (FIXPOINT_PRECISION - 6);

		// clip scanline head
		end_pixel = dst_min;
		if ((end_pixel -= dst_start) > 0)
		{
			// chop scanline to the correct length and move target start location to the left(top) clip boundary
			dst_length -= end_pixel;
			dst_start = dst_min;

			// and skip the source for the left(top) clip region
			src_fx = end_pixel * src_fdx + FIXPOINT_PRECISION_HALF;
		}
		else
		// the point five bias is to ensure even distribution of stretched or shrinked pixels
			src_fx = FIXPOINT_PRECISION_HALF;

		// adjust flipped source
		if (flip)
		{
			// start from the target's clipped end if the scanline is flipped
			dst_start = dst_max + dst_min - dst_start - (dst_length - 1);

			// and move source start location to the opposite end
			src_fx += (dst_length - 1) * src_fdx - 1;
			src_fdx = -src_fdx;
		}
	}
	else
	{
		// draw wrapped scanline at virtual bitmap boundary when source clipping is off
		dst_start = dst_min;
		dst_length = dst_max - dst_min + 1; // target scanline spans the entire visible area
		src_fdx = zoom << (FIXPOINT_PRECISION - 6);

		// pre-advance source for the clipped region
		if (!flip)
			src_fx = (scroll + dst_min) * src_fdx + FIXPOINT_PRECISION_HALF;
		else
		{
			src_fx = (scroll + dst_max) * src_fdx + FIXPOINT_PRECISION_HALF - 1;
			src_fdx = -src_fdx;
		}
	}

	if (!(orientation & ORIENTATION_SWAP_XY))
	{
		// calculate target increment for horizontal scanlines which is exactly one
		dst_adv = 1;
		dst_offset = dst_length;
		pri_base = konami_priority_bitmap + (linepos * nScreenWidth) + dst_start + dst_offset;
		//&priority.pix8(linepos, dst_start + dst_offset);
		dst_base = konami_bitmap32 + (linepos * nScreenWidth) + dst_start + dst_length;
		//&bitmap.pix32(linepos, dst_start + dst_length);
	}
	else
	{
		// calculate target increment for vertical scanlines which is the bitmap's pitch value
		dst_adv = nScreenWidth;
		dst_offset = dst_length * dst_adv;
		pri_base = konami_priority_bitmap + (dst_start * nScreenWidth) + linepos + dst_offset;
		//&priority.pix8(dst_start, linepos + dst_offset);
		dst_base = konami_bitmap32 + (dst_start * nScreenWidth) + linepos + dst_offset;
		//&bitmap.pix32(dst_start, linepos + dst_offset);
	}

	// generalized
	src_base = source;

	// there is no need to wrap source offsets along with source clipping
	// so we set all bits of the wrapmask to one
	src_wrapmask = (clipmask) ? ~0 : wrapmask;

	pal_base = palette;
	dst_offset = -dst_offset; // negate target offset in order to terminated draw loop at 0 condition

	if (pri)
	{
		// draw scanline and update priority bitmap
		do
		{
			pix_data = src_base[(src_fx >> FIXPOINT_PRECISION) & src_wrapmask];
			src_fx += src_fdx;

			if (pix_data)
			{
				pix_data = pal_base[pix_data];
				pri_base[dst_offset] = pri;
				dst_base[dst_offset] = pix_data;
			}
		}
		while (dst_offset += dst_adv);
	}
	else
	{
		// draw scanline but do not update priority bitmap
		do
		{
			pix_data = src_base[(src_fx >> FIXPOINT_PRECISION) & src_wrapmask];
			src_fx += src_fdx;

			if (pix_data)
			{
				dst_base[dst_offset] = pal_base[pix_data];
			}
		}
		while (dst_offset += dst_adv);
	}

#undef FIXPOINT_PRECISION
#undef FIXPOINT_PRECISION_HALF
}

void K053250Draw(INT32, int colorbase, int /*flags*/, int priority)
{
	UINT8* pix_ptr;
	UINT32 *pal_base, *pal_ptr;
	UINT8* unpacked = k053250RomExp;
	UINT32 src_clipmask, src_wrapmask, dst_wrapmask;
	int linedata_offs, line_pos, line_start, line_end, scroll_corr;
	int color, offset, zoom, scroll, passes, i;
	bool wrap500 = false;

	UINT16* line_ram = buffer[page]; // pointer to physical line RAM
	int map_scrollx = static_cast<short>(regs[0] << 8 | regs[1]) - global_offx; // signed horizontal scroll value
	int map_scrolly = static_cast<short>(regs[2] << 8 | regs[3]) - global_offy; // signed vertical scroll value
	UINT8 ctrl = regs[4]; // register four is the main control register

	// copy visible boundary values to more accessible locations
	int dst_minx = 0; //cliprect.min_x;
	int dst_maxx = (nScreenWidth - 1); //cliprect.max_x;
	int dst_miny = 0; //cliprect.min_y;
	int dst_maxy = (nScreenHeight - 1); //cliprect.max_y;

	int orientation = 0; // orientation defaults to no swapping and no flipping
	int dst_height = 512; // virtual bitmap height defaults to 512 pixels
	int linedata_adv = 4; // line info packets are four words(eight bytes) apart

	// switch X and Y parameters when the first bit of the control register is cleared
	if (!(ctrl & 0x01)) orientation |= ORIENTATION_SWAP_XY;

	// invert X parameters when the forth bit of the control register is set
	if (ctrl & 0x08) orientation |= ORIENTATION_FLIP_X;

	// invert Y parameters when the fifth bit of the control register is set
	if (ctrl & 0x10) orientation |= ORIENTATION_FLIP_Y;

	switch (ctrl >> 5) // the upper four bits of the control register select source and target dimensions
	{
	case 0:
		// Xexex: L6 galaxies
		// Metam: L4 forest, L5 arena, L6 tower interior, final boss

		// crop source offset between 0 and 255 inclusive,
		// and set virtual bitmap height to 256 pixels
		src_wrapmask = src_clipmask = 0xff;
		dst_height = 0x100;
		break;
	case 1:
		// Xexex: prologue, L7 nebulae

		// the source offset is cropped to 0 and 511 inclusive
		src_wrapmask = src_clipmask = 0x1ff;
		break;
	case 4:
		// Xexex: L1 sky and boss, L3 planet, L5 poly-face, L7 battle ship patches
		// Metam: L1 summoning circle, L3 caves, L6 gargoyle towers

		// crop source offset between 0 and 255 inclusive,
		// and allow source offset to wrap back at 0x500 to -0x300
		src_wrapmask = src_clipmask = 0xff;
		wrap500 = true;
		break;
	//     	 	case 2 : // Xexex: title
	//      	case 7 : // Xexex: L4 organic stage
	default:
		// crop source offset between 0 and 1023 inclusive,
		// keep other dimensions to their defaults
		src_wrapmask = src_clipmask = 0x3ff;
		break;
	}

	// disable source clipping when the third bit of the control register is set
	if (ctrl & 0x04) src_clipmask = 0;

	if (!(orientation & ORIENTATION_SWAP_XY)) // normal orientaion with no X Y switching
	{
		line_start = dst_miny; // the first scanline starts at the minimum Y clip location
		line_end = dst_maxy; // the last scanline ends at the maximum Y clip location
		scroll_corr = map_scrollx; // concentrate global X scroll
		linedata_offs = map_scrolly; // determine where to get info for the first line

		if (orientation & ORIENTATION_FLIP_X)
		{
			scroll_corr = -scroll_corr; // X scroll adjustment should be negated in X flipped scenarioes
		}

		if (orientation & ORIENTATION_FLIP_Y)
		{
			linedata_adv = -linedata_adv; // traverse line RAM backward in Y flipped scenarioes
			linedata_offs += nScreenHeight - 1; // and get info for the first line from the bottom
		}

		dst_wrapmask = ~0; // scanlines don't seem to wrap horizontally in normal orientation
		passes = 1; // draw scanline in a single pass
	}
	else // orientaion with X and Y parameters switched
	{
		line_start = dst_minx; // the first scanline starts at the minimum X clip location
		line_end = dst_maxx; // the last scanline ends at the maximum X clip location
		scroll_corr = map_scrolly; // concentrate global Y scroll
		linedata_offs = map_scrollx; // determine where to get info for the first line

		if (orientation & ORIENTATION_FLIP_Y)
		{
			scroll_corr = 0x100 - scroll_corr; // apply common vertical correction

			// Y correction (ref: 1st and 5th boss)
			scroll_corr -= 2; // apply unique vertical correction

			// X correction (ref: 1st boss, seems to undo non-rotated global X offset)
			linedata_offs -= 5; // apply unique horizontal correction
		}

		if (orientation & ORIENTATION_FLIP_X)
		{
			linedata_adv = -linedata_adv; // traverse line RAM backward in X flipped scenarioes
			linedata_offs += nScreenWidth - 1; // and get info for the first line from the bottom
		}

		if (src_clipmask)
		{
			// determine target wrap boundary and draw scanline in two passes if the source is clipped
			dst_wrapmask = dst_height - 1;
			passes = 2;
		}
		else
		{
			// otherwise disable target wraparound and draw scanline in a single pass
			dst_wrapmask = ~0;
			passes = 1;
		}
	}

	linedata_offs *= 4; // each line info packet has four words(eight bytes)
	linedata_offs &= 0x7ff; // and it should wrap at the four-kilobyte boundary
	linedata_offs += line_start * linedata_adv; // pre-advance line info offset for the clipped region

	// load physical palette base
	pal_base = konami_palette32 + (colorbase << 4);

	// walk the target bitmap within the visible area vertically or horizontally, one line at a time
	for (line_pos = line_start; line_pos <= line_end; linedata_offs += linedata_adv, line_pos++)
	{
		linedata_offs &= 0x7ff; // line info data wraps at the four-kilobyte boundary

		color = BURN_ENDIAN_SWAP_INT16(line_ram[linedata_offs]); // get scanline color code
		if (color == 0xffff) continue; // reject scanline if color code equals minus one

		offset = BURN_ENDIAN_SWAP_INT16(line_ram[linedata_offs + 1]); // get first pixel offset in ROM
		if (!(color & 0xff) && !offset) continue; // reject scanline if both color and pixel offset are 0

		// calculate physical palette location
		// there can be thirty-two color codes and each code represents sixteen pens
		pal_ptr = pal_base + ((color & 0x1f) << 4);

		// calculate physical pixel location
		// each offset unit represents 256 pixels and should wrap at ROM boundary for safety
		pix_ptr = unpacked + ((offset << 8) % unpacked_size);

		// get scanline zoom factor
		// For example, 0x20 doubles the length, 0x40 maintains a one-to-one length,
		// and 0x80 halves the length. The zoom center is at the beginning of the
		// scanline therefore it is not necessary to adjust render start position
		zoom = BURN_ENDIAN_SWAP_INT16(line_ram[linedata_offs + 2]);

		scroll = static_cast<short>(line_ram[linedata_offs + 3]);
		// get signed local scroll value for the current scanline

		// scavenged from old code; improves Xexex' first level sky
		if (wrap500 && scroll >= 0x500) scroll -= 0x800;

		scroll += scroll_corr; // apply final scroll correction
		scroll &= dst_wrapmask; // wraparound scroll value if necessary

		// draw scanlines wrapped at virtual bitmap boundary in two passes
		// this should not impose too much overhead due to clipping performed by the render code
		i = passes;
		do
		{
			/*
			    Parameter descriptions:

			    bitmap       : pointer to a MAME bitmap as the render target
			    pal_ptr      : pointer to the palette's physical location relative to the scanline
			    pix_ptr      : pointer to the physical start location of source pixels in ROM
			    cliprect     : pointer to a rectangle structue which describes the visible area of the target bitmap
			    line_pos     : scanline render position relative to the target bitmap
			                   should be a Y offset to the target bitmap in normal orientaion,
			                   or an X offset to the target bitmap if X,Y are swapped
			    scroll       : source scroll value of the scanline
			    zoom         : source zoom factor of the scanline
			    src_clipmask : source offset clip mask; source pixels with offsets beyond the scope of this mask will not be drawn
			    src_wrapmask : source offset wrap mask; wraps source offset around, no effect when src_clipmask is set
			    orientation  : flags indicating whether scanlines should be drawn horizontally, vertically, forward or backward
			    priority     : value to be written to the priority bitmap, no effect when equals 0
			*/
			pdraw_scanline32(pal_ptr, pix_ptr, line_pos, scroll, zoom, src_clipmask, src_wrapmask, orientation,
			                 static_cast<UINT8>(priority));

			// shift scanline position one virtual screen upward to render the wrapped end if necessary
			scroll -= dst_height;
		}
		while (--i);
	}
}

static void k053250_dma(INT32, int limiter)
{
	int current_frame = nCurrentFrame;

	if (limiter && current_frame == frame)
		return; // make sure we only do DMA transfer once per frame

	frame = current_frame;
	memcpy(buffer[page], K053250Ram, 0x1000);
	page ^= 1;
}

UINT16 K053250RegRead(INT32, INT32 offset)
{
	return regs[(offset / 2) & 7];
}

void K053250RegWrite(INT32, INT32 offset, UINT8 data)
{
	if (offset & 1)
	{
		offset = (offset / 2) & 7;

		// start LVC DMA transfer at the falling edge of control register's bit1
		if (offset == 4 && !(data & 2) && (regs[4] & 2))
			k053250_dma(0, 1);

		regs[offset] = data;
	}
}

UINT16 K053250RomRead(INT32, INT32 offset)
{
	offset = (offset / 2) & 0xfff;

	return k053250Rom[0x80000 * regs[6] + 0x800 * regs[7] + offset / 2];
}
