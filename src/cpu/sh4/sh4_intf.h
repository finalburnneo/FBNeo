#include <stdint.h>

#ifndef FASTCALL
 #undef __fastcall
 #define __fastcall
#endif

typedef UINT8 (__fastcall *pSh3ReadByteHandler)(UINT32 a);
typedef void (__fastcall *pSh3WriteByteHandler)(UINT32 a, UINT8 d);
typedef UINT16 (__fastcall *pSh3ReadWordHandler)(UINT32 a);
typedef void (__fastcall *pSh3WriteWordHandler)(UINT32 a, UINT16 d);
typedef UINT32 (__fastcall *pSh3ReadLongHandler)(UINT32 a);
typedef void (__fastcall *pSh3WriteLongHandler)(UINT32 a, UINT32 d);

void __fastcall Sh3WriteByte(UINT32 a, UINT8 d);
UINT8 __fastcall Sh3ReadByte(UINT32 a);

void Sh3Init(INT32 num, INT32 hz, char md0, char md1, char md2, char md3, char md4, char md5, char md6, char md7, char md8 );
void Sh3Exit();

void sh4_set_cave_blitter_delay_func(void (*pfunc)(int));
void sh4_set_cave_blitter_delay_timer(int cycles);
INT32 sh4_get_cpu_speed();
void Sh3SetClockCV1k(INT32 clock);

void Sh3SetTimerGranularity(INT32 timergransh); // speedhack

void Sh3Open(const INT32 i);
void Sh3Close();
INT32 Sh3GetActive();

void Sh3Reset();
INT32 Sh3Run(INT32 cycles);

void Sh3SetIRQLine(INT32 line, INT32 state);

INT32 Sh3MapMemory(UINT8* pMemory, UINT32 nStart, UINT32 nEnd, INT32 nType);
INT32 Sh3MapHandler(uintptr_t nHandler, UINT32 nStart, UINT32 nEnd, INT32 nType);

INT32 Sh3SetReadPortHandler(pSh3ReadLongHandler pHandler);
INT32 Sh3SetWritePortHandler(pSh3WriteLongHandler pHandler);

INT32 Sh3SetReadByteHandler(INT32 i, pSh3ReadByteHandler pHandler);
INT32 Sh3SetWriteByteHandler(INT32 i, pSh3WriteByteHandler pHandler);
INT32 Sh3SetReadWordHandler(INT32 i, pSh3ReadWordHandler pHandler);
INT32 Sh3SetWriteWordHandler(INT32 i, pSh3WriteWordHandler pHandler);
INT32 Sh3SetReadLongHandler(INT32 i, pSh3ReadLongHandler pHandler);
INT32 Sh3SetWriteLongHandler(INT32 i, pSh3WriteLongHandler pHandler);

UINT32 Sh3GetPC(INT32 n);
void Sh3RunEnd();

void Sh3BurnUntilInt();

INT32 Sh3TotalCycles();
void Sh3NewFrame();
void Sh3BurnCycles(INT32 cycles);
INT32 Sh3Idle(INT32 cycles);
void Sh3SetEatCycles(INT32 i);

INT32 Sh3Scan(INT32 nAction);


void Sh3CheatWriteByte(UINT32 a, UINT8 d); // cheat core
UINT8 Sh3CheatReadByte(UINT32 a);

extern struct cpu_core_config Sh3Config;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachSh3(clock)	\
	BurnTimerAttach(&Sh3Config, clock)
