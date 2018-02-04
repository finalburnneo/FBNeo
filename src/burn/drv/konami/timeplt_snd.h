void TimepltSndSoundlatch(UINT8 data);
void TimepltSndReset();
void TimepltSndInit(UINT8 *rom, UINT8 *ram, INT32 z80number);
void LocomotnSndInit(UINT8 *rom, UINT8 *ram, INT32 z80number);
void TimepltSndVol(double vol0, double vol1);
void TimepltSndSrcGain(double vol);
void TimepltSndExit();
void TimepltSndUpdate(INT16 *pSoundBuf, INT32 nSegmentLength);
INT32 TimepltSndScan(INT32 nAction, INT32 *pnMin);
