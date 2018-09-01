// Sega USB sound core / cpu combo for FBAlpha, based on code by Aaron Giles,
// also contains large portions of code by Aaron Giles

#include "burnint.h"
#include "i8039.h"
#include "z80_intf.h"

static UINT8 out_latch;
static UINT8 in_latch;
static UINT8 t1_clock;
static UINT8 t1_clock_mask;
static UINT8 last_p2_value;
static UINT8 work_ram_bank;
static INT32 usb_cpu_disabled;

static UINT8 *usb_prgram;  //0x1000
static UINT8 *usb_workram; //0x0400

static INT16  *mixer_buffer; // re-sampler
static INT32 (*pCPUTotalCycles)() = NULL;
static UINT32  nDACCPUMHZ = 0;
static INT32   nCurrentPosition = 0;
static INT32   samples_frame = 0;

#define SPEECH_MASTER_CLOCK 3120000

#define USB_MASTER_CLOCK    6000000
#define USB_2MHZ_CLOCK      (USB_MASTER_CLOCK/3)
#define USB_PCS_CLOCK       (USB_2MHZ_CLOCK/2)
#define USB_GOS_CLOCK       (USB_2MHZ_CLOCK/16/4)
#define MM5837_CLOCK        100000

#define SAMPLE_RATE         (USB_2MHZ_CLOCK/8)

struct filter_state
{
	double              capval;             /* current capacitor value */
	double              exponent;           /* constant exponent */
};

struct timer8253_channel
{
	UINT8               holding;            /* holding until counts written? */
	UINT8               latchmode;          /* latching mode */
	UINT8               latchtoggle;        /* latching state */
	UINT8               clockmode;          /* clocking mode */
	UINT8               bcdmode;            /* BCD mode? */
	UINT8               output;             /* current output value */
	UINT8               lastgate;           /* previous gate value */
	UINT8               gate;               /* current gate value */
	UINT8               subcount;           /* subcount (2MHz clocks per input clock) */
	UINT16              count;              /* initial count */
	UINT16              remain;             /* current down counter value */
};

struct timer8253
{
	timer8253_channel   chan[3];            /* three channels' worth of information */
	double              env[3];             /* envelope value for each channel */
	filter_state        chan_filter[2];     /* filter states for the first two channels */
	filter_state        gate1;              /* first RC filter state */
	filter_state        gate2;              /* second RC filter state */
	UINT8               config;             /* configuration for this timer */
};

static timer8253           m_timer_group[3];       /* 3 groups of timers */
static UINT8               m_timer_mode[3];        /* mode control for each group */
static UINT32              m_noise_shift;
static UINT8               m_noise_state;
static UINT8               m_noise_subcount;
static double              m_gate_rc1_exp[2];
static double              m_gate_rc2_exp[2];
static filter_state        m_final_filter;
static filter_state        m_noise_filters[5];

static void segausb_update_int(INT16 *outputs, int samples);

// Streambuffer handling
static INT32 SyncInternal()
{
	return (INT32)(float)(samples_frame * (pCPUTotalCycles() / (nDACCPUMHZ / (nBurnFPS / 100.0000))));
}

static void UpdateStream(INT32 length)
{
	if (length > samples_frame) length = samples_frame;

	length -= nCurrentPosition;
	if (length <= 0) return;

	INT16 *lbuf = mixer_buffer + nCurrentPosition;

	segausb_update_int(lbuf, length);

	nCurrentPosition += length;
}

INLINE void configure_filter(filter_state *state, double r, double c)
{
	state->capval = 0;
	state->exponent = 1.0 - exp(-1.0 / (r * c * SAMPLE_RATE));
}

INLINE double step_rc_filter(filter_state *state, double input)
{
	state->capval += (input - state->capval) * state->exponent;
	return state->capval;
}

INLINE double step_cr_filter(filter_state *state, double input)
{
	double result = (input - state->capval);
	state->capval += (input - state->capval) * state->exponent;
	return result;
}

INLINE void clock_channel(timer8253_channel *ch)
{
	UINT8 lastgate = ch->lastgate;

	/* update the gate */
	ch->lastgate = ch->gate;

	/* if we're holding, skip */
	if (ch->holding)
		return;

	/* switch off the clock mode */
	switch (ch->clockmode)
	{
		/* oneshot; waits for trigger to restart */
		case 1:
			if (!lastgate && ch->gate)
			{
				ch->output = 0;
				ch->remain = ch->count;
			}
			else
			{
				if (--ch->remain == 0)
					ch->output = 1;
			}
			break;

		/* square wave: counts down by 2 and toggles output */
		case 3:
			ch->remain = (ch->remain - 1) & ~1;
			if (ch->remain == 0)
			{
				ch->output ^= 1;
				ch->remain = ch->count;
			}
			break;
	}
}


