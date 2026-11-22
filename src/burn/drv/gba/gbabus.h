#pragma once

#ifndef GBA_BUS_H
#define GBA_BUS_H

#include "gba.h"
// openbus_word/mmio_word hold the guest word as a host value (LSB = lowest guest
// address), so these extraction shifts do not depend on host endianness.
#define GUEST_HALF_SHIFT(a) ((a) & 2 ? 16 : 0)
#define GUEST_BYTE_SHIFT(k) ((k) * 8)

// Shared ROM read fast path (bank 0x08-0x0d); on a hit *out_word is the host-endian word.
// check_gpio must be true when the caller has not already filtered GPIO addresses.
SB_ALWAYS_INLINE bool gba_rom_fast_path_word(gba_t* gba, UINT32 address, bool check_gpio, UINT32* out_word)
{
	if (SB_UNLIKELY((UINT32)((address >> 24) - 0x08) > 5))
		return false;
	UINT32 maddr = address & 0x1fffffc;
	// EEPROM readiness is only signalled in the top ROM bank (0x0d).
	bool eeprom_ready_window = ((address >> 24) == 0x0d)
		&& (gba->cart.backup_type == GBA_BACKUP_EEPROM)
		&& ((address & 0x1ffffff) >= 0x01ffff00);
	// The matrix mapper only remaps the 8 KB window at 0x08000000-0x08001fff.
	bool matrix_window_active = gba->cart.matrix.active
		&& ((address & 0xffffe000) == 0x08000000);
	if (SB_LIKELY(!matrix_window_active
			&& !eeprom_ready_window
			&& maddr < gba->cart.rom_size
			&& (!check_gpio || !gba_gpio_address(gba, address)))) {
		*out_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.cart_rom + maddr));
		return true;
	}
	return false;
}

// Simple-RAM fast path: EWRAM/IWRAM/Palette/OAM are plain arrays with no read side effects.
// VRAM is excluded (mirror/bitmap-mode blocking is nontrivial).
SB_ALWAYS_INLINE bool gba_simple_ram_fast_path_word(gba_t* gba, UINT32 address, UINT32* out_word)
{
	UINT32 bank = address >> 24;
	switch (bank) {
		case 0x02: *out_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.wram0 + (address & 0x3fffc))); return true;
		case 0x03: *out_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.wram1 + (address & 0x7ffc)));  return true;
		case 0x05: *out_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.palette + (address & 0x3fc)));  return true;
		case 0x07: *out_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.oam + (address & 0x3fc)));      return true;
		default:   return false;
	}
}

// 8-bit read fast path for the same plain RAM banks; *out_byte receives the
// host-order byte and *out_word the containing word.
SB_ALWAYS_INLINE bool gba_simple_ram_fast_path_byte(gba_t* gba, UINT32 address, UINT8* out_byte, UINT32* out_word)
{
	UINT32 bank = address >> 24;
	UINT32 a32  = address & ~3u;
	switch (bank) {
		case 0x02: *out_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.wram0   + (a32 & 0x3fffc))); break;
		case 0x03: *out_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.wram1   + (a32 & 0x7ffc)));  break;
		case 0x05: *out_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.palette + (a32 & 0x3fc)));   break;
		case 0x07: *out_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.oam     + (a32 & 0x3fc)));   break;
		default:   return false;
	}
	*out_byte = (UINT8)(*out_word >> GUEST_BYTE_SHIFT(address & 3));
	return true;
}

// 32-bit write fast path for plain RAM below 0x05000000.
SB_ALWAYS_INLINE bool gba_simple_ram_store32(gba_t* gba, UINT32 address, UINT32 data)
{
	UINT32 bank = address >> 24;
	switch (bank) {
		case 0x02: *(UINT32*)(gba->mem.wram0 + (address & 0x3fffc)) = BURN_ENDIAN_SWAP_INT32(data); return true;
		case 0x03: *(UINT32*)(gba->mem.wram1 + (address & 0x7ffc))  = BURN_ENDIAN_SWAP_INT32(data); return true;
		default:   return false;
	}
}

// Defined with the PPU worker later in this translation unit; counts OAM and
// palette writes for the per-line scanline snapshot.
static inline void gba_ppu_worker_note_mem_write(gba_t* gba, UINT32 bank);

// 16-bit write fast path for plain RAM; palette/OAM writes notify the PPU
// worker dirty tracker.
SB_ALWAYS_INLINE bool gba_simple_ram_store16(gba_t* gba, UINT32 address, UINT16 data)
{
	UINT32 bank = address >> 24;
	switch (bank) {
		case 0x02: *(UINT16*)(gba->mem.wram0 + (address & 0x3fffe)) = BURN_ENDIAN_SWAP_INT16(data); return true;
		case 0x03: *(UINT16*)(gba->mem.wram1 + (address & 0x7ffe))  = BURN_ENDIAN_SWAP_INT16(data); return true;
		case 0x05: *(UINT16*)(gba->mem.palette + (address & 0x3fe)) = BURN_ENDIAN_SWAP_INT16(data);
		           gba_ppu_worker_note_mem_write(gba, 0x05u); return true;
		case 0x07: *(UINT16*)(gba->mem.oam     + (address & 0x3fe)) = BURN_ENDIAN_SWAP_INT16(data);
		           gba_ppu_worker_note_mem_write(gba, 0x07u); return true;
		default:   return false;
	}
}

// 8-bit write fast path; only EWRAM/IWRAM accept 8-bit writes.
SB_ALWAYS_INLINE bool gba_simple_ram_store8(gba_t* gba, UINT32 address, UINT8 data)
{
	UINT32 bank = address >> 24;
	switch (bank) {
		case 0x02: *(gba->mem.wram0 + (address & 0x3ffff)) = data; return true;
		case 0x03: *(gba->mem.wram1 + (address & 0x7fff))  = data; return true;
		default:   return false;
	}
}

