// =============================================================================
//  FBNeo  -  RomData core (platform-independent)
// =============================================================================

#pragma once
#ifndef ROMDATA_CORE_H
#define ROMDATA_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

void  RomDataScan(const TCHAR* szDir, const TCHAR* szCachePath);
void  RomDataScanAppend(const TCHAR* szDir, const TCHAR* szCachePath);
void  RomDataFree(void);

INT32 RomDataIdentify(const TCHAR* szDatPath, char* szShortName, INT32 nShortNameLen);
INT32 RomDataLoadOne(const TCHAR* szDatPath);
INT32 RomDataCopyOne(const TCHAR* szDatPath, const TCHAR* szDestDir);
INT32 RomDataImportOne(const TCHAR* szDatPath, const TCHAR* szDestDir);
void  RomDataImportDirectory(const TCHAR* szDir, const TCHAR* szDestDir);

bool  IsRomDataDrv(void);
char* RomDataDrvGetDrvName(void);
struct BurnRomInfo* RomDataDrvGetRomInfo(UINT32* pRomCount);
const char* RomDataDrvGetExtName(void);

#ifdef __cplusplus
}
#endif

#endif // ROMDATA_CORE_H