/*************************************
 *
 *  USB timer and envelope controls
 *
 *************************************/

static void timer_w(int which, UINT8 offset, UINT8 data)
{
	timer8253 *g = &m_timer_group[which];
	timer8253_channel *ch;
	int was_holding;

	UpdateStream(SyncInternal());

	/* switch off the offset */
	switch (offset)
	{
		case 0:
		case 1:
		case 2:
			ch = &g->chan[offset];
			was_holding = ch->holding;

			/* based on the latching mode */
			switch (ch->latchmode)
			{
				case 1: /* low word only */
					ch->count = data;
					ch->holding = 0;
					break;

				case 2: /* high word only */
					ch->count = data << 8;
					ch->holding = 0;
					break;

				case 3: /* low word followed by high word */
					if (ch->latchtoggle == 0)
					{
						ch->count = (ch->count & 0xff00) | (data & 0x00ff);
						ch->latchtoggle = 1;
					}
					else
					{
						ch->count = (ch->count & 0x00ff) | (data << 8);
						ch->holding = 0;
						ch->latchtoggle = 0;
					}
					break;
			}

			/* if we're not holding, load the initial count for some modes */
			if (was_holding && !ch->holding)
				ch->remain = 1;
			break;

		case 3:
			/* break out the components */
			if (((data & 0xc0) >> 6) < 3)
			{
				ch = &g->chan[(data & 0xc0) >> 6];

				/* extract the bits */
				ch->holding = 1;
				ch->latchmode = (data >> 4) & 3;
				ch->clockmode = (data >> 1) & 7;
				ch->bcdmode = (data >> 0) & 1;
				ch->latchtoggle = 0;
				ch->output = (ch->clockmode == 1);
			}
			break;
	}
}


static void env_w(int which, UINT8 offset, UINT8 data)
{
	timer8253 *g = &m_timer_group[which];

	UpdateStream(SyncInternal());

	if (offset < 3)
		g->env[offset] = (double)data;
	else
		g->config = data & 1;
}

//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

