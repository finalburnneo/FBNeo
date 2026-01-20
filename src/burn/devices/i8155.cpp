// license:BSD-3-Clause
// copyright-holders:Curt Coder,AJR
/**********************************************************************

    Intel 8155/8156 - 2048-Bit Static MOS RAM with I/O Ports and Timer

    The timer primarily functions as a square-wave generator, but can
    also be programmed for a single-cycle low pulse on terminal count.

    The only difference between 8155 and 8156 is that pin 8 (CE) is
    active low on the former device and active high on the latter.

    National's NSC810 RAM-I/O-Timer is pin-compatible with the Intel
    8156, but has different I/O registers (including a second timer)
    with incompatible mapping.

**********************************************************************/
// FBNeo note: ported from MAME 0.280ish -dink 2026

/*

    TODO:

    - ALT 3 and ALT 4 strobed port modes
    - optional NVRAM backup for CMOS versions

*/

#include "burnint.h"
#include "i8155.h"
#include "dtimer.h"
#include "bitswap.h"

#if 0
  extern int counter;
  #define LOGMASKED if (counter) bprintf
#else
  #define LOGMASKED
#endif

// CPU interface
static int m_io_m;                 // I/O or memory select
static UINT8 m_ad;               // address

// registers
static UINT8 m_command;          // command register
static UINT8 m_status;           // status register
static UINT8 m_output[3];        // output latches

// RAM
static UINT8 *m_ram = NULL;

// counter
static UINT16 m_count_length;    // count length register (assigned)
static UINT16 m_count_loaded;    // count length register (loaded)
static int m_to;                   // timer output
static bool m_count_even_phase;

// timers
static dtimer m_timer;         // counter timer
static dtimer m_timer_tc;      // counter timer (for TC)

// fbneo interface stuff
static void dummy_write(INT32 offset, UINT8 data) { }
static UINT8 dummy_read(INT32 offset) { return 0; }
static void dummy_state(INT32 state) { }

static UINT8 (*m_in_pa_cb)(INT32 offset) = NULL;
static UINT8 (*m_in_pb_cb)(INT32 offset) = NULL;
static UINT8 (*m_in_pc_cb)(INT32 offset) = NULL;
static void (*m_out_pa_cb)(INT32 offset, UINT8 data) = NULL;
static void (*m_out_pb_cb)(INT32 offset, UINT8 data) = NULL;
static void (*m_out_pc_cb)(INT32 offset, UINT8 data) = NULL;
// this gets called for each change of the TIMER OUT pin (pin 6)
static void (*m_out_to_cb)(INT32 state) = NULL;

void i8155_set_pa_read_cb(UINT8 (*cb)(INT32)) { m_in_pa_cb = cb; }
void i8155_set_pb_read_cb(UINT8 (*cb)(INT32)) { m_in_pb_cb = cb; }
void i8155_set_pc_read_cb(UINT8 (*cb)(INT32)) { m_in_pc_cb = cb; }

void i8155_set_pa_write_cb(void (*cb)(INT32, UINT8)) { m_out_pa_cb = cb; }
void i8155_set_pb_write_cb(void (*cb)(INT32, UINT8)) { m_out_pb_cb = cb; }
void i8155_set_pc_write_cb(void (*cb)(INT32, UINT8)) { m_out_pc_cb = cb; }

void i8155_set_to_write_cb(void (*cb)(INT32)) { m_out_to_cb = cb; }

// forwards (timer callbacks)
static void timer_tc(INT32 param);
static void timer_half_counted(INT32 param);

