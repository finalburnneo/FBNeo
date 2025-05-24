// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    votrax.c

    Votrax SC01A simulation

***************************************************************************/

/*
  tp3 stb i1 i2 tp2
  1   1   o  o  white noise
  1   0   -  1  phone timing clock
  1   0   1  0  closure tick
  1   0   0  0  sram write pulse
  0   -   -  -  sram write pulse

i1.o = glottal impulse
i2.o = white noise

tp1 = phi clock (tied to f2q rom access)
*/

#include "burnint.h"
#include "bitswap.h"
#include "stream.h"
#include "votrax.h"
#include "math.h"
#include "dtimer.h"

#define logerror
#define LOGMASKED

#define ASSERT_LINE 1
#define CLEAR_LINE 0

#define LOG_PHONE  (1U << 1)
#define LOG_COMMIT (1U << 2)
#define LOG_INT    (1U << 3)
#define LOG_TICK   (1U << 4)
#define LOG_FILTER (1U << 5)

// fbneo port notes:   -dink (march 2024)
//
// I did my best to port this with as little changes as possible
// using fbneo's awesome dtimer and Stream.  Also, changed a few things
// to support the c++98 standard, so all systems old 'n new can benefit.
//
// This is quite literally the greatest thing to come from MAME lately,
// thanks Olivier! :)

static Stream stream; // dink's stream
static dtimer d_timer; // dink's timer, keeps track of things because he's really bad at time.

//#define VERBOSE (LOG_GENERAL | LOG_PHONE)
//#include "logmacro.h"
// Possible timer parameters
enum {
	T_COMMIT_PHONE,
	T_END_OF_PHONE
};

// static const char *const s_phone_table[64];
// static const double s_glottal_wave[9];

//	sound_stream *m_stream;                         // Output stream
//	emu_timer *m_timer;                             // General timer
	//	required_memory_region m_rom;                   // Internal ROM
static UINT64 *m_rom;
static UINT32 m_mainclock;                                // Current main clock
static double m_sclock;                                // Stream sample clock (40KHz, main/18)
static double m_cclock;                                // 20KHz capacitor switching clock (main/36)
static UINT32 m_sample_count;                             // Sample counter, to cadence chip updates

// Inputs
static UINT8 m_inflection;                                // 2-bit inflection value
static UINT8 m_phone;                                     // 6-bit phone value

// Outputs
static void (*m_ar_cb)(INT32);                       // Callback for ar
static bool m_ar_state;                                // Current ar state

// "Unpacked" current rom values
static UINT8 m_rom_duration;                              // Duration in 5KHz units (main/144) of one tick, 16 ticks per phone, 7 bits
static UINT8 m_rom_vd, m_rom_cld;                         // Duration in ticks of the "voice" and "closure" delays, 4 bits
static UINT8 m_rom_fa, m_rom_fc, m_rom_va;                // Analog parameters, noise volume, noise freq cutoff and voice volume, 4 bits each
static UINT8 m_rom_f1, m_rom_f2, m_rom_f2q, m_rom_f3;     // Analog parameters, formant frequencies and Q, 4 bits each
static bool m_rom_closure;                             // Closure bit, true = silence at cld
static bool m_rom_pause;                               // Pause bit

// Current interpolated values (8 bits each)
static UINT8 m_cur_fa, m_cur_fc, m_cur_va;
static UINT8 m_cur_f1, m_cur_f2, m_cur_f2q, m_cur_f3;

// Current committed values
static UINT8 m_filt_fa, m_filt_fc, m_filt_va;             // Analog parameters, noise volume, noise freq cutoff and voice volume, 4 bits each
static UINT8 m_filt_f1, m_filt_f2, m_filt_f2q, m_filt_f3; // Analog parameters, formant frequencies/Q on 4 bits except f2 on 5 bits

// Internal counters
static UINT16 m_phonetick;                                // 9-bits phone tick duration counter
static UINT8  m_ticks;                                    // 5-bits tick counter
static UINT8  m_pitch;                                    // 7-bits pitch counter
static UINT8  m_closure;                                  // 5-bits glottal closure counter
static UINT8  m_update_counter;                           // 6-bits counter for the 625Hz (main/1152) and 208Hz (main/3456) update timing generators

// Internal state
static bool m_cur_closure;                             // Current internal closure state
static UINT16 m_noise;                                    // 15-bit noise shift register
static bool m_cur_noise;                               // Current noise output

// Filter coefficients and level histories
static double m_voice_1[4];
static double m_voice_2[4];
static double m_voice_3[4];

static double m_noise_1[3];
static double m_noise_2[3];
static double m_noise_3[2];
static double m_noise_4[2];

static double m_vn_1[4];
static double m_vn_2[4];
static double m_vn_3[4];
static double m_vn_4[4];
static double m_vn_5[2];
static double m_vn_6[2];

static double m_f1_a[4],  m_f1_b[4];                   // F1 filtering
static double m_f2v_a[4], m_f2v_b[4];                  // F2 voice filtering
static double m_f2n_a[2], m_f2n_b[2];                  // F2 noise filtering
static double m_f3_a[4],  m_f3_b[4];                   // F3 filtering
static double m_f4_a[4],  m_f4_b[4];                   // F4 filtering
static double m_fx_a[1],  m_fx_b[2];                   // Final filtering
static double m_fn_a[3],  m_fn_b[3];                   // Noise shaping

// Compute a total capacitor value based on which bits are currently active

#if 1
// c version
static double bits_to_caps(UINT32 value, int cnt, double *caps_values) {
	double total = 0;
	for(int i = 0; i < cnt; i++) {
		double d = caps_values[i];
		if(value & 1)
			total += d;
		value >>= 1;
	}
	return total;
}
#endif
#if 0
// c++11+ version
	static double bits_to_caps(UINT32 value, std::initializer_list<double> caps_values) {
		double total = 0;
		for(double d : caps_values) {
			if(value & 1)
				total += d;
			value >>= 1;
		}
		return total;
	}
#endif

// Shift a history of values by one and insert the new value at the front
template<UINT32 N> static void shift_hist(double val, double (&hist_array)[N]) {
	for(UINT32 i=N-1; i>0; i--)
		hist_array[i] = hist_array[i-1];
	hist_array[0] = val;
}

// Apply a filter and compute the result. 'a' is applied to x (inputs) and 'b' to y (outputs)
template<UINT32 Nx, UINT32 Ny, UINT32 Na, UINT32 Nb> static double apply_filter(const double (&x)[Nx], const double (&y)[Ny], const double (&a)[Na], const double (&b)[Nb]) {
	double total = 0;
	for(UINT32 i=0; i<Na; i++)
		total += x[i] * a[i];
	for(UINT32 i=1; i<Nb; i++)
		total -= y[i-1] * b[i];
	return total / b[0];
}

static void build_standard_filter(double *a, double *b,
							   double c1t, // Unswitched cap, input, top
							   double c1b, // Switched cap, input, bottom
							   double c2t, // Unswitched cap, over first amp-op, top
							   double c2b, // Switched cap, over first amp-op, bottom
							   double c3,  // Cap between the two op-amps
							   double c4); // Cap over second op-amp

static void build_noise_shaper_filter(double *a, double *b,
								   double c1,  // Cap over first amp-op
								   double c2t, // Unswitched cap between amp-ops, input, top
								   double c2b, // Switched cap between amp-ops, input, bottom
								   double c3,  // Cap over second amp-op
								   double c4); // Switched cap after second amp-op

static void build_lowpass_filter(double *a, double *b,
							  double c1t,  // Unswitched cap, over amp-op, top
							  double c1b); // Switched cap, over amp-op, bottom

static void build_injection_filter(double *a, double *b,
								double c1b, // Switched cap, input, bottom
								double c2t, // Unswitched cap, over first amp-op, top
								double c2b, // Switched cap, over first amp-op, bottom
								double c3,  // Cap between the two op-amps
								double c4); // Cap over second op-amp


