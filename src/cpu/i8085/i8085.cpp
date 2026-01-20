// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller, Roberto Fresca, Grull Osgo
// thanks-to:Marcel De Kogel
/*****************************************************************************
 *
 *   Portable I8085A emulator V1.3
 *
 *   Copyright Juergen Buchmueller
 *   Partially based on information out of Z80Em by Marcel De Kogel
 *
 *   TODO:
 *   - not sure if 8085 DSUB H flag is correct
 *   - accurate 8085 undocumented V/K flags
 *   - most of those is_8085 can probably be done better, like function overrides
 *
 * ---------------------------------------------------------------------------
 *
 * changes in V1.3
 *   - Added undocumented opcodes for the 8085A, based on a german
 *     book about microcomputers: "Mikrocomputertechnik mit dem
 *     Prozessor 8085A".
 *   - This book also suggest that INX/DCX should modify the X flag bit
 *     for a LSB to MSB carry and
 *   - that jumps take 10 T-states only when they're executed, 7 when
 *     they're skipped.
 *     Thanks for the info and a copy of the tables go to Timo Sachsenberg
 *     <timo.sachsenberg@student.uni-tuebingen.de>
 * changes in V1.2
 *   - corrected cycle counts for these classes of opcodes
 *     Thanks go to Jim Battle <frustum@pacbell.bet>
 *
 *                  808x     Z80
 *     DEC A           5       4    \
 *     INC A           5       4     \
 *     LD A,B          5       4      >-- Z80 is faster
 *     JP (HL)         5       4     /
 *     CALL cc,nnnn: 11/17   10/17  /
 *
 *     INC HL          5       6    \
 *     DEC HL          5       6     \
 *     LD SP,HL        5       6      \
 *     ADD HL,BC      10      11       \
 *     INC (HL)       10      11        >-- 8080 is faster
 *     DEC (HL)       10      11       /
 *     IN A,(#)       10      11      /
 *     OUT (#),A      10      11     /
 *     EX (SP),HL     18      19    /
 *
 * Revisions:
 *
 * xx-xx-2002 Acho A. Tang
 * - 8085 emulation was in fact never used. It's been treated as a plain 8080.
 * - protected IRQ0 vector from being overwritten
 * - modified interrupt handler to properly process 8085-specific IRQ's
 * - corrected interrupt masking, RIM and SIM behaviors according to Intel's documentation
 *
 * 20-Jul-2002 Krzysztof Strzecha
 * - SBB r instructions should affect parity flag.
 *   Fixed only for non x86 asm version (#define i8080_EXACT 1).
 *   There are probably more opcodes which should affect this flag, but don't.
 * - JPO nnnn and JPE nnnn opcodes in disassembler were misplaced. Fixed.
 * - Undocumented i8080 opcodes added:
 *   08h, 10h, 18h, 20h, 28h, 30h, 38h  -  NOP
 *   0CBh                               -  JMP
 *   0D9h                               -  RET
 *   0DDh, 0EDh, 0FDh                   -  CALL
 *   Thanks for the info go to Anton V. Ignatichev.
 *
 * 08-Dec-2002 Krzysztof Strzecha
 * - ADC r instructions should affect parity flag.
 *   Fixed only for non x86 asm version (#define i8080_EXACT 1).
 *   There are probably more opcodes which should affect this flag, but don't.
 *
 * 05-Sep-2003 Krzysztof Strzecha
 * - INR r, DCR r, ADD r, SUB r, CMP r instructions should affect parity flag.
 *   Fixed only for non x86 asm version (#define i8080_EXACT 1).
 *
 * 23-Dec-2006 Tomasz Slanina
 * - SIM fixed
 *
 * 28-Jan-2007 Zsolt Vasvari
 * - Removed archaic i8080_EXACT flag.
 *
 * 08-June-2008 Miodrag Milanovic
 * - Flag setting fix for some instructions and cycle count update
 *
 * August 2009, hap
 * - removed DAA table
 * - fixed accidental double memory reads due to macro overuse
 * - fixed cycle deduction on unconditional CALL / RET
 * - added cycle tables and cleaned up big switch source layout (1 tab = 4 spaces)
 * - removed HLT cycle eating (earlier, HLT after EI could theoretically fail)
 * - fixed parity flag on add/sub/cmp
 * - renamed temp register XX to official name WZ
 * - renamed flags from Z80 style S Z Y H X V N C  to  S Z X5 H X3 P V C, and
 *   fixed X5 / V flags where accidentally broken due to flag names confusion
 *
 * 21-Aug-2009, Curt Coder
 * - added 8080A variant
 * - refactored callbacks to use devcb
 *
 * October 2012, hap
 * - fixed H flag on subtraction opcodes
 * - on 8080, don't push the unsupported flags(X5, X3, V) to stack
 * - it passes on 8080/8085 CPU Exerciser (ref: http://www.idb.me.uk/sunhillow/8080.html
 *   tests only 8080 opcodes, link is dead so go via archive.org)
 *
 * April 2025, Roberto Fresca
 * - Reworked the DSUB (Double Subtraction) undocumented instruction.
 * - Reworked the RDEL (Rotate D and E Left with Carry) undocumented instruction.
 *   (ref: https://robertofresca.com/files/New_8085_instruction.pdf)
 *
 *****************************************************************************/

#include "burnint.h"
#include "i8085.h"

#define VERBOSE 0


/***************************************************************************
    CONSTANTS
***************************************************************************/
#define CLEAR_LINE 0

static const UINT8 SF             = 0x80;
static const UINT8 ZF             = 0x40;
static const UINT8 KF             = 0x20;
static const UINT8 HF             = 0x10;
static const UINT8 X3F            = 0x08;
static const UINT8 PF             = 0x04;
static const UINT8 VF             = 0x02;
static const UINT8 CF             = 0x01;

static const UINT8 IM_SID         = 0x80;
static const UINT8 IM_I75         = 0x40;
static const UINT8 IM_I65         = 0x20;
static const UINT8 IM_I55         = 0x10;
static const UINT8 IM_IE          = 0x08;
static const UINT8 IM_M75         = 0x04;
static const UINT8 IM_M65         = 0x02;
static const UINT8 IM_M55         = 0x01;

static const UINT16 ADDR_TRAP     = 0x0024;
static const UINT16 ADDR_RST55    = 0x002c;
static const UINT16 ADDR_RST65    = 0x0034;
static const UINT16 ADDR_RST75    = 0x003c;

static void set_status(UINT8 status);
static UINT8 read_mem(UINT32 a);
static void write_mem(UINT32 a, UINT8 v);
static void set_inte(int state);
static void set_sod(int state);
static void op_push(PAIR p);
static UINT8 read_inta();
static void execute_one(int opcode);
static void execute_set_input(int irqline, int state);

static unsigned population_count_32(UINT32 val)
{
	const UINT32 m1 = 0x55555555;
	const UINT32 m2 = 0x33333333;
	const UINT32 m4 = 0x0f0f0f0f;
	const UINT32 h01= 0x01010101;
	val -= (val >> 1) & m1;
	val = (val & m2) + ((val >> 2) & m2);
	val = (val + (val >> 4)) & m4;
	return unsigned((val * h01) >> 24);
}
static INT32 total_cycles;
static INT32 cycle_start;
static INT32 end_run;
static INT32 m_icount;

static PAIR m_PC,m_SP,m_AF,m_BC,m_DE,m_HL,m_WZ;
static UINT8 m_halt;
static UINT8 m_im;             // interrupt mask (8085A only)
static UINT8 m_status;         // status word

static UINT8 m_after_ei;       // post-EI processing; starts at 2, check for ints at 0
static UINT8 m_nmi_state;      // raw NMI line state
static UINT8 m_irq_state[4];   // raw IRQ line states
static UINT8 m_irq_hold[4];   // raw IRQ line states
static bool m_trap_pending; // TRAP interrupt latched?
static UINT8 m_trap_im_copy;   // copy of IM register when TRAP was taken
static UINT8 m_sod_state;      // state of the SOD line
static bool m_in_acknowledge;