void i8155_init()
{
	m_in_pa_cb = dummy_read;
	m_in_pb_cb = dummy_read;
	m_in_pc_cb = dummy_read;
	m_out_pa_cb = dummy_write;
	m_out_pb_cb = dummy_write;
	m_out_pc_cb = dummy_write;
	m_out_to_cb = dummy_state;

	m_ram = (UINT8*)BurnMalloc(0x100);
	// allocate timers
	m_timer.init(0, timer_half_counted);
	m_timer_tc.init(0, timer_tc);
	//m_timer = timer_alloc(FUNC(timer_half_counted), this);
	//m_timer_tc = timer_alloc(FUNC(timer_tc), this);

	m_io_m = 0;
	m_ad = 0;
	m_command = 0;
	m_status = 0;
	m_output[0] = m_output[1] = m_output[2] = 0;

	m_count_length = 0;
	m_count_loaded = 0;
	m_to = 0;
	m_count_even_phase = 0;
}

void i8155_scan()
{
	m_timer.scan();
	m_timer_tc.scan();

	SCAN_VAR(m_io_m);
	SCAN_VAR(m_ad);
	SCAN_VAR(m_command);
	SCAN_VAR(m_status);
	SCAN_VAR(m_output);

	SCAN_VAR(m_count_length);
	SCAN_VAR(m_count_loaded);
	SCAN_VAR(m_to);
	SCAN_VAR(m_count_even_phase);
	ScanVar(m_ram, 0x100, "i8155 Ram");
}

void i8155_exit()
{
	BurnFree(m_ram);
}

INT32 i8155_timers_run(INT32 cycles)
{
	for (int i = 0; i < cycles; i++) {
		// must (!) run in lockstep.
		m_timer_tc.run(1);
		m_timer.run(1);
	}

	return cycles;
}

//**************************************************************************
//  MACROS / CONSTANTS
//**************************************************************************

#define LOG_PORT  (1U << 1)
#define LOG_TIMER (1U << 2)
#define VERBOSE (0)

enum
{
	REGISTER_COMMAND = 0,
	REGISTER_STATUS = 0,
	REGISTER_PORT_A,
	REGISTER_PORT_B,
	REGISTER_PORT_C,
	REGISTER_TIMER_LOW,
	REGISTER_TIMER_HIGH
};

enum
{
	PORT_A = 0,
	PORT_B,
	PORT_C,
	PORT_COUNT
};

enum
{
	PORT_MODE_INPUT = 0,
	PORT_MODE_OUTPUT,
	PORT_MODE_STROBED_PORT_A,   // not supported
	PORT_MODE_STROBED           // not supported
};

enum
{
	MEMORY = 0,
	IO
};

#define COMMAND_PA                  0x01
#define COMMAND_PB                  0x02
#define COMMAND_PC_MASK             0x0c
#define COMMAND_PC_ALT_1            0x00
#define COMMAND_PC_ALT_2            0x0c
#define COMMAND_PC_ALT_3            0x04    // not supported
#define COMMAND_PC_ALT_4            0x08    // not supported
#define COMMAND_IEA                 0x10    // not supported
#define COMMAND_IEB                 0x20    // not supported
#define COMMAND_TM_MASK             0xc0
#define COMMAND_TM_NOP              0x00
#define COMMAND_TM_STOP             0x40
#define COMMAND_TM_STOP_AFTER_TC    0x80
#define COMMAND_TM_START            0xc0

#define STATUS_INTR_A               0x01    // not supported
#define STATUS_A_BF                 0x02    // not supported
#define STATUS_INTE_A               0x04    // not supported
#define STATUS_INTR_B               0x08    // not supported
#define STATUS_B_BF                 0x10    // not supported
#define STATUS_INTE_B               0x20    // not supported
#define STATUS_TIMER                0x40

#define TIMER_MODE_MASK             0xc0
#define TIMER_MODE_AUTO_RELOAD      0x40
#define TIMER_MODE_TC_PULSE         0x80



//**************************************************************************
//  INLINE HELPERS
//**************************************************************************
//#define d_min(a, b) (((a) < (b)) ? (a) : (b))
//#define d_max(a, b) (((a) > (b)) ? (a) : (b))
#undef d_min
#undef d_max

