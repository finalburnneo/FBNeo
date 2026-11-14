// Sega 32X (Mega Drive 32X / Genesis 32X / Super 32X)
// hardware behaviour follows ares, the boot roms are emulated (HLE)

#include "burnint.h"
#include "m68000_intf.h"
#include "sh2_intf.h"
#include "s32x.h"

INT32 S32XMode = 0;
INT32 (*S32XMainCycles)() = NULL;
void (*S32XCartRemap)() = NULL;
void (*S32XCartWrite)(UINT32 address, UINT8 data) = NULL;

#define HBLANK_START	388		// main cpu cycles into the line

#ifdef LSB_FIRST
#define SH2_BADDR(a)	((a) ^ 3)
#define SH2_WADDR(a)	((a) ^ 2)
#else
#define SH2_BADDR(a)	(a)
#define SH2_WADDR(a)	(a)
#endif

static UINT8 *RomMd;		// main cpu rom (68k layout, owned by the md driver)
static UINT8 *RomSh2;		// sh-2 layout copy
static UINT32 RomSize;		// power of two
static UINT8 *Sdram;		// 256kb, sh-2 layout
static UINT8 *Dram;			// 2 x 128kb frame buffers, sh-2 layout
static UINT8 *CacheRam;		// 2 x 64kb (on-chip cache used as ram)
static UINT16 *Cram;
static UINT16 *Pal15;		// 32x colour -> display colour
static INT32 pal_recalc;
static INT32 is_pal;

struct pwm_fifo {
	INT16 d[3];
	INT32 cnt;
	INT16 last;
};

static struct s32x_state {
	UINT32 vector4;
	UINT8 aden, res, fm, rv, dma68, dreq_active, bank, cartmode;
	UINT8 hperiod, hcounter, hen;
	UINT32 dreq_src, dreq_dst;
	UINT16 dreq_len;
	UINT16 fifo[8];
	INT32 fifo_rd, fifo_cnt;
	UINT16 comm[8];

	// per sh-2: bit 0 pwm, 1 cmd, 2 hint, 3 vint
	UINT8 irq_en[2], irq_act[2];
	UINT8 sh2_on;
	UINT8 cd_wait;			// cd mode: boot roms waiting for "_CD_"

	// vdp
	UINT8 mode, lines, priority, dotshift;
	UINT8 l_mode, l_lines, l_priority, l_dotshift;
	UINT8 afill_len;
	UINT16 afill_addr, afill_data;
	UINT8 fb_select, fb_active;
	UINT8 vblank, hb_fired;
	INT32 fill_end;			// sh-2 cycles
	INT32 line_start;		// main cpu cycles

	// pwm
	UINT8 lmode, rmode, mono, dreq_irq, timer;
	UINT16 cycle;
	UINT32 periods;
	INT16 lsample, rsample;
	UINT16 llatch, rlatch, mlatch;
	struct pwm_fifo lfifo, rfifo;
	INT32 pwm_time;			// sh-2 cycles
	float dc_l[2], dc_r[2];	// dc filter state (x, y)

	INT32 sh2_cyc[2];		// sh-2 cycles done in the current frame

	UINT8 boot_pending;		// cartridge programs waiting for the end of the boot roms
	INT32 boot_at;			// sh-2 cycles
	UINT32 boot_vec[4];		// master / slave entry, master / slave vbr
} st;

#define BOOT_TIMEOUT	0x200000	// sh-2 cycles
#define BOOT_DELAY		0x1000

static INT32 run_cpu = -1;
static INT32 run_base = 0;
static INT32 irq_dirty[2];
static INT32 map_dirty[2];

#define PWM_BUF_SIZE	8192
static INT16 *pwm_buf;
static INT32 pwm_pos;

static void map_68k();
static void sh2_map_fb();
static void s32x_boot();
static void s32x_boot_cd();
static void sh2_run_to(INT32 target);

// -----------------------------------------------------------------------------
// helpers

static inline UINT16 rom_word(UINT32 a)
{
	return *((UINT16*)(RomMd + (a & (RomSize - 1) & ~1)));
}

static inline UINT32 rom_long(UINT32 a)
{
	return (rom_word(a) << 16) | rom_word(a + 2);
}

static inline UINT16 *fb_word(INT32 buffer, UINT32 w)
{
	return (UINT16*)(Dram + buffer * 0x20000 + SH2_WADDR((w & 0xffff) << 1));
}

static INT32 sh2_now()
{
	if (run_cpu >= 0) return st.sh2_cyc[run_cpu] + (Sh2TotalCycles() - run_base);
	if (S32XMainCycles) return S32XMainCycles() * 3;
	return 0;
}

static INT32 in_hblank(INT32 now)
{
	return (now / 3 - st.line_start) >= HBLANK_START;
}

static INT32 fb_engaged(INT32 now)
{
	return (st.l_mode != 0 && st.fb_select != st.fb_active) || (now < st.fill_end);
}

// md vdp dram refresh slots, reported in FEN on the sh-2 side (a few times per line, some games sync on them)
static INT32 md_refreshing(INT32 now)
{
	INT32 pos = now - st.line_start * 3;
	if (pos < 0) pos = 0;
	return (pos % 366) < 8;
}

static INT32 palette_engaged(INT32 now)
{
	return !st.vblank && !in_hblank(now) && (st.l_mode & 1);
}

// -----------------------------------------------------------------------------
// sh-2 interrupts

