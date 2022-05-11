/* emulation of Altera Cyclone EPIC12 FPGA programmed as a blitter */

void epic12_init(INT32 ram_size, UINT16 *ram, UINT8 *dippy);
void epic12_exit();
void epic12_reset();
void epic12_scan(INT32 nAction, INT32 *pnMin);
void epic12_set_blitterdelay(INT32 delay, INT32 burn_cycles);
void epic12_set_blitterthreading(INT32 value);
void epic12_wait_blitterthread();
void epic12_draw_screen(UINT8 &recalc_palette);
UINT32 epic12_blitter_read(UINT32 offset); // 0x18000000 - 0x18000057
void epic12_blitter_write(UINT32 offset, UINT32 data);

