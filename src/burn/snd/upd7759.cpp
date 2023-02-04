// Based on MAME driver by Juergen Buchmueller, Mike Balfour, Howie Cohen, Olivier Galibert, Aaron Giles

#include "burnint.h"
#include "upd7759.h"
#include "downsample.h"
#include "timer.h"
#include <math.h>
#include "biquad.h"

#define FRAC_BITS			20
#define FRAC_ONE			(1 << FRAC_BITS)
#define FRAC_MASK			(FRAC_ONE - 1)

static INT32 SlaveMode = 0;

/* chip states */
enum
{
	STATE_IDLE,
	STATE_DROP_DRQ,
	STATE_START,
	STATE_FIRST_REQ,
	STATE_LAST_SAMPLE,
	STATE_DUMMY1,
	STATE_ADDR_MSB,
	STATE_ADDR_LSB,
	STATE_DUMMY2,
	STATE_BLOCK_HEADER,
	STATE_NIBBLE_COUNT,
	STATE_NIBBLE_MSN,
	STATE_NIBBLE_LSN
};

struct upd7759_chip
{
	INT32		ChipNum; // for callback
	/* internal clock to output sample rate mapping */
	UINT32		pos;						/* current output sample position */
	UINT32		step;						/* step value per output sample */
	double      clock_period;
	INT32       start_delay;

	/* I/O lines */
	UINT8		fifo_in;					/* last data written to the sound chip */
	UINT8		reset;						/* current state of the RESET line */
	UINT8		start;						/* current state of the START line */
	UINT8		drq;						/* current state of the DRQ line */
	void (*drqcallback)(INT32 param);		/* drq callback */

	/* internal state machine */
	INT8		state;						/* current overall chip state */
	INT32		clocks_left;				/* number of clocks left in this state */
	UINT16		nibbles_left;				/* number of ADPCM nibbles left to process */
	UINT8		repeat_count;				/* number of repeats remaining in current repeat block */
	INT8		post_drq_state;				/* state we will be in after the DRQ line is dropped */
	INT32		post_drq_clocks;			/* clocks that will be left after the DRQ line is dropped */
	UINT8		req_sample;					/* requested sample number */
	UINT8		last_sample;				/* last sample number available */
	UINT8		block_header;				/* header byte */
	UINT8		sample_rate;				/* number of UPD clocks per ADPCM nibble */
	UINT8		first_valid_header;			/* did we get our first valid header yet? */
	UINT32		offset;						/* current ROM offset */
	UINT32		repeat_offset;				/* current ROM repeat offset */

	/* ADPCM processing */
	INT8		adpcm_state;				/* ADPCM state index */
	UINT8		adpcm_data;					/* current byte of ADPCM data */
	INT16		sample;						/* current sample value */
	INT32		sample_counts;				// this frame

	/* ROM access */
	UINT8 *		rom;						/* pointer to ROM data or NULL for slave mode */
	UINT8 *		rombase;					/* pointer to ROM data or NULL for slave mode */
	UINT32		romoffset;					/* ROM offset to make save/restore easier */

	// downsampler
	Downsampler resamp;
	INT16 *     out_buf;
	INT16 *     out_buf_linear;
	INT16 *     out_buf_linear_resampled;
	INT32       out_buf_size;
	INT32       out_pos;

	// filter
	BIQ biquadL;
	BIQ biquadR;

	// stream sync
	INT32 (*pTotalCyclesCB)();
	INT32 nCpuMHZ;

	/* route */
	double		volume;
	INT32		output_dir;
};

static struct upd7759_chip *Chips[2] = { NULL, NULL }; // more?
static struct upd7759_chip *Chip = NULL;

static INT32 nNumChips = 0;

