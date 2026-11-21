// Sega Virtua Processor (315-5750), used by Virtua Racing
// Samsung SSP1601 dsp core + programmable memory registers, based on ares (md/cartridge/board/svp.cpp, component/processor/ssp1601)
//
// The svp internal rom (0xfc00-0xffff) isn't required: its reset handler is emulated (see SVPReset()),
// its interrupt handlers aren't (Virtua Racing doesn't enable interrupts).

#include "burnint.h"
#include "m68000_intf.h"
#include "svp.h"

INT32 SVPMode = 0;
INT32 (*SVPMainCycles)() = NULL;

#define SVP_CLOCK	23000000
#define OSC_NTSC	53693175
#define OSC_PAL		53203424

#define PMC_HAVE_ADDRESS	0x0001
#define PMC_SET				0x0002

// status register
#define ST_RPL		(s.st & 7)
#define ST_USR0		0x0020
#define ST_USR1		0x0040
#define ST_OP		0x0100
#define ST_MACS		0x0200
#define ST_C		0x1000
#define ST_Z		0x2000
#define ST_V		0x4000
#define ST_N		0x8000

static UINT16 *rom;
static UINT32 rom_mask;
static UINT8 *mem;
static UINT16 *iram;	// 0x400 words
static UINT16 *dram;	// 0x10000 words, mapped at 0x300000 on the 68k side
static INT32 osc;

static struct {
	// ssp1601
	UINT16 ram[512];
	UINT16 frame[6];
	UINT16 x, y;
	UINT32 a;
	UINT16 st;
	UINT16 stack;
	UINT16 pc;
	UINT32 p;
	UINT8 r[8];

	// svp
	UINT16 pm0, pm1, pm2, xst, pm4;
	UINT32 pmc;
	UINT32 pmac[2][6];
	UINT32 status;
	INT32 halted;

	INT32 cyc;		// ssp cycles done in the current frame
} s;

#define ROM(a)		BURN_ENDIAN_SWAP_INT16(rom[(a) & rom_mask])
#define DRAM(a)		dram[(a) & 0xffff]
#define AH			(s.a >> 16)

static inline UINT16 dram_r(UINT32 a)			{ return BURN_ENDIAN_SWAP_INT16(DRAM(a)); }
static inline void dram_w(UINT32 a, UINT16 d)	{ DRAM(a) = BURN_ENDIAN_SWAP_INT16(d); }

// -----------------------------------------------------------------------------
// programmable memory registers

static INT32 pm_increment(UINT16 mode)
{
	INT32 inc = (mode >> 11) & 7;
	if (inc == 0) return 0;
	if (inc != 7) inc--;
	inc = 1 << inc;
	if (mode & 0x8000) inc = -inc;
	return inc;
}

static void pm_overwrite(UINT32 a, UINT16 d)
{
	UINT16 v = dram_r(a);
	if (d & 0xf000) v = (v & ~0xf000) | (d & 0xf000);
	if (d & 0x0f00) v = (v & ~0x0f00) | (d & 0x0f00);
	if (d & 0x00f0) v = (v & ~0x00f0) | (d & 0x00f0);
	if (d & 0x000f) v = (v & ~0x000f) | (d & 0x000f);
	dram_w(a, v);
}

// returns -1 when the register isn't in programmable mode
static INT32 pm_read(INT32 reg)
{
	if (s.status & PMC_SET) {
		s.pmac[0][reg] = s.pmc;
		s.status &= ~PMC_SET;
		return 0;
	}
	s.status &= ~PMC_HAVE_ADDRESS;

	if (reg == 4 || (s.st & 0x60)) {
		UINT16 addr = s.pmac[0][reg] & 0xffff;
		UINT16 mode = s.pmac[0][reg] >> 16;
		UINT16 d;
		if ((mode & 0xfff0) == 0x0800) {
			s.pmac[0][reg]++;
			d = ROM(addr | ((mode & 0xf) << 16));
		} else if ((mode & 0x47ff) == 0x0018) {
			d = dram_r(addr);
			s.pmac[0][reg] += pm_increment(mode);
		} else {
			d = 0;
		}
		s.pmc = s.pmac[0][reg];
		return d;
	}

	return -1;
}

