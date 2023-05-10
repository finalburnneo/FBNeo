// (also appears in k054539.cpp, d_spectrum.cpp c/o dink)
// direct form II(transposed) biquadradic filter, needed for delay(echo) effect's filter taps -dink
enum { FILT_HIGHPASS = 0, FILT_LOWPASS = 1, FILT_LOWSHELF = 2, FILT_HIGHSHELF = 3, FILT_PEAK = 4, FILT_NOTCH = 5, FILT_BANDPASS };

struct BIQ {
	double a0;
	double a1;
	double a2;
	double b1;
	double b2;
	double q;
	double z1;
	double z2;
	double frequency;
	double samplerate;
	double output;

	float filter(float input) {
		output = input * a0 + z1;
		z1 = input * a1 + z2 - b1 * output;
		z2 = input * a2 - b2 * output;
		return (float)output;
	}

	void filter_buffer(INT16 *buffer, INT32 buflen) {
		for (INT32 i = 0; i < buflen; i++) {
			INT32 m = filter(buffer[i]);
			buffer[i] = BURN_SND_CLIP(m);
		}
	}

	void filter_buffer_mono_stereo_stream(INT16 *buffer, INT32 buflen) {
		for (INT32 i = 0; i < buflen; i++) {
			const INT32 a = i * 2 + 0;
			INT32 m = filter(buffer[a]);
			buffer[a] = BURN_SND_CLIP(m);
		}
	}

	void filter_buffer_2x_mono(INT16 *buffer, INT32 buflen) {
		for (INT32 i = 0; i < buflen; i++) {
			const INT32 a = i * 2 + 0;
			INT32 m = filter(buffer[a]);
			buffer[a] = BURN_SND_CLIP(m);
			buffer[a+1] = buffer[a];
		}
	}

	void reset() {
		z1 = 0.0; z2 = 0.0; output = 0.0;
	}

	void exit() {
		reset();
	}

	void init(INT32 type, INT32 sample_rate, INT32 freqhz, double q_, double gain) {
		samplerate = sample_rate;
		frequency = freqhz;
		q = q_;

		reset();

		double k = tan(3.14159265358979323846 * frequency / samplerate);
		double norm = 1 / (1 + k / q + k * k);
		double v = pow(10, fabs(gain) / 20);

		switch (type) {
			case FILT_HIGHPASS:
				{
					a0 = 1 * norm;
					a1 = -2 * a0;
					a2 = a0;
					b1 = 2 * (k * k - 1) * norm;
					b2 = (1 - k / q + k * k) * norm;
				}
				break;
			case FILT_LOWPASS:
				{
					a0 = k * k * norm;
					a1 = 2 * a0;
					a2 = a0;
					b1 = 2 * (k * k - 1) * norm;
					b2 = (1 - k / q + k * k) * norm;
				}
				break;
			case FILT_BANDPASS:
				{
					norm = 1 / (1 + k / q + k * k);
					a0 = k / q * norm;
					a1 = 0;
					a2 = -a0;
					b1 = 2 * (k * k - 1) * norm;
					b2 = (1 - k / q + k * k) * norm;
				}
				break;
			case FILT_LOWSHELF:
				{
					if (gain >= 0) {
						norm = 1 / (1 + sqrt(2.0) * k + k * k);
						a0 = (1 + sqrt(2*v) * k + v * k * k) * norm;
						a1 = 2 * (v * k * k - 1) * norm;
						a2 = (1 - sqrt(2*v) * k + v * k * k) * norm;
						b1 = 2 * (k * k - 1) * norm;
						b2 = (1 - sqrt(2.0) * k + k * k) * norm;
					} else {
						norm = 1 / (1 + sqrt(2*v) * k + v * k * k);
						a0 = (1 + sqrt(2.0) * k + k * k) * norm;
						a1 = 2 * (k * k - 1) * norm;
						a2 = (1 - sqrt(2.0) * k + k * k) * norm;
						b1 = 2 * (v * k * k - 1) * norm;
						b2 = (1 - sqrt(2*v) * k + v * k * k) * norm;
					}
				}
				break;
			case FILT_HIGHSHELF:
				{
					if (gain >= 0) {
						norm = 1 / (1 + sqrt(2.0) * k + k * k);
						a0 = (v + sqrt(2*v) * k + k * k) * norm;
						a1 = 2 * (k * k - v) * norm;
						a2 = (v - sqrt(2*v) * k + k * k) * norm;
						b1 = 2 * (k * k - 1) * norm;
						b2 = (1 - sqrt(2.0) * k + k * k) * norm;
					} else {
						norm = 1 / (v + sqrt(2*v) * k + k * k);
						a0 = (1 + sqrt(2.0) * k + k * k) * norm;
						a1 = 2 * (k * k - 1) * norm;
						a2 = (1 - sqrt(2.0) * k + k * k) * norm;
						b1 = 2 * (k * k - v) * norm;
						b2 = (v - sqrt(2*v) * k + k * k) * norm;
					}
				}
				break;
			case FILT_PEAK:
				{
					if (gain >= 0) {
						norm = 1 / (1 + 1 / q * k + k * k);
						a0 = (1 + v / q * k + k * k) * norm;
						a1 = 2 * (k * k - 1) * norm;
						a2 = (1 - v / q * k + k * k) * norm;
						b1 = a1;
						b2 = (1 - 1 / q * k + k * k) * norm;
					} else {
						norm = 1 / (1 + v / q * k + k * k);
						a0 = (1 + 1 / q * k + k * k) * norm;
						a1 = 2 * (k * k - 1) * norm;
						a2 = (1 - 1 / q * k + k * k) * norm;
						b1 = a1;
						b2 = (1 - v / q * k + k * k) * norm;
					}
				}
				break;
			case FILT_NOTCH:
				{
					norm = 1 / (1 + k / q + k * k);
					a0 = (1 + k * k) * norm;
					a1 = 2 * (k * k - 1) * norm;
					a2 = a0;
					b1 = a1;
					b2 = (1 - k / q + k * k) * norm;
				}
				break;
		}
	}
};
// end biquad filter

struct BIQSTEREO {
	BIQ L;
	BIQ R;

	void init(INT32 type, INT32 sample_rate, INT32 freqhz, double q_, double gain) {
		L.init(type, sample_rate, freqhz, q_, gain);
		R.init(type, sample_rate, freqhz, q_, gain);
	}

	void reset() {
		L.reset();
		R.reset();
	}

	void exit() {
		L.exit();
		R.exit();
	}

	void filter_buffer(INT16 *buffer, INT32 buflen) {
		for (INT32 i = 0; i < buflen; i++) {
			INT32 idx = i * 2;
			INT32 l = L.filter(buffer[idx + 0]);
			INT32 r = R.filter(buffer[idx + 1]);
			buffer[idx + 0] = BURN_SND_CLIP(l);
			buffer[idx + 1] = BURN_SND_CLIP(r);
		}
	}
};
