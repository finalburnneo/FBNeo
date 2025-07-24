// FB Neo sample player module

#include "burnint.h"
#include "samples.h"

#if defined(BUILD_WIN32) || defined(__LIBRETRO__)	// Already tested platforms are added here
#define INCLUDE_FLACMP3_SUPPORT
#endif // BUILD_WIN32


#ifdef INCLUDE_FLACMP3_SUPPORT

#ifndef DR_FLAC_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"
#endif

#ifndef DR_MP3_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#endif

#endif // INCLUDE_FLACMP3_SUPPORT


#define SAMPLE_DIRECTORY	szAppSamplesPath
#define MAX_CHANNEL			32

#define get_long()	((ptr[3] << 24) | (ptr[2] << 16) | (ptr[1] << 8) | (ptr[0] << 0))
#define get_short()	((ptr[1] << 8) | (ptr[0] << 0))

static inline UINT16 get_shorti(const UINT8* const p)
{
	return (p[1] << 8) | p[0];
}

static INT32 bNiceFadeVolume = 0;
static INT32 bAddToStream = 0;
static INT32 nTotalSamples = 0;
INT32 bBurnSampleTrimSampleEnd = 0;

struct sample_format
{
	UINT8 *data;
	UINT32 length;
	UINT64 position;
	UINT8 playing;
	UINT8 loop;
	UINT8 flags;
	INT32 playback_rate; // 100 = 100%, 200 = 200%,
	double gain[2];
	double gain_target[2]; // ramp gain up or down to gain_target (see BurnSampleSetRouteFade())
	INT32 output_dir[2];
};

static struct sample_format *samples		= NULL; // store samples
static struct sample_format *sample_ptr		= NULL; // generic pointer for sample
static INT32 sample_channels[MAX_CHANNEL];			// channel handling
	
