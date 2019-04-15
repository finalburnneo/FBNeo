void soundsgood_data_write(UINT16 data);
UINT8 soundsgood_status_read();
void soundsgood_reset_write(int state);

void soundsgood_reset();
// n68knum = which 68k chip to use
// dacnum = which dac chip to use
// rom = 0x40000, ram = 0x1000 in size
void soundsgood_init(INT32 n68knum, INT32 dacnum, UINT8 *rom, UINT8 *ram); 
void soundsgood_exit();
void soundsgood_scan(INT32 nAction, INT32 *pnMin);
INT32 soundsgood_reset_status();
INT32 soundsgood_initialized();

extern INT32 soundsgood_rampage;
