// hq2x / hq3x / hq4x filters - shared code
//----------------------------------------------------------
// Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//----------------------------------------------------------
//
// Portable C++ version of the old 32-bit MMX assembler (hq2x32.asm,
// hq3x32.asm, hq4x32.asm).  The pattern tables were translated directly
// from the assembler, and the 15/16-bit -> 32-bit output matches it exactly.
//
// Differences from the assembler:
//  - Output is in the same format as the input (15->15, 16->16, 32->32);
//    the assembler always wrote 32-bit pixels, even to 15/16-bit surfaces.
//  - 32-bit input is supported.
//  - The source pitch is a parameter (the assembler assumed width * 2).
//  - 1-line images no longer read past the end of the source.

#ifndef HQX_H
#define HQX_H

typedef unsigned char  hq_u8;
typedef unsigned short hq_u16;
typedef unsigned int   hq_u32;

extern hq_u32 hqLUT16to32[65536];	// 15/16-bit pixel -> 0x00RRGGBB
extern hq_u32 hqRGBtoYUV[65536];	// 15/16-bit pixel -> 0x00YYUUVV

// Weighted per-channel blends of 0x00RRGGBB colours: (a*wa + b*wb [+ c*wc]) >> s,
// where the weights add up to 1 << s (at most 16)
static inline hq_u32 Mix2(hq_u32 a, hq_u32 wa, hq_u32 b, hq_u32 wb, int s)
{
	hq_u32 rb = (((a & 0xFF00FF) * wa + (b & 0xFF00FF) * wb) >> s) & 0xFF00FF;
	hq_u32 g  = (((a & 0x00FF00) * wa + (b & 0x00FF00) * wb) >> s) & 0x00FF00;

	return rb | g;
}

static inline hq_u32 Mix3(hq_u32 a, hq_u32 wa, hq_u32 b, hq_u32 wb, hq_u32 c, hq_u32 wc, int s)
{
	hq_u32 rb = (((a & 0xFF00FF) * wa + (b & 0xFF00FF) * wb + (c & 0xFF00FF) * wc) >> s) & 0xFF00FF;
	hq_u32 g  = (((a & 0x00FF00) * wa + (b & 0x00FF00) * wb + (c & 0x00FF00) * wc) >> s) & 0x00FF00;

	return rb | g;
}

static inline hq_u32 HqYUV(hq_u32 r, hq_u32 g, hq_u32 b)
{
	int Y = (int)(r + g + b) >> 2;
	int u = 128 + (((int)r - (int)b) >> 2);
	int v = 128 + ((-(int)r + 2 * (int)g - (int)b) >> 3);

	return (Y << 16) + (u << 8) + v;
}

// Are two pixels "different"?  (w = raw pixel, y = its YUV)
static inline bool HqDiff(hq_u32 wa, hq_u32 wb, hq_u32 ya, hq_u32 yb)
{
	if (wa == wb) return false;

	int dY = (int)((ya >> 16) & 0xFF) - (int)((yb >> 16) & 0xFF);
	int dU = (int)((ya >>  8) & 0xFF) - (int)((yb >>  8) & 0xFF);
	int dV = (int)( ya        & 0xFF) - (int)( yb        & 0xFF);

	return (dY > 0x30 || dY < -0x30) || (dU > 0x07 || dU < -0x07) || (dV > 0x06 || dV < -0x06);
}

// Used by the pattern tables: neighbours are numbered
//   1 2 3
//   4 5 6
//   7 8 9
#define Diff(a, b) HqDiff(w[a], w[b], y[a], y[b])

typedef void (*HqPatternFn)(int pattern, const hq_u32* w, const hq_u32* y,
	hq_u32 c1, hq_u32 c2, hq_u32 c3, hq_u32 c4, hq_u32 c5, hq_u32 c6, hq_u32 c7, hq_u32 c8, hq_u32 c9, hq_u32* o);