static INT32 d_min(INT32 a, INT32 b)
{
	return (((a) < (b)) ? (a) : (b));
}

static INT32 d_max(INT32 a, INT32 b)
{
	return (((a) > (b)) ? (a) : (b));
}

static UINT8 get_timer_mode()
{
	return (m_count_loaded >> 8) & TIMER_MODE_MASK;
}

static UINT16 get_timer_count()
{
	if (m_timer.isrunning())
	{
		// timer counts down by twos
		return d_min((UINT16(m_timer.timeleft()) + 1) << 1, m_count_loaded & 0x3ffe) | (m_count_even_phase ? 0 : 1);
	}
	else
		return m_count_length;
}

static void timer_output(int to)
{
	if (to == m_to)
		return;

	m_to = to;
	m_out_to_cb(to);

	LOGMASKED(LOG_TIMER, _T("Timer output: %u\n"), to);
}

static void timer_stop_count()
{
	// stop counting
	if (m_timer.isrunning())
	{
		m_count_loaded = (m_count_loaded & (TIMER_MODE_MASK << 8)) | get_timer_count();
		m_timer.enable(false);
	}
	m_timer_tc.enable(false);

	// clear timer output
	timer_output(1);
}

static void timer_reload_count()
{
	m_count_loaded = m_count_length;

	// valid counts range from 2 to 3FFF
	if ((m_count_length & 0x3fff) < 2)
	{
		timer_stop_count();
		return;
	}

	// begin the odd half of the count, with one extra cycle if count is odd
	m_count_even_phase = false;

	// set up our timer
	//	m_timer->adjust(clocks_to_attotime(((m_count_length & 0x3ffe) >> 1) + (m_count_length & 1)));
	m_timer.start(((m_count_length & 0x3ffe) >> 1) + (m_count_length & 1), -1, 1, 0);
	timer_output(1);

	switch (get_timer_mode())
	{
	case 0:
		// puts out LOW during second half of count
		LOGMASKED(LOG_TIMER, _T("Timer loaded with %d (Mode: LOW)\n"), m_count_loaded & 0x3fff);
		break;

	case TIMER_MODE_AUTO_RELOAD:
		// square wave, i.e. the period of the square wave equals the count length programmed with automatic reload at terminal count
		LOGMASKED(LOG_TIMER, _T("Timer loaded with %d (Mode: Square wave)\n"), m_count_loaded & 0x3fff);
		break;

	case TIMER_MODE_TC_PULSE:
		// single pulse upon TC being reached
		LOGMASKED(LOG_TIMER, _T("Timer loaded with %d (Mode: Single pulse)\n"), m_count_loaded & 0x3fff);
		break;

	case TIMER_MODE_TC_PULSE | TIMER_MODE_AUTO_RELOAD:
		// automatic reload, i.e. single pulse every time TC is reached
		LOGMASKED(LOG_TIMER, _T("Timer loaded with %d (Mode: Automatic reload)\n"), m_count_loaded & 0x3fff);
		break;
	}
}

static int get_port_mode(int port)
{
	int mode = -1;

	switch (port)
	{
	case PORT_A:
		mode = (m_command & COMMAND_PA) ? PORT_MODE_OUTPUT : PORT_MODE_INPUT;
		break;

	case PORT_B:
		mode = (m_command & COMMAND_PB) ? PORT_MODE_OUTPUT : PORT_MODE_INPUT;
		break;

	case PORT_C:
		switch (m_command & COMMAND_PC_MASK)
		{
		case COMMAND_PC_ALT_1: mode = PORT_MODE_INPUT;          break;
		case COMMAND_PC_ALT_2: mode = PORT_MODE_OUTPUT;         break;
		case COMMAND_PC_ALT_3: mode = PORT_MODE_STROBED_PORT_A; break;
		case COMMAND_PC_ALT_4: mode = PORT_MODE_STROBED;        break;
		}
		break;
	}

	return mode;
}

