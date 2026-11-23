#pragma once

#ifndef GBA_PPU_H
#define GBA_PPU_H

#include "gba.h"

#define GBA_LCD_LINE_CYCLES		1232
#define GBA_LCD_HBLANK_START_CYCLE	1006
#define GBA_LCD_HBLANK_DURATION_CYCLES	226
#define GBA_LCD_HBLANK_END_CYCLE	(GBA_LCD_HBLANK_START_CYCLE + GBA_LCD_HBLANK_DURATION_CYCLES)
#define GBA_LCD_VBLANK_START		(GBA_LCD_H * GBA_LCD_LINE_CYCLES)

// 4bpp/8bpp tile pixel decoders for the snapshot (scanline-render) path.
// Return palette_id (pal_bank applied for 4bpp), or -1 for a transparent
// pixel (index 0) or the 64KB VRAM boundary quirk.
static inline INT32 gba_ppu_scan_decode_tile4(const UINT8* vram, INT32 char_base, INT32 tile_id, INT32 palette, INT32 px, INT32 py)
{
	INT32 addr = char_base + tile_id * 8 * 4 + px / 2 + py * 4;
	UINT8 tile_d = vram[addr];
	tile_d = (tile_d >> ((px & 1) * 4)) & 0xf;
	if (tile_d == 0 || SB_UNLIKELY(addr >= 0x10000))
		return -1;
	return (INT32)tile_d + palette * 16;
}
static inline INT32 gba_ppu_scan_decode_tile8(const UINT8* vram, INT32 char_base, INT32 tile_id, INT32 px, INT32 py)
{
	INT32 addr = char_base + tile_id * 8 * 8 + px + py * 8;
	UINT8 tile_d = vram[addr];
	if (tile_d == 0 || SB_UNLIKELY(addr >= 0x10000))
		return -1;
	return (INT32)tile_d;
}

// Max cycles to skip from the current beam position.
// Line layout (cycles 0..1231):
//   0..959      active pixel render (lcd_x 0..239)
//   960..1005   PPU tail, VRAM/OAM access blocked
//   1006..1231  HBlank: flag/IRQ/DMA at 1006, next-line writes visible
//   1232        next line start (line_clock wraps to 0)
static inline INT32 gba_ppu_compute_max_fast_forward(gba_t* gba, bool render)
{
	INT32 line_clock = gba->ppu.scan_clock % GBA_LCD_LINE_CYCLES;
	// inside HBlank: skip to its end
	if (line_clock >= GBA_LCD_HBLANK_START_CYCLE && line_clock < GBA_LCD_HBLANK_END_CYCLE)
		return GBA_LCD_HBLANK_END_CYCLE - line_clock - 1;
	// non-rendering (VBlank, non-visible lines, or scanline-ref/worker mode):
	// jump to HBlank start
	bool not_visible = !render || gba->ppu.scan_clock >= GBA_LCD_VBLANK_START;
	if (not_visible && line_clock < GBA_LCD_HBLANK_START_CYCLE)
		return GBA_LCD_HBLANK_START_CYCLE - line_clock - 1;
	// per-pixel visible line: advance to the next 4-cycle dot boundary; the
	// pixel tail (960..1005) jumps to HBlank entry, with no render or IO
	// side effect before cycle 1006.
	const INT32 active_pixel_end = GBA_LCD_W * 4;  // 960
	if (line_clock >= active_pixel_end)
		return GBA_LCD_HBLANK_START_CYCLE - line_clock - 1;
	INT32 to_next_dot = 3 - (gba->ppu.scan_clock & 3);
	if (line_clock + to_next_dot > active_pixel_end)
		to_next_dot = active_pixel_end - line_clock - 1;
	return to_next_dot;
}

