// Fujitsu MB86233 (TGP) interface

#ifndef MB86233_INTF_H
#define MB86233_INTF_H

void mb86233Init();
void mb86233Exit();
void mb86233Reset();
void mb86233Open(INT32 nCpu);
void mb86233Close();
INT32 mb86233GetActive();

INT32 mb86233Run(INT32 nCycles);
void mb86233RunEnd();
INT32 mb86233Idle(INT32 nCycles);
INT32 mb86233TotalCycles();
void mb86233NewFrame();
INT32 mb86233Scan(INT32 nAction);

// program: 32-bit words in host endianness, nMask is the word index mask
void mb86233SetProgram(UINT32 *pRom, UINT32 nMask);

// external data space (everything outside the internal RAM at 0x000-0x0ff / 0x200-0x3ff)
void mb86233SetDataHandlers(UINT32 (*read)(UINT32), void (*write)(UINT32, UINT32));
void mb86233SetIOHandlers(UINT32 (*read)(UINT32), void (*write)(UINT32, UINT32));
void mb86233SetRFHandlers(UINT32 (*read)(UINT32), void (*write)(UINT32, UINT32));

// direct access to the internal RAM (0x000-0x3ff, dword addressed)
UINT32 mb86233ReadRAM(UINT32 nAddress);
void mb86233WriteRAM(UINT32 nAddress, UINT32 nData);

// called from a handler when an access can't complete (empty/full fifo): the
// instruction is retried once the core runs again
void mb86233Stall();

// a halted core only burns cycles
void mb86233SetHalt(INT32 nState);
INT32 mb86233GetHalt();

void mb86233SetGPIO(INT32 nLine, INT32 nState);
UINT32 mb86233GetPC();

extern struct cpu_core_config mb86233Config;

#endif
