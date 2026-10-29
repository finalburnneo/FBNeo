// GBA PPU multithreaded renderer: main thread captures per-scanline snapshots,
// worker composes pixels into triple-buffered outputs (pthread / Win32 / single).

#pragma once

#include "gba.h"

// Uncomment to force single-threaded mode.
// #define GBAPPU_WORKER_DISABLE

#if !defined(GBAPPU_WORKER_DISABLE)
#  if defined(_WIN32) || defined(BUILD_WIN32)
#    ifndef WIN32_LEAN_AND_MEAN
#      define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#      define NOMINMAX
#    endif
#    include <windows.h>
#    include <stdlib.h>
#    define GBAPPU_WORKER_HAVE_PTHREAD   0
#    define GBAPPU_WORKER_HAVE_WINTHREAD 1
#  elif (defined(__unix__) || defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)) && !defined(__EMSCRIPTEN__)
#    include <pthread.h>
#    include <errno.h>
#    include <stdlib.h>
#    include <string.h>
#    define GBAPPU_WORKER_HAVE_PTHREAD   1
#    define GBAPPU_WORKER_HAVE_WINTHREAD 0
#  else
#    define GBAPPU_WORKER_HAVE_PTHREAD   0
#    define GBAPPU_WORKER_HAVE_WINTHREAD 0
#  endif
#else
#  define GBAPPU_WORKER_HAVE_PTHREAD   0
#  define GBAPPU_WORKER_HAVE_WINTHREAD 0
#endif

#if !defined(GBAPPU_WORKER_HAVE_PTHREAD)
#  define GBAPPU_WORKER_HAVE_PTHREAD   0
#endif
#if !defined(GBAPPU_WORKER_HAVE_WINTHREAD)
#  define GBAPPU_WORKER_HAVE_WINTHREAD 0
#endif

#define GBAPPU_WORKER_ENABLED (GBAPPU_WORKER_HAVE_PTHREAD || GBAPPU_WORKER_HAVE_WINTHREAD)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ppw_blob_hdr_t {
	UINT32 magic;      // PPW_BLOB_MAGIC
	UINT32 version;    // PPW_BLOB_VERSION
	INT32  out_bpp;    // depth of the saved payload (2/3/4)
	INT32  front_idx;  // presented slot (0..N-1), -1 = no frame presented yet
} ppw_blob_hdr_t;
#define PPW_BLOB_MAGIC    0x00575021u	// "!PW"
#define PPW_BLOB_VERSION  1u
#define PPW_BLOB_HEADER   ((int)sizeof(ppw_blob_hdr_t))
#define PPW_BPP_MAX       4                          // 32bpp XRGB8888 = 4 B/px (max of 2/3/4)
#define PPW_BLOB_MAX      (PPW_BLOB_HEADER + GBA_LCD_W * GBA_LCD_H * PPW_BPP_MAX)

#if GBAPPU_WORKER_ENABLED

// --- Constants -------------------------------------------------------------

// PPU MMIO snapshot window (96B; DISPCNT..BLDY plus padding).
#define GBAPPU_WORKER_IO_SIZE     0x60
// Three slots keep presented / being-rendered / being-captured buffers disjoint
// (OPT1 compares against the stable presented slot); one job is in flight.
#define GBAPPU_WORKER_N_BUFFERS   3

// Per-line VRAM journal capacity inside a snapshot slot (bytes).
#define PPW_VRAM_POOL             (256 * 1024)
// Worker thread stack size.
#define GBAPPU_WORKER_STACK_SIZE  (128 * 1024)

// Render context - worker-side pixel composition state.
//   framebuffer/backbuf: XRGB8888 target ([23:0]=BGR888, [31:24]=priority/flags),
//     where all BG/OBJ/alpha/ghosting work runs.
//   out_ptr/out_bpp: scanlines are converted to native format (BGR24/RGB565) when
//     out_bpp != 4, so DrvDraw memcpys rows to pBurnDraw directly.
typedef struct ppu_worker_render_ctx_t {
	UINT8* vram;
	UINT8* oam;
	UINT8* palette;
	UINT8* io;
	gba_ppu_t* ppu;
	UINT32* first_target_buffer;
	UINT32* second_target_buffer;
	UINT8*  window;
	UINT32* framebuffer;   // internal 32-bit XRGB8888 render target
	// Direct output: when out_ptr!=NULL, each scanline is written in out_bpp
	// format right after the pixel loop completes (all pixels then final).
	UINT8*  out_ptr;       // output buffer (row stride = GBA_LCD_W * out_bpp)
	INT32   out_bpp;       // 2=RGB565, 3=BGR24; 4 uses framebuffer directly
	bool    stop_mode;
	// True when every scanline's OAM equals line 0's (the Y-bucket seed): lets
	// render_objs skip both per-line 128-sprite rescans unless a bucket overflowed.
	bool    oam_uniform;
	// Per-frame ghosting Q8 factor (frame-level constant).
	INT32 sbf_q8;
	INT32 one_m_sbf_q8;
	bool  ghost_fast;         // sbf_q8 == 0 -> direct write, no blend

	// Per-frame tile-row cache (same tile + same py reused across scanlines).
	INT32  fc_tile_key[4];
	UINT8  fc_tile_bytes[4][8];
	bool   fc_tile_valid[4];

	// Per-frame OBJ Y-buckets: up to 64 sprites per scanline.
	const INT32 (*obj_bucket)[64];
	const INT32*  obj_bucket_n;
	// Scanline flags set at render_scanline entry.
	bool  no_windows;         // windows have no per-pixel effect this line
	bool  need_second_buf;    // frame-level flag: 1 if any scanline needs a second buffer (set by render_objs)
	INT32 eff_bld_mode;       // BLDCNT effect mode actually in force (0 when inert)
	bool  has_any_obj_this_line; // render_objs saw a visible OBJ; fast-pixel skips backdrop fill when set
	// FP-A: pre-expanded palette table (512 x XRGB8888 UINT32), rebuilt per
	// scanline only when the palette changed (dirty-checked).
	UINT32 pal_xrgb32[512];
	UINT8  last_palette[1024];   // FP-A: snapshot of palette at last rebuild
	bool   pal_valid;            // false until first rebuild (ctx is not zeroed)
	// BG palette bank cache, refreshed at tile boundaries by the generic BG loop.
	INT32  cached_pal_bank[4];
} ppu_worker_render_ctx_t;

// Per-scanline snapshot (~2.1 KB / line).
typedef struct ppu_worker_line_state_t {
	UINT8  io[GBAPPU_WORKER_IO_SIZE];
	UINT8  oam[1024];
	UINT8  palette[1024];
	INT32  bgx[2];
	INT32  bgy[2];
	UINT16 dispcnt_pipeline[3];
} ppu_worker_line_state_t;

// Full frame snapshot: 128 KB VRAM + 160 line states + frame settings.
typedef struct ppu_worker_snapshot_t {
	UINT8  vram[128 * 1024];
	ppu_worker_line_state_t line_state[GBA_LCD_H];
	float  ghosting_strength;
	bool   stop_mode;

	// Per-scanline VRAM journal. The reference composites line y at its own
	// HBlank, so mid-frame VRAM writes make lines differ from the whole-frame
	// vram[] above. vram_base holds line 0; later lines replay their byte range.
	UINT8  vram_base[128 * 1024];
	UINT8  vram_pool[PPW_VRAM_POOL];
	UINT32 vram_line_lo[GBA_LCD_H];   // dirty range low; lo >= hi means unchanged
	UINT32 vram_line_hi[GBA_LCD_H];   // dirty range high (exclusive)
	UINT32 vram_line_off[GBA_LCD_H];  // where line y's bytes start in vram_pool
	UINT32 vram_pool_used;
	bool   vram_base_valid;           // vram_base holds this frame's line-0 state
	bool   vram_journal;              // at least one line changed mid-frame
	bool   vram_journal_overflow;     // pool full: fall back to whole-frame vram[]
} ppu_worker_snapshot_t;

#if GBAPPU_WORKER_HAVE_PTHREAD
typedef struct ppu_worker_evt_t {
	pthread_mutex_t mutex;
	pthread_cond_t  cond;
	int             count;
} ppu_worker_evt_t;
#elif GBAPPU_WORKER_HAVE_WINTHREAD
typedef HANDLE ppu_worker_evt_t;
#endif

typedef struct ppu_worker_t {
	bool enabled;
	bool exiting;

	// Output pixel depth - set once at driver init (nBurnBpp = 2/3/4); fixed per session.
	INT32 out_bpp;

	ppu_worker_snapshot_t snapshots[GBAPPU_WORKER_N_BUFFERS];
	// Internal XRGB8888 render targets (one per slot); presented directly at
	// 32bpp (zero copy), otherwise re-encoded per scanline into out_buf.
	UINT32  backbuf[GBAPPU_WORKER_N_BUFFERS][GBA_LCD_W * GBA_LCD_H];
	// Running VRAM the journal path replays deltas onto (worker thread only).
	UINT8   vram_scratch[128 * 1024];

	// Output scanline buffers (one per slot), lazily allocated when
	// out_bpp!=4 (bpp=2: 75 KB/slot, bpp=3: 112 KB/slot; NULL at 32bpp).
	UINT8* out_buf[GBAPPU_WORKER_N_BUFFERS];

	volatile int w_idx;          // main: slot the next frame is captured into
	volatile int r_idx;          // worker: slot the in-flight job renders into
	volatile int ready_back_idx; // slot of the last completed job
	volatile int front_idx;      // presented slot (backbuf at 32bpp, else out_buf)

	volatile bool worker_busy;
	volatile bool have_first_frame;
	volatile int  inflight;      // submitted-but-not-collected job count
	// Set by main thread to abort the in-flight render (load_state / mode switch).
	volatile bool cancel_render;

	// OPT1 whole-frame reuse: control key captured after each full render and
	// compared on the next frame to memcpy the previous backbuf when unchanged.
	UINT8  prev_key[86];       // line_state[0].io[0..85] of last rendered frame
	bool   prev_sig_valid;

#if GBAPPU_WORKER_HAVE_PTHREAD
	pthread_t        worker;
	ppu_worker_evt_t job_evt;
	ppu_worker_evt_t done_evt;
#elif GBAPPU_WORKER_HAVE_WINTHREAD
	HANDLE           worker;
	ppu_worker_evt_t job_evt;
	ppu_worker_evt_t done_evt;
#endif
} ppu_worker_t;

static inline bool ppu_worker_enabled(const ppu_worker_t* w) {
	return w && w->enabled;
}

// --- Internal helpers ------------------------------------------------------

// Reset per-slot state. line_state is overwritten each frame by snapshots
// before being read - no memset needed.
static inline void ppu_worker_reset_slot(ppu_worker_t* w, int idx, gba_t* gba)
{
	ppu_worker_snapshot_t* s = &w->snapshots[idx];
	s->ghosting_strength = gba->ppu.ghosting_strength;
	s->stop_mode         = gba->stop_mode;
	// Seed line_state[0] OAM/palette: OAM feeds the OBJ Y-bucket seed, and both
	// must be valid even before line 0 is snapshotted.
	memcpy(s->line_state[0].oam,     gba->mem.oam,     1024);
	memcpy(s->line_state[0].palette, gba->mem.palette, 1024);
	// The per-line VRAM journal is rebuilt by the line snapshots of this frame.
	s->vram_base_valid       = false;
	s->vram_journal          = false;
	s->vram_journal_overflow = false;
	s->vram_pool_used        = 0;
}

// Install per-line state into the render context. stop_mode is frame-level.
static inline void ppu_worker_install_io(ppu_worker_render_ctx_t* ctx,
                                         const ppu_worker_line_state_t* ls,
                                         bool stop_mode)
{
	ctx->io      = (UINT8*)ls->io;
	ctx->oam     = (UINT8*)ls->oam;
	ctx->palette = (UINT8*)ls->palette;
	ctx->ppu->aff[0].render_bgx = ls->bgx[0];
	ctx->ppu->aff[0].render_bgy = ls->bgy[0];
	ctx->ppu->aff[1].render_bgx = ls->bgx[1];
	ctx->ppu->aff[1].render_bgy = ls->bgy[1];
	ctx->ppu->dispcnt_pipeline[0] = ls->dispcnt_pipeline[0];
	ctx->ppu->dispcnt_pipeline[1] = ls->dispcnt_pipeline[1];
	ctx->ppu->dispcnt_pipeline[2] = ls->dispcnt_pipeline[2];
	ctx->stop_mode = stop_mode;
}

// Worker-side scanline composition in XRGB8888 ([23:0]=BGR888, [31:24]=flags).
// Fast paths: FP-A palette pre-expand, FP-B bitmap setup, FP-D opaque-OBJ fast
// pixel, FP-E 4bpp 8px decode, FP-F no-OBJ early out; plus window/blend
// reduction, OBJ Y-buckets and tile-row caches.

static inline UINT16 ppu_worker_io_read16(ppu_worker_render_ctx_t* ctx, UINT32 addr)
{
	return BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->io + (addr & 0x3ff)));
}

// OBJ size lookup: index = obj_size * 4 + obj_shape (0..15).
static const INT32 gba_obj_xsize[16] = {
	 8,16, 8, 0,  16,32, 8, 0,  32,32,16, 0,  64,64,32, 0
};
static const INT32 gba_obj_ysize[16] = {
	 8, 8,16, 0,  16, 8,32, 0,  32, 16,32, 0,  64,32,64, 0
};

// Compact hit record - all data Pass 2 needs, no extra OAM reads.
typedef struct ppu_worker_hit_t {
	UINT8  o, rot_scale, double_size, obj_mode;
	UINT8  mosaic, colors_or_palettes, h_flip, v_flip;
	UINT8  priority, palette;
	UINT16 rotscale_param;
	INT16  tile_base;
	INT16  x_coord, x_start, x_end;
	UINT16 y_coord, x_size, y_size;
} ppu_worker_hit_t;

// Helper: fill one hit entry from OAM attributes.
static inline void ppu_worker_fill_hit(ppu_worker_hit_t* h, const UINT8* oam, INT32 o,
                                       bool rot_scale, bool double_size,
                                       INT32* n_semitrans_objs, bool* obj_seen,
                                       INT32 y_coord, INT32 x_coord, INT32 x_size, INT32 y_size,
                                       INT32 xs_c, INT32 xe_c) {
	UINT32 a012 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(oam + o * 8));
	UINT16 attr0 = (UINT16)(a012 & 0xFFFFu);
	UINT16 attr1 = (UINT16)(a012 >> 16);
	UINT16 attr2 = BURN_ENDIAN_SWAP_INT16(*(const UINT16*)(oam + o * 8 + 4));

	h->o                  = (UINT8)o;
	h->rot_scale          = (UINT8)rot_scale;
	h->double_size        = (UINT8)double_size;
	INT32 obj_mode_val    = SB_BFE(attr0, 10, 2);
	h->obj_mode           = (UINT8)obj_mode_val;
	if (obj_mode_val == 1) ++(*n_semitrans_objs);
	h->mosaic             = (UINT8)SB_BFE(attr0, 12, 1);
	bool  is_256          = SB_BFE(attr0, 13, 1);
	h->colors_or_palettes = (UINT8)is_256;
	h->h_flip             = (UINT8)(SB_BFE(attr1, 12, 1) && !rot_scale);
	h->v_flip             = (UINT8)(SB_BFE(attr1, 13, 1) && !rot_scale);
	h->rotscale_param     = (UINT16)SB_BFE(attr1,  9, 5);
	h->priority           = (UINT8)SB_BFE(attr2, 10, 2);
	h->palette            = (UINT8)SB_BFE(attr2, 12, 4);
	h->tile_base          = (INT16)SB_BFE(attr2,  0, 10);
	h->x_coord            = (INT16)x_coord;
	h->x_start            = (INT16)xs_c;
	h->x_end              = (INT16)xe_c;
	h->y_coord            = (UINT16)y_coord;
	h->x_size             = (UINT16)x_size;
	h->y_size             = (UINT16)y_size;
	obj_seen[o] = true;
}

