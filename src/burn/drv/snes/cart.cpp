// todo:
// dsp_bios_reform(): support all format variants of the dsp bios?

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "cart.h"
#include "snes.h"
#include "statehandler.h"
#include "cx4.h"
#include "upd7725.h"
#include "sa1.h"
#include "sdd1.h"

static uint8_t cart_readDummy(Cart* cart, uint8_t bank, uint16_t adr);
static void cart_writeDummy(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val);
static uint8_t cart_readLorom(Cart* cart, uint8_t bank, uint16_t adr);
static void cart_writeLorom(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val);
static uint8_t cart_readExLorom(Cart* cart, uint8_t bank, uint16_t adr);
static uint8_t cart_readLoromSeta(Cart* cart, uint8_t bank, uint16_t adr);
static void cart_writeLoromSeta(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val);
static uint8_t cart_readLoromDSP(Cart* cart, uint8_t bank, uint16_t adr);
static void cart_writeLoromDSP(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val);
static uint8_t cart_readHirom(Cart* cart, uint8_t bank, uint16_t adr);
static uint8_t cart_readExHirom(Cart* cart, uint8_t bank, uint16_t adr);
static void cart_writeHirom(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val);
static uint8_t cart_readHiromDSP(Cart* cart, uint8_t bank, uint16_t adr);
static void cart_writeHiromDSP(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val);
static uint8_t cart_readCX4(Cart* cart, uint8_t bank, uint16_t adr);
static void cart_writeCX4(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val);
static uint8_t cart_readLoromSA1(Cart* cart, uint8_t bank, uint16_t adr);
static void cart_writeLoromSA1(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val);
static uint8_t cart_readLoromOBC1(Cart* cart, uint8_t bank, uint16_t adr);
static void cart_writeLoromOBC1(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val);
static uint8_t cart_readLoromSDD1(Cart* cart, uint8_t bank, uint16_t adr);
static void cart_writeLoromSDD1(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val);

uint8_t (*cart_read)(Cart* cart, uint8_t bank, uint16_t adr) = NULL;
void (*cart_write)(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val) = NULL;
void (*cart_run)() = NULL;
void cart_run_dummy() { }
void cart_mapRun(Cart* cart);

const char *cart_gettype(int ctype) {
  const char* cartTypeNames[CART_MAXENTRY] = {"(none)", "LoROM", "HiROM", "ExLoROM", "ExHiROM", "CX4", "LoROM-DSP", "HiROM-DSP", "LoROM-SeTa", "LoROM-SA1", "LoROM-OBC1", "LoROM-SDD1"};
  return cartTypeNames[(ctype < CART_MAXENTRY) ? ctype : 0];
}

static void cart_mapRwHandlers(Cart* cart) {
  switch(cart->type) {
    case CART_NONE: cart_read = cart_readDummy; cart_write = cart_writeDummy; break;
    case CART_LOROM: cart_read = cart_readLorom; cart_write = cart_writeLorom; break;
    case CART_HIROM: cart_read = cart_readHirom; cart_write = cart_writeHirom; break;
    case CART_EXLOROM: cart_read = cart_readExLorom; cart_write = cart_writeLorom; break;
	case CART_EXHIROM: cart_read = cart_readExHirom; cart_write = cart_writeHirom; break;
	case CART_CX4: cart_read = cart_readCX4; cart_write = cart_writeCX4; break;

	case CART_LOROMDSP: cart_read = cart_readLoromDSP; cart_write = cart_writeLoromDSP; break;
    case CART_HIROMDSP: cart_read = cart_readHiromDSP; cart_write = cart_writeHiromDSP; break;
    case CART_LOROMSETA: cart_read = cart_readLoromSeta; cart_write = cart_writeLoromSeta; break;
    case CART_LOROMSA1: cart_read = cart_readLoromSA1; cart_write = cart_writeLoromSA1; break;
    case CART_LOROMOBC1: cart_read = cart_readLoromOBC1; cart_write = cart_writeLoromOBC1; break;
    case CART_LOROMSDD1: cart_read = cart_readLoromSDD1; cart_write = cart_writeLoromSDD1; break;
	default:
	  bprintf(0, _T("cart_mapRwHandlers(): invalid type specified: %x\n"), cart->type); break;
  }
}