// Renders the OBJ layer and builds the window mask for one scanline
static inline void gba_ppu_render_objs(gba_t* gba, INT32 sprite_lcd_y)
{
	UINT16 dispcnt         = gba_io_read16(gba, GBA_DISPCNT);
	INT32  bg_mode         = SB_BFE(dispcnt, 0, 3);
	INT32  obj_vram_map_2d = !SB_BFE(dispcnt, 6, 1);

	UINT16 mos_reg = gba_io_read16(gba, GBA_MOSAIC);
	INT32  mos_y   = SB_BFE(mos_reg, 12, 4) + 1;
	INT32  mos_x_obj = SB_BFE(mos_reg,  8, 4) + 1;
	// SkyEmu issue 316: mosaic counter fix
	if (++gba->ppu.mosaic_y_counter >= mos_y || sprite_lcd_y == 0) {
		gba->ppu.mosaic_y_counter = 0;
	}
	UINT8 default_window_control = 0x3f;	//bitfield [0-3:bg0-bg3 enable 4:obj enable, 5: special effect enable]
	bool winout_enable = SB_BFE(dispcnt, 13, 3) != 0;
	UINT16 WINOUT      = gba_io_read16(gba, GBA_WINOUT);
	if (winout_enable)
		default_window_control = SB_BFE(WINOUT, 0, 8);

	for (INT32 x = 0; x < 240; ++x) {
		gba->window[x] = default_window_control;
	}
	// Seed compositing targets with backdrop so stale data from the prior line
	// cannot bleed through when windows are disabled.
	{
		UINT32 bd_col = (BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.palette + GBA_BG_PALETTE))) | (5u << 17);
		for (INT32 x = 0; x < 240; ++x) {
			gba->first_target_buffer[x]  = bd_col;
			gba->second_target_buffer[x] = bd_col;
		}
	}
	UINT8 obj_window_control = default_window_control;
	bool  obj_window_enable  = SB_BFE(dispcnt, 15, 1);
	if (obj_window_enable)
		obj_window_control   = SB_BFE(WINOUT, 8, 6);
	bool  display_obj        = SB_BFE(dispcnt, 12, 1);
	if (display_obj) {
		INT32 sprite_cycles  = SB_BFE(dispcnt, 5, 1) ? 954 : 1210;
		INT32 last_obj = 0;
		for (INT32 o = 0; o < 128; ++o) {
			UINT16 attr0 = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.oam + o * 8 + 0));
			//Attr0
			UINT8 y_coord     = SB_BFE(attr0, 0, 8);
			bool  rot_scale   = SB_BFE(attr0, 8, 1);
			bool  double_size = SB_BFE(attr0, 9, 1) && rot_scale;
			bool  obj_disable = SB_BFE(attr0, 9, 1) && !rot_scale;

			// OAM scan gap: 2 cycles per skipped slot between adjacent OBJs.
			sprite_cycles -= 2 * (o - last_obj);
			last_obj = o;
			if (sprite_cycles <= 0)
				break;
			if (obj_disable)
				continue;

			INT32  obj_mode           = SB_BFE(attr0, 10, 2);	//(0=Normal, 1=Semi-transparent, 2=OBJ Window, 3=Prohibited)
			bool   mosaic             = SB_BFE(attr0, 12, 1);
			bool   colors_or_palettes = SB_BFE(attr0, 13, 1);
			INT32  obj_shape          = SB_BFE(attr0, 14, 2);	//(0=Square,1=Horizontal,2=Vertical,3=Prohibited)
			UINT16 attr1 = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.oam + o * 8 + 2));

			INT32 rotscale_param = SB_BFE(attr1,  9, 5);
			bool  h_flip         = SB_BFE(attr1, 12, 1) && !rot_scale;
			bool  v_flip         = SB_BFE(attr1, 13, 1) && !rot_scale;
			INT32 obj_size       = SB_BFE(attr1, 14, 2);
			// Size  Square   Horizontal  Vertical
			// 0     8x8      16x8        8x16
			// 1     16x16    32x8        8x32
			// 2     32x32    32x16       16x32
			// 3     64x64    64x32       32x64
			const INT32 xsize_lookup[16] = {
				 8,16, 8, 0,
				16,32, 8, 0,
				32,32,16, 0,
				64,64,32, 0
			};
			const INT32 ysize_lookup[16] = {
				 8, 8,16, 0,
				16, 8,32, 0,
				32,16,32, 0,
				64,32,64, 0
			};

			INT32 y_size = ysize_lookup[obj_size * 4 + obj_shape];
			INT32 x_size = xsize_lookup[obj_size * 4 + obj_shape];

			// Y+height > 256 wraps the OBJ up by 256.
			INT32 y_size_eff = y_size * (double_size ? 2 : 1);
			INT32 x_draw_w   = x_size * (double_size ? 2 : 1);
			INT32 y_wrap     = (y_coord + y_size_eff > 256) ? (y_coord - 256) : y_coord;
			INT32 y_vis      = sprite_lcd_y - y_wrap;
			if (y_vis >= 0 && y_vis < y_size_eff) {
				INT16 x_coord = SB_BFE(attr1, 0, 9);
				if (SB_BFE(x_coord, 8, 1))
					x_coord |= 0xfe00;
				// Fully off-screen horizontally (left or right): skip pixel loop
				// and charge zero draw cycles; x_draw_w already accounts for
				// rot/scale double-size, so this correctly culls all affined sprites.
				INT32 x_end_vis = x_coord + x_draw_w;
				if (x_end_vis <= 0 || x_coord >= GBA_LCD_W)
					continue;

				// Draw cost: rot/scale pays 8+2*full_width; normal pays width-2.
				// A left overhang reduces the cost; a fully-offscreen OBJ is skipped above.
				if (rot_scale) {
					INT32 obj_cycles = 8 + x_draw_w * 2;
					if (x_coord < 0)
						obj_cycles += x_coord;
					sprite_cycles -= obj_cycles;
				} else {
					INT32 obj_cycles = x_size - 2;
					if (x_coord < 0)
						obj_cycles += x_coord >> 1;
					sprite_cycles -= obj_cycles;
				}
				INT32 x_clip_lo = x_coord < 0 ? 0 : (x_coord >= GBA_LCD_W ? GBA_LCD_W : x_coord);
				INT32 x_clip_hi = x_end_vis > GBA_LCD_W ? GBA_LCD_W : x_end_vis;
				//Attr2
				//Skip objects disabled by window
				UINT16 attr2 = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.oam + o * 8 + 4));
				INT32  tile_base = SB_BFE(attr2,  0, 10);
				// Always place sprites as the highest priority
				INT32  priority  = SB_BFE(attr2, 10,  2);
				INT32  palette   = SB_BFE(attr2, 12,  4);
				for (INT32 x = x_clip_lo; x < x_clip_hi; ++x) {
					INT32 sx = (x - x_coord);
					INT32 sy = (sprite_lcd_y - y_coord) & 0xff;
					if (mosaic) {
						sx = ((x / mos_x_obj) * mos_x_obj - x_coord);
						if (sx < 0)
							sx = 0;
						sy = (sprite_lcd_y - y_coord) & 0xff;
						sy -= gba->ppu.mosaic_y_counter;
						if (sy < 0) {
							sy = 0;
						}
					}
					if (rot_scale) {
						UINT32 param_base = rotscale_param * 0x20;
						INT32 a = (INT16)BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.oam + param_base + 0x6));
						INT32 b = (INT16)BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.oam + param_base + 0xe));
						INT32 c = (INT16)BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.oam + param_base + 0x16));
						INT32 d = (INT16)BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.oam + param_base + 0x1e));

						INT64 x1 = sx << 8;
						INT64 y1 = sy << 8;
						INT64 objref_x = (x_size << (double_size ? 8 : 7));
						INT64 objref_y = (y_size << (double_size ? 8 : 7));

						INT64 x2 = a * (x1 - objref_x) + b * (y1 - objref_y) + (x_size << 15);
						INT64 y2 = c * (x1 - objref_x) + d * (y1 - objref_y) + (y_size << 15);

						sx = (x2 >> 16);
						sy = (y2 >> 16);
						if (sx >= x_size || sy >= y_size || sx < 0 || sy < 0)
							continue;
					} else {
						if (h_flip)
							sx = x_size - sx - 1;
						if (v_flip)
							sy = y_size - sy - 1;
					}
					INT32 tx = sx % 8;
					INT32 ty = sy % 8;

					INT32 tile_step = colors_or_palettes ? 2 : 1;
					INT32 tile;
					if (obj_vram_map_2d) {
						INT32 base = colors_or_palettes ? tile_base & ~1 : tile_base;
						tile  = (base + (sx / 8) * tile_step) & 0x1f;
						tile |= (base + (sy / 8) * 32       ) & 0x3e0;
					} else {
						tile  = (tile_base + (sx / 8) * tile_step + (sy / 8) * (x_size / 8) * tile_step) & 0x3ff;
					}
					//Tiles >511 are not rendered in bg_mode3-5 since that memory is used to store the bitmap graphics.
					if (tile < 512 && bg_mode >= 3 && bg_mode <= 5)
						continue;
					UINT8 palette_id;
					INT32 obj_tile_base = GBA_OBJ_TILES0_2;
					bool transparent = false;
					if (colors_or_palettes == false) {
						INT32 offset = (tile * 32 + tx / 2 + ty * 4) & 0x7fff;
						palette_id   = gba->mem.vram[obj_tile_base + offset];
						palette_id   = (palette_id >> ((tx & 1) * 4)) & 0xf;
						transparent  =  palette_id == 0;
						palette_id  +=  palette * 16;
					} else {
						INT32 offset = (tile * 32 + tx + ty * 8) & 0x7fff;
						palette_id   = gba->mem.vram[obj_tile_base + offset];
						transparent  = palette_id == 0;
					}

					UINT32 col = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.palette + GBA_OBJ_PALETTE + palette_id * 2));
					//Handle window objects(not displayed but control the windowing of other things)
					if (obj_mode == 2 && !transparent) {
						gba->window[x] = obj_window_control;
					} else if (obj_mode != 3) {
						INT32 type = 4;
						col = col | (type << 17) | ((5 - priority) << 28) | ((0x7) << 25);
						if (obj_mode == 1)
							col |= 1 << 16;
						if ((col >> 17) > (gba->first_target_buffer[x] >> 17)) {
							if (transparent) {
								//Update priority for transparent pixels (needed for golden sun)
								if (SB_BFE(gba->first_target_buffer[x], 17, 3) != 5)
									gba->first_target_buffer[x] = (gba->first_target_buffer[x] & (0x0fffffff)) | (col & 0xf0000000);
							} else gba->first_target_buffer[x] = col;
						}
					}
				}
			}
		}
	}
	INT32 enabled_windows = SB_BFE(dispcnt, 13, 3);		// [0: win0, 1:win1, 2: objwin]
	if (enabled_windows) {
		UINT16 winin = gba_io_read16(gba, GBA_WININ);
		for (INT32 win = 1; win >= 0; --win) {
			bool win_enable = SB_BFE(dispcnt, 13 + win, 1);
			if (!win_enable)
				continue;
			UINT16 WINH = gba_io_read16(gba, GBA_WIN0H + 2 * win);
			UINT16 WINV = gba_io_read16(gba, GBA_WIN0V + 2 * win);
			INT32  win_xmin = SB_BFE(WINH, 8, 8);
			INT32  win_xmax = SB_BFE(WINH, 0, 8);
			INT32  win_ymin = SB_BFE(WINV, 8, 8);
			INT32  win_ymax = SB_BFE(WINV, 0, 8);
			// An X1>X2 range wraps and covers [0,X2) plus [X1,240); X2>240
			// clamps to 240 and a larger X1 collapses to 0. Y uses the same rule.
			if (win_xmin > 240 && win_xmin > win_xmax)
				win_xmin = 0;
			if (win_xmax > 240) {
				win_xmax = 240;
				if (win_xmin > 240)
					win_xmin = 240;
			}
			bool y_in_window = (win_ymin > win_ymax)
				? (sprite_lcd_y < win_ymax || sprite_lcd_y >= win_ymin)
				: (sprite_lcd_y >= win_ymin && sprite_lcd_y < win_ymax);
			if (!y_in_window)
				continue;
			UINT8  win_value = SB_BFE(winin, win * 8, 6);
			if (win_xmin > win_xmax) {
				for (INT32 x = 0; x < win_xmax; ++x)
					gba->window[x] = win_value;
				for (INT32 x = win_xmin; x < 240; ++x)
					gba->window[x] = win_value;
			} else {
				for (INT32 x = win_xmin; x < win_xmax; ++x)
					gba->window[x] = win_value;
			}
		}
		INT32  backdrop_type = 5;
		UINT32 backdrop_col  = (BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.palette + GBA_BG_PALETTE + 0 * 2))) | (backdrop_type << 17);
		for (INT32 x = 0; x < 240; ++x) {
			UINT8 window_control = gba->window[x];
			if (SB_BFE(window_control, 4, 1) == 0)
				gba->first_target_buffer[x] = backdrop_col;
		}
	}
}

