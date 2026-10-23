
void cvsd_reset_write(INT32 state);
void cvsd_data_write(UINT16 data);
INT32 cvsd_reset_status();
void cvsd_cpu_reset();
void cvsd_portb_write(UINT8 data);
void cvsd_cb1_write(INT32 state);
void cvsd_set_cb2_callback(void (*cb)(INT32 state));
void cvsd_set_s11_bg_mode(INT32 enable);
void cvsd_set_dac_signed(INT32 enable);
UINT8 cvsd_talkback_read();
void cvsd_reset();
void cvsd_init(INT32 m6809num, INT32 dacnum, INT32 pianum, UINT8 *rom /*0x80000 size!*/, UINT8 *ram/*0x800 size!*/); // cpu is 2000000 hz
void cvsd_exit();
void cvsd_update(INT16 *samples, INT32 length);
INT32 cvsd_initialized();
void cvsd_scan(INT32 nAction, INT32 *pnMin);
