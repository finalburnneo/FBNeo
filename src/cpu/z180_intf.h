#include "z180.h"

void Z180SetWriteHandler(void (__fastcall *write)(UINT32, UINT8));
void Z180SetReadHandler(UINT8 (__fastcall *read)(UINT32));
void Z180SetFetchOpHandler(UINT8 (__fastcall *fetch)(UINT32));
void Z180SetFetchArgHandler(UINT8 (__fastcall *fetch)(UINT32));
void Z180SetWritePortHandler(void (__fastcall *write)(UINT32, UINT8));
void Z180SetReadPortHandler(UINT8 (__fastcall *read)(UINT32));

// pass NULL for ptr to unmap memory
void Z180MapMemory(UINT8 *ptr, UINT32 start, UINT32 end, UINT32 flags);

void Z180Init(UINT32 nCPU);
void Z180Exit();
void Z180Reset();
INT32 Z180Run(INT32 cycles);
void Z180BurnCycles(INT32 cycles);
void Z180SetIRQLine(INT32 irqline, INT32 state);
void Z180Scan(INT32 nAction);

INT32 Z180GetActive();
void Z180Open(INT32);
void Z180Close();

INT32 Z180TotalCycles();
void Z180NewFrame();
void Z180RunEnd();

void z180_cheat_write(UINT32 address, UINT8 data); // cheat core
UINT8 z180_cheat_read(UINT32 address);

extern struct cpu_core_config Z180Config;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachZ180(clock)	\
	BurnTimerAttach(&Z180Config, clock)