static void make_raw(UINT8 *src, UINT32 len)
{
	UINT8 *ptr = src;

	if (ptr[0] != 'R' || ptr[1] != 'I' || ptr[2] != 'F' || ptr[3] != 'F') return;
	                                    ptr += 4; // skip RIFF
	UINT32 length = get_long();		    ptr += 4; // total length of file
	if (len < length) length = len - 8;	    	  // first 8 bytes (RIFF + Len)

	/* "WAVEfmt " */			        ptr += 8; // WAVEfmt + 1 space
	UINT32 length2 = get_long();		ptr += 4; // Wavefmt length
/*	UINT16 format = get_short();  */    ptr += 2; // format?
	UINT16 channels = get_short();		ptr += 2; // channels
	UINT32 sample_rate = get_long();	ptr += 4; // sample rate
/*	UINT32 speed = get_long();      */  ptr += 4; // speed - should equal (bits * channels * sample_rate)
/*	UINT16 align = get_short();   */    ptr += 2; // block align	should be ((bits / 8) * channels)
	UINT16 bytes = get_short() / 8;		ptr += 2; // bytes per sample	(0010)
	ptr += length2 - 16;				          // get past the wave format chunk

	// are we in the 'data' chunk? if not, skip this chunk.
	if (ptr[0] != 'd' || ptr[1] != 'a' || ptr[2] != 't' || ptr[3] != 'a') {
		                                ptr += 4; // skip tag
		UINT32 length3 = get_long();    ptr += 4;
		ptr += length3;
	}

	/* "data" */				        ptr += 4; // "data"
	UINT32 data_length = get_long();	ptr += 4; // should be up to the data...

	if ((len - (ptr - src)) < data_length) data_length = len - (ptr - src);

	UINT32 converted_len = (UINT32)((float)(data_length * (nBurnSoundRate * 1.00000 / sample_rate) / (bytes * channels)));
	if (converted_len == 0) return;

	sample_ptr->data = (UINT8*)BurnMalloc(converted_len * 4);

	// up/down sample everything and convert to raw 16 bit stereo
	INT16 *data = (INT16*)sample_ptr->data;
	INT16 *poin = (INT16*)ptr;
	UINT8 *poib = ptr;

	if ((INT32)sample_rate == nBurnSoundRate)
	{
		// don't try to interpolate, just copy
		bprintf(0, _T("Sample at native rate already..\n"));
		for (UINT32 i = 0; i < converted_len; i++)
		{
			if (bytes == 2)											//  signed 16 bit, stereo & mono
			{
				data[i * 2 + 0] = BURN_ENDIAN_SWAP_INT16(poin[i * channels + 0             ]);
				data[i * 2 + 1] = BURN_ENDIAN_SWAP_INT16(poin[i * channels + (channels / 2)]);
			}
			else if (bytes == 1)										// unsigned 8 bit, stereo & mono
			{
				data[i * 2 + 0] = (poib[i * channels + 0             ] - 128) << 8; data[i * 2 + 0] |= (data[i * 2 + 0] >> 7) & 0xFF;
				data[i * 2 + 1] = (poib[i * channels + (channels / 2)] - 128) << 8; data[i * 2 + 1] |= (data[i * 2 + 1] >> 7) & 0xFF;
			}
		}
	}
	else
	{
		// interpolate sample
		bprintf(0, _T("Converting %dhz [%d bit, %d channels] to %dhz (native).\n"), sample_rate, bytes*8, channels, nBurnSoundRate);
		INT32 buffer_l[4];
		INT32 buffer_r[4];

		memset(buffer_l, 0, sizeof(buffer_l));
		memset(buffer_r, 0, sizeof(buffer_r));

#if 0
		// this block causes clicks when the sample loops, disable for now
		if (sample_ptr->flags & SAMPLE_AUTOLOOP)
		{
			UINT8* end = sample_ptr->data + data_length / (bytes * channels);

			if (bytes == 1)
			{
				buffer_l[1] = (INT16)((*(end - 3 * channels)) - 0x80) << 8; buffer_l[1] |= (buffer_l[1] >> 7) & 0xFF;
				buffer_l[2] = (INT16)((*(end - 2 * channels)) - 0x80) << 8; buffer_l[2] |= (buffer_l[2] >> 7) & 0xFF;
				buffer_l[3] = (INT16)((*(end - 1 * channels)) - 0x80) << 8; buffer_l[3] |= (buffer_l[3] >> 7) & 0xFF;

				buffer_r[1] = (INT16)((*(end - 3 * channels) + (channels / 2)) - 0x80) << 8; buffer_r[1] |= (buffer_r[1] >> 7) & 0xFF;
				buffer_r[2] = (INT16)((*(end - 2 * channels) + (channels / 2)) - 0x80) << 8; buffer_r[2] |= (buffer_r[2] >> 7) & 0xFF;
				buffer_r[3] = (INT16)((*(end - 1 * channels) + (channels / 2)) - 0x80) << 8; buffer_r[3] |= (buffer_r[3] >> 7) & 0xFF;
			}
			else
			{
				buffer_l[1] = (INT16)(get_shorti(end - 6 * channels));
				buffer_l[2] = (INT16)(get_shorti(end - 4 * channels));
				buffer_l[3] = (INT16)(get_shorti(end - 2 * channels));

				buffer_r[1] = (INT16)(get_shorti(end - 6 * channels) + (channels & 2));
				buffer_r[2] = (INT16)(get_shorti(end - 4 * channels) + (channels & 2));
				buffer_r[3] = (INT16)(get_shorti(end - 2 * channels) + (channels & 2));
			}
		}
#endif
		UINT64 prev_offs = ~0;

		for (UINT64 i = 0; i < converted_len; i++)
		{
			UINT64 pos = (i * sample_rate << 12) / nBurnSoundRate;
			UINT64 curr_offs = pos >> 12;

			while (prev_offs != curr_offs)
			{
				prev_offs += 1;
				buffer_l[0] = buffer_l[1]; buffer_r[0] = buffer_r[1];
				buffer_l[1] = buffer_l[2]; buffer_r[1] = buffer_r[2];
				buffer_l[2] = buffer_l[3]; buffer_r[2] = buffer_r[3];

				if (prev_offs * channels + (channels / 2) >= data_length)
				{
					//bprintf(0, _T("uhoh: %I64x, "), prev_offs);
					buffer_l[3] = buffer_r[3] = 0;
					break;
				}

				if (bytes == 2)										// signed 16 bit, stereo & mono
				{
					buffer_l[3] = (INT16)BURN_ENDIAN_SWAP_INT16(poin[prev_offs * channels + 0             ]);
					buffer_r[3] = (INT16)BURN_ENDIAN_SWAP_INT16(poin[prev_offs * channels + (channels / 2)]);
				}
				else if (bytes == 1)									// unsigned 8 bit, stereo & mono
				{
					buffer_l[3] = (INT32)(poib[prev_offs * channels + 0             ] - 128) << 8; buffer_l[3] |= (buffer_l[3] >> 7) & 0xFF;
					buffer_r[3] = (INT32)(poib[prev_offs * channels + (channels / 2)] - 128) << 8; buffer_r[3] |= (buffer_r[3] >> 7) & 0xFF;
				}
			}

			data[i * 2 + 0] = BURN_SND_CLIP(INTERPOLATE4PS_16BIT(pos & 0x0FFF, buffer_l[0], buffer_l[1], buffer_l[2], buffer_l[3]));
			data[i * 2 + 1] = BURN_SND_CLIP(INTERPOLATE4PS_16BIT(pos & 0x0FFF, buffer_r[0], buffer_r[1], buffer_r[2], buffer_r[3]));
		}
	}

	{ // sample cleanup
		if (bBurnSampleTrimSampleEnd) { // trim silence off the end of the sample, bBurnSampleTrimSampleEnd must be set before init!
			while (data[(converted_len - 1) * bytes] == 0 && converted_len > 0) converted_len -= bytes;
		}
	}

	sample_ptr->length = converted_len;
	sample_ptr->playing = 0;
	sample_ptr->position = 0;
}


