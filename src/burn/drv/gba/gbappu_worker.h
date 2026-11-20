// GBA PPU multithreaded renderer: scanline snapshots + worker composition.
// Backends: pthread / Win32 / single-threaded fallback.

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
// Three slots: presented, being-rendered, being-captured; one job in flight.
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
	// True when every scanline's OAM equals line 0's; lets render_objs skip
	// the late catch-up rescan.
	bool    oam_uniform;
	// True when every enabled OBJ in the frame is a regular (non-rot/scale)
	// sprite with OBJ mosaic off and OBJ window off.
	bool    oam_all_regular;
	// Visible OBJ count per scanline; built from line 0 OAM and valid only
	// when oam_all_regular is true.
	INT32   obj_count[GBA_LCD_H];
	// Per-frame ghosting Q8 factor (frame-level constant).
	INT32 ghost_q8;
	INT32 one_minus_ghost_q8;
	bool  ghost_fast;         // ghost_q8 == 0 -> direct write, no blend

	// Per-frame tile-row cache (same tile + same py reused across scanlines).
	INT32  tile_fetch_key[4];
	UINT8  tile_fetch_bytes[4][8];
	bool   tile_fetch_valid[4];

	// Per-scanline OBJ Y-buckets: up to 64 sprites per scanline.
	const INT32 (*obj_bucket)[64];
	const INT32*  obj_bucket_n;
	// Scanline flags set by the OBJ pass before render_scanline runs.
	bool  no_windows;         // windows have no per-pixel effect this line
	bool  frame_needs_second_buf; // frame-level: 1 if any scanline needs second buffer (set by render_objs)
	bool  need_second_this_line;  // per-line flag: true while this scanline actually uses st_buf
	INT32 eff_blend_mode;       // BLDCNT effect mode actually in force (0 when inert)
	// Pre-expanded palette table (512 x XRGB8888 UINT32), rebuilt per
	// scanline only when the palette changed (dirty-checked).
	UINT32 pal_xrgb[512];
	UINT8  pal_snapshot[1024];   // Snapshot of palette at last rebuild
	bool   pal_valid;            // false until first rebuild (ctx is not zeroed)
	// BG palette bank cache, refreshed at tile boundaries by the generic BG loop.
	INT32  cached_pal_bank[4];
} ppu_worker_render_ctx_t;

// Per-scanline snapshot.
typedef struct ppu_worker_line_state_t {
	UINT8  io[GBAPPU_WORKER_IO_SIZE];
	UINT8  oam[1024];
	UINT8  palette[1024];
	INT32  aff_bgx[2];
	INT32  aff_bgy[2];
	UINT16 dispcnt_pipeline[3];
} ppu_worker_line_state_t;

// Full frame snapshot: 160 line states + frame settings + a per-line VRAM
// journal replayed on the shared line-0 baseline (ppu_worker_t::vram_shadow).
typedef struct ppu_worker_snapshot_t {
	ppu_worker_line_state_t line_state[GBA_LCD_H];
	float  ghosting_strength;
	bool   stop_mode;

	// Per-scanline VRAM journal replayed onto the shared vram_shadow baseline.
	UINT8  vram_pool[PPW_VRAM_POOL];
	UINT32 vram_line_lo[GBA_LCD_H];   // dirty range low; lo >= hi means unchanged
	UINT32 vram_line_hi[GBA_LCD_H];   // dirty range high (exclusive)
	UINT32 vram_line_off[GBA_LCD_H];  // where line y's bytes start in vram_pool
	UINT32 vram_pool_used;
	bool   vram_journal;              // at least one line changed mid-frame
	bool   vram_journal_overflow;     // pool full: render reads vram_shadow directly

	// OAM/palette sharing: false while no mid-frame write was seen, in which
	// case every scanline reads line 0's copy and no per-line copy is stored.
	bool   oam_per_line;
	bool   palette_per_line;

	// Frame-level palette pre-expanded to 512 XRGB8888 entries from
	// line_state[0].palette at the lcd_y == 0 HBlank capture; used when
	// palette_per_line is false.
	UINT32 pal_xrgb[512];
} ppu_worker_snapshot_t;

// Resolve a scanline's OAM/palette: line 0's frame-level copy, unless the line
// was captured after a mid-frame write.
SB_ALWAYS_INLINE const UINT8* ppu_worker_line_oam(const ppu_worker_snapshot_t* snap, INT32 y)
{
	return snap->oam_per_line ? snap->line_state[y].oam : snap->line_state[0].oam;
}
SB_ALWAYS_INLINE const UINT8* ppu_worker_line_palette(const ppu_worker_snapshot_t* snap, INT32 y)
{
	return snap->palette_per_line ? snap->line_state[y].palette : snap->line_state[0].palette;
}

// Expand a BGR555 palette snapshot (512 entries) to XRGB8888; bits 24-31 stay 0.
SB_ALWAYS_INLINE void ppu_worker_expand_palette(UINT32* dst, const UINT8* pal_bytes)
{
	const UINT16* p16 = (const UINT16*)pal_bytes;
	for (INT32 pi = 0; pi < 512; ++pi) {
		UINT16 c = BURN_ENDIAN_SWAP_INT16(p16[pi]);
		UINT32 r = c & 0x1Fu, g = (c >> 5) & 0x1Fu, b = (c >> 10) & 0x1Fu;
		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);
		dst[pi] = (r << 16) | (g << 8) | b;
	}
}

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

	// Output pixel depth (2/3/4), fixed per session.
	INT32 out_bpp;

	ppu_worker_snapshot_t snapshots[GBAPPU_WORKER_N_BUFFERS];
	// Internal XRGB8888 render targets (one per slot); presented directly at
	// 32bpp (zero copy), otherwise re-encoded per scanline into out_buf.
	UINT32  backbuf[GBAPPU_WORKER_N_BUFFERS][GBA_LCD_W * GBA_LCD_H];
	// Shared VRAM line-0 baseline of the frame being rendered.
	UINT8   vram_shadow[128 * 1024];
	// Running VRAM the journal path replays deltas onto (worker thread only).
	UINT8   vram_scratch[128 * 1024];

	// Output scanline buffers (one per slot), lazily allocated when
	// out_bpp!=4 (NULL at 32bpp).
	UINT8* out_buf[GBAPPU_WORKER_N_BUFFERS];

	volatile int write_snap_idx;          // main: slot the next frame is captured into
	volatile int read_snap_idx;          // worker: slot the in-flight job renders into
	volatile int ready_back_idx; // slot of the last completed job
	volatile int front_idx;      // presented slot (backbuf at 32bpp, else out_buf)

	volatile bool worker_busy;
	volatile bool have_first_frame;
	volatile int  inflight;      // submitted-but-not-collected job count
	// Set by main thread to abort the in-flight render (load_state / mode switch).
	volatile bool cancel_render;

	// OAM/palette write counters (main thread): the snapshot compares them
	// against the line-0 baseline to detect mid-frame modification.
	UINT32 oam_write_seq;
	UINT32 palette_write_seq;
	UINT32 oam_write_base;
	UINT32 palette_write_base;

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

static inline bool ppu_worker_enabled(const ppu_worker_t* worker) {
	return worker && worker->enabled;
}

// Note a palette (0x05) or OAM (0x07) write. No-op without a worker.
static inline void gba_ppu_worker_note_mem_write(gba_t* gba, UINT32 bank)
{
	ppu_worker_t* worker = gba->ppu_worker_ptr;
	if (worker == NULL) return;
	if (bank == 0x05u)      worker->palette_write_seq = worker->palette_write_seq + 1u;
	else if (bank == 0x07u) worker->oam_write_seq     = worker->oam_write_seq + 1u;
}

// --- Internal helpers ------------------------------------------------------

// Reset per-slot state. line_state is overwritten each frame by snapshots
// before being read - no memset needed.
static inline void ppu_worker_reset_slot(ppu_worker_t* worker, int snap_idx, gba_t* gba)
{
	ppu_worker_snapshot_t* snap = &worker->snapshots[snap_idx];
	snap->ghosting_strength = gba->ppu.ghosting_strength;
	snap->stop_mode         = gba->stop_mode;
	snap->oam_per_line      = false;
	snap->palette_per_line  = false;
	// Seed line_state[0] OAM/palette; both must be valid before line 0 is snapshotted.
	memcpy(snap->line_state[0].oam,     gba->mem.oam,     1024);
	memcpy(snap->line_state[0].palette, gba->mem.palette, 1024);
	// The per-line VRAM journal is rebuilt by the line snapshots of this frame.
	snap->vram_journal          = false;
	snap->vram_journal_overflow = false;
	snap->vram_pool_used        = 0;
}

// Install per-line state into the render context. stop_mode is frame-level.
static inline void ppu_worker_install_io(ppu_worker_render_ctx_t* ctx,
                                         const ppu_worker_line_state_t* line,
                                         bool stop_mode)
{
	ctx->io      = (UINT8*)line->io;
	ctx->oam     = (UINT8*)line->oam;
	ctx->palette = (UINT8*)line->palette;
	ctx->ppu->aff[0].render_bgx = line->aff_bgx[0];
	ctx->ppu->aff[0].render_bgy = line->aff_bgy[0];
	ctx->ppu->aff[1].render_bgx = line->aff_bgx[1];
	ctx->ppu->aff[1].render_bgy = line->aff_bgy[1];
	ctx->ppu->dispcnt_pipeline[0] = line->dispcnt_pipeline[0];
	ctx->ppu->dispcnt_pipeline[1] = line->dispcnt_pipeline[1];
	ctx->ppu->dispcnt_pipeline[2] = line->dispcnt_pipeline[2];
	ctx->stop_mode = stop_mode;
}

// Read a PPU MMIO halfword from the per-line IO snapshot.
SB_ALWAYS_INLINE UINT16 ppu_worker_io_read16(ppu_worker_render_ctx_t* ctx, UINT32 addr)
{
	return BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->io + (addr & 0x3ff)));
}

// 4bpp tile row: fetch 4 VRAM bytes into the per-frame tile cache.
SB_ALWAYS_INLINE const UINT8* ppu_worker_fetch_tile4(ppu_worker_render_ctx_t* ctx, INT32 bg, INT32 addr)
{
	if (!ctx->tile_fetch_valid[bg] || ctx->tile_fetch_key[bg] != addr) {
		ctx->tile_fetch_key[bg] = addr;
		UINT32 tile_word = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(ctx->vram + addr));
		UINT8* tile_bytes = ctx->tile_fetch_bytes[bg];
		tile_bytes[0] = (UINT8)tile_word;
		tile_bytes[1] = (UINT8)(tile_word >> 8);
		tile_bytes[2] = (UINT8)(tile_word >> 16);
		tile_bytes[3] = (UINT8)(tile_word >> 24);
		ctx->tile_fetch_valid[bg] = true;
	}
	return ctx->tile_fetch_bytes[bg];
}

// 8bpp tile row: fetch 8 VRAM bytes into the per-frame tile cache.
SB_ALWAYS_INLINE const UINT8* ppu_worker_fetch_tile8(ppu_worker_render_ctx_t* ctx, INT32 bg, INT32 addr)
{
	if (!ctx->tile_fetch_valid[bg] || ctx->tile_fetch_key[bg] != addr) {
		ctx->tile_fetch_key[bg] = addr;
		UINT64 tile_qword = BURN_ENDIAN_SWAP_INT64(*(const UINT64*)(ctx->vram + addr));
		UINT8* tile_bytes = ctx->tile_fetch_bytes[bg];
		for (INT32 k = 0; k < 8; ++k) tile_bytes[k] = (UINT8)(tile_qword >> (8 * k));
		ctx->tile_fetch_valid[bg] = true;
	}
	return ctx->tile_fetch_bytes[bg];
}

// Fill a 240-pixel row with a constant colour (4-pixel unroll).
SB_ALWAYS_INLINE void ppu_worker_fill_row(UINT32* dst, UINT32 col)
{
	for (INT32 x = 0; x < GBA_LCD_W; x += 4) {
		dst[x]     = col;
		dst[x + 1] = col;
		dst[x + 2] = col;
		dst[x + 3] = col;
	}
}

// OBJ size table; index = obj_size * 4 + obj_shape.
static const INT32 gba_obj_xsize[16] = {
	 8,16, 8, 0,  16,32, 8, 0,  32,32,16, 0,  64,64,32, 0
};
static const INT32 gba_obj_ysize[16] = {
	 8, 8,16, 0,  16, 8,32, 0,  32, 16,32, 0,  64,32,64, 0
};

// Vertical wrap: tall OBJ whose bottom exceeds Y=256 is moved up by 256.
SB_ALWAYS_INLINE INT32 ppu_worker_obj_y_wrap(INT32 y_coord, INT32 y_size_eff)
{
	return (y_coord + y_size_eff > 256) ? (y_coord - 256) : y_coord;
}

// Decoded geometry for one OBJ slot (shared across all per-scanline passes).
typedef struct ppu_worker_obj_geom_t {
	INT32 x_size, y_size, y_size_eff, x_draw_w;
	INT32 y_coord, y_wrap, x_raw;
	INT16 x_coord;
	bool  rot_scale, double_size;
	// Pre-decoded attr0/1/2 fields (populated by ppu_worker_obj_decode for valid slots).
	UINT8 obj_mode, mosaic, colors_or_palettes, h_flip, v_flip;
	UINT8 priority, palette;
	UINT16 rot_scale_param;
	INT16  tile_base;
	bool   valid;   // false for disabled (attr0 bit9 && !rot_scale) slots
} ppu_worker_obj_geom_t;

// Decode one OAM slot into geom (includes attr0/1/2 attribute bits for Pass 2).
// Returns false when the slot is disabled (attr0 bit9 set without rot/scale).
static inline bool ppu_worker_obj_decode(ppu_worker_obj_geom_t* geom, const UINT8* oam, INT32 obj_idx)
{
	const UINT8* entry = oam + obj_idx * 8;
	UINT32 attr_word01 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)entry);
	UINT16 attr0 = (UINT16)attr_word01;
	UINT16 attr1 = (UINT16)(attr_word01 >> 16);
	UINT16 attr2 = BURN_ENDIAN_SWAP_INT16(*(const UINT16*)(entry + 4));

	geom->rot_scale   = SB_BFE(attr0, 8, 1);
	geom->double_size = SB_BFE(attr0, 9, 1) && geom->rot_scale;
	if (SB_BFE(attr0, 9, 1) && !geom->rot_scale) {
		geom->valid = false;
		return false;
	}
	INT32 obj_shape = SB_BFE(attr0, 14, 2);
	INT32 obj_size  = SB_BFE(attr1, 14, 2);
	geom->x_size     = gba_obj_xsize[obj_size * 4 + obj_shape];
	geom->y_size     = gba_obj_ysize[obj_size * 4 + obj_shape];
	geom->y_size_eff = geom->y_size * (geom->double_size ? 2 : 1);
	geom->x_draw_w   = geom->x_size * (geom->double_size ? 2 : 1);
	geom->y_coord    = SB_BFE(attr0, 0, 8);
	geom->y_wrap     = ppu_worker_obj_y_wrap(geom->y_coord, geom->y_size_eff);
	geom->x_raw      = SB_BFE(attr1, 0, 9);
	INT16 x_coord = (INT16)geom->x_raw;
	if (SB_BFE(x_coord, 8, 1)) x_coord |= 0xfe00;
	geom->x_coord = x_coord;
	geom->obj_mode           = (UINT8)SB_BFE(attr0, 10, 2);
	geom->mosaic             = (UINT8)SB_BFE(attr0, 12, 1);
	geom->colors_or_palettes = (UINT8)SB_BFE(attr0, 13, 1);
	geom->h_flip             = (UINT8)(SB_BFE(attr1, 12, 1) && !geom->rot_scale);
	geom->v_flip             = (UINT8)(SB_BFE(attr1, 13, 1) && !geom->rot_scale);
	geom->rot_scale_param    = (UINT16)SB_BFE(attr1, 9, 5);
	geom->priority           = (UINT8)SB_BFE(attr2, 10, 2);
	geom->palette            = (UINT8)SB_BFE(attr2, 12, 4);
	geom->tile_base          = (INT16)SB_BFE(attr2, 0, 10);
	geom->valid = true;
	return true;
}

// True when the OBJ covers the given scanline after vertical wrap.
SB_ALWAYS_INLINE bool ppu_worker_obj_covers_row(const ppu_worker_obj_geom_t* geom, INT32 sprite_lcd_y)
{
	INT32 sy = sprite_lcd_y - geom->y_wrap;
	return sy >= 0 && sy < geom->y_size_eff;
}

// True when the OBJ lies fully outside the 240-pixel row (left or right).
// x_draw_w already accounts for rot/scale double-size.
SB_ALWAYS_INLINE bool ppu_worker_obj_off_row(const ppu_worker_obj_geom_t* geom)
{
	INT32 x_end = geom->x_coord + geom->x_draw_w;
	return x_end <= 0 || geom->x_coord >= GBA_LCD_W;
}