Cart* cart_init(Snes* snes) {
  Cart* cart = (Cart*)BurnMalloc(sizeof(Cart));
  cart->snes = snes;
  cart->type = CART_NONE;
  cart_mapRwHandlers(cart);
  cart_mapRun(cart);
  cart->hasBattery = 0;
  cart->rom = NULL;
  cart->romSize = 0;
  cart->ram = NULL;
  cart->ramSize = 0;
  return cart;
}

// upd (dsp 1, 1b, 2, 3, 4) note, 1a and 1 are the same firmware
// Seta st010, st011
// TODO: put upd/dsp stuff into upd.cpp/h
static double upd_CyclesPerMaster;
static uint64_t upd_cycles;
static uint8_t *upd_ram = NULL;
static Snes *upd_snes_ctx;

void upd_run() {
	const uint64_t upd_sync_to = (uint64_t)upd_snes_ctx->cycles * upd_CyclesPerMaster;

	int to_run = (int)((uint64_t)upd_sync_to - upd_cycles);
	if (to_run > 0) {
		upd_cycles += upd96050Run(to_run);
	}
}

void cart_free(Cart* cart) {
  switch (cart->type) {
    case CART_LOROMSA1: snes_sa1_exit(); break;
    case CART_LOROMSDD1: snes_sdd1_exit(); break;
  }

  if(cart->rom != NULL) BurnFree(cart->rom);
  if(cart->ram != NULL) BurnFree(cart->ram);
  if(cart->bios != NULL) BurnFree(cart->bios);
  if(upd_ram != NULL) BurnFree(upd_ram);

  BurnFree(cart);
}

void cart_mapRun(Cart* cart) {
  switch (cart->type) {
    case CART_CX4: cart_run = cx4_run; break;

	case CART_LOROMSA1: cart_run = snes_sa1_run; cart->heavySync = true; break;

	case CART_LOROMDSP:
	case CART_HIROMDSP:
	case CART_LOROMSETA: cart_run = upd_run; break;

    default: cart_run = cart_run_dummy; cart->heavySync = false; break;
  }
}

void cart_reset(Cart* cart) {
  // do not reset ram, assumed to be battery backed
  bprintf(0, _T("cart reset!  type  %x\n"), cart->type);
  switch (cart->type) {
	case CART_LOROMSA1:
	  snes_sa1_init(cart->snes, cart->rom, cart->romSize, cart->ram, cart->ramSize);
	  snes_sa1_reset();
	  bprintf(0, _T("init/reset sa-1\n"));
	break;
	case CART_LOROMSDD1:
	  snes_sdd1_init(cart->rom, cart->romSize, cart->ram, cart->ramSize);
	  snes_sdd1_reset();
	  bprintf(0, _T("init/reset sdd-1\n"));
	break;
    case CART_CX4: // capcom cx4
      cx4_init(cart->snes);
      cx4_reset();
      bprintf(0, _T("init/reset cx4\n"));
      break;
    case CART_LOROMDSP: // dsp lorom
    case CART_HIROMDSP: // dsp hirom
		upd_snes_ctx = cart->snes;
		memset(upd_ram, 0, 0x200);
		upd96050Init(7725, cart->bios, cart->bios + 0x2000, upd_ram, NULL, NULL);
		upd96050Reset();
		upd_CyclesPerMaster = (double)8000000 / ((cart->snes->palTiming) ? (1364 * 312 * 50.0) : (1364 * 262 * 60.0));
		upd_cycles = 0;
		bprintf(0, _T("init/reset dsp\n"));
      break;
    case CART_LOROMSETA: // Seta ST010/ST011 LoROM
		upd_snes_ctx = cart->snes;
		memset(upd_ram, 0, 0x1000);
		upd96050Init(96050, cart->bios, cart->bios + 0x10000, upd_ram, NULL, NULL);
		upd96050Reset();
		upd_CyclesPerMaster = (double)11000000 / ((cart->snes->palTiming) ? (1364 * 312 * 50.0) : (1364 * 262 * 60.0));
		upd_cycles = 0;
		bprintf(0, _T("init/reset seta-dsp\n"));
      break;
	  case CART_LOROMOBC1:
		 //?
	  break;
  }
}

