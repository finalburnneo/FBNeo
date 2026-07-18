#include "t11.h"

void t11Init(INT32 mode, INT32 (*irqcallback)(INT32));
void t11Reset();
void t11Open(INT32);
void t11Close();
INT32 t11Run(INT32 nCycles);
void t11SetIRQLine(INT32 line, INT32 state);
void t11Exit();
int t11Scan(int nAction);

INT32 t11Idle(INT32 cycles);
void t11RunEnd();
INT32 t11TotalCycles();
void t11NewFrame();
INT32 t11GetActive();

void t11MapMemory(UINT8 *src, UINT16 start, UINT16 finish, INT32 type);

void t11SetWriteByteHandler(void (*write)(UINT16, UINT8));
void t11SetWriteWordHandler(void (*write)(UINT16, UINT16));
void t11SetReadByteHandler(UINT8 (*read)(UINT16));
void t11SetReadWordHandler(UINT16 (*read)(UINT16));

extern struct cpu_core_config t11Config;

#define BurnTimerAttacht11(clock)	\
	BurnTimerAttach(&t11Config, clock)

void t11WriteByte(UINT16 addr, UINT8 data);
void t11WriteWord(UINT16 addr, UINT16 data);
UINT8 t11ReadByte(UINT16 addr);
UINT16 t11ReadWord(UINT16 addr);
UINT16 t11FetchWord(UINT16 addr);
void t11_write_rom_byte(UINT32 addr, UINT8 data);
