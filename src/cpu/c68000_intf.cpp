// 680x0 (Sixty Eight K) Interface
// Interface picked from pfba (https://github.com/Cpasjuste/pfba)
#include "burnint.h"
#include "m68000_intf.h"
#include "m68000_debug.h"

#define EMU_C68K

#ifdef EMU_M68K
INT32 nSekM68KContextSize[SEK_MAX];
INT8* SekM68KContext[SEK_MAX];
#endif

#ifdef EMU_C68K
#include "Cyclone.h"
struct Cyclone c68k[SEK_MAX];
static bool bCycloneInited = false;
#endif

INT32 nSekCount = -1;							// Number of allocated 68000s
struct SekExt *SekExt[SEK_MAX] = { NULL, }, *pSekExt = NULL;

INT32 nSekActive = -1;								// The cpu which is currently being emulated
INT32 nSekCyclesTotal, nSekCyclesScanline, nSekCyclesSegment, nSekCyclesDone, nSekCyclesToDo;

INT32 nSekCPUType[SEK_MAX], nSekCycles[SEK_MAX], nSekIRQPending[SEK_MAX], nSekRESETLine[SEK_MAX], nSekHALT[SEK_MAX];
INT32 nSekCyclesToDoCache[SEK_MAX], nSekm68k_ICount[SEK_MAX];

static UINT32 nSekAddressMask[SEK_MAX], nSekAddressMaskActive;

static INT32 core_idle(INT32 cycles)
{
	SekRunAdjust(cycles);

	return cycles;
}

static void core_set_irq(INT32 cpu, INT32 line, INT32 state)
{
	INT32 active = nSekActive;
	if (active != cpu)
	{
		if (active != -1) SekClose();
		SekOpen(cpu);
	}

	SekSetIRQLine(line, state);

	if (active != cpu)
	{
		SekClose();
		if (active != -1) SekOpen(active);
	}
}

cpu_core_config SekConfig =
{
	"68k",
	SekOpen,
	SekClose,
	SekCheatRead,
	SekWriteByteROM,
	SekGetActive,
	SekTotalCycles,
	SekNewFrame,
	core_idle,
	core_set_irq,
	SekRun,
	SekRunEnd,
	SekReset,
	0x1000000,
	0
};

// ----------------------------------------------------------------------------
// Default memory access handlers

UINT8 __fastcall DefReadByte(UINT32) { return 0; }
void __fastcall DefWriteByte(UINT32, UINT8) { }

#define DEFWORDHANDLERS(i)																				\
	UINT16 __fastcall DefReadWord##i(UINT32 a) { SEK_DEF_READ_WORD(i, a) }				\
	void __fastcall DefWriteWord##i(UINT32 a, UINT16 d) { SEK_DEF_WRITE_WORD(i, a ,d) }
#define DEFLONGHANDLERS(i)																				\
	UINT32 __fastcall DefReadLong##i(UINT32 a) { SEK_DEF_READ_LONG(i, a) }					\
	void __fastcall DefWriteLong##i(UINT32 a, UINT32 d) { SEK_DEF_WRITE_LONG(i, a , d) }

DEFWORDHANDLERS(0)
DEFLONGHANDLERS(0)

#if SEK_MAXHANDLER >= 2
 DEFWORDHANDLERS(1)
 DEFLONGHANDLERS(1)
#endif

#if SEK_MAXHANDLER >= 3
 DEFWORDHANDLERS(2)
 DEFLONGHANDLERS(2)
#endif

#if SEK_MAXHANDLER >= 4
 DEFWORDHANDLERS(3)
 DEFLONGHANDLERS(3)
#endif

#if SEK_MAXHANDLER >= 5
 DEFWORDHANDLERS(4)
 DEFLONGHANDLERS(4)
#endif

#if SEK_MAXHANDLER >= 6
 DEFWORDHANDLERS(5)
 DEFLONGHANDLERS(5)
#endif

#if SEK_MAXHANDLER >= 7
 DEFWORDHANDLERS(6)
 DEFLONGHANDLERS(6)
#endif

#if SEK_MAXHANDLER >= 8
 DEFWORDHANDLERS(7)
 DEFLONGHANDLERS(7)
#endif

#if SEK_MAXHANDLER >= 9
 DEFWORDHANDLERS(8)
 DEFLONGHANDLERS(8)
#endif

#if SEK_MAXHANDLER >= 10
 DEFWORDHANDLERS(9)
 DEFLONGHANDLERS(9)
#endif

// ----------------------------------------------------------------------------
// Memory access functions

// Mapped Memory lookup (               for read)
#define FIND_R(x) pSekExt->MemMap[ x >> SEK_SHIFT]
// Mapped Memory lookup (+ SEK_WADD     for write)
#define FIND_W(x) pSekExt->MemMap[(x >> SEK_SHIFT) + SEK_WADD]
// Mapped Memory lookup (+ SEK_WADD * 2 for fetch)
#define FIND_F(x) pSekExt->MemMap[(x >> SEK_SHIFT) + SEK_WADD * 2]

// Normal memory access functions
inline static UINT8 ReadByte(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("read8 0x%08X\n"), a);

	pr = FIND_R(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		a ^= 1;
		return pr[a & SEK_PAGEM];
	}
	return pSekExt->ReadByte[(uintptr_t)pr](a);
}

inline static UINT8 FetchByte(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("fetch8 0x%08X\n"), a);

	pr = FIND_F(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		a ^= 1;
		return pr[a & SEK_PAGEM];
	}
	return pSekExt->ReadByte[(uintptr_t)pr](a);
}

inline static void WriteByte(UINT32 a, UINT8 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("write8 0x%08X\n"), a);

	pr = FIND_W(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		a ^= 1;
		pr[a & SEK_PAGEM] = (UINT8)d;
		return;
	}
	pSekExt->WriteByte[(uintptr_t)pr](a, d);
}

inline static void WriteByteROM(UINT32 a, UINT8 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

	pr = FIND_R(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		a ^= 1;
		pr[a & SEK_PAGEM] = (UINT8)d;
		return;
	}
	pSekExt->WriteByte[(uintptr_t)pr](a, d);
}

inline static UINT16 ReadWord(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("read16 0x%08X\n"), a);

	pr = FIND_R(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER)
	{
		if (a & 1)
		{
			return BURN_ENDIAN_SWAP_INT16((ReadByte(a + 0) * 256) + ReadByte(a + 1));
		}
		else
		{
			return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(pr + (a & SEK_PAGEM))));
		}
	}

	return pSekExt->ReadWord[(uintptr_t)pr](a);
}