static inline UINT32 gba_read32(gba_t* gba, UINT32 baddr)
{
	if (gba_gpio_address(gba, baddr)) {
		UINT32 address = baddr & ~3;
		// A 32-bit GPIO read spans two halfwords; only the aligned 0xC4 window pairs
		// two GPIO registers, other windows take the upper halfword from ROM.
		UINT32 low     = gba_gpio_read16(gba, address);
		if ((baddr & ~3) == 0x080000c4)
			return low | ((UINT32)gba_gpio_read16(gba, address + 2) << 16);
		UINT32 rom     = gba_rom_read16(gba, address + 2);
		return low | (rom << 16);
	}
	// ROM/simple-RAM fast path.
	UINT32 word;
	if (SB_LIKELY(gba_rom_fast_path_word(gba, baddr, false, &word)
			|| gba_simple_ram_fast_path_word(gba, baddr, &word))) {
		gba->mem.openbus_word = word;
		return word;
	}
	UINT32* val = gba_dword_lookup(gba, baddr, GBA_REQ_READ | GBA_REQ_4B);
	return *val;
}

static inline UINT16 gba_read16(gba_t* gba, UINT32 baddr)
{
	if (gba_gpio_address(gba, baddr))
		return gba_gpio_read16(gba, baddr);
	// ROM/simple-RAM fast path.
	UINT32 word;
	bool is_rom = gba_rom_fast_path_word(gba, baddr, false, &word);
	if (SB_LIKELY(is_rom || gba_simple_ram_fast_path_word(gba, baddr, &word))) {
		UINT16 res16 = (UINT16)(word >> GUEST_HALF_SHIFT(baddr));
		gba->mem.openbus_word = is_rom ? ((UINT32)res16 * 0x00010001u) : word;
		return res16;
	}
	UINT32* val    = gba_dword_lookup(gba, baddr, GBA_REQ_READ | GBA_REQ_2B);
	return (UINT16)(*val >> GUEST_HALF_SHIFT(baddr));
}

static inline UINT8 gba_read8(gba_t* gba, UINT32 baddr)
{
	if (gba_gpio_address(gba, baddr))
		return (gba_gpio_read16(gba, baddr) >> ((baddr & 1) * 8)) & 0xff;
	// ROM fast path (openbus mirrors the containing halfword), then simple-RAM byte fast path.
	UINT32 word;
	if (SB_LIKELY(gba_rom_fast_path_word(gba, baddr, false, &word))) {
		UINT16 res16 = (UINT16)(word >> GUEST_HALF_SHIFT(baddr));
		gba->mem.openbus_word = (UINT32)res16 * 0x00010001u;
		return (UINT8)(res16 >> GUEST_BYTE_SHIFT(baddr & 1));
	}
	UINT8 res8;
	if (SB_LIKELY(gba_simple_ram_fast_path_byte(gba, baddr, &res8, &word))) {
		gba->mem.openbus_word = word;
		return res8;
	}
	UINT32* val    = gba_dword_lookup(gba, baddr, GBA_REQ_READ | GBA_REQ_1B);
	return (UINT8)(*val >> GUEST_BYTE_SHIFT(baddr & 3));
}


static inline void gba_store32(gba_t* gba, UINT32 baddr, UINT32 data)
{
	if (baddr >= 0x08000000) {
		// Mask is 0xfe to catch the sram mirror at 0x0f and 0x0e
		if ((baddr & 0xfe000000) == 0xe000000) {
			gba_process_backup_write(gba, baddr, (data >> GUEST_BYTE_SHIFT(baddr & 3)) & 0xFF);
			return;
		}
		if (gba->cart.matrix.active && (baddr & 0x01ffff00) == 0x00800100) {
			gba_matrix_write(gba, baddr & 0x3c, data);
			return;
		}
		if (gba_gpio_address(gba, baddr)) {
			// Game Pak GPIO registers are 16-bit; 32-bit ROM stores do not access them.
			return;
		}
	}
	if (SB_LIKELY(gba_simple_ram_store32(gba, baddr, data)))
		return;
	UINT32* val = gba_dword_lookup(gba, baddr, GBA_REQ_WRITE | GBA_REQ_4B);
	*val = BURN_ENDIAN_SWAP_INT32(data);
}

static inline void gba_store16(gba_t* gba, UINT32 baddr, UINT32 data)
{
	if (baddr >= 0x08000000) {
		// Mask is 0xfe to catch the sram mirror at 0x0f and 0x0e
		if ((baddr & 0xfe000000) == 0xe000000) {
			gba_process_backup_write(gba, baddr, data >> ((baddr & 1) * 8));
			return;
		}
		if (gba_gpio_address(gba, baddr)) {
			gba_gpio_write16(gba, baddr & ~1, data);
			return;
		}
		// Detected EEPROM savegame
		if (gba->cart.backup_type == GBA_BACKUP_NONE && (baddr & 0xff000000) == 0x0d000000)
			gba->cart.backup_type = GBA_BACKUP_EEPROM;
		if (gba->cart.matrix.active && (baddr & 0x01ffff00) == 0x00800100) {
			gba_matrix_write16(gba, baddr & 0x3c, (UINT16)data);
			return;
		}
	}
	if (SB_LIKELY(gba_simple_ram_store16(gba, baddr, (UINT16)data)))
		return;
	UINT8* val = (UINT8*)gba_dword_lookup(gba, baddr, GBA_REQ_WRITE | GBA_REQ_2B);
	val[baddr & 2]       = (UINT8)(data & 0xff);
	val[(baddr & 2) + 1] = (UINT8)((data >> 8) & 0xff);
}

// Defined below; needed here to pick the VRAM object region bound.
static inline UINT16 gba_io_read16(gba_t* gba, UINT32 baddr);

