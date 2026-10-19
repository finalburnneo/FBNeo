// see d_taitof3.cpp for credits
// renderer based on MAME taito_f3_v.cpp (2024 rewrite by ywy and 12Me21)

#include "tiles_generic.h"
#include "taitof3_video.h"
#include "taito.h"

UINT32 sprite_lag;
UINT32 extended_layers;

UINT8 *TaitoF3CtrlRAM;
UINT8 *TaitoF3PfRAM;
UINT8 *TaitoF3LineRAM;

void (*pPaletteUpdateCallback)(UINT16) = NULL;
UINT8 TaitoF3PalRecalc;

static UINT16 *pal16 = NULL; // fast palette for 16bpp video emulation

UINT16 *bitmap_layer[10];
UINT8 *bitmap_flags[10];
UINT32 *output_bitmap;

UINT8 *dirty_tiles;
UINT8 dirty_tile_count[10];

#define H_TOTAL				432
#define H_VIS				320
#define H_START				46

#define NUM_PLAYFIELDS		4
#define NUM_SPRITEGROUPS	4
#define NUM_CLIPPLANES		4
#define MAX_CLIP_RANGES		16

#define FB_WIDTH			512
#define FB_HEIGHT			256

typedef INT32 fixed8;

static INT32 flipscreen = 0;
static INT32 last_flipscreen = -1;
static INT32 last_pixel_yoffs = -1;

static UINT8 sprite_extra_planes = 0;
static UINT8 sprite_pen_mask = 0;
static UINT8 sprite_trails = 0;
static UINT8 sprite_bank = 0;

static UINT16 *sprite_framebuffer[NUM_SPRITEGROUPS];
static UINT8 *sprite_pri_bitmap;
static UINT8 sprite_pri_row_usage[256];

static INT32 clip_min_x = H_START;
static INT32 clip_max_x = H_START + H_VIS - 1;
static INT32 clip_min_y = 24;
static INT32 clip_max_y = 255;

struct f3_tempsprite
{
	INT32 code;
	UINT8 color;
	UINT8 flip_x, flip_y;
	fixed8 x, y;
	fixed8 scale_x, scale_y;
	UINT8 pri;
	INT32 rampos;
};

static f3_tempsprite *m_spritelist;
static const f3_tempsprite *m_sprite_end;

struct clip_plane_inf
{
	INT16 l;
	INT16 r;
};

struct mix_pix
{
	UINT16 src_pal;
	UINT16 dst_pal;
	UINT8 src_blend;
	UINT8 dst_blend;
};

struct pri_mode
{
	UINT8 src_prio;
	UINT8 dst_prio;
	UINT8 src_blendmode;
};

enum { LAYER_PIVOT = 0, LAYER_SPRITE, LAYER_PF };

struct f3_layer_inf
{
	// common mixing info: BAEI cccc iiii pppp
	UINT16 mix_value;
	UINT8 prio;
	UINT8 x_sample_enable;
	UINT8 blend_select_v;	// sprite groups & pivot

	// playfield
	UINT16 colscroll;		// 4000
	UINT8 alt_tilemap;		// 4000
	fixed8 x_scale;			// 8000
	fixed8 y_scale;			// 8000
	UINT16 pal_add;			// 9000
	fixed8 rowscroll;		// a000
	fixed8 reg_sx, reg_sy;
	fixed8 reg_fx_x, reg_fx_y;
	UINT16 width_mask;

	// pivot
	UINT8 pivot_control;	// 6000
	UINT16 pivot_enable;	// 7000
	UINT16 pv_sx, pv_sy;

	// draw source for the current line
	UINT16 *src_bitmap;
	UINT8 *flags_bitmap;
	INT32 src_pitch;
	UINT8 index;

	inline void set_mix(UINT16 v) { mix_value = v; prio = v & 0xf; }
	inline void set_prio(UINT8 p) { mix_value = (mix_value & 0xfff0) | p; prio = p; }
	inline UINT8 blend_mask() const { return (mix_value >> 14) & 3; }
	inline bool use_pix() const { return (pivot_control & 0xa0) != 0; }
};

struct f3_line_inf
{
	INT32 y;
	clip_plane_inf clip[NUM_CLIPPLANES];	// 4000 / 5000
	UINT8 blend[4];							// 6200
	UINT8 x_sample;							// 6400 (mosaic)
	UINT8 fx_6400;							// 6400 (palette interpretation, unemulated)
	UINT16 bg_palette;						// 6600
	f3_layer_inf pivot;
	f3_layer_inf sp[NUM_SPRITEGROUPS];
	f3_layer_inf pf[NUM_PLAYFIELDS];
};

void TaitoF3VideoReset()
{
	flipscreen = 0;
	last_flipscreen = -1;
	last_pixel_yoffs = -1;

	sprite_extra_planes = 0;
	sprite_pen_mask = 0;
	sprite_trails = 0;
	sprite_bank = 0;

	for (INT32 i = 0; i < NUM_SPRITEGROUPS; i++) {
		memset(sprite_framebuffer[i], 0, FB_WIDTH * FB_HEIGHT * sizeof(UINT16));
	}
	memset(sprite_pri_bitmap, 0, FB_WIDTH * FB_HEIGHT);
	memset(sprite_pri_row_usage, 0, sizeof(sprite_pri_row_usage));

	m_sprite_end = m_spritelist;
}

/*============================================================================*/