// Horizontal clip range on the row; an empty range has no visible pixel.
SB_ALWAYS_INLINE void ppu_worker_obj_x_clip(const ppu_worker_obj_geom_t* geom, INT32* x_lo, INT32* x_hi)
{
	INT32 x_end = geom->x_coord + geom->x_draw_w;
	*x_lo = geom->x_coord < 0 ? 0 : (geom->x_coord > GBA_LCD_W ? GBA_LCD_W : geom->x_coord);
	*x_hi = x_end > GBA_LCD_W ? GBA_LCD_W : (x_end < 0 ? 0 : x_end);
}

// OBJ draw-cycle cost. Returns false when the OBJ is fully left- or
// right-offscreen and charges zero cycles.
static inline bool ppu_worker_obj_draw_cycles(const ppu_worker_obj_geom_t* geom, INT32* cycles)
{
	INT32 x_end = geom->x_coord + geom->x_draw_w;
	if (x_end <= 0 || geom->x_coord >= GBA_LCD_W)
		return false;
	if (geom->rot_scale) {
		INT32 cost = 8 + geom->x_draw_w * 2;
		if (geom->x_coord < 0) cost += geom->x_coord;
		*cycles = cost;
		return true;
	}
	INT32 cost = geom->x_size - 2;
	if (geom->x_coord < 0) cost += geom->x_coord >> 1;
	*cycles = cost;
	return true;
}


// Compact hit record for one OBJ (Pass 2 uses only this, no further OAM reads).
typedef struct ppu_worker_hit_t {
	UINT8  rot_scale, double_size, obj_mode;
	UINT8  mosaic, colors_or_palettes, h_flip, v_flip;
	UINT8  priority, palette;
	UINT16 rot_scale_param;
	INT16  tile_base;
	INT16  x_coord, x_start, x_end;
	UINT16 y_coord, x_size, y_size;
} ppu_worker_hit_t;

// Populate one hit entry from a pre-decoded geom (no further OAM reads).
static void ppu_worker_fill_hit(ppu_worker_hit_t* hit, const ppu_worker_obj_geom_t* geom,
                                       INT32 x_clip_lo, INT32 x_clip_hi) {
	hit->rot_scale          = (UINT8)geom->rot_scale;
	hit->double_size        = (UINT8)geom->double_size;
	hit->obj_mode           = (UINT8)geom->obj_mode;
	hit->mosaic             = geom->mosaic;
	hit->colors_or_palettes = geom->colors_or_palettes;
	hit->h_flip             = geom->h_flip;
	hit->v_flip             = geom->v_flip;
	hit->rot_scale_param    = geom->rot_scale_param;
	hit->priority           = geom->priority;
	hit->palette            = geom->palette;
	hit->tile_base          = geom->tile_base;
	hit->x_coord            = geom->x_coord;
	hit->x_start            = (INT16)x_clip_lo;
	hit->x_end              = (INT16)x_clip_hi;
	hit->y_coord            = (UINT16)geom->y_coord;
	hit->x_size             = (UINT16)geom->x_size;
	hit->y_size             = (UINT16)geom->y_size;
}

// window_permissive(): true when this scanline's WIN0/WIN1 rects reduce to a
// single mask equivalent to the no-window default, so the window passes can be skipped.
static bool ppu_worker_window_permissive(ppu_worker_render_ctx_t* ctx,
                                                UINT16 dispcnt, INT32 lcd_y)
{
	// OBJ-window coverage follows sprite shapes, so it is not statically reducible.
	if (SB_BFE(dispcnt, 15, 1)) return false;

	UINT8  win_out_default  = (UINT8)(ppu_worker_io_read16(ctx, GBA_WINOUT) & 0x3F);
	UINT16 win_in = ppu_worker_io_read16(ctx, GBA_WININ);
	bool   covered[2]  = { false, false };
	bool   full_width[2] = { false, false };
	UINT8  win_val[2]  = { 0, 0 };
	INT32  x_min[2] = { 0, 0 }, x_max[2] = { 0, 0 };
	for (INT32 n = 0; n < 2; ++n) {
		if (!SB_BFE(dispcnt, 13 + n, 1)) continue;
		UINT16 WINH = ppu_worker_io_read16(ctx, (n == 0) ? GBA_WIN0H : GBA_WIN1H);
		UINT16 WINV = ppu_worker_io_read16(ctx, (n == 0) ? GBA_WIN0V : GBA_WIN1V);
		x_min[n] = SB_BFE(WINH, 8, 8); x_max[n] = SB_BFE(WINH, 0, 8);
		INT32 ymin = SB_BFE(WINV, 8, 8), ymax = SB_BFE(WINV, 0, 8);
		if (x_min[n] > x_max[n]) x_max[n] = 240;
		bool y_wrapped = (ymin > ymax);
		if (y_wrapped) {
			// Y-wrap covers [ymin,160) union [0,ymax); never full-height on any line.
			bool in_wrap = (lcd_y >= ymin) || (lcd_y < ymax);
			if (!in_wrap) continue;
			ymin = 0; ymax = 160;
		}
		if (x_max[n] > 240) x_max[n] = 240;
		if (ymax > 160) ymax = 160;
		if (x_min[n] >= x_max[n]) continue;
		if (lcd_y < ymin || lcd_y >= ymax) continue;
		covered[n]  = true;
		full_width[n] = (x_min[n] == 0 && x_max[n] >= 240) && !y_wrapped && (ymin == 0 && ymax >= 160);
		win_val[n]  = (UINT8)(SB_BFE(win_in, n * 8, 6) & 0x3F);
	}
	UINT8 uniform_val;
	if (covered[0] && full_width[0])                 uniform_val = win_val[0];
	else if (!covered[0] && covered[1] && full_width[1]) uniform_val = win_val[1];
	else if (!covered[0] && !covered[1])           uniform_val = win_out_default;
	else {
		// Partial coverage: split at the WIN0/WIN1 x-edges and sample one pixel
		// per interval; the mask is piecewise-constant, so agreement is exact.
		INT32 edges[4]; INT32 n_edges = 0;
		if (covered[0]) { edges[n_edges++] = x_min[0]; edges[n_edges++] = x_max[0]; }
		if (covered[1]) { edges[n_edges++] = x_min[1]; edges[n_edges++] = x_max[1]; }
		for (INT32 i = 0; i < n_edges; ++i)
			for (INT32 j = i + 1; j < n_edges; ++j)
				if (edges[j] < edges[i]) { INT32 swap_tmp = edges[i]; edges[i] = edges[j]; edges[j] = swap_tmp; }
		INT32 n_unique = 0;
		for (INT32 i = 0; i < n_edges; ++i)
			if (edges[i] > 0 && edges[i] < 240 && (n_unique == 0 || edges[i] != edges[n_unique - 1]))
				edges[n_unique++] = edges[i];
		n_edges = n_unique;
		UINT8 scan_val = 0xFF;
		INT32 prev_edge = 0;
		for (INT32 i = 0; i <= n_edges; ++i) {
			INT32 seg_end = (i < n_edges) ? edges[i] : 240;
			if (seg_end > prev_edge) {
				INT32 sample_x = prev_edge + ((seg_end - prev_edge) >> 1);
				UINT8 sample_val;
				if      (covered[0] && sample_x >= x_min[0] && sample_x < x_max[0]) sample_val = win_val[0];
				else if (covered[1] && sample_x >= x_min[1] && sample_x < x_max[1]) sample_val = win_val[1];
				else                                             sample_val = win_out_default;
				if (scan_val == 0xFF) scan_val = sample_val;
				else if (scan_val != sample_val) return false;
			}
			prev_edge = seg_end;
		}
		uniform_val = scan_val;
	}
	if (uniform_val == 0x3F) return true;
	// 0x1F masks out the "effects" bit: inert when no blend/bright/dark applies.
	bool effects_inert = (ctx->eff_blend_mode == 0);
	if (uniform_val == 0x1F && effects_inert) {
		return true;
	}
	return false;
}

static void ppu_worker_render_objs(ppu_worker_render_ctx_t* ctx, INT32 sprite_lcd_y)
{
	UINT16 dispcnt         = ppu_worker_io_read16(ctx, GBA_DISPCNT);
	INT32  bg_mode         = SB_BFE(dispcnt, 0, 3);
	INT32  obj_map_2d      = !SB_BFE(dispcnt, 6, 1);

	UINT16 mosaic_reg = ppu_worker_io_read16(ctx, GBA_MOSAIC);
	INT32  mosaic_x   = SB_BFE(mosaic_reg,  8, 4) + 1;
	INT32  mosaic_y   = SB_BFE(mosaic_reg, 12, 4) + 1;
	if (++ctx->ppu->mosaic_y_counter >= mosaic_y || sprite_lcd_y == 0)
		ctx->ppu->mosaic_y_counter = 0;

	bool win_enabled            = SB_BFE(dispcnt, 13, 3) != 0;
	bool  obj_window_enable = SB_BFE(dispcnt, 15, 1);
	UINT8 default_window_control = 0x3F;     // no windows: all layers visible
	UINT8 obj_window_control    = 0x3F;
	if (win_enabled || obj_window_enable) {
		UINT16 WINOUT = ppu_worker_io_read16(ctx, GBA_WINOUT);
		if (win_enabled)
			default_window_control = SB_BFE(WINOUT, 0, 6);
		obj_window_control = default_window_control;  // WINOUT outside value (gbappu.h)
		if (obj_window_enable)
			obj_window_control   = SB_BFE(WINOUT, 8, 6);
	}

	// Set once a semi-transparent OBJ pixel is drawn on this line.
	bool  saw_semitrans_pixel = false;

	// Read BLDCNT early so the early return still sets need_second_this_line.
	UINT16 bldcnt = ppu_worker_io_read16(ctx, GBA_BLDCNT);
	INT32  bld_mode   = SB_BFE(bldcnt, 6, 2);
	// Mode is inert with an empty first-target mask or alpha with no second target;
	// reporting 0 runs the fast-pixel path.
	bool alpha_inert = (bld_mode == 1) && ((bldcnt & 0x3F00) == 0);
	ctx->eff_blend_mode = (((bldcnt & 0x3F) == 0) || alpha_inert) ? 0 : bld_mode;

	// no_windows: all windows off (window[x] unread) -> skip the window passes.
	ctx->no_windows = !win_enabled && !obj_window_enable;
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
	// Pre-decode all 128 OAM slots once for this scanline; all passes index this array.
	const UINT8* oam_full = ctx->oam;
	ppu_worker_obj_geom_t geoms[128];
	bool  obj_hit_mask[128] = { false };
	for (INT32 obj_idx = 0; obj_idx < 128; ++obj_idx)
		ppu_worker_obj_decode(&geoms[obj_idx], oam_full, obj_idx);

	bool display_obj = SB_BFE(dispcnt, 12, 1);
	if (display_obj) {
		INT32 sprite_cycles = SB_BFE(dispcnt, 5, 1) ? 954 : 1210;

		// Pass 0: budget scan in OAM index order. Every OAM slot costs 2 cycles;
		// an OBJ that misses this row or lies fully off-row costs no draw cycles.
		bool obj_budget_ok[128] = { false };
		INT32 last_idx = 0;
		for (INT32 obj_idx = 0; obj_idx < 128; ++obj_idx) {
			sprite_cycles -= 2 * (obj_idx - last_idx);
			last_idx = obj_idx;
			if (sprite_cycles <= 0) break;
			const ppu_worker_obj_geom_t* geom = &geoms[obj_idx];
			if (!geom->valid) continue;
			if (ppu_worker_obj_off_row(geom)) continue;
			if (!ppu_worker_obj_covers_row(geom, sprite_lcd_y)) continue;
			INT32 obj_cycles;
			if (!ppu_worker_obj_draw_cycles(geom, &obj_cycles)) continue;
			sprite_cycles -= obj_cycles;
			obj_budget_ok[obj_idx] = true;
		}

		// With an empty Y-bucket and no windows, skip the OBJ passes when no
		// OBJ covers this row (oam_all_regular: obj_count; else a Y-only scan).
		INT32 n_bucket = ctx->obj_bucket_n[sprite_lcd_y];
		if (n_bucket == 0 && ctx->no_windows) {
			bool any_visible = false;
			if (ctx->oam_all_regular) {
				any_visible = (ctx->obj_count[sprite_lcd_y] > 0);
			} else {
				for (INT32 obj_idx = 0; obj_idx < 128; ++obj_idx) {
					if (!obj_budget_ok[obj_idx]) continue;
					const ppu_worker_obj_geom_t* geom = &geoms[obj_idx];
					if (ppu_worker_obj_covers_row(geom, sprite_lcd_y)) { any_visible = true; break; }
				}
			}
			if (!any_visible) {
				bool line_needs = (ctx->eff_blend_mode == 1);
				ctx->need_second_this_line = line_needs;
				if (line_needs) ctx->frame_needs_second_buf = true;
				return;
			}
		}
		// Pass 1: scan this line's Y-bucket only; X-clip drops off-screen hits.
		for (INT32 bucket_i = 0; bucket_i < n_bucket; ++bucket_i) {
			INT32 obj_idx = ctx->obj_bucket[sprite_lcd_y][bucket_i];
			if (!obj_budget_ok[obj_idx]) continue;
			const ppu_worker_obj_geom_t* geom = &geoms[obj_idx];
			if (!ppu_worker_obj_covers_row(geom, sprite_lcd_y)) continue;
			INT32 x_clip_lo, x_clip_hi;
			ppu_worker_obj_x_clip(geom, &x_clip_lo, &x_clip_hi);
			if (x_clip_hi <= x_clip_lo) continue;
			if (n_hits >= 128) break;
			ppu_worker_fill_hit(&hits[n_hits++], geom, x_clip_lo, x_clip_hi);
			obj_hit_mask[obj_idx] = true;
		}

		// Late catch-up for HBlank DMA-moved sprites (budget-filtered);
		// skipped when oam_uniform and oam_all_regular.
		if (!(ctx->oam_uniform && ctx->oam_all_regular))
		{
			for (INT32 obj_idx = 0; obj_idx < 128; ++obj_idx) {
				if (!obj_budget_ok[obj_idx]) continue;
				if (obj_hit_mask[obj_idx]) continue;
				const ppu_worker_obj_geom_t* geom = &geoms[obj_idx];
				if (!ppu_worker_obj_covers_row(geom, sprite_lcd_y)) continue;
				INT32 x_clip_lo, x_clip_hi;
				ppu_worker_obj_x_clip(geom, &x_clip_lo, &x_clip_hi);
				if (x_clip_hi <= x_clip_lo) continue;
				if (n_hits >= 128) break;
				ppu_worker_fill_hit(&hits[n_hits++], geom, x_clip_lo, x_clip_hi);
				obj_hit_mask[obj_idx] = true;
			}
		}

		// Pass 2: full decode + pixel composition for hit sprites only.
		const UINT32* SB_RESTRICT pal_obj_xrgb = ctx->pal_xrgb + 256;
		for (INT32 hit_i = 0; hit_i < n_hits; ++hit_i) {
			const ppu_worker_hit_t* hit = &hits[hit_i];
			bool  double_size        = hit->double_size;
			INT32 obj_mode           = hit->obj_mode;
			bool  mosaic             = hit->mosaic;
			bool  colors_or_palettes = hit->colors_or_palettes;
			bool  h_flip             = hit->h_flip;
			bool  v_flip             = hit->v_flip;
			INT32 rot_scale_param     = hit->rot_scale_param;
			INT32 tile_base          = hit->tile_base;
			INT32 priority           = hit->priority;
			INT32 palette            = hit->palette;
			INT32 x_coord            = hit->x_coord;
			INT32 x_start            = hit->x_start;
			INT32 x_end              = hit->x_end;
			INT32 y_coord            = hit->y_coord;
			INT32 x_size             = hit->x_size;
			INT32 y_size             = hit->y_size;
			bool  rot_scale          = hit->rot_scale;

			INT32 y_size_eff_hit = y_size * (double_size ? 2 : 1);
			INT32 y_wrap_hit = ppu_worker_obj_y_wrap(y_coord, y_size_eff_hit);
			INT32 sy0    = sprite_lcd_y - y_wrap_hit;
			INT32 sy_eff = sy0;
			if (mosaic) {
				sy_eff = sy0 - ctx->ppu->mosaic_y_counter;
				if (sy_eff < 0) sy_eff = 0;
			}

			// Rot/scale parameters - loaded once per hit sprite.
			INT32 pa = 0, pb = 0, pc = 0, pd = 0;
			INT32 dda_x = 0, dda_y = 0, dda_step_x = 0, dda_step_y = 0;
			bool use_rot_dda = rot_scale && !mosaic;
			if (rot_scale) {
				UINT32 param_base = rot_scale_param * 0x20;
				pa = (INT16)BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->oam + param_base + 0x6));
				pb = (INT16)BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->oam + param_base + 0xe));
				pc = (INT16)BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->oam + param_base + 0x16));
				pd = (INT16)BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->oam + param_base + 0x1e));

				if (use_rot_dda) {
					INT32 sx_start = x_start - x_coord;
					INT64 start_x_q8 = (INT64)sx_start << 8;
					INT64 y1       = (INT64)sy_eff       << 8;
					INT64 obj_ref_x = (INT64)(x_size << (double_size ? 8 : 7));
					INT64 obj_ref_y = (INT64)(y_size << (double_size ? 8 : 7));
					dda_x = (INT32)(pa * (start_x_q8 - obj_ref_x) + pb * (y1 - obj_ref_y) + ((INT64)x_size << 15));
					dda_y = (INT32)(pc * (start_x_q8 - obj_ref_x) + pd * (y1 - obj_ref_y) + ((INT64)y_size << 15));
					dda_step_x = pa * 256;
					dda_step_y = pc * 256;
				}
			}

			// OBJ tile-row byte cache (regular + rot/scale); key = tile*16 + ty.
			INT32  obj_cached_key = -1;
			UINT8  obj_cached_row[8];
			INT32  pal_offset  = palette * 16;   // hoisted out of pixel loop
			// OBJ priority code (layout in the scanline-render header).
			UINT32 obj_pri_col = ((UINT32)(5 - priority) << 29) | ((UINT32)7 << 26) | (1u << 24);
			// 1D OBJ mapping: tile-row stride in tiles (0 for 2D).
			INT32  tile_step = colors_or_palettes ? 2 : 1;
			INT32  obj_tile_row_stride = 0;
			if (!obj_map_2d)
				obj_tile_row_stride = (x_size >> 3) * tile_step;

			UINT32* SB_RESTRICT ft_obj = &ctx->first_target_buffer[x_start];
			for (INT32 x = x_start; x < x_end; ++x) {
				UINT32* SB_RESTRICT ft_ptr = ft_obj++;
				INT32 src_x = x - x_coord;
				INT32 sy = mosaic ? sy_eff : sy0;

				if (mosaic) {
					src_x = ((x / mosaic_x) * mosaic_x - x_coord);
					if (src_x < 0) src_x = 0;
				}

				if (rot_scale) {
					if (use_rot_dda) {
						src_x = (dda_x >> 16);
						sy = (dda_y >> 16);
						dda_x += dda_step_x;
						dda_y += dda_step_y;
					} else {
						INT64 x1 = (INT64)src_x << 8;
						INT64 y1 = (INT64)sy << 8;
						INT64 obj_ref_x = (INT64)(x_size << (double_size ? 8 : 7));
						INT64 obj_ref_y = (INT64)(y_size << (double_size ? 8 : 7));
						INT64 x2 = pa * (x1 - obj_ref_x) + pb * (y1 - obj_ref_y) + ((INT64)x_size << 15);
						INT64 y2 = pc * (x1 - obj_ref_x) + pd * (y1 - obj_ref_y) + ((INT64)y_size << 15);
						src_x = (INT32)(x2 >> 16);
						sy = (INT32)(y2 >> 16);
					}
					if (src_x >= x_size || sy >= y_size || src_x < 0 || sy < 0)
						continue;
				} else {
					if (h_flip) src_x = x_size - src_x - 1;
					if (v_flip) sy = y_size - sy - 1;
				}
				INT32 tx = src_x & 7;
				INT32 ty = sy & 7;

				INT32 tile;
				if (obj_map_2d) {
					INT32 base = colors_or_palettes ? tile_base & ~1 : tile_base;
					tile  = (base + (src_x >> 3) * tile_step) & 0x1f;
					tile |= (base + (sy >> 3) * 32       ) & 0x3e0;
				} else {
					tile = (tile_base + (src_x >> 3) * tile_step + (sy >> 3) * obj_tile_row_stride) & 0x3ff;
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
					if (((col >> 26) & 0x3F) > (((*ft_ptr) >> 26) & 0x3F)) {
						if (transparent) {
							// Transparent OBJ keeps a drawn BG/OBJ pixel: replace priority+tie
							// only, keep flags/BGR.
							if ((((*ft_ptr) >> 26) & 7) != 0)
								*ft_ptr = ((*ft_ptr) & 0x03FFFFFFu) | (col & 0xFC000000u);
						} else {
							*ft_ptr = col;
							// Record a drawn semi-transparent OBJ pixel.
							if (obj_mode == 1) saw_semitrans_pixel = true;
						}
					}
				}
			}
		}
	}

	// WIN0/WIN1 rectangles - render WIN1 first so WIN0 wins on overlap.
	if (!ctx->no_windows && win_enabled) {
		for (INT32 win = 1; win >= 0; --win) {
			if (!SB_BFE(dispcnt, 13 + win, 1))
				continue;
			UINT16 WINH = ppu_worker_io_read16(ctx, GBA_WIN0H + 2 * win);
			UINT16 WINV = ppu_worker_io_read16(ctx, GBA_WIN0V + 2 * win);
			INT32  win_xmin = SB_BFE(WINH, 8, 8);  // x1 (left edge, high byte)
			INT32  win_xmax = SB_BFE(WINH, 0, 8);  // x2 (right edge, low byte)
			INT32  win_ymin = SB_BFE(WINV, 8, 8);
			INT32  win_ymax = SB_BFE(WINV, 0, 8);
			// An X1>X2 (Y1>Y2) range wraps and covers [0,X2) plus [X1,240).
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
			UINT16 win_in     = ppu_worker_io_read16(ctx, GBA_WININ);
			UINT8  win_value = SB_BFE(win_in, win * 8, 6);
			if (win_xmin > win_xmax) {
				for (INT32 x = 0; x < win_xmax; ++x)
					ctx->window[x] = win_value;
				for (INT32 x = win_xmin; x < 240; ++x)
					ctx->window[x] = win_value;
			} else {
				for (INT32 x = win_xmin; x < win_xmax; ++x)
					ctx->window[x] = win_value;
			}
		}
		// Window regions with OBJ disabled -> backdrop.
		UINT32 backdrop_col = (ctx->pal_xrgb[0] & 0x00FFFFFFu) | ((UINT32)0 << 26) | (1u << 24);
		UINT32* SB_RESTRICT ft_window_ptr = ctx->first_target_buffer;
		UINT8*  SB_RESTRICT window_arr = ctx->window;
		for (INT32 x = 0; x < 240; ++x) {
			if (SB_BFE(window_arr[x], 4, 1) == 0)
				ft_window_ptr[x] = backdrop_col;
		}
	}

	// Per-line: set when this scanline has an alpha blend, a drawn semi-transparent
	// OBJ pixel, or an OBJ window; OR-ed into frame_needs_second_buf.
	bool line_needs_second;
	if (ctx->eff_blend_mode == 0)
		line_needs_second = saw_semitrans_pixel || obj_window_enable;
	else
		line_needs_second = (ctx->eff_blend_mode == 1) || saw_semitrans_pixel || obj_window_enable;
	ctx->need_second_this_line = line_needs_second;
	if (line_needs_second) ctx->frame_needs_second_buf = true;
}

