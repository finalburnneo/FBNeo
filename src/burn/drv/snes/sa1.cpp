// SA-1 mapper/simulator for LakeSnes, (c) 2024-25 dink
// License: MIT

#include "snes.h"
#include "cpu_sa1.h"
#include "sa1.h"

static uint64_t sa1_cycles;
static Snes *snes_ctx;

static uint8_t *sram;
static uint32_t sram_size;
static uint32_t sram_mask;
static uint8_t *iram;

static uint8_t *rom;
static uint32_t rom_size;
static uint32_t rom_mask;

static uint8_t openbus;

static uint8_t scpu_control;
static uint8_t scpu_irq_enable;
static uint16_t scpu_irq_vector;
static uint16_t scpu_nmi_vector;
static uint8_t scpu_irq_pending;
static bool scpu_in_irq;

static uint8_t sa1_control;
static uint8_t sa1_irq_enable;
static uint16_t sa1_reset_vector;
static uint16_t sa1_nmi_vector;
static uint16_t sa1_irq_vector;
static uint8_t sa1_irq_pending;

enum {
	sa1_IRQ			= 1 << 7,
	sa1_IRQ_TIMER	= 1 << 6,
	sa1_IRQ_DMA		= 1 << 5,
	sa1_IRQ_NMI		= 1 << 4
};

#define set_byte(var, data, offset) (var = (var & (~(0xff << (((offset) & 3) * 8)))) | (data << (((offset) & 3) * 8)))

#define sa1_set_irq(flg, f) do { sa1_irq_pending = (sa1_irq_pending & ~flg) | ((f) ? flg : 0); } while (0)
#define scpu_set_irq(flg, f) do { scpu_irq_pending = (scpu_irq_pending & ~flg) | ((f) ? flg : 0); } while (0)

static uint8_t timer_mode;
static uint16_t timer_hpos;
static uint16_t timer_vpos;
static uint16_t hpos_latch;
static uint16_t vpos_latch;

static uint8_t supermmc[4];

static uint32_t bwram_snes_bank;
static uint32_t bwram_sa1_bank;
static uint8_t bwram_sa1_mode;
static uint8_t bwram_sa1_type; // 0 4bpp, !0 2bpp

static uint8_t math_mode;
static uint16_t math_param_a;
static uint16_t math_param_b;
static uint64_t math_result;
static uint8_t math_overflow;
static const uint64_t math_40bit_mask = ((uint64_t)1 << 40) - 1;
static const uint64_t math_overflow_mask = ~math_40bit_mask;

static uint8_t dma_control;
static uint32_t dma_src;
static uint32_t dma_dst;
static uint16_t dma_len;
static uint8_t dma_charconv1_active;

static uint8_t dma_charconv_control;
static uint8_t dma_charconv_bpp;
static uint8_t dma_charconv_brf[0x10];
static uint8_t dma_charconv_line;

static uint32_t vari_src;
static uint32_t vari_temp;
static uint8_t vari_bitcount;
static uint8_t vari_width;
static uint8_t vari_clock;

// memory mapping (test)
static const int banks_len = 0x100; // 00 - ff
static const int page_len = 0x10000; // 0000 - ffff
static const int slot_len = 0x1000; // 4k (smallest mappable segment)
static const int map_len = (page_len * banks_len) / slot_len; // == 0x1000
static uint8_t *rom_map[map_len];
static uint8_t rom_map_type[map_len];

static uint8_t (*sa1_rom_map_read[map_len])(uint32_t);
static void (*sa1_rom_map_write[map_len])(uint32_t, uint8_t);

static uint8_t (*scpu_rom_map_read[map_len])(uint32_t);
static void (*scpu_rom_map_write[map_len])(uint32_t, uint8_t);

#if 0  /* debug unmapped? */
// note: unmapped writes from scpu catches the entire snes bus outside of sa-1(!) (this is why it is /**/'d)
static uint8_t sa1_unmap_read(uint32_t address) { bprintf(0, _T("SA1: unmapped read: %x\n"), address); return 0xff; }
static uint8_t scpu_unmap_read(uint32_t address) { bprintf(0, _T("SNES: unmapped read: %x\n"), address); return 0xff; }
static void sa1_unmap_write(uint32_t address, uint8_t data) { bprintf(0, _T("SA1: unmapped write:  %x  %x\n"), address, data); }
static void scpu_unmap_write(uint32_t address, uint8_t data) { /*bprintf(0, _T("SNES: unmapped write:  %x  %x\n"), address, data);*/ }
#else
static uint8_t sa1_unmap_read(uint32_t address) { return 0xff; }
static uint8_t scpu_unmap_read(uint32_t address) { return 0xff; }
static void sa1_unmap_write(uint32_t address, uint8_t data) { }
static void scpu_unmap_write(uint32_t address, uint8_t data) { }
#endif

static uint8_t *rom_unmap = NULL;

#define S_MAP_TYPE_UNMAP 0
#define S_MAP_TYPE_PRG   1

#define SA1	0x1000
#define SCPU 0x2000

static void map_handlers_sa1();
static void map_handlers_scpu();
static void map_rw(uint32_t cpu, uint8_t bank_from, uint8_t bank_to, uint16_t adr_from, uint16_t adr_to, uint8_t (*rd_handler)(uint32_t), void (*wr_handler)(uint32_t, uint8_t));
static void map_rom(uint8_t bank_from, uint8_t bank_to, uint16_t adr_from, uint16_t adr_to, uint32_t slot);

