// FinalBurn Neo - Hitachi H8/3002 emulation
// note: no dma, wd, rtc (if needed, will add -dink)

#include "burnint.h"
#include "h8_intf.h"

#define F_I		0x80
#define F_UI	0x40
#define F_H		0x20
#define F_U		0x10
#define F_N		0x08
#define F_Z		0x04
#define F_V		0x02
#define F_C		0x01

#define ADDR_MASK	0xffffff

struct h8_timer {
	UINT8  tcr, tior, tier, tsr;
	UINT16 tcnt, gra, grb, bra, brb;
	UINT8  tsr_read;			// flags seen set by a TSR read (for write-0 clearing)
	UINT64 last;				// absolute cycle the count was last synced
	UINT64 event;				// absolute cycle of the next event
};

struct h8_sci {
	UINT8 smr, brr, scr, tdr, ssr, rdr;
	UINT8 ssr_read;
};

struct h8_state {
	UINT32 er[8];
	UINT32 pc;
	UINT8  ccr;
	UINT8  sleeping;

	// interrupt controller
	UINT8  syscr;
	UINT8  iscr, ier, isr, isr_read;
	UINT8  ipra, iprb;
	UINT8  irq_level;			// current state of IRQ0-5 pins
	UINT8  nmi_pending;

	// bus / misc stubs
	UINT8  abwcr, astcr, wcr, brcr;
	UINT8  rtmcsr, rtcnt, rtcor;
	UINT8  wd_tcsr, wd_tcnt;
	UINT8  dma_regs[0x20];

	// ports
	UINT8  ddr[12];
	UINT8  dr[12];
	UINT8  p4pcr;

	// ITU
	UINT8  tstr, tsnc, tmdr, tfcr, toer, tocr;
	h8_timer tmr[5];

	// SCI
	h8_sci sci[2];

	// ADC
	UINT8  adcsr, adcr;
	UINT16 addr[4];
	UINT64 adc_event;

	UINT64 total_cycles;
	UINT64 frame_start;
	UINT8  onchip_ram[0x200];
};

static INT32 irq_dirty = 1;
static h8_state h8;

static UINT8 *mem_read[256];
static UINT8 *mem_write[256];

static UINT8  (*pReadByte)(UINT32) = NULL;
static UINT16 (*pReadWord)(UINT32) = NULL;
static void   (*pWriteByte)(UINT32, UINT8) = NULL;
static void   (*pWriteWord)(UINT32, UINT16) = NULL;
static UINT8  (*pReadPort)(INT32) = NULL;
static void   (*pWritePort)(INT32, UINT8) = NULL;
static UINT16 (*pAdcRead)(INT32) = NULL;
static void   (*pSciTx)(INT32, UINT8) = NULL;
static INT32  (*pSciRx)(INT32) = NULL;

// idle loop detection: a short backward loop that writes
// nothing and leaves every register and the flags unchanged between passes
// is waiting for an interrupt, so it is fast-forwarded to the next event.
static UINT32 idle_pc = ~0U;
static INT32  idle_valid = 0;
static INT32  idle_count = 0;
static UINT32 idle_writes = 0;
static UINT32 idle_inst = 0;
static UINT32 idle_regs[9];
static UINT32 mem_writes = 0;
static UINT32 cur_inst_pc = 0;

static inline void idle_branch(UINT32 target)
{
	if (target <= cur_inst_pc && (cur_inst_pc - target) <= 128 && target != idle_pc) {
		idle_pc = target;
		idle_valid = 0;
		idle_count = 0;
	}
}

static INT32  nCyclesToDo = 0;
static INT32  nCyclesLeft = 0;
static INT32  bEndRun = 0;
static INT32  inst_cycles = 0;
static UINT64 next_event = ~(UINT64)0;

static void update_events();
static void timers_sync(UINT64 now);

// registers
static inline UINT8 r8(INT32 n)
{
	return (n & 8) ? (h8.er[n & 7] & 0xff) : ((h8.er[n & 7] >> 8) & 0xff);
}

static inline void w8(INT32 n, UINT8 v)
{
	if (n & 8) h8.er[n & 7] = (h8.er[n & 7] & 0xffffff00) | v;
	else       h8.er[n & 7] = (h8.er[n & 7] & 0xffff00ff) | (v << 8);
}

static inline UINT16 r16(INT32 n)
{
	return (n & 8) ? (h8.er[n & 7] >> 16) : (h8.er[n & 7] & 0xffff);
}

static inline void w16(INT32 n, UINT16 v)
{
	if (n & 8) h8.er[n & 7] = (h8.er[n & 7] & 0x0000ffff) | ((UINT32)v << 16);
	else       h8.er[n & 7] = (h8.er[n & 7] & 0xffff0000) | v;
}

static inline UINT32 r32(INT32 n) { return h8.er[n & 7]; }
static inline void w32(INT32 n, UINT32 v) { h8.er[n & 7] = v; }

// on-chip I/O
static UINT64 cur_cycle() { return h8.total_cycles + inst_cycles; }

static INT32 timer_prescale(h8_timer *t)
{
	switch (t->tcr & 7) {
		case 0: return 1;
		case 1: return 2;
		case 2: return 4;
		case 3: return 8;
	}
	return 0; // external clock: not connected
}

// raise flags for one timer channel; returns 1 if anything happened
static void timer_advance(INT32 ch, UINT64 now)
{
	irq_dirty = 1;
	h8_timer *t = &h8.tmr[ch];
	INT32 div = timer_prescale(t);

	if (!(h8.tstr & (1 << ch)) || div == 0) {
		t->last = now;
		return;
	}

	UINT64 ticks = (now - t->last) / div;
	t->last += ticks * div;

	INT32 cclr = (t->tcr >> 5) & 3;

	while (ticks > 0)
	{
		UINT32 c = t->tcnt;
		// ticks to the next interesting point
		UINT32 da = ((UINT32)(t->gra - c - 1) & 0xffff) + 1;   // to reach GRA
		UINT32 db = ((UINT32)(t->grb - c - 1) & 0xffff) + 1;   // to reach GRB
		UINT32 dov = 0x10000 - c;                               // to wrap to 0

		UINT32 step = da;
		if (db < step) step = db;
		if (dov < step) step = dov;

		// clearing happens on the tick after a match
		if (cclr == 1 && c == t->gra) step = 1;
		if (cclr == 2 && c == t->grb) step = 1;

		if (ticks < step) {
			t->tcnt = (UINT16)(c + ticks);
			break;
		}

		ticks -= step;

		if ((cclr == 1 && c == t->gra) || (cclr == 2 && c == t->grb)) {
			t->tcnt = 0;
		} else {
			UINT32 n = c + step;
			if (n >= 0x10000) { n &= 0xffff; t->tsr |= 4; }
			t->tcnt = (UINT16)n;
		}

		if (t->tcnt == t->gra) t->tsr |= 1;
		if (t->tcnt == t->grb) t->tsr |= 2;
	}
}

// ticks from count c until the counter shows 'target', given that it is
// cleared to 0 after reaching (period - 1); ~0 if never reached
static UINT32 ticks_until(UINT32 c, UINT32 target, UINT32 period)
{
	if (c < period) {
		if (target >= period) return 0xffffffff;
		if (target > c) return target - c;
		return (period - c) + target;
	}

	// counting beyond the clear point: runs up to 0xffff and wraps
	if (target > c) return target - c;
	if (target >= period) return 0xffffffff;
	return (0x10000 - c) + target;
}

static UINT64 timer_next_event(INT32 ch)
{
	h8_timer *t = &h8.tmr[ch];
	INT32 div = timer_prescale(t);

	if (!(h8.tstr & (1 << ch)) || div == 0 || (t->tier & 7) == 0)
		return ~(UINT64)0;

	UINT32 c = t->tcnt;
	INT32 cclr = (t->tcr >> 5) & 3;
	UINT32 period = 0x10000;
	if (cclr == 1) period = (UINT32)t->gra + 1;
	if (cclr == 2) period = (UINT32)t->grb + 1;

	UINT32 best = 0xffffffff;

	if (t->tier & 1) { UINT32 d = ticks_until(c, t->gra, period); if (d < best) best = d; }
	if (t->tier & 2) { UINT32 d = ticks_until(c, t->grb, period); if (d < best) best = d; }
	if (t->tier & 4) {
		UINT32 d = 0xffffffff;
		if (period == 0x10000 || c >= period) d = 0x10000 - c;
		if (d < best) best = d;
	}

	if (best == 0xffffffff) return ~(UINT64)0;
	if (best == 0) best = 1;

	return t->last + (UINT64)best * div;
}

static void timers_sync(UINT64 now)
{
	for (INT32 i = 0; i < 5; i++) timer_advance(i, now);
}

static void update_events()
{
	next_event = ~(UINT64)0;
	for (INT32 i = 0; i < 5; i++) {
		UINT64 e = timer_next_event(i);
		h8.tmr[i].event = e;
		if (e < next_event) next_event = e;
	}
	if (h8.adc_event < next_event) next_event = h8.adc_event;
}

