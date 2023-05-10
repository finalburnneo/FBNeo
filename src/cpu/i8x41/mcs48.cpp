// license:BSD-3-Clause
// copyright-holders:Dan Boris, Mirko Buffoni, Aaron Giles, Couriersud
/***************************************************************************

    Intel MCS-48/UPI-41 Portable Emulator

    Copyright Mirko Buffoni
    Based on the original work Copyright Dan Boris, an 8048 emulator

    TODO:
    - EA pin - defined by architecture, must implement:
      1 means external access, bypassing internal ROM
      reimplement as a push, not a pull
    - add CMOS devices, 1 new opcode (01 IDL)
    - add special 8022 opcodes (RAD, SEL AN0, SEL AN1, RETI)
    - according to the user manual, some opcodes(dis/enable timer/interrupt)
      don't increment the timer, does it affect the prescaler too?
    - IRQ timing is hacked due to WY-100 needing to take JNI branch before servicing interrupt

****************************************************************************

    Note that the default internal divisor for this chip is by 3 and
    then again by 5, or by 15 total.

    Chip   RAM  ROM  I/O
    ----   ---  ---  ---
    8021    64   1k   21  (ROM, reduced instruction set)
    8022    64   2k   26  (ROM, reduced instruction set, analog comparator)

    8035    64    0   27  (external ROM)
    8048    64   1k   27  (ROM)
    8648    64   1k   27  (OTPROM)
    8748    64   1k   27  (EPROM)
    8884    64   1k
    N7751  128   2k

    8039   128    0   27  (external ROM)
    8049   128   2k   27  (ROM)
    8749   128   2k   27  (EPROM)
    M58715 128    0       (external ROM)

    8040   256   4k   27  (external ROM)
    8050   256   4k   27  (ROM)

****************************************************************************

    UPI-41/42 chips are MCS-48 derived, with some opcode changes:

            MCS-48 opcode       UPI-41/42 opcode
            -------------       ----------------
        02: OUTL BUS,A          OUT  DBB,A
        08: INS  BUS,A          <illegal>
        22: <illegal>           IN   DBB,A
        75: ENT0 CLK            <illegal>
        80: MOVX A,@R0          <illegal>
        81: MOVX A,@R1          <illegal>
        86: JNI  <dest>         JOBF <dest>
        88: ORL  BUS,#n         <illegal>
        90: MOVX @R0,A          MOV  STS,A
        91: MOVX @R1,A          <illegal>
        98: ANL  BUS,#n         <illegal>
        D6: <illegal>           JNIBF <dest>
        E5: SEL  MB0            EN   DMA
        F5: SEL  MB1            EN   FLAGS

    Chip numbers are similar to the MCS-48 series:

    Chip   RAM  ROM  I/O
    ----   ---  ---  ---
    8041A   64   1k       (ROM)
    8041AH 128   1k       (ROM)
    8641A   64   1k       (OTPROM)
    8741A   64   1k       (EPROM)
    8741AH 128   1k       (EPROM)

    8042   128   2k       (ROM)
    8042AH 256   2k       (ROM)
    8642   128   2k       (OTPROM)
    8742   128   2k       (EPROM)
    8742AH 256   2k       (EPROM)

***************************************************************************/

#include "burnint.h"
#include "mcs48.h"
#include <stddef.h>

#define CLEAR_LINE	0
#define ASSERT_LINE 1

#define MAX_MCS48   6


/***************************************************************************
    CONSTANTS
***************************************************************************/

/* timer/counter enable bits */
#define TIMER_ENABLED   0x01
#define COUNTER_ENABLED 0x02

/* flag bits */
#define C_FLAG          0x80
#define A_FLAG          0x40
#define F_FLAG          0x20
#define B_FLAG          0x10

/* status bits (UPI-41) */
#define STS_IBF         0x02
#define STS_OBF         0x01

/* port 2 bits (UPI-41) */
#define P2_OBF          0x10
#define P2_NIBF         0x20
#define P2_DRQ          0x40
#define P2_NDACK        0x80

/* enable bits (UPI-41) */
#define ENABLE_FLAGS    0x01
#define ENABLE_DMA      0x02

/* feature masks */
#define MB_FEATURE      0x01
#define EXT_BUS_FEATURE 0x02
#define UPI41_FEATURE   0x04
#define I802X_FEATURE   0x08
#define I8048_FEATURE   (MB_FEATURE | EXT_BUS_FEATURE)


typedef void (*mcs48_ophandler)();
extern const mcs48_ophandler s_upi41_opcodes[256];
extern const mcs48_ophandler s_mcs48_opcodes[256];

bool cflyball_hack = false;	// If true, gladiatr's coin1 & coin2 dip will not be controlled (the actual keypress result will always be 1c/5c & 2c/1c)

struct MCS48_t {
	UINT16      prevpc;             /* 16-bit previous program counter */
	UINT16      pc;                 /* 16-bit program counter */

	UINT8       a;                  /* 8-bit accumulator */
	UINT8       psw;                /* 8-bit psw */
	bool        f1;                 /* F1 flag (F0 is in PSW) */
	UINT8       p1;                 /* 8-bit latched port 1 */
	UINT8       p2;                 /* 8-bit latched port 2 */
	UINT8       ea;                 /* 1-bit latched ea input */
	UINT8       timer;              /* 8-bit timer */
	UINT8       prescaler;          /* 5-bit timer prescaler */
	UINT8       t1_history;         /* 8-bit history of the T1 input */
	UINT8       sts;                /* 4-bit status register + OBF/IBF flags (UPI-41 only) */
	UINT8       dbbi;               /* 8-bit input data buffer (UPI-41 only) */
	UINT8       dbbo;               /* 8-bit output data buffer (UPI-41 only) */

	bool        irq_state;          /* true if the IRQ line is active */
	bool        irq_polled;         /* true if last instruction was JNI (and not taken) */
	bool        irq_in_progress;    /* true if an IRQ is in progress */
	bool        timer_overflow;     /* true on a timer overflow; cleared by taking interrupt */
	bool        timer_flag;         /* true on a timer overflow; cleared on JTF */
	bool        tirq_enabled;       /* true if the timer IRQ is enabled */
	bool        xirq_enabled;       /* true if the external IRQ is enabled */
	UINT8       timecount_enabled;  /* bitmask of timer/counter enabled */
	bool        flags_enabled;      /* true if I/O flags have been enabled (UPI-41 only) */
	bool        dma_enabled;        /* true if DMA has been enabled (UPI-41 only) */

	UINT16      a11;                /* A11 value, either 0x000 or 0x800 */

	INT32       icount;
	INT32       total_cycles;
	INT32       cycle_start;
	INT32 	    end_run;

	UINT8       ram[256];

	INT32       ptr_divider;

	// configuration / non state-able stuff below
	UINT32      subtype;
	UINT8       feature_mask;
	UINT32      ram_mask;
	UINT32      prg_mask;
	UINT8       *prg;

	UINT8       *regptr;             /* pointer to r0-r7 */

	void		(*io_write_byte_8)(UINT32 p, UINT8 d);
	UINT8		(*io_read_byte_8)(UINT32 p);
	const mcs48_ophandler *opcode_table;
};

static MCS48_t *mcs48 = NULL;
static MCS48_t mcs48_state_store[MAX_MCS48];
static INT32 mcs48_active_cpu = -1;
static INT32 mcs48_total_cpus = 0; // nCpu + 1!

/***************************************************************************
    MACROS
***************************************************************************/

/* r0-r7 map to memory via the regptr */
#define R0              mcs48->regptr[0]
#define R1              mcs48->regptr[1]
#define R2              mcs48->regptr[2]
#define R3              mcs48->regptr[3]
#define R4              mcs48->regptr[4]
#define R5              mcs48->regptr[5]
#define R6              mcs48->regptr[6]
#define R7              mcs48->regptr[7]

/* ROM is mapped to AS_PROGRAM */
static UINT8 program_r(INT32 a)         { return mcs48->prg[a & mcs48->prg_mask]; }

/* RAM is mapped to AS_DATA */
static UINT8 ram_r(INT32 a)             { return mcs48->ram[a & mcs48->ram_mask]; }
static void  ram_w(INT32 a, UINT8 v)    { mcs48->ram[a & mcs48->ram_mask] = v; }

/* ports are mapped to AS_IO and callbacks */
static UINT8 ext_r(INT32 a)             { return mcs48->io_read_byte_8(a); }
static void  ext_w(INT32 a, UINT8 v)    { mcs48->io_write_byte_8(a, v); }
static UINT8 port_r(INT32 a)            { return mcs48->io_read_byte_8(MCS48_P1 + (a - 1)); }
static void  port_w(INT32 a, UINT8 v)   { mcs48->io_write_byte_8(MCS48_P1 + (a - 1), v); }
static int   test_r(INT32 a)            { return mcs48->io_read_byte_8(MCS48_T0 + a); }
static UINT8 bus_r()                    { return mcs48->io_read_byte_8(MCS48_BUS); }
static void  bus_w(UINT8 v)             { mcs48->io_write_byte_8(MCS48_BUS, v); }
static void  prog_w(UINT8 v)            { mcs48->io_write_byte_8(MCS48_PROG, v); }

enum expander_op
{
	EXPANDER_OP_READ = 0,
	EXPANDER_OP_WRITE = 1,
	EXPANDER_OP_OR = 2,
	EXPANDER_OP_AND = 3
};

static void mcs48_dummy_write_port(UINT32, UINT8) { }
static UINT8 mcs48_dummy_read_port(UINT32) { return 0xff; }

void mcs48_set_read_port(UINT8 (*read)(UINT32))
{
	mcs48->io_read_byte_8 = read;
}

void mcs48_set_write_port(void (*write)(UINT32, UINT8))
{
	mcs48->io_write_byte_8 = write;
}

void mcs48Scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		for (INT32 i = 0; i < mcs48_total_cpus; i++) {
			MCS48_t *ptr = &mcs48_state_store[i];

			ScanVar(ptr, STRUCT_SIZE_HELPER(MCS48_t, ptr_divider), "mcs48 RegsAndRAM");
		}
	}
}

INT32 mcs48GetActive()
{
	return mcs48_active_cpu;
}

void mcs48Open(INT32 nCpu)
{
	if (mcs48_active_cpu != -1)	{
		bprintf(PRINT_ERROR, _T("mcs48Open(%d); when cpu already open.\n"), nCpu);
	}

	mcs48 = &mcs48_state_store[nCpu];
	mcs48_active_cpu = nCpu;
}

void mcs48Close()
{
	if (mcs48_active_cpu == -1)	{
		bprintf(PRINT_ERROR, _T("mcs48Close(); when cpu already closed.\n"));
	}

	mcs48 = NULL;
	mcs48_active_cpu = -1;
}

