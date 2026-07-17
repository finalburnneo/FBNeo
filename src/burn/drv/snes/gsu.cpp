// =============================================================================
//  FBNeo SNES  -  SuperFX (GSU) coprocessor core
// =============================================================================
//  Emulation logic derived (clean-room, open-source) from ares:
//        ares/component/processor/gsu/{gsu.cpp,registers.hpp,instruction.cpp,
//                                      instructions.cpp,serialization.cpp}
//        ares/sfc/coprocessor/superfx/{superfx.cpp,bus.cpp,core.cpp,memory.cpp,
//                                      io.cpp,timing.cpp,serialization.cpp}
//  ares is (c) its authors under the ISC licence (see license.txt).
//
//  This file is a deliberate structural re-expression, NOT a transliteration
//  (see the porting notes in gsu.h):
//    * ares' GSU (Processor) + SuperFX (Thread) C++ classes, virtual dispatch,
//      nall BitField/Register operator overloads and libco scheduler are all
//      removed.  The whole chip is one flat C state struct, SnesGsuState, with
//      plain file-scope functions - no members, templates or namespaces.
//    * ares' n8/n16/u32 give way to FBNeo's UINT8/UINT16/UINT32/INT-typedefs.
//    * ares' opcode dispatch (a preprocessor-expanded switch over member
//      functions) is rebuilt as a switch over static functions taking an
//      explicit register-index argument (mercury-style op4/op6/... groups),
//      preserving ares' exact ALT-mode decoding.
//    * ares' cooperative Thread::step / synchronize(cpu) becomes a cycle-budget
//      catch-up loop (snes_gsu_run) driven by cart_run() under Cart::heavySync,
//      exactly as sa1.cpp does.
//    * ares' bus/serializer/cpu.irq bind to FBNeo's linear ROM/RAM buffers,
//      StateHandler and cpu_setIrq().
//
//  FBNeo mount / bridge glue (c) 2026 (see license.txt).
// =============================================================================

#include "snes.h"
#include "cpu.h"
#include "cart.h"
#include "gsu.h"

// -----------------------------------------------------------------------------
//  Flag / bit helpers.  ares stored the SFR (status flag register) as a packed
//  BitField<16,..> word; here each flag is a plain field of SnesGsuState and the
//  packed 16-bit view is (de)composed on the S-CPU-visible MMIO boundary only.
// -----------------------------------------------------------------------------

// SFR bit positions (ares registers.hpp: SFR BitField indices)
#define SNES_GSU_SFR_Z    0x0002	// zero flag
#define SNES_GSU_SFR_CY   0x0004	// carry flag
#define SNES_GSU_SFR_S    0x0008	// sign flag
#define SNES_GSU_SFR_OV   0x0010	// overflow flag
#define SNES_GSU_SFR_G    0x0020	// go flag
#define SNES_GSU_SFR_R    0x0040	// ROM r14 read flag
#define SNES_GSU_SFR_ALT1 0x0100	// alt1 instruction mode
#define SNES_GSU_SFR_ALT2 0x0200	// alt2 instruction mode
#define SNES_GSU_SFR_IL   0x0400	// immediate lower 8-bit flag
#define SNES_GSU_SFR_IH   0x0800	// immediate upper 8-bit flag
#define SNES_GSU_SFR_B    0x1000	// with flag
#define SNES_GSU_SFR_IRQ  0x8000	// interrupt flag

// -----------------------------------------------------------------------------
//  GSU state.  This single struct replaces ares' Registers, Cache, PixelCache,
//  SuperFX chip state and the FBNeo mount bookkeeping.  ares kept many of these
//  as C++ locals or sub-objects; per the porting brief they are hoisted into one
//  global state structure.
// -----------------------------------------------------------------------------

typedef struct SnesGsuPixelCache {
	UINT16 offset;
	UINT8  bitpend;
	UINT8  data[8];
} SnesGsuPixelCache;

typedef struct SnesGsuState {
	// --- general purpose registers (ares: Register r[16]) ---
	UINT16 r[16];			// r0..r15
	UINT8  r_modified[16];	// ares Register::modified (only r14/r15 consulted)

	// --- status flag register, expanded (ares: SFR) ---
	UINT8  sfr_z, sfr_cy, sfr_s, sfr_ov;
	UINT8  sfr_g, sfr_r;
	UINT8  sfr_alt1, sfr_alt2;
	UINT8  sfr_il, sfr_ih;
	UINT8  sfr_b;
	UINT8  sfr_irq;

	// --- misc registers (ares: Registers struct fields) ---
	UINT8  pipeline;		// prefetched opcode
	UINT16 ramaddr;			// last RAM access address (store/load bookkeeping)

	UINT8  pbr;				// program bank register
	UINT8  rombr;			// game pack ROM bank register
	UINT8  rambr;			// game pack RAM bank register (1 bit)
	UINT16 cbr;				// cache base register
	UINT8  scbr;			// screen base register

	// screen mode register (ares: SCMR decomposed)
	UINT32 scmr_ht;
	UINT8  scmr_ron;
	UINT8  scmr_ran;
	UINT32 scmr_md;

	UINT8  colr;			// color register

	// plot option register (ares: POR decomposed)
	UINT8  por_obj;
	UINT8  por_freezehigh;
	UINT8  por_highnibble;
	UINT8  por_dither;
	UINT8  por_transparent;

	UINT8  bramr;			// back-up RAM enable (1 bit)
	UINT8  vcr;				// version code register

	// config register (ares: CFGR decomposed)
	UINT8  cfgr_irq;
	UINT8  cfgr_ms0;

	UINT8  clsr;			// clock select register (1 bit)

	// ROM/RAM access buffers (ares: romcl/romdr, ramcl/ramar/ramdr)
	UINT32 romcl;			// clock ticks until romdr valid
	UINT8  romdr;			// ROM buffer data register
	UINT32 ramcl;			// clock ticks until ramdr valid
	UINT16 ramar;			// RAM buffer address register
	UINT8  ramdr;			// RAM buffer data register

	UINT32 sreg;			// source register index (from)
	UINT32 dreg;			// destination register index (to)

	// --- opcode cache (ares: Cache) ---
	UINT8  cache_buffer[512];
	UINT8  cache_valid[32];

	// --- pixel plot caches (ares: PixelCache pixelcache[2]) ---
	SnesGsuPixelCache pixelcache[2];

	// --- FBNeo mount / run-loop bookkeeping ---
	UINT64 clock;			// GSU master-clock accumulator (snes->cycles domain)
	UINT64 sync_to;			// current run() budget target
	UINT8  r15_modified;	// ares Register r[15].modified, kept explicit

	UINT8  cpu_irq_line;	// GSU-driven S-CPU IRQ line (raw)
	UINT8  in_irq;			// did WE assert cpu_setIrq(true)? (latched)

	// --- oscillator / clock ratio sync ---
	UINT64 gsu_clock_fp;	// 32.32 fixed-point GSU clock (numerator units)
	UINT64 gsu_clock_base;	// snes->cycles at last sync (for delta calc)
	UINT32 gsu_freq;		// GSU oscillator in Hz (21.44 MHz, or cpu freq)
	UINT32 cpu_freq;		// S-CPU clock in Hz (NTSC 21.477 / PAL 21.281)
} SnesGsuState;

