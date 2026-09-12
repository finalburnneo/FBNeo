#pragma once

#ifndef GBA_DMA_H
#define GBA_DMA_H

#include "gba.h"

// PPU-timed DMA wake, 2 cycles after the hblank/vblank edge; rescan timing bits if dma_wait_ppu is stale
static inline void gba_dma_event(gba_t* gba, sb_emu_state_t* emu, UINT32 cycles_late)
{
	(void)emu;
	(void)cycles_late;
	if (!gba->dma_wait_ppu) {
		for (INT32 i = 0; i < 4; ++i) {
			UINT16 cnt_h = gba_io_read16(gba, GBA_DMA0CNT_H + 12 * i);
			INT32  mode  = SB_BFE(cnt_h, 12, 2);
			if ((cnt_h & 0x8000) && (mode == 1 || mode == 2 || (mode == 3 && i == 3))) {
				gba->dma_wait_ppu = true;
				break;
			}
		}
	}
	gba->activate_dmas |= gba->dma_wait_ppu;
}

// Count latched at enable/repeat; CNT_L writes don't affect a pending transfer
static inline UINT32 gba_dma_latch_count(gba_t* gba, INT32 i)
{
	UINT32 cnt = gba_io_read16(gba, GBA_DMA0CNT_L + 12 * i);
	if (i != 3)
		cnt &= 0x3fff;
	if (cnt == 0)
		cnt = i == 3 ? 0x10000 : 0x4000;
	return cnt;
}

