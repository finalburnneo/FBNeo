// Z80 (Zed Eight-Ty) Interface
#include "burnint.h"
#include "z80_intf.h"
#include <stddef.h>

#define MAX_Z80		8
static struct ZetExt * ZetCPUContext[MAX_Z80] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
 
typedef UINT8 (__fastcall *pZetInHandler)(UINT16 a);
typedef void (__fastcall *pZetOutHandler)(UINT16 a, UINT8 d);
typedef UINT8 (__fastcall *pZetReadHandler)(UINT16 a);
typedef void (__fastcall *pZetWriteHandler)(UINT16 a, UINT8 d);
 
struct ZetExt {
	Z80_Regs reg;
	
	UINT8* pZetMemMap[0x100 * 4];

	pZetInHandler ZetIn;
	pZetOutHandler ZetOut;
	pZetReadHandler ZetRead;
	pZetWriteHandler ZetWrite;
	
	UINT32 BusReq;
	UINT32 ResetLine;
};
 
static INT32 nZetCyclesDone[MAX_Z80];
static INT32 nZetCyclesDelayed[MAX_Z80];
static INT32 nZetCyclesTotal;
static INT32 nZ80ICount[MAX_Z80];
static UINT32 Z80EA[MAX_Z80];

static INT32 nOpenedCPU = -1;
static INT32 nCPUCount = 0;
INT32 nHasZet = -1;

cpu_core_config ZetConfig =
{
	"Z80",
	ZetOpen,
	ZetClose,
	ZetCheatRead,
	ZetCheatWriteROM,
	ZetGetActive,
	ZetTotalCycles,
	ZetNewFrame,
	ZetIdle,
	ZetSetIRQLine,
	ZetRun,
	ZetRunEnd,
	ZetReset,
	0x10000,
	0
};

UINT8 __fastcall ZetDummyReadHandler(UINT16) { return 0; }
void __fastcall ZetDummyWriteHandler(UINT16, UINT8) { }
UINT8 __fastcall ZetDummyInHandler(UINT16) { return 0; }
void __fastcall ZetDummyOutHandler(UINT16, UINT8) { }

UINT8 __fastcall ZetReadIO(UINT32 a)
{
	return ZetCPUContext[nOpenedCPU]->ZetIn(a);
}

void __fastcall ZetWriteIO(UINT32 a, UINT8 d)
{
	ZetCPUContext[nOpenedCPU]->ZetOut(a, d);
}

UINT8 __fastcall ZetReadProg(UINT32 a)
{
	// check mem map
	UINT8 * pr = ZetCPUContext[nOpenedCPU]->pZetMemMap[0x000 | (a >> 8)];
	if (pr != NULL) {
		return pr[a & 0xff];
	}
	
	// check handler
	if (ZetCPUContext[nOpenedCPU]->ZetRead != NULL) {
		return ZetCPUContext[nOpenedCPU]->ZetRead(a);
	}
	
	return 0;
}

void __fastcall ZetWriteProg(UINT32 a, UINT8 d)
{
	// check mem map
	UINT8 * pr = ZetCPUContext[nOpenedCPU]->pZetMemMap[0x100 | (a >> 8)];
	if (pr != NULL) {
		pr[a & 0xff] = d;
		return;
	}
	
	// check handler
	if (ZetCPUContext[nOpenedCPU]->ZetWrite != NULL) {
		ZetCPUContext[nOpenedCPU]->ZetWrite(a, d);
		return;
	}
}

UINT8 __fastcall ZetReadOp(UINT32 a)
{
	// check mem map
	UINT8 * pr = ZetCPUContext[nOpenedCPU]->pZetMemMap[0x200 | (a >> 8)];
	if (pr != NULL) {
		return pr[a & 0xff];
	}
	
	// check read handler
	if (ZetCPUContext[nOpenedCPU]->ZetRead != NULL) {
		return ZetCPUContext[nOpenedCPU]->ZetRead(a);
	}
	
	return 0;
}

