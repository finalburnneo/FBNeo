UINT8 k007121_ctrl_read(INT32 chip, UINT8 offset);
void k007121_ctrl_write(INT32 chip, UINT8 offset, UINT8 data);
void k007121_draw(INT32 chip, UINT16 *dest, UINT8 *gfx, UINT8 *ctable, INT32 base_color, INT32 xoffs, INT32 yoffs, INT32 bank_base, INT32 pri_mask, INT32 color_offset);
void k007121_reset();
void k007121_init(INT32 chip, INT32 sprite_mask, UINT8 *spriteram);
void k007121_buffer(INT32 chip);
INT32 k007121_scan(INT32 nAction);
