//#define DIRECTDRAW_VERSION 7				// Use this DirectDraw version
#include <ddraw.h>
#include "vid_support.h"

// General
void VidSExit();
int VidSInit(IDirectDraw7* pDD7);

// Miscellaneous
int VidSGetSurfaceDepth(IDirectDrawSurface7* pSurf);
int VidSClearSurface(IDirectDrawSurface7* pSurf, unsigned int nColour, RECT* pRect);

// Clipper
int VidSClipperInit(IDirectDrawSurface7* pSurf);

// Gamma controls
void VidSRestoreGamma();
int VidSUpdateGamma();
int VidSSetupGamma(IDirectDrawSurface7* pSurf);

// Select optimal full-screen resolution
struct VidSDisplayScoreInfo {
	// Filled in advance by the caller
	unsigned int nRequestedWidth; unsigned int nRequestedHeight; unsigned int nRequestedDepth; unsigned int nRequestedZoom;
	unsigned int nModeWidth; unsigned int nModeHeight; unsigned int nModeDepth;
	// Filled by the scoring routine
	unsigned int nBestWidth; unsigned int nBestHeight; unsigned int nBestDepth;
	unsigned int nBestScore;
};

int VidSScoreDisplayMode(VidSDisplayScoreInfo* pScoreInfo);
int VidSInitScoreInfo(VidSDisplayScoreInfo* pScoreInfo);

void VidSRestoreScreenMode();
int VidSEnterFullscreenMode(int nZoom, int nDepth);

// Image size
int VidSScaleImage(RECT* pRect, int nGameWidth, int nGameHeight, bool bVertScanlines);
bool VidSGetArcaderes(int* pWidth, int* pHeight);

// Text display
// OSD backend: lets blitters that don't use DirectDraw (e.g. Direct3D 11) host the OSD.
// Surfaces are drawn with GDI (pGetDC/pReleaseDC) and blitted using a colour key (0x000001).
struct VidSOSDBackend {
	void* (*pCreateSurface)(int nWidth, int nHeight);
	void  (*pReleaseSurface)(void* pSurf);
	HDC   (*pGetDC)(void* pSurf);
	void  (*pReleaseDC)(void* pSurf, HDC hDC);
	void  (*pFill)(void* pSurf, unsigned int nColour, RECT* pRect);
	void  (*pBlit)(RECT* pDest, void* pSurf, RECT* pSrc);			// colour-keyed blit onto the blitter's render target
};
void VidSSetOSDBackend(VidSOSDBackend* pBackend);					// NULL = use DirectDraw surfaces (VidSInit)

// pSurf may be NULL when an OSD backend is set
void VidSExitOSD();
int VidSInitOSD(int nFlags);
int VidSRestoreOSD();
void VidSDisplayOSD(IDirectDrawSurface7* pSurf, RECT* pRect, int nFlags);

UINT8 VidSGetnStatus();
TCHAR *VidSGetStatus();
int VidSGetnZoom();

void VidSDisplayCheckTimers();
