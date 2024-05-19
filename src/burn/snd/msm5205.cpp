// copyright-holders:Aaron Giles
/*
 *   streaming ADPCM driver
 *   by Aaron Giles
 *
 *   Library to transcode from an ADPCM source to raw PCM.
 *   Written by Buffoni Mirko in 08/06/97
 *   References: various sources and documents.
 *
 *   HJB 08/31/98
 *   modified to use an automatically selected oversampling factor
 *   for the current sample rate
 *
 *   01/06/99
 *    separate MSM5205 emulator form adpcm.c and some fix
 *
 *   07/29/12
 *    added basic support for the MSM6585
 */

#include "burnint.h"
#include "msm5205.h"
#include "math.h"
#include "biquad.h"

#define MAX_MSM5205	2

#define SCANLINE_TABLE_SIZE (256 * 8)

static INT32 nNumChips = 0;

struct _MSM5205_state
{
	INT32 data;               /* next adpcm data              */
	INT32 vclk;               /* vclk signal (external mode)  */
	INT32 reset;              /* reset pin signal             */
	INT32 prescaler;          /* prescaler selector S1 and S2 */
	INT32 bitwidth;           /* bit width selector -3B/4B    */
	INT32 signal;             /* current ADPCM signal         */
	INT32 step;               /* current ADPCM step           */
	double volume;
	INT32 output_dir;

	INT32 use_seperate_vols;  /* support custom Taito panning hardware */
	double left_volume;
	double right_volume;

	INT32 lpfilter;
	INT32 dcblock;
	INT16 lastin_r;
	INT16 lastout_r;
	INT16 lastin_l;
	INT16 lastout_l;

	INT32 clock;		  /* clock rate */

	void (*vclk_callback)();  /* VCLK callback              */
	INT32 (*stream_sync)(INT32);
	INT32 select;       	  /* prescaler / bit width selector        */
	INT32 bAdd;
	INT32 streampos;

	INT32 diff_lookup[49*16];
	BIQ biquad;
};

static INT16 *stream[MAX_MSM5205];
static struct _MSM5205_state chips[MAX_MSM5205];
static struct _MSM5205_state *voice;

static void MSM5205_playmode(INT32 chip, INT32 select);

static const INT32 index_shift[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static UINT8 *scanline_table = NULL;

static void ComputeTables(INT32 chip)
{
	voice = &chips[chip];

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
		INT32 stepval = (INT32)(floor (16.0 * pow (11.0 / 10.0, (double)step)));

		/* loop over all nibbles and compute the difference */
		for (nib = 0; nib < 16; nib++)
		{
			voice->diff_lookup[step*16 + nib] = nbl2bit[nib][0] *
				(stepval   * nbl2bit[nib][1] +
				 stepval/2 * nbl2bit[nib][2] +
				 stepval/4 * nbl2bit[nib][3] +
				 stepval/8);
		}
	}
}

static void MSM5205_playmode(INT32 , INT32 select)
{
	static const INT32 prescaler_table[2][4] = { {96,48,64,0}, {160, 40, 80, 20} };
	INT32 prescaler = prescaler_table[(select >> 3) & 1][select & 3];
	INT32 bitwidth = (select & 4) ? 4 : 3;

	voice->prescaler = prescaler;
	voice->bitwidth = bitwidth;
	voice->select = select;

	// clock * prescaler
	// 384000 / 48 -> 8000

	// if cpu is 4000000 (4mhz)
	// check MSM5205 every 4000000 / 8000 -> 500 cycles
}

