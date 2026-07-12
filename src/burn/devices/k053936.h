// K053936 zoom/rotation (ROZ+) tilemap generator.
//
// A standalone device (independent of konamiic.cpp). The 16-bit indexed path
// draws to pTransDraw; the high-color (32-bit) paths (K053936Draw's non-indexed
// branch and the K053936GP_* konami-GX path) composite into a render target the
// driver supplies via K053936SetRenderTarget() -- typically the shared konami
// video-mixer buffer. Reset / exit / save-state are driven directly by the
// game driver.

#ifndef K053936_H
#define K053936_H

void K053936Init(INT32 chip, UINT8 *ram, INT32 len, INT32 w, INT32 h, void (*pCallback)(INT32 offset, UINT16 *ram, INT32 *code, INT32 *color, INT32 *sx, INT32 *sy, INT32 *fx, INT32 *fy));

void K053936Reset();
void K053936Exit();
void K053936Scan(INT32 nAction);

void K053936EnableWrap(INT32 chip, INT32 status);
void K053936SetOffset(INT32 chip, INT32 xoffs, INT32 yoffs);
void K053936PredrawTiles3(INT32 chip, UINT8 *gfx, INT32 tile_size_x, INT32 tile_size_y, INT32 transparent);
void K053936PredrawTiles2(INT32 chip, UINT8 *gfx);
void K053936PredrawTiles(INT32 chip, UINT8 *gfx, INT32 transparent, INT32 tcol /*transparent color*/);
void K053936Draw(INT32 chip, UINT16 *ctrl, UINT16 *linectrl, INT32 transp);

extern UINT16 *m_k053936_0_ctrl_16;
extern UINT16 *m_k053936_0_linectrl_16;
extern UINT16 *m_k053936_0_ctrl;
extern UINT16 *m_k053936_0_linectrl;
extern UINT16 *K053936_external_bitmap;

void K053936GP_set_colorbase(INT32 chip, INT32 color_base);
void K053936GP_enable(int chip, int enable);
void K053936GP_set_offset(int chip, int xoffs, int yoffs);
void K053936GP_clip_enable(int chip, int status);
void K053936GP_set_cliprect(int chip, int minx, int maxx, int miny, int maxy);
void K053936GP_0_zoom_draw(UINT16 *bitmap, int tilebpp, int blend, int alpha, int pixeldouble_output, UINT16* temp_m_k053936_0_ctrl_16, UINT16* temp_m_k053936_0_linectrl_16,UINT16* temp_m_k053936_0_ctrl, UINT16* temp_m_k053936_0_linectrl);
void K053936GpInit();
void K053936GPExit();

// High-color (32-bit) render target for the shared konami video mixer. The
// driver sets it before drawing when the IC composites into that buffer; the
// 16-bit indexed path ignores it. NULL (default) makes the 32-bit paths a no-op.
void K053936SetRenderTarget(UINT32 *bitmap32, UINT32 *palette32, UINT8 *priority_bitmap);

#endif
