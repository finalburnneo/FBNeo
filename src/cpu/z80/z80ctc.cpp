/***************************************************************************

    Z80 CTC (Z8430) implementation

    based on original version (c) 1997, Tatsuyuki Satoh

    Copyright Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

***************************************************************************/

#include "burnint.h"
#include "z80ctc.h"
#include "z80.h"
#include "z80daisy.h"

/***************************************************************************
    DEBUGGING
***************************************************************************/

#define VERBOSE		0

#define VPRINTF(x)

/***************************************************************************
    CONSTANTS
***************************************************************************/

#define CLEAR_LINE	0
#define ASSERT_LINE	1

/* these are the bits of the incoming commands to the CTC */
#define INTERRUPT			0x80
#define INTERRUPT_ON 		0x80
#define INTERRUPT_OFF		0x00

#define MODE				0x40
#define MODE_TIMER			0x00
#define MODE_COUNTER		0x40

#define PRESCALER			0x20
#define PRESCALER_256		0x20
#define PRESCALER_16		0x00

#define EDGE				0x10
#define EDGE_FALLING		0x00
#define EDGE_RISING			0x10

#define TRIGGER				0x08
#define TRIGGER_AUTO		0x00
#define TRIGGER_CLOCK		0x08

#define CONSTANT			0x04
#define CONSTANT_LOAD		0x04
#define CONSTANT_NONE		0x00

#define RESET				0x02
#define RESET_CONTINUE		0x00
#define RESET_ACTIVE		0x02

#define CONTROL				0x01
#define CONTROL_VECTOR		0x00
#define CONTROL_WORD		0x01

/* these extra bits help us keep things accurate */
#define WAITING_FOR_TRIG	0x100


/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/

struct ctc_channel
{
	UINT8				notimer;			/* no timer masks */
	UINT16 				mode;				/* current mode */
	UINT16 				tconst;				/* time constant */
	UINT16 				down;				/* down counter (clock mode only) */
	UINT8 				extclk;				/* current signal from the external clock */
	UINT8				int_state;			/* interrupt status (for daisy chain) */
};

struct z80ctc
{
	// config stuff
	UINT32				clock;				/* system clock */
	INT32			    period16;			/* 16/system clock */
	INT32			    period256;			/* 256/system clock */

	// stuff to save
	UINT8				vector;				/* interrupt vector */
	ctc_channel			channel[4];			/* data for each channel */

	void                (*intr)(int which);	/* interrupt callback */
	void                (*zc[4])(int, UINT8); /* zero crossing callbacks */
};

static z80ctc *ctc = NULL;

static void timercallback(int param); // forward

// simple timer system -dink 2019
// note: all time is in z80-cycles relative to the cpu using this ctc.
#define TIMERS_MAX  4
struct timer_type
{
	INT32 running;
	INT32 time_trig;
	INT32 time_current;
	INT32 timer_param;
};

static void (*timer_exec[4])(int);

static timer_type timers[TIMERS_MAX];

void timer_reset()
{
	for (INT32 i = 0; i < TIMERS_MAX; i++)
	{
		timers[i].running = 0;
		timers[i].time_trig = 0;
		timers[i].time_current = 0;
		timer_exec[i] = NULL;
		timers[i].timer_param = 0;
	}
}

void timer_start(INT32 timernum, INT32 time, void (*callback)(INT32), INT32 tparam, INT32 running)
{
	if (timernum >= TIMERS_MAX) return;
	timers[timernum].running = running;
	timers[timernum].time_trig = time; // cycle to execute @
	timers[timernum].time_current = 0;
	timer_exec[timernum] = callback;
	timers[timernum].timer_param = tparam;
}

void timer_stop(INT32 timernum)
{
	if (timernum >= TIMERS_MAX) return;
	timers[timernum].running = 0;
	timers[timernum].time_current = 0;
}

INT32 timer_isrunning(INT32 timernum)
{
    if (timernum >= TIMERS_MAX) return 0;
    return timers[timernum].running;
}

INT32 timer_timeleft(INT32 timernum)
{
	if (timernum >= TIMERS_MAX) return 0;
	return timers[timernum].time_trig - timers[timernum].time_current; // maybe?
}