static void draw_pf_layer(INT32 layer)
{
	INT32 offset = (layer * (0x1000 << extended_layers));

	UINT16 *ram = (UINT16*)(TaitoF3PfRAM + offset);

	INT32 width = extended_layers ? 1024 : 512;
	INT32 wide = width / 16;

	if (extended_layers) {
		if (dirty_tile_count[layer*2+0] == 0 && dirty_tile_count[layer*2+1] == 0) {
			return;
		}
		dirty_tile_count[layer*2+0] = dirty_tile_count[layer*2+1] = 0;
	} else {
		if (dirty_tile_count[layer] == 0) {
			return;
		}
		dirty_tile_count[layer] = 0;
	}

	for (INT32 offs = 0; offs < wide * 32; offs++)
	{
		if (dirty_tiles[((offs * 4) + offset) / 4] == 0) continue;
		dirty_tiles[((offs * 4) + offset) / 4] = 0;

		INT32 sx = (offs % wide) * 16;
		INT32 sy = (offs / wide) * 16;

		UINT16 tile = BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 0]);
		UINT16 code = BURN_ENDIAN_SWAP_INT16(ram[offs * 2 + 1]) % TaitoCharModulo;

		UINT8 category = (tile >> 9) & 1; // blend value select

		INT32 explane = (tile >> 10) & 3;
		INT32 flipx   = (tile >> 14) & 1;
		INT32 flipy   = (tile >> 15) & 1;
		INT32 color   = (tile & 0x1ff) << 4;

		// extra planes and palette code share bits of the color address (or'ed)
		INT32 penmask = (explane << 4) | 0x0f;

		if (flipscreen) {
			sx = (width - 16) - sx;
			sy = (512 - 16) - sy;
			flipy ^= 1;
			flipx ^= 1;
		}

		INT32 flip = (flipy * 0xf0) + (flipx * 0x0f);

		UINT8 *gfx = TaitoChars + (code * 0x100);
		UINT16 *dst = bitmap_layer[layer] + (sy * width) + sx;
		UINT8 *flagptr = bitmap_flags[layer] + (sy * width) + sx;

		for (INT32 y = 0; y < 16; y++, dst += width, flagptr += width)
		{
			for (INT32 x = 0; x < 16; x++)
			{
				INT32 pxl = gfx[((y*16)+x)^flip] & penmask;

				dst[x] = color | pxl;
				flagptr[x] = (pxl == 0) ? category : (category | 0x10);
			}
		}
	}
}

static void draw_vram_layer()
{
	UINT16 *ram = (UINT16*)TaitoVideoRam;

	for (INT32 offs = 0; offs < 64 * 64; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		INT32 tile = BURN_ENDIAN_SWAP_INT16(ram[offs]);

		INT32 code = tile & 0x00ff;
		INT32 flipx = tile & 0x0100;
		INT32 flipy = tile & 0x8000;

		if (flipscreen) {
			sx = (512 - 8) - sx;
			sy = (512 - 8) - sy;
			flipx ^= 0x100;
			flipy ^= 0x8000;
		}

		INT32 color = ((tile >> 9) & 0x3f) * 16;
		UINT8 *gfx = TaitoCharsB + (code * 8 * 8);
		INT32 flip = ((flipy ? 0x38 : 0) | (flipx ? 7 : 0));

		UINT16 *dst = bitmap_layer[8] + (sy * 512) + sx;
		UINT8 *flagptr = bitmap_flags[8] + (sy * 512) + sx;

		for (INT32 y = 0; y < 8; y++, dst += 512, flagptr += 512)
		{
			for (INT32 x = 0; x < 8; x++)
			{
				INT32 pxl = gfx[((y*8)+x)^flip];

				dst[x] = pxl + color;
				flagptr[x] = (pxl == 0) ? 0 : 0x10;
			}
		}
	}
}

static void draw_pixel_layer()
{
	UINT16 y_offs = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(TaitoF3CtrlRAM + 0x1a))) & 0x1ff;
	if (flipscreen) y_offs += 0x100;

	// the palette half used by each row depends on the scroll offset
	if (y_offs != last_pixel_yoffs) {
		last_pixel_yoffs = y_offs;
		dirty_tile_count[9] = 1;
	}

	if (dirty_tile_count[9] == 0) {
		return;
	}
	dirty_tile_count[9] = 0;

	UINT16 *ram = (UINT16*)TaitoVideoRam;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs / 0x20) * 8;
		INT32 sy = (offs & 0x1f) * 8;

		INT32 col_off = ((offs & 0x1f) * 0x40) + ((offs & 0xfe0) >> 5);

		if ((((offs & 0x1f) * 8 + y_offs) & 0x1ff) > 0xff)
			col_off += 0x800;

		INT32 tile = BURN_ENDIAN_SWAP_INT16(ram[col_off]);

		INT32 code = offs;
		INT32 flipx = tile & 0x0100;
		INT32 flipy = tile & 0x8000;
		INT32 color = ((tile >> 9) & 0x3f) * 16;

		if (flipscreen)
		{
			flipx ^= 0x0100;
			flipy ^= 0x8000;
			sy = (256 - 8) - sy;
			sx = (512 - 8) - sx;
		}

		UINT8 *gfx = TaitoCharsPivot + (code * 8 * 8);
		INT32 flip = ((flipy ? 0x38 : 0) | (flipx ? 7 : 0));

		UINT16 *dst = bitmap_layer[9] + (sy * 512) + sx;
		UINT8 *flagptr = bitmap_flags[9] + (sy * 512) + sx;

		for (INT32 y = 0; y < 8; y++, dst += 512, flagptr += 512)
		{
			for (INT32 x = 0; x < 8; x++)
			{
				INT32 pxl = gfx[((y*8)+x)^flip];

				dst[x] = pxl + color;
				flagptr[x] = (pxl == 0) ? 0 : 0x10;
			}
		}
	}
}

/*============================================================================*/