static UINT8 read_port(int port)
{
	UINT8 data = 0;

	switch (get_port_mode(port))
	{
	case PORT_MODE_INPUT:
		data = (port == PORT_A) ? m_in_pa_cb(0) : ((port == PORT_B) ? m_in_pb_cb(0) : m_in_pc_cb(0));
		break;

	case PORT_MODE_OUTPUT:
		data = m_output[port];
		break;

	default:
		// strobed mode not implemented yet
		//logerror(_T("8155 Unsupported Port C mode!\n"));
		break;
	}

	return data;
}

static void write_port(int port, UINT8 data)
{
	m_output[port] = data;
	switch (get_port_mode(port))
	{
	case PORT_MODE_OUTPUT:
		if (port == PORT_A)
			m_out_pa_cb((INT32)0, m_output[port]);
		else if (port == PORT_B)
			m_out_pb_cb((INT32)0, m_output[port]);
		else
			m_out_pc_cb((INT32)0, m_output[port]);
		break;
	}
}


//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  i8155_device - constructor
//-------------------------------------------------
#if 0
i8155_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	i8155_device(mconfig, I8155, tag, owner, clock)
{
}

i8155_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, type, tag, owner, clock),
	m_in_pa_cb(*this, 0),
	m_in_pb_cb(*this, 0),
	m_in_pc_cb(*this, 0),
	m_out_pa_cb(*this),
	m_out_pb_cb(*this),
	m_out_pc_cb(*this),
	m_out_to_cb(*this),
	m_command(0),
	m_status(0),
	m_count_length(0),
	m_count_loaded(0),
	m_to(0),
	m_count_even_phase(false)
{
}


//-------------------------------------------------
//  i8156_device - constructor
//-------------------------------------------------

i8156_device::i8156_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	i8155_device(mconfig, I8156, tag, owner, clock)
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void device_start()
{
	// allocate RAM
	m_ram = make_unique_clear<UINT8[]>(256);

	// allocate timers
	m_timer = timer_alloc(FUNC(timer_half_counted), this);
	m_timer_tc = timer_alloc(FUNC(timer_tc), this);

	// register for state saving
	save_item(NAME(m_io_m));
	save_item(NAME(m_ad));
	save_item(NAME(m_command));
	save_item(NAME(m_status));
	save_item(NAME(m_output));
	save_pointer(NAME(m_ram), 256);
	save_item(NAME(m_count_length));
	save_item(NAME(m_count_loaded));
	save_item(NAME(m_to));
}
#endif


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------
static void register_w(int offset, UINT8 data); // fwd

void i8155_reset()
{
	{ // dink addition
		memset(m_ram, 0, 0x100); // dink
		m_io_m = 0;
		m_ad = 0;
		m_command = 0;
		m_status = 0;
		m_output[0] = m_output[1] = m_output[2] = 0;

		m_count_length = 0;
		m_count_loaded = 0;
		m_to = 0;
		m_count_even_phase = 0;
		m_timer.stop_retrig();
		m_timer_tc.stop_retrig();
	}


	// clear output registers
	m_output[PORT_A] = 0;
	m_output[PORT_B] = 0;
	m_output[PORT_C] = 0;

	// set ports to input mode
	register_w(REGISTER_COMMAND, m_command & ~(COMMAND_PA | COMMAND_PB | COMMAND_PC_MASK));

	// clear timer flag
	m_status &= ~STATUS_TIMER;

	// stop timer
	timer_stop_count();
}


//-------------------------------------------------
//  timer_half_counted - handler timer events
//-------------------------------------------------

