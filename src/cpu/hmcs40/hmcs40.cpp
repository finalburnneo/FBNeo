// license:BSD-3-Clause
// copyright-holders:hap
/*

Hitachi HMCS40 MCU family cores

References:
- 1985 #AP1 Hitachi 4-bit Single-Chip Microcomputer Data Book
- 1988 HMCS400 Series Handbook (note: *400 is a newer MCU series, with similarities)
- opcode decoding by Tatsuyuki Satoh, Olivier Galibert, Kevin Horton, Lord Nightmare
  (verified a while later after new documentation was found)

TODO:
- Which opcodes block interrupt on next cycle? LPU is obvious, and Gakken Crazy Kong
  (VFD tabletop game) locks up if CAL doesn't do it. Maybe BR? But that's a
  dangerous assumption since tight infinite loops wouldn't work right anymore.

*/

// FBNeo Note: ported from MAME .284, Jan 2026 (dink)
// Currently supports 1 instance of: HD44801
// ... but: Support for others can be easily added
// ... same for multi-cpu, if needed just ask!

#include "burnint.h"
#include "hmcs40.h"
#include "bitswap.h"

#define IS_PMOS 0
#define IS_CMOS 0xffff

// settings
static int m_pcwidth;				// program counter bit-width
static int m_prgwidth;
static int m_datawidth;
static int m_family;				// MCU family (42-47)
static UINT16 m_polarity;			// i/o polarity (pmos vs cmos)
static int m_stack_levels;			// number of callstack levels
static int m_pcmask;
static int m_prgmask;
static int m_datamask;

static UINT16 *m_rom;
static UINT8 *m_ram;

// data
static INT32 m_icount;
static INT32 total_cycles;
static INT32 cycle_start;
static INT32 end_run;

static UINT16 m_stack[4];			// max 4
static UINT16 m_op;					// current opcode
static UINT16 m_prev_op;
static UINT8 m_i;					// 4-bit immediate opcode param
static int m_eint_line;				// which input_line caused an interrupt
static int m_halt;					// internal HLT state
static UINT8 m_prescaler;			// internal timer prescaler
static bool m_block_int;			// block interrupt on next cycle

static UINT16 m_pc;					// program counter
static UINT16 m_prev_pc;
static UINT8 m_pc_upper;			// LPU prepared upper bits of PC
static UINT8 m_a;					// 4-bit accumulator
static UINT8 m_b;					// 4-bit B register
static UINT8 m_x;					// 1/3/4-bit X register
static UINT8 m_spx;					// 1/3/4-bit SPX register
static UINT8 m_y;					// 4-bit Y register
static UINT8 m_spy;					// 4-bit SPY register
static UINT8 m_s;					// status F/F (F/F = flip-flop)
static UINT8 m_c;					// carry F/F

static UINT8 m_tc;					// timer/counter
static UINT8 m_cf;					// CF F/F (timer mode or counter mode)
static UINT8 m_ie;					// I/E (interrupt enable) F/F
static UINT8 m_iri;					// external interrupt pending I/RI F/F
static UINT8 m_irt;					// timer interrupt pending I/RT F/F
static UINT8 m_if[2];				// external interrupt mask IF0,1 F/F
static UINT8 m_tf;					// timer interrupt mask TF F/F
static UINT8 m_int[2];				// INT0/1 pins state
static UINT8 m_r[8];				// R outputs state
static UINT16 m_d;					// D pins state

static UINT8 dummy_read8(INT32 address) { return m_polarity & 0xf; }
static void dummy_write8(INT32 address, UINT8 data) { }
static UINT16 dummy_read16(INT32 address) { return m_polarity; }
static void dummy_write16(INT32 address, UINT16 data) { }

static UINT8 (*m_read_r[8])(INT32 address);
static void (*m_write_r[8])(INT32 address, UINT8 data);
static UINT16 (*m_read_d)(INT32 address);
static void (*m_write_d)(INT32 address, UINT16 data);

static UINT8 (*m_data_read)(INT32 address);
static void (*m_data_write)(INT32 address, UINT8 data);

#if 0
// I/O handlers
devcb_read8::array<8> m_read_r;
devcb_write8::array<8> m_write_r;
devcb_read16 m_read_d;
devcb_write16 m_write_d;
#endif

#if 0
//-------------------------------------------------
//  device types
//-------------------------------------------------