static void f3_drawgfx(const f3_tempsprite *sprite)
{
	UINT16 *dest_bmp = sprite_framebuffer[sprite->pri];
	const UINT8 *code_base = TaitoSpritesA + ((sprite->code % TaitoSpriteAModulo) * 0x100);

	const UINT8 flipx = sprite->flip_x ? 0xf : 0;
	const UINT8 flipy = sprite->flip_y ? 0xf : 0;

	fixed8 dy8 = sprite->y;
	if (!flipscreen)
		dy8 += 255; // round up in non-flipscreen mode

	for (INT32 y = 0; y < 16; y++)
	{
		const INT32 dy = dy8 >> 8;
		dy8 += sprite->scale_y;
		if (dy < clip_min_y || dy > clip_max_y)
			continue;

		UINT8 *pri = sprite_pri_bitmap + dy * FB_WIDTH;
		UINT16 *dest = dest_bmp + dy * FB_WIDTH;
		UINT8 &usage = sprite_pri_row_usage[dy];
		const UINT8 *src = code_base + ((y ^ flipy) * 16);

		fixed8 dx8 = sprite->x + 128;
		for (INT32 x = 0; x < 16; x++)
		{
			const INT32 dx = dx8 >> 8;
			dx8 += sprite->scale_x;
			if (dx < clip_min_x || dx > clip_max_x)
				continue;
			if (dx == (dx8 >> 8)) // next pixel lands in the same column
				continue;

			const UINT8 c = src[x ^ flipx] & sprite_pen_mask;
			if (c && !pri[dx]) {
				dest[dx] = 0x1000 + ((sprite->color << 4) | c);
				pri[dx] = 1;
				usage |= 1 << sprite->pri;
			}
		}
	}
}

struct sprite_axis
{
	fixed8 block_scale;
	fixed8 pos, block_pos;
	INT16 global, subglobal;

	void update(UINT8 scroll, UINT16 posw, bool multi, UINT8 block_ctrl, UINT8 new_zoom)
	{
		INT16 new_pos = (INT16)(posw << 4) >> 4;

		if (scroll & 1) subglobal = new_pos;
		if (scroll & 2) global = new_pos;

		if (~scroll & 8) {
			new_pos += global;
			if (~scroll & 4)
				new_pos += subglobal;
		}

		switch (block_ctrl) {
			case 0:
				if (!multi) {
					block_pos = new_pos * 256;
					block_scale = 0x100 - new_zoom;
				}
				// fall through
			case 2:
				pos = block_pos;
				break;
			case 3:
				pos += block_scale * 16;
				break;
		}
	}
};

static void get_sprite_info(UINT16 *spriteram16_ptr)
{
	sprite_axis x, y;
	x.block_scale = y.block_scale = 1 << 8;
	x.pos = y.pos = x.block_pos = y.block_pos = 0;
	x.global = y.global = x.subglobal = y.subglobal = 0;

	UINT8 color = 0;
	bool multi = false;

	f3_tempsprite *sprite_ptr = m_spritelist;
	INT32 total_sprites = 0;

	for (INT32 offs = 0; offs < 0x400 && (total_sprites < 0x400); offs++)
	{
		total_sprites++; // prevent infinite loops
		const INT32 bank = sprite_bank ? 0x4000 : 0;
		const UINT16 *ram = spriteram16_ptr + bank + (offs * 8);
		UINT16 spr[8];
		for (INT32 i = 0; i < 8; i++) spr[i] = BURN_ENDIAN_SWAP_INT16(ram[i]);

		if (spr[3] & 0x8000) { // special command
			const UINT16 cntrl = spr[5];
			flipscreen = (cntrl >> 13) & 1;

			sprite_extra_planes = (cntrl >> 8) & 3; // 0 = 4bpp, 1 = 5bpp, 2 = unused?, 3 = 6bpp
			sprite_pen_mask = (sprite_extra_planes << 4) | 0x0f;
			sprite_trails = (cntrl >> 1) & 1;

			sprite_bank = cntrl & 1;
		}

		// the jump is checked after the command (recalh uses both in the same sprite)
		if (spr[6] & 0x8000) {
			const INT32 new_offs = spr[6] & 0x3ff;
			if (new_offs == offs)
				break;

			offs = new_offs - 1;
		}

		const UINT8 spritecont = spr[4] >> 8;
		const bool lock = (spritecont >> 2) & 1;
		if (!lock)
			color = spr[4] & 0xff;
		const UINT8 scroll_mode = (spr[2] >> 12) & 0xf;
		const UINT16 zooms = spr[1];
		x.update(scroll_mode, spr[2] & 0xfff, multi, (spritecont >> 6) & 3, zooms & 0xff);
		y.update(scroll_mode, spr[3] & 0xfff, multi, (spritecont >> 4) & 3, zooms >> 8);
		multi = (spritecont >> 3) & 1;

		const INT32 tile = spr[0] | ((spr[5] & 1) << 16);
		if (!tile)
			continue;

		const fixed8 tx = flipscreen ? (512 << 8) - x.block_scale * 16 - x.pos : x.pos;
		const fixed8 ty = flipscreen ? (256 << 8) - y.block_scale * 16 - y.pos : y.pos;

		if (tx + x.block_scale * 16 <= clip_min_x << 8 || tx > clip_max_x << 8 || ty + y.block_scale * 16 <= clip_min_y << 8 || ty > clip_max_y << 8)
			continue;

		const bool flip_x = spritecont & 1;
		const bool flip_y = (spritecont >> 1) & 1;

		sprite_ptr->x = tx;
		sprite_ptr->y = ty;
		sprite_ptr->flip_x = flipscreen ? !flip_x : flip_x;
		sprite_ptr->flip_y = flipscreen ? !flip_y : flip_y;
		sprite_ptr->code = tile;
		sprite_ptr->color = color;
		sprite_ptr->scale_x = x.block_scale;
		sprite_ptr->scale_y = y.block_scale;
		sprite_ptr->pri = (color >> 6) & 3;
		sprite_ptr->rampos = (offs * 8) & 0x1fff;
		sprite_ptr++;
	}

	if (f3_game == GSEEKER && TaitoDip[0] & 1)
	{ // gseeker st.5 boss spriteram overflow corruption fix.
		if (sprite_ptr - m_spritelist > 1) {
			sprite_ptr--;
			INT32 i = 0x1ff8;
			while ((sprite_ptr->rampos == i || sprite_ptr->rampos >= i-0x400) && sprite_ptr != m_spritelist) {
				i -= 8;
				sprite_ptr--;
			}
			if (sprite_ptr != m_spritelist) sprite_ptr++;
		}
	}

	m_sprite_end = sprite_ptr;
}