void mcs48Init(INT32 nCpu, INT32 subtype, UINT8 *prg)
{
	if (nCpu >= MAX_MCS48) {
		bprintf(PRINT_ERROR, _T("mcs48Init(%d, x); cpu number too high, increase MAX_MCS48.\n"), nCpu);
	}

	mcs48_total_cpus = nCpu + 1;

	mcs48Open(nCpu);
	memset(mcs48, 0x00, sizeof(MCS48_t));
	mcs48->subtype = subtype;
	mcs48->prg = prg;
	/* External access line
	 * EA=1 : read from external rom
	 * EA=0 : read from internal rom
	 */
	mcs48->ea = (mcs48->prg_mask ? 0 : 1);

	switch (subtype) {
		case 8041:
			mcs48->opcode_table = s_upi41_opcodes;
			mcs48->feature_mask = UPI41_FEATURE;
			mcs48->ram_mask = 0x3f;
			mcs48->prg_mask = 0x3ff;
			break;
		case 8042:
			mcs48->opcode_table = s_upi41_opcodes;
			mcs48->feature_mask = UPI41_FEATURE;
			mcs48->ram_mask = 0x7f;
			mcs48->prg_mask = 0x7ff;
			break;
		case 8049:
		case 8749:
			mcs48->opcode_table = s_mcs48_opcodes;
			mcs48->feature_mask = I8048_FEATURE;
			mcs48->ram_mask = 0x7f;
			mcs48->prg_mask = 0x7ff;
			break;
		case 8884:
			mcs48->opcode_table = s_mcs48_opcodes;
			mcs48->feature_mask = I8048_FEATURE;
			mcs48->ram_mask = 0x3f;
			mcs48->prg_mask = 0xfff;
			break;
		default:
			bprintf(PRINT_ERROR, _T("mcs48Init(): Unsupported subtype!!\n"));
			break;
	}

	mcs48_set_read_port(mcs48_dummy_read_port);
	mcs48_set_write_port(mcs48_dummy_write_port);

	mcs48Close();
}


void mcs48Exit()
{
	mcs48 = NULL;
	mcs48_active_cpu = -1;
	mcs48_total_cpus = 0;
}

#if 0
mcs48_cpu_device::mcs48_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, int rom_size, int ram_size, UINT8 feature_mask, const mcs48_cpu_device::mcs48_ophandler *opcode_table)
	: cpu_device(mconfig, type, tag, owner, clock)
	, m_program_config("program", ENDIANNESS_LITTLE, 8, (feature_mask & MB_FEATURE) != 0 ? 12 : 11, 0
					   , (rom_size == 1024) ? address_map_constructor(FUNC(mcs48_cpu_device::program_10bit), this) : (rom_size == 2048) ? address_map_constructor(FUNC(mcs48_cpu_device::program_11bit), this) : (rom_size == 4096) ? address_map_constructor(FUNC(mcs48_cpu_device::program_12bit), this) : address_map_constructor())
	, m_data_config("data", ENDIANNESS_LITTLE, 8, ( ( ram_size == 64 ) ? 6 : ( ( ram_size == 128 ) ? 7 : 8 ) ), 0
					, (ram_size == 64) ? address_map_constructor(FUNC(mcs48_cpu_device::data_6bit), this) : (ram_size == 128) ? address_map_constructor(FUNC(mcs48_cpu_device::data_7bit), this) : address_map_constructor(FUNC(mcs48_cpu_device::data_8bit), this))
	, m_io_config("io", ENDIANNESS_LITTLE, 8, 8, 0)
	, m_port_in_cb(*this)
	, m_port_out_cb(*this)
	, m_bus_in_cb(*this)
	, m_bus_out_cb(*this)
	, m_test_in_cb(*this)
	, m_t0_clk_func(*this)
	, m_prog_out_cb(*this)
	, m_psw(0)
	, m_dataptr(*this, "data")
	, m_feature_mask(feature_mask)
	, m_int_rom_size(rom_size)
	, m_opcode_table(opcode_table)
{
	// Sanity checks
	if ( ram_size != 64 && ram_size != 128 && ram_size != 256 )
	{
		fatalerror("mcs48: Invalid RAM size\n");
	}

	if ( rom_size != 0 && rom_size != 1024 && rom_size != 2048 && rom_size != 4096 )
	{
		fatalerror("mcs48: Invalid ROM size\n");
	}
}

i8021_device::i8021_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, I8021, tag, owner, clock, 1024, 64, I802X_FEATURE, s_i8021_opcodes)
{
}

i8022_device::i8022_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, I8022, tag, owner, clock, 2048, 128, I802X_FEATURE, s_i8022_opcodes)
{
}

i8035_device::i8035_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, I8035, tag, owner, clock, 0, 64, I8048_FEATURE, s_mcs48_opcodes)
{
}

i8048_device::i8048_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, I8048, tag, owner, clock, 1024, 64, I8048_FEATURE, s_mcs48_opcodes)
{
}

i8648_device::i8648_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, I8648, tag, owner, clock, 1024, 64, I8048_FEATURE, s_mcs48_opcodes)
{
}

i8748_device::i8748_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, I8748, tag, owner, clock, 1024, 64, I8048_FEATURE, s_mcs48_opcodes)
{
}

i8039_device::i8039_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, I8039, tag, owner, clock, 0, 128, I8048_FEATURE, s_mcs48_opcodes)
{
}

i8049_device::i8049_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, I8049, tag, owner, clock, 2048, 128, I8048_FEATURE, s_mcs48_opcodes)
{
}

i8749_device::i8749_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, I8749, tag, owner, clock, 2048, 128, I8048_FEATURE, s_mcs48_opcodes)
{
}

i8040_device::i8040_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, I8040, tag, owner, clock, 0, 256, I8048_FEATURE, s_mcs48_opcodes)
{
}

i8050_device::i8050_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, I8050, tag, owner, clock, 4096, 256, I8048_FEATURE, s_mcs48_opcodes)
{
}

mb8884_device::mb8884_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, MB8884, tag, owner, clock, 0, 64, I8048_FEATURE, s_mcs48_opcodes)
{
}

n7751_device::n7751_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, N7751, tag, owner, clock, 1024, 64, I8048_FEATURE, s_mcs48_opcodes)
{
}

m58715_device::m58715_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: mcs48_cpu_device(mconfig, M58715, tag, owner, clock, 2048, 128, I8048_FEATURE, s_mcs48_opcodes)
{
}

upi41_cpu_device::upi41_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, int rom_size, int ram_size)
	: mcs48_cpu_device(mconfig, type, tag, owner, clock, rom_size, ram_size, UPI41_FEATURE, s_upi41_opcodes)
{
}

i8041a_device::i8041a_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: upi41_cpu_device(mconfig, I8041A, tag, owner, clock, 1024, 64)
{
}

i8741a_device::i8741a_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: upi41_cpu_device(mconfig, I8741A, tag, owner, clock, 1024, 64)
{
}

i8041ah_device::i8041ah_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: upi41_cpu_device(mconfig, I8041AH, tag, owner, clock, 1024, 128)
{
}

i8741ah_device::i8741ah_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: upi41_cpu_device(mconfig, I8741AH, tag, owner, clock, 1024, 128)
{
}

i8042_device::i8042_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: upi41_cpu_device(mconfig, I8042, tag, owner, clock, 2048, 128)
{
}

i8742_device::i8742_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: upi41_cpu_device(mconfig, I8742, tag, owner, clock, 2048, 128)
{
}

i8042ah_device::i8042ah_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: upi41_cpu_device(mconfig, I8042AH, tag, owner, clock, 2048, 256)
{
}

i8742ah_device::i8742ah_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: upi41_cpu_device(mconfig, I8742AH, tag, owner, clock, 2048, 256)
{
}

device_memory_interface::space_config_vector mcs48_cpu_device::memory_space_config() const
{
	if ((m_feature_mask & EXT_BUS_FEATURE) != 0)
		return space_config_vector {
			std::make_pair(AS_PROGRAM, &m_program_config),
			std::make_pair(AS_DATA,    &m_data_config),
			std::make_pair(AS_IO,      &m_io_config)
		};
	else
		return space_config_vector {
			std::make_pair(AS_PROGRAM, &m_program_config),
			std::make_pair(AS_DATA,    &m_data_config)
		};
}

std::unique_ptr<util::disasm_interface> mcs48_cpu_device::create_disassembler()
{
	return std::make_unique<mcs48_disassembler>((m_feature_mask & UPI41_FEATURE) != 0, (m_feature_mask & I802X_FEATURE) != 0);
}
#endif
/***************************************************************************
    INLINE FUNCTIONS
***************************************************************************/

/*-------------------------------------------------
    opcode_fetch - fetch an opcode byte
-------------------------------------------------*/

static UINT8 opcode_fetch()
{
	UINT16 address = mcs48->pc;
	mcs48->pc = ((mcs48->pc + 1) & 0x7ff) | (mcs48->pc & 0x800);
	return mcs48->prg[address & mcs48->prg_mask];
}


/*-------------------------------------------------
    argument_fetch - fetch an opcode argument
    byte
-------------------------------------------------*/

static UINT8 argument_fetch()
{
	UINT16 address = mcs48->pc;
	mcs48->pc = ((mcs48->pc + 1) & 0x7ff) | (mcs48->pc & 0x800);
	return mcs48->prg[address & mcs48->prg_mask];
}


/*-------------------------------------------------
    update_regptr - update the regptr member to
    point to the appropriate register bank
-------------------------------------------------*/

static void update_regptr()
{
	mcs48->regptr = &mcs48->ram[(mcs48->psw & B_FLAG) ? 24 : 0];
}


/*-------------------------------------------------
    push_pc_psw - push the m_pc and m_psw values onto
    the stack
-------------------------------------------------*/

static void push_pc_psw()
{
	UINT8 sp = mcs48->psw & 0x07;
	ram_w(8 + 2*sp, mcs48->pc);
	ram_w(9 + 2*sp, ((mcs48->pc >> 8) & 0x0f) | (mcs48->psw & 0xf0));
	mcs48->psw = (mcs48->psw & 0xf0) | ((sp + 1) & 0x07);
}


/*-------------------------------------------------
    pull_pc_psw - pull the PC and PSW values from
    the stack
-------------------------------------------------*/

static void pull_pc_psw()
{
	UINT8 sp = (mcs48->psw - 1) & 0x07;
	mcs48->pc = ram_r(8 + 2*sp);
	mcs48->pc |= ram_r(9 + 2*sp) << 8;
	mcs48->psw = ((mcs48->pc >> 8) & 0xf0) | sp;
	mcs48->pc &= (mcs48->irq_in_progress) ? 0x7ff : 0xfff;
	update_regptr();
}


/*-------------------------------------------------
    pull_pc - pull the PC value from the stack,
    leaving the upper part of PSW intact
-------------------------------------------------*/

static void pull_pc()
{
	UINT8 sp = (mcs48->psw - 1) & 0x07;
	mcs48->pc = ram_r(8 + 2*sp);
	mcs48->pc |= ram_r(9 + 2*sp) << 8;
	mcs48->pc &= (mcs48->irq_in_progress) ? 0x7ff : 0xfff;
	mcs48->psw = (mcs48->psw & 0xf0) | sp;
}


/*-------------------------------------------------
    execute_add - perform the logic of an ADD
    instruction
-------------------------------------------------*/

static void execute_add(UINT8 dat)
{
	UINT16 temp = mcs48->a + dat;
	UINT16 temp4 = (mcs48->a & 0x0f) + (dat & 0x0f);

	mcs48->psw &= ~(C_FLAG | A_FLAG);
	mcs48->psw |= (temp4 << 2) & A_FLAG;
	mcs48->psw |= (temp >> 1) & C_FLAG;
	mcs48->a = temp;
}


/*-------------------------------------------------
    execute_addc - perform the logic of an ADDC
    instruction
-------------------------------------------------*/

