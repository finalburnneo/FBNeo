#pragma once

#ifndef GBA_CART_H
#define GBA_CART_H

#include "gba.h"
#include "gbabios.h"
#include <ctype.h>

// CRC-32 (reflected polynomial 0xEDB88320), table T0 is the standard 8-bit
// table; T1..T7 are pre-generated at startup for slice-by-8 processing.
static UINT32 CRC32_TABLE0[256];
static UINT32 CRC32_TABLE1[256];
static UINT32 CRC32_TABLE2[256];
static UINT32 CRC32_TABLE3[256];
static UINT32 CRC32_TABLE4[256];
static UINT32 CRC32_TABLE5[256];
static UINT32 CRC32_TABLE6[256];
static UINT32 CRC32_TABLE7[256];
static bool   gba_crc32_tables_ready = false;

static void gba_crc32_init_tables(void)
{
	static const UINT32 poly = 0xEDB88320u;
	for (UINT32 i = 0; i < 256; ++i) {
		UINT32 c = i;
		for (UINT32 j = 0; j < 8; ++j)
			c = (c >> 1) ^ ((c & 1) ? poly : 0);
		CRC32_TABLE0[i] = c;
	}
	for (UINT32 i = 0; i < 256; ++i) {
		CRC32_TABLE1[i] = (CRC32_TABLE0[i] >> 8) ^ CRC32_TABLE0[CRC32_TABLE0[i] & 0xFF];
		CRC32_TABLE2[i] = (CRC32_TABLE1[i] >> 8) ^ CRC32_TABLE0[CRC32_TABLE1[i] & 0xFF];
		CRC32_TABLE3[i] = (CRC32_TABLE2[i] >> 8) ^ CRC32_TABLE0[CRC32_TABLE2[i] & 0xFF];
		CRC32_TABLE4[i] = (CRC32_TABLE3[i] >> 8) ^ CRC32_TABLE0[CRC32_TABLE3[i] & 0xFF];
		CRC32_TABLE5[i] = (CRC32_TABLE4[i] >> 8) ^ CRC32_TABLE0[CRC32_TABLE4[i] & 0xFF];
		CRC32_TABLE6[i] = (CRC32_TABLE5[i] >> 8) ^ CRC32_TABLE0[CRC32_TABLE5[i] & 0xFF];
		CRC32_TABLE7[i] = (CRC32_TABLE6[i] >> 8) ^ CRC32_TABLE0[CRC32_TABLE6[i] & 0xFF];
	}
	gba_crc32_tables_ready = true;
}

static UINT32 gba_crc32(const UINT8* data, UINT32 length)
{
	if (!gba_crc32_tables_ready)
		gba_crc32_init_tables();
	UINT32 crc = 0xFFFFFFFFu;
	const UINT8* p = data;
	// Process 8 bytes at a time via slice-by-8. The word is normalized to
	// little-endian order so the byte lanes match the host on either endianness.
	while (length >= 8) {
		UINT64 w;
		memcpy(&w, p, 8);
		w = BURN_ENDIAN_SWAP_INT64(w);
		UINT32 one   = (UINT32)(w & 0xFFFFFFFFu) ^ crc;
		UINT32 two   = (UINT32)(w >> 32);
		crc = CRC32_TABLE7[ one        & 0xFF] ^
		      CRC32_TABLE6[(one >> 8)  & 0xFF] ^
		      CRC32_TABLE5[(one >> 16) & 0xFF] ^
		      CRC32_TABLE4[(one >> 24) & 0xFF] ^
		      CRC32_TABLE3[ two        & 0xFF] ^
		      CRC32_TABLE2[(two >> 8)  & 0xFF] ^
		      CRC32_TABLE1[(two >> 16) & 0xFF] ^
		      CRC32_TABLE0[(two >> 24) & 0xFF];
		p      += 8;
		length -= 8;
	}
	// Tail: single-byte fallback (no branch misprediction on short inputs).
	while (length) {
		crc = CRC32_TABLE0[(crc ^ *p) & 0xFF] ^ (crc >> 8);
		++p;
		--length;
	}
	return ~crc;
}

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

// VFame unlicensed cartridge out-of-bounds ROM read pattern
static inline UINT32 gba_vfame_pattern_right_shift2(UINT32 addr)
{
	UINT32 value = addr & 0xffff;
	value >>= 2;
	value += (addr & 3) == 2  ? 0x8000 : 0;
	value += (addr & 0x10000) ? 0x4000 : 0;
	return value;
}

