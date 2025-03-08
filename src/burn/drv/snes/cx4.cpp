// cx4 simulator for LakeSnes, (c) 2023 dink
// License: MIT

// Big Thanks:
//   Ikari, Nocash, Overload, Segher for all the CX4 research and documentation!
//   Rupert Carmichael for help with degree interpolation
//   Atan for pie recipe

// Notes:
//1: Since neither X2 or X3 relies on: this core doesn't emulate
//     2 of the cache peculiarities documented by ikari: Roll over to
//     second prg-cache on end of first & stop at end of second cache
//2: ~224-cycle/frame deficit
//     Where does it come from?  Possible cpu startup/stop latency?
//     Related to cache population (7f48)?  Somewhere else?

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>
#include "snes.h"
#include "cx4.h"

#include "burnint.h"

#define DEBUG_STARTSTOP 0
#define DEBUG_CACHE     0
#define DEBUG_DMA		0

// CPU condition codes
enum {
	cx4_CC_Z = 1 << 0, // Zero
	cx4_CC_C = 1 << 1, // Carry
	cx4_CC_N = 1 << 2, // Negative
	cx4_CC_V = 1 << 3, // oVerflow
	cx4_CC_I = 1 << 4  // Interrupt
};

// BUS state
enum {
	B_IDLE  = 0,
	B_READ  = 1,
	B_WRITE = 2
};

// IRQ config
enum {
	IRQ_ACKNOWLEDGE = 1 << 0
};

#define set_flg(flg, f) do { cx4.cc = (cx4.cc & ~flg) | ((f) ? flg : 0); } while (0)
#define get_flg(flg) (!!(cx4.cc & flg))

#define set_Z(f) set_flg(cx4_CC_Z, f)
#define get_Z()  get_flg(cx4_CC_Z)
#define set_C(f) set_flg(cx4_CC_C, f)
#define get_C()  get_flg(cx4_CC_C)
#define set_N(f) set_flg(cx4_CC_N, f)
#define get_N()  get_flg(cx4_CC_N)
#define set_V(f) set_flg(cx4_CC_V, f)
#define get_V()  get_flg(cx4_CC_V)
#define set_I(f) set_flg(cx4_CC_I, f)
#define get_I()  get_flg(cx4_CC_I)
#define set_NZ(f) do { set_N(f & 0x800000);	set_Z(!f); } while (0)

#define set_A(f) do { cx4.A = (f) & 0xffffff; } while (0)
#define get_A() (cx4.A << ((0x10080100 >> (8 * sub_op)) & 0xff))

#define set_byte(var, data, offset) (var = (var & (~(0xff << (((offset) & 3) * 8)))) | (data << (((offset) & 3) * 8)))
#define get_byte(var, offset) (var >> ((offset) & 3) * 8)

#define sign_extend(f, frombits) ((int32_t)((f) << (32 - (frombits))) >> (32 - (frombits)))

#define struct_sizeto(type, member) offsetof(type, member) + sizeof(((type*)0)->member)

struct Stack {
	uint32_t PC;
	uint32_t PB;
};

struct CX4 {
	uint64_t cycles;
	uint64_t cycles_start;
	uint64_t suspend_timer;
	uint32_t running;

	uint32_t prg_base_address;
	uint16_t prg_startup_bank;
	uint8_t prg_startup_pc;
	uint8_t prg_cache_page;
	uint8_t prg_cache_lock;
	uint32_t prg_cache[2];
	uint16_t prg[2][0x100];
	int32_t prg_cache_timer;

	uint8_t PC;
	uint16_t PB;
	uint16_t PB_latch;
	uint8_t cc;
	uint32_t A;
	uint32_t SP;
	Stack stack[0x08];
	uint32_t reg[0x10];
	uint8_t vectors[0x20];
	uint8_t ram[0x400 * 3];

	uint64_t multiplier;

	// bus
	uint32_t bus_address;
	uint32_t bus_mode;
	uint32_t bus_data;
	int32_t bus_timer;

	// cpu registers (bus)
	uint32_t bus_address_pointer;
	uint32_t ram_address_pointer;
	uint32_t rom_data;
	uint32_t ram_data;

	uint8_t irqcfg;
	uint8_t unkcfg;
	uint8_t waitstate;

