#define MAX_VIEW2_CHIPS		2

void kaneko_view2_init(INT32 chip, UINT8 *video_ram, UINT8 *reg_ram, UINT8 *gfx_rom, INT32 color_offset, UINT8 *gfx_trans, INT32 global_x, INT32 global_y);
void kaneko_view2_exit();
void kaneko_view2_draw_layer(INT32 chip, INT32 layer, INT32 priority);