// window_permissive(): can this scanline's WIN0/WIN1 rects reduce to a single
// mask equal to the no-window default (0x3F, or 0x1F when the effect bit is
// inert)? If so the window array / rect / backdrop-clear passes are skipped.
static inline bool ppu_worker_window_permissive(ppu_worker_render_ctx_t* ctx,
                                                UINT16 dispcnt, INT32 lcd_y)
{
	// OBJ-window coverage follows sprite shapes, so it is not statically reducible.
	if (SB_BFE(dispcnt, 15, 1)) return false;

	UINT8  cdef  = (UINT8)(ppu_worker_io_read16(ctx, GBA_WINOUT) & 0x3F);
	UINT16 winin = ppu_worker_io_read16(ctx, GBA_WININ);
	bool   cov[2]  = { false, false };
	bool   full[2] = { false, false };
	UINT8  val[2]  = { 0, 0 };
	INT32  xmin[2] = { 0, 0 }, xmax[2] = { 0, 0 };
	for (INT32 n = 0; n < 2; ++n) {
		if (!SB_BFE(dispcnt, 13 + n, 1)) continue;
		UINT16 WINH = ppu_worker_io_read16(ctx, (n == 0) ? GBA_WIN0H : GBA_WIN1H);
		UINT16 WINV = ppu_worker_io_read16(ctx, (n == 0) ? GBA_WIN0V : GBA_WIN1V);
		xmin[n] = SB_BFE(WINH, 8, 8); xmax[n] = SB_BFE(WINH, 0, 8);
		INT32 ymin = SB_BFE(WINV, 8, 8), ymax = SB_BFE(WINV, 0, 8);
		if (xmin[n] > xmax[n]) xmax[n] = 240;
		if (ymin > ymax) ymax = 160;
		if (xmax[n] > 240) xmax[n] = 240;
		if (ymax > 160) ymax = 160;
		if (xmin[n] >= xmax[n]) continue;
		if (lcd_y < ymin || lcd_y >= ymax) continue;
		cov[n]  = true;
		full[n] = (xmin[n] == 0 && xmax[n] >= 240);
		val[n]  = (UINT8)(SB_BFE(winin, n * 8, 6) & 0x3F);
	}
	UINT8 unival;
	if (cov[0] && full[0])                 unival = val[0];
	else if (!cov[0] && cov[1] && full[1]) unival = val[1];
	else if (!cov[0] && !cov[1])           unival = cdef;
	else {
		// Partial coverage: split at the WIN0/WIN1 x-edges and sample one pixel
		// per interval; the mask is piecewise-constant, so agreement is exact.
		INT32 segs[4]; INT32 n_segs = 0;
		if (cov[0]) { segs[n_segs++] = xmin[0]; segs[n_segs++] = xmax[0]; }
		if (cov[1]) { segs[n_segs++] = xmin[1]; segs[n_segs++] = xmax[1]; }
		for (INT32 i = 0; i < n_segs; ++i)
			for (INT32 j = i + 1; j < n_segs; ++j)
				if (segs[j] < segs[i]) { INT32 t = segs[i]; segs[i] = segs[j]; segs[j] = t; }
		INT32 nd = 0;
		for (INT32 i = 0; i < n_segs; ++i)
			if (segs[i] > 0 && segs[i] < 240 && (nd == 0 || segs[i] != segs[nd - 1]))
				segs[nd++] = segs[i];
		n_segs = nd;
		UINT8 uv = 0xFF;
		INT32 prev = 0;
		for (INT32 i = 0; i <= n_segs; ++i) {
			INT32 end = (i < n_segs) ? segs[i] : 240;
			if (end > prev) {
				INT32 sx = prev + ((end - prev) >> 1);
				UINT8 v;
				if      (cov[0] && sx >= xmin[0] && sx < xmax[0]) v = val[0];
				else if (cov[1] && sx >= xmin[1] && sx < xmax[1]) v = val[1];
				else                                             v = cdef;
				if (uv == 0xFF) uv = v;
				else if (uv != v) return false;
			}
			prev = end;
		}
		unival = uv;
	}
	if (unival == 0x3F) return true;
	// 0x1F differs from 0x3F only in the special-effects enable bit, which is inert
	// when the effect mode is (empty mask, 0, or alpha with no second target).
	if (unival == 0x1F && ctx->eff_bld_mode == 0) {
		return true;
	}
	return false;
}

static inline bool ppu_worker_render_objs(ppu_worker_render_ctx_t* ctx, INT32 sprite_lcd_y)
{
	UINT16 dispcnt         = ppu_worker_io_read16(ctx, GBA_DISPCNT);
	INT32  bg_mode         = SB_BFE(dispcnt, 0, 3);
	INT32  obj_vram_map_2d = !SB_BFE(dispcnt, 6, 1);

	UINT16 mos_reg = ppu_worker_io_read16(ctx, GBA_MOSAIC);
	INT32  mos_x   = SB_BFE(mos_reg,  8, 4) + 1;
	INT32  mos_y   = SB_BFE(mos_reg, 12, 4) + 1;
	if (++ctx->ppu->mosaic_y_counter >= mos_y || sprite_lcd_y == 0)
		ctx->ppu->mosaic_y_counter = 0;

	bool win_en            = SB_BFE(dispcnt, 13, 3) != 0;
	bool  obj_window_enable = SB_BFE(dispcnt, 15, 1);
	UINT8 default_window_control = 0x3F;     // no windows: all layers visible (matches gbappu.h)
	UINT8 obj_window_control    = 0x3F;
	if (win_en || obj_window_enable) {
		UINT16 WINOUT = ppu_worker_io_read16(ctx, GBA_WINOUT);
		if (win_en)
			default_window_control = SB_BFE(WINOUT, 0, 8);
		obj_window_control = default_window_control;  // WINOUT outside value (gbappu.h)
		if (obj_window_enable)
			obj_window_control   = SB_BFE(WINOUT, 8, 6);
	}

	// Count semi-transparent OBJs this scanline for need_second_buf.
	INT32 n_semitrans_objs = 0;

	// Read BLDCNT early so FP-F early-return can set need_second_buf correctly.
	UINT16 bldcnt_for_nb = ppu_worker_io_read16(ctx, GBA_BLDCNT);
	INT32  bld_mode_nb   = SB_BFE(bldcnt_for_nb, 6, 2);
	// Effect mode is inert when the first-target mask is empty, alpha mode when
	// the second-target mask is empty; reporting mode 0 runs the fast-pixel path.
	bool alpha_inert = (bld_mode_nb == 1) && ((bldcnt_for_nb & 0x3F00) == 0);
	ctx->eff_bld_mode = (((bldcnt_for_nb & 0x3F) == 0) || alpha_inert) ? 0 : bld_mode_nb;

	// no_windows: all windows off (window[x] unread) -> skip the window passes.
	ctx->no_windows = !win_en && !obj_window_enable;
	if (!ctx->no_windows && !obj_window_enable) {
		if (ppu_worker_window_permissive(ctx, dispcnt, sprite_lcd_y))
			ctx->no_windows = true;
	}

	if (!ctx->no_windows) {
		for (INT32 x = 0; x < 240; ++x)
			ctx->window[x] = default_window_control;
	}

	ppu_worker_hit_t hits[128];
	INT32 n_hits = 0;
	// obj_seen bitmap: sprites added via Y-bucket (used by late catch-up).
	bool  obj_seen[128] = { false };

	bool display_obj = SB_BFE(dispcnt, 12, 1);
	if (display_obj) {
		INT32 sprite_cycles = SB_BFE(dispcnt, 5, 1) ? 954 : 1210;

		// FP-F: empty Y-bucket and no windows -> skip the OBJ passes when no
		// sprite covers this line (uniform OAM proves it; else a Y-only OAM scan).
		INT32 n_bucket = ctx->obj_bucket_n[sprite_lcd_y];
		if (n_bucket == 0 && ctx->no_windows) {
			if (ctx->oam_uniform) {
				ctx->need_second_buf = (ctx->eff_bld_mode == 1) || obj_window_enable;
				return false;
			}
			bool any = false;
			const UINT8* oq_p = ctx->oam;
			for (INT32 oq = 0; oq < 128; ++oq) {
				UINT16 a0 = BURN_ENDIAN_SWAP_INT16(*(const UINT16*)(oq_p + oq*8));
				bool   rs  = SB_BFE(a0, 8, 1);
				if (SB_BFE(a0, 9, 1) && !rs) continue;
				bool   ds  = SB_BFE(a0, 9, 1) && rs;
				INT32  y0  = SB_BFE(a0, 0, 8);
				INT32  sh  = SB_BFE(a0, 14, 2);
				UINT16 a1  = BURN_ENDIAN_SWAP_INT16(*(const UINT16*)(oq_p + oq*8 + 2));
				INT32  sz  = SB_BFE(a1, 14, 2);
				INT32  ys  = gba_obj_ysize[sz*4 + sh];
				INT32  ye  = ys * (ds ? 2 : 1);
				INT32  sy  = sprite_lcd_y - y0;
				if (y0 >= GBA_LCD_H) sy += 256;
				if (sy >= 0 && sy < ye) { any = true; break; }
			}
			if (!any) {
				ctx->need_second_buf = (ctx->eff_bld_mode == 1) || obj_window_enable;
				return false;
			}
		}
		// Pass 1: scan this line's Y-bucket only; X-clip drops off-screen hits.
		for (INT32 bi = 0; bi < n_bucket; ++bi) {
			INT32 o = ctx->obj_bucket[sprite_lcd_y][bi];
			UINT32 attr01 = BURN_ENDIAN_SWAP_INT32(*(UINT32*)(ctx->oam + o * 8));
			UINT16 attr0 = (UINT16)attr01;
			UINT16 attr1 = (UINT16)(attr01 >> 16);

			bool  rot_scale   = SB_BFE(attr0, 8, 1);
			bool  double_size = SB_BFE(attr0, 9, 1) && rot_scale;
			if (SB_BFE(attr0, 9, 1) && !rot_scale)  // obj_disable
				continue;

			INT32 y_coord   = SB_BFE(attr0, 0, 8);
			INT32 obj_shape = SB_BFE(attr0, 14, 2);
			INT32 obj_size  = SB_BFE(attr1, 14, 2);
			INT32 y_size    = gba_obj_ysize[obj_size * 4 + obj_shape];
			INT32 y_size_eff = y_size * (double_size ? 2 : 1);
			INT32 sy        = sprite_lcd_y - y_coord;
			if (y_coord >= GBA_LCD_H) sy += 256;
			if (sy < 0 || sy >= y_size_eff)
				continue;

			// X range first: fully off-screen sprites cost no cycles.
			INT32 x_size = gba_obj_xsize[obj_size * 4 + obj_shape];
			INT16 x_coord = SB_BFE(attr1, 0, 9);
			if (SB_BFE(x_coord, 8, 1))
				x_coord |= 0xfe00;
			INT32 draw_w = x_size * (double_size ? 2 : 1);
			INT32 xs_c   = x_coord < 0 ? 0 : (x_coord > GBA_LCD_W ? GBA_LCD_W : x_coord);
			INT32 xe_c   = (x_coord + draw_w) > GBA_LCD_W ? GBA_LCD_W :
			                ((x_coord + draw_w) < 0 ? 0 : (x_coord + draw_w));
			if (xe_c <= xs_c)
				continue;

			// Charge cycles on the visible span (after X-clip), by colour depth.
			INT32 vis_w = xe_c - xs_c;
			bool  is_256 = SB_BFE(attr0, 13, 1);
			INT32 bpp_cost = is_256 ? 2 : 1;
			if (rot_scale)
				sprite_cycles -= 10 + vis_w * 2;   // 10-cycle setup + double-width fetch
			else
				sprite_cycles -= vis_w * bpp_cost;
			if (sprite_cycles <= 0)
				break;

			if (n_hits >= 128)
				break;

			ppu_worker_fill_hit(&hits[n_hits++], ctx->oam, o, rot_scale, double_size,
			                     &n_semitrans_objs, obj_seen, y_coord, x_coord, x_size, y_size, xs_c, xe_c);
		}

		// Late catch-up: recovers sprites moved in by HBlank DMA after bucketing.
		// With uniform OAM and a non-overflowing bucket this adds nothing.
		if (!(ctx->oam_uniform && n_bucket < 64))
		{
			const UINT8* oam_now = ctx->oam;
			for (INT32 o = 0; o < 128; ++o) {
				if (obj_seen[o]) continue;
				UINT32 a01 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(oam_now + o * 8));
				UINT16 a0_n = (UINT16)a01;
				UINT16 a1_n = (UINT16)(a01 >> 16);
				bool   rs_n = SB_BFE(a0_n, 8, 1);
				bool   ds_n = SB_BFE(a0_n, 9, 1) && rs_n;
				if (SB_BFE(a0_n, 9, 1) && !rs_n) continue;
				INT32 y_n  = SB_BFE(a0_n, 0, 8);
				INT32 sh_n = SB_BFE(a0_n, 14, 2);
				INT32 sz_n = SB_BFE(a1_n, 14, 2);
				INT32 ys_n = gba_obj_ysize[sz_n * 4 + sh_n];
				INT32 ye_n = ys_n * (ds_n ? 2 : 1);
				INT32 sy_n = sprite_lcd_y - y_n;
				if (y_n >= GBA_LCD_H) sy_n += 256;
				if (sy_n < 0 || sy_n >= ye_n) continue;
				INT32 xs_n = gba_obj_xsize[sz_n * 4 + sh_n];
				INT16 xc_n = SB_BFE(a1_n, 0, 9);
				if (SB_BFE(xc_n, 8, 1)) xc_n |= 0xfe00;
				INT32 dw_n = xs_n * (ds_n ? 2 : 1);
				INT32 xsc  = xc_n < 0 ? 0 : (xc_n > GBA_LCD_W ? GBA_LCD_W : xc_n);
				INT32 xec  = (xc_n + dw_n) > GBA_LCD_W ? GBA_LCD_W :
				             ((xc_n + dw_n) < 0 ? 0 : (xc_n + dw_n));
				if (xec <= xsc) continue;
				INT32 vw_n = xec - xsc;
				bool  c256 = SB_BFE(a0_n, 13, 1);
				if (rs_n) sprite_cycles -= 10 + vw_n * 2;
				else     sprite_cycles -= vw_n * (c256 ? 2 : 1);
				if (sprite_cycles <= 0) break;
				if (n_hits >= 128) break;
				ppu_worker_fill_hit(&hits[n_hits++], oam_now, o, rs_n, ds_n,
				                     &n_semitrans_objs, obj_seen, y_n, xc_n, xs_n, ys_n, xsc, xec);
			}
		}

		// Pass 2: full decode + pixel composition for hit sprites only.
		for (INT32 hi = 0; hi < n_hits; ++hi) {
			const ppu_worker_hit_t* h = &hits[hi];
			bool  double_size        = h->double_size;
			INT32 obj_mode           = h->obj_mode;
			bool  mosaic             = h->mosaic;
			bool  colors_or_palettes = h->colors_or_palettes;
			bool  h_flip             = h->h_flip;
			bool  v_flip             = h->v_flip;
			INT32 rotscale_param     = h->rotscale_param;
			INT32 tile_base          = h->tile_base;
			INT32 priority           = h->priority;
			INT32 palette            = h->palette;
			INT32 x_coord            = h->x_coord;
			INT32 x_start            = h->x_start;
			INT32 x_end              = h->x_end;
			INT32 y_coord            = h->y_coord;
			INT32 x_size             = h->x_size;
			INT32 y_size             = h->y_size;
			bool  rot_scale          = h->rot_scale;

			INT32 sy0    = sprite_lcd_y - y_coord;
			if (y_coord >= GBA_LCD_H) sy0 += 256;
			INT32 sy_eff = sy0;
			if (mosaic) {
				sy_eff = sy0 - ctx->ppu->mosaic_y_counter;
				if (sy_eff < 0) sy_eff = 0;
			}

			// Rot/scale parameters - loaded once per hit sprite.
			INT32 a = 0, b = 0, c = 0, d = 0;
			INT32 sx_cx = 0, sy_cy = 0, d_cx = 0, d_cy = 0;
			bool use_rot_dda = rot_scale && !mosaic;
			if (rot_scale) {
				UINT32 param_base = rotscale_param * 0x20;
				a = (INT16)BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->oam + param_base + 0x6));
				b = (INT16)BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->oam + param_base + 0xe));
				c = (INT16)BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->oam + param_base + 0x16));
				d = (INT16)BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->oam + param_base + 0x1e));

				if (use_rot_dda) {
					INT32 sx_start_pix = x_start - x_coord;
					INT64 x1_start = (INT64)sx_start_pix << 8;
					INT64 y1       = (INT64)sy_eff       << 8;
					INT64 objref_x = (INT64)(x_size << (double_size ? 8 : 7));
					INT64 objref_y = (INT64)(y_size << (double_size ? 8 : 7));
					sx_cx = (INT32)(a * (x1_start - objref_x) + b * (y1 - objref_y) + ((INT64)x_size << 15));
					sy_cy = (INT32)(c * (x1_start - objref_x) + d * (y1 - objref_y) + ((INT64)y_size << 15));
					d_cx = a * 256;
					d_cy = c * 256;
				}
			}

			// OBJ tile-row byte cache (regular + rot/scale), key = tile*16 + ty;
			// one VRAM fetch serves all pixels of a tile row.
			INT32  obj_cached_key = -1;
			UINT8  obj_cached_row[8];
			INT32  pal_offset  = palette * 16;   // hoisted out of pixel loop
			// OBJ priority code (layout in the scanline-render header).
			UINT32 obj_pri_col = ((UINT32)(5 - priority) << 29) | ((UINT32)7 << 26) | (1u << 24);
			const UINT32* SB_RESTRICT pal_obj_xrgb = ctx->pal_xrgb32 + 256;
			// 1D OBJ mapping: tile-row stride in tiles (0 for 2D).
			INT32  tile_step = colors_or_palettes ? 2 : 1;
			INT32  obj_tile_row_stride = 0;
			if (!obj_vram_map_2d)
				obj_tile_row_stride = (x_size >> 3) * tile_step;

			UINT32* SB_RESTRICT ft_obj = &ctx->first_target_buffer[x_start];
			for (INT32 x = x_start; x < x_end; ++x) {
				UINT32* SB_RESTRICT ft_p = ft_obj++;
				INT32 sx = x - x_coord;
				INT32 sy = mosaic ? sy_eff : sy0;

				if (mosaic) {
					sx = ((x / mos_x) * mos_x - x_coord);
					if (sx < 0) sx = 0;
				}

				if (rot_scale) {
					if (use_rot_dda) {
						sx = (sx_cx >> 16);
						sy = (sy_cy >> 16);
						sx_cx += d_cx;
						sy_cy += d_cy;
					} else {
						INT64 x1 = (INT64)sx << 8;
						INT64 y1 = (INT64)sy << 8;
						INT64 objref_x = (INT64)(x_size << (double_size ? 8 : 7));
						INT64 objref_y = (INT64)(y_size << (double_size ? 8 : 7));
						INT64 x2 = a * (x1 - objref_x) + b * (y1 - objref_y) + ((INT64)x_size << 15);
						INT64 y2 = c * (x1 - objref_x) + d * (y1 - objref_y) + ((INT64)y_size << 15);
						sx = (INT32)(x2 >> 16);
						sy = (INT32)(y2 >> 16);
					}
					if (sx >= x_size || sy >= y_size || sx < 0 || sy < 0)
						continue;
				} else {
					if (h_flip) sx = x_size - sx - 1;
					if (v_flip) sy = y_size - sy - 1;
				}
				INT32 tx = sx & 7;
				INT32 ty = sy & 7;

				INT32 tile;
				if (obj_vram_map_2d) {
					INT32 base = colors_or_palettes ? tile_base & ~1 : tile_base;
					tile  = (base + (sx >> 3) * tile_step) & 0x1f;
					tile |= (base + (sy >> 3) * 32       ) & 0x3e0;
				} else {
					tile = (tile_base + (sx >> 3) * tile_step + (sy >> 3) * obj_tile_row_stride) & 0x3ff;
				}
				if (tile < 512 && bg_mode >= 3 && bg_mode <= 5)
					continue;

				INT32 cache_key = tile * 16 + ty;
				if (cache_key != obj_cached_key) {
					obj_cached_key = cache_key;
					INT32 obj_tile_base = GBA_OBJ_TILES0_2;
					if (!colors_or_palettes) {
						const UINT32* src32 = (const UINT32*)(ctx->vram + obj_tile_base + ((tile * 32 + ty * 4) & 0x7fff));
						UINT32 row32 = BURN_ENDIAN_SWAP_INT32(*src32);
						obj_cached_row[0] = (UINT8)(row32);
						obj_cached_row[1] = (UINT8)(row32 >> 8);
						obj_cached_row[2] = (UINT8)(row32 >> 16);
						obj_cached_row[3] = (UINT8)(row32 >> 24);
					} else {
						const UINT64* src64 = (const UINT64*)(ctx->vram + obj_tile_base + ((tile * 32 + ty * 8) & 0x7fff));
						UINT64 row64 = BURN_ENDIAN_SWAP_INT64(*src64);
						obj_cached_row[0] = (UINT8)row64;
						obj_cached_row[1] = (UINT8)(row64 >> 8);
						obj_cached_row[2] = (UINT8)(row64 >> 16);
						obj_cached_row[3] = (UINT8)(row64 >> 24);
						obj_cached_row[4] = (UINT8)(row64 >> 32);
						obj_cached_row[5] = (UINT8)(row64 >> 40);
						obj_cached_row[6] = (UINT8)(row64 >> 48);
						obj_cached_row[7] = (UINT8)(row64 >> 56);
					}
				}

				UINT8 palette_id;
				bool  transparent = false;
				if (!colors_or_palettes) {
					UINT8 tile_byte = obj_cached_row[tx >> 1];
					palette_id = (tile_byte >> ((tx & 1) * 4)) & 0xf;
					transparent = (palette_id == 0);
					palette_id += pal_offset;
				} else {
					palette_id = obj_cached_row[tx];
					transparent = (palette_id == 0);
				}

				if (obj_mode == 2 && !transparent && obj_window_enable) {
					ctx->window[x] = obj_window_control;
				} else if (obj_mode != 3) {
					UINT32 col;
					if (transparent) {
						col = ((UINT32)(5 - priority) << 29) | ((UINT32)7 << 26);
					} else {
						col = obj_pri_col | (pal_obj_xrgb[palette_id] & 0x00FFFFFFu);
						if (obj_mode == 1)
							col |= (1u << 25);
					}
					if (((col >> 26) & 0x3F) > (((*ft_p) >> 26) & 0x3F)) {
						if (transparent) {
							// Transparent OBJ must not overwrite a drawn BG/OBJ pixel
							// (matches gbappu.h `type != 5`). Replace priority+tie
							// (bits 26-31) only; keep flags/BGR (gbappu.h keeps 0x0FFFFFFF).
							if ((((*ft_p) >> 26) & 7) != 0)
								*ft_p = ((*ft_p) & 0x03FFFFFFu) | (col & 0xFC000000u);
						} else {
							*ft_p = col;
						}
					}
				}
			}
		}
	}

	// WIN0/WIN1 rectangles - render WIN1 first so WIN0 wins on overlap.
	if (!ctx->no_windows && win_en) {
		for (INT32 win = 1; win >= 0; --win) {
			if (!SB_BFE(dispcnt, 13 + win, 1))
				continue;
			UINT16 WINH = ppu_worker_io_read16(ctx, GBA_WIN0H + 2 * win);
			UINT16 WINV = ppu_worker_io_read16(ctx, GBA_WIN0V + 2 * win);
			INT32  win_xmin = SB_BFE(WINH, 8, 8);  // x1 (left edge, high byte)
			INT32  win_xmax = SB_BFE(WINH, 0, 8);  // x2 (right edge, low byte)
			INT32  win_ymin = SB_BFE(WINV, 8, 8);
			INT32  win_ymax = SB_BFE(WINV, 0, 8);
			// Match gbappu.h: Y2>160 or Y1>Y2 -> Y2=160
			if (win_xmin > win_xmax)
				win_xmax = 240;
			if (win_ymin > win_ymax)
				win_ymax = 160;
			if (win_xmax > 240)
				win_xmax = 240;
			if (win_ymax > 160)
				win_ymax = 160;
			if (sprite_lcd_y < win_ymin || sprite_lcd_y >= win_ymax)
				continue;
			UINT16 winin     = ppu_worker_io_read16(ctx, GBA_WININ);
			UINT8  win_value = SB_BFE(winin, win * 8, 6);
			// Single segment [xmin, xmax); x1==x2 => zero width, skip.
			for (INT32 x = win_xmin; x < win_xmax; ++x)
				ctx->window[x] = win_value;
		}
		// Window regions with OBJ disabled -> backdrop (matches gbappu.h).
		UINT32 backdrop_col = (ctx->pal_xrgb32[0] & 0x00FFFFFFu) | ((UINT32)0 << 26) | (1u << 24);
		UINT32* SB_RESTRICT ft_win = ctx->first_target_buffer;
		UINT8*  SB_RESTRICT win_arr = ctx->window;
		for (INT32 x = 0; x < 240; ++x) {
			if (SB_BFE(win_arr[x], 4, 1) == 0)
				ft_win[x] = backdrop_col;
		}
	}

	// second_target_buffer is needed for alpha blend or any semi-transparent OBJ.
	ctx->need_second_buf = (bld_mode_nb == 1) || (n_semitrans_objs > 0) || obj_window_enable;
	if (ctx->eff_bld_mode == 0)
		ctx->need_second_buf = (n_semitrans_objs > 0) || obj_window_enable;
	return n_hits > 0;
}