// interrupt controller
// returns vector or -1; priority in *prio
static INT32 irq_pending_vector(INT32 *prio)
{
	INT32 best = -1, best_prio = -1;

	// quick test
	{
		UINT32 any = h8.isr & h8.ier & 0x3f;
		for (INT32 ch = 0; ch < 5; ch++) any |= h8.tmr[ch].tsr & h8.tmr[ch].tier & 7;
		for (INT32 ch = 0; ch < 2; ch++) {
			const h8_sci *s = &h8.sci[ch];
			any |= ((s->scr & 0x40) && (s->ssr & 0x78)) | ((s->scr & 0x80) && (s->ssr & 0x80)) | ((s->scr & 0x04) && (s->ssr & 0x04));
		}
		any |= ((h8.adcsr & 0xc0) == 0xc0);
		if (!any) { *prio = -1; return -1; }
	}

#define CAND(vec, cond, pr) \
	if ((cond)) { INT32 p_ = (pr) ? 1 : 0; if (p_ > best_prio) { best_prio = p_; best = (vec); } }

	// lower vector numbers win within the same priority level, so scan in order
	for (INT32 i = 0; i < 6; i++) {
		INT32 pr;
		switch (i) {
			case 0: pr = h8.ipra & 0x80; break;
			case 1: pr = h8.ipra & 0x40; break;
			case 2: case 3: pr = h8.ipra & 0x20; break;
			default: pr = h8.ipra & 0x10; break;
		}
		CAND(12 + i, (h8.isr & h8.ier & (1 << i)), pr)
	}

	for (INT32 ch = 0; ch < 5; ch++) {
		h8_timer *t = &h8.tmr[ch];
		INT32 pr;
		switch (ch) {
			case 0: pr = h8.ipra & 0x04; break;
			case 1: pr = h8.ipra & 0x02; break;
			case 2: pr = h8.ipra & 0x01; break;
			case 3: pr = h8.iprb & 0x80; break;
			default: pr = h8.iprb & 0x40; break;
		}
		CAND(24 + ch * 4 + 0, (t->tsr & t->tier & 1), pr)
		CAND(24 + ch * 4 + 1, (t->tsr & t->tier & 2), pr)
		CAND(24 + ch * 4 + 2, (t->tsr & t->tier & 4), pr)
	}

	for (INT32 ch = 0; ch < 2; ch++) {
		h8_sci *s = &h8.sci[ch];
		INT32 pr = h8.iprb & (ch ? 0x04 : 0x08);
		INT32 vb = 52 + ch * 4;
		CAND(vb + 0, ((s->scr & 0x40) && (s->ssr & 0x38)), pr)    // ERI
		CAND(vb + 1, ((s->scr & 0x40) && (s->ssr & 0x40)), pr)    // RXI
		CAND(vb + 2, ((s->scr & 0x80) && (s->ssr & 0x80)), pr)    // TXI
		CAND(vb + 3, ((s->scr & 0x04) && (s->ssr & 0x04)), pr)    // TEI
	}

	CAND(60, ((h8.adcsr & 0xc0) == 0xc0), h8.iprb & 0x02)

#undef CAND

	*prio = best_prio;
	return best;
}


static void push16(UINT16 v);

static void take_interrupt(INT32 vector);

// memory
static UINT8 io_read(UINT32 a);
static void io_write(UINT32 a, UINT8 d);

static inline INT32 is_onchip_ram(UINT32 a) { return a >= 0xfffd10 && a <= 0xffff0f; }

static UINT8 rb(UINT32 a)
{
	a &= ADDR_MASK;
	inst_cycles += 2;

	if (a >= 0xffff1c) return io_read(a);
	if (is_onchip_ram(a)) return h8.onchip_ram[a - 0xfffd10];

	UINT8 *p = mem_read[a >> 16];
	if (p) return p[a & 0xffff];
	if (pReadByte) return pReadByte(a);
	return 0xff;
}

static UINT16 rw(UINT32 a)
{
	a &= ADDR_MASK & ~1;
	inst_cycles += 2;

	if (a >= 0xffff1c) return (io_read(a) << 8) | io_read(a + 1);
	if (is_onchip_ram(a)) return (h8.onchip_ram[a - 0xfffd10] << 8) | h8.onchip_ram[a - 0xfffd10 + 1];

	UINT8 *p = mem_read[a >> 16];
	if (p) return (p[a & 0xffff] << 8) | p[(a & 0xffff) + 1];
	if (pReadWord) return pReadWord(a);
	return 0xffff;
}

static void wb(UINT32 a, UINT8 d)
{
	mem_writes++;
	a &= ADDR_MASK;
	inst_cycles += 2;

	if (a >= 0xffff1c) { io_write(a, d); return; }
	if (is_onchip_ram(a)) { h8.onchip_ram[a - 0xfffd10] = d; return; }

	UINT8 *p = mem_write[a >> 16];
	if (p) { p[a & 0xffff] = d; return; }
	if (pWriteByte) pWriteByte(a, d);
}

static void ww(UINT32 a, UINT16 d)
{
	mem_writes++;
	a &= ADDR_MASK & ~1;
	inst_cycles += 2;

	if (a >= 0xffff1c) { io_write(a, d >> 8); io_write(a + 1, d & 0xff); return; }
	if (is_onchip_ram(a)) { h8.onchip_ram[a - 0xfffd10] = d >> 8; h8.onchip_ram[a - 0xfffd10 + 1] = d & 0xff; return; }

	UINT8 *p = mem_write[a >> 16];
	if (p) { p[a & 0xffff] = d >> 8; p[(a & 0xffff) + 1] = d & 0xff; return; }
	if (pWriteWord) pWriteWord(a, d);
}

static inline UINT32 rl(UINT32 a) { return ((UINT32)rw(a) << 16) | rw(a + 2); }
static inline void wl(UINT32 a, UINT32 d) { ww(a, d >> 16); ww(a + 2, d & 0xffff); }

static inline UINT16 fetch16()
{
	UINT32 a = h8.pc & ADDR_MASK & ~1;
	UINT16 v;
	UINT8 *p = mem_read[a >> 16];
	inst_cycles += 2;
	if (p) v = (p[a & 0xffff] << 8) | p[(a & 0xffff) + 1];
	else { v = rw(a); inst_cycles -= 2; }
	h8.pc = (h8.pc + 2) & ADDR_MASK;
	return v;
}

static inline UINT32 fetch32()
{
	UINT32 hi = fetch16();
	return (hi << 16) | fetch16();
}

// ports
static INT32 port_index(UINT32 a)
{
	switch (a & 0xff) {
		case 0xc5: case 0xc7: return 4;
		case 0xc9: case 0xcb: return 6;
		case 0xce: return 7;
		case 0xcd: case 0xcf: return 8;
		case 0xd0: case 0xd2: return 9;
		case 0xd1: case 0xd3: return 10;
		case 0xd4: case 0xd6: return 11;
	}
	return -1;
}

static UINT8 port_read(INT32 n)
{
	UINT8 in = pReadPort ? pReadPort(n) : 0xff;
	if (n == 7) return in;
	return (h8.dr[n] & h8.ddr[n]) | (in & ~h8.ddr[n]);
}

static void port_write(INT32 n, UINT8 d)
{
	h8.dr[n] = d;
	if (pWritePort) pWritePort(n, (d | ~h8.ddr[n]) & 0xff); // inputs float high
}

// SCI
static void sci_rx_try(INT32 ch)
{
	irq_dirty = 1;
	h8_sci *s = &h8.sci[ch];
	if (!(s->scr & 0x10)) return;      // RE
	if (s->ssr & 0x40) return;         // RDRF still set
	if (!pSciRx) return;

	INT32 d = pSciRx(ch);
	if (d < 0) return;
	s->rdr = d & 0xff;
	s->ssr |= 0x40;
}

static void sci_tx_kick(INT32 ch)
{
	irq_dirty = 1;
	h8_sci *s = &h8.sci[ch];
	if (!(s->scr & 0x20)) return;      // TE
	if (s->ssr & 0x80) return;         // TDRE set, nothing to send

	if (pSciTx) pSciTx(ch, s->tdr);
	s->ssr |= 0x80 | 0x04;             // TDRE + TEND

	// clocked synchronous mode receives a byte for every byte clocked out
	if ((s->smr & 0x80) && (s->scr & 0x10)) sci_rx_try(ch);
}

static UINT8 sci_read(INT32 ch, INT32 reg)
{
	h8_sci *s = &h8.sci[ch];
	switch (reg) {
		case 0: return s->smr;
		case 1: return s->brr;
		case 2: return s->scr;
		case 3: return s->tdr;
		case 4:
			sci_rx_try(ch);
			s->ssr_read |= s->ssr;
			return s->ssr;
		case 5: return s->rdr;
	}
	return 0xff;
}

static void sci_write(INT32 ch, INT32 reg, UINT8 d)
{
	irq_dirty = 1;
	h8_sci *s = &h8.sci[ch];
	switch (reg) {
		case 0: s->smr = d; break;
		case 1: s->brr = d; break;
		case 2: {
			UINT8 old = s->scr;
			s->scr = d;
			if (!(d & 0x20)) s->ssr |= 0x80 | 0x04;
			if ((d & 0x20) && !(old & 0x20)) s->ssr |= 0x80 | 0x04;
			if ((d & 0x10) && !(old & 0x10) && (s->smr & 0x80)) sci_rx_try(ch);
		}
		break;
		case 3: s->tdr = d; break;
		case 4: {
			// flags can only be cleared (to 0) after being read as 1
			UINT8 clr = ~d & s->ssr_read & 0xf8;
			s->ssr &= ~clr;
			s->ssr_read &= ~clr;
			if (clr & 0x80) {
				s->ssr &= ~0x04;
				sci_tx_kick(ch);
			}
			if ((clr & 0x40) && (s->smr & 0x80) && (s->scr & 0x10) && !(s->scr & 0x20))
				sci_rx_try(ch);
		}
		break;
	}
}