static inline void gba_store8(gba_t* gba, UINT32 baddr, UINT32 data)
{
	if (baddr >= 0x05000000) {
		// 8 bit stores to palette mirror across 8 bit halves
		if ((baddr & 0xff000000) == 0x5000000) {
			gba_store16(gba, baddr & ~1, (data & 0xff) * 0x0101);
			return;
		}
		if ((baddr & 0xff000000) == 0x06000000) {
			// 8-bit stores are ignored inside the object region; its start follows the
			// display mode (0x10000 for text modes, 0x14000 for bitmap modes).
			UINT32 object_start = (SB_BFE(gba_io_read16(gba, GBA_DISPCNT), 0, 3) >= 3) ? 0x14000u : 0x10000u;
			if ((baddr & 0x1ffff) < object_start) {
				gba_store16(gba, baddr & ~1, (data & 0xff) * 0x0101);
			}
			return;
		}
		// Mask is 0xfe to catch the sram mirror at 0x0f and 0x0e
		if ((baddr & 0xfe000000) == 0xe000000) {
			gba_process_backup_write(gba, baddr, data);
			return;
		}
		// Remaining 8 bit ops are not supported on VRAM or ROM
		return;
	}
	if (SB_LIKELY(gba_simple_ram_store8(gba, baddr, (UINT8)data)))
		return;
	// The IO register file is 1 KB; 8-bit stores above it are unmapped and are
	// dropped instead of aliasing openbus_word.
	if ((baddr & 0xff000000) == 0x04000000 && SB_UNLIKELY(baddr > 0x040003ff))
		return;
	((UINT8*)gba_dword_lookup(gba, baddr, GBA_REQ_WRITE | GBA_REQ_1B))[baddr & 3] = (UINT8)data;
}


// The IO register file is 1 KB in io[1024]; these index masks keep each access
// inside the array and keep the 16/32-bit accesses naturally aligned.
static inline void gba_io_store8(gba_t* gba, UINT32 baddr, UINT8  data)
{
	gba->mem.io[baddr & 0x3ff] = data;
}

static inline void gba_io_store16(gba_t* gba, UINT32 baddr, UINT16 data)
{
	*(UINT16*)(gba->mem.io + (baddr & 0x3fe)) = BURN_ENDIAN_SWAP_INT16(data);
}

static inline void gba_io_store32(gba_t* gba, UINT32 baddr, UINT32 data)
{
	*(UINT32*)(gba->mem.io + (baddr & 0x3fc)) = BURN_ENDIAN_SWAP_INT32(data);
}

static inline UINT8  gba_io_read8(gba_t* gba, UINT32 baddr)
{
	return gba->mem.io[baddr & 0x3ff];
}

static inline UINT16 gba_io_read16(gba_t* gba, UINT32 baddr)
{
	return BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.io + (baddr & 0x3fe)));
}

static inline UINT32 gba_io_read32(gba_t* gba, UINT32 baddr)
{
	return BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.io + (baddr & 0x3fc)));
}

// refreshes the cached interrupt poll flag used once per instruction
static inline void gba_update_interrupt_pending(gba_t* gba)
{
	UINT16 ie = gba_io_read16(gba, GBA_IE);
	UINT16 if_value = gba_io_read16(gba, GBA_IF);
	bool flagged = (ie & if_value) != 0;
	gba->interrupt_flagged = flagged;
	gba->interrupt_pending = flagged && (gba_io_read32(gba, GBA_IME) & 1);
}

static inline void gba_recompute_waitstate_table(gba_t* gba, UINT16 waitcnt)
{
	const INT32 wait_state_table[16 * 4] = {
		1, 1, 1, 1,		// 0x00 (bios)
		1, 1, 1, 1,		// 0x01 (bios)
		3, 3, 6, 6,		// 0x02 (256k WRAM)
		1, 1, 1, 1,		// 0x03 (32k WRAM)
		1, 1, 1, 1,		// 0x04 (IO)
		1, 1, 2, 2,		// 0x05 (BG/OBJ Palette)
		1, 1, 2, 2,		// 0x06 (VRAM)
		1, 1, 1, 1,		// 0x07 (OAM)
		4, 4, 8, 8,		// 0x08 (GAMEPAK ROM 0)
		4, 4, 8, 8,		// 0x09 (GAMEPAK ROM 0)
		4, 4, 8, 8,		// 0x0A (GAMEPAK ROM 1)
		4, 4, 8, 8,		// 0x0B (GAMEPAK ROM 1)
		4, 4, 8, 8,		// 0x0C (GAMEPAK ROM 2)
		4, 4, 8, 8,		// 0x0D (GAMEPAK ROM 2)
		4, 4, 4, 4,		// 0x0E (GAMEPAK SRAM)
		1, 1, 1, 1,		// 0x0F (unused)
	};
	for (INT32 i = 0;i < 16 * 4;++i) {
		gba->mem.wait_state_table[i] = wait_state_table[i];
	}
	UINT8 sram_wait = SB_BFE(waitcnt, 0, 2);
	UINT8 wait_first[ 3];
	UINT8 wait_second[3];

	wait_first[ 0]    = SB_BFE(waitcnt,  2, 2);
	wait_second[0]    = SB_BFE(waitcnt,  4, 1);
	wait_first[ 1]    = SB_BFE(waitcnt,  5, 2);
	wait_second[1]    = SB_BFE(waitcnt,  7, 1);
	wait_first[ 2]    = SB_BFE(waitcnt,  8, 2);
	wait_second[2]    = SB_BFE(waitcnt, 10, 1);
	UINT8 prefetch_en = SB_BFE(waitcnt, 14, 1);

	INT32 primary_table[4] = { 4,3,2,8 };

	// Each waitstate area covers two bank entries; each bank holds the 16/32-bit
	// sequential and non-sequential access times.
	for (INT32 ws = 0;ws < 3;++ws) {
		for (INT32 i = 0;i < 2;++i) {
			UINT8 w_first  = primary_table[wait_first[ws]];
			UINT8 w_second = wait_second[ws] ? 1 : 2;
			if (ws == 1)
				w_second = wait_second[ws] ? 1 : 4;
			if (ws == 2)
				w_second = wait_second[ws] ? 1 : 8;
			w_first += 1;w_second += 1;
			// Fill wait tables for this ws (sequential/non-sequential, 16-bit/32-bit)
			INT32 wait16b = w_second;
			INT32 wait32b = w_second * 2;

			INT32 wait16b_nonseq = w_first;
			INT32 wait32b_nonseq = w_first + w_second;

			gba->mem.wait_state_table[(0x08 + i + ws * 2) * 4 + 0] = wait16b;
			gba->mem.wait_state_table[(0x08 + i + ws * 2) * 4 + 1] = wait16b_nonseq;
			gba->mem.wait_state_table[(0x08 + i + ws * 2) * 4 + 2] = wait32b;
			gba->mem.wait_state_table[(0x08 + i + ws * 2) * 4 + 3] = wait32b_nonseq;
		}
	}
	gba->mem.prefetch_en   = prefetch_en;
	gba->mem.prefetch_size = 0;

	// SRAM
	gba->mem.wait_state_table[(0x0e * 4) + 0] = 1 + primary_table[sram_wait];
	gba->mem.wait_state_table[(0x0e * 4) + 1] = 1 + primary_table[sram_wait];
	gba->mem.wait_state_table[(0x0e * 4) + 2] = 1 + primary_table[sram_wait];
	gba->mem.wait_state_table[(0x0e * 4) + 3] = 1 + primary_table[sram_wait];
	waitcnt &= 0x5FFF;	// Force cartridge to report as GBA cart
	gba_io_store16(gba, GBA_WAITCNT, waitcnt);
}