UINT8 __fastcall ZetReadOpArg(UINT32 a)
{
	// check mem map
	UINT8 * pr = ZetCPUContext[nOpenedCPU]->pZetMemMap[0x300 | (a >> 8)];
	if (pr != NULL) {
		return pr[a & 0xff];
	}
	
	// check read handler
	if (ZetCPUContext[nOpenedCPU]->ZetRead != NULL) {
		return ZetCPUContext[nOpenedCPU]->ZetRead(a);
	}
	
	return 0;
}

void ZetSetReadHandler(UINT8 (__fastcall *pHandler)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetReadHandler called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetReadHandler called when no CPU open\n"));
#endif

	ZetCPUContext[nOpenedCPU]->ZetRead = pHandler;
}

void ZetSetWriteHandler(void (__fastcall *pHandler)(UINT16, UINT8))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetWriteHandler called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetWriteHandler called when no CPU open\n"));
#endif

	ZetCPUContext[nOpenedCPU]->ZetWrite = pHandler;
}

void ZetSetInHandler(UINT8 (__fastcall *pHandler)(UINT16))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetInHandler called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetInHandler called when no CPU open\n"));
#endif

	ZetCPUContext[nOpenedCPU]->ZetIn = pHandler;
}

void ZetSetOutHandler(void (__fastcall *pHandler)(UINT16, UINT8))
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetOutHandler called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetOutHandler called when no CPU open\n"));
#endif

	ZetCPUContext[nOpenedCPU]->ZetOut = pHandler;
}

void ZetSetEDFECallback(void (*pCallback)(Z80_Regs*))
{
	// Can be set before init. it's cleared at exit.
	z80edfe_callback = pCallback;
}

void ZetNewFrame()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetNewFrame called without init\n"));
#endif

	for (INT32 i = 0; i < nCPUCount; i++) {
		nZetCyclesDone[i] = 0;
	}
	nZetCyclesTotal = 0;
}

void ZetCheatWriteROM(UINT32 a, UINT8 d)
{
	ZetWriteRom(a, d);
}

UINT8 ZetCheatRead(UINT32 a)
{
	return ZetReadByte(a);
}

INT32 ZetInit(INT32 nCPU)
{
	DebugCPU_ZetInitted = 1;

	nOpenedCPU = -1;

	ZetCPUContext[nCPU] = (struct ZetExt*)BurnMalloc(sizeof(ZetExt));
	memset (ZetCPUContext[nCPU], 0, sizeof(ZetExt));

    Z80Init(); // clear/init next z80 slot (internal to z80.cpp)

	{
		ZetCPUContext[nCPU]->ZetIn = ZetDummyInHandler;
		ZetCPUContext[nCPU]->ZetOut = ZetDummyOutHandler;
		ZetCPUContext[nCPU]->ZetRead = ZetDummyReadHandler;
		ZetCPUContext[nCPU]->ZetWrite = ZetDummyWriteHandler;
		ZetCPUContext[nCPU]->BusReq = 0;
		ZetCPUContext[nCPU]->ResetLine = 0;
		// Z80Init() will set IX IY F regs with default value, so get them ...
		Z80GetContext(&ZetCPUContext[nCPU]->reg);
		
		nZetCyclesDone[nCPU] = 0;
		nZetCyclesDelayed[nCPU] = 0;
		nZ80ICount[nCPU] = 0;
		
		for (INT32 j = 0; j < (0x0100 * 4); j++) {
			ZetCPUContext[nCPU]->pZetMemMap[j] = NULL;
		}
	}

	nZetCyclesTotal = 0;

	Z80SetIOReadHandler(ZetReadIO);
	Z80SetIOWriteHandler(ZetWriteIO);
	Z80SetProgramReadHandler(ZetReadProg);
	Z80SetProgramWriteHandler(ZetWriteProg);
	Z80SetCPUOpReadHandler(ZetReadOp);
	Z80SetCPUOpArgReadHandler(ZetReadOpArg);
	
	nCPUCount = (nCPU+1) % MAX_Z80;

	nHasZet = nCPU+1;

	CpuCheatRegister(nCPU, &ZetConfig);

	return 0;
}