static INT32 pm_write(INT32 reg, UINT16 d)
{
	if (s.status & PMC_SET) {
		s.pmac[1][reg] = s.pmc;
		s.status &= ~PMC_SET;
		return 0;
	}
	s.status &= ~PMC_HAVE_ADDRESS;

	if (reg == 4 || (s.st & 0x60)) {
		UINT16 addr = s.pmac[1][reg] & 0xffff;
		UINT16 mode = s.pmac[1][reg] >> 16;
		if ((mode & 0x43ff) == 0x0018) {
			if (mode & 0x0400) pm_overwrite(addr, d); else dram_w(addr, d);
			s.pmac[1][reg] += pm_increment(mode);
		} else if ((mode & 0xfbff) == 0x4018) {
			if (mode & 0x0400) pm_overwrite(addr, d); else dram_w(addr, d);
			s.pmac[1][reg] += (addr & 1) ? 31 : 1;
		} else if ((mode & 0x47ff) == 0x001c) {
			iram[addr & 0x3ff] = d;
			s.pmac[1][reg] += pm_increment(mode);
		}
		s.pmc = s.pmac[1][reg];
		return d;
	}

	return -1;
}

static UINT16 ext_read(INT32 r)
{
	INT32 d;

	switch (r) {
		case 0:
			if ((d = pm_read(0)) != -1) return d;
			d = s.pm0;
			s.pm0 &= ~2;
			return d;

		case 1:
			if ((d = pm_read(1)) != -1) return d;
			return s.pm1;

		case 2:
			if ((d = pm_read(2)) != -1) return d;
			return s.pm2;

		case 3:
			if ((d = pm_read(3)) != -1) return d;
			return s.xst;

		case 4:
			if ((d = pm_read(4)) != -1) return d;
			return s.pm4;

		case 6:
			if (s.status & PMC_HAVE_ADDRESS) {
				s.status |= PMC_SET;
				s.status &= ~PMC_HAVE_ADDRESS;
				return ((s.pmc << 4) & 0xfff0) | ((s.pmc >> 4) & 0x000f);
			}
			s.status |= PMC_HAVE_ADDRESS;
			return s.pmc & 0xffff;

		case 7:
			s.status &= ~(PMC_HAVE_ADDRESS | PMC_SET);
			break;
	}

	return 0;
}

static void ext_write(INT32 r, UINT16 d)
{
	switch (r) {
		case 0: if (pm_write(0, d) == -1) s.pm0 = d; break;
		case 1: if (pm_write(1, d) == -1) s.pm1 = d; break;
		case 2: if (pm_write(2, d) == -1) s.pm2 = d; break;
		case 3:
			if (pm_write(3, d) == -1) {
				s.pm0 |= 1;
				s.xst = d;
			}
			break;
		case 4: if (pm_write(4, d) == -1) s.pm4 = d; break;

		case 6:
			if (s.status & PMC_HAVE_ADDRESS) {
				s.status |= PMC_SET;
				s.status &= ~PMC_HAVE_ADDRESS;
				s.pmc = (s.pmc & 0x0000ffff) | (d << 16);
			} else {
				s.status |= PMC_HAVE_ADDRESS;
				s.pmc = (s.pmc & 0xffff0000) | d;
			}
			break;
	}
}

// -----------------------------------------------------------------------------
// ssp1601

static inline UINT16 ssp_read(UINT16 a)
{
	if (a < 0x400) {
		s.cyc += 1;
		return iram[a];
	}
	s.cyc += 4;
	return ROM(a);
}

static inline UINT16 fetch()
{
	return ssp_read(s.pc++);
}

static inline UINT32 update_p()
{
	s.p = (UINT32)((INT32)(INT16)s.x * (INT32)(INT16)s.y) << ((s.st & ST_MACS) ? 0 : 1);
	return s.p;
}

static inline void set_zn(UINT32 v)
{
	s.st &= ~(ST_Z | ST_N);
	if (v == 0) s.st |= ST_Z;
	if (v & 0x80000000) s.st |= ST_N;
}