// Max cycles to skip from current beam position; non-per-pixel (worker) specialization:
// whole scanline is rendered at cycle 1006, so no per-pixel stepping needed.
static INT32 ppu_worker_compute_max_fast_forward(ppu_worker_render_ctx_t* ctx)
{
	INT32 line_clock = ctx->ppu->scan_clock % GBA_LCD_LINE_CYCLES;
	// inside HBlank (1006..1231): fast-forward to its end
	if (line_clock >= GBA_LCD_HBLANK_START_CYCLE && line_clock < GBA_LCD_HBLANK_END_CYCLE)
		return GBA_LCD_HBLANK_END_CYCLE - line_clock - 1;
	// active/tail (0..1005): worker renders at HBlank start, no per-pixel work here,
	// so fast-forward straight to HBlank entry even inside visible lines.
	return GBA_LCD_HBLANK_START_CYCLE - line_clock - 1;
}

// Convert one XRGB8888 row to out_bpp (BGR24/RGB565) into dst.
static void ppu_worker_write_scanline_out(UINT8* SB_RESTRICT dst,
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

// Scanline render: registers snapshotted at hblank; pixel loop does no IO reads.
// XRGB8888 pixel encoding (32-bit UINT32):
//   [31:29] = 5 - priority (higher wins)
//   [28:26] = tie-breaker (BD=0, BG3=1..BG0=4, OBJ=7)
//   [25]    = semi-transparent OBJ flag (forces alpha blend)
//   [24]    = opaque flag
//   [23:0]  = BGR888 color; sb_exp5[] = 5b->8b ((v<<3)|(v>>2)).
static const UINT8 sb_exp5[32] = {
	0, 8, 16, 24, 33, 41, 49, 57, 66, 74, 82, 90, 99, 107, 115, 123,
	132, 140, 148, 156, 165, 173, 181, 189, 198, 206, 214, 222, 231, 239, 247, 255
};

// Map a pixel's 3-bit tie code (BG0=4, BG1=3, BG2=2, BG3=1, OBJ=7, backdrop=0)
// to its BLDCNT layer bit.
static const UINT8 sb_tie_to_bldcnt_type[8] = { 5, 3, 2, 1, 0, 0, 0, 4 };

// Ghosting strength -> Q8 blend factor in [0,76]; 0 disables the ghost blend.
SB_ALWAYS_INLINE INT32 ppu_worker_ghost_q8_factor(float strength)
{
	INT32 q8 = (INT32)(strength * 76.8f + 0.5f);
	if (q8 < 0)  q8 = 0;
	if (q8 > 76) q8 = 76;
	return q8;
}

static void ppu_worker_render_scanline(ppu_worker_render_ctx_t* ctx, INT32 lcd_y)
{
	// DISPCNT takes effect 2 scanlines late on hardware; gbappu.h reads
	// dispcnt_pipeline[0], so use ctx->ppu's delayed value, not the raw register.
	UINT16 dispcnt    = ctx->ppu->dispcnt_pipeline[0];
	INT32  bg_mode    = SB_BFE(dispcnt, 0, 3);
	INT32  forced_blank = SB_BFE(dispcnt, 7, 1);
	INT32  frame_select  = SB_BFE(dispcnt, 4, 1);
	UINT16 mosaic_reg    = ppu_worker_io_read16(ctx, GBA_MOSAIC);
	INT32  mosaic_x      = SB_BFE(mosaic_reg, 0, 4) + 1;
	INT32  mosaic_y      = SB_BFE(mosaic_reg, 4, 4) + 1;
	UINT16 bldcnt     = ppu_worker_io_read16(ctx, GBA_BLDCNT);
	// Effective effect mode for this line (0 when the mode cannot apply).
	INT32  eff_mode   = ctx->eff_blend_mode;
	UINT16 bldy   = ppu_worker_io_read16(ctx, GBA_BLDY);
	UINT16 bldalpha   = ppu_worker_io_read16(ctx, GBA_BLDALPHA);
	UINT16 green_swap = ppu_worker_io_read16(ctx, GBA_GREENSWP);
	// Loop-invariant scanline flags hoisted so pixel loops read registers.
	// frame_second: any scanline this frame needs alpha tables (tables are per-line).
	// line_second:  this scanline actually has semi-transparent content; only this
	//               scanline pays the st_buf fill + dual-write cost.
	const bool frame_second = ctx->frame_needs_second_buf;
	const bool line_second  = ctx->need_second_this_line;
	const bool no_win       = ctx->no_windows;

	// Blend tables, rebuilt per scanline so HBlank BLDALPHA/BLDY updates apply.
	// Brighten/darken are pre-baked to 8-bit; alpha uses raw 0-16 EVA/EVB with
	// (r*eva + r2*evb)>>4 per pixel, matching gbappu.h's integer divide.
	UINT8 blend_inc8[32], blend_dec8[32];
	INT32 blend_eva_q4, blend_evb_q4;
	bool need_alpha_table  = (eff_mode == 1) || frame_second;
	bool need_bright_table = (eff_mode == 2) || (eff_mode == 3);
	if (need_alpha_table) {
		blend_eva_q4 = SB_BFE(bldalpha, 0, 5); if (blend_eva_q4 > 16) blend_eva_q4 = 16;
		blend_evb_q4 = SB_BFE(bldalpha, 8, 5); if (blend_evb_q4 > 16) blend_evb_q4 = 16;
	} else { blend_eva_q4 = 0; blend_evb_q4 = 0; }
	if (need_bright_table) {
		INT32 evy = SB_BFE(bldy,  0, 5); if (evy > 16) evy = 16;
		for (INT32 i = 0; i < 32; ++i) {
			INT32 inc = i + (((31 - i) * evy) >> 4);
			if (inc > 31) inc = 31;
			blend_inc8[i] = sb_exp5[inc];
			blend_dec8[i] = sb_exp5[(i * (16 - evy)) >> 4];
		}
	}

	// Ghosting Q8 factor - frame-level constant, stored in ctx.
	INT32 ghost_q8       = ctx->ghost_q8;
	bool  ghost_fast   = ctx->ghost_fast;
	INT32 one_minus_ghost_q8 = ctx->one_minus_ghost_q8;

	bool render_bgs = bg_mode <= 5;
	bool mode_ok[4]  = { false, false, false, false };
	INT32 priority[4], char_base_addr[4], map_base_addr[4], bg_size_x[4], bg_size_y[4], size_sel[4];
	INT32 hofs[4], vofs[4], aff_bgx[4], aff_bgy[4], aff_pa[4], aff_pc[4];
	bool  colors[4], mosaic_bg[4], rot_scale[4], wrap_overflow[4];
	INT32 tile_row_y[4], tile_row_base[4], cached_tile_x[4], cached_tile_row_y[4];
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

	// Per-BG state. Bitmap modes (3/4/5) only use BG2 (BG0/1/3 skipped).
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
			char_base_addr[bg] = SB_BFE(bgcnt,  2, 2) * 16 * 1024;
			mosaic_bg[bg] = SB_BFE(bgcnt,  6, 1);
			colors[bg]    = SB_BFE(bgcnt,  7, 1);
			map_base_addr[bg]  = SB_BFE(bgcnt,  8, 5) * 2048;
			wrap_overflow[bg]  = SB_BFE(bgcnt, 13, 1);
			size_sel[bg]     = SB_BFE(bgcnt, 14, 2);
			bg_size_x[bg]    = (size_sel[bg] & 1 ) ? 512 : 256;
			bg_size_y[bg]    = (size_sel[bg] >= 2) ? 512 : 256;
			if (rot_scale[bg] || is_bitmap_mode) {
				bg_size_x[bg] = bg_size_y[bg] = (16 * 8) << size_sel[bg];
				if (bg_mode == 3 || bg_mode == 4) {
					bg_size_x[bg] = 240;
					bg_size_y[bg] = 160;
				} else if (bg_mode == 5) {
					bg_size_x[bg] = 160;
					bg_size_y[bg] = 128;
				}
				colors[bg] = true;
				aff_bgx[bg] = ctx->ppu->aff[bg - 2].render_bgx;
				aff_bgy[bg] = ctx->ppu->aff[bg - 2].render_bgy;
				aff_pa[bg]  = (INT16)ppu_worker_io_read16(ctx, GBA_BG2PA + (bg - 2) * 0x10);
				aff_pc[bg]  = (INT16)ppu_worker_io_read16(ctx, GBA_BG2PC + (bg - 2) * 0x10);
			} else {
				INT16 h16 = ppu_worker_io_read16(ctx, GBA_BG0HOFS + bg * 4);
				INT16 v16 = ppu_worker_io_read16(ctx, GBA_BG0VOFS + bg * 4);
				hofs[bg] = (h16 << 7) >> 7;
				vofs[bg] = (v16 << 7) >> 7;
				INT32 ly    = mosaic_bg[bg] ? (lcd_y / mosaic_y) * mosaic_y : lcd_y;
				INT32 row_y = (vofs[bg] + ly) & (bg_size_y[bg] - 1);
				tile_row_y[bg]     = row_y & 7;
				INT32 ty    = row_y >> 3;
				tile_row_base[bg] = (ty & 31) * 32 + (ty >= 32 ? 32 * 32 * (size_sel[bg] == 3 ? 2 : 1) : 0);
			}
			bg_pri_code[bg] = ((UINT32)(5 - priority[bg]) << 29)
				                | ((UINT32)(4 - bg) << 26)
			                | (1u << 24);
		}
	}

	UINT32* SB_RESTRICT fb_row = ctx->framebuffer + lcd_y * GBA_LCD_W;
	UINT8*  SB_RESTRICT fb_row_bytes = (UINT8*)fb_row;
	UINT32* SB_RESTRICT ft_buf = ctx->first_target_buffer;
	UINT32* SB_RESTRICT st_buf = ctx->second_target_buffer;
	UINT8*  SB_RESTRICT win_ptr = ctx->window;

	// Active-BG list: iterate only enabled BGs instead of all 4.
	INT32 active_bg[4]; INT32 n_active_bg = 0;
	for (INT32 bg = 0; bg < 4; ++bg)
		if (mode_ok[bg]) active_bg[n_active_bg++] = bg;

	// Fast-pixel path: text mode, no effects/windows/green-swap, ghosting fast.
	// BGs render to ft_buf by priority; opaque OBJs pre-drawn. When semi-transparent
	// OBJs force need_second, BG loop also maintains st_buf and a final pass
	// alpha-blends bit25 pixels against it.
	const bool fast_pixel_ok = !is_bitmap_mode
		&& (eff_mode == 0) && no_win && !(green_swap & 1)
		&& ghost_fast && (forced_blank == 0);
	const bool fast_pixel_row       = fast_pixel_ok && !line_second;
	const bool fast_pixel_alpha_row = fast_pixel_ok &&  line_second;

	// Helper: composite new pixel c against two-layer buffers ft/st.
	// If c wins ft, displaced ft is a candidate for st; otherwise c is a
	// candidate for st (layer below ft but above st).
	#define PPU_COMPOSITE_PIXEL(c_, ftp_, stp_) do { \
		UINT32 _c = (c_); \
		UINT32* _ftp = (ftp_); \
		UINT32* _stp = (stp_); \
		if (((_c >> 26) & 0x3F) > (((*_ftp) >> 26) & 0x3F)) { \
			UINT32 _displaced = *_ftp; \
			*_ftp = _c; \
			if (((_displaced >> 26) & 0x3F) > (((*_stp) >> 26) & 0x3F)) *_stp = _displaced; \
		} else { \
			if (((_c >> 26) & 0x3F) > (((*_stp) >> 26) & 0x3F)) *_stp = _c; \
		} \
	} while(0)

	if (fast_pixel_row || fast_pixel_alpha_row) {
		INT32 x;
		UINT32* SB_RESTRICT st_ptr = fast_pixel_alpha_row ? st_buf : 0;

		// Render each active BG; the per-pixel (tie,pri) compare picks the winner.
		for (INT32 bg_i = n_active_bg - 1; bg_i >= 0; --bg_i) {
			INT32 bg = active_bg[bg_i];
			if (fast_pixel_alpha_row) st_ptr = st_buf;
			// BG mode dispatch: rot_scale vs text is determined per BG.
			if (rot_scale[bg] || mosaic_bg[bg] || colors[bg]) {
				// Complex BG (affine/mosaic/8bpp): generic per-pixel logic for this BG only;
				// bg_rot is hoisted so the loop can be unswitched.
				const bool bg_rot = rot_scale[bg];
				UINT32* SB_RESTRICT ft_ptr = ft_buf;
				for (INT32 lcd_x = 0; lcd_x < 240; ++lcd_x) {
					INT32 bg_x, bg_y;
					if (bg_rot) {
						INT32 src_x = mosaic_bg[bg] ? (lcd_x / mosaic_x) * mosaic_x : lcd_x;
						bg_x = (INT32)(((INT64)aff_pa[bg] * src_x + aff_bgx[bg]) >> 8);
						bg_y = (INT32)(((INT64)aff_pc[bg] * src_x + aff_bgy[bg]) >> 8);
						// Bitmap modes treat out-of-bounds as transparent;
						// affine tile BGs wrap when overflow bit is set.
						if (bg_x < 0 || bg_x >= bg_size_x[bg] || bg_y < 0 || bg_y >= bg_size_y[bg]) {
							if (wrap_overflow[bg] == 0 || is_bitmap_mode)
								{ ++ft_ptr; if (fast_pixel_alpha_row) ++st_ptr; continue; }
							bg_x &= bg_size_x[bg] - 1;
							bg_y &= bg_size_y[bg] - 1;
						}
					} else {
						if (mosaic_bg[bg]) {
							bg_x = (hofs[bg] + (lcd_x / mosaic_x) * mosaic_x) & (bg_size_x[bg] - 1);
							bg_y = (vofs[bg] + (lcd_y / mosaic_y) * mosaic_y) & (bg_size_y[bg] - 1);
						} else {
							bg_x = (hofs[bg] + lcd_x) & (bg_size_x[bg] - 1);
							bg_y = (vofs[bg] + lcd_y) & (bg_size_y[bg] - 1);
						}
					}
					INT32 bg_tile_x = bg_x >> 3;
					UINT16 tile_data; INT32 px, py;
					if (bg_rot) {
						INT32 bg_ty = bg_y >> 3, bg_tiles_w = bg_size_x[bg] >> 3;
						INT32 tile_off = bg_ty * bg_tiles_w + bg_tile_x;
						if (bg_tile_x != cached_aff_tile_x[bg] || bg_ty != cached_aff_tile_y[bg]) {
							cached_aff_tile_x[bg] = bg_tile_x;
							cached_aff_tile_y[bg] = bg_ty;
							cached_aff_tile_data[bg] = ctx->vram[map_base_addr[bg] + tile_off];
						}
						tile_data = cached_aff_tile_data[bg];
						px = bg_x & 7; py = bg_y & 7;
					} else {
						if (bg_tile_x != cached_tile_x[bg]) {
							cached_tile_x[bg] = bg_tile_x;
							INT32 toff = tile_row_base[bg] + (bg_tile_x & 31) + (bg_tile_x >= 32 ? 32*32 : 0);
							tile_data = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + map_base_addr[bg] + toff * 2));
							cached_tile_data[bg] = tile_data;
							cached_tile_row_y[bg] = SB_BFE(tile_data,11,1) ? 7 - tile_row_y[bg] : tile_row_y[bg];
						} else { tile_data = cached_tile_data[bg]; }
						px = bg_x & 7;
						if (SB_BFE(tile_data,10,1)) px = 7 - px;
						py = cached_tile_row_y[bg];
					}
					INT32 tile_id = SB_BFE(tile_data,0,10);
					INT32 palette = SB_BFE(tile_data,12,4);
					UINT8 color_idx;
					if (!colors[bg]) {
						INT32 addr = char_base_addr[bg] + tile_id*32 + py*4;
						// VRAM addr >= 0x10000: skip pixel.
						if (SB_UNLIKELY(addr >= 0x10000)) { ++ft_ptr; if (fast_pixel_alpha_row) ++st_ptr; continue; }
						const UINT8* SB_RESTRICT tb = ppu_worker_fetch_tile4(ctx, bg, addr);
						color_idx = (tb[px >> 1] >> ((px & 1) * 4)) & 0xf;
						if (color_idx == 0) { ++ft_ptr; if (fast_pixel_alpha_row) ++st_ptr; continue; }
						color_idx += palette*16;
					} else {
						INT32 addr = char_base_addr[bg] + tile_id*64 + py*8;
						if (SB_UNLIKELY(addr >= 0x10000)) { ++ft_ptr; if (fast_pixel_alpha_row) ++st_ptr; continue; }
						const UINT8* SB_RESTRICT tb = ppu_worker_fetch_tile8(ctx, bg, addr);
						color_idx = tb[px];
						if (color_idx == 0) { ++ft_ptr; if (fast_pixel_alpha_row) ++st_ptr; continue; }
					}
					UINT32 col2 = (ctx->pal_xrgb[color_idx] & 0x00FFFFFFu) | bg_pri_code[bg];
					if (fast_pixel_alpha_row) {
						PPU_COMPOSITE_PIXEL(col2, ft_ptr, st_ptr);
					} else {
						if (((col2 >> 26) & 0x3F) > (((*ft_ptr) >> 26) & 0x3F)) *ft_ptr = col2;
					}
					++ft_ptr;
					if (fast_pixel_alpha_row) ++st_ptr;
				}
			} else {
				// 4bpp text BG fast path: 8-pixel unrolled tiles; head/tail stay per-pixel.
				UINT32* SB_RESTRICT ft_ptr = ft_buf;
				INT32 lcd_x = 0;
				const UINT32* SB_RESTRICT pal = ctx->pal_xrgb;
				UINT32 pri = bg_pri_code[bg];

				// 1) Head: 0-7 pixels until tile boundary (px==0)
				{
					INT32 start_px = (hofs[bg] + 0) & 7;
					if (start_px != 0) {
						INT32 head_count = 8 - start_px;
						if (head_count > 240) head_count = 240;
						for (INT32 i = 0; i < head_count; ++i) {
							INT32 bg_x = (hofs[bg] + lcd_x) & (bg_size_x[bg] - 1);
							INT32 bg_tile_x = bg_x >> 3, px = bg_x & 7;
							UINT16 tile_data; INT32 py;
							if (bg_tile_x != cached_tile_x[bg]) {
								cached_tile_x[bg] = bg_tile_x;
								INT32 toff = tile_row_base[bg] + (bg_tile_x & 31) + (bg_tile_x >= 32 ? 32*32 : 0);
								tile_data = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + map_base_addr[bg] + toff * 2));
								cached_tile_data[bg] = tile_data;
								cached_tile_row_y[bg] = SB_BFE(tile_data,11,1) ? 7 - tile_row_y[bg] : tile_row_y[bg];
							} else { tile_data = cached_tile_data[bg]; }
							bool hflip = SB_BFE(tile_data,10,1);
							if (hflip) px = 7 - px;
							py = cached_tile_row_y[bg];
							INT32 tile_id = SB_BFE(tile_data,0,10);
							INT32 palette = SB_BFE(tile_data,12,4);
							INT32 addr = char_base_addr[bg] + tile_id*32 + py*4;
							if (SB_LIKELY(addr < 0x10000)) {
								const UINT8* SB_RESTRICT tb = ppu_worker_fetch_tile4(ctx, bg, addr);
								UINT8 color_idx = (tb[px >> 1] >> ((px & 1) * 4)) & 0xf;
								if (color_idx != 0) {
									color_idx += palette*16;
									UINT32 col = (pal[color_idx] & 0x00FFFFFFu) | pri;
									if (fast_pixel_alpha_row) {
										PPU_COMPOSITE_PIXEL(col, ft_ptr, st_ptr);
									} else {
										if (((col >> 26) & 0x3F) > (((*ft_ptr) >> 26) & 0x3F)) *ft_ptr = col;
									}
								}
							}
							++ft_ptr; ++lcd_x;
							if (fast_pixel_alpha_row) ++st_ptr;
						}
					}
				}

				// 2) Main: 8-pixel aligned tile blocks
				for (; lcd_x <= 240 - 8; lcd_x += 8) {
					INT32 bg_x_base = (hofs[bg] + lcd_x) & (bg_size_x[bg] - 1);
					INT32 bg_tile_x = bg_x_base >> 3;
					UINT16 tile_data; INT32 py;
					if (bg_tile_x != cached_tile_x[bg]) {
						cached_tile_x[bg] = bg_tile_x;
						INT32 toff = tile_row_base[bg] + (bg_tile_x & 31) + (bg_tile_x >= 32 ? 32*32 : 0);
						tile_data = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + map_base_addr[bg] + toff * 2));
						cached_tile_data[bg] = tile_data;
						cached_tile_row_y[bg] = SB_BFE(tile_data,11,1) ? 7 - tile_row_y[bg] : tile_row_y[bg];
					} else { tile_data = cached_tile_data[bg]; }

					bool hflip = SB_BFE(tile_data,10,1);
					py = cached_tile_row_y[bg];
					INT32 tile_id = SB_BFE(tile_data,0,10);
					INT32 palette = SB_BFE(tile_data,12,4);
					INT32 addr = char_base_addr[bg] + tile_id*32 + py*4;
					UINT32 pal_bank = (UINT32)(palette * 16);

					if (SB_UNLIKELY(addr >= 0x10000)) { ft_ptr += 8; if (fast_pixel_alpha_row) st_ptr += 8; continue; }

					const UINT8* SB_RESTRICT tb = ppu_worker_fetch_tile4(ctx, bg, addr);
					// Skip fully transparent tile
					if (tb[0]==0 && tb[1]==0 && tb[2]==0 && tb[3]==0) {
						ft_ptr += 8; if (fast_pixel_alpha_row) st_ptr += 8; continue;
					}

					if (SB_UNLIKELY(hflip)) {
						// h-flip: nibble-swap + byte-reverse, then the 8-pixel unrolled decode.
						UINT8 b0 = tb[0], b1 = tb[1], b2 = tb[2], b3 = tb[3];
						UINT8 flip_bytes[4];
						flip_bytes[0] = (UINT8)(((b3 & 0x0F) << 4) | (b3 >> 4));
						flip_bytes[1] = (UINT8)(((b2 & 0x0F) << 4) | (b2 >> 4));
						flip_bytes[2] = (UINT8)(((b1 & 0x0F) << 4) | (b1 >> 4));
						flip_bytes[3] = (UINT8)(((b0 & 0x0F) << 4) | (b0 >> 4));
						#define PPU_EMIT_HFLIP_PIXEL(px_) do { \
							UINT8 nibble = (flip_bytes[(px_)>>1] >> (((px_)&1)*4)) & 0xf; \
							if (nibble != 0) { \
								UINT32 c = (pal[nibble + pal_bank] & 0x00FFFFFFu) | pri; \
								if (fast_pixel_alpha_row) { \
									PPU_COMPOSITE_PIXEL(c, ft_ptr, st_ptr); \
								} else { \
									if (((c >> 26) & 0x3F) > (((*ft_ptr) >> 26) & 0x3F)) *ft_ptr = c; \
								} \
							} \
							++ft_ptr; \
							if (fast_pixel_alpha_row) ++st_ptr; \
						} while(0)
						PPU_EMIT_HFLIP_PIXEL(0); PPU_EMIT_HFLIP_PIXEL(1); PPU_EMIT_HFLIP_PIXEL(2); PPU_EMIT_HFLIP_PIXEL(3);
						PPU_EMIT_HFLIP_PIXEL(4); PPU_EMIT_HFLIP_PIXEL(5); PPU_EMIT_HFLIP_PIXEL(6); PPU_EMIT_HFLIP_PIXEL(7);
						#undef PPU_EMIT_HFLIP_PIXEL
					} else {
						// 8 pixels from cached raw bytes, unrolled.
						#define PPU_EMIT_NORMAL_PIXEL(px_) do { \
							UINT8 nibble = (tb[(px_)>>1] >> (((px_)&1)*4)) & 0xf; \
							if (nibble != 0) { \
								UINT32 c = (pal[nibble + pal_bank] & 0x00FFFFFFu) | pri; \
								if (fast_pixel_alpha_row) { \
									PPU_COMPOSITE_PIXEL(c, ft_ptr, st_ptr); \
								} else { \
									if (((c >> 26) & 0x3F) > (((*ft_ptr) >> 26) & 0x3F)) *ft_ptr = c; \
								} \
							} \
							++ft_ptr; \
							if (fast_pixel_alpha_row) ++st_ptr; \
						} while(0)
						PPU_EMIT_NORMAL_PIXEL(0); PPU_EMIT_NORMAL_PIXEL(1); PPU_EMIT_NORMAL_PIXEL(2); PPU_EMIT_NORMAL_PIXEL(3);
						PPU_EMIT_NORMAL_PIXEL(4); PPU_EMIT_NORMAL_PIXEL(5); PPU_EMIT_NORMAL_PIXEL(6); PPU_EMIT_NORMAL_PIXEL(7);
						#undef PPU_EMIT_NORMAL_PIXEL
					}
				}

				// 3) Tail: remaining 0-7 pixels
				for (; lcd_x < 240; ++lcd_x) {
					INT32 bg_x = (hofs[bg] + lcd_x) & (bg_size_x[bg] - 1);
					INT32 bg_tile_x = bg_x >> 3, px = bg_x & 7;
					UINT16 tile_data; INT32 py;
					if (bg_tile_x != cached_tile_x[bg]) {
						cached_tile_x[bg] = bg_tile_x;
						INT32 toff = tile_row_base[bg] + (bg_tile_x & 31) + (bg_tile_x >= 32 ? 32*32 : 0);
						tile_data = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + map_base_addr[bg] + toff * 2));
						cached_tile_data[bg] = tile_data;
						cached_tile_row_y[bg] = SB_BFE(tile_data,11,1) ? 7 - tile_row_y[bg] : tile_row_y[bg];
					} else { tile_data = cached_tile_data[bg]; }
					bool hflip = SB_BFE(tile_data,10,1);
					if (hflip) px = 7 - px;
					py = cached_tile_row_y[bg];
					INT32 tile_id = SB_BFE(tile_data,0,10);
					INT32 palette = SB_BFE(tile_data,12,4);
					INT32 addr = char_base_addr[bg] + tile_id*32 + py*4;
					if (SB_LIKELY(addr < 0x10000)) {
						const UINT8* SB_RESTRICT tb = ppu_worker_fetch_tile4(ctx, bg, addr);
						UINT8 color_idx = (tb[px >> 1] >> ((px & 1) * 4)) & 0xf;
						if (color_idx != 0) {
							color_idx += palette*16;
							UINT32 col = (pal[color_idx] & 0x00FFFFFFu) | pri;
							if (fast_pixel_alpha_row) {
								PPU_COMPOSITE_PIXEL(col, ft_ptr, st_ptr);
							} else {
								if (((col >> 26) & 0x3F) > (((*ft_ptr) >> 26) & 0x3F)) *ft_ptr = col;
							}
						}
					}
					++ft_ptr;
					if (fast_pixel_alpha_row) ++st_ptr;
				}
			}
		}

		// Write framebuffer: strip flags only (8-pixel unrolled);
		// semi-transparent pixels (bit25) are alpha-blended against st_buf.
		if (fast_pixel_row) {
			UINT32* SB_RESTRICT fb_dst = fb_row;
			UINT32* SB_RESTRICT ft_src = ft_buf;
			for (x = 0; x < 240; x += 8) {
				fb_dst[0] = ft_src[0] & 0x00FFFFFFu;
				fb_dst[1] = ft_src[1] & 0x00FFFFFFu;
				fb_dst[2] = ft_src[2] & 0x00FFFFFFu;
				fb_dst[3] = ft_src[3] & 0x00FFFFFFu;
				fb_dst[4] = ft_src[4] & 0x00FFFFFFu;
				fb_dst[5] = ft_src[5] & 0x00FFFFFFu;
				fb_dst[6] = ft_src[6] & 0x00FFFFFFu;
				fb_dst[7] = ft_src[7] & 0x00FFFFFFu;
				fb_dst += 8; ft_src += 8;
			}
		} else {
			// Alpha blend against st_buf, gated by the BLDCNT second-target mask.
			for (x = 0; x < 240; ++x) {
				UINT32 cf = ft_buf[x];
				UINT32 out = cf & 0x00FFFFFFu;
				if ((cf >> 25) & 1) {
					UINT32 cs = st_buf[x];
					UINT8  type2 = sb_tie_to_bldcnt_type[(cs >> 26) & 7];
					if (SB_BFE(bldcnt, 8 + type2, 1)) {
						INT32 red1   = (INT32)(out >> 16) & 0xFF;
						INT32 green1 = (INT32)(out >> 8)  & 0xFF;
						INT32 blue1  = (INT32)out & 0xFF;
						INT32 red2   = (INT32)(cs >> 16) & 0xFF;
						INT32 green2 = (INT32)(cs >> 8)  & 0xFF;
						INT32 blue2  = (INT32)cs & 0xFF;
						INT32 red    = ((red1   >> 3) * blend_eva_q4 + (red2   >> 3) * blend_evb_q4) >> 4;
						INT32 green  = ((green1 >> 3) * blend_eva_q4 + (green2 >> 3) * blend_evb_q4) >> 4;
						INT32 blue   = ((blue1  >> 3) * blend_eva_q4 + (blue2  >> 3) * blend_evb_q4) >> 4;
						if (red   > 31) red   = 31;
						if (green > 31) green = 31;
						if (blue  > 31) blue  = 31;
						out = ((UINT32)sb_exp5[red] << 16) | ((UINT32)sb_exp5[green] << 8) | (UINT32)sb_exp5[blue];
					}
				}
				fb_row[x] = out;
			}
		}
		#undef PPU_COMPOSITE_PIXEL
	} else {
		UINT32* SB_RESTRICT ft_ptr = ft_buf;
		UINT32* SB_RESTRICT st_ptr = line_second ? st_buf : 0;
		UINT8*  SB_RESTRICT win_cur = win_ptr;
		for (INT32 lcd_x = 0; lcd_x < GBA_LCD_W; ++lcd_x) {
		UINT8 window_control = no_win ? (UINT8)0x3F : *win_cur;
		bool  any_bg_visible = render_bgs && (window_control & 0x0F) != 0;

		if (any_bg_visible) {
			for (INT32 bg_i = n_active_bg - 1; bg_i >= 0; --bg_i) {
				INT32 bg = active_bg[bg_i];
				if (SB_BFE(window_control, bg, 1) == 0)
					continue;
				UINT32 col  = 0;
				INT32  bg_x = 0, bg_y = 0;

				if (rot_scale[bg]) {
					INT32 src_x = mosaic_bg[bg] ? (lcd_x / mosaic_x) * mosaic_x : lcd_x;
					bg_x = (INT32)(((INT64)aff_pa[bg] * src_x + aff_bgx[bg]) >> 8);
					bg_y = (INT32)(((INT64)aff_pc[bg] * src_x + aff_bgy[bg]) >> 8);
					// Bitmap modes treat out-of-bounds as transparent;
					// affine tile BGs wrap when overflow bit is set.
					if (bg_x < 0 || bg_x >= bg_size_x[bg] || bg_y < 0 || bg_y >= bg_size_y[bg]) {
						if (wrap_overflow[bg] == 0 || is_bitmap_mode)
							continue;
						bg_x &= bg_size_x[bg] - 1;
						bg_y &= bg_size_y[bg] - 1;
					}
				} else {
					if (mosaic_bg[bg]) {
						bg_x = hofs[bg] + (lcd_x / mosaic_x) * mosaic_x;
						bg_y = vofs[bg] + (lcd_y / mosaic_y) * mosaic_y;
					} else {
						bg_x = hofs[bg] + lcd_x;
						bg_y = vofs[bg] + lcd_y;
					}
				}

				if (is_bitmap_mode) {
					if (bg_mode == 3) {
						UINT16 c16 = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + (bg_x + bg_y * 240) * 2));
						if (c16 == 0x8000) continue;
						{ UINT32 r5=c16&0x1F, g5=(c16>>5)&0x1F, b5=(c16>>10)&0x1F;
						  col = ((UINT32)sb_exp5[r5] << 16) | ((UINT32)sb_exp5[g5] << 8) | (UINT32)sb_exp5[b5]; }
					} else if (bg_mode == 4) {
						UINT8 palette_id = ctx->vram[bg_x + bg_y * 240 + 0xa000 * frame_select];
						if (palette_id == 0) continue;
						col = ctx->pal_xrgb[palette_id] & 0x00FFFFFFu;
					} else { // bg_mode == 5
						UINT16 c16 = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + (bg_x + bg_y * 160) * 2 + 0xa000 * frame_select));
						if (c16 == 0x8000) continue;
						{ UINT32 r5=c16&0x1F, g5=(c16>>5)&0x1F, b5=(c16>>10)&0x1F;
						  col = ((UINT32)sb_exp5[r5] << 16) | ((UINT32)sb_exp5[g5] << 8) | (UINT32)sb_exp5[b5]; }
					}
				} else {
					// Text BG: wrap coords into the BG map (mirrors fast-pixel branch).
					bg_x = bg_x & (bg_size_x[bg] - 1);
					bg_y = bg_y & (bg_size_y[bg] - 1);
					INT32 bg_tile_x = bg_x >> 3;

					UINT16 tile_data;
					INT32 px, py;
					if (rot_scale[bg]) {
						INT32 bg_ty      = bg_y >> 3;
						INT32 bg_tiles_w = bg_size_x[bg] >> 3;
						INT32 tile_off   = bg_ty * bg_tiles_w + bg_tile_x;
						if (bg_tile_x != cached_aff_tile_x[bg] || bg_ty != cached_aff_tile_y[bg]) {
							cached_aff_tile_x[bg] = bg_tile_x;
							cached_aff_tile_y[bg] = bg_ty;
							cached_aff_tile_data[bg] = ctx->vram[map_base_addr[bg] + tile_off];
						}
						tile_data = cached_aff_tile_data[bg];
						px = bg_x & 7;
						py = bg_y & 7;
					} else {
						if (bg_tile_x != cached_tile_x[bg]) {
							cached_tile_x[bg] = bg_tile_x;
							INT32 toff = tile_row_base[bg] + (bg_tile_x & 31) + (bg_tile_x >= 32 ? 32 * 32 : 0);
							tile_data = BURN_ENDIAN_SWAP_INT16(*(UINT16*)(ctx->vram + map_base_addr[bg] + toff * 2));
							cached_tile_data[bg] = tile_data;
							cached_tile_row_y[bg] = SB_BFE(tile_data, 11, 1) ? 7 - tile_row_y[bg] : tile_row_y[bg];
							ctx->cached_pal_bank[bg] = SB_BFE(tile_data, 12, 4) * 16;  // pal_bank cached at tile boundary
						} else {
							tile_data = cached_tile_data[bg];
						}
						px = bg_x & 7;
						if (SB_BFE(tile_data, 10, 1)) px = 7 - px;
						py = cached_tile_row_y[bg];
					}

					INT32 tile_id = SB_BFE(tile_data,  0, 10);
					UINT8 color_idx;
					// affine BGs force 8bpp (colors[bg]=true); pal_bank is unused in that branch.
					INT32 pal_bank_g = rot_scale[bg] ? 0 : ctx->cached_pal_bank[bg];

					if (!colors[bg]) {
						// 4bpp: 32 bytes/tile, 4 bytes/row
						INT32 addr = char_base_addr[bg] + tile_id * 32 + py * 4;
						if (SB_UNLIKELY(addr >= 0x10000))
							continue;
						const UINT8* SB_RESTRICT tb = ppu_worker_fetch_tile4(ctx, bg, addr);
						color_idx = (tb[px >> 1] >> ((px & 1) * 4)) & 0xf;
						if (color_idx == 0) continue;
						color_idx += pal_bank_g;
					} else {
						// 8bpp: 64 bytes/tile, 8 bytes/row
						INT32 addr = char_base_addr[bg] + tile_id * 64 + py * 8;
						if (SB_UNLIKELY(addr >= 0x10000))
							continue;
						const UINT8* SB_RESTRICT tb = ppu_worker_fetch_tile8(ctx, bg, addr);
						color_idx = tb[px];
						if (color_idx == 0) continue;
					}
					col = ctx->pal_xrgb[color_idx] & 0x00FFFFFFu;
				}
				col |= bg_pri_code[bg];

				if (((col >> 26) & 0x3F) > ((*ft_ptr >> 26) & 0x3F)) {
					UINT32 swap_col = *ft_ptr;
					*ft_ptr = col;
					col = swap_col;
				}
				if (line_second && ((col >> 26) & 0x3F) > ((*st_ptr >> 26) & 0x3F))
					*st_ptr = col;
			}
		}

		// Decode to XRGB, apply special effects, write framebuffer; sb_tie_to_bldcnt_type[] maps
		// the tie-code to its BLDCNT bit (BGs use 4-bg), matching gbappu.h.
		UINT32 col  = *ft_ptr;
		INT32  pix_b   = (INT32)(col & 0xFF);
		INT32  pix_g   = (INT32)((col >> 8)  & 0xFF);
		INT32  pix_r   = (INT32)((col >> 16) & 0xFF);
		UINT32 type = sb_tie_to_bldcnt_type[(col >> 26) & 7];

		INT32 blend_mode = eff_mode;
		// Special-effect enable: window bit 5 AND BLDCNT first-target mask.
		bool  effect_enabled = SB_BFE(window_control, 5, 1);

		if ((col >> 25) & 1) {
			// Semi-transparent OBJ: alpha-blend with second-target pixel in st_buf.
			UINT32 col2  = *st_ptr;
			UINT32 type2 = sb_tie_to_bldcnt_type[(col2 >> 26) & 7];
			bool   blend = SB_BFE(bldcnt, 8 + type2, 1);
			if (blend) {
				blend_mode          = 1;
				effect_enabled = true;
			} else {
				effect_enabled &= SB_BFE(bldcnt, type, 1);
			}
		} else {
			effect_enabled &= SB_BFE(bldcnt, type, 1);
		}

		if (effect_enabled) {
			INT32 red = pix_r >> 3, green = pix_g >> 3, blue = pix_b >> 3;
			switch (blend_mode) {
				case 1: {
					UINT32 col2  = *st_ptr;
					UINT32 type2 = sb_tie_to_bldcnt_type[(col2 >> 26) & 7];
					bool  blend  = SB_BFE(bldcnt, 8 + type2, 1);
					if (blend) {
						INT32 blue2  = (INT32)(col2 & 0xFF) >> 3;
						INT32 green2 = (INT32)((col2 >> 8) & 0xFF) >> 3;
						INT32 red2   = (INT32)((col2 >> 16) & 0xFF) >> 3;
						red   = (red   * blend_eva_q4 + red2   * blend_evb_q4) >> 4;
						green = (green * blend_eva_q4 + green2 * blend_evb_q4) >> 4;
						blue  = (blue  * blend_eva_q4 + blue2  * blend_evb_q4) >> 4;
						if (red   > 31) red   = 31;
						if (green > 31) green = 31;
						if (blue  > 31) blue  = 31;
						pix_r = sb_exp5[red];
						pix_g = sb_exp5[green];
						pix_b = sb_exp5[blue];
					}
				}
					break;
				case 2:
					pix_r = blend_inc8[red];
					pix_g = blend_inc8[green];
					pix_b = blend_inc8[blue];
					break;
				case 3:
					pix_r = blend_dec8[red];
					pix_g = blend_dec8[green];
					pix_b = blend_dec8[blue];
					break;
			}
		}

		if (forced_blank) {
			if (ctx->stop_mode) { pix_r = pix_g = pix_b = 0; }
			else                { pix_r = pix_g = pix_b = 255; }
		}

		INT32 out_r = pix_r;
		INT32 out_g = pix_g;
		INT32 out_b = pix_b;

		if (green_swap & 1) {
			// Green-swap: G written to adjacent pixel's G byte
			UINT8* SB_RESTRICT fb_b = fb_row_bytes;
			INT32 pix_ofs = lcd_x * 4;
			if (ghost_fast) {
				fb_b[pix_ofs + 0] = (UINT8)out_b;
				fb_b[pix_ofs + 2] = (UINT8)out_r;
				fb_b[pix_ofs + 3] = 0x00;
				INT32 green_ofs = (pix_ofs & 4) ? (pix_ofs + 1 - 4) : (pix_ofs + 1 + 4);
				fb_b[green_ofs] = (UINT8)out_g;
			} else {
				INT32 prev_b = (INT32)(UINT8)fb_b[pix_ofs + 0];
				INT32 prev_r = (INT32)(UINT8)fb_b[pix_ofs + 2];
				fb_b[pix_ofs + 0] = (UINT8)((out_b * one_minus_ghost_q8 + prev_b * ghost_q8) >> 8);
				fb_b[pix_ofs + 2] = (UINT8)((out_r * one_minus_ghost_q8 + prev_r * ghost_q8) >> 8);
				fb_b[pix_ofs + 3] = 0x00;
				INT32 green_ofs = (pix_ofs & 4) ? (pix_ofs + 1 - 4) : (pix_ofs + 1 + 4);
				INT32 prev_g = (INT32)(UINT8)fb_b[green_ofs];
				fb_b[green_ofs] = (UINT8)((out_g * one_minus_ghost_q8 + prev_g * ghost_q8) >> 8);
			}
		} else if (ghost_fast) {
			fb_row[lcd_x] = ((UINT32)out_r << 16) | ((UINT32)out_g << 8) | (UINT32)out_b;
		} else {
			UINT32 prev_pix = fb_row[lcd_x];
			INT32 prev_b = (INT32)(prev_pix & 0xFF);
			INT32 prev_g = (INT32)((prev_pix >> 8) & 0xFF);
			INT32 prev_r = (INT32)((prev_pix >> 16) & 0xFF);
			INT32 new_b = (out_b * one_minus_ghost_q8 + prev_b * ghost_q8) >> 8;
			INT32 new_g = (out_g * one_minus_ghost_q8 + prev_g * ghost_q8) >> 8;
			INT32 new_r = (out_r * one_minus_ghost_q8 + prev_r * ghost_q8) >> 8;
			fb_row[lcd_x] = ((UINT32)new_r << 16) | ((UINT32)new_g << 8) | (UINT32)new_b;
		}
		++ft_ptr; if (line_second) ++st_ptr; ++win_cur;
		}
	}

	// Per-scanline output: convert after the pixel loop (green_swap pairs pixels).
	if (ctx->out_ptr) {
		UINT8* SB_RESTRICT dst = ctx->out_ptr + lcd_y * GBA_LCD_W * ctx->out_bpp;
		const UINT32* SB_RESTRICT src = ctx->framebuffer + lcd_y * GBA_LCD_W;
		ppu_worker_write_scanline_out(dst, src, ctx->out_bpp);
	}
}

