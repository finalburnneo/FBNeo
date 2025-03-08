
#ifndef SPC_H
#define SPC_H

#include <stdint.h>
#include <stdbool.h>
#include "burnint.h"

#include "statehandler.h"

typedef uint8_t (*SpcReadHandler)(void* mem, uint16_t adr);
typedef void (*SpcWriteHandler)(void* mem, uint16_t adr, uint8_t val);
typedef void (*SpcIdleHandler)(void* mem, bool waiting);

typedef struct Spc Spc;

struct Spc {
  // reference to memory handler, pointers to read/write/idle handlers
  void* mem;
  SpcReadHandler read;
  SpcWriteHandler write;
  SpcIdleHandler idle;
  // registers
  uint8_t a;
  uint8_t x;
  uint8_t y;
  uint8_t sp;
  uint16_t pc;
  // flags
  bool c;
  bool z;
  bool v;
  bool n;
  bool i;
  bool h;
  bool p;
  bool b;
  // stopping
  bool stopped;
  // reset
  bool resetWanted;
  // single-cycle
  uint8_t opcode;
  uint32_t step;
  uint32_t bstep;
  uint16_t adr;
  uint16_t adr1;
  uint8_t dat;
  uint16_t dat16;
  uint8_t param;
};

Spc* spc_init(void* mem, SpcReadHandler read, SpcWriteHandler write, SpcIdleHandler idle);
void spc_free(Spc* spc);
void spc_reset(Spc* spc, bool hard);
void spc_handleState(Spc* spc, StateHandler* sh);
void spc_runOpcode(Spc* spc);

#endif
