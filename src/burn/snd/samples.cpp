// FB Neo sample player module

#include "burnint.h"
#include "samples.h"

#if defined(BUILD_WIN32) || defined(BUILD_SDL) || defined(BUILD_SDL2) || defined(__LIBRETRO__)	// Already tested platforms are added here
#define INCLUDE_FLACMP3_SUPPORT
#endif // BUILD_WIN32


#ifdef INCLUDE_FLACMP3_SUPPORT

// dr_flac implementation is now in dep/libs/libchdr/libchdr_flac.c
// Only include header here (no DR_FLAC_IMPLEMENTATION)
#include "dr_flac.h"

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

enum { LATCH_NONE = 0, LATCH_STOP = 1, LATCH_RETRIG = 2 };

struct sample_format
{
	UINT8 *data;
	UINT32 length;
	UINT64 position;
	UINT8 playing;
	UINT8 loop;
	UINT8 flags;
	UINT8 latch;
	INT32 playback_rate; // 100 = 100%, 200 = 200%,
	double gain[2];
	double gain_target[2]; // ramp gain up or down to gain_target (see BurnSampleSetRouteFade())
	INT32 output_dir[2];
	UINT32 loop_start; // frame (at nBurnSoundRate) a looping sample restarts from, 0 = beginning
	UINT32 src_rate;   // sample rate of the file before resampling (for sample-exact loop points)
	struct sample_stream *stream; // decoded on the fly (NOSTOREF .mp3/.flac), NULL = all in data
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
	sample_ptr->src_rate = sample_rate;
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
	sample_ptr->src_rate = sample_rate;
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
	sample_ptr->src_rate = sample_rate;
	sample_ptr->playing  = 0;
	sample_ptr->position = 0;

	bprintf(0, _T("Loaded MP3: %d frames (%dHz -> %dHz)\n"),
		sample_ptr->length, sample_rate, nBurnSoundRate);
}
#endif

// for stream-sync
// ---------------------------------------------------------------------------
// Streaming: load-on-demand (SAMPLE_NOSTOREF) .mp3 and .flac samples are decoded on the fly.
// Only the compressed file is kept in memory, plus a small window decoded at the file's rate
// and a small window resampled to nBurnSoundRate.  Random access (BurnSampleSetPosition(),
// loop points, state loads) seeks the decoder.
//
// mp3 seeking: dr_mp3's seek table lands a few thousand frames off (0.7.4), and its exact
// seek decodes from the start.  So as it plays, the decoder's state is saved every few seconds
// and at the loop point; a seek restores the nearest one before it and decodes forward from there.

#define STREAM_SRC_FRAMES	4096	// decoded window, frames at the file's rate
#define STREAM_OUT_FRAMES	2048	// resampled window, frames at nBurnSoundRate
#define STREAM_MAX_CHANNELS	8
#define STREAM_CHECKPOINT_SECS	5		// mp3: decoder state saved every this many seconds

#ifdef INCLUDE_FLACMP3_SUPPORT

enum { STREAM_MP3 = 1, STREAM_FLAC = 2 };

struct stream_checkpoint {
	UINT64 pos;					// frame (at the file's rate) the decoder state is at
	drmp3 state;
};

struct sample_stream {
	INT32 type;
	UINT8 *file;				// the compressed file (malloc'd, owned - the decoder reads from it)
	drmp3 mp3;
	stream_checkpoint *cp;		// sorted by pos
	UINT32 cp_count, cp_cap;
	UINT64 cp_interval;			// frames between checkpoints
	UINT64 loop_src;			// loop point at the file's rate (0 = none), gets a checkpoint of its own
	drflac *flac;
	UINT32 channels;
	UINT32 src_rate;
	UINT64 src_frames;			// length, frames at the file's rate
	UINT64 dec_pos;				// the next frame the decoder will give
	INT16 *tmp;					// decoder output, all channels

	INT16 src[STREAM_SRC_FRAMES * 2];	// stereo, at the file's rate
	UINT64 src_start;
	UINT32 src_len;

	INT16 out[STREAM_OUT_FRAMES * 2];	// stereo, at nBurnSoundRate
	UINT32 out_start;
	UINT32 out_len;
};

static const INT16 stream_silence[2] = { 0, 0 };

static void stream_close(sample_stream *st)
{
	if (st == NULL) return;

	if (st->type == STREAM_MP3) drmp3_uninit(&st->mp3);
	if (st->flac) drflac_close(st->flac);
	free(st->cp);
	free(st->tmp);
	free(st->file);
	free(st);
}