// HMCS42/C/CL, 28 pins, 22 I/O lines, (512+32)x10 ROM, 32x4 RAM, no B or SPY register
//DEFINE_DEVICE_TYPE(HD38702, hd38702_device, "hd38702", "Hitachi HD38702") // PMOS
//DEFINE_DEVICE_TYPE(HD44700, hd44700_device, "hd44700", "Hitachi HD44700") // CMOS
//DEFINE_DEVICE_TYPE(HD44708, hd44708_device, "hd44708", "Hitachi HD44708") // CMOS, low-power

// HMCS43/C/CL, 42 pins, 32 I/O lines, (1024+64)x10 ROM, 80x4 RAM
DEFINE_DEVICE_TYPE(HD38750, hd38750_device, "hd38750", "Hitachi HD38750") // PMOS
DEFINE_DEVICE_TYPE(HD38755, hd38755_device, "hd38755", "Hitachi HD38755") // ceramic filter oscillator type
DEFINE_DEVICE_TYPE(HD44750, hd44750_device, "hd44750", "Hitachi HD44750") // CMOS
DEFINE_DEVICE_TYPE(HD44758, hd44758_device, "hd44758", "Hitachi HD44758") // CMOS, low-power

// HMCS44A/C/CL, 42 pins, 32 I/O lines, (2048+128)x10 ROM, 160x4 RAM
DEFINE_DEVICE_TYPE(HD38800, hd38800_device, "hd38800", "Hitachi HD38800") // PMOS
DEFINE_DEVICE_TYPE(HD38805, hd38805_device, "hd38805", "Hitachi HD38805") // ceramic filter oscillator type
DEFINE_DEVICE_TYPE(HD44801, hd44801_device, "hd44801", "Hitachi HD44801") // CMOS
DEFINE_DEVICE_TYPE(HD44808, hd44808_device, "hd44808", "Hitachi HD44808") // CMOS, low-power

// HMCS45A/C/CL, 54 pins(QFP) or 64 pins(DIP), 44 I/O lines, (2048+128)x10 ROM, 160x4 RAM
DEFINE_DEVICE_TYPE(HD38820, hd38820_device, "hd38820", "Hitachi HD38820") // PMOS
DEFINE_DEVICE_TYPE(HD38825, hd38825_device, "hd38825", "Hitachi HD38825") // ceramic filter oscillator type
DEFINE_DEVICE_TYPE(HD44820, hd44820_device, "hd44820", "Hitachi HD44820") // CMOS
DEFINE_DEVICE_TYPE(HD44828, hd44828_device, "hd44828", "Hitachi HD44828") // CMOS, low-power

// HMCS46C/CL, 42 pins, 32 I/O lines, 4096x10 ROM, 256x4 RAM (no PMOS version exists)
DEFINE_DEVICE_TYPE(HD44840, hd44840_device, "hd44840", "Hitachi HD44840") // CMOS
DEFINE_DEVICE_TYPE(HD44848, hd44848_device, "hd44848", "Hitachi HD44848") // CMOS, low-power

// HMCS47A/C/CL, 54 pins(QFP) or 64 pins(DIP), 44 I/O lines, 4096x10 ROM, 256x4 RAM
DEFINE_DEVICE_TYPE(HD38870, hd38870_device, "hd38870", "Hitachi HD38870") // PMOS
DEFINE_DEVICE_TYPE(HD44860, hd44860_device, "hd44860", "Hitachi HD44860") // CMOS
DEFINE_DEVICE_TYPE(HD44868, hd44868_device, "hd44868", "Hitachi HD44868") // CMOS, low-power

// LCD-III, 64 pins, HMCS44C core, LCDC with 4 commons and 32 segments
//DEFINE_DEVICE_TYPE(HD44790, hd44790_device, "hd44790", "Hitachi HD44790") // CMOS
//DEFINE_DEVICE_TYPE(HD44795, hd44795_device, "hd44795", "Hitachi HD44795") // CMOS, low-power

// LCD-IV, 64 pins, HMCS46C core, LCDC with 4 commons and 32 segments
//DEFINE_DEVICE_TYPE(HD613901, hd613901_device, "hd613901", "Hitachi HD613901") // CMOS


//-------------------------------------------------
//  constructor
//-------------------------------------------------