void z80ctc_timer_update(INT32 cycles)
{
	for (INT32 i = 0; i < TIMERS_MAX; i++)
	{
		if (timers[i].running) {
			timers[i].time_current += cycles;
			while (timers[i].time_current >= timers[i].time_trig) {
				timer_exec[i](timers[i].timer_param);
				timers[i].time_current -= timers[i].time_trig;
			}
		}
	}
}

static void z80ctc_timer_scan(INT32 nAction) // called from z80ctc_scan()! (below)
{
	SCAN_VAR(timers);

	if (nAction & ACB_WRITE) {
		// state load: re-set the timer callback pointer for running timers.
		for (INT32 i = 0; i < TIMERS_MAX; i++)
		{
			if (timers[i].running) {
				timer_exec[i] = timercallback;
			}
		}
	}
}


/***************************************************************************
    INTERNAL STATE MANAGEMENT
***************************************************************************/

static void interrupt_check()
{
	/* if we have a callback, update it with the current state */
	if (ctc->intr != NULL)
		(*ctc->intr)((z80ctc_irq_state() & Z80_DAISY_INT) ? ASSERT_LINE : CLEAR_LINE);
}


static void timercallback(int param)
{
	ctc_channel *channel = &ctc->channel[param];

	/* down counter has reached zero - see if we should interrupt */
	if ((channel->mode & INTERRUPT) == INTERRUPT_ON)
	{
		channel->int_state |= Z80_DAISY_INT;
		VPRINTF(("CTC timer ch%d\n", param));
		interrupt_check();
	}

	/* generate the clock pulse */
	if (ctc->zc[param] != NULL)
	{
		ctc->zc[param](0, 1);
		ctc->zc[param](0, 0);
	}

	/* reset the down counter */
	channel->down = channel->tconst;
}



/***************************************************************************
    INITIALIZATION/CONFIGURATION
***************************************************************************/

INT32 z80ctc_getperiod(int ch)
{
	
	ctc_channel *channel = &ctc->channel[ch];

	/* if reset active, no period */
	if ((channel->mode & RESET) == RESET_ACTIVE)
		return 0; //attotime_zero;

	/* if counter mode, no real period */
	if ((channel->mode & MODE) == MODE_COUNTER)
	{
		//logerror("CTC %d is CounterMode : Can't calculate period\n", ch );
		return 0; //attotime_zero;
	}

	/* compute the period */
	INT32 period = ((channel->mode & PRESCALER) == PRESCALER_16) ? ctc->period16 : ctc->period256;
	//return attotime_mul(period, channel->tconst);
	return period * channel->tconst;   // tih?-timmy?  (dink)
}



/***************************************************************************
    WRITE HANDLERS
***************************************************************************/

