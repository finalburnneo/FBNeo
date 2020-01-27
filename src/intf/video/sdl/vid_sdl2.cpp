// blitter effects via SDL2
#include "burner.h"
#include "vid_support.h"
#include "vid_softfx.h"

#include <SDL.h>
#include <SDL_image.h>

extern int vsync;
extern char videofiltering[3];

static unsigned char* VidMem = NULL;
static SDL_Window* sdlWindow = NULL;
SDL_Renderer* sdlRenderer = NULL;
static SDL_Texture* sdlTexture = NULL;
static SDL_Texture* overlayTexture = NULL;
static int  nRotateGame = 0;
static bool bFlipped = false;
static SDL_Rect dstrect;
static SDL_Rect title_texture_rect;
static SDL_Rect dest_title_texture_rect;
static bool bHasOverlay = false;

static int screenh, screenw;

SDL_Texture* LoadOverlayImage(SDL_Renderer* renderer, SDL_Texture* loadedTexture)
{
	char titlePath[MAX_PATH] = { 0 };
	int overlaywidth, overlayheight;

	if (!bAppFullscreen)
	{
		return NULL;
	}

#ifndef _WIN32
	snprintf(titlePath, MAX_PATH, "%s%s.png", "/usr/local/share/overlays/", BurnDrvGetTextA(0));
#else
	snprintf(titlePath, MAX_PATH, "support\\overlays\\%s.png", BurnDrvGetTextA(0));
#endif
	printf("path %s\n", titlePath);
	loadedTexture = IMG_LoadTexture(renderer, titlePath);
	SDL_QueryTexture(loadedTexture, NULL, NULL, &overlaywidth, &overlayheight);

	SDL_GetWindowSize(sdlWindow,
		&screenw,
		&screenh);

	title_texture_rect.x = 0; //the x coordinate
	title_texture_rect.y = 0; // the y coordinate
	title_texture_rect.w = overlaywidth; //the width of the texture
	title_texture_rect.h = overlayheight; //the height of the texture

	dest_title_texture_rect.x = (screenw - overlaywidth) / 2; //the x coordinate
	dest_title_texture_rect.y = (screenh - overlayheight) / 2; // the y coordinate
	dest_title_texture_rect.w = overlaywidth; //the width of the texture
	dest_title_texture_rect.h = overlayheight; //the height of the texture

#ifndef _WIN32
	snprintf(titlePath, MAX_PATH, "%s%s.cfg", "/usr/local/share/overlays/", BurnDrvGetTextA(0));
#else
	snprintf(titlePath, MAX_PATH, "support\\overlays\\%s.cfg", BurnDrvGetTextA(0));
#endif

	FILE*  h;
	TCHAR  szLine[1024];
	TCHAR* t;
	TCHAR* s;
	TCHAR* szQuote;

	int destx = 0, desty = 0 , destw = 0, desth = 0;
	int    length;
	h = _tfopen(titlePath, _T("rt"));
	if (h == NULL) {
		printf("overlay config %s not found \n" ,titlePath );
		return NULL;
	}

	while (1) {
		if (_fgetts(szLine, sizeof(szLine), h) == NULL) {
			break;
		}

		length = _tcslen(szLine);
		// get rid of the linefeed at the end
		while (length && (szLine[length - 1] == _T('\r') || szLine[length - 1] == _T('\n'))) {
			szLine[length - 1] = 0;
			length--;
		}

		s = szLine;
		if ((t = LabelCheck(s, _T("custom_viewport_height"))) != 0) {
			s = t;
			FIND_QT(s)
			QuoteRead(&szQuote, NULL, s);
			printf("h: %s, %s , %s \n",szQuote, t, s);
			desth = _tcstol(szQuote, &t, 10);
			continue;
		}
		if ((t = LabelCheck(s, _T("custom_viewport_width"))) != 0) {
			s = t;
			FIND_QT(s)
			QuoteRead(&szQuote, NULL, s);
			destw = _tcstol(szQuote, &t, 10);
			continue;
		}
		if ((t = LabelCheck(s, _T("custom_viewport_x"))) != 0) {
			s = t;
			FIND_QT(s)
			QuoteRead(&szQuote, NULL, s);
			destx = _tcstol(szQuote, &t, 10);
			continue;
		}
		if ((t = LabelCheck(s, _T("custom_viewport_y"))) != 0) {
			s = t;
			FIND_QT(s)
			QuoteRead(&szQuote, NULL, s);
			desty = _tcstol(szQuote, &t, 10);
			continue;
		}
	}

	fclose(h);
	if (destx && desty && destw && desth)
	{
		bHasOverlay = true;
		dstrect.y = desty;
		dstrect.x = destx;
		dstrect.h = desth;
		dstrect.w = destw;

		if (nRotateGame)
		{
			dstrect.y = desty + (desty/2);
			dstrect.x = destx - (destx/2);
			dstrect.h = destw;
			dstrect.w = desth;

		}
	}

	return loadedTexture;
}