static SnesGsuState gsu;

// -----------------------------------------------------------------------------
//  Mount context (ares kept ROM/RAM as MappedRAM members of SuperFX).
// -----------------------------------------------------------------------------

static Snes*  gsu_snes;

static UINT8* gsu_rom;
static UINT32 gsu_rom_size;
static UINT32 gsu_rom_mask;

static UINT8* gsu_ram;
static UINT32 gsu_ram_size;
static UINT32 gsu_ram_mask;

static INT32  gsu_hirom;	// 0 = GSU-1 window set, 1 = GSU-2 window set

// -----------------------------------------------------------------------------
//  Forward declarations
// -----------------------------------------------------------------------------

static UINT8 snes_gsu_bus_read(UINT32 address);
static void  snes_gsu_bus_write(UINT32 address, UINT8 data);
static UINT8 snes_gsu_op_read(UINT16 address);
static UINT8 snes_gsu_peekpipe();
static UINT8 snes_gsu_pipe();
static void  snes_gsu_cache_flush();

static void  snes_gsu_step(UINT32 clocks);
static void  snes_gsu_rombuffer_sync();
static UINT8 snes_gsu_rombuffer_read();
static void  snes_gsu_rombuffer_update();
static void  snes_gsu_rambuffer_sync();
static UINT8 snes_gsu_rambuffer_read(UINT16 address);
static void  snes_gsu_rambuffer_write(UINT16 address, UINT8 data);

static void  snes_gsu_stop();
static UINT8 snes_gsu_color(UINT8 source);
static void  snes_gsu_plot(UINT8 x, UINT8 y);
static UINT8 snes_gsu_rpix(UINT8 x, UINT8 y);
static void  snes_gsu_pixelcache_flush(SnesGsuPixelCache* pc);

static void  snes_gsu_execute(UINT8 opcode);
static void  snes_gsu_update_irq_forward();

// -----------------------------------------------------------------------------
//  Register-file helpers.  ares used a Register class whose assignment operators
//  set the "modified" flag and, for r14/r15, triggered ROM buffering / branch
//  detection through SuperFX::main().  Here reads are plain and writes route
//  through snes_gsu_reg_write(), which keeps the modified flags and fires the
//  r14 ROM-buffer side effect exactly as ares' SuperFX::main() did.
// -----------------------------------------------------------------------------

static inline void snes_gsu_reg_write(UINT32 n, UINT16 value)
{
	gsu.r[n] = value;
	gsu.r_modified[n] = 1;
	if (n == 15) gsu.r15_modified = 1;
}

// convenience: source / destination register accessors (ares: regs.sr()/dr())
#define SNES_GSU_SR() (gsu.r[gsu.sreg])
#define SNES_GSU_DR_WRITE(v) snes_gsu_reg_write(gsu.dreg, (v))

// ares regs.reset(): clear WITH/ALT flags and reset src/dst selection
static inline void snes_gsu_regs_reset()
{
	gsu.sfr_b    = 0;
	gsu.sfr_alt1 = 0;
	gsu.sfr_alt2 = 0;
	gsu.sreg     = 0;
	gsu.dreg     = 0;
}

// =============================================================================
//  GSU-internal bus  (ares sfc/coprocessor/superfx/memory.cpp + bus.cpp)
//  The GSU's own view of ROM/RAM: linear, masked.  ares spun on RON/RAN with
//  synchronize(cpu); here (as in sa1.cpp) the buses are granted while g=1, which
//  is always true for real GSU games, so the bus is read directly.
// =============================================================================

static UINT8 snes_gsu_bus_read(UINT32 address)
{
	if ((address & 0xc00000) == 0x000000) {		// $00-3f:0000-7fff, :8000-ffff
		return gsu_rom[(((address & 0x3f0000) >> 1) | (address & 0x7fff)) & gsu_rom_mask];
	}

	if ((address & 0xe00000) == 0x400000) {		// $40-5f:0000-ffff
		return gsu_rom[address & gsu_rom_mask];
	}

	if ((address & 0xfe0000) == 0x700000) {		// $70-71:0000-ffff
		return gsu_ram[address & gsu_ram_mask];
	}

	return 0x00;
}

