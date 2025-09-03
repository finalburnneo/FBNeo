// ideas:
// RAWTAP (.wav): needs some finishing

// FinalBurn NEO ZX Spectrum driver.  NEO edition!
// media: .tap, .tzx, .z80 snapshots
// input: kbd, kempston: joy1, sinclair intf.2: joy1 & joy2

#include "tiles_generic.h"
#include "spectrum.h"
#include "z80_intf.h"
#include "ay8910.h"
#include <math.h>

#if defined (_MSC_VER)
#define strcasecmp stricmp
#endif

static INT32 SpecMode = 0;
#define SPEC_TAP		(1 << 0)
#define SPEC_Z80		(1 << 1)
#define SPEC_128K		(1 << 2)
#define SPEC_PLUS2		(1 << 3) // +2a (Amstrad model)
#define SPEC_INVES		(1 << 4) // Spanish clone (non-contended ula)
#define SPEC_AY8910		(1 << 5)
#define SPEC_SLOWTAP	(1 << 6) // Slow-TAP mode, for games with custom loaders
#define SPEC_TZX        (1 << 7)

#define TAPE_TO_WAV 0 // outputs zxout.pcm (2ch/stereo/sound rate hz)
#if TAPE_TO_WAV
static INT32 tape_to_wav_startup_pause;
static FILE *tape_dump_fp = NULL;
#endif

UINT8 SpecInputKbd[0x10][0x05] = {
	{ 0, 0, 0, 0, 0 }, // Shift, Z, X, C, V
	{ 0, 0, 0, 0, 0 }, // A, S, D, F, G
	{ 0, 0, 0, 0, 0 }, // Q, W, E, R, T
	{ 0, 0, 0, 0, 0 }, // 1, 2, 3, 4, 5
	{ 0, 0, 0, 0, 0 }, // 0, 9, 8, 7, 6
	{ 0, 0, 0, 0, 0 }, // P, O, I, U, Y
	{ 0, 0, 0, 0, 0 }, // Enter, L, K, J, H
	{ 0, 0, 0, 0, 0 }, // Space, Sym, M, N, B
	{ 0, 0, 0, 0, 0 }, // Kempston joy1
	{ 0, 0, 0, 0, 0 }, // Sinclair intf.2 joy1
	{ 0, 0, 0, 0, 0 }, // Sinclair intf.2 joy2
	// 128K+ Caps Shift (Secondary Membrane)
	{ 0, 0, 0, 0, 0 }, // Edit, CapsLock, True Vid, Inv Vid, Cursor Left
	{ 0, 0, 0, 0, 0 }, // Del, Graph, Cursor Right, Cursor Up, Cursor Down
	{ 0, 0, 0, 0, 0 }, // Break, Ext Mode
	// 128K+ Symbol Shift (Secondary Membrane)
	{ 0, 0, 0, 0, 0 }, // Quote, Semicolon
	{ 0, 0, 0, 0, 0 }  // Period, Comma
};

UINT8 SpecDips[2];
UINT8 SpecInput[0x10];
UINT8 SpecReset;
UINT8 SpecRecalc;

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *RamStart;
static UINT8 *RamEnd;
static UINT8 *SpecZ80Rom;
static UINT8 *SpecZ80Ram;
static UINT8 *SpecVideoRam;
static UINT8 *SpecSnapshotData;
static INT32  SpecSnapshotDataLen;
static UINT8 *SpecTAP;
static INT32  SpecTAPLen;

static UINT32 *SpecPalette;

static UINT8 ula_attr;
static UINT8 ula_scr;
static UINT8 ula_byte;
static UINT8 ula_border;
static UINT8 ula_flash;
static INT32 ula_last_cyc;
static INT32 nExtraCycles;

static INT32 in_tape_ffwd = 0;

static INT16 *Buzzer;
static INT16 *BuzzerD8;

static INT32 SpecRamPage; // calculated from Spec128kMapper*
static INT32 SpecRomPage; // calculated from Spec128kMapper*
static INT32 Spec128kMapper;
static INT32 Spec128kMapper2;

static INT32 SpecScanlines;
static INT32 SpecCylesPerScanline;
static INT32 SpecContention;

static INT16 dac_lastin;
static INT16 dac_lastout;

static void spectrum128_bank(); // forwards
static void spectrum_loadz80();
static void ula_init(INT32 scanlines, INT32 cyc_scanline, INT32 contention);
static void update_ula(INT32 cycle);

// Spectrum 48k tap-loading robot -dink
static INT32 CASFrameCounter = 0; // for autoloading
static INT32 CASAutoLoadPos = 0;
static INT32 CASAutoLoadTicker = 0;
static void SpecTAPReset(); // forward

static void SpecLoadTAP()
{
	CASAutoLoadPos = 0;
	CASAutoLoadTicker = 0;
	CASFrameCounter = 0;
}

static UINT8* FindInput(char *str)
{
	for (INT32 i = 0; SpecInputList[i].szName != NULL; i++) {
		if (!strcmp(str, SpecInputList[i].szName)) {
			return SpecInputList[i].pVal;
		}
	}
	return NULL;
}

static void SetInput(char *str, INT32 data)
{
	UINT8 *x = FindInput(str);
	if (x) {
		x[0] = data;
	}
}

static void TAPAutoLoadTick()
{
	const UINT8 TAPLoader[2][10] = { { "J\"\"\n\0" }, { "\n\0" } }; // 48k, 128k
	const INT32 KeyDelay = 12; // frames 0-4: press key, 5-11: delay, 11: next character.

	if (CASAutoLoadPos == 0xff) return;

	UINT8 c = TAPLoader[((SpecMode & SPEC_128K) ? 1 : 0)][CASAutoLoadPos];
	if (!c) {
		CASAutoLoadPos = 0xff;
		return;
	}

	if ((CASAutoLoadTicker % KeyDelay) < 5) {
		switch (c) {
			case '\"': {
				SetInput("SYMBOL SHIFT", 1);
				SetInput("P", 1);
				break;
			}
			case 'J': {
				SetInput("J", 1);
				break;
			}
			case '\n': {
				SetInput("ENTER", 1);
				break;
			}
		}
	}

	if ((CASAutoLoadTicker % KeyDelay) == KeyDelay - 1) CASAutoLoadPos++;
	CASAutoLoadTicker++;
}

static void TAPAutoLoadRobot()
{
	if (SpecMode & SPEC_TAP && CASFrameCounter > 90) {
		TAPAutoLoadTick();
	}
	CASFrameCounter++;
}
// End TAP Robot

// Oversampling Buzzer-DAC
static INT32 buzzer_last_update;
static INT32 buzzer_last_data;
static INT32 buzzer_data_len;
static INT32 buzzer_data_frame;
static INT32 buzzer_data_frame_second;
static UINT32 buzzer_d8_fract;
static UINT32 buzzer_d8_accu;

// Filter section, for "ox8"x oversampling
#define ox8 (4)
#define FIR_TAPS (100 + 1)
static float filt_coeffs[FIR_TAPS];
static float c_buffer[FIR_TAPS];
static INT32 c_position;

enum { WINDOW_NONE = 0, WINDOW_HAMMING = 1, WINDOW_BLACKMAN = 2 };

static float sinc(float x)
{
	return (x == 0.0) ? 1.0 : sin(M_PI * x) / (M_PI * x);
}

static void generate_filter_coeffs(float *coeffs, int window, int taps, float cutoff, float sample_rate)
{
    int mid = taps / 2;
    float normalized_cutoff = cutoff / (sample_rate / 2.0);

    for (int i = 0; i < taps; i++) {
        float sinc_value = sinc((i - mid) * normalized_cutoff);
		float hamming_window = 0.54 - 0.46 * cos(2.0 * M_PI * i / (taps - 1));
		float blackman_window = 0.42 - 0.5 * cos(2.0 * M_PI * i / (taps - 1)) + 0.08 * cos(4.0 * M_PI * i / (taps - 1));
		switch (window) {
			case WINDOW_HAMMING: coeffs[i] = sinc_value * hamming_window; break;
			case WINDOW_BLACKMAN: coeffs[i] = sinc_value * blackman_window; break;
			case WINDOW_NONE: coeffs[i] = sinc_value; break;
		}
    }

	float sum = 0.0;
    for (int i = 0; i < taps; i++) {
        sum += coeffs[i];
	}
//	bprintf(0, _T("sum before normalization: %f\n"), sum);
    for (int i = 0; i < taps; i++) {
        coeffs[i] /= sum;
    }
//	bprintf(0, _T("sum after normalization: %f\n"), sum);
}

static float update_filter(float sample) {
    c_buffer[c_position] = sample;
	c_position = (c_position == FIR_TAPS - 1) ? 0 : c_position + 1; // avoid modulus

	float result = 0.0; // its faster to use a float here, as long as the filter coefficients are floats as well
    int idx = c_position;
    for (int i = 0; i < FIR_TAPS; i++) {
		idx = (idx == 0) ? (FIR_TAPS - 1) : idx - 1; // avoid modulus, pt.2
        result += c_buffer[idx] * filt_coeffs[i];
    }
    return result;
}

static void BuzzerInit() // keep in DoReset()!
{
	// init the coefficients for an "ox8"x oversampling windowed-sinc filter
	generate_filter_coeffs(filt_coeffs, WINDOW_NONE, FIR_TAPS, nBurnSoundRate/4, nBurnSoundRate*ox8);
	// init our filter's ring-buffer
    memset(c_buffer, 0, sizeof(c_buffer));
    c_position = 0;

	buzzer_data_frame_second = (SpecCylesPerScanline * SpecScanlines * 50.00);
	buzzer_data_frame = SpecCylesPerScanline * SpecScanlines;

//	bprintf(0, _T("buzzer_data_frame & _second:  %d  %d\n"), buzzer_data_frame, buzzer_data_frame_second);

	buzzer_d8_fract = (INT64)((INT64)buzzer_data_frame_second * (1 << 16)) / (nBurnSoundRate * ox8);
	buzzer_d8_accu = 0;
}

static void BuzzerExit()
{
}

static void BuzzerAdd(INT16 data)
{
	const INT16 bips[4] = { 0, 0x2000, 0x3000, 0x3000 }; // !, tape, buzzer, tape+buzzer
	data = bips[data & 3];

	if (data != buzzer_last_data && pBurnSoundOut) { // "&& pBurnSoundOut": fix runahead w/o having to scan huge buffer
		INT32 len = ZetTotalCycles() - buzzer_last_update;
		if (len > 0 && ((buzzer_data_len + len) < (buzzer_data_frame + 100)))
		{
			for (INT32 i = buzzer_data_len; i < buzzer_data_len+len; i++) {
				Buzzer[i] = buzzer_last_data;
			}
			buzzer_data_len += len;
		}

		buzzer_last_data = data;
		buzzer_last_update = ZetTotalCycles();
	}
}