#ifdef INCLUDE_FLACMP3_SUPPORT
// Decode FLAC to PCM (Annotation Reference MP3)
static void make_raw_flac(UINT8* src, UINT32 len)
{
	drflac* pFlac = drflac_open_memory(src, len, NULL);
	if (!pFlac) {
		bprintf(0, _T("Failed to initialize FLAC decoder\n"));
		return;
	}

	const UINT32 sample_rate = pFlac->sampleRate, channels = pFlac->channels;
	const drflac_uint64 totalPCMFrameCount = pFlac->totalPCMFrameCount;

	if (0 == totalPCMFrameCount) {
		drflac_close(pFlac);
		bprintf(0, _T("No PCM frames in FLAC\n"));
		return;
	}

	const drflac_uint64 pcmDataSize = totalPCMFrameCount * channels * sizeof(drflac_int16);
	drflac_int16* pcmData = (drflac_int16*)BurnMalloc(pcmDataSize);
	if (!pcmData) {
		drflac_close(pFlac);
		bprintf(0, _T("Memory allocation failed for PCM data\n"));
		return;
	}

	const drflac_uint64 framesRead = drflac_read_pcm_frames_s16(pFlac, totalPCMFrameCount, pcmData);
	drflac_close(pFlac);

	if (framesRead != totalPCMFrameCount) {
		BurnFree(pcmData);
		bprintf(0, _T("FLAC decoding incomplete: %d/%d frames\n"), (INT32)framesRead, (INT32)totalPCMFrameCount);
		return;
	}

	drflac_uint64 convertedFrameCount = totalPCMFrameCount;
	if (sample_rate != nBurnSoundRate) {
		bprintf(0, _T("Converting FLAC %dhz to native %dhz\n"), sample_rate, nBurnSoundRate);

		convertedFrameCount = (drflac_uint64)((double)totalPCMFrameCount * nBurnSoundRate / sample_rate + 0.5);
		if (0 == convertedFrameCount) {
			BurnFree(pcmData);
			return;
		}

		INT16* resampledData = (INT16*)BurnMalloc(convertedFrameCount * 2 * sizeof(INT16));
		if (NULL == resampledData) {
			BurnFree(pcmData);
			return;
		}

		const double src_ratio = (double)nBurnSoundRate / sample_rate;

		for (drflac_uint64 out_frame = 0; out_frame < convertedFrameCount; ++out_frame) {
			const double in_pos          = out_frame / src_ratio;
			const drflac_uint64 in_index = (drflac_uint64)in_pos;
			const double fraction        = in_pos - in_index;

			if (in_index >= totalPCMFrameCount - 1) {
				resampledData[out_frame * 2 + 0] = 0;
				resampledData[out_frame * 2 + 1] = 0;
			} else {
				for (INT32 c = 0; c < 2; c++) {
					const INT32 channel_index = c % channels;
					const INT16 sample1 = pcmData[(in_index + 0) * channels + channel_index];
					const INT16 sample2 = pcmData[(in_index + 1) * channels + channel_index];
					resampledData[out_frame * 2 + c] = (INT16)(sample1 + fraction * (sample2 - sample1));
				}
			}
		}

		BurnFree(pcmData);
		pcmData = resampledData;
	} else if (1 == channels) {
		INT16* stereoData = (INT16*)BurnMalloc(convertedFrameCount * 2 * sizeof(INT16));
		if (stereoData) {
			for (drflac_uint64 i = 0; i < convertedFrameCount; i++) {
				stereoData[i * 2 + 0] = pcmData[i];
				stereoData[i * 2 + 1] = pcmData[i];
			}
			BurnFree(pcmData);
			pcmData = stereoData;
		}
	} else if (channels >= 2) {
		for (drflac_uint64 i = 0; i < convertedFrameCount; i++) {
			const INT16 left   = pcmData[i * channels + 0];
			const INT16 right  = pcmData[i * channels + 1];
			pcmData[i * 2 + 0] = left;
			pcmData[i * 2 + 1] = right;
		}
	}

	if (bBurnSampleTrimSampleEnd) {
		drflac_uint64 frameCount = convertedFrameCount;
		const INT16* data = pcmData;

		while (frameCount > 0) {
			drflac_uint64 idx = (frameCount - 1) * 2;
			if (data[idx] == 0 && data[idx + 1] == 0) {
				frameCount--;
			} else {
				break;
			}
		}
		convertedFrameCount = frameCount;
	}

	sample_ptr->data     = (UINT8*)pcmData;
	sample_ptr->length   = (UINT32)convertedFrameCount;
	sample_ptr->playing  = 0;
	sample_ptr->position = 0;

	bprintf(0, _T("Loaded FLAC: %d frames (%dHz -> %dHz)\n"),
		sample_ptr->length, sample_rate, nBurnSoundRate);
}

