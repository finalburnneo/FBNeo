// re-up-sampler, dink 2021
struct Upsampler {
	UINT32 nSampleSize;
	UINT32 nSampleSize_Otherway;
	UINT32 nSampleRateFrom;
	UINT32 nSampleRateTo;
	INT32 nFractionalPosition; // 16.16 whole.partial samples
	bool bAddStream;

	void init(INT32 rate_from, INT32 rate_to, bool add_to_stream) {
		nFractionalPosition = 0;
		bAddStream = add_to_stream;
		nSampleRateFrom = rate_from;
		nSampleRateTo = rate_to;
		set_rate(rate_from);
	}
	void set_rate(INT32 rate_from) {
		nSampleRateFrom = rate_from;
		nSampleSize = (UINT64)nSampleRateFrom * (1 << 16) / ((nSampleRateTo == 0) ? 44100 : nSampleRateTo);
		nSampleSize_Otherway = (UINT64)((nSampleRateTo == 0) ? 44100 : nSampleRateTo) * (1 << 16) / nSampleRateFrom;
	}
	void exit() {
		nSampleSize = nFractionalPosition = 0;
	}
	INT32 samples_to_host(INT32 samples) {
		return (INT32)((UINT64)nSampleSize_Otherway * samples) / (1 << 16);
	}
	INT32 samples_to_source(INT32 samples) {
		return (INT32)(((UINT64)nSampleSize * samples) / (1 << 16)) + 1;
	}
	void resample(INT16 *in_buffer, INT16 *out_buffer, INT32 samples, INT32 &extra_samples, double volume, INT32 route) {
		INT32 out_buffer_size = samples_to_source(samples);

		// make room for previous frame "carry-over" sample aka in_buffer[-1]
		in_buffer++;

		for (INT32 i = 0; i < samples; i++, out_buffer += 2, nFractionalPosition += nSampleSize) {
			INT32 source_pos = nFractionalPosition >> 16; // nFractionalPosition = 16.16

			// linear-interpolate between last and current sourced-sample
			INT32 sample = in_buffer[source_pos - 1] + (((in_buffer[source_pos] - in_buffer[source_pos - 1]) * ((nFractionalPosition >> 4) & 0x0fff)) >> 16);

			// check for clipping and mix-down
			sample = BURN_SND_CLIP(sample);

			INT32 sample_l = ((route & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) ? sample : 0;
			INT32 sample_r = ((route & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) ? sample : 0;

			if (bAddStream) {
				out_buffer[0] = BURN_SND_CLIP(out_buffer[0] + sample_l);
				out_buffer[1] = BURN_SND_CLIP(out_buffer[1] + sample_r);
			} else {
				out_buffer[0] = sample_l;
				out_buffer[1] = sample_r;
			}
		}

		// figure out if we have extra whole samples
		extra_samples = out_buffer_size - (nFractionalPosition >> 16);

		// 1) take the last sample processed and store it into in_buffer[-1]
		// so that interpolation can be carried out between frames.
		// 2) store the extra whole samples at in_buffer[0+], returning the
		// number of whole samples in &extra_samples.
		for (INT32 i = -1; i < extra_samples; i++) {
			in_buffer[i] = in_buffer[i + (nFractionalPosition >> 16)];
		}

		// mask off the whole samples from our accumulator
		nFractionalPosition &= 0xffff;
	}
};