// cycle table
static UINT8 lut_cycles[256];

// flags lookup
static UINT8 lut_zs[256];
static UINT8 lut_zsp[256];

static int cputype = 1;	/* 0 8080, 1 8085A */

static int ret_taken() { return 6; }
static int jmp_taken() { return (cputype == 1) ? 3 : 0; }
static int call_taken() { return (cputype == 1) ? 9 : 6; }
static bool is_8085() { return (cputype == 1) ? true : false; }

static void dummy_out(INT32 status) { }
static UINT8 dummy_in() { return 0; }
static INT32 dummy_irq_cb(INT32) { return 0xff; }

static void (*m_out_status_func)(INT32) = NULL;
static void (*m_out_inte_func)(INT32) = NULL;
static UINT8 (*m_in_sid_func)() = NULL;
static void (*m_out_sod_func)(INT32) = NULL;

static INT32 (*insn_callback)(INT32) = NULL;
static INT32 (*irq_callback)(INT32) = NULL;

static void set_dummy_funcs()
{
	m_out_status_func = dummy_out;
	m_out_inte_func = dummy_out;
	m_in_sid_func = dummy_in;
	m_out_sod_func = dummy_out;

	insn_callback = NULL;
	irq_callback = dummy_irq_cb;
}

static void dummy_write(UINT16,UINT8) {}
static UINT8 dummy_read(UINT16) { return 0; }

static void (*io_out_handler)(UINT16 port, UINT8 data) = NULL;
static UINT8 (*io_in_handler)(UINT16 port) = NULL;
static void (*program_write_handler)(UINT16 address, UINT8 data) = NULL;
static UINT8 (*program_read_handler)(UINT16 address) = NULL;

#define	io_write_byte_8	io_out_handler
#define	io_read_byte_8	io_in_handler

static UINT8 *mem[3][0x100];

static inline UINT8 cpu_readop(UINT16 a)
{
	if (mem[2][a >> 8]) {
		return mem[2][a >> 8][a & 0xff];
	}
	
	return program_read_handler(a);
}

static inline UINT8 cpu_readop_arg(UINT16 a)
{
	if (mem[0][a >> 8]) {
		return mem[0][a >> 8][a & 0xff];
	}
	
	return program_read_handler(a);
}

static inline UINT8 program_read_byte_8(UINT16 a)
{
	if (mem[0][a >> 8]) {
		return mem[0][a >> 8][a & 0xff];
	}
	
	return program_read_handler(a);
}

static inline void program_write_byte_8(UINT16 a, UINT8 d)
{
	if (mem[1][a >> 8]) {
		mem[1][a >> 8][a & 0xff] = d;
		return;
	}
	
	program_write_handler(a,d);
}

UINT8 i8085ReadByte(UINT16 a)
{
	return program_read_byte_8(a);
}

void i8085WriteByte(UINT16 address, UINT8 data)
{
	program_write_byte_8(address, data);
}

/***************************************************************************
    STATIC TABLES
***************************************************************************/

/* cycles lookup */
static const UINT8 lut_cycles_8080[256]={
/*      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  */
/* 0 */ 4, 10,7, 5, 5, 5, 7, 4, 4, 10,7, 5, 5, 5, 7, 4,
/* 1 */ 4, 10,7, 5, 5, 5, 7, 4, 4, 10,7, 5, 5, 5, 7, 4,
/* 2 */ 4, 10,16,5, 5, 5, 7, 4, 4, 10,16,5, 5, 5, 7, 4,
/* 3 */ 4, 10,13,5, 10,10,10,4, 4, 10,13,5, 5, 5, 7, 4,
/* 4 */ 5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
/* 5 */ 5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
/* 6 */ 5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
/* 7 */ 7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 5, 5, 5, 5, 7, 5,
/* 8 */ 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
/* 9 */ 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
/* A */ 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
/* B */ 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
/* C */ 5, 10,10,10,11,11,7, 11,5, 10,10,10,11,11,7, 11,
/* D */ 5, 10,10,10,11,11,7, 11,5, 10,10,10,11,11,7, 11,
/* E */ 5, 10,10,18,11,11,7, 11,5, 5, 10,4, 11,11,7, 11,
/* F */ 5, 10,10,4, 11,11,7, 11,5, 5, 10,4, 11,11,7, 11 };

static const UINT8 lut_cycles_8085[256]={
/*      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  */
/* 0 */ 4, 10,7, 6, 4, 4, 7, 4, 10,10,7, 6, 4, 4, 7, 4,
/* 1 */ 7, 10,7, 6, 4, 4, 7, 4, 10,10,7, 6, 4, 4, 7, 4,
/* 2 */ 4, 10,16,6, 4, 4, 7, 4, 10,10,16,6, 4, 4, 7, 4,
/* 3 */ 4, 10,13,6, 10,10,10,4, 10,10,13,6, 4, 4, 7, 4,
/* 4 */ 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
/* 5 */ 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
/* 6 */ 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
/* 7 */ 7, 7, 7, 7, 7, 7, 5, 7, 4, 4, 4, 4, 4, 4, 7, 4,
/* 8 */ 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
/* 9 */ 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
/* A */ 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
/* B */ 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
/* C */ 6, 10,7, 7, 9, 12,7, 12,6, 10,7, 6, 9, 9, 7, 12,
/* D */ 6, 10,7, 10,9, 12,7, 12,6, 10,7, 10,9, 7, 7, 12,
/* E */ 6, 10,7, 16,9, 12,7, 12,6, 6, 7, 4, 9, 10,7, 12,
/* F */ 6, 10,7, 4, 9, 12,7, 12,6, 6, 7, 4, 9, 7, 7, 12 };

/* special cases (partially taken care of elsewhere):
                base c    taken?
op_ret  8080    5         +6(11)    (conditional)
op_ret  8085    6         +6(12)    (conditional)
op_jmp  8080    10        +0
op_jmp  8085    7         +3(10)
op_call 8080    11        +6(17)
op_call 8085    9         +9(18)

*/


/***************************************************************************
    CORE INITIALIZATION
***************************************************************************/

static void init_tables()
{
	for (int i = 0; i < 256; i++)
	{
		// cycles
		lut_cycles[i] = is_8085() ? lut_cycles_8085[i] : lut_cycles_8080[i];

		// flags
		UINT8 zs = 0;
		if (i == 0) zs |= ZF;
		if (i & 0x80) zs |= SF;

		UINT8 p = (population_count_32(i) & 1) ? 0 : PF;

		lut_zs[i] = zs;
		lut_zsp[i] = zs | p;
	}
}

static void device_start()
{
	m_PC.d = 0;
	m_SP.d = 0;
	m_AF.d = 0;
	m_BC.d = 0;
	m_DE.d = 0;
	m_HL.d = 0;
	m_WZ.d = 0;
	m_halt = 0;
	m_im = 0;
	m_status = 0;
	m_after_ei = 0;
	m_nmi_state = 0;
	m_irq_state[3] = m_irq_state[2] = m_irq_state[1] = m_irq_state[0] = 0;
	m_irq_hold[3] = m_irq_hold[2] = m_irq_hold[1] = m_irq_hold[0] = 0;
	m_trap_pending = false;
	m_trap_im_copy = 0;
	m_sod_state = 1; // SOD will go low at reset
	m_in_acknowledge = false;

	init_tables();
	set_dummy_funcs();

	total_cycles = 0;
	cycle_start = 0;
	end_run = 0;

	m_icount = 0;
}


/***************************************************************************
    COMMON RESET
***************************************************************************/