uint64_t sa1_getcycles() {
	return sa1_cycles;
}

static void map_init() {
	memset(rom_map, 0, sizeof(rom_map));
	memset(rom_map_type, 0, sizeof(rom_map_type));

	if (rom_unmap == NULL) {
		rom_unmap = (uint8_t*)BurnMalloc(slot_len);
	}

	for (int i = 0; i < map_len; i++) {
		rom_map[i] = rom_unmap;
	}

	// unmapped rom reads return 0xff
	for (int i = 0; i < slot_len; i++) {
		rom_unmap[i] = 0xff;
	}

	map_rw(SA1, 0x00, 0xff, 0x0000, 0xffff, sa1_unmap_read, sa1_unmap_write);
	map_rw(SCPU, 0x00, 0xff, 0x0000, 0xffff, scpu_unmap_read, scpu_unmap_write);

	map_handlers_sa1();
	map_handlers_scpu();
}

static void map_exit() {
	BurnFree(rom_unmap);
	rom_unmap = NULL;
}

static void map_rom(uint8_t bank_from, uint8_t bank_to, uint16_t adr_from, uint16_t adr_to, uint32_t slot) {

	slot *= 0x100;
	const uint32_t bank_stride = (adr_to + 1) - adr_from;
	//bprintf(0, _T("map_rom(%02x, %02x, %04x, %04x, %x, %x)\n"), bank_from, bank_to, adr_from, adr_to, bank_stride, slot);

	for (int bank = bank_from; bank <= bank_to; bank++) {
		for (int adr = adr_from; adr <= adr_to; adr += slot_len) {
			const uint32_t map_offset = ((bank & 0xff) << 4) | (adr >> 12);
			const uint32_t rom_offset = (adr - adr_from) + ((bank - bank_from) * bank_stride) + (slot * slot_len);
		   // bprintf(0, _T("scpu_map[%x] = %x (mirr %x)\n"), map_offset, rom_offset, rom_offset % rom_size);
			rom_map[map_offset] = rom + rom_offset % rom_size;
			rom_map_type[map_offset] = S_MAP_TYPE_PRG;
		}
	}
}

// note: mapping NULL for a handler doesn't unmap
static void map_rw(uint32_t cpu, uint8_t bank_from, uint8_t bank_to, uint16_t adr_from, uint16_t adr_to, uint8_t (*rd_handler)(uint32_t), void (*wr_handler)(uint32_t, uint8_t)) {

	//bprintf(0, _T("map_rw: %S:  %x  %x  %x  %x\n"), (cpu == SA1) ? "sa1" : "scpu", bank_from, bank_to, adr_from, adr_to);

	for (int bank = bank_from; bank <= bank_to; bank++) {
		for (int adr = adr_from; adr <= adr_to; adr += slot_len) {
			const uint32_t map_offset = ((bank & 0xff) << 4) | (adr >> 12);
			if (rd_handler != NULL) {
				switch (cpu) {
					case SA1: sa1_rom_map_read[map_offset] = rd_handler; break;
					case SCPU: scpu_rom_map_read[map_offset] = rd_handler; break;
				}
			}
			if (wr_handler != NULL) {
				switch (cpu) {
					case SA1: sa1_rom_map_write[map_offset] = wr_handler; break;
					case SCPU: scpu_rom_map_write[map_offset] = wr_handler; break;
				}
			}
		}
	}
}

bool sa1_isrom_address(uint32_t address)
{
	return rom_map_type[address >> 12] == S_MAP_TYPE_PRG;
}

bool sa1_snes_rom_conflict() {
	return rom_map_type[snes_ctx->adrBus >> 12] == S_MAP_TYPE_PRG;
}

static inline uint32_t map_get_handler(uint32_t address)
{
	return address >> 12;
}

static inline uint8_t sa1_bus_read(uint32_t address) {
	return sa1_rom_map_read[map_get_handler(address)](address);
}

static void map_update() {
	map_rom(0x00, 0x1f, 0x8000, 0xffff, (supermmc[0] & 0x80) ? (supermmc[0] & 0x07) : 0);
	map_rom(0x20, 0x3f, 0x8000, 0xffff, (supermmc[1] & 0x80) ? (supermmc[1] & 0x07) : 1);
	map_rom(0x80, 0x9f, 0x8000, 0xffff, (supermmc[2] & 0x80) ? (supermmc[2] & 0x07) : 2);
	map_rom(0xa0, 0xbf, 0x8000, 0xffff, (supermmc[3] & 0x80) ? (supermmc[3] & 0x07) : 3);

	map_rom(0xc0, 0xcf, 0x0000, 0xffff, (supermmc[0] & 0x07));
	map_rom(0xd0, 0xdf, 0x0000, 0xffff, (supermmc[1] & 0x07));
	map_rom(0xe0, 0xef, 0x0000, 0xffff, (supermmc[2] & 0x07));
	map_rom(0xf0, 0xff, 0x0000, 0xffff, (supermmc[3] & 0x07));
}


// forwards jib
static inline uint8_t iram_read(uint32_t address);
static inline void iram_write(uint32_t address, uint8_t data);
static inline uint8_t bwram_read(uint32_t address);
static inline void bwram_write(uint32_t address, uint8_t data);
static inline uint8_t rom_read(uint32_t address);

static void check_hv_timers();
static void check_pending_irqs();

static inline const bool sa1_running() {
	return (sa1_control & (0x40 | 0x20) ) == 0;
}

