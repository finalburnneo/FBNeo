/*
Paprium cartridge, low level. The cart's own DATENMEISTER chip is not emulated: the Little-Man
firmware of the Paprium FPGA cores, a rewrite of what that chip does (user supplied in the
romset), runs on an emulated NEORV32, the soft core it is written for ("the mcu" below), and
the cartridge FPGA around it is modelled after the documented MCU interface v1.3.
*/

#include "rv32.h"

#define LM_IROM_SIZE	0x16000
#define LM_WRAM_SIZE	0xc000
#define LM_WS_SIZE		0x200000
#define LM_CRAM_SIZE	0x400000
#define LM_XRAM_SIZE	0x40000
#define LM_BAT_SIZE		0x2000
#define LM_BAT_OFFSET	0x8000		// inside the driver's NV RAM (kept where the earlier builds put it)

#define LM_CLK_HZ		53693175
#define LM_USER_CODE	0x50490103
#define LM_FEAT			0x00580047
#define LM_SYN_ID		0x53350001

#define LM_CAUSE_B		0x01
#define LM_CAUSE_A		0x02
#define LM_CAUSE_S		0x04
#define LM_CAUSE_V		0x08
#define LM_CAUSE_BLK	0x10

static UINT8 *lm_irom = NULL;
static UINT8 *lm_wram = NULL;
static UINT8 *lm_dp = NULL;		// RAMDP: lm_dp[a] = 68000 byte a ^ 1
static UINT8 *lm_ws = NULL;		// SDRAM workspace, same byte order
static UINT8 *lm_cram = NULL;
static UINT8 *lm_xram = NULL;
static UINT8 *lm_fw = NULL;		// the firmware file, mcu byte order
static UINT8 *lm_bat = NULL;

static rv32_state lm_cpu;
static UINT32 lm_idle_pc = 0xffffffff;	// the firmware's thread loop (l3_thread), when recognised
static INT32 lm_idle_backoff = 0, lm_idle_wait = 0;
static UINT64 lm_idle_cost = 0;		// the last pass's cost (a hint)
// thread loop passes that start up to this time (mcu cycles) are like the last probed one, until
// something outside the mcu changes the cart (0: not known)
static UINT64 lm_idle_horizon = 0;
static void lm_idle_forget() { lm_idle_horizon = 0; }
static void lm_idle_skip(UINT64 limit);

struct lm_regs {
	// register file
	UINT16 r_e4, r_e6, r_ea, r_ec;
	UINT8 guard, stream_valid, window_en, mini_view;
	UINT32 remaining;
	UINT32 sptr;

	// fpgaio
	UINT8 ctrl, cause, msip, pad0;
	UINT32 stamp_a, stamp_b, wlock, vscnt;

	// sprite decoder
	UINT32 dec_base, dec_hi, dec_depth, dec_words, dec_stat;
	UINT32 dec_q[16];

	// xlink
	UINT32 xcfg, cache, cache_loaded, bar;

	UINT32 gpio_out;

	// timing
	INT64 offset;		// mcu cycles = 68000 cycles * 7 + offset
	UINT8 held, release_pending, boot_pending, pad3;
};

static lm_regs lm;

static void lm_update_irq()
{
	UINT32 ext = (lm.cause & 0x0f) | ((lm.cause & LM_CAUSE_BLK) && (lm.ctrl & 0x40));
	lm_cpu.irq_lines = (ext ? RV32_IRQ_MEI : 0) | (lm.msip ? RV32_IRQ_MSI : 0);
}

static inline UINT64 lm_68k_to_mcu(UINT64 c68k)
{
	return (UINT64)((INT64)(c68k * 7) + lm.offset);
}

/* ---------------------------------------------------------------------------------------
   the big memories in the save states. The regions the firmware works in are always saved; the
   rest (the sound banks, the curve, code) stays as the power-on boot left it, the same for any
   nv ram. A page outside the work regions written since the boot is saved too, so the states
   stay right, but they keep the same size only as long as that does not happen (run-ahead,
   rewind and netplay need a fixed size)
   --------------------------------------------------------------------------------------- */

#define LM_PAGE_SHIFT	12
#define LM_PAGE_SIZE	(1 << LM_PAGE_SHIFT)

enum { LM_TR_WS = 0, LM_TR_CRAM, LM_TR_XRAM, LM_TR_FLASH, LM_TR_N };

struct lm_track_t {
	UINT8 *mem;
	UINT32 size;
	UINT8 *dirty;	// a byte per page
	UINT8 *fixed;	// a byte per page: in a work region, always saved
	UINT8 **base;	// the boot's content of each page written since, kept at its first write
};

static lm_track_t lm_track[LM_TR_N];
static INT32 lm_tracking = 0;	// on once the power-on boot is done
static INT32 lm_track_warned = 0;
static UINT32 lm_base_hash = 0;

static void lm_page_dirty(lm_track_t *t, UINT32 pg)
{
	if (t->base[pg] == NULL) {
		t->base[pg] = (UINT8*)BurnMalloc(LM_PAGE_SIZE);
		memcpy(t->base[pg], t->mem + (pg << LM_PAGE_SHIFT), LM_PAGE_SIZE);
	}
	t->dirty[pg] = 1;
}

static inline void lm_touch(INT32 r, UINT32 offs)
{
	if (lm_tracking) {
		lm_track_t *t = &lm_track[r];
		UINT32 pg = offs >> LM_PAGE_SHIFT;
		if (!t->dirty[pg]) lm_page_dirty(t, pg);	// (the work regions' pages are always dirty)
	}
}

static void lm_track_init(INT32 r, UINT8 *mem, UINT32 size)
{
	lm_track_t *t = &lm_track[r];
	UINT32 n = size >> LM_PAGE_SHIFT;
	t->mem = mem;
	t->size = size;
	t->dirty = (UINT8*)BurnMalloc(n);
	t->fixed = (UINT8*)BurnMalloc(n);
	t->base = (UINT8**)BurnMalloc(n * sizeof(UINT8*));
	memset(t->dirty, 0, n);
	memset(t->fixed, 0, n);
	memset(t->base, 0, n * sizeof(UINT8*));
}

// a work region: [start, end)
static void lm_track_work(INT32 r, UINT32 start, UINT32 end)
{
	lm_track_t *t = &lm_track[r];
	for (UINT32 pg = start >> LM_PAGE_SHIFT; pg < ((end + LM_PAGE_SIZE - 1) >> LM_PAGE_SHIFT); pg++) {
		t->fixed[pg] = 1;
		t->dirty[pg] = 1;
	}
}

static void lm_track_exit()
{
	for (INT32 r = 0; r < LM_TR_N; r++) {
		lm_track_t *t = &lm_track[r];
		if (t->base) {
			for (UINT32 i = 0; i < (t->size >> LM_PAGE_SHIFT); i++) BurnFree(t->base[i]);
		}
		BurnFree(t->base);
		BurnFree(t->dirty);
		BurnFree(t->fixed);
		t->mem = NULL;
		t->size = 0;
	}
	lm_tracking = 0;
	lm_track_warned = 0;
	lm_base_hash = 0;
}

// the power-on boot is done: what it left is the reference for the save states
static void lm_track_start()
{
	UINT32 h = 0x811c9dc5;
	for (INT32 r = 0; r < LM_TR_N; r++) {
		lm_track_t *t = &lm_track[r];
		for (UINT32 pg = 0; pg < (t->size >> LM_PAGE_SHIFT); pg++) {
			if (t->fixed[pg]) continue;
			const UINT32 *w = (const UINT32*)(t->mem + (pg << LM_PAGE_SHIFT));
			for (UINT32 i = 0; i < LM_PAGE_SIZE / 4; i++) h = (h ^ w[i]) * 0x01000193;
		}
	}
	lm_base_hash = h;
	lm_tracking = 1;
}

static void lm_track_scan(INT32 nAction)
{
	static const char *names[LM_TR_N] = { "lm sdram page", "lm cram page", "lm sram page", "lm flash page" };
	struct BurnArea ba;

	UINT32 hash = lm_base_hash;
	SCAN_VAR(hash);
	if ((nAction & ACB_WRITE) && hash != lm_base_hash) {
		bprintf(PRINT_ERROR, _T("Paprium: the state comes from a different cart start-up (other firmware or settings?)\n"));
	}

	for (INT32 r = 0; r < LM_TR_N; r++) {
		lm_track_t *t = &lm_track[r];
		UINT32 n = t->size >> LM_PAGE_SHIFT;
		UINT8 *was = NULL;

		if (nAction & ACB_WRITE) {
			was = (UINT8*)BurnMalloc(n);
			memcpy(was, t->dirty, n);
		}
		if (nAction & ACB_READ) {
			// a page written back to what the boot left needs no saving
			for (UINT32 pg = 0; pg < n; pg++) {
				if (t->dirty[pg] && !t->fixed[pg] && !memcmp(t->base[pg], t->mem + (pg << LM_PAGE_SHIFT), LM_PAGE_SIZE)) t->dirty[pg] = 0;
			}
		}

		ba.Data = t->dirty; ba.nLen = n; ba.nAddress = 0; ba.szName = "lm pages"; BurnAcb(&ba);

		for (UINT32 pg = 0; pg < n; pg++) {
			if (t->dirty[pg]) {
				if ((nAction & ACB_WRITE) && !was[pg]) {
					t->dirty[pg] = 0;
					lm_page_dirty(t, pg);	// keep the boot's content before it is overwritten
				}
				if (!t->fixed[pg] && !lm_track_warned) {
					lm_track_warned = 1;
					bprintf(0, _T("Paprium: the cart wrote outside its work regions (%S %x), the save states get bigger\n"), names[r], pg << LM_PAGE_SHIFT);
				}
				ba.Data = t->mem + (pg << LM_PAGE_SHIFT); ba.nLen = LM_PAGE_SIZE; ba.nAddress = 0; ba.szName = (char*)names[r];
				BurnAcb(&ba);
			} else if (was && was[pg]) {
				memcpy(t->mem + (pg << LM_PAGE_SHIFT), t->base[pg], LM_PAGE_SIZE);	// back to the boot's content
			}
		}

		BurnFree(was);
	}
}

