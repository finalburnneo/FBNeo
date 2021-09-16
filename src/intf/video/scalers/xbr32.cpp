/*
   Hyllian's 2xBR v3.3b

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

#include "burnint.h"

#define LB_MASK       0x00FEFEFE
#define RED_BLUE_MASK 0x00FF00FF
#define GREEN_MASK    0x0000FF00
#define PART_MASK     0x00FF00FF

#define ALPHA_BLEND_BASE(a, b, m, s) (  (PART_MASK & (((a) & PART_MASK) + (((((b) & PART_MASK) - ((a) & PART_MASK)) * (m)) >> (s)))) \
                                      | ((PART_MASK & ((((a) >> 8) & PART_MASK) + ((((((b) >> 8) & PART_MASK) - (((a) >> 8) & PART_MASK)) * (m)) >> (s)))) << 8))

#define ALPHA_BLEND_32_W(a, b)  ALPHA_BLEND_BASE(a, b, 1, 3)
#define ALPHA_BLEND_64_W(a, b)  ALPHA_BLEND_BASE(a, b, 1, 2)
#define ALPHA_BLEND_128_W(a, b) ALPHA_BLEND_BASE(a, b, 1, 1)
#define ALPHA_BLEND_192_W(a, b) ALPHA_BLEND_BASE(a, b, 3, 2)
#define ALPHA_BLEND_224_W(a, b) ALPHA_BLEND_BASE(a, b, 7, 3)

static UINT32 pixel_diff(UINT32 x, UINT32 y, const UINT32 *r2y)
{
#define YMASK 0xff0000
#define UMASK 0x00ff00
#define VMASK 0x0000ff

    INT32 yuv1 = r2y[x & 0xffffff];
    INT32 yuv2 = r2y[y & 0xffffff];

    return (abs((int)(x >> 24) - (int)(y >> 24))) +
           (abs((int)(yuv1 & YMASK) - (int)(yuv2 & YMASK)) >> 16) +
           (abs((int)(yuv1 & UMASK) - (int)(yuv2 & UMASK)) >>  8) +
           abs((int)(yuv1 & VMASK) - (int)(yuv2 & VMASK));
}

#define df(A, B) pixel_diff(A, B, r2y)
#define eq(A, B) (df(A, B) < 155)

#define FILT2(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1,   \
              N0, N1, N2, N3) do {                                                                  \
    if (PE != PH && PE != PF) {                                                                     \
        const unsigned e = df(PE,PC) + df(PE,PG) + df(PI,H5) + df(PI,F4) + (df(PH,PF)<<2);          \
        const unsigned i = df(PH,PD) + df(PH,I5) + df(PF,I4) + df(PF,PB) + (df(PE,PI)<<2);          \
        if (e <= i) {                                                                               \
            const unsigned px = df(PE,PF) <= df(PE,PH) ? PF : PH;                                   \
            if (e < i && ( (!eq(PF,PB) && !eq(PH,PD)) || (eq(PE,PI)                                     \
                          && (!eq(PF,I4) && !eq(PH,I5)))                                             \
                          || eq(PE,PG) || eq(PE,PC))) {                                             \
                const unsigned ke = df(PF,PG);                                                      \
                const unsigned ki = df(PH,PC);                                                      \
                const int left    = ke<<1 <= ki && PE != PG && PD != PG;                            \
                const int up      = ke >= ki<<1 && PE != PC && PB != PC;                            \
                if (left && up) {                                                                   \
                    E[N3] = ALPHA_BLEND_224_W(E[N3], px);                                           \
                    E[N2] = ALPHA_BLEND_64_W( E[N2], px);                                           \
                    E[N1] = E[N2];                                                                  \
                } else if (left) {                                                                  \
                    E[N3] = ALPHA_BLEND_192_W(E[N3], px);                                           \
                    E[N2] = ALPHA_BLEND_64_W( E[N2], px);                                           \
                } else if (up) {                                                                    \
                    E[N3] = ALPHA_BLEND_192_W(E[N3], px);                                           \
                    E[N1] = ALPHA_BLEND_64_W( E[N1], px);                                           \
                } else { /* diagonal */                                                             \
                    E[N3] = ALPHA_BLEND_128_W(E[N3], px);                                           \
                }                                                                                   \
            } else {                                                                                \
                E[N3] = ALPHA_BLEND_128_W(E[N3], px);                                               \
            }                                                                                       \
        }                                                                                           \
    }                                                                                               \
} while (0)