static inline void acc_cyc() {
	sa1_cycles += 2;
	if (sa1_cycles & 2) check_hv_timers();
}

void snes_sa1_init(void *mem, uint8_t *srom, int32_t sromsize, void *s_ram, int32_t s_ram_size)
{
	if (iram != NULL) return; // snes cart reset calls this, ignore if already initted.

	//bprintf(0, _T("sa1: snes_sa1_init().\n"));

	iram = (UINT8*)BurnMalloc(0x800);

	snes_ctx = (Snes *)mem;

	sram_size = s_ram_size;
	sram_mask = s_ram_size - 1;
	sram = (uint8_t*)s_ram;

	if (sram_size == 0) {
		bprintf(0, _T("SA1 Init Error: SRAM required for operation.\n"));
		// todo: allocate default sram? maybe?
	}

	cpusa1_init();
	map_init();

	rom = srom;
	rom_size = sromsize;
	rom_mask = rom_size - 1;

	bprintf(0, _T("rom size/mask  %x / %x \n"), rom_size, rom_mask);
}

void snes_sa1_handleState(StateHandler* sh)
{
	cpusa1_handleState(sh);
	sh_handleByteArray(sh, (uint8_t*)iram, 0x800);
	sh_handleByteArray(sh, &dma_charconv_brf[0], 0x10);
	sh_handleLongLongs(sh, &sa1_cycles, &math_result, NULL);
	sh_handleBytes(sh, &openbus, &scpu_control, &scpu_irq_enable, &scpu_irq_pending, &sa1_control, &sa1_irq_enable, &sa1_irq_pending, &timer_mode, &supermmc[0], &supermmc[1], &supermmc[2], &supermmc[3], &bwram_sa1_mode, &bwram_sa1_type, &math_mode, &math_overflow, &dma_control, &dma_charconv1_active, &dma_charconv_control, &dma_charconv_bpp, &dma_charconv_line, &vari_bitcount, &vari_width, &vari_clock, NULL);
	sh_handleBools(sh, &scpu_in_irq, NULL);
	sh_handleWords(sh, &scpu_irq_vector, &scpu_nmi_vector, &sa1_reset_vector, &sa1_nmi_vector, &sa1_irq_vector, &timer_hpos, &timer_vpos, &hpos_latch, &vpos_latch, &bwram_snes_bank, &bwram_sa1_bank, &math_param_a, &math_param_b, &dma_len, NULL);
	sh_handleInts(sh, &dma_src, &dma_dst, &vari_src, &vari_temp, NULL);

	if (sh->saving == false) {
		map_update();
	}
}

void snes_sa1_run()
{
  // note: we run sa1 cpu @ snes m-cyc, but count 2 cycles per, for 10.7mhz
  // w/o having to divide down
  const uint64_t sa1_sync_to = snes_ctx->cycles;

  while (sa1_cycles < sa1_sync_to) {
	cpusa1_runOpcode();
  }
}

void snes_sa1_exit()
{
	//bprintf(0, _T("sa1: snes_sa1_exit()\n"));

	BurnFree(iram);
	iram = NULL;

	map_exit();

	cpusa1_free();
}

void snes_sa1_reset()
{
	//bprintf(0, _T("sa1: snes_sa1_reset()\n"));

	memset(iram, 0xff, 0x800);

	openbus = 0xff;

	scpu_control = 0x00;
	scpu_irq_enable = 0x00;
	scpu_irq_vector = 0x00;
	scpu_nmi_vector = 0x00;
	scpu_irq_pending = 0x00;
	scpu_in_irq = 0x00;

	sa1_control = 0x20 | 0x40; // cpu in reset & stopped
	sa1_irq_enable = 0x00;
	sa1_reset_vector = 0x00;
	sa1_nmi_vector = 0x00;
	sa1_irq_vector = 0x00;
	sa1_irq_pending = 0x00;

	timer_mode = 0x00;
	timer_hpos = 0x00;
	timer_vpos = 0x00;
	hpos_latch = 0x00;
	vpos_latch = 0x00;

	supermmc[0] = 0;
	supermmc[1] = 1;
	supermmc[2] = 2;
	supermmc[3] = 3;

	bwram_snes_bank = 0x00;
	bwram_sa1_bank = 0x00;
	bwram_sa1_mode = 0x00;
	bwram_sa1_type = 0x00;

	math_mode = 0x00;
	math_param_a = 0x00;
	math_param_b = 0x00;
	math_result = 0x00;
	math_overflow = 0x00;

	dma_control = 0x00;
	dma_src = 0x00;
	dma_dst = 0x00;
	dma_len = 0x00;

	dma_charconv1_active = 0x00;
	dma_charconv_control = 0x00;
	dma_charconv_bpp = 0x00;
	memset(dma_charconv_brf, 0x00, sizeof(dma_charconv_brf));
	dma_charconv_line = 0x00;

	vari_src = 0x00;
	vari_temp = 0x00;
	vari_bitcount = 0x00;
	vari_width = 0x00;
	vari_clock = 0x00;

	map_update();

	sa1_cycles = 0;

	cpusa1_reset(true);
	cpusa1_setHalt(true);
}

