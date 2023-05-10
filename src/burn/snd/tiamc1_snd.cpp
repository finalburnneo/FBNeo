// license:BSD-3-Clause
// copyright-holders:Eugene Sandulenko
/***************************************************************************

  TIA-MC1 sound hardware

  driver by Eugene Sandulenko
  special thanks to Shiru for his standalone emulator and documentation

            timer1       timer0
          |--------|   |--------|
          |  8253  |   |  8253  |   |---|
      in0 |        |   |        |   | 1 |   |---|
      ----+ g0  o0 +---+ g0  o0 +---+   |   | & |
      in1 |        |   |        |   |   o---+   |  out
      ----+ g1  o1 +---+ g1  o1 +---+   |   |   +-----
      in2 |        |   |        |   |---|   |   |
      ----+ g2  o2 +---+ g2  o2 +-----------+   |
      clk1|        |   |        |           |---|
      ----+ clk    | +-+ clk    |
          |        | | |        |
          |--------| | |--------|
      clk0           |
      ---------------+

  in0-in2 comes from port #da
  clk0    comes from 8224 and equals to 1777777Hz, i.e. processor clock
  clk1    comes from divider 16000000/4/16/16/2 = 7812Hz

  ********************

  TODO: use machine/pit8253.c

***************************************************************************/

#include "burnint.h"
#include "tiamc1_snd.h"
#include "stream.h"

#define CLOCK_DIVIDER 16
#define BUF_LEN 100000

#define T8253_CHAN0     0
#define T8253_CHAN1     1
#define T8253_CHAN2     2
#define T8253_CWORD     3

#if 0
// device type definition
DEFINE_DEVICE_TYPE(TIAMC1, tiamc1_sound_device, "tiamc1_sound", "TIA-MC1 Custom Sound")


//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  tiamc1_sound_device - constructor
//-------------------------------------------------

tiamc1_sound_device::tiamc1_sound_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, TIAMC1, tag, owner, clock),
		device_sound_interface(mconfig, *this),
		m_channel(nullptr),
		m_timer1_divider(0)
{
}
#endif

struct timer8253chan
{
	UINT16 count;
	UINT16 cnval;
	UINT8 bcdMode;
	UINT8 cntMode;
	UINT8 valMode;
	UINT8 gate;
	UINT8 output;
	UINT8 loadCnt;
	UINT8 enable;
};

struct timer8253struct
{
	struct timer8253chan channel[3];
};


	static void timer8253_reset(struct timer8253struct *t);
	static void timer8253_tick(struct timer8253struct *t,int chn);
	static void timer8253_wr(struct timer8253struct *t, int reg, UINT8 val);
	static char timer8253_get_output(struct timer8253struct *t, int chn);
	static void timer8253_set_gate(struct timer8253struct *t, int chn, UINT8 gate);

	static Stream stream;
	static int m_timer1_divider;

	static timer8253struct m_timer0;
	static timer8253struct m_timer1;

	static INT32 kot_mode;

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------
static void sound_stream_update(INT16 **streams, INT32 length);

void tiamc1_sound_reset()
{
	timer8253_reset(&m_timer0);
	timer8253_reset(&m_timer1);

	m_timer1_divider = 0;
}

void tiamc1_sound_init()
{
	stream.init(15750000/9 / CLOCK_DIVIDER, nBurnSoundRate, 1, 0, sound_stream_update);

	tiamc1_sound_reset();

	kot_mode = 0;
}

void tiamc1_sound_init_kot()
{
	tiamc1_sound_init();

	kot_mode = 1;
}

void tiamc1_sound_exit()
{
	stream.exit();
}

void tiamc1_sound_update(INT16 *output, INT32 samples_len)
{
	if (samples_len != nBurnSoundLen) {
		bprintf(0, _T("tiamc1_sound_update(): once per frame, please!\n"));
		return;
	}

	stream.render(output, samples_len);
}

void tiamc1_sound_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(m_timer0);
		SCAN_VAR(m_timer1);
		SCAN_VAR(m_timer1_divider);
	}
}

//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

static void sound_stream_update(INT16 **streams, INT32 length)
{
	int count, o0, o1, o2, len, orval = 0;

	len = length * CLOCK_DIVIDER;

	for (count = 0; count < len; count++)
	{
		m_timer1_divider++;
		if (m_timer1_divider == 228)
		{
			m_timer1_divider = 0;
			timer8253_tick(&m_timer1, 0);
			timer8253_tick(&m_timer1, 1);
			timer8253_tick(&m_timer1, 2);

			timer8253_set_gate(&m_timer0, 0, timer8253_get_output(&m_timer1, 0));
			timer8253_set_gate(&m_timer0, 1, timer8253_get_output(&m_timer1, 1));
			timer8253_set_gate(&m_timer0, 2, timer8253_get_output(&m_timer1, 2));
		}

		if (kot_mode) // myow!
		{
			timer8253_set_gate(&m_timer0, 2, 1);
		}

		timer8253_tick(&m_timer0, 0);
		timer8253_tick(&m_timer0, 1);
		timer8253_tick(&m_timer0, 2);

		o0 = timer8253_get_output(&m_timer0, 0) ? 1 : 0;
		o1 = timer8253_get_output(&m_timer0, 1) ? 1 : 0;
		o2 = timer8253_get_output(&m_timer0, 2) ? 1 : 0;

		orval = (orval << 1) | (((o0 | o1) ^ 0xff) & o2);

		if ((count + 1) % CLOCK_DIVIDER == 0)
		{
			streams[0][count / CLOCK_DIVIDER] = orval ? 0x2828 : 0;
			orval = 0;
		}
	}
}


