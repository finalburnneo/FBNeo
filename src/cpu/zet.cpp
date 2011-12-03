// Z80 (Zed Eight-Ty) Interface
#include "burnint.h"

#define MAX_Z80		8
static struct ZetExt * ZetCPUContext = NULL;
 
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
	
	UINT8 BusReq;
};
 
static INT32 nZetCyclesDone[MAX_Z80];
static INT32 nZetCyclesTotal;
static INT32 nZ80ICount[MAX_Z80];
static UINT32 Z80EA[MAX_Z80];

static INT32 nOpenedCPU = -1;
static INT32 nCPUCount = 0;
INT32 nHasZet = -1;

UINT8 __fastcall ZetDummyReadHandler(UINT16) { return 0; }
void __fastcall ZetDummyWriteHandler(UINT16, UINT8) { }
UINT8 __fastcall ZetDummyInHandler(UINT16) { return 0; }
void __fastcall ZetDummyOutHandler(UINT16, UINT8) { }

UINT8 __fastcall ZetReadIO(UINT32 a)
{
	return ZetCPUContext[nOpenedCPU].ZetIn(a);
}

void __fastcall ZetWriteIO(UINT32 a, UINT8 d)
{
	ZetCPUContext[nOpenedCPU].ZetOut(a, d);
}

UINT8 __fastcall ZetReadProg(UINT32 a)
{
	// check mem map
	UINT8 * pr = ZetCPUContext[nOpenedCPU].pZetMemMap[0x000 | (a >> 8)];
	if (pr != NULL) {
		return pr[a & 0xff];
	}
	
	// check handler
	if (ZetCPUContext[nOpenedCPU].ZetRead != NULL) {
		return ZetCPUContext[nOpenedCPU].ZetRead(a);
	}
	
	return 0;
}

void __fastcall ZetWriteProg(UINT32 a, UINT8 d)
{
	// check mem map
	UINT8 * pr = ZetCPUContext[nOpenedCPU].pZetMemMap[0x100 | (a >> 8)];
	if (pr != NULL) {
		pr[a & 0xff] = d;
		return;
	}
	
	// check handler
	if (ZetCPUContext[nOpenedCPU].ZetWrite != NULL) {
		ZetCPUContext[nOpenedCPU].ZetWrite(a, d);
		return;
	}
}

UINT8 __fastcall ZetReadOp(UINT32 a)
{
	// check mem map
	UINT8 * pr = ZetCPUContext[nOpenedCPU].pZetMemMap[0x200 | (a >> 8)];
	if (pr != NULL) {
		return pr[a & 0xff];
	}
	
	// check read handler
	if (ZetCPUContext[nOpenedCPU].ZetRead != NULL) {
		return ZetCPUContext[nOpenedCPU].ZetRead(a);
	}
	
	return 0;
}

UINT8 __fastcall ZetReadOpArg(UINT32 a)
{
	// check mem map
	UINT8 * pr = ZetCPUContext[nOpenedCPU].pZetMemMap[0x300 | (a >> 8)];
	if (pr != NULL) {
		return pr[a & 0xff];
	}
	
	// check read handler
	if (ZetCPUContext[nOpenedCPU].ZetRead != NULL) {
		return ZetCPUContext[nOpenedCPU].ZetRead(a);
	}
	
	return 0;
}

void ZetSetReadHandler(UINT8 (__fastcall *pHandler)(UINT16))
{
	ZetCPUContext[nOpenedCPU].ZetRead = pHandler;
}

void ZetSetWriteHandler(void (__fastcall *pHandler)(UINT16, UINT8))
{
	ZetCPUContext[nOpenedCPU].ZetWrite = pHandler;
}

void ZetSetInHandler(UINT8 (__fastcall *pHandler)(UINT16))
{
	ZetCPUContext[nOpenedCPU].ZetIn = pHandler;
}

void ZetSetOutHandler(void (__fastcall *pHandler)(UINT16, UINT8))
{
	ZetCPUContext[nOpenedCPU].ZetOut = pHandler;
}