static void BuzzerRender(INT16 *dest)
{
	bool wentover = false;

	if (buzzer_data_len == 0) return;

	// Step 0: if we don't have enough data, fill it up.  If we have too much:
	// note it for later..
	if (buzzer_data_len < buzzer_data_frame) {
		for (INT32 i = buzzer_data_len; i < buzzer_data_frame; i++) {
			Buzzer[i] = buzzer_last_data;
		}
		buzzer_data_len = buzzer_data_frame;
	}
	else if (buzzer_data_len > buzzer_data_frame) {
		// we went over!
		wentover = true;
	}

	// Step 1: convert (downsample) from approx 3500000hz to 48000*"ox8" hz (or native rate * "ox8")
	int sams = 0;

	INT32 totalsam = 0;
	INT32 totalsams = 0;
	INT32 buzzerpos = 0;
    for (int i = 0; i < nBurnSoundLen*ox8; i++, buzzer_d8_accu+=buzzer_d8_fract) {
		while (buzzer_d8_accu >= (1 << 16) ) {
			totalsam += Buzzer[buzzerpos++];
			totalsams++;
			buzzer_d8_accu -= (1 << 16);
		}
		BuzzerD8[sams++] = totalsam / ((totalsams > 0) ? totalsams : 1);

		totalsam = 0;
		totalsams = 0;
	}
	//bprintf(0, _T("total sams  %d, buzzer sams/frame  %d  %d, nburnsoundlen*8  %d\n"), sams, buzzerpos, buzzer_data_frame, nBurnSoundLen*8);

	// Step 0.a: if we ran too many cycles in this frame, save them for the next!
	// Note: has to be after Step 1!
	if (wentover) {
		int sams_ov = buzzer_data_len - buzzer_data_frame;
		int cycs_ov = ZetTotalCycles() - buzzer_data_frame;
		//bprintf(0, _T("went over cycs/sams:  %d  %d\n"), cycs_ov, sams_ov);

		memcpy(&Buzzer[0], &Buzzer[buzzer_data_frame], sams_ov * sizeof(INT16));
		buzzer_data_len = sams_ov;
		buzzer_last_update = cycs_ov;
	} else {
		buzzer_data_len = 0;
		buzzer_last_update = 0;
	}

	INT32 foopos = 0;

	// Step 2&3:
	// With the "ox8"x stream, filter it to remove aliasing, downsample to native rate,
	// run it through a dc blocking filter then render it to our sound buffer.
	for (INT32 i = 0; i < nBurnSoundLen; i++) {
		int sample = 0;
		for (INT32 j = 0; j < ox8; j++) {
#if TAPE_TO_WAV
			sample += BuzzerD8[foopos++];
#else
			sample += update_filter(BuzzerD8[foopos++]);
#endif
		}
		sample /= ox8;

		// dc block
		INT16 out = sample - dac_lastin + 0.999 * dac_lastout;
		dac_lastin = sample, dac_lastout = out;

		// add to stream (+include ay if Spec128)
		if (SpecMode & SPEC_AY8910) {
			dest[0] = BURN_SND_CLIP(dest[0] + out);
			dest[1] = BURN_SND_CLIP(dest[1] + out);
			dest += 2;
		} else {
			dest[0] = BURN_SND_CLIP(out);
			dest[1] = BURN_SND_CLIP(out);
			dest += 2;
		}
	}
}

// end Oversampling Buzzer-DAC

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	SpecZ80Rom              = Next; Next += 0x04000 * 4;
	SpecSnapshotData        = Next; Next += 0x20000;
	SpecTAP                 = Next; Next += 0x800000;

	RamStart                = Next;
	SpecZ80Ram              = Next; Next += 0x20000;
	RamEnd                  = Next;

	SpecPalette             = (UINT32*)Next; Next += 0x00010 * sizeof(UINT32);

	Buzzer                  = (INT16*)Next; Next += 0x20000 * sizeof(INT16);
	BuzzerD8                = (INT16*)Next; Next += 0x20000 * sizeof(INT16);

	MemEnd                  = Next;

	return 0;
}

static INT32 SpecDoReset()
{
	ZetOpen(0);
	ZetReset();
	if (SpecMode & SPEC_AY8910) {
		AY8910Reset(0);
	}
	ZetClose();

	BuzzerInit();

	SpecVideoRam = SpecZ80Ram;
	Spec128kMapper = 0;
	Spec128kMapper2 = 0;
	ula_border = 0;

	if (SpecMode & SPEC_128K) {
		ZetOpen(0);
		spectrum128_bank();
		ZetClose();
	}

	if (SpecMode & SPEC_Z80) {
		spectrum_loadz80();
	}

	if (SpecMode & SPEC_TAP) {
		SpecLoadTAP(); // reset robot
		SpecTAPReset(); // reset tap engine
	}

	dac_lastin = 0;
	dac_lastout = 0;

	ula_last_cyc = 0;
	ula_byte = 0xff;
	ula_attr = 0x00;
	ula_scr = 0x00;

	nExtraCycles = 0;

	return 0;
}

static UINT8 __fastcall SpecZ80Read(UINT16 address)
{
	if (address < 0x4000) {
		return SpecZ80Rom[address];
	}

	if (address >= 0x4000 && address <= 0x7fff) {
		return SpecZ80Ram[address & 0x3fff];
	}

	if (address >= 0x8000 && address <= 0xffff) {
		return SpecZ80Ram[0x4000 + (address & 0x7fff)];
	}

	bprintf(0, _T("mr %x\n"), address);

	return 0;
}

static void __fastcall SpecZ80Write(UINT16 address, UINT8 data)
{
	if (address < 0x4000) return;

	if (address >= 0x4000 && address <= 0x7fff) {
		SpecZ80Ram[address & 0x3fff] = data;
		return;
	}

	if (address >= 0x8000 && address <= 0xffff) {
		SpecZ80Ram[0x4000 + (address & 0x7fff)] = data;
		return;
	}

	bprintf(0, _T("mw %x %x\n"), address, data);
}

// the 128K has a secondary kbd matrix that added more keys to the keyboard
// which were normally accessed by pressing Caps Shift / Symbol Shift.
// When one of those keys are pressed - it basically uses a little hw hack
// to hold the caps/symbol shift button. -dink
static INT32 check_caps_shift()
{
	INT32 ret = 0;
	for (INT32 i = 11; i <= 13; i++) {
		// check if a 2ndary matrix button that needs CAPS Shift has been pressed
		if (SpecInput[i] != 0x1f) ret = 1;
	}
	return ret;
}

static INT32 check_symbol_shift()
{
	INT32 ret = 0;
	for (INT32 i = 14; i <= 15; i++) {
		// check if a 2ndary matrix button that needs Symbol Shift has been pressed
		if (SpecInput[i] != 0x1f) ret = 1;
	}
	return ret;
}

static UINT8 read_keyboard(UINT16 address)
{
	UINT8 keytmp = 0x1f;

	for (INT32 i = 0; i < 8; i++) { // process all kbd rows
		if (~address & (1 << (i + 8))) {
			switch (i) {
				case 0:
					keytmp &= SpecInput[i];
					if (check_caps_shift()) keytmp &= ~(1 << 0);
					break;

				case 3:
					keytmp &= SpecInput[i] & SpecInput[11] & SpecInput[10]; // caps shift0, intf2 joy2
					break;

				case 4:
					keytmp &= SpecInput[i] & SpecInput[12] & SpecInput[9]; // caps shift1, intf2 joy1
					break;

				case 5:
					keytmp &= SpecInput[i] & SpecInput[14]; // symbol shift0
					break;

				case 7:
					keytmp &= SpecInput[i] & SpecInput[13] & SpecInput[15]; // caps shift2, symbol shift1
					if (check_symbol_shift()) keytmp &= ~(1 << 1);
					break;

				default:
					keytmp &= SpecInput[i];
					break;
			}
		}
	}

	if (SpecMode & SPEC_128K || SpecDips[0] & 0x80) {	// 128K or Issue 3
		keytmp |= (ula_border & 0x10) ? 0xe0 : 0xa0;
	} else {											// Issue 2
		keytmp |= (ula_border & 0x18) ? 0xe0 : 0xa0;
	}

	return keytmp;
}

static UINT8 get_pulse(bool from_ula);
static UINT8 last_pulse;
static UINT8 __fastcall SpecZ80PortRead(UINT16 address)
{
	if (~address & 0x0001) { // keyboard
		return (read_keyboard(address) & ~0x40) | (get_pulse(1) & 0x40);
	}

	if ((address & 0x1f) == 0x1f && (address & 0x20) == 0) {
		// Kempston only pulls A5 low - Kempston RE: https://www.youtube.com/watch?v=4e1MlxPVyD4
		return SpecInput[8]; // kempston (returns 0xff when disabled)
	}

	if ((address & 0xc002) == 0xc000 && (SpecMode & SPEC_AY8910)) {
		return AY8910Read(0);
	}

	update_ula(ZetTotalCycles()); // get up-to-date ula_byte!

	return ula_byte; // Floating Memory
}

static void __fastcall SpecZ80PortWrite(UINT16 address, UINT8 data)
{
	if (~address & 0x0001) {
#if !TAPE_TO_WAV
		if (in_tape_ffwd == 0) BuzzerAdd(((data & 0x10) >> 3) | ((SpecDips[1] & 2) ? last_pulse : 0));
#endif
		ula_border = data;
		return;
	}

	if (SpecMode & SPEC_AY8910) {
		switch (address & 0xc002) {
			case 0x8000: AY8910Write(0, 1, data); return;
			case 0xc000: AY8910Write(0, 0, data); return;
		}
	}

	if ((address & 0xff) == 0xfd) return; // Ignore (Jetpac writes here due to a bug in the game code, and it's the reason it won't work on 128k)

	//bprintf(0, _T("pw %x %x\n"), address, data);
}

static void spectrum128_bank()
{
	SpecVideoRam = SpecZ80Ram + ((5 + ((Spec128kMapper & 0x08) >> 2)) << 14);

	SpecRamPage = Spec128kMapper & 0x07;
	SpecRomPage = ((Spec128kMapper & 0x10) >> 4) * 0x4000;

	if (SpecMode & SPEC_PLUS2) {
		SpecRomPage += ((Spec128kMapper2 & 4) >> 1) * 0x4000;

		if (Spec128kMapper2 & 0x01) { // +2a/+3 "Special" paging mode
			const INT32 special_map[4][4] = { { 0, 1, 2, 3 },  { 4, 5, 6, 7 },  { 4, 5, 6, 3 },  { 4, 7, 6, 3 } };
			const INT32 special_sel = (Spec128kMapper2 >> 1) & 3;

			ZetMapMemory(SpecZ80Ram + (special_map[special_sel][0] << 14), 0x0000, 0x3fff, MAP_RAM);
			ZetMapMemory(SpecZ80Ram + (special_map[special_sel][1] << 14), 0x4000, 0x7fff, MAP_RAM);
			ZetMapMemory(SpecZ80Ram + (special_map[special_sel][2] << 14), 0x8000, 0xbfff, MAP_RAM);
			ZetMapMemory(SpecZ80Ram + (special_map[special_sel][3] << 14), 0xc000, 0xffff, MAP_RAM);
		} else {
			ZetUnmapMemory(0x0000, 0xffff, MAP_RAM);
		}
	}

	Z80Contention_set_bank(SpecRamPage);
}

