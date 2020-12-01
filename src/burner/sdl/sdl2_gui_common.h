#pragma once

#define fbn_color           0xfe8a71
#define select_color        0xffffff
#define normal_color        0x1eaab7
#define normal_color_parent 0xaebac7
#define unavailable_color   0xFF0000
#define info_color          0xff9e3c

extern double color_x;
extern double color_y;
extern double color_z;
extern int color_result;

void calcSelectedItemColor();
float random_gen();
float randomRange(float low, float high);
void renderPanel(SDL_Renderer* sdlRenderer, int x, int y, int w, int h, UINT8 r, UINT8 g, UINT8 b );

int GetFullscreen();
void SetFullscreen(int f);
