// SuperScale 2x filter, with optional 75% scanlines (15/16/32-bit)
//
// Portable C++ replacement for the old 32-bit MMX assembler (superscale.asm).
//
// Each source pixel C becomes two pixels on each of two output lines.  On the
// upper line, with A = the pixel above and B = the pixel below:
//   left  = A if (left neighbour == A) and (A != B) and (right neighbour != A), else C
//   right = A if (right neighbour == A) and (A != B) and (left neighbour != A), else C
// The lower line is the same with A and B swapped; in scanline mode it is
// then darkened to 75%.
//
// The output matches the assembler everywhere except at the image borders:
// the assembler read one line above and one line below the image and one
// pixel past each side, and left the last 1-3 columns undrawn when the
// width wasn't a multiple of 4.  Here the source is clamped at the borders
// and every column is drawn.

static unsigned int nScanMask = 0x7BEF;	// halving mask for the current format

template <typename T>
static inline T Scan75(T p)
{
	T h = (T)((p >> 1) & nScanMask);

	return (T)(h + ((h >> 1) & nScanMask));
}

template <typename T, bool bScan75>
static void SuperScaleRun(const unsigned char* src, unsigned int srcPitch, unsigned char* dst, unsigned int dstPitch, int width, int height)
{
	if (width <= 0 || height <= 0) return;

	for (int y = 0; y < height; y++) {
		const T* rU = (const T*)(src + (y > 0 ? y - 1 : 0) * srcPitch);
		const T* rC = (const T*)(src + y * srcPitch);
		const T* rD = (const T*)(src + (y + 1 < height ? y + 1 : height - 1) * srcPitch);

		T* d0 = (T*)(dst + (2 * y + 0) * dstPitch);
		T* d1 = (T*)(dst + (2 * y + 1) * dstPitch);

		for (int x = 0; x < width; x++) {
			const T L = rC[(x > 0) ? x - 1 : 0];
			const T C = rC[x];
			const T R = rC[(x + 1 < width) ? x + 1 : width - 1];
			const T U = rU[x];
			const T D = rD[x];

			// upper line: A = above, B = below
			d0[2 * x + 0] = (L == U && U != D && R != U) ? U : C;
			d0[2 * x + 1] = (R == U && U != D && L != U) ? U : C;

			// lower line: A = below, B = above
			T p0 = (L == D && D != U && R != D) ? D : C;
			T p1 = (R == D && D != U && L != D) ? D : C;

			if (bScan75) {
				p0 = Scan75<T>(p0);
				p1 = Scan75<T>(p1);
			}

			d1[2 * x + 0] = p0;
			d1[2 * x + 1] = p1;
		}
	}
}

// nDepth: 15, 16 or 32
void superscale2x(unsigned char* src, unsigned int srcPitch, unsigned char* dst, unsigned int dstPitch, int width, int height, int nDepth, bool bScan75)
{
	switch (nDepth) {
		case 15: nScanMask = 0x3DEF;   break;
		case 16: nScanMask = 0x7BEF;   break;
		case 32: nScanMask = 0x7F7F7F; break;
	}

	if (nDepth == 32) {
		if (bScan75) {
			SuperScaleRun<unsigned int, true>(src, srcPitch, dst, dstPitch, width, height);
		} else {
			SuperScaleRun<unsigned int, false>(src, srcPitch, dst, dstPitch, width, height);
		}
	} else {
		if (bScan75) {
			SuperScaleRun<unsigned short, true>(src, srcPitch, dst, dstPitch, width, height);
		} else {
			SuperScaleRun<unsigned short, false>(src, srcPitch, dst, dstPitch, width, height);
		}
	}
}
