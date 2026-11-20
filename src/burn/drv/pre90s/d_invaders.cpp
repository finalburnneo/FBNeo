// FB Neo Midway 8080 B&W / Taito 8080 (Space Invaders and friends) driver module
// Based on MAME driver by Michael Strutts, Nicola Salmoria, Tormod Tjaberg,
// Mirko Buffoni, Lee Taylor, Valerio Verrando, Marco Cassili, Zsolt Vasvari,
// Aaron Giles, Derrick Renaud, Jonathan Gevaryahu, hap, Robbbert and others

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6800_intf.h"
#include "ay8910.h"
#include "samples.h"
#include "sn76477.h"
#include "stream.h"
#include "watchdog.h"
#include "burn_gun.h"
#include "burn_artwork.h"
#include "bitswap.h"
#include <math.h>

void SN76477_sound_update_INT(INT32 param, INT16 *buffer, INT32 length);

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvI8080ROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvColorPROM;
static UINT8 *DrvUser1;
static UINT8 *DrvMainRAM;
static UINT8 *DrvExtraRAM;
static UINT8 *DrvColorRAM;
static UINT8 *DrvColorRAM2;
static UINT8 *DrvSndRAM;
static UINT8 *DrvStars;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *ovl_map;			// per (displayed) pixel overlay index
static INT32 ovl_count;
static UINT32 ovl_rgb[16];

static UINT16 shift_data;
static UINT8 shift_count;
static UINT8 shift_reverse;

static UINT8 flipscreen;
static UINT8 color_map;
static UINT8 screen_red;
static UINT8 schaser_background_disable;
static UINT8 schaser_background_select;
static UINT8 polaris_cloud_speed;
static UINT8 polaris_cloud_pos;
static UINT8 port_1_last;
static UINT8 port_2_last;
static UINT8 port_3_last;
static UINT8 soundlatch;
static UINT8 sound_mute;
static double sound_mute_vol;	// soft mute
static INT32 vblank;
static INT32 lupin3_step;
static INT32 darthvdr_fleet_step;
static INT32 nmi_toggle;
static UINT16 claybust_gun_pos;
static INT32 claybust_gun_on;

// cosmo stars
#define STAR_RNG_PERIOD ((1 << 17) - 1)
static INT32 cosmo_stars_sidescroll;
static INT32 cosmo_star_speed;
static UINT32 cosmo_star_origin;
static INT32 cosmo_rng_offs;
static INT32 cosmo_bright_star;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoyP1[8];		// P1 (fire, left, right)
static UINT8 DrvJoyP2[8];		// P2
static UINT8 DrvDips[5];
static UINT8 DrvInputs[4];

// analog / positional controls (mw8080bw games)
static INT16 DrvAnalog[4];
static UINT8 DrvJoyX[8];		// extra digital controls (gun up/down, gear shift etc.)
static INT32 mw_pos[4];			// positional controls
static UINT8 mw_analog[4];		// processed analog values
static UINT8 ctrl_select;
static UINT8 zz_gear, zz_gear_prev;
static UINT8 trench_width, trench_center, trench_slope[16], bright_control, brightness;
static UINT16 phantom2_cloud_counter;
static UINT8 mw_lamps[2];		// artwork lamp latches (seawolf)
static UINT8 DrvInP1;
static UINT8 DrvInP2;
static UINT8 DrvReset;
static INT16 DrvGun0;
static INT16 DrvGun1;

// hardware variants
enum {
	VID_MW = 0,		// Midway b&w (shift register video, 4 pixel delay), overlays
	VID_INVADPT2,	// color map PROM
	VID_BALLBOMB,	// color map PROM, blue background
	VID_INDIANBT,	// color map PROM, no screen red
	VID_SCHASER,
	VID_SCHASERCV,
	VID_LUPIN3,		// scattered color ram, inverted
	VID_SFLUSH,		// scattered color ram (orbite)
	VID_POLARIS,
	VID_ROLLINGC,
	VID_COSMO,
	VID_SHUTTLEI,
	VID_SPACECOM,
	VID_VORTEX,
	VID_SPCENCTR,	// trench + strobe
	VID_PHANTOM2	// cloud prom
};

enum {
	SND_NONE = 0,
	SND_INVADERS,	// Midway/Taito discrete sound board + SN76477
	SND_INVAD2CT,	// 2 x invaders boards (stereo)
	SND_SAMPLES,	// invaders samples + SN76477 (saucer)
	SND_SPCEWARS,	// invaders samples + SN76477 + 1 bit dac
	SND_LRESCUE,	// lrescue samples + 1 bit dac
	SND_SCHASER,	// discrete + SN76477
	SND_SCHASERCV,	// invaders samples + SN76477 + 1 bit dac
	SND_LUPIN3,		// lupin3 samples + SN76477 + music
	SND_POLARIS,	// discrete
	SND_BALLBOMB,	// invaders samples + SN76477 + music
	SND_INDIANBT,	// invaders samples + SN76477 + music
	SND_OZMAWARS,	// ozmawars samples
	SND_INVRVNGE,	// M6802 + AY8910
	SND_SAMPLESONLY,// samples, no SN76477
	SND_GUNFIGHT,	// C version of the netlist (stereo)
	SND_ZZZAP,		// C version of the netlist
	SND_LAGUNAR,	// C version of the netlist
	SND_BOOTHILL,	// discrete (stereo)
	SND_MWMUSIC,	// midway tone generator only (+ samples)
};

struct mw_config {
	INT32 video;
	INT32 sound;
	INT32 palette;		// 0 = RBG_3BIT, 1 = RGB_3BIT, 2 = mono (overlay), 3 = rollingc, 4 = cosmo, 5 = sflush
	void (__fastcall *out)(UINT16, UINT8);
	UINT8 (__fastcall *in)(UINT16);
	INT32 map;			// see DrvMapMemory()
	INT32 cpu_clock;
	INT32 irq_type;		// 0 = midway (rst 08/10), 1 = darthvdr (vblank, rst 38)
	INT32 watchdog;		// frames, 0 = none
	void (*frame_cb)();	// per-frame input processing (analog controls etc.)
};

static const mw_config *cfg;

// overlays
struct ovl_rect { float x0, y0, x1, y1, r, g, b; };
struct ovl_layout { const ovl_rect *rects; INT32 count; float w, h; INT32 rot; };	// rot: 0 = ROT0, 1 = ROT270 (vertical), 2 = ROT90
static const ovl_layout *overlay;

// Discrete sound helpers
#define OP_AMP_NORTON_VBE		0.5
#define OP_AMP_VP_RAIL_OFFSET	1.5
#define TTL_V_LOGIC_1			3.4

enum { TRG_NONE = 0, TRG0, TRG0_INV, TRG1, TRG1_INV, TRG2, TRG2_INV, TRG01_AND, TRG01_NAND };

static double disc_dt = 1.0 / 48000;
static double disc_rate = 48000;

static void disc_set_rate(INT32 rate)
{
	disc_rate = (double)rate;
	disc_dt = 1.0 / disc_rate;
}

static inline double rc_exp(double rc) { return 1.0 - exp(-disc_dt / rc); }
static inline double rc_exp_dt(double rc, double dt) { return 1.0 - exp(-dt / rc); }

static inline INT32 trig_func(INT32 t0, INT32 t1, INT32 t2, INT32 f)
{
	switch (f) {
		case TRG0:       return t0;
		case TRG0_INV:   return !t0;
		case TRG1:       return t1;
		case TRG1_INV:   return !t1;
		case TRG2:       return t2;
		case TRG2_INV:   return !t2;
		case TRG01_AND:  return t0 && t1;
		case TRG01_NAND: return !(t0 && t1);
	}
	return 1;
}

struct d_lfsr {
	UINT32 reg, reset_val;
	INT32 bit0, bit1, outbit, bitlen;
	double t_left, t_clock, amp, bias, out;

	void reset(double clock, double amp_, double bias_, UINT32 resetv, INT32 b0, INT32 b1, INT32 ob) {
		bitlen = 17; reset_val = resetv; bit0 = b0; bit1 = b1; outbit = ob;
		amp = amp_; bias = bias_;
		t_clock = 1.0 / clock; t_left = 0;
		reg = reset_val;
		INT32 fb = ((reg >> bit0) ^ (reg >> bit1)) & 1;
		reg = (reg & ~(1 << bitlen)) | (fb << bitlen);
		out = (((reg >> outbit) & 1) ? amp / 2 : -amp / 2) + bias;
	}
	double step(INT32 enable, INT32 reset_line, INT32 feed = 0) {
		double cycles = (t_left + disc_dt) / t_clock;
		INT32 inc = (INT32)cycles;
		t_left = (cycles - inc) * t_clock;
		if (reset_line) { double c = 1.0 / t_clock; reset(c, amp, bias, reset_val, bit0, bit1, outbit); return out; }
		if (inc > 0) {
			INT32 fb = 0;
			for (INT32 i = 0; i < inc; i++) {
				fb = ((reg >> bitlen) & 1) | (feed ? 1 : 0);
				reg = ((reg << 1) & ((1 << bitlen) - 1) & ~1) | fb;
				fb = ((reg >> bit0) ^ (reg >> bit1)) & 1;
				reg = (reg & ((1 << bitlen) - 1)) | (fb << bitlen);
			}
			out = (((reg >> outbit) & 1) ? amp / 2 : -amp / 2) + bias;
		}
		return enable ? out : 0;
	}
};

#define OSC_1		0x00
#define OSC_VCO_1	0x20
#define OSC_VCO_2	0x30
#define OSC_VCO_3	0x40
#define OSC_NORTON	0x100
#define OSC_OUT_CAP	0x00
#define OSC_OUT_SQW	0x01

struct osc_info { UINT32 type; double r1, r2, r3, r4, r5, r6, r7, r8, c, vP; };

struct d_opamp_osc {
	const osc_info *info;
	INT32 type, out_type, flip_flop, flip_flop_xor, has_enable;
	double v_cap, v_out_high, threshold_low, threshold_high, i_fixed, i_enable, r_total, temp1, temp2;

	void reset(const osc_info *i) {
		info = i;
		out_type = i->type & 0x07;
		type = i->type & (0xf0 | OSC_NORTON);
		i_fixed = 0; has_enable = 0;
		switch (type) {
			case OSC_1 | OSC_NORTON:
				flip_flop_xor = 0; flip_flop = 1;
				if (i->r6 > 0) { has_enable = 1; i_enable = (i->vP - OP_AMP_NORTON_VBE) / (i->r6 + RES_K(1)); }
				break;
			case OSC_VCO_1 | OSC_NORTON: {
				flip_flop_xor = 1; flip_flop = 0;
				if (i->r6 != 0) i_fixed += i->vP / i->r6;
				i_fixed += OP_AMP_NORTON_VBE / i->r1;
				i_fixed += OP_AMP_NORTON_VBE / i->r2;
				r_total = 1.0 / i->r1 + 1.0 / i->r2 + (i->r7 ? 1.0 / i->r7 : 0);
				if (i->r6) r_total += 1.0 / i->r6;
				if (i->r8) r_total += 1.0 / i->r8;
				r_total = 1.0 / r_total;
				double i1 = (i->vP - OP_AMP_NORTON_VBE) / i->r5;
				double i2 = (i->vP - OP_AMP_NORTON_VBE - OP_AMP_NORTON_VBE) / i->r4;
				threshold_low = (i1 - i2) * i->r3 + OP_AMP_NORTON_VBE;
				i2 = (0.0 - OP_AMP_NORTON_VBE) / i->r4;
				threshold_high = (i1 - i2) * i->r3 + OP_AMP_NORTON_VBE;
				break;
			}
			case OSC_VCO_2 | OSC_NORTON: {
				flip_flop_xor = 0; flip_flop = 1;
				temp1 = (i->vP - OP_AMP_NORTON_VBE) / i->r2;
				temp2 = (i->vP - OP_AMP_NORTON_VBE) * (1.0 / i->r2 + 1.0 / i->r6);
				double i1 = (i->vP - OP_AMP_NORTON_VBE) / i->r5;
				double i2 = (0.0 - OP_AMP_NORTON_VBE) / i->r4;
				threshold_low = (i1 + i2) * i->r3 + OP_AMP_NORTON_VBE;
				i2 = (i->vP - OP_AMP_NORTON_VBE - OP_AMP_NORTON_VBE) / i->r4;
				threshold_high = (i1 + i2) * i->r3 + OP_AMP_NORTON_VBE;
				break;
			}
			case OSC_VCO_3 | OSC_NORTON: {
				flip_flop_xor = 0; flip_flop = 1;
				if (i->r8 > 0) { has_enable = 1; i_enable = (i->vP - OP_AMP_NORTON_VBE) / (i->r8 + RES_K(1)); }
				if (i->r7 != 0) i_fixed = (i->vP - OP_AMP_NORTON_VBE) / i->r7;
				temp1 = (i->vP - OP_AMP_NORTON_VBE - OP_AMP_NORTON_VBE) / i->r2;
				double i1 = (i->vP - OP_AMP_NORTON_VBE) / i->r5;
				double i2 = (0.0 - OP_AMP_NORTON_VBE) / i->r4;
				threshold_low = (i1 + i2) * i->r3 + OP_AMP_NORTON_VBE;
				i2 = (i->vP - OP_AMP_NORTON_VBE - OP_AMP_NORTON_VBE) / i->r4;
				threshold_high = (i1 + i2) * i->r3 + OP_AMP_NORTON_VBE;
				break;
			}
		}
		v_out_high = i->vP - OP_AMP_NORTON_VBE;
		v_cap = 0;
	}

	double step(INT32 enable, double vmod1, double vmod2) {
		double charge[2] = { 0, 0 };
		double dt = disc_dt, v_cap_l = v_cap, v_cap_next = 0, i = 0, v;
		INT32 force_charge = 0, ff = flip_flop, count_f = 0, count_r = 0;

		switch (type) {
			case OSC_1 | OSC_NORTON: {
				if (has_enable) { if (enable) i = i_enable; enable = 1; }
				charge[0] = v_out_high / info->r1 - i;
				charge[1] = (v_out_high - OP_AMP_NORTON_VBE) / info->r2 - charge[0];
				double i1 = v_out_high / info->r5;
				double i2 = (0.0 - OP_AMP_NORTON_VBE) / info->r4;
				threshold_low = (i1 + i2) * info->r3 + OP_AMP_NORTON_VBE;
				i2 = (v_out_high - OP_AMP_NORTON_VBE) / info->r4;
				threshold_high = (i1 + i2) * info->r3 + OP_AMP_NORTON_VBE;
				break;
			}
			case OSC_VCO_1 | OSC_NORTON:
				if (info->r7 == 0) v = vmod1;
				else {
					i = i_fixed + vmod1 / info->r7;
					if (info->r8 != 0) i += vmod2 / info->r8;
					v = i * r_total;
				}
				v -= OP_AMP_NORTON_VBE;
				charge[0] = v / info->r1;
				charge[1] = v / info->r2 - charge[0];
				force_charge = !enable;
				enable = 1;
				break;
			case OSC_VCO_2 | OSC_NORTON:
				i = vmod1 / info->r1;
				charge[0] = i - temp1;
				charge[1] = temp2 - i;
				if (charge[0] < 0) { force_charge = 1; charge[0] *= -1; }
				break;
			case OSC_VCO_3 | OSC_NORTON:
				charge[0] = i_fixed;
				if (has_enable) { if (enable) charge[0] -= i_enable; enable = 1; }
				v = vmod1 - OP_AMP_NORTON_VBE; if (v < 0) v = 0;
				charge[0] += v / info->r1;
				if (info->r6 != 0) { v = vmod2 - OP_AMP_NORTON_VBE; charge[0] += v / info->r6; }
				charge[1] = temp1 - charge[0];
				break;
		}

		if (!enable) return 0;

		INT32 loops = 0;
		do {
			if ((ff ^ flip_flop_xor) || force_charge) {
				v_cap_next = v_cap_l + (charge[1] * dt / info->c);
				dt = 0;
				if (v_cap_next > threshold_high) {
					ff = flip_flop_xor;
					if (ff) count_r++; else count_f++;
					if (force_charge) {
						if (v_cap_next > v_out_high) v_cap_next = v_out_high;
					} else {
						dt = info->c * (v_cap_next - threshold_high) / charge[1];
						v_cap_next = threshold_high;
					}
				}
			} else {
				v_cap_next = v_cap_l - (charge[0] * dt / info->c);
				dt = 0;
				if (v_cap_next < threshold_low) {
					ff = !flip_flop_xor;
					if (ff) count_r++; else count_f++;
					dt = info->c * (threshold_low - v_cap_next) / charge[0];
					v_cap_next = threshold_low;
				}
			}
			v_cap_l = v_cap_next;
		} while (dt > 0 && ++loops < 1000);

		if (v_cap_l > v_out_high) v_cap_l = v_out_high;
		if (v_cap_l < 0) v_cap_l = 0;
		v_cap = v_cap_l;

		double v_out;
		if (out_type == OSC_OUT_SQW) {
			if (count_f + count_r >= 2) v_out = flip_flop ? 0 : v_out_high;
			else v_out = ff * v_out_high;
		} else {
			v_out = v_cap_l;
		}
		flip_flop = ff;
		return v_out;
	}
};

struct opamp_info { double r1, r2, r3, r4, c, vN, vP; };

struct d_opamp {
	const opamp_info *info;
	INT32 has_r1, has_r4, has_cap;
	double v_max, v_cap, exponent, i_fixed;

	void reset(const opamp_info *i) {
		info = i;
		has_r1 = i->r1 > 0; has_r4 = i->r4 > 0; has_cap = 0;
		v_max = i->vP - OP_AMP_NORTON_VBE;
		v_cap = 0;
		if (i->c > 0) {
			has_cap = 1;
			exponent = has_r4 ? rc_exp(i->r4 * i->c) : disc_rate * i->c;
		}
		i_fixed = (i->r3 > 0) ? (i->vP - OP_AMP_NORTON_VBE) / i->r3 : 0;
	}
	double step(double in0, double in1) {
		double i_neg = 0, i_pos, i, v_out;
		if (has_r1) { i_neg = (in0 - OP_AMP_NORTON_VBE) / info->r1; if (i_neg < 0) i_neg = 0; }
		i_neg += i_fixed;
		i_pos = (in1 - OP_AMP_NORTON_VBE) / info->r2; if (i_pos < 0) i_pos = 0;
		i = i_pos - i_neg;
		if (has_cap) {
			if (has_r4) { i *= info->r4; v_cap += (i - v_cap) * exponent; }
			else v_cap += i / exponent;
			v_out = v_cap;
		} else if (has_r4) v_out = i * info->r4;
		else v_out = (i > 0) ? v_max : 0;
		if (v_out > v_max) v_out = v_max; else if (v_out < info->vN) v_out = info->vN;
		v_cap = v_out;
		return v_out;
	}
};

struct opamp_1sht_info { double r1, r2, r3, r4, r5, c1, c2, vN, vP; };

struct d_opamp_1sht {
	const opamp_1sht_info *info;
	double exponent1c, exponent1d, exponent2, i_fixed, v_cap1, v_cap2, v_max, r34ratio, v_out;

	void reset(const opamp_1sht_info *i) {
		info = i;
		exponent1c = rc_exp(RES_2_PARALLEL(i->r3, i->r4) * i->c1);
		exponent1d = rc_exp(i->r4 * i->c1);
		exponent2  = rc_exp(i->r2 * i->c2);
		i_fixed = (i->vP - OP_AMP_NORTON_VBE) / i->r1;
		v_cap1 = v_cap2 = 0;
		v_max = i->vP - OP_AMP_NORTON_VBE;
		r34ratio = i->r3 / (i->r3 + i->r4);
		v_out = 0;
	}
	double step(double trig) {
		double i_pos = (trig - v_cap2) / info->r2 + v_out / info->r5;
		v_cap2 += (trig - v_cap2) * exponent2;
		double i_neg = (v_cap1 - OP_AMP_NORTON_VBE) / info->r3; if (i_neg < 0) i_neg = 0;
		i_neg += i_fixed;
		v_out = (i_pos > i_neg) ? v_max : info->vN;
		double v = v_out + 0.6;
		if (v_cap1 > v_out) {
			if (v_cap1 > v) v_cap1 = v;
			else v_cap1 += (v_out - v_cap1) * exponent1d;
		} else {
			v_cap1 += ((v_out - OP_AMP_NORTON_VBE) * r34ratio + OP_AMP_NORTON_VBE - v_cap1) * exponent1c;
		}
		return v_out;
	}
};

struct tvca_info {
	double r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, c1, c2, c3, c4, v1, v2, v3, vP;
	INT32 f0, f1, f2, f3, f4, f5;
};

struct d_tvca {
	const tvca_info *info;
	double r67, v_out_max, v_trig[2], i_fixed, v_cap1, exponent_c[2], exponent_d[2];
	double v_cap2, v_trig2, exponent2[2], v_cap3, v_trig3, exponent3[2], v_cap4, exponent4;
	INT32 has_r4, has_c4;

	void reset(const tvca_info *i) {
		info = i;
		r67 = i->r6 + i->r7;
		v_out_max = i->vP - OP_AMP_NORTON_VBE;
		v_trig[0] = (i->v1 - 0.6) * RES_VOLTAGE_DIVIDER(i->r5, i->r6);
		v_trig[1] = (i->v1 - 0.6 - OP_AMP_NORTON_VBE) * RES_VOLTAGE_DIVIDER(i->r5, r67) + OP_AMP_NORTON_VBE;
		i_fixed = v_out_max / i->r1;
		v_cap1 = 0;
		exponent_c[0] = rc_exp(RES_2_PARALLEL(i->r5, i->r6) * i->c1);
		exponent_c[1] = rc_exp(RES_2_PARALLEL(i->r5, r67) * i->c1);
		exponent_d[1] = rc_exp(r67 * i->c1);
		exponent_d[0] = (i->r6 != 0) ? rc_exp(i->r6 * i->c1) : 0;
		v_cap2 = 0;
		v_trig2 = (i->v2 - 0.6 - OP_AMP_NORTON_VBE) * ((i->r9 + i->r8) ? RES_VOLTAGE_DIVIDER(i->r8, i->r9) : 0);
		exponent2[0] = i->r9 ? rc_exp(i->r9 * i->c2) : 0;
		exponent2[1] = (i->r8 && i->r9) ? rc_exp(RES_2_PARALLEL(i->r8, i->r9) * i->c2) : 0;
		v_cap3 = 0;
		v_trig3 = (i->v3 - 0.6 - OP_AMP_NORTON_VBE) * ((i->r10 + i->r11) ? RES_VOLTAGE_DIVIDER(i->r10, i->r11) : 0);
		exponent3[0] = i->r11 ? rc_exp(i->r11 * i->c3) : 0;
		exponent3[1] = (i->r10 && i->r11) ? rc_exp(RES_2_PARALLEL(i->r10, i->r11) * i->c3) : 0;
		v_cap4 = 0;
		has_r4 = (i->r4 != 0); has_c4 = (i->c4 != 0);
		exponent4 = (has_r4 && has_c4) ? rc_exp(i->r4 * i->c4) : 0;
	}
	double step(INT32 trig0, INT32 trig1, INT32 trig2, double in0, double in1) {
		double i2 = 0, i3 = 0, i_neg, i_pos, i_out, v_out;
		INT32 f3 = trig_func(trig0, trig1, trig2, info->f3);

		if (info->r2 != 0 && trig_func(trig0, trig1, trig2, info->f0)) {
			i2 = (in0 - OP_AMP_NORTON_VBE) / info->r2; if (i2 < 0) i2 = 0;
		}
		if (info->r3 != 0 && trig_func(trig0, trig1, trig2, info->f1)) {
			i3 = (in1 - OP_AMP_NORTON_VBE) / info->r3; if (i3 < 0) i3 = 0;
		}
		i_neg = i_fixed + i2 + i3;

		if (trig_func(trig0, trig1, trig2, info->f2))
			v_cap1 += (v_trig[f3] - v_cap1) * exponent_c[f3];
		else
			v_cap1 += ((f3 ? OP_AMP_NORTON_VBE : 0.0) - v_cap1) * exponent_d[f3];

		i_pos = (v_cap1 - OP_AMP_NORTON_VBE) / r67;
		if (i_pos < 0 || !f3) i_pos = 0;

		if (info->r9 != 0) {
			INT32 f = trig_func(trig0, trig1, trig2, info->f4);
			v_cap2 += ((f ? v_trig2 : 0) - v_cap2) * exponent2[f];
			i_pos += v_cap2 / info->r9;
		}
		if (info->r11 != 0) {
			INT32 f = trig_func(trig0, trig1, trig2, info->f5);
			v_cap3 += ((f ? v_trig3 : 0) - v_cap3) * exponent3[f];
			i_pos += v_cap3 / info->r11;
		}

		i_out = i_pos - i_neg; if (i_out < 0) i_out = 0;

		if (has_c4) {
			if (has_r4) { i_out *= info->r4; v_cap4 += (i_out - v_cap4) * exponent4; }
			else v_cap4 += i_out / exponent4;
			if (v_cap4 < 0) v_cap4 = 0;
			v_out = v_cap4;
		} else v_out = i_out * info->r4;

		if (v_out > v_out_max) v_out = v_out_max;
		return v_out;
	}
};

struct d_rc {
	double exponent, v_out;
	void reset(double r, double c) { exponent = rc_exp(r * c); v_out = 0; }
	double step(double in) { v_out += (in - v_out) * exponent; return v_out; }
};

struct d_cr {
	double exponent, v_cap;
	void reset(double r, double c) { exponent = rc_exp(r * c); v_cap = 0; }
	double step(double in) { double v_out = in - v_cap; v_cap += v_out * exponent; return v_out; }
};

#define D555_OUT_SQW	0x00
#define D555_OUT_CAP	0x01
#define D555_OUT_ENERGY	0x04
#define D555_OUT_AC		0x10

struct d_555_astable {
	INT32 out_type, flip_flop, output_ac;
	double v_out_high, v_charge, threshold, trigger, cap_voltage, ac_shift;
	double last_r1, last_r2, last_c, t_rc_bleed, t_rc_charge, t_rc_discharge, exp_bleed, exp_charge, exp_discharge;

	void reset(INT32 options, double v_pos, double v_charge_, double v_out_high_) {
		out_type = options & 0x07;
		output_ac = options & D555_OUT_AC;
		v_out_high = (v_out_high_ < 0) ? v_pos - 1.2 : v_out_high_;
		v_charge = (v_charge_ < 0) ? v_pos : v_charge_;
		threshold = v_pos * 2.0 / 3.0;
		trigger = v_pos / 3.0;
		ac_shift = output_ac ? -v_out_high / 2.0 : 0;
		flip_flop = 1; cap_voltage = 0;
		last_r1 = last_r2 = last_c = -1;
	}
	double step(INT32 reset_line, double r1, double r2, double c) {
		if (!reset_line) { flip_flop = 1; cap_voltage = 0; return 0; }

		if (r1 != last_r1 || r2 != last_r2 || c != last_c) {
			t_rc_bleed = RES_M(10) * c;
			t_rc_charge = (r1 + r2) * c;
			t_rc_discharge = r2 * c;
			exp_bleed = rc_exp(t_rc_bleed);
			exp_charge = rc_exp(t_rc_charge);
			exp_discharge = rc_exp(t_rc_discharge);
			last_r1 = r1; last_r2 = r2; last_c = c;
		}

		INT32 count_f = 0, count_r = 0, ff = flip_flop, update_exponent = 0;
		double dt = disc_dt, x_time = 0, v_cap = cap_voltage, v_cap_next = 0, exponent;

		if (c == 0) {
			ff = 1; v_cap = v_charge; cap_voltage = 0;
		} else {
			INT32 loops = 0;
			do {
				if (ff) {
					if (r1 == 0) {
						exponent = update_exponent ? rc_exp_dt(t_rc_bleed, dt) : exp_bleed;
						v_cap_next = v_cap - v_cap * exponent;
						dt = 0;
					} else {
						exponent = update_exponent ? rc_exp_dt(t_rc_charge, dt) : exp_charge;
						v_cap_next = v_cap + (v_charge - v_cap) * exponent;
						dt = 0;
						if (v_cap_next >= threshold) {
							dt = t_rc_charge * log(1.0 / (1.0 - ((v_cap_next - threshold) / (v_charge - v_cap))));
							x_time = dt;
							v_cap_next = threshold;
							ff = 0; count_f++; update_exponent = 1;
						}
					}
				} else {
					if (r2 != 0) {
						exponent = update_exponent ? rc_exp_dt(t_rc_discharge, dt) : exp_discharge;
						v_cap_next = v_cap - v_cap * exponent;
						dt = 0;
					} else v_cap_next = trigger;
					if (v_cap_next <= trigger) {
						if (v_cap_next < trigger) dt = t_rc_discharge * log(1.0 / (1.0 - ((trigger - v_cap_next) / v_cap)));
						x_time = dt;
						v_cap_next = trigger;
						ff = 1; count_r++; update_exponent = 1;
					}
				}
				v_cap = v_cap_next;
			} while (dt > 0 && ++loops < 1000);
			cap_voltage = v_cap;
		}

		x_time /= disc_dt;
		double v_out = 0;
		switch (out_type) {
			case D555_OUT_SQW:
				if (count_f + count_r >= 2) v_out = flip_flop ? 0 : v_out_high;
				else v_out = ff * v_out_high;
				v_out += ac_shift;
				break;
			case D555_OUT_CAP:
				v_out = v_cap;
				if (output_ac) v_out -= threshold * 3.0 / 4.0;
				break;
			case D555_OUT_ENERGY:
				if (x_time == 0) x_time = 1.0;
				v_out = v_out_high * (ff ? x_time : (1.0 - x_time));
				v_out += ac_shift;
				break;
		}
		flip_flop = ff;
		return v_out;
	}
};

#define MIXER_IS_RESISTOR	0
#define MIXER_IS_OP_AMP		1

struct mixer_info { INT32 type; double r[8]; double c[8]; double rI, rF, cF, cAmp, vRef, gain; };

struct d_mixer {
	const mixer_info *info;
	INT32 size, type;
	double r_total, v_cap[8], exponent_rc[8], v_cap_f, exponent_c_f, v_cap_amp, exponent_c_amp, gain_ri;

	void reset(const mixer_info *i, INT32 inputs) {
		info = i; size = inputs;
		type = i->type;
		if (type == MIXER_IS_OP_AMP && i->rI != 0) type = 2; // op amp with rI
		r_total = 0;
		for (INT32 b = 0; b < size; b++) {
			if (i->r[b] != 0) r_total += 1.0 / i->r[b];
			v_cap[b] = 0; exponent_rc[b] = 0;
			if (i->c[b] != 0) {
				double rt = i->r[b];
				if (type == MIXER_IS_RESISTOR && i->rF != 0) rt = 1.0 / ((1.0 / i->r[b]) + (1.0 / i->rF));
				else if (type == 2) rt = i->r[b] + i->rI;
				exponent_rc[b] = rc_exp(rt * i->c[b]);
			}
		}
		if (i->rF != 0 && type == MIXER_IS_RESISTOR) r_total += 1.0 / i->rF;
		if (type == 2) r_total += 1.0 / i->rI;
		r_total = 1.0 / r_total;
		v_cap_f = 0; exponent_c_f = 0;
		if (i->cF != 0) exponent_c_f = rc_exp(((i->type == MIXER_IS_OP_AMP) ? i->rF : r_total) * i->cF);
		v_cap_amp = 0; exponent_c_amp = 0;
		if (i->cAmp != 0) exponent_c_amp = rc_exp(RES_K(100) * i->cAmp);
		if (type == 2) gain_ri = i->rF / i->rI;
	}
	double step(INT32 enable, const double *in) {
		if (!enable) return 0;
		double i = 0, v;
		for (INT32 b = 0; b < size; b++) {
			double vt = in[b];
			if (info->c[b] != 0) {
				v_cap[b] += (vt - info->vRef - v_cap[b]) * exponent_rc[b];
				vt -= v_cap[b];
			}
			i += ((type == MIXER_IS_OP_AMP) ? info->vRef - vt : vt) / info->r[b];
		}
		if (type == 2) i += info->vRef / info->rI;
		v = i * ((type == MIXER_IS_OP_AMP) ? info->rF : r_total);
		if (type == 2) v = info->vRef + (gain_ri * (info->vRef - v));
		if (info->cF != 0) { v_cap_f += (v - info->vRef - v_cap_f) * exponent_c_f; v = v_cap_f; }
		if (info->cAmp != 0) { v_cap_amp += (v - v_cap_amp) * exponent_c_amp; v -= v_cap_amp; }
		return v * info->gain;
	}
};

struct d_dac_r1 {
	double v_step[256], v_out, exponent;
	INT32 has_c;

	void reset(INT32 len, const double *r, double rBias, double vBias, double rGnd, double cFilter, double v_on) {
		double r_total = 0, i_bias = (rBias > 0) ? vBias / rBias : 0;
		for (INT32 b = 0; b < len; b++) if (r[b] > 0) r_total += 1.0 / r[b];
		if (rBias > 0) r_total += 1.0 / rBias;
		if (rGnd > 0) r_total += 1.0 / rGnd;
		r_total = 1.0 / r_total;
		v_out = 0;
		has_c = (cFilter > 0);
		if (has_c) exponent = rc_exp(r_total * cFilter);
		for (INT32 n = 0; n < (1 << len); n++) {
			double it = i_bias;
			for (INT32 b = 0; b < len; b++) if (r[b] > 0 && ((n >> b) & 1)) it += v_on / r[b];
			v_step[n] = it * r_total;
		}
	}
	double step(INT32 data) {
		double v = v_step[data & 0xff];
		if (has_c) v_out += (v - v_out) * exponent; else v_out = v;
		return v_out;
	}
};

struct d_note {
	double t_left, t_clock;
	INT32 count1, count2, max1, max2;

	void reset(double clock, INT32 data, INT32 m1, INT32 m2) {
		t_left = 0; t_clock = 1.0 / clock;
		count1 = data; count2 = 0; max1 = m1; max2 = m2;
	}
	INT32 step(INT32 enable, INT32 data) {
		double cycles = (t_left + disc_dt) / t_clock;
		INT32 inc = (INT32)cycles;
		t_left = (cycles - inc) * t_clock;
		if (!enable) return 0;
		if (data != max1) {
			for (INT32 c = 0; c < inc; c++) {
				if (++count1 > max1) {
					count1 = data;
					if (++count2 > max2) count2 = 0;
				}
			}
		}
		return count2;
	}
};

struct d_squarewfix {
	double t_left, t_on, t_off;
	INT32 flip_flop;

	void reset(double freq, double duty, double phase) {
		flip_flop = 1;
		t_off = 1.0 / freq;
		t_left = phase / 360.0; t_left -= (INT32)t_left; if (t_left < 0) t_left += 1.0;
		t_left *= t_off;
		t_on = t_off * (duty / 100.0);
		t_off -= t_on;
	}
	double step(INT32 enable, double amp, double bias) {
		t_left -= disc_dt;
		while (t_left <= 0) { flip_flop ^= 1; t_left += flip_flop ? t_on : t_off; }
		if (!enable) return 0;
		return (flip_flop ? amp / 2.0 : -(amp / 2.0)) + bias;
	}
};

#define OPF_HIGH_PASS_0	0x40
#define OPF_BAND_PASS_0	0x50

struct opfilt_info { double r1, r2, r3, r4, rF, c1, c2, c3, vRef, vP, vN; };

struct d_opamp_filt {
	const opfilt_info *info;
	INT32 type;
	double rTotal, iFixed, vP, vN, vC1, vC2, vC3, exponentC1, exponentC2, exponentC3;

	void reset(const opfilt_info *i, INT32 t) {
		info = i; type = t;
		rTotal = i->r1;
		if (type == OPF_BAND_PASS_0) rTotal += i->r2 + i->r3;
		iFixed = (i->vP - OP_AMP_NORTON_VBE) / i->r4;
		vP = i->vP - OP_AMP_NORTON_VBE;
		vN = i->vN;
		if (type == OPF_BAND_PASS_0) {
			exponentC1 = rc_exp(RES_2_PARALLEL(i->r1, i->r2 + i->r3 + i->r4) * i->c1);
			exponentC2 = rc_exp(RES_2_PARALLEL(i->r1 + i->r2, i->r3 + i->r4) * i->c2);
			exponentC3 = rc_exp((i->r1 + i->r2 + i->r3 + i->r4) * i->c3);
		} else {
			exponentC1 = rc_exp(i->r1 * i->c1);
		}
		vC1 = vC2 = vC3 = 0;
	}
	double step(double in) {
		double v = in - OP_AMP_NORTON_VBE, v_out, i;
		if (v < 0) v = 0;
		if (type == OPF_BAND_PASS_0) {
			vC1 += (v - vC1) * exponentC1;
			vC2 += (vC1 - vC2) * exponentC2;
			v = vC2;
			v_out = v - vC3;
			vC3 += (v - vC3) * exponentC3;
		} else {
			v_out = v - vC1;
			vC1 += (v - vC1) * exponentC1;
		}
		i = v_out / rTotal;
		v_out = (iFixed - i) * info->rF;
		if (v_out > vP) v_out = vP;
		if (v_out < vN) v_out = vN;
		return v_out;
	}
};

struct integrate_info { double r1, r2, r3, c, v1, vP; INT32 f0, f1, f2; };

struct d_integrate {
	const integrate_info *info;
	double v_max_out, v_max_in, v_max_in_d, v_out;

	void reset(const integrate_info *i) {
		info = i;
		v_max_out = i->vP - OP_AMP_NORTON_VBE;
		v_max_in = i->v1 - OP_AMP_NORTON_VBE;
		v_max_in_d = v_max_in - OP_AMP_NORTON_VBE;
		v_out = 0;
	}
	double step(INT32 trig0, INT32 trig1) {
		double i_neg = trig_func(trig0, trig1, 0, info->f0) ? v_max_in_d / info->r1 : 0;
		double i_pos = trig_func(trig0, trig1, 0, info->f1) ? v_max_in / info->r2 : 0;
		i_pos += trig_func(trig0, trig1, 0, info->f2) ? v_max_in_d / info->r3 : 0;
		v_out += (i_pos - i_neg) * disc_dt / info->c;
		if (v_out < 0) v_out = 0;
		if (v_out > v_max_out) v_out = v_max_out;
		return v_out;
	}
};

struct d_bitdac {
	INT32 level;
	double v;
	void reset() { level = 0; v = 0; }
	double step() { v += ((level ? 1.0 : 0.0) - v) * 0.5; return v; }
};

#define OVL(name, w, h, rot) static const ovl_layout name##_overlay = { name##_ovl, sizeof(name##_ovl) / sizeof(name##_ovl[0]), w, h, rot };

static const ovl_rect invaders_ovl[] = {
	{   0,   0, 224, 260, 1.0f,   1.0f,  1.0f   },
	{   0, 184, 224, 240, 0.125f, 1.0f,  0.125f },
	{  16, 240, 134, 260, 0.125f, 1.0f,  0.125f },
	{   0,  32, 224,  62, 1.0f,   0.125f, 0.125f },
};
OVL(invaders, 224, 260, 1)

static const ovl_rect cosmicm_ovl[] = {
	{   0,   0, 224, 260, 1.0f,  0.55f, 1.0f  },
	{   0, 184, 224, 260, 1.0f,  1.0f,  0.15f },
	{   0,   0, 224,  63, 0.15f, 1.0f,  0.15f },
};
OVL(cosmicm, 224, 260, 1)

static const ovl_rect escmars_ovl[] = {
	{   0,   0, 224,  40, 0.125f, 1.0f,   0.125f  },
	{   0,  40, 224, 180, 1.0f,   0.5f,   0.0625f },
	{   0, 180, 224, 240, 1.0f,   0.125f, 0.125f  },
	{   0, 240, 224, 260, 0.125f, 1.0f,   0.125f  },
};
OVL(escmars, 224, 260, 1)

static const ovl_rect galactic_ovl[] = {
	{   0,   0, 224, 260, 0.0f, 1.0f, 0.4f  },
	{   0,  47, 224,  70, 1.0f, 0.2f, 0.15f },
	{   0,  70, 224, 192, 1.0f, 1.0f, 0.2f  },
	{   0, 192, 224, 214, 0.2f, 0.6f, 1.0f  },
};
OVL(galactic, 224, 260, 1)

static const ovl_rect shuttlei_ovl[] = {
	{   0,     0, 224, 182.5f, 0.125f, 1.0f,   0.125f  },
	{   0, 182.5f, 224, 199.5f, 1.0f,   0.5f,   0.0625f },
	{   0, 199.5f, 224, 256,    1.0f,   0.125f, 0.125f  },
};
OVL(shuttlei, 224, 256, 1)

static const ovl_rect spacecom_ovl[] = {
	{   0,   0, 224, 256, 0.1f, 1.0f, 0.2f },
	{   0,  54, 224, 212, 1.0f, 0.1f, 0.1f },
	{   0,  88, 224, 182, 1.0f, 1.0f, 0.1f },
	{   0, 120, 224, 152, 0.1f, 0.2f, 1.0f },
};
OVL(spacecom, 224, 256, 1)

static const ovl_rect yosakdon_ovl[] = {
	{   0,   0,  80,  17, 0.125f, 1.0f,  1.0f  },
	{ 100,   0, 224,  17, 1.0f,   0.2f,  1.0f  },
	{  75,   0, 146,  40, 0.25f,  1.0f,  0.2f  },
	{   0,  33, 224,  60, 0.25f,  1.0f,  0.2f  },
	{   0,  56, 224,  96, 0.3f,   0.4f,  1.0f  },
	{   0,  96, 224, 130, 0.125f, 1.0f,  1.0f  },
	{   0, 128, 224, 160, 1.0f,   1.0f,  0.1f  },
	{   0, 159, 224, 192, 1.0f,   0.2f,  1.0f  },
	{   0, 191, 224, 216, 1.0f,   0.25f, 0.25f },
	{   0, 215, 224, 259, 0.125f, 1.0f,  1.0f  },
	{ 131, 238, 186, 259, 1.0f,   1.0f,  0.125f },
	{   0, 232, 224, 239, 1.0f,   0.25f, 0.25f },
};
OVL(yosakdon, 224, 260, 1)

static const ovl_rect invad2ct_ovl[] = {
	{   0,     0, 224, 27.5f, 1.0f,   1.0f, 0.125f },
	{   0, 27.5f, 224,  52,   0.5f,   1.0f, 0.0f   },
	{   0,  52,   224,  75,   0.0f,   1.0f, 0.5f   },
	{   0,  75,   224, 121,   0.125f, 1.0f, 1.0f   },
	{   0, 121,   224, 144,   0.0f,   1.0f, 0.5f   },
	{   0, 144,   224, 167,   0.125f, 1.0f, 0.125f },
	{   0, 167,   224, 190,   0.5f,   1.0f, 0.0f   },
	{   0, 190,   224, 213,   1.0f,   1.0f, 0.125f },
	{   0, 213,   224, 236,   1.0f,   0.5f, 0.0f   },
	{   0, 236,   224, 260,   1.0f,   0.125f, 0.125f },
};
OVL(invad2ct, 224, 260, 2)

static const ovl_rect attackfc_ovl[] = {
	{ 0, 0, 1, 1, 1.0f, 0.9f, 0.1f },
};
OVL(attackfc, 1, 1, 0)

static const ovl_rect seawolf_ovl[] = {
	{   0,   0,   1,   1, 0.25f, 0.5f,  1.0f  },
};
OVL(seawolf, 1, 1, 0)

static const ovl_rect gunfight_ovl[] = {
	{   0,   0,   1,   1, 1.0f,  1.0f,  0.125f },
};
OVL(gunfight, 1, 1, 0)

static const ovl_rect phantom2_ovl[] = {
	{   0,   0,   1,   1, 0.625f, 0.625f, 1.0f },
};
OVL(phantom2, 1, 1, 0)

static const ovl_rect zzzap_ovl[] = {
	{   0,   0, 260, 224, 1.0f,  1.0f,  1.0f  },
	{   0, 181, 260, 224, 0.2f,  0.4f,  1.0f  },
};
OVL(zzzap, 260, 224, 0)

static const ovl_rect clowns_ovl[] = {
	{   0,   0, 256,  29, 1.0f,   1.0f,   1.0f   },
	{   0,  29, 256,  48, 0.212f, 0.647f, 0.914f },
	{   0,  48, 256,  67, 0.322f, 1.0f,   0.322f },
	{   0,  67, 256,  86, 1.0f,   1.0f,   0.322f },
	{   0,  86, 256, 224, 1.0f,   1.0f,   1.0f   },
};
OVL(clowns, 256, 224, 0)

static const ovl_rect spacwalk_ovl[] = {
	{   0,   0, 260, 224, 1.0f, 1.0f, 1.0f  },
	{   0, 158, 131, 181, 0.2f, 0.5f, 1.0f  },
	{ 131, 158, 260, 181, 0.0f, 0.9f, 0.5f  },
	{   0, 181, 131, 207, 0.0f, 0.9f, 0.5f  },
	{ 131, 181, 260, 207, 0.2f, 0.5f, 1.0f  },
	{   0, 207,  47, 224, 0.2f, 0.5f, 1.0f  },
	{  47, 207, 122, 224, 1.0f, 1.0f, 0.25f },
	{ 122, 207, 141, 224, 1.0f, 0.2f, 0.15f },
	{ 141, 207, 218, 224, 1.0f, 1.0f, 0.25f },
	{ 218, 207, 260, 224, 0.0f, 0.9f, 0.5f  },
};
OVL(spacwalk, 260, 224, 0)

static const ovl_rect maze_ovl[] = {
	{ 0, 0, 1, 1, 0.2f, 1.0f, 0.5f },
};
OVL(maze, 1, 1, 0)

static const ovl_rect gunchamp_ovl[] = {
	{ 0,   0, 400, 300, 1.0f,  0.15f, 0.15f },
	{ 0,  36, 400, 300, 1.0f,  0.29f, 0.15f },
	{ 0,  38, 400, 300, 1.0f,  0.43f, 0.15f },
	{ 0,  40, 400, 300, 1.0f,  0.57f, 0.15f },
	{ 0,  42, 400, 300, 1.0f,  0.71f, 0.15f },
	{ 0,  44, 400, 300, 1.0f,  0.85f, 0.15f },
	{ 0,  46, 400, 300, 1.0f,  1.0f,  0.15f },
	{ 0, 165, 400, 300, 0.85f, 0.85f, 0.29f },
	{ 0, 167, 400, 300, 0.71f, 0.71f, 0.43f },
	{ 0, 169, 400, 300, 0.57f, 0.57f, 0.57f },
	{ 0, 171, 400, 300, 0.43f, 0.43f, 0.71f },
	{ 0, 173, 400, 300, 0.29f, 0.29f, 0.85f },
	{ 0, 175, 400, 300, 0.15f, 0.15f, 1.0f  },
};
OVL(gunchamp, 400, 300, 0)

static void overlay_build()
{
	memset(ovl_map, 0, 260 * 256);
	ovl_count = 1;
	ovl_rgb[0] = 0xffffff;

	if (overlay == NULL) return;

	INT32 nw = nScreenWidth, nh = nScreenHeight;
	INT32 rot = 0;
	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) rot = (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) ? 2 : 1;

	for (INT32 ny = 0; ny < nh; ny++)
	{
		for (INT32 nx = 0; nx < nw; nx++)
		{
			float ex, ey;

			switch (rot) {
				case 1:  ex = (ny + 0.5f) / nh * overlay->w; ey = ((nw - 1 - nx) + 0.5f) / nw * overlay->h; break;
				case 2:  ex = ((nh - 1 - ny) + 0.5f) / nh * overlay->w; ey = (nx + 0.5f) / nw * overlay->h; break;
				default: ex = (nx + 0.5f) / nw * overlay->w; ey = (ny + 0.5f) / nh * overlay->h; break;
			}

			float r = 1.0f, g = 1.0f, b = 1.0f;
			for (INT32 i = 0; i < overlay->count; i++) {
				const ovl_rect *p = &overlay->rects[i];
				if (ex >= p->x0 && ex < p->x1 && ey >= p->y0 && ey < p->y1) { r = p->r; g = p->g; b = p->b; }
			}

			UINT32 rgb = ((INT32)(r * 255 + 0.5f) << 16) | ((INT32)(g * 255 + 0.5f) << 8) | (INT32)(b * 255 + 0.5f);

			INT32 idx;
			for (idx = 0; idx < ovl_count; idx++) if (ovl_rgb[idx] == rgb) break;
			if (idx == ovl_count && ovl_count < 16) ovl_rgb[ovl_count++] = rgb;
			if (idx >= 16) idx = 0;

			ovl_map[ny * nw + nx] = idx;
		}
	}
}

// Palettes
static UINT32 base_palette[256];
static INT32 base_palette_count;

static void palette_init_base()
{
	switch (cfg->palette)
	{
		case 0: // RBG_3BIT
			for (INT32 i = 0; i < 8; i++) base_palette[i] = ((i & 1) ? 0xff0000 : 0) | ((i & 4) ? 0x00ff00 : 0) | ((i & 2) ? 0x0000ff : 0);
			base_palette_count = 8;
		break;

		case 1: // RGB_3BIT
			for (INT32 i = 0; i < 8; i++) base_palette[i] = ((i & 1) ? 0xff0000 : 0) | ((i & 2) ? 0x00ff00 : 0) | ((i & 4) ? 0x0000ff : 0);
			base_palette_count = 8;
		break;

		case 2: // monochrome
			base_palette[0] = 0; base_palette[1] = 0xffffff;
			base_palette_count = 2;
		break;

		case 3: // rollingc: 3bpp + intensity
			for (INT32 i = 0; i < 8; i++) {
				base_palette[i] = ((i & 4) ? 0x7f0000 : 0) | ((i & 2) ? 0x007f00 : 0) | ((i & 1) ? 0x00007f : 0);
				base_palette[i | 8] = ((i & 4) ? 0xff0000 : 0) | ((i & 2) ? 0x00ff00 : 0) | ((i & 1) ? 0x0000ff : 0);
			}
			base_palette[5] = 0xff0080; // pink
			base_palette[6] = 0xff8000; // orange
			base_palette_count = 16;
		break;

		case 6: // 256 level gray ramp (spcenctr / phantom2)
			for (INT32 i = 0; i < 256; i++) base_palette[i] = (i << 16) | (i << 8) | i;
			base_palette_count = 256;
		break;

		case 4: // cosmo
		{
			for (INT32 i = 0; i < 8; i++) base_palette[i] = ((i & 1) ? 0xff0000 : 0) | ((i & 4) ? 0x00ff00 : 0) | ((i & 2) ? 0x0000ff : 0);
			static const UINT8 starmap[4] = { 0, 80, 80, 255 };
			for (INT32 i = 0; i < 64; i++) {
				UINT8 r = starmap[(((i >> 4) & 1) << 1) | ((i >> 1) & 1)];
				UINT8 g = starmap[(((i >> 5) & 1) << 1) | ((i >> 0) & 1)];
				UINT8 b = starmap[(((i >> 3) & 1) << 1) | ((i >> 2) & 1)];
				base_palette[i + 8] = (r << 16) | (g << 8) | b;
			}
			base_palette_count = 72;
		}
		break;
	}
}

static void palette_update()
{
	INT32 use_ovl = (overlay != NULL) && (DrvDips[4] & 1) && !BurnArtworkActive();

	for (INT32 c = 0; c < base_palette_count; c++)
	{
		for (INT32 o = 0; o < 16; o++)
		{
			UINT32 p = base_palette[c];
			INT32 r = (p >> 16) & 0xff, g = (p >> 8) & 0xff, b = p & 0xff;

			if (use_ovl && o < ovl_count) {
				UINT32 m = ovl_rgb[o];
				r = r * ((m >> 16) & 0xff) / 255;
				g = g * ((m >>  8) & 0xff) / 255;
				b = b * ((m >>  0) & 0xff) / 255;
			}

			DrvPalette[(c << 4) | o] = BurnHighCol(r, g, b, 0);
		}
	}
}

// Video
static inline void set_pixel(INT32 x, INT32 y, INT32 color)
{
	if (flipscreen) { x = nScreenWidth - 1 - x; y = nScreenHeight - 1 - y; }
	INT32 offs = y * nScreenWidth + x;
	pTransDraw[offs] = (color << 4) | ovl_map[offs];
}

static inline void set_8_pixels(INT32 y, INT32 x, UINT8 data, INT32 fore, INT32 back)
{
	for (INT32 i = 0; i < 8; i++, x++, data >>= 1)
		set_pixel(x, y, (data & 1) ? fore : back);
}

static void clear_extra_columns(INT32 color)
{
	for (INT32 y = 0; y < 224; y++)
		for (INT32 x = 256; x < 260; x++)
			set_pixel(x, y, color);
}

static void draw_mw()
{
	for (INT32 y = 0x20; y < 0x100; y++)
	{
		UINT8 video_data = 0;
		INT32 sy = y - 0x20;

		for (INT32 x = 0; x < 256; x++)
		{
			if ((x & 7) == 4) video_data = DrvMainRAM[((y << 5) | (x >> 3)) & 0x1fff];
			set_pixel(x, sy, video_data & 1);
			video_data >>= 1;
		}

		for (INT32 i = 0; i < 4; i++, video_data >>= 1)
			set_pixel(256 + i, sy, video_data & 1);
	}
}

static void draw_vortex()
{
	for (INT32 y = 0x20; y < 0x100; y++)
	{
		UINT8 video_data = 0;
		INT32 col = 0, sy = y - 0x20;

		for (INT32 x = 0; x < 256; x++)
		{
			if ((x & 7) == 4) {
				INT32 offs = (y << 5) | (x >> 3);
				video_data = DrvMainRAM[offs & 0x1fff];
				INT32 pix = DrvMainRAM[(offs + 1) & 0x1fff] & 1;
				col = (pix ? 0 : 1) | (pix ? 2 : 0) | ((x & 0x20) ? 4 : 0); // RGB_3BIT: r = !pix, g = pix, b = bit 5 of x
			}
			set_pixel(x, sy, (video_data & 1) ? col : 0);
			video_data >>= 1;
		}

		for (INT32 i = 0; i < 4; i++, video_data >>= 1)
			set_pixel(256 + i, sy, (video_data & 1) ? col : 0);
	}
}

static void draw_color_prom(INT32 back, INT32 use_red)
{
	UINT8 *prom = DrvColorPROM + (color_map ? 0x400 : 0);

	for (INT32 offs = 0x400; offs < 0x2000; offs++)
	{
		INT32 y = (offs >> 5) - 0x20;
		INT32 x = (offs << 3) & 0xff;
		INT32 fore = (use_red && screen_red) ? 1 : (prom[((offs >> 8) << 5) | (offs & 0x1f)] & 7);
		set_8_pixels(y, x, DrvMainRAM[offs], fore, back);
	}

	clear_extra_columns(back);
}

static void draw_schaser()
{
	for (INT32 offs = 0x400; offs < 0x2000; offs++)
	{
		INT32 y = (offs >> 5) - 0x20;
		INT32 x = (offs << 3) & 0xff;
		INT32 back = 0;
		INT32 fore = DrvColorRAM[(offs & 0x1f) | ((offs & 0x1f80) >> 2)] & 7;

		if (!schaser_background_disable) {
			UINT8 back_data = DrvColorPROM[((offs >> 8) << 5) | (offs & 0x1f)];
			back = (((back_data & 0x0c) == 0x0c) && schaser_background_select) ? 4 : 2;
		}

		set_8_pixels(y, x, DrvMainRAM[offs], fore, back);
	}

	clear_extra_columns(schaser_background_disable ? 0 : 2);
}

static void draw_scattered(INT32 invert, INT32 back)
{
	for (INT32 offs = 0x400; offs < 0x2000; offs++)
	{
		INT32 y = (offs >> 5) - 0x20;
		INT32 x = (offs << 3) & 0xff;
		INT32 fore = (DrvColorRAM[(offs & 0x1f) | ((offs & 0x1f80) >> 2)] ^ invert) & 7;
		set_8_pixels(y, x, DrvMainRAM[offs], fore, back);
	}

	clear_extra_columns(back);
}

static void draw_polaris()
{
	for (INT32 offs = 0x400; offs < 0x2000; offs++)
	{
		INT32 y = (offs >> 5);
		INT32 sy = y - 0x20;
		INT32 x = (offs << 3) & 0xff;
		UINT8 data = DrvMainRAM[offs];
		INT32 color_address = ((offs >> 8) << 5) | (offs & 0x1f);
		INT32 back = (DrvColorPROM[color_address] & 0x01) ? 6 : 2;
		INT32 fore = ~DrvColorRAM[(offs & 0x1f) | ((offs & 0x1f80) >> 2)] & 7;
		UINT8 cloud_y = y - polaris_cloud_pos;

		if ((DrvColorPROM[color_address] & 0x08) || (cloud_y >= 64))
		{
			set_8_pixels(sy, x, data, fore, back);
		}
		else
		{
			for (INT32 i = 0; i < 8; i++, x++, data >>= 1)
			{
				INT32 color;
				if (data & 1) {
					color = fore;
				} else {
					INT32 bit = 1 << (~x & 3);
					INT32 cloud_offs = ((x >> 2) & 3) | ((~cloud_y & 0x3f) << 2);
					color = (DrvUser1[cloud_offs] & bit) ? 7 : back;
				}
				set_pixel(x, sy, color);
			}
		}
	}

	clear_extra_columns(6);
}

static void draw_rollingc()
{
	for (INT32 offs = 0x400; offs < 0x2000; offs++)
	{
		INT32 y = (offs >> 5) - 0x20;
		INT32 x = (offs << 3) & 0xff;
		INT32 idx = (offs & 0x1f) | ((offs & 0x1f00) >> 3);
		set_8_pixels(y, x, DrvMainRAM[offs], DrvColorRAM[idx] & 0x0f, DrvColorRAM2[idx] & 0x0f);
	}

	clear_extra_columns(0);
}

static void draw_shuttlei()
{
	for (INT32 offs = 0; offs < 0x1800; offs++)
	{
		INT32 y = offs >> 5;
		INT32 x = (offs << 3) & 0xff;
		UINT8 data = DrvMainRAM[offs];

		for (INT32 i = 0; i < 8; i++, data <<= 1)
			set_pixel(x | i, y, (data >> 7) & 1);
	}
}

static void draw_spacecom()
{
	for (INT32 offs = 0; offs < 0x1c00; offs++)
	{
		INT32 y = offs >> 5;
		INT32 x = (offs << 3) & 0xff;
		INT32 flipx = flipscreen ? 7 : 0;
		UINT8 data = DrvMainRAM[offs + 0x400];

		for (INT32 i = 0; i < 8; i++, data >>= 1) {
			INT32 o = y * nScreenWidth + (x | (i ^ flipx));
			pTransDraw[o] = ((data & 1) << 4) | ovl_map[o];
		}
	}
}

static void cosmo_stars_init()
{
	UINT32 shiftreg = 0;

	for (INT32 i = 0; i < STAR_RNG_PERIOD; i++)
	{
		INT32 enabled = ((shiftreg & 0x0fe00) == 0x0fe00);
		INT32 color = (~shiftreg & 0x1f8) >> 3;
		DrvStars[i] = color | (enabled << 7);
		shiftreg = (shiftreg >> 1) | ((((shiftreg >> 12) ^ ~shiftreg) & 1) << 16);
	}
}

static void draw_cosmo()
{
	for (INT32 offs = 0x400; offs < 0x2000; offs++)
	{
		INT32 y = (offs >> 5) - 0x20;
		INT32 x = (offs << 3) & 0xff;
		INT32 fore = DrvColorRAM[((offs >> 8) << 5) | (offs & 0x1f)] & 7;
		set_8_pixels(y, x, DrvMainRAM[offs], fore, 0);
	}
	clear_extra_columns(0);

	// stars
	INT32 per_frame_delta = 2 - cosmo_star_speed;
	if (cosmo_stars_sidescroll) per_frame_delta += 4096;
	INT32 total = per_frame_delta;
	while (total < 0) total += STAR_RNG_PERIOD;
	cosmo_star_origin = (cosmo_star_origin + total) % STAR_RNG_PERIOD;

	for (INT32 y = 0; y < 224; y++)
	{
		UINT32 star_offs = (cosmo_star_origin + y * 1024) % STAR_RNG_PERIOD;

		for (INT32 x = 0; x < 1024; x++)
		{
			if (y & 1) { if ((x % 32) > 15) cosmo_bright_star = 0; }
			else { if ((x % 32) < 16) cosmo_bright_star = 0; }

			UINT8 star = DrvStars[star_offs++];
			if (star_offs >= STAR_RNG_PERIOD) star_offs = 0;

			if (star & 0x80) {
				if ((x & 3) == 0) {
					INT32 sx = x >> 2;
					if (flipscreen) sx = nScreenWidth - 1 - sx;
					INT32 sy = flipscreen ? (nScreenHeight - 1 - y) : y;
					UINT16 *p = &pTransDraw[sy * nScreenWidth + sx];
					if ((*p >> 4) == 0)
						*p = ((cosmo_bright_star ? (star & 0x3f) : (star & 0x0f)) + 8) << 4;
				}
			} else {
				cosmo_bright_star ^= 1;
			}
		}
	}
}

// Space Encounters: trench + "brite" strobe, 256 level gray (palette 6)
static void draw_spcenctr()
{
	UINT8 line_buf[256];
	UINT8 x = 0, y = 0x20, video_data = 0, draw_line = 0, draw_trench = 0, draw_floor = 0;
	UINT8 width = trench_width, floor_width = width, center = trench_center;

	memset(line_buf, 0, sizeof(line_buf));

	if (bright_control) brightness = 255;
	else if (brightness > 10) brightness -= 10;
	else brightness = 0;

	while (1)
	{
		UINT8 bit = video_data & 1;
		UINT8 pen = bit ? 0xff : 0x00;

		if (!(width & 0x80) && draw_trench) {
			line_buf[x] = draw_line;
			if (!bit) pen = draw_line ? 0xe6 : 0x4d;
		} else if (!(floor_width & 0x80) && (draw_trench || draw_floor)) {
			if (!bit) pen = line_buf[x] ? 0x80 : 0x1a;
		} else if (draw_floor) {
			line_buf[x] = line_buf[(UINT8)(x - 1)];
			if (!bit) pen = line_buf[x] ? 0x66 : 0x0d;
		}

		if (brightness > pen) pen = brightness;

		set_pixel(x, y - 0x20, pen);

		center++;
		width += (center & 0x80) ? -1 : 1;
		floor_width += (center & 0x80) ? -1 : 1;
		video_data >>= 1;
		x++;

		if (x == 0)
		{
			for (INT32 i = 0; i < 4; i++) {
				pen = (video_data & 1) ? 0xff : brightness;
				set_pixel(256 + i, y - 0x20, pen);
				video_data >>= 1;
			}

			UINT8 trench_control = DrvMainRAM[((y << 5) | 0x1f) & 0x1fff];
			if (trench_control & 0x40) draw_trench = 1;
			if (trench_control & 0x20) draw_trench = 0;
			if (trench_control & 0x10) draw_floor = 1;
			if (trench_control & 0x08) draw_floor = 0;
			draw_line = (trench_control & 0x80) >> 7;

			if (draw_trench) width += trench_slope[y & 0x0f] & 0x03;
			if (draw_floor) floor_width += (trench_slope[y & 0x0f] & 0x0c) >> 2;

			y++;
			if (y == 0) break;
		}
		else if ((x & 7) == 4) {
			video_data = DrvMainRAM[((y << 5) | (x >> 3)) & 0x1fff];
		}
	}
}

// Phantom II: cloud generator from the prom, palette 6 (cloud pen 0xc0)
#define PHANTOM2_CLOUD_COUNTER_START	0x0e0b
#define PHANTOM2_CLOUD_COUNTER_END		0x1000

static void draw_phantom2()
{
	UINT8 x = 0, y = 0x20, video_data = 0, cloud_data = 0;
	UINT16 cloud_counter = phantom2_cloud_counter;

	while (1)
	{
		UINT8 bit = video_data & 1;
		UINT8 cloud_to_load = 0;
		INT32 pen = ((bit == 0) && (cloud_data & 1)) ? 0xc0 : (bit ? 0xff : 0x00);

		set_pixel(x, y - 0x20, pen);

		INT32 load = ((x & 0x0f) == 0x0f);
		if (load) cloud_to_load = DrvColorPROM[(((cloud_counter & 0xfe) << 3) | (x >> 4)) & 0x7ff];

		video_data >>= 1;
		x++;

		if (!(x & 1)) cloud_data = load ? cloud_to_load : (cloud_data >> 1);

		if (x == 0)
		{
			for (INT32 i = 0; i < 4; i++) {
				set_pixel(256 + i, y - 0x20, (video_data & 1) ? 0xff : 0);
				video_data >>= 1;
			}

			cloud_counter++;
			if (cloud_counter == PHANTOM2_CLOUD_COUNTER_END) cloud_counter = PHANTOM2_CLOUD_COUNTER_START;

			y++;
			if (y == 0) break;
		}
		else if ((x & 7) == 4) {
			video_data = DrvMainRAM[((y << 5) | (x >> 3)) & 0x1fff];
		}
	}
}

static INT32 DrvDraw()
{
	palette_update();

	switch (cfg->video)
	{
		case VID_MW:		draw_mw(); break;
		case VID_VORTEX:	draw_vortex(); break;
		case VID_INVADPT2:	draw_color_prom(0, 1); break;
		case VID_BALLBOMB:	draw_color_prom(2, 1); break;
		case VID_INDIANBT:	draw_color_prom(0, 0); break;
		case VID_SCHASER:	draw_schaser(); break;
		case VID_SCHASERCV:	draw_scattered(0, 2); break;
		case VID_LUPIN3:	draw_scattered(0xff, 0); break;
		case VID_SFLUSH:	draw_scattered(0, 0); break;
		case VID_POLARIS:	draw_polaris(); break;
		case VID_ROLLINGC:	draw_rollingc(); break;
		case VID_COSMO:		draw_cosmo(); break;
		case VID_SHUTTLEI:	draw_shuttlei(); break;
		case VID_SPACECOM:	draw_spacecom(); break;
		case VID_SPCENCTR:	draw_spcenctr(); break;
		case VID_PHANTOM2:	draw_phantom2(); break;
	}

	BurnTransferCopy(DrvPalette);

	BurnGunDrawTargets();

	return 0;
}

// Midway / Taito "Space Invaders" discrete sound board
static const osc_info inv_saucer_hit_osc = { OSC_1 | OSC_NORTON | OSC_OUT_CAP, RES_M(1), RES_K(470), RES_K(100), RES_K(120), RES_M(1), 0, 0, 0, CAP_U(0.1), 12 };
static const osc_info inv_saucer_hit_vco = { OSC_VCO_1 | OSC_NORTON | OSC_OUT_SQW, RES_M(1), RES_K(470), RES_K(680), RES_M(1), RES_M(1), 0, 0, 0, CAP_P(470), 12 };
static const opamp_info inv_saucer_hit_B3_9  = { 0, RES_K(100), RES_M(1), 0, CAP_U(1), 0, 12 };
static const opamp_info inv_saucer_hit_B3_10 = { RES_K(680), RES_K(680), RES_M(2.7), RES_K(680), 0, 0, 12 };

static const opamp_info inv_invader_hit_D3_10 = { 0, RES_K(10), RES_M(1), 0, CAP_U(0.47), 0, 12 };
static const osc_info inv_invader_hit_vco = { OSC_VCO_1 | OSC_NORTON | OSC_OUT_CAP, RES_M(1), RES_K(470), RES_K(680), RES_M(1), RES_M(1), 0, 0, 0, CAP_P(330), 12 };
static const opamp_info inv_invader_hit_D3_4 = { RES_K(470), RES_K(680), RES_M(2.7), RES_K(680), 0, 0, 12 };
static const opamp_1sht_info inv_invader_hit_1sht[3] = {
	{ RES_M(4.7), RES_K(100), RES_M(1), RES_M(1), RES_M(2.2), CAP_U(0.1),  CAP_P(470), 0, 12 },	// invaders
	{ RES_M(4.7), RES_K(100), RES_M(1), RES_M(1), RES_M(2.2), CAP_U(0.22), CAP_P(470), 0, 12 },	// invad2ct board 1
	{ RES_M(4.7), RES_K(100), RES_M(1), RES_M(1), RES_M(2.2), CAP_U(1),    CAP_P(470), 0, 12 },	// invad2ct board 2
};
static const osc_info inv_invader_hit_osc[3] = {
	{ OSC_1 | OSC_NORTON | OSC_OUT_CAP, RES_M(1), RES_K(10), RES_K(100), RES_K(120), RES_M(1), 0, 0, 0, CAP_U(0.1),  12 },
	{ OSC_1 | OSC_NORTON | OSC_OUT_CAP, RES_M(1), RES_K(10), RES_K(100), RES_K(120), RES_M(1), 0, 0, 0, CAP_U(0.22), 12 },
	{ OSC_1 | OSC_NORTON | OSC_OUT_CAP, RES_M(1), RES_K(10), RES_K(100), RES_K(120), RES_M(1), 0, 0, 0, CAP_U(0.1),  12 },
};

static const opamp_1sht_info inv_explosion_1sht = { RES_M(4.7), RES_K(100), RES_M(1), RES_M(1), RES_M(2.2), CAP_U(2.2), CAP_P(470), 0, 12 };
static const tvca_info inv_explosion_tvca = { RES_M(2.7), RES_K(680), 0, RES_K(680), RES_K(10), 0, RES_K(680), 0, 0, 0, 0, CAP_U(1), 0, 0, 0, 12.0 - OP_AMP_NORTON_VBE, 0, 0, 12, TRG_NONE, TRG_NONE, TRG0, TRG_NONE, TRG_NONE, TRG_NONE };

static const opamp_1sht_info inv_missile_1sht[2] = {
	{ RES_M(4.7), RES_K(100), RES_M(1), RES_M(1), RES_M(2.2), CAP_U(1),    CAP_P(470), 0, 12 },	// invaders
	{ RES_M(4.7), RES_K(100), RES_M(1), RES_M(1), RES_M(2.2), CAP_U(0.22), CAP_P(470), 0, 12 },	// invad2ct board 1
};
static const opamp_info inv_missile_B3 = { 0, RES_K(10), RES_M(1.5), 0, CAP_U(0.22), 0, 12 };
static const osc_info inv_missile_osc = { OSC_VCO_3 | OSC_NORTON | OSC_OUT_SQW, 1.0 / (1.0 / RES_M(1) + 1.0 / RES_K(330)) + RES_M(1.5), RES_M(1), RES_K(560), RES_M(2.2), RES_M(1), RES_M(4.7), RES_M(3.3), 0, CAP_P(330), 12 };
static const opamp_info inv_missile_A3 = { RES_K(560), RES_K(470), RES_M(2.7), RES_K(560), 0, 0, 12 };
static const tvca_info inv_missile_tvca = { RES_M(2.7), RES_K(560), 0, RES_K(560), RES_K(1), 0, RES_K(560), 0, 0, 0, 0, CAP_U(0.1), 0, 0, 0, 5, 0, 0, 12, TRG_NONE, TRG_NONE, TRG0, TRG_NONE, TRG_NONE, TRG_NONE };

static const mixer_info inv_mixer[2] = {
	{ MIXER_IS_OP_AMP, { RES_K(200), RES_K(10) + 100 + 100, RES_K(150), RES_K(200), RES_K(2) + RES_K(6.8) + RES_K(5.6), RES_K(150) }, { 0, 0, 0, 0, 0, CAP_U(0.001) }, 0, RES_K(100), 0, CAP_U(0.1), 0, 1 },
	{ MIXER_IS_OP_AMP, { RES_K(100), RES_K(15) + 100 + 100, RES_K(150), RES_K(150), RES_K(10) + RES_K(6.8) + RES_K(5.6), RES_K(150) }, { 0, 0, 0, 0, 0, CAP_U(0.001) }, 0, RES_K(100), 0, CAP_U(0.1), 0, 1 },
};

struct invaders_board {
	// inputs
	INT32 saucer_hit_en, fleet_data, bonus_en, invader_hit_en, explosion_en, missile_en;
	// saucer hit
	d_opamp sh_b3_9, sh_b3_10; d_opamp_osc sh_osc, sh_vco;
	// fleet
	double fleet_r[16]; d_555_astable fleet_555; d_rc fleet_rc1, fleet_rc2;
	// bonus missile base
	d_555_astable bonus_555; d_squarewfix bonus_sq;
	// invader hit
	d_opamp_1sht ih_1sht; d_opamp ih_d3_10, ih_d3_4; d_opamp_osc ih_osc, ih_vco;
	// explosion
	d_opamp_1sht ex_1sht; d_tvca ex_tvca; d_rc ex_rc1, ex_rc2;
	// missile
	d_opamp_1sht mi_1sht; d_opamp mi_b3, mi_a3; d_cr mi_cr; d_opamp_osc mi_osc; d_tvca mi_tvca;
	// mixer
	d_mixer mixer;
	INT32 hit_type, missile_type, mixer_type;

	void relink() {
		sh_b3_9.info = &inv_saucer_hit_B3_9; sh_osc.info = &inv_saucer_hit_osc; sh_vco.info = &inv_saucer_hit_vco; sh_b3_10.info = &inv_saucer_hit_B3_10;
		ih_1sht.info = &inv_invader_hit_1sht[hit_type]; ih_d3_10.info = &inv_invader_hit_D3_10; ih_osc.info = &inv_invader_hit_osc[hit_type];
		ih_vco.info = &inv_invader_hit_vco; ih_d3_4.info = &inv_invader_hit_D3_4;
		ex_1sht.info = &inv_explosion_1sht; ex_tvca.info = &inv_explosion_tvca;
		mi_1sht.info = &inv_missile_1sht[missile_type]; mi_b3.info = &inv_missile_B3; mi_osc.info = &inv_missile_osc;
		mi_a3.info = &inv_missile_A3; mi_tvca.info = &inv_missile_tvca;
		mixer.info = &inv_mixer[mixer_type];
	}

	void reset(INT32 hit_type_, INT32 missile_type_, INT32 mixer_type_) {
		hit_type = hit_type_; missile_type = missile_type_; mixer_type = mixer_type_;
		saucer_hit_en = fleet_data = bonus_en = invader_hit_en = explosion_en = missile_en = 0;

		sh_b3_9.reset(&inv_saucer_hit_B3_9);
		sh_osc.reset(&inv_saucer_hit_osc);
		sh_vco.reset(&inv_saucer_hit_vco);
		sh_b3_10.reset(&inv_saucer_hit_B3_10);

		static const double rr[4] = { RES_K(20) + RES_K(20), RES_K(68), RES_K(82), RES_K(100) };
		for (INT32 i = 0; i < 16; i++) {
			double t = 0;
			for (INT32 b = 0; b < 4; b++) if (i & (1 << b)) t += 1.0 / rr[b];
			fleet_r[i] = (t != 0) ? 1.0 / t : 0;
		}
		fleet_555.reset(D555_OUT_ENERGY, 5, 5.0 - 0.6, TTL_V_LOGIC_1);
		fleet_rc1.reset(100, CAP_U(4.7));
		fleet_rc2.reset(100 + 100, CAP_U(10));

		bonus_555.reset(D555_OUT_SQW, 5.0, -1, -1);
		bonus_sq.reset(480, 50, 0);

		ih_1sht.reset(&inv_invader_hit_1sht[hit_type]);
		ih_d3_10.reset(&inv_invader_hit_D3_10);
		ih_osc.reset(&inv_invader_hit_osc[hit_type]);
		ih_vco.reset(&inv_invader_hit_vco);
		ih_d3_4.reset(&inv_invader_hit_D3_4);

		ex_1sht.reset(&inv_explosion_1sht);
		ex_tvca.reset(&inv_explosion_tvca);
		ex_rc1.reset(RES_K(5.6), CAP_U(0.1));
		ex_rc2.reset(RES_K(5.6) + RES_K(6.8), CAP_U(0.1));

		mi_1sht.reset(&inv_missile_1sht[missile_type]);
		mi_b3.reset(&inv_missile_B3);
		mi_cr.reset(RES_M(1) + RES_K(330), CAP_U(0.1));
		mi_osc.reset(&inv_missile_osc);
		mi_a3.reset(&inv_missile_A3);
		mi_tvca.reset(&inv_missile_tvca);

		mixer.reset(&inv_mixer[mixer_type], 6);
	}

	double step(double noise) {
		double in[6];

		// saucer hit
		double n20 = sh_b3_9.step(0, saucer_hit_en ? 12.0 : 0.0);
		double n21 = sh_osc.step(1, 0, 0);
		double n22 = sh_vco.step(1, n21, 0);
		in[0] = sh_b3_10.step(n22, n20);

		// fleet movement
		double n31 = fleet_555.step(1, fleet_r[fleet_data & 0x0f], RES_K(75), CAP_U(0.1));
		in[1] = fleet_rc2.step(fleet_rc1.step(n31));

		// bonus missile base
		double n40 = bonus_555.step(bonus_en, RES_K(100), RES_K(47), CAP_U(1));
		double n41 = bonus_sq.step(1, 1, 0.5);
		in[2] = (bonus_en && n41 != 0 && n40 != 0) ? TTL_V_LOGIC_1 : 0;

		// invader hit
		double n50 = ih_1sht.step(invader_hit_en ? 5.0 : 0.0);
		double n51 = ih_d3_10.step(0, n50);
		double n52 = ih_osc.step(1, 0, 0);
		double n53 = ih_vco.step(1, n52, 0);
		in[3] = ih_d3_4.step(n53, n51);

		// explosion
		double n60 = ex_1sht.step(explosion_en ? 5.0 : 0.0);
		double n61 = ex_tvca.step((INT32)n60, 0, 0, noise, 0);
		in[4] = ex_rc2.step(ex_rc1.step(n61));

		// missile
		double n70 = mi_1sht.step(missile_en ? 5.0 : 0.0);
		double n71 = mi_b3.step(0, n70);
		double n73 = n71 - 0.5; if (n73 < 0) n73 = 0; if (n73 > 12) n73 = 12;
		double n75 = mi_cr.step(noise) * (RES_K(330) / (RES_M(1) + RES_K(330)));
		double n76 = mi_osc.step(1, n75, n73);
		double n77 = mi_a3.step(n76, n73);
		in[5] = mi_tvca.step(missile_en ? 5 : 0, 0, 0, n77, 0);

		return mixer.step(1, in);
	}
};

static d_lfsr inv_noise;
static invaders_board inv_board[2];

static void invaders_sound_reset(INT32 boards)
{
	inv_noise.reset(7515, 12, 12.0 / 2, 0x1ffff, 4, 16, 12);
	if (boards == 1) {
		inv_board[0].reset(0, 0, 0);
	} else {
		inv_board[0].reset(1, 1, 1);	// invad2ct board 1
		inv_board[1].reset(2, 0, 0);	// invad2ct board 2
	}
}

// Taito "Polaris" discrete sound
static const opfilt_info polaris_music_filt = { RES_K(580), 0, 0, RES_M(2.2), RES_M(1), CAP_U(.01), 0, 0, 0, 12, 0 };
static const opfilt_info polaris_nol_filt   = { 560, RES_K(6.8), RES_K(1002), RES_M(2.2), RES_M(1), CAP_U(.22), CAP_U(.22), CAP_U(1), 0, 12, 0 };
static const opfilt_info polaris_noh_filt   = { 560, RES_K(6.8), RES_K(1002), RES_M(2.2), RES_M(1), CAP_U(.001), CAP_U(.001), CAP_U(.01), 0, 12, 0 };
static const osc_info polaris_sonar_vco = { OSC_VCO_1 | OSC_NORTON | OSC_OUT_CAP, RES_M(1), RES_K(680), RES_K(680), RES_M(1), RES_M(1), RES_K(120), RES_M(1), 0, CAP_P(180), 12 };
static const tvca_info polaris_sonar_tvca = { RES_M(2.7), RES_K(680), 0, RES_K(680), RES_K(1), RES_K(120), RES_K(560), 0, 0, 0, 0, CAP_U(1), 0, 0, 0, 12, 12, 12, 12, TRG_NONE, TRG_NONE, TRG1, TRG0_INV, TRG_NONE, TRG_NONE };
static const osc_info polaris_boat_mod = { OSC_1 | OSC_NORTON | OSC_OUT_CAP, RES_M(1), RES_K(10), RES_K(100), RES_K(120), RES_M(1), 0, 0, 0, CAP_U(.22), 12 };
static const osc_info polaris_boat_vco = { OSC_VCO_1 | OSC_NORTON | OSC_OUT_CAP, RES_M(1), RES_K(680), RES_K(680), RES_M(1), RES_M(1), 0, 0, 0, CAP_P(180), 12 };
static const tvca_info polaris_shot_tvca    = { RES_M(2.7), RES_K(680), RES_K(680), RES_K(680), RES_K(1), 0, RES_K(680), 0, 0, 0, 0, CAP_U(1), 0, 0, 0, 12, 12, 12, 12, TRG0_INV, TRG0, TRG0, TRG_NONE, TRG_NONE, TRG_NONE };
static const tvca_info polaris_shiphit_tvca = { RES_M(2.7), RES_K(680), RES_K(680), RES_K(680), RES_K(1), 0, RES_K(680), 0, 0, 0, 0, CAP_U(1), 0, 0, 0, 12, 12, 12, 12, TRG0_INV, TRG_NONE, TRG0, TRG_NONE, TRG_NONE, TRG_NONE };
static const tvca_info polaris_exp_tvca     = { RES_M(2.7), RES_K(680), 0, RES_K(680), RES_K(1), 0, RES_K(680), 0, 0, 0, 0, CAP_U(.33), 0, 0, 0, 12, 12, 12, 12, TRG_NONE, TRG_NONE, TRG0, TRG_NONE, TRG_NONE, TRG_NONE };
static const tvca_info polaris_hit_tvca     = { RES_M(2.7), RES_K(1360), RES_K(1360), RES_K(680), RES_K(1), 0, RES_K(680), 0, 0, 0, 0, CAP_U(1), 0, 0, 0, 12, 12, 12, 12, TRG0, TRG1, TRG01_NAND, TRG_NONE, TRG_NONE, TRG_NONE };
static const integrate_info polaris_plane_integrate = { RES_K(1001), RES_K(1001), RES_K(101), CAP_U(2.2), 12, 12, TRG0, TRG0_INV, TRG1_INV };
static const osc_info polaris_plane_vco = { OSC_VCO_1 | OSC_NORTON | OSC_OUT_CAP, RES_M(1), RES_K(680), RES_K(680), RES_M(1), RES_M(1), RES_K(100), RES_K(10), RES_K(100), CAP_U(0.002), 12 };
static const mixer_info polaris_mixer_vr1 = { MIXER_IS_RESISTOR, { RES_K(66), RES_K(43), RES_K(20), RES_K(43) }, { CAP_U(1), CAP_U(1), CAP_U(1), CAP_U(1) }, 0, RES_K(50), 0, 0, 0, 1 };
static const mixer_info polaris_mixer_vr2 = { MIXER_IS_RESISTOR, { RES_K(66), RES_K(110) }, { CAP_U(1), CAP_U(1) }, 0, RES_K(50), 0, 0, 0, 1 };
static const mixer_info polaris_mixer_vr4 = { MIXER_IS_RESISTOR, { RES_K(22), RES_K(20), RES_K(22), RES_K(22) }, { 0, CAP_U(1), 0, 0 }, 0, RES_K(50), 0, CAP_U(1), 0, 40000 };

static struct {
	INT32 music_data, sx0, sx1, sx2, sx3, sx5, sx6, sx7, sx9, sx10;
	d_note music_note; d_dac_r1 music_dac; d_opamp_filt music_filt;
	d_squarewfix sonar_clk; INT32 sonar_count, sonar_last_clk, sonar_dff, sonar_dff_last_clk;
	INT32 os_state, os_last_trig; double os_countdown;
	d_opamp_osc sonar_vco; d_tvca sonar_tvca;
	d_lfsr noise_lo, noise_hi; d_opamp_filt noise_lo_filt, noise_hi_filt;
	d_tvca shot_tvca, shiphit_tvca, exp_tvca, hit_tvca;
	d_opamp_osc boat_mod, boat_vco;
	d_rc exp_rc1, exp_rc2;
	d_integrate plane_int; d_opamp_osc plane_vco;
	d_mixer mix_vr1, mix_vr2, mix_vr4;
	double vr1, vr2, vr3;
} pol;

static void polaris_sound_reset()
{
	pol.music_data = 0xff;
	pol.sx0 = pol.sx1 = pol.sx2 = pol.sx3 = pol.sx5 = pol.sx6 = pol.sx7 = pol.sx9 = pol.sx10 = 0;

	pol.music_note.reset(23396, 0xff, 255, 3);
	static const double music_r[2] = { RES_K(47), RES_K(12) };
	pol.music_dac.reset(2, music_r, 0, 0, 0, CAP_P(180), TTL_V_LOGIC_1);
	pol.music_filt.reset(&polaris_music_filt, OPF_HIGH_PASS_0);

	pol.sonar_clk.reset(60.0 / 16, 50, 0);
	pol.sonar_count = 0; pol.sonar_last_clk = 0;
	pol.sonar_dff = 0; pol.sonar_dff_last_clk = 0;
	pol.os_state = 0; pol.os_last_trig = 0; pol.os_countdown = 0;
	pol.sonar_vco.reset(&polaris_sonar_vco);
	pol.sonar_tvca.reset(&polaris_sonar_tvca);

	pol.noise_lo.reset(800.8, 12.0 - OP_AMP_NORTON_VBE, (12.0 - OP_AMP_NORTON_VBE) / 2, 0, 4, 16, 12);
	pol.noise_hi.reset(23396, 12.0 - OP_AMP_NORTON_VBE, (12.0 - OP_AMP_NORTON_VBE) / 2, 0, 4, 16, 12);
	pol.noise_lo_filt.reset(&polaris_nol_filt, OPF_BAND_PASS_0);
	pol.noise_hi_filt.reset(&polaris_noh_filt, OPF_BAND_PASS_0);

	pol.shot_tvca.reset(&polaris_shot_tvca);
	pol.shiphit_tvca.reset(&polaris_shiphit_tvca);
	pol.boat_mod.reset(&polaris_boat_mod);
	pol.boat_vco.reset(&polaris_boat_vco);
	pol.exp_tvca.reset(&polaris_exp_tvca);
	pol.exp_rc1.reset(560.0, CAP_U(.22));
	pol.exp_rc2.reset(RES_K(6.8), CAP_U(.22));
	pol.plane_int.reset(&polaris_plane_integrate);
	pol.plane_vco.reset(&polaris_plane_vco);
	pol.hit_tvca.reset(&polaris_hit_tvca);

	pol.mix_vr1.reset(&polaris_mixer_vr1, 4);
	pol.mix_vr2.reset(&polaris_mixer_vr2, 2);
	pol.mix_vr4.reset(&polaris_mixer_vr4, 4);

	pol.vr1 = 0.80; pol.vr2 = 0.70; pol.vr3 = 0.90 * 0.5376;
}

static void polaris_relink()
{
	pol.music_filt.info = &polaris_music_filt;
	pol.sonar_vco.info = &polaris_sonar_vco; pol.sonar_tvca.info = &polaris_sonar_tvca;
	pol.noise_lo_filt.info = &polaris_nol_filt; pol.noise_hi_filt.info = &polaris_noh_filt;
	pol.shot_tvca.info = &polaris_shot_tvca; pol.shiphit_tvca.info = &polaris_shiphit_tvca;
	pol.boat_mod.info = &polaris_boat_mod; pol.boat_vco.info = &polaris_boat_vco;
	pol.exp_tvca.info = &polaris_exp_tvca; pol.plane_int.info = &polaris_plane_integrate;
	pol.plane_vco.info = &polaris_plane_vco; pol.hit_tvca.info = &polaris_hit_tvca;
	pol.mix_vr1.info = &polaris_mixer_vr1; pol.mix_vr2.info = &polaris_mixer_vr2; pol.mix_vr4.info = &polaris_mixer_vr4;
}

static double polaris_sound_step()
{
	// music
	INT32 note = pol.music_note.step(1, pol.music_data);
	double music = pol.music_filt.step(pol.music_dac.step(note)) * pol.vr3;

	// sonar clock: 60Hz / 16, counter 0-31 clocked on falling edge
	double clk = pol.sonar_clk.step(1, 1, 0.5);
	INT32 clk_l = (clk != 0);
	if (pol.sonar_last_clk && !clk_l) pol.sonar_count = (pol.sonar_count + 1) & 31;
	pol.sonar_last_clk = clk_l;
	INT32 n42 = (pol.sonar_count & 4) ? 1 : 0;
	INT32 n43 = (pol.sonar_count & 16) ? 0 : 1;

	// one-shot (rising edge, not retriggerable, 1.5ms)
	INT32 n44;
	{
		INT32 do_count = pol.os_state;
		if (n43 != pol.os_last_trig) {
			pol.os_last_trig = n43;
			if (n43 && !pol.os_state) { pol.os_state = 1; pol.os_countdown = 0.0015; }
		}
		if (do_count) {
			pol.os_countdown -= disc_dt;
			if (pol.os_countdown <= 0) { pol.os_countdown = 0; pol.os_state = 0; }
		}
		n44 = pol.os_state;
	}

	INT32 n45 = n42 || pol.sx3;
	// D flip-flop clocked by the sonar clock, data = n45
	if (!pol.sonar_dff_last_clk && clk_l) pol.sonar_dff = n45;
	pol.sonar_dff_last_clk = clk_l;

	// noise
	double noise_lo = pol.noise_lo.step(1, 0, n44);
	double noise_lo_filt = pol.noise_lo_filt.step(noise_lo);
	double noise_hi = pol.noise_hi.step(1, 0, n44);
	double noise_hi_filt = pol.noise_hi_filt.step(noise_hi);

	double sonar_v = pol.sonar_vco.step(1, noise_lo_filt, 0);
	double sonar = pol.sonar_tvca.step(n45, pol.sonar_dff, 0, sonar_v, 0);

	double shot = pol.shot_tvca.step(pol.sx0, 0, 0, noise_hi_filt, noise_lo_filt);
	double shiphit = pol.shiphit_tvca.step(pol.sx1, 0, 0, noise_hi_filt, noise_lo_filt);

	double boat_m = pol.boat_mod.step(1, 0, 0);
	double ship = pol.boat_vco.step(pol.sx2, boat_m, 0);

	double n70 = pol.exp_tvca.step(pol.sx3, 0, 0, noise_lo, 0);
	double explosion = pol.exp_rc2.step(pol.exp_rc1.step(n70));

	double n80 = pol.plane_int.step(pol.sx6, pol.sx7);
	double plane = pol.plane_vco.step(pol.sx7, n80, noise_hi_filt);

	double hit = pol.hit_tvca.step(pol.sx10, pol.sx9, 0, noise_lo_filt, noise_hi_filt);

	double in1[4] = { shot, sonar, hit, plane };
	double n91 = pol.mix_vr1.step(1, in1) * pol.vr1;
	double in2[2] = { ship, shiphit };
	double n93 = pol.mix_vr2.step(1, in2) * pol.vr2;
	double in4[4] = { n91, explosion, n93, music };
	return pol.mix_vr4.step(pol.sx5, in4);
}

// Taito "Space Chaser" sound board
#define SCHASER_HSYNC	18352.94
#define SCHASER_4V		(SCHASER_HSYNC / 2 / 4)
#define SCHASER_8V		(SCHASER_HSYNC / 2 / 8)

static const double schaser_effect_rc[8] = {
	0,
	(RES_K(15) + RES_K(20)) * CAP_U(1),
	(RES_K(39) + RES_K(20)) * CAP_U(1),
	(1.0 / (1.0 / RES_K(15) + 1.0 / RES_K(39)) + RES_K(20)) * CAP_U(1),
	(RES_K(82) + RES_K(20)) * CAP_U(1),
	(1.0 / (1.0 / RES_K(15) + 1.0 / RES_K(82)) + RES_K(20)) * CAP_U(1),
	(1.0 / (1.0 / RES_K(15) + 1.0 / RES_K(82)) + RES_K(20)) * CAP_U(1),
	(1.0 / (1.0 / RES_K(15) + 1.0 / RES_K(39) + 1.0 / RES_K(82)) + RES_K(20)) * CAP_U(1)
};

static struct {
	INT32 dot_en, dot_sel, music_bit, snd_en, explosion, last_effect, effect_555_is_low;
	double effect_555_time_remain;	// seconds, 0 = none
	double effect_555_timer;		// seconds until toggle, < 0 = never
	INT32 effect_555_param;
	d_squarewfix sq4v, sq8v; d_rc dot_rc1, dot_rc2;
	d_rc exp_rc1, exp_rc2; d_cr exp_cr;
	d_cr music_cr;
	double vr1, vr2, vr3;
} sch;

static void schaser_sn_update()
{
	INT32 en = !(sch.effect_555_is_low || sch.explosion);
	SN76477_enable_w(0, en);
}

static void schaser_effect_555_toggle()
{
	sch.effect_555_is_low = !sch.effect_555_is_low;
	sch.effect_555_time_remain = 0;

	if (sch.effect_555_is_low)
		sch.effect_555_timer = 0.693 * (0 + 2 * RES_K(20)) * CAP_U(1) / 2;	// PERIOD_OF_555_ASTABLE(0, 20k, 1u) / 2
	else
		sch.effect_555_timer = sch.effect_555_param ? .8873 * schaser_effect_rc[sch.effect_555_param] : -1;

	schaser_sn_update();
}

static void schaser_sound_reset()
{
	sch.dot_en = sch.dot_sel = sch.music_bit = sch.snd_en = sch.explosion = 0;
	sch.last_effect = 0; sch.effect_555_is_low = 0;
	sch.effect_555_time_remain = 0; sch.effect_555_timer = -1; sch.effect_555_param = 0;
	sch.sq4v.reset(SCHASER_4V, 50, 0);
	sch.sq8v.reset(SCHASER_8V, 50, 0);
	sch.dot_rc1.reset(560, CAP_U(.1));
	sch.dot_rc2.reset(RES_K(6.8) + 560, CAP_U(.1));
	sch.exp_rc1.reset(560, CAP_U(.1));
	sch.exp_rc2.reset(RES_K(6.8) + 560, CAP_U(.1));
	sch.exp_cr.reset(RES_K(6.8) + 560 + RES_K(2) + RES_K(50), CAP_U(1));
	sch.music_cr.reset(RES_K(470) + RES_K(50), CAP_U(.01));
	sch.vr1 = 0.70 * (RES_K(50) / (RES_K(50) + RES_K(470)));
	sch.vr2 = 0.90 * (RES_K(50) / (RES_K(50) + 560 + RES_K(6.8) + RES_K(2)));
	sch.vr3 = 0.70 * (RES_K(50) / (RES_K(50) + 560 + RES_K(6.8) + RES_K(10)));
}

static double schaser_sound_step(double sn_in)
{
	// 555 effect timer runs in sample time
	if (sch.effect_555_timer >= 0) {
		sch.effect_555_timer -= disc_dt;
		if (sch.effect_555_timer < 0) schaser_effect_555_toggle();
	}

	double v4 = sch.sq4v.step(1, TTL_V_LOGIC_1, 0);
	double v8 = sch.sq8v.step(1, TTL_V_LOGIC_1, 0);
	double dot = sch.dot_en ? (sch.dot_sel ? v8 : v4) : 0;
	dot = sch.dot_rc2.step(sch.dot_rc1.step(dot)) * sch.vr3;

	double exp_in = sn_in * (1.0 / 14100) + 2.323;	// scale SN76477 output to 0-3.5V
	double expl = sch.exp_cr.step(sch.exp_rc2.step(sch.exp_rc1.step(exp_in))) * sch.vr2;

	double music = sch.music_cr.step(sch.music_bit ? TTL_V_LOGIC_1 : 0) * sch.vr1;

	return sch.snd_en ? (dot + expl + music) : 0;
}

static void schaser_sh_port_1_write(UINT8 data)
{
	sch.dot_en = data & 0x01;
	sch.dot_sel = (data >> 1) & 1;

	INT32 effect = (data >> 2) & 0x07;
	if (sch.last_effect != effect)
	{
		if (effect)
		{
			if (sch.effect_555_time_remain != 0) {
				sch.effect_555_timer = sch.effect_555_time_remain;
				sch.effect_555_param = effect;
			} else if (!sch.effect_555_is_low) {
				sch.effect_555_timer = .8873 * schaser_effect_rc[effect];
				sch.effect_555_param = effect;
			}
		}
		else
		{
			if (!sch.effect_555_is_low) {
				sch.effect_555_time_remain = (sch.effect_555_timer >= 0) ? sch.effect_555_timer : 0;
				sch.effect_555_timer = -1;
			}
		}
		sch.last_effect = effect;
	}

	sch.explosion = (data >> 5) & 1;
	SN76477_set_amplitude_res(0, sch.explosion ? (1.0 / (1.0 / RES_K(200) + 1.0 / RES_K(68))) : RES_K(200));
	schaser_sn_update();
	SN76477_mixer_b_w(0, sch.explosion);
}

// Taito "music" generator used by Balloon Bomber, Indian Battle and Lupin III
static struct {
	INT32 data;
	d_note note; d_dac_r1 dac; d_cr cr;
	double gain;
} tmus;

static void taito_music_reset(double clk, double gain)
{
	tmus.data = 0;
	tmus.note.reset(clk, 0, 255, 5);
	static const double r[3] = { 0, RES_K(47), RES_K(12) };
	tmus.dac.reset(3, r, 0, 0, 0, CAP_U(0.1), TTL_V_LOGIC_1);
	tmus.cr.reset(RES_K(10), CAP_U(0.1));
	tmus.gain = gain;
}

static double taito_music_step()
{
	INT32 n = tmus.note.step(1, tmus.data);
	INT32 t = n + ((n > 2) ? 1 : 0);
	return tmus.cr.step(tmus.dac.step(t)) * tmus.gain;
}

// Midway "tone generator"
static const tvca_info midway_music_tvca = { RES_M(3.3), RES_K(10) + RES_K(680), 0, RES_K(680), RES_K(10), 0, RES_K(680), 0, 0, 0, 0, CAP_U(.001), 0, 0, 0, 12, 0, 0, 12, TRG0, TRG_NONE, TRG1, TRG_NONE, TRG_NONE, TRG_NONE };
static const tvca_info boothill_tone_tvca = { RES_M(3.3), RES_K(100) + RES_K(680), 0, RES_K(680), RES_K(10), 0, RES_K(680), 0, 0, 0, 0, CAP_U(.001), 0, 0, 0, 12, 0, 0, 12, TRG0, TRG_NONE, TRG1, TRG_NONE, TRG_NONE, TRG_NONE };

struct mw_tonegen {
	INT32 en, lo, hi;
	d_note note;
	d_tvca tvca;

	void reset(const tvca_info *i) {
		en = lo = hi = 0;
		note.reset(19968000.0 / 10 / 2, 0, 0xfff, 1);
		tvca.reset(i);
	}
	void lo_w(UINT8 data) { en = data & 1; lo = (data >> 1) & 0x1f; }
	void hi_w(UINT8 data) { hi = data & 0x3f; }
	double step() {
		INT32 n = note.step(1, hi * 0x40 + lo * 2);
		return tvca.step(n, en, 0, 12, 0);
	}
};

// Boot Hill (MAME boothill_discrete, D. Renaud) - stereo
static const tvca_info boothill_shot_tvca = { RES_M(2.7), RES_K(510), 0, RES_K(510), RES_K(10), 0, RES_K(510), 0, 0, 0, 0, CAP_U(0.22), 0, 0, 0, 12, 0, 0, 12, TRG_NONE, TRG_NONE, TRG0, TRG_NONE, TRG_NONE, TRG_NONE };
static const tvca_info boothill_hit_tvca  = { RES_M(2.7), RES_K(510), 0, RES_K(510), RES_K(10), 0, RES_K(510), 0, 0, 0, 0, 0, CAP_U(1), 0, 0, 12, 0, 0, 12, TRG_NONE, TRG_NONE, TRG0, TRG_NONE, TRG_NONE, TRG_NONE };
static const mixer_info boothill_l_mixer = { MIXER_IS_OP_AMP, { RES_K(12) + RES_K(68) + RES_K(33), RES_K(12) + RES_K(100) + RES_K(33) }, { 0 }, 0, RES_K(100), 0, CAP_U(0.1), 0, 7200 };
static const mixer_info boothill_r_mixer = { MIXER_IS_OP_AMP, { RES_K(12) + RES_K(68) + RES_K(33), RES_K(12) + RES_K(100) + RES_K(33), RES_K(33) + 404000.0 }, { 0 }, 0, RES_K(100), 0, CAP_U(0.1), 0, 7200 };

static struct {
	INT32 game_on, lshot, rshot, lhit, rhit;
	d_lfsr noise;
	d_tvca ls, rs, lh, rh;
	d_rc ls1, ls2, rs1, rs2, lh1, lh2, rh1, rh2;
	d_mixer lmix, rmix;
	mw_tonegen tone;
} bh;

static void boothill_sound_reset()
{
	bh.game_on = bh.lshot = bh.rshot = bh.lhit = bh.rhit = 0;
	bh.noise.reset(7700, 12, 12.0 / 2, 0x1ffff, 4, 16, 12);
	bh.ls.reset(&boothill_shot_tvca); bh.rs.reset(&boothill_shot_tvca);
	bh.lh.reset(&boothill_hit_tvca);  bh.rh.reset(&boothill_hit_tvca);
	bh.ls1.reset(RES_K(12), CAP_U(.01));   bh.ls2.reset(RES_K(12) + RES_K(68), CAP_U(.0022));
	bh.rs1.reset(RES_K(12), CAP_U(.01));   bh.rs2.reset(RES_K(12) + RES_K(68), CAP_U(.0033));
	bh.lh1.reset(RES_K(12), CAP_U(.033));  bh.lh2.reset(RES_K(12) + RES_K(100), CAP_U(.0033));
	bh.rh1.reset(RES_K(12), CAP_U(.0033)); bh.rh2.reset(RES_K(12) + RES_K(100), CAP_U(.0022));
	bh.lmix.reset(&boothill_l_mixer, 2);
	bh.rmix.reset(&boothill_r_mixer, 3);
	bh.tone.reset(&boothill_tone_tvca);
}

static void boothill_sound_relink()
{
	bh.ls.info = &boothill_shot_tvca; bh.rs.info = &boothill_shot_tvca;
	bh.lh.info = &boothill_hit_tvca;  bh.rh.info = &boothill_hit_tvca;
	bh.lmix.info = &boothill_l_mixer; bh.rmix.info = &boothill_r_mixer;
	bh.tone.tvca.info = &boothill_tone_tvca;
}

static void boothill_sound_step(double *l, double *r)
{
	double n = bh.noise.step(1, 0);
	double tone = bh.tone.step();
	double ls = bh.ls2.step(bh.ls1.step(bh.ls.step(bh.lshot, 0, 0, n, 0)));
	double rs = bh.rs2.step(bh.rs1.step(bh.rs.step(bh.rshot, 0, 0, n, 0)));
	double lh = bh.lh2.step(bh.lh1.step(bh.lh.step(bh.lhit, 0, 0, n, 0)));
	double rh = bh.rh2.step(bh.rh1.step(bh.rh.step(bh.rhit, 0, 0, n, 0)));
	double li[2] = { ls, lh };
	double ri[3] = { rs, rh, tone };
	*l = bh.lmix.step(bh.game_on, li);
	*r = bh.rmix.step(bh.game_on, ri);
}

// Generic "music only" for the other Midway tone generator games
// (clowns, spacwalk, dogpatch, desertgu, dplay) - mixer modelled as a simple
// coupling cap + gain, the effects of those boards are not yet emulated
static struct {
	mw_tonegen tone;
	d_cr cr;
	INT32 enable;
} mwmus;

static void mwmusic_sound_reset()
{
	mwmus.tone.reset(&midway_music_tvca);
	mwmus.cr.reset(RES_K(100), CAP_U(0.1));
	mwmus.enable = 1;
}

static void mwmusic_sound_relink()
{
	mwmus.tone.tvca.info = &midway_music_tvca;
}

static double mwmusic_sound_step()
{
	double v = mwmus.cr.step(mwmus.tone.step());
	return mwmus.enable ? v : 0;
}

// Gun Fight
struct gf_voice_info {
	double c_env;		// C122/C116
	double r_lp1, c_lp1;	// R129 C121 / R116 C115
	double c_lp2;		// C120 / C114 (with R128 || bias network)
	double re;			// emitter resistor (100 shot, 1k hit)
	double c_e;			// emitter bypass (0 for shot)
	double c_out, c_out_gnd;	// C119 / C118
};

static const gf_voice_info gf_voice_infos[4] = {
	{ CAP_U(10), RES_K(20), CAP_U(0.015), CAP_U(0.01),  100,  0,          CAP_U(0.047), CAP_U(0.022) },	// left shot
	{ CAP_U(10), RES_K(20), CAP_U(0.015), CAP_U(0.015), 100,  0,          CAP_U(0.047), CAP_U(0.022) },	// right shot
	{ CAP_U(20), RES_K(20), CAP_U(0.033), CAP_U(0.1),   1000, CAP_U(50),  CAP_U(0.1),   CAP_U(0.033) },	// left hit
	{ CAP_U(20), RES_K(20), CAP_U(0.033), CAP_U(0.047), 1000, CAP_U(50),  CAP_U(0.1),   CAP_U(0.033) },	// right hit
};

struct gf_voice {
	double env, lp1, lp2, ve, hp_cap, lp_out;
	double k_lp1, k_lp2, k_ve, k_hp, k_lpo;

	void reset(const gf_voice_info *i) {
		env = lp1 = lp2 = ve = hp_cap = lp_out = 0;
		k_lp1 = rc_exp(i->r_lp1 * i->c_lp1);
		k_lp2 = rc_exp(((RES_K(30) * 27500.0) / (RES_K(30) + 27500.0)) * i->c_lp2);
		k_ve  = (i->c_e != 0) ? rc_exp(i->re * i->c_e) : 0;
		k_hp  = rc_exp(RES_K(50) * i->c_out);
		k_lpo = rc_exp(RES_K(5.1) * i->c_out_gnd);
	}

	double step(const gf_voice_info *i, INT32 sw, double noise) {
		// envelope cap: switch (15 ohm) charges to 16V
		if (sw) env += (16.0 - env) * (1.0 - exp(-disc_dt / (15.0 * i->c_env)));

		// base: bias divider R126 330k / R127 30k from the envelope + filtered noise
		lp1 += (noise - lp1) * k_lp1;
		lp2 += (lp1 - lp2) * k_lp2;
		double vb = env * (30.0 / 360.0) + lp2 * (27.5 / 57.5);

		double ic;
		if (i->c_e != 0) {
			// bypassed emitter: dc set by emitter cap, ac gain set by re
			double vbe = vb - 0.65;
			double target = (vbe > 0) ? vbe : 0;
			ve += (target - ve) * k_ve;
			ic = ve / i->re + (vbe - ve) / 25.0;
		} else {
			ic = (vb - 0.65) / i->re;
		}
		if (ic < 0) ic = 0;

		double vc = env - ic * RES_K(5.1);
		if (vc < 0.1) { vc = (env > 0.1) ? 0.1 : env; ic = (env - vc) / RES_K(5.1); }

		// discharge of the envelope cap: transistor + R126/R127
		if (!sw) {
			env -= (ic + env / RES_K(360)) * disc_dt / i->c_env;
			if (env < 0) env = 0;
		}

		// output coupling: C119 into the volume pot, C118 to ground
		double hp = vc - hp_cap;
		hp_cap += hp * k_hp;
		lp_out += (hp - lp_out) * k_lpo;

		return lp_out * 0.5;	// pot at midpoint
	}
};

struct gf_side {
	double cin_s, cin_h, ve, out_cap, out_lp;
	double k_in_s, k_in_h, k_out, k_lp;

	void reset(double c_shot, double c_hit) {
		cin_s = cin_h = ve = out_cap = out_lp = 0;
		k_in_s = rc_exp(RES_K(30) * c_shot);
		k_in_h = rc_exp(RES_K(30) * c_hit);
		k_out  = rc_exp((RES_K(30) + RES_K(47)) * CAP_U(0.047));
		k_lp   = rc_exp(RES_K(30) * CAP_U(0.001));
	}

	double step(double shot, double hit) {
		// input coupling caps (R122/C117, R109/C110)
		double s = shot - cin_s; cin_s += s * k_in_s;
		double h = hit - cin_h;  cin_h += h * k_in_h;

		// common emitter amp Q102: ~2.36k input impedance, gain ~ -392 (5.1k / 13 ohm)
		double vin = (s + h) * 0.5 * (2360.0 / (RES_K(30) + 2360.0));
		double vc = 5.8 - vin * 392.0;
		if (vc > 16.0) vc = 16.0;
		if (vc < 2.1) vc = 2.1;

		// C108 coupling, R104/C107 low pass, master volume pot midpoint
		double o = vc - out_cap; out_cap += o * k_out;
		out_lp += (o - out_lp) * k_lp;
		return out_lp * 0.5;
	}
};

static struct {
	INT32 sw[4];		// left shot, right shot, left hit, right hit
	gf_voice v[4];
	gf_side side[2];
	double noise_t, noise_val, noise_lp;
	UINT32 rng;
} gf;

static void gunfight_sound_reset()
{
	for (INT32 i = 0; i < 4; i++) { gf.sw[i] = 0; gf.v[i].reset(&gf_voice_infos[i]); }
	gf.side[0].reset(CAP_U(0.047), CAP_U(0.1));
	gf.side[1].reset(CAP_U(0.047), CAP_U(0.1));
	gf.noise_t = gf.noise_val = gf.noise_lp = 0;
	gf.rng = 0x12345678;
}

static double gf_gauss()
{
	// sum of 4 uniforms, roughly gaussian, sigma ~1
	double s = 0;
	for (INT32 i = 0; i < 4; i++) {
		gf.rng = gf.rng * 1664525 + 1013904223;
		s += (double)(gf.rng >> 8) / 16777216.0;
	}
	return (s - 2.0) * 1.73;
}

static void gunfight_sound_step(double *l, double *r)
{
	// zener noise amplified by Q302, sampled at 24kHz
	gf.noise_t += disc_dt;
	while (gf.noise_t >= 1.0 / 24000) {
		gf.noise_t -= 1.0 / 24000;
		gf.noise_val = gf_gauss() * 0.5;
	}

	double ls = gf.v[0].step(&gf_voice_infos[0], gf.sw[0], gf.noise_val);
	double rs = gf.v[1].step(&gf_voice_infos[1], gf.sw[1], gf.noise_val);
	double lh = gf.v[2].step(&gf_voice_infos[2], gf.sw[2], gf.noise_val);
	double rh = gf.v[3].step(&gf_voice_infos[3], gf.sw[3], gf.noise_val);

	*l = gf.side[0].step(ls, lh);
	*r = gf.side[1].step(rs, rh);
}

static void gunfight_sound_w(UINT8 data)
{
	// the 74175 latches and inverts the top 4 bits
	INT32 sel = (~data >> 4) & 0x0f;
	for (INT32 i = 0; i < 4; i++) gf.sw[i] = 0;
	if (sel >= 1 && sel <= 4) gf.sw[sel - 1] = 1;
}

// 280-ZZZAP / Laguna Racer sound board
struct zz_bp {
	double lo, bp, f, q;
	void reset(double fc, double qv) { lo = bp = 0; f = 2.0 * sin(M_PI * fc * disc_dt); q = 1.0 / qv; }
	double step(double in) {
		double hi = in - lo - q * bp;
		bp += f * hi;
		lo += f * bp;
		return bp;
	}
};

static struct {
	INT32 pedal, hi_shift, lo_shift, boom, engine_off, noise_cr1, noise_cr2;
	INT32 lagunar;
	double c18, c19_lag, speed;
	double ph[3];
	double noise_t, noise_raw, noise_lp1, noise_lp2;
	zz_bp skid, boomf;
	double crash_lp1, crash_lp2;
	double mix_cap, out_cap;
	UINT32 rng;
} zz;

static void zzzap_sound_reset(INT32 lagunar)
{
	memset(&zz, 0, sizeof(zz));
	zz.lagunar = lagunar;
	zz.engine_off = 1;
	zz.rng = 0x2545f491;
	zz.skid.reset(1000, 25);
	zz.boomf.reset(60, 19);
}

static void zzzap_sound_p1_w(UINT8 data)
{
	zz.pedal = data & 0x0f;
	zz.hi_shift = (data >> 4) & 1;
	zz.lo_shift = (data >> 5) & 1;
}

static void zzzap_sound_p2_w(UINT8 data)
{
	zz.boom = data & 1;
	zz.engine_off = (data >> 1) & 1;
	zz.noise_cr1 = (data >> 2) & 1;
	zz.noise_cr2 = (data >> 3) & 1;
}

static inline double zz_tri(double ph)	// 0..1 -> triangle -1..1
{
	return (ph < 0.5) ? (ph * 4.0 - 1.0) : (3.0 - ph * 4.0);
}

static double zzzap_sound_step()
{
	// "engine speed": C18 charges from the pedal voltage (0.25V/step), faster discharge,
	// switch G5_A dumps it when the engine sound is off
	double c18_scale = zz.lagunar ? (22.0 / 47.0) : 1.0;
	double target = 0.25 * zz.pedal;
	if (zz.engine_off) {
		zz.c18 += (0 - zz.c18) * rc_exp(270 * CAP_U(47) * c18_scale);
	} else {
		double tau = ((target > zz.c18) ? 2.2 : 0.5) * c18_scale;
		zz.c18 += (target - zz.c18) * rc_exp(tau);
	}

	// high gear: C19 (2.2uF) charging through R59 (560k) depresses the output for a while
	if (zz.hi_shift && !zz.lo_shift) {
		zz.c19_lag += (zz.c18 - zz.c19_lag) * rc_exp(RES_K(560) * CAP_U(2.2));
	} else {
		zz.c19_lag = zz.c18;
	}
	double eff = (zz.hi_shift && !zz.lo_shift) ? (zz.c18 * 0.45 + zz.c19_lag * 0.55) : zz.c18;
	if (eff < 0) eff = 0;
	zz.speed = 0.7 + eff * (3.4 / 3.75);	// H5_4 out, 0.7 - 4.1V

	double engine = 0;
	if (!zz.engine_off || zz.c18 > 0.01) {
		static const double fmax[2][3] = { { 87, 343, 465 }, { 87, 343, 511 } };
		double k = zz.speed / 4.1;
		for (INT32 i = 0; i < 3; i++) {
			zz.ph[i] += fmax[zz.lagunar][i] * k * disc_dt;
			if (zz.ph[i] >= 1.0) zz.ph[i] -= 1.0;
		}
		double sig = (zz_tri(zz.ph[0]) + zz_tri(zz.ph[1])) * 0.5 * k;		// summed, amplitude follows speed
		double ctl = (zz_tri(zz.ph[2]) + 1.0) * 0.5;					// 0..1, more = more attenuation
		engine = sig * pow(10.0, -1.6 * ctl) * 1.2;
	}

	// noise: zener -> saturated op-amp (sign of the noise) at 24kHz, 6.3kHz low pass
	zz.noise_t += disc_dt;
	while (zz.noise_t >= 1.0 / 24000) {
		zz.noise_t -= 1.0 / 24000;
		zz.rng ^= zz.rng << 13; zz.rng ^= zz.rng >> 17; zz.rng ^= zz.rng << 5;
		zz.noise_raw = (zz.rng & 0x80000000) ? 1.0 : -1.0;
	}
	double k63 = rc_exp(1.0 / (2.0 * M_PI * 6300));
	zz.noise_lp1 += (zz.noise_raw * 0.8 - zz.noise_lp1) * k63;
	double noise = zz.noise_lp1;

	// tire skid: 1kHz band pass, Q 25, gain 15 (switched at the output)
	double skid = zz.skid.step(noise) * 15.0 / 25.0;
	if (skid > 2.25) skid = 2.25;
	if (skid < -2.25) skid = -2.25;
	if (!zz.noise_cr1) skid = 0;

	// post crash: 1kHz low pass with gain 100 -> heavily clipped noise
	double kc = rc_exp(1.0 / (2.0 * M_PI * 1000));
	zz.crash_lp1 += (noise - zz.crash_lp1) * kc;
	zz.crash_lp2 += (zz.crash_lp1 - zz.crash_lp2) * kc;
	double crash = zz.crash_lp2 * 100.0;
	if (crash > 2.25) crash = 2.25;
	if (crash < -2.25) crash = -2.25;
	if (!zz.noise_cr2) crash = 0;

	// boom: 60Hz band pass, Q 19, gain ~200, input gated by BOOM (rings on after release)
	double boom = zz.boomf.step(zz.boom ? noise : 0) * 200.0 / 19.0;
	if (boom > 2.25) boom = 2.25;
	if (boom < -2.25) boom = -2.25;

	// final mix (Norton current summing, R69 100k feedback): skid 12k, crash 150k, boom 12k, engine 33k
	double mix = skid * (100.0 / 12.0) * 0.035 + crash * (100.0 / 150.0) * 0.30 + boom * (100.0 / 12.0) * 0.07 + engine * (100.0 / 33.0) * 0.5;

	// output coupling, J5_3/J5_4 clip (0 - 4.5V around 2.5V)
	double o = mix - zz.mix_cap; zz.mix_cap += o * rc_exp(RES_K(100) * CAP_U(0.1));
	if (o > 2.0) o = 2.0;
	if (o < -2.0) o = -2.0;

	return o / 1.25;
}

static Stream snd_stream;
static d_bitdac bitdac;
static INT16 *sn_buf[2];
static INT32 sn_chips;
static double sn_gain;
static double disc_gain;

// Taito (and other) games that runs on invaders hw
static INT32 inv_extra;		// board used in addition to the game's own sound type
static double inv_gain;
static UINT16 inv_lines[3];	// per sound port

static INT32 uses_inv_samples_board(INT32 snd)
{
	switch (snd) {
		case SND_SAMPLES:
		case SND_SPCEWARS:
		case SND_SCHASERCV:
		case SND_BALLBOMB:
		case SND_INDIANBT:
			return 1;
	}
	return 0;
}

static void snd_sync()
{
	snd_stream.update();
}

static void snd_stream_update(INT16 **streams, INT32 samples)
{
	INT16 *outl = streams[0];
	INT16 *outr = streams[1];

	for (INT32 c = 0; c < sn_chips; c++) {
		memset(sn_buf[c], 0, samples * 2 * sizeof(INT16));
		SN76477_sound_update_INT(c, sn_buf[c], samples);
	}

	for (INT32 i = 0; i < samples; i++)
	{
		double l = 0, r = 0;
		double sn0 = sn_chips > 0 ? sn_buf[0][i * 2] : 0;
		double inv = inv_extra ? inv_board[0].step(inv_noise.step(1, 0)) * inv_gain : 0;

		switch (cfg->sound)
		{
			case SND_INVADERS: {
				double noise = inv_noise.step(1, 0);
				l = inv_board[0].step(noise) * disc_gain + sn0 * sn_gain;
				r = l;
			}
			break;

			case SND_INVAD2CT: {
				double noise = inv_noise.step(1, 0);
				l = inv_board[0].step(noise) * disc_gain + sn0 * sn_gain;
				r = inv_board[1].step(noise) * disc_gain + sn_buf[1][i * 2] * sn_gain;
			}
			break;

			case SND_POLARIS:
				l = r = polaris_sound_step() * disc_gain;
			break;

			case SND_SCHASER:
				l = r = schaser_sound_step(sn0) * disc_gain;
			break;

			case SND_LUPIN3:
			case SND_BALLBOMB:
			case SND_INDIANBT:
				l = r = taito_music_step() * disc_gain + sn0 * sn_gain;
			break;

			case SND_SPCEWARS:
			case SND_LRESCUE:
			case SND_SCHASERCV:
				l = r = bitdac.step() * 32767 * 0.25 + sn0 * sn_gain;
			break;

			case SND_GUNFIGHT:
				gunfight_sound_step(&l, &r);
				l *= disc_gain; r *= disc_gain;
			break;

			case SND_ZZZAP:
			case SND_LAGUNAR:
				l = r = zzzap_sound_step() * disc_gain;
			break;

			case SND_BOOTHILL:
				boothill_sound_step(&l, &r);
				l *= disc_gain; r *= disc_gain;
			break;

			case SND_MWMUSIC:
				l = r = mwmusic_sound_step() * disc_gain;
			break;

			default:
				l = r = sn0 * sn_gain;
			break;
		}

		l += inv;
		r += inv;

		outl[i] = BURN_SND_CLIP((INT32)l);
		outr[i] = BURN_SND_CLIP((INT32)r);
	}
}

static void sn76477_invaders_saucer(INT32 chip, double vco_cap, double vco_res)
{
	SN76477_set_noise_params(chip, 0, 0, 0);
	SN76477_set_decay_res(chip, 0);
	SN76477_set_attack_params(chip, 0, RES_K(100));
	SN76477_set_amplitude_res(chip, RES_K(56));
	SN76477_set_feedback_res(chip, RES_K(10));
	SN76477_set_vco_params(chip, 0, vco_cap, vco_res);
	SN76477_set_pitch_voltage(chip, 5.0);
	SN76477_set_slf_params(chip, CAP_U(1.0), RES_K(120));
	SN76477_set_oneshot_params(chip, 0, 0);
	SN76477_set_vco_mode(chip, 1);
	SN76477_set_mixer_params(chip, 0, 0, 0);
	SN76477_set_envelope_params(chip, 1, 0);
	SN76477_set_enable(chip, 1);
}

static void sound_init()
{
	disc_set_rate(nBurnSoundRate ? nBurnSoundRate : 44100);

	sn_chips = 0;
	sn_gain = 1.0;
	disc_gain = 1.0;

	switch (cfg->sound)
	{
		case SND_INVADERS:
		case SND_SAMPLES:
		case SND_SPCEWARS:
		case SND_SCHASERCV:
		case SND_LUPIN3:
		case SND_BALLBOMB:
		case SND_INDIANBT:
			SN76477_init(0);
			SN76477_set_mastervol(0, 4.5);
			sn76477_invaders_saucer(0, CAP_U(0.1), RES_K(8.2));
			sn_chips = 1;
		break;

		case SND_INVAD2CT:
			SN76477_init(0);
			SN76477_init(1);
			SN76477_set_mastervol(0, 3.5);
			sn76477_invaders_saucer(0, CAP_U(0.1), RES_K(8.2));
			sn76477_invaders_saucer(1, CAP_U(0.047), RES_K(39));
			sn_chips = 2;
		break;

		case SND_SCHASER:
			SN76477_init(0);
			SN76477_set_mastervol(0, 10.0);
			SN76477_set_noise_params(0, RES_K(47), RES_K(330), CAP_P(470));
			SN76477_set_decay_res(0, RES_M(2.2));
			SN76477_set_attack_params(0, CAP_U(1.0), RES_K(4.7));
			SN76477_set_amplitude_res(0, RES_K(200));
			SN76477_set_feedback_res(0, RES_K(33));
			SN76477_set_vco_params(0, 0, CAP_U(0.1), RES_K(39));
			SN76477_set_pitch_voltage(0, 5.0);
			SN76477_set_slf_params(0, CAP_U(1.0), RES_K(120));
			SN76477_set_oneshot_params(0, CAP_U(0.1), RES_K(220));
			SN76477_set_vco_mode(0, 1);
			SN76477_set_mixer_params(0, 0, 0, 0);
			SN76477_set_envelope_params(0, 1, 0);
			SN76477_set_enable(0, 1);
			sn_chips = 1;
		break;
	}

	for (INT32 c = 0; c < sn_chips; c++)
		sn_buf[c] = (INT16*)BurnMalloc(nBurnSoundRate * 2 * sizeof(INT16) + 0x100);

	switch (cfg->sound)
	{
		case SND_INVADERS:	disc_gain = 2500 * 0.5; sn_gain = 0.5; break;
		case SND_INVAD2CT:	disc_gain = 2500 * 0.5; sn_gain = 0.3; break;
		case SND_POLARIS:	disc_gain = 1.0 * 0.50; break;
		case SND_SCHASER:	disc_gain = 33080 * 0.65; break;
		case SND_LUPIN3:	disc_gain = 21000 * 1.0; sn_gain = 0.5; break;
		case SND_BALLBOMB:	disc_gain = 21000 * 1.0; sn_gain = 0.5; break;
		case SND_INDIANBT:	disc_gain = 21000 * 0.25; sn_gain = 0.5; break;
		case SND_GUNFIGHT:	disc_gain = 32767.0 / 3.0 * 0.5; break;
		case SND_ZZZAP:
		case SND_LAGUNAR:	disc_gain = 32767.0 * 0.8; break;
		case SND_BOOTHILL:	disc_gain = 1.0; break;
		case SND_MWMUSIC:	disc_gain = 2500.0; break;
		default:			sn_gain = 0.5; break;
	}

	inv_extra = uses_inv_samples_board(cfg->sound);
	inv_gain = 2500 * 0.5;

	snd_stream.init(nBurnSoundRate, nBurnSoundRate, 2, 1, snd_stream_update);
	snd_stream.set_buffered(ZetTotalCycles, cfg->cpu_clock);
}

static void sound_reset()
{
	if (sn_chips) SN76477_reset();

	switch (cfg->sound)
	{
		case SND_INVADERS:	invaders_sound_reset(1); break;
		case SND_INVAD2CT:	invaders_sound_reset(2); break;
		case SND_POLARIS:	polaris_sound_reset(); break;
		case SND_SCHASER:	schaser_sound_reset(); break;
		case SND_LUPIN3:	taito_music_reset(7680.0 * 2 * 2 * 2, 1.0); break;
		case SND_INDIANBT:	taito_music_reset(7680.0 * 2 * 2 * 2, 1.0); break;
		case SND_BALLBOMB:	taito_music_reset(3993.6 * 2 * 2 * 2, 1.0); break;
		case SND_GUNFIGHT:	gunfight_sound_reset(); break;
		case SND_ZZZAP:		zzzap_sound_reset(0); break;
		case SND_LAGUNAR:	zzzap_sound_reset(1); break;
		case SND_BOOTHILL:	boothill_sound_reset(); break;
		case SND_MWMUSIC:	mwmusic_sound_reset(); break;
	}

	if (inv_extra) invaders_sound_reset(1);
	inv_lines[0] = inv_lines[1] = inv_lines[2] = 0;

	// saucer etc. off until the game says otherwise
	for (INT32 c = 0; c < sn_chips; c++) SN76477_enable_w(c, 1);

	if (cfg->sound == SND_SCHASER) {
		schaser_sh_port_1_write(0);
		sch.music_bit = 0; sch.snd_en = 0;
	}

	bitdac.reset();
	sound_mute = 0;
	sound_mute_vol = 1.0;

	// let the coupling caps settle, otherwise every reset starts with a loud "thump"
	if (nBurnSoundRate) {
		INT32 warm = (INT32)(disc_rate * 0.5);
		for (INT32 i = 0; i < warm; i++) {
			if (inv_extra) inv_board[0].step(inv_noise.step(1, 0));

			switch (cfg->sound) {
				case SND_INVADERS: inv_board[0].step(inv_noise.step(1, 0)); break;
				case SND_INVAD2CT: { double n = inv_noise.step(1, 0); inv_board[0].step(n); inv_board[1].step(n); } break;
				case SND_POLARIS:  polaris_sound_step(); break;
				case SND_SCHASER:  schaser_sound_step(0); break;
				case SND_LUPIN3: case SND_INDIANBT: case SND_BALLBOMB: taito_music_step(); break;
				case SND_GUNFIGHT: { double l, r; gunfight_sound_step(&l, &r); } break;
				case SND_ZZZAP: case SND_LAGUNAR: zzzap_sound_step(); break;
				case SND_BOOTHILL: { double l, r; boothill_sound_step(&l, &r); } break;
				case SND_MWMUSIC: mwmusic_sound_step(); break;
			}
		}
	}
}

static void sound_relink()
{
	if (inv_extra) inv_board[0].relink();

	switch (cfg->sound)
	{
		case SND_INVADERS:	inv_board[0].relink(); break;
		case SND_INVAD2CT:	inv_board[0].relink(); inv_board[1].relink(); break;
		case SND_POLARIS:	polaris_relink(); break;
		case SND_BOOTHILL:	boothill_sound_relink(); break;
		case SND_MWMUSIC:	mwmusic_sound_relink(); break;
	}
}

static void sound_exit()
{
	snd_stream.exit();
	if (sn_chips) SN76477_exit();
	for (INT32 c = 0; c < 2; c++) { BurnFree(sn_buf[c]); sn_buf[c] = NULL; }
	sn_chips = 0;
}

// Sound / misc output ports
static inline INT32 is_cocktail()
{
	return (DrvDips[4] & 0x02) ? 1 : 0;
}

#define SPL(ch, sam)	splaych(ch, sam, 0.25)

// Midway / Taito discrete board
static void invaders_audio_p1_w(UINT8 data, INT32 board)
{
	snd_sync();
	SN76477_enable_w(board, (~data) & 1);		// saucer sound
	inv_board[board].missile_en = data & 0x02;
	inv_board[board].explosion_en = data & 0x04;
	inv_board[board].invader_hit_en = data & 0x08;
	inv_board[board].bonus_en = (data >> 4) & 1;
	if (board == 0) sound_mute = (~data >> 5) & 1;
}

static void invaders_audio_p2_w(UINT8 data, INT32 board)
{
	snd_sync();
	inv_board[board].fleet_data = data & 0x0f;
	inv_board[board].saucer_hit_en = data & 0x10;
}

// "sample lines" -> invaders board (see inv_extra above)
#define INV_MISSILE		(1 << 0)	// sample "1"
#define INV_EXPLOSION	(1 << 1)	// sample "2" (base hit)
#define INV_INVHIT		(1 << 2)	// sample "3"
#define INV_FLEET1		(1 << 3)	// samples "4" - "7"
#define INV_FLEET2		(1 << 4)
#define INV_FLEET3		(1 << 5)
#define INV_FLEET4		(1 << 6)
#define INV_UFOHIT		(1 << 7)	// sample "8"
#define INV_BONUS		(1 << 8)	// sample "9"
#define INV(sam, cond)	((cond) ? (1 << (sam)) : 0)

static void inv_lines_w(INT32 port, UINT16 lines)
{
	snd_sync();

	inv_lines[port] = lines;
	UINT16 l = inv_lines[0] | inv_lines[1] | inv_lines[2];

	invaders_board &b = inv_board[0];
	b.missile_en     = (l & INV_MISSILE) ? 1 : 0;
	b.explosion_en   = (l & INV_EXPLOSION) ? 1 : 0;
	b.invader_hit_en = (l & INV_INVHIT) ? 1 : 0;
	b.fleet_data     = (l >> 3) & 0x0f;
	b.saucer_hit_en  = (l & INV_UFOHIT) ? 1 : 0;
	b.bonus_en       = (l & INV_BONUS) ? 1 : 0;
}

static INT32 flip_mode;	// 0 = cocktail fake dip, 1 = cosmicmo (IN2 bit 2), 2 = never

static void invaders_p2_w(UINT8 data)
{
	invaders_audio_p2_w(data, 0);

	switch (flip_mode) {
		case 0: if (is_cocktail()) flipscreen = (data >> 5) & 1; break;
		case 1: flipscreen = ((data >> 5) & 1) && (DrvInputs[2] & 0x04); break;
	}
}

// invaders samples + SN76477
static void invadpt2_sh_port_1_w(UINT8 data)
{
	snd_sync();
	SN76477_enable_w(0, (~data) & 1);			// saucer sound

	inv_lines_w(0,
		INV(0, data & 0x02) |	// missile
		INV(1, data & 0x04) |	// explosion
		INV(2, data & 0x08) |	// invader hit
		INV(8, data & 0x10));	// bonus missile base

	screen_red = data & 0x04;
	sound_mute = (~data >> 5) & 1;

	port_1_last = data;
}

static void invadpt2_sh_port_2_w(UINT8 data)
{
	inv_lines_w(1,
		INV(3, data & 0x01) |	// fleet
		INV(4, data & 0x02) |
		INV(5, data & 0x04) |
		INV(6, data & 0x08) |
		INV(7, data & 0x10));	// saucer hit

	flipscreen = ((data >> 5) & 1) & is_cocktail();
	color_map = (data >> 5) & 1;

	port_2_last = data;
}

static void spacerng_sh_port_2_w(UINT8 data)
{
	inv_lines_w(1,
		INV(3, data & 0x01) |
		INV(4, data & 0x02) |
		INV(5, data & 0x04) |
		INV(6, data & 0x08) |
		INV(7, data & 0x10));

	flipscreen = ((~data >> 5) & 1) & is_cocktail();

	port_2_last = data;
}

static void spcewars_sh_port_w(UINT8 data)
{
	snd_sync();
	SN76477_enable_w(0, (~data) & 1);

	inv_lines_w(0,
		INV(0, data & 0x02) |
		INV(1, data & 0x04) |
		INV(2, data & 0x08));

	bitdac.level = (data >> 4) & 1;

	port_1_last = data;
}

static void lrescue_sh_port_1_w(UINT8 data)
{
	UINT8 rising = data & ~port_1_last;

	if (rising & 0x01) SPL(0, 3);		// thrust
	if (rising & 0x02) SPL(1, 2);		// shot
	if (rising & 0x04) SPL(0, 1);		// death
	if (rising & 0x08) SPL(1, 0);		// alien hit
	if (rising & 0x10) SPL(2, 5);		// bonus ship

	snd_sync();
	sound_mute = (~data >> 5) & 1;
	screen_red = data & 0x04;

	port_1_last = data;
}

static void lrescue_sh_port_2_w(UINT8 data)
{
	UINT8 rising = data & ~port_2_last;
	UINT8 falling = ~data & port_2_last;

	if (rising & 0x01) SPL(1, 8);		// footstep high
	if (rising & 0x02) SPL(1, 7);		// footstep low
	if (rising & 0x04) SPL(1, 4);		// bonus when counting men saved

	snd_sync();
	bitdac.level = (data >> 3) & 1;	// bitstream tunes

	if (rising & 0x10) SPL(3, 6);		// shooting star & rescue ship
	if (falling & 0x10) sstopch(3);

	flipscreen = ((data >> 5) & 1) & is_cocktail();

	port_2_last = data;
}

static void cosmo_sh_port_2_w(UINT8 data)
{
	invadpt2_sh_port_2_w(data ^ 0x20);
}

static void ballbomb_sh_port_1_w(UINT8 data)
{
	inv_lines_w(0,
		INV(2, data & 0x01) |	// hit a balloon
		INV(0, data & 0x02) |	// shot
		INV(1, data & 0x04) |	// base hit
		INV(7, data & 0x08) |	// hit a bomb
		INV(8, data & 0x10));	// bonus base

	snd_sync();
	sound_mute = (~data >> 5) & 1;
	screen_red = data & 0x04;

	port_1_last = data;
}

static void ballbomb_sh_port_2_w(UINT8 data)
{
	inv_lines_w(1,
		INV(7, data & 0x01) |	// plane will drop bombs
		INV(4, data & 0x04) |	// plane dropping new balloons
		INV(2, data & 0x10));	// balloon hit and bomb drops

	flipscreen = ((data >> 5) & 1) & is_cocktail();

	port_2_last = data;
}

static void indianbt_sh_port_1_w(UINT8 data)
{
	inv_lines_w(0,
		INV(7, data & 0x01) |	// death
		INV(1, data & 0x02) |	// shot
		INV(3, data & 0x04) |	// move
		INV(2, data & 0x08));	// hit

	snd_sync();
	sound_mute = (~data >> 5) & 1;

	port_1_last = data;
}

static void indianbt_sh_port_2_w(UINT8 data)
{
	inv_lines_w(1,
		INV(0, data & 0x01) |	// bird dropped an egg, lasso used
		INV(2, data & 0x02) |	// egg hatches, egg shot
		INV(0, data & 0x08) |	// grabber, lasso caught something
		INV(7, data & 0x10));	// lasso

	flipscreen = ((data >> 5) & 1) & is_cocktail();

	port_2_last = data;
}

static void indianbtbr_sh_port_1_w(UINT8 data)
{
	inv_lines_w(0,
		INV(7, data & 0x01) |	// lasso
		INV(1, data & 0x04) |	// shot
		INV(2, data & 0x08));	// hit

	snd_sync();
	sound_mute = (~data >> 5) & 1;

	port_1_last = data;
}

static void indianbtbr_sh_port_2_w(UINT8 data)
{
	inv_lines_w(1,
		INV(3, data & 0x08) |	// move
		INV(7, data & 0x10));	// death

	flipscreen = ((data >> 5) & 1) & is_cocktail();

	port_2_last = data;
}

static void music_w(UINT8 data)
{
	snd_sync();
	tmus.data = data;
}

static void schaser_sh_port_2_w(UINT8 data)
{
	snd_sync();
	sch.music_bit = data & 1;
	sch.snd_en = (data >> 1) & 1;
	sound_mute = (~data >> 1) & 1;

	schaser_background_disable = (data >> 3) & 1;
	schaser_background_select = (data >> 4) & 1;

	flipscreen = ((data >> 5) & 1) & ((DrvInputs[2] >> 6) & 1);

	port_2_last = data;
}

static void schaser_sh_port_1_w(UINT8 data)
{
	snd_sync();
	schaser_sh_port_1_write(data);
}

static void crashrd_port03_w(UINT8 data)
{
	snd_sync();
	sch.snd_en = (data >> 5) & 1;
	sound_mute = (~data >> 5) & 1;
	sch.dot_en = (data >> 4) & 1;
	sch.dot_sel = data & 1;
	sch.explosion = (data >> 2) & 1;
	SN76477_set_amplitude_res(0, sch.explosion ? (1.0 / (1.0 / RES_K(200) + 1.0 / RES_K(68))) : RES_K(200));
	SN76477_enable_w(0, !sch.explosion);
	SN76477_mixer_b_w(0, sch.explosion);
}

static void crashrd_port05_w(UINT8 data)
{
	snd_sync();
	sch.music_bit = data & 1;
}

static void schasercv_sh_port_1_w(UINT8 data)
{
	inv_lines_w(0,
		INV(6, data & 0x02) |	// ran over a dot
		INV(1, data & 0x10));	// death

	port_1_last = data;
}

static void schasercv_sh_port_2_w(UINT8 data)
{
	snd_sync();
	bitdac.level = data & 1;			// end of level
	sound_mute = (~data >> 4) & 1;

	flipscreen = ((data >> 5) & 1) & is_cocktail();
}

static void lupin3_sh_port_1_w(UINT8 data)
{
	UINT8 rising = data & ~port_1_last;

	if (rising & 0x01) {
		SPL(0, lupin3_step);			// walking, steal money
		lupin3_step ^= 1;
	}

	snd_sync();
	SN76477_enable_w(0, (data >> 1) & 1);	// helicopter

	if (rising & 0x04) SPL(1, 4);		// translocate
	if (rising & 0x08) SPL(0, 0);		// jail
	if (rising & 0x10) SPL(2, 5);		// bonus man

	port_1_last = data;
}

static void lupin3_sh_port_2_w(UINT8 data)
{
	UINT8 rising = data & ~port_2_last;

	if (rising & 0x01) SPL(0, 6);		// lands on top of building, wife kicks man
	if (rising & 0x10) SPL(3, 1);		// dog barking

	color_map = data & 0x40;

	flipscreen = ((data >> 5) & 1) & ((DrvInputs[2] >> 2) & 1);

	port_2_last = data;
}

static void polaris_sh_port_2_w(UINT8 data)
{
	snd_sync();
	pol.sx0 = (data >> 0) & 1;	// shot
	pol.sx1 = (data >> 1) & 1;	// ship hit (sub)
	pol.sx2 = (data >> 2) & 1;	// ship
	pol.sx3 = (data >> 3) & 1;	// explosion
	pol.sx5 = (data >> 5) & 1;	// sound enable
}

static void polaris_sh_port_3_w(UINT8 data)
{
	snd_sync();
	flipscreen = ((data >> 5) & 1) & ((DrvInputs[2] >> 2) & 1);
	pol.sx6 = (data >> 0) & 1;	// plane down
	pol.sx7 = (data >> 1) & 1;	// plane up
	pol.sx9 = (data >> 3) & 1;	// hit
	pol.sx10 = (data >> 4) & 1;	// hit
}

static void ozmawars_port03_w(UINT8 data)
{
	UINT8 rising = data & ~port_1_last;

	port_1_last = data;
	screen_red = (data >> 2) & 1;
	port_3_last = (data >> 5) & 1;	// sound enable

	if (port_3_last)
	{
		if (rising & 0x01) splaych(4, 11, 0.25, false, true);	// globe-thing after the comet
		if (rising & 0x02) SPL(0, 0);							// fire
		if (rising & 0x04) SPL(1, 1);							// die
		if (rising & 0x08) SPL(2, 8);							// hit invader
		if (rising & 0x10) SPL(4, 6);							// refuel
	}
	else
		sstopch(4);
}

static void ozmawars_port04_w(UINT8 data)
{
	if (port_3_last)
	{
		if (data == 0x01) splaych(4, 13, 0.25, false, true);	// stage with wheels
		if (data == 0x15) splaych(4, 10, 0.25, false, true);	// stage with invaders
		if (data == 0x17) SPL(3, 9);							// meteor hit
		if (data == 0x1f) splaych(4, 14, 0.25, false, true);	// meteor stage
	}
	if (data == 0) sstopch(4);
}

static void ozmawars_port05_w(UINT8 data)
{
	UINT8 rising = data & ~port_2_last;

	port_2_last = data;
	flipscreen = ((data >> 5) & 1) & is_cocktail();
	color_map = (data >> 5) & 1;

	if (port_3_last)
	{
		if (rising & 0x01) SPL(4, 2);	// docking 1
		if (rising & 0x02) SPL(4, 3);	// docking 2
		if (rising & 0x04) SPL(4, 4);	// docking 3
		if (rising & 0x08) SPL(4, 5);	// docking 4
		if (rising & 0x10) SPL(4, 7);	// comet
	}
}

static void yosakdon_sh_port_1_w(UINT8 data)
{
	inv_lines_w(0,
		INV(3, data & 0x01) |	// game over
		INV(0, data & 0x02) |	// bird dead
		INV(1, data & 0x04) |	// rifle being fired
		INV(2, data & 0x08) |	// man dead
		INV(8, data & 0x10));	// bonus man?

	snd_sync();
	sound_mute = (~data >> 5) & 1;

	port_1_last = data;
}

static void yosakdon_sh_port_2_w(UINT8 data)
{
	inv_lines_w(1,
		INV(6, data & 0x01) |	// ready?, game over
		INV(7, data & 0x04) |	// big bird dead
		INV(7, data & 0x10));	// game over

	snd_sync();
	SN76477_enable_w(0, (data >> 3) & 1);	// big bird

	flipscreen = ((data >> 5) & 1) & is_cocktail();

	port_2_last = data;
}

static void shuttlei_sh_port_1_w(UINT8 data)
{
	inv_lines_w(0,
		INV(4, data & 0x01) |	// fleet move
		INV(8, data & 0x02));	// extra tank

	snd_sync();
	SN76477_enable_w(0, (data & 0x04) ? 0 : 1);	// UFO

	port_1_last = data;
}

static void shuttlei_sh_port_2_w(UINT8 data)
{
	// the latched value selects the effect, held until the next write
	inv_lines_w(1,
		INV(2, data == 0x23) |		// hit
		INV(0, data == 0x2b) |		// shoot
		INV(7, data == 0xa3) |		// hit UFO
		INV(1, data == 0xab));		// death
}

static void darthvdr_08_w(UINT8 data)
{
	UINT8 rising = data & ~port_1_last;

	snd_sync();
	sound_mute = (~data) & 1;

	SN76477_enable_w(0, (~data >> 5) & 1);	// UFO

	// fleet move in 4 steps: each new pulse on bit 3 selects the next fleet tone
	if (rising & 0x08) {
		if (++darthvdr_fleet_step > 6) darthvdr_fleet_step = 3;
	}

	inv_lines_w(0,
		INV(0, data & 0x02) |		// shoot
		INV(7, data & 0x04) |		// hit UFO
		INV(8, data & 0x10) |		// bonus
		INV(1, data & 0x40) |		// death
		INV(2, data & 0x80) |		// hit
		INV(darthvdr_fleet_step, data & 0x08));	// fleet move

	port_1_last = data;
}

static void rollingc_sh_port_w(UINT8 data)
{
	inv_lines_w(2,
		INV(0, data & 0x02) |	// steering
		INV(1, data & 0x04) |	// collision
		INV(8, data & 0x10));	// computer car is starting to move

	port_3_last = data;
}

// Shifter (MB14241)
static inline void shift_count_w(UINT8 data) { shift_count = ~data & 7; }
static inline void shift_data_w(UINT8 data) { shift_data = (data << 7) | (shift_data >> 8); }
static inline UINT8 shift_result_r() { return (shift_data >> shift_count) & 0xff; }
static inline UINT8 shift_result_rev_r() { return BITSWAP08(shift_result_r(), 0, 1, 2, 3, 4, 5, 6, 7); }

// Inputs
static inline UINT8 ctrl_p1() { return DrvInP1 & 7; }
static inline UINT8 ctrl_p2() { return (is_cocktail() ? DrvInP2 : (DrvInP1 | DrvInP2)) & 7; }

// sicv_base and friends (IN1 = p1, IN2 = p2 / p1 on upright)
static UINT8 in_sicv(INT32 port)
{
	switch (port & 3) {
		case 0: return DrvInputs[0];
		case 1: return DrvInputs[1] | (ctrl_p1() << 4);
		case 2: return DrvInputs[2] | (ctrl_p2() << 4);
	}
	return 0;
}

// Midway invaders (IN0 also has the p1 controls on an upright cabinet)
static UINT8 in_invaders(INT32 port)
{
	switch (port & 3) {
		case 0: return DrvInputs[0] | ((is_cocktail() ? 7 : ctrl_p1()) << 4);
		case 1: return DrvInputs[1] | (ctrl_p1() << 4);
		case 2: return DrvInputs[2] | (ctrl_p2() << 4);
	}
	return 0;
}

// Main cpu handlers
static UINT8 (*read_inputs)(INT32 port) = in_sicv;

static void __fastcall mw_write(UINT16 address, UINT8 data)
{
	switch (cfg->map)
	{
		case 1: // schaser / polaris / lupin3a / indianbtbr / orbite (scattered color ram)
			if ((address & 0xe000) == 0xc000) DrvColorRAM[(address & 0x1f) | ((address & 0x1f80) >> 2)] = data;
		return;

		case 2: // rollingc
			if ((address & 0xe000) == 0xa000) DrvColorRAM[(address & 0x1f) | ((address & 0x1f00) >> 3)] = data;
			if ((address & 0xe000) == 0xe000) DrvColorRAM2[(address & 0x1f) | ((address & 0x1f00) >> 3)] = data;
		return;

		case 3: // cosmo
			if (address == 0x5800) { cosmo_stars_sidescroll = data & 0x08; cosmo_star_speed = data & 0x07; }
		return;
	}
}

static UINT8 __fastcall mw_read(UINT16 address)
{
	switch (cfg->map)
	{
		case 1:
			if ((address & 0xe000) == 0xc000) return DrvColorRAM[(address & 0x1f) | ((address & 0x1f80) >> 2)];
		return 0;

		case 2:
			if ((address & 0xe000) == 0xa000) return DrvColorRAM[(address & 0x1f) | ((address & 0x1f00) >> 3)];
			if ((address & 0xe000) == 0xe000) return DrvColorRAM2[(address & 0x1f) | ((address & 0x1f00) >> 3)];
		return 0;

		case 3:
			if (address == 0x5800) {
				UINT8 rng = ~DrvStars[cosmo_rng_offs];
				cosmo_rng_offs = (cosmo_rng_offs + nCurrentFrame) % STAR_RNG_PERIOD;
				return rng & 0x3f;
			}
		return 0;
	}

	return 0;
}

// --- Midway "invaders" sound board games
static void __fastcall invaders_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 2: shift_count_w(data); return;
		case 3: invaders_audio_p1_w(data, 0); return;
		case 4: shift_data_w(data); return;
		case 5: invaders_p2_w(data); return;
		case 6: BurnWatchdogWrite(); return;
	}
}

static UINT8 __fastcall invaders_in(UINT16 port)
{
	if ((port & 3) == 3) return shift_result_r();
	return read_inputs(port & 3);
}

static UINT8 __fastcall invasion_in(UINT16 port)
{
	if ((port & 3) == 3) return DrvInputs[3];
	switch (port & 3) {
		case 0: return DrvInputs[0] | (ctrl_p1() << 4);
		case 1: return DrvInputs[1] | (ctrl_p1() << 4);
		case 2: return DrvInputs[2] | (ctrl_p1() << 4);
	}
	return 0;
}

static void __fastcall invasion_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 3: invaders_audio_p1_w(data, 0); return;
		case 5: invaders_p2_w(data); return;
		case 6: BurnWatchdogWrite(); return;
	}
}

static UINT8 __fastcall astropal_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return DrvInputs[0];
		case 1: return DrvInputs[1];
		case 3: return DrvInputs[3];
	}
	return 0;
}

static void __fastcall astropal_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 3: invaders_audio_p1_w(data, 0); return;
		case 5: invaders_p2_w(data); return;
		case 6: BurnWatchdogWrite(); return;
	}
}

static void __fastcall vortex_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 0: shift_count_w(data); return;
		case 1: invaders_audio_p1_w(data, 0); return;
		case 6: shift_data_w(data); return;
		case 7: invaders_p2_w(data); return;
		case 4: BurnWatchdogWrite(); return;
	}
}

static UINT8 __fastcall vortex_in(UINT16 port)
{
	switch (port & 3) {
		case 2: return in_sicv(0);
		case 3: return in_sicv(1);
		case 0: return in_sicv(2);
		case 1: return shift_result_r();
	}
	return 0;
}

static void __fastcall invad2ct_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 1: invaders_audio_p1_w(data, 1); return;
		case 2: shift_count_w(data); return;
		case 3: invaders_audio_p1_w(data, 0); return;
		case 4: shift_data_w(data); return;
		case 5: invaders_audio_p2_w(data, 0); return;
		case 6: BurnWatchdogWrite(); return;
		case 7: invaders_audio_p2_w(data, 1); return;
	}
}

static UINT8 __fastcall invad2ct_in(UINT16 port)
{
	if ((port & 3) == 3) return shift_result_r();
	return DrvInputs[port & 3];
}

static UINT8 __fastcall claybust_in(UINT16 port)
{
	switch (port & 0xff) {
		case 1: return DrvInputs[1] | (claybust_gun_on ? 1 : 0);
		case 2: return claybust_gun_pos & 0xff;
		case 3: return shift_result_r();
		case 6: return claybust_gun_pos >> 8;
	}
	return 0;
}

static void __fastcall claybust_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 1: shift_count_w(data); return;
		case 2: shift_data_w(data); return;
		case 4: BurnWatchdogWrite(); return;
	}
}

// --- Taito color boards with samples
static UINT8 __fastcall invadpt2_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0: case 1: case 2: return read_inputs(port & 3);
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall invadpt2_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 2: shift_count_w(data); return;
		case 3: invadpt2_sh_port_1_w(data); return;
		case 4: shift_data_w(data); return;
		case 5: invadpt2_sh_port_2_w(data); return;
		case 6: BurnWatchdogWrite(); return;
	}
}

static void __fastcall spacerng_out(UINT16 port, UINT8 data)
{
	if ((port & 0xff) == 5) { spacerng_sh_port_2_w(data); return; }
	invadpt2_out(port, data);
}

static void __fastcall spcewars_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 2: shift_count_w(data); return;
		case 3: spcewars_sh_port_w(data); return;
		case 4: shift_data_w(data); return;
		case 5: invadpt2_sh_port_2_w(data); return;
	}
}

static UINT8 __fastcall spcewarla_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0: case 1: case 2: return read_inputs(port & 3);
		case 6: return shift_result_r();
	}
	return 0;
}

static void __fastcall spcewarla_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 0x04: spcewars_sh_port_w(data); return;
		case 0x05: invadpt2_sh_port_2_w(data); return;
		case 0x06: BurnWatchdogWrite(); return;
		case 0x08: shift_count_w(data); return;
		case 0x0c: shift_data_w(data); return;
	}
}

static UINT8 __fastcall starw1_in(UINT16 port)
{
	switch (port & 0xff) {
		case 1: case 2: return read_inputs(port & 3);
	}
	return 0;
}

static void __fastcall starw1_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 4: invadpt2_sh_port_1_w(data); return;
		case 5: invadpt2_sh_port_2_w(data); return;
		case 6: BurnWatchdogWrite(); return;
	}
}

static UINT8 __fastcall steelwkr_in(UINT16 port)
{
	switch (port & 0xff) {
		case 1: return DrvInputs[1] | ((ctrl_p1() | ((DrvInP1 & 8) ^ 8)) << 4);
		case 2: return DrvInputs[2] | ((ctrl_p2() | (((is_cocktail() ? DrvInP2 : (DrvInP1 | DrvInP2)) & 8) ^ 8)) << 4);
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall steelwkr_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 2: shift_count_w(data); return;
		case 3: invadpt2_sh_port_1_w(data); return;
		case 4: shift_data_w(data); return;
		case 5: invadpt2_sh_port_2_w(data); return;
		case 6: return; // coin lockout
	}
}

static void __fastcall cosmo_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 3: invadpt2_sh_port_1_w(data); return;
		case 5: cosmo_sh_port_2_w(data); return;
		case 6: BurnWatchdogWrite(); return;
	}
}

static UINT8 __fastcall cosmo_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0: case 1: case 2: return read_inputs(port & 3);
	}
	return 0;
}

static void __fastcall lrescue_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 2: shift_count_w(data); return;
		case 3: lrescue_sh_port_1_w(data); return;
		case 4: shift_data_w(data); return;
		case 5: lrescue_sh_port_2_w(data); return;
	}
}

static void __fastcall ozmawars_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 3: ozmawars_port03_w(data); return;
		case 4: ozmawars_port04_w(data); return;
		case 5: ozmawars_port05_w(data); return;
		case 6: BurnWatchdogWrite(); return;
	}
}

static void __fastcall ballbomb_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 1: music_w(data | 0x80); return;
		case 2: shift_count_w(data); return;
		case 3: ballbomb_sh_port_1_w(data); return;
		case 4: shift_data_w(data); return;
		case 5: ballbomb_sh_port_2_w(data); return;
	}
}

static UINT8 __fastcall indianbt_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0:
			switch (ZetGetPC(-1)) {
				case 0x5fed: return 0x10;
				case 0x5ffc: return 0x00;
			}
			return BurnRandom();
		case 1:
			if (!flipscreen) return DrvInputs[1] | (ctrl_p1() << 4);
			return ((DrvInputs[1] | (ctrl_p1() << 4)) & 0x8f) | (ctrl_p2() << 4);
		case 2: return DrvInputs[2] | (ctrl_p2() << 4);
		case 3: return shift_result_r();
	}
	return 0;
}

static UINT8 __fastcall indianbtbr_in(UINT16 port)
{
	if ((port & 0xff) == 0) return DrvInputs[0];
	return indianbt_in(port);
}

static void __fastcall indianbt_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 2: shift_count_w(data); return;
		case 3: indianbt_sh_port_1_w(data); return;
		case 4: shift_data_w(data); return;
		case 5: indianbt_sh_port_2_w(data); return;
		case 6: BurnWatchdogWrite(); return;
		case 7: music_w(data); return;
	}
}

static void __fastcall indianbtbr_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 2: shift_count_w(data); return;
		case 3: indianbtbr_sh_port_1_w(data); return;
		case 4: shift_data_w(data); return;
		case 5: indianbtbr_sh_port_2_w(data); return;
		case 6: BurnWatchdogWrite(); return;
	}
}

static UINT8 __fastcall yosakdon_in(UINT16 port)
{
	switch (port & 0xff) {
		case 1: return DrvInputs[0] | (ctrl_p1() << 4);
		case 2: return DrvInputs[1] | (ctrl_p2() << 4);
	}
	return 0;
}

static void __fastcall yosakdon_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 3: yosakdon_sh_port_1_w(data); return;
		case 5: yosakdon_sh_port_2_w(data); return;
	}
}

static UINT8 __fastcall shuttlei_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0xfe: return DrvInputs[0];		// dsw
		case 0xff: {
			UINT8 data = DrvInputs[1] ^ (vblank ? 0x02 : 0x00);
			if (flipscreen) data = (data & 0x3b) | DrvInputs[2];
			return data;
		}
	}
	return 0;
}

static void __fastcall shuttlei_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 0xfd: shuttlei_sh_port_1_w(data); return;
		case 0xfe: shuttlei_sh_port_2_w(data); return;
		case 0xff: flipscreen = ((data >> 2) & 1) & is_cocktail(); return;
	}
}

static UINT8 __fastcall darthvdr_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0: return DrvInputs[0] & ~(ctrl_p1() << 1);
		case 1: return DrvInputs[1] & ~(ctrl_p2() << 1);
	}
	return 0;
}

static void __fastcall darthvdr_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 0: flipscreen = (data & 1) & is_cocktail(); return;
		case 8: darthvdr_08_w(data); return;
	}
}

static UINT8 __fastcall schaser_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0: case 1: case 2: return DrvInputs[port & 3];
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall schaser_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 2: shift_count_w(data); return;
		case 3: schaser_sh_port_1_w(data); return;
		case 4: shift_data_w(data); return;
		case 5: schaser_sh_port_2_w(data); return;
		case 6: BurnWatchdogWrite(); return;
	}
}

static void __fastcall crashrd_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 2: shift_count_w(data); return;
		case 3: crashrd_port03_w(data); return;
		case 4: shift_data_w(data); return;
		case 5: crashrd_port05_w(data); return;
		case 6: BurnWatchdogWrite(); return;
	}
}

static UINT8 __fastcall schasercv_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0: case 1: return DrvInputs[port & 3];
		case 2: {
			UINT8 data = DrvInputs[2];
			if (flipscreen) return data;
			UINT8 in1 = DrvInputs[1];
			return (data & 0x89) | (in1 & 0x70) | (((in1 >> 3) & 1) << 1) | (((in1 >> 7) & 1) << 2);
		}
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall schasercv_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 2: shift_count_w(data); return;
		case 3: schasercv_sh_port_1_w(data); return;
		case 4: shift_data_w(data); return;
		case 5: schasercv_sh_port_2_w(data); return;
	}
}

static void __fastcall lupin3_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 0: music_w(data); return;
		case 2: shift_count_w(data); return;
		case 3: lupin3_sh_port_1_w(data); return;
		case 4: shift_data_w(data); return;
		case 5: lupin3_sh_port_2_w(data); return;
		case 6: BurnWatchdogWrite(); return;
	}
}

static UINT8 __fastcall polaris_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0: {
			if (flipscreen) return DrvInputs[0];
			return (DrvInputs[0] & 7) | (DrvInputs[1] & 0xf8);
		}
		case 1: case 2: return DrvInputs[port & 3];
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall polaris_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 0: shift_count_w(data); return;
		case 2: snd_sync(); pol.music_data = data; return;
		case 3: shift_data_w(data); return;
		case 4: polaris_sh_port_2_w(data); return;
		case 5: BurnWatchdogWrite(); return;
		case 6: polaris_sh_port_3_w(data); return;
	}
}

static void __fastcall rollingc_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 0: rollingc_sh_port_w(data); return;
		case 3: invadpt2_sh_port_1_w(data); return;
		case 5: invadpt2_sh_port_2_w(data); return;
	}
}

static UINT8 __fastcall rollingc_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0: return (DrvInputs[0] & ~0x06) | (((~((ctrl_p1() >> 2) & 1) & 1) << 1) | ((~((ctrl_p1() >> 1) & 1) & 1) << 2));
		case 1: case 2: return in_sicv(port & 3);
	}
	return 0;
}

static void __fastcall mraker_out(UINT16 port, UINT8 data)
{
	if ((port & 0xff) == 0) flipscreen = data & 1;
}

static UINT8 __fastcall mraker_in(UINT16 port)
{
	switch (port & 0xff) {
		case 1: case 2: case 3: return DrvInputs[(port & 0xff) - 1];
	}
	return 0;
}

static void __fastcall invrvnge_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 2: shift_count_w(data); return;
		case 3: soundlatch = data; return;
		case 4: shift_data_w(data); return;
		case 5:
			screen_red = (data >> 4) & 1;
			flipscreen = ((data >> 5) & 1) & is_cocktail();
		return;
		case 6: BurnWatchdogWrite(); return;
	}
}

static void __fastcall attackfc_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 3: shift_data_w(data); return;
		case 7: shift_count_w(data); return;
	}
}

static UINT8 __fastcall attackfc_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0: return DrvInputs[0];
		case 3: return shift_result_r();
	}
	return 0;
}

static UINT8 __fastcall attackfcu_in(UINT16 port)
{
	switch (port & 0xff) {
		case 1: return DrvInputs[0];
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall cane_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 1: shift_count_w(data); return;
		case 2: shift_data_w(data); return;
		case 4: BurnWatchdogWrite(); return;
	}
}

static UINT8 __fastcall cane_in(UINT16 port)
{
	switch (port & 0xff) {
		case 1: return DrvInputs[1];
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall orbite_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 0x06: BurnWatchdogWrite(); return;
		case 0x20: shift_count_w(data); return;
		case 0x40: shift_data_w(data); return;
	}
}

static UINT8 __fastcall orbite_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0x08: return shift_result_r();
		case 0x66: return DrvInputs[0];
		case 0x76: return DrvInputs[1];
		case 0x7a: return DrvInputs[2];
	}
	return 0;
}

static UINT8 __fastcall spacecom_in(UINT16 port)
{
	switch (port & 0xff) {
		case 0x41: return DrvInputs[0];
		case 0x42: return DrvInputs[1];
		case 0x44: return DrvInputs[2] | (vblank ? 1 : 0);
	}
	return 0;
}

static void __fastcall spacecom_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff) {
		case 0x42: invaders_audio_p1_w(data, 0); return;
		case 0x44: invaders_p2_w(data); return;
	}
}

// --- Invader's Revenge sound cpu
static void invrvnge_sound_write(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0xa002:
		case 0xa003:
			AY8910Write(0, ~address & 1, data);
		return;
	}
}

static UINT8 invrvnge_sound_read(UINT16 address)
{
	if (address == 0xa001) return AY8910Read(0);
	return 0;
}

static UINT8 invrvnge_ay_porta(UINT32) { return soundlatch >> 1; }
static UINT8 invrvnge_ay_portb(UINT32) { return 0xff; }

// Midway mw8080bw games: Sea Wolf, Gun Fight, Tornado Baseball, 280-ZZZAP,
// Laguna Racer, Boot Hill, Desert Gun, Double Play, Extra Inning, M-4, Clowns,
// Space Walk, Shuffleboard, Dog Patch, Space Encounters, Phantom II,
// 4 Player Bowling Alley, Blue Shark
static INT32 mw_pos_timer[4];

// positional control
static INT32 mw_positional(INT32 idx, INT32 dec, INT32 inc, INT16 analog, INT32 positions, INT32 def, INT32 delay)
{
	if (mw_pos[idx] < 0) mw_pos[idx] = def;

	if (analog > 0x100) inc = 1;
	if (analog < -0x100) dec = 1;

	if (dec || inc) {
		if (mw_pos_timer[idx] <= 0) {
			if (dec && mw_pos[idx] > 0) mw_pos[idx]--;
			if (inc && mw_pos[idx] < positions - 1) mw_pos[idx]++;
			mw_pos_timer[idx] = delay;
		} else {
			mw_pos_timer[idx]--;
		}
	} else {
		mw_pos_timer[idx] = 0;
	}

	return mw_pos[idx];
}

// non-centering paddle
static UINT8 mw_paddle(INT32 idx, INT32 left, INT32 right, INT16 analog, INT32 min, INT32 max, INT32 def)
{
	if (mw_pos[idx] < 0) mw_pos[idx] = def;

	INT32 p = mw_pos[idx];
	p += analog / 0x20;
	if (left) p -= 4;
	if (right) p += 4;
	if (p < min) p = min;
	if (p > max) p = max;
	mw_pos[idx] = p;

	return p;
}

// Sea Wolf
static const UINT8 seawolf_controller_table[30] = {
	0x1e, 0x1c, 0x1d, 0x19, 0x18, 0x1a, 0x1b, 0x13, 0x12, 0x10, 0x11, 0x15, 0x14, 0x16, 0x17, 0x07,
	0x06, 0x04, 0x05, 0x01, 0x00, 0x02, 0x03, 0x0b, 0x0a, 0x08, 0x09, 0x0d, 0x0c, 0x0e
};

// periscope: BurnGun, only the horizontal position is used.  The reticle is
// kept on a fixed line, the periscope only turns left / right.
#define SEAWOLF_RETICLE_Y	72

static void seawolf_frame()
{
	BurnGunMakeInputs(0, DrvAnalog[0], DrvAnalog[1]);

	// digital left / right turn the periscope too
	INT32 x = BurnGunX[0] >> 8;
	if (DrvJoyX[0]) x -= 2;
	if (DrvJoyX[1]) x += 2;
	if (x < -8) x = -8;
	if (x > nScreenWidth - 1 - 8) x = nScreenWidth - 1 - 8;
	BurnGunX[0] = x << 8;
	BurnGunY[0] = (SEAWOLF_RETICLE_Y - 8) << 8;

	INT32 pos = ((x + 8) * 30) / nScreenWidth;	// 30 positions across the screen
	if (pos < 0) pos = 0;
	if (pos > 29) pos = 29;

	mw_analog[0] = seawolf_controller_table[pos] ^ 0x1f;
}

// artwork lamps
static void seawolf_explosion_lamp_w(UINT8 data)
{
	static const UINT8 bits_for_lamps[16] = {
		0x18, 0x14, 0x12, 0x11, 0x28, 0x24, 0x22, 0x21,
		0x48, 0x44, 0x42, 0x41, 0x88, 0x84, 0x82, 0x81
	};

	mw_lamps[0] = data;

	for (INT32 i = 0; i < 16; i++) {
		char name[16];
		sprintf(name, "EXP_LAMP_%X", i);
		BurnOutputSet(name, ((data & bits_for_lamps[i]) == bits_for_lamps[i]) ? 1 : 0);
	}
}

static void seawolf_periscope_lamp_w(UINT8 data)
{
	mw_lamps[1] = data;

	BurnOutputSet("TORP_LAMP_4", (data >> 0) & 1);
	BurnOutputSet("TORP_LAMP_3", (data >> 1) & 1);
	BurnOutputSet("TORP_LAMP_2", (data >> 2) & 1);
	BurnOutputSet("TORP_LAMP_1", (data >> 3) & 1);
	BurnOutputSet("READY_LAMP",  (data >> 4) & 1);
	BurnOutputSet("RELOAD_LAMP", (data >> 5) & 1);
}

static UINT8 __fastcall seawolf_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return shift_result_rev_r();
		case 1: return (DrvInputs[0] & 0xe0) | mw_analog[0];
		case 2: return DrvInputs[1];
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall seawolf_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 1: seawolf_explosion_lamp_w(data); return;
		case 2: seawolf_periscope_lamp_w(data); return;
		case 3: shift_data_w(data); return;
		case 4: shift_count_w(data); return;
		case 5: {
			UINT8 rising = data & ~port_1_last;
			port_1_last = data;
			for (INT32 i = 0; i < 5; i++)
				if (rising & (1 << i)) splaych(i, i, 0.25);	// ship hit, torpedo, dive, sonar, mine hit
		}
		return;
	}
}

// Gun Fight
static const UINT8 gunfight_controller_table[7] = { 0x06, 0x02, 0x00, 0x04, 0x05, 0x01, 0x03 };

static void gunfight_frame()
{
	mw_analog[0] = gunfight_controller_table[mw_positional(0, DrvJoyX[1], DrvJoyX[0], 0, 7, 3, 6)] ^ 7;
	mw_analog[1] = gunfight_controller_table[mw_positional(1, DrvJoyX[3], DrvJoyX[2], 0, 7, 3, 6)] ^ 7;
}

static UINT8 __fastcall gunfight_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return (DrvInputs[0] & 0x8f) | (mw_analog[0] << 4);
		case 1: return (DrvInputs[1] & 0x8f) | (mw_analog[1] << 4);
		case 2: return DrvInputs[2];
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall gunfight_out(UINT16 port, UINT8 data)
{
	if (port & 1) gunfight_sound_w(data);
	if (port & 2) shift_count_w(data);
	if (port & 4) shift_data_w(data);
}

// Tornado Baseball (upright, P1/P2 hit buttons)
static UINT8 __fastcall tornbase_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return DrvInputs[0];
		case 1: return DrvInputs[1];
		case 2: return DrvInputs[2];
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall tornbase_out(UINT16 port, UINT8 data)
{
	if (port & 2) shift_count_w(data);
	if (port & 4) shift_data_w(data);
}

// 280-ZZZAP / Laguna Racer
static void zzzap_frame()
{
	// gear shifter (toggle)
	if (DrvJoyX[4] && !zz_gear_prev) zz_gear ^= 1;
	zz_gear_prev = DrvJoyX[4];

	// accelerator
	UINT8 pedal = ProcessAnalog(DrvAnalog[1], 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x00, 0x0f);
	if (DrvJoyX[2]) pedal = 0x0f;
	mw_analog[1] = pedal;

	// steering wheel (reversed), digital left/right steer gradually
	UINT8 wheel = ProcessAnalog(DrvAnalog[0], 1, INPUT_DEADZONE, 0x01, 0xfe);
	if (DrvJoyX[0] || DrvJoyX[1]) {
		if (mw_pos[0] < 0) mw_pos[0] = 0x7f;
		if (DrvJoyX[0] && mw_pos[0] < 0xfe) mw_pos[0] = (mw_pos[0] + 6 > 0xfe) ? 0xfe : mw_pos[0] + 6;
		if (DrvJoyX[1] && mw_pos[0] > 0x01) mw_pos[0] = (mw_pos[0] - 6 < 0x01) ? 0x01 : mw_pos[0] - 6;
		wheel = mw_pos[0];
	} else {
		mw_pos[0] = 0x7f;
	}
	mw_analog[0] = wheel;
}

static UINT8 __fastcall zzzap_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return (DrvInputs[0] & 0xe0) | (zz_gear << 4) | mw_analog[1];
		case 1: return mw_analog[0];
		case 2: return DrvInputs[2];
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall zzzap_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 2: zzzap_sound_p1_w(data); return;
		case 3: shift_data_w(data); return;
		case 4: shift_count_w(data); return;
		case 5: zzzap_sound_p2_w(data); return;
		case 7: BurnWatchdogWrite(); return;
	}
}

// Boot Hill
static const UINT8 boothill_controller_table[7] = { 0x00, 0x04, 0x06, 0x07, 0x03, 0x01, 0x05 };

static void boothill_frame()
{
	mw_analog[0] = boothill_controller_table[mw_positional(0, DrvJoyX[1], DrvJoyX[0], 0, 7, 3, 6)];	// P1
	mw_analog[1] = boothill_controller_table[mw_positional(1, DrvJoyX[3], DrvJoyX[2], 0, 7, 3, 6)];	// P2
}

static UINT8 __fastcall boothill_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return (DrvInputs[0] & 0x8f) | (mw_analog[1] << 4);
		case 1: return (DrvInputs[1] & 0x8f) | (mw_analog[0] << 4);
		case 2: return DrvInputs[2];
		case 3: return shift_reverse ? shift_result_rev_r() : shift_result_r();
	}
	return 0;
}

static void __fastcall boothill_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 1: shift_count_w(data); shift_reverse = (data >> 3) & 1; return;
		case 2: shift_data_w(data); return;
		case 3:
			bh.game_on = (data >> 3) & 1;
			bh.lshot = (data >> 4) & 1;
			bh.rshot = (data >> 5) & 1;
			bh.lhit = (data >> 6) & 1;
			bh.rhit = (data >> 7) & 1;
		return;
		case 4: BurnWatchdogWrite(); return;
		case 5: bh.tone.lo_w(data); return;
		case 6: bh.tone.hi_w(data); return;
	}
}

// Desert Gun / Road Runner
static void desertgu_frame()
{
	BurnGunMakeInputs(0, DrvAnalog[0], DrvAnalog[1]);
	mw_analog[0] = 0x10 + (BurnGunReturnX(0) * (0x8e - 0x10)) / 255;
	mw_analog[1] = 0x10 + (BurnGunReturnY(0) * (0x7f - 0x10)) / 255;
}

static UINT8 __fastcall desertgu_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return shift_result_rev_r();
		case 1: return mw_analog[ctrl_select];
		case 2: return (DrvInputs[1] & 0xfc) | (DrvDips[ctrl_select ? 3 : 2] & 0x03);
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall desertgu_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 1: shift_count_w(data); return;
		case 2: shift_data_w(data); return;
		case 4: BurnWatchdogWrite(); return;
		case 5: mwmus.tone.lo_w(data); return;
		case 6: mwmus.tone.hi_w(data); return;
		case 7: ctrl_select = (data >> 3) & 1; return;
	}
}

// Double Play / Extra Inning (upright: both sides read the left pitch controls)
static UINT8 __fastcall dplay_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return DrvInputs[0];
		case 1: return (DrvInputs[1] & 0x81) | (DrvInputs[0] & 0x7e);
		case 2: return DrvInputs[2];
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall dplay_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 1: shift_count_w(data); return;
		case 2: shift_data_w(data); return;
		case 4: BurnWatchdogWrite(); return;
		case 5: mwmus.tone.lo_w(data); return;
		case 6: mwmus.tone.hi_w(data); return;
	}
}

// M-4
static UINT8 __fastcall m4_in(UINT16 port)
{
	if ((port & 3) == 3) return shift_reverse ? shift_result_rev_r() : shift_result_r();
	return DrvInputs[port & 3];
}

static void __fastcall m4_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 1: shift_count_w(data); shift_reverse = (data >> 3) & 1; return;
		case 2: shift_data_w(data); return;
		case 3: {
			UINT8 rising = data & ~port_1_last;
			port_1_last = data;
			if (rising & 0x10) splaych(0, 0, 0.25);	// left shot
			if (rising & 0x20) splaych(1, 0, 0.25);	// right shot
			if (rising & 0x40) splaych(2, 1, 0.25);	// left explosion
			if (rising & 0x80) splaych(3, 1, 0.25);	// right explosion
		}
		return;
		case 4: BurnWatchdogWrite(); return;
		case 5: {
			UINT8 rising = data & ~port_2_last;
			port_2_last = data;
			if (rising & 0x01) splaych(2, 1, 0.15);	// left explosion (510k)
			if (rising & 0x02) splaych(3, 1, 0.15);	// right explosion (510k)
		}
		return;
	}
}

// Guided Missile samples (port 3)
static void gmissile_sound_w(UINT8 data)
{
	UINT8 rising = data & ~port_1_last;
	port_1_last = data;
	if (rising & 0x10) splaych(1, 0, 0.25);	// right missile
	if (rising & 0x20) splaych(2, 1, 0.25);	// left explosion
	if (rising & 0x40) splaych(0, 0, 0.25);	// left missile
	if (rising & 0x80) splaych(3, 1, 0.25);	// right explosion
}

// Clowns / Space Walk
static void clowns_frame()
{
	mw_analog[0] = mw_paddle(0, DrvJoyX[0], DrvJoyX[1], DrvAnalog[0], 0x01, 0xfe, 0x7f);
	mw_analog[1] = mw_paddle(1, DrvJoyX[2], DrvJoyX[3], DrvAnalog[1], 0x01, 0xfe, 0x7f);
}

static UINT8 __fastcall clowns_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return mw_analog[ctrl_select];
		case 1: return DrvInputs[1];
		case 2: return DrvInputs[2];
		case 3: return shift_result_r();
	}
	return 0;
}

static UINT8 __fastcall spacwalk_in(UINT16 port)
{
	if (port & 4) return 0;
	return clowns_in(port);
}

static void __fastcall clowns_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 1: shift_count_w(data); return;
		case 2: shift_data_w(data); return;
		case 3: ctrl_select = (data >> 1) & 1; return;
		case 4: BurnWatchdogWrite(); return;
		case 5: mwmus.tone.lo_w(data); return;
		case 6: mwmus.tone.hi_w(data); return;
		case 7: {
			UINT8 rising = data & ~port_2_last;
			port_2_last = data;
			if ((rising & 0x20) && cfg->in == clowns_in) splaych(0, 0, 0.25);	// springboard miss
		}
		return;
	}
}

// Shuffleboard / 4 Player Bowling Alley (trackball)
static void trackball_frame()
{
	BurnTrackballConfig(0, AXIS_REVERSED, AXIS_NORMAL);
	BurnTrackballFrame(0, DrvAnalog[0], DrvAnalog[1], 0x01, 0x10);
	BurnTrackballUDLR(0, DrvJoyX[2], DrvJoyX[3], DrvJoyX[0], DrvJoyX[1]);
	BurnTrackballUpdate(0);
}

static UINT8 __fastcall shuffle_in(UINT16 port)
{
	switch (port & 0x0f) {
		case 1: return shift_result_r();
		case 2: return DrvInputs[0];
		case 3: return shift_result_rev_r();
		case 4: return DrvInputs[1];
		case 5: return BurnTrackballRead(0, 1);
		case 6: return BurnTrackballRead(0, 0);
	}
	return 0;
}

static void __fastcall shuffle_out(UINT16 port, UINT8 data)
{
	switch (port & 0x07) {	// 1, 2, 4 mirrored at +8
		case 1: shift_count_w(data); return;
		case 2: shift_data_w(data); return;
		case 4: BurnWatchdogWrite(); return;
	}
}

static UINT8 __fastcall bowler_in(UINT16 port)
{
	if ((port & 0x0f) == 1) return ~shift_result_r();	// inverted, see MAME
	return shuffle_in(port);
}

static void __fastcall bowler_out(UINT16 port, UINT8 data)
{
	switch (port & 0x0f) {
		case 1: shift_count_w(data); return;
		case 2: shift_data_w(data); return;
		case 4: BurnWatchdogWrite(); return;
	}
}

// Dog Patch
static const UINT8 dogpatch_controller_table[7] = { 0x07, 0x06, 0x04, 0x05, 0x01, 0x00, 0x02 };

static void dogpatch_frame()
{
	mw_analog[0] = dogpatch_controller_table[mw_positional(0, DrvJoyX[1], DrvJoyX[0], 0, 7, 3, 6)];		// P1
	mw_analog[1] = dogpatch_controller_table[6 - mw_positional(1, DrvJoyX[3], DrvJoyX[2], 0, 7, 3, 6)];	// P2 (reversed)
}

static UINT8 __fastcall dogpatch_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return (DrvInputs[0] & 0x8f) | (mw_analog[1] << 4);
		case 1: return (DrvInputs[1] & 0x8f) | (mw_analog[0] << 4);
		case 2: return DrvInputs[2];
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall dogpatch_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 1: shift_count_w(data); return;
		case 2: shift_data_w(data); return;
		case 4: BurnWatchdogWrite(); return;
		case 5: mwmus.tone.lo_w(data); return;
		case 6: mwmus.tone.hi_w(data); return;
	}
}

// Space Encounters
static const UINT8 spcenctr_controller_table[64] = {
	0x3f, 0x3e, 0x3c, 0x3d, 0x39, 0x38, 0x3a, 0x3b, 0x33, 0x32, 0x30, 0x31, 0x35, 0x34, 0x36, 0x37,
	0x27, 0x26, 0x24, 0x25, 0x21, 0x20, 0x22, 0x23, 0x2b, 0x2a, 0x28, 0x29, 0x2d, 0x2c, 0x2e, 0x2f,
	0x0f, 0x0e, 0x0c, 0x0d, 0x09, 0x08, 0x0a, 0x0b, 0x03, 0x02, 0x00, 0x01, 0x05, 0x04, 0x06, 0x07,
	0x17, 0x16, 0x14, 0x15, 0x11, 0x10, 0x12, 0x13, 0x1b, 0x1a, 0x18, 0x19, 0x1d, 0x1c, 0x1e, 0x1f
};

static void spcenctr_frame()
{
	INT32 x = mw_positional(0, DrvJoyX[0], DrvJoyX[1], DrvAnalog[0], 35, 17, 1);
	INT32 y = mw_positional(1, DrvJoyX[2], DrvJoyX[3], DrvAnalog[1], 20, 9, 2);
	mw_analog[0] = spcenctr_controller_table[12 + (34 - x)];
	mw_analog[1] = spcenctr_controller_table[22 + (19 - y)];
}

static UINT8 __fastcall spcenctr_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return (DrvInputs[0] & 0xc0) | mw_analog[0];
		case 1: return (DrvInputs[1] & 0xc0) | mw_analog[1];
		case 2: return DrvInputs[2];
	}
	return 0;
}

static void __fastcall spcenctr_out(UINT16 port, UINT8 data)
{
	port &= 0xff;

	if ((port & 0x07) == 0x00) bright_control = (~data >> 3) & 1;
	else if ((port & 0x5f) == 0x01 || (port & 0x5f) == 0x09 || (port & 0x5f) == 0x11) { /* sound (not emulated) */ }
	else if ((port & 0x07) == 0x02) BurnWatchdogWrite();
	else if ((port & 0x07) == 0x03) trench_slope[BITSWAP08(port, 0, 0, 0, 0, 7, 6, 4, 3) & 0x0f] = data;
	else if ((port & 0x07) == 0x04) trench_center = data;
	else if ((port & 0x07) == 0x07) trench_width = data;
}

// Phantom II
static UINT8 __fastcall phantom2_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return shift_result_rev_r();
		case 1: return DrvInputs[0];
		case 2: return DrvInputs[1];
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall phantom2_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 1: shift_count_w(data); return;
		case 2: shift_data_w(data); return;
		case 4: BurnWatchdogWrite(); return;
		case 5: {
			UINT8 rising = data & ~port_1_last;
			port_1_last = data;
			if (rising & 0x01) splaych(0, 0, 0.25);	// shot
		}
		return;
		case 6: {
			UINT8 rising = data & ~port_2_last;
			port_2_last = data;
			if (rising & 0x08) splaych(1, 1, 0.25);	// explosion
		}
		return;
	}
}

// Blue Shark
static void blueshrk_frame()
{
	mw_analog[0] = mw_paddle(0, DrvJoyX[0], DrvJoyX[1], DrvAnalog[0], 0x08, 0x82, 0x45);
}

static UINT8 __fastcall blueshrk_in(UINT16 port)
{
	switch (port & 3) {
		case 0: return shift_result_rev_r();
		case 1: return mw_analog[0];
		case 2: return DrvInputs[1];
		case 3: return shift_result_r();
	}
	return 0;
}

static void __fastcall blueshrk_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 1: shift_count_w(data); return;
		case 2: shift_data_w(data); return;
		case 4: BurnWatchdogWrite(); return;
	}
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	if (cfg->sound == SND_INVRVNGE) {
		M6800Open(0);
		M6800Reset();
		M6800Close();
		AY8910Reset(0);
	}

	BurnSampleReset();
	sound_reset();

	BurnWatchdogReset();

	shift_data = 0;
	shift_count = 0;
	shift_reverse = 0;
	soundlatch = 0;
	for (INT32 i = 0; i < 4; i++) mw_pos[i] = -1;	// set to the defaults by the frame callbacks
	ctrl_select = 0;
	zz_gear = zz_gear_prev = 0;
	trench_width = trench_center = bright_control = brightness = 0;
	memset(trench_slope, 0, sizeof(trench_slope));
	phantom2_cloud_counter = PHANTOM2_CLOUD_COUNTER_START;
	if (cfg->out == seawolf_out) {
		seawolf_explosion_lamp_w(0);
		seawolf_periscope_lamp_w(0);
	}
	flipscreen = 0;
	color_map = 0;
	screen_red = 0;
	schaser_background_disable = 0;
	schaser_background_select = 0;
	polaris_cloud_speed = 0;
	polaris_cloud_pos = 0;
	port_1_last = 0;
	port_2_last = 0;
	port_3_last = 0;
	lupin3_step = 2;
	darthvdr_fleet_step = 3;
	nmi_toggle = 0;
	claybust_gun_pos = 0;
	claybust_gun_on = 0;
	cosmo_stars_sidescroll = 0;
	cosmo_star_speed = 3;
	cosmo_star_origin = 0;
	cosmo_rng_offs = 0;
	cosmo_bright_star = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvI8080ROM		= Next; Next += 0x010000;
	DrvSndROM		= Next; Next += 0x010000;
	DrvColorPROM	= Next; Next += 0x000800;
	DrvUser1		= Next; Next += 0x000800;

	DrvStars		= Next; Next += STAR_RNG_PERIOD + 1;
	ovl_map			= Next; Next += 260 * 256;

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x002000;
	DrvExtraRAM		= Next; Next += 0x002000;
	DrvColorRAM		= Next; Next += 0x000800;
	DrvColorRAM2	= Next; Next += 0x000400;
	DrvSndRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

// rom type: bits 0-3 region, bits 8-15 = load offset >> 8 (main cpu / sound cpu)
#define MW_ROM(t, a)	((t) | (((a) >> 8) << 8))
#define R_MAIN			1
#define R_SOUND			2
#define R_PROM			3
#define R_USER			4

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	INT32 prom_len = 0;

	memset (DrvColorPROM, 0, 0x800);

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if (ri.nType & BRF_NODUMP) continue;

		INT32 offs = ri.nType & 0xff00;

		switch (ri.nType & 0x0f)
		{
			case R_MAIN:
				if (BurnLoadRom(DrvI8080ROM + offs, i, 1)) return 1;
			break;

			case R_SOUND:
				if (BurnLoadRom(DrvSndROM + offs, i, 1)) return 1;
			break;

			case R_PROM:
				if (BurnLoadRom(DrvColorPROM + offs, i, 1)) return 1;
				if (offs + (INT32)ri.nLen > prom_len) prom_len = offs + ri.nLen;
			break;

			case R_USER:
				if (BurnLoadRom(DrvUser1 + offs, i, 1)) return 1;
			break;
		}
	}

	if (prom_len == 0x400) memcpy (DrvColorPROM + 0x400, DrvColorPROM, 0x400);

	return 0;
}

static void DrvMapMemory()
{
	ZetOpen(0);

	switch (cfg->map)
	{
		case 0: // midway main_map (global mask 0x7fff, ram mirrored at 0x6000)
			for (INT32 i = 0; i < 0x10000; i += 0x8000) {
				ZetMapMemory(DrvI8080ROM,			0x0000 + i, 0x1fff + i, MAP_ROM);
				ZetMapMemory(DrvMainRAM,			0x2000 + i, 0x3fff + i, MAP_RAM);
				ZetMapMemory(DrvI8080ROM + 0x4000,	0x4000 + i, 0x5fff + i, MAP_ROM);
				ZetMapMemory(DrvMainRAM,			0x6000 + i, 0x7fff + i, MAP_RAM);
			}
		break;

		case 1: // schaser_map (+ scattered color ram at 0xc000)
		case 2: // rollingc
			ZetMapMemory(DrvI8080ROM,			0x0000, 0x1fff, MAP_ROM);
			ZetMapMemory(DrvMainRAM,			0x2000, 0x3fff, MAP_RAM);
			ZetMapMemory(DrvI8080ROM + 0x4000,	0x4000, 0x5fff, MAP_ROM);
		break;

		case 3: // cosmo
			ZetMapMemory(DrvI8080ROM,			0x0000, 0x1fff, MAP_ROM);
			ZetMapMemory(DrvMainRAM,			0x2000, 0x3fff, MAP_RAM);
			ZetMapMemory(DrvI8080ROM + 0x4000,	0x4000, 0x57ff, MAP_ROM);
			ZetMapMemory(DrvColorRAM,			0x5c00, 0x5fff, MAP_RAM);
		break;

		case 4: // rom + ram only (yosakdon, cane, spacecom, escmars)
			ZetMapMemory(DrvI8080ROM,			0x0000, 0x1fff, MAP_ROM);
			ZetMapMemory(DrvMainRAM,			0x2000, 0x3fff, MAP_RAM);
			ZetMapMemory(DrvI8080ROM + 0x4000,	0x4000, 0x57ff, MAP_ROM);
		break;

		case 5: // shuttlei / skylove
			ZetMapMemory(DrvI8080ROM,			0x0000, 0x1fff, MAP_ROM);
			ZetMapMemory(DrvMainRAM,			0x2000, 0x37ff, MAP_RAM);
			ZetMapMemory(DrvExtraRAM,			0x3800, 0x3fff, MAP_RAM);
			ZetMapMemory(DrvExtraRAM + 0x800,	0x4000, 0x43ff, MAP_RAM);
			ZetMapMemory(DrvExtraRAM + 0x800,	0x6000, 0x63ff, MAP_RAM);
		break;

		case 6: // darthvdr
			ZetMapMemory(DrvI8080ROM,			0x0000, 0x17ff, MAP_ROM);
			ZetMapMemory(DrvExtraRAM,			0x1800, 0x1fff, MAP_RAM);
			ZetMapMemory(DrvMainRAM,			0x4000, 0x5fff, MAP_RAM);
		break;
	}

	ZetSetWriteHandler(mw_write);
	ZetSetReadHandler(mw_read);
	ZetSetOutHandler(cfg->out);
	ZetSetInHandler(cfg->in);
	ZetClose();
}

static INT32 DrvInit(const mw_config *config, const ovl_layout *ovl, void (*decrypt)() = NULL)
{
	cfg = config;
	overlay = ovl;

	BurnAllocMemIndex();

	if (DrvLoadRoms()) return 1;

	if (decrypt) decrypt();

	ZetInit(0); // really i8080
	DrvMapMemory();

	if (cfg->sound == SND_INVRVNGE)
	{
		M6800Init(0); // M6802
		M6800Open(0);
		for (INT32 i = 0xc000; i < 0x10000; i += 0x800)
			M6800MapMemory(DrvSndROM + (i & 0xe000), i, i + 0x7ff, MAP_ROM);
		M6800MapMemory(DrvSndRAM, 0x0000, 0x00ff, MAP_RAM);
		M6800SetReadHandler(invrvnge_sound_read);
		M6800SetWriteHandler(invrvnge_sound_write);
		M6800Close();

		AY8910Init(0, 1000000, 0);
		AY8910SetPorts(0, &invrvnge_ay_porta, &invrvnge_ay_portb, NULL, NULL);
		AY8910SetAllRoutes(0, 0.75, BURN_SND_ROUTE_BOTH);
	}

	if (cfg->video == VID_SHUTTLEI) BurnSetRefreshRate(60.00);
	else if (cfg->video == VID_SPACECOM) BurnSetRefreshRate(18000000.0 / 3 / 384 / 260);
	else BurnSetRefreshRate(19968000.0 / 4 / 320 / 262);	// 59.54 hz

	BurnWatchdogInit(DrvDoReset, cfg->watchdog);

	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.25, BURN_SND_ROUTE_BOTH);

	sound_init();

	if (cfg->video == VID_COSMO) cosmo_stars_init();

	GenericTilesInit();
	overlay_build();
	palette_init_base();

	if (cfg->in == claybust_in || cfg->in == desertgu_in || cfg->in == seawolf_in) BurnGunInit(1, true);
	if (cfg->in == shuffle_in || cfg->in == bowler_in) BurnTrackballInit(1);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	if (cfg->sound == SND_INVRVNGE) {
		M6800Exit();
		AY8910Exit(0);
	}

	BurnSampleExit();
	sound_exit();

	if (cfg->in == claybust_in || cfg->in == desertgu_in || cfg->in == seawolf_in || cfg->in == shuffle_in || cfg->in == bowler_in) BurnGunExit();

	BurnFreeMemIndex();

	read_inputs = in_sicv;
	flip_mode = 0;

	return 0;
}

static void claybust_update_gun()
{
	static UINT8 last_trigger = 0;
	UINT8 trigger = DrvJoy1[1] & 1;

	if (trigger && !last_trigger)
	{
		INT32 gunx = BurnGunReturnX(0);					// 0 - 255 across the screen
		INT32 guny = BurnGunReturnY(0) * 224 / 256 + 0x20;	// 0x20 - 0xff
		if (guny > 0xff) guny = 0xff;
		claybust_gun_pos = (((gunx & 0xff) >> 3) | (guny << 5)) + 2;
		claybust_gun_on = 15;	// ~250ms
	}
	last_trigger = trigger;

	if (claybust_gun_on) {
		if (--claybust_gun_on == 0) claybust_gun_pos = 0;
	}
}

// soft mute / unmute: ramp the whole output (discrete, SN76477 and samples) over ~15ms
// instead of hard cutting it, which avoids clicks and pops
static void sound_mute_ramp(INT16 *buf, INT32 len)
{
	double target = sound_mute ? 0.0 : 1.0;

	if (sound_mute_vol == target) {
		if (target == 0.0) BurnSoundClear();
		return;
	}

	double step = 1.0 / (0.015 * nBurnSoundRate);

	for (INT32 i = 0; i < len; i++) {
		if (sound_mute_vol < target) { sound_mute_vol += step; if (sound_mute_vol > target) sound_mute_vol = target; }
		else if (sound_mute_vol > target) { sound_mute_vol -= step; if (sound_mute_vol < target) sound_mute_vol = target; }

		buf[i * 2 + 0] = (INT16)(buf[i * 2 + 0] * sound_mute_vol);
		buf[i * 2 + 1] = (INT16)(buf[i * 2 + 1] * sound_mute_vol);
	}
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();
	if (cfg->sound == SND_INVRVNGE) M6800NewFrame();

	{
		for (INT32 i = 0; i < 4; i++) DrvInputs[i] = DrvDips[i];
		DrvInP1 = DrvInP2 = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInP1 |= (DrvJoyP1[i] & 1) << i;
			DrvInP2 |= (DrvJoyP2[i] & 1) << i;
		}

		if (cfg->frame_cb) cfg->frame_cb();

		if (cfg->in == claybust_in) {
			BurnGunMakeInputs(0, DrvGun0, DrvGun1);
			DrvInputs[1] &= ~0x02; // trigger handled as impulse
			claybust_update_gun();
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { (INT32)((INT64)cfg->cpu_clock * 100 / nBurnFPS), (INT32)((INT64)1000000 * 100 / nBurnFPS) };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetOpen(0);
	if (cfg->sound == SND_INVRVNGE) M6800Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Zet);

		if (cfg->sound == SND_INVRVNGE)
		{
			INT32 before = nCyclesDone[1];
			CPU_RUN(1, M6800);
			// E-clock / 256 toggles the NMI line
			if ((before >> 8) != (nCyclesDone[1] >> 8)) {
				nmi_toggle ^= 1;
				M6800SetIRQLine(CPU_IRQLINE_NMI, nmi_toggle ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			}
		}

		if (cfg->irq_type == 0)
		{
			if (i == 95 && ZetIFF1()) {
				ZetSetVector(0xcf);		// rst 08h
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			}
			if (i == 223 && ZetIFF1()) {
				ZetSetVector(0xd7);		// rst 10h
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			}
		}
		else
		{
			if (i == 223) {
				ZetSetVector(0xff);		// rst 38h
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			}
		}

		if (i == 223) {
			vblank = 1;

			if (cfg->video == VID_POLARIS) {
				if (++polaris_cloud_speed >= 4) { polaris_cloud_speed = 0; polaris_cloud_pos++; }
			}

			if (pBurnDraw) {
				BurnDrvRedraw();
			}

			if (cfg->video == VID_PHANTOM2) {
				phantom2_cloud_counter += 262;
				if (phantom2_cloud_counter >= PHANTOM2_CLOUD_COUNTER_END)
					phantom2_cloud_counter = PHANTOM2_CLOUD_COUNTER_START + (phantom2_cloud_counter - PHANTOM2_CLOUD_COUNTER_END);
			}
		}
	}

	if (cfg->sound == SND_INVRVNGE) M6800Close();

	if (pBurnSoundOut) {
		BurnSoundClear();
		if (cfg->sound == SND_INVRVNGE) AY8910Render(pBurnSoundOut, nBurnSoundLen);
		snd_stream.render(pBurnSoundOut, nBurnSoundLen);
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
		sound_mute_ramp(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		ZetScan(nAction);
		if (cfg->sound == SND_INVRVNGE) {
			M6800Scan(nAction);
			AY8910Scan(nAction, pnMin);
		}

		BurnWatchdogScan(nAction);
		BurnSampleScan(nAction, pnMin);
		if (sn_chips) SN76477_scan(nAction, pnMin);
		if (cfg->in == claybust_in || cfg->in == desertgu_in || cfg->in == seawolf_in || cfg->in == shuffle_in || cfg->in == bowler_in) BurnGunScan();

		SCAN_VAR(shift_data);
		SCAN_VAR(shift_count);
		SCAN_VAR(flipscreen);
		SCAN_VAR(color_map);
		SCAN_VAR(screen_red);
		SCAN_VAR(schaser_background_disable);
		SCAN_VAR(schaser_background_select);
		SCAN_VAR(polaris_cloud_speed);
		SCAN_VAR(polaris_cloud_pos);
		SCAN_VAR(port_1_last);
		SCAN_VAR(port_2_last);
		SCAN_VAR(port_3_last);
		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_mute);
		SCAN_VAR(sound_mute_vol);
		SCAN_VAR(lupin3_step);
		SCAN_VAR(darthvdr_fleet_step);
		SCAN_VAR(nmi_toggle);
		SCAN_VAR(claybust_gun_pos);
		SCAN_VAR(claybust_gun_on);
		SCAN_VAR(cosmo_stars_sidescroll);
		SCAN_VAR(cosmo_star_speed);
		SCAN_VAR(cosmo_star_origin);
		SCAN_VAR(cosmo_rng_offs);
		SCAN_VAR(cosmo_bright_star);
		SCAN_VAR(mw_pos);
		SCAN_VAR(ctrl_select);
		SCAN_VAR(zz_gear);
		SCAN_VAR(zz_gear_prev);
		SCAN_VAR(trench_width);
		SCAN_VAR(trench_center);
		SCAN_VAR(trench_slope);
		SCAN_VAR(bright_control);
		SCAN_VAR(brightness);
		SCAN_VAR(phantom2_cloud_counter);
		SCAN_VAR(mw_lamps);
		SCAN_VAR(shift_reverse);

		// analog sound circuits (the structs hold pointers to their const component tables,
		// those get re-linked after loading a state)
		SCAN_VAR(bitdac);
		switch (cfg->sound) {
			case SND_INVADERS:
			case SND_INVAD2CT:
				SCAN_VAR(inv_noise);
				SCAN_VAR(inv_board);
			break;
			case SND_SAMPLES:
			case SND_SPCEWARS:
			case SND_SCHASERCV:
				SCAN_VAR(inv_noise);
				SCAN_VAR(inv_board);
				SCAN_VAR(inv_lines);
			break;
			case SND_POLARIS:	SCAN_VAR(pol); break;
			case SND_SCHASER:	SCAN_VAR(sch); break;
			case SND_LUPIN3:	SCAN_VAR(tmus); break;
			case SND_BALLBOMB:
			case SND_INDIANBT:
				SCAN_VAR(tmus);
				SCAN_VAR(inv_noise);
				SCAN_VAR(inv_board);
				SCAN_VAR(inv_lines);
			break;
			case SND_GUNFIGHT:	SCAN_VAR(gf); break;
			case SND_ZZZAP:
			case SND_LAGUNAR:	SCAN_VAR(zz); break;
			case SND_BOOTHILL:	SCAN_VAR(bh); break;
			case SND_MWMUSIC:	SCAN_VAR(mwmus); break;
		}

		if (nAction & ACB_WRITE) {
			sound_relink();

			if (cfg->out == seawolf_out) {	// restore the artwork lamps
				seawolf_explosion_lamp_w(mw_lamps[0]);
				seawolf_periscope_lamp_w(mw_lamps[1]);
			}
		}
	}

	return 0;
}

#define DIP_ENTRIES \
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		}, \
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		}, \
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		}, \
	{"Dip D",			BIT_DIPSWITCH,	DrvDips + 3,	"dip"		}, \
	{"Dip E",			BIT_DIPSWITCH,	DrvDips + 4,	"dip"		},

#define CTRL_P1 \
	{"P1 Left",			BIT_DIGITAL,	DrvJoyP1 + 1,	"p1 left"	}, \
	{"P1 Right",		BIT_DIGITAL,	DrvJoyP1 + 2,	"p1 right"	}, \
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoyP1 + 0,	"p1 fire 1"	},

#define CTRL_P2 \
	{"P2 Left",			BIT_DIGITAL,	DrvJoyP2 + 1,	"p2 left"	}, \
	{"P2 Right",		BIT_DIGITAL,	DrvJoyP2 + 2,	"p2 right"	}, \
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoyP2 + 0,	"p2 fire 1"	},

// coin / start in IN1 (standard), controls via control ports
static struct BurnInputInfo InvadersInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	CTRL_P1
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	CTRL_P2
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 2,	"tilt"		},
	DIP_ENTRIES
};

STDINPUTINFO(Invaders)

static struct BurnInputInfo SitvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	CTRL_P1
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	CTRL_P2
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 2,	"tilt"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	DIP_ENTRIES
};

STDINPUTINFO(Sitv)

// sicv_base clones with a 2nd coin slot at IN2 bit 2 (searthin/alieninv)
static struct BurnInputInfo SearthinInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	CTRL_P1
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	CTRL_P2
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Searthin)

// 2nd coin at IN1 bit 7 (superinv / invasion)
static struct BurnInputInfo SuperinvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	CTRL_P1
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	CTRL_P2
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 2,	"tilt"		},
	DIP_ENTRIES
};

STDINPUTINFO(Superinv)

static struct BurnInputInfo InvasionInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	CTRL_P1
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Spare",			BIT_DIGITAL,	DrvJoy4 + 1,	"service"	},
	DIP_ENTRIES
};

STDINPUTINFO(Invasion)

static struct BurnInputInfo Invadpt2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	CTRL_P1
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	CTRL_P2
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Name Reset",		BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	DIP_ENTRIES
};

STDINPUTINFO(Invadpt2)

static struct BurnInputInfo LrescueInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	CTRL_P1
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	CTRL_P2
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 2,	"tilt"		},
	DIP_ENTRIES
};

STDINPUTINFO(Lrescue)

static struct BurnInputInfo SteelwkrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	CTRL_P1
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoyP1 + 3,	"p1 fire 2"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	CTRL_P2
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoyP2 + 3,	"p2 fire 2"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 2,	"tilt"		},
	DIP_ENTRIES
};

STDINPUTINFO(Steelwkr)

static struct BurnInputInfo SchaserInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Name Reset",		BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 5,	"tilt"		},
	DIP_ENTRIES
};

STDINPUTINFO(Schaser)

static struct BurnInputInfo SchasercvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Schasercv)

static struct BurnInputInfo Lupin3InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 1"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"		},
	DIP_ENTRIES
};

STDINPUTINFO(Lupin3)

static struct BurnInputInfo PolarisInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 1"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"		},
	DIP_ENTRIES
};

STDINPUTINFO(Polaris)

static struct BurnInputInfo YosakdonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	CTRL_P1
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	CTRL_P2
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Yosakdon)

static struct BurnInputInfo ShuttleiInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 1"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Shuttlei)

static struct BurnInputInfo DarthvdrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	CTRL_P1
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 start"	},
	CTRL_P2
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Darthvdr)

static struct BurnInputInfo MrakerInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 2"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Mraker)

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }
static struct BurnInputInfo ClaybustInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 start"	},
	A("P1 Gun X",		BIT_ANALOG_REL, &DrvGun0,		"mouse x-axis"	),
	A("P1 Gun Y",		BIT_ANALOG_REL, &DrvGun1,		"mouse y-axis"	),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"mouse button 1"},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

#undef A

STDINPUTINFO(Claybust)

static struct BurnInputInfo CaneInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Cane)

static struct BurnInputInfo OrbiteInputList[] = {
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 1"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Orbite)

static struct BurnInputInfo AstropalInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy4 + 7,	"p1 fire 3"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Astropal)

static struct BurnInputInfo AttackfcInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Attackfc)

static struct BurnInputInfo AttackfcuInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Attackfcu)

static struct BurnInputInfo SpacecomInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 1"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	DIP_ENTRIES
};

STDINPUTINFO(Spacecom)

static struct BurnInputInfo Invad2ctInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Invad2ct)

static struct BurnInputInfo MazeInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 right"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Maze)

static struct BurnInputInfo GmissileInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 1"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Gmissile)

static struct BurnInputInfo CheckmatInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 right"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy4 + 2,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p3 right"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy2 + 4,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy2 + 5,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy2 + 6,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p4 right"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	DIP_ENTRIES
};

STDINPUTINFO(Checkmat)

#define FAKE_DIPS(o, def) \
	{(o)+4, 0xff, 0xff, def, NULL							}, \

#define OVERLAY_DIP(o) \
	{0,    0xfe, 0   ,    2, "Color Overlay"				}, \
	{(o)+4, 0x01, 0x01, 0x00, "Off"							}, \
	{(o)+4, 0x01, 0x01, 0x01, "On"							},

#define CABINET_DIP(o) \
	{0,    0xfe, 0   ,    2, "Cabinet"						}, \
	{(o)+4, 0x01, 0x02, 0x00, "Upright"						}, \
	{(o)+4, 0x01, 0x02, 0x02, "Cocktail"					},

#define DEFAULTS(o, a, b, c, d, e) \
	DIP_OFFSET(o) \
	{0x00, 0xff, 0xff, a,    NULL							}, \
	{0x01, 0xff, 0xff, b,    NULL							}, \
	{0x02, 0xff, 0xff, c,    NULL							}, \
	{0x03, 0xff, 0xff, d,    NULL							}, \
	{0x04, 0xff, 0xff, e,    NULL							},

#define LIVES_3456(n) \
	{0   , 0xfe, 0   ,    4, "Lives"						}, \
	{n,    0x01, 0x03, 0x00, "3"							}, \
	{n,    0x01, 0x03, 0x01, "4"							}, \
	{n,    0x01, 0x03, 0x02, "5"							}, \
	{n,    0x01, 0x03, 0x03, "6"							},

static struct BurnDIPInfo InvadersDIPList[]=
{
	DEFAULTS(0x0b, 0x08, 0x09, 0x00, 0x00, 0x01)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x08, "1000"						},
	{0x02, 0x01, 0x08, 0x00, "1500"						},

	{0   , 0xfe, 0   ,    2, "Display Coinage"			},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Unknown (SW:8)"			},
	{0x00, 0x01, 0x01, 0x00, "Off"						},
	{0x00, 0x01, 0x01, 0x01, "On"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Invaders)

// sicv_base with "Unknown" at IN0 bit 0
static struct BurnDIPInfo SicvDIPList[]=
{
	DEFAULTS(0x0b, 0xfe, 0x89, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    2, "Unknown (SW1:3)"			},
	{0x00, 0x01, 0x01, 0x01, "Off"						},
	{0x00, 0x01, 0x01, 0x00, "On"						},

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x08, "1000"						},
	{0x02, 0x01, 0x08, 0x00, "1500"						},

	{0   , 0xfe, 0   ,    2, "Coin Info"				},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Sicv)

static struct BurnDIPInfo SitvDIPList[]=
{
	DEFAULTS(0x0c, 0xff, 0x89, 0x00, 0x00, 0x01)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x08, "1000"						},
	{0x02, 0x01, 0x08, 0x00, "1500"						},

	{0   , 0xfe, 0   ,    2, "Coin Info"				},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Sitv)

static struct BurnDIPInfo InvadrmrDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x89, 0x00, 0x00, 0x01)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x00, "1500"						},
	{0x02, 0x01, 0x08, 0x08, "3000"						},

	{0   , 0xfe, 0   ,    2, "Coin Info"				},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Invadrmr)

static struct BurnDIPInfo SpcewarsDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x89, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x02, 0x01, 0x01, 0x00, "3"						},
	{0x02, 0x01, 0x01, 0x01, "4"						},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x00, "1000"						},
	{0x02, 0x01, 0x08, 0x08, "1500"						},

	{0   , 0xfe, 0   ,    2, "Coin Info"				},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Spcewars)

static struct BurnDIPInfo CosmicmoDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x89, 0x01, 0x00, 0x01)

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x02, 0x01, 0x03, 0x00, "2"						},
	{0x02, 0x01, 0x03, 0x01, "3"						},
	{0x02, 0x01, 0x03, 0x02, "4"						},
	{0x02, 0x01, 0x03, 0x03, "5"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x02, 0x01, 0x04, 0x00, "Upright"					},
	{0x02, 0x01, 0x04, 0x04, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x00, "1500"						},
	{0x02, 0x01, 0x08, 0x08, "2500"						},

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x02, 0x01, 0x80, 0x00, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x80, 0x80, "1 Coin  2 Credits"		},

	OVERLAY_DIP(0)
};

STDDIPINFO(Cosmicmo)

static struct BurnDIPInfo SinvzenDIPList[]=
{
	DEFAULTS(0x0b, 0xfe, 0x89, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x00, 0x01, 0x01, 0x00, "Easy"						},
	{0x00, 0x01, 0x01, 0x01, "Hard"						},

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x08, "1000"						},
	{0x02, 0x01, 0x08, 0x00, "1500"						},

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x02, 0x01, 0x80, 0x00, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x80, 0x80, "1 Coin  2 Credits"		},

	OVERLAY_DIP(0)
};

STDDIPINFO(Sinvzen)

static struct BurnDIPInfo Spaceat2DIPList[]=
{
	DEFAULTS(0x0b, 0xfb, 0x89, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x00, 0x01, 0x04, 0x00, "Easy"						},
	{0x00, 0x01, 0x04, 0x04, "Hard"						},

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x08, "1000"						},
	{0x02, 0x01, 0x08, 0x00, "1500"						},

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x02, 0x01, 0x80, 0x00, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x80, 0x80, "1 Coin  2 Credits"		},

	OVERLAY_DIP(0)
};

STDDIPINFO(Spaceat2)

static struct BurnDIPInfo SuperinvDIPList[]=
{
	DEFAULTS(0x0c, 0xff, 0x08, 0x00, 0x00, 0x01)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x08, "1500"						},
	{0x02, 0x01, 0x08, 0x00, "2500"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Superinv)

static struct BurnDIPInfo SinvemagDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x89, 0x00, 0x00, 0x01)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x08, "2000"						},
	{0x02, 0x01, 0x08, 0x00, "3000"						},

	{0   , 0xfe, 0   ,    2, "Coin Info"				},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Sinvemag)

static struct BurnDIPInfo JspecterDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x09, 0x00, 0x00, 0x01)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x08, "1000"						},
	{0x02, 0x01, 0x08, 0x00, "1500"						},

	{0   , 0xfe, 0   ,    2, "Coin Info"				},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Jspecter)

static struct BurnDIPInfo SearthinDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x89, 0x8b, 0x00, 0x01)

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x02, 0x01, 0x01, 0x00, "2"						},
	{0x02, 0x01, 0x01, 0x01, "3"						},

	{0   , 0xfe, 0   ,    2, "Pence Coinage"			},
	{0x02, 0x01, 0x02, 0x02, "Off"						},
	{0x02, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x02, 0x01, 0x80, 0x00, "2C/1C 50p/3C (+ Bonus Life)"	},
	{0x02, 0x01, 0x80, 0x80, "1C/1C 50p/5C"				},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Searthin)

static struct BurnDIPInfo AlieninvDIPList[]=
{
	DEFAULTS(0x0b, 0x00, 0x81, 0x8b, 0x00, 0x01)

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x02, 0x01, 0x01, 0x00, "2"						},
	{0x02, 0x01, 0x01, 0x01, "3"						},

	{0   , 0xfe, 0   ,    2, "Pence Coinage"			},
	{0x02, 0x01, 0x02, 0x02, "Off"						},
	{0x02, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x02, 0x01, 0x80, 0x00, "2C/1C 50p/3C (+ Bonus Life)"	},
	{0x02, 0x01, 0x80, 0x80, "1C/1C 50p/5C"				},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Alieninv)

static struct BurnDIPInfo InvasionDIPList[]=
{
	DEFAULTS(0x09, 0x08, 0x89, 0x83, 0xfd, 0x01)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x08, "1500"						},
	{0x02, 0x01, 0x08, 0x00, "2500"						},

	{0   , 0xfe, 0   ,    2, "Laser Bonus Info"			},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Coin counter"				},
	{0x03, 0x01, 0x04, 0x00, "Disconnected"				},
	{0x03, 0x01, 0x04, 0x04, "Connected"				},

	OVERLAY_DIP(0)
};

STDDIPINFO(Invasion)

static struct BurnDIPInfo Invadpt2DIPList[]=
{
	DEFAULTS(0x0b, 0xf4, 0x81, 0x00, 0x00, 0x00)

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x02, 0x01, 0x01, 0x00, "3"						},
	{0x02, 0x01, 0x01, 0x01, "4"						},

	{0   , 0xfe, 0   ,    2, "Preset Mode"				},
	{0x02, 0x01, 0x08, 0x00, "Game Mode"				},
	{0x02, 0x01, 0x08, 0x08, "Name Entry"				},

	{0   , 0xfe, 0   ,    2, "Coin Info"				},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},

	CABINET_DIP(0)
};

STDDIPINFO(Invadpt2)

static struct BurnDIPInfo SpclaserDIPList[]=
{
	DEFAULTS(0x0b, 0x00, 0x00, 0x00, 0x00, 0x00)

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x02, 0x01, 0x80, 0x00, "1 Coin/1 Or 2 Players"	},
	{0x02, 0x01, 0x80, 0x80, "1 Coin/1 Player  2 Coins/2 Players"	},

	CABINET_DIP(0)
};

STDDIPINFO(Spclaser)

static struct BurnDIPInfo GalxwarsDIPList[]=
{
	DEFAULTS(0x0b, 0x40, 0x89, 0x01, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x02, 0x01, 0x03, 0x00, "2"						},
	{0x02, 0x01, 0x03, 0x01, "3"						},
	{0x02, 0x01, 0x03, 0x02, "4"						},
	{0x02, 0x01, 0x03, 0x03, "5"						},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x00, "3000"						},
	{0x02, 0x01, 0x08, 0x08, "5000"						},

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x02, 0x01, 0x80, 0x00, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x80, 0x80, "1 Coin  2 Credits"		},

	CABINET_DIP(0)
};

STDDIPINFO(Galxwars)

static struct BurnDIPInfo LrescueDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x89, 0x00, 0x00, 0x00)

	LIVES_3456(0x02)

	CABINET_DIP(0)
};

STDDIPINFO(Lrescue)

static struct BurnDIPInfo EscmarsDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x89, 0x00, 0x00, 0x01)

	LIVES_3456(0x02)

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Escmars)

static struct BurnDIPInfo CosmoDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x89, 0x80, 0x00, 0x00)

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x02, 0x01, 0x01, 0x00, "3"						},
	{0x02, 0x01, 0x01, 0x01, "4"						},

	CABINET_DIP(0)
};

STDDIPINFO(Cosmo)

static struct BurnDIPInfo VortexDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x89, 0x8f, 0x00, 0x00)

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x02, 0x01, 0x80, 0x00, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x80, 0x80, "1 Coin  1 Credits"		},

	CABINET_DIP(0)
};

STDDIPINFO(Vortex)

static struct BurnDIPInfo RollingcDIPList[]=
{
	DEFAULTS(0x0b, 0xf8, 0x88, 0x08, 0x00, 0x00)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x08, "RC=3000 / MB=1000"		},
	{0x02, 0x01, 0x08, 0x00, "RC=5000 / MB=2000"		},

	{0   , 0xfe, 0   ,    2, "Coin Info"				},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},

	CABINET_DIP(0)
};

STDDIPINFO(Rollingc)

static struct BurnDIPInfo MrakerDIPList[]=
{
	DEFAULTS(0x10, 0x00, 0x00, 0x10, 0x00, 0x00)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x04, 0x00, "5000"						},
	{0x02, 0x01, 0x04, 0x04, "7000"						},

	{0   , 0xfe, 0   ,    4, "Cabinet"					},
	{0x02, 0x01, 0x30, 0x00, "Cocktail"					},
	{0x02, 0x01, 0x30, 0x10, "Upright (Normal)"			},
	{0x02, 0x01, 0x30, 0x20, "Upright (Flip Screen)"	},
	{0x02, 0x01, 0x30, 0x30, "Deluxe (2 Coins/1 Credit)"},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x02, 0x01, 0xc0, 0x00, "Easy"						},
	{0x02, 0x01, 0xc0, 0x40, "Hard"						},
	{0x02, 0x01, 0xc0, 0x80, "Harder"					},
	{0x02, 0x01, 0xc0, 0xc0, "Hardest"					},
};

STDDIPINFO(Mraker)

static struct BurnDIPInfo OzmawarsDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x88, 0x00, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Energy"					},
	{0x02, 0x01, 0x03, 0x00, "15000"					},
	{0x02, 0x01, 0x03, 0x01, "20000"					},
	{0x02, 0x01, 0x03, 0x02, "25000"					},
	{0x02, 0x01, 0x03, 0x03, "35000"					},

	{0   , 0xfe, 0   ,    2, "Bonus Energy"				},
	{0x02, 0x01, 0x08, 0x00, "15000"					},
	{0x02, 0x01, 0x08, 0x08, "10000"					},

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x02, 0x01, 0x80, 0x00, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x80, 0x80, "1 Coin  2 Credits"		},

	CABINET_DIP(0)
};

STDDIPINFO(Ozmawars)

static struct BurnDIPInfo SpacephDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x88, 0x00, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Energy"					},
	{0x02, 0x01, 0x03, 0x00, "15000"					},
	{0x02, 0x01, 0x03, 0x01, "20000"					},
	{0x02, 0x01, 0x03, 0x02, "25000"					},
	{0x02, 0x01, 0x03, 0x03, "35000"					},

	{0   , 0xfe, 0   ,    2, "Bonus Energy"				},
	{0x02, 0x01, 0x08, 0x00, "15000"					},
	{0x02, 0x01, 0x08, 0x08, "10000"					},

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x02, 0x01, 0x80, 0x00, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x80, 0x80, "1 Coin  2 Credits"		},

	CABINET_DIP(0)
};

STDDIPINFO(Spaceph)

static struct BurnDIPInfo BallbombDIPList[]=
{
	DEFAULTS(0x0b, 0xff, 0x88, 0x00, 0x00, 0x00)

	LIVES_3456(0x02)

	CABINET_DIP(0)
};

STDDIPINFO(Ballbomb)

static struct BurnDIPInfo SchaserDIPList[]=
{
	DEFAULTS(0x10, 0x00, 0x00, 0x00, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Hard Starting Level"		},
	{0x00, 0x01, 0x60, 0x00, "3"						},
	{0x00, 0x01, 0x60, 0x20, "4"						},
	{0x00, 0x01, 0x60, 0x40, "5"						},
	{0x00, 0x01, 0x60, 0x60, "6"						},

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x02, 0x01, 0x08, 0x00, "Easy"						},
	{0x02, 0x01, 0x08, 0x08, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x02, 0x01, 0x40, 0x00, "Upright"					},
	{0x02, 0x01, 0x40, 0x40, "Cocktail"					},
};

STDDIPINFO(Schaser)

static struct BurnDIPInfo SchasermDIPList[]=
{
	DEFAULTS(0x10, 0x00, 0x00, 0x00, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Hard Starting Level"		},
	{0x00, 0x01, 0x60, 0x00, "3"						},
	{0x00, 0x01, 0x60, 0x20, "4"						},
	{0x00, 0x01, 0x60, 0x40, "5"						},
	{0x00, 0x01, 0x60, 0x60, "6"						},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x02, 0x01, 0x03, 0x00, "1"						},
	{0x02, 0x01, 0x03, 0x01, "2"						},
	{0x02, 0x01, 0x03, 0x02, "3"						},
	{0x02, 0x01, 0x03, 0x03, "4"						},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x02, 0x01, 0x08, 0x00, "Easy"						},
	{0x02, 0x01, 0x08, 0x08, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x02, 0x01, 0x40, 0x00, "Upright"					},
	{0x02, 0x01, 0x40, 0x40, "Cocktail"					},
};

STDDIPINFO(Schaserm)

static struct BurnDIPInfo SchasercvDIPList[]=
{
	DEFAULTS(0x0e, 0x00, 0x01, 0x00, 0x00, 0x00)

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x02, 0x01, 0x01, 0x00, "3"						},
	{0x02, 0x01, 0x01, 0x01, "4"						},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x02, 0x01, 0x08, 0x00, "Easy"						},
	{0x02, 0x01, 0x08, 0x08, "Hard"						},

	CABINET_DIP(0)
};

STDDIPINFO(Schasercv)

static struct BurnDIPInfo Lupin3DIPList[]=
{
	DEFAULTS(0x0f, 0x03, 0x00, 0x00, 0x00, 0x00)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x02, 0x01, 0x04, 0x00, "Upright"					},
	{0x02, 0x01, 0x04, 0x04, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Bags To Collect"			},
	{0x02, 0x01, 0x08, 0x08, "2"						},
	{0x02, 0x01, 0x08, 0x00, "8"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x02, 0x01, 0x80, 0x00, "Off"						},
	{0x02, 0x01, 0x80, 0x80, "On"						},
};

STDDIPINFO(Lupin3)

static struct BurnDIPInfo Lupin3aDIPList[]=
{
	DEFAULTS(0x0f, 0x00, 0x00, 0x00, 0x00, 0x00)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x02, 0x01, 0x04, 0x00, "Upright"					},
	{0x02, 0x01, 0x04, 0x04, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Bags To Collect"			},
	{0x02, 0x01, 0x08, 0x08, "2"						},
	{0x02, 0x01, 0x08, 0x00, "8"						},

	{0   , 0xfe, 0   ,    2, "Language"					},
	{0x02, 0x01, 0x10, 0x00, "English"					},
	{0x02, 0x01, 0x10, 0x10, "Japanese"					},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x02, 0x01, 0x80, 0x00, "Off"						},
	{0x02, 0x01, 0x80, 0x80, "On"						},
};

STDDIPINFO(Lupin3a)

static struct BurnDIPInfo PolarisDIPList[]=
{
	DEFAULTS(0x0f, 0x00, 0x01, 0x10, 0x00, 0x00)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x02, 0x01, 0x04, 0x00, "Upright"					},
	{0x02, 0x01, 0x04, 0x04, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Invincible Test"			},
	{0x02, 0x01, 0x08, 0x00, "Off"						},
	{0x02, 0x01, 0x08, 0x08, "On"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x02, 0x01, 0x10, 0x00, "Off"						},
	{0x02, 0x01, 0x10, 0x10, "On"						},

	{0   , 0xfe, 0   ,    2, "High Score Preset Mode"	},
	{0x02, 0x01, 0x80, 0x00, "Off"						},
	{0x02, 0x01, 0x80, 0x80, "On"						},
};

STDDIPINFO(Polaris)

static struct BurnDIPInfo IndianbtDIPList[]=
{
	DEFAULTS(0x0b, 0x00, 0x81, 0x00, 0x00, 0x00)

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Number of Catch Animals"	},
	{0x02, 0x01, 0x08, 0x00, "6"						},
	{0x02, 0x01, 0x08, 0x08, "3"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x02, 0x01, 0x80, 0x00, "Off"						},
	{0x02, 0x01, 0x80, 0x80, "On"						},

	CABINET_DIP(0)
};

STDDIPINFO(Indianbt)

static struct BurnDIPInfo IndianbtbrDIPList[]=
{
	DEFAULTS(0x0b, 0x00, 0x81, 0x80, 0x00, 0x00)

	LIVES_3456(0x00)

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x02, 0x01, 0x08, 0x00, "Off"						},
	{0x02, 0x01, 0x08, 0x08, "On"						},

	CABINET_DIP(0)
};

STDDIPINFO(Indianbtbr)

static struct BurnDIPInfo SteelwkrDIPList[]=
{
	DEFAULTS(0x0d, 0x00, 0x01, 0x02, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x02, 0x01, 0x03, 0x00, "1"						},
	{0x02, 0x01, 0x03, 0x01, "2"						},
	{0x02, 0x01, 0x03, 0x02, "3"						},
	{0x02, 0x01, 0x03, 0x03, "4"						},

	CABINET_DIP(0)
};

STDDIPINFO(Steelwkr)

static struct BurnDIPInfo GalacticDIPList[]=
{
	DEFAULTS(0x0b, 0x00, 0x01, 0x02, 0x00, 0x01)

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x02, 0x01, 0x03, 0x00, "Off"						},
	{0x02, 0x01, 0x03, 0x01, "4000"						},
	{0x02, 0x01, 0x03, 0x02, "5000"						},
	{0x02, 0x01, 0x03, 0x03, "7000"						},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x02, 0x01, 0x08, 0x00, "3"						},
	{0x02, 0x01, 0x08, 0x08, "6"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Galactic)

static struct BurnDIPInfo InvrvngeDIPList[]=
{
	DEFAULTS(0x0b, 0x80, 0x81, 0x08, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x00, 0x01, 0x06, 0x00, "Easy"						},
	{0x00, 0x01, 0x06, 0x02, "Hard"						},
	{0x00, 0x01, 0x06, 0x04, "Harder"					},
	{0x00, 0x01, 0x06, 0x06, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "Fuel Destroyed by Comet"	},
	{0x00, 0x01, 0x80, 0x00, "3"						},
	{0x00, 0x01, 0x80, 0x80, "6"						},

	LIVES_3456(0x02)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x08, 0x00, "1500"						},
	{0x02, 0x01, 0x08, 0x08, "2000"						},

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x02, 0x01, 0x80, 0x80, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x80, 0x00, "1 Coin  1 Credits"		},

	CABINET_DIP(0)
};

STDDIPINFO(Invrvnge)

static struct BurnDIPInfo YosakdonDIPList[]=
{
	DEFAULTS(0x0a, 0x00, 0x00, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0x03, 0x00, "3"						},
	{0x01, 0x01, 0x03, 0x01, "4"						},
	{0x01, 0x01, 0x03, 0x02, "5"						},
	{0x01, 0x01, 0x03, 0x03, "6"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Yosakdon)

static struct BurnDIPInfo ShuttleiDIPList[]=
{
	DEFAULTS(0x0a, 0x00, 0x00, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x00, 0x01, 0x01, 0x01, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x01, 0x00, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x06, 0x00, "3"						},
	{0x00, 0x01, 0x06, 0x02, "4"						},
	{0x00, 0x01, 0x06, 0x04, "5"						},
	{0x00, 0x01, 0x06, 0x06, "6"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Shuttlei)

static struct BurnDIPInfo SkyloveDIPList[]=
{
	DEFAULTS(0x0a, 0x00, 0xc7, 0xc4, 0x00, 0x01)

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x00, 0x01, 0x01, 0x01, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x01, 0x00, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x06, 0x00, "3"						},
	{0x00, 0x01, 0x06, 0x02, "4"						},
	{0x00, 0x01, 0x06, 0x04, "5"						},
	{0x00, 0x01, 0x06, 0x06, "6"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x00, 0x01, 0x60, 0x00, "10k"						},
	{0x00, 0x01, 0x60, 0x20, "20k"						},
	{0x00, 0x01, 0x60, 0x40, "40k"						},
	{0x00, 0x01, 0x60, 0x60, "60k"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Skylove)

static struct BurnDIPInfo DarthvdrDIPList[]=
{
	DEFAULTS(0x0a, 0x7f, 0xff, 0x00, 0x00, 0x00)

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x00, 0x01, 0x20, 0x00, "1000"						},
	{0x00, 0x01, 0x20, 0x20, "3000"						},

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x00, 0x01, 0x80, 0x00, "3 (4 with One less life off)"	},
	{0x00, 0x01, 0x80, 0x80, "5 (6 with One less life off)"	},

	{0   , 0xfe, 0   ,    2, "One less life"			},
	{0x01, 0x01, 0x10, 0x10, "Off"						},
	{0x01, 0x01, 0x10, 0x00, "On"						},

	CABINET_DIP(0)
};

STDDIPINFO(Darthvdr)

static struct BurnDIPInfo ClaybustDIPList[]=
{
	DEFAULTS(0x06, 0x00, 0xbc, 0x00, 0x00, 0x00)

	{0   , 0xfe, 0   ,    2, "Shots"					},
	{0x01, 0x01, 0x10, 0x10, "4"						},
	{0x01, 0x01, 0x10, 0x00, "2"						},

	{0   , 0xfe, 0   ,    2, "Number of Flings"			},
	{0x01, 0x01, 0x40, 0x40, "8"						},
	{0x01, 0x01, 0x40, 0x00, "10"						},
};

STDDIPINFO(Claybust)

static struct BurnDIPInfo GunchampDIPList[]=
{
	DEFAULTS(0x06, 0x00, 0x5c, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    2, "Enter Initials"			},
	{0x01, 0x01, 0x10, 0x10, "Yes"						},
	{0x01, 0x01, 0x10, 0x00, "No"						},

	{0   , 0xfe, 0   ,    8, "Coinage"					},
	{0x01, 0x01, 0xe0, 0xa0, "4 Coins 1 Credits"		},
	{0x01, 0x01, 0xe0, 0xc0, "3 Coins 1 Credits"		},
	{0x01, 0x01, 0xe0, 0xe0, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0xe0, 0x40, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0xe0, 0x20, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0xe0, 0x00, "2 Coins 3 Credits"		},
	{0x01, 0x01, 0xe0, 0x60, "1 Coin  3 Credits"		},
	{0x01, 0x01, 0xe0, 0x80, "1 Coin  4 Credits"		},

	OVERLAY_DIP(0)
};

STDDIPINFO(Gunchamp)

static struct BurnDIPInfo CaneDIPList[]=
{
	DEFAULTS(0x07, 0x00, 0x7c, 0x00, 0x00, 0x00)
};

STDDIPINFO(Cane)

static struct BurnDIPInfo OrbiteDIPList[]=
{
	DEFAULTS(0x04, 0x00, 0x00, 0x00, 0x00, 0x00)
};

STDDIPINFO(Orbite)

static struct BurnDIPInfo AstropalDIPList[]=
{
	DEFAULTS(0x09, 0xfc, 0x81, 0x00, 0x80, 0x00)

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x03, 0x00, "2"						},
	{0x00, 0x01, 0x03, 0x01, "3"						},
	{0x00, 0x01, 0x03, 0x02, "4"						},
	{0x00, 0x01, 0x03, 0x03, "5"						},
};

STDDIPINFO(Astropal)

static struct BurnDIPInfo AttackfcDIPList[]=
{
	DEFAULTS(0x05, 0xff, 0x00, 0x00, 0x00, 0x01)

	OVERLAY_DIP(0)
};

STDDIPINFO(Attackfc)

static struct BurnDIPInfo AttackfcuDIPList[]=
{
	DEFAULTS(0x05, 0x01, 0x00, 0x00, 0x00, 0x01)

	OVERLAY_DIP(0)
};

STDDIPINFO(Attackfcu)

static struct BurnDIPInfo SpacecomDIPList[]=
{
	DEFAULTS(0x0b, 0x1b, 0x00, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    2, "Lives"					},
	{0x00, 0x01, 0x04, 0x00, "3"						},
	{0x00, 0x01, 0x04, 0x04, "5"						},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x00, 0x01, 0x08, 0x08, "1000"						},
	{0x00, 0x01, 0x08, 0x00, "1500"						},

	OVERLAY_DIP(0)
	CABINET_DIP(0)
};

STDDIPINFO(Spacecom)

static struct BurnDIPInfo Invad2ctDIPList[]=
{
	DEFAULTS(0x0a, 0xf5, 0x81, 0x8c, 0x00, 0x01)

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x00, 0x01, 0x01, 0x01, "Off"						},
	{0x00, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x02, 0x01, 0x03, 0x02, "2 Coins 1 Credits"		},
	{0x02, 0x01, 0x03, 0x03, "2 Coins 2 Credits"		},
	{0x02, 0x01, 0x03, 0x00, "1 Coin  1 Credits"		},
	{0x02, 0x01, 0x03, 0x01, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x02, 0x01, 0x08, 0x08, "Upright"					},
	{0x02, 0x01, 0x08, 0x00, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x02, 0x01, 0x80, 0x80, "1500"						},
	{0x02, 0x01, 0x80, 0x00, "2000"						},

	OVERLAY_DIP(0)
};

STDDIPINFO(Invad2ct)

static struct BurnDIPInfo MazeDIPList[]=
{
	DEFAULTS(0x0c, 0x00, 0x40, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x01, 0x01, 0x30, 0x10, "2 Coins 1 Credits"		},
	{0x01, 0x01, 0x30, 0x00, "1 Coin  1 Credits"		},
	{0x01, 0x01, 0x30, 0x30, "2 Coins 3 Credits"		},
	{0x01, 0x01, 0x30, 0x20, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "2 Player Game Time"		},
	{0x01, 0x01, 0x40, 0x40, "4 minutes"				},
	{0x01, 0x01, 0x40, 0x00, "6 minutes"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x01, 0x01, 0x80, 0x00, "Off"						},
	{0x01, 0x01, 0x80, 0x80, "On"						},

	OVERLAY_DIP(0)
};

STDDIPINFO(Maze)

static struct BurnDIPInfo GmissileDIPList[]=
{
	DEFAULTS(0x0a, 0xff, 0xff, 0x9b, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Coinage"					},
	{0x02, 0x01, 0x03, 0x01, "2 Coins per Player"		},
	{0x02, 0x01, 0x03, 0x00, "2 Coins/1 or 2 Players"	},
	{0x02, 0x01, 0x03, 0x03, "1 Coin per Player"		},
	{0x02, 0x01, 0x03, 0x02, "1 Coin/1 or 2 Players"	},

	{0   , 0xfe, 0   ,    4, "Game Time"				},
	{0x02, 0x01, 0x0c, 0x00, "60 seconds + 30 extended"	},
	{0x02, 0x01, 0x0c, 0x08, "70 seconds + 35 extended"	},
	{0x02, 0x01, 0x0c, 0x04, "80 seconds + 40 extended"	},
	{0x02, 0x01, 0x0c, 0x0c, "90 seconds + 45 extended"	},

	{0   , 0xfe, 0   ,    4, "Extended Time At"			},
	{0x02, 0x01, 0x30, 0x00, "500"						},
	{0x02, 0x01, 0x30, 0x20, "700"						},
	{0x02, 0x01, 0x30, 0x10, "1000"						},
	{0x02, 0x01, 0x30, 0x30, "1300"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Gmissile)

static struct BurnDIPInfo CheckmatDIPList[]=
{
	DEFAULTS(0x16, 0x00, 0x00, 0x04, 0x00, 0x00)

	{0   , 0xfe, 0   ,    2, "Coinage"					},
	{0x02, 0x01, 0x01, 0x00, "1 Coin/1 or 2 Players"	},
	{0x02, 0x01, 0x01, 0x01, "1 Coin/1 or 2 Players, 2 Coins/3 or 4 Players"	},

	{0   , 0xfe, 0   ,    4, "Rounds"					},
	{0x02, 0x01, 0x0c, 0x00, "2"						},
	{0x02, 0x01, 0x0c, 0x04, "3"						},
	{0x02, 0x01, 0x0c, 0x08, "4"						},
	{0x02, 0x01, 0x0c, 0x0c, "5"						},

	{0   , 0xfe, 0   ,    4, "Language"					},
	{0x02, 0x01, 0x60, 0x00, "English"					},
	{0x02, 0x01, 0x60, 0x20, "Language 2"				},
	{0x02, 0x01, 0x60, 0x40, "Language 3"				},
	{0x02, 0x01, 0x60, 0x60, "Language 4"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x02, 0x01, 0x80, 0x00, "Off"						},
	{0x02, 0x01, 0x80, 0x80, "On"						},
};

STDDIPINFO(Checkmat)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo SeawolfInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy2 + 0,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy2 + 1,     "p1 start"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoyX + 0,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoyX + 1,     "p1 right"},
	A("P1 Periscope X",      BIT_ANALOG_REL, DrvAnalog + 0,   "mouse x-axis"),
	A("P1 Periscope Y",      BIT_ANALOG_REL, DrvAnalog + 1,   "mouse y-axis"),
	{"P1 Button 1",         BIT_DIGITAL,	DrvJoy1 + 5,     "mouse button 1"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};
#undef A

STDINPUTINFO(Seawolf)

static struct BurnDIPInfo SeawolfDIPList[]=
{
	DEFAULTS(0x08, 0x40, 0x60, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    4, "Game Time"},
	{0x00, 0x01, 0xc0, 0x00, "60 seconds + 20 extended"},
	{0x00, 0x01, 0xc0, 0x40, "70 seconds + 20 extended"},
	{0x00, 0x01, 0xc0, 0x80, "80 seconds + 20 extended"},
	{0x00, 0x01, 0xc0, 0xc0, "90 seconds + 20 extended"},

	{0   , 0xfe, 0   ,    4, "Coinage"},
	{0x01, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"},
	{0x01, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"},
	{0x01, 0x01, 0x0c, 0x0c, "2 Coins 3 Credits"},
	{0x01, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"},

	{0   , 0xfe, 0   ,    8, "Extended Time At"},
	{0x01, 0x01, 0xe0, 0x00, "None"},
	{0x01, 0x01, 0xe0, 0x20, "2000"},
	{0x01, 0x01, 0xe0, 0x40, "3000"},
	{0x01, 0x01, 0xe0, 0x60, "4000"},
	{0x01, 0x01, 0xe0, 0x80, "5000"},
	{0x01, 0x01, 0xe0, 0xa0, "6000"},
	{0x01, 0x01, 0xe0, 0xc0, "7000"},
	{0x01, 0x01, 0xe0, 0xe0, "Test Mode"},

	OVERLAY_DIP(0)
};

STDDIPINFO(Seawolf)

static struct BurnInputInfo GunfightInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy3 + 6,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy3 + 7,     "p1 start"},
	{"P1 Up",               BIT_DIGITAL,	DrvJoy1 + 0,     "p1 up"},
	{"P1 Down",             BIT_DIGITAL,	DrvJoy1 + 1,     "p1 down"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoy1 + 2,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoy1 + 3,     "p1 right"},
	{"P1 Button 1",         BIT_DIGITAL,	DrvJoy1 + 7,     "p1 fire 1"},
	{"P1 Gun Up",           BIT_DIGITAL,	DrvJoyX + 0,     "p1 fire 2"},
	{"P1 Gun Down",         BIT_DIGITAL,	DrvJoyX + 1,     "p1 fire 3"},
	{"P2 Up",               BIT_DIGITAL,	DrvJoy2 + 0,     "p2 up"},
	{"P2 Down",             BIT_DIGITAL,	DrvJoy2 + 1,     "p2 down"},
	{"P2 Left",             BIT_DIGITAL,	DrvJoy2 + 2,     "p2 left"},
	{"P2 Right",            BIT_DIGITAL,	DrvJoy2 + 3,     "p2 right"},
	{"P2 Button 1",         BIT_DIGITAL,	DrvJoy2 + 7,     "p2 fire 1"},
	{"P2 Gun Up",           BIT_DIGITAL,	DrvJoyX + 2,     "p2 fire 2"},
	{"P2 Gun Down",         BIT_DIGITAL,	DrvJoyX + 3,     "p2 fire 3"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};

STDINPUTINFO(Gunfight)

static struct BurnDIPInfo GunfightDIPList[]=
{
	DEFAULTS(0x11, 0x00, 0x00, 0x10, 0x00, 0x01)

	{0   , 0xfe, 0   ,   16, "Coinage"},
	{0x02, 0x01, 0x0f, 0x03, "4 Coins 1 Credits"},
	{0x02, 0x01, 0x0f, 0x02, "3 Coins 1 Credits"},
	{0x02, 0x01, 0x0f, 0x07, "4 Coins 2 Credits"},
	{0x02, 0x01, 0x0f, 0x01, "2 Coins 1 Credits"},
	{0x02, 0x01, 0x0f, 0x06, "3 Coins 2 Credits"},
	{0x02, 0x01, 0x0f, 0x0b, "4 Coins 3 Credits"},
	{0x02, 0x01, 0x0f, 0x0f, "4 Coins 4 Credits"},
	{0x02, 0x01, 0x0f, 0x0a, "3 Coins 3 Credits"},
	{0x02, 0x01, 0x0f, 0x05, "2 Coins 2 Credits"},
	{0x02, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"},
	{0x02, 0x01, 0x0f, 0x0e, "3 Coins 4 Credits"},
	{0x02, 0x01, 0x0f, 0x09, "2 Coins 3 Credits"},
	{0x02, 0x01, 0x0f, 0x0d, "2 Coins 4 Credits"},
	{0x02, 0x01, 0x0f, 0x04, "1 Coin  2 Credits"},
	{0x02, 0x01, 0x0f, 0x08, "1 Coin  3 Credits"},
	{0x02, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"},

	{0   , 0xfe, 0   ,    4, "Game Time"},
	{0x02, 0x01, 0x30, 0x00, "60 seconds"},
	{0x02, 0x01, 0x30, 0x10, "70 seconds"},
	{0x02, 0x01, 0x30, 0x20, "80 seconds"},
	{0x02, 0x01, 0x30, 0x30, "90 seconds"},

	OVERLAY_DIP(0)
};

STDDIPINFO(Gunfight)

static struct BurnInputInfo TornbaseInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy3 + 0,     "p1 coin"},
	{"P1 Hit",              BIT_DIGITAL,	DrvJoy1 + 0,     "p1 fire 1"},
	{"P1 Move Outfield Left", BIT_DIGITAL,	DrvJoy1 + 1,     "p1 fire 2"},
	{"P1 Move Outfield Right", BIT_DIGITAL,	DrvJoy1 + 2,     "p1 fire 3"},
	{"P1 Pitch Left",       BIT_DIGITAL,	DrvJoy1 + 3,     "p1 left"},
	{"P1 Pitch Right",      BIT_DIGITAL,	DrvJoy1 + 4,     "p1 right"},
	{"P1 Pitch Slow",       BIT_DIGITAL,	DrvJoy1 + 5,     "p1 up"},
	{"P1 Pitch Fast",       BIT_DIGITAL,	DrvJoy1 + 6,     "p1 down"},
	{"P2 Hit",              BIT_DIGITAL,	DrvJoy2 + 0,     "p2 fire 1"},
	{"P2 Move Outfield Left", BIT_DIGITAL,	DrvJoy2 + 1,     "p2 fire 2"},
	{"P2 Move Outfield Right", BIT_DIGITAL,	DrvJoy2 + 2,     "p2 fire 3"},
	{"P2 Pitch Left",       BIT_DIGITAL,	DrvJoy2 + 3,     "p2 left"},
	{"P2 Pitch Right",      BIT_DIGITAL,	DrvJoy2 + 4,     "p2 right"},
	{"P2 Pitch Slow",       BIT_DIGITAL,	DrvJoy2 + 5,     "p2 up"},
	{"P2 Pitch Fast",       BIT_DIGITAL,	DrvJoy2 + 6,     "p2 down"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};

STDINPUTINFO(Tornbase)

static struct BurnDIPInfo TornbaseDIPList[]=
{
	DEFAULTS(0x10, 0x00, 0x00, 0x40, 0x00, 0x00)

	{0   , 0xfe, 0   ,    2, "Unused"},
	{0x00, 0x01, 0x80, 0x00, "Off"},
	{0x00, 0x01, 0x80, 0x80, "On"},

	{0   , 0xfe, 0   ,   16, "Coinage"},
	{0x02, 0x01, 0x78, 0x18, "4 Coins/1 Inning"},
	{0x02, 0x01, 0x78, 0x10, "3 Coins/1 Inning"},
	{0x02, 0x01, 0x78, 0x38, "4 Coins/2 Innings"},
	{0x02, 0x01, 0x78, 0x08, "2 Coins/1 Inning"},
	{0x02, 0x01, 0x78, 0x30, "3 Coins/2 Innings"},
	{0x02, 0x01, 0x78, 0x28, "2 Coins/2 Innings"},
	{0x02, 0x01, 0x78, 0x00, "1 Coin/1 Inning"},
	{0x02, 0x01, 0x78, 0x58, "4 Coins/4 Innings"},
	{0x02, 0x01, 0x78, 0x50, "3 Coins/4 Innings"},
	{0x02, 0x01, 0x78, 0x48, "2 Coins/4 Innings"},
	{0x02, 0x01, 0x78, 0x20, "1 Coin/2 Innings"},
	{0x02, 0x01, 0x78, 0x40, "1 Coin/4 Innings"},
	{0x02, 0x01, 0x78, 0x78, "4 Coins/9 Innings"},
	{0x02, 0x01, 0x78, 0x70, "3 Coins/9 Innings"},
	{0x02, 0x01, 0x78, 0x68, "2 Coins/9 Innings"},
	{0x02, 0x01, 0x78, 0x60, "1 Coin/9 Innings"},

	{0   , 0xfe, 0   ,    2, "Service Mode"},
	{0x02, 0x01, 0x80, 0x00, "Off"},
	{0x02, 0x01, 0x80, 0x80, "On"},

};

STDDIPINFO(Tornbase)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo ZzzapInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy1 + 6,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy1 + 7,     "p1 start"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoyX + 0,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoyX + 1,     "p1 right"},
	A("P1 Wheel",            BIT_ANALOG_REL, DrvAnalog + 0,   "p1 x-axis"),
	{"P1 Accelerate",       BIT_DIGITAL,	DrvJoyX + 2,     "p1 fire 1"},
	A("P1 Pedal",            BIT_ANALOG_REL, DrvAnalog + 1,   "p1 z-axis"),
	{"P1 Gear Shift",       BIT_DIGITAL,	DrvJoyX + 4,     "p1 fire 2"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};
#undef A

STDINPUTINFO(Zzzap)

static struct BurnDIPInfo ZzzapDIPList[]=
{
	DEFAULTS(0x09, 0xc0, 0x00, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    4, "Coinage"},
	{0x02, 0x01, 0x03, 0x02, "2 Coins 1 Credits"},
	{0x02, 0x01, 0x03, 0x00, "1 Coin  1 Credits"},
	{0x02, 0x01, 0x03, 0x03, "2 Coins 3 Credits"},
	{0x02, 0x01, 0x03, 0x01, "1 Coin  2 Credits"},

	{0   , 0xfe, 0   ,    4, "Game Time"},
	{0x02, 0x01, 0x0c, 0x0c, "60 seconds + 30 extended"},
	{0x02, 0x01, 0x0c, 0x00, "80 seconds + 40 extended"},
	{0x02, 0x01, 0x0c, 0x08, "99 seconds + 50 extended"},
	{0x02, 0x01, 0x0c, 0x04, "Test Mode"},

	{0   , 0xfe, 0   ,    3, "Extended Time At"},
	{0x02, 0x01, 0x30, 0x10, "2.00"},
	{0x02, 0x01, 0x30, 0x00, "2.50"},
	{0x02, 0x01, 0x30, 0x20, "None"},

	{0   , 0xfe, 0   ,    4, "Language"},
	{0x02, 0x01, 0xc0, 0x00, "English"},
	{0x02, 0x01, 0xc0, 0x40, "German"},
	{0x02, 0x01, 0xc0, 0x80, "French"},
	{0x02, 0x01, 0xc0, 0xc0, "Spanish"},

	OVERLAY_DIP(0)
};

STDDIPINFO(Zzzap)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo LagunarInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy1 + 6,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy1 + 7,     "p1 start"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoyX + 0,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoyX + 1,     "p1 right"},
	A("P1 Wheel",            BIT_ANALOG_REL, DrvAnalog + 0,   "p1 x-axis"),
	{"P1 Accelerate",       BIT_DIGITAL,	DrvJoyX + 2,     "p1 fire 1"},
	A("P1 Pedal",            BIT_ANALOG_REL, DrvAnalog + 1,   "p1 z-axis"),
	{"P1 Gear Shift",       BIT_DIGITAL,	DrvJoyX + 4,     "p1 fire 2"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};
#undef A

STDINPUTINFO(Lagunar)

static struct BurnDIPInfo LagunarDIPList[]=
{
	DEFAULTS(0x09, 0xc0, 0x00, 0x24, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Coinage"},
	{0x02, 0x01, 0x03, 0x02, "2 Coins 1 Credits"},
	{0x02, 0x01, 0x03, 0x00, "1 Coin  1 Credits"},
	{0x02, 0x01, 0x03, 0x03, "2 Coins 3 Credits"},
	{0x02, 0x01, 0x03, 0x01, "1 Coin  2 Credits"},

	{0   , 0xfe, 0   ,    4, "Game Time"},
	{0x02, 0x01, 0x0c, 0x00, "45 seconds + 22 extended"},
	{0x02, 0x01, 0x0c, 0x04, "60 seconds + 30 extended"},
	{0x02, 0x01, 0x0c, 0x08, "75 seconds + 37 extended"},
	{0x02, 0x01, 0x0c, 0x0c, "90 seconds + 45 extended"},

	{0   , 0xfe, 0   ,    4, "Extended Time At"},
	{0x02, 0x01, 0x30, 0x00, "350"},
	{0x02, 0x01, 0x30, 0x10, "400"},
	{0x02, 0x01, 0x30, 0x20, "450"},
	{0x02, 0x01, 0x30, 0x30, "500"},

	{0   , 0xfe, 0   ,    4, "Test Modes/Extended Time"},
	{0x02, 0x01, 0xc0, 0x00, "Off"},
	{0x02, 0x01, 0xc0, 0x40, "RAM/ROM Test"},
	{0x02, 0x01, 0xc0, 0x80, "Input Test"},
	{0x02, 0x01, 0xc0, 0xc0, "No Extended Time"},

};

STDDIPINFO(Lagunar)

static struct BurnInputInfo BoothillInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy3 + 6,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy3 + 5,     "p1 start"},
	{"P1 Up",               BIT_DIGITAL,	DrvJoy2 + 0,     "p1 up"},
	{"P1 Down",             BIT_DIGITAL,	DrvJoy2 + 1,     "p1 down"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoy2 + 2,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoy2 + 3,     "p1 right"},
	{"P1 Button 1",         BIT_DIGITAL,	DrvJoy2 + 7,     "p1 fire 1"},
	{"P1 Gun Up",           BIT_DIGITAL,	DrvJoyX + 0,     "p1 fire 2"},
	{"P1 Gun Down",         BIT_DIGITAL,	DrvJoyX + 1,     "p1 fire 3"},
	{"P2 Start",            BIT_DIGITAL,	DrvJoy3 + 7,     "p2 start"},
	{"P2 Up",               BIT_DIGITAL,	DrvJoy1 + 0,     "p2 up"},
	{"P2 Down",             BIT_DIGITAL,	DrvJoy1 + 1,     "p2 down"},
	{"P2 Left",             BIT_DIGITAL,	DrvJoy1 + 2,     "p2 left"},
	{"P2 Right",            BIT_DIGITAL,	DrvJoy1 + 3,     "p2 right"},
	{"P2 Button 1",         BIT_DIGITAL,	DrvJoy1 + 7,     "p2 fire 1"},
	{"P2 Gun Up",           BIT_DIGITAL,	DrvJoyX + 2,     "p2 fire 2"},
	{"P2 Gun Down",         BIT_DIGITAL,	DrvJoyX + 3,     "p2 fire 3"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};

STDINPUTINFO(Boothill)

static struct BurnDIPInfo BoothillDIPList[]=
{
	DEFAULTS(0x12, 0x8f, 0x8f, 0xe4, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Coinage"},
	{0x02, 0x01, 0x03, 0x02, "2 Coins per Player"},
	{0x02, 0x01, 0x03, 0x03, "2 Coins/1 or 2 Players"},
	{0x02, 0x01, 0x03, 0x00, "1 Coin per Player"},
	{0x02, 0x01, 0x03, 0x01, "1 Coin/1 or 2 Players"},

	{0   , 0xfe, 0   ,    4, "Game Time"},
	{0x02, 0x01, 0x0c, 0x00, "60 seconds"},
	{0x02, 0x01, 0x0c, 0x04, "70 seconds"},
	{0x02, 0x01, 0x0c, 0x08, "80 seconds"},
	{0x02, 0x01, 0x0c, 0x0c, "90 seconds"},

};

STDDIPINFO(Boothill)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo DesertguInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy2 + 6,     "p1 coin"},
	A("P1 Gun X",            BIT_ANALOG_REL, DrvAnalog + 0,   "mouse x-axis"),
	A("P1 Gun Y",            BIT_ANALOG_REL, DrvAnalog + 1,   "mouse y-axis"),
	{"P1 Button 1",         BIT_DIGITAL,	DrvJoy2 + 7,     "mouse button 1"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};
#undef A

STDINPUTINFO(Desertgu)

static struct BurnDIPInfo DesertguDIPList[]=
{
	DEFAULTS(0x05, 0x00, 0xd0, 0x00, 0x01, 0x00)

	{0   , 0xfe, 0   ,    4, "Language"},
	{0x01, 0x01, 0x0c, 0x00, "English"},
	{0x01, 0x01, 0x0c, 0x04, "German"},
	{0x01, 0x01, 0x0c, 0x08, "French"},
	{0x01, 0x01, 0x0c, 0x0c, "Danish"},

	{0   , 0xfe, 0   ,    4, "Extended Time At"},
	{0x01, 0x01, 0x30, 0x00, "5000"},
	{0x01, 0x01, 0x30, 0x10, "7000"},
	{0x01, 0x01, 0x30, 0x20, "9000"},
	{0x01, 0x01, 0x30, 0x30, "Test Mode"},

	{0   , 0xfe, 0   ,    4, "Coinage"},
	{0x02, 0x01, 0x03, 0x02, "2 Coins 1 Credits"},
	{0x02, 0x01, 0x03, 0x00, "1 Coin  1 Credits"},
	{0x02, 0x01, 0x03, 0x03, "2 Coins 3 Credits"},
	{0x02, 0x01, 0x03, 0x01, "1 Coin  2 Credits"},

	{0   , 0xfe, 0   ,    4, "Game Time"},
	{0x03, 0x01, 0x03, 0x00, "40 seconds + 30 extended"},
	{0x03, 0x01, 0x03, 0x01, "50 seconds + 30 extended"},
	{0x03, 0x01, 0x03, 0x02, "60 seconds + 30 extended"},
	{0x03, 0x01, 0x03, 0x03, "70 seconds + 30 extended"},

};

STDDIPINFO(Desertgu)

static struct BurnInputInfo DplayInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy3 + 7,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy1 + 7,     "p1 start"},
	{"P1 Hit",              BIT_DIGITAL,	DrvJoy1 + 0,     "p1 fire 1"},
	{"P1 Move Outfield Left", BIT_DIGITAL,	DrvJoy1 + 1,     "p1 fire 2"},
	{"P1 Move Outfield Right", BIT_DIGITAL,	DrvJoy1 + 2,     "p1 fire 3"},
	{"P1 Pitch Left",       BIT_DIGITAL,	DrvJoy1 + 3,     "p1 left"},
	{"P1 Pitch Right",      BIT_DIGITAL,	DrvJoy1 + 4,     "p1 right"},
	{"P1 Pitch Slow",       BIT_DIGITAL,	DrvJoy1 + 5,     "p1 up"},
	{"P1 Pitch Fast",       BIT_DIGITAL,	DrvJoy1 + 6,     "p1 down"},
	{"P2 Start",            BIT_DIGITAL,	DrvJoy2 + 7,     "p2 start"},
	{"P2 Hit",              BIT_DIGITAL,	DrvJoy2 + 0,     "p2 fire 1"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};

STDINPUTINFO(Dplay)

static struct BurnDIPInfo DplayDIPList[]=
{
	DEFAULTS(0x0c, 0xff, 0x81, 0xc0, 0x00, 0x00)

	{0   , 0xfe, 0   ,    6, "Coinage"},
	{0x02, 0x01, 0x07, 0x05, "2 Coins/1 Inning/1 Player, 4 Coins/1 Inning/2 Players, 8 Coins/3 Innings/2 Players"},
	{0x02, 0x01, 0x07, 0x04, "1 Coin/1 Inning/1 Player, 2 Coins/1 Inning/2 Players, 4 Coins/3 Innings/2 Players"},
	{0x02, 0x01, 0x07, 0x02, "2 Coins per Inning"},
	{0x02, 0x01, 0x07, 0x03, "2 Coins/1 Inning, 4 Coins/3 Innings"},
	{0x02, 0x01, 0x07, 0x00, "1 Coin per Inning"},
	{0x02, 0x01, 0x07, 0x01, "1 Coin/1 Inning, 2 Coins/3 Innings"},

	{0   , 0xfe, 0   ,    2, "Unused"},
	{0x02, 0x01, 0x08, 0x00, "Off"},
	{0x02, 0x01, 0x08, 0x08, "On"},

	{0   , 0xfe, 0   ,    2, "Unused"},
	{0x02, 0x01, 0x10, 0x00, "Off"},
	{0x02, 0x01, 0x10, 0x10, "On"},

	{0   , 0xfe, 0   ,    2, "Unused"},
	{0x02, 0x01, 0x20, 0x00, "Off"},
	{0x02, 0x01, 0x20, 0x20, "On"},

	{0   , 0xfe, 0   ,    2, "Service Mode"},
	{0x02, 0x01, 0x40, 0x40, "Off"},
	{0x02, 0x01, 0x40, 0x00, "On"},

};

STDDIPINFO(Dplay)

static struct BurnInputInfo EinningInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy3 + 7,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy1 + 7,     "p1 start"},
	{"P1 Hit",              BIT_DIGITAL,	DrvJoy1 + 0,     "p1 fire 1"},
	{"P1 Move Outfield Left", BIT_DIGITAL,	DrvJoy1 + 1,     "p1 fire 2"},
	{"P1 Move Outfield Right", BIT_DIGITAL,	DrvJoy1 + 2,     "p1 fire 3"},
	{"P1 Pitch Left",       BIT_DIGITAL,	DrvJoy1 + 3,     "p1 left"},
	{"P1 Pitch Right",      BIT_DIGITAL,	DrvJoy1 + 4,     "p1 right"},
	{"P1 Pitch Slow",       BIT_DIGITAL,	DrvJoy1 + 5,     "p1 up"},
	{"P1 Pitch Fast",       BIT_DIGITAL,	DrvJoy1 + 6,     "p1 down"},
	{"P2 Start",            BIT_DIGITAL,	DrvJoy2 + 7,     "p2 start"},
	{"P2 Hit",              BIT_DIGITAL,	DrvJoy2 + 0,     "p2 fire 1"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};

STDINPUTINFO(Einning)

static struct BurnDIPInfo EinningDIPList[]=
{
	DEFAULTS(0x0c, 0xff, 0x81, 0xc0, 0x00, 0x00)

	{0   , 0xfe, 0   ,    6, "Coinage"},
	{0x02, 0x01, 0x07, 0x05, "2 Coins/1 Inning/1 Player, 4 Coins/1 Inning/2 Players, 8 Coins/3 Innings/2 Players"},
	{0x02, 0x01, 0x07, 0x04, "1 Coin/1 Inning/1 Player, 2 Coins/1 Inning/2 Players, 4 Coins/3 Innings/2 Players"},
	{0x02, 0x01, 0x07, 0x02, "2 Coins per Inning"},
	{0x02, 0x01, 0x07, 0x03, "2 Coins/1 Inning, 4 Coins/3 Innings"},
	{0x02, 0x01, 0x07, 0x00, "1 Coin per Inning"},
	{0x02, 0x01, 0x07, 0x01, "1 Coin/1 Inning, 2 Coins/3 Innings"},

	{0   , 0xfe, 0   ,    2, "Wall Knock Out Behavior"},
	{0x02, 0x01, 0x08, 0x00, "Individually"},
	{0x02, 0x01, 0x08, 0x08, "In Pairs"},

	{0   , 0xfe, 0   ,    2, "Double Score when Special Lit"},
	{0x02, 0x01, 0x10, 0x00, "Home Run Only"},
	{0x02, 0x01, 0x10, 0x10, "Any Hit"},

	{0   , 0xfe, 0   ,    2, "Unused"},
	{0x02, 0x01, 0x20, 0x00, "Off"},
	{0x02, 0x01, 0x20, 0x20, "On"},

	{0   , 0xfe, 0   ,    2, "Service Mode"},
	{0x02, 0x01, 0x40, 0x40, "Off"},
	{0x02, 0x01, 0x40, 0x00, "On"},

};

STDDIPINFO(Einning)

static struct BurnInputInfo M4InputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy2 + 0,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy2 + 2,     "p1 start"},
	{"P1 Up",               BIT_DIGITAL,	DrvJoy2 + 1,     "p1 up"},
	{"P1 Down",             BIT_DIGITAL,	DrvJoy2 + 3,     "p1 down"},
	{"P1 Trigger",          BIT_DIGITAL,	DrvJoy2 + 4,     "p1 fire 1"},
	{"P1 Reload",           BIT_DIGITAL,	DrvJoy2 + 5,     "p1 fire 2"},
	{"P2 Start",            BIT_DIGITAL,	DrvJoy2 + 6,     "p2 start"},
	{"P2 Up",               BIT_DIGITAL,	DrvJoy1 + 1,     "p2 up"},
	{"P2 Down",             BIT_DIGITAL,	DrvJoy1 + 3,     "p2 down"},
	{"P2 Trigger",          BIT_DIGITAL,	DrvJoy1 + 4,     "p2 fire 1"},
	{"P2 Reload",           BIT_DIGITAL,	DrvJoy1 + 5,     "p2 fire 2"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};

STDINPUTINFO(M4)

static struct BurnDIPInfo M4DIPList[]=
{
	DEFAULTS(0x0c, 0xff, 0xff, 0x14, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Coinage"},
	{0x02, 0x01, 0x03, 0x02, "2 Coins per Player"},
	{0x02, 0x01, 0x03, 0x03, "2 Coins/1 or 2 Players"},
	{0x02, 0x01, 0x03, 0x00, "1 Coin per Player"},
	{0x02, 0x01, 0x03, 0x01, "1 Coin/1 or 2 Players"},

	{0   , 0xfe, 0   ,    4, "Game Time"},
	{0x02, 0x01, 0x0c, 0x00, "60 seconds"},
	{0x02, 0x01, 0x0c, 0x04, "70 seconds"},
	{0x02, 0x01, 0x0c, 0x08, "80 seconds"},
	{0x02, 0x01, 0x0c, 0x0c, "90 seconds"},

	{0   , 0xfe, 0   ,    2, "Service Mode"},
	{0x02, 0x01, 0x10, 0x10, "Off"},
	{0x02, 0x01, 0x10, 0x00, "On"},

	{0   , 0xfe, 0   ,    2, "Extended Play"},
	{0x02, 0x01, 0x20, 0x20, "Off"},
	{0x02, 0x01, 0x20, 0x00, "On"},

	{0   , 0xfe, 0   ,    4, "Extended Play At"},
	{0x02, 0x01, 0xc0, 0xc0, "70"},
	{0x02, 0x01, 0xc0, 0x40, "80"},
	{0x02, 0x01, 0xc0, 0x80, "100"},
	{0x02, 0x01, 0xc0, 0x00, "110"},

};

STDDIPINFO(M4)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo ClownsInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy2 + 6,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy2 + 5,     "p1 start"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoyX + 0,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoyX + 1,     "p1 right"},
	A("P1 Paddle",           BIT_ANALOG_REL, DrvAnalog + 0,   "p1 x-axis"),
	{"P2 Start",            BIT_DIGITAL,	DrvJoy2 + 4,     "p2 start"},
	{"P2 Left",             BIT_DIGITAL,	DrvJoyX + 2,     "p2 left"},
	{"P2 Right",            BIT_DIGITAL,	DrvJoyX + 3,     "p2 right"},
	A("P2 Paddle",           BIT_ANALOG_REL, DrvAnalog + 1,   "p2 x-axis"),
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};
#undef A

STDINPUTINFO(Clowns)

static struct BurnDIPInfo ClownsDIPList[]=
{
	DEFAULTS(0x0a, 0x00, 0xff, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    4, "Coinage"},
	{0x02, 0x01, 0x03, 0x03, "2 Coins 1 Credits"},
	{0x02, 0x01, 0x03, 0x02, "2 Coins 2 Credits"},
	{0x02, 0x01, 0x03, 0x00, "1 Coin  1 Credits"},
	{0x02, 0x01, 0x03, 0x01, "1 Coin  2 Credits"},

	{0   , 0xfe, 0   ,    4, "Bonus Game"},
	{0x02, 0x01, 0x0c, 0x00, "No Bonus"},
	{0x02, 0x01, 0x0c, 0x04, "9000"},
	{0x02, 0x01, 0x0c, 0x08, "11000"},
	{0x02, 0x01, 0x0c, 0x0c, "13000"},

	{0   , 0xfe, 0   ,    2, "Balloon Resets"},
	{0x02, 0x01, 0x10, 0x00, "Each Row"},
	{0x02, 0x01, 0x10, 0x10, "All Rows"},

	{0   , 0xfe, 0   ,    2, "Bonus Life"},
	{0x02, 0x01, 0x20, 0x00, "3000"},
	{0x02, 0x01, 0x20, 0x20, "4000"},

	{0   , 0xfe, 0   ,    2, "Lives"},
	{0x02, 0x01, 0x40, 0x00, "3"},
	{0x02, 0x01, 0x40, 0x40, "4"},

	{0   , 0xfe, 0   ,    2, "Service Mode"},
	{0x02, 0x01, 0x80, 0x00, "Off"},
	{0x02, 0x01, 0x80, 0x80, "On"},

	OVERLAY_DIP(0)
};

STDDIPINFO(Clowns)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo Clowns1InputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy2 + 6,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy2 + 5,     "p1 start"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoyX + 0,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoyX + 1,     "p1 right"},
	A("P1 Paddle",           BIT_ANALOG_REL, DrvAnalog + 0,   "p1 x-axis"),
	{"P2 Start",            BIT_DIGITAL,	DrvJoy2 + 4,     "p2 start"},
	{"P2 Left",             BIT_DIGITAL,	DrvJoyX + 2,     "p2 left"},
	{"P2 Right",            BIT_DIGITAL,	DrvJoyX + 3,     "p2 right"},
	A("P2 Paddle",           BIT_ANALOG_REL, DrvAnalog + 1,   "p2 x-axis"),
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};
#undef A

STDINPUTINFO(Clowns1)

static struct BurnDIPInfo Clowns1DIPList[]=
{
	DEFAULTS(0x0a, 0x00, 0xff, 0x04, 0x00, 0x01)

	{0   , 0xfe, 0   ,    4, "Coinage"},
	{0x02, 0x01, 0x03, 0x03, "2 Coins 1 Credits"},
	{0x02, 0x01, 0x03, 0x02, "2 Coins 2 Credits"},
	{0x02, 0x01, 0x03, 0x00, "1 Coin  1 Credits"},
	{0x02, 0x01, 0x03, 0x01, "1 Coin  2 Credits"},

	{0   , 0xfe, 0   ,    4, "Lives"},
	{0x02, 0x01, 0x0c, 0x00, "2"},
	{0x02, 0x01, 0x0c, 0x04, "3"},
	{0x02, 0x01, 0x0c, 0x08, "4"},
	{0x02, 0x01, 0x0c, 0x0c, "5"},

	{0   , 0xfe, 0   ,    2, "Balloon Resets"},
	{0x02, 0x01, 0x10, 0x00, "Each Row"},
	{0x02, 0x01, 0x10, 0x10, "All Rows"},

	{0   , 0xfe, 0   ,    2, "Bonus Life"},
	{0x02, 0x01, 0x20, 0x00, "3000"},
	{0x02, 0x01, 0x20, 0x20, "4000"},

	{0   , 0xfe, 0   ,    2, "Input Test"},
	{0x02, 0x01, 0x40, 0x00, "Off"},
	{0x02, 0x01, 0x40, 0x40, "On"},

	{0   , 0xfe, 0   ,    2, "Service Mode"},
	{0x02, 0x01, 0x80, 0x00, "Off"},
	{0x02, 0x01, 0x80, 0x80, "On"},

	OVERLAY_DIP(0)
};

STDDIPINFO(Clowns1)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo SpacwalkInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy2 + 6,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy2 + 5,     "p1 start"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoyX + 0,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoyX + 1,     "p1 right"},
	A("P1 Paddle",           BIT_ANALOG_REL, DrvAnalog + 0,   "p1 x-axis"),
	{"P2 Start",            BIT_DIGITAL,	DrvJoy2 + 4,     "p2 start"},
	{"P2 Left",             BIT_DIGITAL,	DrvJoyX + 2,     "p2 left"},
	{"P2 Right",            BIT_DIGITAL,	DrvJoyX + 3,     "p2 right"},
	A("P2 Paddle",           BIT_ANALOG_REL, DrvAnalog + 1,   "p2 x-axis"),
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};
#undef A

STDINPUTINFO(Spacwalk)

static struct BurnDIPInfo SpacwalkDIPList[]=
{
	DEFAULTS(0x0a, 0x00, 0xff, 0x01, 0x00, 0x01)

	{0   , 0xfe, 0   ,    4, "Game Time"},
	{0x02, 0x01, 0x03, 0x03, "40 seconds + 20 extended"},
	{0x02, 0x01, 0x03, 0x02, "50 seconds + 25 extended"},
	{0x02, 0x01, 0x03, 0x01, "60 seconds + 30 extended"},
	{0x02, 0x01, 0x03, 0x00, "70 seconds + 35 extended"},

	{0   , 0xfe, 0   ,    4, "Coinage"},
	{0x02, 0x01, 0x0c, 0x00, "1 Coin per Player"},
	{0x02, 0x01, 0x0c, 0x04, "1 Coin/1 or 2 Players"},
	{0x02, 0x01, 0x0c, 0x0c, "2 Coins per Player"},
	{0x02, 0x01, 0x0c, 0x08, "2 Coins/1 or 2 Players"},

	{0   , 0xfe, 0   ,    4, "Extended Time At"},
	{0x02, 0x01, 0x30, 0x00, "None"},
	{0x02, 0x01, 0x30, 0x10, "5000"},
	{0x02, 0x01, 0x30, 0x20, "6000"},
	{0x02, 0x01, 0x30, 0x30, "7000"},

	{0   , 0xfe, 0   ,    2, "Springboard Alignment"},
	{0x02, 0x01, 0x40, 0x00, "Off"},
	{0x02, 0x01, 0x40, 0x40, "On"},

	{0   , 0xfe, 0   ,    2, "Service Mode"},
	{0x02, 0x01, 0x80, 0x00, "Off"},
	{0x02, 0x01, 0x80, 0x80, "On"},

	OVERLAY_DIP(0)
};

STDDIPINFO(Spacwalk)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo ShuffleInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy2 + 0,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy2 + 1,     "p1 start"},
	{"P2 Start",            BIT_DIGITAL,	DrvJoy2 + 2,     "p2 start"},
	{"Game Select",         BIT_DIGITAL,	DrvJoy2 + 3,     "p1 fire 1"},
	A("P1 Trackball X",      BIT_ANALOG_REL, DrvAnalog + 0,   "p1 x-axis"),
	A("P1 Trackball Y",      BIT_ANALOG_REL, DrvAnalog + 1,   "p1 y-axis"),
	{"P1 Up",               BIT_DIGITAL,	DrvJoyX + 2,     "p1 up"},
	{"P1 Down",             BIT_DIGITAL,	DrvJoyX + 3,     "p1 down"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoyX + 0,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoyX + 1,     "p1 right"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};
#undef A

STDINPUTINFO(Shuffle)

static struct BurnDIPInfo ShuffleDIPList[]=
{
	DEFAULTS(0x0b, 0xd4, 0xff, 0x00, 0x00, 0x00)

	{0   , 0xfe, 0   ,    3, "Language"},
	{0x00, 0x01, 0x03, 0x00, "English"},
	{0x00, 0x01, 0x03, 0x01, "French"},
	{0x00, 0x01, 0x03, 0x02, "German"},

	{0   , 0xfe, 0   ,    4, "Points to Win"},
	{0x00, 0x01, 0x0c, 0x00, "Game 1 = 25, Game 2 = 11"},
	{0x00, 0x01, 0x0c, 0x04, "Game 1 = 35, Game 2 = 15"},
	{0x00, 0x01, 0x0c, 0x08, "Game 1 = 40, Game 2 = 18"},
	{0x00, 0x01, 0x0c, 0x0c, "Game 1 = 50, Game 2 = 21"},

	{0   , 0xfe, 0   ,    4, "Coinage"},
	{0x00, 0x01, 0x30, 0x30, "2 Coins per Player"},
	{0x00, 0x01, 0x30, 0x20, "2 Coins/1 or 2 Players"},
	{0x00, 0x01, 0x30, 0x10, "1 Coin per Player"},
	{0x00, 0x01, 0x30, 0x00, "1 Coin/1 or 2 Players"},

	{0   , 0xfe, 0   ,    2, "Time Limit"},
	{0x00, 0x01, 0x40, 0x00, "Off"},
	{0x00, 0x01, 0x40, 0x40, "On"},

	{0   , 0xfe, 0   ,    2, "Service Mode"},
	{0x00, 0x01, 0x80, 0x80, "Off"},
	{0x00, 0x01, 0x80, 0x00, "On"},

};

STDDIPINFO(Shuffle)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo BowlerInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy2 + 0,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy2 + 2,     "p1 start"},
	{"Hook/Straight",       BIT_DIGITAL,	DrvJoy2 + 1,     "p1 fire 1"},
	{"Game Select",         BIT_DIGITAL,	DrvJoy2 + 3,     "p1 fire 2"},
	A("P1 Trackball X",      BIT_ANALOG_REL, DrvAnalog + 0,   "p1 x-axis"),
	A("P1 Trackball Y",      BIT_ANALOG_REL, DrvAnalog + 1,   "p1 y-axis"),
	{"P1 Up",               BIT_DIGITAL,	DrvJoyX + 2,     "p1 up"},
	{"P1 Down",             BIT_DIGITAL,	DrvJoyX + 3,     "p1 down"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoyX + 0,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoyX + 1,     "p1 right"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};
#undef A

STDINPUTINFO(Bowler)

static struct BurnDIPInfo BowlerDIPList[]=
{
	DEFAULTS(0x0b, 0x2c, 0x00, 0x00, 0x00, 0x00)

	{0   , 0xfe, 0   ,    3, "Language"},
	{0x00, 0x01, 0x03, 0x00, "English"},
	{0x00, 0x01, 0x03, 0x01, "French"},
	{0x00, 0x01, 0x03, 0x02, "German"},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"},
	{0x00, 0x01, 0x04, 0x00, "Off"},
	{0x00, 0x01, 0x04, 0x04, "On"},

	{0   , 0xfe, 0   ,    2, "Game Time"},
	{0x00, 0x01, 0x08, 0x00, "No Limit"},
	{0x00, 0x01, 0x08, 0x08, "5 Minutes"},

	{0   , 0xfe, 0   ,    2, "Coinage"},
	{0x00, 0x01, 0x10, 0x10, "2 Coins 1 Credits"},
	{0x00, 0x01, 0x10, 0x00, "1 Coin  1 Credits"},

	{0   , 0xfe, 0   ,    2, "Difficulty"},
	{0x00, 0x01, 0x20, 0x20, "Easy"},
	{0x00, 0x01, 0x20, 0x00, "Hard"},

	{0   , 0xfe, 0   ,    2, "Cabinet"},
	{0x00, 0x01, 0x40, 0x00, "Upright"},
	{0x00, 0x01, 0x40, 0x40, "Cocktail (not functional)"},

	{0   , 0xfe, 0   ,    2, "Service Mode"},
	{0x00, 0x01, 0x80, 0x00, "Off"},
	{0x00, 0x01, 0x80, 0x80, "On"},

};

STDDIPINFO(Bowler)

static struct BurnInputInfo DogpatchInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy1 + 0,     "p1 coin"},
	{"P1 Start",            BIT_DIGITAL,	DrvJoy1 + 1,     "p1 start"},
	{"P1 Button 1",         BIT_DIGITAL,	DrvJoy2 + 7,     "p1 fire 1"},
	{"P1 Gun Up",           BIT_DIGITAL,	DrvJoyX + 0,     "p1 fire 2"},
	{"P1 Gun Down",         BIT_DIGITAL,	DrvJoyX + 1,     "p1 fire 3"},
	{"P2 Start",            BIT_DIGITAL,	DrvJoy1 + 2,     "p2 start"},
	{"P2 Button 1",         BIT_DIGITAL,	DrvJoy1 + 7,     "p2 fire 1"},
	{"P2 Gun Up",           BIT_DIGITAL,	DrvJoyX + 2,     "p2 fire 2"},
	{"P2 Gun Down",         BIT_DIGITAL,	DrvJoyX + 3,     "p2 fire 3"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};

STDINPUTINFO(Dogpatch)

static struct BurnDIPInfo DogpatchDIPList[]=
{
	DEFAULTS(0x0a, 0x8f, 0x8f, 0x72, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Number of Cans"},
	{0x02, 0x01, 0x03, 0x03, "10"},
	{0x02, 0x01, 0x03, 0x02, "15"},
	{0x02, 0x01, 0x03, 0x01, "20"},
	{0x02, 0x01, 0x03, 0x00, "25"},

	{0   , 0xfe, 0   ,    4, "Coinage"},
	{0x02, 0x01, 0x0c, 0x08, "2 Coins per Player"},
	{0x02, 0x01, 0x0c, 0x0c, "2 Coins/1 or 2 Players"},
	{0x02, 0x01, 0x0c, 0x00, "1 Coin per Player"},
	{0x02, 0x01, 0x0c, 0x04, "1 Coin/1 or 2 Players"},

	{0   , 0xfe, 0   ,    2, "Extended Time Reward"},
	{0x02, 0x01, 0x10, 0x10, "3 extra cans"},
	{0x02, 0x01, 0x10, 0x00, "5 extra cans"},

	{0   , 0xfe, 0   ,    2, "Service Mode"},
	{0x02, 0x01, 0x20, 0x20, "Off"},
	{0x02, 0x01, 0x20, 0x00, "On"},

	{0   , 0xfe, 0   ,    4, "Extended Time At"},
	{0x02, 0x01, 0xc0, 0xc0, "150"},
	{0x02, 0x01, 0xc0, 0x80, "175"},
	{0x02, 0x01, 0xc0, 0x40, "225"},
	{0x02, 0x01, 0xc0, 0x00, "275"},

};

STDDIPINFO(Dogpatch)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo SpcenctrInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy1 + 7,     "p1 coin"},
	{"P1 Up",               BIT_DIGITAL,	DrvJoyX + 2,     "p1 up"},
	{"P1 Down",             BIT_DIGITAL,	DrvJoyX + 3,     "p1 down"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoyX + 0,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoyX + 1,     "p1 right"},
	A("P1 X Axis",           BIT_ANALOG_REL, DrvAnalog + 0,   "p1 x-axis"),
	A("P1 Y Axis",           BIT_ANALOG_REL, DrvAnalog + 1,   "p1 y-axis"),
	{"P1 Button 1",         BIT_DIGITAL,	DrvJoy1 + 6,     "p1 fire 1"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};
#undef A

STDINPUTINFO(Spcenctr)

static struct BurnDIPInfo SpcenctrDIPList[]=
{
	DEFAULTS(0x09, 0xc0, 0x00, 0x42, 0x00, 0x00)

	{0   , 0xfe, 0   ,    4, "Bonus Life"},
	{0x02, 0x01, 0x03, 0x00, "2000 4000 8000"},
	{0x02, 0x01, 0x03, 0x01, "3000 6000 12000"},
	{0x02, 0x01, 0x03, 0x02, "4000 8000 16000"},
	{0x02, 0x01, 0x03, 0x03, "5000 10000 20000"},

	{0   , 0xfe, 0   ,    4, "Coinage"},
	{0x02, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"},
	{0x02, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"},
	{0x02, 0x01, 0x0c, 0x0c, "2 Coins 3 Credits"},
	{0x02, 0x01, 0x0c, 0x08, "1 Coin  3 Credits"},

	{0   , 0xfe, 0   ,    4, "Bonus/Test Mode"},
	{0x02, 0x01, 0x30, 0x00, "Bonus On"},
	{0x02, 0x01, 0x30, 0x30, "Bonus Off"},
	{0x02, 0x01, 0x30, 0x20, "Cross Hatch"},
	{0x02, 0x01, 0x30, 0x10, "Test Mode"},

	{0   , 0xfe, 0   ,    4, "Time"},
	{0x02, 0x01, 0xc0, 0x00, "45"},
	{0x02, 0x01, 0xc0, 0x40, "60"},
	{0x02, 0x01, 0xc0, 0x80, "75"},
	{0x02, 0x01, 0xc0, 0xc0, "90"},

};

STDDIPINFO(Spcenctr)

static struct BurnInputInfo Phantom2InputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy1 + 5,     "p1 coin"},
	{"P1 Up",               BIT_DIGITAL,	DrvJoy1 + 0,     "p1 up"},
	{"P1 Down",             BIT_DIGITAL,	DrvJoy1 + 1,     "p1 down"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoy1 + 2,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoy1 + 3,     "p1 right"},
	{"P1 Button 1",         BIT_DIGITAL,	DrvJoy1 + 4,     "p1 fire 1"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};

STDINPUTINFO(Phantom2)

static struct BurnDIPInfo Phantom2DIPList[]=
{
	DEFAULTS(0x07, 0xff, 0x26, 0x00, 0x00, 0x01)

	{0   , 0xfe, 0   ,    2, "Coinage"},
	{0x01, 0x01, 0x01, 0x01, "2 Coins 1 Credits"},
	{0x01, 0x01, 0x01, 0x00, "1 Coin  1 Credits"},

	{0   , 0xfe, 0   ,    4, "Game Time"},
	{0x01, 0x01, 0x06, 0x00, "45 seconds + 20 extended (at 20 points)"},
	{0x01, 0x01, 0x06, 0x02, "60 seconds + 25 extended (at 25 points)"},
	{0x01, 0x01, 0x06, 0x04, "75 seconds + 30 extended (at 30 points)"},
	{0x01, 0x01, 0x06, 0x06, "90 seconds + 35 extended (at 35 points)"},

	{0   , 0xfe, 0   ,    2, "Unused"},
	{0x01, 0x01, 0x08, 0x00, "Off"},
	{0x01, 0x01, 0x08, 0x08, "On"},

	{0   , 0xfe, 0   ,    2, "Unused"},
	{0x01, 0x01, 0x10, 0x00, "Off"},
	{0x01, 0x01, 0x10, 0x10, "On"},

	{0   , 0xfe, 0   ,    2, "Service Mode"},
	{0x01, 0x01, 0x20, 0x20, "Off"},
	{0x01, 0x01, 0x20, 0x00, "On"},

	{0   , 0xfe, 0   ,    2, "Unused"},
	{0x01, 0x01, 0x40, 0x00, "Off"},
	{0x01, 0x01, 0x40, 0x40, "On"},

	{0   , 0xfe, 0   ,    2, "Unused"},
	{0x01, 0x01, 0x80, 0x00, "Off"},
	{0x01, 0x01, 0x80, 0x80, "On"},

	OVERLAY_DIP(0)
};

STDDIPINFO(Phantom2)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo BlueshrkInputList[] = {
	{"P1 Coin",             BIT_DIGITAL,	DrvJoy2 + 1,     "p1 coin"},
	{"P1 Left",             BIT_DIGITAL,	DrvJoyX + 0,     "p1 left"},
	{"P1 Right",            BIT_DIGITAL,	DrvJoyX + 1,     "p1 right"},
	A("P1 Spear",            BIT_ANALOG_REL, DrvAnalog + 0,   "p1 x-axis"),
	{"P1 Button 1",         BIT_DIGITAL,	DrvJoy2 + 0,     "p1 fire 1"},
	{"Tilt",                BIT_DIGITAL,	DrvJoy2 + 3,     "tilt"},
	{"Reset",               BIT_DIGITAL,	&DrvReset,       "reset"},
	DIP_ENTRIES
};
#undef A

STDINPUTINFO(Blueshrk)

static struct BurnDIPInfo BlueshrkDIPList[]=
{
	DEFAULTS(0x07, 0x00, 0xdf, 0x00, 0x00, 0x00)

	{0   , 0xfe, 0   ,    2, "Unused"},
	{0x01, 0x01, 0x04, 0x04, "Off"},
	{0x01, 0x01, 0x04, 0x00, "On"},

	{0   , 0xfe, 0   ,    2, "Unused"},
	{0x01, 0x01, 0x10, 0x10, "Off"},
	{0x01, 0x01, 0x10, 0x00, "On"},

	{0   , 0xfe, 0   ,    4, "Replay"},
	{0x01, 0x01, 0x60, 0x20, "14000"},
	{0x01, 0x01, 0x60, 0x40, "18000"},
	{0x01, 0x01, 0x60, 0x60, "22000"},
	{0x01, 0x01, 0x60, 0x00, "None"},

	{0   , 0xfe, 0   ,    2, "Service Mode"},
	{0x01, 0x01, 0x80, 0x80, "Off"},
	{0x01, 0x01, 0x80, 0x00, "On"},

};

STDDIPINFO(Blueshrk)

// Midway: Amazing Maze, Guided Missile, Checkmate
static void __fastcall maze_out(UINT16 port, UINT8 data)
{
	if (port & 0x02) BurnWatchdogWrite();
}

static UINT8 __fastcall maze_in(UINT16 port)
{
	return DrvInputs[port & 1];
}

static void __fastcall checkmat_out(UINT16 port, UINT8 data)
{
	if (port & 0x02) BurnWatchdogWrite();
}

static UINT8 __fastcall checkmat_in(UINT16 port)
{
	return DrvInputs[port & 3];
}

static void __fastcall gmissile_out(UINT16 port, UINT8 data)
{
	switch (port & 7) {
		case 1: shift_count_w(data); shift_reverse = (data >> 3) & 1; return;
		case 2: shift_data_w(data); return;
		case 3: gmissile_sound_w(data); return;
		case 4: BurnWatchdogWrite(); return;
	}
}

static UINT8 __fastcall gmissile_in(UINT16 port)
{
	if ((port & 3) == 3) return shift_reverse ? shift_result_rev_r() : shift_result_r();
	return DrvInputs[port & 3];
}

static UINT8 in_jspecter(INT32 port)
{
	UINT8 ret = in_sicv(port);
	if (port == 1) ret |= (DrvInP1 & 4) << 5;	// 2nd "right" input
	return ret;
}

// Machine configurations
#define MW_CLOCK	1996800

//                                      video          sound          pal  out              in               map  clock     irq  watchdog
static const mw_config cfg_invaders   = { VID_MW,       SND_INVADERS,  2, invaders_out,    invaders_in,     0, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_cosmicbat  = { VID_MW,       SND_INVADERS,  2, invaders_out,    invaders_in,     0, 2000000,   0, 255, NULL };
static const mw_config cfg_invasion   = { VID_MW,       SND_INVADERS,  2, invasion_out,    invasion_in,     0, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_astropal   = { VID_MW,       SND_INVADERS,  2, astropal_out,    astropal_in,     0, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_vortex     = { VID_VORTEX,   SND_INVADERS,  1, vortex_out,      vortex_in,       0, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_spacecom   = { VID_SPACECOM, SND_INVADERS,  2, spacecom_out,    spacecom_in,     4, 2000000,   0, 0, NULL };
static const mw_config cfg_invad2ct   = { VID_MW,       SND_INVAD2CT,  2, invad2ct_out,    invad2ct_in,     0, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_claybust   = { VID_MW,       SND_NONE,      2, claybust_out,    claybust_in,     0, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_yosakdon   = { VID_MW,       SND_SAMPLES,   2, yosakdon_out,    yosakdon_in,     4, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_invadpt2   = { VID_INVADPT2, SND_SAMPLES,   0, invadpt2_out,    invadpt2_in,     0, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_spacerng   = { VID_INVADPT2, SND_SAMPLES,   0, spacerng_out,    invadpt2_in,     0, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_spcewars   = { VID_MW,       SND_SPCEWARS,  2, spcewars_out,    invadpt2_in,     0, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_spcewarla  = { VID_INVADPT2, SND_SPCEWARS,  0, spcewarla_out,   spcewarla_in,    0, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_starw1     = { VID_INVADPT2, SND_SAMPLES,   0, starw1_out,      starw1_in,       0, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_steelwkr   = { VID_INVADPT2, SND_SAMPLES,   0, steelwkr_out,    steelwkr_in,     0, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_cosmo      = { VID_COSMO,    SND_SAMPLES,   4, cosmo_out,       cosmo_in,        3, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_lrescue    = { VID_INVADPT2, SND_LRESCUE,   0, lrescue_out,     invadpt2_in,     0, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_escmars    = { VID_MW,       SND_LRESCUE,   2, lrescue_out,     invadpt2_in,     4, 1800000,   0, 0, NULL };
static const mw_config cfg_schaser    = { VID_SCHASER,  SND_SCHASER,   0, schaser_out,     schaser_in,      1, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_crashrd    = { VID_SCHASER,  SND_SCHASER,   0, crashrd_out,     schaser_in,      1, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_schasercv  = { VID_SCHASERCV,SND_SCHASERCV, 0, schasercv_out,   schasercv_in,    1, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_lupin3     = { VID_INDIANBT, SND_LUPIN3,    1, lupin3_out,      schaser_in,      0, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_lupin3a    = { VID_LUPIN3,   SND_LUPIN3,    0, lupin3_out,      schaser_in,      1, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_polaris    = { VID_POLARIS,  SND_POLARIS,   0, polaris_out,     polaris_in,      1, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_ozmawars   = { VID_INVADPT2, SND_OZMAWARS,  0, ozmawars_out,    invadpt2_in,     0, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_ballbomb   = { VID_BALLBOMB, SND_BALLBOMB,  0, ballbomb_out,    invadpt2_in,     0, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_indianbt   = { VID_INDIANBT, SND_INDIANBT,  1, indianbt_out,    indianbt_in,     0, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_indianbtbr = { VID_INDIANBT, SND_SAMPLES,   1, indianbtbr_out,  indianbtbr_in,   1, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_shuttlei   = { VID_SHUTTLEI, SND_SAMPLES,   2, shuttlei_out,    shuttlei_in,     5, 2000000,   0, 0, NULL };
static const mw_config cfg_darthvdr   = { VID_MW,       SND_SAMPLES,   2, darthvdr_out,    darthvdr_in,     6, MW_CLOCK,  1, 0, NULL };
static const mw_config cfg_rollingc   = { VID_ROLLINGC, SND_SAMPLES,   3, rollingc_out,    rollingc_in,     2, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_mraker     = { VID_ROLLINGC, SND_NONE,      3, mraker_out,      mraker_in,       2, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_invrvnge   = { VID_INVADPT2, SND_INVRVNGE,  0, invrvnge_out,    invadpt2_in,     0, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_attackfc   = { VID_MW,       SND_NONE,      2, attackfc_out,    attackfc_in,     0, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_attackfcu  = { VID_MW,       SND_NONE,      2, attackfc_out,    attackfcu_in,    0, MW_CLOCK,  0, 0, NULL };
static const mw_config cfg_cane       = { VID_MW,       SND_NONE,      2, cane_out,        cane_in,         4, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_orbite     = { VID_SFLUSH,   SND_NONE,      1, orbite_out,      orbite_in,       1, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_maze       = { VID_MW,       SND_NONE,      2, maze_out,        maze_in,         0, MW_CLOCK,  0, 180, NULL };
static const mw_config cfg_gmissile   = { VID_MW,       SND_SAMPLESONLY,     2, gmissile_out,    gmissile_in,     0, MW_CLOCK,  0, 255, NULL };
static const mw_config cfg_checkmat   = { VID_MW,       SND_NONE,      2, checkmat_out,    checkmat_in,     0, MW_CLOCK,  0, 180, NULL };

// Decryption / rom patches
static void invrvnge_decrypt()
{
	for (INT32 i = 0; i < 0x800; i++) {
		DrvSndROM[0xc000 + i] = BITSWAP08(DrvSndROM[0xc000 + i], 7, 6, 5, 3, 4, 2, 1, 0);
		DrvSndROM[0xe000 + i] = BITSWAP08(DrvSndROM[0xe000 + i], 7, 6, 5, 3, 4, 2, 1, 0);
	}
}

static void vortex_decrypt()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);

	for (INT32 x = 0; x < 0x10000; x++)
	{
		INT32 addr = x;
		switch (x & 0xe000) {
			case 0x0000: case 0x2000: case 0x4000: addr ^= 0x0209; break;
			default: addr ^= 0x0208; break;
		}
		tmp[addr] = DrvI8080ROM[x];
	}

	memcpy (DrvI8080ROM, tmp, 0x10000);
	BurnFree (tmp);
}

static void attackfc_decrypt()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);

	for (INT32 i = 0; i < 0x10000; i++)
		tmp[(i & ~0x300) | ((i & 0x100) << 1) | ((i & 0x200) >> 1)] = DrvI8080ROM[i];

	memcpy (DrvI8080ROM, tmp, 0x10000);
	BurnFree (tmp);
}

static void spacecom_patch()
{
	DrvI8080ROM[0x10] = 0xf5; // bad byte, should be push a at rst 10h
}

// sample sets
static struct BurnSampleInfo LrescueSampleDesc[] = {
	{ "alienexplosion",			SAMPLE_NOLOOP },
	{ "rescueshipexplosion",	SAMPLE_NOLOOP },
	{ "beamgun",				SAMPLE_NOLOOP },
	{ "thrust",					SAMPLE_NOLOOP },
	{ "bonus2",					SAMPLE_NOLOOP },
	{ "bonus3",					SAMPLE_NOLOOP },
	{ "shootingstar",			SAMPLE_NOLOOP },
	{ "stepl",					SAMPLE_NOLOOP },
	{ "steph",					SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Lrescue)
STD_SAMPLE_FN(Lrescue)

static struct BurnSampleInfo Lupin3SampleDesc[] = {
	{ "cap",	SAMPLE_NOLOOP },	// go to jail
	{ "bark",	SAMPLE_NOLOOP },	// dog barking
	{ "walk1",	SAMPLE_NOLOOP },	// walk, get money
	{ "walk2",	SAMPLE_NOLOOP },	// walk, get money
	{ "warp",	SAMPLE_NOLOOP },	// translocate, deposit money
	{ "extend",	SAMPLE_NOLOOP },	// bonus man
	{ "kick",	SAMPLE_NOLOOP },	// lands on top of building, wife kicks man
	{ "", 0 }
};

STD_SAMPLE_PICK(Lupin3)
STD_SAMPLE_FN(Lupin3)

static struct BurnSampleInfo OzmawarsSampleDesc[] = {
	{ "ow0",  SAMPLE_NOLOOP },	// shoot
	{ "ow1",  SAMPLE_NOLOOP },	// die
	{ "ow2",  SAMPLE_NOLOOP },	// docking 1
	{ "ow3",  SAMPLE_NOLOOP },	// docking 2
	{ "ow4",  SAMPLE_NOLOOP },	// docking 3
	{ "ow5",  SAMPLE_NOLOOP },	// docking 4
	{ "ow6",  SAMPLE_NOLOOP },	// refuel
	{ "ow7",  SAMPLE_NOLOOP },	// comet
	{ "ow8",  SAMPLE_NOLOOP },	// hit invader
	{ "ow9",  SAMPLE_NOLOOP },	// hit meteor
	{ "ow10", SAMPLE_NOLOOP },	// stage with invaders
	{ "ow11", SAMPLE_NOLOOP },	// globe-thing after the comet
	{ "ow12", SAMPLE_NOLOOP },	// not sure
	{ "ow13", SAMPLE_NOLOOP },	// stage with wheels
	{ "ow14", SAMPLE_NOLOOP },	// meteor stage
	{ "", 0 }
};

STD_SAMPLE_PICK(Ozmawars)
STD_SAMPLE_FN(Ozmawars)

static INT32 MwInit(const mw_config *config, const ovl_layout *ovl, UINT8 (*inputs)(INT32), INT32 flip, void (*decrypt)())
{
	read_inputs = inputs;
	flip_mode = flip;
	return DrvInit(config, ovl, decrypt);
}

// Midway mw8080bw games: configurations and samples
//                                      video          sound            pal  out              in               map  clock     irq  watchdog  frame
static const mw_config cfg_seawolf    = { VID_MW,       SND_SAMPLESONLY, 2, seawolf_out,     seawolf_in,      0, MW_CLOCK,  0, 0,   seawolf_frame  };
static const mw_config cfg_gunfight   = { VID_MW,       SND_GUNFIGHT,    2, gunfight_out,    gunfight_in,     0, MW_CLOCK,  0, 0,   gunfight_frame };
static const mw_config cfg_tornbase   = { VID_MW,       SND_NONE,        2, tornbase_out,    tornbase_in,     0, MW_CLOCK,  0, 0,   NULL           };
static const mw_config cfg_zzzap      = { VID_MW,       SND_ZZZAP,       2, zzzap_out,       zzzap_in,        0, MW_CLOCK,  0, 66,  zzzap_frame    };
static const mw_config cfg_lagunar    = { VID_MW,       SND_LAGUNAR,     2, zzzap_out,       zzzap_in,        0, MW_CLOCK,  0, 66,  zzzap_frame    };
static const mw_config cfg_boothill   = { VID_MW,       SND_BOOTHILL,    2, boothill_out,    boothill_in,     0, MW_CLOCK,  0, 178, boothill_frame };
static const mw_config cfg_desertgu   = { VID_MW,       SND_MWMUSIC,     2, desertgu_out,    desertgu_in,     0, MW_CLOCK,  0, 255, desertgu_frame };
static const mw_config cfg_dplay      = { VID_MW,       SND_MWMUSIC,     2, dplay_out,       dplay_in,        0, MW_CLOCK,  0, 255, NULL           };
static const mw_config cfg_m4         = { VID_MW,       SND_SAMPLESONLY, 2, m4_out,          m4_in,           0, MW_CLOCK,  0, 255, NULL           };
static const mw_config cfg_clowns     = { VID_MW,       SND_MWMUSIC,     2, clowns_out,      clowns_in,       0, MW_CLOCK,  0, 255, clowns_frame   };
static const mw_config cfg_spacwalk   = { VID_MW,       SND_MWMUSIC,     2, clowns_out,      spacwalk_in,     0, MW_CLOCK,  0, 255, clowns_frame   };
static const mw_config cfg_shuffle    = { VID_MW,       SND_NONE,        2, shuffle_out,     shuffle_in,      0, MW_CLOCK,  0, 255, trackball_frame };
static const mw_config cfg_bowler     = { VID_MW,       SND_NONE,        2, bowler_out,      bowler_in,       0, MW_CLOCK,  0, 255, trackball_frame };
static const mw_config cfg_dogpatch   = { VID_MW,       SND_MWMUSIC,     2, dogpatch_out,    dogpatch_in,     0, MW_CLOCK,  0, 255, dogpatch_frame };
static const mw_config cfg_spcenctr   = { VID_SPCENCTR, SND_NONE,        6, spcenctr_out,    spcenctr_in,     0, MW_CLOCK,  0, 255, spcenctr_frame };
static const mw_config cfg_phantom2   = { VID_PHANTOM2, SND_SAMPLESONLY, 6, phantom2_out,    phantom2_in,     0, MW_CLOCK,  0, 255, NULL           };
static const mw_config cfg_blueshrk   = { VID_MW,       SND_NONE,        2, blueshrk_out,    blueshrk_in,     0, MW_CLOCK,  0, 255, blueshrk_frame };

static struct BurnSampleInfo SeawolfSampleDesc[] = {
	{ "shiphit",	SAMPLE_NOLOOP },
	{ "torpedo",	SAMPLE_NOLOOP },
	{ "dive",		SAMPLE_NOLOOP },
	{ "sonar",		SAMPLE_NOLOOP },
	{ "minehit",	SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Seawolf)
STD_SAMPLE_FN(Seawolf)

static struct BurnSampleInfo GmissileSampleDesc[] = {
	{ "1",			SAMPLE_NOLOOP },	// missile
	{ "2",			SAMPLE_NOLOOP },	// explosion
	{ "", 0 }
};

STD_SAMPLE_PICK(Gmissile)
STD_SAMPLE_FN(Gmissile)

static struct BurnSampleInfo M4SampleDesc[] = {
	{ "1",			SAMPLE_NOLOOP },	// shot
	{ "2",			SAMPLE_NOLOOP },	// explosion
	{ "", 0 }
};

STD_SAMPLE_PICK(M4)
STD_SAMPLE_FN(M4)

static struct BurnSampleInfo ClownsSampleDesc[] = {
	{ "miss",		SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Clowns)
STD_SAMPLE_FN(Clowns)

static struct BurnSampleInfo Phantom2SampleDesc[] = {
	{ "1",			SAMPLE_NOLOOP },	// shot
	{ "2",			SAMPLE_NOLOOP },	// explosion
	{ "", 0 }
};

STD_SAMPLE_PICK(Phantom2)
STD_SAMPLE_FN(Phantom2)

// Init wrappers
static INT32 Init0() { return MwInit(&cfg_seawolf, &seawolf_overlay, in_sicv, 0, NULL); }
static INT32 Init1() { return MwInit(&cfg_gunfight, &gunfight_overlay, in_sicv, 0, NULL); }
static INT32 Init2() { return MwInit(&cfg_tornbase, NULL, in_sicv, 0, NULL); }
static INT32 Init3() { return MwInit(&cfg_zzzap, &zzzap_overlay, in_sicv, 0, NULL); }
static INT32 Init4() { return MwInit(&cfg_maze, &maze_overlay, in_sicv, 0, NULL); }
static INT32 Init5() { return MwInit(&cfg_boothill, NULL, in_sicv, 0, NULL); }
static INT32 Init6() { return MwInit(&cfg_checkmat, NULL, in_sicv, 0, NULL); }
static INT32 Init7() { return MwInit(&cfg_desertgu, NULL, in_sicv, 0, NULL); }
static INT32 Init8() { return MwInit(&cfg_dplay, NULL, in_sicv, 0, NULL); }
static INT32 Init9() { return MwInit(&cfg_lagunar, NULL, in_sicv, 0, NULL); }
static INT32 Init10() { return MwInit(&cfg_gmissile, NULL, in_sicv, 0, NULL); }
static INT32 Init11() { return MwInit(&cfg_m4, NULL, in_sicv, 0, NULL); }
static INT32 Init12() { return MwInit(&cfg_clowns, &clowns_overlay, in_sicv, 0, NULL); }
static INT32 Init13() { return MwInit(&cfg_spacwalk, &spacwalk_overlay, in_sicv, 0, NULL); }
static INT32 Init14() { return MwInit(&cfg_shuffle, NULL, in_sicv, 0, NULL); }
static INT32 Init15() { return MwInit(&cfg_dogpatch, NULL, in_sicv, 0, NULL); }
static INT32 Init16() { return MwInit(&cfg_spcenctr, NULL, in_sicv, 0, NULL); }
static INT32 Init17() { return MwInit(&cfg_phantom2, &phantom2_overlay, in_sicv, 0, NULL); }
static INT32 Init18() { return MwInit(&cfg_bowler, NULL, in_sicv, 0, NULL); }
static INT32 Init19() { return MwInit(&cfg_invaders, &invaders_overlay, in_invaders, 0, NULL); }
static INT32 Init20() { return MwInit(&cfg_blueshrk, NULL, in_sicv, 0, NULL); }
static INT32 Init21() { return MwInit(&cfg_invad2ct, &invad2ct_overlay, in_sicv, 0, NULL); }
static INT32 Init22() { return MwInit(&cfg_invaders, &invad2ct_overlay, in_invaders, 0, NULL); }
static INT32 Init23() { return MwInit(&cfg_invaders, &invaders_overlay, in_sicv, 0, NULL); }
static INT32 Init24() { return MwInit(&cfg_invadpt2, NULL, in_sicv, 0, NULL); }
static INT32 Init25() { return MwInit(&cfg_spcewars, &invaders_overlay, in_sicv, 0, NULL); }
static INT32 Init26() { return MwInit(&cfg_spcewarla, NULL, in_sicv, 0, NULL); }
static INT32 Init27() { return MwInit(&cfg_invaders, &cosmicm_overlay, in_sicv, 1, NULL); }
static INT32 Init28() { return MwInit(&cfg_spacecom, &spacecom_overlay, in_sicv, 0, spacecom_patch); }
static INT32 Init29() { return MwInit(&cfg_spacerng, NULL, in_sicv, 0, NULL); }
static INT32 Init30() { return MwInit(&cfg_invasion, &invaders_overlay, in_sicv, 0, NULL); }
static INT32 Init31() { return MwInit(&cfg_cosmicbat, &invaders_overlay, in_sicv, 0, NULL); }
static INT32 Init32() { return MwInit(&cfg_invaders, &invaders_overlay, in_jspecter, 0, NULL); }
static INT32 Init33() { return MwInit(&cfg_darthvdr, NULL, in_sicv, 0, NULL); }
static INT32 Init34() { return MwInit(&cfg_lrescue, NULL, in_sicv, 0, NULL); }
static INT32 Init35() { return MwInit(&cfg_escmars, &escmars_overlay, in_sicv, 0, NULL); }
static INT32 Init36() { return MwInit(&cfg_schaser, NULL, in_sicv, 0, NULL); }
static INT32 Init37() { return MwInit(&cfg_schasercv, NULL, in_sicv, 0, NULL); }
static INT32 Init38() { return MwInit(&cfg_crashrd, NULL, in_sicv, 0, NULL); }
static INT32 Init39() { return MwInit(&cfg_lupin3, NULL, in_sicv, 0, NULL); }
static INT32 Init40() { return MwInit(&cfg_lupin3a, NULL, in_sicv, 0, NULL); }
static INT32 Init41() { return MwInit(&cfg_polaris, NULL, in_sicv, 0, NULL); }
static INT32 Init42() { return MwInit(&cfg_ballbomb, NULL, in_sicv, 0, NULL); }
static INT32 Init43() { return MwInit(&cfg_indianbt, NULL, in_sicv, 0, NULL); }
static INT32 Init44() { return MwInit(&cfg_indianbtbr, NULL, in_sicv, 0, NULL); }
static INT32 Init45() { return MwInit(&cfg_steelwkr, NULL, in_sicv, 0, NULL); }
static INT32 Init46() { return MwInit(&cfg_invaders, &galactic_overlay, in_sicv, 0, NULL); }
static INT32 Init47() { return MwInit(&cfg_starw1, NULL, in_sicv, 0, NULL); }
static INT32 Init48() { return MwInit(&cfg_cosmo, NULL, in_sicv, 0, NULL); }
static INT32 Init49() { return MwInit(&cfg_invrvnge, NULL, in_sicv, 0, invrvnge_decrypt); }
static INT32 Init50() { return MwInit(&cfg_invrvnge, NULL, in_sicv, 0, NULL); }
static INT32 Init51() { return MwInit(&cfg_vortex, NULL, in_sicv, 0, vortex_decrypt); }
static INT32 Init52() { return MwInit(&cfg_rollingc, NULL, in_sicv, 0, NULL); }
static INT32 Init53() { return MwInit(&cfg_mraker, NULL, in_sicv, 0, NULL); }
static INT32 Init54() { return MwInit(&cfg_ozmawars, NULL, in_sicv, 0, NULL); }
static INT32 Init55() { return MwInit(&cfg_yosakdon, &yosakdon_overlay, in_sicv, 0, NULL); }
static INT32 Init56() { return MwInit(&cfg_shuttlei, &shuttlei_overlay, in_sicv, 0, NULL); }
static INT32 Init57() { return MwInit(&cfg_claybust, NULL, in_sicv, 0, NULL); }
static INT32 Init58() { return MwInit(&cfg_claybust, &gunchamp_overlay, in_sicv, 0, NULL); }
static INT32 Init59() { return MwInit(&cfg_cane, NULL, in_sicv, 0, NULL); }
static INT32 Init60() { return MwInit(&cfg_orbite, NULL, in_sicv, 0, NULL); }
static INT32 Init61() { return MwInit(&cfg_astropal, NULL, in_sicv, 0, NULL); }
static INT32 Init62() { return MwInit(&cfg_attackfc, &attackfc_overlay, in_sicv, 0, attackfc_decrypt); }
static INT32 Init63() { return MwInit(&cfg_attackfcu, &attackfc_overlay, in_sicv, 0, NULL); }

// Sea Wolf (set 1)

static struct BurnRomInfo seawolfRomDesc[] = {
	{ "sw0041.h",            0x0400, 0x8f597323, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sw0042.g",            0x0400, 0xdb980974, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "sw0043.f",            0x0400, 0xe6ffa008, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "sw0044.e",            0x0400, 0xc3557d6a, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(seawolf)
STD_ROM_FN(seawolf)

struct BurnDriver BurnDrvSeawolf = {
	"seawolf", NULL, NULL, "seawolf", "1976",
	"Sea Wolf (set 1)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, seawolfRomInfo, seawolfRomName, NULL, NULL, SeawolfSampleInfo, SeawolfSampleName, SeawolfInputInfo, SeawolfDIPInfo,
	Init0, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Sea Wolf (set 2)

static struct BurnRomInfo seawolfoRomDesc[] = {
	{ "1.h1",                0x0200, 0x941b8f2b, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2.g1",                0x0200, 0xc047ef88, MW_ROM(R_MAIN, 0x0200) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3.f1",                0x0200, 0x9624b1ab, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4.e1",                0x0200, 0x553ff531, MW_ROM(R_MAIN, 0x0600) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5.d1",                0x0200, 0xe8e07d03, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6.c1",                0x0200, 0xe2ffe499, MW_ROM(R_MAIN, 0x0a00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7.b1",                0x0200, 0xd40a52b5, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "8.a1",                0x0200, 0xda61df76, MW_ROM(R_MAIN, 0x0e00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(seawolfo)
STD_ROM_FN(seawolfo)

struct BurnDriver BurnDrvSeawolfo = {
	"seawolfo", "seawolf", NULL, "seawolf", "1976",
	"Sea Wolf (set 2)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, seawolfoRomInfo, seawolfoRomName, NULL, NULL, SeawolfSampleInfo, SeawolfSampleName, SeawolfInputInfo, SeawolfDIPInfo,
	Init0, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Sea Wolf (set 3)

static struct BurnRomInfo seawolfaRomDesc[] = {
	{ "9316b-2863_9316b-h-sw.h1", 0x0800, 0xe542c042, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "9316b-2801_9316b-g-sw.g1", 0x0800, 0x4c41a1d1, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
};

STD_ROM_PICK(seawolfa)
STD_ROM_FN(seawolfa)

struct BurnDriver BurnDrvSeawolfa = {
	"seawolfa", "seawolf", NULL, "seawolf", "1976",
	"Sea Wolf (set 3)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, seawolfaRomInfo, seawolfaRomName, NULL, NULL, SeawolfSampleInfo, SeawolfSampleName, SeawolfInputInfo, SeawolfDIPInfo,
	Init0, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Gun Fight (set 1)

static struct BurnRomInfo gunfightRomDesc[] = {
	{ "7609h.bin",           0x0400, 0x0b117d73, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "7609g.bin",           0x0400, 0x57bc3159, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "7609f.bin",           0x0400, 0x8049a6bd, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "7609e.bin",           0x0400, 0x773264e2, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(gunfight)
STD_ROM_FN(gunfight)

struct BurnDriver BurnDrvGunfight = {
	"gunfight", NULL, NULL, NULL, "1975",
	"Gun Fight (set 1)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, gunfightRomInfo, gunfightRomName, NULL, NULL, NULL, NULL, GunfightInputInfo, GunfightDIPInfo,
	Init1, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Gun Fight (set 2)

static struct BurnRomInfo gunfightoRomDesc[] = {
	{ "gf-h.h",              0x0200, 0x9d29cc7a, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "gf-g.g",              0x0200, 0x5816911b, MW_ROM(R_MAIN, 0x0200) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "gf-f.f",              0x0200, 0x58f6ee8d, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "gf-e.e",              0x0200, 0x59078036, MW_ROM(R_MAIN, 0x0600) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "gf-d.d",              0x0200, 0x2b64e17f, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "gf-c.c",              0x0200, 0xe0bbf98c, MW_ROM(R_MAIN, 0x0a00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "gf-b.b",              0x0200, 0x91114108, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "gf-a.a",              0x0200, 0x3fbf9a91, MW_ROM(R_MAIN, 0x0e00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(gunfighto)
STD_ROM_FN(gunfighto)

struct BurnDriver BurnDrvGunfighto = {
	"gunfighto", "gunfight", NULL, NULL, "1975",
	"Gun Fight (set 2)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, gunfightoRomInfo, gunfightoRomName, NULL, NULL, NULL, NULL, GunfightInputInfo, GunfightDIPInfo,
	Init1, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Tornado Baseball / Ball Park

static struct BurnRomInfo tornbaseRomDesc[] = {
	{ "tb.h",                0x0800, 0x653f4797, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "tb.g",                0x0800, 0xb63dcdb3, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "tb.f",                0x0800, 0x215e070c, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
};

STD_ROM_PICK(tornbase)
STD_ROM_FN(tornbase)

struct BurnDriver BurnDrvTornbase = {
	"tornbase", NULL, NULL, NULL, "1976",
	"Tornado Baseball / Ball Park\0", NULL, "Dave Nutting Associates / Midway / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, tornbaseRomInfo, tornbaseRomName, NULL, NULL, NULL, NULL, TornbaseInputInfo, TornbaseDIPInfo,
	Init2, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// 280-ZZZAP (set 1)

static struct BurnRomInfo _280zzzapRomDesc[] = {
	{ "zzzaph",              0x0400, 0x1fa86e1c, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "zzzapg",              0x0400, 0x9639bc6b, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "zzzapf",              0x0400, 0xadc6ede1, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "zzzape",              0x0400, 0x472493d6, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "zzzapd",              0x0400, 0x4c240ee1, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "zzzapc",              0x0400, 0x6e85aeaf, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(_280zzzap)
STD_ROM_FN(_280zzzap)

struct BurnDriver BurnDrv_280zzzap = {
	"280zzzap", NULL, NULL, NULL, "1976",
	"280-ZZZAP (set 1)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, _280zzzapRomInfo, _280zzzapRomName, NULL, NULL, NULL, NULL, ZzzapInputInfo, ZzzapDIPInfo,
	Init3, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// 280-ZZZAP (set 2)

static struct BurnRomInfo _280zzzapaRomDesc[] = {
	{ "h",                   0x0800, 0xcb56e0f8, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "g",                   0x0800, 0xf30dd025, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "f",                   0x0800, 0x8d51ec2d, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
};

STD_ROM_PICK(_280zzzapa)
STD_ROM_FN(_280zzzapa)

struct BurnDriver BurnDrv_280zzzapa = {
	"280zzzapa", "280zzzap", NULL, NULL, "1976",
	"280-ZZZAP (set 2)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, _280zzzapaRomInfo, _280zzzapaRomName, NULL, NULL, NULL, NULL, ZzzapInputInfo, ZzzapDIPInfo,
	Init3, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Amazing Maze

static struct BurnRomInfo mazeRomDesc[] = {
	{ "maze.h",              0x0800, 0xf2860cff, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "maze.g",              0x0800, 0x65fad839, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
};

STD_ROM_PICK(maze)
STD_ROM_FN(maze)

struct BurnDriver BurnDrvMaze = {
	"maze", NULL, NULL, NULL, "1976",
	"Amazing Maze\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mazeRomInfo, mazeRomName, NULL, NULL, NULL, NULL, MazeInputInfo, MazeDIPInfo,
	Init4, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Boot Hill

static struct BurnRomInfo boothillRomDesc[] = {
	{ "romh.cpu",            0x0800, 0x1615d077, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "romg.cpu",            0x0800, 0x65a90420, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "romf.cpu",            0x0800, 0x3fdafd79, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "rome.cpu",            0x0800, 0x374529f4, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(boothill)
STD_ROM_FN(boothill)

struct BurnDriver BurnDrvBoothill = {
	"boothill", NULL, NULL, NULL, "1977",
	"Boot Hill\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, boothillRomInfo, boothillRomName, NULL, NULL, NULL, NULL, BoothillInputInfo, BoothillDIPInfo,
	Init5, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Checkmate (set 1)

static struct BurnRomInfo checkmatRomDesc[] = {
	{ "checkmat.h",          0x0400, 0x3481a6d1, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "checkmat.g",          0x0400, 0xdf5fa551, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "checkmat.f",          0x0400, 0x25586406, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "checkmat.e",          0x0400, 0x59330d84, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "checkmat.d",          0x0400, 0x00000000, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  4 i8080 code
};

STD_ROM_PICK(checkmat)
STD_ROM_FN(checkmat)

struct BurnDriver BurnDrvCheckmat = {
	"checkmat", NULL, NULL, NULL, "1977",
	"Checkmate (set 1)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, checkmatRomInfo, checkmatRomName, NULL, NULL, NULL, NULL, CheckmatInputInfo, CheckmatDIPInfo,
	Init6, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Checkmate (set 2)

static struct BurnRomInfo checkmataRomDesc[] = {
	{ "1.h",                 0x0200, 0x15c90cde, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2.g",                 0x0200, 0xaf52a8d0, MW_ROM(R_MAIN, 0x0200) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3.f",                 0x0200, 0xa4650524, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4.e",                 0x0200, 0xd02488c9, MW_ROM(R_MAIN, 0x0600) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5.d",                 0x0200, 0xa0477f03, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6.c",                 0x0200, 0x1ae4e225, MW_ROM(R_MAIN, 0x0a00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7.b",                 0x0200, 0x1eb04641, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "8.a",                 0x0200, 0xb5ce1a2a, MW_ROM(R_MAIN, 0x0e00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(checkmata)
STD_ROM_FN(checkmata)

struct BurnDriver BurnDrvCheckmata = {
	"checkmata", "checkmat", NULL, NULL, "1977",
	"Checkmate (set 2)\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, checkmataRomInfo, checkmataRomName, NULL, NULL, NULL, NULL, CheckmatInputInfo, CheckmatDIPInfo,
	Init6, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Desert Gun

static struct BurnRomInfo desertguRomDesc[] = {
	{ "9316.1h",             0x0800, 0xc0030d7c, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "9316.1g",             0x0800, 0x1ddde10b, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "9316.1f",             0x0800, 0x808e46f1, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "desertgu.e",          0x0800, 0xac64dc62, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(desertgu)
STD_ROM_FN(desertgu)

struct BurnDriver BurnDrvDesertgu = {
	"desertgu", NULL, NULL, NULL, "1977",
	"Desert Gun\0", NULL, "Dave Nutting Associates / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, desertguRomInfo, desertguRomName, NULL, NULL, NULL, NULL, DesertguInputInfo, DesertguDIPInfo,
	Init7, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Road Runner (Midway)

static struct BurnRomInfo roadrunmRomDesc[] = {
	{ "9316.1h",             0x0800, 0xc0030d7c, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "9316.1g",             0x0800, 0x1ddde10b, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "9316.1f",             0x0800, 0x808e46f1, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "9316.1e",             0x0800, 0x06f01571, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(roadrunm)
STD_ROM_FN(roadrunm)

struct BurnDriver BurnDrvRoadrunm = {
	"roadrunm", "desertgu", NULL, NULL, "1977",
	"Road Runner (Midway)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, roadrunmRomInfo, roadrunmRomName, NULL, NULL, NULL, NULL, DesertguInputInfo, DesertguDIPInfo,
	Init7, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Double Play

static struct BurnRomInfo dplayRomDesc[] = {
	{ "dplay619.h",          0x0800, 0x6680669b, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "dplay619.g",          0x0800, 0x0eec7e01, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "dplay619.f",          0x0800, 0x3af4b719, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "dplay619.e",          0x0800, 0x65cab4fc, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(dplay)
STD_ROM_FN(dplay)

struct BurnDriver BurnDrvDplay = {
	"dplay", NULL, NULL, NULL, "1977",
	"Double Play\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, dplayRomInfo, dplayRomName, NULL, NULL, NULL, NULL, DplayInputInfo, DplayDIPInfo,
	Init8, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Laguna Racer

static struct BurnRomInfo lagunarRomDesc[] = {
	{ "lagunar.h",           0x0800, 0x0cd5a280, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "lagunar.g",           0x0800, 0x824cd6f5, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "lagunar.f",           0x0800, 0x62692ca7, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "lagunar.e",           0x0800, 0x20e098ed, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(lagunar)
STD_ROM_FN(lagunar)

struct BurnDriver BurnDrvLagunar = {
	"lagunar", NULL, NULL, NULL, "1977",
	"Laguna Racer\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, lagunarRomInfo, lagunarRomName, NULL, NULL, NULL, NULL, LagunarInputInfo, LagunarDIPInfo,
	Init9, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Guided Missile

static struct BurnRomInfo gmissileRomDesc[] = {
	{ "gm_623.h",            0x0800, 0xa3ebb792, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "gm_623.g",            0x0800, 0xa5e740bb, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "gm_623.f",            0x0800, 0xda381025, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "gm_623.e",            0x0800, 0xf350146b, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(gmissile)
STD_ROM_FN(gmissile)

struct BurnDriver BurnDrvGmissile = {
	"gmissile", NULL, NULL, "gmissile", "1977",
	"Guided Missile\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, gmissileRomInfo, gmissileRomName, NULL, NULL, GmissileSampleInfo, GmissileSampleName, GmissileInputInfo, GmissileDIPInfo,
	Init10, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// M-4

static struct BurnRomInfo m4RomDesc[] = {
	{ "m4.h",                0x0800, 0x9ee2a0b5, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "m4.g",                0x0800, 0x0e84b9cb, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "m4.f",                0x0800, 0x9ded9956, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "m4.e",                0x0800, 0xb6983238, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(m4)
STD_ROM_FN(m4)

struct BurnDriver BurnDrvM4 = {
	"m4", NULL, NULL, "m4", "1977",
	"M-4\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, m4RomInfo, m4RomName, NULL, NULL, M4SampleInfo, M4SampleName, M4InputInfo, M4DIPInfo,
	Init11, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Clowns (rev. 2)

static struct BurnRomInfo clownsRomDesc[] = {
	{ "h2.cpu",              0x0400, 0xff4432eb, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "g2.cpu",              0x0400, 0x676c934b, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "f2.cpu",              0x0400, 0x00757962, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "e2.cpu",              0x0400, 0x9e506a36, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "d2.cpu",              0x0400, 0xd61b5b47, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "c2.cpu",              0x0400, 0x154d129a, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(clowns)
STD_ROM_FN(clowns)

struct BurnDriver BurnDrvClowns = {
	"clowns", NULL, NULL, "clowns", "1978",
	"Clowns (rev. 2)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, clownsRomInfo, clownsRomName, NULL, NULL, ClownsSampleInfo, ClownsSampleName, ClownsInputInfo, ClownsDIPInfo,
	Init12, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Clowns (rev. 1)

static struct BurnRomInfo clowns1RomDesc[] = {
	{ "clownsv1.h",          0x0400, 0x5560c951, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "clownsv1.g",          0x0400, 0x6a571d66, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "clownsv1.f",          0x0400, 0xa2d56cea, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "clownsv1.e",          0x0400, 0xbbd606f6, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "clownsv1.d",          0x0400, 0x37b6ff0e, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "clownsv1.c",          0x0400, 0x12968e52, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(clowns1)
STD_ROM_FN(clowns1)

struct BurnDriver BurnDrvClowns1 = {
	"clowns1", "clowns", NULL, "clowns", "1978",
	"Clowns (rev. 1)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, clowns1RomInfo, clowns1RomName, NULL, NULL, ClownsSampleInfo, ClownsSampleName, Clowns1InputInfo, Clowns1DIPInfo,
	Init12, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Space Walk

static struct BurnRomInfo spacwalkRomDesc[] = {
	{ "sw.h",                0x0400, 0x1b07fc1f, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sw.g",                0x0400, 0x52220910, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "sw.f",                0x0400, 0x787d4ef6, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "sw.e",                0x0400, 0xd62d324b, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "sw.d",                0x0400, 0x17dcc591, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "sw.c",                0x0400, 0x61aef726, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "sw.b",                0x0400, 0xc59d45d0, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "sw.a",                0x0400, 0xd563da07, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(spacwalk)
STD_ROM_FN(spacwalk)

struct BurnDriver BurnDrvSpacwalk = {
	"spacwalk", NULL, NULL, NULL, "1978",
	"Space Walk\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, spacwalkRomInfo, spacwalkRomName, NULL, NULL, NULL, NULL, SpacwalkInputInfo, SpacwalkDIPInfo,
	Init13, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Extra Inning / Ball Park II

static struct BurnRomInfo einningRomDesc[] = {
	{ "ei.h",                0x0800, 0xeff9c7af, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ei.g",                0x0800, 0x5d1e66cb, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ei.f",                0x0800, 0xed96785d, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ei.e",                0x0800, 0xad096a5d, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ei.b",                0x0800, 0x56b407d4, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
};

STD_ROM_PICK(einning)
STD_ROM_FN(einning)

struct BurnDriver BurnDrvEinning = {
	"einning", NULL, NULL, NULL, "1978",
	"Extra Inning / Ball Park II\0", NULL, "Midway / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, einningRomInfo, einningRomName, NULL, NULL, NULL, NULL, EinningInputInfo, EinningDIPInfo,
	Init8, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Shuffleboard

static struct BurnRomInfo shuffleRomDesc[] = {
	{ "shuffle.h",           0x0800, 0x0d422a18, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "shuffle.g",           0x0800, 0x7db7fcf9, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "shuffle.f",           0x0800, 0xcd04d848, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "shuffle.e",           0x0800, 0x2c118357, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(shuffle)
STD_ROM_FN(shuffle)

struct BurnDriver BurnDrvShuffle = {
	"shuffle", NULL, NULL, NULL, "1978",
	"Shuffleboard\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, shuffleRomInfo, shuffleRomName, NULL, NULL, NULL, NULL, ShuffleInputInfo, ShuffleDIPInfo,
	Init14, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Dog Patch

static struct BurnRomInfo dogpatchRomDesc[] = {
	{ "dogpatch.h",          0x0800, 0x74ebdf4d, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "dogpatch.g",          0x0800, 0xac246f70, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "dogpatch.f",          0x0800, 0xa975b011, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "dogpatch.e",          0x0800, 0xc12b1f60, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(dogpatch)
STD_ROM_FN(dogpatch)

struct BurnDriver BurnDrvDogpatch = {
	"dogpatch", NULL, NULL, NULL, "1977",
	"Dog Patch\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, dogpatchRomInfo, dogpatchRomName, NULL, NULL, NULL, NULL, DogpatchInputInfo, DogpatchDIPInfo,
	Init15, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Space Encounters

static struct BurnRomInfo spcenctrRomDesc[] = {
	{ "9316b-4m33_m645h-09s0.h1", 0x0800, 0x7458b2db, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "9316b-4m32_m645g-09s0.g1", 0x0800, 0x1b873788, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "9316b-4m31_m645f-09s0.f1", 0x0800, 0xd4319c91, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "9316b-4m30_m645e-09s0.e1", 0x0800, 0x9b9a1a45, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "9316b-4m29_m645d-09s0.d1", 0x0800, 0x294d52ce, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "9316b-4m28_m645c-09s0.c1", 0x0800, 0xce44c923, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "9316b-4m27_m645b-09s0.b1", 0x0800, 0x098070ab, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "9316b-4m26_m645a-09s0.a1", 0x0800, 0x7f1d1f44, MW_ROM(R_MAIN, 0x5800) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(spcenctr)
STD_ROM_FN(spcenctr)

struct BurnDriver BurnDrvSpcenctr = {
	"spcenctr", NULL, NULL, NULL, "1980",
	"Space Encounters\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spcenctrRomInfo, spcenctrRomName, NULL, NULL, NULL, NULL, SpcenctrInputInfo, SpcenctrDIPInfo,
	Init16, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Phantom II

static struct BurnRomInfo phantom2RomDesc[] = {
	{ "phantom2.h",          0x0800, 0x0e3c2439, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "phantom2.g",          0x0800, 0xe8df3e52, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "phantom2.f",          0x0800, 0x30e83c6d, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "phantom2.e",          0x0800, 0x8c641cac, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "p2clouds.f2",         0x0800, 0xdcdd2927, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  4 color proms
};

STD_ROM_PICK(phantom2)
STD_ROM_FN(phantom2)

struct BurnDriver BurnDrvPhantom2 = {
	"phantom2", NULL, NULL, "phantom2", "1979",
	"Phantom II\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, phantom2RomInfo, phantom2RomName, NULL, NULL, Phantom2SampleInfo, Phantom2SampleName, Phantom2InputInfo, Phantom2DIPInfo,
	Init17, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Bowling Alley

static struct BurnRomInfo bowlerRomDesc[] = {
	{ "h.cpu",               0x0800, 0x74c29b93, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "g.cpu",               0x0800, 0xca26d8b4, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "f.cpu",               0x0800, 0xba8a0bfa, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "e.cpu",               0x0800, 0x4da65a40, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "d.cpu",               0x0800, 0xe7dbc9d9, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
};

STD_ROM_PICK(bowler)
STD_ROM_FN(bowler)

struct BurnDriver BurnDrvBowler = {
	"bowler", NULL, NULL, NULL, "1978",
	"Bowling Alley\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, bowlerRomInfo, bowlerRomName, NULL, NULL, NULL, NULL, BowlerInputInfo, BowlerDIPInfo,
	Init18, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders / Space Invaders M

static struct BurnRomInfo invadersRomDesc[] = {
	{ "9316b-0869_m739h.h1", 0x0800, 0x734f5ad8, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "9316b-0856_m739g.g1", 0x0800, 0x6bfaca4a, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "9316b-0855_m739f.f1", 0x0800, 0x0ccead96, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "9316b-0854_m739e.e1", 0x0800, 0x14e538b0, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(invaders)
STD_ROM_FN(invaders)

struct BurnDriver BurnDrvInvaders = {
	"invaders", NULL, NULL, NULL, "1978",
	"Space Invaders / Space Invaders M\0", NULL, "Taito / Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invadersRomInfo, invadersRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, InvadersDIPInfo,
	Init19, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Blue Shark (Midway)

static struct BurnRomInfo blueshrkRomDesc[] = {
	{ "blueshrk.h",          0x0800, 0x4ff94187, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "blueshrk.g",          0x0800, 0xe49368fd, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "blueshrk.f",          0x0800, 0x86cca79d, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
};

STD_ROM_PICK(blueshrk)
STD_ROM_FN(blueshrk)

struct BurnDriver BurnDrvBlueshrk = {
	"blueshrk", NULL, NULL, NULL, "1978",
	"Blue Shark (Midway)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, blueshrkRomInfo, blueshrkRomName, NULL, NULL, NULL, NULL, BlueshrkInputInfo, BlueshrkDIPInfo,
	Init20, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Blue Shark (Model Racing bootleg, set 1)

static struct BurnRomInfo blueshrkmrRomDesc[] = {
	{ "mr14.1s",             0x0400, 0xea2ba987, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "mr15.1r",             0x0400, 0x049337a4, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "mr16.1n",             0x0400, 0x3f9793ca, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "mr17.1m",             0x0400, 0x0a73e0eb, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "mr18.1l",             0x0400, 0xa18bb930, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "mr19.1j",             0x0400, 0x23c63d02, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(blueshrkmr)
STD_ROM_FN(blueshrkmr)

struct BurnDriver BurnDrvBlueshrkmr = {
	"blueshrkmr", "blueshrk", NULL, NULL, "1978",
	"Blue Shark (Model Racing bootleg, set 1)\0", NULL, "bootleg (Model Racing)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, blueshrkmrRomInfo, blueshrkmrRomName, NULL, NULL, NULL, NULL, BlueshrkInputInfo, BlueshrkDIPInfo,
	Init20, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Blue Shark (Model Racing bootleg, set 2)

static struct BurnRomInfo blueshrkmr2RomDesc[] = {
	{ "mr14.1s",             0x0400, 0xea2ba987, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "mr15.1r",             0x0400, 0x049337a4, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "mr16.1n",             0x0400, 0x8776920f, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "mr17.1m",             0x0400, 0x0a73e0eb, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "mr18.1l",             0x0400, 0xa18bb930, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "mr19.1j",             0x0400, 0x23c63d02, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(blueshrkmr2)
STD_ROM_FN(blueshrkmr2)

struct BurnDriver BurnDrvBlueshrkmr2 = {
	"blueshrkmr2", "blueshrk", NULL, NULL, "1978",
	"Blue Shark (Model Racing bootleg, set 2)\0", NULL, "bootleg (Model Racing)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, blueshrkmr2RomInfo, blueshrkmr2RomName, NULL, NULL, NULL, NULL, BlueshrkInputInfo, BlueshrkDIPInfo,
	Init20, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Space Invaders II (Midway, cocktail)

static struct BurnRomInfo invad2ctRomDesc[] = {
	{ "invad2ct.h",          0x0800, 0x51d02a71, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "invad2ct.g",          0x0800, 0x533ac770, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "invad2ct.f",          0x0800, 0xd1799f39, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "invad2ct.e",          0x0800, 0x291c1418, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "invad2ct.b",          0x0800, 0x8d9a07c4, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "invad2ct.a",          0x0800, 0xefdabb03, MW_ROM(R_MAIN, 0x5800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(invad2ct)
STD_ROM_FN(invad2ct)

struct BurnDriver BurnDrvInvad2ct = {
	"invad2ct", NULL, NULL, NULL, "1980",
	"Space Invaders II (Midway, cocktail)\0", NULL, "Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invad2ctRomInfo, invad2ctRomName, NULL, NULL, NULL, NULL, Invad2ctInputInfo, Invad2ctDIPInfo,
	Init21, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space King Part II (Space Invaders II bootleg)

static struct BurnRomInfo spcking2irRomDesc[] = {
	{ "gm2_1-2_tms2516.h",   0x0800, 0xa5cad5b7, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "gm2_3-4_tms2516.g",   0x0800, 0xc7b5f8ae, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "gm2_5-6_tms2516.f",   0x0800, 0xa5084884, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "gm2_7-8_tms2516.e",   0x0800, 0x3ff5d28f, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "gm2_9-10_tms2516.d",  0x0800, 0x98db0e7d, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
};

STD_ROM_PICK(spcking2ir)
STD_ROM_FN(spcking2ir)

struct BurnDriver BurnDrvSpcking2ir = {
	"spcking2ir", "invad2ct", NULL, NULL, "1980",
	"Space King Part II (Space Invaders II bootleg)\0", NULL, "bootleg (Irecsa)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spcking2irRomInfo, spcking2irRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, InvadersDIPInfo,
	Init22, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders (SV Version rev 1)

static struct BurnRomInfo sisv1RomDesc[] = {
	{ "sv01.36",             0x0400, 0xd0c32d72, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sv02.35",             0x0400, 0x0e159534, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "sv03.34",             0x0400, 0x00000000, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  2 i8080 code
	{ "sv04.31",             0x0400, 0x1293b826, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "sv05.42",             0x0400, 0x00000000, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS | BRF_NODUMP }, //  4 i8080 code
	{ "sv06.41",             0x0400, 0x2c68e0b4, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(sisv1)
STD_ROM_FN(sisv1)

struct BurnDriver BurnDrvSisv1 = {
	"sisv1", "invaders", NULL, NULL, "1978",
	"Space Invaders (SV Version rev 1)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sisv1RomInfo, sisv1RomName, NULL, NULL, NULL, NULL, SitvInputInfo, SitvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders (SV Version rev 2)

static struct BurnRomInfo sisv2RomDesc[] = {
	{ "sv01.36",             0x0400, 0xd0c32d72, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sv02.35",             0x0400, 0x0e159534, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "sv10.34",             0x0400, 0x483e651e, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "sv04.31",             0x0400, 0x1293b826, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "sv09.42",             0x0400, 0xcd80b13f, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "sv06.41",             0x0400, 0x2c68e0b4, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(sisv2)
STD_ROM_FN(sisv2)

struct BurnDriver BurnDrvSisv2 = {
	"sisv2", "invaders", NULL, NULL, "1978",
	"Space Invaders (SV Version rev 2)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sisv2RomInfo, sisv2RomName, NULL, NULL, NULL, NULL, SitvInputInfo, SitvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders (SV Version rev 3)

static struct BurnRomInfo sisv3RomDesc[] = {
	{ "sv0h.36",             0x0400, 0x86bb8cb6, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sv02.35",             0x0400, 0x0e159534, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "sv10.34",             0x0400, 0x483e651e, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "sv04.31",             0x0400, 0x1293b826, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "sv09.42",             0x0400, 0xcd80b13f, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "sv06.41",             0x0400, 0x2c68e0b4, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(sisv3)
STD_ROM_FN(sisv3)

struct BurnDriver BurnDrvSisv3 = {
	"sisv3", "invaders", NULL, NULL, "1978",
	"Space Invaders (SV Version rev 3)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sisv3RomInfo, sisv3RomName, NULL, NULL, NULL, NULL, SitvInputInfo, SitvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders (SV Version rev 4)

static struct BurnRomInfo sisvRomDesc[] = {
	{ "sv0h.36",             0x0400, 0x86bb8cb6, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sv11.35",             0x0400, 0xfebe6d1a, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "sv12.34",             0x0400, 0xa08e7202, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "sv04.31",             0x0400, 0x1293b826, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "sv13.42",             0x0400, 0xa9011634, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "sv14.41",             0x0400, 0x58730370, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(sisv)
STD_ROM_FN(sisv)

struct BurnDriver BurnDrvSisv = {
	"sisv", "invaders", NULL, NULL, "1978",
	"Space Invaders (SV Version rev 4)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sisvRomInfo, sisvRomName, NULL, NULL, NULL, NULL, SitvInputInfo, SitvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders (TV Version rev 1)

static struct BurnRomInfo sitv1RomDesc[] = {
	{ "tv01.s1",             0x0800, 0x9f37b146, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "tv02.rp1",            0x0800, 0x3c759a90, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "tv03.n1",             0x0800, 0x0ad3657f, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "tv04.m1",             0x0800, 0xcd2c67f6, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(sitv1)
STD_ROM_FN(sitv1)

struct BurnDriver BurnDrvSitv1 = {
	"sitv1", "invaders", NULL, NULL, "1978",
	"Space Invaders (TV Version rev 1)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sitv1RomInfo, sitv1RomName, NULL, NULL, NULL, NULL, SitvInputInfo, SitvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders (TV Version rev 2)

static struct BurnRomInfo sitvRomDesc[] = {
	{ "tv0h.s1",             0x0800, 0xfef18aad, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "tv02.rp1",            0x0800, 0x3c759a90, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "tv03.n1",             0x0800, 0x0ad3657f, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "tv04.m1",             0x0800, 0xcd2c67f6, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(sitv)
STD_ROM_FN(sitv)

struct BurnDriver BurnDrvSitv = {
	"sitv", "invaders", NULL, NULL, "1978",
	"Space Invaders (TV Version rev 2)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sitvRomInfo, sitvRomName, NULL, NULL, NULL, NULL, SitvInputInfo, SitvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders (CV Version, larger roms)

static struct BurnRomInfo sicvRomDesc[] = {
	{ "cv17.36",             0x0800, 0x3dfbe9e6, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "cv18.35",             0x0800, 0xbc3c82bf, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "cv19.34",             0x0800, 0xd202b41c, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "cv20.33",             0x0800, 0xc74ee7b6, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "cv01.1",              0x0400, 0x037e16ac, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  4 color proms
	{ "cv02.2",              0x0400, 0x8263da38, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  5 color proms
};

STD_ROM_PICK(sicv)
STD_ROM_FN(sicv)

struct BurnDriver BurnDrvSicv = {
	"sicv", "invaders", NULL, NULL, "1979",
	"Space Invaders (CV Version, larger roms)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sicvRomInfo, sicvRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders (CV Version, smaller roms)

static struct BurnRomInfo sicv1RomDesc[] = {
	{ "cv11.s1",             0x0400, 0x309d4582, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "cv12.r1",             0x0400, 0x70153e09, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "cv13.np1",            0x0400, 0x2ca24fee, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "cv14.jk1",            0x0400, 0x556d9a97, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "cv15.i1",             0x0400, 0xac520cf5, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "cv16.g1",             0x0400, 0x285cfb59, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "cv01.1",              0x0400, 0x037e16ac, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
	{ "cv02.2",              0x0400, 0x8263da38, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  7 color proms
};

STD_ROM_PICK(sicv1)
STD_ROM_FN(sicv1)

struct BurnDriver BurnDrvSicv1 = {
	"sicv1", "invaders", NULL, NULL, "1979",
	"Space Invaders (CV Version, smaller roms)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sicv1RomInfo, sicv1RomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders (Model Racing)

static struct BurnRomInfo invadrmrRomDesc[] = {
	{ "11.1t",               0x0400, 0x389d44b6, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sv02.1p",             0x0400, 0x0e159534, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "20.1n",               0x0400, 0x805b04f0, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "sv04.1j",             0x0400, 0x1293b826, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "13.1h",               0x0400, 0x76b4a6ea, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "sv06.1g",             0x0400, 0x2c68e0b4, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(invadrmr)
STD_ROM_FN(invadrmr)

struct BurnDriver BurnDrvInvadrmr = {
	"invadrmr", "invaders", NULL, NULL, "1978",
	"Space Invaders (Model Racing)\0", NULL, "Taito / Model Racing", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invadrmrRomInfo, invadrmrRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, InvadrmrDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders (Logitec)

static struct BurnRomInfo invaderlRomDesc[] = {
	{ "c01",                 0x0400, 0x499f253a, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "c02",                 0x0400, 0x2d0b2e1f, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "c03",                 0x0400, 0x03033dc2, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "c07",                 0x0400, 0x5a7bbf1f, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "c04",                 0x0400, 0x455b1fa7, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "c05",                 0x0400, 0x40cbef75, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "sv06.bin",            0x0400, 0x2c68e0b4, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  6 i8080 code
};

STD_ROM_PICK(invaderl)
STD_ROM_FN(invaderl)

struct BurnDriver BurnDrvInvaderl = {
	"invaderl", "invaders", NULL, NULL, "1978",
	"Space Invaders (Logitec)\0", NULL, "Taito / Logitec", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invaderlRomInfo, invaderlRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders (Nas Corp bootleg)

static struct BurnRomInfo invaderncRomDesc[] = {
	{ "1.ic65",              0x0400, 0x499f253a, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2.ic66",              0x0400, 0x4b7f232e, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3.ic67",              0x0400, 0x5c6bdd47, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "7.ic57",              0x0400, 0x5a7bbf1f, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "4.ic70",              0x0400, 0x455b1fa7, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "5.ic71",              0x0400, 0xb9ea71a0, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "6.ic72",              0x0400, 0x2c68e0b4, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "82s137_1.ic17",       0x0400, 0x1eb2bc60, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  7 color proms
};

STD_ROM_PICK(invadernc)
STD_ROM_FN(invadernc)

struct BurnDriver BurnDrvInvadernc = {
	"invadernc", "invaders", NULL, NULL, "1978",
	"Space Invaders (Nas Corp bootleg)\0", NULL, "bootleg (Nas Corp)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invaderncRomInfo, invaderncRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space War (Sanritsu)

static struct BurnRomInfo spcewarsRomDesc[] = {
	{ "sanritsu.1",          0x0400, 0xca331679, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sanritsu.2",          0x0400, 0x48dc791c, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "sanritsu.3",          0x0400, 0xc34842cb, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "sanritsu.4",          0x0400, 0xa7fdfd0e, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "sanritsu.5",          0x0400, 0x77475431, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "sanritsu.6",          0x0400, 0x392ef82c, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "sanritsu.7",          0x0400, 0xb3a93df8, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "sanritsu.8",          0x0400, 0x64fdc3e1, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "sanritsu.9",          0x0400, 0xb2f29601, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
};

STD_ROM_PICK(spcewars)
STD_ROM_FN(spcewars)

struct BurnDriver BurnDrvSpcewars = {
	"spcewars", "invaders", NULL, NULL, "1978",
	"Space War (Sanritsu)\0", NULL, "Taito / Sanritsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spcewarsRomInfo, spcewarsRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SpcewarsDIPInfo,
	Init25, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space War (Leisure and Allied)

static struct BurnRomInfo spcewarlaRomDesc[] = {
	{ "ps1.bin",             0x0400, 0x222f6913, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ps2.bin",             0x0400, 0x48dc791c, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ps3.bin",             0x0400, 0x58ddc18c, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ps4.bin",             0x0400, 0x1da5e383, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ps5.bin",             0x0400, 0x3b6d9f23, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "ps6.bin",             0x0400, 0x50be9b7a, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "ps7.bin",             0x0400, 0x7b8efd7c, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "ps8.bin",             0x0400, 0x64fdc3e1, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "ps9.bin",             0x0400, 0xb2f29601, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "cv01_1.bin",          0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  9 color proms
	{ "cv02_2.bin",          0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, // 10 color proms
};

STD_ROM_PICK(spcewarla)
STD_ROM_FN(spcewarla)

struct BurnDriver BurnDrvSpcewarla = {
	"spcewarla", "invaders", NULL, NULL, "1979",
	"Space War (Leisure and Allied)\0", NULL, "bootleg (Leisure and Allied)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spcewarlaRomInfo, spcewarlaRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SpcewarsDIPInfo,
	Init26, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space King

static struct BurnRomInfo spcekingRomDesc[] = {
	{ "invaders.h",          0x0800, 0x734f5ad8, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "spcekng2",            0x0800, 0x96dcdd42, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "spcekng3",            0x0800, 0x95fc96ad, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "spcekng4",            0x0800, 0x54170ada, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(spceking)
STD_ROM_FN(spceking)

struct BurnDriver BurnDrvSpceking = {
	"spceking", "invaders", NULL, NULL, "1978",
	"Space King\0", NULL, "Taito / Leijac", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spcekingRomInfo, spcekingRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Battle (Space Invaders bootleg)

static struct BurnRomInfo spcebttlRomDesc[] = {
	{ "1",                   0x0400, 0x86bb8cb6, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2",                   0x0400, 0xfebe6d1a, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3",                   0x0400, 0xe11ef4ae, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4",                   0x0400, 0x1293b826, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5",                   0x0400, 0x3c89b4d5, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6",                   0x0400, 0xe154f4e5, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(spcebttl)
STD_ROM_FN(spcebttl)

struct BurnDriver BurnDrvSpcebttl = {
	"spcebttl", "invaders", NULL, NULL, "1978",
	"Space Battle (Space Invaders bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spcebttlRomInfo, spcebttlRomName, NULL, NULL, NULL, NULL, SitvInputInfo, SitvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Cosmic Monsters (version II)

static struct BurnRomInfo cosmicmoRomDesc[] = {
	{ "ii-1.h1",             0x0400, 0xd6e4e5da, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ii-2.h2",             0x0400, 0x8f7988e6, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ii-3.h3",             0x0400, 0x2d2e9dc8, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ii-4.h4",             0x0400, 0x26cae456, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ii-5.h5",             0x0400, 0xb13f228e, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "ii-6.h6",             0x0400, 0x4ae1b9c4, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "ii-7.h7",             0x0400, 0x6a13b15b, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
};

STD_ROM_PICK(cosmicmo)
STD_ROM_FN(cosmicmo)

struct BurnDriver BurnDrvCosmicmo = {
	"cosmicmo", "invaders", NULL, NULL, "1979",
	"Cosmic Monsters (version II)\0", NULL, "bootleg (Universal)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, cosmicmoRomInfo, cosmicmoRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, CosmicmoDIPInfo,
	Init27, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Cosmic Monsters 2

static struct BurnRomInfo cosmicm2RomDesc[] = {
	{ "3907.bin",            0x1000, 0xbbffede6, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "3906.bin",            0x1000, 0xb841f894, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  1 i8080 code
};

STD_ROM_PICK(cosmicm2)
STD_ROM_FN(cosmicm2)

struct BurnDriver BurnDrvCosmicm2 = {
	"cosmicm2", "invaders", NULL, NULL, "1979",
	"Cosmic Monsters 2\0", NULL, "bootleg (Universal)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, cosmicm2RomInfo, cosmicm2RomName, NULL, NULL, NULL, NULL, InvadersInputInfo, CosmicmoDIPInfo,
	Init27, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Super Invaders (Zenitone-Microsec)

static struct BurnRomInfo sinvzenRomDesc[] = {
	{ "1.bin",               0x0400, 0x9b0da779, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2.bin",               0x0400, 0x9858ccab, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3.bin",               0x0400, 0xa1cc38b5, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4.bin",               0x0400, 0x1f2db7a8, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5.bin",               0x0400, 0x9b505fcd, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6.bin",               0x0400, 0xde0ca0ae, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7.bin",               0x0400, 0x25a296f6, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "8.bin",               0x0400, 0xf4bc4a98, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(sinvzen)
STD_ROM_FN(sinvzen)

struct BurnDriver BurnDrvSinvzen = {
	"sinvzen", "invaders", NULL, NULL, "1980?",
	"Super Invaders (Zenitone-Microsec)\0", NULL, "Taito / Zenitone-Microsec", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sinvzenRomInfo, sinvzenRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SinvzenDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Attack II (bootleg of Super Invaders)

static struct BurnRomInfo spaceat2RomDesc[] = {
	{ "spaceatt.h",          0x0800, 0xa31d0756, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "spaceatt.g",          0x0800, 0xf41241f7, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "spaceatt.f",          0x0800, 0x4c060223, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "spaceatt.e",          0x0800, 0x7cf6f604, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(spaceat2)
STD_ROM_FN(spaceat2)

struct BurnDriver BurnDrvSpaceat2 = {
	"spaceat2", "invaders", NULL, NULL, "1980",
	"Space Attack II (bootleg of Super Invaders)\0", NULL, "bootleg (Video Games UK)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spaceat2RomInfo, spaceat2RomName, NULL, NULL, NULL, NULL, InvadersInputInfo, Spaceat2DIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders Part Four (bootleg of Space Attack II)

static struct BurnRomInfo invader4RomDesc[] = {
	{ "spin4.a",             0x0800, 0xbb386dfe, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "spin4.b",             0x0800, 0x63afa11d, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "spin4.c",             0x0800, 0x22b0317c, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "spin4.d",             0x0800, 0x9102fd68, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(invader4)
STD_ROM_FN(invader4)

struct BurnDriver BurnDrvInvader4 = {
	"invader4", "invaders", NULL, NULL, "1980",
	"Space Invaders Part Four (bootleg of Space Attack II)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invader4RomInfo, invader4RomName, NULL, NULL, NULL, NULL, InvadersInputInfo, Spaceat2DIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Ultra Invaders

static struct BurnRomInfo ultrainvRomDesc[] = {
	{ "in-01.bin",           0x0400, 0xdb9de599, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "in-02.bin",           0x0400, 0xfebe6d1a, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "in-03.bin",           0x0400, 0x3d5c9820, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "in-04.bin",           0x0400, 0x1293b826, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "in-05.bin",           0x0400, 0xe315a8c4, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "in-06.bin",           0x0400, 0xd958478c, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(ultrainv)
STD_ROM_FN(ultrainv)

struct BurnDriver BurnDrvUltrainv = {
	"ultrainv", "invaders", NULL, NULL, "1980",
	"Ultra Invaders\0", NULL, "Taito / Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ultrainvRomInfo, ultrainvRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Attack (bootleg of Space Invaders)

static struct BurnRomInfo spaceattRomDesc[] = {
	{ "h",                   0x0400, 0xd0c32d72, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sv02.bin",            0x0400, 0x0e159534, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "f",                   0x0400, 0x483e651e, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "c",                   0x0400, 0x1293b826, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "b",                   0x0400, 0x6fc782aa, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "a",                   0x0400, 0x211ac4a3, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(spaceatt)
STD_ROM_FN(spaceatt)

struct BurnDriver BurnDrvSpaceatt = {
	"spaceatt", "invaders", NULL, NULL, "1978",
	"Space Attack (bootleg of Space Invaders)\0", NULL, "bootleg (Video Games GmbH)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spaceattRomInfo, spaceattRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Attack (bootleg of Space Invaders, bproms)

static struct BurnRomInfo spaceattbpRomDesc[] = {

};

STD_ROM_PICK(spaceattbp)
STD_ROM_FN(spaceattbp)

struct BurnDriver BurnDrvSpaceattbp = {
	"spaceattbp", "invaders", NULL, NULL, "1978",
	"Space Attack (bootleg of Space Invaders, bproms)\0", NULL, "bootleg (Video Games GmbH)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spaceattbpRomInfo, spaceattbpRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Attack (bootleg of Space Invaders, 2k roms)

static struct BurnRomInfo spaceatt2kRomDesc[] = {
	{ "h1.bin",              0x0800, 0x734f5ad8, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "g1.bin",              0x0800, 0x6bfaca4a, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "f1.bin",              0x0800, 0x0ccead96, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "e1.bin",              0x0800, 0x19971ca7, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(spaceatt2k)
STD_ROM_FN(spaceatt2k)

struct BurnDriver BurnDrvSpaceatt2k = {
	"spaceatt2k", "invaders", NULL, NULL, "1978",
	"Space Attack (bootleg of Space Invaders, 2k roms)\0", NULL, "bootleg (Video Games GmbH)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spaceatt2kRomInfo, spaceatt2kRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Cosmic Invaders (bootleg of Space Invaders)

static struct BurnRomInfo cosmicinRomDesc[] = {
	{ "cn7472n-7921.bin",    0x0800, 0x734f5ad8, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "cn7471n-7918.bin",    0x0800, 0x6bfaca4a, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "cn7470n-7918.bin",    0x0800, 0x0ccead96, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "cn7469n-7921.bin",    0x0800, 0x5733048c, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(cosmicin)
STD_ROM_FN(cosmicin)

struct BurnDriver BurnDrvCosmicin = {
	"cosmicin", "invaders", NULL, NULL, "1978",
	"Cosmic Invaders (bootleg of Space Invaders)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, cosmicinRomInfo, cosmicinRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Galaxy Monsters (Laguna S.A. Spanish bootleg of Space Invaders)

static struct BurnRomInfo galmonstRomDesc[] = {
	{ "h.5m",                0x0400, 0x8a0395e9, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "g.5n",                0x0400, 0x6183ed16, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "f.5p",                0x0400, 0xb6047fdd, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "c.5t",                0x0400, 0xe88e8c96, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "b.5u",                0x0400, 0x34678b80, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "a.5v",                0x0400, 0x05a6806b, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(galmonst)
STD_ROM_FN(galmonst)

struct BurnDriver BurnDrvGalmonst = {
	"galmonst", "invaders", NULL, NULL, "1978",
	"Galaxy Monsters (Laguna S.A. Spanish bootleg of Space Invaders)\0", NULL, "bootleg (Laguna S.A.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, galmonstRomInfo, galmonstRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Combat (bootleg of Space Invaders)

static struct BurnRomInfo spacecomRomDesc[] = {
	{ "1f.ic67",             0x0400, 0x703f2cbe, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2g.ic82",             0x0400, 0x7269b719, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3f.ic68",             0x0400, 0x6badac4f, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4g.ic83",             0x0400, 0x75b59ea7, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5f.ic69",             0x0400, 0x84b61117, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6g.ic84",             0x0400, 0xde383625, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7f.ic70",             0x0400, 0x5a23dbc8, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "8g.ic85",             0x0400, 0xa5a467e3, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(spacecom)
STD_ROM_FN(spacecom)

struct BurnDriver BurnDrvSpacecom = {
	"spacecom", "invaders", NULL, NULL, "1979",
	"Space Combat (bootleg of Space Invaders)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spacecomRomInfo, spacecomRomName, NULL, NULL, NULL, NULL, SpacecomInputInfo, SpacecomDIPInfo,
	Init28, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 256, 3, 4
};

// Space Ranger

static struct BurnRomInfo spacerngRomDesc[] = {
	{ "sr1.u36",             0x0800, 0xb984f52d, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sr2.u35",             0x0800, 0x4b4f07e6, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "sr3.u34",             0x0800, 0xedc28ba9, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "sr4.u33",             0x0800, 0xa95f559f, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "cv01.1",              0x0400, 0x037e16ac, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  4 color proms
	{ "cv02.2",              0x0400, 0x8263da38, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  5 color proms
};

STD_ROM_PICK(spacerng)
STD_ROM_FN(spacerng)

struct BurnDriver BurnDrvSpacerng = {
	"spacerng", "invaders", NULL, NULL, "1978",
	"Space Ranger\0", NULL, "bootleg (Leisure Time Electronics)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spacerngRomInfo, spacerngRomName, NULL, NULL, NULL, NULL, SitvInputInfo, SitvDIPInfo,
	Init29, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Invasion (Sidam)

static struct BurnRomInfo invasionRomDesc[] = {
	{ "10136-0.0k",          0x0400, 0x7a9b4485, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "10136-1.1k",          0x0400, 0x7c86620d, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "10136-2.2k",          0x0400, 0xccaf38f6, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "10136-5.5k",          0x0400, 0x8ec9eae2, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "10136-6.6k",          0x0400, 0xff0b0690, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "10136-7.7k",          0x0400, 0x75d7acaf, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(invasion)
STD_ROM_FN(invasion)

struct BurnDriver BurnDrvInvasion = {
	"invasion", "invaders", NULL, NULL, "19??",
	"Invasion (Sidam)\0", NULL, "bootleg (Sidam)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invasionRomInfo, invasionRomName, NULL, NULL, NULL, NULL, InvasionInputInfo, InvasionDIPInfo,
	Init30, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// UFO Robot Attack (bootleg of Invasion, newer set)

static struct BurnRomInfo invasionaRomDesc[] = {
	{ "invasiona_0.bin",     0x0400, 0xc2fe6197, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "invasiona_1.bin",     0x0400, 0x7c86620d, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "invasiona_2.bin",     0x0400, 0xccaf38f6, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "invasiona_3.bin",     0x0400, 0x8ec9eae2, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "invasiona_4.bin",     0x0400, 0x24b39879, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "invasiona_5.bin",     0x0400, 0x59134ff8, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(invasiona)
STD_ROM_FN(invasiona)

struct BurnDriver BurnDrvInvasiona = {
	"invasiona", "invaders", NULL, NULL, "1979",
	"UFO Robot Attack (bootleg of Invasion, newer set)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invasionaRomInfo, invasionaRomName, NULL, NULL, NULL, NULL, InvasionInputInfo, InvasionDIPInfo,
	Init30, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// UFO Robot Attack (bootleg of Invasion, older set)

static struct BurnRomInfo invasiona2RomDesc[] = {
	{ "0.0.bin",             0x0400, 0x7a9b4485, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "1.1.bin",             0x0400, 0x7c86620d, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "2.2.bin",             0x0400, 0xb949185e, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "3.5.bin",             0x0400, 0x8ec9eae2, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "4.6.bin",             0x0400, 0xec0edb4a, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "5.7.bin",             0x0400, 0xc3466380, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(invasiona2)
STD_ROM_FN(invasiona2)

struct BurnDriver BurnDrvInvasiona2 = {
	"invasiona2", "invaders", NULL, NULL, "1979",
	"UFO Robot Attack (bootleg of Invasion, older set)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invasiona2RomInfo, invasiona2RomName, NULL, NULL, NULL, NULL, InvasionInputInfo, InvasionDIPInfo,
	Init30, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Invasion (Italian bootleg)

static struct BurnRomInfo invasionbRomDesc[] = {
	{ "invasionb_0.bin",     0x0400, 0x7a9b4485, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "invasionb_1.bin",     0x0400, 0x7c86620d, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "invasionb_2.bin",     0x0400, 0xccaf38f6, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "invasionb_5.bin",     0x0400, 0x8ec9eae2, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "invasionb_6.bin",     0x0400, 0xec0edb4a, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "invasionb_7.bin",     0x0400, 0x6aac1281, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(invasionb)
STD_ROM_FN(invasionb)

struct BurnDriver BurnDrvInvasionb = {
	"invasionb", "invaders", NULL, NULL, "1979",
	"Invasion (Italian bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invasionbRomInfo, invasionbRomName, NULL, NULL, NULL, NULL, InvasionInputInfo, InvasionDIPInfo,
	Init30, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Invasion (bootleg set 1, R Z SRL Bologna)

static struct BurnRomInfo invasionrzRomDesc[] = {
	{ "rz0.0k",              0x0400, 0x7a9b4485, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "rz1.1k",              0x0400, 0x7c86620d, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "rz2.2k",              0x0400, 0xccaf38f6, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "rz5.5k",              0x0400, 0x8ec9eae2, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "rz6.6k",              0x0400, 0xec0edb4a, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "rz7.7k",              0x0400, 0xe4ab9012, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(invasionrz)
STD_ROM_FN(invasionrz)

struct BurnDriver BurnDrvInvasionrz = {
	"invasionrz", "invaders", NULL, NULL, "1979",
	"Invasion (bootleg set 1, R Z SRL Bologna)\0", NULL, "bootleg (R Z SRL Bologna)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invasionrzRomInfo, invasionrzRomName, NULL, NULL, NULL, NULL, InvasionInputInfo, InvasionDIPInfo,
	Init30, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Invasion (bootleg set 2, R Z SRL Bologna)

static struct BurnRomInfo invasionrzaRomDesc[] = {
	{ "rz0-0.9k",            0x0400, 0x3044806f, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "rz1-1.8k",            0x0400, 0x7c86620d, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "rz2-2.7k",            0x0400, 0xc808e941, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "rz5-5.4k",            0x0400, 0x8ec9eae2, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "rz6-6.3k",            0x0400, 0xc48df3ca, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "rz6-6.3ka",           0x0400, 0xaa51b2c3, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "rz7-7.2k",            0x0400, 0x27dbea48, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  6 i8080 code
};

STD_ROM_PICK(invasionrza)
STD_ROM_FN(invasionrza)

struct BurnDriver BurnDrvInvasionrza = {
	"invasionrza", "invaders", NULL, NULL, "1979",
	"Invasion (bootleg set 2, R Z SRL Bologna)\0", NULL, "bootleg (R Z SRL Bologna)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invasionrzaRomInfo, invasionrzaRomName, NULL, NULL, NULL, NULL, InvasionInputInfo, InvasionDIPInfo,
	Init30, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders (Electromar, Spanish)

static struct BurnRomInfo invadersemRomDesc[] = {
	{ "h.bin",               0x0400, 0x7fc672a5, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "g.bin",               0x0400, 0xad518883, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "f.bin",               0x0400, 0xf4a6c480, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "c.bin",               0x0400, 0x8f62c513, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "b.bin",               0x0400, 0x2808e5c0, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "a.bin",               0x0400, 0x04c9b084, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(invadersem)
STD_ROM_FN(invadersem)

struct BurnDriver BurnDrvInvadersem = {
	"invadersem", "invaders", NULL, NULL, "19??",
	"Space Invaders (Electromar, Spanish)\0", NULL, "Electromar", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invadersemRomInfo, invadersemRomName, NULL, NULL, NULL, NULL, SitvInputInfo, SitvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Super Invaders (bootleg set 1)

static struct BurnRomInfo superinvRomDesc[] = {
	{ "00",                  0x0400, 0x7a9b4485, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "01",                  0x0400, 0x7c86620d, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "02",                  0x0400, 0xccaf38f6, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "03",                  0x0400, 0x8ec9eae2, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "04",                  0x0400, 0x68719b30, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "05",                  0x0400, 0x8abe2466, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(superinv)
STD_ROM_FN(superinv)

struct BurnDriver BurnDrvSuperinv = {
	"superinv", "invaders", NULL, NULL, "1978",
	"Super Invaders (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, superinvRomInfo, superinvRomName, NULL, NULL, NULL, NULL, SuperinvInputInfo, SuperinvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Super Invaders (Emag bootleg set 1)

static struct BurnRomInfo sinvemagRomDesc[] = {
	{ "sv01.36",             0x0400, 0x86bb8cb6, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "emag_si.b",           0x0400, 0xfebe6d1a, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "emag_si.c",           0x0400, 0xaafb24f7, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "emag_si.d",           0x0400, 0x68c4b9da, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "emag_si.e",           0x0400, 0xc4e80586, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "emag_si.f",           0x0400, 0x077f5ef2, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(sinvemag)
STD_ROM_FN(sinvemag)

struct BurnDriver BurnDrvSinvemag = {
	"sinvemag", "invaders", NULL, NULL, "1978",
	"Super Invaders (Emag bootleg set 1)\0", NULL, "bootleg (Emag)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sinvemagRomInfo, sinvemagRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SinvemagDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Super Invaders (Emag bootleg set 2)

static struct BurnRomInfo sinvemag2RomDesc[] = {
	{ "sie.a",               0x0400, 0x86bb8cb6, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sie.b",               0x0400, 0xfebe6d1a, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "sie.c",               0x0400, 0xad6529f0, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "sie.d",               0x0400, 0x68c4b9da, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "sie.e",               0x0400, 0x636a6b7d, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "sie.f",               0x0400, 0x52062faa, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(sinvemag2)
STD_ROM_FN(sinvemag2)

struct BurnDriver BurnDrvSinvemag2 = {
	"sinvemag2", "invaders", NULL, NULL, "1978",
	"Super Invaders (Emag bootleg set 2)\0", NULL, "bootleg (Emag)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, sinvemag2RomInfo, sinvemag2RomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SinvemagDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Super Earth Invasion (set 1)

static struct BurnRomInfo searthinRomDesc[] = {
	{ "earthinv.h",          0x0800, 0x58a750c8, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "earthinv.g",          0x0800, 0xb91742f1, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "earthinv.f",          0x0800, 0x4acbbc60, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "earthinv.e",          0x0800, 0xdf397b12, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(searthin)
STD_ROM_FN(searthin)

struct BurnDriver BurnDrvSearthin = {
	"searthin", "invaders", NULL, NULL, "1980",
	"Super Earth Invasion (set 1)\0", NULL, "bootleg (Competitive Video)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, searthinRomInfo, searthinRomName, NULL, NULL, NULL, NULL, SearthinInputInfo, SearthinDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Super Earth Invasion (set 2)

static struct BurnRomInfo searthinaRomDesc[] = {
	{ "unkh.h1",             0x0400, 0x272b9bf3, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "unkg.g1",             0x0400, 0x61bb6101, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "unkf.f1",             0x0400, 0x2a8d9cd5, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "unke.e1",             0x0400, 0x1938d349, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "unkd.d1",             0x0400, 0x9bc2ab88, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "unkc.c1",             0x0400, 0xd4e2dada, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "unkb.b1",             0x0400, 0xab645a9c, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "unka.a1",             0x0400, 0x4b65bd7c, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(searthina)
STD_ROM_FN(searthina)

struct BurnDriver BurnDrvSearthina = {
	"searthina", "invaders", NULL, NULL, "1980",
	"Super Earth Invasion (set 2)\0", NULL, "bootleg (Competitive Video)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, searthinaRomInfo, searthinaRomName, NULL, NULL, NULL, NULL, SearthinInputInfo, SearthinDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Super Earth Invasion (Centromatic, Spanish)

static struct BurnRomInfo searthincRomDesc[] = {
	{ "imbader1.bin",        0x0800, 0xc46df7f4, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "imbader2.bin",        0x0800, 0x7b6cddc3, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "imbader3.bin",        0x0800, 0x76429ed7, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "imbader4.bin",        0x0800, 0x7e519b8e, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(searthinc)
STD_ROM_FN(searthinc)

struct BurnDriver BurnDrvSearthinc = {
	"searthinc", "invaders", NULL, NULL, "19??",
	"Super Earth Invasion (Centromatic, Spanish)\0", NULL, "bootleg (Centromatic)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, searthincRomInfo, searthincRomName, NULL, NULL, NULL, NULL, SearthinInputInfo, SearthinDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Super Invasion (Electromar, Spanish)

static struct BurnRomInfo supinvsionRomDesc[] = {
	{ "h-am4708.bin",        0x0400, 0x281570f0, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "g-am4708.bin",        0x0400, 0xc0b6cd79, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "f-am4708.bin",        0x0400, 0x2a8d9cd5, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "e-am4708.bin",        0x0400, 0x03e9ef33, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "d-am4708.bin",        0x0400, 0xb2527c77, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "c-am4708.bin",        0x0400, 0xa883ff01, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "b-am4708.bin",        0x0400, 0x46e02fcf, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "a-am4708.bin",        0x0400, 0xbf4d3267, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(supinvsion)
STD_ROM_FN(supinvsion)

struct BurnDriver BurnDrvSupinvsion = {
	"supinvsion", "invaders", NULL, NULL, "1979",
	"Super Invasion (Electromar, Spanish)\0", NULL, "bootleg (Electromar / Irecsa)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, supinvsionRomInfo, supinvsionRomName, NULL, NULL, NULL, NULL, SearthinInputInfo, SearthinDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Cosmic Battle (Recreativos Franco bootleg of Space Invaders)

static struct BurnRomInfo cosmicbatRomDesc[] = {
	{ "1.5m",                0x0400, 0x6019a1aa, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2.5n",                0x0400, 0xc12eab91, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3.5p",                0x0400, 0x2a8d9cd5, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4.5r",                0x0400, 0x4c77ce80, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5.5s",                0x0400, 0xb2527c77, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6.5t",                0x0400, 0xa883ff01, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7.5u",                0x0400, 0x46e02fcf, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "8.5v",                0x0400, 0xff88f38e, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(cosmicbat)
STD_ROM_FN(cosmicbat)

struct BurnDriver BurnDrvCosmicbat = {
	"cosmicbat", "invaders", NULL, NULL, "1979",
	"Cosmic Battle (Recreativos Franco bootleg of Space Invaders)\0", NULL, "bootleg (Recreativos Franco)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, cosmicbatRomInfo, cosmicbatRomName, NULL, NULL, NULL, NULL, SearthinInputInfo, SearthinDIPInfo,
	Init31, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Super Earth Invasion (set 3)

static struct BurnRomInfo searthieRomDesc[] = {
	{ "searthie.h",          0x0800, 0x92b08b91, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "searthie.g",          0x0800, 0x23e24bcc, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "searthie.f",          0x0800, 0x8700286a, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "searthie.e",          0x0800, 0xbaf949b0, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(searthie)
STD_ROM_FN(searthie)

struct BurnDriver BurnDrvSearthie = {
	"searthie", "invaders", NULL, NULL, "1980",
	"Super Earth Invasion (set 3)\0", NULL, "bootleg (Electrocoin)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, searthieRomInfo, searthieRomName, NULL, NULL, NULL, NULL, SearthinInputInfo, SearthinDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Alien Invasion

static struct BurnRomInfo alieninvRomDesc[] = {
	{ "alieninv.h",          0x0800, 0x6ad601c3, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "alieninv.g",          0x0800, 0xc6bb6fb3, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "alieninv.f",          0x0800, 0x1d2ff324, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "alieninv.e",          0x0800, 0x2f2e6791, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(alieninv)
STD_ROM_FN(alieninv)

struct BurnDriver BurnDrvAlieninv = {
	"alieninv", "invaders", NULL, NULL, "19??",
	"Alien Invasion\0", NULL, "bootleg (Margamatics)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, alieninvRomInfo, alieninvRomName, NULL, NULL, NULL, NULL, SearthinInputInfo, AlieninvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Alien Invasion Part II

static struct BurnRomInfo alieninvp2RomDesc[] = {
	{ "1h.bin",              0x0800, 0xc46df7f4, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "1g.bin",              0x0800, 0x4b1112d6, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "1f.bin",              0x0800, 0xadca18a5, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "1e.bin",              0x0800, 0x0449cb52, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(alieninvp2)
STD_ROM_FN(alieninvp2)

struct BurnDriver BurnDrvAlieninvp2 = {
	"alieninvp2", "invaders", NULL, NULL, "19??",
	"Alien Invasion Part II\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, alieninvp2RomInfo, alieninvp2RomName, NULL, NULL, NULL, NULL, SearthinInputInfo, SearthinDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Jatre Specter (set 1)

static struct BurnRomInfo jspecterRomDesc[] = {
	{ "3305.u6",             0x1000, 0xab211a4f, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "3306.u7",             0x1000, 0x0df142a7, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
};

STD_ROM_PICK(jspecter)
STD_ROM_FN(jspecter)

struct BurnDriver BurnDrvJspecter = {
	"jspecter", "invaders", NULL, NULL, "1979",
	"Jatre Specter (set 1)\0", NULL, "Taito / Jatre", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, jspecterRomInfo, jspecterRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, JspecterDIPInfo,
	Init32, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Jatre Specter (set 2)

static struct BurnRomInfo jspecter2RomDesc[] = {
	{ "unksi.b2",            0x1000, 0x0584b6c4, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "unksi.a2",            0x1000, 0x58095955, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
};

STD_ROM_PICK(jspecter2)
STD_ROM_FN(jspecter2)

struct BurnDriver BurnDrvJspecter2 = {
	"jspecter2", "invaders", NULL, NULL, "1979",
	"Jatre Specter (set 2)\0", NULL, "Taito / Jatre", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, jspecter2RomInfo, jspecter2RomName, NULL, NULL, NULL, NULL, InvadersInputInfo, JspecterDIPInfo,
	Init32, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space War Part 3

static struct BurnRomInfo spacewr3RomDesc[] = {
	{ "ic36.bin",            0x0800, 0x9e30f88a, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ic35.bin",            0x0800, 0x40c2d55b, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ic34.bin",            0x0800, 0xb435f021, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ic33.bin",            0x0800, 0xcbdc6fe8, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ic32.bin",            0x0800, 0x1e5a753c, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
};

STD_ROM_PICK(spacewr3)
STD_ROM_FN(spacewr3)

struct BurnDriver BurnDrvSpacewr3 = {
	"spacewr3", "invaders", NULL, NULL, "1978",
	"Space War Part 3\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spacewr3RomInfo, spacewr3RomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init25, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Wipeout

static struct BurnRomInfo swipeoutRomDesc[] = {
	{ "sw1.bin",             0x0800, 0x576b5897, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sw2.bin",             0x0800, 0x40c2d55b, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "sw3.bin",             0x0800, 0x65e8ce64, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "sw4.bin",             0x0800, 0xddf1fb9c, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "sw5.bin",             0x0800, 0x1e5a753c, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
};

STD_ROM_PICK(swipeout)
STD_ROM_FN(swipeout)

struct BurnDriver BurnDrvSwipeout = {
	"swipeout", "invaders", NULL, NULL, "1978",
	"Space Wipeout\0", NULL, "bootleg (Beyer and Brown)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, swipeoutRomInfo, swipeoutRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init25, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Darth Vader (bootleg of Space Invaders)

static struct BurnRomInfo darthvdrRomDesc[] = {
	{ "rom0",                0x0400, 0xb15785b6, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "rom1",                0x0400, 0x95947743, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "rom2",                0x0400, 0x19b1731f, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "rom3",                0x0400, 0xca1b5e3c, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "rom4",                0x0400, 0xeede5f41, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "rom5",                0x0400, 0xcc52a4bb, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(darthvdr)
STD_ROM_FN(darthvdr)

struct BurnDriver BurnDrvDarthvdr = {
	"darthvdr", "invaders", NULL, NULL, "1978",
	"Darth Vader (bootleg of Space Invaders)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, darthvdrRomInfo, darthvdrRomName, NULL, NULL, NULL, NULL, DarthvdrInputInfo, DarthvdrDIPInfo,
	Init33, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders Test ROM

static struct BurnRomInfo tst_invdRomDesc[] = {
	{ "test.h",              0x0800, 0xf86a2eea, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "invaders.g",          0x0800, 0x6bfaca4a, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "invaders.f",          0x0800, 0x0ccead96, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "invaders.e",          0x0800, 0x14e538b0, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(tst_invd)
STD_ROM_FN(tst_invd)

struct BurnDriver BurnDrvTst_invd = {
	"tst_invd", "invaders", NULL, NULL, "19??",
	"Space Invaders Test ROM\0", NULL, "<unknown>", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, tst_invdRomInfo, tst_invdRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SicvDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Space Invaders Part II (Taito, bigger ROMs)

static struct BurnRomInfo invadpt2RomDesc[] = {
	{ "pv01",                0x0800, 0x7288a511, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "pv02",                0x0800, 0x097dd8d5, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "pv03",                0x0800, 0x1766337e, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "pv04",                0x0800, 0x8f0e62e0, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "pv05",                0x0800, 0x19b505e9, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "pv06.1",              0x0400, 0xa732810b, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  5 color proms
	{ "pv07.2",              0x0400, 0x2c5b91cb, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  6 color proms
};

STD_ROM_PICK(invadpt2)
STD_ROM_FN(invadpt2)

struct BurnDriver BurnDrvInvadpt2 = {
	"invadpt2", NULL, NULL, NULL, "1979",
	"Space Invaders Part II (Taito, bigger ROMs)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invadpt2RomInfo, invadpt2RomName, NULL, NULL, NULL, NULL, Invadpt2InputInfo, Invadpt2DIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders Part II (Taito, smaller ROMs)

static struct BurnRomInfo invadpt2aRomDesc[] = {
	{ "uv01.36",             0x0400, 0x82dbf2c7, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "uv02.35",             0x0400, 0xc867f5b4, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "uv03.34",             0x0400, 0xcb23ccc1, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "uv04.33",             0x0400, 0x9a11abe2, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "uv05.32",             0x0400, 0x787821dd, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "uv06.31",             0x0400, 0xf5e8114f, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "uv07.42",             0x0400, 0x07839f04, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "uv08.41",             0x0400, 0xa7e1c6ef, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "uv09.40",             0x0400, 0x261a39ae, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "uv10.39",             0x0400, 0xb2cbcc8b, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "pv06",                0x0400, 0xa732810b, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, // 10 color proms
	{ "pv07",                0x0400, 0x2c5b91cb, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, // 11 color proms
};

STD_ROM_PICK(invadpt2a)
STD_ROM_FN(invadpt2a)

struct BurnDriver BurnDrvInvadpt2a = {
	"invadpt2a", "invadpt2", NULL, NULL, "1979",
	"Space Invaders Part II (Taito, smaller ROMs)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invadpt2aRomInfo, invadpt2aRomName, NULL, NULL, NULL, NULL, Invadpt2InputInfo, Invadpt2DIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders Part II (Brazil)

static struct BurnRomInfo invadpt2brRomDesc[] = {
	{ "pv01",                0x0800, 0x7288a511, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "br_pv02",             0x0800, 0x420c7c35, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "br_pv03",             0x0800, 0xdffd04b9, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "br_pv04",             0x0800, 0xb0626aff, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "br_pv05",             0x0800, 0x84c70bb8, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "pv06.1",              0x0400, 0xa732810b, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  5 color proms
	{ "pv07.2",              0x0400, 0x2c5b91cb, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  6 color proms
};

STD_ROM_PICK(invadpt2br)
STD_ROM_FN(invadpt2br)

struct BurnDriver BurnDrvInvadpt2br = {
	"invadpt2br", "invadpt2", NULL, NULL, "1979",
	"Space Invaders Part II (Brazil)\0", NULL, "Taito do Brasil", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invadpt2brRomInfo, invadpt2brRomName, NULL, NULL, NULL, NULL, Invadpt2InputInfo, Invadpt2DIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Invaders Deluxe

static struct BurnRomInfo invaddlxRomDesc[] = {
	{ "invdelux.h",          0x0800, 0xe690818f, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "invdelux.g",          0x0800, 0x4268c12d, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "invdelux.f",          0x0800, 0xf4aa1880, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "invdelux.e",          0x0800, 0x408849c1, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "invdelux.d",          0x0800, 0xe8d5afcd, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
};

STD_ROM_PICK(invaddlx)
STD_ROM_FN(invaddlx)

struct BurnDriver BurnDrvInvaddlx = {
	"invaddlx", "invadpt2", NULL, NULL, "1980",
	"Space Invaders Deluxe\0", NULL, "Taito (Midway license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invaddlxRomInfo, invaddlxRomName, NULL, NULL, NULL, NULL, Invadpt2InputInfo, Invadpt2DIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Moon Base Zeta (set 1)

static struct BurnRomInfo moonbaseRomDesc[] = {
	{ "ze3-1.a4",            0x0400, 0x82dbf2c7, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ze3-2.c4",            0x0400, 0xc867f5b4, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ze3-3.e4",            0x0400, 0xcb23ccc1, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ze3-4.f4",            0x0400, 0x9a11abe2, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ze3-5.h4",            0x0400, 0x2b105ed3, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "ze3-6.l4",            0x0400, 0xcb3d6dcb, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "ze3-7.a5",            0x0400, 0x774b52c9, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "ze3-8.c5",            0x0400, 0xe88ea83b, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "ze3-9.e5",            0x0400, 0x2dd5adfa, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "ze3-10.f5",           0x0400, 0x1e7c22a4, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "cv02.h7",             0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, // 10 color proms
	{ "cv01.g7",             0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, // 11 color proms
};

STD_ROM_PICK(moonbase)
STD_ROM_FN(moonbase)

struct BurnDriver BurnDrvMoonbase = {
	"moonbase", "invadpt2", NULL, NULL, "1979",
	"Moon Base Zeta (set 1)\0", NULL, "Taito / Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, moonbaseRomInfo, moonbaseRomName, NULL, NULL, NULL, NULL, Invadpt2InputInfo, Invadpt2DIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Moon Base Zeta (set 2)

static struct BurnRomInfo moonbaseaRomDesc[] = {
	{ "ze3-1.a4",            0x0400, 0x82dbf2c7, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ze3-2.c4",            0x0400, 0xc867f5b4, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ze3-3.e4",            0x0400, 0xcb23ccc1, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ze3-4_alt.f4",        0x0400, 0x86a00411, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ze3-5.h4",            0x0400, 0x2b105ed3, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "ze3-6.l4",            0x0400, 0xcb3d6dcb, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "ze3-7.a5",            0x0400, 0x774b52c9, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "ze3-8.c5",            0x0400, 0xe88ea83b, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "ze3-9.e5",            0x0400, 0x2dd5adfa, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "ze3-10.f5",           0x0400, 0x1e7c22a4, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "cv02.h7",             0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, // 10 color proms
	{ "cv01.g7",             0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, // 11 color proms
};

STD_ROM_PICK(moonbasea)
STD_ROM_FN(moonbasea)

struct BurnDriver BurnDrvMoonbasea = {
	"moonbasea", "invadpt2", NULL, NULL, "1979",
	"Moon Base Zeta (set 2)\0", NULL, "Taito / Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, moonbaseaRomInfo, moonbaseaRomName, NULL, NULL, NULL, NULL, Invadpt2InputInfo, Invadpt2DIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space War (Leijac Corporation)

static struct BurnRomInfo spcewarlRomDesc[] = {
	{ "spcewarl.1",          0x0800, 0x1fcd34d2, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "spcewarl.2",          0x0800, 0x43bc65c5, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "spcewarl.3",          0x0800, 0x7820df3a, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "spcewarl.4",          0x0800, 0xadc05b8d, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "01.1",                0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  4 color proms
	{ "02.2",                0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  5 color proms
};

STD_ROM_PICK(spcewarl)
STD_ROM_FN(spcewarl)

struct BurnDriver BurnDrvSpcewarl = {
	"spcewarl", NULL, NULL, NULL, "1979",
	"Space War (Leijac Corporation)\0", NULL, "Konami (Leijac Corporation license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spcewarlRomInfo, spcewarlRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SpclaserDIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Laser

static struct BurnRomInfo spclaserRomDesc[] = {
	{ "la01",                0x0800, 0xbedc0078, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "la02",                0x0800, 0x43bc65c5, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "la03",                0x0800, 0x1083e9cc, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "la04",                0x0800, 0x5116b234, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "01.1",                0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  4 color proms
	{ "02.2",                0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  5 color proms
};

STD_ROM_PICK(spclaser)
STD_ROM_FN(spclaser)

struct BurnDriver BurnDrvSpclaser = {
	"spclaser", "spcewarl", NULL, NULL, "1980",
	"Space Laser\0", NULL, "Konami (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spclaserRomInfo, spclaserRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SpclaserDIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Intruder

static struct BurnRomInfo intruderRomDesc[] = {
	{ "la01-1.36",           0x0800, 0xbedc0078, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "la02-1.35",           0x0800, 0x43bc65c5, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "la03-1.34",           0x0800, 0x278ef9cf, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "la04-1.33",           0x0800, 0x5116b234, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "01.1",                0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  4 color proms
	{ "02.2",                0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  5 color proms
};

STD_ROM_PICK(intruder)
STD_ROM_FN(intruder)

struct BurnDriver BurnDrvIntruder = {
	"intruder", "spcewarl", NULL, NULL, "1980",
	"Intruder\0", NULL, "Konami (Game Plan license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, intruderRomInfo, intruderRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SpclaserDIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Astro Laser (bootleg of Space Laser)

static struct BurnRomInfo laserRomDesc[] = {
	{ "1.u36",               0x0800, 0xb44e2c41, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2.u35",               0x0800, 0x9876f331, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3.u34",               0x0800, 0xed79000b, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4.u33",               0x0800, 0x10a160a1, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "01.1",                0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  4 color proms
	{ "02.2",                0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  5 color proms
};

STD_ROM_PICK(laser)
STD_ROM_FN(laser)

struct BurnDriver BurnDrvLaser = {
	"laser", "spcewarl", NULL, NULL, "1980",
	"Astro Laser (bootleg of Space Laser)\0", NULL, "bootleg (Leisure Time Electronics)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, laserRomInfo, laserRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SpclaserDIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Lunar Rescue

static struct BurnRomInfo lrescueRomDesc[] = {
	{ "lrescue.1",           0x0800, 0x2bbc4778, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "lrescue.2",           0x0800, 0x49e79706, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "lrescue.3",           0x0800, 0x1ac969be, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "lrescue.4",           0x0800, 0x782fee3c, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "lrescue.5",           0x0800, 0x58fde8bc, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "lrescue.6",           0x0800, 0xbfb0f65d, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7643-1.cpu",          0x0400, 0x8b2e38de, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
};

STD_ROM_PICK(lrescue)
STD_ROM_FN(lrescue)

struct BurnDriver BurnDrvLrescue = {
	"lrescue", NULL, NULL, "lrescue", "1979",
	"Lunar Rescue\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, lrescueRomInfo, lrescueRomName, NULL, NULL, LrescueSampleInfo, LrescueSampleName, LrescueInputInfo, LrescueDIPInfo,
	Init34, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Galaxy Rescue

static struct BurnRomInfo grescueRomDesc[] = {
	{ "lrescue.1",           0x0800, 0x2bbc4778, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "lrescue.2",           0x0800, 0x49e79706, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "lrescue.3",           0x0800, 0x1ac969be, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "grescue.4",           0x0800, 0xca412991, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "grescue.5",           0x0800, 0xa419a4d6, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "lrescue.6",           0x0800, 0xbfb0f65d, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7643-1.cpu",          0x0400, 0x8b2e38de, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
};

STD_ROM_PICK(grescue)
STD_ROM_FN(grescue)

struct BurnDriver BurnDrvGrescue = {
	"grescue", "lrescue", NULL, "lrescue", "1979",
	"Galaxy Rescue\0", NULL, "Taito (Universal license?)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, grescueRomInfo, grescueRomName, NULL, NULL, LrescueSampleInfo, LrescueSampleName, LrescueInputInfo, LrescueDIPInfo,
	Init34, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Moon Lander (bootleg of Lunar Rescue, set 1)

static struct BurnRomInfo mlanderRomDesc[] = {
	{ "ml1.u36",             0x0800, 0x69df529a, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ml2.u35",             0x0800, 0x3b503337, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ml3.u34",             0x0800, 0x64e53458, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ml4.u33",             0x0800, 0xc9a74571, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ml5.u32",             0x0800, 0x88291fa2, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "ml6.u31",             0x0800, 0xbfb0f65d, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "01.bin",              0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
	{ "02.bin",              0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  7 color proms
};

STD_ROM_PICK(mlander)
STD_ROM_FN(mlander)

struct BurnDriver BurnDrvMlander = {
	"mlander", "lrescue", NULL, "lrescue", "1980",
	"Moon Lander (bootleg of Lunar Rescue, set 1)\0", NULL, "bootleg (Leisure Time Electronics)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, mlanderRomInfo, mlanderRomName, NULL, NULL, LrescueSampleInfo, LrescueSampleName, LrescueInputInfo, LrescueDIPInfo,
	Init34, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Moon Lander (bootleg of Lunar Rescue, set 2)

static struct BurnRomInfo mlanderaRomDesc[] = {
	{ "ml-1_u36.u36",        0x0800, 0x2bbc4778, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ml-2_u35.u35",        0x0800, 0x752fb664, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ml-3_u34.u34",        0x0800, 0x64e53458, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ml-4_u33.u33",        0x0800, 0xc9a74571, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ml-5_u32_rev_d.u32",  0x0800, 0xdb5e57cd, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "ml-6_u31.u31",        0x0800, 0xbfb0f65d, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "cv01.bin",            0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
	{ "cv02.bin",            0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  7 color proms
};

STD_ROM_PICK(mlandera)
STD_ROM_FN(mlandera)

struct BurnDriver BurnDrvMlandera = {
	"mlandera", "lrescue", NULL, "lrescue", "1980",
	"Moon Lander (bootleg of Lunar Rescue, set 2)\0", NULL, "bootleg (Leisure Time Electronics)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, mlanderaRomInfo, mlanderaRomName, NULL, NULL, LrescueSampleInfo, LrescueSampleName, LrescueInputInfo, LrescueDIPInfo,
	Init34, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Lunar Rescue (Model Racing bootleg, set 1)

static struct BurnRomInfo lrescuemRomDesc[] = {
	{ "48.ic36",             0x0400, 0xbad5ba48, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "49.ic35",             0x0400, 0xa6dc23d6, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "50.ic34",             0x0400, 0x90179fee, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "51.ic33",             0x0400, 0x1d197d87, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "52.ic32",             0x0400, 0x4326d338, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "53.ic31",             0x0400, 0x3b272372, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "54.ic42",             0x0400, 0xa877c5b6, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "55.ic41",             0x0400, 0xc9a93407, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "56.ic40",             0x0400, 0x3398798f, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "57.ic39",             0x0400, 0x37c5bfc6, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "58.ic38",             0x0400, 0x1b7a5644, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, // 10 i8080 code
	{ "59.ic37",             0x0400, 0xc342b907, MW_ROM(R_MAIN, 0x4c00) | BRF_PRG | BRF_ESS }, // 11 i8080 code
	{ "cv01-7643.2c",        0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, // 12 color proms
	{ "cv02-7643.1c",        0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, // 13 color proms
};

STD_ROM_PICK(lrescuem)
STD_ROM_FN(lrescuem)

struct BurnDriver BurnDrvLrescuem = {
	"lrescuem", "lrescue", NULL, "lrescue", "1979",
	"Lunar Rescue (Model Racing bootleg, set 1)\0", NULL, "bootleg (Model Racing)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, lrescuemRomInfo, lrescuemRomName, NULL, NULL, LrescueSampleInfo, LrescueSampleName, LrescueInputInfo, LrescueDIPInfo,
	Init34, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Lunar Rescue (Model Racing bootleg, set 2)

static struct BurnRomInfo lrescuem2RomDesc[] = {
	{ "0.bin",               0x0800, 0x27d37ad6, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "1.bin",               0x0800, 0xd8ed56f0, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "2.bin",               0x0800, 0x3aed9788, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "3.bin",               0x0800, 0xfa121b92, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "4.bin",               0x0800, 0x535b4a78, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "5.bin",               0x0800, 0x0613a977, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "6.bin",               0x0800, 0x8fe51cc0, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "cv01-7643.2c",        0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  7 color proms
	{ "cv02-7643.1c",        0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  8 color proms
};

STD_ROM_PICK(lrescuem2)
STD_ROM_FN(lrescuem2)

struct BurnDriver BurnDrvLrescuem2 = {
	"lrescuem2", "lrescue", NULL, "lrescue", "1979",
	"Lunar Rescue (Model Racing bootleg, set 2)\0", NULL, "bootleg (Model Racing)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, lrescuem2RomInfo, lrescuem2RomName, NULL, NULL, LrescueSampleInfo, LrescueSampleName, LrescueInputInfo, LrescueDIPInfo,
	Init34, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Lunar Rescue (Artic bootleg)

static struct BurnRomInfo lrescueablRomDesc[] = {
	{ "2708_1.1",            0x0400, 0xe3796dec, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2708_2.2",            0x0400, 0x8067c036, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "2708_3.3",            0x0400, 0x90179fee, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "2708_4.4",            0x0400, 0x1d197d87, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "2708_5.5",            0x0400, 0x4326d338, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "2708_6.6",            0x0400, 0x3b272372, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "2708_7.7",            0x0400, 0x06fc1ecc, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "2708_8.8",            0x0400, 0xb3a3f24e, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "2708_9.9",            0x0400, 0x3398798f, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "2708_10.10",          0x0400, 0x37c5bfc6, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "2708_11.11",          0x0400, 0x1b7a5644, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, // 10 i8080 code
	{ "2708_12.12",          0x0400, 0xc342b907, MW_ROM(R_MAIN, 0x4c00) | BRF_PRG | BRF_ESS }, // 11 i8080 code
	{ "cv01-7643.2c",        0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, // 12 color proms
	{ "cv02-7643.1c",        0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, // 13 color proms
};

STD_ROM_PICK(lrescueabl)
STD_ROM_FN(lrescueabl)

struct BurnDriver BurnDrvLrescueabl = {
	"lrescueabl", "lrescue", NULL, "lrescue", "1979",
	"Lunar Rescue (Artic bootleg)\0", NULL, "bootleg (Artic)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, lrescueablRomInfo, lrescueablRomName, NULL, NULL, LrescueSampleInfo, LrescueSampleName, LrescueInputInfo, LrescueDIPInfo,
	Init34, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Destination Earth (bootleg of Lunar Rescue)

static struct BurnRomInfo desterthRomDesc[] = {
	{ "36_h.bin",            0x0800, 0xf86923e5, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "35_g.bin",            0x0800, 0x797f440d, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "34_f.bin",            0x0800, 0x993d0846, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "33_e.bin",            0x0800, 0x8d155fc5, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "32_d.bin",            0x0800, 0x3f531b6f, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "31_c.bin",            0x0800, 0xab019c30, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "42_b.bin",            0x0800, 0xed9dbac6, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "7643-1.cpu",          0x0400, 0x8b2e38de, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  7 color proms
};

STD_ROM_PICK(desterth)
STD_ROM_FN(desterth)

struct BurnDriver BurnDrvDesterth = {
	"desterth", "lrescue", NULL, "lrescue", "1979",
	"Destination Earth (bootleg of Lunar Rescue)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, desterthRomInfo, desterthRomName, NULL, NULL, LrescueSampleInfo, LrescueSampleName, LrescueInputInfo, LrescueDIPInfo,
	Init34, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Escape from Mars (bootleg of Lunar Rescue)

static struct BurnRomInfo escmarsRomDesc[] = {
	{ "2516_em.m5",          0x0800, 0x6580f1c3, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2716_em.n5",          0x0800, 0x49e79706, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "2516_em.p5",          0x0800, 0x1ac969be, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "2516_em.r5",          0x0800, 0xc1bd5949, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "2716_em.s5",          0x0800, 0x1ec21a31, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "2716_em.t5",          0x0800, 0xbfb0f65d, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(escmars)
STD_ROM_FN(escmars)

struct BurnDriver BurnDrvEscmars = {
	"escmars", "lrescue", NULL, "lrescue", "1980",
	"Escape from Mars (bootleg of Lunar Rescue)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, escmarsRomInfo, escmarsRomName, NULL, NULL, LrescueSampleInfo, LrescueSampleName, LrescueInputInfo, EscmarsDIPInfo,
	Init35, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Rescate Lunar (Spanish bootleg of Lunar Rescue)

static struct BurnRomInfo resclunarRomDesc[] = {
	{ "1-2716.h1",           0x0800, 0x6234e240, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2-2516.g1",           0x0800, 0x67ab3599, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3-2716.f1",           0x0800, 0x337b6266, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4-2716.e1",           0x0800, 0xdd90ad9a, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5-2716.d1",           0x0800, 0x741212d4, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6-8516.c1",           0x0800, 0xc8994fc7, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7-2516.b1",           0x0800, 0x1adff5d7, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
};

STD_ROM_PICK(resclunar)
STD_ROM_FN(resclunar)

struct BurnDriver BurnDrvResclunar = {
	"resclunar", "lrescue", NULL, "lrescue", "1980",
	"Rescate Lunar (Spanish bootleg of Lunar Rescue)\0", NULL, "bootleg (Niemer S.A.)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, resclunarRomInfo, resclunarRomName, NULL, NULL, LrescueSampleInfo, LrescueSampleName, LrescueInputInfo, EscmarsDIPInfo,
	Init35, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Chaser (set 1)

static struct BurnRomInfo schaserRomDesc[] = {
	{ "rt13.bin",            0x0400, 0x0dfbde68, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "rt14.bin",            0x0400, 0x5a508a25, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "rt15.bin",            0x0400, 0x2ac43a93, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "rt16.bin",            0x0400, 0xf5583afc, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "rt17.bin",            0x0400, 0x51cf1155, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "rt18.bin",            0x0400, 0x3f0fc73a, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "rt19.bin",            0x0400, 0xb66ea369, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "rt20.bin",            0x0400, 0xe3a7466a, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "rt21.bin",            0x0400, 0xb368ac98, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "rt22.bin",            0x0400, 0x6e060dfb, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "rt06.ic2",            0x0400, 0x950cf973, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, // 10 color proms
};

STD_ROM_PICK(schaser)
STD_ROM_FN(schaser)

struct BurnDriver BurnDrvSchaser = {
	"schaser", NULL, NULL, NULL, "1979",
	"Space Chaser (set 1)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, schaserRomInfo, schaserRomName, NULL, NULL, NULL, NULL, SchaserInputInfo, SchaserDIPInfo,
	Init36, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Chaser (set 2)

static struct BurnRomInfo schaseraRomDesc[] = {
	{ "rt13.bin",            0x0800, 0x7b0bfeed, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "rt15.bin",            0x0800, 0x825fc8ac, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "rt17.bin",            0x0800, 0xde9d3f85, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "rt19.bin",            0x0800, 0xc0adab87, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "rt21.bin",            0x0800, 0xa3b31070, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "rt06.ic2",            0x0400, 0x950cf973, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  5 color proms
};

STD_ROM_PICK(schasera)
STD_ROM_FN(schasera)

struct BurnDriver BurnDrvSchasera = {
	"schasera", "schaser", NULL, NULL, "1979",
	"Space Chaser (set 2)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, schaseraRomInfo, schaseraRomName, NULL, NULL, NULL, NULL, SchaserInputInfo, SchaserDIPInfo,
	Init36, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Chaser (set 3)

static struct BurnRomInfo schaserbRomDesc[] = {
	{ "rt33.bin",            0x0800, 0xeec6b032, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "rt34.bin",            0x0800, 0x13a73701, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "rt35.bin",            0x0800, 0xde9d3f85, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "rt36.bin",            0x0800, 0x521ec25e, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "rt37.bin",            0x0800, 0x44f65f19, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "rt06.ic2",            0x0400, 0x950cf973, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  5 color proms
};

STD_ROM_PICK(schaserb)
STD_ROM_FN(schaserb)

struct BurnDriver BurnDrvSchaserb = {
	"schaserb", "schaser", NULL, NULL, "1979",
	"Space Chaser (set 3)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, schaserbRomInfo, schaserbRomName, NULL, NULL, NULL, NULL, SchaserInputInfo, SchaserDIPInfo,
	Init36, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Chaser (set 4)

static struct BurnRomInfo schasercRomDesc[] = {
	{ "45.ic30",             0x0400, 0xca90619c, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "46.ic36",             0x0400, 0x6a016895, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "rt15.bin",            0x0400, 0x2ac43a93, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "rt16.bin",            0x0400, 0xf5583afc, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "rt17.bin",            0x0400, 0x51cf1155, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "rt18.bin",            0x0400, 0x3f0fc73a, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "rt19.bin",            0x0400, 0xb66ea369, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "47.ic39",             0x0400, 0xd476e182, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "rt21.bin",            0x0400, 0xb368ac98, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "rt22.bin",            0x0400, 0x6e060dfb, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "rt06.ic2",            0x0400, 0x950cf973, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, // 10 color proms
};

STD_ROM_PICK(schaserc)
STD_ROM_FN(schaserc)

struct BurnDriver BurnDrvSchaserc = {
	"schaserc", "schaser", NULL, NULL, "1979",
	"Space Chaser (set 4)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, schasercRomInfo, schasercRomName, NULL, NULL, NULL, NULL, SchaserInputInfo, SchaserDIPInfo,
	Init36, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Chaser (CV version - set 1)

static struct BurnRomInfo schasercvRomDesc[] = {
	{ "1",                   0x0400, 0xbec2b16b, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2",                   0x0400, 0x9d25e608, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3",                   0x0400, 0x113d0635, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4",                   0x0400, 0xf3a43c8d, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5",                   0x0400, 0x47c84f23, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6",                   0x0400, 0x02ff2199, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7",                   0x0400, 0x87d06b88, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "8",                   0x0400, 0x6dfaad08, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "9",                   0x0400, 0x3d1a2ae3, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "10",                  0x0400, 0x037edb99, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "cv01",                0x0400, 0x037e16ac, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, // 10 color proms
	{ "cv02",                0x0400, 0x8263da38, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, // 11 color proms
};

STD_ROM_PICK(schasercv)
STD_ROM_FN(schasercv)

struct BurnDriver BurnDrvSchasercv = {
	"schasercv", "schaser", NULL, NULL, "1979",
	"Space Chaser (CV version - set 1)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, schasercvRomInfo, schasercvRomName, NULL, NULL, NULL, NULL, SchasercvInputInfo, SchasercvDIPInfo,
	Init37, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Chaser (Model Racing bootleg)

static struct BurnRomInfo schasermRomDesc[] = {
	{ "mr26.71",             0x0800, 0x4e547879, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "rt08.70",             0x0800, 0x825fc8ac, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "rt09.69",             0x0800, 0xde9d3f85, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "mr27.62",             0x0800, 0x069ec108, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "rt11.61",             0x0800, 0x17a7ef7a, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "rt06.ic2",            0x0400, 0x950cf973, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  5 color proms
};

STD_ROM_PICK(schaserm)
STD_ROM_FN(schaserm)

struct BurnDriver BurnDrvSchaserm = {
	"schaserm", "schaser", NULL, NULL, "1979",
	"Space Chaser (Model Racing bootleg)\0", NULL, "bootleg (Model Racing)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, schasermRomInfo, schasermRomName, NULL, NULL, NULL, NULL, SchaserInputInfo, SchasermDIPInfo,
	Init36, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Crash Road (bootleg of Space Chaser)

static struct BurnRomInfo crashrdRomDesc[] = {
	{ "2716-5m.bin",         0x0800, 0x53749427, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2716-5n.bin",         0x0800, 0xe391d768, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "2716-5p.bin",         0x0800, 0xfededc5d, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "2716-5r.bin",         0x0800, 0x30830779, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "2716-5s.bin",         0x0800, 0x6a974917, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "rt06.ic2",            0x0400, 0x950cf973, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  5 color proms
};

STD_ROM_PICK(crashrd)
STD_ROM_FN(crashrd)

struct BurnDriver BurnDrvCrashrd = {
	"crashrd", "schaser", NULL, NULL, "1979",
	"Crash Road (bootleg of Space Chaser)\0", NULL, "bootleg (Centromatic)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, crashrdRomInfo, crashrdRomName, NULL, NULL, NULL, NULL, SchaserInputInfo, SchasermDIPInfo,
	Init38, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Lupin III (set 1)

static struct BurnRomInfo lupin3RomDesc[] = {
	{ "lp01.36",             0x0800, 0xfd506ee8, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "lp02.35",             0x0800, 0xec4225f8, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "lp03.34",             0x0800, 0x9307d377, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "lp04.33",             0x0800, 0xe41e8b2b, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "lp05.32",             0x0800, 0xf5c2faf4, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "lp06.31",             0x0800, 0x66289ab2, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "lp08.1",              0x0400, 0x33dbd03a, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
	{ "lp09.2",              0x0400, 0x9eaee652, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  7 color proms
};

STD_ROM_PICK(lupin3)
STD_ROM_FN(lupin3)

struct BurnDriver BurnDrvLupin3 = {
	"lupin3", NULL, NULL, "lupin3", "1980",
	"Lupin III (set 1)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, lupin3RomInfo, lupin3RomName, NULL, NULL, Lupin3SampleInfo, Lupin3SampleName, Lupin3InputInfo, Lupin3DIPInfo,
	Init39, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Lupin III (set 2)

static struct BurnRomInfo lupin3aRomDesc[] = {
	{ "lp12.bin",            0x0800, 0x68a7f47a, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "lp13.bin",            0x0800, 0xcae9a17b, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "lp14.bin",            0x0800, 0x3553b9e4, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "lp15.bin",            0x0800, 0xacbeef64, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "lp16.bin",            0x0800, 0x19fcdc54, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "lp17.bin",            0x0800, 0x66289ab2, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "lp18.bin",            0x0800, 0x2f07b4ba, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
};

STD_ROM_PICK(lupin3a)
STD_ROM_FN(lupin3a)

struct BurnDriver BurnDrvLupin3a = {
	"lupin3a", "lupin3", NULL, "lupin3", "1980",
	"Lupin III (set 2)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, lupin3aRomInfo, lupin3aRomName, NULL, NULL, Lupin3SampleInfo, Lupin3SampleName, Lupin3InputInfo, Lupin3aDIPInfo,
	Init40, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Polaris (latest version)

static struct BurnRomInfo polarisRomDesc[] = {
	{ "ps01-1.ic71",         0x0800, 0x7d41007c, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ps02-9.ic70",         0x0800, 0x9a5c8cb2, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ps03-1.ic69",         0x0800, 0x21f32415, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ps04-18.ic62",        0x0800, 0xd717aef3, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ps05.ic61",           0x0800, 0x772e31f3, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "ps06-10.ic60",        0x0800, 0x3df77bac, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "ps26.ic60a",          0x0800, 0x9d5c3d50, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "ps08.1b",             0x0400, 0x164aa05d, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  7 color proms
	{ "ps07.2c",             0x0100, 0x2953253b, MW_ROM(R_USER, 0x0000) | BRF_GRA }, //  8 cloud graphics / misc
};

STD_ROM_PICK(polaris)
STD_ROM_FN(polaris)

struct BurnDriver BurnDrvPolaris = {
	"polaris", NULL, NULL, NULL, "1980",
	"Polaris (latest version)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, polarisRomInfo, polarisRomName, NULL, NULL, NULL, NULL, PolarisInputInfo, PolarisDIPInfo,
	Init41, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Polaris (second revision)

static struct BurnRomInfo polarisaRomDesc[] = {
	{ "ps01-1.ic71",         0x0800, 0x7d41007c, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ps02-9.ic70",         0x0800, 0x9a5c8cb2, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ps03-1.ic69",         0x0800, 0x21f32415, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ps04.ic62",           0x0800, 0x65694948, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ps05.ic61",           0x0800, 0x772e31f3, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "ps06-10.ic60",        0x0800, 0x3df77bac, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "ps26.ic60a",          0x0800, 0x9d5c3d50, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "ps08.1b",             0x0400, 0x164aa05d, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  7 color proms
	{ "ps07.2c",             0x0100, 0x2953253b, MW_ROM(R_USER, 0x0000) | BRF_GRA }, //  8 cloud graphics / misc
};

STD_ROM_PICK(polarisa)
STD_ROM_FN(polarisa)

struct BurnDriver BurnDrvPolarisa = {
	"polarisa", "polaris", NULL, NULL, "1980",
	"Polaris (second revision)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, polarisaRomInfo, polarisaRomName, NULL, NULL, NULL, NULL, PolarisInputInfo, PolarisDIPInfo,
	Init41, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Polaris (first revision)

static struct BurnRomInfo polarisbRomDesc[] = {
	{ "ps01.ic71",           0x0800, 0xc04ce5a9, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ps02-9.ic70",         0x0800, 0x9a5c8cb2, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ps03.ic69",           0x0800, 0x8680d7ea, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ps04.ic62",           0x0800, 0x65694948, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ps05.ic61",           0x0800, 0x772e31f3, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "ps06-10.ic60",        0x0800, 0x3df77bac, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "ps08.1b",             0x0400, 0x164aa05d, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
	{ "ps07.2c",             0x0100, 0x2953253b, MW_ROM(R_USER, 0x0000) | BRF_GRA }, //  7 cloud graphics / misc
};

STD_ROM_PICK(polarisb)
STD_ROM_FN(polarisb)

struct BurnDriver BurnDrvPolarisb = {
	"polarisb", "polaris", NULL, NULL, "1980",
	"Polaris (first revision)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, polarisbRomInfo, polarisbRomName, NULL, NULL, NULL, NULL, PolarisInputInfo, PolarisDIPInfo,
	Init41, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Polaris (original version)

static struct BurnRomInfo polarisoRomDesc[] = {
	{ "ps01.ic71",           0x0800, 0xc04ce5a9, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ps02.ic70",           0x0800, 0x18648e4f, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ps03.ic69",           0x0800, 0x8680d7ea, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ps04.ic62",           0x0800, 0x65694948, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ps05.ic61",           0x0800, 0x772e31f3, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "ps06.ic60",           0x0800, 0xf577cb72, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "ps08.1b",             0x0400, 0x164aa05d, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
	{ "ps07.2c",             0x0100, 0x2953253b, MW_ROM(R_USER, 0x0000) | BRF_GRA }, //  7 cloud graphics / misc
};

STD_ROM_PICK(polariso)
STD_ROM_FN(polariso)

struct BurnDriver BurnDrvPolariso = {
	"polariso", "polaris", NULL, NULL, "1980",
	"Polaris (original version)\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, polarisoRomInfo, polarisoRomName, NULL, NULL, NULL, NULL, PolarisInputInfo, PolarisDIPInfo,
	Init41, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Polaris (Brazil)

static struct BurnRomInfo polarisbrRomDesc[] = {
	{ "1",                   0x0800, 0x17015f52, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2",                   0x0800, 0x9a5c8cb2, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3",                   0x0800, 0x60118368, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4",                   0x0800, 0x65694948, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5",                   0x0800, 0x6cb21b31, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6",                   0x0800, 0x3df77bac, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7",                   0x0800, 0x0d811b92, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "ps08.1b",             0x0400, 0x164aa05d, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  7 color proms
	{ "ps07.2c",             0x0100, 0x2953253b, MW_ROM(R_USER, 0x0000) | BRF_GRA }, //  8 cloud graphics / misc
};

STD_ROM_PICK(polarisbr)
STD_ROM_FN(polarisbr)

struct BurnDriver BurnDrvPolarisbr = {
	"polarisbr", "polaris", NULL, NULL, "1981",
	"Polaris (Brazil)\0", NULL, "Taito do Brasil", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, polarisbrRomInfo, polarisbrRomName, NULL, NULL, NULL, NULL, PolarisInputInfo, PolarisDIPInfo,
	Init41, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Balloon Bomber

static struct BurnRomInfo ballbombRomDesc[] = {
	{ "tn01",                0x0800, 0x551585b5, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "tn02",                0x0800, 0x7e1f734f, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "tn03",                0x0800, 0xd93e20bc, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "tn04",                0x0800, 0xd0689a22, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "tn05-1",              0x0800, 0x5d5e94f1, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "tn06",                0x0400, 0x7ec554c4, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  5 color proms
	{ "tn07",                0x0400, 0xdeb0ac82, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  6 color proms
	{ "mb7052.2c",           0x0100, 0x00000000, 0 | BRF_OPT | BRF_NODUMP }, //  7 cloud graphics / misc
};

STD_ROM_PICK(ballbomb)
STD_ROM_FN(ballbomb)

struct BurnDriver BurnDrvBallbomb = {
	"ballbomb", NULL, NULL, NULL, "1980",
	"Balloon Bomber\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ballbombRomInfo, ballbombRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, BallbombDIPInfo,
	Init42, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Indian Battle

static struct BurnRomInfo indianbtRomDesc[] = {
	{ "1.36",                0x0800, 0xddc2b25d, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2.35",                0x0800, 0x6499b062, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3.34",                0x0800, 0x5c51675d, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4.33",                0x0800, 0x70ebec95, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5.32",                0x0800, 0x7b4022f4, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6.31",                0x0800, 0x89bd6f73, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7.42",                0x0800, 0x7060ba0b, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "8.41",                0x0800, 0xeaccfc0a, MW_ROM(R_MAIN, 0x5800) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "mb7054.1",            0x0400, 0x4acf4db3, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  8 color proms
	{ "mb7054.2",            0x0400, 0x62cb3419, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  9 color proms
};

STD_ROM_PICK(indianbt)
STD_ROM_FN(indianbt)

struct BurnDriver BurnDrvIndianbt = {
	"indianbt", NULL, NULL, NULL, "1980",
	"Indian Battle\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, indianbtRomInfo, indianbtRomName, NULL, NULL, NULL, NULL, LrescueInputInfo, IndianbtDIPInfo,
	Init43, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Indian Battle (Brazil)

static struct BurnRomInfo indianbtbrRomDesc[] = {
	{ "1.36",                0x0800, 0x5cf6316b, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2.35",                0x0800, 0x882c7421, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3.34",                0x0800, 0x5c51675d, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4.33",                0x0800, 0x70ebec95, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5.32",                0x0800, 0xaa12dbae, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6.31",                0x0800, 0xd9cb1691, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7.42",                0x0800, 0x7060ba0b, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "8.41",                0x0800, 0xe96699d6, MW_ROM(R_MAIN, 0x5800) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "mb7054.1",            0x0400, 0x4acf4db3, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  8 color proms
	{ "mb7054.2",            0x0400, 0x62cb3419, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  9 color proms
};

STD_ROM_PICK(indianbtbr)
STD_ROM_FN(indianbtbr)

struct BurnDriver BurnDrvIndianbtbr = {
	"indianbtbr", "indianbt", NULL, NULL, "1983",
	"Indian Battle (Brazil)\0", NULL, "Taito do Brasil", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, indianbtbrRomInfo, indianbtbrRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, IndianbtbrDIPInfo,
	Init44, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Steel Worker

static struct BurnRomInfo steelwkrRomDesc[] = {
	{ "1.36",                0x0400, 0x5d78873a, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2.35",                0x0400, 0x99cd70c6, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3.34",                0x0400, 0x18103b67, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4.33",                0x0400, 0xc413ae82, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5.32",                0x0400, 0xca7b07b5, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6.31",                0x0400, 0xf8181fa0, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7.42",                0x0400, 0xa35f113e, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "8.41",                0x0400, 0xaf208370, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "la05.1",              0x0400, 0x98f31392, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  8 color proms
	{ "la06.2",              0x0400, 0x98f31392, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  9 color proms
};

STD_ROM_PICK(steelwkr)
STD_ROM_FN(steelwkr)

struct BurnDriver BurnDrvSteelwkr = {
	"steelwkr", NULL, NULL, NULL, "1980",
	"Steel Worker\0", NULL, "Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, steelwkrRomInfo, steelwkrRomName, NULL, NULL, NULL, NULL, SteelwkrInputInfo, SteelwkrDIPInfo,
	Init45, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Galactica - Batalha Espacial

static struct BurnRomInfo galacticRomDesc[] = {
	{ "1",                   0x0800, 0xb5098f1e, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2",                   0x0800, 0xf97410ee, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3",                   0x0800, 0xc1175feb, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4",                   0x0800, 0xb4451d7c, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5",                   0x0800, 0x74c9da61, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6",                   0x0800, 0x5e7c6c44, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7",                   0x0800, 0x02619e18, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
};

STD_ROM_PICK(galactic)
STD_ROM_FN(galactic)

struct BurnDriver BurnDrvGalactic = {
	"galactic", NULL, NULL, NULL, "1980?",
	"Galactica - Batalha Espacial\0", NULL, "Taito do Brasil", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, galacticRomInfo, galacticRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, GalacticDIPInfo,
	Init46, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Missile - Space Fighting Game

static struct BurnRomInfo spacmissRomDesc[] = {
	{ "1",                   0x0800, 0xe212dc88, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2",                   0x0800, 0xf97410ee, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3",                   0x0800, 0xc1175feb, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4",                   0x0800, 0xb4451d7c, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5",                   0x0800, 0x74c9da61, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6",                   0x0800, 0x5e7c6c44, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7",                   0x0800, 0x02619e18, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "8",                   0x0800, 0x942e5261, 0 | BRF_OPT }, //  7 cloud graphics / misc
};

STD_ROM_PICK(spacmiss)
STD_ROM_FN(spacmiss)

struct BurnDriver BurnDrvSpacmiss = {
	"spacmiss", "galactic", NULL, NULL, "1980?",
	"Space Missile - Space Fighting Game\0", NULL, "bootleg?", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spacmissRomInfo, spacmissRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, GalacticDIPInfo,
	Init46, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Galaxy Wars (Universal set 1)

static struct BurnRomInfo galxwarsRomDesc[] = {
	{ "univgw3.0",           0x0400, 0x937796f4, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "univgw4.1",           0x0400, 0x4b86e7a6, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "univgw5.2",           0x0400, 0x47a187cd, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "univgw6.3",           0x0400, 0x7b7d22ff, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "univgw1.4",           0x0400, 0x0871156e, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "univgw2.5",           0x0400, 0x6036d7bf, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "01.1",                0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
	{ "02.2",                0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  7 color proms
};

STD_ROM_PICK(galxwars)
STD_ROM_FN(galxwars)

struct BurnDriver BurnDrvGalxwars = {
	"galxwars", NULL, NULL, NULL, "1979",
	"Galaxy Wars (Universal set 1)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, galxwarsRomInfo, galxwarsRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, GalxwarsDIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Galaxy Wars (Universal set 2)

static struct BurnRomInfo galxwars2RomDesc[] = {
	{ "3192.h6",             0x1000, 0xbde6860b, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "3193.h7",             0x1000, 0xa17cd507, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "01.1",                0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  2 color proms
	{ "02.2",                0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  3 color proms
};

STD_ROM_PICK(galxwars2)
STD_ROM_FN(galxwars2)

struct BurnDriver BurnDrvGalxwars2 = {
	"galxwars2", "galxwars", NULL, NULL, "1979",
	"Galaxy Wars (Universal set 2)\0", NULL, "Universal", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, galxwars2RomInfo, galxwars2RomName, NULL, NULL, NULL, NULL, InvadersInputInfo, GalxwarsDIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Galaxy Wars (Taito?)

static struct BurnRomInfo galxwarstRomDesc[] = {
	{ "galxwars.0",          0x0400, 0x608bfe7f, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "galxwars.1",          0x0400, 0xa810b258, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "galxwars.2",          0x0400, 0x74f31781, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "galxwars.3",          0x0400, 0xc88f886c, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "galxwars.4",          0x0400, 0xae4fe8fb, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "galxwars.5",          0x0400, 0x37708a35, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "01.1",                0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
	{ "02.2",                0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  7 color proms
};

STD_ROM_PICK(galxwarst)
STD_ROM_FN(galxwarst)

struct BurnDriver BurnDrvGalxwarst = {
	"galxwarst", "galxwars", NULL, NULL, "1979",
	"Galaxy Wars (Taito?)\0", NULL, "Universal (Taito license?)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, galxwarstRomInfo, galxwarstRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, GalxwarsDIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Galaxy Wars (Taito)

static struct BurnRomInfo galxwarst2RomDesc[] = {
	{ "gv01.bin",            0x0400, 0x0eeb9952, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "gv02.bin",            0x0400, 0xd385c224, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "gv03.bin",            0x0400, 0xbb9201af, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "gv04.bin",            0x0400, 0x9a2a5b68, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "gv05.bin",            0x0400, 0x43ac3f02, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "gv06.bin",            0x0400, 0x3243af69, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "01.1",                0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
	{ "02.2",                0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  7 color proms
};

STD_ROM_PICK(galxwarst2)
STD_ROM_FN(galxwarst2)

struct BurnDriver BurnDrvGalxwarst2 = {
	"galxwarst2", "galxwars", NULL, NULL, "1979",
	"Galaxy Wars (Taito)\0", NULL, "Universal (Taito Corporation license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, galxwarst2RomInfo, galxwarst2RomName, NULL, NULL, NULL, NULL, InvadersInputInfo, GalxwarsDIPInfo,
	Init24, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Star Wars (bootleg of Galaxy Wars, set 1)

static struct BurnRomInfo starwRomDesc[] = {
	{ "roma",                0x0400, 0x60e8993c, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "romb",                0x0400, 0xb8060773, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "romc",                0x0400, 0x307ce6b8, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "romd",                0x0400, 0x2b0d0a88, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "rome",                0x0400, 0x5b1c3ad0, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "romf",                0x0400, 0xc8e42d3d, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(starw)
STD_ROM_FN(starw)

struct BurnDriver BurnDrvStarw = {
	"starw", "galxwars", NULL, NULL, "1979",
	"Star Wars (bootleg of Galaxy Wars, set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, starwRomInfo, starwRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, GalxwarsDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Star Wars (bootleg of Galaxy Wars, set 2)

static struct BurnRomInfo starw1RomDesc[] = {
	{ "gc.75",               0x0400, 0xad10c128, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "gc.77",               0x0400, 0xab77c474, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "gc.76",               0x0400, 0x3638aed4, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "gc.80",               0x0400, 0x4c67957b, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "gc.81",               0x0400, 0x246621ef, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "gc.82",               0x0400, 0x19bd32ee, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "cv01",                0x0800, 0x8d892ef3, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
	{ "cv02",                0x0800, 0xb44ddde8, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  7 color proms
};

STD_ROM_PICK(starw1)
STD_ROM_FN(starw1)

struct BurnDriver BurnDrvStarw1 = {
	"starw1", "galxwars", NULL, NULL, "1979",
	"Star Wars (bootleg of Galaxy Wars, set 2)\0", NULL, "bootleg (Yamashita)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, starw1RomInfo, starw1RomName, NULL, NULL, NULL, NULL, InvadersInputInfo, GalxwarsDIPInfo,
	Init47, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Cosmo

static struct BurnRomInfo cosmoRomDesc[] = {
	{ "1.36",                0x0800, 0x445c9a98, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2.35",                0x0800, 0xdf3eb731, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3.34",                0x0800, 0x772c813f, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4.33",                0x0800, 0x279f66e6, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5.32",                0x0800, 0xcefb18df, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6.31",                0x0800, 0xb037f6c4, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "7.42",                0x0800, 0xc3831ea2, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "n-1.7d",              0x0800, 0xbd8576f1, MW_ROM(R_PROM, 0x0800) | BRF_GRA }, //  7 color proms
	{ "n-2.6e",              0x0800, 0x48f1ade5, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  8 color proms
};

STD_ROM_PICK(cosmo)
STD_ROM_FN(cosmo)

struct BurnDriver BurnDrvCosmo = {
	"cosmo", NULL, NULL, NULL, "1979",
	"Cosmo\0", NULL, "TDS & MINTS", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, cosmoRomInfo, cosmoRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, CosmoDIPInfo,
	Init48, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Invader's Revenge (set 1)

static struct BurnRomInfo invrvngeRomDesc[] = {
	{ "h.ic36",              0x0800, 0x0e229b9f, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "g.ic35",              0x0800, 0x26b38aa4, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "f.ic34",              0x0800, 0xb3b2749e, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "e.ic33",              0x0800, 0xd8e75102, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "snd.2c",              0x0800, 0x135f3b16, MW_ROM(R_SOUND, 0xc000) | BRF_PRG | BRF_ESS }, //  4 M6802 code
	{ "snd.1c",              0x0800, 0x152fc85e, MW_ROM(R_SOUND, 0xe000) | BRF_PRG | BRF_ESS }, //  5 M6802 code
	{ "colour.bin",          0x0800, 0x7de74988, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
};

STD_ROM_PICK(invrvnge)
STD_ROM_FN(invrvnge)

struct BurnDriver BurnDrvInvrvnge = {
	"invrvnge", NULL, NULL, NULL, "1980?",
	"Invader's Revenge (set 1)\0", NULL, "Zenitone-Microsec Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invrvngeRomInfo, invrvngeRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, InvrvngeDIPInfo,
	Init49, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Invader's Revenge (set 2)

static struct BurnRomInfo invrvngeaRomDesc[] = {
	{ "h.ic36",              0x0800, 0x0914b279, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "g.ic35",              0x0800, 0x84d9497c, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "f.ic34",              0x0800, 0x78d34d97, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "e.ic33",              0x0800, 0x30c71887, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "snd.2c",              0x0800, 0x135f3b16, MW_ROM(R_SOUND, 0xc000) | BRF_PRG | BRF_ESS }, //  4 M6802 code
	{ "snd.1c",              0x0800, 0x152fc85e, MW_ROM(R_SOUND, 0xe000) | BRF_PRG | BRF_ESS }, //  5 M6802 code
	{ "colour.bin",          0x0800, 0x7de74988, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
};

STD_ROM_PICK(invrvngea)
STD_ROM_FN(invrvngea)

struct BurnDriver BurnDrvInvrvngea = {
	"invrvngea", "invrvnge", NULL, NULL, "1980?",
	"Invader's Revenge (set 2)\0", NULL, "Zenitone-Microsec Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invrvngeaRomInfo, invrvngeaRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, InvrvngeDIPInfo,
	Init49, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Invader's Revenge (set 3)

static struct BurnRomInfo invrvngebRomDesc[] = {
	{ "invrvnge.h",          0x0800, 0xaca41bbb, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "invrvnge.g",          0x0800, 0xcfe89dad, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "invrvnge.f",          0x0800, 0xe350de2c, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "invrvnge.e",          0x0800, 0x1ec8dfc8, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "snd.2c",              0x0800, 0x135f3b16, MW_ROM(R_SOUND, 0xc000) | BRF_PRG | BRF_ESS }, //  4 M6802 code
	{ "snd.1c",              0x0800, 0x152fc85e, MW_ROM(R_SOUND, 0xe000) | BRF_PRG | BRF_ESS }, //  5 M6802 code
	{ "colour.bin",          0x0800, 0x7de74988, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
};

STD_ROM_PICK(invrvngeb)
STD_ROM_FN(invrvngeb)

struct BurnDriver BurnDrvInvrvngeb = {
	"invrvngeb", "invrvnge", NULL, NULL, "1980?",
	"Invader's Revenge (set 3)\0", NULL, "Zenitone-Microsec Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invrvngebRomInfo, invrvngebRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, InvrvngeDIPInfo,
	Init49, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Invader's Revenge (Dutchford, single PCB)

static struct BurnRomInfo invrvngeduRomDesc[] = {
	{ "ir.5m",               0x0800, 0xb145cb71, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ir.5n",               0x0800, 0x660e8af3, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ir.5p",               0x0800, 0x6ec5a9ad, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ir.5r",               0x0800, 0x74516811, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ir.1t",               0x0800, 0x135f3b16, MW_ROM(R_SOUND, 0xc000) | BRF_PRG | BRF_ESS }, //  4 M6802 code
	{ "ir.1u",               0x0800, 0x152fc85e, MW_ROM(R_SOUND, 0xe000) | BRF_PRG | BRF_ESS }, //  5 M6802 code
	{ "ir.3r",               0x0800, 0x57da51a9, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
};

STD_ROM_PICK(invrvngedu)
STD_ROM_FN(invrvngedu)

struct BurnDriver BurnDrvInvrvngedu = {
	"invrvngedu", "invrvnge", NULL, NULL, "1980?",
	"Invader's Revenge (Dutchford, single PCB)\0", NULL, "Zenitone-Microsec Ltd. (Dutchford license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invrvngeduRomInfo, invrvngeduRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, InvrvngeDIPInfo,
	Init49, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Invader's Revenge (Game World, single PCB)

static struct BurnRomInfo invrvngegwRomDesc[] = {
	{ "ir.5m",               0x0800, 0x4fe35d1f, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "ir.5n",               0x0800, 0x92d0442c, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "ir.5p",               0x0800, 0x18d2372d, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "ir.5r",               0x0800, 0x657ddf27, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "ir.1t",               0x0800, 0x64e9e81e, MW_ROM(R_SOUND, 0xc000) | BRF_PRG | BRF_ESS }, //  4 M6802 code
	{ "ir.1u",               0x0800, 0x152fc85e, MW_ROM(R_SOUND, 0xe000) | BRF_PRG | BRF_ESS }, //  5 M6802 code
	{ "ir.3r",               0x0800, 0x6ce639bf, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
};

STD_ROM_PICK(invrvngegw)
STD_ROM_FN(invrvngegw)

struct BurnDriver BurnDrvInvrvngegw = {
	"invrvngegw", "invrvnge", NULL, NULL, "1980?",
	"Invader's Revenge (Game World, single PCB)\0", NULL, "Zenitone-Microsec Ltd. (Game World license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, invrvngegwRomInfo, invrvngegwRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, InvrvngeDIPInfo,
	Init50, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Vortex

static struct BurnRomInfo vortexRomDesc[] = {
	{ "1.t36",               0x0800, 0x577417a6, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2.t35",               0x0800, 0x126d0049, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3.t34",               0x0800, 0x4a2510b3, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4.t33",               0x0800, 0xda0274fe, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5.t32",               0x0800, 0xa3de49d6, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "6.t31",               0x0800, 0x271085d0, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(vortex)
STD_ROM_FN(vortex)

struct BurnDriver BurnDrvVortex = {
	"vortex", NULL, NULL, NULL, "1980",
	"Vortex\0", NULL, "Zilec Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, vortexRomInfo, vortexRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, VortexDIPInfo,
	Init51, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Rolling Crash / Moon Base

static struct BurnRomInfo rollingcRomDesc[] = {
	{ "rc01.a4",             0x0400, 0x66fa50bf, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "rc02.c4",             0x0400, 0x61c06ae4, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "rc03.e4",             0x0400, 0x77e39fa0, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "rc04.f4",             0x0400, 0x3fdfd0f3, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "rc05.h4",             0x0400, 0xc26a8f5b, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "rc06.l4",             0x0400, 0x0b98dbe5, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "rc07.a5",             0x0400, 0x6242145c, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "rc08.c5",             0x0400, 0xd23c2ef1, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "rc09.e4",             0x0800, 0x2e2c5b95, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "rc10.f4",             0x0800, 0xef94c502, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "rc11.h4",             0x0800, 0xa3164b18, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, // 10 i8080 code
	{ "rc12.l4",             0x0800, 0x2052f6d9, MW_ROM(R_MAIN, 0x5800) | BRF_PRG | BRF_ESS }, // 11 i8080 code
};

STD_ROM_PICK(rollingc)
STD_ROM_FN(rollingc)

struct BurnDriver BurnDrvRollingc = {
	"rollingc", NULL, NULL, NULL, "1979",
	"Rolling Crash / Moon Base\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, rollingcRomInfo, rollingcRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, RollingcDIPInfo,
	Init52, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Moon Raker

static struct BurnRomInfo mrakerRomDesc[] = {
	{ "mr_1.a4",             0x0400, 0x0ba47dea, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "mr_2.c4",             0x0400, 0xfead8849, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "mr_3.e4",             0x0400, 0x7d65358f, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "mr_4.f4",             0x0400, 0x22cc55ad, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "mr_5.h4",             0x0400, 0xe8048768, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "mr_6.l4",             0x0400, 0x1e717545, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "mr_7.a5",             0x0400, 0x2f19de73, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "mr_8.c5",             0x0400, 0xfddaf8d0, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "mr_9.e5",             0x0800, 0x1ca1be22, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "mr_10.f5",            0x0800, 0x1901c0d4, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "mr_11.h5",            0x0800, 0xcb8e34ae, MW_ROM(R_MAIN, 0x5000) | BRF_PRG | BRF_ESS }, // 10 i8080 code
	{ "star",                0x0400, 0xc87a42dd, MW_ROM(R_USER, 0x0000) | BRF_GRA }, // 11 misc
};

STD_ROM_PICK(mraker)
STD_ROM_FN(mraker)

struct BurnDriver BurnDrvMraker = {
	"mraker", NULL, NULL, NULL, "1980",
	"Moon Raker\0", NULL, "Nichibutsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, mrakerRomInfo, mrakerRomName, NULL, NULL, NULL, NULL, MrakerInputInfo, MrakerDIPInfo,
	Init53, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Ozma Wars (set 1)

static struct BurnRomInfo ozmawarsRomDesc[] = {
	{ "mw01",                0x0800, 0x31f4397d, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "mw02",                0x0800, 0xd8e77c62, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "mw03",                0x0800, 0x3bfa418f, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "mw04",                0x0800, 0xe190ce6c, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "mw05",                0x0800, 0x3bc7d4c7, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "mw06",                0x0800, 0x99ca2eae, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "01.1",                0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, //  6 color proms
	{ "02.2",                0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, //  7 color proms
};

STD_ROM_PICK(ozmawars)
STD_ROM_FN(ozmawars)

struct BurnDriver BurnDrvOzmawars = {
	"ozmawars", NULL, NULL, "ozmawars", "1979",
	"Ozma Wars (set 1)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ozmawarsRomInfo, ozmawarsRomName, NULL, NULL, OzmawarsSampleInfo, OzmawarsSampleName, InvadersInputInfo, OzmawarsDIPInfo,
	Init54, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Ozma Wars (set 2)

static struct BurnRomInfo ozmawars2RomDesc[] = {
	{ "oz1",                 0x0400, 0x9300830e, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "oz2",                 0x0400, 0x957fc661, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "oz3",                 0x0400, 0xcf8f4d6c, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "oz4",                 0x0400, 0xf51544a5, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "oz5",                 0x0400, 0x5597bf52, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "oz6",                 0x0400, 0x19b43578, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "oz7",                 0x0400, 0xa285bfde, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "oz8",                 0x0400, 0xae59a629, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "oz9",                 0x0400, 0xdf0cc633, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "oz10",                0x0400, 0x31b7692e, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "oz11",                0x0400, 0x660e934c, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, // 10 i8080 code
	{ "oz12",                0x0400, 0x8b969f61, MW_ROM(R_MAIN, 0x4c00) | BRF_PRG | BRF_ESS }, // 11 i8080 code
	{ "01.1",                0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, // 12 color proms
	{ "02.2",                0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, // 13 color proms
};

STD_ROM_PICK(ozmawars2)
STD_ROM_FN(ozmawars2)

struct BurnDriver BurnDrvOzmawars2 = {
	"ozmawars2", "ozmawars", NULL, "ozmawars", "1979",
	"Ozma Wars (set 2)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ozmawars2RomInfo, ozmawars2RomName, NULL, NULL, OzmawarsSampleInfo, OzmawarsSampleName, InvadersInputInfo, OzmawarsDIPInfo,
	Init54, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Ozma Wars (set 3)

static struct BurnRomInfo ozmawars3RomDesc[] = {
	{ "u36",                 0x0400, 0xf71ae28d, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "u35",                 0x0400, 0xab922611, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "u34",                 0x0400, 0x689c5c2b, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "u33",                 0x0400, 0xda0c394a, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "u32",                 0x0400, 0x1980bad9, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "u31",                 0x0400, 0x19b43578, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "u42",                 0x0400, 0xa285bfde, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "u41",                 0x0400, 0xae59a629, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "u40",                 0x0400, 0xdf0cc633, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "u39",                 0x0400, 0x31b7692e, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "u38",                 0x0400, 0x50257351, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, // 10 i8080 code
	{ "u37",                 0x0400, 0x8b969f61, MW_ROM(R_MAIN, 0x4c00) | BRF_PRG | BRF_ESS }, // 11 i8080 code
	{ "82s137.u1",           0x0400, 0xaac24f34, MW_ROM(R_PROM, 0x0000) | BRF_GRA }, // 12 color proms
	{ "82s137.u2",           0x0400, 0x2bdf83a0, MW_ROM(R_PROM, 0x0400) | BRF_GRA }, // 13 color proms
};

STD_ROM_PICK(ozmawars3)
STD_ROM_FN(ozmawars3)

struct BurnDriver BurnDrvOzmawars3 = {
	"ozmawars3", "ozmawars", NULL, "ozmawars", "1979",
	"Ozma Wars (set 3)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ozmawars3RomInfo, ozmawars3RomName, NULL, NULL, OzmawarsSampleInfo, OzmawarsSampleName, InvadersInputInfo, OzmawarsDIPInfo,
	Init54, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Ozma Wars (Model Racing bootleg)

static struct BurnRomInfo ozmawarsmrRomDesc[] = {
	{ "73.1s",               0x0400, 0x9300830e, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "74.1pr",              0x0400, 0x957fc661, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "75.1n",               0x0400, 0xcf8f4d6c, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "76.1m",               0x0400, 0xf51544a5, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "77.1l",               0x0400, 0x4a653fe6, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "78.1kj",              0x0400, 0xfb3db187, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "79.1h",               0x0400, 0xed2d7c34, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "80.1g",               0x0400, 0x85728971, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "81.1ef",              0x0400, 0xdf0cc633, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "82.1d",               0x0400, 0x31b7692e, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "83.1c",               0x0400, 0x50257351, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, // 10 i8080 code
	{ "84.1b",               0x0400, 0x293303c9, MW_ROM(R_MAIN, 0x4c00) | BRF_PRG | BRF_ESS }, // 11 i8080 code
};

STD_ROM_PICK(ozmawarsmr)
STD_ROM_FN(ozmawarsmr)

struct BurnDriver BurnDrvOzmawarsmr = {
	"ozmawarsmr", "ozmawars", NULL, NULL, "1979",
	"Ozma Wars (Model Racing bootleg)\0", NULL, "bootleg (Model Racing)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ozmawarsmrRomInfo, ozmawarsmrRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, OzmawarsDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Space Phantoms (bootleg of Ozma Wars)

static struct BurnRomInfo spacephRomDesc[] = {
	{ "sv01.bin",            0x0400, 0xde84771d, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "sv02.bin",            0x0400, 0x957fc661, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "sv03.bin",            0x0400, 0xdbda38b9, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "sv04.bin",            0x0400, 0xf51544a5, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "sv05.bin",            0x0400, 0x98d02683, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "sv06.bin",            0x0400, 0x4ec390fd, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "sv07.bin",            0x0400, 0x170862fd, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "sv08.bin",            0x0400, 0x511b12cf, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
	{ "sv09.bin",            0x0400, 0xaf1cd1af, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  8 i8080 code
	{ "sv10.bin",            0x0400, 0x31b7692e, MW_ROM(R_MAIN, 0x4400) | BRF_PRG | BRF_ESS }, //  9 i8080 code
	{ "sv11.bin",            0x0400, 0x50257351, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, // 10 i8080 code
	{ "sv12.bin",            0x0400, 0xa2a3366a, MW_ROM(R_MAIN, 0x4c00) | BRF_PRG | BRF_ESS }, // 11 i8080 code
};

STD_ROM_PICK(spaceph)
STD_ROM_FN(spaceph)

struct BurnDriver BurnDrvSpaceph = {
	"spaceph", "ozmawars", NULL, NULL, "1979",
	"Space Phantoms (bootleg of Ozma Wars)\0", NULL, "bootleg? (Zilec Games)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, spacephRomInfo, spacephRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, SpacephDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Solar Fight (bootleg of Ozma Wars)

static struct BurnRomInfo solfightRomDesc[] = {
	{ "solfight.m",          0x0800, 0xa4f2814e, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "solfight.n",          0x0800, 0x5657ec07, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "solfight.p",          0x0800, 0xef9ce96d, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "solfight.r",          0x0800, 0x4f1ef540, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "mw05",                0x0800, 0x3bc7d4c7, MW_ROM(R_MAIN, 0x4000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "solfight.t",          0x0800, 0x3b6fb206, MW_ROM(R_MAIN, 0x4800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(solfight)
STD_ROM_FN(solfight)

struct BurnDriver BurnDrvSolfight = {
	"solfight", "ozmawars", NULL, NULL, "1979",
	"Solar Fight (bootleg of Ozma Wars)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, solfightRomInfo, solfightRomName, NULL, NULL, NULL, NULL, InvadersInputInfo, OzmawarsDIPInfo,
	Init23, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Yosaku to Donbei (set 1)

static struct BurnRomInfo yosakdonRomDesc[] = {
	{ "yd1.bin",             0x0400, 0x607899c9, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "yd2.bin",             0x0400, 0x78336df4, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "yd3.bin",             0x0400, 0xc5af6d52, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "yd4.bin",             0x0400, 0xdca8064f, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "yd5.bin",             0x0400, 0x38804ff1, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "yd6.bin",             0x0400, 0x988d2362, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "yd7.bin",             0x0400, 0x2744e68b, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  6 i8080 code
};

STD_ROM_PICK(yosakdon)
STD_ROM_FN(yosakdon)

struct BurnDriver BurnDrvYosakdon = {
	"yosakdon", NULL, NULL, NULL, "1979",
	"Yosaku to Donbei (set 1)\0", NULL, "Wing", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, yosakdonRomInfo, yosakdonRomName, NULL, NULL, NULL, NULL, YosakdonInputInfo, YosakdonDIPInfo,
	Init55, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Yosaku to Donbei (set 2)

static struct BurnRomInfo yosakdonaRomDesc[] = {
	{ "yosaku1",             0x0400, 0xd132f4f0, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "yd2.bin",             0x0400, 0x78336df4, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "yosaku3",             0x0400, 0xb1a0b3eb, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "yosaku4",             0x0400, 0xc06c225e, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "yosaku5",             0x0400, 0xae422a43, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "yosaku6",             0x0400, 0x26b24a12, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "yosaku7",             0x0400, 0x878d5a18, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  6 i8080 code
};

STD_ROM_PICK(yosakdona)
STD_ROM_FN(yosakdona)

struct BurnDriver BurnDrvYosakdona = {
	"yosakdona", "yosakdon", NULL, NULL, "1979",
	"Yosaku to Donbei (set 2)\0", NULL, "Wing", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, yosakdonaRomInfo, yosakdonaRomName, NULL, NULL, NULL, NULL, YosakdonInputInfo, YosakdonDIPInfo,
	Init55, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Shuttle Invader

static struct BurnRomInfo shuttleiRomDesc[] = {
	{ "1.13c",               0x0400, 0xb6d4f0cd, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2.11c",               0x0400, 0x168d6138, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "3.13d",               0x0400, 0x804bd7fb, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "4.11d",               0x0400, 0x8205b369, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "5.13e",               0x0400, 0xb50df820, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "8.11f",               0x0400, 0x4978552b, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "82s129.2b",           0x0100, 0xf108d00d, 0 | BRF_OPT }, //  6 color proms
};

STD_ROM_PICK(shuttlei)
STD_ROM_FN(shuttlei)

struct BurnDriver BurnDrvShuttlei = {
	"shuttlei", NULL, NULL, NULL, "1979",
	"Shuttle Invader\0", NULL, "Omori Electric Co., Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, shuttleiRomInfo, shuttleiRomName, NULL, NULL, NULL, NULL, ShuttleiInputInfo, ShuttleiDIPInfo,
	Init56, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	192, 256, 3, 4
};

// Sky Love

static struct BurnRomInfo skyloveRomDesc[] = {
	{ "01",                  0x0400, 0x391ad7d0, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "02",                  0x0400, 0x365ba070, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "03",                  0x0400, 0x47364dad, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "04",                  0x0400, 0x9d76f33d, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "05",                  0x0400, 0x09084954, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "06",                  0x0400, 0x6d494e82, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "07",                  0x0400, 0x1a9aa4b8, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "08",                  0x0400, 0xecaacacc, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(skylove)
STD_ROM_FN(skylove)

struct BurnDriver BurnDrvSkylove = {
	"skylove", NULL, NULL, NULL, "1979",
	"Sky Love\0", NULL, "Omori Electric Co., Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, skyloveRomInfo, skyloveRomName, NULL, NULL, NULL, NULL, ShuttleiInputInfo, SkyloveDIPInfo,
	Init56, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	192, 256, 3, 4
};

// Claybuster

static struct BurnRomInfo claybustRomDesc[] = {
	{ "0.a1",                0x0400, 0x90810582, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "1.a2",                0x0400, 0x5ce6fb0e, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "2.a4",                0x0400, 0xd4c1d523, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "3.a5",                0x0400, 0x1ca00825, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "4.a6",                0x0400, 0x09a21120, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "5.a8",                0x0400, 0x92cd4da8, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
};

STD_ROM_PICK(claybust)
STD_ROM_FN(claybust)

struct BurnDriver BurnDrvClaybust = {
	"claybust", NULL, NULL, NULL, "1978",
	"Claybuster\0", NULL, "Model Racing", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, claybustRomInfo, claybustRomName, NULL, NULL, NULL, NULL, ClaybustInputInfo, ClaybustDIPInfo,
	Init57, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Gun Champ

static struct BurnRomInfo gunchampRomDesc[] = {
	{ "251.a1",              0x0400, 0xf27a8c1e, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "252.a2",              0x0400, 0xd53b8f91, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "253.a4",              0x0400, 0x9ef35c6c, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "254.a5",              0x0400, 0xba5b562d, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "255.a6",              0x0400, 0x00ea8293, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "256.a8",              0x0400, 0xe271150c, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "257.a9",              0x0400, 0x0da5d9ad, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "258.a10",             0x0400, 0x471d4052, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(gunchamp)
STD_ROM_FN(gunchamp)

struct BurnDriver BurnDrvGunchamp = {
	"gunchamp", NULL, NULL, NULL, "1980",
	"Gun Champ\0", NULL, "Model Racing", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, gunchampRomInfo, gunchampRomName, NULL, NULL, NULL, NULL, ClaybustInputInfo, GunchampDIPInfo,
	Init58, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Cane (prototype)

static struct BurnRomInfo caneRomDesc[] = {
	{ "mrcane.71",           0x0800, 0x47de691e, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "mrcane.70",           0x0800, 0x3f3ee3b9, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "mrcane.69",           0x0800, 0xd1fd883f, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "mrcane.62",           0x0800, 0x0d37cc00, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  3 i8080 code
};

STD_ROM_PICK(cane)
STD_ROM_FN(cane)

struct BurnDriver BurnDrvCane = {
	"cane", NULL, NULL, NULL, "1979?",
	"Cane (prototype)\0", NULL, "Model Racing", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, caneRomInfo, caneRomName, NULL, NULL, NULL, NULL, CaneInputInfo, CaneDIPInfo,
	Init59, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Orbite (prototype)

static struct BurnRomInfo orbiteRomDesc[] = {
	{ "mrxx.71",             0x0800, 0x78cf0c8a, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "mrxx.70",             0x0800, 0x2914a5c4, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "mrxx.69",             0x0800, 0xc3e464f5, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  2 i8080 code
};

STD_ROM_PICK(orbite)
STD_ROM_FN(orbite)

struct BurnDriver BurnDrvOrbite = {
	"orbite", NULL, NULL, NULL, "1979?",
	"Orbite (prototype)\0", NULL, "Model Racing", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, orbiteRomInfo, orbiteRomName, NULL, NULL, NULL, NULL, OrbiteInputInfo, OrbiteDIPInfo,
	Init60, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	224, 260, 3, 4
};

// Astropal

static struct BurnRomInfo astropalRomDesc[] = {
	{ "2708.0a",             0x0400, 0xe6883322, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "2708.1a",             0x0400, 0x4401df1d, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "2708.2a",             0x0400, 0x5bac1ee4, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "2708.3a",             0x0400, 0xa870afad, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "2708.4a",             0x0400, 0x8bd2d985, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "2708.5a",             0x0400, 0x5e97a86b, MW_ROM(R_MAIN, 0x1400) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "2708.6a",             0x0400, 0x22c354d0, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  6 i8080 code
	{ "2708.7a",             0x0400, 0xaeca51c1, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  7 i8080 code
};

STD_ROM_PICK(astropal)
STD_ROM_FN(astropal)

struct BurnDriver BurnDrvAstropal = {
	"astropal", NULL, NULL, NULL, "1980?",
	"Astropal\0", NULL, "Sidam?", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, astropalRomInfo, astropalRomName, NULL, NULL, NULL, NULL, AstropalInputInfo, AstropalDIPInfo,
	Init61, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Attack Force (encrypted)

static struct BurnRomInfo attackfcRomDesc[] = {
	{ "30a.bin",             0x0400, 0xc12e3386, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "36a.bin",             0x0400, 0x6738dcb9, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "31a.bin",             0x0400, 0x787a4658, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "37a.bin",             0x0400, 0xad6bfbbe, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "32a.bin",             0x0400, 0xcbe0a711, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "33a.bin",             0x0400, 0x53147393, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "39a.bin",             0x0400, 0xf538cf08, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  6 i8080 code
};

STD_ROM_PICK(attackfc)
STD_ROM_FN(attackfc)

struct BurnDriver BurnDrvAttackfc = {
	"attackfc", NULL, NULL, NULL, "1979?",
	"Attack Force (encrypted)\0", NULL, "Electronic Games Systems", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, attackfcRomInfo, attackfcRomName, NULL, NULL, NULL, NULL, AttackfcInputInfo, AttackfcDIPInfo,
	Init62, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

// Attack Force (unencrypted)

static struct BurnRomInfo attackfcuRomDesc[] = {
	{ "egs0.bin",            0x0400, 0x653bbb40, MW_ROM(R_MAIN, 0x0000) | BRF_PRG | BRF_ESS }, //  0 i8080 code
	{ "egs1.bin",            0x0400, 0x56024445, MW_ROM(R_MAIN, 0x0400) | BRF_PRG | BRF_ESS }, //  1 i8080 code
	{ "egs2.bin",            0x0400, 0x0a5fbe34, MW_ROM(R_MAIN, 0x0800) | BRF_PRG | BRF_ESS }, //  2 i8080 code
	{ "egs3.bin",            0x0400, 0x50f7cd22, MW_ROM(R_MAIN, 0x0c00) | BRF_PRG | BRF_ESS }, //  3 i8080 code
	{ "egs4.bin",            0x0400, 0xf59bac9e, MW_ROM(R_MAIN, 0x1000) | BRF_PRG | BRF_ESS }, //  4 i8080 code
	{ "egs6.bin",            0x0400, 0xa9eb4699, MW_ROM(R_MAIN, 0x1800) | BRF_PRG | BRF_ESS }, //  5 i8080 code
	{ "egs7.bin",            0x0400, 0x7e705388, MW_ROM(R_MAIN, 0x1c00) | BRF_PRG | BRF_ESS }, //  6 i8080 code
};

STD_ROM_PICK(attackfcu)
STD_ROM_FN(attackfcu)

struct BurnDriver BurnDrvAttackfcu = {
	"attackfcu", "attackfc", NULL, NULL, "1979?",
	"Attack Force (unencrypted)\0", NULL, "bootleg?", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, attackfcuRomInfo, attackfcuRomName, NULL, NULL, NULL, NULL, AttackfcuInputInfo, AttackfcuDIPInfo,
	Init63, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	260, 224, 4, 3
};

