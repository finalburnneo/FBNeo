#ifndef _LIBRETRO_OPTIMIZATIONS_H_
#define _LIBRETRO_OPTIMIZATIONS_H_

#define BurnHighCol LIBRETRO_COLOR_CONVERT
#define BurnHighColSwitch LIBRETRO_COLOR_CONVERT_SWITCH

#ifdef FRONTEND_SUPPORTS_RGB565
#define LIBRETRO_COLOR_15BPP_XBGR(color, unused) (((color & 0x001f) << 11) | ((color & 0x03e0) << 1) | ((color & 0x7c00) >> 10))
#define LIBRETRO_COLOR_CONVERT(r, g, b, a) (((r << 8) & 0xf800) | ((g << 3) & 0x07e0) | ((b >> 3) & 0x001f))
#define LIBRETRO_COLOR_CONVERT_SWITCH(r, g, b, a) (nBurnBpp == 2) ? (((r << 8) & 0xf800) | ((g << 3) & 0x07e0) | ((b >> 3) & 0x001f)) : (((r << 16) & 0xff0000) | ((g << 8) & 0x00ff00) | ((b) & 0x0000ff))
#else
#define LIBRETRO_COLOR_15BPP_XBGR(color, unused) ((((color & 0x1f) << 10) | (((color & 0x3e0) >> 5) << 5) | (((color & 0x7c00) >> 10))) & 0x7fff)
#define LIBRETRO_COLOR_CONVERT(r, g, b, a) (((r << 7) & 0x7c00) | ((g << 2) & 0x03e0) | ((b >> 3) & 0x001f))
#define LIBRETRO_COLOR_CONVERT_SWITCH(r, g, b, a) (nBurnBpp == 2) ? (((r << 7) & 0x7c00) | ((g << 2) & 0x03e0) | ((b >> 3) & 0x001f)) : (((r << 16) & 0xff0000) | ((g << 8) & 0x00ff00) | ((b) & 0x0000ff))
#endif

#endif