SB_ALWAYS_INLINE void gba_compute_access_cycles(gba_t* gba, UINT32 address, INT32 request_size /*0/1: 16-bit seq/nonseq, 2/3: 32-bit seq/nonseq*/)
{
	INT32 bank        = SB_BFE(address, 24, 4);
	bool  prefetch_en = gba->mem.prefetch_en;
	if (SB_UNLIKELY(!prefetch_en)) {
		if (gba->cpu.i_cycles)
			request_size |= 1;
		if (request_size & 1)
			gba->cpu.next_fetch_sequential = false;
		gba->mem.prefetch_size = 0;
	}
	UINT32 wait = gba->mem.wait_state_table[bank * 4 + request_size];
	if (SB_LIKELY(prefetch_en)) {
		gba->mem.prefetch_size += gba->cpu.i_cycles;
		if (bank >= 0x08 && bank <= 0x0d) {
			if (SB_UNLIKELY(request_size & 1)) {
				UINT32 pc = gba->cpu.prefetch_pc;
				if (pc >= 0x08000000) {
					INT32 pc_bank         = SB_BFE(pc, 24, 4);
					INT32 prefetch_cycles = gba->mem.wait_state_table[pc_bank * 4];
					INT32 prefetch_phase  = (gba->mem.prefetch_size) % prefetch_cycles;
					if (gba->mem.prefetch_size >
						gba->cpu.i_cycles && prefetch_phase == prefetch_cycles - 1)wait += 1;
				}
				// Non-sequential access resets the prefetch buffer
				gba->mem.prefetch_size         = 0;
				gba->cpu.next_fetch_sequential = false;
			} else {
				// Sequential fetch from prefetch buffer based on available wait states
				if (gba->mem.prefetch_size >= wait) {
					gba->mem.prefetch_size -= wait - 1;
					wait = 1;
				} else {
					wait -= gba->mem.prefetch_size;
					gba->mem.prefetch_size = 0;
				}
			}
		} else gba->mem.prefetch_size += wait;
	}
	gba->mem.requests += wait;
}

static inline UINT32 gba_compute_access_cycles_dma(gba_t* gba, UINT32 address, INT32 request_size/*0/1: 16-bit seq/nonseq, 2/3: 32-bit seq/nonseq*/)
{
	INT32  bank = SB_BFE(address, 24, 4);
	UINT32 wait = gba->mem.wait_state_table[bank * 4 + request_size];
	return wait;
}

// Memory IO functions for the emulated CPU
static inline UINT32 arm7_read32(void* user_data, UINT32 address)
{
	gba_compute_access_cycles((gba_t*)user_data, address, 3);
	UINT32 value = gba_read32((gba_t*)user_data, address);
	return value;
}

static inline UINT32 arm7_read16(void* user_data, UINT32 address)
{
	gba_compute_access_cycles((gba_t*)user_data, address, 1);
	UINT16 value = gba_read16((gba_t*)user_data, address);
	return value;
}

SB_ALWAYS_INLINE UINT32 arm7_read32_seq(void* user_data, UINT32 address, bool seq)
{
	// ROM/simple-RAM read fast path; covers sequential fetches, branch
	// targets and LDM data loads.
	gba_t* gba = (gba_t*)user_data;
	UINT32 word;
	if (SB_LIKELY(   gba_rom_fast_path_word(gba, address, true, &word)
			|| gba_simple_ram_fast_path_word(gba, address, &word))) {
		gba_compute_access_cycles(gba, address, seq ? 2 : 3);
		gba->mem.openbus_word = word;
		return gba->mem.openbus_word;
	}
	gba_compute_access_cycles(gba, address, seq ? 2 : 3);
	return gba_read32(gba, address);
}

SB_ALWAYS_INLINE UINT32 arm7_read16_seq(void* user_data, UINT32 address, bool seq)
{
	// ROM/simple-RAM read fast path; see arm7_read32_seq.
	gba_t* gba = (gba_t*)user_data;
	UINT32 word;
	bool is_rom = gba_rom_fast_path_word(gba, address, true, &word);
	if (SB_LIKELY(is_rom || gba_simple_ram_fast_path_word(gba, address, &word))) {
		gba_compute_access_cycles(gba, address, seq ? 0 : 1);
		UINT16 res16 = (UINT16)(word >> GUEST_HALF_SHIFT(address));
		// ROM mirrors the halfword on the bus; RAM leaves the whole word
		gba->mem.openbus_word = is_rom ? ((UINT32)res16 * 0x10001u) : word;
		return (UINT32)res16;
	}
	gba_compute_access_cycles(gba, address, seq ? 0 : 1);
	return gba_read16(gba, address);
}

//Used to process special behavior triggered by MMIO write
static inline bool gba_process_mmio_write(gba_t* gba, UINT32 address, UINT32 data, INT32 req_size_bytes);

static inline UINT8 arm7_read8(void* user_data, UINT32 address)
{
	gba_compute_access_cycles((gba_t*)user_data, address, 1);
	return gba_read8((gba_t*)user_data, address);
}

