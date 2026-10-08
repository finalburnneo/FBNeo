#ifndef MIPS3_INTF
#define MIPS3_INTF

// MIPS III (R4600) interpreter, little-endian guest.
// Addresses given to Mips3MapMemory/Mips3MapHandler and passed to handlers are physical.

typedef UINT8 (*pMips3ReadByteHandler)(UINT32 a);
typedef void (*pMips3WriteByteHandler)(UINT32 a, UINT8 d);

typedef UINT16 (*pMips3ReadHalfHandler)(UINT32 a);
typedef void (*pMips3WriteHalfHandler)(UINT32 a, UINT16 d);

typedef UINT32 (*pMips3ReadWordHandler)(UINT32 a);
typedef void (*pMips3WriteWordHandler)(UINT32 a, UINT32 d);

typedef UINT64 (*pMips3ReadDoubleHandler)(UINT32 a);
typedef void (*pMips3WriteDoubleHandler)(UINT32 a, UINT64 d);

int Mips3Init();
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

int Mips3MapMemory(unsigned char* pMemory, unsigned int nStart, unsigned int nEnd, int nType);
int Mips3MapHandler(uintptr_t nHandler, unsigned int nStart, unsigned int nEnd, int nType);

// Must be called when something other than the CPU (DMA, driver code...) writes code into mapped memory
void Mips3InvalidateRange(unsigned int nStart, unsigned int nEnd);

int Mips3SetReadByteHandler(int i, pMips3ReadByteHandler pHandler);
int Mips3SetWriteByteHandler(int i, pMips3WriteByteHandler pHandler);

int Mips3SetReadHalfHandler(int i, pMips3ReadHalfHandler pHandler);
int Mips3SetWriteHalfHandler(int i, pMips3WriteHalfHandler pHandler);

int Mips3SetReadWordHandler(int i, pMips3ReadWordHandler pHandler);
int Mips3SetWriteWordHandler(int i, pMips3WriteWordHandler pHandler);

int Mips3SetReadDoubleHandler(int i, pMips3ReadDoubleHandler pHandler);
int Mips3SetWriteDoubleHandler(int i, pMips3WriteDoubleHandler pHandler);

// line 0-4 -> Cause.IP2-IP6 (IP7 is the internal Count/Compare timer)
void Mips3SetIRQLine(const int line, const int state);

extern struct cpu_core_config Mips3Config;

#endif // MIPS3_INTF
