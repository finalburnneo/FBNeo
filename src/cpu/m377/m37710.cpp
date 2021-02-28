/*
    Mitsubishi M37702/37710 CPU Emulator

    The 7700 series is based on the WDC 65C816 core, with the following
    notable changes:

    - Second accumulator called "B" (on the 65816, "A" and "B" were the
      two 8-bit halves of the 16-bit "C" accumulator).
    - 6502 emulation mode and XCE instruction are not present.
    - No NMI line.  BRK and the watchdog interrupt are non-maskable, but there
      is no provision for the traditional 6502/65816 NMI line.
    - 3-bit interrupt priority levels like the 68000.  Interrupts in general
      are very different from the 65816.
    - New single-instruction immediate-to-memory move instructions (LDM)
      replaces STZ.
    - CLM and SEM (clear and set "M" status bit) replace CLD/SED.  Decimal
      mode is still available via REP/SEP instructions.
    - INC and DEC (0x1A and 0x3A) switch places for no particular reason.
    - The microcode bug that caused MVN/NVP to take 2 extra cycles per byte
      on the 65816 seems to have been fixed.
    - The WDM (0x42) and BIT immediate (0x89) instructions are now prefixes.
      0x42 when used before an instruction involving the A accumulator makes
      it use the B accumulator instead.  0x89 adds multiply and divide
      opcodes, which the real 65816 doesn't have.
    - The 65C816 preserves the upper 8 bits of A when in 8-bit M mode, but
      not the upper 8 bits of X or Y when in 8-bit X.  The 7700 preserves
      the top bits of all registers in all modes (code in the C74 BIOS
      starting at d881 requires this!).

    The various 7700 series models differ primarily by their on board
    peripherals.  The 7750 and later models do include some additional
    instructions, vs. the 770x/1x/2x, notably signed multiply/divide and
    sign extension opcodes.

    Peripherals common across the 7700 series include: programmable timers,
    digital I/O ports, and analog to digital converters.

    Reference: 7700 Family Software User's Manual (instruction set)
               7702/7703 Family User's Manual (on-board peripherals)
           7720 Family User's Manual

    Emulator by R. Belmont.
    Based on G65816 Emulator by Karl Stenrud.

    History:
    - v1.0  RB  First version, basic operation OK, timers not complete
    - v1.1  RB  Data bus is 16-bit, dozens of bugfixes to IRQs, opcodes,
                    and opcode mapping.  New opcodes added, internal timers added.
    - v1.2  RB  Fixed execution outside of bank 0, fixed LDM outside of bank 0,
                fixed so top 8 bits of X & Y are preserved while in 8-bit mode,
        added save state support.
*/

#include "burnint.h"
#include "driver.h"
#include "m37710.h"
#include "m377_intf.h"

#define M37710_DEBUG    (0) // enables verbose logging for peripherals, etc.

// forwards
static void m37710_clock_timers(INT32 clkcnt);
static void m37710i_interrupt_software(UINT32 vector);
static void m37710i_set_execution_mode(UINT32 mode);
static void m37710_set_irq_line(int line, int state);
static void m37710i_update_irqs();
static void m37710_internal_w(int offset, UINT8 data);
static UINT8 m37710_internal_r(int offset);

// FBNeo notes:
// Auto-mapped by the core:
// Internal regs mapped @ 0x000000 - 0x00007f
// Internal RAM  mapped @ 0x000080 - 0x00087f
// always maps 0x800 @ 0x80 to handle all chip variants.

#if 0
// On-board RAM, ROM, and peripherals

// M37702M2: 512 bytes internal RAM, 16K internal mask ROM
// (M37702E2: same with EPROM instead of mask ROM)
DEVICE_ADDRESS_MAP_START( map, 16, m37702m2_device )
	AM_RANGE(0x000000, 0x00007f) AM_READWRITE(m37710_internal_word_r, m37710_internal_word_w)
	AM_RANGE(0x000080, 0x00027f) AM_RAM
	AM_RANGE(0x00c000, 0x00ffff) AM_ROM AM_REGION(M37710_INTERNAL_ROM_REGION, 0)
ADDRESS_MAP_END


// M37702S1: 512 bytes internal RAM, no internal ROM
DEVICE_ADDRESS_MAP_START( map, 16, m37702s1_device )
	AM_RANGE(0x000000, 0x00007f) AM_READWRITE(m37710_internal_word_r, m37710_internal_word_w)
	AM_RANGE(0x000080, 0x00027f) AM_RAM
ADDRESS_MAP_END


// M37710S4: 2048 bytes internal RAM, no internal ROM
DEVICE_ADDRESS_MAP_START( map, 16, m37710s4_device )
	AM_RANGE(0x000000, 0x00007f) AM_READWRITE(m37710_internal_word_r, m37710_internal_word_w)
	AM_RANGE(0x000080, 0x00087f) AM_RAM
ADDRESS_MAP_END

#endif

#define page_size	0x80
#define page_mask	0x7f
#define address_mask 0xffffff

static UINT8 **mem[3];
static UINT8 *internal_ram = NULL;

static UINT8  (*M377_read8)(UINT32) = NULL;
static UINT16 (*M377_read16)(UINT32) = NULL;
static void (*M377_write8)(UINT32,UINT8) = NULL;
static void (*M377_write16)(UINT32,UINT16) = NULL;