static UINT8 __fastcall SpecSpec128Z80Read(UINT16 address)
{
	if (address < 0x4000) {
		return SpecZ80Rom[SpecRomPage + address];
	}

	if (address >= 0x4000 && address <= 0x7fff) {
		return SpecZ80Ram[(5 << 14) + (address & 0x3fff)];
	}

	if (address >= 0x8000 && address <= 0xbfff) {
		return SpecZ80Ram[(2 << 14) + (address & 0x3fff)];
	}

	if (address >= 0xc000 && address <= 0xffff) {
		return SpecZ80Ram[(SpecRamPage << 14) + (address & 0x3fff)];
	}

	bprintf(0, _T("mr %x\n"), address);

	return 0;
}

static void __fastcall SpecSpec128Z80Write(UINT16 address, UINT8 data)
{
	if (address < 0x4000) return; // ROM

	if (address >= 0x4000 && address <= 0x7fff) {
		SpecZ80Ram[(5 << 14) + (address & 0x3fff)] = data;
		return;
	}

	if (address >= 0x8000 && address <= 0xbfff) {
		SpecZ80Ram[(2 << 14) + (address & 0x3fff)] = data;
		return;
	}

	if (address >= 0xc000 && address <= 0xffff) {
		SpecZ80Ram[(SpecRamPage << 14) + (address & 0x3fff)] = data;
		return;
	}

	bprintf(0, _T("mw %x %x\n"), address, data);
}

static UINT8 __fastcall SpecSpec128Z80PortRead(UINT16 address)
{
	if (~address & 0x0001) { // keyboard
		return (read_keyboard(address) & ~0x40) | (get_pulse(1) & 0x40);
	}

	if ((address & 0x1f) == 0x1f && (address & 0x20) == 0) {
		// Kempston only pulls A5 low - Kempston RE: https://www.youtube.com/watch?v=4e1MlxPVyD4
		return SpecInput[8]; // kempston (returns 0xff when disabled)
	}

	if ((address & 0xc002) == 0xc000 && (SpecMode & SPEC_AY8910)) {
		return AY8910Read(0);
	}

	if ((address & 0x8002) == 0x0000) {
		// todo: figure out what 0x7ffd / 3ffd read does
		//bprintf(0, _T("reading %x (%x)\n"), address, Spec128kMapper);
	}

	update_ula(ZetTotalCycles()); // get up-to-date ula_byte!

	return ula_byte; // Floating Memory
}

static void __fastcall SpecSpec128Z80PortWrite(UINT16 address, UINT8 data)
{
	if (~address & 0x0001) {
#if !TAPE_TO_WAV
		if (in_tape_ffwd == 0) BuzzerAdd(((data & 0x10) >> 3) | ((SpecDips[1] & 2) ? last_pulse : 0));
#endif
		ula_border = data;
		// needs to fall through!!
	}

	if (SpecMode & SPEC_PLUS2) { // 128k +2a / +3
		if ((address & 0xc002) == 0x4000) { // 7ffd
			if (Spec128kMapper & 0x20) return; // memory lock-out latch

			if ((data ^ Spec128kMapper) & 0x08) {
				// update video before video page swap
				update_ula(ZetTotalCycles());
			}

			Spec128kMapper = data;
			//bprintf(0, _T("7ffd: %x\t%x\n"), address, data);
			spectrum128_bank();
			return;
		}

		if ((address & 0xf002) == 0x1000) { // 1ffd
			if (Spec128kMapper & 0x20) return; // memory lock-out latch

			//bprintf(0, _T("1ffd: %x\t%x\n"), address, data);
			Spec128kMapper2 = data;
			spectrum128_bank();
			return;
		}
	} else { // standard 128k
		if ((address & 0x8002) == 0x0000) { // 7ffd
			if (Spec128kMapper & 0x20) return; // memory lock-out latch

			if ((data ^ Spec128kMapper) & 0x08) {
				// update video before video page swap
				update_ula(ZetTotalCycles());
			}

			Spec128kMapper = data;
			//bprintf(0, _T("7ffd: %x\t%x\n"), address, data);
			spectrum128_bank();
			return;
		}
	}

	if (SpecMode & SPEC_AY8910) {
		switch (address & 0xc002) {
			case 0x8000: AY8910Write(0, 1, data); return;
			case 0xc000: AY8910Write(0, 0, data); return;
		}
	}

	if (address == 0xff3b || address == 0xbf3b) return; // ignore (some games check for "ula plus" here)

	//bprintf(0, _T("pw %x %x\n"), address, data);
}

// Spectrum TAP loader (c) 2020 dink
#define BLKNUM 0x800
static UINT8 *SpecTAPBlock[BLKNUM];
static INT32 SpecTAPBlockLen[BLKNUM];

static INT32 SpecTAPBlocks = 0; // 1-based
static INT32 SpecTAPBlocknum = 0; // 0-based
static INT32 SpecTAPPos = 0; // 0-based

enum {
	PULSE_KICKOFF = 0,
	PULSE_LEADER,
	PULSE_SEQ,
	PULSE_DATA,
	PULSE_RAWDATA,  // .wav, tzx "direct recording", ...
	PULSE_PAUSE,
	PULSE_DONE,
	PULSE_WAIT_EMIT
};

static const char *pulse_mdescr[] = {
	"PULSE_KICKOFF", "PULSE_LEADER", "PULSE_SEQ", "PULSE_DATA",
	"PULSE_RAWDATA", "PULSE_PAUSE", "PULSE_DONE", "PULSE_WAIT_EMIT"
};

static INT32 pulse_status = 0;

enum {
	TAPE_STOPPED = 0,
	TAPE_PLAYING
};

static INT32 pulse_mode = 0;
static INT32 pulse_length = 0;
static INT32 pulse_index = 0;
static UINT8 pulse_startup = 0;
static INT32 pulse_count = 0;
static INT32 pulse_pulse = 0;

static UINT64 super_tstate = 0;
static UINT64 start_tstate = 0;
static UINT64 target_tstate = 0;

static UINT64 total_tstates()
{
	return ZetTotalCycles() + super_tstate;
}

static INT32 load_check = 0;
static INT64 last_cycle = 0;
static INT32 last_bc = 0;

#define DEBUG_TAP 0
#define TAPERAW 0
#if TAPERAW
// Note:
//  TAPERAW is for debugging loader with a .wav tape stream, and nothing else
static UINT8 *rawtap;
static INT16 *rawtap16;
static UINT16 rawtap_channels;
static UINT16 rawtap_samplerate;
static const INT32 rawtap_size = 46106924;

static void taperaw_init()
{
	BurnFree(rawtap);
	rawtap = (UINT8*)BurnMalloc(rawtap_size+10);
	BurnDumpLoad("testioso.wav", rawtap, rawtap_size);
	rawtap16 = (INT16*)rawtap + 0x2c;
	rawtap_channels = *((UINT16*)&rawtap[0x16]);
	rawtap_samplerate = *((UINT16*)&rawtap[0x18]);
	bprintf(0, _T("rawtape channels/samplerate %d, %d\n"),rawtap_channels,rawtap_samplerate);
}

static INT32 taperaw_index()
{
	const INT32 total_cyc = SpecScanlines*SpecCylesPerScanline*50;

	INT64 t = total_tstates() - start_tstate;
	t = t*rawtap_samplerate / total_cyc;

	return t;
}

static UINT8 taperaw_pulse()
{
	const INT32 index = taperaw_index() * rawtap_channels + 0;
	if (index < ((rawtap_size - 0x2c) / 2)) {
		pulse_pulse = (rawtap16[index] > 1024) ? 0x40 : 0x00;
	last_pulse = (pulse_pulse >> 6) & 1;
	} else {
		pulse_status = TAPE_STOPPED;
		bprintf(0, _T("rawtape: finished loading.\n"));
	}
	return pulse_pulse;
}
#endif

// .tzx & SlowTAP zxtape handling consumers

static INT32 tap_pos = 0;
static INT32 tap_op = 0;
static INT32 tap_op_num = 0;

static inline UINT8 tapbyte() {
	return SpecTAP[tap_pos++];
}
static inline UINT16 tapword() {
	const UINT8 b = tapbyte();
	return b | tapbyte() << 8;
}
static inline UINT16 tapdword() {
	const UINT8 a = tapbyte();
	const UINT8 b = tapbyte();
	const UINT8 c = tapbyte();
	const UINT8 d = tapbyte();
	return a | b << 8 | c << 16 | d << 24;
}
static inline UINT8 *tapptr() {
	return &SpecTAP[tap_pos];
}
static inline UINT8 *tapptr(INT32 i) {
	return &SpecTAP[tap_pos + i];
}
static inline UINT8 tapdata(INT32 i) {
	return SpecTAP[tap_pos + i];
}

static UINT16 pause_len;
static UINT32 block_len;
static UINT32 block_len_pulses;
static UINT16 leader_pulse_len;

static UINT16 seq_pulses[0x100] = { 0, };
static UINT16 seq_pulsecount;

static UINT16 raw_t_per_sample;

static UINT16 zero_bit_len;
static UINT16 one_bit_len;
static UINT16 leader_pulse_count;
static UINT8 last8;
static INT32 loop_start = 0;
static INT32 loop_count = 0;
static bool got_emit = false;

static void SpecTZXReset()
{
	tap_pos = (SpecMode & SPEC_TZX) ? 10 : 0; // skip ZXTape!\x1a,ver,ver header
	tap_op = -1;
	tap_op_num = 0;

	loop_count = 0;
	loop_start = 0;

	got_emit = false;
}

static void emit_leader(INT32 pulselen, INT32 pulsecount) {
	pulse_mode = PULSE_LEADER;
	pulse_startup = 0;

	if (DEBUG_TAP) bprintf(0, _T("emit_leader: %d, %d\n"), pulselen, pulsecount);

	leader_pulse_len = pulselen;
	leader_pulse_count = pulsecount;
	got_emit = true;
}

static void emit_sync(INT32 pulses_count) {
	pulse_mode = PULSE_SEQ;
	pulse_startup = 0;

	seq_pulsecount = pulses_count;
	if (DEBUG_TAP) bprintf(0, _T("emit_sync: %d sync's\n"), pulses_count);
	for (int i = 0; i < pulses_count; i++) {
		if (DEBUG_TAP) bprintf(0, _T("sync[%x]:  %d\n"), i, seq_pulses[i]);
	}

	got_emit = true;
}

static void emit_block(INT32 blocklen, INT32 zerobitlen, INT32 onebitlen, INT32 lasteight) {
	pulse_mode = PULSE_DATA;
	pulse_startup = 0;

	if (DEBUG_TAP) bprintf(0, _T("emit_block: %x size, %d %d last8: %x\n"), blocklen, zerobitlen, onebitlen, lasteight);

	block_len = blocklen;
	zero_bit_len = zerobitlen;
	one_bit_len = onebitlen;
	last8 = lasteight;

	block_len_pulses = (block_len - 1) * 8 + last8;

	got_emit = true;
}

static void emit_rawblock(INT32 blocklen, INT32 tstatespersample, INT32 lasteight) {
	pulse_mode = PULSE_RAWDATA;
	pulse_startup = 0;

	if (DEBUG_TAP) bprintf(0, _T("emit_rawblock: %x size, %d t-per-sample, last8: %x\n"), blocklen, tstatespersample, lasteight);

	block_len = blocklen;
	raw_t_per_sample = tstatespersample;
	last8 = lasteight;

	block_len_pulses = (block_len - 1) * 8 + last8;

	got_emit = true;
}