static void draw_sprites()
{
	if (!sprite_trails) {
		memset(sprite_pri_bitmap, 0, FB_WIDTH * FB_HEIGHT);
		memset(sprite_pri_row_usage, 0, sizeof(sprite_pri_row_usage));
		for (INT32 i = 0; i < NUM_SPRITEGROUPS; i++) {
			memset(sprite_framebuffer[i], 0, FB_WIDTH * FB_HEIGHT * sizeof(UINT16));
		}
	}

	for (const f3_tempsprite *spr = m_sprite_end; spr-- != m_spritelist; ) {
		f3_drawgfx(spr);
	}
}

/*============================================================================*/

// line: latched line info from the previous call, modified in place
// y is called 0->255 for non-flipscreen, 255->0 for flipscreen
static void read_line_ram(f3_line_inf &line, INT32 y)
{
	const UINT16 *line_ram = (const UINT16*)TaitoF3LineRAM;

	#define LRAM(a) BURN_ENDIAN_SWAP_INT16(line_ram[(a)])

	struct latch {
		static INT32 addr(const UINT16 *ram, INT32 y, INT32 section, INT32 subsection) {
			const UINT16 latches = BURN_ENDIAN_SWAP_INT16(ram[(section * 0x200) / 2 + y]);
			const INT32 base = 0x4000 + 0x1000 * section + 0x200 * subsection;
			if ((latches >> (subsection + 4)) & 1)
				return (base + 0x800) / 2 + y;
			else if ((latches >> subsection) & 1)
				return base / 2 + y;
			return 0;
		}
	};

	INT32 where;

	// 4000: column scroll, alt tilemap, clip high bits
	for (INT32 i = 2; i < 4; i++) {
		if ((where = latch::addr(line_ram, y, 0, i))) {
			const UINT16 colscroll = LRAM(where);
			line.pf[i].colscroll   = colscroll & 0x1ff;
			line.pf[i].alt_tilemap = (colscroll & 0x200) ? 1 : 0;

			clip_plane_inf &c0 = line.clip[2*(i-2) + 0];
			clip_plane_inf &c1 = line.clip[2*(i-2) + 1];
			c0.l = (c0.l & 0xff) | (((colscroll >> 12) & 1) << 8);
			c0.r = (c0.r & 0xff) | (((colscroll >> 13) & 1) << 8);
			c1.l = (c1.l & 0xff) | (((colscroll >> 14) & 1) << 8);
			c1.r = (c1.r & 0xff) | (((colscroll >> 15) & 1) << 8);
		}
	}

	// 5000: clip planes low bits
	for (INT32 i = 0; i < 4; i++) {
		if ((where = latch::addr(line_ram, y, 1, i))) {
			const UINT16 clip_lows = LRAM(where);
			line.clip[i].l = (line.clip[i].l & 0x100) | (clip_lows & 0xff);
			line.clip[i].r = (line.clip[i].r & 0x100) | (clip_lows >> 8);
		}
	}

	// 6000: sprite blend modes, pivot control
	if ((where = latch::addr(line_ram, y, 2, 0))) {
		const UINT16 line_6000 = LRAM(where);

		line.pivot.blend_select_v = (line_6000 >> 9) & 1;
		line.pivot.pivot_control = line_6000 >> 8;

		for (INT32 sp_group = 0; sp_group < NUM_SPRITEGROUPS; sp_group++) {
			line.sp[sp_group].mix_value &= 0x3fff;
			line.sp[sp_group].mix_value |= ((line_6000 >> (sp_group * 2)) & 3) << 14;
		}
	}
	if ((where = latch::addr(line_ram, y, 2, 1))) { // blend values
		const UINT16 blend_vals = LRAM(where);
		for (INT32 idx = 0; idx < 4; idx++) {
			const INT32 alpha = (blend_vals >> (4 * idx)) & 0xf;
			line.blend[idx] = (UINT8)((0xf - alpha) * 32 > 255 ? 255 : (0xf - alpha) * 32);
		}
	}
	if ((where = latch::addr(line_ram, y, 2, 2))) { // mosaic, palette depth effects
		const UINT16 x_mosaic = LRAM(where);

		line.x_sample = 16 - ((x_mosaic >> 4) & 0xf);

		for (INT32 pf_num = 0; pf_num < NUM_PLAYFIELDS; pf_num++) {
			line.pf[pf_num].x_sample_enable = (x_mosaic >> pf_num) & 1;
		}
		for (INT32 i = 0; i < NUM_SPRITEGROUPS; i++) {
			line.sp[i].x_sample_enable = (x_mosaic >> 8) & 1;
		}
		line.pivot.x_sample_enable = (x_mosaic >> 9) & 1;

		line.fx_6400 = (x_mosaic & 0xfc00) >> 8;
	}
	if ((where = latch::addr(line_ram, y, 2, 3))) { // background palette
		line.bg_palette = LRAM(where);
	}

	// 7000
	if ((where = latch::addr(line_ram, y, 3, 0))) {
		line.pivot.pivot_enable = LRAM(where);
	}
	if ((where = latch::addr(line_ram, y, 3, 1))) { // pivot layer mix info
		line.pivot.set_mix(LRAM(where));
	}
	if ((where = latch::addr(line_ram, y, 3, 2))) { // sprite clip info, blend select
		const UINT16 sprite_mix = LRAM(where);

		for (INT32 group = 0; group < NUM_SPRITEGROUPS; group++) {
			line.sp[group].mix_value = (line.sp[group].mix_value & 0xc00f) | ((sprite_mix & 0x3ff) << 4);
			line.sp[group].blend_select_v = (sprite_mix >> (12 + group)) & 1;
		}
	}
	if ((where = latch::addr(line_ram, y, 3, 3))) { // sprite priority
		const UINT16 sprite_prio = LRAM(where);
		for (INT32 group = 0; group < NUM_SPRITEGROUPS; group++) {
			line.sp[group].set_prio((sprite_prio >> (group * 4)) & 0xf);
		}
	}

	// 8000: playfield zoom (y zooms of pf2 and pf4 are swapped)
	static const INT32 FIX_Y[4] = { 0, 3, 2, 1 };
	for (INT32 i = 0; i < 4; i++) {
		if ((where = latch::addr(line_ram, y, 4, i))) {
			const UINT16 pf_scale = LRAM(where);
			line.pf[i].x_scale = 256 - (pf_scale >> 8);
			line.pf[FIX_Y[i]].y_scale = (pf_scale & 0xff) << 1;
		}
	}

	// 9000: playfield palette addition
	for (INT32 i = 0; i < 4; i++) {
		if ((where = latch::addr(line_ram, y, 5, i))) {
			line.pf[i].pal_add = LRAM(where) * 16;
		}
	}

	// a000: playfield rowscroll, iiii iiii iiff ffff (fractional part is negative)
	for (INT32 i = 0; i < 4; i++) {
		if ((where = latch::addr(line_ram, y, 6, i))) {
			const fixed8 rowscroll = LRAM(where) << (8-6);
			line.pf[i].rowscroll = (rowscroll & 0xffffff00) - (rowscroll & 0x000000ff);
		}
	}

	// b000: playfield mix info
	for (INT32 i = 0; i < 4; i++) {
		if ((where = latch::addr(line_ram, y, 7, i))) {
			line.pf[i].set_mix(LRAM(where));
		}
	}

	#undef LRAM
}