static UINT8  (*M377_ioread8)(UINT32) = NULL;
static void (*M377_iowrite8)(UINT32,UINT8) = NULL;

void M377SetWritePortHandler(void (*write)(UINT32,UINT8))
{
	M377_iowrite8 = write;
}

void M377SetReadPortHandler(UINT8  (*read)(UINT32))
{
	M377_ioread8 = read;
}

void M377SetWriteByteHandler(void (*write)(UINT32,UINT8))
{
	M377_write8 = write;
}

void M377SetWriteWordHandler(void (*write)(UINT32,UINT16))
{
	M377_write16 = write;
}

void M377SetReadByteHandler(UINT8  (*read)(UINT32))
{
	M377_read8 = read;
}

void M377SetReadWordHandler(UINT16 (*read)(UINT32))
{
	M377_read16 = read;
}

void M377MapMemory(UINT8 *ptr, UINT64 start, UINT64 end, UINT32 flags)
{
	for (UINT64 i = start; i < end; i+= page_size)
	{
		if (flags & 1) mem[0][i/page_size] = (ptr == NULL) ? NULL : (ptr + (i - start));
		if (flags & 2) mem[1][i/page_size] = (ptr == NULL) ? NULL : (ptr + (i - start));
		if (flags & 4) mem[2][i/page_size] = (ptr == NULL) ? NULL : (ptr + (i - start));
	}
}

static UINT8 io_read_byte(UINT32 a)
{
	if (M377_ioread8) {
		return M377_ioread8(a);
	}

	return 0;
}

static void io_write_byte(UINT32 a, UINT8 d)
{
	if (M377_iowrite8) {
		return M377_iowrite8(a,d);
	}
}

static UINT16 program_read_word_16le(UINT32 a)
{
	a &= address_mask;

	UINT8 *p = mem[0][a / page_size];

	if (p) {
		UINT16 *z = (UINT16*)(p + (a & page_mask));
#ifdef LOG_MEM
		bprintf (0, _T("PRW: %6.6x %4.4x\n"), a, *z);
#endif

		return BURN_ENDIAN_SWAP_INT16(*z);
	}

	if (M377_read16) {
#ifdef LOG_MEM
		bprintf (0, _T("PRW: %6.6x %4.4x\n"), a, M377_read16(a));
#endif
		if (a < 0x80) {
			UINT16 offset = a >> 1;
			UINT16 ret = m37710_internal_r(offset * 2 + 0);
			ret |= m37710_internal_r(offset * 2 + 1) << 8;
			return ret;
		}

		return M377_read16(a);
	}

	return 0;
}

static UINT8 program_read_byte_16le(UINT32 a)
{
	a &= address_mask;

#ifdef LOG_MEM
	bprintf (0, _T("PRB: %6.6x\n"), a);
#endif

	if (mem[0][a / page_size]) {
		return mem[0][a / page_size][a & page_mask];
	}

	if (M377_read8) {

		if (a < 0x80) {
			return m37710_internal_r(a);
		}

		return M377_read8(a);
	}

	return 0;
}

static void program_write_word_16le(UINT32 a, UINT16 d)
{
	a &= address_mask;
#ifdef LOG_MEM
	bprintf (0, _T("PWW: %6.6x %4.4x\n"), a,d);
#endif

	UINT8 *p = mem[1][a / page_size];

	if (p) {
		UINT16 *z = (UINT16*)(p + (a & page_mask));
		*z = BURN_ENDIAN_SWAP_INT16(d);
		return;
	}

	if (M377_write16) {

		if (a < 0x80) {
			UINT16 offset = a >> 1;
			m37710_internal_w(offset * 2 + 0, d & 0xff);
			m37710_internal_w(offset * 2 + 1, d >> 8);
			return;
		}

		M377_write16(a,d);
		return;
	}
}

static void program_write_byte_16le(UINT32 a, UINT8 d)
{
	a &= address_mask;
#ifdef LOG_MEM
	bprintf (0, _T("PWB: %6.6x %2.2x\n"), a,d);
#endif

	if (mem[1][a / page_size]) {
		mem[1][a / page_size][a & page_mask] = d;
		return;
	}

	if (M377_write8) {

		if (a < 0x80) {
			m37710_internal_w(a, d);
			return;
		}

		return M377_write8(a,d);
	}
}

void M377WriteWord(UINT32 address, UINT16 data)
{
	program_write_word_16le(address, data);
}

void M377WriteByte(UINT32 address, UINT8 data)
{
	program_write_byte_16le(address, data);
}

UINT16 M377ReadWord(UINT32 address)
{
	return program_read_word_16le(address);
}

UINT8 M377ReadByte(UINT32 address)
{
	return program_read_byte_16le(address);
}

static void core_set_irq(INT32 /*cpu*/, INT32 line, INT32 state)
{
	M377SetIRQLine(line, state);
}

cpu_core_config M377Config =
{
	"M377xx",
	M377Open,
	M377Close,
	M377ReadByte,  //M377CheatRead,
	M377WriteByte, //M377WriteROM,
	M377GetActive,
	M377TotalCycles,
	M377NewFrame,
	M377Idle,
	core_set_irq,
	M377Run,
	M377RunEnd,
	M377Reset,
	0x1000000,
	0
};