static void execute_addc(UINT8 dat)
{
	UINT8 carryin = (mcs48->psw & C_FLAG) >> 7;
	UINT16 temp = mcs48->a + dat + carryin;
	UINT16 temp4 = (mcs48->a & 0x0f) + (dat & 0x0f) + carryin;

	mcs48->psw &= ~(C_FLAG | A_FLAG);
	mcs48->psw |= (temp4 << 2) & A_FLAG;
	mcs48->psw |= (temp >> 1) & C_FLAG;
	mcs48->a = temp;
}


/*-------------------------------------------------
    execute_jmp - perform the logic of a JMP
    instruction
-------------------------------------------------*/

static void execute_jmp(UINT16 address)
{
	UINT16 a11 = (mcs48->irq_in_progress) ? 0 : mcs48->a11;
	mcs48->pc = address | a11;
}


/*-------------------------------------------------
    execute_call - perform the logic of a CALL
    instruction
-------------------------------------------------*/

static void execute_call(UINT16 address)
{
	push_pc_psw();
	execute_jmp(address);
}


/*-------------------------------------------------
    execute_jcc - perform the logic of a
    conditional jump instruction
-------------------------------------------------*/

static void execute_jcc(bool result)
{
	UINT16 pch = mcs48->pc & 0xf00;
	UINT8 offset = argument_fetch();
	if (result)
		mcs48->pc = pch | offset;
}


/*-------------------------------------------------
    p2_mask - return the mask of bits that the
    code can directly affect
-------------------------------------------------*/

static UINT8 p2_mask()
{
	UINT8 result = 0xff;
	if ((mcs48->feature_mask & UPI41_FEATURE) == 0)
		return result;
	if (mcs48->flags_enabled)
		result &= ~(P2_OBF | P2_NIBF);
	if (mcs48->dma_enabled)
		result &= ~(P2_DRQ | P2_NDACK);
	return result;
}


/*-------------------------------------------------
    expander_operation - perform an operation via
    the 8243 expander chip
-------------------------------------------------*/

static void expander_operation(expander_op operation, UINT8 port)
{
	// put opcode on low 4 bits of P2 (overwriting latch)
	port_w(2, mcs48->p2 = (mcs48->p2 & 0xf0) | (UINT8(operation) << 2) | (port & 3));

	// generate high-to-low transition on PROG line
	prog_w(0);

	// transfer data on low 4 bits of P2
	if (operation != EXPANDER_OP_READ)
		port_w(2, mcs48->p2 = (mcs48->p2 & 0xf0) | (mcs48->a & 0x0f));
	else
	{
		// place P20-P23 in input mode
		port_w(2, mcs48->p2 |= 0x0f);

		// input data to lower 4 bits of A (upper 4 bits are cleared)
		mcs48->a = port_r(2) & 0x0f;
	}

	// generate low-to-high transition on PROG line
	prog_w(1);
}

static void burn_cycles(int count); // forward



/***************************************************************************
    OPCODE HANDLERS
***************************************************************************/

#define OPHANDLER(_name) static void _name()


OPHANDLER( illegal )
{
	burn_cycles(1);
	//logerror("MCS-48 PC:%04X - Illegal opcode = %02x\n", mcs48->prevpc, program_r(m_prevpc));
}

OPHANDLER( add_a_r0 )       { burn_cycles(1); execute_add(R0); }
OPHANDLER( add_a_r1 )       { burn_cycles(1); execute_add(R1); }
OPHANDLER( add_a_r2 )       { burn_cycles(1); execute_add(R2); }
OPHANDLER( add_a_r3 )       { burn_cycles(1); execute_add(R3); }
OPHANDLER( add_a_r4 )       { burn_cycles(1); execute_add(R4); }
OPHANDLER( add_a_r5 )       { burn_cycles(1); execute_add(R5); }
OPHANDLER( add_a_r6 )       { burn_cycles(1); execute_add(R6); }
OPHANDLER( add_a_r7 )       { burn_cycles(1); execute_add(R7); }
OPHANDLER( add_a_xr0 )      { burn_cycles(1); execute_add(ram_r(R0)); }
OPHANDLER( add_a_xr1 )      { burn_cycles(1); execute_add(ram_r(R1)); }
OPHANDLER( add_a_n )        { burn_cycles(2); execute_add(argument_fetch()); }

OPHANDLER( adc_a_r0 )       { burn_cycles(1); execute_addc(R0); }
OPHANDLER( adc_a_r1 )       { burn_cycles(1); execute_addc(R1); }
OPHANDLER( adc_a_r2 )       { burn_cycles(1); execute_addc(R2); }
OPHANDLER( adc_a_r3 )       { burn_cycles(1); execute_addc(R3); }
OPHANDLER( adc_a_r4 )       { burn_cycles(1); execute_addc(R4); }
OPHANDLER( adc_a_r5 )       { burn_cycles(1); execute_addc(R5); }
OPHANDLER( adc_a_r6 )       { burn_cycles(1); execute_addc(R6); }
OPHANDLER( adc_a_r7 )       { burn_cycles(1); execute_addc(R7); }
OPHANDLER( adc_a_xr0 )      { burn_cycles(1); execute_addc(ram_r(R0)); }
OPHANDLER( adc_a_xr1 )      { burn_cycles(1); execute_addc(ram_r(R1)); }
OPHANDLER( adc_a_n )        { burn_cycles(2); execute_addc(argument_fetch()); }

OPHANDLER( anl_a_r0 )       { burn_cycles(1); mcs48->a &= R0; }
OPHANDLER( anl_a_r1 )       { burn_cycles(1); mcs48->a &= R1; }
OPHANDLER( anl_a_r2 )       { burn_cycles(1); mcs48->a &= R2; }
OPHANDLER( anl_a_r3 )       { burn_cycles(1); mcs48->a &= R3; }
OPHANDLER( anl_a_r4 )       { burn_cycles(1); mcs48->a &= R4; }
OPHANDLER( anl_a_r5 )       { burn_cycles(1); mcs48->a &= R5; }
OPHANDLER( anl_a_r6 )       { burn_cycles(1); mcs48->a &= R6; }
OPHANDLER( anl_a_r7 )       { burn_cycles(1); mcs48->a &= R7; }
OPHANDLER( anl_a_xr0 )      { burn_cycles(1); mcs48->a &= ram_r(R0); }
OPHANDLER( anl_a_xr1 )      { burn_cycles(1); mcs48->a &= ram_r(R1); }
OPHANDLER( anl_a_n )        { burn_cycles(2); mcs48->a &= argument_fetch(); }

OPHANDLER( anl_bus_n )      { burn_cycles(2); bus_w(bus_r() & argument_fetch()); }
OPHANDLER( anl_p1_n )       { burn_cycles(2); port_w(1, mcs48->p1 &= argument_fetch()); }
OPHANDLER( anl_p2_n )       { burn_cycles(2); port_w(2, mcs48->p2 &= argument_fetch() | ~p2_mask()); }
OPHANDLER( anld_p4_a )      { burn_cycles(2); expander_operation(EXPANDER_OP_AND, 4); }
OPHANDLER( anld_p5_a )      { burn_cycles(2); expander_operation(EXPANDER_OP_AND, 5); }
OPHANDLER( anld_p6_a )      { burn_cycles(2); expander_operation(EXPANDER_OP_AND, 6); }
OPHANDLER( anld_p7_a )      { burn_cycles(2); expander_operation(EXPANDER_OP_AND, 7); }

OPHANDLER( call_0 )         { burn_cycles(2); execute_call(argument_fetch() | 0x000); }
OPHANDLER( call_1 )         { burn_cycles(2); execute_call(argument_fetch() | 0x100); }
OPHANDLER( call_2 )         { burn_cycles(2); execute_call(argument_fetch() | 0x200); }
OPHANDLER( call_3 )         { burn_cycles(2); execute_call(argument_fetch() | 0x300); }
OPHANDLER( call_4 )         { burn_cycles(2); execute_call(argument_fetch() | 0x400); }
OPHANDLER( call_5 )         { burn_cycles(2); execute_call(argument_fetch() | 0x500); }
OPHANDLER( call_6 )         { burn_cycles(2); execute_call(argument_fetch() | 0x600); }
OPHANDLER( call_7 )         { burn_cycles(2); execute_call(argument_fetch() | 0x700); }

OPHANDLER( clr_a )          { burn_cycles(1); mcs48->a = 0; }
OPHANDLER( clr_c )          { burn_cycles(1); mcs48->psw &= ~C_FLAG; }
OPHANDLER( clr_f0 )         { burn_cycles(1); mcs48->psw &= ~F_FLAG; }
OPHANDLER( clr_f1 )         { burn_cycles(1); mcs48->f1 = false; }

OPHANDLER( cpl_a )          { burn_cycles(1); mcs48->a ^= 0xff; }
OPHANDLER( cpl_c )          { burn_cycles(1); mcs48->psw ^= C_FLAG; }
OPHANDLER( cpl_f0 )         { burn_cycles(1); mcs48->psw ^= F_FLAG; }
OPHANDLER( cpl_f1 )         { burn_cycles(1); mcs48->f1 = !mcs48->f1; }

OPHANDLER( da_a )
{
	burn_cycles(1);

	if ((mcs48->a & 0x0f) > 0x09 || (mcs48->psw & A_FLAG))
	{
		if (mcs48->a > 0xf9)
			mcs48->psw |= C_FLAG;
		mcs48->a += 0x06;
	}
	if ((mcs48->a & 0xf0) > 0x90 || (mcs48->psw & C_FLAG))
	{
		mcs48->a += 0x60;
		mcs48->psw |= C_FLAG;
	}
}

OPHANDLER( dec_a )          { burn_cycles(1); mcs48->a--; }
OPHANDLER( dec_r0 )         { burn_cycles(1); R0--; }
OPHANDLER( dec_r1 )         { burn_cycles(1); R1--; }
OPHANDLER( dec_r2 )         { burn_cycles(1); R2--; }
OPHANDLER( dec_r3 )         { burn_cycles(1); R3--; }
OPHANDLER( dec_r4 )         { burn_cycles(1); R4--; }
OPHANDLER( dec_r5 )         { burn_cycles(1); R5--; }
OPHANDLER( dec_r6 )         { burn_cycles(1); R6--; }
OPHANDLER( dec_r7 )         { burn_cycles(1); R7--; }

OPHANDLER( dis_i )          { burn_cycles(1); mcs48->xirq_enabled = false; }
OPHANDLER( dis_tcnti )      { burn_cycles(1); mcs48->tirq_enabled = false; mcs48->timer_overflow = false; }

OPHANDLER( djnz_r0 )        { burn_cycles(2); execute_jcc(--R0 != 0); }
OPHANDLER( djnz_r1 )        { burn_cycles(2); execute_jcc(--R1 != 0); }
OPHANDLER( djnz_r2 )        { burn_cycles(2); execute_jcc(--R2 != 0); }
OPHANDLER( djnz_r3 )        { burn_cycles(2); execute_jcc(--R3 != 0); }
OPHANDLER( djnz_r4 )        { burn_cycles(2); execute_jcc(--R4 != 0); }
OPHANDLER( djnz_r5 )        { burn_cycles(2); execute_jcc(--R5 != 0); }
OPHANDLER( djnz_r6 )        { burn_cycles(2); execute_jcc(--R6 != 0); }
OPHANDLER( djnz_r7 )        { burn_cycles(2); execute_jcc(--R7 != 0); }