static void emit_pause(INT32 pausems) {
	pulse_mode = PULSE_PAUSE;
	pulse_startup = 0;

	if (DEBUG_TAP) bprintf(0, _T("emit_pause: %d\n"), pausems);
	pause_len = pausems;

	got_emit = true;
}

static void emit_pause_or_stop(INT32 pausems) {
	pulse_mode = PULSE_PAUSE;
	pulse_startup = 0;

	if (DEBUG_TAP) bprintf(0, _T("emit_pause_or_stop: %d%S\n"), pausems, (pausems == 0) ? " (stop)" : "");
	pause_len = (pausems == 0) ? 0xffff : pausems;

	got_emit = true;
}

static void SpecTZXOperation()
{
	if (tap_op == -1) {
		tap_op = tapbyte();
		tap_op_num = 0;
		if (DEBUG_TAP) bprintf(0, _T("--- tzx op:  %x  @  %x ---\n"), tap_op, tap_pos);
	}

	switch (tap_op) {
		case 0x10: // normal speed data
			switch (tap_op_num) {
				case 0:
					pause_len = tapword();
					block_len = tapword();
					emit_leader(2168, (tapdata(0) & 0x80) ? 3223 : 8063);
					tap_op_num++;
					break;
				case 1:
					seq_pulses[0] = 667;
					seq_pulses[1] = 735;
					seq_pulsecount = 2;
					emit_sync(seq_pulsecount);
					tap_op_num++;
					break;
				case 2:
					emit_block(block_len, 855, 1710, 8);
					tap_op_num++;
					break;
				case 3:
					emit_pause(pause_len);
					tap_pos += block_len;
					tap_op = -1; // next!
					break;
			}
			break;
		case 0x11: // turbo data
			switch (tap_op_num) {
				case 0:
					leader_pulse_len = tapword();
					seq_pulses[0] = tapword();
					seq_pulses[1] = tapword();
					zero_bit_len = tapword();
					one_bit_len = tapword();
					leader_pulse_count = tapword();
					last8 = tapbyte();
					pause_len = tapword();
					block_len = tapword() | (tapbyte() << 16);
					emit_leader(leader_pulse_len, leader_pulse_count);
					tap_op_num++;
					break;
				case 1:
					seq_pulsecount = 2;
					emit_sync(seq_pulsecount);
					tap_op_num++;
					break;
				case 2:
					emit_block(block_len, zero_bit_len, one_bit_len, last8);
					tap_op_num++;
					break;
				case 3:
					emit_pause(pause_len);
					tap_pos += block_len;
					tap_op = -1;  // next!
					break;
			}
			break;

		case 0x12: // tone (leader)
			leader_pulse_len = tapword();
			leader_pulse_count = tapword();
			emit_leader(leader_pulse_len, leader_pulse_count);
			tap_op = -1;
			break;

		case 0x13: // pulse sequences
			seq_pulsecount = tapbyte();

			for (int i = 0; i < seq_pulsecount; i++) {
				seq_pulses[i] = tapword();
			}
			emit_sync(seq_pulsecount);
			tap_op = -1;
			break;

		case 0x14: // data block
			switch (tap_op_num) {
				case 0:
					zero_bit_len = tapword();
					one_bit_len = tapword();
					last8 = tapbyte();
					pause_len = tapword();
					block_len = tapword() | tapbyte() << 16;
					emit_block(block_len, zero_bit_len, one_bit_len, last8);
					tap_op_num++;
					break;
				case 1:
					emit_pause(pause_len);
					tap_pos += block_len;
					tap_op = -1;
					break;
			}
			break;

		case 0x15: // raw data block (Direct Recording)
			switch (tap_op_num) {
				case 0:
					raw_t_per_sample = tapword();
					pause_len = tapword();
					last8 = tapbyte();
					block_len = tapword() | tapbyte() << 16;

					emit_rawblock(block_len, raw_t_per_sample, last8);
					tap_op_num++;
					break;
				case 1:
					emit_pause(pause_len);
					tap_pos += block_len;
					tap_op = -1;
					break;
			}
			break;

		case 0x20: // pause or stop
			pause_len = tapword();
			emit_pause_or_stop(pause_len);
			tap_op = -1;
			break;

		case 0x21: { // skip over group name
			UINT8 skip = tapbyte();
			UINT8 *textdescr = (UINT8*)BurnMalloc(skip + 1);

			memcpy(textdescr, tapptr(), skip);
			bprintf(0, _T("Group Name: [%S]\n"), textdescr);
			BurnFree(textdescr);
			tap_pos += skip;

			tap_op = -1;
			break;
		}

		case 0x22: // end group name
			bprintf(0, _T("End Group\n"));
			tap_op = -1;
			break;

		case 0x24: // loop start
			loop_count = tapword();
			loop_start = tap_pos;
			tap_op = -1;
			break;

		case 0x25: // loop end
			if (loop_count > 0) {
				loop_count--;
				tap_pos = loop_start;
			}
			tap_op = -1;
			break;

		case 0x26: // call seq
			bprintf(0, _T(".txz: call seq unimpl!\n"));
			tap_op = -1;
			break;

		case 0x27: // call seq-RETURN
			bprintf(0, _T(".tzx: call seq-RETURN unimpl!\n"));
			tap_op = -1;
			break;

		case 0x28: // select block
			bprintf(0, _T(".tzx: select block unimpl!\n"));
			tap_op = -1;
			break;

		case 0x2a: // stop if 48k spectrum
			tapdword(); // eat the reserved dword
			if (~SpecMode & SPEC_128K) {
				if (DEBUG_TAP) bprintf(0, _T(".tzx: we should stop if 48k speccy\n"));
				pulse_mode = PULSE_DONE;
				// done!
			}
			tap_op = -1;
			break;

		case 0x2b: // set signal level
			tapdword(); // eat the reserved dword
			pulse_pulse = tapbyte() ? 0x40 : 0x00;
			if (DEBUG_TAP) bprintf(0, _T(".tzx 0x2b: set signal level: %x\n"), pulse_pulse);
			tap_op = -1;
			break;

		case 0x30: { // tzx description (text)
			UINT8 skip = tapbyte();
			UINT8 *textdescr = (UINT8*)BurnMalloc(skip + 1);

			memcpy(textdescr, tapptr(), skip);
			bprintf(0, _T("TZX Description: [%S]\n"), textdescr);
			BurnFree(textdescr);

			tap_pos += skip;
			tap_op = -1;
			break;
		}

		case 0x32: { // archive description
			const char *archivetypes[] = { "Title", "Publisher", "Author",
			"Year", "Language", "Type", "Price", "Loader", "Origin"	};
			char a_line[0x200] = "\0";
			char b_line[0x200] = "\0";

			bprintf(0, _T("TZX Archival Info\n"));

			UINT16 skip = tapword();
			int pos = 0;
			UINT8 entries = tapdata(pos++);
			for (int i = 0; i < entries; i++) {
				UINT8 entrytype = tapdata(pos++);
				if (entrytype < 9) {
					sprintf(a_line, "%s:", archivetypes[entrytype]);
				} else {
					sprintf(a_line, "Comment:");
				}
				UINT8 s_len = tapdata(pos++);
				strncpy(b_line, (char*)tapptr(pos), s_len);
				b_line[s_len] = '\0';
				bprintf(0, _T("%S %S\n"), a_line, b_line);
				pos += s_len;
				if (pos >= skip) break;
			}

			tap_pos += skip;
			tap_op = -1;
			break;
		}

		case 0x33: { // hardware description
			UINT16 skip = tapbyte() * 3;
			UINT8 *textdescr = (UINT8*)BurnMalloc(skip + 1);

			memcpy(textdescr, tapptr(), skip);
			bprintf(0, _T("TZX HW Description: [%S]\n"), textdescr);
			BurnFree(textdescr);

			tap_pos += skip;
			tap_op = -1;
			break;
		}

		case 0x35: { // custom info block
			tap_pos += 10; // skip char[10] ascii string
			UINT16 skip = tapdword();
			tap_pos += skip;
			tap_op = -1;
			break;
		}

		case 0x5a: // glue block, to detect spliced tzx files
			tap_pos += 9;
			tap_op = -1;
			break;

		default:
			bprintf(0, _T(".tzx: UNIMPL operation %x\n"), tap_op);
			pulse_mode = PULSE_DONE;
			tap_op = -1;
			break;
	}
}

static void SpecTZXGetEmit() {
	got_emit = false;

	if (tap_pos == 10) {
		// start playback!
		pulse_pulse = 0;
		start_tstate = total_tstates();
		target_tstate = total_tstates();
	}

	if (tap_pos >= SpecTAPLen) {
		bprintf(0, _T(".tzx: end of tape reached!\n"));
		pulse_mode = PULSE_DONE;
	} else {
		do {
			SpecTZXOperation();
		} while (got_emit == false && tap_pos < SpecTAPLen);
	}
}

static void pulse_reset()
{
#if TAPERAW
	taperaw_init();
#endif
	SpecTZXReset();

	pulse_mode = PULSE_DONE;
	pulse_status = TAPE_STOPPED;
	pulse_length = 0;
	pulse_index = 0;
	pulse_startup = 0;
	pulse_count = 0;
	pulse_pulse = 0;

	start_tstate = 0;
	target_tstate = 0;

	load_check = 0;
	last_cycle = 0;
	last_bc = 0;

	if (SpecMode & SPEC_TZX || SpecMode & SPEC_SLOWTAP) {
		pulse_mode = PULSE_WAIT_EMIT;

#if TAPE_TO_WAV
		bprintf(0, _T("DUMPING TAPE TO zxout.pcm.\n"));
		tape_to_wav_startup_pause = 20;

		if (tape_dump_fp) {
			fclose(tape_dump_fp);
			tape_dump_fp = NULL;
		}
		tape_dump_fp = fopen("zxout.pcm", "wb+");
		if (!tape_dump_fp) bprintf(0, _T("Can't open zxout.pcm for writing!\n"));
#endif
	}
}

static void pulse_scan()
{
	SCAN_VAR(pulse_status);
	SCAN_VAR(pulse_mode);
	SCAN_VAR(pulse_length);
	SCAN_VAR(pulse_index);
	SCAN_VAR(pulse_startup);
	SCAN_VAR(pulse_count);
	SCAN_VAR(pulse_pulse);

	SCAN_VAR(super_tstate);
	SCAN_VAR(start_tstate);
	SCAN_VAR(target_tstate);

	SCAN_VAR(load_check);
	SCAN_VAR(last_cycle);
	SCAN_VAR(last_bc);

	SCAN_VAR(tap_pos);
	SCAN_VAR(tap_op);
	SCAN_VAR(tap_op_num);
	SCAN_VAR(pause_len);
	SCAN_VAR(block_len);
	SCAN_VAR(block_len_pulses);
	SCAN_VAR(leader_pulse_len);

	SCAN_VAR(seq_pulses);
	SCAN_VAR(seq_pulsecount);

	SCAN_VAR(raw_t_per_sample);

	SCAN_VAR(zero_bit_len);
	SCAN_VAR(one_bit_len);
	SCAN_VAR(leader_pulse_count);
	SCAN_VAR(last8);
	SCAN_VAR(loop_start);
	SCAN_VAR(loop_count);
	SCAN_VAR(got_emit);
}

