/***************************************************************************

    Konami 005289 - SCC sound as used in Bubblesystem

    This file is pieced together by Bryan McPhail from a combination of
    Namco Sound, Amuse by Cab, Nemesis schematics and whoever first
    figured out SCC!

    The 005289 is a 2 channel sound generator. Each channel gets its
    waveform from a prom (4 bits wide).

    (From Nemesis schematics)

    Address lines A0-A4 of the prom run to the 005289, giving 32 bytes
    per waveform.  Address lines A5-A7 of the prom run to PA5-PA7 of
    the AY8910 control port A, giving 8 different waveforms. PA0-PA3
    of the AY8910 control volume.

    The second channel is the same as above except port B is used.

    The 005289 has 12 address inputs and 4 control inputs: LD1, LD2, TG1, TG2.
    It has no data bus, so data values written don't matter.
    When LD1 or LD2 is asserted, the 12 bit value on the address bus is
    latched. Each of the two channels has its own latch.
    When TG1 or TG2 is asserted, the frequency of the respective channel is
    set to the previously latched value.

    The 005289 itself is nothing but an address generator. Digital to analog
    conversion, volume control and mixing of the channels is all done
    externally via resistor networks and 4066 switches and is only implemented
    here for convenience.

    Ported from MAME SVN 27/05/2014

***************************************************************************/

#include "burnint.h"
#include "k005289.h"

// is this an actual hardware limit? or just an arbitrary divider
// to bring the output frequency down to a reasonable value for MAME?
#define CLOCK_DIVIDER 32

static const UINT8 *sound_prom;
static INT32 rate;

static INT16 *mixer_table;
static INT16 *mixer_lookup;
static INT16 *mixer_buffer;
static double gain[2];
static INT32  output_dir[2];

static UINT32 counter[2];
static UINT16 frequency[2];
static UINT16 freq_latch[2];
static UINT16 waveform[2];
static UINT8 volume[2];

/* build a table to divide by the number of voices */
static void make_mixer_table(INT32 voices)
{
	INT32 count = voices * 128;
	INT32 i;
	INT32 ngain = 16;

	/* allocate memory */
	mixer_table = (INT16*)BurnMalloc(256 * voices * sizeof(INT16));

	/* find the middle of the table */
	mixer_lookup = mixer_table + (128 * voices);

	/* fill in the table - 16 bit case */
	for (i = 0; i < count; i++)
	{
		INT32 val = i * ngain * 16 / voices;
		if (val > 32767) val = 32767;
		mixer_lookup[ i] = val;
		mixer_lookup[-i] = -val;
	}
}

void K005289Reset()
{
#if defined FBA_DEBUG
	if (!DebugSnd_K005289Initted) bprintf(PRINT_ERROR, _T("K005289Reset called without init\n"));
#endif

	/* reset all the voices */
	for (INT32 i = 0; i < 2; i++)
	{
		counter[i] = 0;
		frequency[i] = 0;
		freq_latch[i] = 0;
		waveform[i] = i * 0x100;
		volume[i] = 0;
	}
}

void K005289Init(INT32 clock, UINT8 *prom)
{
	/* get stream channels */
	rate = ((clock / CLOCK_DIVIDER) * 100) / nBurnFPS;

	/* allocate a pair of buffers to mix into - 1 frame's worth should be more than enough */
	mixer_buffer = (INT16*)BurnMalloc(rate * sizeof(INT16));

	/* build the mixer table */
	make_mixer_table(2);

	sound_prom = prom;
	
	DebugSnd_K005289Initted = 1;
}

void K005289SetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBA_DEBUG
	if (!DebugSnd_K005289Initted) bprintf(PRINT_ERROR, _T("K005289SetRoute called without init\n"));
#endif

	gain[nIndex] = nVolume;
	output_dir[nIndex] = nRouteDir;
}

void K005289Exit()
{
#if defined FBA_DEBUG
	if (!DebugSnd_K005289Initted) bprintf(PRINT_ERROR, _T("K005289Exit called without init\n"));
#endif

	if (!DebugSnd_K005289Initted) return;

	BurnFree (mixer_buffer);
	BurnFree (mixer_table);
	mixer_buffer = NULL;
	mixer_table = NULL;

	DebugSnd_K005289Initted = 0;
}

