void K051649Update(INT16 *pBuf, INT32 samples);
void K051649Init(INT32 clock);
void K051649SetRoute(double nVolume, INT32 nRouteDir);
void K051649SetSync(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz);
void K051649Reset();
void K051649Exit();

void K051649Scan(INT32 nAction, INT32 *pnMin);

void K051649Write(INT32 offset, UINT8 data);
UINT8 K051649Read(INT32 offset);

void K051649WaveformWrite(INT32 offset, INT32 data);
UINT8 K051649WaveformRead(INT32 offset);

void K052539WaveformWrite(INT32 offset, INT32 data);

void K051649VolumeWrite(INT32 offset, INT32 data);
void K051649FrequencyWrite(INT32 offset, INT32 data);
void K051649KeyonoffWrite(INT32 data);