// Decode MP3 to PCM
static void make_raw_mp3(UINT8* src, UINT32 len)
{
	drmp3 mp3;
	drmp3_int16* pcmData = NULL;

	// Initialize MP3 Decoder
	if (!drmp3_init_memory(&mp3, src, len, NULL)) {
		bprintf(0, _T("Failed to initialize MP3 decoder\n"));
		return;
	}

	// Get raw sample rate and number of channels
	const UINT32 sample_rate = mp3.sampleRate, channels = mp3.channels;

	// Get total PCM frames (each frame contains all channels)
	const drmp3_uint64 totalPCMFrameCount = drmp3_get_pcm_frame_count(&mp3);
	if (0 == totalPCMFrameCount) {
		drmp3_uninit(&mp3);
		bprintf(0, _T("No PCM frames in MP3\n"));
		return;
	}

	// Allocate memory to store decoded PCM data (16-bit signed integer format)
	const drmp3_uint64 pcmDataSize = totalPCMFrameCount * channels * sizeof(drmp3_int16);
	pcmData = (drmp3_int16*)BurnMalloc(pcmDataSize);
	if (NULL == pcmData) {
		drmp3_uninit(&mp3);
		bprintf(0, _T("Memory allocation failed for PCM data\n"));
		return;
	}

	// Decode the entire MP3 file
	const drmp3_uint64 framesRead = drmp3_read_pcm_frames_s16(&mp3, totalPCMFrameCount, pcmData);
	drmp3_uninit(&mp3);  // Release resources as soon as decoding is complete

	if (framesRead != totalPCMFrameCount) {
		BurnFree(pcmData);
		bprintf(0, _T("MP3 decoding incomplete: %d/%d frames\n"), (INT32)framesRead, (INT32)totalPCMFrameCount);
		return;
	}

	// Resampling processing (if required)
	drmp3_uint64 convertedFrameCount = totalPCMFrameCount;
	if (sample_rate != nBurnSoundRate) {
		bprintf(0, _T("Converting MP3 %dhz to native %dhz\n"), sample_rate, nBurnSoundRate);

		// Calculate the number of frames after resampling (round up)
		convertedFrameCount = (drmp3_uint64)((double)totalPCMFrameCount * nBurnSoundRate / sample_rate + 0.5);
		if (0 == convertedFrameCount) {
			BurnFree(pcmData);
			return;
		}

		// Allocating the buffer after resampling (converting to stereo)
		INT16* resampledData = (INT16*)BurnMalloc(convertedFrameCount * 2 * sizeof(INT16));
		if (NULL == resampledData) {
			BurnFree(pcmData);
			return;
		}

		// Resample Ratio: Output Sample Rate/Input Sample Rate
		const double src_ratio = (double)nBurnSoundRate / sample_rate;

		// Linear interpolation resampling
		for (drmp3_uint64 out_frame = 0; out_frame < convertedFrameCount; ++out_frame) {
			const double in_pos         = out_frame / src_ratio;
			const drmp3_uint64 in_index = (drmp3_uint64)in_pos;
			const double fraction       = in_pos - in_index;

			// Boundary check
			if (in_index >= totalPCMFrameCount - 1) {
				// Fill 0 when out of range
				resampledData[out_frame * 2 + 0] = 0;
				resampledData[out_frame * 2 + 1] = 0;
			} else {
				// Interpolation for each channel
				for (INT32 c = 0; c < 2; c++) {
					// Ensure that even mono files are handled correctly
					const INT32 channel_index = c % channels;
					const INT16 sample1 = pcmData[(in_index + 0) * channels + channel_index];
					const INT16 sample2 = pcmData[(in_index + 1) * channels + channel_index];
					resampledData[out_frame * 2 + c] = (INT16)(sample1 + fraction * (sample2 - sample1));
				}
			}
		}

		// Replace with new resampled data
		BurnFree(pcmData);
		pcmData = resampledData;
	}
	// If the file is mono, convert to stereo
	else if (1 == channels) {
		INT16* stereoData = (INT16*)BurnMalloc(convertedFrameCount * 2 * sizeof(INT16));
		if (stereoData) {
			for (drmp3_uint64 i = 0; i < convertedFrameCount; i++) {
				stereoData[i * 2 + 0] = pcmData[i];	// Left channel
				stereoData[i * 2 + 1] = pcmData[i];	// Right channel
			}
			BurnFree(pcmData);
			pcmData = stereoData;
		}
	}
	// If it is stereo but the channel order is not correct, adjust the channel order
	else if (channels >= 2) {
		// Make sure the first two channels are left and right
		for (drmp3_uint64 i = 0; i < convertedFrameCount; i++) {
			const INT16 left   = pcmData[i * channels + 0];
			const INT16 right  = pcmData[i * channels + 1];
			pcmData[i * 2 + 0] = left;
			pcmData[i * 2 + 1] = right;
		}
	}

	// Mute trimming (if enabled)
	if (bBurnSampleTrimSampleEnd) {
		drmp3_uint64 frameCount = convertedFrameCount;
		const INT16* data       = pcmData;

		// Look forward from the end until a non-silent frame is found
		while (frameCount > 0) {
			drmp3_uint64 idx = (frameCount - 1) * 2;
			if (data[idx] == 0 && data[idx + 1] == 0) {
				frameCount--;
			} else {
				break;
			}
		}
		convertedFrameCount = frameCount;
	}

	// Store to sample_ptr
	sample_ptr->data     = (UINT8*)pcmData;
	sample_ptr->length   = (UINT32)convertedFrameCount;	// Key: store frames not samples
	sample_ptr->playing  = 0;
	sample_ptr->position = 0;

	bprintf(0, _T("Loaded MP3: %d frames (%dHz -> %dHz)\n"),
		sample_ptr->length, sample_rate, nBurnSoundRate);
}
#endif

// for stream-sync
static INT32 samples_buffered = 0;
static INT32 (*pCPUTotalCycles)() = NULL;
static UINT32 nDACCPUMHZ = 0;
static INT32 nPosition = 0;
static INT16 *soundbuf = NULL;
static INT32 IN_RESET = 0;

// forward
static void BurnSampleRender_INT(UINT32 pLen);


// Streambuffer handling
static INT32 SyncInternal()
{
    if (!samples_buffered) return 0;
	return (INT32)(float)(nBurnSoundLen * (pCPUTotalCycles() / (nDACCPUMHZ / (nBurnFPS / 100.0000))));
}

static void UpdateStream(INT32 samples_len)
{
    if (!samples_buffered || !pBurnSoundOut) return;
    if (samples_len > nBurnSoundLen) samples_len = nBurnSoundLen;

	INT32 nSamplesNeeded = samples_len - nPosition;
	if (nSamplesNeeded < 1) return;

	//bprintf(0, _T("samples_sync: %d samples    frame %d\n"), nSamplesNeeded, nCurrentFrame);

    BurnSampleRender_INT(nSamplesNeeded);
    nPosition += nSamplesNeeded;
}

void BurnSampleSync()
{
	if (IN_RESET) return;
	UpdateStream(SyncInternal());
}

void BurnSampleSetBuffered(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ)
{
    bprintf(0, _T("*** Using BUFFERED samples-mode.\n"));

	nPosition = 0;

    samples_buffered = 1;

    pCPUTotalCycles = pCPUCyclesCB;
	nDACCPUMHZ = nCpuMHZ;
}

void BurnSampleInitOne(INT32); // below...

INT32 BurnSampleGetChannelSample(INT32 channel)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleGetChannelSample called without init\n"));
	if (channel >= MAX_CHANNEL) bprintf(PRINT_ERROR, _T("BurnSampleGetChannelSample called with invalid channel (%d), max is %d\n"), channel, MAX_CHANNEL);
#endif

	BurnSampleSync();

	return sample_channels[channel];
}

void BurnSamplePlay(INT32 sample)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSamplePlay called without init\n"));
#endif

	if (sample >= nTotalSamples) return;

	BurnSampleSync();

	sample_ptr = &samples[sample];

	if (sample_ptr->flags & SAMPLE_IGNORE) return;

	if (sample_ptr->flags & SAMPLE_NOSTOREF) {
		BurnSampleInitOne(sample);
	}

	sample_ptr->playing = 1;
	sample_ptr->position = 0;
}

