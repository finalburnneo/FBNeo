// license:BSD-3-Clause
// copyright-holders:Wilbert Pol
/**********************************************************************

    8259 PIC interface and emulation

    The 8259 is a programmable interrupt controller used to multiplex
    interrupts for x86 and other computers.  The chip is set up by
    writing a series of Initialization Command Words (ICWs) after which
    the chip is operational and capable of dispatching interrupts.  After
    this, Operation Command Words (OCWs) can be written to control further
    behavior.
ported from mame 0.228 by dink, oct 2022
**********************************************************************/

#include "burnint.h"
#include "pic8259.h"
#include "bitswap.h"

#define LOG_GENERAL (1U << 0)
#define LOG_ICW     (1U << 1)
#define LOG_OCW     (1U << 2)

//#define VERBOSE (LOG_GENERAL | LOG_ICW | LOG_OCW)
//#include "logmacro.h"
#define LOG
#define LOGMASKED
#define logerror
#define LOGICW
#define LOGOCW
//#define LOGICW(...) LOGMASKED(LOG_ICW, __VA_ARGS__)
//#define LOGOCW(...) LOGMASKED(LOG_OCW, __VA_ARGS__)

enum
{
	ICW1 = 0,
	ICW2,
	ICW3,
	ICW4,
	READY
};

static UINT8 (*m_read_slave_ack_func)(UINT8) = NULL;
static void (*m_out_int_func)(INT32) = NULL;
static UINT8 (*m_in_sp_func)() = NULL;

static UINT8 m_state;

static UINT8 m_isr;
static UINT8 m_irr;
static UINT8 m_prio;
static UINT8 m_imr;
static UINT8 m_irq_lines;

static UINT8 m_input;
static UINT8 m_ocw3;

static UINT8 m_master;
/* ICW1 state */
static UINT8 m_level_trig_mode;
static UINT8 m_vector_size;
static UINT8 m_cascade;
static UINT8 m_icw4_needed;
static UINT32 m_vector_addr_low;
/* ICW2 state */
static UINT8 m_base;
static UINT8 m_vector_addr_high;

/* ICW3 state */
static UINT8 m_slave;

/* ICW4 state */
static UINT8 m_nested;
static UINT8 m_mode;
static UINT8 m_auto_eoi;
static UINT8 m_is_x86;

static INT32 m_current_level;
static UINT8 m_inta_sequence;

void pic8259_scan(INT32 nAction)
{
	SCAN_VAR(m_state);

	SCAN_VAR(m_isr);
	SCAN_VAR(m_irr);
	SCAN_VAR(m_prio);
	SCAN_VAR(m_imr);
	SCAN_VAR(m_irq_lines);

	SCAN_VAR(m_input);
	SCAN_VAR(m_ocw3);

	SCAN_VAR(m_master);
	SCAN_VAR(m_level_trig_mode);
	SCAN_VAR(m_vector_size);
	SCAN_VAR(m_cascade);
	SCAN_VAR(m_icw4_needed);
	SCAN_VAR(m_vector_addr_low);

	SCAN_VAR(m_base);
	SCAN_VAR(m_vector_addr_high);

	SCAN_VAR(m_slave);
	SCAN_VAR(m_nested);
	SCAN_VAR(m_mode);
	SCAN_VAR(m_auto_eoi);
	SCAN_VAR(m_is_x86);

	SCAN_VAR(m_current_level);
	SCAN_VAR(m_inta_sequence);
}

static void irq_timer_tick()
{
	/* check the various IRQs */
	for (int n = 0, irq = m_prio; n < 8; n++, irq = (irq + 1) & 7)
	{
		UINT8 mask = 1 << irq;

		/* is this IRQ in service and not cascading and sfnm? */
		if ((m_isr & mask) && !(m_master && m_cascade && m_nested && (m_slave & mask)))
		{
			LOG("pic8259_timerproc(): PIC IR%d still in service\n", irq);
			break;
		}

		/* is this IRQ pending and enabled? */
		if ((m_state == READY) && (m_irr & mask) && !(m_imr & mask))
		{
			LOG("pic8259_timerproc(): PIC triggering IR%d\n", irq);
			m_current_level = irq;
			m_out_int_func(1);
			return;
		}
		// if sfnm and in-service don't continue
		if((m_isr & mask) && m_master && m_cascade && m_nested && (m_slave & mask))
			break;
	}
	m_current_level = -1;
	m_out_int_func(0);
}

