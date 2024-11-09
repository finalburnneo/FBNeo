\
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "input.h"
#include "snes.h"
#include "statehandler.h"

Input* input_init(Snes* snes, int8_t tag) {
  Input* input = (Input*)BurnMalloc(sizeof(Input));
  input->snes = snes;
  // TODO: handle (where?)
  input->portTag = tag; // 1, 2, ...
  input->type = DEVICE_GAMEPAD;
  input->currentState = 0;
  // TODO: handle I/O line (and latching of PPU)
  return input;
}

void input_free(Input* input) {
  BurnFree(input);
}

void input_reset(Input* input) {
  input->latchLine = false;
  input->latchedState = 0;
  input->currentState = 0;
  input->lastX = 0;
  input->lastY = 0;
  input->mouseSens = 0;
}

void input_handleState(Input* input, StateHandler* sh) {
  sh_handleBytes(sh, &input->type, &input->lastX, &input->lastY, &input->mouseSens, NULL);
  sh_handleBools(sh, &input->latchLine, NULL);
  sh_handleInts(sh, &input->currentState, &input->latchedState, NULL);
}

void input_setType(Input* input, uint8_t type) {
  input->type = type;
}

#define d_min(a, b) (((a) < (b)) ? (a) : (b))
#define d_abs(z) (((z) < 0) ? -(z) : (z))

void input_setMouse(Input* input, int16_t x, int16_t y, uint8_t buttonA, uint8_t buttonB) {
  x >>= 8;
  y >>= 8;
  input->lastX = (x) ? (x < 0) << 7 : input->lastX;
  input->lastY = (y) ? (y < 0) << 7 : input->lastY;
  x = d_min(d_abs(x), 0x7f);
  y = d_min(d_abs(y), 0x7f);
  input->currentState  = 0;
  input->currentState |= (0x01 | (input->mouseSens % 3) << 4 | (buttonA) << 6 | (buttonB) << 7) << 16;
  input->currentState |= (y | input->lastY) << 8;
  input->currentState |= (x | input->lastX) << 0;
}

static void update_mouse_sensitivity(Input* input) {
  input->currentState = (input->currentState & ~0x300000) | ((input->mouseSens % 3) << (4 + 16));
}

void input_latch(Input* input, bool value) {
//  if(input->latchLine && !value) {
  if(value) {
    input->latchedState = input->currentState;
	if (input->type == DEVICE_MOUSE) {
      update_mouse_sensitivity(input);
	}
  }
  input->latchLine = value;
}

uint8_t input_read(Input* input) {
  if(input->latchLine) {
    input->latchedState = input->currentState;
  }
  uint8_t ret = 0;
  switch (input->type) {
    case DEVICE_NONE:
      input_reset(input);
      ret = 0x00;
      break;
    case DEVICE_MOUSE:
      if (input->latchLine) {
        // fun feature: reading the mouse while latched changes the sensitivty
        // setting
	    update_mouse_sensitivity(input);
        input->mouseSens++;
      }
      ret = (input->latchedState >> 31) & 1;
      input->latchedState <<= 1;
      input->latchedState |= 1;
      break;
    case DEVICE_GAMEPAD:
    case DEVICE_SUPERSCOPE:
    default:
      ret = input->latchedState & 1;
      input->latchedState >>= 1;
      input->latchedState |= 0x8000;
      break;
  }
  return ret;
}