struct m377_struct {
	UINT32 a;         /* Accumulator */
	UINT32 b;         /* holds high byte of accumulator */
	UINT32 ba;        /* Secondary Accumulator */
	UINT32 bb;        /* holds high byte of secondary accumulator */
	UINT32 x;         /* Index Register X */
	UINT32 y;         /* Index Register Y */
	UINT32 xh;        /* holds high byte of x */
	UINT32 yh;        /* holds high byte of y */
	UINT32 s;         /* Stack Pointer */
	UINT32 pc;        /* Program Counter */
	UINT32 ppc;       /* Previous Program Counter */
	UINT32 pb;        /* Program Bank (shifted left 16) */
	UINT32 db;        /* Data Bank (shifted left 16) */
	UINT32 d;         /* Direct Register */
	UINT32 flag_e;        /* Emulation Mode Flag */
	UINT32 flag_m;        /* Memory/Accumulator Select Flag */
	UINT32 flag_x;        /* Index Select Flag */
	UINT32 flag_n;        /* Negative Flag */
	UINT32 flag_v;        /* Overflow Flag */
	UINT32 flag_d;        /* Decimal Mode Flag */
	UINT32 flag_i;        /* Interrupt Mask Flag */
	UINT32 flag_z;        /* Zero Flag (inverted) */
	UINT32 flag_c;        /* Carry Flag */
	UINT32 line_irq;      /* Bitmask of pending IRQs */
	UINT32 ipl;       /* Interrupt priority level (top of PSW) */
	UINT32 ir;        /* Instruction Register */
	UINT32 im;        /* Immediate load value */
	UINT32 im2;       /* Immediate load target */
	UINT32 im3;       /* Immediate load target */
	UINT32 im4;       /* Immediate load target */
	UINT32 irq_delay;     /* delay 1 instruction before checking irq */
	UINT32 irq_level;     /* irq level */
	INT32 ICount;     	  /* cycle count */
	UINT32 source;        /* temp register */
	UINT32 destination;   /* temp register */
	UINT32 stopped;       /* Sets how the CPU is stopped */

	// on-board peripheral stuff
	UINT8 m37710_regs[128];
	INT32 reload[8];
	INT32 timers[8];

	// for debugger
	UINT32 debugger_pc;
	UINT32 debugger_pb;
	UINT32 debugger_db;
	UINT32 debugger_p;
	UINT32 debugger_a;
	UINT32 debugger_b;

	// DINK
	INT32 end_run;
	INT32 total_cycles;
	INT32 segment_cycles;
};

static m377_struct m377; // cpu!

// Statics
typedef void (*opcode_func)();
typedef UINT32 (*get_reg_func)(int regnum);
typedef void (*set_reg_func)(int regnum, UINT32 val);
typedef void (*set_line_func)(int line, int state);
typedef int  (*execute_func)(int cycles);

static const opcode_func *m_opcodes;    /* opcodes with no prefix */
static const opcode_func *m_opcodes42;  /* opcodes with 0x42 prefix */
static const opcode_func *m_opcodes89;  /* opcodes with 0x89 prefix */
static get_reg_func m_get_reg;
static set_reg_func m_set_reg;
static set_line_func m_set_line;
static execute_func m_execute;


/* interrupt control mapping */

static const int m37710_irq_levels[M37710_LINE_MAX] =
{
	// maskable
	0x70,   // ADC           0
	0x73,   // UART 1 XMIT   1
	0x74,   // UART 1 RECV   2
	0x71,   // UART 0 XMIT   3
	0x72,   // UART 0 RECV   4
	0x7c,   // Timer B2      5
	0x7b,   // Timer B1      6
	0x7a,   // Timer B0      7
	0x79,   // Timer A4      8
	0x78,   // Timer A3      9
	0x77,   // Timer A2      10
	0x76,   // Timer A1      11
	0x75,   // Timer A0      12
	0x7f,   // IRQ 2         13
	0x7e,   // IRQ 1         14
	0x7d,   // IRQ 0         15

	// non-maskable
	0,  // watchdog
	0,  // debugger control
	0,  // BRK
	0,  // divide by zero
	0,  // reset
};

static const int m37710_irq_vectors[M37710_LINE_MAX] =
{
	// maskable
	0xffd6, // A-D converter
	0xffd8, // UART1 transmit
	0xffda, // UART1 receive
	0xffdc, // UART0 transmit
	0xffde, // UART0 receive
	0xffe0, // Timer B2
	0xffe2, // Timer B1
	0xffe4, // Timer B0
	0xffe6, // Timer A4
	0xffe8, // Timer A3
	0xffea, // Timer A2
	0xffec, // Timer A1
	0xffee, // Timer A0
	0xfff0, // external INT2 pin
	0xfff2, // external INT1 pin
	0xfff4, // external INT0 pin

	// non-maskable
	0xfff6, // watchdog timer
	0xfff8, // debugger control (not used in shipping ICs?)
	0xfffa, // BRK
	0xfffc, // divide by zero
	0xfffe, // RESET
};

// M37710 internal peripherals

