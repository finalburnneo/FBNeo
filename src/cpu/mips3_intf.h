#ifndef MIPS3_INTF
#define MIPS3_INTF

#include "mips3/mips3_common.h"

typedef UINT8 (*pMips3ReadByteHandler)(UINT32 a);
typedef void (*pMips3WriteByteHandler)(UINT32 a, UINT8 d);

typedef UINT16 (*pMips3ReadHalfHandler)(UINT32 a);
typedef void (*pMips3WriteHalfHandler)(UINT32 a, UINT16 d);

typedef UINT32 (*pMips3ReadWordHandler)(UINT32 a);
typedef void (*pMips3WriteWordHandler)(UINT32 a, UINT32 d);

typedef UINT64 (*pMips3ReadDoubleHandler)(UINT32 a);
typedef void (*pMips3WriteDoubleHandler)(UINT32 a, UINT64 d);

int Mips3Init();
int Mips3UseRecompiler(bool use);
int Mips3Exit();
void Mips3Reset();
int Mips3Run(int cycles);
unsigned int Mips3GetPC();

int Mips3MapMemory(unsigned char* pMemory, unsigned int nStart, unsigned int nEnd, int nType);
int Mips3MapHandler(uintptr_t nHandler, unsigned int nStart, unsigned int nEnd, int nType);

int Mips3SetReadByteHandler(int i, pMips3ReadByteHandler pHandler);
int Mips3SetWriteByteHandler(int i, pMips3WriteByteHandler pHandler);

int Mips3SetReadHalfHandler(int i, pMips3ReadHalfHandler pHandler);
int Mips3SetWriteHalfHandler(int i, pMips3WriteHalfHandler pHandler);

int Mips3SetReadWordHandler(int i, pMips3ReadWordHandler pHandler);
int Mips3SetWriteWordHandler(int i, pMips3WriteWordHandler pHandler);

int Mips3SetReadDoubleHandler(int i, pMips3ReadDoubleHandler pHandler);
int Mips3SetWriteDoubleHandler(int i, pMips3WriteDoubleHandler pHandler);

void Mips3SetIRQLine(const int line, const int state);

#endif // MIPS3_INTF

