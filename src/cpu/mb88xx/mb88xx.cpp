// license:BSD-3-Clause
// copyright-holders:Ernesto Corvi
/***************************************************************************

    mb88xx.cpp
    Core implementation for the portable Fujitsu MB88xx series MCU emulator.

    Written by Ernesto Corvi


    TODO:
    - Add support for the timer
    - Add support for the serial interface
    - Split the core to support multiple CPU types?

***************************************************************************/

// FBNeo port note:
//  serial timer isn't hooked up (not needed for Arabian)

#include "burnint.h"
#include "driver.h"

enum
{
	MB88_PC=1,
	MB88_PA,
	MB88_FLAGS,
	MB88_SI,
	MB88_A,
	MB88_X,
	MB88_Y,
	MB88_PIO,
	MB88_TH,
	MB88_TL,
	MB88_SB
};

#define MB88_IRQ_LINE       0

static UINT8 m_PC;     /* Program Counter: 6 bits */
static UINT8 m_PA;     /* Page Address: 4 bits */
static UINT16 m_SP[4];  /* Stack is 4*10 bit addresses deep, but we also use 3 top bits per address to store flags during irq */
static UINT8 m_SI;     /* Stack index: 2 bits */
static UINT8 m_A;      /* Accumulator: 4 bits */
static UINT8 m_X;      /* Index X: 4 bits */
static UINT8 m_Y;      /* Index Y: 4 bits */
static UINT8 m_st;     /* State flag: 1 bit */
static UINT8 m_zf;     /* Zero flag: 1 bit */
static UINT8 m_cf;     /* Carry flag: 1 bit */
static UINT8 m_vf;     /* Timer overflow flag: 1 bit */
static UINT8 m_sf;     /* Serial Full/Empty flag: 1 bit */
static UINT8 m_nf;     /* Interrupt flag: 1 bit */

	/* Peripheral Control */
static UINT8 m_pio; /* Peripheral enable bits: 8 bits */

	/* Timer registers */
static UINT8 m_TH; /* Timer High: 4 bits */
static UINT8 m_TL; /* Timer Low: 4 bits */
static UINT8 m_TP; /* Timer Prescale: 6 bits? */
static UINT8 m_ctr; /* current external counter value */

	/* Serial registers */
static UINT8 m_SB; /* Serial buffer: 4 bits */
static UINT16 m_SBcount;    /* number of bits received */

static UINT8 m_pending_interrupt;
static UINT8 *m_PLA = NULL;

static int m_icount;
static int total_cycles;
static int segment_cycles;
static int end_run;
static int m_in_irq;
static int in_reset;

static INT32 program_rom_mask = 0;
static INT32 program_ram_mask = 0;	// ram region!
static UINT8 *program_ram;
static UINT8 *program_rom; // largest is 1 << 11

typedef UINT8 (*read_function)();
typedef void  (*write_function)(UINT8 data);
typedef INT32 (*read_line_function)();
typedef void  (*write_line_function)(INT32 line);

static read_function m_read_k = NULL;
static write_function m_write_o = NULL;
static write_function m_write_p = NULL;
static read_function m_read_r[4] = { NULL, NULL, NULL, NULL };
static write_function m_write_r[4] = { NULL, NULL, NULL, NULL };
static write_line_function m_write_so = NULL;
static read_line_function m_read_si = NULL;

void mb88xxSetSerialReadWriteFunction(void (*write)(INT32 line), INT32 (*read)())
{
	if (write) m_write_so = write;
	if (read) m_read_si = read;
}

void mb88xxSetReadWriteFunction(void (*write)(UINT8), UINT8 (*read)(), char select, INT32 which)
{
	switch (select)
	{
		case 'k':
		case 'K':
			if (read) m_read_k = read;
		break;

		case 'o':
		case 'O':
			if (write) m_write_o = write;
		break;

		case 'p':
		case 'P':
			if (write) m_write_p = write;
		break;

		case 'r':
		case 'R':
			if (write) m_write_r[which & 3] = write;
			if (read) m_read_r[which & 3] = read;
		break;
	}
}


