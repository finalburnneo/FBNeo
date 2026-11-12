// FinalBurn Neo Atari 2600 (VCS) driver module
//
// Useful info:
// Andrew Towers' "TIA Hardware Notes" - https://www.atarihq.com/danb/files/TIA_HW_Notes.txt
// The Stella Programmer's Guide - https://forums.atariage.com/topic/278499-stella-programmers-guide-digital-download-print-on-demand/
// Kevin Horton's bankswitching notes - http://kevtris.org/files/sizes.txt

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "burn_gun.h"
#include "burn_pal.h"
#include <math.h>

enum {
	CART_2K = 0, CART_4K, CART_F8, CART_F6, CART_F4, CART_FA, CART_FE, CART_E0,
	CART_E7, CART_3F, CART_3E, CART_CV, CART_UA, CART_DPC, CART_F0, CART_DPCP, CART_CDF
};

enum { CTRL_JOY = 0, CTRL_PADDLE };

static INT32 cart_type = CART_4K;
static INT32 cart_superchip = 0;		// 128 bytes of Superchip RAM (F8SC/F6SC/F4SC)
static INT32 cart_startbank = -1;		// -1 = mapper default
static INT32 is_pal = 0;
static INT32 ctrl_type = CTRL_JOY;
static INT32 paddle_min = 0x00;
static INT32 paddle_max = 0xff;

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvCartROM;
static UINT8 *DrvRIOTRAM;
static UINT8 *DrvCartRAM;
static UINT8 *DrvFrameBuf;		// 2 x A26_MAXLINES x 160 palette indexes

static UINT32 cart_size;

#define A26_MAXLINES		400
#define A26_W				160
#define A26_NTSC_H			228
#define A26_PAL_H			270
// first line shown: like a TV, a fixed band of lines after VSYNC.  Standard
// games (NTSC: 192 lines from line 40, PAL: 228 from line 48) end up centred.
#define A26_NTSC_Y			22
#define A26_PAL_Y			27
#define A26_SCALE			2		// output is pixel-doubled (both axes)
#define A26_CARTRAM_SIZE	0x8000	// 3E carts can have 32k

static const INT32 ntsc_cpu_clock = 1193182;	// 3579545 / 3
static const INT32 pal_cpu_clock  = 1182298;	// 3546894 / 3

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];		// console switches
static UINT8 DrvJoyP[4][3];	// paddle digital left/right/fire
static INT16 DrvAnalog[4];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static UINT8 input_swcha;		// what the controllers drive onto port A
static UINT8 input_swchb;		// console switches
static UINT8 input_fire[2];		// INPT4/5 bit 7 (1 = released)
static INT32 paddle_lines[4];	// charge time in scanlines

static UINT64 cpu_base;			// absolute CPU cycles at the start of this FBNeo frame
static UINT8 bus_last;			// open bus

static inline UINT64 cpu_now()
{
	return cpu_base + (UINT64)M6502TotalCycles();
}

// TIA
enum { OP0 = 0, OP1, OM0, OM1, OBL };

#define CX_P0	0x01
#define CX_M0	0x02
#define CX_P1	0x04
#define CX_M1	0x08
#define CX_BL	0x10
#define CX_PF	0x20

struct a26_obj {
	INT32 cnt;			// position counter, 0 - 159
	INT32 pend;			// clocks until graphics start (0 = none pending)
	INT32 pend_main;	// pending start is the main copy
	INT32 scan;			// current graphics pixel, -1 = not drawing
	INT32 is_main;		// currently drawing the main copy
	INT32 moving;		// HMOVE "more movement required" latch
};

static a26_obj tia_obj[5];

static UINT8 tia_nusiz[2];
static UINT8 tia_colup0, tia_colup1, tia_colupf, tia_colubk;
static UINT8 tia_ctrlpf;
static UINT8 tia_refp[2];
static UINT8 tia_pf[3];
static UINT8 tia_grp_new[2];
static UINT8 tia_grp_old[2];
static UINT8 tia_enam[2];
static UINT8 tia_enabl_new, tia_enabl_old;
static UINT8 tia_hm[5];
static UINT8 tia_vdelp[2], tia_vdelbl;
static UINT8 tia_resmp[2];
static UINT8 tia_vsync, tia_vblank;
static UINT16 tia_collision;

static INT32 tia_hpos;			// color clock within the line, 0 - 227
static INT32 tia_line;			// line within the current frame
static INT32 tia_line_ext;		// this line has the 8 clock HMOVE extended hblank
static INT32 tia_hmove_latch;
static INT32 tia_hmove_active;
static INT32 tia_hmove_step;
static UINT64 tia_hmove_next;
static INT32 tia_gfx_reg;
static UINT8 tia_gfx_data;
static UINT64 tia_gfx_clk;
static UINT64 tia_res_pending[5];
static INT32 tia_res_any;
static INT32 tia_hmove_pulse;
static UINT64 tia_hmove_pulse_clk;
static UINT64 tia_clk;			// absolute color clock (next to be emulated)
static INT32 tia_pf_cur;

static UINT8 tia_inpt_latch[2];
static INT32 paddle_dumped;
static UINT64 paddle_release;	// cpu cycle the pot capacitors were released

static UINT8 tia_audc[2], tia_audf[2], tia_audv[2];

struct a26_aud {
	INT32 div;
	INT32 p4, p5, p9;
	INT32 c3, c15, c31;
	INT32 out;
};
static a26_aud tia_aud[2];

static INT16 *aud_buf;
static INT32 aud_pos;
#define AUD_BUF_SIZE	2048

// frame handling
static INT32 frame_done;
static INT32 fb_cur, fb_disp;

static UINT16 cx_table[64];

// RIOT
static UINT8 riot_swcha_out, riot_swacnt, riot_swchb_out, riot_swbcnt;
static UINT8 riot_timer_val;
static INT32 riot_timer_shift;
static UINT64 riot_timer_load;
static UINT64 riot_timer_under;
static UINT64 riot_flag_clear;
static INT32 riot_flag_cleared;
static UINT8 riot_pa7_flag;

static INT32 cart_bank;			// 4k bank for F8/F6/F4/FA/FE/UA/DPC/F0, 2k bank for 3F/3E
static INT32 cart_e0[4];
static INT32 cart_e7_bank, cart_e7_ram;
static INT32 cart_3e_ram, cart_3e_rambank;
static INT32 cart_fe_armed;

// DPC (Pitfall II)
static UINT8 dpc_top[8], dpc_bot[8], dpc_flag[8];
static UINT16 dpc_cnt[8];
static UINT8 dpc_music[3];
static UINT8 dpc_rand;
static UINT8 dpc_movamt;
static INT32 dpc_latch62, dpc_latch64;
static UINT64 dpc_last;
static UINT64 dpc_frac;
static const INT32 dpc_osc = 20000;	// DPC music oscillator, Hz

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }

static struct BurnInputInfo A2600InputList[] = {
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Fire",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Fire",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Game Select",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 select"	},
	{"Game Reset",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(A2600)

static struct BurnInputInfo A2600PaddleInputList[] = {
	A("P1 Paddle",		BIT_ANALOG_REL,	&DrvAnalog[0],	"p1 x-axis"	),
	{"P1 Left",			BIT_DIGITAL,	DrvJoyP[0] + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoyP[0] + 1,	"p1 right"	},
	{"P1 Fire",			BIT_DIGITAL,	DrvJoyP[0] + 2,	"p1 fire 1"	},

	A("P2 Paddle",		BIT_ANALOG_REL,	&DrvAnalog[1],	"p2 x-axis"	),
	{"P2 Left",			BIT_DIGITAL,	DrvJoyP[1] + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoyP[1] + 1,	"p2 right"	},
	{"P2 Fire",			BIT_DIGITAL,	DrvJoyP[1] + 2,	"p2 fire 1"	},

	A("P3 Paddle",		BIT_ANALOG_REL,	&DrvAnalog[2],	"p3 x-axis"	),
	{"P3 Left",			BIT_DIGITAL,	DrvJoyP[2] + 0,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoyP[2] + 1,	"p3 right"	},
	{"P3 Fire",			BIT_DIGITAL,	DrvJoyP[2] + 2,	"p3 fire 1"	},

	A("P4 Paddle",		BIT_ANALOG_REL,	&DrvAnalog[3],	"p4 x-axis"	),
	{"P4 Left",			BIT_DIGITAL,	DrvJoyP[3] + 0,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoyP[3] + 1,	"p4 right"	},
	{"P4 Fire",			BIT_DIGITAL,	DrvJoyP[3] + 2,	"p4 fire 1"	},

	{"Game Select",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 select"	},
	{"Game Reset",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(A2600Paddle)

#undef A

#define A2600_DIPS																\
	{0x00, 0xff, 0xff, 0x00, NULL						},						\
																				\
	{0,    0xfe, 0,    2,    "Left Difficulty (P1)"		},						\
	{0x00, 0x01, 0x01, 0x00, "B (Novice)"				},						\
	{0x00, 0x01, 0x01, 0x01, "A (Expert)"				},						\
																				\
	{0,    0xfe, 0,    2,    "Right Difficulty (P2)"	},						\
	{0x00, 0x01, 0x02, 0x00, "B (Novice)"				},						\
	{0x00, 0x01, 0x02, 0x02, "A (Expert)"				},						\
																				\
	{0,    0xfe, 0,    2,    "TV Type"					},						\
	{0x00, 0x01, 0x04, 0x00, "Color"					},						\
	{0x00, 0x01, 0x04, 0x04, "Black & White"			},						\
																				\
	{0,    0xfe, 0,    2,    "Show HMOVE Comb"			},						\
	{0x00, 0x01, 0x08, 0x00, "Yes"						},						\
	{0x00, 0x01, 0x08, 0x08, "No"						},

static struct BurnDIPInfo A2600DIPList[] =
{
	DIP_OFFSET(0x0d)
	A2600_DIPS
};

STDDIPINFO(A2600)

static struct BurnDIPInfo A2600PaddleDIPList[] =
{
	DIP_OFFSET(0x13)
	A2600_DIPS
};

STDDIPINFO(A2600Paddle)

// TIA (audio)
static void tia_audio_tick()
{
	for (INT32 c = 0; c < 2; c++)
	{
		a26_aud *a = &tia_aud[c];

		if (a->div < (tia_audf[c] & 0x1f)) {
			a->div++;
			continue;
		}
		a->div = 0;

		// polynomial counters: 4 bit (x^4+x^3+1), 5 bit (x^5+x^3+1), 9 bit (x^9+x^5+1)
#define AUD_STEP4() a->p4 = (a->p4 >> 1) | ((((a->p4 >> 0) ^ (a->p4 >> 1)) & 1) << 3)
#define AUD_STEP5() a->p5 = (a->p5 >> 1) | ((((a->p5 >> 0) ^ (a->p5 >> 2)) & 1) << 4)
#define AUD_STEP9() a->p9 = (a->p9 >> 1) | ((((a->p9 >> 0) ^ (a->p9 >> 4)) & 1) << 8)

		switch (tia_audc[c] & 0x0f)
		{
			case 0x00:
			case 0x0b:	// set to 1 (volume only)
				a->out = 1;
			break;

			case 0x01:	// 4 bit poly
				AUD_STEP4();
				a->out = a->p4 & 1;
			break;

			case 0x02:	// div 15 -> 4 bit poly
				if (++a->c15 >= 15) { a->c15 = 0; AUD_STEP4(); }
				a->out = a->p4 & 1;
			break;

			case 0x03:	// 5 bit poly -> 4 bit poly
				AUD_STEP5();
				if (a->p5 & 1) AUD_STEP4();
				a->out = a->p4 & 1;
			break;

			case 0x04:
			case 0x05:	// div 2, pure tone
				a->out ^= 1;
			break;

			case 0x06:
			case 0x0a:	// div 31, pure tone (13:18 duty)
				if (++a->c31 >= 31) a->c31 = 0;
				a->out = (a->c31 >= 13) ? 1 : 0;
			break;

			case 0x07:	// 5 bit poly -> div 2
				AUD_STEP5();
				if (a->p5 & 1) a->out ^= 1;
			break;

			case 0x08:	// 9 bit poly, white noise
				AUD_STEP9();
				a->out = a->p9 & 1;
			break;

			case 0x09:	// 5 bit poly
				AUD_STEP5();
				a->out = a->p5 & 1;
			break;

			case 0x0c:
			case 0x0d:	// div 6, pure tone
				if (++a->c3 >= 3) { a->c3 = 0; a->out ^= 1; }
			break;

			case 0x0e:	// div 93, pure tone
				if (++a->c3 >= 3) {
					a->c3 = 0;
					if (++a->c31 >= 31) a->c31 = 0;
				}
				a->out = (a->c31 >= 13) ? 1 : 0;
			break;

			case 0x0f:	// 5 bit poly -> div 6
				AUD_STEP5();
				if (a->p5 & 1) {
					if (++a->c3 >= 3) { a->c3 = 0; a->out ^= 1; }
				}
			break;
		}
#undef AUD_STEP4
#undef AUD_STEP5
#undef AUD_STEP9
	}

	if (aud_pos < AUD_BUF_SIZE) {
		INT32 s = (tia_aud[0].out ? (tia_audv[0] & 0x0f) : 0) + (tia_aud[1].out ? (tia_audv[1] & 0x0f) : 0);
		aud_buf[aud_pos++] = (INT16)(s * 0x300);
	}
}

static void tia_audio_reset()
{
	memset(tia_aud, 0, sizeof(tia_aud));
	for (INT32 c = 0; c < 2; c++) {
		tia_aud[c].p4 = 0x0f;
		tia_aud[c].p5 = 0x1f;
		tia_aud[c].p9 = 0x1ff;
	}
}

// TIA (video)
static void tia_build_tables()
{
	for (INT32 m = 0; m < 64; m++)
	{
		UINT16 c = 0;
		// bit (r*2) = register r bit 7, bit (r*2+1) = register r bit 6
		if ((m & CX_M0) && (m & CX_P1)) c |= 1 << 0;	// CXM0P
		if ((m & CX_M0) && (m & CX_P0)) c |= 1 << 1;
		if ((m & CX_M1) && (m & CX_P0)) c |= 1 << 2;	// CXM1P
		if ((m & CX_M1) && (m & CX_P1)) c |= 1 << 3;
		if ((m & CX_P0) && (m & CX_PF)) c |= 1 << 4;	// CXP0FB
		if ((m & CX_P0) && (m & CX_BL)) c |= 1 << 5;
		if ((m & CX_P1) && (m & CX_PF)) c |= 1 << 6;	// CXP1FB
		if ((m & CX_P1) && (m & CX_BL)) c |= 1 << 7;
		if ((m & CX_M0) && (m & CX_PF)) c |= 1 << 8;	// CXM0FB
		if ((m & CX_M0) && (m & CX_BL)) c |= 1 << 9;
		if ((m & CX_M1) && (m & CX_PF)) c |= 1 << 10;	// CXM1FB
		if ((m & CX_M1) && (m & CX_BL)) c |= 1 << 11;
		if ((m & CX_BL) && (m & CX_PF)) c |= 1 << 12;	// CXBLPF
		if ((m & CX_P0) && (m & CX_P1)) c |= 1 << 14;	// CXPPMM
		if ((m & CX_M0) && (m & CX_M1)) c |= 1 << 15;
		cx_table[m] = c;
	}
}

static inline INT32 player_width(INT32 nus)
{
	nus &= 7;
	return (nus == 5) ? 2 : ((nus == 7) ? 4 : 1);
}

static inline INT32 obj_len(INT32 i)
{
	if (i < 2) return 8 * player_width(tia_nusiz[i]);
	if (i < 4) return 1 << ((tia_nusiz[i - 2] >> 4) & 3);
	return 1 << ((tia_ctrlpf >> 4) & 3);
}

static inline INT32 pf_bit(INT32 idx)
{
	if (idx < 4)  return (tia_pf[0] >> (4 + idx)) & 1;
	if (idx < 12) return (tia_pf[1] >> (11 - idx)) & 1;
	return (tia_pf[2] >> (idx - 12)) & 1;
}

static void obj_tick(INT32 i)
{
	a26_obj *o = &tia_obj[i];

	if (o->scan >= 0) {
		if (++o->scan >= obj_len(i)) o->scan = -1;
	}

	if (o->pend > 0) {
		if (--o->pend == 0) {
			o->scan = 0;
			o->is_main = o->pend_main;
		}
	}

	if (++o->cnt >= 160) o->cnt = 0;

	const INT32 c = o->cnt;

	if (i == OBL) {
		if (c == 0) { o->pend = 2; o->pend_main = 1; }
		return;
	}

	const INT32 nus = tia_nusiz[i & 1] & 7;
	INT32 start = 0, main = 0;

	if (c == 0) {
		start = main = 1;
	} else if (c == 16) {
		start = (nus == 1 || nus == 3);
	} else if (c == 32) {
		start = (nus == 2 || nus == 3 || nus == 6);
	} else if (c == 64) {
		start = (nus == 4 || nus == 6);
	}

	if (start) {
		// player graphics have one clock more latency than missiles/ball,
		// and the stretched (double / quad) players one more again.
		o->pend = (i < 2) ? ((nus == 5 || nus == 7) ? 4 : 3) : 2;
		o->pend_main = main;
	}

	// RESMPx: missile follows the centre of its player's main copy
	if (i < 2 && tia_resmp[i] && o->is_main && o->scan == 4 * player_width(nus)) {
		tia_obj[OM0 + i].cnt = 2;
	}
}

static void frame_end()
{
	UINT8 *fb = DrvFrameBuf + fb_cur * (A26_MAXLINES * A26_W);

	// blank anything below the last line drawn this frame
	if (tia_line < A26_MAXLINES - 1) {
		memset(fb + (tia_line + 1) * A26_W, 0, (A26_MAXLINES - 1 - tia_line) * A26_W);
	}

	fb_disp = fb_cur;
	fb_cur ^= 1;
	tia_line = 0;
	frame_done = 1;
}

static void obj_reset_apply(INT32 i);

static void tia_gfx_apply()
{
	const UINT8 data = tia_gfx_data;

	switch (tia_gfx_reg)
	{
		case 0x1b: // GRP0
			tia_grp_new[0] = data;
			tia_grp_old[1] = tia_grp_new[1];
		break;

		case 0x1c: // GRP1
			tia_grp_new[1] = data;
			tia_grp_old[0] = tia_grp_new[0];
			tia_enabl_old = tia_enabl_new;
		break;

		case 0x0d: tia_pf[0] = data; break;
		case 0x0e: tia_pf[1] = data; break;
		case 0x0f: tia_pf[2] = data; break;

		case 0x1d: tia_enam[0] = data; break;
		case 0x1e: tia_enam[1] = data; break;
		case 0x1f: tia_enabl_new = data; break;

		case 0x20: tia_hm[OP0] = data >> 4; break;
		case 0x21: tia_hm[OP1] = data >> 4; break;
		case 0x22: tia_hm[OM0] = data >> 4; break;
		case 0x23: tia_hm[OM1] = data >> 4; break;
		case 0x24: tia_hm[OBL] = data >> 4; break;
		case 0x2b: memset(tia_hm, 0, sizeof(tia_hm)); break;
	}

	tia_gfx_reg = 0;
}

static void tia_step()
{
	const INT32 h = tia_hpos;

	if (tia_gfx_reg && tia_clk == tia_gfx_clk) tia_gfx_apply();

	if (tia_res_any) {
		tia_res_any = 0;
		for (INT32 i = 0; i < 5; i++) {
			if (tia_res_pending[i]) {
				if (tia_res_pending[i] == tia_clk) {
					obj_reset_apply(i);
					tia_res_pending[i] = 0;
				} else {
					tia_res_any = 1;
				}
			}
		}
	}

	if (h == 68) {
		tia_line_ext = tia_hmove_latch;
	} else if (h == 224) {
		// the extended hblank latch is reset as hblank starts: an HMOVE that
		// lands after this (cycle 74) gives movement without the comb.
		tia_hmove_latch = 0;
	}

	// HMOVE: every 4 clocks the ripple counter is compared against each
	// object's HMxx (as it is *now*); objects that haven't matched yet get
	// an extra clock, which reaches the object counters 3 clocks later.
	// Once the counter has run down it stays at 0, so an object whose HMxx
	// was changed mid-HMOVE (Cosmic Ark's starfield) never matches and keeps
	// getting an extra clock every 4 clocks - those only have an effect
	// during hblank (17 per line).
	if (tia_hmove_active && tia_clk == tia_hmove_next) {
		const INT32 ctr = (tia_hmove_step < 16) ? (15 - tia_hmove_step) : 0;
		INT32 mask = 0, any = 0;
		for (INT32 i = 0; i < 5; i++) {
			if (tia_obj[i].moving) {
				if (ctr == (tia_hm[i] ^ 7)) {
					tia_obj[i].moving = 0;
				} else {
					mask |= 1 << i;
					any = 1;
				}
			}
		}
		tia_hmove_pulse = mask;
		tia_hmove_pulse_clk = tia_clk + 3;
		if (tia_hmove_step < 16) tia_hmove_step++;
		if (tia_hmove_step >= 16 && !any) {
			tia_hmove_active = 0;
		} else {
			tia_hmove_next += 4;
		}
	}

	INT32 pulse = 0;
	if (tia_hmove_pulse && tia_clk == tia_hmove_pulse_clk) {
		pulse = tia_hmove_pulse;
		tia_hmove_pulse = 0;
	}

	if (h >= 68)
	{
		const INT32 x = h - 68;
		UINT8 *dst = DrvFrameBuf + (fb_cur * A26_MAXLINES + tia_line) * A26_W + x;

		if (tia_line_ext && h < 76)
		{
			// HMOVE comb
			*dst = (DrvDips[0] & 0x08) ? (tia_colubk >> 1) : 0;
			if (tia_vblank & 2) *dst = 0;
			for (INT32 i = 0; i < 5; i++) {
				if (pulse & (1 << i)) obj_tick(i);
			}
		}
		else
		{
			if ((x & 3) == 0) {
				INT32 idx = x >> 2;
				if (idx >= 20) idx = (tia_ctrlpf & 1) ? (39 - idx) : (idx - 20);
				tia_pf_cur = pf_bit(idx);
			}

			INT32 m = tia_pf_cur ? CX_PF : 0;

			for (INT32 p = 0; p < 2; p++) {
				const a26_obj *o = &tia_obj[p];
				if (o->scan >= 0) {
					const INT32 bit = o->scan / player_width(tia_nusiz[p]);
					const UINT8 g = tia_vdelp[p] ? tia_grp_old[p] : tia_grp_new[p];
					const INT32 v = (tia_refp[p] & 0x08) ? ((g >> bit) & 1) : ((g >> (7 - bit)) & 1);
					if (v) m |= (p == 0) ? CX_P0 : CX_P1;
				}
				if (tia_obj[OM0 + p].scan >= 0 && (tia_enam[p] & 2) && !(tia_resmp[p] & 2)) {
					m |= (p == 0) ? CX_M0 : CX_M1;
				}
			}

			if (tia_obj[OBL].scan >= 0 && ((tia_vdelbl & 1) ? tia_enabl_old : tia_enabl_new) & 2) {
				m |= CX_BL;
			}

			tia_collision |= cx_table[m];

			UINT8 col;
			const UINT8 pfcol = (tia_ctrlpf & 0x02) ? ((x < 80) ? tia_colup0 : tia_colup1) : tia_colupf;

			if (tia_ctrlpf & 0x04) {	// playfield / ball have priority
				if (m & CX_PF)                col = pfcol;
				else if (m & CX_BL)           col = tia_colupf;
				else if (m & (CX_P0 | CX_M0)) col = tia_colup0;
				else if (m & (CX_P1 | CX_M1)) col = tia_colup1;
				else                          col = tia_colubk;
			} else {
				if (m & (CX_P0 | CX_M0))      col = tia_colup0;
				else if (m & (CX_P1 | CX_M1)) col = tia_colup1;
				else if (m & CX_PF)           col = pfcol;
				else if (m & CX_BL)           col = tia_colupf;
				else                          col = tia_colubk;
			}

			*dst = (tia_vblank & 2) ? 0 : (col >> 1);

			// visible area: every object counter is clocked (HMOVE pulses are absorbed)
			for (INT32 i = 0; i < 5; i++) obj_tick(i);
		}
	}
	else
	{
		for (INT32 i = 0; i < 5; i++) {
			if (pulse & (1 << i)) obj_tick(i);
		}
	}

	if (h == 9 || h == 123) tia_audio_tick();

	tia_clk++;

	if (++tia_hpos >= 228) {
		tia_hpos = 0;
		if (++tia_line >= 380) {
			frame_end();	// game isn't generating VSYNC (yet)
		}
	}
}

static inline void tia_run(UINT64 target)
{
	while (tia_clk < target) tia_step();
}

static void obj_reset(INT32 i)
{
	// the position counter is reset a couple of clocks after the write;
	// an HMOVE extra clock arriving in between is lost (Pitfall II's
	// edge-masking missile relies on this)
	tia_res_pending[i] = tia_clk + 2;
	tia_res_any = 1;
}

static void obj_reset_apply(INT32 i)
{
	a26_obj *o = &tia_obj[i];

	o->cnt = 0;

	if (i == OBL) {			// RESBL starts the ball straight away
		o->pend = 2;
		o->pend_main = 1;
	}
}

static void tia_reset()
{
	memset(tia_obj, 0, sizeof(tia_obj));
	for (INT32 i = 0; i < 5; i++) tia_obj[i].scan = -1;

	tia_nusiz[0] = tia_nusiz[1] = 0;
	tia_colup0 = tia_colup1 = tia_colupf = tia_colubk = 0;
	tia_ctrlpf = 0;
	tia_refp[0] = tia_refp[1] = 0;
	tia_pf[0] = tia_pf[1] = tia_pf[2] = 0;
	tia_grp_new[0] = tia_grp_new[1] = tia_grp_old[0] = tia_grp_old[1] = 0;
	tia_enam[0] = tia_enam[1] = 0;
	tia_enabl_new = tia_enabl_old = 0;
	memset(tia_hm, 0, sizeof(tia_hm));
	tia_vdelp[0] = tia_vdelp[1] = tia_vdelbl = 0;
	tia_resmp[0] = tia_resmp[1] = 0;
	tia_vsync = tia_vblank = 0;
	tia_collision = 0;

	tia_hpos = 0;
	tia_line = 0;
	tia_line_ext = 0;
	tia_hmove_latch = 0;
	tia_hmove_active = 0;
	tia_hmove_step = 0;
	tia_hmove_next = 0;
	tia_gfx_reg = 0;
	tia_gfx_data = 0;
	tia_gfx_clk = 0;
	memset(tia_res_pending, 0, sizeof(tia_res_pending));
	tia_res_any = 0;
	tia_hmove_pulse = 0;
	tia_hmove_pulse_clk = 0;
	tia_clk = 0;
	tia_pf_cur = 0;

	tia_inpt_latch[0] = tia_inpt_latch[1] = 0x80;
	paddle_dumped = 0;
	paddle_release = 0;

	tia_audc[0] = tia_audc[1] = tia_audf[0] = tia_audf[1] = tia_audv[0] = tia_audv[1] = 0;
	tia_audio_reset();

	frame_done = 0;
	fb_cur = 0;
	fb_disp = 1;

	memset(DrvFrameBuf, 0, 2 * A26_MAXLINES * A26_W);
}

static UINT8 tia_read(UINT16 address)
{
	const UINT64 now = cpu_now();
	tia_run(now * 3 + 3);

	const INT32 r = address & 0x0f;
	UINT8 data = 0;

	if (r < 8) {
		const UINT16 c = tia_collision >> (r * 2);
		data = ((c & 1) << 7) | ((c & 2) << 5);
	} else if (r < 12) {
		if (ctrl_type == CTRL_PADDLE && !paddle_dumped) {
			if ((now - paddle_release) >= (UINT64)paddle_lines[r - 8] * 76) data = 0x80;
		}
	} else if (r < 14) {
		data = input_fire[r - 12] << 7;
		if (tia_vblank & 0x40) data &= tia_inpt_latch[r - 12];
	}

	// mix open bus
	return data | (bus_last & ((r < 8) ? 0x3f : 0x7f));
}

static void tia_write(UINT16 address, UINT8 data)
{
	const UINT64 now = cpu_now();
	tia_run(now * 3 + 3);

	switch (address & 0x3f)
	{
		case 0x00: // VSYNC
			if ((data & 2) && !(tia_vsync & 2) && tia_line >= 100) {
				frame_end();
			}
			tia_vsync = data;
		return;

		case 0x01: // VBLANK
			if (data & 0x80) {
				paddle_dumped = 1;
			} else if (paddle_dumped) {
				paddle_dumped = 0;
				paddle_release = now;
			}
			if (!(data & 0x40)) {
				tia_inpt_latch[0] = tia_inpt_latch[1] = 0x80;
			}
			tia_vblank = data;
		return;

		case 0x02: // WSYNC - halt the cpu until the start of the next line
		{
			const INT32 c = (INT32)((now + 1) % 76);
			if (c) m6502_dec_icount(76 - c);
		}
		return;

		case 0x03: // RSYNC (not emulated)
		return;

		case 0x04: tia_nusiz[0] = data; return;
		case 0x05: tia_nusiz[1] = data; return;
		case 0x06: tia_colup0 = data; return;
		case 0x07: tia_colup1 = data; return;
		case 0x08: tia_colupf = data; return;
		case 0x09: tia_colubk = data; return;
		case 0x0a: tia_ctrlpf = data; return;
		case 0x0b: tia_refp[0] = data; return;
		case 0x0c: tia_refp[1] = data; return;
		case 0x0d: // PF0
		case 0x0e: // PF1
		case 0x0f: // PF2
			// the playfield sees writes two clocks later: one landing on the
			// first clock of a playfield pixel doesn't change that pixel yet
			// (Jutland Skies' title)
			tia_gfx_reg = address & 0x3f;
			tia_gfx_data = data;
			tia_gfx_clk = tia_clk + 2;
		return;

		case 0x10: obj_reset(OP0); return;
		case 0x11: obj_reset(OP1); return;
		case 0x12: obj_reset(OM0); return;
		case 0x13: obj_reset(OM1); return;
		case 0x14: obj_reset(OBL); return;

		case 0x15: tia_audc[0] = data & 0x0f; return;
		case 0x16: tia_audc[1] = data & 0x0f; return;
		case 0x17: tia_audf[0] = data & 0x1f; return;
		case 0x18: tia_audf[1] = data & 0x1f; return;
		case 0x19: tia_audv[0] = data & 0x0f; return;
		case 0x1a: tia_audv[1] = data & 0x0f; return;

		case 0x1b: // GRP0
		case 0x1c: // GRP1
		case 0x1d: // ENAM0
		case 0x1e: // ENAM1
		case 0x1f: // ENABL
			// graphics / enable registers reach the object logic one clock
			// later than other writes (a GRPx write for the next player copy
			// mustn't hit the last pixels of the copy still being drawn)
			tia_gfx_reg = address & 0x3f;
			tia_gfx_data = data;
			tia_gfx_clk = tia_clk + 1;
		return;

		case 0x20: // HMP0
		case 0x21: // HMP1
		case 0x22: // HMM0
		case 0x23: // HMM1
		case 0x24: // HMBL
		case 0x2b: // HMCLR
			// like GRPx: reaches the HMOVE comparators one clock later, so a
			// write landing on the last comparison doesn't count for it yet
			// (Donkey Kong Junior), one 3 clocks earlier does (Cosmic Ark)
			tia_gfx_reg = address & 0x3f;
			tia_gfx_data = data;
			tia_gfx_clk = tia_clk + 1;
		return;

		case 0x25: tia_vdelp[0] = data & 1; return;
		case 0x26: tia_vdelp[1] = data & 1; return;
		case 0x27: tia_vdelbl = data & 1; return;

		case 0x28:
		case 0x29: {
			const INT32 p = address & 1;
			if ((tia_resmp[p] & 2) && !(data & 2)) {
				// missile left at the centre of the player
			}
			tia_resmp[p] = data;
		}
		return;

		case 0x2a: // HMOVE
			tia_hmove_latch = 1;
			tia_hmove_active = 1;
			tia_hmove_step = 0;
			// the first comparison comes ~15 clocks after STA HMOVE starts
			// (6 after the write lands), its extra clock 3 clocks after that:
			// STA HMOVE ending on cycle 3 just fits all 15 pulses into the
			// extended hblank, one ending on cycle 72 loses one pulse.
			tia_hmove_next = tia_clk + 6;
			tia_hmove_pulse = 0;
			for (INT32 i = 0; i < 5; i++) tia_obj[i].moving = 1;
		return;


		case 0x2c: // CXCLR
			tia_collision = 0;
		return;
	}
}

// RIOT (6532)
static void riot_reset()
{
	riot_swcha_out = riot_swacnt = riot_swchb_out = riot_swbcnt = 0;
	riot_timer_val = 0xff;
	riot_timer_shift = 10;
	riot_timer_load = 0;
	riot_timer_under = ((UINT64)0x100 << 10) + 1;
	riot_flag_clear = 0;
	riot_flag_cleared = 0;
	riot_pa7_flag = 0;
}

static UINT8 riot_timer_read(UINT64 now)
{
	if (now < riot_timer_under) {
		const UINT64 el = now - riot_timer_load;
		if (el == 0) return riot_timer_val;
		return (UINT8)(riot_timer_val - 1 - ((el - 1) >> riot_timer_shift));
	}

	// counted through zero: continues at 1 clock per count
	return (UINT8)(0xff - (now - riot_timer_under));
}

static inline INT32 riot_timer_flag(UINT64 now)
{
	return (now >= riot_timer_under) && !(riot_flag_cleared && riot_flag_clear >= riot_timer_under);
}

static UINT8 riot_read(UINT16 address)
{
	const UINT64 now = cpu_now();

	if (address & 0x04) {
		if (address & 0x01) {	// TIMINT
			UINT8 r = (riot_timer_flag(now) ? 0x80 : 0) | riot_pa7_flag;
			riot_pa7_flag = 0;
			return r;
		}

		// INTIM
		UINT8 r = riot_timer_read(now);
		if (now > riot_timer_under) {
			riot_flag_cleared = 1;
			riot_flag_clear = now;
		}
		return r;
	}

	switch (address & 3)
	{
		case 0: return (riot_swcha_out & riot_swacnt) | (input_swcha & ~riot_swacnt);
		case 1: return riot_swacnt;
		case 2: return (riot_swchb_out & riot_swbcnt) | (input_swchb & ~riot_swbcnt);
		case 3: return riot_swbcnt;
	}

	return 0;
}

static void riot_write(UINT16 address, UINT8 data)
{
	if (address & 0x04) {
		if (address & 0x10) {
			static const INT32 shifts[4] = { 0, 3, 6, 10 };
			const UINT64 now = cpu_now();
			riot_timer_val = data;
			riot_timer_shift = shifts[address & 3];
			riot_timer_load = now;
			riot_timer_under = now + 1 + ((UINT64)data << riot_timer_shift);
			riot_flag_cleared = 0;
		}
		// else: PA7 edge detect control (not used by anything)
		return;
	}

	switch (address & 3)
	{
		case 0: riot_swcha_out = data; return;
		case 1: riot_swacnt = data; return;
		case 2: riot_swchb_out = data; return;
		case 3: riot_swbcnt = data; return;
	}
}

// DPC (David Crane's Pitfall II coprocessor)
static inline void dpc_check_flag(INT32 i)
{
	const INT32 low = dpc_cnt[i] & 0xff;
	if (low == dpc_top[i]) dpc_flag[i] = 0xff;
	if (low == dpc_bot[i]) dpc_flag[i] = 0x00;
}

static void dpc_decrement(INT32 i)
{
	INT32 low = (dpc_cnt[i] & 0xff) - 1;
	INT32 high = dpc_cnt[i] >> 8;

	if (low < 0) {
		low = 0xff;
		high = (high - 1) & 7;
		if (i > 4 && (dpc_music[i - 5] & 0x10)) low = dpc_top[i];
	}

	dpc_cnt[i] = (high << 8) | low;
	dpc_check_flag(i);
}

static void dpc_update_osc()
{
	const UINT64 now = cpu_now();
	const INT32 clock = is_pal ? pal_cpu_clock : ntsc_cpu_clock;

	if (now <= dpc_last) return;

	const UINT64 acc = (now - dpc_last) * dpc_osc + dpc_frac;
	dpc_last = now;

	UINT64 ticks = acc / clock;
	dpc_frac = acc % clock;

	for (INT32 i = 5; i < 8; i++)
	{
		const UINT8 mode = dpc_music[i - 5];
		if (!(mode & 0x20)) continue;	// not clocked by the oscillator

		UINT64 n = ticks;
		if ((mode & 0x10) && n > (UINT64)(dpc_top[i] + 1) * 2) {
			// music mode is periodic: skip whole periods
			n = (n % (dpc_top[i] + 1)) + (dpc_top[i] + 1);
		} else if (n > 0x1000) {
			n = (n & 0x7ff) + 0x800;
		}

		while (n--) dpc_decrement(i);
	}
}

static UINT8 dpc_read(INT32 offset)
{
	const INT32 idx = offset & 7;

	if (offset < 0x08)
	{
		if (offset < 4) {
			// random number generator (8 bit xnor lfsr, taps 7,5,4,3)
			const INT32 r = dpc_rand;
			dpc_rand = (UINT8)((r << 1) | (~((r >> 7) ^ (r >> 5) ^ (r >> 4) ^ (r >> 3)) & 1));
			return dpc_rand;
		}

		static const UINT8 amp[8] = { 0x00, 0x04, 0x05, 0x09, 0x06, 0x0a, 0x0b, 0x0f };

		dpc_update_osc();

		if (offset < 6) dpc_latch62 = dpc_latch64;	// with draw line add
		const INT32 sum = dpc_latch62 + dpc_top[4];
		dpc_latch64 = sum & 0xff;
		const INT32 dlc = (sum > 0xff);

		INT32 v = 0;
		if ((dpc_music[0] & 0x10) && dpc_flag[5]) v |= 1;
		if ((dpc_music[1] & 0x10) && dpc_flag[6]) v |= 2;
		if ((dpc_music[2] & 0x10) && dpc_flag[7]) v |= 4;

		return (dlc ? (dpc_movamt & 0xf0) : 0) | amp[v];
	}

	if (idx > 4) dpc_update_osc();

	const UINT8 d = DrvCartROM[0x2000 + (~dpc_cnt[idx] & 0x7ff)];
	const INT32 f = dpc_flag[idx];
	UINT8 data = 0;

	switch (offset & 0x38)
	{
		case 0x08: data = d; break;
		case 0x10: data = f ? d : 0; break;
		case 0x18: data = f ? (UINT8)((d << 4) | (d >> 4)) : 0; break;
		case 0x20: {
			UINT8 r = 0;
			for (INT32 b = 0; b < 8; b++) if (d & (1 << b)) r |= 0x80 >> b;
			data = f ? r : 0;
		}
		break;
		case 0x28: data = f ? (d >> 1) : 0; break;
		case 0x30: data = f ? (UINT8)(d << 1) : 0; break;
		case 0x38: data = f ? 0xff : 0; break;
	}

	if (idx < 5 || !(dpc_music[idx - 5] & 0x20)) {
		dpc_decrement(idx);
	}

	return data;
}

static void dpc_write(INT32 offset, UINT8 data)
{
	const INT32 idx = offset & 7;

	if (idx > 4) dpc_update_osc();

	switch (offset & 0x38)
	{
		case 0x00:	// top count
			dpc_top[idx] = data;
			dpc_flag[idx] = 0;
			dpc_check_flag(idx);
		break;

		case 0x08:	// bottom count
			dpc_bot[idx] = data;
			dpc_check_flag(idx);
		break;

		case 0x10:	// counter low
			if (idx == 4) dpc_latch64 = data;
			if (idx > 4 && (dpc_music[idx - 5] & 0x10)) data = dpc_top[idx];
			dpc_cnt[idx] = (dpc_cnt[idx] & 0x700) | data;
			dpc_check_flag(idx);
		break;

		case 0x18:	// counter high, music mode / oscillator enable for 5-7
			dpc_cnt[idx] = ((data & 7) << 8) | (dpc_cnt[idx] & 0xff);
			if (idx > 4) {
				dpc_music[idx - 5] = data & 0x30;
				if ((data & 0x10) && (dpc_cnt[idx] & 0xff) == 0xff) {
					dpc_cnt[idx] = (dpc_cnt[idx] & 0x700) | dpc_top[idx];
					dpc_check_flag(idx);
				}
			}
		break;

		case 0x20:	// draw line movement value
			dpc_movamt = data;
		break;

		case 0x30:	// random number generator reset
			dpc_rand = 0;
		break;
	}
}

// Harmony / Melody ARM cartridges (DPC+, CDF, CDFJ)
//
// The cart has an LPC2103 (ARM7TDMI): 32k flash at $00000000 holding the ARM
// driver, the game's own Thumb code and the 6507 banks, plus 8k of SRAM at
// $40000000 which holds the display data the 6507 reads through the data
// fetchers / data streams.  The ARM driver is emulated natively, the game's
// own Thumb code (CALLFUNCTION / CALLFN 254 or 255) is interpreted, and runs
// in zero 6507 time.
//
// Info: the DPC+ and CDFJ programmer's guides by Darrell Spice Jr. and
// Fred Quimby; ARM7TDMI Technical Reference Manual (Thumb instruction set).

#define HARM_RAM_SIZE		0x2000

static UINT8 *harm_ram;			// = DrvCartRAM

// ---- Thumb interpreter ----
static UINT32 thumb_r[16];
static UINT32 thumb_n, thumb_z, thumb_c, thumb_v;	// flags, 0 or 1
static UINT32 thumb_exit;		// the LR the driver called the game's code with
static UINT32 thumb_cb[4];		// driver callback entry points (CDF), 0 = none
static UINT64 thumb_icount;		// for the T1TC timer
static INT32 thumb_trace = 0;		// log problems with the game's ARM code


static UINT32 thumb_read(UINT32 a, INT32 size)
{
	const UINT8 *p = NULL;

	if (a < cart_size) {
		p = DrvCartROM + a;
		if (a + size > cart_size) return 0;
	} else if ((a & 0xfff00000) == 0x40000000) {
		a &= 0xfffff;
		if (a + size > HARM_RAM_SIZE) return 0;
		p = harm_ram + a;
	} else {
		if (a == 0xe0008008) return (UINT32)thumb_icount;	// T1TC
		return 0;
	}

	switch (size) {
		case 1: return p[0];
		case 2: return p[0] | (p[1] << 8);
	}
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((UINT32)p[3] << 24);
}

static void thumb_write(UINT32 a, UINT32 d, INT32 size)
{
	if ((a & 0xfff00000) != 0x40000000) return;	// flash, peripherals: ignored
	a &= 0xfffff;
	if (a + size > HARM_RAM_SIZE) return;

	UINT8 *p = harm_ram + a;
	p[0] = d;
	if (size > 1) p[1] = d >> 8;
	if (size > 2) { p[2] = d >> 16; p[3] = d >> 24; }
}

static inline UINT32 thumb_read32(UINT32 a)
{
	// unaligned word loads rotate on the ARM7
	const UINT32 d = thumb_read(a & ~3, 4);
	const INT32 rot = (a & 3) * 8;
	return rot ? ((d >> rot) | (d << (32 - rot))) : d;
}

static inline void thumb_nz(UINT32 r) { thumb_n = r >> 31; thumb_z = (r == 0); }

static inline UINT32 thumb_add(UINT32 a, UINT32 b, UINT32 cin)
{
	const UINT64 r64 = (UINT64)a + b + cin;
	const UINT32 r = (UINT32)r64;
	thumb_nz(r);
	thumb_c = (UINT32)(r64 >> 32);
	thumb_v = ((a ^ r) & (b ^ r)) >> 31;
	return r;
}

static inline UINT32 thumb_sub(UINT32 a, UINT32 b) { return thumb_add(a, ~b, 1); }

static UINT32 thumb_shift(INT32 type, UINT32 v, UINT32 n)	// register specified amounts
{
	n &= 0xff;
	if (n == 0) return v;

	switch (type)
	{
		case 0:	// lsl
			if (n < 32) { thumb_c = (v >> (32 - n)) & 1; return v << n; }
			thumb_c = (n == 32) ? (v & 1) : 0;
			return 0;

		case 1:	// lsr
			if (n < 32) { thumb_c = (v >> (n - 1)) & 1; return v >> n; }
			thumb_c = (n == 32) ? (v >> 31) : 0;
			return 0;

		case 2:	// asr
			if (n < 32) { thumb_c = (v >> (n - 1)) & 1; return (UINT32)((INT32)v >> n); }
			thumb_c = v >> 31;
			return (UINT32)((INT32)v >> 31);

		case 3:	// ror
			n &= 31;
			if (n) v = (v >> n) | (v << (32 - n));
			thumb_c = v >> 31;
			return v;
	}
	return v;
}

static UINT32 harm_callback(INT32 func, UINT32 a, UINT32 b);

// returns 0 when the game's code returned to the driver
static INT32 thumb_branch_x(UINT32 target)
{
	if (target & 1) {
		thumb_r[15] = target & ~1;
		return 1;
	}

	// switch to ARM state: a driver function, or the return to the driver
	for (INT32 i = 0; i < 4; i++) {
		if (thumb_cb[i] && target == thumb_cb[i]) {
			const UINT32 res = harm_callback(i, thumb_r[2], thumb_r[3]);
			if (i == 2) thumb_r[2] = res;
			return thumb_branch_x(thumb_r[14] | 1);	// the driver returns with "orr r4, lr, #1; bx r4"
		}
	}

	if (thumb_trace && target != thumb_exit) {
		bprintf(0, _T("a2600: thumb: unhandled branch to arm code at %x\n"), target);
	}

	return 0;
}

static void thumb_run(UINT32 entry, UINT32 lr, UINT32 sp)
{
	memset(thumb_r, 0, sizeof(thumb_r));
	thumb_n = thumb_z = thumb_c = thumb_v = 0;
	thumb_r[13] = sp;
	thumb_r[14] = lr;
	thumb_r[15] = entry;
	thumb_exit = lr;

	UINT32 *r = thumb_r;

	for (INT32 count = 0; count < 20000000; count++)
	{
		const UINT32 pc = r[15];
		const UINT32 op = thumb_read(pc, 2);
		const UINT32 pcr = pc + 4;	// pc as read by instructions
		r[15] = pc + 2;
		thumb_icount++;

		const INT32 rd = op & 7;
		const INT32 rs = (op >> 3) & 7;

		switch (op >> 11)
		{
			case 0x00:	// lsl #
			case 0x01:	// lsr #
			case 0x02:	// asr #
			{
				const INT32 type = op >> 11;
				UINT32 n = (op >> 6) & 0x1f;
				UINT32 v = r[rs];
				if (n == 0) {
					if (type == 0) { r[rd] = v; thumb_nz(v); break; }
					n = 32;
				}
				r[rd] = thumb_shift(type, v, n);
				thumb_nz(r[rd]);
			}
			break;

			case 0x03:	// add / sub, register or 3 bit immediate
			{
				const UINT32 b = (op & 0x400) ? ((op >> 6) & 7) : r[(op >> 6) & 7];
				r[rd] = (op & 0x200) ? thumb_sub(r[rs], b) : thumb_add(r[rs], b, 0);
			}
			break;

			case 0x04:	// mov #
				r[(op >> 8) & 7] = op & 0xff;
				thumb_nz(op & 0xff);
			break;

			case 0x05:	// cmp #
				thumb_sub(r[(op >> 8) & 7], op & 0xff);
			break;

			case 0x06:	// add #
				r[(op >> 8) & 7] = thumb_add(r[(op >> 8) & 7], op & 0xff, 0);
			break;

			case 0x07:	// sub #
				r[(op >> 8) & 7] = thumb_sub(r[(op >> 8) & 7], op & 0xff);
			break;

			case 0x08:
				if ((op & 0x400) == 0)	// alu operations
				{
					const UINT32 a = r[rd], b = r[rs];
					switch ((op >> 6) & 0xf)
					{
						case 0x0: r[rd] = a & b; thumb_nz(r[rd]); break;			// and
						case 0x1: r[rd] = a ^ b; thumb_nz(r[rd]); break;			// eor
						case 0x2: r[rd] = thumb_shift(0, a, b); thumb_nz(r[rd]); break;	// lsl
						case 0x3: r[rd] = thumb_shift(1, a, b); thumb_nz(r[rd]); break;	// lsr
						case 0x4: r[rd] = thumb_shift(2, a, b); thumb_nz(r[rd]); break;	// asr
						case 0x5: r[rd] = thumb_add(a, b, thumb_c); break;			// adc
						case 0x6: r[rd] = thumb_add(a, ~b, thumb_c); break;			// sbc
						case 0x7: r[rd] = thumb_shift(3, a, b); thumb_nz(r[rd]); break;	// ror
						case 0x8: thumb_nz(a & b); break;							// tst
						case 0x9: r[rd] = thumb_sub(0, b); break;					// neg
						case 0xa: thumb_sub(a, b); break;							// cmp
						case 0xb: thumb_add(a, b, 0); break;						// cmn
						case 0xc: r[rd] = a | b; thumb_nz(r[rd]); break;			// orr
						case 0xd: r[rd] = a * b; thumb_nz(r[rd]); break;			// mul
						case 0xe: r[rd] = a & ~b; thumb_nz(r[rd]); break;			// bic
						case 0xf: r[rd] = ~b; thumb_nz(r[rd]); break;				// mvn
					}
				}
				else	// hi register operations, bx
				{
					const INT32 hd = rd | ((op >> 4) & 8);
					const INT32 hs = (op >> 3) & 0xf;
					const UINT32 b = (hs == 15) ? pcr : r[hs];
					const UINT32 a = (hd == 15) ? pcr : r[hd];

					switch ((op >> 8) & 3)
					{
						case 0:	// add
							if (hd == 15) r[15] = (a + b) & ~1;
							else r[hd] = a + b;
						break;

						case 1:	// cmp
							thumb_sub(a, b);
						break;

						case 2:	// mov
							if (hd == 15) r[15] = b & ~1;
							else r[hd] = b;
						break;

						case 3:	// bx, blx
							if (op & 0x80) r[14] = (pc + 2) | 1;
							if (!thumb_branch_x(b)) return;
						break;
					}
				}
			break;

			case 0x09:	// ldr rd, [pc, #]
				r[(op >> 8) & 7] = thumb_read32((pcr & ~3) + ((op & 0xff) << 2));
			break;

			case 0x0a:
			case 0x0b:	// load / store with register offset
			{
				const UINT32 addr = r[rs] + r[(op >> 6) & 7];
				switch ((op >> 9) & 7)
				{
					case 0: thumb_write(addr & ~3, r[rd], 4); break;					// str
					case 1: thumb_write(addr & ~1, r[rd], 2); break;					// strh
					case 2: thumb_write(addr, r[rd], 1); break;							// strb
					case 3: r[rd] = (INT8)thumb_read(addr, 1); break;					// ldsb
					case 4: r[rd] = thumb_read32(addr); break;							// ldr
					case 5: r[rd] = thumb_read(addr & ~1, 2); break;					// ldrh
					case 6: r[rd] = thumb_read(addr, 1); break;							// ldrb
					case 7: r[rd] = (INT16)thumb_read(addr & ~1, 2); break;				// ldsh
				}
			}
			break;

			case 0x0c:	// str rd, [rb, #]
				thumb_write((r[rs] + ((op >> 4) & 0x7c)) & ~3, r[rd], 4);
			break;

			case 0x0d:	// ldr rd, [rb, #]
				r[rd] = thumb_read32(r[rs] + ((op >> 4) & 0x7c));
			break;

			case 0x0e:	// strb
				thumb_write(r[rs] + ((op >> 6) & 0x1f), r[rd], 1);
			break;

			case 0x0f:	// ldrb
				r[rd] = thumb_read(r[rs] + ((op >> 6) & 0x1f), 1);
			break;

			case 0x10:	// strh
				thumb_write((r[rs] + ((op >> 5) & 0x3e)) & ~1, r[rd], 2);
			break;

			case 0x11:	// ldrh
				r[rd] = thumb_read((r[rs] + ((op >> 5) & 0x3e)) & ~1, 2);
			break;

			case 0x12:	// str rd, [sp, #]
				thumb_write((r[13] + ((op & 0xff) << 2)) & ~3, r[(op >> 8) & 7], 4);
			break;

			case 0x13:	// ldr rd, [sp, #]
				r[(op >> 8) & 7] = thumb_read32(r[13] + ((op & 0xff) << 2));
			break;

			case 0x14:	// add rd, pc, #
				r[(op >> 8) & 7] = (pcr & ~3) + ((op & 0xff) << 2);
			break;

			case 0x15:	// add rd, sp, #
				r[(op >> 8) & 7] = r[13] + ((op & 0xff) << 2);
			break;

			case 0x16:
			case 0x17:
				if ((op & 0x0f00) == 0x0000) {	// add / sub sp, #
					if (op & 0x80) r[13] -= (op & 0x7f) << 2;
					else r[13] += (op & 0x7f) << 2;
				}
				else if ((op & 0x0600) == 0x0400) {	// push / pop
					if (op & 0x0800) {	// pop
						UINT32 sp = r[13];
						for (INT32 i = 0; i < 8; i++) {
							if (op & (1 << i)) { r[i] = thumb_read(sp, 4); sp += 4; }
						}
						if (op & 0x100) {
							const UINT32 t = thumb_read(sp, 4); sp += 4;
							r[13] = sp;
							if (!thumb_branch_x(t | 1)) return;	// v4T: pop pc stays in thumb state
						}
						r[13] = sp;
					} else {	// push
						INT32 n = (op & 0x100) ? 1 : 0;
						for (INT32 i = 0; i < 8; i++) if (op & (1 << i)) n++;
						UINT32 sp = r[13] - n * 4;
						r[13] = sp;
						for (INT32 i = 0; i < 8; i++) {
							if (op & (1 << i)) { thumb_write(sp, r[i], 4); sp += 4; }
						}
						if (op & 0x100) thumb_write(sp, r[14], 4);
					}
				}
				else {
					if (thumb_trace) bprintf(0, _T("a2600: thumb: undefined %04x at %x\n"), op, pc);
					return;
				}
			break;

			case 0x18:	// stmia rb!, {rlist}
			{
				const INT32 rb = (op >> 8) & 7;
				UINT32 addr = r[rb] & ~3;
				for (INT32 i = 0; i < 8; i++) {
					if (op & (1 << i)) { thumb_write(addr, r[i], 4); addr += 4; }
				}
				r[rb] = addr;
			}
			break;

			case 0x19:	// ldmia rb!, {rlist}
			{
				const INT32 rb = (op >> 8) & 7;
				UINT32 addr = r[rb] & ~3;
				for (INT32 i = 0; i < 8; i++) {
					if (op & (1 << i)) { r[i] = thumb_read(addr, 4); addr += 4; }
				}
				if (!(op & (1 << rb))) r[rb] = addr;
			}
			break;

			case 0x1a:
			case 0x1b:	// conditional branch, swi
			{
				INT32 take = 0;
				switch ((op >> 8) & 0xf)
				{
					case 0x0: take = thumb_z; break;
					case 0x1: take = !thumb_z; break;
					case 0x2: take = thumb_c; break;
					case 0x3: take = !thumb_c; break;
					case 0x4: take = thumb_n; break;
					case 0x5: take = !thumb_n; break;
					case 0x6: take = thumb_v; break;
					case 0x7: take = !thumb_v; break;
					case 0x8: take = thumb_c && !thumb_z; break;
					case 0x9: take = !thumb_c || thumb_z; break;
					case 0xa: take = (thumb_n == thumb_v); break;
					case 0xb: take = (thumb_n != thumb_v); break;
					case 0xc: take = !thumb_z && (thumb_n == thumb_v); break;
					case 0xd: take = thumb_z || (thumb_n != thumb_v); break;
					default:
						if (thumb_trace) bprintf(0, _T("a2600: thumb: swi/undefined %04x at %x\n"), op, pc);
						return;
				}
				if (take) r[15] = pcr + ((INT32)(INT8)(op & 0xff) << 1);
			}
			break;

			case 0x1c:	// b
				r[15] = pcr + (((INT32)(op << 21)) >> 20);
			break;

			case 0x1e:	// bl, high part
				r[14] = pcr + (((INT32)(op << 21)) >> 9);
			break;

			case 0x1f:	// bl, low part
			{
				const UINT32 t = r[14] + ((op & 0x7ff) << 1);
				r[14] = (pc + 2) | 1;
				r[15] = t & ~1;
			}
			break;

			default:	// 0x1d: blx suffix (v5 only)
				if (thumb_trace) bprintf(0, _T("a2600: thumb: undefined %04x at %x\n"), op, pc);
			return;
		}
	}

	bprintf(0, _T("a2600: thumb: the game's arm code did not return\n"));
}

// ---- state shared by DPC+ and CDF ----
static UINT8 *harm_prog;			// 6507 banks
static UINT8 *harm_disp;			// display data (in harm_ram)
static UINT32 harm_mus_cnt[3];
static UINT32 harm_mus_freq[3];
static UINT64 harm_mus_last;		// cpu cycle of the last music update
static UINT64 harm_mus_frac;
static INT32 harm_lda_imm;			// DPC+: last ROM byte read was $a9
static UINT32 harm_lda_addr;		// CDF: address of the LDA # operand, 0 = none
static UINT64 harm_lda_cyc;			// CDF: cpu cycle the LDA # opcode was read

static void harm_update_music()
{
	const UINT64 now = cpu_now();
	const INT32 clock = is_pal ? pal_cpu_clock : ntsc_cpu_clock;

	if (now <= harm_mus_last) return;

	// 20kHz oscillator
	const UINT64 acc = (now - harm_mus_last) * 20000 + harm_mus_frac;
	harm_mus_last = now;
	const UINT32 clocks = (UINT32)(acc / clock);
	harm_mus_frac = acc % clock;

	for (INT32 i = 0; i < 3; i++) {
		harm_mus_cnt[i] += harm_mus_freq[i] * clocks;
	}
}

// ---- DPC+ ----
// 32k: 3k ARM driver, 6 x 4k 6507 banks, 4k display data, 1k frequency table.
// RAM: 3k for the driver / game's C variables, then 4k display + 1k frequency.
static UINT16 dpcp_cnt[8];			// data fetcher pointers (12 bit)
static UINT32 dpcp_frac[8];			// fractional pointers (12.8)
static UINT8 dpcp_finc[8];
static UINT8 dpcp_top[8], dpcp_bot[8];
static UINT8 dpcp_fastfetch;
static UINT8 dpcp_param[8];
static INT32 dpcp_param_ptr;
static UINT32 dpcp_rand;
static UINT8 dpcp_wave[3];
static UINT32 dpcp_fraclow_mask;	// what a DFxFRACLOW write keeps

// md5 of the 3k ARM driver, to tell the DPC+ driver versions apart
static void harm_md5(const UINT8 *msg, UINT32 len, UINT8 *out)
{
	static const UINT32 k[64] = {
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };
	static const UINT8 r[64] = {
		7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
		4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };

	UINT32 h[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
	const UINT32 total = ((len + 8) / 64 + 1) * 64;

	for (UINT32 off = 0; off < total; off += 64)
	{
		UINT32 w[16];
		for (INT32 i = 0; i < 64; i++) {	// message, 0x80, zeroes, bit length
			const UINT32 pos = off + i;
			UINT8 b = 0;
			if (pos < len) b = msg[pos];
			else if (pos == len) b = 0x80;
			else if (pos >= total - 8) b = (UINT8)(((UINT64)len * 8) >> ((pos - (total - 8)) * 8));
			if ((i & 3) == 0) w[i >> 2] = 0;
			w[i >> 2] |= b << ((i & 3) * 8);
		}

		UINT32 a = h[0], b = h[1], c = h[2], d = h[3];
		for (INT32 i = 0; i < 64; i++) {
			UINT32 f, g;
			if (i < 16)      { f = (b & c) | (~b & d); g = i; }
			else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
			else if (i < 48) { f = b ^ c ^ d;          g = (3 * i + 5) & 15; }
			else             { f = c ^ (b | ~d);       g = (7 * i) & 15; }
			const UINT32 t = d;
			d = c; c = b;
			const UINT32 x = a + f + k[i] + w[g];
			b = b + ((x << r[i]) | (x >> (32 - r[i])));
			a = t;
		}
		h[0] += a; h[1] += b; h[2] += c; h[3] += d;
	}

	for (INT32 i = 0; i < 16; i++) out[i] = h[i >> 2] >> ((i & 3) * 8);
}

static void dpcp_reset()
{
	harm_prog = DrvCartROM + 0x0c00;
	harm_disp = harm_ram + 0x0c00;
	memcpy(harm_disp, DrvCartROM + 0x6c00, 0x1400);	// display data + frequency table

	memset(dpcp_cnt, 0, sizeof(dpcp_cnt));
	memset(dpcp_frac, 0, sizeof(dpcp_frac));
	memset(dpcp_finc, 0, sizeof(dpcp_finc));
	memset(dpcp_top, 0, sizeof(dpcp_top));
	memset(dpcp_bot, 0, sizeof(dpcp_bot));
	memset(dpcp_param, 0, sizeof(dpcp_param));
	memset(dpcp_wave, 0, sizeof(dpcp_wave));
	dpcp_fastfetch = 0;
	dpcp_param_ptr = 0;
	dpcp_rand = 0x2b435044;	// "DPC+"

	// two of the known driver versions ("stable") clear the fraction on a
	// DFxFRACLOW write, the others ("jitter") keep it
	static const UINT8 stable[2][16] = {
		{ 0x5f, 0x80, 0xb5, 0xa5, 0xad, 0xbe, 0x48, 0x3a, 0xdd, 0xc3, 0xf6, 0xe6, 0xf1, 0xb4, 0x72, 0xf8 },
		{ 0x8d, 0xd7, 0x3b, 0x44, 0xfd, 0x11, 0xc4, 0x88, 0x32, 0x6c, 0xe5, 0x07, 0xcb, 0xeb, 0x19, 0xd1 } };
	UINT8 md5[16];
	harm_md5(DrvCartROM, 0x0c00, md5);
	dpcp_fraclow_mask = 0x0f00ff;
	if (!memcmp(md5, stable[0], 16) || !memcmp(md5, stable[1], 16)) dpcp_fraclow_mask = 0x0f0000;
}

static void dpcp_callfunction(UINT8 data)
{
	switch (data)
	{
		case 0:	// parameter pointer reset
			dpcp_param_ptr = 0;
		break;

		case 1:	// copy rom to a fetcher (source is an offset from the start of the 6507 banks)
		{
			const UINT32 src = (dpcp_param[1] << 8) | dpcp_param[0];
			for (INT32 i = 0; i < dpcp_param[3]; i++) {
				harm_disp[(dpcp_cnt[dpcp_param[2] & 7] + i) & 0xfff] = harm_prog[(src + i) % (cart_size - 0x0c00)];
			}
			dpcp_param_ptr = 0;
		}
		break;

		case 2:	// fill a fetcher with a value
			for (INT32 i = 0; i < dpcp_param[3]; i++) {
				harm_disp[(dpcp_cnt[dpcp_param[2] & 7] + i) & 0xfff] = dpcp_param[0];
			}
			dpcp_param_ptr = 0;
		break;

		case 254:
		case 255:	// run the game's ARM code
			thumb_run(0x0c08, 0x0c00, 0x40001ffc);
		break;
	}
}

static UINT8 dpcp_read_reg(INT32 off)
{
	const INT32 idx = off & 7;
	const UINT8 flag = (((dpcp_top[idx] - (dpcp_cnt[idx] & 0xff)) & 0xff) > ((dpcp_top[idx] - dpcp_bot[idx]) & 0xff)) ? 0xff : 0;
	UINT8 data = 0;

	switch (off >> 3)
	{
		case 0:
			switch (idx)
			{
				case 0:	// RANDOM0NEXT
					dpcp_rand = ((dpcp_rand & (1 << 10)) ? 0x10adab1e : 0) ^ ((dpcp_rand >> 11) | (dpcp_rand << 21));
					data = dpcp_rand;
				break;

				case 1:	// RANDOM0PRIOR
					if (dpcp_rand & (1u << 31)) {
						const UINT32 t = dpcp_rand ^ 0x10adab1e;
						dpcp_rand = (t << 11) | (t >> 21);
					} else {
						dpcp_rand = (dpcp_rand << 11) | (dpcp_rand >> 21);
					}
					data = dpcp_rand;
				break;

				case 2: data = dpcp_rand >> 8; break;
				case 3: data = dpcp_rand >> 16; break;
				case 4: data = dpcp_rand >> 24; break;

				case 5:	// AMPLITUDE
					harm_update_music();
					data = harm_disp[((dpcp_wave[0] << 5) + (harm_mus_cnt[0] >> 27)) & 0xfff]
						 + harm_disp[((dpcp_wave[1] << 5) + (harm_mus_cnt[1] >> 27)) & 0xfff]
						 + harm_disp[((dpcp_wave[2] << 5) + (harm_mus_cnt[2] >> 27)) & 0xfff];
				break;
			}
		break;

		case 1:	// DFxDATA
			data = harm_disp[dpcp_cnt[idx]];
			dpcp_cnt[idx] = (dpcp_cnt[idx] + 1) & 0xfff;
		break;

		case 2:	// DFxDATAW
			data = harm_disp[dpcp_cnt[idx]] & flag;
			dpcp_cnt[idx] = (dpcp_cnt[idx] + 1) & 0xfff;
		break;

		case 3:	// DFxFRACDATA
			data = harm_disp[dpcp_frac[idx] >> 8];
			dpcp_frac[idx] = (dpcp_frac[idx] + dpcp_finc[idx]) & 0x0fffff;
		break;

		case 4:	// DFxFLAG
			if (idx < 4) data = flag;
		break;
	}

	return data;
}

static UINT8 dpcp_read(INT32 off)
{
	UINT8 data = harm_prog[(cart_bank << 12) + off];

	if (dpcp_fastfetch && harm_lda_imm && data < 0x28) {
		// LDA #<register: the operand is replaced with the register's value
		harm_lda_imm = 0;
		return dpcp_read_reg(data);
	}
	harm_lda_imm = 0;

	if (off < 0x28) return dpcp_read_reg(off);

	if (off >= 0xff6 && off <= 0xffb) cart_bank = off - 0xff6;

	if (dpcp_fastfetch) harm_lda_imm = (data == 0xa9);

	return data;
}

static void dpcp_write(INT32 off, UINT8 data)
{
	if (off >= 0xff6 && off <= 0xffb) {
		cart_bank = off - 0xff6;
		return;
	}

	if (off < 0x28 || off >= 0x80) return;

	const INT32 idx = off & 7;

	switch ((off - 0x28) >> 3)
	{
		case 0x0:	// DFxFRACLOW
			dpcp_frac[idx] = (dpcp_frac[idx] & dpcp_fraclow_mask) | (data << 8);
		break;

		case 0x1:	// DFxFRACHI
			dpcp_frac[idx] = ((data & 0x0f) << 16) | (dpcp_frac[idx] & 0x00ffff);
		break;

		case 0x2:	// DFxFRACINC
			dpcp_finc[idx] = data;
			dpcp_frac[idx] &= 0x0fff00;
		break;

		case 0x3: dpcp_top[idx] = data; break;	// DFxTOP
		case 0x4: dpcp_bot[idx] = data; break;	// DFxBOT

		case 0x5:	// DFxLOW
			dpcp_cnt[idx] = (dpcp_cnt[idx] & 0x0f00) | data;
		break;

		case 0x6:
			switch (idx)
			{
				case 0: dpcp_fastfetch = (data == 0); break;	// FASTFETCH
				case 1:	// PARAMETER
					if (dpcp_param_ptr < 8) dpcp_param[dpcp_param_ptr++] = data;
				break;
				case 2: dpcp_callfunction(data); break;			// CALLFUNCTION
				case 5:
				case 6:
				case 7:	// WAVEFORMx
					harm_update_music();
					dpcp_wave[idx - 5] = data & 0x7f;
				break;
			}
		break;

		case 0x7:	// DFxPUSH
			dpcp_cnt[idx] = (dpcp_cnt[idx] - 1) & 0x0fff;
			harm_disp[dpcp_cnt[idx]] = data;
		break;

		case 0x8:	// DFxHI
			dpcp_cnt[idx] = ((data & 0x0f) << 8) | (dpcp_cnt[idx] & 0x00ff);
		break;

		case 0x9:
			switch (idx)
			{
				case 0: dpcp_rand = 0x2b435044; break;	// RRESET
				case 1: dpcp_rand = (dpcp_rand & 0xffffff00) | data; break;
				case 2: dpcp_rand = (dpcp_rand & 0xffff00ff) | (data << 8); break;
				case 3: dpcp_rand = (dpcp_rand & 0xff00ffff) | (data << 16); break;
				case 4: dpcp_rand = (dpcp_rand & 0x00ffffff) | ((UINT32)data << 24); break;
				case 5:
				case 6:
				case 7:	// NOTEx
				{
					const UINT8 *f = harm_disp + 0x1000 + (data << 2);
					harm_update_music();
					harm_mus_freq[idx - 5] = f[0] | (f[1] << 8) | (f[2] << 16) | ((UINT32)f[3] << 24);
				}
				break;
			}
		break;

		case 0xa:	// DFxWRITE
			harm_disp[dpcp_cnt[idx]] = data;
			dpcp_cnt[idx] = (dpcp_cnt[idx] + 1) & 0x0fff;
		break;
	}
}

// ---- CDF (CDF0, CDF1, CDFJ) ----
// 32k: 2k ARM driver, 2k+ of the game's ARM code, 7 x 4k 6507 banks (the
// first one(s) usually hold more ARM code).  RAM: 2k copy of the driver, which
// also holds the data stream pointers and increments, then 6k display data.
enum { CDF_0 = 0, CDF_1, CDF_J };

static INT32 cdf_sub;
static UINT32 cdf_ds_base, cdf_inc_base, cdf_wave_base;
static INT32 cdf_amp_stream;		// fast fetch registers are 0 - cdf_amp_stream
static INT32 cdf_jmp_mask;			// JMP $0000 (and $0001 for CDFJ) = FASTJMP
static UINT8 cdf_mode;
static UINT8 cdf_wavesize[3];
static INT32 cdf_jmp_active;
static UINT32 cdf_jmp_addr;
static INT32 cdf_jmp_stream;

#define CDF_COMMSTREAM	0x20
#define CDF_JMPSTREAM	0x21

static inline UINT32 harm_ram32(UINT32 a)
{
	a &= HARM_RAM_SIZE - 4;
	return harm_ram[a] | (harm_ram[a + 1] << 8) | (harm_ram[a + 2] << 16) | ((UINT32)harm_ram[a + 3] << 24);
}

static inline void harm_ram32_w(UINT32 a, UINT32 d)
{
	a &= HARM_RAM_SIZE - 4;
	harm_ram[a] = d; harm_ram[a + 1] = d >> 8; harm_ram[a + 2] = d >> 16; harm_ram[a + 3] = d >> 24;
}

static UINT32 harm_callback(INT32 func, UINT32 a, UINT32 b)
{
	a %= 3;

	switch (func)
	{
		case 0:	// set note
			harm_update_music();
			harm_mus_freq[a] = b;
		break;

		case 1:	// reset wave
			harm_update_music();
			harm_mus_cnt[a] = 0;
		break;

		case 2:	// get wave pointer
			harm_update_music();
			return harm_mus_cnt[a];

		case 3:	// set wave size
			cdf_wavesize[a] = b;
		break;
	}

	return 0;
}

// the driver's functions the game's code calls (set note, reset wave, get
// wave pointer, set wave size) are listed in a table at the end of the driver
static void cdf_find_callbacks()
{
	for (INT32 i = 0; i < 4; i++) {
		const UINT8 *p = DrvCartROM + 0x7e8 + i * 4;
		const UINT32 a = p[0] | (p[1] << 8) | (p[2] << 16) | ((UINT32)p[3] << 24);
		thumb_cb[i] = (a < 0x800 && !(a & 3)) ? a : 0;
	}
}

static void cdf_reset()
{
	cdf_sub = CDF_J;
	for (UINT32 i = 0; i + 4 <= cart_size; i++) {
		if (memcmp(DrvCartROM + i, "CDF", 3) == 0) {
			if (DrvCartROM[i + 3] == 'J') { cdf_sub = CDF_J; break; }
			if (DrvCartROM[i + 3] == 0) { cdf_sub = CDF_0; break; }
			if (DrvCartROM[i + 3] == 1) { cdf_sub = CDF_1; break; }
		}
	}

	harm_prog = DrvCartROM + 0x1000;
	harm_disp = harm_ram + 0x0800;
	memcpy(harm_ram, DrvCartROM, 0x800);	// the driver runs from RAM

	switch (cdf_sub)
	{
		case CDF_0:
			cdf_ds_base = 0x06e0; cdf_inc_base = 0x0768; cdf_wave_base = 0x07f0;
			cdf_amp_stream = 0x22; cdf_jmp_mask = 0xff;
		break;

		case CDF_1:
			cdf_ds_base = 0x00a0; cdf_inc_base = 0x0128; cdf_wave_base = 0x01b0;
			cdf_amp_stream = 0x22; cdf_jmp_mask = 0xff;
		break;

		case CDF_J:
			cdf_ds_base = 0x0098; cdf_inc_base = 0x0124; cdf_wave_base = 0x01b0;
			cdf_amp_stream = 0x23; cdf_jmp_mask = 0xfe;
		break;
	}

	cdf_mode = 0xff;
	cdf_wavesize[0] = cdf_wavesize[1] = cdf_wavesize[2] = 27;
	cdf_jmp_active = 0;
	cdf_jmp_addr = 0;
	cdf_jmp_stream = CDF_JMPSTREAM;
	cdf_find_callbacks();
}

static UINT8 cdf_stream_read(INT32 idx, INT32 inc_one)
{
	const UINT32 a = cdf_ds_base + idx * 4;
	const UINT32 ptr = harm_ram32(a);
	const UINT32 inc = inc_one ? 0x100 : harm_ram32(cdf_inc_base + idx * 4);
	const UINT8 data = harm_disp[ptr >> 20];	// 12.20 fixed point, 4k
	harm_ram32_w(a, ptr + (inc << 12));
	return data;
}

static UINT8 cdf_amplitude()
{
	harm_update_music();

	if ((cdf_mode & 0xf0) == 0) {	// digital audio: packed 4 bit samples
		const UINT32 a = harm_ram32(cdf_wave_base) + (harm_mus_cnt[0] >> 21);
		UINT8 d = 0;
		if (a < cart_size) d = DrvCartROM[a];
		else if (a >= 0x40000000 && a < 0x40000000 + HARM_RAM_SIZE) d = harm_ram[a - 0x40000000];
		if ((harm_mus_cnt[0] & (1 << 20)) == 0) d >>= 4;
		return d & 0x0f;
	}

	UINT32 sum = 0;
	for (INT32 i = 0; i < 3; i++) {
		UINT32 w = harm_ram32(cdf_wave_base + i * 4) - 0x40000800;
		if (w >= 0x1000) w &= 0xfff;
		const INT32 shift = (cdf_wavesize[i] > 31) ? 31 : cdf_wavesize[i];
		sum += harm_disp[(w + (harm_mus_cnt[i] >> shift)) % 0x1800];
	}
	return sum;
}

static UINT8 cdf_read(INT32 off)
{
	const INT32 fastfetch = (cdf_mode & 0x0f) == 0;
	const UINT8 data = harm_prog[(cart_bank << 12) + off];

	if (cdf_jmp_active && cdf_jmp_addr == (UINT32)off) {	// operand of JMP FASTJMP
		cdf_jmp_active--;
		cdf_jmp_addr++;
		return cdf_stream_read(cdf_jmp_stream, 1);
	}

	if (fastfetch && data == 0x4c && off < 0xffd &&
		(harm_prog[(cart_bank << 12) + off + 1] & cdf_jmp_mask) == 0 && harm_prog[(cart_bank << 12) + off + 2] == 0)
	{
		cdf_jmp_active = 2;
		cdf_jmp_addr = off + 1;
		cdf_jmp_stream = CDF_JMPSTREAM + harm_prog[(cart_bank << 12) + off + 1];
		return data;
	}
	cdf_jmp_addr = 0;

	if (harm_lda_addr == (UINT32)off && cpu_now() == harm_lda_cyc + 1) {	// operand right after the opcode
		harm_lda_addr = 0;
		if (data < cdf_amp_stream) return cdf_stream_read(data, 0);
		if (data == cdf_amp_stream) return cdf_amplitude();
	}
	harm_lda_addr = 0;

	if (off >= 0xff4 && off <= 0xffb) cart_bank = (off == 0xff4) ? 6 : (off - 0xff5);

	if (fastfetch && data == 0xa9) {	// LDA #
		harm_lda_addr = off + 1;
		harm_lda_cyc = cpu_now();
	}

	return data;
}

static void cdf_write(INT32 off, UINT8 data)
{
	switch (off)
	{
		case 0xff0:	// DSWRITE
		{
			const UINT32 a = cdf_ds_base + CDF_COMMSTREAM * 4;
			const UINT32 ptr = harm_ram32(a);
			harm_disp[ptr >> 20] = data;
			harm_ram32_w(a, ptr + 0x100000);
		}
		break;

		case 0xff1:	// DSPTR
		{
			const UINT32 a = cdf_ds_base + CDF_COMMSTREAM * 4;
			UINT32 ptr = harm_ram32(a);
			ptr = ((ptr << 8) & 0xf0000000) | (data << 20);
			harm_ram32_w(a, ptr);
		}
		break;

		case 0xff2:	// SETMODE
			cdf_mode = data;
		break;

		case 0xff3:	// CALLFN
			if (data >= 254) thumb_run(0x0808, 0x0800, 0x40001ffc);
		break;

		default:
			if (off >= 0xff4 && off <= 0xffb) cart_bank = (off == 0xff4) ? 6 : (off - 0xff5);
		break;
	}
}

static void harm_reset()
{
	harm_ram = DrvCartRAM;
	memset(harm_ram, 0, HARM_RAM_SIZE);

	memset(harm_mus_cnt, 0, sizeof(harm_mus_cnt));
	memset(harm_mus_freq, 0, sizeof(harm_mus_freq));
	harm_mus_last = 0;
	harm_mus_frac = 0;
	harm_lda_imm = 0;
	harm_lda_addr = 0;
	harm_lda_cyc = 0;
	thumb_icount = 0;

	if (cart_type == CART_DPCP) {
		memset(thumb_cb, 0, sizeof(thumb_cb));
		dpcp_reset();
		cart_bank = 5;
	} else {
		cdf_reset();
		cart_bank = 6;
	}
}

static INT32 harm_scan()
{
	SCAN_VAR(harm_mus_cnt);
	SCAN_VAR(harm_mus_freq);
	SCAN_VAR(harm_mus_last);
	SCAN_VAR(harm_mus_frac);
	SCAN_VAR(harm_lda_imm);
	SCAN_VAR(harm_lda_addr);
	SCAN_VAR(harm_lda_cyc);
	SCAN_VAR(thumb_icount);

	if (cart_type == CART_DPCP) {
		SCAN_VAR(dpcp_cnt);
		SCAN_VAR(dpcp_frac);
		SCAN_VAR(dpcp_finc);
		SCAN_VAR(dpcp_top);
		SCAN_VAR(dpcp_bot);
		SCAN_VAR(dpcp_fastfetch);
		SCAN_VAR(dpcp_param);
		SCAN_VAR(dpcp_param_ptr);
		SCAN_VAR(dpcp_rand);
		SCAN_VAR(dpcp_wave);
		SCAN_VAR(dpcp_fraclow_mask);
	} else {
		SCAN_VAR(cdf_mode);
		SCAN_VAR(cdf_wavesize);
		SCAN_VAR(cdf_jmp_active);
		SCAN_VAR(cdf_jmp_addr);
		SCAN_VAR(cdf_jmp_stream);
	}

	return 0;
}

static void cart_reset()
{
	switch (cart_type)
	{
		case CART_F4:  cart_bank = 7; break;
		case CART_3F:
		case CART_3E:  cart_bank = (cart_size >> 11) - 1; break;	// last 2k bank
		default:       cart_bank = 0; break;
	}
	if (cart_startbank >= 0) cart_bank = cart_startbank;

	cart_e0[0] = 4; cart_e0[1] = 5; cart_e0[2] = 6; cart_e0[3] = 7;
	cart_e7_bank = 0;
	cart_e7_ram = 0;
	cart_3e_ram = 0;
	cart_3e_rambank = 0;
	cart_fe_armed = 0;

	memset(dpc_top, 0, sizeof(dpc_top));
	memset(dpc_bot, 0, sizeof(dpc_bot));
	memset(dpc_flag, 0, sizeof(dpc_flag));
	memset(dpc_cnt, 0, sizeof(dpc_cnt));
	memset(dpc_music, 0, sizeof(dpc_music));
	dpc_rand = 0;
	dpc_movamt = 0;
	dpc_latch62 = dpc_latch64 = 0;
	dpc_last = 0;
	dpc_frac = 0;

	if (cart_type == CART_DPCP || cart_type == CART_CDF) harm_reset();
}

static inline void cart_hotspot(INT32 off)
{
	switch (cart_type)
	{
		case CART_F8:
		case CART_DPC:
			if (off == 0xff8 || off == 0xff9) cart_bank = off - 0xff8;
		break;

		case CART_F6:
			if (off >= 0xff6 && off <= 0xff9) cart_bank = off - 0xff6;
		break;

		case CART_F4:
			if (off >= 0xff4 && off <= 0xffb) cart_bank = off - 0xff4;
		break;

		case CART_FA:
			if (off >= 0xff8 && off <= 0xffa) cart_bank = off - 0xff8;
		break;

		case CART_E0:
			if (off >= 0xfe0 && off <= 0xff7) cart_e0[(off - 0xfe0) >> 3] = off & 7;
		break;

		case CART_E7:
			if (off >= 0xfe0 && off <= 0xfe7) cart_e7_bank = off & 7;
			else if (off >= 0xfe8 && off <= 0xfeb) cart_e7_ram = off & 3;
		break;

		case CART_F0:
			if (off == 0xff0) cart_bank = (cart_bank + 1) & 0x0f;
		break;
	}
}

static UINT8 cart_read(UINT16 address)
{
	const INT32 off = address & 0xfff;

	switch (cart_type)
	{
		case CART_2K:
			return DrvCartROM[off & 0x7ff];

		case CART_4K:
			return DrvCartROM[off];

		case CART_F8:
		case CART_F6:
		case CART_F4:
		case CART_F0:
			if (cart_superchip && off < 0x100) {
				if (off >= 0x80) return DrvCartRAM[off & 0x7f];
				return bus_last;	// read from the write port
			}
			cart_hotspot(off);
			return DrvCartROM[((cart_bank << 12) + off) % cart_size];

		case CART_FA:
			if (off < 0x200) {
				if (off >= 0x100) return DrvCartRAM[off & 0xff];
				return bus_last;
			}
			cart_hotspot(off);
			return DrvCartROM[(cart_bank << 12) + off];

		case CART_FE:
		case CART_UA:
			return DrvCartROM[(cart_bank << 12) + off];

		case CART_E0:
			cart_hotspot(off);
			return DrvCartROM[(cart_e0[off >> 10] << 10) + (off & 0x3ff)];

		case CART_E7:
			cart_hotspot(off);
			if (off < 0x800) {
				// bank 7 = 1k RAM: written at $1000-$13ff, read at $1400-$17ff
				if (cart_e7_bank == 7 && off >= 0x400) return DrvCartRAM[off & 0x3ff];
				return DrvCartROM[(cart_e7_bank << 11) + off];
			}
			if (off >= 0x900 && off < 0xa00) {	// 4x 256 byte RAM banks, read port
				return DrvCartRAM[0x400 + (cart_e7_ram << 8) + (off & 0xff)];
			}
			return DrvCartROM[0x3800 + (off & 0x7ff)];

		case CART_3F:
		case CART_3E:
			if (off < 0x800) {
				if (cart_3e_ram) {
					if (off < 0x400) return DrvCartRAM[((cart_3e_rambank << 10) + off) & (A26_CARTRAM_SIZE - 1)];
					return bus_last;
				}
				return DrvCartROM[((cart_bank << 11) + off) % cart_size];
			}
			return DrvCartROM[cart_size - 0x800 + (off & 0x7ff)];

		case CART_CV:
			if (off >= 0x800) return DrvCartROM[off & 0x7ff];
			if (off < 0x400) return DrvCartRAM[off];
			return bus_last;

		case CART_DPC:
			if (off < 0x40) return dpc_read(off);
			cart_hotspot(off);
			return DrvCartROM[(cart_bank << 12) + off];

		case CART_DPCP:
			return dpcp_read(off);

		case CART_CDF:
			return cdf_read(off);
	}

	return 0xff;
}

static void cart_write(UINT16 address, UINT8 data)
{
	const INT32 off = address & 0xfff;

	switch (cart_type)
	{
		case CART_F8:
		case CART_F6:
		case CART_F4:
		case CART_F0:
			if (cart_superchip && off < 0x80) {
				DrvCartRAM[off] = data;
				return;
			}
			cart_hotspot(off);
		return;

		case CART_FA:
			if (off < 0x100) {
				DrvCartRAM[off] = data;
				return;
			}
			cart_hotspot(off);
		return;

		case CART_E0:
			cart_hotspot(off);
		return;

		case CART_E7:
			if (off < 0x400 && cart_e7_bank == 7) {
				DrvCartRAM[off] = data;
				return;
			}
			if (off >= 0x800 && off < 0x900) {
				DrvCartRAM[0x400 + (cart_e7_ram << 8) + (off & 0xff)] = data;
				return;
			}
			cart_hotspot(off);
		return;

		case CART_3E:
			if (cart_3e_ram && off >= 0x400 && off < 0x800) {
				DrvCartRAM[((cart_3e_rambank << 10) + (off & 0x3ff)) & (A26_CARTRAM_SIZE - 1)] = data;
			}
		return;

		case CART_CV:
			if (off >= 0x400 && off < 0x800) DrvCartRAM[off & 0x3ff] = data;
		return;

		case CART_DPC:
			if (off >= 0x40 && off < 0x80) {
				dpc_write(off - 0x40, data);
				return;
			}
			cart_hotspot(off);
		return;

		case CART_DPCP:
			dpcp_write(off, data);
		return;

		case CART_CDF:
			cdf_write(off, data);
		return;
	}
}

// accesses outside the cart space that some mappers watch
static inline void cart_snoop(UINT16 address, UINT8 data, INT32 is_write)
{
	switch (cart_type)
	{
		case CART_FE:
			// Activision: the access after $01fe carries the new PCH, D5 picks the bank
			if (cart_fe_armed) {
				cart_bank = (data & 0x20) ? 0 : 1;
				cart_fe_armed = 0;
			}
			if (address == 0x01fe) cart_fe_armed = 1;
		break;

		case CART_UA:
			// accesses to $0200-$023f select the low 4k, $0240-$027f the high 4k
			if ((address & 0x1f80) == 0x0200) cart_bank = (address >> 6) & 1;
		break;

		case CART_3F:
			if (is_write && address < 0x40) cart_bank = data % (cart_size >> 11);
		break;

		case CART_3E:
			if (is_write && address < 0x40) {
				if (address == 0x3e) {
					cart_3e_ram = 1;
					cart_3e_rambank = data;
				} else if (address == 0x3f) {
					cart_3e_ram = 0;
					cart_bank = data % (cart_size >> 11);
				}
			}
		break;
	}
}

static UINT8 a2600_read(UINT16 address)
{
	address &= 0x1fff;
	UINT8 data;

	if (address & 0x1000) {
		data = cart_read(address);
	} else if (!(address & 0x80)) {
		data = tia_read(address);
	} else if (!(address & 0x200)) {
		data = DrvRIOTRAM[address & 0x7f];
	} else {
		data = riot_read(address);
	}

	if (cart_type == CART_FE || cart_type == CART_UA) cart_snoop(address, data, 0);

	bus_last = data;
	return data;
}

static void a2600_write(UINT16 address, UINT8 data)
{
	address &= 0x1fff;
	bus_last = data;

	if (address & 0x1000) {
		cart_write(address, data);
	} else {
		if (!(address & 0x80)) {
			tia_write(address, data);
		} else if (!(address & 0x200)) {
			DrvRIOTRAM[address & 0x7f] = data;
		} else {
			riot_write(address, data);
		}
	}

	switch (cart_type) {
		case CART_FE:
		case CART_UA:
		case CART_3F:
		case CART_3E:
			cart_snoop(address, data, 1);
		break;
	}
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 128; i++)
	{
		const INT32 hue = i >> 3;
		const INT32 lum = i & 7;

		double y, u = 0.0, v = 0.0;
		INT32 gray;
		double angle = 0.0;

		if (is_pal) {
			gray = (hue < 2 || hue > 13);
			if (!gray) {
				if (hue & 1) angle = 200.0 + ((hue - 3) / 2) * 33.0;
				else         angle = 167.0 - ((hue - 2) / 2) * 33.0;
			}
		} else {
			gray = (hue == 0);
			angle = 167.0 - (hue - 1) * 25.7;
		}

		if (gray) {
			y = (lum / 7.0) * 0.925;
		} else {
			y = 0.07 + lum * 0.12;
			const double sat = 0.20;
			const double rad = angle * 3.14159265358979 / 180.0;
			u = sat * cos(rad);
			v = sat * sin(rad);
		}

		double r = y + 1.140 * v;
		double g = y - 0.395 * u - 0.581 * v;
		double b = y + 2.032 * u;

		if (r < 0.0) r = 0.0;
		if (r > 1.0) r = 1.0;
		if (g < 0.0) g = 0.0;
		if (g > 1.0) g = 1.0;
		if (b < 0.0) b = 0.0;
		if (b > 1.0) b = 1.0;

		BurnPalette[i] = BurnHighCol((INT32)(r * 255.0 + 0.5), (INT32)(g * 255.0 + 0.5), (INT32)(b * 255.0 + 0.5), 0);
	}
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvCartROM		= Next; Next += (cart_size < 0x1000) ? 0x1000 : ((cart_size + 0xfff) & ~0xfff);

	BurnPalette		= (UINT32*)Next; Next += 0x80 * sizeof(UINT32);

	DrvFrameBuf		= Next; Next += 2 * A26_MAXLINES * A26_W;

	aud_buf			= (INT16*)Next; Next += AUD_BUF_SIZE * sizeof(INT16);

	AllRam			= Next;

	DrvRIOTRAM		= Next; Next += 0x000080;
	DrvCartRAM		= Next; Next += A26_CARTRAM_SIZE;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	cpu_base = 0;
	bus_last = 0;

	tia_reset();
	riot_reset();
	cart_reset();

	M6502Open(0);
	M6502Reset();
	M6502NewFrame();
	M6502Close();

	aud_pos = 0;

	if (ctrl_type == CTRL_PADDLE) {
		BurnTrackballReadReset();
	}

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(is_pal ? 49.86 : 59.92);

	{
		struct BurnRomInfo ri;
		ri.nLen = 0;
		BurnDrvGetRomInfo(&ri, 0);
		cart_size = ri.nLen;
		if (cart_size == 0) return 1;
	}

	BurnAllocMemIndex();

	if (BurnLoadRom(DrvCartROM, 0, 1)) return 1;

	if (cart_size < 0x1000) {	// mirror small carts (2k) across 4k
		for (UINT32 i = cart_size; i < 0x1000; i++) {
			DrvCartROM[i] = DrvCartROM[i % cart_size];
		}
		if (cart_type != CART_CV) cart_size = 0x1000;
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x1fff);
	M6502SetReadHandler(a2600_read);
	M6502SetWriteHandler(a2600_write);
	M6502Close();

	if (ctrl_type == CTRL_PADDLE) {
		BurnTrackballInit(2, 0x80);
	}

	tia_build_tables();

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	M6502Exit();

	if (ctrl_type == CTRL_PADDLE) {
		BurnTrackballExit();
	}

	BurnFreeMemIndex();
	BurnPalette = NULL;

	cart_type = CART_4K;
	cart_superchip = 0;
	cart_startbank = -1;
	is_pal = 0;
	ctrl_type = CTRL_JOY;
	paddle_min = 0x00;
	paddle_max = 0xff;

	return 0;
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteInit();
		BurnRecalc = 0;
	}

	const UINT8 *fb = DrvFrameBuf + fb_disp * (A26_MAXLINES * A26_W);

	// 1x or 2x, whatever the driver's screen size says
	const INT32 scale = (nScreenWidth >= A26_W * 2) ? 2 : 1;

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		const INT32 sy = (is_pal ? A26_PAL_Y : A26_NTSC_Y) + y / scale;
		UINT16 *dst = pTransDraw + y * nScreenWidth;

		if (sy < 0 || sy >= A26_MAXLINES) {
			memset(dst, 0, nScreenWidth * sizeof(UINT16));
			continue;
		}

		const UINT8 *src = fb + sy * A26_W;
		if (scale == 2) {
			for (INT32 x = 0; x < A26_W && x * 2 + 1 < nScreenWidth; x++) {
				dst[x * 2] = dst[x * 2 + 1] = src[x];
			}
		} else {
			for (INT32 x = 0; x < nScreenWidth && x < A26_W; x++) {
				dst[x] = src[x];
			}
		}
	}

	BurnTransferCopy(BurnPalette);

	return 0;
}

static void DrvMakeInputs()
{
	UINT8 j1 = 0, j2 = 0, sw = 0;

	for (INT32 i = 0; i < 8; i++) {
		j1 |= (DrvJoy1[i] & 1) << i;
		j2 |= (DrvJoy2[i] & 1) << i;
		sw |= (DrvJoy3[i] & 1) << i;
	}

	// console switches: D0 reset, D1 select, D3 color, D6 left difficulty, D7 right difficulty
	input_swchb = 0x0b;
	if (sw & 1) input_swchb &= ~0x01;
	if (sw & 2) input_swchb &= ~0x02;
	if (DrvDips[0] & 0x04) input_swchb &= ~0x08;
	if (DrvDips[0] & 0x01) input_swchb |= 0x40;
	if (DrvDips[0] & 0x02) input_swchb |= 0x80;

	if (ctrl_type == CTRL_PADDLE)
	{
		// paddle fire buttons are wired to port A: P1 D7, P2 D6, P3 D3, P4 D2
		static const UINT8 fire_bit[4] = { 0x80, 0x40, 0x08, 0x04 };

		input_swcha = 0xff;
		for (INT32 i = 0; i < 4; i++) {
			if (DrvJoyP[i][2]) input_swcha &= ~fire_bit[i];
		}
		input_fire[0] = input_fire[1] = 1;

		for (INT32 dev = 0; dev < 2; dev++) {
			BurnTrackballConfig(dev, AXIS_NORMAL, AXIS_NORMAL);
			BurnTrackballFrame(dev, DrvAnalog[dev * 2 + 0], DrvAnalog[dev * 2 + 1], 0x02, 0x0a);
			BurnTrackballUDLR(dev, DrvJoyP[dev * 2 + 1][0], DrvJoyP[dev * 2 + 1][1], DrvJoyP[dev * 2 + 0][0], DrvJoyP[dev * 2 + 0][1], 4);
			BurnTrackballConfigStartStopPoints(dev, paddle_min, paddle_max, paddle_min, paddle_max);
			BurnTrackballUpdate(dev);
		}

		for (INT32 i = 0; i < 4; i++) {
			const INT32 pos = BurnTrackballRead(i >> 1, i & 1);
			// turning right lowers the resistance -> the capacitor charges sooner
			paddle_lines[i] = 2 + ((255 - pos) * 236) / 255;
		}
	}
	else
	{
		// joysticks: P1 on D7-D4 (right, left, down, up), P2 on D3-D0, active low
		input_swcha = 0xff;
		if (j1 & 0x01) input_swcha &= ~0x10;
		if (j1 & 0x02) input_swcha &= ~0x20;
		if (j1 & 0x04) input_swcha &= ~0x40;
		if (j1 & 0x08) input_swcha &= ~0x80;
		if (j2 & 0x01) input_swcha &= ~0x01;
		if (j2 & 0x02) input_swcha &= ~0x02;
		if (j2 & 0x04) input_swcha &= ~0x04;
		if (j2 & 0x08) input_swcha &= ~0x08;

		input_fire[0] = (j1 & 0x10) ? 0 : 1;
		input_fire[1] = (j2 & 0x10) ? 0 : 1;
	}

	// latched fire buttons (VBLANK D6)
	if (tia_vblank & 0x40) {
		if (!input_fire[0]) tia_inpt_latch[0] = 0;
		if (!input_fire[1]) tia_inpt_latch[1] = 0;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	DrvMakeInputs();

	M6502Open(0);

	aud_pos = 0;
	frame_done = 0;

	INT32 guard = 0;
	while (!frame_done && guard < A26_MAXLINES + 8)
	{
		M6502Run(76);
		tia_run(cpu_now() * 3);
		guard++;
	}

	cpu_base += M6502TotalCycles();
	M6502NewFrame();

	M6502Close();

	if (pBurnSoundOut) {
		if (aud_pos > 1) {
			// resample the TIA's ~31.4kHz stream to the output rate
			for (INT32 i = 0; i < nBurnSoundLen; i++) {
				const INT32 pos = (INT32)(((INT64)i * (aud_pos - 1) * 256) / (nBurnSoundLen > 1 ? nBurnSoundLen - 1 : 1));
				const INT32 p = pos >> 8, f = pos & 0xff;
				const INT32 s0 = aud_buf[p];
				const INT32 s1 = aud_buf[(p + 1 < aud_pos) ? p + 1 : p];
				const INT16 s = (INT16)(s0 + (((s1 - s0) * f) >> 8));
				pBurnSoundOut[i * 2 + 0] = s;
				pBurnSoundOut[i * 2 + 1] = s;
			}
		} else {
			memset(pBurnSoundOut, 0, nBurnSoundLen * 2 * sizeof(INT16));
		}
		BurnSoundDCFilter();
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		if (ctrl_type == CTRL_PADDLE) {
			BurnTrackballScan();
		}

		SCAN_VAR(cpu_base);
		SCAN_VAR(bus_last);

		SCAN_VAR(tia_obj);
		SCAN_VAR(tia_nusiz);
		SCAN_VAR(tia_colup0);
		SCAN_VAR(tia_colup1);
		SCAN_VAR(tia_colupf);
		SCAN_VAR(tia_colubk);
		SCAN_VAR(tia_ctrlpf);
		SCAN_VAR(tia_refp);
		SCAN_VAR(tia_pf);
		SCAN_VAR(tia_grp_new);
		SCAN_VAR(tia_grp_old);
		SCAN_VAR(tia_enam);
		SCAN_VAR(tia_enabl_new);
		SCAN_VAR(tia_enabl_old);
		SCAN_VAR(tia_hm);
		SCAN_VAR(tia_vdelp);
		SCAN_VAR(tia_vdelbl);
		SCAN_VAR(tia_resmp);
		SCAN_VAR(tia_vsync);
		SCAN_VAR(tia_vblank);
		SCAN_VAR(tia_collision);
		SCAN_VAR(tia_hpos);
		SCAN_VAR(tia_line);
		SCAN_VAR(tia_line_ext);
		SCAN_VAR(tia_hmove_latch);
		SCAN_VAR(tia_hmove_active);
		SCAN_VAR(tia_hmove_step);
		SCAN_VAR(tia_hmove_next);
		SCAN_VAR(tia_gfx_reg);
		SCAN_VAR(tia_gfx_data);
		SCAN_VAR(tia_gfx_clk);
		SCAN_VAR(tia_res_pending);
		SCAN_VAR(tia_res_any);
		SCAN_VAR(tia_hmove_pulse);
		SCAN_VAR(tia_hmove_pulse_clk);
		SCAN_VAR(tia_clk);
		SCAN_VAR(tia_pf_cur);
		SCAN_VAR(tia_inpt_latch);
		SCAN_VAR(paddle_dumped);
		SCAN_VAR(paddle_release);
		SCAN_VAR(tia_audc);
		SCAN_VAR(tia_audf);
		SCAN_VAR(tia_audv);
		SCAN_VAR(tia_aud);
		SCAN_VAR(fb_cur);
		SCAN_VAR(fb_disp);

		SCAN_VAR(riot_swcha_out);
		SCAN_VAR(riot_swacnt);
		SCAN_VAR(riot_swchb_out);
		SCAN_VAR(riot_swbcnt);
		SCAN_VAR(riot_timer_val);
		SCAN_VAR(riot_timer_shift);
		SCAN_VAR(riot_timer_load);
		SCAN_VAR(riot_timer_under);
		SCAN_VAR(riot_flag_clear);
		SCAN_VAR(riot_flag_cleared);
		SCAN_VAR(riot_pa7_flag);

		SCAN_VAR(cart_bank);
		SCAN_VAR(cart_e0);
		SCAN_VAR(cart_e7_bank);
		SCAN_VAR(cart_e7_ram);
		SCAN_VAR(cart_3e_ram);
		SCAN_VAR(cart_3e_rambank);
		SCAN_VAR(cart_fe_armed);

		if (cart_type == CART_DPC) {
			SCAN_VAR(dpc_top);
			SCAN_VAR(dpc_bot);
			SCAN_VAR(dpc_flag);
			SCAN_VAR(dpc_cnt);
			SCAN_VAR(dpc_music);
			SCAN_VAR(dpc_rand);
			SCAN_VAR(dpc_movamt);
			SCAN_VAR(dpc_latch62);
			SCAN_VAR(dpc_latch64);
			SCAN_VAR(dpc_last);
			SCAN_VAR(dpc_frac);
		}

		if (cart_type == CART_DPCP || cart_type == CART_CDF) {
			harm_scan();
		}
	}

	return 0;
}

static INT32 A2600GetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		pszGameName = BurnDrvGetTextA(DRV_PARENT);
	}

	if (pszGameName == NULL || i > 1) {
		*pszName = NULL;
		return 1;
	}

	// remove the "a26_" prefix
	memset(szFilename, 0, MAX_PATH);
	strncpy(szFilename, pszGameName + 4, MAX_PATH - 1);

	*pszName = szFilename;

	return 0;
}

// per-cart. config
static INT32 A26Init_2K() { cart_type = CART_2K; return DrvInit(); }
static INT32 A26Init_2K_PAD() { cart_type = CART_2K; ctrl_type = CTRL_PADDLE; return DrvInit(); }
static INT32 A26Init_3F() { cart_type = CART_3F; return DrvInit(); }
static INT32 A26Init_4K() { cart_type = CART_4K; return DrvInit(); }
static INT32 A26Init_4K_PAD() { cart_type = CART_4K; ctrl_type = CTRL_PADDLE; return DrvInit(); }
static INT32 A26Init_4K_PAL() { cart_type = CART_4K; is_pal = 1; return DrvInit(); }
static INT32 A26Init_CDF() { cart_type = CART_CDF; return DrvInit(); }
static INT32 A26Init_DPC() { cart_type = CART_DPC; return DrvInit(); }
static INT32 A26Init_DPCPLUS() { cart_type = CART_DPCP; return DrvInit(); }
static INT32 A26Init_E0() { cart_type = CART_E0; return DrvInit(); }
static INT32 A26Init_E7() { cart_type = CART_E7; return DrvInit(); }
static INT32 A26Init_F4() { cart_type = CART_F4; return DrvInit(); }
static INT32 A26Init_F4_PAD() { cart_type = CART_F4; ctrl_type = CTRL_PADDLE; return DrvInit(); }
static INT32 A26Init_F4_SC() { cart_type = CART_F4; cart_superchip = 1; return DrvInit(); }
static INT32 A26Init_F6() { cart_type = CART_F6; return DrvInit(); }
static INT32 A26Init_F6_SC() { cart_type = CART_F6; cart_superchip = 1; return DrvInit(); }
static INT32 A26Init_F8() { cart_type = CART_F8; return DrvInit(); }
static INT32 A26Init_F8_B1() { cart_type = CART_F8; cart_startbank = 1; return DrvInit(); }
static INT32 A26Init_F8_SC() { cart_type = CART_F8; cart_superchip = 1; return DrvInit(); }
static INT32 A26Init_FA() { cart_type = CART_FA; return DrvInit(); }
static INT32 A26Init_FE() { cart_type = CART_FE; return DrvInit(); }
static INT32 A26Init_kaboom() { paddle_min = 0x40; paddle_max = 0xd0; return A26Init_2K_PAD(); }

// -------------
// Classic Games
// -------------

// 3-D Tic-Tac-Toe (USA)

static struct BurnRomInfo a26_3dtictacRomDesc[] = {
	{ "3-D Tic-Tac-Toe (1978)(Atari).bin", 0x00800, 0x58805709, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_3dtictac)
STD_ROM_FN(a26_3dtictac)

struct BurnDriver BurnDrva26_3dtictac = {
	"a26_3dtictac", NULL, NULL, NULL, "1978",
	"3-D Tic-Tac-Toe (USA)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_PUZZLE, 0,
	A2600GetZipName, a26_3dtictacRomInfo, a26_3dtictacRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Abre-te, Sesamo! (USA)

static struct BurnRomInfo a26_abreteRomDesc[] = {
	{ "Abre-te, Sesamo! (1983)(CCE).bin", 0x01000, 0x8cf511a4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_abrete)
STD_ROM_FN(a26_abrete)

struct BurnDriver BurnDrva26_abrete = {
	"a26_abrete", NULL, NULL, NULL, "1983",
	"Abre-te, Sesamo! (USA)\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_abreteRomInfo, a26_abreteRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Activision Decathlon, The (USA)

static struct BurnRomInfo a26_decathlnRomDesc[] = {
	{ "Activision Decathlon, The (1983)(Activision).bin", 0x02000, 0x3feb39b1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_decathln)
STD_ROM_FN(a26_decathln)

struct BurnDriver BurnDrva26_decathln = {
	"a26_decathln", NULL, NULL, NULL, "1983",
	"Activision Decathlon, The (USA)\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ATARI2600, GBF_SPORTSMISC, 0,
	A2600GetZipName, a26_decathlnRomInfo, a26_decathlnRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_FE, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Adventure (USA)

static struct BurnRomInfo a26_adventurRomDesc[] = {
	{ "Adventure (1978)(Atari).bin", 0x01000, 0xa6db4b3a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_adventur)
STD_ROM_FN(a26_adventur)

struct BurnDriver BurnDrva26_adventur = {
	"a26_adventur", NULL, NULL, NULL, "1978",
	"Adventure (USA)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_ADV | GBF_MAZE, 0,
	A2600GetZipName, a26_adventurRomInfo, a26_adventurRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Adventures of Tron (USA)

static struct BurnRomInfo a26_advtronRomDesc[] = {
	{ "Adventures of Tron (1983)(Mattel Electronics).bin", 0x01000, 0x1b8019d6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_advtron)
STD_ROM_FN(a26_advtron)

struct BurnDriver BurnDrva26_advtron = {
	"a26_advtron", NULL, NULL, NULL, "1983",
	"Adventures of Tron (USA)\0", NULL, "Mattel Electronics", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_advtronRomInfo, a26_advtronRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Air Raiders (USA)

static struct BurnRomInfo a26_airraidsRomDesc[] = {
	{ "Air Raiders (1982)(Mattel Electronics).bin", 0x01000, 0xbb16539d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_airraids)
STD_ROM_FN(a26_airraids)

struct BurnDriver BurnDrva26_airraids = {
	"a26_airraids", NULL, NULL, NULL, "1982",
	"Air Raiders (USA)\0", NULL, "Mattel Electronics", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_SHOOT, 0,
	A2600GetZipName, a26_airraidsRomInfo, a26_airraidsRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Air-Sea Battle (USA)

static struct BurnRomInfo a26_airseabaRomDesc[] = {
	{ "Air-Sea Battle (1977)(Atari).bin", 0x00800, 0x1fc6c0f1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_airseaba)
STD_ROM_FN(a26_airseaba)

struct BurnDriver BurnDrva26_airseaba = {
	"a26_airseaba", NULL, NULL, NULL, "1977",
	"Air-Sea Battle (USA)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_SHOOT, 0,
	A2600GetZipName, a26_airseabaRomInfo, a26_airseabaRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Airlock (USA)

static struct BurnRomInfo a26_airlockRomDesc[] = {
	{ "Airlock (1982)(Data Age).bin", 0x01000, 0x25c90ce7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_airlock)
STD_ROM_FN(a26_airlock)

struct BurnDriver BurnDrva26_airlock = {
	"a26_airlock", NULL, NULL, NULL, "1982",
	"Airlock (USA)\0", NULL, "Data Age", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_airlockRomInfo, a26_airlockRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Alien (USA)

static struct BurnRomInfo a26_alienRomDesc[] = {
	{ "Alien (1982)(20th Century Fox).bin", 0x01000, 0x6d61059f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_alien)
STD_ROM_FN(a26_alien)

struct BurnDriver BurnDrva26_alien = {
	"a26_alien", NULL, NULL, NULL, "1982",
	"Alien (USA)\0", NULL, "20th Century Fox", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_ACTION | GBF_MAZE, 0,
	A2600GetZipName, a26_alienRomInfo, a26_alienRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Amidar (USA)

static struct BurnRomInfo a26_amidarRomDesc[] = {
	{ "Amidar (1983)(Parker Bros).bin", 0x01000, 0x8eb5bd72, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_amidar)
STD_ROM_FN(a26_amidar)

struct BurnDriver BurnDrva26_amidar = {
	"a26_amidar", NULL, NULL, NULL, "1983",
	"Amidar (USA)\0", NULL, "Parker Bros", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_ACTION, 0,
	A2600GetZipName, a26_amidarRomInfo, a26_amidarRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Aquaventure (USA)

static struct BurnRomInfo a26_aquaventRomDesc[] = {
	{ "Aquaventure (198x)(CCE).bin", 0x02000, 0x7fa61fa0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_aquavent)
STD_ROM_FN(a26_aquavent)

struct BurnDriver BurnDrva26_aquavent = {
	"a26_aquavent", NULL, NULL, NULL, "198?",
	"Aquaventure (USA)\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_MULTISHOOT, 0,
	A2600GetZipName, a26_aquaventRomInfo, a26_aquaventRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Armor Ambush (USA)

static struct BurnRomInfo a26_armorambRomDesc[] = {
	{ "Armor Ambush (1982)(Mattel Electronics).bin", 0x01000, 0xfc35704b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_armoramb)
STD_ROM_FN(a26_armoramb)

struct BurnDriver BurnDrva26_armoramb = {
	"a26_armoramb", NULL, NULL, NULL, "1982",
	"Armor Ambush (USA)\0", NULL, "Mattel Electronics", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_ACTION, 0,
	A2600GetZipName, a26_armorambRomInfo, a26_armorambRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Artillery Duel (USA)

static struct BurnRomInfo a26_artduelRomDesc[] = {
	{ "Artillery Duel (1983)(Xonox).bin", 0x02000, 0x1d0f7e8d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_artduel)
STD_ROM_FN(a26_artduel)

struct BurnDriver BurnDrva26_artduel = {
	"a26_artduel", NULL, NULL, NULL, "1983",
	"Artillery Duel (USA)\0", NULL, "Xonox", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_SHOOT, 0,
	A2600GetZipName, a26_artduelRomInfo, a26_artduelRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Assault (USA)

static struct BurnRomInfo a26_assaultRomDesc[] = {
	{ "Assault (1983)(Bomb).bin", 0x01000, 0xab3a2b77, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_assault)
STD_ROM_FN(a26_assault)

struct BurnDriver BurnDrva26_assault = {
	"a26_assault", NULL, NULL, NULL, "1983",
	"Assault (USA)\0", NULL, "Bomb", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_VERSHOOT, 0,
	A2600GetZipName, a26_assaultRomInfo, a26_assaultRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Asteroids (USA)

static struct BurnRomInfo a26_asteroidRomDesc[] = {
	{ "Asteroids (1981)(Atari).bin", 0x02000, 0xc7d64c94, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_asteroid)
STD_ROM_FN(a26_asteroid)

struct BurnDriver BurnDrva26_asteroid = {
	"a26_asteroid", NULL, NULL, NULL, "1981",
	"Asteroids (USA)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MULTISHOOT, 0,
	A2600GetZipName, a26_asteroidRomInfo, a26_asteroidRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Astroblast (USA)

static struct BurnRomInfo a26_astrblstRomDesc[] = {
	{ "Astroblast (1982)(Mattel Electronics).bin", 0x01000, 0x1de25331, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_astrblst)
STD_ROM_FN(a26_astrblst)

struct BurnDriver BurnDrva26_astrblst = {
	"a26_astrblst", NULL, NULL, NULL, "1982",
	"Astroblast (USA)\0", NULL, "Mattel Electronics", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_VERSHOOT, 0,
	A2600GetZipName, a26_astrblstRomInfo, a26_astrblstRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Astrowar (USA)

static struct BurnRomInfo a26_astrowarRomDesc[] = {
	{ "Astrowar (1983)(Starsoft).bin", 0x01000, 0x7b4b7eaf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_astrowar)
STD_ROM_FN(a26_astrowar)

struct BurnDriver BurnDrva26_astrowar = {
	"a26_astrowar", NULL, NULL, NULL, "1983",
	"Astrowar (USA)\0", NULL, "Starsoft", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_HORSHOOT, 0,
	A2600GetZipName, a26_astrowarRomInfo, a26_astrowarRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Atari Video Cube (USA)

static struct BurnRomInfo a26_avcRomDesc[] = {
	{ "Atari Video Cube (1982)(Atari).bin", 0x01000, 0x272ae37f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_avc)
STD_ROM_FN(a26_avc)

struct BurnDriver BurnDrva26_avc = {
	"a26_avc", NULL, NULL, NULL, "1982",
	"Atari Video Cube (USA)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_PUZZLE, 0,
	A2600GetZipName, a26_avcRomInfo, a26_avcRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Atlantis (USA)

static struct BurnRomInfo a26_atlantisRomDesc[] = {
	{ "Atlantis (1982)(Imagic).bin", 0x01000, 0x92560498, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_atlantis)
STD_ROM_FN(a26_atlantis)

struct BurnDriver BurnDrva26_atlantis = {
	"a26_atlantis", NULL, NULL, NULL, "1982",
	"Atlantis (USA)\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_SHOOT, 0,
	A2600GetZipName, a26_atlantisRomInfo, a26_atlantisRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Backgammon

static struct BurnRomInfo a26_backgammRomDesc[] = {
	{ "Backgammon (1979)(Atari).bin", 0x01000, 0xe2a7ffa6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_backgamm)
STD_ROM_FN(a26_backgamm)

struct BurnDriver BurnDrva26_backgamm = {
	"a26_backgamm", NULL, NULL, NULL, "1979",
	"Backgammon\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_backgammRomInfo, a26_backgammRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Bank Heist

static struct BurnRomInfo a26_bankheisRomDesc[] = {
	{ "Bank Heist (1983)(20th Century Fox).bin", 0x01000, 0x7aa79d1e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bankheis)
STD_ROM_FN(a26_bankheis)

struct BurnDriver BurnDrva26_bankheis = {
	"a26_bankheis", NULL, NULL, NULL, "1983",
	"Bank Heist\0", NULL, "20th Century Fox", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_bankheisRomInfo, a26_bankheisRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Barnstorming

static struct BurnRomInfo a26_barnstrmRomDesc[] = {
	{ "Barnstorming (1982)(Activision).bin", 0x01000, 0x4a005181, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_barnstrm)
STD_ROM_FN(a26_barnstrm)

struct BurnDriver BurnDrva26_barnstrm = {
	"a26_barnstrm", NULL, NULL, NULL, "1982",
	"Barnstorming\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_barnstrmRomInfo, a26_barnstrmRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Super Challenge Baseball (Mattel)

static struct BurnRomInfo a26_scbasbalRomDesc[] = {
	{ "Super Challenge Baseball (1978)(Mattel Electronics).bin", 0x01000, 0xf7e88ec2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_scbasbal)
STD_ROM_FN(a26_scbasbal)

struct BurnDriver BurnDrva26_scbasbal = {
	"a26_scbasbal", NULL, NULL, NULL, "1978",
	"Super Challenge Baseball (Mattel)\0", NULL, "Mattel Electronics", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_scbasbalRomInfo, a26_scbasbalRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Basketball

static struct BurnRomInfo a26_bsktballRomDesc[] = {
	{ "Basketball (1978)(Atari).bin", 0x00800, 0x711a2340, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bsktball)
STD_ROM_FN(a26_bsktball)

struct BurnDriver BurnDrva26_bsktball = {
	"a26_bsktball", NULL, NULL, NULL, "1978",
	"Basketball\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_bsktballRomInfo, a26_bsktballRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Battlezone (USA)

static struct BurnRomInfo a26_bzoneRomDesc[] = {
	{ "Battlezone (1983)(Atari).bin", 0x02000, 0x9155dd1d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bzone)
STD_ROM_FN(a26_bzone)

struct BurnDriver BurnDrva26_bzone = {
	"a26_bzone", NULL, NULL, NULL, "1983",
	"Battlezone (USA)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_SHOOT, 0,
	A2600GetZipName, a26_bzoneRomInfo, a26_bzoneRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Beamrider (USA)

static struct BurnRomInfo a26_beamridrRomDesc[] = {
	{ "Beamrider (1984)(Activision).bin", 0x02000, 0x1618565b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_beamridr)
STD_ROM_FN(a26_beamridr)

struct BurnDriver BurnDrva26_beamridr = {
	"a26_beamridr", NULL, NULL, NULL, "1984",
	"Beamrider (USA)\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ATARI2600, GBF_VERSHOOT, 0,
	A2600GetZipName, a26_beamridrRomInfo, a26_beamridrRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Beany Bopper (USA)

static struct BurnRomInfo a26_beanboppRomDesc[] = {
	{ "Beany Bopper (1983)(20th Century Fox).bin", 0x01000, 0xb722bc8f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_beanbopp)
STD_ROM_FN(a26_beanbopp)

struct BurnDriver BurnDrva26_beanbopp = {
	"a26_beanbopp", NULL, NULL, NULL, "1983",
	"Beany Bopper (USA)\0", NULL, "20th Century Fox", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_MULTISHOOT, 0,
	A2600GetZipName, a26_beanboppRomInfo, a26_beanboppRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Berenstain Bears

static struct BurnRomInfo a26_bernstinRomDesc[] = {
	{ "Berenstain Bears (1984)(Coleco).bin", 0x02000, 0x29d28baf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bernstin)
STD_ROM_FN(a26_bernstin)

struct BurnDriver BurnDrva26_bernstin = {
	"a26_bernstin", NULL, NULL, NULL, "1984",
	"Berenstain Bears\0", NULL, "Coleco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_bernstinRomInfo, a26_bernstinRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8_B1, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Bermuda (Rainbow Vision)

static struct BurnRomInfo a26_bermudaRomDesc[] = {
	{ "Bermuda (1983)(Rainbow Vision).bin", 0x01000, 0x5e5ff905, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bermuda)
STD_ROM_FN(a26_bermuda)

struct BurnDriver BurnDrva26_bermuda = {
	"a26_bermuda", NULL, NULL, NULL, "1983",
	"Bermuda (Rainbow Vision)\0", NULL, "Rainbow Vision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_bermudaRomInfo, a26_bermudaRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Bermuda Triangle

static struct BurnRomInfo a26_bermtriaRomDesc[] = {
	{ "Bermuda Triangle (1982)(Data Age).bin", 0x01000, 0x0fcb5b94, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bermtria)
STD_ROM_FN(a26_bermtria)

struct BurnDriver BurnDrva26_bermtria = {
	"a26_bermtria", NULL, NULL, NULL, "1982",
	"Bermuda Triangle\0", NULL, "Data Age", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_bermtriaRomInfo, a26_bermtriaRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Berzerk

static struct BurnRomInfo a26_berzerkRomDesc[] = {
	{ "Berzerk (1982)(Atari).bin", 0x01000, 0x2e8b4b5f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_berzerk)
STD_ROM_FN(a26_berzerk)

struct BurnDriver BurnDrva26_berzerk = {
	"a26_berzerk", NULL, NULL, NULL, "1982",
	"Berzerk\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_berzerkRomInfo, a26_berzerkRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Bi! Bi!

static struct BurnRomInfo a26_bibiRomDesc[] = {
	{ "Bi! Bi! (1983)(Starsoft).bin", 0x01000, 0x9c3a516b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bibi)
STD_ROM_FN(a26_bibi)

struct BurnDriver BurnDrva26_bibi = {
	"a26_bibi", NULL, NULL, NULL, "1983",
	"Bi! Bi!\0", NULL, "Starsoft", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_bibiRomInfo, a26_bibiRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K_PAL, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_PAL_H * A26_SCALE, 4, 3
};

// Blackjack - Black Jack

static struct BurnRomInfo a26_blckjackRomDesc[] = {
	{ "Blackjack (1977)(Atari).bin", 0x00800, 0x4b0ee010, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_blckjack)
STD_ROM_FN(a26_blckjack)

struct BurnDriver BurnDrva26_blckjack = {
	"a26_blckjack", NULL, NULL, NULL, "1977",
	"Blackjack - Black Jack\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_blckjackRomInfo, a26_blckjackRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_2K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Blueprint

static struct BurnRomInfo a26_blueprntRomDesc[] = {
	{ "Blueprint (1983)(CBS Electronics).bin", 0x02000, 0xda7b9dfa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_blueprnt)
STD_ROM_FN(a26_blueprnt)

struct BurnDriver BurnDrva26_blueprnt = {
	"a26_blueprnt", NULL, NULL, NULL, "1983",
	"Blueprint\0", NULL, "CBS Electronics", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_blueprntRomInfo, a26_blueprntRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// BMX Air Master (Atari)

static struct BurnRomInfo a26_bmxRomDesc[] = {
	{ "BMX Air Master (1990)(Atari).bin", 0x04000, 0xb4017ee3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bmx)
STD_ROM_FN(a26_bmx)

struct BurnDriver BurnDrva26_bmx = {
	"a26_bmx", NULL, NULL, NULL, "1990",
	"BMX Air Master (Atari)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_bmxRomInfo, a26_bmxRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Bob Is Going Home (JVP)

static struct BurnRomInfo a26_bobhomejRomDesc[] = {
	{ "Bob Is Going Home (19xx)(JVP).bin", 0x01000, 0x7d620b52, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bobhomej)
STD_ROM_FN(a26_bobhomej)

struct BurnDriver BurnDrva26_bobhomej = {
	"a26_bobhomej", NULL, NULL, NULL, "19??",
	"Bob Is Going Home (JVP)\0", NULL, "JVP", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_bobhomejRomInfo, a26_bobhomejRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Bogey Blaster

static struct BurnRomInfo a26_bogeyblaRomDesc[] = {
	{ "Bogey Blaster (1989)(Telegames).bin", 0x01000, 0xa44eb837, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bogeybla)
STD_ROM_FN(a26_bogeybla)

struct BurnDriver BurnDrva26_bogeybla = {
	"a26_bogeybla", NULL, NULL, NULL, "1989",
	"Bogey Blaster\0", NULL, "Telegames", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_bogeyblaRomInfo, a26_bogeyblaRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Boing!

static struct BurnRomInfo a26_boingRomDesc[] = {
	{ "Boing! (1983)(First Star Software).bin", 0x01000, 0xad380487, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_boing)
STD_ROM_FN(a26_boing)

struct BurnDriver BurnDrva26_boing = {
	"a26_boing", NULL, NULL, NULL, "1983",
	"Boing!\0", NULL, "First Star Software", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_boingRomInfo, a26_boingRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Boom Bang (CCE)

static struct BurnRomInfo a26_boombangRomDesc[] = {
	{ "Boom Bang (1983)(CCE).bin", 0x01000, 0xbb713c1c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_boombang)
STD_ROM_FN(a26_boombang)

struct BurnDriver BurnDrva26_boombang = {
	"a26_boombang", NULL, NULL, NULL, "1983",
	"Boom Bang (CCE)\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_boombangRomInfo, a26_boombangRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Bowling

static struct BurnRomInfo a26_bowlingRomDesc[] = {
	{ "Bowling (1979)(Atari).bin", 0x00800, 0xfea0d6d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bowling)
STD_ROM_FN(a26_bowling)

struct BurnDriver BurnDrva26_bowling = {
	"a26_bowling", NULL, NULL, NULL, "1979",
	"Bowling\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_bowlingRomInfo, a26_bowlingRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Boxing - La Boxe

static struct BurnRomInfo a26_boxingRomDesc[] = {
	{ "Boxing (1980)(Activision).bin", 0x00800, 0x9ece0021, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_boxing)
STD_ROM_FN(a26_boxing)

struct BurnDriver BurnDrva26_boxing = {
	"a26_boxing", NULL, NULL, NULL, "1980",
	"Boxing - La Boxe\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_boxingRomInfo, a26_boxingRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Breakout - Breakaway IV

static struct BurnRomInfo a26_breakoutRomDesc[] = {
	{ "Breakout (1978)(Atari).bin", 0x00800, 0x3037638c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_breakout)
STD_ROM_FN(a26_breakout)

struct BurnDriver BurnDrva26_breakout = {
	"a26_breakout", NULL, NULL, NULL, "1978",
	"Breakout - Breakaway IV\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_breakoutRomInfo, a26_breakoutRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_2K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Bridge

static struct BurnRomInfo a26_bridgeRomDesc[] = {
	{ "Bridge (1980)(Activision).bin", 0x01000, 0x9c71d175, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bridge)
STD_ROM_FN(a26_bridge)

struct BurnDriver BurnDrva26_bridge = {
	"a26_bridge", NULL, NULL, NULL, "1980",
	"Bridge\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_bridgeRomInfo, a26_bridgeRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Buck Rogers: Planet of Zoom (USA)

static struct BurnRomInfo a26_buckrogRomDesc[] = {
	{ "Buck Rogers - Planet of Zoom (1983)(Sega).bin", 0x02000, 0x2030f686, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_buckrog)
STD_ROM_FN(a26_buckrog)

struct BurnDriver BurnDrva26_buckrog = {
	"a26_buckrog", NULL, NULL, NULL, "1983",
	"Buck Rogers: Planet of Zoom (USA)\0", NULL, "Sega", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_SHOOT, 0,
	A2600GetZipName, a26_buckrogRomInfo, a26_buckrogRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Bugs (USA)

static struct BurnRomInfo a26_bugsRomDesc[] = {
	{ "Bugs (1982)(Data Age).bin", 0x01000, 0x1202427c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bugs)
STD_ROM_FN(a26_bugs)

struct BurnDriver BurnDrva26_bugs = {
	"a26_bugs", NULL, NULL, NULL, "1982",
	"Bugs (USA)\0", NULL, "Data Age", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_SHOOT, 0,
	A2600GetZipName, a26_bugsRomInfo, a26_bugsRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Bump 'n' Jump (USA)

static struct BurnRomInfo a26_bnjRomDesc[] = {
	{ "Bump 'n' Jump (1983)(Telegames).bin", 0x04000, 0xdf2bc303, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bnj)
STD_ROM_FN(a26_bnj)

struct BurnDriver BurnDrva26_bnj = {
	"a26_bnj", NULL, NULL, NULL, "1983",
	"Bump 'n' Jump (USA)\0", NULL, "Telegames", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_ACTION, 0,
	A2600GetZipName, a26_bnjRomInfo, a26_bnjRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E7, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Bumper Bash

static struct BurnRomInfo a26_bumpbashRomDesc[] = {
	{ "Bumper Bash (1983)(Spectravideo).bin", 0x01000, 0xb1a6a23e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_bumpbash)
STD_ROM_FN(a26_bumpbash)

struct BurnDriver BurnDrva26_bumpbash = {
	"a26_bumpbash", NULL, NULL, NULL, "1983",
	"Bumper Bash\0", NULL, "Spectravideo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_bumpbashRomInfo, a26_bumpbashRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// BurgerTime

static struct BurnRomInfo a26_btimeRomDesc[] = {
	{ "BurgerTime (1983)(M Network).bin", 0x04000, 0xc183fbbc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_btime)
STD_ROM_FN(a26_btime)

struct BurnDriver BurnDrva26_btime = {
	"a26_btime", NULL, NULL, NULL, "1983",
	"BurgerTime\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_btimeRomInfo, a26_btimeRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E7, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Busy Police

static struct BurnRomInfo a26_busypolRomDesc[] = {
	{ "Busy Police (19xx)(Zellers).bin", 0x01000, 0xdb1122e6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_busypol)
STD_ROM_FN(a26_busypol)

struct BurnDriver BurnDrva26_busypol = {
	"a26_busypol", NULL, NULL, NULL, "19??",
	"Busy Police\0", NULL, "Zellers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_busypolRomInfo, a26_busypolRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Cakewalk

static struct BurnRomInfo a26_cakewalkRomDesc[] = {
	{ "Cakewalk (1983)(CommaVid).bin", 0x01000, 0x73494211, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_cakewalk)
STD_ROM_FN(a26_cakewalk)

struct BurnDriver BurnDrva26_cakewalk = {
	"a26_cakewalk", NULL, NULL, NULL, "1983",
	"Cakewalk\0", NULL, "CommaVid", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_cakewalkRomInfo, a26_cakewalkRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// California Games

static struct BurnRomInfo a26_calgamesRomDesc[] = {
	{ "California Games (1988)(Epyx).bin", 0x04000, 0xe9a3fdc3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_calgames)
STD_ROM_FN(a26_calgames)

struct BurnDriver BurnDrva26_calgames = {
	"a26_calgames", NULL, NULL, NULL, "1988",
	"California Games\0", NULL, "Epyx", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_calgamesRomInfo, a26_calgamesRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Canyon Bomber

static struct BurnRomInfo a26_canyonbRomDesc[] = {
	{ "Canyon Bomber (1979)(Atari).bin", 0x00800, 0xe914b8ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_canyonb)
STD_ROM_FN(a26_canyonb)

struct BurnDriver BurnDrva26_canyonb = {
	"a26_canyonb", NULL, NULL, NULL, "1979",
	"Canyon Bomber\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_canyonbRomInfo, a26_canyonbRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_2K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Carnival

static struct BurnRomInfo a26_carnivalRomDesc[] = {
	{ "Carnival (1982)(Coleco).bin", 0x01000, 0x7ac3c700, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_carnival)
STD_ROM_FN(a26_carnival)

struct BurnDriver BurnDrva26_carnival = {
	"a26_carnival", NULL, NULL, NULL, "1982",
	"Carnival\0", NULL, "Coleco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_carnivalRomInfo, a26_carnivalRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Casino - Poker Plus

static struct BurnRomInfo a26_casinoRomDesc[] = {
	{ "Casino (1979)(Atari).bin", 0x01000, 0x420b8248, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_casino)
STD_ROM_FN(a26_casino)

struct BurnDriver BurnDrva26_casino = {
	"a26_casino", NULL, NULL, NULL, "1979",
	"Casino - Poker Plus\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_casinoRomInfo, a26_casinoRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Centipede

static struct BurnRomInfo a26_centipedRomDesc[] = {
	{ "Centipede (1983)(Atari).bin", 0x02000, 0x77396102, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_centiped)
STD_ROM_FN(a26_centiped)

struct BurnDriver BurnDrva26_centiped = {
	"a26_centiped", NULL, NULL, NULL, "1983",
	"Centipede\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_centipedRomInfo, a26_centipedRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Challenge (unknown)

static struct BurnRomInfo a26_challengRomDesc[] = {
	{ "Challenge (19xx)(Unknown).bin", 0x01000, 0x086a0cb2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_challeng)
STD_ROM_FN(a26_challeng)

struct BurnDriver BurnDrva26_challeng = {
	"a26_challeng", NULL, NULL, NULL, "19??",
	"Challenge (unknown)\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_challengRomInfo, a26_challengRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// The Challenge of.... Nexar

static struct BurnRomInfo a26_nexarRomDesc[] = {
	{ "The Challenge of.... Nexar (1982)(Spectravideo).bin", 0x01000, 0x74cc37d4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_nexar)
STD_ROM_FN(a26_nexar)

struct BurnDriver BurnDrva26_nexar = {
	"a26_nexar", NULL, NULL, NULL, "1982",
	"The Challenge of.... Nexar\0", NULL, "Spectravideo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_nexarRomInfo, a26_nexarRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Championship Soccer - Soccer

static struct BurnRomInfo a26_chmpsocrRomDesc[] = {
	{ "Championship Soccer (1980)(Atari).bin", 0x01000, 0x3f59bb60, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_chmpsocr)
STD_ROM_FN(a26_chmpsocr)

struct BurnDriver BurnDrva26_chmpsocr = {
	"a26_chmpsocr", NULL, NULL, NULL, "1980",
	"Championship Soccer - Soccer\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_chmpsocrRomInfo, a26_chmpsocrRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Chase the Chuckwagon

static struct BurnRomInfo a26_chaschukRomDesc[] = {
	{ "Chase the Chuckwagon (1983)(Spectravideo).bin", 0x01000, 0x63e4fc7f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_chaschuk)
STD_ROM_FN(a26_chaschuk)

struct BurnDriver BurnDrva26_chaschuk = {
	"a26_chaschuk", NULL, NULL, NULL, "1983",
	"Chase the Chuckwagon\0", NULL, "Spectravideo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_chaschukRomInfo, a26_chaschukRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Checkers

static struct BurnRomInfo a26_checkersRomDesc[] = {
	{ "Checkers (1980)(Activision).bin", 0x00800, 0xe74f1b59, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_checkers)
STD_ROM_FN(a26_checkers)

struct BurnDriver BurnDrva26_checkers = {
	"a26_checkers", NULL, NULL, NULL, "1980",
	"Checkers\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_checkersRomInfo, a26_checkersRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// China Syndrome

static struct BurnRomInfo a26_chinasynRomDesc[] = {
	{ "China Syndrome (1982)(Spectravideo).bin", 0x01000, 0xa714e79c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_chinasyn)
STD_ROM_FN(a26_chinasyn)

struct BurnDriver BurnDrva26_chinasyn = {
	"a26_chinasyn", NULL, NULL, NULL, "1982",
	"China Syndrome\0", NULL, "Spectravideo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_chinasynRomInfo, a26_chinasynRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Chopper Command

static struct BurnRomInfo a26_chopcommRomDesc[] = {
	{ "Chopper Command (1982)(Activision).bin", 0x01000, 0x1dfa64df, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_chopcomm)
STD_ROM_FN(a26_chopcomm)

struct BurnDriver BurnDrva26_chopcomm = {
	"a26_chopcomm", NULL, NULL, NULL, "1982",
	"Chopper Command\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_chopcommRomInfo, a26_chopcommRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Chuck Norris Superkicks

static struct BurnRomInfo a26_chucknorRomDesc[] = {
	{ "Chuck Norris Superkicks (1983)(Xonox).bin", 0x02000, 0x96210c6c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_chucknor)
STD_ROM_FN(a26_chucknor)

struct BurnDriver BurnDrva26_chucknor = {
	"a26_chucknor", NULL, NULL, NULL, "1983",
	"Chuck Norris Superkicks\0", NULL, "Xonox", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_chucknorRomInfo, a26_chucknorRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Circus Atari - Circus

static struct BurnRomInfo a26_circusaRomDesc[] = {
	{ "Circus Atari (1980)(Atari).bin", 0x01000, 0xd2a330a3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_circusa)
STD_ROM_FN(a26_circusa)

struct BurnDriver BurnDrva26_circusa = {
	"a26_circusa", NULL, NULL, NULL, "1980",
	"Circus Atari - Circus\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_circusaRomInfo, a26_circusaRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Coco Nuts

static struct BurnRomInfo a26_coconutsRomDesc[] = {
	{ "Coco Nuts (1982)(Telesys).bin", 0x01000, 0xb3e44e5c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_coconuts)
STD_ROM_FN(a26_coconuts)

struct BurnDriver BurnDrva26_coconuts = {
	"a26_coconuts", NULL, NULL, NULL, "1982",
	"Coco Nuts\0", NULL, "Telesys", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_coconutsRomInfo, a26_coconutsRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Color Bar Generator

static struct BurnRomInfo a26_colorbarRomDesc[] = {
	{ "Color Bar Generator (1984)(Video Soft).bin", 0x01000, 0x19b52142, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_colorbar)
STD_ROM_FN(a26_colorbar)

struct BurnDriver BurnDrva26_colorbar = {
	"a26_colorbar", NULL, NULL, NULL, "1984",
	"Color Bar Generator\0", NULL, "Video Soft", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_colorbarRomInfo, a26_colorbarRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Comando Suicida

static struct BurnRomInfo a26_comandosRomDesc[] = {
	{ "Comando Suicida (19xx)(Dismac).bin", 0x01000, 0xa56d1a00, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_comandos)
STD_ROM_FN(a26_comandos)

struct BurnDriver BurnDrva26_comandos = {
	"a26_comandos", NULL, NULL, NULL, "19??",
	"Comando Suicida\0", NULL, "Dismac", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_comandosRomInfo, a26_comandosRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Combat - Tank-Plus

static struct BurnRomInfo a26_combatRomDesc[] = {
	{ "Combat (1977)(Atari).bin", 0x00800, 0x9c326a97, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_combat)
STD_ROM_FN(a26_combat)

struct BurnDriver BurnDrva26_combat = {
	"a26_combat", NULL, NULL, NULL, "1977",
	"Combat - Tank-Plus\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_combatRomInfo, a26_combatRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Commando

static struct BurnRomInfo a26_commandoRomDesc[] = {
	{ "Commando (1988)(Activision).bin", 0x04000, 0x8d3025dc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_commando)
STD_ROM_FN(a26_commando)

struct BurnDriver BurnDrva26_commando = {
	"a26_commando", NULL, NULL, NULL, "1988",
	"Commando\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_commandoRomInfo, a26_commandoRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Commando Raid

static struct BurnRomInfo a26_commraidRomDesc[] = {
	{ "Commando Raid (1982)(U.S. Games Corporation).bin", 0x01000, 0xe46583e6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_commraid)
STD_ROM_FN(a26_commraid)

struct BurnDriver BurnDrva26_commraid = {
	"a26_commraid", NULL, NULL, NULL, "1982",
	"Commando Raid\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_commraidRomInfo, a26_commraidRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Condor Attack

static struct BurnRomInfo a26_condoratRomDesc[] = {
	{ "Condor Attack (1982)(Ultravision).bin", 0x01000, 0xb6c582eb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_condorat)
STD_ROM_FN(a26_condorat)

struct BurnDriver BurnDrva26_condorat = {
	"a26_condorat", NULL, NULL, NULL, "1982",
	"Condor Attack\0", NULL, "Ultravision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_condoratRomInfo, a26_condoratRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Congo Bongo (USA)

static struct BurnRomInfo a26_congoRomDesc[] = {
	{ "Congo Bongo (1983)(Sega).bin", 0x02000, 0x3f6e7e0c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_congo)
STD_ROM_FN(a26_congo)

struct BurnDriver BurnDrva26_congo = {
	"a26_congo", NULL, NULL, NULL, "1983",
	"Congo Bongo (USA)\0", NULL, "Sega", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_congoRomInfo, a26_congoRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8_B1, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Corrida da Matematica

static struct BurnRomInfo a26_corridaRomDesc[] = {
	{ "Corrida da Matematica (19xx)(CCE).bin", 0x01000, 0xff570220, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_corrida)
STD_ROM_FN(a26_corrida)

struct BurnDriver BurnDrva26_corrida = {
	"a26_corrida", NULL, NULL, NULL, "19??",
	"Corrida da Matematica\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_corridaRomInfo, a26_corridaRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Cosmic Ark (Imagic)

static struct BurnRomInfo a26_cosmarkRomDesc[] = {
	{ "Cosmic Ark (1982)(Imagic).bin", 0x01000, 0xcb374f0e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_cosmark)
STD_ROM_FN(a26_cosmark)

struct BurnDriver BurnDrva26_cosmark = {
	"a26_cosmark", NULL, NULL, NULL, "1982",
	"Cosmic Ark (Imagic)\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_cosmarkRomInfo, a26_cosmarkRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Cosmic Commuter

static struct BurnRomInfo a26_cosmiccoRomDesc[] = {
	{ "Cosmic Commuter (1984)(Activision).bin", 0x01000, 0x4939ba40, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_cosmicco)
STD_ROM_FN(a26_cosmicco)

struct BurnDriver BurnDrva26_cosmicco = {
	"a26_cosmicco", NULL, NULL, NULL, "1984",
	"Cosmic Commuter\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_cosmiccoRomInfo, a26_cosmiccoRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Cosmic Corridor

static struct BurnRomInfo a26_cosmcorrRomDesc[] = {
	{ "Cosmic Corridor (1983)(Vidco).bin", 0x01000, 0x2511afc4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_cosmcorr)
STD_ROM_FN(a26_cosmcorr)

struct BurnDriver BurnDrva26_cosmcorr = {
	"a26_cosmcorr", NULL, NULL, NULL, "1983",
	"Cosmic Corridor\0", NULL, "Vidco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_cosmcorrRomInfo, a26_cosmcorrRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Cosmic Creeps

static struct BurnRomInfo a26_cosmcrepRomDesc[] = {
	{ "Cosmic Creeps (1982)(Telesys).bin", 0x01000, 0x47aaa2dd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_cosmcrep)
STD_ROM_FN(a26_cosmcrep)

struct BurnDriver BurnDrva26_cosmcrep = {
	"a26_cosmcrep", NULL, NULL, NULL, "1982",
	"Cosmic Creeps\0", NULL, "Telesys", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_cosmcrepRomInfo, a26_cosmcrepRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Cosmic Swarm

static struct BurnRomInfo a26_cosmswrmRomDesc[] = {
	{ "Cosmic Swarm (1982)(CommaVid).bin", 0x00800, 0x27f1ae48, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_cosmswrm)
STD_ROM_FN(a26_cosmswrm)

struct BurnDriver BurnDrva26_cosmswrm = {
	"a26_cosmswrm", NULL, NULL, NULL, "1982",
	"Cosmic Swarm\0", NULL, "CommaVid", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_cosmswrmRomInfo, a26_cosmswrmRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Crash Dive

static struct BurnRomInfo a26_crashdivRomDesc[] = {
	{ "Crash Dive (1983)(20th Century Fox Video Games).bin", 0x01000, 0x59fe4666, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_crashdiv)
STD_ROM_FN(a26_crashdiv)

struct BurnDriver BurnDrva26_crashdiv = {
	"a26_crashdiv", NULL, NULL, NULL, "1983",
	"Crash Dive\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_crashdivRomInfo, a26_crashdivRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Crazy Climber

static struct BurnRomInfo a26_cclimberRomDesc[] = {
	{ "Crazy Climber (1982)(Atari).bin", 0x02000, 0xa3a3c009, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_cclimber)
STD_ROM_FN(a26_cclimber)

struct BurnDriver BurnDrva26_cclimber = {
	"a26_cclimber", NULL, NULL, NULL, "1982",
	"Crazy Climber\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_cclimberRomInfo, a26_cclimberRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Cross Force

static struct BurnRomInfo a26_xforceRomDesc[] = {
	{ "Cross Force (1982)(Spectravideo).bin", 0x01000, 0x41b0d615, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_xforce)
STD_ROM_FN(a26_xforce)

struct BurnDriver BurnDrva26_xforce = {
	"a26_xforce", NULL, NULL, NULL, "1982",
	"Cross Force\0", NULL, "Spectravideo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_xforceRomInfo, a26_xforceRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Crossbow

static struct BurnRomInfo a26_crossbowRomDesc[] = {
	{ "Crossbow (1987)(Atari).bin", 0x04000, 0x1f233140, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_crossbow)
STD_ROM_FN(a26_crossbow)

struct BurnDriver BurnDrva26_crossbow = {
	"a26_crossbow", NULL, NULL, NULL, "1987",
	"Crossbow\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_crossbowRomInfo, a26_crossbowRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Cruise Missile

static struct BurnRomInfo a26_cruisemiRomDesc[] = {
	{ "Cruise Missile (1987)(Froggo).bin", 0x01000, 0x5161a246, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_cruisemi)
STD_ROM_FN(a26_cruisemi)

struct BurnDriver BurnDrva26_cruisemi = {
	"a26_cruisemi", NULL, NULL, NULL, "1987",
	"Cruise Missile\0", NULL, "Froggo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_cruisemiRomInfo, a26_cruisemiRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Crypts of Chaos

static struct BurnRomInfo a26_cryptschRomDesc[] = {
	{ "Crypts of Chaos (1982)(20th Century Fox Video Games).bin", 0x01000, 0xc5b29962, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_cryptsch)
STD_ROM_FN(a26_cryptsch)

struct BurnDriver BurnDrva26_cryptsch = {
	"a26_cryptsch", NULL, NULL, NULL, "1982",
	"Crypts of Chaos\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_cryptschRomInfo, a26_cryptschRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Crystal Castles

static struct BurnRomInfo a26_ccastlesRomDesc[] = {
	{ "Crystal Castles (1984)(Atari).bin", 0x04000, 0x9007b5ac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_ccastles)
STD_ROM_FN(a26_ccastles)

struct BurnDriver BurnDrva26_ccastles = {
	"a26_ccastles", NULL, NULL, NULL, "1984",
	"Crystal Castles\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_ccastlesRomInfo, a26_ccastlesRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Cubo Magico

static struct BurnRomInfo a26_cubomagiRomDesc[] = {
	{ "Cubo Magico (19xx)(CCE).bin", 0x01000, 0xb053bee5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_cubomagi)
STD_ROM_FN(a26_cubomagi)

struct BurnDriver BurnDrva26_cubomagi = {
	"a26_cubomagi", NULL, NULL, NULL, "19??",
	"Cubo Magico\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_cubomagiRomInfo, a26_cubomagiRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Dark Cavern

static struct BurnRomInfo a26_darkcaveRomDesc[] = {
	{ "Dark Cavern (1982)(M Network).bin", 0x01000, 0x0481078c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_darkcave)
STD_ROM_FN(a26_darkcave)

struct BurnDriver BurnDrva26_darkcave = {
	"a26_darkcave", NULL, NULL, NULL, "1982",
	"Dark Cavern\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_darkcaveRomInfo, a26_darkcaveRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Dark Chambers

static struct BurnRomInfo a26_darkchamRomDesc[] = {
	{ "Dark Chambers (1988)(Atari).bin", 0x04000, 0x83900281, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_darkcham)
STD_ROM_FN(a26_darkcham)

struct BurnDriver BurnDrva26_darkcham = {
	"a26_darkcham", NULL, NULL, NULL, "1988",
	"Dark Chambers\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_darkchamRomInfo, a26_darkchamRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Deadly Duck

static struct BurnRomInfo a26_deadlyduRomDesc[] = {
	{ "Deadly Duck (1982)(20th Century Fox Video Games).bin", 0x01000, 0xf33a9a2d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_deadlydu)
STD_ROM_FN(a26_deadlydu)

struct BurnDriver BurnDrva26_deadlydu = {
	"a26_deadlydu", NULL, NULL, NULL, "1982",
	"Deadly Duck\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_deadlyduRomInfo, a26_deadlyduRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Death Trap

static struct BurnRomInfo a26_deathtrpRomDesc[] = {
	{ "Death Trap (1983)(Avalon Hill).bin", 0x01000, 0xfdf79a60, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_deathtrp)
STD_ROM_FN(a26_deathtrp)

struct BurnDriver BurnDrva26_deathtrp = {
	"a26_deathtrp", NULL, NULL, NULL, "1983",
	"Death Trap\0", NULL, "Avalon Hill", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_deathtrpRomInfo, a26_deathtrpRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Defender

static struct BurnRomInfo a26_defenderRomDesc[] = {
	{ "Defender (1982)(Atari).bin", 0x01000, 0x0df43d8e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_defender)
STD_ROM_FN(a26_defender)

struct BurnDriver BurnDrva26_defender = {
	"a26_defender", NULL, NULL, NULL, "1982",
	"Defender\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_defenderRomInfo, a26_defenderRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Defender II

static struct BurnRomInfo a26_defendr2RomDesc[] = {
	{ "Defender II (1988)(Atari).bin", 0x02000, 0x93217704, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_defendr2)
STD_ROM_FN(a26_defendr2)

struct BurnDriver BurnDrva26_defendr2 = {
	"a26_defendr2", NULL, NULL, NULL, "1988",
	"Defender II\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_defendr2RomInfo, a26_defendr2RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Demolition Herby

static struct BurnRomInfo a26_demohrbyRomDesc[] = {
	{ "Demolition Herby (1983)(Telesys).bin", 0x01000, 0xe9305ae2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_demohrby)
STD_ROM_FN(a26_demohrby)

struct BurnDriver BurnDrva26_demohrby = {
	"a26_demohrby", NULL, NULL, NULL, "1983",
	"Demolition Herby\0", NULL, "Telesys", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_demohrbyRomInfo, a26_demohrbyRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Demon Attack

static struct BurnRomInfo a26_demonatkRomDesc[] = {
	{ "Demon Attack (1982)(Imagic).bin", 0x01000, 0x818927c1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_demonatk)
STD_ROM_FN(a26_demonatk)

struct BurnDriver BurnDrva26_demonatk = {
	"a26_demonatk", NULL, NULL, NULL, "1982",
	"Demon Attack\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_demonatkRomInfo, a26_demonatkRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Demons to Diamonds

static struct BurnRomInfo a26_demodiamRomDesc[] = {
	{ "Demons to Diamonds (1982)(Atari).bin", 0x01000, 0x9b97c3da, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_demodiam)
STD_ROM_FN(a26_demodiam)

struct BurnDriver BurnDrva26_demodiam = {
	"a26_demodiam", NULL, NULL, NULL, "1982",
	"Demons to Diamonds\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_demodiamRomInfo, a26_demodiamRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Desert Falcon

static struct BurnRomInfo a26_dsrtflcnRomDesc[] = {
	{ "Desert Falcon (1987)(Atari).bin", 0x04000, 0xcaa0054e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_dsrtflcn)
STD_ROM_FN(a26_dsrtflcn)

struct BurnDriver BurnDrva26_dsrtflcn = {
	"a26_dsrtflcn", NULL, NULL, NULL, "1987",
	"Desert Falcon\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_dsrtflcnRomInfo, a26_dsrtflcnRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Dice Puzzle

static struct BurnRomInfo a26_dicepuzlRomDesc[] = {
	{ "Dice Puzzle (1983)(Panda).bin", 0x01000, 0x8d05d61b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_dicepuzl)
STD_ROM_FN(a26_dicepuzl)

struct BurnDriver BurnDrva26_dicepuzl = {
	"a26_dicepuzl", NULL, NULL, NULL, "1983",
	"Dice Puzzle\0", NULL, "Panda", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_dicepuzlRomInfo, a26_dicepuzlRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Dig Dug

static struct BurnRomInfo a26_digdugRomDesc[] = {
	{ "Dig Dug (1983)(Atari).bin", 0x04000, 0xee7b80d1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_digdug)
STD_ROM_FN(a26_digdug)

struct BurnDriver BurnDrva26_digdug = {
	"a26_digdug", NULL, NULL, NULL, "1983",
	"Dig Dug\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_digdugRomInfo, a26_digdugRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Dishaster

static struct BurnRomInfo a26_dishastrRomDesc[] = {
	{ "Dishaster (1983)(Vidco).bin", 0x01000, 0xc8150316, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_dishastr)
STD_ROM_FN(a26_dishastr)

struct BurnDriver BurnDrva26_dishastr = {
	"a26_dishastr", NULL, NULL, NULL, "1983",
	"Dishaster\0", NULL, "Vidco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_dishastrRomInfo, a26_dishastrRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Dodge 'Em - Dodger Cars

static struct BurnRomInfo a26_dodgeemRomDesc[] = {
	{ "Dodge 'Em (1980)(Atari).bin", 0x01000, 0xbc3602b5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_dodgeem)
STD_ROM_FN(a26_dodgeem)

struct BurnDriver BurnDrva26_dodgeem = {
	"a26_dodgeem", NULL, NULL, NULL, "1980",
	"Dodge 'Em - Dodger Cars\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_dodgeemRomInfo, a26_dodgeemRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Dolphin

static struct BurnRomInfo a26_dolphinRomDesc[] = {
	{ "Dolphin (1983)(Activision).bin", 0x01000, 0x9e38a7b9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_dolphin)
STD_ROM_FN(a26_dolphin)

struct BurnDriver BurnDrva26_dolphin = {
	"a26_dolphin", NULL, NULL, NULL, "1983",
	"Dolphin\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_dolphinRomInfo, a26_dolphinRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Donkey Kong (USA)

static struct BurnRomInfo a26_dkongRomDesc[] = {
	{ "Donkey Kong (1987)(Atari).bin", 0x01000, 0xf331b069, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_dkong)
STD_ROM_FN(a26_dkong)

struct BurnDriver BurnDrva26_dkong = {
	"a26_dkong", NULL, NULL, NULL, "1987",
	"Donkey Kong (USA)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_dkongRomInfo, a26_dkongRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Donkey Kong Junior (USA)

static struct BurnRomInfo a26_dkongjrRomDesc[] = {
	{ "Donkey Kong Junior (1987)(Atari).bin", 0x02000, 0x9ef649e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_dkongjr)
STD_ROM_FN(a26_dkongjr)

struct BurnDriver BurnDrva26_dkongjr = {
	"a26_dkongjr", NULL, NULL, NULL, "1987",
	"Donkey Kong Junior (USA)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_dkongjrRomInfo, a26_dkongjrRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Double Dragon

static struct BurnRomInfo a26_ddragonRomDesc[] = {
	{ "Double Dragon (1989)(Activision).bin", 0x04000, 0x8320bb37, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_ddragon)
STD_ROM_FN(a26_ddragon)

struct BurnDriver BurnDrva26_ddragon = {
	"a26_ddragon", NULL, NULL, NULL, "1989",
	"Double Dragon\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_ddragonRomInfo, a26_ddragonRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Double Dunk

static struct BurnRomInfo a26_ddunkRomDesc[] = {
	{ "Double Dunk (1989)(Atari).bin", 0x04000, 0x208f6c20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_ddunk)
STD_ROM_FN(a26_ddunk)

struct BurnDriver BurnDrva26_ddunk = {
	"a26_ddunk", NULL, NULL, NULL, "1989",
	"Double Dunk\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_ddunkRomInfo, a26_ddunkRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Dragonfire

static struct BurnRomInfo a26_dragfireRomDesc[] = {
	{ "Dragonfire (1982)(Imagic).bin", 0x01000, 0xba5dc02c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_dragfire)
STD_ROM_FN(a26_dragfire)

struct BurnDriver BurnDrva26_dragfire = {
	"a26_dragfire", NULL, NULL, NULL, "1982",
	"Dragonfire\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_dragfireRomInfo, a26_dragfireRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Dragster

static struct BurnRomInfo a26_dragsterRomDesc[] = {
	{ "Dragster (1980)(Activision).bin", 0x00800, 0x4042d3ab, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_dragster)
STD_ROM_FN(a26_dragster)

struct BurnDriver BurnDrva26_dragster = {
	"a26_dragster", NULL, NULL, NULL, "1980",
	"Dragster\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_dragsterRomInfo, a26_dragsterRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Dukes of Hazzard

static struct BurnRomInfo a26_hazzardRomDesc[] = {
	{ "Dukes of Hazzard (1983)(Atari).bin", 0x04000, 0x2db406dc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_hazzard)
STD_ROM_FN(a26_hazzard)

struct BurnDriver BurnDrva26_hazzard = {
	"a26_hazzard", NULL, NULL, NULL, "1983",
	"Dukes of Hazzard\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_hazzardRomInfo, a26_hazzardRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// E.T. - The Extra-Terrestrial

static struct BurnRomInfo a26_etRomDesc[] = {
	{ "E.T. - The Extra-Terrestrial (1982)(Atari).bin", 0x02000, 0x6d0a475f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_et)
STD_ROM_FN(a26_et)

struct BurnDriver BurnDrva26_et = {
	"a26_et", NULL, NULL, NULL, "1982",
	"E.T. - The Extra-Terrestrial\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_etRomInfo, a26_etRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Earth Attack

static struct BurnRomInfo a26_earthattRomDesc[] = {
	{ "Earth Attack (19xx)(Zellers).bin", 0x01000, 0x82b2f050, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_earthatt)
STD_ROM_FN(a26_earthatt)

struct BurnDriver BurnDrva26_earthatt = {
	"a26_earthatt", NULL, NULL, NULL, "19??",
	"Earth Attack\0", NULL, "Zellers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_earthattRomInfo, a26_earthattRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// The Earth Dies Screaming

static struct BurnRomInfo a26_earthdieRomDesc[] = {
	{ "The Earth Dies Screaming (1983)(20th Century Fox Video Games).bin", 0x01000, 0x8a6c65f0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_earthdie)
STD_ROM_FN(a26_earthdie)

struct BurnDriver BurnDrva26_earthdie = {
	"a26_earthdie", NULL, NULL, NULL, "1983",
	"The Earth Dies Screaming\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_earthdieRomInfo, a26_earthdieRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Eggomania

static struct BurnRomInfo a26_eggomaniRomDesc[] = {
	{ "Eggomania (1982)(U.S. Games Corporation).bin", 0x01000, 0xc78ec927, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_eggomani)
STD_ROM_FN(a26_eggomani)

struct BurnDriver BurnDrva26_eggomani = {
	"a26_eggomani", NULL, NULL, NULL, "1982",
	"Eggomania\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_eggomaniRomInfo, a26_eggomaniRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Eli's Ladder

static struct BurnRomInfo a26_eliladdrRomDesc[] = {
	{ "Eli's Ladder (1982)(Simage).bin", 0x01000, 0x2c5e6932, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_eliladdr)
STD_ROM_FN(a26_eliladdr)

struct BurnDriver BurnDrva26_eliladdr = {
	"a26_eliladdr", NULL, NULL, NULL, "1982",
	"Eli's Ladder\0", NULL, "Simage", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_eliladdrRomInfo, a26_eliladdrRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Encounter at L-5

static struct BurnRomInfo a26_encountrRomDesc[] = {
	{ "Encounter at L-5 (1982)(Data Age).bin", 0x01000, 0xe78fe552, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_encountr)
STD_ROM_FN(a26_encountr)

struct BurnDriver BurnDrva26_encountr = {
	"a26_encountr", NULL, NULL, NULL, "1982",
	"Encounter at L-5\0", NULL, "Data Age", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_encountrRomInfo, a26_encountrRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Enduro

static struct BurnRomInfo a26_enduroRomDesc[] = {
	{ "Enduro (1983)(Activision).bin", 0x01000, 0x2ef17b2e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_enduro)
STD_ROM_FN(a26_enduro)

struct BurnDriver BurnDrva26_enduro = {
	"a26_enduro", NULL, NULL, NULL, "1983",
	"Enduro\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_enduroRomInfo, a26_enduroRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Entombed

static struct BurnRomInfo a26_entombedRomDesc[] = {
	{ "Entombed (1982)(U.S. Games Corporation).bin", 0x01000, 0xe48ec9dc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_entombed)
STD_ROM_FN(a26_entombed)

struct BurnDriver BurnDrva26_entombed = {
	"a26_entombed", NULL, NULL, NULL, "1982",
	"Entombed\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_entombedRomInfo, a26_entombedRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Espial

static struct BurnRomInfo a26_espialRomDesc[] = {
	{ "Espial (1984)(Tigervision).bin", 0x02000, 0x1f95351a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_espial)
STD_ROM_FN(a26_espial)

struct BurnDriver BurnDrva26_espial = {
	"a26_espial", NULL, NULL, NULL, "1984",
	"Espial\0", NULL, "Tigervision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_espialRomInfo, a26_espialRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_3F, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Exocet

static struct BurnRomInfo a26_exocetRomDesc[] = {
	{ "Exocet (1983)(Panda).bin", 0x01000, 0xfd424b96, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_exocet)
STD_ROM_FN(a26_exocet)

struct BurnDriver BurnDrva26_exocet = {
	"a26_exocet", NULL, NULL, NULL, "1983",
	"Exocet\0", NULL, "Panda", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_exocetRomInfo, a26_exocetRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Fantastic Voyage

static struct BurnRomInfo a26_fvoyageRomDesc[] = {
	{ "Fantastic Voyage (1982)(20th Century Fox Video Games).bin", 0x01000, 0xa60e153f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_fvoyage)
STD_ROM_FN(a26_fvoyage)

struct BurnDriver BurnDrva26_fvoyage = {
	"a26_fvoyage", NULL, NULL, NULL, "1982",
	"Fantastic Voyage\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_fvoyageRomInfo, a26_fvoyageRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Farmyard Fun

static struct BurnRomInfo a26_farmyardRomDesc[] = {
	{ "Farmyard Fun (19xx)(Unknown).bin", 0x01000, 0x73474b03, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_farmyard)
STD_ROM_FN(a26_farmyard)

struct BurnDriver BurnDrva26_farmyard = {
	"a26_farmyard", NULL, NULL, NULL, "19??",
	"Farmyard Fun\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_farmyardRomInfo, a26_farmyardRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Fast Eddie

static struct BurnRomInfo a26_fasteddiRomDesc[] = {
	{ "Fast Eddie (1982)(20th Century Fox Video Games).bin", 0x01000, 0x0c0a7a02, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_fasteddi)
STD_ROM_FN(a26_fasteddi)

struct BurnDriver BurnDrva26_fasteddi = {
	"a26_fasteddi", NULL, NULL, NULL, "1982",
	"Fast Eddie\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_fasteddiRomInfo, a26_fasteddiRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Fast Food

static struct BurnRomInfo a26_fastfoodRomDesc[] = {
	{ "Fast Food (1982)(Telesys).bin", 0x01000, 0x62388455, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_fastfood)
STD_ROM_FN(a26_fastfood)

struct BurnDriver BurnDrva26_fastfood = {
	"a26_fastfood", NULL, NULL, NULL, "1982",
	"Fast Food\0", NULL, "Telesys", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_fastfoodRomInfo, a26_fastfoodRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Fathom

static struct BurnRomInfo a26_fathomRomDesc[] = {
	{ "Fathom (1983)(Imagic).bin", 0x02000, 0x93da13cc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_fathom)
STD_ROM_FN(a26_fathom)

struct BurnDriver BurnDrva26_fathom = {
	"a26_fathom", NULL, NULL, NULL, "1983",
	"Fathom\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_fathomRomInfo, a26_fathomRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Final Approach

static struct BurnRomInfo a26_finalappRomDesc[] = {
	{ "Final Approach (1982)(Apollo).bin", 0x01000, 0xff23f469, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_finalapp)
STD_ROM_FN(a26_finalapp)

struct BurnDriver BurnDrva26_finalapp = {
	"a26_finalapp", NULL, NULL, NULL, "1982",
	"Final Approach\0", NULL, "Apollo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_finalappRomInfo, a26_finalappRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Fire Fighter

static struct BurnRomInfo a26_ffighterRomDesc[] = {
	{ "Fire Fighter (1982)(Imagic).bin", 0x01000, 0xe361fb42, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_ffighter)
STD_ROM_FN(a26_ffighter)

struct BurnDriver BurnDrva26_ffighter = {
	"a26_ffighter", NULL, NULL, NULL, "1982",
	"Fire Fighter\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_ffighterRomInfo, a26_ffighterRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Fire Fly

static struct BurnRomInfo a26_fireflyRomDesc[] = {
	{ "Fire Fly (1983)(Mythicon).bin", 0x01000, 0x7a8e4ec7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_firefly)
STD_ROM_FN(a26_firefly)

struct BurnDriver BurnDrva26_firefly = {
	"a26_firefly", NULL, NULL, NULL, "1983",
	"Fire Fly\0", NULL, "Mythicon", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_fireflyRomInfo, a26_fireflyRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Fisher Price

static struct BurnRomInfo a26_fishpricRomDesc[] = {
	{ "Fisher Price (1983)(CCE).bin", 0x01000, 0xe8295349, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_fishpric)
STD_ROM_FN(a26_fishpric)

struct BurnDriver BurnDrva26_fishpric = {
	"a26_fishpric", NULL, NULL, NULL, "1983",
	"Fisher Price\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_fishpricRomInfo, a26_fishpricRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Fishing Derby

static struct BurnRomInfo a26_fishdrbyRomDesc[] = {
	{ "Fishing Derby (1980)(Activision).bin", 0x00800, 0x204c928f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_fishdrby)
STD_ROM_FN(a26_fishdrby)

struct BurnDriver BurnDrva26_fishdrby = {
	"a26_fishdrby", NULL, NULL, NULL, "1980",
	"Fishing Derby\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_fishdrbyRomInfo, a26_fishdrbyRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Flag Capture - Capture

static struct BurnRomInfo a26_flagcapRomDesc[] = {
	{ "Flag Capture (1978)(Atari).bin", 0x00800, 0x8aa1c41e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_flagcap)
STD_ROM_FN(a26_flagcap)

struct BurnDriver BurnDrva26_flagcap = {
	"a26_flagcap", NULL, NULL, NULL, "1978",
	"Flag Capture - Capture\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_flagcapRomInfo, a26_flagcapRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Football

static struct BurnRomInfo a26_footballRomDesc[] = {
	{ "Football (1979)(Atari).bin", 0x00800, 0x3b73ee02, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_football)
STD_ROM_FN(a26_football)

struct BurnDriver BurnDrva26_football = {
	"a26_football", NULL, NULL, NULL, "1979",
	"Football\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_footballRomInfo, a26_footballRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Frankenstein's Monster

static struct BurnRomInfo a26_frankenmRomDesc[] = {
	{ "Frankenstein's Monster (1983)(Data Age).bin", 0x01000, 0xad44dff9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_frankenm)
STD_ROM_FN(a26_frankenm)

struct BurnDriver BurnDrva26_frankenm = {
	"a26_frankenm", NULL, NULL, NULL, "1983",
	"Frankenstein's Monster\0", NULL, "Data Age", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_frankenmRomInfo, a26_frankenmRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Freeway

static struct BurnRomInfo a26_freewayRomDesc[] = {
	{ "Freeway (1981)(Activision).bin", 0x00800, 0xb13ec413, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_freeway)
STD_ROM_FN(a26_freeway)

struct BurnDriver BurnDrva26_freeway = {
	"a26_freeway", NULL, NULL, NULL, "1981",
	"Freeway\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_freewayRomInfo, a26_freewayRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Frisco

static struct BurnRomInfo a26_friscoRomDesc[] = {
	{ "Frisco (19xx)(Unknown).bin", 0x01000, 0xc4f16d4d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_frisco)
STD_ROM_FN(a26_frisco)

struct BurnDriver BurnDrva26_frisco = {
	"a26_frisco", NULL, NULL, NULL, "19??",
	"Frisco\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_friscoRomInfo, a26_friscoRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Frogger

static struct BurnRomInfo a26_froggerRomDesc[] = {
	{ "Frogger (1982)(Parker Brothers).bin", 0x01000, 0xf5e1af92, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_frogger)
STD_ROM_FN(a26_frogger)

struct BurnDriver BurnDrva26_frogger = {
	"a26_frogger", NULL, NULL, NULL, "1982",
	"Frogger\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_froggerRomInfo, a26_froggerRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Frogger II - Threeedeep!

static struct BurnRomInfo a26_frogger2RomDesc[] = {
	{ "Frogger II - Threeedeep! (1984)(Parker Brothers).bin", 0x02000, 0x3ba0d9bf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_frogger2)
STD_ROM_FN(a26_frogger2)

struct BurnDriver BurnDrva26_frogger2 = {
	"a26_frogger2", NULL, NULL, NULL, "1984",
	"Frogger II - Threeedeep!\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_frogger2RomInfo, a26_frogger2RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E0, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Frogs and Flies - Frogs 'n' Flies

static struct BurnRomInfo a26_frogsfliRomDesc[] = {
	{ "Frogs and Flies (1982)(M Network).bin", 0x01000, 0x6476c136, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_frogsfli)
STD_ROM_FN(a26_frogsfli)

struct BurnDriver BurnDrva26_frogsfli = {
	"a26_frogsfli", NULL, NULL, NULL, "1982",
	"Frogs and Flies - Frogs 'n' Flies\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_frogsfliRomInfo, a26_frogsfliRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Front Line

static struct BurnRomInfo a26_frontlinRomDesc[] = {
	{ "Front Line (1984)(Coleco).bin", 0x02000, 0xc352f290, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_frontlin)
STD_ROM_FN(a26_frontlin)

struct BurnDriver BurnDrva26_frontlin = {
	"a26_frontlin", NULL, NULL, NULL, "1984",
	"Front Line\0", NULL, "Coleco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_frontlinRomInfo, a26_frontlinRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Frostbite

static struct BurnRomInfo a26_frostbitRomDesc[] = {
	{ "Frostbite (1983)(Activision).bin", 0x01000, 0xb61be7a3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_frostbit)
STD_ROM_FN(a26_frostbit)

struct BurnDriver BurnDrva26_frostbit = {
	"a26_frostbit", NULL, NULL, NULL, "1983",
	"Frostbite\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_frostbitRomInfo, a26_frostbitRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Fun with Numbers

static struct BurnRomInfo a26_funnumbRomDesc[] = {
	{ "Fun with Numbers (1980)(Atari).bin", 0x00800, 0xd48ebac0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_funnumb)
STD_ROM_FN(a26_funnumb)

struct BurnDriver BurnDrva26_funnumb = {
	"a26_funnumb", NULL, NULL, NULL, "1980",
	"Fun with Numbers\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_funnumbRomInfo, a26_funnumbRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// G.I. Joe - Cobra Strike

static struct BurnRomInfo a26_gijoeRomDesc[] = {
	{ "G.I. Joe - Cobra Strike (1983)(Parker Brothers).bin", 0x01000, 0x6bcbcd37, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_gijoe)
STD_ROM_FN(a26_gijoe)

struct BurnDriver BurnDrva26_gijoe = {
	"a26_gijoe", NULL, NULL, NULL, "1983",
	"G.I. Joe - Cobra Strike\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_gijoeRomInfo, a26_gijoeRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Galaxian

static struct BurnRomInfo a26_galaxianRomDesc[] = {
	{ "Galaxian (1983)(Atari).bin", 0x02000, 0x4e9fe271, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_galaxian)
STD_ROM_FN(a26_galaxian)

struct BurnDriver BurnDrva26_galaxian = {
	"a26_galaxian", NULL, NULL, NULL, "1983",
	"Galaxian\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_galaxianRomInfo, a26_galaxianRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Gamma-Attack

static struct BurnRomInfo a26_gammaattRomDesc[] = {
	{ "Gamma-Attack (1983)(Gammation).bin", 0x00800, 0xd1efe8bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_gammaatt)
STD_ROM_FN(a26_gammaatt)

struct BurnDriver BurnDrva26_gammaatt = {
	"a26_gammaatt", NULL, NULL, NULL, "1983",
	"Gamma-Attack\0", NULL, "Gammation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_gammaattRomInfo, a26_gammaattRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Gangster Alley

static struct BurnRomInfo a26_gangstalRomDesc[] = {
	{ "Gangster Alley (1982)(Spectravideo).bin", 0x01000, 0x03c2452c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_gangstal)
STD_ROM_FN(a26_gangstal)

struct BurnDriver BurnDrva26_gangstal = {
	"a26_gangstal", NULL, NULL, NULL, "1982",
	"Gangster Alley\0", NULL, "Spectravideo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_gangstalRomInfo, a26_gangstalRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Gas Hog

static struct BurnRomInfo a26_gashogRomDesc[] = {
	{ "Gas Hog (1983)(Spectravideo).bin", 0x01000, 0x89eec77e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_gashog)
STD_ROM_FN(a26_gashog)

struct BurnDriver BurnDrva26_gashog = {
	"a26_gashog", NULL, NULL, NULL, "1983",
	"Gas Hog\0", NULL, "Spectravideo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_gashogRomInfo, a26_gashogRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Gauntlet

static struct BurnRomInfo a26_gauntletRomDesc[] = {
	{ "Gauntlet (1983)(TY Associates).bin", 0x01000, 0xc7ac23c7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_gauntlet)
STD_ROM_FN(a26_gauntlet)

struct BurnDriver BurnDrva26_gauntlet = {
	"a26_gauntlet", NULL, NULL, NULL, "1983",
	"Gauntlet\0", NULL, "TY Associates", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_gauntletRomInfo, a26_gauntletRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Ghost Manor

static struct BurnRomInfo a26_ghostmanRomDesc[] = {
	{ "Ghost Manor (1983)(Xonox).bin", 0x02000, 0x6fc46219, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_ghostman)
STD_ROM_FN(a26_ghostman)

struct BurnDriver BurnDrva26_ghostman = {
	"a26_ghostman", NULL, NULL, NULL, "1983",
	"Ghost Manor\0", NULL, "Xonox", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_ghostmanRomInfo, a26_ghostmanRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Ghostbusters

static struct BurnRomInfo a26_ghostbstRomDesc[] = {
	{ "Ghostbusters (1985)(Activision).bin", 0x02000, 0x45443d13, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_ghostbst)
STD_ROM_FN(a26_ghostbst)

struct BurnDriver BurnDrva26_ghostbst = {
	"a26_ghostbst", NULL, NULL, NULL, "1985",
	"Ghostbusters\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_ghostbstRomInfo, a26_ghostbstRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Glacier Patrol

static struct BurnRomInfo a26_glacierpRomDesc[] = {
	{ "Glacier Patrol (1989)(Telegames).bin", 0x01000, 0x44e77fd6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_glacierp)
STD_ROM_FN(a26_glacierp)

struct BurnDriver BurnDrva26_glacierp = {
	"a26_glacierp", NULL, NULL, NULL, "1989",
	"Glacier Patrol\0", NULL, "Telegames", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_glacierpRomInfo, a26_glacierpRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Glib - Video Word Game

static struct BurnRomInfo a26_glibRomDesc[] = {
	{ "Glib - Video Word Game (1983)(Selchow & Righter).bin", 0x01000, 0x50bb03a4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_glib)
STD_ROM_FN(a26_glib)

struct BurnDriver BurnDrva26_glib = {
	"a26_glib", NULL, NULL, NULL, "1983",
	"Glib - Video Word Game\0", NULL, "Selchow & Righter", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_glibRomInfo, a26_glibRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Golf

static struct BurnRomInfo a26_golfRomDesc[] = {
	{ "Golf (1980)(Atari).bin", 0x00800, 0x46a9f200, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_golf)
STD_ROM_FN(a26_golf)

struct BurnDriver BurnDrva26_golf = {
	"a26_golf", NULL, NULL, NULL, "1980",
	"Golf\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_golfRomInfo, a26_golfRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Gopher

static struct BurnRomInfo a26_gopherRomDesc[] = {
	{ "Gopher (1982)(U.S. Games Corporation).bin", 0x01000, 0xac8321d8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_gopher)
STD_ROM_FN(a26_gopher)

struct BurnDriver BurnDrva26_gopher = {
	"a26_gopher", NULL, NULL, NULL, "1982",
	"Gopher\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_gopherRomInfo, a26_gopherRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Gorf

static struct BurnRomInfo a26_gorfRomDesc[] = {
	{ "Gorf (1982)(CBS Electronics).bin", 0x01000, 0x9bd137aa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_gorf)
STD_ROM_FN(a26_gorf)

struct BurnDriver BurnDrva26_gorf = {
	"a26_gorf", NULL, NULL, NULL, "1982",
	"Gorf\0", NULL, "CBS Electronics", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_gorfRomInfo, a26_gorfRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Grand Prix

static struct BurnRomInfo a26_grandprxRomDesc[] = {
	{ "Grand Prix (1982)(Activision).bin", 0x01000, 0xe855273d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_grandprx)
STD_ROM_FN(a26_grandprx)

struct BurnDriver BurnDrva26_grandprx = {
	"a26_grandprx", NULL, NULL, NULL, "1982",
	"Grand Prix\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_grandprxRomInfo, a26_grandprxRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Grand Prize

static struct BurnRomInfo a26_grandprzRomDesc[] = {
	{ "Grand Prize (19xx)(Funvision).bin", 0x01000, 0x5108d36d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_grandprz)
STD_ROM_FN(a26_grandprz)

struct BurnDriver BurnDrva26_grandprz = {
	"a26_grandprz", NULL, NULL, NULL, "19??",
	"Grand Prize\0", NULL, "Funvision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_grandprzRomInfo, a26_grandprzRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Gravitar

static struct BurnRomInfo a26_gravitarRomDesc[] = {
	{ "Gravitar (1983)(Atari).bin", 0x02000, 0xc87fccbe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_gravitar)
STD_ROM_FN(a26_gravitar)

struct BurnDriver BurnDrva26_gravitar = {
	"a26_gravitar", NULL, NULL, NULL, "1983",
	"Gravitar\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_gravitarRomInfo, a26_gravitarRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Great Escape

static struct BurnRomInfo a26_greatescRomDesc[] = {
	{ "Great Escape (1983)(Unknown).bin", 0x01000, 0x99c240c1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_greatesc)
STD_ROM_FN(a26_greatesc)

struct BurnDriver BurnDrva26_greatesc = {
	"a26_greatesc", NULL, NULL, NULL, "1983",
	"Great Escape\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_greatescRomInfo, a26_greatescRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Gremlins

static struct BurnRomInfo a26_gremlinsRomDesc[] = {
	{ "Gremlins (1984)(Atari).bin", 0x02000, 0x48d5991f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_gremlins)
STD_ROM_FN(a26_gremlins)

struct BurnDriver BurnDrva26_gremlins = {
	"a26_gremlins", NULL, NULL, NULL, "1984",
	"Gremlins\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_gremlinsRomInfo, a26_gremlinsRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Guardian

static struct BurnRomInfo a26_guardianRomDesc[] = {
	{ "Guardian (1982)(Apollo).bin", 0x01000, 0x121738f5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_guardian)
STD_ROM_FN(a26_guardian)

struct BurnDriver BurnDrva26_guardian = {
	"a26_guardian", NULL, NULL, NULL, "1982",
	"Guardian\0", NULL, "Apollo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_guardianRomInfo, a26_guardianRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Gyruss

static struct BurnRomInfo a26_gyrussRomDesc[] = {
	{ "Gyruss (1984)(Parker Brothers).bin", 0x02000, 0x0d78e8a9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_gyruss)
STD_ROM_FN(a26_gyruss)

struct BurnDriver BurnDrva26_gyruss = {
	"a26_gyruss", NULL, NULL, NULL, "1984",
	"Gyruss\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_gyrussRomInfo, a26_gyrussRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E0, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// H.E.R.O.

static struct BurnRomInfo a26_heroRomDesc[] = {
	{ "H.E.R.O. (1984)(Activision).bin", 0x02000, 0x721e95b7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_hero)
STD_ROM_FN(a26_hero)

struct BurnDriver BurnDrva26_hero = {
	"a26_hero", NULL, NULL, NULL, "1984",
	"H.E.R.O.\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_heroRomInfo, a26_heroRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Halloween

static struct BurnRomInfo a26_hallowenRomDesc[] = {
	{ "Halloween (1983)(Wizard Video Games).bin", 0x01000, 0xb1b11806, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_hallowen)
STD_ROM_FN(a26_hallowen)

struct BurnDriver BurnDrva26_hallowen = {
	"a26_hallowen", NULL, NULL, NULL, "1983",
	"Halloween\0", NULL, "Wizard Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_hallowenRomInfo, a26_hallowenRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Hangman - Spelling

static struct BurnRomInfo a26_hangmanRomDesc[] = {
	{ "Hangman (1978)(Atari).bin", 0x01000, 0xc2bcc789, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_hangman)
STD_ROM_FN(a26_hangman)

struct BurnDriver BurnDrva26_hangman = {
	"a26_hangman", NULL, NULL, NULL, "1978",
	"Hangman - Spelling\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_hangmanRomInfo, a26_hangmanRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Harbor Escape

static struct BurnRomInfo a26_harboresRomDesc[] = {
	{ "Harbor Escape (1983)(Panda).bin", 0x01000, 0x85998ae2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_harbores)
STD_ROM_FN(a26_harbores)

struct BurnDriver BurnDrva26_harbores = {
	"a26_harbores", NULL, NULL, NULL, "1983",
	"Harbor Escape\0", NULL, "Panda", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_harboresRomInfo, a26_harboresRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Haunted House (NTSC)

static struct BurnRomInfo a26_hauntedRomDesc[] = {
	{ "Haunted House (1981)(Atari).bin", 0x01000, 0xaa62d961, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_haunted)
STD_ROM_FN(a26_haunted)

struct BurnDriver BurnDrva26_haunted = {
	"a26_haunted", NULL, NULL, NULL, "1981",
	"Haunted House (NTSC)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_hauntedRomInfo, a26_hauntedRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Hole Hunter

static struct BurnRomInfo a26_holehuntRomDesc[] = {
	{ "Hole Hunter (19xx)(Ariola).bin", 0x01000, 0x66d5ce67, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_holehunt)
STD_ROM_FN(a26_holehunt)

struct BurnDriver BurnDrva26_holehunt = {
	"a26_holehunt", NULL, NULL, NULL, "19??",
	"Hole Hunter\0", NULL, "Ariola", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_holehuntRomInfo, a26_holehuntRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Home Run - Baseball

static struct BurnRomInfo a26_homerunRomDesc[] = {
	{ "Home Run (1978)(Atari).bin", 0x00800, 0x45ace998, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_homerun)
STD_ROM_FN(a26_homerun)

struct BurnDriver BurnDrva26_homerun = {
	"a26_homerun", NULL, NULL, NULL, "1978",
	"Home Run - Baseball\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_homerunRomInfo, a26_homerunRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Human Cannonball - Cannon Man

static struct BurnRomInfo a26_humancanRomDesc[] = {
	{ "Human Cannonball (1979)(Atari).bin", 0x00800, 0xf05a41e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_humancan)
STD_ROM_FN(a26_humancan)

struct BurnDriver BurnDrva26_humancan = {
	"a26_humancan", NULL, NULL, NULL, "1979",
	"Human Cannonball - Cannon Man\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_humancanRomInfo, a26_humancanRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// I Want My Mommy

static struct BurnRomInfo a26_iwantmomRomDesc[] = {
	{ "I Want My Mommy (1983)(ZiMAG).bin", 0x01000, 0x57908550, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_iwantmom)
STD_ROM_FN(a26_iwantmom)

struct BurnDriver BurnDrva26_iwantmom = {
	"a26_iwantmom", NULL, NULL, NULL, "1983",
	"I Want My Mommy\0", NULL, "ZiMAG", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_iwantmomRomInfo, a26_iwantmomRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Ice Hockey - Le Hockey Sur Glace

static struct BurnRomInfo a26_icehockRomDesc[] = {
	{ "Ice Hockey (1981)(Activision).bin", 0x01000, 0xadab8cdf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_icehock)
STD_ROM_FN(a26_icehock)

struct BurnDriver BurnDrva26_icehock = {
	"a26_icehock", NULL, NULL, NULL, "1981",
	"Ice Hockey - Le Hockey Sur Glace\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_icehockRomInfo, a26_icehockRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Ikari Warriors

static struct BurnRomInfo a26_ikariRomDesc[] = {
	{ "Ikari Warriors (1989)(Atari).bin", 0x04000, 0x5b7ce555, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_ikari)
STD_ROM_FN(a26_ikari)

struct BurnDriver BurnDrva26_ikari = {
	"a26_ikari", NULL, NULL, NULL, "1989",
	"Ikari Warriors\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_ikariRomInfo, a26_ikariRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Imagic Selector ROM

static struct BurnRomInfo a26_imagicslRomDesc[] = {
	{ "Imagic Selector ROM (1982)(Imagic).bin", 0x01000, 0x552e3b21, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_imagicsl)
STD_ROM_FN(a26_imagicsl)

struct BurnDriver BurnDrva26_imagicsl = {
	"a26_imagicsl", NULL, NULL, NULL, "1982",
	"Imagic Selector ROM\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_imagicslRomInfo, a26_imagicslRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Immies & Aggies

static struct BurnRomInfo a26_immiesagRomDesc[] = {
	{ "Immies & Aggies (1983)(CCE).bin", 0x01000, 0xbfd8c8f9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_immiesag)
STD_ROM_FN(a26_immiesag)

struct BurnDriver BurnDrva26_immiesag = {
	"a26_immiesag", NULL, NULL, NULL, "1983",
	"Immies & Aggies\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_immiesagRomInfo, a26_immiesagRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Inca Gold

static struct BurnRomInfo a26_incagoldRomDesc[] = {
	{ "Inca Gold (19xx)(Zellers).bin", 0x01000, 0xf06e8235, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_incagold)
STD_ROM_FN(a26_incagold)

struct BurnDriver BurnDrva26_incagold = {
	"a26_incagold", NULL, NULL, NULL, "19??",
	"Inca Gold\0", NULL, "Zellers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_incagoldRomInfo, a26_incagoldRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Infiltrate

static struct BurnRomInfo a26_infiltraRomDesc[] = {
	{ "Infiltrate (1981)(Apollo).bin", 0x01000, 0x612bd4da, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_infiltra)
STD_ROM_FN(a26_infiltra)

struct BurnDriver BurnDrva26_infiltra = {
	"a26_infiltra", NULL, NULL, NULL, "1981",
	"Infiltrate\0", NULL, "Apollo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_infiltraRomInfo, a26_infiltraRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// International Soccer

static struct BurnRomInfo a26_intsocrRomDesc[] = {
	{ "International Soccer (1982)(M Network).bin", 0x01000, 0xb9d549a2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_intsocr)
STD_ROM_FN(a26_intsocr)

struct BurnDriver BurnDrva26_intsocr = {
	"a26_intsocr", NULL, NULL, NULL, "1982",
	"International Soccer\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_intsocrRomInfo, a26_intsocrRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// IQ 180

static struct BurnRomInfo a26_iq180RomDesc[] = {
	{ "IQ 180 (19xx)(Unknown).bin", 0x01000, 0x4dc2a674, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_iq180)
STD_ROM_FN(a26_iq180)

struct BurnDriver BurnDrva26_iq180 = {
	"a26_iq180", NULL, NULL, NULL, "19??",
	"IQ 180\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_iq180RomInfo, a26_iq180RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// James Bond 007

static struct BurnRomInfo a26_jbondRomDesc[] = {
	{ "James Bond 007 (1983)(Parker Brothers).bin", 0x02000, 0x34d3ffc8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_jbond)
STD_ROM_FN(a26_jbond)

struct BurnDriver BurnDrva26_jbond = {
	"a26_jbond", NULL, NULL, NULL, "1983",
	"James Bond 007\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_jbondRomInfo, a26_jbondRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E0, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Jawbreaker

static struct BurnRomInfo a26_jawbreakRomDesc[] = {
	{ "Jawbreaker (1982)(Tigervision).bin", 0x01000, 0xfc11bf67, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_jawbreak)
STD_ROM_FN(a26_jawbreak)

struct BurnDriver BurnDrva26_jawbreak = {
	"a26_jawbreak", NULL, NULL, NULL, "1982",
	"Jawbreaker\0", NULL, "Tigervision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_jawbreakRomInfo, a26_jawbreakRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Journey Escape

static struct BurnRomInfo a26_journeyRomDesc[] = {
	{ "Journey Escape (1982)(Data Age).bin", 0x01000, 0x52ff8027, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_journey)
STD_ROM_FN(a26_journey)

struct BurnDriver BurnDrva26_journey = {
	"a26_journey", NULL, NULL, NULL, "1982",
	"Journey Escape\0", NULL, "Data Age", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_journeyRomInfo, a26_journeyRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Joust

static struct BurnRomInfo a26_joustRomDesc[] = {
	{ "Joust (1983)(Atari).bin", 0x02000, 0xa07b3304, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_joust)
STD_ROM_FN(a26_joust)

struct BurnDriver BurnDrva26_joust = {
	"a26_joust", NULL, NULL, NULL, "1983",
	"Joust\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_joustRomInfo, a26_joustRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Jr. Pac-Man

static struct BurnRomInfo a26_jrpacmanRomDesc[] = {
	{ "Jr. Pac-Man (1984)(Atari).bin", 0x04000, 0x5c345bac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_jrpacman)
STD_ROM_FN(a26_jrpacman)

struct BurnDriver BurnDrva26_jrpacman = {
	"a26_jrpacman", NULL, NULL, NULL, "1984",
	"Jr. Pac-Man\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_jrpacmanRomInfo, a26_jrpacmanRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Jungle Hunt

static struct BurnRomInfo a26_junglehRomDesc[] = {
	{ "Jungle Hunt (1983)(Atari).bin", 0x02000, 0x9c3e8734, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_jungleh)
STD_ROM_FN(a26_jungleh)

struct BurnDriver BurnDrva26_jungleh = {
	"a26_jungleh", NULL, NULL, NULL, "1983",
	"Jungle Hunt\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_junglehRomInfo, a26_junglehRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Kaboom! (USA)

static struct BurnRomInfo a26_kaboomRomDesc[] = {
	{ "Kaboom! (1981)(Activision).bin", 0x00800, 0x813f00f6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_kaboom)
STD_ROM_FN(a26_kaboom)

struct BurnDriver BurnDrva26_kaboom = {
	"a26_kaboom", NULL, NULL, NULL, "1981",
	"Kaboom! (USA)\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_ACTION, 0,
	A2600GetZipName, a26_kaboomRomInfo, a26_kaboomRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_kaboom, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Kangaroo (USA)

static struct BurnRomInfo a26_kangarooRomDesc[] = {
	{ "Kangaroo (1983)(Atari).bin", 0x02000, 0xb9ab57e6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_kangaroo)
STD_ROM_FN(a26_kangaroo)

struct BurnDriver BurnDrva26_kangaroo = {
	"a26_kangaroo", NULL, NULL, NULL, "1983",
	"Kangaroo (USA)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_kangarooRomInfo, a26_kangarooRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Karate (USA)

static struct BurnRomInfo a26_karateRomDesc[] = {
	{ "Karate (1982)(Ultravision).bin", 0x01000, 0xac0a9ce0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_karate)
STD_ROM_FN(a26_karate)

struct BurnDriver BurnDrva26_karate = {
	"a26_karate", NULL, NULL, NULL, "1982",
	"Karate (USA)\0", NULL, "Ultravision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_VSFIGHT, 0,
	A2600GetZipName, a26_karateRomInfo, a26_karateRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Keystone Kapers (USA)

static struct BurnRomInfo a26_keykaperRomDesc[] = {
	{ "Keystone Kapers (1983)(Activision).bin", 0x01000, 0x3e55f3a1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_keykaper)
STD_ROM_FN(a26_keykaper)

struct BurnDriver BurnDrva26_keykaper = {
	"a26_keykaper", NULL, NULL, NULL, "1983",
	"Keystone Kapers (USA)\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_keykaperRomInfo, a26_keykaperRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// King Kong (USA)

static struct BurnRomInfo a26_kingkongRomDesc[] = {
	{ "King Kong (1982)(Tigervision).bin", 0x01000, 0xcb6f5617, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_kingkong)
STD_ROM_FN(a26_kingkong)

struct BurnDriver BurnDrva26_kingkong = {
	"a26_kingkong", NULL, NULL, NULL, "1982",
	"King Kong (USA)\0", NULL, "Tigervision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_kingkongRomInfo, a26_kingkongRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Kool-Aid Man (USA)

static struct BurnRomInfo a26_koolaidmRomDesc[] = {
	{ "Kool-Aid Man (1983)(M Network).bin", 0x01000, 0xd9bd009b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_koolaidm)
STD_ROM_FN(a26_koolaidm)

struct BurnDriver BurnDrva26_koolaidm = {
	"a26_koolaidm", NULL, NULL, NULL, "1983",
	"Kool-Aid Man (USA)\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_ACTION, 0,
	A2600GetZipName, a26_koolaidmRomInfo, a26_koolaidmRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Krull

static struct BurnRomInfo a26_krullRomDesc[] = {
	{ "Krull (1983)(Atari).bin", 0x02000, 0x1a67e6ed, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_krull)
STD_ROM_FN(a26_krull)

struct BurnDriver BurnDrva26_krull = {
	"a26_krull", NULL, NULL, NULL, "1983",
	"Krull\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_krullRomInfo, a26_krullRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Kung-Fu Master

static struct BurnRomInfo a26_kungfumRomDesc[] = {
	{ "Kung-Fu Master (1987)(Activision).bin", 0x02000, 0x5fea6e51, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_kungfum)
STD_ROM_FN(a26_kungfum)

struct BurnDriver BurnDrva26_kungfum = {
	"a26_kungfum", NULL, NULL, NULL, "1987",
	"Kung-Fu Master\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_kungfumRomInfo, a26_kungfumRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Laser Blast

static struct BurnRomInfo a26_laserblsRomDesc[] = {
	{ "Laser Blast (1981)(Activision).bin", 0x00800, 0x99365f26, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_laserbls)
STD_ROM_FN(a26_laserbls)

struct BurnDriver BurnDrva26_laserbls = {
	"a26_laserbls", NULL, NULL, NULL, "1981",
	"Laser Blast\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_laserblsRomInfo, a26_laserblsRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Laser Gate

static struct BurnRomInfo a26_lasergatRomDesc[] = {
	{ "Laser Gate (1983)(CCE).bin", 0x01000, 0x81c413a4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_lasergat)
STD_ROM_FN(a26_lasergat)

struct BurnDriver BurnDrva26_lasergat = {
	"a26_lasergat", NULL, NULL, NULL, "1983",
	"Laser Gate\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_lasergatRomInfo, a26_lasergatRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Laser Gates

static struct BurnRomInfo a26_lasrgatsRomDesc[] = {
	{ "Laser Gates (1983)(Imagic).bin", 0x01000, 0x4b7865af, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_lasrgats)
STD_ROM_FN(a26_lasrgats)

struct BurnDriver BurnDrva26_lasrgats = {
	"a26_lasrgats", NULL, NULL, NULL, "1983",
	"Laser Gates\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_lasrgatsRomInfo, a26_lasrgatsRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Laser Volley (Zellers)

static struct BurnRomInfo a26_laservolRomDesc[] = {
	{ "Laser Volley (19xx)(Zellers).bin", 0x01000, 0xd5840593, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_laservol)
STD_ROM_FN(a26_laservol)

struct BurnDriver BurnDrva26_laservol = {
	"a26_laservol", NULL, NULL, NULL, "19??",
	"Laser Volley (Zellers)\0", NULL, "Zellers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_laservolRomInfo, a26_laservolRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Lochjaw

static struct BurnRomInfo a26_lochjawRomDesc[] = {
	{ "Lochjaw (1981)(Apollo).bin", 0x01000, 0xb2a2ff31, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_lochjaw)
STD_ROM_FN(a26_lochjaw)

struct BurnDriver BurnDrva26_lochjaw = {
	"a26_lochjaw", NULL, NULL, NULL, "1981",
	"Lochjaw\0", NULL, "Apollo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_lochjawRomInfo, a26_lochjawRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Lock 'n' Chase

static struct BurnRomInfo a26_lncRomDesc[] = {
	{ "Lock 'n' Chase (1982)(M Network).bin", 0x01000, 0x7994bf16, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_lnc)
STD_ROM_FN(a26_lnc)

struct BurnDriver BurnDrva26_lnc = {
	"a26_lnc", NULL, NULL, NULL, "1982",
	"Lock 'n' Chase\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_lncRomInfo, a26_lncRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// London Blitz

static struct BurnRomInfo a26_londonRomDesc[] = {
	{ "London Blitz (1983)(Avalon Hill).bin", 0x01000, 0xd23cd66c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_london)
STD_ROM_FN(a26_london)

struct BurnDriver BurnDrva26_london = {
	"a26_london", NULL, NULL, NULL, "1983",
	"London Blitz\0", NULL, "Avalon Hill", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_londonRomInfo, a26_londonRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Lost Luggage

static struct BurnRomInfo a26_lostluggRomDesc[] = {
	{ "Lost Luggage (1981)(Apollo).bin", 0x01000, 0x551bdd37, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_lostlugg)
STD_ROM_FN(a26_lostlugg)

struct BurnDriver BurnDrva26_lostlugg = {
	"a26_lostlugg", NULL, NULL, NULL, "1981",
	"Lost Luggage\0", NULL, "Apollo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_lostluggRomInfo, a26_lostluggRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// M.A.D.

static struct BurnRomInfo a26_madRomDesc[] = {
	{ "M.A.D. (1982)(U.S. Games Corporation).bin", 0x01000, 0x5cb68fe5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mad)
STD_ROM_FN(a26_mad)

struct BurnDriver BurnDrva26_mad = {
	"a26_mad", NULL, NULL, NULL, "1982",
	"M.A.D.\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_madRomInfo, a26_madRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// M*A*S*H

static struct BurnRomInfo a26_mashRomDesc[] = {
	{ "M.A.S.H (1983)(20th Century Fox Video Games).bin", 0x01000, 0x13e1c83d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mash)
STD_ROM_FN(a26_mash)

struct BurnDriver BurnDrva26_mash = {
	"a26_mash", NULL, NULL, NULL, "1983",
	"M*A*S*H\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mashRomInfo, a26_mashRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Malagai

static struct BurnRomInfo a26_malagaiRomDesc[] = {
	{ "Malagai (1983)(TY Associates).bin", 0x01000, 0x65790ef9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_malagai)
STD_ROM_FN(a26_malagai)

struct BurnDriver BurnDrva26_malagai = {
	"a26_malagai", NULL, NULL, NULL, "1983",
	"Malagai\0", NULL, "TY Associates", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_malagaiRomInfo, a26_malagaiRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Mangia'

static struct BurnRomInfo a26_mangiaRomDesc[] = {
	{ "Mangia' (1983)(Spectravideo).bin", 0x01000, 0xb807b775, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mangia)
STD_ROM_FN(a26_mangia)

struct BurnDriver BurnDrva26_mangia = {
	"a26_mangia", NULL, NULL, NULL, "1983",
	"Mangia'\0", NULL, "Spectravideo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mangiaRomInfo, a26_mangiaRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Marauder

static struct BurnRomInfo a26_marauderRomDesc[] = {
	{ "Marauder (1982)(Tigervision).bin", 0x01000, 0xf8a8b470, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_marauder)
STD_ROM_FN(a26_marauder)

struct BurnDriver BurnDrva26_marauder = {
	"a26_marauder", NULL, NULL, NULL, "1982",
	"Marauder\0", NULL, "Tigervision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_marauderRomInfo, a26_marauderRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Marine Wars

static struct BurnRomInfo a26_marinwarRomDesc[] = {
	{ "Marine Wars (1983)(Konami).bin", 0x01000, 0x4d281ee3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_marinwar)
STD_ROM_FN(a26_marinwar)

struct BurnDriver BurnDrva26_marinwar = {
	"a26_marinwar", NULL, NULL, NULL, "1983",
	"Marine Wars\0", NULL, "Konami", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_marinwarRomInfo, a26_marinwarRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Mario Bros.

static struct BurnRomInfo a26_marioRomDesc[] = {
	{ "Mario Bros. (1983)(Atari).bin", 0x02000, 0x8fbf7e90, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mario)
STD_ROM_FN(a26_mario)

struct BurnDriver BurnDrva26_mario = {
	"a26_mario", NULL, NULL, NULL, "1983",
	"Mario Bros.\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_marioRomInfo, a26_marioRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Master Builder

static struct BurnRomInfo a26_masterbRomDesc[] = {
	{ "Master Builder (1983)(Spectravideo).bin", 0x01000, 0xdd196d6c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_masterb)
STD_ROM_FN(a26_masterb)

struct BurnDriver BurnDrva26_masterb = {
	"a26_masterb", NULL, NULL, NULL, "1983",
	"Master Builder\0", NULL, "Spectravideo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_masterbRomInfo, a26_masterbRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Masters of the Universe - The Power of He-Man

static struct BurnRomInfo a26_mastersRomDesc[] = {
	{ "Masters of the Universe - The Power of He-Man (1983)(M Network).bin", 0x04000, 0x0603e177, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_masters)
STD_ROM_FN(a26_masters)

struct BurnDriver BurnDrva26_masters = {
	"a26_masters", NULL, NULL, NULL, "1983",
	"Masters of the Universe - The Power of He-Man\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mastersRomInfo, a26_mastersRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E7, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Math Gran Prix

static struct BurnRomInfo a26_mathgpRomDesc[] = {
	{ "Math Gran Prix (1982)(Atari).bin", 0x01000, 0xccc90c98, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mathgp)
STD_ROM_FN(a26_mathgp)

struct BurnDriver BurnDrva26_mathgp = {
	"a26_mathgp", NULL, NULL, NULL, "1982",
	"Math Gran Prix\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mathgpRomInfo, a26_mathgpRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Maze Craze: A Game of Cops 'n Robbers (USA)

static struct BurnRomInfo a26_mazecrazRomDesc[] = {
	{ "Maze Craze - A Game of Cops 'n Robbers (1980)(Atari).bin", 0x01000, 0x0098e428, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mazecraz)
STD_ROM_FN(a26_mazecraz)

struct BurnDriver BurnDrva26_mazecraz = {
	"a26_mazecraz", NULL, NULL, NULL, "1980",
	"Maze Craze: A Game of Cops 'n Robbers (USA)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MAZE, 0,
	A2600GetZipName, a26_mazecrazRomInfo, a26_mazecrazRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Mega Force (USA)

static struct BurnRomInfo a26_megafrceRomDesc[] = {
	{ "Mega Force (1982)(20th Century Fox Video Games).bin", 0x01000, 0x9e6ae8c2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_megafrce)
STD_ROM_FN(a26_megafrce)

struct BurnDriver BurnDrva26_megafrce = {
	"a26_megafrce", NULL, NULL, NULL, "1982",
	"Mega Force (USA)\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_HORSHOOT, 0,
	A2600GetZipName, a26_megafrceRomInfo, a26_megafrceRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// MegaMania - A Space Nightmare

static struct BurnRomInfo a26_megmaniaRomDesc[] = {
	{ "MegaMania - A Space Nightmare (1982)(Activision).bin", 0x01000, 0x33edc33e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_megmania)
STD_ROM_FN(a26_megmania)

struct BurnDriver BurnDrva26_megmania = {
	"a26_megmania", NULL, NULL, NULL, "1982",
	"MegaMania - A Space Nightmare\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_megmaniaRomInfo, a26_megmaniaRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Midnight Magic

static struct BurnRomInfo a26_midmagicRomDesc[] = {
	{ "Midnight Magic (1984)(Atari).bin", 0x04000, 0x5c5447b9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_midmagic)
STD_ROM_FN(a26_midmagic)

struct BurnDriver BurnDrva26_midmagic = {
	"a26_midmagic", NULL, NULL, NULL, "1984",
	"Midnight Magic\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_midmagicRomInfo, a26_midmagicRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Mighty Mouse

static struct BurnRomInfo a26_mightymoRomDesc[] = {
	{ "Mighty Mouse (19xx)(Funvision).bin", 0x01000, 0x9aee6020, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mightymo)
STD_ROM_FN(a26_mightymo)

struct BurnDriver BurnDrva26_mightymo = {
	"a26_mightymo", NULL, NULL, NULL, "19??",
	"Mighty Mouse\0", NULL, "Funvision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mightymoRomInfo, a26_mightymoRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Millipede

static struct BurnRomInfo a26_millipedRomDesc[] = {
	{ "Millipede (1984)(Atari).bin", 0x04000, 0xccc82dd0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_milliped)
STD_ROM_FN(a26_milliped)

struct BurnDriver BurnDrva26_milliped = {
	"a26_milliped", NULL, NULL, NULL, "1984",
	"Millipede\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_millipedRomInfo, a26_millipedRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Miner 2049er - Starring Bounty Bob

static struct BurnRomInfo a26_mine2049RomDesc[] = {
	{ "Miner 2049er - Starring Bounty Bob (1982)(Tigervision).bin", 0x02000, 0xbd08d915, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mine2049)
STD_ROM_FN(a26_mine2049)

struct BurnDriver BurnDrva26_mine2049 = {
	"a26_mine2049", NULL, NULL, NULL, "1982",
	"Miner 2049er - Starring Bounty Bob\0", NULL, "Tigervision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mine2049RomInfo, a26_mine2049RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_3F, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Miner 2049er Volume II

static struct BurnRomInfo a26_m2049v2RomDesc[] = {
	{ "Miner 2049er Volume II (1983)(Tigervision).bin", 0x02000, 0x71e814e9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_m2049v2)
STD_ROM_FN(a26_m2049v2)

struct BurnDriver BurnDrva26_m2049v2 = {
	"a26_m2049v2", NULL, NULL, NULL, "1983",
	"Miner 2049er Volume II\0", NULL, "Tigervision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_m2049v2RomInfo, a26_m2049v2RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_3F, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Mines of Minos

static struct BurnRomInfo a26_mineminoRomDesc[] = {
	{ "Mines of Minos (1982)(CommaVid).bin", 0x01000, 0xc799e944, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_minemino)
STD_ROM_FN(a26_minemino)

struct BurnDriver BurnDrva26_minemino = {
	"a26_minemino", NULL, NULL, NULL, "1982",
	"Mines of Minos\0", NULL, "CommaVid", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mineminoRomInfo, a26_mineminoRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Miniature Golf - Arcade Golf

static struct BurnRomInfo a26_minigolfRomDesc[] = {
	{ "Miniature Golf (1979)(Atari).bin", 0x00800, 0x28562315, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_minigolf)
STD_ROM_FN(a26_minigolf)

struct BurnDriver BurnDrva26_minigolf = {
	"a26_minigolf", NULL, NULL, NULL, "1979",
	"Miniature Golf - Arcade Golf\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_minigolfRomInfo, a26_minigolfRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Missile Command

static struct BurnRomInfo a26_missileRomDesc[] = {
	{ "Missile Command (1981)(Atari).bin", 0x01000, 0xcff14904, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_missile)
STD_ROM_FN(a26_missile)

struct BurnDriver BurnDrva26_missile = {
	"a26_missile", NULL, NULL, NULL, "1981",
	"Missile Command\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_missileRomInfo, a26_missileRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Mission 3,000 A.D. - Mission 3000

static struct BurnRomInfo a26_miss3000RomDesc[] = {
	{ "Mission 3,000 A.D. (1983)(Bit Corporation).bin", 0x01000, 0xb959a92f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_miss3000)
STD_ROM_FN(a26_miss3000)

struct BurnDriver BurnDrva26_miss3000 = {
	"a26_miss3000", NULL, NULL, NULL, "1983",
	"Mission 3,000 A.D. - Mission 3000\0", NULL, "Bit Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_miss3000RomInfo, a26_miss3000RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Mogul Maniac

static struct BurnRomInfo a26_mogulRomDesc[] = {
	{ "Mogul Maniac (1983)(Amiga).bin", 0x01000, 0x50afe04b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mogul)
STD_ROM_FN(a26_mogul)

struct BurnDriver BurnDrva26_mogul = {
	"a26_mogul", NULL, NULL, NULL, "1983",
	"Mogul Maniac\0", NULL, "Amiga", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mogulRomInfo, a26_mogulRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Montezuma's Revenge - Featuring Panama Joe

static struct BurnRomInfo a26_montezumRomDesc[] = {
	{ "Montezuma's Revenge - Featuring Panama Joe (1984)(Parker Brothers).bin", 0x02000, 0xe680a1c9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_montezum)
STD_ROM_FN(a26_montezum)

struct BurnDriver BurnDrva26_montezum = {
	"a26_montezum", NULL, NULL, NULL, "1984",
	"Montezuma's Revenge - Featuring Panama Joe\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_montezumRomInfo, a26_montezumRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E0, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Moon Patrol

static struct BurnRomInfo a26_mpatrolRomDesc[] = {
	{ "Moon Patrol (1983)(Atari).bin", 0x02000, 0xd641ef2d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mpatrol)
STD_ROM_FN(a26_mpatrol)

struct BurnDriver BurnDrva26_mpatrol = {
	"a26_mpatrol", NULL, NULL, NULL, "1983",
	"Moon Patrol\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mpatrolRomInfo, a26_mpatrolRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Moonsweeper

static struct BurnRomInfo a26_moonswepRomDesc[] = {
	{ "Moonsweeper (1983)(Imagic).bin", 0x02000, 0x450b5c57, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_moonswep)
STD_ROM_FN(a26_moonswep)

struct BurnDriver BurnDrva26_moonswep = {
	"a26_moonswep", NULL, NULL, NULL, "1983",
	"Moonsweeper\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_moonswepRomInfo, a26_moonswepRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8_B1, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Moto Laser

static struct BurnRomInfo a26_motolasrRomDesc[] = {
	{ "Moto Laser (19xx)(Star Game).bin", 0x01000, 0xc71bc0d3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_motolasr)
STD_ROM_FN(a26_motolasr)

struct BurnDriver BurnDrva26_motolasr = {
	"a26_motolasr", NULL, NULL, NULL, "19??",
	"Moto Laser\0", NULL, "Star Game", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_motolasrRomInfo, a26_motolasrRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Motocross

static struct BurnRomInfo a26_motocrsRomDesc[] = {
	{ "Motocross (19xx)(Joystik).bin", 0x02000, 0x1bd58e61, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_motocrs)
STD_ROM_FN(a26_motocrs)

struct BurnDriver BurnDrva26_motocrs = {
	"a26_motocrs", NULL, NULL, NULL, "19??",
	"Motocross\0", NULL, "Joystik", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_motocrsRomInfo, a26_motocrsRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Motocross Racer

static struct BurnRomInfo a26_motoxracRomDesc[] = {
	{ "Motocross Racer (1983)(Xonox).bin", 0x02000, 0x0d1bc1cb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_motoxrac)
STD_ROM_FN(a26_motoxrac)

struct BurnDriver BurnDrva26_motoxrac = {
	"a26_motoxrac", NULL, NULL, NULL, "1983",
	"Motocross Racer\0", NULL, "Xonox", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_motoxracRomInfo, a26_motoxracRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// MotoRodeo

static struct BurnRomInfo a26_motorodRomDesc[] = {
	{ "MotoRodeo (1990)(Atari).bin", 0x04000, 0x89998e29, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_motorod)
STD_ROM_FN(a26_motorod)

struct BurnDriver BurnDrva26_motorod = {
	"a26_motorod", NULL, NULL, NULL, "1990",
	"MotoRodeo\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_motorodRomInfo, a26_motorodRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Mountain King

static struct BurnRomInfo a26_mkingRomDesc[] = {
	{ "Mountain King (1983)(CBS Electronics).bin", 0x03000, 0xed778991, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mking)
STD_ROM_FN(a26_mking)

struct BurnDriver BurnDrva26_mking = {
	"a26_mking", NULL, NULL, NULL, "1983",
	"Mountain King\0", NULL, "CBS Electronics", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mkingRomInfo, a26_mkingRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_FA, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Mouse Trap

static struct BurnRomInfo a26_mtrapRomDesc[] = {
	{ "Mouse Trap (1982)(Coleco).bin", 0x01000, 0x9ddd45d3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mtrap)
STD_ROM_FN(a26_mtrap)

struct BurnDriver BurnDrva26_mtrap = {
	"a26_mtrap", NULL, NULL, NULL, "1982",
	"Mouse Trap\0", NULL, "Coleco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mtrapRomInfo, a26_mtrapRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Mr. Do!

static struct BurnRomInfo a26_mrdoRomDesc[] = {
	{ "Mr. Do! (1983)(Coleco).bin", 0x02000, 0x860a47a1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mrdo)
STD_ROM_FN(a26_mrdo)

struct BurnDriver BurnDrva26_mrdo = {
	"a26_mrdo", NULL, NULL, NULL, "1983",
	"Mr. Do!\0", NULL, "Coleco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mrdoRomInfo, a26_mrdoRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Mr. Do!'s Castle

static struct BurnRomInfo a26_docastleRomDesc[] = {
	{ "Mr. Do!'s Castle (1984)(Parker Brothers).bin", 0x02000, 0x044735b9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_docastle)
STD_ROM_FN(a26_docastle)

struct BurnDriver BurnDrva26_docastle = {
	"a26_docastle", NULL, NULL, NULL, "1984",
	"Mr. Do!'s Castle\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_docastleRomInfo, a26_docastleRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E0, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Mr. Postman (Digitel)

static struct BurnRomInfo a26_mrpostRomDesc[] = {
	{ "Mr. Postman (19xx)(Digitel).bin", 0x01000, 0x0d3eba3a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mrpost)
STD_ROM_FN(a26_mrpost)

struct BurnDriver BurnDrva26_mrpost = {
	"a26_mrpost", NULL, NULL, NULL, "19??",
	"Mr. Postman (Digitel)\0", NULL, "Digitel", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mrpostRomInfo, a26_mrpostRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Ms. Pac-Man

static struct BurnRomInfo a26_mspacmanRomDesc[] = {
	{ "Ms. Pac-Man (1982)(Atari).bin", 0x02000, 0xb2d08fc9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_mspacman)
STD_ROM_FN(a26_mspacman)

struct BurnDriver BurnDrva26_mspacman = {
	"a26_mspacman", NULL, NULL, NULL, "1982",
	"Ms. Pac-Man\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_mspacmanRomInfo, a26_mspacmanRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// The Music Machine

static struct BurnRomInfo a26_musicmacRomDesc[] = {
	{ "The Music Machine (1983)(Sparrow).bin", 0x01000, 0xbd133d59, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_musicmac)
STD_ROM_FN(a26_musicmac)

struct BurnDriver BurnDrva26_musicmac = {
	"a26_musicmac", NULL, NULL, NULL, "1983",
	"The Music Machine\0", NULL, "Sparrow", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_musicmacRomInfo, a26_musicmacRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Name This Game

static struct BurnRomInfo a26_namethisRomDesc[] = {
	{ "Name This Game (1982)(U.S. Games Corporation).bin", 0x01000, 0xa86e1ad8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_namethis)
STD_ROM_FN(a26_namethis)

struct BurnDriver BurnDrva26_namethis = {
	"a26_namethis", NULL, NULL, NULL, "1982",
	"Name This Game\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_namethisRomInfo, a26_namethisRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Night Driver

static struct BurnRomInfo a26_nitedrvrRomDesc[] = {
	{ "Night Driver (1980)(Atari).bin", 0x00800, 0x600d8a96, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_nitedrvr)
STD_ROM_FN(a26_nitedrvr)

struct BurnDriver BurnDrva26_nitedrvr = {
	"a26_nitedrvr", NULL, NULL, NULL, "1980",
	"Night Driver\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_nitedrvrRomInfo, a26_nitedrvrRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_2K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Nightmare

static struct BurnRomInfo a26_nightmarRomDesc[] = {
	{ "Nightmare (19xx)(CCE).bin", 0x01000, 0x16885278, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_nightmar)
STD_ROM_FN(a26_nightmar)

struct BurnDriver BurnDrva26_nightmar = {
	"a26_nightmar", NULL, NULL, NULL, "19??",
	"Nightmare\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_nightmarRomInfo, a26_nightmarRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// No Escape!

static struct BurnRomInfo a26_noescapeRomDesc[] = {
	{ "No Escape! (1982)(Imagic).bin", 0x01000, 0x8941caea, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_noescape)
STD_ROM_FN(a26_noescape)

struct BurnDriver BurnDrva26_noescape = {
	"a26_noescape", NULL, NULL, NULL, "1982",
	"No Escape!\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_noescapeRomInfo, a26_noescapeRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Nuts

static struct BurnRomInfo a26_nutsRomDesc[] = {
	{ "Nuts (19xx)(Unknown).bin", 0x01000, 0x9b443896, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_nuts)
STD_ROM_FN(a26_nuts)

struct BurnDriver BurnDrva26_nuts = {
	"a26_nuts", NULL, NULL, NULL, "19??",
	"Nuts\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_nutsRomInfo, a26_nutsRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Obelix

static struct BurnRomInfo a26_obelixRomDesc[] = {
	{ "Obelix (1983)(Atari).bin", 0x02000, 0x29a51ea4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_obelix)
STD_ROM_FN(a26_obelix)

struct BurnDriver BurnDrva26_obelix = {
	"a26_obelix", NULL, NULL, NULL, "1983",
	"Obelix\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_obelixRomInfo, a26_obelixRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Ocean City

static struct BurnRomInfo a26_oceancitRomDesc[] = {
	{ "Ocean City (19xx)(Funvision).bin", 0x01000, 0xc6617840, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_oceancit)
STD_ROM_FN(a26_oceancit)

struct BurnDriver BurnDrva26_oceancit = {
	"a26_oceancit", NULL, NULL, NULL, "19??",
	"Ocean City\0", NULL, "Funvision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_oceancitRomInfo, a26_oceancitRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Off the Wall

static struct BurnRomInfo a26_offtwallRomDesc[] = {
	{ "Off the Wall (1989)(Atari).bin", 0x04000, 0xa09779ea, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_offtwall)
STD_ROM_FN(a26_offtwall)

struct BurnDriver BurnDrva26_offtwall = {
	"a26_offtwall", NULL, NULL, NULL, "1989",
	"Off the Wall\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_offtwallRomInfo, a26_offtwallRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Oink!

static struct BurnRomInfo a26_oinkRomDesc[] = {
	{ "Oink! (1982)(Activision).bin", 0x01000, 0x86f99b5e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_oink)
STD_ROM_FN(a26_oink)

struct BurnDriver BurnDrva26_oink = {
	"a26_oink", NULL, NULL, NULL, "1982",
	"Oink!\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_oinkRomInfo, a26_oinkRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Omega Race

static struct BurnRomInfo a26_omegaracRomDesc[] = {
	{ "Omega Race (1983)(CBS Electronics).bin", 0x03000, 0xe9876116, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_omegarac)
STD_ROM_FN(a26_omegarac)

struct BurnDriver BurnDrva26_omegarac = {
	"a26_omegarac", NULL, NULL, NULL, "1983",
	"Omega Race\0", NULL, "CBS Electronics", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_omegaracRomInfo, a26_omegaracRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_FA, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Othello

static struct BurnRomInfo a26_othelloRomDesc[] = {
	{ "Othello (1981)(Atari).bin", 0x00800, 0x171ae72f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_othello)
STD_ROM_FN(a26_othello)

struct BurnDriver BurnDrva26_othello = {
	"a26_othello", NULL, NULL, NULL, "1981",
	"Othello\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_othelloRomInfo, a26_othelloRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Out of Control

static struct BurnRomInfo a26_outcontrRomDesc[] = {
	{ "Out of Control (1983)(Avalon Hill).bin", 0x01000, 0xca4d03fb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_outcontr)
STD_ROM_FN(a26_outcontr)

struct BurnDriver BurnDrva26_outcontr = {
	"a26_outcontr", NULL, NULL, NULL, "1983",
	"Out of Control\0", NULL, "Avalon Hill", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_outcontrRomInfo, a26_outcontrRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Outlaw - Gunslinger

static struct BurnRomInfo a26_outlawRomDesc[] = {
	{ "Outlaw (1978)(Atari).bin", 0x00800, 0x68dd7acd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_outlaw)
STD_ROM_FN(a26_outlaw)

struct BurnDriver BurnDrva26_outlaw = {
	"a26_outlaw", NULL, NULL, NULL, "1978",
	"Outlaw - Gunslinger\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_outlawRomInfo, a26_outlawRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pac Kong (unknown)

static struct BurnRomInfo a26_packong1RomDesc[] = {
	{ "Pac Kong (19xx)(Unknown).bin", 0x01000, 0xd3d0cc39, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_packong1)
STD_ROM_FN(a26_packong1)

struct BurnDriver BurnDrva26_packong1 = {
	"a26_packong1", NULL, NULL, NULL, "19??",
	"Pac Kong (unknown)\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_packong1RomInfo, a26_packong1RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pac-Man

static struct BurnRomInfo a26_pacmanRomDesc[] = {
	{ "Pac-Man (1982)(Atari).bin", 0x01000, 0xddc9a881, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_pacman)
STD_ROM_FN(a26_pacman)

struct BurnDriver BurnDrva26_pacman = {
	"a26_pacman", NULL, NULL, NULL, "1982",
	"Pac-Man\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_pacmanRomInfo, a26_pacmanRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pega Ladrao

static struct BurnRomInfo a26_pegaladrRomDesc[] = {
	{ "Pega Ladrao (19xx)(Dismac).bin", 0x01000, 0x33c7f94f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_pegaladr)
STD_ROM_FN(a26_pegaladr)

struct BurnDriver BurnDrva26_pegaladr = {
	"a26_pegaladr", NULL, NULL, NULL, "19??",
	"Pega Ladrao\0", NULL, "Dismac", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_pegaladrRomInfo, a26_pegaladrRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pengo

static struct BurnRomInfo a26_pengoRomDesc[] = {
	{ "Pengo (1984)(Atari).bin", 0x02000, 0x7667e739, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_pengo)
STD_ROM_FN(a26_pengo)

struct BurnDriver BurnDrva26_pengo = {
	"a26_pengo", NULL, NULL, NULL, "1984",
	"Pengo\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_pengoRomInfo, a26_pengoRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pepsi Invaders - Coke Wins

static struct BurnRomInfo a26_pepsiinvRomDesc[] = {
	{ "Pepsi Invaders (1983)(Atari).bin", 0x01000, 0x7ac99b5c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_pepsiinv)
STD_ROM_FN(a26_pepsiinv)

struct BurnDriver BurnDrva26_pepsiinv = {
	"a26_pepsiinv", NULL, NULL, NULL, "1983",
	"Pepsi Invaders - Coke Wins\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_pepsiinvRomInfo, a26_pepsiinvRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pete Rose Baseball

static struct BurnRomInfo a26_peteroseRomDesc[] = {
	{ "Pete Rose Baseball (1988)(Absolute Entertainment).bin", 0x04000, 0xd0c6fc32, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_peterose)
STD_ROM_FN(a26_peterose)

struct BurnDriver BurnDrva26_peterose = {
	"a26_peterose", NULL, NULL, NULL, "1988",
	"Pete Rose Baseball\0", NULL, "Absolute Entertainment", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_peteroseRomInfo, a26_peteroseRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Phanton Tank (Digivision)

static struct BurnRomInfo a26_phanttnkdvRomDesc[] = {
	{ "Phanton Tank (19xx)(Digivision).bin", 0x01000, 0xa99dce32, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_phanttnkdv)
STD_ROM_FN(a26_phanttnkdv)

struct BurnDriver BurnDrva26_phanttnkdv = {
	"a26_phanttnkdv", NULL, NULL, NULL, "19??",
	"Phanton Tank (Digivision)\0", NULL, "Digivision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_phanttnkdvRomInfo, a26_phanttnkdvRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pharaoh's Curse (unknown)

static struct BurnRomInfo a26_pharaoh2RomDesc[] = {
	{ "Pharaoh's Curse (19xx)(Unknown).bin", 0x01000, 0xc17c5f64, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_pharaoh2)
STD_ROM_FN(a26_pharaoh2)

struct BurnDriver BurnDrva26_pharaoh2 = {
	"a26_pharaoh2", NULL, NULL, NULL, "19??",
	"Pharaoh's Curse (unknown)\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_pharaoh2RomInfo, a26_pharaoh2RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Phoenix

static struct BurnRomInfo a26_phoenixRomDesc[] = {
	{ "Phoenix (1982)(Atari).bin", 0x02000, 0x6ae1b66c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_phoenix)
STD_ROM_FN(a26_phoenix)

struct BurnDriver BurnDrva26_phoenix = {
	"a26_phoenix", NULL, NULL, NULL, "1982",
	"Phoenix\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_phoenixRomInfo, a26_phoenixRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Picnic

static struct BurnRomInfo a26_picnicRomDesc[] = {
	{ "Picnic (1982)(U.S. Games Corporation).bin", 0x01000, 0x1d507a61, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_picnic)
STD_ROM_FN(a26_picnic)

struct BurnDriver BurnDrva26_picnic = {
	"a26_picnic", NULL, NULL, NULL, "1982",
	"Picnic\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_picnicRomInfo, a26_picnicRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Piece o' Cake

static struct BurnRomInfo a26_piececakRomDesc[] = {
	{ "Piece o' Cake (1982)(U.S. Games Corporation).bin", 0x01000, 0x40355166, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_piececak)
STD_ROM_FN(a26_piececak)

struct BurnDriver BurnDrva26_piececak = {
	"a26_piececak", NULL, NULL, NULL, "1982",
	"Piece o' Cake\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_piececakRomInfo, a26_piececakRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pigs in Space - Starring Miss Piggy

static struct BurnRomInfo a26_pigspaceRomDesc[] = {
	{ "Pigs in Space - Starring Miss Piggy (1983)(Atari).bin", 0x02000, 0x27673d7c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_pigspace)
STD_ROM_FN(a26_pigspace)

struct BurnDriver BurnDrva26_pigspace = {
	"a26_pigspace", NULL, NULL, NULL, "1983",
	"Pigs in Space - Starring Miss Piggy\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_pigspaceRomInfo, a26_pigspaceRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pimball

static struct BurnRomInfo a26_pimballRomDesc[] = {
	{ "Pimball (1983)(CCE).bin", 0x01000, 0x65e077f1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_pimball)
STD_ROM_FN(a26_pimball)

struct BurnDriver BurnDrva26_pimball = {
	"a26_pimball", NULL, NULL, NULL, "1983",
	"Pimball\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_pimballRomInfo, a26_pimballRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pitfall II - Lost Caverns

static struct BurnRomInfo a26_pitfall2RomDesc[] = {
	{ "Pitfall II - Lost Caverns (1983)(Activision).bin", 0x028ff, 0x097ce7ad, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_pitfall2)
STD_ROM_FN(a26_pitfall2)

struct BurnDriver BurnDrva26_pitfall2 = {
	"a26_pitfall2", NULL, NULL, NULL, "1983",
	"Pitfall II - Lost Caverns\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_pitfall2RomInfo, a26_pitfall2RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_DPC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pitfall! - Pitfall Harry's Jungle Adventure

static struct BurnRomInfo a26_pitfallRomDesc[] = {
	{ "Pitfall! - Pitfall Harry's Jungle Adventure (1982)(Activision).bin", 0x01000, 0x42ad47bf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_pitfall)
STD_ROM_FN(a26_pitfall)

struct BurnDriver BurnDrva26_pitfall = {
	"a26_pitfall", NULL, NULL, NULL, "1982",
	"Pitfall! - Pitfall Harry's Jungle Adventure\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_pitfallRomInfo, a26_pitfallRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pizza Chef

static struct BurnRomInfo a26_pizzachfRomDesc[] = {
	{ "Pizza Chef (1983)(CCE).bin", 0x01000, 0xb9a9264e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_pizzachf)
STD_ROM_FN(a26_pizzachf)

struct BurnDriver BurnDrva26_pizzachf = {
	"a26_pizzachf", NULL, NULL, NULL, "1983",
	"Pizza Chef\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_pizzachfRomInfo, a26_pizzachfRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Planet Patrol

static struct BurnRomInfo a26_ppatrolRomDesc[] = {
	{ "Planet Patrol (1982)(Spectravideo).bin", 0x01000, 0x1e8ebb51, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_ppatrol)
STD_ROM_FN(a26_ppatrol)

struct BurnDriver BurnDrva26_ppatrol = {
	"a26_ppatrol", NULL, NULL, NULL, "1982",
	"Planet Patrol\0", NULL, "Spectravideo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_ppatrolRomInfo, a26_ppatrolRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Plaque Attack

static struct BurnRomInfo a26_plaqueatRomDesc[] = {
	{ "Plaque Attack (1983)(Activision).bin", 0x01000, 0xd5a5eb9e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_plaqueat)
STD_ROM_FN(a26_plaqueat)

struct BurnDriver BurnDrva26_plaqueat = {
	"a26_plaqueat", NULL, NULL, NULL, "1983",
	"Plaque Attack\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_plaqueatRomInfo, a26_plaqueatRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Plug Attack

static struct BurnRomInfo a26_plugatckRomDesc[] = {
	{ "Plug Attack (19xx)(Funvision).bin", 0x01000, 0x7b9d4c75, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_plugatck)
STD_ROM_FN(a26_plugatck)

struct BurnDriver BurnDrva26_plugatck = {
	"a26_plugatck", NULL, NULL, NULL, "19??",
	"Plug Attack\0", NULL, "Funvision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_plugatckRomInfo, a26_plugatckRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Polaris

static struct BurnRomInfo a26_polarisRomDesc[] = {
	{ "Polaris (1983)(Tigervision).bin", 0x02000, 0x25b78f89, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_polaris)
STD_ROM_FN(a26_polaris)

struct BurnDriver BurnDrva26_polaris = {
	"a26_polaris", NULL, NULL, NULL, "1983",
	"Polaris\0", NULL, "Tigervision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_polarisRomInfo, a26_polarisRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_3F, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pole Position

static struct BurnRomInfo a26_poleposRomDesc[] = {
	{ "Pole Position (1983)(Atari).bin", 0x02000, 0xbd822518, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_polepos)
STD_ROM_FN(a26_polepos)

struct BurnDriver BurnDrva26_polepos = {
	"a26_polepos", NULL, NULL, NULL, "1983",
	"Pole Position\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_poleposRomInfo, a26_poleposRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pooyan

static struct BurnRomInfo a26_pooyanRomDesc[] = {
	{ "Pooyan (1983)(Konami).bin", 0x01000, 0xfe24be60, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_pooyan)
STD_ROM_FN(a26_pooyan)

struct BurnDriver BurnDrva26_pooyan = {
	"a26_pooyan", NULL, NULL, NULL, "1983",
	"Pooyan\0", NULL, "Konami", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_pooyanRomInfo, a26_pooyanRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Popeye

static struct BurnRomInfo a26_popeyeRomDesc[] = {
	{ "Popeye (1983)(Parker Brothers).bin", 0x02000, 0x7d287f20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_popeye)
STD_ROM_FN(a26_popeye)

struct BurnDriver BurnDrva26_popeye = {
	"a26_popeye", NULL, NULL, NULL, "1983",
	"Popeye\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_popeyeRomInfo, a26_popeyeRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E0, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Porky's

static struct BurnRomInfo a26_porkysRomDesc[] = {
	{ "Porky's (1983)(20th Century Fox Video Games).bin", 0x02000, 0xadd0b98a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_porkys)
STD_ROM_FN(a26_porkys)

struct BurnDriver BurnDrva26_porkys = {
	"a26_porkys", NULL, NULL, NULL, "1983",
	"Porky's\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_porkysRomInfo, a26_porkysRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Pressure Cooker

static struct BurnRomInfo a26_pressureRomDesc[] = {
	{ "Pressure Cooker (1983)(Activision).bin", 0x02000, 0xccf597d8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_pressure)
STD_ROM_FN(a26_pressure)

struct BurnDriver BurnDrva26_pressure = {
	"a26_pressure", NULL, NULL, NULL, "1983",
	"Pressure Cooker\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_pressureRomInfo, a26_pressureRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Private Eye

static struct BurnRomInfo a26_privateRomDesc[] = {
	{ "Private Eye (1983)(Activision).bin", 0x02000, 0xea3bff1c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_private)
STD_ROM_FN(a26_private)

struct BurnDriver BurnDrva26_private = {
	"a26_private", NULL, NULL, NULL, "1983",
	"Private Eye\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_privateRomInfo, a26_privateRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Q*bert

static struct BurnRomInfo a26_qbertRomDesc[] = {
	{ "Q-bert (1983)(Parker Brothers).bin", 0x01000, 0x54142e3e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_qbert)
STD_ROM_FN(a26_qbert)

struct BurnDriver BurnDrva26_qbert = {
	"a26_qbert", NULL, NULL, NULL, "1983",
	"Q*bert\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_qbertRomInfo, a26_qbertRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Q*bert's Qubes

static struct BurnRomInfo a26_qbertqRomDesc[] = {
	{ "Q-bert's Qubes (1984)(Parker Brothers).bin", 0x02000, 0xd9f499c5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_qbertq)
STD_ROM_FN(a26_qbertq)

struct BurnDriver BurnDrva26_qbertq = {
	"a26_qbertq", NULL, NULL, NULL, "1984",
	"Q*bert's Qubes\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_qbertqRomInfo, a26_qbertqRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E0, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Quadrun

static struct BurnRomInfo a26_quadrunRomDesc[] = {
	{ "Quadrun (1983)(Atari).bin", 0x02000, 0x8d3c887b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_quadrun)
STD_ROM_FN(a26_quadrun)

struct BurnDriver BurnDrva26_quadrun = {
	"a26_quadrun", NULL, NULL, NULL, "1983",
	"Quadrun\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_quadrunRomInfo, a26_quadrunRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Quest for Quintana Roo

static struct BurnRomInfo a26_quintanaRomDesc[] = {
	{ "Quest for Quintana Roo (1984)(Sunrise Software).bin", 0x02000, 0xf9f58dd3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_quintana)
STD_ROM_FN(a26_quintana)

struct BurnDriver BurnDrva26_quintana = {
	"a26_quintana", NULL, NULL, NULL, "1984",
	"Quest for Quintana Roo\0", NULL, "Sunrise Software", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_quintanaRomInfo, a26_quintanaRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Quick Step!

static struct BurnRomInfo a26_quickstpRomDesc[] = {
	{ "Quick Step! (1983)(Imagic).bin", 0x01000, 0x057440fe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_quickstp)
STD_ROM_FN(a26_quickstp)

struct BurnDriver BurnDrva26_quickstp = {
	"a26_quickstp", NULL, NULL, NULL, "1983",
	"Quick Step!\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_quickstpRomInfo, a26_quickstpRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Racing Car (unknown)

static struct BurnRomInfo a26_racing1RomDesc[] = {
	{ "Racing Car (19xx)(Unknown).bin", 0x01000, 0xf78489b0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_racing1)
STD_ROM_FN(a26_racing1)

struct BurnDriver BurnDrva26_racing1 = {
	"a26_racing1", NULL, NULL, NULL, "19??",
	"Racing Car (unknown)\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_racing1RomInfo, a26_racing1RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Racquetball

static struct BurnRomInfo a26_racquetbRomDesc[] = {
	{ "Racquetball (1981)(Apollo).bin", 0x01000, 0x7eef9ac0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_racquetb)
STD_ROM_FN(a26_racquetb)

struct BurnDriver BurnDrva26_racquetb = {
	"a26_racquetb", NULL, NULL, NULL, "1981",
	"Racquetball\0", NULL, "Apollo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_racquetbRomInfo, a26_racquetbRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Radar

static struct BurnRomInfo a26_radarRomDesc[] = {
	{ "Radar (1983)(Unknown).bin", 0x01000, 0x7efdcaf1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_radar)
STD_ROM_FN(a26_radar)

struct BurnDriver BurnDrva26_radar = {
	"a26_radar", NULL, NULL, NULL, "1983",
	"Radar\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_radarRomInfo, a26_radarRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Radar Lock

static struct BurnRomInfo a26_radarlocRomDesc[] = {
	{ "Radar Lock (1989)(Atari).bin", 0x04000, 0xc29f7285, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_radarloc)
STD_ROM_FN(a26_radarloc)

struct BurnDriver BurnDrva26_radarloc = {
	"a26_radarloc", NULL, NULL, NULL, "1989",
	"Radar Lock\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_radarlocRomInfo, a26_radarlocRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Raft Rider

static struct BurnRomInfo a26_raftridrRomDesc[] = {
	{ "Raft Rider (1982)(U.S. Games Corporation).bin", 0x01000, 0x3d1292f8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_raftridr)
STD_ROM_FN(a26_raftridr)

struct BurnDriver BurnDrva26_raftridr = {
	"a26_raftridr", NULL, NULL, NULL, "1982",
	"Raft Rider\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_raftridrRomInfo, a26_raftridrRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Raiders of the Lost Ark

static struct BurnRomInfo a26_raidersRomDesc[] = {
	{ "Raiders of the Lost Ark (1982)(Atari).bin", 0x02000, 0xe05fe273, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_raiders)
STD_ROM_FN(a26_raiders)

struct BurnDriver BurnDrva26_raiders = {
	"a26_raiders", NULL, NULL, NULL, "1982",
	"Raiders of the Lost Ark\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_raidersRomInfo, a26_raidersRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Ram It

static struct BurnRomInfo a26_ramitRomDesc[] = {
	{ "Ram It (1982)(Telesys).bin", 0x01000, 0x582d5b62, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_ramit)
STD_ROM_FN(a26_ramit)

struct BurnDriver BurnDrva26_ramit = {
	"a26_ramit", NULL, NULL, NULL, "1982",
	"Ram It\0", NULL, "Telesys", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_ramitRomInfo, a26_ramitRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Rampage!

static struct BurnRomInfo a26_rampageRomDesc[] = {
	{ "Rampage! (1989)(Activision).bin", 0x04000, 0xf0b446ac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rampage)
STD_ROM_FN(a26_rampage)

struct BurnDriver BurnDrva26_rampage = {
	"a26_rampage", NULL, NULL, NULL, "1989",
	"Rampage!\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_rampageRomInfo, a26_rampageRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Reactor

static struct BurnRomInfo a26_reactorRomDesc[] = {
	{ "Reactor (1982)(Parker Brothers).bin", 0x01000, 0x1619e222, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_reactor)
STD_ROM_FN(a26_reactor)

struct BurnDriver BurnDrva26_reactor = {
	"a26_reactor", NULL, NULL, NULL, "1982",
	"Reactor\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_reactorRomInfo, a26_reactorRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// RealSports Baseball

static struct BurnRomInfo a26_rsbasebRomDesc[] = {
	{ "RealSports Baseball (1982)(Atari).bin", 0x02000, 0x3c5a1b5f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rsbaseb)
STD_ROM_FN(a26_rsbaseb)

struct BurnDriver BurnDrva26_rsbaseb = {
	"a26_rsbaseb", NULL, NULL, NULL, "1982",
	"RealSports Baseball\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_rsbasebRomInfo, a26_rsbasebRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// RealSports Boxing

static struct BurnRomInfo a26_rsboxingRomDesc[] = {
	{ "RealSports Boxing (1987)(Atari).bin", 0x04000, 0x3398a1b2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rsboxing)
STD_ROM_FN(a26_rsboxing)

struct BurnDriver BurnDrva26_rsboxing = {
	"a26_rsboxing", NULL, NULL, NULL, "1987",
	"RealSports Boxing\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_rsboxingRomInfo, a26_rsboxingRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// RealSports Football

static struct BurnRomInfo a26_rsfootbRomDesc[] = {
	{ "RealSports Football (1982)(Atari).bin", 0x02000, 0xf89f64ef, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rsfootb)
STD_ROM_FN(a26_rsfootb)

struct BurnDriver BurnDrva26_rsfootb = {
	"a26_rsfootb", NULL, NULL, NULL, "1982",
	"RealSports Football\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_rsfootbRomInfo, a26_rsfootbRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// RealSports Soccer - Football - RealSports Soccer

static struct BurnRomInfo a26_rssoccerRomDesc[] = {
	{ "RealSports Soccer (1983)(Atari).bin", 0x02000, 0x02d89819, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rssoccer)
STD_ROM_FN(a26_rssoccer)

struct BurnDriver BurnDrva26_rssoccer = {
	"a26_rssoccer", NULL, NULL, NULL, "1983",
	"RealSports Soccer - Football - RealSports Soccer\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_rssoccerRomInfo, a26_rssoccerRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// RealSports Tennis

static struct BurnRomInfo a26_rstennisRomDesc[] = {
	{ "RealSports Tennis (1983)(Unknown).bin", 0x02000, 0x379001a0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rstennis)
STD_ROM_FN(a26_rstennis)

struct BurnDriver BurnDrva26_rstennis = {
	"a26_rstennis", NULL, NULL, NULL, "1983",
	"RealSports Tennis\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_rstennisRomInfo, a26_rstennisRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// RealSports Volleyball

static struct BurnRomInfo a26_rsvolleyRomDesc[] = {
	{ "RealSports Volleyball (1982)(Atari).bin", 0x01000, 0x54fc9eb2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rsvolley)
STD_ROM_FN(a26_rsvolley)

struct BurnDriver BurnDrva26_rsvolley = {
	"a26_rsvolley", NULL, NULL, NULL, "1982",
	"RealSports Volleyball\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_rsvolleyRomInfo, a26_rsvolleyRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Rescue Terra I

static struct BurnRomInfo a26_rescueteRomDesc[] = {
	{ "Rescue Terra I (1982)(VentureVision).bin", 0x01000, 0xa509ad53, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rescuete)
STD_ROM_FN(a26_rescuete)

struct BurnDriver BurnDrva26_rescuete = {
	"a26_rescuete", NULL, NULL, NULL, "1982",
	"Rescue Terra I\0", NULL, "VentureVision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_rescueteRomInfo, a26_rescueteRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Resgate Espacial

static struct BurnRomInfo a26_resgateRomDesc[] = {
	{ "Resgate Espacial (19xx)(CCE).bin", 0x02000, 0x0c139bb8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_resgate)
STD_ROM_FN(a26_resgate)

struct BurnDriver BurnDrva26_resgate = {
	"a26_resgate", NULL, NULL, NULL, "19??",
	"Resgate Espacial\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_resgateRomInfo, a26_resgateRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8_B1, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Revenge of the Beefsteak Tomatoes

static struct BurnRomInfo a26_beeftomRomDesc[] = {
	{ "Revenge of the Beefsteak Tomatoes (1982)(20th Century Fox Video Games).bin", 0x01000, 0x243a36df, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_beeftom)
STD_ROM_FN(a26_beeftom)

struct BurnDriver BurnDrva26_beeftom = {
	"a26_beeftom", NULL, NULL, NULL, "1982",
	"Revenge of the Beefsteak Tomatoes\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_beeftomRomInfo, a26_beeftomRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Riddle of the Sphinx

static struct BurnRomInfo a26_rsphinxRomDesc[] = {
	{ "Riddle of the Sphinx (1982)(Imagic).bin", 0x01000, 0x183a87e3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rsphinx)
STD_ROM_FN(a26_rsphinx)

struct BurnDriver BurnDrva26_rsphinx = {
	"a26_rsphinx", NULL, NULL, NULL, "1982",
	"Riddle of the Sphinx\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_rsphinxRomInfo, a26_rsphinxRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// River Patrol

static struct BurnRomInfo a26_rpatrolRomDesc[] = {
	{ "River Patrol (1984)(Tigervision).bin", 0x02000, 0xc820bd75, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rpatrol)
STD_ROM_FN(a26_rpatrol)

struct BurnDriver BurnDrva26_rpatrol = {
	"a26_rpatrol", NULL, NULL, NULL, "1984",
	"River Patrol\0", NULL, "Tigervision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_rpatrolRomInfo, a26_rpatrolRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_3F, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// River Raid

static struct BurnRomInfo a26_riveraidRomDesc[] = {
	{ "River Raid (1982)(Activision).bin", 0x01000, 0xc3eb7e1e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_riveraid)
STD_ROM_FN(a26_riveraid)

struct BurnDriver BurnDrva26_riveraid = {
	"a26_riveraid", NULL, NULL, NULL, "1982",
	"River Raid\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_riveraidRomInfo, a26_riveraidRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// River Raid II

static struct BurnRomInfo a26_riverai2RomDesc[] = {
	{ "River Raid II (1988)(Activision).bin", 0x04000, 0x27d2df2c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_riverai2)
STD_ROM_FN(a26_riverai2)

struct BurnDriver BurnDrva26_riverai2 = {
	"a26_riverai2", NULL, NULL, NULL, "1988",
	"River Raid II\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_riverai2RomInfo, a26_riverai2RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Road Runner

static struct BurnRomInfo a26_roadrunRomDesc[] = {
	{ "Road Runner (1989)(Atari).bin", 0x04000, 0xcb79c061, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_roadrun)
STD_ROM_FN(a26_roadrun)

struct BurnDriver BurnDrva26_roadrun = {
	"a26_roadrun", NULL, NULL, NULL, "1989",
	"Road Runner\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_roadrunRomInfo, a26_roadrunRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Robin Hood

static struct BurnRomInfo a26_robinhRomDesc[] = {
	{ "Robin Hood (1983)(Xonox).bin", 0x02000, 0xdf96102b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_robinh)
STD_ROM_FN(a26_robinh)

struct BurnDriver BurnDrva26_robinh = {
	"a26_robinh", NULL, NULL, NULL, "1983",
	"Robin Hood\0", NULL, "Xonox", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_robinhRomInfo, a26_robinhRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Robot Tank

static struct BurnRomInfo a26_robottnkRomDesc[] = {
	{ "Robot Tank (1983)(Activision).bin", 0x02000, 0xe127c012, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_robottnk)
STD_ROM_FN(a26_robottnk)

struct BurnDriver BurnDrva26_robottnk = {
	"a26_robottnk", NULL, NULL, NULL, "1983",
	"Robot Tank\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_robottnkRomInfo, a26_robottnkRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_FE, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Roc 'n Rope

static struct BurnRomInfo a26_rocnropeRomDesc[] = {
	{ "Roc 'n Rope (1984)(Coleco).bin", 0x02000, 0x9d51c969, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rocnrope)
STD_ROM_FN(a26_rocnrope)

struct BurnDriver BurnDrva26_rocnrope = {
	"a26_rocnrope", NULL, NULL, NULL, "1984",
	"Roc 'n Rope\0", NULL, "Coleco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_rocnropeRomInfo, a26_rocnropeRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Room of Doom

static struct BurnRomInfo a26_roomdoomRomDesc[] = {
	{ "Room of Doom (1982)(CommaVid).bin", 0x01000, 0xf27fc3be, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_roomdoom)
STD_ROM_FN(a26_roomdoom)

struct BurnDriver BurnDrva26_roomdoom = {
	"a26_roomdoom", NULL, NULL, NULL, "1982",
	"Room of Doom\0", NULL, "CommaVid", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_roomdoomRomInfo, a26_roomdoomRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Rubik's Cube

static struct BurnRomInfo a26_rubikcubRomDesc[] = {
	{ "Rubik's Cube (1984)(Atari).bin", 0x01000, 0x37f23cb8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rubikcub)
STD_ROM_FN(a26_rubikcub)

struct BurnDriver BurnDrva26_rubikcub = {
	"a26_rubikcub", NULL, NULL, NULL, "1984",
	"Rubik's Cube\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_rubikcubRomInfo, a26_rubikcubRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Save Our Ship (unknown)

static struct BurnRomInfo a26_sos1RomDesc[] = {
	{ "Save Our Ship (19xx)(Unknown).bin", 0x01000, 0xc15a66e7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sos1)
STD_ROM_FN(a26_sos1)

struct BurnDriver BurnDrva26_sos1 = {
	"a26_sos1", NULL, NULL, NULL, "19??",
	"Save Our Ship (unknown)\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sos1RomInfo, a26_sos1RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Scuba Diver

static struct BurnRomInfo a26_scubaRomDesc[] = {
	{ "Scuba Diver (1983)(Unknown).bin", 0x01000, 0x3869182c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_scuba)
STD_ROM_FN(a26_scuba)

struct BurnDriver BurnDrva26_scuba = {
	"a26_scuba", NULL, NULL, NULL, "1983",
	"Scuba Diver\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_scubaRomInfo, a26_scubaRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sea Battle

static struct BurnRomInfo a26_seabattlRomDesc[] = {
	{ "Sea Battle (1983)(M Network).bin", 0x01000, 0x83b4eb19, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_seabattl)
STD_ROM_FN(a26_seabattl)

struct BurnDriver BurnDrva26_seabattl = {
	"a26_seabattl", NULL, NULL, NULL, "1983",
	"Sea Battle\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_seabattlRomInfo, a26_seabattlRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sea Hawk

static struct BurnRomInfo a26_seahawkeRomDesc[] = {
	{ "Sea Hawk (1983)(Panda).bin", 0x01000, 0x5cc76c35, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_seahawke)
STD_ROM_FN(a26_seahawke)

struct BurnDriver BurnDrva26_seahawke = {
	"a26_seahawke", NULL, NULL, NULL, "1983",
	"Sea Hawk\0", NULL, "Panda", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_seahawkeRomInfo, a26_seahawkeRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sea Hunt (CCE)

static struct BurnRomInfo a26_seahuntRomDesc[] = {
	{ "Sea Hunt (19xx)(CCE).bin", 0x01000, 0xc4d18781, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_seahunt)
STD_ROM_FN(a26_seahunt)

struct BurnDriver BurnDrva26_seahunt = {
	"a26_seahunt", NULL, NULL, NULL, "19??",
	"Sea Hunt (CCE)\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_seahuntRomInfo, a26_seahuntRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Seamonster - Monstre des Mers

static struct BurnRomInfo a26_seamonstRomDesc[] = {
	{ "Seamonster (1982)(Bit Corporation).bin", 0x01000, 0xe913a209, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_seamonst)
STD_ROM_FN(a26_seamonst)

struct BurnDriver BurnDrva26_seamonst = {
	"a26_seamonst", NULL, NULL, NULL, "1982",
	"Seamonster - Monstre des Mers\0", NULL, "Bit Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_seamonstRomInfo, a26_seamonstRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Seaquest

static struct BurnRomInfo a26_seaquestRomDesc[] = {
	{ "Seaquest (1983)(Activision).bin", 0x01000, 0x8658e8d9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_seaquest)
STD_ROM_FN(a26_seaquest)

struct BurnDriver BurnDrva26_seaquest = {
	"a26_seaquest", NULL, NULL, NULL, "1983",
	"Seaquest\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_seaquestRomInfo, a26_seaquestRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Secret Quest

static struct BurnRomInfo a26_squestRomDesc[] = {
	{ "Secret Quest (1989)(Atari).bin", 0x04000, 0x93c9eb47, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_squest)
STD_ROM_FN(a26_squest)

struct BurnDriver BurnDrva26_squest = {
	"a26_squest", NULL, NULL, NULL, "1989",
	"Secret Quest\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_squestRomInfo, a26_squestRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Shark Attack

static struct BurnRomInfo a26_sharkattRomDesc[] = {
	{ "Shark Attack (1982)(Apollo).bin", 0x01000, 0xf86448f5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sharkatt)
STD_ROM_FN(a26_sharkatt)

struct BurnDriver BurnDrva26_sharkatt = {
	"a26_sharkatt", NULL, NULL, NULL, "1982",
	"Shark Attack\0", NULL, "Apollo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sharkattRomInfo, a26_sharkattRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Shootin' Gallery

static struct BurnRomInfo a26_shootingRomDesc[] = {
	{ "Shootin' Gallery (1982)(Imagic).bin", 0x01000, 0xb89aff8b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_shooting)
STD_ROM_FN(a26_shooting)

struct BurnDriver BurnDrva26_shooting = {
	"a26_shooting", NULL, NULL, NULL, "1982",
	"Shootin' Gallery\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_shootingRomInfo, a26_shootingRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Shuttle Orbiter

static struct BurnRomInfo a26_shuttlorRomDesc[] = {
	{ "Shuttle Orbiter (1983)(Avalon Hill).bin", 0x01000, 0x253d6396, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_shuttlor)
STD_ROM_FN(a26_shuttlor)

struct BurnDriver BurnDrva26_shuttlor = {
	"a26_shuttlor", NULL, NULL, NULL, "1983",
	"Shuttle Orbiter\0", NULL, "Avalon Hill", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_shuttlorRomInfo, a26_shuttlorRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sir Lancelot

static struct BurnRomInfo a26_sirlanceRomDesc[] = {
	{ "Sir Lancelot (1983)(Xonox).bin", 0x02000, 0xeb792891, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sirlance)
STD_ROM_FN(a26_sirlance)

struct BurnDriver BurnDrva26_sirlance = {
	"a26_sirlance", NULL, NULL, NULL, "1983",
	"Sir Lancelot\0", NULL, "Xonox", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sirlanceRomInfo, a26_sirlanceRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Skate Boardin'

static struct BurnRomInfo a26_skatebRomDesc[] = {
	{ "Skate Boardin' (1987)(Absolute Entertainment).bin", 0x02000, 0x6ee721f7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_skateb)
STD_ROM_FN(a26_skateb)

struct BurnDriver BurnDrva26_skateb = {
	"a26_skateb", NULL, NULL, NULL, "1987",
	"Skate Boardin'\0", NULL, "Absolute Entertainment", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_skatebRomInfo, a26_skatebRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Skeet Shoot

static struct BurnRomInfo a26_skeetRomDesc[] = {
	{ "Skeet Shoot (1981)(Apollo).bin", 0x00800, 0x57d69b13, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_skeet)
STD_ROM_FN(a26_skeet)

struct BurnDriver BurnDrva26_skeet = {
	"a26_skeet", NULL, NULL, NULL, "1981",
	"Skeet Shoot\0", NULL, "Apollo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_skeetRomInfo, a26_skeetRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Skiing ~ Le Ski (USA)

static struct BurnRomInfo a26_skiingRomDesc[] = {
	{ "Skiing (1980)(Activision).bin", 0x00800, 0x35f80a22, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_skiing)
STD_ROM_FN(a26_skiing)

struct BurnDriver BurnDrva26_skiing = {
	"a26_skiing", NULL, NULL, NULL, "1980",
	"Skiing ~ Le Ski (USA)\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_SPORTSMISC, 0,
	A2600GetZipName, a26_skiingRomInfo, a26_skiingRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sky Diver ~ Dare Diver (USA)

static struct BurnRomInfo a26_skydiverRomDesc[] = {
	{ "Sky Diver (1979)(Atari).bin", 0x00800, 0x00312ea9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_skydiver)
STD_ROM_FN(a26_skydiver)

struct BurnDriver BurnDrva26_skydiver = {
	"a26_skydiver", NULL, NULL, NULL, "1979",
	"Sky Diver ~ Dare Diver (USA)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_ACTION, 0,
	A2600GetZipName, a26_skydiverRomInfo, a26_skydiverRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sky Jinks (USA)

static struct BurnRomInfo a26_skyjinkRomDesc[] = {
	{ "Sky Jinks (1982)(Activision).bin", 0x00800, 0x96f8cf59, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_skyjink)
STD_ROM_FN(a26_skyjink)

struct BurnDriver BurnDrva26_skyjink = {
	"a26_skyjink", NULL, NULL, NULL, "1982",
	"Sky Jinks (USA)\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_ACTION, 0,
	A2600GetZipName, a26_skyjinkRomInfo, a26_skyjinkRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sky Skipper

static struct BurnRomInfo a26_skyskippRomDesc[] = {
	{ "Sky Skipper (1983)(Parker Brothers).bin", 0x01000, 0xcc6e0f22, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_skyskipp)
STD_ROM_FN(a26_skyskipp)

struct BurnDriver BurnDrva26_skyskipp = {
	"a26_skyskipp", NULL, NULL, NULL, "1983",
	"Sky Skipper\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_skyskippRomInfo, a26_skyskippRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Slot Machine - Slots

static struct BurnRomInfo a26_slotRomDesc[] = {
	{ "Slot Machine (1979)(Atari).bin", 0x00800, 0xcb378d95, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_slot)
STD_ROM_FN(a26_slot)

struct BurnDriver BurnDrva26_slot = {
	"a26_slot", NULL, NULL, NULL, "1979",
	"Slot Machine - Slots\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_slotRomInfo, a26_slotRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Slot Racers - Maze

static struct BurnRomInfo a26_slotraceRomDesc[] = {
	{ "Slot Racers (1978)(Atari).bin", 0x00800, 0x177dbdf8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_slotrace)
STD_ROM_FN(a26_slotrace)

struct BurnDriver BurnDrva26_slotrace = {
	"a26_slotrace", NULL, NULL, NULL, "1978",
	"Slot Racers - Maze\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_slotraceRomInfo, a26_slotraceRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Smurf - Rescue in Gargamel's Castle

static struct BurnRomInfo a26_smurfsRomDesc[] = {
	{ "Smurf - Rescue in Gargamel's Castle (1982)(Coleco).bin", 0x02000, 0xe0624a7f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_smurfs)
STD_ROM_FN(a26_smurfs)

struct BurnDriver BurnDrva26_smurfs = {
	"a26_smurfs", NULL, NULL, NULL, "1982",
	"Smurf - Rescue in Gargamel's Castle\0", NULL, "Coleco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_smurfsRomInfo, a26_smurfsRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Smurfs Save the Day

static struct BurnRomInfo a26_smurfssdRomDesc[] = {
	{ "Smurfs Save the Day (1983)(Coleco).bin", 0x02000, 0xad89c697, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_smurfssd)
STD_ROM_FN(a26_smurfssd)

struct BurnDriver BurnDrva26_smurfssd = {
	"a26_smurfssd", NULL, NULL, NULL, "1983",
	"Smurfs Save the Day\0", NULL, "Coleco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_smurfssdRomInfo, a26_smurfssdRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sneak 'n Peek

static struct BurnRomInfo a26_sneakpekRomDesc[] = {
	{ "Sneak 'n Peek (1982)(U.S. Games Corporation).bin", 0x01000, 0x93b17c39, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sneakpek)
STD_ROM_FN(a26_sneakpek)

struct BurnDriver BurnDrva26_sneakpek = {
	"a26_sneakpek", NULL, NULL, NULL, "1982",
	"Sneak 'n Peek\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sneakpekRomInfo, a26_sneakpekRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Snoopy and the Red Baron

static struct BurnRomInfo a26_snoopyRomDesc[] = {
	{ "Snoopy and the Red Baron (1983)(Atari).bin", 0x02000, 0xd1039967, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_snoopy)
STD_ROM_FN(a26_snoopy)

struct BurnDriver BurnDrva26_snoopy = {
	"a26_snoopy", NULL, NULL, NULL, "1983",
	"Snoopy and the Red Baron\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_snoopyRomInfo, a26_snoopyRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Soccer

static struct BurnRomInfo a26_soccerRomDesc[] = {
	{ "Soccer (1989)(Telegames).bin", 0x01000, 0xebc723c0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_soccer)
STD_ROM_FN(a26_soccer)

struct BurnDriver BurnDrva26_soccer = {
	"a26_soccer", NULL, NULL, NULL, "1989",
	"Soccer\0", NULL, "Telegames", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_soccerRomInfo, a26_soccerRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Solar Fox

static struct BurnRomInfo a26_solarfoxRomDesc[] = {
	{ "Solar Fox (1983)(CBS Electronics).bin", 0x02000, 0xd2ca6ce8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_solarfox)
STD_ROM_FN(a26_solarfox)

struct BurnDriver BurnDrva26_solarfox = {
	"a26_solarfox", NULL, NULL, NULL, "1983",
	"Solar Fox\0", NULL, "CBS Electronics", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_solarfoxRomInfo, a26_solarfoxRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Solar Storm

static struct BurnRomInfo a26_solarstrRomDesc[] = {
	{ "Solar Storm (1983)(Imagic).bin", 0x01000, 0xa970b3e3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_solarstr)
STD_ROM_FN(a26_solarstr)

struct BurnDriver BurnDrva26_solarstr = {
	"a26_solarstr", NULL, NULL, NULL, "1983",
	"Solar Storm\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_solarstrRomInfo, a26_solarstrRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Solaris

static struct BurnRomInfo a26_solarisRomDesc[] = {
	{ "Solaris (1986)(Atari).bin", 0x04000, 0x2b87850e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_solaris)
STD_ROM_FN(a26_solaris)

struct BurnDriver BurnDrva26_solaris = {
	"a26_solaris", NULL, NULL, NULL, "1986",
	"Solaris\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_solarisRomInfo, a26_solarisRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sorcerer

static struct BurnRomInfo a26_sorcererRomDesc[] = {
	{ "Sorcerer (1983)(Mythicon).bin", 0x01000, 0xc7734be6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sorcerer)
STD_ROM_FN(a26_sorcerer)

struct BurnDriver BurnDrva26_sorcerer = {
	"a26_sorcerer", NULL, NULL, NULL, "1983",
	"Sorcerer\0", NULL, "Mythicon", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sorcererRomInfo, a26_sorcererRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sorcerer's Apprentice

static struct BurnRomInfo a26_sorcapprRomDesc[] = {
	{ "Sorcerer's Apprentice (1983)(Atari).bin", 0x02000, 0xe5dbfed1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sorcappr)
STD_ROM_FN(a26_sorcappr)

struct BurnDriver BurnDrva26_sorcappr = {
	"a26_sorcappr", NULL, NULL, NULL, "1983",
	"Sorcerer's Apprentice\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sorcapprRomInfo, a26_sorcapprRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Space Adventure

static struct BurnRomInfo a26_spaceadvRomDesc[] = {
	{ "Space Adventure (19xx)(Zellers).bin", 0x01000, 0x0d61ce7d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spaceadv)
STD_ROM_FN(a26_spaceadv)

struct BurnDriver BurnDrva26_spaceadv = {
	"a26_spaceadv", NULL, NULL, NULL, "19??",
	"Space Adventure\0", NULL, "Zellers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spaceadvRomInfo, a26_spaceadvRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Space Attack

static struct BurnRomInfo a26_spaceattRomDesc[] = {
	{ "Space Attack (1982)(M Network).bin", 0x01000, 0xb34a3e57, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spaceatt)
STD_ROM_FN(a26_spaceatt)

struct BurnDriver BurnDrva26_spaceatt = {
	"a26_spaceatt", NULL, NULL, NULL, "1982",
	"Space Attack\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spaceattRomInfo, a26_spaceattRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Space Canyon

static struct BurnRomInfo a26_spacecanRomDesc[] = {
	{ "Space Canyon (1983)(Unknown).bin", 0x01000, 0xce7268b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spacecan)
STD_ROM_FN(a26_spacecan)

struct BurnDriver BurnDrva26_spacecan = {
	"a26_spacecan", NULL, NULL, NULL, "1983",
	"Space Canyon\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spacecanRomInfo, a26_spacecanRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Space Cavern

static struct BurnRomInfo a26_spacecavRomDesc[] = {
	{ "Space Cavern (1981)(Apollo).bin", 0x01000, 0x3b0ffd89, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spacecav)
STD_ROM_FN(a26_spacecav)

struct BurnDriver BurnDrva26_spacecav = {
	"a26_spacecav", NULL, NULL, NULL, "1981",
	"Space Cavern\0", NULL, "Apollo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spacecavRomInfo, a26_spacecavRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Space Invaders

static struct BurnRomInfo a26_spaceinvRomDesc[] = {
	{ "Space Invaders (1980)(Atari).bin", 0x01000, 0xa6e867b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spaceinv)
STD_ROM_FN(a26_spaceinv)

struct BurnDriver BurnDrva26_spaceinv = {
	"a26_spaceinv", NULL, NULL, NULL, "1980",
	"Space Invaders\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spaceinvRomInfo, a26_spaceinvRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Space Jockey

static struct BurnRomInfo a26_spacejocRomDesc[] = {
	{ "Space Jockey (1982)(U.S. Games Corporation).bin", 0x00800, 0xab24d73e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spacejoc)
STD_ROM_FN(a26_spacejoc)

struct BurnDriver BurnDrva26_spacejoc = {
	"a26_spacejoc", NULL, NULL, NULL, "1982",
	"Space Jockey\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spacejocRomInfo, a26_spacejocRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Space Robot

static struct BurnRomInfo a26_spcrobotRomDesc[] = {
	{ "Space Robot (19xx)(Sinmax).bin", 0x01000, 0x8fffc351, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spcrobot)
STD_ROM_FN(a26_spcrobot)

struct BurnDriver BurnDrva26_spcrobot = {
	"a26_spcrobot", NULL, NULL, NULL, "19??",
	"Space Robot\0", NULL, "Sinmax", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spcrobotRomInfo, a26_spcrobotRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Space Shuttle - A Journey Into Space

static struct BurnRomInfo a26_shuttleRomDesc[] = {
	{ "Space Shuttle - A Journey Into Space (1983)(Activision).bin", 0x02000, 0xdd210cf3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_shuttle)
STD_ROM_FN(a26_shuttle)

struct BurnDriver BurnDrva26_shuttle = {
	"a26_shuttle", NULL, NULL, NULL, "1983",
	"Space Shuttle - A Journey Into Space\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_shuttleRomInfo, a26_shuttleRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Space Tunnel - Weltraum-Tunnel

static struct BurnRomInfo a26_spacetunRomDesc[] = {
	{ "Space Tunnel (1982)(Bit Corporation).bin", 0x01000, 0xf09f489f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spacetun)
STD_ROM_FN(a26_spacetun)

struct BurnDriver BurnDrva26_spacetun = {
	"a26_spacetun", NULL, NULL, NULL, "1982",
	"Space Tunnel - Weltraum-Tunnel\0", NULL, "Bit Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spacetunRomInfo, a26_spacetunRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Space War - Space Combat

static struct BurnRomInfo a26_spacewarRomDesc[] = {
	{ "Space War (1978)(Atari).bin", 0x00800, 0x65622f2a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spacewar)
STD_ROM_FN(a26_spacewar)

struct BurnDriver BurnDrva26_spacewar = {
	"a26_spacewar", NULL, NULL, NULL, "1978",
	"Space War - Space Combat\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spacewarRomInfo, a26_spacewarRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Spacechase

static struct BurnRomInfo a26_schaseRomDesc[] = {
	{ "Spacechase (1981)(Apollo).bin", 0x01000, 0x2954d22d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_schase)
STD_ROM_FN(a26_schase)

struct BurnDriver BurnDrva26_schase = {
	"a26_schase", NULL, NULL, NULL, "1981",
	"Spacechase\0", NULL, "Apollo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_schaseRomInfo, a26_schaseRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// SpaceMaster X-7

static struct BurnRomInfo a26_smastx7RomDesc[] = {
	{ "SpaceMaster X-7 (1983)(20th Century Fox Video Games).bin", 0x01000, 0xe554d7d0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_smastx7)
STD_ROM_FN(a26_smastx7)

struct BurnDriver BurnDrva26_smastx7 = {
	"a26_smastx7", NULL, NULL, NULL, "1983",
	"SpaceMaster X-7\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_smastx7RomInfo, a26_smastx7RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Spider Fighter

static struct BurnRomInfo a26_spidfghtRomDesc[] = {
	{ "Spider Fighter (1982)(Activision).bin", 0x01000, 0xbf0941f6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spidfght)
STD_ROM_FN(a26_spidfght)

struct BurnDriver BurnDrva26_spidfght = {
	"a26_spidfght", NULL, NULL, NULL, "1982",
	"Spider Fighter\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spidfghtRomInfo, a26_spidfghtRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Spider Maze

static struct BurnRomInfo a26_spidmazeRomDesc[] = {
	{ "Spider Maze (1982)(K-Tel Vision).bin", 0x01000, 0xfbc1911d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spidmaze)
STD_ROM_FN(a26_spidmaze)

struct BurnDriver BurnDrva26_spidmaze = {
	"a26_spidmaze", NULL, NULL, NULL, "1982",
	"Spider Maze\0", NULL, "K-Tel Vision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spidmazeRomInfo, a26_spidmazeRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Spider-Man

static struct BurnRomInfo a26_spidermnRomDesc[] = {
	{ "Spider-Man (1982)(Parker Brothers).bin", 0x01000, 0x4dfea848, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spidermn)
STD_ROM_FN(a26_spidermn)

struct BurnDriver BurnDrva26_spidermn = {
	"a26_spidermn", NULL, NULL, NULL, "1982",
	"Spider-Man\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spidermnRomInfo, a26_spidermnRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Spiderdroid

static struct BurnRomInfo a26_spidroidRomDesc[] = {
	{ "Spiderdroid (1987)(Unknown).bin", 0x01000, 0x4347ae8e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spidroid)
STD_ROM_FN(a26_spidroid)

struct BurnDriver BurnDrva26_spidroid = {
	"a26_spidroid", NULL, NULL, NULL, "1987",
	"Spiderdroid\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spidroidRomInfo, a26_spidroidRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Spike's Peak

static struct BurnRomInfo a26_spikpeakRomDesc[] = {
	{ "Spike's Peak (1983)(Xonox).bin", 0x02000, 0x50efea8d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spikpeak)
STD_ROM_FN(a26_spikpeak)

struct BurnDriver BurnDrva26_spikpeak = {
	"a26_spikpeak", NULL, NULL, NULL, "1983",
	"Spike's Peak\0", NULL, "Xonox", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spikpeakRomInfo, a26_spikpeakRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Spitfire Attack

static struct BurnRomInfo a26_spitfireRomDesc[] = {
	{ "Spitfire Attack (1983)(Milton Bradley Company).bin", 0x01000, 0x093507d8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spitfire)
STD_ROM_FN(a26_spitfire)

struct BurnDriver BurnDrva26_spitfire = {
	"a26_spitfire", NULL, NULL, NULL, "1983",
	"Spitfire Attack\0", NULL, "Milton Bradley Company", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spitfireRomInfo, a26_spitfireRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Springer

static struct BurnRomInfo a26_springerRomDesc[] = {
	{ "Springer (1982)(Tigervision).bin", 0x02000, 0xdd183a4f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_springer)
STD_ROM_FN(a26_springer)

struct BurnDriver BurnDrva26_springer = {
	"a26_springer", NULL, NULL, NULL, "1982",
	"Springer\0", NULL, "Tigervision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_springerRomInfo, a26_springerRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_3F, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sprint Master

static struct BurnRomInfo a26_sprintmRomDesc[] = {
	{ "Sprint Master (1988)(Atari).bin", 0x04000, 0xc495904e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sprintm)
STD_ROM_FN(a26_sprintm)

struct BurnDriver BurnDrva26_sprintm = {
	"a26_sprintm", NULL, NULL, NULL, "1988",
	"Sprint Master\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sprintmRomInfo, a26_sprintmRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Spy Hunter

static struct BurnRomInfo a26_spyhuntRomDesc[] = {
	{ "Spy Hunter (1984)(Unknown).bin", 0x02000, 0x4f804e49, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_spyhunt)
STD_ROM_FN(a26_spyhunt)

struct BurnDriver BurnDrva26_spyhunt = {
	"a26_spyhunt", NULL, NULL, NULL, "1984",
	"Spy Hunter\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_spyhuntRomInfo, a26_spyhuntRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Squeeze Box

static struct BurnRomInfo a26_squeezeRomDesc[] = {
	{ "Squeeze Box (1982)(U.S. Games Corporation).bin", 0x01000, 0xd37f3fb3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_squeeze)
STD_ROM_FN(a26_squeeze)

struct BurnDriver BurnDrva26_squeeze = {
	"a26_squeeze", NULL, NULL, NULL, "1982",
	"Squeeze Box\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_squeezeRomInfo, a26_squeezeRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Squirrel - O Esquilo

static struct BurnRomInfo a26_squirrelRomDesc[] = {
	{ "Squirrel (1983)(Unknown).bin", 0x01000, 0xac2920ef, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_squirrel)
STD_ROM_FN(a26_squirrel)

struct BurnDriver BurnDrva26_squirrel = {
	"a26_squirrel", NULL, NULL, NULL, "1983",
	"Squirrel - O Esquilo\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_squirrelRomInfo, a26_squirrelRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sssnake

static struct BurnRomInfo a26_sssnakeRomDesc[] = {
	{ "Sssnake (1982)(Data Age).bin", 0x01000, 0x1b5b0212, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sssnake)
STD_ROM_FN(a26_sssnake)

struct BurnDriver BurnDrva26_sssnake = {
	"a26_sssnake", NULL, NULL, NULL, "1982",
	"Sssnake\0", NULL, "Data Age", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sssnakeRomInfo, a26_sssnakeRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Stampede

static struct BurnRomInfo a26_stampedeRomDesc[] = {
	{ "Stampede (1981)(Activision).bin", 0x00800, 0x740800f3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_stampede)
STD_ROM_FN(a26_stampede)

struct BurnDriver BurnDrva26_stampede = {
	"a26_stampede", NULL, NULL, NULL, "1981",
	"Stampede\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_stampedeRomInfo, a26_stampedeRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Star Fox

static struct BurnRomInfo a26_starfoxRomDesc[] = {
	{ "Star Fox (1983)(Mythicon).bin", 0x01000, 0x915ec040, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_starfox)
STD_ROM_FN(a26_starfox)

struct BurnDriver BurnDrva26_starfox = {
	"a26_starfox", NULL, NULL, NULL, "1983",
	"Star Fox\0", NULL, "Mythicon", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_starfoxRomInfo, a26_starfoxRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Star Ship - Outer Space

static struct BurnRomInfo a26_starshipRomDesc[] = {
	{ "Star Ship (1977)(Atari).bin", 0x00800, 0x6609267e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_starship)
STD_ROM_FN(a26_starship)

struct BurnDriver BurnDrva26_starship = {
	"a26_starship", NULL, NULL, NULL, "1977",
	"Star Ship - Outer Space\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_starshipRomInfo, a26_starshipRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Star Strike

static struct BurnRomInfo a26_starstrkRomDesc[] = {
	{ "Star Strike (1983)(M Network).bin", 0x01000, 0x936a596d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_starstrk)
STD_ROM_FN(a26_starstrk)

struct BurnDriver BurnDrva26_starstrk = {
	"a26_starstrk", NULL, NULL, NULL, "1983",
	"Star Strike\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_starstrkRomInfo, a26_starstrkRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Star Trek - Strategic Operations Simulator

static struct BurnRomInfo a26_startrekRomDesc[] = {
	{ "Star Trek - Strategic Operations Simulator (1983)(Sega).bin", 0x02000, 0x820ea8a2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_startrek)
STD_ROM_FN(a26_startrek)

struct BurnDriver BurnDrva26_startrek = {
	"a26_startrek", NULL, NULL, NULL, "1983",
	"Star Trek - Strategic Operations Simulator\0", NULL, "Sega", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_startrekRomInfo, a26_startrekRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Star Voyager

static struct BurnRomInfo a26_svoyagerRomDesc[] = {
	{ "Star Voyager (1982)(Imagic).bin", 0x01000, 0x4fe8477f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_svoyager)
STD_ROM_FN(a26_svoyager)

struct BurnDriver BurnDrva26_svoyager = {
	"a26_svoyager", NULL, NULL, NULL, "1982",
	"Star Voyager\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_svoyagerRomInfo, a26_svoyagerRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Star Wars - Jedi Arena

static struct BurnRomInfo a26_jediarenRomDesc[] = {
	{ "Star Wars - Jedi Arena (1983)(Parker Brothers).bin", 0x01000, 0x9e36d347, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_jediaren)
STD_ROM_FN(a26_jediaren)

struct BurnDriver BurnDrva26_jediaren = {
	"a26_jediaren", NULL, NULL, NULL, "1983",
	"Star Wars - Jedi Arena\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_jediarenRomInfo, a26_jediarenRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Star Wars - Return of the Jedi - Death Star Battle

static struct BurnRomInfo a26_jedidsbRomDesc[] = {
	{ "Star Wars - Return of the Jedi - Death Star Battle (1983)(Parker Brothers).bin", 0x02000, 0x0886a55d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_jedidsb)
STD_ROM_FN(a26_jedidsb)

struct BurnDriver BurnDrva26_jedidsb = {
	"a26_jedidsb", NULL, NULL, NULL, "1983",
	"Star Wars - Return of the Jedi - Death Star Battle\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_jedidsbRomInfo, a26_jedidsbRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E0, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Star Wars - The Arcade Game

static struct BurnRomInfo a26_starwarsRomDesc[] = {
	{ "Star Wars - The Arcade Game (1984)(Parker Brothers).bin", 0x02000, 0x65c31ca4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_starwars)
STD_ROM_FN(a26_starwars)

struct BurnDriver BurnDrva26_starwars = {
	"a26_starwars", NULL, NULL, NULL, "1984",
	"Star Wars - The Arcade Game\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_starwarsRomInfo, a26_starwarsRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E0, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Star Wars - The Empire Strikes Back

static struct BurnRomInfo a26_esbRomDesc[] = {
	{ "Star Wars - The Empire Strikes Back (1982)(Parker Brothers).bin", 0x01000, 0xee8c9a89, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_esb)
STD_ROM_FN(a26_esb)

struct BurnDriver BurnDrva26_esb = {
	"a26_esb", NULL, NULL, NULL, "1982",
	"Star Wars - The Empire Strikes Back\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_esbRomInfo, a26_esbRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Stargate

static struct BurnRomInfo a26_stargateRomDesc[] = {
	{ "Stargate (1984)(Atari).bin", 0x02000, 0xcde3530e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_stargate)
STD_ROM_FN(a26_stargate)

struct BurnDriver BurnDrva26_stargate = {
	"a26_stargate", NULL, NULL, NULL, "1984",
	"Stargate\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_stargateRomInfo, a26_stargateRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Stargunner

static struct BurnRomInfo a26_stargunnRomDesc[] = {
	{ "Stargunner (1982)(Telesys).bin", 0x01000, 0xfca9c17b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_stargunn)
STD_ROM_FN(a26_stargunn)

struct BurnDriver BurnDrva26_stargunn = {
	"a26_stargunn", NULL, NULL, NULL, "1982",
	"Stargunner\0", NULL, "Telesys", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_stargunnRomInfo, a26_stargunnRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// StarMaster

static struct BurnRomInfo a26_starmastRomDesc[] = {
	{ "StarMaster (1982)(Activision).bin", 0x01000, 0x25248b4b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_starmast)
STD_ROM_FN(a26_starmast)

struct BurnDriver BurnDrva26_starmast = {
	"a26_starmast", NULL, NULL, NULL, "1982",
	"StarMaster\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_starmastRomInfo, a26_starmastRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Steeplechase

static struct BurnRomInfo a26_steeplecRomDesc[] = {
	{ "Steeplechase (1980)(Sears).bin", 0x00800, 0x5f6c32c3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_steeplec)
STD_ROM_FN(a26_steeplec)

struct BurnDriver BurnDrva26_steeplec = {
	"a26_steeplec", NULL, NULL, NULL, "1980",
	"Steeplechase\0", NULL, "Sears", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_steeplecRomInfo, a26_steeplecRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_2K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Stellar Track

static struct BurnRomInfo a26_stellartRomDesc[] = {
	{ "Stellar Track (1980)(Sears).bin", 0x01000, 0xa76e93bf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_stellart)
STD_ROM_FN(a26_stellart)

struct BurnDriver BurnDrva26_stellart = {
	"a26_stellart", NULL, NULL, NULL, "1980",
	"Stellar Track\0", NULL, "Sears", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_stellartRomInfo, a26_stellartRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Stone Age

static struct BurnRomInfo a26_stoneageRomDesc[] = {
	{ "Stone Age (1983)(CCE).bin", 0x01000, 0x8bcdb222, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_stoneage)
STD_ROM_FN(a26_stoneage)

struct BurnDriver BurnDrva26_stoneage = {
	"a26_stoneage", NULL, NULL, NULL, "1983",
	"Stone Age\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_stoneageRomInfo, a26_stoneageRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Strategy X

static struct BurnRomInfo a26_strategxRomDesc[] = {
	{ "Strategy X (1983)(Konami).bin", 0x01000, 0x2af91a81, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_strategx)
STD_ROM_FN(a26_strategx)

struct BurnDriver BurnDrva26_strategx = {
	"a26_strategx", NULL, NULL, NULL, "1983",
	"Strategy X\0", NULL, "Konami", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_strategxRomInfo, a26_strategxRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Strawberry Shortcake - Musical Match-Ups

static struct BurnRomInfo a26_strawmmRomDesc[] = {
	{ "Strawberry Shortcake - Musical Match-Ups (1983)(Parker Brothers).bin", 0x01000, 0xfb29e31f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_strawmm)
STD_ROM_FN(a26_strawmm)

struct BurnDriver BurnDrva26_strawmm = {
	"a26_strawmm", NULL, NULL, NULL, "1983",
	"Strawberry Shortcake - Musical Match-Ups\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_strawmmRomInfo, a26_strawmmRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Street Racer - Speedway II

static struct BurnRomInfo a26_sracerRomDesc[] = {
	{ "Street Racer (1977)(Atari).bin", 0x00800, 0x47592880, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sracer)
STD_ROM_FN(a26_sracer)

struct BurnDriver BurnDrva26_sracer = {
	"a26_sracer", NULL, NULL, NULL, "1977",
	"Street Racer - Speedway II\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sracerRomInfo, a26_sracerRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_2K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Stronghold

static struct BurnRomInfo a26_stronghRomDesc[] = {
	{ "Stronghold (1983)(CommaVid).bin", 0x01000, 0xbb91ffa6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_strongh)
STD_ROM_FN(a26_strongh)

struct BurnDriver BurnDrva26_strongh = {
	"a26_strongh", NULL, NULL, NULL, "1983",
	"Stronghold\0", NULL, "CommaVid", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_stronghRomInfo, a26_stronghRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Stunt Man

static struct BurnRomInfo a26_stuntmanRomDesc[] = {
	{ "Stunt Man (1983)(Unknown).bin", 0x01000, 0xea30cdd5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_stuntman)
STD_ROM_FN(a26_stuntman)

struct BurnDriver BurnDrva26_stuntman = {
	"a26_stuntman", NULL, NULL, NULL, "1983",
	"Stunt Man\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_stuntmanRomInfo, a26_stuntmanRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Sub-Scan

static struct BurnRomInfo a26_subscanRomDesc[] = {
	{ "Sub-Scan (1982)(Sega).bin", 0x01000, 0x5069d7da, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_subscan)
STD_ROM_FN(a26_subscan)

struct BurnDriver BurnDrva26_subscan = {
	"a26_subscan", NULL, NULL, NULL, "1982",
	"Sub-Scan\0", NULL, "Sega", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_subscanRomInfo, a26_subscanRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Submarine Commander

static struct BurnRomInfo a26_subcommRomDesc[] = {
	{ "Submarine Commander (1982)(Sears).bin", 0x01000, 0x3262960a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_subcomm)
STD_ROM_FN(a26_subcomm)

struct BurnDriver BurnDrva26_subcomm = {
	"a26_subcomm", NULL, NULL, NULL, "1982",
	"Submarine Commander\0", NULL, "Sears", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_subcommRomInfo, a26_subcommRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Subterranea

static struct BurnRomInfo a26_subterraRomDesc[] = {
	{ "Subterranea (1983)(Imagic).bin", 0x02000, 0x2ab951f7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_subterra)
STD_ROM_FN(a26_subterra)

struct BurnDriver BurnDrva26_subterra = {
	"a26_subterra", NULL, NULL, NULL, "1983",
	"Subterranea\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_subterraRomInfo, a26_subterraRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Summer Games

static struct BurnRomInfo a26_sumgamesRomDesc[] = {
	{ "Summer Games (1987)(Epyx).bin", 0x04000, 0xb9cd3f86, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sumgames)
STD_ROM_FN(a26_sumgames)

struct BurnDriver BurnDrva26_sumgames = {
	"a26_sumgames", NULL, NULL, NULL, "1987",
	"Summer Games\0", NULL, "Epyx", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sumgamesRomInfo, a26_sumgamesRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Super Baseball

static struct BurnRomInfo a26_superbasRomDesc[] = {
	{ "Super Baseball (1988)(Atari).bin", 0x04000, 0x2245f170, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_superbas)
STD_ROM_FN(a26_superbas)

struct BurnDriver BurnDrva26_superbas = {
	"a26_superbas", NULL, NULL, NULL, "1988",
	"Super Baseball\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_superbasRomInfo, a26_superbasRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Super Box (CCE)

static struct BurnRomInfo a26_superboxRomDesc[] = {
	{ "Super Box (19xx)(CCE).bin", 0x04000, 0x0b629d9b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_superbox)
STD_ROM_FN(a26_superbox)

struct BurnDriver BurnDrva26_superbox = {
	"a26_superbox", NULL, NULL, NULL, "19??",
	"Super Box (CCE)\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_superboxRomInfo, a26_superboxRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Super Breakout

static struct BurnRomInfo a26_sbrkoutRomDesc[] = {
	{ "Super Breakout (1981)(Atari).bin", 0x01000, 0xb0f20d31, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sbrkout)
STD_ROM_FN(a26_sbrkout)

struct BurnDriver BurnDrva26_sbrkout = {
	"a26_sbrkout", NULL, NULL, NULL, "1981?",
	"Super Breakout\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sbrkoutRomInfo, a26_sbrkoutRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Super Challenge Football - Football

static struct BurnRomInfo a26_scfootbRomDesc[] = {
	{ "Super Challenge Football (1982)(M Network).bin", 0x01000, 0x55f02efe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_scfootb)
STD_ROM_FN(a26_scfootb)

struct BurnDriver BurnDrva26_scfootb = {
	"a26_scfootb", NULL, NULL, NULL, "1982",
	"Super Challenge Football - Football\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_scfootbRomInfo, a26_scfootbRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Super Cobra

static struct BurnRomInfo a26_scobraRomDesc[] = {
	{ "Super Cobra (1982)(Parker Brothers).bin", 0x02000, 0xde97103d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_scobra)
STD_ROM_FN(a26_scobra)

struct BurnDriver BurnDrva26_scobra = {
	"a26_scobra", NULL, NULL, NULL, "1982",
	"Super Cobra\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_scobraRomInfo, a26_scobraRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E0, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Super-Ferrari (unknown)

static struct BurnRomInfo a26_ferrari1RomDesc[] = {
	{ "Super-Ferrari (19xx)(Unknown).bin", 0x01000, 0x4d07b643, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_ferrari1)
STD_ROM_FN(a26_ferrari1)

struct BurnDriver BurnDrva26_ferrari1 = {
	"a26_ferrari1", NULL, NULL, NULL, "19??",
	"Super-Ferrari (unknown)\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_ferrari1RomInfo, a26_ferrari1RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Super Football

static struct BurnRomInfo a26_supfootbRomDesc[] = {
	{ "Super Football (1988)(Atari).bin", 0x04000, 0xc9b16f3c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_supfootb)
STD_ROM_FN(a26_supfootb)

struct BurnDriver BurnDrva26_supfootb = {
	"a26_supfootb", NULL, NULL, NULL, "1988",
	"Super Football\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_supfootbRomInfo, a26_supfootbRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Superman

static struct BurnRomInfo a26_supermanRomDesc[] = {
	{ "Superman (1979)(Atari).bin", 0x01000, 0x39562bd7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_superman)
STD_ROM_FN(a26_superman)

struct BurnDriver BurnDrva26_superman = {
	"a26_superman", NULL, NULL, NULL, "1979",
	"Superman\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_supermanRomInfo, a26_supermanRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Surround - Chase

static struct BurnRomInfo a26_surroundRomDesc[] = {
	{ "Surround (1977)(Atari).bin", 0x00800, 0xdcb11f23, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_surround)
STD_ROM_FN(a26_surround)

struct BurnDriver BurnDrva26_surround = {
	"a26_surround", NULL, NULL, NULL, "1977",
	"Surround - Chase\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_surroundRomInfo, a26_surroundRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Survival Run

static struct BurnRomInfo a26_survivalRomDesc[] = {
	{ "Survival Run (1983)(Milton Bradley Company).bin", 0x01000, 0x8c0eea30, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_survival)
STD_ROM_FN(a26_survival)

struct BurnDriver BurnDrva26_survival = {
	"a26_survival", NULL, NULL, NULL, "1983",
	"Survival Run\0", NULL, "Milton Bradley Company", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_survivalRomInfo, a26_survivalRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Swordfight

static struct BurnRomInfo a26_swordfgtRomDesc[] = {
	{ "Swordfight (1983)(M Network).bin", 0x01000, 0xf1ad77be, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_swordfgt)
STD_ROM_FN(a26_swordfgt)

struct BurnDriver BurnDrva26_swordfgt = {
	"a26_swordfgt", NULL, NULL, NULL, "1983",
	"Swordfight\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_swordfgtRomInfo, a26_swordfgtRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// SwordQuest - EarthWorld

static struct BurnRomInfo a26_sqearthRomDesc[] = {
	{ "SwordQuest - EarthWorld (1982)(Atari).bin", 0x02000, 0x9031a479, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sqearth)
STD_ROM_FN(a26_sqearth)

struct BurnDriver BurnDrva26_sqearth = {
	"a26_sqearth", NULL, NULL, NULL, "1982",
	"SwordQuest - EarthWorld\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sqearthRomInfo, a26_sqearthRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// SwordQuest - FireWorld

static struct BurnRomInfo a26_sqfireRomDesc[] = {
	{ "SwordQuest - FireWorld (1982)(Atari).bin", 0x02000, 0x6ae46a0c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sqfire)
STD_ROM_FN(a26_sqfire)

struct BurnDriver BurnDrva26_sqfire = {
	"a26_sqfire", NULL, NULL, NULL, "1982",
	"SwordQuest - FireWorld\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sqfireRomInfo, a26_sqfireRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// SwordQuest - WaterWorld

static struct BurnRomInfo a26_sqwaterRomDesc[] = {
	{ "SwordQuest - WaterWorld (1983)(Atari).bin", 0x02000, 0xca7b4685, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_sqwater)
STD_ROM_FN(a26_sqwater)

struct BurnDriver BurnDrva26_sqwater = {
	"a26_sqwater", NULL, NULL, NULL, "1983",
	"SwordQuest - WaterWorld\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_sqwaterRomInfo, a26_sqwaterRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tac-Scan

static struct BurnRomInfo a26_tacscanRomDesc[] = {
	{ "Tac-Scan (1982)(Sega).bin", 0x01000, 0xa2a42bec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tacscan)
STD_ROM_FN(a26_tacscan)

struct BurnDriver BurnDrva26_tacscan = {
	"a26_tacscan", NULL, NULL, NULL, "1982",
	"Tac-Scan\0", NULL, "Sega", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_tacscanRomInfo, a26_tacscanRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tank Brigade

static struct BurnRomInfo a26_tankbrigRomDesc[] = {
	{ "Tank Brigade (1983)(Panda).bin", 0x01000, 0x84e9e475, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tankbrig)
STD_ROM_FN(a26_tankbrig)

struct BurnDriver BurnDrva26_tankbrig = {
	"a26_tankbrig", NULL, NULL, NULL, "1983",
	"Tank Brigade\0", NULL, "Panda", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_tankbrigRomInfo, a26_tankbrigRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tank City

static struct BurnRomInfo a26_tankcityRomDesc[] = {
	{ "Tank City (19xx)(Funvision).bin", 0x01000, 0x524822ef, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tankcity)
STD_ROM_FN(a26_tankcity)

struct BurnDriver BurnDrva26_tankcity = {
	"a26_tankcity", NULL, NULL, NULL, "19??",
	"Tank City\0", NULL, "Funvision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_tankcityRomInfo, a26_tankcityRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tanks But No Tanks

static struct BurnRomInfo a26_tanksnoRomDesc[] = {
	{ "Tanks But No Tanks (1983)(ZiMAG).bin", 0x01000, 0x4adbb1f9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tanksno)
STD_ROM_FN(a26_tanksno)

struct BurnDriver BurnDrva26_tanksno = {
	"a26_tanksno", NULL, NULL, NULL, "1983",
	"Tanks But No Tanks\0", NULL, "ZiMAG", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_tanksnoRomInfo, a26_tanksnoRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tapeworm

static struct BurnRomInfo a26_tapewormRomDesc[] = {
	{ "Tapeworm (1982)(Spectravideo).bin", 0x01000, 0xd63474b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tapeworm)
STD_ROM_FN(a26_tapeworm)

struct BurnDriver BurnDrva26_tapeworm = {
	"a26_tapeworm", NULL, NULL, NULL, "1982",
	"Tapeworm\0", NULL, "Spectravideo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_tapewormRomInfo, a26_tapewormRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tapper

static struct BurnRomInfo a26_tapperRomDesc[] = {
	{ "Tapper (1984)(Sega).bin", 0x02000, 0xd28afb2c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tapper)
STD_ROM_FN(a26_tapper)

struct BurnDriver BurnDrva26_tapper = {
	"a26_tapper", NULL, NULL, NULL, "1984",
	"Tapper\0", NULL, "Sega", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_tapperRomInfo, a26_tapperRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Target Practice

static struct BurnRomInfo a26_targpracRomDesc[] = {
	{ "Target Practice (1983)(CEE).bin", 0x01000, 0x54f602eb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_targprac)
STD_ROM_FN(a26_targprac)

struct BurnDriver BurnDrva26_targprac = {
	"a26_targprac", NULL, NULL, NULL, "1983",
	"Target Practice\0", NULL, "CEE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_targpracRomInfo, a26_targpracRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Task Force

static struct BurnRomInfo a26_taskfrceRomDesc[] = {
	{ "Task Force (1987)(Unknown).bin", 0x01000, 0x779b87e3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_taskfrce)
STD_ROM_FN(a26_taskfrce)

struct BurnDriver BurnDrva26_taskfrce = {
	"a26_taskfrce", NULL, NULL, NULL, "1987",
	"Task Force\0", NULL, "<unknown>", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_taskfrceRomInfo, a26_taskfrceRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tax Avoiders

static struct BurnRomInfo a26_taxavoidRomDesc[] = {
	{ "Tax Avoiders (1982)(American Videogame).bin", 0x02000, 0x468d734c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_taxavoid)
STD_ROM_FN(a26_taxavoid)

struct BurnDriver BurnDrva26_taxavoid = {
	"a26_taxavoid", NULL, NULL, NULL, "1982",
	"Tax Avoiders\0", NULL, "American Videogame", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_taxavoidRomInfo, a26_taxavoidRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Taz (USA)

static struct BurnRomInfo a26_tazRomDesc[] = {
	{ "Taz (1983)(Atari).bin", 0x02000, 0xc9d7ec9b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_taz)
STD_ROM_FN(a26_taz)

struct BurnDriver BurnDrva26_taz = {
	"a26_taz", NULL, NULL, NULL, "1983",
	"Taz (USA)\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_ACTION, 0,
	A2600GetZipName, a26_tazRomInfo, a26_tazRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tennis - Le Tennis

static struct BurnRomInfo a26_tennisRomDesc[] = {
	{ "Tennis (1981)(Activision).bin", 0x00800, 0x0a42515b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tennis)
STD_ROM_FN(a26_tennis)

struct BurnDriver BurnDrva26_tennis = {
	"a26_tennis", NULL, NULL, NULL, "1981",
	"Tennis - Le Tennis\0", NULL, "Activision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_tennisRomInfo, a26_tennisRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_2K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// The Texas Chainsaw Massacre

static struct BurnRomInfo a26_texascmRomDesc[] = {
	{ "The Texas Chainsaw Massacre (1983)(Wizard Video Games).bin", 0x01000, 0xec389caa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_texascm)
STD_ROM_FN(a26_texascm)

struct BurnDriver BurnDrva26_texascm = {
	"a26_texascm", NULL, NULL, NULL, "1983",
	"The Texas Chainsaw Massacre\0", NULL, "Wizard Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_texascmRomInfo, a26_texascmRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Threshold

static struct BurnRomInfo a26_thresholRomDesc[] = {
	{ "Threshold (1982)(Tigervision).bin", 0x01000, 0xce5097fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_threshol)
STD_ROM_FN(a26_threshol)

struct BurnDriver BurnDrva26_threshol = {
	"a26_threshol", NULL, NULL, NULL, "1982",
	"Threshold\0", NULL, "Tigervision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_thresholRomInfo, a26_thresholRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Thunderground

static struct BurnRomInfo a26_thundergRomDesc[] = {
	{ "Thunderground (1983)(Sega).bin", 0x01000, 0x9e67c699, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_thunderg)
STD_ROM_FN(a26_thunderg)

struct BurnDriver BurnDrva26_thunderg = {
	"a26_thunderg", NULL, NULL, NULL, "1983",
	"Thunderground\0", NULL, "Sega", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_thundergRomInfo, a26_thundergRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Time Pilot

static struct BurnRomInfo a26_timepltRomDesc[] = {
	{ "Time Pilot (1983)(Coleco).bin", 0x02000, 0x21ee7db4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_timeplt)
STD_ROM_FN(a26_timeplt)

struct BurnDriver BurnDrva26_timeplt = {
	"a26_timeplt", NULL, NULL, NULL, "1983",
	"Time Pilot\0", NULL, "Coleco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_timepltRomInfo, a26_timepltRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Time Warp

static struct BurnRomInfo a26_timewarpRomDesc[] = {
	{ "Time Warp (1982)(Funvision).bin", 0x01000, 0xe5bc054d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_timewarp)
STD_ROM_FN(a26_timewarp)

struct BurnDriver BurnDrva26_timewarp = {
	"a26_timewarp", NULL, NULL, NULL, "1982",
	"Time Warp\0", NULL, "Funvision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_timewarpRomInfo, a26_timewarpRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Title Match Pro Wrestling

static struct BurnRomInfo a26_prowresRomDesc[] = {
	{ "Title Match Pro Wrestling (1987)(Absolute Entertainment).bin", 0x02000, 0xef708c03, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_prowres)
STD_ROM_FN(a26_prowres)

struct BurnDriver BurnDrva26_prowres = {
	"a26_prowres", NULL, NULL, NULL, "1987",
	"Title Match Pro Wrestling\0", NULL, "Absolute Entertainment", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_prowresRomInfo, a26_prowresRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tomarc the Barbarian

static struct BurnRomInfo a26_tomarcRomDesc[] = {
	{ "Tomarc the Barbarian (1983)(Xonox).bin", 0x02000, 0xb5b5ac84, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tomarc)
STD_ROM_FN(a26_tomarc)

struct BurnDriver BurnDrva26_tomarc = {
	"a26_tomarc", NULL, NULL, NULL, "1983",
	"Tomarc the Barbarian\0", NULL, "Xonox", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_tomarcRomInfo, a26_tomarcRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tomcat - The F-14 Fighter Simulator

static struct BurnRomInfo a26_tomcatRomDesc[] = {
	{ "Tomcat - The F-14 Fighter Simulator (1988)(Absolute Entertainment).bin", 0x04000, 0x8987c473, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tomcat)
STD_ROM_FN(a26_tomcat)

struct BurnDriver BurnDrva26_tomcat = {
	"a26_tomcat", NULL, NULL, NULL, "1988",
	"Tomcat - The F-14 Fighter Simulator\0", NULL, "Absolute Entertainment", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_tomcatRomInfo, a26_tomcatRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tooth Protectors

static struct BurnRomInfo a26_toothproRomDesc[] = {
	{ "Tooth Protectors (1983)(DSD-Camelot).bin", 0x02000, 0xfd8c81e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_toothpro)
STD_ROM_FN(a26_toothpro)

struct BurnDriver BurnDrva26_toothpro = {
	"a26_toothpro", NULL, NULL, NULL, "1983",
	"Tooth Protectors\0", NULL, "DSD-Camelot", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_toothproRomInfo, a26_toothproRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E0, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Towering Inferno

static struct BurnRomInfo a26_towinferRomDesc[] = {
	{ "Towering Inferno (1982)(U.S. Games Corporation).bin", 0x01000, 0x2c7b86b8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_towinfer)
STD_ROM_FN(a26_towinfer)

struct BurnDriver BurnDrva26_towinfer = {
	"a26_towinfer", NULL, NULL, NULL, "1982",
	"Towering Inferno\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_towinferRomInfo, a26_towinferRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Track and Field

static struct BurnRomInfo a26_trackfldRomDesc[] = {
	{ "Track and Field (1984)(Atari).bin", 0x04000, 0x21827056, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_trackfld)
STD_ROM_FN(a26_trackfld)

struct BurnDriver BurnDrva26_trackfld = {
	"a26_trackfld", NULL, NULL, NULL, "1984",
	"Track and Field\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_trackfldRomInfo, a26_trackfldRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Treasure Hunting

static struct BurnRomInfo a26_thuntingRomDesc[] = {
	{ "Treasure Hunting (19xx)(Funvision).bin", 0x01000, 0xb1600d79, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_thunting)
STD_ROM_FN(a26_thunting)

struct BurnDriver BurnDrva26_thunting = {
	"a26_thunting", NULL, NULL, NULL, "19??",
	"Treasure Hunting\0", NULL, "Funvision", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_thuntingRomInfo, a26_thuntingRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Trick Shot

static struct BurnRomInfo a26_trickshtRomDesc[] = {
	{ "Trick Shot (1982)(Imagic).bin", 0x01000, 0xd1ce4634, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tricksht)
STD_ROM_FN(a26_tricksht)

struct BurnDriver BurnDrva26_tricksht = {
	"a26_tricksht", NULL, NULL, NULL, "1982",
	"Trick Shot\0", NULL, "Imagic", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_trickshtRomInfo, a26_trickshtRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// TRON - Deadly Discs

static struct BurnRomInfo a26_tronddRomDesc[] = {
	{ "TRON - Deadly Discs (1982)(M Network).bin", 0x01000, 0xd35b22f9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_trondd)
STD_ROM_FN(a26_trondd)

struct BurnDriver BurnDrva26_trondd = {
	"a26_trondd", NULL, NULL, NULL, "1982",
	"TRON - Deadly Discs\0", NULL, "M Network", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_tronddRomInfo, a26_tronddRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tunnel Runner

static struct BurnRomInfo a26_tunnelrnRomDesc[] = {
	{ "Tunnel Runner (1983)(CBS Electronics).bin", 0x03000, 0xa02745f8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tunnelrn)
STD_ROM_FN(a26_tunnelrn)

struct BurnDriver BurnDrva26_tunnelrn = {
	"a26_tunnelrn", NULL, NULL, NULL, "1983",
	"Tunnel Runner\0", NULL, "CBS Electronics", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_tunnelrnRomInfo, a26_tunnelrnRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_FA, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Turmoil

static struct BurnRomInfo a26_turmoilRomDesc[] = {
	{ "Turmoil (1982)(20th Century Fox Video Games).bin", 0x01000, 0x8c65298a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_turmoil)
STD_ROM_FN(a26_turmoil)

struct BurnDriver BurnDrva26_turmoil = {
	"a26_turmoil", NULL, NULL, NULL, "1982",
	"Turmoil\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_turmoilRomInfo, a26_turmoilRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tutankham

static struct BurnRomInfo a26_tutankhmRomDesc[] = {
	{ "Tutankham (1983)(Parker Brothers).bin", 0x02000, 0xec959bf2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tutankhm)
STD_ROM_FN(a26_tutankhm)

struct BurnDriver BurnDrva26_tutankhm = {
	"a26_tutankhm", NULL, NULL, NULL, "1983",
	"Tutankham\0", NULL, "Parker Brothers", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_tutankhmRomInfo, a26_tutankhmRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_E0, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Universal Chaos

static struct BurnRomInfo a26_unichaosRomDesc[] = {
	{ "Universal Chaos (1989)(Telegames).bin", 0x01000, 0x25efa169, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_unichaos)
STD_ROM_FN(a26_unichaos)

struct BurnDriver BurnDrva26_unichaos = {
	"a26_unichaos", NULL, NULL, NULL, "1989",
	"Universal Chaos\0", NULL, "Telegames", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_unichaosRomInfo, a26_unichaosRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Up 'n Down

static struct BurnRomInfo a26_upndownRomDesc[] = {
	{ "Up 'n Down (1984)(Sega).bin", 0x02000, 0xc04c2b58, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_upndown)
STD_ROM_FN(a26_upndown)

struct BurnDriver BurnDrva26_upndown = {
	"a26_upndown", NULL, NULL, NULL, "1984",
	"Up 'n Down\0", NULL, "Sega", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_upndownRomInfo, a26_upndownRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Vanguard

static struct BurnRomInfo a26_vanguardRomDesc[] = {
	{ "Vanguard (1982)(Atari).bin", 0x02000, 0xc4bec521, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_vanguard)
STD_ROM_FN(a26_vanguard)

struct BurnDriver BurnDrva26_vanguard = {
	"a26_vanguard", NULL, NULL, NULL, "1982",
	"Vanguard\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_vanguardRomInfo, a26_vanguardRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Venture

static struct BurnRomInfo a26_ventureRomDesc[] = {
	{ "Venture (1982)(Coleco).bin", 0x01000, 0x85e0ca62, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_venture)
STD_ROM_FN(a26_venture)

struct BurnDriver BurnDrva26_venture = {
	"a26_venture", NULL, NULL, NULL, "1982",
	"Venture\0", NULL, "Coleco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_ventureRomInfo, a26_ventureRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Video Checkers - Checkers - Atari Video Checkers

static struct BurnRomInfo a26_videochkRomDesc[] = {
	{ "Video Checkers (1980)(Atari).bin", 0x01000, 0x3df33335, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_videochk)
STD_ROM_FN(a26_videochk)

struct BurnDriver BurnDrva26_videochk = {
	"a26_videochk", NULL, NULL, NULL, "1980",
	"Video Checkers - Checkers - Atari Video Checkers\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_videochkRomInfo, a26_videochkRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Video Chess

static struct BurnRomInfo a26_videochsRomDesc[] = {
	{ "Video Chess (1979)(Atari).bin", 0x01000, 0xb6226a54, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_videochs)
STD_ROM_FN(a26_videochs)

struct BurnDriver BurnDrva26_videochs = {
	"a26_videochs", NULL, NULL, NULL, "1979",
	"Video Chess\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_videochsRomInfo, a26_videochsRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Video Cube (CCE)

static struct BurnRomInfo a26_videocubRomDesc[] = {
	{ "Video Cube (19xx)(CCE).bin", 0x01000, 0x1bc8b7af, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_videocub)
STD_ROM_FN(a26_videocub)

struct BurnDriver BurnDrva26_videocub = {
	"a26_videocub", NULL, NULL, NULL, "19??",
	"Video Cube (CCE)\0", NULL, "CCE", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_videocubRomInfo, a26_videocubRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Video Jogger

static struct BurnRomInfo a26_videojogRomDesc[] = {
	{ "Video Jogger (1983)(Exus Corporation).bin", 0x01000, 0x28fa8d77, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_videojog)
STD_ROM_FN(a26_videojog)

struct BurnDriver BurnDrva26_videojog = {
	"a26_videojog", NULL, NULL, NULL, "1983",
	"Video Jogger\0", NULL, "Exus Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_videojogRomInfo, a26_videojogRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Video Olympics - Pong Sports

static struct BurnRomInfo a26_videolymRomDesc[] = {
	{ "Video Olympics (1977)(Atari).bin", 0x00800, 0xe4bc89c4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_videolym)
STD_ROM_FN(a26_videolym)

struct BurnDriver BurnDrva26_videolym = {
	"a26_videolym", NULL, NULL, NULL, "1977",
	"Video Olympics - Pong Sports\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_videolymRomInfo, a26_videolymRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_2K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Video Pinball - Arcade Pinball

static struct BurnRomInfo a26_videopinRomDesc[] = {
	{ "Video Pinball (1981)(Atari).bin", 0x01000, 0x10d95426, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_videopin)
STD_ROM_FN(a26_videopin)

struct BurnDriver BurnDrva26_videopin = {
	"a26_videopin", NULL, NULL, NULL, "1981",
	"Video Pinball - Arcade Pinball\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_videopinRomInfo, a26_videopinRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Video Reflex

static struct BurnRomInfo a26_videorefRomDesc[] = {
	{ "Video Reflex (1983)(Exus Corporation).bin", 0x01000, 0x77b8b0d1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_videoref)
STD_ROM_FN(a26_videoref)

struct BurnDriver BurnDrva26_videoref = {
	"a26_videoref", NULL, NULL, NULL, "1983",
	"Video Reflex\0", NULL, "Exus Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_videorefRomInfo, a26_videorefRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Wabbit

static struct BurnRomInfo a26_wabbitRomDesc[] = {
	{ "Wabbit (1982)(Apollo).bin", 0x01000, 0x2d07ed77, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_wabbit)
STD_ROM_FN(a26_wabbit)

struct BurnDriver BurnDrva26_wabbit = {
	"a26_wabbit", NULL, NULL, NULL, "1982",
	"Wabbit\0", NULL, "Apollo", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_wabbitRomInfo, a26_wabbitRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Wall Ball

static struct BurnRomInfo a26_wallballRomDesc[] = {
	{ "Wall Ball (1983)(Avalon Hill).bin", 0x01000, 0x9ac6a295, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_wallball)
STD_ROM_FN(a26_wallball)

struct BurnDriver BurnDrva26_wallball = {
	"a26_wallball", NULL, NULL, NULL, "1983",
	"Wall Ball\0", NULL, "Avalon Hill", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_wallballRomInfo, a26_wallballRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Wall-Defender

static struct BurnRomInfo a26_walldefRomDesc[] = {
	{ "Wall-Defender (1983)(Bomb).bin", 0x01000, 0x6841d2a7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_walldef)
STD_ROM_FN(a26_walldef)

struct BurnDriver BurnDrva26_walldef = {
	"a26_walldef", NULL, NULL, NULL, "1983",
	"Wall-Defender\0", NULL, "Bomb", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_walldefRomInfo, a26_walldefRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Warlords

static struct BurnRomInfo a26_warlordsRomDesc[] = {
	{ "Warlords (1981)(Atari).bin", 0x01000, 0xcf174b57, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_warlords)
STD_ROM_FN(a26_warlords)

struct BurnDriver BurnDrva26_warlords = {
	"a26_warlords", NULL, NULL, NULL, "1981",
	"Warlords\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_warlordsRomInfo, a26_warlordsRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Warplock

static struct BurnRomInfo a26_warplockRomDesc[] = {
	{ "Warplock (1982)(Data Age).bin", 0x01000, 0x6531d4e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_warplock)
STD_ROM_FN(a26_warplock)

struct BurnDriver BurnDrva26_warplock = {
	"a26_warplock", NULL, NULL, NULL, "1982",
	"Warplock\0", NULL, "Data Age", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_warplockRomInfo, a26_warplockRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_4K_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Winter Games (USA)

static struct BurnRomInfo a26_wingamesRomDesc[] = {
	{ "Winter Games (1987)(Epyx).bin", 0x04000, 0xddff6850, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_wingames)
STD_ROM_FN(a26_wingames)

struct BurnDriver BurnDrva26_wingames = {
	"a26_wingames", NULL, NULL, NULL, "1987",
	"Winter Games (USA)\0", NULL, "Epyx", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 8, HARDWARE_ATARI2600, GBF_SPORTSMISC, 0,
	A2600GetZipName, a26_wingamesRomInfo, a26_wingamesRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Wizard of Wor (USA)

static struct BurnRomInfo a26_wowRomDesc[] = {
	{ "Wizard of Wor (1982)(CBS Electronics).bin", 0x01000, 0x926f6f11, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_wow)
STD_ROM_FN(a26_wow)

struct BurnDriver BurnDrva26_wow = {
	"a26_wow", NULL, NULL, NULL, "1982",
	"Wizard of Wor (USA)\0", NULL, "CBS Electronics", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_ACTION | GBF_MAZE, 0,
	A2600GetZipName, a26_wowRomInfo, a26_wowRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Word Zapper (USA)

static struct BurnRomInfo a26_wordzapRomDesc[] = {
	{ "Word Zapper (1982)(U.S. Games Corporation).bin", 0x01000, 0xa11eeeab, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_wordzap)
STD_ROM_FN(a26_wordzap)

struct BurnDriver BurnDrva26_wordzap = {
	"a26_wordzap", NULL, NULL, NULL, "1982",
	"Word Zapper (USA)\0", NULL, "U.S. Games Corporation", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_ATARI2600, GBF_MULTISHOOT, 0,
	A2600GetZipName, a26_wordzapRomInfo, a26_wordzapRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Worm War I

static struct BurnRomInfo a26_wormwarRomDesc[] = {
	{ "Worm War I (1982)(20th Century Fox Video Games).bin", 0x01000, 0x97b83a54, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_wormwar)
STD_ROM_FN(a26_wormwar)

struct BurnDriver BurnDrva26_wormwar = {
	"a26_wormwar", NULL, NULL, NULL, "1982",
	"Worm War I\0", NULL, "20th Century Fox Video Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_wormwarRomInfo, a26_wormwarRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Xenophobe

static struct BurnRomInfo a26_xenophobRomDesc[] = {
	{ "Xenophobe (1990)(Atari).bin", 0x04000, 0xf875c406, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_xenophob)
STD_ROM_FN(a26_xenophob)

struct BurnDriver BurnDrva26_xenophob = {
	"a26_xenophob", NULL, NULL, NULL, "1990",
	"Xenophobe\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_xenophobRomInfo, a26_xenophobRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Yars' Revenge

static struct BurnRomInfo a26_yarsrevRomDesc[] = {
	{ "Yars' Revenge (1982)(Atari).bin", 0x01000, 0xdfa1c825, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_yarsrev)
STD_ROM_FN(a26_yarsrev)

struct BurnDriver BurnDrva26_yarsrev = {
	"a26_yarsrev", NULL, NULL, NULL, "1982",
	"Yars' Revenge\0", NULL, "Atari", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_yarsrevRomInfo, a26_yarsrevRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Z-Tack

static struct BurnRomInfo a26_ztackRomDesc[] = {
	{ "Z-Tack (1983)(Bomb).bin", 0x01000, 0x42a76f54, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_ztack)
STD_ROM_FN(a26_ztack)

struct BurnDriver BurnDrva26_ztack = {
	"a26_ztack", NULL, NULL, NULL, "1983",
	"Z-Tack\0", NULL, "Bomb", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_ztackRomInfo, a26_ztackRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_4K, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Zaxxon

static struct BurnRomInfo a26_zaxxonRomDesc[] = {
	{ "Zaxxon (1982)(Coleco).bin", 0x02000, 0x265aa87f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_zaxxon)
STD_ROM_FN(a26_zaxxon)

struct BurnDriver BurnDrva26_zaxxon = {
	"a26_zaxxon", NULL, NULL, NULL, "1982",
	"Zaxxon\0", NULL, "Coleco", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_ATARI2600, GBF_MISC, 0,
	A2600GetZipName, a26_zaxxonRomInfo, a26_zaxxonRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};


// -------------------------------
// Aftermarket/Hack/Homebrew Games
// -------------------------------


// Aardvark (HB)

static struct BurnRomInfo a26_aardvarkRomDesc[] = {
	{ "Aardvark (2019)(Oscar Toledo G.).bin", 32768, 0x3a6ca45b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_aardvark)
STD_ROM_FN(a26_aardvark)

struct BurnDriver BurnDrva26_aardvark = {
	"a26_aardvark", NULL, NULL, NULL, "2019",
	"Aardvark (HB)\0", "32K + Superchip RAM (128 bytes)", "Oscar Toledo G.", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_ATARI2600, GBF_ACTION | GBF_MAZE, 0,
	A2600GetZipName, a26_aardvarkRomInfo, a26_aardvarkRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F4_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Conquest of Mars (HB)

static struct BurnRomInfo a26_conqmarsRomDesc[] = {
	{ "Conquest of Mars (2006)(Champ Games).bin", 16384, 0xbace5a14, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_conqmars)
STD_ROM_FN(a26_conqmars)

struct BurnDriver BurnDrva26_conqmars = {
	"a26_conqmars", NULL, NULL, NULL, "2006",
	"Conquest of Mars (HB)\0", NULL, "Champ Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_ATARI2600, GBF_VERSHOOT, 0,
	A2600GetZipName, a26_conqmarsRomInfo, a26_conqmarsRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Donkey Kong VCS (HB)

static struct BurnRomInfo a26_dkongvcsRomDesc[] = {
	{ "Donkey Kong VCS (2017)(Andreas Dietrich).bin", 32768, 0x6c87f24e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_dkongvcs)
STD_ROM_FN(a26_dkongvcs)

struct BurnDriver BurnDrva26_dkongvcs = {
	"a26_dkongvcs", NULL, NULL, NULL, "2017",
	"Donkey Kong VCS (HB)\0", "32K + Superchip RAM (128 bytes)", "Andreas Dietrich", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_dkongvcsRomInfo, a26_dkongvcsRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F4_SC, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Draconian (HB)

static struct BurnRomInfo a26_draconianRomDesc[] = {
	{ "Draconian (2017)(SpiceWare).bin", 32768, 0xd104b3d9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_draconian)
STD_ROM_FN(a26_draconian)

struct BurnDriver BurnDrva26_draconian = {
	"a26_draconian", NULL, NULL, NULL, "2017",
	"Draconian (HB)\0", "CDF coprocessor", "SpiceWare", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_ATARI2600, GBF_MULTISHOOT, 0,
	A2600GetZipName, a26_draconianRomInfo, a26_draconianRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_CDF, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Drone Attack (HB)

static struct BurnRomInfo a26_droneRomDesc[] = {
	{ "Drone Attack (2025)(Artefacto Games).bin", 8192, 0x1790e5d6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_drone)
STD_ROM_FN(a26_drone)

struct BurnDriver BurnDrva26_drone = {
	"a26_drone", "a26_dronedx", NULL, NULL, "2025",
	"Drone Attack (HB)\0", NULL, "Artefacto Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HOMEBREW, 1, HARDWARE_ATARI2600, GBF_HORSHOOT, 0,
	A2600GetZipName, a26_droneRomInfo, a26_droneRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Drone Attack DX (HB)

static struct BurnRomInfo a26_dronedxRomDesc[] = {
	{ "Drone Attack DX (2025)(Artefacto Games).bin", 16384, 0x32ba4d8c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_dronedx)
STD_ROM_FN(a26_dronedx)

struct BurnDriver BurnDrva26_dronedx = {
	"a26_dronedx", NULL, NULL, NULL, "2025",
	"Drone Attack DX (HB)\0", NULL, "Artefacto Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_ATARI2600, GBF_HORSHOOT, 0,
	A2600GetZipName, a26_dronedxRomInfo, a26_dronedxRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Galagon (HB)

static struct BurnRomInfo a26_galagonRomDesc[] = {
	{ "Galagon (2019)(Champ Games).bin", 32768, 0x31d4975f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_galagon)
STD_ROM_FN(a26_galagon)

struct BurnDriver BurnDrva26_galagon = {
	"a26_galagon", NULL, NULL, NULL, "2019",
	"Galagon (HB)\0", "CDF coprocessor", "Champ Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_ATARI2600, GBF_VERSHOOT, 0,
	A2600GetZipName, a26_galagonRomInfo, a26_galagonRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_CDF, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// I Ran (HB)

static struct BurnRomInfo a26_iranRomDesc[] = {
	{ "I Ran (2020)(VHZC).bin", 16384, 0xbcdc3f73, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_iran)
STD_ROM_FN(a26_iran)

struct BurnDriver BurnDrva26_iran = {
	"a26_iran", NULL, NULL, NULL, "2020",
	"I Ran (HB)\0", NULL, "VHZC Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_ATARI2600, GBF_ACTION, 0,
	A2600GetZipName, a26_iranRomInfo, a26_iranRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Juno First (HB)

static struct BurnRomInfo a26_junofirstRomDesc[] = {
	{ "Juno First (2009)(Chris Walton).bin", 32768, 0x30baa209, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_junofirst)
STD_ROM_FN(a26_junofirst)

struct BurnDriver BurnDrva26_junofirst = {
	"a26_junofirst", NULL, NULL, NULL, "2009",
	"Juno First (HB)\0", NULL, "Chris Walton", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_ATARI2600, GBF_VERSHOOT, 0,
	A2600GetZipName, a26_junofirstRomInfo, a26_junofirstRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F4, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Jutland Skies (HB)

static struct BurnRomInfo a26_jutskiesRomDesc[] = {
	{ "Jutland Skies (2023)(KiKi).bin", 32768, 0xb4f01093, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_jutskies)
STD_ROM_FN(a26_jutskies)

struct BurnDriver BurnDrva26_jutskies = {
	"a26_jutskies", NULL, NULL, NULL, "2023",
	"Jutland Skies (HB)\0", "Enhanced DPC coprocessor", "KiKi", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_ATARI2600, GBF_SHOOT, 0,
	A2600GetZipName, a26_jutskiesRomInfo, a26_jutskiesRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_DPCPLUS, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Knight Guy (HB)

static struct BurnRomInfo a26_knightguyRomDesc[] = {
	{ "Knight Guy (2018)(VHZC Games).bin", 16384, 0x2f04be66, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_knightguy)
STD_ROM_FN(a26_knightguy)

struct BurnDriver BurnDrva26_knightguy = {
	"a26_knightguy", NULL, NULL, NULL, "2018",
	"Knight Guy (HB)\0", NULL, "VHZC Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_knightguyRomInfo, a26_knightguyRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F6, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Knight Guy: Castle Days (HB)

static struct BurnRomInfo a26_knightguy2RomDesc[] = {
	{ "Knight Guy - Castle Days (2021)(VHZC Games).bin", 32768, 0x87333357, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_knightguy2)
STD_ROM_FN(a26_knightguy2)

struct BurnDriver BurnDrva26_knightguy2 = {
	"a26_knightguy2", NULL, NULL, NULL, "2021",
	"Knight Guy: Castle Days (HB)\0", NULL, "VHZC Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_knightguy2RomInfo, a26_knightguy2RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F4, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Lady Bug Arcade (HB)

static struct BurnRomInfo a26_ladybugarcRomDesc[] = {
	{ "Lady Bug Arcade (2021)(Champ Games).bin", 32768, 0x59a3ed32, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_ladybugarc)
STD_ROM_FN(a26_ladybugarc)

struct BurnDriver BurnDrva26_ladybugarc = {
	"a26_ladybugarc", NULL, NULL, NULL, "2021",
	"Lady Bug Arcade (HB)\0", "CDF coprocessor", "Champ Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_ATARI2600, GBF_ACTION | GBF_MAZE, 0,
	A2600GetZipName, a26_ladybugarcRomInfo, a26_ladybugarcRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_CDF, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Medieval Mayhem (HB)

static struct BurnRomInfo a26_medmayhemRomDesc[] = {
	{ "Medieval Mayhem (2006)(SpiceWare).bin", 32768, 0xb5344ce4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_medmayhem)
STD_ROM_FN(a26_medmayhem)

struct BurnDriver BurnDrva26_medmayhem = {
	"a26_medmayhem", NULL, NULL, NULL, "2006",
	"Medieval Mayhem (HB)\0", NULL, "SpiceWare", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 4, HARDWARE_ATARI2600, GBF_BALLPADDLE, 0,
	A2600GetZipName, a26_medmayhemRomInfo, a26_medmayhemRomName, NULL, NULL, NULL, NULL, A2600PaddleInputInfo, A2600PaddleDIPInfo,
	A26Init_F4_PAD, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Oozy the Goo: Slime Quest (HB)

static struct BurnRomInfo a26_oozysqRomDesc[] = {
	{ "Oozy the Goo - Slime Quest (2022)(VHZC Games).bin", 32768, 0xdd56e194, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_oozysq)
STD_ROM_FN(a26_oozysq)

struct BurnDriver BurnDrva26_oozysq = {
	"a26_oozysq", NULL, NULL, NULL, "2022",
	"Oozy the Goo: Slime Quest (HB)\0", NULL, "VHZC Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_ATARI2600, GBF_ADV | GBF_MAZE, 0,
	A2600GetZipName, a26_oozysqRomInfo, a26_oozysqRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F4, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// RobotWar 2684 (HB)

static struct BurnRomInfo a26_rw2684RomDesc[] = {
	{ "RobotWar 2684 (2021)(Champ Games).bin", 32768, 0x039bf799, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_rw2684)
STD_ROM_FN(a26_rw2684)

struct BurnDriver BurnDrva26_rw2684 = {
	"a26_rw2684", NULL, NULL, NULL, "2021",
	"RobotWar 2684 (HB)\0", "CDF coprocessor", "Champ Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_ATARI2600, GBF_MULTISHOOT, 0,
	A2600GetZipName, a26_rw2684RomInfo, a26_rw2684RomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_CDF, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tony Montezuma's Gold (HB)

static struct BurnRomInfo a26_tonymgRomDesc[] = {
	{ "Tony Montezuma's Gold (2025)(Monochrome Productions).bin", 32768, 0xbb2dc54c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_tonymg)
STD_ROM_FN(a26_tonymg)

struct BurnDriver BurnDrva26_tonymg = {
	"a26_tonymg", NULL, NULL, NULL, "2025",
	"Tony Montezuma's Gold (HB)\0", "CDF coprocessor", "Monochrome Productions", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_ATARI2600, GBF_PLATFORM, 0,
	A2600GetZipName, a26_tonymgRomInfo, a26_tonymgRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_CDF, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Tower of Rubble (HB, v1.1)

static struct BurnRomInfo a26_towrubbleRomDesc[] = {
	{ "Tower of Rubble v1.1 (2018)(DionoiD).bin", 8192, 0x45082a89, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_towrubble)
STD_ROM_FN(a26_towrubble)

struct BurnDriver BurnDrva26_towrubble = {
	"a26_towrubble", NULL, NULL, NULL, "2018",
	"Tower of Rubble (HB, v1.1)\0", NULL, "DionoiD", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_ATARI2600, GBF_ACTION, 0,
	A2600GetZipName, a26_towrubbleRomInfo, a26_towrubbleRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_F8, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Wizard of Wor Arcade (HB)

static struct BurnRomInfo a26_wowarcRomDesc[] = {
	{ "Wizard of Wor Arcade (2019)(Champ Games).bin", 32768, 0x11bc3a97, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_wowarc)
STD_ROM_FN(a26_wowarc)

struct BurnDriver BurnDrva26_wowarc = {
	"a26_wowarc", NULL, NULL, NULL, "2019",
	"Wizard of Wor Arcade (HB)\0", "CDF coprocessor", "Champ Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_ATARI2600, GBF_ACTION | GBF_MAZE, 0,
	A2600GetZipName, a26_wowarcRomInfo, a26_wowarcRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_CDF, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

// Zoo Keeper (HB)

static struct BurnRomInfo a26_zookeepRomDesc[] = {
	{ "Zoo Keeper (2020)(Champ Games).bin", 32768, 0x8ce0b8f4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(a26_zookeep)
STD_ROM_FN(a26_zookeep)

struct BurnDriver BurnDrva26_zookeep = {
	"a26_zookeep", NULL, NULL, NULL, "2020",
	"Zoo Keeper (HB)\0", "CDF coprocessor", "Champ Games", "Atari 2600",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_ATARI2600, GBF_ACTION, 0,
	A2600GetZipName, a26_zookeepRomInfo, a26_zookeepRomName, NULL, NULL, NULL, NULL, A2600InputInfo, A2600DIPInfo,
	A26Init_CDF, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x80,
	A26_W * A26_SCALE, A26_NTSC_H * A26_SCALE, 4, 3
};