bool cart_handleTypeState(Cart* cart, StateHandler* sh) {
  // when loading, return if values match
  if(sh->saving) {
    sh_handleBytes(sh, &cart->type, NULL);
    sh_handleInts(sh, &cart->romSize, &cart->ramSize, NULL);
    return true;
  } else {
    uint8_t type = 0;
    uint32_t romSize = 0;
    uint32_t ramSize = 0;
    sh_handleBytes(sh, &type, NULL);
    sh_handleInts(sh, &romSize, &ramSize, NULL);
    return !(type != cart->type || romSize != cart->romSize || ramSize != cart->ramSize);
  }
}

void upd_handleState(StateHandler* sh, int cart_type) {
	void *upd_data = NULL;
	int32_t upd_data_size = upd96050ExportRegs(&upd_data);

	sh_handleByteArray(sh, (uint8_t*)upd_data, upd_data_size); // from upd7725 cpu core
	sh_handleByteArray(sh, upd_ram, (cart_type == CART_LOROMSETA) ? 0x1000 : 0x200); // from upd7725 cpu core

	sh_handleLongLongs(sh, &upd_cycles, NULL);
}

void cart_handleState(Cart* cart, StateHandler* sh) {
  if(cart->ram != NULL) sh_handleByteArray(sh, cart->ram, cart->ramSize);

  switch(cart->type) {
	  case CART_CX4: cx4_handleState(sh); break;
	  case CART_LOROMSA1: snes_sa1_handleState(sh); break;
	  case CART_LOROMDSP:
	  case CART_HIROMDSP:
	  case CART_LOROMSETA: upd_handleState(sh, cart->type); break;
	  case CART_LOROMSDD1: snes_sdd1_handleState(sh); break;
  }
}

static void dsp_bios_reform(uint8_t *ori_bios, uint8_t *new_bios, int is_seta) {
	const int bios_sizes[2] = { 0x2000, 0x10000 };
	const int data_sizes[2] = { 0x800,  0x1000 };

	bprintf(0, _T("bios_reform:  seta? %x\n"), is_seta);

	int bios_size = bios_sizes[is_seta];
	int data_size = data_sizes[is_seta];
	bprintf(0, _T("bios size / data size:  %x  %x\n"), bios_size, data_size);
	uint32_t *newbios = (uint32_t*)new_bios;
	for (int i = 0; i < bios_size; i += 4) {
		*newbios = (ori_bios[i + 0] << 24) | (ori_bios[i + 1] << 16) | (ori_bios[i + 2] << 8); // only uses 24bits!
		newbios++;
	}

	UINT16 *newbios_data = (UINT16*)&new_bios[bios_size];
	for (int i = 0; i < data_size; i += 2) {
		*newbios_data = (ori_bios[bios_size + i + 0] << 8) | (ori_bios[bios_size + i + 1] << 0);
		newbios_data++;
	}
}

void cart_load(Cart* cart, int type, uint8_t* rom, int romSize, uint8_t* biosrom, int biosromSize, int ramSize, bool hasBattery) {
  cart->type = type;
  cart_mapRwHandlers(cart);
  cart_mapRun(cart);
  cart->hasBattery = hasBattery;
  if(cart->rom != NULL) BurnFree(cart->rom);
  if(cart->ram != NULL) BurnFree(cart->ram);
  if(cart->bios != NULL) BurnFree(cart->bios);
  cart->rom = BurnMalloc(romSize);
  cart->romSize = romSize;
  if(ramSize > 0) {
    cart->ram = BurnMalloc(ramSize);
  } else {
    cart->ram = NULL;
  }
  // dsp[1-4] & seta st010/st011 bios memory init/re-format
  if (biosromSize > 0 && (type == CART_LOROMDSP || type == CART_HIROMDSP || type == CART_LOROMSETA)) {
	  cart->bios = BurnMalloc(0x18000); // dsp 0x2800, Seta st010/st011: 0x11000
	  cart->biosSize = biosromSize;
	  dsp_bios_reform(biosrom, cart->bios, (type == CART_LOROMSETA));
	  upd_ram = (uint8_t*)BurnMalloc(0x2000); // 0x200 dsp, 0x800 seta
  }
  cart->ramSize = ramSize;
  memcpy(cart->rom, rom, romSize);
}

bool cart_handleBattery(Cart* cart, bool save, uint8_t* data, int* size) {
  if(cart->hasBattery == false) return false;
  if(save) {
    *size = cart->ramSize;
    if(data == NULL) return true;
    // assumes data is correct size
    if(cart->ram != NULL) memcpy(data, cart->ram, cart->ramSize);
    return true;
  } else {
    if(*size != cart->ramSize) return false;
    if(cart->ram != NULL) memcpy(cart->ram, data, cart->ramSize);
    return true;
  }
}