	uint32_t dma_source;
	uint32_t dma_dest;
	uint16_t dma_length;
	int32_t dma_timer;

	// - calculated @ init -
	int32_t struct_data_length;
	double CyclesPerMaster;
	uint64_t sync_to;
	uint32_t rom[0x400];
	Snes *snes;
};

static CX4 cx4;

void cx4_init(void *mem)
{
	cx4.snes = (Snes *)mem;

	cx4.CyclesPerMaster = (double)20000000 / ((cx4.snes->palTiming) ? (1364 * 312 * 50.0) : (1364 * 262 * 60.0));

	cx4.struct_data_length = struct_sizeto(CX4, dma_timer);

	double pi = atan(1) * 4;

	for (int i = 0; i < 0x100; i++) {
		cx4.rom[0x000 + i] = (i == 0) ? 0xffffff : (0x800000 / i);
		cx4.rom[0x100 + i] = 0x100000 * sqrt(i);
	}
	for (int i = 0; i < 0x80; i++) {
		cx4.rom[0x200 + i] = 0x1000000 * sin(((i * 90.0) / 128.0) * pi / 180.0);
		cx4.rom[0x280 + i] = 0x800000 / (90.0 * pi / 180.0) * asin(i / 128.0);
		cx4.rom[0x300 + i] = 0x10000 * (tan(((i * 90.0) / 128.0) * pi / 180.0) + 0.00000001); // 0x340 needs a little push
		cx4.rom[0x380 + i] = (i == 0) ? 0xffffff : (0x1000000 * cos(((double)(i * 90.0) / 128.0) * pi / 180.0));
	}
	// test validity of generated rom
	int64_t hash = 0;
	for (int i = 0; i < 0x400; i++) {
		hash += cx4.rom[i];
	}
	if (hash != 0x169c91535ULL) {
		bprintf(PRINT_ERROR, _T("CX4 rom generation failed (bad hash, %I64x)\n"), hash);
	}
}

void cx4_reset()
{
	memset(&cx4, 0, cx4.struct_data_length);
	cx4.A = 0xffffff;
	cx4.cc = 0x00;
	cx4.running = 0;
	cx4.unkcfg = 1;
	cx4.waitstate = 0x33;
	cx4.bus_mode = B_IDLE;
}

void cx4_handleState(StateHandler* sh)
{
	sh_handleByteArray(sh, (uint8_t*)&cx4, cx4.struct_data_length);
}

#define CACHE_PAGE 0x100

static uint32_t resolve_cache_address()
{
	return cx4.prg_base_address + cx4.PB * (CACHE_PAGE << 1);
}

static int find_cache(uint32_t address)
{
	for (int i = 0; i < 2; i++) {
		if (cx4.prg_cache[i] == address) {
			return i;
		}
	}
	return -1;
}

static void populate_cache(uint32_t address)
{
	cx4.prg_cache_timer = 224; // what is the source of this?  (re: note at top of file)

	if (cx4.prg_cache[cx4.prg_cache_page] == address) return;
#if 0
	int temp = -1;
	if ((temp = find_cache(address)) != -1) {
		bprintf(0, _T("populate cache is already cached!  %x\n"), address);
		cx4.prg_cache_page = temp;
		return;
	}
#endif

#if DEBUG_CACHE
	bprintf(0, _T("caching bank  %x  @  cache pg.  %x  (prev: %x)\n"), cx4.PB, cx4.prg_cache_page, cx4.prg_cache[cx4.prg_cache_page]);
#endif

	cx4.prg_cache[cx4.prg_cache_page] = address;

	for (int i = 0; i < CACHE_PAGE; i++) {
		cx4.prg[cx4.prg_cache_page][i] = (snes_read(cx4.snes, address++) << 0) | (snes_read(cx4.snes, address++) << 8);
	}

	cx4.prg_cache_timer += ((cx4.waitstate & 0x07) * CACHE_PAGE) * 2;
#if DEBUG_CACHE
	bprintf(0, _T("cache loaded, cycles %d\n"), cx4.prg_cache_timer);
#endif
}

