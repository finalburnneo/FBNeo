
#ifndef CART_H
#define CART_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Cart Cart;

#include "snes.h"
#include "statehandler.h"

//const char* typeNames[12] = {"(none)", "LoROM", "HiROM", "ExLoROM", "ExHiROM", "CX4", "LoROM-DSP", "HiROM-DSP", "LoROM-SeTa", "LoROM-SA1", "LoROM-OBC1", "LoROM-SDD1"};
enum { CART_NONE = 0, CART_LOROM, CART_HIROM, CART_EXLOROM, CART_EXHIROM, CART_CX4, CART_LOROMDSP, CART_HIROMDSP, CART_LOROMSETA, CART_LOROMSA1, CART_LOROMOBC1, CART_LOROMSDD1 };

struct Cart {
  Snes* snes;
  uint8_t type;
  bool hasBattery;
  bool heavySync;
  uint8_t* rom;
  uint32_t romSize;
  uint8_t* ram;
  uint32_t ramSize;
  uint8_t* bios;
  uint32_t biosSize;
};

// TODO: how to handle reset & load?

Cart* cart_init(Snes* snes);
void cart_free(Cart* cart);
void cart_reset(Cart* cart); // will reset special chips etc, general reading is set up in load
bool cart_handleTypeState(Cart* cart, StateHandler* sh);
void cart_handleState(Cart* cart, StateHandler* sh);
void cart_load(Cart* cart, int type, uint8_t* rom, int romSize, uint8_t* biosrom, int biosromSize, int ramSize, bool hasBattery); // loads rom, sets up ram buffer
bool cart_handleBattery(Cart* cart, bool save, uint8_t* data, int* size); // saves/loads ram
extern uint8_t (*cart_read)(Cart* cart, uint8_t bank, uint16_t adr);
extern void (*cart_write)(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val);
extern void (*cart_run)(); // runs special co-processor chips, if avail

#endif