// ADC
static void adc_convert()
{
	irq_dirty = 1;
	INT32 chmax = h8.adcsr & 7;
	INT32 first = (h8.adcsr & 0x10) ? (chmax & 4) : chmax;

	for (INT32 c = first; c <= chmax; c++) {
		UINT16 v = pAdcRead ? (pAdcRead(c) & 0x3ff) : 0;
		h8.addr[c & 3] = v << 6;
	}

	h8.adcsr |= 0x80; // ADF
	if (!(h8.adcsr & 0x10)) h8.adcsr &= ~0x20; // single mode ends
}

static void adc_start()
{
	INT32 chans = (h8.adcsr & 0x10) ? ((h8.adcsr & 3) + 1) : 1;
	INT32 per = (h8.adcsr & 0x08) ? 134 : 266;
	h8.adc_event = cur_cycle() + chans * per;
	update_events();
}

void H8AdcTrigger()
{
	irq_dirty = 1;
	if ((h8.adcr & 0x80) && !(h8.adcsr & 0x20)) {
		h8.adcsr |= 0x20;
		adc_start();
	}
}

// timers (register level)
static UINT8 timer_read(INT32 ch, INT32 reg)
{
	h8_timer *t = &h8.tmr[ch];
	timer_advance(ch, cur_cycle());

	switch (reg) {
		case 0: return t->tcr;
		case 1: return t->tior;
		case 2: return t->tier;
		case 3: t->tsr_read |= t->tsr; return t->tsr | 0xf8;
		case 4: return t->tcnt >> 8;
		case 5: return t->tcnt & 0xff;
		case 6: return t->gra >> 8;
		case 7: return t->gra & 0xff;
		case 8: return t->grb >> 8;
		case 9: return t->grb & 0xff;
		case 10: return t->bra >> 8;
		case 11: return t->bra & 0xff;
		case 12: return t->brb >> 8;
		case 13: return t->brb & 0xff;
	}
	return 0xff;
}

static void timer_write(INT32 ch, INT32 reg, UINT8 d)
{
	irq_dirty = 1;
	h8_timer *t = &h8.tmr[ch];
	timer_advance(ch, cur_cycle());

	switch (reg) {
		case 0: t->tcr = d; break;
		case 1: t->tior = d; break;
		case 2: t->tier = d; break;
		case 3: {
			UINT8 clr = ~d & t->tsr_read & 7;
			t->tsr &= ~clr;
			t->tsr_read &= ~clr;
		}
		break;
		case 4: t->tcnt = (t->tcnt & 0x00ff) | (d << 8); break;
		case 5: t->tcnt = (t->tcnt & 0xff00) | d; break;
		case 6: t->gra = (t->gra & 0x00ff) | (d << 8); break;
		case 7: t->gra = (t->gra & 0xff00) | d; break;
		case 8: t->grb = (t->grb & 0x00ff) | (d << 8); break;
		case 9: t->grb = (t->grb & 0xff00) | d; break;
		case 10: t->bra = (t->bra & 0x00ff) | (d << 8); break;
		case 11: t->bra = (t->bra & 0xff00) | d; break;
		case 12: t->brb = (t->brb & 0x00ff) | (d << 8); break;
		case 13: t->brb = (t->brb & 0xff00) | d; break;
	}

	update_events();
}

// map 0xffff60-0xffff9f to (channel, register)
static INT32 timer_decode(UINT32 a, INT32 *ch, INT32 *reg)
{
	UINT32 o = a & 0xff;
	if (o >= 0x64 && o <= 0x6d) { *ch = 0; *reg = o - 0x64; return 1; }
	if (o >= 0x6e && o <= 0x77) { *ch = 1; *reg = o - 0x6e; return 1; }
	if (o >= 0x78 && o <= 0x81) { *ch = 2; *reg = o - 0x78; return 1; }
	if (o >= 0x82 && o <= 0x8f) { *ch = 3; *reg = o - 0x82; return 1; }
	if (o >= 0x92 && o <= 0x9f) { *ch = 4; *reg = o - 0x92; return 1; }
	return 0;
}

// I/O register file
static UINT8 io_read(UINT32 a)
{
	UINT32 o = a & 0xff;
	INT32 ch, reg;

	if (o >= 0x20 && o <= 0x3f) return h8.dma_regs[o - 0x20];

	if (o == 0x60) { timers_sync(cur_cycle()); return h8.tstr | 0xe0; }
	if (o == 0x61) return h8.tsnc | 0xe0;
	if (o == 0x62) return h8.tmdr | 0x80;
	if (o == 0x63) return h8.tfcr | 0xc0;
	if (o == 0x90) return h8.toer | 0xc0;
	if (o == 0x91) return h8.tocr | 0xec;
	if (timer_decode(a, &ch, &reg)) return timer_read(ch, reg);

	if (o == 0xa8) return h8.wd_tcsr | 0x18;
	if (o == 0xa9) return h8.wd_tcnt;
	if (o == 0xad) return h8.rtmcsr;
	if (o == 0xae) return h8.rtcnt;
	if (o == 0xaf) return h8.rtcor;

	if (o >= 0xb0 && o <= 0xb5) return sci_read(0, o - 0xb0);
	if (o >= 0xb8 && o <= 0xbd) return sci_read(1, o - 0xb8);

	switch (o) {
		case 0xc5: case 0xc9: case 0xcd: case 0xd0: case 0xd1: case 0xd4: return 0xff; // DDRs are write only
		case 0xc7: case 0xcb: case 0xce: case 0xcf: case 0xd2: case 0xd3: case 0xd6:
			return port_read(port_index(a));
		case 0xda: return h8.p4pcr;
	}

	if (o >= 0xe0 && o <= 0xe7) {
		UINT16 v = h8.addr[(o - 0xe0) >> 1];
		return (o & 1) ? (v & 0xc0) : (v >> 8);
	}
	if (o == 0xe8) return h8.adcsr;
	if (o == 0xe9) return h8.adcr | 0x7f;

	switch (o) {
		case 0xec: return h8.abwcr;
		case 0xed: return h8.astcr;
		case 0xee: return h8.wcr | 0xf0;
		case 0xf2: return h8.syscr;
		case 0xf3: return h8.brcr;
		case 0xf4: return h8.iscr | 0xc0;
		case 0xf5: return h8.ier | 0xc0;
		case 0xf6: h8.isr_read |= h8.isr; return h8.isr | 0xc0;
		case 0xf8: return h8.ipra;
		case 0xf9: return h8.iprb;
	}

	return 0xff;
}

static void io_write(UINT32 a, UINT8 d)
{
	irq_dirty = 1;
	UINT32 o = a & 0xff;
	INT32 ch, reg;

	if (o >= 0x20 && o <= 0x3f) { h8.dma_regs[o - 0x20] = d; return; }

	if (o == 0x60) {
		timers_sync(cur_cycle());
		for (INT32 i = 0; i < 5; i++) h8.tmr[i].last = cur_cycle();
		h8.tstr = d & 0x1f;
		update_events();
		return;
	}
	if (o == 0x61) { h8.tsnc = d; return; }
	if (o == 0x62) { h8.tmdr = d; return; }
	if (o == 0x63) { h8.tfcr = d; return; }
	if (o == 0x90) { h8.toer = d; return; }
	if (o == 0x91) { h8.tocr = d; return; }
	if (timer_decode(a, &ch, &reg)) { timer_write(ch, reg, d); return; }

	if (o == 0xa8 || o == 0xa9 || o == 0xaa || o == 0xab) return; // watchdog: never fires
	if (o == 0xad) { h8.rtmcsr = d; return; }
	if (o == 0xae) { h8.rtcnt = d; return; }
	if (o == 0xaf) { h8.rtcor = d; return; }

	if (o >= 0xb0 && o <= 0xb5) { sci_write(0, o - 0xb0, d); return; }
	if (o >= 0xb8 && o <= 0xbd) { sci_write(1, o - 0xb8, d); return; }

	switch (o) {
		case 0xc5: case 0xc9: case 0xcd: case 0xd0: case 0xd1: case 0xd4: {
			INT32 n = port_index(a);
			h8.ddr[n] = d;
			if (pWritePort) pWritePort(n, (h8.dr[n] | ~d) & 0xff);
			return;
		}
		case 0xc7: case 0xcb: case 0xcf: case 0xd2: case 0xd3: case 0xd6:
			port_write(port_index(a), d);
			return;
		case 0xda: h8.p4pcr = d; return;
	}

	if (o == 0xe8) {
		UINT8 old = h8.adcsr;
		// ADF is cleared by writing 0 after reading 1
		UINT8 adf = old & 0x80;
		if (!(d & 0x80)) adf = 0;
		h8.adcsr = adf | (d & 0x7f);
		if ((d & 0x20) && !(old & 0x20)) adc_start();
		if (!(d & 0x20)) { h8.adc_event = ~(UINT64)0; update_events(); }
		return;
	}
	if (o == 0xe9) { h8.adcr = d & 0x80; return; }

	switch (o) {
		case 0xec: h8.abwcr = d; return;
		case 0xed: h8.astcr = d; return;
		case 0xee: h8.wcr = d; return;
		case 0xf2: h8.syscr = (h8.syscr & 0x01) | (d & 0xfe); return; // RAME etc.
		case 0xf3: h8.brcr = d; return;
		case 0xf4: h8.iscr = d & 0x3f; return;
		case 0xf5: h8.ier = d & 0x3f; return;
		case 0xf6: {
			UINT8 clr = ~d & h8.isr_read & 0x3f;
			h8.isr &= ~clr;
			h8.isr_read &= ~clr;
			// level sensitive inputs that are still asserted stay pending
			h8.isr |= h8.irq_level & ~h8.iscr & 0x3f;
			return;
		}
		case 0xf8: h8.ipra = d; return;
		case 0xf9: h8.iprb = d; return;
	}
}

