// Software blitter effects via SDL

#ifdef BUILD_SDL

#include "burner.h"
#include "vid_support.h"
#include "vid_filter.h"

#ifdef _MSC_VER
#pragma comment(lib, "SDL")
#pragma comment(lib, "SDLmain")
#endif

static int nInitedSubsytems = 0;

SDL_Surface* sdlsBlitFX[2] = {NULL, };				// The image surfaces
SDL_Surface* sdlsFramebuf = NULL;

static int nSize;
static int nUseBlitter;

static int nUseSys = 0;
static int nDirectAccess = 1;

static int PrimClear()
{
	SDL_FillRect(sdlsFramebuf, NULL, 0);

	if (nVidFullscreen) {
		SDL_Flip(sdlsFramebuf);
		SDL_FillRect(sdlsFramebuf, NULL, 0);
	}

	return 0;
}

// Create a secondary DD surface for the screen
static int BlitFXMakeSurf()
{
	sdlsBlitFX[0] = NULL;
	sdlsBlitFX[1] = NULL;

	// Try to allocate buffer in Video memory if required, always allocate one in System memory
	if (nUseSys == 0) {
		sdlsBlitFX[0] = SDL_CreateRGBSurface(SDL_HWSURFACE, nGameWidth * nSize, nGameHeight * nSize, sdlsFramebuf->format->BitsPerPixel, sdlsFramebuf->format->Rmask, sdlsFramebuf->format->Gmask, sdlsFramebuf->format->Bmask, sdlsFramebuf->format->Amask);
		if (sdlsBlitFX[0] == NULL) {
			nDirectAccess = 0;
			nUseSys = 1;
		}
	}

	if (nDirectAccess == 0) {
		sdlsBlitFX[1] = SDL_CreateRGBSurface(SDL_SWSURFACE, nGameWidth * nSize, nGameHeight * nSize, sdlsFramebuf->format->BitsPerPixel, sdlsFramebuf->format->Rmask, sdlsFramebuf->format->Gmask, sdlsFramebuf->format->Bmask, sdlsFramebuf->format->Amask);
		if (sdlsBlitFX[1] == NULL) {
			return 1;
		}
	}

	SDL_FillRect(sdlsBlitFX[1 ^ nDirectAccess], NULL, 0);

	return 0;
}

static int BlitFXExit()
{
	SDL_FreeSurface(sdlsBlitFX[0]);
	sdlsBlitFX[0] = NULL;
	SDL_FreeSurface(sdlsBlitFX[1]);
	sdlsBlitFX[1] = NULL;

	VidSFreeVidImage();

	nRotateGame = 0;

	return 0;
}

static int BlitFXInit()
{
	if (nRotateGame & 1) {
		nVidImageWidth = nGameHeight;
		nVidImageHeight = nGameWidth;
	} else {
		nVidImageWidth = nGameWidth;
		nVidImageHeight = nGameHeight;
	}

//	if (nUseBlitter == FILTER_HQ2X || nUseBlitter == FILTER_HQ3X) {
//		nVidImageDepth = 16;								// Use 565 format
//	} else
	{
		nVidImageDepth = sdlsFramebuf->format->BitsPerPixel;// Use color depth of primary surface
	}
	nVidImageBPP = sdlsFramebuf->format->BytesPerPixel;
	nBurnBpp = nVidImageBPP;								// Set Burn library Bytes per pixel

	// Use our callback to get colors:
	SetBurnHighCol(nVidImageDepth);

	// Make the normal memory buffer
	if (VidSAllocVidImage()) {
		BlitFXExit();
		return 1;
	}

	if (BlitFXMakeSurf()) {
		BlitFXExit();
		return 1;
	}

	return 0;
}

static int Exit()
{
	BlitFXExit();

	if (!(nInitedSubsytems & SDL_INIT_VIDEO)) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
	nInitedSubsytems = 0;

	return 0;
}

static int Init()
{
	nInitedSubsytems = SDL_WasInit(SDL_INIT_VIDEO);

	if (!(nInitedSubsytems & SDL_INIT_VIDEO)) {
		SDL_InitSubSystem(SDL_INIT_VIDEO);
	}

	nUseBlitter = nVidFilter;//nVidBlitterOpt[nVidSelect] & 0xFF;

	VidInitInfo();

	nSize = VidFilterGetZoom(nUseBlitter);

	if (nVidFullscreen) {

		nVidScrnWidth = nVidWidth; nVidScrnHeight = nVidHeight;

		if ((sdlsFramebuf = SDL_SetVideoMode(nVidWidth, nVidHeight, nVidDepth, SDL_HWSURFACE | SDL_ANYFORMAT | SDL_DOUBLEBUF | SDL_FULLSCREEN)) == NULL) {
			dprintf(_T("*** Couldn't enter fullscreen mode.\n"));
			return 1;
		}
	} else {
		if ((sdlsFramebuf = SDL_SetVideoMode(nGameWidth * nSize, nGameHeight * nSize, 0, SDL_RESIZABLE | SDL_HWSURFACE)) == NULL) {
			return 1;
		}
	}

	SDL_SetClipRect(sdlsFramebuf, NULL);

	// Initialize the buffer surfaces
	BlitFXInit();

	if (VidFilterInit(nUseBlitter, nRotateGame)) {
		if (VidFilterInit(0, nRotateGame)) {
			Exit();
			return 1;
		}
	}

	return 0;
}

