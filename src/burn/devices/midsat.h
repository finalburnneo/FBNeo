// Midway Squeak and Talk module

void midsat_write(UINT8 data);

void midsat_reset_write(INT32 state);
INT32 midsat_reset_status();

void midsat_reset();
void midsat_init(UINT8 *rom);
void midsat_exit();

INT32 has_midsat();

void midsatNewFrame();
INT32 midsatRun(INT32 cycles);
void midsat_update(INT16 *samples, INT32 length);
void midsat_scan(INT32 nAction, INT32 *pnMin);
