// MSM6295 module header

#define MAX_MSM6295 (2)

INT32 MSM6295Init(INT32 nChip, INT32 nSamplerate, float fMaxVolume, bool bAddSignal);
void MSM6295Reset(INT32 nChip);
void MSM6295Exit(INT32 nChip);

INT32 MSM6295Render(INT32 nChip, INT16* pSoundBuf, INT32 nSegmenLength);
void MSM6295Command(INT32 nChip, UINT8 nCommand);
INT32 MSM6295Scan(INT32 nChip, INT32 nAction);

extern UINT8* MSM6295ROM;
extern UINT8* MSM6295SampleInfo[MAX_MSM6295][4];
extern UINT8* MSM6295SampleData[MAX_MSM6295][4];

inline static UINT32 MSM6295ReadStatus(const INT32 nChip)
{
	extern UINT32 nMSM6295Status[MAX_MSM6295];

	return nMSM6295Status[nChip];
}

