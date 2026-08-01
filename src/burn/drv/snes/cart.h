
#ifndef CART_H
#define CART_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Cart Cart;

#include "snes.h"
#include "statehandler.h"

enum { CART_NONE = 0, CART_LOROM, CART_HIROM, CART_EXLOROM, CART_EXHIROM, CART_CX4, CART_LOROMDSP, CART_HIROMDSP, CART_LOROMSETA, CART_LOROMSA1, CART_LOROMOBC1, CART_LOROMSDD1, CART_SUPERFX, CART_SPC7110, CART_ST018, CART_SFEX, CART_SUPERFX3, CART_MAXENTRY };

struct Cart {
  Snes*  snes;
  UINT8  type;
  bool   hasBattery;
  bool   heavySync;
  UINT8* rom;
  UINT32 romSize;
  UINT32 romTrueSize;			// true image size before power-of-2 mirror padding (SPC7110 drom/erom split)
  UINT8* ram;
  UINT32 ramSize;
  UINT8* bios;
  UINT32 biosSize;
  UINT32 oscillator;
  UINT32 promSize;				// SPC7110: program-ROM size (data-ROM = romSize - promSize - eromSize)
  UINT32 eromSize;				// SPC7110: expansion-ROM size (EXSPC7110 board, mapped $40-4f); 0 = absent
  bool   hasRTC;				// SPC7110: Epson RTC-4513 present (Tengai Makyou Zero)
  UINT8  msu1Enable;
  UINT8  sfexTag;
  UINT8  gsuType;
};

Cart* cart_init(Snes* snes);
void  cart_free(Cart* cart);
void  cart_reset(Cart* cart);	// will reset special chips etc, general reading is set up in load
bool  cart_handleTypeState(Cart* cart, StateHandler* sh);
void  cart_handleState(Cart* cart, StateHandler* sh);
void  cart_load(Cart* cart, INT32 type, UINT8* rom, INT32 romSize, UINT8* biosrom, INT32 biosromSize, INT32 ramSize, bool ramFill, bool hasBattery); // loads rom, sets up ram buffer
bool  cart_handleBattery(Cart* cart, bool save, UINT8* data, INT32* size); // saves/loads ram
const char *cart_gettype(INT32 ctype);
extern UINT8 (*cart_read)(Cart* cart, UINT8 bank, UINT16 adr);
extern void (*cart_write)(Cart* cart, UINT8 bank, UINT16 adr, UINT8 val);
extern void (*cart_run)();		// runs special co-processor chips, if avail

#endif