static void do_cache()
{
	int new_page;

#if DEBUG_CACHE
	bprintf(0, _T("cache list: %x  %x\n"), cx4.prg_cache[0], cx4.prg_cache[1]);
#endif

	// is our page cached?
	if ((new_page = find_cache(resolve_cache_address())) != -1) {
		//bprintf(0, _T("our page is already cached, yay.\n"));
		cx4.prg_cache_page = new_page;
		return;
	} else {
		// not cached, go to next slot
		cx4.prg_cache_page = (cx4.prg_cache_page + 1) & 1;
#if 0
		extern int counter;
		if (counter) {
		// Locked page issue:
		// (X2) after boss battle, on the "You got ..." screen: the blue raster box
		// flickers in for a frame or 2 before it fades in, because:
		// Cache pg 0 is locked with bank 0, pg1 is unlocked
		// pg 1 has to swap between pb 2 and 9 about 100 times per frame, and
		// this eats a lot of cycles!

		// can we use this slot?
		if (cx4.prg_cache_lock & (1 << cx4.prg_cache_page)) {
			bprintf(0, _T("-> page %x is locked (with %x) ...\n"), cx4.prg_cache_page, cx4.prg_cache[cx4.prg_cache_page]);
			cx4.prg_cache_page = (cx4.prg_cache_page + 1) & 1;
			// how about the other one?
			if (cx4.prg_cache_lock & (1 << cx4.prg_cache_page)) {
				bprintf(0, _T("CX4: we can't cache, operations terminated.\n"));
				cx4.running = 0;
				return; // not cached, can't cache. uhoh!
			}
		}
		}
#endif
	}

	populate_cache(resolve_cache_address());
}

static void cycle_advance(int32_t cyc)
{
	if (cx4.bus_timer) {
		cx4.bus_timer -= cyc;

		if (cx4.bus_timer < 1) {
			switch (cx4.bus_mode) {
				case B_READ: cx4.bus_data = snes_read(cx4.snes, cx4.bus_address); break;
				case B_WRITE: snes_write(cx4.snes, cx4.bus_address, cx4.bus_data); break;
			}
			cx4.bus_mode = B_IDLE;
			cx4.bus_timer = 0;
		}
	}

	cx4.cycles += cyc;
}

static uint16_t fetch()
{
#if 0
	// debug: bypass cache
	uint16_t opcode = 0;
	uint32_t address = (cx4.prg_base_address + (cx4.PB * (CACHE_PAGE << 1)) + (cx4.PC << 1)) & 0xffffff;
	opcode  = snes_read(cx4.snes, address++);
	opcode |= snes_read(cx4.snes, address++) << 8;
#else
	const uint16_t opcode = cx4.prg[cx4.prg_cache_page][cx4.PC];
#endif
	cx4.PC++;
	if (cx4.PC == 0) {
		bprintf(0, _T("PC == 0!  PB / Next:  %x  %x\n"), cx4.PB, cx4.PB_latch);
		cx4.PB = cx4.PB_latch;

		do_cache();
	}

	cycle_advance(1);

	return opcode;
}

#define is_internal_ram(a) ((a & 0x40e000) == 0x6000)

static uint32_t get_waitstate(uint32_t address)
{
	// assumptions: waitstate is always the same for cart ROM and RAM
	// .waitstate 0x33 (boot) 0x44 (set by X2/X3)
	return (is_internal_ram(address)) ? 0 : (cx4.waitstate & 0x07);
}

static void do_dma()
{
	uint32_t dest = cx4.dma_dest;
	uint32_t source = cx4.dma_source;

	uint32_t dest_cyc = get_waitstate(dest);
	uint32_t source_cyc = get_waitstate(source);
#if DEBUG_DMA
	bprintf(0, _T("dma\tsrc/dest/len:  %x  %x  %x\n"), source, dest, cx4.dma_length);
#endif
	for (int i = 0; i < cx4.dma_length; i++) {
		snes_write(cx4.snes, dest++, snes_read(cx4.snes, source++));
	}

	cx4.dma_timer = cx4.dma_length * (1 + dest_cyc + source_cyc);
#if DEBUG_DMA
	bprintf(0, _T("dma end, cycles %d\n"), cx4.dma_timer);
#endif
}