// Max cycles to skip from current beam position (worker-side copy).
static inline INT32 ppu_worker_compute_max_fast_forward(ppu_worker_render_ctx_t* ctx, bool render)
{
	INT32 scanline_clock = (ctx->ppu->scan_clock) % 1232;
	if (scanline_clock >= GBA_LCD_HBLANK_START * 4 && scanline_clock <= GBA_LCD_HBLANK_END * 4)
		return GBA_LCD_HBLANK_END   * 4 - scanline_clock - 1;
	bool not_visible = !render || ctx->ppu->scan_clock > GBA_LCD_VBLANK_START;
	if (not_visible && scanline_clock >= 1 && scanline_clock <= GBA_LCD_HBLANK_START * 4)
		return GBA_LCD_HBLANK_START * 4 - scanline_clock - 1;
	// Past HBLANK_END the line is finished (nothing drawn, last DISPSTAT/VCOUNT
	// update was at HBLANK_END): jump straight to the next line.
	if (scanline_clock > GBA_LCD_HBLANK_END * 4)
		return 1232 - scanline_clock - 1;
	return 3 - ((ctx->ppu->scan_clock) % 4);
}

// Convert one XRGB8888 row to out_bpp (BGR24/RGB565) into dst.
static inline void ppu_worker_write_scanline_out(UINT8* SB_RESTRICT dst,
                                                 const UINT32* SB_RESTRICT src,
                                                 INT32 out_bpp)
{
	if (out_bpp == 3) {
		// BGR24 output; 4-pixel unroll.
		INT32 x = 0;
		for (; x < GBA_LCD_W - 3; x += 4) {
			UINT32 p0 = src[0], p1 = src[1], p2 = src[2], p3 = src[3];
			dst[0]=(UINT8)p0; dst[1]=(UINT8)(p0>>8); dst[2]=(UINT8)(p0>>16);
			dst[3]=(UINT8)p1; dst[4]=(UINT8)(p1>>8); dst[5]=(UINT8)(p1>>16);
			dst[6]=(UINT8)p2; dst[7]=(UINT8)(p2>>8); dst[8]=(UINT8)(p2>>16);
			dst[9]=(UINT8)p3; dst[10]=(UINT8)(p3>>8); dst[11]=(UINT8)(p3>>16);
			src += 4; dst += 12;
		}
		for (; x < GBA_LCD_W; ++x) {
			UINT32 pix = *src++;
			*dst++ = (UINT8)(pix & 0xFF);
			*dst++ = (UINT8)((pix >> 8) & 0xFF);
			*dst++ = (UINT8)((pix >> 16) & 0xFF);
		}
	} else if (out_bpp == 2) {
		// RGB565 output (R=bits19-23, G=10-15, B=3-7); 4-pixel unroll.
		UINT16* SB_RESTRICT dst16 = (UINT16*)dst;
		INT32 x = 0;
		for (; x < GBA_LCD_W - 3; x += 4) {
			UINT32 p0 = src[0], p1 = src[1], p2 = src[2], p3 = src[3];
			dst16[0] = (UINT16)(((p0>>8)&0xF800) | ((p0>>5)&0x07E0) | ((p0>>3)&0x001F));
			dst16[1] = (UINT16)(((p1>>8)&0xF800) | ((p1>>5)&0x07E0) | ((p1>>3)&0x001F));
			dst16[2] = (UINT16)(((p2>>8)&0xF800) | ((p2>>5)&0x07E0) | ((p2>>3)&0x001F));
			dst16[3] = (UINT16)(((p3>>8)&0xF800) | ((p3>>5)&0x07E0) | ((p3>>3)&0x001F));
			src += 4; dst16 += 4;
		}
		for (; x < GBA_LCD_W; ++x) {
			UINT32 pix = *src++;
			*dst16++ = (UINT16)(((pix>>8)&0xF800) | ((pix>>5)&0x07E0) | ((pix>>3)&0x001F));
		}
	}
}

// Scanline render - registers snapshotted at hblank; pixel loop does no IO reads.
// XRGB8888 pixel layout (32-bit UINT32):
//   [31:29] = 5 - priority (higher wins)
//   [28:26] = tie-breaker  (gbappu.h bits 25-27: BG3=1..BG0=4, OBJ=7, BD=0)
//   [25]    = semi-transparent OBJ flag (forced alpha blend)
//   [24]    = opaque flag  (1 = visible; 0 = transparent)
//   [23:0]  = BGR888 color (R<<16, G<<8, B<<0)
// Priority order matches gbappu.h; sb_exp5[] = 5b->8b ((v << 3) | (v >> 2)).
static const UINT8 sb_exp5[32] = {
	0, 8, 16, 24, 33, 41, 49, 57, 66, 74, 82, 90, 99, 107, 115, 123,
	132, 140, 148, 156, 165, 173, 181, 189, 198, 206, 214, 222, 231, 239, 247, 255
};