static UINT16 pr1_read(INT32 r, INT32 bank, INT32 mode)
{
	UINT16 *p = &s.ram[bank << 8];
	UINT8 *i = &s.r[bank << 2];

	if (r == 3) return p[mode];
	if (mode == 0) return p[i[r]];
	if (mode == 1) return p[i[r]++];

	UINT16 v = p[i[r]];
	INT32 rpl = ST_RPL;
	INT32 mask = (1 << rpl) - 1;

	if (rpl == 0) {
		if (mode == 2) i[r]--; else i[r]++;
	} else {
		INT32 n = (mode == 2) ? (i[r] - 1) : (i[r] + 1);
		i[r] = (i[r] & ~mask) | (n & mask);
	}

	return v;
}

static inline UINT16 pr1_read_op(UINT16 op)
{
	return pr1_read(op & 3, (op >> 8) & 1, (op >> 2) & 3);
}

static void pr1_write(UINT16 op, UINT16 v)
{
	INT32 r = op & 3;
	INT32 mode = (op >> 2) & 3;
	UINT16 *p = &s.ram[(op & 0x100)];
	UINT8 *i = &s.r[(op >> 6) & 4];

	if (r == 3) { p[mode] = v; return; }

	switch (mode) {
		case 0: p[i[r]] = v; break;
		case 1: p[i[r]++] = v; break;
		case 2: p[i[r]--] = v; break;
		case 3: p[i[r]++] = v; break;
	}
}

static UINT16 pr2_read(UINT16 op)
{
	INT32 r = op & 3;
	INT32 mode = (op >> 2) & 3;
	UINT16 *p = &s.ram[(op & 0x100)];
	UINT8 *i = &s.r[(op >> 6) & 4];

	if (r == 3) return ssp_read(p[mode]++);
	if (mode == 0) return ssp_read(p[i[r]]++);
	return 0;
}

static UINT16 gr_read(INT32 r)
{
	switch (r) {
		case 0: return 0xffff;
		case 1: return s.x;
		case 2: return s.y;
		case 3: return AH;
		case 4: return s.st;
		case 5:
			if ((INT16)--s.stack < 0) s.stack = 5;
			return s.frame[s.stack];
		case 6: return s.pc;
		case 7: return update_p() >> 16;
		case 15: ext_read(7); return s.a & 0xffff;
	}

	return ext_read(r & 7);
}

static void gr_write(INT32 r, UINT16 v)
{
	switch (r) {
		case 0: return;
		case 1: s.x = v; return;
		case 2: s.y = v; return;
		case 3: s.a = (s.a & 0xffff) | (v << 16); return;
		case 4: s.st = v; return;
		case 5:
			if (s.stack >= 6) s.stack = 0;
			s.frame[s.stack++] = v;
			return;
		case 6: s.pc = v; return;
		case 7: return;
		case 15: ext_write(7, v); s.a = (s.a & 0xffff0000) | v; return;
	}

	ext_write(r & 7, v);
}

static INT32 test(UINT16 op)
{
	INT32 flag = (op >> 8) & 1;

	switch ((op >> 4) & 7) {
		case 0: return flag == 0;
		case 1: return 0;
		case 2: return ((s.st & ST_USR0) ? 1 : 0) == flag;
		case 3: return ((s.st & ST_USR1) ? 1 : 0) == flag;
		case 4: return ((s.st & ST_C) ? 1 : 0) == flag;
		case 5: return ((s.st & ST_Z) ? 1 : 0) == flag;
		case 6: return ((s.st & ST_V) ? 1 : 0) == flag;
		case 7: return ((s.st & ST_N) ? 1 : 0) == flag;
	}

	return 0;
}

static void alu_add(UINT32 src)
{
	UINT32 res = s.a + src;
	UINT32 carries = s.a ^ src ^ res;
	UINT32 ovf = (s.a ^ res) & (src ^ res);
	s.st &= ~(ST_C | ST_V);
	if ((carries ^ ovf) >> 31) s.st |= ST_C;
	if (ovf >> 31) s.st |= ST_V;
	s.a = ((~s.st & ST_OP) || (~s.st & ST_V)) ? res : 0x7fffffff;
	set_zn(s.a);
}

