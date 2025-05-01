#include "burner.h"

#ifndef UINT32_MAX
#define UINT32_MAX	(UINT32)4294967295U
#endif

#define IS_STRING_EMPTY(s) (NULL == (s) || (s)[0] == _T('\0'))

struct DatListInfo {
	TCHAR szRomSet[100];
	TCHAR szFullName[1024];
	TCHAR szDrvName[100];
	TCHAR szHardware[256];
	UINT32 nMarker;
};

struct LVCOMPAREINFO
{
	HWND  hWnd;
	INT32 nColumn;
	BOOL  bAscending;
};
static LVCOMPAREINFO lv_compare;

struct PNGRESOLUTION { INT32 nWidth; INT32 nHeight; };

struct BurnRomInfo* pDataRomDesc = NULL;

static struct RomDataInfo RDI = { 0 };
RomDataInfo*  pRDI = &RDI;

static HBRUSH hWhiteBGBrush;

static INT32  sort_direction = 0;
enum {
	SORT_ASCENDING = 0,
	SORT_DESCENDING = 1
};

static HWND hRDMgrWnd   = NULL;
static HWND hRDListView = NULL;
static HWND hRdCoverDlg = NULL;

static TCHAR szCover[MAX_PATH] = { 0 };

static INT32 nSelItem = -1;

TCHAR szRomdataName[MAX_PATH] = _T("");
bool  bRDListScanSub          = false;

static HIMAGELIST hHardwareIconList = NULL;

struct HardwareIcon {
	UINT32 nHardwareCode;
	INT32  nIconIndex;
};

static struct HardwareIcon IconTable[] =
{
	{	HARDWARE_SEGA_MEGADRIVE,		-1	},
	{	HARDWARE_PCENGINE_PCENGINE,		-1	},
	{	HARDWARE_PCENGINE_SGX,			-1	},
	{	HARDWARE_PCENGINE_TG16,			-1	},
	{	HARDWARE_SEGA_SG1000,			-1	},
	{	HARDWARE_COLECO,				-1	},
	{	HARDWARE_SEGA_MASTER_SYSTEM,	-1	},
	{	HARDWARE_SEGA_GAME_GEAR,		-1	},
	{	HARDWARE_MSX,					-1	},
	{	HARDWARE_SPECTRUM,				-1	},
	{	HARDWARE_NES,					-1	},
	{	HARDWARE_FDS,					-1	},
	{	HARDWARE_SNES,					-1	},
	{	HARDWARE_SNK_NGPC,				-1	},
	{	HARDWARE_SNK_NGP,				-1	},
	{	HARDWARE_CHANNELF,				-1	},
	{	0,								-1	},

	{	~0U,							-1	}	// End Marker
};

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

static INT32 FileExists(const TCHAR* szName)
{
	return GetFileAttributes(szName) != INVALID_FILE_ATTRIBUTES;
}

static HIMAGELIST HardwareIconListInit()
{
	if (!bEnableIcons) return NULL;

	INT32 cx = 0, cy = 0, nIdx = 0;

	switch (nIconsSize) {
		case ICON_16x16: cx = cy = 16;	break;
		case ICON_24x24: cx = cy = 24;	break;
		case ICON_32x32: cx = cy = 32;	break;
	}

	hHardwareIconList = ImageList_Create(cx, cy, ILC_COLOR32 | ILC_MASK, (sizeof(IconTable) / sizeof(HardwareIcon)) - 1, 0);
	if (NULL == hHardwareIconList) return NULL;
	ListView_SetImageList(hRDListView, hHardwareIconList, LVSIL_SMALL);

	struct HardwareIcon* _it = &IconTable[0];

	while (~0U != _it->nHardwareCode) {
		_it->nIconIndex = ImageList_AddIcon(hHardwareIconList, pIconsCache[nBurnDrvCount + nIdx]);

		_it++, nIdx++;
	}

	return hHardwareIconList;
}

static void DestroyHardwareIconList()
{
	struct HardwareIcon* _it = &IconTable[0];

	while (~0U != _it->nHardwareCode) {
		_it->nIconIndex = -1; _it++;
	}

	if (NULL != hHardwareIconList) {
		ImageList_Destroy(hHardwareIconList); hHardwareIconList = NULL;
	}
}

