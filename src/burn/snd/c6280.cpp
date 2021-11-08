/*
	HuC6280 sound chip emulator
	by Charles MacDonald
	E-mail: cgfm2@hotmail.com
	WWW: http://cgfm2.emuviews.com

	Thanks to:

	- Paul Clifford for his PSG documentation.
	- Richard Bannister for the TGEmu-specific sound updating code.
	- http://www.uspto.gov for the PSG patents.
	- All contributors to the tghack-list.

	Changes:

	(03/30/2003)
	- Removed TGEmu specific code and added support functions for MAME.
	- Modified setup code to handle multiple chips with different clock and
	  volume settings.

	Missing features / things to do:

	- Add LFO support. But do any games actually use it?

	- Add shared index for waveform playback and sample writes. Almost every
	  game will reset the index prior to playback so this isn't an issue.

	- While the noise emulation is complete, the data for the pseudo-random
	  bitstream is calculated by machine.rand() and is not a representation of what
	  the actual hardware does.

	For some background on Hudson Soft's C62 chipset:

	- http://www.hudsonsoft.net/ww/about/about.html
	- http://www.hudson.co.jp/corp/eng/coinfo/history.html

	Legal information:

	Copyright Charles MacDonald

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

	Ported from MAME 0.236, improvements by cam900 and Charles MacDonald (Noise RE)
*/

#include "burnint.h"
#include "h6280_intf.h"
#include "c6280.h"
#include "stream.h"
#include "bitswap.h"
#include <stddef.h>
#include "math.h"

typedef struct {
	UINT16 frequency;
	UINT8 control;
	UINT8 balance;
	UINT8 waveform[32];
	UINT8 index;
	INT16 dda;
	UINT8 noise_control;
	INT32 noise_counter;
	UINT32 noise_frequency;
	UINT32 noise_seed;
	INT32 tick;
} t_channel;

typedef struct {
	UINT8 select;
	UINT8 balance;
	UINT8 lfo_frequency;
	UINT8 lfo_control;
	t_channel channel[8];

	INT16 volume_table[32];
	UINT32 noise_freq_tab[32];
	UINT32 wave_freq_tab[4096];
	INT32 bAdd;
	double gain[2];
	INT32 output_dir[2];
} c6280_t;

enum { RENDERER_LQ = 0, RENDERER_HQ };

static Stream stream;
static INT32 AddToStream;
static INT32 renderer;
static c6280_t chip[1];

static INT32 lostsunh_hack;

static void c6280_stream_update(INT16 **streams, INT32 samples); // forward
static void c6280_stream_update_OLD(INT16 **streams, INT32 samples); // forward

void c6280_reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_C6280Initted) bprintf(PRINT_ERROR, _T("c6280_reset called without init\n"));
#endif

	c6280_t *p = &chip[0];

	p->select = 0;
	p->balance = 0;
	p->lfo_frequency = 0;
	p->lfo_control = 0;
	memset (p->channel, 0, sizeof(p->channel));
	p->channel[4].noise_seed = 1;
	p->channel[5].noise_seed = 1;
}

void c6280_set_renderer(INT32 new_version)
{
	stream.exit();

	if (new_version) {
		renderer = RENDERER_HQ;
		stream.init(3579545, nBurnSoundRate, 2, AddToStream, c6280_stream_update);
	} else {
		renderer = RENDERER_LQ;
		stream.init(96000, nBurnSoundRate, 2, AddToStream, c6280_stream_update_OLD);
	}
	stream.set_buffered(h6280TotalCycles, 7159090);

	bprintf(0, _T("C6280 Renderer set: "));
	switch (renderer) {
		case RENDERER_LQ: bprintf(0, _T("LQ\n")); break;
		case RENDERER_HQ: bprintf(0, _T("HQ\n")); break;
	}
}