static void get_pf_scroll(INT32 pf_num, fixed8 &reg_sx, fixed8 &reg_sy)
{
	const UINT16 *control_0 = (const UINT16*)TaitoF3CtrlRAM;

	// x: iiii iiii iiFF FFFF (fixed10.6, fractional bits inverted)
	// y: iiii iiii ifff ffff (fixed9.7)
	INT16 sx_raw = BURN_ENDIAN_SWAP_INT16(control_0[pf_num]);
	INT16 sy_raw = BURN_ENDIAN_SWAP_INT16(control_0[pf_num + 4]);

	sy_raw += (1 << 7);

	if (flipscreen) {
		sx_raw += 320 << 6;
		sx_raw += (512 + 192) << 6;

		sy_raw = -sy_raw;
	}

	sx_raw += (40 - 4*pf_num) << 6;

	fixed8 sx = sx_raw * (1 << (8-6));
	fixed8 sy = sy_raw * (1 << (8-7));
	sx ^= 0xfc;
	sx -= H_START << 8;
	if (flipscreen)
		sy = -sy;

	reg_sx = sx;
	reg_sy = sy;
}

static INT32 calc_clip(clip_plane_inf *ranges, const clip_plane_inf *clip, UINT16 mix_value)
{
	const INT16 INF_L = H_START;
	const INT16 INF_R = H_START + H_VIS;

	const UINT8 clip_inv = (mix_value >> 4) & 0xf;
	const UINT8 clip_enable = (mix_value >> 8) & 0xf;
	UINT8 normal_planes = clip_enable & ~clip_inv & 0xf;
	UINT8 invert_planes = clip_enable & clip_inv;
	if (~mix_value & 0x1000) {
		UINT8 t = normal_planes; normal_planes = invert_planes; invert_planes = t;
	}

	// start with a visible region spanning the entire line
	INT32 count = 1;
	ranges[0].l = INF_L;
	ranges[0].r = INF_R;

	for (INT32 plane = 0; plane < NUM_CLIPPLANES; plane++) {
		const INT16 clip_l = clip[plane].l - 1;
		const INT16 clip_r = clip[plane].r - 2;

		if ((normal_planes >> plane) & 1) {
			INT32 n = 0;
			for (INT32 i = 0; i < count; i++) {
				// a clip less than 1px wide hides the entire line
				if (clip_l > clip_r || ranges[i].r < clip_l || ranges[i].l > clip_r)
					continue;
				clip_plane_inf r = ranges[i];
				if (clip_l > r.l) r.l = clip_l;
				if (clip_r < r.r) r.r = clip_r;
				ranges[n++] = r;
			}
			count = n;
		} else if (((invert_planes >> plane) & 1) && (clip_l <= clip_r)) {
			clip_plane_inf new_ranges[MAX_CLIP_RANGES * 2];
			INT32 new_count = 0;
			for (INT32 i = 0; i < count; i++) { new_ranges[new_count].l = INF_L; new_ranges[new_count].r = clip_l; new_count++; }
			for (INT32 i = 0; i < count; i++) { new_ranges[new_count].l = clip_r; new_ranges[new_count].r = INF_R; new_count++; }

			INT32 n = 0;
			for (INT32 j = 0; j < new_count; j++) {
				clip_plane_inf nr = new_ranges[j];
				bool keep = true;
				for (INT32 i = 0; i < count; i++) {
					if (ranges[i].l > nr.l) nr.l = ranges[i].l;
					if (ranges[i].l > nr.r) nr.r = ranges[i].l;
					if (nr.l >= nr.r) {
						keep = false;
						break;
					}
				}
				if (keep && n < MAX_CLIP_RANGES)
					new_ranges[n++] = nr;
			}

			for (INT32 i = 0; i < n; i++) ranges[i] = new_ranges[i];
			count = n;
		}
	}

	// keep everything inside the line buffer
	for (INT32 i = 0; i < count; i++) {
		if (ranges[i].r > INF_R) ranges[i].r = INF_R;
	}

	return count;
}