static void check_hv_timers()
{
	switch (timer_mode & 0x03) {
		case 0x01:
			if (snes_ctx->hPos == timer_hpos) {
				//bprintf(0, _T("h @ %d\n"), timer_hpos);
				sa1_set_irq(sa1_IRQ_TIMER, 1);
				check_pending_irqs();
			}
			break;
		case 0x02:
			if ((snes_ctx->hPos == 0) && (snes_ctx->vPos == timer_vpos)) {
				//bprintf(0, _T("v @ %d\n"), timer_vpos);
				sa1_set_irq(sa1_IRQ_TIMER, 1);
				check_pending_irqs();
			}
			break;
		case 0x03:
			if ((snes_ctx->hPos == timer_hpos) && (snes_ctx->vPos == timer_vpos)) {
				//bprintf(0, _T("hv @ %d,%d\n"), timer_hpos, timer_vpos);
				sa1_set_irq(sa1_IRQ_TIMER, 1);
				check_pending_irqs();
			}
			break;
	}
}

static void check_pending_irqs()
{
	if (scpu_irq_pending & scpu_irq_enable & sa1_IRQ) {
		scpu_in_irq = true;
		cpu_setIrq(true);
		//bprintf(0, _T("sa1->snes irq\n"));
	} else {
		if (scpu_in_irq) {
			cpu_setIrq(false); // must be latched! as it will conflict with regular line-irq's
			scpu_in_irq = false;
		}
	}

	if (sa1_running() && (sa1_irq_pending & sa1_irq_enable & (sa1_IRQ | sa1_IRQ_TIMER | sa1_IRQ_DMA))) {
		cpusa1_setIrq(true);
		//bprintf(0, _T("sa1->sa1 irq\n"));
	} else {
		cpusa1_setIrq(false);
	}

	if (sa1_running() && (sa1_irq_pending & sa1_irq_enable & sa1_IRQ_NMI)) {
		cpusa1_nmi();
		//bprintf(0, _T("sa1->sa1 nmi\n"));
	}
}

static void dma_xfer()
{
	const uint32_t dma_src_dev = dma_control & 0x03;
	const uint32_t dma_dst_dev = (dma_control & 0x04) >> 2;
	//bprintf(0, _T("sa1 : dma_xfer %x (%x) -> %x (%x)  size %x\n"), dma_src, dma_src_dev, dma_dst, dma_dst_dev, dma_len);

	while (dma_len > 0) {
		uint8_t src = 0;
		switch (dma_src_dev) {
			case 0: src = rom_read(dma_src); break;
			case 1: src = bwram_read(dma_src & 0xfffff); acc_cyc(); break;
			case 2: src = iram_read(dma_src); break;
		}
		acc_cyc();

		switch (dma_dst_dev) {
			case 0: iram_write(dma_dst, src); break;
			case 1: bwram_write(dma_dst & 0xfffff, src); acc_cyc(); break;
		}

		dma_src++;
		dma_dst++;
		dma_len--;
	}

	sa1_set_irq(sa1_IRQ_DMA, 1);
	check_pending_irqs();
}

static void dma_regs(uint32_t address, uint8_t data)
{
	switch (address) {
		case 0x2230:
			// 7       0
			// epc1 .dss
			// s. src device (prg, bwram, iram, unk)
			// d. dst device (iram, bwram)
			// 1. char conversion type-1 / type-2 (unset)
			// c. char conversion enable
			// p. dma priority (?)
			// e. dma enable
			dma_control = data;
			if (~dma_control & 0x80) {
				dma_charconv_line = 0;
			}
			break;
		case 0x2231: // char conv. dma
			dma_charconv_control = data & 0x1f;
			dma_charconv_bpp = 1 << (~data & 3);
			if (data & 0x80) {
				dma_charconv1_active = 0;
			}
			break;
		case 0x2232:
		case 0x2233:
		case 0x2234:
			set_byte(dma_src, data, address - 0x2232);
			break;
		case 0x2235:
		case 0x2236:
		case 0x2237:
			set_byte(dma_dst, data, address - 0x2235);
			if (dma_control & 0x80) {
				switch (address) {
					case 0x2236:
						if (dma_control & 0x20 && dma_control & 0x10) {
							dma_charconv1_active = 1;
							scpu_set_irq(sa1_IRQ_DMA, 1);
							check_pending_irqs();
						}
						if ((~dma_control & 0x20) && (dma_control & 0x04) == 0x00) {
							dma_xfer();
						}
						break;
					case 0x2237:
						if ((~dma_control & 0x20) && (dma_control & 0x04) == 0x04) {
							dma_xfer();
						}
						break;
				}
			}
			break;
		case 0x2238:
		case 0x2239:
			set_byte(dma_len, data, address & 1);
			break;
	}
}

static uint8_t sa1_regs_read(uint32_t address)
{
	address &= 0xffff;
//	bprintf(0, _T("sa1_regs_read %x\n"), address);
	switch (address) {
		case 0x2301:
			return (sa1_control & 0x0f) | (sa1_irq_pending & 0xf0);

		case 0x2302:
			hpos_latch = snes_ctx->hPos / 4;
			vpos_latch = snes_ctx->vPos;
			return hpos_latch & 0xff;
		case 0x2303:
			return (hpos_latch >> 8) & 0xff;
		case 0x2304:
			return vpos_latch & 0xff;
		case 0x2305:
			return (vpos_latch >> 8) & 0xff;

		case 0x2306:
			return math_result & 0xff;
		case 0x2307:
			return (math_result >> 8) & 0xff;
		case 0x2308:
			return (math_result >> 16) & 0xff;
		case 0x2309:
			return (math_result >> 24) & 0xff;
		case 0x230a:
			return (math_result >> 32) & 0xff;
		case 0x230b:
			return math_overflow;

		case 0x230c:
		case 0x230d:
			if ((vari_src & 0xc0c000) == 0x000000) return 0xff; // prevent access to regs (recursion)
			set_byte(vari_temp, sa1_bus_read(vari_src + 0), 0);
			set_byte(vari_temp, sa1_bus_read(vari_src + 1), 1);
			set_byte(vari_temp, sa1_bus_read(vari_src + 2), 2);
			vari_temp >>= vari_bitcount + ((address & 1) << 3);
			if (address & 1 && vari_clock) {
				vari_src += (vari_bitcount += vari_width) >> 3;
				vari_bitcount &= 0x07;
			}
			return vari_temp & 0xff;

		default:
//			bprintf(0, _T("sa1.regs_read(%x) unimpl openbus %x\n"), address, openbus);
			break;
	}

	return openbus;
}

