// FBNeo sound-streams & re-sampler, dink sept. 2021

// usage?
// -- examples by code / good working examples --
// c140.cpp, 2ch stereo  driver: pst90s/d_namcos2.cpp & d_namcona1.cpp
// tms5220.cpp, 1ch mono  driver: pre90s/d_jedi.cpp & d_starwars.cpp

// -- global: setting up the Stream object --
//   #include "stream.h"
//   static Stream stream;

// -- init --
//   stream.init(soundchips_soundrate, nBurnSoundRate, ch#, add_to_stream, soundchips_update_function);
//   stream.set_volume(1.00);
//   stream.set_route(BURN_SND_ROUTE_BOTH);
//   note: stream.set_volume & stream.set_route aren't needed if you intend to
//         use the defaults of 1.00 / BURN_SND_ROUTE_BOTH

// -- exit --
//   stream.exit();

// -- before register writes or reads - see "synchronicity" below --
//   stream.update();

// -- render samples --
//   stream.render(pBurnSoundOut, nBurnSoundLen);

// -- synchronicity: should the stream sync/update with the cpu?
//   stream.set_buffered([CPU]TotalCycles, CPUMhz);
//   note: make sure [CPU]NewFrame(); is called at the top of each frame!

// -- input samplerate change
//   stream.set_rate(new_rate);
//   doing this live (during a frame) causes the buffer to restart!
//   * berzerk, (atari) games with tms5220/tms5110 write silence before
//   rate changes, so this is not a problem.
//   should we encounter something that needs to rate change several times
//   per frame, while outputting non-silence then a little re-write is in-order..

struct Stream {
	// the re-sampler section
	UINT32 nSampleSize;
	UINT32 nSampleSize_Otherway;
	UINT32 nSampleRateFrom;
	UINT32 nSampleRateTo;
	INT64 nFractionalPosition; // 48.16 [whole.partial samples]
	#define MAX_CHANNELS  8
	INT32 nChannels;
	bool bAddStream;

	INT16 *in_buffer[MAX_CHANNELS]; // resampler in-buffers
	INT32 out_buffer_size;

