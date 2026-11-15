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

// C only.  (The old MMX/x86 __asm versions were never compiled - USE_C_CODE
// was always defined - and they can't be built for x64 anyway.)

#include "hq_shared32.h"
#include "interp.h"

// 32-bit (0x00RRGGBB) blends, per channel.  The old Interp1/2/5 blended the
// whole 32-bit word at once, so each channel's overflow leaked into the next
// one up (e.g. bright blue tinted green).

void Interp1(unsigned char * pc, unsigned int c1, unsigned int c2)
{
	*((unsigned int*)pc) = ((((c1 & 0x00FF00)*3 + (c2 & 0x00FF00)) & 0x0003FC00) +
	                        (((c1 & 0xFF00FF)*3 + (c2 & 0xFF00FF)) & 0x03FC03FC)) >> 2;
}

void Interp2(unsigned char * pc, unsigned int c1, unsigned int c2, unsigned int c3)
{
	*((unsigned int*)pc) = ((((c1 & 0x00FF00)*2 + (c2 & 0x00FF00) + (c3 & 0x00FF00)) & 0x0003FC00) +
	                        (((c1 & 0xFF00FF)*2 + (c2 & 0xFF00FF) + (c3 & 0xFF00FF)) & 0x03FC03FC)) >> 2;
}

void Interp3(unsigned char * pc, unsigned int c1, unsigned int c2)
{
	*((unsigned int*)pc) = ((((c1 & 0x00FF00)*7 + (c2 & 0x00FF00)) & 0x0007F800) +
	                        (((c1 & 0xFF00FF)*7 + (c2 & 0xFF00FF)) & 0x07F807F8)) >> 3;
}

void Interp4(unsigned char * pc, unsigned int c1, unsigned int c2, unsigned int c3)
{
	*((unsigned int*)pc) = ((((c1 & 0x00FF00)*2 + ((c2 & 0x00FF00) + (c3 & 0x00FF00))*7) & 0x000FF000) +
	                        (((c1 & 0xFF00FF)*2 + ((c2 & 0xFF00FF) + (c3 & 0xFF00FF))*7) & 0x0FF00FF0)) >> 4;
}

void Interp5(unsigned char * pc, unsigned int c1, unsigned int c2)
{
	*((unsigned int*)pc) = ((((c1 & 0x00FF00) + (c2 & 0x00FF00)) & 0x0001FE00) +
	                        (((c1 & 0xFF00FF) + (c2 & 0xFF00FF)) & 0x01FE01FE)) >> 1;
}

// 15/16-bit blends (masks set by hq2xS_init())

void Interp1_16(unsigned char * pc, unsigned short c1, unsigned short c2)
{
	*((unsigned short*)pc) = interp_16_31(c1, c2);
}

void Interp2_16(unsigned char * pc, unsigned short c1, unsigned short c2, unsigned short c3)
{
	*((unsigned short*)pc) = interp_16_211(c1, c2, c3);
}

void Interp3_16(unsigned char * pc, unsigned short c1, unsigned short c2)
{
	*((unsigned short*)pc) = interp_16_71(c1, c2);
}

void Interp4_16(unsigned char * pc, unsigned short c1, unsigned short c2, unsigned short c3)
{
	*((unsigned short*)pc) = interp_16_772(c2, c3, c1);
}

void Interp5_16(unsigned char * pc, unsigned short c1, unsigned short c2)
{
	*((unsigned short*)pc) = interp_16_11(c1, c2);
}

static inline int AbsDiff(int a, int b)
{
	return (a > b) ? (a - b) : (b - a);
}

bool Diff(unsigned int c1, unsigned int c2)
{
	unsigned int YUV1 = RGBtoYUV(c1);
	unsigned int YUV2 = RGBtoYUV(c2);

	if (YUV1 == YUV2) return false; // Save some processing power

	return
		( AbsDiff(YUV1 & Ymask, YUV2 & Ymask) > trY ) ||
		( AbsDiff(YUV1 & Umask, YUV2 & Umask) > trU ) ||
		( AbsDiff(YUV1 & Vmask, YUV2 & Vmask) > trV );
}

unsigned int RGBtoYUV(unsigned int c)
{
	int r = (c & 0x000000FF);
	int g = (c & 0x0000FF00) >> 8;
	int b = (c & 0x00FF0000) >> 16;

	unsigned char Y = (r + g + b) >> 2;
	unsigned char u = 128 + ((r - b) >> 2);
	unsigned char v = 128 + ((-r + 2*g - b) >> 3);

	return (Y << 16) + (u << 8) + v;
}