static void interpolate(UINT8 &reg, UINT8 target);    // Do one interpolation step
static void chip_update();                             // Global update called at 20KHz (main/36)
static void filters_commit(bool force);                // Commit the currently computed interpolation values to the filters
static void phone_commit();                            // Commit the current phone id
static INT16 analog_calc();                  // Compute one more sample
static void phone_tick(int param); // timer cb


#if 0
DEFINE_DEVICE_TYPE(VOTRAX_SC01, votrax_sc01_device, "votrsc01", "Votrax SC-01")
DEFINE_DEVICE_TYPE(VOTRAX_SC01A, votrax_sc01a_device, "votrsc01a", "Votrax SC-01-A")

// ROM definition for the Votrax phone ROM
ROM_START( votrax_sc01 )
	ROM_REGION64_LE( 0x200, "internal", 0 )
	ROM_LOAD( "sc01.bin", 0x000, 0x200, CRC(528d1c57) SHA1(268b5884dce04e49e2376df3e2dc82e852b708c1) )
ROM_END

ROM_START( votrax_sc01a )
	ROM_REGION64_LE( 0x200, "internal", 0 )
	ROM_LOAD( "sc01a.bin", 0x000, 0x200, CRC(fc416227) SHA1(1d6da90b1807a01b5e186ef08476119a862b5e6d) )
ROM_END
#endif

// todo: load these externally
static UINT8 sc01_bin[] = {
	0xa4,0x50,0xa0,0xf0,0xe0,0x00,0x00,0x03,0xa4,0x50,0xa0,0x00,0x23,0x0a,0x00,0x3e,
	0xa4,0x58,0xa0,0x30,0xf0,0x00,0x00,0x3f,0xa3,0x81,0x69,0xb0,0xc1,0x0c,0x00,0x3d,
	0x26,0xd3,0x49,0x90,0xa1,0x09,0x00,0x3c,0x27,0x81,0x68,0x94,0x21,0x0a,0x00,0x3b,
	0x82,0xc3,0x48,0x24,0xa1,0x08,0x00,0x3a,0xa4,0x00,0x38,0x18,0x68,0x01,0x00,0x39,
	0x20,0x52,0xe1,0x88,0x63,0x0a,0x00,0x38,0x22,0xc1,0xe8,0x90,0x61,0x04,0x00,0x37,
	0xa2,0x83,0x60,0x10,0x66,0x03,0x00,0x36,0xa2,0xc1,0xe8,0x80,0xa1,0x09,0x00,0x35,
	0xa2,0xc1,0xe8,0x34,0x61,0x0a,0x00,0x34,0xa3,0x81,0xc9,0xb4,0x21,0x0a,0x00,0x33,
	0xa3,0x81,0xc9,0xe4,0xa1,0x07,0x00,0x32,0xa3,0x81,0xc9,0x54,0x63,0x01,0x00,0x31,
	0xa3,0x81,0x69,0x60,0x61,0x04,0x00,0x30,0xa7,0x81,0xe8,0x74,0xa0,0x07,0x00,0x2f,
	0xa7,0x81,0xe8,0x74,0x20,0x0a,0x00,0x2e,0x22,0xc1,0x60,0x14,0x66,0x0a,0x00,0x2d,
	0x26,0xd3,0x49,0x70,0x20,0x0a,0x00,0x2c,0x82,0x43,0x08,0x54,0x63,0x04,0x00,0x2b,
	0xe0,0x32,0x11,0xe8,0x72,0x01,0x00,0x2a,0x26,0x53,0x01,0x64,0xa1,0x07,0x00,0x29,
	0x22,0xc1,0xe8,0x80,0x21,0x0a,0x00,0x28,0xa6,0x91,0x61,0x80,0x21,0x0a,0x00,0x27,
	0xa2,0xc1,0xe8,0x84,0x21,0x0a,0x00,0x26,0xa8,0x24,0x13,0x63,0xb2,0x07,0x00,0x25,
	0xa3,0xc1,0xe9,0x84,0xc1,0x0c,0x00,0x24,0xa3,0x81,0xc9,0x54,0xe3,0x00,0x00,0x23,
	0x26,0x12,0xa0,0x64,0x61,0x0a,0x00,0x22,0x26,0xd3,0x69,0x70,0x61,0x05,0x00,0x21,
	0xa6,0xc1,0xc9,0x84,0x21,0x0a,0x00,0x20,0xe0,0x32,0x91,0x48,0x68,0x04,0x00,0x1f,
	0x26,0x91,0xe8,0x00,0x7c,0x0b,0x00,0x1e,0xa8,0x2c,0x83,0x65,0xa2,0x07,0x00,0x1d,
	0x26,0xc1,0x41,0xe0,0x73,0x01,0x00,0x1c,0xac,0x04,0x22,0xfd,0x62,0x01,0x00,0x1b,
	0x2c,0x34,0x7b,0xdb,0xe8,0x00,0x00,0x1a,0x2c,0x64,0x23,0x11,0x72,0x0a,0x00,0x19,
	0xa2,0xd0,0x09,0xf4,0xa1,0x07,0x00,0x18,0x23,0x81,0x49,0x20,0x21,0x0a,0x00,0x17,
	0x23,0x81,0x49,0x30,0xa1,0x07,0x00,0x16,0xa3,0xc1,0xe9,0x84,0xa1,0x08,0x00,0x15,
	0x36,0x4b,0x08,0xd4,0xa0,0x09,0x00,0x14,0xa3,0x81,0x69,0x70,0xa0,0x08,0x00,0x13,
	0x60,0x58,0xd1,0x9c,0x63,0x01,0x00,0x12,0x6c,0x54,0x8b,0xfb,0xa2,0x09,0x00,0x11,
	0x6c,0x54,0x8b,0xfb,0x63,0x01,0x00,0x10,0x28,0x64,0xd3,0xf7,0x63,0x01,0x00,0x0f,
	0x22,0x91,0xe1,0x90,0x73,0x01,0x00,0x0e,0x36,0x19,0x24,0xe6,0x61,0x0a,0x00,0x0d,
	0x32,0x88,0xa5,0x66,0xa3,0x07,0x00,0x0c,0xa6,0x91,0x61,0x90,0xa1,0x09,0x00,0x0b,
	0xa6,0x91,0x61,0x90,0x61,0x0a,0x00,0x0a,0xa6,0x91,0x61,0x80,0x61,0x0b,0x00,0x09,
	0xa3,0xc1,0xe9,0xc4,0x61,0x01,0x00,0x08,0x6c,0x54,0xcb,0xf3,0x63,0x04,0x00,0x07,
	0xa6,0xc1,0xc9,0x34,0xa1,0x07,0x00,0x06,0xa6,0xc1,0xc9,0x64,0x61,0x01,0x00,0x05,
	0xe8,0x16,0x03,0x61,0xfb,0x00,0x00,0x04,0x27,0x81,0x68,0xc4,0xa1,0x09,0x00,0x02,
	0x27,0x81,0x68,0xd4,0x61,0x01,0x00,0x01,0x27,0x81,0x68,0x74,0x61,0x03,0x00,0x00,
};