static const char *const m37710_rnames[128] =
{
	"",
	"",
	"Port P0 reg",
	"Port P1 reg",
	"Port P0 dir reg",
	"Port P1 dir reg",
	"Port P2 reg",
	"Port P3 reg",
	"Port P2 dir reg",
	"Port P3 dir reg",
	"Port P4 reg",
	"Port P5 reg",
	"Port P4 dir reg",
	"Port P5 dir reg",
	"Port P6 reg",
	"Port P7 reg",
	"Port P6 dir reg",  // 16 (0x10)
	"Port P7 dir reg",
	"Port P8 reg",
	"",
	"Port P8 dir reg",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"A/D control reg",
	"A/D sweep pin select",
	"A/D 0",        // 32 (0x20)
	"",
	"A/D 1",
	"",
	"A/D 2",
	"",
	"A/D 3",
	"",
	"A/D 4",
	"",
	"A/D 5",
	"",
	"A/D 6",
	"",
	"A/D 7",
	"",
	"UART0 transmit/recv mode",     // 48 (0x30)
	"UART0 baud rate",          // 0x31
	"UART0 transmit buf L",     // 0x32
	"UART0 transmit buf H",     // 0x33
	"UART0 transmit/recv ctrl 0",   // 0x34
	"UART0 transmit/recv ctrl 1",   // 0x35
	"UART0 recv buf L",     // 0x36
	"UART0 recv buf H",     // 0x37
	"UART1 transmit/recv mode", // 0x38
	"UART1 baud rate",
	"UART1 transmit buf L",
	"UART1 transmit buf H",
	"UART1 transmit/recv ctrl 0",
	"UART1 transmit/recv ctrl 1",
	"UART1 recv buf L",
	"UART1 recv buf H",
	"Count start",          // 0x40
	"",
	"One-shot start",
	"",
	"Up-down register",
	"",
	"Timer A0 L",       // 0x46
	"Timer A0 H",
	"Timer A1 L",
	"Timer A1 H",
	"Timer A2 L",
	"Timer A2 H",
	"Timer A3 L",
	"Timer A3 H",
	"Timer A4 L",
	"Timer A4 H",
	"Timer B0 L",
	"Timer B0 H",       // 0x50
	"Timer B1 L",
	"Timer B1 H",
	"Timer B2 L",
	"Timer B2 H",
	"Timer A0 mode",
	"Timer A1 mode",
	"Timer A2 mode",
	"Timer A3 mode",
	"Timer A4 mode",
	"Timer B0 mode",
	"Timer B1 mode",
	"Timer B2 mode",
	"Processor mode",
	"",
	"Watchdog reset",       // 0x60
	"Watchdog frequency",   // 0x61
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"",
	"A/D IRQ ctrl",
	"UART0 xmit IRQ ctrl",      // 0x70
	"UART0 recv IRQ ctrl",
	"UART1 xmit IRQ ctrl",
	"UART1 recv IRQ ctrl",
	"Timer A0 IRQ ctrl",        // 0x74
	"Timer A1 IRQ ctrl",        // 0x75
	"Timer A2 IRQ ctrl",        // 0x76
	"Timer A3 IRQ ctrl",
	"Timer A4 IRQ ctrl",        // 0x78
	"Timer B0 IRQ ctrl",
	"Timer B1 IRQ ctrl",
	"Timer B2 IRQ ctrl",
	"INT0 IRQ ctrl",
	"INT1 IRQ ctrl",
	"INT2 IRQ ctrl",
};

static const char *const m37710_tnames[8] =
{
	"A0", "A1", "A2", "A3", "A4", "B0", "B1", "B2"
};

#include "m37710cm.h"
#include "m37710il.h"
#define EXECUTION_MODE EXECUTION_MODE_M0X0
#include "m37710op.h"
#undef EXECUTION_MODE
#define EXECUTION_MODE EXECUTION_MODE_M0X1
#include "m37710op.h"
#undef EXECUTION_MODE
#define EXECUTION_MODE EXECUTION_MODE_M1X0
#include "m37710op.h"
#undef EXECUTION_MODE
#define EXECUTION_MODE EXECUTION_MODE_M1X1
#include "m37710op.h"


static void m37710_timer_cb(INT32 param)
{
	int which = param;
	int curirq = M37710_LINE_TIMERA0 - which;

	m377.timers[which] = m377.reload[which];

	m37710_set_irq_line(curirq, HOLD_LINE);
}

static void m37710_external_tick(int timer, int state)
{
	// we only care if the state is "on"
	if (!state)
	{
		return;
	}

	// check if enabled
	if (m377.m37710_regs[0x40] & (1<<timer))
	{
		if ((m377.m37710_regs[0x56+timer] & 0x3) == 1)
		{
			if (m377.m37710_regs[0x46+(timer*2)] == 0xff)
			{
				m377.m37710_regs[0x46+(timer*2)] = 0;
				m377.m37710_regs[0x46+(timer*2)+1]++;
			}
			else
			{
				m377.m37710_regs[0x46+(timer*2)]++;
			}
		}
		else
		{
			logerror("M37710: external tick for timer %d, not in event counter mode!\n", timer);
		}
	}
}

