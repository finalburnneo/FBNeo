#include "burner.h"

#ifndef UINT32_MAX
#define UINT32_MAX	(UINT32)4294967295U
#endif

static struct RomDataInfo RDI = { 0 };
RomDataInfo* pRDI = &RDI;

struct BurnRomInfo* pDataRomDesc = NULL;

TCHAR szRomdataName[MAX_PATH] = _T("");

static TCHAR* _strqtoken(TCHAR* s, const TCHAR* delims)
{
	static TCHAR* prev_str = NULL;
	TCHAR* token = NULL;

	if (!s) {
		if (!prev_str) return NULL;

		s = prev_str;
	}

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
#if defined (_UNICODE)
		prev_str = (TCHAR*)wmemchr(token, _T('\0'), MAX_PATH);
#else
		prev_str = (char*)memchr((void*)token, '\0', MAX_PATH);
#endif
	}

	return token;
}

static INT32 IsUTF8Text(const void* pBuffer, long size)
{
	INT32 nCode = 0;
	unsigned char* start = (unsigned char*)pBuffer;
	unsigned char* end = (unsigned char*)pBuffer + size;

	while (start < end) {
		if (*start < 0x80) {        // (10000000) ASCII
			if (0 == nCode) nCode = 1;

			start++;
		} else if (*start < 0xc0) { // (11000000) Invalid UTF-8
			return 0;
		} else if (*start < 0xe0) { // (11100000) 2-byte UTF-8
			if (nCode < 2) nCode = 2;
			if (start >= end - 1) break;
			if (0x80 != (start[1] & 0xc0)) return 0;

			start += 2;
		} else if (*start < 0xf0) { // (11110000) 3-byte UTF-8
			if (nCode < 3) nCode = 3;
			if (start >= end - 2) break;
			if ((0x80 != (start[1] & 0xc0)) || (0x80 != (start[2] & 0xc0))) return 0;

			start += 3;
		} else {
			return 0;
		}
	}

	return nCode;
}

static INT32 IsDatUTF8BOM()
{
	FILE* fp = _tfopen(szRomdataName, _T("rb"));
	if (NULL == fp) return -1;

	// get dat size
	fseek(fp, 0, SEEK_END);
	INT32 nDatSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	INT32 nRet = 0;

	char* pszTest = (char*)malloc(nDatSize + 1);

	if (NULL != pszTest) {
		memset(pszTest, 0, nDatSize + 1);
		fread(pszTest, nDatSize, 1, fp);
		nRet = IsUTF8Text(pszTest, nDatSize);
		free(pszTest);
		pszTest = NULL;
	}

	fclose(fp);

	return nRet;
}

#define DELIM_TOKENS_NAME	_T(" \t\r\n,%:|{}")