static void timer_half_counted(INT32 param)
{
	if (m_count_even_phase)
	{
		timer_output(1);
		m_count_even_phase = false;

		if ((get_timer_mode() & TIMER_MODE_AUTO_RELOAD) == 0 || (m_command & COMMAND_TM_MASK) == COMMAND_TM_STOP_AFTER_TC)
		{
			// stop timer
			timer_stop_count();
			LOGMASKED(LOG_TIMER, _T("Timer stopped\n"));
		}
		else
		{
			// automatically reload the counter
			timer_reload_count();
		}
	}
	else
	{
		LOGMASKED(LOG_TIMER, _T("Timer count half finished\n"));

		// reload the even half of the count
		// m_timer->adjust(clocks_to_attotime((m_count_loaded & 0x3ffe) >> 1));
		m_timer.start((m_count_loaded & 0x3ffe) >> 1, -1, 1, 0);
		m_count_even_phase = true;

		// square wave modes produce a low output in the second half of the counting period
		if ((get_timer_mode() & TIMER_MODE_TC_PULSE) == 0) {
			timer_output(0);
		}
		else {
			// m_timer_tc->adjust(clocks_to_attotime((d_max(m_count_loaded & 0x3ffe, 2) - 2) >> 1));
			m_timer_tc.start(((d_max(m_count_loaded & 0x3ffe, 2) - 2) >> 1), -1, 1, 0);
		}
	}
}


//-------------------------------------------------
//  timer_tc - generate TC low pulse
//-------------------------------------------------

static void timer_tc(INT32 param)
{
	if ((get_timer_mode() & TIMER_MODE_TC_PULSE) != 0)
	{
		// pulse low on TC being reached
		timer_output(0);
	}

	// set timer flag
	m_status |= STATUS_TIMER;
}


//-------------------------------------------------
//  io_r - register read
//-------------------------------------------------

UINT8 i8155_io_read(INT32 offset)
{
	UINT8 data = 0;

	switch (offset & 0x07)
	{
	case REGISTER_STATUS:
		data = m_status;

		// clear timer flag
		m_status &= ~STATUS_TIMER;
		break;

	case REGISTER_PORT_A:
		data = read_port(PORT_A);
		break;

	case REGISTER_PORT_B:
		data = read_port(PORT_B);
		break;

	case REGISTER_PORT_C:
		data = read_port(PORT_C) | 0xc0;
		break;

	case REGISTER_TIMER_LOW:
		data = get_timer_count() & 0xff;
		break;

	case REGISTER_TIMER_HIGH:
		data = (get_timer_count() >> 8 & 0x3f) | get_timer_mode();
		break;
	}

	return data;
}

//-------------------------------------------------
//  write_command - set port modes and start/stop
//  timer
//-------------------------------------------------

