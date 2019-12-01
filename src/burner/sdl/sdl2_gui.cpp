#include "burner.h"

static SDL_Window *  sdlWindow   = NULL;
static SDL_Renderer *sdlRenderer = NULL;
static SDL_Texture * sdlTexture  = NULL;


static int nVidGuiWidth  = 800;
static int nVidGuiHeight = 600;

static unsigned int startGame      = 0; // game at top of list as it is displayed on the menu
static unsigned int gamesperscreen = 12;
static unsigned int gametoplay     = 0;


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

   gamesperscreen = (nVidGuiHeight - 100) / 10;
}

void gui_render()
{
   SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
   SDL_SetRenderDrawColor(sdlRenderer, 50, 50, 50, SDL_ALPHA_OPAQUE);
   SDL_RenderClear(sdlRenderer);
   incolor(0xffffff, /* unused */ 0);
   inprint(sdlRenderer, "FinalBurn Neo", 10, 10);
   inprint(sdlRenderer, "=============", 10, 20);
   incolor(0x000fff, /* unused */ 0);

   for (unsigned int i = startGame, game_counter = 0; game_counter < gamesperscreen; i++, game_counter++)
   {
      if (i > 0 && i < nBurnDrvCount)
      {
         nBurnDrvActive = i;
         if (game_counter == gamesperscreen / 2)
         {
            incolor(0xffffff, /* unused */ 0);
            inprint_shadowed(sdlRenderer, BurnDrvGetTextA(DRV_FULLNAME), 10, 30 + (game_counter * 10));
            gametoplay = i;
            incolor(0x000fff, /* unused */ 0);
            char infoLine[512];
            snprintf(infoLine, 512, "Year: %s - Manufacturer: %s - System: %s", BurnDrvGetTextA(DRV_DATE), BurnDrvGetTextA(DRV_MANUFACTURER), BurnDrvGetTextA(DRV_SYSTEM));
            char romLine[512];
            snprintf(romLine, 512, "Romset: %s - Parent: %s", BurnDrvGetTextA(DRV_NAME), BurnDrvGetTextA(DRV_PARENT));

            inprint_shadowed(sdlRenderer, BurnDrvGetTextA(DRV_FULLNAME), 10, nVidGuiHeight - 60);
            inprint_shadowed(sdlRenderer, infoLine, 10, nVidGuiHeight - 50);
            inprint_shadowed(sdlRenderer, romLine, 10, nVidGuiHeight - 40);
            inprint_shadowed(sdlRenderer, BurnDrvGetTextA(DRV_COMMENT), 10, nVidGuiHeight - 30);
         }
         else
         {
            incolor(0xfff000, /* unused */ 0);
            inprint_shadowed(sdlRenderer, BurnDrvGetTextA(DRV_FULLNAME), 10, 30 + (game_counter * 10));
         }
      }
   }

   SDL_RenderPresent(sdlRenderer);
}

int gui_process()
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
            switch (e.key.keysym.sym)
            {
            case SDLK_UP:
               if (startGame > 0)
               {
                  startGame--;
               }
               break;

            case SDLK_DOWN:
               if (startGame < nBurnDrvCount)
               {
                  startGame++;
               }
               break;

            case SDLK_LEFT:
               if (startGame > 10)
               {
                  startGame -= 10;
               }
               break;

            case SDLK_RIGHT:
               if (startGame < nBurnDrvCount - 10)
               {
                  startGame += 10;
               }
               break;

            case SDLK_RETURN:
               nBurnDrvActive = gametoplay;
               printf("selected %s\n", BurnDrvGetTextA(DRV_FULLNAME));
               return gametoplay;

            default:
               break;
            }
            break;
         }
         if (e.type == SDL_MOUSEBUTTONDOWN)
         {
            quit = true;
         }
      }
      gui_render();
   }
   return -1;
}
