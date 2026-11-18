// plutosvg + plutovg, built as one object for FBNeo (SVG images in MAME artwork layouts)
//   plutosvg: https://github.com/sammycage/plutosvg (fd8a080)
//   plutovg:  https://github.com/sammycage/plutovg (bbd91f0)
// both MIT licensed, see LICENSE-plutosvg / LICENSE-plutovg (FTL.TXT for the FreeType-derived rasterizer)

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"	// false positives from inlining
#endif

#define PLUTOVG_BUILD_STATIC
#define PLUTOSVG_BUILD_STATIC

#include "plutovg-blend.c"
#include "plutovg-canvas.c"
#include "plutovg-font.c"
#include "plutovg-ft-math.c"
#include "plutovg-ft-raster.c"
#include "plutovg-ft-stroker.c"
#include "plutovg-matrix.c"
#include "plutovg-paint.c"
#include "plutovg-path.c"
#include "plutovg-rasterize.c"
#include "plutovg-surface.c"
#undef MAX_NAME
#include "plutosvg.c"