/***************************************************************************
    CONSTANTS
***************************************************************************/

#define SERIAL_PRESCALE         6       /* guess */
#define TIMER_PRESCALE          32      /* guess */

#define SERIAL_DISABLE_THRESH   1000    /* at this value, we give up driving the serial port */

#define INT_CAUSE_SERIAL        0x01
#define INT_CAUSE_TIMER         0x02
#define INT_CAUSE_EXTERNAL      0x04


/***************************************************************************
    MACROS
***************************************************************************/

#define READOP(a)           (program_rom[(a) & program_rom_mask])

#define RDMEM(a)            (program_ram[(a) & program_ram_mask] & 0x0f)
#define WRMEM(a,v)          (program_ram[(a) & program_ram_mask] = ((v) & 0x0f))

#define TEST_ST()           (m_st & 1)
#define TEST_ZF()           (m_zf & 1)
#define TEST_CF()           (m_cf & 1)
#define TEST_VF()           (m_vf & 1)
#define TEST_SF()           (m_sf & 1)
#define TEST_NF()           (m_nf & 1)

#define UPDATE_ST_C(v)      m_st=(v&0x10) ? 0 : 1
#define UPDATE_ST_Z(v)      m_st=(v==0) ? 0 : 1

#define UPDATE_CF(v)        m_cf=((v&0x10)==0) ? 0 : 1
#define UPDATE_ZF(v)        m_zf=(v!=0) ? 0 : 1

#define CYCLES(x)           do { m_icount -= (x); } while (0)

#define GETPC()             (((int)m_PA << 6)+m_PC)
#define GETEA()             ((m_X << 4)+m_Y)

#define INCPC()             do { m_PC++; if ( m_PC >= 0x40 ) { m_PC = 0; m_PA++; } } while (0)


static void dummy_write(UINT8) {}
static UINT8 dummy_read() { return 0; }
static void dummy_line_write(INT32) {}
static INT32 dummy_line_read() { return 0; }

void mb88xxInit(INT32 nCPU, INT32 nType, UINT8 *rom)
{
	if (nCPU != 0) {
		bprintf (0, _T("Only one instance of mb88xx implemented!\n"));
	}

	INT32 program_width = 11;
	INT32 data_width = 7;

	switch (nType)
	{
		case 88201:
			program_width = 9;
			data_width = 4;
		break;

		case 88202:
			program_width = 10;
			data_width = 5;
		break;

		case 8841:
		case 8842:
			program_width = 11;
			data_width = 7;
		break;

		case 8843:
		case 8844:
			program_width = 10;
			data_width = 6;
		break;
	}

	// mb88201 - program_width ==  9, data_width == 4
	// mb88202 - program_width == 10, data_width == 5
	// mb8841  - program_width == 11, data_width == 7
	// mb8842  - program_width == 11, data_width == 7
	// mb8843  - program_width == 10, data_width == 6
	// mb8844  - program_width == 10, data_width == 6

	program_rom_mask = (1 << program_width) - 1;
	program_ram_mask = (1 << data_width) - 1;	// ram region!!

	program_rom = (UINT8*)BurnMalloc(1 << program_width);
	program_ram = (UINT8*)BurnMalloc(1 << data_width);

	memcpy (program_rom, rom, 1 << program_width);

	m_ctr = 0;
//	m_serial = timer_alloc(FUNC(mb88_cpu_device::serial_timer), this);

	m_read_k = dummy_read;

	m_write_o = dummy_write;
	m_write_p = dummy_write;
	for (INT32 i = 0; i < 4; i++) {
		m_read_r[i] = dummy_read;
		m_write_r[i] = dummy_write;
	}
	m_read_si = dummy_line_read;
	m_write_so = dummy_line_write;

	// leave out of reset: -dink
	end_run = 0;
	total_cycles = 0;
	segment_cycles = 0;
	m_icount = 0;

	in_reset = 0;
}