static void segausb_update_int(INT16 *outputs, int samples)
{
	INT16 *dest = outputs;

	/* iterate over samples */
	while (samples--)
	{
		double noiseval;
		double sample = 0;
		int group;
		int step;


		/*----------------
		    Noise Source
		  ----------------

		                 RC
		   MM5837 ---> FILTER ---> CR FILTER ---> 3.2x AMP ---> NOISE
		               LADDER
		*/

		/* update the noise source */
		for (step = USB_2MHZ_CLOCK / SAMPLE_RATE; step >= m_noise_subcount; step -= m_noise_subcount)
		{
			m_noise_shift = (m_noise_shift << 1) | (((m_noise_shift >> 13) ^ (m_noise_shift >> 16)) & 1);
			m_noise_state = (m_noise_shift >> 16) & 1;
			m_noise_subcount = USB_2MHZ_CLOCK / MM5837_CLOCK;
		}
		m_noise_subcount -= step;

		/* update the filtered noise value -- this is just an approximation to the pink noise filter */
		/* being applied on the PCB, but it sounds pretty close */
		m_noise_filters[0].capval = 0.99765 * m_noise_filters[0].capval + m_noise_state * 0.0990460;
		m_noise_filters[1].capval = 0.96300 * m_noise_filters[1].capval + m_noise_state * 0.2965164;
		m_noise_filters[2].capval = 0.57000 * m_noise_filters[2].capval + m_noise_state * 1.0526913;
		noiseval = m_noise_filters[0].capval + m_noise_filters[1].capval + m_noise_filters[2].capval + m_noise_state * 0.1848;

		/* final output goes through a CR filter; the scaling factor is arbitrary to get the noise to the */
		/* correct relative volume */
		noiseval = step_cr_filter(&m_noise_filters[4], noiseval);
		noiseval *= 0.075;

		/* there are 3 identical groups of circuits, each with its own 8253 */
		for (group = 0; group < 3; group++)
		{
			timer8253 *g = &m_timer_group[group];
			double chan0, chan1, chan2, mix;


			/*-------------
			    Channel 0
			  -------------

			    8253        CR                   AD7524
			    OUT0 ---> FILTER ---> BUFFER--->  VRef  ---> 100k ---> mix
			*/

			/* channel 0 clocks with the PCS clock */
			for (step = USB_2MHZ_CLOCK / SAMPLE_RATE; step >= g->chan[0].subcount; step -= g->chan[0].subcount)
			{
				g->chan[0].subcount = USB_2MHZ_CLOCK / USB_PCS_CLOCK;
				g->chan[0].gate = 1;
				clock_channel(&g->chan[0]);
			}
			g->chan[0].subcount -= step;

			/* channel 0 is mixed in with a resistance of 100k */
			chan0 = step_cr_filter(&g->chan_filter[0], g->chan[0].output) * g->env[0] * (1.0/100.0);


			/*-------------
			    Channel 1
			  -------------

			    8253        CR                   AD7524
			    OUT1 ---> FILTER ---> BUFFER--->  VRef  ---> 100k ---> mix
			*/

			/* channel 1 clocks with the PCS clock */
			for (step = USB_2MHZ_CLOCK / SAMPLE_RATE; step >= g->chan[1].subcount; step -= g->chan[1].subcount)
			{
				g->chan[1].subcount = USB_2MHZ_CLOCK / USB_PCS_CLOCK;
				g->chan[1].gate = 1;
				clock_channel(&g->chan[1]);
			}
			g->chan[1].subcount -= step;

			/* channel 1 is mixed in with a resistance of 100k */
			chan1 = step_cr_filter(&g->chan_filter[1], g->chan[1].output) * g->env[1] * (1.0/100.0);


			/*-------------
			    Channel 2
			  -------------

			  If timer_mode == 0:

			               SWITCHED                                  AD7524
			    NOISE --->    RC   ---> 1.56x AMP ---> INVERTER --->  VRef ---> 33k ---> mix
			                FILTERS

			  If timer mode == 1:

			                             AD7524                                    SWITCHED
			    NOISE ---> INVERTER --->  VRef ---> 33k ---> mix ---> INVERTER --->   RC   ---> 1.56x AMP ---> finalmix
			                                                                        FILTERS
			*/

			/* channel 2 clocks with the 2MHZ clock and triggers with the GOS clock */
			for (step = 0; step < USB_2MHZ_CLOCK / SAMPLE_RATE; step++)
			{
				if (g->chan[2].subcount-- == 0)
				{
					g->chan[2].subcount = USB_2MHZ_CLOCK / USB_GOS_CLOCK / 2 - 1;
					g->chan[2].gate = !g->chan[2].gate;
				}
				clock_channel(&g->chan[2]);
			}

			/* the exponents for the gate filters are determined by channel 2's output */
			g->gate1.exponent = m_gate_rc1_exp[g->chan[2].output];
			g->gate2.exponent = m_gate_rc2_exp[g->chan[2].output];

			/* based on the envelope mode, we do one of two things with source 2 */
			if (g->config == 0)
			{
				chan2 = step_rc_filter(&g->gate2, step_rc_filter(&g->gate1, noiseval)) * -1.56 * g->env[2] * (1.0/33.0);
				mix = chan0 + chan1 + chan2;
			}
			else
			{
				chan2 = -noiseval * g->env[2] * (1.0/33.0);
				mix = chan0 + chan1 + chan2;
				mix = step_rc_filter(&g->gate2, step_rc_filter(&g->gate1, -mix)) * 1.56;
			}

			/* accumulate the sample */
			sample += (mix * 0.75);
		}


		/*-------------
		    Final mix
		  -------------

		  INPUTS
		  EQUAL ---> 1.2x INVERTER ---> CR FILTER ---> out
		  WEIGHT

		*/
		*dest++ = BURN_SND_CLIP(4000 * step_cr_filter(&m_final_filter, sample));
	}
}

void segausb_update(INT16 *outputs, INT32 sample_len)
{
	if (sample_len != nBurnSoundLen) {
		bprintf(PRINT_ERROR, _T("*** segausb_update(): call once per frame!\n"));
		return;
	}

	samples_frame = (INT32)((double)((SAMPLE_RATE * 100) / nBurnFPS) + 0.5);

	UpdateStream(samples_frame);

	for (INT32 j = 0; j < sample_len; j++)
	{
		INT32 k = (samples_frame * j) / nBurnSoundLen;

		INT32 rlmono = mixer_buffer[k];

		outputs[0] = BURN_SND_CLIP(outputs[0] + BURN_SND_CLIP(rlmono));
		outputs[1] = BURN_SND_CLIP(outputs[1] + BURN_SND_CLIP(rlmono));
		outputs += 2;
	}

	memset(mixer_buffer, 0, samples_frame * sizeof(INT16));
	nCurrentPosition = 0;
}


