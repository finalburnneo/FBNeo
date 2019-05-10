#include "v60.h"

void v60Init();
void v70Init();

void v60Exit();
void v60Reset();
void v60Open(int cpu);
void v60Close();

INT32 v60Run(int cycles);

void v60SetIRQLine(INT32 irqline, INT32 state);

void v60SetIRQCallback(int (*callback)(int irqline));
void v60SetWriteByteHandler(void (*write)(UINT32,UINT8));
void v60SetWriteWordHandler(void (*write)(UINT32,UINT16));
void v60SetReadByteHandler(UINT8  (*read)(UINT32));
void v60SetReadWordHandler(UINT16 (*read)(UINT32));
void v60MapMemory(UINT8 *ptr, UINT32 start, UINT32 end, UINT32 flags);

INT32 v60TotalCycles();
void v60RunEnd();
void v60NewFrame();
INT32 v60GetActive();
INT32 v60Idle(INT32 cycles);

INT32 v60Scan(INT32 nAction);

void v60WriteROM(UINT32 a, UINT8 d);
UINT8 v60CheatRead(UINT32 a);

extern struct cpu_core_config v60Config;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachV60(clock)	\
	BurnTimerAttach(&v60Config, clock)