static void snes_gsu_bus_write(UINT32 address, UINT8 data)
{
	if ((address & 0xfe0000) == 0x700000) {		// $70-71:0000-ffff
		gsu_ram[address & gsu_ram_mask] = data;
	}
}

// ares SuperFX::readOpcode(): fetch through the 512-byte instruction cache
static UINT8 snes_gsu_op_read(UINT16 address) {
	UINT16 offset = address - gsu.cbr;
	if (offset < 512) {
		if (gsu.cache_valid[offset >> 4] == 0) {
			UINT32 dp = offset & 0xfff0;
			UINT32 sp = (gsu.pbr << 16) + ((gsu.cbr + dp) & 0xfff0);
			for (UINT32 n = 0; n < 16; n++) {
				snes_gsu_step(gsu.clsr ? 5 : 6);
				gsu.cache_buffer[dp++] = snes_gsu_bus_read(sp++);
			}
			gsu.cache_valid[offset >> 4] = 1;
		} else {
			snes_gsu_step(gsu.clsr ? 1 : 2);
		}
		return gsu.cache_buffer[offset];
	}

	if (gsu.pbr <= 0x5f) {
		// $00-5f:0000-ffff ROM
		snes_gsu_rombuffer_sync();
		snes_gsu_step(gsu.clsr ? 5 : 6);
		return snes_gsu_bus_read((gsu.pbr << 16) | address);
	} else {
		// $60-7f:0000-ffff RAM
		snes_gsu_rambuffer_sync();
		snes_gsu_step(gsu.clsr ? 5 : 6);
		return snes_gsu_bus_read((gsu.pbr << 16) | address);
	}
}

// ares SuperFX::peekpipe(): read pipeline without advancing r15 fetch position
static UINT8 snes_gsu_peekpipe()
{
	UINT8 result = gsu.pipeline;
	gsu.pipeline       = snes_gsu_op_read(gsu.r[15]);
	gsu.r_modified[15] = 0;
	gsu.r15_modified   = 0;
	return result;
}

// ares SuperFX::pipe(): read pipeline and prefetch next (++r15)
static UINT8 snes_gsu_pipe()
{
	UINT8 result = gsu.pipeline;
	gsu.r[15]++;                              // ares: ++regs.r[15] (bare increment)
	gsu.pipeline = snes_gsu_op_read(gsu.r[15]);
	gsu.r_modified[15] = 0;
	gsu.r15_modified   = 0;
	return result;
}

static void snes_gsu_cache_flush()
{
	for (UINT32 n = 0; n < 32; n++) gsu.cache_valid[n] = 0;
}

static UINT8 snes_gsu_cache_mmio_read(UINT16 address)
{
	address = (address + gsu.cbr) & 511;
	return gsu.cache_buffer[address];
}

static void snes_gsu_cache_mmio_write(UINT16 address, UINT8 data)
{
	address = (address + gsu.cbr) & 511;
	gsu.cache_buffer[address] = data;
	if ((address & 15) == 15) gsu.cache_valid[address >> 4] = 1;
}

// =============================================================================
//  Timing  (ares sfc/coprocessor/superfx/timing.cpp)
//  ares step(): resolve ROM/RAM buffer countdowns, advance the Thread clock and
//  synchronize(cpu).  Here the Thread advance becomes accumulation into
//  gsu.clock; synchronisation is implicit (run() yields when the budget spent).
// =============================================================================

static void snes_gsu_step(UINT32 clocks)
{
	if (gsu.romcl) {
		gsu.romcl -= (clocks < gsu.romcl) ? clocks : gsu.romcl;
		if (gsu.romcl == 0) {
			gsu.sfr_r = 0;
			gsu.romdr = snes_gsu_bus_read((gsu.rombr << 16) + gsu.r[14]);
		}
	}

	if (gsu.ramcl) {
		gsu.ramcl -= (clocks < gsu.ramcl) ? clocks : gsu.ramcl;
		if (gsu.ramcl == 0) {
			snes_gsu_bus_write(0x700000 + (gsu.rambr << 16) + gsu.ramar, gsu.ramdr);
		}
	}

	gsu.clock += clocks;
}

static void snes_gsu_rombuffer_sync()
{
	if (gsu.romcl) snes_gsu_step(gsu.romcl);
}

static UINT8 snes_gsu_rombuffer_read()
{
	snes_gsu_rombuffer_sync();
	return gsu.romdr;
}

static void snes_gsu_rombuffer_update()
{
	gsu.sfr_r = 1;
	gsu.romcl = gsu.clsr ? 5 : 6;
}

static void snes_gsu_rambuffer_sync()
{
	if (gsu.ramcl) snes_gsu_step(gsu.ramcl);
}

static UINT8 snes_gsu_rambuffer_read(UINT16 address)
{
	snes_gsu_rambuffer_sync();
	return snes_gsu_bus_read(0x700000 + (gsu.rambr << 16) + address);
}

static void snes_gsu_rambuffer_write(UINT16 address, UINT8 data)
{
	snes_gsu_rambuffer_sync();
	gsu.ramcl = gsu.clsr ? 5 : 6;
	gsu.ramar = address;
	gsu.ramdr = data;
}

// =============================================================================
//  Core: raster / plot pipeline  (ares sfc/coprocessor/superfx/core.cpp)
// =============================================================================

static void snes_gsu_stop()
{
	// ares: cpu.irq(1)  -> raise the S-CPU IRQ line via FBNeo's latched forward
	gsu.cpu_irq_line = 1;
	snes_gsu_update_irq_forward();
}