static void sa1_regs_write(uint32_t address, uint8_t data)
{
	address &= 0xffff;
	//bprintf(0, _T("sa1_regs_write: %x  %x\n"), address, data);
	switch (address) {
		case 0x2209:
			scpu_control = data;
			scpu_set_irq(sa1_IRQ, data & sa1_IRQ);
			check_pending_irqs();
			break;
		case 0x220a:
			sa1_irq_enable = data;
			//bprintf(0, _T("sa1_irq_enab %x\n"), data);
			check_pending_irqs();
			break;
		case 0x220b:
			//bprintf(0, _T("irq_ack: %x\n"), data);
			if (data & sa1_IRQ) sa1_set_irq(sa1_IRQ, 0);
			if (data & sa1_IRQ_TIMER) sa1_set_irq(sa1_IRQ_TIMER, 0);
			if (data & sa1_IRQ_DMA) sa1_set_irq(sa1_IRQ_DMA, 0);
			if (data & sa1_IRQ_NMI) sa1_set_irq(sa1_IRQ_NMI, 0);
			check_pending_irqs();
			break;
		case 0x220c:
		case 0x220d:
			set_byte(scpu_nmi_vector, data, address & 1);
			break;
		case 0x220e:
		case 0x220f:
			set_byte(scpu_irq_vector, data, address & 1);
			break;

		case 0x2210:
			timer_mode = data;
			break;
		case 0x2211: // linear timer?
			break;
		case 0x2212:
			timer_hpos = (timer_hpos & 0x0400) | (data << 2);
			break;
		case 0x2213:
			timer_hpos = (timer_hpos & 0x03fc) | ((data & 1) << 10);
			break;
		case 0x2214:
			timer_vpos = (timer_vpos & 0x0100) | data;
			break;
		case 0x2215:
			timer_vpos = (timer_vpos & 0x00ff) | ((data & 1) << 8);
			break;

		case 0x2225:
			bwram_sa1_bank = (data & 0x7f) * 0x2000;
			bwram_sa1_mode = data & 0x80;
			break;
		case 0x2227:
			// bwram write protect
			break;
		case 0x222a:
			// iram write protect bitmask
			break;

		case 0x2230:
		case 0x2231:
		case 0x2232:
		case 0x2233:
		case 0x2234:
		case 0x2235:
		case 0x2236:
		case 0x2237:
		case 0x2238:
		case 0x2239:
			dma_regs(address, data);
			break;

		case 0x223f: // bwram bitmap 4bpp (0) / 2bpp (&0x80)
			bwram_sa1_type = data & 0x80;
			//bprintf(0, _T("bwram bitmap type %x bpp\n"), (bwram_sa1_type) ? 2 : 4);
			break;

		case 0x2240: // dma type-2 char conversion
		case 0x2241:
		case 0x2242:
		case 0x2243:
		case 0x2244:
		case 0x2245:
		case 0x2246:
		case 0x2247:
		case 0x2248:
		case 0x2249:
		case 0x224a:
		case 0x224b:
		case 0x224c:
		case 0x224d:
		case 0x224e:
		case 0x224f:
			dma_charconv_brf[address & 0xf] = data;
			if ((address & 7) == 7 && dma_control & 0x80) {
				if (dma_control & 0x20 && ~dma_control & 0x10) {
					const uint8_t bank = ((address & 0xf) == 0xf) << 3;
					const uint16_t mask = 0x700 | (dma_charconv_bpp * 0x10);
					address = (dma_dst & mask) + (dma_charconv_line & 8) * dma_charconv_bpp + ((dma_charconv_line & 7) << 1);

					for (int b = 0; b < dma_charconv_bpp; b++) {
						uint8_t pxl = 0;

						for (int x = 0; x < 8; x++)
							pxl |= ((dma_charconv_brf[bank + x] >> b) & 1) << (x ^ 7);

						iram_write(address + ((b & 6) << 3) + (b & 1), pxl);
					}

					dma_charconv_line = (dma_charconv_line + 1) & 0xf;
				}
			}
			break;

		case 0x2250:
			math_mode = data & 0x03;
			if (data & (1 << 1)) {
				math_result = 0;
			}
			break;
		case 0x2251:
			set_byte(math_param_a, data, ~address & 1);
			break;
		case 0x2252:
			set_byte(math_param_a, data, ~address & 1);
			break;
		case 0x2253:
			set_byte(math_param_b, data, ~address & 1);
			break;
		case 0x2254:
			set_byte(math_param_b, data, ~address & 1);
			switch (math_mode) {
				case 0:
					math_result = (uint32_t)((int16_t)math_param_a * (int16_t)math_param_b);
					math_param_b = 0;
					break;
				case 1:
					if (math_param_b) {
						math_result = (((int16_t)math_param_a + ((uint32_t)math_param_b << 16)) / math_param_b) & 0xffff;
						math_result |= ((uint16_t)(((int16_t)math_param_a + ((uint32_t)math_param_b << 16)) % math_param_b) & 0xffff) << 16;
					} else {
						math_result = 0;
					}
					math_param_a = math_param_b = 0;
					break;
				case 2:
					math_result = math_result + (int16_t)math_param_a * (int16_t)math_param_b;
					math_overflow = (math_result & math_overflow_mask) != 0;
					math_result &= math_40bit_mask;
					math_param_b = 0;
					break;
				case 3:
					/* reserved mode */
					break;
			}
			//bprintf(0, _T("math mode %x:  res: %I64x\n"), math_mode, math_result);
			break;

		case 0x2258: // variable bit-length bus read
			vari_width = ((data & 0x0f) == 0) ? 0x10 : data & 0x0f;
			vari_clock = data & 0x80;

			if (vari_clock == 0) {
				vari_src += (vari_bitcount += vari_width) >> 3;
				vari_bitcount &= 0x07;
			}
			break;
		case 0x2259:
		case 0x225a:
		case 0x225b:
			vari_bitcount = 0;
			set_byte(vari_src, data, address - 0x2259);
			break;
	}
}

