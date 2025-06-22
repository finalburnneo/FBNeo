
#ifndef SNES_H
#define SNES_H

#include "burnint.h"

typedef struct Snes Snes;

#include "cpu.h"
#include "apu.h"
#include "dma.h"
#include "ppu.h"
#include "cart.h"
#include "input.h"
#include "statehandler.h"

struct Snes {
  Cpu* cpu;
  Apu* apu;
  Dma* dma;
  Cart* cart;
  bool palTiming;
  // input
  Input* input1;
  Input* input2;
  // ram
  uint8_t ram[0x20000];
  uint32_t ramAdr;
  uint8_t ramFill;
  // frame timing
  int16_t hPos;
  uint16_t vPos;
  uint32_t frames;
  uint64_t cycles;
  uint64_t syncCycle;
  uint32_t nextHoriEvent;
  // cpu handling
  // nmi / irq
  bool hIrqEnabled;
  bool vIrqEnabled;
  bool nmiEnabled;
  uint16_t hTimer;
  uint16_t vTimer;
  uint32_t hvTimer;
  bool inNmi;
  bool irqCondition;
  bool inIrq;
  bool inVblank;
  bool inRefresh;
  // joypad handling
  uint16_t portAutoRead[4]; // as read by auto-joypad read
  bool autoJoyRead;
  uint64_t autoJoyTimer; // times how long until reading is done
  bool ppuLatch;
  // multiplication/division
  uint8_t multiplyA;
  uint16_t multiplyResult;
  uint16_t divideA;
  uint16_t divideResult;
  // misc
  bool fastMem;
  uint32_t adrBus;
  uint8_t openBus;
  uint8_t vramhack; // 1. allow vram writes during rendering, 2. oam hack for uniracers
};

Snes* snes_init(void);
void snes_free(Snes* snes);
void snes_reset(Snes* snes, bool hard);
void snes_handleState(Snes* snes, StateHandler* sh);
void snes_runFrame(Snes* snes);
// used by dma, cpu
void snes_runCycles(Snes* snes, int cycles);
void snes_runCycles4(Snes* snes);
void snes_runCycles6(Snes* snes);
void snes_runCycles8(Snes* snes);
void snes_syncCycles(Snes* snes, bool start, int syncCycles);
uint8_t snes_readBBus(Snes* snes, uint8_t adr);
void snes_writeBBus(Snes* snes, uint8_t adr, uint8_t val);
uint8_t snes_read(Snes* snes, uint32_t adr);
void snes_write(Snes* snes, uint32_t adr, uint8_t val);
void snes_cpuIdle(void* mem, bool waiting);
uint8_t snes_cpuRead(void* mem, uint32_t adr);
void snes_cpuWrite(void* mem, uint32_t adr, uint8_t val);
int snes_verticalLinecount(Snes* snes);
// debugging
void snes_runCpuCycle(Snes* snes);
void snes_runSpcCycle(Snes* snes);

// snes_other.c functions:

bool snes_loadRom(Snes* snes, const uint8_t* data, int length, uint8_t* biosdata, int bioslength);
void snes_setButtonState(Snes* snes, int player, int button, int pressed, int device);
void snes_setMouseState(Snes* snes, int player, int16_t x, int16_t y, uint8_t buttonA, uint8_t buttonB);
void snes_setGunState(Snes* snes, int x1, int y1, int x2, int y2);
void snes_setPixels(Snes* snes, uint8_t* pixelData, int height);
void snes_setSamples(Snes* snes, int16_t* sampleData, int samplesPerFrame);
int snes_saveBattery(Snes* snes, uint8_t* data);
bool snes_loadBattery(Snes* snes, uint8_t* data, int size);
int snes_saveState(Snes* snes, uint8_t* data);
bool snes_loadState(Snes* snes, uint8_t* data, int size);
bool snes_isPal(Snes* snes);

#endif
