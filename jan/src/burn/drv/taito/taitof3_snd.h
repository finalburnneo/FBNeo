extern UINT8 *TaitoF3SoundRom;		// 1 MB
extern UINT8 *TaitoF3SoundRam;		// 64 KB
extern UINT8 *TaitoF3ES5506Rom;		// variable...
extern UINT8 *TaitoF3SharedRam;		// 2 KB
extern UINT8 *TaitoES5510DSPRam;	// 512 Bytes
extern UINT32 *TaitoES5510GPR;		// 192x4 Bytes
extern UINT16 *TaitoES5510DRAM;     // 4 MB
extern INT32 TaitoF3ES5506RomSize;	//
extern double TaitoF3VolumeOffset;  // Games set their own volume (mb87078), but sometimes it is too low (superchs).

void TaitoF3SoundReset();
void TaitoF3SoundExit();
void TaitoF3SoundInit(INT32 cpunum); // which cpu?

void TaitoF3CpuUpdate(INT32 nInterleave, INT32 nCurrentSlice);
void TaitoF3SoundUpdate(INT16 *pDest, INT32 nLen);

INT32 TaitoF3SoundScan(INT32 nAction, INT32 *pnMin);