void RenderMessage()
{
	// First render anything that is key-held based
	if (bAppDoFast)
	{
		inprint_shadowed(sdlRenderer, "FFWD", 10, 10);
	}
	if (bAppShowFPS)
	{
		inprint_shadowed(sdlRenderer, fpsstring, 10, 50);
	}

	if (messageFrames > 1)
	{
		inprint_shadowed(sdlRenderer, lastMessage, 10, 30);
		messageFrames--;
	}
}

static int Exit()
{
	kill_inline_font(); //TODO: This is not supposed to be here
	SDL_DestroyTexture(overlayTexture);
	SDL_DestroyTexture(sdlTexture);
	SDL_DestroyRenderer(sdlRenderer);
	SDL_DestroyWindow(sdlWindow);

	free(VidMem);
	return 0;
}
static int display_w = 400, display_h = 300;


static int Init()
{
	int nMemLen = 0;
	int GameAspectX = 4, GameAspectY = 3;
	bHasOverlay = false;

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		printf("vid init error\n");
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
		return 3;
	}

	nRotateGame = 0;

	if (bDrvOkay)
	{
		// Get the game screen size
		BurnDrvGetVisibleSize(&nVidImageWidth, &nVidImageHeight);
		BurnDrvGetAspect(&GameAspectX, &GameAspectY);

		display_w = nVidImageWidth;
		display_h = nVidImageWidth * GameAspectY / GameAspectX;

		if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)
		{
			BurnDrvGetVisibleSize(&nVidImageHeight, &nVidImageWidth);
			BurnDrvGetAspect(&GameAspectY, &GameAspectX);
			//BurnDrvGetAspect(&GameAspectX, &GameAspectY);
			printf("Vertical\n");
			nRotateGame = 1;
			display_w = nVidImageHeight * GameAspectX / GameAspectY;
			display_h = nVidImageHeight;
		}

		if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED)
		{
			printf("Flipped\n");
			bFlipped = 1;
		}
	}

	char title[512];

	sprintf(title, "FBNeo - %s - %s", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_FULLNAME));

	Uint32 screenFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

	if (bAppFullscreen)
	{
		screenFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	dstrect.y = 0;
	dstrect.x = 0;
	dstrect.h = display_h;
	dstrect.w = display_w;

	if (nRotateGame)
	{
		sdlWindow = SDL_CreateWindow(
			title,
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			display_h,
			display_w,
			screenFlags
		);
		dstrect.y = (display_w - display_h) / 2;
		dstrect.x = (display_h - display_w) / 2;
		dstrect.h = display_h;
		dstrect.w = display_w;

	}
	else
	{
		sdlWindow = SDL_CreateWindow(
			title,
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			display_w,
			display_h,
			screenFlags
		);
	}



	// Check that the window was successfully created
	if (sdlWindow == NULL)
	{
		// In the case that the window could not be made...
		printf("Could not create window: %s\n", SDL_GetError());
		return 1;
	}

	Uint32 renderflags = SDL_RENDERER_ACCELERATED;

	if (vsync)
	{
		renderflags = renderflags | SDL_RENDERER_PRESENTVSYNC;
	}

	sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, renderflags);
	if (sdlRenderer == NULL)
	{
		// In the case that the window could not be made...
		printf("Could not create renderer: %s\n", SDL_GetError());
		return 1;
	}

	overlayTexture = LoadOverlayImage(sdlRenderer, overlayTexture);
	if (bHasOverlay)
	{
		SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
	}
	else
	{
		SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_NONE);
	}

	nVidImageDepth = 32;

	if (BurnDrvGetFlags() & BDF_16BIT_ONLY)
	{
		nVidImageDepth = 16;
		printf("Forcing 16bit color\n");
	}
	printf("bbp: %d\n", nVidImageDepth);

	if (bIntegerScale)
	{
		SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE);
	}

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, videofiltering);

	if (!bHasOverlay)
	{
		printf("setting logical size w: %d h: %d", display_w, display_h);

		if (nRotateGame)
		{
			SDL_RenderSetLogicalSize(sdlRenderer, display_h, display_w);
		}
		else
		{
			SDL_RenderSetLogicalSize(sdlRenderer, display_w, display_h);
		}
	}

	inrenderer(sdlRenderer); //TODO: this is not supposed to be here
	prepare_inline_font();   // TODO: BAD
	incolor(0xFFF000, 0);

	if (nVidImageDepth == 32)
	{
		sdlTexture = SDL_CreateTexture(sdlRenderer,
			SDL_PIXELFORMAT_RGB888,
			SDL_TEXTUREACCESS_STREAMING,
			nVidImageWidth, nVidImageHeight);
	}
	else
	{
		sdlTexture = SDL_CreateTexture(sdlRenderer,
			SDL_PIXELFORMAT_RGB565,
			SDL_TEXTUREACCESS_STREAMING,
			nVidImageWidth, nVidImageHeight);
	}
	if (!sdlTexture)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create sdlTexture from surface: %s", SDL_GetError());
		return 3;
	}

	nVidImageBPP = (nVidImageDepth + 7) >> 3;
	nBurnBpp = nVidImageBPP;

	SetBurnHighCol(nVidImageDepth);

	nVidImagePitch = nVidImageWidth * nVidImageBPP;
	nBurnPitch = nVidImagePitch;

	nMemLen = nVidImageWidth * nVidImageHeight * nVidImageBPP;

	printf("nVidImageWidth=%d nVidImageHeight=%d nVidImagePitch=%d\n", nVidImageWidth, nVidImageHeight, nVidImagePitch);

	VidMem = (unsigned char*)malloc(nMemLen);
	if (VidMem)
	{
		memset(VidMem, 0, nMemLen);
		pVidImage = VidMem;
		printf("Malloc for video Ok %d\n", nMemLen);
		return 0;
	}
	else
	{
		pVidImage = NULL;
		return 1;
	}


	printf("done vid init");
	return 0;
}

