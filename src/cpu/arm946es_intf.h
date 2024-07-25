#ifndef ARM9_INTF_H
#define ARM9_INTF_H

#define ARM9_IRQ_LINE		0
#define ARM9_FIRQ_LINE		1


void Arm9WriteByte(UINT32 addr, UINT8 data);
void Arm9WriteWord(UINT32 addr, UINT16 data);
void Arm9WriteLong(UINT32 addr, UINT32 data);
UINT8  Arm9ReadByte(UINT32 addr);
UINT16 Arm9ReadWord(UINT32 addr);
UINT32 Arm9ReadLong(UINT32 addr);
UINT16 Arm9FetchWord(UINT32 addr);
UINT32 Arm9FetchLong(UINT32 addr);

void Arm9RunEnd();
void Arm7RunEndEatCycles();
void Arm9BurnCycles(INT32 cycles);
INT32 Arm9Idle(int cycles);
INT32 Arm9TotalCycles();
void Arm9NewFrame();

void Arm9Init(INT32 nCPU);
void Arm9Reset();
INT32 Arm9Run(INT32 cycles);
void Arm9Exit();
void Arm9Open(INT32);
void Arm9Close();

INT32 Arm9Scan(INT32 nAction);

void Arm9SetIRQLine(INT32 line, INT32 state);

void Arm9MapMemory(UINT8* src, UINT32 start, UINT32 finish, INT32 type);

void Arm9SetWriteByteHandler(void (*write)(UINT32, UINT8));
void Arm9SetWriteWordHandler(void (*write)(UINT32, UINT16));
void Arm9SetWriteLongHandler(void (*write)(UINT32, UINT32));
void Arm9SetReadByteHandler(UINT8 (*read)(UINT32));
void Arm9SetReadWordHandler(UINT16 (*read)(UINT32));
void Arm9SetReadLongHandler(UINT32 (*read)(UINT32));

// speed hack function
void Arm9SetIdleLoopAddress(UINT32 address);

void Arm946esInit(INT32 nCPU);
void Arm946esExit();
int Arm946esScan(int nAction);


#endif