static void timer8253_reset(struct timer8253struct *t)
{
	memset(t,0,sizeof(struct timer8253struct));
}


static void timer8253_tick(struct timer8253struct *t, int chn)
{
	if (t->channel[chn].enable && t->channel[chn].gate)
	{
		switch (t->channel[chn].cntMode)
		{
		case 0:
			t->channel[chn].count--;
			if (t->channel[chn].count == 0xffff)
				t->channel[chn].output = 1;
			break;

		case 3:
			t->channel[chn].count--;

			if (t->channel[chn].count < (t->channel[chn].cnval >> 1))
				t->channel[chn].output = 0;
			else
				t->channel[chn].output = 1;

			if (t->channel[chn].count == 0xffff)
				t->channel[chn].count = t->channel[chn].cnval;
			break;

		case 4:
			t->channel[chn].count--;
			if(t->channel[chn].count==0)
				t->channel[chn].output = 1;

			if(t->channel[chn].count == 0xffff)
			{
				t->channel[chn].enable = 0;
				t->channel[chn].output = 1;
			}
			break;
		}
	}
}



static void timer8253_wr(struct timer8253struct *t, int reg, UINT8 val)
{
	int chn;

	switch (reg)
	{
	case T8253_CWORD:
		chn = val >> 6;
		if (chn < 3)
		{
			t->channel[chn].bcdMode = (val & 1) ? 1 : 0;
			t->channel[chn].cntMode = (val >> 1) & 0x07;
			t->channel[chn].valMode = (val >> 4) & 0x03;

			switch (t->channel[chn].valMode)
			{
			case 1:
			case 2:
				t->channel[chn].loadCnt = 1;
				break;

			case 3:
				t->channel[chn].loadCnt = 2;
				break;
			}

			switch (t->channel[chn].cntMode)
			{
			case 0:
				t->channel[chn].output = 0;
				t->channel[chn].enable = 0;
				break;

			case 3:
				t->channel[chn].output = 1;
				break;

			case 4:
				t->channel[chn].output = 1;
				t->channel[chn].enable = 0;
				break;
			}
		}
		break;

	default:
		chn = reg;

		switch (t->channel[chn].valMode)
		{
		case 1:
			t->channel[chn].cnval = (t->channel[chn].cnval & 0xff00) | val;
			break;
		case 2:
			t->channel[chn].cnval = (t->channel[chn].cnval & 0x00ff) | (val << 8);
			break;
		case 3:
			t->channel[chn].cnval = (t->channel[chn].cnval >> 8) | (val << 8);
			break;
		}

		if (t->channel[chn].cntMode==0)
		{
			t->channel[chn].enable = 0;
		}

		t->channel[chn].loadCnt--;

		if (t->channel[chn].loadCnt == 0)
		{
			switch (t->channel[chn].valMode)
			{
			case 1:
			case 2:
				t->channel[chn].loadCnt = 1;
				break;

			case 3:
				t->channel[chn].loadCnt = 2;
				break;
			}

			switch (t->channel[chn].cntMode)
			{
			case 3:
				t->channel[chn].count = t->channel[chn].cnval;
				t->channel[chn].enable = 1;
				break;

			case 0:
			case 4:
				t->channel[chn].count = t->channel[chn].cnval;
				t->channel[chn].enable = 1;
				break;
			}
		}
	}
}

static void timer8253_set_gate(struct timer8253struct *t, int chn, UINT8 gate)
{
	t->channel[chn].gate = gate;
}



static char timer8253_get_output(struct timer8253struct *t, int chn)
{
	return t->channel[chn].output;
}

void tiamc1_sound_timer0_write(INT32 offset, UINT8 data)
{
	stream.update();

	timer8253_wr(&m_timer0, offset, data);
}

void tiamc1_sound_timer1_write(INT32 offset, UINT8 data)
{
	stream.update();

	timer8253_wr(&m_timer1, offset, data);
}

void tiamc1_sound_gate_write(UINT8 data)
{
	stream.update();

	timer8253_set_gate(&m_timer1, 0, (data & 1) ? 1 : 0);
	timer8253_set_gate(&m_timer1, 1, (data & 2) ? 1 : 0);
	timer8253_set_gate(&m_timer1, 2, (data & 4) ? 1 : 0);
}
