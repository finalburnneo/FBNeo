void t6w28Scan(INT32 nAction, INT32 *pnMin);
void t6w28Write(INT32 offset, UINT8 data);
void t6w28Update(INT16 *buffer, INT32 samples_len);
void t6w28Init(INT32 clock, INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ, INT32 nAdd);
void t6w28Exit();
void t6w28Reset();
void t6w28Enable(bool enable);
void t6w28SetVolume(double vol);
