
#ifndef CPU_SA1_H
#define CPU_SA1_H

#include <stdint.h>
#include <stdbool.h>

#include "statehandler.h"

#if 0
typedef uint8_t (*CpuReadHandler)(void* mem, uint32_t adr);
typedef void (*CpuWriteHandler)(void* mem, uint32_t adr, uint8_t val);
typedef void (*CpuIdleHandler)(void* mem, bool waiting);

void cpusa1_init(void* mem, CpuReadHandler read, CpuWriteHandler write, CpuIdleHandler idle);
#endif
void cpusa1_init();
void cpusa1_free();
void cpusa1_reset(bool hard);
void cpusa1_handleState(StateHandler* sh);
void cpusa1_runOpcode();
void cpusa1_nmi();
void cpusa1_setIrq(bool state);
void cpusa1_setIntDelay();
void cpusa1_setHalt(bool haltValue);
uint32_t cpusa1_getPC();
bool cpusa1_isVector();

#endif