void pic8259_set_irq_line(int irq, int state)
{
	UINT8 mask = (1 << irq);

	if (state)
	{
		/* setting IRQ line */
		LOG("set_irq_line(): PIC set IR%d line\n", irq);

		if(m_level_trig_mode || (!m_level_trig_mode && !(m_irq_lines & mask)))
		{
			m_irr |= mask;
		}
		m_irq_lines |= mask;
	}
	else
	{
		/* clearing IRQ line */
		LOG("set_irq_line(): PIC cleared IR%d line\n", irq);

		m_irq_lines &= ~mask;
		m_irr &= ~mask;
	}

	if (m_inta_sequence == 0)
		irq_timer_tick();
}

static bool is_x86() { return m_is_x86; }

static UINT8 acknowledge()
{
	if (is_x86())
	{
		/* is this IRQ pending and enabled? */
		if (m_current_level != -1)
		{
			// fbneo port note: irq_timer_tick() originally happens on a timer
			// after this function returns, it sets m_current_level to -1.
			// Since we're calling it directly (instead of timer), we cache
			// m_current_level with a temporary variable to return the proper
			// value below...  -dink Oct. 30, 2022
			INT32 current_level_temp = m_current_level;

			UINT8 mask = 1 << m_current_level;

			{
				LOG("pic8259_acknowledge(): PIC acknowledge IR%d\n", m_current_level);
				if (!m_level_trig_mode)
					m_irr &= ~mask;

				if (!m_auto_eoi)
					m_isr |= mask;

				irq_timer_tick();
			}

			if ((m_cascade!=0) && (m_master!=0) && (mask & m_slave))
			{
				// it's from slave device
				return m_read_slave_ack_func(m_current_level);
			}
			else
			{
				/* For x86 mode*/
				//return m_current_level + m_base;
				return current_level_temp + m_base;
			}
		}
		else
		{
			logerror("Spurious INTA\n");
			return m_base + 7;
		}
	}
	else
	{
		/* in case of 8080/85 */
		if (m_inta_sequence == 0)
		{

			{
				if (m_current_level != -1)
				{
					LOG("pic8259_acknowledge(): PIC acknowledge IR%d\n", m_current_level);

					UINT8 mask = 1 << m_current_level;
					if (!m_level_trig_mode)
						m_irr &= ~mask;
					m_isr |= mask;
				}
				else
					logerror("Spurious INTA\n");
				m_inta_sequence = 1;
			}
			if (m_cascade && m_master && m_current_level != -1 && BIT(m_slave, m_current_level))
				return m_read_slave_ack_func(m_current_level);
			else
				return 0xcd;
		}
		else if (m_inta_sequence == 1)
		{
			m_inta_sequence = 2;
			if (m_cascade && m_master && m_current_level != -1 && BIT(m_slave, m_current_level))
				return m_read_slave_ack_func(m_current_level);
			else
				return m_vector_addr_low + ((m_current_level & 7) << (3-m_vector_size));
		}
		else
		{
			{
				m_inta_sequence = 0;
				if (m_auto_eoi && m_current_level != -1)
					m_isr &= ~(1 << m_current_level);
				irq_timer_tick();
			}
			if (m_cascade && m_master && m_current_level != -1 && BIT(m_slave, m_current_level))
				return m_read_slave_ack_func(m_current_level);
			else
				return m_vector_addr_high;
		}
	}
}


UINT8 pic8259_inta_cb()
{
	return acknowledge();
}