static void m37710_recalc_timer(int timer)
{
	int tval;
	INT32 time;
	static const int tscales[4] = { 2, 16, 64, 512 };

	// check if enabled
	if (m377.m37710_regs[0x40] & (1<<timer))
	{
		#if M37710_DEBUG
		logerror("Timer %d (%s) is enabled\n", timer, m37710_tnames[timer]);
		#endif

		// set the timer's value
		tval = m377.m37710_regs[0x46+(timer*2)] | (m377.m37710_regs[0x47+(timer*2)]<<8);

		// HACK: ignore if timer is 8MHz (MAME slows down to a crawl)
		if (tval == 0 && (m377.m37710_regs[0x56+timer]&0xc0) == 0) return;

		// check timer's mode
		// modes are slightly different between timer groups A and B
		if (timer < 5)
		{
			switch (m377.m37710_regs[0x56+timer] & 0x3)
			{
				case 0:         // timer mode
					//time = attotime::from_hz(unscaled_clock()) * tscales[m377.m37710_regs[0x56+timer]>>6];
					time = tscales[m377.m37710_regs[0x56+timer]>>6];
					time *= (tval + 1);

					#if M37710_DEBUG
					logerror("Timer %d in timer mode, %f Hz\n", timer, 1.0 / time.as_double());
					#endif

					//m377.timers[timer]->adjust(time, timer);
					m377.timers[timer] = time;
					m377.reload[timer] = time;
					break;

				case 1:         // event counter mode
					#if M37710_DEBUG
					logerror("Timer %d in event counter mode\n", timer);
					#endif
					break;

				case 2:     // one-shot pulse mode
					#if M37710_DEBUG
					logerror("Timer %d in one-shot mode\n", timer);
					#endif
					break;

				case 3:         // PWM mode
					#if M37710_DEBUG
					logerror("Timer %d in PWM mode\n", timer);
					#endif
					break;
			}
		}
		else
		{
			switch (m377.m37710_regs[0x56+timer] & 0x3)
			{
				case 0:         // timer mode
					time = tscales[m377.m37710_regs[0x56+timer]>>6];
					time *= (tval + 1);

					#if M37710_DEBUG
					logerror("Timer %d in timer mode, %f Hz\n", timer, 1.0 / time.as_double());
					#endif

					m377.timers[timer] = time;
					m377.reload[timer] = time;
					break;

				case 1:         // event counter mode
					#if M37710_DEBUG
					logerror("Timer %d in event counter mode\n", timer);
					#endif
					break;

				case 2:     // pulse period/pulse width measurement mode
					#if M37710_DEBUG
					logerror("Timer %d in pulse period/width measurement mode\n", timer);
					#endif
					break;

				case 3:
					#if M37710_DEBUG
					logerror("Timer %d in unknown mode!\n", timer);
					#endif
					break;
			}
		}
	}
}

static UINT8 m37710_internal_r(int offset)
{
	UINT8 d;

	#if M37710_DEBUG
	if (offset > 1)
	logerror("m37710_internal_r from %02x: %s (PC=%x)\n", (int)offset, m37710_rnames[(int)offset], REG_PB<<16 | REG_PC);
	#endif

	switch (offset)
	{
		// ports
		case 0x02: // p0
			d = m377.m37710_regs[0x04];
			if (d != 0xff)
				return (io_read_byte(M37710_PORT0)&~d) | (m377.m37710_regs[offset]&d);
			break;
		case 0x03: // p1
			d = m377.m37710_regs[0x05];
			if (d != 0xff)
				return (io_read_byte(M37710_PORT1)&~d) | (m377.m37710_regs[offset]&d);
			break;
		case 0x06: // p2
			d = m377.m37710_regs[0x08];
			if (d != 0xff)
				return (io_read_byte(M37710_PORT2)&~d) | (m377.m37710_regs[offset]&d);
			break;
		case 0x07: // p3
			d = m377.m37710_regs[0x09];
			if (d != 0xff)
				return (io_read_byte(M37710_PORT3)&~d) | (m377.m37710_regs[offset]&d);
			break;
		case 0x0a: // p4
			d = m377.m37710_regs[0x0c];
			if (d != 0xff)
				return (io_read_byte(M37710_PORT4)&~d) | (m377.m37710_regs[offset]&d);
			break;
		case 0x0b: // p5
			d = m377.m37710_regs[0x0d];
			if (d != 0xff)
				return (io_read_byte(M37710_PORT5)&~d) | (m377.m37710_regs[offset]&d);
			break;
		case 0x0e: // p6
			d = m377.m37710_regs[0x10];
			if (d != 0xff)
				return (io_read_byte(M37710_PORT6)&~d) | (m377.m37710_regs[offset]&d);
			break;
		case 0x0f: // p7
			d = m377.m37710_regs[0x11];
			if (d != 0xff)
				return (io_read_byte(M37710_PORT7)&~d) | (m377.m37710_regs[offset]&d);
			break;
		case 0x12: // p8
			d = m377.m37710_regs[0x14];
			if (d != 0xff)
				return (io_read_byte(M37710_PORT8)&~d) | (m377.m37710_regs[offset]&d);
			break;

		// A-D regs
		case 0x20:
			return io_read_byte(M37710_ADC0_L);
		case 0x21:
			return io_read_byte(M37710_ADC0_H);
		case 0x22:
			return io_read_byte(M37710_ADC1_L);
		case 0x23:
			return io_read_byte(M37710_ADC1_H);
		case 0x24:
			return io_read_byte(M37710_ADC2_L);
		case 0x25:
			return io_read_byte(M37710_ADC2_H);
		case 0x26:
			return io_read_byte(M37710_ADC3_L);
		case 0x27:
			return io_read_byte(M37710_ADC3_H);
		case 0x28:
			return io_read_byte(M37710_ADC4_L);
		case 0x29:
			return io_read_byte(M37710_ADC4_H);
		case 0x2a:
			return io_read_byte(M37710_ADC5_L);
		case 0x2b:
			return io_read_byte(M37710_ADC5_H);
		case 0x2c:
			return io_read_byte(M37710_ADC6_L);
		case 0x2d:
			return io_read_byte(M37710_ADC6_H);
		case 0x2e:
			return io_read_byte(M37710_ADC7_L);
		case 0x2f:
			return io_read_byte(M37710_ADC7_H);

		// UART control (not hooked up yet)
		case 0x34: case 0x3c:
			return 0x08;
		case 0x35: case 0x3d:
			return 0xff;

		// A-D IRQ control (also not properly hooked up yet)
		case 0x70:
			return m377.m37710_regs[offset] | 8;

		default:
			return m377.m37710_regs[offset];
	}

	return m377.m37710_regs[offset];
}

