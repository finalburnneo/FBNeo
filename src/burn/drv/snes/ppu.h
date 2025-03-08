
#ifndef PPU_H
#define PPU_H

#include <stdint.h>
#include <stdbool.h>

#include "snes.h"
#include "statehandler.h"

typedef struct BgLayer {
  uint16_t hScroll;
  uint16_t vScroll;
  bool tilemapWider;
  bool tilemapHigher;
  uint16_t tilemapAdr;
  uint16_t tileAdr;
  bool bigTiles;
  bool mosaicEnabled;
} BgLayer;

typedef struct Layer {
  bool mainScreenEnabled;
  bool subScreenEnabled;
  bool mainScreenWindowed;
  bool subScreenWindowed;
} Layer;

typedef struct WindowLayer {
  bool window1enabled;
  bool window2enabled;
  bool window1inversed;
  bool window2inversed;
  uint8_t maskLogic;
} WindowLayer;

void ppu_init(Snes* ssnes);
void ppu_free();
void ppu_reset();
void ppu_handleState(StateHandler* sh);
bool ppu_checkOverscan();
void ppu_handleVblank();
void ppu_handleFrameStart();
void ppu_runLine(int line);
void ppu_latchMode7(int line);
uint8_t ppu_read(uint8_t adr);
void ppu_write(uint8_t adr, uint8_t val);
void ppu_latchHV();
void ppu_latchScope(int x, int y);
void ppu_latchScopeCheck(bool reset);
void ppu_putPixels(uint8_t* pixels, int height);
void ppu_setPixelOutputFormat(int pixelOutputFormat);
bool ppu_frameInterlace();
bool ppu_evenFrame();

#endif