uint8_t cx4_read(uint32_t address)
{
	cx4_run(); // get up-to-date

	if ((address & 0xfff) < 0xc00) {
		return cx4.ram[address & 0xfff];
	}

	if (address >= 0x7f80 && (address & 0x3f) <= 0x2f) {
		address &= 0x3f;
		return get_byte(cx4.reg[address / 3], address % 3);
	}

	switch (address) {
		case 0x7f40: return (cx4.dma_source >> 0) & 0xff;
		case 0x7f41: return (cx4.dma_source >> 8) & 0xff;
		case 0x7f42: return (cx4.dma_source >> 16) & 0xff;
		case 0x7f43: return (cx4.dma_length >> 0) & 0xff;
		case 0x7f44: return (cx4.dma_length >> 8) & 0xff;
		case 0x7f45: return (cx4.dma_dest >> 0) & 0xff;
		case 0x7f46: return (cx4.dma_dest >> 8) & 0xff;
		case 0x7f47: return (cx4.dma_dest >> 16) & 0xff;
		case 0x7f48: return cx4.prg_cache_page;
		case 0x7f49: return (cx4.prg_base_address >> 0) & 0xff;
		case 0x7f4a: return (cx4.prg_base_address >> 8) & 0xff;
		case 0x7f4b: return (cx4.prg_base_address >> 16) & 0xff;
		case 0x7f4c: return cx4.prg_cache_lock;
		case 0x7f4d: return (cx4.prg_startup_bank >> 0) & 0xff;
		case 0x7f4e: return (cx4.prg_startup_bank >> 8) & 0xff;
		case 0x7f4f: return cx4.prg_startup_pc;
		case 0x7f50: return cx4.waitstate;
		case 0x7f51: return cx4.irqcfg;
		case 0x7f52: return cx4.unkcfg;
		case 0x7f53: case 0x7f54: case 0x7f55: case 0x7f56:
		case 0x7f57: case 0x7f59: case 0x7f5b: case 0x7f5c:
		case 0x7f5d: case 0x7f5e: case 0x7f5f: {
			// cx4 status (address 58 & 5a not mapped, returns 0)
			// tr.. ..is
			//           t.transfer in-progress (bus, cache, dma)
			//           r.running or transfer in-progress
			//           i.irq flag
			//           s.processor suspended
			const int transfer = (cx4.prg_cache_timer > 0) || (cx4.bus_timer > 0) || (cx4.dma_timer > 0);
			const int running = transfer || cx4.running;
			const uint8_t res = (transfer << 7) | (running << 6) | (get_I() << 1) | (cx4.suspend_timer != 0);
			return res;
		}
		case 0x7f60: case 0x7f61: case 0x7f62: case 0x7f63:
		case 0x7f64: case 0x7f65: case 0x7f66: case 0x7f67:
		case 0x7f68: case 0x7f69: case 0x7f6a: case 0x7f6b:
		case 0x7f6c: case 0x7f6d: case 0x7f6e: case 0x7f6f:
		case 0x7f70: case 0x7f71: case 0x7f72: case 0x7f73:
		case 0x7f74: case 0x7f75: case 0x7f76: case 0x7f77:
		case 0x7f78: case 0x7f79: case 0x7f7a: case 0x7f7b:
		case 0x7f7c: case 0x7f7d: case 0x7f7e: case 0x7f7f:
			// this provides the vector table for when the cx4 chip disconnects
			// the rom(s) from the bus during cpu/transfer operations
			return cx4.vectors[address & 0x1f];
	}

	return 0;
}

