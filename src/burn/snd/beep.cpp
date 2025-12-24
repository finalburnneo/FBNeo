// license:BSD-3-Clause
// copyright-holders:Kevin Thacker
/***************************************************************************

    Simple beeper sound driver

    This is used for computers/systems which can only output a constant tone.
    This tone can be turned on and off.
    e.g. PCW and PCW16 computer systems

****************************************************************************/

#include "burnint.h"
#include "stream.h"

static Stream stream;

#define BEEP_RATE			48000

struct beep_state
{
	bool enable; 		// enable beep
	UINT32 frequency;	// set frequency - this can be changed using the appropriate function
	INT32 incr;			// initial wave state
	INT16 signal;		// current signal
};

static beep_state beepstates;

void beep_set_buffered(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz)
{
	stream.set_buffered(pCPUCyclesCB, nCPUMhz);
}

void beep_set_volume(double volume)
{
	stream.set_volume(volume);
}

void beep_set_state(bool on)
{
	stream.update();
	// only update if new state is not the same as old state
	if (beepstates.enable == on)
		return;

	beepstates.enable = on ? true : false;
	// restart wave from beginning
	beepstates.incr = 0;
	beepstates.signal = 0x07fff;
}

void beep_set_clock(UINT32 frequency)
{
	stream.update();
	if (beepstates.frequency == frequency)
		return;

	beepstates.frequency = frequency;
	beepstates.signal = 0x07fff;
	beepstates.incr = 0;
}


void beep_update(INT16 *output, INT32 samples_len)
{
	if (samples_len != nBurnSoundLen) {
		bprintf(0, _T("beep_update(): once per frame, please!\n"));
		return;
	}

	stream.render(output, samples_len);
}

void beep_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(beepstates);
	}
}

static void beep_stream_update(INT16 **streams, INT32 samples)
{
	INT16 *output = streams[0];

	memset(output, 0, samples * sizeof(INT16) * 2);

	// if we're not enabled, just fill with 0
	if ( !beepstates.enable || beepstates.frequency == 0 )
		return;

	// fill in the sample
	while( samples-- > 0 )
	{
		output[0] = beepstates.signal;
		output++;
		beepstates.incr -= beepstates.frequency;
		while ( beepstates.incr < 0 )
		{
			beepstates.incr += BEEP_RATE / 2;
			beepstates.signal = -beepstates.signal;
		}
	}
}

void beep_init(INT32 frequency)
{
	stream.init(48000, nBurnSoundRate, 1, 0, beep_stream_update);
    stream.set_volume(1.00);

	beepstates.enable = 0;
	beep_set_clock(frequency);
}

void beep_reset()
{
	beepstates.enable = 0;
	beepstates.signal = 0x07fff;
	beepstates.incr = 0;
}

void beep_exit()
{
	stream.exit();
}
