
#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>

#include "statehandler.h"

#if 0
typedef uint8_t (*CpuReadHandler)(void* mem, uint32_t adr);
typedef void (*CpuWriteHandler)(void* mem, uint32_t adr, uint8_t val);
typedef void (*CpuIdleHandler)(void* mem, bool waiting);
#endif

typedef struct Cpu Cpu;

struct Cpu {
};

void cpu_init(void* mem);
void cpu_free();
void cpu_reset(bool hard);
void cpu_handleState(StateHandler* sh);
void cpu_runOpcode();
void cpu_nmi();
void cpu_setIrq(bool state);
void cpu_setIntDelay();
bool cpu_isVector();
uint32_t cpu_getPC();

#endif
