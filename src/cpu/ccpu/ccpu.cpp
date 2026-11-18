// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/***************************************************************************

    ccpu.cpp
    Core implementation for the portable Cinematronics CPU emulator.

    Written by Aaron Giles
    Special thanks to Zonn Moore for his detailed documentation.

    FinalBurn Neo port: single cpu, direct rom/ram pointers, port and
    vector callbacks supplied by the driver.

***************************************************************************/

#include "burnint.h"
#include "ccpu_intf.h"

struct ccpu_state {
	UINT16 PC;
	UINT16 A;
	UINT16 B;
	UINT8  I;
	UINT16 J;
	UINT8  P;
	UINT16 X;
	UINT16 Y;
	UINT16 T;
	UINT8  acc_is_b;	// 0 = A, 1 = B (stands in for MAME's m_acc pointer)

	UINT16 a0flag, ncflag, cmpacc, cmpval;
	UINT16 miflag, nextmiflag, nextnextmiflag;
	UINT16 drflag;

	UINT8  waiting;
	UINT8  watchdog;
	UINT8  extinput;

	INT32  total_cycles;
};

static ccpu_state cs;

static INT32 icount;
static INT32 cycles_start;
static INT32 nCcpuActive = -1;

static UINT8  *prg_rom = NULL;
static INT32   prg_type = CCPU_PRG_4K;
static UINT16 *data_ram = NULL;
static UINT16  dummy_ram[0x100];

static UINT8 (*read_port)(UINT8) = NULL;
static void (*write_port)(UINT8, UINT8) = NULL;
static UINT8 (*external_input)() = NULL;
static void (*vector_cb)(INT16, INT16, INT16, INT16, UINT8) = NULL;

static void core_set_irq(INT32, INT32, INT32) { } // no interrupts on this cpu

cpu_core_config ccpuConfig =
{
	"ccpu",
	ccpuOpen,
	ccpuClose,
	ccpuReadCheat,
	ccpuWriteCheat,
	ccpuGetActive,
	ccpuTotalCycles,
	ccpuNewFrame,
	ccpuIdle,
	core_set_irq,
	ccpuRun,
	ccpuRunEnd,
	ccpuReset,
	ccpuScan,
	ccpuExit,
	0x200,		// cheats: data ram as bytes (0x100 words)
	0
};

/***************************************************************************
    MEMORY / IO
***************************************************************************/

static inline UINT8 READOP(UINT16 a)
{
	switch (prg_type) {
		case CCPU_PRG_4K:  a &= 0x0fff; break;
		case CCPU_PRG_8K:  a = ((a & 0x2000) >> 1) | (a & 0x0fff); break;
		case CCPU_PRG_16K: a &= 0x3fff; break;
		default:           a &= 0x7fff; break;
	}
	return prg_rom[a];
}

#define RDMEM(a)         (data_ram[(a) & 0xff])
#define WRMEM(a,v)       do { data_ram[(a) & 0xff] = (v); } while (0)

#define READPORT(a)      ((read_port) ? read_port(a) : 0)
#define WRITEPORT(a,v)   do { if (write_port) write_port((a), (v)); } while (0)

#define ACC              (cs.acc_is_b ? cs.B : cs.A)
#define SET_ACC(v)       do { if (cs.acc_is_b) cs.B = (v); else cs.A = (v); } while (0)

#define SET_A0           do { cs.a0flag = cs.A; } while (0)
#define SET_CMP_VAL(x)   do { cs.cmpacc = ACC; cs.cmpval = (x) & 0xfff; } while (0)
#define SET_NC(a)        do { cs.ncflag = ~(a); } while (0)
#define SET_MI(a)        do { cs.nextnextmiflag = (a); } while (0)

#define TEST_A0          (cs.a0flag & 1)
#define TEST_NC          ((cs.ncflag >> 12) & 1)
#define TEST_MI          ((cs.miflag >> 11) & 1)
#define TEST_LT          (cs.cmpval < cs.cmpacc)
#define TEST_EQ          (cs.cmpval == cs.cmpacc)
#define TEST_DR          (cs.drflag != 0)