static inline void ppu_worker_render_scanline(ppu_worker_render_ctx_t* ctx, INT32 lcd_y)
{
	// DISPCNT takes effect 2 scanlines late on hardware; gbappu.h reads
	// dispcnt_pipeline[0], so use ctx->ppu's delayed value, not the raw register.
	UINT16 dispcnt    = ctx->ppu->dispcnt_pipeline[0];
	INT32  bg_mode    = SB_BFE(dispcnt, 0, 3);
	INT32  forced_blank = SB_BFE(dispcnt, 7, 1);
	INT32  frame_sel  = SB_BFE(dispcnt, 4, 1);
	UINT16 mos_reg    = ppu_worker_io_read16(ctx, GBA_MOSAIC);
	INT32  mos_x      = SB_BFE(mos_reg, 0, 4) + 1;
	INT32  mos_y      = SB_BFE(mos_reg, 4, 4) + 1;
	UINT16 bldcnt     = ppu_worker_io_read16(ctx, GBA_BLDCNT);
	// Effective effect mode for this line (0 when the mode cannot apply).
	INT32  eff_mode   = ctx->eff_bld_mode;
	UINT16 bldy_reg   = ppu_worker_io_read16(ctx, GBA_BLDY);
	UINT16 bldalpha   = ppu_worker_io_read16(ctx, GBA_BLDALPHA);
	UINT16 green_swap = ppu_worker_io_read16(ctx, GBA_GREENSWP);
	// Loop-invariant scanline flags hoisted so pixel loops read registers.
	const bool need_second = ctx->need_second_buf;
	const bool no_win      = ctx->no_windows;
	// Backdrop initial fill: pri=0 (lowest), tie=0 (matches gbappu.h BD encoding).
	UINT32 backdrop_col = (ctx->pal_xrgb32[0] & 0x00FFFFFFu) | ((UINT32)0 << 26) | (1u << 24);

	// Blend tables, rebuilt per scanline so HBlank BLDALPHA/BLDY updates apply.
	// Brighten/darken are pre-baked to 8-bit; alpha uses raw 0-16 EVA/EVB with
	// (r*eva + r2*evb)>>4 per pixel, matching gbappu.h's integer divide.
	UINT8 bl_inc8[32], bl_dec8[32];
	INT32 bl_eva_q4, bl_evb_q4;
	bool need_alpha_tbl  = (eff_mode == 1) || need_second;
	bool need_bright_tbl = (eff_mode == 2) || (eff_mode == 3);
	if (need_alpha_tbl) {
		bl_eva_q4 = SB_BFE(bldalpha, 0, 5); if (bl_eva_q4 > 16) bl_eva_q4 = 16;
		bl_evb_q4 = SB_BFE(bldalpha, 8, 5); if (bl_evb_q4 > 16) bl_evb_q4 = 16;
	} else { bl_eva_q4 = 0; bl_evb_q4 = 0; }
	if (need_bright_tbl) {
		INT32 evy = SB_BFE(bldy_reg,  0, 5); if (evy > 16) evy = 16;
		for (INT32 i = 0; i < 32; ++i) {
			INT32 inc = i + (((31 - i) * evy) >> 4);
			if (inc > 31) inc = 31;
			bl_inc8[i] = sb_exp5[inc];
			bl_dec8[i] = sb_exp5[(i * (16 - evy)) >> 4];
		}
	}

	// Ghosting Q8 factor - frame-level constant, stored in ctx.
	INT32 sbf_q8       = ctx->sbf_q8;
	bool  ghost_fast   = ctx->ghost_fast;
	INT32 one_m_sbf_q8 = ctx->one_m_sbf_q8;

	bool render_bgs = bg_mode <= 5;
	bool mode_ok[4]  = { false, false, false, false };
	INT32 priority[4], char_addr[4], scr_addr[4], size_x[4], size_y[4], ssize[4];
	INT32 hoff[4], voff[4], bgx[4], bgy[4], pa[4], pc[4];
	bool  colors[4], mosaic_bg[4], rot_scale[4], overflow[4];
	INT32 py0[4], trow_base[4], cached_tile_x[4], cached_py[4];
	UINT16 cached_tile_data[4];
	// Affine BG tilemap-entry cache (8-bit entries, no flip/palette bits).
	INT32  cached_aff_tile_x[4];
	INT32  cached_aff_tile_y[4];
	UINT8  cached_aff_tile_data[4];
	// BG priority codes - per-BG bits 24-31 (pri/tie/opaque) of composed col.
	UINT32 bg_pri_code[4];

	for (INT32 bg = 0; bg < 4; ++bg) {
		cached_tile_x[bg]     = -1;
		bg_pri_code[bg]       = 0;
		cached_aff_tile_x[bg] = -1;
		cached_aff_tile_y[bg] = -1;
		cached_aff_tile_data[bg] = 0;
	}

	// Per-BG state. Bitmap modes (3/4/5) only use BG2 (FP-B skips BG0/1/3).
	const bool is_bitmap_mode = (bg_mode >= 3 && bg_mode <= 5);
	if (render_bgs) {
		for (INT32 bg = 0; bg < 4; ++bg) {
			if (is_bitmap_mode) {
				// Bitmap mode: BG2 is 256-colour affine over linear VRAM.
				if (bg != 2) continue;
			} else {
				if ((bg < 2 && bg_mode == 2) || (bg == 3 && bg_mode == 1))
					continue;
			}
			if (!SB_BFE(dispcnt, 8 + bg, 1))
				continue;
			mode_ok[bg]   = true;
			rot_scale[bg] = bg_mode >= 1 && bg >= 2;
			UINT16 bgcnt  = ppu_worker_io_read16(ctx, GBA_BG0CNT + bg * 2);
			priority[bg]  = SB_BFE(bgcnt,  0, 2);
			char_addr[bg] = SB_BFE(bgcnt,  2, 2) * 16 * 1024;
			mosaic_bg[bg] = SB_BFE(bgcnt,  6, 1);
			colors[bg]    = SB_BFE(bgcnt,  7, 1);
			scr_addr[bg]  = SB_BFE(bgcnt,  8, 5) * 2048;
			overflow[bg]  = SB_BFE(bgcnt, 13, 1);
			ssize[bg]     = SB_BFE(bgcnt, 14, 2);
			size_x[bg]    = (ssize[bg] & 1 ) ? 512 : 256;
			size_y[bg]    = (ssize[bg] >= 2) ? 512 : 256;
			if (rot_scale[bg] || is_bitmap_mode) {
				size_x[bg] = size_y[bg] = (16 * 8) << ssize[bg];
				if (bg_mode == 3 || bg_mode == 4) {
					size_x[bg] = 240;
					size_y[bg] = 160;
				} else if (bg_mode == 5) {
					size_x[bg] = 160;
					size_y[bg] = 128;
				}
				colors[bg] = true;
				bgx[bg] = ctx->ppu->aff[bg - 2].render_bgx;
				bgy[bg] = ctx->ppu->aff[bg - 2].render_bgy;
				pa[bg]  = (INT16)ppu_worker_io_read16(ctx, GBA_BG2PA + (bg - 2) * 0x10);
				pc[bg]  = (INT16)ppu_worker_io_read16(ctx, GBA_BG2PC + (bg - 2) * 0x10);
			} else {
				INT16 h16 = ppu_worker_io_read16(ctx, GBA_BG0HOFS + bg * 4);
				INT16 v16 = ppu_worker_io_read16(ctx, GBA_BG0VOFS + bg * 4);
				hoff[bg] = (h16 << 7) >> 7;
				voff[bg] = (v16 << 7) >> 7;
				INT32 ly    = mosaic_bg[bg] ? (lcd_y / mos_y) * mos_y : lcd_y;
				INT32 row_y = (voff[bg] + ly) & (size_y[bg] - 1);
				py0[bg]     = row_y & 7;
				INT32 ty    = row_y >> 3;
				trow_base[bg] = (ty & 31) * 32 + (ty >= 32 ? 32 * 32 * (ssize[bg] == 3 ? 2 : 1) : 0);
			}
			bg_pri_code[bg] = ((UINT32)(5 - priority[bg]) << 29)
				                | ((UINT32)(4 - bg) << 26)
			                | (1u << 24);
		}
	}

	UINT32* SB_RESTRICT fb_row_p = ctx->framebuffer + lcd_y * GBA_LCD_W;
	UINT8*  SB_RESTRICT fb_row_b = (UINT8*)fb_row_p;
	UINT32* SB_RESTRICT ft = ctx->first_target_buffer;
	UINT32* SB_RESTRICT st = ctx->second_target_buffer;
	UINT8*  SB_RESTRICT win_p = ctx->window;

	// Active-BG list: iterate only enabled BGs instead of all 4.
	INT32 active_bg[4]; INT32 n_active_bg = 0;
	for (INT32 bg = 0; bg < 4; ++bg)
		if (mode_ok[bg]) active_bg[n_active_bg++] = bg;

	// Fast-pixel path: text mode, no effects/windows/green-swap, ghost_fast.
	// BGs render into ft by priority, then fb gets an 8-pixel copy. FP-D also covers
	// opaque OBJs (pre-drawn, BG tie 0-4 < OBJ tie 7); semi-transparent -> generic.
	const bool fast_pixel_row = !is_bitmap_mode
		&& (eff_mode == 0) && !need_second
		&& no_win && !(green_swap & 1) && ghost_fast
		&& (forced_blank == 0);

	if (fast_pixel_row) {
		// Fill ft with backdrop only when no OBJs: render_objs already seeded ft
		// and drew sprites on top otherwise.
		INT32 xi;
		if (!ctx->has_any_obj_this_line) {
			for (xi = 0; xi < GBA_LCD_W; xi += 8) {
				ft[xi]     = backdrop_col;
				ft[xi + 1] = backdrop_col;
				ft[xi + 2] = backdrop_col;
				ft[xi + 3] = backdrop_col;
				ft[xi + 4] = backdrop_col;
				ft[xi + 5] = backdrop_col;
				ft[xi + 6] = backdrop_col;
				ft[xi + 7] = backdrop_col;
			}
		}

		// Render each active BG; the per-pixel (tie,pri) compare picks the winner.
		for (INT32 bi = n_active_bg - 1; bi >= 0; --bi) {
			INT32 bg = active_bg[bi];
			// BG mode dispatch: rot_scale vs text is determined per BG.
			if (rot_scale[bg] || mosaic_bg[bg] || colors[bg]) {
				// Complex BG (affine/mosaic/8bpp): generic per-pixel logic for this BG only;
				// bg_rot is hoisted so the loop can be unswitched.
				const bool bg_rot = rot_scale[bg];
				UINT32* SB_RESTRICT ft_p = ft;
				for (INT32 lcd_x = 0; lcd_x < 240; ++lcd_x) {
					INT32 bg_x, bg_y;
					if (bg_rot) {
						INT32 sx = mosaic_bg[bg] ? (lcd_x / mos_x) * mos_x : lcd_x;
						bg_x = (INT32)(((INT64)pa[bg] * sx + bgx[bg]) >> 8);
						bg_y = (INT32)(((INT64)pc[bg] * sx + bgy[bg]) >> 8);
						if (overflow[bg] == 0) {
							if (bg_x < 0 || bg_x >= size_x[bg] || bg_y < 0 || bg_y >= size_y[bg])
								{ ++ft_p; continue; }
						} else {
							bg_x &= size_x[bg] - 1;
							bg_y &= size_y[bg] - 1;
						}
					} else {
						if (mosaic_bg[bg]) {
							bg_x = (hoff[bg] + (lcd_x / mos_x) * mos_x) & (size_x[bg] - 1);
							bg_y = (voff[bg] + (lcd_y / mos_y) * mos_y) & (size_y[bg] - 1);
						} else {
							bg_x = (hoff[bg] + lcd_x) & (size_x[bg] - 1);
							bg_y = (voff[bg] + lcd_y) & (size_y[bg] - 1);
						}
					}
					INT32 bg_tile_x = bg_x >> 3;
					UINT16 tile_data; INT32 px, py;
					if (bg_rot) {
						INT32 bg_ty = bg_y >> 3, bg_tiles_w = size_x[bg] >> 3;
						INT32 tile_off = bg_ty * bg_tiles_w + bg_tile_x;
						if (bg_tile_x != cached_aff_tile_x[bg] || bg_ty != cached_aff_tile_y[bg]) {
							cached_aff_tile_x[bg] = bg_tile_x;
							cached_aff_tile_y[bg] = bg_ty;
							cached_aff_tile_data[bg] = ctx->vram[scr_addr[bg] + tile_off];
						}
						tile_data = cached_aff_tile_data[bg];
						px = bg_x & 7; py = bg_y & 7;
					} else {
						if (bg_tile_x != cached_tile_x[bg]) {
							cached_tile_x[bg] = bg_tile_x;
							INT32 toff = trow_base[bg] + (bg_tile_x & 31) + (bg_tile_x >= 32 ? 32*32 : 0);
							tile_data = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + scr_addr[bg] + toff * 2));
							cached_tile_data[bg] = tile_data;
							cached_py[bg] = SB_BFE(tile_data,11,1) ? 7 - py0[bg] : py0[bg];
						} else { tile_data = cached_tile_data[bg]; }
						px = bg_x & 7;
						if (SB_BFE(tile_data,10,1)) px = 7 - px;
						py = cached_py[bg];
					}
					INT32 tile_id = SB_BFE(tile_data,0,10);
					INT32 palette = SB_BFE(tile_data,12,4);
					UINT8 tile_d;
					if (!colors[bg]) {
						INT32 addr = char_addr[bg] + tile_id*32 + py*4;
						// VRAM addr >= 0x10000: skip pixel (matches gbappu.h).
						if (SB_UNLIKELY(addr >= 0x10000)) { ++ft_p; continue; }
						if (!ctx->fc_tile_valid[bg] || ctx->fc_tile_key[bg] != addr) {
							ctx->fc_tile_key[bg] = addr;
							UINT32 r32 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(ctx->vram + addr));
							ctx->fc_tile_bytes[bg][0]=(UINT8)r32;
							ctx->fc_tile_bytes[bg][1]=(UINT8)(r32>>8);
							ctx->fc_tile_bytes[bg][2]=(UINT8)(r32>>16);
							ctx->fc_tile_bytes[bg][3]=(UINT8)(r32>>24);
							ctx->fc_tile_valid[bg] = true;
						}
						tile_d = (ctx->fc_tile_bytes[bg][px>>1]>>((px&1)*4))&0xf;
						if (tile_d == 0) { ++ft_p; continue; }
						tile_d += palette*16;
					} else {
						INT32 addr = char_addr[bg] + tile_id*64 + py*8;
						if (SB_UNLIKELY(addr >= 0x10000)) { ++ft_p; continue; }
						if (!ctx->fc_tile_valid[bg] || ctx->fc_tile_key[bg] != addr) {
							ctx->fc_tile_key[bg] = addr;
							UINT64 r64 = BURN_ENDIAN_SWAP_INT64(*(const UINT64*)(ctx->vram + addr));
							ctx->fc_tile_bytes[bg][0]=(UINT8)r64; ctx->fc_tile_bytes[bg][1]=(UINT8)(r64>>8);
							ctx->fc_tile_bytes[bg][2]=(UINT8)(r64>>16); ctx->fc_tile_bytes[bg][3]=(UINT8)(r64>>24);
							ctx->fc_tile_bytes[bg][4]=(UINT8)(r64>>32); ctx->fc_tile_bytes[bg][5]=(UINT8)(r64>>40);
							ctx->fc_tile_bytes[bg][6]=(UINT8)(r64>>48); ctx->fc_tile_bytes[bg][7]=(UINT8)(r64>>56);
							ctx->fc_tile_valid[bg] = true;
						}
						tile_d = ctx->fc_tile_bytes[bg][px];
						if (tile_d == 0) { ++ft_p; continue; }
					}
					UINT32 col2 = (ctx->pal_xrgb32[tile_d] & 0x00FFFFFFu) | bg_pri_code[bg];
					if (((col2 >> 26) & 0x3F) > (((*ft_p) >> 26) & 0x3F)) *ft_p = col2;
					++ft_p;
				}
			} else {
				// FP-E: simple 4bpp text BG. Full tiles use one 4-byte VRAM read + 8-pixel
				// unrolled decode (h-flip via nibble-swap); head/tail tiles stay per-pixel.
				UINT32* SB_RESTRICT ft_p = ft;
				INT32 lcd_x = 0;

				// 1) Head: 0-7 pixels until tile boundary (px==0)
				{
					INT32 start_px = (hoff[bg] + 0) & 7;
					if (start_px != 0) {
						INT32 head_count = 8 - start_px;
						if (head_count > 240) head_count = 240;
						for (INT32 i = 0; i < head_count; ++i) {
							INT32 bg_x = (hoff[bg] + lcd_x) & (size_x[bg] - 1);
							INT32 bg_tile_x = bg_x >> 3, px = bg_x & 7;
							UINT16 tile_data; INT32 py;
							if (bg_tile_x != cached_tile_x[bg]) {
								cached_tile_x[bg] = bg_tile_x;
								INT32 toff = trow_base[bg] + (bg_tile_x & 31) + (bg_tile_x >= 32 ? 32*32 : 0);
								tile_data = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + scr_addr[bg] + toff * 2));
								cached_tile_data[bg] = tile_data;
								cached_py[bg] = SB_BFE(tile_data,11,1) ? 7 - py0[bg] : py0[bg];
							} else { tile_data = cached_tile_data[bg]; }
							bool hflip = SB_BFE(tile_data,10,1);
							if (hflip) px = 7 - px;
							py = cached_py[bg];
							INT32 tile_id = SB_BFE(tile_data,0,10);
							INT32 palette = SB_BFE(tile_data,12,4);
							INT32 addr = char_addr[bg] + tile_id*32 + py*4;
							if (SB_LIKELY(addr < 0x10000)) {
								if (!ctx->fc_tile_valid[bg] || ctx->fc_tile_key[bg] != addr) {
									ctx->fc_tile_key[bg] = addr;
									UINT32 r32 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(ctx->vram + addr));
									ctx->fc_tile_bytes[bg][0]=(UINT8)r32;
									ctx->fc_tile_bytes[bg][1]=(UINT8)(r32>>8);
									ctx->fc_tile_bytes[bg][2]=(UINT8)(r32>>16);
									ctx->fc_tile_bytes[bg][3]=(UINT8)(r32>>24);
									ctx->fc_tile_valid[bg] = true;
								}
								UINT8 tile_d = (ctx->fc_tile_bytes[bg][px>>1]>>((px&1)*4))&0xf;
								if (tile_d != 0) {
									tile_d += palette*16;
									UINT32 col = (ctx->pal_xrgb32[tile_d] & 0x00FFFFFFu) | bg_pri_code[bg];
									if (((col >> 26) & 0x3F) > (((*ft_p) >> 26) & 0x3F)) *ft_p = col;
								}
							}
							++ft_p; ++lcd_x;
						}
					}
				}

				// 2) Main: 8-pixel aligned tile blocks
				for (; lcd_x <= 240 - 8; lcd_x += 8) {
					INT32 bg_x_base = (hoff[bg] + lcd_x) & (size_x[bg] - 1);
					INT32 bg_tile_x = bg_x_base >> 3;
					UINT16 tile_data; INT32 py;
					if (bg_tile_x != cached_tile_x[bg]) {
						cached_tile_x[bg] = bg_tile_x;
						INT32 toff = trow_base[bg] + (bg_tile_x & 31) + (bg_tile_x >= 32 ? 32*32 : 0);
						tile_data = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + scr_addr[bg] + toff * 2));
						cached_tile_data[bg] = tile_data;
						cached_py[bg] = SB_BFE(tile_data,11,1) ? 7 - py0[bg] : py0[bg];
					} else { tile_data = cached_tile_data[bg]; }

					bool hflip = SB_BFE(tile_data,10,1);
					py = cached_py[bg];
					INT32 tile_id = SB_BFE(tile_data,0,10);
					INT32 palette = SB_BFE(tile_data,12,4);
					INT32 addr = char_addr[bg] + tile_id*32 + py*4;

					if (SB_UNLIKELY(addr >= 0x10000)) { ft_p += 8; continue; }

					// Fetch tile bytes (VRAM byte order) with single-addr cache.
					bool tile_is_zero = false;
					if (!ctx->fc_tile_valid[bg] || ctx->fc_tile_key[bg] != addr) {
						ctx->fc_tile_key[bg] = addr;
						UINT32 r32 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(ctx->vram + addr));
						ctx->fc_tile_bytes[bg][0]=(UINT8)r32;
						ctx->fc_tile_bytes[bg][1]=(UINT8)(r32>>8);
						ctx->fc_tile_bytes[bg][2]=(UINT8)(r32>>16);
						ctx->fc_tile_bytes[bg][3]=(UINT8)(r32>>24);
						ctx->fc_tile_valid[bg] = true;
						if (r32 == 0) tile_is_zero = true;
					}
					if (tile_is_zero) { ft_p += 8; continue; }
					if (ctx->fc_tile_bytes[bg][0]==0 && ctx->fc_tile_bytes[bg][1]==0
					 && ctx->fc_tile_bytes[bg][2]==0 && ctx->fc_tile_bytes[bg][3]==0) {
						ft_p += 8; continue;
					}

					if (SB_UNLIKELY(hflip)) {
						// h-flip: nibble-swap + byte-reverse, then the 8-pixel unrolled decode.
						const UINT8* SB_RESTRICT rb = ctx->fc_tile_bytes[bg];
						UINT8 b0 = rb[0], b1 = rb[1], b2 = rb[2], b3 = rb[3];
						UINT8 fb[4];
						fb[0] = (UINT8)(((b3 & 0x0F) << 4) | (b3 >> 4));
						fb[1] = (UINT8)(((b2 & 0x0F) << 4) | (b2 >> 4));
						fb[2] = (UINT8)(((b1 & 0x0F) << 4) | (b1 >> 4));
						fb[3] = (UINT8)(((b0 & 0x0F) << 4) | (b0 >> 4));
						UINT32 pal_bank = (UINT32)(palette * 16);
						UINT32 pri = bg_pri_code[bg];
						const UINT32* SB_RESTRICT pal = ctx->pal_xrgb32;
						#define FP_E_HFPX(px_) do { \
							UINT8 t = (fb[(px_)>>1] >> (((px_)&1)*4)) & 0xf; \
							if (t != 0) { \
								UINT32 c = (pal[t + pal_bank] & 0x00FFFFFFu) | pri; \
								if (((c >> 26) & 0x3F) > (((*ft_p) >> 26) & 0x3F)) *ft_p = c; \
							} \
							++ft_p; \
						} while(0)
						FP_E_HFPX(0); FP_E_HFPX(1); FP_E_HFPX(2); FP_E_HFPX(3);
						FP_E_HFPX(4); FP_E_HFPX(5); FP_E_HFPX(6); FP_E_HFPX(7);
						#undef FP_E_HFPX
					} else {
						// FP-E fast: 8 pixels from cached raw bytes, unrolled.
						UINT32 pal_bank = (UINT32)(palette * 16);
						UINT32 pri = bg_pri_code[bg];
						const UINT32* SB_RESTRICT pal = ctx->pal_xrgb32;
						const UINT8* SB_RESTRICT fb_r = ctx->fc_tile_bytes[bg];
						#define FP_E_NPX(px_) do { \
							UINT8 t = (fb_r[(px_)>>1] >> (((px_)&1)*4)) & 0xf; \
							if (t != 0) { \
								UINT32 c = (pal[t + pal_bank] & 0x00FFFFFFu) | pri; \
								if (((c >> 26) & 0x3F) > (((*ft_p) >> 26) & 0x3F)) *ft_p = c; \
							} \
							++ft_p; \
						} while(0)
						FP_E_NPX(0); FP_E_NPX(1); FP_E_NPX(2); FP_E_NPX(3);
						FP_E_NPX(4); FP_E_NPX(5); FP_E_NPX(6); FP_E_NPX(7);
						#undef FP_E_NPX
					}
				}

				// 3) Tail: remaining 0-7 pixels
				for (; lcd_x < 240; ++lcd_x) {
					INT32 bg_x = (hoff[bg] + lcd_x) & (size_x[bg] - 1);
					INT32 bg_tile_x = bg_x >> 3, px = bg_x & 7;
					UINT16 tile_data; INT32 py;
					if (bg_tile_x != cached_tile_x[bg]) {
						cached_tile_x[bg] = bg_tile_x;
						INT32 toff = trow_base[bg] + (bg_tile_x & 31) + (bg_tile_x >= 32 ? 32*32 : 0);
						tile_data = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + scr_addr[bg] + toff * 2));
						cached_tile_data[bg] = tile_data;
						cached_py[bg] = SB_BFE(tile_data,11,1) ? 7 - py0[bg] : py0[bg];
					} else { tile_data = cached_tile_data[bg]; }
					bool hflip = SB_BFE(tile_data,10,1);
					if (hflip) px = 7 - px;
					py = cached_py[bg];
					INT32 tile_id = SB_BFE(tile_data,0,10);
					INT32 palette = SB_BFE(tile_data,12,4);
					INT32 addr = char_addr[bg] + tile_id*32 + py*4;
					if (SB_LIKELY(addr < 0x10000)) {
						if (!ctx->fc_tile_valid[bg] || ctx->fc_tile_key[bg] != addr) {
							ctx->fc_tile_key[bg] = addr;
							UINT32 r32 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(ctx->vram + addr));
							ctx->fc_tile_bytes[bg][0]=(UINT8)r32;
							ctx->fc_tile_bytes[bg][1]=(UINT8)(r32>>8);
							ctx->fc_tile_bytes[bg][2]=(UINT8)(r32>>16);
							ctx->fc_tile_bytes[bg][3]=(UINT8)(r32>>24);
							ctx->fc_tile_valid[bg] = true;
						}
						UINT8 tile_d = (ctx->fc_tile_bytes[bg][px>>1]>>((px&1)*4))&0xf;
						if (tile_d != 0) {
							tile_d += palette*16;
							UINT32 col = (ctx->pal_xrgb32[tile_d] & 0x00FFFFFFu) | bg_pri_code[bg];
							if (((col >> 26) & 0x3F) > (((*ft_p) >> 26) & 0x3F)) *ft_p = col;
						}
					}
					++ft_p;
				}
			}
		}

		// Copy ft to framebuffer, stripping the top-byte flags (pure XRGB8888).
		UINT32* SB_RESTRICT fbs = fb_row_p;
		UINT32* SB_RESTRICT fts = ft;
		for (xi = 0; xi < 240; xi += 8) {
			fbs[0] = fts[0] & 0x00FFFFFFu;
			fbs[1] = fts[1] & 0x00FFFFFFu;
			fbs[2] = fts[2] & 0x00FFFFFFu;
			fbs[3] = fts[3] & 0x00FFFFFFu;
			fbs[4] = fts[4] & 0x00FFFFFFu;
			fbs[5] = fts[5] & 0x00FFFFFFu;
			fbs[6] = fts[6] & 0x00FFFFFFu;
			fbs[7] = fts[7] & 0x00FFFFFFu;
			fbs += 8; fts += 8;
		}
	} else {
		UINT32* SB_RESTRICT ft_p = ft;
		UINT32* SB_RESTRICT st_p = st;
		UINT8*  SB_RESTRICT win_pp = win_p;
		for (INT32 lcd_x = 0; lcd_x < GBA_LCD_W; ++lcd_x) {
		UINT8 window_control = no_win ? (UINT8)0x3F : *win_pp;
		bool  any_bg_visible = render_bgs && (window_control & 0x0F) != 0;

		if (any_bg_visible) {
			for (INT32 bi = n_active_bg - 1; bi >= 0; --bi) {
				INT32 bg = active_bg[bi];
				if (SB_BFE(window_control, bg, 1) == 0)
					continue;
				UINT32 col  = 0;
				INT32  bg_x = 0, bg_y = 0;

				if (rot_scale[bg]) {
					INT32 sx = mosaic_bg[bg] ? (lcd_x / mos_x) * mos_x : lcd_x;
					bg_x = (INT32)(((INT64)pa[bg] * sx + bgx[bg]) >> 8);
					bg_y = (INT32)(((INT64)pc[bg] * sx + bgy[bg]) >> 8);
					if (overflow[bg] == 0) {
						if (bg_x < 0 || bg_x >= size_x[bg] || bg_y < 0 || bg_y >= size_y[bg])
							continue;
					} else {
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

				if (is_bitmap_mode) {
					if (bg_mode == 3) {
						UINT16 c16 = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + (bg_x + bg_y * 240) * 2));
						if (c16 == 0x8000) continue;
						{ UINT32 rb=c16&0x1F,gb=(c16>>5)&0x1F,bb=(c16>>10)&0x1F;
						  col = ((rb<<3)|(rb>>2)) | (((gb<<3)|(gb>>2))<<8) | (((bb<<3)|(bb>>2))<<16); }
					} else if (bg_mode == 4) {
						UINT8 palette_id = ctx->vram[bg_x + bg_y * 240 + 0xa000 * frame_sel];
						if (palette_id == 0) continue;
						col = ctx->pal_xrgb32[palette_id] & 0x00FFFFFFu;
					} else { // bg_mode == 5
						UINT16 c16 = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + (bg_x + bg_y * 160) * 2 + 0xa000 * frame_sel));
						if (c16 == 0x8000) continue;
						{ UINT32 rb=c16&0x1F,gb=(c16>>5)&0x1F,bb=(c16>>10)&0x1F;
						  col = ((rb<<3)|(rb>>2)) | (((gb<<3)|(gb>>2))<<8) | (((bb<<3)|(bb>>2))<<16); }
					}
				} else {
					// Text BG: wrap coords into the BG map (mirrors fast-pixel branch).
					bg_x = bg_x & (size_x[bg] - 1);
					bg_y = bg_y & (size_y[bg] - 1);
					INT32 bg_tile_x = bg_x >> 3;

					UINT16 tile_data;
					INT32 px, py;
					if (rot_scale[bg]) {
						INT32 bg_ty      = bg_y >> 3;
						INT32 bg_tiles_w = size_x[bg] >> 3;
						INT32 tile_off   = bg_ty * bg_tiles_w + bg_tile_x;
						if (bg_tile_x != cached_aff_tile_x[bg] || bg_ty != cached_aff_tile_y[bg]) {
							cached_aff_tile_x[bg] = bg_tile_x;
							cached_aff_tile_y[bg] = bg_ty;
							cached_aff_tile_data[bg] = ctx->vram[scr_addr[bg] + tile_off];
						}
						tile_data = cached_aff_tile_data[bg];
						px = bg_x & 7;
						py = bg_y & 7;
					} else {
						if (bg_tile_x != cached_tile_x[bg]) {
							cached_tile_x[bg] = bg_tile_x;
							INT32 toff = trow_base[bg] + (bg_tile_x & 31) + (bg_tile_x >= 32 ? 32 * 32 : 0);
							tile_data = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + scr_addr[bg] + toff * 2));
							cached_tile_data[bg] = tile_data;
							cached_py[bg] = SB_BFE(tile_data, 11, 1) ? 7 - py0[bg] : py0[bg];
							ctx->cached_pal_bank[bg] = SB_BFE(tile_data, 12, 4) * 16;  // pal_bank cached at tile boundary
						} else {
							tile_data = cached_tile_data[bg];
						}
						px = bg_x & 7;
						if (SB_BFE(tile_data, 10, 1)) px = 7 - px;
						py = cached_py[bg];
					}

					INT32 tile_id = SB_BFE(tile_data,  0, 10);
					UINT8 tile_d;
					// affine BGs force 8bpp (colors[bg]=true); pal_bank is unused in that branch.
					INT32 pal_bank_g = rot_scale[bg] ? 0 : ctx->cached_pal_bank[bg];

					if (!colors[bg]) {
						// 4bpp: 32 bytes/tile, 4 bytes/row
						INT32 addr = char_addr[bg] + tile_id * 32 + py * 4;
						if (SB_UNLIKELY(addr >= 0x10000))
							continue;
						if (!ctx->fc_tile_valid[bg] || ctx->fc_tile_key[bg] != addr) {
							ctx->fc_tile_key[bg] = addr;
							UINT32 r32 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(ctx->vram + addr));
							ctx->fc_tile_bytes[bg][0] = (UINT8)(r32);
							ctx->fc_tile_bytes[bg][1] = (UINT8)(r32 >> 8);
							ctx->fc_tile_bytes[bg][2] = (UINT8)(r32 >> 16);
							ctx->fc_tile_bytes[bg][3] = (UINT8)(r32 >> 24);
							ctx->fc_tile_valid[bg] = true;
						}
						tile_d = (ctx->fc_tile_bytes[bg][px >> 1] >> ((px & 1) * 4)) & 0xf;
						if (tile_d == 0) continue;
						tile_d += pal_bank_g;
					} else {
						// 8bpp: 64 bytes/tile, 8 bytes/row
						INT32 addr = char_addr[bg] + tile_id * 64 + py * 8;
						if (SB_UNLIKELY(addr >= 0x10000))
							continue;
						if (!ctx->fc_tile_valid[bg] || ctx->fc_tile_key[bg] != addr) {
							ctx->fc_tile_key[bg] = addr;
							UINT64 r64 = BURN_ENDIAN_SWAP_INT64(*(const UINT64*)(ctx->vram + addr));
							ctx->fc_tile_bytes[bg][0] = (UINT8)r64;
							ctx->fc_tile_bytes[bg][1] = (UINT8)(r64 >> 8);
							ctx->fc_tile_bytes[bg][2] = (UINT8)(r64 >> 16);
							ctx->fc_tile_bytes[bg][3] = (UINT8)(r64 >> 24);
							ctx->fc_tile_bytes[bg][4] = (UINT8)(r64 >> 32);
							ctx->fc_tile_bytes[bg][5] = (UINT8)(r64 >> 40);
							ctx->fc_tile_bytes[bg][6] = (UINT8)(r64 >> 48);
							ctx->fc_tile_bytes[bg][7] = (UINT8)(r64 >> 56);
							ctx->fc_tile_valid[bg] = true;
						}
						tile_d = ctx->fc_tile_bytes[bg][px];
						if (tile_d == 0) continue;
					}
					col = ctx->pal_xrgb32[tile_d] & 0x00FFFFFFu;
				}
				col |= ((UINT32)(5 - priority[bg]) << 29) | ((UINT32)(4 - bg) << 26) | (1u << 24);

				if (((col >> 26) & 0x3F) > ((*ft_p >> 26) & 0x3F)) {
					UINT32 t = *ft_p;
					*ft_p = col;
					col = t;
				}
				if (need_second && ((col >> 26) & 0x3F) > ((*st_p >> 26) & 0x3F))
					*st_p = col;
			}
		}

		// Color decode + special effects + framebuffer write. tie_to_type[] maps
		// the XRGB tie-code to its BLDCNT bit; BGs use tie (4-bg), matching gbappu.h.
		UINT32 col  = *ft_p;
		static const UINT8 tie_to_type[8] = {5, 3, 2, 1, 0, 0, 0, 4};
		INT32  b8   = (INT32)(col & 0xFF);
		INT32  g8   = (INT32)((col >> 8)  & 0xFF);
		INT32  r8   = (INT32)((col >> 16) & 0xFF);
		UINT32 type = tie_to_type[(col >> 26) & 7];

		INT32 mode = eff_mode;
		// Special-effect enable: window bit 5 AND BLDCNT first-target mask.
		bool  effect_enable = SB_BFE(window_control, 5, 1);

		if ((col >> 25) & 1) {
			// Semi-transparent OBJ: alpha-blend with second-target pixel in st.
			UINT32 col2  = *st_p;
			UINT32 type2 = tie_to_type[(col2 >> 26) & 7];
			bool   blend = SB_BFE(bldcnt, 8 + type2, 1);
			if (blend) {
				mode          = 1;
				effect_enable = true;
			} else {
				effect_enable &= SB_BFE(bldcnt, type, 1);
			}
		} else {
			effect_enable &= SB_BFE(bldcnt, type, 1);
		}

		if (effect_enable) {
			INT32 r = r8 >> 3, g = g8 >> 3, b = b8 >> 3;
			switch (mode) {
				case 1: {
					UINT32 col2  = *st_p;
					UINT32 type2 = tie_to_type[(col2 >> 26) & 7];
					bool  blend  = SB_BFE(bldcnt, 8 + type2, 1);
					if (blend) {
						INT32 b2 = (INT32)(col2 & 0xFF) >> 3;
						INT32 g2 = (INT32)((col2 >> 8) & 0xFF) >> 3;
						INT32 r2 = (INT32)((col2 >> 16) & 0xFF) >> 3;
						r = (r * bl_eva_q4 + r2 * bl_evb_q4) >> 4;
						g = (g * bl_eva_q4 + g2 * bl_evb_q4) >> 4;
						b = (b * bl_eva_q4 + b2 * bl_evb_q4) >> 4;
						if (r > 31) r = 31;
						if (g > 31) g = 31;
						if (b > 31) b = 31;
						r8 = sb_exp5[r];
						g8 = sb_exp5[g];
						b8 = sb_exp5[b];
					}
				}
					break;
				case 2:
					r8 = bl_inc8[r];
					g8 = bl_inc8[g];
					b8 = bl_inc8[b];
					break;
				case 3:
					r8 = bl_dec8[r];
					g8 = bl_dec8[g];
					b8 = bl_dec8[b];
					break;
			}
		}

		if (forced_blank) {
			if (ctx->stop_mode) { r8 = g8 = b8 = 0; }
			else                { r8 = g8 = b8 = 255; }
		}

		INT32 rr = r8;
		INT32 gg = g8;
		INT32 bb = b8;

		if (green_swap & 1) {
			// Green-swap: G written to adjacent pixel's G byte
			UINT8* SB_RESTRICT fb_b = fb_row_b;
			INT32 p = lcd_x * 4;
			if (ghost_fast) {
				fb_b[p + 0] = (UINT8)bb;
				fb_b[p + 2] = (UINT8)rr;
				fb_b[p + 3] = 0x00;
				INT32 g_off = (p & 4) ? (p + 1 - 4) : (p + 1 + 4);
				fb_b[g_off] = (UINT8)gg;
			} else {
				INT32 old_b = (INT32)(UINT8)fb_b[p + 0];
				INT32 old_r = (INT32)(UINT8)fb_b[p + 2];
				fb_b[p + 0] = (UINT8)((bb * one_m_sbf_q8 + old_b * sbf_q8) >> 8);
				fb_b[p + 2] = (UINT8)((rr * one_m_sbf_q8 + old_r * sbf_q8) >> 8);
				fb_b[p + 3] = 0x00;
				INT32 g_off = (p & 4) ? (p + 1 - 4) : (p + 1 + 4);
				INT32 old_g = (INT32)(UINT8)fb_b[g_off];
				fb_b[g_off] = (UINT8)((gg * one_m_sbf_q8 + old_g * sbf_q8) >> 8);
			}
		} else if (ghost_fast) {
			fb_row_p[lcd_x] = ((UINT32)rr << 16) | ((UINT32)gg << 8) | (UINT32)bb;
		} else {
			UINT32 old_pix = fb_row_p[lcd_x];
			INT32 old_b = (INT32)(old_pix & 0xFF);
			INT32 old_g = (INT32)((old_pix >> 8) & 0xFF);
			INT32 old_r = (INT32)((old_pix >> 16) & 0xFF);
			INT32 nb = (bb * one_m_sbf_q8 + old_b * sbf_q8) >> 8;
			INT32 ng = (gg * one_m_sbf_q8 + old_g * sbf_q8) >> 8;
			INT32 nr = (rr * one_m_sbf_q8 + old_r * sbf_q8) >> 8;
			fb_row_p[lcd_x] = ((UINT32)nr << 16) | ((UINT32)ng << 8) | (UINT32)nb;
		}
		++ft_p; ++st_p; ++win_pp;
		}
	}

	// Per-scanline output: convert after the pixel loop (green_swap pairs pixels).
	if (ctx->out_ptr) {
		UINT8* SB_RESTRICT dst = ctx->out_ptr + lcd_y * GBA_LCD_W * ctx->out_bpp;
		const UINT32* SB_RESTRICT src = ctx->framebuffer + lcd_y * GBA_LCD_W;
		ppu_worker_write_scanline_out(dst, src, ctx->out_bpp);
	}
}

