#ifndef SPC7110_H
#define SPC7110_H

#include "statehandler.h"

void  snes_spc7110_init(UINT8* rom, INT32 romSize, UINT8* ram, INT32 ramSize, INT32 promSize, INT32 eromSize, INT32 hasRTC);
void  snes_spc7110_exit();
void  snes_spc7110_reset();
void  snes_spc7110_handleState(StateHandler* sh);

UINT8 snes_spc7110_cart_read(UINT32 address, UINT8 openbus);
void  snes_spc7110_cart_write(UINT32 address, UINT8 data);

#endif
