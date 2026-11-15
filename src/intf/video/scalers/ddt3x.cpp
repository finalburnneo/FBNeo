/*
   Hyllian's Data Dependent Triangulation 3x
   
   Copyright (C) 2011, 2012 Hyllian/Jararaca - sergiogdb@gmail.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdlib.h>

// Changes:
//  - The first line read the line two below as its "below" neighbour (a byte
//    pitch was added to a pixel pointer), which read outside the image when
//    the height was 2 or less.
//  - Sizes and pitches were held in 16-bit variables.
//  - 15-bit images are supported (was 16-bit only).

// The blend spreads a pixel's channels out into a 32-bit value
// (green in the top half, red and blue in the bottom half) so that all three
// can be blended at once
#define RGB_MASK565 0x07E0F81F
#define RGB_MASK555 0x03E07C1F

static unsigned int RGB_MASK = RGB_MASK565;


#define ALPHA_BLEND_X_W(dst, src, alpha) \
    ts = src; td = dst;\
    td = ((td|(td<<16)) & RGB_MASK); ts = ((ts|(ts<<16)) & RGB_MASK);\
	td = ((( ( (ts-td)*alpha ) >> 5 ) + td ) & RGB_MASK); \
	dst= (td|(td>>16));\


#define BIL3X(PF, PH, PI, N5, N7, N8) \
       ALPHA_BLEND_X_W(E[N5], PF, 5); \
       ALPHA_BLEND_X_W(E[N7], PH, 5); \
       ALPHA_BLEND_X_W(E[N8], PF, 9); \
       ALPHA_BLEND_X_W(E[N8], PH, 7); \
       ALPHA_BLEND_X_W(E[N8], PI, 3); \


#define DDT3X_BC(PF, PH, PI, N5, N7, N8) \
       ALPHA_BLEND_X_W(E[N5], PF, 6); \
       ALPHA_BLEND_X_W(E[N7], PH, 6); \
       ALPHA_BLEND_X_W(E[N8], PI, 12); \


#define DDT3X_D(PF, PH, N5, N7, N8) \
       ALPHA_BLEND_X_W(E[N5], PF, 6); \
       ALPHA_BLEND_X_W(E[N7], PH, 6); \
       ALPHA_BLEND_X_W(E[N8], PF, 12); \
       ALPHA_BLEND_X_W(E[N8], PH, 12); \


#define FILTRO(PE, PI, PH, PF, PG, PC, PD, PB, PA, N0, N1, N2, N3, N4, N5, N6, N7, N8) \
{\
  if (PE!=PF || PE!=PH || PE!=PI) \
  {\
    ad = abs(PE-PI); bc = abs(PF-PH);\
	if (ad < bc)\
	{\
        DDT3X_BC(PF, PH, PI, N5, N7, N8);\
	}\
	else if (ad > bc)\
	{\
        DDT3X_D(PF, PH, N5, N7, N8);\
	}\
	else\
	{\
        BIL3X(PF, PH, PI, N5, N7, N8) \
    }\
 }\
}\

        
// nDepth: 15 or 16
void ddt3x(unsigned char * src,  unsigned int srcPitch,
			unsigned char * dest, unsigned int dstPitch,
			int Xres, int Yres, int nDepth)
{	
	unsigned int x, y;
	unsigned short int PA, PB, PC, PD, PE, PF, PG, PH, PI;
	unsigned short int *sa1, *sa2, *sa3; // sa = start_address
	unsigned int nl, nl_src; // nl = new_line
	unsigned int nl1;
	unsigned short int *E;  // E = dst_pixel
	unsigned int src_width = (unsigned int)Xres;
	unsigned int src_height = (unsigned int)Yres;
	unsigned int dst_width = src_width * 3;
	unsigned int complete_line_src, complete_line_dst;
	unsigned int src_pitch = srcPitch;
	unsigned char pprev;
    unsigned int ad, bc;
    unsigned int td, ts;

	if (Xres <= 0 || Yres <= 0) return;

	RGB_MASK = (nDepth == 15) ? RGB_MASK555 : RGB_MASK565;
	
	nl_src = src_pitch >> 1;
	nl = dstPitch >> 1;
	nl1= dstPitch;

	// fixed by Steve Snake
	complete_line_src = (src_pitch>>1) - src_width;
	complete_line_dst = ((dstPitch*3)>>1) - dst_width;
	
	sa2 = (unsigned short int *)(src - 4);
    sa1 = sa2;
	sa3 = (src_height > 1) ? sa2 + nl_src : sa2;	// (was "+ src_pitch": bytes, not pixels)
	
	E = (unsigned short int *)(dest);	
	
	y = src_height;

	while(y--)
	{	
        if (!y) sa3 = sa2;
        pprev = 2;
        x = src_width;
		
		while(x--) 
		{			
            PB = sa1[2];
			PE = sa2[2];			
			PH = sa3[2];
			
			PA = sa1[pprev];
			PD = sa2[pprev];			
			PG = sa3[pprev];
			
            if (x)
            {
    			PC = sa1[3];
				PF = sa2[3];
				PI = sa3[3];
            }
            else	// right edge (used to read one pixel past the line first)
            {
   				PC = sa1[2];
   				PF = sa2[2];
   				PI = sa3[2];
   			}

			E[0]   = E[1]     = E[2]     = PE;
			E[nl]  = E[nl+1]  = E[nl+2]  = PE; // 3, 4, 5
			E[nl1] = E[nl1+1] = E[nl1+2] = PE; // 6, 7, 8
			
if (PE!=PH || PE!=PI || PE!=PF || PE!=PC || PE!=PB || PE!=PA || PE!=PD || PE!=PG) 
{
			
FILTRO(PE, PI, PH, PF, PG, PC, PD, PB, PA, 0, 1, 2, nl, nl+1, nl+2, nl1, nl1+1, nl1+2);
FILTRO(PE, PC, PF, PB, PI, PA, PH, PD, PG, nl1, nl, 0, nl1+1, nl+1, 1, nl1+2, nl+2, 2);
FILTRO(PE, PA, PB, PD, PC, PG, PF, PH, PI, nl1+2, nl1+1, nl1, nl+2, nl+1, nl, 2, 1, 0);
FILTRO(PE, PG, PD, PH, PA, PI, PB, PF, PC, 2, nl+2, nl1+2, 1, nl+1, nl1+1, 0, nl, nl1);

}
			sa1++;
			sa2++;
			sa3++;
			
            E+=3;
			
            pprev = 1;
		}
		
        sa2 += complete_line_src;
		sa1 = sa2 - nl_src;		
		sa3 = sa2 + nl_src;
		
        E += complete_line_dst;				
	}
}