void mb88xxExit()
{
	if (program_rom) {
		BurnFree(program_rom);
	}
	if (program_ram) {
		BurnFree(program_ram);
	}
}

/***************************************************************************
    INITIALIZATION AND SHUTDOWN
***************************************************************************/

void mb88xxReset()
{
	/* zero registers and flags */
	m_PC = 0;
	m_PA = 0;
	m_SP[0] = m_SP[1] = m_SP[2] = m_SP[3] = 0;
	m_SI = 0;
	m_A = 0;
	m_X = 0;
	m_Y = 0;
	m_st = 1;   /* start off with st=1 */
	m_zf = 0;
	m_cf = 0;
	m_vf = 0;
	m_sf = 0;
	m_nf = 0;
	m_pio = 0;
	m_TH = 0;
	m_TL = 0;
	m_TP = 0;
	m_SB = 0;
	m_SBcount = 0;
	m_pending_interrupt = 0;
	m_in_irq = 0;

	end_run = 0;
}

void mb88xxSetRESETLine(INT32 status)
{
	if (in_reset && status == 0) {
		mb88xxReset();
	}

	in_reset = status;
}

void mb88xxScan(INT32 nAction)
{
	ScanVar(program_ram, program_ram_mask + 1, "mb88xx ram");

	SCAN_VAR(m_PC);
	SCAN_VAR(m_PA);
	SCAN_VAR(m_SP);
	SCAN_VAR(m_SI);
	SCAN_VAR(m_A);
	SCAN_VAR(m_X);
	SCAN_VAR(m_Y);
	SCAN_VAR(m_st);
	SCAN_VAR(m_zf);
	SCAN_VAR(m_cf);
	SCAN_VAR(m_vf);
	SCAN_VAR(m_sf);
	SCAN_VAR(m_nf);
	SCAN_VAR(m_pio);
	SCAN_VAR(m_TH);
	SCAN_VAR(m_TL);
	SCAN_VAR(m_TP);
	SCAN_VAR(m_ctr);
	SCAN_VAR(m_SB);
	SCAN_VAR(m_SBcount);
	SCAN_VAR(m_pending_interrupt);
	SCAN_VAR(m_in_irq);
	SCAN_VAR(total_cycles);
	SCAN_VAR(in_reset);
}

/***************************************************************************
    CORE EXECUTION LOOP
***************************************************************************/

#if 0
static void mb88_serial_timer()
{
	m_SBcount++;

	/* if we get too many interrupts with no servicing, disable the timer
	   until somebody does something */
//	if (m_SBcount >= SERIAL_DISABLE_THRESH)
//		m_serial->adjust(attotime::never);
// iq_132

	/* only read if not full; this is needed by the Namco 52xx to ensure that
	   the program can write to S and recover the value even if serial is enabled */
	if (!m_sf)
	{
		m_SB = (m_SB >> 1) | (m_read_si() ? 8 : 0);

		if (m_SBcount >= 4)
		{
			m_sf = 1;
			m_pending_interrupt |= INT_CAUSE_SERIAL;
		}
	}
}
#endif

static int pla( int inA, int inB )
{
	int index = ((inB&1) << 4) | (inA&0x0f);

	if ( m_PLA )
		return m_PLA[index];

	return index;
}

void mb88xxSetIRQLine(int /*inputnum*/, int state)
{
	/* On rising edge trigger interrupt.
	 * Note this is a logical level, the actual pin is high-to-low voltage
	 * triggered. */
	if ( (m_pio & INT_CAUSE_EXTERNAL) && !m_nf && state != CLEAR_LINE )
	{
		m_pending_interrupt |= INT_CAUSE_EXTERNAL;
	}

	m_nf = state != CLEAR_LINE;
}