static void alu_sub(UINT32 src)
{
	UINT32 res = s.a - src;
	UINT32 carries = s.a ^ src ^ res;
	UINT32 ovf = (s.a ^ res) & (src ^ s.a);
	s.st &= ~(ST_C | ST_V);
	if ((carries ^ ovf) >> 31) s.st |= ST_C;
	if (ovf >> 31) s.st |= ST_V;
	s.a = ((~s.st & ST_OP) || (~s.st & ST_V)) ? res : 0x80000000;
	set_zn(s.a);
}

static void alu_cmp(UINT32 src)
{
	UINT32 res = s.a - src;
	UINT32 carries = s.a ^ src ^ res;
	UINT32 ovf = (s.a ^ res) & (src ^ s.a);
	s.st &= ~(ST_C | ST_V);
	if ((carries ^ ovf) >> 31) s.st |= ST_C;
	if (ovf >> 31) s.st |= ST_V;
	set_zn(res);
}

static void alu(INT32 type, UINT32 v)
{
	switch (type) {
		case 1: alu_sub(v); break;
		case 3: alu_cmp(v); break;
		case 4: alu_add(v); break;
		case 5: s.a &= v; set_zn(s.a); break;
		case 6: s.a |= v; set_zn(s.a); break;
		case 7: s.a ^= v; set_zn(s.a); break;
	}
}

static inline void mul_load(UINT16 op)
{
	s.x = pr1_read(op & 3, 0, (op >> 2) & 3);
	s.y = pr1_read((op >> 4) & 3, 1, (op >> 6) & 3);
}

static void ssp_instruction()
{
	UINT16 op = fetch();

	switch (op >> 9) {
		case 0x00: { // ld d,s
			if (op == 0) break;
			INT32 sr = op & 0xf, dr = (op >> 4) & 0xf;
			if (sr == 7 && dr == 3) {
				s.a = update_p();
			} else {
				gr_write(dr, gr_read(sr));
			}
			break;
		}

		case 0x01: gr_write((op >> 4) & 0xf, pr1_read_op(op)); break;	// ld d,(ri)
		case 0x02: pr1_write(op, gr_read((op >> 4) & 0xf)); break;		// ld (ri),s
		case 0x03: s.a = (s.a & 0xffff) | (s.ram[op & 0x1ff] << 16); break; // ld a,addr
		case 0x04: gr_write((op >> 4) & 0xf, fetch()); break;			// ldi d,imm
		case 0x05: gr_write((op >> 4) & 0xf, pr2_read(op)); break;		// ld d,((ri))
		case 0x06: pr1_write(op, fetch()); break;						// ldi (ri),imm
		case 0x07: s.ram[op & 0x1ff] = AH; break;						// ld addr,a
		case 0x09: gr_write((op >> 4) & 0xf, s.r[(op & 3) | ((op >> 6) & 4)]); break; // ld d,ri
		case 0x0a: s.r[(op & 3) | ((op >> 6) & 4)] = gr_read((op >> 4) & 0xf); break; // ld ri,s
		case 0x0c: case 0x0d: case 0x0e: case 0x0f: s.r[(op >> 8) & 7] = op & 0xff; break; // ldi ri,simm

		case 0x24: { // call cond,addr
			UINT16 pc = fetch();
			if (!test(op)) break;
			if (s.stack >= 6) s.stack = 0;
			s.frame[s.stack++] = s.pc;
			s.pc = pc;
			break;
		}

		case 0x25: gr_write((op >> 4) & 0xf, ssp_read(AH)); break;		// ld d,(a)

		case 0x26: { // bra cond,addr
			UINT16 pc = fetch();
			if (test(op)) s.pc = pc;
			break;
		}

		case 0x48: // mod cond,op
			if (!test(op)) break;
			switch (op & 7) {
				case 2: s.a = (INT32)s.a >> 1; set_zn(s.a); break;
				case 3: s.a <<= 1; set_zn(s.a); break;
				case 6: s.a = -(INT32)s.a; set_zn(s.a); break;
				case 7: if ((INT32)s.a < 0) s.a = -(INT32)s.a; set_zn(s.a); break;
			}
			break;

		case 0x1b: s.a -= update_p(); set_zn(s.a); mul_load(op); break; // mpys
		case 0x4b: s.a += update_p(); set_zn(s.a); mul_load(op); break; // mpya
		case 0x5b: // mld
			s.a = 0;
			s.st &= ~(ST_C | ST_V | ST_N);
			s.st |= ST_Z;
			mul_load(op);
			break;

		// alu a,s
		case 0x10: case 0x30: case 0x40: case 0x50: case 0x60: case 0x70: {
			INT32 sr = op & 0xf;
			UINT32 v = (sr == 3) ? s.a : (sr == 7) ? update_p() : (UINT32)gr_read(sr) << 16;
			alu(op >> 13, v);
			break;
		}

		// alu a,(ri)
		case 0x11: case 0x31: case 0x41: case 0x51: case 0x61: case 0x71:
			alu(op >> 13, (UINT32)pr1_read_op(op) << 16);
			break;

		// alu a,addr
		case 0x13: case 0x33: case 0x43: case 0x53: case 0x63: case 0x73:
			alu(op >> 13, (UINT32)s.ram[op & 0x1ff] << 16);
			break;

		// alu a,imm
		case 0x14: case 0x34: case 0x44: case 0x54: case 0x64: case 0x74:
			alu(op >> 13, (UINT32)fetch() << 16);
			break;

		// alu a,((ri))
		case 0x15: case 0x35: case 0x45: case 0x55: case 0x65: case 0x75:
			alu(op >> 13, (UINT32)pr2_read(op) << 16);
			break;

		// alu a,ri
		case 0x19: case 0x39: case 0x49: case 0x59: case 0x69: case 0x79:
			alu(op >> 13, (UINT32)s.r[(op & 3) | ((op >> 6) & 4)] << 16);
			break;

		// alu simm
		case 0x1c: case 0x3c: case 0x4c: case 0x5c: case 0x6c: case 0x7c:
			alu(op >> 13, (UINT32)(op & 0xff) << 16);
			break;
	}

	update_p();
}