#define FILT3(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1,   \
              N0, N1, N2, N3, N4, N5, N6, N7, N8) do {                                              \
    if (PE != PH && PE != PF) {                                                                     \
        const unsigned e = df(PE,PC) + df(PE,PG) + df(PI,H5) + df(PI,F4) + (df(PH,PF)<<2);          \
        const unsigned i = df(PH,PD) + df(PH,I5) + df(PF,I4) + df(PF,PB) + (df(PE,PI)<<2);          \
        if (e <= i) {                                                                               \
            const unsigned px = df(PE,PF) <= df(PE,PH) ? PF : PH;                                   \
            if (e < i && ( (!eq(PF,PB) && !eq(PF,PC)) || (!eq(PH,PD) && !eq(PH,PG)) || ((eq(PE,PI)         \
                          && (!eq(PF,F4) && !eq(PF,I4))) || (!eq(PH,H5) && !eq(PH,I5)))                 \
                          || eq(PE,PG) || eq(PE,PC))) {                                             \
                const unsigned ke = df(PF,PG);                                                      \
                const unsigned ki = df(PH,PC);                                                      \
                const int left    = ke<<1 <= ki && PE != PG && PD != PG;                            \
                const int up      = ke >= ki<<1 && PE != PC && PB != PC;                            \
                if (left && up) {                                                                   \
                    E[N7] = ALPHA_BLEND_192_W(E[N7], px);                                           \
                    E[N6] = ALPHA_BLEND_64_W( E[N6], px);                                           \
                    E[N5] = E[N7];                                                                  \
                    E[N2] = E[N6];                                                                  \
                    E[N8] = px;                                                                     \
                } else if (left) {                                                                  \
                    E[N7] = ALPHA_BLEND_192_W(E[N7], px);                                           \
                    E[N5] = ALPHA_BLEND_64_W( E[N5], px);                                           \
                    E[N6] = ALPHA_BLEND_64_W( E[N6], px);                                           \
                    E[N8] = px;                                                                     \
                } else if (up) {                                                                    \
                    E[N5] = ALPHA_BLEND_192_W(E[N5], px);                                           \
                    E[N7] = ALPHA_BLEND_64_W( E[N7], px);                                           \
                    E[N2] = ALPHA_BLEND_64_W( E[N2], px);                                           \
                    E[N8] = px;                                                                     \
                } else { /* diagonal */                                                             \
                    E[N8] = ALPHA_BLEND_224_W(E[N8], px);                                           \
                    E[N5] = ALPHA_BLEND_32_W( E[N5], px);                                           \
                    E[N7] = ALPHA_BLEND_32_W( E[N7], px);                                           \
                }                                                                                   \
            } else {                                                                                \
                E[N8] = ALPHA_BLEND_128_W(E[N8], px);                                               \
            }                                                                                       \
        }                                                                                           \
    }                                                                                               \
} while (0)

