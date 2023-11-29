#include "burner.h"

#ifndef UINT32_MAX
#define UINT32_MAX	(UINT32)4294967295
#endif

static struct RomDataInfo RDI = {0};
RomDataInfo* pRDI = &RDI;

struct BurnRomInfo* pDataRomDesc = nullptr;

TCHAR szRomdataName[MAX_PATH] = _T("");

static TCHAR* _strqtoken(TCHAR* s, const TCHAR* delims)
{
	static TCHAR* prev_str = nullptr;
	TCHAR* token = nullptr;

	if (!s)
	{
		if (!prev_str) return nullptr;

		s = prev_str;
	}

	s += _tcsspn(s, delims);

	if (s[0] == _T('\0'))
	{
		prev_str = nullptr;
		return nullptr;
	}

	if (s[0] == _T('\"'))
	{
		// time to grab quoted string!
		token = ++s;
		if ((s = _tcspbrk(token, _T("\""))))
		{
			*(s++) = '\0';
		}
		if (!s)
		{
			prev_str = nullptr;
			return nullptr;
		}
	}
	else
	{
		token = s;
	}

	if ((s = _tcspbrk(s, delims)))
	{
		*(s++) = _T('\0');
		prev_str = s;
	}
	else
	{
		// we're at the end of the road
#if defined (_UNICODE)
		prev_str = wmemchr(token, _T('\0'), MAX_PATH);
#else
		prev_str = (char*)memchr((void*)token, '\0', MAX_PATH);
#endif
	}

	return token;
}

#define UTF8_SIGNATURE	"\xef\xbb\xbf"

static INT32 IsDatUTF8BOM()
{
	FILE* fp = _tfopen(szRomdataName, _T("rb"));
	if (nullptr == fp) return -1;

	// get dat size
	fseek(fp, 0, SEEK_END);
	INT32 nDatSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	INT32 nRet = 0;

	if (nDatSize > strlen(UTF8_SIGNATURE))
	{
		auto pszTest = static_cast<char*>(malloc(nDatSize));

		if (nullptr != pszTest)
		{
			fread(pszTest, nDatSize, 1, fp);
			if (0 == strncmp(pszTest, UTF8_SIGNATURE, strlen(UTF8_SIGNATURE)))
			{
				nRet = 1;
			}
			free(pszTest);
			pszTest = nullptr;
		}
	}
	fclose(fp);

	return nRet;
}

#undef UTF8_SIGNATURE

#define DELIM_TOKENS_NAME	_T(" \t\r\n,%:|{}")