hmcs40_cpu_device::hmcs40_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, UINT32 clock, int family, UINT16 polarity, int stack_levels, int pcwidth, int prgwidth, address_map_constructor program, int datawidth, address_map_constructor data) :
	cpu_device(mconfig, type, tag, owner, clock),
	m_program_config("program", ENDIANNESS_LITTLE, 16, prgwidth, -1, program),
	m_data_config("data", ENDIANNESS_LITTLE, 8, datawidth, 0, data),
	m_pcwidth(pcwidth),
	m_prgwidth(prgwidth),
	m_datawidth(datawidth),
	m_family(family),
	m_polarity(polarity),
	m_stack_levels(stack_levels),
	m_read_r(*this, polarity & 0xf), // dinknote: this sets the "default unset handler" return value!
	m_write_r(*this),
	m_read_d(*this, polarity), // this too!
	m_write_d(*this)
{ }

hmcs40_cpu_device::~hmcs40_cpu_device() { }


hmcs43_cpu_device::hmcs43_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, UINT32 clock, UINT16 polarity) :
	hmcs40_cpu_device(mconfig, type, tag, owner, clock, HMCS43_FAMILY, polarity, 3 /* stack levels */, 10 /* pc width */, 11 /* prg width */, address_map_constructor(FUNC(hmcs43_cpu_device::program_1k), this), 7 /* data width */, address_map_constructor(FUNC(hmcs43_cpu_device::data_80x4), this))
{ }

hd38750_device::hd38750_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs43_cpu_device(mconfig, HD38750, tag, owner, clock, IS_PMOS)
{ }
hd38755_device::hd38755_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs43_cpu_device(mconfig, HD38755, tag, owner, clock, IS_PMOS)
{ }
hd44750_device::hd44750_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs43_cpu_device(mconfig, HD44750, tag, owner, clock, IS_CMOS)
{ }
hd44758_device::hd44758_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs43_cpu_device(mconfig, HD44758, tag, owner, clock, IS_CMOS)
{ }


hmcs44_cpu_device::hmcs44_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, UINT32 clock, UINT16 polarity) :
	hmcs40_cpu_device(mconfig, type, tag, owner, clock, HMCS44_FAMILY, polarity, 4, 11, 12, address_map_constructor(FUNC(hmcs44_cpu_device::program_2k), this), 8, address_map_constructor(FUNC(hmcs44_cpu_device::data_160x4), this))
{ }

hd38800_device::hd38800_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs44_cpu_device(mconfig, HD38800, tag, owner, clock, IS_PMOS)
{ }
hd38805_device::hd38805_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs44_cpu_device(mconfig, HD38805, tag, owner, clock, IS_PMOS)
{ }
hd44801_device::hd44801_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs44_cpu_device(mconfig, HD44801, tag, owner, clock, IS_CMOS)
{ }
hd44808_device::hd44808_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs44_cpu_device(mconfig, HD44808, tag, owner, clock, IS_CMOS)
{ }


hmcs45_cpu_device::hmcs45_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, UINT32 clock, UINT16 polarity) :
	hmcs40_cpu_device(mconfig, type, tag, owner, clock, HMCS45_FAMILY, polarity, 4, 11, 12, address_map_constructor(FUNC(hmcs45_cpu_device::program_2k), this), 8, address_map_constructor(FUNC(hmcs45_cpu_device::data_160x4), this))
{ }

hd38820_device::hd38820_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs45_cpu_device(mconfig, HD38820, tag, owner, clock, IS_PMOS)
{ }
hd38825_device::hd38825_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs45_cpu_device(mconfig, HD38825, tag, owner, clock, IS_PMOS)
{ }
hd44820_device::hd44820_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs45_cpu_device(mconfig, HD44820, tag, owner, clock, IS_CMOS)
{ }
hd44828_device::hd44828_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs45_cpu_device(mconfig, HD44828, tag, owner, clock, IS_CMOS)
{ }


hmcs46_cpu_device::hmcs46_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, UINT32 clock, UINT16 polarity) :
	hmcs40_cpu_device(mconfig, type, tag, owner, clock, HMCS46_FAMILY, polarity, 4, 12, 12, address_map_constructor(FUNC(hmcs46_cpu_device::program_2k), this), 8, address_map_constructor(FUNC(hmcs46_cpu_device::data_256x4), this))
{ }

