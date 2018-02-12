#ifndef tms5220_h
#define tms5220_h

void tms5220_init();
void tms5220_exit();
void tms5220_reset();
void tms5220_scan(INT32 nAction, INT32 *pnMin);

void tms5220_write(UINT8 data);
UINT8 tms5220_status(); // read
UINT8 tms5220_ready();  // read
UINT8 tms5220_cycles(); // to ready
UINT8 tms5220_irq(); // read interrupt state

void tms5220_set_frequency(UINT32 freq);
void tms5220_update(INT16 *buffer, INT32 samples_len); // render samples

#endif
