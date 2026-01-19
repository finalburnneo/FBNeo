#include "i8085.h"

void i8085Init();
void i8080Init();
void i8085SetIRQCallback(INT32 (*irqcallback)(INT32));
void i8085SetCallback(INT32 (*callback)(INT32)); // for cycles
void i8085MapMemory(UINT8 *ptr, UINT16 start, UINT16 end, UINT8 flags);
void i8085SetWriteHandler(void (*write)(UINT16, UINT8));
void i8085SetReadHandler(UINT8 (*read)(UINT16));
void i8085SetOutHandler(void (*write)(UINT16, UINT8));
void i8085SetInHandler(UINT8 (*read)(UINT16));
void i8085Open(INT32);
void i8085Reset();
INT32 i8085GetActive();
INT32 i8085TotalCycles();
void i8085NewFrame();
INT32 i8085Idle(INT32 cycles);
void i8085RunEnd();
INT32 i8085Run(INT32 cycles);
void i8085SetIRQLine(INT32 line, INT32 state);
void i8085Close();
INT32 i8085Scan(INT32 nAction);
void i8085Exit();

UINT8 i8085ReadByte(UINT16 address);
void i8085WriteByte(UINT16 address, UINT8 data);

extern struct cpu_core_config i8085Config;

#define i8080SetIRQCallback i8085SetIRQCallback
#define i8080SetCallback i8085SetCallback
#define i8080MapMemory i8085MapMemory
#define i8080SetWriteHandler i8085SetWriteHandler
#define i8080SetReadHandler i8085SetReadHandler
#define i8080SetOutHandler i8085SetOutHandler
#define i8080SetInHandler i8085SetInHandler
#define i8080Open i8085Open
#define i8080Reset i8085Reset
#define i8080GetActive i8085GetActive
#define i8080TotalCycles i8085TotalCycles
#define i8080NewFrame i8085NewFrame
#define i8080Idle i8085Idle
#define i8080RunEnd i8085RunEnd
#define i8080Run i8085Run
#define i8080SetIRQLine i8085SetIRQLine
#define i8080Close i8085Close
#define i8080Scan i8085Scan
#define i8080Exit i8085Exit

#define i8080ReadByte i8085ReadByte
#define i8080WriteByte i8085WriteByte

extern struct cpu_core_config i8080Config;
