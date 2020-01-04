void sp0256_reset();
void sp0256_init(UINT8 *rom, INT32 clock);
void sp0256_set_drq_cb(void (*cb)(UINT8));
void sp0256_set_sby_cb(void (*cb)(UINT8));
void sp0256_set_clock(INT32 clock);
void sp0256_exit();
void sp0256_scan(INT32 nAction, INT32* pnMin);

void sp0256_ald_write(UINT8 data);
UINT8 sp0256_lrq_read();
UINT8 sp0256_sby_read();
UINT16 sp0256_spb640_read();
void sp0256_spb640_write(UINT16 offset, UINT16 data);
void sp0256_set_clock(INT32 clock);

void sp0256_update(INT16 *sndbuff, INT32 samples);