static INT32 FindHardwareIconIndex(const TCHAR* pszDrvName)
{
	if (!bEnableIcons) return 0;

	const UINT32 nOldDrvSel    = nBurnDrvActive;	// Backup
	nBurnDrvActive             = BurnDrvGetIndex(TCHARToANSI(pszDrvName, NULL, 0));
	const UINT32 nHardwareCode = BurnDrvGetHardwareCode();

	struct HardwareIcon* _it = &IconTable[0];
	INT32 nIdx = 0;

	while (~0U != _it->nHardwareCode) {
		if (_it->nHardwareCode > 0) {				// Consoles
			if (_it->nHardwareCode == (nHardwareCode & HARDWARE_SNK_NGPC)) {
				nBurnDrvActive = nOldDrvSel;		// Restore
				return _it->nIconIndex;				// NeoGeo Pocket Color
			}
			if (_it->nHardwareCode == (nHardwareCode & HARDWARE_PUBLIC_MASK)) {
				nBurnDrvActive = nOldDrvSel;		// Restore
				return _it->nIconIndex;
			}
		}
		_it++, nIdx++;
	}

	nBurnDrvActive = nOldDrvSel;					// Restore
	return IconTable[--nIdx].nIconIndex;			// Arcade
}

static INT32 IsUTF8Text(const void* pBuffer, long size)
{
	INT32 nCode  = 0;
	UINT8* start = (UINT8*)pBuffer;
	UINT8* end   = (UINT8*)pBuffer + size;

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

	memset(RDI.szExtraRom, 0, sizeof(RDI.szExtraRom));
	memset(RDI.szFullName, 0, sizeof(RDI.szFullName));

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
								(0 == _tcscmp(pszInfo, _T("CPS1_68K_PROGRAM_BYTESWAP"))) ||
								(0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_000000")))) {			//  1
								ri.nType |= 0x01;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_PRG_68K_SIMM"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_68K_PROGRAM_NO_BYTESWAP"))) ||
								(0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_000001")))) {			//  2
								ri.nType |= 0x02;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_PRG_68K_XOR_TABLE"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_Z80_PROGRAM"))) ||
								(0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_020000")))) {			//  3
								ri.nType |= 0x03;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS1_TILES"))) ||
								(0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_080000")))) {			//  4
								ri.nType |= 0x04;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_OKIM6295_SAMPLES"))) ||
								(0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_100000")))) {			//  5
								ri.nType |= 0x05;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX_SIMM"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_QSOUND_SAMPLES"))) ||
								(0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_100001")))) {			//  6
								ri.nType |= 0x06;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX_SPLIT4"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_PIC"))) ||
								(0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_200000")))) {			//  7
								ri.nType |= 0x07;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX_SPLIT8"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_SF2EBBL_400000"))) ||
								(0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_OFFS_300000")))) {			//  8
								ri.nType |= 0x08;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_GFX_19XXJ"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_400000"))) ||
								(0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_RELOAD_200000_200000")))) {	//  9
								ri.nType |= 0x09;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_PRG_Z80"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_SF2KORYU_400000"))) ||
								(0 == _tcscmp(pszInfo, _T("SEGA_MD_ROM_RELOAD_100000_300000")))) {	// 10
								ri.nType |= 0x0a;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_SF2B_400000"))) {		// 11
								ri.nType |= 0x0b;
								continue;
							}
							if ((0 == _tcscmp(pszInfo, _T("CPS2_QSND"))) ||
								(0 == _tcscmp(pszInfo, _T("CPS1_EXTRA_TILES_SF2MKOT_400000")))) {	// 12
								ri.nType |= 0x0c;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS2_QSND_SIMM"))) {						// 13
								ri.nType |= 0x0d;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS2_QSND_SIMM_BYTESWAP"))) {				// 14
								ri.nType |= 0x0e;
								continue;
							}
							if (0 == _tcscmp(pszInfo, _T("CPS2_ENCRYPTION_KEY"))) {					// 15
								ri.nType |= 0x0f;
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

								pDataRomDesc[RDI.nDescCount].nLen  = ri.nLen;
								pDataRomDesc[RDI.nDescCount].nCrc  = ri.nCrc;
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

static DatListInfo* RomdataGetListInfo(const TCHAR* pszDatFile)
{
	if (!FileExists(pszDatFile)) return NULL;

	DatListInfo* pDatListInfo = (DatListInfo*)malloc(sizeof(DatListInfo));
	if (NULL == pDatListInfo)    return NULL;

	_tcscpy(szRomdataName,           pszDatFile);

	INT32 nType = IsDatUTF8BOM();
	memset(szRomdataName, 0, sizeof(szRomdataName));
	memset(pDatListInfo,  0, sizeof(DatListInfo));

	if (-1 == nType) {
		free(pDatListInfo);
		return NULL;
	}

	const TCHAR* szReadMode = (3 == nType) ? _T("rt, ccs=UTF-8") : _T("rt");

	FILE* fp = _tfopen(pszDatFile, szReadMode);
	if (NULL == fp) {
		free(pDatListInfo);
		return NULL;
	}

	TCHAR szBuf[MAX_PATH] = { 0 };
	TCHAR* pszBuf = NULL, * pszLabel = NULL, * pszInfo = NULL;

	while (!feof(fp)) {
		if (NULL != _fgetts(szBuf, MAX_PATH, fp)) {
			pszBuf = szBuf;

			pszLabel = _strqtoken(pszBuf, DELIM_TOKENS_NAME);
			if (NULL == pszLabel) continue;
			if ((_T('/') == pszLabel[0]) && (_T('/') == pszLabel[1])) continue;

			if (0 == _tcsicmp(_T("ZipName"), pszLabel)) {	// Romset
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL == pszInfo) {						// No romset specified
					free(pDatListInfo); fclose(fp);
					return NULL;
				}
				_tcscpy(pDatListInfo->szRomSet, pszInfo);
				pDatListInfo->nMarker |= (1 << 0);
			}
			if (0 == _tcsicmp(_T("DrvName"), pszLabel)) {	// Driver
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL == pszInfo) {						// No driver specified
					free(pDatListInfo); fclose(fp);
					return NULL;
				}
				_tcscpy(pDatListInfo->szDrvName, pszInfo);
				pDatListInfo->nMarker |= (1 << 1);

				{											// Hardware
					UINT32 nOldDrvSel = nBurnDrvActive;		// Backup
					nBurnDrvActive = BurnDrvGetIndex(TCHARToANSI(pDatListInfo->szDrvName, NULL, 0));

					INT32 nGetTextFlags = nLoadMenuShowY & (1 << 31) ? DRV_ASCIIONLY : 0;

					TCHAR szUnknown[100]   = { 0 };
					TCHAR szCartridge[100] = { 0 };

					_stprintf(szUnknown,   FBALoadStringEx(hAppInst, IDS_ERR_UNKNOWN,   true));
					_stprintf(szCartridge, FBALoadStringEx(hAppInst, IDS_MVS_CARTRIDGE, true));
					_stprintf(
						pDatListInfo->szHardware,
						FBALoadStringEx(hAppInst, IDS_ROMDATA_HARDWARE_DESC, true),
						(HARDWARE_SNK_MVS == (BurnDrvGetHardwareCode() & HARDWARE_SNK_MVS) && HARDWARE_SNK_NEOGEO == (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)) ? szCartridge : BurnDrvGetText(nGetTextFlags | DRV_SYSTEM)
					);

					pDatListInfo->nMarker |= (1 << 2);
					nBurnDrvActive = nOldDrvSel;			// Restore
				}
			}
			if (0 == _tcsicmp(_T("FullName"), pszLabel)) {	// FullName
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				_tcscpy(pDatListInfo->szFullName, pszInfo);
				pDatListInfo->nMarker |= (1 << 3);
			}
		}
	}
	fclose(fp);

	if (!(pDatListInfo->nMarker & (UINT32)0x03)) {			// No romset specified or No driver specified
		free(pDatListInfo); pDatListInfo = NULL;
		return NULL;
	}
	if (!(pDatListInfo->nMarker & (UINT32)0x08)) {			// No FullName specified
		_tcscpy(pDatListInfo->szFullName, pDatListInfo->szRomSet);
	}

	return pDatListInfo;
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

		free(pDataRomDesc); pDataRomDesc = NULL;

		if (-1 != RDI.nDriverId) {
			BurnDrvSetZipName(RDI.szDrvName, RDI.nDriverId);
			if (0 != wcscmp(BurnDrvGetFullNameW(RDI.nDriverId), RDI.szOldName)) {
				BurnDrvSetFullNameW(RDI.szOldName, RDI.nDriverId);
			}
		}

		memset(&RDI,          0, sizeof(RomDataInfo));
		memset(szRomdataName, 0, sizeof(szRomdataName));

		RDI.nDescCount = -1;
	}
}