static const INT32 upd7759_step[16][16] =
{
	{ 0,  0,  1,  2,  3,   5,   7,  10,  0,   0,  -1,  -2,  -3,   -5,   -7,  -10 },
	{ 0,  1,  2,  3,  4,   6,   8,  13,  0,  -1,  -2,  -3,  -4,   -6,   -8,  -13 },
	{ 0,  1,  2,  4,  5,   7,  10,  15,  0,  -1,  -2,  -4,  -5,   -7,  -10,  -15 },
	{ 0,  1,  3,  4,  6,   9,  13,  19,  0,  -1,  -3,  -4,  -6,   -9,  -13,  -19 },
	{ 0,  2,  3,  5,  8,  11,  15,  23,  0,  -2,  -3,  -5,  -8,  -11,  -15,  -23 },
	{ 0,  2,  4,  7, 10,  14,  19,  29,  0,  -2,  -4,  -7, -10,  -14,  -19,  -29 },
	{ 0,  3,  5,  8, 12,  16,  22,  33,  0,  -3,  -5,  -8, -12,  -16,  -22,  -33 },
	{ 1,  4,  7, 10, 15,  20,  29,  43, -1,  -4,  -7, -10, -15,  -20,  -29,  -43 },
	{ 1,  4,  8, 13, 18,  25,  35,  53, -1,  -4,  -8, -13, -18,  -25,  -35,  -53 },
	{ 1,  6, 10, 16, 22,  31,  43,  64, -1,  -6, -10, -16, -22,  -31,  -43,  -64 },
	{ 2,  7, 12, 19, 27,  37,  51,  76, -2,  -7, -12, -19, -27,  -37,  -51,  -76 },
	{ 2,  9, 16, 24, 34,  46,  64,  96, -2,  -9, -16, -24, -34,  -46,  -64,  -96 },
	{ 3, 11, 19, 29, 41,  57,  79, 117, -3, -11, -19, -29, -41,  -57,  -79, -117 },
	{ 4, 13, 24, 36, 50,  69,  96, 143, -4, -13, -24, -36, -50,  -69,  -96, -143 },
	{ 4, 16, 29, 44, 62,  85, 118, 175, -4, -16, -29, -44, -62,  -85, -118, -175 },
	{ 6, 20, 36, 54, 76, 104, 144, 214, -6, -20, -36, -54, -76, -104, -144, -214 },
};

static const INT32 upd7759_state[16] = { -1, -1, 0, 0, 1, 2, 2, 3, -1, -1, 0, 0, 1, 2, 2, 3 };

inline static void UpdateAdpcm(INT32 Data)
{
	Chip->sample += upd7759_step[Chip->adpcm_state][Data];
	Chip->adpcm_state += upd7759_state[Data];

	/* clamp the state to 0..15 */
	if (Chip->adpcm_state < 0)
		Chip->adpcm_state = 0;
	else if (Chip->adpcm_state > 15)
		Chip->adpcm_state = 15;
}

static INT32 SyncUPD(upd7759_chip *Chippy, INT32 cycles)
{
	return (INT32)((double)cycles * ((double)(Chippy->pTotalCyclesCB()) / ((double)Chippy->nCpuMHZ / (nBurnFPS / 100.0000))));
}

static void UpdateStream(INT32 chip, INT32 end);

