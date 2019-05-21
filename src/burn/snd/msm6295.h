// MSM6295 module header

#define MAX_MSM6295 (4)
#define MSM6295_PIN7_HIGH (132)
#define MSM6295_PIN7_LOW (165)

INT32 MSM6295Init(INT32 nChip, INT32 nSamplerate, bool bAddSignal);
void MSM6295SetSamplerate(INT32 nChip, INT32 nSamplerate);
void MSM6295SetRoute(INT32 nChip, double nVolume, INT32 nRouteDir);
void MSM6295Reset(INT32 nChip);
void MSM6295Exit(INT32 nChip);
void MSM6295Reset(); // reset all
void MSM6295Exit(); // exit all

INT32 MSM6295Render(INT16* pSoundBuf, INT32 nSegmenLength); // render all
INT32 MSM6295Render(INT32 nChip, INT16* pSoundBuf, INT32 nSegmenLength);
void MSM6295Write(INT32 nChip, UINT8 nCommand);
void MSM6295Scan(INT32 nAction, INT32 *pnMin);

// for backwards compatibility. Remove when done configuring all banks
extern UINT8* MSM6295ROM;

// Call this in the driver to set the bank
void MSM6295SetBank(INT32 nChip, UINT8 *pRomData, INT32 nStart, INT32 nEnd);

inline static UINT32 MSM6295Read(const INT32 nChip)
{
#if defined FBNEO_DEBUG
	extern INT32 nLastMSM6295Chip;
	if (!DebugSnd_MSM6295Initted) bprintf(PRINT_ERROR, _T("MSM6295Read called without init\n"));
	if (nChip > nLastMSM6295Chip) bprintf(PRINT_ERROR, _T("MSM6295Read called with invalid chip %x\n"), nChip);
#endif

	extern UINT32 nMSM6295Status[MAX_MSM6295];

	return nMSM6295Status[nChip];
}