void z80ctc_write(int offset, UINT8 data)
{
	int ch = offset & 3;
	ctc_channel *channel = &ctc->channel[ch];
	int mode;

	/* get the current mode */
	mode = channel->mode;

	/* if we're waiting for a time constant, this is it */
	if ((mode & CONSTANT) == CONSTANT_LOAD)
	{
		VPRINTF(("CTC ch.%d constant = %02x\n", ch, data));

		/* set the time constant (0 -> 0x100) */
		channel->tconst = data ? data : 0x100;

		/* clear the internal mode -- we're no longer waiting */
		channel->mode &= ~CONSTANT;

		/* also clear the reset, since the constant gets it going again */
		channel->mode &= ~RESET;

		/* if we're in timer mode.... */
		if ((mode & MODE) == MODE_TIMER)
		{
			/* if we're triggering on the time constant, reset the down counter now */
			if ((mode & TRIGGER) == TRIGGER_AUTO)
			{
				if (!channel->notimer)
				{
					INT32 period = ((mode & PRESCALER) == PRESCALER_16) ? ctc->period16 : ctc->period256;
					period *= channel->tconst;
					//period = attotime_mul(period, channel->tconst);
					timer_start(ch, period, timercallback, ch, 1);
					//timer_adjust_periodic(channel->timer, period, ch, period);
				}
				else
				{
					timer_stop(ch); // off
					//timer_adjust_oneshot(channel->timer, attotime_never, 0);
				}
			}

			/* else set the bit indicating that we're waiting for the appropriate trigger */
			else
				channel->mode |= WAITING_FOR_TRIG;
		}

		/* also set the down counter in case we're clocking externally */
		channel->down = channel->tconst;

		/* all done here */
		return;
	}

	/* if we're writing the interrupt vector, handle it specially */
#if 0	/* Tatsuyuki Satoh changes */
	/* The 'Z80family handbook' wrote,                            */
	/* interrupt vector is able to set for even channel (0 or 2)  */
	if ((data & CONTROL) == CONTROL_VECTOR && (ch&1) == 0)
#else
	if ((data & CONTROL) == CONTROL_VECTOR && ch == 0)
#endif
	{
		ctc->vector = data & 0xf8;
		//logerror("CTC Vector = %02x\n", ctc->vector);
		return;
	}

	/* this must be a control word */
	if ((data & CONTROL) == CONTROL_WORD)
	{
		/* set the new mode */
		channel->mode = data;
		VPRINTF(("CTC ch.%d mode = %02x\n", ch, data));

		/* if we're being reset, clear out any pending timers for this channel */
		if ((data & RESET) == RESET_ACTIVE)
		{
			timer_stop(ch);
			//timer_adjust_oneshot(channel->timer, attotime_never, 0);
			/* note that we don't clear the interrupt state here! */
		}

		/* all done here */
		return;
	}
}



/***************************************************************************
    READ HANDLERS
***************************************************************************/

UINT8 z80ctc_read(int offset)
{
	int ch = offset & 3;
	ctc_channel *channel = &ctc->channel[ch];

	/* if we're in counter mode, just return the count */
	if ((channel->mode & MODE) == MODE_COUNTER || (channel->mode & WAITING_FOR_TRIG))
		return channel->down;

	/* else compute the down counter value */
	else
	{
		INT32 period = ((channel->mode & PRESCALER) == PRESCALER_16) ? ctc->period16 : ctc->period256;

		//VPRINTF(("CTC clock %f\n",ATTOSECONDS_TO_HZ(period.attoseconds)));

        if (timer_isrunning(ch))
            return ((int)timer_timeleft(ch) / period + 1) & 0xff;
        else
            return 0;
#if 0
		if (channel->timer != NULL)
			return ((int)(attotime_to_double(timer_timeleft(channel->timer)) * attotime_to_double(period)) + 1) & 0xff;
		else
			return 0;
#endif
	}
}



/***************************************************************************
    EXTERNAL TRIGGERS
***************************************************************************/

void z80ctc_trg_write(int ch, UINT8 data)
{
	ctc_channel *channel = &ctc->channel[ch];

	/* normalize data */
	data = data ? 1 : 0;

	/* see if the trigger value has changed */
	if (data != channel->extclk)
	{
		channel->extclk = data;

		/* see if this is the active edge of the trigger */
		if (((channel->mode & EDGE) == EDGE_RISING && data) || ((channel->mode & EDGE) == EDGE_FALLING && !data))
		{
			/* if we're waiting for a trigger, start the timer */
			if ((channel->mode & WAITING_FOR_TRIG) && (channel->mode & MODE) == MODE_TIMER)
			{
				if (!channel->notimer)
				{
					INT32 period = ((channel->mode & PRESCALER) == PRESCALER_16) ? ctc->period16 : ctc->period256;
					//period = attotime_mul(period, channel->tconst);
					period = period * channel->tconst;

					//VPRINTF(("CTC period %s\n", attotime_string(period, 9)));
					timer_start(ch, period, timercallback, ch, 1);
					//timer_adjust_periodic(channel->timer, period, ch, period);
				}
				else
				{
					VPRINTF(("CTC disabled\n"));
					timer_stop(ch);
					//timer_adjust_oneshot(channel->timer, attotime_never, 0);
				}
			}

			/* we're no longer waiting */
			channel->mode &= ~WAITING_FOR_TRIG;

			/* if we're clocking externally, decrement the count */
			if ((channel->mode & MODE) == MODE_COUNTER)
			{
				channel->down--;

				/* if we hit zero, do the same thing as for a timer interrupt */
				if (!channel->down)
				{
					timercallback(ch);
				}
			}
		}
	}
}