static int vidScale(RECT*, int, int)
{
	return 0;
}

// Run one frame and render the screen
static int Frame(bool bRedraw)                                          // bRedraw = 0
{
	if (pVidImage == NULL)
	{
		return 1;
	}

	if (bDrvOkay)
	{
		if (bRedraw)
		{                   // Redraw current frame
			if (BurnDrvRedraw())
			{
				BurnDrvFrame();                                             // No redraw function provided, advance one frame
			}
		}
		else
		{
			BurnDrvFrame();                                // Run one frame and draw the screen
		}

		if ((BurnDrvGetFlags() & BDF_16BIT_ONLY) && pVidTransCallback)
		{
			pVidTransCallback();
		}
	}
	return 0;
}

// Paint the BlitFX surface onto the primary surface
static int Paint(int bValidate)
{
	void* pixels;
	int   pitch;

	SDL_RenderClear(sdlRenderer);

	if (nRotateGame)
	{SDL_Point window_size = {nVidImageHeight, nVidImageWidth};
		SDL_UpdateTexture(sdlTexture, NULL, pVidImage, nVidImagePitch);
		if (nRotateGame && bFlipped)
		{
			SDL_RenderCopyEx(sdlRenderer, sdlTexture, NULL, &dstrect, 90, NULL, SDL_FLIP_NONE);
		}
		if (nRotateGame && !bFlipped)
		{
			SDL_RenderCopyEx(sdlRenderer, sdlTexture, NULL, &dstrect, 270, NULL, SDL_FLIP_NONE);
		}
	}
	else
	{
		SDL_LockTexture(sdlTexture, NULL, &pixels, &pitch);
		memcpy(pixels, pVidImage, nVidImagePitch * nVidImageHeight);
		SDL_UnlockTexture(sdlTexture);
		SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &dstrect);
	}

	RenderMessage();


	if (bHasOverlay)
	{
		if (bIntegerScale)
		{
			SDL_RenderSetIntegerScale(sdlRenderer, SDL_FALSE);
		}

		SDL_RenderCopy(sdlRenderer, overlayTexture, &title_texture_rect, &dest_title_texture_rect);

		if (bIntegerScale)
		{
			SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE);
		}
		SDL_SetHint(SDL_HINT_RENDER_LOGICAL_SIZE_MODE, "0");
	}

	SDL_RenderPresent(sdlRenderer);

	return 0;
}

static int GetSettings(InterfaceInfo* pInfo)
{
	return 0;
}

// The Video Output plugin:
struct VidOut VidOutSDL2 = { Init, Exit, Frame, Paint, vidScale, GetSettings, _T("SDL2 video output") };
