void soundsgood_data_write(UINT16 data);
UINT8 soundsgood_status_read();
void soundsgood_reset_write(int state);

void soundsgood_reset();
void soundsgood_init(UINT8 *rom, UINT8 *ram); // rom = 0x40000, ram = 0x1000 in size
void soundsgood_exit();
void soundsgood_scan(INT32 nAction, INT32 *pnMin);
INT32 soundsgood_reset_status();
INT32 soundsgood_initialized();

extern INT32 soundsgood_rampage;
