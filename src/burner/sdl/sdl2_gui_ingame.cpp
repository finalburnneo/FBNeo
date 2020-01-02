#include "burner.h"

static SDL_Renderer* sdlRenderer = NULL;
static SDL_Surface* screenshot = NULL;
static SDL_Texture* screenshotTexture = NULL;


static SDL_Rect title_texture_rect;
static SDL_Rect dest_title_texture_rect;

static int screenW = 0;
static int screenH = 0;

#if SDL_BYTEORDER != SDL_BIG_ENDIAN
const UINT32 amask = 0xff000000;
const UINT32 rmask = 0x00ff0000;
const UINT32 gmask = 0x0000ff00;
const UINT32 bmask = 0x000000ff;
#else
const UINT32 amask = 0x000000ff;
const UINT32 rmask = 0x0000ff00;
const UINT32 gmask = 0x00ff0000;
const UINT32 bmask = 0xff000000;
#endif



void ingame_gui_init()
{
  AudSoundStop();
  inrenderer(sdlRenderer);
	prepare_inline_font();
}

void ingame_gui_exit()
{
  kill_inline_font();
  AudSoundPlay();
  SDL_FreeSurface(screenshot);
  SDL_DestroyTexture(screenshotTexture);
}

void ingame_gui_render()
{
  SDL_SetRenderDrawColor(sdlRenderer, 0x1a, 0x1e, 0x1d, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(sdlRenderer);
  SDL_RenderCopy(sdlRenderer, screenshotTexture, &title_texture_rect, &dest_title_texture_rect);
  incolor(0xfe8a71, /* unused */ 0);
  inprint(sdlRenderer, "FinalBurn Neo", 55, 10);
  inprint(sdlRenderer, "=============", 55, 20);
  SDL_RenderPresent(sdlRenderer);
}

int ingame_gui_process()
{
	SDL_Event e;

	while (SDL_PollEvent(&e))
	{
		if (e.type == SDL_QUIT)
		{
			return 1;
		}
    if (e.type == SDL_KEYDOWN)
    {
      switch (e.key.keysym.sym)
      {
      case SDLK_TAB:
        return 1;
        break;
      }
    }
  }
  return 0;
}

void ingame_gui_start(SDL_Renderer* renderer)
{
  int finished = 0;

  sdlRenderer = renderer;
  SDL_GetRendererOutputSize(sdlRenderer, &screenW, &screenH);

  screenshot =  SDL_CreateRGBSurface(0, screenW, screenH, 32, rmask, gmask, bmask, amask);
  SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screenshot->pixels, screenshot->pitch);
  screenshotTexture = SDL_CreateTextureFromSurface(renderer, screenshot);
  SDL_FreeSurface(screenshot);
  screenshot = NULL;

	title_texture_rect.x = 0; //the x coordinate
	title_texture_rect.y = 0; // the y coordinate
	title_texture_rect.w = screenW; //the width of the texture
	title_texture_rect.h = screenH; //the height of the texture

	dest_title_texture_rect.x = 0; //the x coordinate
	dest_title_texture_rect.y = 0; // the y coordinate
	dest_title_texture_rect.w = 50; //the width of the texture
	dest_title_texture_rect.h = 50; //the height of the texture

  ingame_gui_init();

  while (!finished)
  {
    finished = ingame_gui_process();
    ingame_gui_render();
  }

  ingame_gui_exit();

}
