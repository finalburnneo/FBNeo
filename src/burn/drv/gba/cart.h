#pragma once

#ifndef GBA_CART_H
#define GBA_CART_H

#include "gba.h"
#include "bios.h"
#include <ctype.h>


static inline UINT8* sb_load_file_data(const char*, size_t* fileSize)
{
	if (fileSize)
		*fileSize = 0;
	return NULL;
}

static inline void sb_free_file_data(UINT8 *data)
{
	free(data);
}

static sb_emu_state_t *gba_host_loading;

static inline bool gba_load_bios_file(const char*, const char*, const char*, UINT8* data, size_t dataSize)
{
	if (gba_host_loading == NULL || gba_host_loading->bios_data == NULL || gba_host_loading->bios_size != dataSize)
		return false;
	memcpy(data, gba_host_loading->bios_data, dataSize);
	return true;
}

// FC Mini (FC Mini) cartridge pattern value for out-of-bounds ROM reads
static inline UINT32 gba_fcmini_pattern_right_shift2(UINT32 addr)
{
	UINT32 value = addr & 0xffff;
	value >>= 2;
	value += (addr & 3) == 2  ? 0x8000 : 0;
	value += (addr & 0x10000) ? 0x4000 : 0;
	return value;
}

static inline UINT16 gba_fcmini_get_pattern(UINT32 addr)
{
	addr &= 0x1fffff;
	UINT32 value = 0;
	switch (addr & 0x1f0000) {
		case 0x000000:
		case 0x010000: value = (addr >> 1) & 0xffff;							break;
		case 0x020000: value = addr & 0xffff;									break;
		case 0x030000: value = (addr & 0xffff) + 1;								break;
		case 0x040000: value = 0xffff - (addr & 0xffff);						break;
		case 0x050000: value = (0xffff - (addr & 0xffff)) - 1;					break;
		case 0x060000: value = (addr & 0xffff) ^ 0xaaaa;						break;
		case 0x070000: value = ((addr & 0xffff) ^ 0xaaaa) + 1;					break;
		case 0x080000: value = (addr & 0xffff) ^ 0x5555;						break;
		case 0x090000: value = ((addr & 0xffff) ^ 0x5555) - 1;					break;
		case 0x0a0000:
		case 0x0b0000: value = gba_fcmini_pattern_right_shift2(addr);			break;
		case 0x0c0000:
		case 0x0d0000: value = 0xffff - gba_fcmini_pattern_right_shift2(addr);	break;
		case 0x0e0000:
		case 0x0f0000: value = gba_fcmini_pattern_right_shift2(addr) ^ 0xaaaa;	break;
		case 0x100000:
		case 0x110000: value = gba_fcmini_pattern_right_shift2(addr) ^ 0x5555;	break;
		case 0x120000: value = 0xffff - ((addr & 0xffff) >> 1);					break;
		case 0x130000: value = 0xffff - ((addr & 0xffff) >> 1) - 0x8000;		break;
		case 0x140000:
		case 0x150000: value = ((addr >> 1) & 0xffff) ^ 0xaaaa;					break;
		case 0x160000:
		case 0x170000: value = ((addr >> 1) & 0xffff) ^ 0x5555;					break;
		case 0x180000:
		case 0x190000: value = ((addr >> 1) & 0xffff) ^ 0xf0f0;					break;
		case 0x1a0000:
		case 0x1b0000: value = ((addr >> 1) & 0xffff) ^ 0x0f0f;					break;
		case 0x1c0000:
		case 0x1d0000: value = ((addr >> 1) & 0xffff) ^ 0xff00;					break;
		case 0x1e0000:
		case 0x1f0000: value = ((addr >> 1) & 0xffff) ^ 0x00ff;					break;
	}
	return value & 0xffff;
}

static inline bool gba_rom_mirrors_1m(const gba_t* gba, UINT32 offset)
{
	return gba->cart.rom_size == 0x100000 && offset < 0x400000;
}

static inline UINT16 gba_rom_read16(const gba_t* gba, UINT32 address)
{
	UINT32 offset = address & 0x1fffffe;
	if (offset >= gba->cart.rom_size) {
		if (gba->cart.fcmini.type)
			return gba_fcmini_get_pattern(address);
		if (gba_rom_mirrors_1m(gba, offset))
			offset &= 0x0ffffe;
		else
			return (address >> 1) & 0xffff;
	}
	return gba->mem.cart_rom[offset] | (gba->mem.cart_rom[offset + 1] << 8);
}