static void UPD7759AdvanceState()
{
	switch (Chip->state)
	{
		/* Idle state: we stick around here while there's nothing to do */
		case STATE_IDLE:
			Chip->clocks_left = 4;
			break;

		/* drop DRQ state: update to the intended state */
		case STATE_DROP_DRQ:
			Chip->drq = 0;

			Chip->clocks_left = Chip->post_drq_clocks;
			Chip->state = Chip->post_drq_state;
			break;

		/* Start state: we begin here as soon as a sample is triggered */
		case STATE_START:
			Chip->req_sample = (Chip->rom) ? Chip->fifo_in : 0x10;
			/* 35+ cycles after we get here, the /DRQ goes low
             *     (first byte (number of samples in ROM) should be sent in response)
             *
             * (35 is the minimum number of cycles I found during heavy tests.
             * Depending on the state the chip was in just before the /MD was set to 0 (reset, standby
             * or just-finished-playing-previous-sample) this number can range from 35 up to ~24000).
             * It also varies slightly from test to test, but not much - a few cycles at most.) */
			Chip->clocks_left = 70 + Chip->start_delay;	/* 35 - breaks cotton */
			Chip->state = STATE_FIRST_REQ;
			break;

		/* First request state: issue a request for the first byte */
		/* The expected response will be the index of the last sample */
		case STATE_FIRST_REQ:
			Chip->drq = 1;

			/* 44 cycles later, we will latch this value and request another byte */
			Chip->clocks_left = 44;
			Chip->state = STATE_LAST_SAMPLE;
			break;

		/* Last sample state: latch the last sample value and issue a request for the second byte */
		/* The second byte read will be just a dummy */
		case STATE_LAST_SAMPLE:
			Chip->last_sample = Chip->rom ? Chip->rom[0] : Chip->fifo_in;

			Chip->drq = 1;

			/* 28 cycles later, we will latch this value and request another byte */
			Chip->clocks_left = 28;	/* 28 - breaks cotton */
			Chip->state = (Chip->req_sample > Chip->last_sample) ? STATE_IDLE : STATE_DUMMY1;
			break;

		/* First dummy state: ignore any data here and issue a request for the third byte */
		/* The expected response will be the MSB of the sample address */
		case STATE_DUMMY1:
			Chip->drq = 1;

			/* 32 cycles later, we will latch this value and request another byte */
			Chip->clocks_left = 32;
			Chip->state = STATE_ADDR_MSB;
			break;

		/* Address MSB state: latch the MSB of the sample address and issue a request for the fourth byte */
		/* The expected response will be the LSB of the sample address */
		case STATE_ADDR_MSB:
			Chip->offset = ((Chip->rom) ? Chip->rom[Chip->req_sample * 2 + 5] : Chip->fifo_in) << 9;

			Chip->drq = 1;

			/* 44 cycles later, we will latch this value and request another byte */
			Chip->clocks_left = 44;
			Chip->state = STATE_ADDR_LSB;
			break;

		/* Address LSB state: latch the LSB of the sample address and issue a request for the fifth byte */
		/* The expected response will be just a dummy */
		case STATE_ADDR_LSB:
			Chip->offset |= ((Chip->rom) ? Chip->rom[Chip->req_sample * 2 + 6] : Chip->fifo_in) << 1;
			Chip->drq = 1;

			/* 36 cycles later, we will latch this value and request another byte */
			Chip->clocks_left = 36;
			Chip->state = STATE_DUMMY2;
			break;

		/* Second dummy state: ignore any data here and issue a request for the the sixth byte */
		/* The expected response will be the first block header */
		case STATE_DUMMY2:
			Chip->offset++;
			Chip->first_valid_header = 0;
			Chip->drq = 1;

			/* 36?? cycles later, we will latch this value and request another byte */
			Chip->clocks_left = 36;
			Chip->state = STATE_BLOCK_HEADER;
			break;

		/* Block header state: latch the header and issue a request for the first byte afterwards */
		case STATE_BLOCK_HEADER:

			/* if we're in a repeat loop, reset the offset to the repeat point and decrement the count */
			if (Chip->repeat_count)
			{
				Chip->repeat_count--;
				Chip->offset = Chip->repeat_offset;
			}
			Chip->block_header = (Chip->rom) ? Chip->rom[Chip->offset++ & 0x1ffff] : Chip->fifo_in;
			Chip->drq = 1;

			/* our next step depends on the top two bits */
			switch (Chip->block_header & 0xc0)
			{
				case 0x00:	/* silence */
					Chip->clocks_left = 1024 * ((Chip->block_header & 0x3f) + 1);
					Chip->state = (Chip->block_header == 0 && Chip->first_valid_header) ? STATE_IDLE : STATE_BLOCK_HEADER;
					Chip->sample = 0;
					Chip->adpcm_state = 0;
					break;

				case 0x40:	/* 256 nibbles */
					Chip->sample_rate = (Chip->block_header & 0x3f) + 1;
					Chip->nibbles_left = 256;
					Chip->clocks_left = 36;	/* just a guess */
					Chip->state = STATE_NIBBLE_MSN;
					break;

				case 0x80:	/* n nibbles */
					Chip->sample_rate = (Chip->block_header & 0x3f) + 1;
					Chip->clocks_left = 36;	/* just a guess */
					Chip->state = STATE_NIBBLE_COUNT;
					break;

				case 0xc0:	/* repeat loop */
					Chip->repeat_count = (Chip->block_header & 7) + 1;
					Chip->repeat_offset = Chip->offset;
					Chip->clocks_left = 36;	/* just a guess */
					Chip->state = STATE_BLOCK_HEADER;
					break;
			}

			/* set a flag when we get the first non-zero header */
			if (Chip->block_header != 0)
				Chip->first_valid_header = 1;
			break;

		/* Nibble count state: latch the number of nibbles to play and request another byte */
		/* The expected response will be the first data byte */
		case STATE_NIBBLE_COUNT:
			Chip->nibbles_left = ((Chip->rom) ? Chip->rom[Chip->offset++ & 0x1ffff] : Chip->fifo_in) + 1;
			Chip->drq = 1;

			/* 36?? cycles later, we will latch this value and request another byte */
			Chip->clocks_left = 36;	/* just a guess */
			Chip->state = STATE_NIBBLE_MSN;
			break;

		/* MSN state: latch the data for this pair of samples and request another byte */
		/* The expected response will be the next sample data or another header */
		case STATE_NIBBLE_MSN:
			Chip->adpcm_data = (Chip->rom) ? Chip->rom[Chip->offset++ & 0x1ffff] : Chip->fifo_in;
			UpdateAdpcm(Chip->adpcm_data >> 4);
			Chip->drq = 1;

			/* we stay in this state until the time for this sample is complete */
			Chip->clocks_left = Chip->sample_rate * 4;
			if (--Chip->nibbles_left == 0)
				Chip->state = STATE_BLOCK_HEADER;
			else
				Chip->state = STATE_NIBBLE_LSN;
			break;

		/* LSN state: process the lower nibble */
		case STATE_NIBBLE_LSN:
			UpdateAdpcm(Chip->adpcm_data & 15);

			/* we stay in this state until the time for this sample is complete */
			Chip->clocks_left = Chip->sample_rate * 4;
			if (--Chip->nibbles_left == 0)
				Chip->state = STATE_BLOCK_HEADER;
			else
				Chip->state = STATE_NIBBLE_MSN;
			break;
	}

	/* if there's a DRQ, fudge the state */
	if (Chip->drq)
	{
		Chip->post_drq_state = Chip->state;
		Chip->post_drq_clocks = Chip->clocks_left - 21;
		Chip->state = STATE_DROP_DRQ;
		Chip->clocks_left = 21;
	}
}