static uint8_t snes_regs_read(uint32_t address)
{
	address &= 0xffff;
//	if ((address & 0x2f00) == 0x2300)
//		bprintf(0, _T("snes_regs_read(%x)\n"), address);
	switch (address) {
		case 0x2300: return (scpu_control & 0x0f) | (scpu_irq_pending & 0xf0);
		case 0x2301: return (sa1_control & 0x0f) | (sa1_irq_pending & 0xf0); // is this valid from snes side?  kirby superstar seems to think so
	}

//	bprintf(0, _T("snes_regs_read(%x)\n"), address);

	return snes_ctx->openBus;
}

static void snes_regs_write(uint32_t address, uint8_t data)
{
	address &= 0xffff;
//	if ((address & 0x2f00) == 0x2200)
//		bprintf(0, _T("snes_regs_write(%x)  %x\n"), address, data);
	switch (address) {
		case 0x2200:
//			bprintf(0, _T("sa1_control  %x\n"), data);
			if (~data & 0x20 && sa1_control & 0x20) {
				cpusa1_reset(true);
			}

			cpusa1_setHalt(data & 0x40);

			sa1_control = data;

			if (data & sa1_IRQ) sa1_set_irq(sa1_IRQ, 1);
			if (data & sa1_IRQ_NMI) sa1_set_irq(sa1_IRQ_NMI, 1);
			check_pending_irqs();
			break;

		case 0x2201:
			scpu_irq_enable = data;
			check_pending_irqs();
			break;
		case 0x2202:
			if (data & sa1_IRQ) scpu_set_irq(sa1_IRQ, 0);
			if (data & sa1_IRQ_DMA) scpu_set_irq(sa1_IRQ_DMA, 0);
			check_pending_irqs();
			break;

		case 0x2203:
		case 0x2204:
			set_byte(sa1_reset_vector, data, ~address & 1);
			break;
		case 0x2205:
		case 0x2206:
			set_byte(sa1_nmi_vector, data, ~address & 1);
			break;
		case 0x2207:
		case 0x2208:
			set_byte(sa1_irq_vector, data, ~address & 1);
			break;

		case 0x2220:
		case 0x2221:
		case 0x2222:
		case 0x2223:
			if (data != supermmc[address & 3]) {
				supermmc[address & 3] = data;
				//bprintf(0, _T("MAP[%x] = %x %S\n"), address & 3, data & 7, (data & 0x80) ? "lowbank: use reg" : "lowbank: fixed");
				map_update();
			}
			break;

		case 0x2224:
			bwram_snes_bank = (data & 0x1f) * 0x2000;
			break;
		case 0x2226: // bwram write enable
		case 0x2228: // bwram write protect
			break;

		case 0x2229: // iram write protect (?)
			//bprintf(0, _T("iramwp %x  %x\n"), address, data);
			break;

		case 0x2231: // dma
		case 0x2232:
		case 0x2233:
		case 0x2234:
		case 0x2235:
		case 0x2236:
		case 0x2237:
			dma_regs(address, data);
			break;
	}
}

static void acc_cyc_rom_conflict(uint32_t address) {
	if (sa1_snes_rom_conflict()) {
		acc_cyc();
	}
}

static void acc_cyc_iram_conflict() {
	if ( (snes_ctx->adrBus & 0x40f800) == 0x003000) {
		if (snes_ctx->inRefresh == false ) acc_cyc();
		if (snes_ctx->inRefresh == false ) acc_cyc();
	}
}

static void acc_cyc_bwram_conflict() {
	if ( ((snes_ctx->adrBus & 0x40e000) == 0x006000)
		|| ((snes_ctx->adrBus & 0xf00000) == 0x400000) ) {
		acc_cyc();
		acc_cyc();
	}
}

static inline uint8_t iram_read(uint32_t address)
{
	if (address & 0x800) return 0xff;
	return iram[address & 0x7ff];
}

static inline void iram_write(uint32_t address, uint8_t data)
{
	if (address & 0x800) return;
	iram[address & 0x7ff] = data;
}

static inline uint8_t iram_read_sa1(uint32_t address) {
	acc_cyc_iram_conflict();
	return iram_read(address);
}