inline static UINT16 FetchWord(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("fetch16 0x%08X\n"), a);

	pr = FIND_F(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(pr + (a & SEK_PAGEM))));
	}

	return pSekExt->ReadWord[(uintptr_t)pr](a);
}

inline static void WriteWord(UINT32 a, UINT16 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("write16 0x%08X\n"), a);

	pr = FIND_W(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER)
	{
		if (a & 1)
		{
		//	bprintf(PRINT_NORMAL, _T("write16 0x%08X\n"), a);

			d = BURN_ENDIAN_SWAP_INT16(d);

			WriteByte(a + 0, d / 0x100);
			WriteByte(a + 1, d);

			return;
		}
		else
		{
			*((UINT16*)(pr + (a & SEK_PAGEM))) = (UINT16)BURN_ENDIAN_SWAP_INT16(d);
			return;
		}
	}

	pSekExt->WriteWord[(uintptr_t)pr](a, d);
}

inline static void WriteWordROM(UINT32 a, UINT16 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

	pr = FIND_R(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		*((UINT16*)(pr + (a & SEK_PAGEM))) = (UINT16)d;
		return;
	}
	pSekExt->WriteWord[(uintptr_t)pr](a, d);
}

// be [3210] -> (r >> 16) | (r << 16) -> [1032] -> UINT32(le) = -> [0123]
// le [0123]

inline static UINT32 ReadLong(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("read32 0x%08X\n"), a);

	pr = FIND_R(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER)
	{
		UINT32 r = 0;

		if (a & 1)
		{
			r  = ReadByte((a + 0)) * 0x1000000;
			r += ReadByte((a + 1)) * 0x10000;
			r += ReadByte((a + 2)) * 0x100;
			r += ReadByte((a + 3));

			return BURN_ENDIAN_SWAP_INT32(r);
		}
		else
		{
			r = *((UINT32*)(pr + (a & SEK_PAGEM)));
			r = (r >> 16) | (r << 16);

			return BURN_ENDIAN_SWAP_INT32(r);
		}
	}

	return pSekExt->ReadLong[(uintptr_t)pr](a);
}

inline static UINT32 FetchLong(UINT32 a)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("fetch32 0x%08X\n"), a);

	pr = FIND_F(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		UINT32 r = *((UINT32*)(pr + (a & SEK_PAGEM)));
		r = (r >> 16) | (r << 16);
		return BURN_ENDIAN_SWAP_INT32(r);
	}
	return pSekExt->ReadLong[(uintptr_t)pr](a);
}

inline static void WriteLong(UINT32 a, UINT32 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

//	bprintf(PRINT_NORMAL, _T("write32 0x%08X\n"), a);

	pr = FIND_W(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER)
	{
		if (a & 1)
		{
		//	bprintf(PRINT_NORMAL, _T("write32 0x%08X 0x%8.8x\n"), a,d);

			d = BURN_ENDIAN_SWAP_INT32(d);

			WriteByte((a + 0), d / 0x1000000);
			WriteByte((a + 1), d / 0x10000);
			WriteByte((a + 2), d / 0x100);
			WriteByte((a + 3), d);

			return;
		}
		else
		{
			d = (d >> 16) | (d << 16);
			*((UINT32*)(pr + (a & SEK_PAGEM))) = BURN_ENDIAN_SWAP_INT32(d);

			return;
		}
	}
	pSekExt->WriteLong[(uintptr_t)pr](a, d);
}

inline static void WriteLongROM(UINT32 a, UINT32 d)
{
	UINT8* pr;

	a &= nSekAddressMaskActive;

	pr = FIND_R(a);
	if ((uintptr_t)pr >= SEK_MAXHANDLER) {
		d = (d >> 16) | (d << 16);
		*((UINT32*)(pr + (a & SEK_PAGEM))) = d;
		return;
	}
	pSekExt->WriteLong[(uintptr_t)pr](a, d);
}

// Normal memory access functions
#ifdef EMU_C68K
extern "C" {
UINT32 __fastcall m68k_read8(UINT32 a) { return (UINT32)ReadByte(a); }
UINT32 __fastcall m68k_read16(UINT32 a) { return (UINT32)ReadWord(a); }
UINT32 __fastcall m68k_read32(UINT32 a) { return               ReadLong(a); }

UINT32 __fastcall m68k_fetch8(UINT32 a) { return (UINT32)FetchByte(a); }
UINT32 __fastcall m68k_fetch16(UINT32 a) { return (UINT32)FetchWord(a); }
UINT32 __fastcall m68k_fetch32(UINT32 a) { return               FetchLong(a); }

void __fastcall m68k_write8(UINT32 a, UINT32 d) { WriteByte(a, d); }
void __fastcall m68k_write16(UINT32 a, UINT32 d) { WriteWord(a, d); }
void __fastcall m68k_write32(UINT32 a, UINT32 d) { WriteLong(a, d); }
}
#endif

#ifdef EMU_M68K
extern "C" {
UINT32 __fastcall M68KReadByte(UINT32 a) { return (UINT32)ReadByte(a); }
UINT32 __fastcall M68KReadWord(UINT32 a) { return (UINT32)ReadWord(a); }
UINT32 __fastcall M68KReadLong(UINT32 a) { return               ReadLong(a); }

UINT32 __fastcall M68KFetchByte(UINT32 a) { return (UINT32)FetchByte(a); }
UINT32 __fastcall M68KFetchWord(UINT32 a) { return (UINT32)FetchWord(a); }
UINT32 __fastcall M68KFetchLong(UINT32 a) { return               FetchLong(a); }

void __fastcall M68KWriteByte(UINT32 a, UINT32 d) { WriteByte(a, d); }
void __fastcall M68KWriteWord(UINT32 a, UINT32 d) { WriteWord(a, d); }
void __fastcall M68KWriteLong(UINT32 a, UINT32 d) { WriteLong(a, d); }
}
#endif

// ----------------------------------------------------------------------------
// Memory accesses (non-emu specific)

UINT32 SekReadByte(UINT32 a) { return (UINT32)ReadByte(a); }
UINT32 SekReadWord(UINT32 a) { return (UINT32)ReadWord(a); }
UINT32 SekReadLong(UINT32 a) { return ReadLong(a); }

UINT32 SekFetchByte(UINT32 a) { return (UINT32)FetchByte(a); }
UINT32 SekFetchWord(UINT32 a) { return (UINT32)FetchWord(a); }
UINT32 SekFetchLong(UINT32 a) { return FetchLong(a); }

void SekWriteByte(UINT32 a, UINT8 d) { WriteByte(a, d); }
void SekWriteWord(UINT32 a, UINT16 d) { WriteWord(a, d); }
void SekWriteLong(UINT32 a, UINT32 d) { WriteLong(a, d); }

