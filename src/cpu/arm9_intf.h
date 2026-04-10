// ARM9 interface header for FBNeo
// Modeled on arm7_intf.h with Arm9 naming for ARM946E-S CPU (IGS036)

#pragma once

void Arm9WriteByte(UINT32 addr, UINT8 data);
void Arm9WriteWord(UINT32 addr, UINT16 data);
void Arm9WriteLong(UINT32 addr, UINT32 data);
UINT8  Arm9ReadByte(UINT32 addr);
UINT16 Arm9ReadWord(UINT32 addr);
UINT32 Arm9ReadLong(UINT32 addr);
UINT16 Arm9FetchWord(UINT32 addr);
UINT32 Arm9FetchLong(UINT32 addr);

void Arm9RunEnd();
void Arm9RunEndEatCycles();
void Arm9BurnUntilInterrupt();
void Arm9BurnCycles(INT32 cycles);
INT32 Arm9Idle(int cycles);
INT32 Arm9TotalCycles();
void Arm9NewFrame();
INT32 Arm9GetActive();

void Arm9Init(INT32 nCPU);
void Arm9Reset();
INT32 Arm9Run(INT32 cycles);
void Arm9Exit();
void Arm9Open(INT32);
void Arm9Close();

INT32 Arm9Scan(INT32 nAction);

void Arm9SetIRQLine(INT32 line, INT32 state);

void Arm9MapMemory(UINT8 *src, UINT32 start, UINT32 finish, INT32 type);
void Arm9UnmapMemory(UINT32 start, UINT32 finish, INT32 type);

void Arm9SetWriteByteHandler(void (*write)(UINT32, UINT8));
void Arm9SetWriteWordHandler(void (*write)(UINT32, UINT16));
void Arm9SetWriteLongHandler(void (*write)(UINT32, UINT32));
void Arm9SetReadByteHandler(UINT8 (*read)(UINT32));
void Arm9SetReadWordHandler(UINT16 (*read)(UINT32));
void Arm9SetReadLongHandler(UINT32 (*read)(UINT32));

void Arm9SetIdleLoopAddress(UINT32 address);

void Arm9_write_rom_byte(UINT32 addr, UINT8 data);

// Debug helpers
UINT32 Arm9DbgGetPC();
UINT32 Arm9DbgGetCPSR();
UINT32 Arm9DbgGetRegister(INT32 index);
void   Arm9DbgSetRegister(INT32 index, UINT32 value);
UINT32 Arm9DbgGetCp15ReadCount();
UINT32 Arm9DbgGetCp15WriteCount();
UINT32 Arm9DbgGetCp15Value(UINT32 crn, UINT32 op1, UINT32 crm, UINT32 op2);
UINT32 Arm9DbgGetCp15LastReadInsn();
UINT32 Arm9DbgGetCp15LastReadValue();
UINT32 Arm9DbgGetCp15LastWriteInsn();
UINT32 Arm9DbgGetCp15LastWriteValue();

extern struct cpu_core_config Arm9Config;

#define BurnTimerAttachArm9(clock)	\
	BurnTimerAttach(&Arm9Config, clock)