static inline void gba_dma_write32(gba_t* gba, UINT32 address, UINT32 data)
{
	if ((address & 0xfffffc00) == 0x04000000) {
		if (gba_process_mmio_write(gba, address, data, 4))
			return;
	}
	gba_store32(gba, address, data);
}

static inline void gba_dma_write16(gba_t* gba, UINT32 address, UINT16 data)
{
	if ((address & 0xfffffc00) == 0x04000000) {
		if (gba_process_mmio_write(gba, address, data, 2))
			return;
	}
	gba_store16(gba, address, data);
}

static inline void arm7_write32(void* user_data, UINT32 address, UINT32 data)
{
	gba_compute_access_cycles((gba_t*)user_data, address, 3);
	gba_dma_write32((gba_t*)user_data, address, data);
}

static inline void arm7_write16(void* user_data, UINT32 address, UINT16 data)
{
	gba_compute_access_cycles((gba_t*)user_data, address, 1);
	gba_dma_write16((gba_t*)user_data, address, data);
}

static inline void arm7_write8(void* user_data, UINT32 address, UINT8 data)
{
	gba_compute_access_cycles((gba_t*)user_data, address, 1);
	if ((address & 0xfffffc00) == 0x04000000) {
		if (gba_process_mmio_write((gba_t*)user_data, address, data, 1))
			return;
	}
	gba_store8((gba_t*)user_data, address, data);
}

