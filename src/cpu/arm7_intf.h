
void Arm7WriteByte(UINT32 addr, UINT8 data);
void Arm7WriteWord(UINT32 addr, UINT16 data);
void Arm7WriteLong(UINT32 addr, UINT32 data);
UINT8  Arm7ReadByte(UINT32 addr);
UINT16 Arm7ReadWord(UINT32 addr);
UINT32 Arm7ReadLong(UINT32 addr);
UINT16 Arm7FetchWord(UINT32 addr);
UINT32 Arm7FetchLong(UINT32 addr);

void Arm7RunEnd();
void Arm7BurnCycles(INT32 cycles);
INT32 Arm7TotalCycles();
void Arm7NewFrame();

void Arm7Init(INT32);
void Arm7Reset();
INT32 Arm7Run(INT32 cycles);
void Arm7Exit();
void Arm7Open(INT32 );
void Arm7Close();

INT32 Arm7Scan(INT32 nAction);

#define ARM7_IRQ_LINE		0
#define ARM7_FIRQ_LINE		1

#define ARM7_IRQSTATUS_NONE	0
#define ARM7_IRQSTATUS_ACK	1
#define ARM7_IRQSTATUS_AUTO	2

void Arm7SetIRQLine(INT32 line, INT32 state);

#define ARM7_READ		1
#define ARM7_WRITE		2
#define ARM7_FETCH		4

#define ARM7_ROM		(ARM7_READ | ARM7_FETCH)
#define ARM7_RAM		(ARM7_READ | ARM7_FETCH | ARM7_WRITE)

void Arm7MapMemory(UINT8 *src, UINT32 start, UINT32 finish, INT32 type);

void Arm7SetWriteByteHandler(void (*write)(UINT32, UINT8));
void Arm7SetWriteWordHandler(void (*write)(UINT32, UINT16));
void Arm7SetWriteLongHandler(void (*write)(UINT32, UINT32));
void Arm7SetReadByteHandler(UINT8 (*read)(UINT32));
void Arm7SetReadWordHandler(UINT16 (*read)(UINT32));
void Arm7SetReadLongHandler(UINT32 (*read)(UINT32));

// speed hack function
void Arm7SetIdleLoopAddress(UINT32 address);

