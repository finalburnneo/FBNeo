// Cinematronics CPU (CCPU) interface for FinalBurn Neo    - dink November '26

#ifndef _CCPU_INTF_H_
#define _CCPU_INTF_H_

enum {
	CCPU_PC = 1,
	CCPU_FLAGS,
	CCPU_A,
	CCPU_B,
	CCPU_I,
	CCPU_J,
	CCPU_P,
	CCPU_X,
	CCPU_Y,
	CCPU_T
};

// program rom layouts
#define CCPU_PRG_4K		0	// 0x0000-0x0fff (mask 0x0fff)
#define CCPU_PRG_8K		1	// 2 x 4k, each mirrored once in a 16k space
#define CCPU_PRG_16K	2	// 0x0000-0x3fff
#define CCPU_PRG_32K	3	// 0x0000-0x7fff

void ccpuInit();
void ccpuExit();
void ccpuOpen(INT32 nCPU);
void ccpuClose();
INT32 ccpuGetActive();
void ccpuReset();
INT32 ccpuRun(INT32 nCycles);
void ccpuRunEnd();
INT32 ccpuIdle(INT32 nCycles);
INT32 ccpuTotalCycles();
void ccpuNewFrame();
INT32 ccpuScan(INT32 nAction);

void ccpuMapProgram(UINT8 *rom, INT32 type);
void ccpuMapData(UINT16 *ram);		// 0x100 words of 12-bit ram (driver can re-map for banking)

void ccpuSetReadPortHandler(UINT8 (*pRead)(UINT8 port));			// port 0x00-0x17, bit 0 used
void ccpuSetWritePortHandler(void (*pWrite)(UINT8 port, UINT8 data));	// port 0x00-0x07, bit 0
void ccpuSetExternalHandler(UINT8 (*pExt)());	// NULL = JMI jumper (read MI flag)
void ccpuSetVectorHandler(void (*pVec)(INT16 sx, INT16 sy, INT16 ex, INT16 ey, UINT8 shift));

void ccpuWatchdogTrigger();	// call at vblank: releases FRM wait, resets cpu if CST not hit for 3 frames
INT32 ccpuIsWaiting();

UINT32 ccpuGetReg(INT32 reg);
void ccpuSetReg(INT32 reg, UINT32 val);
UINT32 ccpuGetPC(INT32);

UINT8 ccpuReadCheat(UINT32 a);
void ccpuWriteCheat(UINT32 a, UINT8 d);

extern struct cpu_core_config ccpuConfig;

#endif