// mp3 checkpoints
static stream_checkpoint *stream_cp_find(sample_stream *st, UINT64 pos, INT32 bExact)	// last one <= pos
{
	stream_checkpoint *best = NULL;
	for (UINT32 i = 0; i < st->cp_count && st->cp[i].pos <= pos; i++) best = &st->cp[i];

	return (bExact && best && best->pos != pos) ? NULL : best;
}

static void stream_cp_save(sample_stream *st)
{
	if (st->dec_pos == 0 || stream_cp_find(st, st->dec_pos, 1)) return;

	if (st->cp_count == st->cp_cap) {
		UINT32 cap = st->cp_cap ? st->cp_cap * 2 : 16;
		stream_checkpoint *cp = (stream_checkpoint*)realloc(st->cp, cap * sizeof(stream_checkpoint));
		if (cp == NULL) return;
		st->cp = cp;
		st->cp_cap = cap;
	}

	UINT32 i = st->cp_count;
	while (i > 0 && st->cp[i - 1].pos > st->dec_pos) {
		st->cp[i] = st->cp[i - 1];
		i--;
	}
	st->cp[i].pos = st->dec_pos;
	memcpy(&st->cp[i].state, &st->mp3, sizeof(drmp3));	// (memory decoders keep no buffers of their own)
	st->cp_count++;
}

static UINT64 stream_cp_next(sample_stream *st)		// next frame a checkpoint goes at
{
	UINT64 next = (st->dec_pos / st->cp_interval + 1) * st->cp_interval;
	if (st->loop_src > st->dec_pos && st->loop_src < next) next = st->loop_src;

	return next;
}

// decode up to "frames" frames into dst (stereo, or NULL to skip), returns the number decoded
static UINT32 stream_decode(sample_stream *st, UINT32 frames, INT16 *dst)
{
	const UINT32 ch = st->channels;
	UINT32 total = 0;

	while (frames > 0) {
		UINT32 n = frames;
		UINT64 next = 0;

		if (st->type == STREAM_MP3) {
			next = stream_cp_next(st);
			if (st->dec_pos + n > next) n = (UINT32)(next - st->dec_pos);	// stop there, to save the state
		}

		UINT64 got = 0;
		if (st->type == STREAM_MP3)  got = drmp3_read_pcm_frames_s16(&st->mp3, n, st->tmp);
		if (st->type == STREAM_FLAC) got = drflac_read_pcm_frames_s16(st->flac, n, st->tmp);

		if (dst) {
			for (UINT64 i = 0; i < got; i++, dst += 2) {
				dst[0] = st->tmp[i * ch];
				dst[1] = st->tmp[i * ch + ((ch > 1) ? 1 : 0)];	// mono: both sides
			}
		}

		st->dec_pos += got;
		total += (UINT32)got;
		frames -= (UINT32)got;

		if (st->type == STREAM_MP3 && st->dec_pos == next) stream_cp_save(st);

		if (got < n) break;					// end of file / error
	}

	return total;
}

static void stream_seek(sample_stream *st, UINT64 frame)
{
	if (st->type == STREAM_FLAC) {
		drflac_seek_to_pcm_frame(st->flac, frame);			// exact, and quick
		st->dec_pos = frame;
		return;
	}

	// mp3: from the nearest saved state before "frame" (or from where it is, if that's nearer)
	if (!(st->dec_pos <= frame && (frame - st->dec_pos) < st->cp_interval)) {
		stream_checkpoint *cp = stream_cp_find(st, frame, 0);
		if (cp && !(st->dec_pos <= frame && st->dec_pos >= cp->pos)) {
			memcpy(&st->mp3, &cp->state, sizeof(drmp3));
			st->dec_pos = cp->pos;
		} else if (st->dec_pos > frame) {
			drmp3_seek_to_pcm_frame(&st->mp3, 0);			// (exact)
			st->dec_pos = 0;
		}
	}

	while (st->dec_pos < frame) {
		UINT64 n = frame - st->dec_pos;
		if (n > STREAM_SRC_FRAMES) n = STREAM_SRC_FRAMES;
		if (stream_decode(st, (UINT32)n, NULL) == 0) break;
	}
}