void c6280_init(INT32 clk, INT32 bAdd, INT32 lostsunh_hf_hack)
{
	DebugSnd_C6280Initted = 1;

	INT32 i;
	double step;
	c6280_t *p = &chip[0];

	AddToStream = bAdd;

	/* Loudest volume level for table */
	double level = 65535.0 / 6.0 / 32.0;

	/* Clear context */
	memset(p, 0, sizeof(c6280_t));

	/* Make waveform frequency table */
	for(i = 0; i < 4096; i += 1)
	{
		step = ((clk / (96000 * 1.0000)) * 4096) / (i+1);
		p->wave_freq_tab[(1 + i) & 0xFFF] = (UINT32)step;
	}

	lostsunh_hack = lostsunh_hf_hack;

	if (lostsunh_hf_hack) {
		bprintf(0, _T("C6280 pce_lostsunh soundhack/fix enabled.\n"));
		// bouken danshaku don - the lost sunheart (japan)
		// incorrectly disables channels using frequency of 1
		// this hack prevents the game from making a high-pitched whine
	}

	/* Make noise frequency table */
	for(i = 0; i < 32; i += 1)
	{
		step = ((clk / (96000 * 1.0000)) * 32) / (i+1);
		p->noise_freq_tab[i] = (UINT32)step;
	}

	/* Make volume table */
	/* PSG has 48dB volume range spread over 32 steps */
	step = 48.0 / 32.0;
	for(i = 0; i < 30; i++)
	{
		p->volume_table[i] = (UINT16)level;
		level /= pow(10.0, step / 20.0);
	}
	p->volume_table[30] = p->volume_table[31] = 0;

	p->bAdd = bAdd;
	p->gain[BURN_SND_C6280_ROUTE_1] = 1.00;
	p->gain[BURN_SND_C6280_ROUTE_2] = 1.00;
	p->output_dir[BURN_SND_C6280_ROUTE_1] = BURN_SND_ROUTE_LEFT;
	p->output_dir[BURN_SND_C6280_ROUTE_2] = BURN_SND_ROUTE_RIGHT;

	bprintf(0, _T("clock is %d, sndrate is %d\n"), clk, nBurnSoundRate);
	c6280_set_renderer(0);
}

void c6280_set_route(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_C6280Initted) bprintf(PRINT_ERROR, _T("c6280_set_route called without init\n"));
#endif

	c6280_t *p = &chip[0];
	
	p->gain[nIndex] = nVolume;
	p->output_dir[nIndex] = nRouteDir;

	stream.set_volume(nVolume);
}

void c6280_exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_C6280Initted) bprintf(PRINT_ERROR, _T("c6280_exit called without init\n"));
#endif

	stream.exit();

	DebugSnd_C6280Initted = 0;
}

static void c6280_stream_update_OLD(INT16 **streams, INT32 samples)
{
	c6280_t *p = &chip[0];

	INT32 i;

	INT32 lmal = (p->balance >> 4) & 0x0F;
	INT32 rmal = (p->balance >> 0) & 0x0F;

	/* Clear buffer */
	memset (streams[0], 0, samples * sizeof(INT16));
	memset (streams[1], 0, samples * sizeof(INT16));

	for(INT32 ch = 0; ch < 6; ch++)
	{
		/* Only look at enabled channels */
		if(p->channel[ch].control & 0x80)
		{
			const UINT8 lal = (p->channel[ch].balance >> 4) & 0x0f;
			const UINT8 ral = (p->channel[ch].balance >> 0) & 0x0f;
			const UINT8 al  = (p->channel[ch].control >> 1) & 0x0f; // only high 4 bit is affected to calculating volume, low 1 bit is independent

			// verified from both patent and manual
			int vll = (0xf - lmal) + (0xf - al) + (0xf - lal);
			if (vll > 0xf) vll = 0xf;

			int vlr = (0xf - rmal) + (0xf - al) + (0xf - ral);
			if (vlr > 0xf) vlr = 0xf;

			vll = p->volume_table[(vll << 1) | (~p->channel[ch].control & 1)];
			vlr = p->volume_table[(vlr << 1) | (~p->channel[ch].control & 1)];

			/* Check channel mode */
			if((ch >= 4) && (p->channel[ch].noise_control & 0x80))
			{
				/* Noise mode */
				UINT32 step = p->noise_freq_tab[(p->channel[ch].noise_control & 0x1F) ^ 0x1F];
				for(i = 0; i < samples; i++)
				{
					//static INT32 data = 0;
					INT16 data = BIT(p->channel[ch].noise_seed, 0) ? 0x1f : 0;
					p->channel[ch].noise_counter += step;
					if(p->channel[ch].noise_counter >= 0x800)
					{
						const UINT32 seed = p->channel[ch].noise_seed;
						// based on Charles MacDonald's research
						p->channel[ch].noise_seed = (seed >> 1) | ((BIT(seed, 0) ^ BIT(seed, 1) ^ BIT(seed, 11) ^ BIT(seed, 12) ^ BIT(seed, 17)) << 17);
					}
					p->channel[ch].noise_counter &= 0x7FF;
					streams[0][i] += (INT16)(vll * (data - 16));
					streams[1][i] += (INT16)(vlr * (data - 16));
				}
			}
			else
			if(p->channel[ch].control & 0x40)
			{
				/* DDA mode */
				for(i = 0; i < samples; i++)
				{
					streams[0][i] += (INT16)(vll * (p->channel[ch].dda - 16));
					streams[1][i] += (INT16)(vlr * (p->channel[ch].dda - 16));
				}
			}
			else
			{
				/* Waveform mode */
				if (lostsunh_hack && p->channel[ch].frequency == 1)
					continue;
				UINT32 step = p->wave_freq_tab[p->channel[ch].frequency];
				for(i = 0; i < samples; i++)
				{
					INT32 offset = (p->channel[ch].tick >> 12) & 0x1F;
					p->channel[ch].tick += step;
					p->channel[ch].tick &= 0x1FFFF;
					INT16 data = p->channel[ch].waveform[offset];
					streams[0][i] += (INT16)(vll * (data - 16));
					streams[1][i] += (INT16)(vlr * (data - 16));
				}
			}
		}
	}
}