static inline void gba_process_flash_state_machine(gba_t* gba, UINT32 baddr, UINT8 data)
{
#define FLASH_DEFAULT		0x00
#define FLASH_RECV_AA		0x01
#define FLASH_RECV_55		0x02
#define FLASH_ERASE_RECV_AA	0x03
#define FLASH_ERASE_RECV_55	0x04

#define FLASH_ENTER_CHIP_ID	0x90
#define FLASH_EXIT_CHIP_ID	0xF0
#define FLASH_PREP_ERASE	0x80
#define FLASH_ERASE_CHIP	0x10
#define FLASH_ERASE_4KB		0x30
#define FLASH_WRITE_BYTE	0xA0
#define FLASH_SET_BANK		0xB0

	INT32 state = gba->cart.flash_state;
	gba->cart.flash_state = FLASH_DEFAULT;
	baddr &= 0xffff;
	switch (state) {
		default:
			printf("Unknown flash state %02x\n", gba->cart.flash_state);
		case FLASH_DEFAULT:
			if (baddr == 0x5555 && data == 0xaa) gba->cart.flash_state = FLASH_RECV_AA;
			break;
		case FLASH_RECV_AA:
			if (baddr == 0x2aaa && data == 0x55) gba->cart.flash_state = FLASH_RECV_55;
			break;
		case FLASH_RECV_55:
			if (baddr == 0x5555) {
				switch (data) {
					case FLASH_ENTER_CHIP_ID:gba->cart.in_chip_id_mode = true;             break;
					case FLASH_EXIT_CHIP_ID: gba->cart.in_chip_id_mode = false;            break;
					case FLASH_PREP_ERASE:   gba->cart.flash_state     = FLASH_PREP_ERASE; break;
					case FLASH_WRITE_BYTE:   gba->cart.flash_state     = FLASH_WRITE_BYTE; break;
					case FLASH_SET_BANK:     gba->cart.flash_state     = FLASH_SET_BANK;   break;
					default: printf("Unknown flash command: %02x\n", data);                break;
				}
			}
			break;
		case FLASH_PREP_ERASE:
			if (baddr == 0x5555 && data == 0xaa) gba->cart.flash_state = FLASH_ERASE_RECV_AA;
			break;
		case FLASH_ERASE_RECV_AA:
			if (baddr == 0x2aaa && data == 0x55) gba->cart.flash_state = FLASH_ERASE_RECV_55;
			break;
		case FLASH_ERASE_RECV_55:
			if (baddr == 0x5555 || data == FLASH_ERASE_4KB) {
				INT32 size         = gba->cart.backup_type == GBA_BACKUP_FLASH_64K ? 64 * 1024 : 128 * 1024;
				INT32 erase_4k_off = gba->cart.flash_bank * 64 * 1024 + SB_BFE(baddr, 12, 4) * 4096;
				// Process command
				switch (data) {
					case FLASH_ERASE_CHIP:
						printf("Erase Flash Chip %d bytes\n", size);
						for (INT32 i = 0;i < size;++i) gba->mem.cart_backup[i] = 0xff;
						break;
					case FLASH_ERASE_4KB:
						for (INT32 i = 0;i < 4096;++i) gba->mem.cart_backup[erase_4k_off + i] = 0xff;
						break;
					default:
						printf("Unknown flash erase command: %02x\n", data);
						break;
				}
				gba->cart.backup_is_dirty = true;
			}
			break;
		case FLASH_WRITE_BYTE:
			gba->mem.cart_backup[gba->cart.flash_bank * 64 * 1024 + baddr] &= data;
			gba->cart.backup_is_dirty = true;
			break;
		case FLASH_SET_BANK:
			gba->cart.flash_bank = data & 1;
			break;
	}
}

// FC Mini SRAM write - address/value scrambling for FC Mini carts
static inline UINT32 gba_fcmini_reorder_bits(UINT32 value, const UINT8* reordering, INT32 len)
{
	UINT32 result = 0;
	for (INT32 i = 0; i < len; i++)
		if (SB_BFE(value, i, 1))
			result |= 1 << reordering[i];
	return result;
}