UINT8 ZetReadByte(UINT16 address)
{
	if (nOpenedCPU < 0) return 0;

	return ZetReadProg(address);
}

void ZetWriteByte(UINT16 address, UINT8 data)
{
	if (nOpenedCPU < 0) return;

	ZetWriteProg(address, data);
}

void ZetWriteRom(UINT16 address, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetWriteRom called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetWriteRom called when no CPU open\n"));
#endif

	if (nOpenedCPU < 0) return;

	if (ZetCPUContext[nOpenedCPU]->pZetMemMap[0x200 | (address >> 8)] != NULL) {
		ZetCPUContext[nOpenedCPU]->pZetMemMap[0x200 | (address >> 8)][address & 0xff] = data;
	}
	
	if (ZetCPUContext[nOpenedCPU]->pZetMemMap[0x300 | (address >> 8)] != NULL) {
		ZetCPUContext[nOpenedCPU]->pZetMemMap[0x300 | (address >> 8)][address & 0xff] = data;
	}
	
	ZetWriteProg(address, data);
}

void ZetClose()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetClose called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetClose called when no CPU open\n"));
#endif

	Z80GetContext(&ZetCPUContext[nOpenedCPU]->reg);
	nZetCyclesDone[nOpenedCPU] = nZetCyclesTotal;
	nZ80ICount[nOpenedCPU] = z80_ICount;
	Z80EA[nOpenedCPU] = EA;

	nOpenedCPU = -1;
}

void ZetOpen(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetOpen called without init\n"));
	if (nCPU >= nCPUCount) bprintf(PRINT_ERROR, _T("ZetOpen called with invalid index %x\n"), nCPU);
	if (nOpenedCPU != -1) bprintf(PRINT_ERROR, _T("ZetOpen called when CPU already open with index %x\n"), nCPU);
	if (ZetCPUContext[nCPU] == NULL) bprintf (PRINT_ERROR, _T("ZetOpen called for uninitialized cpu %x\n"), nCPU);
#endif

	Z80SetContext(&ZetCPUContext[nCPU]->reg);
	nZetCyclesTotal = nZetCyclesDone[nCPU];
	z80_ICount = nZ80ICount[nCPU];
	EA = Z80EA[nCPU];

	nOpenedCPU = nCPU;
}

void ZetSwapActive(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSwapActive called without init\n"));
	if (nCPU >= nCPUCount) bprintf(PRINT_ERROR, _T("ZetSwapActive called with invalid index %x\n"), nCPU);
#endif

	if (ZetGetActive() != -1)
		ZetClose();

	ZetOpen(nCPU);
}

INT32 ZetGetActive()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetGetActive called without init\n"));
	//if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetGetActive called when no CPU open\n"));
#endif

	return nOpenedCPU;
}

// ## ZetCPUPush() / ZetCPUPop() ## internal helpers for sending signals to other 68k's
struct z80pstack {
	INT32 nHostCPU;
	INT32 nPushedCPU;
};
#define MAX_PSTACK 10

static z80pstack pstack[MAX_PSTACK];
static INT32 pstacknum = 0;

static void ZetCPUPush(INT32 nCPU)
{
	z80pstack *p = &pstack[pstacknum++];

	if (pstacknum + 1 >= MAX_PSTACK) {
		bprintf(0, _T("ZetCPUPush(): out of stack!  Possible infinite recursion?  Crash pending..\n"));
	}

	p->nPushedCPU = nCPU;

	p->nHostCPU = ZetGetActive();

	if (p->nHostCPU != p->nPushedCPU) {
		if (p->nHostCPU != -1) ZetClose();
		ZetOpen(p->nPushedCPU);
	}
}

static void ZetCPUPop()
{
	z80pstack *p = &pstack[--pstacknum];

	if (p->nHostCPU != p->nPushedCPU) {
		ZetClose();
		if (p->nHostCPU != -1) ZetOpen(p->nHostCPU);
	}
}

