#include "burnint.h"
#include "burn_sound.h"

#define DAC_NUM		(8)	// allow for 8 DAC chips

#define OLD_STYLE_DAC		// depreciated!

struct dac_info
{
	INT16			Output;
	INT32 			nVolShift;
	INT32 			nCurrentPosition;
	INT32			Initialized;
};

static struct dac_info dac_table[DAC_NUM];

static INT16 UnsignedVolTable[256];
static INT16 SignedVolTable[256];

static INT32 (*pSyncCallback)() = NULL; // should be moved to struct?

static INT16 *buffer = NULL;

static INT32 NumChips;

static INT32 bAddSignal;

// delay buffer allocation for cases when fps is not 60
static inline void allocate_buffer()
{
	if (buffer == NULL) {
		buffer = (INT16*)BurnMalloc(nBurnSoundLen * sizeof(INT16));
	}
}

static void UpdateStream(INT32 chip, INT32 len)
{
	// check buffer
	allocate_buffer();

	struct dac_info *ptr;

	{
		ptr = &dac_table[chip];

		if (ptr->Initialized == 0) return;

		INT32 length = len - ptr->nCurrentPosition;

		// should never happen...
		if (length <= 0) return;

		// can happen, so let's make sure it doesn't
		if ((length + ptr->nCurrentPosition) > nBurnSoundLen) length = nBurnSoundLen - ptr->nCurrentPosition;
		if (length <= 0) return;

		INT16 *buf = buffer + ptr->nCurrentPosition;

		ptr->nCurrentPosition += length;	

		INT32 Out = ptr->Output;
		
		while (length--) {
			*buf = BURN_SND_CLIP((INT32)(*buf + Out));
			*buf++;
		}
	}
}

void DACUpdate(INT16* Buffer, INT32 Length)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACUpdate called without init\n"));
#endif

	struct dac_info *ptr;

#ifdef OLD_STYLE_DAC
	if (pSyncCallback == NULL)
	{
		INT32 Out = 0;

		for (INT32 i = 0; i < NumChips; i++) {
			ptr = &dac_table[i];

			if (ptr->Initialized) Out += ptr->Output;
		}
		
		Out = BURN_SND_CLIP(Out);
		
		while (Length--) {
			if (bAddSignal) {
				Buffer[0] += Out;
				Buffer[1] += Out;
			} else {
				Buffer[0] = Out;
				Buffer[1] = Out;
			}
			Buffer += 2;
		}
	}
	else
#endif
	{
		for (INT32 i = 0; i < NumChips; i++) {
			UpdateStream(i, nBurnSoundLen);
		}

		INT16 *buf = buffer;
	
		while (Length--) {
			if (bAddSignal) {
				Buffer[1] = Buffer[0] += buf[0];
			} else {
				Buffer[1] = Buffer[0]  = buf[0];
			}
			Buffer += 2;
			buf[0] = 0; // clear buffer
			buf++;
		}

		for (INT32 i = 0; i < NumChips; i++) {
			ptr = &dac_table[i];
			ptr->nCurrentPosition = 0;
		}
#ifdef OLD_STYLE_DAC
	}
#endif
}

void DACWrite(INT32 Chip, UINT8 Data)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACWrite called without init\n"));
	if (Chip > NumChips) bprintf(PRINT_ERROR, _T("DACWrite called with invalid chip number %x\n"), Chip);
#endif

	struct dac_info *ptr;

#ifdef OLD_STYLE_DAC
	if (pSyncCallback != NULL)
#endif
		UpdateStream(Chip, pSyncCallback());

	ptr = &dac_table[Chip];
	ptr->Output = UnsignedVolTable[Data] >> ptr->nVolShift;
}

void DACSignedWrite(INT32 Chip, UINT8 Data)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACSignedWrite called without init\n"));
	if (Chip > NumChips) bprintf(PRINT_ERROR, _T("DACSignedWrite called with invalid chip number %x\n"), Chip);
#endif

	struct dac_info *ptr;

#ifdef OLD_STYLE_DAC
	if (pSyncCallback != NULL)
#endif
		UpdateStream(Chip, pSyncCallback());

	ptr = &dac_table[Chip];
	ptr->Output = SignedVolTable[Data] >> ptr->nVolShift;
}

static void DACBuildVolTables()
{
	INT32 i;
	
	for (i = 0;i < 256;i++) {
		UnsignedVolTable[i] = i * 0x101 / 2;
		SignedVolTable[i] = i * 0x101 - 0x8000;
	}
}

static void DACInitCommon(INT32 Num, UINT32, INT32 bAdd)
{
	struct dac_info *ptr;

	DebugSnd_DACInitted = 1;
	
	NumChips = Num+1;

	ptr = &dac_table[Num];

	memset (ptr, 0, sizeof(dac_info));

	ptr->Initialized = 1;
	ptr->nVolShift = 0;
	
	DACBuildVolTables();
	
	bAddSignal = bAdd;
}

#ifdef OLD_STYLE_DAC
void DACInit(INT32 Num, UINT32 Clock, INT32 bAdd)
{
	pSyncCallback = NULL;

	DACInitCommon(Num, Clock, bAdd);
}
#endif

void DACInit(INT32 Num, UINT32 Clock, INT32 bAdd, INT32 (*pSyncCB)())
{
	pSyncCallback = pSyncCB;

	DACInitCommon(Num, Clock, bAdd);
}

void DACSetVolShift(INT32 Chip, INT32 nShift)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACSetVolShift called without init\n"));
	if (Chip > NumChips) bprintf(PRINT_ERROR, _T("DACSetVolShift called with invalid chip number %x\n"), Chip);
#endif

	struct dac_info *ptr;

	ptr = &dac_table[Chip];
	ptr->nVolShift = nShift;
}

void DACReset()
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACReset called without init\n"));
#endif

	struct dac_info *ptr;

	for (INT32 i = 0; i < NumChips; i++) {
		ptr = &dac_table[i];

		ptr->nCurrentPosition = 0;
		ptr->Output = 0;
	}
}

void DACExit()
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACExit called without init\n"));
#endif

	struct dac_info *ptr;

	for (INT32 i = 0; i < DAC_NUM; i++) {
		ptr = &dac_table[i];

		ptr->Initialized = 0;
	}

	NumChips = 0;
	
	DebugSnd_DACInitted = 0;

	pSyncCallback = NULL;

	BurnFree (buffer);
}

INT32 DACScan(INT32 nAction,INT32 *pnMin)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACScan called without init\n"));
#endif
	
	if (pnMin != NULL) {
		*pnMin = 0x029719;
	}
	
	struct dac_info *ptr;

	if (nAction & ACB_DRIVER_DATA) {
		for (INT32 i = 0; i < NumChips; i++) {
			ptr = &dac_table[i];

			SCAN_VAR(ptr->Output);
		}
	}

	return 0;
}