/* ---------------------------------------------------------------------------------------
   sprite decoder: the cart FPGA's 0x80 unpacker on the load-list queue
   --------------------------------------------------------------------------------------- */

static void lm_ws_put(UINT32 o, UINT8 v)
{
	lm_touch(LM_TR_WS, (o & (LM_WS_SIZE - 1)) ^ 1);
	lm_ws[(o & (LM_WS_SIZE - 1)) ^ 1] = v;
}

static void lm_dec_start()
{
	if (lm.dec_depth == 0) return;

	UINT32 base = lm.dec_base & 0x1ffffe;
	UINT32 o = 0;
	UINT8 ring[0x100];
	memset(ring, 0, sizeof(ring));

	lm.sptr = base;

	for (INT32 e = lm.dec_depth - 1; e >= 0; e--) {
		UINT32 a = lm.dec_q[e] & 0x7ffffe;
		if (RomMain[a ^ 1] != 0x80) { lm.dec_stat |= 1 << 17; continue; }
		a++;

		while (1) {
			UINT8 t = RomMain[(a++ & 0x7fffff) ^ 1];
			UINT32 n = t & 0x3f;
			UINT32 code = t >> 6;
			UINT8 arg = 0;

			if (code == 0 && n == 0) break;
			if (code == 1 || code == 2) arg = RomMain[(a++ & 0x7fffff) ^ 1];

			while (n--) {
				UINT8 v = 0;
				switch (code) {
					case 0: v = RomMain[(a++ & 0x7fffff) ^ 1]; break;
					case 1: v = arg; break;
					case 2: v = ring[(o - arg) & 0xff]; break;
					case 3: v = 0; break;
				}
				ring[o & 0xff] = v;
				lm_ws_put(base + o, v);
				o++;
			}
		}
	}

	lm.dec_depth = 0;
	lm.dec_words = o >> 1;
	lm.dec_stat |= 1 << 11;

	// GO from the decoder
	lm.remaining = o >> 1;
	lm.stream_valid = (lm.remaining != 0);
	if (lm.remaining) lm.guard = 0;
}

/* ---------------------------------------------------------------------------------------
   the register file, mcu side (0x0B00_0000 + chip offset)
   --------------------------------------------------------------------------------------- */

static void lm_reg_write16(UINT32 off, UINT16 v)
{
	switch (off & 0x1e) {
		case 0x04: lm.r_e4 = v; break;
		case 0x06: lm.r_e6 = v; break;
		case 0x08:
			if (v & 1) lm.window_en = 1;
			else if (v == 0) {
				if (lm.window_en) lm.stream_valid = 0;
				lm.window_en = 0; lm.mini_view = 1;
			}
			break;
		case 0x0a: lm.r_ea = v; lm.guard = 0; break;
		case 0x0c: lm.r_ec = v; break;
		case 0x0e:
			if (v == 0xaf5b) {
				if (lm.window_en) lm.stream_valid = 0;
				lm.window_en = 0; lm.mini_view = 1;
			}
			break;
		case 0x12:
			if (v == 0 || (v & 0x8000)) { lm.stream_valid = 0; lm.guard = 0; }
			if (v == 0x8000) lm_dec_start();
			break;
		case 0x14: lm.dec_hi = v & 0x7f; break;
		case 0x16:
			if (v & 1) lm.dec_depth = 0;
			else if (lm.dec_depth < 16) lm.dec_q[lm.dec_depth++] = (lm.dec_hi << 16) | v;
			else lm.dec_stat |= 1 << 16;
			break;
	}
}

static UINT16 lm_reg_read16(UINT32 off)
{
	switch (off & 0x1e) {
		case 0x04: return lm.r_e4;
		case 0x06: return lm.r_e6;
		case 0x0a: return lm.r_ea;
		case 0x0c: return lm.r_ec;
	}
	return 0;
}

/* ---------------------------------------------------------------------------------------
   fpgaio (0x0100_0000, slot = addr[6:2])
   --------------------------------------------------------------------------------------- */

static UINT32 lm_fio_read(INT32 slot)
{
	switch (slot) {
		case 0:  return ((lm.cause & 1) << 15) | lm.ctrl;
		case 2:  return lm.sptr;
		case 7:  return (lm.stream_valid << 31) | lm.remaining;
		case 8:  return lm.cause;
		case 9:  return lm.msip;
		case 10: return rv32_timer(&lm_cpu);
		case 11: return lm_cpu.tcmp;
		case 12: return lm_cpu.ten;
		case 13: return lm.stamp_b;
		case 14: return lm.stamp_a;
		case 15: return lm.wlock;
		case 16: return lm.vscnt & 0xffff;
		case 17: return lm.dec_base & 0x1ffffe;
		case 18: return lm.dec_depth | lm.dec_stat;
		case 19: return lm.dec_words;
		case 20: return 0x80000000;
		case 21: return 0x7fffffff;
		case 22: return 0;
		case 23: return LM_FEAT;
	}
	return 0xffffffff;
}

static void lm_fio_write(INT32 slot, UINT32 v)
{
	switch (slot) {
		case 0:
			lm.ctrl = v & 0xff;
			if (lm.ctrl & 1) lm.held = 1;
			else if (lm.held) { lm.held = 0; lm.release_pending = 1; }
			break;
		case 2: lm.sptr = v & 0x1ffffe; break;
		case 6: if (v & 1) lm.cause &= ~LM_CAUSE_B; break;
		case 7:
			if (v & 0x1fffff) { lm.remaining = v & 0x1fffff; lm.stream_valid = 1; lm.guard = 0; }
			else lm.stream_valid = 0;
			break;
		case 8: lm.cause &= ~(v & 0x1f); break;
		case 9: lm.msip = v & 1; break;
		case 11: lm_cpu.tcmp = v; break;
		case 12: lm_cpu.ten = v & 1; break;
		case 15: lm.wlock |= v & 0x1fffff; break;
		case 17: lm.dec_base = v & 0x1ffffe; break;
		case 18: if (v & 1) lm.dec_stat &= ~0x3f0000; break;
	}
	lm_update_irq();
}

/* ---------------------------------------------------------------------------------------
   the synth (region 0x06): the cart chip's block scheduler and mixer. Blocks of 160 frames
   are mixed at once when their TIM6 instant is serviced; the Datenmeister output is played at TIM8 (48 kHz)
   --------------------------------------------------------------------------------------- */

#define SYN_TICKS_HZ	480000		// tenths of a TIM8 period per second
#define SYN_QN			8
#define SYN_OUT_SIZE	0x2000

#define FL_SEQON		0x01
#define FL_PLAYING		0x02
#define FL_SFXON		0x04
#define FL_BGMECHO		0x08
#define FL_SFXECHO		0x10
#define FL_LEVELNZ		0x20

#define CR_BGMRING		0x152100
#define CR_SFXRING		0x15c100
#define CR_DACBUF_L		0x170000
#define CR_DACBUF_R		0x170f00
#define CR_CURVE_CTR	0x198000

#include "paprium_lm_tables.h"

struct lm_synth {
	UINT32 config, flags, wmaster, wboost, smaster, smcfg, echob, echos;
	UINT32 done, ymfilt, dtout, diag, timeout;
	UINT16 arr, seqlat_a, seqlat_b, pad0;
	UINT32 voice[16][16];
	UINT32 sfx[8][16];

	// scheduler words
	UINT8 path, fp, cur, fill, cnt, nblk, dtrun, pad1;
	UINT16 half, blk, dt_idx, pad2;

	// TIM6 queue
	UINT32 q_need[SYN_QN], q_pos[SYN_QN];
	UINT64 q_ts[SYN_QN];
	INT32 q_rp, q_n;

	// timebase: ticks (tenths of a TIM8 period) since the epoch
	UINT64 epoch, tick;

	// the gain chain's held boost slot
	UINT32 slot;
	UINT8 slot_ring, pad3[3];

	// the Datenmeister output stand-in
	INT32 lp_l, lp_r;
};

static lm_synth syn;
static UINT16 lm_level_T[512];

// 48 kHz output (the Datenmeister path), consumed every frame
static INT16 *lm_out = NULL;
static INT32 lm_out_w = 0, lm_out_r = 0;

static void syn_tables_init()
{
	UINT32 x = 256u << 23;
	for (INT32 k = 255; k >= 2; k--) {
		UINT32 c = (x + ((1u << 23) - 1u)) >> 23;
		lm_level_T[k] = (c > 255) ? 255 : c;
		x = (UINT32)(((UINT64)x * 2114190000u) >> 31);
	}
	lm_level_T[0] = lm_level_T[1] = 0;
	for (INT32 k = 0; k < 256; k++) lm_level_T[256 + k] = 255 + k;
}

static inline UINT16 cr16(UINT32 a) { a &= LM_CRAM_SIZE - 2; return lm_cram[a] | (lm_cram[a + 1] << 8); }
static inline void cw16(UINT32 a, UINT16 v) { a &= LM_CRAM_SIZE - 2; lm_touch(LM_TR_CRAM, a); lm_cram[a] = v; lm_cram[a + 1] = v >> 8; }

// the bus of one block: bgm 160 frames, sfx 80 frames (24 kHz), 16-bit lanes that wrap
static UINT16 bus_bgm[160][2], bus_sfx[80][2];

static inline INT16 div256(INT32 p) { return (INT16)(p / 256); }	// toward zero