static INT32 LoadRomdata()
{
	INT32 nType = IsDatUTF8BOM();
	if (-1 == nType) return -1;

	RDI.nDescCount = -1; // Failed

	const TCHAR* szReadMode = (0 == nType) ? _T("rt") : _T("rt, ccs=UTF-8");

	FILE* fp = _tfopen(szRomdataName, szReadMode);
	if (nullptr == fp) return RDI.nDescCount;

	TCHAR szBuf[MAX_PATH] = {0};
	TCHAR *pszBuf = nullptr, *pszLabel = nullptr, *pszInfo = nullptr;

	memset(RDI.szExtraRom, '\0', sizeof(RDI.szExtraRom));
	memset(RDI.szFullName, '\0', sizeof(RDI.szFullName));

	while (!feof(fp))
	{
		if (_fgetts(szBuf, MAX_PATH, fp) != nullptr)
		{
			pszBuf = szBuf;

			pszLabel = _strqtoken(pszBuf, DELIM_TOKENS_NAME);
			if (nullptr == pszLabel) continue;
			if ((_T('/') == pszLabel[0]) && (_T('/') == pszLabel[1])) continue;

			if (0 == _tcsicmp(_T("ZipName"), pszLabel))
			{
				pszInfo = _strqtoken(nullptr, DELIM_TOKENS_NAME);
				if (nullptr == pszInfo) break; // No romset specified
				if (nullptr != pDataRomDesc)
				{
					strcpy(RDI.szZipName, TCHARToANSI(pszInfo, nullptr, 0));
				}
			}
			if (0 == _tcsicmp(_T("DrvName"), pszLabel))
			{
				pszInfo = _strqtoken(nullptr, DELIM_TOKENS_NAME);
				if (nullptr == pszInfo) break; // No driver specified
				if (nullptr != pDataRomDesc)
				{
					strcpy(RDI.szDrvName, TCHARToANSI(pszInfo, nullptr, 0));
				}
			}
			if (0 == _tcsicmp(_T("ExtraRom"), pszLabel))
			{
				pszInfo = _strqtoken(nullptr, DELIM_TOKENS_NAME);
				if ((nullptr != pszInfo) && (nullptr != pDataRomDesc))
				{
					strcpy(RDI.szExtraRom, TCHARToANSI(pszInfo, nullptr, 0));
				}
			}
			if (0 == _tcsicmp(_T("FullName"), pszLabel))
			{
				pszInfo = _strqtoken(nullptr, DELIM_TOKENS_NAME);
				if ((nullptr != pszInfo) && (nullptr != pDataRomDesc))
				{
#ifdef UNICODE
					_tcscpy(RDI.szFullName, pszInfo);
#else
					wcscpy(RDI.szFullName, ANSIToTCHAR(pszInfo, NULL, 0));
#endif
				}
			}

			{
				// romname, len, crc, type
				struct BurnRomInfo ri = {0};
				ri.nLen = UINT32_MAX;
				ri.nCrc = UINT32_MAX;
				ri.nType = 0;

				pszInfo = _strqtoken(nullptr, DELIM_TOKENS_NAME);
				if (nullptr != pszInfo)
				{
					_stscanf(pszInfo, _T("%x"), &(ri.nLen));
					if (UINT32_MAX == ri.nLen) continue;

					pszInfo = _strqtoken(nullptr, DELIM_TOKENS_NAME);
					if (nullptr != pszInfo)
					{
						_stscanf(pszInfo, _T("%x"), &(ri.nCrc));
						if (UINT32_MAX == ri.nCrc) continue;

						while (nullptr != (pszInfo = _strqtoken(nullptr, DELIM_TOKENS_NAME)))
						{
							INT32 nValue = -1;

							if (0 == _tcscmp(pszInfo, _T("BRF_PRG")))
							{
								ri.nType |= BRF_PRG;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_GRA")))
							{
								ri.nType |= BRF_GRA;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_SND")))
							{
								ri.nType |= BRF_SND;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_ESS")))
							{
								ri.nType |= BRF_ESS;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_BIOS")))
							{
								ri.nType |= BRF_BIOS;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_SELECT")))
							{
								ri.nType |= BRF_SELECT;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_OPT")))
							{
								ri.nType |= BRF_OPT;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("BRF_NODUMP")))
							{
								ri.nType |= BRF_NODUMP;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_PRG_68K"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_68K_PROGRAM_BYTESWAP"))))
							{
								//  1
								ri.nType |= 1;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_PRG_68K_SIMM"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_68K_PROGRAM_NO_BYTESWAP"))))
							{
								//  2
								ri.nType |= 2;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_PRG_68K_XOR_TABLE"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_Z80_PROGRAM"))))
							{
								//  3
								ri.nType |= 3;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS1_TILES")))
							{
								//  4
								ri.nType |= 4;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_OKIM6295_SAMPLES"))))
							{
								//  5
								ri.nType |= 5;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX_SIMM"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_QSOUND_SAMPLES"))))
							{
								//  6
								ri.nType |= 6;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX_SPLIT4"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_PIC"))))
							{
								//  7
								ri.nType |= 7;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX_SPLIT8"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_SF2EBBL_400000"))))
							{
								//  8
								ri.nType |= 8;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX_19XXJ"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_400000"))))
							{
								//  9
								ri.nType |= 9;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_PRG_Z80"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_SF2KORYU_400000"))))
							{
								// 10
								ri.nType |= 10;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_SF2B_400000")))
							{
								// 11
								ri.nType |= 11;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_QSND"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_SF2MKOT_400000"))))
							{
								// 12
								ri.nType |= 12;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS2_QSND_SIMM")))
							{
								// 13
								ri.nType |= 13;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS2_QSND_SIMM_BYTESWAP")))
							{
								// 14
								ri.nType |= 14;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS2_ENCRYPTION_KEY")))
							{
								// 15
								ri.nType |= 15;
								continue;
							}
							_stscanf(pszInfo, _T("%x"), &nValue);
							if (-1 != nValue)
							{
								ri.nType |= static_cast<UINT32>(nValue);
							}
						}

						if (ri.nType > 0)
						{
							RDI.nDescCount++;

							if (nullptr != pDataRomDesc)
							{
								strcpy(pDataRomDesc[RDI.nDescCount].szName, TCHARToANSI(pszLabel, nullptr, 0));

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
	if (-1 == nType) return nullptr;

	const TCHAR* szReadMode = (0 == nType) ? _T("rt") : _T("rt, ccs=UTF-8");

	FILE* fp = _tfopen(szRomdataName, szReadMode);
	if (nullptr == fp) return nullptr;

	TCHAR szBuf[MAX_PATH] = {0};
	TCHAR *pszBuf = nullptr, *pszLabel = nullptr, *pszInfo = nullptr;

	while (!feof(fp))
	{
		if (_fgetts(szBuf, MAX_PATH, fp) != nullptr)
		{
			pszBuf = szBuf;

			pszLabel = _strqtoken(pszBuf, DELIM_TOKENS_NAME);
			if (nullptr == pszLabel) continue;
			if ((_T('/') == pszLabel[0]) && (_T('/') == pszLabel[1])) continue;

			if (0 == _tcsicmp(_T("DrvName"), pszLabel))
			{
				pszInfo = _strqtoken(nullptr, DELIM_TOKENS_NAME);
				if (nullptr == pszInfo) break; // No driver specified

				fclose(fp);
				return TCHARToANSI(pszInfo, nullptr, 0);
			}
		}
	}

	fclose(fp);

	return nullptr;
}

#undef DELIM_TOKENS_NAME

void RomDataInit()
{
	INT32 nLen = LoadRomdata();

	if ((-1 != nLen) && (nullptr == pDataRomDesc))
	{
		pDataRomDesc = static_cast<struct BurnRomInfo*>(malloc((nLen + 1) * sizeof(BurnRomInfo)));
		if (nullptr != pDataRomDesc)
		{
			LoadRomdata();
			RDI.nDriverId = BurnDrvGetIndex(RDI.szDrvName);

			if (-1 != RDI.nDriverId)
			{
				BurnDrvSetZipName(RDI.szZipName, RDI.nDriverId);
			}
		}
	}
}

void RomDataSetFullName()
{
	// At this point, the driver's default ZipName has been replaced with the ZipName from the rom data
	RDI.nDriverId = BurnDrvGetIndex(RDI.szZipName);

	if (-1 != RDI.nDriverId)
	{
		wchar_t* szOldName = BurnDrvGetFullNameW(RDI.nDriverId);
		memset(RDI.szOldName, '\0', sizeof(RDI.szOldName));

		if (0 != wcscmp(szOldName, RDI.szOldName))
		{
			wcscpy(RDI.szOldName, szOldName);
		}

		BurnDrvSetFullNameW(RDI.szFullName, RDI.nDriverId);
	}
}

void RomDataExit()
{
	if (nullptr != pDataRomDesc)
	{
		free(pDataRomDesc);
		pDataRomDesc = nullptr;

		if (-1 != RDI.nDriverId)
		{
			BurnDrvSetZipName(RDI.szDrvName, RDI.nDriverId);
			if (0 != wcscmp(BurnDrvGetFullNameW(RDI.nDriverId), RDI.szOldName))
			{
				BurnDrvSetFullNameW(RDI.szOldName, RDI.nDriverId);
			}
		}

		memset(&RDI, 0, sizeof(RomDataInfo));

		RDI.nDescCount = -1;
	}
}