static void update_pio_enable( UINT8 newpio )
{
	/* if the serial state has changed, configure the timer */
	if ((m_pio ^ newpio) & 0x30)
	{
	// iq_132
		if ((newpio & 0x30) == 0)
			bprintf(0, _T("mb88xx: serial off (serial is disabled in this cpu core)\n"));//m_serial->adjust(attotime::never);
		else if ((newpio & 0x30) == 0x20)
			bprintf(0, _T("mb88xx: serial on (serial is disabled in this cpu core)\n"));//m_serial->adjust(attotime::from_hz(clock() / SERIAL_PRESCALE), 0, attotime::from_hz(clock() / SERIAL_PRESCALE));
	//	else
	//		fatalerror("mb88xx: update_pio_enable set serial enable to unsupported value %02X\n", newpio & 0x30);
	}

	m_pio = newpio;
}

static void increment_timer()
{
	m_TL = (m_TL + 1) & 0x0f;
	if (m_TL == 0)
	{
		m_TH = (m_TH + 1) & 0x0f;
		if (m_TH == 0)
		{
			m_vf = 1;
			m_pending_interrupt |= INT_CAUSE_TIMER;
		}
	}
}

static void update_pio( int cycles )
{
	/* TODO: improve/validate serial and timer support */

	/* internal clock enable */
	if ( m_pio & 0x80 )
	{
		m_TP += cycles;
		while (m_TP >= TIMER_PRESCALE)
		{
			m_TP -= TIMER_PRESCALE;
			increment_timer();
		}
	}

	/* process pending interrupts */
	if (!m_in_irq && m_pending_interrupt & m_pio)
	{
		m_in_irq = 1;
		UINT16 intpc = GETPC();

		m_SP[m_SI] = intpc;
		m_SP[m_SI] |= TEST_CF() << 15;
		m_SP[m_SI] |= TEST_ZF() << 14;
		m_SP[m_SI] |= TEST_ST() << 13;
		m_SI = ( m_SI + 1 ) & 3;

		/* the datasheet doesn't mention interrupt vectors but
		the Arabian MCU program expects the following */
		if (m_pending_interrupt & m_pio & INT_CAUSE_EXTERNAL)
		{
			/* if we have a live external source, call the irqcallback */
			//standard_irq_callback( 0, intpc );
			m_PC = 0x02;
		}
		else if (m_pending_interrupt & m_pio & INT_CAUSE_TIMER)
		{
			//standard_irq_callback( 1, intpc );
			m_PC = 0x04;
		}
		else if (m_pending_interrupt & m_pio & INT_CAUSE_SERIAL)
		{
			//standard_irq_callback( 2, intpc );
			m_PC = 0x06;
		}

		m_PA = 0x00;
		m_st = 1;
		m_pending_interrupt = 0;

		CYCLES(3); /* ? */
	}
}

void mb88xx_clock_write(INT32 state)
{
	if (state != m_ctr)
	{
		m_ctr = state;

		/* on a falling clock, increment the timer, but only if enabled */
		if (m_ctr == 0 && (m_pio & 0x40))
			increment_timer();
	}
}

void mb88xx_set_pla(UINT8 *pla)
{
	m_PLA = pla;
}

void mb88xxEndRun()
{
	end_run = 1;
}

