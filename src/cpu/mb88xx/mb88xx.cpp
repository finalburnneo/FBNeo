// license:BSD-3-Clause
// copyright-holders:Ernesto Corvi
/***************************************************************************

    mb88xx.cpp
    Core implementation for the portable Fujitsu MB88xx series MCU emulator.

    Written by Ernesto Corvi

***************************************************************************/

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


typedef UINT8 (*read_function)();
typedef void  (*write_function)(UINT8 data);
typedef INT32 (*read_line_function)();
typedef void  (*write_line_function)(INT32 line);

#define MB88_MAX_CPU	4

struct mb88_context {
	UINT8 PC;     /* Program Counter: 6 bits */
	UINT8 PA;     /* Page Address: 4 bits */
	UINT16 SP[4];  /* Stack is 4*10 bit addresses deep, but we also use 3 top bits per address to store flags during irq */
	UINT8 SI;     /* Stack index: 2 bits */
	UINT8 A;      /* Accumulator: 4 bits */
	UINT8 X;      /* Index X: 4 bits */
	UINT8 Y;      /* Index Y: 4 bits */
	UINT8 st;     /* State flag: 1 bit */
	UINT8 zf;     /* Zero flag: 1 bit */
	UINT8 cf;     /* Carry flag: 1 bit */
	UINT8 vf;     /* Timer overflow flag: 1 bit */
	UINT8 sf;     /* Serial Full/Empty flag: 1 bit */
	UINT8 nf;     /* Interrupt flag: 1 bit */

	/* Peripheral Control */
	UINT8 pio; /* Peripheral enable bits: 8 bits */

	/* Timer registers */
	UINT8 TH; /* Timer High: 4 bits */
	UINT8 TL; /* Timer Low: 4 bits */
	UINT8 TP; /* Timer Prescale: 6 bits? */
	UINT8 ctr; /* current external counter value */

	/* Serial registers */
	UINT8 SB; /* Serial buffer: 4 bits */
	UINT16 SBcount;    /* number of bits received */
	UINT8 serial_active; /* serial shift clock running */

	UINT8 pending_interrupt;

	INT32 icount;
	INT32 total_cycles;
	INT32 segment_cycles;
	INT32 end_run;
	INT32 in_irq;
	INT32 in_reset; // last non-config / scanned(state) variable!!

	// config, not scanned

	INT32 program_rom_mask;
	INT32 program_ram_mask;	// ram region!
	UINT8 *program_ram;
	UINT8 *program_rom; // largest is 1 << 11

	UINT8 *PLA;

	read_function read_k;
	write_function write_o;
	write_function write_p;
	read_function read_r[4];
	write_function write_r[4];
	write_line_function write_so;
	read_line_function read_si;
};

static mb88_context mb88_ctx[MB88_MAX_CPU];
static mb88_context *mb88 = NULL;
static INT32 mb88_initted[MB88_MAX_CPU];
static INT32 mb88_active = -1;

static void mb88xx_close()
{
	mb88_active = -1;
	mb88 = NULL;
}

static void mb88xx_load_context(INT32 n)
{
	mb88_active = n;
	mb88 = &mb88_ctx[n];
}

#if defined FBNEO_DEBUG
#define MB88_CHECK_OPEN(fn) \
	if (mb88_active == -1) bprintf(PRINT_ERROR, _T(fn " called when no CPU open\n"));
#else
#define MB88_CHECK_OPEN(fn)
#endif

void mb88xxOpen(INT32 nCPU)
{
	if (nCPU < 0 || nCPU >= MB88_MAX_CPU || !mb88_initted[nCPU]) {
		bprintf(PRINT_ERROR, _T("mb88xxOpen: cpu %d not initialized\n"), nCPU);
		return;
	}
#if defined FBNEO_DEBUG
	if (mb88_active != -1) bprintf(PRINT_ERROR, _T("mb88xxOpen(%d) called while cpu %d is open\n"), nCPU, mb88_active);
#endif

	mb88xx_load_context(nCPU);
}

void mb88xxClose()
{
#if defined FBNEO_DEBUG
	if (mb88_active == -1) bprintf(PRINT_ERROR, _T("mb88xxClose called when no CPU open\n"));
#endif

	mb88xx_close();
}

INT32 mb88xxGetActive()
{
	return mb88_active;
}