INT32 ZetRun(INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetRun called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetRun called when no CPU open\n"));
#endif

	if (nCycles <= 0) return 0;

	INT32 nDelayed = 0;  // handle delayed cycle counts (from nmi / irq)
	if (nZetCyclesDelayed[nOpenedCPU]) {
		nDelayed = nZetCyclesDelayed[nOpenedCPU];
		nZetCyclesDelayed[nOpenedCPU] = 0;
		nCycles -= nDelayed;
	}

	if (!ZetCPUContext[nOpenedCPU]->BusReq && !ZetCPUContext[nOpenedCPU]->ResetLine) {
		nCycles = Z80Execute(nCycles);
	}

	nCycles += nDelayed;

	nZetCyclesTotal += nCycles;
	
	return nCycles;
}

INT32 ZetRun(INT32 nCPU, INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetRun called without init\n"));
#endif

	ZetCPUPush(nCPU);

	INT32 nRet = ZetRun(nCycles);

	ZetCPUPop();

	return nRet;
}

void ZetRunAdjust(INT32 /*nCycles*/)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetRunAdjust called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetRunAdjust called when no CPU open\n"));
#endif
}

void ZetRunEnd()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetRunEnd called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetRunEnd called when no CPU open\n"));
#endif

	Z80StopExecute();
}

// This function will make an area callback ZetRead/ZetWrite
INT32 ZetMemCallback(INT32 nStart, INT32 nEnd, INT32 nMode)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetMemCallback called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetMemCallback called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU]->pZetMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		switch (nMode) {
			case 0:
				pMemMap[0     + i] = NULL;
				break;
			case 1:
				pMemMap[0x100 + i] = NULL;
				break;
			case 2:
				pMemMap[0x200 + i] = NULL;
				pMemMap[0x300 + i] = NULL;
				break;
		}
	}

	return 0;
}

void ZetExit()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetExit called without init\n"));
#endif

	if (!DebugCPU_ZetInitted) return;

    for (INT32 i = 0; i < nCPUCount; i++) {
        ZetOpen(i);
        Z80Exit(); // exit daisy chain & peripherals attached to this cpu.
        ZetClose();
    }

	for (INT32 i = 0; i < MAX_Z80; i++) {
		if (ZetCPUContext[i]) {
			BurnFree (ZetCPUContext[i]);
			ZetCPUContext[i] = NULL;
		}
	}

	nCPUCount = 0;
	nHasZet = -1;
	
	DebugCPU_ZetInitted = 0;
}


// This function will make an area callback ZetRead/ZetWrite
INT32 ZetUnmapMemory(INT32 nStart, INT32 nEnd, INT32 nFlags)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetUnmapMemory called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetUnmapMemory called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU]->pZetMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		if (nFlags & (1 << 0)) pMemMap[0     + i] = NULL; // READ
		if (nFlags & (1 << 1)) pMemMap[0x100 + i] = NULL; // WRITE
		if (nFlags & (1 << 2)) pMemMap[0x200 + i] = NULL; // OP
		if (nFlags & (1 << 3)) pMemMap[0x300 + i] = NULL; // ARG
	}

	return 0;
}

void ZetMapMemory(UINT8 *Mem, INT32 nStart, INT32 nEnd, INT32 nFlags)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetMapMemory called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetMapMemory called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU]->pZetMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		if (nFlags & (1 << 0)) pMemMap[0     + i] = Mem + ((i - cStart) << 8); // READ
		if (nFlags & (1 << 1)) pMemMap[0x100 + i] = Mem + ((i - cStart) << 8); // WRITE
		if (nFlags & (1 << 2)) pMemMap[0x200 + i] = Mem + ((i - cStart) << 8); // OP
		if (nFlags & (1 << 3)) pMemMap[0x300 + i] = Mem + ((i - cStart) << 8); // ARG
	}
}

