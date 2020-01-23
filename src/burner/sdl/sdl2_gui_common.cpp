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
