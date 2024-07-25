#ifndef ARM946ES_INTF_H
#define ARM946ES_INTF_H

#define ARM946ES_IRQ_LINE		0
#define ARM946ES_FIRQ_LINE		1

typedef void(__fastcall* pArm946esWriteLongHandler)(UINT32 a, UINT32 d, UINT32 m);
typedef void(__fastcall* pArm946esWriteWordHandler)(UINT32 a, UINT16 d, UINT32 m);
typedef void(__fastcall* pArm946esWriteByteHandler)(UINT32 a, UINT8 d, UINT32 m);

typedef UINT16(__fastcall* pArm946esReadWordHandler)(UINT32 a);
typedef UINT32(__fastcall* pArm946esReadLongHandler)(UINT32 a);
typedef UINT8(__fastcall* pArm946esReadByteHandler)(UINT32 a);

void Arm946esSetWriteByteHandler(INT32 i, pArm946esWriteByteHandler handle);
void Arm946esSetWriteWordHandler(INT32 i, pArm946esWriteWordHandler handle);
void Arm946esSetWriteLongHandler(INT32 i, pArm946esWriteLongHandler handle);
void Arm946esSetReadByteHandler(INT32 i, pArm946esReadByteHandler handle);
void Arm946esSetReadWordHandler(INT32 i, pArm946esReadWordHandler handle);
void Arm946esSetReadLongHandler(INT32 i, pArm946esReadLongHandler handle);

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
void Arm946esMapHandler(uintptr_t nHandler, UINT32 start, UINT32 finish, INT32 type);

// speed hack function
void Arm946esSetIdleLoopAddress(UINT32 address);


#endif
