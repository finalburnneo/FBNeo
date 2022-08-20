#include "burner.h"
#include "sdl2_gui_common.h"

int color_result = 0;
double color_x = 0.01;
double color_y = 0.01;
double color_z = 0.01;

void calcSelectedItemColor()
{
	color_x += randomRange(0.01, 0.02);
	color_y += randomRange(0.01, 0.07);
	color_z += randomRange(0.01, 0.09);

	SDL_Color pal[1];
	pal[0].r = 190 + 64 * sin(color_x + (color_result * 0.004754));
	pal[0].g = 190 + 64 * sin(color_y + (color_result * 0.006754));
	pal[0].b = 190 + 64 * sin(color_z + (color_result * 0.005754));
	color_result+5;

	incolor1(pal);
}


float random_gen()
{
	return rand() / (float)RAND_MAX;
}

float randomRange(float low, float high)
{
	float range = high - low;

	return (float)(random_gen() * range) + low;
}


void renderPanel(SDL_Renderer* sdlRenderer, int x, int y, int w, int h, UINT8 r, UINT8 g, UINT8 b )
{
	SDL_Rect fillRect = { x, y, w, h };
	SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(sdlRenderer, r, g, b, 220);
	SDL_RenderFillRect(sdlRenderer, &fillRect);
	SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_NONE);
}

int GetFullscreen()
{
		return bAppFullscreen?1:0;
}

void SetFullscreen(int f)
{
	bAppFullscreen = f?1:0;

	if (bAppFullscreen)
	{
		SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}
	else
	{
		SDL_SetWindowFullscreen(sdlWindow, 0);
	}
}