// Add DatListInfo to List
static INT32 RomdataAddListItem(TCHAR* pszDatFile)
{
	DatListInfo* pDatListInfo = RomdataGetListInfo(pszDatFile);
	if (NULL == pDatListInfo) return -1;

	LVITEM lvi;
	memset(&lvi, 0, sizeof(LVITEM));

	// Dat path
	lvi.iImage     = FindHardwareIconIndex(pDatListInfo->szDrvName);
	lvi.iItem      = ListView_GetItemCount(hRDListView);
	lvi.mask       = LVIF_TEXT | LVIF_IMAGE;
	lvi.cchTextMax = MAX_PATH;
	lvi.pszText    = pszDatFile;

	INT32 nItem = ListView_InsertItem(hRDListView, &lvi);

	// Romset
	memset(&lvi, 0, sizeof(LVITEM));

	lvi.iSubItem   = 1;
	lvi.iItem      = nItem;
	lvi.cchTextMax = 100;
	lvi.mask       = LVIF_TEXT | LVIF_IMAGE;
	lvi.pszText    = pDatListInfo->szRomSet;

	ListView_SetItem(hRDListView, &lvi);

	free(pDatListInfo); pDatListInfo = NULL;

	return nItem;
}

static INT32 ends_with_slash(const TCHAR* dirPath)
{
	UINT32 len = _tcslen(dirPath);
	if (0 == len) return 0;

	TCHAR last_char = dirPath[len - 1];
	return (last_char == _T('/') || last_char == _T('\\'));
}

