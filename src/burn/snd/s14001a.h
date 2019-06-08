void s14001a_render(INT16 *buffer, INT32 length);
void s14001a_init(UINT8 *rom, INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ);
void s14001a_exit();
int s14001a_bsy_read();
void s14001a_reg_write(int data);
void s14001a_rst_write(int data);
void s14001a_set_clock(int clock);
void s14001a_set_volume(int volume);