void SekWriteByteROM(UINT32 a, UINT8 d) { WriteByteROM(a, d); }
void SekWriteWordROM(UINT32 a, UINT16 d) { WriteWordROM(a, d); }
void SekWriteLongROM(UINT32 a, UINT32 d) { WriteLongROM(a, d); }

// ----------------------------------------------------------------------------
// Callbacks for C68K

#ifdef EMU_C68K
extern "C" unsigned int m68k_checkpc(UINT32 pc)
{
	pc -= c68k[nSekActive].membase; // Get real pc
	pc &= nSekAddressMaskActive;

	c68k[nSekActive].membase = (uintptr_t)FIND_F(pc) - (pc & ~SEK_PAGEM);

	return c68k[nSekActive].membase + pc;
}

extern "C" INT32 C68KIRQAcknowledge(INT32 nIRQ)
{
	if (nSekIRQPending[nSekActive] & SEK_IRQSTATUS_AUTO) {
		c68k[nSekActive].irq = 0;
		nSekIRQPending[nSekActive] = 0;
	}

	if (pSekExt->IrqCallback) {
		return pSekExt->IrqCallback(nIRQ);
	}

	return CYCLONE_INT_ACK_AUTOVECTOR;
}

extern "C" void C68KResetCallback()
{
	if (pSekExt->ResetCallback) {
		pSekExt->ResetCallback();
	}
}

extern "C" int C68KUnrecognizedCallback()
{
#if defined (FBNEO_DEBUG)
	bprintf(PRINT_NORMAL, _T("UnrecognizedCallback();\n"));
#endif
	return 0;
}

extern "C" void C68KRTECallback()
{
	if (pSekExt->RTECallback) {
		pSekExt->RTECallback();
	}
}

extern "C" void C68KcmpildCallback(UINT32 val, INT32 reg)
{
	if (pSekExt->CmpCallback) {
		pSekExt->CmpCallback(val, reg);
	}
}

extern "C" INT32 C68KTASCallback()
{
	if (pSekExt->TASCallback) {
		return pSekExt->TASCallback();
	}

	return 0; // disabled by default
}
#endif

// ----------------------------------------------------------------------------
// Callbacks for Musashi

#ifdef EMU_M68K
extern "C" INT32 M68KIRQAcknowledge(INT32 nIRQ)
{
	if (nSekIRQPending[nSekActive] & SEK_IRQSTATUS_AUTO) {
		m68k_set_irq(0);
		nSekIRQPending[nSekActive] = 0;
	}

	if (nSekIRQPending[nSekActive] & SEK_IRQSTATUS_VAUTO &&
		(nSekIRQPending[nSekActive] & 0x0007) == nIRQ) {
		m68k_set_virq(nIRQ, 0);
		nSekIRQPending[nSekActive] = 0;
	}

	if (pSekExt->IrqCallback) {
		return pSekExt->IrqCallback(nIRQ);
	}

	return M68K_INT_ACK_AUTOVECTOR;
}

extern "C" void M68KResetCallback()
{
	if (pSekExt->ResetCallback) {
		pSekExt->ResetCallback();
	}
}

extern "C" void M68KRTECallback()
{
	if (pSekExt->RTECallback) {
		pSekExt->RTECallback();
	}
}

extern "C" void M68KcmpildCallback(UINT32 val, INT32 reg)
{
	if (pSekExt->CmpCallback) {
		pSekExt->CmpCallback(val, reg);
	}
}

extern "C" INT32 M68KTASCallback()
{
	if (pSekExt->TASCallback) {
		return pSekExt->TASCallback();
	}
	
	return 1; // enable by default
}
#endif

// ## SekCPUPush() / SekCPUPop() ## internal helpers for sending signals to other 68k's
struct m68kpstack {
	INT32 nHostCPU;
	INT32 nPushedCPU;
};
#define MAX_PSTACK 10

static m68kpstack pstack[MAX_PSTACK];
static INT32 pstacknum = 0;

static void SekCPUPush(INT32 nCPU)
{
	m68kpstack *p = &pstack[pstacknum++];

	if (pstacknum + 1 >= MAX_PSTACK) {
		bprintf(0, _T("SekCPUPush(): out of stack!  Possible infinite recursion?  Crash pending..\n"));
	}

	p->nPushedCPU = nCPU;

	p->nHostCPU = SekGetActive();

	if (p->nHostCPU != p->nPushedCPU) {
		if (p->nHostCPU != -1) SekClose();
		SekOpen(p->nPushedCPU);
	}
}

static void SekCPUPop()
{
	m68kpstack *p = &pstack[--pstacknum];

	if (p->nHostCPU != p->nPushedCPU) {
		SekClose();
		if (p->nHostCPU != -1) SekOpen(p->nHostCPU);
	}
}

// ----------------------------------------------------------------------------
// Initialisation/exit/reset

#ifdef EMU_C68K
static INT32 SekInitCPUC68K(INT32 nCount, INT32 nCPUType)
{
#if defined (FBNEO_DEBUG)
	bprintf(PRINT_NORMAL, _T("EMU_C68K: SekInitCPUC68K(%i, %x)\n"), nCount, nCPUType);
#endif
	if (nCPUType != 0x68000) {
		return 1;
	}

	nSekCPUType[nCount] = nCPUType;

	if (!bCycloneInited) {
#if defined (FBNEO_DEBUG)
		bprintf(PRINT_NORMAL, _T("EMU_C68K: CycloneInit\n"));
#endif
		CycloneInit();
#if defined (FBNEO_DEBUG)
		bprintf(PRINT_NORMAL, _T("EMU_C68K: CycloneInit OK\n"));
#endif
		bCycloneInited = true;
	}
	memset(&c68k[nCount], 0, sizeof(Cyclone));
	c68k[nCount].checkpc = m68k_checkpc;
	c68k[nCount].IrqCallback = C68KIRQAcknowledge;
	c68k[nCount].ResetCallback = C68KResetCallback;
	c68k[nCount].UnrecognizedCallback = C68KUnrecognizedCallback;

	return 0;
}
#endif

