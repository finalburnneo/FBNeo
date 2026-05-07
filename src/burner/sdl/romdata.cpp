#include "burner.h"

static struct RomDataInfo RDI = { 0 };
RomDataInfo* pRDI = &RDI;

struct BurnRomInfo* pDataRomDesc = NULL;

char* RomdataGetDrvName(TCHAR* szFile)
{
	return NULL;
}

void RomDataInit()
{

}

void RomDataSetFullName()
{

}

void RomDataExit()
{

}

// Check if the active driver is a RomData driver based on the active driver index and driver flags
extern "C" bool IsRomDataDrv()
{
#ifdef BUILD_WIN32
	return (nBurnDrvActive >= nIntlDrvCount && (BurnDrvGetFlags() & BDF_ROMDATA_DRIVER));
#else
	return (NULL != pDataRomDesc);
#endif // BUILD_WIN32
}

// Get the driver name from the active RomData driver
extern "C" char* RomDataDrvGetDrvName()
{
#ifdef BUILD_WIN32
	if (!pRDDrv)
		return NULL;									// RomData driver array is not available

	// The RomData driver index is calculated by subtracting the number of internal drivers from the active driver index
	const UINT32 nDataDrvActive = RomDataDrvGetActive();
	if (~0U == nDataDrvActive || nDataDrvActive >= nRDDrvCount || !pRDDrv[nDataDrvActive])
		return NULL;			// Invalid RomData driver index

	return pRDDrv[nDataDrvActive]->pszDrvName;
#else
	return RomdataGetDrvName();
#endif // BUILD_WIN32
}

// Get the ROM info array from the active RomData driver, and optionally set the ROM count if the pointer is provided
extern "C" struct BurnRomInfo* RomDataDrvGetRomInfo(UINT32* pRomCount)
{
#ifdef BUILD_WIN32
	if (!pRDDrv)
		return NULL;									// RomData driver array is not available

	// The RomData driver index is calculated by subtracting the number of internal drivers from the active driver index
	const UINT32 nDataDrvActive = RomDataDrvGetActive();
	if (~0U == nDataDrvActive || nDataDrvActive >= nRDDrvCount || !pRDDrv[nDataDrvActive])
		return NULL;									// Invalid RomData driver index

	if (pRomCount)
		// Set the output ROM count if the pointer is provided
		*pRomCount = pRDDrv[nDataDrvActive]->nRomInfoCount;

	return pRDDrv[nDataDrvActive]->pRomInfo;
#else
	if (!pDataRomDesc)
		return NULL;
	if (pRomCount && pRDI)
		*pRomCount = pRDI->nDescCount;

	return pDataRomDesc;
#endif // BUILD_WIN32
}

// For Neo Geo games with an extra ROM that needs to be mapped at 0x900000,
// this function maps it into memory after the main ROMs are loaded
void NeoProcessExtraRom(UINT8* rom)
{
#ifdef BUILD_WIN32
	if (!IsRomDataDrv())
		return;

	const char* extraRom = RomDataDrvGetExtraRom();
	if (!extraRom)
		return;

	char* romName = NULL;
	bool   found = false;
	UINT32 romLen = 0;
	UINT32 exromLen = 0;
	struct BurnRomInfo ri = { 0 };

	for (UINT32 i = 0; !BurnDrvGetRomName(&romName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);
		if (0 == strcmp(romName, extraRom)) {			// Find
			found = true;
			exromLen = ri.nLen;
		}
		if (1 == (ri.nType & 7))						// P Roms
			romLen += ri.nLen;
	}

	if (0 == exromLen || romLen <= exromLen)
		return;

	const UINT32 startOffs = romLen - exromLen;
	const UINT32 mapEnd = 0x900000 + (exromLen - 1);
	SekMapMemory(rom + startOffs, 0x900000, mapEnd, MAP_ROM);
#else
	if (pDataRomDesc && pRDI->szExtraRom) {
		UINT32 nRomLen = 0, nExtraRomLen = 0;
		for (INT32 i = 0; i < pRDI->nDescCount; i++) {
			if (1 == (pDataRomDesc[i].nType & 7)) {								// P Roms
				nRomLen += pDataRomDesc[i].nLen;

				if (0 == strcmp(pDataRomDesc[i].szName, pRDI->szExtraRom)) {	// Extra rom
					nExtraRomLen = pDataRomDesc[i].nLen;
				}
			}
		}
		if ((nExtraRomLen > 0) && (nExtraRomLen < nRomLen)) {
			SekMapMemory(rom + (nRomLen - nExtraRomLen), 0x900000, 0x900000 + (nExtraRomLen - 1), MAP_ROM);
		}
	}
#endif // BUILD_WIN32
}
