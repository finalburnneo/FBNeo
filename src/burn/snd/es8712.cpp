/**********************************************************************************************
 *
 *  Streaming singe channel ADPCM core for the ES8712 chip
 *  Chip is branded by Excellent Systems, probably OEM'd.
 *
 *  Samples are currently looped, but whether they should and how, is unknown.
 *  Interface to the chip is also not 100% clear.
 *  Should there be any status signals signifying busy, end of sample - etc?
 *
 *  Heavily borrowed from the OKI M6295 source
 *
 *  Excellent Systems ADPCM Emulation
 *  Copyright Nicola Salmoria and the MAME Team
 *
 *  From MAME 0.139u1. Modified for use in FBA Aug 23, 2010.
 *
 **********************************************************************************************/

#include "burnint.h"
#include "math.h"
#include "es8712.h"

#include "stream.h"
static Stream stream;

#define MAX_ES8712_CHIPS	1

#define MAX_SAMPLE_CHUNK	10000

/* struct describing a playing ADPCM chip */
typedef struct _es8712_state es8712_state;
struct _es8712_state
{
	UINT8 playing;			/* 1 if we're actively playing */

	UINT32 base_offset;		/* pointer to the base memory location */
	UINT32 sample;			/* current sample number */
	UINT32 count;			/* total samples to play */

	UINT32 signal;			/* current ADPCM signal */
	UINT32 step;			/* current ADPCM step */

	UINT32 start;			/* starting address for the next loop */
	UINT32 end;				/* ending address for the next loop */
	UINT8  repeat;			/* Repeat current sample when 1 */

	INT32 bank_offset;

// non volatile
	UINT8 *region_base;		/* pointer to the base of the region */

	INT32 sample_rate;		/* samples per frame */
	double volume;			/* set gain */
	INT32 output_dir;
	INT32 addSignal;			/* add signal to stream? */
};

static void (*es8712irq_cb)(INT32) = NULL;

static INT16 *tbuf[MAX_ES8712_CHIPS] = { NULL };

static _es8712_state chips[MAX_ES8712_CHIPS];
static _es8712_state *chip;