#define d_abs(z) (((z) < 0) ? -(z) : (z))

static INT32 check_loading()
{
#if TAPE_TO_WAV
	return 0;
#endif
	INT32 cycles = total_tstates() - last_cycle;
	INT32 bc = d_abs(last_bc - ZetBc(-1)) & 0xff00;
	last_bc = ZetBc(-1) & 0xff00;
	last_cycle = total_tstates();

	if (pulse_status == TAPE_STOPPED) {
		load_check = (cycles < 600 && (bc == 0x100 || bc == 0xfe00|| bc == 0xff00 || bc == 0xfd00 )) ? load_check+1 : 0;
		if (load_check > 0x1000) {
			bprintf(0, _T("Tape: auto hit play\n"));
			pulse_status = TAPE_PLAYING;
			load_check = 0;

			// rawtap hack
			start_tstate = total_tstates();
			if (SpecMode & SPEC_SLOWTAP) {
				tap_op = -1;
			}
		}
	} else if (pulse_mode != PULSE_PAUSE) { // don't auto stop when in a pause block (yiearkungfu)
		load_check = (cycles > 1000 && (bc != 0x100 && bc != 0x0000 && bc!=0x6200 && (bc & 0xfc00) != 0xfc00)) ? load_check+1 : 0;
		if (load_check > 2) {
			bprintf(0, _T("Tape: auto hit STOP\n"));
		    pulse_status = TAPE_STOPPED;
			load_check = 0;
		}
	}
#if 0
	// debug detector
	extern int counter;
	if (counter || load_check > 0)
		bprintf(0, _T(">lc %x  bc %x,%x cyc %d\n"), load_check, ZetBc(-1),bc,cycles);
#endif
	return 0;
}

static void SpecSlowTAPGetEmit() {
	got_emit = false;

	if (tap_pos == 0) {
		// start playback!
		pulse_pulse = 0;
		start_tstate = total_tstates();
		target_tstate = total_tstates();
		tap_op_num = 0;
		tap_op = 0;
	}

	if (tap_pos >= SpecTAPLen) {
		bprintf(0, _T(".tap: end of .tap reached!\n"));
		pulse_mode = PULSE_DONE;
	} else {
		switch (tap_op_num) {
			case 0:
				block_len = tapword();
				pause_len = 1000;
				emit_leader(2168, (tapdata(0) & 0x80) ? 3223 : 8063);
				tap_op_num++;
				break;
			case 1:
				seq_pulses[0] = 667;
				seq_pulses[1] = 735;
				seq_pulsecount = 2;
				emit_sync(seq_pulsecount);
				tap_op_num++;
				break;
			case 2:
				emit_block(block_len, 855, 1710, 8);
				tap_op_num++;
				break;
			case 3:
				emit_pause(pause_len);
				tap_pos += block_len;
				tap_op_num = 0;
				break;
		}
	}
}

static UINT8 pulse_synth()
{
	if (pulse_mode == PULSE_WAIT_EMIT) {
		if (SpecMode & SPEC_TZX) {
			SpecTZXGetEmit();
		} else {
			SpecSlowTAPGetEmit();
		}
		if (DEBUG_TAP) bprintf(0, _T("Got emit.  pulse_mode %x / %S\n"), pulse_mode, pulse_mdescr[pulse_mode]);
	}

	switch (pulse_mode) {
		case PULSE_LEADER:
			if (pulse_startup == 0) {
				pulse_startup = 1;
				pulse_index = 0;
				pulse_count = 0;
				pulse_pulse = 0;
				pulse_length = leader_pulse_len;
				if (DEBUG_TAP) bprintf(0, _T("pulse LEADER:  %I64x\n"), start_tstate);
			}
			if (total_tstates() >= target_tstate) {
				start_tstate = total_tstates();
				target_tstate += pulse_length;
				pulse_pulse ^= 0x40;
				pulse_count++;
				if (pulse_count >= leader_pulse_count) {
					pulse_mode = PULSE_WAIT_EMIT;
					pulse_startup = 0;
					if (DEBUG_TAP) bprintf(0, _T("..leader END. next @ %I64x  time now: %I64x   (difference %I64x)\n"), target_tstate, total_tstates(), target_tstate - total_tstates());
				}
			}
			break;

		case PULSE_SEQ:
			if (pulse_startup == 0) {
				pulse_startup = 1;
				pulse_index = 0;
			}
			if (total_tstates() >= target_tstate) {
				if (DEBUG_TAP) bprintf(0, _T("..SEQ hits @ %I64x  time now: %I64x   (difference %I64x)\n"), target_tstate, total_tstates(), total_tstates() - target_tstate);
				pulse_length = seq_pulses[pulse_index];
				pulse_index++;
				pulse_pulse ^= 0x40;
				target_tstate += pulse_length;
				if (pulse_index == seq_pulsecount) {
					pulse_length = 0;
					pulse_mode = PULSE_WAIT_EMIT;
					pulse_count = 0;
					pulse_index = 0;
					pulse_startup = 0;
					if (DEBUG_TAP) bprintf(0, _T("seq ends\n"));
				}
			}
			break;
		case PULSE_DATA:
			if (pulse_startup == 0) {
				pulse_startup = 1;

				pulse_index = 0;
				pulse_length = 0;
				pulse_count = 0;
			}
			if (total_tstates() >= target_tstate) {
				UINT8 data = tapdata(pulse_count >> 3);

				data &= (1 << (7 - (pulse_count & 7)));
				//bprintf(0, _T("byte %x-%d:  %x\n"), pulse_count>>3, pulse_count&7, data ? 1 : 0);
				pulse_length = ((data) ? one_bit_len : zero_bit_len);

				target_tstate += pulse_length;
				pulse_pulse ^= 0x40;

				pulse_index++;
				if (pulse_index == 2) {
					pulse_index = 0;
					pulse_count++;

					if (pulse_count == block_len_pulses) {
						pulse_mode = PULSE_WAIT_EMIT;
						pulse_startup = 0;
						if (DEBUG_TAP) bprintf(0, _T("block-EOF HIT.\n"));
					}
				}
			}
			break;
		case PULSE_RAWDATA:
			if (pulse_startup == 0) {
				pulse_startup = 1;

				pulse_index = 0;
				pulse_length = 0;
				pulse_count = 0;
			}
			if (total_tstates() >= target_tstate) {
				UINT8 data = tapdata(pulse_count >> 3);

				data &= (1 << (7 - (pulse_count & 7)));
				//bprintf(0, _T("byte %x-%d:  %x\n"), pulse_count>>3, pulse_count&7, data ? 1 : 0);

				pulse_pulse = ((data) ? 0x40 : 0x00);
				pulse_length = raw_t_per_sample;
				target_tstate += pulse_length;

				pulse_count++;

				if (pulse_count == block_len_pulses) {
					pulse_mode = PULSE_WAIT_EMIT;
					pulse_startup = 0;
					if (DEBUG_TAP) bprintf(0, _T("rawdata_block-EOF HIT.\n"));
				}
			}
			break;
		case PULSE_PAUSE:
			if (pulse_startup == 0) {
				pulse_startup = 1;
				pulse_index = 0;
			}
			start_tstate = total_tstates(); // keep synchronized for stopping during pause.
			if (total_tstates() >= target_tstate) {
				if (DEBUG_TAP) bprintf(0, _T("pause index %x  @  %I64x\n"), pulse_index, total_tstates());
				if (pulse_index == 0) {
					if (pause_len == 0xffff) {
						if (DEBUG_TAP) bprintf(0, _T("pause-stop mode (0), tape stopped!\n"));
						pulse_mode = PULSE_DONE;
						pause_len = 0;
					}
					target_tstate += pause_len * 3500;
					if (pause_len != 0) pulse_pulse ^= 0x40; // 0-length pauses don't get an edge
					//bprintf(0, _T("tape position in pause: %x  total len: %x\n"), tap_pos, SpecTAPLen);
					if (tap_pos == SpecTAPLen && pause_len != 0) {
						// end of the road, no need to wait!
						pulse_status = TAPE_STOPPED;
						target_tstate = total_tstates(); // break out of catch-up loop, just incase
						pulse_mode = PULSE_WAIT_EMIT;
						pulse_index = 0;
					}
				}
				pulse_index++;
				if (pulse_index == 2) {
					pulse_mode = PULSE_WAIT_EMIT;
					pulse_index = 0;
					if (DEBUG_TAP) bprintf(0, _T("done PAUSE.\n"));
				}
			}
			break;

		case PULSE_DONE:
			if (DEBUG_TAP) bprintf(0, _T("pulse_DONE hits.\n"));
			pulse_status = TAPE_STOPPED;
			pulse_pulse ^= 0x40; // end of tape edge
			target_tstate = total_tstates(); // break out of catch-up loop, just incase
#if TAPE_TO_WAV
			bprintf(0, _T("zxout.pcm: Tape end reached.\n"));
#endif
			break;
	}

	last_pulse = (pulse_pulse >> 6) & 1;

	return pulse_pulse;
}

static UINT8 get_pulse(bool from_ula)
{
	if (!(SpecMode & SPEC_SLOWTAP || SpecMode & SPEC_TZX)) return 0;

	if (from_ula) check_loading();

	if (pulse_status == TAPE_STOPPED) return pulse_pulse;

#if TAPERAW
	return taperaw_pulse();
#endif

	// Sometimes the ULA stops reading, for example:
	// During leader - it only checks every couple frames until it gets the
	// steady leader pulse.
	// Protection Check - 1942 stops (reading ula) when there are still 8 bits
	// left in a block, then starts reading shortly after @ the next leader
	// This loop will get us to where we need to be.
	while (total_tstates() >= target_tstate && pulse_status != TAPE_STOPPED) {
		pulse_synth();
	}

	return pulse_pulse;
}


static void SpecTAPReset()
{
	SpecTAPBlocknum = 0;
	SpecTAPPos = 0;

	pulse_reset();
}

static void SpecTAPInit()
{
	for (INT32 i = 0; i < BLKNUM; i++) {
		SpecTAPBlock[i] = NULL;
		SpecTAPBlockLen[i] = 0;
	}
	SpecTAPBlocks = 0;
	SpecTAPBlocknum = 0;

	if (SpecMode & SPEC_TZX) {
		bprintf(0, _T("**  - TZX Loader\n"));
		return;
	}

	if (DEBUG_TAP) {
		bprintf(0, _T("**  - Spectrum TAP Loader -\n"));
		bprintf(0, _T("Block#\tLength\tOffset\n"));
	}
	for (INT32 i = 0; i < SpecTAPLen;) {
		INT32 block_size = SpecTAP[i+0] | (SpecTAP[i+1] << 8);

		if (block_size) {
			if (DEBUG_TAP) {
				bprintf(0, _T("%x\t%d\t%x\n"), SpecTAPBlocks, block_size, i+2);
			}

			SpecTAPBlock[SpecTAPBlocks] = &SpecTAP[i+2];
			SpecTAPBlockLen[SpecTAPBlocks] = block_size-2;
			SpecTAPBlocks++;
			if (SpecTAPBlocks >= BLKNUM) {
				bprintf(PRINT_ERROR, _T(".TAP Loader: Tape blocks exceeded.\n"));
				break;
			}
		}

		i += block_size + 2;
	}
}

