// K051316 zoom/rotation tilemap generator.
//
// A standalone device (independent of konamiic.cpp). The index-color path draws
// to pTransDraw; the high-color (32-bit) path composites into a render target
// the driver supplies via K051316SetRenderTarget() -- typically the shared
// konami video-mixer buffer for konami games. Save-state / reset / exit are
// driven directly by the game driver.

#ifndef K051316_H
#define K051316_H

void K051316Init(INT32 chip, UINT8 *gfx, UINT8 *gfxexp, INT32 mask, void (*callback)(INT32 *code,INT32 *color,INT32 *flags), INT32 bpp, INT32 transp);
void K051316Reset();
void K051316Exit();

#define K051316_16BIT	(1<<8)
#define K051316_OPAQUE  (2<<8)

void K051316RedrawTiles(INT32 chip);

UINT8 K051316ReadRom(INT32 chip, INT32 offset);
UINT8 K051316Read(INT32 chip, INT32 offset);
void K051316Write(INT32 chip, INT32 offset, INT32 data);

void K051316WriteCtrl(INT32 chip, INT32 offset, INT32 data);
UINT8 K051316ReadCtrl(INT32 chip, INT32 offset);
void K051316WrapEnable(INT32 chip, INT32 status);
void K051316SetOffset(INT32 chip, INT32 xoffs, INT32 yoffs);
void K051316_zoom_draw(INT32 chip, INT32 flags);
void K051316Scan(INT32 nAction);

// High-color (32-bit) render target for the shared konami video mixer. The
// driver sets it before drawing when the IC composites into that buffer; the
// index-color path ignores it. NULL (default) makes the 32-bit path a no-op.
void K051316SetRenderTarget(UINT32 *bitmap32, UINT32 *palette32, UINT8 *priority_bitmap);

#endif