/* ---- B3: the wave envelope ---- */
static void syn_envelope()
{
	INT32 playing = syn.flags & FL_PLAYING;
	if (playing && !(syn.flags & FL_SEQON)) return;

	for (INT32 v = 0; v < 16; v++) {
		UINT32 *w = syn.voice[v];

		if (!playing) {	// the stopped-mode decay
			INT16 lev = w[7] & 0xffff;
			if (lev != 0) {
				lev = lev - ((lev > 7) ? 8 : 1);
				w[7] = (w[7] & 0xffff0000) | (UINT16)lev;
			}
			continue;
		}

		UINT32 st = w[8] & 0xff;
		if (st > 7) continue;
		INT16 lev = w[7] & 0xffff;
		UINT16 cnt = w[7] >> 16;
		UINT32 r1 = w[9], r2 = w[10];
		UINT8 ar = r1, hold = r1 >> 8, dr = r1 >> 16, sl = r1 >> 24;
		UINT8 sus = r2, d2r = r2 >> 8, rr = r2 >> 16;

		INT32 st_lev = 0, st_cnt = 0, st_st = 0;
		INT16 n_lev = lev;
		UINT16 n_cnt = cnt;
		UINT32 n_st = st;
		UINT32 m6;

		switch (st) {
			case 0:
				n_lev = 0; st_lev = 1;
				break;
			case 1: {
				INT16 s = lev + ar;
				st_lev = 1;
				if (ar == 0) n_lev = 0x5ff;
				else if (ar != 0xff && s <= 0x5fe) n_lev = s;
				else { n_lev = 0x5ff; n_cnt = 0; n_st = 2; st_cnt = st_st = 1; }
				break;
			}
			case 2: case 4:
				m6 = ((st == 2) ? hold : sus) * 6;
				n_cnt = cnt + 1; st_cnt = 1;
				if (n_cnt >= m6) { n_st = st + 1; st_st = 1; }
				break;
			case 3: {
				INT16 s = lev - dr;
				m6 = sl * 6;
				st_lev = 1;
				if (s <= (INT32)m6) { n_lev = m6; n_cnt = 0; n_st = 4; st_cnt = st_st = 1; }
				else n_lev = s;
				break;
			}
			case 5: case 6: {
				INT16 s = lev - ((st == 5) ? d2r : rr);
				st_lev = 1;
				if (s <= 0) { n_lev = 0; n_cnt = 0; n_st = 0; st_cnt = st_st = 1; }
				else n_lev = s;
				break;
			}
			case 7: {
				INT32 half = ((INT32)lev - ((lev < 0) ? 63 : 64)) >> 1;
				st_lev = 1;
				if ((INT16)half <= 0) { n_lev = 0; n_st = 0; st_st = 1; }
				else n_lev = half;
				break;
			}
		}

		if (st_lev) w[7] = (w[7] & 0xffff0000) | (UINT16)n_lev;
		if (st_cnt) w[7] = (w[7] & 0x0000ffff) | (n_cnt << 16);
		if (st_st)  w[8] = (w[8] & 0xffffff00) | n_st;
	}
}

/* ---- B4: the gain chain of one voice ---- */
static void syn_gains(UINT32 *w, UINT16 *gl, UINT16 *gr)
{
	INT16 level = w[7] & 0xffff;
	UINT32 bb = (w[8] >> 8) & 0xff;

	if (syn.flags & FL_LEVELNZ) {
		syn.slot_ring = 0;
		syn.slot = bb ? (syn.wboost & 0xffff) : ((syn.wboost & 0xffff) >> 1);
	}
	UINT32 bp = (syn.slot_ring ? 0xc01e5800 : syn.slot) + 0x100;

	UINT32 t;
	if (level < 0) t = 0;
	else if (level >= 3072) t = 510;
	else t = lm_level_T[((((UINT32)level >> 1) * 683) >> 11) & 0x1ff];

	UINT32 wavemaster = syn.wboost >> 16;
	UINT32 wm[2] = { syn.wmaster & 0xffff, syn.wmaster >> 16 };
	UINT32 gain[2] = { w[5] & 0xffff, w[5] >> 16 };
	UINT16 out[2];

	for (INT32 s = 0; s < 2; s++) {
		UINT32 t1 = (wm[s] * bp) >> 8;
		UINT32 t2 = (UINT32)((UINT64)wavemaster * t1) >> 8;
		UINT32 m = (t2 > 0xffff) ? 0xffff : t2;
		UINT32 q1 = gain[s] * t;
		UINT32 a = (q1 >> 24) ? 0x1ff : ((q1 >> 8) & 0x1ff);
		UINT32 q2 = m * a;
		UINT32 g1 = (q2 >> 24) ? 0xffff : ((q2 >> 8) & 0xffff);
		out[s] = (g1 * 6) & 0xffff;
	}
	*gl = out[0];
	*gr = out[1];
}

/* ---- B5: the 16 wave voices ---- */
static void syn_wave(INT32 ym)
{
	if ((syn.flags & (FL_SEQON | FL_PLAYING)) != (FL_SEQON | FL_PLAYING)) return;

	for (INT32 v = 0; v < 16; v++) {
		UINT32 *w = syn.voice[v];
		UINT32 len = w[1];
		INT16 level = w[7] & 0xffff;

		if ((w[8] & 0xff) == 0 || len == 0 || level == 0 || (w[8] >> 16) != 0) {
			w[6] = 0;
			continue;
		}

		UINT16 gl, gr;
		syn_gains(w, &gl, &gr);
		INT32 mono = (gl == gr) || ym;
		UINT32 ga = mono ? (((UINT32)gl + gr) >> 1) : gl;
		UINT32 gb = mono ? ga : gr;

		UINT32 pos = w[0];
		UINT32 loop = w[2];
		INT32 one = (loop == 0xffffffff);
		UINT32 base = w[3] & 0x3fffff;
		UINT32 step = w[4] & 0xffff;
		UINT32 dck = w[4] >> 16;
		INT32 dc = (dck > 160) ? 160 : (dck & 0xff);
		INT16 oldl = w[6] & 0xffff, oldr = w[6] >> 16;
		INT32 lenhi = (len >> 24) != 0;
		UINT32 len24 = len & 0xffffff;

		UINT32 endpos = pos + 160 * step;
		INT32 slow = !lenhi && (len24 < (endpos >> 8));
		INT32 ended = 0;
		UINT16 lastl = 0, lastr = 0;

		for (INT32 j = 0; j < 160; j++) {
			UINT32 n = pos + step;
			if (slow && (n >> 8) >= len24) {
				if (one) { pos = n; ended = 1; break; }
				n = (loop & 0xffffff) << 8;
			}
			pos = n;
			INT32 s = (INT8)lm_cram[(base + ((pos >> 8) & 0x3fffff)) & 0x3fffff];
			UINT16 ol = div256(s * (INT32)ga), orr = div256(s * (INT32)gb);
			bus_bgm[j][0] += ol;
			bus_bgm[j][1] += orr;
			lastl = ol; lastr = orr;
		}

		for (INT32 k = dc, f = 0; k >= 1; k--, f++) {	// the declick ramp from the old LAST
			bus_bgm[f][0] += (UINT16)((32 * k * oldl) / 256);
			bus_bgm[f][1] += (UINT16)((32 * k * oldr) / 256);
		}

		w[0] = pos;
		if (ended && mono) { lastl = lastr = 0; }
		w[6] = lastl | (lastr << 16);
		if (ended) w[8] &= 0xffffff00;
		if (dc) w[4] &= 0x0000ffff;
	}
}

/* ---- B6: the 8 sfx channels ---- */
static inline UINT32 clamp16x(UINT32 p) { return (p >> 24) ? 0xffff : ((p >> 8) & 0xffff); }

static void syn_sfx_channels(INT32 ym)
{
	if (!(syn.flags & FL_SFXON)) return;

	UINT32 sfxmaster = syn.smcfg & 0xffff;
	UINT32 gml = clamp16x((syn.smaster & 0xffff) * sfxmaster);
	UINT32 gmr = clamp16x((syn.smaster >> 16) * sfxmaster);

	for (INT32 c = 0; c < 8; c++) {
		UINT32 *w = syn.sfx[c];
		if ((w[4] >> 16) == 0) continue;
		if ((w[7] & 0xffff) != 0) {
			w[7] = (w[7] & 0xffff0000) | ((w[7] - 1) & 0xffff);
			continue;
		}

		UINT32 ptr = w[0] & 0x3fffff, base = w[1] & 0x3fffff, pos = w[2], len = w[3];
		INT32 fmt8 = (w[4] & 0xff) == 1;
		UINT32 rate = (w[4] >> 8) & 0xff;
		UINT32 loopf = w[6] & 0xff, filt = (w[6] >> 8) & 7;
		INT16 y = w[6] >> 16;
		INT32 ended;

		if (pos >= len) {
			ended = 1;
		} else {
			UINT32 n = (len >= pos + 80) ? 80 : ((len - pos) & 0x7f);
			UINT32 gl = clamp16x(gml * (w[5] & 0xffff));
			UINT32 gr = clamp16x(gmr * (w[5] >> 16));
			INT32 mono = (gl == gr) || ym;
			INT32 rate_ok = rate >= 1 && rate <= 5;
			UINT32 h0 = rate & 7, h1 = (rate == 3) ? 2 : (rate & 7);
			UINT32 gw = fmt8 ? (h0 + h1) : 2 * (h0 + h1);

			// the words this call fetches
			UINT32 words = 0;
			if (rate_ok) words = (n + gw - 1) / gw;
			UINT32 wptr = ptr;
			ptr += 2 * words;

			UINT16 wcur = 0;
			UINT32 part = 0, hc = 0, need = 1;
			for (UINT32 i = 0; i < n; i++) {
				INT32 x = 0;
				if (rate_ok) {
					if (need) { wcur = cr16(wptr); wptr += 2; need = 0; part = 0; hc = 0; }
					if (fmt8) x = lm_sfx8[part ? (wcur & 0xff) : (wcur >> 8)];
					else x = lm_sfx4[(wcur >> (12 - 4 * part)) & 0xf];
				}

				INT32 yy = y;
				switch (filt) {
					case 1: y = (x + yy) / 2; break;
					case 2: y = (x + 2 * yy) / 3; break;
					case 3: y = (x + 3 * yy) / 4; break;
					case 4: y = (x + 4 * yy) / 5; break;
					case 5: y = (x + 7 * yy) / 8; break;
					default: y = x; break;
				}

				INT16 ol = div256(y * (INT32)(mono ? ((gl + gr) >> 1) : gl));
				INT16 orr = mono ? ol : div256(y * (INT32)gr);
				bus_sfx[i][0] += ol;
				bus_sfx[i][1] += orr;

				// the next sample's part / hold
				UINT32 hlen = (part & 1) ? h1 : h0;
				INT32 plast = fmt8 ? (part & 1) : (part == 3);
				if (hc + 1 >= hlen) {
					hc = 0;
					if (plast) need = 1; else part++;
				} else hc++;
			}
			pos += n;
			ended = (pos >= len);
		}

		INT32 rew = ended && loopf;
		w[0] = rew ? base : (ptr & 0x3fffff);
		w[2] = rew ? 0 : pos;
		w[6] = (w[6] & 0x0000ffff) | ((UINT16)y << 16);
		if (ended && !loopf) w[4] &= 0x0000ffff;
	}
}

