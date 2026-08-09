// Philips XA CPU Interface
INT32 xaInit(INT32 nCpu, INT32 clock);
void xaOpen(INT32 nCpu);
void xaClose();
void xaExit();

void xaReset();
INT32 xaRun(INT32 cycles);
void xaRunEnd();
void xaNewFrame();
INT32 xaTotalCycles();
INT32 xaIdle(INT32 cycles);
INT32 xaGetActive();

INT32 xaScan(INT32 nAction);

void xaSetIRQLine(INT32 line, INT32 state);

void xaMapMemory(UINT8 *ptr, UINT32 start, UINT32 end, INT32 flags);		// program space (24-bit)
void xaMapDataMemory(UINT8 *ptr, UINT32 start, UINT32 end, INT32 flags);	// data space (24-bit)

void xaSetProgramReadHandler(UINT8 (*pread)(UINT32));
void xaSetProgramWriteHandler(void (*pwrite)(UINT32, UINT8));

void xaSetDataReadHandler(UINT8 (*pread)(UINT32));
void xaSetDataWriteHandler(void (*pwrite)(UINT32, UINT8));

// SFR "ports" (P0-P3)
void xaSetPortReadHandler(UINT8 (*pread)(UINT32));
void xaSetPortWriteHandler(void (*pwrite)(UINT32, UINT8));

void xaWriteROM(UINT32 address, UINT8 data); // cheat core
UINT8 xaCheatRead(UINT32 address);

INT32 xaGetPC(INT32 n);

extern struct cpu_core_config xaConfig;

#define BurnTimerAttachXa(clock) BurnTimerAttach(&xaConfig, clock)