static void device_reset()
{
	m_PC.d = 0;
	m_SP.d = 0;
	m_AF.d = 0;
	m_BC.d = 0;
	m_DE.d = 0;
	m_HL.d = 0;
	m_WZ.d = 0;
	m_halt = 0;
	m_im = 0;
	m_status = 0;
	m_after_ei = 0;
	m_nmi_state = 0;
	m_irq_state[3] = m_irq_state[2] = m_irq_state[1] = m_irq_state[0] = 0;
	m_irq_hold[3] = m_irq_hold[2] = m_irq_hold[1] = m_irq_hold[0] = 0;
	m_trap_pending = false;
	m_trap_im_copy = 0;
	m_sod_state = 1; // SOD will go low at reset
	m_in_acknowledge = false;

	m_PC.d = 0;
	m_halt = 0;
	m_im &= ~IM_I75;
	m_im |= IM_M55 | IM_M65 | IM_M75;
	m_after_ei = 0;
	m_trap_pending = false;
	m_trap_im_copy = 0;
	set_inte(0);
	set_sod(0);
}

void i8085SetIRQCallback(INT32 (*irqcallback)(INT32))
{
	irq_callback = irqcallback ? irqcallback : dummy_irq_cb;
}

void i8085SetCallback(INT32 (*callback)(INT32))
{
	insn_callback = callback;
}

void i8085Open(INT32)
{
}

void i8085Close()
{
}

void i8085Reset()
{
	device_reset();
}

void i8085Exit()
{
	m_out_status_func = NULL;
	m_out_inte_func = NULL;
	m_in_sid_func = NULL;
	m_out_sod_func = NULL;

	insn_callback = NULL;
	irq_callback = NULL;
}

void i8085SetIRQLine(INT32 irqline, INT32 state)
{
	execute_set_input(irqline, state);
}

static void core_set_irq_line(INT32, INT32 line, INT32 state)
{
	i8085SetIRQLine(line, state);
}


/***************************************************************************
    INTERRUPTS
***************************************************************************/

static void execute_set_input(int irqline, int state)
{
	int hold = (state == CPU_IRQSTATUS_HOLD || state == CPU_IRQSTATUS_AUTO);
	if (hold) {
		state = 1;
	}
	int newstate = (state != CLEAR_LINE);

	// TRAP is level and edge-triggered NMI
	if (irqline == I8085_TRAP_LINE)
	{
		if (!m_nmi_state && newstate)
			m_trap_pending = true;
		else if (!newstate)
			m_trap_pending = false;
		m_nmi_state = newstate;
	}

	// RST7.5 is edge-triggered
	else if (irqline == I8085_RST75_LINE)
	{
		if (!m_irq_state[I8085_RST75_LINE] && newstate)
			m_im |= IM_I75;
		m_irq_state[I8085_RST75_LINE] = newstate;
	}

	// remaining sources are level triggered
	else if (irqline < sizeof(m_irq_state)) {
		m_irq_state[irqline] = state;
		m_irq_hold[irqline] = hold;
	}
}

static void break_halt_for_interrupt()
{
	// de-halt if necessary
	if (m_halt)
	{
		m_PC.w.l++;
		m_halt = 0;
		set_status(0x26); // int ack while halt
	}
	else
		set_status(0x23); // int ack

	m_in_acknowledge = true;
}

static void check_for_interrupts()
{
	// TRAP is the highest priority
	if (m_trap_pending)
	{
		// the first RIM after a TRAP reflects the original IE state; remember it here,
		// setting the high bit to indicate it is valid
		m_trap_im_copy = m_im | 0x80;

		// reset the pending state
		m_trap_pending = false;

		// break out of HALT state and call the IRQ ack callback
		break_halt_for_interrupt();
//		standard_irq_callback(I8085_TRAP_LINE, m_PC.w.l);
		// push the PC and jump to $0024
		op_push(m_PC);
		set_inte(0);
		m_PC.w.l = ADDR_TRAP;
		m_icount -= 11;
	}

	// followed by RST7.5
	else if ((m_im & IM_I75) && !(m_im & IM_M75) && (m_im & IM_IE))
	{
		// reset the pending state (which is CPU-visible via the RIM instruction)
		m_im &= ~IM_I75;

		// break out of HALT state and call the IRQ ack callback
		break_halt_for_interrupt();
		//standard_irq_callback(I8085_RST75_LINE, m_PC.w.l);

		// push the PC and jump to $003C
		op_push(m_PC);
		set_inte(0);
		m_PC.w.l = ADDR_RST75;
		m_icount -= 11;
	}

	// followed by RST6.5
	else if (m_irq_state[I8085_RST65_LINE] && !(m_im & IM_M65) && (m_im & IM_IE))
	{
		// break out of HALT state and call the IRQ ack callback
		break_halt_for_interrupt();
		//standard_irq_callback(I8085_RST65_LINE, m_PC.w.l);

		// push the PC and jump to $0034
		op_push(m_PC);
		set_inte(0);
		m_PC.w.l = ADDR_RST65;
		m_icount -= 11;
	}

	// followed by RST5.5
	else if (m_irq_state[I8085_RST55_LINE] && !(m_im & IM_M55) && (m_im & IM_IE))
	{
		// break out of HALT state and call the IRQ ack callback
		break_halt_for_interrupt();
		//standard_irq_callback(I8085_RST55_LINE, m_PC.w.l);

		// push the PC and jump to $002C
		op_push(m_PC);
		set_inte(0);
		m_PC.w.l = ADDR_RST55;
		m_icount -= 11;
	}

	// followed by classic INTR
	else if (m_irq_state[I8085_INTR_LINE] && (m_im & IM_IE))
	{
		// break out of HALT state and call the IRQ ack callback
		//if (!m_in_inta_func.isunset())
		//	standard_irq_callback(I8085_INTR_LINE, m_PC.w.l);
		if (m_irq_hold[I8085_INTR_LINE]) {
			execute_set_input(I8085_INTR_LINE, 0);
		}
		break_halt_for_interrupt();

		UINT8 vector = read_inta();

		// use the resulting vector as an opcode to execute
		set_inte(0);
		//bprintf(0, _T("i8085 take int $%02x\n"), vector);
		execute_one(vector);
	}
}


/***************************************************************************
    OPCODE HELPERS
***************************************************************************/

static void set_sod(int state)
{
	if (state != 0 && m_sod_state == 0)
	{
		m_sod_state = 1;
		m_out_sod_func(m_sod_state);
	}
	else if (state == 0 && m_sod_state != 0)
	{
		m_sod_state = 0;
		m_out_sod_func(m_sod_state);
	}
}

static void set_inte(int state)
{
	if (state != 0 && (m_im & IM_IE) == 0)
	{
		m_im |= IM_IE;
		m_out_inte_func(1);
	}
	else if (state == 0 && (m_im & IM_IE) != 0)
	{
		m_im &= ~IM_IE;
		m_out_inte_func(0);
	}
}

static void set_status(UINT8 status)
{
	if (m_out_status_func != NULL && status != m_status)
		m_out_status_func(status);

	m_status = status;
}

static UINT8 get_rim_value()
{
	UINT8 result = m_im;
	int sid = m_in_sid_func();

	// copy live RST5.5 and RST6.5 states
	result &= ~(IM_I65 | IM_I55);
	if (m_irq_state[I8085_RST65_LINE]) result |= IM_I65;
	if (m_irq_state[I8085_RST55_LINE]) result |= IM_I55;

	// fetch the SID bit if we have a callback
	result = (result & ~IM_SID) | (sid ? IM_SID : 0);

	return result;
}

// memory access
static UINT8 read_arg()
{
	set_status(0x82); // memory read
	if (m_in_acknowledge)
		return read_inta();
	else
		return cpu_readop_arg(m_PC.w.l++);
}

static PAIR read_arg16()
{
	PAIR p;
	set_status(0x82); // memory read
	if (m_in_acknowledge)
	{
		p.b.l = read_inta();
		p.b.h = read_inta();
	}
	else
	{
		p.b.l = cpu_readop_arg(m_PC.w.l++);
		p.b.h = cpu_readop_arg(m_PC.w.l++);
	}
	return p;
}

static UINT8 read_op()
{
	set_status(0xa2); // instruction fetch
	return cpu_readop(m_PC.w.l++);
}

