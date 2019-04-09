#include "burnint.h"
#include "dac.h"

#define DAC_NUM		(8)	// Maximum DAC chips

struct dac_info
{
	INT16	Output;
	INT16	Output2;
	INT32   Stereo;
	double 	nVolume;
	INT32 	nCurrentPosition;
	INT32	Initialized;
	INT32	OutputDir;
	INT32	(*pSyncCallback)();
};

static struct dac_info dac_table[DAC_NUM];

static INT16 UnsignedVolTable[256];
static INT16 SignedVolTable[256];

static INT16 *lBuffer = NULL;
static INT16 *rBuffer = NULL;

static INT32 NumChips;

static INT32 bAddSignal;

static INT32 (*pCPUTotalCycles)() = NULL;
static UINT32 nDACCPUMHZ = 0;

// single-order dc blocking filter
static INT32 dac_dcblock   = 0; // enable
static INT16 dac_lastin_r  = 0;
static INT16 dac_lastout_r = 0;
static INT16 dac_lastin_l  = 0;
static INT16 dac_lastout_l = 0;

void DACDCBlock(INT32 enable)
{
    dac_dcblock = enable;
}

static INT16 dc_blockR(INT16 sam)
{
    if (!dac_dcblock) return sam;
    INT16 outr = sam - dac_lastin_r + 0.998 * dac_lastout_r;
    dac_lastin_r = sam;
    dac_lastout_r = outr;

    return outr;
}

static INT16 dc_blockL(INT16 sam)
{
    if (!dac_dcblock) return sam;
    INT16 outl = sam - dac_lastin_l + 0.998 * dac_lastout_l;
    dac_lastin_l = sam;
    dac_lastout_l = outl;

    return outl;
}

static INT32 DACSyncInternal()
{
	return (INT32)(float)(nBurnSoundLen * (pCPUTotalCycles() / (nDACCPUMHZ / (nBurnFPS / 100.0000))));
}

static void UpdateStream(INT32 chip, INT32 length)
{
	struct dac_info *ptr;

	if (lBuffer == NULL) {	// delay buffer allocation for cases when fps is not 60
		lBuffer = (INT16*)BurnMalloc(nBurnSoundLen * sizeof(INT16));
		memset (lBuffer, 0, nBurnSoundLen * sizeof(INT16));
	}
	if (rBuffer == NULL) {	// delay buffer allocation for cases when fps is not 60
		rBuffer = (INT16*)BurnMalloc(nBurnSoundLen * sizeof(INT16));
		memset (rBuffer, 0, nBurnSoundLen * sizeof(INT16));
	}

	ptr = &dac_table[chip];
	if (ptr->Initialized == 0) return;

	if (length > nBurnSoundLen) length = nBurnSoundLen;
	length -= ptr->nCurrentPosition;
	if (length <= 0) return;

	INT16 *lbuf = lBuffer + ptr->nCurrentPosition;
	INT16 *rbuf = rBuffer + ptr->nCurrentPosition;

	INT16 lOut = ((ptr->OutputDir & BURN_SND_ROUTE_LEFT ) == BURN_SND_ROUTE_LEFT ) ? ptr->Output : 0;
	INT16 rOut = ((ptr->OutputDir & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) ? ((ptr->Stereo) ? ptr->Output2 : ptr->Output) : 0;

	ptr->nCurrentPosition += length;

	if (rOut && lOut) {
		while (length--) {
			*lbuf = BURN_SND_CLIP((INT32)(*lbuf + lOut));
			*rbuf = BURN_SND_CLIP((INT32)(*rbuf + rOut));
			lbuf++,rbuf++;
		}
	} else if (lOut) {
		while (length--) {
			*lbuf = BURN_SND_CLIP((INT32)(*lbuf + lOut));
			lbuf++;
		}
	} else if (rOut) {
		while (length--) {
			*rbuf = BURN_SND_CLIP((INT32)(*rbuf + rOut));
			rbuf++;
		}
	}
}

void DACUpdate(INT16* Buffer, INT32 Length)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACUpdate called without init\n"));
#endif

	struct dac_info *ptr;

	for (INT32 i = 0; i < NumChips; i++) {
		UpdateStream(i, nBurnSoundLen);
	}

	INT16 *lbuf = lBuffer;
	INT16 *rbuf = rBuffer;

	if (bAddSignal) {
		while (Length--) {
			Buffer[0] = BURN_SND_CLIP((INT32)(dc_blockL(lbuf[0]) + Buffer[0]));
			Buffer[1] = BURN_SND_CLIP((INT32)(dc_blockR(rbuf[0]) + Buffer[1]));
			Buffer += 2;
			lbuf[0] = 0; // clear buffer
			rbuf[0] = 0; // clear buffer
			lbuf++;
			rbuf++;
		}
	} else {
		while (Length--) {
			Buffer[0] = dc_blockL(lbuf[0]);
			Buffer[1] = dc_blockR(rbuf[0]);
			Buffer += 2;
			lbuf[0] = 0; // clear buffer
			rbuf[0] = 0; // clear buffer
			lbuf++;
			rbuf++;
		}
	}

	for (INT32 i = 0; i < NumChips; i++) {
		ptr = &dac_table[i];
		ptr->nCurrentPosition = 0;
	}
}

