/***************************************************************************

    Konami 051649 - SCC1 sound as used in Haunted Castle, City Bomber

    This file is pieced together by Bryan McPhail from a combination of
    Namco Sound, Amuse by Cab, Haunted Castle schematics and whoever first
    figured out SCC!

    The 051649 is a 5 channel sound generator, each channel gets it's
    waveform from RAM (32 bytes per waveform, 8 bit signed data).

    This sound chip is the same as the sound chip in some Konami
    megaROM cartridges for the MSX. It is actually well researched
    and documented:

        http://www.msxnet.org/tech/scc

    Thanks to Sean Young (sean@mess.org) for some bugfixes.

    K052539 is equivalent to this chip except channel 5 does not share
    waveforms with channel 4.

***************************************************************************/

#include "burnint.h"
#include "k051649.h"
#include "stream.h"

static Stream stream;

/* this structure defines the parameters for a channel */
typedef struct
{
	UINT32 counter;
	INT32 clock;
	INT32 frequency;
	INT32 volume;
	INT32 key;
	INT8 waveform[32];		/* 19991207.CAB */
} k051649_sound_channel;

typedef struct _k051649_state k051649_state;
struct _k051649_state
{
	k051649_sound_channel channel_list[5];

	UINT8 test;

	/* global sound parameters */
	INT32 mclock,rate;
	double gain;
	INT32 output_dir;

	/* mixer tables and internal buffers */
	INT16 *mixer_table;
	INT16 *mixer_lookup;
	INT16 *mixer_buffer;
};

static k051649_state Chips[1]; // ok? (one is good enough)
static k051649_state *info;

/* build a table to divide by the number of voices */
static void make_mixer_table(INT32 voices)
{
	INT32 count = voices * 256;
	INT32 i;
	INT32 gain = 8;

	/* allocate memory */
	info->mixer_table = (INT16 *)BurnMalloc(512 * voices * sizeof(INT16));

	/* find the middle of the table */
	info->mixer_lookup = info->mixer_table + (256 * voices);

	/* fill in the table - 16 bit case */
	for (i = 0; i < count; i++)
	{
		INT32 val = i * gain * 16 / voices;
		if (val > 32767) val = 32767;
		info->mixer_lookup[ i] = val;
		info->mixer_lookup[-i] = -val;
	}
}

/* generate sound to the mix buffer */
static void update_INT(INT16 **streams, INT32 samples_len)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649Update called without init\n"));
#endif

	info = &Chips[0];
	k051649_sound_channel *voice=info->channel_list;
	INT32 i,v,j;

	/* zap the contents of the mixer buffer */
	memset(info->mixer_buffer, 0, samples_len * sizeof(INT16));

	for (j=0; j<5; j++)
	{
		// channel is halted for freq < 9
		if (voice[j].frequency > 8)
		{
			v=voice[j].volume * voice[j].key;
			int a = voice[j].counter;
			int c = voice[j].clock;
			const int step = voice[j].frequency;

			/* add our contribution */
			for (i = 0; i < samples_len; i++)
			{
				c += 32;
				while (c > step)
				{
					a = (a + 1) & 0x1f;
					c -= step+1;
				}
				info->mixer_buffer[i] += (voice[j].waveform[a] * v) >> 3;
			}

			// update the counter for this voice
			voice[j].counter = a;
			voice[j].clock = c;
		}
	}

	INT16 *mixer = streams[0];

	for (j = 0; j < samples_len; j++)
	{
		mixer[j] = info->mixer_lookup[info->mixer_buffer[j]];
	}
}

void K051649Update(INT16 *pBuf, INT32 samples)
{
	if (samples != nBurnSoundLen) {
		bprintf(0, _T("K051649Update(): once per frame, please!\n"));
		return;
	}

	stream.render(pBuf, samples);
}

void K051649Init(INT32 clock)
{
	DebugSnd_K051649Initted = 1;

	info = &Chips[0];

	/* get stream channels */
	info->rate = clock/16;
	info->mclock = clock;
	info->gain = 1.00;
	info->output_dir = BURN_SND_ROUTE_BOTH;
	
	stream.init(info->rate, nBurnSoundRate, 1, 1, update_INT);
    stream.set_volume(1.00);

	/* allocate a buffer to mix into - 1 second's worth should be more than enough */
	info->mixer_buffer = (INT16 *)BurnMalloc(2 * sizeof(INT16) * info->rate);
	memset(info->mixer_buffer, 0, 2 * sizeof(INT16) * info->rate);
	
	/* build the mixer table */
	make_mixer_table(5);

	K051649Reset(); // clear things on init.
}

void K051649SetSync(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz)
{
	stream.set_buffered(pCPUCyclesCB, nCPUMhz);
}

void K051649SetRoute(double nVolume, INT32 nRouteDir)
{
	info = &Chips[0];
	
	info->gain = nVolume;
	info->output_dir = nRouteDir;

	stream.set_volume(nVolume);
}