static UINT8 sc01a_bin[] = {
	0xa4,0x50,0xa0,0xf0,0xe0,0x00,0x00,0x03,0xa4,0x50,0xa0,0x00,0x23,0x0a,0x00,0x3e,
	0xa4,0x58,0xa0,0x30,0xf0,0x00,0x00,0x3f,0xa3,0x80,0x69,0xb0,0xc1,0x0c,0x00,0x3d,
	0x26,0xd3,0x49,0x90,0xa1,0x09,0x00,0x3c,0x27,0x81,0x68,0x94,0x21,0x0a,0x00,0x3b,
	0x82,0xc3,0x48,0x24,0xa1,0x08,0x00,0x3a,0xa4,0x00,0x38,0x18,0x68,0x01,0x00,0x39,
	0x20,0x52,0xe1,0x88,0x63,0x0a,0x00,0x38,0x22,0xc1,0xe8,0x90,0x61,0x04,0x00,0x37,
	0xa2,0x83,0x60,0x10,0x66,0x03,0x00,0x36,0xa2,0xc1,0xe8,0x80,0xa1,0x09,0x00,0x35,
	0xa2,0xc1,0xe8,0x34,0x61,0x0a,0x00,0x34,0xa3,0x81,0x89,0xb4,0x21,0x0a,0x00,0x33,
	0xa3,0x81,0x89,0xe4,0xa1,0x07,0x00,0x32,0xa3,0x81,0x89,0x54,0x63,0x01,0x00,0x31,
	0xa3,0x80,0x69,0x60,0x61,0x04,0x00,0x30,0xa7,0x80,0xe8,0x74,0xa0,0x07,0x00,0x2f,
	0xa7,0x80,0xe8,0x74,0x20,0x0a,0x00,0x2e,0x22,0xc1,0x60,0x14,0x66,0x0a,0x00,0x2d,
	0x26,0xd3,0x49,0x70,0x20,0x0a,0x00,0x2c,0x82,0x43,0x08,0x54,0x63,0x04,0x00,0x2b,
	0xe0,0x32,0x11,0xe8,0x72,0x01,0x00,0x2a,0x26,0x53,0x01,0x64,0xa1,0x07,0x00,0x29,
	0x22,0xc1,0xe8,0x80,0x21,0x0a,0x00,0x28,0xa6,0x91,0x61,0x80,0x21,0x0a,0x00,0x27,
	0xa2,0xc1,0xe8,0x84,0x21,0x0a,0x00,0x26,0xa8,0x24,0x13,0x63,0xb2,0x07,0x00,0x25,
	0xa3,0x40,0xe9,0x84,0xc1,0x0c,0x00,0x24,0xa3,0x81,0x89,0x54,0xe3,0x00,0x00,0x23,
	0x26,0x12,0xa0,0x64,0x61,0x0a,0x00,0x22,0x26,0xd3,0x69,0x70,0x61,0x05,0x00,0x21,
	0xa6,0xc1,0xc9,0x84,0x21,0x0a,0x00,0x20,0xe0,0x32,0x91,0x48,0x68,0x04,0x00,0x1f,
	0x26,0x91,0xe8,0x00,0x7c,0x0b,0x00,0x1e,0xa8,0x2c,0x83,0x65,0xa2,0x07,0x00,0x1d,
	0x26,0xc1,0x41,0xe0,0x73,0x01,0x00,0x1c,0xac,0x04,0x22,0xfd,0x62,0x01,0x00,0x1b,
	0x2c,0x34,0x7b,0xdb,0xe8,0x00,0x00,0x1a,0x2c,0x64,0x23,0x11,0x72,0x0a,0x00,0x19,
	0xa2,0xd0,0x09,0xf4,0xa1,0x07,0x00,0x18,0x23,0x81,0x49,0x20,0x21,0x0a,0x00,0x17,
	0x23,0x81,0x49,0x30,0xa1,0x07,0x00,0x16,0xa3,0x40,0xe9,0x84,0xa1,0x08,0x00,0x15,
	0x36,0x4b,0x08,0xd4,0xa0,0x09,0x00,0x14,0xa3,0x80,0x69,0x70,0xa0,0x08,0x00,0x13,
	0x60,0x58,0xd1,0x9c,0x63,0x01,0x00,0x12,0x6c,0x54,0x8b,0xfb,0xa2,0x09,0x00,0x11,
	0x6c,0x54,0x8b,0xfb,0x63,0x01,0x00,0x10,0x28,0x64,0xd3,0xf7,0x63,0x01,0x00,0x0f,
	0x22,0x91,0xe1,0x90,0x73,0x01,0x00,0x0e,0x36,0x19,0x24,0xe6,0x61,0x0a,0x00,0x0d,
	0x32,0x88,0xa5,0x66,0xa3,0x07,0x00,0x0c,0xa6,0x91,0x61,0x90,0xa1,0x09,0x00,0x0b,
	0xa6,0x91,0x61,0x90,0x61,0x0a,0x00,0x0a,0xa6,0x91,0x61,0x80,0x61,0x0b,0x00,0x09,
	0xa3,0x40,0xe9,0xc4,0x61,0x01,0x00,0x08,0x6c,0x54,0xcb,0xf3,0x63,0x04,0x00,0x07,
	0xa6,0xc1,0xc9,0x34,0xa1,0x07,0x00,0x06,0xa6,0xc1,0xc9,0x64,0x61,0x01,0x00,0x05,
	0xe8,0x16,0x03,0x61,0xfb,0x00,0x00,0x04,0x27,0x81,0x68,0xc4,0xa1,0x09,0x00,0x02,
	0x27,0x81,0x68,0xd4,0x61,0x01,0x00,0x01,0x27,0x81,0x68,0x74,0x61,0x03,0x00,0x00,
};


// textual phone names for debugging
const char *const s_phone_table[64] =
{
	"EH3",  "EH2",  "EH1",  "PA0",  "DT",   "A1",   "A2",   "ZH",
	"AH2",  "I3",   "I2",   "I1",   "M",    "N",    "B",    "V",
	"CH",   "SH",   "Z",    "AW1",  "NG",   "AH1",  "OO1",  "OO",
	"L",    "K",    "J",    "H",    "G",    "F",    "D",    "S",
	"A",    "AY",   "Y1",   "UH3",  "AH",   "P",    "O",    "I",
	"U",    "Y",    "T",    "R",    "E",    "W",    "AE",   "AE1",
	"AW2",  "UH2",  "UH1",  "UH",   "O2",   "O1",   "IU",   "U1",
	"THV",  "TH",   "ER",   "EH",   "E1",   "AW",   "PA1",  "STOP"
};

// This waveform is built using a series of transistors as a resistor
// ladder.  There is first a transistor to ground, then a series of
// seven transistors one quarter the size of the first one, then it
// finishes by an active resistor to +9V.
//
// The terminal of the transistor to ground is used as a middle value.
// Index 0 is at that value. Index 1 is at 0V.  Index 2 to 8 start at
// just after the resistor down the latter.  Indices 9+ are the middle
// value again.
//
// For simplicity, we rescale the values to get the middle at 0 and
// the top at 1.  The final wave is very similar to the patent
// drawing.

const double s_glottal_wave[9] =
{
	0,
	-4/7.0,
	7/7.0,
	6/7.0,
	5/7.0,
	4/7.0,
	3/7.0,
	2/7.0,
	1/7.0
};
#if 0
votrax_sc01_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: votrax_sc01_device(mconfig, VOTRAX_SC01, tag, owner, clock)
{
}

// overridable type for subclass
votrax_sc01_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock),
	  device_sound_interface(mconfig, *this),
	  m_stream(nullptr),
	  m_rom(*this, "internal"),
	  m_ar_cb(*this)
{
}

votrax_sc01a_device::votrax_sc01a_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: votrax_sc01_device(mconfig, VOTRAX_SC01A, tag, owner, clock)
{
}
#endif

static void ar_dummy_cb(INT32) { };

INT32 sc01_read_request()
{
	stream.update();

	return m_ar_state;
}

void sc01_write(UINT8 data)
{
	// flush out anything currently processing
	stream.update();

	//UINT8 prev = m_phone;

	// only 6 bits matter
	m_phone = data & 0x3f;

//	if(m_phone != prev || m_phone != 0x3f)
//		LOGMASKED(LOG_PHONE, "phone %02x.%d %s\n", m_phone, m_inflection, s_phone_table[m_phone]);

	m_ar_state = CLEAR_LINE;
	m_ar_cb(m_ar_state);

	// Schedule a commit/ar reset at roughly 0.1ms in the future (one
	// phi1 transition followed by the rom extra state in practice),
	// but only if there isn't already one on the fly.  It will
	// override an end-of-phone timeout if there's one pending, but
	// that's not a problem since stb does that anyway.
	//if(m_timer->expire().is_never() || m_timer->param() != T_COMMIT_PHONE)
	//	m_timer->adjust(attotime::from_ticks(72, m_mainclock), T_COMMIT_PHONE);

	if (d_timer.isrunning() == 0 || d_timer.timer_param != T_COMMIT_PHONE) {
		d_timer.start(72, T_COMMIT_PHONE, 1, 0);
	}

}