static UINT8 snes_gsu_color(UINT8 source)
{
	if (gsu.por_highnibble) return (gsu.colr & 0xf0) | (source >> 4);
	if (gsu.por_freezehigh) return (gsu.colr & 0xf0) | (source & 0x0f);
	return source;
}

static void snes_gsu_plot(UINT8 x, UINT8 y)
{
	if (!gsu.por_transparent) {
		if (gsu.scmr_md == 3) {
			if (gsu.por_freezehigh) {
				if ((gsu.colr & 0x0f) == 0) return;
			} else {
				if (gsu.colr == 0)          return;
			}
		} else {
			if ((gsu.colr & 0x0f) == 0)     return;
		}
	}

	UINT8 color = gsu.colr;
	if (gsu.por_dither && gsu.scmr_md != 3) {
		if ((x ^ y) & 1) color >>= 4;
		color &= 0x0f;
	}

	UINT16 offset = (y << 5) + (x >> 3);
	if (offset != gsu.pixelcache[0].offset) {
		snes_gsu_pixelcache_flush(&gsu.pixelcache[1]);
		gsu.pixelcache[1] = gsu.pixelcache[0];
		gsu.pixelcache[0].bitpend = 0x00;
		gsu.pixelcache[0].offset  = offset;
	}

	x = (x & 7) ^ 7;
	gsu.pixelcache[0].data[x] = color;
	gsu.pixelcache[0].bitpend |= 1 << x;
	if (gsu.pixelcache[0].bitpend == 0xff) {
		snes_gsu_pixelcache_flush(&gsu.pixelcache[1]);
		gsu.pixelcache[1] = gsu.pixelcache[0];
		gsu.pixelcache[0].bitpend = 0x00;
	}
}

static UINT8 snes_gsu_rpix(UINT8 x, UINT8 y)
{
	snes_gsu_pixelcache_flush(&gsu.pixelcache[1]);
	snes_gsu_pixelcache_flush(&gsu.pixelcache[0]);

	UINT32 cn = 0;	// character number
	switch (gsu.por_obj ? 3 : gsu.scmr_ht) {
		case 0: cn = ((x & 0xf8) << 1) + ((y & 0xf8) >> 3);                                         break;
		case 1: cn = ((x & 0xf8) << 1) + ((x & 0xf8) >> 1) + ((y & 0xf8) >> 3);                     break;
		case 2: cn = ((x & 0xf8) << 1) + ((x & 0xf8) << 0) + ((y & 0xf8) >> 3);                     break;
		case 3: cn = ((y & 0x80) << 2) + ((x & 0x80) << 1) + ((y & 0x78) << 1) + ((x & 0x78) >> 3); break;
	}
	UINT32 bpp  = 2 << (gsu.scmr_md - (gsu.scmr_md >> 1));	// { 2, 4, 4, 8 }
	UINT32 addr = 0x700000 + (cn * (bpp << 3)) + (gsu.scbr << 10) + ((y & 0x07) * 2);
	UINT8  data = 0x00;
	x = (x & 7) ^ 7;

	for (UINT32 n = 0; n < bpp; n++) {
		UINT32 byte = ((n >> 1) << 4) + (n & 1);			// { 0,1,16,17,32,33,48,49 }
		snes_gsu_step(gsu.clsr ? 5 : 6);
		data |= ((snes_gsu_bus_read(addr + byte) >> x) & 1) << n;
	}

	return data;
}

static void snes_gsu_pixelcache_flush(SnesGsuPixelCache* pc)
{
	if (pc->bitpend == 0x00) return;

	UINT8 x = pc->offset << 3;
	UINT8 y = pc->offset >> 5;

	UINT32 cn = 0;  // character number
	switch (gsu.por_obj ? 3 : gsu.scmr_ht) {
		case 0: cn = ((x & 0xf8) << 1) + ((y & 0xf8) >> 3);                                         break;
		case 1: cn = ((x & 0xf8) << 1) + ((x & 0xf8) >> 1) + ((y & 0xf8) >> 3);                     break;
		case 2: cn = ((x & 0xf8) << 1) + ((x & 0xf8) << 0) + ((y & 0xf8) >> 3);                     break;
		case 3: cn = ((y & 0x80) << 2) + ((x & 0x80) << 1) + ((y & 0x78) << 1) + ((x & 0x78) >> 3); break;
	}
	UINT32 bpp  = 2 << (gsu.scmr_md - (gsu.scmr_md >> 1));	// { 2, 4, 4, 8 }
	UINT32 addr = 0x700000 + (cn * (bpp << 3)) + (gsu.scbr << 10) + ((y & 0x07) * 2);

	for (UINT32 n = 0; n < bpp; n++) {
		UINT32 byte = ((n >> 1) << 4) + (n & 1);			// { 0,1,16,17,32,33,48,49 }
		UINT8  data = 0x00;
		for (UINT32 px = 0; px < 8; px++) data |= ((pc->data[px] >> n) & 1) << px;
		if (pc->bitpend != 0xff) {
			snes_gsu_step(gsu.clsr ? 5 : 6);
			data &= pc->bitpend;
			data |= snes_gsu_bus_read(addr + byte) & ~pc->bitpend;
		}
		snes_gsu_step(gsu.clsr ? 5 : 6);
		snes_gsu_bus_write(addr + byte, data);
	}

	pc->bitpend = 0x00;
}

// =============================================================================
//  Instruction set  (ares component/processor/gsu/instructions.cpp)
//  ares member functions become file-scope statics; ares' member calls become
//  direct calls; ares' regs.sr()/dr() become the SNES_GSU_SR / DR_WRITE macros.
// =============================================================================
#include "gsu_ops.h"

