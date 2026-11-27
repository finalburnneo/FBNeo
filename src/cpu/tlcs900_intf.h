#include "tlcs900.h"

void tlcs900Init(INT32);
void tlcs900Open(INT32);
void tlcs900MapMemory(UINT8 *ptr, UINT32 start, UINT32 end, UINT32 flags);
void tlcs900SetWriteHandler(void (*write)(UINT32, UINT8));
void tlcs900SetReadHandler(UINT8 (*read)(UINT32));
void tlcs900SetToxHandler(INT32 select, void (*tox)(UINT32, UINT8));
// TMP95C061 I/O pins (port 1..9, 10 = A, 11 = B) and the A/D inputs (10 bits)
void tlcs900SetPortHandlers(UINT8 (*read)(INT32 port), void (*write)(INT32 port, UINT8 data));
void tlcs900SetAnalogHandler(UINT16 (*read)(INT32 channel));
void tlcs900Reset();
INT32 tlcs900Run(INT32);
UINT32 tlcs900GetPC(INT32);
void tlcs900SetPC(UINT32 address);
void tlcs900SetIRQLine(INT32 line, INT32 state);
INT32 tlcs900TotalCycles();
void tlcs900NewFrame();
void tlcs900RunEnd();
INT32 tlcs900GetActive();
INT32 tlcs900Idle(INT32 nCycles);
void tlcs900Close();
void tlcs900Exit();
INT32 tlcs900Scan(INT32 nAction);

extern struct cpu_core_config tlcs900Config;
