// Zilog Z8002 (non-segmented Z8000) for FBNeo

#ifndef Z8000_INTF_H
#define Z8000_INTF_H

// input lines
#define Z8002_INPUT_LINE_NVI	0
#define Z8002_INPUT_LINE_VI		1
#define Z8002_INPUT_LINE_SEGT	2
#define Z8002_INPUT_LINE_NMI	CPU_IRQLINE_NMI

void Z8002Init(INT32 nCpu);
void Z8002Exit();
void Z8002Open(INT32 nCpu);
void Z8002Close();
INT32 Z8002GetActive();
void Z8002Reset();
INT32 Z8002Run(INT32 nCycles);
void Z8002RunEnd();
INT32 Z8002Idle(INT32 nCycles);
INT32 Z8002TotalCycles();
void Z8002NewFrame();
INT32 Z8002Scan(INT32 nAction);

void Z8002SetIRQLine(INT32 line, INT32 state);
void Z8002SetRESETLine(INT32 state);
INT32 Z8002GetRESETLine();
UINT32 Z8002GetPC();

void Z8002MapMemory(UINT8 *ptr, UINT32 start, UINT32 end, INT32 flags);
void Z8002SetReadWordHandler(UINT16 (*handler)(UINT32 address));
void Z8002SetWriteWordHandler(void (*handler)(UINT32 address, UINT16 data));
void Z8002SetReadByteHandler(UINT8 (*handler)(UINT32 address));		// optional, derived from word handler if missing
void Z8002SetWriteByteHandler(void (*handler)(UINT32 address, UINT8 data));
void Z8002SetReadPortHandler(UINT16 (*handler)(UINT16 port));
void Z8002SetWritePortHandler(void (*handler)(UINT16 port, UINT16 data));
void Z8002SetIRQAckCallback(UINT16 (*handler)(INT32 line));			// returns the vector / identifier word

UINT16 Z8002ReadWord(UINT32 address);
void Z8002WriteWord(UINT32 address, UINT16 data);
UINT8 Z8002ReadByte(UINT32 address);
void Z8002WriteByte(UINT32 address, UINT8 data);

extern struct cpu_core_config Z8002Config;

#define BurnTimerAttachZ8002(clock)	\
	BurnTimerAttach(&Z8002Config, clock)

#endif
