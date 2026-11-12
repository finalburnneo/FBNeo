// hq2x / hq3x / hq4x filters - lookup tables
// Copyright (C) 2003 MaxSt ( maxst@hiend3d.com ), GPL v2 or later - see hqx.h

#include "hqx.h"

hq_u32 hqLUT16to32[65536];
hq_u32 hqRGBtoYUV[65536];
int nHqDepth = 0;

// nDepth: 15, 16 or 32 (input and output depth)
void hqx_init(int nDepth)
{
	nHqDepth = nDepth;

	if (nDepth == 15) {
		for (int i = 0; i < 32768; i++) {
			hq_u32 r = (i & 0x7C00) >> 7, g = (i & 0x03E0) >> 2, b = (i & 0x001F) << 3;
			hqLUT16to32[i] = (r << 16) + (g << 8) + b;
			hqRGBtoYUV[i]  = HqYUV(r, g, b);
		}
	}

	if (nDepth == 16) {
		for (int i = 0; i < 65536; i++) {
			hq_u32 r = (i & 0xF800) >> 8, g = (i & 0x07E0) >> 3, b = (i & 0x001F) << 3;
			hqLUT16to32[i] = (r << 16) + (g << 8) + b;
			hqRGBtoYUV[i]  = HqYUV(r, g, b);
		}
	}

	// 32-bit doesn't use the tables
}