UINT8 pic8259_read(INT32 offset)
{
	/* NPW 18-May-2003 - Changing 0xFF to 0x00 as per Ruslan */
	UINT8 data = 0x00;

	switch(offset)
	{
		case 0: /* PIC acknowledge IRQ */
			if ( m_ocw3 & 0x04 )
			{
				/* Polling mode */
				if (m_current_level != -1)
				{
					data = 0x80 | m_current_level;

					if (!m_level_trig_mode)
						m_irr &= ~(1 << m_current_level);

					if (!m_auto_eoi)
						m_isr |= 1 << m_current_level;

					irq_timer_tick();
				}
			}
			else
			{
				switch ( m_ocw3 & 0x03 )
				{
				case 2:
					data = m_irr;
					break;
				case 3:
					data = m_isr & ~m_imr;
					break;
				default:
					data = 0x00;
					break;
				}
			}
			break;

		case 1: /* PIC mask register */
			data = m_imr;
			break;
	}
	return data;
}


void pic8259_write(INT32 offset, UINT8 data)
{
	switch(offset)
	{
		case 0:    /* PIC acknowledge IRQ */
			if (data & 0x10)
			{
				/* write ICW1 - this pretty much resets the chip */
				LOGICW("pic8259_device::write(): ICW1; data=0x%02X\n", data);

				m_imr                = 0x00;
				m_isr                = 0x00;
				m_irr                = 0x00;
				m_level_trig_mode    = (data & 0x08) ? 1 : 0;
				m_vector_size        = (data & 0x04) ? 1 : 0;
				m_cascade            = (data & 0x02) ? 0 : 1;
				m_icw4_needed        = (data & 0x01) ? 1 : 0;
				m_vector_addr_low    = (data & 0xe0);
				m_state              = ICW2;
				m_current_level      = -1;
				m_inta_sequence      = 0;
				m_out_int_func(0);
			}
			else if (m_state == READY)
			{
				if ((data & 0x98) == 0x08)
				{
					/* write OCW3 */
					LOGOCW("pic8259_device::write(): OCW3; data=0x%02X\n", data);

					m_ocw3 = data;
				}
				else if ((data & 0x18) == 0x00)
				{
					int n = data & 7;
					UINT8 mask = 1 << n;

					/* write OCW2 */
					LOGOCW("pic8259_device::write(): OCW2; data=0x%02X\n", data);

					switch (data & 0xe0)
					{
						case 0x00:
							m_prio = 0;
							break;
						case 0x20:
							for (n = 0, mask = 1<<m_prio; n < 8; n++, mask = (mask<<1) | (mask>>7))
							{
								if (m_isr & mask)
								{
									m_isr &= ~mask;
									break;
								}
							}
							break;
						case 0x40:
							break;
						case 0x60:
							if( m_isr & mask )
							{
								m_isr &= ~mask;
							}
							break;
						case 0x80:
							m_prio = (m_prio + 1) & 7;
							break;
						case 0xa0:
							for (n = 0, mask = 1<<m_prio; n < 8; n++, mask = (mask<<1) | (mask>>7))
							{
								if( m_isr & mask )
								{
									m_isr &= ~mask;
									m_prio = (m_prio + 1) & 7;
									break;
								}
							}
							break;
						case 0xc0:
							m_prio = (n + 1) & 7;
							break;
						case 0xe0:
							if( m_isr & mask )
							{
								m_isr &= ~mask;
								m_prio = (n + 1) & 7;
							}
							break;
					}
				}
			}
			break;

		case 1:
			switch(m_state)
			{
				case ICW1:
					break;

				case ICW2:
					/* write ICW2 */
					LOGICW("pic8259_device::write(): ICW2; data=0x%02X\n", data);

					m_base = data & 0xf8;
					m_vector_addr_high = data ;
					if (m_cascade)
					{
						m_state = ICW3;
					}
					else
					{
						m_state = m_icw4_needed ? ICW4 : READY;
					}
					break;

				case ICW3:
					/* write ICW3 */
					LOGICW("pic8259_device::write(): ICW3; data=0x%02X\n", data);

					m_slave = data;
					m_state = m_icw4_needed ? ICW4 : READY;
					break;

				case ICW4:
					/* write ICW4 */
					LOGICW("pic8259_device::write(): ICW4; data=0x%02X\n", data);

					m_nested = (data & 0x10) ? 1 : 0;
					m_mode = (data >> 2) & 3;
					m_auto_eoi = (data & 0x02) ? 1 : 0;
					m_is_x86 = (data & 0x01) ? 1 : 0;
					m_state = READY;
					break;

				case READY:
					/* write OCW1 - set interrupt mask register */
					LOGOCW("pic8259_device::write(): OCW1; data=0x%02X\n", data);

					//printf("%s %02x\n",m_master ? "master pic8259 mask" : "slave pic8259 mask",data);
					m_imr = data;
					break;
			}
			break;
	}
	irq_timer_tick();
}


