void astrocde_snd_init(INT32 ndev, INT32 route);
void astrocde_snd_reset();
void astrocde_snd_exit();
void astrocde_snd_scan(INT32 nAction, INT32 *pnMin);
void astrocde_snd_update(INT16 *output, INT32 samples_len);
void astrocde_snd_set_buffered(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz);
void astrocde_snd_set_volume(double vol);
void astrocde_snd_write(INT32 dev, INT32 offset, UINT8 data);