/* ---- B7: the echo rings ---- */
static void syn_prefill(INT32 sfx, INT32 ym)
{
	INT32 n = sfx ? 80 : 160;
	UINT16 (*bus)[2] = sfx ? bus_sfx : bus_bgm;
	UINT32 reg = sfx ? syn.echos : syn.echob;
	INT32 on = !ym && (sfx ? ((syn.flags & FL_SFXON) && (syn.flags & FL_SFXECHO)) : (syn.flags & FL_BGMECHO));

	if (!on) { memset(bus, 0, n * 4); return; }

	INT16 gain = reg & 0xffff;
	UINT8 blk = (UINT8)((reg >> 24) - (reg >> 16));
	UINT32 ring = (sfx ? CR_SFXRING : CR_BGMRING) + blk * (n / 4) * 4;
	for (INT32 j = 0; j < n / 4; j++) {
		INT16 l = cr16(ring + 4 * j), r = cr16(ring + 4 * j + 2);
		UINT16 ol = div256(gain * l), orr = div256(gain * r);
		for (INT32 k = 0; k < 4; k++) { bus[4 * j + k][0] = ol; bus[4 * j + k][1] = orr; }
	}
}

static void syn_ring_write(INT32 sfx)
{
	if (sfx && !(syn.flags & FL_SFXON)) return;
	INT32 n = sfx ? 80 : 160;
	UINT16 (*bus)[2] = sfx ? bus_sfx : bus_bgm;
	UINT32 *reg = sfx ? &syn.echos : &syn.echob;
	UINT8 idx = (UINT8)((*reg >> 24) + 1);
	*reg = (*reg & 0x00ffffff) | (idx << 24);
	UINT32 ring = (sfx ? CR_SFXRING : CR_BGMRING) + idx * (n / 4) * 4;
	for (INT32 j = 0; j < n / 4; j++) {
		cw16(ring + 4 * j, bus[4 * j][0]);
		cw16(ring + 4 * j + 2, bus[4 * j][1]);
	}
}

/* ---- B8: the output stage ---- */
static inline UINT16 syn_curve(INT32 x)
{
	if (!(syn.config & 8)) {
		if (x < -8192) x = -8192;
		if (x > 8191) x = 8191;
	}
	return cr16(CR_CURVE_CTR + 2 * x);
}

static void syn_output(UINT32 idx, INT32 ym)
{
	INT16 fa = syn.ymfilt & 0xffff, fb = syn.ymfilt >> 16;

	for (INT32 i = 0; i < 80; i++) {
		for (INT32 h = 0; h < 2; h++) {
			UINT32 e = (idx + 2 * i + h) % 1920;
			UINT16 *bgm = bus_bgm[2 * i + h];
			if (ym) {
				INT16 ys = (INT16)(bgm[0] + bus_sfx[i][0]);
				INT32 yx = (ys < -0x1fff) ? -0x1ffe : (ys > 0x1fff) ? 0x1ffe : ys;
				INT32 t = (h ? fb : fa) + yx;
				INT16 yn = (t + (t < 0)) >> 1;
				if (h) fb = yn; else fa = yn;
				cw16(CR_DACBUF_L + 2 * e, syn_curve(yn) >> 4);
			} else {
				for (INT32 s = 0; s < 2; s++) {
					INT32 b = (INT16)bgm[s];
					INT32 d = ((b + (b < 0)) >> 1) + (INT16)bus_sfx[i][s];
					cw16((s ? CR_DACBUF_R : CR_DACBUF_L) + 2 * e, syn_curve(d));
				}
			}
		}
	}

	if (ym) syn.ymfilt = (UINT16)fa | ((UINT32)(UINT16)fb << 16);
}

static void syn_block(UINT32 idx, INT32 ym)
{
	syn_envelope();
	syn_prefill(0, ym);
	syn_wave(ym);
	syn_ring_write(0);
	syn_prefill(1, ym);
	if ((syn.flags & FL_SFXON) && !ym && (syn.flags & FL_SFXECHO)) syn.slot_ring = 1;	// the sfx prefill stores the ring's address in the slot
	syn_sfx_channels(ym);
	syn_ring_write(1);
	syn_output(idx, ym);
}

/* ---- B2: the scheduler ---- */
static inline UINT64 syn_tick_cycle(UINT64 k)
{
	return syn.epoch + (k * LM_CLK_HZ + SYN_TICKS_HZ - 1) / SYN_TICKS_HZ;
}

static void syn_decide(UINT32 pos)
{
	UINT32 idx = 0;
	INT32 mix = 0;
	UINT32 nb = syn.nblk;

	if (!syn.path) {
		INT32 c1 = (pos >= syn.half);
		UINT32 k1 = syn.cnt;
		if (syn.cur != c1) {
			if (syn.cnt < nb) syn.diag = (syn.diag & ~0xff) | (((syn.diag & 0xff) == 0xff) ? 0xff : ((syn.diag & 0xff) + 1));
			syn.cur = c1;
			syn.fill = (pos < syn.half);
			k1 = 0;
		}
		syn.cnt = k1;
		if (syn.half == 0) {
			if (k1 < nb) syn.cnt = k1 + 1;
		} else if (k1 < nb) {
			mix = 1;
			idx = (syn.fill ? syn.half : 0) + 160 * k1;
			syn.cnt = k1 + 1;
			syn.blk = (syn.blk + 1) & 0xfff;
		}
	} else {
		if (syn.fp && syn.cnt < nb) {
			if (syn.half) {
				mix = 1;
				idx = (syn.fill ? syn.half : 0) + 160 * syn.cnt;
				syn.blk = (syn.blk + 1) & 0xfff;
			}
			syn.cnt++;
		} else {
			UINT32 k1 = syn.cnt;
			if (syn.fp) { syn.fp = 0; syn.cur = 0; syn.fill = 0; k1 = 0; }
			syn.cnt = k1;
			if (k1 < nb) {
				if (syn.half) {
					mix = 1;
					idx = (syn.fill ? syn.half : 0) + 160 * k1;
					syn.blk = (syn.blk + 1) & 0xfff;
				}
				syn.cnt = k1 + 1;
			}
		}
	}

	if (mix) syn_block(idx, syn.path);
}

static INT32 syn_head_ok(UINT64 now)
{
	if (syn.q_n == 0) return 0;
	if (syn.q_n == SYN_QN) return 1;
	if ((INT16)(syn.done - syn.q_need[syn.q_rp]) >= 0) return 1;
	return (now - syn.q_ts[syn.q_rp]) >= (syn.timeout & 0xffffff);
}

static void syn_service(UINT64 now)
{
	while (syn_head_ok(now)) {
		UINT32 pos = syn.q_pos[syn.q_rp];
		syn.q_rp = (syn.q_rp + 1) % SYN_QN;
		syn.q_n--;
		syn_decide((syn.dtrun && syn.half) ? pos : 0);
		lm.cause |= LM_CAUSE_BLK;
	}
	lm_update_irq();
}

static void syn_strobe()
{
	INT32 l = 0, r = 0;
	if (syn.dtrun) {
		UINT32 i = syn.dt_idx;
		INT32 code[2] = { cr16(CR_DACBUF_L + 2 * i) & 0xfff, cr16(CR_DACBUF_R + 2 * i) & 0xfff };
		INT32 *lp[2] = { &syn.lp_l, &syn.lp_r };
		INT32 gain = syn.dtout & 0xffff, dck = (syn.dtout >> 17) & 0xf;
		INT32 out[2];
		for (INT32 s = 0; s < 2; s++) {
			INT32 x = code[s] - 2047;
			if (dck) {	// the dc blocker: the output uses the state before this sample
				INT32 lr = (*lp[s] + 0x8000) >> 16;
				*lp[s] += ((x * 65536) - *lp[s]) >> dck;
				x -= lr;
			}
			INT32 y = (x * gain) >> 8;
			if (syn.dtout & 0x10000) y = -y;
			out[s] = (y < -32768) ? -32768 : (y > 32767) ? 32767 : y;
		}
		l = out[0]; r = out[1];
		syn.dt_idx = (syn.dt_idx + 1) % 1920;
	}
	if (lm_out) {
		lm_out[lm_out_w * 2 + 0] = l;
		lm_out[lm_out_w * 2 + 1] = r;
		lm_out_w = (lm_out_w + 1) % SYN_OUT_SIZE;
	}
}

// brings the synth's timebase up to mcu cycle now
static void syn_catchup(UINT64 now)
{
	while (1) {
		UINT64 k8 = (syn.tick / 10 + 1) * 10;
		UINT64 k6 = (syn.tick / 1001 + 1) * 1001;
		UINT64 k = (k8 < k6) ? k8 : k6;
		UINT64 tc = syn_tick_cycle(k);

		if (syn.q_n) {	// a head that waited TIMEOUT is serviced anyway
			UINT64 td = syn.q_ts[syn.q_rp] + (syn.timeout & 0xffffff);
			if (td < tc && td <= now) { syn_service(td); continue; }
		}
		if (tc > now) break;

		syn.tick = k;
		if ((k % 10) == 0) syn_strobe();
		if ((k % 1001) == 0) {
			if (syn.q_n == SYN_QN) syn_service(tc);
			INT32 wp = (syn.q_rp + syn.q_n) % SYN_QN;
			syn.q_need[wp] = syn.arr;
			syn.q_pos[wp] = syn.dt_idx;
			syn.q_ts[wp] = tc;
			syn.q_n++;
			syn_service(tc);
		}
	}
}