static INT32 SpecTAPDMACallback()
{
	if (~SpecMode & SPEC_TAP || SpecTAPBlocks == 0) return 0;

	UINT8 *data = SpecTAPBlock[SpecTAPBlocknum];

	INT32 transfer_ok = 0;
	INT32 carry_val = 0;
	INT32 checksum = 0;
	INT32 offset = 0;
	UINT8 byte = 0;

	ActiveZ80EXAF();
	INT32 tap_block = data[0];
	INT32 cpu_block = ActiveZ80GetAF() >> 8;
	INT32 address = ActiveZ80GetIX();
	INT32 length = ActiveZ80GetDE();
	INT32 length_unadjusted = length;

	// If anything is changed here, try the following testcases:
	// Chickin Chase, Alter Ego, V

	if (DEBUG_TAP) {
		bprintf(0, _T("TAP blocknum %d\n"), SpecTAPBlocknum);
		bprintf(0, _T("TAP blocklen %d\n"), SpecTAPBlockLen[SpecTAPBlocknum]);
		bprintf(0, _T("TAP blocktype %x\n"), tap_block);
		bprintf(0, _T("CPU blocktype %x\n"), cpu_block);
		bprintf(0, _T("CPU address %x\n"), address);
		bprintf(0, _T("CPU length %x\n"), length);
	}
	if (length > SpecTAPBlockLen[SpecTAPBlocknum]) {
		bprintf(0, _T("CPU Requested length %x > tape block length %x, adjusting.\n"), length, SpecTAPBlockLen[SpecTAPBlocknum]);
		length = SpecTAPBlockLen[SpecTAPBlocknum];
	}
	if (cpu_block == tap_block) { // we found our block! :)
		if (ActiveZ80GetCarry()) {
			if (DEBUG_TAP) {
				bprintf(0, _T("loading data\n"));
			}
			// load
			offset = 0;
			checksum = tap_block;
			while (offset < length) {
				if (offset+1 > SpecTAPBlockLen[SpecTAPBlocknum]) {
					bprintf(0, _T(".TAP Loader: trying to read past block.  offset %x  blocklen %x\n"), offset, SpecTAPBlockLen[SpecTAPBlocknum]);
					carry_val = 0;
					break;
				}
				byte = data[offset+1];
				ZetWriteByte((address + offset) & 0xffff, data[offset+1]);
				checksum ^= data[offset+1];
				offset++;
			}
			if (DEBUG_TAP) {
				bprintf(0, _T("end dma, checksum %x  tap checksum %x\n"), checksum, data[offset+1]);
			}
			carry_val = (checksum == data[offset+1]);
			transfer_ok = 1;
		}
	}

	ActiveZ80SetCarry(carry_val);
	ActiveZ80SetIX((address + offset) & 0xffff);
	if (transfer_ok) ActiveZ80SetDE(length_unadjusted - SpecTAPBlockLen[SpecTAPBlocknum]);
	ActiveZ80SetHL((checksum << 8) | byte);
	ActiveZ80SetA(0);
	ActiveZ80SetPC(0x05e2);

	SpecTAPBlocknum = (SpecTAPBlocknum + 1) % SpecTAPBlocks;

	return 0;
}

static void snapshot_write_ram(UINT32 address, UINT8 data)
{
	if (address >= 0x4000 && address < 0x24000) { // ignore writes to bios area (snapshot)
		SpecZ80Ram[address - 0x4000] = data;
	} else {
		bprintf(PRINT_ERROR, _T(".z80, snapshot_write_ram(%x, %x).\n"));
	}
}

static void z80_rle_decompress(UINT8 *source, UINT32 dest, UINT16 size)
{
	while (size > 0) {
		if (size > 2 && source[0] == 0xed && source[1] == 0xed) {
			UINT8 count = source[2];
			UINT8 data = source[3];

			if (count == 0)	{
				bprintf(PRINT_ERROR, _T(".z80 rle_decompress: zero length rle-block? eek. (bad .z80 file)\n"));
				return;
			}

			if (count > size) {
				bprintf(PRINT_ERROR, _T(".z80 rle_decompress: count > size, eek. (bad .z80 file)\n"));
				count = size;
			}

			for (INT32 i = 0; i < count; i++) {
				snapshot_write_ram(dest, data);
				dest++;
			}

			source += 4;
			size -= count;
		} else {
			snapshot_write_ram(dest, source[0]);

			dest++;
			source++;
			size--;
		}
	}
}

static UINT16 mem2uint16(INT32 posi, INT32 bigendian)
{
	UINT8 *umem = (UINT8*)&SpecSnapshotData[posi];

	if (bigendian) {
		return (umem[0] << 8) + umem[1];
	} else {
		return (umem[1] << 8) + umem[0];
	}
}

static UINT32 page_to_mem(INT32 page, INT32 is128k)
{
	if (is128k) {
		switch (page) {
			case 3: return 0x4000;
			case 4: return 0x8000;
			case 5: return 0xc000;
			case 6: return 0x10000;
			case 7: return 0x14000;
			case 8: return 0x18000;
			case 9: return 0x1c000;
			case 10: return 0x20000; // ram ends at 0x24000 (0-4000 = rom)
			default: return 0x0000;
		}
	} else {
		switch (page) {
			case 4: return 0x8000;
			case 5: return 0xc000;
			case 8: return 0x4000;
			default: return 0x0000;
		}
	}

	return 0x0000;
}

static void spectrum_loadz80()
{
	ZetSetAF(0, mem2uint16(0, 1));
	ZetSetBC(0, mem2uint16(2, 0));
	ZetSetHL(0, mem2uint16(4, 0));
	UINT16 v1PC = mem2uint16(6, 0);
	ZetSetSP(0, mem2uint16(8, 0));
	ZetSetI(0, (SpecSnapshotData[10] & 0xff));
	ZetSetR(0, (SpecSnapshotData[11] & 0x7f) | ((SpecSnapshotData[12] & 0x01) << 7));
	ula_border = (ula_border & 0xf8) | ((SpecSnapshotData[12] & 0x0e) >> 1);
	ZetSetDE(0, mem2uint16(13, 0));
	ZetSetBC2(0, mem2uint16(15, 0));
	ZetSetDE2(0, mem2uint16(17, 0));
	ZetSetHL2(0, mem2uint16(19, 0));
	ZetSetAF2(0, mem2uint16(21, 1));
	ZetSetIY(0, mem2uint16(23, 0));
	ZetSetIX(0, mem2uint16(25, 0));
	ZetSetIFF1(0, (SpecSnapshotData[27]) ? 1 : 0);
	ZetSetIFF2(0, (SpecSnapshotData[28]) ? 1 : 0);
	ZetSetIM(0, (SpecSnapshotData[29] & 0x03));

	if (v1PC != 0) { // version 1 (48k) snapshot
		bprintf(0, _T(".z80 version 1 - 48k snapshot - "));

		ZetSetPC(0, v1PC);

		if (SpecSnapshotData[12] & 0x20) {
			bprintf(0, _T(".z80: rle-compressed\n"));
			z80_rle_decompress(SpecSnapshotData + 30, 0x4000, 0xc000);
		} else {
			bprintf(0, _T(".z80: un-compressed\n")); // testcase: Horace Goes Skiing
			for (INT32 i = 0x4000; i < 0xc000; i++) {
				snapshot_write_ram(i, SpecSnapshotData[30 + (i - 0x4000)]);
			}
		}
	} else {
		INT32 v2_v3_headerlen = mem2uint16(30, 0);
		INT32 hwmode = SpecSnapshotData[34];
		INT32 v2 = (v2_v3_headerlen == 23);
		INT32 is_128k = ((v2) ? (hwmode > 2) : (hwmode > 3));

		bprintf(0, _T(".z80 version %d - "), (v2) ? 2 : 3);
		if (is_128k) {
			bprintf(0, _T("128k\n"));
			if (~SpecMode & SPEC_128K) {
				bprintf(PRINT_ERROR, _T(".z80 Error: loading 128k snapshot on 48k hw!\n"));
				return;
			}
		} else {
			bprintf(0, _T("48k\n"));
		}

		ZetSetPC(0, mem2uint16(32, 0));

		if (SpecMode & SPEC_AY8910 && SpecSnapshotData[37] & (1<<2)) { // AY8910
			bprintf(0, _T(".z80 contains AY8910 registers\n"));
			ZetOpen(0);
			for (INT32 i = 0; i < 0x10; i++) { // write regs
				AY8910Write(0, 0, i);
				AY8910Write(0, 1, SpecSnapshotData[39 + i]);
			}
			AY8910Write(0, 0, SpecSnapshotData[38]); // write latch
			ZetClose();
		}

		INT32 offset = v2_v3_headerlen + 32;
		while (offset < SpecSnapshotDataLen)
		{
			UINT16 length = mem2uint16(offset, 0);
			UINT32 dest = page_to_mem(SpecSnapshotData[offset + 2], is_128k);

			if (length == 0xffff) {
				length = 0x4000;
				bprintf(0, _T(".z80: copying $%x uncompressed bytes to %x\n"), length, dest);
				for (INT32 i = 0; i < length; i++)
					snapshot_write_ram(dest + i, SpecSnapshotData[offset + 3 + i]);
			} else {
				bprintf(0, _T(".z80: decompressing $%x bytes to %x\n"), length, dest);
				z80_rle_decompress(&SpecSnapshotData[offset + 3], dest, 0x4000);
			}

			offset += 3 + length;
		}

		if (is_128k) {
			Spec128kMapper = (SpecSnapshotData[35] & 0xff);
			ZetOpen(0);
			spectrum128_bank();
			ZetClose();
		}
	}
}

static INT32 BurnGetLength(INT32 rom_index)
{
	struct BurnRomInfo ri = { "", 0, 0, 0 };
	BurnDrvGetRomInfo(&ri, 0);
	return ri.nLen;
}

#if 0
static char *BurnGetName(INT32 rom_index)
{
	char *szName = NULL;
	BurnDrvGetRomName(&szName, rom_index, 0);

	return szName;
}
#endif

struct s_modes {
	INT32 mode;
	TCHAR text[40];
};

static s_modes speccy_modes[] = {
	{ SPEC_TAP,		_T("tape file")						},
	{ SPEC_TZX,		_T("(.tzx format)")					},
	{ SPEC_SLOWTAP,	_T("(.tap format, slow load)")		},
	{ SPEC_Z80,		_T(".z80 file")						},
	{ SPEC_128K,	_T("128K")							},
	{ SPEC_PLUS2,	_T("+2a")							},
	{ SPEC_INVES,	_T("Investronica (un-contended)")	},
	{ SPEC_AY8910,	_T("AY-8910 PSG")					},
	{ -1,			_T("")								}
};