static inline UINT16 gba_vfame_get_pattern(UINT32 addr)
{
	addr &= 0x1fffff;
	UINT32 value = 0;
	switch (addr & 0x1f0000) {
		case 0x000000:
		case 0x010000: value = (addr >> 1) & 0xffff;								break;
		case 0x020000: value = addr & 0xffff;									break;
		case 0x030000: value = (addr & 0xffff) + 1;								break;
		case 0x040000: value = 0xffff - (addr & 0xffff);						break;
		case 0x050000: value = (0xffff - (addr & 0xffff)) - 1;					break;
		case 0x060000: value = (addr & 0xffff) ^ 0xaaaa;						break;
		case 0x070000: value = ((addr & 0xffff) ^ 0xaaaa) + 1;					break;
		case 0x080000: value = (addr & 0xffff) ^ 0x5555;						break;
		case 0x090000: value = ((addr & 0xffff) ^ 0x5555) - 1;					break;
		case 0x0a0000:
		case 0x0b0000: value = gba_vfame_pattern_right_shift2(addr);			break;
		case 0x0c0000:
		case 0x0d0000: value = 0xffff - gba_vfame_pattern_right_shift2(addr);	break;
		case 0x0e0000:
		case 0x0f0000: value = gba_vfame_pattern_right_shift2(addr) ^ 0xaaaa;	break;
		case 0x100000:
		case 0x110000: value = gba_vfame_pattern_right_shift2(addr) ^ 0x5555;	break;
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
		if (gba->cart.vfame.type)
			return gba_vfame_get_pattern(address);
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
					case FLASH_ENTER_CHIP_ID: gba->cart.in_chip_id_mode = true;             break;
					case FLASH_EXIT_CHIP_ID:  gba->cart.in_chip_id_mode = false;            break;
					case FLASH_PREP_ERASE:    gba->cart.flash_state     = FLASH_PREP_ERASE; break;
					case FLASH_WRITE_BYTE:    gba->cart.flash_state     = FLASH_WRITE_BYTE; break;
					case FLASH_SET_BANK:      gba->cart.flash_state     = FLASH_SET_BANK;   break;
					default: printf("Unknown flash command: %02x\n", data);                 break;
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

// VFame SRAM write address/value bit-scrambling
static inline UINT32 gba_vfame_reorder_bits(UINT32 value, const UINT8* reordering, INT32 len)
{
	UINT32 result = 0;
	for (INT32 i = 0; i < len; i++)
		if (SB_BFE(value, reordering[i], 1))
			result |= 1 << ((len == 16) ? (15 - i) : (7 - i));
	return result;
}

static inline void gba_vfame_sram_write(gba_t* gba, UINT32 address, UINT8 value)
{
	gba_vfame_t* vfame = &gba->cart.vfame;
	// Mode change sequence detection
	if (address >= 0xfff8 && address <= 0xfffc) {
		vfame->write_sequence[address - 0xfff8] = value;
		if (address == 0xfffc) {
			static const UINT8 start_seq[5] = { 0x99, 0x02, 0x05, 0x02, 0x03 };
			static const UINT8 end_seq[5]   = { 0x99, 0x03, 0x62, 0x02, 0x56 };
			if (memcmp(start_seq, vfame->write_sequence, 5) == 0)
				vfame->accepting_mode_change = true;
			if (memcmp(end_seq,   vfame->write_sequence, 5) == 0)
				vfame->accepting_mode_change = false;
		}
	}
	if (vfame->accepting_mode_change) {
		if (address == 0xfffe) {
			vfame->sram_mode = value;
		} else if (address == 0xfffd) {
			vfame->rom_mode = value;
		}
	}
	if (vfame->sram_mode == -1)
		return;

	// addr_reorder[type][mode-1][16]
	static const UINT8 addr_reorder[3][3][16] = {
		{	// VFAME_STANDARD [0]
			{ 15, 14,  9,  1,  8, 10,  7,  3,  5, 11,  4,  0, 13, 12,  2,  6 },
			{ 15,  7, 13,  5, 11,  6,  0,  9, 12,  2, 10, 14,  3,  1,  8,  4 },
			{ 15,  0,  3, 12,  2,  4, 14, 13,  1, 8,   6,  7,  9,  5, 11, 10 }
		},
		{	// VFAME_GEORGE [1]
			{ 15,  7, 13,  1, 11, 10, 14,  9, 12,  2,  4,  0,  3,  5,  8,  6 },
			{ 15, 14,  3, 12,  8,  4,  0, 13,  5, 11,  6,  7,  9,  1,  2, 10 },
			{ 15,  0,  9,  5,  2,  6,  7,  3,  1,  8, 10, 14, 13, 12, 11,  4 }
		},
		{	// VFAME_ALTERNATE [2]
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

	INT32 addr_mode = vfame->sram_mode & 0x3;
	INT32 type = vfame->type - 1;	// 0: standard, 1: george, 2: alternate
	if (addr_mode != 0) {
		address = gba_vfame_reorder_bits(address, addr_reorder[type][addr_mode - 1], 16);
	}
	INT32 val_mode = (vfame->sram_mode >> 2) & 0x3;
	if (val_mode != 0) {
		value = gba_vfame_reorder_bits(value, val_reorder[type][val_mode - 1], 8);
	}
	if (vfame->sram_mode & 0x80)
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
		if (gba->cart.vfame.type) {
			gba_vfame_sram_write(gba, baddr, data & 0xff);
		} else if (gba->mem.cart_backup[baddr & 0x7fff] != (data & 0xff)) {
			gba->mem.cart_backup[baddr & 0x7fff] = data & 0xff;
			gba->cart.backup_is_dirty = true;
		}
	} else if (gba->cart.features & GBA_CART_TILT) {
		gba_process_tilt_write(gba, baddr, data & 0xff);
	}
}

// SWAR helper: returns a 64-bit mask where each byte position is 0x80 if that
// byte of w equals ch, otherwise 0x00.
static inline UINT64 gba_swar_byte_eq(UINT64 w, UINT8 ch)
{
	UINT64 m = 0x0101010101010101ULL * ch;
	UINT64 x = w ^ m;
	return (x - 0x0101010101010101ULL) & ~x & 0x8080808080808080ULL;
}

// Count trailing zero bits of a non-zero 64-bit value.
static inline UINT32 gba_count_trailing_zeros(UINT64 value)
{
#if defined(__GNUC__) || defined(__clang__)
	return (UINT32)__builtin_ctzll(value);
#else
	UINT32 count = 0;
	if ((value & 0x00000000FFFFFFFFULL) == 0) { value >>= 32; count += 32; }
	if ((value & 0x000000000000FFFFULL) == 0) { value >>= 16; count += 16; }
	if ((value & 0x00000000000000FFULL) == 0) { value >>=  8; count +=  8; }
	if ((value & 0x000000000000000FULL) == 0) { value >>=  4; count +=  4; }
	if ((value & 0x0000000000000003ULL) == 0) { value >>=  2; count +=  2; }
	if ((value & 0x0000000000000001ULL) == 0) { count +=  1; }
	return count;
#endif
}

// The cartridge declares its backup type through a signature string placed in the ROM image.
// Signature prefixes are dispatched by lead byte: 'E' -> EEPROM_, 'S' -> SRAM_, 'F' -> FLASH*_.
// Bulk 8-byte SWAR scanning skips most bytes without branching; only candidate bytes run memcmp.
INT32 gba_search_rom_for_backup_string(gba_t* gba)
{
	// Per-lead signatures: only E/S/F lead bytes occur in our list.
	typedef struct { const char* text; UINT32 len; INT32 btype; } sig_entry;
	static const sig_entry sig_e[] = { { "EEPROM_",  sizeof("EEPROM_")  - 1, GBA_BACKUP_EEPROM      } };
	static const sig_entry sig_s[] = { { "SRAM_",    sizeof("SRAM_")    - 1, GBA_BACKUP_SRAM        } };
	static const sig_entry sig_f[] = {
		{ "FLASH_",     sizeof("FLASH_")     - 1, GBA_BACKUP_FLASH_64K },
		{ "FLASH512_",  sizeof("FLASH512_")  - 1, GBA_BACKUP_FLASH_64K },
		{ "FLASH1M_",   sizeof("FLASH1M_")   - 1, GBA_BACKUP_FLASH_128K },
	};

	UINT32       rom_size = gba->cart.rom_size;
	const UINT8* rom      = gba->mem.cart_rom;
	INT32        btype    = GBA_BACKUP_NONE;
	UINT32       offset   = 0;

	// 64-bit bulk SWAR filter: entire 8-byte blocks with no E/S/F at any
	// position are skipped in one branch.
	while (offset + 8 <= rom_size) {
		UINT64 w;
		memcpy(&w, rom + offset, 8);
		w = BURN_ENDIAN_SWAP_INT64(w);
		UINT64 mE = gba_swar_byte_eq(w, 'E');
		UINT64 mS = gba_swar_byte_eq(w, 'S');
		UINT64 mF = gba_swar_byte_eq(w, 'F');
		UINT64 m  = mE | mS | mF;
		if (!m) {
			offset += 8;
			continue;
		}
		// At least one lead byte hit; check each hit byte by dispatch.
		while (m) {
			UINT32 bit  = gba_count_trailing_zeros(m);
			UINT32 byte = bit >> 3;
			UINT32 pos  = offset + byte;
			UINT8  c    = (UINT8)(w >> (byte * 8));
			const sig_entry* sigs;
			UINT32 n_sigs;
			switch (c) {
				case 'E': sigs = sig_e; n_sigs = ARRAY_SIZE(sig_e); break;
				case 'S': sigs = sig_s; n_sigs = ARRAY_SIZE(sig_s); break;
				case 'F': sigs = sig_f; n_sigs = ARRAY_SIZE(sig_f); break;
				default:  m &= m - 1; continue;
			}
			for (UINT32 k = 0; k < n_sigs; ++k) {
				UINT32 len = sigs[k].len;
				if (pos + len > rom_size)
					continue;
				if (memcmp(sigs[k].text, rom + pos, len) != 0)
					continue;
				if (btype != sigs[k].btype && btype != GBA_BACKUP_NONE) {
					printf("Found multiple backup types, defaulting to none\n");
					return GBA_BACKUP_NONE;
				}
				btype = sigs[k].btype;
			}
			m &= m - 1;
		}
		offset += 8;
	}
	// Tail (< 8 bytes): per-byte check with same lead dispatch.
	while (offset < rom_size) {
		UINT8 c = rom[offset];
		const sig_entry* sigs;
		UINT32 n_sigs;
		switch (c) {
			case 'E': sigs = sig_e; n_sigs = ARRAY_SIZE(sig_e); break;
			case 'S': sigs = sig_s; n_sigs = ARRAY_SIZE(sig_s); break;
			case 'F': sigs = sig_f; n_sigs = ARRAY_SIZE(sig_f); break;
			default:  ++offset; continue;
		}
		for (UINT32 k = 0; k < n_sigs; ++k) {
			UINT32 len = sigs[k].len;
			if (offset + len > rom_size)
				continue;
			if (memcmp(sigs[k].text, rom + offset, len) != 0)
				continue;
			if (btype != sigs[k].btype && btype != GBA_BACKUP_NONE) {
				printf("Found multiple backup types, defaulting to none\n");
				return GBA_BACKUP_NONE;
			}
			btype = sigs[k].btype;
		}
		++offset;
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

	// VFame unlicensed cartridge detection by init signature at ROM+0x15C,
	// title string, or CRC32. 32MB dumps are de-protected.
	if (emu->rom_size < 0x2000000) {
		static const UINT8 vfame_init_seq[16] = {
			0xb4, 0x00, 0x9f, 0xe5, 0x99, 0x10, 0xa0, 0xe3,
			0x00, 0x10, 0xc0, 0xe5, 0xac, 0x00, 0x9f, 0xe5
		};
		static const UINT8 vfame_lotr_title[16] = {
			0x00, 0x4c, 0x4f, 0x52, 0x44, 0x00, 0x57, 0x4f,
			0x52, 0x44, 0x00, 0x00, 0x41, 0x4b, 0x49, 0x4a
		};
		bool is_vfame = false;
		if (emu->rom_size >= 0x16C && memcmp(vfame_init_seq, emu->rom_data + 0x15c, 16) == 0) {
			gba->cart.vfame.type      = GBA_VFAME_STANDARD;
			is_vfame                  = true;
		}
		if (emu->rom_size >= 0xB0) {
			const char* title = (const char*)emu->rom_data + 0xA0;
			if (memcmp(title, vfame_lotr_title, 16) == 0) {
				gba->cart.vfame.type  = GBA_VFAME_STANDARD;
				is_vfame              = true;
			}
			if (memcmp(title, "George Sango", 12) == 0) {
				gba->cart.vfame.type  = GBA_VFAME_GEORGE;
				is_vfame              = true;
			}
		}
		// CRC32 match for Digimon Sapphire (Chinese), can override STANDARD
		{
			UINT32 rom_crc = gba_crc32(emu->rom_data, emu->rom_size);
			if (rom_crc == 0x793A328F) {
				gba->cart.vfame.type  = GBA_VFAME_ALTERNATE;
				is_vfame              = true;
			}
		}
		if (is_vfame) {
			gba->cart.vfame.sram_mode = -1;
			gba->cart.backup_type     = GBA_BACKUP_SRAM;
		}
	}

	// Scan ROM for a backup signature string; skip 32MB and larger de-protected dumps to avoid false matches
	if (!gba->cart.vfame.type && gba->cart.rom_size < 0x2000000)
		gba->cart.backup_type = gba_search_rom_for_backup_string(gba);

	// Famicom Mini (Classic NES Series) fallback by game-code prefix: 'F' with
	// EEPROM backup, some carrying no signature string. Applied only when the
	// scan above found none.
	if (!gba->cart.vfame.type &&
	    gba->cart.backup_type == GBA_BACKUP_NONE &&
	    emu->rom_size >= 0xB2 &&
	    emu->rom_data[0xAC] == 'F' &&
	    emu->rom_data[0xB0] == '0' && emu->rom_data[0xB1] == '1') {
		gba->cart.backup_type = GBA_BACKUP_EEPROM;
	}

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