SB_ALWAYS_INLINE UINT32* gba_dword_lookup(gba_t* gba, UINT32 addr, INT32 req_type)
{
	UINT32 bank = addr >> 24;

	// ROM fast path (bank 0x08-0x0d); matrix/EEPROM/OOB fall through, and the
	// gba_read* callers have already filtered GPIO.
	{
		UINT32 word;
		if (SB_LIKELY(gba_rom_fast_path_word(gba, addr, false, &word))) {
			gba->mem.openbus_word = word;
			if (req_type & 0x3) {
				UINT16 res16 = (UINT16)(word >> GUEST_HALF_SHIFT(addr));
				gba->mem.openbus_word = ((UINT32)res16) * 0x10001u;
			}
			return &gba->mem.openbus_word;
		}
	}

	UINT32* ret = &gba->mem.openbus_word;
	bool raw_ret = false;
	switch (bank) {
		case 0x0:
			if (addr < 0x4000) {
				if (gba->cpu.registers[15] < 0x4000)
					gba->mem.bios_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.bios + (addr & ~3)));
				gba->mem.openbus_word = gba->mem.bios_word;
			}
			break;
		case 0x1:
			break;
		case 0x2:
			ret = (UINT32*)(gba->mem.wram0 + (addr & 0x3fffc));
			raw_ret = true;
			gba->mem.openbus_word = BURN_ENDIAN_SWAP_INT32(*ret);
			break;
		case 0x3:
			ret = (UINT32*)(gba->mem.wram1 + (addr & 0x7ffc));
			raw_ret = true;
			gba->mem.openbus_word = BURN_ENDIAN_SWAP_INT32(*ret);
			break;
		case 0x4:
			if (SB_LIKELY(addr <= 0x40003ff)) {
				if (req_type & GBA_REQ_READ) {
					INT32 io_reg = (addr >> 2) & 0xff;
					if (SB_LIKELY(gba->mem.mmio_reg_valid_lookup[io_reg])) {
						gba_process_mmio_read(gba, addr);
						gba->mem.mmio_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.io + (addr & 0x3fc))) & gba->mem.mmio_data_mask_lookup[io_reg];
						ret = &gba->mem.mmio_word;
					}
				} else
					ret = (UINT32*)(gba->mem.io + (addr & 0x3fc));
			}
			break;
		case 0x5:
			ret = (UINT32*)(gba->mem.palette + (addr & 0x3fc));
			raw_ret = true;
			gba->mem.openbus_word = BURN_ENDIAN_SWAP_INT32(*ret);
			if (req_type & GBA_REQ_WRITE) gba_ppu_worker_note_mem_write(gba, 0x05u);
			break;
		case 0x6:
			if (req_type & GBA_REQ_WRITE) {
				// Record the bound for the worker's per-line VRAM capture and flag the
				// change for whole-frame reuse detection.
				UINT32 vo = (addr & 0x10000) ? ((addr & 0x07ffc) | 0x10000u) : (addr & 0x1fffc);
				if (vo < gba->ppu.vram_dirty_lo) gba->ppu.vram_dirty_lo = vo;
				if (vo + 4 > gba->ppu.vram_dirty_hi) gba->ppu.vram_dirty_hi = vo + 4;
				gba_ppu_worker_note_mem_write(gba, 0x06u);
			}
			if (addr & 0x10000) {
				ret = (UINT32*)(gba->mem.vram + (addr & 0x07ffc) + 0x10000);
				gba->mem.openbus_word = BURN_ENDIAN_SWAP_INT32(*ret);
				if (addr & 0x08000) {
					UINT16 dispcnt = gba_io_read16(gba, GBA_DISPCNT);
					INT32 bg_mode  = SB_BFE(dispcnt, 0, 3);
					// block writes to mirrored VRAM in bitmap mode (Acrobat Kid)
					if (bg_mode > 2 && !(addr & 0x04000)) {
						ret = &gba->mem.openbus_word;*ret = 0;
					}
				}
			} else {
				ret = (UINT32*)(gba->mem.vram + (addr & 0x1fffc));
				gba->mem.openbus_word = BURN_ENDIAN_SWAP_INT32(*ret);
			}
			raw_ret = true;
			break;
		case 0x7:
			ret = (UINT32*)(gba->mem.oam + (addr & 0x3fc));
			raw_ret = true;
			gba->mem.openbus_word = BURN_ENDIAN_SWAP_INT32(*ret);
			if (req_type & GBA_REQ_WRITE) gba_ppu_worker_note_mem_write(gba, 0x07u);
			break;
		case 0x8:
		case 0x9:
		case 0xa:
		case 0xb:
		case 0xc:
		case 0xd: {
			if (gba->cart.backup_type == GBA_BACKUP_EEPROM && (addr & 0xff000000) == 0x0d000000) {
				gba->mem.openbus_word = 1;	// ready when done writing EEPROM
				break;
			}
			if (gba->cart.matrix.active) {
				INT32 maddr = addr & 0x0ffffff;
				if (maddr < 0x2000) {
					gba->mem.openbus_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.matrix_window + (maddr & ~3)));
					if (req_type & 0x3) {
						UINT16 res16 = gba->mem.openbus_word >> (addr & 2) * 8;
						gba->mem.openbus_word = res16 * 0x10001u;
					}
				} else {
					UINT32 echo = ((addr & ~3) >> 1) & 0xffff;
					echo |= (((addr & ~3) + 2) >> 1) << 16;
					gba->mem.openbus_word = echo;
				}
				break;
			}
			INT32 maddr = addr & 0x1fffffc;
			if (SB_UNLIKELY(maddr >= gba->cart.rom_size)) {
				if (gba->cart.vfame.type) {
					UINT32 aligned_addr = addr & ~2;
					UINT32 p0 = gba_vfame_get_pattern(aligned_addr);
					UINT32 p2 = gba_vfame_get_pattern(aligned_addr + 2);
					if (req_type & GBA_REQ_1B) { // 8-bit read
						UINT16 half = (addr & 2) ? p2 : p0;
						UINT8 byte = (addr & 1) ? (half & 0xff) : ((half >> 8) & 0xff);
						gba->mem.openbus_word = byte * 0x01010101u;
					} else if (req_type & GBA_REQ_2B) { // 16-bit read
						UINT16 half = (addr & 2) ? p2 : p0;
						gba->mem.openbus_word = half * 0x00010001u;
					} else { // 32-bit read
						gba->mem.openbus_word = (p0 << 2) + p2;
					}
					break;
				}
				if (gba_rom_mirrors_1m(gba, maddr)) {
					maddr &= 0x0ffffc;
					gba->mem.openbus_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.cart_rom + maddr));
					if (req_type & 0x3) {
						UINT16 res16 = gba->mem.openbus_word >> (addr & 2) * 8;
						gba->mem.openbus_word = res16 * 0x10001u;
					}
				} else {
					gba->mem.openbus_word = ((maddr / 2) & 0xffff) | (((maddr / 2 + 1) & 0xffff) << 16);
					// EEPROM ready only at top of ROM space, not every OOB read
					if (gba->cart.backup_type == GBA_BACKUP_EEPROM && (addr & 0x1ffffff) >= 0x01ffff00)
						gba->mem.openbus_word = 1;
				}
			} else {
				gba->mem.openbus_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(gba->mem.cart_rom + maddr));
				if (req_type & 0x3) {
					UINT16 res16 = gba->mem.openbus_word >> (addr & 2) * 8;
					gba->mem.openbus_word = res16 * 0x10001u;
				}
			}
		}
			break;
		case 0xe:
		case 0xf:
			if (gba->cart.backup_type == GBA_BACKUP_SRAM) {
				gba->mem.sram_word = (UINT32)gba->mem.cart_backup[addr & 0x7fff] * 0x01010101u;
				ret = &gba->mem.sram_word;
			} else if (gba->cart.features & GBA_CART_TILT) {
				gba->mem.sram_word = (UINT32)gba_process_tilt_read(gba, addr) * 0x01010101u;
				ret = &gba->mem.sram_word;
			} else if (gba->cart.backup_type == GBA_BACKUP_EEPROM) {
				ret = (UINT32*)&gba->mem.eeprom_word;
			} else if (gba->cart.backup_type == GBA_BACKUP_NONE) {
				// Detected SRAM savegame
				gba->cart.backup_type = GBA_BACKUP_SRAM;
				gba->mem.sram_word = (UINT32)gba->mem.cart_backup[addr & 0x7fff] * 0x01010101u;
				ret = &gba->mem.sram_word;
			} else if (gba->cart.backup_type == GBA_BACKUP_FORCE_NONE) {
				gba->mem.sram_word = 0xffffffff;
				ret = &gba->mem.sram_word;
			} else {
				// Flash
				if (gba->cart.in_chip_id_mode && addr <= 0xe000001) {
					gba->mem.openbus_word = BURN_ENDIAN_SWAP_INT32(*(UINT32*)gba->mem.flash_chip_id);
					ret = &gba->mem.openbus_word;
				} else {
					gba->mem.sram_word = gba->mem.cart_backup[(addr & 0xffff) + gba->cart.flash_bank * 64 * 1024] * 0x01010101;
					ret = &gba->mem.sram_word;
				}
			}
			gba->mem.openbus_word = (*ret & 0xffff) * 0x10001;
			break;
	}
	if (raw_ret && (req_type & GBA_REQ_READ))
		return &gba->mem.openbus_word;
	return ret;
}