static void UPD7759SlaveModeUpdate()
{
	Chip = Chips[0]; // slave mode only available on Chip 0
	UINT8 OldDrq = Chip->drq;

	UpdateStream(Chip->ChipNum, 0);

	UPD7759AdvanceState();

	if (OldDrq != Chip->drq && Chip->drqcallback) {
		(*Chip->drqcallback)(Chip->drq);
	}

	/* set a timer to go off when that is done */

	if (Chip->state != STATE_IDLE) {
		BurnTimerSetRetrig(0, (double)Chip->clocks_left * Chip->clock_period);
	}
}

INT32 slave_timer_cb(INT32 n, INT32 c)
{
	UPD7759SlaveModeUpdate();

	return 0;
}


void UPD7759Update(INT32 chip, INT32 nLength)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759Update called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("UPD7759Update called with invalid chip %x\n"), chip);
#endif
	Chip = Chips[chip];
	INT32 ClocksLeft = Chip->clocks_left;
	INT16 Sample = Chip->sample;
	UINT32 Step = Chip->step;
	UINT32 Pos = Chip->pos;

	/* loop until done */
	if (Chip->state != STATE_IDLE)
	{
		while (nLength != 0)
		{
			Chip->out_buf[Chip->out_pos] = Sample * 128;
			Chip->out_pos = (Chip->out_pos + 1) % Chip->out_buf_size;

			Chip->sample_counts++;

			nLength--;

			/* advance by the number of clocks/output sample */
			Pos += Step;

			/* handle clocks, but only in standalone mode */
			while (Chip->rom && Pos >= FRAC_ONE)
			{
				INT32 ClocksThisTime = Pos >> FRAC_BITS;
				if (ClocksThisTime > ClocksLeft)
					ClocksThisTime = ClocksLeft;

				/* clock once */
				Pos -= ClocksThisTime * FRAC_ONE;
				ClocksLeft -= ClocksThisTime;

				/* if we're out of clocks, time to handle the next state */
				if (ClocksLeft == 0)
				{
					/* advance one state; if we hit idle, bail */
					UPD7759AdvanceState();
					if (Chip->state == STATE_IDLE)
						break;

					/* reimport the variables that we cached */
					ClocksLeft = Chip->clocks_left;
					Sample = Chip->sample;
				}
			}
		}
	}

	while (nLength > 0) { // fill unused with nothing
		Chip->out_buf[Chip->out_pos] = 0;
		Chip->out_pos = (Chip->out_pos + 1) % Chip->out_buf_size;

		Chip->sample_counts++;

		nLength--;
	}

	Chip->clocks_left = ClocksLeft;
	Chip->pos = Pos;
}