static inline void gba_fcmini_sram_write(gba_t* gba, UINT32 address, UINT8 value)
{
	gba_fcmini_t* fcmini = &gba->cart.fcmini;
	// Mode change sequence detection
	if (address >= 0xfff8 && address <= 0xfffc) {
		fcmini->write_sequence[address - 0xfff8] = value;
		if (address == 0xfffc) {
			static const UINT8 start_seq[5] = { 0x99, 0x02, 0x05, 0x02, 0x03 };
			static const UINT8 end_seq[5]   = { 0x99, 0x03, 0x62, 0x02, 0x56 };
			if (memcmp(start_seq, fcmini->write_sequence, 5) == 0)
				fcmini->accepting_mode_change = true;
			if (memcmp(end_seq,   fcmini->write_sequence, 5) == 0)
				fcmini->accepting_mode_change = false;
		}
	}
	if (fcmini->accepting_mode_change) {
		if (address == 0xfffe) {
			fcmini->sram_mode = value;
			return;
		} else if (address == 0xfffd) {
			return;
		}
	}
	if (fcmini->sram_mode == -1)
		return;

	// addr_reorder[type][mode-1][16]
	static const UINT8 addr_reorder[3][3][16] = {
		{	// FCMINI_STANDARD [0]
			{ 15, 14,  9,  1,  8, 10,  7,  3,  5, 11,  4,  0, 13, 12,  2,  6 },
			{ 15,  7, 13,  5, 11,  6,  0,  9, 12,  2, 10, 14,  3,  1,  8,  4 },
			{ 15,  0,  3, 12,  2,  4, 14, 13,  1, 8,   6,  7,  9,  5, 11, 10 }
		},
		{	// FCMINI_GEORGE [1]
			{ 15,  7, 13,  1, 11, 10, 14,  9, 12,  2,  4,  0,  3,  5,  8,  6 },
			{ 15, 14,  3, 12,  8,  4,  0, 13,  5, 11,  6,  7,  9,  1,  2, 10 },
			{ 15,  0,  9,  5,  2,  6,  7,  3,  1,  8, 10, 14, 13, 12, 11,  4 }
		},
		{	// FCMINI_ALTERNATE [2]
			{ 15,  0, 13,  5,  8,  4,  7,  3,  1,  2, 10, 14,  9, 12, 11,  6 },
			{ 15,  7,  9,  1,  2,  6, 14, 13, 12, 11,  4,  0,  3,  5,  8, 10 },
			{ 15, 14,  3, 12, 11, 10,  0,  9,  5,  8,  6,  7, 13,  1,  2,  4 }
		},
	};
	// val_reorder[type][reorder_val-1][8]
	static const UINT8 val_reorder[3][3][8] = {
		{ { 5, 4, 3, 2, 1, 0, 7, 6 }, { 3, 2, 1, 0, 7, 6, 5, 4 }, { 1, 0, 7, 6, 5, 4, 3, 2 } },
		{ { 3, 0, 7, 2, 1, 4, 5, 6 }, { 1, 4, 3, 0, 5, 6, 7, 2 }, { 5, 2, 1, 6, 7, 0, 3, 4 } },
		{ { 5, 4, 7, 2, 1, 0, 3, 6 }, { 1, 2, 3, 0, 5, 6, 7, 4 }, { 3, 0, 1, 6, 7, 4, 5, 2 } },
	};

	INT32 mode = fcmini->sram_mode & 0x3;
	INT32 type = fcmini->type - 1;	// 0: standard, 1: george, 2: alternate
	if (mode != 0) {
		address = gba_fcmini_reorder_bits(address, addr_reorder[type][mode - 1], 16);
		INT32 reorder_val = (fcmini->sram_mode & 0xf) >> 2;
		if (reorder_val != 0)
			value = gba_fcmini_reorder_bits(value, val_reorder[type][reorder_val - 1], 8);
	}
	if (fcmini->sram_mode & 0x80)
		value ^= 0xaa;
	address &= 0x7fff;
	if (gba->mem.cart_backup[address] != value) {
		gba->mem.cart_backup[address] = value;
		gba->cart.backup_is_dirty = true;
	}
}