static UINT8 read_inta()
{
	return irq_callback(I8085_INTR_LINE); // returns vector (usually 0xff)

#if 0 // dink
	if (m_in_inta_func.isunset())
		return standard_irq_callback(I8085_INTR_LINE, m_PC.w.l);
	else
		return m_in_inta_func(m_PC.w.l);
#endif
}

static UINT8 read_mem(UINT32 a)
{
	set_status(0x82); // memory read
	return program_read_byte_8(a);
}

static void write_mem(UINT32 a, UINT8 v)
{
	set_status(0x00); // memory write
	program_write_byte_8(a, v);
}

static void op_push(PAIR p)
{
	set_status(0x04); // stack push
	program_write_byte_8(--m_SP.w.l, p.b.h);
	program_write_byte_8(--m_SP.w.l, p.b.l);
}

static PAIR op_pop()
{
	PAIR p;
	set_status(0x86); // stack pop
	p.b.l = program_read_byte_8(m_SP.w.l++);
	p.b.h = program_read_byte_8(m_SP.w.l++);
	return p;
}

// logical
static void op_ora(UINT8 v)
{
	m_AF.b.h |= v;
	m_AF.b.l = lut_zsp[m_AF.b.h];
}

static void op_xra(UINT8 v)
{
	m_AF.b.h ^= v;
	m_AF.b.l = lut_zsp[m_AF.b.h];
}

static void op_ana(UINT8 v)
{
	UINT8 hc = ((m_AF.b.h | v) << 1) & HF;
	m_AF.b.h &= v;
	m_AF.b.l = lut_zsp[m_AF.b.h];
	if (is_8085())
		m_AF.b.l |= HF;
	else
		m_AF.b.l |= hc;
}

// increase / decrease
static UINT8 op_inr(UINT8 v)
{
	UINT8 hc = ((v & 0x0f) == 0x0f) ? HF : 0;
	m_AF.b.l = (m_AF.b.l & CF) | lut_zsp[++v] | hc;
	return v;
}

static UINT8 op_dcr(UINT8 v)
{
	UINT8 hc = ((v & 0x0f) != 0x00) ? HF : 0;
	m_AF.b.l = (m_AF.b.l & CF) | lut_zsp[--v] | hc | VF;
	return v;
}

// arithmetic
static void op_add(UINT8 v)
{
	int q = m_AF.b.h + v;
	m_AF.b.l = lut_zsp[q & 0xff] | ((q >> 8) & CF) | ((m_AF.b.h ^ q ^ v) & HF);
	m_AF.b.h = q;
}

static void op_adc(UINT8 v)
{
	int q = m_AF.b.h + v + (m_AF.b.l & CF);
	m_AF.b.l = lut_zsp[q & 0xff] | ((q >> 8) & CF) | ((m_AF.b.h ^ q ^ v) & HF);
	m_AF.b.h = q;
}

static void op_sub(UINT8 v)
{
	int q = m_AF.b.h - v;
	m_AF.b.l = lut_zsp[q & 0xff] | ((q >> 8) & CF) | (~(m_AF.b.h ^ q ^ v) & HF) | VF;
	m_AF.b.h = q;
}

static void op_sbb(UINT8 v)
{
	int q = m_AF.b.h - v - (m_AF.b.l & CF);
	m_AF.b.l = lut_zsp[q & 0xff] | ((q >> 8) & CF) | (~(m_AF.b.h ^ q ^ v) & HF) | VF;
	m_AF.b.h = q;
}

static void op_cmp(UINT8 v)
{
	int q = m_AF.b.h - v;
	m_AF.b.l = lut_zsp[q & 0xff] | ((q >> 8) & CF) | (~(m_AF.b.h ^ q ^ v) & HF) | VF;
}

static void op_dad(UINT16 v)
{
	int q = m_HL.w.l + v;
	m_AF.b.l = (m_AF.b.l & ~CF) | (q >> 16 & CF);
	m_HL.w.l = q;
}

// jumps
static void op_jmp(int cond)
{
	if (cond)
	{
		m_PC = read_arg16();
		m_icount -= jmp_taken();
	}
	else
	{
		m_PC.w.l += 2;
	}
}

static void op_call(int cond)
{
	if (cond)
	{
		PAIR p = read_arg16();
		m_icount -= call_taken();
		op_push(m_PC);
		m_PC = p;
	}
	else
	{
		m_PC.w.l += 2;
	}
}

static void op_ret(int cond)
{
	// conditional RET only
	if (cond)
	{
		m_icount -= ret_taken();
		m_PC = op_pop();
	}
}

static void op_rst(UINT8 v)
{
	op_push(m_PC);
	m_PC.d = 8 * v;
}


/***************************************************************************
    COMMON EXECUTION
***************************************************************************/

INT32 i8085Run(INT32 cycles)
{
	m_icount = cycles;
	cycle_start = cycles;
	end_run = 0;

	INT32 pICOUNT = m_icount;

	// check for TRAPs before diving in (can't do others because of after_ei)
	if (m_trap_pending || m_after_ei == 0)
		check_for_interrupts();

	if (insn_callback) {
		insn_callback(pICOUNT - m_icount);
	}

	do
	{
		pICOUNT = m_icount;

		// the instruction after an EI does not take an interrupt, so
		// we cannot check immediately; handle post-EI behavior here
		if (m_after_ei != 0 && --m_after_ei == 0)
			check_for_interrupts();

		m_in_acknowledge = false;
		//debugger_instruction_hook(m_PC.d);

		// here we go...
		execute_one(read_op());

		if (insn_callback) {
			insn_callback(pICOUNT - m_icount);
		}

	} while (m_icount > 0 && !end_run);

	cycles = cycles - m_icount;
	m_icount = cycle_start = 0;

	total_cycles += cycles;

	return cycles;
}

INT32 i8085GetActive()
{
	return 0;
}

INT32 i8085TotalCycles()
{
	return total_cycles + (cycle_start - m_icount);
}

void i8085NewFrame()
{
	total_cycles = 0;
}

INT32 i8085Idle(INT32 cycles)
{
	total_cycles += cycles;

	return cycles;
}

void i8085RunEnd()
{
	end_run = 1;
}

#define READ		0
#define WRITE		1
#define FETCH		2

void i8085MapMemory(UINT8 *ptr, UINT16 nStart, UINT16 nEnd, UINT8 flags)
{
	for (INT32 i = nStart / 0x100; i < (nEnd / 0x100) + 1; i++)
	{
		if (flags & (1 <<  READ)) mem[ READ][i] = ptr + ((i * 0x100) - nStart);
		if (flags & (1 << WRITE)) mem[WRITE][i] = ptr + ((i * 0x100) - nStart);
		if (flags & (1 << FETCH)) mem[FETCH][i] = ptr + ((i * 0x100) - nStart);
	}
}

void i8085SetWriteHandler(void (*write)(UINT16, UINT8))
{
	program_write_handler = write;
}

void i8085SetReadHandler(UINT8 (*read)(UINT16))
{
	program_read_handler = read;
}

void i8085SetOutHandler(void (*write)(UINT16, UINT8))
{
	io_out_handler = write;
}

void i8085SetInHandler(UINT8 (*read)(UINT16))
{
	io_in_handler = read;
}

INT32 i8085Scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA)
	{
		SCAN_VAR(total_cycles);
		SCAN_VAR(cycle_start);
		SCAN_VAR(end_run);
		SCAN_VAR(m_icount);

		SCAN_VAR(m_PC);
		SCAN_VAR(m_SP);
		SCAN_VAR(m_AF);
		SCAN_VAR(m_BC);
		SCAN_VAR(m_DE);
		SCAN_VAR(m_HL);
		SCAN_VAR(m_WZ);

		SCAN_VAR(m_halt);
		SCAN_VAR(m_im);
		SCAN_VAR(m_status);

		SCAN_VAR(m_after_ei);
		SCAN_VAR(m_nmi_state);
		SCAN_VAR(m_irq_state);
		SCAN_VAR(m_irq_hold);
		SCAN_VAR(m_trap_pending);
		SCAN_VAR(m_trap_im_copy);
		SCAN_VAR(m_sod_state);
		SCAN_VAR(m_in_acknowledge);

		//ScanVar(&I, STRUCT_SIZE_HELPER(struct i8085_Regs, filler), "i8085 cpu");
	}

	return 0;
}