// OPT1: with ghosting off and out_bpp==4, memcpy the previous backbuf when the
// snapshot matches the presented frame (three-stage reject: control key,
// line_state, VRAM). Needs triple-buffering, else memcmp/memcpy races capture.
static inline bool ppu_worker_try_reuse_frame(ppu_worker_snapshot_t* snap,
                                               UINT32* out_fb,
                                               ppu_worker_t* w, INT32 ridx)
{
	if (snap->ghosting_strength != 0.0f) return false; // ghosting seeds from prev backbuf
	if (w->out_bpp != 4)   return false;                // backbuf is XRGB8888 only
	if (!w->prev_sig_valid) return false;               // no cached frame yet
	int front = w->front_idx;
	if (front < 0 || front == ridx) return false;       // no stable previous slot
	ppu_worker_snapshot_t* prev = &w->snapshots[front];
	// stage1: control key + scalars / stage2: line_state / stage3: VRAM
	if (memcmp(snap->line_state[0].io, w->prev_key, 86) != 0) return false;
	if (snap->ghosting_strength != prev->ghosting_strength) return false;
	if (snap->stop_mode != prev->stop_mode) return false;
	if (memcmp(snap->line_state, prev->line_state, sizeof(snap->line_state)) != 0) return false;
	// A per-line VRAM journal means the frame is not a plain copy of the previous
	// one even when the whole-frame snapshots match.
	if (snap->vram_journal || prev->vram_journal) return false;
	if (memcmp(snap->vram, prev->vram, 128 * 1024) != 0) return false;
	// Identical inputs: memcpy the previous backbuf.
	memcpy(out_fb, w->backbuf[front], GBA_LCD_W * GBA_LCD_H * 4);
	return true;
}

