// Software blitter effects

// TODO: use .c/cpp version of scalers instead of asm (if avail.) for maximal
// compatibility with 64bit exe

#include "burner.h"
#include "vid_softfx.h"
#include "xbr.h"

#include "crt.h"

typedef unsigned long uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;

#ifndef _WIN32
#ifndef WITH_QTCREATOR
 typedef UINT32 DWORD;
#endif
#endif

void _2xpm_lq(void *SrcPtr, void *DstPtr, unsigned long SrcPitch, unsigned long DstPitch, unsigned long SrcW, unsigned long SrcH, int nDepth);
void _2xpm_hq(void *SrcPtr, void *DstPtr, unsigned long SrcPitch, unsigned long DstPitch, unsigned long SrcW, unsigned long SrcH, int nDepth);

extern void hq2xS_init(unsigned bits_per_pixel);
extern void hq2xS(unsigned char*, unsigned int, unsigned char*, unsigned char*, unsigned int, int, int);
extern void hq2xS32(unsigned char*, unsigned int, unsigned char*, unsigned char*, unsigned int, int, int);
extern void hq3xS(unsigned char*,unsigned int,unsigned char*,unsigned char*,unsigned int,int,int);
extern void hq3xS32(unsigned char*,unsigned int,unsigned char*,unsigned char*,unsigned int,int,int);

extern int Init_2xSaI(unsigned int BitFormat, unsigned int systemColorDepth);
extern void _2xSaI32(unsigned char*, unsigned int, unsigned char*, unsigned char*, unsigned int, int, int);
extern void Super2xSaI32(unsigned char*, unsigned int, unsigned char*, unsigned char*, unsigned int, int, int);
extern void SuperEagle32(unsigned char*, unsigned int, unsigned char*, unsigned char*, unsigned int, int, int);

extern void Init_2xSaI16(int nDepth);
extern void _2xSaI16(unsigned char*, unsigned int, unsigned char*, unsigned int, int, int);
extern void Super2xSaI16(unsigned char*, unsigned int, unsigned char*, unsigned int, int, int);
extern void SuperEagle16(unsigned char*, unsigned int, unsigned char*, unsigned int, int, int);

extern void hqx_init(int nDepth);
extern void hq2x(unsigned char*, unsigned int, unsigned char*, unsigned int, int, int);
extern void hq3x(unsigned char*, unsigned int, unsigned char*, unsigned int, int, int);
extern void hq4x(unsigned char*, unsigned int, unsigned char*, unsigned int, int, int);

extern void InitLUTs();
extern void RenderHQ2XS(unsigned char*, unsigned int, unsigned char*, unsigned int, int, int, int type);
extern void RenderHQ3XS(unsigned char*, unsigned int, unsigned char*, unsigned int, int, int, int type);

void RenderEPXB(unsigned char*, unsigned int, unsigned char*, unsigned int, int, int, int);
void RenderEPXC(unsigned char*, unsigned int, unsigned char*, unsigned int, int, int, int);

void ddt3x(unsigned char * src,  unsigned int srcPitch, unsigned char * dest, unsigned int dstPitch, int Xres, int Yres, int nDepth);


#include "scale2x.h"
#include "scale3x.h"

extern void eagle2x(unsigned char*, unsigned int, unsigned char*, unsigned int, int, int, int nDepth);
extern void superscale2x(unsigned char*, unsigned int, unsigned char*, unsigned int, int, int, int nDepth, bool bScan75);

#define FXF_MMX		(unsigned int)(1 << 31)