#define NEXT_ACC_A       do { SET_MI(ACC); cs.acc_is_b = 0; } while (0)
#define NEXT_ACC_B       do { SET_MI(ACC); cs.acc_is_b ^= 1; } while (0)

#define CYCLES(x)        do { icount -= (x); } while (0)

#define ASR(x)           (((x) >> 1) | ((x) & 0x800))

#define STANDARD_ACC_OP(resexp,cmpval) \
do { \
	UINT16 result = resexp; \
	SET_A0;                     /* set the A0 bit based on the previous 'A' value */ \
	SET_CMP_VAL(cmpval);        /* set the compare values to the previous accumulator and the cmpval */ \
	SET_NC(result);             /* set the NC flag based on the unmasked result */ \
	SET_ACC(result & 0xfff);    /* store the low 12 bits of the new value */ \
} while (0)

#define JUMP_J           do { cs.PC = ((cs.PC - 1) & 0xf000) + cs.J; } while (0)

static inline INT16 sext12(UINT16 v)
{
	return (INT16)(v << 4) >> 4;
}

/***************************************************************************
    INTERFACE
***************************************************************************/

void ccpuMapProgram(UINT8 *rom, INT32 type)
{
	prg_rom = rom;
	prg_type = type;
}

void ccpuMapData(UINT16 *ram)
{
	data_ram = (ram) ? ram : dummy_ram;
}

void ccpuSetReadPortHandler(UINT8 (*pRead)(UINT8))
{
	read_port = pRead;
}

void ccpuSetWritePortHandler(void (*pWrite)(UINT8, UINT8))
{
	write_port = pWrite;
}

void ccpuSetExternalHandler(UINT8 (*pExt)())
{
	external_input = pExt;
}

void ccpuSetVectorHandler(void (*pVec)(INT16, INT16, INT16, INT16, UINT8))
{
	vector_cb = pVec;
}

void ccpuOpen(INT32 nCPU)
{
	nCcpuActive = nCPU;
}

void ccpuClose()
{
	nCcpuActive = -1;
}

INT32 ccpuGetActive()
{
	return nCcpuActive;
}

void ccpuInit()
{
	memset(&cs, 0, sizeof(cs));
	memset(dummy_ram, 0, sizeof(dummy_ram));

	prg_rom = NULL;
	prg_type = CCPU_PRG_4K;
	data_ram = dummy_ram;
	read_port = NULL;
	write_port = NULL;
	external_input = NULL;
	vector_cb = NULL;
	icount = 0;
	cycles_start = 0;

	CpuCheatRegister(0, &ccpuConfig);
}

void ccpuExit()
{
	prg_rom = NULL;
	data_ram = dummy_ram;
	read_port = NULL;
	write_port = NULL;
	external_input = NULL;
	vector_cb = NULL;
	nCcpuActive = -1;
}

void ccpuReset()
{
	/* zero registers */
	cs.PC = 0;
	cs.A = 0;
	cs.B = 0;
	cs.I = 0;
	cs.J = 0;
	cs.P = 0;
	cs.X = 0;
	cs.Y = 0;
	cs.T = 0;
	cs.acc_is_b = 0;

	/* zero flags */
	cs.a0flag = 0;
	cs.ncflag = 0;
	cs.cmpacc = 0;
	cs.cmpval = 1;
	cs.miflag = cs.nextmiflag = cs.nextnextmiflag = 0;
	cs.drflag = 0;

	cs.waiting = 0;
	cs.watchdog = 0;
}

void ccpuWatchdogTrigger()
{
	cs.waiting = 0;
	cs.watchdog++;
	if (cs.watchdog >= 3) {
		INT32 total = cs.total_cycles;
		ccpuReset();
		cs.total_cycles = total;
	}
}

INT32 ccpuIsWaiting()
{
	return cs.waiting;
}

INT32 ccpuTotalCycles()
{
	return cs.total_cycles + (cycles_start - icount);
}

void ccpuNewFrame()
{
	cs.total_cycles = 0;
}