// Each source pixel becomes an N x N block.  InDepth: 15, 16 or 32; OutDepth: 15, 16 or 32.
// Source reads are clamped to the image, as in the original.
template <int N, int InDepth, int OutDepth>
static void HqRun(HqPatternFn Pattern, const hq_u8* src, hq_u32 srcPitch, hq_u8* dst, hq_u32 dstPitch, int width, int height)
{
	if (width <= 0 || height <= 0) return;

	for (int yy = 0; yy < height; yy++) {
		const hq_u8* rows[3] = {
			src + (yy > 0 ? yy - 1 : 0) * srcPitch,
			src + yy * srcPitch,
			src + (yy + 1 < height ? yy + 1 : height - 1) * srcPitch
		};

		hq_u8* d = dst + yy * N * dstPitch;

		for (int xx = 0; xx < width; xx++) {
			const int cols[3] = { (xx > 0) ? xx - 1 : 0, xx, (xx + 1 < width) ? xx + 1 : width - 1 };

			hq_u32 w[10], y[10], c[10];

			for (int j = 0; j < 3; j++) {
				for (int i = 0; i < 3; i++) {
					const int k = 1 + j * 3 + i;

					if (InDepth == 32) {
						hq_u32 p = ((const hq_u32*)rows[j])[cols[i]] & 0xFFFFFF;
						w[k] = p;
						c[k] = p;
						y[k] = HqYUV(p >> 16, (p >> 8) & 0xFF, p & 0xFF);
					} else {
						hq_u32 p = ((const hq_u16*)rows[j])[cols[i]];
						w[k] = p;
						c[k] = hqLUT16to32[p];
						y[k] = hqRGBtoYUV[p];
					}
				}
			}

			int pattern = 0;
			if (HqDiff(w[5], w[1], y[5], y[1])) pattern |= 0x01;
			if (HqDiff(w[5], w[2], y[5], y[2])) pattern |= 0x02;
			if (HqDiff(w[5], w[3], y[5], y[3])) pattern |= 0x04;
			if (HqDiff(w[5], w[4], y[5], y[4])) pattern |= 0x08;
			if (HqDiff(w[5], w[6], y[5], y[6])) pattern |= 0x10;
			if (HqDiff(w[5], w[7], y[5], y[7])) pattern |= 0x20;
			if (HqDiff(w[5], w[8], y[5], y[8])) pattern |= 0x40;
			if (HqDiff(w[5], w[9], y[5], y[9])) pattern |= 0x80;

			hq_u32 o[N * N];
			Pattern(pattern, w, y, c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], o);

			for (int j = 0; j < N; j++) {
				hq_u8* line = d + j * dstPitch;

				for (int i = 0; i < N; i++) {
					hq_u32 p = o[j * N + i];

					if (OutDepth == 32) {
						((hq_u32*)line)[xx * N + i] = p;
					} else if (OutDepth == 16) {
						((hq_u16*)line)[xx * N + i] = (hq_u16)(((p >> 8) & 0xF800) | ((p >> 5) & 0x07E0) | ((p >> 3) & 0x001F));
					} else {
						((hq_u16*)line)[xx * N + i] = (hq_u16)(((p >> 9) & 0x7C00) | ((p >> 6) & 0x03E0) | ((p >> 3) & 0x001F));
					}
				}
			}
		}
	}
}

// Called by the filters: input and output are both in the depth given to hqx_init()
extern int nHqDepth;

template <int N>
static void HqRunDepth(HqPatternFn Pattern, const hq_u8* src, hq_u32 srcPitch, hq_u8* dst, hq_u32 dstPitch, int width, int height)
{
	switch (nHqDepth) {
		case 15: HqRun<N, 15, 15>(Pattern, src, srcPitch, dst, dstPitch, width, height); break;
		case 16: HqRun<N, 16, 16>(Pattern, src, srcPitch, dst, dstPitch, width, height); break;
		case 32: HqRun<N, 32, 32>(Pattern, src, srcPitch, dst, dstPitch, width, height); break;
	}
}

#endif
