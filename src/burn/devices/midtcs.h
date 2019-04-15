void tcs_data_write(UINT16 data);
UINT8 tcs_status_read();
void tcs_reset_write(int state);

void tcs_reset();
void tcs_init(INT32 cpunum, INT32 pianum, INT32 dacnum, UINT8 *rom, UINT8 *ram); // rom 0x10000, ram 0x800 in size
void tcs_exit();
void tcs_scan(INT32 nAction, INT32 *pnMin);
INT32 tcs_reset_status();
INT32 tcs_initialized();
