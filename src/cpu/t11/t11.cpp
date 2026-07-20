/*** t11: Portable DEC T-11 emulator ******************************************

    Copyright Aaron Giles

    System dependencies:    long must be at least 32 bits
                            word must be 16 bit unsigned int
                            byte must be 8 bit unsigned int
                            long must be more than 16 bits
                            arrays up to 65536 bytes must be supported
                            machine must be twos complement

MAME 0.149

*****************************************************************************/

#include "burnint.h"
#include "driver.h"
#include "t11_intf.h"

/*************************************
 *
 *  Internal state representation
 *
 *************************************/



struct t11_state
{
	PAIR                ppc;    /* previous program counter */
	PAIR                reg[8];
	PAIR                psw;
	UINT8               wait_state;
	UINT8               irq_state;
	int                 icount;
	int                 start_cycles;
	UINT32				total_cycles;
	int					end_run;
	// config
	UINT16              initial_pc;
	INT32 (*irq_callback)(INT32);
};

static t11_state t11_regs[1];
static t11_state *cpustate = NULL;

/*************************************
 *
 *  Macro shortcuts
 *
 *************************************/

/* registers of various sizes */
#define REGD(x) reg[x].d
#define REGW(x) reg[x].w.l
#define REGB(x) reg[x].b.l

/* PC, SP, and PSW definitions */
#define SP      REGW(6)
#define PC      REGW(7)
#define SPD     REGD(6)
#define PCD     REGD(7)
#define PSW     psw.b.l


/*************************************
 *
 *  Low-level memory operations
 *
 *************************************/

static inline int ROPCODE()
{
	cpustate->PC &= 0xfffe;
	int val = t11FetchWord(cpustate->PC);
	cpustate->PC += 2;
	return val;
}

static inline int RBYTE(int addr)
{
	return t11ReadByte(addr);
}

static inline void WBYTE(int addr, int data)
{
	t11WriteByte(addr, data);
}

static inline int RWORD(int addr)
{
	return t11ReadWord((addr) & 0xfffe);
}
static inline void WWORD(int addr, int data)
{
	t11WriteWord((addr) & 0xfffe, data);
}

/*************************************
 *
 *  Low-level stack operations
 *
 *************************************/

static inline void PUSH(int val)
{
	cpustate->SP -= 2;
	WWORD(cpustate->SPD, val);
}


static inline int POP()
{
	int result = RWORD(cpustate->SPD);
	cpustate->SP += 2;
	return result;
}



/*************************************
 *
 *  Flag definitions and operations
 *
 *************************************/

/* flag definitions */
#define CFLAG 1
#define VFLAG 2
#define ZFLAG 4
#define NFLAG 8

/* extracts flags */
#define GET_C (cpustate->PSW & CFLAG)
#define GET_V (cpustate->PSW & VFLAG)
#define GET_Z (cpustate->PSW & ZFLAG)
#define GET_N (cpustate->PSW & NFLAG)

/* clears flags */
#define CLR_C (cpustate->PSW &= ~CFLAG)
#define CLR_V (cpustate->PSW &= ~VFLAG)
#define CLR_Z (cpustate->PSW &= ~ZFLAG)
#define CLR_N (cpustate->PSW &= ~NFLAG)

/* sets flags */
#define SET_C (cpustate->PSW |= CFLAG)
#define SET_V (cpustate->PSW |= VFLAG)
#define SET_Z (cpustate->PSW |= ZFLAG)
#define SET_N (cpustate->PSW |= NFLAG)



/*************************************
 *
 *  Interrupt handling
 *
 *************************************/

struct irq_table_entry
{
	UINT8   priority;
	UINT8   vector;
};

static const struct irq_table_entry irq_table[] =
{
	{ 0<<5, 0x00 },
	{ 4<<5, 0x38 },
	{ 4<<5, 0x34 },
	{ 4<<5, 0x30 },
	{ 5<<5, 0x5c },
	{ 5<<5, 0x58 },
	{ 5<<5, 0x54 },
	{ 5<<5, 0x50 },
	{ 6<<5, 0x4c },
	{ 6<<5, 0x48 },
	{ 6<<5, 0x44 },
	{ 6<<5, 0x40 },
	{ 7<<5, 0x6c },
	{ 7<<5, 0x68 },
	{ 7<<5, 0x64 },
	{ 7<<5, 0x60 }
};

