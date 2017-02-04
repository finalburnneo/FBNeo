
#ifndef FASTCALL
 #undef __fastcall
 #define __fastcall
#endif

typedef unsigned char (__fastcall *pSh2ReadByteHandler)(unsigned int a);
typedef void (__fastcall *pSh2WriteByteHandler)(unsigned int a, unsigned char d);
typedef unsigned short (__fastcall *pSh2ReadWordHandler)(unsigned int a);
typedef void (__fastcall *pSh2WriteWordHandler)(unsigned int a, unsigned short d);
typedef unsigned int (__fastcall *pSh2ReadLongHandler)(unsigned int a);
typedef void (__fastcall *pSh2WriteLongHandler)(unsigned int a, unsigned int d);

extern int has_sh2;
extern INT32 cps3speedhack;
extern INT32 sh2_suprnova_speedhack;
extern INT32 sh2_busyloop_speedhack_mode2;

void __fastcall Sh2WriteByte(unsigned int a, unsigned char d);
unsigned char __fastcall Sh2ReadByte(unsigned int a);

int Sh2Init(int nCount);
int Sh2Exit();

void Sh2Open(const int i);
void Sh2Close();
int Sh2GetActive();

void Sh2Reset();
void Sh2Reset(unsigned int pc, unsigned r15); // hack
int Sh2Run(int cycles);

int Sh2MapMemory(unsigned char* pMemory, unsigned int nStart, unsigned int nEnd, int nType);
int Sh2MapHandler(uintptr_t nHandler, unsigned int nStart, unsigned int nEnd, int nType);

int Sh2SetReadByteHandler(int i, pSh2ReadByteHandler pHandler);
int Sh2SetWriteByteHandler(int i, pSh2WriteByteHandler pHandler);
int Sh2SetReadWordHandler(int i, pSh2ReadWordHandler pHandler);
int Sh2SetWriteWordHandler(int i, pSh2WriteWordHandler pHandler);
int Sh2SetReadLongHandler(int i, pSh2ReadLongHandler pHandler);
int Sh2SetWriteLongHandler(int i, pSh2WriteLongHandler pHandler);

void Sh2SetIRQLine(const int line, const int state);

unsigned int Sh2GetPC(int n);
void Sh2StopRun();
void Sh2SetVBR(unsigned int i);

void Sh2BurnUntilInt(int);

int Sh2TotalCycles();
void Sh2NewFrame();
void Sh2BurnCycles(int cycles);
void Sh2Idle(int cycles);

int Sh2Scan(int);

