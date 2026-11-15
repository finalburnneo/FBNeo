// epxb, epxc filter, added by regret
// source from Snes9X rerecording (http://code.google.com/p/snes9x151-rerecording/)
//
// Rewritten as a per-pixel loop (the pixel logic is unchanged):
//  - Every neighbour is read with its coordinates clamped to the image.  The old
//    row/column macros left some neighbours of the edge pixels uninitialised
//    or holding values from the previous row (e.g. the top row used 0 for its
//    upper diagonals, and the left column used the previous row's right edge).
//    The right edge already behaved like this, so it's unchanged.
//  - 1-pixel wide or high images no longer loop (nearly) forever.
//  - 15, 16 and 32-bit images are supported (was 16-bit only; the unused
//    "32-bit" path took 16-bit input).

typedef unsigned short uint16;
typedef unsigned int uint32;

struct EpxFormat {
	uint32 Mask_2;	// middle channel (green)
	uint32 Mask13;	// outer channels (red, blue)
};

static const EpxFormat EpxFormat555 = { 0x03E0,   0x7C1F   };
static const EpxFormat EpxFormat565 = { 0x07E0,   0xF81F   };
static const EpxFormat EpxFormat888 = { 0x00FF00, 0xFF00FF };

#define Mask_2 (fmt.Mask_2)
#define Mask13 (fmt.Mask13)

#define Interp01(c1, c2) \
	((((c1) == (c2)) ? (c1) : \
	(((((((c1) & Mask_2) *  3) + ((c2) & Mask_2)) >> 2) & Mask_2) + \
	 ((((((c1) & Mask13) *  3) + ((c2) & Mask13)) >> 2) & Mask13))))

#define Interp44(c1, c2, c3, c4) \
	((((((c1) & Mask_2) * 5 + ((c2) & Mask_2) + ((c3) & Mask_2) + ((c4) & Mask_2)) >> 3) & Mask_2) + \
	 (((((c1) & Mask13) * 5 + ((c2) & Mask13) + ((c3) & Mask13) + ((c4) & Mask13)) >> 3) & Mask13))

// Neighbourhood of colorX:
//   E D H
//   A X C
//   F B G
// on00/on01/on10/on11: does the top-left/top-right/bottom-left/bottom-right
// corner of the output block have both of its edge neighbours inside the image?

