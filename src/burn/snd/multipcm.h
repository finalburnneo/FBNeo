// single chip (chip 0)
UINT8 MultiPCMRead();
void MultiPCMWrite(INT32 offset, UINT8 data);
void MultiPCMSetBank(UINT32 leftoffs, UINT32 rightoffs);
void MultiPCMSetVolume(double vol);
void MultiPCMInit(INT32 clock, UINT8 *SndROM, INT32 bAdd);
void MultiPCMSetMonoMode(INT32 monomode);

// multiple chips (up to 2), each one with its own sample rom
UINT8 MultiPCMRead(INT32 nChip);
void MultiPCMWrite(INT32 nChip, INT32 offset, UINT8 data);
void MultiPCMSetBank(INT32 nChip, UINT32 leftoffs, UINT32 rightoffs);
void MultiPCMSetVolume(INT32 nChip, double vol);
void MultiPCMInit(INT32 nChip, INT32 clock, UINT8 *SndROM, INT32 bAdd);
void MultiPCMSetMonoMode(INT32 nChip, INT32 monomode);

// all chips
void MultiPCMExit();
void MultiPCMReset();
void MultiPCMUpdate(INT16 *buffer, INT32 samples);
void MultiPCMScan(INT32 nAction, INT32 *pnMin);