static void c6280_stream_update(INT16 **streams, INT32 samples)
{
	c6280_t *p = &chip[0];

	INT32 lmal = (p->balance >> 4) & 0x0F;
	INT32 rmal = (p->balance >> 0) & 0x0F;

	/* Clear buffer */
	memset (streams[0], 0, samples * sizeof(INT16));
	memset (streams[1], 0, samples * sizeof(INT16));

	for (int ch = 0; ch < 6; ch++)
	{
		t_channel *chan = &p->channel[ch];
		/* Only look at enabled channels */
		if (chan->control & 0x80)
		{
			const UINT8 lal = (chan->balance >> 4) & 0x0f;
			const UINT8 ral = (chan->balance >> 0) & 0x0f;
			const UINT8 al  = (chan->control >> 1) & 0x0f; // only high 4 bit is affected to calculating volume, low 1 bit is independent

			// verified from both patent and manual
			int vll = (0xf - lmal) + (0xf - al) + (0xf - lal);
			if (vll > 0xf) vll = 0xf;

			int vlr = (0xf - rmal) + (0xf - al) + (0xf - ral);
			if (vlr > 0xf) vlr = 0xf;

			vll = p->volume_table[(vll << 1) | (~chan->control & 1)];
			vlr = p->volume_table[(vlr << 1) | (~chan->control & 1)];

			/* Check channel mode */
			if ((ch >= 4) && (chan->noise_control & 0x80))
			{
				/* Noise mode */
				const UINT32 step = (chan->noise_control & 0x1f) ^ 0x1f;
				for (int i = 0; i < samples; i += 1)
				{
					INT16 data = BIT(chan->noise_seed, 0) ? 0x1f : 0;
					chan->noise_counter--;
					if (chan->noise_counter <= 0)
					{
						chan->noise_counter = step << 6; // 32 * 2
						const UINT32 seed = chan->noise_seed;
						// based on Charles MacDonald's research
						chan->noise_seed = (seed >> 1) | ((BIT(seed, 0) ^ BIT(seed, 1) ^ BIT(seed, 11) ^ BIT(seed, 12) ^ BIT(seed, 17)) << 17);
					}
					streams[0][i] += (INT16)(vll * (data - 16));
					streams[1][i] += (INT16)(vlr * (data - 16));
				}
			}
			else
			if (chan->control & 0x40)
			{
				/* DDA mode */
				for (int i = 0; i < samples; i++)
				{
					streams[0][i] += (INT16)(vll * (chan->dda - 16));
					streams[1][i] += (INT16)(vlr * (chan->dda - 16));
				}
			}
			else
			{
				if ((p->lfo_control & 3) && (ch < 2))
				{
					if (ch == 0) // CH 0 only, CH 1 is muted
					{
						/* Waveform mode with LFO */
						t_channel *lfo_srcchan = &p->channel[1];
						t_channel *lfo_dstchan = &p->channel[0];
						const UINT16 lfo_step = lfo_srcchan->frequency ? lfo_srcchan->frequency : 0x1000;
						for (int i = 0; i < samples; i += 1)
						{
							INT32 step = lfo_dstchan->frequency ? lfo_dstchan->frequency : 0x1000;
							if (p->lfo_control & 0x80) // reset LFO
							{
								lfo_srcchan->tick = lfo_step * p->lfo_frequency;
								lfo_srcchan->index = 0;
							}
							else
							{
								const INT16 lfo_data = lfo_srcchan->waveform[lfo_srcchan->index];
								lfo_srcchan->tick--;
								if (lfo_srcchan->tick <= 0)
								{
									lfo_srcchan->tick = lfo_step * p->lfo_frequency; // verified from manual
									lfo_srcchan->index = (lfo_srcchan->index + 1) & 0x1f;
								}
								step += ((lfo_data - 16) << (((p->lfo_control & 3)-1)<<1)); // verified from manual
							}
							const INT16 data = lfo_dstchan->waveform[lfo_dstchan->index];
							lfo_dstchan->tick--;
							if (lfo_dstchan->tick <= 0)
							{
								lfo_dstchan->tick = step;
								lfo_dstchan->index = (lfo_dstchan->index + 1) & 0x1f;
							}
							streams[0][i] += INT16(vll * (data - 16));
							streams[1][i] += INT16(vlr * (data - 16));
						}
					}
				}
				else
				{
					/* Waveform mode */
					if (lostsunh_hack && chan->frequency == 1)
						continue;
					const UINT32 step = chan->frequency ? chan->frequency : 0x1000;
					for (int i = 0; i < samples; i += 1)
					{
						const INT16 data = chan->waveform[chan->index];
						chan->tick--;
						if (chan->tick <= 0)
						{
							chan->tick = step;
							chan->index = (chan->index + 1) & 0x1f;
						}
						streams[0][i] += INT16(vll * (data - 16));
						streams[1][i] += INT16(vlr * (data - 16));
					}
				}
			}
		}
	}
}

