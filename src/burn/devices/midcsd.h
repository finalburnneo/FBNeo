void csd_data_write(UINT16 data);
UINT8 csd_status_read();
void csd_reset_write(int state);

void csd_reset();
void csd_init(UINT8 *rom, UINT8 *ram);
void csd_exit();
void csd_scan(INT32 nAction, INT32 *pnMin);
INT32 csd_reset_status();
INT32 csd_initialized();
