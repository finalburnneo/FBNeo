// 2xSaI, Super 2xSaI and SuperEagle for 15/16-bit (555/565) images
//
// Portable C++ replacement for the old 32-bit MMX assembler (2xsaimmx.asm),
// so these filters work in 32-bit, 64-bit and non-x86 builds alike.
//
// The output matches the MMX version pixel-for-pixel, with these exceptions:
//  - Edge pixels: the source is clamped at the image borders instead of
//    reading outside the buffer.
//  - SuperEagle / 2xSaI: when the "guess" vote went negative, the asm OR'd the
//    raw vote count into its select mask (instead of the vote<0 flag), which
//    corrupted the low bits of the result.  Fixed.
//  - Super 2xSaI: two conditions compared a mask register against a colour
//    (intended "colorB1 != color6" and "colorA1 != color3").  Fixed.  Where
//    several cases matched at once the asm OR'd their results together; here
//    the first matching case (in the order of the original C code) wins.
//
//---------------------------------------------------------------------*
// The 2xSaI engine is copyright (c) 2001 by Derek Liauw Kie Fa.        *
// Non-Commercial use of the engine is allowed and is encouraged,       *
// provided that appropriate credit be given and that this copyright    *
// notice will not be removed under any circumstance.                   *
// You may freely modify this code, but I request                       *
// that any improvements to the engine be submitted to me, so           *
// that I can implement these improvements in newer versions of         *
// the engine.                                                          *
// If you need more information, have any comments or suggestions,      *
// you can e-mail me. My e-mail: DerekL666@yahoo.com                    *
//---------------------------------------------------------------------*

typedef unsigned char  sai_u8;
typedef unsigned short sai_u16;
typedef unsigned int   sai_u32;

static sai_u32 colorMask16     = 0xF7DE;	// 565 defaults
static sai_u32 lowPixelMask16  = 0x0821;
static sai_u32 qcolorMask16    = 0xE79C;
static sai_u32 qlowpixelMask16 = 0x1863;

// nDepth: 15 (555) or 16 (565)
void Init_2xSaI16(int nDepth)
{
	if (nDepth == 15) {
		colorMask16     = 0x7BDE;
		lowPixelMask16  = 0x0421;
		qcolorMask16    = 0x739C;
		qlowpixelMask16 = 0x0C63;
	} else {
		colorMask16     = 0xF7DE;
		lowPixelMask16  = 0x0821;
		qcolorMask16    = 0xE79C;
		qlowpixelMask16 = 0x1863;
	}
}

// 50/50 blend
static inline sai_u32 Interp(sai_u32 a, sai_u32 b)
{
	return ((a & colorMask16) >> 1) + ((b & colorMask16) >> 1) + (a & b & lowPixelMask16);
}

// 25/25/25/25 blend
static inline sai_u32 QInterp(sai_u32 a, sai_u32 b, sai_u32 c, sai_u32 d)
{
	sai_u32 x = ((a & qcolorMask16) >> 2) + ((b & qcolorMask16) >> 2) + ((c & qcolorMask16) >> 2) + ((d & qcolorMask16) >> 2);
	sai_u32 y = (a & qlowpixelMask16) + (b & qlowpixelMask16) + (c & qlowpixelMask16) + (d & qlowpixelMask16);

	return x + ((y >> 2) & qlowpixelMask16);
}

// Map of the source pixels around the one being scaled (A):
//   I E F J
//   G A B K
//   H C D L
//   M N O P
// (in the Super2xSaI / SuperEagle naming: B0 B1 B2 B3 / 4 5 6 S2 / 1 2 3 S1 / A0 A1 A2 A3)
struct SaiGrid {
	sai_u32 I, E, F, J;
	sai_u32 G, A, B, K;
	sai_u32 H, C, D, L;
	sai_u32 M, N, O, P;
};

// One pair's vote in the A-vs-B tie-break: +1 favours A, -1 favours B
static inline int SaiVote(sai_u32 a, sai_u32 b, sai_u32 p, sai_u32 q)
{
	int na = (p == a) + (q == a);
	int nb = (p == b) + (q == b);

	return (na <= 1) - (nb <= 1);
}