#define FILT4(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1,   \
              N15, N14, N11, N3, N7, N10, N13, N12, N9, N6, N2, N1, N5, N8, N4, N0) do {            \
    if (PE != PH && PE != PF) {                                                                     \
        const unsigned e = df(PE,PC) + df(PE,PG) + df(PI,H5) + df(PI,F4) + (df(PH,PF)<<2);          \
        const unsigned i = df(PH,PD) + df(PH,I5) + df(PF,I4) + df(PF,PB) + (df(PE,PI)<<2);          \
        if (e <= i) {                                                                               \
            const unsigned px = df(PE,PF) <= df(PE,PH) ? PF : PH;                                   \
            if (e < i && ( (!eq(PF,PB) && !eq(PH,PD)) || ( eq(PE,PI)                                     \
                          && (!eq(PF,I4) && !eq(PH,I5)))                                             \
                          || eq(PE,PG) || eq(PE,PC))) {                                             \
                const unsigned ke = df(PF,PG);                                                      \
                const unsigned ki = df(PH,PC);                                                      \
                const int left    = ke<<1 <= ki && PE != PG && PD != PG;                            \
                const int up      = ke >= ki<<1 && PE != PC && PB != PC;                            \
                if (left && up) {                                                                   \
                    E[N13] = ALPHA_BLEND_192_W(E[N13], px);                                         \
                    E[N12] = ALPHA_BLEND_64_W( E[N12], px);                                         \
                    E[N15] = E[N14] = E[N11] = px;                                                  \
                    E[N10] = E[N3]  = E[N12];                                                       \
                    E[N7]  = E[N13];                                                                \
                } else if (left) {                                                                  \
                    E[N11] = ALPHA_BLEND_192_W(E[N11], px);                                         \
                    E[N13] = ALPHA_BLEND_192_W(E[N13], px);                                         \
                    E[N10] = ALPHA_BLEND_64_W( E[N10], px);                                         \
                    E[N12] = ALPHA_BLEND_64_W( E[N12], px);                                         \
                    E[N14] = px;                                                                    \
                    E[N15] = px;                                                                    \
                } else if (up) {                                                                    \
                    E[N14] = ALPHA_BLEND_192_W(E[N14], px);                                         \
                    E[N7 ] = ALPHA_BLEND_192_W(E[N7 ], px);                                         \
                    E[N10] = ALPHA_BLEND_64_W( E[N10], px);                                         \
                    E[N3 ] = ALPHA_BLEND_64_W( E[N3 ], px);                                         \
                    E[N11] = px;                                                                    \
                    E[N15] = px;                                                                    \
                } else { /* diagonal */                                                             \
                    E[N11] = ALPHA_BLEND_128_W(E[N11], px);                                         \
                    E[N14] = ALPHA_BLEND_128_W(E[N14], px);                                         \
                    E[N15] = px;                                                                    \
                }                                                                                   \
            } else {                                                                                \
                E[N15] = ALPHA_BLEND_128_W(E[N15], px);                                             \
            }                                                                                       \
        }                                                                                           \
    }                                                                                               \
} while (0)

typedef struct {
    const UINT8 *input;
    UINT8 *output;
    int inWidth, inHeight;
    int inPitch, outPitch;
    UINT32 rgbtoyuv[1<<24];
} xbr_params;