static void m37710_internal_w(int offset, UINT8 data)
{
	int i;
	UINT8 prevdata;
	UINT8 d;

	#if M37710_DEBUG
	if (offset != 0x60) // filter out watchdog
	logerror("m37710_internal_w %x to %02x: %s = %x\n", data, (int)offset, m37710_rnames[(int)offset], m377.m37710_regs[offset]);
	#endif

	prevdata = m377.m37710_regs[offset];
	m377.m37710_regs[offset] = data;

	switch(offset)
	{
		// ports
		case 0x02: // p0
			d = m377.m37710_regs[0x04];
			if (d != 0)
				io_write_byte(M37710_PORT0, data&d);
			break;
		case 0x03: // p1
			d = m377.m37710_regs[0x05];
			if (d != 0)
				io_write_byte(M37710_PORT1, data&d);
			break;
		case 0x06: // p2
			d = m377.m37710_regs[0x08];
			if (d != 0)
				io_write_byte(M37710_PORT2, data&d);
			break;
		case 0x07: // p3
			d = m377.m37710_regs[0x09];
			if (d != 0)
				io_write_byte(M37710_PORT3, data&d);
			break;
		case 0x0a: // p4
			d = m377.m37710_regs[0x0c];
			if (d != 0)
				io_write_byte(M37710_PORT4, data&d);
			break;
		case 0x0b: // p5
			d = m377.m37710_regs[0x0d];
			if (d != 0)
				io_write_byte(M37710_PORT5, data&d);
			break;
		case 0x0e: // p6
			d = m377.m37710_regs[0x10];
			if (d != 0)
				io_write_byte(M37710_PORT6, data&d);
			break;
		case 0x0f: // p7
			d = m377.m37710_regs[0x11];
			if (d != 0)
				io_write_byte(M37710_PORT7, data&d);
			break;
		case 0x12: // p8
			d = m377.m37710_regs[0x14];
			if (d != 0)
				io_write_byte(M37710_PORT8, data&d);
			break;

		case 0x40:  // count start
			for (i = 0; i < 8; i++)
			{
				if ((data & (1<<i)) && !(prevdata & (1<<i)))
					m37710_recalc_timer(i);
			}
			break;

		// internal interrupt control
		case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75:
		case 0x76: case 0x77: case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c:
			m37710_set_irq_line(offset, (data & 8) ? HOLD_LINE : CLEAR_LINE);
			m37710i_update_irqs();
			break;

		// external interrupt control
		case 0x7d: case 0x7e: case 0x7f:
			m37710_set_irq_line(offset, (data & 8) ? HOLD_LINE : CLEAR_LINE);
			m37710i_update_irqs();

			// level-sense interrupts are not implemented yet
			if (data & 0x20) logerror("error M37710: INT%d level-sense\n",offset-0x7d);
			break;

		default:
			break;
	}
}

const opcode_func *m37710i_opcodes[4] =
{
	m37710i_opcodes_M0X0,
	m37710i_opcodes_M0X1,
	m37710i_opcodes_M1X0,
	m37710i_opcodes_M1X1,
};

const opcode_func *m37710i_opcodes2[4] =
{
	m37710i_opcodes42_M0X0,
	m37710i_opcodes42_M0X1,
	m37710i_opcodes42_M1X0,
	m37710i_opcodes42_M1X1,
};

const opcode_func *m37710i_opcodes3[4] =
{
	m37710i_opcodes89_M0X0,
	m37710i_opcodes89_M0X1,
	m37710i_opcodes89_M1X0,
	m37710i_opcodes89_M1X1,
};

const get_reg_func m37710i_get_reg[4] =
{
	&m37710i_get_reg_M0X0,
	&m37710i_get_reg_M0X1,
	&m37710i_get_reg_M1X0,
	&m37710i_get_reg_M1X1,
};

const set_reg_func m37710i_set_reg[4] =
{
	&m37710i_set_reg_M0X0,
	&m37710i_set_reg_M0X1,
	&m37710i_set_reg_M1X0,
	&m37710i_set_reg_M1X1,
};

const set_line_func m37710i_set_line[4] =
{
	&m37710i_set_line_M0X0,
	&m37710i_set_line_M0X1,
	&m37710i_set_line_M1X0,
	&m37710i_set_line_M1X1,
};

const execute_func m37710i_execute[4] =
{
	&m37710i_execute_M0X0,
	&m37710i_execute_M0X1,
	&m37710i_execute_M1X0,
	&m37710i_execute_M1X1,
};

/* internal functions */