static inline INT32 mosaic(INT32 x, UINT8 sample)
{
	INT32 x_count = (x - 46 + 114);
	// hw quirk: the counter resets 2px from the right edge
	x_count = x_count >= 432 ? x_count - 432 : x_count;
	return x - (x_count % sample);
}

template<INT32 KIND>
static inline bool layer_enable(const f3_layer_inf &l)
{
	if (KIND == LAYER_SPRITE)
		return (l.mix_value & 0x2000) && l.blend_mask() != 0;
	return (l.mix_value & 0x2000) && l.blend_mask() != 3;
}

template<INT32 KIND>
static void mix_line(const f3_layer_inf *gfx, mix_pix *z, pri_mode *pri, const f3_line_inf &line, const clip_plane_inf &range)
{
	INT32 y;
	if (KIND == LAYER_PF)
		y = ((gfx->reg_fx_y >> 8) + gfx->colscroll) & 0x1ff;
	else if (KIND == LAYER_PIVOT)
		y = (gfx->pv_sy + line.y) & (gfx->use_pix() ? 0xff : 0x1ff);
	else
		y = line.y;

	const UINT16 *src = gfx->src_bitmap + y * gfx->src_pitch;
	const UINT8 *flags = (KIND != LAYER_SPRITE) ? (gfx->flags_bitmap + y * gfx->src_pitch) : NULL;

	const UINT8 blend_mask = gfx->blend_mask();
	const UINT8 prio = gfx->prio;

	for (INT32 x = range.l; x < range.r; x++) {
		if (blend_mask == pri[x].src_blendmode)
			continue; // layers cannot blend against the same blend mode

		const INT32 real_x = gfx->x_sample_enable ? mosaic(x, line.x_sample) : x;

		INT32 gfx_x;
		if (KIND == LAYER_PF)
			gfx_x = (((gfx->reg_fx_x + (real_x - H_START) * gfx->x_scale) >> 8) + H_START) & gfx->width_mask;
		else if (KIND == LAYER_PIVOT)
			gfx_x = (real_x + gfx->pv_sx) & 0x1ff;
		else
			gfx_x = real_x;

		if (KIND != LAYER_SPRITE && !(flags[gfx_x] & 0xf0))
			continue;

		UINT16 pal = src[gfx_x];
		if (KIND == LAYER_PF)
			pal = (UINT16)(pal + gfx->pal_add);
		if (!pal)
			continue;

		const UINT8 blend_sel = (KIND == LAYER_PF) ? (flags[gfx_x] & 1) : gfx->blend_select_v;

		if (prio > pri[x].src_prio) {
			// submit source pixel
			UINT8 sel = blend_sel;

			switch (blend_mask) {
				case 1: // normal blend
					sel = 2 + sel;
					// fall through
				case 2: // reverse blend
					if (line.blend[sel] == 0)
						continue;
					z[x].src_blend = line.blend[sel];
					break;
				default: // opaque layer
					if (line.blend[sel] + line.blend[2 + sel] == 0)
						continue;
					z[x].src_blend = line.blend[2 + sel];
					z[x].dst_blend = line.blend[sel];
					pri[x].dst_prio = prio;
					z[x].dst_pal = pal;
					break;
			}

			// lock in source color for blending and update the prio test buffer
			z[x].src_pal = pal;
			pri[x].src_blendmode = blend_mask;
			pri[x].src_prio = prio;
		} else if (prio >= pri[x].dst_prio) {
			// submit destination pixel
			if (prio != pri[x].dst_prio)
				z[x].dst_pal = pal;
			else // prio conflict (dariusg, bubblem)
				z[x].dst_pal = 0;
			pri[x].dst_prio = prio;

			if (pri[x].src_blendmode == 1)
				z[x].dst_blend = line.blend[blend_sel];
			else
				z[x].dst_blend = line.blend[2 + blend_sel];
		}
	}
}

template<INT32 KIND>
static void draw_layer(const f3_layer_inf *gfx, mix_pix *z, pri_mode *pri, const f3_line_inf &line)
{
	if (!layer_enable<KIND>(*gfx))
		return;
	if (KIND == LAYER_SPRITE && !(sprite_pri_row_usage[line.y] & (1 << gfx->index)))
		return;

	clip_plane_inf ranges[MAX_CLIP_RANGES];
	const INT32 count = calc_clip(ranges, line.clip, gfx->mix_value);
	for (INT32 i = 0; i < count; i++) {
		mix_line<KIND>(gfx, z, pri, line, ranges[i]);
	}
}