// flags
static inline void set_nz8(UINT8 v)   { h8.ccr &= ~(F_N|F_Z); if (!v) h8.ccr |= F_Z; else if (v & 0x80) h8.ccr |= F_N; }
static inline void set_nz16(UINT16 v) { h8.ccr &= ~(F_N|F_Z); if (!v) h8.ccr |= F_Z; else if (v & 0x8000) h8.ccr |= F_N; }
static inline void set_nz32(UINT32 v) { h8.ccr &= ~(F_N|F_Z); if (!v) h8.ccr |= F_Z; else if (v & 0x80000000) h8.ccr |= F_N; }
static inline void set_nzv8(UINT8 v)   { set_nz8(v);  h8.ccr &= ~F_V; }
static inline void set_nzv16(UINT16 v) { set_nz16(v); h8.ccr &= ~F_V; }
static inline void set_nzv32(UINT32 v) { set_nz32(v); h8.ccr &= ~F_V; }

#define ADD_IMPL(name, T, BITS, HMASK) \
static T name(T v1, T v2) { \
	UINT64 res = (UINT64)v1 + (UINT64)v2; \
	T r = (T)res; \
	h8.ccr &= ~(F_N|F_V|F_Z|F_C|F_H); \
	if ((((UINT64)v1 & HMASK) + ((UINT64)v2 & HMASK)) & (HMASK + 1)) h8.ccr |= F_H; \
	if (!r) h8.ccr |= F_Z; else if (r >> (BITS - 1)) h8.ccr |= F_N; \
	if ((~(v1 ^ v2) & (v1 ^ r)) >> (BITS - 1) & 1) h8.ccr |= F_V; \
	if (res >> BITS) h8.ccr |= F_C; \
	return r; \
}

#define SUB_IMPL(name, T, BITS, HMASK) \
static T name(T v1, T v2) { \
	UINT64 res = (UINT64)v1 - (UINT64)v2; \
	T r = (T)res; \
	h8.ccr &= ~(F_N|F_V|F_Z|F_C|F_H); \
	if ((((UINT64)v1 & HMASK) - ((UINT64)v2 & HMASK)) & (HMASK + 1)) h8.ccr |= F_H; \
	if (!r) h8.ccr |= F_Z; else if (r >> (BITS - 1)) h8.ccr |= F_N; \
	if (((v1 ^ v2) & (v1 ^ r)) >> (BITS - 1) & 1) h8.ccr |= F_V; \
	if ((res >> BITS) & 1) h8.ccr |= F_C; \
	return r; \
}

ADD_IMPL(do_add8,  UINT8,  8,  0xfULL)
ADD_IMPL(do_add16, UINT16, 16, 0xfffULL)
ADD_IMPL(do_add32, UINT32, 32, 0xfffffffULL)
SUB_IMPL(do_sub8,  UINT8,  8,  0xfULL)
SUB_IMPL(do_sub16, UINT16, 16, 0xfffULL)
SUB_IMPL(do_sub32, UINT32, 32, 0xfffffffULL)

static UINT8 do_addx8(UINT8 v1, UINT8 v2)
{
	UINT8 c = (h8.ccr & F_C) ? 1 : 0;
	UINT16 res = v1 + v2 + c;
	h8.ccr &= ~(F_N|F_V|F_C|F_H);
	if (((v1 & 0xf) + (v2 & 0xf) + c) & 0x10) h8.ccr |= F_H;
	if ((UINT8)res) h8.ccr &= ~F_Z;
	if (res & 0x80) h8.ccr |= F_N;
	if (~(v1 ^ v2) & (v1 ^ res) & 0x80) h8.ccr |= F_V;
	if (res & 0x100) h8.ccr |= F_C;
	return (UINT8)res;
}

static UINT8 do_subx8(UINT8 v1, UINT8 v2)
{
	UINT8 c = (h8.ccr & F_C) ? 1 : 0;
	UINT16 res = v1 - v2 - c;
	h8.ccr &= ~(F_N|F_V|F_C|F_H);
	if (((v1 & 0xf) - (v2 & 0xf) - c) & 0x10) h8.ccr |= F_H;
	if ((UINT8)res) h8.ccr &= ~F_Z;
	if (res & 0x80) h8.ccr |= F_N;
	if ((v1 ^ v2) & (v1 ^ res) & 0x80) h8.ccr |= F_V;
	if (res & 0x100) h8.ccr |= F_C;
	return (UINT8)res;
}

#define INCDEC_IMPL(name, T, BITS, OP, VEXPR) \
static T name(T v1, T v2) { \
	T res = (T)(v1 OP v2); \
	h8.ccr &= ~(F_N|F_V|F_Z); \
	if (!res) h8.ccr |= F_Z; else if (res >> (BITS - 1)) h8.ccr |= F_N; \
	if ((VEXPR) >> (BITS - 1) & 1) h8.ccr |= F_V; \
	return res; \
}

INCDEC_IMPL(do_inc8,  UINT8,  8,  +, (~(v1 ^ v2) & (v1 ^ res)))
INCDEC_IMPL(do_inc16, UINT16, 16, +, (~(v1 ^ v2) & (v1 ^ res)))
INCDEC_IMPL(do_inc32, UINT32, 32, +, (~(v1 ^ v2) & (v1 ^ res)))
INCDEC_IMPL(do_dec8,  UINT8,  8,  -, ((v1 ^ v2) & (v1 ^ res)))
INCDEC_IMPL(do_dec16, UINT16, 16, -, ((v1 ^ v2) & (v1 ^ res)))
INCDEC_IMPL(do_dec32, UINT32, 32, -, ((v1 ^ v2) & (v1 ^ res)))

// shifts/rotates: size 0=byte 1=word 2=long
static UINT32 do_shift(INT32 type, INT32 size, UINT32 v)
{
	INT32 bits = 8 << size;
	UINT32 mask = (size == 2) ? 0xffffffff : ((1U << bits) - 1);
	UINT32 msb = 1U << (bits - 1);
	UINT32 r = 0;
	INT32 c = (h8.ccr & F_C) ? 1 : 0;

	v &= mask;
	h8.ccr &= ~(F_N|F_V|F_Z|F_C);

	switch (type) {
		case 0: // shll
			if (v & msb) h8.ccr |= F_C;
			r = (v << 1) & mask;
		break;
		case 1: // shal
			if (v & msb) h8.ccr |= F_C;
			r = (v << 1) & mask;
			if ((v ^ r) & msb) h8.ccr |= F_V;
		break;
		case 2: // shlr
			if (v & 1) h8.ccr |= F_C;
			r = v >> 1;
		break;
		case 3: // shar
			if (v & 1) h8.ccr |= F_C;
			r = (v >> 1) | (v & msb);
		break;
		case 4: // rotxl
			if (v & msb) h8.ccr |= F_C;
			r = ((v << 1) | c) & mask;
		break;
		case 5: // rotl
			if (v & msb) h8.ccr |= F_C;
			r = ((v << 1) | ((v & msb) ? 1 : 0)) & mask;
		break;
		case 6: // rotxr
			if (v & 1) h8.ccr |= F_C;
			r = (v >> 1) | (c ? msb : 0);
		break;
		case 7: // rotr
			if (v & 1) h8.ccr |= F_C;
			r = (v >> 1) | ((v & 1) ? msb : 0);
		break;
	}

	if (!r) h8.ccr |= F_Z; else if (r & msb) h8.ccr |= F_N;
	return r;
}

// stack / control flow
static void push16(UINT16 v)
{
	h8.er[7] -= 2;
	ww(h8.er[7], v);
}

static void push_pc32(UINT32 pc)
{
	h8.er[7] -= 4;
	ww(h8.er[7], (pc >> 16) & 0xffff);
	ww(h8.er[7] + 2, pc & 0xffff);
}

static UINT32 pop_pc32()
{
	UINT32 hi = rw(h8.er[7]);
	UINT32 lo = rw(h8.er[7] + 2);
	h8.er[7] += 4;
	return ((hi << 16) | lo) & ADDR_MASK;
}

static void take_interrupt(INT32 vector)
{
	irq_dirty = 1;
	mem_writes++;
	// accepting an external IRQ clears its ISR flag (unless it is a level
	// triggered input that is still being held low)
	if (vector >= 12 && vector <= 17) {
		UINT8 m = 1 << (vector - 12);
		if ((h8.iscr & m) || !(h8.irq_level & m)) h8.isr &= ~m;
	}

	push16(h8.pc & 0xffff);
	push16((h8.ccr << 8) | ((h8.pc >> 16) & 0xff));

	if (h8.syscr & 0x08) h8.ccr |= F_I;
	else h8.ccr |= F_I | F_UI;

	h8.pc = rl(vector * 4) & ADDR_MASK;
	h8.sleeping = 0;
	inst_cycles += 4;
}