// Whether sprite obj_idx covers scanline y (mirrors the OBJ Y-bucket builder).
SB_ALWAYS_INLINE bool ppu_worker_obj_row_covers(const UINT8* oam_row, INT32 obj_idx, INT32 y)
{
	UINT32 attr01 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(oam_row + obj_idx * 8));
	UINT16 attr0 = (UINT16)attr01;
	UINT16 attr1 = (UINT16)(attr01 >> 16);
	if (SB_BFE(attr0, 9, 1) && !SB_BFE(attr0, 8, 1)) return false;
	bool  rot_scale   = SB_BFE(attr0, 8, 1);
	bool  double_size = SB_BFE(attr0, 9, 1) && rot_scale;
	INT32 y_coord   = SB_BFE(attr0, 0, 8);
	INT32 obj_shape = SB_BFE(attr0, 14, 2);
	INT32 obj_size  = SB_BFE(attr1, 14, 2);
	INT32 y_size    = gba_obj_ysize[obj_size * 4 + obj_shape] * (double_size ? 2 : 1);
	INT32 y_start   = ppu_worker_obj_y_wrap(y_coord, y_size);
	return (y >= y_start && y < y_start + y_size);
}

// True when scanline y renders identically to the same scanline of prev_snap:
// same registers, affine state, DISPCNT pipeline, palette and OBJ content. The
// OBJ side only inspects sprites that actually cover this scanline.
static bool ppu_worker_line_reusable(const ppu_worker_snapshot_t* snap,
                                     const ppu_worker_snapshot_t* prev_snap, INT32 y)
{
	const ppu_worker_line_state_t* current_line  = &snap->line_state[y];
	const ppu_worker_line_state_t* previous_line = &prev_snap->line_state[y];
	if (memcmp(current_line->io, previous_line->io, GBAPPU_WORKER_IO_SIZE) != 0) return false;
	if (current_line->aff_bgx[0] != previous_line->aff_bgx[0]
	 || current_line->aff_bgy[0] != previous_line->aff_bgy[0]
	 || current_line->aff_bgx[1] != previous_line->aff_bgx[1]
	 || current_line->aff_bgy[1] != previous_line->aff_bgy[1]) return false;
	if (current_line->dispcnt_pipeline[0] != previous_line->dispcnt_pipeline[0]
	 || current_line->dispcnt_pipeline[1] != previous_line->dispcnt_pipeline[1]
	 || current_line->dispcnt_pipeline[2] != previous_line->dispcnt_pipeline[2]) return false;
	if (memcmp(ppu_worker_line_palette(snap, y), ppu_worker_line_palette(prev_snap, y), 1024) != 0)
		return false;
	const UINT8* current_oam  = ppu_worker_line_oam(snap, y);
	const UINT8* previous_oam = ppu_worker_line_oam(prev_snap, y);
	if (memcmp(current_oam, previous_oam, 1024) == 0) return true;
	for (INT32 obj_idx = 0; obj_idx < 128; ++obj_idx) {
		if (memcmp(current_oam + obj_idx * 8, previous_oam + obj_idx * 8, 8) == 0) continue;
		if (ppu_worker_obj_row_covers(current_oam, obj_idx, y)
		 || ppu_worker_obj_row_covers(previous_oam, obj_idx, y)) return false;
	}
	// Rot/scale matrices referenced by sprites that cover this line; their parameter
	// words live in the OAM area and can change without touching the OBJ entry.
	for (INT32 obj_idx = 0; obj_idx < 128; ++obj_idx) {
		UINT32 attr01 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(current_oam + obj_idx * 8));
		if (!SB_BFE((UINT16)attr01, 8, 1)) continue;
		if (!ppu_worker_obj_row_covers(current_oam, obj_idx, y)) continue;
		UINT32 param_base = (UINT32)SB_BFE((UINT16)(attr01 >> 16), 9, 5) * 0x20;
		for (INT32 word_offset = 0x6; word_offset <= 0x1e; word_offset += 8) {
			if (memcmp(current_oam + param_base + word_offset,
			           previous_oam + param_base + word_offset, 2) != 0)
				return false;
		}
	}
	return true;
}

