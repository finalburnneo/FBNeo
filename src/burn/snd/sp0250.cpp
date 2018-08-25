/*
   GI SP0250 digital LPC sound synthesizer

   By O. Galibert.

   Unknown:
   - Exact clock divider
   - Exact noise algorithm
   - Exact noise pitch (probably ok)
   - 7 bits output mapping
   - Whether the pitch starts counting from 0 or 1

   Unimplemented:
   - Direct Data test mode (pin 7)

   Sound quite reasonably already though.
*/

#include "burnint.h"
#include <math.h>
#include "sp0250.h"

#define ASSERT_LINE 1
#define CLEAR_LINE 0

static INT32 (*pCPUTotalCycles)() = NULL;
static UINT32  nDACCPUMHZ = 0;
static INT32   nCurrentPosition = 0;
static INT16  *mixer_buffer; // re-sampler

static struct sp0250 *sp = NULL;
static void (*drq)(INT32 state) = NULL;
static INT32 sp0250_clock; // sp0250 clockrate
static INT32 sp0250_frame; // frame sample-size

static void sp0250_update_int(INT16 *buffer, INT32 length); // forward

// Streambuffer handling
static INT32 SyncInternal()
{
	return (INT32)(float)(sp0250_frame * (pCPUTotalCycles() / (nDACCPUMHZ / (nBurnFPS / 100.0000))));
}

static void UpdateStream(INT32 length)
{
	length -= nCurrentPosition;
	if (length <= 0) return;

	INT16 *lbuf = mixer_buffer + nCurrentPosition;

	sp0250_update_int(lbuf, length);

	nCurrentPosition += length;
}

/*
standard external clock is 3.12MHz
the chip provides a 445.7kHz output clock, which is = 3.12MHz / 7
therefore I expect the clock divider to be a multiple of 7
Also there are 6 cascading filter stages so I expect the divider to be a multiple of 6.

The SP0250 manual states that the original speech is sampled at 10kHz, so the divider
should be 312, but 312 = 39*8 so it doesn't look right because a divider by 39 is unlikely.

7*6*8 = 336 gives a 9.286kHz sample rate and matches the samples from the Sega boards.
*/
#define CLOCK_DIVIDER (7*6*8)

struct sp0250
{
	INT16 amp;
	UINT8 pitch;
	UINT8 repeat;
	INT32 pcount, rcount;
	INT32 playing;
	UINT32 RNG;
	INT32 voiced;
	UINT8 fifo[15];
	INT32 fifo_pos;

	struct
	{
		INT16 F, B;
		INT16 z1, z2;
	} filter[6];
};

static UINT16 sp0250_ga(UINT8 v)
{
	return (v & 0x1f) << (v>>5);
}

static INT16 sp0250_gc(UINT8 v)
{
	// Internal ROM to the chip, cf. manual
	static const UINT16 coefs[128] =
	{
		  0,   9,  17,  25,  33,  41,  49,  57,  65,  73,  81,  89,  97, 105, 113, 121,
		129, 137, 145, 153, 161, 169, 177, 185, 193, 201, 203, 217, 225, 233, 241, 249,
		257, 265, 273, 281, 289, 297, 301, 305, 309, 313, 317, 321, 325, 329, 333, 337,
		341, 345, 349, 353, 357, 361, 365, 369, 373, 377, 381, 385, 389, 393, 397, 401,
		405, 409, 413, 417, 421, 425, 427, 429, 431, 433, 435, 437, 439, 441, 443, 445,
		447, 449, 451, 453, 455, 457, 459, 461, 463, 465, 467, 469, 471, 473, 475, 477,
		479, 481, 482, 483, 484, 485, 486, 487, 488, 489, 490, 491, 492, 493, 494, 495,
		496, 497, 498, 499, 500, 501, 502, 503, 504, 505, 506, 507, 508, 509, 510, 511
	};
	INT16 res = coefs[v & 0x7f];

	if (!(v & 0x80))
		res = -res;
	return res;
}

static void sp0250_load_values()
{
	INT32 f;


	sp->filter[0].B = sp0250_gc(sp->fifo[ 0]);
	sp->filter[0].F = sp0250_gc(sp->fifo[ 1]);
	sp->amp         = sp0250_ga(sp->fifo[ 2]);
	sp->filter[1].B = sp0250_gc(sp->fifo[ 3]);
	sp->filter[1].F = sp0250_gc(sp->fifo[ 4]);
	sp->pitch       =           sp->fifo[ 5];
	sp->filter[2].B = sp0250_gc(sp->fifo[ 6]);
	sp->filter[2].F = sp0250_gc(sp->fifo[ 7]);
	sp->repeat      =           sp->fifo[ 8] & 0x3f;
	sp->voiced      =           sp->fifo[ 8] & 0x40;
	sp->filter[3].B = sp0250_gc(sp->fifo[ 9]);
	sp->filter[3].F = sp0250_gc(sp->fifo[10]);
	sp->filter[4].B = sp0250_gc(sp->fifo[11]);
	sp->filter[4].F = sp0250_gc(sp->fifo[12]);
	sp->filter[5].B = sp0250_gc(sp->fifo[13]);
	sp->filter[5].F = sp0250_gc(sp->fifo[14]);
	sp->fifo_pos = 0;
	drq(ASSERT_LINE);

	sp->pcount = 0;
	sp->rcount = 0;

	for (f = 0; f < 6; f++)
		sp->filter[f].z1 = sp->filter[f].z2 = 0;

	sp->playing = 1;
}