// decode a new source window, starting at frame i
static void stream_src_fill(sample_stream *st, UINT64 i)
{
	UINT32 keep = 0;

	if (st->src_len && i == st->src_start + st->src_len && st->dec_pos == i) {
		// carrying on: keep the last frame, interpolation needs it at the window edge
		st->src[0] = st->src[(st->src_len - 1) * 2 + 0];
		st->src[1] = st->src[(st->src_len - 1) * 2 + 1];
		keep = 1;
	} else if (st->dec_pos != i) {
		stream_seek(st, i);
	}

	const UINT64 start = st->dec_pos;
	const UINT32 got = stream_decode(st, STREAM_SRC_FRAMES - keep, st->src + keep * 2);

	st->src_start = start - keep;
	st->src_len = keep + got;
}

static inline const INT16 *stream_src_frame(sample_stream *st, UINT64 i)
{
	if (i >= st->src_frames) i = st->src_frames - 1;

	if (i < st->src_start || i >= st->src_start + st->src_len) {
		stream_src_fill(st, i);
		if (i >= st->src_start + st->src_len) return stream_silence;	// decoder error
	}

	return &st->src[(i - st->src_start) * 2];
}

// resample a new output window, starting at frame "frame" (at nBurnSoundRate)
static void stream_out_fill(sample_format *s, UINT32 frame)
{
	sample_stream *st = s->stream;
	const bool native = (st->src_rate == (UINT32)nBurnSoundRate);
	const double step = (double)st->src_rate / nBurnSoundRate;
	UINT32 n;

	st->loop_src = (s->loop && s->loop_start < s->length) ? (UINT64)(s->loop_start * step) : 0;

	for (n = 0; n < STREAM_OUT_FRAMES && frame + n < s->length; n++) {
		INT16 *out = &st->out[n * 2];

		if (native) {
			const INT16 *f = stream_src_frame(st, frame + n);
			out[0] = f[0];
			out[1] = f[1];
			continue;
		}

		// linear interpolation, like the whole-file converters
		const double src_pos = (frame + n) * step;
		const UINT64 i0 = (UINT64)src_pos;
		const double frac = src_pos - (double)i0;

		const INT16 *f = stream_src_frame(st, i0);
		const INT32 l0 = f[0], r0 = f[1];		// (the next fetch can move the window)
		f = stream_src_frame(st, i0 + 1);

		out[0] = (INT16)(l0 + (f[0] - l0) * frac);
		out[1] = (INT16)(r0 + (f[1] - r0) * frac);
	}

	st->out_start = frame;
	st->out_len = n;
}

static inline const INT16 *sample_stream_frame(sample_format *s, UINT32 frame)
{
	sample_stream *st = s->stream;

	if (frame < st->out_start || frame >= st->out_start + st->out_len) {
		stream_out_fill(s, frame);
		if (st->out_len == 0) return stream_silence;
	}

	return &st->out[(frame - st->out_start) * 2];
}

// open "file" for streaming into sample_ptr, takes ownership of file (malloc'd) on success
static INT32 make_stream(UINT8 *file, UINT32 len, INT32 type)
{
	sample_stream *st = (sample_stream*)calloc(1, sizeof(sample_stream));
	if (st == NULL) return 0;

	st->type = type;

	if (type == STREAM_MP3) {
		if (!drmp3_init_memory(&st->mp3, file, len, NULL)) {
			free(st);
			return 0;
		}
		st->channels = st->mp3.channels;
		st->src_rate = st->mp3.sampleRate;
		st->src_frames = drmp3_get_pcm_frame_count(&st->mp3);	// (from the Xing/Info tag, or a quick scan of the frame headers)
		st->cp_interval = (UINT64)st->src_rate * STREAM_CHECKPOINT_SECS;
	}

	if (type == STREAM_FLAC) {
		st->flac = drflac_open_memory(file, len, NULL);
		if (st->flac == NULL) {
			free(st);
			return 0;
		}
		st->channels = st->flac->channels;
		st->src_rate = st->flac->sampleRate;
		st->src_frames = st->flac->totalPCMFrameCount;
	}

	if (st->channels == 0 || st->channels > STREAM_MAX_CHANNELS || st->src_rate == 0 || st->src_frames == 0 ||
		(st->tmp = (INT16*)malloc(STREAM_SRC_FRAMES * st->channels * sizeof(INT16))) == NULL)
	{
		st->file = NULL;	// not ours yet
		stream_close(st);
		return 0;
	}

	st->file = file;
	if (type == STREAM_MP3) drmp3_seek_to_pcm_frame(&st->mp3, 0);
	if (type == STREAM_FLAC) drflac_seek_to_pcm_frame(st->flac, 0);
	st->dec_pos = 0;

	sample_ptr->stream   = st;
	sample_ptr->data     = (UINT8*)st->out;		// non-NULL = loaded
	sample_ptr->length   = (UINT32)((double)st->src_frames * nBurnSoundRate / st->src_rate + 0.5);
	sample_ptr->src_rate = st->src_rate;
	sample_ptr->playing  = 0;
	sample_ptr->position = 0;

	bprintf(0, _T("Streaming %s: %d frames (%dHz -> %dHz)\n"), (type == STREAM_MP3) ? _T("MP3") : _T("FLAC"),
		sample_ptr->length, st->src_rate, nBurnSoundRate);

	return 1;
}