static UINT8 i8085CheatRead(UINT32 a)
{
	return program_read_byte_8(a);
}

static void i8085WriteROM(UINT32 a, UINT8 d)
{
	if (mem[0][(a >> 8) & 0xff]) {
		mem[0][(a >> 8) & 0xff][a & 0xff] = d;
	}

	if (mem[1][(a >> 8) & 0xff]) {
		mem[1][(a >> 8) & 0xff][a & 0xff] = d;
	}

	if (mem[2][(a >> 8) & 0xff]) {
		mem[2][(a >> 8) & 0xff][a & 0xff] = d;
	}
}

cpu_core_config i8085Config =
{
	"i8085",
	i8085Open,
	i8085Close,
	i8085CheatRead,
	i8085WriteROM,
	i8085GetActive,
	i8085TotalCycles,
	i8085NewFrame,
	i8085Idle,
	core_set_irq_line,
	i8085Run,
	i8085RunEnd,
	i8085Reset,
	i8085Scan,
	i8085Exit,
	0x10000,
	0
};

cpu_core_config i8080Config =
{
	"i8080",
	i8085Open,
	i8085Close,
	i8085CheatRead,
	i8085WriteROM,
	i8085GetActive,
	i8085TotalCycles,
	i8085NewFrame,
	i8085Idle,
	core_set_irq_line,
	i8085Run,
	i8085RunEnd,
	i8085Reset,
	i8085Scan,
	i8085Exit,
	0x10000,
	0
};

static void common_init(INT32 type)
{
	memset (mem, 0, sizeof(mem));

	cputype = type; // 8085a = 1, 8080 = 0

	device_start();

	i8085SetWriteHandler(dummy_write);
	i8085SetReadHandler(dummy_read);
	i8085SetOutHandler(dummy_write);
	i8085SetInHandler(dummy_read);

	CpuCheatRegister(0, (cputype == 1) ? &i8085Config : &i8080Config);
}

void i8085Init()
{
	common_init(1);
}

void i8080Init()
{
	common_init(0);
}