//-------------------------------------------------
//  inflection_w - handle a write to the
//  inflection bits
//-------------------------------------------------

void sc01_inflection_write(UINT8 data)
{
	// only 2 bits matter
	data &= 3;
	if(m_inflection == data)
		return;

	stream.update();
	m_inflection = data;
}


//-------------------------------------------------
//  sound_stream_update - handle update requests
//  for our sound stream
//-------------------------------------------------

static void sound_stream_update(INT16 **streams, INT32 samples)
{
	INT16 *out = streams[0];

	for(int i=0; i<samples; i++) {
		d_timer.run(18); // 1 sample is 18 machine cycles (see init)
		m_sample_count++;
		if(m_sample_count & 1)
			chip_update();
		out[i] = analog_calc();
	}
}



//**************************************************************************
//  DEVICE INTERFACE
//**************************************************************************

//-------------------------------------------------
//  rom_region - return a pointer to the device's
//  internal ROM region
//-------------------------------------------------
#if 0
const tiny_rom_entry *device_rom_region() const
{
	return ROM_NAME( votrax_sc01 );
}

const tiny_rom_entry *votrax_sc01a_device::device_rom_region() const
{
	return ROM_NAME( votrax_sc01a );
}
#endif

//-------------------------------------------------
//  device_start - handle device startup
//-------------------------------------------------

static void make_rom(INT32 reva)
{
	UINT8 *temp = (reva) ? sc01a_bin : sc01_bin;

	for (int i = 0; i < 64; i++) {
		m_rom[i] = 0;
		for (int b = 0; b < 8; b++) {
			m_rom[i] |= (UINT64)temp[i*8+b] << (b*8);
		}
	}
}

void sc01_set_buffered(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz)
{
	stream.set_buffered(pCPUCyclesCB, nCPUMhz);
}

void sc01_update(INT16 *output, INT32 samples_len)
{
	if (samples_len != nBurnSoundLen) {
		bprintf(0, _T("sc01_update(): once per frame, please!\n"));
		return;
	}

	stream.render(output, samples_len);
}

void sc01_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		d_timer.scan();

		SCAN_VAR(m_mainclock);
		SCAN_VAR(m_sclock);
		SCAN_VAR(m_cclock);
		SCAN_VAR(m_sample_count);
		SCAN_VAR(m_inflection);
		SCAN_VAR(m_phone);
		SCAN_VAR(m_ar_state);
		SCAN_VAR(m_rom_duration);
		SCAN_VAR(m_rom_vd);
		SCAN_VAR(m_rom_cld);
		SCAN_VAR(m_rom_fa);
		SCAN_VAR(m_rom_fc);
		SCAN_VAR(m_rom_va);
		SCAN_VAR(m_rom_f1);
		SCAN_VAR(m_rom_f2);
		SCAN_VAR(m_rom_f2q);
		SCAN_VAR(m_rom_f3);
		SCAN_VAR(m_rom_closure);
		SCAN_VAR(m_rom_pause);
		SCAN_VAR(m_cur_fa);
		SCAN_VAR(m_cur_fc);
		SCAN_VAR(m_cur_va);
		SCAN_VAR(m_cur_f1);
		SCAN_VAR(m_cur_f2);
		SCAN_VAR(m_cur_f2q);
		SCAN_VAR(m_cur_f3);
		SCAN_VAR(m_filt_fa);
		SCAN_VAR(m_filt_fc);
		SCAN_VAR(m_filt_va);
		SCAN_VAR(m_filt_f1);
		SCAN_VAR(m_filt_f2);
		SCAN_VAR(m_filt_f2q);
		SCAN_VAR(m_filt_f3);
		SCAN_VAR(m_phonetick);
		SCAN_VAR(m_ticks);
		SCAN_VAR(m_pitch);
		SCAN_VAR(m_closure);
		SCAN_VAR(m_update_counter);
		SCAN_VAR(m_cur_closure);
		SCAN_VAR(m_noise);
		SCAN_VAR(m_cur_noise);
		SCAN_VAR(m_voice_1);
		SCAN_VAR(m_voice_2);
		SCAN_VAR(m_voice_3);
		SCAN_VAR(m_noise_1);
		SCAN_VAR(m_noise_2);
		SCAN_VAR(m_noise_3);
		SCAN_VAR(m_noise_4);
		SCAN_VAR(m_vn_1);
		SCAN_VAR(m_vn_2);
		SCAN_VAR(m_vn_3);
		SCAN_VAR(m_vn_4);
		SCAN_VAR(m_vn_5);
		SCAN_VAR(m_vn_6);
		SCAN_VAR(m_f1_a);
		SCAN_VAR(m_f1_b);
		SCAN_VAR(m_f2v_a);
		SCAN_VAR(m_f2v_b);
		SCAN_VAR(m_f2n_a);
		SCAN_VAR(m_f2n_b);
		SCAN_VAR(m_f3_a);
		SCAN_VAR(m_f3_b);
		SCAN_VAR(m_f4_a);
		SCAN_VAR(m_f4_b);
		SCAN_VAR(m_fx_a);
		SCAN_VAR(m_fx_b);
		SCAN_VAR(m_fn_a);
		SCAN_VAR(m_fn_b);
	}
}

void sc01_init(INT32 clock, void (*ar_cb)(INT32), INT32 reva)
{
	// initialize internal state
	m_mainclock = clock;
	m_sclock = m_mainclock / 18.0;
	m_cclock = m_mainclock / 36.0;
	//m_stream = stream_alloc(0, 1, m_sclock);
	stream.init(m_sclock, nBurnSoundRate, 1, 1, sound_stream_update);

	m_ar_cb = (ar_cb) ? ar_cb : ar_dummy_cb;

	m_rom = (UINT64*)BurnMalloc(64 * 8);
	make_rom(reva);

	//m_timer = timer_alloc(FUNC(phone_tick), this);
	d_timer.init(0, phone_tick);

	// reset outputs
	m_ar_state = ASSERT_LINE;
}

void sc01_exit()
{
	BurnFree(m_rom);
	stream.exit();
}

//-------------------------------------------------
//  device_reset - handle device reset
//-------------------------------------------------

void sc01_reset()
{
	// Technically, there's no reset in this chip, and initial state
	// is random.  Still, it's a good idea to start it with something
	// sane.

	m_phone = 0x3f;
	m_inflection = 0;
	m_ar_state = ASSERT_LINE;
	m_ar_cb(m_ar_state);

	m_sample_count = 0;

	// Initialize the m_rom* values
	phone_commit();

	// Clear the interpolation sram
	m_cur_fa = m_cur_fc = m_cur_va = 0;
	m_cur_f1 = m_cur_f2 = m_cur_f2q = m_cur_f3 = 0;

	// Initialize the m_filt* values and the filter coefficients
	filters_commit(true);

	// Clear the rest of the internal digital state
	m_pitch = 0;
	m_closure = 0;
	m_update_counter = 0;
	m_cur_closure = true;
	m_noise = 0;
	m_cur_noise = false;

	// Clear the analog level histories
	memset(m_voice_1, 0, sizeof(m_voice_1));
	memset(m_voice_2, 0, sizeof(m_voice_2));
	memset(m_voice_3, 0, sizeof(m_voice_3));

	memset(m_noise_1, 0, sizeof(m_noise_1));
	memset(m_noise_2, 0, sizeof(m_noise_2));
	memset(m_noise_3, 0, sizeof(m_noise_3));
	memset(m_noise_4, 0, sizeof(m_noise_4));

	memset(m_vn_1, 0, sizeof(m_vn_1));
	memset(m_vn_2, 0, sizeof(m_vn_2));
	memset(m_vn_3, 0, sizeof(m_vn_3));
	memset(m_vn_4, 0, sizeof(m_vn_4));
	memset(m_vn_5, 0, sizeof(m_vn_5));
	memset(m_vn_6, 0, sizeof(m_vn_6));
}