void cx4_write(uint32_t address, uint8_t data)
{
	cx4_run();

	if ((address & 0xfff) < 0xc00) {
		cx4.ram[address & 0xfff] = data;
		return;
	}

	if (address >= 0x7f80 && (address & 0x3f) <= 0x2f) {
		address &= 0x3f;
		set_byte(cx4.reg[address / 3], data, address % 3);
		return;
	}

	switch (address) {
		case 0x7f40: cx4.dma_source = (cx4.dma_source & 0xffff00) | (data << 0); break;
		case 0x7f41: cx4.dma_source = (cx4.dma_source & 0xff00ff) | (data << 8); break;
		case 0x7f42: cx4.dma_source = (cx4.dma_source & 0x00ffff) | (data << 16); break;
		case 0x7f43: cx4.dma_length = (cx4.dma_length & 0xff00) | (data << 0); break;
		case 0x7f44: cx4.dma_length = (cx4.dma_length & 0x00ff) | (data << 8); break;
		case 0x7f45: cx4.dma_dest = (cx4.dma_dest & 0xffff00) | (data << 0); break;
		case 0x7f46: cx4.dma_dest = (cx4.dma_dest & 0xff00ff) | (data << 8); break;
		case 0x7f47: cx4.dma_dest = (cx4.dma_dest & 0x00ffff) | (data << 16); do_dma(); break;
		case 0x7f48: cx4.prg_cache_page = data & 0x01; populate_cache(resolve_cache_address()); break;
		case 0x7f49: cx4.prg_base_address = (cx4.prg_base_address & 0xffff00) | (data << 0); break;
		case 0x7f4a: cx4.prg_base_address = (cx4.prg_base_address & 0xff00ff) | (data << 8); break;
		case 0x7f4b: cx4.prg_base_address = (cx4.prg_base_address & 0x00ffff) | (data << 16); break;
		case 0x7f4c: cx4.prg_cache_lock = data & 0x03; break;
		case 0x7f4d: cx4.prg_startup_bank = (cx4.prg_startup_bank & 0xff00) | data; break;
		case 0x7f4e: cx4.prg_startup_bank = (cx4.prg_startup_bank & 0x00ff) | ((data & 0x7f) << 8); break;
		case 0x7f4f:
			cx4.prg_startup_pc = data;
			if (cx4.running == 0) {
				cx4.PB = cx4.prg_startup_bank;
				cx4.PC = cx4.prg_startup_pc;
				cx4.running = 1;
				cx4.cycles_start = cx4.cycles;
#if DEBUG_STARTSTOP
				bprintf(0, _T("cx4 start @ %I64u  -  "), cx4.cycles);
				bprintf(0, _T("cache PB: %x\tPC: %x\tcache: %x\n"), cx4.PB, cx4.PC, resolve_cache_address());
#endif
				do_cache();
			}
			break;
		case 0x7f50: cx4.waitstate = data & 0x77; break; // oooo aaaa  o.rom, a.ram
		case 0x7f51:
			cx4.irqcfg = data & 0x01;
			if (cx4.irqcfg & IRQ_ACKNOWLEDGE) {
				cpu_setIrq(false);
				set_I(0);
			}
			break;
		case 0x7f52: cx4.unkcfg = data & 0x01; break; // this is up for debate, previously thought to en/disable 2nd rom chip on certain carts
		case 0x7f53: cx4.running = 0; bprintf(0, _T("7f53: CX4 - stop running!\n")); break;
		case 0x7f55: case 0x7f56: case 0x7f57: case 0x7f58:
		case 0x7f59: case 0x7f5a: case 0x7f5b: case 0x7f5c: {
			const int32_t offset = (address - 0x7f55);
			cx4.suspend_timer = (offset == 0) ? -1 : (offset << 5);
			break;
		}
		case 0x7f5d: cx4.suspend_timer = 0; break;
		case 0x7f5e: set_I(0); break;
		case 0x7f60: case 0x7f61: case 0x7f62: case 0x7f63:
		case 0x7f64: case 0x7f65: case 0x7f66: case 0x7f67:
		case 0x7f68: case 0x7f69: case 0x7f6a: case 0x7f6b:
		case 0x7f6c: case 0x7f6d: case 0x7f6e: case 0x7f6f:
		case 0x7f70: case 0x7f71: case 0x7f72: case 0x7f73:
		case 0x7f74: case 0x7f75: case 0x7f76: case 0x7f77:
		case 0x7f78: case 0x7f79: case 0x7f7a: case 0x7f7b:
		case 0x7f7c: case 0x7f7d: case 0x7f7e: case 0x7f7f:
			cx4.vectors[address & 0x1f] = data; break;
	}
}

// special function (purpose?) registers