static inline void gba_process_backup_write(gba_t* gba, UINT32 baddr, UINT32 data)
{
	if (gba->cart.backup_type == GBA_BACKUP_NONE) {
		// store at the flash command base detects flash, other stores detect SRAM
		if (baddr == 0x0e005555) {
			gba->cart.backup_type     = GBA_BACKUP_FLASH_64K;
			gba->mem.flash_chip_id[1] = 0xd4;
			gba->mem.flash_chip_id[0] = 0xbf;
		} else {
			gba->cart.backup_type = GBA_BACKUP_SRAM;
		}
	}
	if (gba->cart.backup_type == GBA_BACKUP_FLASH_64K || gba->cart.backup_type == GBA_BACKUP_FLASH_128K) {
		gba_process_flash_state_machine(gba, baddr, data);
	} else if (gba->cart.backup_type == GBA_BACKUP_SRAM) {
		if (gba->cart.fcmini.type) {
			gba_fcmini_sram_write(gba, baddr, data & 0xff);
		} else if (gba->mem.cart_backup[baddr & 0x7fff] != (data & 0xff)) {
			gba->mem.cart_backup[baddr & 0x7fff] = data & 0xff;
			gba->cart.backup_is_dirty = true;
		}
	} else if (gba->cart.features & GBA_CART_TILT) {
		gba_process_tilt_write(gba, baddr, data & 0xff);
	}
}

INT32 gba_search_rom_for_backup_string(gba_t* gba)
{
	INT32 btype = GBA_BACKUP_NONE;
	for (INT32 b = 0; b < gba->cart.rom_size;++b) {
		const char* strings[] = { "EEPROM_", "SRAM_", "FLASH_", "FLASH512_", "FLASH1M_" };
		INT32 backup_type[] = { GBA_BACKUP_EEPROM, GBA_BACKUP_SRAM, GBA_BACKUP_FLASH_64K, GBA_BACKUP_FLASH_64K, GBA_BACKUP_FLASH_128K };
		for (INT32 type = 0; type < ARRAY_SIZE(strings);++type) {
			INT32 str_off   = 0;
			bool  matches   = true;
			const char* str = strings[type];
			while (str[str_off] && matches) {
				if (b + str_off  >= gba->cart.rom_size)
					matches = false;
				else if (str[str_off] != gba->mem.cart_rom[b + str_off])
					matches = false;
				++str_off;
			}
			if (matches) {
				if (btype != backup_type[type] && btype != GBA_BACKUP_NONE) {
					printf("Found multiple backup types, defaulting to none\n");
					return GBA_BACKUP_NONE;
				}
				btype = backup_type[type];
			}
		}
	}
	return btype;
}

static inline void gba_setup_flash_id(gba_t* gba)
{
	// Not used unless the cartridge enters flash chip-id mode.
	if (gba->cart.backup_type == GBA_BACKUP_FLASH_64K) {
		gba->mem.flash_chip_id[1] = 0xd4;
		gba->mem.flash_chip_id[0] = 0xbf;
	} else {
		gba->mem.flash_chip_id[1] = 0x13;
		gba->mem.flash_chip_id[0] = 0x62;
	}
}

void gba_unload(gba_t* /*gba*/, gba_scratch_t* /*scratch*/)
{
	printf("Unloading GBA\n");
}

// 64MB carts bank-switch an 8KB window through mapper registers
static inline void gba_matrix_remap(gba_t* gba)
{
	if (gba->cart.matrix.vaddr & 0xffffe1ff)
		return;
	if (gba->cart.matrix.size  & 0xffffe1ff)
		return;
	if ((gba->cart.matrix.vaddr + gba->cart.matrix.size - 1) & 0xffffe000)
		return;
	if (gba->cart.matrix.paddr >= gba->cart.rom_size)
		return;
	UINT32 size = gba->cart.matrix.size;
	if (gba->cart.matrix.paddr + size > gba->cart.rom_size)
		size = gba->cart.rom_size - gba->cart.matrix.paddr;
	memcpy(gba->mem.matrix_window + gba->cart.matrix.vaddr, gba->mem.cart_rom + gba->cart.matrix.paddr, size);
}

