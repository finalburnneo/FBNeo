
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ppu.h"
#include "snes.h"
#include "statehandler.h"

// array for layer definitions per mode:
//   0-7: mode 0-7; 8: mode 1 + l3prio; 9: mode 7 + extbg

//   0-3; layers 1-4; 4: sprites; 5: nonexistent
static const int layersPerMode[10][12] = {
  {4, 0, 1, 4, 0, 1, 4, 2, 3, 4, 2, 3},
  {4, 0, 1, 4, 0, 1, 4, 2, 4, 2, 5, 5},
  {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
  {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
  {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
  {4, 0, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5},
  {4, 0, 4, 4, 0, 4, 5, 5, 5, 5, 5, 5},
  {4, 4, 4, 0, 4, 5, 5, 5, 5, 5, 5, 5},
  {2, 4, 0, 1, 4, 0, 1, 4, 4, 2, 5, 5},
  {4, 4, 1, 4, 0, 4, 1, 5, 5, 5, 5, 5}
};

static const int prioritysPerMode[10][12] = {
  {3, 1, 1, 2, 0, 0, 1, 1, 1, 0, 0, 0},
  {3, 1, 1, 2, 0, 0, 1, 1, 0, 0, 5, 5},
  {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
  {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
  {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
  {3, 1, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5},
  {3, 1, 2, 1, 0, 0, 5, 5, 5, 5, 5, 5},
  {3, 2, 1, 0, 0, 5, 5, 5, 5, 5, 5, 5},
  {1, 3, 1, 1, 2, 0, 0, 1, 0, 0, 5, 5},
  {3, 2, 1, 1, 0, 0, 0, 5, 5, 5, 5, 5}
};

static const int layerCountPerMode[10] = {
  12, 10, 8, 8, 8, 8, 6, 5, 10, 7
};

static const int bitDepthsPerMode[10][4] = {
  {2, 2, 2, 2},
  {4, 4, 2, 5},
  {4, 4, 5, 5},
  {8, 4, 5, 5},
  {8, 2, 5, 5},
  {4, 2, 5, 5},
  {4, 5, 5, 5},
  {8, 5, 5, 5},
  {4, 4, 2, 5},
  {8, 7, 5, 5}
};

static const int spriteSizes[8][2] = {
  {8, 16}, {8, 32}, {8, 64}, {16, 32},
  {16, 64}, {32, 64}, {16, 32}, {16, 32}
};

static uint32_t bright_lut[0x10];
static uint8_t color_clamp_lut[0x20 * 3];
static uint8_t *color_clamp_lut_i20 = &color_clamp_lut[0x20];

static uint16_t bg_line_buf[4];
static uint8_t bg_prio_buf[4];
static bool bg_window_state[6]; // 0-3 (bg) 4 (spr) 5 (colorwind)

static Snes* snes;
// vram access
static uint16_t vram[0x8000];
static uint16_t vramPointer;
static bool vramIncrementOnHigh;
static uint16_t vramIncrement;
static uint8_t vramRemapMode;
static uint16_t vramReadBuffer;
// cgram access
static uint16_t cgram[0x100];
static uint8_t cgramPointer;
static bool cgramSecondWrite;
static uint8_t cgramBuffer;
// oam access
static uint16_t oam[0x100];
static uint8_t highOam[0x20];
static uint8_t oamAdr;
static uint8_t oamAdrWritten;
static bool oamInHigh;
static bool oamInHighWritten;
static bool oamSecondWrite;
static uint8_t oamBuffer;
// object/sprites
static bool objPriority;
static uint16_t objTileAdr1;
static uint16_t objTileAdr2;
static uint8_t objSize;
static uint8_t objPixelBuffer[256]; // line buffers
static uint8_t objPriorityBuffer[256];
static bool timeOver;
static bool rangeOver;
static bool objInterlace;
// background layers
static BgLayer bgLayer[4];
static uint8_t scrollPrev;
static uint8_t scrollPrev2;
static uint8_t mosaicSize;
static uint8_t mosaicStartLine;
// layers
static Layer layer[5];
// mode 7
static int16_t m7matrix[8]; // a, b, c, d, x, y, h, v
static uint8_t m7prev;
static bool m7largeField;
static bool m7charFill;
static bool m7xFlip;
static bool m7yFlip;
static bool m7extBg;
// mode 7 internal
static int32_t m7startX;
static int32_t m7startY;
// windows
static WindowLayer windowLayer[6];
static uint8_t window1left;
static uint8_t window1right;
static uint8_t window2left;
static uint8_t window2right;
// color math
static uint8_t clipMode;
static uint8_t preventMathMode;
static bool addSubscreen;
static bool subtractColor;
static bool halfColor;
static bool mathEnabled[6];
static uint8_t fixedColorR;
static uint8_t fixedColorG;
static uint8_t fixedColorB;
// settings
static bool forcedBlank;
static uint8_t brightness;
static uint8_t mode;
static bool bg3priority;
static bool evenFrame;
static bool pseudoHires;
static bool overscan;
static bool frameOverscan; // if we are overscanning this frame (determined at 0,225)
static bool interlace;
static bool frameInterlace; // if we are interlacing this frame (determined at start vblank)
static bool directColor;
// latching
static uint16_t hCount;
static uint16_t vCount;
static bool hCountSecond;
static bool vCountSecond;
static bool countersLatched;
static uint8_t ppu1openBus;
static uint8_t ppu2openBus;
// super scope (zapper)
static uint16_t hScope;
static uint16_t vScope;
static bool bScopeLatch;

// pixel buffer (char)(xbgr) == (int)(xrgb) [int endianness]
// times 2 for even and odd frame
static uint8_t pixelBuffer[512 * 4 * 239 * 2];

bool ppu_frameInterlace() {
	return frameInterlace;
}

bool ppu_evenFrame() {
	return evenFrame;
}

static inline void ppu_handlePixel(int x, int y);
static inline int ppu_getPixel(int x, int y, bool sub, int* r, int* g, int* b);
static uint16_t ppu_getOffsetValue(int col, int row);
static inline void ppu_getPixelForBgLayer(int x, int y, int nlayer); //, bool priority, uint16_t *pixelCache, uint8_t *pixelCachePriority);
static void ppu_handleOPT(int nlayer, int* lx, int* ly);
static void ppu_calculateMode7Starts(int y);
static int ppu_getPixelForMode7(int x, int nlayer, bool priority);
static inline bool ppu_getWindowState(int nlayer, int x);
static void ppu_evaluateSprites(int line);
static uint16_t ppu_getVramRemap();

void ppu_init(Snes* ssnes) {
  snes = ssnes;
}

void ppu_free() {
}

void ppu_reset() {
  for (int i = 0; i < 0x10; i++) {
	bright_lut[i] = (i * 0x10000) / 15;
  }
  for (int i = 0; i < 0x20*3; i++) {
	  if (i < 0x20) {
		  color_clamp_lut[i] = 0;
	  }
	  if (i >= 0x20 && i <= 0x3f) {
		  color_clamp_lut[i] = i - 0x20; // 0 - 1f
	  }
	  if (i >= 0x40) {
		  color_clamp_lut[i] = 0x1f;
	  }
  }
  memset(vram, 0, sizeof(vram));
  vramPointer = 0;
  vramIncrementOnHigh = false;
  vramIncrement = 1;
  vramRemapMode = 0;
  vramReadBuffer = 0;
  memset(cgram, 0, sizeof(cgram));
  cgramPointer = 0;
  cgramSecondWrite = false;
  cgramBuffer = 0;
  memset(oam, 0, sizeof(oam));
  memset(highOam, 0, sizeof(highOam));
  oamAdr = 0;
  oamAdrWritten = 0;
  oamInHigh = false;
  oamInHighWritten = false;
  oamSecondWrite = false;
  oamBuffer = 0;
  objPriority = false;
  objTileAdr1 = 0;
  objTileAdr2 = 0;
  objSize = 0;
  memset(objPixelBuffer, 0, sizeof(objPixelBuffer));
  memset(objPriorityBuffer, 0, sizeof(objPriorityBuffer));
  timeOver = false;
  rangeOver = false;
  objInterlace = false;
  for(int i = 0; i < 4; i++) {
    bgLayer[i].hScroll = 0;
    bgLayer[i].vScroll = 0;
    bgLayer[i].tilemapWider = false;
    bgLayer[i].tilemapHigher = false;
    bgLayer[i].tilemapAdr = 0;
    bgLayer[i].tileAdr = 0;
    bgLayer[i].bigTiles = false;
    bgLayer[i].mosaicEnabled = false;
  }
  scrollPrev = 0;
  scrollPrev2 = 0;
  mosaicSize = 1;
  mosaicStartLine = 1;
  for(int i = 0; i < 5; i++) {
    layer[i].mainScreenEnabled = false;
    layer[i].subScreenEnabled = false;
    layer[i].mainScreenWindowed = false;
    layer[i].subScreenWindowed = false;
  }
  memset(m7matrix, 0, sizeof(m7matrix));
  m7prev = 0;
  m7largeField = false;
  m7charFill = false;
  m7xFlip = false;
  m7yFlip = false;
  m7extBg = false;
  m7startX = 0;
  m7startY = 0;
  for(int i = 0; i < 6; i++) {
    windowLayer[i].window1enabled = false;
    windowLayer[i].window2enabled = false;
    windowLayer[i].window1inversed = false;
    windowLayer[i].window2inversed = false;
    windowLayer[i].maskLogic = 0;
  }
  window1left = 0;
  window1right = 0;
  window2left = 0;
  window2right = 0;
  clipMode = 0;
  preventMathMode = 0;
  addSubscreen = false;
  subtractColor = false;
  halfColor = false;
  memset(mathEnabled, 0, sizeof(mathEnabled));
  fixedColorR = 0;
  fixedColorG = 0;
  fixedColorB = 0;
  forcedBlank = true;
  brightness = 0;
  mode = 0;
  bg3priority = false;
  evenFrame = false;
  pseudoHires = false;
  overscan = false;
  frameOverscan = false;
  interlace = false;
  frameInterlace = false;
  directColor = false;
  hCount = 0;
  vCount = 0;
  hCountSecond = false;
  vCountSecond = false;
  countersLatched = false;
  ppu1openBus = 0;
  ppu2openBus = 0;
  memset(pixelBuffer, 0, sizeof(pixelBuffer));
}

void ppu_handleState(StateHandler* sh) {
  sh_handleBools(sh,
    &vramIncrementOnHigh, &cgramSecondWrite, &oamInHigh, &oamInHighWritten, &oamSecondWrite,
    &objPriority, &timeOver, &rangeOver, &objInterlace, &m7largeField, &m7charFill,
    &m7xFlip, &m7yFlip, &m7extBg, &addSubscreen, &subtractColor, &halfColor,
    &mathEnabled[0], &mathEnabled[1], &mathEnabled[2], &mathEnabled[3], &mathEnabled[4],
    &mathEnabled[5], &forcedBlank, &bg3priority, &evenFrame, &pseudoHires, &overscan,
    &frameOverscan, &interlace, &frameInterlace, &directColor, &hCountSecond, &vCountSecond,
    &countersLatched, NULL
  );
  sh_handleBytes(sh,
    &vramRemapMode, &cgramPointer, &cgramBuffer, &oamAdr, &oamAdrWritten, &oamBuffer,
    &objSize, &scrollPrev, &scrollPrev2, &mosaicSize, &mosaicStartLine, &m7prev,
    &window1left, &window1right, &window2left, &window2right, &clipMode, &preventMathMode,
    &fixedColorR, &fixedColorG, &fixedColorB, &brightness, &mode,
    &ppu1openBus, &ppu2openBus, NULL
  );
  sh_handleWords(sh,
    &vramPointer, &vramIncrement, &vramReadBuffer, &objTileAdr1, &objTileAdr2,
    &hCount, &vCount, NULL
  );
  sh_handleWordsS(sh,
    &m7matrix[0], &m7matrix[1], &m7matrix[2], &m7matrix[3], &m7matrix[4], &m7matrix[5],
    &m7matrix[6], &m7matrix[7], NULL
  );
  sh_handleIntsS(sh, &m7startX, &m7startY, NULL);
  for(int i = 0; i < 4; i++) {
    sh_handleBools(sh,
      &bgLayer[i].tilemapWider, &bgLayer[i].tilemapHigher, &bgLayer[i].bigTiles,
      &bgLayer[i].mosaicEnabled, NULL
    );
    sh_handleWords(sh,
      &bgLayer[i].hScroll, &bgLayer[i].vScroll, &bgLayer[i].tilemapAdr, &bgLayer[i].tileAdr, NULL
    );
  }
  for(int i = 0; i < 5; i++) {
    sh_handleBools(sh,
      &layer[i].mainScreenEnabled, &layer[i].subScreenEnabled, &layer[i].mainScreenWindowed,
      &layer[i].subScreenWindowed, NULL
    );
  }
  for(int i = 0; i < 6; i++) {
    sh_handleBools(sh,
      &windowLayer[i].window1enabled, &windowLayer[i].window1inversed, &windowLayer[i].window2enabled,
      &windowLayer[i].window2inversed, NULL
    );
    sh_handleBytes(sh, &windowLayer[i].maskLogic, NULL);
  }
  sh_handleWordArray(sh, vram, 0x8000);
  sh_handleWordArray(sh, cgram, 0x100);
  sh_handleWordArray(sh, oam, 0x100);
  sh_handleByteArray(sh, highOam, 0x20);
  sh_handleByteArray(sh, objPixelBuffer, 256);
  sh_handleByteArray(sh, objPriorityBuffer, 256);
}

bool ppu_checkOverscan() {
  // called at (0,225)
  frameOverscan = overscan; // set if we have a overscan-frame
  return frameOverscan;
}

void ppu_handleVblank() {
  // called either right after ppu_checkOverscan at (0,225), or at (0,240)
  if(!forcedBlank) {
    oamAdr = oamAdrWritten;
    oamInHigh = oamInHighWritten;
    oamSecondWrite = false;
  }
  frameInterlace = interlace; // set if we have a interlaced frame
  ppu_latchScopeCheck(true); // reset scope latch if it was not hit this frame
}

void ppu_handleFrameStart() {
  // called at (0, 0)
  mosaicStartLine = 1;
  rangeOver = false;
  timeOver = false;
  evenFrame = !evenFrame;
}

static int layerCache[4] = { -1, -1, -1, -1 };

void ppu_latchMode7(int line) {
  if(mode == 7) ppu_calculateMode7Starts(line);
}

void ppu_runLine(int line) {
  // called for lines 1-224/239
  // evaluate sprites
  memset(objPixelBuffer, 0, sizeof(objPixelBuffer));
  if(!forcedBlank) ppu_evaluateSprites(line - 1);
  if (!pBurnDraw) { return;} /// super speeeeeeeeeeeeeeeeeeeeeeeeeeeeed!!!!
  // actual line
  //  if(mode == 7) ppu_calculateMode7Starts(line); // note: latched at hPos == 22!
  layerCache[0] = layerCache[1] = layerCache[2] = layerCache[3] = -1;
#if 0
  for(int x = 0; x < 256; x++) {
    ppu_handlePixel(x, line);
  }
#endif
#if 1
  for(int x = 0; x < 256; x+=8) {
    ppu_handlePixel(x + 0, line);
    ppu_handlePixel(x + 1, line);
    ppu_handlePixel(x + 2, line);
    ppu_handlePixel(x + 3, line);
    ppu_handlePixel(x + 4, line);
    ppu_handlePixel(x + 5, line);
    ppu_handlePixel(x + 6, line);
    ppu_handlePixel(x + 7, line);
  }
#endif
}

static inline void ppu_handlePixel(int x, int y) {
  int r = 0, r2 = 0;
  int g = 0, g2 = 0;
  int b = 0, b2 = 0;
  bool bhalfColor = halfColor;

  bg_window_state[0] = ppu_getWindowState(0, x);
  bg_window_state[1] = ppu_getWindowState(1, x);
  bg_window_state[2] = ppu_getWindowState(2, x);
  bg_window_state[3] = ppu_getWindowState(3, x);
  bg_window_state[4] = ppu_getWindowState(4, x);
  bg_window_state[5] = ppu_getWindowState(5, x);
 
  if(!forcedBlank) {
    int mainLayer = ppu_getPixel(x, y, false, &r, &g, &b);
	//    bool colorWindowState = ppu_getWindowState(5, x);
	bool colorWindowState = bg_window_state[5];
	bool bClipIfHires = false;
    if(
      clipMode == 3 ||
      (clipMode == 2 && colorWindowState) ||
      (clipMode == 1 && !colorWindowState)
    ) {
      if (clipMode < 3) bhalfColor = false;
      r = 0;
      g = 0;
	  b = 0;
	  bClipIfHires = true;
	}
    int secondLayer = 5; // backdrop
    bool bmathEnabled = mainLayer < 6 && mathEnabled[mainLayer] && !(
      preventMathMode == 3 ||
      (preventMathMode == 2 && colorWindowState) ||
      (preventMathMode == 1 && !colorWindowState)
    );
	bool bHighRes = pseudoHires || mode == 5 || mode == 6;
	if((bmathEnabled && addSubscreen) || bHighRes) {
      secondLayer = ppu_getPixel(x, y, true, &r2, &g2, &b2);
	  if (bHighRes && bClipIfHires) { r2 = g2 = b2 = 0; } // jpark hires odd pixels border clipping
	}
    // TODO: subscreen pixels can be clipped to black as well (done, line above -dink)
	// TODO: math for subscreen pixels (add/sub sub to main, in hires mode) (done, partially: only add/sub subscreen with fixedcolor for now -dink)
    if(bmathEnabled) {
      if(subtractColor) {
		  if (addSubscreen && secondLayer != 5) {
			  r -= r2;
			  g -= g2;
			  b -= b2;
		  } else {
			  r -= fixedColorR;
			  g -= fixedColorG;
			  b -= fixedColorB;
			  if (bHighRes) {
				r2 = color_clamp_lut_i20[r2 - fixedColorR];
				g2 = color_clamp_lut_i20[g2 - fixedColorG];
				b2 = color_clamp_lut_i20[b2 - fixedColorB];
			  }
		  }
      } else {
		  if (addSubscreen && secondLayer != 5) {
			  r += r2;
			  g += g2;
			  b += b2;
		  } else {
			  r += fixedColorR;
			  g += fixedColorG;
			  b += fixedColorB;
			  if (bHighRes) {
				r2 = color_clamp_lut_i20[r2 + fixedColorR];
				g2 = color_clamp_lut_i20[g2 + fixedColorG];
				b2 = color_clamp_lut_i20[b2 + fixedColorB];
			  }
		  }
      }
      if(bhalfColor && (secondLayer != 5 || !addSubscreen)) {
        r >>= 1;
        g >>= 1;
        b >>= 1;
	  }
	  r = color_clamp_lut_i20[r];
	  g = color_clamp_lut_i20[g];
	  b = color_clamp_lut_i20[b];
	}
	if(pseudoHires && mode < 5) {
		r = r2 = (r + r2) >> 1;
		b = b2 = (b + b2) >> 1;
		g = g2 = (g + g2) >> 1;
	}
    if(bHighRes == false) {
      r2 = r; g2 = g; b2 = b;
    }
  }
  UINT32 *dest = (UINT32*)&pixelBuffer[((y - 1) + (evenFrame ? 0 : 239)) * 2048 + x * 8];

  dest[0] = (UINT8)((((b2 << 3) | (b2 >> 2)) * bright_lut[brightness]) >> 16) << 0 |
            (UINT8)((((g2 << 3) | (g2 >> 2)) * bright_lut[brightness]) >> 16) << 8 |
			(UINT8)((((r2 << 3) | (r2 >> 2)) * bright_lut[brightness]) >> 16) << 16;

  dest[1] = (UINT8)((((b << 3) | (b >> 2)) * bright_lut[brightness]) >> 16) << 0 |
            (UINT8)((((g << 3) | (g >> 2)) * bright_lut[brightness]) >> 16) << 8 |
			(UINT8)((((r << 3) | (r >> 2)) * bright_lut[brightness]) >> 16) << 16;
}

static inline int ppu_getPixel(int x, int y, bool sub, int* r, int* g, int* b) {
  // figure out which color is on this location on main- or subscreen, sets it in r, g, b
  // returns which layer it is: 0-3 for bg layer, 4 or 6 for sprites (depending on palette), 5 for backdrop
  uint32_t actMode = mode == 1 && bg3priority ? 8 : mode;
  actMode = mode == 7 && m7extBg ? 9 : actMode;
  uint32_t nlayer = 5;
  uint32_t pixel = 0;

  for(int i = 0; i < layerCountPerMode[actMode]; i++) {
    uint32_t curLayer = layersPerMode[actMode][i];
    uint32_t curPriority = prioritysPerMode[actMode][i];
    bool layerActive = false;
    if(!sub) {
      layerActive = layer[curLayer].mainScreenEnabled && (
        !layer[curLayer].mainScreenWindowed || !bg_window_state[curLayer] //!ppu_getWindowState(curLayer, x)
      );
    } else {
      layerActive = layer[curLayer].subScreenEnabled && (
        !layer[curLayer].subScreenWindowed || !bg_window_state[curLayer] //!ppu_getWindowState(curLayer, x)
      );
    }
	if(layerActive) {
#if 0
		if (y==1 && x==0) {
			bprintf(0, _T("Layer  %x    Prio  %x    i  %x\n"), curLayer, curPriority, i);
		}
#endif
      if(curLayer < 4) {
        // bg layer
        int lx = x;
        int ly = y;
        if(bgLayer[curLayer].mosaicEnabled && mosaicSize > 1) {
          lx -= lx % mosaicSize;
          ly -= (ly - mosaicStartLine) % mosaicSize;
        }
        if(mode == 7) {
          pixel = ppu_getPixelForMode7(lx, curLayer, curPriority);
        } else {
          lx += bgLayer[curLayer].hScroll;
		  if(mode == 5 || mode == 6) {
            lx *= 2;
            lx += (sub || bgLayer[curLayer].mosaicEnabled) ? 0 : 1;
            if(interlace) {
              ly *= 2;
              ly += (evenFrame || bgLayer[curLayer].mosaicEnabled) ? 0 : 1;
            }
          }
          ly += bgLayer[curLayer].vScroll;
		  if(mode == 2 || mode == 4 || mode == 6) {
            ppu_handleOPT(curLayer, &lx, &ly);
		  }
		  if (lx != layerCache[curLayer]) {
			  ppu_getPixelForBgLayer(lx & 0x3ff, ly & 0x3ff, curLayer);
			  layerCache[curLayer] = lx;
		  }
		  pixel = (bg_prio_buf[curLayer] == curPriority) ? bg_line_buf[curLayer] : 0;
        }
      } else {
        // get a pixel from the sprite buffer
        pixel = 0;
		if(objPriorityBuffer[x] == curPriority) pixel = objPixelBuffer[x];
      }
    }
    if(pixel) {
      nlayer = curLayer;
      break;
    }
  }
  if(directColor && nlayer < 4 && bitDepthsPerMode[actMode][nlayer] == 8) {
    *r = ((pixel & 0x7) << 2) | ((pixel & 0x100) >> 7);
    *g = ((pixel & 0x38) >> 1) | ((pixel & 0x200) >> 8);
    *b = ((pixel & 0xc0) >> 3) | ((pixel & 0x400) >> 8);
  } else {
    uint16_t color = cgram[pixel & 0xff];
    *r = color & 0x1f;
    *g = (color >> 5) & 0x1f;
    *b = (color >> 10) & 0x1f;
  }
  if(nlayer == 4 && pixel < 0xc0) nlayer = 6; // sprites with palette color < 0xc0
  return nlayer;
}

static void ppu_handleOPT(int nlayer, int* lx, int* ly) {
  int x = *lx;
  int y = *ly;
  int column = 0;
  if(mode == 6) {
    column = ((x - (x & 0xf)) - ((bgLayer[nlayer].hScroll * 2) & 0xfff0)) >> 4;
  } else {
    column = ((x - (x & 0x7)) - (bgLayer[nlayer].hScroll & 0xfff8)) >> 3;
  }
  if(column > 0) {
    // fetch offset values from nlayer 3 tilemap
    int valid = nlayer == 0 ? 0x2000 : 0x4000;
    uint16_t hOffset = ppu_getOffsetValue(column - 1, 0);
    uint16_t vOffset = 0;
    if(mode == 4) {
      if(hOffset & 0x8000) {
        vOffset = hOffset;
        hOffset = 0;
      }
    } else {
      vOffset = ppu_getOffsetValue(column - 1, 1);
    }
    if(mode == 6) {
      // TODO: not sure if correct
      if(hOffset & valid) *lx = (((hOffset & 0x3f8) + (column * 8)) * 2) | (x & 0xf);
    } else {
      if(hOffset & valid) *lx = ((hOffset & 0x3f8) + (column * 8)) | (x & 0x7);
    }
    // TODO: not sure if correct for interlace
    if(vOffset & valid) *ly = (vOffset & 0x3ff) + (y - bgLayer[nlayer].vScroll);
  }
}

static uint16_t ppu_getOffsetValue(int col, int row) {
  int x = col * 8 + bgLayer[2].hScroll;
  int y = row * 8 + bgLayer[2].vScroll;
  int tileBits = bgLayer[2].bigTiles ? 4 : 3;
  int tileHighBit = bgLayer[2].bigTiles ? 0x200 : 0x100;
  uint16_t tilemapAdr = bgLayer[2].tilemapAdr + (((y >> tileBits) & 0x1f) << 5 | ((x >> tileBits) & 0x1f));
  if((x & tileHighBit) && bgLayer[2].tilemapWider) tilemapAdr += 0x400;
  if((y & tileHighBit) && bgLayer[2].tilemapHigher) tilemapAdr += bgLayer[2].tilemapWider ? 0x800 : 0x400;
  return vram[tilemapAdr & 0x7fff];
}

static inline void ppu_getPixelForBgLayer(int x, int y, int nlayer) { //, bool priority, uint16_t *pixelCache, uint8_t *pixelCachePriority) {
  // figure out address of tilemap word and read it
  bool wideTiles = bgLayer[nlayer].bigTiles || mode == 5 || mode == 6;
  int tileBitsX = wideTiles ? 4 : 3;
  //const int tileBitsX = 3 + wideTiles;
  int tileHighBitX = wideTiles ? 0x200 : 0x100;
  //const int tileHighBitX = 1 << (wideTiles + 8);
  int tileBitsY = bgLayer[nlayer].bigTiles ? 4 : 3;
  //const int tileBitsY = 3 + bgLayer[nlayer].bigTiles;
  int tileHighBitY = bgLayer[nlayer].bigTiles ? 0x200 : 0x100;
  //const int tileHighBitY = 1 << (bgLayer[nlayer].bigTiles + 8);
  uint16_t tilemapAdr = bgLayer[nlayer].tilemapAdr + (((y >> tileBitsY) & 0x1f) << 5 | ((x >> tileBitsX) & 0x1f));
  //tilemapAdr += 0x400 & (0 - ((x & tileHighBitX) && bgLayer[nlayer].tilemapWider));
  //tilemapAdr += (0x400 << bgLayer[nlayer].tilemapWider) & (0 - ((y & tileHighBitY) && bgLayer[nlayer].tilemapHigher));
  if((x & tileHighBitX) && bgLayer[nlayer].tilemapWider) tilemapAdr += 0x400;
  if((y & tileHighBitY) && bgLayer[nlayer].tilemapHigher) tilemapAdr += bgLayer[nlayer].tilemapWider ? 0x800 : 0x400;
  uint16_t tile = vram[tilemapAdr & 0x7fff];
  // check priority, get palette
  int tilePrio = (tile >> 13) & 1;
  int paletteNum = (tile & 0x1c00) >> 10;
  // figure out position within tile
  int row = (tile & 0x8000) ? (y & 0x7)^7 : (y & 0x7);
//  int row = (y & 0x7) ^ ( 7 & (0 - ((tile & 0x8000) >> 15)));
  int col = (tile & 0x4000) ? (x & 0x7) : (x & 0x7)^7;
//  int col = (x & 0x7) ^ ( 7 & (0 - ((~tile & 0x4000) >> 14)));
  int tileNum = tile & 0x3ff;
  if(wideTiles) {
    // if unflipped right half of tile, or flipped left half of tile
    if(((bool) (x & 8)) ^ ((bool) (tile & 0x4000))) tileNum += 1;
	//tileNum += (((x & 8) >> 3) ^ ((tile & 0x4000) >> 14)) & (0 - wideTiles);
  }
  if(bgLayer[nlayer].bigTiles) {
    // if unflipped bottom half of tile, or flipped upper half of tile
    if(((bool) (y & 8)) ^ ((bool) (tile & 0x8000))) tileNum += 0x10;
	//tileNum += (((x & 8) << 1) ^ ((tile & 0x8000) >> 11)) & (0 - bgLayer[nlayer].bigTiles);
  }
  // read tiledata, ajust palette for mode 0
  const int bitDepth = bitDepthsPerMode[mode][nlayer];
  if(mode == 0) paletteNum += 8 * nlayer;
  // plane 1 (always)
  const uint16_t base_addr = bgLayer[nlayer].tileAdr + ((tileNum & 0x3ff) * 4 * bitDepth);
  const int bit2shift = 8 + col;
  uint16_t pixel = 0;
#if 1
  switch (bitDepth) {
    case 2: {
		uint16_t plane = vram[(base_addr + row) & 0x7fff];
		pixel = (plane >> col) & 1;
		pixel |= ((plane >> bit2shift) & 1) << 1;
	} break;
    case 4: {
		uint16_t plane = vram[(base_addr + row) & 0x7fff];
		pixel = (plane >> col) & 1;
		pixel |= ((plane >> bit2shift) & 1) << 1;

		plane = vram[(base_addr + 8 + row) & 0x7fff];
		pixel |= ((plane >> col) & 1) << 2;
		pixel |= ((plane >> bit2shift) & 1) << 3;
	} break;
    case 8: {
		uint16_t plane = vram[(base_addr + row) & 0x7fff];
		pixel = (plane >> col) & 1;
		pixel |= ((plane >> bit2shift) & 1) << 1;

		plane = vram[(base_addr + 8 + row) & 0x7fff];
		pixel |= ((plane >> col) & 1) << 2;
		pixel |= ((plane >> bit2shift) & 1) << 3;

		plane = vram[(base_addr + 16 + row) & 0x7fff];
		pixel |= ((plane >> col) & 1) << 4;
		pixel |= ((plane >> bit2shift) & 1) << 5;

		plane = vram[(base_addr + 24 + row) & 0x7fff];
		pixel |= ((plane >> col) & 1) << 6;
		pixel |= ((plane >> bit2shift) & 1) << 7;
	} break;
  }
#endif
  // return cgram index, or 0 if transparent, palette number in bits 10-8 for 8-color nlayers
  bg_line_buf[nlayer] = (pixel == 0) ? 0 : (paletteNum << bitDepth) + pixel;
//  *pixelCache = (paletteSize * paletteNum + pixel) & (0 - (pixel != 0));
  bg_prio_buf[nlayer] = tilePrio;
}

static void ppu_calculateMode7Starts(int y) {
  // expand 13-bit values to signed values
  int hScroll = ((int16_t) (m7matrix[6] << 3)) >> 3;
  int vScroll = ((int16_t) (m7matrix[7] << 3)) >> 3;
  int xCenter = ((int16_t) (m7matrix[4] << 3)) >> 3;
  int yCenter = ((int16_t) (m7matrix[5] << 3)) >> 3;
  // do calculation
  int clippedH = hScroll - xCenter;
  int clippedV = vScroll - yCenter;
  clippedH = (clippedH & 0x2000) ? (clippedH | ~1023) : (clippedH & 1023);
  clippedV = (clippedV & 0x2000) ? (clippedV | ~1023) : (clippedV & 1023);
  if(bgLayer[0].mosaicEnabled && mosaicSize > 1) {
    y -= (y - mosaicStartLine) % mosaicSize;
  }
  uint8_t ry = m7yFlip ? 255 - y : y;
  m7startX = (
    ((m7matrix[0] * clippedH) & ~63) +
    ((m7matrix[1] * ry) & ~63) +
    ((m7matrix[1] * clippedV) & ~63) +
    (xCenter << 8)
  );
  m7startY = (
    ((m7matrix[2] * clippedH) & ~63) +
    ((m7matrix[3] * ry) & ~63) +
    ((m7matrix[3] * clippedV) & ~63) +
    (yCenter << 8)
  );
}

static int ppu_getPixelForMode7(int x, int nlayer, bool priority) {
  uint8_t rx = m7xFlip ? 255 - x : x;
  int xPos = (m7startX + m7matrix[0] * rx) >> 8;
  int yPos = (m7startY + m7matrix[2] * rx) >> 8;
  bool outsideMap = xPos < 0 || xPos >= 1024 || yPos < 0 || yPos >= 1024;
  xPos &= 0x3ff;
  yPos &= 0x3ff;
  if(!m7largeField) outsideMap = false;
  uint8_t tile = outsideMap ? 0 : vram[(yPos >> 3) * 128 + (xPos >> 3)] & 0xff;
  uint8_t pixel = outsideMap && !m7charFill ? 0 : vram[tile * 64 + (yPos & 7) * 8 + (xPos & 7)] >> 8;
  if(nlayer == 1) {
    if(((bool) (pixel & 0x80)) != priority) return 0;
    return pixel & 0x7f;
  }
  return pixel;
}

static inline bool ppu_getWindowState(int nlayer, int x) {
  if(!windowLayer[nlayer].window1enabled && !windowLayer[nlayer].window2enabled) {
    return false;
  }
  if(windowLayer[nlayer].window1enabled && !windowLayer[nlayer].window2enabled) {
    bool test = x >= window1left && x <= window1right;
    return windowLayer[nlayer].window1inversed ? !test : test;
  }
  if(!windowLayer[nlayer].window1enabled && windowLayer[nlayer].window2enabled) {
    bool test = x >= window2left && x <= window2right;
    return windowLayer[nlayer].window2inversed ? !test : test;
  }
  bool test1 = x >= window1left && x <= window1right;
  bool test2 = x >= window2left && x <= window2right;
  if(windowLayer[nlayer].window1inversed) test1 = !test1;
  if(windowLayer[nlayer].window2inversed) test2 = !test2;
  switch(windowLayer[nlayer].maskLogic) {
    case 0: return test1 || test2;
    case 1: return test1 && test2;
    case 2: return test1 != test2;
    case 3: return test1 == test2;
  }
  return false;
}

static void ppu_evaluateSprites(int line) {
  // TODO: rectangular sprites
  uint8_t index = objPriority ? (oamAdr & 0xfe) : 0;
  int spritesFound = 0;
  int tilesFound = 0;
  uint8_t foundSprites[32] = {};
  // iterate over oam to find sprites in range
  for(int i = 0; i < 128; i++) {
    uint8_t y = oam[index] >> 8;
    // check if the sprite is on this line and get the sprite size
    uint8_t row = line - y;
    int spriteSize = spriteSizes[objSize][(highOam[index >> 3] >> ((index & 7) + 1)) & 1];
    int spriteHeight = objInterlace ? spriteSize / 2 : spriteSize;
    if(row < spriteHeight) {
      // in y-range, get the x location, using the high bit as well
      int x = oam[index] & 0xff;
	  x |= ((highOam[index >> 3] >> (index & 7)) & 1) << 8;
      if(x > 255) x -= 512;
      // if in x-range, record
      if(x > -spriteSize || x == -256) {
        // break if we found 32 sprites already
        spritesFound++;
        if(spritesFound > 32) {
          rangeOver = true;
          spritesFound = 32;
          break;
        }
        foundSprites[spritesFound - 1] = index;
      }
    }
    index += 2;
  }
  // iterate over found sprites backwards to fetch max 34 tile slivers
  for(int i = spritesFound; i > 0; i--) {
    index = foundSprites[i - 1];
    uint8_t y = oam[index] >> 8;
    uint8_t row = line - y;
    int spriteSize = spriteSizes[objSize][(highOam[index >> 3] >> ((index & 7) + 1)) & 1];
    int x = oam[index] & 0xff;
    x |= ((highOam[index >> 3] >> (index & 7)) & 1) << 8;
    if(x > 255) x -= 512;
    if(x > -spriteSize) {
      // update row according to obj-interlace
      if(objInterlace) row = row * 2 + (evenFrame ? 0 : 1);
      // get some data for the sprite and y-flip row if needed
      int tile = oam[index + 1] & 0xff;
      int palette = (oam[index + 1] & 0xe00) >> 9;
      bool hFlipped = oam[index + 1] & 0x4000;
      if(oam[index + 1] & 0x8000) row = spriteSize - 1 - row;
      // fetch all tiles in x-range
      for(int col = 0; col < spriteSize; col += 8) {
        if(col + x > -8 && col + x < 256) {
          // break if we found > 34 8*1 slivers already
          tilesFound++;
          if(tilesFound > 34) {
            timeOver = true;
            break;
          }
          // figure out which tile this uses, looping within 16x16 pages, and get it's data
          int usedCol = hFlipped ? spriteSize - 1 - col : col;
          uint8_t usedTile = (((tile >> 4) + (row / 8)) << 4) | (((tile & 0xf) + (usedCol / 8)) & 0xf);
          uint16_t objAdr = (oam[index + 1] & 0x100) ? objTileAdr2 : objTileAdr1;
          uint16_t plane1 = vram[(objAdr + usedTile * 16 + (row & 0x7)) & 0x7fff];
          uint16_t plane2 = vram[(objAdr + usedTile * 16 + 8 + (row & 0x7)) & 0x7fff];
          // go over each pixel
          for(int px = 0; px < 8; px++) {
            int shift = hFlipped ? px : 7 - px;
            int pixel = (plane1 >> shift) & 1;
            pixel |= ((plane1 >> (8 + shift)) & 1) << 1;
            pixel |= ((plane2 >> shift) & 1) << 2;
            pixel |= ((plane2 >> (8 + shift)) & 1) << 3;
            // draw it in the buffer if there is a pixel here
            int screenCol = col + x + px;
            if(pixel > 0 && screenCol >= 0 && screenCol < 256) {
              objPixelBuffer[screenCol] = 0x80 + 16 * palette + pixel;
              objPriorityBuffer[screenCol] = (oam[index + 1] & 0x3000) >> 12;
            }
          }
        }
      }
      if(tilesFound > 34) break; // break out of sprite-loop if max tiles found
    }
  }
}

static inline uint16_t ppu_getVramRemap() {
  const uint16_t adr = vramPointer;
  switch(vramRemapMode) {
    case 0: return adr;
    case 1: return (adr & 0xff00) | ((adr & 0xe0) >> 5) | ((adr & 0x1f) << 3);
    case 2: return (adr & 0xfe00) | ((adr & 0x1c0) >> 6) | ((adr & 0x3f) << 3);
    case 3: return (adr & 0xfc00) | ((adr & 0x380) >> 7) | ((adr & 0x7f) << 3);
  }
  return adr;
}

void ppu_latchHV() {
  hCount = snes->hPos / 4;
  vCount = snes->vPos;
  countersLatched = true;
}

void ppu_latchScope(int x, int y) {
	hScope = (0x28 + x) * 4; // x -> cycle
	vScope = y * (ppu_checkOverscan() ? 240 : 225) / 255;
	//bprintf (0, _T("latch scope @ %d,%d  (cycle %d)\n"), x, y, hScope);
	bScopeLatch = true;
}

void ppu_latchScopeCheck(bool reset) {
	if (bScopeLatch) {
		if ((snes->vPos == vScope && snes->hPos >= hScope) || snes->vPos > vScope) {
			hCount = hScope / 4;
			vCount = vScope;
			countersLatched = true;
			bScopeLatch = false;
		}
		if (reset) {
			bScopeLatch = false;
		}
	}
}

uint8_t ppu_read(uint8_t adr) {
  switch(adr) {
    case 0x04: case 0x14: case 0x24:
    case 0x05: case 0x15: case 0x25:
    case 0x06: case 0x16: case 0x26:
    case 0x08: case 0x18: case 0x28:
    case 0x09: case 0x19: case 0x29:
    case 0x0a: case 0x1a: case 0x2a: {
      return ppu1openBus;
    }
    case 0x34:
    case 0x35:
    case 0x36: {
      int result = m7matrix[0] * (m7matrix[1] >> 8);
      ppu1openBus = (result >> (8 * (adr - 0x34))) & 0xff;
      return ppu1openBus;
    }
    case 0x37: {
      if (snes->ppuLatch) {
        ppu_latchHV();
      }
      return snes->openBus;
    }
    case 0x38: {
      uint8_t ret = 0;
      if(oamInHigh) {
        ret = highOam[((oamAdr & 0xf) << 1) | oamSecondWrite];
        if(oamSecondWrite) {
          oamAdr++;
          if(oamAdr == 0) oamInHigh = false;
        }
      } else {
        if(!oamSecondWrite) {
          ret = oam[oamAdr] & 0xff;
        } else {
          ret = oam[oamAdr++] >> 8;
          if(oamAdr == 0) oamInHigh = true;
        }
      }
      oamSecondWrite = !oamSecondWrite;
      ppu1openBus = ret;
      return ret;
    }
    case 0x39: {
      uint16_t val = vramReadBuffer;
      if(!vramIncrementOnHigh) {
        vramReadBuffer = vram[ppu_getVramRemap() & 0x7fff];
        vramPointer += vramIncrement;
      }
      ppu1openBus = val & 0xff;
      return val & 0xff;
    }
    case 0x3a: {
      uint16_t val = vramReadBuffer;
      if(vramIncrementOnHigh) {
        vramReadBuffer = vram[ppu_getVramRemap() & 0x7fff];
        vramPointer += vramIncrement;
      }
      ppu1openBus = val >> 8;
      return val >> 8;
    }
    case 0x3b: {
      uint8_t ret = 0;
      if(!cgramSecondWrite) {
        ret = cgram[cgramPointer] & 0xff;
      } else {
        ret = ((cgram[cgramPointer++] >> 8) & 0x7f) | (ppu2openBus & 0x80);
      }
      cgramSecondWrite = !cgramSecondWrite;
      ppu2openBus = ret;
      return ret;
    }
    case 0x3c: {
      uint8_t val = 0;
      ppu_latchScopeCheck(false);
	  if(hCountSecond) {
        val = ((hCount >> 8) & 1) | (ppu2openBus & 0xfe);
      } else {
        val = hCount & 0xff;
      }
      hCountSecond = !hCountSecond;
      ppu2openBus = val;
      return val;
    }
    case 0x3d: {
      uint8_t val = 0;
      ppu_latchScopeCheck(false);
      if(vCountSecond) {
        val = ((vCount >> 8) & 1) | (ppu2openBus & 0xfe);
      } else {
        val = vCount & 0xff;
      }
      vCountSecond = !vCountSecond;
      ppu2openBus = val;
      return val;
    }
    case 0x3e: {
      uint8_t val = 0x1; // ppu1 version (4 bit)
      val |= ppu1openBus & 0x10;
      val |= rangeOver << 6;
      val |= timeOver << 7;
      ppu1openBus = val;
      return val;
    }
    case 0x3f: {
      uint8_t val = 0x3; // ppu2 version (4 bit)
      ppu_latchScopeCheck(false);
      val |= snes->palTiming << 4; // ntsc/pal
      val |= ppu2openBus & 0x20;
      val |= countersLatched << 6;
      val |= evenFrame << 7;

      if (snes->ppuLatch) {
        countersLatched = false;
        hCountSecond = false;
        vCountSecond = false;
      }
      ppu2openBus = val;
      return val;
    }
    default: {
      return snes->openBus;
    }
  }
}

void ppu_write(uint8_t adr, uint8_t val) {
  switch(adr) {
    case 0x00: {
      // TODO: oam address reset when written on first line of vblank, (and when forced blank is disabled?)
      brightness = val & 0xf;
      forcedBlank = val & 0x80;
      break;
    }
    case 0x01: {
      objSize = val >> 5;
      objTileAdr1 = (val & 7) << 13;
      objTileAdr2 = objTileAdr1 + (((val & 0x18) + 8) << 9);
      break;
    }
    case 0x02: {
      oamAdr = val;
      oamAdrWritten = oamAdr;
      oamInHigh = oamInHighWritten;
      oamSecondWrite = false;
      break;
    }
    case 0x03: {
      objPriority = val & 0x80;
      oamInHigh = val & 1;
      oamInHighWritten = oamInHigh;
      oamAdr = oamAdrWritten;
      oamSecondWrite = false;
      break;
    }
    case 0x04: {
      if(oamInHigh) {
        highOam[((oamAdr & 0xf) << 1) | oamSecondWrite] = val;
        if(oamSecondWrite) {
          oamAdr++;
          if(oamAdr == 0) oamInHigh = false;
        }
      } else {
        if(!oamSecondWrite) {
          oamBuffer = val;
        } else {
          oam[oamAdr++] = (val << 8) | oamBuffer;
          if(oamAdr == 0) oamInHigh = true;
        }
      }
      oamSecondWrite = !oamSecondWrite;
      break;
    }
    case 0x05: {
	  mode = val & 0x7;
	  //extern int counter;
	  //if (counter) bprintf(0, _T("ppu MODE  %x\n"), mode);
	  bg3priority = val & 0x8;
      bgLayer[0].bigTiles = val & 0x10;
      bgLayer[1].bigTiles = val & 0x20;
      bgLayer[2].bigTiles = val & 0x40;
      bgLayer[3].bigTiles = val & 0x80;
      break;
    }
    case 0x06: {
      // TODO: mosaic line reset specifics
      bgLayer[0].mosaicEnabled = val & 0x1;
      bgLayer[1].mosaicEnabled = val & 0x2;
      bgLayer[2].mosaicEnabled = val & 0x4;
      bgLayer[3].mosaicEnabled = val & 0x8;
      mosaicSize = (val >> 4) + 1;
      mosaicStartLine = snes->vPos;
      break;
    }
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0a: {
      bgLayer[adr - 7].tilemapWider = val & 0x1;
      bgLayer[adr - 7].tilemapHigher = val & 0x2;
      bgLayer[adr - 7].tilemapAdr = (val & 0xfc) << 8;
      break;
    }
    case 0x0b: {
      bgLayer[0].tileAdr = (val & 0xf) << 12;
      bgLayer[1].tileAdr = (val & 0xf0) << 8;
      break;
    }
    case 0x0c: {
      bgLayer[2].tileAdr = (val & 0xf) << 12;
      bgLayer[3].tileAdr = (val & 0xf0) << 8;
      break;
    }
    case 0x0d: {
      m7matrix[6] = ((val << 8) | m7prev) & 0x1fff;
      m7prev = val;
      // fallthrough to normal layer BG-HOFS
    }
    case 0x0f:
    case 0x11:
    case 0x13: {
      bgLayer[(adr - 0xd) / 2].hScroll = ((val << 8) | (scrollPrev & 0xf8) | (scrollPrev2 & 0x7)) & 0x3ff;
	  scrollPrev = val;
      scrollPrev2 = val;
      break;
    }
    case 0x0e: {
      m7matrix[7] = ((val << 8) | m7prev) & 0x1fff;
      m7prev = val;
      // fallthrough to normal layer BG-VOFS
    }
    case 0x10:
    case 0x12:
    case 0x14: {
      bgLayer[(adr - 0xe) / 2].vScroll = ((val << 8) | scrollPrev) & 0x3ff;
      scrollPrev = val;
      break;
    }
    case 0x15: {
      if((val & 3) == 0) {
        vramIncrement = 1;
      } else if((val & 3) == 1) {
        vramIncrement = 32;
      } else {
        vramIncrement = 128;
      }
      vramRemapMode = (val & 0xc) >> 2;
      vramIncrementOnHigh = val & 0x80;
      break;
    }
    case 0x16: {
      vramPointer = (vramPointer & 0xff00) | val;
      vramReadBuffer = vram[ppu_getVramRemap() & 0x7fff];
      break;
    }
    case 0x17: {
      vramPointer = (vramPointer & 0x00ff) | (val << 8);
      vramReadBuffer = vram[ppu_getVramRemap() & 0x7fff];
      break;
    }
    case 0x18: {
      uint16_t vramAdr = ppu_getVramRemap();
	  if (forcedBlank || snes->inVblank) { // TODO: also cgram and oam?
		  vram[vramAdr & 0x7fff] = (vram[vramAdr & 0x7fff] & 0xff00) | val;
	  }
      if(!vramIncrementOnHigh) vramPointer += vramIncrement;
      break;
    }
    case 0x19: {
      uint16_t vramAdr = ppu_getVramRemap();
	  if (forcedBlank || snes->inVblank) {
		  vram[vramAdr & 0x7fff] = (vram[vramAdr & 0x7fff] & 0x00ff) | (val << 8);
	  }
      if(vramIncrementOnHigh) vramPointer += vramIncrement;
      break;
    }
    case 0x1a: {
      m7largeField = val & 0x80;
      m7charFill = val & 0x40;
      m7yFlip = val & 0x2;
      m7xFlip = val & 0x1;
      break;
    }
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e: {
      m7matrix[adr - 0x1b] = (val << 8) | m7prev;
      m7prev = val;
      break;
    }
    case 0x1f:
    case 0x20: {
      m7matrix[adr - 0x1b] = ((val << 8) | m7prev) & 0x1fff;
      m7prev = val;
      break;
    }
    case 0x21: {
      cgramPointer = val;
      cgramSecondWrite = false;
      break;
    }
    case 0x22: {
      if(!cgramSecondWrite) {
        cgramBuffer = val;
      } else {
        cgram[cgramPointer++] = (val << 8) | cgramBuffer;
      }
      cgramSecondWrite = !cgramSecondWrite;
      break;
    }
    case 0x23:
    case 0x24:
    case 0x25: {
      windowLayer[(adr - 0x23) * 2].window1inversed = val & 0x1;
      windowLayer[(adr - 0x23) * 2].window1enabled = val & 0x2;
      windowLayer[(adr - 0x23) * 2].window2inversed = val & 0x4;
      windowLayer[(adr - 0x23) * 2].window2enabled = val & 0x8;
      windowLayer[(adr - 0x23) * 2 + 1].window1inversed = val & 0x10;
      windowLayer[(adr - 0x23) * 2 + 1].window1enabled = val & 0x20;
      windowLayer[(adr - 0x23) * 2 + 1].window2inversed = val & 0x40;
      windowLayer[(adr - 0x23) * 2 + 1].window2enabled = val & 0x80;
      break;
    }
    case 0x26: {
      window1left = val;
      break;
    }
    case 0x27: {
      window1right = val;
      break;
    }
    case 0x28: {
      window2left = val;
      break;
    }
    case 0x29: {
      window2right = val;
      break;
    }
    case 0x2a: {
      windowLayer[0].maskLogic = val & 0x3;
      windowLayer[1].maskLogic = (val >> 2) & 0x3;
      windowLayer[2].maskLogic = (val >> 4) & 0x3;
      windowLayer[3].maskLogic = (val >> 6) & 0x3;
      break;
    }
    case 0x2b: {
      windowLayer[4].maskLogic = val & 0x3;
      windowLayer[5].maskLogic = (val >> 2) & 0x3;
      break;
    }
    case 0x2c: {
      layer[0].mainScreenEnabled = val & 0x1;
      layer[1].mainScreenEnabled = val & 0x2;
      layer[2].mainScreenEnabled = val & 0x4;
      layer[3].mainScreenEnabled = val & 0x8;
      layer[4].mainScreenEnabled = val & 0x10;
      break;
    }
    case 0x2d: {
      layer[0].subScreenEnabled = val & 0x1;
      layer[1].subScreenEnabled = val & 0x2;
      layer[2].subScreenEnabled = val & 0x4;
      layer[3].subScreenEnabled = val & 0x8;
      layer[4].subScreenEnabled = val & 0x10;
      break;
    }
    case 0x2e: {
      layer[0].mainScreenWindowed = val & 0x1;
      layer[1].mainScreenWindowed = val & 0x2;
      layer[2].mainScreenWindowed = val & 0x4;
      layer[3].mainScreenWindowed = val & 0x8;
      layer[4].mainScreenWindowed = val & 0x10;
      break;
    }
    case 0x2f: {
      layer[0].subScreenWindowed = val & 0x1;
      layer[1].subScreenWindowed = val & 0x2;
      layer[2].subScreenWindowed = val & 0x4;
      layer[3].subScreenWindowed = val & 0x8;
      layer[4].subScreenWindowed = val & 0x10;
      break;
    }
    case 0x30: {
      directColor = val & 0x1;
      addSubscreen = val & 0x2;
      preventMathMode = (val & 0x30) >> 4;
      clipMode = (val & 0xc0) >> 6;
      break;
    }
    case 0x31: {
      subtractColor = val & 0x80;
      halfColor = val & 0x40;
//	  bprintf(0, _T("math enab: "));
      for(int i = 0; i < 6; i++) {
        mathEnabled[i] = val & (1 << i);
//		bprintf(0, _T("%02x  "), mathEnabled[i]);
	  }
//		  bprintf(0, _T("\n"));
      break;
    }
    case 0x32: {
      if(val & 0x80) fixedColorB = val & 0x1f;
      if(val & 0x40) fixedColorG = val & 0x1f;
      if(val & 0x20) fixedColorR = val & 0x1f;
      break;
    }
    case 0x33: {
      interlace = val & 0x1;
      objInterlace = val & 0x2;
      overscan = val & 0x4;
      pseudoHires = val & 0x8;
      m7extBg = val & 0x40;
      break;
    }
    default: {
      break;
    }
  }
}

void ppu_putPixels(uint8_t* pixels, int height) {
  for(int y = 0; y < (frameOverscan ? 239 : 224); y++) {
    int dest = y * 2;// + (frameOverscan ? 2 : 16);
    int y1 = y, y2 = y + 239;
    if(!frameInterlace) {
      y1 = y + (evenFrame ? 0 : 239);
      y2 = y1;
	}
	if (dest + 1 >= height) continue;
    memcpy(pixels + ((dest + 0) * 2048), &pixelBuffer[y1 * 2048], 2048);
    memcpy(pixels + ((dest + 1) * 2048), &pixelBuffer[y2 * 2048], 2048);
  }
#if 0
  // we just don't show this, anyways -dink
  // clear top 2 lines, and following 14 and last 16 lines if not overscanning
  memset(pixels, 0, 2048 * 2);
  if(!frameOverscan) {
    memset(pixels + (2 * 2048), 0, 2048 * 14);
    memset(pixels + (464 * 2048), 0, 2048 * 16);
  }
#endif
}
