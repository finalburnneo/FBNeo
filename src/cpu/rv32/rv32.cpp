// RV32IM + Zicsr interpreter, machine mode only, with the NEORV32 (1.4.x) interrupt model
//
// Cycle costs follow the NEORV32 multi-cycle core with tightly coupled memories: the bus
// callbacks return the cost of each access, the execute part is added here.

#include "burnint.h"
#include "rv32.h"

static rv32_bus *bus;
static UINT32 *trace;

void rv32_trace(UINT32 *hash)
{
	trace = hash;
}

static inline void trace_pc(UINT32 pc)
{
	if (trace) *trace = (*trace ^ pc) * 0x01000193;
}

#define EXEC_ALU	1
#define EXEC_BRANCH	3
#define EXEC_MUL	3
#define EXEC_DIV	33
#define EXEC_CSR	2
#define EXEC_TRAP	14
#define EXEC_MRET	13

#define CAUSE_IMA	0x00000000	// instruction address misaligned
#define CAUSE_IAF	0x00000001	// instruction access fault
#define CAUSE_ILL	0x00000002
#define CAUSE_BRK	0x00000003
#define CAUSE_LMA	0x00000004
#define CAUSE_LAF	0x00000005
#define CAUSE_SMA	0x00000006
#define CAUSE_SAF	0x00000007
#define CAUSE_ECALL	0x0000000b

void rv32_init(rv32_state *s, rv32_bus *b)
{
	bus = b;
	memset(s, 0, sizeof(*s));
	s->stop_pc = 0xffffffff;
}

void rv32_reset(rv32_state *s)
{
	UINT64 cyc = s->cycles;
	memset(s, 0, sizeof(*s));
	s->cycles = cyc;
	s->timer_epoch = cyc;
	s->mcycle_base = cyc;
	s->mstatus = 0x1800;
	s->stop_pc = 0xffffffff;
}

UINT32 rv32_timer(rv32_state *s)
{
	return (UINT32)(s->cycles - s->timer_epoch);
}

static void take_trap(rv32_state *s, UINT32 cause, UINT32 epc, UINT32 tval)
{
	s->mepc = epc;
	s->mcause = cause;
	s->mtval = tval;
	UINT32 mie = (s->mstatus >> 3) & 1;
	s->mstatus = (s->mstatus & ~0x88) | (mie << 7) | 0x1800;
	s->pc = s->mtvec & ~3;
	s->cycles += EXEC_TRAP;
	trace_pc(s->pc);
}

static UINT32 csr_read(rv32_state *s, UINT32 csr, INT32 *ok)
{
	*ok = 1;
	switch (csr) {
		case 0x300: return s->mstatus | 0x1800;
		case 0x301: return 0x40001104;		// rv32 i m
		case 0x304: return s->mie;
		case 0x305: return s->mtvec;
		case 0x340: return s->mscratch;
		case 0x341: return s->mepc;
		case 0x342: return s->mcause;
		case 0x343: return s->mtval;
		case 0x344: return s->irq_buf;
		case 0xb00: case 0xc00: return (UINT32)(s->cycles - s->mcycle_base);
		case 0xb80: case 0xc80: return (UINT32)((s->cycles - s->mcycle_base) >> 32);
		case 0xb02: case 0xc02: return (UINT32)(s->instret - s->minstret_base);
		case 0xb82: case 0xc82: return (UINT32)((s->instret - s->minstret_base) >> 32);
		case 0xc01: case 0xc81: return 0;	// no mtime unit
		case 0xf11: case 0xf12: case 0xf13: case 0xf14: return 0;
	}
	return 0;
}

static void csr_write(rv32_state *s, UINT32 csr, UINT32 v)
{
	switch (csr) {
		case 0x300: s->mstatus = (v & 0x88) | 0x1800; break;
		case 0x304: s->mie = v & (RV32_IRQ_MSI | RV32_IRQ_MTI | RV32_IRQ_MEI); s->irq_buf &= s->mie; break;
		case 0x305: s->mtvec = v & ~3; break;
		case 0x340: s->mscratch = v; break;
		case 0x341: s->mepc = v & ~3; break;
		case 0x342: s->mcause = v; break;
		case 0x343: s->mtval = v; break;
		case 0xb00: { UINT64 cur = s->cycles - s->mcycle_base; cur = (cur & 0xffffffff00000000ULL) | v; s->mcycle_base = s->cycles - cur; } break;
		case 0xb80: { UINT64 cur = s->cycles - s->mcycle_base; cur = (cur & 0xffffffffULL) | ((UINT64)v << 32); s->mcycle_base = s->cycles - cur; } break;
		case 0xb02: { UINT64 cur = s->instret - s->minstret_base; cur = (cur & 0xffffffff00000000ULL) | v; s->minstret_base = s->instret - cur; } break;
		case 0xb82: { UINT64 cur = s->instret - s->minstret_base; cur = (cur & 0xffffffffULL) | ((UINT64)v << 32); s->minstret_base = s->instret - cur; } break;
	}
}