#ifdef EMU_M68K
static INT32 SekInitCPUM68K(INT32 nCount, INT32 nCPUType)
{
#if defined (FBNEO_DEBUG)
	bprintf(PRINT_NORMAL, _T("EMU_M68K: SekInitCPUM68K(%i, %x)\n"), nCount, nCPUType);
#endif
	nSekCPUType[nCount] = nCPUType;

	switch (nCPUType) {
		case 0x68000:
			m68k_set_cpu_type(M68K_CPU_TYPE_68000);
			break;
		case 0x68010:
			m68k_set_cpu_type(M68K_CPU_TYPE_68010);
			break;
		case 0x68EC020:
			m68k_set_cpu_type(M68K_CPU_TYPE_68EC020);
			break;
		default:
			return 1;
	}

	nSekM68KContextSize[nCount] = m68k_context_size();
	SekM68KContext[nCount] = (INT8*)malloc(nSekM68KContextSize[nCount]);
	if (SekM68KContext[nCount] == NULL) {
		return 1;
	}
	memset(SekM68KContext[nCount], 0, nSekM68KContextSize[nCount]);
	m68k_get_context(SekM68KContext[nCount]);

	return 0;
}
#endif

void SekNewFrame()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekNewFrame called without init\n"));
#endif

	for (INT32 i = 0; i <= nSekCount; i++) {
		nSekCycles[i] = 0;
		nSekCyclesToDoCache[i] = 0;
		nSekm68k_ICount[i] = 0;
	}

#ifdef EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
		nSekCyclesToDo = c68k[nSekActive].cycles = 0;
	} else {
#endif
		nSekCyclesToDo = m68k_ICount = 0;
#ifdef EMU_C68K
	}
#endif
	nSekCyclesTotal = 0;
}

void SekSetCyclesScanline(INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetCyclesScanline called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetCyclesScanline called when no CPU open\n"));
#endif

	nSekCyclesScanline = nCycles;
}

UINT8 SekCheatRead(UINT32 a)
{
	return SekReadByte(a);
}

INT32 SekInit(INT32 nCount, INT32 nCPUType)
{
	DebugCPU_SekInitted = 1;

	struct SekExt* ps = NULL;

/*#if !defined BUILD_A68K
	bBurnUseASMCPUEmulation = false;
#endif*/

	if (nSekActive >= 0) {
		SekClose();
		nSekActive = -1;
	}

	if (nCount > nSekCount) {
		nSekCount = nCount;
	}

	// Allocate cpu extenal data (memory map etc)
	SekExt[nCount] = (struct SekExt*)malloc(sizeof(struct SekExt));
	if (SekExt[nCount] == NULL) {
		SekExit();
		return 1;
	}
	memset(SekExt[nCount], 0, sizeof(struct SekExt));

	// Put in default memory handlers
	ps = SekExt[nCount];

	for (INT32 j = 0; j < SEK_MAXHANDLER; j++) {
		ps->ReadByte[j]  = DefReadByte;
		ps->WriteByte[j] = DefWriteByte;
	}

	ps->ReadWord[0]  = DefReadWord0;
	ps->WriteWord[0] = DefWriteWord0;
	ps->ReadLong[0]  = DefReadLong0;
	ps->WriteLong[0] = DefWriteLong0;

#if SEK_MAXHANDLER >= 2
	ps->ReadWord[1]  = DefReadWord1;
	ps->WriteWord[1] = DefWriteWord1;
	ps->ReadLong[1]  = DefReadLong1;
	ps->WriteLong[1] = DefWriteLong1;
#endif

#if SEK_MAXHANDLER >= 3
	ps->ReadWord[2]  = DefReadWord2;
	ps->WriteWord[2] = DefWriteWord2;
	ps->ReadLong[2]  = DefReadLong2;
	ps->WriteLong[2] = DefWriteLong2;
#endif

#if SEK_MAXHANDLER >= 4
	ps->ReadWord[3]  = DefReadWord3;
	ps->WriteWord[3] = DefWriteWord3;
	ps->ReadLong[3]  = DefReadLong3;
	ps->WriteLong[3] = DefWriteLong3;
#endif

#if SEK_MAXHANDLER >= 5
	ps->ReadWord[4]  = DefReadWord4;
	ps->WriteWord[4] = DefWriteWord4;
	ps->ReadLong[4]  = DefReadLong4;
	ps->WriteLong[4] = DefWriteLong4;
#endif

#if SEK_MAXHANDLER >= 6
	ps->ReadWord[5]  = DefReadWord5;
	ps->WriteWord[5] = DefWriteWord5;
	ps->ReadLong[5]  = DefReadLong5;
	ps->WriteLong[5] = DefWriteLong5;
#endif

#if SEK_MAXHANDLER >= 7
	ps->ReadWord[6]  = DefReadWord6;
	ps->WriteWord[6] = DefWriteWord6;
	ps->ReadLong[6]  = DefReadLong6;
	ps->WriteLong[6] = DefWriteLong6;
#endif

#if SEK_MAXHANDLER >= 8
	ps->ReadWord[7]  = DefReadWord7;
	ps->WriteWord[7] = DefWriteWord7;
	ps->ReadLong[7]  = DefReadLong7;
	ps->WriteLong[7] = DefWriteLong7;
#endif

#if SEK_MAXHANDLER >= 9
	ps->ReadWord[8]  = DefReadWord8;
	ps->WriteWord[8] = DefWriteWord8;
	ps->ReadLong[8]  = DefReadLong8;
	ps->WriteLong[8] = DefWriteLong8;
#endif

#if SEK_MAXHANDLER >= 10
	ps->ReadWord[9]  = DefReadWord9;
	ps->WriteWord[9] = DefWriteWord9;
	ps->ReadLong[9]  = DefReadLong9;
	ps->WriteLong[9] = DefWriteLong9;
#endif

#if SEK_MAXHANDLER >= 11
	for (int j = 10; j < SEK_MAXHANDLER; j++) {
		ps->ReadWord[j]  = DefReadWord0;
		ps->WriteWord[j] = DefWriteWord0;
		ps->ReadLong[j]  = DefReadLong0;
		ps->WriteLong[j] = DefWriteLong0;
	}
#endif

	// Map the normal memory handlers
	SekDbgDisableBreakpoints();

#ifdef EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nCPUType == 0x68000) {
		if (SekInitCPUC68K(nCount, nCPUType)) {
			SekExit();
			return 1;
		}
	} else {
#endif

#ifdef EMU_M68K
		m68k_init();
		if (SekInitCPUM68K(nCount, nCPUType)) {
			SekExit();
			return 1;
		}
#endif

#ifdef EMU_C68K
	}
#endif

	nSekAddressMask[nCount] = 0xffffff;

	nSekCycles[nCount] = 0;
	nSekCyclesToDoCache[nCount] = 0;
	nSekm68k_ICount[nCount] = 0;

	nSekIRQPending[nCount] = 0;
	nSekRESETLine[nCount] = 0;
	nSekHALT[nCount] = 0;

	nSekCyclesTotal = 0;
	nSekCyclesScanline = 0;

	CpuCheatRegister(nCount, &SekConfig);

	return 0;
}