static const INT32 index_shift[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
static INT32 diff_lookup[49*16];

/**********************************************************************************************

     compute_tables -- compute the difference tables

***********************************************************************************************/

static void compute_tables()
{
	/* nibble to bit map */
	static const INT32 nbl2bit[16][4] =
	{
		{ 1, 0, 0, 0}, { 1, 0, 0, 1}, { 1, 0, 1, 0}, { 1, 0, 1, 1},
		{ 1, 1, 0, 0}, { 1, 1, 0, 1}, { 1, 1, 1, 0}, { 1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1},
		{-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1}
	};

	INT32 step, nib;

	/* loop over all possible steps */
	for (step = 0; step <= 48; step++)
	{
		/* compute the step value */
		INT32 stepval = (INT32)(floor(16.0 * pow(11.0 / 10.0, (double)step)));

		/* loop over all nibbles and compute the difference */
		for (nib = 0; nib < 16; nib++)
		{
			diff_lookup[step*16 + nib] = nbl2bit[nib][0] *
				(stepval   * nbl2bit[nib][1] +
				 stepval/2 * nbl2bit[nib][2] +
				 stepval/4 * nbl2bit[nib][3] +
				 stepval/8);
		}
	}
}

static void reset_handler(INT32 state)
{
	if (es8712irq_cb) {
		es8712irq_cb(state);
	}
}

/**********************************************************************************************

    generate_adpcm -- general ADPCM decoding routine

***********************************************************************************************/

static void sound_update(INT16 **streams, INT32 samples)
{
	INT16 *buffer = streams[0];

	/* if this chip is active */
	if (chip->playing)
	{
		UINT8 *base = chip->region_base + chip->bank_offset + chip->base_offset;
		INT32 sample = chip->sample;
		INT32 signal = chip->signal;
		INT32 count = chip->count;
		INT32 step = chip->step;
		double volume = chip->volume;
		INT32 val;

		/* loop while we still have samples to generate */
		while (samples)
		{
			/* compute the new amplitude and update the current step */
			val = base[sample / 2] >> (((sample & 1) << 2) ^ 4);
			signal += diff_lookup[step * 16 + (val & 15)];

			/* clamp to the maximum */
			if (signal > 2047)
				signal = 2047;
			else if (signal < -2048)
				signal = -2048;

			/* adjust the step size and clamp */
			step += index_shift[val & 7];
			if (step > 48)
				step = 48;
			else if (step < 0)
				step = 0;

			/* output to the buffer */
			*buffer++ = (INT32)(signal * 16 * volume);
			samples--;

			/* next! */
			++sample;
			if (sample >= count || sample >= 0x100000)
			{
				reset_handler(1);
				if (chip->repeat)
				{
					sample = 0;
					signal = -2;
					step = 0;
				}
				else
				{
					chip->playing = 0;
					break;
				}
			}
		}

		/* update the parameters */
		chip->sample = sample;
		chip->signal = signal;
		chip->step = step;
	}

	/* fill the rest with silence */
	while (samples--)
		*buffer++ = (INT32)(chip->signal * 16 * chip->volume);   // no clicks, please
		//*buffer++ = 0;
}


/**********************************************************************************************

    es8712Update -- update the sound chip so that it is in sync with CPU execution

***********************************************************************************************/

void es8712Update(INT32 device, INT16 *outputs, INT32 samples_len)
{
	if (samples_len != nBurnSoundLen) {
		bprintf(0, _T("es8712Update(): once per frame, please!\n"));
		return;
	}

	stream.render(outputs, samples_len);
}


/**********************************************************************************************

    es8712Init -- start emulation of an ES8712 chip

***********************************************************************************************/

void es8712Init(INT32 device, UINT8 *rom, INT32 sample_rate, INT32 addSignal)
{
	DebugSnd_ES8712Initted = 1;

	if (device != 0) {
		bprintf(0, _T("es8712Init(dev, x, x, x): core supports 1 device (#0)!\n"));
		return;
	}
	if (device >= MAX_ES8712_CHIPS) return;

	chip = &chips[device];

	compute_tables();

	chip->start = 0;
	chip->end = 0;
	chip->repeat = 0;

	chip->bank_offset = 0;
	chip->region_base = (UINT8*)rom;

	/* initialize the rest of the structure */
	chip->signal = (UINT32)-2;

	chip->sample_rate = sample_rate;

	chip->volume = 1.00;
	chip->output_dir = BURN_SND_ROUTE_BOTH;
	chip->addSignal = addSignal;

	if (tbuf[device] == NULL) {
		tbuf[device] = (INT16*)BurnMalloc(sample_rate * sizeof(INT16));
	}

	stream.init(sample_rate, nBurnSoundRate, 1, 0, sound_update);
    stream.set_volume(0.30);

	es8712irq_cb = NULL;
}

void es8712SetBuffered(INT32 (*pCPUCyclesCB)(), INT32 nCpuMhz)
{
	stream.set_buffered(pCPUCyclesCB, nCpuMhz);
}

void es8712SetIRQ(void (*pIRQCB)(INT32))
{
	es8712irq_cb = pIRQCB;
}

void es8712SetRoute(INT32 device, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ES8712Initted) bprintf(PRINT_ERROR, _T("es8712SetRoute called without init\n"));
#endif

	if (device >= MAX_ES8712_CHIPS) return;
	
	chip = &chips[device];
	chip->volume = nVolume;
	chip->output_dir = nRouteDir;

	stream.set_volume(nVolume);
	stream.set_route(nRouteDir);
}

/**********************************************************************************************

    es8712Exit -- stop emulation of an ES8712 chip

***********************************************************************************************/

void es8712Exit(INT32 device)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ES8712Initted) bprintf(PRINT_ERROR, _T("es8712Exit called without init\n"));
#endif

	if (!DebugSnd_ES8712Initted) return;

	if (device >= MAX_ES8712_CHIPS) return;

	chip = &chips[device];

	memset (chip, 0, sizeof(_es8712_state));

	BurnFree (tbuf[device]);

	stream.exit();

	DebugSnd_ES8712Initted = 0;
}

/*************************************************************************************

     es8712Reset -- stop emulation of an ES8712-compatible chip

**************************************************************************************/

void es8712Reset(INT32 device)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ES8712Initted) bprintf(PRINT_ERROR, _T("es8712Reset called without init\n"));
#endif

	if (device >= MAX_ES8712_CHIPS) return;

	chip = &chips[device];

	reset_handler(0);

	if (chip->playing)
	{
		/* update the stream, then turn it off */
		chip->playing = 0;
		chip->repeat = 0;
	}
}