void BurnSampleChannelPlay(INT32 channel, INT32 sample, INT32 loop)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleChannelPlay called without init\n"));
	if (channel >= MAX_CHANNEL) bprintf(PRINT_ERROR, _T("BurnSampleChannelPlay called with invalid channel (%d), max is %d\n"), channel, MAX_CHANNEL);
#endif

	if (sample >= nTotalSamples) return;

	BurnSampleChannelStop(channel);

	sample_channels[channel] = sample;

	BurnSamplePlay(sample);
	if (loop != -1) { // -1, use config from sample struct
		BurnSampleSetLoop(sample, loop);
	}
}

void BurnSamplePause(INT32 sample)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSamplePause called without init\n"));
#endif

	if (sample >= nTotalSamples) return;

	BurnSampleSync();

	sample_ptr = &samples[sample];
	sample_ptr->playing = 0;
}

void BurnSampleChannelPause(INT32 channel, bool pause)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleChannelPause called without init\n"));
	if (channel >= MAX_CHANNEL) bprintf(PRINT_ERROR, _T("BurnSampleChannelPause called with invalid channel (%d), max is %d\n"), channel, MAX_CHANNEL);
#endif

	if (sample_channels[channel] >= nTotalSamples) return;

	BurnSampleSync();

	sample_ptr = &samples[sample_channels[channel]];
	sample_ptr->playing = pause ? 0 : 1;
}

void BurnSampleResume(INT32 sample)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleResume called without init\n"));
#endif

	if (sample >= nTotalSamples) return;

	BurnSampleSync();

	sample_ptr = &samples[sample];
	sample_ptr->playing = 1;
}

void BurnSampleStop_INT(INT32 sample) // internal use, without _SYNC
{
	if (sample >= nTotalSamples) return;

	sample_ptr = &samples[sample];
	sample_ptr->playing = 0;
	sample_ptr->position = 0;
	//sample_ptr->playback_rate = 100; // 100% // on load and reset, only!
}

void BurnSampleStop(INT32 sample)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleStop called without init\n"));
#endif

	if (sample >= nTotalSamples) return;

	BurnSampleSync();

	sample_ptr = &samples[sample];
	sample_ptr->playing = 0;
	sample_ptr->position = 0;
	//sample_ptr->playback_rate = 100; // 100% // on load and reset, only!
}

void BurnSampleChannelStop(INT32 channel)
{
#if defined FBNEO_DEBUG
	if (channel >= MAX_CHANNEL) bprintf(PRINT_ERROR, _T("BurnSampleChannelStop called with invalid channel (%d), max is %d\n"), channel, MAX_CHANNEL);
#endif

	BurnSampleStop(sample_channels[channel]);
}

void BurnSampleSetLoop(INT32 sample, bool dothis)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleSetLoop called without init\n"));
#endif

	if (sample >= nTotalSamples) return;

	BurnSampleSync();

	sample_ptr = &samples[sample];

	sample_ptr->loop = (dothis ? 1 : 0);
}

INT32 BurnSampleGetStatus(INT32 sample)
{
	// this is also used to see if samples initialized and/or the game has samples.

	if (sample >= nTotalSamples) return SAMPLE_INVALID;

	BurnSampleSync();

	sample_ptr = &samples[sample];

	if (!sample_ptr->playing && sample_ptr->position)
		return SAMPLE_PAUSED;

	return (sample_ptr->playing) ? SAMPLE_PLAYING : SAMPLE_STOPPED;
}

INT32 BurnSampleGetChannelStatus(INT32 channel)
{
#if defined FBNEO_DEBUG
	if (channel >= MAX_CHANNEL) bprintf(PRINT_ERROR, _T("BurnSampleGetChannelStatus called with invalid channel (%d), max is %d\n"), channel, MAX_CHANNEL);
#endif

	return BurnSampleGetStatus(sample_channels[channel]);
}

INT32 BurnSampleGetPosition(INT32 sample)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleGetPosition called without init\n"));
#endif

	if (sample >= nTotalSamples) return -1;

	BurnSampleSync();

	sample_ptr = &samples[sample];
	return (sample_ptr->position / 0x10000);
}

INT32 BurnSampleChannelGetPosition(INT32 channel)
{
#if defined FBNEO_DEBUG
	if (channel >= MAX_CHANNEL) bprintf(PRINT_ERROR, _T("BurnSampleChannelGetPosition called with invalid channel (%d), max is %d\n"), channel, MAX_CHANNEL);
#endif

	return BurnSampleGetPosition(sample_channels[channel]);
}

void BurnSampleSetPosition(INT32 sample, UINT32 position)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleSetPosition called without init\n"));
#endif

	if (sample >= nTotalSamples) return;

	BurnSampleSync();

	sample_ptr = &samples[sample];
	sample_ptr->position = position * 0x10000;
}

void BurnSampleChannelSetPosition(INT32 channel, UINT32 position)
{
#if defined FBNEO_DEBUG
	if (channel >= MAX_CHANNEL) bprintf(PRINT_ERROR, _T("BurnSampleChannelSetPosition called with invalid channel (%d), max is %d\n"), channel, MAX_CHANNEL);
#endif

	BurnSampleSetPosition(sample_channels[channel], position);
}

void BurnSampleSetPlaybackRate(INT32 sample, INT32 rate)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleSetPlaybackRate called without init\n"));
	if (rate > 5000 || rate < 0) bprintf (PRINT_ERROR, _T("BurnSampleSetPlaybackRate called with unlikely rate (%d)!\n"), rate);
#endif

	if (sample >= nTotalSamples) return;

	BurnSampleSync();

	sample_ptr = &samples[sample];
	sample_ptr->playback_rate = rate;
}

