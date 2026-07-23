// =============================================================================
//  FBNeo SNES  -  ST018 (Seta ARMv3 / ARM7TDMI) coprocessor  -  public interface
// =============================================================================

#ifndef ST018_H
#define ST018_H

#include "burnint.h"
#include "statehandler.h"

typedef struct Snes Snes;

// -----------------------------------------------------------------------------
//  Lifecycle  (called from cart.cpp)
// -----------------------------------------------------------------------------、
void snes_st018_init(void* mem, UINT8* bios, INT32 biosSize);
void snes_st018_reset();
void snes_st018_exit();
void snes_st018_run();                 // catch-up: advance ARM to the S-CPU clock
void snes_st018_handleState(StateHandler* sh);

// -----------------------------------------------------------------------------
//  S-CPU bus bridge  ($3800-$3807 in banks $00-$3f / $80-$bf)
// -----------------------------------------------------------------------------
UINT8 snes_st018_read(UINT32 address, UINT8 openbus);
void  snes_st018_write(UINT32 address, UINT8 data);

#endif
