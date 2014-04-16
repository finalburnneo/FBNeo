
UINT8 tms34061_latch_read();
void tms34061_latch_write(UINT8 data);
UINT8 tms34061_read(INT32 col, INT32 row, INT32 func);
void tms34061_write(INT32 col, INT32 row, INT32 func, UINT8 data);

INT32 tms34061_display_blanked();
UINT8 *tms34061_get_vram_pointer();
extern INT32 tms34061_current_scanline;

void tms34061_interrupt();
void tms34061_init(UINT8 rowshift, UINT32 ram_size, void (*partial_update)(), void (*callback)(INT32 state));
void tms34061_reset();
void tms34061_exit();
INT32 tms34061_scan(INT32 nAction, INT32 *);