static void render_line(UINT32 *dst, const mix_pix *z)
{
	for (INT32 x = H_START; x < H_START + H_VIS; x++) {
		const mix_pix &mix = z[x];
		const UINT32 s = BURN_ENDIAN_SWAP_INT32(TaitoPalette[mix.src_pal & 0x1fff]);
		const UINT32 d = BURN_ENDIAN_SWAP_INT32(TaitoPalette[mix.dst_pal & 0x1fff]);

		INT32 r = ((((s >> 16) & 0xff) * mix.src_blend) >> 8) + ((((d >> 16) & 0xff) * mix.dst_blend) >> 8);
		INT32 g = ((((s >>  8) & 0xff) * mix.src_blend) >> 8) + ((((d >>  8) & 0xff) * mix.dst_blend) >> 8);
		INT32 b = ((((s >>  0) & 0xff) * mix.src_blend) >> 8) + ((((d >>  0) & 0xff) * mix.dst_blend) >> 8);
		if (r > 0xff) r = 0xff;
		if (g > 0xff) g = 0xff;
		if (b > 0xff) b = 0xff;

		dst[x] = (r << 16) | (g << 8) | b;
	}
}

struct layer_ref
{
	UINT8 kind;
	f3_layer_inf *inf;
};

static void scanline_draw()
{
	const UINT16 *control_1 = (const UINT16*)(TaitoF3CtrlRAM + 0x10);

	static f3_line_inf line_data;
	memset(&line_data, 0, sizeof(line_data));
	line_data.x_sample = 16;

	const INT32 pf_width = extended_layers ? 1024 : 512;

	for (INT32 i = 0; i < NUM_SPRITEGROUPS; i++) {
		line_data.sp[i].index = i;
		line_data.sp[i].src_bitmap = sprite_framebuffer[i];
		line_data.sp[i].src_pitch = FB_WIDTH;
	}
	for (INT32 pf = 0; pf < NUM_PLAYFIELDS; pf++) {
		get_pf_scroll(pf, line_data.pf[pf].reg_sx, line_data.pf[pf].reg_sy);

		line_data.pf[pf].x_scale = 0x80;
		line_data.pf[pf].reg_fx_y = line_data.pf[pf].reg_sy;
		line_data.pf[pf].width_mask = pf_width - 1;
		line_data.pf[pf].src_pitch = pf_width;
		line_data.pf[pf].index = pf;
	}

	const UINT16 pv_x = BURN_ENDIAN_SWAP_INT16(control_1[4]);
	const UINT16 pv_y = BURN_ENDIAN_SWAP_INT16(control_1[5]);
	if (flipscreen) {
		line_data.pivot.pv_sx = pv_x - 12;
		line_data.pivot.pv_sy = pv_y;
	} else {
		line_data.pivot.pv_sx = -pv_x - 5;
		line_data.pivot.pv_sy = -pv_y;
	}
	line_data.pivot.src_pitch = 512;

	static mix_pix line_buf[H_TOTAL];
	static pri_mode line_pri[H_TOTAL];

	for (INT32 screen_y = 0; screen_y != 256; screen_y++)
	{
		const INT32 y = flipscreen ? 255 - screen_y : screen_y;
		read_line_ram(line_data, y);
		line_data.y = screen_y;

		// tilemap source selection depends on current line data
		for (INT32 pf_num = 0; pf_num < NUM_PLAYFIELDS; pf_num++) {
			f3_layer_inf &pf = line_data.pf[pf_num];
			INT32 tmap_number = pf_num;
			if (!extended_layers && pf.alt_tilemap)
				tmap_number += 2;
			pf.src_bitmap = bitmap_layer[tmap_number];
			pf.flags_bitmap = bitmap_flags[tmap_number];

			pf.reg_fx_x = pf.reg_sx + pf.rowscroll;
			pf.reg_fx_x += 10 * (pf.x_scale - (1 << 8));
		}
		if (line_data.pivot.use_pix()) {
			line_data.pivot.src_bitmap = bitmap_layer[9];
			line_data.pivot.flags_bitmap = bitmap_flags[9];
		} else {
			line_data.pivot.src_bitmap = bitmap_layer[8];
			line_data.pivot.flags_bitmap = bitmap_flags[8];
		}

		if (screen_y >= clip_min_y && screen_y <= clip_max_y)
		{
			for (INT32 x = 0; x < H_TOTAL; x++) {
				line_buf[x].src_pal = 0;
				line_buf[x].dst_pal = line_data.bg_palette;
				line_buf[x].src_blend = 0x00;
				line_buf[x].dst_blend = 0xff;
				line_pri[x].src_prio = 0;
				line_pri[x].dst_prio = 0;
				line_pri[x].src_blendmode = 0xff;
			}

			// sort layers by priority (stable, highest first)
			layer_ref layers[NUM_SPRITEGROUPS + NUM_PLAYFIELDS + 1] = {
				{ LAYER_PIVOT,  &line_data.pivot },
				{ LAYER_SPRITE, &line_data.sp[0] }, { LAYER_PF, &line_data.pf[0] },
				{ LAYER_SPRITE, &line_data.sp[3] }, { LAYER_PF, &line_data.pf[3] },
				{ LAYER_SPRITE, &line_data.sp[2] }, { LAYER_PF, &line_data.pf[2] },
				{ LAYER_SPRITE, &line_data.sp[1] }, { LAYER_PF, &line_data.pf[1] }
			};
			const INT32 num_layers = NUM_SPRITEGROUPS + NUM_PLAYFIELDS + 1;
			for (INT32 i = 1; i < num_layers; i++) {
				for (INT32 j = i; j > 0 && layers[j - 1].inf->prio < layers[j].inf->prio; j--) {
					layer_ref t = layers[j]; layers[j] = layers[j - 1]; layers[j - 1] = t;
				}
			}

			// draw layers to the line buffer, top to bottom
			for (INT32 i = 0; i < num_layers; i++) {
				const f3_layer_inf *gfx = layers[i].inf;
				switch (layers[i].kind) {
					case LAYER_PIVOT:
						if (nBurnLayer & 0x10) draw_layer<LAYER_PIVOT>(gfx, line_buf, line_pri, line_data);
						break;
					case LAYER_SPRITE:
						if (nSpriteEnable & (1 << gfx->index)) draw_layer<LAYER_SPRITE>(gfx, line_buf, line_pri, line_data);
						break;
					case LAYER_PF:
						if (nBurnLayer & (1 << gfx->index)) draw_layer<LAYER_PF>(gfx, line_buf, line_pri, line_data);
						break;
				}
			}

			render_line(output_bitmap + screen_y * 512, line_buf);
		}

		if (screen_y != 0) {
			for (INT32 i = 0; i < NUM_PLAYFIELDS; i++) {
				line_data.pf[i].reg_fx_y += line_data.pf[i].y_scale;
			}
		}
	}
}