void K051649Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649Exit called without init\n"));
#endif

	if (!DebugSnd_K051649Initted) return;

	info = &Chips[0];

	BurnFree (info->mixer_buffer);
	BurnFree (info->mixer_table);
	
	stream.exit();
	
	DebugSnd_K051649Initted = 0;
}

void K051649Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649Reset called without init\n"));
#endif

	info = &Chips[0];
	k051649_sound_channel *voice = info->channel_list;
	INT32 i;

	/* reset all the voices */
	for (i = 0; i < 5; i++) {
		voice[i].frequency = 0;
		voice[i].volume = 0xf;
		voice[i].key = 0;
		voice[i].counter = 0;
		memset(&voice[i].waveform, 0, 32);
	}
}

void K051649Scan(INT32 nAction, INT32 *pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649Scan called without init\n"));
#endif

	struct BurnArea ba;

	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return;
	}
	
	if (pnMin != NULL) {
		*pnMin = 0x029705;
	}

	memset(&ba, 0, sizeof(ba));
	ba.Data		= &info->channel_list;
	ba.nLen		= sizeof(k051649_sound_channel) * 5;
	ba.nAddress = 0;
	ba.szName	= "K051649 Channel list";
	BurnAcb(&ba);
}

/********************************************************************************/

void K051649WaveformWrite(INT32 offset, INT32 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649WaveformWrite called without init\n"));
#endif

	// waveram is read-only?
	if (info->test & 0x40 || (info->test & 0x80 && offset >= 0x60))
		return;


	info = &Chips[0];
	info->channel_list[offset>>5].waveform[offset&0x1f]=data;
	/* SY 20001114: Channel 5 shares the waveform with channel 4 */
	if (offset >= 0x60)
		info->channel_list[4].waveform[offset&0x1f]=data;
}

UINT8 K051649WaveformRead(INT32 offset)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649WaveformRead called without init\n"));
#endif

	info = &Chips[0];

	// test-register bits 6/7 expose the internal counter
	if (info->test & 0xc0)
	{
		stream.update();

		if (offset >= 0x60)
			offset += info->channel_list[3 + (info->test >> 6 & 1)].counter;
		else if (info->test & 0x40)
			offset += info->channel_list[offset >> 5].counter;
	}
	return info->channel_list[offset>>5].waveform[offset&0x1f];
}

/* SY 20001114: Channel 5 doesn't share the waveform with channel 4 on this chip */
void K052539WaveformWrite(INT32 offset, INT32 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K052539WaveformWrite called without init\n"));
#endif

	info = &Chips[0];

	info->channel_list[offset>>5].waveform[offset&0x1f]=data;
}

void K051649VolumeWrite(INT32 offset, INT32 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649VolumeWrite called without init\n"));
#endif

	info = &Chips[0];

	info->channel_list[offset&0x7].volume=data&0xf;
}

void K051649FrequencyWrite(INT32 offset, INT32 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649FrequencyWrite called without init\n"));
#endif
	INT32 freq_hi = offset & 1;
	offset >>= 1;

	info = &Chips[0];

	if (info->test & 0x20) {
		info->channel_list[offset].clock = 0;
		info->channel_list[offset].counter = 0;
	} else if (info->channel_list[offset].frequency < 9) {
		info->channel_list[offset].clock = 0;
	}

	// update frequency
	if (freq_hi)
		info->channel_list[offset].frequency = (info->channel_list[offset].frequency & 0x0ff) | (data << 8 & 0xf00);
	else
		info->channel_list[offset].frequency = (info->channel_list[offset].frequency & 0xf00) | data;
}

void K051649KeyonoffWrite(INT32 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649KeyonoffWrite called without init\n"));
#endif

	info = &Chips[0];
	info->channel_list[0].key=(data&1) ? 1 : 0;
	info->channel_list[1].key=(data&2) ? 1 : 0;
	info->channel_list[2].key=(data&4) ? 1 : 0;
	info->channel_list[3].key=(data&8) ? 1 : 0;
	info->channel_list[4].key=(data&16) ? 1 : 0;
}

UINT8 K051649Read(INT32 offset)
{
	stream.update();

	offset &= 0xff;

	if (offset < 0x80) {
		return K051649WaveformRead(offset);
	}

	offset &= ~0x10; // mirror

	if (offset >= 0xe0) { // test register
		info = &Chips[0];
		info->test = 0xff;
		return 0xff;
	}

	return 0;
}

void K051649Write(INT32 offset, UINT8 data)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649Write called without init\n"));
#endif

	stream.update();
	offset &= 0xff;

	if ((offset & 0x80) == 0x00) {
		K051649WaveformWrite(offset & 0x7f, data);
		return;
	}

	offset &= ~0x10; // mirror

	if (offset >= 0x80 && offset <= 0x89) { // freq register
		K051649FrequencyWrite(offset & 0xf, data);
		return;
	}

	if (offset >= 0x8a && offset <= 0x8e) { // volume register
		K051649VolumeWrite(offset - 0x8a, data);
		return;
	}

	if (offset == 0x8f) {
		K051649KeyonoffWrite(data);
		return;
	}

	if (offset >= 0xe0) { // test register
		info = &Chips[0];
		info->test = data;
		return;
	}
}