INT32 ccpuIdle(INT32 nCycles)
{
	cs.total_cycles += nCycles;
	return nCycles;
}

void ccpuRunEnd()
{
	icount = 0;
}

UINT32 ccpuGetPC(INT32)
{
	return cs.PC;
}

UINT32 ccpuGetReg(INT32 reg)
{
	switch (reg)
	{
		case CCPU_PC: return cs.PC;
		case CCPU_A:  return cs.A;
		case CCPU_B:  return cs.B;
		case CCPU_I:  return cs.I;
		case CCPU_J:  return cs.J;
		case CCPU_P:  return cs.P;
		case CCPU_X:  return cs.X;
		case CCPU_Y:  return cs.Y;
		case CCPU_T:  return cs.T;
	}
	return 0;
}

void ccpuSetReg(INT32 reg, UINT32 val)
{
	switch (reg)
	{
		case CCPU_PC: cs.PC = val; break;
		case CCPU_A:  cs.A = val & 0xfff; break;
		case CCPU_B:  cs.B = val & 0xfff; break;
		case CCPU_I:  cs.I = val; break;
		case CCPU_J:  cs.J = val & 0xfff; break;
		case CCPU_P:  cs.P = val & 0xf; break;
		case CCPU_X:  cs.X = val; break;
		case CCPU_Y:  cs.Y = val; break;
		case CCPU_T:  cs.T = val; break;
	}
}

UINT8 ccpuReadCheat(UINT32 a)
{
	a &= 0x1ff;
	UINT16 w = data_ram[a >> 1];
	return (a & 1) ? (w >> 8) : (w & 0xff);
}

void ccpuWriteCheat(UINT32 a, UINT8 d)
{
	a &= 0x1ff;
	UINT16 &w = data_ram[a >> 1];
	if (a & 1)
		w = (w & 0x00ff) | (d << 8);
	else
		w = (w & 0xff00) | d;
}

INT32 ccpuScan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		ScanVar(&cs, sizeof(cs), "ccpu Regs");
	}

	return 0;
}

/***************************************************************************
    CORE EXECUTION LOOP
***************************************************************************/