// Samples and composites one pixel; live register reads give snapshot semantics
static inline void gba_ppu_render_pixel(gba_t* gba, INT32 lcd_x, INT32 lcd_y)
{
	UINT16 dispcnt = gba_io_read16(gba, GBA_DISPCNT);
	INT32  bg_mode      = SB_BFE(dispcnt, 0, 3);
//	INT32  obj_vram_map_2d = !SB_BFE(dispcnt, 6, 1);
	INT32  forced_blank = SB_BFE(dispcnt, 7, 1);
	UINT8  window_control = gba->window[lcd_x];
	if (bg_mode == 6 || bg_mode == 7) {
		//Palette 0 is taken as the background
	} else if (bg_mode <= 5) {
		for (INT32 bg = 3; bg >= 0; --bg) {
			UINT32 col = 0;
			if ((bg < 2 && bg_mode == 2) || (bg == 3 && bg_mode == 1) || (bg != 2 && bg_mode >= 3))
				continue;
			bool bg_en = SB_BFE(dispcnt, 8 + bg, 1);
			if (!bg_en || SB_BFE(window_control, bg, 1) == 0)
				continue;

			bool   rot_scale = bg_mode >= 1 && bg >= 2;
			UINT16 bgcnt = gba_io_read16(gba, GBA_BG0CNT + bg * 2);
			INT32  priority         = SB_BFE(bgcnt,  0, 2);
			INT32  character_base   = SB_BFE(bgcnt,  2, 2);
			bool   mosaic           = SB_BFE(bgcnt,  6, 1);
			bool   colors           = SB_BFE(bgcnt,  7, 1);
			INT32  screen_base      = SB_BFE(bgcnt,  8, 5);
			bool   display_overflow = SB_BFE(bgcnt, 13, 1);
			INT32  screen_size      = SB_BFE(bgcnt, 14, 2);

			INT32 screen_size_x = (screen_size  & 1) ? 512 : 256;
			INT32 screen_size_y = (screen_size >= 2) ? 512 : 256;

			INT32 bg_x = 0;
			INT32 bg_y = 0;

			if (rot_scale) {
				screen_size_x = screen_size_y = (16 * 8) << screen_size;
				if (bg_mode == 3 || bg_mode == 4) {
					screen_size_x = 240;
					screen_size_y = 160;
				} else if (bg_mode == 5) {
					screen_size_x = 160;
					screen_size_y = 128;
				}
				colors = true;

				INT32 bgx = gba->ppu.aff[bg - 2].render_bgx;
				INT32 bgy = gba->ppu.aff[bg - 2].render_bgy;

				INT32 a = (INT16)gba_io_read16(gba, GBA_BG2PA + (bg - 2) * 0x10);
				INT32 c = (INT16)gba_io_read16(gba, GBA_BG2PC + (bg - 2) * 0x10);

				// Shift lcd_coords into fixed point
				INT64 x2 = a * lcd_x + (((INT64)bgx));
				INT64 y2 = c * lcd_x + (((INT64)bgy));
				if (mosaic) {
					INT16 mos_reg = gba_io_read16(gba, GBA_MOSAIC);
					INT32 mos_x   = SB_BFE(mos_reg, 0, 4) + 1;
					x2 = a * ((lcd_x / mos_x) * mos_x) + (((INT64)bgx));
					y2 = c * ((lcd_x / mos_x) * mos_x) + (((INT64)bgy));
				}


				bg_x = (x2 >> 8);
				bg_y = (y2 >> 8);

				// Bitmap modes (3/4/5) treat out-of-bounds as transparent;
				// affine tile BGs wrap only when overflow bit is set.
				if (bg_x < 0 || bg_x >= screen_size_x || bg_y < 0 || bg_y >= screen_size_y) {
					if (display_overflow == 0 || bg_mode >= 3)
						continue;
					bg_x &= screen_size_x - 1;
					bg_y &= screen_size_y - 1;
				}
			} else {
				INT16 hoff = gba_io_read16(gba, GBA_BG0HOFS + bg * 4);
				INT16 voff = gba_io_read16(gba, GBA_BG0VOFS + bg * 4);
				hoff = (hoff << 7) >> 7;
				voff = (voff << 7) >> 7;
				bg_x = (hoff + lcd_x);
				bg_y = (voff + lcd_y);
				if (mosaic) {
					UINT16 mos_reg = gba_io_read16(gba, GBA_MOSAIC);
					INT32  mos_x = SB_BFE(mos_reg, 0, 4) + 1;
					INT32  mos_y = SB_BFE(mos_reg, 4, 4) + 1;
					bg_x = hoff + (lcd_x / mos_x) * mos_x;
					bg_y = voff + (lcd_y / mos_y) * mos_y;
				}
			}
			if (bg_mode == 3) {
				INT32 p    = bg_x + bg_y * 240;
				INT32 addr = p * 2;
				col = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.vram + addr));
			} else if (bg_mode == 4) {
				INT32 p          = bg_x + bg_y * 240;
				INT32 frame_sel  = SB_BFE(dispcnt, 4, 1);
				INT32 addr       = p * 1 + 0xA000 * frame_sel;
				UINT8 palette_id = gba->mem.vram[addr];
				if (palette_id == 0)
					continue;
				col = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.palette + GBA_BG_PALETTE + palette_id * 2));
			} else if (bg_mode == 5) {
				INT32 p         = bg_x + bg_y * 160;
				INT32 frame_sel = SB_BFE(dispcnt, 4, 1);
				INT32 addr      = p * 2 + 0xA000 * frame_sel;
				col = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.vram + addr));
			} else {
				bg_x = bg_x & (screen_size_x - 1);
				bg_y = bg_y & (screen_size_y - 1);
				INT32 bg_tile_x = bg_x / 8;
				INT32 bg_tile_y = bg_y / 8;

				INT32 tile_off  = bg_tile_y * (screen_size_x / 8) + bg_tile_x;

				INT32 screen_base_addr    = screen_base * 2048;
				INT32 character_base_addr = character_base * 16 * 1024;

				UINT16 tile_data = 0;

				INT32 px = bg_x % 8;
				INT32 py = bg_y % 8;

				if (rot_scale)tile_data = gba->mem.vram[screen_base_addr + tile_off];
				else {
					INT32 tile_off2 = (bg_tile_y % 32) * 32 + (bg_tile_x % 32);
					if (bg_tile_x >= 32)
						tile_off2 += 32 * 32;
					if (bg_tile_y >= 32)
						tile_off2 += 32 * 32 * (screen_size == 3 ? 2 : 1);
					tile_data = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.vram + screen_base_addr + tile_off2 * 2));

					INT32 h_flip = SB_BFE(tile_data, 10, 1);
					INT32 v_flip = SB_BFE(tile_data, 11, 1);
					if (h_flip)
						px = 7 - px;
					if (v_flip)
						py = 7 - py;
				}
				INT32 tile_id = SB_BFE(tile_data,  0, 10);
				INT32 palette = SB_BFE(tile_data, 12,  4);

				UINT8 tile_d = tile_id;
				if (colors == false) {
					INT32 addr = character_base_addr + tile_id * 8 * 4 + px / 2 + py * 4;
					tile_d = gba->mem.vram[addr];
					tile_d = (tile_d >> ((px & 1) * 4)) & 0xf;
					//There is an undocumented GBA quirk where tiles over 64KB are not loaded
					if (tile_d == 0 || SB_UNLIKELY(addr >= 0x10000))
						continue;
					tile_d += palette * 16;
				} else {
					//There is an undocumented GBA quirk where tiles over 64KB are not loaded
					INT32 addr = character_base_addr + tile_id * 8 * 8 + px + py * 8;
					tile_d = gba->mem.vram[addr];
					if (tile_d == 0 || SB_UNLIKELY(addr >= 0x10000))
						continue;
				}
				UINT8 palette_id = tile_d;
				col = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.palette + GBA_BG_PALETTE + palette_id * 2));
			}
			col |= (bg << 17) | ((5 - priority) << 28) | ((4 - bg) << 25);
			if (col > gba->first_target_buffer[lcd_x]) {
				UINT32 t = gba->first_target_buffer[lcd_x];
				gba->first_target_buffer[lcd_x] = col;
				col = t;
			}
			if (col > gba->second_target_buffer[lcd_x])
				gba->second_target_buffer[lcd_x] = col;
		}
	}
	// Backdrop (palette[0]) is read live per-dot on real hardware; refresh
	// any slot still at type==5 so mid-line palette writes take effect.
	{
		UINT32 live_bd = (BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.palette + GBA_BG_PALETTE))) | (5u << 17);
		if (SB_BFE(gba->first_target_buffer[lcd_x], 17, 3) == 5)
			gba->first_target_buffer[lcd_x]  = live_bd;
		if (SB_BFE(gba->second_target_buffer[lcd_x], 17, 3) == 5)
			gba->second_target_buffer[lcd_x] = live_bd;
	}
	UINT32 col  = gba->first_target_buffer[lcd_x];
	INT32  r    = SB_BFE(col,  0, 5);
	INT32  g    = SB_BFE(col,  5, 5);
	INT32  b    = SB_BFE(col, 10, 5);
	UINT32 type = SB_BFE(col, 17, 3);

	bool   effect_enable = SB_BFE(window_control, 5, 1);
	UINT16 bldcnt        = gba_io_read16(gba, GBA_BLDCNT);
	INT32  mode          = SB_BFE(bldcnt        , 6, 2);

	//Semitransparent objects are always selected for blending
	if (SB_BFE(col, 16, 1)) {
		UINT32 col2  = gba->second_target_buffer[lcd_x];
		UINT32 type2 = SB_BFE(col2,   17,         3);
		bool   blend = SB_BFE(bldcnt,  8 + type2, 1);
		if (blend) {
			mode          = 1;
			effect_enable = true;
		} else effect_enable &= SB_BFE(bldcnt, type, 1);
	} else effect_enable &= SB_BFE(bldcnt, type, 1);
	if (effect_enable) {
		UINT16 bldy = gba_io_read16(gba, GBA_BLDY);
		float  evy  = SB_BFE(bldy, 0, 5) / 16.;
		if (evy > 1.0)
			evy = 1;
		switch (mode) {
			case 0:
				break;	//None
			case 1: {
				UINT32 col2  = gba->second_target_buffer[lcd_x];
				UINT32 type2 = SB_BFE(col2,   17,         3);
				bool  blend  = SB_BFE(bldcnt,  8 + type2, 1);
				if (blend) {
					UINT16 bldalpha = gba_io_read16(gba, GBA_BLDALPHA);
					INT32  r2  = SB_BFE(col2,     0, 5);
					INT32  g2  = SB_BFE(col2,     5, 5);
					INT32  b2  = SB_BFE(col2,    10, 5);
					INT32  eva = SB_BFE(bldalpha, 0, 5);
					INT32  evb = SB_BFE(bldalpha, 8, 5);
					if (eva > 16) eva = 16;
					if (evb > 16) evb = 16;
					r = (r * eva + r2 * evb) / 16;
					g = (g * eva + g2 * evb) / 16;
					b = (b * eva + b2 * evb) / 16;
					if (r > 31) r = 31;
					if (g > 31) g = 31;
					if (b > 31) b = 31;
				}
			}
				break;	//Alpha Blend
			case 2:		//Lighten
				r = r + (31 - r) * evy;
				g = g + (31 - g) * evy;
				b = b + (31 - b) * evy;
				break;
			case 3:		//Darken
				r = r - (r     ) * evy;
				g = g - (g     ) * evy;
				b = b - (b     ) * evy;
				break;
		}
	}
	if (forced_blank) {
		r = g = b = 31;
		if (gba->stop_mode)
			r = g = b = 0;
	}

	INT32  backdrop_type = 5;
	UINT32 backdrop_col  = (BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.palette + GBA_BG_PALETTE + 0 * 2))) | (backdrop_type << 17);
	gba->first_target_buffer[ lcd_x] = backdrop_col;
	gba->second_target_buffer[lcd_x] = backdrop_col;

	INT32  p = (lcd_x + lcd_y * 240) * 4;
	float  screen_blend_factor = 0.3 * gba->ppu.ghosting_strength;
	UINT16 green_swap = gba_io_read16(gba, GBA_GREENSWP);
	INT32  gp = (green_swap & 1) ? ((p & 4) ? (p + 1 - 4) : (p + 1 + 4)) : (p + 1);
	if (screen_blend_factor == 0.0f) {
		// ghosting off -> the blend reduces to a plain copy
		gba->framebuffer[p + 0] = ((r << 3) | (r >> 2));
		gba->framebuffer[p + 2] = ((b << 3) | (b >> 2));
		gba->framebuffer[gp]    = ((g << 3) | (g >> 2));
	} else {
		gba->framebuffer[p + 0] = ((r << 3) | (r >> 2)) * (1.0 - screen_blend_factor) + gba->framebuffer[p + 0] * screen_blend_factor;
		gba->framebuffer[p + 2] = ((b << 3) | (b >> 2)) * (1.0 - screen_blend_factor) + gba->framebuffer[p + 2] * screen_blend_factor;
		gba->framebuffer[gp]    = ((g << 3) | (g >> 2)) * (1.0 - screen_blend_factor) + gba->framebuffer[gp] * screen_blend_factor;
	}
}