static struct { TCHAR* pszName; int nZoom; unsigned int nFlags; } SoftFXInfo[] = {
	{ _T("Plain Software Scale"),			2, 0	   },
	{ _T("AdvanceMAME Scale2x"),			2, 0	   },
	{ _T("AdvanceMAME Scale3x"),			3, 0	   },
	{ _T("2xPM LQ"),						2, 0	   },
	{ _T("2xPM HQ"),						2, 0	   },
	{ _T("Eagle Graphics"),					2, 0       },
	{ _T("SuperEagle"),						2, 0       },
	{ _T("2xSaI"),							2, 0       },
	{ _T("Super 2xSaI"),					2, 0       },
	{ _T("SuperEagle (VBA)"),				2, 0       },
	{ _T("2xSaI (VBA)"),					2, 0       },
	{ _T("Super 2xSaI (VBA)"),				2, 0       },
	{ _T("SuperScale"),						2, 0       },
	{ _T("SuperScale (75% Scanlines)"),		2, 0       },
	{ _T("hq2x Filter"),					2, 0       },
	{ _T("hq3x Filter"),					3, 0       },
	{ _T("hq4x Filter"),					4, 0       },
	{ _T("hq2xS (VBA) Filter"),				2, 0       },
	{ _T("hq3xS (VBA) Filter"),				3, 0       },
	{ _T("hq2xS (SNES9X) Filter"),			2, 0       },
	{ _T("hq3xS (SNEX9X) Filter"),			3, 0       },
	{ _T("hq2xBold Filter"),				2, 0       },
	{ _T("hq3xBold Filter"),				3, 0       },
	{ _T("EPXB Filter"),					2, 0       },
	{ _T("EPXC Filter"),					2, 0       },
	{ _T("2xBR (Squared) Filter"),			2, FXF_MMX },
	{ _T("2xBR (Semi-Rounded) Filter"),		2, FXF_MMX },
	{ _T("2xBR (Rounded) Filter"),			2, FXF_MMX },
	{ _T("3xBR (Squared) Filter"),			3, FXF_MMX },
	{ _T("3xBR (Semi-Rounded) Filter"),		3, FXF_MMX },
	{ _T("3xBR (Rounded) Filter"),			3, FXF_MMX },
	{ _T("4xBR (Squared) Filter"),			4, FXF_MMX },
	{ _T("4xBR (Semi-Rounded) Filter"),		4, FXF_MMX },
	{ _T("4xBR (Rounded) Filter"),			4, FXF_MMX },
	{ _T("DDT3x"),                          3, 0       },
	{ _T("CRT 2x2"),						2, 0       },
	{ _T("CRT 3x3"),						3, 0       },
	{ _T("CRT 4x4"),						4, 0       },
};

static unsigned char* pSoftFXImage = NULL;
static int nSoftFXImageWidth = 0;
static int nSoftFXImageHeight = 0;
static int nSoftFXImagePitch = 0;

static int nSoftFXRotate = 0;
static int nSoftFXBlitter = 0;
static bool nSoftFXEnlarge = 0;

static bool MMXSupport()
{
#if defined BUILD_X86_ASM
	unsigned int nSignatureEAX = 0, nSignatureEBX = 0, nSignatureECX = 0, nSignatureEDX = 0;

	CPUID(1, nSignatureEAX, nSignatureEBX, nSignatureECX, nSignatureEDX);

	return (nSignatureEDX >> 23) & 1;						// bit 23 of edx ndicates MMX support
#else
	#if defined (BUILD_WIN32) && defined (BUILD_X64_EXE) && !defined (_M_ARM64)
		return 1;
	#endif
	return 0;
#endif
}

TCHAR* VidSoftFXGetEffect(int nEffect)
{
	return SoftFXInfo[nEffect].pszName;
}

int VidSoftFXGetZoom(int nEffect)
{
	return SoftFXInfo[nEffect].nZoom;
}

int VidSoftFXCheckDepth(int nEffect, int nDepth)
{
	switch (nEffect) {
		case FILTER_PLAIN:
			return nDepth;
		case FILTER_ADVMAME_SCALE_2X:
		case FILTER_ADVMAME_SCALE_3X:
		case FILTER_HQ2XS_VBA:
		case FILTER_HQ3XS_VBA:
		case FILTER_EPXB:
		case FILTER_EPXC:
			if (nDepth == 15 || nDepth == 16 || nDepth == 32) {
				return nDepth;
			}
			break;
		case FILTER_DDT3X:
			if (nDepth == 15 || nDepth == 16) {
				return nDepth;
			}
			break;
		case FILTER_HQ2XS_SNES9X:
		case FILTER_HQ3XS_SNES9X:
		case FILTER_HQ2XBOLD:
		case FILTER_HQ3XBOLD:
			if (nDepth == 16) {
				return nDepth;
			}
			break;
		case FILTER_2XBR_A:
		case FILTER_2XBR_B:
		case FILTER_2XBR_C:
		case FILTER_3XBR_A:
		case FILTER_3XBR_B:
		case FILTER_3XBR_C:
		case FILTER_4XBR_A:
		case FILTER_4XBR_B:
		case FILTER_4XBR_C:
			if (nDepth == 16 || nDepth == 32) {
				return nDepth;
			}
			break;
		case FILTER_2XPM_LQ:
		case FILTER_2XPM_HQ:
		case FILTER_SUPEREAGLE:
		case FILTER_2XSAI:
		case FILTER_SUPER_2XSAI:
			if (nDepth == 15 || nDepth == 16) {
				return nDepth;
			}
			break;
		case FILTER_EAGLE:
		case FILTER_SUPERSCALE:
		case FILTER_SUPERSCALE_75SCAN:
			if (nDepth == 15 || nDepth == 16 || nDepth == 32) {
				return nDepth;
			}
			break;
		case FILTER_SUPEREAGLE_VBA:
		case FILTER_2XSAI_VBA:
		case FILTER_SUPER_2XSAI_VBA:
			if (nDepth == 15 || nDepth == 16 || nDepth == 32) {
				return nDepth;
			}
			break;
		case FILTER_HQ2X:
		case FILTER_HQ3X:
		case FILTER_HQ4X:
			if (nDepth == 15 || nDepth == 16 || nDepth == 32) {
				return nDepth;
			}
			break;

		case FILTER_CRTx22:
		case FILTER_CRTx33:
		case FILTER_CRTx44:
			if (nDepth == 32) {
				return 32;
			}
			break;
	}

	return 0;
}