void mb88xxSetSerialReadWriteFunction(void (*write)(INT32 line), INT32 (*read)())
{
	MB88_CHECK_OPEN("mb88xxSetSerialReadWriteFunction")

	if (write) mb88->write_so = write;
	if (read) mb88->read_si = read;
}

void mb88xxSetReadWriteFunction(void (*write)(UINT8), UINT8 (*read)(), char select, INT32 which)
{
	MB88_CHECK_OPEN("mb88xxSetReadWriteFunction")

	switch (select)
	{
		case 'k':
		case 'K':
			if (read) mb88->read_k = read;
		break;

		case 'o':
		case 'O':
			if (write) mb88->write_o = write;
		break;

		case 'p':
		case 'P':
			if (write) mb88->write_p = write;
		break;

		case 'r':
		case 'R':
			if (write) mb88->write_r[which & 3] = write;
			if (read) mb88->read_r[which & 3] = read;
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

#define READOP(a)           (mb88->program_rom[(a) & mb88->program_rom_mask])

#define RDMEM(a)            (mb88->program_ram[(a) & mb88->program_ram_mask] & 0x0f)
#define WRMEM(a,v)          (mb88->program_ram[(a) & mb88->program_ram_mask] = ((v) & 0x0f))

#define TEST_ST()           (mb88->st & 1)
#define TEST_ZF()           (mb88->zf & 1)
#define TEST_CF()           (mb88->cf & 1)
#define TEST_VF()           (mb88->vf & 1)
#define TEST_SF()           (mb88->sf & 1)
#define TEST_NF()           (mb88->nf & 1)

#define UPDATE_ST_C(v)      mb88->st=(v&0x10) ? 0 : 1
#define UPDATE_ST_Z(v)      mb88->st=(v==0) ? 0 : 1

#define UPDATE_CF(v)        mb88->cf=((v&0x10)==0) ? 0 : 1
#define UPDATE_ZF(v)        mb88->zf=(v!=0) ? 0 : 1

#define CYCLES(x)           do { mb88->icount -= (x); } while (0)

#define GETPC()             (((int)mb88->PA << 6)+mb88->PC)
#define GETEA()             ((mb88->X << 4)+mb88->Y)

#define INCPC()             do { mb88->PC++; if ( mb88->PC >= 0x40 ) { mb88->PC = 0; mb88->PA++; } } while (0)


static void dummy_write(UINT8) {}
static UINT8 dummy_read() { return 0; }
static void dummy_line_write(INT32) {}
static INT32 dummy_line_read() { return 0; }

void mb88xxInit(INT32 nCPU, INT32 nType, UINT8 *rom)
{
	if (nCPU < 0 || nCPU >= MB88_MAX_CPU) {
		bprintf (PRINT_ERROR, _T("mb88xxInit: bad cpu %d (max %d)\n"), nCPU, MB88_MAX_CPU);
		return;
	}

	memset(&mb88_ctx[nCPU], 0, sizeof(mb88_ctx[nCPU]));
	mb88xx_load_context(nCPU);

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

	mb88->program_rom_mask = (1 << program_width) - 1;
	mb88->program_ram_mask = (1 << data_width) - 1;	// ram region!!

	mb88->program_rom = (UINT8*)BurnMalloc(1 << program_width);
	mb88->program_ram = (UINT8*)BurnMalloc(1 << data_width);

	memcpy (mb88->program_rom, rom, 1 << program_width);

	mb88->PLA = NULL;
	mb88->ctr = 0;
//	mb88->serial = timer_alloc(FUNC(mb88_cpu_device::serial_timer), this);

	mb88->read_k = dummy_read;

	mb88->write_o = dummy_write;
	mb88->write_p = dummy_write;
	for (INT32 i = 0; i < 4; i++) {
		mb88->read_r[i] = dummy_read;
		mb88->write_r[i] = dummy_write;
	}
	mb88->read_si = dummy_line_read;
	mb88->write_so = dummy_line_write;

	// leave out of reset: -dink
	mb88->end_run = 0;
	mb88->total_cycles = 0;
	mb88->segment_cycles = 0;
	mb88->icount = 0;

	mb88->in_reset = 0;

	mb88_initted[nCPU] = 1;

	mb88xx_close();
}

void mb88xxExit()
{
	for (INT32 i = 0; i < MB88_MAX_CPU; i++)
	{
		if (!mb88_initted[i]) continue;

		mb88xx_load_context(i);

		BurnFree(mb88->program_rom);
		BurnFree(mb88->program_ram);

		mb88_initted[i] = 0;
	}

	memset(&mb88_ctx, 0, sizeof(mb88_ctx));
	mb88xx_close();
	mb88_active = -1;
}

/***************************************************************************
    INITIALIZATION AND SHUTDOWN
***************************************************************************/

void mb88xxReset()
{
	MB88_CHECK_OPEN("mb88xxReset")

	/* zero registers and flags */
	mb88->PC = 0;
	mb88->PA = 0;
	mb88->SP[0] = mb88->SP[1] = mb88->SP[2] = mb88->SP[3] = 0;
	mb88->SI = 0;
	mb88->A = 0;
	mb88->X = 0;
	mb88->Y = 0;
	mb88->st = 1;   /* start off with st=1 */
	mb88->zf = 0;
	mb88->cf = 0;
	mb88->vf = 0;
	mb88->sf = 0;
	mb88->nf = 0;
	mb88->pio = 0;
	mb88->TH = 0;
	mb88->TL = 0;
	mb88->TP = 0;
	mb88->SB = 0;
	mb88->SBcount = 0;
	mb88->serial_active = 0;
	mb88->pending_interrupt = 0;
	mb88->in_irq = 0;

	mb88->end_run = 0;
}

void mb88xxSetRESETLine(INT32 status)
{
	MB88_CHECK_OPEN("mb88xxSetRESETLine")

	if (mb88->in_reset && status == 0) {
		mb88xxReset();
	}

	mb88->in_reset = status;
}

void mb88xxScan(INT32 nAction)
{
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return;
	}

	char szTextRam[] = "mb88xx #0 ram";
	char szTextVars[] = "mb88xx #0 vars";

	for (INT32 i = 0; i < MB88_MAX_CPU; i++)
	{
		if (!mb88_initted[i]) continue;
		szTextRam[8] = '0' + i;
		szTextVars[8] = '0' + i;
		ScanVar(mb88_ctx[i].program_ram, mb88_ctx[i].program_ram_mask + 1, szTextRam);

		ScanVar(&mb88_ctx[i], STRUCT_SIZE_HELPER(mb88_context, in_reset), szTextVars);
	}
}

/***************************************************************************
    CORE EXECUTION LOOP
***************************************************************************/

// the serial clock is clock() / SERIAL_PRESCALE; one tick per instruction cycle here
// (instruction cycles are clock() / 6, as is SERIAL_PRESCALE)
static void mb88_serial_timer()
{
	mb88->SBcount++;

	/* if we get too many interrupts with no servicing, disable the timer
	   until somebody does something */
	if (mb88->SBcount >= SERIAL_DISABLE_THRESH)
		mb88->serial_active = 0;

	/* only read if not full; this is needed by the Namco 52xx to ensure that
	   the program can write to S and recover the value even if serial is enabled */
	if (!mb88->sf)
	{
		mb88->SB = (mb88->SB >> 1) | (mb88->read_si() ? 8 : 0);

		if (mb88->SBcount >= 4)
		{
			mb88->sf = 1;
			mb88->pending_interrupt |= INT_CAUSE_SERIAL;
		}
	}
}

static int pla( int inA, int inB )
{
	int index = ((inB&1) << 4) | (inA&0x0f);

	if ( mb88->PLA )
		return mb88->PLA[index];

	return index;
}

void mb88xxSetIRQLine(int /*inputnum*/, int state)
{
	MB88_CHECK_OPEN("mb88xxSetIRQLine")

	/* On rising edge trigger interrupt.
	 * Note this is a logical level, the actual pin is high-to-low voltage
	 * triggered. */
	if ( (mb88->pio & INT_CAUSE_EXTERNAL) && !mb88->nf && state != CLEAR_LINE )
	{
		mb88->pending_interrupt |= INT_CAUSE_EXTERNAL;
	}

	mb88->nf = state != CLEAR_LINE;
}

static void update_pio_enable( UINT8 newpio )
{
	/* if the serial state has changed, configure the timer */
	if ((mb88->pio ^ newpio) & 0x30)
	{
		if ((newpio & 0x30) == 0)
			mb88->serial_active = 0;
		else if ((newpio & 0x30) == 0x20)
			mb88->serial_active = 1;
	//	else
	//		fatalerror("mb88xx: update_pio_enable set serial enable to unsupported value %02X\n", newpio & 0x30);
	}

	mb88->pio = newpio;
}

static void increment_timer()
{
	mb88->TL = (mb88->TL + 1) & 0x0f;
	if (mb88->TL == 0)
	{
		mb88->TH = (mb88->TH + 1) & 0x0f;
		if (mb88->TH == 0)
		{
			mb88->vf = 1;
			mb88->pending_interrupt |= INT_CAUSE_TIMER;
		}
	}
}

static void update_pio( int cycles )
{
	/* TODO: improve/validate serial and timer support */

	/* serial shift clock */
	for (INT32 i = 0; i < cycles && mb88->serial_active; i++)
		mb88_serial_timer();

	/* internal clock enable */
	if ( mb88->pio & 0x80 )
	{
		mb88->TP += cycles;
		while (mb88->TP >= TIMER_PRESCALE)
		{
			mb88->TP -= TIMER_PRESCALE;
			increment_timer();
		}
	}

	/* process pending interrupts */
	if (!mb88->in_irq && mb88->pending_interrupt & mb88->pio)
	{
		mb88->in_irq = 1;
		UINT16 intpc = GETPC();

		mb88->SP[mb88->SI] = intpc;
		mb88->SP[mb88->SI] |= TEST_CF() << 15;
		mb88->SP[mb88->SI] |= TEST_ZF() << 14;
		mb88->SP[mb88->SI] |= TEST_ST() << 13;
		mb88->SI = ( mb88->SI + 1 ) & 3;

		/* the datasheet doesn't mention interrupt vectors but
		the Arabian MCU program expects the following */
		if (mb88->pending_interrupt & mb88->pio & INT_CAUSE_EXTERNAL)
		{
			/* if we have a live external source, call the irqcallback */
			//standard_irq_callback( 0, intpc );
			mb88->PC = 0x02;
		}
		else if (mb88->pending_interrupt & mb88->pio & INT_CAUSE_TIMER)
		{
			//standard_irq_callback( 1, intpc );
			mb88->PC = 0x04;
		}
		else if (mb88->pending_interrupt & mb88->pio & INT_CAUSE_SERIAL)
		{
			//standard_irq_callback( 2, intpc );
			mb88->PC = 0x06;
		}

		mb88->PA = 0x00;
		mb88->st = 1;
		mb88->pending_interrupt = 0;

		CYCLES(3); /* ? */
	}
}

void mb88xx_clock_write(INT32 state)
{
	MB88_CHECK_OPEN("mb88xx_clock_write")

	if (state != mb88->ctr)
	{
		mb88->ctr = state;

		/* on a falling clock, increment the timer, but only if enabled */
		if (mb88->ctr == 0 && (mb88->pio & 0x40))
			increment_timer();
	}
}

void mb88xx_set_pla(UINT8 *pla)
{
	MB88_CHECK_OPEN("mb88xx_set_pla")

	mb88->PLA = pla;
}

void mb88xxEndRun()
{
	MB88_CHECK_OPEN("mb88xxEndRun")

	mb88->end_run = 1;
}

INT32 mb88xxRun(INT32 nCycles)
{
	MB88_CHECK_OPEN("mb88xxRun")

	mb88->end_run = 0;
	mb88->segment_cycles = nCycles;

	mb88->icount = nCycles;

	if (mb88->in_reset) {
		mb88->icount = 0;
	}

	while (mb88->icount > 0 && !mb88->end_run)
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
				mb88->st = 1;
				break;

			case 0x01: /* outO ZCS:...*/
				mb88->write_o(pla(mb88->A, TEST_CF()));
				mb88->st = 1;
				break;

			case 0x02: /* outP ZCS:... */
				mb88->write_p(mb88->A);
				mb88->st = 1;
				break;

			case 0x03: /* outR ZCS:... */
				arg = mb88->Y;
				mb88->write_r[arg & 3](mb88->A);
				mb88->st = 1;
				break;

			case 0x04: /* tay ZCS:... */
				mb88->Y = mb88->A;
				mb88->st = 1;
				break;

			case 0x05: /* tath ZCS:... */
				mb88->TH = mb88->A;
				mb88->st = 1;
				break;

			case 0x06: /* tatl ZCS:... */
				mb88->TL = mb88->A;
				mb88->st = 1;
				break;

			case 0x07: /* tas ZCS:... */
				mb88->SB = mb88->A;
				mb88->st = 1;
				break;

			case 0x08: /* icy ZCS:x.x */
				mb88->Y++;
				UPDATE_ST_C(mb88->Y);
				mb88->Y &= 0x0f;
				UPDATE_ZF(mb88->Y);
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
				WRMEM(GETEA(),mb88->A);
				mb88->Y++;
				UPDATE_ST_C(mb88->Y);
				mb88->Y &= 0x0f;
				UPDATE_ZF(mb88->Y);
				break;

			case 0x0b: /* x ZCS:x.. */
				arg = RDMEM(GETEA());
				WRMEM(GETEA(),mb88->A);
				mb88->A = arg;
				UPDATE_ZF(mb88->A);
				mb88->st = 1;
				break;

			case 0x0c: /* rol ZCS:xxx */
				mb88->A <<= 1;
				mb88->A |= TEST_CF();
				UPDATE_ST_C(mb88->A);
				mb88->cf = mb88->st ^ 1;
				mb88->A &= 0x0f;
				UPDATE_ZF(mb88->A);
				break;

			case 0x0d: /* l ZCS:x.. */
				mb88->A = RDMEM(GETEA());
				UPDATE_ZF(mb88->A);
				mb88->st = 1;
				break;

			case 0x0e: /* adc ZCS:xxx */
				arg = RDMEM(GETEA());
				arg += mb88->A;
				arg += TEST_CF();
				UPDATE_ST_C(arg);
				mb88->cf = mb88->st ^ 1;
				mb88->A = arg & 0x0f;
				UPDATE_ZF(mb88->A);
				break;

			case 0x0f: /* and ZCS:x.x */
				mb88->A &= RDMEM(GETEA());
				UPDATE_ZF(mb88->A);
				mb88->st = mb88->zf ^ 1;
				break;

			case 0x10: /* daa ZCS:.xx */
				if ( TEST_CF() || mb88->A > 9 ) mb88->A += 6;
				UPDATE_ST_C(mb88->A);
				mb88->cf = mb88->st ^ 1;
				mb88->A &= 0x0f;
				break;

			case 0x11: /* das ZCS:.xx */
				if ( TEST_CF() || mb88->A > 9 ) mb88->A += 10;
				UPDATE_ST_C(mb88->A);
				mb88->cf = mb88->st ^ 1;
				mb88->A &= 0x0f;
				break;

			case 0x12: /* inK ZCS:x.. */
				mb88->A = mb88->read_k() & 0x0f;
				UPDATE_ZF(mb88->A);
				mb88->st = 1;
				break;

			case 0x13: /* inR ZCS:x.. */
				arg = mb88->Y;
				mb88->A = mb88->read_r[arg & 3]() & 0x0f;
				UPDATE_ZF(mb88->A);
				mb88->st = 1;
				break;

			case 0x14: /* tya ZCS:x.. */
				mb88->A = mb88->Y;
				UPDATE_ZF(mb88->A);
				mb88->st = 1;
				break;

			case 0x15: /* ttha ZCS:x.. */
				mb88->A = mb88->TH;
				UPDATE_ZF(mb88->A);
				mb88->st = 1;
				break;

			case 0x16: /* ttla ZCS:x.. */
				mb88->A = mb88->TL;
				UPDATE_ZF(mb88->A);
				mb88->st = 1;
				break;

			case 0x17: /* tsa ZCS:x.. */
				mb88->A = mb88->SB;
				UPDATE_ZF(mb88->A);
				mb88->st = 1;
				break;

			case 0x18: /* dcy ZCS:..x */
				mb88->Y--;
				UPDATE_ST_C(mb88->Y);
				mb88->Y &= 0x0f;
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
				WRMEM(GETEA(),mb88->A);
				mb88->Y--;
				UPDATE_ST_C(mb88->Y);
				mb88->Y &= 0x0f;
				UPDATE_ZF(mb88->Y);
				break;

			case 0x1b: /* xx ZCS:x.. */
				arg = mb88->X;
				mb88->X = mb88->A;
				mb88->A = arg;
				UPDATE_ZF(mb88->A);
				mb88->st = 1;
				break;

			case 0x1c: /* ror ZCS:xxx */
				mb88->A |= TEST_CF() << 4;
				UPDATE_ST_C(mb88->A << 4);
				mb88->cf = mb88->st ^ 1;
				mb88->A >>= 1;
				mb88->A &= 0x0f;
				UPDATE_ZF(mb88->A);
				break;

			case 0x1d: /* st ZCS:x.. */
				WRMEM(GETEA(),mb88->A);
				mb88->st = 1;
				break;

			case 0x1e: /* sbc ZCS:xxx */
				arg = RDMEM(GETEA());
				arg -= mb88->A;
				arg -= TEST_CF();
				UPDATE_ST_C(arg);
				mb88->cf = mb88->st ^ 1;
				mb88->A = arg & 0x0f;
				UPDATE_ZF(mb88->A);
				break;

			case 0x1f: /* or ZCS:x.x */
				mb88->A |= RDMEM(GETEA());
				UPDATE_ZF(mb88->A);
				mb88->st = mb88->zf ^ 1;
				break;

			case 0x20: /* setR ZCS:... */
				arg = mb88->read_r[mb88->Y/4]() & 0x0f;
				mb88->write_r[mb88->Y/4](arg | (1 << (mb88->Y%4)));
				mb88->st = 1;
				break;

			case 0x21: /* setc ZCS:.xx */
				mb88->cf = 1;
				mb88->st = 1;
				break;

			case 0x22: /* rstR ZCS:... */
				arg = mb88->read_r[mb88->Y/4]() & 0x0f;
				mb88->write_r[mb88->Y/4](arg & ~(1 << (mb88->Y%4)));
				mb88->st = 1;
				break;

			case 0x23: /* rstc ZCS:.xx */
				mb88->cf = 0;
				mb88->st = 1;
				break;

			case 0x24: /* tstr ZCS:..x */
				arg = mb88->read_r[mb88->Y/4]() & 0x0f;
				mb88->st = ( arg & ( 1 << (mb88->Y%4) ) ) ? 0 : 1;
				break;

			case 0x25: /* tsti ZCS:..x */
				mb88->st = mb88->nf ^ 1;
				break;

			case 0x26: /* tstv ZCS:..x */
				mb88->st = mb88->vf ^ 1;
				mb88->vf = 0;
				break;

			case 0x27: /* tsts ZCS:..x */
				mb88->st = mb88->sf ^ 1;
				if (mb88->sf)
				{
					/* re-enable the timer if we disabled it previously */
					if (mb88->SBcount >= SERIAL_DISABLE_THRESH && (mb88->pio & 0x30) == 0x20)
						mb88->serial_active = 1;
					mb88->SBcount = 0;
				}
				mb88->sf = 0;
				break;

			case 0x28: /* tstc ZCS:..x */
				mb88->st = mb88->cf ^ 1;
				break;

			case 0x29: /* tstz ZCS:..x */
				mb88->st = mb88->zf ^ 1;
				break;

			case 0x2a: /* sts ZCS:x.. */
				WRMEM(GETEA(),mb88->SB);
				UPDATE_ZF(mb88->SB);
				mb88->st = 1;
				break;

			case 0x2b: /* ls ZCS:x.. */
				mb88->SB = RDMEM(GETEA());
				UPDATE_ZF(mb88->SB);
				mb88->st = 1;
				break;

			case 0x2c: /* rts ZCS:... */
				mb88->SI = ( mb88->SI - 1 ) & 3;
				mb88->PC = mb88->SP[mb88->SI] & 0x3f;
				mb88->PA = (mb88->SP[mb88->SI] >> 6) & 0x1f;
				mb88->st = 1;
				break;

			case 0x2d: /* neg ZCS: ..x */
				mb88->A = (~mb88->A)+1;
				mb88->A &= 0x0f;
				UPDATE_ST_Z(mb88->A);
				break;

			case 0x2e: /* c ZCS:xxx */
				arg = RDMEM(GETEA());
				arg -= mb88->A;
				UPDATE_CF(arg);
				arg &= 0x0f;
				UPDATE_ST_Z(arg);
				mb88->zf = mb88->st ^ 1;
				break;

			case 0x2f: /* eor ZCS:x.x */
				mb88->A ^= RDMEM(GETEA());
				UPDATE_ST_Z(mb88->A);
				mb88->zf = mb88->st ^ 1;
				break;

			case 0x30: case 0x31: case 0x32: case 0x33: /* sbit ZCS:... */
				arg = RDMEM(GETEA());
				WRMEM(GETEA(), arg | (1 << (opcode&3)));
				mb88->st = 1;
				break;

			case 0x34: case 0x35: case 0x36: case 0x37: /* rbit ZCS:... */
				arg = RDMEM(GETEA());
				WRMEM(GETEA(), arg & ~(1 << (opcode&3)));
				mb88->st = 1;
				break;

			case 0x38: case 0x39: case 0x3a: case 0x3b: /* tbit ZCS:... */
				arg = RDMEM(GETEA());
				mb88->st = ( arg & (1 << (opcode&3) ) ) ? 0 : 1;
				break;

			case 0x3c: /* rti ZCS:... */
				/* restore address and saved state flags on the top bits of the stack */
				mb88->in_irq = 0;
				mb88->SI = ( mb88->SI - 1 ) & 3;
				mb88->PC = mb88->SP[mb88->SI] & 0x3f;
				mb88->PA = (mb88->SP[mb88->SI] >> 6) & 0x1f;
				mb88->st = (mb88->SP[mb88->SI] >> 13)&1;
				mb88->zf = (mb88->SP[mb88->SI] >> 14)&1;
				mb88->cf = (mb88->SP[mb88->SI] >> 15)&1;
				break;

			case 0x3d: /* jpa imm ZCS:..x */
				mb88->PA = READOP(GETPC()) & 0x1f;
				mb88->PC = mb88->A * 4;
				oc = 2;
				mb88->st = 1;
				break;

			case 0x3e: /* en imm ZCS:... */
				update_pio_enable(mb88->pio | READOP(GETPC()));
				INCPC();
				oc = 2;
				mb88->st = 1;
				break;

			case 0x3f: /* dis imm ZCS:... */
				update_pio_enable(mb88->pio & ~(READOP(GETPC())));
				INCPC();
				oc = 2;
				mb88->st = 1;
				break;

			case 0x40:  case 0x41:  case 0x42:  case 0x43: /* setD ZCS:... */
				arg = mb88->read_r[0]() & 0x0f;
				arg |= (1 << (opcode&3));
				mb88->write_r[0](arg);
				mb88->st = 1;
				break;

			case 0x44:  case 0x45:  case 0x46:  case 0x47: /* rstD ZCS:... */
				arg = mb88->read_r[0]() & 0x0f;
				arg &= ~(1 << (opcode&3));
				mb88->write_r[0](arg);
				mb88->st = 1;
				break;

			case 0x48:  case 0x49:  case 0x4a:  case 0x4b: /* tstD ZCS:..x */
				arg = mb88->read_r[2]() & 0x0f;
				mb88->st = (arg & (1 << (opcode&3))) ? 0 : 1;
				break;

			case 0x4c:  case 0x4d:  case 0x4e:  case 0x4f: /* tba ZCS:..x */
				mb88->st = (mb88->A & (1 << (opcode&3))) ? 0 : 1;
				break;

			case 0x50:  case 0x51:  case 0x52:  case 0x53: /* xd ZCS:x.. */
				arg = RDMEM(opcode&3);
				WRMEM((opcode&3),mb88->A);
				mb88->A = arg;
				UPDATE_ZF(mb88->A);
				mb88->st = 1;
				break;

			case 0x54:  case 0x55:  case 0x56:  case 0x57: /* xyd ZCS:x.. */
				arg = RDMEM((opcode&3)+4);
				WRMEM((opcode&3)+4,mb88->Y);
				mb88->Y = arg;
				UPDATE_ZF(mb88->Y);
				mb88->st = 1;
				break;

			case 0x58:  case 0x59:  case 0x5a:  case 0x5b:
			case 0x5c:  case 0x5d:  case 0x5e:  case 0x5f: /* lxi ZCS:x.. */
				mb88->X = opcode & 7;
				UPDATE_ZF(mb88->X);
				mb88->st = 1;
				break;

			case 0x60:  case 0x61:  case 0x62:  case 0x63:
			case 0x64:  case 0x65:  case 0x66:  case 0x67: /* call imm ZCS:..x */
				arg = READOP(GETPC());
				INCPC();
				oc = 2;
				if ( TEST_ST() )
				{
					mb88->SP[mb88->SI] = GETPC();
					mb88->SI = ( mb88->SI + 1 ) & 3;
					mb88->PC = arg & 0x3f;
					mb88->PA = ( ( opcode & 7 ) << 2 ) | ( arg >> 6 );
				}
				mb88->st = 1;
				break;

			case 0x68:  case 0x69:  case 0x6a:  case 0x6b:
			case 0x6c:  case 0x6d:  case 0x6e:  case 0x6f: /* jpl imm ZCS:..x */
				arg = READOP(GETPC());
				INCPC();
				oc = 2;
				if ( TEST_ST() )
				{
					mb88->PC = arg & 0x3f;
					mb88->PA = ( ( opcode & 7 ) << 2 ) | ( arg >> 6 );
				}
				mb88->st = 1;
				break;

			case 0x70:  case 0x71:  case 0x72:  case 0x73:
			case 0x74:  case 0x75:  case 0x76:  case 0x77:
			case 0x78:  case 0x79:  case 0x7a:  case 0x7b:
			case 0x7c:  case 0x7d:  case 0x7e:  case 0x7f: /* ai ZCS:xxx */
				arg = opcode & 0x0f;
				arg += mb88->A;
				UPDATE_ST_C(arg);
				mb88->cf = mb88->st ^ 1;
				mb88->A = arg & 0x0f;
				UPDATE_ZF(mb88->A);
				break;

			case 0x80:  case 0x81:  case 0x82:  case 0x83:
			case 0x84:  case 0x85:  case 0x86:  case 0x87:
			case 0x88:  case 0x89:  case 0x8a:  case 0x8b:
			case 0x8c:  case 0x8d:  case 0x8e:  case 0x8f: /* lxi ZCS:x.. */
				mb88->Y = opcode & 0x0f;
				UPDATE_ZF(mb88->Y);
				mb88->st = 1;
				break;

			case 0x90:  case 0x91:  case 0x92:  case 0x93:
			case 0x94:  case 0x95:  case 0x96:  case 0x97:
			case 0x98:  case 0x99:  case 0x9a:  case 0x9b:
			case 0x9c:  case 0x9d:  case 0x9e:  case 0x9f: /* li ZCS:x.. */
				mb88->A = opcode & 0x0f;
				UPDATE_ZF(mb88->A);
				mb88->st = 1;
				break;

			case 0xa0:  case 0xa1:  case 0xa2:  case 0xa3:
			case 0xa4:  case 0xa5:  case 0xa6:  case 0xa7:
			case 0xa8:  case 0xa9:  case 0xaa:  case 0xab:
			case 0xac:  case 0xad:  case 0xae:  case 0xaf: /* cyi ZCS:xxx */
				arg = (opcode & 0x0f) - mb88->Y;
				UPDATE_CF(arg);
				arg &= 0x0f;
				UPDATE_ST_Z(arg);
				mb88->zf = mb88->st ^ 1;
				break;

			case 0xb0:  case 0xb1:  case 0xb2:  case 0xb3:
			case 0xb4:  case 0xb5:  case 0xb6:  case 0xb7:
			case 0xb8:  case 0xb9:  case 0xba:  case 0xbb:
			case 0xbc:  case 0xbd:  case 0xbe:  case 0xbf: /* ci ZCS:xxx */
				arg = (opcode & 0x0f) - mb88->A;
				UPDATE_CF(arg);
				arg &= 0x0f;
				UPDATE_ST_Z(arg);
				mb88->zf = mb88->st ^ 1;
				break;

			default: /* jmp ZCS:..x */
				if ( TEST_ST() )
				{
					mb88->PC = opcode & 0x3f;
				}
				mb88->st = 1;
				break;
		}

		/* update cycle counts */
		CYCLES( oc );

		/* update interrupts, serial and timer flags */
		update_pio(oc);
	}

	nCycles = nCycles - mb88->icount;

	mb88->segment_cycles = mb88->icount = 0;

	mb88->total_cycles += nCycles;

	return nCycles;
}

INT32 mb88xxIdle(INT32 cycles)
{
	MB88_CHECK_OPEN("mb88xxIdle")

	mb88->total_cycles += cycles;

	return cycles;
}

void mb88xxNewFrame()
{
	for (INT32 i = 0; i < MB88_MAX_CPU; i++)
	{
		if (!mb88_initted[i]) continue;

		mb88_ctx[i].total_cycles = mb88_ctx[i].segment_cycles = mb88_ctx[i].icount = 0;
	}
}

INT32 mb88xxTotalCycles()
{
	MB88_CHECK_OPEN("mb88xxTotalCycles")

	return mb88->total_cycles + (mb88->segment_cycles - mb88->icount);
}