// Scanline render: registers snapshotted at hblank start; pixel loop performs no IO reads
static inline void gba_ppu_render_scanline(gba_t* gba, INT32 lcd_y)
{
	UINT16 dispcnt      = gba_io_read16(gba, GBA_DISPCNT);
	INT32  bg_mode      = SB_BFE(dispcnt, 0, 3);
	INT32  forced_blank = SB_BFE(dispcnt, 7, 1);
	INT32  frame_sel    = SB_BFE(dispcnt, 4, 1);
	UINT16 mos_reg      = gba_io_read16(gba, GBA_MOSAIC);
	INT32  mos_x        = SB_BFE(mos_reg, 0, 4) + 1;
	INT32  mos_y        = SB_BFE(mos_reg, 4, 4) + 1;
	UINT16 bldcnt       = gba_io_read16(gba, GBA_BLDCNT);
	INT32  bld_mode     = SB_BFE(bldcnt, 6, 2);
	UINT16 bldy_reg     = gba_io_read16(gba, GBA_BLDY);
	UINT16 bldalpha     = gba_io_read16(gba, GBA_BLDALPHA);
	UINT16 green_swap   = gba_io_read16(gba, GBA_GREENSWP);
	UINT32 backdrop_col = (BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.palette + GBA_BG_PALETTE + 0 * 2))) | (5 << 17);
	float  sbf          = 0.3 * gba->ppu.ghosting_strength;
	float  evy          = SB_BFE(bldy_reg, 0, 5) / 16.;
	if (evy > 1.0)
		evy = 1;

	bool render_bgs  = bg_mode <= 5;
	bool mode_ok[4]  = { false, false, false, false };
	INT32 priority[4], char_addr[4], scr_addr[4], size_x[4], size_y[4], ssize[4];
	INT32 hoff[4], voff[4], bgx[4], bgy[4], pa[4], pc[4];
	bool colors[4], mosaic_bg[4], rot_scale[4], overflow[4];
	// text BG row constants + per-tile-column cache: tilemap read once per tile,
	// all 8 pixels of the tile row decoded in a single pass
	INT32 py0[4], trow_base[4], cached_tile_x[4];
	INT32 cached_tile_pixels[4][8];
	for (INT32 bg = 0; bg < 4; ++bg)
		cached_tile_x[bg] = -1;
	if (render_bgs) {
		for (INT32 bg = 0; bg < 4; ++bg) {
			if ((bg < 2 && bg_mode == 2) || (bg == 3 && bg_mode == 1) || (bg != 2 && bg_mode >= 3))
				continue;
			if (!SB_BFE(dispcnt, 8 + bg, 1))
				continue;
			mode_ok[bg]    = true;
			rot_scale[bg]  = bg_mode >= 1 && bg >= 2;
			UINT16 bgcnt   = gba_io_read16(gba, GBA_BG0CNT + bg * 2);
			priority[bg]   = SB_BFE(bgcnt,  0, 2);
			char_addr[bg]  = SB_BFE(bgcnt,  2, 2) * 16 * 1024;
			mosaic_bg[bg]  = SB_BFE(bgcnt,  6, 1);
			colors[bg]     = SB_BFE(bgcnt,  7, 1);
			scr_addr[bg]   = SB_BFE(bgcnt,  8, 5) * 2048;
			overflow[bg]   = SB_BFE(bgcnt, 13, 1);
			ssize[bg]      = SB_BFE(bgcnt, 14, 2);
			size_x[bg]     = (ssize[bg] & 1 ) ? 512 : 256;
			size_y[bg]     = (ssize[bg] >= 2) ? 512 : 256;
			if (rot_scale[bg]) {
				size_x[bg] = size_y[bg] = (16 * 8) << ssize[bg];
				if (bg_mode == 3 || bg_mode == 4) {
					size_x[bg] = 240;
					size_y[bg] = 160;
				} else if (bg_mode == 5) {
					size_x[bg] = 160;
					size_y[bg] = 128;
				}
				colors[bg] = true;
				bgx[bg] = gba->ppu.aff[bg - 2].render_bgx;
				bgy[bg] = gba->ppu.aff[bg - 2].render_bgy;
				pa[bg] = (INT16)gba_io_read16(gba, GBA_BG2PA + (bg - 2) * 0x10);
				pc[bg] = (INT16)gba_io_read16(gba, GBA_BG2PC + (bg - 2) * 0x10);
			} else {
				INT16 h16 = gba_io_read16(gba, GBA_BG0HOFS + bg * 4);
				INT16 v16 = gba_io_read16(gba, GBA_BG0VOFS + bg * 4);
				hoff[bg] = (h16 << 7) >> 7;
				voff[bg] = (v16 << 7) >> 7;
				INT32 ly     = mosaic_bg[bg] ? (lcd_y / mos_y) * mos_y : lcd_y;
				INT32 row_y  = (voff[bg] + ly) & (size_y[bg] - 1);
				py0[bg]      = row_y  & 7;
				INT32 ty     = row_y >> 3;
				trow_base[bg] = (ty & 31) * 32 + (ty >= 32 ? 32 * 32 * (ssize[bg] == 3 ? 2 : 1) : 0);
			}
		}
	}

	for (INT32 lcd_x = 0; lcd_x < 240; ++lcd_x) {
		UINT8 window_control = gba->window[lcd_x];
		if (render_bgs) {
			for (INT32 bg = 3; bg >= 0; --bg) {
				if (!mode_ok[bg] || SB_BFE(window_control, bg, 1) == 0)
					continue;
				UINT32 col  = 0;
				INT32  bg_x = 0;
				INT32  bg_y = 0;
				if (rot_scale[bg]) {
					INT32 sx = mosaic_bg[bg] ? (lcd_x / mos_x) * mos_x : lcd_x;
					INT64 x2 = (INT64)pa[bg] * sx + (((INT64)bgx[bg]));
					INT64 y2 = (INT64)pc[bg] * sx + (((INT64)bgy[bg]));
					bg_x = (INT32)(x2 >> 8);
					bg_y = (INT32)(y2 >> 8);
					// Bitmap modes treat out-of-bounds as transparent;
					// affine tile BGs wrap when overflow bit is set.
					if (bg_x < 0 || bg_x >= size_x[bg] || bg_y < 0 || bg_y >= size_y[bg]) {
						if (overflow[bg] == 0 || bg_mode >= 3)
							continue;
						bg_x &= size_x[bg] - 1;
						bg_y &= size_y[bg] - 1;
					}
				} else {
					if (mosaic_bg[bg]) {
						bg_x = hoff[bg] + (lcd_x / mos_x) * mos_x;
						bg_y = voff[bg] + (lcd_y / mos_y) * mos_y;
					} else {
						bg_x = hoff[bg] + lcd_x;
						bg_y = voff[bg] + lcd_y;
					}
				}
				if (bg_mode == 3) {
					INT32 p    = bg_x + bg_y * 240;
					col = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.vram + p * 2));
				} else if (bg_mode == 4) {
					INT32 p    = bg_x + bg_y * 240;
					INT32 addr = p + 0xa000 * frame_sel;
					UINT8 palette_id = gba->mem.vram[addr];
					if (palette_id == 0)
						continue;
					col = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.palette + GBA_BG_PALETTE + palette_id * 2));
				} else if (bg_mode == 5) {
					INT32 p    = bg_x + bg_y * 160;
					INT32 addr = p * 2 + 0xa000 * frame_sel;
					col = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.vram + addr));
				} else {
					bg_x = bg_x & (size_x[bg] - 1);
					INT32 bg_tile_x = bg_x >> 3;
					INT32 palette_id;
					if (rot_scale[bg]) {
						INT32 tile_off = (bg_y >> 3) * (size_x[bg] >> 3) + bg_tile_x;
						UINT16 tile_data = gba->mem.vram[scr_addr[bg] + tile_off];
						INT32 tile_id = SB_BFE(tile_data,  0, 10);
						INT32 palette = SB_BFE(tile_data, 12,  4);
						palette_id = colors[bg]
							? gba_ppu_scan_decode_tile8(gba->mem.vram, char_addr[bg], tile_id, bg_x & 7, bg_y & 7)
							: gba_ppu_scan_decode_tile4(gba->mem.vram, char_addr[bg], tile_id, palette, bg_x & 7, bg_y & 7);
					} else {
						if (bg_tile_x != cached_tile_x[bg]) {
							cached_tile_x[bg] = bg_tile_x;
							INT32 toff = trow_base[bg] + (bg_tile_x & 31) + (bg_tile_x >= 32 ? 32 * 32 : 0);
							UINT16 tile_data = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.vram + scr_addr[bg] + toff * 2));
							INT32 tile_id = SB_BFE(tile_data,  0, 10);
							INT32 palette = SB_BFE(tile_data, 12,  4);
							INT32 py = SB_BFE(tile_data, 11, 1) ? 7 - py0[bg] : py0[bg];
							bool  hflip = SB_BFE(tile_data, 10, 1);
							for (INT32 tx = 0; tx < 8; ++tx) {
								INT32 px = hflip ? 7 - tx : tx;
								cached_tile_pixels[bg][tx] = colors[bg]
									? gba_ppu_scan_decode_tile8(gba->mem.vram, char_addr[bg], tile_id, px, py)
									: gba_ppu_scan_decode_tile4(gba->mem.vram, char_addr[bg], tile_id, palette, px, py);
							}
						}
						palette_id = cached_tile_pixels[bg][bg_x & 7];
					}
					if (palette_id < 0)
						continue;
					col = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(gba->mem.palette + GBA_BG_PALETTE + palette_id * 2));
				}
				col |= (bg << 17) | ((5 - priority[bg]) << 28) | ((4 - bg) << 25);
				if (col > gba->first_target_buffer[lcd_x]) {
					UINT32 t = gba->first_target_buffer[lcd_x];
					gba->first_target_buffer[lcd_x] = col;
					col = t;
				}
				if (col > gba->second_target_buffer[lcd_x])
					gba->second_target_buffer[lcd_x] = col;
			}
		}

		UINT32 col  = gba->first_target_buffer[lcd_x];
		INT32  r    = SB_BFE(col,  0, 5);
		INT32  g    = SB_BFE(col,  5, 5);
		INT32  b    = SB_BFE(col, 10, 5);
		UINT32 type = SB_BFE(col, 17, 3);

		INT32 mode = bld_mode;
		bool  effect_enable = SB_BFE(window_control, 5, 1);

		//Semitransparent objects are always selected for blending
		if (SB_BFE(col, 16, 1)) {
			UINT32 col2  = gba->second_target_buffer[lcd_x];
			UINT32 type2 = SB_BFE(col2,   17,         3);
			bool   blend = SB_BFE(bldcnt,  8 + type2, 1);
			if (blend) {
				mode          = 1;
				effect_enable = true;
			} else effect_enable &= SB_BFE(bldcnt, type, 1);
		} else effect_enable &= SB_BFE(bldcnt, type, 1);
		if (effect_enable) {
			switch (mode) {
				case 0:
					break;	//None
				case 1: {
					UINT32 col2  = gba->second_target_buffer[lcd_x];
					UINT32 type2 = SB_BFE(col2,  17,         3);
					bool  blend  = SB_BFE(bldcnt, 8 + type2, 1);
					if (blend) {
						INT32 r2  = SB_BFE(col2,     0, 5);
						INT32 g2  = SB_BFE(col2,     5, 5);
						INT32 b2  = SB_BFE(col2,    10, 5);
						INT32 eva = SB_BFE(bldalpha, 0, 5);
						INT32 evb = SB_BFE(bldalpha, 8, 5);
						if (eva > 16) eva = 16;
						if (evb > 16) evb = 16;
						r = (r * eva + r2 * evb) / 16;
						g = (g * eva + g2 * evb) / 16;
						b = (b * eva + b2 * evb) / 16;
						if (r > 31) r = 31;
						if (g > 31) g = 31;
						if (b > 31) b = 31;
					}
				}
					break;	//Alpha Blend
				case 2:		//Lighten
					r = r + (31 - r) * evy;
					g = g + (31 - g) * evy;
					b = b + (31 - b) * evy;
					break;
				case 3:		//Darken
					r = r - (r     ) * evy;
					g = g - (g     ) * evy;
					b = b - (b     ) * evy;
					break;
			}
		}
		if (forced_blank) {
			r = g = b = 31;
			if (gba->stop_mode)
				r = g = b = 0;
		}

		gba->first_target_buffer[ lcd_x] = backdrop_col;
		gba->second_target_buffer[lcd_x] = backdrop_col;

		INT32 p = (lcd_x + lcd_y * 240) * 4;
		INT32 gp = (green_swap & 1) ? ((p & 4) ? (p + 1 - 4) : (p + 1 + 4)) : (p + 1);
		if (sbf == 0.0f) {
			// ghosting off -> the blend reduces to a plain copy
			gba->framebuffer[p + 0] = ((r << 3) | (r >> 2));
			gba->framebuffer[p + 2] = ((b << 3) | (b >> 2));
			gba->framebuffer[gp]    = ((g << 3) | (g >> 2));
		} else {
			gba->framebuffer[p + 0] = ((r << 3) | (r >> 2)) * (1.0 - sbf) + gba->framebuffer[p + 0] * sbf;
			gba->framebuffer[p + 2] = ((b << 3) | (b >> 2)) * (1.0 - sbf) + gba->framebuffer[p + 2] * sbf;
			gba->framebuffer[gp]    = ((g << 3) | (g >> 2)) * (1.0 - sbf) + gba->framebuffer[gp] * sbf;
		}
	}
}

