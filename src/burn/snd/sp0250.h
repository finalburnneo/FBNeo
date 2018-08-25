// sp0250 speech core

void sp0250_init(INT32 clock, void (*drqCB)(INT32), INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ);
void sp0250_update(INT16 *inputs, INT32 sample_len);
void sp0250_exit();
void sp0250_reset();
void sp0250_tick(); // sp0250_tick() needs to be called "sp0250_frame" times per frame! (see sp0250.cpp sp0250_init() code for info.)
void sp0250_write(UINT8 data);
void sp0250_scan(INT32 nAction, INT32 *);