// Render one captured snapshot into out_fb (XRGB8888, GBA_LCD_W*GBA_LCD_H).
static bool ppu_worker_render_frame_into(ppu_worker_snapshot_t* snap, UINT32* out_fb,
                                                const UINT32* ghost_fb, ppu_worker_t* worker, INT32 slot_idx)
{
	gba_ppu_t ppu_state;
	UINT32    first_buf[GBA_LCD_W];
	UINT32    second_buf[GBA_LCD_W];
	UINT8     win_buf[GBA_LCD_W];

	memset(&ppu_state, 0, sizeof(ppu_state));

	ppu_worker_render_ctx_t ctx;
	UINT8* vram_work = NULL;
	if (snap->vram_journal && !snap->vram_journal_overflow) {
		// Mid-frame VRAM writes: replay the journal onto the line-0 base.
		memcpy(worker->vram_scratch, worker->vram_shadow, 128 * 1024);
		vram_work = worker->vram_scratch;
		ctx.vram  = vram_work;
	} else {
		// No mid-frame writes (or journal overflow): read the vblank baseline directly.
		ctx.vram  = worker->vram_shadow;
	}
	ctx.oam                  = NULL;
	ctx.palette              = NULL;
	ctx.io                   = NULL;
	ctx.ppu                  = &ppu_state;
	ctx.first_target_buffer  = first_buf;
	ctx.second_target_buffer = second_buf;
	ctx.window               = win_buf;
	ctx.framebuffer          = out_fb;
	ctx.out_ptr              = (worker->out_bpp == 4) ? NULL : worker->out_buf[slot_idx];
	ctx.out_bpp              = worker->out_bpp;
	ctx.stop_mode            = snap->stop_mode;
	{
		// Frame-level OAM uniformity: all scanlines equal line 0.
		bool uniform = true;
		if (snap->oam_per_line) {
			const UINT8* oam_ref = snap->line_state[0].oam;
			for (INT32 y = 1; y < GBA_LCD_H; ++y) {
				if (memcmp(snap->line_state[y].oam, oam_ref, 1024) != 0) { uniform = false; break; }
			}
		}
		ctx.oam_uniform = uniform;
	}
	{
		// Frame-level OAM regularity: every enabled OBJ is regular normal (non-rot/scale,
		// mosaic off, obj_mode=0 normal, no OBJ window sprites); pre-count visible
		// OBJs per scanline.
		UINT16 dispcnt0 = BURN_ENDIAN_SWAP_INT16(*(const UINT16*)(snap->line_state[0].io + (GBA_DISPCNT & 0x3FF)));
		bool   obj_window_enable = SB_BFE(dispcnt0, 15, 1);
		bool   all_regular = !obj_window_enable;
		const UINT8* oam0 = snap->line_state[0].oam;
		for (INT32 y = 0; y < GBA_LCD_H; ++y) ctx.obj_count[y] = 0;
		if (all_regular) {
			for (INT32 obj_idx = 0; obj_idx < 128; ++obj_idx) {
				UINT32 attr01 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(oam0 + obj_idx * 8));
				UINT16 attr0 = (UINT16)attr01;
				UINT16 attr1 = (UINT16)(attr01 >> 16);
				bool rot_scale   = SB_BFE(attr0, 8, 1);
				bool obj_disable = SB_BFE(attr0, 9, 1) && !rot_scale;
				if (obj_disable) continue;
				INT32 obj_mode   = SB_BFE(attr0, 10, 2);
				if (SB_BFE(attr0, 12, 1)) { all_regular = false; break; }
				if (rot_scale) { all_regular = false; break; }
				if (obj_mode != 0) { all_regular = false; break; }
				INT32 obj_shape = SB_BFE(attr0, 14, 2);
				INT32 obj_size  = SB_BFE(attr1, 14, 2);
				INT32 y_size    = gba_obj_ysize[obj_size * 4 + obj_shape];
				INT32 y_coord   = SB_BFE(attr0, 0, 8);
				INT32 y_start   = ppu_worker_obj_y_wrap(y_coord, y_size);
				for (INT32 y = y_start; y < y_start + y_size; ++y) {
					if (y < 0 || y >= GBA_LCD_H) {
						if (y < 0) continue;
						break;
					}
					ctx.obj_count[y] += 1;
				}
			}
		}
		ctx.oam_all_regular = all_regular;
	}

	// Per-frame ghosting Q8 factor - frame-level constant, compute once.
	INT32 frame_ghost_q8 = ppu_worker_ghost_q8_factor(snap->ghosting_strength);
	ctx.ghost_q8           = frame_ghost_q8;
	ctx.one_minus_ghost_q8 = 256 - frame_ghost_q8;
	ctx.ghost_fast         = (frame_ghost_q8 == 0);

	// Per-scanline OBJ Y buckets: 64 sprites/line cap, rebuilt from
	// that line's OAM snapshot so HBlank DMA moves/creates sprites are seen.
	INT32 obj_bucket[GBA_LCD_H][64];
	INT32 obj_bucket_n[GBA_LCD_H];

	// Initialize frame-level tile cache (per BG; invalidated on tile/py change).
	ctx.tile_fetch_key[0] = ctx.tile_fetch_key[1] = ctx.tile_fetch_key[2] = ctx.tile_fetch_key[3] = -1;
	ctx.tile_fetch_valid[0] = ctx.tile_fetch_valid[1] = ctx.tile_fetch_valid[2] = ctx.tile_fetch_valid[3] = false;
	ctx.cached_pal_bank[0]=ctx.cached_pal_bank[1]=ctx.cached_pal_bank[2]=ctx.cached_pal_bank[3] = 0;
	ctx.obj_bucket   = (const INT32(*)[64])obj_bucket;
	ctx.obj_bucket_n = obj_bucket_n;
	ctx.no_windows   = false;
	ctx.frame_needs_second_buf = false;
	ctx.need_second_this_line  = false;
	// Seed the ctx palette cache from the pre-expanded frame-level palette
	// when palette_per_line is false.
	if (!snap->palette_per_line) {
		memcpy(ctx.pal_snapshot, snap->line_state[0].palette, 1024);
		memcpy(ctx.pal_xrgb,     snap->pal_xrgb,              512 * sizeof(UINT32));
		ctx.pal_valid = true;
	} else {
		ctx.pal_valid = false;
	}

	// Seed backbuf from the previous frame when ghosting is on; out_buf needs
	// no zero-init (overwritten per scanline at bpp 2/3).
	const INT32 fb_bytes = GBA_LCD_W * GBA_LCD_H * 4;
	if (frame_ghost_q8 > 0 && ghost_fb)
		memcpy(out_fb, ghost_fb, fb_bytes);

	// Per-line reuse source: the previously presented frame's row, used only when
	// neither frame has mid-frame VRAM writes.
	const UINT32* prev_rows = NULL;
	ppu_worker_snapshot_t* prev_snap = NULL;
	if (frame_ghost_q8 == 0 && worker->have_first_frame
	    && !snap->vram_journal && !snap->vram_journal_overflow) {
		int front_slot = worker->front_idx;
		if (front_slot >= 0 && front_slot != slot_idx) {
			ppu_worker_snapshot_t* candidate = &worker->snapshots[front_slot];
			INT32 prev_ghost_q8 = ppu_worker_ghost_q8_factor(candidate->ghosting_strength);
			if (prev_ghost_q8 == 0 && !candidate->vram_journal && !candidate->vram_journal_overflow
			    && candidate->stop_mode == snap->stop_mode) {
				prev_snap = candidate;
				prev_rows = worker->backbuf[front_slot];
			}
		}
	}

	for (INT32 y = 0; y < GBA_LCD_H; ++y) {
		if (worker->exiting || worker->cancel_render) return false;
		if (prev_snap != NULL && ppu_worker_line_reusable(snap, prev_snap, y)) {
			// Identical inputs: the previously presented row is the rendered row.
			UINT32*       dst_row = out_fb + y * GBA_LCD_W;
			const UINT32* src_row = prev_rows + y * GBA_LCD_W;
			memcpy(dst_row, src_row, GBA_LCD_W * 4);
			if (ctx.out_ptr)
				ppu_worker_write_scanline_out(ctx.out_ptr + y * GBA_LCD_W * ctx.out_bpp,
				                              dst_row, ctx.out_bpp);
			continue;
		}
		if (vram_work != NULL && y > 0) {
			// Writes captured during hblank of line (y-1) become visible at line y.
			UINT32 lo = snap->vram_line_lo[y-1];
			UINT32 hi = snap->vram_line_hi[y-1];
			if (lo < hi)
				memcpy(vram_work + lo, snap->vram_pool + snap->vram_line_off[y-1], hi - lo);
		}
		ppu_worker_install_io(&ctx, &snap->line_state[y], snap->stop_mode);
		ctx.oam     = (UINT8*)ppu_worker_line_oam(snap, y);
		ctx.palette = (UINT8*)ppu_worker_line_palette(snap, y);
		{
			if (!ctx.pal_valid || memcmp(ctx.pal_snapshot, ctx.palette, 1024) != 0) {
				memcpy(ctx.pal_snapshot, ctx.palette, 1024);
				ctx.pal_valid = true;
				ppu_worker_expand_palette(ctx.pal_xrgb, ctx.palette);
			}
		}
		// Build OBJ Y bucket for this scanline from its own OAM snapshot; the
		// per-line rebuild makes HBlank DMA OAM writes visible.
		obj_bucket_n[y] = 0;
		{
			const UINT8* oam_row = ctx.oam;
			for (INT32 obj_idx = 0; obj_idx < 128; ++obj_idx) {
				UINT32 attr01 = BURN_ENDIAN_SWAP_INT32(*(const UINT32*)(oam_row + obj_idx * 8));
				UINT16 attr0 = (UINT16)attr01;
				UINT16 attr1 = (UINT16)(attr01 >> 16);
				if (SB_BFE(attr0, 9, 1) && !SB_BFE(attr0, 8, 1))
					continue;
				bool  rot_scale_r = SB_BFE(attr0, 8, 1);
				bool  double_size_r = SB_BFE(attr0, 9, 1) && rot_scale_r;
				INT32 y_coord_r   = SB_BFE(attr0, 0, 8);
				INT32 obj_shape_r = SB_BFE(attr0, 14, 2);
				INT32 obj_size_r  = SB_BFE(attr1, 14, 2);
				INT32 y_size_r    = gba_obj_ysize[obj_size_r * 4 + obj_shape_r] * (double_size_r ? 2 : 1);
				INT32 y_start_r = ppu_worker_obj_y_wrap(y_coord_r, y_size_r);
				INT32 y_end_r = y_start_r + y_size_r;
				if (y >= y_start_r && y < y_end_r && obj_bucket_n[y] < 64) {
					// Horizontal cull: fully off-screen sprites never enter
					// the bucket, saving decode + hit-record + pixel loop.
					INT32 x_raw_r  = SB_BFE(attr1, 0, 9);
					INT32 xs_r     = gba_obj_xsize[obj_size_r * 4 + obj_shape_r] * (double_size_r ? 2 : 1);
					bool right_off = x_raw_r >= GBA_LCD_W && (x_raw_r + xs_r) < 512;
					INT16 xc_r     = (INT16)x_raw_r;
					if (SB_BFE(x_raw_r, 8, 1)) xc_r |= (INT16)0xfe00;
					bool left_off  = (xc_r + xs_r) <= 0;
					if (!right_off && !left_off) {
						obj_bucket[y][obj_bucket_n[y]++] = obj_idx;
					}
				}
			}
		}
		UINT32 backdrop = (ctx.pal_xrgb[0] & 0x00FFFFFFu) | ((UINT32)0 << 26) | (1u << 24);
		ppu_worker_fill_row(first_buf, backdrop);
		ppu_worker_render_objs(&ctx, y);
		if (ctx.need_second_this_line) ppu_worker_fill_row(second_buf, backdrop);
		ppu_worker_render_scanline(&ctx, y);
	}
	return true;
}