static void m37710i_update_irqs()
{
	int curirq, pending = LINE_IRQ;
	int wantedIRQ = -1;
	int curpri = 0;

	for (curirq = M37710_LINE_MAX - 1; curirq >= 0; curirq--)
	{
		if ((pending & (1 << curirq)))
		{
			// this IRQ is set
			if (m37710_irq_levels[curirq])
			{
				int control = m377.m37710_regs[m37710_irq_levels[curirq]];
				int thispri = control & 7;
				// logerror("line %d set, level %x curpri %x IPL %x\n", curirq, thispri, curpri, m377.ipl);
				// it's maskable, check if the level works, also make sure it's acceptable for the current CPU level
				if (!FLAG_I && thispri > curpri && thispri > m377.ipl)
				{
					// mark us as the best candidate
					wantedIRQ = curirq;
					curpri = thispri;
				}
			}
			else
			{
				// non-maskable
				wantedIRQ = curirq;
				curpri = 7;
				break;  // no more processing, NMIs always win
			}
		}
	}

	if (wantedIRQ != -1)
	{
		//standard_irq_callback(wantedIRQ);

		// make sure we're running to service the interrupt
		CPU_STOPPED &= ~STOP_LEVEL_WAI;

		// auto-clear line
		m37710_set_irq_line(wantedIRQ, CLEAR_LINE);

		// let's do it...
		// push PB, then PC, then status
		CLK(13);
//      osd_printf_debug("taking IRQ %d: PC = %06x, SP = %04x, IPL %d\n", wantedIRQ, REG_PB | REG_PC, REG_S, m377.ipl);
		m37710i_push_8(REG_PB>>16);
		m37710i_push_16(REG_PC);
		m37710i_push_8(m377.ipl);
		m37710i_push_8(m37710i_get_reg_p());

		// set I to 1, set IPL to the interrupt we're taking
		FLAG_I = IFLAG_SET;
		m377.ipl = curpri;
		// then PB=0, PC=(vector)
		REG_PB = 0;
		REG_PC = m37710_read_16(m37710_irq_vectors[wantedIRQ]);
//      logerror("IRQ @ %06x\n", REG_PB | REG_PC);
	}
}

/* external functions */

void M377Reset()
{
	int i;

	/* Reset DINK timers */
	for (i = 0; i < 8; i++)
	{
		m377.timers[i] = -1;
		m377.reload[i] = -1;
	}

	/* Start the CPU */
	CPU_STOPPED = 0;

	/* Reset internal registers */
	// port direction
	m377.m37710_regs[0x04] = 0;
	m377.m37710_regs[0x05] = 0;
	m377.m37710_regs[0x08] = 0;
	m377.m37710_regs[0x09] = 0;
	m377.m37710_regs[0x0c] = 0;
	m377.m37710_regs[0x0d] = 0;
	m377.m37710_regs[0x10] = 0;
	m377.m37710_regs[0x11] = 0;
	m377.m37710_regs[0x14] = 0;

	m377.m37710_regs[0x1e] &= 7; // A-D control
	m377.m37710_regs[0x1f] |= 3; // A-D sweep

	// UART
	m377.m37710_regs[0x30] = 0;
	m377.m37710_regs[0x38] = 0;
	m377.m37710_regs[0x34] = (m377.m37710_regs[0x34] & 0xf0) | 8;
	m377.m37710_regs[0x3c] = (m377.m37710_regs[0x3c] & 0xf0) | 8;
	m377.m37710_regs[0x35] = 2;
	m377.m37710_regs[0x3d] = 2;
	m377.m37710_regs[0x37]&= 1;
	m377.m37710_regs[0x3f]&= 1;

	// timer
	m377.m37710_regs[0x40] = 0;
	m377.m37710_regs[0x42]&= 0x1f;
	m377.m37710_regs[0x44] = 0;
	for (i = 0x56; i < 0x5e; i++)
		m377.m37710_regs[i] = 0;

	m377.m37710_regs[0x5e] = 0; // processor mode
	m377.m37710_regs[0x61]&= 1; // watchdog frequency

	// interrupt control
	m377.m37710_regs[0x7d] &= 0x3f;
	m377.m37710_regs[0x7e] &= 0x3f;
	m377.m37710_regs[0x7f] &= 0x3f;
	for (i = 0x70; i < 0x7d; i++)
		m377.m37710_regs[i] &= 0xf;

	/* Clear IPL, m, x, D and set I */
	m377.ipl = 0;
	FLAG_M = MFLAG_CLEAR;
	FLAG_X = XFLAG_CLEAR;
	FLAG_D = DFLAG_CLEAR;
	FLAG_I = IFLAG_SET;

	/* Clear all pending interrupts (should we really do this?) */
	LINE_IRQ = 0;
	IRQ_DELAY = 0;

	/* 37710 boots in full native mode */
	REG_D = 0;
	REG_PB = 0;
	REG_DB = 0;
	REG_S = (REG_S & 0xff) | 0x100;
	REG_XH = REG_X & 0xff00; REG_X &= 0xff;
	REG_YH = REG_Y & 0xff00; REG_Y &= 0xff;
	REG_B = REG_A & 0xff00; REG_A &= 0xff;
	REG_BB = REG_BA & 0xff00; REG_BA &= 0xff;

	/* Set the function tables to emulation mode */
	m37710i_set_execution_mode(EXECUTION_MODE_M0X0);

	/* Fetch the reset vector */
	REG_PC = m37710_read_16(0xfffe);
}

