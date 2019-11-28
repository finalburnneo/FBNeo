#include "burner.h"

static SDL_Window *  sdlWindow   = NULL;
static SDL_Renderer *sdlRenderer = NULL;
static SDL_Texture * sdlTexture  = NULL;


static int nVidGuiWidth  = 800;
static int nVidGuiHeight = 600;

void gui_exit()
{
   kill_inline_font();
   SDL_DestroyTexture(sdlTexture);
   SDL_DestroyRenderer(sdlRenderer);
   SDL_DestroyWindow(sdlWindow);
}

void gui_init()
{
   if (SDL_Init(SDL_INIT_VIDEO) < 0)
   {
      printf("vid init error\n");
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
      return;
   }


   sdlWindow = SDL_CreateWindow(
      "FBNeo - Choose your weapon...",        // window title
      SDL_WINDOWPOS_CENTERED,                 // initial x position
      SDL_WINDOWPOS_CENTERED,                 // initial y position
      nVidGuiWidth,                           // width, in pixels
      nVidGuiHeight,                          // height, in pixels
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE // flags - see below
      );

   // Check that the window was successfully created
   if (sdlWindow == NULL)
   {
      // In the case that the window could not be made...
      printf("Could not create window: %s\n", SDL_GetError());
      return;
   }

   sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_ACCELERATED);
   if (sdlRenderer == NULL)
   {
      // In the case that the window could not be made...
      printf("Could not create renderer: %s\n", SDL_GetError());
      return;
   }


   SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, 0);
   SDL_RenderSetLogicalSize(sdlRenderer, nVidGuiWidth, nVidGuiHeight);
   //SDL_RenderSetIntegerScale(sdlRenderer, SDL_TRUE); // Probably best not turn this on

   sdlTexture = SDL_CreateTexture(sdlRenderer,
                                  SDL_PIXELFORMAT_RGB888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  nVidGuiWidth, nVidGuiHeight);

   if (!sdlTexture)
   {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create sdlTexture from surface: %s", SDL_GetError());
      return;
   }
inrenderer(sdlRenderer);
   prepare_inline_font();
}

static unsigned int startGame=0; // game at top of list as it is displayed on the menu
static unsigned int gamesperscreen=12;
static unsigned int gametoplay=0;


void gui_render()
{
   SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
   SDL_SetRenderDrawColor(sdlRenderer, 200, 200, 200, SDL_ALPHA_OPAQUE);

   void *pixels;
   int   pitch;

   SDL_LockTexture(sdlTexture, NULL, &pixels, &pitch);
  // memcpy(pixels, pVidImage, nVidImagePitch * nGamesHeight);
   memset(pixels, 0,  sizeof(pixels));
   SDL_UnlockTexture(sdlTexture);

   SDL_RenderClear(sdlRenderer);
   SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);

   incolor(0xffffff, /* unused */ 0);
   inprint(sdlRenderer, "FinalBurn Neo", 10, 10);
   inprint(sdlRenderer, "=============", 10, 20);
   incolor(0x000fff, /* unused */ 0);
   for (unsigned int i=startGame,game_counter=0; game_counter<gamesperscreen; i++,game_counter++)
 	{
 		if (i>0 && i<nBurnDrvCount)
 		{
 			nBurnDrvActive=i;
 			if (game_counter==gamesperscreen/2)
 			{
        incolor(0x000fff, /* unused */ 0);
 				inprint(sdlRenderer,BurnDrvGetTextA(DRV_FULLNAME), 10,game_counter*16+40);
 				gametoplay=i;
 			}
 			else
 			{
        incolor(0xfff000, /* unused */ 0);
        inprint(sdlRenderer,BurnDrvGetTextA(DRV_FULLNAME), 10,game_counter*16+40);
 			}
 		}
 	}

   SDL_RenderPresent(sdlRenderer);
}

void gui_process()
{
   SDL_Event e;
   bool      quit = false;

   while (!quit)
   {
      while (SDL_PollEvent(&e))
      {
         if (e.type == SDL_QUIT)
         {
            quit = true;
         }
         if (e.type == SDL_KEYDOWN)
         {
            startGame++;
         }
         if (e.type == SDL_MOUSEBUTTONDOWN)
         {
            quit = true;
         }
      }
      gui_render();
   }
}