// On-read refresh of DISPSTAT/VCOUNT from the current beam position; internal reads are not routed here
static inline void gba_ppu_refresh_status(gba_t* gba)
{
	if (!gba->ppu_event.active)
		return;
	INT32 remaining = (INT32)(gba->ppu_event.when - gba->global_timer);
	if (remaining < 0)
		remaining = 0;
	INT32 beam = (INT32)gba->ppu.scan_clock - remaining;
	if (beam < 0)
		beam += 280896;
	INT32 lcd_y      = beam / GBA_LCD_LINE_CYCLES;
	INT32 line_clock = beam % GBA_LCD_LINE_CYCLES;
	INT32 vcount     = lcd_y % 228;
	bool  vblank     = lcd_y >= GBA_LCD_H && lcd_y < 228;
	bool  hblank     = line_clock >= GBA_LCD_HBLANK_START_CYCLE && line_clock < GBA_LCD_HBLANK_END_CYCLE;
	UINT16 disp_stat = gba_io_read16(gba, GBA_DISPSTAT) & ~0x7;
	disp_stat |= vblank ? 0x1 : 0;
	disp_stat |= hblank ? 0x2 : 0;
	disp_stat |= vcount == SB_BFE(disp_stat, 8, 8) ? 0x4 : 0;
	gba_io_store16(gba, GBA_DISPSTAT, disp_stat);
	gba_io_store16(gba, GBA_VCOUNT,   vcount);
}

