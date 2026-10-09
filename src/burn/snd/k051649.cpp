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

    K052539 (SCC+) is equivalent to this chip except channel 5 does not share
    waveforms with channel 4.

***************************************************************************/

#include "burnint.h"
#include "k051649.h"
#include "stream.h"

#define MAX_K051649_CHIPS	2

static Stream stream[MAX_K051649_CHIPS];

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

	/* internal mix buffer */
	INT16 *mixer_buffer;
};

static k051649_state Chips[MAX_K051649_CHIPS];
static UINT8 chip_initted[MAX_K051649_CHIPS] = { 0, 0 };

/* mixer table - identical for every chip, so it is shared */
static INT16 *mixer_table = NULL;
static INT16 *mixer_lookup = NULL;

#if defined FBNEO_DEBUG
#define CHECK_CHIP(func, ret) \
	if (nChip < 0 || nChip >= MAX_K051649_CHIPS || !chip_initted[nChip]) { \
		bprintf(PRINT_ERROR, _T("%s called with invalid / uninitialized chip %d\n"), _T(func), nChip); \
		return ret; \
	}
#else
#define CHECK_CHIP(func, ret)
#endif

/* build a table to divide by the number of voices */
static void make_mixer_table(INT32 voices)
{
	INT32 count = voices * 256;
	INT32 i;
	INT32 gain = 8;

	/* allocate memory */
	mixer_table = (INT16 *)BurnMalloc(512 * voices * sizeof(INT16));

	/* find the middle of the table */
	mixer_lookup = mixer_table + (256 * voices);

	/* fill in the table - 16 bit case */
	for (i = 0; i < count; i++)
	{
		INT32 val = i * gain * 16 / voices;
		if (val > 32767) val = 32767;
		mixer_lookup[ i] = val;
		mixer_lookup[-i] = -val;
	}
}

/* generate sound to the mix buffer */
static void update_INT(INT32 nChip, INT16 **streams, INT32 samples_len)
{
	k051649_state *info = &Chips[nChip];
	k051649_sound_channel *voice = info->channel_list;
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
		mixer[j] = mixer_lookup[info->mixer_buffer[j]];
	}
}

// stream callbacks
static void update_INT0(INT16 **streams, INT32 samples_len) { update_INT(0, streams, samples_len); }
static void update_INT1(INT16 **streams, INT32 samples_len) { update_INT(1, streams, samples_len); }

static void (*update_cb[MAX_K051649_CHIPS])(INT16 **, INT32) = { update_INT0, update_INT1 };

void K051649Update(INT16 *pBuf, INT32 samples)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649Update called without init\n"));
#endif

	if (samples != nBurnSoundLen) {
		bprintf(0, _T("K051649Update(): once per frame, please!\n"));
		return;
	}

	for (INT32 i = 0; i < MAX_K051649_CHIPS; i++) {
		if (chip_initted[i]) stream[i].render(pBuf, samples);
	}
}

void K051649Init(INT32 nChip, INT32 clock)
{
	if (nChip < 0 || nChip >= MAX_K051649_CHIPS) {
		bprintf(PRINT_ERROR, _T("K051649Init(): chip %d out of range (max %d)\n"), nChip, MAX_K051649_CHIPS);
		return;
	}

	DebugSnd_K051649Initted = 1;

	k051649_state *info = &Chips[nChip];
	memset(info, 0, sizeof(k051649_state));

	/* get stream channels */
	info->rate = clock/16;
	info->mclock = clock;
	info->gain = 1.00;
	info->output_dir = BURN_SND_ROUTE_BOTH;

	stream[nChip].init(info->rate, nBurnSoundRate, 1, 1, update_cb[nChip]);
	stream[nChip].set_volume(1.00);

	/* allocate a buffer to mix into - 1 second's worth should be more than enough */
	info->mixer_buffer = (INT16 *)BurnMalloc(2 * sizeof(INT16) * info->rate);
	memset(info->mixer_buffer, 0, 2 * sizeof(INT16) * info->rate);

	/* build the (shared) mixer table */
	if (mixer_table == NULL) make_mixer_table(5);

	chip_initted[nChip] = 1;

	K051649Reset(nChip); // clear things on init.
}

