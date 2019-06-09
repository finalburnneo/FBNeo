void s14001a_render(INT16 *buffer, INT32 length);
void s14001a_init(UINT8 *rom, INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ);
void s14001a_exit();
void s14001a_reset();
void s14001a_scan(INT32 nAction, INT32 *pnMin);
INT32 s14001a_bsy_read();
void s14001a_reg_write(INT32 data);
void s14001a_rst_write(INT32 data);
void s14001a_set_clock(INT32 clock);
void s14001a_set_volume(INT32 volume);