static uint8_t cart_readDummy(Cart* cart, uint8_t bank, uint16_t adr) {
  return cart->snes->openBus;
}

static void cart_writeDummy(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val) {
}

#if 0
uint8_t cart_read_switched(Cart* cart, uint8_t bank, uint16_t adr) {
  switch(cart->type) {
    case CART_NONE: return cart->snes->openBus;
    case CART_LOROM: return cart_readLorom(cart, bank, adr);
    case CART_HIROM: return cart_readHirom(cart, bank, adr);
    case CART_EXLOROM: return cart_readExLorom(cart, bank, adr);
    case CART_EXHIROM: return cart_readExHirom(cart, bank, adr);
    case CART_CX4: return cart_readCX4(cart, bank, adr);
    case CART_LOROMDSP: return cart_readLoromDSP(cart, bank, adr);
    case CART_HIROMDSP: return cart_readHiromDSP(cart, bank, adr);
    case CART_LOROMSETA: return cart_readLoromSeta(cart, bank, adr);
    case CART_LOROMSA1: return cart_readLoromSA1(cart, bank, adr);
    case CART_LOROMOBC1: return cart_readLoromOBC1(cart, bank, adr);
    case CART_LOROMSDD1: return cart_readLoromSDD1(cart, bank, adr);
  }
  return cart->snes->openBus;
}

void cart_write_switched(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val) {
  switch(cart->type) {
    case CART_NONE: break;
    case CART_LOROM: cart_writeLorom(cart, bank, adr, val); break;
    case CART_HIROM: cart_writeHirom(cart, bank, adr, val); break;
    case CART_EXLOROM: cart_writeLorom(cart, bank, adr, val); break;
    case CART_EXHIROM: cart_writeHirom(cart, bank, adr, val); break;
    case CART_CX4: cart_writeCX4(cart, bank, adr, val); break;
    case CART_LOROMDSP: cart_writeLoromDSP(cart, bank, adr, val); break;
    case CART_HIROMDSP: cart_writeHiromDSP(cart, bank, adr, val); break;
    case CART_LOROMSETA: cart_writeLoromSeta(cart, bank, adr, val); break;
    case CART_LOROMSA1: cart_writeLoromSA1(cart, bank, adr, val); break;
    case CART_LOROMOBC1: cart_writeLoromOBC1(cart, bank, adr, val); break;
    case CART_LOROMSDD1: cart_writeLoromSDD1(cart, bank, adr, val); break;
  }
}
#endif

static uint8_t cart_readLorom(Cart* cart, uint8_t bank, uint16_t adr) {
  if(((bank >= 0x70 && bank < 0x7e) || bank >= 0xf0) && ((cart->romSize >= 0x200000 && adr < 0x8000) || (cart->romSize < 0x200000)) && cart->ramSize > 0) {
    // banks 70-7d and f0-ff, adr 0000-7fff & rom >= 2MB || adr 0000-ffff & rom < 2MB
    return cart->ram[(((bank & 0xf) << 15) | adr) & (cart->ramSize - 1)];
  }
  bank &= 0x7f;
  if(adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return cart->rom[((bank << 15) | (adr & 0x7fff)) & (cart->romSize - 1)];
  }

  return cart->snes->openBus;
}

static void cart_writeLorom(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val) {
  if(((bank >= 0x70 && bank < 0x7e) || bank > 0xf0) && ((cart->romSize >= 0x200000 && adr < 0x8000) || (cart->romSize < 0x200000)) && cart->ramSize > 0) {
    // banks 70-7d and f0-ff, adr 0000-7fff & rom >= 2MB || adr 0000-ffff & rom < 2MB
    cart->ram[(((bank & 0xf) << 15) | adr) & (cart->ramSize - 1)] = val;
  }
}

static uint8_t cart_readExLorom(Cart* cart, uint8_t bank, uint16_t adr) {
  if(((bank >= 0x70 && bank < 0x7e) || bank >= 0xf0) && adr < 0x8000 && cart->ramSize > 0) {
    // banks 70-7d and f0-ff, adr 0000-7fff
    return cart->ram[(((bank & 0xf) << 15) | adr) & (cart->ramSize - 1)];
  }
  const bool secondHalf = bank < 0x80;
  bank &= 0x7f;
  if(adr >= 0x8000) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return cart->rom[((bank << 15) | (secondHalf ? 0x400000 : 0) | (adr & 0x7fff)) & (cart->romSize - 1)];
  }
  return cart->snes->openBus;
}