#endif // INCLUDE_FLACMP3_SUPPORT

static void sample_free_data(sample_format *s)
{
#ifdef INCLUDE_FLACMP3_SUPPORT
	if (s->stream) {
		stream_close(s->stream);
		s->stream = NULL;
		s->data = NULL;
		return;
	}
#endif

	BurnFree(s->data);
}

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

	if (BurnSampleGetStatus(sample) == SAMPLE_PLAYING) {
		//bprintf(0, _T("BurnSamplePlay(): REtrig sample %x  fr %d\n"), sample, nCurrentFrame);
		sample_ptr->latch = LATCH_RETRIG;
	} else {
		//bprintf(0, _T("BurnSamplePlay(): play sample %x  fr %d\n"), sample, nCurrentFrame);
		sample_ptr->playing = 1;
		sample_ptr->position = 0;
	}
}

void BurnSampleChannelPlay(INT32 channel, INT32 sample, INT32 loop)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleChannelPlay called without init\n"));
	if (channel >= MAX_CHANNEL) bprintf(PRINT_ERROR, _T("BurnSampleChannelPlay called with invalid channel (%d), max is %d\n"), channel, MAX_CHANNEL);
#endif

	if (sample >= nTotalSamples || channel >= MAX_CHANNEL) return;

	if (sample_channels[channel] != sample) {
		// different sample on this channel, stop previous sample first
		BurnSampleChannelStop(channel, true);
	}

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

void BurnSampleStop(INT32 sample, bool softstop)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleStop called without init\n"));
#endif

	if (sample >= nTotalSamples) return;

	BurnSampleSync();

	sample_ptr = &samples[sample];

	if (softstop && BurnSampleGetStatus(sample) == SAMPLE_PLAYING) {
		//bprintf(0, _T("BurnSampleStop(): Soft-stop sample %x\n"), sample);
		sample_ptr->latch = LATCH_STOP;
	} else {
		sample_ptr->playing = 0;
		sample_ptr->position = 0;
		//sample_ptr->playback_rate = 100; // 100% // on load and reset, only!
	}
}

void BurnSampleStopAll(bool softstop)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleStopAll called without init\n"));
#endif

	for (INT32 i = 0; i < nTotalSamples; i++) {
		BurnSampleStop(i, softstop);
	}
}