hd44840_device::hd44840_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs46_cpu_device(mconfig, HD44840, tag, owner, clock, IS_CMOS)
{ }
hd44848_device::hd44848_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs46_cpu_device(mconfig, HD44848, tag, owner, clock, IS_CMOS)
{ }


hmcs47_cpu_device::hmcs47_cpu_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, UINT32 clock, UINT16 polarity) :
	hmcs40_cpu_device(mconfig, type, tag, owner, clock, HMCS47_FAMILY, polarity, 4, 12, 12, address_map_constructor(FUNC(hmcs47_cpu_device::program_2k), this), 8, address_map_constructor(FUNC(hmcs47_cpu_device::data_256x4), this))
{ }

hd38870_device::hd38870_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs47_cpu_device(mconfig, HD38870, tag, owner, clock, IS_PMOS)
{ }
hd44860_device::hd44860_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs47_cpu_device(mconfig, HD44860, tag, owner, clock, IS_CMOS)
{ }
hd44868_device::hd44868_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock) :
	hmcs47_cpu_device(mconfig, HD44868, tag, owner, clock, IS_CMOS)
{ }
#endif


//-------------------------------------------------
//  initialization
//-------------------------------------------------

static void reset_io(); // fwd
static void cycle();
static void clock_timer();

void hmcs40SetReadR(INT32 index, UINT8 (*read_r)(INT32))
{
	m_read_r[index & 7] = read_r;
}

void hmcs40SetWriteR(INT32 index, void (*write_r)(INT32, UINT8))
{
	m_write_r[index & 7] = write_r;
}

void hmcs40SetReadD(UINT16 (*read_d)(INT32))
{
	m_read_d = read_d;
}

void hmcs40SetWriteD(void (*write_d)(INT32, UINT16))
{
	m_write_d = write_d;
}

static UINT16 program_read_word(INT32 address)
{
	return m_rom[address & m_prgmask];
}

static UINT8 hd44_data_read(INT32 address)
{
	address &= 0xff;
	if (address & 0x80) address &= ~0x30; // "160x4" mirroring
	return m_ram[address];
}

static void hd44_data_write(INT32 address, UINT8 data)
{
	address &= 0xff;
	if (address & 0x80) address &= ~0x30; // "160x4" mirroring

	m_ram[address] = data;
}

static void hmcs40_init(int family, UINT16 polarity, int stack_lev, int pc_width, int prg_width, int data_width)
{
	m_family = family;
	m_polarity = polarity;
	m_stack_levels = stack_lev;
	m_pcwidth = pc_width;
	m_prgwidth = prg_width;
	m_datawidth = data_width;

	m_prgmask = (1 << m_prgwidth) - 1;
	m_datamask = (1 << m_datawidth) - 1;
	m_pcmask = (1 << m_pcwidth) - 1;

#if 0
	bprintf(0, _T("HMCS40 cpu init, family %x  polarity %x  stack %x  pc_width %x  prg_width %x  datawidth %x\n"),
		   m_family, m_polarity, m_stack_levels, m_pcwidth, m_prgwidth, m_datawidth);

	bprintf(0, _T("prgmask  %x\n"), m_prgmask);
	bprintf(0, _T("datamask %x\n"), m_datamask);
	bprintf(0, _T("pcmask   %x\n"), m_pcmask);
#endif

	m_ram = (UINT8*)BurnMalloc(0x100);

	for (int i = 0; i < 8; i++) {
		m_read_r[i] = dummy_read8;
		m_write_r[i] = dummy_write8;
	}

	m_read_d = dummy_read16;
	m_write_d = dummy_write16;
}

void hmcs40_hd44801Init(UINT8 *mcurom)
{
	m_rom = (UINT16*)mcurom;

	m_data_read = hd44_data_read;
	m_data_write = hd44_data_write;

	hmcs40_init(HMCS44_FAMILY, IS_CMOS, 4, 11, 12, 8);
}

void hmcs40Exit()
{
	BurnFree(m_ram);
}

void hmcs40Open(INT32)
{
}

void hmcs40Close()
{
}

