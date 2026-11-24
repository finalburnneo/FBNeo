#ifndef MIPS3_INTF
#define MIPS3_INTF

#include <stdint.h>

// MIPS III (R4600) / MIPS IV (VR5500) interpreter, little-endian guest.
// Addresses given to Mips3MapMemory/Mips3MapHandler and passed to handlers are physical:
// 29 bits on the R4600, 32 bits on the VR5500 (only reachable through the TLB above 512MB).

#define MIPS3_TYPE_R4600	0
#define MIPS3_TYPE_VR5500	1	// MIPS IV, VR5500 MUL/MADD/ROR extensions, 1GB pages

typedef UINT8 (*pMips3ReadByteHandler)(UINT32 a);
typedef void (*pMips3WriteByteHandler)(UINT32 a, UINT8 d);

typedef UINT16 (*pMips3ReadHalfHandler)(UINT32 a);
typedef void (*pMips3WriteHalfHandler)(UINT32 a, UINT16 d);

typedef UINT32 (*pMips3ReadWordHandler)(UINT32 a);
typedef void (*pMips3WriteWordHandler)(UINT32 a, UINT32 d);

typedef UINT64 (*pMips3ReadDoubleHandler)(UINT32 a);
typedef void (*pMips3WriteDoubleHandler)(UINT32 a, UINT64 d);

int Mips3Init(int nType = MIPS3_TYPE_R4600);
int Mips3UseRecompiler(bool use);   // kept for compatibility, ignored
int Mips3Exit();
void Mips3Reset();
int Mips3Run(int cycles);
void Mips3RunEnd();
int Mips3Idle(int cycles);
INT64 Mips3TotalCycles();
void Mips3NewFrame();
unsigned int Mips3GetPC();
int Mips3Scan(int nAction);

// MAP_FETCH sets the memory used for instruction fetches, so a page can have a data read handler
// (e.g. adding wait states) while code is still fetched and cached from memory.
int Mips3MapMemory(unsigned char* pMemory, unsigned int nStart, unsigned int nEnd, int nType);
int Mips3MapHandler(uintptr_t nHandler, unsigned int nStart, unsigned int nEnd, int nType);

// Must be called when something other than the CPU (DMA, driver code...) writes code into mapped memory
void Mips3InvalidateRange(unsigned int nStart, unsigned int nEnd);
// called before the VR5500 data cache model touches memory directly (drivers that let other threads use RAM)
void Mips3SetCacheSyncCallback(void (*pCallback)());
// write tracking of the RAM from physical address 0 (4KB pages, below nBytes): before the cpu first writes
// page n through a direct map while pMap[n] is 0, pCallback(n) runs (if not NULL) and pMap[n] is set;
// call again after clearing bytes of pMap, so that pages held in the host write cache are seen again
// with tracking on, a load with ACB_RUNAHEAD keeps the code cache: the driver must invalidate the RAM it restores
void Mips3SetWriteTrack(unsigned char *pMap, unsigned int nBytes, void (*pCallback)(unsigned int nPage));

int Mips3SetReadByteHandler(int i, pMips3ReadByteHandler pHandler);
int Mips3SetWriteByteHandler(int i, pMips3WriteByteHandler pHandler);

int Mips3SetReadHalfHandler(int i, pMips3ReadHalfHandler pHandler);
int Mips3SetWriteHalfHandler(int i, pMips3WriteHalfHandler pHandler);

int Mips3SetReadWordHandler(int i, pMips3ReadWordHandler pHandler);
int Mips3SetWriteWordHandler(int i, pMips3WriteWordHandler pHandler);

int Mips3SetReadDoubleHandler(int i, pMips3ReadDoubleHandler pHandler);
int Mips3SetWriteDoubleHandler(int i, pMips3WriteDoubleHandler pHandler);

// adds cycles to the running instruction, for wait states in memory handlers
void Mips3BurnCycles(int cycles);

// line 0-4 -> Cause.IP2-IP6 (IP7 is the internal Count/Compare timer)
void Mips3SetIRQLine(const int line, const int state);

extern struct cpu_core_config Mips3Config;

#endif // MIPS3_INTF