static void c6280_write_internal(INT32 offset, INT32 data)
{
	c6280_t *p = &chip[0];
	t_channel *q = &p->channel[p->select];

	stream.update();

	switch(offset & 0x0F)
	{
		case 0x00: /* Channel select */
			p->select = data & 0x07;
			break;

		case 0x01: /* Global balance */
			p->balance  = data;
			break;

		case 0x02: /* Channel frequency (LSB) */
			q->frequency = (q->frequency & 0x0F00) | data;
			break;

		case 0x03: /* Channel frequency (MSB) */
			q->frequency = (q->frequency & 0x00FF) | ((data << 8) & 0x0f00);
			break;

		case 0x04: /* Channel control (key-on, DDA mode, volume) */

			/* 1-to-0 transition of DDA bit resets waveform index */
			if((q->control & 0x40) && ((data & 0x40) == 0))
			{
				q->index = 0;
			}
			if(((q->control & 0x80) == 0) && (data & 0x80))
			{
				switch (renderer) {
					case RENDERER_LQ: q->tick = p->wave_freq_tab[q->frequency]; break;
					case RENDERER_HQ: q->tick = q->frequency; break;
				}
			}
			q->control = data;
			break;

		case 0x05: /* Channel balance */
			q->balance = data;
			break;

		case 0x06: /* Channel waveform data */

			switch(q->control & 0x40)
			{
				case 0x00:
					q->waveform[q->index & 0x1F] = data & 0x1F;
					if (!(q->control & 0x80)) // TODO : wave pointer is increased at writing data when sound playback is off??
						q->index = (q->index + 1) & 0x1F;
					break;

				case 0x40:
					q->dda = data & 0x1F;
					break;
			}

			break;

		case 0x07: /* Noise control (enable, frequency) */
			q->noise_control = data;
			break;

		case 0x08: /* LFO frequency */
			p->lfo_frequency = data;
			break;

		case 0x09: /* LFO control (enable, mode) */
			p->lfo_control = data;
			break;

		default:
			break;
	}
}

void c6280_update(INT16 *output, INT32 samples_len)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_C6280Initted) bprintf(PRINT_ERROR, _T("c6280_update called without init\n"));
#endif

	if (samples_len != nBurnSoundLen) {
		bprintf(0, _T("c352_update(): once per frame, please!\n"));
		return;
	}

	stream.render(output, samples_len);
}

UINT8 c6280_read()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_C6280Initted) bprintf(PRINT_ERROR, _T("c6280_read called without init\n"));
#endif

	return h6280io_get_buffer();
}

void c6280_write(UINT8 offset, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_C6280Initted) bprintf(PRINT_ERROR, _T("c6280_write called without init\n"));
#endif

	h6280io_set_buffer(data);
	c6280_write_internal(offset, data);
}

void c6280_scan(INT32 nAction, INT32 *pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_C6280Initted) bprintf(PRINT_ERROR, _T("c6280_scan called without init\n"));
#endif

	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_DRIVER_DATA) {
		c6280_t *p = &chip[0];

		memset(&ba, 0, sizeof(ba));
		ba.Data		= p;
		ba.nLen		= STRUCT_SIZE_HELPER(c6280_t, channel);
		ba.szName	= "c6280 Chip #0";
		BurnAcb(&ba);
	}
}