void VidSoftFXExit()
{
	if (nSoftFXRotate) {
		free(pSoftFXImage);
		pSoftFXImage = NULL;
	}
	pSoftFXImage = NULL;

	nSoftFXRotate = 0;
	nSoftFXEnlarge = 0;
	nSoftFXBlitter = 0;

	return;
}

int VidSoftFXInit(int nBlitter, int nRotate)
{
	nSoftFXBlitter = nBlitter;
	nSoftFXEnlarge = true;
	
	if ((MMXSupport() == false && (SoftFXInfo[nSoftFXBlitter].nFlags & FXF_MMX)) || VidSoftFXCheckDepth(nSoftFXBlitter, nVidImageDepth) == 0) {
		VidSoftFXExit();
		return 1;
	}

	pSoftFXImage = pVidImage + nVidImageLeft * nVidImageBPP;

	nSoftFXImageWidth = nVidImageWidth; nSoftFXImageHeight = nVidImageHeight;

	nSoftFXRotate = 0;
    if (bDrvOkay) {
		BurnDrvGetFullSize(&nSoftFXImageWidth, &nSoftFXImageHeight);

		if ((nRotate & 1) && (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
			nSoftFXRotate |= 1;

			BurnDrvGetFullSize(&nSoftFXImageHeight, &nSoftFXImageWidth);
		}

		if ((nRotate & 2) && (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED)) {
			nSoftFXRotate |= 2;
		}

		if (nSoftFXRotate) {
			pSoftFXImage = (unsigned char*)malloc(nSoftFXImageWidth * nSoftFXImageHeight * nVidImageBPP);
			if (pSoftFXImage == NULL) {
				VidSoftFXExit();
				return 1;
			}
		}
	}
	nSoftFXImagePitch = nSoftFXImageWidth * nVidImageBPP;

	if (nSoftFXBlitter >= FILTER_SUPEREAGLE && nSoftFXBlitter <= FILTER_SUPER_2XSAI_VBA) {	// Initialize the 2xSaI engine
		if (nVidImageDepth == 32) {
			Init_2xSaI(565, 32);
		} else {
			Init_2xSaI16(nVidImageDepth);
		}
	}

	if (nSoftFXBlitter >= FILTER_HQ2X && nSoftFXBlitter <= FILTER_HQ4X) {
		hqx_init(nVidImageDepth);
	}
	
	if (nSoftFXBlitter >= FILTER_HQ2XS_VBA && nSoftFXBlitter <= FILTER_HQ3XS_VBA) {
                hq2xS_init(nVidImageDepth);
        }
        
        if (nSoftFXBlitter == FILTER_HQ2XS_SNES9X || nSoftFXBlitter == FILTER_HQ3XS_SNES9X || nSoftFXBlitter == FILTER_HQ2XBOLD || nSoftFXBlitter == FILTER_HQ3XBOLD) {
        	InitLUTs();
        }

#ifdef PRINT_DEBUG_INFO
   	dprintf(_T("  * SoftFX initialised: using %s in %i-bit mode.\n"), SoftFXInfo[nSoftFXBlitter].pszName, nVidImageDepth);
#endif

	return 0;
}

int VidSoftFXScale(RECT* pRect, int nGameWidth, int nGameHeight)
{
	int nWidth = pRect->right - pRect->left;
	int nHeight = pRect->bottom - pRect->top;

	nSoftFXEnlarge = false;

	if ((nWidth >= (nGameWidth * SoftFXInfo[nSoftFXBlitter].nZoom)) && (nHeight >= (nGameHeight * SoftFXInfo[nSoftFXBlitter].nZoom))) {
		nWidth = nGameWidth * SoftFXInfo[nSoftFXBlitter].nZoom;
		nHeight = nGameHeight * SoftFXInfo[nSoftFXBlitter].nZoom;
		nSoftFXEnlarge = true;
	}

	if (!nSoftFXEnlarge) {
		nWidth = nGameWidth;
		nHeight = nGameHeight;
	}

	pRect->left = (pRect->right + pRect->left) / 2;
	pRect->left -= nWidth / 2;
	pRect->right = pRect->left + nWidth;

	pRect->top = (pRect->bottom + pRect->top) / 2;
	pRect->top -= nHeight / 2;
	pRect->bottom = pRect->top + nHeight;

	return 0;
}

// Plain pixel replication, for images too small for a filter
static void VidSoftFXNearest(unsigned char* ps, unsigned char* pd, int nPitch, int nZoom)
{
	for (int y = 0; y < nSoftFXImageHeight * nZoom; y++) {
		unsigned char* s = ps + (y / nZoom) * nSoftFXImagePitch;
		unsigned char* d = pd + y * nPitch;
		for (int x = 0; x < nSoftFXImageWidth * nZoom; x++) {
			memcpy(d + x * nVidImageBPP, s + (x / nZoom) * nVidImageBPP, nVidImageBPP);
		}
	}
}

static void VidSoftFXRotate()
{
	if (nSoftFXRotate) {
		unsigned char* ps;
		unsigned char* pd = pSoftFXImage;
		if (nSoftFXRotate & 2) {
			pd += nSoftFXImageHeight * nSoftFXImagePitch - nVidImageBPP;
		}

		switch (nVidImageBPP) {
			case 4:	{
				switch (nSoftFXRotate) {
					case 1: {
						for (int y = 0; y < nSoftFXImageHeight; y++) {
							ps = pVidImage + (nSoftFXImageHeight + nVidImageLeft - 1 - y) * 4;
							for (int x = 0; x < nSoftFXImageWidth; x++) {
								*(int*)pd = *(int*)ps;
								ps += nVidImagePitch;
								pd += 4;
							}
						}
						break;
					}
					case 2: {
						for (int y = 0; y < nSoftFXImageHeight; y++) {
							ps = pVidImage + y * nVidImagePitch + nVidImageLeft * 2;
							for (int x = 0; x < nSoftFXImageWidth; x++) {
								*(int*)pd = *(int*)ps;
								ps += 4;
								pd -= 4;
							}
						}
						break;
					}
					case 3: {
						for (int y = 0; y <nSoftFXImageHeight; y++) {
							ps = pVidImage + (nSoftFXImageHeight + nVidImageLeft - 1 - y) * 4;
							for (int x = 0; x < nSoftFXImageWidth; x++) {
								*(int*)pd = *(int*)ps;
								ps += nVidImagePitch;
								pd -= 4;
							}
						}
						break;
					}
				}
				break;
			}

			case 3: {
				switch (nSoftFXRotate) {
					case 1: {
						for (int y = 0; y < nSoftFXImageHeight; y++) {
							ps = pVidImage + (nSoftFXImageHeight + nVidImageLeft - 1 - y) * 3;
							for (int x = 0; x < nSoftFXImageWidth; x++) {
								pd[0] = ps[0];
								pd[1] = ps[2];
								pd[2] = ps[2];
								ps += nVidImagePitch;
								pd += 3;
							}
						}
						break;
					}
					case 2: {
						for (int y = 0; y < nSoftFXImageHeight; y++) {
							ps = pVidImage + y * nVidImagePitch + nVidImageLeft * 3;
							for (int x = 0; x < nSoftFXImageWidth; x++) {
								pd[0] = ps[0];
								pd[1] = ps[2];
								pd[2] = ps[2];
								ps += 3;
								pd -= 3;
							}
						}
						break;
					}
					case 3: {
						for (int y = 0; y < nSoftFXImageHeight; y++) {
							ps = pVidImage + (nSoftFXImageHeight + nVidImageLeft - 1 - y) * 3;
							for (int x = 0; x < nSoftFXImageWidth; x++) {
								pd[0] = ps[0];
								pd[1] = ps[2];
								pd[2] = ps[2];
								ps += nVidImagePitch;
								pd -= 3;
							}
						}
						break;
					}
				}
				break;
			}

			case 2:	{
				switch (nSoftFXRotate) {
					case 1: {
						for (int y = 0; y < nSoftFXImageHeight; y++) {
							ps = pVidImage + (nSoftFXImageHeight + nVidImageLeft - 1 - y) * 2;
							for (int x = 0; x < nSoftFXImageWidth; x++) {
								*(short*)pd = *(short*)ps;
								ps += nVidImagePitch;
								pd += 2;
							}
						}
						break;
					}
					case 2: {
						for (int y = 0; y < nSoftFXImageHeight; y++) {
							ps = pVidImage + y * nVidImagePitch + nVidImageLeft * 2;
							for (int x = 0; x < nSoftFXImageWidth; x++) {
								*(short*)pd = *(short*)ps;
								ps += 2;
								pd -= 2;
							}
						}
						break;
					}
					case 3: {
						for (int y = 0; y < nSoftFXImageHeight; y++) {
							ps = pVidImage + (nSoftFXImageHeight + nVidImageLeft - 1 - y) * 2;
							for (int x = 0; x < nSoftFXImageWidth; x++) {
								*(short*)pd = *(short*)ps;
								ps += nVidImagePitch;
								pd -= 2;
							}
						}
						break;
					}
				}
				break;
			}
		}
	}
}

void VidSoftFXApplyEffect(unsigned char* ps, unsigned char* pd, int nPitch)
{
	// Apply effects to the image
	switch (nSoftFXBlitter) {

		case FILTER_PLAIN: {											// Software 2x zoom
			if (nVidImageBPP == 2) {						// 15/16-bit
				for (int y = 0; y < nSoftFXImageHeight; y++, pd += (nPitch << 1), ps += nSoftFXImagePitch) {
					unsigned short* psEnd = (unsigned short*)(ps + nSoftFXImagePitch);
					unsigned short* pdpc = (unsigned short*)pd;
					unsigned short* pdpn = (unsigned short*)(pd + nPitch);
					unsigned short* psp = (unsigned short*)ps;
					do {
						*pdpc++ = *psp;
						*pdpc++ = *psp;
						*pdpn++ = *psp;
						*pdpn++ = *psp++;
					} while (psp < psEnd);
				}
			} else {										// 32-bit
				if (nVidImageBPP == 4) {
					for (int y = 0; y < nSoftFXImageHeight; y++, pd += (nPitch << 1), ps += nSoftFXImagePitch) {
						unsigned int* psEnd = (unsigned int*)(ps + nSoftFXImagePitch);
						unsigned int* pdpc = (unsigned int*)pd;
						unsigned int* pdpn = (unsigned int*)(pd + nPitch);
						unsigned int* psp = (unsigned int*)ps;
						do {
							*pdpc++ = *psp;
							*pdpc++ = *psp;
							*pdpn++ = *psp;
							*pdpn++ = *psp++;
						} while (psp < psEnd);
					}
				} else {									// 24-bit
					for (int y = 0; y < nSoftFXImageHeight; y++, pd += (nPitch << 1), ps += nSoftFXImagePitch) {
						unsigned char* psEnd = (unsigned char*)(ps + nSoftFXImagePitch);
						unsigned char* pdpc = (unsigned char*)pd;
						unsigned char* pdpn = (unsigned char*)(pd + nPitch);
						unsigned char* psp = (unsigned char*)ps;
						do {
							pdpc[0] = psp[0];
							pdpc[3] = psp[0];
							pdpn[0] = psp[0];
							pdpn[3] = psp[0];
							pdpc[1] = psp[1];
							pdpc[4] = psp[1];
							pdpn[1] = psp[1];
							pdpn[4] = psp[1];
							pdpc[2] = psp[2];
							pdpc[5] = psp[2];
							pdpn[2] = psp[2];
							pdpn[5] = psp[2];
							psp += 3;
							pdpc += 3;
							pdpn += 3;
						} while (psp < psEnd);
					}
				}
			}
			break;
		}

		case FILTER_ADVMAME_SCALE_2X: {											// AdvanceMAME Scale2x blitter (15/16/32BPP)
			if (nSoftFXImageWidth < 2 || nSoftFXImageHeight < 2) {			// (the scale2x code needs 2+ pixels each way)
				VidSoftFXNearest(ps, pd, nPitch, 2);
				break;
			}
			unsigned char* psp = pSoftFXImage;
			unsigned char* psc = pSoftFXImage;
			unsigned char* psn = pSoftFXImage + nSoftFXImagePitch;
			if (nVidImageBPP == 2) {
				scale2x_16_def((scale2x_uint16*)pd, (scale2x_uint16*)(pd + nPitch), (scale2x_uint16*)psp, (scale2x_uint16*)psc, (scale2x_uint16*)psn, nSoftFXImageWidth);
			} else {
				scale2x_32_def((scale2x_uint32*)pd, (scale2x_uint32*)(pd + nPitch), (scale2x_uint32*)psp, (scale2x_uint32*)psc, (scale2x_uint32*)psn, nSoftFXImageWidth);
			}
			psp -= nSoftFXImagePitch;

			if (nVidImageBPP == 2) {
				for (int y = 2; y < nSoftFXImageHeight; y++) {
					pd += (nPitch << 1);
					psp += nSoftFXImagePitch;
					psc += nSoftFXImagePitch;
					psn += nSoftFXImagePitch;
					scale2x_16_def((scale2x_uint16*)pd, (scale2x_uint16*)(pd + nPitch), (scale2x_uint16*)psp, (scale2x_uint16*)psc, (scale2x_uint16*)psn, nSoftFXImageWidth);
				}
			} else {
				for (int y = 2; y < nSoftFXImageHeight; y++) {
					pd += (nPitch << 1);
					psp += nSoftFXImagePitch;
					psc += nSoftFXImagePitch;
					psn += nSoftFXImagePitch;
					scale2x_32_def((scale2x_uint32*)pd, (scale2x_uint32*)(pd + nPitch), (scale2x_uint32*)psp, (scale2x_uint32*)psc, (scale2x_uint32*)psn, nSoftFXImageWidth);
				}
			}

			pd += (nPitch << 1);
			psp += nSoftFXImagePitch;
			psc += nSoftFXImagePitch;
			if (nVidImageBPP == 2) {
				scale2x_16_def((scale2x_uint16*)pd, (scale2x_uint16*)(pd + nPitch), (scale2x_uint16*)psp, (scale2x_uint16*)psc, (scale2x_uint16*)psn, nSoftFXImageWidth);
			} else {
				scale2x_32_def((scale2x_uint32*)pd, (scale2x_uint32*)(pd + nPitch), (scale2x_uint32*)psp, (scale2x_uint32*)psc, (scale2x_uint32*)psn, nSoftFXImageWidth);
			}

			break;
		}

		case FILTER_ADVMAME_SCALE_3X: {											// AdvanceMAME Scale3x blitter (15/16/32BPP)
			if (nSoftFXImageWidth < 2 || nSoftFXImageHeight < 2) {			// (the scale3x code needs 2+ pixels each way)
				VidSoftFXNearest(ps, pd, nPitch, 3);
				break;
			}
			unsigned char* src_prev = pSoftFXImage;
			unsigned char* src_curr = pSoftFXImage;
			unsigned char* src_next = pSoftFXImage + nSoftFXImagePitch;

			if (nVidImageBPP == 2) {
				scale3x_16_def((scale3x_uint16*)pd, (scale3x_uint16*)(pd + nPitch), (scale3x_uint16*)(pd + 2 * nPitch), (scale3x_uint16*)src_prev, (scale3x_uint16*)src_curr, (scale3x_uint16*)src_next, nSoftFXImageWidth);
			} else {
				scale3x_32_def((scale3x_uint32*)pd, (scale3x_uint32*)(pd + nPitch), (scale3x_uint32*)(pd + 2 * nPitch), (scale3x_uint32*)src_prev, (scale3x_uint32*)src_curr, (scale3x_uint32*)src_next, nSoftFXImageWidth);
			}

			if (nVidImageBPP == 2) {
				for (int y = 2; y < nSoftFXImageHeight; y++) {
					pd += 3 * nPitch;
					src_prev = src_curr;
					src_curr = src_next;
					src_next += nSoftFXImagePitch;
					scale3x_16_def((scale3x_uint16*)pd, (scale3x_uint16*)(pd + nPitch), (scale3x_uint16*)(pd + 2 * nPitch), (scale3x_uint16*)src_prev, (scale3x_uint16*)src_curr, (scale3x_uint16*)src_next, nSoftFXImageWidth);
				}
			} else {
				for (int y = 2; y < nSoftFXImageHeight; y++) {
					pd += 3 * nPitch;
					src_prev = src_curr;
					src_curr = src_next;
					src_next += nSoftFXImagePitch;
					scale3x_32_def((scale3x_uint32*)pd, (scale3x_uint32*)(pd + nPitch), (scale3x_uint32*)(pd + 2 * nPitch), (scale3x_uint32*)src_prev, (scale3x_uint32*)src_curr, (scale3x_uint32*)src_next, nSoftFXImageWidth);
				}
			}

			pd += 3 * nPitch;
			src_prev = src_curr;
			src_curr = src_next;
			if (nVidImageBPP == 2) {
				scale3x_16_def((scale3x_uint16*)pd, (scale3x_uint16*)(pd + nPitch), (scale3x_uint16*)(pd + 2 * nPitch), (scale3x_uint16*)src_prev, (scale3x_uint16*)src_curr, (scale3x_uint16*)src_next, nSoftFXImageWidth);
			} else {
				scale3x_32_def((scale3x_uint32*)pd, (scale3x_uint32*)(pd + nPitch), (scale3x_uint32*)(pd + 2 * nPitch), (scale3x_uint32*)src_prev, (scale3x_uint32*)src_curr, (scale3x_uint32*)src_next, nSoftFXImageWidth);
			}

			break;
		}
		case FILTER_HQ2X: {											// hq2x filter (15/16/32BPP)
			hq2x(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			break;
		}
		case FILTER_HQ3X: {											// hq3x filter (15/16/32BPP)
			hq3x(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			break;
		}
		case FILTER_HQ4X: {											// hq4x filter (15/16/32BPP)
			hq4x(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			break;
		}
		case FILTER_SUPEREAGLE:											// Super Eagle blitter (15/16BPP only)
		case FILTER_SUPEREAGLE_VBA: {									// Super Eagle blitter (15/16/32BPP only)
			if (nVidImageDepth == 32) {
				SuperEagle32(ps, nSoftFXImagePitch, NULL, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				SuperEagle16(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_2XSAI:												// 2xSaI blitter (15/16BPP only)
		case FILTER_2XSAI_VBA: {										// 2xSaI blitter (15/16/32BPP only)
			if (nVidImageDepth == 32) {
				_2xSaI32(ps, nSoftFXImagePitch, NULL, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				_2xSaI16(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_SUPER_2XSAI:										// Super 2xSaI blitter (15/16BPP only)
		case FILTER_SUPER_2XSAI_VBA: {									// Super 2xSaI blitter (15/16/32BPP only)
			if (nVidImageDepth == 32) {
				Super2xSaI32(ps, nSoftFXImagePitch, NULL, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				Super2xSaI16(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_EAGLE: {											// Eagle blitter (15/16/32BPP)
			eagle2x(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight, nVidImageDepth);
			break;
		}
		case FILTER_SUPERSCALE: {										// SuperScale blitter (15/16/32BPP)
			superscale2x(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight, nVidImageDepth, false);
			break;
		}
		case FILTER_SUPERSCALE_75SCAN: {								// SuperScale with 75% scanlines (15/16/32BPP)
			superscale2x(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight, nVidImageDepth, true);
			break;
		}
		case FILTER_2XPM_LQ: {
			_2xpm_lq(ps, pd, (unsigned long)nSoftFXImagePitch, (unsigned long)nPitch, (unsigned long)nSoftFXImageWidth, (unsigned long)nSoftFXImageHeight, nVidImageDepth);
			break;			
		}
		case FILTER_2XPM_HQ: {
			_2xpm_hq(ps, pd, (unsigned long)nSoftFXImagePitch, (unsigned long)nPitch, (unsigned long)nSoftFXImageWidth, (unsigned long)nSoftFXImageHeight, nVidImageDepth);
			break;			
		}
		case FILTER_HQ2XS_VBA: {                                                                                      // hq2xS filter (15/16/32BPP)
			if (nVidImageDepth == 32) {
				hq2xS32(ps, nSoftFXImagePitch, NULL, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				hq2xS(ps, nSoftFXImagePitch, NULL, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_HQ3XS_VBA: {									// hq3xS filter (15/16/32BPP)
			if (nVidImageDepth == 32) {
				hq3xS32(ps, nSoftFXImagePitch, NULL, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				hq3xS(ps, nSoftFXImagePitch, NULL, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_HQ2XS_SNES9X: {
			RenderHQ2XS(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight, 0);
			break;
		}
		case FILTER_HQ3XS_SNES9X: {
			RenderHQ3XS(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight, 0);
			break;
		}
		case FILTER_HQ2XBOLD: {
			RenderHQ2XS(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight, 1);
			break;
		}
		case FILTER_HQ3XBOLD: {
			RenderHQ3XS(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight, 1);
			break;
		}
		case FILTER_EPXB: {
			RenderEPXB(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight, nVidImageDepth);
			break;
		}
		case FILTER_EPXC: {
			RenderEPXC(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight, nVidImageDepth);
			break;
		}
		case FILTER_2XBR_A: {
			if (nVidImageBPP == 4) {
				xbr2x_32(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				xbr2x_a(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_2XBR_B: {
			if (nVidImageBPP == 4) {
				xbr2x_32(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				xbr2x_b(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_2XBR_C: {
			if (nVidImageBPP == 4) {
				xbr2x_32(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				xbr2x_c(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_3XBR_A: {
			if (nVidImageBPP == 4) {
				xbr3x_32(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				xbr3x_a(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_3XBR_B: {
			if (nVidImageBPP == 4) {
				xbr3x_32(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				xbr3x_b(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_3XBR_C: {
			if (nVidImageBPP == 4) {
				xbr3x_32(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				xbr3x_c(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_4XBR_A: {
			if (nVidImageBPP == 4) {
				xbr4x_32(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				xbr4x_a(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_4XBR_B: {
			if (nVidImageBPP == 4) {
				xbr4x_32(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				xbr4x_b(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_4XBR_C: {
			if (nVidImageBPP == 4) {
				xbr4x_32(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			} else {
				xbr4x_c(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight);
			}
			break;
		}
		case FILTER_DDT3X: {
			ddt3x(ps, nSoftFXImagePitch, pd, nPitch, nSoftFXImageWidth, nSoftFXImageHeight, nVidImageDepth);
			break;
		}
		case FILTER_CRTx22: {
			CRTx22fast(ps,pd,nSoftFXImageWidth,nSoftFXImageHeight, nSoftFXImagePitch, nPitch);
			break;
		}
		case FILTER_CRTx33: {
			CRTx33(ps,pd,nSoftFXImageWidth,nSoftFXImageHeight, nSoftFXImagePitch, nPitch);
			break;
		}
		case FILTER_CRTx44: {
			CRTx44(ps,pd,nSoftFXImageWidth,nSoftFXImageHeight, nSoftFXImagePitch, nPitch);
			break;
		}
	}
}

#ifdef SOFTFX_ENABLE_DIRECTX

int VidSoftFXApplyEffectDirectX(IDirectDrawSurface7* pSurf, RECT* pRect)
{
	DDSURFACEDESC2 ddsd;

	VidSoftFXRotate();

	// Lock the surface so we can write to it
	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	if (FAILED(pSurf->Lock(pRect, &ddsd, DDLOCK_SURFACEMEMORYPTR | DDLOCK_WAIT, NULL))) {
		return 1;
	}

	VidSoftFXApplyEffect(pSoftFXImage, (unsigned char*)ddsd.lpSurface, ddsd.lPitch);

	pSurf->Unlock(NULL);

	return 0;
}

#endif

#ifdef SOFTFX_ENABLE_SDL

int VidSoftFXApplyEffectSDL(SDL_Surface* pSurf)
{
	VidSoftFXRotate();

	// Lock the surface so we can write to it
	if (SDL_LockSurface(pSurf)) {
		return 1;
	}

	VidSoftFXApplyEffect(pSoftFXImage, (unsigned char*)pSurf->pixels, pSurf->pitch);

	SDL_UnlockSurface(pSurf);

	return 0;
}

#endif

int VidFilterApplyEffect(unsigned char* pd, int pitch)
{
	if (!pd) {
		return 1;
	}

	VidSoftFXRotate();
	VidSoftFXApplyEffect(pSoftFXImage, pd, pitch);

	return 0;
}