// =============================================================================
//  Opcode dispatch  (ares component/processor/gsu/instruction.cpp)
//  ares expanded a set of op/op4/op6/op12/op15/op16 macros over member function
//  templates.  The same grouping is reproduced verbatim below, dispatching to
//  the static instruction functions with an explicit 4-bit register index.
// =============================================================================
#include "gsu_table.h"

// =============================================================================
//  GSU power / reset  (ares component/processor/gsu/gsu.cpp GSU::power)
// =============================================================================

static void snes_gsu_power()
{
	for (UINT32 n = 0; n < 16; n++) {
		gsu.r[n] = 0x0000;
		gsu.r_modified[n] = 0;
	}

	gsu.sfr_z    = gsu.sfr_cy = gsu.sfr_s = gsu.sfr_ov = 0;
	gsu.sfr_g    = gsu.sfr_r = 0;
	gsu.sfr_alt1 = gsu.sfr_alt2 = 0;
	gsu.sfr_il   = gsu.sfr_ih = 0;
	gsu.sfr_b    = 0;
	gsu.sfr_irq  = 0;

	gsu.pbr   = 0x00;
	gsu.rombr = 0x00;
	gsu.rambr = 0;
	gsu.cbr   = 0x0000;
	gsu.scbr  = 0x00;

	gsu.scmr_ht = 0; gsu.scmr_ron = 0; gsu.scmr_ran = 0; gsu.scmr_md = 0;	// scmr = 0x00

	gsu.colr = 0x00;

	gsu.por_obj    = gsu.por_freezehigh = gsu.por_highnibble = 0;
	gsu.por_dither = gsu.por_transparent = 0;								// por = 0x00

	gsu.bramr = 0;
	gsu.vcr   = 0x04;

	gsu.cfgr_irq = 0; gsu.cfgr_ms0 = 0;										// cfgr = 0x00

	gsu.clsr     = 0;
	gsu.pipeline = 0x01;													// nop
	gsu.ramaddr  = 0x0000;

	snes_gsu_regs_reset();
}

// =============================================================================
//  MMIO: GSU control registers as seen by the S-CPU  ($3000-$34ff)
//  (ares sfc/coprocessor/superfx/io.cpp readIO/writeIO)
// =============================================================================

static UINT16 snes_gsu_sfr_pack() {
	UINT16 v = 0;
	if (gsu.sfr_z)    v |= SNES_GSU_SFR_Z;
	if (gsu.sfr_cy)   v |= SNES_GSU_SFR_CY;
	if (gsu.sfr_s)    v |= SNES_GSU_SFR_S;
	if (gsu.sfr_ov)   v |= SNES_GSU_SFR_OV;
	if (gsu.sfr_g)    v |= SNES_GSU_SFR_G;
	if (gsu.sfr_r)    v |= SNES_GSU_SFR_R;
	if (gsu.sfr_alt1) v |= SNES_GSU_SFR_ALT1;
	if (gsu.sfr_alt2) v |= SNES_GSU_SFR_ALT2;
	if (gsu.sfr_il)   v |= SNES_GSU_SFR_IL;
	if (gsu.sfr_ih)   v |= SNES_GSU_SFR_IH;
	if (gsu.sfr_b)    v |= SNES_GSU_SFR_B;
	if (gsu.sfr_irq)  v |= SNES_GSU_SFR_IRQ;
	return v & 0x9f7e;	// ares SFR::operator u32: mask $9f7e
}

static UINT8 snes_gsu_mmio_read(UINT32 address)
{
	address = 0x3000 | (address & 0x03ff);

	if (address >= 0x3100 && address <= 0x32ff) {
		return snes_gsu_cache_mmio_read(address - 0x3100);
	}

	if (address >= 0x3000 && address <= 0x301f) {
		return gsu.r[(address >> 1) & 15] >> ((address & 1) << 3);
	}

	switch (address) {
		case 0x3030:
			return snes_gsu_sfr_pack() >> 0;

		case 0x3031: {
			UINT8 r = snes_gsu_sfr_pack() >> 8;		// irq is bit15 -> high-byte bit7
			gsu.sfr_irq      = 0;
			gsu.cpu_irq_line = 0;					// ares: cpu.irq(0)
			snes_gsu_update_irq_forward();
			return r;
		}

		case 0x3034: return gsu.pbr;
		case 0x3036: return gsu.rombr;
		case 0x303b: return gsu.vcr;
		case 0x303c: return gsu.rambr;
		case 0x303e: return gsu.cbr >> 0;
		case 0x303f: return gsu.cbr >> 8;
	}

	return 0x00;
}

