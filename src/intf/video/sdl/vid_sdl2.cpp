// blitter effects via SDL2
#include "burner.h"
#include "vid_support.h"
#include "vid_softfx.h"

#include <SDL.h>

static unsigned char* VidMem = NULL;
static SDL_Window *sdlWindow = NULL;
static SDL_Renderer *sdlRenderer = NULL;
static SDL_Texture *sdlTexture = NULL;

static int nGamesWidth = 0, nGamesHeight = 0; // screen size

static int nRotateGame = 0;
static bool bFlipped = false;


static int VidSScaleImage(RECT* pRect)
{
		int nScrnWidth, nScrnHeight;

		int nGameAspectX = 4, nGameAspectY = 3;
		int nWidth = pRect->right - pRect->left;
		int nHeight = pRect->bottom - pRect->top;

		if (bVidFullStretch)
		{ // Arbitrary stretch
				return 0;
		}

		if (bDrvOkay)
		{
				if ((BurnDrvGetFlags() & (BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED)))
				{
						BurnDrvGetAspect(&nGameAspectY, &nGameAspectX);
				}
				else
				{
						BurnDrvGetAspect(&nGameAspectX, &nGameAspectY);
				}
		}

		nScrnWidth = nGameAspectX;
		nScrnHeight = nGameAspectY;

		int nWidthScratch = nHeight * nVidScrnAspectY * nGameAspectX * nScrnWidth /
							(nScrnHeight * nVidScrnAspectX * nGameAspectY);

		if (nWidthScratch > nWidth)
		{ // The image is too wide
				if (nGamesWidth < nGamesHeight)
				{ // Vertical games
						nHeight = nWidth * nVidScrnAspectY * nGameAspectY * nScrnWidth /
								  (nScrnHeight * nVidScrnAspectX * nGameAspectX);
				}
				else
				{ // Horizontal games
						nHeight = nWidth * nVidScrnAspectX * nGameAspectY * nScrnHeight /
								  (nScrnWidth * nVidScrnAspectY * nGameAspectX);
				}
		}
		else
		{
				nWidth = nWidthScratch;
		}

		pRect->left = (pRect->right + pRect->left) / 2;
		pRect->left -= nWidth / 2;
		pRect->right = pRect->left + nWidth;

		pRect->top = (pRect->top + pRect->bottom) / 2;
		pRect->top -= nHeight / 2;
		pRect->bottom = pRect->top + nHeight;

		return 0;
}


static int Exit()
{
  SDL_DestroyTexture(sdlTexture);
  SDL_DestroyRenderer(sdlRenderer);
  SDL_DestroyWindow(sdlWindow);

	free(VidMem);
	return 0;
}

static int Init()
{

	int nMemLen = 0;

		  if(SDL_Init(SDL_INIT_VIDEO) < 0)
      {
          printf("vid init error\n");
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
					return 3;
			}

	nGamesWidth = nVidImageWidth;
	nGamesHeight = nVidImageHeight;

	nRotateGame = 0;

	if (bDrvOkay) {
			// Get the game screen size
			BurnDrvGetVisibleSize(&nGamesWidth, &nGamesHeight);

			if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)
			{
					printf("Vertical\n");
					nRotateGame = 1;
			}

			if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED)
			{
					printf("Flipped\n");
					bFlipped = true;
			}
	}

	if (!nRotateGame)
	{
		//	nTextureWidth = GetTextureSize(nGamesWidth);
	//		nTextureHeight = GetTextureSize(nGamesHeight);
	}
	else
	{
		//	nTextureWidth = GetTextureSize(nGamesHeight);
		//	nTextureHeight = GetTextureSize(nGamesWidth);
	}

  	sdlWindow = SDL_CreateWindow(
  			"An SDL2 window",                  // window title
  			SDL_WINDOWPOS_CENTERED,           // initial x position
  			SDL_WINDOWPOS_CENTERED,           // initial y position
  			nGamesWidth,                               // width, in pixels
  			nGamesHeight,                               // height, in pixels
  			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE                 // flags - see below
  	);

  	// Check that the window was successfully created
  	if (sdlWindow == NULL) {
  			// In the case that the window could not be made...
  			printf("Could not create window: %s\n", SDL_GetError());
  			return 1;
  	}

   	sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
  	if (sdlRenderer == NULL) {
  			// In the case that the window could not be made...
  			printf("Could not create renderer: %s\n", SDL_GetError());
  			return 1;
  	}

	   nVidImageDepth = bDrvOkay ? 16 : 32;

     RECT test_rect;
     test_rect.left = 0;
     test_rect.right = nGamesWidth;
     test_rect.top = 0;
     test_rect.bottom = nGamesHeight;

     printf("correctx before %d, %d\n", test_rect.right, test_rect.bottom);
     VidSScaleImage(&test_rect);
     printf("correctx after %d, %d\n", test_rect.right, test_rect.bottom);


  	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, 0);  
  	SDL_RenderSetLogicalSize(sdlRenderer, test_rect.right, test_rect.bottom);

    if(nVidImageDepth == 32)
    {
      sdlTexture = SDL_CreateTexture(sdlRenderer,
                                     SDL_PIXELFORMAT_RGB888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     nGamesWidth, nGamesHeight);

    }
    else
    {
      sdlTexture = SDL_CreateTexture(sdlRenderer,
    	                               SDL_PIXELFORMAT_RGB565,
    	                               SDL_TEXTUREACCESS_STREAMING,
    	                               nGamesWidth, nGamesHeight);

    }
  	 if (!sdlTexture) {
         SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create sdlTexture from surface: %s", SDL_GetError());
         return 3;
     }



	nVidImageBPP = (nVidImageDepth + 7) >> 3;
	nBurnBpp = nVidImageBPP;

	SetBurnHighCol(nVidImageDepth);

	if (!nRotateGame)
	{
			nVidImageWidth = nGamesWidth;
			nVidImageHeight = nGamesHeight;
	}
	else
	{
			nVidImageWidth = nGamesHeight;
			nVidImageHeight = nGamesWidth;
	}

	nVidImagePitch = nVidImageWidth * nVidImageBPP;
	nBurnPitch = nVidImagePitch;

	nMemLen = nVidImageWidth * nVidImageHeight * nVidImageBPP;

	printf("nVidImageWidth=%d nVidImageHeight=%d nVidImagePitch=%d\n", nVidImageWidth, nVidImageHeight, nVidImagePitch);

	VidMem = (unsigned char *)malloc(nMemLen);
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

static int vidScale(RECT* , int, int)
{
	return 0;
}

// Run one frame and render the screen
static int Frame(bool bRedraw)						// bRedraw = 0
{
	if (pVidImage == NULL)
	{
    printf("lost image\n");
	}

	if (bDrvOkay)
	{
			if (bRedraw)
			{ // Redraw current frame
					if (BurnDrvRedraw())
					{
							BurnDrvFrame(); // No redraw function provided, advance one frame
					}
			}
			else
			{
					BurnDrvFrame(); // Run one frame and draw the screen
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
	//SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255);
	SDL_UpdateTexture(sdlTexture, NULL, pVidImage, nVidImagePitch);

  SDL_RenderClear(sdlRenderer);
  SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
  SDL_RenderPresent(sdlRenderer);

	return 0;
}

static int GetSettings(InterfaceInfo* pInfo)
{
	return 0;
}

// The Video Output plugin:
struct VidOut VidOutSDL2 = { Init, Exit, Frame, Paint, vidScale, GetSettings, _T("SDL2 video output") };