void hmcs40Reset()
{
	// zerofill
	memset(m_ram, 0, 0x100);
	memset(m_stack, 0, sizeof(m_stack));
	m_op = 0;
	m_prev_op = 0;
	m_i = 0;
	m_eint_line = 0;
	m_halt = 0;
	m_prescaler = 0;
	m_block_int = false;

	m_pc = 0;
	m_prev_pc = 0;
	m_pc_upper = 0;
	m_a = 0;
	m_b = 0;
	m_x = 0;
	m_spx = 0;
	m_y = 0;
	m_spy = 0;
	m_s = 1;
	m_c = 0;

	m_tc = 0;
	m_cf = 0;
	m_ie = 0;
	m_iri = m_irt = 0;
	memset(m_if, 0, sizeof(m_if));
	m_tf = 0;
	memset(m_int, 0, sizeof(m_int));
	memset(m_r, 0, sizeof(m_r));
	m_d = 0;

	// original "reset"
	m_pc = m_pcmask;
	m_prev_op = m_op = 0;

	// clear interrupts
	m_cf = 0;
	m_ie = 0;
	m_iri = m_irt = 0;
	m_if[0] = m_if[1] = m_tf = 1;

	// all I/O ports set to input
	reset_io();

	// HMCS46/47 R70 set to 1 (already the default on CMOS devices)
	if (m_family == HMCS46_FAMILY || m_family == HMCS47_FAMILY)
		m_r[7] |= 1;
}

void hmcs40Scan(INT32 nAction)
{
	SCAN_VAR(m_icount);
	SCAN_VAR(total_cycles);
	SCAN_VAR(cycle_start);
	SCAN_VAR(end_run);

	SCAN_VAR(m_stack);
	SCAN_VAR(m_op);
	SCAN_VAR(m_prev_op);
	SCAN_VAR(m_i);
	SCAN_VAR(m_eint_line);
	SCAN_VAR(m_halt);
	SCAN_VAR(m_prescaler);
	SCAN_VAR(m_block_int);

	SCAN_VAR(m_pc);
	SCAN_VAR(m_prev_pc);
	SCAN_VAR(m_pc_upper);
	SCAN_VAR(m_a);
	SCAN_VAR(m_b);
	SCAN_VAR(m_x);
	SCAN_VAR(m_spx);
	SCAN_VAR(m_y);
	SCAN_VAR(m_spy);
	SCAN_VAR(m_s);
	SCAN_VAR(m_c);

	SCAN_VAR(m_tc);
	SCAN_VAR(m_cf);
	SCAN_VAR(m_ie);
	SCAN_VAR(m_iri);
	SCAN_VAR(m_irt);
	SCAN_VAR(m_if);
	SCAN_VAR(m_tf);
	SCAN_VAR(m_int);
	SCAN_VAR(m_r);
	SCAN_VAR(m_d);

	ScanVar(m_ram, 0x100, "hmcs40 RAM");
}


//-------------------------------------------------
//  internal memory maps
//-------------------------------------------------

/*

On HMCS42/43/44/45, only half of the ROM address range contains user-executable
code, there is up to 128 bytes of pattern data in the 2nd half. The 2nd half
also includes a couple of pages with factory test code by Hitachi, only executable
when MCU test mode is enabled externally (TEST pin). This data can still be accessed
with the P opcode.

On HMCS46/47, the 2nd half can be jumped to with a bank bit from R70. These MCUs
have 2 more banks with factory test code, but that part of the ROM is only accessible
under MCU test mode.

*/

#if 0
// dinknote: this is in words! 0x0000 - 0x0fff is 0x2000 bytes.
void hmcs40_cpu_device::program_1k(address_map &map)
{
	map(0x0000, 0x07ff).rom();
}

void hmcs40_cpu_device::program_2k(address_map &map)
{
	map(0x0000, 0x0fff).rom();
}


void hmcs40_cpu_device::data_80x4(address_map &map)
{
	map(0x00, 0x3f).ram();
	map(0x40, 0x4f).ram().mirror(0x30);
}

void hmcs40_cpu_device::data_160x4(address_map &map)
{
	map(0x00, 0x7f).ram();
	map(0x80, 0x8f).ram().mirror(0x30);
	map(0xc0, 0xcf).ram().mirror(0x30);
}

void hmcs40_cpu_device::data_256x4(address_map &map)
{
	map(0x00, 0xff).ram();
}
#endif

//-------------------------------------------------
//  i/o ports
//-------------------------------------------------