static void snes_gsu_mmio_write(UINT32 address, UINT8 data)
{
	address = 0x3000 | (address & 0x03ff);

	if (address >= 0x3100 && address <= 0x32ff) {
		snes_gsu_cache_mmio_write(address - 0x3100, data);
		return;
	}

	if (address >= 0x3000 && address <= 0x301f) {
		UINT32 n = (address >> 1) & 15;
		// ares writeIO: regs.r[n] = ... routes through Register::operator=, which
		// sets the modified flag (consulted by the GSU main loop for r14/r15).
		if ((address & 1) == 0) {
			snes_gsu_reg_write(n, (gsu.r[n] & 0xff00) | data);
		} else {
			snes_gsu_reg_write(n, (data << 8) | (gsu.r[n] & 0x00ff));
		}
		if (n == 14) snes_gsu_rombuffer_update();	// ares io.cpp: if(n==14) updateROMBuffer()

		if (address == 0x301f) gsu.sfr_g = 1;
		return;
	}

	switch (address) {
		case 0x3030: {
			UINT8 g = gsu.sfr_g;
			// low byte of SFR (ares: regs.sfr = (sfr & 0xff00) | data)
			gsu.sfr_z  = (data & SNES_GSU_SFR_Z)  != 0;
			gsu.sfr_cy = (data & SNES_GSU_SFR_CY) != 0;
			gsu.sfr_s  = (data & SNES_GSU_SFR_S)  != 0;
			gsu.sfr_ov = (data & SNES_GSU_SFR_OV) != 0;
			gsu.sfr_g  = (data & SNES_GSU_SFR_G)  != 0;
			gsu.sfr_r  = (data & SNES_GSU_SFR_R)  != 0;
			if (g == 1 && gsu.sfr_g == 0) {
				gsu.cbr = 0x0000;
				snes_gsu_cache_flush();
			}
		} break;

		case 0x3031:
			// high byte of SFR
			gsu.sfr_alt1 = (data & (SNES_GSU_SFR_ALT1 >> 8)) != 0;
			gsu.sfr_alt2 = (data & (SNES_GSU_SFR_ALT2 >> 8)) != 0;
			gsu.sfr_il   = (data & (SNES_GSU_SFR_IL >> 8))   != 0;
			gsu.sfr_ih   = (data & (SNES_GSU_SFR_IH >> 8))   != 0;
			gsu.sfr_b    = (data & (SNES_GSU_SFR_B >> 8))    != 0;
			gsu.sfr_irq  = (data & (SNES_GSU_SFR_IRQ >> 8))  != 0;
			break;

		case 0x3033:
			gsu.bramr = data & 0x01;
			break;

		case 0x3034:
			gsu.pbr = data & 0x7f;
			snes_gsu_cache_flush();
			break;

		case 0x3037:
			gsu.cfgr_irq = (data & 0x80) != 0;
			gsu.cfgr_ms0 = (data & 0x20) != 0;
			break;

		case 0x3038:
			gsu.scbr = data;
			break;

		case 0x3039:
			gsu.clsr = data & 0x01;
			break;

		case 0x303a:
			// scmr (ares SCMR::operator=)
			gsu.scmr_ht  = (UINT32)((data & 0x20) != 0) << 1;
			gsu.scmr_ht |= (UINT32)((data & 0x04) != 0) << 0;
			gsu.scmr_ron = (data & 0x10) != 0;
			gsu.scmr_ran = (data & 0x08) != 0;
			gsu.scmr_md  = data & 0x03;
			break;
	}
}

// =============================================================================
//  GSU -> S-CPU IRQ forwarding
//  ares raised cpu.irq(1) on STOP (unless CFGR.irq masks it) and cpu.irq(0) when
//  the S-CPU acknowledged $3031.  FBNeo latches this (like sa1.cpp scpu_in_irq)
//  so a genuine SNES h/v line-IRQ is never cleared out from under the CPU.
// =============================================================================

static void snes_gsu_update_irq_forward()
{
	if (gsu.cpu_irq_line) {
		gsu.in_irq = 1;
		gsu_snes->superfxIrq = true;
		cpu_setIrq(true);
	} else if (gsu.in_irq) {
		gsu.in_irq = 0;
		gsu_snes->superfxIrq = false;
		if (!gsu_snes->inIrq) cpu_setIrq(false);	// keep a pending SNES line-IRQ
	}
}

// =============================================================================
//  S-CPU -> GSU cartridge bus bridge
//  Address windows follow the ares SuperFX board definitions (SHVC-1C0N /
//  SHVC-1CA0N...).  While g=1 the GSU owns ROM (RON) / RAM (RAN) and the S-CPU
//  reads see the ares CPUROM/CPURAM "busy" values instead of live data.
//    MMIO : 00-3f,80-bf : 3000-34ff
//    ROM  : 00-3f,80-bf : 8000-ffff (32K banks)   [GSU-1: 00-1f,80-9f]
//           40-5f,c0-df : 0000-ffff               [GSU-2 extended window]
//    RAM  : GSU-1  60-7d,e0-fd : 0000-ffff
//           GSU-2  70-71,f0-f1 : 0000-ffff  + 00-3f,80-bf : 6000-7fff (8K)
// =============================================================================

static inline UINT32 snes_gsu_scpu_rom_offset(UINT8 bank, UINT16 adr)
{
	if (bank & 0x40) {
		return (((bank & 0x3f) << 16) | adr) & gsu_rom_mask;
	}
	// 00-3f / 80-bf : 8000-ffff -> 32K banks
	return (((bank & 0x7f) << 15) | (adr & 0x7fff)) & gsu_rom_mask;
}

// ares SuperFX::CPUROM::read: idle vector while GSU holds RON
static UINT8 snes_gsu_cpurom_read(UINT8 bank, UINT16 adr)
{
	if (gsu.sfr_g && gsu.scmr_ron) {
		static const UINT8 vector[16] = {
		  0x00, 0x01, 0x00, 0x01, 0x04, 0x01, 0x00, 0x01,
		  0x00, 0x01, 0x08, 0x01, 0x00, 0x01, 0x0c, 0x01,
		};
		return vector[adr & 15];
	}
	return gsu_rom[snes_gsu_scpu_rom_offset(bank, adr)];
}

// ares SuperFX::CPURAM::read: open bus while GSU holds RAN
static UINT8 snes_gsu_cpuram_read(UINT32 ram_offset)
{
	if (gsu.sfr_g && gsu.scmr_ran) return gsu_snes->openBus;
	return gsu_ram[ram_offset & gsu_ram_mask];
}