static inline void iram_write_sa1(uint32_t address, uint8_t data) {
	acc_cyc_iram_conflict();
	iram_write(address, data);
}

static uint8_t read_charconv1_dma(uint32_t address)
{
    const uint32_t mask = (dma_charconv_bpp << 3) - 1;

    if ((address & mask) == 0) {
		const uint32_t ccsize = (dma_charconv_control >> 2) & 7;
		const uint32_t tile_pitch = (8 << (ccsize - (dma_charconv_control & 3)));
		const uint32_t tile_start = dma_src & sram_mask;
        const uint32_t tile_index = (address - tile_start) / (dma_charconv_bpp << 3);
		uint32_t bwram_adr = tile_start + ((tile_index >> ccsize) * 8 * tile_pitch)
			+ ((tile_index & ((1 << ccsize) - 1)) * dma_charconv_bpp);

		//bprintf(0, _T("tile index %x  addr %x  dma_src %x  bwram_adr %x\n"), tile_index, address, dma_src, bwram_adr);

		for (uint32_t y = 0; y < 8; y++, bwram_adr += tile_pitch) {
			uint64_t planar = 0;
			uint8_t linear[8] = { 0, };
			for (uint32_t bit = 0; bit < dma_charconv_bpp; bit++) {
				planar |= (uint64_t)sram[(bwram_adr + bit) & sram_mask] << (bit << 3);
			}

			for (uint8_t x = 0; x < 8; x++, planar >>= dma_charconv_bpp) {
				for (uint8_t bit = 0; bit < dma_charconv_bpp; bit++) {
					linear[bit] |= ((planar >> bit) & 1) << (x ^ 7);
				}
			}

			for (uint8_t byte = 0; byte < dma_charconv_bpp; byte++) {
				iram_write(dma_dst + (y << 1) + ((byte & 1) | ((byte & 6) << 3)), linear[byte]);
			}
		}
	}

	return iram_read(dma_dst + (address & mask));
}

static inline uint8_t bwram_read(uint32_t address)
{
	return sram[address & sram_mask];
}

static inline uint8_t bwram_read_scpu(uint32_t address)
{
	if (dma_charconv1_active) {
		return read_charconv1_dma(address & 0xfffff);
	}

	return sram[address & sram_mask];
}

static inline uint8_t bwram_read_bitmap(uint32_t address)
{
	if (bwram_sa1_type == 0x80) {
		return (sram[(address >> 2) & sram_mask] >> ((address & 0x03) << 1)) & 0x03;
	} else {
		return (sram[(address >> 1) & sram_mask] >> ((address & 0x01) << 2)) & 0x0f;
	}
}

static inline uint8_t bwram_read_bank(uint32_t address)
{
	return bwram_read(bwram_snes_bank + (address & 0x1fff));
}

#if 0
static inline uint8_t bwram_read_bank_scpu(uint32_t address)
{
	address = bwram_snes_bank + (address & 0x1fff);

	if (dma_charconv1_active) {
		//bprintf(0, _T("."));
		return read_charconv1_dma(address);
	}

	return bwram_read(address);
}
#endif

static inline uint8_t bwram_read_sa1(uint32_t address)
{
	acc_cyc();
	acc_cyc_bwram_conflict();
	return bwram_read(address);
}

static inline uint8_t bwram_read_bitmap_sa1(uint32_t address)
{
	acc_cyc();
	acc_cyc_bwram_conflict();
	return bwram_read_bitmap(address);
}

static inline uint8_t bwram_read_bank_sa1(uint32_t address)
{
    acc_cyc();
	acc_cyc_bwram_conflict();
	return bwram_read(bwram_sa1_bank + (address & 0x1fff));
}

static inline void bwram_write(uint32_t address, uint8_t data)
{
	sram[address & sram_mask] = data;
}

static inline void bwram_write_bitmap(uint32_t address, uint8_t data)
{
	const bool is_2bpp = (bwram_sa1_type == 0x80);
	const uint8_t shift = (is_2bpp) ? ((address & 3) << 1) : ((address & 1) << 2);
	uint8_t mask = (is_2bpp) ? 0x03 : 0x0f;

	data = (data & mask) << shift;
	mask <<= shift;
	address >>= (is_2bpp) ? 2 : 1;

	sram[address & sram_mask] = (sram[address & sram_mask] & ~mask) | data;
}

static inline void bwram_write_bank(uint32_t address, uint8_t data)
{
	bwram_write(bwram_snes_bank + (address & 0x1fff), data);
}

static inline void bwram_write_sa1(uint32_t address, uint8_t data)
{
	acc_cyc();
	acc_cyc_bwram_conflict();
	bwram_write(address, data);
}

static inline void bwram_write_bitmap_sa1(uint32_t address, uint8_t data)
{
	acc_cyc();
	acc_cyc_bwram_conflict();
	bwram_write_bitmap(address & 0xfffff, data);
}

static inline void bwram_write_bank_sa1(uint32_t address, uint8_t data)
{
	address = bwram_sa1_bank + (address & 0x1fff);
    acc_cyc();
	acc_cyc_bwram_conflict();
	if (bwram_sa1_mode & 0x80) {
		bwram_write_bitmap(address, data);
	} else {
		bwram_write(address, data);
	}
}

static inline uint8_t rom_read(uint32_t address)
{
	return rom_map[address >> 12][address & 0xfff];
}

static inline uint8_t rom_read_sa1(uint32_t address)
{
	acc_cyc_rom_conflict(address);
	return rom_map[address >> 12][address & 0xfff];
}

