
void Arm7WriteByte(UINT32 addr, UINT8 data);
void Arm7WriteWord(UINT32 addr, UINT16 data);
void Arm7WriteLong(UINT32 addr, UINT32 data);
UINT8  Arm7ReadByte(UINT32 addr);
UINT16 Arm7ReadWord(UINT32 addr);
UINT32 Arm7ReadLong(UINT32 addr);
UINT16 Arm7FetchWord(UINT32 addr);
UINT32 Arm7FetchLong(UINT32 addr);

void Arm7RunEnd();
void Arm7RunEndEatCycles();
void Arm7BurnCycles(INT32 cycles);
INT32 Arm7Idle(int cycles);
INT32 Arm7TotalCycles();
void Arm7NewFrame();
INT32 Arm7GetActive();

void Arm7Init(INT32 nCPU);
void Arm7Reset();
INT32 Arm7Run(INT32 cycles);
void Arm7Exit();
void Arm7Open(INT32 );
void Arm7Close();

INT32 Arm7Scan(INT32 nAction);

#define ARM7_IRQ_LINE		0
#define ARM7_FIRQ_LINE		1

void Arm7SetIRQLine(INT32 line, INT32 state);

void Arm7MapMemory(UINT8 *src, UINT32 start, UINT32 finish, INT32 type);

void Arm7SetWriteByteHandler(void (*write)(UINT32, UINT8));
void Arm7SetWriteWordHandler(void (*write)(UINT32, UINT16));
void Arm7SetWriteLongHandler(void (*write)(UINT32, UINT32));
void Arm7SetReadByteHandler(UINT8 (*read)(UINT32));
void Arm7SetReadWordHandler(UINT16 (*read)(UINT32));
void Arm7SetReadLongHandler(UINT32 (*read)(UINT32));

// speed hack function
void Arm7SetIdleLoopAddress(UINT32 address);

void Arm7_write_rom_byte(UINT32 addr, UINT8 data); // for cheating

extern struct cpu_core_config Arm7Config;

// depreciate this and use BurnTimerAttach directly!
#define BurnTimerAttachArm7(clock)	\
	BurnTimerAttach(&Arm7Config, clock)