// Tie-break for the "both diagonals match" case (A == D and B == C, A != B)
static inline int SaiGuess(const SaiGrid& g)
{
	return SaiVote(g.A, g.B, g.E, g.G) + SaiVote(g.A, g.B, g.F, g.K) + SaiVote(g.A, g.B, g.H, g.N) + SaiVote(g.A, g.B, g.L, g.O);
}

// Diagonal classification shared by all three filters
enum { SAI_NONE = 0, SAI_AD, SAI_BC };	// A-D diagonal wins / B-C diagonal wins

static inline int SaiDiagonal(const SaiGrid& g)
{
	if (g.A == g.D) {
		if (g.B != g.C || g.A == g.B) return SAI_AD;

		int r = SaiGuess(g);
		if (r > 0) return SAI_AD;
		if (r < 0) return SAI_BC;
		return SAI_NONE;
	}

	return (g.B == g.C) ? SAI_BC : SAI_NONE;
}

// out[0] = top-left, out[1] = top-right, out[2] = bottom-left, out[3] = bottom-right

static inline void Kernel2xSaI(const SaiGrid& g, sai_u32* out)
{
	const sai_u32 A = g.A, B = g.B, C = g.C, D = g.D;

	out[0] = A;

	// top-right
	if ((A == D && B != C && A == g.E && B == g.L) || (A == C && A == g.F && B != g.E && B == g.J)) {
		out[1] = A;
	} else if ((B == C && A != D && B == g.F && A == g.H) || (B == D && B == g.E && A != g.F && A == g.I)) {
		out[1] = B;
	} else {
		out[1] = Interp(A, B);
	}

	// bottom-left
	if ((A == D && C != B && A == g.G && C == g.O) || (A == B && A == g.H && C != g.G && C == g.M)) {
		out[2] = A;
	} else if ((C == B && A != D && C == g.H && A == g.F) || (C == D && C == g.G && A != g.H && A == g.I)) {
		out[2] = C;
	} else {
		out[2] = Interp(A, C);
	}

	// bottom-right
	switch (SaiDiagonal(g)) {
		case SAI_AD: out[3] = A; break;
		case SAI_BC: out[3] = B; break;
		default:     out[3] = QInterp(A, B, C, D); break;
	}
}

static inline void KernelSuper2xSaI(const SaiGrid& g, sai_u32* out)
{
	const sai_u32 A = g.A, B = g.B, C = g.C, D = g.D;
	const int diag = SaiDiagonal(g);

	// right column
	if (diag == SAI_BC) {
		out[1] = B;
		out[3] = C;
	} else if (diag == SAI_AD) {
		out[1] = A;
		out[3] = D;
	} else {
		sai_u32 iAB = Interp(A, B);
		sai_u32 iCD = Interp(C, D);

		if (B == D && B == g.E && A != g.F && B != g.I) {
			out[1] = Interp(iAB, B);
		} else if (A == C && A == g.F && B != g.E && A != g.J) {
			out[1] = Interp(iAB, A);
		} else {
			out[1] = iAB;
		}

		if (B == D && D == g.N && C != g.O && D != g.M) {
			out[3] = Interp(iCD, D);
		} else if (A == C && C == g.O && g.N != D && C != g.P) {
			out[3] = Interp(iCD, C);
		} else {
			out[3] = iCD;
		}
	}

	// left column
	sai_u32 iAC = Interp(A, C);

	if ((diag == SAI_BC && g.H == C && g.F != C) || (D == C && g.G == C && A != g.H && g.I != C)) {
		out[0] = iAC;
	} else {
		out[0] = A;
	}

	if ((diag == SAI_AD && g.G == A && g.O != A) || (B == A && g.H == A && C != g.G && g.M != A)) {
		out[2] = iAC;
	} else {
		out[2] = C;
	}
}

