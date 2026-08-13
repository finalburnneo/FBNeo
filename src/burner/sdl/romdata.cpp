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

// RomData driver API used by the core (neo_run.cpp etc.).
// SDL build uses this lightweight stub (no .dat database), matching the
// reference behaviour: no RomData driver is active, so these return defaults.

extern "C" bool IsRomDataDrv(void)
{
	return false;
}

extern "C" char* RomDataDrvGetDrvName(void)
{
	return NULL;
}

extern "C" struct BurnRomInfo* RomDataDrvGetRomInfo(UINT32* pRomCount)
{
	if (pRomCount) *pRomCount = 0;
	return NULL;
}

extern "C" const char* RomDataDrvGetExtName(void)
{
	return NULL;
}
