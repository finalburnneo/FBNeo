// FBNeo GBA PPU worker thread (single-header)
// Frame-level pipelining: main thread runs CPU/PPU state machine (same as ST),
// pixel composition runs in worker thread. All data passed via snapshots.
// Back-ends: POSIX pthreads | Win32 threads | ST fallback (PPU_WORKER_DISABLE).

#pragma once

#include "gba.h"

// #define PPU_WORKER_DISABLE   // force single-threaded

#if !defined(PPU_WORKER_DISABLE)

// Windows (native threads + semaphores)
#  if defined(_WIN32) || defined(BUILD_WIN32)
#    ifndef WIN32_LEAN_AND_MEAN
#      define WIN32_LEAN_AND_MEAN
#    endif
#    ifndef NOMINMAX
#      define NOMINMAX
#    endif
#    include <windows.h>
#    define PPU_WORKER_HAVE_PTHREAD   0
#    define PPU_WORKER_HAVE_WINTHREAD 1

// POSIX (pthreads + named semaphores). Emscripten has no named semaphores.
#  elif defined(__unix__) && !defined(__EMSCRIPTEN__)
#    include <pthread.h>
#    include <semaphore.h>
#    include <fcntl.h>
#    include <errno.h>
#    include <stdio.h>
#    define PPU_WORKER_HAVE_PTHREAD   1
#    define PPU_WORKER_HAVE_WINTHREAD 0

// Other platforms: ST fallback
#  else
#    define PPU_WORKER_HAVE_PTHREAD   0
#    define PPU_WORKER_HAVE_WINTHREAD 0
#  endif

#else   // PPU_WORKER_DISABLE
#  define PPU_WORKER_HAVE_PTHREAD   0
#  define PPU_WORKER_HAVE_WINTHREAD 0
#endif

#if !defined(PPU_WORKER_HAVE_PTHREAD)
#  define PPU_WORKER_HAVE_PTHREAD   0
#endif
#if !defined(PPU_WORKER_HAVE_WINTHREAD)
#  define PPU_WORKER_HAVE_WINTHREAD 0
#endif

#define PPU_WORKER_ENABLED (PPU_WORKER_HAVE_PTHREAD || PPU_WORKER_HAVE_WINTHREAD)