static inline void KernelSuperEagle(const SaiGrid& g, sai_u32* out)
{
	const sai_u32 A = g.A, B = g.B, C = g.C, D = g.D;

	int diag = SAI_NONE;
	bool strong = false;

	if (A == D && B != C) {
		diag = SAI_AD;
		strong = (g.L == A && g.G == A) || (g.O == A && g.E == A);
	} else if (A != D && B == C) {
		diag = SAI_BC;
		strong = (g.H == B && g.K == B) || (g.N == B && g.F == B);
	} else if (A == D && B == C && A != B) {
		int r = SaiGuess(g);
		if (r > 0) diag = SAI_AD;
		if (r < 0) diag = SAI_BC;
	} else if (A == C || B == D) {
		out[0] = A; out[1] = B; out[2] = C; out[3] = D;
		return;
	}

	sai_u32 iAB = Interp(A, B);
	sai_u32 iCD = Interp(C, D);

	if (diag == SAI_AD) {
		out[0] = A;
		out[1] = strong ? Interp(iAB, A) : iAB;
		out[2] = strong ? Interp(iCD, D) : iCD;
		out[3] = D;
	} else if (diag == SAI_BC) {
		out[0] = strong ? Interp(iAB, B) : iAB;
		out[1] = B;
		out[2] = C;
		out[3] = strong ? Interp(iCD, C) : iCD;
	} else {
		out[0] = Interp(iAB, A);
		out[1] = Interp(iAB, B);
		out[2] = Interp(iCD, C);
		out[3] = Interp(iCD, D);
	}
}

// Runs a kernel over the whole image: each source pixel becomes a 2x2 block.
// Source reads are clamped to the image, so no border/margin lines are needed.
template <void (*Kernel)(const SaiGrid&, sai_u32*)>
static void Run2xSaI16(sai_u8* srcPtr, sai_u32 srcPitch, sai_u8* dstPtr, sai_u32 dstPitch, int width, int height)
{
	if (width <= 0 || height <= 0) return;

	for (int y = 0; y < height; y++) {
		const sai_u16* r0 = (const sai_u16*)(srcPtr + (y > 0 ? y - 1 : 0) * srcPitch);
		const sai_u16* r1 = (const sai_u16*)(srcPtr + y * srcPitch);
		const sai_u16* r2 = (const sai_u16*)(srcPtr + (y + 1 < height ? y + 1 : height - 1) * srcPitch);
		const sai_u16* r3 = (const sai_u16*)(srcPtr + (y + 2 < height ? y + 2 : height - 1) * srcPitch);

		sai_u16* d0 = (sai_u16*)(dstPtr + (2 * y + 0) * dstPitch);
		sai_u16* d1 = (sai_u16*)(dstPtr + (2 * y + 1) * dstPitch);

		for (int x = 0; x < width; x++) {
			const int xl  = (x > 0) ? x - 1 : 0;
			const int xr  = (x + 1 < width) ? x + 1 : width - 1;
			const int xrr = (x + 2 < width) ? x + 2 : width - 1;

			SaiGrid g;
			g.I = r0[xl]; g.E = r0[x]; g.F = r0[xr]; g.J = r0[xrr];
			g.G = r1[xl]; g.A = r1[x]; g.B = r1[xr]; g.K = r1[xrr];
			g.H = r2[xl]; g.C = r2[x]; g.D = r2[xr]; g.L = r2[xrr];
			g.M = r3[xl]; g.N = r3[x]; g.O = r3[xr]; g.P = r3[xrr];

			sai_u32 out[4];
			Kernel(g, out);

			d0[2 * x + 0] = (sai_u16)out[0];
			d0[2 * x + 1] = (sai_u16)out[1];
			d1[2 * x + 0] = (sai_u16)out[2];
			d1[2 * x + 1] = (sai_u16)out[3];
		}
	}
}

void _2xSaI16(sai_u8* srcPtr, sai_u32 srcPitch, sai_u8* dstPtr, sai_u32 dstPitch, int width, int height)
{
	Run2xSaI16<Kernel2xSaI>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void Super2xSaI16(sai_u8* srcPtr, sai_u32 srcPitch, sai_u8* dstPtr, sai_u32 dstPitch, int width, int height)
{
	Run2xSaI16<KernelSuper2xSaI>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}

void SuperEagle16(sai_u8* srcPtr, sai_u32 srcPitch, sai_u8* dstPtr, sai_u32 dstPitch, int width, int height)
{
	Run2xSaI16<KernelSuperEagle>(srcPtr, srcPitch, dstPtr, dstPitch, width, height);
}