static void UpdateStream(INT32 chip, INT32 end)
{
	if (!pBurnSoundOut) return;

	Chip = Chips[chip];

	INT32 framelen = Chip->resamp.samples_to_source(nBurnSoundLen);
	INT32 position = (end) ? framelen : SyncUPD(Chip, framelen);

	INT32 samples = position - Chip->sample_counts;

	if (samples < 1) return;

	UPD7759Update(chip, samples);
}

void UPD7759Render(INT16 *pSoundBuf, INT32 samples)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759Render called without init\n"));
#endif

	for (INT32 i = 0; i <= nNumChips; i++) // first chip 0, second chip 1...
	{
		UPD7759Render(i, pSoundBuf, samples);
	}
}

void UPD7759Render(INT32 chip, INT16 *pSoundBuf, INT32 samples)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759Render called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("UPD7759Render called with invalid chip %x\n"), chip);
#endif

	Chip = Chips[chip];

	if (Chip->pTotalCyclesCB) UpdateStream(chip, 1); // fill 'er up!

	INT32 nFrameLength = Chip->resamp.samples_to_source(nBurnSoundLen);

	// copy frame worth of samples out of circular buffer into linear buffer.
	for (INT32 i = 0; i < nFrameLength; i++) {
		INT32 cpos = (Chip->out_pos + nFrameLength + i) % Chip->out_buf_size;
		Chip->out_buf_linear[i] = Chip->out_buf[cpos];
	    Chip->out_buf[cpos] = 0;
	}

	// resample to host rate
	Chip->resamp.resample_mono(Chip->out_buf_linear, Chip->out_buf_linear_resampled, samples, 1.00);

	// filter
	Chip->biquadL.filter_buffer(Chip->out_buf_linear_resampled, samples); // shelf
	Chip->biquadR.filter_buffer(Chip->out_buf_linear_resampled, samples); // lowpass

	// filter and mix into pSoundBuf
	for (INT32 i = 0; i < samples; i++) {
		INT32 l = 0, r = 0;
		INT32 sample = Chip->out_buf_linear_resampled[i];

		if ((Chip->output_dir & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			l = (INT32)(sample * Chip->volume);
		}
		if ((Chip->output_dir & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			r = (INT32)(sample * Chip->volume);
		}
		
		pSoundBuf[i*2 + 0] = BURN_SND_CLIP(pSoundBuf[i*2 + 0] + l);
		pSoundBuf[i*2 + 1] = BURN_SND_CLIP(pSoundBuf[i*2 + 1] + r);
	}

	Chip->sample_counts = 0; // ready for next frame!
}

static void UPD7759ResetINT(INT32 chip)
{
	upd7759_chip *rChip = Chips[chip];

	if (SlaveMode) {
		BurnTimerReset();
	}

	rChip->pos                = 0;
	rChip->fifo_in            = 0;
	rChip->drq                = 0;
	rChip->state              = STATE_IDLE;
	rChip->clocks_left        = 0;
	rChip->nibbles_left       = 0;
	rChip->repeat_count       = 0;
	rChip->post_drq_state     = STATE_IDLE;
	rChip->post_drq_clocks    = 0;
	rChip->req_sample         = 0;
	rChip->last_sample        = 0;
	rChip->block_header       = 0;
	rChip->sample_rate        = 0;
	rChip->first_valid_header = 0;
	rChip->offset             = 0;
	rChip->repeat_offset      = 0;
	rChip->adpcm_state        = 0;
	rChip->adpcm_data         = 0;
	rChip->sample             = 0;
}

void UPD7759Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759Reset called without init\n"));
#endif

	if (SlaveMode) {
		BurnTimerReset();
	}

	for (INT32 i = 0; i <= nNumChips; i++) { // first chip 0, second chip 1...
		UPD7759ResetINT(i);
	}
}

