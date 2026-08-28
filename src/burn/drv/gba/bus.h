#pragma once

#ifndef GBA_BUS_H
#define GBA_BUS_H

#include "gba.h"

static inline UINT32 gba_read32(gba_t* gba, UINT32 baddr)
{
	if (gba_gpio_address(gba, baddr)) {
		UINT32 address = baddr & ~3;
		UINT32 low     = gba_gpio_read16(gba, address);
		if ((address & 0x1ffffff) == 0xc4)
			return low | ((UINT32)gba_gpio_read16(gba, address + 2) << 16);
		UINT32 rom     = gba_rom_read16( gba, address + 2);
		return low | (rom << 16);
	}
	return *gba_dword_lookup(gba, baddr, GBA_REQ_READ | GBA_REQ_4B);
}

static inline UINT16 gba_read16(gba_t* gba, UINT32 baddr)
{
	if (gba_gpio_address(gba, baddr))
		return gba_gpio_read16(gba, baddr);
	UINT32* val    = gba_dword_lookup(gba, baddr, GBA_REQ_READ | GBA_REQ_2B);
	INT32   offset = SB_BFE(baddr, 1, 1);
	return ((UINT16*)val)[offset];
}

static inline UINT8 gba_read8(gba_t* gba, UINT32 baddr)
{
	if (gba_gpio_address(gba, baddr))
		return (gba_gpio_read16(gba, baddr) >> ((baddr & 1) * 8)) & 0xff;
	UINT32* val    = gba_dword_lookup(gba, baddr, GBA_REQ_READ | GBA_REQ_1B);
	INT32   offset = SB_BFE(baddr, 0, 2);
	return ((UINT8*)val)[offset];
}