#ifdef EMU_M68K
static void SekCPUExitM68K(INT32 i)
{
		if(SekM68KContext[i]) {
			free(SekM68KContext[i]);
			SekM68KContext[i] = NULL;
		}
}
#endif

INT32 SekExit()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekExit called without init\n"));
#endif

	if (!DebugCPU_SekInitted) return 1;

	// Deallocate cpu extenal data (memory map etc)
	for (INT32 i = 0; i <= nSekCount; i++) {

#ifdef EMU_C68K
		if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[i] == 0x68000) {
		} else {
#endif

#ifdef EMU_M68K
		SekCPUExitM68K(i);
#endif

#ifdef EMU_C68K
		}
#endif

		// Deallocate other context data
		if (SekExt[i]) {
			free(SekExt[i]);
			SekExt[i] = NULL;
		}
	}

	pSekExt = NULL;

	nSekActive = -1;
	nSekCount = -1;

	DebugCPU_SekInitted = 0;

	return 0;
}

void SekReset()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekReset called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekReset called when no CPU open\n"));
#endif

#ifdef EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
		memset(&c68k[nSekActive], 0, 22 * 4); // clear all regs
		c68k[nSekActive].state_flags	= 0;
		c68k[nSekActive].srh			= 0x27; // Supervisor mode
		c68k[nSekActive].a[7]			= m68k_fetch32(0); // Stack Pointer
		c68k[nSekActive].membase		= 0;
		c68k[nSekActive].pc				= c68k[nSekActive].checkpc(m68k_fetch32(4));
	} else {
#endif

#ifdef EMU_M68K
		m68k_pulse_reset();
#endif

#ifdef EMU_C68K
	}
#endif

}

void SekReset(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekReset called without init\n"));
#endif

	SekCPUPush(nCPU);

	SekReset();

	SekCPUPop();
}

// ----------------------------------------------------------------------------
// Control the active CPU

// Open a CPU
void SekOpen(const INT32 i)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekOpen called without init\n"));
	if (i > nSekCount) bprintf(PRINT_ERROR, _T("SekOpen called with invalid index %x\n"), i);
	if (nSekActive != -1) bprintf(PRINT_ERROR, _T("SekOpen called when CPU already open (%x) with index %x\n"), nSekActive, i);
#endif

	if (i != nSekActive) {
		nSekActive = i;

		pSekExt = SekExt[nSekActive];						// Point to cpu context

		nSekAddressMaskActive = nSekAddressMask[nSekActive];

#ifdef EMU_C68K
		if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
			//...
		} else {
#endif

#ifdef EMU_M68K
			m68k_set_context(SekM68KContext[nSekActive]);
#endif

#ifdef EMU_C68K
		}
#endif

		nSekCyclesTotal = nSekCycles[nSekActive];

		// Allow for SekRun() reentrance:
		nSekCyclesToDo = nSekCyclesToDoCache[nSekActive];
#ifdef EMU_C68K
		if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
			c68k[nSekActive].cycles = nSekm68k_ICount[nSekActive];
		} else {
#endif
			m68k_ICount = nSekm68k_ICount[nSekActive];
#ifdef EMU_C68K
		}
#endif
	}
}

// Close the active cpu
void SekClose()
{
#ifdef EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
		//...
	} else {
#endif

#ifdef EMU_M68K
		m68k_get_context(SekM68KContext[nSekActive]);
#endif

#ifdef EMU_C68K
	}
#endif

	nSekCycles[nSekActive] = nSekCyclesTotal;

	// Allow for SekRun() reentrance:
	nSekCyclesToDoCache[nSekActive] = nSekCyclesToDo;
#ifdef EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
		nSekm68k_ICount[nSekActive] = c68k[nSekActive].cycles;
	} else {
#endif
		nSekm68k_ICount[nSekActive] = m68k_ICount;
#ifdef EMU_C68K
	}
#endif

	nSekActive = -1;
}

// Get the current CPU
INT32 SekGetActive()
{
	return nSekActive;
}

// For Megadrive - check if the vdp controlport should set IRQ
INT32 SekShouldInterrupt()
{
	if(nSekCpuCore == SEK_CORE_M68K) {
		return m68k_check_shouldinterrupt();
	} else {
		return 0;
	}
}

void SekBurnUntilInt()
{
	if(nSekCpuCore == SEK_CORE_M68K) {
		m68k_burn_until_irq(1);
	}
}

INT32 SekGetRESETLine()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetRESETLine called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekGetRESETLine called when no CPU open\n"));
#endif


	if (nSekActive != -1)
	{
		return nSekRESETLine[nSekActive];
	}

	return 0;
}

INT32 SekGetRESETLine(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetRESETLine called without init\n"));
#endif

	SekCPUPush(nCPU);

	INT32 rc = SekGetRESETLine();

	SekCPUPop();

	return rc;
}

void SekSetRESETLine(INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetRESETLine called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetRESETLine called when no CPU open\n"));
#endif

	if (nSekActive != -1)
	{
		if (nSekRESETLine[nSekActive] && nStatus == 0)
		{
			SekReset();
			//bprintf(0, _T("SEK: cleared resetline.\n"));
		}

		nSekRESETLine[nSekActive] = nStatus;
	}
}

void SekSetRESETLine(INT32 nCPU, INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetRESETLine called without init\n"));
#endif

	SekCPUPush(nCPU);

	SekSetRESETLine(nStatus);

	SekCPUPop();
}

INT32 SekGetHALT()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetHALT called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekGetHALT called when no CPU open\n"));
#endif


	if (nSekActive != -1)
	{
		return nSekHALT[nSekActive];
	}

	return 0;
}

INT32 SekGetHALT(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetHALT called without init\n"));
#endif

	SekCPUPush(nCPU);

	INT32 rc = SekGetHALT();

	SekCPUPop();

	return rc;
}

INT32 SekTotalCycles(INT32 nCPU)
{
	SekCPUPush(nCPU);

	INT32 rc = SekTotalCycles();

	SekCPUPop();

	return rc;
}

void SekSetHALT(INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetHALT called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetHALT called when no CPU open\n"));
#endif

	if (nSekActive != -1)
	{
		if (nSekHALT[nSekActive] && nStatus == 0)
		{
			//bprintf(0, _T("SEK: cleared HALT.\n"));
		}

		nSekHALT[nSekActive] = nStatus;
	}
}

void SekSetHALT(INT32 nCPU, INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetHALT called without init\n"));
#endif

	SekCPUPush(nCPU);

	SekSetHALT(nStatus);

	SekCPUPop();
}