void BurnSampleChannelStop(INT32 channel, bool softstop)
{
#if defined FBNEO_DEBUG
	if (channel >= MAX_CHANNEL) bprintf(PRINT_ERROR, _T("BurnSampleChannelStop called with invalid channel (%d), max is %d\n"), channel, MAX_CHANNEL);
#endif

	BurnSampleStop(sample_channels[channel], softstop);
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

	if (sample_channels[channel] == MAX_CHANNEL-1) {
		return SAMPLE_STOPPED; // nothing here yet
	}

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

void BurnSampleChannelSetPlaybackRate(INT32 channel, INT32 rate)
{
#if defined FBNEO_DEBUG
	if (channel >= MAX_CHANNEL) bprintf(PRINT_ERROR, _T("BurnSampleChannelSetPlaybackRate called with invalid channel (%d), max is %d\n"), channel, MAX_CHANNEL);
#endif

	BurnSampleSetPlaybackRate(sample_channels[channel], rate);
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

	for (INT32 i = 0; i < MAX_CHANNEL; i++) {
		sample_channels[i] = MAX_CHANNEL-1;
	}

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

// ---------------------------------------------------------------------------
// Sample source: by default the driver's own sample list + support/samples/<samplename>.zip,
// or an external list set with BurnSampleSetSource() (used by burn_ost.cpp for OST packs)

static const BurnSampleSource *pSampleSource = NULL;
static char szSampleArchive[MAX_PATH * 2];	// zip/7z path without extension ("" = none)
static char szSampleFolder[MAX_PATH * 2];	// folder for loose files, with trailing slash ("" = none)
static INT32 bSampleArchiveOk = 0;

void BurnSampleSetSource(const BurnSampleSource *pSrc)
{
	pSampleSource = pSrc;
}

static INT32 SampleSourceFlags(INT32 i)
{
	if (pSampleSource) {
		return (i < pSampleSource->nCount) ? pSampleSource->GetFlags(i) : 0;
	}

	struct BurnSampleInfo si;
	BurnDrvGetSampleInfo(&si, i);
	return si.nFlags;
}

static const char *SampleSourceName(INT32 i)
{
	if (pSampleSource) {
		return pSampleSource->GetName(i);
	}

	char *szName = NULL;
	BurnDrvGetSampleName(&szName, i, 0);
	return szName;
}

static void SampleSourcePaths()
{
	szSampleArchive[0] = szSampleFolder[0] = 0;

	if (pSampleSource) {
		if (pSampleSource->szArchive) snprintf(szSampleArchive, sizeof(szSampleArchive), "%s", pSampleSource->szArchive);
		if (pSampleSource->szFolder)  snprintf(szSampleFolder,  sizeof(szSampleFolder),  "%s", pSampleSource->szFolder);
	} else {
		snprintf(szSampleArchive, sizeof(szSampleArchive), "%s%s", _TtoA(SAMPLE_DIRECTORY), BurnDrvGetTextA(DRV_SAMPLENAME));
	}

	// test to see if the archive exists (saves opening it for every sample when it doesn't)
	bSampleArchiveOk = 0;
	if (szSampleArchive[0]) {
		static const char *arc_ext[] = { ".zip", ".7z" };
		for (INT32 e = 0; e < 2 && !bSampleArchiveOk; e++) {
			char path[MAX_PATH * 2 + 4];
			snprintf(path, sizeof(path), "%s%s", szSampleArchive, arc_ext[e]);
			FILE *test = fopen(path, "rb");
			if (test) {
				bSampleArchiveOk = 1;
				fclose(test);
			}
		}
	}
}

static void *SampleLoadLooseFile(const char *path, INT32 *pnLen) // malloc()'d, like ZipLoadOneFile
{
	*pnLen = 0;
	FILE *fp = fopen(path, "rb");
	if (fp == NULL) return NULL;

	fseek(fp, 0, SEEK_END);
	long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	void *data = (len > 0) ? malloc(len) : NULL;
	if (data && fread(data, 1, len, fp) == (size_t)len) {
		*pnLen = (INT32)len;
	} else {
		free(data);
		data = NULL;
	}
	fclose(fp);

	return data;
}

// find + decode sample i into samples[i]: archive first, then loose file; .wav, then .flac, then .mp3
static INT32 SampleLoadInto(INT32 i)
{
	static const char *ext[] = { ".wav",
#ifdef INCLUDE_FLACMP3_SUPPORT
		".flac", ".mp3"
#endif
	};

	const char *szName = SampleSourceName(i);
	if (szName == NULL || szName[0] == 0) return 0;

	sample_ptr = &samples[i];

	for (UINT32 e = 0; e < sizeof(ext) / sizeof(ext[0]); e++) {
		char szFile[1024];
		snprintf(szFile, sizeof(szFile), "%s%s", szName, ext[e]);

		void *destination = NULL;
		INT32 length = 0;

		if (bSampleArchiveOk) {
			ZipLoadOneFile(szSampleArchive, szFile, &destination, &length);
		}
		if (length == 0 && szSampleFolder[0]) {
			char path[MAX_PATH * 2 + 1024];
			snprintf(path, sizeof(path), "%s%s", szSampleFolder, szFile);
			destination = SampleLoadLooseFile(path, &length);
		}

		if (length) {
			bprintf(0, _T("Loading \"%S\" @ %d: "), szFile, i);
#ifdef INCLUDE_FLACMP3_SUPPORT
			// load-on-demand mp3/flac: decode while playing, instead of all of it now
			if (e > 0 && (sample_ptr->flags & SAMPLE_NOSTOREF)) {
				if (make_stream((UINT8*)destination, length, (e == 1) ? STREAM_FLAC : STREAM_MP3)) {
					return 1;	// the stream owns "destination" now
				}
				bprintf(0, _T("can't stream, decoding all of it: "));
			}
#endif
			switch (e) {
				case 0: make_raw((UINT8*)destination, length); break;
#ifdef INCLUDE_FLACMP3_SUPPORT
				case 1: make_raw_flac((UINT8*)destination, length); break;
				case 2: make_raw_mp3((UINT8*)destination, length); break;
#endif
			}
			free(destination); // ZipLoadOneFile & SampleLoadLooseFile use malloc()
			return (sample_ptr->data != NULL);
		}
	}

	bprintf(0, _T("Loading \"%S\" @ %d: sample missing!\n"), szName, i);
	return 0;
}

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

	if (pSampleSource == NULL && BurnDrvGetTextA(DRV_SAMPLENAME) == NULL) { // called with no samples
		nTotalSamples = 0;
		return;
	}

	SampleSourcePaths();

	soundbuf = (INT16*)BurnMalloc(0x1000 * 2 * 2); // mixing buffer

	if (pSampleSource) {
		nTotalSamples = pSampleSource->nCount;
	} else {
		INT32 nSampleOffset = -1;
		while (SampleSourceFlags(++nSampleOffset)) nTotalSamples++;
	}

	samples = (sample_format*)BurnMalloc(sizeof(sample_format) * nTotalSamples);
	memset (samples, 0, sizeof(sample_format) * nTotalSamples);

	BurnSetProgressRange(0.99); // Expand Progress bar
	BurnUpdateProgress(0.0, _T("Loading samples..."), 0);

	for (INT32 i = 0; i < nTotalSamples; i++) {
		BurnUpdateProgress(1.0 / nTotalSamples, NULL, 0);

		INT32 nFlags = SampleSourceFlags(i);
		if (nFlags == 0) break;

		sample_ptr = &samples[i];

		// set defaults before NOSTORE check!
		sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] = 1.00;
		sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_1] = 1.00;

		sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] = 1.00;
		sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_2] = 1.00;

		sample_ptr->output_dir[BURN_SND_SAMPLE_ROUTE_1] = BURN_SND_ROUTE_BOTH;
		sample_ptr->output_dir[BURN_SND_SAMPLE_ROUTE_2] = BURN_SND_ROUTE_BOTH;
		sample_ptr->playback_rate = 100;

		if (nFlags & SAMPLE_NOSTOREF || bForceNostore) {
			sample_ptr->flags = nFlags | (bForceNostore ? SAMPLE_NOSTOREF : 0);
			sample_ptr->data = NULL;
			continue;
		}

		sample_ptr->flags = nFlags;

		if (SampleLoadInto(i) == 0) {
			sample_ptr->flags = SAMPLE_IGNORE;
		}
	}
}