	void init(INT32 rate_from, INT32 rate_to, INT32 channels, bool add_to_stream, void (*update_stream)(INT16 **, INT32 )) {
		nFractionalPosition = 0;
		bAddStream = add_to_stream;
		nSampleRateFrom = rate_from;
		nSampleRateTo = rate_to;
		nChannels = channels;
		set_rate(rate_from);

		stream_init(update_stream);
	}
	void set_rate(INT32 rate_from) {
		nSampleRateFrom = rate_from;
		nSampleSize = (UINT64)nSampleRateFrom * (1 << 16) / ((nSampleRateTo == 0) ? 44100 : nSampleRateTo);
		nSampleSize_Otherway = (UINT64)((nSampleRateTo == 0) ? 44100 : nSampleRateTo) * (1 << 16) / ((nSampleRateFrom == 0) ? 44100 : nSampleRateFrom);
		nPosition = 0; // re-start the frame
	}
	void exit() {
		nSampleSize = nFractionalPosition = 0;
		stream_exit();
	}
	INT32 samples_to_host(INT32 samples) {
		return (INT32)((UINT64)nSampleSize_Otherway * samples) / (1 << 16);
	}
	INT32 samples_to_source(INT32 samples) {
		return (INT32)(((UINT64)nSampleSize * samples) / (1 << 16)) + 1;
	}
	void render(INT16 *out_buffer, INT32 samples) {
		// make sure buffers are full for this frame
		UpdateStream(1);

		for (INT32 i = 0; i < nChannels; i++) {
			// prepare the in-buffers
			in_buffer[i] = soundbuf[i];
			// make room for previous frame's "carry-over" sample (aka in_buffer[-1])
			in_buffer[i]++;
		}
		out_buffer_size = samples_to_source(samples);

		if (nSampleRateFrom == nSampleRateTo) {
			// stream(s) running at same samplerate as host
			samplesample(out_buffer, samples);
		} else if (nSampleRateFrom > nSampleRateTo) {
			// stream(s) greater than the samplerate of host
			downsample(out_buffer, samples);
		} else if (nSampleRateFrom < nSampleRateTo) {
			// stream(s) less than the samplerate of host
			upsample(out_buffer, samples);
		}

		// figure out if we have extra whole samples
		nPosition = out_buffer_size - (nFractionalPosition >> 16);

		// 1) take the last processed sample and store it into in_buffer[-1]
		// so that interpolation can be carried out between frames.
		// 2) store the extra whole samples at in_buffer[0+], returning the
		// number of whole samples in nPosition (stream-buffer position)
		for (INT32 ch = 0; ch < nChannels; ch++) {
			for (INT32 i = -1; i < nPosition; i++) {
				in_buffer[ch][i] = in_buffer[ch][i + (nFractionalPosition >> 16)];
			}
		}

		// mask off the whole samples from our accumulator
		nFractionalPosition &= 0xffff;

	}
	void samplesample(INT16 *out_buffer, INT32 samples) {
		for (INT32 i = 0; i < samples; i++, out_buffer += 2, nFractionalPosition += nSampleSize) {
			INT32 source_pos = (nFractionalPosition >> 16);
			INT32 sample_l = 0;
			INT32 sample_r = 0;

			for (INT32 ch = 0; ch < nChannels; ch++) {
				INT32 sample = in_buffer[ch][source_pos];

				if (ch & 1) sample_r += sample; else sample_l += sample;
			}

			if (nChannels == 1) sample_r = sample_l; // duplicate left ch. for MONO

			// apply volume, check for clipping and mix-down
			sample_l = BURN_SND_CLIP(sample_l * volume);
			sample_r = BURN_SND_CLIP(sample_r * volume);

			sample_l = ((route & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) ? sample_l : 0;
			sample_r = ((route & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) ? sample_r : 0;

			if (bAddStream) {
				out_buffer[0] = BURN_SND_CLIP(out_buffer[0] + sample_l);
				out_buffer[1] = BURN_SND_CLIP(out_buffer[1] + sample_r);
			} else {
				out_buffer[0] = sample_l;
				out_buffer[1] = sample_r;
			}
		}
	}
	void downsample(INT16 *out_buffer, INT32 samples) {
#define DOWNSAMPLE_DEBUG 0
#if DOWNSAMPLE_DEBUG
		INT32 last_pos = 0;
		UINT32 last_sample = 0;
		extern int counter;
#endif
		for (INT32 i = 0; i < samples; i++, out_buffer += 2, nFractionalPosition += nSampleSize) {
			INT64 sample[MAX_CHANNELS];
			INT32 source_pos = (nFractionalPosition >> 16) - 1; // "-1" - we start at previous frame's carry-over sample.
			INT32 samples_sourced = 0; // 24.8: 0x100 whole sample, 0x0xx fractional
			INT32 left = nSampleSize; // 16.16  whole_samples.partial_sample

			// deal with first partial sample
			INT32 partial_sam_vol = ((1 << 16) - (nFractionalPosition & 0xffff));
#if DOWNSAMPLE_DEBUG
			if (counter) bprintf(0, _T("--start--\n%d  (partial: %x)\n"), source_pos, ((1 << 16) - (nFractionalPosition & 0xffff)));
			if (i==0) bprintf(0, _T("FIRST sample: %x\n"), in_buffer[0][source_pos]);
			last_pos = source_pos;
#endif
			for (INT32 ch = 0; ch < nChannels; ch++) {
				sample[ch] = in_buffer[ch][source_pos] * partial_sam_vol >> 8;
			}
			source_pos++;
			samples_sourced += partial_sam_vol >> 8;
			left -= partial_sam_vol;

			// deal with whole samples
			while (left >= (1 << 16)) {
#if DOWNSAMPLE_DEBUG
				if (counter) bprintf(0, _T("%d\n"),source_pos);
				last_pos = source_pos;
#endif
				for (INT32 ch = 0; ch < nChannels; ch++) {
					sample[ch] += in_buffer[ch][source_pos] * (1 << 8);
				}
				source_pos++;
				samples_sourced += (1 << 8);
				left -= (1 << 16);
			}

			if (source_pos < out_buffer_size) {
				// deal with last partial sample
				partial_sam_vol = (left & 0xffff) >> 8;
				for (INT32 ch = 0; ch < nChannels; ch++) {
					sample[ch] += in_buffer[ch][source_pos] * partial_sam_vol; // source_pos is not incremented here, partial sample carried over to next iter
				}
				samples_sourced += partial_sam_vol;
#if DOWNSAMPLE_DEBUG
				last_pos = source_pos;
				last_sample = in_buffer[0][source_pos];
				if (counter) {
					if (partial_sam_vol) bprintf(0, _T("%d  (last partial: %X)\n"),source_pos, left & 0xffff);
					else bprintf(0, _T("last partial not used.\n"));
					bprintf(0, _T("left %x  partial_sam_vol %x  samples_sourced %x @end\n"), left, partial_sam_vol, samples_sourced);
				}
#endif
			}
			// average
			INT32 sample_l = 0;
			INT32 sample_r = 0;

			for (INT32 ch = 0; ch < nChannels; ch++) {
				sample[ch] = sample[ch] / samples_sourced;

				if (ch & 1) sample_r += sample[ch]; else sample_l += sample[ch];
			}

			// duplicate left ch. for MONO / single streams
			if (nChannels == 1) sample_r = sample_l;

			// apply volume, check for clipping and mix-down
			sample_l = BURN_SND_CLIP(sample_l * volume);
			sample_r = BURN_SND_CLIP(sample_r * volume);

			sample_l = ((route & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) ? sample_l : 0;
			sample_r = ((route & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) ? sample_r : 0;

			if (bAddStream) {
				out_buffer[0] = BURN_SND_CLIP(out_buffer[0] + sample_l);
				out_buffer[1] = BURN_SND_CLIP(out_buffer[1] + sample_r);
			} else {
				out_buffer[0] = sample_l;
				out_buffer[1] = sample_r;
			}
		}

#if DOWNSAMPLE_DEBUG
		bprintf(0, _T("LAST sample: %x\n"), last_sample);
		bprintf(0, _T("samples: %d   last_pos: %d  nFractionalwhole.samples:  %d\n"), samples, last_pos, nFractionalPosition >> 16);//, samples_per_frame);
#endif
	}

	void upsample(INT16 *out_buffer, INT32 samples) {
		for (INT32 i = 0; i < samples; i++, out_buffer += 2, nFractionalPosition += nSampleSize) {
			INT32 source_pos = nFractionalPosition >> 16; // nFractionalPosition = 16.16

			INT32 sample_l = 0;
			INT32 sample_r = 0;

			for (INT32 ch = 0; ch < nChannels; ch++) {
				// linear-interpolate between last and current sourced-sample
				INT32 sample = in_buffer[ch][source_pos - 1] + (((in_buffer[ch][source_pos] - in_buffer[ch][source_pos - 1]) * ((nFractionalPosition >> 4) & 0x0fff)) >> 16);

				if (ch & 1) sample_r += sample; else sample_l += sample;
			}

			if (nChannels == 1) sample_r = sample_l; // duplicate left ch. for MONO

			// apply volume, check for clipping and mix-down
			sample_l = BURN_SND_CLIP(sample_l * volume);
			sample_r = BURN_SND_CLIP(sample_r * volume);

			sample_l = ((route & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) ? sample_l : 0;
			sample_r = ((route & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) ? sample_r : 0;

			if (bAddStream) {
				out_buffer[0] = BURN_SND_CLIP(out_buffer[0] + sample_l);
				out_buffer[1] = BURN_SND_CLIP(out_buffer[1] + sample_r);
			} else {
				out_buffer[0] = sample_l;
				out_buffer[1] = sample_r;
			}
		}
	}

	// -[ stream/streambuffer handling section ]-
	INT16 *soundbuf[MAX_CHANNELS];
	double volume;
	INT32 route;
	INT32 nPosition;
	INT32 buffered;

	void  (*pUpdateStream)(INT16 **, INT32 );
	INT32 (*pTotalCyclesCB)();
	INT32 nCpuMHZ;

	INT32 debug;

	void set_volume(double vol)
	{
		volume = vol;
	}

	void set_route(INT32 rt)
	{
		route = rt;
	}

	void set_buffered(INT32 (*pCPUCyclesCB)(), INT32 nCpuMhz)
	{
		buffered = 1;

		pTotalCyclesCB = pCPUCyclesCB;
		nCpuMHZ = nCpuMhz;
	}

	void stream_init(void (*update_stream)(INT16 **, INT32 )) {
		pUpdateStream = update_stream;

		for (INT32 ch = 0; ch < nChannels; ch++) {
			soundbuf[ch] = (INT16*)BurnMalloc(nSampleRateFrom * 2);
		}

		nPosition = 0;

		set_volume(1.00);
		set_route(BURN_SND_ROUTE_BOTH);
		set_debug(0);
	}

	void stream_exit() {
		for (INT32 ch = 0; ch < nChannels; ch++) {
			BurnFree(soundbuf[ch]);
		}
		buffered = 0;

		pUpdateStream = NULL;
		pTotalCyclesCB = NULL;
		nCpuMHZ = 0;
	}

	INT32 SyncInternal(INT32 cycles)
	{
		if (!buffered) return 0;
		return (INT32)((double)cycles * ((double)pTotalCyclesCB() / ((double)nCpuMHZ / (nBurnFPS / 100.0000))));
	}

	void update()
	{
		UpdateStream(0);
	}

	void set_debug(INT32 level)
	{
		debug = level;
	}

	void UpdateStream(INT32 end)
	{
		if (!pBurnSoundOut) return;
		if (end == 0 && buffered == 0) return;

		INT32 framelen = samples_to_source(nBurnSoundLen);
		INT32 position = (end) ? framelen : SyncInternal(framelen);

		if (position > framelen) position = framelen;

		INT32 samples = position - nPosition;

		if (samples < 1) return;

		if ((debug == 2) || (debug == 1 && end)) bprintf(0, _T("stream_sync: %d samples   pos %d  framelen %d   frame %d\n"), samples, nPosition, framelen, nCurrentFrame);

		INT16 *mix[MAX_CHANNELS];

		for (INT32 i = 0; i < nChannels; i++) {
			// NOTE: soundbuf[ch][0] = upsampler internal use! (see notes in upsample())
			mix[i] = soundbuf[i] + 1 + nPosition;
		}
		pUpdateStream(mix, samples);

		nPosition += samples;
	}
};