// -----------------------------------------------------------------------------
// 68k side

static void svp_sync()
{
	if (SVPMainCycles) SVPRun(SVPMainCycles());
}

// 0x390000-0x3affff: dram views used to transfer cells (tile order)
static UINT16 cell_read(UINT32 address)
{
	if (address >= 0x390000 && address <= 0x39ffff) {
		address = (address & 0xe002) | ((address & 0x7c) << 6) | ((address & 0x1f80) >> 5);
		return dram_r(address >> 1);
	}
	if (address >= 0x3a0000 && address <= 0x3affff) {
		address = (address & 0xf002) | ((address & 0x3c) << 6) | ((address & 0x0fc0) >> 4);
		return dram_r(address >> 1);
	}
	return 0xffff;
}

static UINT16 __fastcall svp_cell_read_word(UINT32 address)
{
	return cell_read(address & 0xfffffe);
}

static UINT8 __fastcall svp_cell_read_byte(UINT32 address)
{
	UINT16 d = cell_read(address & 0xfffffe);
	return (address & 1) ? (d & 0xff) : (d >> 8);
}

static void __fastcall svp_cell_write_word(UINT32, UINT16) { }
static void __fastcall svp_cell_write_byte(UINT32, UINT8) { }

static UINT16 io_read(UINT32 address)
{
	svp_sync();

	switch (address & 0x3fe) {
		case 0x00:
		case 0x02:
			return s.xst;

		case 0x04: {
			UINT16 d = s.pm0;
			s.pm0 &= ~1;
			return d;
		}
	}

	return 0;
}

static void io_write(UINT32 address, UINT16 data)
{
	svp_sync();

	switch (address & 0x3fe) {
		case 0x00:
		case 0x02:
			s.xst = data;
			s.pm0 |= 2;
			break;
	}
}

static UINT16 __fastcall svp_io_read_word(UINT32 address)
{
	return io_read(address);
}