static INT32 LoadRomdata()
{
	INT32 nType = IsDatUTF8BOM();
	if (-1 == nType) return -1;

	RDI.nDescCount = -1;	// Failed

	const TCHAR* szReadMode = (3 == nType) ? _T("rt, ccs=UTF-8") : _T("rt");

	FILE* fp = _tfopen(szRomdataName, szReadMode);
	if (NULL == fp) return RDI.nDescCount;

	TCHAR szBuf[MAX_PATH] = { 0 };
	TCHAR* pszBuf = NULL, * pszLabel = NULL, * pszInfo = NULL;

	memset(RDI.szExtraRom, '\0', sizeof(RDI.szExtraRom));
	memset(RDI.szFullName, '\0', sizeof(RDI.szFullName));

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
				ri.nType = 0;

				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL != pszInfo) {
					_stscanf(pszInfo, _T("%x"), &(ri.nLen));
					if (UINT32_MAX == ri.nLen) continue;

					pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
					if (NULL != pszInfo) {
						_stscanf(pszInfo, _T("%x"), &(ri.nCrc));
						if (UINT32_MAX == ri.nCrc) continue;

						while (NULL != (pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME))) {
							INT32 nValue = -1;

							if (0 == _tcscmp(pszInfo, _T("BRF_PRG"))) {
								ri.nType |= BRF_PRG;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_GRA"))) {
								ri.nType |= BRF_GRA;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_SND"))) {
								ri.nType |= BRF_SND;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_ESS"))) {
								ri.nType |= BRF_ESS;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_BIOS"))) {
								ri.nType |= BRF_BIOS;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_SELECT"))) {
								ri.nType |= BRF_SELECT;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_OPT"))) {
								ri.nType |= BRF_OPT;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_NODUMP"))) {
								ri.nType |= BRF_NODUMP;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_PRG_68K"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_68K_PROGRAM_BYTESWAP")))) {			//  1
								ri.nType |= 1;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_PRG_68K_SIMM"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_68K_PROGRAM_NO_BYTESWAP")))) {		//  2
								ri.nType |= 2;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_PRG_68K_XOR_TABLE"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_Z80_PROGRAM")))) {					//  3
								ri.nType |= 3;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS1_TILES"))) {							//  4
								ri.nType |= 4;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_OKIM6295_SAMPLES")))) {				//  5
								ri.nType |= 5;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX_SIMM"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_QSOUND_SAMPLES")))) {				//  6
								ri.nType |= 6;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX_SPLIT4"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_PIC")))) {							//  7
								ri.nType |= 7;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX_SPLIT8"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_SF2EBBL_400000")))) {	//  8
								ri.nType |= 8;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX_19XXJ"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_400000")))) {			//  9
								ri.nType |= 9;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_PRG_Z80"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_SF2KORYU_400000")))) {	// 10
								ri.nType |= 10;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_SF2B_400000"))) {		// 11
								ri.nType |= 11;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_QSND"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_SF2MKOT_400000")))) {	// 12
								ri.nType |= 12;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS2_QSND_SIMM"))) {						// 13
								ri.nType |= 13;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS2_QSND_SIMM_BYTESWAP"))) {				// 14
								ri.nType |= 14;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS2_ENCRYPTION_KEY"))) {					// 15
								ri.nType |= 15;
								continue;
							}
							// megadrive
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_LOAD_NORMAL"))) {
								ri.nType |= 0x10;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_LOAD16_WORD_SWAP"))) {
								ri.nType |= 0x20;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_LOAD16_BYTE"))) {
								ri.nType |= 0x30;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_LOAD16_WORD_SWAP_CONTINUE_040000_100000"))) {
								ri.nType |= 0x40;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_LOAD_NORMAL_CONTINUE_020000_080000"))) {
								ri.nType |= 0x50;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_000000"))) {
								ri.nType |= 0x01;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_000001"))) {
								ri.nType |= 0x02;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_020000"))) {
								ri.nType |= 0x03;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_080000"))) {
								ri.nType |= 0x04;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_100000"))) {
								ri.nType |= 0x05;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_100001"))) {
								ri.nType |= 0x06;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_200000"))) {
								ri.nType |= 0x07;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_300000"))) {
								ri.nType |= 0x08;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_RELOAD_200000_200000"))) {
								ri.nType |= 0x09;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_RELOAD_100000_300000"))) {
								ri.nType |= 0x0a;
								continue;
							}
							_stscanf(pszInfo, _T("%x"), &nValue);
							if (-1 != nValue) {
								ri.nType |= (UINT32)nValue;
								continue;
							}
						}

						if (ri.nType > 0) {
							RDI.nDescCount++;

							if (NULL != pDataRomDesc) {
								pDataRomDesc[RDI.nDescCount].szName = (char*)malloc(512);

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

char* RomdataGetDrvName()
{
	INT32 nType = IsDatUTF8BOM();
	if (-1 == nType) return NULL;

	const TCHAR* szReadMode = (3 == nType) ? _T("rt, ccs=UTF-8") : _T("rt");

	FILE* fp = _tfopen(szRomdataName, szReadMode);
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
		memset(RDI.szOldName, '\0', sizeof(RDI.szOldName));

		if (0 != wcscmp(szOldName, RDI.szOldName)) {
			wcscpy(RDI.szOldName, szOldName);
		}

		BurnDrvSetFullNameW(RDI.szFullName, RDI.nDriverId);
	}
}

void RomDataExit()
{
	if (NULL != pDataRomDesc) {

		for (int i = 0; i < RDI.nDescCount + 1; i++) {
			free(pDataRomDesc[i].szName);
		}

		free(pDataRomDesc);
		pDataRomDesc = NULL;

		if (-1 != RDI.nDriverId) {
			BurnDrvSetZipName(RDI.szDrvName, RDI.nDriverId);
			if (0 != wcscmp(BurnDrvGetFullNameW(RDI.nDriverId), RDI.szOldName)) {
				BurnDrvSetFullNameW(RDI.szOldName, RDI.nDriverId);
			}
		}

		memset(&RDI, 0, sizeof(RomDataInfo));
		memset(szRomdataName, '\0', sizeof(szRomdataName));

		RDI.nDescCount = -1;
	}
}
