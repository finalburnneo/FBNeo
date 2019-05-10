#define MAX_S2650	4
extern INT32 nActiveS2650;
extern INT32 s2650Count;

void s2650Write(UINT16 address, UINT8 data);
UINT8 s2650Read(UINT16 address);
UINT8 s2650Fetch(UINT16 address);
void s2650WritePort(UINT16 port, UINT8 data);
UINT8 s2650ReadPort(UINT16 port);

typedef INT32 (*s2650irqcallback)(INT32);
extern s2650irqcallback s2650_irqcallback[MAX_S2650];
void s2650SetIrqCallback(INT32 (*irqcallback)(INT32));

void s2650MapMemory(UINT8 *src, INT32 start, INT32 end, INT32 type);

void s2650SetIRQLine(INT32 irqline, INT32 state);

void s2650SetWriteHandler(void (*write)(UINT16, UINT8));
void s2650SetReadHandler(UINT8 (*read)(UINT16));
void s2650SetOutHandler(void (*write)(UINT16, UINT8));
void s2650SetInHandler(UINT8 (*read)(UINT16));

INT32 s2650Run(INT32 cycles);
void s2650Reset();
void s2650Open(INT32 num);
void s2650Close();
void s2650Exit();
void s2650Init(INT32 num);

UINT32 s2650GetPC(INT32);


INT32 s2650Scan(INT32 nAction);

#define S2650_CTRL_PORT 0x100
#define S2650_DATA_PORT 0x101
#define S2650_EXT_PORT	0xff
#define S2650_SENSE_PORT 0x102

INT32 s2650GetActive();
INT32 s2650TotalCycles();
void s2650NewFrame();
void s2650RunEnd();
INT32 s2650Idle(INT32 cycles);

void s2650WriteROM(UINT32 address, UINT8 data); // cheat core
UINT8 s2650ReadCheat(UINT32 a);

extern struct cpu_core_config s2650Config;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachS2650(clock)	\
	BurnTimerAttach(&s2650Config, clock)