void K005289Update(INT16 *buffer, INT32 samples)
{
#if defined FBA_DEBUG
	if (!DebugSnd_K005289Initted) bprintf(PRINT_ERROR, _T("K005289Update called without init\n"));
#endif

	INT16 *mix;
	INT32 i,v,f;

	/* zap the contents of the mixer buffer */
	memset(mixer_buffer, 0, rate * sizeof(INT16));

	v=volume[0];
	f=frequency[0];
	if (v && f)
	{
		const UINT8 *w = sound_prom + waveform[0];
		INT32 c = counter[0];

		mix = mixer_buffer;

		/* add our contribution */
		for (i = 0; i < rate; i++)
		{
			INT32 offs;

			c += CLOCK_DIVIDER;
			offs = (c / f) & 0x1f;
			*mix++ += ((w[offs] & 0x0f) - 8) * v;
		}

		/* update the counter for this voice */
		counter[0] = c % (f * 0x20);
	}

	v=volume[1];
	f=frequency[1];
	if (v && f)
	{
		const UINT8 *w = sound_prom + waveform[1];
		INT32 c = counter[1];

		mix = mixer_buffer;

		/* add our contribution */
		for (i = 0; i < rate; i++)
		{
			INT32 offs;

			c += CLOCK_DIVIDER;
			offs = (c / f) & 0x1f;
			*mix++ += ((w[offs] & 0x0f) - 8) * v;
		}

		/* update the counter for this voice */
		counter[1] = c % (f * 0x20);
	}

	/* mix it down */
	mix = mixer_buffer;
	for (i = 0; i < samples; i++)
	{
		INT32 nLeftSample = mixer_lookup[mix[(i * rate) / nBurnSoundLen]];
		INT32 nRightSample = nLeftSample;

		if ((output_dir[BURN_SND_K005289_ROUTE_1] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample = (INT32)(nLeftSample * gain[BURN_SND_K005289_ROUTE_1]);
		}
		if ((output_dir[BURN_SND_K005289_ROUTE_1] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample = (INT32)(nRightSample * gain[BURN_SND_K005289_ROUTE_1]);
		}

		nLeftSample = BURN_SND_CLIP(nLeftSample);
		nRightSample = BURN_SND_CLIP(nRightSample);

		buffer[0] = BURN_SND_CLIP(buffer[0] + nLeftSample);
		buffer[1] = BURN_SND_CLIP(buffer[1] + nRightSample);
		buffer+=2;
	}
}

void K005289ControlAWrite(UINT8 data)
{
#if defined FBA_DEBUG
	if (!DebugSnd_K005289Initted) bprintf(PRINT_ERROR, _T("K005289ControlAWrite called without init\n"));
#endif

	volume[0] = data & 0xf;
	waveform[0] = data & 0xe0;
}

void K005289ControlBWrite(UINT8 data)
{
#if defined FBA_DEBUG
	if (!DebugSnd_K005289Initted) bprintf(PRINT_ERROR, _T("K005289ControlBWrite called without init\n"));
#endif

	volume[1] = data & 0xf;
	waveform[1] = (data & 0xe0) + 0x100;
}

void K005289Ld1Write(INT32 offset)
{
#if defined FBA_DEBUG
	if (!DebugSnd_K005289Initted) bprintf(PRINT_ERROR, _T("K005289Ld1 called without init\n"));
#endif

	offset &= 0xfff;
	freq_latch[0] = 0xfff - offset;
}

void K005289Ld2Write(INT32 offset)
{
#if defined FBA_DEBUG
	if (!DebugSnd_K005289Initted) bprintf(PRINT_ERROR, _T("K005289Ld2 called without init\n"));
#endif

	offset &= 0xfff;
	freq_latch[1] = 0xfff - offset;
}

void K005289Tg1Write()
{
#if defined FBA_DEBUG
	if (!DebugSnd_K005289Initted) bprintf(PRINT_ERROR, _T("K005289Tg1 called without init\n"));
#endif

	frequency[0] = freq_latch[0];
}

void K005289Tg2Write()
{
#if defined FBA_DEBUG
	if (!DebugSnd_K005289Initted) bprintf(PRINT_ERROR, _T("K005289Tg2 called without init\n"));
#endif

	frequency[1] = freq_latch[1];
}

INT32 K005289Scan(INT32 nAction, INT32 *)
{
#if defined FBA_DEBUG
	if (!DebugSnd_K005289Initted) bprintf(PRINT_ERROR, _T("K005289Scan called without init\n"));
#endif

	if (nAction & ACB_DRIVER_DATA)
	{
		for (INT32 i = 0; i < 2; i++) {
			SCAN_VAR(counter[i]);
			SCAN_VAR(frequency[i]);
			SCAN_VAR(freq_latch[i]);
			SCAN_VAR(waveform[i]);
			SCAN_VAR(volume[i]);
		}

		return 0;
	}

	return 0;
}