static uint8_t cart_readLoromSeta(Cart* cart, uint8_t bank, uint16_t adr) {

	if ((bank & 0x78) == 0x60 && (adr & 0xc000) == 0x0000) { // 60-67,0-3fff
	  cart_run();
	  return snesdsp_read(~adr & 0x01);
	}

	if ((bank & 0x78) == 0x68 && (adr & 0x8000) == 0x0000) { // 68-6f,0-7fff
		cart_run();
		return upd_ram[adr & 0xfff];
	}

  if(((bank >= 0x70 && bank < 0x7e) || bank >= 0xf0) && ((cart->romSize >= 0x200000 && adr < 0x8000) || (cart->romSize < 0x200000)) && cart->ramSize > 0) {
    // banks 70-7d and f0-ff, adr 0000-7fff & rom >= 2MB || adr 0000-ffff & rom < 2MB
    return cart->ram[(((bank & 0xf) << 15) | adr) & (cart->ramSize - 1)];
  }
  bank &= 0x7f;
  if(adr >= 0x8000) {
    // adr 8000-ffff in all other banks
    return cart->rom[((bank << 15) | (adr & 0x7fff)) & (cart->romSize - 1)];
  }

  return cart->snes->openBus;
}

static void cart_writeLoromSeta(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val) {
	if ((bank & 0x78) == 0x60 && (adr & 0xc000) == 0x0000) {
	  cart_run();
	  snesdsp_write(~adr & 0x01, val);
	  return;
	}

	if ((bank & 0x78) == 0x68 && (adr & 0x8000) == 0x0000) {
	  cart_run();
	  upd_ram[adr & 0xfff] = val;
	  return;
	}

  if(((bank >= 0x70 && bank < 0x7e) || bank > 0xf0) && ((cart->romSize >= 0x200000 && adr < 0x8000) || (cart->romSize < 0x200000)) && cart->ramSize > 0) {
    // banks 70-7d and f0-ff, adr 0000-7fff & rom >= 2MB || adr 0000-ffff & rom < 2MB
    cart->ram[(((bank & 0xf) << 15) | adr) & (cart->ramSize - 1)] = val;
  }
}

static uint8_t cart_readLoromDSP(Cart* cart, uint8_t bank, uint16_t adr) {
  if(((bank >= 0x70 && bank < 0x7e) || bank >= 0xf0) && adr < 0x8000 && cart->ramSize > 0) {
    // banks 70-7d and f0-ff, adr 0000-7fff
    return cart->ram[(((bank & 0xf) << 15) | adr) & (cart->ramSize - 1)];
  }
  bank &= 0x7f;
  if ( ((bank & 0x70) == 0x30 && adr & 0x8000) || // 30-3f,b0-bf 8000-ffff
	   ((bank & 0x70) == 0x60 && ~adr & 0x8000) ) { // super bases loaded 60-6f,e0-ef,0-7fff
	  cart_run();
	  return snesdsp_read(!(adr & 0x4000));
  }
  if(adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return cart->rom[((bank << 15) | (adr & 0x7fff)) & (cart->romSize - 1)];
  }
  //bprintf(0, _T("missed.r %x.%x\n"), bank,adr);

  return cart->snes->openBus;
}

static void cart_writeLoromDSP(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val) {
  if(((bank >= 0x70 && bank < 0x7e) || bank > 0xf0) && ((cart->romSize >= 0x200000 && adr < 0x8000) || (cart->romSize < 0x200000)) && cart->ramSize > 0) {
    // banks 70-7d and f0-ff, adr 0000-7fff & rom >= 2MB || adr 0000-ffff & rom < 2MB
    cart->ram[(((bank & 0xf) << 15) | adr) & (cart->ramSize - 1)] = val;
  }

 // bprintf(0, _T("missed.w %x.%x  %x\n"), bank,adr, val);

  if ( ((bank & 0x70) == 0x30 && adr & 0x8000) || // 30-3f,b0-bf 8000-ffff
	   ((bank & 0x70) == 0x60 && ~adr & 0x8000) ) { // super bases loaded 60-6f,e0-ef,0-7fff
	  cart_run();
	  snesdsp_write(!(adr & 0x4000), val);
  }
}

