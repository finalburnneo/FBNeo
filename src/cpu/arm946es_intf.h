#ifndef ARM946ES_INTF_H
#define ARM946ES_INTF_H

#define ARM946ES_IRQ_LINE		0
#define ARM946ES_FIRQ_LINE		1


void Arm946esWriteByte(UINT32 addr, UINT8 data);
void Arm946esWriteWord(UINT32 addr, UINT16 data);
void Arm946esWriteLong(UINT32 addr, UINT32 data);
UINT8  Arm946esReadByte(UINT32 addr);
UINT16 Arm946esReadWord(UINT32 addr);
UINT32 Arm946esReadLong(UINT32 addr);
UINT16 Arm946esFetchWord(UINT32 addr);
UINT32 Arm946esFetchLong(UINT32 addr);

void Arm946esRunEnd();
void Arm7RunEndEatCycles();
void Arm946esBurnCycles(INT32 cycles);
INT32 Arm946esIdle(int cycles);
INT32 Arm946esTotalCycles();
void Arm946esNewFrame();

void Arm946esInit(INT32 nCPU);
void Arm946esReset();
INT32 Arm946esRun(INT32 cycles);
void Arm946esExit();
void Arm946esOpen(INT32);
void Arm946esClose();

INT32 Arm946esScan(INT32 nAction);

void Arm946esSetIRQLine(INT32 line, INT32 state);

void Arm946esMapMemory(UINT8* src, UINT32 start, UINT32 finish, INT32 type);

void Arm946esSetWriteByteHandler(void (*write)(UINT32, UINT8));
void Arm946esSetWriteWordHandler(void (*write)(UINT32, UINT16));
void Arm946esSetWriteLongHandler(void (*write)(UINT32, UINT32));
void Arm946esSetReadByteHandler(UINT8 (*read)(UINT32));
void Arm946esSetReadWordHandler(UINT16 (*read)(UINT32));
void Arm946esSetReadLongHandler(UINT32 (*read)(UINT32));

// speed hack function
void Arm946esSetIdleLoopAddress(UINT32 address);


#endif