OPHANDLER( en_i )           { burn_cycles(1); mcs48->xirq_enabled = true; }
OPHANDLER( en_tcnti )       { burn_cycles(1); mcs48->tirq_enabled = true; }
OPHANDLER( en_dma )         { burn_cycles(1); mcs48->dma_enabled = true; port_w(2, mcs48->p2); }
OPHANDLER( en_flags )       { burn_cycles(1); mcs48->flags_enabled = true; port_w(2, mcs48->p2); }
OPHANDLER( ent0_clk )
{
	burn_cycles(1);

#if 0
	if (!mcs48->t0_clk_func.isnull())
		mcs48->t0_clk_func(clock() / 3);
	else
		logerror("T0 clock enabled\n");
#endif
	bprintf(0, _T("T0 clock callback unimplimented in mcs48.cpp.\n"));
}

OPHANDLER( in_a_p0 )        { burn_cycles(2); mcs48->a = bus_r() & mcs48->dbbo; }
OPHANDLER( in_a_p1 )        { burn_cycles(2); mcs48->a = port_r(1) & mcs48->p1; }
OPHANDLER( in_a_p2 )        { burn_cycles(2); mcs48->a = port_r(2) & mcs48->p2; }
OPHANDLER( ins_a_bus )      { burn_cycles(2); mcs48->a = bus_r(); }
OPHANDLER( in_a_dbb )
{
	burn_cycles(2);

	/* acknowledge the IBF IRQ and clear the bit in STS */
//	if ((mcs48->sts & STS_IBF) != 0)
//		standard_irq_callback(UPI41_INPUT_IBF);
	mcs48->sts &= ~STS_IBF;

	/* if P2 flags are enabled, update the state of P2 */
	if (mcs48->flags_enabled && (mcs48->p2 & P2_NIBF) == 0)
		port_w(2, mcs48->p2 |= P2_NIBF);
	mcs48->a = mcs48->dbbi;
}

OPHANDLER( inc_a )          { burn_cycles(1); mcs48->a++; }
OPHANDLER( inc_r0 )         { burn_cycles(1); R0++; }
OPHANDLER( inc_r1 )         { burn_cycles(1); R1++; }
OPHANDLER( inc_r2 )         { burn_cycles(1); R2++; }
OPHANDLER( inc_r3 )         { burn_cycles(1); R3++; }
OPHANDLER( inc_r4 )         { burn_cycles(1); R4++; }
OPHANDLER( inc_r5 )         { burn_cycles(1); R5++; }
OPHANDLER( inc_r6 )         { burn_cycles(1); R6++; }
OPHANDLER( inc_r7 )         { burn_cycles(1); R7++; }
OPHANDLER( inc_xr0 )        { burn_cycles(1); ram_w(R0, ram_r(R0) + 1); }
OPHANDLER( inc_xr1 )        { burn_cycles(1); ram_w(R1, ram_r(R1) + 1); }

OPHANDLER( jb_0 )           { burn_cycles(2); execute_jcc((mcs48->a & 0x01) != 0); }
OPHANDLER( jb_1 )           { burn_cycles(2); execute_jcc((mcs48->a & 0x02) != 0); }
OPHANDLER( jb_2 )           { burn_cycles(2); execute_jcc((mcs48->a & 0x04) != 0); }
OPHANDLER( jb_3 )           { burn_cycles(2); execute_jcc((mcs48->a & 0x08) != 0); }
OPHANDLER( jb_4 )           { burn_cycles(2); execute_jcc((mcs48->a & 0x10) != 0); }
OPHANDLER( jb_5 )           { burn_cycles(2); execute_jcc((mcs48->a & 0x20) != 0); }
OPHANDLER( jb_6 )           { burn_cycles(2); execute_jcc((mcs48->a & 0x40) != 0); }
OPHANDLER( jb_7 )           { burn_cycles(2); execute_jcc((mcs48->a & 0x80) != 0); }
OPHANDLER( jc )             { burn_cycles(2); execute_jcc((mcs48->psw & C_FLAG) != 0); }
OPHANDLER( jf0 )            { burn_cycles(2); execute_jcc((mcs48->psw & F_FLAG) != 0); }
OPHANDLER( jf1 )            { burn_cycles(2); execute_jcc(mcs48->f1); }
OPHANDLER( jnc )            { burn_cycles(2); execute_jcc((mcs48->psw & C_FLAG) == 0); }
OPHANDLER( jni )            { burn_cycles(2); mcs48->irq_polled = (mcs48->irq_state == 0); execute_jcc(mcs48->irq_state != 0); }
OPHANDLER( jnibf )          { burn_cycles(2); mcs48->irq_polled = (mcs48->sts & STS_IBF) != 0; execute_jcc((mcs48->sts & STS_IBF) == 0); }
OPHANDLER( jnt_0 )          { burn_cycles(2); execute_jcc(test_r(0) == 0); }
OPHANDLER( jnt_1 )          { burn_cycles(2); execute_jcc(test_r(1) == 0); }
OPHANDLER( jnz )            { burn_cycles(2); execute_jcc(mcs48->a != 0); }
OPHANDLER( jobf )           { burn_cycles(2); execute_jcc((mcs48->sts & STS_OBF) != 0); }
OPHANDLER( jtf )            { burn_cycles(2); execute_jcc(mcs48->timer_flag); mcs48->timer_flag = false; }
OPHANDLER( jt_0 )           { burn_cycles(2); execute_jcc(test_r(0) != 0); }
OPHANDLER( jt_1 )           { burn_cycles(2); execute_jcc(test_r(1) != 0); }
OPHANDLER( jz )             { burn_cycles(2); execute_jcc(mcs48->a == 0); }

OPHANDLER( jmp_0 )          { burn_cycles(2); execute_jmp(argument_fetch() | 0x000); }
OPHANDLER( jmp_1 )          { burn_cycles(2); execute_jmp(argument_fetch() | 0x100); }
OPHANDLER( jmp_2 )          { burn_cycles(2); execute_jmp(argument_fetch() | 0x200); }
OPHANDLER( jmp_3 )          { burn_cycles(2); execute_jmp(argument_fetch() | 0x300); }
OPHANDLER( jmp_4 )          { burn_cycles(2); execute_jmp(argument_fetch() | 0x400); }
OPHANDLER( jmp_5 )          { burn_cycles(2); execute_jmp(argument_fetch() | 0x500); }
OPHANDLER( jmp_6 )          { burn_cycles(2); execute_jmp(argument_fetch() | 0x600); }
OPHANDLER( jmp_7 )          { burn_cycles(2); execute_jmp(argument_fetch() | 0x700); }
OPHANDLER( jmpp_xa )        { burn_cycles(2); mcs48->pc &= 0xf00; mcs48->pc |= program_r(mcs48->pc | mcs48->a); }

OPHANDLER( mov_a_n )        { burn_cycles(2); mcs48->a = argument_fetch(); }
OPHANDLER( mov_a_psw )      { burn_cycles(1); mcs48->a = mcs48->psw | 0x08; }
OPHANDLER( mov_a_r0 )       { burn_cycles(1); mcs48->a = R0; }
OPHANDLER( mov_a_r1 )       { burn_cycles(1); mcs48->a = R1; }
OPHANDLER( mov_a_r2 )       { burn_cycles(1); mcs48->a = R2; }
OPHANDLER( mov_a_r3 )       { burn_cycles(1); mcs48->a = R3; }
OPHANDLER( mov_a_r4 )       { burn_cycles(1); mcs48->a = R4; }
OPHANDLER( mov_a_r5 )       { burn_cycles(1); mcs48->a = R5; }
OPHANDLER( mov_a_r6 )       { burn_cycles(1); mcs48->a = R6; }
OPHANDLER( mov_a_r7 )       { burn_cycles(1); mcs48->a = R7; }
OPHANDLER( mov_a_xr0 )      { burn_cycles(1); mcs48->a = ram_r(R0); }
OPHANDLER( mov_a_xr1 )      { burn_cycles(1); mcs48->a = ram_r(R1); }
OPHANDLER( mov_a_t )        { burn_cycles(1); mcs48->a = mcs48->timer; }

OPHANDLER( mov_psw_a )      { burn_cycles(1); mcs48->psw = mcs48->a & ~0x08; update_regptr(); }
OPHANDLER( mov_sts_a )      { burn_cycles(1); mcs48->sts = (mcs48->sts & 0x0f) | (mcs48->a & 0xf0); }
OPHANDLER( mov_r0_a )       { burn_cycles(1); R0 = mcs48->a; }
OPHANDLER( mov_r1_a )       { burn_cycles(1); R1 = mcs48->a; }
OPHANDLER( mov_r2_a )       { burn_cycles(1); R2 = mcs48->a; }
OPHANDLER( mov_r3_a )       { burn_cycles(1); R3 = mcs48->a; }
OPHANDLER( mov_r4_a )       { burn_cycles(1); R4 = mcs48->a; }
OPHANDLER( mov_r5_a )       { burn_cycles(1); R5 = mcs48->a; }
OPHANDLER( mov_r6_a )       { burn_cycles(1); R6 = mcs48->a; }
OPHANDLER( mov_r7_a )       { burn_cycles(1); R7 = mcs48->a; }
OPHANDLER( mov_r0_n )       { burn_cycles(2); R0 = argument_fetch(); }
OPHANDLER( mov_r1_n )       { burn_cycles(2); R1 = argument_fetch(); }
OPHANDLER( mov_r2_n )       { burn_cycles(2); R2 = argument_fetch(); }
OPHANDLER( mov_r3_n )       { burn_cycles(2); R3 = argument_fetch(); }
OPHANDLER( mov_r4_n )       { burn_cycles(2); R4 = argument_fetch(); }
OPHANDLER( mov_r5_n )       { burn_cycles(2); R5 = argument_fetch(); }
OPHANDLER( mov_r6_n )       { burn_cycles(2); R6 = argument_fetch(); }
OPHANDLER( mov_r7_n )       { burn_cycles(2); R7 = argument_fetch(); }
OPHANDLER( mov_t_a )        { burn_cycles(1); mcs48->timer = mcs48->a; }
OPHANDLER( mov_xr0_a )      { burn_cycles(1); ram_w(R0, mcs48->a); }
OPHANDLER( mov_xr1_a )      { burn_cycles(1); ram_w(R1, mcs48->a); }
OPHANDLER( mov_xr0_n )      { burn_cycles(2); ram_w(R0, argument_fetch()); }
OPHANDLER( mov_xr1_n )      { burn_cycles(2); ram_w(R1, argument_fetch()); }

OPHANDLER( movd_a_p4 )      { burn_cycles(2); expander_operation(EXPANDER_OP_READ, 4); }
OPHANDLER( movd_a_p5 )      { burn_cycles(2); expander_operation(EXPANDER_OP_READ, 5); }
OPHANDLER( movd_a_p6 )      { burn_cycles(2); expander_operation(EXPANDER_OP_READ, 6); }
OPHANDLER( movd_a_p7 )      { burn_cycles(2); expander_operation(EXPANDER_OP_READ, 7); }
OPHANDLER( movd_p4_a )      { burn_cycles(2); expander_operation(EXPANDER_OP_WRITE, 4); }
OPHANDLER( movd_p5_a )      { burn_cycles(2); expander_operation(EXPANDER_OP_WRITE, 5); }
OPHANDLER( movd_p6_a )      { burn_cycles(2); expander_operation(EXPANDER_OP_WRITE, 6); }
OPHANDLER( movd_p7_a )      { burn_cycles(2); expander_operation(EXPANDER_OP_WRITE, 7); }

