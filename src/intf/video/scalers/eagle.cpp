// Eagle 2x filter (15/16/32-bit)
//
// Portable C++ replacement for the old 32-bit MMX assembler (eagle_fm.asm),
// which implemented the classic Eagle algorithm: each source pixel becomes
// a 2x2 block, and each quarter of the block takes the colour of the three
// neighbours in its direction when those three are the same colour.
//
// The output matches the assembler everywhere except at the image borders,
// where the assembler went wrong: the top output line was one pixel out of
// alignment, the last call read the line below the image and wrote a line
// past the end of the output, each line copy wrote twice the line width,
// the rightmost output column was black, and the leftmost pixels used the
// previous call's last pixel (and zero) as their left neighbour.  Here the
// source is clamped at the borders, and only the 2W x 2H output is written.

template <typename T>
static void EagleRun(const unsigned char* src, unsigned int srcPitch, unsigned char* dst, unsigned int dstPitch, int width, int height)
{
	if (width <= 0 || height <= 0) return;

	for (int y = 0; y < height; y++) {
		const T* rU = (const T*)(src + (y > 0 ? y - 1 : 0) * srcPitch);
		const T* rC = (const T*)(src + y * srcPitch);
		const T* rD = (const T*)(src + (y + 1 < height ? y + 1 : height - 1) * srcPitch);

		T* d0 = (T*)(dst + (2 * y + 0) * dstPitch);
		T* d1 = (T*)(dst + (2 * y + 1) * dstPitch);

		for (int x = 0; x < width; x++) {
			const int xl = (x > 0) ? x - 1 : 0;
			const int xr = (x + 1 < width) ? x + 1 : width - 1;

			const T UL = rU[xl], U = rU[x], UR = rU[xr];
			const T L  = rC[xl], C = rC[x], R  = rC[xr];
			const T DL = rD[xl], D = rD[x], DR = rD[xr];

			d0[2 * x + 0] = (UL == U && U == L) ? U : C;
			d0[2 * x + 1] = (U == UR && UR == R) ? U : C;
			d1[2 * x + 0] = (L == DL && DL == D) ? D : C;
			d1[2 * x + 1] = (R == DR && DR == D) ? D : C;
		}
	}
}

// nDepth: 15, 16 or 32
void eagle2x(unsigned char* src, unsigned int srcPitch, unsigned char* dst, unsigned int dstPitch, int width, int height, int nDepth)
{
	if (nDepth == 32) {
		EagleRun<unsigned int>(src, srcPitch, dst, dstPitch, width, height);
	} else {
		EagleRun<unsigned short>(src, srcPitch, dst, dstPitch, width, height);
	}
}