//-------------------------------------------------
//  device_clock_changed - handle dynamic clock
//  changes by altering our output frequency
//-------------------------------------------------

void sc01_set_clock(INT32 newclock)
{
	// lookup the new frequency of the master clock, and update if changed
	UINT32 newfreq = newclock;
	if(newfreq != m_mainclock) {
		stream.update();
#if 0
		// dinknote: probably not needed:
		// qbert and coily's "falling off the pyramid" votrax slide works fine w/o this
		// nb, test something that uses larger pitch(clock) steps and see if its ok?
		if(!m_timer->expire().is_never()) {
			// determine how many clock ticks remained on the timer
			UINT64 remaining = m_timer->remaining().as_ticks(m_mainclock);

			// adjust the timer to the same number of ticks based on the new frequency
			m_timer->adjust(attotime::from_ticks(remaining, newfreq));
		}
#endif
		m_mainclock = newfreq;
		m_sclock = m_mainclock / 18.0;
		m_cclock = m_mainclock / 36.0;
		//m_stream->set_sample_rate(m_sclock);
		stream.set_rate(m_sclock);
		filters_commit(true);
	}
}


//-------------------------------------------------
//  phone_tick - process transitions
//-------------------------------------------------

static void phone_tick(int param)
{
	// stream.update(); not needed here, since timer updates are happening in
	// sound_stream_update()! (basically: it's already synched at this point)

	switch(param) {
	case T_COMMIT_PHONE:
		// strobe -> commit transition,
		phone_commit();
		//m_timer->adjust(attotime::from_ticks(16*(m_rom_duration*4+1)*4*9+2, m_mainclock), T_END_OF_PHONE);
		d_timer.start(16*(m_rom_duration*4+1)*4*9+2, T_END_OF_PHONE, 1, 0);
		break;

	case T_END_OF_PHONE:
		// end of phone
		m_ar_state = ASSERT_LINE;
		break;
	}

	m_ar_cb(m_ar_state);
}


static void phone_commit()
{
	// Only these two counters are reset on phone change, the rest is
	// free-running.
	m_phonetick = 0;
	m_ticks = 0;

	// In the real chip, the rom is re-read all the time.  Since it's
	// internal and immutable, no point in not caching it though.
	for(int i=0; i<64; i++) {
		UINT64 val = m_rom[i];
		if(m_phone == ((val >> 56) & 0x3f)) {
			m_rom_f1  = BITSWAP04(val,  0,  7, 14, 21);
			m_rom_va  = BITSWAP04(val,  1,  8, 15, 22);
			m_rom_f2  = BITSWAP04(val,  2,  9, 16, 23);
			m_rom_fc  = BITSWAP04(val,  3, 10, 17, 24);
			m_rom_f2q = BITSWAP04(val,  4, 11, 18, 25);
			m_rom_f3  = BITSWAP04(val,  5, 12, 19, 26);
			m_rom_fa  = BITSWAP04(val,  6, 13, 20, 27);

			// These two values have their bit orders inverted
			// compared to everything else due to a bug in the
			// prototype (miswiring of the comparator with the ticks
			// count) they compensated in the rom.

			m_rom_cld = BITSWAP04(val, 34, 32, 30, 28);
			m_rom_vd  = BITSWAP04(val, 35, 33, 31, 29);

			m_rom_closure  = BIT(val, 36);
			m_rom_duration = BITSWAP07(~val, 37, 38, 39, 40, 41, 42, 43);

			// Hard-wired on the die, not an actual part of the rom.
			m_rom_pause = (m_phone == 0x03) || (m_phone == 0x3e);

			//LOGMASKED(LOG_COMMIT, "commit fa=%x va=%x fc=%x f1=%x f2=%x f2q=%x f3=%x dur=%02x cld=%x vd=%d cl=%d pause=%d\n", m_rom_fa, m_rom_va, m_rom_fc, m_rom_f1, m_rom_f2, m_rom_f2q, m_rom_f3, m_rom_duration, m_rom_cld, m_rom_vd, m_rom_closure, m_rom_pause);

			// That does not happen in the sc01(a) rom, but let's
			// cover our behind.
			if(m_rom_cld == 0)
				m_cur_closure = m_rom_closure;

			return;
		}
	}
}

static void interpolate(UINT8 &reg, UINT8 target)
{
	// One step of interpolation, adds one eight of the distance
	// between the current value and the target.
	reg = reg - (reg >> 3) + (target << 1);
}

static void chip_update()
{
	// Phone tick counter update.  Stopped when ticks reach 16.
	// Technically the counter keeps updating, but the comparator is
	// disabled.
	if(m_ticks != 0x10) {
		m_phonetick++;
		// Comparator is with duration << 2, but there's a one-tick
		// delay in the path.
		if(m_phonetick == ((m_rom_duration << 2) | 1)) {
			m_phonetick = 0;
			m_ticks++;
			if(m_ticks == m_rom_cld)
				m_cur_closure = m_rom_closure;
		}
	}

	// The two update timing counters.  One divides by 16, the other
	// by 48, and they're phased so that the 208Hz counter ticks
	// exactly between two 625Hz ticks.
	m_update_counter++;
	if(m_update_counter == 0x30)
		m_update_counter = 0;

	bool tick_625 = !(m_update_counter & 0xf);
	bool tick_208 = m_update_counter == 0x28;

	// Formant update.  Die bug there: fc should be updated, not va.
	// The formants are frozen on a pause phone unless both voice and
	// noise volumes are zero.
	if(tick_208 && (!m_rom_pause || !(m_filt_fa || m_filt_va))) {
		// interpolate(m_cur_va,  m_rom_va);
		interpolate(m_cur_fc,  m_rom_fc);
		interpolate(m_cur_f1,  m_rom_f1);
		interpolate(m_cur_f2,  m_rom_f2);
		interpolate(m_cur_f2q, m_rom_f2q);
		interpolate(m_cur_f3,  m_rom_f3);
		//LOGMASKED(LOG_INT, "int fa=%x va=%x fc=%x f1=%x f2=%02x f2q=%02x f3=%x\n", m_cur_fa >> 4, m_cur_va >> 4, m_cur_fc >> 4, m_cur_f1 >> 4, m_cur_f2 >> 3, m_cur_f2q >> 4, m_cur_f3 >> 4);
	}

	// Non-formant update. Same bug there, va should be updated, not fc.
	if(tick_625) {
		if(m_ticks >= m_rom_vd)
			interpolate(m_cur_fa, m_rom_fa);
		if(m_ticks >= m_rom_cld) {
			// interpolate(m_cur_fc, m_rom_fc);
			interpolate(m_cur_va, m_rom_va);
			//LOGMASKED(LOG_INT, "int fa=%x va=%x fc=%x f1=%x f2=%02x f2q=%02x f3=%x\n", m_cur_fa >> 4, m_cur_va >> 4, m_cur_fc >> 4, m_cur_f1 >> 4, m_cur_f2 >> 3, m_cur_f2q >> 4, m_cur_f3 >> 4);
		}
	}

	// Closure counter, reset every other tick in theory when not
	// active (on the extra rom cycle).
	//
	// The closure level is immediately used in the analog path,
	// there's no pitch synchronization.

	if(!m_cur_closure && (m_filt_fa || m_filt_va))
		m_closure = 0;
	else if(m_closure != 7 << 2)
		m_closure ++;

	// Pitch counter.  Equality comparison, so it's possible to make
	// it miss by manipulating the inflection inputs, but it'll wrap.
	// There's a delay, hence the +2.

	// Intrinsically pre-divides by two, so we added one bit on the 7

	m_pitch = (m_pitch + 1) & 0xff;
	if(m_pitch == (0xe0 ^ (m_inflection << 5) ^ (m_filt_f1 << 1)) + 2)
		m_pitch = 0;

	// Filters are updated in index 1 of the pitch wave, which does
	// indeed mean four times in a row.
	if((m_pitch & 0xf9) == 0x08)
		filters_commit(false);

	// Noise shift register.  15 bits, with a nxor on the last two
	// bits for the loop.
	bool inp = (1||m_filt_fa) && m_cur_noise && (m_noise != 0x7fff);
	m_noise = ((m_noise << 1) & 0x7ffe) | inp;
	m_cur_noise = !(((m_noise >> 14) ^ (m_noise >> 13)) & 1);

	//LOGMASKED(LOG_TICK, "%s tick %02x.%03x 625=%d 208=%d pitch=%02x.%x ns=%04x ni=%d noise=%d cl=%x.%x clf=%d/%d\n", machine().time().to_string(), m_ticks, m_phonetick, tick_625, tick_208, m_pitch >> 3, m_pitch & 7, m_noise, inp, m_cur_noise, m_closure >> 2, m_closure & 3, m_rom_closure, m_cur_closure);
}