OPHANDLER( movp_a_xa )      { burn_cycles(2); mcs48->a = program_r((mcs48->pc & 0xf00) | mcs48->a); }
OPHANDLER( movp3_a_xa )     { burn_cycles(2); mcs48->a = program_r(0x300 | mcs48->a); }

OPHANDLER( movx_a_xr0 )     { burn_cycles(2); mcs48->a = ext_r(R0); }
OPHANDLER( movx_a_xr1 )     { burn_cycles(2); mcs48->a = ext_r(R1); }
OPHANDLER( movx_xr0_a )     { burn_cycles(2); ext_w(R0, mcs48->a); }
OPHANDLER( movx_xr1_a )     { burn_cycles(2); ext_w(R1, mcs48->a); }

OPHANDLER( nop )            { burn_cycles(1); }

OPHANDLER( orl_a_r0 )       { burn_cycles(1); mcs48->a |= R0; }
OPHANDLER( orl_a_r1 )       { burn_cycles(1); mcs48->a |= R1; }
OPHANDLER( orl_a_r2 )       { burn_cycles(1); mcs48->a |= R2; }
OPHANDLER( orl_a_r3 )       { burn_cycles(1); mcs48->a |= R3; }
OPHANDLER( orl_a_r4 )       { burn_cycles(1); mcs48->a |= R4; }
OPHANDLER( orl_a_r5 )       { burn_cycles(1); mcs48->a |= R5; }
OPHANDLER( orl_a_r6 )       { burn_cycles(1); mcs48->a |= R6; }
OPHANDLER( orl_a_r7 )       { burn_cycles(1); mcs48->a |= R7; }
OPHANDLER( orl_a_xr0 )      { burn_cycles(1); mcs48->a |= ram_r(R0); }
OPHANDLER( orl_a_xr1 )      { burn_cycles(1); mcs48->a |= ram_r(R1); }
OPHANDLER( orl_a_n )        { burn_cycles(2); mcs48->a |= argument_fetch(); }

OPHANDLER( orl_bus_n )      { burn_cycles(2); bus_w(bus_r() | argument_fetch()); }
OPHANDLER( orl_p1_n )       { burn_cycles(2); port_w(1, mcs48->p1 |= argument_fetch()); }
OPHANDLER( orl_p2_n )       { burn_cycles(2); port_w(2, mcs48->p2 |= argument_fetch() & p2_mask()); }
OPHANDLER( orld_p4_a )      { burn_cycles(2); expander_operation(EXPANDER_OP_OR, 4); }
OPHANDLER( orld_p5_a )      { burn_cycles(2); expander_operation(EXPANDER_OP_OR, 5); }
OPHANDLER( orld_p6_a )      { burn_cycles(2); expander_operation(EXPANDER_OP_OR, 6); }
OPHANDLER( orld_p7_a )      { burn_cycles(2); expander_operation(EXPANDER_OP_OR, 7); }

OPHANDLER( outl_bus_a )     { burn_cycles(2); bus_w(mcs48->a); }
OPHANDLER( outl_p0_a )      { burn_cycles(2); bus_w(mcs48->dbbo = mcs48->a); }
OPHANDLER( outl_p1_a )      { burn_cycles(2); port_w(1, mcs48->p1 = mcs48->a); }
OPHANDLER( outl_p2_a )      { burn_cycles(2); UINT8 mask = p2_mask(); port_w(2, mcs48->p2 = (mcs48->p2 & ~mask) | (mcs48->a & mask)); }
OPHANDLER( out_dbb_a )
{
	burn_cycles(2);

	/* copy to the DBBO and update the bit in STS */
	mcs48->dbbo = mcs48->a;
	mcs48->sts |= STS_OBF;

	/* if P2 flags are enabled, update the state of P2 */
	if (mcs48->flags_enabled && (mcs48->p2 & P2_OBF) == 0)
		port_w(2, mcs48->p2 |= P2_OBF);
}


OPHANDLER( ret )            { burn_cycles(2); pull_pc(); }
OPHANDLER( retr )
{
	burn_cycles(2);

	/* implicitly clear the IRQ in progress flip flop */
	mcs48->irq_in_progress = false;
	pull_pc_psw();
}

OPHANDLER( rl_a )           { burn_cycles(1); mcs48->a = (mcs48->a << 1) | (mcs48->a >> 7); }
OPHANDLER( rlc_a )          { burn_cycles(1); UINT8 newc = mcs48->a & C_FLAG; mcs48->a = (mcs48->a << 1) | (mcs48->psw >> 7); mcs48->psw = (mcs48->psw & ~C_FLAG) | newc; }

OPHANDLER( rr_a )           { burn_cycles(1); mcs48->a = (mcs48->a >> 1) | (mcs48->a << 7); }
OPHANDLER( rrc_a )          { burn_cycles(1); UINT8 newc = (mcs48->a << 7) & C_FLAG; mcs48->a = (mcs48->a >> 1) | (mcs48->psw & C_FLAG); mcs48->psw = (mcs48->psw & ~C_FLAG) | newc; }

OPHANDLER( sel_mb0 )        { burn_cycles(1); mcs48->a11 = 0x000; }
OPHANDLER( sel_mb1 )        { burn_cycles(1); mcs48->a11 = 0x800; }

OPHANDLER( sel_rb0 )        { burn_cycles(1); mcs48->psw &= ~B_FLAG; update_regptr(); }
OPHANDLER( sel_rb1 )        { burn_cycles(1); mcs48->psw |=  B_FLAG; update_regptr(); }

OPHANDLER( stop_tcnt )      { burn_cycles(1); mcs48->timecount_enabled = 0; }
OPHANDLER( strt_t )         { burn_cycles(1); mcs48->timecount_enabled = TIMER_ENABLED; mcs48->prescaler = 0; }
OPHANDLER( strt_cnt )
{
	burn_cycles(1);
	if (!(mcs48->timecount_enabled & COUNTER_ENABLED))
		mcs48->t1_history = test_r(1);

	mcs48->timecount_enabled = COUNTER_ENABLED;
}

OPHANDLER( swap_a )         { burn_cycles(1); mcs48->a = (mcs48->a << 4) | (mcs48->a >> 4); }

OPHANDLER( xch_a_r0 )       { burn_cycles(1); UINT8 tmp = mcs48->a; mcs48->a = R0; R0 = tmp; }
OPHANDLER( xch_a_r1 )       { burn_cycles(1); UINT8 tmp = mcs48->a; mcs48->a = R1; R1 = tmp; }
OPHANDLER( xch_a_r2 )       { burn_cycles(1); UINT8 tmp = mcs48->a; mcs48->a = R2; R2 = tmp; }
OPHANDLER( xch_a_r3 )       { burn_cycles(1); UINT8 tmp = mcs48->a; mcs48->a = R3; R3 = tmp; }
OPHANDLER( xch_a_r4 )       { burn_cycles(1); UINT8 tmp = mcs48->a; mcs48->a = R4; R4 = tmp; }
OPHANDLER( xch_a_r5 )       { burn_cycles(1); UINT8 tmp = mcs48->a; mcs48->a = R5; R5 = tmp; }
OPHANDLER( xch_a_r6 )       { burn_cycles(1); UINT8 tmp = mcs48->a; mcs48->a = R6; R6 = tmp; }
OPHANDLER( xch_a_r7 )       { burn_cycles(1); UINT8 tmp = mcs48->a; mcs48->a = R7; R7 = tmp; }
OPHANDLER( xch_a_xr0 )      { burn_cycles(1); UINT8 tmp = mcs48->a; mcs48->a = ram_r(R0); ram_w(R0, tmp); }
OPHANDLER( xch_a_xr1 )      { burn_cycles(1); UINT8 tmp = mcs48->a; mcs48->a = ram_r(R1); ram_w(R1, tmp); }

OPHANDLER( xchd_a_xr0 )     { burn_cycles(1); UINT8 oldram = ram_r(R0); ram_w(R0, (oldram & 0xf0) | (mcs48->a & 0x0f)); mcs48->a = (mcs48->a & 0xf0) | (oldram & 0x0f); }
OPHANDLER( xchd_a_xr1 )     { burn_cycles(1); UINT8 oldram = ram_r(R1); ram_w(R1, (oldram & 0xf0) | (mcs48->a & 0x0f)); mcs48->a = (mcs48->a & 0xf0) | (oldram & 0x0f); }

OPHANDLER( xrl_a_r0 )       { burn_cycles(1); mcs48->a ^= R0; }
OPHANDLER( xrl_a_r1 )       { burn_cycles(1); mcs48->a ^= R1; }
OPHANDLER( xrl_a_r2 )       { burn_cycles(1); mcs48->a ^= R2; }
OPHANDLER( xrl_a_r3 )       { burn_cycles(1); mcs48->a ^= R3; }
OPHANDLER( xrl_a_r4 )       { burn_cycles(1); mcs48->a ^= R4; }
OPHANDLER( xrl_a_r5 )       { burn_cycles(1); mcs48->a ^= R5; }
OPHANDLER( xrl_a_r6 )       { burn_cycles(1); mcs48->a ^= R6; }
OPHANDLER( xrl_a_r7 )       { burn_cycles(1); mcs48->a ^= R7; }
OPHANDLER( xrl_a_xr0 )      { burn_cycles(1); mcs48->a ^= ram_r(R0); }
OPHANDLER( xrl_a_xr1 )      { burn_cycles(1); mcs48->a ^= ram_r(R1); }
OPHANDLER( xrl_a_n )        { burn_cycles(2); mcs48->a ^= argument_fetch(); }



/***************************************************************************
    OPCODE TABLES
***************************************************************************/

#define OP(_a) &_a

