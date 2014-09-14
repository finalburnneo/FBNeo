void ArmWriteByte(UINT32 addr, UINT8 data);
void ArmWriteLong(UINT32 addr, UINT32 data);
UINT8  ArmReadByte(UINT32 addr);
UINT32 ArmReadLong(UINT32 addr);
UINT32 ArmFetchLong(UINT32 addr);

#define ARM_READ		1
#define ARM_WRITE		2
#define ARM_FETCH		4

#define ARM_ROM		(ARM_READ | ARM_FETCH)
#define ARM_RAM		(ARM_READ | ARM_FETCH | ARM_WRITE)

void ArmMapMemory(UINT8 *src, INT32 start, INT32 finish, INT32 type);

void ArmSetWriteByteHandler(void (*write)(UINT32, UINT8));
void ArmSetWriteLongHandler(void (*write)(UINT32, UINT32));
void ArmSetReadByteHandler(UINT8 (*read)(UINT32));
void ArmSetReadLongHandler(UINT32 (*read)(UINT32));

void ArmInit(INT32);
void ArmOpen(INT32);
void ArmReset();
INT32 ArmRun(INT32 cycles);
INT32 ArmScan(INT32 nAction);

#define ARM_IRQ_LINE		0
#define ARM_FIRQ_LINE		1

#define ARM_IRQSTATUS_NONE	0
#define ARM_IRQSTATUS_ACK	1
#define ARM_IRQSTATUS_AUTO	2

void ArmSetIRQLine(INT32 line, INT32 state);

void ArmExit();
void ArmClose();

extern UINT32 ArmSpeedHackAddress;
void ArmIdleCycles(INT32 cycles);
void ArmSetSpeedHack(UINT32 address, void (*pCallback)());

UINT32 ArmGetPC(INT32);

UINT32 ArmRemainingCycles();
INT32 ArmGetTotalCycles();
void ArmRunEnd();
void ArmNewFrame();



//-----------------------------------------------------------------------------------------------------------------------------------------------
// Macros for reading/writing to 16-bit RAM regions

#define Write16Long(ram, a, b)							\
	if (address >= a && address <= b) {					\
		*((UINT16*)(ram + (((address - a) & ~3)/2))) = BURN_ENDIAN_SWAP_INT16(data);	\
		return;								\
	}

#define Write16Byte(ram, a, b)							\
	if (address >= a && address <= b) {					\
		if (~address & 2) {						\
			INT32 offset = address - a;				\
			ram[(offset & 1) | ((offset & ~3) / 2)] = data;		\
		}								\
		return;								\
	}

#define Read16Long(ram, a, b)							\
	if (address >= a && address <= b) {					\
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(ram + (((address - a) & ~3)/2)))) | ~0xffff;	\
	}

#define Read16Byte(ram, a, b)							\
	if (address >= a && address <= b) {					\
		if (~address & 2) {						\
			INT32 offset = address - a;				\
			return ram[(offset & 1) | ((offset & ~3) / 2)];		\
		} else {							\
			return 0xff;						\
		}								\
	}
