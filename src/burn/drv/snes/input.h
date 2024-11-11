
#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Input Input;

#include "snes.h"
#include "statehandler.h"

enum { DEVICE_NONE = 0, DEVICE_GAMEPAD = 1, DEVICE_SUPERSCOPE = 2, DEVICE_MOUSE = 3 };
enum { SCOPE_FIRE = 1 << 0, SCOPE_CURSOR = 1 << 1, SCOPE_TURBO = 1 << 2, SCOPE_PAUSE = 1 << 3, SCOPE_RELOAD = 1 << 6 };

struct Input {
  Snes* snes;
  uint8_t type;
  uint8_t portTag;
  // latchline
  bool latchLine;
  // for controller
  uint32_t currentState; // actual state
  uint32_t latchedState;
  // for mouse
  uint8_t lastX, lastY;
  uint8_t mouseSens;
};

Input* input_init(Snes* snes, int8_t tag);
void input_free(Input* input);
void input_reset(Input* input);
void input_handleState(Input* input, StateHandler* sh);
void input_latch(Input* input, bool value);
uint8_t input_read(Input* input);
void input_setMouse(Input* input, int16_t x, int16_t y, uint8_t buttonA, uint8_t buttonB);
void input_setType(Input* input, uint8_t type);

#endif
