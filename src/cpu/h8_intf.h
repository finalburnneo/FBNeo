// FinalBurn Neo - Hitachi H8/3002 interface

#ifndef _H8_INTF_H_
#define _H8_INTF_H_

#define H8_READ		1
#define H8_WRITE	2
#define H8_FETCH	4
#define H8_ROM		(H8_READ | H8_FETCH)
#define H8_RAM		(H8_READ | H8_WRITE | H8_FETCH)

// external interrupt lines
#define H8_IRQ0		0
#define H8_IRQ1		1
#define H8_IRQ2		2
#define H8_IRQ3		3
#define H8_IRQ4		4
#define H8_IRQ5		5
#define H8_NMI		0x20

// I/O ports
#define H8_PORT_4	4
#define H8_PORT_6	6
#define H8_PORT_7	7
#define H8_PORT_8	8
#define H8_PORT_9	9
#define H8_PORT_A	10
#define H8_PORT_B	11

void   H8Init(INT32 nCpu);
void   H8Exit();
void   H8Open(INT32 nCpu);
void   H8Close();
void   H8Reset();
INT32  H8Run(INT32 nCycles);
void   H8RunEnd();
INT32  H8Idle(INT32 nCycles);
INT32  H8TotalCycles();
void   H8NewFrame();
INT32  H8Scan(INT32 nAction);
INT32  H8GetActive();

// memory: pMem is in big-endian (natural) byte order, 64KB granularity
void   H8MapMemory(UINT8 *pMem, UINT32 nStart, UINT32 nEnd, INT32 nType);
void   H8SetReadByteHandler(UINT8 (*pHandler)(UINT32));
void   H8SetReadWordHandler(UINT16 (*pHandler)(UINT32));
void   H8SetWriteByteHandler(void (*pHandler)(UINT32, UINT8));
void   H8SetWriteWordHandler(void (*pHandler)(UINT32, UINT16));

// on-chip peripherals
void   H8SetReadPortHandler(UINT8 (*pHandler)(INT32 nPort));
void   H8SetWritePortHandler(void (*pHandler)(INT32 nPort, UINT8 nData));
void   H8SetAdcReadHandler(UINT16 (*pHandler)(INT32 nChannel));		// 10-bit
void   H8SetSciTxHandler(void (*pHandler)(INT32 nChannel, UINT8 nData));
void   H8SetSciRxHandler(INT32 (*pHandler)(INT32 nChannel));			// -1 = no data

void   H8SetIRQLine(INT32 nLine, INT32 nState);
void   H8AdcTrigger();								// ADTRG pin pulse

UINT32 H8GetPC();

UINT8  H8CheatRead(UINT32 a);
void   H8CheatWrite(UINT32 a, UINT8 d);

extern struct cpu_core_config H8Config;

#endif
