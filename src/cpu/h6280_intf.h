
void h6280WritePort(UINT8 port, UINT8 data);
void h6280Write(UINT32 address, UINT8 data);
UINT8 h6280Read(UINT32 address);
UINT8 h6280Fetch(UINT32 address);

void h6280MapMemory(UINT8 *src, UINT32 start, UINT32 finish, INT32 type);

void h6280SetIrqCallbackHandler(INT32 (*callback)(INT32));
void h6280SetlinesCallback(void  (*setlines_callback)(INT32 lines));

void h6280SetWritePortHandler(void (*write)(UINT8, UINT8));
void h6280SetWriteHandler(void (*write)(UINT32, UINT8));
void h6280SetReadHandler(UINT8 (*read)(UINT32));

void h6280Init(INT32 nCPU);
void h6280Open(INT32);
void h6280Reset();
INT32 h6280Run(INT32 cycles);
void h6280Close();
void h6280Exit();

extern INT32 nh6280CpuCount;

#define H6280_INPUT_LINE_NMI	0x20

void h6280SetIRQLine(INT32 line, INT32 state);

void h6280SetVDCPenalty(INT32 enabled);

INT32 h6280Scan(INT32 nAction);

INT32 h6280TotalCycles();
void h6280NewFrame();
void h6280RunEnd();
INT32 h6280Idle(INT32 cycles);

INT32 h6280GetActive();

UINT8 h6280_irq_status_r(UINT32 offset);
void h6280_irq_status_w(UINT32 offset, UINT8 data);
void h6280_irqcallback(int (*irqcallback)(int));

UINT8 h6280_timer_r(UINT32);
void h6280_timer_w(UINT32 offset, UINT8 data);

// functions for use by the PSG and joypad port only!
UINT8 h6280io_get_buffer(void);
void h6280io_set_buffer(UINT8);

void h6280_write_rom(UINT32 address, UINT8 data); // cheat core

extern struct cpu_core_config H6280Config;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachH6280(clock)	\
	BurnTimerAttach(&H6280Config, clock)