static void snes_gsu_cpuram_write(UINT32 ram_offset, UINT8 data)
{
	gsu_ram[ram_offset & gsu_ram_mask] = data;
}

UINT8 snes_gsu_cart_read(UINT32 address)
{
	UINT8  bank = (address >> 16) & 0xff;
	UINT16 adr  = address & 0xffff;

	// MMIO: $3000-34ff in banks 00-3f / 80-bf
	if (adr >= 0x3000 && adr <= 0x34ff && (bank & 0x40) == 0x00) {
		return snes_gsu_mmio_read(adr);
	}

	// Save-RAM window
	if (gsu_ram_size > 0) {
		if (gsu_hirom) {
			// GSU-2
			if ((bank & 0x40) == 0x00 && adr >= 0x6000 && adr < 0x8000) {
				return snes_gsu_cpuram_read(adr & 0x1fff);					// 00-3f,80-bf:6000-7fff 8K page
			}
			if (((bank & 0x7f) == 0x70) || ((bank & 0x7f) == 0x71)) {
				return snes_gsu_cpuram_read(((bank & 1) << 16) | adr);		// 70-71,f0-f1 64K banks
			}
		} else {
			// GSU-1
			if ((bank & 0x7f) >= 0x60 && (bank & 0x7f) <= 0x7d) {
				return snes_gsu_cpuram_read(((bank & 0x1f) << 16) | adr);	// 60-7d,e0-fd 64K banks
			}
		}
	}

	// ROM window
	if (adr >= 0x8000 || (bank & 0x40)) {
		return snes_gsu_cpurom_read(bank, adr);
	}

	return gsu_snes->openBus;
}

void snes_gsu_cart_write(UINT32 address, UINT8 data)
{
	UINT8  bank = (address >> 16) & 0xff;
	UINT16 adr = address & 0xffff;

	if ((bank & 0x40) == 0x00 && adr >= 0x3000 && adr <= 0x34ff) {
		snes_gsu_mmio_write(adr, data);
		return;
	}

	if (gsu_ram_size > 0) {
		if (gsu_hirom) {
			// GSU-2
			if ((bank & 0x40) == 0x00 && adr >= 0x6000 && adr < 0x8000) {
				snes_gsu_cpuram_write(adr & 0x1fff, data);
				return;
			}
			if (((bank & 0x7f) == 0x70) || ((bank & 0x7f) == 0x71)) {
				snes_gsu_cpuram_write(((bank & 1) << 16) | adr, data);
				return;
			}
		} else {
			// GSU-1
			if ((bank & 0x7f) >= 0x60 && (bank & 0x7f) <= 0x7d) {
				snes_gsu_cpuram_write(((bank & 0x1f) << 16) | adr, data);
				return;
			}
		}
	}
}

// =============================================================================
//  Run loop  (ares sfc/coprocessor/superfx/superfx.cpp SuperFX::main)
//  ares ran main() from a libco thread scheduled against the S-CPU.  Here it is
//  a cycle-budget catch-up: execute GSU opcodes until gsu.clock reaches
//  snes->cycles.  cart_run() calls this every 2 master cycles (Cart::heavySync),
//  giving the same near-lockstep interleave ares got from its scheduler.
// =============================================================================

// ares SuperFX::main() body for a single opcode step.
static void snes_gsu_main_step()
{
	if (gsu.sfr_g == 0) { snes_gsu_step(6); return; }

	UINT8 opcode = snes_gsu_peekpipe();
	snes_gsu_execute(opcode);

	if (gsu.r_modified[14]) {
		gsu.r_modified[14] = 0;
		snes_gsu_rombuffer_update();
	}

	if (gsu.r_modified[15]) {
		gsu.r_modified[15] = 0;
	} else {
		gsu.r[15]++;
	}
	gsu.r15_modified = 0;
}

void snes_gsu_run()
{
	UINT64 snes_now    = gsu_snes->cycles;
	UINT64 snes_delta  = (snes_now >= gsu.gsu_clock_base) ? (snes_now - gsu.gsu_clock_base) : 0;
	gsu.gsu_clock_base = snes_now;

	gsu.gsu_clock_fp += snes_delta * (UINT64)gsu.gsu_freq;
	UINT64 gsu_target = gsu.gsu_clock_fp / gsu.cpu_freq;

	gsu.sync_to = gsu_target;

	while (gsu.clock < gsu.sync_to) {
		if (gsu.sfr_g == 0) {
			UINT64 remaining = gsu.sync_to - gsu.clock;
			snes_gsu_step(remaining < 6 ? (UINT32)remaining : 6);
			continue;
		}
		snes_gsu_main_step();
	}
}

// =============================================================================
//  round rom size up to the next power of two (ares superfx.cpp romSizeRound):
//  the SuperFX voxel demo ships a non-power-of-two ROM which would corrupt the
//  ROM mask otherwise.
// =============================================================================

static UINT32 snes_gsu_rom_size_round(UINT32 size)
{
	UINT32 count = 0;
	if (size && !(size & (size - 1))) return size;
	while (size != 0) { size >>= 1; count += 1; }
	return 1 << count;
}

// =============================================================================
//  Lifecycle  (mount surface for cart.cpp - mirrors sa1 / sdd1)
// =============================================================================