//----------------------------------------------------------------------------
// use in z80 map

UINT8 usb_sound_status_read()
{
	ZetIdle(200);

	return (out_latch & 0x81) | (in_latch & 0x7e);
}

void usb_sound_data_write(UINT8 data)
{
	if (data & 0x80)
	{
		I8039Open(1);
		I8039Reset();
		I8039Close();
	}
	usb_cpu_disabled = data >> 7;

	if ((last_p2_value & 0x40) == 0)
		data &= ~0x7f;

	in_latch = data;
}

UINT8 usb_sound_prgram_read(UINT16 offset)
{
	return usb_prgram[offset & 0xfff];
}

void usb_sound_prgram_write(UINT16 offset, UINT8 data)
{
	if (in_latch & 0x80) {
		usb_prgram[offset & 0xfff] = data;
	}
}

//----------------------------------------------------------------------------
// i8035 

static void workram_write(UINT16 offset, UINT8 data)
{
	switch (offset & ~3)
	{
		case 0x00:
			timer_w(0, offset & 3, data);
			break;

		case 0x04:
			env_w(0, offset & 3, data);
			break;

		case 0x08:
			timer_w(1, offset & 3, data);
			break;

		case 0x0c:
			env_w(1, offset & 3, data);
			break;

		case 0x10:
			timer_w(2, offset & 3, data);
			break;

		case 0x14:
			env_w(2, offset & 3, data);
			break;
	}
}

static UINT8 __fastcall sega_usb_sound_read(UINT32 address)
{
	return usb_prgram[address & 0xfff];
}

static void __fastcall sega_usb_sound_write_port(UINT32 port, UINT8 data)
{
	if (port < 0x100) {
		usb_workram[work_ram_bank * 0x100 + port] = data;
		workram_write(work_ram_bank * 0x100 + port, data);
		return;
	}

	switch (port & 0x1ff)
	{
		case I8039_p1:
			out_latch = (out_latch & 0xfe) | (data >> 7);
		return;

		case I8039_p2:
		{
			UINT8 old = last_p2_value;
			last_p2_value = data;

			work_ram_bank = data & 3;

			out_latch = ((data & 0x40) << 1) | (out_latch & 0x7f);
			if ((data & 0x40) == 0)
				in_latch = 0;

			if ((old & 0x80) && !(data & 0x80))
				t1_clock = 0;
		}
		return;
	}
}

static UINT8 __fastcall sega_usb_sound_read_port(UINT32 port)
{
	if (port < 0x100) {
		return usb_workram[work_ram_bank * 0x100 + port];
	}

	switch (port & 0x1ff)
	{
		case I8039_p1:
			return in_latch & 0x7f;

		case I8039_t1:
			return (t1_clock & t1_clock_mask) != 0;;
	}

	return 0;
}

static void usb_sound_init_filters()
{
	filter_state temp;

	m_noise_shift = 0x15555;
	m_noise_state = 0;
	m_noise_subcount = 0;
	memset(&m_timer_group, 0, sizeof(m_timer_group));
	memset(&m_timer_mode, 0, sizeof(m_timer_mode));
	memset(&m_gate_rc1_exp, 0, sizeof(m_gate_rc1_exp));
	memset(&m_gate_rc2_exp, 0, sizeof(m_gate_rc2_exp));
	memset(&m_final_filter, 0, sizeof(m_final_filter));
	memset(&m_noise_filters, 0, sizeof(m_noise_filters));
	memset(&temp, 0, sizeof(temp));

	for (INT32 tgroup = 0; tgroup < 3; tgroup++)
	{
		timer8253 *g = &m_timer_group[tgroup];
		configure_filter(&g->chan_filter[0], 10e3, 1e-6);
		configure_filter(&g->chan_filter[1], 10e3, 1e-6);
		configure_filter(&g->gate1, 100e3, 0.01e-6);
		configure_filter(&g->gate2, 2 * 100e3, 0.01e-6);
	}

	configure_filter(&temp, 100e3, 0.01e-6);
	m_gate_rc1_exp[0] = temp.exponent;
	configure_filter(&temp, 1e3, 0.01e-6);
	m_gate_rc1_exp[1] = temp.exponent;
	configure_filter(&temp, 2 * 100e3, 0.01e-6);
	m_gate_rc2_exp[0] = temp.exponent;
	configure_filter(&temp, 2 * 1e3, 0.01e-6);
	m_gate_rc2_exp[1] = temp.exponent;

	configure_filter(&m_noise_filters[0], 2.7e3 + 2.7e3, 1.0e-6);
	configure_filter(&m_noise_filters[1], 2.7e3 + 1e3, 0.30e-6);
	configure_filter(&m_noise_filters[2], 2.7e3 + 270, 0.15e-6);
	configure_filter(&m_noise_filters[3], 2.7e3 + 0, 0.082e-6);
	configure_filter(&m_noise_filters[4], 33e3, 0.1e-6);

	configure_filter(&m_final_filter, 100e3, 4.7e-6);
}