static void MSM5205StreamUpdate(INT32 chip, INT32 end)
{
	voice = &chips[chip];

	if (!pBurnSoundOut) return;

	UINT32 len = (end) ? nBurnSoundLen : voice->stream_sync((nBurnSoundLen * nBurnFPS) / 100);
	if (len > (UINT32)nBurnSoundLen) len = nBurnSoundLen;
	UINT32 pos = voice->streampos;

	if (pos >= len) return;

	len -= pos;
	voice->streampos = pos + len;

	if (pos == 0) {
		memset (stream[chip], 0, nBurnSoundLen * sizeof(INT16));
	}

	{
		INT16 *buffer = stream[chip];
		buffer += pos;
		
		if(voice->signal)
		{
			INT32 i = 0;

			INT32 volval = (INT32)((voice->signal * 16) * voice->volume);
			INT16 val = BURN_SND_CLIP(volval);
			while (len)
			{
				buffer[i] = val;
				len--; i++;
			}
		} else {
			memset (buffer, 0, sizeof(INT16) * len);
		}
	}
}

static void MSM5205_vclk_callback(INT32 chip)
{
	voice = &chips[chip];

	if(voice->vclk_callback)(*voice->vclk_callback)();

	INT32 new_signal;

	if(voice->reset)
	{
		new_signal = 0;
		voice->step = 0;
	}
	else
	{
		INT32 val = voice->data;
		new_signal = voice->signal + voice->diff_lookup[voice->step * 16 + (val & 15)];
		if (new_signal > 2047) new_signal = 2047;
		else if (new_signal < -2048) new_signal = -2048;
		voice->step += index_shift[val & 7];
		if (voice->step > 48) voice->step = 48;
		else if (voice->step < 0) voice->step = 0;
	}
	/* update when signal changed */
	if( voice->signal != new_signal)
	{
		MSM5205StreamUpdate(chip, 0);
		voice->signal = new_signal;
	}
}

void MSM5205LPFilter(INT32 chip, INT32 enable)
{
	voice = &chips[chip];

    voice->lpfilter = enable;
}

void MSM5205DCBlock(INT32 chip, INT32 enable)
{
	voice = &chips[chip];

    voice->dcblock = enable;
}

static inline INT16 dc_blockR(INT16 sam)
{
    if (!voice->dcblock) return sam;
    INT16 outr = sam - voice->lastin_r + 0.998 * voice->lastout_r;
    voice->lastin_r = sam;
    voice->lastout_r = outr;

    return outr;
}

static inline INT16 dc_blockL(INT16 sam)
{
    if (!voice->dcblock) return sam;
    INT16 outl = sam - voice->lastin_l + 0.998 * voice->lastout_l;
    voice->lastin_l = sam;
    voice->lastout_l = outl;

    return outl;
}

void MSM5205Render(INT32 chip, INT16 *buffer, INT32 len)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205Render called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("MSM5205Render called with invalid chip %x\n"), chip);
#endif

	voice = &chips[chip];
	INT16 *source = stream[chip];

	MSM5205StreamUpdate(chip, 1);

	voice->streampos = 0;
	
	for (INT32 i = 0; i < len; i++) {
		INT32 nLeftSample = 0, nRightSample = 0;
		INT32 source_sample = (voice->lpfilter) ? voice->biquad.filter(source[i]) : source[i];

		if (voice->use_seperate_vols) {
			nLeftSample += (INT32)(source_sample * voice->left_volume);
			nRightSample += (INT32)(source_sample * voice->right_volume);
		} else {
			if ((voice->output_dir & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				nLeftSample += source_sample;
			}
			if ((voice->output_dir & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				nRightSample += source_sample;
			}
		}

		source[i] = 0; // clear dac

		nLeftSample = BURN_SND_CLIP(nLeftSample);
		nRightSample = BURN_SND_CLIP(nRightSample);

		nLeftSample = dc_blockL(nLeftSample);
		nRightSample = dc_blockR(nRightSample);

		if (voice->bAdd) {
			buffer[0] = BURN_SND_CLIP(buffer[0] + nLeftSample);
			buffer[1] = BURN_SND_CLIP(buffer[1] + nRightSample);
		} else {
			buffer[0] = nLeftSample;
			buffer[1] = nRightSample;
		}
		buffer += 2;
	}
}

void MSM5205Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205Reset called without init\n"));
#endif

	for (INT32 chip = 0; chip < MAX_MSM5205; chip++)
	{
		voice = &chips[chip];

		if (stream[chip] == NULL) continue;

		voice->data    = 0;
		voice->vclk    = 0;
		voice->reset   = 0;
		voice->signal  = 0;
		voice->step    = 0;

		MSM5205_playmode(chip,voice->select);
		voice->streampos = 0;

		voice->biquad.reset();
	}
}