static void t11_check_irqs()
{
	const struct irq_table_entry *irq = &irq_table[cpustate->irq_state & 15];
	int priority = cpustate->PSW & 0xe0;

	/* compare the priority of the interrupt to the PSW */
	if (irq->priority > priority)
	{
		int vector = irq->vector;
		int new_pc, new_psw;

		/* call the callback; if we don't get -1 back, use the return value as our vector */
		if (cpustate->irq_callback != NULL)
		{
			int new_vector = (*cpustate->irq_callback)(cpustate->irq_state & 15);
			if (new_vector != -1)
				vector = new_vector;
		}
	   // bprintf(0, _T("T11: Take IRQ, vector %x\n"), vector);
		/* fetch the new PC and PSW from that vector */
		//assert((vector & 3) == 0);
		new_pc = RWORD(vector);
		new_psw = RWORD(vector + 2);

		/* push the old state, set the new one */
		PUSH(cpustate->PSW);
		PUSH(cpustate->PC);
		cpustate->PCD = new_pc;
		cpustate->PSW = new_psw;
		t11_check_irqs();

		/* count cycles and clear the WAIT flag */
		cpustate->icount -= 114;
		cpustate->wait_state = 0;
	}
}



/*************************************
 *
 *  Core opcodes
 *
 *************************************/

/* includes the static function prototypes and the master opcode table */
#include "t11table.c"

/* includes the actual opcode implementations */
#include "t11ops.c"



/*************************************
 *
 *  Low-level initialization/cleanup
 *
 *************************************/
 
static INT32 dummy_irqcallback(INT32) { return -1; }

void t11_core_init(INT32 mode, INT32 (*irqcallback)(INT32))
{
	static const UINT16 initial_pc[] =
	{
		0xc000, 0x8000, 0x4000, 0x2000,
		0x1000, 0x0000, 0xf600, 0xf400
	};

	cpustate = &t11_regs[0];

	cpustate->initial_pc = initial_pc[mode];
	cpustate->irq_callback = irqcallback ? irqcallback : dummy_irqcallback;
}



/*************************************
 *
 *  CPU reset
 *
 *************************************/

void t11Reset()
{
	memset(&t11_regs, 0, STRUCT_SIZE_HELPER(t11_state, end_run));

	/* initial SP is 376 octal, or 0xfe */
	cpustate->SP = 0x00fe;

	/* initial PC comes from the setup word */
	cpustate->PC = cpustate->initial_pc;

	/* PSW starts off at highest priority */
	cpustate->PSW = 0xe0;

	/* initialize the IRQ state */
	cpustate->irq_state = 0;

	/* reset the remaining state */
	cpustate->REGD(0) = 0;
	cpustate->REGD(1) = 0;
	cpustate->REGD(2) = 0;
	cpustate->REGD(3) = 0;
	cpustate->REGD(4) = 0;
	cpustate->REGD(5) = 0;
	cpustate->ppc.d = 0;
	cpustate->wait_state = 0;
}



/*************************************
 *
 *  Interrupt handling
 *
 *************************************/

void t11_set_irq_line(INT32 irqline, INT32 state)
{
	/* set the appropriate bit */
	if (state == CLEAR_LINE)
		cpustate->irq_state &= ~(1 << irqline);
	else
		cpustate->irq_state |= 1 << irqline;
}


/*************************************
 *
 *  Core execution
 *
 *************************************/

INT32 t11Run(INT32 nCycles)
{
	cpustate->end_run = 0;
	cpustate->icount = nCycles;
	cpustate->start_cycles = nCycles;

	t11_check_irqs();

	if (cpustate->wait_state) {
		cpustate->icount = 0;
	} else {
		do
		{
			UINT16 op;

			cpustate->ppc = cpustate->reg[7];   /* copy PC to previous PC */

			// debugger_instruction_hook(device, cpustate->PCD);

			op = ROPCODE();
			(*opcode_table[op >> 3])(op);

		} while (cpustate->icount > 0 && !cpustate->end_run);
	}

	nCycles = nCycles - cpustate->icount;
	cpustate->icount = cpustate->start_cycles = 0;
	cpustate->total_cycles += nCycles;

	return nCycles;
}

void t11NewFrame()
{
	cpustate->total_cycles = 0;
}

INT32 t11Idle(INT32 cycles)
{
	cpustate->total_cycles += cycles;

	return cycles;
}

void t11RunEnd()
{
	cpustate->end_run = 1;
}

INT32 t11TotalCycles()
{
	return cpustate->total_cycles + (cpustate->start_cycles - cpustate->icount);
}

INT32 t11Scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		ScanVar(t11_regs, STRUCT_SIZE_HELPER(t11_state, end_run), "DEC T11 CPU");
	}

	return 0;
}