void BurnSampleInitOne(INT32 sample)
{
	if (sample >= nTotalSamples) {
		return;
	}

	{
		// free the other load-on-demand samples, except ones still playing (so music can crossfade/overlap)
		struct sample_format *clr_ptr = &samples[0];

		int i = 0;
		while (i < nTotalSamples) {
			if (clr_ptr->data != NULL && i != sample && (clr_ptr->flags & SAMPLE_NOSTOREF) && clr_ptr->playing == 0) {
				sample_free_data(clr_ptr);
				clr_ptr->playback_rate = 100;
				clr_ptr->data = NULL;
			}

			clr_ptr++, i++;
		}
	}

	sample_ptr = &samples[sample];

	if ((sample_ptr->flags & SAMPLE_NOSTOREF) == 0) {
		return;
	}

	if (sample_ptr->data != NULL || sample_ptr->flags == SAMPLE_IGNORE) {
		return;
	}

	// the decoders reset playing/position, keep them so a reload mid-play
	// (state load, resume after being freed) continues where it was
	const UINT8 playing = sample_ptr->playing;
	const UINT64 position = sample_ptr->position;

	SampleLoadInto(sample);

	sample_ptr = &samples[sample];
	sample_ptr->playing = playing;
	sample_ptr->position = position;
}

UINT32 BurnSampleGetLength(INT32 sample)
{
	if (sample >= nTotalSamples) return 0;

	return samples[sample].length;
}

UINT32 BurnSampleGetSourceRate(INT32 sample)
{
	if (sample >= nTotalSamples) return 0;

	return samples[sample].src_rate;
}