static uint8_t cart_readCX4(Cart* cart, uint8_t bank, uint16_t adr) {
  // cx4 mapper
  if((bank & 0x7f) < 0x40 && adr >= 0x6000 && adr < 0x8000) {
    // banks 00-3f and 80-bf, adr 6000-7fff
	return cx4_read(adr);
  }
  // save ram
  if(((bank >= 0x70 && bank < 0x7e) || bank >= 0xf0) && adr < 0x8000 && cart->ramSize > 0) {
    // banks 70-7d and f0-ff, adr 0000-7fff
    return cart->ram[(((bank & 0xf) << 15) | adr) & (cart->ramSize - 1)];
  }
  bank &= 0x7f;
  if(adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return cart->rom[((bank << 15) | (adr & 0x7fff)) & (cart->romSize - 1)];
  }
  return cart->snes->openBus;
}

static void cart_writeCX4(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val) {
  // cx4 mapper
  if((bank & 0x7f) < 0x40 && adr >= 0x6000 && adr < 0x8000) {
    // banks 00-3f and 80-bf, adr 6000-7fff
	cx4_write(adr, val);
  }
  // save ram
  if(((bank >= 0x70 && bank < 0x7e) || bank > 0xf0) && adr < 0x8000 && cart->ramSize > 0) {
    // banks 70-7d and f0-ff, adr 0000-7fff
    cart->ram[(((bank & 0xf) << 15) | adr) & (cart->ramSize - 1)] = val;
  }
}

static uint8_t cart_readHirom(Cart* cart, uint8_t bank, uint16_t adr) {
  bank &= 0x7f;
  if(bank >= 0x20 && bank < 0x40 && adr >= 0x6000 && adr < 0x8000 && cart->ramSize > 0) {
    // banks 00-3f and 80-bf, adr 6000-7fff
    return cart->ram[(((bank & 0x1f) << 13) | (adr & 0x1fff)) & (cart->ramSize - 1)];
  }
  if(adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return cart->rom[(((bank & 0x3f) << 16) | adr) & (cart->romSize - 1)];
  }
  return cart->snes->openBus;
}

static void cart_writeHirom(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val) {
  bank &= 0x7f;
  if(bank >= 0x20 && bank < 0x40 && adr >= 0x6000 && adr < 0x8000 && cart->ramSize > 0) {
    // banks 00-3f and 80-bf, adr 6000-7fff
    cart->ram[(((bank & 0x1f) << 13) | (adr & 0x1fff)) & (cart->ramSize - 1)] = val;
  }
}

// HiROM w/DSP
static uint8_t cart_readHiromDSP(Cart* cart, uint8_t bank, uint16_t adr) {
  bank &= 0x7f;

  if (bank < 0x20 && adr >= 0x6000 && adr < 0x8000) {
	  cart_run();
	  return snesdsp_read(!(adr & 0x1000));
  }

  if(bank >= 0x20 && bank < 0x40 && adr >= 0x6000 && adr < 0x8000 && cart->ramSize > 0) {
    // banks 00-3f and 80-bf, adr 6000-7fff
    return cart->ram[(((bank & 0x1f) << 13) | (adr & 0x1fff)) & (cart->ramSize - 1)];
  }
  if(adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return cart->rom[(((bank & 0x3f) << 16) | adr) & (cart->romSize - 1)];
  }
 // bprintf(0, _T("missed.r %x.%x\n"), bank,adr);
  return cart->snes->openBus;
}

static void cart_writeHiromDSP(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val) {
  bank &= 0x7f;
  if (bank < 0x20 && adr >= 0x6000 && adr < 0x8000) {
	  cart_run();
	  snesdsp_write(!(adr & 0x1000), val);
	  return;
  }

 // bprintf(0, _T("missed.w %x.%x  %x\n"), bank,adr, val);
  if(bank >= 0x20 && bank < 0x40 && adr >= 0x6000 && adr < 0x8000 && cart->ramSize > 0) {
    // banks 00-3f and 80-bf, adr 6000-7fff
    cart->ram[(((bank & 0x1f) << 13) | (adr & 0x1fff)) & (cart->ramSize - 1)] = val;
  }
}