// Render a frame from snapshot into backbuf+out_ptr (if out_bpp!=4).
// Returns true on full render, false if cancelled mid-frame.
static bool ppu_worker_render_frame_into(ppu_worker_snapshot_t* snap, UINT32* out_fb,
                                         UINT32* ghost_fb, ppu_worker_t* w, INT32 ridx)
{
	// OPT1: reuse previous backbuf before any setup.
	if (ppu_worker_try_reuse_frame(snap, out_fb, w, ridx)) {
		return true;
	}

	gba_ppu_t ppu_state;
	UINT32    first_buf[GBA_LCD_W];
	UINT32    second_buf[GBA_LCD_W];
	UINT8     win_buf[GBA_LCD_W];

	memset(&ppu_state, 0, sizeof(ppu_state));
	ppu_state.ghosting_strength = snap->ghosting_strength;
	ppu_state.render_per_pixel  = false;

	ppu_worker_render_ctx_t ctx;
	UINT8* vram_work = NULL;
	if (snap->vram_journal && !snap->vram_journal_overflow && snap->vram_base_valid) {
		// Mid-frame VRAM writes: replay the journal onto the line-0 base.
		memcpy(w->vram_scratch, snap->vram_base, 128 * 1024);
		vram_work = w->vram_scratch;
		ctx.vram  = vram_work;
	} else {
		ctx.vram  = snap->vram;
	}
	ctx.oam                  = NULL;
	ctx.palette              = NULL;
	ctx.io                   = NULL;
	ctx.ppu                  = &ppu_state;
	ctx.first_target_buffer  = first_buf;
	ctx.second_target_buffer = second_buf;
	ctx.window               = win_buf;
	ctx.framebuffer          = out_fb;
	ctx.out_ptr              = (w->out_bpp == 4) ? NULL : w->out_buf[ridx];
	ctx.out_bpp              = w->out_bpp;
	ctx.stop_mode            = snap->stop_mode;
	{
		// OAM uniformity: all scanlines equal line 0? Cheap 1KB compares replace
		// the per-line 128-sprite rescans in render_objs.
		const UINT8* oam_ref = snap->line_state[0].oam;
		bool uniform = true;
		for (INT32 y = 1; y < GBA_LCD_H; ++y) {
			if (memcmp(snap->line_state[y].oam, oam_ref, 1024) != 0) { uniform = false; break; }
		}
		ctx.oam_uniform = uniform;
	}

	// Per-frame ghosting Q8 factor - frame-level constant, compute once.
	INT32 frame_sbf_q8 = (INT32)(snap->ghosting_strength * 76.8f + 0.5f);
	if (frame_sbf_q8 < 0) frame_sbf_q8 = 0;
	if (frame_sbf_q8 > 76) frame_sbf_q8 = 76;
	ctx.sbf_q8       = frame_sbf_q8;
	ctx.one_m_sbf_q8 = 256 - frame_sbf_q8;
	ctx.ghost_fast   = (frame_sbf_q8 == 0);

	// Per-frame OBJ Y buckets: 64 sprites/line cap, so a scanline scans only its
	// own list instead of all 128 sprites.
	INT32 obj_bucket[GBA_LCD_H][64];
	INT32 obj_bucket_n[GBA_LCD_H] = {0};
	{
		const UINT8* oam_seed = snap->line_state[0].oam;
		for (INT32 o = 0; o < 128; ++o) {
			UINT32 attr01 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(oam_seed + o * 8));
			UINT16 attr0 = (UINT16)attr01;
			UINT16 attr1 = (UINT16)(attr01 >> 16);
			if (SB_BFE(attr0, 9, 1) && !SB_BFE(attr0, 8, 1))
				continue;
			bool  rot_scale = SB_BFE(attr0, 8, 1);
			bool  dbl_size  = SB_BFE(attr0, 9, 1) && rot_scale;
			INT32 y_coord   = SB_BFE(attr0, 0, 8);
			INT32 obj_shape = SB_BFE(attr0, 14, 2);
			INT32 obj_size  = SB_BFE(attr1, 14, 2);
			INT32 y_size    = gba_obj_ysize[obj_size * 4 + obj_shape] * (dbl_size ? 2 : 1);
			INT32 y_start = y_coord;
			if (y_start >= GBA_LCD_H) y_start -= 256;
			INT32 y_end = y_start + y_size;
			if (y_end <= 0 || y_start >= GBA_LCD_H)
				continue;
			if (y_start < 0) y_start = 0;
			if (y_end > GBA_LCD_H) y_end = GBA_LCD_H;
			for (INT32 yy = y_start; yy < y_end; ++yy) {
				if (obj_bucket_n[yy] < 64)
					obj_bucket[yy][obj_bucket_n[yy]++] = o;
			}
		}
	}

	// Initialize frame-level tile cache (per BG; invalidated on tile/py change).
	ctx.fc_tile_key[0] = ctx.fc_tile_key[1] = ctx.fc_tile_key[2] = ctx.fc_tile_key[3] = -1;
	ctx.fc_tile_valid[0] = ctx.fc_tile_valid[1] = ctx.fc_tile_valid[2] = ctx.fc_tile_valid[3] = false;
	ctx.cached_pal_bank[0]=ctx.cached_pal_bank[1]=ctx.cached_pal_bank[2]=ctx.cached_pal_bank[3] = 0;
	ctx.obj_bucket   = (const INT32(*)[64])obj_bucket;
	ctx.obj_bucket_n = obj_bucket_n;
	ctx.no_windows   = false;
	ctx.need_second_buf = false;
	ctx.pal_valid        = false;

	// Seed backbuf from the previous frame when ghosting is on (out_buf is fully
	// overwritten per scanline at bpp 2/3, so it needs no zero-init).
	const INT32 fb_bytes = GBA_LCD_W * GBA_LCD_H * 4;
	if (frame_sbf_q8 > 0 && ghost_fb)
		memcpy(out_fb, ghost_fb, fb_bytes);

	for (INT32 y = 0; y < GBA_LCD_H; ++y) {
		// Cancel/shutdown check: exit at a scanline boundary.
		if (w->exiting || w->cancel_render) return false;

		if (vram_work != NULL && y > 0) {
			UINT32 lo = snap->vram_line_lo[y];
			UINT32 hi = snap->vram_line_hi[y];
			if (lo < hi)
				memcpy(vram_work + lo, snap->vram_pool + snap->vram_line_off[y], hi - lo);
		}

		ppu_worker_install_io(&ctx, &snap->line_state[y], snap->stop_mode);
		// FP-A: expand palette to XRGB8888 once per scanline. Bits 24-31 MUST stay 0
		// (they carry priority/semi/opaque flags OR'ed in later).
		{
			// Rebuild only when the 1KB palette snapshot changed (memcmp).
			if (!ctx.pal_valid || memcmp(ctx.last_palette, ctx.palette, 1024) != 0) {
				memcpy(ctx.last_palette, ctx.palette, 1024);
				ctx.pal_valid = true;
				const UINT16* p16 = (const UINT16*)ctx.palette;
				for (INT32 pi = 0; pi < 512; ++pi) {
					UINT16 c = BURN_ENDIAN_SWAP_INT16(p16[pi]);
					UINT32 r = c & 0x1Fu;
					UINT32 g = (c >> 5) & 0x1Fu;
					UINT32 b = (c >> 10) & 0x1Fu;
					r = (r << 3) | (r >> 2);
					g = (g << 3) | (g >> 2);
					b = (b << 3) | (b >> 2);
					ctx.pal_xrgb32[pi] = (r << 16) | (g << 8) | b;
				}
			}
		}

		// Backdrop = palette entry 0 with BD encoding (pri=0, tie=0).
		UINT32 backdrop = (ctx.pal_xrgb32[0] & 0x00FFFFFFu) | ((UINT32)0 << 26) | (1u << 24);

		// Fill first_buf with backdrop; second_buf fill is deferred until
		// render_objs knows need_second_buf.
		{
			UINT32* SB_RESTRICT ft_fill = first_buf;
			INT32 x;
			for (x = 0; x < GBA_LCD_W; x += 4) {
				ft_fill[x]     = backdrop;
				ft_fill[x + 1] = backdrop;
				ft_fill[x + 2] = backdrop;
				ft_fill[x + 3] = backdrop;
			}
		}
		// win_buf is initialized by ppu_worker_render_objs with default_window_control.

		bool any_obj = ppu_worker_render_objs(&ctx, y);
		ctx.has_any_obj_this_line = any_obj;

		if (ctx.need_second_buf) {
			UINT32* SB_RESTRICT st_fill = second_buf;
			for (INT32 x = 0; x < GBA_LCD_W; x += 4) {
				st_fill[x]     = backdrop;
				st_fill[x + 1] = backdrop;
				st_fill[x + 2] = backdrop;
				st_fill[x + 3] = backdrop;
			}
		}

		// Scanline composition (FP-D fast path handled inside).
		ppu_worker_render_scanline(&ctx, y);
	}
	// Cache control key for next frame's stage1 check.
	memcpy(w->prev_key, snap->line_state[0].io, 86);
	w->prev_sig_valid = true;
	return true;
}

// --- Snapshot capture (main thread) --------------------------------------

// HBlank snapshot: IO (96B) + OAM (1KB) + palette (1KB), plus this line's VRAM
// delta (line 0 seeds vram_base; later lines record their dirty range).
static void ppu_worker_hblank_snapshot(ppu_worker_t* w, gba_t* gba, INT32 lcd_y)
{
	if (lcd_y < 0 || lcd_y >= GBA_LCD_H) return;

	ppu_worker_snapshot_t* snap = &w->snapshots[w->w_idx];
	ppu_worker_line_state_t* ls  = &snap->line_state[lcd_y];
	// IO (96B): always copied - HBlank DMA can change WIN/BGxHOFS/BLDCNT/MOSAIC.
	memcpy(ls->io, gba->mem.io, GBAPPU_WORKER_IO_SIZE);

	// OAM (1KB): unconditional copy (a memcmp pre-check is not worth it).
	memcpy(ls->oam, gba->mem.oam, 1024);

	memcpy(ls->palette, gba->mem.palette, 1024);

	// VRAM checkpoint: the reference composites line lcd_y at this instant, so
	// this line must render with the VRAM as it is now. Line 0 seeds the base;
	// later lines record what changed since the previous line's checkpoint.
	if (lcd_y == 0) {
		memcpy(snap->vram_base, gba->mem.vram, 128 * 1024);
		snap->vram_base_valid       = true;
		snap->vram_journal          = false;
		snap->vram_journal_overflow = false;
		snap->vram_pool_used        = 0;
		gba->ppu.vram_dirty_lo      = 0xFFFFFFFFu;
		gba->ppu.vram_dirty_hi      = 0u;
	} else {
		UINT32 lo = gba->ppu.vram_dirty_lo;
		UINT32 hi = gba->ppu.vram_dirty_hi;
		gba->ppu.vram_dirty_lo = 0xFFFFFFFFu;
		gba->ppu.vram_dirty_hi = 0u;
		snap->vram_line_lo[lcd_y] = 1;
		snap->vram_line_hi[lcd_y] = 0;
		if (lo < hi && !snap->vram_journal_overflow) {
			UINT32 len = hi - lo;
			if (snap->vram_pool_used + len <= PPW_VRAM_POOL) {
				snap->vram_line_lo[lcd_y]  = lo;
				snap->vram_line_hi[lcd_y]  = hi;
				snap->vram_line_off[lcd_y] = snap->vram_pool_used;
				memcpy(snap->vram_pool + snap->vram_pool_used, gba->mem.vram + lo, len);
				snap->vram_pool_used += len;
				snap->vram_journal = true;
			} else {
				// Pool full: keep whole-frame VRAM precision for this frame.
				snap->vram_journal_overflow = true;
				snap->vram_journal          = false;
			}
		}
	}
}

// Line-start snapshot: affine reference points + DISPCNT pipeline.
static void ppu_worker_line_start_snapshot(ppu_worker_t* w, gba_t* gba, INT32 lcd_y)
{
	if (lcd_y < 0 || lcd_y >= GBA_LCD_H) return;

	ppu_worker_snapshot_t* snap = &w->snapshots[w->w_idx];
	ppu_worker_line_state_t* ls = &snap->line_state[lcd_y];
	ls->bgx[0] = gba->ppu.aff[0].render_bgx;
	ls->bgy[0] = gba->ppu.aff[0].render_bgy;
	ls->bgx[1] = gba->ppu.aff[1].render_bgx;
	ls->bgy[1] = gba->ppu.aff[1].render_bgy;
	ls->dispcnt_pipeline[0] = gba->ppu.dispcnt_pipeline[0];
	ls->dispcnt_pipeline[1] = gba->ppu.dispcnt_pipeline[1];
	ls->dispcnt_pipeline[2] = gba->ppu.dispcnt_pipeline[2];
}

// VBlank entry: capture frame-level render settings (VRAM is copied at publish).
static void ppu_worker_vblank_entry(ppu_worker_t* w, gba_t* gba)
{
	ppu_worker_snapshot_t* snap = &w->snapshots[w->w_idx];
	snap->ghosting_strength = gba->ppu.ghosting_strength;
	snap->stop_mode         = gba->stop_mode;
}

// --- Job/done event signaling (counting semaphore) -----------------------
#if GBAPPU_WORKER_HAVE_PTHREAD

static inline void ppu_worker_evt_wait(ppu_worker_evt_t* e) {
	pthread_mutex_lock(&e->mutex);
	while (e->count == 0)
		pthread_cond_wait(&e->cond, &e->mutex);
	e->count--;
	pthread_mutex_unlock(&e->mutex);
}
static inline bool ppu_worker_evt_trywait(ppu_worker_evt_t* e) {
	pthread_mutex_lock(&e->mutex);
	bool ok = (e->count > 0);
	if (ok) e->count--;
	pthread_mutex_unlock(&e->mutex);
	return ok;
}
static inline void ppu_worker_evt_post(ppu_worker_evt_t* e) {
	pthread_mutex_lock(&e->mutex);
	if (e->count < GBAPPU_WORKER_N_BUFFERS + 1)
		e->count++;
	pthread_cond_signal(&e->cond);
	pthread_mutex_unlock(&e->mutex);
}
static inline void ppu_worker_evt_init(ppu_worker_evt_t* e) {
	pthread_mutex_init(&e->mutex, NULL);
	pthread_cond_init(&e->cond, NULL);
	e->count = 0;
}
static inline void ppu_worker_evt_destroy(ppu_worker_evt_t* e) {
	pthread_cond_destroy(&e->cond);
	pthread_mutex_destroy(&e->mutex);
}