// the next cycle the scheduler needs the mcu to stop at (a TIM6 instant or a timeout)
static UINT64 syn_next_event()
{
	UINT64 t = syn_tick_cycle((syn.tick / 1001 + 1) * 1001);
	if (syn.q_n) {
		UINT64 td = syn.q_ts[syn.q_rp] + (syn.timeout & 0xffffff);
		if (td < t) t = td;
	}
	return t;
}

static void syn_config_apply()
{
	switch (syn.config & 3) {
		case 1:
			syn.path = 0; syn.half = 960; syn.nblk = 6;
			syn.fill = 1; syn.cur = 0; syn.cnt = 0xff;
			syn.dtrun = 1;
			break;
		case 2: {
			INT32 pal = (syn.config >> 2) & 1;
			syn.path = 1; syn.half = pal ? 960 : 800; syn.nblk = pal ? 6 : 5;
			syn.fill = 1; syn.cur = 0; syn.cnt = 0xff;
			syn.dtrun = 0;
			break;
		}
		default:
			syn.half = 0;
			syn.dtrun = 0;
			break;
	}
}

static UINT32 syn_read(UINT32 off)
{
	syn_catchup(lm_cpu.cycles);

	off &= 0xffc;
	if (off >= 0x400 && off < 0x800) return syn.voice[(off >> 6) & 15][(off >> 2) & 15];
	if (off >= 0x800 && off < 0xa00) return syn.sfx[(off >> 6) & 7][(off >> 2) & 15];

	switch (off) {
		case 0x000: return LM_SYN_ID;
		case 0x004: return syn.config;
		case 0x008: return syn.flags;
		case 0x00c: return syn.wmaster;
		case 0x010: return syn.wboost;
		case 0x014: return syn.smaster;
		case 0x018: return syn.smcfg;
		case 0x01c: return syn.echob;
		case 0x020: return syn.echos;
		case 0x028: return ((UINT32)syn.done << 16) | syn.arr;
		case 0x02c: return syn.done & 0xffff;
		case 0x030: return (syn_head_ok(lm_cpu.cycles) ? 1 : 0) | (syn.fp << 2) | (syn.fill << 3) | (syn.cur << 4) | (syn.q_n << 5) | ((UINT32)syn.blk << 16);
		case 0x034: return syn.dt_idx | ((UINT32)((syn.tick % 1001) / 10) << 16);
		case 0x038: return syn.ymfilt;
		case 0x03c: return syn.dtout;
		case 0x040: return syn.diag;
		case 0x044: return syn.timeout;
		case 0x048: return ((UINT32)syn.seqlat_a << 16) | syn.seqlat_b;
		case 0x04c: return syn.cnt | (syn.nblk << 8) | ((UINT32)syn.half << 16);
	}
	return 0;
}

static void syn_write(UINT32 off, UINT32 v, UINT32 mask)
{
	syn_catchup(lm_cpu.cycles);

	off &= 0xffc;
	UINT32 *p = NULL, keep = 0xffffffff;
	if (off >= 0x400 && off < 0x800) {
		INT32 wd = (off >> 2) & 15;
		p = &syn.voice[(off >> 6) & 15][wd];
		keep = (wd == 3) ? 0x003fffff : (wd >= 11) ? 0 : 0xffffffff;
	}
	else if (off >= 0x800 && off < 0xa00) {
		INT32 wd = (off >> 2) & 15;
		p = &syn.sfx[(off >> 6) & 7][wd];
		keep = (wd <= 1) ? 0x003fffff : (wd == 7) ? 0x0000ffff : (wd >= 8) ? 0 : 0xffffffff;
	}
	else switch (off) {
		case 0x004: p = &syn.config; break;
		case 0x008: p = &syn.flags; keep = 0x3f; break;
		case 0x00c: p = &syn.wmaster; break;
		case 0x010: p = &syn.wboost; break;
		case 0x014: p = &syn.smaster; break;
		case 0x018: p = &syn.smcfg; keep = 0xffff; break;
		case 0x01c: p = &syn.echob; break;
		case 0x020: p = &syn.echos; break;
		case 0x024: if (mask & 1) syn.fp = v & 1; return;
		case 0x02c: p = &syn.done; keep = 0xffff; break;
		case 0x034: return;
		case 0x038: p = &syn.ymfilt; break;
		case 0x03c: p = &syn.dtout; keep = 0x1fffff; break;
		case 0x040: syn.diag = 0; return;
		case 0x044: p = &syn.timeout; keep = 0xffffff; break;
	}
	if (p == NULL) return;
	*p = ((*p & ~mask) | (v & mask)) & keep;

	if (off == 0x004) syn_config_apply();
	if (off == 0x02c) syn_service(lm_cpu.cycles);
}

static void syn_post()
{
	syn_catchup(lm_cpu.cycles);
	syn.arr++;
}

static void syn_reset()
{
	UINT32 voice[16][16], sfx[8][16];
	memcpy(voice, syn.voice, sizeof(voice));
	memcpy(sfx, syn.sfx, sizeof(sfx));
	memset(&syn, 0, sizeof(syn));
	memcpy(syn.voice, voice, sizeof(voice));
	memcpy(syn.sfx, sfx, sizeof(sfx));

	syn.timeout = LM_CLK_HZ / 200;
	syn.epoch = lm_cpu.cycles;
	lm_out_w = lm_out_r = 0;
}

// mixes the frame's Datenmeister output (48 kHz) into the sound buffer
static void syn_render(INT16 *dst, INT32 len)
{
	INT32 avail = (lm_out_w - lm_out_r + SYN_OUT_SIZE) % SYN_OUT_SIZE;
	if (avail < 2 || len == 0) return;

	// keep the latency bounded
	if (avail > len * 3) {
		lm_out_r = (lm_out_w - len + SYN_OUT_SIZE) % SYN_OUT_SIZE;
		avail = len;
	}

	UINT32 step = (UINT32)(((UINT64)avail << 16) / len);
	UINT32 pos = 0;
	for (INT32 i = 0; i < len; i++, pos += step) {
		INT32 p0 = (lm_out_r + (pos >> 16)) % SYN_OUT_SIZE;
		INT32 p1 = ((INT32)(pos >> 16) + 1 < avail) ? (p0 + 1) % SYN_OUT_SIZE : p0;
		INT32 f = pos & 0xffff;
		INT32 l = lm_out[p0 * 2 + 0] + (((lm_out[p1 * 2 + 0] - lm_out[p0 * 2 + 0]) * f) >> 16);
		INT32 r = lm_out[p0 * 2 + 1] + (((lm_out[p1 * 2 + 1] - lm_out[p0 * 2 + 1]) * f) >> 16);
		dst[i * 2 + 0] = BURN_SND_CLIP(dst[i * 2 + 0] + l);
		dst[i * 2 + 1] = BURN_SND_CLIP(dst[i * 2 + 1] + r);
	}
	lm_out_r = (lm_out_r + avail) % SYN_OUT_SIZE;
}

static void lm_run_until(UINT64 target)
{
	while (lm_cpu.cycles < target) {
		UINT64 t = syn_next_event();
		if (t > target) t = target;
		lm_cpu.stop_pc = lm_idle_pc;
		while (lm_cpu.cycles < t) {
			lm_cpu.stop_from = lm_cpu.instret;
			rv32_run(&lm_cpu, t);
			if (lm_cpu.cycles < t) lm_idle_skip(t);
		}
		lm_cpu.stop_pc = 0xffffffff;
		syn_catchup(lm_cpu.cycles);
	}
}

static void lm_sync()
{
	lm_run_until(lm_68k_to_mcu(SekCyclesDone()));
}

/* ---------------------------------------------------------------------------------------
   the mcu bus
   --------------------------------------------------------------------------------------- */

static inline UINT32 lm_rd(const UINT8 *p, UINT32 a, INT32 size)
{
	switch (size) {
		case 1: return p[a];
		case 2: return p[a] | (p[a + 1] << 8);
	}
	return p[a] | (p[a + 1] << 8) | (p[a + 2] << 16) | (p[a + 3] << 24);
}

static inline void lm_wr(UINT8 *p, UINT32 a, UINT32 v, INT32 size)
{
	p[a] = v;
	if (size >= 2) p[a + 1] = v >> 8;
	if (size == 4) { p[a + 2] = v >> 16; p[a + 3] = v >> 24; }
}

static inline INT32 lm_wram_locked(UINT32 a)
{
	UINT32 k = (a & 0xffff) >> 10;
	if (k >= 11 && k <= 26) return 1;
	if (k >= 48) return 1;
	if (k >= 27 && k <= 47) return (lm.wlock >> (k - 27)) & 1;
	return 0;
}