static inline void gba_recompute_mmio_mask_table(gba_t* gba)
{
	for (INT32 io_reg = 0; io_reg < 256;io_reg++) {
		UINT32 dword_address = 0x04000000 + io_reg * 4;
		UINT32 data_mask     = 0xffffffff;
		bool   valid         = true;
		if (dword_address == 0x4000008)
			data_mask &= 0xdfffdfff;
		else if (dword_address == 0x4000048)
			data_mask &= 0x3f3f3f3f;
		else if (dword_address == 0x4000050)
			data_mask &= 0x1f1f3fff;
		else if (dword_address == 0x4000060)
			data_mask &= 0xffc0007f;
		else if (dword_address == 0x4000064 || dword_address == 0x400006c || dword_address == 0x4000074)
			data_mask &= 0x00004000;
		else if (dword_address == 0x4000068)
			data_mask &= 0x0000ffc0;
		else if (dword_address == 0x4000070)
			data_mask &= 0xe00000e0;
		else if (dword_address == 0x4000078)
			data_mask &= 0x0000ff00;
		else if (dword_address == 0x400007c)
			data_mask &= 0x000040ff;
		else if (dword_address == 0x4000080)
			data_mask &= 0x770fff77;
		else if (dword_address == 0x4000084)
			data_mask &= 0x0000008f;
		else if (dword_address == 0x4000088 || dword_address == 0x4000134 || dword_address == 0x4000140 || dword_address == 0x4000158 || dword_address == 0x4000204 || dword_address == 0x4000208)
			data_mask = 0x0000ffff;
		else if (dword_address == 0x40000b8 || dword_address == 0x40000c4 || dword_address == 0x40000d0)
			data_mask &= 0xf7e00000;
		else if (dword_address == 0x40000dc)
			data_mask &= 0xffe00000;
		else if ((dword_address >= 0x4000010 && dword_address <= 0x4000046) ||
			(dword_address == 0x400004c) ||
			(dword_address >= 0x4000054 && dword_address <= 0x400005e) ||
			(dword_address == 0x400008c) ||
			(dword_address >= 0x40000a0 && dword_address <= 0x40000b6) ||
			(dword_address >= 0x40000bc && dword_address <= 0x40000c2) ||
			(dword_address >= 0x40000c8 && dword_address <= 0x40000ce) ||
			(dword_address >= 0x40000d4 && dword_address <= 0x40000da) ||
			(dword_address >= 0x40000e0 && dword_address <= 0x40000fe) ||
			(dword_address == 0x400100c))
			valid = false;
		gba->mem.mmio_data_mask_lookup[io_reg] = data_mask;
		gba->mem.mmio_reg_valid_lookup[io_reg] = valid;
	}
}

static inline void gba_process_mmio_read(gba_t* gba, UINT32 address)
{
	// Force recomputing timers on timer read
	if (address >= GBA_TM0CNT_L && address <= GBA_TM3CNT_H)
		gba_compute_timers(gba);
	// Derive DISPSTAT/VCOUNT from the live beam position (io holds stale boundary snapshots)
	else if (address >= GBA_DISPSTAT && address <= GBA_VCOUNT)
		gba_ppu_refresh_status(gba);
}

static inline bool gba_process_mmio_write(gba_t* gba, UINT32 address, UINT32 data, INT32 req_size_bytes)
{
	UINT32 address_u32 = address & ~3;
	UINT32 word_mask   = 0xffffffff;
	UINT32 word_data   = data;
	if (req_size_bytes == 2) {
		word_data <<= (address & 2) * 8;
		word_mask = 0x0000ffffu << ((address & 2) * 8u);
	} else if (req_size_bytes == 1) {
		word_data <<= (address & 3) * 8;
		word_mask = 0x000000ffu << ((address & 3) * 8u);
	}
	word_data &= word_mask;

	if (address_u32 == GBA_IE) {
		// IE is low half-word (R/W). IF is high half-word; write 1 to acknowledge (W1C).
		// 8/16-bit writes to IF (0x4000202/03) also land here since address & ~3 = 0x4000200.
		UINT32 word = gba_io_read32(gba, GBA_IE);
		UINT16 IE = (UINT16)((word & ~word_mask) | (word_data & word_mask));
		UINT16 IF = (UINT16)((word >> 16) & ~(word_data >> 16));
		gba_io_store32(gba, GBA_IE, ((UINT32)IF << 16) | IE);
		gba_update_interrupt_pending(gba);
		return true;
	} else if (address_u32 == GBA_IME) {
		UINT32 ime = gba_io_read32(gba, GBA_IME);
		gba_io_store32(gba, GBA_IME, (ime & ~word_mask) | (word_data & word_mask));
		gba_update_interrupt_pending(gba);
		return true;
	} else if (address_u32 == GBA_SOUNDCNT_L) {
		// FIFO reset bits are W1C: bit11 for FIFO A (low word of SOUNDCNT_H),
		// bit15 for FIFO B (high byte of SOUNDCNT_H).
		for (INT32 i = 0; i < 2; ++i) {
			UINT32 reset_bit = 1u << (11 + i * 4);
			if ((word_mask >> 16) & reset_bit & (word_data >> 16)) {
				gba->audio.fifo[i].read_ptr  = 0;
				gba->audio.fifo[i].write_ptr = 0;
				gba->audio.fifo[i].latch     = 0;
				word_data &= ~(reset_bit << 16);
			}
		}
		// Merge and store both halves; FIFO reset bits are cleared above.
		UINT32 old = gba_io_read32(gba, GBA_SOUNDCNT_L);
		gba_io_store32(gba, GBA_SOUNDCNT_L, (old & ~word_mask) | (word_data & word_mask));
		return true;
	} else if (address_u32 == GBA_TM0CNT_L || address_u32 == GBA_TM1CNT_L || address_u32 == GBA_TM2CNT_L || address_u32 == GBA_TM3CNT_L) {
		gba_compute_timers(gba);
		INT32 timer_off = (address_u32 - GBA_TM0CNT_L) / 4;
		if (word_mask & 0xffff) {
			gba->timers[timer_off + 0].pending_reload_value = word_data & (word_mask & 0xffff);
		}
		if (word_mask & 0xffff0000) {
			gba_store16(gba, address_u32 + 2, (word_data >> 16) & 0xffff);
			gba->timers[timer_off + 0].reload_value = gba->timers[timer_off + 0].pending_reload_value;
		}
		// Settles at the next processed cycle, matching the forced horizon reset
		gba_timing_schedule(gba, &gba->timer_event, 0);
		return true;
	} else if (address_u32 == GBA_SIOCNT) {
		UINT32 sio_word = gba_io_read32(gba, GBA_SIOCNT);
		sio_word = (sio_word & ~word_mask) | (word_data & word_mask);
		gba_io_store32(gba, GBA_SIOCNT, sio_word);
		if (word_mask & 0xffff) {
			UINT16 siocnt         = gba_io_read16(gba, GBA_SIOCNT);
			bool active           = SB_BFE(siocnt,  7, 1);
			bool internal_clock   = SB_BFE(siocnt,  0, 1);
			gba_timing_deschedule(gba, &gba->sio_event);
			if (active && internal_clock) {
				gba->sio.last_active = true;
				gba_timing_schedule(gba, &gba->sio_event, GBA_SIO_TRANSFER_TICKS);
			}
		}
		return true;
	} else if (address_u32 == GBA_POSTFLG) {
		// Only BIOS can update Post Flag and haltcnt
		if (gba->cpu.registers[15] < 0x4000) {
			// Writes to haltcnt halt the CPU
			if (word_mask & 0xff00) {
				if (word_data & 0x8000) {
					gba->stop_mode         = true;
					gba->frame_in_progress = false;
				}
				gba->cpu.wait_for_interrupt = true;
			}
			UINT32 post = gba_io_read32(gba, address_u32);
			// POST can only be initialized once, then other writes are dropped.
			if ((word_mask & 0xff) && (post & 0xff))
				word_mask &= ~0xff;
			post &= ~word_mask;
			post |= word_data & word_mask;
			gba_io_store32(gba, address_u32, post);
		}
		return true;
	} else if (address_u32 == GBA_BG2X || address_u32 == GBA_BG3X) {
		INT32 aff_bg = (address_u32 - GBA_BG2X) / 0x10;
		gba->ppu.aff[aff_bg].wrote_bgx = true;
	} else if (address_u32 == GBA_BG2Y || address_u32 == GBA_BG3Y) {
		INT32 aff_bg = (address_u32 - GBA_BG2Y) / 0x10;
		gba->ppu.aff[aff_bg].wrote_bgy = true;
	} else if (address_u32 == GBA_DMA0CNT_L || address_u32 == GBA_DMA1CNT_L ||
		address_u32 == GBA_DMA2CNT_L || address_u32 == GBA_DMA3CNT_L) {
		INT32 dma = (address_u32 - GBA_DMA0CNT_L) / 12;
		UINT32 old_word = gba_io_read32(gba, address_u32);
		UINT32 new_word = (old_word & ~word_mask) | (word_data & word_mask);
		bool old_enable = (old_word & 0x80000000) != 0;
		bool new_enable = (new_word & 0x80000000) != 0;
		if (!old_enable && new_enable)
			gba->activate_dmas = true;
		else if (old_enable && !new_enable)
			gba->dma[dma].last_enable = false;
	} else if (address_u32 == GBA_WAITCNT) {
		UINT16 waitcnt = gba_io_read16(gba, GBA_WAITCNT);
		waitcnt = ((waitcnt & ~word_mask) | (word_data & word_mask));
		gba_recompute_waitstate_table(gba, waitcnt);
		return true;
	} else if (address_u32 == GBA_KEYINPUT) {
		// KEYCNT (high word) writes must not clobber read-only KEYINPUT (low word)
		if (word_mask & 0xffff0000) {
			gba_io_store16(gba, GBA_KEYCNT, (word_data >> 16) & 0xc3ff);
			gba_tick_keypad(NULL, gba);
		}
		return true;
	} else if (address_u32 >= GBA_SOUND1CNT_L && address_u32 < GBA_WAVE_RAM) {
		for (INT32 i = 0;i < 4;++i) {
			if (word_mask & (0xffu << (i * 8))) {
				UINT8 byte = gba_audio_process_reg_write(gba, address_u32 + i, SB_BFE(word_data, (i * 8), 8));
				gba_io_store8(gba, address_u32 + i, byte);
			}
		}
		gba_audio_apply_writes(gba);
		return true;
	} else if (address_u32 >= GBA_WAVE_RAM && address_u32 < GBA_WAVE_RAM + 16) {
		// Wave RAM: the byte write already relocates samples, no extra store needed.
		for (INT32 i = 0;i < 4;++i) {
			if (word_mask & (0xffu << (i * 8))) {
				(void)gba_audio_process_reg_write(gba, address_u32 + i, SB_BFE(word_data, (i * 8), 8));
			}
		}
		return true;
	}
	return false;
}