// Set the status of an IRQ line on the active CPU
void SekSetIRQLine(const INT32 line, INT32 nstatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetIRQLine called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetIRQLine called when no CPU open\n"));
#endif

	if (nstatus == CPU_IRQSTATUS_HOLD)
		nstatus = CPU_IRQSTATUS_AUTO; // on sek, AUTO is HOLD.

	INT32 status = nstatus << 12; // needed for compatibility

	if (status) {
		nSekIRQPending[nSekActive] = line | status;

#ifdef EMU_C68K
		//printf("EMU_C68K: SekSetIRQLine\n");
		if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
			nSekCyclesTotal += (nSekCyclesToDo - nSekCyclesDone) - c68k[nSekActive].cycles;
			nSekCyclesDone += (nSekCyclesToDo - nSekCyclesDone) - c68k[nSekActive].cycles;

			c68k[nSekActive].irq = line;
			c68k[nSekActive].cycles = nSekCyclesToDo = -1;
		} else {
#endif

#ifdef EMU_M68K
			//printf("EMU_M68K: SekSetIRQLine\n");
			m68k_set_irq(line);
#endif

#ifdef EMU_C68K
		}
#endif

		return;
	}

	nSekIRQPending[nSekActive] = 0;

#ifdef EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
		c68k[nSekActive].irq = 0;
	} else {
#endif

#ifdef EMU_M68K
		m68k_set_irq(0);
#endif

#ifdef EMU_C68K
	}
#endif

}

void SekSetIRQLine(INT32 nCPU, const INT32 line, INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetIRQLine called without init\n"));
#endif

	SekCPUPush(nCPU);

	SekSetIRQLine(line, status);

	SekCPUPop();
}

void SekSetVIRQLine(const INT32 line, INT32 nstatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetIRQLine called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekSetIRQLine called when no CPU open\n"));
#endif

	if (nstatus == CPU_IRQSTATUS_AUTO) {
		nstatus = 4; // special handling for virq
	}

	INT32 status = nstatus << 12; // needed for compatibility

	if (status) {
		nSekIRQPending[nSekActive] = line | status;

#ifdef EMU_C68K
		if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
			// is there an equivalent to m68k_set_virq in cyclone ?
		} else {
#endif

#ifdef EMU_M68K
			m68k_set_virq(line, 1);
#endif

#ifdef EMU_C68K
		}
#endif

		return;
	}

	nSekIRQPending[nSekActive] = 0;

#ifdef EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
		// is there an equivalent to m68k_set_virq in cyclone ?
	} else {
#endif

#ifdef EMU_M68K
		m68k_set_virq(line, 0);
#endif

#ifdef EMU_C68K
	}
#endif
}

void SekSetVIRQLine(INT32 nCPU, const INT32 line, INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetVIRQLine called without init\n"));
#endif

	SekCPUPush(nCPU);

	SekSetVIRQLine(line, status);

	SekCPUPop();
}

// Adjust the active CPU's timeslice
void SekRunAdjust(const INT32 nCycles)
{
#ifndef EMU_C68K
	INT32 count = m68k_ICount;
#else
	INT32 count = (nSekCpuCore == SEK_CORE_C68K && nSekCPUType[nSekActive] == 0x68000) ? c68k[nSekActive].cycles : m68k_ICount;
#endif
	if (nCycles < 0 && count < -nCycles) {
		SekRunEnd();
		return;
	}

#ifdef EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
		//printf("EMU_C68K: SekRunAdjust\n");
		c68k[nSekActive].cycles += nCycles;
		nSekCyclesToDo += nCycles;
		nSekCyclesSegment += nCycles;
	} else {
#endif

#ifdef EMU_M68K
		//printf("EMU_M68K: SekRunAdjust\n");
		nSekCyclesToDo += nCycles;
		m68k_modify_timeslice(nCycles);
#endif

#ifdef EMU_C68K
	}
#endif

}

// End the active CPU's timeslice
void SekRunEnd()
{
#ifdef EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
		nSekCyclesTotal += (nSekCyclesToDo - nSekCyclesDone) - c68k[nSekActive].cycles;
		nSekCyclesDone += (nSekCyclesToDo - nSekCyclesDone) - c68k[nSekActive].cycles;
		nSekCyclesSegment = nSekCyclesDone;
		c68k[nSekActive].cycles = nSekCyclesToDo = -1;
	} else {
#endif

#ifdef EMU_M68K
		m68k_end_timeslice();
#endif

#ifdef EMU_C68K
	}
#endif

}

// Run the active CPU
INT32 SekRun(const INT32 nCycles)
{
#ifdef EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
		//printf("EMU_C68K: SekRun\n");
		nSekCyclesDone = 0;
		nSekCyclesSegment = nCycles;

		if (nSekRESETLine[nSekActive] || nSekHALT[nSekActive])
		{
			nSekCyclesSegment = nCycles; // idle when RESET high or halted
		}
		else
		{
			do {
				c68k[nSekActive].cycles = nSekCyclesToDo = nSekCyclesSegment - nSekCyclesDone;

				if (c68k[nSekActive].irq == 0x80) {						// Cpu is in stopped state till interrupt
					nSekCyclesDone = nSekCyclesSegment;
					nSekCyclesTotal += nSekCyclesSegment;
				} else {
					CycloneRun(&c68k[nSekActive]);
					nSekCyclesDone += nSekCyclesToDo - c68k[nSekActive].cycles;
					nSekCyclesTotal += nSekCyclesToDo - c68k[nSekActive].cycles;
				}
			} while (nSekCyclesDone < nSekCyclesSegment);
			nSekCyclesSegment = nSekCyclesDone;
		}
		nSekCyclesToDo = c68k[nSekActive].cycles = 0; // was -1; changed june26, 2019 -dink
		nSekCyclesDone = 0;

		return nSekCyclesSegment;								// Return the number of cycles actually done
	} else {
#endif

#ifdef EMU_M68K
		nSekCyclesToDo = nCycles;

		if (nSekRESETLine[nSekActive] || nSekHALT[nSekActive])
		{
			nSekCyclesSegment = nCycles; // idle when RESET high or halted
		}
		else
		{
			nSekCyclesSegment = m68k_execute(nCycles);
		}

		nSekCyclesTotal += nSekCyclesSegment;
		nSekCyclesToDo = m68k_ICount = 0; // was -1; changed june26, 2019 -dink

		return nSekCyclesSegment;
#else
		return 0;
#endif

#ifdef EMU_C68K
	}
#endif

}

INT32 SekRun(INT32 nCPU, INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekRun called without init\n"));
#endif

	SekCPUPush(nCPU);

	INT32 nRet = SekRun(nCycles);

	SekCPUPop();

	return nRet;
}