void BurnSampleReset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleReset called without init\n"));
#endif

	memset (sample_channels, 0, sizeof(sample_channels));

	IN_RESET = 1;

	for (INT32 i = 0; i < nTotalSamples; i++) {
		BurnSampleStop(i);
		BurnSampleSetPlaybackRate(i, 100);

		if (sample_ptr->flags & SAMPLE_AUTOLOOP) {
			BurnSampleSetLoop(i, true); // this sets the loop flag, from the driver.
		}
	}

	IN_RESET = 0;
}

INT32 __cdecl ZipLoadOneFile(char* arcName, const char* fileName, void** Dest, INT32* pnWrote);
char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, INT32 nOutSize);
#define _TtoA(a)	TCHARToANSI(a, NULL, 0)

void BurnSampleInit(INT32 bAdd /*add samples to stream?*/)
{
	bAddToStream = bAdd & ~0x8000;
	bool bForceNostore = bAdd & 0x8000;

	nTotalSamples = 0;
	bNiceFadeVolume = 0;

	DebugSnd_SamplesInitted = 1;

	if (nBurnSoundRate == 0) {
		nTotalSamples = 0;
		return;
	}

	INT32 length;
	char path[256*2];
	char setname[128];
	void *destination = NULL;
	char szTempPath[MAX_PATH];
	sprintf(szTempPath, "%s", _TtoA(SAMPLE_DIRECTORY));

	// test to see if file exists
	INT32 nEnableSamples = 0;

	if (BurnDrvGetTextA(DRV_SAMPLENAME) == NULL) { // called with no samples
		nTotalSamples = 0;
		return;
	}

	strcpy(setname, BurnDrvGetTextA(DRV_SAMPLENAME));
	sprintf(path, "%s%s.zip", szTempPath, setname);
	
	FILE *test = fopen(path, "rb");
	if (test) 
	{
		nEnableSamples = 1;
		fclose(test);
	}
	
#ifdef INCLUDE_7Z_SUPPORT
	sprintf(path, "%s%s.7z", szTempPath, setname);
	
	test = fopen(path, "rb");
	if (test)
	{	
		nEnableSamples = 1;
		fclose(test);
	}
#endif

	soundbuf = (INT16*)BurnMalloc(0x1000 * 2 * 2); // mixing buffer

	struct BurnSampleInfo si;
	INT32 nSampleOffset = -1;
	do {
		BurnDrvGetSampleInfo(&si, ++nSampleOffset);
		if (si.nFlags) nTotalSamples++;
	} while (si.nFlags);

	samples = (sample_format*)BurnMalloc(sizeof(sample_format) * nTotalSamples);
	memset (samples, 0, sizeof(sample_format) * nTotalSamples);

	BurnSetProgressRange(0.99); // Expand Progress bar
	BurnUpdateProgress(0.0, _T("Loading samples..."), 0);

	for (INT32 i = 0; i < nTotalSamples; i++) {
		BurnUpdateProgress(1.0 / nTotalSamples, NULL, 0);

		BurnDrvGetSampleInfo(&si, i);
		char *szSampleNameTmp = NULL;
		BurnDrvGetSampleName(&szSampleNameTmp, i, 0);

		sample_ptr = &samples[i];
		
		// append .wav to filename
		char szSampleName[1024];
		memset(&szSampleName, 0, sizeof(szSampleName));
		strncpy(&szSampleName[0], szSampleNameTmp, sizeof(szSampleName) - 5);			// leave space for ".wav" + null, just incase!
		strcat(&szSampleName[0], ".wav");

		if (si.nFlags == 0) break;

		// set defaults before NOSTORE check!
		sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] = 1.00;
		sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_1] = 1.00;

		sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] = 1.00;
		sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_2] = 1.00;

		sample_ptr->output_dir[BURN_SND_SAMPLE_ROUTE_1] = BURN_SND_ROUTE_BOTH;
		sample_ptr->output_dir[BURN_SND_SAMPLE_ROUTE_2] = BURN_SND_ROUTE_BOTH;
		sample_ptr->playback_rate = 100;

		if (si.nFlags & SAMPLE_NOSTOREF || bForceNostore) {
			sample_ptr->flags = si.nFlags | (bForceNostore ? SAMPLE_NOSTOREF : 0);
			sample_ptr->data = NULL;
			continue;
		}

		sprintf (path, "%s%s", szTempPath, setname);

		destination = NULL;
		length = 0;

		if (nEnableSamples) {
			ZipLoadOneFile((char*)path, (const char*)szSampleName, &destination, &length);
		}

		if (length) {
			sample_ptr->flags = si.nFlags;
			bprintf(0, _T("Loading \"%S\": "), szSampleName);
			make_raw((UINT8*)destination, length);
			free(destination);			// ZipLoadOneFile uses malloc()
		} else {
#ifdef INCLUDE_FLACMP3_SUPPORT
			// append .flac to filename
			memset(&szSampleName, 0, sizeof(szSampleName));
			strncpy(&szSampleName[0], szSampleNameTmp, sizeof(szSampleName) - 6);		// leave space for ".flac" + null, just incase!
			strcat(&szSampleName[0], ".flac");

			destination = NULL;
			length = 0;

			ZipLoadOneFile((char*)path, (const char*)szSampleName, &destination, &length);

			if (length) {
				sample_ptr->flags = si.nFlags;
				bprintf(0, _T("Loading FLAC \"%S\": "), szSampleName);
				make_raw_flac((UINT8*)destination, length);
				free(destination);		// ZipLoadOneFile uses malloc()
			} else {
				// append .mp3 to filename
				memset(&szSampleName, 0, sizeof(szSampleName));
				strncpy(&szSampleName[0], szSampleNameTmp, sizeof(szSampleName) - 5);	// leave space for ".mp3" + null, just incase!
				strcat(&szSampleName[0], ".mp3");

				destination = NULL;
				length = 0;

				ZipLoadOneFile((char*)path, (const char*)szSampleName, &destination, &length);

				if (length) {
					sample_ptr->flags = si.nFlags;
					bprintf(0, _T("Loading MP3 \"%S\": "), szSampleName);
					make_raw_mp3((UINT8*)destination, length);
					free(destination);	// ZipLoadOneFile uses malloc()
				} else {
					sample_ptr->flags = SAMPLE_IGNORE;
				}
			}
#else
				sample_ptr->flags = SAMPLE_IGNORE;
#endif
		}
	}
}