static inline void gba_matrix_write(gba_t* gba, UINT32 reg, UINT32 value)
{
	switch (reg) {
		case 0x0:
			gba->cart.matrix.cmd = value;
			if (value == 0x01 || value == 0x11)
				gba_matrix_remap(gba);
			return;
		case 0x4:
			gba->cart.matrix.paddr = value & 0x03ffffff;
			return;
		case 0x8:
			gba->cart.matrix.vaddr = value & 0x007fffff;
			return;
		case 0xc:
			if (value == 0)
				return;
			gba->cart.matrix.size = value << 9;
			return;
	}
}

static inline void gba_matrix_write16(gba_t* gba, UINT32 reg, UINT16 value)
{
	switch (reg) {
		case 0x0: gba_matrix_write(gba, reg, value | (gba->cart.matrix.cmd   & 0xffff0000)); break;
		case 0x4: gba_matrix_write(gba, reg, value | (gba->cart.matrix.paddr & 0xffff0000)); break;
		case 0x8: gba_matrix_write(gba, reg, value | (gba->cart.matrix.vaddr & 0xffff0000)); break;
		case 0xc: gba_matrix_write(gba, reg, value | (gba->cart.matrix.size  & 0xffff0000)); break;
	}
}

static inline void gba_matrix_reset(gba_t* gba)
{
	gba->cart.matrix.size  = 0x1000;
	gba->cart.matrix.paddr = 0;
	gba->cart.matrix.vaddr = 0;
	gba_matrix_remap(gba);
	gba->cart.matrix.paddr = 0x200;
	gba->cart.matrix.vaddr = 0x1000;
	gba_matrix_remap(gba);
}

