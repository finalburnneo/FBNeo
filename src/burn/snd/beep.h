void beep_init(INT32 frequency);
void beep_exit();
void beep_reset();
void beep_scan(INT32 nAction, INT32 *pnMin);
void beep_update(INT16 *output, INT32 samples_len);
void beep_set_clock(UINT32 frequency);
void beep_set_state(bool on);
void beep_set_volume(double volume);
void beep_set_buffered(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz);