const mcs48_ophandler s_mcs48_opcodes[256] =
{
	OP(nop),        OP(illegal),    OP(outl_bus_a),OP(add_a_n),   OP(jmp_0),     OP(en_i),       OP(illegal),   OP(dec_a),         /* 00 */
	OP(ins_a_bus),  OP(in_a_p1),    OP(in_a_p2),   OP(illegal),   OP(movd_a_p4), OP(movd_a_p5),  OP(movd_a_p6), OP(movd_a_p7),
	OP(inc_xr0),    OP(inc_xr1),    OP(jb_0),      OP(adc_a_n),   OP(call_0),    OP(dis_i),      OP(jtf),       OP(inc_a),         /* 10 */
	OP(inc_r0),     OP(inc_r1),     OP(inc_r2),    OP(inc_r3),    OP(inc_r4),    OP(inc_r5),     OP(inc_r6),    OP(inc_r7),
	OP(xch_a_xr0),  OP(xch_a_xr1),  OP(illegal),   OP(mov_a_n),   OP(jmp_1),     OP(en_tcnti),   OP(jnt_0),     OP(clr_a),         /* 20 */
	OP(xch_a_r0),   OP(xch_a_r1),   OP(xch_a_r2),  OP(xch_a_r3),  OP(xch_a_r4),  OP(xch_a_r5),   OP(xch_a_r6),  OP(xch_a_r7),
	OP(xchd_a_xr0), OP(xchd_a_xr1), OP(jb_1),      OP(illegal),   OP(call_1),    OP(dis_tcnti),  OP(jt_0),      OP(cpl_a),         /* 30 */
	OP(illegal),    OP(outl_p1_a),  OP(outl_p2_a), OP(illegal),   OP(movd_p4_a), OP(movd_p5_a),  OP(movd_p6_a), OP(movd_p7_a),
	OP(orl_a_xr0),  OP(orl_a_xr1),  OP(mov_a_t),   OP(orl_a_n),   OP(jmp_2),     OP(strt_cnt),   OP(jnt_1),     OP(swap_a),        /* 40 */
	OP(orl_a_r0),   OP(orl_a_r1),   OP(orl_a_r2),  OP(orl_a_r3),  OP(orl_a_r4),  OP(orl_a_r5),   OP(orl_a_r6),  OP(orl_a_r7),
	OP(anl_a_xr0),  OP(anl_a_xr1),  OP(jb_2),      OP(anl_a_n),   OP(call_2),    OP(strt_t),     OP(jt_1),      OP(da_a),          /* 50 */
	OP(anl_a_r0),   OP(anl_a_r1),   OP(anl_a_r2),  OP(anl_a_r3),  OP(anl_a_r4),  OP(anl_a_r5),   OP(anl_a_r6),  OP(anl_a_r7),
	OP(add_a_xr0),  OP(add_a_xr1),  OP(mov_t_a),   OP(illegal),   OP(jmp_3),     OP(stop_tcnt),  OP(illegal),   OP(rrc_a),         /* 60 */
	OP(add_a_r0),   OP(add_a_r1),   OP(add_a_r2),  OP(add_a_r3),  OP(add_a_r4),  OP(add_a_r5),   OP(add_a_r6),  OP(add_a_r7),
	OP(adc_a_xr0),  OP(adc_a_xr1),  OP(jb_3),      OP(illegal),   OP(call_3),    OP(ent0_clk),   OP(jf1),       OP(rr_a),          /* 70 */
	OP(adc_a_r0),   OP(adc_a_r1),   OP(adc_a_r2),  OP(adc_a_r3),  OP(adc_a_r4),  OP(adc_a_r5),   OP(adc_a_r6),  OP(adc_a_r7),
	OP(movx_a_xr0), OP(movx_a_xr1), OP(illegal),   OP(ret),       OP(jmp_4),     OP(clr_f0),     OP(jni),       OP(illegal),       /* 80 */
	OP(orl_bus_n),  OP(orl_p1_n),   OP(orl_p2_n),  OP(illegal),   OP(orld_p4_a), OP(orld_p5_a),  OP(orld_p6_a), OP(orld_p7_a),
	OP(movx_xr0_a), OP(movx_xr1_a), OP(jb_4),      OP(retr),      OP(call_4),    OP(cpl_f0),     OP(jnz),       OP(clr_c),         /* 90 */
	OP(anl_bus_n),  OP(anl_p1_n),   OP(anl_p2_n),  OP(illegal),   OP(anld_p4_a), OP(anld_p5_a),  OP(anld_p6_a), OP(anld_p7_a),
	OP(mov_xr0_a),  OP(mov_xr1_a),  OP(illegal),   OP(movp_a_xa), OP(jmp_5),     OP(clr_f1),     OP(illegal),   OP(cpl_c),         /* A0 */
	OP(mov_r0_a),   OP(mov_r1_a),   OP(mov_r2_a),  OP(mov_r3_a),  OP(mov_r4_a),  OP(mov_r5_a),   OP(mov_r6_a),  OP(mov_r7_a),
	OP(mov_xr0_n),  OP(mov_xr1_n),  OP(jb_5),      OP(jmpp_xa),   OP(call_5),    OP(cpl_f1),     OP(jf0),       OP(illegal),       /* B0 */
	OP(mov_r0_n),   OP(mov_r1_n),   OP(mov_r2_n),  OP(mov_r3_n),  OP(mov_r4_n),  OP(mov_r5_n),   OP(mov_r6_n),  OP(mov_r7_n),
	OP(illegal),    OP(illegal),    OP(illegal),   OP(illegal),   OP(jmp_6),     OP(sel_rb0),    OP(jz),        OP(mov_a_psw),     /* C0 */
	OP(dec_r0),     OP(dec_r1),     OP(dec_r2),    OP(dec_r3),    OP(dec_r4),    OP(dec_r5),     OP(dec_r6),    OP(dec_r7),
	OP(xrl_a_xr0),  OP(xrl_a_xr1),  OP(jb_6),      OP(xrl_a_n),   OP(call_6),    OP(sel_rb1),    OP(illegal),   OP(mov_psw_a),     /* D0 */
	OP(xrl_a_r0),   OP(xrl_a_r1),   OP(xrl_a_r2),  OP(xrl_a_r3),  OP(xrl_a_r4),  OP(xrl_a_r5),   OP(xrl_a_r6),  OP(xrl_a_r7),
	OP(illegal),    OP(illegal),    OP(illegal),   OP(movp3_a_xa),OP(jmp_7),     OP(sel_mb0),    OP(jnc),       OP(rl_a),          /* E0 */
	OP(djnz_r0),    OP(djnz_r1),    OP(djnz_r2),   OP(djnz_r3),   OP(djnz_r4),   OP(djnz_r5),    OP(djnz_r6),   OP(djnz_r7),
	OP(mov_a_xr0),  OP(mov_a_xr1),  OP(jb_7),      OP(illegal),   OP(call_7),    OP(sel_mb1),    OP(jc),        OP(rlc_a),         /* F0 */
	OP(mov_a_r0),   OP(mov_a_r1),   OP(mov_a_r2),  OP(mov_a_r3),  OP(mov_a_r4),  OP(mov_a_r5),   OP(mov_a_r6),  OP(mov_a_r7)
};

const mcs48_ophandler s_upi41_opcodes[256] =
{
	OP(nop),        OP(illegal),    OP(out_dbb_a), OP(add_a_n),   OP(jmp_0),     OP(en_i),       OP(illegal),   OP(dec_a),         /* 00 */
	OP(illegal),    OP(in_a_p1),    OP(in_a_p2),   OP(illegal),   OP(movd_a_p4), OP(movd_a_p5),  OP(movd_a_p6), OP(movd_a_p7),
	OP(inc_xr0),    OP(inc_xr1),    OP(jb_0),      OP(adc_a_n),   OP(call_0),    OP(dis_i),      OP(jtf),       OP(inc_a),         /* 10 */
	OP(inc_r0),     OP(inc_r1),     OP(inc_r2),    OP(inc_r3),    OP(inc_r4),    OP(inc_r5),     OP(inc_r6),    OP(inc_r7),
	OP(xch_a_xr0),  OP(xch_a_xr1),  OP(in_a_dbb),  OP(mov_a_n),   OP(jmp_1),     OP(en_tcnti),   OP(jnt_0),     OP(clr_a),         /* 20 */
	OP(xch_a_r0),   OP(xch_a_r1),   OP(xch_a_r2),  OP(xch_a_r3),  OP(xch_a_r4),  OP(xch_a_r5),   OP(xch_a_r6),  OP(xch_a_r7),
	OP(xchd_a_xr0), OP(xchd_a_xr1), OP(jb_1),      OP(illegal),   OP(call_1),    OP(dis_tcnti),  OP(jt_0),      OP(cpl_a),         /* 30 */
	OP(illegal),    OP(outl_p1_a),  OP(outl_p2_a), OP(illegal),   OP(movd_p4_a), OP(movd_p5_a),  OP(movd_p6_a), OP(movd_p7_a),
	OP(orl_a_xr0),  OP(orl_a_xr1),  OP(mov_a_t),   OP(orl_a_n),   OP(jmp_2),     OP(strt_cnt),   OP(jnt_1),     OP(swap_a),        /* 40 */
	OP(orl_a_r0),   OP(orl_a_r1),   OP(orl_a_r2),  OP(orl_a_r3),  OP(orl_a_r4),  OP(orl_a_r5),   OP(orl_a_r6),  OP(orl_a_r7),
	OP(anl_a_xr0),  OP(anl_a_xr1),  OP(jb_2),      OP(anl_a_n),   OP(call_2),    OP(strt_t),     OP(jt_1),      OP(da_a),          /* 50 */
	OP(anl_a_r0),   OP(anl_a_r1),   OP(anl_a_r2),  OP(anl_a_r3),  OP(anl_a_r4),  OP(anl_a_r5),   OP(anl_a_r6),  OP(anl_a_r7),
	OP(add_a_xr0),  OP(add_a_xr1),  OP(mov_t_a),   OP(illegal),   OP(jmp_3),     OP(stop_tcnt),  OP(illegal),   OP(rrc_a),         /* 60 */
	OP(add_a_r0),   OP(add_a_r1),   OP(add_a_r2),  OP(add_a_r3),  OP(add_a_r4),  OP(add_a_r5),   OP(add_a_r6),  OP(add_a_r7),
	OP(adc_a_xr0),  OP(adc_a_xr1),  OP(jb_3),      OP(illegal),   OP(call_3),    OP(illegal),    OP(jf1),       OP(rr_a),          /* 70 */
	OP(adc_a_r0),   OP(adc_a_r1),   OP(adc_a_r2),  OP(adc_a_r3),  OP(adc_a_r4),  OP(adc_a_r5),   OP(adc_a_r6),  OP(adc_a_r7),
	OP(illegal),    OP(illegal),    OP(illegal),   OP(ret),       OP(jmp_4),     OP(clr_f0),     OP(jobf),      OP(illegal),       /* 80 */
	OP(illegal),    OP(orl_p1_n),   OP(orl_p2_n),  OP(illegal),   OP(orld_p4_a), OP(orld_p5_a),  OP(orld_p6_a), OP(orld_p7_a),
	OP(mov_sts_a),  OP(illegal),    OP(jb_4),      OP(retr),      OP(call_4),    OP(cpl_f0),     OP(jnz),       OP(clr_c),         /* 90 */
	OP(illegal),    OP(anl_p1_n),   OP(anl_p2_n),  OP(illegal),   OP(anld_p4_a), OP(anld_p5_a),  OP(anld_p6_a), OP(anld_p7_a),
	OP(mov_xr0_a),  OP(mov_xr1_a),  OP(illegal),   OP(movp_a_xa), OP(jmp_5),     OP(clr_f1),     OP(illegal),   OP(cpl_c),         /* A0 */
	OP(mov_r0_a),   OP(mov_r1_a),   OP(mov_r2_a),  OP(mov_r3_a),  OP(mov_r4_a),  OP(mov_r5_a),   OP(mov_r6_a),  OP(mov_r7_a),
	OP(mov_xr0_n),  OP(mov_xr1_n),  OP(jb_5),      OP(jmpp_xa),   OP(call_5),    OP(cpl_f1),     OP(jf0),       OP(illegal),       /* B0 */
	OP(mov_r0_n),   OP(mov_r1_n),   OP(mov_r2_n),  OP(mov_r3_n),  OP(mov_r4_n),  OP(mov_r5_n),   OP(mov_r6_n),  OP(mov_r7_n),
	OP(illegal),    OP(illegal),    OP(illegal),   OP(illegal),   OP(jmp_6),     OP(sel_rb0),    OP(jz),        OP(mov_a_psw),     /* C0 */
	OP(dec_r0),     OP(dec_r1),     OP(dec_r2),    OP(dec_r3),    OP(dec_r4),    OP(dec_r5),     OP(dec_r6),    OP(dec_r7),
	OP(xrl_a_xr0),  OP(xrl_a_xr1),  OP(jb_6),      OP(xrl_a_n),   OP(call_6),    OP(sel_rb1),    OP(jnibf),     OP(mov_psw_a),     /* D0 */
	OP(xrl_a_r0),   OP(xrl_a_r1),   OP(xrl_a_r2),  OP(xrl_a_r3),  OP(xrl_a_r4),  OP(xrl_a_r5),   OP(xrl_a_r6),  OP(xrl_a_r7),
	OP(illegal),    OP(illegal),    OP(illegal),   OP(movp3_a_xa),OP(jmp_7),     OP(en_dma),     OP(jnc),       OP(rl_a),          /* E0 */
	OP(djnz_r0),    OP(djnz_r1),    OP(djnz_r2),   OP(djnz_r3),   OP(djnz_r4),   OP(djnz_r5),    OP(djnz_r6),   OP(djnz_r7),
	OP(mov_a_xr0),  OP(mov_a_xr1),  OP(jb_7),      OP(illegal),   OP(call_7),    OP(en_flags),   OP(jc),        OP(rlc_a),         /* F0 */
	OP(mov_a_r0),   OP(mov_a_r1),   OP(mov_a_r2),  OP(mov_a_r3),  OP(mov_a_r4),  OP(mov_a_r5),   OP(mov_a_r6),  OP(mov_a_r7)
};

