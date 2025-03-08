
#ifndef STATEHANDLER_H
#define STATEHANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "burnint.h"

typedef struct StateHandler {
  bool saving;
  int offset;
  uint8_t* data;
  int allocSize;
} StateHandler;

StateHandler* sh_init(bool saving, const uint8_t* data, int size);
void sh_free(StateHandler* sh);

void sh_handleBools(StateHandler* sh, ...);
void sh_handleBytes(StateHandler* sh, ...);
void sh_handleBytesS(StateHandler* sh, ...);
void sh_handleWords(StateHandler* sh, ...);
void sh_handleWordsS(StateHandler* sh, ...);
void sh_handleLongLongs(StateHandler* sh, ...);
void sh_handleInts(StateHandler* sh, ...);
void sh_handleIntsS(StateHandler* sh, ...);
void sh_handleFloats(StateHandler* sh, ...);
void sh_handleDoubles(StateHandler* sh, ...);
void sh_handleByteArray(StateHandler* sh, uint8_t* data, int size);
void sh_handleWordArray(StateHandler* sh, uint16_t* data, int size);
void sh_placeInt(StateHandler* sh, int location, uint32_t value);

#endif