static inline INT32 sext(UINT32 v, INT32 bits)
{
	return (INT32)(v << (32 - bits)) >> (32 - bits);
}

// decoded instructions, cached per fetch region (checked against the word at each fetch)
enum {
	K_ILL_COST = 0,		// illegal, after the fetch cost (0: a blank cache entry holds the decoding of word 0)
	K_ILL,				// illegal, before any cost
	K_LUI, K_AUIPC, K_JAL, K_JALR,
	K_BEQ, K_BNE, K_BLT, K_BGE, K_BLTU, K_BGEU,
	K_LB, K_LH, K_LW, K_LBU, K_LHU, K_LBAD,
	K_SB, K_SH, K_SW,
	K_ADDI, K_SLLI, K_SLTI, K_SLTIU, K_XORI, K_SRLI, K_SRAI, K_ORI, K_ANDI,
	K_ADD, K_SUB, K_SLL, K_SLT, K_SLTU, K_XOR, K_SRL, K_SRA, K_OR, K_AND,
	K_MUL, K_MULH, K_MULHSU, K_MULHU, K_DIV, K_DIVU, K_REM, K_REMU,
	K_FENCE, K_ECALL, K_EBREAK, K_MRET, K_WFI, K_SYS_ILL, K_CSR
};

struct rv32_dec {
	UINT32 op;
	UINT8 kind, rd, rs1, rs2;	// K_CSR: rs2 = funct3
	INT32 imm;					// K_CSR: the csr
};

#define RV32_MAX_REGIONS	8
static rv32_dec *dec_tab[RV32_MAX_REGIONS];
static const rv32_region *dec_regions = NULL;

void rv32_exit()
{
	for (INT32 i = 0; i < RV32_MAX_REGIONS; i++) {
		BurnFree(dec_tab[i]);
	}
	dec_regions = NULL;
}

static void decode(rv32_dec *d, UINT32 op)
{
	static const UINT8 load[8]  = { K_LB, K_LH, K_LW, K_LBAD, K_LBU, K_LHU, K_LBAD, K_LBAD };
	static const UINT8 store[8] = { K_SB, K_SH, K_SW, K_ILL, K_ILL, K_ILL, K_ILL, K_ILL };
	static const UINT8 bra[8]   = { K_BEQ, K_BNE, K_ILL, K_ILL, K_BLT, K_BGE, K_BLTU, K_BGEU };
	static const UINT8 alui[8]  = { K_ADDI, K_SLLI, K_SLTI, K_SLTIU, K_XORI, K_SRLI, K_ORI, K_ANDI };
	static const UINT8 alu[8]   = { K_ADD, K_SLL, K_SLT, K_SLTU, K_XOR, K_SRL, K_OR, K_AND };
	static const UINT8 mul[8]   = { K_MUL, K_MULH, K_MULHSU, K_MULHU, K_DIV, K_DIVU, K_REM, K_REMU };

	UINT32 f3 = (op >> 12) & 7;
	d->op = op;
	d->rd = (op >> 7) & 31;
	d->rs1 = (op >> 15) & 31;
	d->rs2 = (op >> 20) & 31;
	d->imm = 0;

	switch (op & 0x7f) {
		case 0x37: d->kind = K_LUI;   d->imm = op & 0xfffff000; break;
		case 0x17: d->kind = K_AUIPC; d->imm = op & 0xfffff000; break;
		case 0x6f: d->kind = K_JAL;   d->imm = sext(((op >> 31) << 20) | (((op >> 12) & 0xff) << 12) | (((op >> 20) & 1) << 11) | (((op >> 21) & 0x3ff) << 1), 21); break;
		case 0x67: d->kind = K_JALR;  d->imm = sext(op >> 20, 12); break;
		case 0x63: d->kind = bra[f3]; d->imm = sext(((op >> 31) << 12) | (((op >> 7) & 1) << 11) | (((op >> 25) & 0x3f) << 5) | (((op >> 8) & 0xf) << 1), 13); break;
		case 0x03: d->kind = load[f3]; d->imm = sext(op >> 20, 12); break;
		case 0x23: d->kind = store[f3]; d->imm = sext(((op >> 25) << 5) | ((op >> 7) & 0x1f), 12); break;
		case 0x13:
			d->kind = alui[f3];
			if (f3 == 5 && (op & 0x40000000)) d->kind = K_SRAI;
			d->imm = sext(op >> 20, 12);
			break;
		case 0x33:
			if ((op >> 25) == 1) d->kind = mul[f3];
			else {
				d->kind = alu[f3];
				if (op & 0x40000000) {
					if (f3 == 0) d->kind = K_SUB;
					if (f3 == 5) d->kind = K_SRA;
				}
			}
			break;
		case 0x0f: d->kind = K_FENCE; break;
		case 0x73:
			if (f3 == 0) {
				d->kind = (op == 0x00000073) ? K_ECALL : (op == 0x00100073) ? K_EBREAK : (op == 0x30200073) ? K_MRET : (op == 0x10500073) ? K_WFI : K_SYS_ILL;
			} else {
				d->kind = K_CSR;
				d->rs2 = f3;
				d->imm = op >> 20;
			}
			break;
		default: d->kind = K_ILL_COST; break;
	}
}