void BurnSampleInitOne(INT32 sample)
{
	if (sample >= nTotalSamples) {
		return;
	}

	{
		struct sample_format *clr_ptr = &samples[0];

		int i = 0;
		while (i < nTotalSamples) {
			if (clr_ptr->data != NULL && i != sample && (clr_ptr->flags & SAMPLE_NOSTOREF)) {
				BurnFree(clr_ptr->data);
				clr_ptr->playing = 0;
				clr_ptr->playback_rate = 100;
				clr_ptr->data = NULL;
			}

			clr_ptr++, i++;
		}
	}

	if ((sample_ptr->flags & SAMPLE_NOSTOREF) == 0) {
		return;
	}

	INT32 length;
	char path[256*2];
	char setname[128];
	void *destination = NULL;
	char szTempPath[MAX_PATH];
	sprintf(szTempPath, "%s", _TtoA(SAMPLE_DIRECTORY));
	strcpy(setname, BurnDrvGetTextA(DRV_SAMPLENAME));

	struct BurnSampleInfo si;
	BurnDrvGetSampleInfo(&si, sample);
	char *szSampleNameTmp = NULL;
	BurnDrvGetSampleName(&szSampleNameTmp, sample, 0);

	sample_ptr = &samples[sample];

	// append .wav to filename
	char szSampleName[1024];
	memset(&szSampleName, 0, sizeof(szSampleName));
	strncpy(&szSampleName[0], szSampleNameTmp, sizeof(szSampleName) - 5);			// leave space for ".wav" + null, just incase!
	strcat(&szSampleName[0], ".wav");

	if (sample_ptr->data != NULL || sample_ptr->flags == SAMPLE_IGNORE) {
		return;
	}

	sprintf (path, "%s%s", szTempPath, setname);

	destination = NULL;
	length = 0;
	ZipLoadOneFile((char*)path, (const char*)szSampleName, &destination, &length);

	if (length) {
		make_raw((UINT8*)destination, length);
	}
#ifdef INCLUDE_FLACMP3_SUPPORT
	else {
		// append .flac to filename
		memset(&szSampleName, 0, sizeof(szSampleName));
		strncpy(&szSampleName[0], szSampleNameTmp, sizeof(szSampleName) - 6);		// leave space for ".flac" + null, just incase!
		strcat(&szSampleName[0], ".flac");

		sprintf(path, "%s%s", szTempPath, setname);

		destination = NULL;
		length = 0;
		ZipLoadOneFile((char*)path, (const char*)szSampleName, &destination, &length);

		if (length) {
			make_raw_flac((UINT8*)destination, length);
		} else {
			// append .mp3 to filename
			memset(&szSampleName, 0, sizeof(szSampleName));
			strncpy(&szSampleName[0], szSampleNameTmp, sizeof(szSampleName) - 5);	// leave space for ".mp3" + null, just incase!
			strcat(&szSampleName[0], ".mp3");

			sprintf(path, "%s%s", szTempPath, setname);

			destination = NULL;
			length = 0;
			ZipLoadOneFile((char*)path, (const char*)szSampleName, &destination, &length);

			if (length) {
				make_raw_mp3((UINT8*)destination, length);
			}
		}
	}
#endif

	free(destination); // ZipLoadOneFile uses malloc()
}

// round ##.###### to ##.##
static double round2dec(double d)
{
	d = (INT32)(d * 100 + 0.5);
	return (double)d / 100;
}

// BurnSampleSetRouteFade() fades up/down to volume to eliminate clicks/pops
// when a game changes volume often.
void BurnSampleSetRouteFade(INT32 sample, INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleSetRouteFade called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("BurnSampleSetRouteFade called with invalid index %i\n"), nIndex);
#endif

	if (sample >= nTotalSamples) return;

	sample_ptr = &samples[sample];
	sample_ptr->gain_target[nIndex] = round2dec(nVolume);
	sample_ptr->output_dir[nIndex] = nRouteDir;

	bNiceFadeVolume = 1;
}

void BurnSampleSetRoute(INT32 sample, INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleSetRoute called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("BurnSampleSetRoute called with invalid index %i\n"), nIndex);
#endif

	if (sample >= nTotalSamples) return;

	sample_ptr = &samples[sample];
	sample_ptr->gain_target[nIndex] = round2dec(nVolume);
	sample_ptr->gain[nIndex] = round2dec(nVolume);
	sample_ptr->output_dir[nIndex] = nRouteDir;
}

void BurnSampleSetRouteAllSamples(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleSetRouteAllSamples called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("BurnSampleSetRouteAllSamples called with invalid index %i\n"), nIndex);
#endif

	if (!nTotalSamples) return;

	for (INT32 i = 0; i < nTotalSamples; i++) {
		sample_ptr = &samples[i];
		sample_ptr->gain[nIndex] = round2dec(nVolume);
		sample_ptr->gain_target[nIndex] = round2dec(nVolume);
		sample_ptr->output_dir[nIndex] = nRouteDir;
	}
}