static inline void xbr_filter(const xbr_params *params, int n)
{
    int x, y;
    const UINT32 *r2y = params->rgbtoyuv;
    const int nl = params->outPitch >> 2;
    const int nl1 = nl + nl;
    const int nl2 = nl1 + nl;

    for (y = 0; y < params->inHeight; y++) {

        UINT32 *E = (UINT32 *)(params->output + y * params->outPitch * n);
        const UINT32 *sa2 = (UINT32 *)(params->input + y * params->inPitch - 8); /* center */
        const UINT32 *sa1 = sa2 - (params->inPitch>>2); /* up x1 */
        const UINT32 *sa0 = sa1 - (params->inPitch>>2); /* up x2 */
        const UINT32 *sa3 = sa2 + (params->inPitch>>2); /* down x1 */
        const UINT32 *sa4 = sa3 + (params->inPitch>>2); /* down x2 */

        if (y <= 1) {
            sa0 = sa1;
            if (y == 0) {
                sa0 = sa1 = sa2;
            }
        }

        if (y >= params->inHeight - 2) {
            sa4 = sa3;
            if (y == params->inHeight - 1) {
                sa4 = sa3 = sa2;
            }
        }

        for (x = 0; x < params->inWidth; x++) {
            const UINT32 B1 = sa0[2];
            const UINT32 PB = sa1[2];
            const UINT32 PE = sa2[2];
            const UINT32 PH = sa3[2];
            const UINT32 H5 = sa4[2];

            const int pprev = 2 - (x > 0);
            const UINT32 A1 = sa0[pprev];
            const UINT32 PA = sa1[pprev];
            const UINT32 PD = sa2[pprev];
            const UINT32 PG = sa3[pprev];
            const UINT32 G5 = sa4[pprev];

            const int pprev2 = pprev - (x > 1);
            const UINT32 A0 = sa1[pprev2];
            const UINT32 D0 = sa2[pprev2];
            const UINT32 G0 = sa3[pprev2];

            const int pnext = 3 - (x == params->inWidth - 1);
            const UINT32 C1 = sa0[pnext];
            const UINT32 PC = sa1[pnext];
            const UINT32 PF = sa2[pnext];
            const UINT32 PI = sa3[pnext];
            const UINT32 I5 = sa4[pnext];

            const int pnext2 = pnext + 1 - (x >= params->inWidth - 2);
            const UINT32 C4 = sa1[pnext2];
            const UINT32 F4 = sa2[pnext2];
            const UINT32 I4 = sa3[pnext2];

            if (n == 2) {
                E[0]  = E[1]      =     // 0, 1
                E[nl] = E[nl + 1] = PE; // 2, 3

                FILT2(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, 0, 1, nl, nl+1);
                FILT2(PE, PC, PF, PB, PI, PA, PH, PD, PG, I4, A1, I5, H5, A0, D0, B1, C1, F4, C4, G5, G0, nl, 0, nl+1, 1);
                FILT2(PE, PA, PB, PD, PC, PG, PF, PH, PI, C1, G0, C4, F4, G5, H5, D0, A0, B1, A1, I4, I5, nl+1, nl, 1, 0);
                FILT2(PE, PG, PD, PH, PA, PI, PB, PF, PC, A0, I5, A1, B1, I4, F4, H5, G5, D0, G0, C1, C4, 1, nl+1, 0, nl);
            } else if (n == 3) {
                E[0]   = E[1]     = E[2]     =     // 0, 1, 2
                E[nl]  = E[nl+1]  = E[nl+2]  =     // 3, 4, 5
                E[nl1] = E[nl1+1] = E[nl1+2] = PE; // 6, 7, 8

                FILT3(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, 0, 1, 2, nl, nl+1, nl+2, nl1, nl1+1, nl1+2);
                FILT3(PE, PC, PF, PB, PI, PA, PH, PD, PG, I4, A1, I5, H5, A0, D0, B1, C1, F4, C4, G5, G0, nl1, nl, 0, nl1+1, nl+1, 1, nl1+2, nl+2, 2);
                FILT3(PE, PA, PB, PD, PC, PG, PF, PH, PI, C1, G0, C4, F4, G5, H5, D0, A0, B1, A1, I4, I5, nl1+2, nl1+1, nl1, nl+2, nl+1, nl, 2, 1, 0);
                FILT3(PE, PG, PD, PH, PA, PI, PB, PF, PC, A0, I5, A1, B1, I4, F4, H5, G5, D0, G0, C1, C4, 2, nl+2, nl1+2, 1, nl+1, nl1+1, 0, nl, nl1);
            } else if (n == 4) {
                E[0]   = E[1]     = E[2]     = E[3]     =     //  0,  1,  2,  3
                E[nl]  = E[nl+1]  = E[nl+2]  = E[nl+3]  =     //  4,  5,  6,  7
                E[nl1] = E[nl1+1] = E[nl1+2] = E[nl1+3] =     //  8,  9, 10, 11
                E[nl2] = E[nl2+1] = E[nl2+2] = E[nl2+3] = PE; // 12, 13, 14, 15

                FILT4(PE, PI, PH, PF, PG, PC, PD, PB, PA, G5, C4, G0, D0, C1, B1, F4, I4, H5, I5, A0, A1, nl2+3, nl2+2, nl1+3, 3, nl+3, nl1+2, nl2+1, nl2, nl1+1, nl+2, 2, 1, nl+1, nl1, nl, 0);
                FILT4(PE, PC, PF, PB, PI, PA, PH, PD, PG, I4, A1, I5, H5, A0, D0, B1, C1, F4, C4, G5, G0, 3, nl+3, 2, 0, 1, nl+2, nl1+3, nl2+3, nl1+2, nl+1, nl, nl1, nl1+1, nl2+2, nl2+1, nl2);
                FILT4(PE, PA, PB, PD, PC, PG, PF, PH, PI, C1, G0, C4, F4, G5, H5, D0, A0, B1, A1, I4, I5, 0, 1, nl, nl2, nl1, nl+1, 2, 3, nl+2, nl1+1, nl2+1, nl2+2, nl1+2, nl+3, nl1+3, nl2+3);
                FILT4(PE, PG, PD, PH, PA, PI, PB, PF, PC, A0, I5, A1, B1, I4, F4, H5, G5, D0, G0, C1, C4, nl2, nl1, nl2+1, nl2+3, nl2+2, nl1+1, nl, 0, nl+1, nl1+2, nl1+3, nl+3, nl+2, 1, 2, 3);
            }

            sa0 += 1;
            sa1 += 1;
            sa2 += 1;
            sa3 += 1;
            sa4 += 1;

            E += n;
        }
    }
}

