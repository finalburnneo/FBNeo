extern int bac06_depth;
extern int bac06_yadjust;

void bac06_draw_layer(UINT8 *vram, UINT16 control[2][4], UINT8 *rsram, UINT8 *csram, UINT8 *gfx8, INT32 col8, INT32 mask8, UINT8 *gfx16, INT32 col16, INT32 mask16, INT32 widetype, INT32 opaque);