static INT32 irq_allowed(INT32 prio)
{
	if (h8.syscr & 0x08) {
		return (h8.ccr & F_I) ? 0 : 1;
	}
	if ((h8.ccr & (F_I | F_UI)) == (F_I | F_UI)) return 0;
	if (h8.ccr & F_I) return prio > 0;
	return 1;
}

static inline INT32 cond(INT32 c)
{
	UINT8 f = h8.ccr;
	INT32 C = (f & F_C) != 0, V = (f & F_V) != 0, Z = (f & F_Z) != 0, N = (f & F_N) != 0;
	switch (c) {
		case 0x0: return 1;
		case 0x1: return 0;
		case 0x2: return !(C || Z);
		case 0x3: return C || Z;
		case 0x4: return !C;
		case 0x5: return C;
		case 0x6: return !Z;
		case 0x7: return Z;
		case 0x8: return !V;
		case 0x9: return V;
		case 0xa: return !N;
		case 0xb: return N;
		case 0xc: return N == V;
		case 0xd: return N != V;
		case 0xe: return !Z && (N == V);
		case 0xf: return Z || (N != V);
	}
	return 0;
}

// bit operations on an 8-bit value
static UINT8 bitop(INT32 op, INT32 bit, UINT8 v, INT32 *write)
{
	UINT8 m = 1 << (bit & 7);
	*write = 0;
	switch (op) {
		case 0x0: v |= m; *write = 1; break;             // bset
		case 0x1: v ^= m; *write = 1; break;             // bnot
		case 0x2: v &= ~m; *write = 1; break;            // bclr
		case 0x3: if (v & m) h8.ccr &= ~F_Z; else h8.ccr |= F_Z; break; // btst
		case 0x4: if (v & m) h8.ccr |= F_C; break;       // bor
		case 0x5: if (!(v & m)) h8.ccr |= F_C; break;    // bior
		case 0x6: if (v & m) h8.ccr ^= F_C; break;       // bxor
		case 0x7: if (!(v & m)) h8.ccr ^= F_C; break;    // bixor
		case 0x8: if (!(v & m)) h8.ccr &= ~F_C; break;   // band
		case 0x9: if (v & m) h8.ccr &= ~F_C; break;      // biand
		case 0xa: if (v & m) h8.ccr |= F_C; else h8.ccr &= ~F_C; break;  // bld
		case 0xb: if (!(v & m)) h8.ccr |= F_C; else h8.ccr &= ~F_C; break; // bild
		case 0xc: if (h8.ccr & F_C) v |= m; else v &= ~m; *write = 1; break;  // bst
		case 0xd: if (!(h8.ccr & F_C)) v |= m; else v &= ~m; *write = 1; break; // bist
	}
	return v;
}

// decode the second word of a 7c/7d/7e/7f instruction (or the 6x/7x reg forms)
// returns op code for bitop(), and the bit number
static INT32 decode_bit_insn(UINT16 w, INT32 *bit, INT32 *ok)
{
	UINT8 hi = w >> 8, lo = w & 0xff;
	*ok = 1;
	switch (hi) {
		case 0x60: *bit = r8(lo >> 4); return 0x0;
		case 0x61: *bit = r8(lo >> 4); return 0x1;
		case 0x62: *bit = r8(lo >> 4); return 0x2;
		case 0x63: *bit = r8(lo >> 4); return 0x3;
		case 0x67: *bit = (lo >> 4) & 7; return (lo & 0x80) ? 0xd : 0xc;
		case 0x70: *bit = (lo >> 4) & 7; return 0x0;
		case 0x71: *bit = (lo >> 4) & 7; return 0x1;
		case 0x72: *bit = (lo >> 4) & 7; return 0x2;
		case 0x73: *bit = (lo >> 4) & 7; return 0x3;
		case 0x74: *bit = (lo >> 4) & 7; return (lo & 0x80) ? 0x5 : 0x4;
		case 0x75: *bit = (lo >> 4) & 7; return (lo & 0x80) ? 0x7 : 0x6;
		case 0x76: *bit = (lo >> 4) & 7; return (lo & 0x80) ? 0x9 : 0x8;
		case 0x77: *bit = (lo >> 4) & 7; return (lo & 0x80) ? 0xb : 0xa;
	}
	*ok = 0;
	return 0;
}

// addressing helpers for mov
static inline UINT32 abs16(UINT16 a) { return (UINT32)(INT32)(INT16)a & ADDR_MASK; }
static inline UINT32 abs8(UINT8 a)   { return 0xffff00 | a; }

static void illegal()
{
	// treat as nop; real hardware would raise an exception the BIOS never uses
}

// size: 0 byte, 1 word, 2 long
static void mov_load(INT32 size, INT32 rd, UINT32 ea)
{
	switch (size) {
		case 0: { UINT8 v = rb(ea); w8(rd, v); set_nzv8(v); } break;
		case 1: { UINT16 v = rw(ea); w16(rd, v); set_nzv16(v); } break;
		default: { UINT32 v = rl(ea); w32(rd, v); set_nzv32(v); } break;
	}
}

static void mov_store(INT32 size, INT32 rs, UINT32 ea)
{
	switch (size) {
		case 0: { UINT8 v = r8(rs); set_nzv8(v); wb(ea, v); } break;
		case 1: { UINT16 v = r16(rs); set_nzv16(v); ww(ea, v); } break;
		default: { UINT32 v = r32(rs); set_nzv32(v); wl(ea, v); } break;
	}
}

// handles the 68..6f / 78 memory move group; w0 is the first opcode word.
// size override for mov.l (prefix 0100) is passed in 'size'.
static void mov_mem_group(UINT16 w0, INT32 size)
{
	UINT8 op = w0 >> 8;
	UINT8 b = w0 & 0xff;
	INT32 store = b & 0x80;
	INT32 ern = (b >> 4) & 7;
	INT32 r = b & 0x0f;
	INT32 inc = 1 << size;

	switch (op) {
		case 0x68: case 0x69: // @ERn
			if (store) mov_store(size, r, h8.er[ern] & ADDR_MASK);
			else mov_load(size, r, h8.er[ern] & ADDR_MASK);
		break;

		case 0x6a: case 0x6b: { // absolute
			INT32 mode = (b >> 4) & 0xf;
			UINT32 ea;
			switch (mode & 7) {
				case 0: ea = abs16(fetch16()); break;
				case 2: ea = fetch32() & ADDR_MASK; break;
				default: illegal(); return;
			}
			if (mode & 8) mov_store(size, r, ea);
			else mov_load(size, r, ea);
		}
		break;

		case 0x6c: case 0x6d: // @ERn+ / @-ERn
			if (store) {
				h8.er[ern] -= inc;
				mov_store(size, r, h8.er[ern] & ADDR_MASK);
			} else {
				UINT32 ea = h8.er[ern] & ADDR_MASK;
				h8.er[ern] += inc;
				mov_load(size, r, ea);
			}
		break;

		case 0x6e: case 0x6f: { // @(d16, ERn)
			UINT32 ea = (h8.er[ern] + (INT32)(INT16)fetch16()) & ADDR_MASK;
			if (store) mov_store(size, r, ea);
			else mov_load(size, r, ea);
		}
		break;
	}
}

