
void tlcs90SetReadHandler(UINT8 (*pread)(UINT32));
void tlcs90SetWriteHandler(void (*pwrite)(UINT32, UINT8));
void tlcs90SetReadPortHandler(UINT8 (*pread)(UINT16));
void tlcs90SetWritePortHandler(void (*pwrite)(UINT16, UINT8));

INT32 tlcs90Init(INT32 nCpu, INT32 clock);
void tlcs90Open(INT32 nCpu);
INT32 tlcs90Run(INT32 nCycles);
void tlcs90Reset();
void tlcs90Close();
INT32 tlcs90Exit();

void tlcs90SetIRQLine(INT32 line, INT32 state);

void tlcs90BurnCycles(INT32 nCpu, INT32 cycles);

void tlcs90MapMemory(UINT8 *rom, UINT32 start, UINT32 end, INT32 flags);

void tlcs90NewFrame();
void tlcs90RunEnd();
INT32 tlcs90TotalCycles();
INT32 tlcs90GetActive();

INT32 tlcs90Scan(INT32 nAction);

void tlcs90WriteROM(UINT32 address, UINT8 data); // cheat core
UINT8 tlcs90CheatRead(UINT32 address);

extern struct cpu_core_config tlcs90Config;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachTlcs90(clock) BurnTimerAttach(&tlcs90Config, clock)
