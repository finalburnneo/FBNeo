// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "hq_shared32.h"
#include "interp.h"

#define SIZE_PIXEL 2 // 16bit = 2 bytes
#define PIXELTYPE unsigned short
#define Interp1 Interp1_16
#define Interp2 Interp2_16
#define Interp3 Interp3_16
#define Interp4 Interp4_16
#define Interp5 Interp5_16

void hq3xS(unsigned char * pIn,  unsigned int srcPitch,
			unsigned char *,
			unsigned char * pOut, unsigned int dstPitch,
			int Xres, int Yres)
{
	int i, j;
	PIXELTYPE c[10];

	// +----+----+----+
	// |    |    |    |
	// | c1 | c2 | c3 |
	// +----+----+----+
	// |    |    |    |
	// | c4 | c5 | c6 |
	// +----+----+----+
	// |    |    |    |
	// | c7 | c8 | c9 |
	// +----+----+----+

	for (j=0; j<Yres; j++)
	{
		// clamp reads to the image
		const int up   = (j > 0)        ? -(int)srcPitch : 0;
		const int down = (j < Yres - 1) ?  (int)srcPitch : 0;

		for (i=0; i<Xres; i++)
		{
			const int left  = (i > 0)        ? -SIZE_PIXEL : 0;
			const int right = (i < Xres - 1) ?  SIZE_PIXEL : 0;

			c[2] = *((PIXELTYPE*)(pIn + up           ));
			c[5] = *((PIXELTYPE*)(pIn                ));
			c[8] = *((PIXELTYPE*)(pIn + down         ));

			c[1] = *((PIXELTYPE*)(pIn + up   + left  ));
			c[4] = *((PIXELTYPE*)(pIn        + left  ));
			c[7] = *((PIXELTYPE*)(pIn + down + left  ));

			c[3] = *((PIXELTYPE*)(pIn + up   + right ));
			c[6] = *((PIXELTYPE*)(pIn        + right ));
			c[9] = *((PIXELTYPE*)(pIn + down + right ));


			int pattern = 0;

			// hq3xS dynamic edge detection:
			// simply comparing the center color against its surroundings will give bad results in many cases,
			// so, instead, compare the center color relative to the max difference in brightness of this 3x3 block
			int brightArray[10];
			int maxBright = 0, minBright = 999999;
			for(int j = 1 ; j < 10 ; j++)
			{
				int r,g,b;
				if (interp_bits_per_pixel == 16) {
					b = (int)((c[j] & 0x1F)) << 3;
					g = (int)((c[j] & 0x7E0)) >> 3;
					r = (int)((c[j] & 0xF800)) >> 8;
				} else {
					b = (int)((c[j] & 0x1F)) << 3;
					g = (int)((c[j] & 0x3E0)) >> 2;
					r = (int)((c[j] & 0x7C00)) >> 7;
				}
				const int bright = r+r+r + g+g+g + b+b;
				if(bright > maxBright) maxBright = bright;
				if(bright < minBright) minBright = bright;

				brightArray[j] = bright;
			}
			const int diffBright = ((maxBright - minBright) * 7) >> 4;
			if(diffBright > 7)
			{
				#define ABS(x) ((x) < 0 ? -(x) : (x))

				const int centerBright = brightArray[5];
				if(ABS(brightArray[1] - centerBright) > diffBright)
					pattern |= 1 << 0;
				if(ABS(brightArray[2] - centerBright) > diffBright)
					pattern |= 1 << 1;
				if(ABS(brightArray[3] - centerBright) > diffBright)
					pattern |= 1 << 2;
				if(ABS(brightArray[4] - centerBright) > diffBright)
					pattern |= 1 << 3;
				if(ABS(brightArray[6] - centerBright) > diffBright)
					pattern |= 1 << 4;
				if(ABS(brightArray[7] - centerBright) > diffBright)
					pattern |= 1 << 5;
				if(ABS(brightArray[8] - centerBright) > diffBright)
					pattern |= 1 << 6;
				if(ABS(brightArray[9] - centerBright) > diffBright)
					pattern |= 1 << 7;
			}

#define Diff(x,y) false//(ABS((x) - (y)) > diffBright)
#undef cget
#define cget(x) brightArray[x]
#include "hq3xs.h"
#undef cget
#undef Diff
			pIn+=SIZE_PIXEL;
			pOut+=3<<1;
		}
		pIn+=srcPitch-(Xres<<1);
		pOut+=dstPitch-(3*Xres<<1);
		pOut+=dstPitch<<1;
		//	pIn+=SIZE_PIXEL;
		//	pOut+=3*SIZE_PIXEL;
		//}
		//pIn+=srcPitch-(4*Xres);
		//pOut+=dstPitch-(3*Xres*SIZE_PIXEL);
		//pOut+=2*dstPitch;
	}
}

