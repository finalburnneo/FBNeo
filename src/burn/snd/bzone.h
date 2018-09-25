void bzone_sound_write(UINT8 data);
void bzone_sound_update(INT16 *inputs, INT32 sample_len);

void bzone_sound_init(INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ);
void bzone_sound_exit();
void bzone_sound_reset();
void bzone_sound_scan(INT32 nAction, INT32 *);

extern INT32 bzone_sound_enable; // global sound enable