static UINT8 read_r(UINT8 index)
{
	index &= 7;

	if (index >= 6) {  // this is for hd44801
		bprintf(0, _T("HMCS40: read from unknown port R%d @ $%04X\n"), index, m_prev_pc);
	}

	UINT8 inp = m_read_r[index](index);

	if (m_polarity)
		return (inp & m_r[index]) & 0xf;
	else
		return (inp | m_r[index]) & 0xf;
}

static void write_r(UINT8 index, UINT8 data)
{
	index &= 7;

	if (index >= 6) {  // this is for hd44801
		bprintf(0, _T("HMCS40: ineffective write to port R%d = $%X @ $%04X\n"), index, data & 0xf, m_prev_pc);
	} else {
		data &= 0xf;
		m_r[index] = data;
		m_write_r[index](index, data);
	}
}

static int read_d(UINT8 index)
{
	index &= 0xf;
	UINT16 inp = m_read_d(1 << index);

	if (m_polarity)
		return BIT(inp & m_d, index);
	else
		return BIT(inp | m_d, index);
}

static void write_d(UINT8 index, int state)
{
	index &= 0xf;
	UINT16 mask = 1 << index;

	m_d = (m_d & ~mask) | (state ? mask : 0);
	m_write_d(0, m_d);//, mask);
}

static void reset_io()
{
	m_d = m_polarity;
	m_write_d(0, m_polarity);

	for (int i = 0; i < 8; i++)
		write_r(i, m_polarity);
}

#include "hmcs40op.cpp"

#if 0
// HMCS43:
// R0 is input-only, R1 is i/o, R2,R3 are output-only, no R4-R7
// D0-D3 are i/o, D4-D15 are output-only

UINT8 hmcs43_cpu_device::read_r(UINT8 index)
{
	index &= 7;

	if (index >= 2)
		logerror("read from %s port R%d @ $%04X\n", (index >= 4) ? "unknown" : "output", index, m_prev_pc);

	return read_r(index);
}

void hmcs43_cpu_device::write_r(UINT8 index, UINT8 data)
{
	index &= 7;

	if (index != 0 && index < 4)
		write_r(index, data);
	else
		logerror("ineffective write to port R%d = $%X @ $%04X\n", index, data & 0xf, m_prev_pc);
}

int hmcs43_cpu_device::read_d(UINT8 index)
{
	index &= 15;

	if (index >= 4)
		logerror("read from output pin D%d @ $%04X\n", index, m_prev_pc);

	return read_d(index);
}

// HMCS44:
// R0-R3 are i/o, R4,R5 are extra registers, no R6,R7
// D0-D15 are i/o

UINT8 hmcs44_cpu_device::read_r(UINT8 index)
{
	index &= 7;

	if (index >= 6)
		logerror("read from unknown port R%d @ $%04X\n", index, m_prev_pc);

	return read_r(index);
}

void hmcs44_cpu_device::write_r(UINT8 index, UINT8 data)
{
	index &= 7;

	if (index < 6)
		write_r(index, data);
	else
		logerror("ineffective write to port R%d = $%X @ $%04X\n", index, data & 0xf, m_prev_pc);
}

// HMCS45:
// R0-R5 are i/o, R6 is output-only, no R7
// D0-D15 are i/o

UINT8 hmcs45_cpu_device::read_r(UINT8 index)
{
	index &= 7;

	if (index >= 6)
		logerror("read from %s port R%d @ $%04X\n", (index == 7) ? "unknown" : "output", index, m_prev_pc);

	return read_r(index);
}

void hmcs45_cpu_device::write_r(UINT8 index, UINT8 data)
{
	index &= 7;

	if (index != 7)
		write_r(index, data);
	else
		logerror("ineffective write to port R%d = $%X @ $%04X\n", index, data & 0xf, m_prev_pc);
}

// HMCS46:
// R0-R3 are i/o, R4,R5,R7 are extra registers, no R6
// D0-D15 are i/o

UINT8 hmcs46_cpu_device::read_r(UINT8 index)
{
	index &= 7;

	if (index == 6)
		logerror("read from unknown port R%d @ $%04X\n", index, m_prev_pc);

	return read_r(index);
}

void hmcs46_cpu_device::write_r(UINT8 index, UINT8 data)
{
	index &= 7;

	if (index != 6)
		write_r(index, data);
	else
		logerror("ineffective write to port R%d = $%X @ $%04X\n", index, data & 0xf, m_prev_pc);
}