static void execute_one()
{
	UINT32 ppc = h8.pc;
	UINT16 w0 = fetch16();
	UINT8 op = w0 >> 8;
	UINT8 b = w0 & 0xff;
	INT32 hi = b >> 4, lo = b & 0xf;

	switch (op)
	{
		case 0x00: // nop
		break;

		case 0x01:
			switch (b) {
				case 0x00: { // mov.l prefix
					UINT16 w1 = fetch16();
					UINT8 op2 = w1 >> 8;
					if (op2 == 0x78) {
						// mov.l @(d24,ERs) forms: 0100 78s0 6b2r/6bar disp32
						UINT16 w2 = fetch16();
						UINT32 disp = fetch32();
						INT32 ern = ((w1 & 0xff) >> 4) & 7;
						INT32 rr = w2 & 7;
						UINT32 ea = (h8.er[ern] + disp) & ADDR_MASK;
						if (w2 & 0x80) mov_store(2, rr, ea);
						else mov_load(2, rr, ea);
					} else {
						mov_mem_group(w1 & 0xfff7 ? w1 : w1, 2);
					}
				}
				break;

				case 0x40: { // ldc.w / stc.w memory
					UINT16 w1 = fetch16();
					UINT8 op2 = w1 >> 8, b2 = w1 & 0xff;
					INT32 store = b2 & 0x80;
					INT32 ern = (b2 >> 4) & 7;
					UINT32 ea = 0;
					switch (op2) {
						case 0x69: ea = h8.er[ern]; break;
						case 0x6b: ea = ((b2 & 0x20) ? fetch32() : abs16(fetch16())); break;
						case 0x6d:
							if (store) { h8.er[ern] -= 2; ea = h8.er[ern]; }
							else { ea = h8.er[ern]; h8.er[ern] += 2; }
						break;
						case 0x6f: ea = h8.er[ern] + (INT32)(INT16)fetch16(); break;
						case 0x78: {
							UINT16 w2 = fetch16();
							UINT32 disp = fetch32();
							store = w2 & 0x80;
							ea = h8.er[ern] + disp;
						}
						break;
						default: illegal(); return;
					}
					if (store) ww(ea & ADDR_MASK, h8.ccr << 8);
					else h8.ccr = rw(ea & ADDR_MASK) >> 8;
				}
				break;

				case 0x80: // sleep
					h8.sleeping = 1;
				break;

				case 0xc0: { // mulxs
					UINT16 w1 = fetch16();
					INT32 rs = (w1 >> 4) & 0xf, rd = w1 & 0xf;
					if ((w1 >> 8) == 0x50) {
						INT32 res = (INT32)(INT8)r16(rd) * (INT32)(INT8)r8(rs);
						w16(rd, (UINT16)res);
						set_nz16((UINT16)res);
					} else if ((w1 >> 8) == 0x52) {
						INT32 res = (INT32)(INT16)r32(rd) * (INT32)(INT16)r16(rs);
						w32(rd, (UINT32)res);
						set_nz32((UINT32)res);
					}
					inst_cycles += 16;
				}
				break;

				case 0xd0: { // divxs
					UINT16 w1 = fetch16();
					INT32 rs = (w1 >> 4) & 0xf, rd = w1 & 0xf;
					inst_cycles += 16;
					if ((w1 >> 8) == 0x51) {
						INT32 p = (INT16)r16(rd);
						INT32 d = (INT8)r8(rs);
						h8.ccr &= ~(F_Z | F_N);
						if (!d) h8.ccr |= F_Z;
						else {
							INT32 q = p / d, rm = p % d; // C truncation matches the H8 sign rules
							if (q < 0) h8.ccr |= F_N;
							w16(rd, (q & 0xff) | ((rm & 0xff) << 8));
						}
					} else if ((w1 >> 8) == 0x53) {
						INT64 p = (INT32)r32(rd);
						INT64 d = (INT16)r16(rs);
						h8.ccr &= ~(F_Z | F_N);
						if (!d) h8.ccr |= F_Z;
						else {
							INT64 q = p / d, rm = p % d;
							if (q < 0) h8.ccr |= F_N;
							w32(rd, (UINT32)((q & 0xffff) | ((rm & 0xffff) << 16)));
						}
					}
				}
				break;

				case 0xf0: { // or.l / xor.l / and.l reg
					UINT16 w1 = fetch16();
					INT32 rs = (w1 >> 4) & 7, rd = w1 & 7;
					UINT32 v;
					switch (w1 >> 8) {
						case 0x64: v = r32(rs) | r32(rd); break;
						case 0x65: v = r32(rs) ^ r32(rd); break;
						case 0x66: v = r32(rs) & r32(rd); break;
						default: illegal(); return;
					}
					w32(rd, v);
					set_nzv32(v);
				}
				break;

				default:
					illegal();
				break;
			}
		break;

		case 0x02: w8(lo, h8.ccr); break;                            // stc ccr,rd
		case 0x03: h8.ccr = r8(lo); break;                           // ldc rs,ccr
		case 0x04: h8.ccr |= b; break;                               // orc
		case 0x05: h8.ccr ^= b; break;                               // xorc
		case 0x06: h8.ccr &= b; break;                               // andc
		case 0x07: h8.ccr = b; break;                                // ldc #imm

		case 0x08: w8(lo, do_add8(r8(lo), r8(hi))); break;
		case 0x09: w16(lo, do_add16(r16(lo), r16(hi))); break;
		case 0x0a:
			if (hi == 0) w8(lo, do_inc8(r8(lo), 1));
			else if (hi & 8) w32(lo, do_add32(r32(lo), r32(hi & 7)));
			else illegal();
		break;
		case 0x0b:
			switch (hi) {
				case 0x0: w32(lo, r32(lo) + 1); break;
				case 0x5: w16(lo, do_inc16(r16(lo), 1)); break;
				case 0x7: w32(lo, do_inc32(r32(lo), 1)); break;
				case 0x8: w32(lo, r32(lo) + 2); break;
				case 0x9: w32(lo, r32(lo) + 4); break;
				case 0xd: w16(lo, do_inc16(r16(lo), 2)); break;
				case 0xf: w32(lo, do_inc32(r32(lo), 2)); break;
				default: illegal(); break;
			}
		break;
		case 0x0c: { UINT8 v = r8(hi); w8(lo, v); set_nzv8(v); } break;
		case 0x0d: { UINT16 v = r16(hi); w16(lo, v); set_nzv16(v); } break;
		case 0x0e: w8(lo, do_addx8(r8(lo), r8(hi))); break;
		case 0x0f:
			if (hi == 0) { // daa
				UINT8 v = r8(lo), adj = 0;
				if (h8.ccr & F_C) {
					if (h8.ccr & F_H) { if ((v & 0xf0) <= 0x30 && (v & 0x0f) <= 3) adj = 0x66; }
					else { if ((v & 0xf0) <= 0x20) adj = (v & 0x0f) <= 9 ? 0x60 : 0x66; }
				} else {
					if (h8.ccr & F_H) { if ((v & 0x0f) <= 3) adj = (v & 0xf0) <= 0x90 ? 0x06 : 0x66; }
					else {
						if ((v & 0x0f) <= 9) adj = (v & 0xf0) <= 0x90 ? 0x00 : 0x60;
						else adj = (v & 0xf0) <= 0x80 ? 0x06 : 0x66;
					}
				}
				w8(lo, do_add8(v, adj));
			} else if (hi & 8) { // mov.l ers,erd
				UINT32 v = r32(hi & 7);
				w32(lo, v);
				set_nzv32(v);
			} else illegal();
		break;

		case 0x10: case 0x11: case 0x12: case 0x13: {
			// shll shal / shlr shar / rotxl rotl / rotxr rotr
			INT32 kind = ((op - 0x10) << 1) | ((hi >> 3) & 1);
			static const INT32 map[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
			INT32 type = map[kind];
			switch (hi & 7) {
				case 0: w8(lo, do_shift(type, 0, r8(lo))); break;
				case 1: w16(lo, do_shift(type, 1, r16(lo))); break;
				case 3: w32(lo, do_shift(type, 2, r32(lo))); break;
				default: illegal(); break;
			}
		}
		break;

		case 0x14: { UINT8 v = r8(lo) | r8(hi); w8(lo, v); set_nzv8(v); } break;
		case 0x15: { UINT8 v = r8(lo) ^ r8(hi); w8(lo, v); set_nzv8(v); } break;
		case 0x16: { UINT8 v = r8(lo) & r8(hi); w8(lo, v); set_nzv8(v); } break;

		case 0x17:
			switch (hi) {
				case 0x0: { UINT8 v = ~r8(lo); w8(lo, v); set_nzv8(v); } break;
				case 0x1: { UINT16 v = ~r16(lo); w16(lo, v); set_nzv16(v); } break;
				case 0x3: { UINT32 v = ~r32(lo); w32(lo, v); set_nzv32(v); } break;
				case 0x5: { UINT16 v = r16(lo) & 0xff; w16(lo, v); set_nzv16(v); } break;
				case 0x7: { UINT32 v = r32(lo) & 0xffff; w32(lo, v); set_nzv32(v); } break;
				case 0x8: w8(lo, do_sub8(0, r8(lo))); break;
				case 0x9: w16(lo, do_sub16(0, r16(lo))); break;
				case 0xb: w32(lo, do_sub32(0, r32(lo))); break;
				case 0xd: { UINT16 v = (UINT16)(INT16)(INT8)r16(lo); w16(lo, v); set_nzv16(v); } break;
				case 0xf: { UINT32 v = (UINT32)(INT32)(INT16)r32(lo); w32(lo, v); set_nzv32(v); } break;
				default: illegal(); break;
			}
		break;

		case 0x18: w8(lo, do_sub8(r8(lo), r8(hi))); break;
		case 0x19: w16(lo, do_sub16(r16(lo), r16(hi))); break;
		case 0x1a:
			if (hi == 0) w8(lo, do_dec8(r8(lo), 1));
			else if (hi & 8) w32(lo, do_sub32(r32(lo), r32(hi & 7)));
			else illegal();
		break;
		case 0x1b:
			switch (hi) {
				case 0x0: w32(lo, r32(lo) - 1); break;
				case 0x5: w16(lo, do_dec16(r16(lo), 1)); break;
				case 0x7: w32(lo, do_dec32(r32(lo), 1)); break;
				case 0x8: w32(lo, r32(lo) - 2); break;
				case 0x9: w32(lo, r32(lo) - 4); break;
				case 0xd: w16(lo, do_dec16(r16(lo), 2)); break;
				case 0xf: w32(lo, do_dec32(r32(lo), 2)); break;
				default: illegal(); break;
			}
		break;
		case 0x1c: do_sub8(r8(lo), r8(hi)); break;
		case 0x1d: do_sub16(r16(lo), r16(hi)); break;
		case 0x1e: w8(lo, do_subx8(r8(lo), r8(hi))); break;
		case 0x1f:
			if (hi == 0) { // das
				UINT8 v = r8(lo), adj = 0;
				if (h8.ccr & F_C) {
					if (h8.ccr & F_H) { if ((v & 0xf0) >= 0x60 && (v & 0x0f) >= 6) adj = 0x9a; }
					else { if ((v & 0xf0) >= 0x70 && (v & 0x0f) <= 9) adj = 0xa0; }
				} else {
					if (h8.ccr & F_H) { if ((v & 0xf0) <= 0x80 && (v & 0x0f) >= 6) adj = 0xfa; }
				}
				w8(lo, do_add8(v, adj));
			} else if (hi & 8) {
				do_sub32(r32(lo), r32(hi & 7));
			} else illegal();
		break;

		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
		case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
			mov_load(0, op & 0xf, abs8(b));
		break;

		case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
		case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
			mov_store(0, op & 0xf, abs8(b));
		break;

		case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
			if (cond(op & 0xf)) { h8.pc = (h8.pc + (INT8)b) & ADDR_MASK; idle_branch(h8.pc); }
		break;

		case 0x50: w16(lo, (UINT16)((r16(lo) & 0xff) * r8(hi))); inst_cycles += 10; break;
		case 0x51: {
			UINT16 p = r16(lo);
			UINT8 d = r8(hi);
			h8.ccr &= ~(F_Z | F_N);
			if (d & 0x80) h8.ccr |= F_N;
			if (!d) h8.ccr |= F_Z;
			else w16(lo, ((p / d) & 0xff) | (((p % d) & 0xff) << 8));
			inst_cycles += 10;
		}
		break;
		case 0x52: w32(lo, (UINT32)(r32(lo) & 0xffff) * r16(hi)); inst_cycles += 18; break;
		case 0x53: {
			UINT32 p = r32(lo);
			UINT16 d = r16(hi);
			h8.ccr &= ~(F_Z | F_N);
			if (d & 0x8000) h8.ccr |= F_N;
			if (!d) h8.ccr |= F_Z;
			else w32(lo, ((p / d) & 0xffff) | (((p % d) & 0xffff) << 16));
			inst_cycles += 18;
		}
		break;

		case 0x54: // rts
			h8.pc = pop_pc32();
		break;

		case 0x55: { // bsr d8
			push_pc32(h8.pc);
			h8.pc = (h8.pc + (INT8)b) & ADDR_MASK;
		}
		break;

		case 0x56: { // rte
			UINT16 t = rw(h8.er[7]);
			UINT16 l = rw(h8.er[7] + 2);
			h8.er[7] += 4;
			h8.ccr = t >> 8;
			h8.pc = (((UINT32)(t & 0xff) << 16) | l) & ADDR_MASK;
		}
		break;

		case 0x57: { // trapa #n
			INT32 vec = 8 + ((b >> 4) & 3);
			take_interrupt(vec);
			if (!(h8.syscr & 0x08)) {} // UI already handled
		}
		break;

		case 0x58: { // bcc d16
			INT16 d = (INT16)fetch16();
			if (cond(hi)) { h8.pc = (h8.pc + d) & ADDR_MASK; idle_branch(h8.pc); }
		}
		break;

		case 0x59: h8.pc = h8.er[hi & 7] & ADDR_MASK; break;      // jmp @ERn
		case 0x5a: h8.pc = ((b << 16) | fetch16()) & ADDR_MASK; break; // jmp @aa:24
		case 0x5b: h8.pc = rl(b) & ADDR_MASK; break;              // jmp @@aa:8
		case 0x5c: { // bsr d16
			INT16 d = (INT16)fetch16();
			push_pc32(h8.pc);
			h8.pc = (h8.pc + d) & ADDR_MASK;
		}
		break;
		case 0x5d: { UINT32 t = h8.er[hi & 7]; push_pc32(h8.pc); h8.pc = t & ADDR_MASK; } break;
		case 0x5e: { UINT32 t = (b << 16) | fetch16(); push_pc32(h8.pc); h8.pc = t & ADDR_MASK; } break;
		case 0x5f: { UINT32 t = rl(b); push_pc32(h8.pc); h8.pc = t & ADDR_MASK; } break;

		case 0x60: case 0x61: case 0x62: case 0x63: { // bset/bnot/bclr/btst Rn,Rd
			INT32 wr;
			UINT8 v = bitop(op - 0x60, r8(hi), r8(lo), &wr);
			if (wr) w8(lo, v);
		}
		break;

		case 0x64: { UINT16 v = r16(lo) | r16(hi); w16(lo, v); set_nzv16(v); } break;
		case 0x65: { UINT16 v = r16(lo) ^ r16(hi); w16(lo, v); set_nzv16(v); } break;
		case 0x66: { UINT16 v = r16(lo) & r16(hi); w16(lo, v); set_nzv16(v); } break;

		case 0x67: { // bst/bist #imm,Rd
			INT32 wr;
			UINT8 v = bitop((b & 0x80) ? 0xd : 0xc, hi & 7, r8(lo), &wr);
			w8(lo, v);
		}
		break;

		case 0x68: case 0x6a: case 0x6c: case 0x6e:
			mov_mem_group(w0, 0);
		break;
		case 0x69: case 0x6b: case 0x6d: case 0x6f:
			mov_mem_group(w0, 1);
		break;

		case 0x70: case 0x71: case 0x72: case 0x73: { // bset/bnot/bclr/btst #imm,Rd
			INT32 wr;
			UINT8 v = bitop(op - 0x70, hi & 7, r8(lo), &wr);
			if (wr) w8(lo, v);
		}
		break;
		case 0x74: case 0x75: case 0x76: case 0x77: {
			static const INT32 base[4] = { 0x4, 0x6, 0x8, 0xa };
			INT32 wr;
			bitop(base[op - 0x74] + ((b & 0x80) ? 1 : 0), hi & 7, r8(lo), &wr);
		}
		break;

		case 0x78: { // mov.b/w @(d24,ERs)
			UINT16 w1 = fetch16();
			UINT32 disp = fetch32();
			INT32 ern = hi & 7;
			INT32 rr = w1 & 0xf;
			INT32 size = ((w1 >> 8) == 0x6b) ? 1 : 0;
			UINT32 ea = (h8.er[ern] + disp) & ADDR_MASK;
			if (w1 & 0x80) mov_store(size, rr, ea);
			else mov_load(size, rr, ea);
		}
		break;

		case 0x79: { // word immediate
			UINT16 imm = fetch16();
			switch (hi) {
				case 0: w16(lo, imm); set_nzv16(imm); break;
				case 1: w16(lo, do_add16(r16(lo), imm)); break;
				case 2: do_sub16(r16(lo), imm); break;
				case 3: w16(lo, do_sub16(r16(lo), imm)); break;
				case 4: { UINT16 v = r16(lo) | imm; w16(lo, v); set_nzv16(v); } break;
				case 5: { UINT16 v = r16(lo) ^ imm; w16(lo, v); set_nzv16(v); } break;
				case 6: { UINT16 v = r16(lo) & imm; w16(lo, v); set_nzv16(v); } break;
				default: illegal(); break;
			}
		}
		break;

		case 0x7a: { // long immediate
			UINT32 imm = fetch32();
			switch (hi) {
				case 0: w32(lo, imm); set_nzv32(imm); break;
				case 1: w32(lo, do_add32(r32(lo), imm)); break;
				case 2: do_sub32(r32(lo), imm); break;
				case 3: w32(lo, do_sub32(r32(lo), imm)); break;
				case 4: { UINT32 v = r32(lo) | imm; w32(lo, v); set_nzv32(v); } break;
				case 5: { UINT32 v = r32(lo) ^ imm; w32(lo, v); set_nzv32(v); } break;
				case 6: { UINT32 v = r32(lo) & imm; w32(lo, v); set_nzv32(v); } break;
				default: illegal(); break;
			}
		}
		break;

		case 0x7b: { // eepmov
			UINT16 w1 = fetch16();
			if (w0 == 0x7b5c && w1 == 0x598f) {
				while (r8(4 + 8)) {
					wb(r32(6), rb(r32(5)));
					w32(5, r32(5) + 1);
					w32(6, r32(6) + 1);
					w8(4 + 8, r8(4 + 8) - 1);
				}
			} else if (w0 == 0x7bd4 && w1 == 0x598f) {
				while (r16(4)) {
					wb(r32(6), rb(r32(5)));
					w32(5, r32(5) + 1);
					w32(6, r32(6) + 1);
					w16(4, r16(4) - 1);
				}
			} else illegal();
		}
		break;

		case 0x7c: case 0x7d: { // bit ops on @ERd
			UINT16 w1 = fetch16();
			INT32 bit, ok;
			INT32 bop = decode_bit_insn(w1, &bit, &ok);
			if (!ok) { illegal(); break; }
			UINT32 ea = h8.er[hi & 7] & ADDR_MASK;
			INT32 wr;
			UINT8 v = bitop(bop, bit, rb(ea), &wr);
			if (wr && op == 0x7d) wb(ea, v);
		}
		break;

		case 0x7e: case 0x7f: { // bit ops on @aa:8
			UINT16 w1 = fetch16();
			INT32 bit, ok;
			INT32 bop = decode_bit_insn(w1, &bit, &ok);
			if (!ok) { illegal(); break; }
			UINT32 ea = abs8(b);
			INT32 wr;
			UINT8 v = bitop(bop, bit, rb(ea), &wr);
			if (wr && op == 0x7f) wb(ea, v);
		}
		break;

		default:
			switch (op >> 4) {
				case 0x8: w8(op & 0xf, do_add8(r8(op & 0xf), b)); break;
				case 0x9: w8(op & 0xf, do_addx8(r8(op & 0xf), b)); break;
				case 0xa: do_sub8(r8(op & 0xf), b); break;
				case 0xb: w8(op & 0xf, do_subx8(r8(op & 0xf), b)); break;
				case 0xc: { UINT8 v = r8(op & 0xf) | b; w8(op & 0xf, v); set_nzv8(v); } break;
				case 0xd: { UINT8 v = r8(op & 0xf) ^ b; w8(op & 0xf, v); set_nzv8(v); } break;
				case 0xe: { UINT8 v = r8(op & 0xf) & b; w8(op & 0xf, v); set_nzv8(v); } break;
				case 0xf: w8(op & 0xf, b); set_nzv8(b); break;
				default: illegal(); break;
			}
		break;
	}

	(void)ppc;
}

static void process_events()
{
	irq_dirty = 1;
	UINT64 now = h8.total_cycles;

	timers_sync(now);

	if (h8.adc_event <= now) {
		adc_convert();
		h8.adc_event = ~(UINT64)0;
		if (h8.adcsr & 0x10) {
			// scan mode keeps converting
			h8.adcsr |= 0x20;
			adc_start();
		}
	}

	update_events();
}

INT32 H8Run(INT32 nCycles)
{
	nCyclesToDo = nCycles;
	nCyclesLeft = nCycles;
	bEndRun = 0;

	while (nCyclesLeft > 0 && !bEndRun)
	{
		if (h8.total_cycles >= next_event) process_events();

		inst_cycles = 0;

		// interrupts
		if (h8.nmi_pending) {
			h8.nmi_pending = 0;
			take_interrupt(7);
		} else {
			if (irq_dirty) {
				INT32 prio;
				INT32 vec = irq_pending_vector(&prio);
				if (vec < 0) irq_dirty = 0; // nothing requested until state changes again
				else if (irq_allowed(prio)) take_interrupt(vec);
			}
		}

		if (h8.sleeping) {
			// skip ahead to the next event
			INT32 skip = nCyclesLeft;
			if (next_event != ~(UINT64)0 && next_event > h8.total_cycles) {
				UINT64 d = next_event - h8.total_cycles;
				if (d < (UINT64)skip) skip = (INT32)d;
			}
			if (skip < 1) skip = 1;
			h8.total_cycles += skip;
			nCyclesLeft -= skip;
			continue;
		}

		if (inst_cycles == 0) {
			if (h8.pc == idle_pc) {
				if (idle_valid && idle_inst <= 32 && mem_writes == idle_writes &&
					idle_regs[8] == h8.ccr && memcmp(idle_regs, h8.er, sizeof(h8.er)) == 0)
				{
					if (++idle_count >= 3) {
						UINT64 end = h8.total_cycles + nCyclesLeft;
						if (next_event < end) end = next_event;
						if (end > h8.total_cycles + 2) {
							INT32 n = (INT32)(end - h8.total_cycles - 2);
							h8.total_cycles += n;
							nCyclesLeft -= n;
						}
						idle_count = 0;
					}
				} else {
					idle_count = 0;
				}
				memcpy(idle_regs, h8.er, sizeof(h8.er));
				idle_regs[8] = h8.ccr;
				idle_writes = mem_writes;
				idle_inst = 0;
				idle_valid = 1;
			}
			idle_inst++;
			cur_inst_pc = h8.pc;
			execute_one();
		}

		if (inst_cycles < 2) inst_cycles = 2;
		h8.total_cycles += inst_cycles;
		nCyclesLeft -= inst_cycles;
	}

	INT32 ran = nCyclesToDo - nCyclesLeft;
	nCyclesToDo = nCyclesLeft = 0;
	return ran;
}

void H8RunEnd()
{
	bEndRun = 1;
}

INT32 H8Idle(INT32 nCycles)
{
	h8.total_cycles += nCycles;
	return nCycles;
}

INT32 H8TotalCycles()
{
	return (INT32)(h8.total_cycles + inst_cycles - h8.frame_start);
}

void H8NewFrame()
{
	h8.frame_start = h8.total_cycles;
}

void H8SetIRQLine(INT32 nLine, INT32 nState)
{
	irq_dirty = 1;
	if (nLine == H8_NMI) {
		if (nState) h8.nmi_pending = 1;
		return;
	}

	if (nLine < 0 || nLine > 5) return;
	UINT8 m = 1 << nLine;
	UINT8 old = h8.irq_level;

	if (nState) h8.irq_level |= m;
	else h8.irq_level &= ~m;

	if (h8.iscr & m) {
		// edge triggered (falling edge of the active low pin == assertion)
		if (nState && !(old & m)) h8.isr |= m;
	} else {
		// level triggered: latched in ISR on the H8/300H
		if (nState) h8.isr |= m;
	}

	if (nState) h8.sleeping = 0;
}

UINT32 H8GetPC()
{
	return h8.pc;
}

// setup
void H8MapMemory(UINT8 *pMem, UINT32 nStart, UINT32 nEnd, INT32 nType)
{
	for (UINT32 a = nStart & ~0xffff; a <= nEnd; a += 0x10000) {
		UINT8 *p = pMem ? pMem + (a - nStart) : NULL;
		if (nType & H8_READ) mem_read[a >> 16] = p;
		if (nType & H8_WRITE) mem_write[a >> 16] = p;
		if (a >= 0xff0000) break;
	}
}

void H8SetReadByteHandler(UINT8 (*pHandler)(UINT32)) { pReadByte = pHandler; }
void H8SetReadWordHandler(UINT16 (*pHandler)(UINT32)) { pReadWord = pHandler; }
void H8SetWriteByteHandler(void (*pHandler)(UINT32, UINT8)) { pWriteByte = pHandler; }
void H8SetWriteWordHandler(void (*pHandler)(UINT32, UINT16)) { pWriteWord = pHandler; }
void H8SetReadPortHandler(UINT8 (*pHandler)(INT32)) { pReadPort = pHandler; }
void H8SetWritePortHandler(void (*pHandler)(INT32, UINT8)) { pWritePort = pHandler; }
void H8SetAdcReadHandler(UINT16 (*pHandler)(INT32)) { pAdcRead = pHandler; }
void H8SetSciTxHandler(void (*pHandler)(INT32, UINT8)) { pSciTx = pHandler; }
void H8SetSciRxHandler(INT32 (*pHandler)(INT32)) { pSciRx = pHandler; }

void H8Init(INT32)
{
	memset(mem_read, 0, sizeof(mem_read));
	memset(mem_write, 0, sizeof(mem_write));
	pReadByte = NULL; pReadWord = NULL;
	pWriteByte = NULL; pWriteWord = NULL;
	pReadPort = NULL; pWritePort = NULL;
	pAdcRead = NULL; pSciTx = NULL; pSciRx = NULL;
	memset(&h8, 0, sizeof(h8));
}

void H8Exit()
{
}

void H8Open(INT32) {}
void H8Close() {}
INT32 H8GetActive() { return 0; }

void H8Reset()
{
	irq_dirty = 1;
	UINT64 tc = h8.total_cycles, fs = h8.frame_start;
	UINT8 ram[0x200];
	memcpy(ram, h8.onchip_ram, sizeof(ram));

	memset(&h8, 0, sizeof(h8));
	h8.total_cycles = tc;
	h8.frame_start = fs;
	memcpy(h8.onchip_ram, ram, sizeof(ram));

	h8.syscr = 0x0b;
	h8.ipra = h8.iprb = 0;
	for (INT32 i = 0; i < 5; i++) {
		h8.tmr[i].gra = h8.tmr[i].grb = 0xffff;
		h8.tmr[i].bra = h8.tmr[i].brb = 0xffff;
		h8.tmr[i].last = h8.total_cycles;
	}
	for (INT32 i = 0; i < 2; i++) {
		h8.sci[i].brr = 0xff;
		h8.sci[i].tdr = 0xff;
		h8.sci[i].ssr = 0x84;
	}
	h8.adcsr = 0;
	h8.adcr = 0;
	h8.adc_event = ~(UINT64)0;

	h8.ccr = F_I;
	inst_cycles = 0;
	h8.pc = rl(0) & ADDR_MASK;
	inst_cycles = 0;
	idle_pc = ~0U;
	idle_valid = 0;
	update_events();
}

INT32 H8Scan(INT32 nAction)
{
	irq_dirty = 1;
	if (nAction & ACB_VOLATILE) {
		ScanVar(&h8, sizeof(h8), "H8/3002");

		if (nAction & ACB_WRITE) {
			idle_pc = ~0U;
			idle_valid = 0;
			update_events();
		}
	}
	return 0;
}

UINT8 H8CheatRead(UINT32 a)
{
	a &= ADDR_MASK;
	if (is_onchip_ram(a)) return h8.onchip_ram[a - 0xfffd10];
	UINT8 *p = mem_read[a >> 16];
	return p ? p[a & 0xffff] : 0;
}

void H8CheatWrite(UINT32 a, UINT8 d)
{
	a &= ADDR_MASK;
	if (is_onchip_ram(a)) { h8.onchip_ram[a - 0xfffd10] = d; return; }
	UINT8 *p = mem_write[a >> 16];
	if (p) p[a & 0xffff] = d;
}

static void core_set_irq(INT32, INT32 line, INT32 state)
{
	H8SetIRQLine(line, state);
}

cpu_core_config H8Config =
{
	"H8/3002",
	H8Open,
	H8Close,
	H8CheatRead,
	H8CheatWrite,
	H8GetActive,
	H8TotalCycles,
	H8NewFrame,
	H8Idle,
	core_set_irq,
	H8Run,
	H8RunEnd,
	H8Reset,
	H8Scan,
	H8Exit,
	0x1000000,
	0
};
