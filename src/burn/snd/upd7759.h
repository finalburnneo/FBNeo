#define UPD7759_STANDARD_CLOCK		640000

typedef void (*drqcallback)(INT32 param);

void UPD7759Render(INT16 *pSoundBuf, INT32 samples); // render all chips
void UPD7759Render(INT32 chip, INT16 *pSoundBuf, INT32 samples); // render single chip
void UPD7759Reset();
void UPD7759Init(INT32 chip, INT32 clock, UINT8* pSoundData);
void UPD7759SetStartDelay(INT32 chip, INT32 nDelay);
void UPD7759SetRoute(INT32 chip, double nVolume, INT32 nRouteDir);
void UPD7759SetFilter(INT32 chip, INT32 nCutOff);
void UPD7759SetDrqCallback(INT32 chip, drqcallback Callback);
void UPD7759SetSyncCallback(INT32 chip, INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz);
INT32 UPD7759BusyRead(INT32 chip);
void UPD7759ResetWrite(INT32 chip, UINT8 Data);
void UPD7759StartWrite(INT32 chip, UINT8 Data);
void UPD7759PortWrite(INT32 chip, UINT8 Data);
void UPD7759Scan(INT32 nAction, INT32 *pnMin);
void UPD7759Exit();
void UPD7759SyncStream();