void ZetNewFrame()
{
	for (INT32 i = 0; i < nCPUCount; i++) {
		nZetCyclesDone[i] = 0;
	}
	nZetCyclesTotal = 0;
}

INT32 ZetInit(INT32 nCount)
{
	nOpenedCPU = -1;
	
	ZetCPUContext = (struct ZetExt *) malloc(nCount * sizeof(ZetExt));
	if (ZetCPUContext == NULL) return 1;
	memset(ZetCPUContext, 0, nCount * sizeof(ZetExt));
	
	Z80Init();
	
	for (INT32 i = 0; i < nCount; i++) {
		ZetCPUContext[i].ZetIn = ZetDummyInHandler;
		ZetCPUContext[i].ZetOut = ZetDummyOutHandler;
		ZetCPUContext[i].ZetRead = ZetDummyReadHandler;
		ZetCPUContext[i].ZetWrite = ZetDummyWriteHandler;
		ZetCPUContext[i].BusReq = 0;
		// TODO: Z80Init() will set IX IY F regs with default value, so get them ...
		Z80GetContext(&ZetCPUContext[i].reg);
		
		nZetCyclesDone[i] = 0;
		nZ80ICount[i] = 0;
		
		for (INT32 j = 0; j < (0x0100 * 4); j++) {
			ZetCPUContext[i].pZetMemMap[j] = NULL;
		}
	}
	
	nZetCyclesTotal = 0;
	
	Z80SetIOReadHandler(ZetReadIO);
	Z80SetIOWriteHandler(ZetWriteIO);
	Z80SetProgramReadHandler(ZetReadProg);
	Z80SetProgramWriteHandler(ZetWriteProg);
	Z80SetCPUOpReadHandler(ZetReadOp);
	Z80SetCPUOpArgReadHandler(ZetReadOpArg);
	
	ZetOpen(0);
	
	nCPUCount = nCount % MAX_Z80;

	nHasZet = nCount;

	for (INT32 i = 0; i < nCount; i++)
		CpuCheatRegister(0x0004, i);

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
	if (nOpenedCPU < 0) return;

	if (ZetCPUContext[nOpenedCPU].pZetMemMap[0x200 | (address >> 8)] != NULL) {
		ZetCPUContext[nOpenedCPU].pZetMemMap[0x200 | (address >> 8)][address] = data;
	}
	
	if (ZetCPUContext[nOpenedCPU].pZetMemMap[0x300 | (address >> 8)] != NULL) {
		ZetCPUContext[nOpenedCPU].pZetMemMap[0x300 | (address >> 8)][address] = data;
	}
	
	ZetWriteProg(address, data);
}

void ZetClose()
{
	Z80GetContext(&ZetCPUContext[nOpenedCPU].reg);
	nZetCyclesDone[nOpenedCPU] = nZetCyclesTotal;
	nZ80ICount[nOpenedCPU] = z80_ICount;
	Z80EA[nOpenedCPU] = EA;

	nOpenedCPU = -1;
}

void ZetOpen(INT32 nCPU)
{
	Z80SetContext(&ZetCPUContext[nCPU].reg);
	nZetCyclesTotal = nZetCyclesDone[nCPU];
	z80_ICount = nZ80ICount[nCPU];
	EA = Z80EA[nCPU];

	nOpenedCPU = nCPU;
}

INT32 ZetGetActive()
{
	return nOpenedCPU;
}

INT32 ZetRun(INT32 nCycles)
{
	if (nCycles <= 0) return 0;
	
	if (ZetCPUContext[nOpenedCPU].BusReq) {
		nZetCyclesTotal += nCycles;
		return nCycles;
	}
	
	nCycles = Z80Execute(nCycles);
	
	nZetCyclesTotal += nCycles;
	
	return nCycles;
}

void ZetRunAdjust(INT32 /*nCycles*/)
{
}

void ZetRunEnd()
{
}

