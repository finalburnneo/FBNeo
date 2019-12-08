// blitter effects via SDL2
#include "burner.h"
#include "vid_support.h"
#include "vid_softfx.h"

#include <SDL.h>
extern int vsync;

static unsigned char *VidMem      = NULL;
static SDL_Window *   sdlWindow   = NULL;
static SDL_Renderer * sdlRenderer = NULL;
static SDL_Texture *  sdlTexture  = NULL;

static int  nRotateGame = 0;
static bool bFlipped    = false;


static int Exit()
{
   kill_inline_font(); //TODO: This is not supposed to be here

   SDL_DestroyTexture(sdlTexture);
   SDL_DestroyRenderer(sdlRenderer);
   SDL_DestroyWindow(sdlWindow);

   free(VidMem);
   return 0;
}

static int Init()
{
   int nMemLen = 0;
   int GameAspectX = 4, GameAspectY = 3;
   int display_w = 400, display_h = 300;

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
      display_h = nVidImageWidth * GameAspectY/GameAspectX;

      if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)
      {
         BurnDrvGetVisibleSize(&nVidImageHeight, &nVidImageWidth);
         BurnDrvGetAspect(&GameAspectX, &GameAspectY);
         printf("Vertical\n");
         nRotateGame = 1;
         display_w = nVidImageHeight* GameAspectX/GameAspectY;
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

   sdlWindow = SDL_CreateWindow(
      title,                  // window title
      SDL_WINDOWPOS_CENTERED, // initial x position
      SDL_WINDOWPOS_CENTERED, // initial y position
      display_w,              // width, in pixels
      display_h,              // height, in pixels
      screenFlags             // flags - see below
      );

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

   nVidImageDepth = bDrvOkay ? 16 : 32;
   SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, 0);
  // SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE);   // Probably best not turn this on

   if (nRotateGame)
   {
      SDL_RenderSetLogicalSize(sdlRenderer, display_w, display_w);
   }
   else
   {
      SDL_RenderSetLogicalSize(sdlRenderer, display_w, display_h);
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
   nBurnBpp     = nVidImageBPP;

   SetBurnHighCol(nVidImageDepth);



   nVidImagePitch = nVidImageWidth * nVidImageBPP;
   nBurnPitch     = nVidImagePitch;

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

static int vidScale(RECT *, int, int)
{
   return 0;
}

// Run one frame and render the screen
static int Frame(bool bRedraw)                                          // bRedraw = 0
{
   if (pVidImage == NULL)
   {
      printf("lost image\n");
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
   void *pixels;
   int   pitch;

   SDL_RenderClear(sdlRenderer);
   if (nRotateGame)
   {
      SDL_UpdateTexture(sdlTexture, NULL, pVidImage, nVidImagePitch);
      if (nRotateGame && bFlipped)
      {
         SDL_RenderCopyEx(sdlRenderer, sdlTexture, NULL, NULL, 90, NULL, SDL_FLIP_NONE);
      }
      if (nRotateGame && !bFlipped)
      {
         SDL_RenderCopyEx(sdlRenderer, sdlTexture, NULL, NULL, 270, NULL, SDL_FLIP_NONE);
      }
   }
   else
   {
      SDL_LockTexture(sdlTexture, NULL, &pixels, &pitch);
      memcpy(pixels, pVidImage, nVidImagePitch * nVidImageHeight);
      SDL_UnlockTexture(sdlTexture);
      SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
   }
   if (bAppDoFast)
   {
      inprint_shadowed(sdlRenderer, "FFWD", 10, 10);
   }
   if (bAppShowFPS)
   {
      inprint_shadowed(sdlRenderer, fpsstring, 10, 50);
   }
   SDL_RenderPresent(sdlRenderer);

   return 0;
}

static int GetSettings(InterfaceInfo *pInfo)
{
   return 0;
}

// The Video Output plugin:
struct VidOut VidOutSDL2 = { Init, Exit, Frame, Paint, vidScale, GetSettings, _T("SDL2 video output") };