INT32 SekIdle(INT32 nCPU, INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekIdle called without init\n"));
#endif

	SekCPUPush(nCPU);

	INT32 nRet = SekIdle(nCycles);

	SekCPUPop();

	return nRet;
}

// ----------------------------------------------------------------------------
// Breakpoint support

void SekDbgDisableBreakpoints()
{
}

// ----------------------------------------------------------------------------
// Memory map setup

void SekSetAddressMask(UINT32 nAddressMask)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekSetAddressMask called without init\n"));
	if (nSekActive == -1) { bprintf(PRINT_ERROR, _T("SekSetAddressMask called when no CPU open\n")); return; }
	if ((nAddressMask & 1) == 0) bprintf(PRINT_ERROR, _T("SekSetAddressMask called with invalid mask! (%x)\n"), nAddressMask);
#endif

	nSekAddressMask[nSekActive] = nSekAddressMaskActive = nAddressMask;
}

// Note - each page is 1 << SEK_BITS.
INT32 SekMapMemory(UINT8* pMemory, UINT32 nStart, UINT32 nEnd, INT32 nType)
{
	UINT8* Ptr = pMemory - nStart;
	UINT8** pMemMap = pSekExt->MemMap + (nStart >> SEK_SHIFT);

	// Special case for ROM banks
	if (nType == MAP_ROM) {
		for (UINT32 i = (nStart & ~SEK_PAGEM); i <= nEnd; i += SEK_PAGE_SIZE, pMemMap++) {
			pMemMap[0]			  = Ptr + i;
			pMemMap[SEK_WADD * 2] = Ptr + i;
		}

		return 0;
	}

	for (UINT32 i = (nStart & ~SEK_PAGEM); i <= nEnd; i += SEK_PAGE_SIZE, pMemMap++) {

		if (nType & MAP_READ) {					// Read
			pMemMap[0]			  = Ptr + i;
		}
		if (nType & MAP_WRITE) {					// Write
			pMemMap[SEK_WADD]	  = Ptr + i;
		}
		if (nType & MAP_FETCH) {					// Fetch
			pMemMap[SEK_WADD * 2] = Ptr + i;
		}
	}

	return 0;
}

INT32 SekMapHandler(uintptr_t nHandler, UINT32 nStart, UINT32 nEnd, INT32 nType)
{
	UINT8** pMemMap = pSekExt->MemMap + (nStart >> SEK_SHIFT);

	// Add to memory map
	for (UINT32 i = (nStart & ~SEK_PAGEM); i <= nEnd; i += SEK_PAGE_SIZE, pMemMap++) {

		if (nType & MAP_READ) {					// Read
			pMemMap[0]			  = (UINT8*)nHandler;
		}
		if (nType & MAP_WRITE) {					// Write
			pMemMap[SEK_WADD]	  = (UINT8*)nHandler;
		}
		if (nType & MAP_FETCH) {					// Fetch
			pMemMap[SEK_WADD * 2] = (UINT8*)nHandler;
		}
	}

	return 0;
}

// Set callbacks
INT32 SekSetResetCallback(pSekResetCallback pCallback)
{
	pSekExt->ResetCallback = pCallback;

	return 0;
}

INT32 SekSetRTECallback(pSekRTECallback pCallback)
{
	pSekExt->RTECallback = pCallback;

	return 0;
}

INT32 SekSetIrqCallback(pSekIrqCallback pCallback)
{
	pSekExt->IrqCallback = pCallback;

	return 0;
}

INT32 SekSetCmpCallback(pSekCmpCallback pCallback)
{
	pSekExt->CmpCallback = pCallback;

	return 0;
}

INT32 SekSetTASCallback(pSekTASCallback pCallback)
{
	pSekExt->TASCallback = pCallback;

	return 0;
}

// Set handlers
INT32 SekSetReadByteHandler(INT32 i, pSekReadByteHandler pHandler)
{
	if (i >= SEK_MAXHANDLER) {
		return 1;
	}

	pSekExt->ReadByte[i] = pHandler;

	return 0;
}

INT32 SekSetWriteByteHandler(INT32 i, pSekWriteByteHandler pHandler)
{
	if (i >= SEK_MAXHANDLER) {
		return 1;
	}

	pSekExt->WriteByte[i] = pHandler;

	return 0;
}

INT32 SekSetReadWordHandler(INT32 i, pSekReadWordHandler pHandler)
{
	if (i >= SEK_MAXHANDLER) {
		return 1;
	}

	pSekExt->ReadWord[i] = pHandler;

	return 0;
}

INT32 SekSetWriteWordHandler(INT32 i, pSekWriteWordHandler pHandler)
{
	if (i >= SEK_MAXHANDLER) {
		return 1;
	}

	pSekExt->WriteWord[i] = pHandler;

	return 0;
}

INT32 SekSetReadLongHandler(INT32 i, pSekReadLongHandler pHandler)
{
	if (i >= SEK_MAXHANDLER) {
		return 1;
	}

	pSekExt->ReadLong[i] = pHandler;

	return 0;
}

INT32 SekSetWriteLongHandler(INT32 i, pSekWriteLongHandler pHandler)
{
	if (i >= SEK_MAXHANDLER) {
		return 1;
	}

	pSekExt->WriteLong[i] = pHandler;

	return 0;
}

// ----------------------------------------------------------------------------
// Query register values

#ifdef EMU_C68K
UINT32 SekGetPC(INT32 n)
#else
UINT32 SekGetPC(INT32)
#endif
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetPC called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekGetPC called when no CPU open\n"));
#endif

#ifdef EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
		if (n < 0) {								// Currently active CPU
		  return c68k[nSekActive].pc-c68k[nSekActive].membase;
		} else {
			return c68k[n].pc-c68k[n].membase;					// Any CPU
		}
	} else {
#endif

#ifdef EMU_M68K
		return m68k_get_reg(NULL, M68K_REG_PC);
#else
		return 0;
#endif

#ifdef EMU_C68K
	}
#endif

}

UINT32 SekGetPPC(INT32)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekGetPC called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekGetPC called when no CPU open\n"));
#endif

#ifdef EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {
		return c68k[nSekActive].prev_pc;
	} else {
#endif

#ifdef EMU_M68K
		return m68k_get_reg(NULL, M68K_REG_PPC);
#else
		return 0;
#endif

#ifdef EMU_C68K
	}
#endif
}

