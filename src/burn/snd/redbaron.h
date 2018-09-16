void redbaron_sound_write(UINT8 data);
void redbaron_sound_update(INT16 *inputs, INT32 sample_len);

void redbaron_sound_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ);
void redbaron_sound_exit();
void redbaron_sound_reset();
void redbaron_sound_scan(INT32 nAction, INT32 *);