const mcs48_ophandler s_i8021_opcodes[256] =
{
	OP(nop),        OP(illegal),    OP(illegal),   OP(add_a_n),   OP(jmp_0),     OP(illegal),    OP(illegal),   OP(dec_a),         /* 00 */
	OP(in_a_p0),    OP(in_a_p1),    OP(in_a_p2),   OP(illegal),   OP(movd_a_p4), OP(movd_a_p5),  OP(movd_a_p6), OP(movd_a_p7),
	OP(inc_xr0),    OP(inc_xr1),    OP(illegal),   OP(adc_a_n),   OP(call_0),    OP(illegal),    OP(jtf),       OP(inc_a),         /* 10 */
	OP(inc_r0),     OP(inc_r1),     OP(inc_r2),    OP(inc_r3),    OP(inc_r4),    OP(inc_r5),     OP(inc_r6),    OP(inc_r7),
	OP(xch_a_xr0),  OP(xch_a_xr1),  OP(illegal),   OP(mov_a_n),   OP(jmp_1),     OP(illegal),    OP(illegal),   OP(clr_a),         /* 20 */
	OP(xch_a_r0),   OP(xch_a_r1),   OP(xch_a_r2),  OP(xch_a_r3),  OP(xch_a_r4),  OP(xch_a_r5),   OP(xch_a_r6),  OP(xch_a_r7),
	OP(xchd_a_xr0), OP(xchd_a_xr1), OP(illegal),   OP(illegal),   OP(call_1),    OP(illegal),    OP(illegal),   OP(cpl_a),         /* 30 */
	OP(illegal),    OP(outl_p1_a),  OP(outl_p2_a), OP(illegal),   OP(movd_p4_a), OP(movd_p5_a),  OP(movd_p6_a), OP(movd_p7_a),
	OP(orl_a_xr0),  OP(orl_a_xr1),  OP(mov_a_t),   OP(orl_a_n),   OP(jmp_2),     OP(strt_cnt),   OP(jnt_1),     OP(swap_a),        /* 40 */
	OP(orl_a_r0),   OP(orl_a_r1),   OP(orl_a_r2),  OP(orl_a_r3),  OP(orl_a_r4),  OP(orl_a_r5),   OP(orl_a_r6),  OP(orl_a_r7),
	OP(anl_a_xr0),  OP(anl_a_xr1),  OP(illegal),   OP(anl_a_n),   OP(call_2),    OP(strt_t),     OP(jt_1),      OP(da_a),          /* 50 */
	OP(anl_a_r0),   OP(anl_a_r1),   OP(anl_a_r2),  OP(anl_a_r3),  OP(anl_a_r4),  OP(anl_a_r5),   OP(anl_a_r6),  OP(anl_a_r7),
	OP(add_a_xr0),  OP(add_a_xr1),  OP(mov_t_a),   OP(illegal),   OP(jmp_3),     OP(stop_tcnt),  OP(illegal),   OP(rrc_a),         /* 60 */
	OP(add_a_r0),   OP(add_a_r1),   OP(add_a_r2),  OP(add_a_r3),  OP(add_a_r4),  OP(add_a_r5),   OP(add_a_r6),  OP(add_a_r7),
	OP(adc_a_xr0),  OP(adc_a_xr1),  OP(illegal),   OP(illegal),   OP(call_3),    OP(illegal),    OP(illegal),   OP(rr_a),          /* 70 */
	OP(adc_a_r0),   OP(adc_a_r1),   OP(adc_a_r2),  OP(adc_a_r3),  OP(adc_a_r4),  OP(adc_a_r5),   OP(adc_a_r6),  OP(adc_a_r7),
	OP(illegal),    OP(illegal),    OP(illegal),   OP(ret),       OP(jmp_4),     OP(illegal),    OP(illegal),   OP(illegal),       /* 80 */
	OP(illegal),    OP(illegal),    OP(illegal),   OP(illegal),   OP(orld_p4_a), OP(orld_p5_a),  OP(orld_p6_a), OP(orld_p7_a),
	OP(outl_p0_a),  OP(illegal),    OP(illegal),   OP(illegal),   OP(call_4),    OP(illegal),    OP(jnz),       OP(clr_c),         /* 90 */
	OP(illegal),    OP(illegal),    OP(illegal),   OP(illegal),   OP(anld_p4_a), OP(anld_p5_a),  OP(anld_p6_a), OP(anld_p7_a),
	OP(mov_xr0_a),  OP(mov_xr1_a),  OP(illegal),   OP(movp_a_xa), OP(jmp_5),     OP(illegal),    OP(illegal),   OP(cpl_c),         /* A0 */
	OP(mov_r0_a),   OP(mov_r1_a),   OP(mov_r2_a),  OP(mov_r3_a),  OP(mov_r4_a),  OP(mov_r5_a),   OP(mov_r6_a),  OP(mov_r7_a),
	OP(mov_xr0_n),  OP(mov_xr1_n),  OP(illegal),   OP(jmpp_xa),   OP(call_5),    OP(illegal),    OP(illegal),   OP(illegal),       /* B0 */
	OP(mov_r0_n),   OP(mov_r1_n),   OP(mov_r2_n),  OP(mov_r3_n),  OP(mov_r4_n),  OP(mov_r5_n),   OP(mov_r6_n),  OP(mov_r7_n),
	OP(illegal),    OP(illegal),    OP(illegal),   OP(illegal),   OP(jmp_6),     OP(illegal),    OP(jz),        OP(illegal),       /* C0 */
	OP(illegal),    OP(illegal),    OP(illegal),   OP(illegal),   OP(illegal),   OP(illegal),    OP(illegal),   OP(illegal),
	OP(xrl_a_xr0),  OP(xrl_a_xr1),  OP(illegal),   OP(xrl_a_n),   OP(call_6),    OP(illegal),    OP(illegal),   OP(illegal),       /* D0 */
	OP(xrl_a_r0),   OP(xrl_a_r1),   OP(xrl_a_r2),  OP(xrl_a_r3),  OP(xrl_a_r4),  OP(xrl_a_r5),   OP(xrl_a_r6),  OP(xrl_a_r7),
	OP(illegal),    OP(illegal),    OP(illegal),   OP(illegal),   OP(jmp_7),     OP(illegal),    OP(jnc),       OP(rl_a),          /* E0 */
	OP(djnz_r0),    OP(djnz_r1),    OP(djnz_r2),   OP(djnz_r3),   OP(djnz_r4),   OP(djnz_r5),    OP(djnz_r6),   OP(djnz_r7),
	OP(mov_a_xr0),  OP(mov_a_xr1),  OP(illegal),   OP(illegal),   OP(call_7),    OP(illegal),    OP(jc),        OP(rlc_a),         /* F0 */
	OP(mov_a_r0),   OP(mov_a_r1),   OP(mov_a_r2),  OP(mov_a_r3),  OP(mov_a_r4),  OP(mov_a_r5),   OP(mov_a_r6),  OP(mov_a_r7)
};