INT32 SekDbgGetCPUType()
{
	switch (nSekCPUType[nSekActive]) {
		case 0:
		case 0x68000:
			return M68K_CPU_TYPE_68000;
		case 0x68010:
			return M68K_CPU_TYPE_68010;
		case 0x68EC020:
			return M68K_CPU_TYPE_68EC020;
	}

	return 0;
}

INT32 SekDbgGetPendingIRQ()
{
	return nSekIRQPending[nSekActive] & 7;
}

UINT32 SekDbgGetRegister(SekRegister nRegister)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_SekInitted) bprintf(PRINT_ERROR, _T("SekDbgGetRegister called without init\n"));
	if (nSekActive == -1) bprintf(PRINT_ERROR, _T("SekDbgGetRegister called when no CPU open\n"));
#endif

#if defined EMU_C68K
	if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[nSekActive] == 0x68000) {

	switch (nRegister) {
		case SEK_REG_A0:
			return c68k[nSekActive].a[0];
		case SEK_REG_A1:
			return c68k[nSekActive].a[1];
		case SEK_REG_A2:
			return c68k[nSekActive].a[2];
		case SEK_REG_A3:
			return c68k[nSekActive].a[3];
		case SEK_REG_A4:
			return c68k[nSekActive].a[4];
		case SEK_REG_A5:
			return c68k[nSekActive].a[5];
		case SEK_REG_A6:
			return c68k[nSekActive].a[6];
		case SEK_REG_A7:
			return c68k[nSekActive].a[7];

		case SEK_REG_PPC:
			return c68k[nSekActive].prev_pc;

		default:
			return 0;
	}
	}
#endif

	switch (nRegister) {
		case SEK_REG_D0:
			return m68k_get_reg(NULL, M68K_REG_D0);
		case SEK_REG_D1:
			return m68k_get_reg(NULL, M68K_REG_D1);
		case SEK_REG_D2:
			return m68k_get_reg(NULL, M68K_REG_D2);
		case SEK_REG_D3:
			return m68k_get_reg(NULL, M68K_REG_D3);
		case SEK_REG_D4:
			return m68k_get_reg(NULL, M68K_REG_D4);
		case SEK_REG_D5:
			return m68k_get_reg(NULL, M68K_REG_D5);
		case SEK_REG_D6:
			return m68k_get_reg(NULL, M68K_REG_D6);
		case SEK_REG_D7:
			return m68k_get_reg(NULL, M68K_REG_D7);

		case SEK_REG_A0:
			return m68k_get_reg(NULL, M68K_REG_A0);
		case SEK_REG_A1:
			return m68k_get_reg(NULL, M68K_REG_A1);
		case SEK_REG_A2:
			return m68k_get_reg(NULL, M68K_REG_A2);
		case SEK_REG_A3:
			return m68k_get_reg(NULL, M68K_REG_A3);
		case SEK_REG_A4:
			return m68k_get_reg(NULL, M68K_REG_A4);
		case SEK_REG_A5:
			return m68k_get_reg(NULL, M68K_REG_A5);
		case SEK_REG_A6:
			return m68k_get_reg(NULL, M68K_REG_A6);
		case SEK_REG_A7:
			return m68k_get_reg(NULL, M68K_REG_A7);

		case SEK_REG_PC:
			return m68k_get_reg(NULL, M68K_REG_PC);
		case SEK_REG_PPC:
			return m68k_get_reg(NULL, M68K_REG_PPC);

		case SEK_REG_SR:
			return m68k_get_reg(NULL, M68K_REG_SR);

		case SEK_REG_SP:
			return m68k_get_reg(NULL, M68K_REG_SP);
		case SEK_REG_USP:
			return m68k_get_reg(NULL, M68K_REG_USP);
		case SEK_REG_ISP:
			return m68k_get_reg(NULL, M68K_REG_ISP);
		case SEK_REG_MSP:
			return m68k_get_reg(NULL, M68K_REG_MSP);

		case SEK_REG_VBR:
			return m68k_get_reg(NULL, M68K_REG_VBR);

		case SEK_REG_SFC:
			return m68k_get_reg(NULL, M68K_REG_SFC);
		case SEK_REG_DFC:
			return m68k_get_reg(NULL, M68K_REG_DFC);

		case SEK_REG_CACR:
			return m68k_get_reg(NULL, M68K_REG_CACR);
		case SEK_REG_CAAR:
			return m68k_get_reg(NULL, M68K_REG_CAAR);

		default:
			return 0;
	}
}

bool SekDbgSetRegister(SekRegister nRegister, UINT32 nValue)
{
	return false;
}

// ----------------------------------------------------------------------------
// Savestate support

UINT8 cyclone_buffer[128];

INT32 SekScan(INT32 nAction)
{
	// Scan the 68000 states
	struct BurnArea ba;

	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return 1;
	}

	memset(&ba, 0, sizeof(ba));

	nSekActive = -1;

	for (INT32 i = 0; i <= nSekCount; i++) {

		char szName[] = "MC68000 #n";
		szName[9] = '0' + i;

		SCAN_VAR(nSekCPUType[i]);
		SCAN_VAR(nSekIRQPending[i]);
		SCAN_VAR(nSekCycles[i]);
		SCAN_VAR(nSekRESETLine[i]);
		SCAN_VAR(nSekHALT[i]);

#ifdef EMU_C68K
		if ((nSekCpuCore == SEK_CORE_C68K) && nSekCPUType[i] == 0x68000) {

			ba.nLen = 128;
			ba.szName = szName;

			if (nAction & ACB_READ) { // save
				memset(cyclone_buffer, 0, 128);
				CyclonePack(&c68k[i], cyclone_buffer);
				ba.Data = &cyclone_buffer;
				BurnAcb(&ba);
			} else if (nAction & ACB_WRITE) { // load
				memset(cyclone_buffer, 0, 128);
				ba.Data = &cyclone_buffer;
				BurnAcb(&ba);
				int sekActive = nSekActive; // CycloneUnpack needs m68k_checkpc which needs nSekActive set to current cpu
				nSekActive = i; // CycloneUnpack needs m68k_checkpc which needs nSekActive set to current cpu
				CycloneUnpack(&c68k[i], cyclone_buffer);
				nSekActive = sekActive;
			}
		} else {
#endif
#ifdef EMU_M68K
			if (nSekCPUType[i] != 0) {
				ba.Data = SekM68KContext[i];
				// for savestate portability: preserve our cpu's pointers, they are set up in DrvInit() and can be specific to different systems.
				// Therefore we scan the cpu context structure up until right before the pointers
				ba.nLen = m68k_context_size_no_pointers();
				ba.szName = szName;
				BurnAcb(&ba);
			}
#endif
#ifdef EMU_C68K
		}
#endif
	}

	return 0;
}