static void write_command(UINT8 data)
{
	UINT8 old_command = m_command;
	m_command = data;

	LOGMASKED(LOG_PORT, _T("Port A Mode: %s\n"), (data & COMMAND_PA) ? _T("output") : _T("input"));
	LOGMASKED(LOG_PORT, _T("Port B Mode: %s\n"), (data & COMMAND_PB) ? _T("output") : _T("input"));

	LOGMASKED(LOG_PORT, _T("Port A Interrupt: %s\n"), (data & COMMAND_IEA) ? _T("enabled") : _T("disabled"));
	LOGMASKED(LOG_PORT, _T("Port B Interrupt: %s\n"), (data & COMMAND_IEB) ? _T("enabled") : _T("disabled"));

	if ((data & COMMAND_PA) && (~old_command & COMMAND_PA))
		m_out_pa_cb((INT32)0, m_output[PORT_A]);
	if ((data & COMMAND_PB) && (~old_command & COMMAND_PB))
		m_out_pb_cb((INT32)0, m_output[PORT_B]);

	switch (data & COMMAND_PC_MASK)
	{
	case COMMAND_PC_ALT_1:
		LOGMASKED(LOG_PORT, _T("Port C Mode: Alt 1 (PC0-PC5 input)\n"));
		break;

	case COMMAND_PC_ALT_2:
		LOGMASKED(LOG_PORT, _T("Port C Mode: Alt 2 (PC0-PC5 output)\n"));
		if ((old_command & COMMAND_PC_MASK) != COMMAND_PC_ALT_2)
			m_out_pc_cb((INT32)0, m_output[PORT_C]);
		break;

	case COMMAND_PC_ALT_3:
		LOGMASKED(LOG_PORT, _T("Port C Mode: Alt 3 (PC0-PC2 A handshake, PC3-PC5 output)\n"));
		break;

	case COMMAND_PC_ALT_4:
		LOGMASKED(LOG_PORT, _T("Port C Mode: Alt 4 (PC0-PC2 A handshake, PC3-PC5 B handshake)\n"));
		break;
	}

	switch (data & COMMAND_TM_MASK)
	{
	case COMMAND_TM_NOP:
		// do not affect counter operation
		break;

	case COMMAND_TM_STOP:
		// NOP if timer has not started, stop counting if the timer is running
		LOGMASKED(LOG_PORT, _T("Timer Command: Stop\n"));
		timer_stop_count();
		break;

	case COMMAND_TM_STOP_AFTER_TC:
		// stop immediately after present TC is reached (NOP if timer has not started)
		LOGMASKED(LOG_PORT, _T("Timer Command: Stop after TC\n"));
		break;

	case COMMAND_TM_START:
		LOGMASKED(LOG_PORT, _T("Timer Command: Start\n"));

		if (m_timer.isrunning())
		{
			// if timer is running, start the new mode and CNT length immediately after present TC is reached
		}
		else
		{
			// load mode and CNT length and start immediately after loading (if timer is not running)
			timer_reload_count();
		}
		break;
	}
}


//-------------------------------------------------
//  register_w - register write
//-------------------------------------------------

static void register_w(int offset, UINT8 data)
{
	switch (offset & 0x07)
	{
	case REGISTER_COMMAND:
		write_command(data);
		break;

	case REGISTER_PORT_A:
		write_port(PORT_A, data);
		break;

	case REGISTER_PORT_B:
		write_port(PORT_B, data);
		break;

	case REGISTER_PORT_C:
		write_port(PORT_C, data & 0x3f);
		break;

	case REGISTER_TIMER_LOW:
		m_count_length = (m_count_length & 0xff00) | data;
		break;

	case REGISTER_TIMER_HIGH:
		m_count_length = (data << 8) | (m_count_length & 0xff);
		break;
	}
}

//-------------------------------------------------
//  io_w - register write
//-------------------------------------------------

void i8155_io_write(INT32 offset, UINT8 data)
{
	register_w(offset, data);
}


//-------------------------------------------------
//  memory_r - internal RAM read
//-------------------------------------------------

UINT8 i8155_memory_read(INT32 offset)
{
	return m_ram[offset & 0xff];
}


//-------------------------------------------------
//  memory_w - internal RAM write
//-------------------------------------------------

void i8155_memory_write(INT32 offset, UINT8 data)
{
	m_ram[offset & 0xff] = data;
}


//-------------------------------------------------
//  ale_w - address latch write
//-------------------------------------------------

void i8155_ale_write(INT32 offset, UINT8 data)
{
	// I/O / memory select
	m_io_m = BIT(offset, 0);

	// address
	m_ad = data;
}


//-------------------------------------------------
//  data_r - memory or I/O read
//-------------------------------------------------

UINT8 i8155_data_read()
{
	UINT8 data = 0;

	switch (m_io_m)
	{
	case MEMORY:
		data = i8155_memory_read(m_ad);
		break;

	case IO:
		data = i8155_io_read(m_ad);
		break;
	}

	return data;
}


//-------------------------------------------------
//  data_w - memory or I/O write
//-------------------------------------------------

void i8155_data_write(UINT8 data)
{
	switch (m_io_m)
	{
	case MEMORY:
		i8155_memory_write(m_ad, data);
		break;

	case IO:
		i8155_io_write(m_ad, data);
		break;
	}
}
