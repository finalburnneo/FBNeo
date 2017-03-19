// MSM6295 module header

#define MAX_MSM6295 (2)
#define MSM6295_PIN7_HIGH (132)
#define MSM6295_PIN7_LOW (165)

INT32 MSM6295Init(INT32 nChip, INT32 nSamplerate, bool bAddSignal);
void MSM6295SetSamplerate(INT32 nChip, INT32 nSamplerate);
void MSM6295SetRoute(INT32 nChip, double nVolume, INT32 nRouteDir);
void MSM6295Reset(INT32 nChip);
void MSM6295Exit(INT32 nChip);

INT32 MSM6295Render(INT32 nChip, INT16* pSoundBuf, INT32 nSegmenLength);
void MSM6295Command(INT32 nChip, UINT8 nCommand);
INT32 MSM6295Scan(INT32 nChip, INT32 nAction);

// for backwards compatibility. Remove when done configuring all banks
extern UINT8* MSM6295ROM;

// Call this in the driver to set the bank
void MSM6295SetBank(INT32 nChip, UINT8 *pRomData, INT32 nStart, INT32 nEnd);

inline static UINT32 MSM6295ReadStatus(const INT32 nChip)
{
#if defined FBA_DEBUG
	extern INT32 nLastMSM6295Chip;
	if (!DebugSnd_MSM6295Initted) bprintf(PRINT_ERROR, _T("MSM6295ReadStatus called without init\n"));
	if (nChip > nLastMSM6295Chip) bprintf(PRINT_ERROR, _T("MSM6295ReadStatus called with invalid chip %x\n"), nChip);
#endif

	extern UINT32 nMSM6295Status[MAX_MSM6295];

	return nMSM6295Status[nChip];
}