void BurnSampleExit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleExit called without init\n"));
#endif

	if (!DebugSnd_SamplesInitted) return;

	for (INT32 i = 0; i < nTotalSamples; i++) {
		sample_ptr = &samples[i];
		if (sample_ptr)
			BurnFree (sample_ptr->data);
	}

	if (samples) {
		BurnFree (samples);
	}

	BurnFree(soundbuf);

	if (samples_buffered) {
        samples_buffered = 0;
        pCPUTotalCycles = NULL;
        nDACCPUMHZ = 0;
		nPosition = 0;
    }

	sample_ptr = NULL;
	nTotalSamples = 0;
	bAddToStream = 0;
	bBurnSampleTrimSampleEnd = 0;

	DebugSnd_SamplesInitted = 0;
}

void BurnSampleRender(INT16 *pDest, UINT32 pLen)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleRender called without init\n"));
#endif

	if (pBurnSoundOut == NULL || pDest == NULL) {
		return;
	}

	if (samples_buffered && pLen != nBurnSoundLen) {
		bprintf(0, _T("BurnSampleRender(): once per frame, please!\n"));
		return;
	}

	// if the sample player is the only, or the first, sound chip, clear out the sound buffer!
	if (bAddToStream == 0) {
		memset (pDest, 0, pLen * 2 * sizeof(INT16)); // clear buffer
	}

	if (!soundbuf) return; // after addToStream check

	if (samples_buffered) {
		UpdateStream(pLen);
	} else {
		BurnSampleRender_INT(pLen);
	}

	INT16 *src = soundbuf;

	for (INT32 i = 0; i < pLen; i++) {
		pDest[0] = BURN_SND_CLIP(src[0] + pDest[0]);
		pDest[1] = BURN_SND_CLIP(src[1] + pDest[1]);
		pDest += 2;
		src += 2;
	}

	nPosition = 0;
}

static void BurnSampleRender_INT(UINT32 pLen)
{
	if (pBurnSoundOut == NULL || soundbuf == NULL) {
		return;
	}

	INT16 *pDest = soundbuf + (nPosition * 2); // "* 2", soundbuf is stereo
	memset (pDest, 0, pLen * 2 * sizeof(INT16)); // clear buffer

	for (INT32 i = 0; i < nTotalSamples; i++)
	{
		sample_ptr = &samples[i];
		if (sample_ptr->playing == 0 || sample_ptr->length == 0) continue;

		if (sample_ptr->data == NULL) {
			if (sample_ptr->flags & SAMPLE_NOSTOREF) {
				BurnSampleInitOne(i);
			} else {
				// something went wrong here :)
				continue;
			}
		}

		INT32 playlen = pLen;
		INT32 length = sample_ptr->length;
		UINT64 pos = sample_ptr->position;
		INT32 playback_rate = (0x10000 * sample_ptr->playback_rate) / 100;

		INT16 *dst = pDest;
		INT16 *dat = (INT16*)sample_ptr->data;
		
		if (sample_ptr->loop == 0) // if not looping, check to make sure sample is in bounds
		{
			INT32 current_pos = (pos / 0x10000);
			// if sample position is greater than length, stop playback
			if ((length - current_pos) <= 0) {
				BurnSampleStop_INT(i);
				pos = 0;
				continue;
			}

			// if samples remaining are less than playlen, set playlen to samples remaining
			//if (playlen > (length - current_pos)) playlen = length - current_pos;
			// commented above line oct.2022 (dink) - causes end of sample to be delayed until next sync
		}

		length *= 2; // (stereo) used to ensure position is within bounds

		for (INT32 j = 0; j < playlen; j++, dst+=2, pos+=playback_rate) {
			INT32 nLeftSample = 0, nRightSample = 0;
			UINT32 current_pos = (pos / 0x10000);
			UINT32 position = current_pos * 2; // ~1

			if (sample_ptr->loop == 0) // if not looping, check to make sure sample is in bounds
			{
				// if sample position is greater than length, stop playback
				if ((sample_ptr->length - current_pos) <= 0) {
					BurnSampleStop_INT(i);
					pos = 0;
					break;
				}
			}

			if ((sample_ptr->output_dir[BURN_SND_SAMPLE_ROUTE_1] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				nLeftSample += (INT32)(dat[(position) % length] * sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1]);
			}
			if ((sample_ptr->output_dir[BURN_SND_SAMPLE_ROUTE_1] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				nRightSample += (INT32)(dat[(position) % length] * sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1]);
			}

			if ((sample_ptr->output_dir[BURN_SND_SAMPLE_ROUTE_2] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				nLeftSample += (INT32)(dat[(position + 1) % length] * sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2]);
			}
			if ((sample_ptr->output_dir[BURN_SND_SAMPLE_ROUTE_2] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				nRightSample += (INT32)(dat[(position + 1) % length] * sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2]);
			}

			dst[0] = BURN_SND_CLIP(dst[0] + nLeftSample);
			dst[1] = BURN_SND_CLIP(dst[1] + nRightSample);

			if (bNiceFadeVolume) {
				if (sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] != sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_1]) {
					if (sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] > sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_1]) {
						sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] -= 0.01;
					} else if (sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] < sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_1]) {
						sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] += 0.01;
					}
				}
				if (sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] != sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_2]) {
					if (sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] > sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_2]) {
						sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] -= 0.01;
					} else if (sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] < sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_2]) {
						sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] += 0.01;
					}
				}
			}
		}

		sample_ptr->position = pos; // store the updated position
	}
}

void BurnSampleScan(INT32 nAction, INT32 *pnMin)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleScan called without init\n"));
#endif

	if (pnMin != NULL) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_DRIVER_DATA) {
		for (INT32 i = 0; i < nTotalSamples; i++) {
			sample_ptr = &samples[i];
			SCAN_VAR(sample_ptr->playing);
			SCAN_VAR(sample_ptr->loop);
			SCAN_VAR(sample_ptr->position);
			SCAN_VAR(sample_ptr->playback_rate);
		}

		SCAN_VAR(sample_channels);
	}
}