// --- Snapshot capture (main thread) --------------------------------------

// HBlank snapshot: IO (96B) + OAM (1KB) + palette (1KB) + this line's VRAM
// delta (vram_shadow is seeded at publish; later lines append their dirty range).
static void ppu_worker_hblank_snapshot(ppu_worker_t* worker, gba_t* gba, INT32 lcd_y)
{
	if (lcd_y < 0 || lcd_y >= GBA_LCD_H) return;

	ppu_worker_snapshot_t* snap = &worker->snapshots[worker->write_snap_idx];
	ppu_worker_line_state_t* line  = &snap->line_state[lcd_y];
	// IO (96B): always copied - HBlank DMA can change WIN/BGxHOFS/BLDCNT/MOSAIC.
	memcpy(line->io, gba->mem.io, GBAPPU_WORKER_IO_SIZE);

	// OAM (1KB) / palette (1KB): line 0 stores the frame-level copy; later lines
	// store their own only after a mid-frame write, and already-captured lines are
	// backfilled with line 0's copy before switching to per-line capture.
	if (lcd_y == 0) {
		snap->oam_per_line     = false;
		snap->palette_per_line = false;
		worker->oam_write_base      = worker->oam_write_seq;
		worker->palette_write_base  = worker->palette_write_seq;
		memcpy(line->oam,     gba->mem.oam,     1024);
		memcpy(line->palette, gba->mem.palette, 1024);
		// Pre-expand the frame-level palette from the line-0 snapshot.
		ppu_worker_expand_palette(snap->pal_xrgb, line->palette);
	} else {
		INT32 yy;
		if (snap->oam_per_line || worker->oam_write_seq != worker->oam_write_base) {
			if (!snap->oam_per_line) {
				for (yy = 1; yy < lcd_y; ++yy)
					memcpy(snap->line_state[yy].oam, snap->line_state[0].oam, 1024);
				snap->oam_per_line = true;
			}
			memcpy(line->oam, gba->mem.oam, 1024);
		}
		if (snap->palette_per_line || worker->palette_write_seq != worker->palette_write_base) {
			if (!snap->palette_per_line) {
				for (yy = 1; yy < lcd_y; ++yy)
					memcpy(snap->line_state[yy].palette, snap->line_state[0].palette, 1024);
				snap->palette_per_line = true;
			}
			memcpy(line->palette, gba->mem.palette, 1024);
		}
	}

	// VRAM checkpoint: writes since the last hblank belong to the next line.
	{
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
				// Pool full: stop recording mid-frame writes for this frame.
				snap->vram_journal_overflow = true;
			}
		}
	}
}