static uint32_t get_sfr(uint8_t address)
{
	switch (address & 0x7f) {
		case 0x01: return (cx4.multiplier >> 24) & 0xffffff;
		case 0x02: return (cx4.multiplier >>  0) & 0xffffff;
		case 0x03: return cx4.bus_data;
		case 0x08: return cx4.rom_data;
		case 0x0c: return cx4.ram_data;
		case 0x13: return cx4.bus_address_pointer;
		case 0x1c: return cx4.ram_address_pointer;
		case 0x20: return cx4.PC;
		case 0x28: return cx4.PB_latch;
		case 0x2e: // rom
		case 0x2f: // ram
			cx4.bus_timer = ((cx4.waitstate >> ((~address & 1) << 2)) & 0x07) + 1;
			cx4.bus_address = cx4.bus_address_pointer;
			cx4.bus_mode = B_READ;
			return 0;
		case 0x50: return 0x000000;
		case 0x51: return 0xffffff;
		case 0x52: return 0x00ff00;
		case 0x53: return 0xff0000;
		case 0x54: return 0x00ffff;
		case 0x55: return 0xffff00;
		case 0x56: return 0x800000;
		case 0x57: return 0x7fffff;
		case 0x58: return 0x008000;
		case 0x59: return 0x007fff;
		case 0x5a: return 0xff7fff;
		case 0x5b: return 0xffff7f;
		case 0x5c: return 0x010000;
		case 0x5d: return 0xfeffff;
		case 0x5e: return 0x000100;
		case 0x5f: return 0x00feff;
		case 0x60: case 0x61: case 0x62: case 0x63:
		case 0x64: case 0x65: case 0x66: case 0x67:
		case 0x68: case 0x69: case 0x6a: case 0x6b:
		case 0x6c: case 0x6d: case 0x6e: case 0x6f:
			return cx4.reg[address & 0x0f];
	}

	bprintf(0, _T("get_sfr() - invalid %x\n"), address);

	return 0;
}

static void set_sfr(uint8_t address, uint32_t data)
{
	switch (address & 0x7f) {
		case 0x01: cx4.multiplier = (cx4.multiplier & 0x000000ffffffULL) | ((uint64_t)data << 24); break;
		case 0x02: cx4.multiplier = (cx4.multiplier & 0xffffff000000ULL) | ((uint64_t)data <<  0); break;
		case 0x03: cx4.bus_data = data; break;
		case 0x08: cx4.rom_data = data; break;
		case 0x0c: cx4.ram_data = data; break;
		case 0x13: cx4.bus_address_pointer = data; break;
		case 0x1c: cx4.ram_address_pointer = data; break;
		case 0x20: cx4.PC = data; break;
		case 0x28: cx4.PB_latch = (data & 0x7fff); break;
		case 0x2e: // rom
		case 0x2f: // ram
			cx4.bus_timer = ((cx4.waitstate >> ((~address & 1) << 2)) & 0x07) + 1;
			cx4.bus_address = cx4.bus_address_pointer;
			cx4.bus_mode = B_WRITE;
			break;
		case 0x60: case 0x61: case 0x62: case 0x63:
		case 0x64: case 0x65: case 0x66: case 0x67:
		case 0x68: case 0x69: case 0x6a: case 0x6b:
		case 0x6c: case 0x6d: case 0x6e: case 0x6f:
			cx4.reg[address & 0x0f] = data; break;

		default:
			bprintf(0, _T("set_sfr() - invalid %x (%x)\n"), address, data);
			break;
	}
}

static void jmpjsr(bool is_jsr, bool take, uint8_t page, uint8_t address) {
	if (take) {
		if (is_jsr) {
			cx4.stack[cx4.SP].PC = cx4.PC;
			cx4.stack[cx4.SP].PB = cx4.PB;
			cx4.SP = (cx4.SP + 1) & 0x07;
		}
		if (page) {
			cx4.PB = cx4.PB_latch;
			do_cache();
		}
		cx4.PC = address;
		cycle_advance(2);
	}
}

static uint32_t add(uint32_t a1, uint32_t a2)
{
	const uint32_t sum = a1 + a2;

	set_C(sum & 0xff000000);
	set_V(((a1 ^ sum) & (a2 ^ sum)) & 0x800000);
	set_N(sum & 0x800000);
	set_Z(!(sum & 0xffffff));

	return sum & 0xffffff;
}

static uint32_t sub(uint32_t m, uint32_t s)
{
	const int32_t diff = m - s;

	set_C(!(diff < 0));
	set_V(((m ^ diff) & (s ^ diff)) & 0x800000);
	set_NZ(diff);

	return diff & 0xffffff;
}

#define DIRECT_IMM 0x0400
#define get_immed() ((opcode & DIRECT_IMM) ? immed : get_sfr(immed))

