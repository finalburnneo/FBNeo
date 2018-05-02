void konamiWrite(UINT16 address, UINT8 data);
UINT8 konamiRead(UINT16 address);
UINT8 konamiFetch(UINT16 address);

void konamiMapMemory(UINT8 *src, UINT16 start, UINT16 finish, INT32 type);

void konamiSetIrqCallbackHandler(INT32 (*callback)(INT32));
void konamiSetlinesCallback(void  (*setlines_callback)(INT32 lines));

void konamiSetWriteHandler(void (*write)(UINT16, UINT8));
void konamiSetReadHandler(UINT8 (*read)(UINT16));

void konamiInit(INT32 nCpu);
void konamiOpen(INT32 );
void konamiReset();
INT32 konamiRun(INT32 cycles);
void konamiClose();
void konamiExit();

extern INT32 nKonamiCpuCount;

#define KONAMI_IRQ_LINE		0
#define KONAMI_FIRQ_LINE	1
#define KONAMI_INPUT_LINE_NMI	0x20

void konamiSetIrqLine(INT32 line, INT32 state);

INT32 konamiCpuScan(INT32 nAction);

INT32 konamiTotalCycles();
void konamiNewFrame();
void konamiRunEnd(); // not hooked up
INT32 konamiGetActive();

UINT8 konami_cheat_read(UINT32 a);					// cheat
void konami_write_rom(UINT32 address, UINT8 data);

extern cpu_core_config konamiCPUConfig;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachKonami(clock)	\
	BurnTimerAttach(&KonamiCPUConfig, clock)