void DACWrite(INT32 Chip, UINT8 Data)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACWrite called without init\n"));
	if (Chip > NumChips) bprintf(PRINT_ERROR, _T("DACWrite called with invalid chip number %x\n"), Chip);
#endif

	struct dac_info *ptr;

	ptr = &dac_table[Chip];

	UpdateStream(Chip, ptr->pSyncCallback());

	ptr->Output = (INT32)(UnsignedVolTable[Data] * ptr->nVolume);
}

void DACWrite16(INT32 Chip, INT16 Data)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACWrite16 called without init\n"));
	if (Chip > NumChips) bprintf(PRINT_ERROR, _T("DACWrite16 called with invalid chip number %x\n"), Chip);
#endif

	struct dac_info *ptr;

	ptr = &dac_table[Chip];

	Data = (INT32)(Data * ptr->nVolume);

	if (Data != ptr->Output) {
		UpdateStream(Chip, ptr->pSyncCallback());
		ptr->Output = Data;
	}
}

void DACWrite16Stereo(INT32 Chip, INT16 Data, INT16 Data2)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACWrite16Stereo called without init\n"));
	if (Chip > NumChips) bprintf(PRINT_ERROR, _T("DACWrite16Stereo called with invalid chip number %x\n"), Chip);
#endif

	struct dac_info *ptr;

	ptr = &dac_table[Chip];

	Data = (INT32)(Data * ptr->nVolume);
	Data2 = (INT32)(Data2 * ptr->nVolume);

	if (Data != ptr->Output || Data2 != ptr->Output2) {
		UpdateStream(Chip, ptr->pSyncCallback());
		ptr->Output = Data;
		ptr->Output2 = Data2;
	}
}

void DACWrite16Signed(INT32 Chip, UINT16 Data)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACWrite16 called without init\n"));
	if (Chip > NumChips) bprintf(PRINT_ERROR, _T("DACWrite16 called with invalid chip number %x\n"), Chip);
#endif

	struct dac_info *ptr;

	ptr = &dac_table[Chip];

    INT16 Signed = Data - 0x8000;

	Data = (INT32)(Signed * ptr->nVolume);

	if (Data != ptr->Output) {
		UpdateStream(Chip, ptr->pSyncCallback());
		ptr->Output = Data;
	}
}

void DACSignedWrite(INT32 Chip, UINT8 Data)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACSignedWrite called without init\n"));
	if (Chip > NumChips) bprintf(PRINT_ERROR, _T("DACSignedWrite called with invalid chip number %x\n"), Chip);