// Line-start snapshot: affine reference points + DISPCNT pipeline.
static void ppu_worker_line_start_snapshot(ppu_worker_t* worker, gba_t* gba, INT32 lcd_y)
{
	if (lcd_y < 0 || lcd_y >= GBA_LCD_H) return;

	ppu_worker_snapshot_t* snap = &worker->snapshots[worker->write_snap_idx];
	ppu_worker_line_state_t* line = &snap->line_state[lcd_y];
	line->aff_bgx[0] = gba->ppu.aff[0].render_bgx;
	line->aff_bgy[0] = gba->ppu.aff[0].render_bgy;
	line->aff_bgx[1] = gba->ppu.aff[1].render_bgx;
	line->aff_bgy[1] = gba->ppu.aff[1].render_bgy;
	line->dispcnt_pipeline[0] = gba->ppu.dispcnt_pipeline[0];
	line->dispcnt_pipeline[1] = gba->ppu.dispcnt_pipeline[1];
	line->dispcnt_pipeline[2] = gba->ppu.dispcnt_pipeline[2];
}

// VBlank entry: capture frame-level render settings (VRAM is copied at publish).
static void ppu_worker_vblank_entry(ppu_worker_t* worker, gba_t* gba)
{
	ppu_worker_snapshot_t* snap = &worker->snapshots[worker->write_snap_idx];
	snap->ghosting_strength = gba->ppu.ghosting_strength;
	snap->stop_mode         = gba->stop_mode;
}

// --- Job/done event signaling (counting semaphore) -----------------------
#if GBAPPU_WORKER_HAVE_PTHREAD

static inline void ppu_worker_evt_wait(ppu_worker_evt_t* evt) {
	pthread_mutex_lock(&evt->mutex);
	while (evt->count == 0)
		pthread_cond_wait(&evt->cond, &evt->mutex);
	evt->count--;
	pthread_mutex_unlock(&evt->mutex);
}
static inline bool ppu_worker_evt_trywait(ppu_worker_evt_t* evt) {
	pthread_mutex_lock(&evt->mutex);
	bool ok = (evt->count > 0);
	if (ok) evt->count--;
	pthread_mutex_unlock(&evt->mutex);
	return ok;
}
static inline void ppu_worker_evt_post(ppu_worker_evt_t* evt) {
	pthread_mutex_lock(&evt->mutex);
	if (evt->count < GBAPPU_WORKER_N_BUFFERS + 1)
		evt->count++;
	pthread_cond_signal(&evt->cond);
	pthread_mutex_unlock(&evt->mutex);
}
static inline void ppu_worker_evt_init(ppu_worker_evt_t* evt) {
	pthread_mutex_init(&evt->mutex, NULL);
	pthread_cond_init(&evt->cond, NULL);
	evt->count = 0;
}
static inline void ppu_worker_evt_destroy(ppu_worker_evt_t* evt) {
	pthread_cond_destroy(&evt->cond);
	pthread_mutex_destroy(&evt->mutex);
}

static inline void ppu_worker_plat_wait_job(ppu_worker_t* worker)     { ppu_worker_evt_wait(&worker->job_evt); }
static inline void ppu_worker_plat_release_done(ppu_worker_t* worker) { ppu_worker_evt_post(&worker->done_evt); }
static inline bool ppu_worker_plat_trywait_done(ppu_worker_t* worker) { return ppu_worker_evt_trywait(&worker->done_evt); }
static inline void ppu_worker_plat_wait_done(ppu_worker_t* worker)    { ppu_worker_evt_wait(&worker->done_evt); }
static inline void ppu_worker_plat_release_job(ppu_worker_t* worker)  { ppu_worker_evt_post(&worker->job_evt); }
static inline void ppu_worker_plat_mb(void)                      { __sync_synchronize(); }

#elif GBAPPU_WORKER_HAVE_WINTHREAD

static inline void ppu_worker_plat_wait_job(ppu_worker_t* worker)     { WaitForSingleObject(worker->job_evt, INFINITE); }
static inline void ppu_worker_plat_release_done(ppu_worker_t* worker) { ReleaseSemaphore(worker->done_evt, 1, NULL); }
static inline bool ppu_worker_plat_trywait_done(ppu_worker_t* worker) { return (WaitForSingleObject(worker->done_evt, 0) == WAIT_OBJECT_0); }
static inline void ppu_worker_plat_wait_done(ppu_worker_t* worker)    { WaitForSingleObject(worker->done_evt, INFINITE); }
static inline void ppu_worker_plat_release_job(ppu_worker_t* worker)  { ReleaseSemaphore(worker->job_evt, 1, NULL); }
static inline void ppu_worker_plat_mb(void)                      { MemoryBarrier(); }

#endif

// Collect the single in-flight job if any; advances front_idx to the completed slot.
static inline void ppu_worker_collect_inflight(ppu_worker_t* worker)
{
	if (worker->inflight <= 0) return;
	ppu_worker_plat_wait_done(worker);
	worker->inflight    = worker->inflight - 1;
	worker->worker_busy = false;
	worker->front_idx   = worker->ready_back_idx;
}

// --- Worker thread main loop ---------------------------------------------

static inline void ppu_worker_process_job(ppu_worker_t* worker)
{
	int slot_idx  = worker->read_snap_idx;
	int front_slot = worker->front_idx;
	UINT32* ghost_src = (front_slot >= 0 && worker->have_first_frame)
		? worker->backbuf[front_slot] : NULL;
	// Render into backbuf (XRGB8888); at out_bpp 2/3 the scanline renderer
	// writes native pixels to out_buf directly.
	bool completed = ppu_worker_render_frame_into(&worker->snapshots[slot_idx],
	                             worker->backbuf[slot_idx], ghost_src, worker, slot_idx);

	ppu_worker_plat_mb();
	// Cancelled renders still signal done (ready_back_idx = -1); main thread
	// does not advance front_slot and overwrites backbuf on the next load.
	worker->ready_back_idx = completed ? slot_idx : -1;
	ppu_worker_plat_release_done(worker);
}

// VBlank publish - one in-flight job, triple-buffered: copy VRAM, collect the
// previous job (blocking), submit, then present zero-copy (scratch on frame 0).
static void ppu_worker_vblank_publish(ppu_worker_t* worker, gba_t* gba,
                                      gba_scratch_t* scratch, bool want_render)
{
	if (!worker->enabled) {
		gba->framebuffer = scratch->framebuffer;
		gba->framebuffer_is_direct_copy = false;
		return;
	}

	int write_idx = worker->write_snap_idx;

	// Wait for the previous job so the submit below has a free slot.
	ppu_worker_collect_inflight(worker);

	// Seed the shared line-0 VRAM baseline from the current state.
	memcpy(worker->vram_shadow, gba->mem.vram, 128 * 1024);
	// The journal/pool of the slot being published were filled by this frame's
	// line snapshots and must survive until the worker renders it; the slot is
	// cleared by ppu_worker_reset_slot when it is recycled as write slot.
	gba->ppu.vram_dirty_lo                        = 0xFFFFFFFFu;
	gba->ppu.vram_dirty_hi                        = 0u;
	// line_state[0] OAM/palette was seeded by ppu_worker_reset_slot at submit.

	// Submit as a new job (inflight is 0 after the collect above).
	bool should_submit = want_render || !worker->have_first_frame;

	if (should_submit) {
		if (!worker->have_first_frame) {
			// First frame: black scratch until the worker finishes.
			memset(scratch->framebuffer, 0, sizeof(scratch->framebuffer));
			gba->framebuffer = scratch->framebuffer;
			gba->framebuffer_is_direct_copy = false;
		}

		worker->read_snap_idx = write_idx;
		worker->write_snap_idx = (write_idx + 1) % GBAPPU_WORKER_N_BUFFERS;
		ppu_worker_reset_slot(worker, worker->write_snap_idx, gba);
		worker->worker_busy = true;
		worker->inflight = worker->inflight + 1;
		ppu_worker_plat_mb();
		ppu_worker_plat_release_job(worker);
		worker->have_first_frame = true;
	} else {
		// Keep the slot: the next frame overwrites it, so its journal and pool
		// are recycled here instead of at publish.
		worker->write_snap_idx = write_idx;
		ppu_worker_reset_slot(worker, write_idx, gba);
	}

	// Zero-copy present: point gba->framebuffer at the finished output.
	if (worker->have_first_frame && worker->front_idx >= 0) {
		if (worker->out_bpp == 4) {
			// 32bpp: backbuf already matches output format (zero-copy).
			gba->framebuffer = (UINT8*)worker->backbuf[worker->front_idx];
		} else {
			// 16/24bpp: per-scanline output lives in out_buf.
			gba->framebuffer = worker->out_buf[worker->front_idx];
		}
		gba->framebuffer_is_direct_copy = true;
	} else if (!gba->framebuffer || gba->framebuffer == (UINT8*)0xCDCDCDCD) {
		// No frame yet (0xCDCDCDCD = MSVC debug-heap fill): fall back to scratch.
		gba->framebuffer = scratch->framebuffer;
		gba->framebuffer_is_direct_copy = false;
	}
}

static void ppu_worker_drain_for_mode_switch(ppu_worker_t* worker)
{
	if (!worker->enabled) return;
	if (worker->inflight > 0) {
		// Request early cancellation; result is about to be discarded.
		worker->cancel_render = true;
		ppu_worker_plat_mb();
		ppu_worker_plat_wait_done(worker);
		worker->inflight = worker->inflight - 1;
		// Advance front_slot only when the frame completed; cancelled jobs have ready_back_idx == -1.
		if (worker->ready_back_idx >= 0)
			worker->front_idx = worker->ready_back_idx;
		// Clear stale done signals and reset cancel flag.
		while (ppu_worker_plat_trywait_done(worker)) {}
		worker->cancel_render = false;
	}
	worker->worker_busy = false;
}

// Configure output pixel format (2=RGB565, 3=BGR24, 4=XRGB8888).
// Called once at driver init (after ppu_worker_init). Drains in-flight work.
static inline void ppu_worker_set_output_bpp(ppu_worker_t* worker, INT32 bpp)
{
	if (!worker->enabled) return;
	if (bpp != 2 && bpp != 3 && bpp != 4) bpp = 4;
	ppu_worker_drain_for_mode_switch(worker);
	if (bpp == worker->out_bpp) return;
	// Lazy (re)allocate out_buf for 16/24bpp; free when switching to 32bpp.
	if (bpp == 4) {
		for (int slot_i = 0; slot_i < GBAPPU_WORKER_N_BUFFERS; ++slot_i) {
			if (worker->out_buf[slot_i]) { free(worker->out_buf[slot_i]); worker->out_buf[slot_i] = NULL; }
		}
		worker->out_bpp = 4;
		return;
	}
	// Two-phase commit: allocate all slots first, roll back on failure (realloc
	// keeps the old block, so the previous bpp and buffers stay valid).
	INT32 slot_bytes = GBA_LCD_W * GBA_LCD_H * bpp;
	UINT8* new_buf[GBAPPU_WORKER_N_BUFFERS];
	int slot_i;
	for (slot_i = 0; slot_i < GBAPPU_WORKER_N_BUFFERS; ++slot_i) {
		void* new_slot = realloc(worker->out_buf[slot_i], slot_bytes);
		if (!new_slot) break;
		new_buf[slot_i] = (UINT8*)new_slot;
		memset(new_buf[slot_i], 0, slot_bytes);
	}
	if (slot_i < GBAPPU_WORKER_N_BUFFERS) {
		// Rollback: realloc failed - free newly allocated slots.
		for (int roll_i = 0; roll_i < slot_i; ++roll_i) { free(new_buf[roll_i]); new_buf[roll_i] = NULL; }
		return; // keep old out_bpp and old buffers
	}
	// Phase 2: all allocations succeeded - commit.
	for (slot_i = 0; slot_i < GBAPPU_WORKER_N_BUFFERS; ++slot_i) {
		worker->out_buf[slot_i] = new_buf[slot_i];
	}
	worker->out_bpp = bpp;
}

static void ppu_worker_reset(ppu_worker_t* worker)
{
	if (!worker->enabled) return;
	ppu_worker_drain_for_mode_switch(worker);
	// Drain any extra stale signals the semaphore may carry.
	while (ppu_worker_plat_trywait_done(worker)) {}

	worker->exiting          = false;
	worker->worker_busy      = false;
	worker->have_first_frame = false;
	worker->inflight         = 0;
	worker->cancel_render    = false;
	worker->ready_back_idx   = -1;
	worker->front_idx        = -1;
	worker->read_snap_idx    = 0;
	worker->write_snap_idx   = 0;
	worker->oam_write_seq      = 0;
	worker->oam_write_base     = 0;
	worker->palette_write_seq  = 0;
	worker->palette_write_base = 0;
	memset(worker->vram_shadow,  0, sizeof(worker->vram_shadow));
	memset(worker->vram_scratch, 0, sizeof(worker->vram_scratch));

	memset(worker->backbuf,   0, sizeof(worker->backbuf));
	for (int slot_i = 0; slot_i < GBAPPU_WORKER_N_BUFFERS; ++slot_i)
		if (worker->out_buf[slot_i]) memset(worker->out_buf[slot_i], 0, GBA_LCD_W * GBA_LCD_H * worker->out_bpp);
	memset((void*)worker->snapshots, 0, sizeof(worker->snapshots));
}

// --- PPU state machine (main thread) --------------------------------------
// Parallel twin of the single-threaded handler; captures worker snapshots at
// line boundaries and fast-forwards idle cycles.
//
// Three event boundaries per line (cycle-exact):
//   0    : line start  - DISPCNT pipeline advance, affine reload, line-start snapshot
//   1006 : HBlank start- HBlank flag set, IRQ fires, DMA wakes, HBlank snapshot+render
// HBlank end (1232) coincides with line 0 of the next row and is handled there.