// This function will make an area callback ZetRead/ZetWrite
INT32 ZetMemCallback(INT32 nStart, INT32 nEnd, INT32 nMode)
{
	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU].pZetMemMap;

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

INT32 ZetMemEnd()
{
	return 0;
}

void ZetExit()
{
	Z80Exit();
	if (ZetCPUContext) {
		free(ZetCPUContext);
		ZetCPUContext = NULL;
	}

	nCPUCount = 0;
	nHasZet = -1;
}


INT32 ZetMapArea(INT32 nStart, INT32 nEnd, INT32 nMode, UINT8 *Mem)
{
	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU].pZetMemMap;

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
	UINT8 cStart = (nStart >> 8);
	UINT8 **pMemMap = ZetCPUContext[nOpenedCPU].pZetMemMap;
	
	if (nMode != 2) {
		return 1;
	}
	
	for (UINT16 i = cStart; i <= (nEnd >> 8); i++) {
		pMemMap[0x200 + i] = Mem01 + ((i - cStart) << 8);
		pMemMap[0x300 + i] = Mem02 + ((i - cStart) << 8);
	}

	return 0;
}

INT32 ZetReset()
{
	Z80Reset();

	return 0;
}

INT32 ZetPc(INT32 n)
{
	if (n < 0) {
		return ActiveZ80GetPC();
	} else {
		return ZetCPUContext[n].reg.pc.w.l;
	}
}

INT32 ZetBc(INT32 n)
{
	if (n < 0) {
		return ActiveZ80GetBC();
	} else {
		return ZetCPUContext[n].reg.bc.w.l;
	}
}

INT32 ZetDe(INT32 n)
{
	if (n < 0) {
		return ActiveZ80GetDE();
	} else {
		return ZetCPUContext[n].reg.de.w.l;
	}
}

INT32 ZetHL(INT32 n)
{
	if (n < 0) {
		return ActiveZ80GetHL();
	} else {
		return ZetCPUContext[n].reg.hl.w.l;
	}
}

INT32 ZetScan(INT32 nAction)
{
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return 0;
	}

	char szText[] = "Z80 #0";
	
	for (INT32 i = 0; i < nCPUCount; i++) {
		szText[5] = '1' + i;

		ScanVar(&ZetCPUContext[i].reg, sizeof(Z80_Regs), szText);
		SCAN_VAR(Z80EA[i]);
		SCAN_VAR(nZ80ICount[i]);
		SCAN_VAR(nZetCyclesDone[i]);
	}
	
	SCAN_VAR(nZetCyclesTotal);	

	return 0;
}

void ZetSetIRQLine(const INT32 line, const INT32 status)
{
	switch ( status ) {
		case ZET_IRQSTATUS_NONE:
			Z80SetIrqLine(0, 0);
			break;
		case ZET_IRQSTATUS_ACK: 	
			Z80SetIrqLine(line, 1);
			break;
		case ZET_IRQSTATUS_AUTO:
			Z80SetIrqLine(line, 1);
			Z80Execute(0);
			Z80SetIrqLine(0, 0);
			Z80Execute(0);
			break;
	}
}

void ZetSetVector(INT32 vector)
{
	Z80Vector = vector;
}

INT32 ZetNmi()
{
	Z80SetIrqLine(Z80_INPUT_LINE_NMI, 1);
	Z80Execute(0);
	Z80SetIrqLine(Z80_INPUT_LINE_NMI, 0);
	Z80Execute(0);
	INT32 nCycles = 12;
	nZetCyclesTotal += nCycles;

	return nCycles;
}

INT32 ZetIdle(INT32 nCycles)
{
	nZetCyclesTotal += nCycles;

	return nCycles;
}

INT32 ZetSegmentCycles()
{
	return 0;
}

INT32 ZetTotalCycles()
{
	return nZetCyclesTotal;
}

void ZetSetBUSREQLine(INT32 nStatus)
{
	if (nOpenedCPU < 0) return;
	
	ZetCPUContext[nOpenedCPU].BusReq = nStatus;
}

#undef MAX_Z80