/*============================================================================*/

void TaitoF3VideoInit()
{
	m_spritelist = (f3_tempsprite*)BurnMalloc(0x400 * sizeof(f3_tempsprite));
	m_sprite_end = m_spritelist;

	for (INT32 i = 0; i < NUM_SPRITEGROUPS; i++) {
		sprite_framebuffer[i] = (UINT16*)BurnMalloc(FB_WIDTH * FB_HEIGHT * sizeof(UINT16));
	}
	sprite_pri_bitmap = (UINT8*)BurnMalloc(FB_WIDTH * FB_HEIGHT);

	TaitoF3VideoReset();
}

static void pal16_check_init()
{
	if (nBurnBpp < 3 && !pal16) {
		pal16 = (UINT16 *)BurnMalloc((1 << 24) * sizeof (UINT16));

		for (INT32 i = 0; i < (1 << 24); i++) {
			pal16[i] = BurnHighCol(i / 0x10000, (i / 0x100) & 0xff, i & 0xff, 0);
		}
	}
}

void TaitoF3VideoExit()
{
	BurnFree (m_spritelist);

	for (INT32 i = 0; i < NUM_SPRITEGROUPS; i++) {
		BurnFree (sprite_framebuffer[i]);
	}
	BurnFree (sprite_pri_bitmap);

	if (pal16) {
		BurnFree(pal16);
		pal16 = NULL;
	}
}

void TaitoF3VideoScan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(flipscreen);
		SCAN_VAR(sprite_extra_planes);
		SCAN_VAR(sprite_pen_mask);
		SCAN_VAR(sprite_trails);
		SCAN_VAR(sprite_bank);

		// the sprite framebuffers are only carried over between frames when trails are on
		if (sprite_trails) {
			for (INT32 i = 0; i < NUM_SPRITEGROUPS; i++) {
				ScanVar(sprite_framebuffer[i], FB_WIDTH * FB_HEIGHT * sizeof(UINT16), "F3 sprite framebuffer");
			}
			ScanVar(sprite_pri_bitmap, FB_WIDTH * FB_HEIGHT, "F3 sprite priority");
			SCAN_VAR(sprite_pri_row_usage);
		}

		if (nAction & ACB_WRITE) {
			last_flipscreen = -1;
			last_pixel_yoffs = -1;
		}
	}
}

void TaitoF3DrawCommon(INT32 scanline_start)
{
	if (TaitoF3PalRecalc) {
		pal16_check_init();

		for (INT32 i = 0; i < 0x8000; i+=4) {
			pPaletteUpdateCallback(i);
		}
		TaitoF3PalRecalc = 0;
	}

	clip_min_x = H_START;
	clip_max_x = H_START + H_VIS - 1;
	clip_min_y = scanline_start;
	clip_max_y = scanline_start + nScreenHeight - 1;
	if (clip_max_y > 255) clip_max_y = 255;

	switch (sprite_lag) {
		case 2: get_sprite_info((UINT16*)TaitoSpriteRamDelayed2); break;
		case 1: get_sprite_info((UINT16*)TaitoSpriteRamDelayed); break;
		default: get_sprite_info((UINT16*)TaitoSpriteRam); break;
	}

	draw_sprites();

	// flipscreen is baked into the tilemap bitmaps
	if (flipscreen != last_flipscreen) {
		last_flipscreen = flipscreen;
		memset (dirty_tiles, 1, 0x8000/4);
		memset (dirty_tile_count, 1, 10);
	}

	for (INT32 i = 0; i < (8 >> extended_layers); i++) {
		draw_pf_layer(i);
	}

	draw_pixel_layer();
	draw_vram_layer();

	memset (output_bitmap, 0, 512 * 512 * sizeof(UINT32));

	scanline_draw();

	// copy video to draw surface
	{
		UINT32 *src = output_bitmap + (scanline_start * 512) + H_START;

		for (INT32 y = 0; y < nScreenHeight; y++, src += 512)
		{
			UINT8 *dst = pBurnDraw + y * nBurnPitch;

			if (nBurnBpp == 2) { // 16bpp
				for (INT32 x = 0; x < nScreenWidth; x++, dst += nBurnBpp)
				{
					PutPix(dst, pal16[src[x] & 0xffffff]);
				}
			} else if (nBurnBpp == 4) { // 32bpp
				UINT32 *dst32 = (UINT32*)dst;
				for (INT32 x = 0; x < nScreenWidth; x++)
				{
					dst32[x] = src[x];
				}
			} else { // 24bpp
				for (INT32 x = 0; x < nScreenWidth; x++, dst += nBurnBpp)
				{
					PutPix(dst, src[x]);
				}
			}
		}
	}
}