INT32 ZetMapArea(INT32 nStart, INT32 nEnd, INT32 nMode, UINT8 *Mem)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetMapArea called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetMapArea called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU]->pZetMemMap;

	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		switch (nMode) {
			case 0: {
				pMemMap[0     + i] = Mem + ((i - cStart) << 8);
				break;
			}
		
			case 1: {
				pMemMap[0x100 + i] = Mem + ((i - cStart) << 8);
				break;
			}
			
			case 2: {
				pMemMap[0x200 + i] = Mem + ((i - cStart) << 8);
				pMemMap[0x300 + i] = Mem + ((i - cStart) << 8);
				break;
			}
		}
	}

	return 0;
}

INT32 ZetMapArea(INT32 nStart, INT32 nEnd, INT32 nMode, UINT8 *Mem01, UINT8 *Mem02)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetMapArea called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetMapArea called when no CPU open\n"));
#endif

	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU]->pZetMemMap;
	
	if (nMode != 2) {
		return 1;
	}
	
	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		pMemMap[0x200 + i] = Mem01 + ((i - cStart) << 8);
		pMemMap[0x300 + i] = Mem02 + ((i - cStart) << 8);
	}

	return 0;
}

void ZetReset()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetReset called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetReset called when no CPU open\n"));
#endif

	nZetCyclesDelayed[nOpenedCPU] = 0;
	Z80Reset();
}

void ZetReset(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetReset called without init\n"));
#endif

	ZetCPUPush(nCPU);

	ZetReset();

	ZetCPUPop();
}

UINT32 ZetGetPC(INT32 n)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetGetPC called without init\n"));
	if (nOpenedCPU == -1 && n < 0) bprintf(PRINT_ERROR, _T("ZetGetPC called when no CPU open\n"));
#endif

	if (n < 0) {
		return ActiveZ80GetPC();
	} else {
		return ZetCPUContext[n]->reg.pc.w.l;
	}
}

INT32 ZetGetPrevPC(INT32 n)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetGetPrvPC called without init\n"));
	if (nOpenedCPU == -1 && n < 0) bprintf(PRINT_ERROR, _T("ZetGetPrevPC called when no CPU open\n"));
#endif

	if (n < 0) {
		return ActiveZ80GetPrevPC();
	} else {
		return ZetCPUContext[n]->reg.prvpc.d;
	}
}

INT32 ZetBc(INT32 n)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetBc called without init\n"));
	if (nOpenedCPU == -1 && n < 0) bprintf(PRINT_ERROR, _T("ZetBc called when no CPU open\n"));
#endif

	if (n < 0) {
		return ActiveZ80GetBC();
	} else {
		return ZetCPUContext[n]->reg.bc.w.l;
	}
}

INT32 ZetDe(INT32 n)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetDe called without init\n"));
	if (nOpenedCPU == -1 && n < 0) bprintf(PRINT_ERROR, _T("ZetDe called when no CPU open\n"));
#endif

	if (n < 0) {
		return ActiveZ80GetDE();
	} else {
		return ZetCPUContext[n]->reg.de.w.l;
	}
}

INT32 ZetHL(INT32 n)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetHL called without init\n"));
	if (nOpenedCPU == -1 && n < 0) bprintf(PRINT_ERROR, _T("ZetHL called when no CPU open\n"));
#endif

	if (n < 0) {
		return ActiveZ80GetHL();
	} else {
		return ZetCPUContext[n]->reg.hl.w.l;
	}
}

INT32 ZetI(INT32 n)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetI called without init\n"));
	if (nOpenedCPU == -1 && n < 0) bprintf(PRINT_ERROR, _T("ZetI called when no CPU open\n"));
#endif

	if (n < 0) {
		return ActiveZ80GetI();
	} else {
		return ZetCPUContext[n]->reg.i;
	}
}

INT32 ZetSP(INT32 n)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSP called without init\n"));
	if (nOpenedCPU == -1 && n < 0) bprintf(PRINT_ERROR, _T("ZetSP called when no CPU open\n"));
#endif

	if (n < 0) {
		return ActiveZ80GetSP();
	} else {
		return ZetCPUContext[n]->reg.sp.w.l;
	}
}