#undef Interp1
#undef Interp2
#undef Interp3
#undef Interp4
#undef Interp5
#undef SIZE_PIXEL
#undef PIXELTYPE
#define SIZE_PIXEL 4 // 32bit = 4 bytes
#define PIXELTYPE unsigned int

void hq3xS32(unsigned char * pIn,  unsigned int srcPitch,
			unsigned char *,
			unsigned char * pOut, unsigned int dstPitch,
			int Xres, int Yres)
{
	int i, j;
	PIXELTYPE c[10];

	// +----+----+----+
	// |    |    |    |
	// | c1 | c2 | c3 |
	// +----+----+----+
	// |    |    |    |
	// | c4 | c5 | c6 |
	// +----+----+----+
	// |    |    |    |
	// | c7 | c8 | c9 |
	// +----+----+----+

	for (j=0; j<Yres; j++)
	{
		// clamp reads to the image
		const int up   = (j > 0)        ? -(int)srcPitch : 0;
		const int down = (j < Yres - 1) ?  (int)srcPitch : 0;

		for (i=0; i<Xres; i++)
		{
			const int left  = (i > 0)        ? -SIZE_PIXEL : 0;
			const int right = (i < Xres - 1) ?  SIZE_PIXEL : 0;

			c[2] = *((PIXELTYPE*)(pIn + up           ));
			c[5] = *((PIXELTYPE*)(pIn                ));
			c[8] = *((PIXELTYPE*)(pIn + down         ));

			c[1] = *((PIXELTYPE*)(pIn + up   + left  ));
			c[4] = *((PIXELTYPE*)(pIn        + left  ));
			c[7] = *((PIXELTYPE*)(pIn + down + left  ));

			c[3] = *((PIXELTYPE*)(pIn + up   + right ));
			c[6] = *((PIXELTYPE*)(pIn        + right ));
			c[9] = *((PIXELTYPE*)(pIn + down + right ));

			int pattern = 0;

			// hq3xS dynamic edge detection:
			// simply comparing the center color against its surroundings will give bad results in many cases,
			// so, instead, compare the center color relative to the max difference in brightness of this 3x3 block
			int brightArray[10];
			int maxBright = 0, minBright = 999999;
			for(int j = 1 ; j < 10 ; j++)
			{
				const int b = (int)((c[j] & 0xF8));
				const int g = (int)((c[j] & 0xF800)) >> 8;
				const int r = (int)((c[j] & 0xF80000)) >> 16;
				const int bright = r+r+r + g+g+g + b+b;
				if(bright > maxBright) maxBright = bright;
				if(bright < minBright) minBright = bright;

				brightArray[j] = bright;
			}
			int diffBright = ((maxBright - minBright) * 7) >> 4;
			if(diffBright > 7)
			{
				#define ABS(x) ((x) < 0 ? -(x) : (x))

				const int centerBright = brightArray[5];
				if(ABS(brightArray[1] - centerBright) > diffBright)
					pattern |= 1 << 0;
				if(ABS(brightArray[2] - centerBright) > diffBright)
					pattern |= 1 << 1;
				if(ABS(brightArray[3] - centerBright) > diffBright)
					pattern |= 1 << 2;
				if(ABS(brightArray[4] - centerBright) > diffBright)
					pattern |= 1 << 3;
				if(ABS(brightArray[6] - centerBright) > diffBright)
					pattern |= 1 << 4;
				if(ABS(brightArray[7] - centerBright) > diffBright)
					pattern |= 1 << 5;
				if(ABS(brightArray[8] - centerBright) > diffBright)
					pattern |= 1 << 6;
				if(ABS(brightArray[9] - centerBright) > diffBright)
					pattern |= 1 << 7;
			}

#define Diff(x,y) false//(ABS((x) - (y)) > diffBright)
#undef cget
#define cget(x) brightArray[x]
#include "hq3xs.h"
#undef cget
#undef Diff
			pIn+=SIZE_PIXEL;
			pOut+=3<<2;
		}
		pIn+=srcPitch-(Xres<<2);
		pOut+=dstPitch-(3*Xres<<2);
		pOut+=dstPitch<<1;
		//	pIn+=SIZE_PIXEL;
		//	pOut+=3*SIZE_PIXEL;
		//}
		//pIn+=srcPitch-(4*Xres);
		//pOut+=dstPitch-(3*Xres*SIZE_PIXEL);
		//pOut+=2*dstPitch;
	}
}
