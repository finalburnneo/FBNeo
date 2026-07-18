
#ifndef CART_H
#define CART_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Cart Cart;

#include "snes.h"
#include "statehandler.h"

enum { CART_NONE = 0, CART_LOROM, CART_HIROM, CART_EXLOROM, CART_EXHIROM, CART_CX4, CART_LOROMDSP, CART_HIROMDSP, CART_LOROMSETA, CART_LOROMSA1, CART_LOROMOBC1, CART_LOROMSDD1, CART_SUPERFX, CART_SPC7110, CART_MAXENTRY };

struct Cart {
  Snes* snes;
  uint8_t type;
  bool hasBattery;
  bool heavySync;
  uint8_t* rom;
  uint32_t romSize;
  uint32_t romTrueSize; // true image size before power-of-2 mirror padding (SPC7110 drom/erom split)
  uint8_t* ram;
  uint32_t ramSize;
  uint8_t* bios;
  uint32_t biosSize;
  uint32_t oscillator;
  uint32_t promSize; // SPC7110: program-ROM size (data-ROM = romSize - promSize - eromSize)
  uint32_t eromSize; // SPC7110: expansion-ROM size (EXSPC7110 board, mapped $40-4f); 0 = absent
  bool hasRTC;       // SPC7110: Epson RTC-4513 present (Tengai Makyou Zero)
};

Cart* cart_init(Snes* snes);
void cart_free(Cart* cart);
void cart_reset(Cart* cart); // will reset special chips etc, general reading is set up in load
bool cart_handleTypeState(Cart* cart, StateHandler* sh);
void cart_handleState(Cart* cart, StateHandler* sh);
void cart_load(Cart* cart, int type, uint8_t* rom, int romSize, uint8_t* biosrom, int biosromSize, int ramSize, bool ramFill, bool hasBattery); // loads rom, sets up ram buffer
bool cart_handleBattery(Cart* cart, bool save, uint8_t* data, int* size); // saves/loads ram
const char *cart_gettype(int ctype);
extern uint8_t (*cart_read)(Cart* cart, uint8_t bank, uint16_t adr);
extern void (*cart_write)(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val);
extern void (*cart_run)(); // runs special co-processor chips, if avail

#endif