/****************************************************************************

    es8712_set_bank_base -- set the base of the bank on a given chip

*****************************************************************************/

void es8712SetBankBase(INT32 device, INT32 base)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ES8712Initted) bprintf(PRINT_ERROR, _T("es8712SetBankBase called without init\n"));
#endif

	if (device >= MAX_ES8712_CHIPS) return;

	stream.update();

	chip = &chips[device];

	chip->bank_offset = base;
}


/**********************************************************************************************

    es8712Play -- Begin playing the addressed sample

***********************************************************************************************/

void es8712Play(INT32 device)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ES8712Initted) bprintf(PRINT_ERROR, _T("es8712Play called without init\n"));
#endif

	if (device >= MAX_ES8712_CHIPS) return;

	chip = &chips[device];

	if (chip->start < chip->end)
	{
		if (!chip->playing)
		{
			chip->playing = 1;
			chip->base_offset = chip->start;
			chip->sample = 0;
			chip->count = 2 * (chip->end - chip->start + 1);
			chip->repeat = 0;
			//bprintf(0, _T("playing.. start %x\tend %x \tcount %x\n"), chip->start, chip->end, chip->count);

			/* also reset the ADPCM parameters */
			//chip->signal = (UINT32)-2;  // makes clicks (dink)
			//chip->step = 0;
			reset_handler(0);
		}
	}
	/* invalid samples go here */
	else
	{
		if (chip->playing)
		{
			/* update the stream */
			chip->playing = 0;
			reset_handler(1);
		}
	}
}


/**********************************************************************************************

     es8712Write -- generic data write function

***********************************************************************************************/

/**********************************************************************************************
 *
 *  offset  Start       End
 *          0hmmll  -  0HMMLL
 *    00    ----ll
 *    01    --mm--
 *    02    0h----
 *    03               ----LL
 *    04               --MM--
 *    05               0H----
 *    06           Go!
 *
 * Offsets are written in the order -> 00, 02, 01, 03, 05, 04, 06
 * Offset 06 is written with the same value as offset 04.
 *
***********************************************************************************************/

void es8712Write(INT32 device, INT32 offset, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ES8712Initted) bprintf(PRINT_ERROR, _T("es8712Write called without init\n"));
#endif

	if (device >= MAX_ES8712_CHIPS) return;

	chip = &chips[device];

	stream.update();

	switch (offset)
	{
		case 00:	chip->start &= 0x000fff00;
					chip->start |= ((data & 0xff) <<  0); break;
		case 01:	chip->start &= 0x000f00ff;
					chip->start |= ((data & 0xff) <<  8); break;
		case 02:	chip->start &= 0x0000ffff;
					chip->start |= ((data & 0x0f) << 16); break;
		case 03:	chip->end   &= 0x000fff00;
					chip->end   |= ((data & 0xff) <<  0); break;
		case 04:	chip->end   &= 0x000f00ff;
					chip->end   |= ((data & 0xff) <<  8); break;
		case 05:	chip->end   &= 0x0000ffff;
					chip->end   |= ((data & 0x0f) << 16); break;
		case 06:
				es8712Play(device);
				break;
		default:	break;
	}

	chip->start &= 0xfffff;
	chip->end &= 0xfffff;
}


/**********************************************************************************************

     es8712Scan -- save state function

***********************************************************************************************/

void es8712Scan(INT32 nAction, INT32 *)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_ES8712Initted) bprintf(PRINT_ERROR, _T("es8712Scan called without init\n"));
#endif

	if (nAction & ACB_DRIVER_DATA)
	{
		for (INT32 i = 0; i < MAX_ES8712_CHIPS; i++)
		{
			chip = &chips[i];

			SCAN_VAR(chip->playing);
			SCAN_VAR(chip->base_offset);
			SCAN_VAR(chip->sample);
			SCAN_VAR(chip->count);
			SCAN_VAR(chip->signal);
			SCAN_VAR(chip->step);
			SCAN_VAR(chip->start);
			SCAN_VAR(chip->end);
			SCAN_VAR(chip->repeat);
			SCAN_VAR(chip->bank_offset);
		}
	}
}