static void filters_commit(bool force)
{
	m_filt_fa = m_cur_fa >> 4;
	m_filt_fc = m_cur_fc >> 4;
	m_filt_va = m_cur_va >> 4;
#if 0
	// dinknote: convert the c++17 initializer list params in bits_to_caps() to something more palatable by older compilers
	if(force || m_filt_f1 != m_cur_f1 >> 4) {
		m_filt_f1 = m_cur_f1 >> 4;

		build_standard_filter(m_f1_a, m_f1_b,
							  11247,
							  11797,
							  949,
							  52067,
							  2280 + bits_to_caps(m_filt_f1, { 2546, 4973, 9861, 19724 }),
							  166272);
	}

	if(force || m_filt_f2 != m_cur_f2 >> 3 || m_filt_f2q != m_cur_f2q >> 4) {
		m_filt_f2 = m_cur_f2 >> 3;
		m_filt_f2q = m_cur_f2q >> 4;

		build_standard_filter(m_f2v_a, m_f2v_b,
							  24840,
							  29154,
							  829 + bits_to_caps(m_filt_f2q, { 1390, 2965, 5875, 11297 }),
							  38180,
							  2352 + bits_to_caps(m_filt_f2, { 833, 1663, 3164, 6327, 12654 }),
							  34270);

		build_injection_filter(m_f2n_a, m_f2n_b,
							   29154,
							   829 + bits_to_caps(m_filt_f2q, { 1390, 2965, 5875, 11297 }),
							   38180,
							   2352 + bits_to_caps(m_filt_f2, { 833, 1663, 3164, 6327, 12654 }),
							   34270);
	}

	if(force || m_filt_f3 != m_cur_f3 >> 4) {
		m_filt_f3 = m_cur_f3 >> 4;
		build_standard_filter(m_f3_a, m_f3_b,
							  0,
							  17594,
							  868,
							  18828,
							  8480 + bits_to_caps(m_filt_f3, { 2226, 4485, 9056, 18111 }),
							  50019);
	}

	if(force) {
		build_standard_filter(m_f4_a, m_f4_b,
							  0,
							  28810,
							  1165,
							  21457,
							  8558,
							  7289);

		build_lowpass_filter(m_fx_a, m_fx_b,
							 1122,
							 23131);

		build_noise_shaper_filter(m_fn_a, m_fn_b,
								  15500,
								  14854,
								  8450,
								  9523,
								  14083);
	}
#endif

#if 1
	if(force || m_filt_f1 != m_cur_f1 >> 4) {
		m_filt_f1 = m_cur_f1 >> 4;
		double arg_f1_a[4] = { 2546, 4973, 9861, 19724 };
		build_standard_filter(m_f1_a, m_f1_b,
							  11247,
							  11797,
							  949,
							  52067,
							  2280 + bits_to_caps(m_filt_f1, 4, &arg_f1_a[0]),
							  166272);
	}

	if(force || m_filt_f2 != m_cur_f2 >> 3 || m_filt_f2q != m_cur_f2q >> 4) {
		m_filt_f2 = m_cur_f2 >> 3;
		m_filt_f2q = m_cur_f2q >> 4;

		double arg_f2q[4] = { 1390, 2965, 5875, 11297 };
		double arg_f2[5] = { 833, 1663, 3164, 6327, 12654 };
		build_standard_filter(m_f2v_a, m_f2v_b,
							  24840,
							  29154,
							  829 + bits_to_caps(m_filt_f2q, 4, &arg_f2q[0]),
							  38180,
							  2352 + bits_to_caps(m_filt_f2, 5, &arg_f2[0]),
							  34270);

		build_injection_filter(m_f2n_a, m_f2n_b,
							   29154,
							   829 + bits_to_caps(m_filt_f2q, 4, &arg_f2q[0]),
							   38180,
							   2352 + bits_to_caps(m_filt_f2, 5, &arg_f2[0]),
							   34270);
	}

	if(force || m_filt_f3 != m_cur_f3 >> 4) {
		m_filt_f3 = m_cur_f3 >> 4;
		double arg_f3[4] = { 2226, 4485, 9056, 18111 };
		build_standard_filter(m_f3_a, m_f3_b,
							  0,
							  17594,
							  868,
							  18828,
							  8480 + bits_to_caps(m_filt_f3, 4, &arg_f3[0]),
							  50019);
	}

	if(force) {
		build_standard_filter(m_f4_a, m_f4_b,
							  0,
							  28810,
							  1165,
							  21457,
							  8558,
							  7289);

		build_lowpass_filter(m_fx_a, m_fx_b,
							 1122,
							 23131);

		build_noise_shaper_filter(m_fn_a, m_fn_b,
								  15500,
								  14854,
								  8450,
								  9523,
								  14083);
	}
#endif
//	if(m_filt_fa | m_filt_va | m_filt_fc | m_filt_f1 | m_filt_f2 | m_filt_f2q | m_filt_f3)
		//LOGMASKED(LOG_FILTER, "filter fa=%x va=%x fc=%x f1=%x f2=%02x f2q=%x f3=%x\n", m_filt_fa, m_filt_va, m_filt_fc, m_filt_f1, m_filt_f2, m_filt_f2q, m_filt_f3);
}