INT32 ccpuRun(INT32 nCycles)
{
	cycles_start = nCycles;
	icount = nCycles;

	if (cs.waiting)
	{
		icount = 0;
		cs.total_cycles += nCycles;
		cycles_start = icount = 0;
		return nCycles;
	}

	do
	{
		UINT16 tempval;
		UINT8 opcode;

		/* update the delayed MI flag */
		cs.miflag = cs.nextmiflag;
		cs.nextmiflag = cs.nextnextmiflag;

		/* fetch the opcode */
		opcode = READOP(cs.PC);
		if (opcode == 0x51 || opcode == 0x59)
			cs.extinput = (external_input) ? external_input() : TEST_MI;
		cs.PC++;

		switch (opcode)
		{
			/* LDAI */
			case 0x00:  case 0x01:  case 0x02:  case 0x03:
			case 0x04:  case 0x05:  case 0x06:  case 0x07:
			case 0x08:  case 0x09:  case 0x0a:  case 0x0b:
			case 0x0c:  case 0x0d:  case 0x0e:  case 0x0f:
				tempval = (opcode & 0x0f) << 8;
				STANDARD_ACC_OP(tempval, tempval);
				NEXT_ACC_A; CYCLES(1);
				break;

			/* INP */
			case 0x10:  case 0x11:  case 0x12:  case 0x13:
			case 0x14:  case 0x15:  case 0x16:  case 0x17:
			case 0x18:  case 0x19:  case 0x1a:  case 0x1b:
			case 0x1c:  case 0x1d:  case 0x1e:  case 0x1f:
				if (cs.acc_is_b == 0)
					tempval = READPORT(opcode & 0x0f) & 1;
				else
					tempval = READPORT(16 + (opcode & 0x07)) & 1;
				STANDARD_ACC_OP(tempval, tempval);
				NEXT_ACC_A; CYCLES(1);
				break;

			/* A8I */
			case 0x20:
				tempval = READOP(cs.PC++);
				STANDARD_ACC_OP(ACC + tempval, tempval);
				NEXT_ACC_A; CYCLES(3);
				break;

			/* A4I */
			case 0x21:  case 0x22:  case 0x23:
			case 0x24:  case 0x25:  case 0x26:  case 0x27:
			case 0x28:  case 0x29:  case 0x2a:  case 0x2b:
			case 0x2c:  case 0x2d:  case 0x2e:  case 0x2f:
				tempval = opcode & 0x0f;
				STANDARD_ACC_OP(ACC + tempval, tempval);
				NEXT_ACC_A; CYCLES(1);
				break;

			/* S8I */
			case 0x30:
				tempval = READOP(cs.PC++);
				STANDARD_ACC_OP(ACC + (tempval ^ 0xfff) + 1, tempval);
				NEXT_ACC_A; CYCLES(3);
				break;

			/* S4I */
			case 0x31:  case 0x32:  case 0x33:
			case 0x34:  case 0x35:  case 0x36:  case 0x37:
			case 0x38:  case 0x39:  case 0x3a:  case 0x3b:
			case 0x3c:  case 0x3d:  case 0x3e:  case 0x3f:
				tempval = opcode & 0x0f;
				STANDARD_ACC_OP(ACC + (tempval ^ 0xfff) + 1, tempval);
				NEXT_ACC_A; CYCLES(1);
				break;

			/* LPAI */
			case 0x40:  case 0x41:  case 0x42:  case 0x43:
			case 0x44:  case 0x45:  case 0x46:  case 0x47:
			case 0x48:  case 0x49:  case 0x4a:  case 0x4b:
			case 0x4c:  case 0x4d:  case 0x4e:  case 0x4f:
				tempval = READOP(cs.PC++);
				cs.J = (opcode & 0x0f) + (tempval & 0xf0) + ((tempval & 0x0f) << 8);
				NEXT_ACC_A; CYCLES(3);
				break;

			/* T4K */
			case 0x50:
				cs.PC = (cs.P << 12) + cs.J;
				NEXT_ACC_B; CYCLES(4);
				break;

			/* JMIB/JEHB */
			case 0x51:
				if (cs.extinput) { JUMP_J; CYCLES(2); }
				NEXT_ACC_B; CYCLES(2);
				break;

			/* JVNB */
			case 0x52:
				if (TEST_DR) { JUMP_J; CYCLES(2); }
				NEXT_ACC_B; CYCLES(2);
				break;

			/* JLTB */
			case 0x53:
				if (TEST_LT) { JUMP_J; CYCLES(2); }
				NEXT_ACC_B; CYCLES(2);
				break;

			/* JEQB */
			case 0x54:
				if (TEST_EQ) { JUMP_J; CYCLES(2); }
				NEXT_ACC_B; CYCLES(2);
				break;

			/* JCZB */
			case 0x55:
				if (TEST_NC) { JUMP_J; CYCLES(2); }
				NEXT_ACC_B; CYCLES(2);
				break;

			/* JOSB */
			case 0x56:
				if (TEST_A0) { JUMP_J; CYCLES(2); }
				NEXT_ACC_B; CYCLES(2);
				break;

			/* SSA */
			case 0x57:
				NEXT_ACC_B; CYCLES(2);
				break;

			/* JMP */
			case 0x58:
				JUMP_J;
				NEXT_ACC_A; CYCLES(4);
				break;

			/* JMI/JEH */
			case 0x59:
				if (cs.extinput) { JUMP_J; CYCLES(2); }
				NEXT_ACC_A; CYCLES(2);
				break;

			/* JVN */
			case 0x5a:
				if (TEST_DR) { JUMP_J; CYCLES(2); }
				NEXT_ACC_A; CYCLES(2);
				break;

			/* JLT */
			case 0x5b:
				if (TEST_LT) { JUMP_J; CYCLES(2); }
				NEXT_ACC_A; CYCLES(2);
				break;

			/* JEQ */
			case 0x5c:
				if (TEST_EQ) { JUMP_J; CYCLES(2); }
				NEXT_ACC_A; CYCLES(2);
				break;

			/* JCZ */
			case 0x5d:
				if (TEST_NC) { JUMP_J; CYCLES(2); }
				NEXT_ACC_A; CYCLES(2);
				break;

			/* JOS */
			case 0x5e:
				if (TEST_A0) { JUMP_J; CYCLES(2); }
				NEXT_ACC_A; CYCLES(2);
				break;

			/* NOP */
			case 0x5f:
				NEXT_ACC_A; CYCLES(2);
				break;

			/* ADD */
			case 0x60:  case 0x61:  case 0x62:  case 0x63:
			case 0x64:  case 0x65:  case 0x66:  case 0x67:
			case 0x68:  case 0x69:  case 0x6a:  case 0x6b:
			case 0x6c:  case 0x6d:  case 0x6e:  case 0x6f:
				cs.I = (cs.P << 4) + (opcode & 0x0f);
				tempval = RDMEM(cs.I);
				STANDARD_ACC_OP(ACC + tempval, tempval);
				NEXT_ACC_A; CYCLES(3);
				break;

			/* SUB */
			case 0x70:  case 0x71:  case 0x72:  case 0x73:
			case 0x74:  case 0x75:  case 0x76:  case 0x77:
			case 0x78:  case 0x79:  case 0x7a:  case 0x7b:
			case 0x7c:  case 0x7d:  case 0x7e:  case 0x7f:
				cs.I = (cs.P << 4) + (opcode & 0x0f);
				tempval = RDMEM(cs.I);
				STANDARD_ACC_OP(ACC + (tempval ^ 0xfff) + 1, tempval);
				NEXT_ACC_A; CYCLES(3);
				break;

			/* SETP */
			case 0x80:  case 0x81:  case 0x82:  case 0x83:
			case 0x84:  case 0x85:  case 0x86:  case 0x87:
			case 0x88:  case 0x89:  case 0x8a:  case 0x8b:
			case 0x8c:  case 0x8d:  case 0x8e:  case 0x8f:
				cs.P = opcode & 0x0f;
				NEXT_ACC_A; CYCLES(1);
				break;

			/* OUT */
			case 0x90:  case 0x91:  case 0x92:  case 0x93:
			case 0x94:  case 0x95:  case 0x96:  case 0x97:
			case 0x98:  case 0x99:  case 0x9a:  case 0x9b:
			case 0x9c:  case 0x9d:  case 0x9e:  case 0x9f:
				if (cs.acc_is_b == 0)
					WRITEPORT(opcode & 0x07, ~cs.A & 1);
				NEXT_ACC_A; CYCLES(1);
				break;

			/* LDA */
			case 0xa0:  case 0xa1:  case 0xa2:  case 0xa3:
			case 0xa4:  case 0xa5:  case 0xa6:  case 0xa7:
			case 0xa8:  case 0xa9:  case 0xaa:  case 0xab:
			case 0xac:  case 0xad:  case 0xae:  case 0xaf:
				cs.I = (cs.P << 4) + (opcode & 0x0f);
				tempval = RDMEM(cs.I);
				STANDARD_ACC_OP(tempval, tempval);
				NEXT_ACC_A; CYCLES(3);
				break;

			/* TST */
			case 0xb0:  case 0xb1:  case 0xb2:  case 0xb3:
			case 0xb4:  case 0xb5:  case 0xb6:  case 0xb7:
			case 0xb8:  case 0xb9:  case 0xba:  case 0xbb:
			case 0xbc:  case 0xbd:  case 0xbe:  case 0xbf:
				cs.I = (cs.P << 4) + (opcode & 0x0f);
				tempval = RDMEM(cs.I);
				{
					UINT16 result = ACC + (tempval ^ 0xfff) + 1;
					SET_A0;
					SET_CMP_VAL(tempval);
					SET_NC(result);
					SET_MI(result);
				}
				NEXT_ACC_A; CYCLES(3);
				break;

			/* WS */
			case 0xc0:  case 0xc1:  case 0xc2:  case 0xc3:
			case 0xc4:  case 0xc5:  case 0xc6:  case 0xc7:
			case 0xc8:  case 0xc9:  case 0xca:  case 0xcb:
			case 0xcc:  case 0xcd:  case 0xce:  case 0xcf:
				cs.I = (cs.P << 4) + (opcode & 0x0f);
				cs.I = RDMEM(cs.I) & 0xff;
				NEXT_ACC_A; CYCLES(3);
				break;

			/* STA */
			case 0xd0:  case 0xd1:  case 0xd2:  case 0xd3:
			case 0xd4:  case 0xd5:  case 0xd6:  case 0xd7:
			case 0xd8:  case 0xd9:  case 0xda:  case 0xdb:
			case 0xdc:  case 0xdd:  case 0xde:  case 0xdf:
				cs.I = (cs.P << 4) + (opcode & 0x0f);
				WRMEM(cs.I, ACC);
				NEXT_ACC_A; CYCLES(3);
				break;

			/* DV */
			case 0xe0:
				{
					INT16 stopX = sext12(cs.A);
					INT16 stopY = sext12(cs.B);

					stopX = ((INT16)(stopX - cs.X) >> cs.T) + cs.X;
					stopY = ((INT16)(stopY - cs.Y) >> cs.T) + cs.Y;

					if (vector_cb)
						vector_cb(cs.X, cs.Y, stopX, stopY, cs.T);

					/* hack to make QB3 display semi-correctly during explosions */
					cs.A = cs.X & 0xfff;
					cs.B = cs.Y & 0xfff;
				}
				NEXT_ACC_A; CYCLES(1);
				break;

			/* LPAP */
			case 0xe1:
				cs.J = RDMEM(cs.I);
				NEXT_ACC_A; CYCLES(3);
				break;

			/* WSP */
			case 0xf1:
				cs.I = RDMEM(cs.I) & 0xff;
				NEXT_ACC_A; CYCLES(3);
				break;

			/* LKP */
			case 0xe2:
			case 0xf2:
				tempval = READOP(((cs.PC - 1) & 0xf000) + ACC);
				STANDARD_ACC_OP(tempval, tempval);
				NEXT_ACC_A; CYCLES(7);
				cs.PC++;
				break;

			/* MUL */
			case 0xe3:
			case 0xf3:
				tempval = RDMEM(cs.I);
				SET_A0;
				cs.cmpval = tempval & 0xfff;
				if (cs.acc_is_b == 0)
				{
					if (cs.A & 1)
					{
						UINT16 result;
						cs.cmpacc = cs.B;
						cs.A = (cs.A >> 1) | ((cs.B << 11) & 0x800);
						cs.B = ASR(cs.B);
						result = cs.B + tempval;
						SET_NC(result);
						SET_MI(result);
						cs.B = result & 0xfff;
					}
					else
					{
						UINT16 result;
						cs.cmpacc = cs.A;
						result = cs.A + tempval;
						cs.A = (cs.A >> 1) | ((cs.B << 11) & 0x800);
						cs.B = ASR(cs.B);
						SET_NC(result);
						SET_MI(result);
					}
				}
				else
				{
					UINT16 result;
					cs.cmpacc = cs.B;
					cs.B = ASR(cs.B);
					result = cs.B + tempval;
					SET_NC(result);
					SET_MI(result);
					if (cs.A & 1)
						cs.B = result & 0xfff;
				}
				NEXT_ACC_A; CYCLES(2);
				break;

			/* NV */
			case 0xe4:
			case 0xf4:
				cs.T = 0;
				while (((cs.A & 0xa00) == 0x000 || (cs.A & 0xa00) == 0xa00) &&
						((cs.B & 0xa00) == 0x000 || (cs.B & 0xa00) == 0xa00) &&
						cs.T < 16)
				{
					cs.A = (cs.A << 1) & 0xfff;
					cs.B = (cs.B << 1) & 0xfff;
					cs.T++;
					CYCLES(1);
				}
				NEXT_ACC_A; CYCLES(1);
				break;

			/* FRM */
			case 0xe5:
			case 0xf5:
				cs.waiting = 1;
				NEXT_ACC_A;
				CYCLES(1);
				/* some games repeat the FRM opcode twice; it apparently does not cause
				   a second wait, so we make sure we skip any duplicate opcode at this
				   point */
				if (READOP(cs.PC) == opcode)
					cs.PC++;
				/* burn the rest of the timeslice, like MAME does */
				if (icount > 0) icount = 0;
				break;

			/* STAP */
			case 0xe6:
			case 0xf6:
				WRMEM(cs.I, ACC);
				NEXT_ACC_A; CYCLES(2);
				break;

			/* CST */
			case 0xf7:
				cs.watchdog = 0;
				/* fall through */
			/* ADDP */
			case 0xe7:
				tempval = RDMEM(cs.I);
				STANDARD_ACC_OP(ACC + tempval, tempval);
				NEXT_ACC_A; CYCLES(2);
				break;

			/* SUBP */
			case 0xe8:
			case 0xf8:
				tempval = RDMEM(cs.I);
				STANDARD_ACC_OP(ACC + (tempval ^ 0xfff) + 1, tempval);
				NEXT_ACC_A; CYCLES(3);
				break;

			/* ANDP */
			case 0xe9:
			case 0xf9:
				tempval = RDMEM(cs.I);
				STANDARD_ACC_OP(ACC & tempval, tempval);
				NEXT_ACC_A; CYCLES(2);
				break;

			/* LDAP */
			case 0xea:
			case 0xfa:
				tempval = RDMEM(cs.I);
				STANDARD_ACC_OP(tempval, tempval);
				NEXT_ACC_A; CYCLES(2);
				break;

			/* SHR */
			case 0xeb:
			case 0xfb:
				tempval = (cs.acc_is_b == 0) ? (cs.A >> 1) : ASR(cs.B);
				tempval |= (ACC + (0xb0b | (opcode & 0xf0))) & 0x1000;
				STANDARD_ACC_OP(tempval, 0xb0b | (opcode & 0xf0));
				NEXT_ACC_A; CYCLES(1);
				break;

			/* SHL */
			case 0xec:
			case 0xfc:
				tempval = (ACC << 1) & 0xfff;
				tempval |= (ACC + (0xc0c | (opcode & 0xf0))) & 0x1000;
				STANDARD_ACC_OP(tempval, 0xc0c | (opcode & 0xf0));
				NEXT_ACC_A; CYCLES(1);
				break;

			/* ASR */
			case 0xed:
			case 0xfd:
				tempval = ASR(ACC);
				STANDARD_ACC_OP(tempval, 0xd0d | (opcode & 0xf0));
				NEXT_ACC_A; CYCLES(1);
				break;

			/* SHRB */
			case 0xee:
			case 0xfe:
				if (cs.acc_is_b == 0)
				{
					tempval = (cs.A >> 1) | ((cs.B << 11) & 0x800);
					cs.B = ASR(cs.B);
				}
				else
					tempval = ASR(cs.B);
				tempval |= (ACC + (0xe0e | (opcode & 0xf0))) & 0x1000;
				STANDARD_ACC_OP(tempval, 0xe0e | (opcode & 0xf0));
				NEXT_ACC_A; CYCLES(1);
				break;

			/* SHLB */
			case 0xef:
			case 0xff:
				if (cs.acc_is_b == 0)
				{
					tempval = (cs.A << 1) & 0xfff;
					cs.B = (cs.B << 1) & 0xfff;
				}
				else
					tempval = (cs.B << 1) & 0xfff;
				tempval |= (ACC + (0xf0f | (opcode & 0xf0))) & 0x1000;
				STANDARD_ACC_OP(tempval, 0xf0f | (opcode & 0xf0));
				NEXT_ACC_A; CYCLES(1);
				break;

			/* IV */
			case 0xf0:
				cs.X = sext12(cs.A);
				cs.Y = sext12(cs.B);
				NEXT_ACC_A; CYCLES(1);
				break;
		}
	} while (icount > 0);

	INT32 ran = cycles_start - icount;
	cs.total_cycles += ran;
	cycles_start = icount = 0;

	return ran;
}