void BurnSampleSetLoopStart(INT32 sample, UINT32 frame)
{
	if (sample >= nTotalSamples) return;

	BurnSampleSync();

	samples[sample].loop_start = frame;
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

void BurnSampleSetRouteFadeAllSamples(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_SamplesInitted) bprintf(PRINT_ERROR, _T("BurnSampleSetRouteAllSamples called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("BurnSampleSetRouteAllSamples called with invalid index %i\n"), nIndex);
#endif

	if (!nTotalSamples) return;

	for (INT32 i = 0; i < nTotalSamples; i++) {
		BurnSampleSetRouteFade(i, nIndex, nVolume, nRouteDir);
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
			sample_free_data(sample_ptr);
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
	pSampleSource = NULL;
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

static bool sample_low(INT32 sam)
{
	sam = d_abs(sam);
	return ( sam == 0 );
}

static inline void latch_ramp_gain(double &gain1, double &gain2)
{
	// note: ramp steps larger than 0.005 will cause clicks with low frequencies.
	if (gain1 >= 0.005) {
		gain1 -= 0.005;
	} else if (gain1 >= 0.001) {
		gain1 -= 0.001;
	} else if (gain1 < 0.001) {
		gain1 = 0.0;
	}
	if (gain2 >= 0.005) {
		gain2 -= 0.005;
	} else if (gain2 >= 0.001) {
		gain2 -= 0.001;
	} else if (gain2 < 0.001) {
		gain2 = 0.0;
	}
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
		if (sample_ptr->playing == 0) {
			sample_ptr->latch = 0; // clear latch (for now!)
			continue;
		}

		if (sample_ptr->data == NULL) {
			if (sample_ptr->flags & SAMPLE_NOSTOREF) {
				BurnSampleInitOne(i); // (re)load on demand, ie. after a state load
				sample_ptr = &samples[i];
			} else {
				// something went wrong here :)
				continue;
			}
		}

		if (sample_ptr->length == 0 || sample_ptr->data == NULL) {
			sample_ptr->latch = 0;
			continue;
		}

		INT32 playlen = pLen;
		INT32 length = sample_ptr->length;
		UINT64 pos = sample_ptr->position;
		INT32 playback_rate = (0x10000 * sample_ptr->playback_rate) / 100;
		INT16 *dst = pDest;
		INT16 *dat = (INT16*)sample_ptr->data;

		length *= 2; // (stereo) used to ensure position is within bounds

		for (INT32 j = 0; j < playlen; j++, dst+=2) {
			INT32 nLeftSample = 0, nRightSample = 0;

			if (sample_ptr->loop && (pos / 0x10000) >= sample_ptr->length) {
				// wrap to the loop point (0 = beginning, same as before), keeping the fractional overshoot
				const UINT32 loop_start = (sample_ptr->loop_start < sample_ptr->length) ? sample_ptr->loop_start : 0;
				pos -= (UINT64)(sample_ptr->length - loop_start) * 0x10000;
			}

			const UINT32 current_pos = (pos / 0x10000);
			const UINT32 position = current_pos * 2;

			if (sample_ptr->loop == 0) // if not looping, check to make sure sample is in bounds
			{
				// if sample position is greater than length, stop playback
				if (current_pos >= sample_ptr->length) {
					BurnSampleStop_INT(i);
					pos = 0;
					break;
				}
			}

#ifdef INCLUDE_FLACMP3_SUPPORT
			const INT16 *frame = (sample_ptr->stream) ? sample_stream_frame(sample_ptr, current_pos) : &dat[position % length];
#else
			const INT16 *frame = &dat[position % length];
#endif

			if ((sample_ptr->output_dir[BURN_SND_SAMPLE_ROUTE_1] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				nLeftSample += (INT32)(frame[0] * sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1]);
			}
			if ((sample_ptr->output_dir[BURN_SND_SAMPLE_ROUTE_1] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				nRightSample += (INT32)(frame[0] * sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1]);
			}

			if ((sample_ptr->output_dir[BURN_SND_SAMPLE_ROUTE_2] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				nLeftSample += (INT32)(frame[1] * sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2]);
			}
			if ((sample_ptr->output_dir[BURN_SND_SAMPLE_ROUTE_2] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				nRightSample += (INT32)(frame[1] * sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2]);
			}

			dst[0] = BURN_SND_CLIP(dst[0] + nLeftSample);
			dst[1] = BURN_SND_CLIP(dst[1] + nRightSample);

			bool position_increment = true;

			if (sample_ptr->latch & (LATCH_RETRIG | LATCH_STOP)) {
				// LATCH_RETRIG: ramp down and then re-start sample, to avoid clicks
				// LATCH_STOP: ramp down and stop sample.

				//bprintf(0, _T("[%.15g  %.15g]\n"),sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1], sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2]);

				if (sample_low(nLeftSample) &&
					sample_low(nRightSample) )
				{
					position_increment = false; // we don't want to skip the first sample if starting over w/LATCH_RETRIG
					pos = 0;
					sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] = sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_1];
					sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] = sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_2];

					if (sample_ptr->latch & LATCH_STOP) {
						BurnSampleStop_INT(i);
						sample_ptr->latch = LATCH_NONE;
						//bprintf(0, _T("[soft-stop %d!]\n"), i);
						break; // break out of this channel's loop
					} else if (sample_ptr->latch & LATCH_RETRIG) {
						position_increment = false; // we don't want to skip the first sample if starting over w/LATCH_RETRIG
						//bprintf(0, _T("[soft-retrig %d!]\n"), i);
						sample_ptr->latch = LATCH_NONE;
					}
				}
				latch_ramp_gain(sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1], sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2]);
			} else {
				if (bNiceFadeVolume) {
					if (sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] != sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_1]) {
						if (sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] > sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_1]) {
							sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] -= 0.01;
						} else if (sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] < sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_1] && sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_1] != 0.0) {
							sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_1] += 0.01;
						}
					}
					if (sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] != sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_2]) {
						if (sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] > sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_2]) {
							sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] -= 0.01;
						} else if (sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] < sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_2] && sample_ptr->gain_target[BURN_SND_SAMPLE_ROUTE_2] != 0.0) {
							sample_ptr->gain[BURN_SND_SAMPLE_ROUTE_2] += 0.01;
						}
					}
				}
			}
			if (position_increment) pos+=playback_rate;
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
			SCAN_VAR(sample_ptr->latch);
			SCAN_VAR(sample_ptr->gain);
			SCAN_VAR(sample_ptr->gain_target);
		}

		SCAN_VAR(sample_channels);
	}
}