static inline void gba_ppu_event(gba_t* gba, sb_emu_state_t* emu, UINT32 cycles_late)
{
	bool render = emu->render_frame;
	if (gba->ppu.scan_clock >= 280896)
		gba->ppu.scan_clock -= 280896;
	INT32 lcd_y      = gba->ppu.scan_clock / GBA_LCD_LINE_CYCLES;
	INT32 line_clock = gba->ppu.scan_clock % GBA_LCD_LINE_CYCLES;
	gba->ppu.scan_clock++;
	INT32 fast_forward_ticks = gba_ppu_compute_max_fast_forward(gba, render && gba->ppu.render_per_pixel) + 1;
	gba->ppu.scan_clock += fast_forward_ticks;

	// Three event boundaries per line (all cycle-exact):
	//   0    : line start  - DISPCNT pipeline advance, affine reload, scanline render (non-per-pixel)
	//   1006 : HBlank start- HBlank flag set, IRQ fires, DMA wakes, HBlank snapshot
	// HBlank end (1232) coincides with line start of the next row and is handled there.
	bool at_line_start  = (line_clock == 0);
	bool at_hblank_start = (line_clock == GBA_LCD_HBLANK_START_CYCLE);

	if (at_line_start || at_hblank_start) {
		UINT16 disp_stat  = gba_io_read16(gba, GBA_DISPSTAT) & ~0x7;
		UINT16 vcount_cmp = SB_BFE(disp_stat, 8, 8);
		// VCount advances at HBlank exit (cycle 1232 = next line start); our events only
		// fire at 0 or 1006, so VCount always equals lcd_y here.
		INT32  vcount = lcd_y % 228;
		bool   vblank = lcd_y >= GBA_LCD_H && lcd_y < 228;
		bool   hblank = line_clock >= GBA_LCD_HBLANK_START_CYCLE && line_clock < GBA_LCD_HBLANK_END_CYCLE;
		disp_stat |= vblank ? 0x1 : 0;
		disp_stat |= hblank ? 0x2 : 0;
		disp_stat |= vcount == vcount_cmp ? 0x4 : 0;
		gba_io_store16(gba, GBA_DISPSTAT, disp_stat);
		gba_io_store16(gba, GBA_VCOUNT  , vcount);
		UINT32 new_if = 0;
		if (hblank != gba->ppu.last_hblank) {
			gba->ppu.last_hblank = hblank;
			bool hblank_irq_en   = SB_BFE(disp_stat, 4, 1);
			if (hblank && hblank_irq_en)
				new_if |= (1 << GBA_INT_LCD_HBLANK);
			if (hblank) {
				// DMA wake 2 cycles after the edge (hardware startup delay)
				++gba->ppu.hblank_seq;
				gba_timing_schedule(gba, &gba->dma_event, 2);
			}
			if (!hblank) {
				gba->ppu.dispcnt_pipeline[0] = gba->ppu.dispcnt_pipeline[1];
				gba->ppu.dispcnt_pipeline[1] = gba->ppu.dispcnt_pipeline[2];
				gba->ppu.dispcnt_pipeline[2] = gba_io_read16(gba, GBA_DISPCNT);
			}
		}
		if (lcd_y != gba->ppu.last_lcd_y) {
			if (vblank != gba->ppu.last_vblank) {
				if (vblank) {
					gba->frame_in_progress = false;
					++gba->ppu.vblank_seq;
					gba_timing_schedule(gba, &gba->dma_event, 2);
				}
				gba->ppu.last_vblank = vblank;
				bool vblank_irq_en = SB_BFE(disp_stat, 3, 1);
				if (vblank && vblank_irq_en)
					new_if |= (1 << GBA_INT_LCD_VBLANK);
			}
			gba->ppu.last_lcd_y = lcd_y;
		}
		// VCount match is a level flag; the IRQ fires only on 0->1 edge.
		bool vcount_match_cur = (vcount == (INT32)vcount_cmp);
		if (vcount_match_cur && !gba->ppu.last_vcount_match) {
			bool vcnt_irq_en = SB_BFE(disp_stat, 5, 1);
			if (vcnt_irq_en)
				new_if |= (1 << GBA_INT_LCD_VCOUNT);
		}
		gba->ppu.last_vcount_match = vcount_match_cur;
		gba_send_interrupt(gba, 3, new_if);
	}

	if (!render)
		return;

	if (at_line_start) {
		// Affine reference updated at scanline start so the per-line increment uses
		// the CURRENT line's PB/PD (sampled ~cycle 6)
		UINT16 dispcnt = gba->ppu.dispcnt_pipeline[0];
		INT32  bg_mode = SB_BFE(dispcnt, 0, 3);
		// Line 0 skips the increment and reloads from BG2X/BG2Y
		if (bg_mode != 0 && lcd_y != 0) {
			for (INT32 aff = 0; aff < 2; ++aff) {
				bool bg_en = SB_BFE(dispcnt, 8 + aff + 2, 1);
				if (!bg_en)
					continue;
				INT32  b = (INT16)gba_io_read16(gba, GBA_BG2PB  + (aff) * 0x10);
				INT32  d = (INT16)gba_io_read16(gba, GBA_BG2PD  + (aff) * 0x10);
				UINT16 bgcnt = gba_io_read16(gba, GBA_BG2CNT +  aff  * 2);
				bool mosaic = SB_BFE(bgcnt, 6, 1);
				if (mosaic) {
					UINT16 mos_reg = gba_io_read16(gba, GBA_MOSAIC);
					INT32  mos_y   = SB_BFE(mos_reg, 4, 4) + 1;
					if ((lcd_y % mos_y) == 0) {
						gba->ppu.aff[aff].render_bgx += b * mos_y;
						gba->ppu.aff[aff].render_bgy += d * mos_y;
					}
				} else {
					gba->ppu.aff[aff].render_bgx += b;
					gba->ppu.aff[aff].render_bgy += d;
				}
			}
		}
		// Reload from BG2X/BG2Y when written this scanline, or unconditionally on line 0
		for (INT32 aff = 0; aff < 2; ++aff) {
			if (gba->ppu.aff[aff].wrote_bgx || lcd_y == 0) {
				gba->ppu.aff[aff].render_bgx = gba_io_read32(gba, GBA_BG2X + (aff) * 0x10);
				gba->ppu.aff[aff].render_bgx = SB_BFE(gba->ppu.aff[aff].render_bgx, 0, 28);
				gba->ppu.aff[aff].render_bgx = ((INT32)(gba->ppu.aff[aff].render_bgx << 4)) >> 4;
				gba->ppu.aff[aff].wrote_bgx  = false;
			}
			if (gba->ppu.aff[aff].wrote_bgy || lcd_y == 0) {
				gba->ppu.aff[aff].render_bgy = gba_io_read32(gba, GBA_BG2Y + (aff) * 0x10);
				gba->ppu.aff[aff].render_bgy = SB_BFE(gba->ppu.aff[aff].render_bgy, 0, 28);
				gba->ppu.aff[aff].render_bgy = ((INT32)(gba->ppu.aff[aff].render_bgy << 4)) >> 4;
				gba->ppu.aff[aff].wrote_bgy  = false;
			}
		}
		// Render the scanline at line-start using line-start register state.
		if (!gba->ppu.render_per_pixel && lcd_y < GBA_LCD_H) {
			gba_ppu_render_objs(gba, lcd_y);
			gba_ppu_render_scanline(gba, lcd_y);
		}
	}
	if (gba->ppu.render_per_pixel) {
		// OBJ pre-decode for the next line runs at HBlank entry (cycle 1006)
		if ((lcd_y < GBA_LCD_H - 1 || lcd_y == 227) && at_hblank_start)
			gba_ppu_render_objs(gba, (lcd_y + 1) % 228);
		// lcd_x = line_clock / 4; render only during active pixel phase (cycles 0..959, lcd_x 0..239)
		INT32 lcd_x = line_clock / 4;
		if (lcd_x < GBA_LCD_W && lcd_y < GBA_LCD_H)
			gba_ppu_render_pixel(gba, lcd_x, lcd_y);
	}

	// next boundary: keep the absolute scanline grid regardless of dispatch lateness
	gba_timing_deschedule(gba, &gba->ppu_event);
	gba_timing_schedule(gba, &gba->ppu_event, fast_forward_ticks + 1 - (INT32)cycles_late);
}

#endif