static void RomdataListFindDats(const TCHAR* dirPath)
{
	if (IS_STRING_EMPTY(dirPath)) return;

	TCHAR searchPath[MAX_PATH] = { 0 };

	const TCHAR* szFormatA = ends_with_slash(dirPath) ? _T("%s*") : _T("%s\\*.*");
	const TCHAR* szFormatB = ends_with_slash(dirPath) ? _T("%s%s") : _T("%s\\%s");

	_stprintf(searchPath, szFormatA, dirPath);

	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile(searchPath, &findFileData);
	if (INVALID_HANDLE_VALUE == hFind) return;

	do {
		if (0 == _tcscmp(findFileData.cFileName, _T(".")) || 0 == _tcscmp(findFileData.cFileName, _T("..")))
			continue;
		// like: c:\1st_dir + '\' + 2nd_dir + '\' + "1.dat" + '\0' = 8 chars
		if ((_tcslen(dirPath) + _tcslen(findFileData.cFileName)) > (MAX_PATH - 8))
			continue;

		TCHAR szFullPath[MAX_PATH] = { 0 };
		_stprintf(szFullPath, szFormatB, dirPath, findFileData.cFileName);

		if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (bRDListScanSub)
				RomdataListFindDats(szFullPath);
		} else {
			if (IsFileExt(findFileData.cFileName, _T(".dat")))
				RomdataAddListItem(szFullPath);
		}
	} while (FindNextFile(hFind, &findFileData));

	FindClose(hFind);
}

#undef IS_STRING_EMPTY

static INT32 CALLBACK ListViewCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	TCHAR buf1[MAX_PATH];
	TCHAR buf2[MAX_PATH];
	LVCOMPAREINFO* lpsd = (struct LVCOMPAREINFO*)lParamSort;

	ListView_GetItemText(lpsd->hWnd, (INT32)lParam1, lpsd->nColumn, buf1, sizeof(buf1));
	ListView_GetItemText(lpsd->hWnd, (INT32)lParam2, lpsd->nColumn, buf2, sizeof(buf2));

	switch (lpsd->bAscending) {
		case SORT_ASCENDING:
			return (_wcsicmp(buf1, buf2));
		case SORT_DESCENDING:
			return (0 - _wcsicmp(buf1, buf2));
	}

	return 0;
}

static void ListViewSort(INT32 nDirection, INT32 nColumn)
{
	// sort the list
	lv_compare.hWnd       = hRDMgrWnd;
	lv_compare.nColumn    = nColumn;
	lv_compare.bAscending = nDirection;
	ListView_SortItemsEx(hRDListView, ListViewCompareFunc, &lv_compare);
}

static void RomDataInitListView()
{
	LVCOLUMN LvCol;
	ListView_SetExtendedListViewStyle(hRDListView, LVS_EX_FULLROWSELECT);

	memset(&LvCol, 0, sizeof(LvCol));
	LvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

	LvCol.cx       = 415;
	LvCol.pszText  = FBALoadStringEx(hAppInst, IDS_ROMDATA_DATPATH, true);
	SendMessage(hRDListView, LVM_INSERTCOLUMN, 0, (LPARAM)&LvCol);

	LvCol.cx       = 100;
	LvCol.pszText  = FBALoadStringEx(hAppInst, IDS_ROMDATA_ROMSET, true);
	SendMessage(hRDListView, LVM_INSERTCOLUMN, 1, (LPARAM)&LvCol);

	sort_direction = SORT_ASCENDING; // dink
}