INT32 mb88xxRun(INT32 nCycles)
{
	end_run = 0;
	segment_cycles = nCycles;

	m_icount = nCycles;

	if (in_reset) {
		m_icount = 0;
	}

	while (m_icount > 0 && !end_run)
	{
		UINT8 opcode, arg, oc;

		/* fetch the opcode */
		opcode = READOP(GETPC());

		/* increment the PC */
		INCPC();

		/* start with instruction doing 1 cycle */
		oc = 1;

		switch (opcode)
		{
			case 0x00: /* nop ZCS:...*/
				m_st = 1;
				break;

			case 0x01: /* outO ZCS:...*/
				m_write_o(pla(m_A, TEST_CF()));
				m_st = 1;
				break;

			case 0x02: /* outP ZCS:... */
				m_write_p(m_A);
				m_st = 1;
				break;

			case 0x03: /* outR ZCS:... */
				arg = m_Y;
				m_write_r[arg & 3](m_A);
				m_st = 1;
				break;

			case 0x04: /* tay ZCS:... */
				m_Y = m_A;
				m_st = 1;
				break;

			case 0x05: /* tath ZCS:... */
				m_TH = m_A;
				m_st = 1;
				break;

			case 0x06: /* tatl ZCS:... */
				m_TL = m_A;
				m_st = 1;
				break;

			case 0x07: /* tas ZCS:... */
				m_SB = m_A;
				m_st = 1;
				break;

			case 0x08: /* icy ZCS:x.x */
				m_Y++;
				UPDATE_ST_C(m_Y);
				m_Y &= 0x0f;
				UPDATE_ZF(m_Y);
				break;

			case 0x09: /* icm ZCS:x.x */
				arg=RDMEM(GETEA());
				arg++;
				UPDATE_ST_C(arg);
				arg &= 0x0f;
				UPDATE_ZF(arg);
				WRMEM(GETEA(),arg);
				break;

			case 0x0a: /* stic ZCS:x.x */
				WRMEM(GETEA(),m_A);
				m_Y++;
				UPDATE_ST_C(m_Y);
				m_Y &= 0x0f;
				UPDATE_ZF(m_Y);
				break;

			case 0x0b: /* x ZCS:x.. */
				arg = RDMEM(GETEA());
				WRMEM(GETEA(),m_A);
				m_A = arg;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x0c: /* rol ZCS:xxx */
				m_A <<= 1;
				m_A |= TEST_CF();
				UPDATE_ST_C(m_A);
				m_cf = m_st ^ 1;
				m_A &= 0x0f;
				UPDATE_ZF(m_A);
				break;

			case 0x0d: /* l ZCS:x.. */
				m_A = RDMEM(GETEA());
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x0e: /* adc ZCS:xxx */
				arg = RDMEM(GETEA());
				arg += m_A;
				arg += TEST_CF();
				UPDATE_ST_C(arg);
				m_cf = m_st ^ 1;
				m_A = arg & 0x0f;
				UPDATE_ZF(m_A);
				break;

			case 0x0f: /* and ZCS:x.x */
				m_A &= RDMEM(GETEA());
				UPDATE_ZF(m_A);
				m_st = m_zf ^ 1;
				break;

			case 0x10: /* daa ZCS:.xx */
				if ( TEST_CF() || m_A > 9 ) m_A += 6;
				UPDATE_ST_C(m_A);
				m_cf = m_st ^ 1;
				m_A &= 0x0f;
				break;

			case 0x11: /* das ZCS:.xx */
				if ( TEST_CF() || m_A > 9 ) m_A += 10;
				UPDATE_ST_C(m_A);
				m_cf = m_st ^ 1;
				m_A &= 0x0f;
				break;

			case 0x12: /* inK ZCS:x.. */
				m_A = m_read_k() & 0x0f;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x13: /* inR ZCS:x.. */
				arg = m_Y;
				m_A = m_read_r[arg & 3]() & 0x0f;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x14: /* tya ZCS:x.. */
				m_A = m_Y;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x15: /* ttha ZCS:x.. */
				m_A = m_TH;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x16: /* ttla ZCS:x.. */
				m_A = m_TL;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x17: /* tsa ZCS:x.. */
				m_A = m_SB;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x18: /* dcy ZCS:..x */
				m_Y--;
				UPDATE_ST_C(m_Y);
				m_Y &= 0x0f;
				break;

			case 0x19: /* dcm ZCS:x.x */
				arg=RDMEM(GETEA());
				arg--;
				UPDATE_ST_C(arg);
				arg &= 0x0f;
				UPDATE_ZF(arg);
				WRMEM(GETEA(),arg);
				break;

			case 0x1a: /* stdc ZCS:x.x */
				WRMEM(GETEA(),m_A);
				m_Y--;
				UPDATE_ST_C(m_Y);
				m_Y &= 0x0f;
				UPDATE_ZF(m_Y);
				break;

			case 0x1b: /* xx ZCS:x.. */
				arg = m_X;
				m_X = m_A;
				m_A = arg;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x1c: /* ror ZCS:xxx */
				m_A |= TEST_CF() << 4;
				UPDATE_ST_C(m_A << 4);
				m_cf = m_st ^ 1;
				m_A >>= 1;
				m_A &= 0x0f;
				UPDATE_ZF(m_A);
				break;

			case 0x1d: /* st ZCS:x.. */
				WRMEM(GETEA(),m_A);
				m_st = 1;
				break;

			case 0x1e: /* sbc ZCS:xxx */
				arg = RDMEM(GETEA());
				arg -= m_A;
				arg -= TEST_CF();
				UPDATE_ST_C(arg);
				m_cf = m_st ^ 1;
				m_A = arg & 0x0f;
				UPDATE_ZF(m_A);
				break;

			case 0x1f: /* or ZCS:x.x */
				m_A |= RDMEM(GETEA());
				UPDATE_ZF(m_A);
				m_st = m_zf ^ 1;
				break;

			case 0x20: /* setR ZCS:... */
				arg = m_read_r[m_Y/4]() & 0x0f;
				m_write_r[m_Y/4](arg | (1 << (m_Y%4)));
				m_st = 1;
				break;

			case 0x21: /* setc ZCS:.xx */
				m_cf = 1;
				m_st = 1;
				break;

			case 0x22: /* rstR ZCS:... */
				arg = m_read_r[m_Y/4]() & 0x0f;
				m_write_r[m_Y/4](arg & ~(1 << (m_Y%4)));
				m_st = 1;
				break;

			case 0x23: /* rstc ZCS:.xx */
				m_cf = 0;
				m_st = 1;
				break;

			case 0x24: /* tstr ZCS:..x */
				arg = m_read_r[m_Y/4]() & 0x0f;
				m_st = ( arg & ( 1 << (m_Y%4) ) ) ? 0 : 1;
				break;

			case 0x25: /* tsti ZCS:..x */
				m_st = m_nf ^ 1;
				break;

			case 0x26: /* tstv ZCS:..x */
				m_st = m_vf ^ 1;
				m_vf = 0;
				break;

			case 0x27: /* tsts ZCS:..x */
				m_st = m_sf ^ 1;
				if (m_sf)
				{
				// iq_132
					/* re-enable the timer if we disabled it previously */
				//	if (m_SBcount >= SERIAL_DISABLE_THRESH)
				//		m_serial->adjust(attotime::from_hz(clock() / SERIAL_PRESCALE), 0, attotime::from_hz(clock() / SERIAL_PRESCALE));
					m_SBcount = 0;
				}
				m_sf = 0;
				break;

			case 0x28: /* tstc ZCS:..x */
				m_st = m_cf ^ 1;
				break;

			case 0x29: /* tstz ZCS:..x */
				m_st = m_zf ^ 1;
				break;

			case 0x2a: /* sts ZCS:x.. */
				WRMEM(GETEA(),m_SB);
				UPDATE_ZF(m_SB);
				m_st = 1;
				break;

			case 0x2b: /* ls ZCS:x.. */
				m_SB = RDMEM(GETEA());
				UPDATE_ZF(m_SB);
				m_st = 1;
				break;

			case 0x2c: /* rts ZCS:... */
				m_SI = ( m_SI - 1 ) & 3;
				m_PC = m_SP[m_SI] & 0x3f;
				m_PA = (m_SP[m_SI] >> 6) & 0x1f;
				m_st = 1;
				break;

			case 0x2d: /* neg ZCS: ..x */
				m_A = (~m_A)+1;
				m_A &= 0x0f;
				UPDATE_ST_Z(m_A);
				break;

			case 0x2e: /* c ZCS:xxx */
				arg = RDMEM(GETEA());
				arg -= m_A;
				UPDATE_CF(arg);
				arg &= 0x0f;
				UPDATE_ST_Z(arg);
				m_zf = m_st ^ 1;
				break;

			case 0x2f: /* eor ZCS:x.x */
				m_A ^= RDMEM(GETEA());
				UPDATE_ST_Z(m_A);
				m_zf = m_st ^ 1;
				break;

			case 0x30: case 0x31: case 0x32: case 0x33: /* sbit ZCS:... */
				arg = RDMEM(GETEA());
				WRMEM(GETEA(), arg | (1 << (opcode&3)));
				m_st = 1;
				break;

			case 0x34: case 0x35: case 0x36: case 0x37: /* rbit ZCS:... */
				arg = RDMEM(GETEA());
				WRMEM(GETEA(), arg & ~(1 << (opcode&3)));
				m_st = 1;
				break;

			case 0x38: case 0x39: case 0x3a: case 0x3b: /* tbit ZCS:... */
				arg = RDMEM(GETEA());
				m_st = ( arg & (1 << (opcode&3) ) ) ? 0 : 1;
				break;

			case 0x3c: /* rti ZCS:... */
				/* restore address and saved state flags on the top bits of the stack */
				m_in_irq = 0;
				m_SI = ( m_SI - 1 ) & 3;
				m_PC = m_SP[m_SI] & 0x3f;
				m_PA = (m_SP[m_SI] >> 6) & 0x1f;
				m_st = (m_SP[m_SI] >> 13)&1;
				m_zf = (m_SP[m_SI] >> 14)&1;
				m_cf = (m_SP[m_SI] >> 15)&1;
				break;

			case 0x3d: /* jpa imm ZCS:..x */
				m_PA = READOP(GETPC()) & 0x1f;
				m_PC = m_A * 4;
				oc = 2;
				m_st = 1;
				break;

			case 0x3e: /* en imm ZCS:... */
				update_pio_enable(m_pio | READOP(GETPC()));
				INCPC();
				oc = 2;
				m_st = 1;
				break;

			case 0x3f: /* dis imm ZCS:... */
				update_pio_enable(m_pio & ~(READOP(GETPC())));
				INCPC();
				oc = 2;
				m_st = 1;
				break;

			case 0x40:  case 0x41:  case 0x42:  case 0x43: /* setD ZCS:... */
				arg = m_read_r[0]() & 0x0f;
				arg |= (1 << (opcode&3));
				m_write_r[0](arg);
				m_st = 1;
				break;

			case 0x44:  case 0x45:  case 0x46:  case 0x47: /* rstD ZCS:... */
				arg = m_read_r[0]() & 0x0f;
				arg &= ~(1 << (opcode&3));
				m_write_r[0](arg);
				m_st = 1;
				break;

			case 0x48:  case 0x49:  case 0x4a:  case 0x4b: /* tstD ZCS:..x */
				arg = m_read_r[2]() & 0x0f;
				m_st = (arg & (1 << (opcode&3))) ? 0 : 1;
				break;

			case 0x4c:  case 0x4d:  case 0x4e:  case 0x4f: /* tba ZCS:..x */
				m_st = (m_A & (1 << (opcode&3))) ? 0 : 1;
				break;

			case 0x50:  case 0x51:  case 0x52:  case 0x53: /* xd ZCS:x.. */
				arg = RDMEM(opcode&3);
				WRMEM((opcode&3),m_A);
				m_A = arg;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0x54:  case 0x55:  case 0x56:  case 0x57: /* xyd ZCS:x.. */
				arg = RDMEM((opcode&3)+4);
				WRMEM((opcode&3)+4,m_Y);
				m_Y = arg;
				UPDATE_ZF(m_Y);
				m_st = 1;
				break;

			case 0x58:  case 0x59:  case 0x5a:  case 0x5b:
			case 0x5c:  case 0x5d:  case 0x5e:  case 0x5f: /* lxi ZCS:x.. */
				m_X = opcode & 7;
				UPDATE_ZF(m_X);
				m_st = 1;
				break;

			case 0x60:  case 0x61:  case 0x62:  case 0x63:
			case 0x64:  case 0x65:  case 0x66:  case 0x67: /* call imm ZCS:..x */
				arg = READOP(GETPC());
				INCPC();
				oc = 2;
				if ( TEST_ST() )
				{
					m_SP[m_SI] = GETPC();
					m_SI = ( m_SI + 1 ) & 3;
					m_PC = arg & 0x3f;
					m_PA = ( ( opcode & 7 ) << 2 ) | ( arg >> 6 );
				}
				m_st = 1;
				break;

			case 0x68:  case 0x69:  case 0x6a:  case 0x6b:
			case 0x6c:  case 0x6d:  case 0x6e:  case 0x6f: /* jpl imm ZCS:..x */
				arg = READOP(GETPC());
				INCPC();
				oc = 2;
				if ( TEST_ST() )
				{
					m_PC = arg & 0x3f;
					m_PA = ( ( opcode & 7 ) << 2 ) | ( arg >> 6 );
				}
				m_st = 1;
				break;

			case 0x70:  case 0x71:  case 0x72:  case 0x73:
			case 0x74:  case 0x75:  case 0x76:  case 0x77:
			case 0x78:  case 0x79:  case 0x7a:  case 0x7b:
			case 0x7c:  case 0x7d:  case 0x7e:  case 0x7f: /* ai ZCS:xxx */
				arg = opcode & 0x0f;
				arg += m_A;
				UPDATE_ST_C(arg);
				m_cf = m_st ^ 1;
				m_A = arg & 0x0f;
				UPDATE_ZF(m_A);
				break;

			case 0x80:  case 0x81:  case 0x82:  case 0x83:
			case 0x84:  case 0x85:  case 0x86:  case 0x87:
			case 0x88:  case 0x89:  case 0x8a:  case 0x8b:
			case 0x8c:  case 0x8d:  case 0x8e:  case 0x8f: /* lxi ZCS:x.. */
				m_Y = opcode & 0x0f;
				UPDATE_ZF(m_Y);
				m_st = 1;
				break;

			case 0x90:  case 0x91:  case 0x92:  case 0x93:
			case 0x94:  case 0x95:  case 0x96:  case 0x97:
			case 0x98:  case 0x99:  case 0x9a:  case 0x9b:
			case 0x9c:  case 0x9d:  case 0x9e:  case 0x9f: /* li ZCS:x.. */
				m_A = opcode & 0x0f;
				UPDATE_ZF(m_A);
				m_st = 1;
				break;

			case 0xa0:  case 0xa1:  case 0xa2:  case 0xa3:
			case 0xa4:  case 0xa5:  case 0xa6:  case 0xa7:
			case 0xa8:  case 0xa9:  case 0xaa:  case 0xab:
			case 0xac:  case 0xad:  case 0xae:  case 0xaf: /* cyi ZCS:xxx */
				arg = (opcode & 0x0f) - m_Y;
				UPDATE_CF(arg);
				arg &= 0x0f;
				UPDATE_ST_Z(arg);
				m_zf = m_st ^ 1;
				break;

			case 0xb0:  case 0xb1:  case 0xb2:  case 0xb3:
			case 0xb4:  case 0xb5:  case 0xb6:  case 0xb7:
			case 0xb8:  case 0xb9:  case 0xba:  case 0xbb:
			case 0xbc:  case 0xbd:  case 0xbe:  case 0xbf: /* ci ZCS:xxx */
				arg = (opcode & 0x0f) - m_A;
				UPDATE_CF(arg);
				arg &= 0x0f;
				UPDATE_ST_Z(arg);
				m_zf = m_st ^ 1;
				break;

			default: /* jmp ZCS:..x */
				if ( TEST_ST() )
				{
					m_PC = opcode & 0x3f;
				}
				m_st = 1;
				break;
		}

		/* update cycle counts */
		CYCLES( oc );

		/* update interrupts, serial and timer flags */
		update_pio(oc);
	}

	nCycles = nCycles - m_icount;

	segment_cycles = m_icount = 0;

	total_cycles += nCycles;

	return nCycles;
}

INT32 mb88xxIdle(INT32 cycles)
{
	total_cycles += cycles;

	return cycles;
}

void mb88xxNewFrame()
{
	total_cycles = segment_cycles = m_icount = 0;
}

INT32 mb88xxTotalCycles()
{
	return total_cycles + (segment_cycles - m_icount);
}
