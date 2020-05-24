
void k1geInit(INT32 color, void (*vblank_pin_cb)(INT32,INT32), void (*hblank_pin_cb)(INT32,INT32));
void k1geReset();
void k1ge_hblank_on_timer_callback();
INT32 k1ge_scanline_timer_callback(INT32 scanline);
void k1ge_w(UINT32,UINT8);
UINT8 k1ge_r(UINT32);
INT32 k1geDraw();
void k1geExit();
void k1geScan(INT32 nAction, INT32 *);