static PNGRESOLUTION GetPNGResolution(TCHAR* szFile)
{
	PNGRESOLUTION nResolution = { 0, 0 };
	IMAGE img = { 0, 0, 0, 0, NULL, NULL, 0 };

	FILE* fp = _tfopen(szFile, _T("rb"));

	if (!fp) {
		return nResolution;
	}

	PNGGetInfo(&img, fp);

	nResolution.nWidth  = img.width;
	nResolution.nHeight = img.height;

	fclose(fp);

	return nResolution;
}

static void RomDataShowPreview(HWND hDlg, TCHAR* szFile, INT32 nCtrID, INT32 nFrameCtrID, float maxw, float maxh)
{
	PNGRESOLUTION PNGRes = { 0, 0 };
	if (!_tfopen(szFile, _T("rb"))) {
		HRSRC hrsrc     = FindResource(NULL, MAKEINTRESOURCE(BMP_SPLASH), RT_BITMAP);
		HGLOBAL hglobal = LoadResource(NULL, (HRSRC)hrsrc);

		BITMAPINFOHEADER* pbmih = (BITMAPINFOHEADER*)LockResource(hglobal);

		PNGRes.nWidth  = pbmih->biWidth;
		PNGRes.nHeight = pbmih->biHeight;

		FreeResource(hglobal);
	} else {
		PNGRes = GetPNGResolution(szFile);
	}

	// ------------------------------------------------------
	// PROPER ASPECT RATIO CALCULATIONS

	float w = (float)PNGRes.nWidth;
	float h = (float)PNGRes.nHeight;

	if (maxw == 0 && maxh == 0) {
		// derive max w/h from dialog size
		RECT rect = { 0, 0, 0, 0 };
		GetWindowRect(GetDlgItem(hDlg, IDC_STATIC2), &rect);
		INT32 dw = rect.right  - rect.left;
		INT32 dh = rect.bottom - rect.top;

		dw = dw * 90 / 100; // make W 90% of the "Preview / Title" windowpane
		dh = dw * 75 / 100; // make H 75% of w (4:3)

		maxw = dw;
		maxh = dh;
	}

	// max WIDTH
	if (w > maxw) {
		float nh = maxw * (float)(h / w);
		float nw = maxw;

		// max HEIGHT
		if (nh > maxh) {
			nw = maxh * (float)(nw / nh);
			nh = maxh;
		}

		w = nw;
		h = nh;
	}

	// max HEIGHT
	if (h > maxh) {
		float nw = maxh * (float)(w / h);
		float nh = maxh;

		// max WIDTH
		if (nw > maxw) {
			nh = maxw * (float)(nh / nw);
			nw = maxw;
		}

		w = nw;
		h = nh;
	}

	// Proper centering of preview
	float x = ((maxw - w) / 2);
	float y = ((maxh - h) / 2);

	RECT rc = { 0, 0, 0, 0 };
	GetWindowRect(GetDlgItem(hDlg, nFrameCtrID), &rc);

	POINT pt;
	pt.x = rc.left;
	pt.y = rc.top;

	ScreenToClient(hDlg, &pt);

	// ------------------------------------------------------

	FILE* fp = _tfopen(szFile, _T("rb"));

	HBITMAP hCoverBmp = PNGLoadBitmap(hDlg, fp, (int)w, (int)h, 0);

	SetWindowPos(GetDlgItem(hDlg, nCtrID), NULL, (int)(pt.x + x), (int)(pt.y + y), 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
	SendDlgItemMessage(hDlg, nCtrID, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hCoverBmp);

	if (fp) fclose(fp);

}

static void RomDataClearList()
{
	ListView_DeleteAllItems(hRDListView);

	RomDataShowPreview(hRDMgrWnd, _T(""), IDC_ROMDATA_TITLE,   IDC_ROMDATA_TITLE_FRAME,    0, 0);
	RomDataShowPreview(hRDMgrWnd, _T(""), IDC_ROMDATA_PREVIEW, IDC_ROMDATA_PREVIEW_FRAME,  0, 0);

	SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTGAME),     _T(""));
	SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDRIVER),   _T(""));
	SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTHARDWARE), _T(""));

	nSelItem = -1;
}