#endif
	struct dac_info *ptr;

	ptr = &dac_table[Chip];

	UpdateStream(Chip, ptr->pSyncCallback());

	ptr->Output = (INT32)(SignedVolTable[Data] * ptr->nVolume);
}

static void DACBuildVolTables()
{
	for (INT32 i = 0;i < 256;i++) {
		UnsignedVolTable[i] = i * 0x101 / 2;
		SignedVolTable[i] = i * 0x101 - 0x8000;
	}
}

void DACStereoMode(INT32 Chip)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACStereoMode called without init\n"));
	if (Chip > NumChips) bprintf(PRINT_ERROR, _T("DACStereoMode called with invalid chip number %x\n"), Chip);
#endif

	struct dac_info *ptr;
	ptr = &dac_table[Chip];

	ptr->Stereo = 1;
}

void DACInit(INT32 Num, UINT32 Clock, INT32 bAdd, INT32 (*pCPUCyclesCB)(), INT32 nCpuMHZ)
{
	if (pCPUCyclesCB == NULL) {
		bprintf(PRINT_ERROR, _T("DACInit pCPUCyclesCB is NULL.\n"));
	}
	if (nCpuMHZ == 0) {
		bprintf(PRINT_ERROR, _T("DACInit nCPUMHZ is 0.\n"));
	}

	pCPUTotalCycles = pCPUCyclesCB;
	nDACCPUMHZ = nCpuMHZ;

	DACInit(Num, Clock, bAdd, DACSyncInternal);
}


void DACInit(INT32 Num, UINT32 /*Clock*/, INT32 bAdd, INT32 (*pSyncCB)())
{
#if defined FBA_DEBUG
	if (Num >= DAC_NUM) bprintf (PRINT_ERROR, _T("DACInit called for too many chips (%d)! Change DAC_NUM (%d)!\n"), Num, DAC_NUM);
	if (pSyncCB == NULL) bprintf (PRINT_ERROR, _T("DACInit called with NULL callback!\n"));
#endif

	struct dac_info *ptr;

	DebugSnd_DACInitted = 1;
	
	NumChips = Num + 1;

	ptr = &dac_table[Num];

	memset (ptr, 0, sizeof(dac_info));

	ptr->Initialized = 1;
	ptr->nVolume = 1.00;
	ptr->OutputDir = BURN_SND_ROUTE_BOTH;
	ptr->Stereo = 0;
	ptr->pSyncCallback = pSyncCB;

	DACBuildVolTables(); // necessary to build for every chip?
	
	bAddSignal = bAdd;
}

void DACSetRoute(INT32 Chip, double nVolume, INT32 nRouteDir)
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACSetRoute called without init\n"));
	if (Chip > NumChips) bprintf(PRINT_ERROR, _T("DACSetRoute called with invalid chip %i\n"), Chip);
#endif

	struct dac_info *ptr;

	ptr = &dac_table[Chip];
	ptr->nVolume = nVolume;
	ptr->OutputDir = nRouteDir;
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
		ptr->Output2 = 0;
    }

    dac_lastin_r = dac_lastout_r = 0;
	dac_lastin_l = dac_lastout_l = 0;
}

void DACExit()
{
#if defined FBA_DEBUG
	if (!DebugSnd_DACInitted) bprintf(PRINT_ERROR, _T("DACExit called without init\n"));
#endif

	if (!DebugSnd_DACInitted) return;

	struct dac_info *ptr;

	for (INT32 i = 0; i < DAC_NUM; i++) {
		ptr = &dac_table[i];

		ptr->Initialized = 0;
		ptr->pSyncCallback = NULL;
	}

	pCPUTotalCycles = NULL;
	nDACCPUMHZ = 0;

	NumChips = 0;

    dac_dcblock = 0;

	DebugSnd_DACInitted = 0;

	BurnFree (lBuffer);
	BurnFree (rBuffer);
	lBuffer = NULL;
	rBuffer = NULL;
}

void DACScan(INT32 nAction, INT32 *pnMin)
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
			SCAN_VAR(ptr->Output2);
		}
	}
}