// HMCS47:
// R0-R5 are i/o, R6 is output-only, R7 is an extra register
// D0-D15 are i/o

UINT8 hmcs47_cpu_device::read_r(UINT8 index)
{
	index &= 7;

	if (index == 6)
		logerror("read from output port R%d @ $%04X\n", index, m_prev_pc);

	return read_r(index);
}
#endif

//-------------------------------------------------
//  interrupt/timer
//-------------------------------------------------

static void take_interrupt()
{
	push_stack();
	m_ie = 0;

	// line 0/1 for external interrupt, let's use 2 for t/c interrupt
	//int line = (m_iri) ? m_eint_line : 2;
	//standard_irq_callback(line, m_pc);

	// vector $3f, on page 0(timer/counter), or page 1(external)
	// external interrupt has priority over t/c interrupt
	m_pc = 0x3f | (m_iri ? 0x40 : 0);
	if (m_iri)
		m_iri = 0;
	else
		m_irt = 0;

	m_prev_pc = m_pc;

	cycle();
}

void hmcs40SetIRQLine(INT32 line, INT32 state)
{
	state = state ? 1 : 0;

	// halt/unhalt mcu
	if (line == HMCS40_INPUT_LINE_HLT && state != m_halt)
	{
		m_halt = state;
		return;
	}

	if (line != 0 && line != 1)
		return;

	// external interrupt request on rising edge
	if (state && !m_int[line])
	{
		if (!m_if[line])
		{
			m_eint_line = line;
			m_iri = 1;
			m_if[line] = 1;
		}

		// clock tc if it is in counter mode
		if (m_cf && line == 1)
			clock_timer();
	}

	m_int[line] = state;
}

static void clock_timer()
{
	// increment timer/counter
	m_tc = (m_tc + 1) & 0xf;

	// timer interrupt request on overflow
	if (m_tc == 0 && !m_tf)
	{
		m_irt = 1;
		m_tf = 1;
	}
}

static void clock_prescaler()
{
	m_prescaler = (m_prescaler + 1) & 0x3f;

	// timer prescaler overflow
	if (m_prescaler == 0 && !m_cf)
		clock_timer();
}


//-------------------------------------------------
//  execute
//-------------------------------------------------

static inline void increment_pc()
{
	// PC lower bits is a LFSR identical to TI TMS1000
	UINT8 mask = 0x3f;
	UINT8 low = m_pc & mask;
	int fb = ((low << 1) & 0x20) == (low & 0x20);

	if (low == (mask >> 1))
		fb = 1;
	else if (low == mask)
		fb = 0;

	m_pc = (m_pc & ~mask) | (((m_pc << 1) | fb) & mask);
}

static void cycle()
{
	m_icount--;
	clock_prescaler();
}

INT32 hmcs40GetActive()
{
	return 0;
}

INT32 hmcs40TotalCycles()
{
	return total_cycles + (cycle_start - m_icount);
}

void hmcs40NewFrame()
{
	total_cycles = 0;
}

INT32 hmcs40Idle(INT32 cycles)
{
	total_cycles += cycles;

	return cycles;
}

void hmcs40RunEnd()
{
	end_run = 1;
}