static inline uint8_t rom_read_low_sa1(uint32_t address)
{
	acc_cyc_rom_conflict(address);

	if (cpusa1_isVector()) {
		// it's absolutely necessary to detect if the cpu core is reading from
		// the vectors here.  Some games rely on reading from rom @ these addresses.
		switch (address) {
			case 0xffee:
				return sa1_irq_vector & 0xff;
			case 0xffef:
				return (sa1_irq_vector >> 8) & 0xff;
			case 0xffea:
				return sa1_nmi_vector & 0xff;
			case 0xffeb:
				return (sa1_nmi_vector >> 8) & 0xff;
			case 0xfffc:
				return sa1_reset_vector & 0xff;
			case 0xfffd:
				return (sa1_reset_vector >> 8) & 0xff;
		}
	}
	return rom_map[address >> 12][address & 0xfff];
}

static inline uint8_t snes_rom_vect_read(uint32_t address)
{
	if (cpu_isVector()) {
		switch (address) {
			case 0xffea: if (scpu_control & 0x10) return scpu_nmi_vector & 0xff; break;
			case 0xffeb: if (scpu_control & 0x10) return (scpu_nmi_vector >> 8) & 0xff; break;
			case 0xffee: if (scpu_control & 0x40) return scpu_irq_vector & 0xff; break;
			case 0xffef: if (scpu_control & 0x40) return (scpu_irq_vector >> 8) & 0xff; break;
		}
	}

	return rom_map[address >> 12][address & 0xfff];
}

static void map_handlers_scpu() {
	map_rw(SCPU, 0x00, 0x3f, 0x2000, 0x2fff, snes_regs_read, snes_regs_write);
	map_rw(SCPU, 0x80, 0xbf, 0x2000, 0x2fff, snes_regs_read, snes_regs_write);

	map_rw(SCPU, 0x00, 0x3f, 0x3000, 0x3fff, iram_read, iram_write);
	map_rw(SCPU, 0x80, 0xbf, 0x3000, 0x3fff, iram_read, iram_write);

	map_rw(SCPU, 0x00, 0x3f, 0x6000, 0x7fff, bwram_read_bank, bwram_write_bank);
	map_rw(SCPU, 0x80, 0xbf, 0x6000, 0x7fff, bwram_read_bank, bwram_write_bank);

	map_rw(SCPU, 0x00, 0x3f, 0x8000, 0xffff, snes_rom_vect_read, NULL);
	map_rw(SCPU, 0x80, 0xbf, 0x8000, 0xffff, snes_rom_vect_read, NULL);
	map_rw(SCPU, 0xc0, 0xff, 0x0000, 0xffff, rom_read, NULL);

	map_rw(SCPU, 0x40, 0x4f, 0x0000, 0xffff, bwram_read_scpu, bwram_write);
}

// snes-cart bus
uint8_t snes_sa1_cart_read(uint32_t address)
{
	snes_sa1_run();

	return scpu_rom_map_read[map_get_handler(address)](address);
}

void snes_sa1_cart_write(uint32_t address, uint8_t data)
{
	snes_sa1_run();

	scpu_rom_map_write[map_get_handler(address)](address, data);
}

// sa1 bus map & callbacks for cpu_sa1
static void map_handlers_sa1() {
	map_rw(SA1, 0x00, 0x3f, 0x0000, 0x0fff, iram_read_sa1, iram_write_sa1);
	map_rw(SA1, 0x80, 0xbf, 0x0000, 0x0fff, iram_read_sa1, iram_write_sa1);

	map_rw(SA1, 0x00, 0x3f, 0x2000, 0x2fff, sa1_regs_read, sa1_regs_write);
	map_rw(SA1, 0x80, 0xbf, 0x2000, 0x2fff, sa1_regs_read, sa1_regs_write);

	map_rw(SA1, 0x00, 0x3f, 0x3000, 0x3fff, iram_read_sa1, iram_write_sa1);
	map_rw(SA1, 0x80, 0xbf, 0x3000, 0x3fff, iram_read_sa1, iram_write_sa1);

	map_rw(SA1, 0x00, 0x3f, 0x6000, 0x7fff, bwram_read_bank_sa1, bwram_write_bank_sa1);
	map_rw(SA1, 0x80, 0xbf, 0x6000, 0x7fff, bwram_read_bank_sa1, bwram_write_bank_sa1);

	map_rw(SA1, 0x00, 0x3f, 0x8000, 0xffff, rom_read_low_sa1, NULL);
	map_rw(SA1, 0x80, 0xbf, 0x8000, 0xffff, rom_read_low_sa1, NULL);
	map_rw(SA1, 0xc0, 0xff, 0x0000, 0xffff, rom_read_sa1, NULL);

	map_rw(SA1, 0x40, 0x4f, 0x0000, 0xffff, bwram_read_sa1, bwram_write_sa1);
	map_rw(SA1, 0x60, 0x6f, 0x0000, 0xffff, bwram_read_bitmap_sa1, bwram_write_bitmap_sa1);
}

void sa1_cpuIdle() {
  acc_cyc();
}

uint8_t sa1_cpuRead(uint32_t adr) {
  acc_cyc();

  return openbus = sa1_bus_read(adr); //sa1_rom_map_read[map_get_handler(adr)](adr);
}

void sa1_cpuWrite(uint32_t adr, uint8_t val) {
  acc_cyc();
  openbus = val;

  sa1_rom_map_write[map_get_handler(adr)](adr, val);
}