INT32 ZetScan(INT32 nAction)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetScan called without init\n"));
#endif

	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return 0;
	}

	char szText[] = "Z80 #0";
	
	for (INT32 i = 0; i < nCPUCount; i++) {
		szText[5] = '1' + i;

		ScanVar(&ZetCPUContext[i]->reg, STRUCT_SIZE_HELPER(Z80_Regs, hold_irq), szText);
		SCAN_VAR(Z80EA[i]);
		SCAN_VAR(nZ80ICount[i]);
		SCAN_VAR(nZetCyclesDone[i]);
		SCAN_VAR(nZetCyclesDelayed[i]);
		SCAN_VAR(ZetCPUContext[i]->BusReq);
		SCAN_VAR(ZetCPUContext[i]->ResetLine);
	}
	
	SCAN_VAR(nZetCyclesTotal);

    for (INT32 i = 0; i < nCPUCount; i++) {
        ZetOpen(i);
        Z80Scan(nAction); // scan daisy chain & peripherals attached to this cpu.
        ZetClose();
    }

	return 0;
}

void ZetDaisyInit(INT32 dev0, INT32 dev1)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetDaisyInit called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetDaisyInit called when no CPU open\n"));
#endif

    z80daisy_init(dev0, dev1);
}

void ZetSetIRQLine(const INT32 line, const INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetIRQLine called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetIRQLine called when no CPU open\n"));
#endif

	switch ( status ) {
		case CPU_IRQSTATUS_NONE:
			Z80SetIrqLine(line, 0);
			break;
		case CPU_IRQSTATUS_ACK:
			Z80SetIrqLine(line, 1);
			break;
		case CPU_IRQSTATUS_AUTO:
			Z80SetIrqLine(line, 1);
			nZetCyclesDelayed[nOpenedCPU] += Z80Execute(0);
			Z80SetIrqLine(0, 0);
			nZetCyclesDelayed[nOpenedCPU] += Z80Execute(0);
			break;
		case CPU_IRQSTATUS_HOLD:
			ActiveZ80SetIRQHold();
			Z80SetIrqLine(line, 1);
			break;
	}
}

void ZetSetIRQLine(INT32 nCPU, const INT32 line, const INT32 status)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetIRQLine called without init\n"));
#endif

	ZetCPUPush(nCPU);

	ZetSetIRQLine(line, status);

	ZetCPUPop();
}

void ZetSetVector(INT32 nCPU, INT32 vector)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetVector called without init\n"));
#endif

	ZetCPUPush(nCPU);

	ZetSetVector(vector);

	ZetCPUPop();
}

void ZetSetVector(INT32 vector)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetVector called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetVector called when no CPU open\n"));
#endif

	ActiveZ80SetVector(vector);
}

UINT8 ZetGetVector()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetGetVector called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetGetVector called when no CPU open\n"));
#endif

	return ActiveZ80GetVector();
}

UINT8 ZetGetVector(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetGetVector called without init\n"));
#endif

	ZetCPUPush(nCPU);

	INT32 nRet = ZetGetVector();

	ZetCPUPop();

	return nRet;
}

INT32 ZetNmi()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetNmi called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetNmi called when no CPU open\n"));
#endif

	Z80SetIrqLine(Z80_INPUT_LINE_NMI, 1);
	nZetCyclesDelayed[nOpenedCPU] += Z80Execute(0);
	Z80SetIrqLine(Z80_INPUT_LINE_NMI, 0);
	nZetCyclesDelayed[nOpenedCPU] += Z80Execute(0);

	return 0;
}

INT32 ZetNmi(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetNmi called without init\n"));
#endif

	ZetCPUPush(nCPU);

	ZetNmi();

	ZetCPUPop();

	return 0;
}

INT32 ZetIdle(INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetIdle called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetIdle called when no CPU open\n"));
#endif

	nZetCyclesTotal += nCycles;

	return nCycles;
}