void K051649Init(INT32 clock)
{
	K051649Init(0, clock);
}

void K051649SetSync(INT32 nChip, INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz)
{
	CHECK_CHIP("K051649SetSync", )

	stream[nChip].set_buffered(pCPUCyclesCB, nCPUMhz);
}

void K051649SetSync(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz)
{
	K051649SetSync(0, pCPUCyclesCB, nCPUMhz);
}

void K051649SetRoute(INT32 nChip, double nVolume, INT32 nRouteDir)
{
	CHECK_CHIP("K051649SetRoute", )

	k051649_state *info = &Chips[nChip];

	info->gain = nVolume;
	info->output_dir = nRouteDir;

	stream[nChip].set_volume(nVolume);
}

void K051649SetRoute(double nVolume, INT32 nRouteDir)
{
	K051649SetRoute(0, nVolume, nRouteDir);
}

void K051649Exit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649Exit called without init\n"));
#endif

	if (!DebugSnd_K051649Initted) return;

	for (INT32 i = 0; i < MAX_K051649_CHIPS; i++) {
		if (!chip_initted[i]) continue;

		BurnFree (Chips[i].mixer_buffer);
		stream[i].exit();

		chip_initted[i] = 0;
	}

	BurnFree (mixer_table);
	mixer_lookup = NULL;

	DebugSnd_K051649Initted = 0;
}

void K051649Reset(INT32 nChip)
{
	CHECK_CHIP("K051649Reset", )

	k051649_sound_channel *voice = Chips[nChip].channel_list;

	/* reset all the voices */
	for (INT32 i = 0; i < 5; i++) {
		voice[i].frequency = 0;
		voice[i].volume = 0xf;
		voice[i].key = 0;
		voice[i].counter = 0;
		memset(&voice[i].waveform, 0, 32);
	}
}

void K051649Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649Reset called without init\n"));
#endif

	for (INT32 i = 0; i < MAX_K051649_CHIPS; i++) {
		if (chip_initted[i]) K051649Reset(i);
	}
}

void K051649Scan(INT32 nAction, INT32 *pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_K051649Initted) bprintf(PRINT_ERROR, _T("K051649Scan called without init\n"));
#endif

	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return;
	}

	if (pnMin != NULL) {
		*pnMin = 0x029705;
	}

	const char *names[MAX_K051649_CHIPS] = { "K051649 Channel list", "K051649 #1 Channel list" };

	for (INT32 i = 0; i < MAX_K051649_CHIPS; i++) {
		if (!chip_initted[i]) continue;

		ScanVar(&Chips[i].channel_list, sizeof(Chips[i].channel_list), (char *)names[i]);
		SCAN_VAR(Chips[i].test);
	}
}

/********************************************************************************/

void K051649WaveformWrite(INT32 nChip, INT32 offset, INT32 data)
{
	CHECK_CHIP("K051649WaveformWrite", )

	k051649_state *info = &Chips[nChip];

	// waveram is read-only?
	if (info->test & 0x40 || (info->test & 0x80 && offset >= 0x60))
		return;

	info->channel_list[offset>>5].waveform[offset&0x1f]=data;
	/* SY 20001114: Channel 5 shares the waveform with channel 4 */
	if (offset >= 0x60)
		info->channel_list[4].waveform[offset&0x1f]=data;
}

void K051649WaveformWrite(INT32 offset, INT32 data)
{
	K051649WaveformWrite(0, offset, data);
}