void MSM5205Init(INT32 chip, INT32 (*stream_sync)(INT32), INT32 clock, void (*vclk_callback)(), INT32 select, INT32 bAdd)
{
	DebugSnd_MSM5205Initted = 1;
	
	voice = &chips[chip];

	memset (voice, 0, sizeof(_MSM5205_state));

	voice->stream_sync	= stream_sync;
	voice->vclk_callback	= vclk_callback;
	voice->select		= select;
	voice->clock		= clock;
	voice->bAdd		= bAdd;
	voice->volume		= 1.00;
	voice->output_dir = BURN_SND_ROUTE_BOTH;
	
	voice->left_volume = 1.00;
	voice->right_volume = 1.00;
	voice->use_seperate_vols = 0;
	
	float FPSRatio = (float)(6000 - nBurnFPS) / 6000;
	INT32 nSoundLen = nBurnSoundLen + (INT32)((float)nBurnSoundLen * FPSRatio) + 1;
	stream[chip]		= (INT16*)BurnMalloc(nSoundLen * sizeof(INT16));

	if (chip == 0)
		scanline_table = (UINT8*)BurnMalloc(SCANLINE_TABLE_SIZE);

	ComputeTables (chip);
	
	nNumChips = chip;

	voice->biquad.init(FILT_LOWPASS, nBurnSoundRate, 2000.00, 0.929, 0);
}

void MSM5205SetRoute(INT32 chip, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205SetRoute called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("MSM5205SetRoute called with invalid chip %x\n"), chip);
#endif

	voice = &chips[chip];
	voice->volume = nVolume;
	voice->output_dir = nRouteDir;
}

void MSM5205SetLeftVolume(INT32 chip, double nLeftVolume)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205SetLeftVolume called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("MSM5205SetLeftVolume called with invalid chip %x\n"), chip);
#endif

	voice = &chips[chip];
	voice->left_volume = nLeftVolume;
}

void MSM5205SetRightVolume(INT32 chip, double nRightVolume)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205SetRightVolume called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("MSM5205SetRightVolume called with invalid chip %x\n"), chip);
#endif

	voice = &chips[chip];
	voice->right_volume = nRightVolume;
}

void MSM5205SetSeperateVolumes(INT32 chip, INT32 state)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205SetSeperateVolumes called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("MSM5205SetSeperateVolumes called with invalid chip %x\n"), chip);
#endif

	voice = &chips[chip];
	voice->use_seperate_vols = state;
}

void MSM5205Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205Exit called without init\n"));
#endif

	if (!DebugSnd_MSM5205Initted) return;

	for (INT32 chip = 0; chip < MAX_MSM5205; chip++)
	{
		voice = &chips[chip];

		if (stream[chip] == NULL) continue;

		memset (voice, 0, sizeof(_MSM5205_state));

		BurnFree (stream[chip]);

		voice->biquad.exit();
	}

	BurnFree(scanline_table);
	scanline_table = NULL;

	DebugSnd_MSM5205Initted = 0;
	nNumChips = 0;
}

void MSM5205VCLKWrite(INT32 chip, INT32 vclk)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205VCLKWrite called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("MSM5205VCLKWrite called with invalid chip %x\n"), chip);
#endif

	voice = &chips[chip];

	if (voice->prescaler == 0)
	{
		if( voice->vclk != vclk)
		{
			voice->vclk = vclk;
			if( !vclk ) MSM5205_vclk_callback(chip);
		}
	}
}

void MSM5205ResetWrite(INT32 chip, INT32 reset)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205ResetWrite called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("MSM5205ResetWrite called with invalid chip %x\n"), chip);
#endif

	voice = &chips[chip];
	voice->reset = reset;
}