/***************************************************************************
    DAISY CHAIN INTERFACE
***************************************************************************/

int z80ctc_irq_state()
{
	int state = 0;
	int ch;

	VPRINTF(("CTC IRQ state = %d%d%d%d\n", ctc->channel[0].int_state, ctc->channel[1].int_state, ctc->channel[2].int_state, ctc->channel[3].int_state));

	/* loop over all channels */
	for (ch = 0; ch < 4; ch++)
	{
		ctc_channel *channel = &ctc->channel[ch];

		/* if we're servicing a request, don't indicate more interrupts */
		if (channel->int_state & Z80_DAISY_IEO)
		{
			state |= Z80_DAISY_IEO;
			break;
		}
		state |= channel->int_state;
	}

	return state;
}


int z80ctc_irq_ack()
{
	int ch;

	/* loop over all channels */
	for (ch = 0; ch < 4; ch++)
	{
		ctc_channel *channel = &ctc->channel[ch];

		/* find the first channel with an interrupt requested */
		if (channel->int_state & Z80_DAISY_INT)
		{
			VPRINTF(("CTC IRQAck ch%d\n", ch));

			/* clear interrupt, switch to the IEO state, and update the IRQs */
			channel->int_state = Z80_DAISY_IEO;
			interrupt_check();
			return ctc->vector + ch * 2;
		}
	}

	//logerror("z80ctc_irq_ack: failed to find an interrupt to ack!\n");
	return ctc->vector;
}


void z80ctc_irq_reti()
{
	int ch;

	/* loop over all channels */
	for (ch = 0; ch < 4; ch++)
	{
		ctc_channel *channel = &ctc->channel[ch];

		/* find the first channel with an IEO pending */
		if (channel->int_state & Z80_DAISY_IEO)
		{
			VPRINTF(("CTC IRQReti ch%d\n", ch));

			/* clear the IEO state and update the IRQs */
			channel->int_state &= ~Z80_DAISY_IEO;
			interrupt_check();
			return;
		}
	}

	//logerror("z80ctc_irq_reti: failed to find an interrupt to clear IEO on!\n");
}

void z80ctc_init(INT32 clock, INT32 notimer, void (*intr)(int), void (*zc0)(int, UINT8), void (*zc1)(int, UINT8), void (*zc2)(int, UINT8))
{
	ctc = (z80ctc *)BurnMalloc(sizeof(z80ctc));
	ctc->clock = clock;
	ctc->period16 = 16;
	ctc->period256 = 256;
	//ctc->period16 = attotime_mul(ATTOTIME_IN_HZ(ctc->clock), 16);
	//ctc->period256 = attotime_mul(ATTOTIME_IN_HZ(ctc->clock), 256);
	for (int ch = 0; ch < 4; ch++)
	{
		ctc_channel *channel = &ctc->channel[ch];
		channel->notimer = (notimer >> ch) & 1;
		//channel->timer = timer_alloc(timercallback, ptr);
	}
	ctc->intr = intr;
	ctc->zc[0] = zc0;
	ctc->zc[1] = zc1;
	ctc->zc[2] = zc2;
	ctc->zc[3] = NULL;
}

void z80ctc_exit()
{
	BurnFree(ctc);
}

void z80ctc_reset()
{
	int ch;

	/* set up defaults */
	for (ch = 0; ch < 4; ch++)
	{
		ctc_channel *channel = &ctc->channel[ch];
		channel->mode = RESET_ACTIVE;
		channel->tconst = 0x100;
		//timer_adjust_oneshot(channel->timer, attotime_never, 0);
		channel->int_state = 0;
	}
	interrupt_check();

	timer_reset();

	VPRINTF(("CTC Reset\n"));
}

void z80ctc_scan(INT32 nAction)
{
	SCAN_VAR(ctc->vector);
	SCAN_VAR(ctc->channel);

	z80ctc_timer_scan(nAction);
}

