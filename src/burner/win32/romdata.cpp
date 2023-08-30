#include "burner.h"

#ifndef UINT32_MAX
#define UINT32_MAX	(UINT32)4294967295
#endif

static struct RomDataInfo RDI = { 0 };
RomDataInfo* pRDI = &RDI;

struct BurnRomInfo* pDataRomDesc = NULL;

static TCHAR* _strqtoken(TCHAR* s, const TCHAR* delims)
{
	static TCHAR* prev_str = NULL;
	TCHAR* token = NULL;

	if (!s) s = prev_str;
	if (!prev_str) return NULL;

	s += _tcsspn(s, delims);

	if (s[0] == _T('\0')) {
		prev_str = NULL;
		return NULL;
	}

	if (s[0] == _T('\"')) { // time to grab quoted string!
		token = ++s;
		if ((s = _tcspbrk(token, _T("\"")))) {
			*(s++) = '\0';
		}
		if (!s) {
			prev_str = NULL;
			return NULL;
		}
	} else {
		token = s;
	}

	if ((s = _tcspbrk(s, delims))) {
		*(s++) = _T('\0');
		prev_str = s;
	} else {
		// we're at the end of the road
		prev_str = (TCHAR*)memchr((void*)token, '\0', MAX_PATH);
	}

	return token;
}

#define DELIM_TOKENS_NAME	_T(" \t\r\n,%:")

static INT32 LoadRomdata()
{
	RDI.nDescCount = -1;	// Failed

	FILE* fp = _tfopen(szChoice, _T("rt"));
	if (NULL == fp) return RDI.nDescCount;

	TCHAR szBuf[MAX_PATH] = { 0 };
	TCHAR* pszBuf = NULL, * pszLabel = NULL, * pszInfo = NULL;

	memset(RDI.szExtraRom, '\0', MAX_PATH * sizeof(char));
	memset(RDI.szFullName, L'\0', MAX_PATH * sizeof(wchar_t));

	while (!feof(fp)) {
		if (_fgetts(szBuf, MAX_PATH, fp) != NULL) {
			pszBuf = szBuf;

			pszLabel = _strqtoken(pszBuf, DELIM_TOKENS_NAME);
			if (NULL == pszLabel) continue;
			if ((_T('/') == pszLabel[0]) && (_T('/') == pszLabel[1])) continue;

			if (0 == _tcsicmp(_T("ZipName"), pszLabel)) {
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL == pszInfo) break;	// No romset specified
				if (NULL != pDataRomDesc) {
					strcpy(RDI.szZipName, TCHARToANSI(pszInfo, NULL, 0));
				}
			}
			if (0 == _tcsicmp(_T("DrvName"), pszLabel)) {
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL == pszInfo) break;	// No driver specified
				if (NULL != pDataRomDesc) {
					strcpy(RDI.szDrvName, TCHARToANSI(pszInfo, NULL, 0));
				}
			}
			if (0 == _tcsicmp(_T("ExtraRom"), pszLabel)) {
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if ((NULL != pszInfo) && (NULL != pDataRomDesc)) {
					strcpy(RDI.szExtraRom, TCHARToANSI(pszInfo, NULL, 0));
				}
			}
			if (0 == _tcsicmp(_T("FullName"), pszLabel)) {
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if ((NULL != pszInfo) && (NULL != pDataRomDesc)) {
#ifdef UNICODE
					_tcscpy(RDI.szFullName, pszInfo);
#else
					wcscpy(RDI.szFullName, ANSIToTCHAR(pszInfo, NULL, 0));
#endif
				}
			}

			{
				// romname, len, crc, type
				struct BurnRomInfo ri = { 0 };
				ri.nLen  = UINT32_MAX;
				ri.nCrc  = UINT32_MAX;
				ri.nType = UINT32_MAX;

				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL != pszInfo) {
					_stscanf(pszInfo, _T("%x"), &(ri.nLen));
					if (UINT32_MAX == ri.nLen) continue;

					pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
					if (NULL != pszInfo) {
						_stscanf(pszInfo, _T("%x"), &(ri.nCrc));
						if (UINT32_MAX == ri.nCrc) continue;

						pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
						if (NULL != pszInfo) {
							_stscanf(pszInfo, _T("%x"), &(ri.nType));
							if (UINT32_MAX == ri.nType) continue;

							RDI.nDescCount++;

							if (NULL != pDataRomDesc) {
								strcpy(pDataRomDesc[RDI.nDescCount].szName, TCHARToANSI(pszLabel, NULL, 0));

								pDataRomDesc[RDI.nDescCount].nLen = ri.nLen;
								pDataRomDesc[RDI.nDescCount].nCrc = ri.nCrc;
								pDataRomDesc[RDI.nDescCount].nType = ri.nType;
							}
						}
					}
				}
			}
		}
	}

	fclose(fp);

	return RDI.nDescCount;
}