void usb_sound_reset()
{
	I8039Open(1);
	I8039Reset();
	I8039Close();

	out_latch = 0;
	in_latch = 0;
	t1_clock = 0;
	t1_clock_mask = 0x10;
	last_p2_value = 0;
	work_ram_bank = 0;
	usb_cpu_disabled = 1;
	memset (usb_prgram, 0, 0x1000);
	memset (usb_workram, 0, 0x400);
	samples_frame = (INT32)((double)((SAMPLE_RATE * 100) / nBurnFPS) + 0.5);

	usb_sound_init_filters();
}

void usb_sound_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ)
{
	I8035Init(1);
	I8039Open(1);
	I8039SetProgramReadHandler(sega_usb_sound_read);
	I8039SetCPUOpReadHandler(sega_usb_sound_read);
	I8039SetCPUOpReadArgHandler(sega_usb_sound_read);
	I8039SetIOReadHandler(sega_usb_sound_read_port);
	I8039SetIOWriteHandler(sega_usb_sound_write_port);
	I8039Close();

	usb_prgram = (UINT8*)BurnMalloc(0x1000 * sizeof(UINT8));
	usb_workram = (UINT8*)BurnMalloc(0x400 * sizeof(UINT8));

	mixer_buffer = (INT16*)BurnMalloc(2 * sizeof(INT16) * SAMPLE_RATE);
	pCPUTotalCycles = pCPUCyclesCB;
	nDACCPUMHZ = nCpuMHZ;
}

void usb_sound_exit()
{
	BurnFree(mixer_buffer);
	BurnFree(usb_prgram);
	BurnFree(usb_workram);
}

void usb_sound_scan(INT32 nAction, INT32 *)
{
	if (nAction & ACB_VOLATILE) {
		struct BurnArea ba;

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = usb_prgram;
		ba.nLen	  = 0x1000;
		ba.szName = "usb prgram";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = usb_workram;
		ba.nLen	  = 0x0400;
		ba.szName = "usb workram";
		BurnAcb(&ba);

		SCAN_VAR(out_latch);
		SCAN_VAR(in_latch);
		SCAN_VAR(t1_clock);
		SCAN_VAR(t1_clock_mask);
		SCAN_VAR(last_p2_value);
		SCAN_VAR(work_ram_bank);
		SCAN_VAR(usb_cpu_disabled);

		SCAN_VAR(m_timer_group);
		SCAN_VAR(m_timer_mode);
		SCAN_VAR(m_noise_shift);
		SCAN_VAR(m_noise_state);
		SCAN_VAR(m_noise_subcount);
		SCAN_VAR(m_gate_rc1_exp);
		SCAN_VAR(m_gate_rc2_exp);
		SCAN_VAR(m_final_filter);
		SCAN_VAR(m_noise_filters);
	}

	if (nAction & ACB_WRITE) {
		memset(mixer_buffer, 0, samples_frame * sizeof(INT16));
		nCurrentPosition = 0;
	}
}

void usb_timer_t1_clock() // 7812.5 per sec / 195.3125 per frame (40hz)
{
	if ((last_p2_value & 0x80) == 0)
		t1_clock++;
}

INT32 usb_sound_run(INT32 cycles)
{
	if (usb_cpu_disabled)
		return cycles;

	I8039Open(1);
	INT32 ret = I8039Run(cycles);
	I8039Close();

	return ret;
}