// dink's super handy macros
void splay(INT32 sam, double volume, bool checkplay, bool loop)
{
	if ( (checkplay && BurnSampleGetStatus(sam) == SAMPLE_STOPPED) || !checkplay ) {
		BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_1, volume, BURN_SND_ROUTE_BOTH);
		BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_2, volume, BURN_SND_ROUTE_BOTH);

		BurnSampleSetLoop(sam, loop);

		BurnSamplePlay(sam);
	}
}

void splayex(INT32 sam, double volume, INT32 rate, bool checkplay, bool loop)
{
	if ( (checkplay && BurnSampleGetStatus(sam) == SAMPLE_STOPPED) || !checkplay ) {
		BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_1, volume, BURN_SND_ROUTE_BOTH);
		BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_2, volume, BURN_SND_ROUTE_BOTH);

		BurnSampleSetLoop(sam, loop);
		BurnSampleSetPlaybackRate(sam, rate);

		BurnSamplePlay(sam);
	}
}

void splaych(INT32 ch, INT32 sam, double volume, bool checkplay, bool loop)
{
	if ( (checkplay && BurnSampleGetChannelStatus(ch) == SAMPLE_STOPPED) || !checkplay ) {
		BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_1, volume, BURN_SND_ROUTE_BOTH);
		BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_2, volume, BURN_SND_ROUTE_BOTH);

		BurnSampleChannelPlay(ch, sam, loop);
	}
}

void splayexch(INT32 ch, INT32 sam, double volume, INT32 rate, bool checkplay, bool loop)
{
	if ( (checkplay && BurnSampleGetChannelStatus(ch) == SAMPLE_STOPPED) || !checkplay ) {
		BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_1, volume, BURN_SND_ROUTE_BOTH);
		BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_2, volume, BURN_SND_ROUTE_BOTH);

		BurnSampleSetPlaybackRate(sam, rate);

		BurnSampleChannelPlay(ch, sam, loop);
	}
}

void sstop(INT32 sam)
{
	BurnSampleStop(sam, true); // +softstop
}

void sstopch(INT32 ch)
{
	BurnSampleChannelStop(ch, true);
}

bool splaying(INT32 sam)
{
	return (BurnSampleGetStatus(sam) == SAMPLE_PLAYING);
}

bool splayingch(INT32 ch)
{
	return (BurnSampleGetChannelStatus(ch) == SAMPLE_PLAYING);
}