static INT_PTR CALLBACK RomDataCoveProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM /*lParam*/)
{
	if (Msg == WM_INITDIALOG) {
		hRdCoverDlg = hDlg;
		RomDataShowPreview(hDlg, szCover, IDC_ROMDATA_COVER_PREVIEW_PIC, IDC_ROMDATA_COVER_PREVIEW_PIC, 580, 415);

		return TRUE;
	}

	if (Msg == WM_CLOSE) {
		EndDialog(hDlg, 0); hRdCoverDlg = NULL;
	}

	if (Msg == WM_COMMAND) {
		if (LOWORD(wParam) == WM_DESTROY) SendMessage(hDlg, WM_CLOSE, 0, 0);
	}

	return 0;
}

static void RomdataCoverInit()
{
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_ROMDATA_COVER_DLG), hRDMgrWnd, (DLGPROC)RomDataCoveProc);
}

static void RomDataManagerExit()
{
	RomDataClearList();
	DestroyHardwareIconList();
	DeleteObject(hWhiteBGBrush);

	hRDMgrWnd = hRDListView = NULL;
}

static INT_PTR CALLBACK RomDataManagerProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_INITDIALOG) {
		hRDMgrWnd = hDlg;

		INITCOMMONCONTROLSEX icc;
		icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
		icc.dwICC  = ICC_LISTVIEW_CLASSES;
		InitCommonControlsEx(&icc);

		hRDListView   = GetDlgItem(hDlg, IDC_ROMDATA_LIST);

		RomDataInitListView();
		HardwareIconListInit();

		HICON hIcon   = LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_APP));
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);		// Set the Game Selection dialog icon.

		hWhiteBGBrush = CreateSolidBrush(RGB(0xff, 0xff, 0xff));

		RomDataShowPreview(hRDMgrWnd, _T(""), IDC_ROMDATA_TITLE,   IDC_ROMDATA_TITLE_FRAME,    0, 0);
		RomDataShowPreview(hRDMgrWnd, _T(""), IDC_ROMDATA_PREVIEW, IDC_ROMDATA_PREVIEW_FRAME,  0, 0);

		SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTGAME),     _T(""));
		SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDRIVER),   _T(""));
		SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTHARDWARE), _T(""));

		CheckDlgButton(hRDMgrWnd, IDC_ROMDATA_SSUBDIR_CHECK, bRDListScanSub ? BST_CHECKED : BST_UNCHECKED);

		TreeView_SetItemHeight(hRDListView, 40);

		TCHAR szDialogTitle[200] = { 0 };
		_stprintf(szDialogTitle, FBALoadStringEx(hAppInst, IDS_ROMDATAMANAGER_TITLE, true), _T(APP_TITLE), _T(SEPERATOR_1), _T(SEPERATOR_1));
		SetWindowText(hDlg, szDialogTitle);

		RomdataListFindDats(szAppRomdataPath);
		WndInMid(hDlg, hScrnWnd);
		SetFocus(hRDListView);

		return TRUE;
	}

	if (Msg == WM_CLOSE) {
		RomDataManagerExit();
		EndDialog(hDlg, 0);
	}

	if (Msg == WM_CTLCOLORSTATIC) {
		if ((HWND)lParam == GetDlgItem(hRDMgrWnd, IDC_ROMDATA_LABELGAME))     return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hRDMgrWnd, IDC_ROMDATA_LABELDRIVER))   return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hRDMgrWnd, IDC_ROMDATA_LABELHARDWARE)) return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTGAME))      return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDRIVER))    return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTHARDWARE))  return (INT_PTR)hWhiteBGBrush;
	}

	if (Msg == WM_NOTIFY) {
		NMLISTVIEW* pNMLV = (NMLISTVIEW*)lParam;

		// Dat selected
		if (pNMLV->hdr.code == LVN_ITEMCHANGED && pNMLV->hdr.idFrom == IDC_ROMDATA_LIST) {
			INT32 nCount    = SendMessage(hRDListView, LVM_GETITEMCOUNT,     0, 0);
			INT32 nSelCount = SendMessage(hRDListView, LVM_GETSELECTEDCOUNT, 0, 0);

			if (0 == nCount || 0 == nSelCount) return 1;

			TCHAR szSelDat[MAX_PATH] = { 0 };

			nSelItem = pNMLV->iItem;

			LVITEM LvItem;
			memset(&LvItem, 0, sizeof(LvItem));

			LvItem.iItem      = nSelItem;
			LvItem.mask       = LVIF_TEXT;
			LvItem.iSubItem   = 0;							// Dat Path column
			LvItem.pszText    = szSelDat;
			LvItem.cchTextMax = sizeof(szSelDat);

			SendMessage(hRDListView, LVM_GETITEMTEXT, (WPARAM)nSelItem, (LPARAM)&LvItem);

			SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTGAME),     _T(""));
			SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDRIVER),   _T(""));
			SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTHARDWARE), _T(""));

			DatListInfo* pDatListInfo = RomdataGetListInfo(szSelDat);
			if (NULL != pDatListInfo) {
				SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTGAME),     pDatListInfo->szFullName);
				SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDRIVER),   pDatListInfo->szDrvName);
				SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTHARDWARE), pDatListInfo->szHardware);

				_stprintf(szCover, _T("%s%s.png"), szAppTitlesPath, pDatListInfo->szRomSet);
				if (!FileExists(szCover))
					_stprintf(szCover, _T("%s%s.png"), szAppTitlesPath, pDatListInfo->szDrvName);
				if (!FileExists(szCover))
					szCover[0] = 0;

				RomDataShowPreview(hRDMgrWnd, szCover, IDC_ROMDATA_TITLE, IDC_ROMDATA_TITLE_FRAME, 0, 0);

				_stprintf(szCover, _T("%s%s.png"), szAppPreviewsPath, pDatListInfo->szRomSet);
				if (!FileExists(szCover))
					_stprintf(szCover, _T("%s%s.png"), szAppPreviewsPath, pDatListInfo->szDrvName);
				if (!FileExists(szCover))
					szCover[0] = 0;

				RomDataShowPreview(hRDMgrWnd, szCover, IDC_ROMDATA_PREVIEW, IDC_ROMDATA_PREVIEW_FRAME, 0, 0);

				free(pDatListInfo); pDatListInfo = NULL;
			}
		}

		// Sort Change
		if (pNMLV->hdr.code == LVN_COLUMNCLICK && pNMLV->hdr.idFrom == IDC_ROMDATA_LIST) {
			sort_direction ^= 1;
			ListViewSort(sort_direction, pNMLV->iSubItem);
		}

		// Double Click
		if (pNMLV->hdr.code == NM_DBLCLK && pNMLV->hdr.idFrom == IDC_ROMDATA_LIST) {
			if (nSelItem >= 0) {
				LVITEM LvItem;
				memset(&LvItem,       0, sizeof(LvItem));
				memset(szRomdataName, 0, sizeof(szRomdataName));

				LvItem.iItem      = nSelItem;
				LvItem.mask       = LVIF_TEXT;
				LvItem.iSubItem   = 0;							// Dat path column
				LvItem.pszText    = szRomdataName;
				LvItem.cchTextMax = sizeof(szRomdataName);

				SendMessage(hRDListView, LVM_GETITEMTEXT, (WPARAM)nSelItem, (LPARAM)&LvItem);

				char* szDrvName = RomdataGetDrvName();
				INT32 nGame     = BurnDrvGetIndex(szDrvName);

				RomDataManagerExit();
				EndDialog(hDlg, 0);

				DrvInit(nGame, true);	// Init the game driver
				MenuEnableItems();
				bAltPause = 0;
				if (bVidAutoSwitchFull) {
					nVidFullscreen = 1;
					POST_INITIALISE_MESSAGE;
				}
				POST_INITIALISE_MESSAGE;
			}
		}
	}

	if (Msg == WM_COMMAND) {
		if (HIWORD(wParam) == STN_CLICKED) {
			INT32 nCtrlID = LOWORD(wParam);

			if (nCtrlID == IDC_ROMDATA_TITLE) {
				if (nSelItem >= 0) {
					TCHAR szSelDat[MAX_PATH] = { 0 };
					LVITEM LvItem;
					memset(&LvItem, 0, sizeof(LvItem));

					LvItem.iItem = nSelItem;
					LvItem.mask = LVIF_TEXT;
					LvItem.iSubItem = 0;							// Dat path column
					LvItem.pszText = szSelDat;
					LvItem.cchTextMax = sizeof(szSelDat);

					SendMessage(hRDListView, LVM_GETITEMTEXT, (WPARAM)nSelItem, (LPARAM)&LvItem);

					DatListInfo* pDatListInfo = RomdataGetListInfo(szSelDat);
					if (NULL != pDatListInfo) {
						_stprintf(szCover, _T("%s%s.png"), szAppTitlesPath, pDatListInfo->szRomSet);
						if (!FileExists(szCover))
							_stprintf(szCover, _T("%s%s.png"), szAppTitlesPath, pDatListInfo->szDrvName);
						if (!FileExists(szCover)) {
							szCover[0] = 0;
							free(pDatListInfo); pDatListInfo = NULL;

							return 0;
						}

						free(pDatListInfo); pDatListInfo = NULL;
					}
					RomdataCoverInit();
				}
				return 0;
			}

			if (nCtrlID == IDC_ROMDATA_PREVIEW) {
				if (nSelItem >= 0) {
					TCHAR szSelDat[MAX_PATH] = { 0 };
					LVITEM LvItem;
					memset(&LvItem, 0, sizeof(LvItem));

					LvItem.iItem = nSelItem;
					LvItem.mask = LVIF_TEXT;
					LvItem.iSubItem = 0;							// Dat path column
					LvItem.pszText = szSelDat;
					LvItem.cchTextMax = sizeof(szSelDat);

					SendMessage(hRDListView, LVM_GETITEMTEXT, (WPARAM)nSelItem, (LPARAM)&LvItem);

					DatListInfo* pDatListInfo = RomdataGetListInfo(szSelDat);
					if (NULL != pDatListInfo) {
						_stprintf(szCover, _T("%s%s.png"), szAppPreviewsPath, pDatListInfo->szRomSet);
						if (!FileExists(szCover))
							_stprintf(szCover, _T("%s%s.png"), szAppPreviewsPath, pDatListInfo->szDrvName);
						if (!FileExists(szCover)) {
							szCover[0] = 0;
							free(pDatListInfo); pDatListInfo = NULL;

							return 0;
						}
						free(pDatListInfo); pDatListInfo = NULL;
					}

					RomdataCoverInit();
				}
				return 0;
			}
		}

		if (LOWORD(wParam) == WM_DESTROY) {
			SendMessage(hDlg, WM_CLOSE, 0, 0);
		}

		if (HIWORD(wParam) == BN_CLICKED) {
			INT32 nCtrlID = LOWORD(wParam);

			switch (nCtrlID) {
				case IDC_ROMDATA_PLAY_BUTTON:
				{
					if (nSelItem >= 0) {
						LVITEM LvItem;
						memset(&LvItem, 0, sizeof(LvItem));
						memset(szRomdataName, 0, sizeof(szRomdataName));

						LvItem.iItem      = nSelItem;
						LvItem.mask       = LVIF_TEXT;
						LvItem.iSubItem   = 0;							// Dat path column
						LvItem.pszText    = szRomdataName;
						LvItem.cchTextMax = sizeof(szRomdataName);

						SendMessage(hRDListView, LVM_GETITEMTEXT, (WPARAM)nSelItem, (LPARAM)&LvItem);

						char* szDrvName = RomdataGetDrvName();
						INT32 nGame     = BurnDrvGetIndex(szDrvName);

						RomDataManagerExit();
						EndDialog(hDlg, 0);

						DrvInit(nGame, true);	// Init the game driver
						MenuEnableItems();
						bAltPause = 0;
						if (bVidAutoSwitchFull) {
							nVidFullscreen = 1;
							POST_INITIALISE_MESSAGE;
						}
						POST_INITIALISE_MESSAGE;
					}
					break;
				}

				case IDC_ROMDATA_SCAN_BUTTON: {
					RomDataClearList();
					RomdataListFindDats(szAppRomdataPath);
					SetFocus(hRDListView);
					break;
				}

				case IDC_ROMDATA_SEL_DIR_BUTTON: {
					RomDataClearList();
					SupportDirCreateTab(IDC_SUPPORTDIR_EDIT25, hRDMgrWnd);
					RomdataListFindDats(szAppRomdataPath);
					SetFocus(hRDListView);
					break;
				}

				case IDC_ROMDATA_SSUBDIR_CHECK: {
					bRDListScanSub = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_ROMDATA_SSUBDIR_CHECK));
					RomDataClearList();
					RomdataListFindDats(szAppRomdataPath);
					SetFocus(hRDListView);
					break;
				}

				case IDC_ROMDATA_CANCEL_BUTTON: {
					RomDataManagerExit();
					EndDialog(hDlg, 0);
					break;
				}
			}
		}
	}
	return 0;
}

INT32 RomDataManagerInit()
{
	return FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_ROMDATA_MANAGER), hScrnWnd, (DLGPROC)RomDataManagerProc);
}