static INT16 analog_calc()
{
	// Voice-only path.
	// 1. Pick up the pitch wave

	double v = m_pitch >= (9 << 3) ? 0 : s_glottal_wave[m_pitch >> 3];

	// 2. Multiply by the initial amplifier.  It's linear on the die,
	// even if it's not in the patent.
	v = v * m_filt_va / 15.0;
	shift_hist(v, m_voice_1);

	// 3. Apply the f1 filter
	v = apply_filter(m_voice_1, m_voice_2, m_f1_a, m_f1_b);
	shift_hist(v, m_voice_2);

	// 4. Apply the f2 filter, voice half
	v = apply_filter(m_voice_2, m_voice_3, m_f2v_a, m_f2v_b);
	shift_hist(v, m_voice_3);

	// Noise-only path
	// 5. Pick up the noise pitch.  Amplitude is linear.  Base
	// intensity should be checked w.r.t the voice.
	double n = 1e4 * ((m_pitch & 0x40 ? m_cur_noise : false) ? 1 : -1);
	n = n * m_filt_fa / 15.0;
	shift_hist(n, m_noise_1);

	// 6. Apply the noise shaper
	n = apply_filter(m_noise_1, m_noise_2, m_fn_a, m_fn_b);
	shift_hist(n, m_noise_2);

	// 7. Scale with the f2 noise input
	double n2 = n * m_filt_fc / 15.0;
	shift_hist(n2, m_noise_3);

	// 8. Apply the f2 filter, noise half,
	n2 = apply_filter(m_noise_3, m_noise_4, m_f2n_a, m_f2n_b);
	shift_hist(n2, m_noise_4);

	// Mixed path
	// 9. Add the f2 voice and f2 noise outputs
	double vn = v + n2;
	shift_hist(vn, m_vn_1);

	// 10. Apply the f3 filter
	vn = apply_filter(m_vn_1, m_vn_2, m_f3_a, m_f3_b);
	shift_hist(vn, m_vn_2);

	// 11. Second noise insertion
	vn += n * (5 + (15^m_filt_fc))/20.0;
	shift_hist(vn, m_vn_3);

	// 12. Apply the f4 filter
	vn = apply_filter(m_vn_3, m_vn_4, m_f4_a, m_f4_b);
	shift_hist(vn, m_vn_4);

	// 13. Apply the glottal closure amplitude, also linear
	vn = vn * (7 ^ (m_closure >> 2)) / 7.0;
	shift_hist(vn, m_vn_5);

	// 13. Apply the final fixed filter
	vn = apply_filter(m_vn_5, m_vn_6, m_fx_a, m_fx_b);
	shift_hist(vn, m_vn_6);

	return (vn*0.35) * 0x7fff; // dinknote: was returning a double, let's promote this to INT16
}

/*
  Playing with analog filters, or where all the magic filter formulas are coming from.

  First you start with an analog circuit, for instance this one:

  |                     +--[R2]--+
  |                     |        |
  |                     +--|C2|--+<V1     +--|C3|--+
  |                     |        |        |        |
  |  Vi   +--[R1]--+    |  |\    |        |  |\    |
  |  -----+        +----+--+-\   |        +--+-\   |
  |       +--|C1|--+       |  >--+--[Rx]--+  |  >--+----- Vo
  |                |     0-++/             0-++/   |
  |                |       |/    +--[R0]--+  |/    |
  |                |             |        |        |
  |                |             |    /|  |        |
  |                |             |   /-+--+--[R0]--+
  |                +--[R4]-------+--<  |
  |                            V2^   \++-0
  |                                   \|

  It happens to be what most of the filters in the sc01a look like.

  You need to determine the transfer function H(s) of the circuit, which is
  defined as the ratio Vo/Vi.  To do that, you use some properties:

  - The intensity through an element is equal to the voltage
    difference through the element divided by the impedance

  - The impedance of a resistance is equal to its resistance

  - The impedance of a capacitor is 1/(s*C) where C is its capacitance

  - The impedance of elements in series is the sum of their impedances

  - The impedance of elements in parallel is the inverse of the sum of
    their inverses

  - The sum of all intensities flowing into a node is 0 (there's no
    charge accumulation in a wire)

  - An operational amplifier in looped mode is an interesting beast:
    the intensity at its two inputs is always 0, and the voltage is
    forced identical between the inputs.  In our case, since the '+'
    inputs are all tied to ground, that means that the '-' inputs are at
    voltage 0, intensity 0.

  From here we can build some equations.  Noting:
  X1 = 1/(1/R1 + s*C1)
  X2 = 1/(1/R2 + s*C2)
  X3 = 1/(s*C3)

  Then computing the intensity flow at each '-' input we have:
  Vi/X1 + V2/R4 + V1/X2 = 0
  V2/R0 + Vo/R0 = 0
  V1/Rx + Vo/X3 = 0

  Wrangling the equations, one eventually gets:
  |                            1 + s * C1*R1
  | Vo/Vi = H(s) = (R4/R1) * -------------------------------------------
  |                            1 + s * C3*Rx*R4/R2 + s^2 * C2*C3*Rx*R4

  To check the mathematics between the 's' stuff, check "Laplace
  transform".  In short, it's a nice way of manipulating derivatives
  and integrals without having to manipulate derivatives and
  integrals.

  With that transfer function, we first can compute what happens to
  every frequency in the input signal.  You just compute H(2i*pi*f)
  where f is the frequency, which will give you a complex number
  representing the amplitude and phase effect.  To get the usual dB
  curves, compute 20*log10(abs(v))).

  Now, once you have an analog transfer function, you can build a
  digital filter from it using what is called the bilinear transform.

  In our case, we have an analog filter with the transfer function:
  |                 1 + k[0]*s
  |        H(s) = -------------------------
  |                 1 + k[1]*s + k[2]*s^2

  We can always reintroduce the global multiplier later, and it's 1 in
  most of our cases anyway.

  The we pose:
  |                    z-1
  |        s(z) = zc * ---
  |                    z+1

  where zc = 2*pi*fr/tan(pi*fr/fs)
  with fs = sampling frequency
  and fr = most interesting frequency

  Then we rewrite H in function of negative integer powers of z.

  Noting m0 = zc*k[0], m1 = zc*k[1], m2=zc*zc*k[2],

  a little equation wrangling then gives:

  |                 (1+m0)    + (3+m0)   *z^-1 + (3-m0)   *z^-2 +    (1-m0)*z^-3
  |        H(z) = ----------------------------------------------------------------
  |                 (1+m1+m2) + (3+m1-m2)*z^-1 + (3-m1-m2)*z^-2 + (1-m1+m2)*z^-3

  That beast in the digital transfer function, of which you can
  extract response curves by posing z = exp(2*i*pi*f/fs).

  Note that the bilinear transform is an approximation, and H(z(f)) =
  H(s(f)) only at frequency fr.  And the shape of the filter will be
  better respected around fr.  If you look at the curves of the
  filters we're interested in, the frequency:
  fr = sqrt(abs(k[0]*k[1]-k[2]))/(2*pi*k[2])

  which is a (good) approximation of the filter peak position is a
  good choice.

  Note that terminology wise, the "standard" bilinear transform is
  with fr = fs/2, and using a different fr is called "pre-warping".

  So now we have a digital transfer function of the generic form:

  |                 a[0] + a[1]*z^-1 + a[2]*z^-2 + a[3]*z^-3
  |        H(z) = --------------------------------------------
  |                 b[0] + b[1]*z^-1 + b[2]*z^-2 + b[3]*z^-3

  The magic then is that the powers of z represent time in samples.
  Noting x the input stream and y the output stream, you have:
  H(z) = y(z)/x(z)

  or in other words:
  y*b[0]*z^0 + y*b[1]*z^-1 + y*b[2]*z^-2 + y*b[3]*z^-3 = x*a[0]*z^0 + x*a[1]*z^-1 + x*a[2]*z^-2 + x*a[3]*z^-3

  i.e.

  y*z^0 = (x*a[0]*z^0 + x*a[1]*z^-1 + x*a[2]*z^-2 + x*a[3]*z^-3 - y*b[1]*z^-1 - y*b[2]*z^-2 - y*b[3]*z^-3) / b[0]

  and powers of z being time in samples,

  y[0] = (x[0]*a[0] + x[-1]*a[1] + x[-2]*a[2] + x[-3]*a[3] - y[-1]*b[1] - y[-2]*b[2] - y[-3]*b[3]) / b[0]

  So you have a filter you can apply.  Note that this is why you want
  negative powers of z.  Positive powers would mean looking into the
  future (which is possible in some cases, in particular with x, and
  has some very interesting properties, but is not very useful in
  analog circuit simulation).

  Note that if you have multiple inputs, all this stuff is linear.
  Or, in other words, you just have to split it in multiple circuits
  with only one input connected each time and sum the results.  It
  will be correct.

  Also, since we're in practice in a dynamic system, for an amplifying
  filter (i.e. where things like r4/r1 is not 1), it's better to
  proceed in two steps:

  - amplify the input by the current value of the coefficient, and
    historize it
  - apply the now non-amplifying filter to the historized amplified
    input

  That way reduces the probability of the output bouncing all over the
  place.

  Except, we're not done yet.  Doing resistors precisely in an IC is
  very hard and/or expensive (you may have heard of "laser cut
  resistors" in DACs of the time).  Doing capacitors is easier, and
  their value is proportional to their surface.  So there are no
  resistors on the sc01 die (which is a lie, there are three, but not
  in the filter path.  They are used to scale the voltage in the pitch
  wave and to generate +5V from the +9V), but a magic thing called a
  switched capacitor.  Lookup patent 4,433,210 for details.  Using
  high frequency switching a capacitor can be turned into a resistor
  of value 1/(C*f) where f is the switching frequency (20Khz,
  main/36).  And the circuit is such that the absolute value of the
  capacitors is irrelevant, only their ratio is useful, which factors
  out the intrinsic capacity-per-surface-area of the IC which may be
  hard to keep stable from one die to another.  As a result all the
  capacitor values we use are actually surfaces in square micrometers.

  For the curious, it looks like the actual capacitance was around 25
  femtofarad per square micrometer.

*/