static uint8_t cart_readExHirom(Cart* cart, uint8_t bank, uint16_t adr) {
  if((bank & 0x7f) < 0x40 && adr >= 0x6000 && adr < 0x8000 && cart->ramSize > 0) {
    // banks 00-3f and 80-bf, adr 6000-7fff
    return cart->ram[(((bank & 0x3f) << 13) | (adr & 0x1fff)) & (cart->ramSize - 1)];
  }
  bool secondHalf = bank < 0x80;
  bank &= 0x7f;
  if(adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return cart->rom[(((bank & 0x3f) << 16) | (secondHalf ? 0x400000 : 0) | adr) & (cart->romSize - 1)];
  }
  return cart->snes->openBus;
}

static uint8_t cart_readLoromSA1(Cart* cart, uint8_t bank, uint16_t adr) {
  return snes_sa1_cart_read(bank << 16 | adr);
}

static void cart_writeLoromSA1(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val) {
  snes_sa1_cart_write(bank << 16 | adr, val);
}

#define obc1_offset (0x1800 | (~cart->ram[0x1ff5] & 0x01) << 10)
#define obc1_offset2 ((cart->ram[0x1ff6] & 0x7f) << 2)

static uint8_t cart_readLoromOBC1(Cart* cart, uint8_t bank, uint16_t adr) {
  if(((bank >= 0x70 && bank < 0x7e) || bank >= 0xf0) && ((cart->romSize >= 0x200000 && adr < 0x8000) || (cart->romSize < 0x200000)) && cart->ramSize > 0) {
    // banks 70-7d and f0-ff, adr 0000-7fff & rom >= 2MB || adr 0000-ffff & rom < 2MB
    return cart->ram[(((bank & 0xf) << 15) | adr) & (cart->ramSize - 1)];
  }
  bank &= 0x7f;
  if(adr >= 0x8000 || bank >= 0x40) {
    // adr 8000-ffff in all banks or all addresses in banks 40-7f and c0-ff
    return cart->rom[((bank << 15) | (adr & 0x7fff)) & (cart->romSize - 1)];
  }

  if(bank < 0x40 && adr >= 0x6fff && adr <= 0x7fff) { // 00-3f,80-bf,6fff-7fff
	  adr &= 0x1fff;
	  switch (adr) {
		  case 0x1ff0:
		  case 0x1ff1:
		  case 0x1ff2:
		  case 0x1ff3:
			  return cart->ram[obc1_offset + obc1_offset2 + (adr & 0x03)];
		  case 0x1ff4:
			  return cart->ram[obc1_offset + (obc1_offset2 >> 4) + 0x200];
		  default:
			  return cart->ram[adr];
	  }
  }

  return cart->snes->openBus;
}

static void cart_writeLoromOBC1(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val) {
  if(((bank >= 0x70 && bank < 0x7e) || bank > 0xf0) && ((cart->romSize >= 0x200000 && adr < 0x8000) || (cart->romSize < 0x200000)) && cart->ramSize > 0) {
    // banks 70-7d and f0-ff, adr 0000-7fff & rom >= 2MB || adr 0000-ffff & rom < 2MB
    cart->ram[(((bank & 0xf) << 15) | adr) & (cart->ramSize - 1)] = val;
  }

  bank &= 0x7f;

  if(bank < 0x40 && adr >= 0x6fff && adr <= 0x7fff) { // 00-3f,80-bf,6fff-7fff
	  adr &= 0x1fff;
	  switch (adr) {
		  case 0x1ff0:
		  case 0x1ff1:
		  case 0x1ff2:
		  case 0x1ff3:
			  cart->ram[obc1_offset + obc1_offset2 + (adr & 0x03)] = val;
			  break;
		  case 0x1ff4:
			  cart->ram[obc1_offset + (obc1_offset2 >> 4) + 0x200] = (cart->ram[obc1_offset + (obc1_offset2 >> 4) + 0x200] & ~(3 << ((cart->ram[0x1ff6] & 0x03) << 1))) | ((val & 0x03) << ((cart->ram[0x1ff6] & 0x03) << 1));
			  break;
		  default:
			  cart->ram[adr] = val;
			  break;
	  }
  }
}

static uint8_t cart_readLoromSDD1(Cart* cart, uint8_t bank, uint16_t adr) {
  return snes_sdd1_cart_read(bank << 16 | adr);
}

static void cart_writeLoromSDD1(Cart* cart, uint8_t bank, uint16_t adr, uint8_t val) {
  snes_sdd1_cart_write(bank << 16 | adr, val);
}