#if 0
//-------------------------------------------------
//  device_resolve_objects - resolve objects that
//  may be needed for other devices to set
//  initial conditions at start time
//-------------------------------------------------

void pic8259_device::device_resolve_objects()
{
	// resolve callbacks
	m_out_int_func.resolve_safe();
	m_in_sp_func.resolve_safe(1);
	m_read_slave_ack_func.resolve_safe(0);
}
#endif

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

static UINT8 dummy_in_sp_func()
{
	return 1; // master
}

void pic8259_set_in_sp_func(UINT8 (*in_sp_func)())
{
	m_in_sp_func = in_sp_func;
}

void pic8259_set_slave_ack(UINT8 (*slave_ack_func)(UINT8))
{
	m_read_slave_ack_func = slave_ack_func;
}

void pic8259_init(void (*int_func)(INT32))
{
	m_out_int_func = int_func;
	m_read_slave_ack_func = NULL;
	m_in_sp_func = dummy_in_sp_func;

	pic8259_reset();
}

void pic8259_exit()
{
	m_out_int_func = NULL;
	m_read_slave_ack_func = NULL;
	m_in_sp_func = NULL;
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void pic8259_reset()
{
	m_state = READY;
	m_isr = 0;
	m_irr = 0;
	m_irq_lines = 0;
	m_prio = 0;
	m_imr = 0;
	m_input = 0;
	m_ocw3 = 2;
	m_level_trig_mode = 0;
	m_vector_size = 0;
	m_cascade = 0;
	m_icw4_needed = 0;
	m_base = 0;
	m_slave = 0;
	m_nested = 0;
	m_mode = 0;
	m_auto_eoi = 0;
	m_is_x86 = 1;
	m_vector_addr_low = 0;
	m_vector_addr_high = 0;
	m_current_level = -1;
	m_inta_sequence = 0;

	m_master = m_in_sp_func();
}
#if 0
DEFINE_DEVICE_TYPE(PIC8259, pic8259_device, "pic8259", "Intel 8259 PIC")
DEFINE_DEVICE_TYPE(V5X_ICU, v5x_icu_device, "v5x_icu", "NEC V5X ICU")
DEFINE_DEVICE_TYPE(MK98PIC, mk98pic_device, "mk98pic", "Elektronika MK-98 PIC")

pic8259_device::pic8259_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, UINT32 clock)
	: device_t(mconfig, type, tag, owner, clock)
	, m_out_int_func(*this)
	, m_in_sp_func(*this)
	, m_read_slave_ack_func(*this)
	, m_irr(0)
	, m_irq_lines(0)
	, m_level_trig_mode(0)
{
}

pic8259_device::pic8259_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: pic8259_device(mconfig, PIC8259, tag, owner, clock)
{
}

v5x_icu_device::v5x_icu_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: pic8259_device(mconfig, V5X_ICU, tag, owner, clock)
{
}

mk98pic_device::mk98pic_device(const machine_config &mconfig, const char *tag, device_t *owner, UINT32 clock)
	: pic8259_device(mconfig, MK98PIC, tag, owner, clock)
{
}
#endif