INT32 hmcs40Run(INT32 cycles)
{
	m_icount = cycles;
	cycle_start = cycles;
	end_run = 0;

	// in HLT state, the internal clock is not running
	if (m_halt)
	{
		//debugger_wait_hook();
		m_icount = 0;
	}

	while (m_icount > 0 && !end_run)
	{
		// LPU is handled 1 cycle later
		if ((m_prev_op & 0x7e0) == 0x340)
			m_pc = ((m_pc_upper << 6) | (m_pc & 0x3f)) & m_pcmask;

		// remember previous state
		m_prev_op = m_op;
		m_prev_pc = m_pc;

		// check/handle interrupt
		if (m_ie && (m_iri || m_irt) && !m_block_int)
			take_interrupt();
		m_block_int = false;

		// fetch next opcode
		//debugger_instruction_hook(m_pc);
		m_op = program_read_word(m_pc) & 0x3ff;
		m_i = BITSWAP04(m_op,0,1,2,3); // reversed bit-order for 4-bit immediate param (except for XAMR)
		increment_pc();
		cycle();

		// handle opcode
		switch (m_op & 0x3f0)
		{
			case 0x1c0: case 0x1d0: case 0x1e0: case 0x1f0: op_br(); break;
			case 0x3c0: case 0x3d0: case 0x3e0: case 0x3f0: op_cal(); break;
			case 0x340: case 0x350: op_lpu(); break;

			case 0x010: op_lmiiy(); break;
			case 0x070: op_lai(); break;
			case 0x080: op_ai(); break;
			case 0x0f0: op_xamr(); break;
			case 0x140: op_lxi(); break;
			case 0x150: op_lyi(); break;
			case 0x160: op_lbi(); break;
			case 0x170: op_lti(); break;
			case 0x210: op_mnei(); break;
			case 0x270: op_alei(); break;
			case 0x280: op_ynei(); break;

			default:
				switch (m_op & 0x3fc)
				{
			case 0x0c0: case 0x0c4: op_lar(); break;
			case 0x0e0: case 0x0e4: op_lbr(); break;
			case 0x2c0: case 0x2c4: op_lra(); break;
			case 0x2e0: case 0x2e4: op_lrb(); break;
			case 0x360: case 0x364: op_tbr(); break;
			case 0x368: case 0x36c: op_p(); break;

			case 0x000: op_xsp(); break;
			case 0x004: op_sem(); break;
			case 0x008: op_lam(); break;
			case 0x020: op_lbm(); break;
			case 0x0d0: op_sedd(); break;
			case 0x200: op_tm(); break;
			case 0x204: op_rem(); break;
			case 0x208: op_xma(); break;
			case 0x220: op_xmb(); break;
			case 0x2d0: op_redd(); break;

			default:
				switch (m_op)
				{
			case 0x024: op_blem(); break;
			case 0x030: op_amc(); break;
			case 0x034: op_am(); break;
			case 0x03c: op_lta(); break;
			case 0x040: op_lxa(); break;
			case 0x045: op_das(); break;
			case 0x046: op_daa(); break;
			case 0x04c: op_rec(); break;
			case 0x04f: op_sec(); break;
			case 0x050: op_lya(); break;
			case 0x054: op_iy(); break;
			case 0x058: op_ayy(); break;
			case 0x060: op_lba(); break;
			case 0x064: op_ib(); break;
			case 0x090: op_sed(); break;
			case 0x094: op_td(); break;
			case 0x0a0: op_seif1(); break;
			case 0x0a1: op_secf(); break;
			case 0x0a2: op_seif0(); break;
			case 0x0a4: op_seie(); break;
			case 0x0a5: op_setf(); break;

			case 0x110: case 0x111: op_lmaiy(); break;
			case 0x114: case 0x115: op_lmady(); break;
			case 0x118: op_lay(); break;
			case 0x120: op_or(); break;
			case 0x124: op_anem(); break;
			case 0x1a0: op_tif1(); break;
			case 0x1a1: op_ti1(); break;
			case 0x1a2: op_tif0(); break;
			case 0x1a3: op_ti0(); break;
			case 0x1a5: op_ttf(); break;

			case 0x224: op_rotr(); break;
			case 0x225: op_rotl(); break;
			case 0x230: op_smc(); break;
			case 0x234: op_alem(); break;
			case 0x23c: op_lat(); break;
			case 0x240: op_laspx(); break;
			case 0x244: op_nega(); break;
			case 0x24f: op_tc(); break;
			case 0x250: op_laspy(); break;
			case 0x254: op_dy(); break;
			case 0x258: op_syy(); break;
			case 0x260: op_lab(); break;
			case 0x267: op_db(); break;
			case 0x290: op_red(); break;
			case 0x2a0: op_reif1(); break;
			case 0x2a1: op_recf(); break;
			case 0x2a2: op_reif0(); break;
			case 0x2a4: op_reie(); break;
			case 0x2a5: op_retf(); break;

			case 0x320: op_comb(); break;
			case 0x324: op_bnem(); break;
			case 0x3a4: op_rtni(); break;
			case 0x3a7: op_rtn(); break;

			default: op_illegal(); break;
				}
				break; // 0x3ff

				}
				break; // 0x3fc

		} // 0x3f0
	}

	cycles = cycles - m_icount;
	m_icount = cycle_start = 0;

	total_cycles += cycles;

	return cycles;
}