static UINT32 lm_bus_read(UINT32 addr, INT32 size, INT32 *cost, INT32 *err)
{
	switch (addr >> 24) {
		case 0x00:
			*cost += 5;
			if ((addr & 0x1ffff) >= LM_IROM_SIZE) return 0;
			return lm_rd(lm_irom, addr & 0x1ffff, size);

		case 0x80:
			*cost += 5;
			if ((addr & 0xffff) >= LM_WRAM_SIZE) return 0;
			return lm_rd(lm_wram, addr & 0xffff, size);

		case 0x01: {
			*cost += 7;
			UINT32 v = lm_fio_read((addr >> 2) & 31);
			return (size == 4) ? v : (v >> ((addr & 3) * 8)) & ((1 << (size * 8)) - 1);
		}

		case 0x02:
			*cost += 7;
			if (addr & 0x800000) {	// byte view
				UINT32 v = 0;
				for (INT32 i = 0; i < size; i++) v |= lm_dp[((addr + i) & 0x1fff) ^ 1] << (i * 8);
				return v;
			}
			return lm_rd(lm_dp, addr & 0x1fff, size);

		case 0x03:
			*cost += (size == 4) ? 15 : 11;
			return lm_rd(lm_ws, addr & (LM_WS_SIZE - 1), size);

		case 0x04:
			*cost += (size == 4) ? 15 : 11;
			if (addr & 0x800000) {
				UINT32 v = 0;
				for (INT32 i = 0; i < size; i++) v |= RomMain[((addr + i) & 0x7fffff) ^ 1] << (i * 8);
				return v;
			}
			return lm_rd(RomMain, addr & 0x7fffff, size);

		case 0x05:
			*cost += 7;
			return lm_rd(lm_bat, addr & (LM_BAT_SIZE - 1), size);

		case 0x06: {
			*cost += 8;
			UINT32 v = syn_read(addr);
			return (size == 4) ? v : (v >> ((addr & 3) * 8)) & ((1 << (size * 8)) - 1);
		}

		case 0x08:
			*cost += 20;
			return lm_rd(lm_xram, addr & (LM_XRAM_SIZE - 1), size);

		case 0x09:
			*cost += 40;
			return lm_rd(lm_cram, addr & (LM_CRAM_SIZE - 1), size);

		case 0x0a: {
			*cost += 3;
			UINT32 v = 0;
			switch ((addr >> 2) & 31) {
				case 0x00: v = lm.xcfg; break;
				case 0x01: v = 0x58524d32; break;
				case 0x07: v = 1; break;			// cram ready
				case 0x10: v = lm.cache; break;
				case 0x11: v = lm.cache_loaded; break;
				case 0x12: v = lm.bar | (320 << 16); break;
			}
			return (size == 4) ? v : (v >> ((addr & 3) * 8)) & ((1 << (size * 8)) - 1);
		}

		case 0x0b: {
			*cost += 7;
			UINT32 w = addr & 0x1c;
			UINT32 v = lm_reg_read16(w) | (lm_reg_read16(w + 2) << 16);
			return (size == 4) ? v : (v >> ((addr & 3) * 8)) & ((1 << (size * 8)) - 1);
		}

		case 0xff:
			*cost += 8;
			if ((addr & 0xffffff00) == 0xffffff00) {
				UINT32 v;
				switch (addr & 0xfc) {
					case 0x80: v = (Hardware & 0x80) ? 0 : (1 << 25); break;	// option word: console model 0, japan
					case 0x84: v = lm.gpio_out; break;
					case 0xe0: v = LM_CLK_HZ; break;
					case 0xe4: v = LM_USER_CODE; break;
					case 0xe8: case 0xec: case 0xf0: case 0xf4: case 0xf8: case 0xfc: v = 0; break;
					default: *err = 1; return 0;
				}
				return (size == 4) ? v : (v >> ((addr & 3) * 8)) & ((1 << (size * 8)) - 1);
			}
			*err = 1;
			return 0;
	}

	*cost += 6;
	return 0xffffffff;
}

static void lm_bus_write(UINT32 addr, UINT32 v, INT32 size, INT32 *cost, INT32 *err)
{
	UINT32 shift = (addr & 3) * 8;
	UINT32 mask = (size == 4) ? 0xffffffff : (((1 << (size * 8)) - 1) << shift);
	UINT32 wv = v << shift;

	switch (addr >> 24) {
		case 0x00:
			*cost += 5;
			return;

		case 0x80:
			*cost += 5;
			if ((addr & 0xffff) >= LM_WRAM_SIZE || lm_wram_locked(addr)) return;
			lm_wr(lm_wram, addr & 0xffff, v, size);
			return;

		case 0x01:
			*cost += 7;
			lm_fio_write((addr >> 2) & 31, (size == 4) ? v : (lm_fio_read((addr >> 2) & 31) & ~mask) | wv);
			return;

		case 0x02:
			*cost += 7;
			if (addr & 0x800000) return;
			lm_wr(lm_dp, addr & 0x1fff, v, size);
			return;

		case 0x03:
			*cost += (size == 4) ? 15 : 11;
			lm_touch(LM_TR_WS, addr & (LM_WS_SIZE - 1));
			lm_wr(lm_ws, addr & (LM_WS_SIZE - 1), v, size);
			return;

		case 0x04:
			*cost += (size == 4) ? 15 : 11;
			if (addr & 0x800000) return;
			lm_touch(LM_TR_FLASH, addr & 0x7fffff);
			lm_wr(RomMain, addr & 0x7fffff, v, size);
			return;

		case 0x05:
			*cost += 7;
			lm_wr(lm_bat, addr & (LM_BAT_SIZE - 1), v, size);
			return;

		case 0x06:
			*cost += 8;
			syn_write(addr, wv, mask);
			return;

		case 0x08:
			*cost += 15;
			if ((lm.xcfg & 0x10000) && (addr & (LM_XRAM_SIZE - 1)) < 0x10000) return;
			lm_touch(LM_TR_XRAM, addr & (LM_XRAM_SIZE - 1));
			lm_wr(lm_xram, addr & (LM_XRAM_SIZE - 1), v, size);
			return;

		case 0x09:
			*cost += 40;
			lm_touch(LM_TR_CRAM, addr & (LM_CRAM_SIZE - 1));
			lm_wr(lm_cram, addr & (LM_CRAM_SIZE - 1), v, size);
			return;

		case 0x0a:
			*cost += 3;
			switch ((addr >> 2) & 31) {
				case 0x00: lm.xcfg = (lm.xcfg & ~mask) | (wv & mask); break;
				case 0x10: if (!(lm.cache & 0x80000000)) lm.cache = v & 0x807fffff; break;
				case 0x12: lm.bar = v & 0x800007ff; break;
			}
			return;

		case 0x0b: {
			*cost += 7;
			UINT32 w = addr & 0x1c;
			UINT32 m = ((lm_reg_read16(w) | (lm_reg_read16(w + 2) << 16)) & ~mask) | (wv & mask);
			if (mask & 0x0000ffff) lm_reg_write16(w, m & 0xffff);
			if (mask & 0xffff0000) lm_reg_write16(w + 2, m >> 16);
			return;
		}

		case 0xff:
			*cost += 8;
			if ((addr & 0xfc) == 0x84) { lm.gpio_out = v; return; }
			if ((addr & 0xffffff00) == 0xffffff00 && (addr & 0xfc) >= 0xe0) return;
			if ((addr & 0xffffff00) == 0xffffff00 && (addr & 0xfc) == 0x80) return;
			*err = 1;
			return;
	}

	*cost += 6;
}

static UINT32 lm_bus_fetch(UINT32 addr, INT32 *cost, INT32 *err)
{
	switch (addr >> 24) {
		case 0x00:
			*cost += 2;
			if ((addr & 0x1ffff) >= LM_IROM_SIZE) return 0;
			return lm_rd(lm_irom, addr & 0x1ffff, 4);
		case 0x80:
			*cost += 2;
			if ((addr & 0xffff) >= LM_WRAM_SIZE) return 0;
			return lm_rd(lm_wram, addr & 0xffff, 4);
		case 0x08:
			*cost += 17;
			return lm_rd(lm_xram, addr & (LM_XRAM_SIZE - 1), 4);
	}
	*cost += 6;
	*err = 1;
	return 0;
}

static rv32_region lm_fetch_regions[3], lm_read_regions[3];
static rv32_bus lm_bus = { lm_bus_read, lm_bus_write, lm_bus_fetch, lm_fetch_regions, 3, lm_read_regions, 3 };

/* ---------------------------------------------------------------------------------------
   idle skip: the firmware has nothing to do most of the time and spins in its thread loop,
   polling memory and the timer. When one pass of that loop changes nothing, the passes
   until the next interrupt or event are skipped at their exact cost. A pass is probed on a
   copy of the cpu: no interrupt, no store that would change memory, no access to a device
   with side effects, the same live registers when it comes back to the loop. Passes only
   depend on the timer through comparisons that flip once, so equal probes at the first and
   the last pass mean all the passes between are equal too.
   --------------------------------------------------------------------------------------- */

static INT32 lm_probe_fail;
static UINT32 lm_probe_hash;

static UINT32 lm_probe_read(UINT32 addr, INT32 size, INT32 *cost, INT32 *err)
{
	switch (addr >> 24) {
		case 0x06: case 0x0b: lm_probe_fail = 1; return 0;
	}
	return lm_bus_read(addr, size, cost, err);
}

// a store is allowed when it leaves memory as it is
static void lm_probe_write(UINT32 addr, UINT32 v, INT32 size, INT32 *cost, INT32 *err)
{
	UINT8 *p = NULL;
	UINT32 a = 0;

	switch (addr >> 24) {
		case 0x00: break;
		case 0x80:
			if ((addr & 0xffff) < LM_WRAM_SIZE && !lm_wram_locked(addr)) { p = lm_wram; a = addr & 0xffff; }
			break;
		case 0x02: if (!(addr & 0x800000)) { p = lm_dp; a = addr & 0x1fff; } break;
		case 0x03: p = lm_ws; a = addr & (LM_WS_SIZE - 1); break;
		case 0x04: if (!(addr & 0x800000)) { p = RomMain; a = addr & 0x7fffff; } break;
		case 0x05: p = lm_bat; a = addr & (LM_BAT_SIZE - 1); break;
		case 0x08:
			if (!((lm.xcfg & 0x10000) && (addr & (LM_XRAM_SIZE - 1)) < 0x10000)) { p = lm_xram; a = addr & (LM_XRAM_SIZE - 1); }
			break;
		case 0x09: p = lm_cram; a = addr & (LM_CRAM_SIZE - 1); break;
		default: lm_probe_fail = 1; return;
	}

	if (p) {
		UINT32 m = (size == 4) ? 0xffffffff : (1 << (size * 8)) - 1;
		if ((lm_rd(p, a, size) & m) != (v & m)) { lm_probe_fail = 1; return; }
	}

	lm_bus_write(addr, v, size, cost, err);	// a no-op, for its cost
}