static UINT8 __fastcall svp_io_read_byte(UINT32 address)
{
	UINT16 d = io_read(address);
	return (address & 1) ? (d & 0xff) : (d >> 8);
}

static void __fastcall svp_io_write_word(UINT32 address, UINT16 data)
{
	io_write(address, data);
}

static void __fastcall svp_io_write_byte(UINT32 address, UINT8 data)
{
	io_write(address, data | (data << 8));
}

INT32 SVPDmaSource(UINT32 source)
{
	if (source >= 0x300000 && source < 0x400000) source -= 2;
	return source;
}

// -----------------------------------------------------------------------------

INT32 SVPInit(UINT8 *romdata, INT32 rom_size)
{
	SVPMode = 1;
	osc = OSC_NTSC;

	rom = (UINT16*)romdata;
	rom_mask = 1;
	while (rom_mask < (UINT32)rom_size / 2) rom_mask <<= 1;
	rom_mask--;

	mem = (UINT8*)BurnMalloc((0x400 + 0x10000) * sizeof(UINT16));
	if (mem == NULL) return 1;
	iram = (UINT16*)mem;
	dram = iram + 0x400;

	for (INT32 i = 0; i < 4; i++) {
		SekMapMemory((UINT8*)dram, 0x300000 + i * 0x20000, 0x31ffff + i * 0x20000, MAP_RAM);
	}

	SekMapHandler(7, 0x380000, 0x3fffff, MAP_READ | MAP_WRITE);
	SekSetReadByteHandler(7, svp_cell_read_byte);
	SekSetReadWordHandler(7, svp_cell_read_word);
	SekSetWriteByteHandler(7, svp_cell_write_byte);
	SekSetWriteWordHandler(7, svp_cell_write_word);

	SekMapHandler(8, 0xa15000, 0xa153ff, MAP_READ | MAP_WRITE);
	SekSetReadByteHandler(8, svp_io_read_byte);
	SekSetReadWordHandler(8, svp_io_read_word);
	SekSetWriteByteHandler(8, svp_io_write_byte);
	SekSetWriteWordHandler(8, svp_io_write_word);

	return 0;
}

void SVPExit()
{
	BurnFree(mem);
	iram = dram = NULL;
	rom = NULL;
	SVPMainCycles = NULL;
	SVPMode = 0;
}

void SVPSetPAL(INT32 pal)
{
	osc = (pal) ? OSC_PAL : OSC_NTSC;
}

void SVPReset()
{
	memset(&s, 0, sizeof(s));
	memset(mem, 0, (0x400 + 0x10000) * sizeof(UINT16));

	// internal rom reset handler: checks the "SV" header tag, sets PM0 and jumps to the entry point,
	// header words are read through PM4 (rom mode, autoincrement)
	UINT32 hdr = 0x1c8 / 2;
	if (ROM(hdr) != 0x5356 || (ROM(hdr + 1) & 0x3fe) != 0) {
		s.halted = 1;
		return;
	}
	s.pm0 = (ROM(hdr + 1) & 0x1c00) | 0xe000;
	s.pc = ROM(hdr + 3);
	s.pmac[0][4] = 0x08000000 | (hdr + 4);
	s.pmc = s.pmac[0][4];
	s.r[6] = 0xfc;
}

void SVPRun(INT32 main_cycles)
{
	INT32 target = (INT32)((INT64)main_cycles * SVP_CLOCK * 7 / osc);

	if (s.halted) {
		if (s.cyc < target) s.cyc = target;
		return;
	}

	while (s.cyc < target) {
		ssp_instruction();
	}
}

void SVPEndFrame(INT32 main_cycles)
{
	SVPRun(main_cycles);

	s.cyc -= (INT32)((INT64)main_cycles * SVP_CLOCK * 7 / osc);
}

INT32 SVPScan(INT32 nAction, INT32 *)
{
	if (nAction & ACB_VOLATILE) {
		struct BurnArea ba;

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = mem;
		ba.nLen	  = (0x400 + 0x10000) * sizeof(UINT16);
		ba.szName = "SVP RAM";
		BurnAcb(&ba);

		SCAN_VAR(s);
	}

	return 0;
}
