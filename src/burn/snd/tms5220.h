#ifndef tms5220_h
#define tms5220_h

void tms5200_init();
void tms5220_init();
void tms5220c_init();
void tms5200_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ); // init for buffered.
void tms5220_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ); // init for buffered.
void tms5220c_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ); // ""
void tms5220_exit();
void tms5220_reset();
void tms5220_scan(INT32 nAction, INT32 *pnMin);
void tms5220_volume(double vol);

void tms5220_write(UINT8 data);
void tms5220_rsq_w(UINT8 state);
void tms5220_wsq_w(UINT8 state);

UINT8 tms5220_status(); // read
UINT8 tms5220_ready();  // read
UINT8 tms5220_cycles(); // to ready
UINT8 tms5220_irq(); // read interrupt state

void tms5220_set_frequency(UINT32 freq);
void tms5220_update(INT16 *buffer, INT32 samples_len); // render samples

#endif