static inline INT32 find_region(const rv32_region *r, INT32 n, UINT32 addr, INT32 size)
{
	for (INT32 i = 0; i < n; i++) {
		if (addr - r[i].base <= r[i].size - size) return i;
	}
	return -1;
}

void rv32_run(rv32_state *s, UINT64 target)
{
	const rv32_region *fregs = bus->fetch_regions, *rregs = bus->read_regions;
	const INT32 nfetch = fregs ? ((bus->nfetch < RV32_MAX_REGIONS) ? bus->nfetch : RV32_MAX_REGIONS) : 0;
	const INT32 nread = rregs ? bus->nread : 0;
	const rv32_region *fcur = NULL;
	rv32_dec *fdec = NULL;
	rv32_dec tmp;

	if (fregs != dec_regions) {
		rv32_exit();
		for (INT32 i = 0; i < nfetch; i++) {
			dec_tab[i] = (rv32_dec*)BurnMalloc((fregs[i].size / 4) * sizeof(rv32_dec));
			memset(dec_tab[i], 0, (fregs[i].size / 4) * sizeof(rv32_dec));
		}
		dec_regions = fregs;
	}

	// the interrupt state can only change with time (the timer line), a bus access through the
	// callbacks or a csr / trap / mret, so it is looked at again only then
	UINT64 irq_check = 0;
	UINT64 cycles = s->cycles, instret = s->instret;

#define SYNC_OUT()	do { s->cycles = cycles; s->instret = instret; } while (0)
#define TRAP(c, e, t)	{ SYNC_OUT(); take_trap(s, c, e, t); cycles = s->cycles; continue; }

	while (cycles < target) {
		if (cycles >= irq_check) {
			// interrupt buffer (NEORV32: a level is caught and held until taken or masked)
			UINT32 lines = s->irq_lines;
			irq_check = ~(UINT64)0;
			if (s->ten) {
				INT32 d = (INT32)((UINT32)(cycles - s->timer_epoch) - s->tcmp);
				if (d >= 0) lines |= RV32_IRQ_MTI;
				else if (s->mie & RV32_IRQ_MTI) irq_check = cycles + (UINT32)-d;
			}
			s->irq_buf = s->mie & (s->irq_buf | lines);

			if (s->irq_buf) {
				irq_check = 0;
				if ((s->mstatus & 0x08) && !s->irq_inhibit) {
					UINT32 bit, cause;
					if (s->irq_buf & RV32_IRQ_MEI)      { bit = RV32_IRQ_MEI; cause = 0x8000000b; }
					else if (s->irq_buf & RV32_IRQ_MTI) { bit = RV32_IRQ_MTI; cause = 0x80000007; }
					else                                { bit = RV32_IRQ_MSI; cause = 0x80000003; }
					s->irq_buf &= ~bit;
					TRAP(cause, s->pc, 0)
				}
			}
		}

		INT32 cost, err = 0;
		UINT32 pc = s->pc;
		if (pc == s->stop_pc && instret != s->stop_from) break;
		if (pc & 3) { TRAP(CAUSE_IMA, pc, pc) }

		const rv32_dec *d;
		UINT32 off = pc - (fcur ? fcur->base : 0);
		if (!fcur || off > fcur->size - 4) {
			INT32 i = find_region(fregs, nfetch, pc, 4);
			if (i >= 0) {
				fcur = &fregs[i];
				fdec = dec_tab[i];
				off = pc - fcur->base;
			} else {
				fcur = NULL;
			}
		}
		if (fcur) {
			UINT32 op = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(fcur->mem + off));
			rv32_dec *e = fdec + (off >> 2);
			if (e->op != op) decode(e, op);
			d = e;
			cost = fcur->cost;
		} else {
			cost = 0;
			SYNC_OUT();
			UINT32 op = bus->fetch(pc, &cost, &err);
			if (err) { cycles += cost; TRAP(CAUSE_IAF, pc, pc) }
			decode(&tmp, op);
			d = &tmp;
		}

		const UINT32 a = s->x[d->rs1], b = s->x[d->rs2];
		UINT32 npc = pc + 4;
		UINT32 res = 0;
		INT32 wb = 1;

		switch (d->kind) {
			case K_LUI:   res = d->imm; cost += EXEC_ALU; break;
			case K_AUIPC: res = pc + d->imm; cost += EXEC_ALU; break;

			case K_JAL:  res = npc; npc = pc + d->imm; cost += EXEC_BRANCH; trace_pc(npc); break;
			case K_JALR: res = npc; npc = (a + d->imm) & ~1; cost += EXEC_BRANCH; trace_pc(npc); break;

#define BRANCH(cond)	wb = 0; if (cond) { npc = pc + d->imm; cost += EXEC_BRANCH; } else cost += EXEC_ALU; trace_pc(npc); break;
			case K_BEQ:  BRANCH(a == b)
			case K_BNE:  BRANCH(a != b)
			case K_BLT:  BRANCH((INT32)a < (INT32)b)
			case K_BGE:  BRANCH((INT32)a >= (INT32)b)
			case K_BLTU: BRANCH(a < b)
			case K_BGEU: BRANCH(a >= b)
#undef BRANCH

			case K_LB: case K_LH: case K_LW: case K_LBU: case K_LHU: case K_LBAD: {
				UINT32 addr = a + d->imm;
				INT32 size = (d->kind == K_LB || d->kind == K_LBU) ? 1 : (d->kind == K_LH || d->kind == K_LHU) ? 2 : 4;
				if (addr & (size - 1)) { cycles += cost; TRAP(CAUSE_LMA, pc, addr) }
				UINT32 v;
				INT32 r = find_region(rregs, nread, addr, size);
				if (r >= 0) {
					const UINT8 *m = rregs[r].mem + (addr - rregs[r].base);
					v = (size == 4) ? BURN_ENDIAN_SWAP_INT32(*(UINT32*)m) : (size == 2) ? (UINT32)(m[0] | (m[1] << 8)) : m[0];
					cost += rregs[r].cost;
				} else {
					SYNC_OUT();
					v = bus->read(addr, size, &cost, &err);
					irq_check = 0;
					if (err) { cycles += cost; TRAP(CAUSE_LAF, pc, addr) }
				}
				switch (d->kind) {
					case K_LB:  res = sext(v & 0xff, 8); break;
					case K_LH:  res = sext(v & 0xffff, 16); break;
					case K_LW:  res = v; break;
					case K_LBU: res = v & 0xff; break;
					case K_LHU: res = v & 0xffff; break;
					default: TRAP(CAUSE_ILL, pc, d->op)
				}
			} break;

			case K_SB: case K_SH: case K_SW: {
				UINT32 addr = a + d->imm;
				INT32 size = (d->kind == K_SB) ? 1 : (d->kind == K_SH) ? 2 : 4;
				if (addr & (size - 1)) { cycles += cost; TRAP(CAUSE_SMA, pc, addr) }
				SYNC_OUT();
				bus->write(addr, size == 4 ? b : size == 2 ? (b & 0xffff) : (b & 0xff), size, &cost, &err);
				irq_check = 0;
				if (err) { cycles += cost; TRAP(CAUSE_SAF, pc, addr) }
				wb = 0;
			} break;

			case K_ADDI:  res = a + d->imm; cost += EXEC_ALU; break;
			case K_SLLI:  res = a << d->rs2; cost += EXEC_ALU; break;
			case K_SLTI:  res = (INT32)a < d->imm; cost += EXEC_ALU; break;
			case K_SLTIU: res = a < (UINT32)d->imm; cost += EXEC_ALU; break;
			case K_XORI:  res = a ^ d->imm; cost += EXEC_ALU; break;
			case K_SRLI:  res = a >> d->rs2; cost += EXEC_ALU; break;
			case K_SRAI:  res = (UINT32)((INT32)a >> d->rs2); cost += EXEC_ALU; break;
			case K_ORI:   res = a | d->imm; cost += EXEC_ALU; break;
			case K_ANDI:  res = a & d->imm; cost += EXEC_ALU; break;

			case K_ADD:  res = a + b; cost += EXEC_ALU; break;
			case K_SUB:  res = a - b; cost += EXEC_ALU; break;
			case K_SLL:  res = a << (b & 31); cost += EXEC_ALU; break;
			case K_SLT:  res = (INT32)a < (INT32)b; cost += EXEC_ALU; break;
			case K_SLTU: res = a < b; cost += EXEC_ALU; break;
			case K_XOR:  res = a ^ b; cost += EXEC_ALU; break;
			case K_SRL:  res = a >> (b & 31); cost += EXEC_ALU; break;
			case K_SRA:  res = (UINT32)((INT32)a >> (b & 31)); cost += EXEC_ALU; break;
			case K_OR:   res = a | b; cost += EXEC_ALU; break;
			case K_AND:  res = a & b; cost += EXEC_ALU; break;

			case K_MUL:    res = a * b; cost += EXEC_MUL; break;
			case K_MULH:   res = (UINT32)(((INT64)(INT32)a * (INT64)(INT32)b) >> 32); cost += EXEC_MUL; break;
			case K_MULHSU: res = (UINT32)(((INT64)(INT32)a * (INT64)(UINT64)b) >> 32); cost += EXEC_MUL; break;
			case K_MULHU:  res = (UINT32)(((UINT64)a * (UINT64)b) >> 32); cost += EXEC_MUL; break;
			case K_DIV:    res = (b == 0) ? 0xffffffff : (a == 0x80000000 && b == 0xffffffff) ? a : (UINT32)((INT32)a / (INT32)b); cost += EXEC_DIV; break;
			case K_DIVU:   res = (b == 0) ? 0xffffffff : a / b; cost += EXEC_DIV; break;
			case K_REM:    res = (b == 0) ? a : (a == 0x80000000 && b == 0xffffffff) ? 0 : (UINT32)((INT32)a % (INT32)b); cost += EXEC_DIV; break;
			case K_REMU:   res = (b == 0) ? a : a % b; cost += EXEC_DIV; break;

			case K_FENCE: wb = 0; cost += EXEC_ALU; break;

			case K_ECALL:  irq_check = 0; cycles += cost; TRAP(CAUSE_ECALL, pc, 0)
			case K_EBREAK: irq_check = 0; cycles += cost; TRAP(CAUSE_BRK, pc, pc)
			case K_MRET: {
				irq_check = 0;
				UINT32 mpie = (s->mstatus >> 7) & 1;
				s->mstatus = (s->mstatus & ~0x88) | (mpie << 3) | 0x80 | 0x1800;
				npc = s->mepc;
				cost += EXEC_MRET;
				trace_pc(npc);
				wb = 0;
			} break;
			case K_WFI: irq_check = 0; wb = 0; cost += EXEC_ALU; break;	// treated as a nop
			case K_SYS_ILL: irq_check = 0; TRAP(CAUSE_ILL, pc, d->op)

			case K_CSR: {
				irq_check = 0;
				UINT32 csr = d->imm, f3 = d->rs2, rs1 = d->rs1, ok;
				INT32 iok;
				SYNC_OUT();
				UINT32 old = csr_read(s, csr, &iok);
				UINT32 src = (f3 & 4) ? rs1 : a;
				ok = 1;
				switch (f3 & 3) {
					case 1: csr_write(s, csr, src); break;
					case 2: if (rs1) csr_write(s, csr, old | src); break;
					case 3: if (rs1) csr_write(s, csr, old & ~src); break;
					default: ok = 0; break;
				}
				if (!ok) { TRAP(CAUSE_ILL, pc, d->op) }
				res = old;
				cost += EXEC_CSR;
			} break;

			case K_ILL: TRAP(CAUSE_ILL, pc, d->op)

			default:	// K_ILL_COST
				cycles += cost;
				TRAP(CAUSE_ILL, pc, d->op)
		}

		if (wb && d->rd) s->x[d->rd] = res;
		s->pc = npc;
		cycles += cost;
		instret++;
	}

	SYNC_OUT();
#undef TRAP
#undef SYNC_OUT
}
