extern INT32 i4x00_irq_enable;
extern INT32 i4x00_blitter_timer;
extern UINT8 DrvRecalc;

void i4x00_reset();
void i4x00_init(UINT32 address, UINT8 *gfx8, UINT8 *gfx4, UINT32 gfxlen, void (*irqcausewrite)(UINT16), UINT16 (*irqcauseread)(), void (*soundlatch)(UINT16), INT32 has_8bpp, INT32 has_16bpp);
void i4x00_set_offsets(INT32 layer0, INT32 layer1, INT32 layer2);
void i4x00_set_extrachip_callback(void (*callback)());
void i4x00_exit();
void i4x00_scan(INT32 nAction, INT32 *pnMin);

// raster/partial updates:
extern INT32 i4x00_raster_update; // see pst90s/d_hyprduel.cpp
void i4x00_draw_begin();
void i4x00_draw_scanline(INT32 drawto);
void i4x00_draw_end();

// regular updates:
INT32 i4x00_draw();