static inline int _max(int a, int b)
{
	return (a > b) ? a : b;
}

static inline int _min(int a, int b)
{
	return (a < b) ? a : b;
}

static void xbr_init_data(xbr_params *params)
{
    UINT32 c;
    int bg, rg, g;

    for (bg = -255; bg < 256; bg++) {
        for (rg = -255; rg < 256; rg++) {
            const UINT32 u = (UINT32)((-169*rg + 500*bg)/1000) + 128;
            const UINT32 v = (UINT32)(( 500*rg -  81*bg)/1000) + 128;
            int startg = _max(-bg, _max(-rg, 0));
            int endg = _min(255-bg, _min(255-rg, 255));
            UINT32 y = (UINT32)(( 299*rg + 1000*startg + 114*bg)/1000);
            c = bg + (rg<<16) + 0x010101 * startg;
            for (g = startg; g <= endg; g++) {
                params->rgbtoyuv[c] = ((y++) << 16) + (u << 8) + v;
                c+= 0x010101;
            }
        }
    }
}

void xbr_filter_xbr2x(const xbr_params *params)
{
    xbr_filter(params, 2);
}

void xbr_filter_xbr3x(const xbr_params *params)
{
    xbr_filter(params, 3);
}

void xbr_filter_xbr4x(const xbr_params *params)
{
    xbr_filter(params, 4);
}

static xbr_params xbrparams;

static void xbr32_init()
{
    static int initialized = 0;
    if (initialized){
        return;
    }
    initialized = 1;

	memset(&xbrparams, 0, sizeof(xbrparams));

	xbr_init_data(&xbrparams);
}

void xbr2x_32(unsigned char * pIn,  unsigned int srcPitch, unsigned char * pOut, unsigned int dstPitch, int Xres, int Yres)
{
	xbr32_init();

	xbrparams.input = pIn;
	xbrparams.output = pOut;
	xbrparams.inPitch = srcPitch;
	xbrparams.outPitch = dstPitch;
	xbrparams.inWidth = Xres;
	xbrparams.inHeight = Yres;

	xbr_filter_xbr2x(&xbrparams);
}

void xbr3x_32(unsigned char * pIn,  unsigned int srcPitch, unsigned char * pOut, unsigned int dstPitch, int Xres, int Yres)
{
	xbr32_init();

	xbrparams.input = pIn;
	xbrparams.output = pOut;
	xbrparams.inPitch = srcPitch;
	xbrparams.outPitch = dstPitch;
	xbrparams.inWidth = Xres;
	xbrparams.inHeight = Yres;

	xbr_filter_xbr3x(&xbrparams);
}

void xbr4x_32(unsigned char * pIn,  unsigned int srcPitch, unsigned char * pOut, unsigned int dstPitch, int Xres, int Yres)
{
	xbr32_init();

	xbrparams.input = pIn;
	xbrparams.output = pOut;
	xbrparams.inPitch = srcPitch;
	xbrparams.outPitch = dstPitch;
	xbrparams.inWidth = Xres;
	xbrparams.inHeight = Yres;

	xbr_filter_xbr4x(&xbrparams);
}