bool gba_load_rom(sb_emu_state_t* emu, gba_t* gba, gba_scratch_t* scratch)
{
	memset(gba,     0, sizeof(gba_t));
	memset(scratch, 0, sizeof(gba_scratch_t));
	gba->solar_sensor.value         = 0xff;
	gba->solar_sensor.pending_value = 0xff;
	gba->gyro_sensor.pending_sample = 0x700;
	gba->tilt_sensor.sample_x       = 0xfff;
	gba->tilt_sensor.sample_y       = 0xfff;
	gba->tilt_sensor.pending_x      = 0x3a0;
	gba->tilt_sensor.pending_y      = 0x3a0;
	if (emu->rom_size > 64 * 1024 * 1024) {
		printf("ROMs with sizes >64MB (%u bytes) are too big for the GBA\n", (UINT32)emu->rom_size);
		return false;
	}

	gba->mem.bios = scratch->bios;
	bool loaded_bios = gba_load_bios_file("GBA BIOS", emu->save_file_path, "gba_bios.bin", scratch->bios, 16 * 1024);
	if (!loaded_bios) {
		memcpy(scratch->bios, gba_bios_bin, sizeof(gba_bios_bin));
		scratch->skip_bios_intro = true;
	}
	gba->cart.rom_size = emu->rom_size;
	gba->mem.cart_rom  = emu->rom_data;

	// 64MB carts use bank-switched windows
	if (emu->rom_size > 0x2000000 && emu->rom_data[0xac] == 'M') {
		gba->cart.matrix.active = true;
		gba_matrix_reset(gba);
	}

	// FC Mini cartridge detection
	{
		static const UINT8 fcmini_init_seq[16] = {
			0xb4, 0x00, 0x9f, 0xe5, 0x99, 0x10, 0xa0, 0xe3,
			0x00, 0x10, 0xc0, 0xe5, 0xac, 0x00, 0x9f, 0xe5
		};
		if (emu->rom_size >= 0x16C && memcmp(fcmini_init_seq, emu->rom_data + 0x15c, 16) == 0) {
			gba->cart.fcmini.type      =  1;	// FCMINI_STANDARD
			gba->cart.fcmini.sram_mode = -1;
			gba->cart.backup_type      = GBA_BACKUP_SRAM;
		}
	}

	// Scan for backup strings; 32MB carts are data-dense, so skip them (false positives)
	if (!gba->cart.fcmini.type && gba->cart.rom_size < 0x2000000)
		gba->cart.backup_type = gba_search_rom_for_backup_string(gba);

	size_t bytes = 0;
	UINT8* data  = sb_load_file_data(emu->save_file_path, &bytes);
	if (data) {
		printf("Loaded save file: %s, bytes: %u\n", emu->save_file_path, (UINT32)bytes);
		if (bytes >= 128 * 1024)
			bytes  = 128 * 1024;
		memcpy(gba->mem.cart_backup, data, (UINT32)bytes);
		sb_free_file_data(data);
	} else {
		printf("Could not find save file: %s\n", emu->save_file_path);
		for (INT32 i = 0;i < sizeof(gba->mem.cart_backup);++i)
			gba->mem.cart_backup[i] = 0xff;
	}

	// Setup flash chip id (unused unless cart has flash backup)
	gba_setup_flash_id(gba);

	gba->cpu = arm7_init(gba);

	for (INT32 bg = 2;bg < 4;++bg) {
		gba_io_store16(gba, GBA_BG2PA + (bg - 2) * 0x10, 1 << 8);
		gba_io_store16(gba, GBA_BG2PB + (bg - 2) * 0x10, 0 << 8);
		gba_io_store16(gba, GBA_BG2PC + (bg - 2) * 0x10, 0 << 8);
		gba_io_store16(gba, GBA_BG2PD + (bg - 2) * 0x10, 1 << 8);
	}
	gba_store16(gba, 0x04000088, 512);
	gba_recompute_waitstate_table(gba, 0);
	gba_recompute_mmio_mask_table(gba);
	gba_io_store16(gba, GBA_KEYINPUT, 0x3ff);	// power-on default: no keys pressed

	if (scratch->skip_bios_intro) {
		printf("No GBA bios using bundled bios\n");
		memcpy(gba->mem.bios, gba_bios_bin, sizeof(gba_bios_bin));
		const UINT32 initial_regs[37] = {
			0x0      , 0x0,0x0      , 0x0,0x0,0x0      , 0x0,0x0,
			0x0      , 0x0,0x0      , 0x0,0x0,0x3007f00, 0x0,0x8000000,
			0xdf     , 0x0,0x0      , 0x0,0x0,0x0      , 0x0,0x0,
			0x3007fa0, 0x0,0x3007fe0, 0x0,0x0,0x0      , 0x0,0x0,
			0x0      , 0x0,0x0      , 0x0,0x0,
		};
		for (INT32 i = 0;i < 37;++i)
			gba->cpu.registers[i] = initial_regs[i];

		const UINT32 initial_mmio_writes[] = {
			0x4000000, 0x80,
			0x4000004, 0x7e0000,
			0x4000020, 0x100,
			0x4000024, 0x1000000,
			0x4000030, 0x100,
			0x4000034, 0x1000000,
			0x4000080, 0xe0000,
			0x4000084, 0xf,
			0x4000088, 0x200,
			0x4000100, 0xff8a,
			0x4000130, 0x3ff,
			0x4000134, 0x8000,
			0x4000300, 0x1,
		};
		for (INT32 i = 0;i < ARRAY_SIZE(initial_mmio_writes);i += 2) {
			UINT32 addr  = initial_mmio_writes[i + 0];
			UINT32 wdata = initial_mmio_writes[i + 1];
			arm7_write32(gba, addr, wdata);
		}
		gba_store32(gba, GBA_IE,      0x1);
		gba_store16(gba, GBA_DISPCNT, 0x9140);
		gba->ppu.dispcnt_pipeline[0] = 0x9140;
		gba->ppu.dispcnt_pipeline[1] = 0x9140;
		gba->ppu.dispcnt_pipeline[2] = 0x9140;
	} else {
		gba->cpu.registers[PC  ] = 0x00000000;
		gba->cpu.registers[CPSR] = 0x000000d3;
	}
	gba->audio.current_sample_generated_time = gba->audio.current_sim_time = 0;
	gba_timing_init(gba);
	return true;
}

UINT64 gba_read_eeprom_bitstream(gba_t* gba, UINT32 source_address, INT32 offset, INT32 size, INT32 elem_size, INT32 dir)
{
	UINT64 data = 0;
	for (INT32 i = 0; i < size; ++i) {
		data |= ((UINT64)(gba_read16(gba, source_address + (i + offset) * elem_size * dir) & 1)) << (size - i - 1);
	}
	return data;
}

void gba_store_eeprom_bitstream(gba_t* gba, UINT32 source_address, INT32 offset, INT32 size, INT32 elem_size, INT32 dir, UINT64 data)
{
	for (INT32 i = 0; i < size; ++i) {
		gba_store16(gba, source_address + (i + offset) * elem_size * dir, data >> (size - i - 1) & 1);
	}
}

#endif