static void PrintSpeccyMode()
{
	bprintf(0, _T("Speccy Init w/ "));

	for (INT32 i = 0; speccy_modes[i].mode != -1; i++) {
		if (SpecMode & speccy_modes[i].mode) {
			bprintf(0, _T("%s, "), speccy_modes[i].text);
		}
	}

	bprintf(0, _T("...\n"));
}


static INT32 SpectrumInit(INT32 Mode)
{
	SpecMode = Mode;

	SpecMode |= SPEC_AY8910; // Add an AY-8910!

	PrintSpeccyMode();

	BurnSetRefreshRate(50.0);

	BurnAllocMemIndex();

	INT32 nRet = 0;

	if (SpecMode & SPEC_Z80) {
		// Snapshot
		SpecSnapshotDataLen = BurnGetLength(0);
		nRet = BurnLoadRom(SpecSnapshotData + 0x00000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(SpecZ80Rom + 0x00000, 0x80, 1); if (nRet != 0) return 1;
	}
	else if (SpecMode & SPEC_TAP) {
		// TAP
		SpecTAPLen = BurnGetLength(0);
		nRet = BurnLoadRom(SpecTAP + 0x00000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(SpecZ80Rom + 0x00000, 0x80, 1); if (nRet != 0) return 1;

		SpecTAPInit();
	} else {
		// System
		nRet = BurnLoadRom(SpecZ80Rom + 0x00000, 0, 1); if (nRet != 0) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(SpecZ80Read);
	ZetSetWriteHandler(SpecZ80Write);
	ZetSetInHandler(SpecZ80PortRead);
	ZetSetOutHandler(SpecZ80PortWrite);
	if (SpecMode & SPEC_TAP && !(SpecMode & SPEC_SLOWTAP || SpecMode & SPEC_TZX)) {
		bprintf(0, _T("**  Spectrum: Using TAP file (len 0x%x) - DMA Loader\n"), SpecTAPLen);
		z80_set_spectrum_tape_callback(SpecTAPDMACallback);
	}
	if (~SpecMode & SPEC_INVES) {
		Z80InitContention(48, &update_ula);
	}
	ZetClose();

	AY8910Init(0, 17734475 / 10, 0);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 224*312*50);

	// Init Buzzer (in DoReset!)

	GenericTilesInit();

	ula_init(312, 224, 14335);

	SpecDoReset();

	return 0;
}

static INT32 Spectrum128Init(INT32 Mode)
{
	SpecMode = Mode;

	SpecMode |= SPEC_AY8910; // Add an AY-8910! (always on-board w/128K)

	BurnSetRefreshRate(50.0);

	BurnAllocMemIndex();

	PrintSpeccyMode();

	INT32 nRet = 0;

	if (SpecMode & SPEC_Z80) {
		// Snapshot
		SpecSnapshotDataLen = BurnGetLength(0);
		nRet = BurnLoadRom(SpecSnapshotData + 0x00000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(SpecZ80Rom + 0x00000, 0x80, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(SpecZ80Rom + 0x04000, 0x81, 1); if (nRet != 0) return 1;
		if (SpecMode & SPEC_PLUS2) {
			nRet = BurnLoadRom(SpecZ80Rom + 0x08000, 0x82, 1); if (nRet != 0) return 1;
			nRet = BurnLoadRom(SpecZ80Rom + 0x0c000, 0x83, 1); if (nRet != 0) return 1;
		}
	}
	else if (SpecMode & SPEC_TAP) {
		// TAP
		SpecTAPLen = BurnGetLength(0);
		nRet = BurnLoadRom(SpecTAP + 0x00000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(SpecZ80Rom + 0x00000, 0x80, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(SpecZ80Rom + 0x04000, 0x81, 1); if (nRet != 0) return 1;
		if (SpecMode & SPEC_PLUS2) {
			nRet = BurnLoadRom(SpecZ80Rom + 0x08000, 0x82, 1); if (nRet != 0) return 1;
			nRet = BurnLoadRom(SpecZ80Rom + 0x0c000, 0x83, 1); if (nRet != 0) return 1;
		}

		SpecTAPInit();
	} else {
		// System
		nRet = BurnLoadRom(SpecZ80Rom + 0x00000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(SpecZ80Rom + 0x04000, 1, 1); if (nRet != 0) return 1;
		if (SpecMode & SPEC_PLUS2) {
			nRet = BurnLoadRom(SpecZ80Rom + 0x08000, 2, 1); if (nRet != 0) return 1;
			nRet = BurnLoadRom(SpecZ80Rom + 0x0c000, 3, 1); if (nRet != 0) return 1;
		}
	}

	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(SpecSpec128Z80Read);
	ZetSetWriteHandler(SpecSpec128Z80Write);
	ZetSetInHandler(SpecSpec128Z80PortRead);
	ZetSetOutHandler(SpecSpec128Z80PortWrite);
	if (SpecMode & SPEC_TAP && !(SpecMode & SPEC_SLOWTAP || SpecMode & SPEC_TZX)) {
		bprintf(0, _T("**  Spectrum 128k: Using TAP file (len 0x%x) - DMA Loader\n"), SpecTAPLen);
		z80_set_spectrum_tape_callback(SpecTAPDMACallback);
	}
	if (~SpecMode & SPEC_INVES) {
		Z80InitContention((SpecMode & SPEC_PLUS2) ? 1282 : 128, &update_ula);
	}
	ZetClose();

	AY8910Init(0, 17734475 / 10, 0);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 228*311*50);

	// Init Buzzer (in DoReset!)

	GenericTilesInit();

	ula_init(311, 228, 14361);

	SpecDoReset();

	return 0;
}

static INT32 get_type()
{
	char *rn = NULL;

	if (!BurnDrvGetRomName(&rn, 0, 0))
	{
		INT32 len = strlen(rn);

		if (len > 4) {
			if (!strcasecmp(".z80", rn + (len-4))) {
				return SPEC_Z80;
			}
			if (!strcasecmp(".tap", rn + (len-4))) {
				return SPEC_TAP;
			}
			if (!strcasecmp(".tzx", rn + (len-4))) {
				return SPEC_TAP | SPEC_TZX;
			}
		}
	}

	return 0;
}

INT32 SpecInit()
{
	return SpectrumInit(get_type());
}

INT32 SpecSlowTAPInit()
{
	return SpectrumInit(get_type() | SPEC_SLOWTAP);
}

INT32 Spec128KInit()
{
	return Spectrum128Init(SPEC_128K | get_type());
}

INT32 Spec128KSlowTAPInit()
{
	return Spectrum128Init(SPEC_128K | get_type() | SPEC_SLOWTAP);
}

INT32 Spec128KPlus2Init()
{
	return Spectrum128Init(SPEC_128K | SPEC_PLUS2 | get_type());
}

INT32 Spec128KInvesInit()
{
	return Spectrum128Init(SPEC_128K | SPEC_INVES | get_type());
}

INT32 SpecExit()
{
	ZetExit();

	if (SpecMode & SPEC_AY8910) AY8910Exit(0);

#if TAPE_TO_WAV
	fclose(tape_dump_fp);
	tape_dump_fp = NULL;
#endif
	BuzzerExit();

	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static void SpecCalcPalette()
{
	UINT32 spec_pal[0x10] = {
		0x000000, 0x0000bf, 0xbf0000, 0xbf00bf,
		0x00bf00, 0x00bfbf, 0xbfbf00, 0xbfbfbf,
		0x000000, 0x0000ff, 0xff0000, 0xff00ff,
		0x00ff00, 0x00ffff, 0xffff00, 0xffffff
	};

	for (INT32 i = 0; i < 0x10; i++) {
		SpecPalette[i] = BurnHighCol((spec_pal[i] >> 16) & 0xff, (spec_pal[i] >> 8) & 0xff, spec_pal[i] & 0xff, 0);
	}
}

INT32 SpecDraw()
{
	if (SpecRecalc) {
		SpecCalcPalette();
		SpecRecalc = 0;
	}

	BurnTransferCopy(SpecPalette);

	return 0;
}

// dink's ULA simulator 2000 SSEI (super special edition intense)
static INT32 CONT_START;
static INT32 CONT_END;
static INT32 BORDER_START;
static INT32 BORDER_END;

static void ula_init(INT32 scanlines, INT32 cyc_scanline, INT32 contention)
{
	SpecScanlines = scanlines;
	SpecCylesPerScanline = cyc_scanline;
	SpecContention = contention;
	CONT_START  = SpecContention; // 48k; 14335 / 128k: 14361
	CONT_END    = (CONT_START + 192 * SpecCylesPerScanline);

	BORDER_START= SpecContention + 3; // start at first vis. pixel (top-left)
	BORDER_START-=(SpecCylesPerScanline * 16) + 8; // go back 16 lines
	BORDER_END  = SpecCylesPerScanline * (16+256+16);
}

#if 0
static UINT8 ula_snow()
{
	extern int counter;
	return (ActiveZ80GetI() >= 0x40 && ActiveZ80GetI() <= 0x7f && (ActiveZ80GetLastOp() & counter) == 0/* && (rand() & 0x187)==0x181*/) ? (ActiveZ80GetR() & 0x7f) : 0x00;
}
#endif

static void ula_run_cyc(INT32 cyc, INT32 draw_screen)
{
	ula_byte = 0xff; // open-bus byte 0xff unless ula fetches screen data

	// borders (top + sides + bottom) - note: only drawing 16px borders here!
	if (cyc >= BORDER_START && cyc <= BORDER_END) {
		INT32 offset = cyc - BORDER_START;
		INT32 x = ((offset) % SpecCylesPerScanline) * 2;
		INT32 y =  (offset) / SpecCylesPerScanline;
		INT32 border = ula_border & 0x07;

		INT32 draw = 0;

		// top border
		if (y >= 0 && y < 16 && x >= 0 && x < nScreenWidth) {
			draw = 1;
		}

		// side borders
		if (y >= 16 && y < 16+192+16 && ((x >= 0 && x < 16) || (x >= 16+256 && x < 16+256+16)) && x < nScreenWidth) {
			draw = 1;
		}

		// bottom border
		if (y >= 16 + 192 && y < 16+192+16 && x >= 0 && x < nScreenWidth) {
			draw = 1;
		}

		if (draw) {
			if (draw_screen && ((x & 7) == 0) && x <= nScreenWidth - 8) {
				for (INT32 xx = x; xx < x + 8; xx++) {
					pTransDraw[y * nScreenWidth + xx] = border;
				}
			}
		}
	}

	// active area
	if (cyc > CONT_START && cyc <= CONT_END) {
		INT32 offset = cyc - (CONT_START+1); // get to start of contention pattern
		INT32 x = (offset) % SpecCylesPerScanline;
		INT32 y = (offset) / SpecCylesPerScanline;

		if (x < 128) {
			switch (x & 7) {
				default: // 0, 1, 6, 7
					ula_byte = 0xff;
					break;

				case 2:
				case 4:
					ula_scr = SpecVideoRam[((y & 0xc0) << 5) | ((y & 0x38) << 2) | ((y & 0x07) << 8) | (x >> 2)];// | ula_snow()<<1];
					ula_byte = ula_scr;
					break;

				case 3:
				case 5:
					ula_attr = SpecVideoRam[0x1800 | ((y & 0xf8) << 2) | (x >> 2)];
					ula_byte = ula_attr;

					UINT16 *dst = pTransDraw + ((y + 16) * nScreenWidth) + (((x << 1) + 16) & ~7);
					UINT8 ink = (ula_attr & 0x07) + ((ula_attr >> 3) & 0x08);
					UINT8 pap = (ula_attr >> 3) & 0x0f;

					if (ula_flash & 0x10 && ula_attr & 0x80) ula_scr = ~ula_scr;

					if (draw_screen) {
						*dst++ = (ula_scr & (1 << 7)) ? ink : pap;
						*dst++ = (ula_scr & (1 << 6)) ? ink : pap;
						*dst++ = (ula_scr & (1 << 5)) ? ink : pap;
						*dst++ = (ula_scr & (1 << 4)) ? ink : pap;
						*dst++ = (ula_scr & (1 << 3)) ? ink : pap;
						*dst++ = (ula_scr & (1 << 2)) ? ink : pap;
						*dst++ = (ula_scr & (1 << 1)) ? ink : pap;
						*dst++ = (ula_scr & (1 << 0)) ? ink : pap;
					}
					break;
			}
		}
	}
}

static void update_ula(INT32 cycle)
{
	if (in_tape_ffwd) return;
	//bprintf(0, _T("update_ula:  %d   last %d\n"), cycle, ula_last_cyc);
	if (cycle == -1) {
		// next frame!
		ula_last_cyc = 0;
		return;
	}

	for (INT32 i = ula_last_cyc+1; i <= cycle; i++) {
		ula_run_cyc(i, 1);
	}

	ula_last_cyc = cycle;
}

#if TAPE_TO_WAV
static void DumpTape()
{
	if (tape_to_wav_startup_pause) {
		tape_to_wav_startup_pause--;
		BurnSoundClear();

		if (tape_to_wav_startup_pause == 0) {
			pulse_status = TAPE_PLAYING;
			start_tstate = total_tstates();
			if (SpecMode & SPEC_SLOWTAP) {
				tap_op = -1;
			}
		} else {
			ZetClose();
			return;
		}
	}
	for (INT32 i = 0; i < buzzer_data_frame; i++) {
		ZetIdle(1);
		get_pulse(0);
		BuzzerAdd(last_pulse);
	}
	ZetClose();
	nExtraCycles = 0;
	SpecMode &= ~SPEC_AY8910;
	INT16 *buf = (INT16*)BurnMalloc(nBurnSoundLen*2*2*2);
	BuzzerRender(buf);
	//	BurnDumpAppend("out.pcm", (UINT8*)buf, nBurnSoundLen*2*2); - kinda slow for this
	fwrite((UINT8*)buf, 1, nBurnSoundLen*2*2, tape_dump_fp);
	BurnFree(buf);
	return;
}
#endif

static void SwapByte(UINT8 &a, UINT8 &b)
{ // a <-> b, using xor
	a = a ^ b;
	b = a ^ b;
	a = a ^ b;
}

INT32 SpecFrame()
{
	if (SpecReset) SpecDoReset();

	if (SpecMode & SPEC_TAP) TAPAutoLoadRobot();

	{
		// Init keyboard matrix & 128k secondary matrix (caps/shifts lines) active-low (0x1f)
		SpecInput[0] = SpecInput[1] = SpecInput[2] = SpecInput[3] = SpecInput[4] = SpecInput[5] = SpecInput[6] = SpecInput[7] = 0x1f;
		SpecInput[11] = SpecInput[12] = SpecInput[13] = SpecInput[14] = SpecInput[15] = 0x1f;

		SpecInput[8] = 0x00; // kempston joy (active high)
		SpecInput[9] = SpecInput[10] = 0x1f; // intf2 joy (active low)

		if (SpecDips[0] & 0x01) { // use intf2: map kempston joy1 to intf2 joy1
			SpecInputKbd[9][1] = SpecInputKbd[8][3];
			SpecInputKbd[9][2] = SpecInputKbd[8][2];
			SpecInputKbd[9][4] = SpecInputKbd[8][1];
			SpecInputKbd[9][3] = SpecInputKbd[8][0];
			SpecInputKbd[9][0] = SpecInputKbd[8][4];

			if (SpecDips[0] & 0x20) { // swap intf2 joy1 <-> joy2
				SwapByte(SpecInputKbd[9][1], SpecInputKbd[10][3]);
				SwapByte(SpecInputKbd[9][2], SpecInputKbd[10][2]);
				SwapByte(SpecInputKbd[9][4], SpecInputKbd[10][0]);
				SwapByte(SpecInputKbd[9][3], SpecInputKbd[10][1]);
				SwapByte(SpecInputKbd[9][0], SpecInputKbd[10][4]);
			}
		}

		if (SpecDips[0] & (0x02|0x04)) { // map Kempston to QAOPM/QAOP SPACE
			SpecInputKbd[2][0] |= SpecInputKbd[8][3]; // Up -> Q
			SpecInputKbd[1][0] |= SpecInputKbd[8][2]; // Down -> A
			SpecInputKbd[5][1] |= SpecInputKbd[8][1]; // Left -> O
			SpecInputKbd[5][0] |= SpecInputKbd[8][0]; // Right -> P

			switch (SpecDips[0] & (0x02|0x04)) {
				case 0x02:
					SpecInputKbd[7][2] |= SpecInputKbd[8][4]; // Fire -> M
					break;
				case 0x04:
					SpecInputKbd[7][0] |= SpecInputKbd[8][4]; // Fire -> Space
					break;
			}
		}

		if (SpecDips[0] & 0x08) { // map kempston joy1 to Cursor Keys
			SpecInputKbd[4][3] |= SpecInputKbd[8][3]; // Up -> 7
			SpecInputKbd[4][4] |= SpecInputKbd[8][2]; // Down -> 6
			SpecInputKbd[3][4] |= SpecInputKbd[8][1]; // Left -> 5
			SpecInputKbd[4][2] |= SpecInputKbd[8][0]; // Right -> 8
			SpecInputKbd[4][0] |= SpecInputKbd[8][4]; // Fire -> 0
		}

		if (SpecDips[0] & 0x40) { // map kempston joy1 to Jumping Jack keys
			SpecInputKbd[7][1] |= SpecInputKbd[8][1]; // Left -> Symbol Shift
			SpecInputKbd[7][0] |= SpecInputKbd[8][0]; // Right -> Space
			SpecInputKbd[0][0] |= SpecInputKbd[8][4]; // Fire -> Caps Shift
		}

		// Compile the input matrix
		for (INT32 i = 0; i < 5; i++) {
			for (INT32 j = 0; j < 0x10; j++) { // 8x5 keyboard matrix
				SpecInput[j] ^= (SpecInputKbd[j][i] & 1) << i;
			}
		}

		// Disable inactive hw
		if ((SpecDips[0] & 0x1f) != 0x00) { // kempston not selected
			SpecInput[8] = 0xff; // kempston joy (active high, though none present returns 0xff)
		}
		if ((SpecDips[0] & 0x1f) != 0x01) { // intf2 not selected
			SpecInput[9] = SpecInput[10] = 0x1f; // intf2 joy (active low)
		}
	}

	INT32 nCyclesDo = 0;

	super_tstate += ZetTotalCycles(0);

	ZetNewFrame();
	ZetOpen(0);
	ZetIdle(nExtraCycles);
	nExtraCycles = 0;

	const INT32 IRQ_LENGTH = 38; // 48k 32, 128k 36 (z80 core takes care of this - only allowing irq for proper length of machine, so value here is OK)
#if TAPE_TO_WAV
	DumpTape();
	return 0;
#endif
	for (INT32 i = 0; i < SpecScanlines; i++) {
		if (i == 0) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			ZetRun(IRQ_LENGTH);
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			ula_flash = (ula_flash + 1) & 0x1f;
		}

		nCyclesDo += SpecCylesPerScanline;
		ZetRun(nCyclesDo - ZetTotalCycles());

		if (SpecMode & SPEC_INVES) {
			update_ula(ZetTotalCycles());
		}
	}

	if (SpecMode & SPEC_INVES) {
		update_ula(-1); // notify start next frame
	}

	if (pBurnDraw) {
		SpecDraw();
	}

	if (pBurnSoundOut) {
		if (SpecMode & SPEC_AY8910) {
			AY8910Render(pBurnSoundOut, nBurnSoundLen);
		}

		BuzzerRender(pBurnSoundOut);
	}

	INT32 tot_frame = SpecScanlines * SpecCylesPerScanline;
	nExtraCycles = ZetTotalCycles() - tot_frame;

	ZetClose();

	if ((~SpecDips[1] & 1) && (SpecMode & SPEC_SLOWTAP || SpecMode & SPEC_TZX) && in_tape_ffwd == 0 && pulse_status == TAPE_PLAYING) {
		in_tape_ffwd = 1;
		INT32 cnt = 50;
		UINT8 *draw_temp = pBurnDraw;
		INT16 *sound_temp = pBurnSoundOut;
		pBurnDraw = NULL;
		pBurnSoundOut = NULL;
		while (cnt && pulse_status == TAPE_PLAYING) {
			SpecFrame();
			cnt--;
		}
		pBurnSoundOut = sound_temp;
		pBurnDraw = draw_temp;
		in_tape_ffwd = 0;
	}

	return 0;
}

INT32 SpecScan(INT32 nAction, INT32* pnMin)
{
	if (pnMin != NULL) {
		*pnMin = 0x029744;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(RamStart, RamEnd-RamStart, "All RAM");
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);

		if (SpecMode & SPEC_AY8910) {
			AY8910Scan(nAction, pnMin);
		}

		SCAN_VAR(ula_attr);
		SCAN_VAR(ula_scr);
		SCAN_VAR(ula_byte);
		SCAN_VAR(ula_border);
		SCAN_VAR(ula_flash);
		SCAN_VAR(ula_last_cyc);

		SCAN_VAR(Spec128kMapper);
		SCAN_VAR(Spec128kMapper2);

		SCAN_VAR(nExtraCycles);

		if (SpecMode & SPEC_SLOWTAP || SpecMode & SPEC_TZX) {
			pulse_scan();
		}

		if (SpecMode & SPEC_TAP) {
			// .TAP
			SCAN_VAR(SpecTAPBlocknum);
			// .TAP Robot
			SCAN_VAR(CASAutoLoadPos);
			SCAN_VAR(CASAutoLoadTicker);
			SCAN_VAR(CASFrameCounter);
		}
	}

	if (nAction & ACB_WRITE && SpecMode & SPEC_128K) {
		ZetOpen(0);
		spectrum128_bank();
		ZetClose();
	}

	return 0;
}

INT32 SpectrumGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		if (i == 1 && BurnDrvGetTextA(DRV_BOARDROM)) {
			pszGameName = BurnDrvGetTextA(DRV_BOARDROM);
		} else {
			pszGameName = BurnDrvGetTextA(DRV_PARENT);
		}
	}

	if (pszGameName == NULL || i > 2) {
		*pszName = NULL;
		return 1;
	}

	// remove leader
	memset(szFilename, 0, MAX_PATH);
	for (UINT32 j = 0; j < (strlen(pszGameName) - 5); j++) {
		szFilename[j] = pszGameName[j + 5];
	}

	*pszName = szFilename;

	return 0;
}