template <typename T, int nType>
static void EpxRun(const unsigned char* src, unsigned int srcPitch, unsigned char* dst, unsigned int dstPitch, int width, int height, const EpxFormat& fmt)
{
	if (width <= 0 || height <= 0) return;

	for (int y = 0; y < height; y++) {
		const T* rU = (const T*)(src + (y > 0 ? y - 1 : 0) * srcPitch);
		const T* rC = (const T*)(src + y * srcPitch);
		const T* rD = (const T*)(src + (y + 1 < height ? y + 1 : height - 1) * srcPitch);

		T* dP1 = (T*)(dst + (2 * y + 0) * dstPitch);
		T* dP2 = (T*)(dst + (2 * y + 1) * dstPitch);

		const bool top = (y > 0), bot = (y + 1 < height);

		for (int x = 0; x < width; x++) {
			const int xl = (x > 0) ? x - 1 : 0;
			const int xr = (x + 1 < width) ? x + 1 : width - 1;

			const bool left = (x > 0), right = (x + 1 < width);
			const bool on00 = top && left, on01 = top && right, on10 = bot && left, on11 = bot && right;

			const uint32 colorE = rU[xl], colorD = rU[x], colorH = rU[xr];
			const uint32 colorA = rC[xl], colorX = rC[x], colorC = rC[xr];
			const uint32 colorF = rD[xl], colorB = rD[x], colorG = rD[xr];

			uint32 p00 = colorX, p01 = colorX, p10 = colorX, p11 = colorX;

			if (nType == 0) {
				// EPXB: improved 2X EPX, which tends to do better with diagonal edges than regular EPX
				if ((((on00||on10)?colorA:colorX) != ((on01||on11)?colorC:colorX))
				&& (((on10||on11)?colorB:colorX) != ((on00||on01)?colorD:colorX))
				&& ((!on00||!on01||!on10||!on11) ||
					((colorX == colorA) || (colorX == colorB) || (colorX == colorC) || (colorX == colorD) || /* diagonal    */
					 (((colorE != colorG) || (colorX == colorF) || (colorX == colorH)) &&                    /*  edge       */
					  ((colorF != colorH) || (colorX == colorE) || (colorX == colorG))))))                   /*   smoothing */
				{
					p00 = (on00 && colorD == colorA && (colorX != colorE || colorX != colorG || colorD != colorH || colorA != colorF)) ? colorD : colorX;
					p01 = (on01 && colorC == colorD && (colorX != colorH || colorX != colorF || colorC != colorG || colorD != colorE)) ? colorC : colorX;
					p10 = (on10 && colorA == colorB && (colorX != colorF || colorX != colorH || colorA != colorE || colorB != colorG)) ? colorA : colorX;
					p11 = (on11 && colorB == colorC && (colorX != colorG || colorX != colorE || colorB != colorF || colorC != colorH)) ? colorB : colorX;
				}
			} else {
				// EPXC: EPX3 scaled down to 2X
				if ((((on00||on10)?colorA:colorX) != ((on01||on11)?colorC:colorX))
				 && (((on10||on11)?colorB:colorX) != ((on00||on01)?colorD:colorX)))
				{
					const bool XnE = colorX != colorE;
					const bool XnF = colorX != colorF;
					const bool XnG = colorX != colorG;
					const bool XnH = colorX != colorH;
					const bool DA = on00 && colorD == colorA && (XnE || XnG || colorD != colorH || colorA != colorF);
					const bool AB = on10 && colorA == colorB && (XnF || XnH || colorA != colorE || colorB != colorG);
					const bool BC = on11 && colorB == colorC && (XnG || XnE || colorB != colorF || colorC != colorH);
					const bool CD = on01 && colorC == colorD && (XnH || XnF || colorC != colorG || colorD != colorE);
					if (!on00||!on01||!on10||!on11 || ((colorA != colorC) && (colorB != colorD) &&
						((colorX == colorA) || (colorX==colorB) || (colorX==colorC) || (colorX==colorD) || (colorX==colorE) || (colorX==colorF) || (colorX==colorG) || (colorX==colorH))))
					{
						const uint32 colorAA = on00&&on10 && ((DA && XnF) || (AB && XnE)) ? colorA : colorX;
						const uint32 colorBB = on10&&on11 && ((AB && XnG) || (BC && XnF)) ? colorB : colorX;
						const uint32 colorCC = on01&&on11 && ((BC && XnH) || (CD && XnG)) ? colorC : colorX;
						const uint32 colorDD = on00&&on01 && ((CD && XnE) || (DA && XnH)) ? colorD : colorX;
						p00 = Interp44(on00 && DA ? colorA : colorX, colorDD, colorAA, colorX);
						p01 = Interp44(on01 && CD ? colorC : colorX, colorBB, colorCC, colorX);
						p10 = Interp44(on10 && AB ? colorA : colorX, colorAA, colorBB, colorX);
						p11 = Interp44(on11 && BC ? colorC : colorX, colorCC, colorDD, colorX);
					} else {
						p00 = Interp01(colorX, on00 && DA && (colorX!=colorB&&colorX!=colorC) ? colorA : colorX);
						p01 = Interp01(colorX, on01 && CD && (colorX!=colorA&&colorX!=colorB) ? colorC : colorX);
						p10 = Interp01(colorX, on10 && AB && (colorX!=colorC&&colorX!=colorD) ? colorA : colorX);
						p11 = Interp01(colorX, on11 && BC && (colorX!=colorD&&colorX!=colorA) ? colorC : colorX);
					}
				}
			}

			dP1[2 * x + 0] = (T)p00;
			dP1[2 * x + 1] = (T)p01;
			dP2[2 * x + 0] = (T)p10;
			dP2[2 * x + 1] = (T)p11;
		}
	}
}

#undef Mask_2
#undef Mask13
#undef Interp01
#undef Interp44

template <int nType>
static void EpxDepth(unsigned char *src, unsigned int srcpitch, unsigned char *dst, unsigned int dstpitch, int nWidth, int nHeight, int vidDepth)
{
	switch (vidDepth) {
		case 15: EpxRun<uint16, nType>(src, srcpitch, dst, dstpitch, nWidth, nHeight, EpxFormat555); break;
		case 16: EpxRun<uint16, nType>(src, srcpitch, dst, dstpitch, nWidth, nHeight, EpxFormat565); break;
		case 32: EpxRun<uint32, nType>(src, srcpitch, dst, dstpitch, nWidth, nHeight, EpxFormat888); break;
	}
}

void RenderEPXB(unsigned char *src, unsigned int srcpitch, unsigned char *dst, unsigned int dstpitch, int nWidth, int nHeight, int vidDepth)
{
	EpxDepth<0>(src, srcpitch, dst, dstpitch, nWidth, nHeight, vidDepth);
}

void RenderEPXC(unsigned char *src, unsigned int srcpitch, unsigned char *dst, unsigned int dstpitch, int nWidth, int nHeight, int vidDepth)
{
	EpxDepth<1>(src, srcpitch, dst, dstpitch, nWidth, nHeight, vidDepth);
}