char* RomdataGetDrvName(TCHAR* szFile)
{
	FILE* fp = _tfopen(szFile, _T("rt"));
	if (NULL == fp) return NULL;

	TCHAR szBuf[MAX_PATH] = { 0 };
	TCHAR* pszBuf = NULL, * pszLabel = NULL, * pszInfo = NULL;

	while (!feof(fp)) {
		if (_fgetts(szBuf, MAX_PATH, fp) != NULL) {
			pszBuf = szBuf;

			pszLabel = _strqtoken(pszBuf, DELIM_TOKENS_NAME);
			if (NULL == pszLabel) continue;
			if ((_T('/') == pszLabel[0]) && (_T('/') == pszLabel[1])) continue;

			if (0 == _tcsicmp(_T("DrvName"), pszLabel)) {
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL == pszInfo) break;	// No driver specified

				fclose(fp);
				return TCHARToANSI(pszInfo, NULL, 0);
			}
		}
	}

	fclose(fp);

	return NULL;
}

#undef DELIM_TOKENS_NAME

void RomDataInit()
{
	INT32 nLen = LoadRomdata();

	if ((-1 != nLen) && (NULL == pDataRomDesc)) {
		pDataRomDesc = (struct BurnRomInfo*)malloc((nLen + 1) * sizeof(BurnRomInfo));
		if (NULL != pDataRomDesc) {
			LoadRomdata();
			RDI.nDriverId = BurnDrvGetIndex(RDI.szDrvName);

			if (-1 != RDI.nDriverId) {
				BurnDrvSetZipName(RDI.szZipName, RDI.nDriverId);
			}
		}
	}
}

void RomDataSetFullName()
{
	// At this point, the driver's default ZipName has been replaced with the ZipName from the rom data
	RDI.nDriverId = BurnDrvGetIndex(RDI.szZipName);

	if (-1 != RDI.nDriverId) {
		wchar_t* szOldName = BurnDrvGetFullNameW(RDI.nDriverId);
		memset(RDI.szOldName, L'\0', MAX_PATH * sizeof(wchar_t));

		if (0 != wcscmp(szOldName, RDI.szOldName)) {
			wcscpy(RDI.szOldName, szOldName);
		}

		BurnDrvSetFullNameW(RDI.szFullName, RDI.nDriverId);
	}
}

void RomDataExit()
{
	if (NULL != pDataRomDesc) {
		free(pDataRomDesc);
		pDataRomDesc = NULL;

		if (-1 != RDI.nDriverId) {
			BurnDrvSetZipName(RDI.szDrvName, RDI.nDriverId);
			if (0 != wcscmp(BurnDrvGetFullNameW(RDI.nDriverId), RDI.szOldName)) {
				BurnDrvSetFullNameW(RDI.szOldName, RDI.nDriverId);
			}
		}

		memset(&RDI, 0, sizeof(RomDataInfo));

		RDI.nDescCount = -1;
	}
}