const mcs48_ophandler s_i8022_opcodes[256] =
{
	OP(nop),        OP(illegal),    OP(illegal),   OP(add_a_n),   OP(jmp_0),     OP(en_i),       OP(illegal),   OP(dec_a),         /* 00 */
	OP(in_a_p0),    OP(in_a_p1),    OP(in_a_p2),   OP(illegal),   OP(movd_a_p4), OP(movd_a_p5),  OP(movd_a_p6), OP(movd_a_p7),
	OP(inc_xr0),    OP(inc_xr1),    OP(illegal),   OP(adc_a_n),   OP(call_0),    OP(dis_i),      OP(jtf),       OP(inc_a),         /* 10 */
	OP(inc_r0),     OP(inc_r1),     OP(inc_r2),    OP(inc_r3),    OP(inc_r4),    OP(inc_r5),     OP(inc_r6),    OP(inc_r7),
	OP(xch_a_xr0),  OP(xch_a_xr1),  OP(illegal),   OP(mov_a_n),   OP(jmp_1),     OP(en_tcnti),   OP(jnt_0),     OP(clr_a),         /* 20 */
	OP(xch_a_r0),   OP(xch_a_r1),   OP(xch_a_r2),  OP(xch_a_r3),  OP(xch_a_r4),  OP(xch_a_r5),   OP(xch_a_r6),  OP(xch_a_r7),
	OP(xchd_a_xr0), OP(xchd_a_xr1), OP(illegal),   OP(illegal),   OP(call_1),    OP(dis_tcnti),  OP(jt_0),      OP(cpl_a),         /* 30 */
	OP(illegal),    OP(outl_p1_a),  OP(outl_p2_a), OP(illegal),   OP(movd_p4_a), OP(movd_p5_a),  OP(movd_p6_a), OP(movd_p7_a),
	OP(orl_a_xr0),  OP(orl_a_xr1),  OP(mov_a_t),   OP(orl_a_n),   OP(jmp_2),     OP(strt_cnt),   OP(jnt_1),     OP(swap_a),        /* 40 */
	OP(orl_a_r0),   OP(orl_a_r1),   OP(orl_a_r2),  OP(orl_a_r3),  OP(orl_a_r4),  OP(orl_a_r5),   OP(orl_a_r6),  OP(orl_a_r7),
	OP(anl_a_xr0),  OP(anl_a_xr1),  OP(illegal),   OP(anl_a_n),   OP(call_2),    OP(strt_t),     OP(jt_1),      OP(da_a),          /* 50 */
	OP(anl_a_r0),   OP(anl_a_r1),   OP(anl_a_r2),  OP(anl_a_r3),  OP(anl_a_r4),  OP(anl_a_r5),   OP(anl_a_r6),  OP(anl_a_r7),
	OP(add_a_xr0),  OP(add_a_xr1),  OP(mov_t_a),   OP(illegal),   OP(jmp_3),     OP(stop_tcnt),  OP(illegal),   OP(rrc_a),         /* 60 */
	OP(add_a_r0),   OP(add_a_r1),   OP(add_a_r2),  OP(add_a_r3),  OP(add_a_r4),  OP(add_a_r5),   OP(add_a_r6),  OP(add_a_r7),
	OP(adc_a_xr0),  OP(adc_a_xr1),  OP(illegal),   OP(illegal),   OP(call_3),    OP(illegal),    OP(illegal),   OP(rr_a),          /* 70 */
	OP(adc_a_r0),   OP(adc_a_r1),   OP(adc_a_r2),  OP(adc_a_r3),  OP(adc_a_r4),  OP(adc_a_r5),   OP(adc_a_r6),  OP(adc_a_r7),
	OP(illegal),    OP(illegal),    OP(illegal),   OP(ret),       OP(jmp_4),     OP(illegal),    OP(illegal),   OP(illegal),       /* 80 */
	OP(illegal),    OP(illegal),    OP(illegal),   OP(illegal),   OP(orld_p4_a), OP(orld_p5_a),  OP(orld_p6_a), OP(orld_p7_a),
	OP(outl_p0_a),  OP(illegal),    OP(illegal),   OP(retr),      OP(call_4),    OP(illegal),    OP(jnz),       OP(clr_c),         /* 90 */
	OP(illegal),    OP(illegal),    OP(illegal),   OP(illegal),   OP(anld_p4_a), OP(anld_p5_a),  OP(anld_p6_a), OP(anld_p7_a),
	OP(mov_xr0_a),  OP(mov_xr1_a),  OP(illegal),   OP(movp_a_xa), OP(jmp_5),     OP(illegal),    OP(illegal),   OP(cpl_c),         /* A0 */
	OP(mov_r0_a),   OP(mov_r1_a),   OP(mov_r2_a),  OP(mov_r3_a),  OP(mov_r4_a),  OP(mov_r5_a),   OP(mov_r6_a),  OP(mov_r7_a),
	OP(mov_xr0_n),  OP(mov_xr1_n),  OP(illegal),   OP(jmpp_xa),   OP(call_5),    OP(illegal),    OP(illegal),   OP(illegal),       /* B0 */
	OP(mov_r0_n),   OP(mov_r1_n),   OP(mov_r2_n),  OP(mov_r3_n),  OP(mov_r4_n),  OP(mov_r5_n),   OP(mov_r6_n),  OP(mov_r7_n),
	OP(illegal),    OP(illegal),    OP(illegal),   OP(illegal),   OP(jmp_6),     OP(illegal),    OP(jz),        OP(illegal),       /* C0 */
	OP(illegal),    OP(illegal),    OP(illegal),   OP(illegal),   OP(illegal),   OP(illegal),    OP(illegal),   OP(illegal),
	OP(xrl_a_xr0),  OP(xrl_a_xr1),  OP(illegal),   OP(xrl_a_n),   OP(call_6),    OP(illegal),    OP(illegal),   OP(illegal),       /* D0 */
	OP(xrl_a_r0),   OP(xrl_a_r1),   OP(xrl_a_r2),  OP(xrl_a_r3),  OP(xrl_a_r4),  OP(xrl_a_r5),   OP(xrl_a_r6),  OP(xrl_a_r7),
	OP(illegal),    OP(illegal),    OP(illegal),   OP(illegal),   OP(jmp_7),     OP(illegal),    OP(jnc),       OP(rl_a),          /* E0 */
	OP(djnz_r0),    OP(djnz_r1),    OP(djnz_r2),   OP(djnz_r3),   OP(djnz_r4),   OP(djnz_r5),    OP(djnz_r6),   OP(djnz_r7),
	OP(mov_a_xr0),  OP(mov_a_xr1),  OP(illegal),   OP(illegal),   OP(call_7),    OP(illegal),    OP(jc),        OP(rlc_a),         /* F0 */
	OP(mov_a_r0),   OP(mov_a_r1),   OP(mov_a_r2),  OP(mov_a_r3),  OP(mov_a_r4),  OP(mov_a_r5),   OP(mov_a_r6),  OP(mov_a_r7)
};



/***************************************************************************
    INITIALIZATION/RESET
***************************************************************************/

void mcs48Reset()
{
	/* confirmed from reset description */
	mcs48->pc = 0;
	mcs48->psw = mcs48->psw & (C_FLAG | A_FLAG);
	update_regptr();
	mcs48->f1 = 0;
	mcs48->a11 = 0;
	mcs48->dbbo = 0xff;
	mcs48->dbbi = 0xff;
	bus_w(0xff);
	mcs48->p1 = 0xff;
	mcs48->p2 = 0xff;
	port_w(1, mcs48->p1);
	port_w(2, mcs48->p2);
	mcs48->tirq_enabled = false;
	mcs48->xirq_enabled = false;
	mcs48->timecount_enabled = 0;
	mcs48->timer_flag = false;
	mcs48->sts = 0;
	mcs48->flags_enabled = false;
	mcs48->dma_enabled = false;
	//if (!mcs48->t0_clk_func.isnull())
	//	mcs48->t0_clk_func(0);

	/* confirmed from interrupt logic description */
	mcs48->irq_in_progress = false;
	mcs48->timer_overflow = false;

	mcs48->irq_polled = false;
}


/***************************************************************************
    EXECUTION
***************************************************************************/

/*-------------------------------------------------
    check_irqs - check for and process IRQs
-------------------------------------------------*/

static void check_irqs()
{
	/* if something is in progress, we do nothing */
	if (mcs48->irq_in_progress)
		return;

	/* external interrupts take priority */
	else if ((mcs48->irq_state || (mcs48->sts & STS_IBF) != 0) && mcs48->xirq_enabled)
	{
		burn_cycles(2);
		mcs48->irq_in_progress = true;

		// force JNI to be taken (hack)
		if (mcs48->irq_polled)
		{
			mcs48->pc = ((mcs48->prevpc + 1) & 0x7ff) | (mcs48->prevpc & 0x800);
			execute_jcc(true);
		}

		/* transfer to location 0x03 */
		execute_call(0x03);

		/* indicate we took the external IRQ */
		//standard_irq_callback(0);
	}

	/* timer overflow interrupts follow */
	else if (mcs48->timer_overflow && mcs48->tirq_enabled)
	{
		burn_cycles(2);
		mcs48->irq_in_progress = true;

		/* transfer to location 0x07 */
		execute_call(0x07);

		/* timer overflow flip-flop is reset once taken */
		mcs48->timer_overflow = false;
	}
}


/*-------------------------------------------------
    burn_cycles - burn cycles, processing timers
    and counters
-------------------------------------------------*/

static void burn_cycles(int count)
{
	if (mcs48->timecount_enabled)
	{
		bool timerover = false;

		/* if the timer is enabled, accumulate prescaler cycles */
		if (mcs48->timecount_enabled & TIMER_ENABLED)
		{
			UINT8 oldtimer = mcs48->timer;
			mcs48->prescaler += count;
			mcs48->timer += mcs48->prescaler >> 5;
			mcs48->prescaler &= 0x1f;
			timerover = (oldtimer != 0 && mcs48->timer == 0);
		}

		/* if the counter is enabled, poll the T1 test input once for each cycle */
		else if (mcs48->timecount_enabled & COUNTER_ENABLED)
			for ( ; count > 0; count--, mcs48->icount--)
			{
				mcs48->t1_history = (mcs48->t1_history << 1) | (test_r(1) & 1);
				if ((mcs48->t1_history & 3) == 2)
				{
					if (++mcs48->timer == 0)
						timerover = true;
				}
			}

		/* if either source caused a timer overflow, set the flags and check IRQs */
		if (timerover)
		{
			mcs48->timer_flag = true;

			/* according to the docs, if an overflow occurs with interrupts disabled, the overflow is not stored */
			if (mcs48->tirq_enabled)
				mcs48->timer_overflow = true;
		}
	}

	/* if timer counter was disabled, adjust icount here (otherwise count is 0) */
	mcs48->icount -= count;
}

/*-------------------------------------------------
    mcs48_execute - execute until we run out
    of cycles
-------------------------------------------------*/

INT32 mcs48Run(INT32 cycles)
{
	mcs48->cycle_start = cycles;
	mcs48->icount = cycles;
	mcs48->end_run = 0;

	update_regptr();

	// iterate over remaining cycles, guaranteeing at least one instruction
	do
	{
		// check interrupts
		check_irqs();
		mcs48->irq_polled = false;

		mcs48->prevpc = mcs48->pc;
		//debugger_instruction_hook(mcs48->pc);

		// fetch and process opcode
		unsigned opcode = opcode_fetch();
		(*mcs48->opcode_table[opcode])();

	} while (mcs48->icount > 0 && !mcs48->end_run );

	cycles = cycles - mcs48->icount;
	mcs48->icount = mcs48->cycle_start = 0;

	mcs48->total_cycles += cycles;

	return cycles;
}

void mcs48RunEnd()
{
	mcs48->end_run = 1;
}

INT32 mcs48TotalCycles()
{
	return mcs48->total_cycles + (mcs48->cycle_start - mcs48->icount);
}

void mcs48NewFrame()
{
	for (INT32 i = 0; i < MAX_MCS48; i++) {
		MCS48_t *ptr = &mcs48_state_store[i];
		ptr->total_cycles = 0;
	}
}

INT32 mcs48Idle(INT32 cycles)
{
	mcs48->total_cycles += cycles;

	return cycles;
}


/***************************************************************************
    DATA ACCESS HELPERS
***************************************************************************/

/*-------------------------------------------------
    upi41_master_r - master CPU data/status
    read
-------------------------------------------------*/

UINT8 mcs48_master_r(INT32 offset)
{
	/* if just reading the status, return it */
	if ((offset & 1) != 0)
		return (mcs48->sts & 0xf3) | (mcs48->f1 ? 8 : 0) | ((mcs48->psw & F_FLAG) ? 4 : 0);

	/* if the output buffer was full, it gets cleared now */
	if (mcs48->sts & STS_OBF)
	{
		mcs48->sts &= ~STS_OBF;
		if (mcs48->flags_enabled)
			port_w(2, mcs48->p2 &= ~P2_OBF);
	}
	return mcs48->dbbo;
}


/*-------------------------------------------------
    upi41_master_w - master CPU command/data
    write
-------------------------------------------------*/

void mcs48_master_w(INT32 offset, UINT8 data)
{
	UINT8 a0 = offset & 1;

	/* data always goes to the input buffer */
	mcs48->dbbi = data;
	if (cflyball_hack)
		mcs48->dbbo = data; // hack needed for cflyball to fully boot -dink

	/* set the appropriate flags */
	if ((mcs48->sts & STS_IBF) == 0)
	{
		mcs48->sts |= STS_IBF;
		if (mcs48->flags_enabled)
			port_w(2, mcs48->p2 &= ~P2_NIBF);
	}

	/* set F1 accordingly */
	mcs48->f1 = bool(a0);
}

void mcs48SetIRQLine(INT32 inputnum, INT32 state)
{
	switch( inputnum )
	{
		case MCS48_INPUT_IRQ:
			mcs48->irq_state = (state != CLEAR_LINE);
			break;

		case MCS48_INPUT_EA:
			mcs48->ea = (state != CLEAR_LINE);
			break;
	}
}