void UPD7759Init(INT32 chip, INT32 clock, UINT8* pSoundData)
{
	DebugSnd_UPD7759Initted = 1;

	Chips[chip] = (struct upd7759_chip*)BurnMalloc(sizeof(upd7759_chip));
	Chip = Chips[chip];

	memset(Chip, 0, sizeof(upd7759_chip));

	SlaveMode = 0;

	Chip->ChipNum = chip;

	// init resampler
	Chip->resamp.init(clock / 4, nBurnSoundRate, 0);

	// init filter (basically passthru w/these settings)
	Chip->biquadL.init(FILT_LOWPASS, nBurnSoundRate, 15000, 0.554, 0.0);
	Chip->biquadR.init(FILT_LOWPASS, nBurnSoundRate, 15000, 0.554, 0.0);

	Chip->step = (INT32)(4 * FRAC_ONE);
	Chip->state = STATE_IDLE;
	Chip->clock_period = 1.0 / (double)clock;

	// our 16bit (INT16) output buffer is 2 frames in length @ 640,000 kHz
	// out_buf - circular buffer @ clock/4 rate
	// out_buf_linear - linearized right before resampling and rendering
	// out_buf_linear_resampled - resampled and ready to be filtered
	Chip->out_buf_size = (clock / 4) * 100 / (nBurnFPS / 2); // word-size (byte size = *2)
	Chip->out_buf = (INT16*)BurnMalloc(Chip->out_buf_size * sizeof(INT16));
	Chip->out_buf_linear = (INT16*)BurnMalloc(Chip->out_buf_size * sizeof(INT16));
	Chip->out_buf_linear_resampled = (INT16*)BurnMalloc(Chip->out_buf_size * sizeof(INT16));
	Chip->out_pos = 0;

	if (pSoundData) {
		Chip->rom = pSoundData;
		SlaveMode = 0;
	} else {
		SlaveMode = 1;
		BurnTimerInit(&slave_timer_cb, NULL); // for high-freq timer
	}
	
	Chip->reset = 1;
	Chip->start = 1;
	Chip->volume = 1.00;
	Chip->output_dir = BURN_SND_ROUTE_BOTH;
	
	nNumChips = chip;
	
	UPD7759Reset();
}

void UPD7759SetStartDelay(INT32 chip, INT32 nDelay)
{
	Chip = Chips[chip];

	Chip->start_delay = nDelay;
}

void UPD7759SetFilter(INT32 chip, INT32 nCutOff)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759SetFilter called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("UPD7759SetFilter called with invalid chip %i\n"), chip);
#endif

	Chip = Chips[chip];

	Chip->biquadL.init(FILT_HIGHSHELF, nBurnSoundRate, nCutOff, 0.0, -25.0);
	Chip->biquadR.init(FILT_LOWPASS, nBurnSoundRate, 4000, 0.70, 0.0);
}

void UPD7759SetRoute(INT32 chip, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759SetRoute called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("UPD7759SetRoute called with invalid chip %i\n"), chip);
#endif

	Chip = Chips[chip];
	Chip->volume = nVolume;
	Chip->output_dir = nRouteDir;
}

void UPD7759SetDrqCallback(INT32 chip, drqcallback Callback)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759SetDrqCallback called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("UPD7759SetDrqCallback called with invalid chip %x\n"), chip);
#endif

	Chip = Chips[chip];
	Chip->drqcallback = Callback;
}

void UPD7759SetSyncCallback(INT32 chip, INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759SetSyncCallback called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("UPD7759SetSyncCallback called with invalid chip %x\n"), chip);
#endif

	Chip = Chips[chip];
	Chip->pTotalCyclesCB = pCPUCyclesCB;
	Chip->nCpuMHZ = nCPUMhz;
}