static void execute_one(int opcode)
{
	m_icount -= lut_cycles[opcode];

	switch (opcode)
	{
		case 0x00: // NOP
			break;
		case 0x01: // LXI B,nnnn
			m_BC = read_arg16();
			break;
		case 0x02: // STAX B
			write_mem(m_BC.d, m_AF.b.h);
			break;
		case 0x03: // INX B
			m_BC.w.l++;
			if (is_8085())
			{
				if (m_BC.w.l == 0x0000)
					m_AF.b.l |= KF;
				else
					m_AF.b.l &= ~KF;
			}
			break;
		case 0x04: // INR B
			m_BC.b.h = op_inr(m_BC.b.h);
			break;
		case 0x05: // DCR B
			m_BC.b.h = op_dcr(m_BC.b.h);
			break;
		case 0x06: // MVI B,nn
			m_BC.b.h = read_arg();
			break;
		case 0x07: // RLC
			m_AF.b.h = (m_AF.b.h << 1) | (m_AF.b.h >> 7);
			m_AF.b.l = (m_AF.b.l & 0xfe) | (m_AF.b.h & CF);
			break;

		case 0x08: // 8085: undocumented DSUB (Double Subtraction, HL - BC)
			if (is_8085())
			{
				// Low byte subtraction: L = L - C
				int q_low = m_HL.b.l - m_BC.b.l;
				UINT8 res_low = q_low & 0xff;
				// Calculate flags for low byte
				m_AF.b.l = lut_zs[res_low]
					| ((q_low >> 8) & CF) // Carry
					| ((m_HL.b.l ^ res_low ^ m_BC.b.l) & HF) // Half Carry
					| (((m_BC.b.l ^ m_HL.b.l) & (m_HL.b.l ^ res_low) & SF)) >> 5; // Overflow
				m_HL.b.l = res_low;

				// High byte subtraction: H = H - B - carry_from_low
				int q_high = m_HL.b.h - m_BC.b.h - (m_AF.b.l & CF);
				UINT8 res_high = q_high & 0xff;
				// Calculate flags for high byte
				m_AF.b.l = lut_zs[res_high]
					| ((q_high >> 8) & CF) // Carry
					| ((m_HL.b.h ^ res_high ^ m_BC.b.h) & HF) // Half Carry
					| (((m_BC.b.h ^ m_HL.b.h) & (m_HL.b.h ^ res_high) & SF)) >> 5; // Overflow
				m_HL.b.h = res_high;

				// Set Zero flag based on 16-bit result
				m_AF.b.l = (m_AF.b.l & ~ZF) | (((m_HL.b.l | m_HL.b.h) == 0) ? ZF : 0);
			}
			break;
		case 0x09: // DAD B
			op_dad(m_BC.w.l);
			break;
		case 0x0a: // LDAX B
			m_AF.b.h = read_mem(m_BC.d);
			break;
		case 0x0b: // DCX B
			m_BC.w.l--;
			if (is_8085())
			{
				if (m_BC.w.l == 0xffff)
					m_AF.b.l |= KF;
				else
					m_AF.b.l &= ~KF;
			}
			break;
		case 0x0c: // INR C
			m_BC.b.l = op_inr(m_BC.b.l);
			break;
		case 0x0d: // DCR C
			m_BC.b.l = op_dcr(m_BC.b.l);
			break;
		case 0x0e: // MVI C,nn
			m_BC.b.l = read_arg();
			break;
		case 0x0f: // RRC
			m_AF.b.l = (m_AF.b.l & 0xfe) | (m_AF.b.h & CF);
			m_AF.b.h = (m_AF.b.h >> 1) | (m_AF.b.h << 7);
			break;

		case 0x10: // 8085: undocumented ARHL, otherwise undocumented NOP
			if (is_8085())
			{
				m_AF.b.l = (m_AF.b.l & ~CF) | (m_HL.b.l & CF);
				m_HL.w.l = (m_HL.w.l & 0x8000) | (m_HL.w.l >> 1);
			}
			break;
		case 0x11: // LXI D,nnnn
			m_DE = read_arg16();
			break;
		case 0x12: // STAX D
			write_mem(m_DE.d, m_AF.b.h);
			break;
		case 0x13: // INX D
			m_DE.w.l++;
			if (is_8085())
			{
				if (m_DE.w.l == 0x0000)
					m_AF.b.l |= KF;
				else
					m_AF.b.l &= ~KF;
			}
			break;
		case 0x14: // INR D
			m_DE.b.h = op_inr(m_DE.b.h);
			break;
		case 0x15: // DCR D
			m_DE.b.h = op_dcr(m_DE.b.h);
			break;
		case 0x16: // MVI D,nn
			m_DE.b.h = read_arg();
			break;
		case 0x17: // RAL
		{
			int c = m_AF.b.l & CF;
			m_AF.b.l = (m_AF.b.l & 0xfe) | (m_AF.b.h >> 7);
			m_AF.b.h = (m_AF.b.h << 1) | c;
			break;
		}

		case 0x18: // 8085: undocumented RDEL (Rotate D and E Left through Carry), otherwise undocumented NOP
			if (is_8085())
			{
				int c = m_AF.b.l & CF;                                // save old carry state
				m_AF.b.l = (m_AF.b.l & ~(CF | VF)) | (m_DE.b.h >> 7); // set new carry state
				m_DE.w.l = (m_DE.w.l << 1) | c;                       // rotate with carry
				if ((((m_DE.w.l >> 15) ^ m_AF.b.l) & CF) != 0)
					m_AF.b.l |= VF;
			}
			break;
		case 0x19: // DAD D
			op_dad(m_DE.w.l);
			break;
		case 0x1a: // LDAX D
			m_AF.b.h = read_mem(m_DE.d);
			break;
		case 0x1b: // DCX D
			m_DE.w.l--;
			if (is_8085())
			{
				if (m_DE.w.l == 0xffff)
					m_AF.b.l |= KF;
				else
					m_AF.b.l &= ~KF;
			}
			break;
		case 0x1c: // INR E
			m_DE.b.l = op_inr(m_DE.b.l);
			break;
		case 0x1d: // DCR E
			m_DE.b.l = op_dcr(m_DE.b.l);
			break;
		case 0x1e: // MVI E,nn
			m_DE.b.l = read_arg();
			break;
		case 0x1f: // RAR
		{
			int c = (m_AF.b.l & CF) << 7;
			m_AF.b.l = (m_AF.b.l & 0xfe) | (m_AF.b.h & CF);
			m_AF.b.h = (m_AF.b.h >> 1) | c;
			break;
		}

		case 0x20: // 8085: RIM, otherwise undocumented NOP
			if (is_8085())
			{
				m_AF.b.h = get_rim_value();

				// if we have remembered state from taking a TRAP, fix up the IE flag here
				if (m_trap_im_copy & 0x80)
					m_AF.b.h = (m_AF.b.h & ~IM_IE) | (m_trap_im_copy & IM_IE);
				m_trap_im_copy = 0;
			}
			break;
		case 0x21: // LXI H,nnnn
			m_HL = read_arg16();
			break;
		case 0x22: // SHLD nnnn
			m_WZ = read_arg16();
			write_mem(m_WZ.d, m_HL.b.l);
			m_WZ.w.l++;
			write_mem(m_WZ.d, m_HL.b.h);
			break;
		case 0x23: // INX H
			m_HL.w.l++;
			if (is_8085())
			{
				if (m_HL.w.l == 0x0000)
					m_AF.b.l |= KF;
				else
					m_AF.b.l &= ~KF;
			}
			break;
		case 0x24: // INR H
			m_HL.b.h = op_inr(m_HL.b.h);
			break;
		case 0x25: // DCR H
			m_HL.b.h = op_dcr(m_HL.b.h);
			break;
		case 0x26: // MVI H,nn
			m_HL.b.h = read_arg();
			break;
		case 0x27: // DAA
			m_WZ.b.h = m_AF.b.h;
			if ((m_AF.b.l & HF) || ((m_AF.b.h & 0xf) > 9))
				m_WZ.b.h += 6;
			if ((m_AF.b.l & CF) || (m_AF.b.h > 0x99))
				m_WZ.b.h += 0x60;

			m_AF.b.l = (m_AF.b.l & 0x23) | ((m_AF.b.h > 0x99) ? 1 : 0) | ((m_AF.b.h ^ m_WZ.b.h) & 0x10) | lut_zsp[m_WZ.b.h];
			m_AF.b.h = m_WZ.b.h;
			break;

		case 0x28: // 8085: undocumented LDHI nn, otherwise undocumented NOP
			if (is_8085())
			{
				m_WZ.d = read_arg();
				m_DE.d = (m_HL.d + m_WZ.d) & 0xffff;
			}
			break;
		case 0x29: // DAD H
			op_dad(m_HL.w.l);
			break;
		case 0x2a: // LHLD nnnn
			m_WZ = read_arg16();
			m_HL.b.l = read_mem(m_WZ.d);
			m_WZ.w.l++;
			m_HL.b.h = read_mem(m_WZ.d);
			break;
		case 0x2b: // DCX H
			m_HL.w.l--;
			if (is_8085())
			{
				if (m_HL.w.l == 0xffff)
					m_AF.b.l |= KF;
				else
					m_AF.b.l &= ~KF;
			}
			break;
		case 0x2c: // INR L
			m_HL.b.l = op_inr(m_HL.b.l);
			break;
		case 0x2d: // DCR L
			m_HL.b.l = op_dcr(m_HL.b.l);
			break;
		case 0x2e: // MVI L,nn
			m_HL.b.l = read_arg();
			break;
		case 0x2f: // CMA
			m_AF.b.h ^= 0xff;
			if (is_8085())
				m_AF.b.l |= VF;
			break;

		case 0x30: // 8085: SIM, otherwise undocumented NOP
			if (is_8085())
			{
				// if bit 3 is set, bits 0-2 become the new masks
				if (m_AF.b.h & 0x08)
				{
					m_im &= ~(IM_M55 | IM_M65 | IM_M75 | IM_I55 | IM_I65);
					m_im |= m_AF.b.h & (IM_M55 | IM_M65 | IM_M75);

					// update live state based on the new masks
					if ((m_im & IM_M55) == 0 && m_irq_state[I8085_RST55_LINE])
						m_im |= IM_I55;
					if ((m_im & IM_M65) == 0 && m_irq_state[I8085_RST65_LINE])
						m_im |= IM_I65;
				}

				// bit if 4 is set, the 7.5 flip-flop is cleared
				if (m_AF.b.h & 0x10)
					m_im &= ~IM_I75;

				// if bit 6 is set, then bit 7 is the new SOD state
				if (m_AF.b.h & 0x40)
					set_sod(m_AF.b.h >> 7);

				// check for revealed interrupts
				check_for_interrupts();
			}
			break;
		case 0x31: // LXI SP,nnnn
			m_SP = read_arg16();
			break;
		case 0x32: // STAX nnnn
			m_WZ = read_arg16();
			write_mem(m_WZ.d, m_AF.b.h);
			break;
		case 0x33: // INX SP
			m_SP.w.l++;
			if (is_8085())
			{
				if (m_SP.w.l == 0x0000)
					m_AF.b.l |= KF;
				else
					m_AF.b.l &= ~KF;
			}
			break;
		case 0x34: // INR M
			m_WZ.b.l = op_inr(read_mem(m_HL.d));
			write_mem(m_HL.d, m_WZ.b.l);
			break;
		case 0x35: // DCR M
			m_WZ.b.l = op_dcr(read_mem(m_HL.d));
			write_mem(m_HL.d, m_WZ.b.l);
			break;
		case 0x36: // MVI M,nn
			m_WZ.b.l = read_arg();
			write_mem(m_HL.d, m_WZ.b.l);
			break;
		case 0x37: // STC
			m_AF.b.l = (m_AF.b.l & 0xfe) | CF;
			break;

		case 0x38: // 8085: undocumented LDSI nn, otherwise undocumented NOP
			if (is_8085())
			{
				m_WZ.d = read_arg();
				m_DE.d = (m_SP.d + m_WZ.d) & 0xffff;
			}
			break;
		case 0x39: // DAD SP
			op_dad(m_SP.w.l);
			break;
		case 0x3a: // LDAX nnnn
			m_WZ = read_arg16();
			m_AF.b.h = read_mem(m_WZ.d);
			break;
		case 0x3b: // DCX SP
			m_SP.w.l--;
			if (is_8085())
			{
				if (m_SP.w.l == 0xffff)
					m_AF.b.l |= KF;
				else
					m_AF.b.l &= ~KF;
			}
			break;
		case 0x3c: // INR A
			m_AF.b.h = op_inr(m_AF.b.h);
			break;
		case 0x3d: // DCR A
			m_AF.b.h = op_dcr(m_AF.b.h);
			break;
		case 0x3e: // MVI A,nn
			m_AF.b.h = read_arg();
			break;
		case 0x3f: // CMC
			m_AF.b.l = (m_AF.b.l & 0xfe) | (~m_AF.b.l & CF);
			break;

		// MOV [B/C/D/E/H/L/M/A],[B/C/D/E/H/L/M/A]
		case 0x40: break; // MOV B,B
		case 0x41: m_BC.b.h = m_BC.b.l; break;
		case 0x42: m_BC.b.h = m_DE.b.h; break;
		case 0x43: m_BC.b.h = m_DE.b.l; break;
		case 0x44: m_BC.b.h = m_HL.b.h; break;
		case 0x45: m_BC.b.h = m_HL.b.l; break;
		case 0x46: m_BC.b.h = read_mem(m_HL.d); break;
		case 0x47: m_BC.b.h = m_AF.b.h; break;

		case 0x48: m_BC.b.l = m_BC.b.h; break;
		case 0x49: break; // MOV C,C
		case 0x4a: m_BC.b.l = m_DE.b.h; break;
		case 0x4b: m_BC.b.l = m_DE.b.l; break;
		case 0x4c: m_BC.b.l = m_HL.b.h; break;
		case 0x4d: m_BC.b.l = m_HL.b.l; break;
		case 0x4e: m_BC.b.l = read_mem(m_HL.d); break;
		case 0x4f: m_BC.b.l = m_AF.b.h; break;

		case 0x50: m_DE.b.h = m_BC.b.h; break;
		case 0x51: m_DE.b.h = m_BC.b.l; break;
		case 0x52: break; // MOV D,D
		case 0x53: m_DE.b.h = m_DE.b.l; break;
		case 0x54: m_DE.b.h = m_HL.b.h; break;
		case 0x55: m_DE.b.h = m_HL.b.l; break;
		case 0x56: m_DE.b.h = read_mem(m_HL.d); break;
		case 0x57: m_DE.b.h = m_AF.b.h; break;

		case 0x58: m_DE.b.l = m_BC.b.h; break;
		case 0x59: m_DE.b.l = m_BC.b.l; break;
		case 0x5a: m_DE.b.l = m_DE.b.h; break;
		case 0x5b: break; // MOV E,E
		case 0x5c: m_DE.b.l = m_HL.b.h; break;
		case 0x5d: m_DE.b.l = m_HL.b.l; break;
		case 0x5e: m_DE.b.l = read_mem(m_HL.d); break;
		case 0x5f: m_DE.b.l = m_AF.b.h; break;

		case 0x60: m_HL.b.h = m_BC.b.h; break;
		case 0x61: m_HL.b.h = m_BC.b.l; break;
		case 0x62: m_HL.b.h = m_DE.b.h; break;
		case 0x63: m_HL.b.h = m_DE.b.l; break;
		case 0x64: break; // MOV H,H
		case 0x65: m_HL.b.h = m_HL.b.l; break;
		case 0x66: m_HL.b.h = read_mem(m_HL.d); break;
		case 0x67: m_HL.b.h = m_AF.b.h; break;

		case 0x68: m_HL.b.l = m_BC.b.h; break;
		case 0x69: m_HL.b.l = m_BC.b.l; break;
		case 0x6a: m_HL.b.l = m_DE.b.h; break;
		case 0x6b: m_HL.b.l = m_DE.b.l; break;
		case 0x6c: m_HL.b.l = m_HL.b.h; break;
		case 0x6d: break; // MOV L,L
		case 0x6e: m_HL.b.l = read_mem(m_HL.d); break;
		case 0x6f: m_HL.b.l = m_AF.b.h; break;

		case 0x70: write_mem(m_HL.d, m_BC.b.h); break;
		case 0x71: write_mem(m_HL.d, m_BC.b.l); break;
		case 0x72: write_mem(m_HL.d, m_DE.b.h); break;
		case 0x73: write_mem(m_HL.d, m_DE.b.l); break;
		case 0x74: write_mem(m_HL.d, m_HL.b.h); break;
		case 0x75: write_mem(m_HL.d, m_HL.b.l); break;
		case 0x76: // HLT (instead of MOV M,M)
			m_PC.w.l--;
			m_halt = 1;
			set_status(0x8a); // halt acknowledge
			break;
		case 0x77: write_mem(m_HL.d, m_AF.b.h); break;

		case 0x78: m_AF.b.h = m_BC.b.h; break;
		case 0x79: m_AF.b.h = m_BC.b.l; break;
		case 0x7a: m_AF.b.h = m_DE.b.h; break;
		case 0x7b: m_AF.b.h = m_DE.b.l; break;
		case 0x7c: m_AF.b.h = m_HL.b.h; break;
		case 0x7d: m_AF.b.h = m_HL.b.l; break;
		case 0x7e: m_AF.b.h = read_mem(m_HL.d); break;
		case 0x7f: break; // MOV A,A

		// alu op [B/C/D/E/H/L/M/A]
		case 0x80: op_add(m_BC.b.h); break;
		case 0x81: op_add(m_BC.b.l); break;
		case 0x82: op_add(m_DE.b.h); break;
		case 0x83: op_add(m_DE.b.l); break;
		case 0x84: op_add(m_HL.b.h); break;
		case 0x85: op_add(m_HL.b.l); break;
		case 0x86: m_WZ.b.l = read_mem(m_HL.d); op_add(m_WZ.b.l); break;
		case 0x87: op_add(m_AF.b.h); break;

		case 0x88: op_adc(m_BC.b.h); break;
		case 0x89: op_adc(m_BC.b.l); break;
		case 0x8a: op_adc(m_DE.b.h); break;
		case 0x8b: op_adc(m_DE.b.l); break;
		case 0x8c: op_adc(m_HL.b.h); break;
		case 0x8d: op_adc(m_HL.b.l); break;
		case 0x8e: m_WZ.b.l = read_mem(m_HL.d); op_adc(m_WZ.b.l); break;
		case 0x8f: op_adc(m_AF.b.h); break;

		case 0x90: op_sub(m_BC.b.h); break;
		case 0x91: op_sub(m_BC.b.l); break;
		case 0x92: op_sub(m_DE.b.h); break;
		case 0x93: op_sub(m_DE.b.l); break;
		case 0x94: op_sub(m_HL.b.h); break;
		case 0x95: op_sub(m_HL.b.l); break;
		case 0x96: m_WZ.b.l = read_mem(m_HL.d); op_sub(m_WZ.b.l); break;
		case 0x97: op_sub(m_AF.b.h); break;

		case 0x98: op_sbb(m_BC.b.h); break;
		case 0x99: op_sbb(m_BC.b.l); break;
		case 0x9a: op_sbb(m_DE.b.h); break;
		case 0x9b: op_sbb(m_DE.b.l); break;
		case 0x9c: op_sbb(m_HL.b.h); break;
		case 0x9d: op_sbb(m_HL.b.l); break;
		case 0x9e: m_WZ.b.l = read_mem(m_HL.d); op_sbb(m_WZ.b.l); break;
		case 0x9f: op_sbb(m_AF.b.h); break;

		case 0xa0: op_ana(m_BC.b.h); break;
		case 0xa1: op_ana(m_BC.b.l); break;
		case 0xa2: op_ana(m_DE.b.h); break;
		case 0xa3: op_ana(m_DE.b.l); break;
		case 0xa4: op_ana(m_HL.b.h); break;
		case 0xa5: op_ana(m_HL.b.l); break;
		case 0xa6: m_WZ.b.l = read_mem(m_HL.d); op_ana(m_WZ.b.l); break;
		case 0xa7: op_ana(m_AF.b.h); break;

		case 0xa8: op_xra(m_BC.b.h); break;
		case 0xa9: op_xra(m_BC.b.l); break;
		case 0xaa: op_xra(m_DE.b.h); break;
		case 0xab: op_xra(m_DE.b.l); break;
		case 0xac: op_xra(m_HL.b.h); break;
		case 0xad: op_xra(m_HL.b.l); break;
		case 0xae: m_WZ.b.l = read_mem(m_HL.d); op_xra(m_WZ.b.l); break;
		case 0xaf: op_xra(m_AF.b.h); break;

		case 0xb0: op_ora(m_BC.b.h); break;
		case 0xb1: op_ora(m_BC.b.l); break;
		case 0xb2: op_ora(m_DE.b.h); break;
		case 0xb3: op_ora(m_DE.b.l); break;
		case 0xb4: op_ora(m_HL.b.h); break;
		case 0xb5: op_ora(m_HL.b.l); break;
		case 0xb6: m_WZ.b.l = read_mem(m_HL.d); op_ora(m_WZ.b.l); break;
		case 0xb7: op_ora(m_AF.b.h); break;

		case 0xb8: op_cmp(m_BC.b.h); break;
		case 0xb9: op_cmp(m_BC.b.l); break;
		case 0xba: op_cmp(m_DE.b.h); break;
		case 0xbb: op_cmp(m_DE.b.l); break;
		case 0xbc: op_cmp(m_HL.b.h); break;
		case 0xbd: op_cmp(m_HL.b.l); break;
		case 0xbe: m_WZ.b.l = read_mem(m_HL.d); op_cmp(m_WZ.b.l); break;
		case 0xbf: op_cmp(m_AF.b.h); break;

		case 0xc0: // RNZ
			op_ret(!(m_AF.b.l & ZF));
			break;
		case 0xc1: // POP B
			m_BC = op_pop();
			break;
		case 0xc2: // JNZ nnnn
			op_jmp(!(m_AF.b.l & ZF));
			break;
		case 0xc3: // JMP nnnn
			op_jmp(1);
			break;
		case 0xc4: // CNZ nnnn
			op_call(!(m_AF.b.l & ZF));
			break;
		case 0xc5: // PUSH B
			op_push(m_BC);
			break;
		case 0xc6: // ADI nn
			m_WZ.b.l = read_arg();
			op_add(m_WZ.b.l);
			break;
		case 0xc7: // RST 0
			op_rst(0);
			break;

		case 0xc8: // RZ
			op_ret(m_AF.b.l & ZF);
			break;
		case 0xc9: // RET
			m_PC = op_pop();
			break;
		case 0xca: // JZ  nnnn
			op_jmp(m_AF.b.l & ZF);
			break;
		case 0xcb: // 8085: undocumented RSTV, otherwise undocumented JMP nnnn
			if (is_8085())
			{
				if (m_AF.b.l & VF)
				{
					// RST taken = 6 more cycles
					m_icount -= ret_taken();
					op_rst(8);
				}
			}
			else
				op_jmp(1);
			break;
		case 0xcc: // CZ  nnnn
			op_call(m_AF.b.l & ZF);
			break;
		case 0xcd: // CALL nnnn
			op_call(1);
			break;
		case 0xce: // ACI nn
			m_WZ.b.l = read_arg();
			op_adc(m_WZ.b.l);
			break;
		case 0xcf: // RST 1
			op_rst(1);
			break;

		case 0xd0: // RNC
			op_ret(!(m_AF.b.l & CF));
			break;
		case 0xd1: // POP D
			m_DE = op_pop();
			break;
		case 0xd2: // JNC nnnn
			op_jmp(!(m_AF.b.l & CF));
			break;
		case 0xd3: // OUT nn
			set_status(0x10);
			m_WZ.d = read_arg();
			io_write_byte_8(m_WZ.d, m_AF.b.h);
			break;
		case 0xd4: // CNC nnnn
			op_call(!(m_AF.b.l & CF));
			break;
		case 0xd5: // PUSH D
			op_push(m_DE);
			break;
		case 0xd6: // SUI nn
			m_WZ.b.l = read_arg();
			op_sub(m_WZ.b.l);
			break;
		case 0xd7: // RST 2
			op_rst(2);
			break;

		case 0xd8: // RC
			op_ret(m_AF.b.l & CF);
			break;
		case 0xd9: // 8085: undocumented SHLX, otherwise undocumented RET
			if (is_8085())
			{
				m_WZ.w.l = m_DE.w.l;
				write_mem(m_WZ.d, m_HL.b.l);
				m_WZ.w.l++;
				write_mem(m_WZ.d, m_HL.b.h);
			}
			else
				m_PC = op_pop();
			break;
		case 0xda: // JC nnnn
			op_jmp(m_AF.b.l & CF);
			break;
		case 0xdb: // IN nn
			set_status(0x42);
			m_WZ.d = read_arg();
			m_AF.b.h = io_read_byte_8(m_WZ.d);
			break;
		case 0xdc: // CC nnnn
			op_call(m_AF.b.l & CF);
			break;
		case 0xdd: // 8085: undocumented JNX5 nnnn, otherwise undocumented CALL nnnn
			if (is_8085())
				op_jmp(!(m_AF.b.l & KF));
			else
				op_call(1);
			break;
		case 0xde: // SBI nn
			m_WZ.b.l = read_arg();
			op_sbb(m_WZ.b.l);
			break;
		case 0xdf: // RST 3
			op_rst(3);
			break;

		case 0xe0: // RPO
			op_ret(!(m_AF.b.l & PF));
			break;
		case 0xe1: // POP H
			m_HL = op_pop();
			break;
		case 0xe2: // JPO nnnn
			op_jmp(!(m_AF.b.l & PF));
			break;
		case 0xe3: // XTHL
			m_WZ = op_pop();
			op_push(m_HL);
			m_HL.d = m_WZ.d;
			break;
		case 0xe4: // CPO nnnn
			op_call(!(m_AF.b.l & PF));
			break;
		case 0xe5: // PUSH H
			op_push(m_HL);
			break;
		case 0xe6: // ANI nn
			m_WZ.b.l = read_arg();
			op_ana(m_WZ.b.l);
			break;
		case 0xe7: // RST 4
			op_rst(4);
			break;

		case 0xe8: // RPE
			op_ret(m_AF.b.l & PF);
			break;
		case 0xe9: // PCHL
			m_PC.d = m_HL.w.l;
			break;
		case 0xea: // JPE nnnn
			op_jmp(m_AF.b.l & PF);
			break;
		case 0xeb: // XCHG
			m_WZ.d = m_DE.d;
			m_DE.d = m_HL.d;
			m_HL.d = m_WZ.d;
			break;
		case 0xec: // CPE nnnn
			op_call(m_AF.b.l & PF);
			break;
		case 0xed: // 8085: undocumented LHLX, otherwise undocumented CALL nnnn
			if (is_8085())
			{
				m_WZ.w.l = m_DE.w.l;
				m_HL.b.l = read_mem(m_WZ.d);
				m_WZ.w.l++;
				m_HL.b.h = read_mem(m_WZ.d);
			}
			else
				op_call(1);
			break;
		case 0xee: // XRI nn
			m_WZ.b.l = read_arg();
			op_xra(m_WZ.b.l);
			break;
		case 0xef: // RST 5
			op_rst(5);
			break;

		case 0xf0: // RP
			op_ret(!(m_AF.b.l & SF));
			break;
		case 0xf1: // POP A
			m_AF = op_pop();
			break;
		case 0xf2: // JP nnnn
			op_jmp(!(m_AF.b.l & SF));
			break;
		case 0xf3: // DI
			set_inte(0);
			break;
		case 0xf4: // CP nnnn
			op_call(!(m_AF.b.l & SF));
			break;
		case 0xf5: // PUSH A
			// X3F is always 0, and on 8080, VF=1 and KF=0
			// (we don't have to check for it elsewhere)
			m_AF.b.l &= ~X3F;
			if (!is_8085())
				m_AF.b.l = (m_AF.b.l & ~KF) | VF;
			op_push(m_AF);
			break;
		case 0xf6: // ORI nn
			m_WZ.b.l = read_arg();
			op_ora(m_WZ.b.l);
			break;
		case 0xf7: // RST 6
			op_rst(6);
			break;

		case 0xf8: // RM
			op_ret(m_AF.b.l & SF);
			break;
		case 0xf9: // SPHL
			m_SP.d = m_HL.d;
			break;
		case 0xfa: // JM nnnn
			op_jmp(m_AF.b.l & SF);
			break;
		case 0xfb: // EI
			set_inte(1);
			m_after_ei = 2;
			break;
		case 0xfc: // CM nnnn
			op_call(m_AF.b.l & SF);
			break;
		case 0xfd: // 8085: undocumented JX5 nnnn, otherwise undocumented CALL nnnn
			if (is_8085())
				op_jmp(m_AF.b.l & KF);
			else
				op_call(1);
			break;
		case 0xfe: // CPI nn
			m_WZ.b.l = read_arg();
			op_cmp(m_WZ.b.l);
			break;
		case 0xff: // RST 7
			op_rst(7);
			break;
	} // end big switch
}
