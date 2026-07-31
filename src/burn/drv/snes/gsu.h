// =============================================================================
//  FBNeo SNES  -  SuperFX (GSU) coprocessor  -  public interface
// =============================================================================

#ifndef GSU_H
#define GSU_H

#include "burnint.h"
#include "statehandler.h"

#define SNES_GSU_1 1
#define SNES_GSU_2 2
#define SNES_GSU_3 3

typedef struct Snes Snes;
void snes_gsu_init(void* mem, UINT8* rom, INT32 romSize, UINT8* ram, INT32 ramSize, UINT8 gsuType, UINT32 oscillator);
void snes_gsu_reset();
void snes_gsu_exit();
void snes_gsu_run();
void snes_gsu_handleState(StateHandler* sh);

UINT8 snes_gsu_cart_read(UINT32 address);
void  snes_gsu_cart_write(UINT32 address, UINT8 data);

#endif