static inline void ppu_worker_plat_wait_job(ppu_worker_t* w)     { ppu_worker_evt_wait(&w->job_evt); }
static inline void ppu_worker_plat_release_done(ppu_worker_t* w) { ppu_worker_evt_post(&w->done_evt); }
static inline bool ppu_worker_plat_trywait_done(ppu_worker_t* w) { return ppu_worker_evt_trywait(&w->done_evt); }
static inline void ppu_worker_plat_wait_done(ppu_worker_t* w)    { ppu_worker_evt_wait(&w->done_evt); }
static inline void ppu_worker_plat_release_job(ppu_worker_t* w)  { ppu_worker_evt_post(&w->job_evt); }
static inline void ppu_worker_plat_mb(void)                      { __sync_synchronize(); }

#elif GBAPPU_WORKER_HAVE_WINTHREAD

static inline void ppu_worker_plat_wait_job(ppu_worker_t* w)     { WaitForSingleObject(w->job_evt, INFINITE); }
static inline void ppu_worker_plat_release_done(ppu_worker_t* w) { ReleaseSemaphore(w->done_evt, 1, NULL); }
static inline bool ppu_worker_plat_trywait_done(ppu_worker_t* w) { return (WaitForSingleObject(w->done_evt, 0) == WAIT_OBJECT_0); }
static inline void ppu_worker_plat_wait_done(ppu_worker_t* w)    { WaitForSingleObject(w->done_evt, INFINITE); }
static inline void ppu_worker_plat_release_job(ppu_worker_t* w)  { ReleaseSemaphore(w->job_evt, 1, NULL); }
static inline void ppu_worker_plat_mb(void)                      { MemoryBarrier(); }

#endif

// Block on the single in-flight job's done event; the caller advances front_idx.
static inline void ppu_worker_collect_done(ppu_worker_t* w)
{
	ppu_worker_plat_wait_done(w);
}

// --- Worker thread main loop ---------------------------------------------

static inline void ppu_worker_process_job(ppu_worker_t* w)
{
	int ridx  = w->r_idx;
	int front = w->front_idx;
	UINT32* ghost_src = (front >= 0 && w->have_first_frame)
		? w->backbuf[front] : NULL;
	// Render into backbuf (XRGB8888); at out_bpp 2/3 the scanline renderer
	// writes native pixels to out_buf directly.
	bool completed = ppu_worker_render_frame_into(&w->snapshots[ridx],
	                             w->backbuf[ridx], ghost_src, w, ridx);

	ppu_worker_plat_mb();
	// Cancelled renders still signal done (ready_back_idx = -1); main thread
	// does not advance front and overwrites backbuf on the next load.
	w->ready_back_idx = completed ? ridx : -1;
	ppu_worker_plat_release_done(w);
}

// VBlank publish - one in-flight job, triple-buffered: copy VRAM, collect the
// previous job (blocking), submit, then present zero-copy (scratch on frame 0).
static void ppu_worker_vblank_publish(ppu_worker_t* w, gba_t* gba,
                                      gba_scratch_t* scratch, bool want_render)
{
	if (!w->enabled) {
		gba->framebuffer = scratch->framebuffer;
		gba->framebuffer_is_direct_copy = false;
		return;
	}

	int wi = w->w_idx;
	// VRAM: unconditional full copy.
	memcpy(w->snapshots[wi].vram, gba->mem.vram, 128 * 1024);
	// line_state[0] OAM/palette was seeded by ppu_worker_reset_slot at submit.

	// Wait for the previous job so the submit below has a free slot.
	if (w->inflight > 0) {
		ppu_worker_collect_done(w);
		w->inflight = w->inflight - 1;
		w->worker_busy = false;
		w->front_idx   = w->ready_back_idx;
	}

	// Submit as a new job (inflight is 0 after the collect above).
	bool should_submit = want_render || !w->have_first_frame;

	if (should_submit) {
		if (!w->have_first_frame) {
			// First frame: black scratch until the worker finishes.
			memset(scratch->framebuffer, 0, sizeof(scratch->framebuffer));
			gba->framebuffer = scratch->framebuffer;
			gba->framebuffer_is_direct_copy = false;
		}

		w->r_idx = wi;
		w->w_idx = (wi + 1) % GBAPPU_WORKER_N_BUFFERS;
		ppu_worker_reset_slot(w, w->w_idx, gba);
		w->worker_busy = true;
		w->inflight = w->inflight + 1;
		ppu_worker_plat_mb();
		ppu_worker_plat_release_job(w);
		w->have_first_frame = true;
	} else {
		w->w_idx = wi;  // keep slot - next frame overwrites it
	}

	// Zero-copy present: point gba->framebuffer at the finished output.
	if (w->have_first_frame && w->front_idx >= 0) {
		if (w->out_bpp == 4) {
			// 32bpp: backbuf already matches output format (zero-copy).
			gba->framebuffer = (UINT8*)w->backbuf[w->front_idx];
		} else {
			// 16/24bpp: per-scanline output lives in out_buf.
			gba->framebuffer = w->out_buf[w->front_idx];
		}
		gba->framebuffer_is_direct_copy = true;
	} else if (!gba->framebuffer || gba->framebuffer == (UINT8*)0xCDCDCDCD) {
		// No frame yet (0xCDCDCDCD = MSVC debug-heap fill): fall back to scratch.
		gba->framebuffer = scratch->framebuffer;
		gba->framebuffer_is_direct_copy = false;
	}
}

static void ppu_worker_drain_for_mode_switch(ppu_worker_t* w)
{
	if (!w->enabled) return;
	if (w->inflight > 0) {
		// Request early cancellation; result is about to be discarded.
		w->cancel_render = true;
		ppu_worker_plat_mb();
		ppu_worker_plat_wait_done(w);
		w->inflight = w->inflight - 1;
		// Advance front only when the frame completed; cancelled jobs have ready_back_idx == -1.
		if (w->ready_back_idx >= 0)
			w->front_idx = w->ready_back_idx;
		// Clear stale done signals and reset cancel flag.
		while (ppu_worker_plat_trywait_done(w)) {}
		w->cancel_render = false;
	}
	w->worker_busy = false;
}

// Configure output pixel format (2=RGB565, 3=BGR24, 4=XRGB8888).
// Called once at driver init (after ppu_worker_init). Drains in-flight work.
static inline void ppu_worker_set_output_bpp(ppu_worker_t* w, INT32 bpp)
{
	if (!w->enabled) return;
	if (bpp != 2 && bpp != 3 && bpp != 4) bpp = 4;
	ppu_worker_drain_for_mode_switch(w);
	if (bpp == w->out_bpp) return;
	// Lazy (re)allocate out_buf for 16/24bpp; free when switching to 32bpp.
	if (bpp == 4) {
		for (int _si = 0; _si < GBAPPU_WORKER_N_BUFFERS; ++_si) {
			if (w->out_buf[_si]) { free(w->out_buf[_si]); w->out_buf[_si] = NULL; }
		}
		w->out_bpp = 4;
		return;
	}
	// Two-phase commit: allocate all slots first, roll back on failure (realloc
	// keeps the old block, so the previous bpp and buffers stay valid).
	INT32 slot_bytes = GBA_LCD_W * GBA_LCD_H * bpp;
	UINT8* new_buf[GBAPPU_WORKER_N_BUFFERS];
	int _si;
	for (_si = 0; _si < GBAPPU_WORKER_N_BUFFERS; ++_si) {
		void* nb = realloc(w->out_buf[_si], slot_bytes);
		if (!nb) break;
		new_buf[_si] = (UINT8*)nb;
		memset(new_buf[_si], 0, slot_bytes);
	}
	if (_si < GBAPPU_WORKER_N_BUFFERS) {
		// Rollback: realloc failed - free newly allocated slots.
		for (int _rj = 0; _rj < _si; ++_rj) { free(new_buf[_rj]); new_buf[_rj] = NULL; }
		return; // keep old out_bpp and old buffers
	}
	// Phase 2: all allocations succeeded - commit.
	for (_si = 0; _si < GBAPPU_WORKER_N_BUFFERS; ++_si) {
		w->out_buf[_si] = new_buf[_si];
	}
	w->out_bpp = bpp;
}

static void ppu_worker_reset(ppu_worker_t* w)
{
	if (!w->enabled) return;
	ppu_worker_drain_for_mode_switch(w);
	// Drain any extra stale signals the semaphore may carry.
	while (ppu_worker_plat_trywait_done(w)) {}

	w->exiting          = false;
	w->worker_busy      = false;
	w->have_first_frame = false;
	w->inflight         = 0;
	w->cancel_render    = false;
	w->ready_back_idx   = -1;
	w->prev_sig_valid   = false;
	w->front_idx        = -1;

	memset(w->backbuf,   0, sizeof(w->backbuf));
	for (int _si = 0; _si < GBAPPU_WORKER_N_BUFFERS; ++_si)
		if (w->out_buf[_si]) memset(w->out_buf[_si], 0, GBA_LCD_W * GBA_LCD_H * w->out_bpp);
	memset((void*)w->snapshots, 0, sizeof(w->snapshots));
}

// --- PPU state machine (main thread) --------------------------------------
// Parallel twin of the single-threaded handler; captures worker snapshots at
// line boundaries and fast-forwards idle cycles.

static inline void gba_ppu_event_worker(gba_t* gba, sb_emu_state_t* emu, UINT32 cycles_late)
{
	(void)emu;

	if (gba->ppu.scan_clock >= 280896)
		gba->ppu.scan_clock -= 280896;

	INT32 lcd_y = (INT32)(gba->ppu.scan_clock / 1232);
	INT32 lcd_x = (INT32)((gba->ppu.scan_clock % 1232) / 4);
	gba->ppu.scan_clock++;

	ppu_worker_render_ctx_t fwd_ctx = {0};
	fwd_ctx.ppu       = &gba->ppu;
	fwd_ctx.stop_mode = gba->stop_mode;
	INT32 fast_forward_ticks = ppu_worker_compute_max_fast_forward(&fwd_ctx, false) + 1;
	gba->ppu.scan_clock += fast_forward_ticks;

	bool hblank = (lcd_x >= GBA_LCD_HBLANK_START) && (lcd_x < GBA_LCD_HBLANK_END);
	bool vblank = (lcd_y >= GBA_LCD_H) && (lcd_y < 228);
	INT32 vcount = (lcd_y + (lcd_x >= GBA_LCD_HBLANK_END)) % 228;

	// DISPSTAT / VCount update at x=0, hblank start, hblank end.
	if ((lcd_x == 0) || (lcd_x == GBA_LCD_HBLANK_START) || (lcd_x == GBA_LCD_HBLANK_END)) {
		UINT16 disp_stat_raw = (UINT16)gba_io_read16(gba, GBA_DISPSTAT);
		UINT32 vcount_cmp  = disp_stat_raw >> 8;
		UINT16 disp_stat   = (UINT16)(disp_stat_raw & ~0x7);
		if (vblank)                      disp_stat |= 1;
		if (hblank)                      disp_stat |= 2;
		if (vcount == (INT32)vcount_cmp) disp_stat |= 4;
		gba_io_store16(gba, GBA_DISPSTAT, disp_stat);
		gba_io_store16(gba, GBA_VCOUNT,  (UINT16)vcount);

		bool hblank_irq_en = SB_BFE(disp_stat, 4, 1);
		bool vblank_irq_en = SB_BFE(disp_stat, 3, 1);
		bool vcount_irq_en = SB_BFE(disp_stat, 5, 1);

		if (hblank != gba->ppu.last_hblank) {
			gba->ppu.last_hblank = hblank;
			if (hblank) {
				if (hblank_irq_en)
					gba_send_interrupt(gba, 3, 1 << GBA_INT_LCD_HBLANK);
				++gba->ppu.hblank_seq;
				if (gba->ppu_worker_ptr && gba->ppu_worker_ptr->enabled && lcd_y < GBA_LCD_H)
					ppu_worker_hblank_snapshot(gba->ppu_worker_ptr, gba, lcd_y);
				gba_timing_schedule(gba, &gba->dma_event, 2);
			} else {
				gba->ppu.dispcnt_pipeline[0] = gba->ppu.dispcnt_pipeline[1];
				gba->ppu.dispcnt_pipeline[1] = gba->ppu.dispcnt_pipeline[2];
				gba->ppu.dispcnt_pipeline[2] = gba_io_read16(gba, GBA_DISPCNT);
			}
		}

		if (lcd_y != gba->ppu.last_lcd_y) {
			if (vblank != gba->ppu.last_vblank) {
				gba->ppu.last_vblank = vblank;
				if (vblank) {
					if (vblank_irq_en)
						gba_send_interrupt(gba, 3, 1 << GBA_INT_LCD_VBLANK);
					++gba->ppu.vblank_seq;
					gba_timing_schedule(gba, &gba->dma_event, 2);
					gba->frame_in_progress = false;
					if (gba->ppu_worker_ptr && gba->ppu_worker_ptr->enabled)
						ppu_worker_vblank_entry(gba->ppu_worker_ptr, gba);
				}
			}
			gba->ppu.last_lcd_y = lcd_y;
			if (vcount == (INT32)vcount_cmp && vcount_irq_en)
				gba_send_interrupt(gba, 3, 1 << GBA_INT_LCD_VCOUNT);
		}
	}

	// Affine PB/PD advance + BG2X/Y reload at lcd_x == 0.
	if (lcd_x == 0 && lcd_y < GBA_LCD_H) {
		UINT16 dispcnt = gba->ppu.dispcnt_pipeline[0];
		INT32  bg_mode = SB_BFE(dispcnt, 0, 3);

		if (bg_mode != 0 && lcd_y != 0) {
			for (INT32 aff = 0; aff < 2; ++aff) {
				if (!SB_BFE(dispcnt, 8 + aff + 2, 1)) continue;

				INT32  pb = (INT16)gba_io_read16(gba, GBA_BG2PB + aff * 0x10);
				INT32  pd = (INT16)gba_io_read16(gba, GBA_BG2PD + aff * 0x10);
				UINT16 bgcnt = gba_io_read16(gba, GBA_BG2CNT + aff * 2);
				bool mosaic = SB_BFE(bgcnt, 6, 1);

				if (mosaic) {
					UINT16 mos_reg = gba_io_read16(gba, GBA_MOSAIC);
					INT32  mos_y   = SB_BFE(mos_reg, 4, 4) + 1;
					if ((lcd_y % mos_y) == 0) {
						gba->ppu.aff[aff].render_bgx += pb * mos_y;
						gba->ppu.aff[aff].render_bgy += pd * mos_y;
					}
				} else {
					gba->ppu.aff[aff].render_bgx += pb;
					gba->ppu.aff[aff].render_bgy += pd;
				}
			}
		}

		for (INT32 aff = 0; aff < 2; ++aff) {
			if (gba->ppu.aff[aff].wrote_bgx || lcd_y == 0) {
				gba->ppu.aff[aff].render_bgx = gba_io_read32(gba, GBA_BG2X + aff * 0x10);
				gba->ppu.aff[aff].render_bgx = SB_BFE(gba->ppu.aff[aff].render_bgx, 0, 28);
				gba->ppu.aff[aff].render_bgx = ((INT32)(gba->ppu.aff[aff].render_bgx << 4)) >> 4;
			}
			if (gba->ppu.aff[aff].wrote_bgy || lcd_y == 0) {
				gba->ppu.aff[aff].render_bgy = gba_io_read32(gba, GBA_BG2Y + aff * 0x10);
				gba->ppu.aff[aff].render_bgy = SB_BFE(gba->ppu.aff[aff].render_bgy, 0, 28);
				gba->ppu.aff[aff].render_bgy = ((INT32)(gba->ppu.aff[aff].render_bgy << 4)) >> 4;
			}
			gba->ppu.aff[aff].wrote_bgx = false;
			gba->ppu.aff[aff].wrote_bgy = false;
		}

		if (gba->ppu_worker_ptr && gba->ppu_worker_ptr->enabled)
			ppu_worker_line_start_snapshot(gba->ppu_worker_ptr, gba, lcd_y);
	}

	gba_timing_deschedule(gba, &gba->ppu_event);
	gba_timing_schedule(gba, &gba->ppu_event,
	                    fast_forward_ticks + 1 - (INT32)cycles_late);
}

// --- POSIX back-end -------------------------------------------------------
#if GBAPPU_WORKER_HAVE_PTHREAD

static void* ppu_worker_thread(void* arg)
{
	ppu_worker_t* w = (ppu_worker_t*)arg;

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) \
 || defined(__NetBSD__) || defined(__DragonFly__)
	pthread_setname_np("FBNeo-GBA-PPU");