static inline void gba_tick_keypad(sb_joy_t* joy, gba_t* gba)
{
	UINT16 reg_value = 0;
	// Null joy updates are used to tick the joypad when mmios are set
	if (joy) {
		reg_value |= !(joy->inputs[SE_KEY_A     ] > 0.3) << 0;
		reg_value |= !(joy->inputs[SE_KEY_B     ] > 0.3) << 1;
		reg_value |= !(joy->inputs[SE_KEY_SELECT] > 0.3) << 2;
		reg_value |= !(joy->inputs[SE_KEY_START ] > 0.3) << 3;
		reg_value |= !(joy->inputs[SE_KEY_RIGHT ] > 0.3) << 4;
		reg_value |= !(joy->inputs[SE_KEY_LEFT  ] > 0.3) << 5;
		reg_value |= !(joy->inputs[SE_KEY_UP    ] > 0.3) << 6;
		reg_value |= !(joy->inputs[SE_KEY_DOWN  ] > 0.3) << 7;
		reg_value |= !(joy->inputs[SE_KEY_R     ] > 0.3) << 8;
		reg_value |= !(joy->inputs[SE_KEY_L     ] > 0.3) << 9;
		gba_io_store16(gba, GBA_KEYINPUT, reg_value);
	} else
		reg_value = gba_io_read16(gba, GBA_KEYINPUT);

	UINT16 keycnt      = gba_io_read16(gba, GBA_KEYCNT);
	bool irq_enable    = SB_BFE(keycnt, 14, 1);
	bool irq_condition = SB_BFE(keycnt, 15, 1);		//[0: any key, 1: all keys]
	INT32 if_bit = 0;
	if (irq_enable || gba->stop_mode) {
		UINT16 pressed = SB_BFE(reg_value, 0, 10) ^ 0x3ff;
		UINT16 mask    = SB_BFE(keycnt,    0, 10);

		if (irq_condition && ((pressed & mask) == mask))
			if_bit |= 1 << GBA_INT_KEYPAD;
		if (!irq_condition && ((pressed & mask) != 0))
			if_bit |= 1 << GBA_INT_KEYPAD;

		if (if_bit)
			gba->stop_mode = false;

		if (if_bit && !gba->prev_key_interrupt && irq_enable) {
			gba_send_interrupt(gba, 4, if_bit);
			gba->prev_key_interrupt = true;
		} else
			gba->prev_key_interrupt = false;

	}
}

#endif
