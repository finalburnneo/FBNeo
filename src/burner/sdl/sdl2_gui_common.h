#pragma once

#define fbn_color           0xfe8a71
#define select_color        0xffffff
#define normal_color        0x1eaab7
#define normal_color_parent 0xaebac7
#define unavailable_color   0xFF0000
#define info_color          0x3a3e3d

extern double color_x;
extern double color_y;
extern double color_z;
extern int color_result;

void calcSelectedItemColor();
float random_gen();
float randomRange(float low, float high);