static inline void gba_ppu_event_worker(gba_t* gba, sb_emu_state_t* emu, UINT32 cycles_late)
{
	(void)emu;

	if (gba->ppu.scan_clock >= 280896)
		gba->ppu.scan_clock -= 280896;

	INT32 lcd_y      = (INT32)(gba->ppu.scan_clock / GBA_LCD_LINE_CYCLES);
	INT32 line_clock = (INT32)(gba->ppu.scan_clock % GBA_LCD_LINE_CYCLES);
	gba->ppu.scan_clock++;

	ppu_worker_render_ctx_t fwd_ctx = {0};
	fwd_ctx.ppu = &gba->ppu;
	INT32 fast_forward_ticks = ppu_worker_compute_max_fast_forward(&fwd_ctx) + 1;
	gba->ppu.scan_clock += fast_forward_ticks;

	bool at_line_start   = (line_clock == 0);
	bool at_hblank_start = (line_clock == GBA_LCD_HBLANK_START_CYCLE);

	bool vblank = (lcd_y >= GBA_LCD_H) && (lcd_y < 228);
	bool hblank = (line_clock >= GBA_LCD_HBLANK_START_CYCLE) && (line_clock < GBA_LCD_HBLANK_END_CYCLE);
	// VCount advances at HBlank exit (cycle 1232); events only fire at 0/1006, so VCount == lcd_y.
	INT32 vcount = lcd_y % 228;

	// DISPSTAT / VCount update at the two handled boundaries.
	if (at_line_start || at_hblank_start) {
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
				// HBlank snapshot and line render fire at cycle 1006 (HBlank entry)
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
		}
		// VCount match edge detection.
		bool vcount_match_cur = (vcount == (INT32)vcount_cmp);
		if (vcount_match_cur && !gba->ppu.last_vcount_match && vcount_irq_en)
			gba_send_interrupt(gba, 3, 1 << GBA_INT_LCD_VCOUNT);
		gba->ppu.last_vcount_match = vcount_match_cur;
	}

	// Affine PB/PD advance + BG2X/Y reload at line start (cycle 0).
	if (at_line_start && lcd_y < GBA_LCD_H) {
		UINT16 dispcnt = gba->ppu.dispcnt_pipeline[0];
		INT32  bg_mode = SB_BFE(dispcnt, 0, 3);

		if (bg_mode != 0 && lcd_y != 0) {
			for (INT32 aff = 0; aff < 2; ++aff) {
				if (!SB_BFE(dispcnt, 8 + aff + 2, 1)) continue;

				INT32  aff_pb = (INT16)gba_io_read16(gba, GBA_BG2PB + aff * 0x10);
				INT32  aff_pd = (INT16)gba_io_read16(gba, GBA_BG2PD + aff * 0x10);
				UINT16 bgcnt = gba_io_read16(gba, GBA_BG2CNT + aff * 2);
				bool mosaic = SB_BFE(bgcnt, 6, 1);

				if (mosaic) {
					UINT16 mosaic_reg = gba_io_read16(gba, GBA_MOSAIC);
					INT32  mosaic_y   = SB_BFE(mosaic_reg, 4, 4) + 1;
					if ((lcd_y % mosaic_y) == 0) {
						gba->ppu.aff[aff].render_bgx += aff_pb * mosaic_y;
						gba->ppu.aff[aff].render_bgy += aff_pd * mosaic_y;
					}
				} else {
					gba->ppu.aff[aff].render_bgx += aff_pb;
					gba->ppu.aff[aff].render_bgy += aff_pd;
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
	ppu_worker_t* worker = (ppu_worker_t*)arg;

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) \
 || defined(__NetBSD__) || defined(__DragonFly__)
	pthread_setname_np("FBNeo-GBA-PPU");
#endif

	for (;;) {
		ppu_worker_plat_wait_job(worker);
		if (worker->exiting) break;
		ppu_worker_process_job(worker);
	}
	return NULL;
}

static INT32 ppu_worker_init(ppu_worker_t* worker)
{
	memset(worker, 0, sizeof(*worker));
	worker->ready_back_idx = -1;
	worker->front_idx      = -1;
	// Default 32bpp; driver calls set_output_bpp(nBurnBpp) at init.
	worker->out_bpp        = 4;

	ppu_worker_evt_init(&worker->job_evt);
	ppu_worker_evt_init(&worker->done_evt);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, GBAPPU_WORKER_STACK_SIZE);
	int rc = pthread_create(&worker->worker, &attr, ppu_worker_thread, worker);
	pthread_attr_destroy(&attr);
	if (rc != 0) {
		ppu_worker_evt_destroy(&worker->job_evt);
		ppu_worker_evt_destroy(&worker->done_evt);
		return 1;
	}

#if defined(__linux__)
	pthread_setname_np(worker->worker, "FBNeo-GBA-PPU");
#endif

	worker->enabled = true;
	return 0;
}

static void ppu_worker_exit(ppu_worker_t* worker)
{
	if (!worker->enabled) return;
	worker->exiting = true;
	ppu_worker_plat_mb();
	ppu_worker_plat_release_job(worker);
	pthread_join(worker->worker, NULL);
	ppu_worker_evt_destroy(&worker->job_evt);
	ppu_worker_evt_destroy(&worker->done_evt);
	for (int slot_i = 0; slot_i < GBAPPU_WORKER_N_BUFFERS; ++slot_i) { if (worker->out_buf[slot_i]) { free(worker->out_buf[slot_i]); worker->out_buf[slot_i] = NULL; } }
	worker->enabled = false;
}

// --- Win32 back-end -------------------------------------------------------
#elif GBAPPU_WORKER_HAVE_WINTHREAD

static DWORD WINAPI ppu_worker_thread_win32(LPVOID arg)
{
	ppu_worker_t* worker = (ppu_worker_t*)arg;

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
		ppu_worker_plat_wait_job(worker);
		if (worker->exiting) break;
		ppu_worker_process_job(worker);
	}
	return 0;
}

static INT32 ppu_worker_init(ppu_worker_t* worker)
{
	memset(worker, 0, sizeof(*worker));
	worker->ready_back_idx = -1;
	worker->front_idx      = -1;
	// Default 32bpp; driver calls set_output_bpp(nBurnBpp) at init.
	worker->out_bpp        = 4;
	worker->worker  = NULL;
	worker->job_evt = worker->done_evt = NULL;

	worker->job_evt  = CreateSemaphoreA(NULL, 0, GBAPPU_WORKER_N_BUFFERS + 1, NULL);
	worker->done_evt = CreateSemaphoreA(NULL, 0, GBAPPU_WORKER_N_BUFFERS + 1, NULL);
	if (!worker->job_evt || !worker->done_evt) {
		if (worker->job_evt)  { CloseHandle(worker->job_evt);  worker->job_evt  = NULL; }
		if (worker->done_evt) { CloseHandle(worker->done_evt); worker->done_evt = NULL; }
		return 1;
	}

	DWORD tid = 0;
	worker->worker = CreateThread(NULL, GBAPPU_WORKER_STACK_SIZE,
	                          ppu_worker_thread_win32, worker, 0, &tid);
	if (!worker->worker) {
		CloseHandle(worker->job_evt);
		CloseHandle(worker->done_evt);
		worker->job_evt = worker->done_evt = NULL;
		return 1;
	}
	SetThreadPriority(worker->worker, THREAD_PRIORITY_NORMAL);
	worker->enabled = true;
	return 0;
}

static void ppu_worker_exit(ppu_worker_t* worker)
{
	if (!worker->enabled) return;
	worker->exiting = true;
	ppu_worker_plat_mb();
	ppu_worker_plat_release_job(worker);
	WaitForSingleObject(worker->worker, INFINITE);
	CloseHandle(worker->worker); worker->worker = NULL;
	if (worker->job_evt)  { CloseHandle(worker->job_evt);  worker->job_evt  = NULL; }
	if (worker->done_evt) { CloseHandle(worker->done_evt); worker->done_evt = NULL; }
	for (int slot_i = 0; slot_i < GBAPPU_WORKER_N_BUFFERS; ++slot_i) { if (worker->out_buf[slot_i]) { free(worker->out_buf[slot_i]); worker->out_buf[slot_i] = NULL; } }
	worker->enabled = false;
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
				#define EXP565(p) do { \
				UINT32 rr=(UINT32)(((p)>>11)&0x1F); UINT32 gg=(UINT32)(((p)>>5)&0x3F); UINT32 bb=(UINT32)((p)&0x1F); \
				*dst++ = ((rr<<3)|(rr>>2))<<16 | ((gg<<2)|(gg>>4))<<8 | ((bb<<3)|(bb>>2)); } while(0)
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

static inline size_t ppu_worker_save_state(ppu_worker_t* worker, UINT8* dst)
{
	// Complete the in-flight render before capturing, so the captured frame is
	// deterministic regardless of worker thread scheduling.
	ppu_worker_collect_inflight(worker);
	const INT32 front_slot = worker->front_idx;
	const INT32 idx   = (front_slot >= 0 && front_slot < GBAPPU_WORKER_N_BUFFERS) ? front_slot : -1;
	const size_t frame_bytes  = (size_t)GBA_LCD_W * GBA_LCD_H * worker->out_bpp;
	ppw_blob_hdr_t hdr;
	hdr.magic     = PPW_BLOB_MAGIC;
	hdr.version   = PPW_BLOB_VERSION;
	hdr.out_bpp   = worker->out_bpp;
	hdr.front_idx = idx;
	memcpy(dst, &hdr, sizeof(hdr));
	if (idx >= 0) {
		const UINT8* src = (worker->out_bpp == 4) ? (const UINT8*)worker->backbuf[idx]
		                                     : (const UINT8*)worker->out_buf[idx];
		if (src != NULL) memcpy(dst + PPW_BLOB_HEADER, src, frame_bytes);
		else             memset(dst + PPW_BLOB_HEADER, 0, frame_bytes);
	}
	// state_size reports the bpp=4 bound, so zero the unused tail at bpp<4.
	const size_t written = (size_t)PPW_BLOB_HEADER + (idx >= 0 ? frame_bytes : 0);
	if (written < (size_t)PPW_BLOB_MAX)
		memset(dst + written, 0, (size_t)PPW_BLOB_MAX - written);
	return written;
}

static inline bool ppu_worker_load_state(ppu_worker_t* worker, const UINT8* src, size_t len,
                                         UINT8** out_fb, bool* out_direct)
{
	if (!worker->enabled) return false;   // worker never started: nothing to restore
	ppw_blob_hdr_t hdr;
	if (len < (size_t)PPW_BLOB_HEADER) return false;
	memcpy(&hdr, src, sizeof(hdr));
	if (hdr.magic != PPW_BLOB_MAGIC || hdr.version != PPW_BLOB_VERSION) return false;
	if (hdr.out_bpp != 2 && hdr.out_bpp != 3 && hdr.out_bpp != 4) return false;
	if (hdr.front_idx < -1 || hdr.front_idx >= GBAPPU_WORKER_N_BUFFERS) return false;

	const INT32 idx = hdr.front_idx;
	// Payload length comes from the SAVED depth, so any depth can be loaded.
	const size_t src_frame_bytes = (size_t)GBA_LCD_W * GBA_LCD_H * hdr.out_bpp;
	if (len < (size_t)PPW_BLOB_HEADER + (idx >= 0 ? src_frame_bytes : 0)) return false;

	// Quiesce the worker and drain stale done signals before touching buffers.
	ppu_worker_drain_for_mode_switch(worker);
	while (ppu_worker_plat_trywait_done(worker)) {}

	// No frame was presented. Normalise the indices (runahead callers skip
	// ppu_worker_reset) and fail; the return also keeps out_fb off backbuf[-1].
	if (idx < 0) {
		worker->front_idx        = -1;
		worker->read_snap_idx            = 0;
		worker->write_snap_idx            = 0;
		worker->ready_back_idx   = -1;
		worker->have_first_frame = false;
		return false;
	}

	{
		// out_buf is malloc'd by set_output_bpp; a load before any vblank sees NULL.
		if (worker->out_bpp != 4 && worker->out_buf[idx] == NULL) {
			worker->out_buf[idx] = (UINT8*)malloc((size_t)GBA_LCD_W * GBA_LCD_H * worker->out_bpp);
			if (worker->out_buf[idx] == NULL) return false;
		}
		const UINT8* src_p = src + PPW_BLOB_HEADER;
		if (hdr.out_bpp == worker->out_bpp) {
			// Same depth: native layout, single bulk copy.
			if (worker->out_bpp == 4) memcpy(worker->backbuf[idx], src_p, src_frame_bytes);
			else                 memcpy(worker->out_buf[idx], src_p, src_frame_bytes);
		} else {
			// Cross-depth: decode to XRGB8888, then re-encode to native depth.
			UINT32 line_xrgb[GBA_LCD_W];
			if (worker->out_bpp == 4) {
				for (INT32 y = 0; y < GBA_LCD_H; ++y) {
					ppu_worker_decode_scanline_to_xrgb8888(line_xrgb, src_p, hdr.out_bpp);
					memcpy(worker->backbuf[idx] + y*(size_t)GBA_LCD_W, line_xrgb, sizeof(line_xrgb));
					src_p += GBA_LCD_W * hdr.out_bpp;
				}
			} else {
				UINT8* dst_p = worker->out_buf[idx];
				for (INT32 y = 0; y < GBA_LCD_H; ++y) {
					ppu_worker_decode_scanline_to_xrgb8888(line_xrgb, src_p, hdr.out_bpp);
					ppu_worker_write_scanline_out(dst_p, line_xrgb, worker->out_bpp);
					src_p += GBA_LCD_W * hdr.out_bpp;
					dst_p += GBA_LCD_W * worker->out_bpp;
				}
			}
		}
	}

	// Present the restored frame; advance the write slot past it. inflight and
	// worker_busy are already clear after the drain above.
	worker->front_idx        = idx;
	worker->read_snap_idx            = 0;
	worker->write_snap_idx            = (idx + 1) % GBAPPU_WORKER_N_BUFFERS;
	worker->ready_back_idx   = -1;

	// Reset the shared VRAM buffers and every slot's journal state.
	memset(worker->vram_shadow, 0, sizeof(worker->vram_shadow));
	memset(worker->vram_scratch, 0, sizeof(worker->vram_scratch));
	for (INT32 i = 0; i < GBAPPU_WORKER_N_BUFFERS; ++i) {
		worker->snapshots[i].vram_journal          = false;
		worker->snapshots[i].vram_journal_overflow = false;
		worker->snapshots[i].vram_pool_used        = 0;
		worker->snapshots[i].oam_per_line          = false;
		worker->snapshots[i].palette_per_line      = false;
		// No dirty line (lo >= hi), the same default used by ppu_worker_hblank_snapshot.
		for (INT32 line_y = 0; line_y < GBA_LCD_H; ++line_y) {
			worker->snapshots[i].vram_line_lo[line_y] = 1;
			worker->snapshots[i].vram_line_hi[line_y] = 0;
		}
	}

	worker->have_first_frame = true;

	// Hand the restored slot back to the caller; 32bpp is zero-copy.
	if (worker->out_bpp == 4) *out_fb = (UINT8*)worker->backbuf[idx];
	else                 *out_fb = worker->out_buf[idx];
	*out_direct = (worker->out_bpp == 4);
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
	UINT32* backbuf[1];        // unused in stub; pointer-sized to silence type checks
	UINT8*  out_buf[1];        // unused in stub; always NULL
} ppu_worker_t;

static inline INT32 ppu_worker_init(ppu_worker_t* worker)
{
	worker->enabled        = false;
	worker->worker_busy    = false;
	worker->out_bpp        = 4;
	worker->front_idx      = -1;
	worker->backbuf[0]     = NULL;
	worker->out_buf[0]     = NULL;
	return 1;
}
static inline void  ppu_worker_exit(ppu_worker_t* worker)                                  { (void)worker; }
static inline void  ppu_worker_reset(ppu_worker_t* worker)                                 { worker->front_idx = -1; worker->backbuf[0] = NULL; worker->out_buf[0] = NULL; }
static inline void  ppu_worker_set_output_bpp(ppu_worker_t* worker, INT32 bpp)             { worker->out_bpp = bpp; (void)bpp; }
static inline void  gba_ppu_worker_note_mem_write(gba_t* gba, UINT32 bank)   { (void)gba; (void)bank; }
static inline void  ppu_worker_hblank_snapshot(ppu_worker_t* worker, gba_t* gba, INT32 lcd_y)     { (void)worker; (void)gba; (void)lcd_y; }
static inline void  ppu_worker_line_start_snapshot(ppu_worker_t* worker, gba_t* gba, INT32 lcd_y) { (void)worker; (void)gba; (void)lcd_y; }
static inline void  ppu_worker_vblank_entry(ppu_worker_t* worker, gba_t* gba)                 { (void)worker; (void)gba; }
static inline void  ppu_worker_vblank_publish(ppu_worker_t* worker, gba_t* gba, gba_scratch_t* scratch, bool want_render)
                                                                                       { (void)worker; (void)gba; (void)scratch; (void)want_render; }
static inline void  ppu_worker_drain_for_mode_switch(ppu_worker_t* worker)                  { (void)worker; }
static inline bool  ppu_worker_enabled(const ppu_worker_t* worker)                          { (void)worker; return false; }

static inline void gba_ppu_event_worker(gba_t* gba, sb_emu_state_t* emu, UINT32 cycles_late)
{
	gba_ppu_event(gba, emu, cycles_late);
}

// Zero-filled payload: the size must still match the worker build, or FBNeo's
// ScanVar (length + name hash) would silently skip gba_machine across builds.
static inline size_t ppu_worker_state_size()                                                           { return PPW_BLOB_MAX; }
static inline size_t ppu_worker_save_state(ppu_worker_t* worker, UINT8* dst)                                { (void)worker; memset(dst, 0, PPW_BLOB_MAX); return PPW_BLOB_MAX; }
static inline bool   ppu_worker_load_state(ppu_worker_t* worker, const UINT8* src, size_t len,
                                           UINT8** fb, bool* direct)                                   { (void)worker; (void)src; (void)len; (void)fb; (void)direct; return false; }

#endif // GBAPPU_WORKER_ENABLED
