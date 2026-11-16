// RV32IM + Zicsr interpreter, machine mode only, with the NEORV32 (1.4.x) interrupt model

#ifndef RV32_H
#define RV32_H

#define RV32_IRQ_MSI	(1 << 3)
#define RV32_IRQ_MTI	(1 << 7)
#define RV32_IRQ_MEI	(1 << 11)

// plain little endian memory, read directly instead of through the callbacks
struct rv32_region {
	UINT32 base, size;
	UINT8 *mem;
	INT32 cost;
};

struct rv32_bus {
	// size: 1, 2 or 4; addresses are aligned. *err set to 1 = bus error.
	UINT32 (*read)(UINT32 addr, INT32 size, INT32 *cost, INT32 *err);
	void   (*write)(UINT32 addr, UINT32 data, INT32 size, INT32 *cost, INT32 *err);
	UINT32 (*fetch)(UINT32 addr, INT32 *cost, INT32 *err);

	const rv32_region *fetch_regions;	// optional
	INT32 nfetch;
	const rv32_region *read_regions;	// optional
	INT32 nread;
};

struct rv32_state {
	UINT32 x[32];
	UINT32 pc;

	UINT32 mstatus, mie, mtvec, mscratch, mepc, mcause, mtval;
	UINT32 irq_buf;		// NEORV32 pending buffer
	UINT32 irq_lines;	// external levels (RV32_IRQ_MEI, RV32_IRQ_MSI)

	// timer line: TEN & ((TIMER - TCMP) >= 0), TIMER = cycles - timer_epoch
	UINT32 ten, tcmp;
	UINT64 timer_epoch;

	UINT64 cycles;		// total clocks
	UINT64 mcycle_base, minstret_base;
	UINT64 instret;

	INT32 halted;		// stuck on a fatal trap loop guard (informational)

	// rv32_run also returns when it reaches stop_pc after having retired an instruction since stop_from
	UINT32 stop_pc;
	UINT64 stop_from;
	INT32 irq_inhibit;	// no interrupt is taken while set
};

void rv32_init(rv32_state *s, rv32_bus *bus);
void rv32_exit();
void rv32_reset(rv32_state *s);
// runs until s->cycles >= target
void rv32_run(rv32_state *s, UINT64 target);
UINT32 rv32_timer(rv32_state *s);
// while set, every change of flow (jump, taken or not taken branch, trap, mret) is hashed into *hash
void rv32_trace(UINT32 *hash);

#endif