static inline INT32 gba_tick_dma(gba_t*gba, INT32 cycle_delta)
{
	INT32 ticks = 0;
	gba->activate_dmas = false;
	gba->dma_wait_ppu  = false;
	for (INT32 i = 0;i < 4;++i) {
		UINT16 cnt_h = gba_io_read16(gba, GBA_DMA0CNT_H + 12 * i);
		bool enable  = SB_BFE(cnt_h, 15, 1);
		if (enable) {
			bool type = SB_BFE(cnt_h, 10, 1); // 0: 16b 1:32b
			bool enable_edge = !gba->dma[i].last_enable;

			if (enable_edge) {
				gba->dma[i].last_enable = enable;
				gba->dma[i].source_addr = gba_io_read32(gba, GBA_DMA0SAD + 12 * i);
				gba->dma[i].dest_addr   = gba_io_read32(gba, GBA_DMA0DAD + 12 * i);
				// force align addresses on enable
				if (type) {
					gba->dma[i].dest_addr   &= ~3;
					gba->dma[i].source_addr &= ~3;
				} else {
					gba->dma[i].dest_addr   &= ~1;
					gba->dma[i].source_addr &= ~1;
				}
				gba->dma[i].current_transaction = 0;
				gba->dma[i].startup_delay       = 2;
				// (re)enabled PPU-timed channel waits for the NEXT edge, not the current one
				gba->dma[i].last_vblank_seq = gba->ppu.vblank_seq;
				gba->dma[i].last_hblank_seq = gba->ppu.hblank_seq;
				gba->dma[i].latched_count   = gba_dma_latch_count(gba, i);
			}
			INT32  dst_addr_ctl = SB_BFE(cnt_h,  5, 2);	// 0: incr 1: decr 2: fixed 3: incr reload
			INT32  src_addr_ctl = SB_BFE(cnt_h,  7, 2);	// 0: incr 1: decr 2: fixed 3: not allowed
			bool   dma_repeat   = SB_BFE(cnt_h,  9, 1);
			INT32  mode         = SB_BFE(cnt_h, 12, 2);
			bool   irq_enable   = SB_BFE(cnt_h, 14, 1);
			bool   force_first_write_sequential = false;
			INT32  transfer_bytes = type ? 4 : 2;
			bool   skip_dma = false;
			if (gba->dma[i].current_transaction == 0) {
				if (mode == 3 && i == 0)
					continue;
				bool startup_active = gba->dma[i].startup_delay >= 0;
				if (startup_active) {
					gba->dma[i].startup_delay -= cycle_delta;
					if (gba->dma[i].startup_delay >= 0) {
						gba->activate_dmas = true;
						continue;
					}
					gba->dma[i].startup_delay = -1;
				}
				if (mode == 0 && !enable_edge && !startup_active)
					continue;
				if (dst_addr_ctl == 3) {
					gba->dma[i].dest_addr = gba_io_read32(gba, GBA_DMA0DAD + 12 * i);
				}
				// PPU-timed triggers: fire at most once per hblank/vblank rising
				// edge.  The seq counters increment inside the ppu event at the
				// edge, so this is exact regardless of when the poll runs.
				bool vblank_edge = gba->ppu.last_vblank && gba->dma[i].last_vblank_seq != gba->ppu.vblank_seq;
				bool hblank_edge = gba->ppu.last_hblank && gba->dma[i].last_hblank_seq != gba->ppu.hblank_seq;
				gba->dma[i].last_vblank_seq = gba->ppu.vblank_seq;
				gba->dma[i].last_hblank_seq = gba->ppu.hblank_seq;
				if (mode == 1 && !vblank_edge) {
					gba->dma_wait_ppu = true;
					continue;
				}
				if (mode == 2) {
					gba->dma_wait_ppu = true;
					UINT16 vcount = gba_io_read16(gba, GBA_VCOUNT);
					if (vcount >= 160 || !hblank_edge)
						continue;
				}
				// Video dma: fires once per scanline on lines 2-161
				if (mode == 3 && i == 3) {
					gba->dma_wait_ppu = true;
					UINT16 vcount = gba_io_read16(gba, GBA_VCOUNT);
					if (!hblank_edge)
						continue;
					if (vcount < 2 || vcount >= 162)
						continue;
					if (vcount == 161)
						dma_repeat = false;
				}

				if (dst_addr_ctl == 3) {
					gba->dma[i].dest_addr = gba_io_read32(gba, GBA_DMA0DAD + 12 * i);
					// force align reloaded dest address
					if (type)
						gba->dma[i].dest_addr &= ~3;
					else
						gba->dma[i].dest_addr &= ~1;
				}
				bool audio_dma = (mode == 3) && (i == 1 || i == 2);
				if (audio_dma) {
					if (gba->dma[i].activate_audio_dma == false)
						continue;
					gba->dma[i].activate_audio_dma = false;
				}
				if (gba->dma[i].source_addr >= 0x08000000 && gba->dma[i].dest_addr >= 0x08000000) {
					force_first_write_sequential = true;
				} else {
					if (gba->dma[i].dest_addr >= 0x08000000) {
						// Allow the in process prefetech to finish before starting DMA
						if (!gba->mem.prefetch_size && gba->mem.prefetch_en)
							ticks += gba_compute_access_cycles_dma(gba, gba->dma[i].dest_addr, 2) > 4;
					}
				}
				if (gba->dma[i].source_addr >= 0x08000000) {
					if (gba->mem.prefetch_en)
						ticks += gba_compute_access_cycles_dma(gba, gba->dma[i].source_addr, 2) <= 4;
				}
				gba->last_transaction_dma = true;
				UINT32 cnt = gba->dma[i].latched_count;	// latched at enable / last completion

				static const UINT32 src_mask[] = { 0x07FFFFFF, 0x0FFFFFFF, 0x0FFFFFFF, 0x0FFFFFFF };
				static const UINT32 dst_mask[] = { 0x07FFFFFF, 0x07FFFFFF, 0x07FFFFFF, 0x0FFFFFFF };
				gba->dma[i].source_addr &= src_mask[i];
				gba->dma[i].dest_addr   &= dst_mask[i];
			}
			const static INT32 dir_lookup[4] = { 1, -1, 0, 1 };
			INT32 src_dir = dir_lookup[src_addr_ctl];
			INT32 dst_dir = dir_lookup[dst_addr_ctl];

			UINT32 src = gba->dma[i].source_addr;
			UINT32 dst = gba->dma[i].dest_addr;
			UINT32 cnt = gba->dma[i].latched_count;	// keep the whole transfer on the latched count

			// ROM ignores direction and always increments
			if (src >= 0x08000000 && src < 0x0e000000)
				src_dir = 1;
			if (dst >= 0x08000000 && dst < 0x0e000000)
				dst_dir = 1;

			// EEPROM DMA transfers
			if (i == 3 && gba->cart.backup_type == GBA_BACKUP_NONE && (dst & 0xff000000) == 0x0d000000) {
				// Detected EEPROM savegame
				gba->cart.backup_type = GBA_BACKUP_EEPROM;
			}
			if (i == 3 && gba->cart.backup_type == GBA_BACKUP_EEPROM) {
				INT32 src_in_eeprom = (src & 0x1ffffff) >= gba->cart.rom_size || (src & 0x1ffffff) >= 0x01ffff00 || (src & 0xff000000) == 0x0d000000;
				INT32 dst_in_eeprom = (dst & 0x1ffffff) >= gba->cart.rom_size || (dst & 0x1ffffff) >= 0x01ffff00 || (dst & 0xff000000) == 0x0d000000;
				src_in_eeprom &= src >= 0x8000000 && src <= 0xdffffff;
				dst_in_eeprom &= dst >= 0x8000000 && dst <= 0xdffffff;
				skip_dma = src_in_eeprom || dst_in_eeprom;
				if (dst_in_eeprom) {
					if (cnt == 73) {
						// Write data 6 bit address
						UINT32 addr = gba_read_eeprom_bitstream(gba, src, 2,      6, type ? 4 : 2, src_dir);
						UINT64 data = gba_read_eeprom_bitstream(gba, src, 2 + 6, 64, type ? 4 : 2, src_dir);
						((UINT64*)gba->mem.cart_backup)[addr] = data;
						gba->cart.backup_is_dirty = true;
					} else if (cnt == 81) {
						// Write data 14 bit address
						UINT32 addr = gba_read_eeprom_bitstream(gba, src, 2,      14, type ? 4 : 2, src_dir) & 0x3ff;
						UINT64 data = gba_read_eeprom_bitstream(gba, src, 2 + 14, 64, type ? 4 : 2, src_dir);
						((UINT64*)gba->mem.cart_backup)[addr] = data;
						gba->cart.backup_is_dirty = true;
					} else if (cnt ==  9) {
						// read request: 2b "11" + 6b addr(MSB first) + 1b "0"
						gba->mem.eeprom_addr = gba_read_eeprom_bitstream(gba, src, 2,  6, type ? 4 : 2, src_dir);
						gba->cart.eeprom_read_bits_remaining = 68;
					} else if (cnt == 17) {
						// read request: 2b "11" + 14b addr(MSB first) + 1b "0"
						gba->mem.eeprom_addr = gba_read_eeprom_bitstream(gba, src, 2, 14, type ? 4 : 2, src_dir) & 0x3ff;
						gba->cart.eeprom_read_bits_remaining = 68;
					} else {
						printf("Bad cnt: %d for eeprom write\n", cnt);
					}
					gba->dma[i].current_transaction = cnt;
				}
				if (src_in_eeprom) {
					// One EEPROM bit per transfer: 4 dummy zeros, then 64 data bits, then ready=1
					for (INT32 x = 0; x < cnt; ++x) {
						UINT32 bit = 1;
						if (gba->cart.eeprom_read_bits_remaining > 0) {
							--gba->cart.eeprom_read_bits_remaining;
							bit = 0;
							if (gba->cart.eeprom_read_bits_remaining < 64) {
								INT32 step = 63 - gba->cart.eeprom_read_bits_remaining;
								UINT32 byte_addr = gba->mem.eeprom_addr * 8 + 7 - (step >> 3);
								bit = (gba->mem.cart_backup[byte_addr] >> (0x7 - (step & 0x7))) & 1;
							}
						}
						gba_store16(gba, dst + x * transfer_bytes * dst_dir, bit);
					}
					gba->dma[i].current_transaction = cnt;
				}
			}
			bool audio_dma = (mode == 3) && (i == 1 || i == 2);
			if (audio_dma) {
				INT32 fifo = i - 1;
				dst &= ~3;
				src &= ~3;
				for (INT32 x = 0;x < 4;++x) {
					UINT32 src_addr = src + x * 4 * src_dir;
					UINT32 data     = gba_read32(gba, src_addr);
					gba_audio_fifo_push(gba, fifo, SB_BFE(data,  0, 8));
					gba_audio_fifo_push(gba, fifo, SB_BFE(data,  8, 8));
					gba_audio_fifo_push(gba, fifo, SB_BFE(data, 16, 8));
					gba_audio_fifo_push(gba, fifo, SB_BFE(data, 24, 8));
					ticks += gba_compute_access_cycles_dma(gba, src_addr, x != 0 ? 2 : 3);
					ticks += gba_compute_access_cycles_dma(gba, dst, x != 0 || force_first_write_sequential ? 2 : 3);
				}
				dst_addr_ctl   = 2;
				transfer_bytes = 4;
				cnt            = 4;
				skip_dma       = true;
				gba->dma[i].current_transaction = cnt;
			} else if (!skip_dma) {
				if (gba->dma[i].current_transaction < cnt) {
					INT32 x        = gba->dma[i].current_transaction++;
					INT32 dst_addr = dst + x * transfer_bytes * dst_dir;
					INT32 src_addr = src + x * transfer_bytes * src_dir;
					if (type) {
						if (src_addr >= 0x02000000) {
							gba->dma[i].latched_transfer = gba_read32(gba, src_addr);
							ticks += gba_compute_access_cycles_dma(gba, src_addr, x != 0 ? 2 : 3);
						}
						gba_dma_write32(gba, dst_addr, gba->dma[i].latched_transfer);
						ticks += gba_compute_access_cycles_dma(gba, dst_addr, x != 0 || force_first_write_sequential ? 2 : 3);
					} else {
						INT32 v = 0;
						if (src_addr >= 0x02000000) {
							v = gba->dma[i].latched_transfer = (gba_read16(gba, src_addr)) & 0xffff;
							gba->dma[i].latched_transfer |= gba->dma[i].latched_transfer << 16;
							ticks += gba_compute_access_cycles_dma(gba, src_addr, x != 0 ? 0 : 1);
						} else
							v = gba->dma[i].latched_transfer >> (((dst_addr) & 0x3) * 8);
						gba_dma_write16(gba, dst_addr, v & 0xffff);
						ticks += gba_compute_access_cycles_dma(gba, dst_addr, x != 0 || force_first_write_sequential ? 0 : 1);
					}
				}
			}
		
			if (gba->dma[i].current_transaction >= cnt) {
				if (dst_addr_ctl == 0 || dst_addr_ctl == 3)
					dst += cnt * transfer_bytes;
				else if (dst_addr_ctl == 1)
					dst -= cnt * transfer_bytes;
				if (src_addr_ctl == 0)
					src += cnt * transfer_bytes;
				else if (src_addr_ctl == 1)
					src -= cnt * transfer_bytes;

				gba->dma[i].source_addr = src;
				gba->dma[i].dest_addr   = dst;

				if (irq_enable) {
					UINT16 if_bit = 1 << (GBA_INT_DMA0 + i);
					gba_send_interrupt(gba, 4, if_bit);
				}
				if (!dma_repeat || mode == 0) {
					cnt_h &= 0x7fff;
					enable = false;
					gba_io_store16(gba, GBA_DMA0CNT_H + 12 * i, cnt_h);
				} else {
					gba->dma[i].current_transaction = 0;
					gba->dma[i].latched_count = gba_dma_latch_count(gba, i);
				}
			}
		}
		gba->dma[i].last_enable = enable;
		if (ticks)
			break;
	}
	gba->activate_dmas |= ticks != 0;
 
	if (gba->last_transaction_dma && ticks == 0) {
		ticks += 2;
		gba->last_transaction_dma = false;
	}

	return ticks;
}

#endif