static rv32_bus lm_probe_bus = { lm_probe_read, lm_probe_write, lm_bus_fetch, lm_fetch_regions, 3, lm_read_regions, 3 };

struct lm_pass {
	UINT64 cost, insns;
	UINT32 hash;
};

// one pass of the thread loop as if it started at the given time
static INT32 lm_probe(UINT64 start, lm_pass *r)
{
	static const UINT8 live[] = { 2, 3, 4, 8, 9, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 };
	rv32_state save = lm_cpu;

	lm_cpu.cycles = start;
	lm_cpu.irq_inhibit = 1;
	lm_cpu.stop_pc = lm_idle_pc;
	lm_cpu.stop_from = lm_cpu.instret;
	lm_probe_fail = 0;
	lm_probe_hash = 0x811c9dc5;

	rv32_bus keep = lm_bus;
	lm_bus = lm_probe_bus;
	rv32_trace(&lm_probe_hash);
	rv32_run(&lm_cpu, start + 0x4000);
	rv32_trace(NULL);
	lm_bus = keep;

	INT32 ok = !lm_probe_fail && lm_cpu.pc == lm_idle_pc && lm_cpu.cycles < start + 0x4000;
	for (UINT32 i = 0; i < sizeof(live) && ok; i++) {
		if (lm_cpu.x[live[i]] != save.x[live[i]]) ok = 0;
	}
	r->cost = lm_cpu.cycles - start;
	r->insns = lm_cpu.instret - save.instret;
	r->hash = lm_probe_hash;

	lm_cpu = save;
	return ok;
}

static INT32 lm_probe_same(UINT64 start, const lm_pass *p0)
{
	lm_pass p;
	return lm_probe(start, &p) && p.cost == p0->cost && p.insns == p0->insns && p.hash == p0->hash;
}

static lm_pass lm_idle_pass;

static void lm_idle_skip(UINT64 limit)
{
	if (lm_cpu.pc != lm_idle_pc) return;

	if (!(lm_cpu.mstatus & 0x08)) return;
	if (lm_cpu.mie & (lm_cpu.irq_buf | lm_cpu.irq_lines)) return;

	UINT64 now = lm_cpu.cycles, irq = ~(UINT64)0;
	if (lm_cpu.ten && (lm_cpu.mie & RV32_IRQ_MTI)) {
		INT32 d = (INT32)(lm_cpu.tcmp - rv32_timer(&lm_cpu));
		if (d <= 0) return;
		irq = now + d;
		if (irq < limit) limit = irq;
	}

	// passes 0..k-1 end by the limit. The last pass that is like pass 0 runs for real, so the
	// registers it leaves are the ones it would have left
	UINT64 n, k;

	if (lm_idle_horizon && now <= lm_idle_horizon) {
		k = (limit - now) / lm_idle_pass.cost;
		if (k < 3) return;
		n = (lm_idle_horizon - now) / lm_idle_pass.cost;
		if (n > k - 1) n = k - 1;
	} else {
		lm_idle_horizon = 0;
		if (lm_idle_wait) { lm_idle_wait--; return; }
		if (lm_idle_cost && (limit - now) / lm_idle_cost < 3) return;	// too short to gain anything

		lm_pass p0;
		if (!lm_probe(now, &p0) || p0.cost == 0) {
			lm_idle_backoff = lm_idle_backoff ? ((lm_idle_backoff < 64) ? lm_idle_backoff * 2 : 64) : 1;
			lm_idle_wait = lm_idle_backoff;
			return;
		}
		lm_idle_backoff = 0;
		lm_idle_cost = p0.cost;

		k = (limit - now) / p0.cost;
		if (k < 3) return;

		// how far the passes stay alike: up to the timer interrupt, the next synth event or about
		// a frame, whichever comes first, so the next slices need no probe
		UINT64 far = now + 0x100000;
		if (irq < far) far = irq;
		UINT64 ev = syn_next_event();
		if (ev < far) far = ev;
		UINT64 m = (far > now) ? (far - now) / p0.cost : 0;
		if (m < k) m = k;
		m--;

		if (!lm_probe_same(now + m * p0.cost, &p0)) {
			UINT64 lo = 0, hi = m;	// pass lo is like pass 0, pass hi is not
			while (hi - lo > 1) {
				UINT64 mid = lo + (hi - lo) / 2;
				if (lm_probe_same(now + mid * p0.cost, &p0)) lo = mid; else hi = mid;
			}
			m = lo;
		}

		lm_idle_pass = p0;
		lm_idle_horizon = now + m * p0.cost;
		n = (m < k - 1) ? m : k - 1;
	}

	lm_cpu.cycles += n * lm_idle_pass.cost;
	lm_cpu.instret += n * lm_idle_pass.insns;
}

/* ---------------------------------------------------------------------------------------
   the 68000 side
   --------------------------------------------------------------------------------------- */

static void lm_post(INT32 mbox, UINT16 data)
{
	syn_post();
	if (mbox) {
		lm.r_ec = data;
		lm.cause |= LM_CAUSE_A;
		lm.stamp_a = rv32_timer(&lm_cpu);
		syn.seqlat_a = syn.arr;
	} else {
		lm.r_ea = data;
		lm.cause |= LM_CAUSE_B;
		lm.stamp_b = rv32_timer(&lm_cpu);
		syn.seqlat_b = syn.arr;
		UINT8 hb = data >> 8;
		if (hb == 0xda || hb == 0xdb || hb == 0xf2) lm.guard = 1;
	}
	lm_update_irq();
}

static UINT16 lm_window_pop()
{
	UINT32 a = lm.sptr & (LM_WS_SIZE - 2);
	UINT16 d = lm_ws[a] | (lm_ws[a + 1] << 8);
	lm.sptr = (lm.sptr + 2) & 0x1ffffe;
	if (lm.remaining) {
		lm.remaining--;
		if (lm.remaining == 0) lm.stream_valid = 0;
	}
	return d;
}

static UINT16 lm_68k_read_word(UINT32 a)
{
	a &= 0xfffe;

	if (a < 0x1fe0) return lm_dp[a] | (lm_dp[a + 1] << 8);

	if (a < 0x2000) {
		switch (a & 0x1e) {
			case 0x04: return ((lm.guard | !lm.stream_valid) << 2);
			case 0x06: return lm.r_e6;
			case 0x0a: return lm.r_ea;
			case 0x0c: return lm.r_ec;
		}
		return 0;
	}

	if (a >= 0xc000 && lm.window_en && !lm.mini_view) return lm_window_pop();

	UINT16 d = RomMain[a] | (RomMain[a + 1] << 8);
	if (a < 0x8000 && !lm.mini_view) d ^= 0xa8a2;
	return d;
}

// the mcu needs to catch up only if it could have changed the cart since: not while it is known
// to be idle
static inline void lm_sync_lazy()
{
	if (lm_idle_horizon && lm_68k_to_mcu(SekCyclesDone()) <= lm_idle_horizon) return;
	lm_sync();
}

// a read that changes nothing
static inline void lm_sync_read(UINT32 address)
{
	if ((address & 0xc000) == 0xc000 && lm.window_en && !lm.mini_view) {
		lm_sync();
		lm_idle_forget();
		return;
	}
	lm_sync_lazy();
}

static UINT16 __fastcall lm_r16(UINT32 address)
{
	lm_sync_read(address);
	return lm_68k_read_word(address);
}

static UINT8 __fastcall lm_r8(UINT32 address)
{
	lm_sync_read(address);
	UINT16 d = lm_68k_read_word(address);
	return (address & 1) ? (d & 0xff) : (d >> 8);
}

static void lm_68k_write_word(UINT32 a, UINT16 d)
{
	a &= 0xfffe;

	if (a < 0x1fe0) {
		lm_dp[a] = d & 0xff;
		lm_dp[a + 1] = d >> 8;
		return;
	}

	if (a == 0x1fea) lm_post(0, d);
	if (a == 0x1fec) lm_post(1, d);
}

static void __fastcall lm_w16(UINT32 address, UINT16 data)
{
	lm_sync();
	lm_68k_write_word(address, data);
	lm_idle_forget();
}

static void __fastcall lm_w8(UINT32 address, UINT8 data)
{
	lm_sync();
	// the cart ignores the byte strobes: the byte is on both halves of the bus
	lm_68k_write_word(address, (data << 8) | data);
	lm_idle_forget();
}

/* ---------------------------------------------------------------------------------------
   machine glue
   --------------------------------------------------------------------------------------- */

static INT32 lm_load_firmware(INT32 rom_index)
{
	UINT8 *file = (UINT8*)BurnMalloc(0x40000);
	if (BurnLoadRom(file, rom_index, 1)) {
		BurnFree(file);
		return 1;
	}

	lm_fw = (UINT8*)BurnMalloc(0x40000);
	for (INT32 i = 0; i < 0x40000; i++) lm_fw[i] = file[i ^ 1];
	BurnFree(file);

	UINT32 *w = (UINT32*)lm_fw;
	if (BURN_ENDIAN_SWAP_INT32(w[0]) != 0x52464146 || BURN_ENDIAN_SWAP_INT32(w[5]) != LM_USER_CODE || BURN_ENDIAN_SWAP_INT32(w[6]) != LM_FEAT ||
		BURN_ENDIAN_SWAP_INT32(w[3]) != 0x00583001 || BURN_ENDIAN_SWAP_INT32(w[0x3fffc / 4]) != 0x444e4546) {
		bprintf(PRINT_ERROR, _T("Paprium: unsupported Little-Man firmware file\n"));
		BurnFree(lm_fw);
		return 1;
	}

	return 0;
}

/* ---------------------------------------------------------------------------------------
   the sound cache (MCU interface v1.3, the cache slot): the cram image the firmware leaves
   after unpacking the sound bank at its first power-on. Given back at power-on, the firmware
   checks it (header, rom key, crcs) and skips the unpack. One file for all the versions of the
   game: one that does not match is unpacked again and the file rewritten
   --------------------------------------------------------------------------------------- */