static void build_standard_filter(double *a, double *b,
											   double c1t, // Unswitched cap, input, top
											   double c1b, // Switched cap, input, bottom
											   double c2t, // Unswitched cap, over first amp-op, top
											   double c2b, // Switched cap, over first amp-op, bottom
											   double c3,  // Cap between the two op-amps
											   double c4)  // Cap over second op-amp
{
	// First compute the three coefficients of H(s).  One can note
	// that there is as many capacitor values on both sides of the
	// division, which confirms that the capacity-per-surface-area
	// is not needed.
	double k0 = c1t / (m_cclock * c1b);
	double k1 = c4 * c2t / (m_cclock * c1b * c3);
	double k2 = c4 * c2b / (m_cclock * m_cclock * c1b * c3);

	// Estimate the filter cutoff frequency
	double fpeak = sqrt(fabs(k0*k1 - k2))/(2*M_PI*k2);

	// Turn that into a warp multiplier
	double zc = 2*M_PI*fpeak/tan(M_PI*fpeak / m_sclock);

	// Finally compute the result of the z-transform
	double m0 = zc*k0;
	double m1 = zc*k1;
	double m2 = zc*zc*k2;

	a[0] = 1+m0;
	a[1] = 3+m0;
	a[2] = 3-m0;
	a[3] = 1-m0;
	b[0] = 1+m1+m2;
	b[1] = 3+m1-m2;
	b[2] = 3-m1-m2;
	b[3] = 1-m1+m2;
}

/*
  Second filter type used once at the end, much simpler:

  |           +--[R1]--+
  |           |        |
  |           +--|C1|--+
  |           |        |
  |  Vi       |  |\    |
  |  ---[R0]--+--+-\   |
  |              |  >--+------ Vo
  |            0-++/
  |              |/


  Vi/R0 = Vo / (1/(1/R1 + s.C1)) = Vo (1/R1 + s.C1)
  H(s) = Vo/Vi = (R1/R0) * (1 / (1 + s.R1.C1))
*/

static void build_lowpass_filter(double *a, double *b,
											  double c1t, // Unswitched cap, over amp-op, top
											  double c1b) // Switched cap, over amp-op, bottom
{
	// The caps values puts the cutoff at around 150Hz, put that's no good.
	// Recordings shows we want it around 4K, so fuzz it.

	// Compute the only coefficient we care about
	double k = c1b / (m_cclock * c1t) * (150.0/4000.0);

	// Compute the filter cutoff frequency
	double fpeak = 1/(2*M_PI*k);

	// Turn that into a warp multiplier
	double zc = 2*M_PI*fpeak/tan(M_PI*fpeak / m_sclock);

	// Finally compute the result of the z-transform
	double m = zc*k;

	a[0] = 1;
	b[0] = 1+m;
	b[1] = 1-m;
}

/*
  Used to shape the white noise

         +-------------------------------------------------------------------+
         |                                                                   |
         +--|C1|--+---------|C3|----------+--|C4|--+                         |
         |        |      +        +       |        |                         |
   Vi    |  |\    |     (1)      (1)      |        |       +        +        |
   -|R0|-+--+-\   |      |        |       |  |\    |      (1)      (1)       |
            |  >--+--(2)-+--|C2|--+---(2)-+--+-\   |       |        |        |
          0-++/          |                   |  >--+--(2)--+--|C5|--+---(2)--+
            |/          Vo                 0-++/
                                             |/
   Equivalent:

         +------------------|R5|-------------------+
         |                                         |
         +--|C1|--+---------|C3|----------+--|C4|--+
         |        |                       |        |
   Vi    |  |\    |                       |        |
   -|R0|-+--+-\   |                       |  |\    |
            |  >--+---------|R2|----------+--+-\   |
          0-++/   |                          |  >--+
            |/   Vo                        0-++/
                                             |/

  We assume r0 = r2
*/

static void build_noise_shaper_filter(double *a, double *b,
												   double c1,  // Cap over first amp-op
												   double c2t, // Unswitched cap between amp-ops, input, top
												   double c2b, // Switched cap between amp-ops, input, bottom
												   double c3,  // Cap over second amp-op
												   double c4)  // Switched cap after second amp-op
{
	// Coefficients of H(s) = k1*s / (1 + k2*s + k3*s^2)
	double k0 = c2t*c3*c2b/c4;
	double k1 = c2t*(m_cclock * c2b);
	double k2 = c1*c2t*c3/(m_cclock * c4);

	// Estimate the filter cutoff frequency
	double fpeak = sqrt(1/k2)/(2*M_PI);

	// Turn that into a warp multiplier
	double zc = 2*M_PI*fpeak/tan(M_PI*fpeak / m_sclock);

	// Finally compute the result of the z-transform
	double m0 = zc*k0;
	double m1 = zc*k1;
	double m2 = zc*zc*k2;

	a[0] = m0;
	a[1] = 0;
	a[2] = -m0;
	b[0] = 1+m1+m2;
	b[1] = 2-2*m2;
	b[2] = 1-m1+m2;
}

/*
  Noise injection in f2

  |                     +--[R2]--+        +--[R1]-------- Vi
  |                     |        |        |
  |                     +--|C2|--+<V1     +--|C3|--+
  |                     |        |        |        |
  |                     |  |\    |        |  |\    |
  |                +----+--+-\   |        +--+-\   |
  |                |       |  >--+--[Rx]--+  |  >--+----- Vo
  |                |     0-++/             0-++/   |
  |                |       |/    +--[R0]--+  |/    |
  |                |             |        |        |
  |                |             |    /|  |        |
  |                |             |   /-+--+--[R0]--+
  |                +--[R4]-------+--<  |
  |                            V2^   \++-0
  |                                   \|

  We drop r0/r1 out of the equation (it factorizes), and we rescale so
  that H(infinity)=1.
*/

static void build_injection_filter(double *a, double *b,
												double c1b, // Switched cap, input, bottom
												double c2t, // Unswitched cap, over first amp-op, top
												double c2b, // Switched cap, over first amp-op, bottom
												double c3,  // Cap between the two op-amps
												double c4)  // Cap over second op-amp
{
	// First compute the three coefficients of H(s) = (k0 + k2*s)/(k1 - k2*s)
	double k0 = m_cclock * c2t;
	double k1 = m_cclock * (c1b * c3 / c2t - c2t);
	double k2 = c2b;

	// Don't pre-warp
	double zc = 2*m_sclock;

	// Finally compute the result of the z-transform
	double m = zc*k2;

	a[0] = k0 + m;
	a[1] = k0 - m;
	b[0] = k1 - m;
	b[1] = k1 + m;

	// That ends up in a numerically unstable filter.  Neutralize it for now.
	a[0] = 0;
	a[1] = 0;
	b[0] = 1;
	b[1] = 0;
}