static void sh2_irq_apply(INT32 cpu)
{
	UINT8 m = st.irq_act[cpu] & st.irq_en[cpu];

	Sh2SetIRQLine( 6, (m & 1) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	Sh2SetIRQLine( 8, (m & 2) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	Sh2SetIRQLine(10, (m & 4) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	Sh2SetIRQLine(12, (m & 8) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void sh2_irq_update(INT32 cpu)
{
	if (run_cpu == cpu) {
		sh2_irq_apply(cpu);
	} else if (run_cpu >= 0 || !(st.sh2_on & (1 << cpu))) {
		irq_dirty[cpu] = 1; // applied when the cpu runs next
	} else {
		Sh2Open(cpu);
		sh2_irq_apply(cpu);
		Sh2Close();
	}
}

static void sh2_irq_set(INT32 cpu, UINT8 bit, INT32 state)
{
	UINT8 old = st.irq_act[cpu];

	if (state) st.irq_act[cpu] |= bit; else st.irq_act[cpu] &= ~bit;

	if (old != st.irq_act[cpu]) sh2_irq_update(cpu);
}

// -----------------------------------------------------------------------------
// fifo (68k -> sh-2 dreq)

static void dreq_update()
{
	Sh2SetDreq(0, 0, st.fifo_cnt != 0);
	Sh2SetDreq(1, 0, st.fifo_cnt != 0);
}

static UINT16 fifo_read()
{
	UINT16 d = st.fifo[st.fifo_rd];

	if (st.fifo_cnt) {
		st.fifo_rd = (st.fifo_rd + 1) & 7;
		st.fifo_cnt--;
	}
	dreq_update();

	return d;
}

static void fifo_write(UINT16 d)
{
	if (st.dreq_active && st.fifo_cnt < 8) {
		st.fifo[(st.fifo_rd + st.fifo_cnt) & 7] = d;
		st.fifo_cnt++;
		if (--st.dreq_len == 0) st.dreq_active = 0;
		dreq_update();
	}
}

// -----------------------------------------------------------------------------
// pwm

static INT16 pwm_fifo_read(struct pwm_fifo *f)
{
	if (f->cnt) {
		f->last = f->d[0];
		f->d[0] = f->d[1];
		f->d[1] = f->d[2];
		f->cnt--;
	}
	return f->last;
}

static void pwm_fifo_write(struct pwm_fifo *f, UINT16 v)
{
	if (f->cnt == 3) pwm_fifo_read(f);
	f->d[f->cnt++] = (INT16)((v & 0x800) ? (v & 0xfff) - 0x1000 : (v & 0xfff)); // 12-bit signed
}

static inline float dc_filter(float *s, float x)
{
	float y = x - s[0] + 0.998f * s[1];
	s[0] = x;
	s[1] = y;
	return y;
}

static void pwm_update(INT32 target)
{
	while (1) {
		INT32 clocks = (st.cycle - 1) & 0xfff;
		if (clocks == 0) clocks = 1;
		if (st.pwm_time + clocks > target) break;
		st.pwm_time += clocks;

		if (st.cycle != 1 && (((st.lmode ^ (st.lmode >> 1)) & 1) || ((st.rmode ^ (st.rmode >> 1)) & 1))) {
			st.lsample = pwm_fifo_read(&st.lfifo);
			st.rsample = pwm_fifo_read(&st.rfifo);

			if (st.periods++ == (UINT32)((st.timer - 1) & 0xf)) {
				st.periods = 0;
				sh2_irq_set(0, 1, 1);
				sh2_irq_set(1, 1, 1);
				if (st.dreq_irq) {
					Sh2PulseDreq(0, 1);
					Sh2PulseDreq(1, 1);
				}
			}
		}

		INT32 outl = 0, outr = 0;
		if (st.lmode == 1) outl = st.lsample;
		if (st.lmode == 2) outl = st.rsample;
		if (st.rmode == 1) outr = st.rsample;
		if (st.rmode == 2) outr = st.lsample;

		float l = dc_filter(st.dc_l, (float)outl / clocks);
		float r = dc_filter(st.dc_r, (float)outr / clocks);
		if (l > 1.0f) l = 1.0f; else if (l < -1.0f) l = -1.0f;
		if (r > 1.0f) r = 1.0f; else if (r < -1.0f) r = -1.0f;

		if (pwm_pos < PWM_BUF_SIZE) {
			pwm_buf[pwm_pos * 2 + 0] = (INT16)(l * 32767.0f);
			pwm_buf[pwm_pos * 2 + 1] = (INT16)(r * 32767.0f);
			pwm_pos++;
		}
	}
}

static void pwm_sync()
{
	pwm_update(sh2_now());
}

static UINT16 pwm_read(UINT32 reg)
{
	switch (reg) {
		case 0x00: return st.lmode | (st.rmode << 2) | (st.mono << 4) | (st.dreq_irq << 7) | (st.timer << 8);
		case 0x02: return st.cycle;
		case 0x04: pwm_sync(); return (st.llatch & 0xfff) | ((st.lfifo.cnt == 0) << 14) | ((st.lfifo.cnt == 3) << 15);
		case 0x06: pwm_sync(); return (st.rlatch & 0xfff) | ((st.rfifo.cnt == 0) << 14) | ((st.rfifo.cnt == 3) << 15);
		case 0x08: pwm_sync(); return (st.mlatch & 0xfff) | ((st.lfifo.cnt == 0 && st.rfifo.cnt == 0) << 14) | ((st.lfifo.cnt == 3 || st.rfifo.cnt == 3) << 15);
	}
	return 0;
}

static void pwm_write(UINT32 reg, UINT16 d, INT32 upper, INT32 lower, INT32 sh2)
{
	pwm_sync();

	switch (reg) {
		case 0x00:
			if (lower) {
				st.lmode = d & 3;
				st.rmode = (d >> 2) & 3;
				st.mono = (d >> 4) & 1;
				if (sh2) st.dreq_irq = (d >> 7) & 1;
			}
			if (upper && sh2) {
				st.timer = (d >> 8) & 0xf;
				st.periods = 0;
			}
		break;

		case 0x02:
			if (lower) st.cycle = (st.cycle & 0xf00) | (d & 0xff);
			if (upper) st.cycle = (st.cycle & 0x0ff) | (d & 0xf00);
		break;

		case 0x04:
			if (upper) st.llatch = (st.llatch & 0x0ff) | (d & 0xf00);
			if (lower) { st.llatch = (st.llatch & 0xf00) | (d & 0xff); pwm_fifo_write(&st.lfifo, st.llatch); }
		break;

		case 0x06:
			if (upper) st.rlatch = (st.rlatch & 0x0ff) | (d & 0xf00);
			if (lower) { st.rlatch = (st.rlatch & 0xf00) | (d & 0xff); pwm_fifo_write(&st.rfifo, st.rlatch); }
		break;

		case 0x08:
			if (upper) st.mlatch = (st.mlatch & 0x0ff) | (d & 0xf00);
			if (lower) {
				st.mlatch = (st.mlatch & 0xf00) | (d & 0xff);
				pwm_fifo_write(&st.lfifo, st.mlatch);
				pwm_fifo_write(&st.rfifo, st.mlatch);
			}
		break;
	}
}

// -----------------------------------------------------------------------------
// vdp

static void select_framebuffer(INT32 select)
{
	st.fb_select = select & 1;
	if (!st.vblank && st.l_mode) return; // swapped at the next vblank

	if (st.fb_active != st.fb_select) {
		st.fb_active = st.fb_select;

		if (run_cpu >= 0) {
			sh2_map_fb();
			map_dirty[run_cpu ^ 1] = 1;
		} else {
			for (INT32 i = 0; i < 2; i++) {
				Sh2Open(i);
				sh2_map_fb();
				Sh2Close();
			}
		}
	}
}

static void autofill()
{
	for (INT32 i = 0; i <= st.afill_len; i++) {
		*fb_word(st.fb_active, st.afill_addr) = st.afill_data;
		st.afill_addr = (st.afill_addr & 0xff00) | ((st.afill_addr + 1) & 0xff);
	}
	st.fill_end = sh2_now() + 7 + 3 * (st.afill_len + 1);
}

static UINT16 vdp_read(UINT32 reg)
{
	switch (reg) {
		case 0x00: return st.mode | (st.lines << 6) | (st.priority << 7) | ((!is_pal) << 15);
		case 0x02: return st.dotshift;
		case 0x04: return st.afill_len;
		case 0x06: return st.afill_addr;
		case 0x08: return st.afill_data;
		case 0x0a: {
			INT32 now = sh2_now();
			return st.fb_active | ((fb_engaged(now) || (run_cpu >= 0 && md_refreshing(now))) << 1) | ((!palette_engaged(now)) << 13) | (in_hblank(now) << 14) | (st.vblank << 15);
		}
	}
	return 0;
}

static void vdp_write(UINT32 reg, UINT16 d, INT32 upper, INT32 lower)
{
	switch (reg) {
		case 0x00:
			if (lower) {
				st.mode = d & 3;
				st.lines = (d >> 6) & 1;
				st.priority = (d >> 7) & 1;
			}
		break;

		case 0x02:
			if (lower) st.dotshift = d & 1;
		break;

		case 0x04:
			if (lower) st.afill_len = d & 0xff;
		break;

		case 0x06:
			if (upper) st.afill_addr = (st.afill_addr & 0x00ff) | (d & 0xff00);
			if (lower) st.afill_addr = (st.afill_addr & 0xff00) | (d & 0x00ff);
		break;

		case 0x08:
			if (upper) st.afill_data = (st.afill_data & 0x00ff) | (d & 0xff00);
			if (lower) st.afill_data = (st.afill_data & 0xff00) | (d & 0x00ff);
			autofill();
		break;

		case 0x0a:
			if (lower) select_framebuffer(d & 1);
		break;
	}
}

static void cram_write(UINT32 a, UINT16 d, INT32 upper, INT32 lower)
{
	UINT16 *p = &Cram[(a >> 1) & 0xff];
	if (upper) *p = (*p & 0x00ff) | (d & 0xff00);
	if (lower) *p = (*p & 0xff00) | (d & 0x00ff);
}

// -----------------------------------------------------------------------------
// main cpu (68k) side

static const UINT16 vector_rom_code[0x20] = {
	// $c0: write d0 to (a1) with the rom visible at $000000 (RV set)
	0x08f9, 0x0000, 0x00a1, 0x5107,		// bset    #0, $a15107
	0x1280,								// move.b  d0, (a1)
	0x08b9, 0x0000, 0x00a1, 0x5107,		// bclr    #0, $a15107
	0x4e75,								// rts
	// $d4: copy 8 bytes from (a0) to the $a130f1 bank registers, RV set
	0x48e7, 0x0140,						// movem.l d7/a1, -(sp)
	0x08f9, 0x0000, 0x00a1, 0x5107,		// bset    #0, $a15107
	0x43f9, 0x00a1, 0x30f1,				// lea     $a130f1, a1
	0x7e07,								// moveq   #7, d7
	0x1298,								// move.b  (a0)+, (a1)
	0xd2fc, 0x0002,						// adda.w  #2, a1
	0x51cf, 0xfff8,						// dbra    d7, *-6
	0x08b9, 0x0000, 0x00a1, 0x5107,		// bclr    #0, $a15107
	0x4cdf, 0x0280,						// movem.l (sp)+, d7/a1
	0x4e75								// rts
};

static UINT16 vector_rom_read(UINT32 a)
{
	a &= 0xfe;

	if (a == 0x70) return st.vector4 >> 16;
	if (a == 0x72) return st.vector4 & 0xffff;

	if (a < 0xc0) {
		// vector n (n > 0) jumps into the cartridge table at $880200
		UINT32 v = (a >= 4) ? (0x880200 + ((a >> 2) - 1) * 6) : 0;
		return (a & 2) ? (v & 0xffff) : (v >> 16);
	}

	return vector_rom_code[(a - 0xc0) >> 1];
}

static UINT16 sys_read_68k(UINT32 a)
{
	a &= 0x3fe;

	if (a >= 0x100 && a < 0x140) {
		switch (a & 0x3e) {
			case 0x00: return st.aden | (st.res << 1) | 0x80 | (st.fm << 15);
			case 0x02: return (st.irq_act[0] >> 1 & 1) | (st.irq_act[1] & 2);
			case 0x04: return st.bank;
			case 0x06: return st.rv | (st.dma68 << 1) | (st.dreq_active << 2) | ((st.fifo_cnt == 8) << 7);
			case 0x08: return (st.dreq_src >> 16) & 0xff;
			case 0x0a: return st.dreq_src & 0xffff;
			case 0x0c: return (st.dreq_dst >> 16) & 0xff;
			case 0x0e: return st.dreq_dst & 0xffff;
			case 0x10: return st.dreq_len;
			case 0x1a: return st.cartmode;
			case 0x20: case 0x22: case 0x24: case 0x26:
			case 0x28: case 0x2a: case 0x2c: case 0x2e:
				return st.comm[(a >> 1) & 7];
			case 0x30: case 0x32: case 0x34: case 0x36: case 0x38:
				return pwm_read(a - 0x130);
		}
		return 0;
	}

	if (a >= 0x180 && a < 0x18c) {
		return vdp_read(a - 0x180);
	}

	if (a >= 0x200) {
		if (st.fm) return 0xffff;
		return Cram[(a >> 1) & 0xff];
	}

	return 0;
}

static void sys_write_68k(UINT32 a, UINT16 d, INT32 upper, INT32 lower)
{
	a &= 0x3fe;

	if (a >= 0x100 && a < 0x140) {
		switch (a & 0x3e) {
			case 0x00:
				if (lower) {
					INT32 old_aden = st.aden;
					INT32 old_res = st.res;
					st.aden = d & 1;
					st.res = (d >> 1) & 1;
					if (old_aden != st.aden) map_68k();
					if (st.res == 0) {
						st.sh2_on = 0; // held in reset
						st.boot_pending = 0;
					} else if (old_res == 0) {
						s32x_boot();
					}
				}
				if (upper) st.fm = (d >> 15) & 1;
			return;

			case 0x02:
				if (lower) {
					sh2_irq_set(0, 2, d & 1);
					sh2_irq_set(1, 2, d & 2);
				}
			return;

			case 0x04:
				if (lower && st.bank != (d & 3)) {
					st.bank = d & 3;
					map_68k();
				}
			return;

			case 0x06:
				if (lower) {
					INT32 old_rv = st.rv;
					st.rv = d & 1;
					st.dma68 = (d >> 1) & 1;
					if (st.dreq_active && !(d & 4)) {
						st.fifo_cnt = 0;
						dreq_update();
					}
					st.dreq_active = (d >> 2) & 1;
					if (old_rv != st.rv) map_68k();
				}
			return;

			case 0x08: if (lower) st.dreq_src = (st.dreq_src & 0x00ffff) | ((d & 0xff) << 16); return;
			case 0x0a:
				if (upper) st.dreq_src = (st.dreq_src & 0xff00ff) | (d & 0xff00);
				if (lower) st.dreq_src = (st.dreq_src & 0xffff00) | (d & 0x00ff);
			return;
			case 0x0c: if (lower) st.dreq_dst = (st.dreq_dst & 0x00ffff) | ((d & 0xff) << 16); return;
			case 0x0e:
				if (upper) st.dreq_dst = (st.dreq_dst & 0xff00ff) | (d & 0xff00);
				if (lower) st.dreq_dst = (st.dreq_dst & 0xffff00) | (d & 0x00ff);
			return;
			case 0x10:
				if (upper) st.dreq_len = (st.dreq_len & 0x00ff) | (d & 0xff00);
				if (lower) st.dreq_len = (st.dreq_len & 0xff00) | (d & 0x00fc);
			return;

			case 0x12:
				fifo_write(d);
			return;

			case 0x1a:
				if (lower) st.cartmode = d & 1;
			return;

			case 0x20: case 0x22: case 0x24: case 0x26:
			case 0x28: case 0x2a: case 0x2c: case 0x2e: {
				UINT16 *c = &st.comm[(a >> 1) & 7];
				if (upper) *c = (*c & 0x00ff) | (d & 0xff00);
				if (lower) *c = (*c & 0xff00) | (d & 0x00ff);
				if (st.cd_wait && st.comm[0] == 0x5f43 && st.comm[1] == 0x445f) s32x_boot_cd(); // "_CD_"
				if (st.boot_pending == 1 && (a & 0x3c) == 0x28) {
					st.boot_pending = 2;
					st.boot_at = sh2_now() + BOOT_DELAY;
				}
			}
			return;

			case 0x30: case 0x32: case 0x34: case 0x36: case 0x38:
				pwm_write(a - 0x130, d, upper, lower, 0);
			return;
		}
		return;
	}

	if (st.fm) return; // vdp and palette belong to the sh-2 side

	if (a >= 0x180 && a < 0x18c) {
		vdp_write(a - 0x180, d, upper, lower);
		return;
	}

	if (a >= 0x200) {
		cram_write(a, d, upper, lower);
	}
}

static UINT16 read_68k(UINT32 a)
{
	a &= 0xfffffe;

	if (a < 0x400000) {
		if (st.aden && a < 0x100) return vector_rom_read(a);
		if (!st.aden || st.rv) return *((UINT16*)(RomMd + a));
		return 0xffff;
	}

	if (a >= 0x800000 && a < 0xa00000 && !st.aden) return 0xffff;

	if (a >= 0x840000 && a < 0x880000) {
		if (st.fm) return 0xffff;
		return *fb_word(st.fb_active, a >> 1);
	}

	if ((a & 0xffff00) == 0xa13000) {
		if (a == 0xa130ec) return 0x4d41; // "MA"
		if (a == 0xa130ee) return 0x5253; // "RS"
		return 0xffff;
	}

	if ((a & 0xfffc00) == 0xa15000) {
		return sys_read_68k(a);
	}

	return 0xffff;
}

static void write_68k(UINT32 a, UINT16 d, INT32 upper, INT32 lower)
{
	a &= 0xfffffe;

	if (a < 0x100) {
		if (!st.aden) return;
		if (a == 0x70) {
			if (upper) st.vector4 = (st.vector4 & 0x00ffffff) | ((d & 0xff00) << 16);
			if (lower) st.vector4 = (st.vector4 & 0xff00ffff) | ((d & 0x00ff) << 16);
		}
		if (a == 0x72) {
			if (upper) st.vector4 = (st.vector4 & 0xffff00ff) | (d & 0xff00);
			if (lower) st.vector4 = (st.vector4 & 0xffffff00) | (d & 0x00ff);
		}
		return;
	}

	if (a >= 0x840000 && a < 0x880000) {
		if (st.fm || !st.aden) return;
		UINT16 *p = fb_word(st.fb_active, a >> 1);
		if (a < 0x860000) {
			if (d == 0 && (!upper || !lower)) return; // byte writes of 0 are ignored
			if (upper) *p = (*p & 0x00ff) | (d & 0xff00);
			if (lower) *p = (*p & 0xff00) | (d & 0x00ff);
		} else {
			if (upper && (d & 0xff00)) *p = (*p & 0x00ff) | (d & 0xff00);
			if (lower && (d & 0x00ff)) *p = (*p & 0xff00) | (d & 0x00ff);
		}
		return;
	}

	if ((a & 0xfffc00) == 0xa15000) {
		sys_write_68k(a, d, upper, lower);
		return;
	}

	if ((a & 0xffff00) == 0xa13000) {
		if (S32XCartWrite && lower) S32XCartWrite(a | 1, d & 0xff); // sram control
		return;
	}
}

static UINT8 __fastcall s32x_68k_read_byte(UINT32 a)
{
	UINT16 d = read_68k(a);
	return (a & 1) ? (d & 0xff) : (d >> 8);
}

static UINT16 __fastcall s32x_68k_read_word(UINT32 a)
{
	return read_68k(a);
}

static void __fastcall s32x_68k_write_byte(UINT32 a, UINT8 d)
{
	write_68k(a, d | (d << 8), !(a & 1), a & 1);
}

static void __fastcall s32x_68k_write_word(UINT32 a, UINT16 d)
{
	write_68k(a, d, 1, 1);
}

static void map_68k()
{
	if (RomMd == NULL) {
		SekMapHandler(3, 0x800000, 0x9fffff, MAP_RAM); // cd mode: no cartridge, no vector rom
		return;
	}

	if (!st.aden) {
		SekMapMemory(RomMd, 0x000000, 0x3fffff, MAP_ROM);
		SekMapHandler(3, 0x800000, 0x9fffff, MAP_RAM);
		if (S32XCartRemap) S32XCartRemap(); // sram, eeprom
		return;
	}

	SekMapHandler(3, 0x000000, 0x0003ff, MAP_RAM); // vector rom
	SekMapHandler(3, 0x800000, 0x87ffff, MAP_RAM); // frame buffer

	if (st.rv) {
		// rom at its normal location (md vdp dma), not visible in the 32x area
		SekMapMemory(RomMd + 0x400, 0x000400, 0x3fffff, MAP_ROM);
		SekMapHandler(3, 0x880000, 0x9fffff, MAP_RAM);
		if (S32XCartRemap) S32XCartRemap();
	} else {
		SekMapHandler(3, 0x000400, 0x3fffff, MAP_RAM);
		for (UINT32 i = 0; i < 0x80000; i += RomSize) {
			SekMapMemory(RomMd, 0x880000 + i, 0x880000 + i + ((RomSize < 0x80000) ? RomSize : 0x80000) - 1, MAP_ROM);
		}
		SekMapMemory(RomMd + st.bank * 0x100000, 0x900000, 0x9fffff, MAP_ROM);
	}
}

// -----------------------------------------------------------------------------
// sh-2 side

static UINT16 sys_read_sh2(UINT32 a)
{
	INT32 cpu = run_cpu & 1;

	a &= 0x3fe;

	if (a < 0x100) {
		switch (a) {
			case 0x00: return st.irq_en[cpu] | (st.hen << 7) | ((S32XMode == 2) << 8) | (st.aden << 9) | (st.fm << 15);
			case 0x04: return st.hperiod;
			case 0x06: return st.rv | (st.dma68 << 1) | (st.dreq_active << 2) | ((st.fifo_cnt == 0) << 14) | ((st.fifo_cnt == 8) << 15);
			case 0x08: return (st.dreq_src >> 16) & 0xff;
			case 0x0a: return st.dreq_src & 0xffff;
			case 0x0c: return (st.dreq_dst >> 16) & 0xff;
			case 0x0e: return st.dreq_dst & 0xffff;
			case 0x10: return st.dreq_len;
			case 0x12: return fifo_read();
			case 0x20: case 0x22: case 0x24: case 0x26:
			case 0x28: case 0x2a: case 0x2c: case 0x2e:
				return st.comm[(a >> 1) & 7];
			case 0x30: case 0x32: case 0x34: case 0x36: case 0x38:
				return pwm_read(a - 0x30);
		}
		return 0;
	}

	if (a >= 0x100 && a < 0x10c) {
		return vdp_read(a - 0x100);
	}

	if (a >= 0x200) {
		return Cram[(a >> 1) & 0xff];
	}

	return 0;
}

static void sys_write_sh2(UINT32 a, UINT16 d, INT32 upper, INT32 lower)
{
	INT32 cpu = run_cpu & 1;

	a &= 0x3fe;

	if (a < 0x100) {
		switch (a) {
			case 0x00:
				if (lower) {
					if (st.irq_en[cpu] != (d & 0xf)) {
						st.irq_en[cpu] = d & 0xf;
						sh2_irq_update(cpu);
					}
					st.hen = (d >> 7) & 1;
				}
				if (upper) st.fm = (d >> 15) & 1;
			return;

			case 0x04:
				if (lower) {
					st.hperiod = d & 0xff;
					st.hcounter = 0;
				}
			return;

			case 0x14: return; // vres clear
			case 0x16: sh2_irq_set(cpu, 8, 0); return;
			case 0x18: sh2_irq_set(cpu, 4, 0); return;
			case 0x1a: sh2_irq_set(cpu, 2, 0); return;
			case 0x1c: sh2_irq_set(cpu, 1, 0); return;

			case 0x20: case 0x22: case 0x24: case 0x26:
			case 0x28: case 0x2a: case 0x2c: case 0x2e: {
				UINT16 *c = &st.comm[(a >> 1) & 7];
				if (upper) *c = (*c & 0x00ff) | (d & 0xff00);
				if (lower) *c = (*c & 0xff00) | (d & 0x00ff);
			}
			return;

			case 0x30: case 0x32: case 0x34: case 0x36: case 0x38:
				pwm_write(a - 0x30, d, upper, lower, 1);
			return;
		}
		return;
	}

	if (!st.fm) return; // vdp and palette belong to the 68k side

	if (a >= 0x100 && a < 0x10c) {
		vdp_write(a - 0x100, d, upper, lower);
		return;
	}

	if (a >= 0x200) {
		cram_write(a, d, upper, lower);
	}
}

static UINT16 read_sh2(UINT32 a)
{
	a &= 0x07fffffe;

	if (a >= 0x4000 && a < 0x4400) return sys_read_sh2(a);

	return 0; // boot rom (hle) and open bus
}

static void write_sh2(UINT32 a, UINT16 d, INT32 upper, INT32 lower)
{
	a &= 0x07fffffe;

	if (a >= 0x4000 && a < 0x4400) sys_write_sh2(a, d, upper, lower);
}

static UINT8 __fastcall s32x_sh2_read_byte(UINT32 a)
{
	UINT16 d = read_sh2(a);
	return (a & 1) ? (d & 0xff) : (d >> 8);
}

static UINT16 __fastcall s32x_sh2_read_word(UINT32 a)
{
	return read_sh2(a);
}

static UINT32 __fastcall s32x_sh2_read_long(UINT32 a)
{
	return (read_sh2(a) << 16) | read_sh2(a + 2);
}

static void __fastcall s32x_sh2_write_byte(UINT32 a, UINT8 d)
{
	write_sh2(a, d | (d << 8), !(a & 1), a & 1);
}

static void __fastcall s32x_sh2_write_word(UINT32 a, UINT16 d)
{
	write_sh2(a, d, 1, 1);
}

static void __fastcall s32x_sh2_write_long(UINT32 a, UINT32 d)
{
	write_sh2(a + 0, d >> 16, 1, 1);
	write_sh2(a + 2, d & 0xffff, 1, 1);
}

// frame buffer writes (reads are mapped directly)
static void fb_write_sh2(UINT32 a, UINT16 d, INT32 upper, INT32 lower)
{
	if (!st.fm) return;

	UINT16 *p = fb_word(st.fb_active, (a >> 1) & 0xffff);

	if ((a & 0x20000) == 0) {
		if (d == 0 && (!upper || !lower)) return; // byte writes of 0 are ignored
		if (upper) *p = (*p & 0x00ff) | (d & 0xff00);
		if (lower) *p = (*p & 0xff00) | (d & 0x00ff);
	} else {
		if (upper && (d & 0xff00)) *p = (*p & 0x00ff) | (d & 0xff00);
		if (lower && (d & 0x00ff)) *p = (*p & 0xff00) | (d & 0x00ff);
	}
}

static UINT8 __fastcall s32x_fb_read_byte(UINT32 a)
{
	UINT16 d = *fb_word(st.fb_active, (a >> 1) & 0xffff);
	return (a & 1) ? (d & 0xff) : (d >> 8);
}

static UINT16 __fastcall s32x_fb_read_word(UINT32 a)
{
	return *fb_word(st.fb_active, (a >> 1) & 0xffff);
}

static UINT32 __fastcall s32x_fb_read_long(UINT32 a)
{
	return (*fb_word(st.fb_active, (a >> 1) & 0xffff) << 16) | *fb_word(st.fb_active, ((a >> 1) + 1) & 0xffff);
}

static void __fastcall s32x_fb_write_byte(UINT32 a, UINT8 d)
{
	fb_write_sh2(a, d | (d << 8), !(a & 1), a & 1);
}

static void __fastcall s32x_fb_write_word(UINT32 a, UINT16 d)
{
	fb_write_sh2(a, d, 1, 1);
}

static void __fastcall s32x_fb_write_long(UINT32 a, UINT32 d)
{
	fb_write_sh2(a + 0, d >> 16, 1, 1);
	fb_write_sh2(a + 2, d & 0xffff, 1, 1);
}

// frame buffer mapping for the currently open sh-2
static void sh2_map_fb()
{
	UINT8 *fb = Dram + st.fb_active * 0x20000;

	for (UINT32 a = 0x04000000; a < 0x06000000; a += 0x40000) {
		Sh2MapMemory(fb, a + 0x00000, a + 0x1ffff, MAP_ROM);
		Sh2MapMemory(fb, a + 0x20000, a + 0x3ffff, MAP_ROM);
		Sh2MapHandler(1, a, a + 0x3ffff, MAP_WRITE);
	}
}

static void sh2_map(INT32 cpu)
{
	Sh2MapHandler(0, 0x00000000, 0x07ffffff, MAP_RAM);
	Sh2MapHandler(0, 0xc0000000, 0xdfffffff, MAP_RAM);

	if (RomSh2) {
		for (UINT32 a = 0x02000000; a < 0x04000000; a += RomSize) {
			Sh2MapMemory(RomSh2, a, a + RomSize - 1, MAP_ROM);
		}
	}

	sh2_map_fb();

	Sh2MapMemory(Sdram, 0x06000000, 0x0603ffff, MAP_RAM);
	Sh2MapMemory(CacheRam + cpu * 0x10000, 0xc0000000, 0xc000ffff, MAP_RAM);

	Sh2SetReadByteHandler (0, s32x_sh2_read_byte);
	Sh2SetReadWordHandler (0, s32x_sh2_read_word);
	Sh2SetReadLongHandler (0, s32x_sh2_read_long);
	Sh2SetWriteByteHandler(0, s32x_sh2_write_byte);
	Sh2SetWriteWordHandler(0, s32x_sh2_write_word);
	Sh2SetWriteLongHandler(0, s32x_sh2_write_long);

	Sh2SetReadByteHandler (1, s32x_fb_read_byte);
	Sh2SetReadWordHandler (1, s32x_fb_read_word);
	Sh2SetReadLongHandler (1, s32x_fb_read_long);
	Sh2SetWriteByteHandler(1, s32x_fb_write_byte);
	Sh2SetWriteWordHandler(1, s32x_fb_write_word);
	Sh2SetWriteLongHandler(1, s32x_fb_write_long);
}

// -----------------------------------------------------------------------------
// boot rom hle
//
// both boot roms wait for the adapter to be enabled, set up the bus state controller,
// clear the sdram, and for a cartridge:
//  master: checks the security code, sums the rom words from $200 to the end address
//          at $1a4 (when the checksum at $18e is not 0) and writes the result to comm 4,
//          copies the program described at $3d4 (rom offset, sdram offset, length),
//          then jumps to the entry point at $3e0 with vbr = $3e8, comm 0 = "M_OK"
//  slave:  waits for "M_OK", then jumps to $3e4 with vbr = $3ec, comm 2 = "S_OK"
// for a cd, both wait for "_CD_" in comm 0, the master takes the frame buffer (FM),
// and the program is described at frame buffer offset $18: sdram address, length,
// master entry, slave entry, master vbr, slave vbr, followed by the data at $38

static void sh2_start(INT32 now, INT32 pc_m, INT32 pc_s, INT32 vbr_m, INT32 vbr_s)
{

	for (INT32 i = 0; i < 2; i++) {
		Sh2Open(i);
		Sh2Reset(i ? pc_s : pc_m, (i == 0) ? 0x06040000 : 0x0603f800);
		Sh2SetVBR(i ? vbr_s : vbr_m);
		Sh2SetGBR(0x20004000);
		Sh2Close();

		st.irq_en[i] = 0;
		st.irq_act[i] = 0;
		irq_dirty[i] = 0;
		if (st.sh2_cyc[i] < now) st.sh2_cyc[i] = now;
	}

	st.comm[0] = 0x4d5f; st.comm[1] = 0x4f4b; // "M_OK"
	st.comm[2] = 0x535f; st.comm[3] = 0x4f4b; // "S_OK"

	st.sh2_on = 3;

	// the boot roms take a long time, the programs are already running when the 68k sees "M_OK" / "S_OK"
	if (run_cpu < 0) sh2_run_to(now + 4096);
}

static UINT32 fb_long(UINT32 a)
{
	return (*fb_word(st.fb_active, a >> 1) << 16) | *fb_word(st.fb_active, (a >> 1) + 1);
}

static void s32x_boot_cd()
{
	st.cd_wait = 0;
	st.fm = 1;

	UINT32 dst = fb_long(0x18);
	UINT32 len = fb_long(0x1c);
	if (len > 0x1ffe0) len = 0x1ffe0;

	for (UINT32 i = 0; i < len; i += 4, dst += 4) {
		if ((dst & 0x07fc0000) == 0x06000000) {
			*((UINT32*)(Sdram + (dst & 0x3fffc))) = fb_long(0x38 + i);
		}
	}

	sh2_start(sh2_now(), fb_long(0x20), fb_long(0x24), fb_long(0x28), fb_long(0x2c));
}

static void s32x_boot()
{
	if (!st.aden) {
		st.sh2_on = 0; // the boot roms put themselves into standby
		return;
	}

	memset(Sdram, 0, 0x40000);

	if (RomMd == NULL) {
		st.sh2_on = 0;
		st.cd_wait = 1;
		if (st.comm[0] == 0x5f43 && st.comm[1] == 0x445f) s32x_boot_cd();
		return;
	}

	UINT16 sum = 0;
	if (rom_word(0x18e)) {
		UINT32 count = (((rom_long(0x1a4) - 0x200) >> 1) & 0x3fffff) + 1;
		for (UINT32 i = 0, a = 0x200; i < count; i++, a += 2) {
			sum += rom_word(a);
		}
	}
	st.comm[4] = sum;

	UINT32 src = rom_long(0x3d4);
	UINT32 dst = rom_long(0x3d8) & 0x3fffc;
	UINT32 len = (rom_long(0x3dc) + 3) & ~3;
	for (UINT32 i = 0; i < len && dst < 0x40000; i += 4, dst += 4) {
		*((UINT32*)(Sdram + dst)) = rom_long(src + i);
	}

	// the boot roms start the programs once the 68k security code has checked the
	// checksum (it clears comm4 / comm6), the 68k sets up the vdp and clears FM before
	// that (motocross championship sets FM very early)
	for (INT32 i = 0; i < 4; i++) st.boot_vec[i] = rom_long(0x3e0 + i * 4);
	st.boot_pending = 1;
	st.boot_at = sh2_now() + BOOT_TIMEOUT;
	st.sh2_on = 0;
}

// -----------------------------------------------------------------------------
// timing

static void sh2_run_cpus(INT32 target)
{
	// the slave runs first: when both poll the same comm register and the master
	// clears it, the slave still gets to see it (fifa soccer 96)
	for (INT32 i = 1; i >= 0; i--) {
		INT32 cyc = target - st.sh2_cyc[i];
		if (cyc <= 0) continue;

		if (!(st.sh2_on & (1 << i))) {
			st.sh2_cyc[i] = target;
			continue;
		}

		Sh2Open(i);
		if (irq_dirty[i]) { irq_dirty[i] = 0; sh2_irq_apply(i); }
		if (map_dirty[i]) { map_dirty[i] = 0; sh2_map_fb(); }
		run_cpu = i;
		run_base = Sh2TotalCycles();
		Sh2Run(cyc);
		st.sh2_cyc[i] += Sh2TotalCycles() - run_base;
		run_cpu = -1;
		Sh2Close();
	}
}

static void sh2_run_to(INT32 target)
{
	if (st.boot_pending && target >= st.boot_at) {
		sh2_run_cpus(st.boot_at);
		st.boot_pending = 0;
		sh2_start(st.boot_at, st.boot_vec[0], st.boot_vec[1], st.boot_vec[2], st.boot_vec[3]);
	}

	sh2_run_cpus(target);

	pwm_update(target);
}

static void hblank_start()
{
	if (!st.vblank || st.hen) {
		if (st.hcounter++ >= st.hperiod) {
			st.hcounter = 0;
			sh2_irq_set(0, 4, 1);
			sh2_irq_set(1, 4, 1);
		}
	}
}

void S32XRunSh2(INT32 main_cycles)
{
	if (!st.hb_fired && main_cycles >= st.line_start + HBLANK_START) {
		sh2_run_to((st.line_start + HBLANK_START) * 3);
		hblank_start();
		st.hb_fired = 1;
	}

	sh2_run_to(main_cycles * 3);
}

void S32XLineStart(INT32 line, INT32 lines_vis, INT32 main_cycles)
{
	S32XRunSh2(main_cycles);

	// end of hblank
	sh2_irq_set(0, 4, 0);
	sh2_irq_set(1, 4, 0);
	st.l_mode = st.mode;
	st.l_lines = st.lines;
	st.l_priority = st.priority;
	st.l_dotshift = st.dotshift;

	if (line == 0) {
		st.vblank = 0;
		sh2_irq_set(0, 8, 0);
		sh2_irq_set(1, 8, 0);
	}

	if (line == lines_vis) {
		st.vblank = 1;
		sh2_irq_set(0, 8, 1);
		sh2_irq_set(1, 8, 1);
		select_framebuffer(st.fb_select);
	}

	st.line_start = main_cycles;
	st.hb_fired = 0;
}

void S32XEndFrame(INT32 main_cycles)
{
	S32XRunSh2(main_cycles);

	INT32 sh2_cycles = main_cycles * 3;

	for (INT32 i = 0; i < 2; i++) {
		st.sh2_cyc[i] -= sh2_cycles;
		Sh2Open(i);
		Sh2NewFrame(); // keeps the run counters small
		Sh2Close();
	}
	st.pwm_time -= sh2_cycles;
	st.fill_end -= sh2_cycles;
	if (st.boot_pending) st.boot_at -= sh2_cycles;
	if (st.fill_end < -0x100000) st.fill_end = -0x100000;
	st.line_start -= main_cycles;
}

// -----------------------------------------------------------------------------
// video

void S32XRecalcPal()
{
	pal_recalc = 1;
}

static void recalc_pal()
{
	for (INT32 i = 0; i < 0x8000; i++) {
		INT32 r = (i >>  0) & 0x1f;
		INT32 g = (i >>  5) & 0x1f;
		INT32 b = (i >> 10) & 0x1f;
		Pal15[i] = BurnHighCol((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2), 0);
	}
	pal_recalc = 0;
}

void S32XDrawLine(INT32 line, UINT8 *md_line, UINT16 *dest, UINT32 *md_pal, INT32 backdrop, INT32 h40)
{
	if (pal_recalc) recalc_pal();

	UINT8 md[320];
	for (INT32 x = 0; x < 320; x++) {
		md[x] = md_line[h40 ? x : (x * 4 / 5)];
	}

	if (st.l_mode == 0 || line >= (st.l_lines ? 240 : 224)) {
		for (INT32 x = 0; x < 320; x++) dest[x] = md_pal[md[x]];
		return;
	}

	UINT16 px[320];
	INT32 buf = st.fb_active ^ 1; // displayed frame buffer
	UINT16 addr = *fb_word(buf, line);

	switch (st.l_mode) {
		case 1:
			for (INT32 x = 0; x < 320; x++) {
				INT32 p = x + st.l_dotshift;
				UINT16 w = *fb_word(buf, addr + (p >> 1));
				px[x] = Cram[(p & 1) ? (w & 0xff) : (w >> 8)];
			}
		break;

		case 2:
			for (INT32 x = 0; x < 320; x++) {
				px[x] = *fb_word(buf, addr++);
			}
		break;

		case 3:
			for (INT32 x = 0; x < 320; ) {
				UINT16 w = *fb_word(buf, addr++);
				UINT16 c = Cram[w & 0xff];
				for (INT32 n = (w >> 8) + 1; n > 0 && x < 320; n--) px[x++] = c;
			}
		break;
	}

	for (INT32 x = 0; x < 320; x++) {
		INT32 through = px[x] >> 15;
		INT32 is_backdrop = (md[x] & 0x3f) == backdrop;
		INT32 show = (st.l_priority) ? (!through || is_backdrop) : (through || is_backdrop);
		dest[x] = show ? Pal15[px[x] & 0x7fff] : md_pal[md[x]];
	}
}

// -----------------------------------------------------------------------------
// sound

void S32XSoundUpdate(INT16 *output, INT32 samples)
{
	if (output && pwm_pos > 0) {
		for (INT32 i = 0; i < samples; i++) {
			INT32 s = (INT32)((INT64)i * pwm_pos / samples);
			INT32 l = output[i * 2 + 0] + pwm_buf[s * 2 + 0];
			INT32 r = output[i * 2 + 1] + pwm_buf[s * 2 + 1];
			output[i * 2 + 0] = BURN_SND_CLIP(l);
			output[i * 2 + 1] = BURN_SND_CLIP(r);
		}
	}

	pwm_pos = 0;
}

// -----------------------------------------------------------------------------
// init / exit

void S32XReset()
{
	memset(&st, 0, sizeof(st));
	st.res = 1; // the sh-2s are running their boot roms (standby until reset)
	st.vblank = 1;

	memset(Sdram, 0, 0x40000);
	memset(Dram, 0, 0x40000);
	memset(CacheRam, 0, 0x20000);
	memset(Cram, 0, 0x200);

	run_cpu = -1;
	irq_dirty[0] = irq_dirty[1] = 0;
	map_dirty[0] = map_dirty[1] = 0;
	pwm_pos = 0;

	for (INT32 i = 0; i < 2; i++) {
		Sh2Open(i);
		Sh2Reset(0, 0);
		sh2_map_fb();
		Sh2Close();
		Sh2SetDreq(i, 0, 0);
		Sh2SetDreq(i, 1, 0);
	}

	SekOpen(0);
	map_68k();
	SekClose();
}

void S32XSetPAL(INT32 pal)
{
	is_pal = pal ? 1 : 0;
}

INT32 S32XInit(UINT8 *rom, INT32 rom_size)
{
	S32XMode = (rom) ? 1 : 2; // 2: sega cd 32x (no cartridge)

	RomMd = rom;
	RomSize = 1;
	while (RomSize < (UINT32)rom_size) RomSize <<= 1;
	if (RomSize < 0x80000) RomSize = 0x80000;

	RomSh2   = (rom) ? (UINT8*)BurnMalloc(RomSize) : NULL;
	Sdram    = (UINT8*)BurnMalloc(0x40000);
	Dram     = (UINT8*)BurnMalloc(0x40000);
	CacheRam = (UINT8*)BurnMalloc(0x20000);
	Cram     = (UINT16*)BurnMalloc(0x200);
	Pal15    = (UINT16*)BurnMalloc(0x8000 * sizeof(UINT16));
	pwm_buf  = (INT16*)BurnMalloc(PWM_BUF_SIZE * 2 * sizeof(INT16));

	// sh-2 layout rom copy (mirrored up to the next power of two)
	for (UINT32 a = 0; rom && a < RomSize; a += 2) {
		*((UINT16*)(RomSh2 + SH2_WADDR(a))) = *((UINT16*)(RomMd + (a % rom_size)));
	}

	SekOpen(0);
	SekMapHandler(3, 0xa13000, 0xa133ff, MAP_RAM);
	SekMapHandler(3, 0xa15000, 0xa153ff, MAP_RAM);
	SekSetReadByteHandler (3, s32x_68k_read_byte);
	SekSetReadWordHandler (3, s32x_68k_read_word);
	SekSetWriteByteHandler(3, s32x_68k_write_byte);
	SekSetWriteWordHandler(3, s32x_68k_write_word);
	SekClose();

	Sh2Init(2);
	Sh2SetSciLink(1);
	for (INT32 i = 0; i < 2; i++) {
		Sh2Open(i);
		sh2_map(i);
		Sh2Close();
	}

	pal_recalc = 1;

	S32XReset();

	return 0;
}

void S32XExit()
{
	Sh2Exit();

	BurnFree(RomSh2);
	BurnFree(Sdram);
	BurnFree(Dram);
	BurnFree(CacheRam);
	BurnFree(Cram);
	BurnFree(Pal15);
	BurnFree(pwm_buf);

	RomMd = NULL;
	S32XMode = 0;
	S32XMainCycles = NULL;
	S32XCartRemap = NULL;
	S32XCartWrite = NULL;
}

INT32 S32XScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(Sdram, 0x40000, "32X SDRAM");
		ScanVar(Dram, 0x40000, "32X Frame Buffer");
		ScanVar(CacheRam, 0x20000, "32X SH-2 Cache");
		ScanVar(Cram, 0x200, "32X Palette");
	}

	if (nAction & ACB_DRIVER_DATA) {
		Sh2Scan(nAction);

		SCAN_VAR(st);
		SCAN_VAR(irq_dirty);
		SCAN_VAR(map_dirty);
	}

	if (nAction & ACB_WRITE) {
		SekOpen(0);
		map_68k();
		SekClose();

		for (INT32 i = 0; i < 2; i++) {
			Sh2Open(i);
			sh2_map_fb();
			Sh2Close();
		}
		map_dirty[0] = map_dirty[1] = 0;
		pwm_pos = 0;
	}

	return 0;
}