void snes_gsu_init(void* mem, UINT8* rom, INT32 romSize, UINT8* ram, INT32 ramSize, INT32 hirom, UINT32 oscillator)
{
	gsu_snes     = (Snes*)mem;

	gsu_rom      = rom;
	gsu_rom_size = romSize;
	gsu_ram      = ram;
	gsu_ram_size = ramSize;
	gsu_hirom    = hirom;

	gsu_rom_mask = snes_gsu_rom_size_round(gsu_rom_size) - 1;
	gsu_ram_mask = (gsu_ram_size > 0) ? (gsu_ram_size - 1) : 0;

	// Determine S-CPU clock frequency (NTSC vs PAL)
	// NTSC: 21.477272 MHz, PAL: 21.281372 MHz
	gsu.cpu_freq = gsu_snes->palTiming ? 21281372u : 21477272u;

	// GSU oscillator: if 0, fall back to S-CPU clock (MARIO CHIP 1 style)
	gsu.gsu_freq = (oscillator != 0) ? oscillator : gsu.cpu_freq;

	// Initialize fixed-point clock tracking
	gsu.gsu_clock_fp   = 0;
	gsu.gsu_clock_base = 0;

	bprintf(0, _T("gsu (superfx): init  rom %x  ram %x  (%S)  osc %u Hz  cpu %u Hz\n"),
		gsu_rom_size, gsu_ram_size, hirom ? "GSU-2" : "GSU-1", gsu.gsu_freq, gsu.cpu_freq);
}

void snes_gsu_reset()
{
	snes_gsu_power();

	gsu.clock        = 0;
	gsu.sync_to      = 0;
	gsu.r15_modified = 0;
	gsu.cpu_irq_line = 0;
	gsu.in_irq       = 0;

	gsu.gsu_clock_fp   = 0;
	gsu.gsu_clock_base = 0;

	// ares SuperFX::power(): clear cache + pixel caches + buffers
	for (UINT32 n = 0; n < 512; n++) gsu.cache_buffer[n] = 0x00;
	for (UINT32 n = 0; n <  32; n++) gsu.cache_valid[n]  = 0;
	for (UINT32 n = 0; n <   2; n++) {
		gsu.pixelcache[n].offset  = 0xffff;		// ares: ~0
		gsu.pixelcache[n].bitpend = 0x00;
		for (UINT32 p = 0; p < 8; p++) gsu.pixelcache[n].data[p] = 0x00;
	}

	gsu.romcl = 0; gsu.romdr = 0;
	gsu.ramcl = 0; gsu.ramar = 0; gsu.ramdr = 0;

	if (gsu_snes) gsu_snes->superfxIrq = false;
}

void snes_gsu_exit()
{
	// ROM/RAM are owned by Cart; nothing GSU-private to free.
	gsu_snes = NULL;
	gsu_rom  = NULL;
	gsu_ram  = NULL;
}

// =============================================================================
//  Save state  (ares component/processor/gsu/serialization.cpp
//               + sfc/coprocessor/superfx/serialization.cpp -> StateHandler)
//  Save-RAM contents are serialized by cart_handleState (cart->ram), exactly as
//  for the other coprocessors - not duplicated here.
// =============================================================================

void snes_gsu_handleState(StateHandler* sh)
{
	// general registers + their modified flags
	for (UINT32 n = 0; n < 16; n++) sh_handleWords(sh, &gsu.r[n], NULL);
	sh_handleByteArray(sh, gsu.r_modified, 16);

	// sfr flags
	sh_handleBytes(sh,
		&gsu.sfr_z, &gsu.sfr_cy, &gsu.sfr_s, &gsu.sfr_ov, &gsu.sfr_g, &gsu.sfr_r,
		&gsu.sfr_alt1, &gsu.sfr_alt2, &gsu.sfr_il, &gsu.sfr_ih, &gsu.sfr_b, &gsu.sfr_irq, NULL);

	// scalar registers
	sh_handleBytes(sh,
		&gsu.pipeline, &gsu.pbr, &gsu.rombr, &gsu.rambr, &gsu.scbr, &gsu.colr,
		&gsu.bramr, &gsu.vcr, &gsu.clsr, &gsu.romdr, &gsu.ramdr, NULL);
	sh_handleWords(sh, &gsu.ramaddr, &gsu.cbr, &gsu.ramar, NULL);

	// scmr / por / cfgr decomposed fields
	sh_handleBytes(sh, &gsu.scmr_ron, &gsu.scmr_ran, NULL);
	sh_handleInts(sh, &gsu.scmr_ht, &gsu.scmr_md, NULL);
	sh_handleBytes(sh,
		&gsu.por_obj, &gsu.por_freezehigh, &gsu.por_highnibble, &gsu.por_dither, &gsu.por_transparent,
		&gsu.cfgr_irq, &gsu.cfgr_ms0, NULL);

	// source / destination register indices
	sh_handleInts(sh, &gsu.sreg, &gsu.dreg, NULL);

	// ROM/RAM buffer countdowns
	sh_handleInts(sh, &gsu.romcl, &gsu.ramcl, NULL);

	// instruction cache
	sh_handleByteArray(sh, gsu.cache_buffer, 512);
	sh_handleByteArray(sh, gsu.cache_valid, 32);

	// pixel caches
	for (UINT32 i = 0; i < 2; i++) {
		sh_handleWords(sh, &gsu.pixelcache[i].offset, NULL);
		sh_handleBytes(sh, &gsu.pixelcache[i].bitpend, NULL);
		sh_handleByteArray(sh, gsu.pixelcache[i].data, 8);
	}

	// mount / run-loop state
	sh_handleBytes(sh, &gsu.r15_modified, &gsu.cpu_irq_line, &gsu.in_irq, NULL);
	sh_handleLongLongs(sh, &gsu.clock, NULL);

	// oscillator / clock ratio sync state
	sh_handleLongLongs(sh, &gsu.gsu_clock_fp, &gsu.gsu_clock_base, NULL);
	sh_handleInts(sh, &gsu.gsu_freq, &gsu.cpu_freq, NULL);
}