INT32 ZetIdle(INT32 nCPU, INT32 nCycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetIdle called without init\n"));
#endif

	ZetCPUPush(nCPU);

	INT32 nRet = ZetIdle(nCycles);

	ZetCPUPop();

	return nRet;
}

INT32 ZetSegmentCycles()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSegmentCycles called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSegmentCycles called when no CPU open\n"));
#endif

	return z80TotalCycles();
}

INT32 ZetTotalCycles()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetTotalCycles called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetTotalCycles called when no CPU open\n"));
#endif

	return nZetCyclesTotal + z80TotalCycles();
}

INT32 ZetTotalCycles(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetTotalCycles called without init\n"));
#endif

	ZetCPUPush(nCPU);

	INT32 nRet = ZetTotalCycles();

	ZetCPUPop();

	return nRet;
}

void ZetSetHALT(INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetHALT called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetHALT called when no CPU open\n"));
#endif

	if (nOpenedCPU < 0) return;
	
	ZetCPUContext[nOpenedCPU]->BusReq = nStatus;
}

void ZetSetHALT(INT32 nCPU, INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetHALT called without init\n"));
#endif

	ZetCPUPush(nCPU);

	ZetSetHALT(nStatus);

	ZetCPUPop();
}

INT32 ZetGetHALT()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetGetHALT called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetGetHALT called when no CPU open\n"));
#endif

	return ZetCPUContext[nOpenedCPU]->BusReq;
}

INT32 ZetGetHALT(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetGetHALT called without init\n"));
#endif

	ZetCPUPush(nCPU);

	INT32 nRet = ZetGetHALT();

	ZetCPUPop();

	return nRet;
}

void ZetSetRESETLine(INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetRESETLine called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetSetRESETLine called when no CPU open\n"));
#endif

	if (nOpenedCPU < 0) return;

	if (ZetCPUContext[nOpenedCPU]->ResetLine && nStatus == 0) {
		ZetReset();
	}

	ZetCPUContext[nOpenedCPU]->ResetLine = nStatus;
}

void ZetSetRESETLine(INT32 nCPU, INT32 nStatus)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetSetRESETLine called without init\n"));
#endif

	ZetCPUPush(nCPU);

	ZetSetRESETLine(nStatus);

	ZetCPUPop();
}

INT32 ZetGetRESETLine()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetGetRESETLine called without init\n"));
	if (nOpenedCPU == -1) bprintf(PRINT_ERROR, _T("ZetGetRESETLine called when no CPU open\n"));
#endif

	return ZetCPUContext[nOpenedCPU]->ResetLine;
}

INT32 ZetGetRESETLine(INT32 nCPU)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ZetInitted) bprintf(PRINT_ERROR, _T("ZetGetRESETLine called without init\n"));
#endif

	ZetCPUPush(nCPU);

	INT32 nRet = ZetGetRESETLine();

	ZetCPUPop();

	return nRet;
}

void ZetSetAF(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.af.w.l = value;
}

void ZetSetAF2(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.af2.w.l = value;
}

void ZetSetBC(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.bc.w.l = value;
}

void ZetSetBC2(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.bc2.w.l = value;
}

void ZetSetDE(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.de.w.l = value;
}

void ZetSetDE2(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.de2.w.l = value;
}

void ZetSetHL(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.hl.w.l = value;
}

void ZetSetHL2(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.hl2.w.l = value;
}

void ZetSetI(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.i = value;
}

void ZetSetIFF1(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.iff1 = value;
}

void ZetSetIFF2(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.iff2 = value;
}

void ZetSetIM(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.im = value;
}

void ZetSetIX(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.ix.w.l = value;
}

void ZetSetIY(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.iy.w.l = value;
}

void ZetSetPC(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.pc.w.l = value;
}

void ZetSetR(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.r = value;
	ZetCPUContext[n]->reg.r2 = value & 0x80;
}

void ZetSetSP(INT32 n, UINT16 value)
{
	ZetCPUContext[n]->reg.sp.w.l = value;
}

#undef MAX_Z80
