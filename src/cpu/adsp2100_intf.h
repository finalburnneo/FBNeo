#ifndef ADSP2100_INTF
#define ADSP2100_INTF

#include "adsp2100/adsp2100_defs.h"

typedef unsigned short (*pAdsp2100ReadWordHandler)(unsigned int a);
typedef void (*pAdsp2100WriteWordHandler)(unsigned int a, unsigned short d);

typedef unsigned int (*pAdsp2100ReadLongHandler)(unsigned int a);
typedef void (*pAdsp2100WriteLongHandler)(unsigned int a, unsigned int d);

typedef int (*pAdsp2100RxCallback)(int port);
typedef void  (*pAdsp2100TxCallback)(int port, int data);
typedef void (*pAdsp2100TimerCallback)(int enable);

int Adsp2100Init();
int Adsp2100Exit();
void Adsp2100Reset();
int Adsp2100Run(int cycles);
int Adsp2100LoadBootROM(void *src, void *dst);

int Adsp2100TotalCycles();
void Adsp2100NewFrame();
void Adsp2100RunEnd();
void Adsp2100Scan(INT32 nAction);

void Adsp2100SetRxCallback(pAdsp2100RxCallback cb);
void Adsp2100SetTxCallback(pAdsp2100TxCallback cb);
void Adsp2100SetTimerCallback(pAdsp2100TimerCallback cb);
void Adsp2100SetIRQCallback(int (*irq)(int));

int Adsp2100MapMemory(unsigned char* pMemory, unsigned int nStart, unsigned int nEnd, int nType);
int Adsp2100MapHandler(uintptr_t nHandler, unsigned int nStart, unsigned int nEnd, int nType);

int Adsp2100MapData(unsigned char* pMemory, unsigned int nStart, unsigned int nEnd, int nType);
int Adsp2100MapDataHandler(uintptr_t nHandler, unsigned int nStart, unsigned int nEnd, int nType);

int Adsp2100SetReadLongHandler(int i, pAdsp2100ReadLongHandler pHandler);
int Adsp2100SetWriteLongHandler(int i, pAdsp2100WriteLongHandler pHandler);

int Adsp2100SetReadDataWordHandler(int i, pAdsp2100ReadWordHandler pHandler);
int Adsp2100SetWriteDataWordHandler(int i, pAdsp2100WriteWordHandler pHandler);

void Adsp2100SetIRQLine(const int line, const int state);

adsp2100_state *Adsp2100GetState();

#endif // ADSP2100_INTF