void MSM5205DataWrite(INT32 chip, INT32 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205DataWrite called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("MSM5205DataWrite called with invalid chip %x\n"), chip);
#endif

	voice = &chips[chip];

	if( voice->bitwidth == 4)
		voice->data = data & 0x0f;
	else
		voice->data = (data & 0x07)<<1;
}

void MSM5205PlaymodeWrite(INT32 chip, INT32 select)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205PlaymodeWrite called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("MSM5205PlaymodeWrite called with invalid chip %x\n"), chip);
#endif

	voice = &chips[chip];
	MSM5205_playmode(chip,select);
}


void MSM5205NewFrame(INT32 chip, INT32 cpu_speed, INT32 interleave)
{
	INT32 MSMCalcdInterleave = MSM5205CalcInterleave(chip, cpu_speed);
	INT32 LastIdx = -1;
	INT32 Idx = 0;

	if (interleave >= SCANLINE_TABLE_SIZE) {
		bprintf(PRINT_ERROR, _T("*** MSM5205 error: interleave too large (%d), increase SCANLINE_TABLE_SIZE!\n"), interleave);
		return;
	}

	for (INT32 i = 0; i < interleave; i++)
	{
		Idx = (INT32)round(((double)MSMCalcdInterleave / (double)interleave) * (double)i);

		if (Idx != LastIdx) {
			scanline_table[i] = 1;
		} else scanline_table[i] = 0;
		LastIdx = Idx;
	}
}

void MSM5205UpdateScanline(INT32 scanline)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205UpdateScanline called without init\n"));
#endif
	if (scanline_table[scanline]) {
		MSM5205Update();
	}
}

void MSM5205Update()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205Update called without init\n"));
#endif

	for (INT32 chip = 0; chip < MAX_MSM5205; chip++)
	{
		voice = &chips[chip];

		if (voice->prescaler) {
			MSM5205_vclk_callback(chip);
		} else {
			if (stream[chip]) {
				MSM5205StreamUpdate(chip, 0);
			}
		}
	}
}

// see MSM5205_playmode for a more in-depth explanation of this
INT32 MSM5205CalcInterleave(INT32 chip, INT32 cpu_speed)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205CalcInterleave called without init\n"));
	if (chip > nNumChips) bprintf(PRINT_ERROR, _T("MSM5205CalcInterleave called with invalid chip %x\n"), chip);
#endif

	static const INT32 table[2][4] = { {96, 48, 64, 0}, {160, 40, 80, 20} };

	voice = &chips[chip];

	if ((voice->select & 3) == 3) {
		return 133;  // (usually...)
	}

	INT32 ret = cpu_speed / (cpu_speed / (voice->clock / table[(voice->select >> 3) & 1][voice->select & 3]));
	
	return ret / (nBurnFPS / 100);
}

void MSM5205Scan(INT32 nAction, INT32 *pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_MSM5205Initted) bprintf(PRINT_ERROR, _T("MSM5205Scan called without init\n"));
#endif

	if (pnMin != NULL) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_DRIVER_DATA) {
		for (INT32 chip = 0; chip < MAX_MSM5205; chip++) {
			voice = &chips[chip];

			SCAN_VAR(voice->data);

			SCAN_VAR(voice->vclk);
			SCAN_VAR(voice->reset);
			SCAN_VAR(voice->prescaler);
			SCAN_VAR(voice->bitwidth);
			SCAN_VAR(voice->signal);
			SCAN_VAR(voice->step);
			SCAN_VAR(voice->volume);
			SCAN_VAR(voice->left_volume);
			SCAN_VAR(voice->right_volume);
			SCAN_VAR(voice->clock);
			SCAN_VAR(voice->select);
			SCAN_VAR(voice->streampos);
			SCAN_VAR(voice->lastin_r);
			SCAN_VAR(voice->lastout_r);
			SCAN_VAR(voice->lastin_l);
			SCAN_VAR(voice->lastout_l);
		}
	}
}