#ifdef __cplusplus
extern "C" {
#endif

#if PPU_WORKER_ENABLED

#define PPU_WORKER_IO_SIZE     0x60   // MMIO window: DISPCNT..GREENSWP (96B)
#define PPU_WORKER_N_BUFFERS   3      // triple-buffered pipeline

#if PPU_WORKER_N_BUFFERS < 2 || PPU_WORKER_N_BUFFERS > 8
#error PPU_WORKER_N_BUFFERS must be between 2 and 8
#endif

// Per-scanline state snapshot
typedef struct {
	UINT8  io[PPU_WORKER_IO_SIZE];
	INT32  bgx[2];
	INT32  bgy[2];
	UINT16 dispcnt_pipeline[3];
} ppu_worker_line_state_t;

// Per-frame snapshot (1:1 with backbuf slot)
typedef struct {
	UINT8  vram[128 * 1024];
	UINT8  oam[1024];
	UINT8  palette[1024];
	ppu_worker_line_state_t line_state[GBA_LCD_H];
	bool   line_hb_valid[GBA_LCD_H];
	bool   line_ls_valid[GBA_LCD_H];
	bool   vram_copied;
	float  ghosting_strength;
	bool   render_per_pixel;
} ppu_worker_snapshot_t;

// Worker control block
typedef struct ppu_worker_s {
	bool enabled;
	bool exiting;

	ppu_worker_snapshot_t snapshots[PPU_WORKER_N_BUFFERS];
	UINT8  backbuf[PPU_WORKER_N_BUFFERS][GBA_LCD_W * GBA_LCD_H * 4];

	volatile int w_idx;          // main: write slot
	volatile int r_idx;          // worker: read/render slot
	volatile int ready_back_idx; // last completed backbuf
	volatile int front_idx;      // currently presented backbuf
	UINT32       frame_count;

	volatile bool want_render;
	volatile bool worker_busy;
	volatile bool job_pending;
	volatile bool have_first_frame;
	volatile bool fast_forward;

#if PPU_WORKER_HAVE_PTHREAD
	pthread_t      worker;
	sem_t*         job_sem;
	sem_t*         done_sem;
	char           job_sem_name[40];
	char           done_sem_name[40];
#elif PPU_WORKER_HAVE_WINTHREAD
	HANDLE         worker;
	HANDLE         job_sem;
	HANDLE         done_sem;
#endif
} ppu_worker_t;

extern ppu_worker_t* g_ppu_worker_current;

// Public API
static INT32  ppu_worker_init(ppu_worker_t* w);
static void   ppu_worker_exit(ppu_worker_t* w);
static void   ppu_worker_reset(ppu_worker_t* w);
static void   ppu_worker_begin_frame(ppu_worker_t* w, gba_t* gba, sb_emu_state_t* emu);
static void   ppu_worker_hblank_snapshot(ppu_worker_t* w, gba_t* gba, INT32 lcd_y);
static void   ppu_worker_line_start_snapshot(ppu_worker_t* w, gba_t* gba, INT32 lcd_y);
static void   ppu_worker_vblank_entry(ppu_worker_t* w, gba_t* gba);
static void   ppu_worker_vblank_publish(ppu_worker_t* w, gba_t* gba, gba_scratch_t* scratch);

static inline bool ppu_worker_enabled(const ppu_worker_t* w) {
	return w && w->enabled;
}

// Fill snapshot slot with VBlank defaults (seed for missed rows).
// Call ONLY when slot is not being read by the worker.
static void ppu_worker_reset_slot(ppu_worker_t* w, int idx, gba_t* gba)
{
	ppu_worker_snapshot_t* s = &w->snapshots[idx];
	memset(s->line_hb_valid, 0, sizeof(s->line_hb_valid));
	memset(s->line_ls_valid, 0, sizeof(s->line_ls_valid));
	s->vram_copied       = false;
	s->ghosting_strength = gba->ppu.ghosting_strength;
	s->render_per_pixel  = gba->ppu.render_per_pixel;

	const UINT8* io_base = gba->mem.io + (GBA_DISPCNT & 0xfff);
	ppu_worker_line_state_t seed;
	memcpy(seed.io, io_base, PPU_WORKER_IO_SIZE);
	for (int aff = 0; aff < 2; ++aff) {
		seed.bgx[aff] = gba->ppu.aff[aff].render_bgx;
		seed.bgy[aff] = gba->ppu.aff[aff].render_bgy;
	}
	for (int p = 0; p < 3; ++p)
		seed.dispcnt_pipeline[p] = gba->ppu.dispcnt_pipeline[p];

	INT32 y = 0;
	for (; y + 3 < GBA_LCD_H; y += 4) {
		s->line_state[y+0] = seed;
		s->line_state[y+1] = seed;
		s->line_state[y+2] = seed;
		s->line_state[y+3] = seed;
	}
	for (; y < GBA_LCD_H; ++y)
		s->line_state[y] = seed;
}

// Copy MMIO + affine + pipeline from a line state snapshot into gba.
static inline void ppu_worker_install_io(gba_t* gba, const ppu_worker_line_state_t* ls)
{
	memcpy(gba->mem.io + (GBA_DISPCNT & 0xfff), ls->io, PPU_WORKER_IO_SIZE);
	for (int aff = 0; aff < 2; ++aff) {
		gba->ppu.aff[aff].render_bgx = ls->bgx[aff];
		gba->ppu.aff[aff].render_bgy = ls->bgy[aff];
		gba->ppu.aff[aff].wrote_bgx  = false;
		gba->ppu.aff[aff].wrote_bgy  = false;
	}
	for (int p = 0; p < 3; ++p)
		gba->ppu.dispcnt_pipeline[p] = ls->dispcnt_pipeline[p];
}

static inline void ppu_worker_install_row(gba_t* gba, const ppu_worker_line_state_t* ls,
                                       int row_y, ppu_worker_line_state_t* installed_prev,
                                       int* prev_valid_row)
{
	(void)row_y;
	ppu_worker_install_io(gba, ls);
	*installed_prev = *ls;
	*prev_valid_row = row_y;
}

// Worker pixel composition. Reads only snap, writes only out_fb.
// w optional: when provided, rendering aborts early on w->exiting (fast exit).
static void ppu_worker_render_frame_into(ppu_worker_snapshot_t* snap, UINT32* out_fb,
                                     UINT32* prev_fb_for_ghosting, ppu_worker_t* w)
{
	gba_t gba;
	memset(&gba, 0, sizeof(gba));

	memcpy(gba.mem.vram,    snap->vram,    sizeof(snap->vram));
	memcpy(gba.mem.oam,     snap->oam,     sizeof(snap->oam));
	memcpy(gba.mem.palette, snap->palette, sizeof(snap->palette));
	gba.ppu.ghosting_strength = snap->ghosting_strength;
	gba.ppu.render_per_pixel  = snap->render_per_pixel;
	gba.framebuffer           = (UINT8*)out_fb;

	// Backdrop + ghosting setup
	const INT32 fb_bytes = GBA_LCD_W * GBA_LCD_H * 4;
	if (snap->ghosting_strength > 0.0f && prev_fb_for_ghosting)
		memcpy(out_fb, prev_fb_for_ghosting, fb_bytes);
	else
		memset(out_fb, 0, fb_bytes);

	UINT16 bg_pal0 = *(const UINT16*)(gba.mem.palette + GBA_BG_PALETTE);
	UINT32 backdrop = ((UINT32)bg_pal0) | (5u << 17);

	// Fill both target buffers with backdrop color
	for (INT32 x = 0; x < GBA_LCD_W; ++x) {
		gba.first_target_buffer[x]  = backdrop;
		gba.second_target_buffer[x] = backdrop;
	}
	memset(gba.window, 0x3F, sizeof(gba.window));

	const bool render_per_pixel = snap->render_per_pixel;

	ppu_worker_line_state_t installed_prev;
	int prev_valid_row = -1;

	ppu_worker_install_row(&gba, &snap->line_state[0], 0,
	                    &installed_prev, &prev_valid_row);
	gba.ppu.mosaic_y_counter = 0;

	if (render_per_pixel)
		gba_ppu_render_objs(&gba, 0);

	for (INT32 lcd_y = 0; lcd_y < GBA_LCD_H; ++lcd_y) {
		const ppu_worker_line_state_t* ls = &snap->line_state[lcd_y];
		bool row_ok = snap->line_hb_valid[lcd_y] && snap->line_ls_valid[lcd_y];

		// Fast exit check (every 8 lines)
		if (w && (lcd_y & 7) == 0 && w->exiting) return;

		if (lcd_y > 0 || render_per_pixel) {
			const ppu_worker_line_state_t* ls_use = ls;
			if (!row_ok)
				ls_use = (prev_valid_row >= 0) ? &installed_prev : &snap->line_state[0];
			ppu_worker_install_row(&gba, ls_use, lcd_y,
			                    &installed_prev, &prev_valid_row);
		}

		if (render_per_pixel) {
			for (INT32 lcd_x = 0; lcd_x < GBA_LCD_W; ++lcd_x)
				gba_ppu_render_pixel(&gba, lcd_x, lcd_y);
			// Preload OBJs for next line (uses current IO = line y HBlank state)
			if (lcd_y < GBA_LCD_H - 1)
				gba_ppu_render_objs(&gba, lcd_y + 1);
		} else {
			gba_ppu_render_objs(&gba, lcd_y);
			gba_ppu_render_scanline(&gba, lcd_y);
		}
	}
}

// Set per-frame flags. Do NOT call reset_slot here — FF frames must not touch
// snapshot memory (worker might still be reading it). reset_slot is only
// called when w_idx advances (publish new job), on a guaranteed-free slot.
static void ppu_worker_begin_frame(ppu_worker_t* w, gba_t* gba, sb_emu_state_t* emu)
{
	(void)gba;
	if (!w || !w->enabled) return;
	bool want = (emu && emu->render_frame);
	w->fast_forward = !want;
	w->want_render  = want;
}

static void ppu_worker_hblank_snapshot(ppu_worker_t* w, gba_t* gba, INT32 lcd_y)
{
	if (!w || !w->enabled || !w->want_render) return;
	if (lcd_y < 0 || lcd_y >= GBA_LCD_H) return;

	ppu_worker_snapshot_t* snap = &w->snapshots[w->w_idx];
	memcpy(snap->line_state[lcd_y].io,
	       gba->mem.io + (GBA_DISPCNT & 0xfff), PPU_WORKER_IO_SIZE);
	snap->line_hb_valid[lcd_y] = true;
}

static void ppu_worker_line_start_snapshot(ppu_worker_t* w, gba_t* gba, INT32 lcd_y)
{
	if (!w || !w->enabled || !w->want_render) return;
	if (lcd_y < 0 || lcd_y >= GBA_LCD_H) return;

	ppu_worker_snapshot_t* snap = &w->snapshots[w->w_idx];
	ppu_worker_line_state_t* ls = &snap->line_state[lcd_y];
	for (int aff = 0; aff < 2; ++aff) {
		ls->bgx[aff] = gba->ppu.aff[aff].render_bgx;
		ls->bgy[aff] = gba->ppu.aff[aff].render_bgy;
	}
	for (int p = 0; p < 3; ++p)
		ls->dispcnt_pipeline[p] = gba->ppu.dispcnt_pipeline[p];
	snap->line_ls_valid[lcd_y] = true;
}

// VBlank entry: bulk copy VRAM/OAM/palette for this frame's snapshot.
static void ppu_worker_vblank_entry(ppu_worker_t* w, gba_t* gba)
{
	if (!w || !w->enabled || !w->want_render) return;

	ppu_worker_snapshot_t* snap = &w->snapshots[w->w_idx];
	if (snap->vram_copied) return;

	memcpy(snap->vram,    gba->mem.vram,    sizeof(snap->vram));
	memcpy(snap->oam,     gba->mem.oam,     sizeof(snap->oam));
	memcpy(snap->palette, gba->mem.palette, sizeof(snap->palette));
	snap->ghosting_strength = gba->ppu.ghosting_strength;
	snap->render_per_pixel  = gba->ppu.render_per_pixel;
	snap->vram_copied       = true;
}

// PPU state machine + snapshot capture (bit-exact with ST).
// Pixel composition runs in the worker thread.
static inline void gba_ppu_event_worker(gba_t* gba, sb_emu_state_t* emu, UINT32 cycles_late)
{
	bool render = emu ? emu->render_frame : true;

	if (gba->ppu.scan_clock >= 280896)
		gba->ppu.scan_clock -= 280896;

	INT32 lcd_y = (INT32)(gba->ppu.scan_clock / 1232);
	INT32 lcd_x = (INT32)((gba->ppu.scan_clock % 1232) / 4);
	gba->ppu.scan_clock++;

	INT32 fast_forward_ticks =
		gba_ppu_compute_max_fast_forward(gba, render && gba->ppu.render_per_pixel) + 1;

	gba->ppu.scan_clock += fast_forward_ticks;

	const bool at_column_edge =
		(lcd_x == 0) ||
		(lcd_x == GBA_LCD_HBLANK_START) ||
		(lcd_x == GBA_LCD_HBLANK_END);

	bool hblank = (lcd_x >= GBA_LCD_HBLANK_START) && (lcd_x < GBA_LCD_HBLANK_END);
	bool vblank = (lcd_y >= GBA_LCD_H) && (lcd_y < 227);
	INT32 vcount = (lcd_y + (lcd_x >= GBA_LCD_HBLANK_END)) % 228;

	UINT32 new_if = 0;
	if (at_column_edge) {
		UINT32 vcount_cmp = gba_io_read16(gba, GBA_DISPSTAT) >> 8;
		UINT16 disp_stat  = (UINT16)(gba_io_read16(gba, GBA_DISPSTAT) & ~0b111);
		if (vblank)                      disp_stat |= 1;
		if (hblank)                      disp_stat |= 2;
		if (vcount == (INT32)vcount_cmp) disp_stat |= 4;
		gba_io_store16(gba, GBA_DISPSTAT, disp_stat);
		gba_io_store16(gba, GBA_VCOUNT,  (UINT16)vcount);

		bool hblank_irq_en = SB_BFE(gba_io_read16(gba, GBA_DISPSTAT), 4, 1);
		if (hblank != gba->ppu.last_hblank) {
			gba->ppu.last_hblank = hblank;
			if (hblank) {
				if (hblank_irq_en)
					new_if |= (1 << GBA_INT_LCD_HBLANK);
				++gba->ppu.hblank_seq;
				gba_timing_schedule(gba, &gba->dma_event, 2);
				if (g_ppu_worker_current && g_ppu_worker_current->enabled
				    && lcd_y < GBA_LCD_H)
					ppu_worker_hblank_snapshot(g_ppu_worker_current, gba, lcd_y);
			}
			if (!hblank) {
				gba->ppu.dispcnt_pipeline[0] = gba->ppu.dispcnt_pipeline[1];
				gba->ppu.dispcnt_pipeline[1] = gba->ppu.dispcnt_pipeline[2];
				gba->ppu.dispcnt_pipeline[2] = gba_io_read16(gba, GBA_DISPCNT);
			}
		}

		if (vblank != gba->ppu.last_vblank) {
			gba->ppu.last_vblank = vblank;
			bool vblank_irq_en   = SB_BFE(gba_io_read16(gba, GBA_DISPSTAT), 3, 1);
			if (vblank) {
				if (vblank_irq_en)
					new_if |= (1 << GBA_INT_LCD_VBLANK);
				++gba->ppu.vblank_seq;
				gba_timing_schedule(gba, &gba->dma_event, 2);
				gba->frame_in_progress = false;
				if (g_ppu_worker_current && g_ppu_worker_current->enabled)
					ppu_worker_vblank_entry(g_ppu_worker_current, gba);
			}
		}

		bool vcount_irq_en = SB_BFE(gba_io_read16(gba, GBA_DISPSTAT), 5, 1);
		UINT32 vcount_cmp2 = gba_io_read16(gba, GBA_DISPSTAT) >> 8;
		if (vcount == (INT32)vcount_cmp2 && vcount_irq_en)
			new_if |= (1 << GBA_INT_LCD_VCOUNT);
	}

	if (new_if)
		gba_send_interrupt(gba, 3, new_if);

	// Affine increment/reload at lcd_x == 0 — runs on ALL frames (render + FF)
	if (lcd_x == 0 && lcd_y < GBA_LCD_H) {
		UINT16 dispcnt = gba->ppu.dispcnt_pipeline[0];
		INT32  bg_mode = SB_BFE(dispcnt, 0, 3);

		if (bg_mode != 0 && lcd_y != 0) {
			for (INT32 aff = 0; aff < 2; ++aff) {
				bool bg_en = SB_BFE(dispcnt, 8 + aff + 2, 1);
				if (!bg_en) continue;

				INT32  pb = (INT16)gba_io_read16(gba, GBA_BG2PB + aff * 0x10);
				INT32  pd = (INT16)gba_io_read16(gba, GBA_BG2PD + aff * 0x10);
				UINT16 bgcnt = gba_io_read16(gba, GBA_BG2CNT + aff * 2);
				bool mosaic = SB_BFE(bgcnt, 6, 1);

				if (gba->ppu.aff[aff].wrote_bgx) {
					gba->ppu.aff[aff].wrote_bgx = false;
				} else if (mosaic) {
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
				if (gba->ppu.aff[aff].wrote_bgy)
					gba->ppu.aff[aff].wrote_bgy = false;
			}
		}

		for (INT32 aff = 0; aff < 2; ++aff) {
			if (gba->ppu.aff[aff].wrote_bgx || lcd_y == 0) {
				gba->ppu.aff[aff].render_bgx = gba_io_read32(gba, GBA_BG2X + aff * 0x10);
				gba->ppu.aff[aff].render_bgx = SB_BFE(gba->ppu.aff[aff].render_bgx, 0, 28);
				gba->ppu.aff[aff].render_bgx = ((INT32)(gba->ppu.aff[aff].render_bgx << 4)) >> 4;
				gba->ppu.aff[aff].wrote_bgx  = false;
			}
			if (gba->ppu.aff[aff].wrote_bgy || lcd_y == 0) {
				gba->ppu.aff[aff].render_bgy = gba_io_read32(gba, GBA_BG2Y + aff * 0x10);
				gba->ppu.aff[aff].render_bgy = SB_BFE(gba->ppu.aff[aff].render_bgy, 0, 28);
				gba->ppu.aff[aff].render_bgy = ((INT32)(gba->ppu.aff[aff].render_bgy << 4)) >> 4;
				gba->ppu.aff[aff].wrote_bgy  = false;
			}
		}

		if (g_ppu_worker_current && g_ppu_worker_current->enabled && lcd_y < GBA_LCD_H)
			ppu_worker_line_start_snapshot(g_ppu_worker_current, gba, lcd_y);
	}

	gba_timing_deschedule(gba, &gba->ppu_event);
	gba_timing_schedule(gba, &gba->ppu_event,
	                    fast_forward_ticks + 1 - (INT32)cycles_late);
}

// ---- Platform primitives --------------------------------------------------
#if PPU_WORKER_HAVE_PTHREAD

static inline bool ppu_worker_plat_trywait_done(ppu_worker_t* w) {
	int rv;
	do { rv = sem_trywait(w->done_sem); } while (rv == -1 && errno == EINTR);
	return (rv == 0);
}

static inline void ppu_worker_plat_wait_done(ppu_worker_t* w) {
	for (;;) { int rv = sem_wait(w->done_sem); if (rv == 0 || errno != EINTR) break; }
}

static inline void ppu_worker_plat_release_job(ppu_worker_t* w) {
	sem_post(w->job_sem);
}

static inline void ppu_worker_plat_mb(void) {
	__sync_synchronize();
}

#elif PPU_WORKER_HAVE_WINTHREAD

static inline bool ppu_worker_plat_trywait_done(ppu_worker_t* w) {
	return (WaitForSingleObject(w->done_sem, 0) == WAIT_OBJECT_0);
}

static inline void ppu_worker_plat_wait_done(ppu_worker_t* w) {
	WaitForSingleObject(w->done_sem, INFINITE);
}

static inline void ppu_worker_plat_release_job(ppu_worker_t* w) {
	ReleaseSemaphore(w->job_sem, 1, NULL);
}

static inline void ppu_worker_plat_mb(void) {
	MemoryBarrier();
}

#endif

// VBlank publish: pipeline handoff.
//   1. Sync with worker (block in normal play; non-blocking in FF).
//   2. Promote completed backbuf to front; copy to presentation buffer.
//   3. If rendering & worker free: advance w_idx, reset_slot, post job.
static void ppu_worker_vblank_publish(ppu_worker_t* w, gba_t* gba,
                                      gba_scratch_t* scratch)
{
	if (!w || !w->enabled) {
		gba->framebuffer = scratch->framebuffer;
		return;
	}

	// Step 1: sync with worker
	if (w->worker_busy) {
		bool finished;
		if (w->fast_forward)
			finished = ppu_worker_plat_trywait_done(w);
		else {
			ppu_worker_plat_wait_done(w);
			finished = true;
		}
		if (finished) {
			w->worker_busy = false;
			w->job_pending  = false;
			w->front_idx    = w->ready_back_idx;
		}
	} else if (w->fast_forward) {
		// Drain stale done tokens
		while (ppu_worker_plat_trywait_done(w)) {}
	}

	// Step 2: present front buffer
	if (w->have_first_frame) {
		memcpy(scratch->framebuffer, w->backbuf[w->front_idx],
		       sizeof(scratch->framebuffer));
	} else {
		memset(scratch->framebuffer, 0, sizeof(scratch->framebuffer));
	}
	gba->framebuffer = scratch->framebuffer;

	// Step 3: post new job (rendering + worker free)
	if (w->want_render && !w->worker_busy) {
		int wi = w->w_idx;
		ppu_worker_snapshot_t* snap = &w->snapshots[wi];
		if (snap->vram_copied) {
			w->r_idx = wi;
			w->w_idx = (wi + 1) % PPU_WORKER_N_BUFFERS;
			// Seed the new write slot (worker not reading it)
			ppu_worker_reset_slot(w, w->w_idx, gba);
			w->job_pending      = true;
			w->worker_busy      = true;
			w->have_first_frame = true;
			ppu_worker_plat_mb();
			ppu_worker_plat_release_job(w);
			w->frame_count++;
		} else {
			// vblank_entry didn't run — re-seed same slot for next frame
			ppu_worker_reset_slot(w, wi, gba);
		}
	}
	w->want_render = false;
}

// ---- POSIX back-end -------------------------------------------------------
#if PPU_WORKER_HAVE_PTHREAD

static void* ppu_worker_thread(void* arg)
{
	ppu_worker_t* w = (ppu_worker_t*)arg;
	for (;;) {
		for (;;) { int rv = sem_wait(w->job_sem); if (rv == 0 || errno != EINTR) break; }
		if (w->exiting) break;
		if (!w->job_pending) continue;

		int ridx = w->r_idx;
		int front = w->front_idx;
		UINT32* ghost_src = (front >= 0 && w->have_first_frame)
			? (UINT32*)w->backbuf[front] : NULL;

		ppu_worker_render_frame_into(&w->snapshots[ridx],
		                         (UINT32*)w->backbuf[ridx], ghost_src, w);

		ppu_worker_plat_mb();
		w->ready_back_idx = ridx;
		sem_post(w->done_sem);
	}
	return NULL;
}

static INT32 ppu_worker_init(ppu_worker_t* w)
{
	memset(w, 0, sizeof(*w));
	w->ready_back_idx = -1;
	w->job_sem = w->done_sem = NULL;

	snprintf(w->job_sem_name,  sizeof(w->job_sem_name),
	         "/fbngbaj_%p", (void*)w);
	snprintf(w->done_sem_name, sizeof(w->done_sem_name),
	         "/fbngbad_%p", (void*)w);
	sem_unlink(w->job_sem_name);
	sem_unlink(w->done_sem_name);
	w->job_sem  = sem_open(w->job_sem_name,  O_CREAT, 0644, 0);
	w->done_sem = sem_open(w->done_sem_name, O_CREAT, 0644, 0);
	sem_unlink(w->job_sem_name);
	sem_unlink(w->done_sem_name);
	if (w->job_sem  == SEM_FAILED) w->job_sem  = NULL;
	if (w->done_sem == SEM_FAILED) w->done_sem = NULL;
	if (!w->job_sem || !w->done_sem) {
		if (w->job_sem)  { sem_close(w->job_sem);  w->job_sem  = NULL; }
		if (w->done_sem) { sem_close(w->done_sem); w->done_sem = NULL; }
		return 1;
	}

	memset(w->backbuf,   0, sizeof(w->backbuf));
	memset(w->snapshots, 0, sizeof(w->snapshots));

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024);
	if (pthread_create(&w->worker, &attr, ppu_worker_thread, w) != 0) {
		pthread_attr_destroy(&attr);
		sem_close(w->job_sem);
		sem_close(w->done_sem);
		w->job_sem = w->done_sem = NULL;
		return 1;
	}
	pthread_attr_destroy(&attr);
#if defined(__linux__) && defined(__GLIBC__)
	pthread_setname_np(w->worker, "FBNeo-GBA-PPU");
#endif
	w->enabled = true;
	return 0;
}

static void ppu_worker_exit(ppu_worker_t* w)
{
	if (!w || !w->enabled) return;

	w->exiting = true;
	w->job_pending = false;
	ppu_worker_plat_mb();
	ppu_worker_plat_release_job(w);
	pthread_join(w->worker, NULL);

	if (w->job_sem)  { sem_close(w->job_sem);  w->job_sem  = NULL; }
	if (w->done_sem) { sem_close(w->done_sem); w->done_sem = NULL; }
	w->enabled = false;
}

static void ppu_worker_reset(ppu_worker_t* w)
{
	if (!w || !w->enabled) return;

	// Wait for in-flight job, then drain stale tokens
	if (w->worker_busy || w->job_pending) {
		while (w->worker_busy) {
			ppu_worker_plat_wait_done(w);
			w->worker_busy = false;
			w->job_pending = false;
		}
		while (ppu_worker_plat_trywait_done(w)) {}
	}

	memset(w->backbuf,   0, sizeof(w->backbuf));
	memset(w->snapshots, 0, sizeof(w->snapshots));
	w->ready_back_idx  = -1;
	w->w_idx = w->r_idx = w->front_idx = 0;
	w->frame_count     = 0;
	w->have_first_frame = false;
	w->fast_forward    = false;
	w->want_render     = false;
}

// ---- Win32 back-end -------------------------------------------------------
#elif PPU_WORKER_HAVE_WINTHREAD

static DWORD WINAPI ppu_worker_thread_win32(LPVOID arg)
{
	ppu_worker_t* w = (ppu_worker_t*)arg;
	for (;;) {
		WaitForSingleObject(w->job_sem, INFINITE);
		if (w->exiting) break;
		if (!w->job_pending) continue;

		int ridx = w->r_idx;
		int front = w->front_idx;
		UINT32* ghost_src = (front >= 0 && w->have_first_frame)
			? (UINT32*)w->backbuf[front] : NULL;

		ppu_worker_render_frame_into(&w->snapshots[ridx],
		                         (UINT32*)w->backbuf[ridx], ghost_src, w);

		ppu_worker_plat_mb();
		w->ready_back_idx = ridx;
		ReleaseSemaphore(w->done_sem, 1, NULL);
	}
	return 0;
}

#if defined(_MSC_VER)
static void ppu_worker_set_thread_name_win32(DWORD tid, const char* name)
{
	typedef struct { DWORD dwType; LPCSTR szName; DWORD dwThreadID; DWORD dwFlags; } TNI;
	TNI info;
	info.dwType     = 0x1000;
	info.szName     = name;
	info.dwThreadID = tid;
	info.dwFlags    = 0;
	__try {
		RaiseException(0x406D1388, 0,
		               sizeof(info)/sizeof(ULONG_PTR), (const ULONG_PTR*)&info);
	} __except(EXCEPTION_EXECUTE_HANDLER) {}
}
#endif

static INT32 ppu_worker_init(ppu_worker_t* w)
{
	memset(w, 0, sizeof(*w));
	w->ready_back_idx = -1;
	w->worker  = NULL;
	w->job_sem = w->done_sem = NULL;

	w->job_sem  = CreateSemaphoreA(NULL, 0, PPU_WORKER_N_BUFFERS + 1, NULL);
	w->done_sem = CreateSemaphoreA(NULL, 0, PPU_WORKER_N_BUFFERS + 1, NULL);
	if (!w->job_sem || !w->done_sem) {
		if (w->job_sem)  { CloseHandle(w->job_sem);  w->job_sem  = NULL; }
		if (w->done_sem) { CloseHandle(w->done_sem); w->done_sem = NULL; }
		return 1;
	}

	memset(w->backbuf,   0, sizeof(w->backbuf));
	memset(w->snapshots, 0, sizeof(w->snapshots));

	DWORD tid = 0;
	w->worker = CreateThread(NULL, 1024 * 1024,
	                          ppu_worker_thread_win32, w, 0, &tid);
	if (!w->worker) {
		CloseHandle(w->job_sem);
		CloseHandle(w->done_sem);
		w->job_sem = w->done_sem = NULL;
		return 1;
	}
	SetThreadPriority(w->worker, THREAD_PRIORITY_NORMAL);
#if defined(_MSC_VER)
	ppu_worker_set_thread_name_win32(tid, "FBNeo-GBA-PPU");
#endif
	w->enabled = true;
	return 0;
}

static void ppu_worker_exit(ppu_worker_t* w)
{
	if (!w || !w->enabled) return;

	w->exiting = true;
	w->job_pending = false;
	ppu_worker_plat_mb();
	ppu_worker_plat_release_job(w);
	WaitForSingleObject(w->worker, INFINITE);

	CloseHandle(w->worker); w->worker = NULL;
	if (w->job_sem)  { CloseHandle(w->job_sem);  w->job_sem  = NULL; }
	if (w->done_sem) { CloseHandle(w->done_sem); w->done_sem = NULL; }
	w->enabled = false;
}

static void ppu_worker_reset(ppu_worker_t* w)
{
	if (!w || !w->enabled) return;

	if (w->worker_busy || w->job_pending) {
		while (w->worker_busy) {
			ppu_worker_plat_wait_done(w);
			w->worker_busy = false;
			w->job_pending = false;
		}
		while (ppu_worker_plat_trywait_done(w)) {}
	}

	memset(w->backbuf,   0, sizeof(w->backbuf));
	memset(w->snapshots, 0, sizeof(w->snapshots));
	w->ready_back_idx  = -1;
	w->w_idx = w->r_idx = w->front_idx = 0;
	w->frame_count     = 0;
	w->have_first_frame = false;
	w->fast_forward    = false;
	w->want_render     = false;
}

#endif // back-end selection

// ---- Single-threaded fallback stubs ---------------------------------------
#else // !PPU_WORKER_ENABLED

typedef struct {
	bool enabled;
	int  dummy;
} ppu_worker_t;

extern ppu_worker_t* g_ppu_worker_current;

static inline INT32 ppu_worker_init(ppu_worker_t* w)                                 { (void)w; return 1; }
static inline void  ppu_worker_exit(ppu_worker_t* w)                                 { (void)w; }
static inline void  ppu_worker_reset(ppu_worker_t* w)                                { (void)w; }
static inline void  ppu_worker_begin_frame(ppu_worker_t* w, gba_t* g, sb_emu_state_t* e)
                                                                              { (void)w; (void)g; (void)e; }
static inline void  ppu_worker_hblank_snapshot(ppu_worker_t* w, gba_t* g, INT32 y)
                                                                              { (void)w; (void)g; (void)y; }
static inline void  ppu_worker_line_start_snapshot(ppu_worker_t* w, gba_t* g, INT32 y)
                                                                              { (void)w; (void)g; (void)y; }
static inline void  ppu_worker_vblank_entry(ppu_worker_t* w, gba_t* g)           { (void)w; (void)g; }
static inline void  ppu_worker_vblank_publish(ppu_worker_t* w, gba_t* g, gba_scratch_t* s)
                                                                              { (void)w; (void)g; (void)s; }
static inline bool  ppu_worker_enabled(const ppu_worker_t* w)                        { (void)w; return false; }

static inline void gba_ppu_event_worker(gba_t* gba, sb_emu_state_t* emu, UINT32 cycles_late)
{
	gba_ppu_event(gba, emu, cycles_late);
}

#endif // PPU_WORKER_ENABLED

#ifdef __cplusplus
}
#endif