static void run_insn()
{
	const uint16_t opcode = fetch();
	const uint8_t sub_op = (opcode & 0x0300) >> 8;
	const uint8_t immed  = (opcode & 0x00ff) >> 0;
	uint32_t temp = 0;

	switch (opcode & 0xfc00) {
		case 0x0000: // nop
			break;

		case 0x0800: // jmp page,pc
			jmpjsr(false, true, sub_op, immed); break;
		case 0x0c00: // jmp if flag,page,pc
			jmpjsr(false, get_Z(), sub_op, immed); break;
		case 0x1000:
			jmpjsr(false, get_C(), sub_op, immed); break;
		case 0x1400:
			jmpjsr(false, get_N(), sub_op, immed); break;
		case 0x1800:
			jmpjsr(false, get_V(), sub_op, immed); break;
		case 0x2800: // jsr page,pc
			jmpjsr(true, true, sub_op, immed); break;
		case 0x2c00: // jsr if flag,page,pc
			jmpjsr(true, get_Z(), sub_op, immed); break;
		case 0x3000:
			jmpjsr(true, get_C(), sub_op, immed); break;
		case 0x3400:
			jmpjsr(true, get_N(), sub_op, immed); break;
		case 0x3800:
			jmpjsr(true, get_V(), sub_op, immed); break;

		case 0x3c00: // return
			cx4.SP = (cx4.SP - 1) & 0x07;
			cx4.PC = cx4.stack[cx4.SP].PC;
			cx4.PB = cx4.stack[cx4.SP].PB;
			do_cache();
			cycle_advance(2);
			break;

		case 0x1c00: // finish/execute bus transfer
			cycle_advance(cx4.bus_timer);
			break;

		case 0x2400: // skip cc,imm
			if (!!(cx4.cc & (1 << ((0x13 >> sub_op) & 3))) == immed) { // note: re-indexes processor flags to match order of sub_op [O,C,Z,N]
				fetch();
			}
			break;

		case 0x4000: // inc bus address
			cx4.bus_address_pointer = (cx4.bus_address_pointer + 1) & 0xffffff;
			break;

		case 0x4800: // cmp imm,A
		case 0x4c00:
			sub(get_immed(), get_A());
			break;

		case 0x5000: // cmp A,imm
		case 0x5400:
			sub(get_A(), get_immed());
			break;

		case 0x5800: // sign_extend A[?,8,16,? bit]
			cx4.A = sign_extend(cx4.A, sub_op << 3) & 0xffffff;
			set_NZ(cx4.A);
			break;

		case 0x6000: // mov x,imm
		case 0x6400:
			switch (sub_op) {
				case 0: cx4.A = get_immed(); break;
				case 1: cx4.bus_data = get_immed(); break;
				case 2: cx4.bus_address_pointer = get_immed(); break;
				case 3: cx4.PB_latch = get_immed() & 0x7fff; break;
			}
			break;

		case 0xe000: // mov sfr[imm],x
			switch (sub_op) {
				case 0: set_sfr(immed, cx4.A); break;
				case 1: set_sfr(immed, cx4.bus_data); break;
				case 2: set_sfr(immed, cx4.bus_address_pointer); break;
				case 3: set_sfr(immed, cx4.PB_latch); break;
			}
			break;

		case 0x6800: // RDRAM subop,A
			temp = cx4.A & 0xfff;
			if (temp < 0xc00) {
				set_byte(cx4.ram_data, cx4.ram[temp], sub_op);
			}
			break;
		case 0x6c00: // RDRAM imm,A
			temp = (cx4.ram_address_pointer + immed) & 0xfff;
			if (temp < 0xc00) {
				set_byte(cx4.ram_data, cx4.ram[temp], sub_op);
			}
			break;

		case 0xe800: // WRRAM subop,A
			temp = cx4.A & 0xfff;
			if (temp < 0xc00) {
				cx4.ram[temp] = get_byte(cx4.ram_data, sub_op);
			}
			break;
		case 0xec00: // WRRAM imm,A
			temp = (cx4.ram_address_pointer + immed) & 0xfff;
			if (temp < 0xc00) {
				cx4.ram[temp] = get_byte(cx4.ram_data, sub_op);
			}
			break;

		case 0x7000: // RDROM
			cx4.rom_data = cx4.rom[cx4.A & 0x3ff];
			break;
		case 0x7400:
			cx4.rom_data = cx4.rom[(sub_op << 8) | immed];
			break;

		case 0x7c00: // mov PB_latch[l/h],imm
			set_byte(cx4.PB_latch, immed, sub_op);
			cx4.PB_latch &= 0x7fff;
			break;

		case 0x8000: // ADD A,imm
		case 0x8400:
			cx4.A = add(get_A(), get_immed());
			break;

		case 0x8800: // SUB imm,A
		case 0x8c00:
			cx4.A = sub(get_immed(), get_A());
			break;

		case 0x9000: // SUB A,imm
		case 0x9400:
			cx4.A = sub(get_A(), get_immed());
			break;

		case 0x9800: // MUL imm,A
		case 0x9c00:
			cx4.multiplier = ((int64_t)sign_extend(get_immed(), 24) * sign_extend(cx4.A, 24)) & 0xffffffffffffULL;
			break;

		case 0xa000: // XNOR A,imm
		case 0xa400:
			set_A(~(get_A()) ^ get_immed());
			set_NZ(cx4.A);
			break;

		case 0xa800: // XOR A,imm
		case 0xac00:
			set_A((get_A()) ^ get_immed());
			set_NZ(cx4.A);
			break;

		case 0xb000: // AND A,imm
		case 0xb400:
			set_A((get_A()) & get_immed());
			set_NZ(cx4.A);
			break;

		case 0xb800: // OR A,imm
		case 0xbc00:
			set_A((get_A()) | get_immed());
			set_NZ(cx4.A);
			break;

		case 0xc000: // SHR A,imm
		case 0xc400:
			set_A(cx4.A >> (get_immed() & 0x1f));
			set_NZ(cx4.A);
			break;

		case 0xc800: // ASR A,imm
		case 0xcc00:
			set_A(sign_extend(cx4.A, 24) >> (get_immed() & 0x1f));
			set_NZ(cx4.A);
			break;

		case 0xd000: // ROR A,imm
		case 0xd400:
			temp = get_immed() & 0x1f;
			set_A((cx4.A >> temp) | (cx4.A << (24 - temp)));
			set_NZ(cx4.A);
			break;

		case 0xd800: // SHL A,imm
		case 0xdc00:
			set_A(cx4.A << (get_immed() & 0x1f));
			set_NZ(cx4.A);
			break;

		case 0xf000: // XCHG A,regs
			temp = cx4.A;
			cx4.A = cx4.reg[immed & 0xf];
			cx4.reg[immed & 0xf] = temp;
			break;

		case 0xf800: // clear
			cx4.A = cx4.ram_address_pointer = cx4.ram_data = cx4.PB_latch = 0x00;
			break;

		case 0xfc00: // stop
#if DEBUG_STARTSTOP
			bprintf(0, _T("cx4 OP-stop, cycles ran %d\n"), (int)((int64_t)cx4.cycles - cx4.cycles_start));
#endif
			cx4.running = 0;
			if (~cx4.irqcfg & IRQ_ACKNOWLEDGE) {
				set_I(1);
				cpu_setIrq(true);
			}
			break;
	}
}

static void tally_cycles()
{
	cx4.sync_to = (uint64_t)cx4.snes->cycles * cx4.CyclesPerMaster;
}

static inline int64_t cycles_left()
{
	return cx4.sync_to - cx4.cycles;
}

void cx4_run()
{
	int tcyc = 0;
	tally_cycles();

	while (cx4.cycles < cx4.sync_to) {
		if (cx4.prg_cache_timer) {
			tcyc = (cycles_left() > cx4.prg_cache_timer) ? cx4.prg_cache_timer : 1;
			cycle_advance(tcyc);
			cx4.prg_cache_timer -= tcyc;
		} else if (cx4.dma_timer) {
			tcyc = (cycles_left() > cx4.dma_timer) ? cx4.dma_timer : 1;
			cycle_advance(tcyc);
			cx4.dma_timer -= tcyc;
		} else if (cx4.suspend_timer) {
			tcyc = (cycles_left() > cx4.suspend_timer) ? cx4.suspend_timer : 1;
			cycle_advance(tcyc);
			cx4.suspend_timer -= tcyc;
		} else if (!cx4.running) {
			cycle_advance(cycles_left());
		} else {
			run_insn();
		}
	}
}