static int vidScale(RECT* pRect, int nWidth, int nHeight)
{
	if (vidUseFilter && vidForceFilterSize) {
		return VidFilterScale(pRect, nWidth, nHeight);
	}

	return VidSScaleImage(pRect, nWidth, nHeight);
}

static int vidFilterApplyEffect(SDL_Surface* pSurf)
{
	// Lock the surface so we can write to it
	if (SDL_LockSurface(pSurf)) {
		return 1;
	}

	VidFilterApplyEffect((unsigned char*)pSurf->pixels, pSurf->pitch);

	SDL_UnlockSurface(pSurf);

	return 0;
}

static int MemToSurf()
{
	vidFilterApplyEffect(sdlsBlitFX[1 ^ nDirectAccess]);

	if (nUseSys == 0 && nDirectAccess == 0) {

		// Lock the surface so we can write to it
		if (SDL_LockSurface(sdlsBlitFX[1])) {
			return 1;
		}
		unsigned char* Surf = (unsigned char*)sdlsBlitFX[1]->pixels;
		int nPitch = sdlsBlitFX[1]->pitch;

		// Lock the surface so we can write to it
		if (SDL_LockSurface(sdlsBlitFX[0])) {
			return 1;
		}
		unsigned char* VidSurf = (unsigned char*)sdlsBlitFX[0]->pixels;
		int nVidPitch = sdlsBlitFX[0]->pitch;

		unsigned char *pd, *ps;

		int nHeight = nGameHeight * nSize;

		pd = VidSurf; ps = Surf;
		for (int y = 0; y < nHeight; y++, pd += nVidPitch, ps += nPitch) {
			memcpy(pd, ps, nPitch);
		}

		SDL_UnlockSurface(sdlsBlitFX[0]);
		SDL_UnlockSurface(sdlsBlitFX[1]);
	}

	return 0;
}

// Run one frame and render the screen
static int Frame(bool bRedraw)						// bRedraw = 0
{
	if (pVidImage == NULL) {
		return 1;
	}

	if (bDrvOkay) {
		if (bRedraw) {								// Redraw current frame
			if (BurnDrvRedraw()) {
				BurnDrvFrame();						// No redraw function provided, advance one frame
			}
		} else {
			BurnDrvFrame();							// Run one frame and draw the screen
		}
	}

	MemToSurf();									// Copy the memory buffer to the directdraw buffer for later blitting

	return 0;
}

// Paint the BlitFX surface onto the primary surface
static int Paint(int bValidate)
{
	SDL_Rect sdlrDest = { 0, 0, nGameWidth * nSize, nGameHeight * nSize };

	if (bValidate & 2) {
		MemToSurf();									// Copy the memory buffer to the directdraw buffer for later blitting
	}

	if (nVidFullscreen) {
		// Double bufferring

		sdlrDest.x = (nVidScrnWidth - nGameWidth * nSize) / 2;
		sdlrDest.y = (nVidScrnHeight - nGameHeight * nSize) / 2;

		if (SDL_BlitSurface(sdlsBlitFX[nUseSys], NULL, sdlsFramebuf, &sdlrDest)) {
			return 1;
		}
		SDL_Flip(sdlsFramebuf);
	} else {
		// Normal

		if (SDL_BlitSurface(sdlsBlitFX[nUseSys], NULL, sdlsFramebuf, &sdlrDest)) {
			return 1;
		}
		SDL_UpdateRect(sdlsFramebuf, 0, 0, 0, 0);
	}

	return 0;
}

static int GetSettings(InterfaceInfo* pInfo)
{
	TCHAR szString[MAX_PATH] = _T("");

	if (nRotateGame) {
		IntInfoAddStringModule(pInfo, _T("Using software rotation"));
	}

	return 0;
}

// The Video Output plugin:
struct VidOut VidOutSDLFX = { Init, Exit, Frame, Paint, vidScale, GetSettings };

#endif