#define LM_CACHE_MAX	0x152100	// header + bank

static void lm_cache_path(TCHAR *path)
{
	_stprintf(path, _T("%smd_paprium.cache"), szAppEEPROMPath);
}

// at power-on, before the boot
static void lm_cache_load()
{
	// a netplay or a recording must boot the same way everywhere: no cache
	if (is_netgame_or_recording()) return;

	TCHAR path[MAX_PATH];
	lm_cache_path(path);

	FILE *f = _tfopen(path, _T("rb"));
	if (f == NULL) return;
	UINT32 len = fread(lm_cram, 1, LM_CACHE_MAX, f);
	fclose(f);

	lm.cache_loaded = len & ~3;
}

// after the power-on boot: the firmware sets CACHE once its image is complete
static void lm_cache_save()
{
	UINT32 size = lm.cache & 0x7fffff;
	if (!(lm.cache & 0x80000000) || size == 0 || size > LM_CACHE_MAX) return;
	if (lm.cache_loaded == size) return;	// the file was taken

	TCHAR path[MAX_PATH];
	lm_cache_path(path);

	FILE *f = _tfopen(path, _T("wb"));
	if (f == NULL) return;
	fwrite(lm_cram, 1, size, f);
	fclose(f);
}

static void lm_power_on()
{
	// sections: irom, wram, the megawire sram image
	UINT32 *w = (UINT32*)lm_fw;
	for (INT32 s = 0; s < 3; s++) {
		UINT32 dest = BURN_ENDIAN_SWAP_INT32(w[0x0c + s * 3 + 0]);
		UINT32 len  = BURN_ENDIAN_SWAP_INT32(w[0x0c + s * 3 + 1]);
		UINT32 offs = BURN_ENDIAN_SWAP_INT32(w[0x0c + s * 3 + 2]);
		if (offs + len > 0x40000) continue;
		switch (dest >> 24) {
			case 0x00: if (len <= LM_IROM_SIZE) memcpy(lm_irom, lm_fw + offs, len); break;
			case 0x80: if ((dest & 0xffff) + len <= LM_WRAM_SIZE) memcpy(lm_wram + (dest & 0xffff), lm_fw + offs, len); break;
			case 0x08: if (len <= 0x10000) memcpy(lm_xram, lm_fw + offs, len); break;
		}
	}

	lm_idle_forget();
	memset(&lm, 0, sizeof(lm));
	lm.xcfg = 0;
	lm.cache = 0;
	lm.cache_loaded = 0;
	lm_cpu.cycles = 0;
}

static void lm_reset_regs()
{
	UINT32 xcfg = lm.xcfg, cache = lm.cache, cache_loaded = lm.cache_loaded;

	memset(&lm, 0, sizeof(lm));

	lm.xcfg = xcfg; lm.cache = cache; lm.cache_loaded = cache_loaded;

	lm.ctrl = 0x09;
	lm.held = 1;
}

// every reset: the cart and the firmware restart, the 68000 is held until the firmware releases it
static void lm_reset()
{
	lm_idle_forget();
	lm_reset_regs();
	rv32_reset(&lm_cpu);
	syn_reset();
	lm_update_irq();
	lm.boot_pending = 1;
}

// run the mcu alone until it releases the console (at power-on it also unpacks the sound bank);
// done at the start of the first frame, once the nv ram is in place
static void lm_boot_mcu()
{
	if (lm.boot_pending != 1) return;
	lm.boot_pending = 2;
	lm_idle_forget();

	UINT64 limit = lm_cpu.cycles + (UINT64)LM_CLK_HZ * 120;
	while (lm.held && lm_cpu.cycles < limit) {
		lm_run_until(lm_cpu.cycles + 100000);
	}
	if (lm.held) bprintf(PRINT_ERROR, _T("Paprium: the firmware never released the console\n"));

	if (!lm_tracking) {
		lm_cache_save();
		lm_track_start();
	}
}

static void lm_boot()
{
	lm_boot_mcu();
	lm.boot_pending = 0;

	lm.release_pending = 0;
	lm.offset = (INT64)lm_cpu.cycles - (INT64)(SekCyclesDone() * 7);

	SekReset();
}

static void lm_map()
{
	SekOpen(0);
	SekMapHandler(7, 0x000000, 0x00ffff, MAP_READ | MAP_WRITE);
	SekSetReadByteHandler(7, lm_r8);
	SekSetReadWordHandler(7, lm_r16);
	SekSetWriteByteHandler(7, lm_w8);
	SekSetWriteWordHandler(7, lm_w16);
	SekMapMemory(lm_dp,        0x000000, 0x001fff, MAP_FETCH);
	SekMapMemory(RomMain + 0x2000, 0x002000, 0x00ffff, MAP_FETCH);
	SekClose();
}

static INT32 lm_init(INT32 rom_index)
{
	if (lm_load_firmware(rom_index)) return 1;

	lm_irom = (UINT8*)BurnMalloc(0x20000);
	lm_wram = (UINT8*)BurnMalloc(0x10000);
	lm_dp   = (UINT8*)BurnMalloc(0x2000);
	lm_ws   = (UINT8*)BurnMalloc(LM_WS_SIZE);
	lm_cram = (UINT8*)BurnMalloc(LM_CRAM_SIZE);
	lm_xram = (UINT8*)BurnMalloc(LM_XRAM_SIZE);

	lm_track_init(LM_TR_WS, lm_ws, LM_WS_SIZE);
	lm_track_init(LM_TR_CRAM, lm_cram, LM_CRAM_SIZE);
	lm_track_init(LM_TR_XRAM, lm_xram, LM_XRAM_SIZE);
	lm_track_init(LM_TR_FLASH, RomMain, 0x800000);	// the flash's word view is writable

	// the work regions (MCU interface v1.3, 3.8): all the sdram workspace; in cram the echo rings,
	// dac_buf and the megawire text; in the megawire sram the firmware's data
	lm_track_work(LM_TR_WS, 0, LM_WS_SIZE);
	lm_track_work(LM_TR_CRAM, 0x152000, 0x162000);
	lm_track_work(LM_TR_CRAM, 0x170000, 0x172000);
	lm_track_work(LM_TR_CRAM, 0x1b0000, 0x1e0000);
	lm_track_work(LM_TR_XRAM, 0x18000, 0x1e000);

	// same costs as lm_bus_fetch / lm_bus_read
	UINT8 *mem[3] = { lm_irom, lm_wram, lm_xram };
	static const UINT32 base[3] = { 0x00000000, 0x80000000, 0x08000000 }, size[3] = { LM_IROM_SIZE, LM_WRAM_SIZE, LM_XRAM_SIZE };
	static const INT32 fcost[3] = { 2, 2, 17 }, rcost[3] = { 5, 5, 20 };
	for (INT32 i = 0; i < 3; i++) {
		lm_fetch_regions[i].base = lm_read_regions[i].base = base[i];
		lm_fetch_regions[i].size = lm_read_regions[i].size = size[i];
		lm_fetch_regions[i].mem = lm_read_regions[i].mem = mem[i];
		lm_fetch_regions[i].cost = fcost[i];
		lm_read_regions[i].cost = rcost[i];
	}
	memset(lm_irom, 0, 0x20000);
	memset(lm_wram, 0, 0x10000);
	memset(lm_dp, 0, 0x2000);
	memset(lm_ws, 0, LM_WS_SIZE);
	memset(lm_cram, 0, LM_CRAM_SIZE);
	memset(lm_xram, 0, LM_XRAM_SIZE);
	lm_out  = (INT16*)BurnMalloc(SYN_OUT_SIZE * 2 * sizeof(INT16));

	syn_tables_init();
	memset(&syn, 0, sizeof(syn));

	lm_bat = SRam + LM_BAT_OFFSET;
	memset(lm_bat, 0xff, LM_BAT_SIZE);	// a blank eeprom, until the nv ram is loaded

	rv32_init(&lm_cpu, &lm_bus);
	lm_power_on();
	lm_cache_load();
	lm_map();

	if (lm_rd(lm_irom, 0xd64, 4) == 0xdc5ff0ef && lm_rd(lm_irom, 0xd68, 4) == 0xffdff06f) lm_idle_pc = 0xd64;

	return 0;
}

static void lm_exit()
{
	rv32_exit();
	lm_track_exit();
	lm_idle_pc = 0xffffffff;
	lm_idle_cost = 0;
	lm_idle_backoff = lm_idle_wait = 0;
	lm_idle_forget();
	BurnFree(lm_irom);
	BurnFree(lm_wram);
	BurnFree(lm_dp);
	BurnFree(lm_ws);
	BurnFree(lm_cram);
	BurnFree(lm_xram);
	BurnFree(lm_fw);
	BurnFree(lm_out);
	lm_bat = NULL;
}

// called at the start of every line
static void lm_line(INT32 line, INT32 vs_line)
{
	if (line == vs_line) lm_sync(); else lm_sync_lazy();

	if (line == vs_line && !lm.held) {
		lm.vscnt++;
		lm_idle_forget();
	}

	if (lm.release_pending) {
		lm_idle_forget();
		// the console comes out of reset
		lm.release_pending = 0;
		SekReset();
		ZetReset();
	}
}

static inline INT32 lm_held()
{
	return lm.held;
}

static void lm_scan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (nAction & ACB_VOLATILE) {
		if (nAction & ACB_WRITE) lm_idle_forget();

		// the reference for the big memories is the power-on boot: do it now if it is still due
		if (!lm_tracking && lm.boot_pending == 1) lm_boot_mcu();

		SCAN_VAR(lm);
		SCAN_VAR(lm_cpu);
		SCAN_VAR(syn);

		ba.Data = lm_wram; ba.nLen = LM_WRAM_SIZE; ba.nAddress = 0; ba.szName = "lm wram";   BurnAcb(&ba);
		ba.Data = lm_dp;   ba.nLen = 0x2000;       ba.nAddress = 0; ba.szName = "lm ramdp";  BurnAcb(&ba);
		lm_track_scan(nAction);
	}
}