UINT8 K051649WaveformRead(INT32 nChip, INT32 offset)
{
	CHECK_CHIP("K051649WaveformRead", 0)

	k051649_state *info = &Chips[nChip];

	// test-register bits 6/7 expose the internal counter
	if (info->test & 0xc0)
	{
		stream[nChip].update();

		if (offset >= 0x60)
			offset += info->channel_list[3 + (info->test >> 6 & 1)].counter;
		else if (info->test & 0x40)
			offset += info->channel_list[offset >> 5].counter;
	}
	return info->channel_list[offset>>5].waveform[offset&0x1f];
}

UINT8 K051649WaveformRead(INT32 offset)
{
	return K051649WaveformRead(0, offset);
}

/* SY 20001114: Channel 5 doesn't share the waveform with channel 4 on this chip */
void K052539WaveformWrite(INT32 nChip, INT32 offset, INT32 data)
{
	CHECK_CHIP("K052539WaveformWrite", )

	Chips[nChip].channel_list[offset>>5].waveform[offset&0x1f]=data;
}

void K052539WaveformWrite(INT32 offset, INT32 data)
{
	K052539WaveformWrite(0, offset, data);
}

void K051649VolumeWrite(INT32 nChip, INT32 offset, INT32 data)
{
	CHECK_CHIP("K051649VolumeWrite", )

	Chips[nChip].channel_list[offset&0x7].volume=data&0xf;
}

void K051649VolumeWrite(INT32 offset, INT32 data)
{
	K051649VolumeWrite(0, offset, data);
}

void K051649FrequencyWrite(INT32 nChip, INT32 offset, INT32 data)
{
	CHECK_CHIP("K051649FrequencyWrite", )

	k051649_state *info = &Chips[nChip];

	INT32 freq_hi = offset & 1;
	offset >>= 1;

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

void K051649FrequencyWrite(INT32 offset, INT32 data)
{
	K051649FrequencyWrite(0, offset, data);
}

void K051649KeyonoffWrite(INT32 nChip, INT32 data)
{
	CHECK_CHIP("K051649KeyonoffWrite", )

	k051649_state *info = &Chips[nChip];
	info->channel_list[0].key=(data&1) ? 1 : 0;
	info->channel_list[1].key=(data&2) ? 1 : 0;
	info->channel_list[2].key=(data&4) ? 1 : 0;
	info->channel_list[3].key=(data&8) ? 1 : 0;
	info->channel_list[4].key=(data&16) ? 1 : 0;
}

void K051649KeyonoffWrite(INT32 data)
{
	K051649KeyonoffWrite(0, data);
}

UINT8 K051649Read(INT32 nChip, INT32 offset)
{
	CHECK_CHIP("K051649Read", 0)

	stream[nChip].update();

	offset &= 0xff;

	if (offset < 0x80) {
		return K051649WaveformRead(nChip, offset);
	}

	offset &= ~0x10; // mirror

	if (offset >= 0xe0) { // test register
		Chips[nChip].test = 0xff;
		return 0xff;
	}

	return 0;
}

UINT8 K051649Read(INT32 offset)
{
	return K051649Read(0, offset);
}

void K051649Write(INT32 nChip, INT32 offset, UINT8 data)
{
	CHECK_CHIP("K051649Write", )

	stream[nChip].update();
	offset &= 0xff;

	if ((offset & 0x80) == 0x00) {
		K051649WaveformWrite(nChip, offset & 0x7f, data);
		return;
	}

	offset &= ~0x10; // mirror

	if (offset >= 0x80 && offset <= 0x89) { // freq register
		K051649FrequencyWrite(nChip, offset & 0xf, data);
		return;
	}

	if (offset >= 0x8a && offset <= 0x8e) { // volume register
		K051649VolumeWrite(nChip, offset - 0x8a, data);
		return;
	}

	if (offset == 0x8f) {
		K051649KeyonoffWrite(nChip, data);
		return;
	}

	if (offset >= 0xe0) { // test register
		Chips[nChip].test = data;
		return;
	}
}

void K051649Write(INT32 offset, UINT8 data)
{
	K051649Write(0, offset, data);
}