#endif

	for (;;) {
		ppu_worker_plat_wait_job(w);
		if (w->exiting) break;
		ppu_worker_process_job(w);
	}
	return NULL;
}

static INT32 ppu_worker_init(ppu_worker_t* w)
{
	memset(w, 0, sizeof(*w));
	w->ready_back_idx = -1;
	w->front_idx      = -1;
	// Default 32bpp; driver calls set_output_bpp(nBurnBpp) at init.
	w->out_bpp        = 4;

	ppu_worker_evt_init(&w->job_evt);
	ppu_worker_evt_init(&w->done_evt);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, GBAPPU_WORKER_STACK_SIZE);
	int rc = pthread_create(&w->worker, &attr, ppu_worker_thread, w);
	pthread_attr_destroy(&attr);
	if (rc != 0) {
		ppu_worker_evt_destroy(&w->job_evt);
		ppu_worker_evt_destroy(&w->done_evt);
		return 1;
	}

#if defined(__linux__)
	pthread_setname_np(w->worker, "FBNeo-GBA-PPU");
#endif

	w->enabled = true;
	return 0;
}

static void ppu_worker_exit(ppu_worker_t* w)
{
	if (!w->enabled) return;
	w->exiting = true;
	ppu_worker_plat_mb();
	ppu_worker_plat_release_job(w);
	pthread_join(w->worker, NULL);
	ppu_worker_evt_destroy(&w->job_evt);
	ppu_worker_evt_destroy(&w->done_evt);
	for (int _si = 0; _si < GBAPPU_WORKER_N_BUFFERS; ++_si) { if (w->out_buf[_si]) { free(w->out_buf[_si]); w->out_buf[_si] = NULL; } }
	w->enabled = false;
}

// --- Win32 back-end -------------------------------------------------------
#elif GBAPPU_WORKER_HAVE_WINTHREAD

static DWORD WINAPI ppu_worker_thread_win32(LPVOID arg)
{
	ppu_worker_t* w = (ppu_worker_t*)arg;

#if defined(_MSC_VER)
	// Set thread name for debugger (MSVC convention via RaiseException).
	{
		typedef struct { DWORD dwType; LPCSTR szName; DWORD dwThreadID; DWORD dwFlags; } TNI;
		TNI info;
		info.dwType     = 0x1000;
		info.szName     = "FBNeo-GBA-PPU";
		info.dwThreadID = (DWORD)-1;
		info.dwFlags    = 0;
		__try {
			RaiseException(0x406D1388, 0,
			               sizeof(info)/sizeof(ULONG_PTR), (const ULONG_PTR*)&info);
		} __except(EXCEPTION_EXECUTE_HANDLER) {}
	}
#endif

	for (;;) {
		ppu_worker_plat_wait_job(w);
		if (w->exiting) break;
		ppu_worker_process_job(w);
	}
	return 0;
}

static INT32 ppu_worker_init(ppu_worker_t* w)
{
	memset(w, 0, sizeof(*w));
	w->ready_back_idx = -1;
	w->front_idx      = -1;
	// Default 32bpp; driver calls set_output_bpp(nBurnBpp) at init.
	w->out_bpp        = 4;
	w->worker  = NULL;
	w->job_evt = w->done_evt = NULL;

	w->job_evt  = CreateSemaphoreA(NULL, 0, GBAPPU_WORKER_N_BUFFERS + 1, NULL);
	w->done_evt = CreateSemaphoreA(NULL, 0, GBAPPU_WORKER_N_BUFFERS + 1, NULL);
	if (!w->job_evt || !w->done_evt) {
		if (w->job_evt)  { CloseHandle(w->job_evt);  w->job_evt  = NULL; }
		if (w->done_evt) { CloseHandle(w->done_evt); w->done_evt = NULL; }
		return 1;
	}

	DWORD tid = 0;
	w->worker = CreateThread(NULL, GBAPPU_WORKER_STACK_SIZE,
	                          ppu_worker_thread_win32, w, 0, &tid);
	if (!w->worker) {
		CloseHandle(w->job_evt);
		CloseHandle(w->done_evt);
		w->job_evt = w->done_evt = NULL;
		return 1;
	}
	SetThreadPriority(w->worker, THREAD_PRIORITY_NORMAL);
	w->enabled = true;
	return 0;
}

static void ppu_worker_exit(ppu_worker_t* w)
{
	if (!w->enabled) return;
	w->exiting = true;
	ppu_worker_plat_mb();
	ppu_worker_plat_release_job(w);
	WaitForSingleObject(w->worker, INFINITE);
	CloseHandle(w->worker); w->worker = NULL;
	if (w->job_evt)  { CloseHandle(w->job_evt);  w->job_evt  = NULL; }
	if (w->done_evt) { CloseHandle(w->done_evt); w->done_evt = NULL; }
	for (int _si = 0; _si < GBAPPU_WORKER_N_BUFFERS; ++_si) { if (w->out_buf[_si]) { free(w->out_buf[_si]); w->out_buf[_si] = NULL; } }
	w->enabled = false;
}

#endif // back-end selection

// --- Save-state serialization ------------------------------------------------
// Presented frame + slot indices (worker-owned, outside gba_t).

static inline size_t ppu_worker_state_size(void) { return PPW_BLOB_MAX; }

// Inverse of ppu_worker_write_scanline_out: decode one scanline back to XRGB8888.
static inline void ppu_worker_decode_scanline_to_xrgb8888(UINT32* dst, const UINT8* src, INT32 src_bpp)
{
	if (src_bpp == 4) {
		memcpy(dst, src, (size_t)GBA_LCD_W * 4);
	} else if (src_bpp == 3) {
		INT32 i = 0;
		for (; i < GBA_LCD_W - 3; i += 4) {
			UINT8 b0=src[0],g0=src[1],r0=src[2];
			UINT8 b1=src[3],g1=src[4],r1=src[5];
			UINT8 b2=src[6],g2=src[7],r2=src[8];
			UINT8 b3=src[9],g3=src[10],r3=src[11];
			dst[0]=((UINT32)r0<<16)|((UINT32)g0<<8)|b0;
			dst[1]=((UINT32)r1<<16)|((UINT32)g1<<8)|b1;
			dst[2]=((UINT32)r2<<16)|((UINT32)g2<<8)|b2;
			dst[3]=((UINT32)r3<<16)|((UINT32)g3<<8)|b3;
			src+=12; dst+=4;
		}
		for (; i < GBA_LCD_W; ++i) {
			UINT8 b=*src++,g=*src++,r=*src++;
			*dst++ = ((UINT32)r<<16)|((UINT32)g<<8)|b;
		}
	} else { // src_bpp == 2, RGB565 little-endian
		const UINT16* s16 = (const UINT16*)src;
		for (INT32 i = 0; i < GBA_LCD_W; ) {
			// 4-pixel unroll; 5-bit -> (v<<3)|(v>>2), 6-bit -> (v<<2)|(v>>4).
			if (GBA_LCD_W - i >= 4) {
				UINT16 p0=s16[0], p1=s16[1], p2=s16[2], p3=s16[3];
				#define EXP565(p) do { UINT32 rr=(UINT32)(((p)>>11)&0x1F); UINT32 gg=(UINT32)(((p)>>5)&0x3F); UINT32 bb=(UINT32)((p)&0x1F); 				                            *dst++ = ((rr<<3)|(rr>>2))<<16 | ((gg<<2)|(gg>>4))<<8 | ((bb<<3)|(bb>>2)); } while(0)
				EXP565(p0); EXP565(p1); EXP565(p2); EXP565(p3);
				#undef EXP565
				s16 += 4; i += 4;
			} else {
				UINT16 px = *s16++;
				UINT32 rr=(UINT32)((px>>11)&0x1F), gg=(UINT32)((px>>5)&0x3F), bb=(UINT32)(px&0x1F);
				*dst++ = ((rr<<3)|(rr>>2))<<16 | ((gg<<2)|(gg>>4))<<8 | ((bb<<3)|(bb>>2));
				++i;
			}
		}
	}
}

static inline size_t ppu_worker_save_state(ppu_worker_t* w, UINT8* dst)
{
	// Complete the in-flight render before capturing. This is behavior-neutral
	// (the next publish's collect performs the same retire) and makes the
	// captured frame the deterministic one the next vblank presents, no matter
	// how the worker thread was scheduled relative to this call.
	if (w->inflight > 0) {
		ppu_worker_collect_done(w);
		w->inflight    = w->inflight - 1;
		w->worker_busy = false;
		w->front_idx   = w->ready_back_idx;
	}
	const INT32 front = w->front_idx;
	const INT32 idx   = (front >= 0 && front < GBAPPU_WORKER_N_BUFFERS) ? front : -1;
	const size_t slot  = (size_t)GBA_LCD_W * GBA_LCD_H * w->out_bpp;
	ppw_blob_hdr_t hdr;
	hdr.magic     = PPW_BLOB_MAGIC;
	hdr.version   = PPW_BLOB_VERSION;
	hdr.out_bpp   = w->out_bpp;
	hdr.front_idx = idx;
	memcpy(dst, &hdr, sizeof(hdr));
	if (idx >= 0) {
		const UINT8* src = (w->out_bpp == 4) ? (const UINT8*)w->backbuf[idx]
		                                     : (const UINT8*)w->out_buf[idx];
		if (src != NULL) memcpy(dst + PPW_BLOB_HEADER, src, slot);
		else             memset(dst + PPW_BLOB_HEADER, 0, slot);
	}
	// state_size reports the bpp=4 bound, so zero the unused tail at bpp<4.
	const size_t written = (size_t)PPW_BLOB_HEADER + (idx >= 0 ? slot : 0);
	if (written < (size_t)PPW_BLOB_MAX)
		memset(dst + written, 0, (size_t)PPW_BLOB_MAX - written);
	return written;
}

static inline bool ppu_worker_load_state(ppu_worker_t* w, const UINT8* src, size_t len,
                                         UINT8** out_fb, bool* out_direct)
{
	if (!w->enabled) return false;   // worker never started: nothing to restore
	ppw_blob_hdr_t hdr;
	if (len < (size_t)PPW_BLOB_HEADER) return false;
	memcpy(&hdr, src, sizeof(hdr));
	if (hdr.magic != PPW_BLOB_MAGIC || hdr.version != PPW_BLOB_VERSION) return false;
	if (hdr.out_bpp != 2 && hdr.out_bpp != 3 && hdr.out_bpp != 4) return false;
	if (hdr.front_idx < -1 || hdr.front_idx >= GBAPPU_WORKER_N_BUFFERS) return false;

	const INT32 idx = hdr.front_idx;
	// Payload length comes from the SAVED depth, so any depth can be loaded.
	const size_t src_slot = (size_t)GBA_LCD_W * GBA_LCD_H * hdr.out_bpp;
	if (len < (size_t)PPW_BLOB_HEADER + (idx >= 0 ? src_slot : 0)) return false;

	// Quiesce the worker and drain stale done signals before touching buffers.
	ppu_worker_drain_for_mode_switch(w);
	while (ppu_worker_plat_trywait_done(w)) {}

	// No frame was presented. Normalise the indices (runahead callers skip
	// ppu_worker_reset) and fail; the return also keeps out_fb off backbuf[-1].
	if (idx < 0) {
		w->front_idx        = -1;
		w->r_idx            = 0;
		w->w_idx            = 0;
		w->ready_back_idx   = -1;
		w->prev_sig_valid   = false;
		w->have_first_frame = false;
		return false;
	}

	{
		// out_buf is malloc'd by set_output_bpp; a load before any vblank sees NULL.
		if (w->out_bpp != 4 && w->out_buf[idx] == NULL) {
			w->out_buf[idx] = (UINT8*)malloc((size_t)GBA_LCD_W * GBA_LCD_H * w->out_bpp);
			if (w->out_buf[idx] == NULL) return false;
		}
		const UINT8* sp = src + PPW_BLOB_HEADER;
		if (hdr.out_bpp == w->out_bpp) {
			// Same depth: native layout, single bulk copy.
			if (w->out_bpp == 4) memcpy(w->backbuf[idx], sp, src_slot);
			else                 memcpy(w->out_buf[idx], sp, src_slot);
		} else {
			// Cross-depth: decode to XRGB8888, then re-encode to native depth.
			UINT32 tmp[GBA_LCD_W];
			if (w->out_bpp == 4) {
				for (INT32 y = 0; y < GBA_LCD_H; ++y) {
					ppu_worker_decode_scanline_to_xrgb8888(tmp, sp, hdr.out_bpp);
					memcpy(w->backbuf[idx] + y*(size_t)GBA_LCD_W, tmp, sizeof(tmp));
					sp += GBA_LCD_W * hdr.out_bpp;
				}
			} else {
				UINT8* dp = w->out_buf[idx];
				for (INT32 y = 0; y < GBA_LCD_H; ++y) {
					ppu_worker_decode_scanline_to_xrgb8888(tmp, sp, hdr.out_bpp);
					ppu_worker_write_scanline_out(dp, tmp, w->out_bpp);
					sp += GBA_LCD_W * hdr.out_bpp;
					dp += GBA_LCD_W * w->out_bpp;
				}
			}
		}
	}

	// Present the restored frame; advance the write slot past it. inflight and
	// worker_busy are already clear after the drain above.
	w->front_idx        = idx;
	w->r_idx            = 0;
	w->w_idx            = (idx + 1) % GBAPPU_WORKER_N_BUFFERS;
	w->ready_back_idx   = -1;
	w->prev_sig_valid   = false;   // OPT1 cache stale: front_idx/backbuf replaced
	w->have_first_frame = true;

	// Hand the restored slot back to the caller; 32bpp is zero-copy.
	if (w->out_bpp == 4) *out_fb = (UINT8*)w->backbuf[idx];
	else                 *out_fb = w->out_buf[idx];
	*out_direct = (w->out_bpp == 4);
	return true;
}

#else // !GBAPPU_WORKER_ENABLED

typedef struct ppu_worker_snapshot_t { } ppu_worker_snapshot_t;

// Stub ppu_worker_t: just enough fields for gba.cpp / d_gba.cpp to compile.
// front_idx stays -1 (vblank_publish no-op) and out_buf is NULL; single-thread
// mode renders through the standard gba_ppu_event path into gba->framebuffer.
typedef struct ppu_worker_t {
	bool enabled;
	bool worker_busy;
	INT32 out_bpp;
	INT32 front_idx;
	bool  prev_sig_valid;      // referenced by stub reset below
	UINT32* backbuf[1];        // unused in stub; pointer-sized to silence type checks
	UINT8*  out_buf[1];        // unused in stub; always NULL
} ppu_worker_t;

static inline INT32 ppu_worker_init(ppu_worker_t* w)
{
	w->enabled        = false;
	w->worker_busy    = false;
	w->out_bpp        = 4;
	w->front_idx      = -1;
	w->prev_sig_valid = false;
	w->backbuf[0]     = NULL;
	w->out_buf[0]     = NULL;
	return 1;
}
static inline void  ppu_worker_exit(ppu_worker_t* w)                                  { (void)w; }
static inline void  ppu_worker_reset(ppu_worker_t* w)                                 { w->front_idx = -1; w->backbuf[0] = NULL; w->out_buf[0] = NULL; w->prev_sig_valid=false; }
static inline void  ppu_worker_set_output_bpp(ppu_worker_t* w, INT32 bpp)             { w->out_bpp = bpp; (void)bpp; }
static inline void  ppu_worker_hblank_snapshot(ppu_worker_t* w, gba_t* g, INT32 y)     { (void)w; (void)g; (void)y; }
static inline void  ppu_worker_line_start_snapshot(ppu_worker_t* w, gba_t* g, INT32 y) { (void)w; (void)g; (void)y; }
static inline void  ppu_worker_vblank_entry(ppu_worker_t* w, gba_t* g)                 { (void)w; (void)g; }
static inline void  ppu_worker_vblank_publish(ppu_worker_t* w, gba_t* g, gba_scratch_t* s, bool want_render)
                                                                                       { (void)w; (void)g; (void)s; (void)want_render; }
static inline void  ppu_worker_drain_for_mode_switch(ppu_worker_t* w)                  { (void)w; }
static inline bool  ppu_worker_enabled(const ppu_worker_t* w)                          { (void)w; return false; }

static inline void gba_ppu_event_worker(gba_t* gba, sb_emu_state_t* emu, UINT32 cycles_late)
{
	gba_ppu_event(gba, emu, cycles_late);
}

// Zero-filled payload: the size must still match the worker build, or FBNeo's
// ScanVar (length + name hash) would silently skip gba_machine across builds.
static inline size_t ppu_worker_state_size()                                                           { return PPW_BLOB_MAX; }
static inline size_t ppu_worker_save_state(ppu_worker_t* w, UINT8* dst)                                { (void)w; memset(dst, 0, PPW_BLOB_MAX); return PPW_BLOB_MAX; }
static inline bool   ppu_worker_load_state(ppu_worker_t* w, const UINT8* src, size_t len,
                                           UINT8** fb, bool* direct)                                   { (void)w; (void)src; (void)len; (void)fb; (void)direct; return false; }

#endif // GBAPPU_WORKER_ENABLED

#ifdef __cplusplus
}
#endif