static void m37710_clock_timers(INT32 clkcnt)
{
	for (INT32 c = 0; c < clkcnt; c++) {
		for (INT32 i = 0; i < 8; i++) {
			if (m377.timers[i] > 0) { // active
				m377.timers[i]--;
				if (m377.timers[i] <= 0) { // timer hits!
					m377.timers[i] = -1; // disable this timer

					m37710_timer_cb(i);
				}
			}
		}
	}
}

static INT32 current_cpu = -1;

INT32 M377GetActive()
{
	return current_cpu;
}

void M377Open(INT32 cpunum)
{
	current_cpu = cpunum; // dummy, currently single-cpu core
}

void M377Close()
{
	current_cpu = -1; // dummy, currently single-cpu core
}

void M377RunEnd()
{
	m377.end_run = 1;
}

INT32 M377Idle(INT32 cycles)
{
	m377.total_cycles += cycles;

	return cycles;
}

/* Execute some instructions */
INT32 M377Run(INT32 cycles)
{
	m377.end_run = 0;

	m37710i_update_irqs();

	(*m_execute)(m377.ICount);

	cycles = cycles - m377.ICount;

	m377.segment_cycles = m377.ICount = 0;

	m377.total_cycles += cycles;

	return cycles;
}

void M377NewFrame()
{
	m377.total_cycles = 0;
}

INT32 M377TotalCycles()
{
	return m377.total_cycles + (m377.segment_cycles - m377.ICount);
}


/* Set the Program Counter */
void m37710_set_pc(unsigned val)
{
	REG_PC = MAKE_UINT_16(val);
}

/* Get the current Stack Pointer */
unsigned m37710_get_sp()
{
	return REG_S;
}

/* Set the Stack Pointer */
void m37710_set_sp(unsigned val)
{
	REG_S = MAKE_UINT_16(val);
}

/* Get a register */
unsigned m37710_get_reg(int regnum)
{
	return (*m_get_reg)(regnum);
}

/* Set a register */
void m37710_set_reg(int regnum, unsigned value)
{
	(*m_set_reg)(regnum, value);
}

/* Set an interrupt line */
static void m37710_set_irq_line(int line, int state)
{
	(*m_set_line)(line, state);
}


void m37710_restore_state()
{
	// restore proper function pointers
	m37710i_set_execution_mode((FLAG_M>>4) | (FLAG_X>>4));
}

INT32 M377Scan(INT32 nAction)
{
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return 1;
	}

	ScanVar(internal_ram, 0x800, "M377xx Int.RAM");

	SCAN_VAR(m377);

	if (nAction & ACB_WRITE) {
		m37710_restore_state();
	}

	return 0;
}

void M377Init(INT32 cpunum)
{
	cpunum = cpunum; // dummy

	for (INT32 i = 0; i < 3; i++) {
		mem[i] = (UINT8**)BurnMalloc(((address_mask / page_size) + 1) * sizeof(UINT8**));
		memset (mem[i], 0, ((address_mask / page_size) + 1) * sizeof(UINT8**));
	}

	internal_ram = (UINT8*)BurnMalloc(0x800);

	M377MapMemory(internal_ram, 0x80, 0x87f, MAP_RAM);

	memset(&m377, 0, sizeof(m377));
	memset(internal_ram, 0, 0x800);

	for (int i = 0; i < 8; i++)
	{
		m377.timers[i] = -1;
		m377.reload[i] = -1;
	}

	CpuCheatRegister(0, &M377Config);
}

void M377Exit()
{
	for (INT32 i = 0; i < 3; i++) {
		BurnFree(mem[i]);
	}

	BurnFree(internal_ram);
}

void M377SetIRQLine(INT32 inputnum, INT32 state)
{
	switch( inputnum )
	{
		case M37710_LINE_ADC:
		case M37710_LINE_IRQ0:
		case M37710_LINE_IRQ1:
		case M37710_LINE_IRQ2:
			m37710_set_irq_line(inputnum, state);
			break;

		case M37710_LINE_TIMERA0TICK:
		case M37710_LINE_TIMERA1TICK:
		case M37710_LINE_TIMERA2TICK:
		case M37710_LINE_TIMERA3TICK:
		case M37710_LINE_TIMERA4TICK:
		case M37710_LINE_TIMERB0TICK:
		case M37710_LINE_TIMERB1TICK:
		case M37710_LINE_TIMERB2TICK:
			m37710_external_tick(inputnum - M37710_LINE_TIMERA0TICK, state);
			break;
	}
}


static void m37710i_set_execution_mode(UINT32 mode)
{
	m_opcodes = m37710i_opcodes[mode];
	m_opcodes42 = m37710i_opcodes2[mode];
	m_opcodes89 = m37710i_opcodes3[mode];
	m_get_reg = m37710i_get_reg[mode];
	m_set_reg = m37710i_set_reg[mode];
	m_set_line = m37710i_set_line[mode];
	m_execute = m37710i_execute[mode];
}


/* ======================================================================== */
/* =============================== INTERRUPTS ============================= */
/* ======================================================================== */

static void m37710i_interrupt_software(UINT32 vector)
{
	CLK(13);
	m37710i_push_8(REG_PB>>16);
	m37710i_push_16(REG_PC);
	m37710i_push_8(m377.ipl);
	m37710i_push_8(m37710i_get_reg_p());
	FLAG_I = IFLAG_SET;
	REG_PB = 0;
	REG_PC = m37710_read_16(vector);
}



/* ======================================================================== */
/* ============================== END OF FILE ============================= */
/* ======================================================================== */