static inline void gba_store32(gba_t* gba, UINT32 baddr, UINT32 data)
{
	if (baddr >= 0x08000000) {
		//Mask is 0xfe to catch the sram mirror at 0x0f and 0x0e
		if ((baddr & 0xfe000000) == 0xe000000) {
			gba_process_backup_write(gba, baddr, data >> ((baddr & 3) * 8));
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
	UINT32* val = gba_dword_lookup(gba, baddr, GBA_REQ_WRITE | GBA_REQ_4B);
	*val = data;
}

static inline void gba_store16(gba_t* gba, UINT32 baddr, UINT32 data)
{
	if (baddr >= 0x08000000) {
		//Mask is 0xfe to catch the sram mirror at 0x0f and 0x0e
		if ((baddr & 0xfe000000) == 0xe000000) {
			gba_process_backup_write(gba, baddr, data >> ((baddr & 1) * 8));
			return;
		}
		if (gba_gpio_address(gba, baddr)) {
			gba_gpio_write16(gba, baddr & ~1, data);
			return;
		}
		//Detected EEPROM savegame
		if (gba->cart.backup_type == GBA_BACKUP_NONE && (baddr & 0xff000000) == 0x0d000000)
			gba->cart.backup_type = GBA_BACKUP_EEPROM;
		if (gba->cart.matrix.active && (baddr & 0x01ffff00) == 0x00800100) {
			gba_matrix_write16(gba, baddr & 0x3c, (UINT16)data);
			return;
		}
	}
	UINT32* val    = gba_dword_lookup(gba, baddr, GBA_REQ_WRITE | GBA_REQ_2B);
	INT32   offset = SB_BFE(baddr, 1, 1);
	((UINT16*)val)[offset] = data;
}

static inline void gba_store8(gba_t* gba, UINT32 baddr, UINT32 data)
{
	if (baddr >= 0x05000000) {
		// 8 bit stores to palette mirror across 8 bit halves
		if ((baddr & 0xff000000) == 0x5000000) {
			gba_store16(gba, baddr & ~1, (data & 0xff) * 0x0101);
			return;
		}
		if (((baddr & 0xff000000) == 0x06000000) && ((baddr & 0x1ffff) <= 0x0013fff)) {
			gba_store16(gba, baddr & ~1, (data & 0xff) * 0x0101);
			return;
		}
		//Mask is 0xfe to catch the sram mirror at 0x0f and 0x0e
		if ((baddr & 0xfe000000) == 0xe000000) {
			gba_process_backup_write(gba, baddr, data);
			return;
		}
		// Remaining 8 bit ops are not supported on VRAM or ROM
		return;
	}
	UINT32* val    = gba_dword_lookup(gba, baddr, GBA_REQ_WRITE | GBA_REQ_1B);
	INT32   offset = SB_BFE(baddr, 0, 2);
	((UINT8*)val)[offset] = data;
}


static inline void gba_io_store8(gba_t* gba, UINT32 baddr, UINT8  data)
{
	gba->mem.io[baddr & 0xfff] = data;
}

static inline void gba_io_store16(gba_t* gba, UINT32 baddr, UINT16 data)
{
	*(UINT16*)(gba->mem.io + (baddr & 0xfff)) = data;
}

static inline void gba_io_store32(gba_t* gba, UINT32 baddr, UINT32 data)
{
	*(UINT32*)(gba->mem.io + (baddr & 0xfff)) = data;
}

static inline UINT8  gba_io_read8(gba_t* gba, UINT32 baddr)
{
	return gba->mem.io[baddr & 0xfff];
}

static inline UINT16 gba_io_read16(gba_t* gba, UINT32 baddr)
{
	return *(UINT16*)(gba->mem.io + (baddr & 0xfff));
}

static inline UINT32 gba_io_read32(gba_t* gba, UINT32 baddr)
{
	return *(UINT32*)(gba->mem.io + (baddr & 0xfff));
}

// refreshes the cached interrupt poll flag used once per instruction
static inline void gba_update_interrupt_pending(gba_t* gba)
{
	gba->interrupt_pending = (gba_io_read16(gba, GBA_IE) & gba_io_read16(gba, GBA_IF)) != 0
		&& (gba_io_read32(gba, GBA_IME) & 1);
}

static inline void gba_recompute_waitstate_table(gba_t* gba, UINT16 waitcnt)
{
	// TODO: Make the waitstate for the ROM configureable
	const INT32 wait_state_table[16 * 4] = {
		1, 1, 1, 1,		//0x00 (bios)
		1, 1, 1, 1,		//0x01 (bios)
		3, 3, 6, 6,		//0x02 (256k WRAM)
		1, 1, 1, 1,		//0x03 (32k WRAM)
		1, 1, 1, 1,		//0x04 (IO)
		1, 1, 2, 2,		//0x05 (BG/OBJ Palette)
		1, 1, 2, 2,		//0x06 (VRAM)
		1, 1, 1, 1,		//0x07 (OAM)
		4, 4, 8, 8,		//0x08 (GAMEPAK ROM 0)
		4, 4, 8, 8,		//0x09 (GAMEPAK ROM 0)
		4, 4, 8, 8,		//0x0A (GAMEPAK ROM 1)
		4, 4, 8, 8,		//0x0B (GAMEPAK ROM 1)
		4, 4, 8, 8,		//0x0C (GAMEPAK ROM 2)
		4, 4, 8, 8,		//0x0D (GAMEPAK ROM 2)
		4, 4, 4, 4,		//0x0E (GAMEPAK SRAM)
		1, 1, 1, 1,		//0x0F (unused)
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

	//Each waitstate is two entries in table
	for (INT32 ws = 0;ws < 3;++ws) {
		for (INT32 i = 0;i < 2;++i) {
			UINT8 w_first  = primary_table[wait_first[ws]];
			UINT8 w_second = wait_second[ws] ? 1 : 2;
			if (ws == 1)
				w_second = wait_second[ws] ? 1 : 4;
			if (ws == 2)
				w_second = wait_second[ws] ? 1 : 8;
			w_first += 1;w_second += 1;
			//Wait 0
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

	//SRAM
	gba->mem.wait_state_table[(0x0e * 4) + 0] = 1 + primary_table[sram_wait];
	gba->mem.wait_state_table[(0x0e * 4) + 1] = 1 + primary_table[sram_wait];
	gba->mem.wait_state_table[(0x0e * 4) + 2] = 1 + primary_table[sram_wait];
	gba->mem.wait_state_table[(0x0e * 4) + 3] = 1 + primary_table[sram_wait];
	waitcnt &= (1 << 15);	// Force cartridge to report as GBA cart
	gba_io_store16(gba, GBA_WAITCNT, waitcnt);
}

static inline void gba_compute_access_cycles(gba_t* gba, UINT32 address, INT32 request_size /*0: 1B,1: 2B,3: 4B*/)
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
				//Non sequential->reset prefetch buffer
				gba->mem.prefetch_size         = 0;
				gba->cpu.next_fetch_sequential = false;
			} else {
				//Sequential fetch from prefetch buffer based on available wait states
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

static inline UINT32 gba_compute_access_cycles_dma(gba_t* gba, UINT32 address, INT32 request_size/*0: 1B,1: 2B,3: 4B*/)
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

static inline UINT32 arm7_read32_seq(void* user_data, UINT32 address, bool seq)
{
	gba_compute_access_cycles((gba_t*)user_data, address, seq ? 2 : 3);
	return gba_read32((gba_t*)user_data, address);
}

static inline UINT32 arm7_read16_seq(void* user_data, UINT32 address, bool seq)
{
	gba_compute_access_cycles((gba_t*)user_data, address, seq ? 0 : 1);
	return gba_read16((gba_t*)user_data, address);
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
	if ((address & 0xfffff000) == 0x04000000) {
		if (gba_process_mmio_write((gba_t*)user_data, address, data, 1))
			return;
	}
	gba_store8((gba_t*)user_data, address, data);
}

static inline UINT32* gba_dword_lookup(gba_t* gba, UINT32 addr, INT32 req_type)
{
	UINT32* ret = &gba->mem.openbus_word;
	switch (addr >> 24) {
		case 0x0:
			if (addr < 0x4000) {
				if (gba->cpu.registers[15] < 0x4000)
					gba->mem.bios_word = *(UINT32*)(gba->mem.bios + (addr & ~3));
				gba->mem.openbus_word = gba->mem.bios_word;
			}
			break;
		case 0x1:
			break;
		case 0x2:
			ret = (UINT32*)(gba->mem.wram0 + (addr & 0x3fffc));
			gba->mem.openbus_word = *ret;
			break;
		case 0x3:
			ret = (UINT32*)(gba->mem.wram1 + (addr & 0x7ffc));
			gba->mem.openbus_word = *ret;
			break;
		case 0x4:
			if (SB_LIKELY(addr <= 0x40003ff)) {
				if (req_type & GBA_REQ_READ) {
					INT32 io_reg = (addr >> 2) & 0xff;
					if (SB_LIKELY(gba->mem.mmio_reg_valid_lookup[io_reg])) {
						gba_process_mmio_read(gba, addr);
						gba->mem.mmio_word = (*(UINT32*)(gba->mem.io + (addr & 0x3fc))) & gba->mem.mmio_data_mask_lookup[io_reg];
						ret = &gba->mem.mmio_word;
					}
				} else
					ret = (UINT32*)(gba->mem.io + (addr & 0x3fc));
			}
			break;
		case 0x5:
			ret = (UINT32*)(gba->mem.palette + (addr & 0x3fc));
			gba->mem.openbus_word = *ret;
			break;
		case 0x6:
			if (addr & 0x10000) {
				ret = (UINT32*)(gba->mem.vram + (addr & 0x07ffc) + 0x10000);
				gba->mem.openbus_word = *ret;
				if (addr & 0x08000) {
					UINT16 dispcnt = gba_io_read16(gba, GBA_DISPCNT);
					INT32 bg_mode  = SB_BFE(dispcnt, 0, 3);
					// block writes to mirrored VRAM in bitmap mode (Acrobat Kid)
					if (bg_mode > 2 && !(addr & 0x04000)) {
						ret = &gba->mem.openbus_word;*ret = 0;
					}
				}
			} else
				ret = (UINT32*)(gba->mem.vram + (addr & 0x1fffc));
			gba->mem.openbus_word = *ret;
			break;
		case 0x7:
			ret = (UINT32*)(gba->mem.oam + (addr & 0x3fc));
			gba->mem.openbus_word = *ret;
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
					gba->mem.openbus_word = *(UINT32*)(gba->mem.matrix_window + (maddr & ~3));
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
				if (gba->cart.fcmini.type) {
					gba->mem.openbus_word = gba_fcmini_get_pattern(addr) | (gba_fcmini_get_pattern(addr + 2) << 16);
					break;
				}
				if (gba_rom_mirrors_1m(gba, maddr)) {
					maddr &= 0x0ffffc;
					gba->mem.openbus_word = *(UINT32*)(gba->mem.cart_rom + maddr);
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
				gba->mem.openbus_word = *(UINT32*)(gba->mem.cart_rom + maddr);
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
				//Flash
				if (gba->cart.in_chip_id_mode && addr <= 0xe000001) {
					gba->mem.openbus_word = *(UINT32*)gba->mem.flash_chip_id;
					ret = &gba->mem.openbus_word;
				} else {
					gba->mem.sram_word = gba->mem.cart_backup[(addr & 0xffff) + gba->cart.flash_bank * 64 * 1024] * 0x01010101;
					ret = &gba->mem.sram_word;
				}
			}
			gba->mem.openbus_word = (*ret & 0xffff) * 0x10001;
			break;
	}
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
		UINT16 IE = gba_io_read16(gba, GBA_IE);
		UINT16 IF = gba_io_read16(gba, GBA_IF);

		IE = ((IE & ~word_mask) | (word_data & word_mask)) >> 0;
		IF &= ~((word_data) >> 16);
		gba_io_store16(gba, GBA_IE, IE);
		gba_io_store16(gba, GBA_IF, IF);
		gba_update_interrupt_pending(gba);

		return true;
	} else if (address_u32 == GBA_IF) {
		//Writing 1 to an IF bit acknowledges (clears) the interrupt
		UINT16 IF = gba_io_read16(gba, GBA_IF);
		IF &= ~(word_data & word_mask);
		gba_io_store16(gba, GBA_IF, IF);
		gba_update_interrupt_pending(gba);
		return true;
	} else if (address_u32 == GBA_IME) {
		UINT32 ime = gba_io_read32(gba, GBA_IME);
		gba_io_store32(gba, GBA_IME, (ime & ~word_mask) | (word_data & word_mask));
		gba_update_interrupt_pending(gba);
		return true;
	} else if (address_u32 == GBA_SOUNDCNT_L) {
		if (word_mask & 0xffff0000) {
			UINT16 soundcnt_h = word_data >> 16;
			for (INT32 i = 0;i < 2;++i) {
				if (SB_BFE(soundcnt_h, 11 + i * 4, 1)) {
					gba->audio.fifo[i].read_ptr  = 0;
					gba->audio.fifo[i].write_ptr = 0;
					for (INT32 d = 0;d < 32;++d)
						gba->audio.fifo[i].data[d] = 0;
					word_data &= ~(1u << (27 + i * 4));
				}
			}
		}
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
		// settle at the next processed cycle, matching the forced horizon reset
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
		//Only BIOS can update Post Flag and haltcnt
		if (gba->cpu.registers[15] < 0x4000) {
			//Writes to haltcnt halt the CPU
			if (word_mask & 0xff00) {
				if (word_data & 0x8000) {
					gba->stop_mode         = true;
					gba->frame_in_progress = false;
				}
				gba->cpu.wait_for_interrupt = true;
			}
			UINT32 post = gba_io_read32(gba, address_u32);
			//POST can only be initialized once, then other writes are dropped. 
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
	} else if (address_u32 == GBA_KEYINPUT) {
		// KEYCNT (high word) writes must not clobber read-only KEYINPUT (low word)
		if (word_mask & 0xffff0000) {
			gba_io_store16(gba, GBA_KEYCNT, (word_data >> 16) & 0xc3ff);
			gba_tick_keypad(NULL, gba);
		}
		return true;
	} else if (address_u32 >= GBA_SOUND1CNT_L && address_u32 < GBA_WAVE_RAM) {
		for (INT32 i = 0;i < 4;++i) {
			if (word_mask & (0xff << (i * 8))) {
				UINT8 byte = gba_audio_process_byte_write(gba, address_u32 + i, SB_BFE(word_data, (i * 8), 8));
				gba_io_store8(gba, address_u32 + i, byte);
			}
		}
		gba_process_audio_writes(gba);
		return true;
	}
	return false;
}

static inline void gba_tick_keypad(sb_joy_t* joy, gba_t* gba)
{
	UINT16 reg_value = 0;
	//Null joy updates are used to tick the joypad when mmios are set
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
