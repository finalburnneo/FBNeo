#ifndef MIPS3_INTF
#define MIPS3_INTF

#include "mips3/mips3_common.h"

typedef unsigned char (*pMips3ReadByteHandler)(unsigned int a);
typedef void (*pMips3WriteByteHandler)(unsigned int a, unsigned char d);

typedef unsigned short (*pMips3ReadHalfHandler)(unsigned int a);
typedef void (*pMips3WriteHalfHandler)(unsigned int a, unsigned short d);

typedef unsigned int (*pMips3ReadWordHandler)(unsigned int a);
typedef void (*pMips3WriteWordHandler)(unsigned int a, unsigned int d);

typedef unsigned long long (*pMips3ReadDoubleHandler)(unsigned int a);
typedef void (*pMips3WriteDoubleHandler)(unsigned int a, unsigned long long d);

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

