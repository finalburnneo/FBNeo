
INT32 i386Run(INT32 num_cycles);
void i386SetIRQLine(INT32 irqline, INT32 state);
void i386Exit();
void i386Open(INT32);
void i386Close();
void i386Reset();
void i386Init(INT32); // only 1 supported
void i386MapMemory(UINT8 *mem, UINT64 start, UINT64 end, UINT32 flags);
void i386SetReadHandlers(UINT8 (*read8)(UINT32), UINT16 (*read16)(UINT32), UINT32 (*read32)(UINT32));
void i386SetWriteHandlers(void (*write8)(UINT32,UINT8), void (*write16)(UINT32,UINT16), void (*write32)(UINT32,UINT32));
void i386NewFrame();
void i386RunEnd();
void i386HaltUntilInterrupt(INT32 val);
INT32 i386TotalCycles();
INT32 i386GetActive(); 
void i386SetIRQCallback(INT32 (*irq_callback)(INT32));
INT32 i386Idle(INT32 cycles);
INT32 i386Scan(INT32 nAction);

UINT32 i386GetPC(INT32);

UINT8 i386ReadByte(UINT32 address);
UINT32 i386ReadLong(UINT32 address);

extern cpu_core_config i386Config;