INT32 UPD7759BusyRead(INT32 chip)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759BusyRead called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("UPD7759BusyRead called with invalid chip %x\n"), chip);
#endif

	Chip = Chips[chip];
	return (Chip->state == STATE_IDLE);
}

void UPD7759ResetWrite(INT32 chip, UINT8 Data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759ResetWrite called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("UPD7759ResetWrite called with invalid chip %x\n"), chip);
#endif

	Chip = Chips[chip];

	if (Chip->pTotalCyclesCB) UpdateStream(chip, 0);

	UINT8 Oldreset = Chip->reset;
	Chip->reset = (Data != 0);

	if (Oldreset && !Chip->reset) {
		UPD7759ResetINT(chip);
	}
}

void UPD7759StartWrite(INT32 chip, UINT8 Data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759StartWrite called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("UPD7759StartWrite called with invalid chip %x\n"), chip);
#endif

	Chip = Chips[chip];

	UINT8 Oldstart = Chip->start;
	Chip->start = (Data != 0);

	if (Chip->pTotalCyclesCB) UpdateStream(chip, 0);

	if (Chip->state == STATE_IDLE && !Oldstart && Chip->start && Chip->reset) {
		Chip->state = STATE_START;

		if (SlaveMode) UPD7759SlaveModeUpdate();
	}
}

void UPD7759PortWrite(INT32 chip, UINT8 Data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759PortWrite called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("UPD7759PortWrite called with invalid chip %x\n"), chip);
#endif

	Chip = Chips[chip];

	Chip->fifo_in = Data;
}

void UPD7759Scan(INT32 nAction, INT32 *pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759Scan called without init\n"));
#endif
	
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return;
	}
	
	if (pnMin != NULL) {
		*pnMin = 0x029680;
	}

	if (SlaveMode) {
		BurnTimerScan(nAction, pnMin);
	}

	for (INT32 i = 0; i <= nNumChips; i++) // first chip 0, second chip 1...
	{
		upd7759_chip *sChip = Chips[i];

		SCAN_VAR(sChip->pos);
		SCAN_VAR(sChip->step);
		SCAN_VAR(sChip->fifo_in);
		SCAN_VAR(sChip->reset);
		SCAN_VAR(sChip->start);
		SCAN_VAR(sChip->drq);
		SCAN_VAR(sChip->state);
		SCAN_VAR(sChip->clocks_left);
		SCAN_VAR(sChip->nibbles_left);
		SCAN_VAR(sChip->repeat_count);
		SCAN_VAR(sChip->post_drq_state);
		SCAN_VAR(sChip->post_drq_clocks);
		SCAN_VAR(sChip->req_sample);
		SCAN_VAR(sChip->last_sample);
		SCAN_VAR(sChip->block_header);
		SCAN_VAR(sChip->sample_rate);
		SCAN_VAR(sChip->first_valid_header);
		SCAN_VAR(sChip->offset);
		SCAN_VAR(sChip->repeat_offset);
		SCAN_VAR(sChip->adpcm_state);
		SCAN_VAR(sChip->adpcm_data);
		SCAN_VAR(sChip->sample);
		SCAN_VAR(sChip->romoffset);
		SCAN_VAR(sChip->volume);
		SCAN_VAR(sChip->output_dir);
	}
}

void UPD7759Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_UPD7759Initted) bprintf(PRINT_ERROR, _T("UPD7759Exit called without init\n"));
#endif

	for (INT32 i = 0; i <= nNumChips; i++) // first chip 0, second chip 1...
	{
		Chip = Chips[i];
		if (Chip) {
			Chip->biquadL.exit();
			Chip->biquadR.exit();
			BurnFree(Chip->out_buf);
			BurnFree(Chip->out_buf_linear);
			BurnFree(Chip->out_buf_linear_resampled);
			BurnFree(Chips[i]);
		}
	}

	if (SlaveMode) {
		BurnTimerExit();
	}

	SlaveMode = 0;

	DebugSnd_UPD7759Initted = 0;
	nNumChips = 0;
}
