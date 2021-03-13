#define DOWNSAMPLE_DEBUG 0
// re-down-sampler, dink 2020
struct Downsampler {
	UINT32 nSampleSize;
	UINT32 nSampleSize_Otherway;
	UINT32 nFractionalPosition;
	bool bAddStream;

	void init(INT32 rate_from, INT32 rate_to, bool add_to_stream) {
		nSampleSize = (UINT64)rate_from * (1 << 16) / ((rate_to == 0) ? 44100 : rate_to);
		nSampleSize_Otherway = (UINT64)((rate_to == 0) ? 44100 : rate_to) * (1 << 16) / rate_from;
		nFractionalPosition = 0;
		bAddStream = add_to_stream;
	}
	void exit() {
		nSampleSize = nFractionalPosition = 0;
	}
	INT32 samples_to_host(INT32 samples) {
		return (INT32)((UINT64)nSampleSize_Otherway * samples) / (1 << 16);
	}
	INT32 samples_to_source(INT32 samples) {
		return (INT32)((UINT64)nSampleSize * samples) / (1 << 16);
	}
	void resample(INT16 *in_buffer, INT16 *out_buffer, INT32 samples, double volume, INT32 route) {
		INT32 last_pos = 0;
#if DOWNSAMPLE_DEBUG
		extern int counter;
#endif
		for (INT32 i = 0; i < samples; i++) {
			INT32 sample = 0;
			INT32 source_pos = nFractionalPosition >> 16; // nFractionalPosition = 16.16
			INT32 samples_sourced = 0; // 24.8: 0x100 whole sample, 0x0xx fractional
			INT32 left = nSampleSize; // 16.16  whole_samples.partial_sample

			// deal with first partial sample
			INT32 partial_sam_vol = ((1 << 16) - (nFractionalPosition & 0xffff));
#if DOWNSAMPLE_DEBUG
			if (counter) bprintf(0, _T("--start--\n%d  (partial: %x)\n"), source_pos, ((1 << 16) - (nFractionalPosition & 0xffff)));
#endif
			sample += in_buffer[source_pos++] * partial_sam_vol >> 8;
			samples_sourced += partial_sam_vol >> 8;
			left -= partial_sam_vol;

			// deal with whole samples
			while (left >= (1 << 16)) {
#if DOWNSAMPLE_DEBUG
				if (counter) bprintf(0, _T("%d\n"),source_pos);
#endif
				sample += in_buffer[source_pos++] * (1 << 8);
				samples_sourced += (1 << 8);
				left -= (1 << 16);
			}

			// deal with last partial sample
			partial_sam_vol = (left & 0xffff) >> 8;
			sample += in_buffer[source_pos] * partial_sam_vol; // source_pos is not incremented here, partial sample carried over to next iter
			samples_sourced += partial_sam_vol;
			last_pos = source_pos;
#if DOWNSAMPLE_DEBUG
			if (counter) {
				if (partial_sam_vol) bprintf(0, _T("%d  (last partial: %X)\n"),source_pos, left & 0xffff);
				else bprintf(0, _T("last partial not used.\n"));
				bprintf(0, _T("left %x  partial_sam_vol %x  samples_sourced %x @end\n"), left, partial_sam_vol, samples_sourced);
			}
#endif
			// average + mixdown
			sample = sample / samples_sourced;

			INT32 sample_l = 0;
			INT32 sample_r = 0;

			if ((route & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				sample_l = (INT32)(sample * volume);
			}
			if ((route & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				sample_r = (INT32)(sample * volume);
			}

			sample_l = BURN_SND_CLIP(sample_l);
			sample_r = BURN_SND_CLIP(sample_r);

			if (bAddStream) {
				out_buffer[0] = BURN_SND_CLIP(out_buffer[0] + sample_l);
				out_buffer[1] = BURN_SND_CLIP(out_buffer[1] + sample_r);
			} else {
				out_buffer[0] = sample_l;
				out_buffer[1] = sample_r;
			}
			out_buffer += 2;

			nFractionalPosition += nSampleSize;
		}

#if DOWNSAMPLE_DEBUG
		bprintf(0, _T("samples: %d   last_pos: %d   samps_frame %d\n"), samples, last_pos, samples_per_frame);
#endif
		nFractionalPosition &= 0xffff;
	}

	void resample_mono(INT16 *in_buffer, INT16 *out_buffer, INT32 samples, double volume) {
		INT32 last_pos = 0;
#if DOWNSAMPLE_DEBUG
		extern int counter;
#endif
		for (INT32 i = 0; i < samples; i++) {
			INT32 sample = 0;
			INT32 source_pos = nFractionalPosition >> 16; // nFractionalPosition = 16.16
			INT32 samples_sourced = 0; // 24.8: 0x100 whole sample, 0x0xx fractional
			INT32 left = nSampleSize; // 16.16  whole_samples.partial_sample

			// deal with first partial sample
			INT32 partial_sam_vol = ((1 << 16) - (nFractionalPosition & 0xffff));
#if DOWNSAMPLE_DEBUG
			if (counter) bprintf(0, _T("--start--\n%d  (partial: %x)\n"), source_pos, ((1 << 16) - (nFractionalPosition & 0xffff)));
#endif
			sample += in_buffer[source_pos++] * partial_sam_vol >> 8;
			samples_sourced += partial_sam_vol >> 8;
			left -= partial_sam_vol;

			// deal with whole samples
			while (left >= (1 << 16)) {
#if DOWNSAMPLE_DEBUG
				if (counter) bprintf(0, _T("%d\n"),source_pos);
#endif
				sample += in_buffer[source_pos++] * (1 << 8);
				samples_sourced += (1 << 8);
				left -= (1 << 16);
			}

			// deal with last partial sample
			partial_sam_vol = (left & 0xffff) >> 8;
			sample += in_buffer[source_pos] * partial_sam_vol; // source_pos is not incremented here, partial sample carried over to next iter
			samples_sourced += partial_sam_vol;
			last_pos = source_pos;
#if DOWNSAMPLE_DEBUG
			if (counter) {
				if (partial_sam_vol) bprintf(0, _T("%d  (last partial: %X)\n"),source_pos, left & 0xffff);
				else bprintf(0, _T("last partial not used.\n"));
				bprintf(0, _T("left %x  partial_sam_vol %x  samples_sourced %x @end\n"), left, partial_sam_vol, samples_sourced);
			}
#endif
			// average + mixdown
			sample = sample / samples_sourced;

			// amplitude modulate
			INT32 sample_l = (INT32)(sample * volume);
			sample_l = BURN_SND_CLIP(sample_l);

			out_buffer[0] = sample_l;
			out_buffer ++;

			nFractionalPosition += nSampleSize;
		}

#if DOWNSAMPLE_DEBUG
		bprintf(0, _T("samples: %d   last_pos: %d   samps_frame %d\n"), samples, last_pos, samples_per_frame);
#endif
		nFractionalPosition &= 0xffff;
	}
};