static void sp0250_update_int(INT16 *buffer, INT32 length)
{
	for (INT32 i = 0; i < length; i++)
	{
		if (sp->playing)
		{
			INT16 z0;
			INT32 f;

			if (sp->voiced)
			{
				if(!sp->pcount)
					z0 = sp->amp;
				else
					z0 = 0;
			}
			else
			{
				// Borrowing the ay noise generation LFSR
				if(sp->RNG & 1)
				{
					z0 = sp->amp;
					sp->RNG ^= 0x24000;
				}
				else
					z0 = -sp->amp;

				sp->RNG >>= 1;
			}

			for (f = 0; f < 6; f++)
			{
				z0 += ((sp->filter[f].z1 * sp->filter[f].F) >> 8)
					+ ((sp->filter[f].z2 * sp->filter[f].B) >> 9);
				sp->filter[f].z2 = sp->filter[f].z1;
				sp->filter[f].z1 = z0;
			}

			// Physical resolution is only 7 bits, but heh

			// max amplitude is 0x0f80 so we have margin to push up the output
			buffer[i] = z0 << 3;

			sp->pcount++;
			if (sp->pcount >= sp->pitch)
			{
				sp->pcount = 0;
				sp->rcount++;
				if (sp->rcount >= sp->repeat)
					sp->playing = 0;
			}
		}
		else
			buffer[i] = 0;

		if (!sp->playing)
		{
			if(sp->fifo_pos == 15)
				sp0250_load_values();
		}
	}
}

void sp0250_update(INT16 *inputs, INT32 sample_len)
{
	if (sample_len != nBurnSoundLen) {
		bprintf(PRINT_ERROR, _T("*** sp0250_update(): call once per frame!\n"));
		return;
	}

	sp0250_frame = (sp0250_clock / CLOCK_DIVIDER) * 100 / nBurnFPS; // this can change.

	UpdateStream(sp0250_frame);

	INT32 samples_from = (INT32)((double)(((sp0250_clock / CLOCK_DIVIDER) * 100) / nBurnFPS) + 0.5);

	for (INT32 j = 0; j < sample_len; j++)
	{
		INT32 k = (samples_from * j) / nBurnSoundLen;

		INT32 rlmono = mixer_buffer[k];

		inputs[0] = BURN_SND_CLIP(inputs[0] + BURN_SND_CLIP(rlmono));
		inputs[1] = BURN_SND_CLIP(inputs[1] + BURN_SND_CLIP(rlmono));
		inputs += 2;
	}

	memset(mixer_buffer, 0, samples_from * sizeof(INT16));
	nCurrentPosition = 0;
}


void sp0250_init(INT32 clock, void (*drqCB)(INT32), INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ)
{
	sp = (sp0250*)BurnMalloc(sizeof(sp0250));
	drq = drqCB;

	sp0250_clock = clock;
	sp0250_frame = (clock / CLOCK_DIVIDER) * 100 / nBurnFPS; // this can change.

	mixer_buffer = (INT16*)BurnMalloc(2 * sizeof(INT16) * nBurnSoundRate);
	memset(mixer_buffer, 0, 2 * sizeof(INT16) * nBurnSoundRate);

	pCPUTotalCycles = pCPUCyclesCB;
	nDACCPUMHZ = nCpuMHZ;

}

void sp0250_exit()
{
	BurnFree(sp);
	BurnFree(mixer_buffer);
}

void sp0250_scan(INT32 nAction, INT32 *)
{
	ScanVar(sp, sizeof(sp0250), "sp0250 core");

	if (nAction & ACB_WRITE) {
		nCurrentPosition = 0;
	}
}

void sp0250_reset()
{
	memset(sp, 0, sizeof(*sp));
	sp->RNG = 1;

	nCurrentPosition = 0;
	drq(ASSERT_LINE);
}

// sp0250_tick() needs to be called "sp0250_frame" times per frame!
void sp0250_tick()
{
	UpdateStream(SyncInternal());
}

void sp0250_write(UINT8 data)
{
	UpdateStream(SyncInternal());

	if (sp->fifo_pos != 15)
	{
		sp->fifo[sp->fifo_pos++] = data;
		if (sp->fifo_pos == 15)
			drq(CLEAR_LINE);
	}
}

