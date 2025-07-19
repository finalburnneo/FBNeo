// Burner Zip module
#include "burner.h"

INT32 nBzipError = 0;												// non-zero if there is a problem with the opened romset

static TCHAR* szBzipName[BZIP_MAX] = { NULL, };					// Zip files to search through

struct RomFind { INT32 nState; INT32 nZip; INT32 nPos; };				// State is non-zero if found. 1 = found totally okay.
static struct RomFind* RomFind = NULL;
static INT32 nRomCount = 0; static INT32 nTotalSize = 0;
static struct ZipEntry* List = NULL; static INT32 nListCount = 0;	// List of entries for current zip file
static INT32 nCurrentZip = -1;									// Zip which is currently open
struct SubDirsZip { TCHAR** pszZipName; UINT32 nCount; };		// Zip files in subdirectories
static struct SubDirsZip _SubDirsZip;

// ----------------------------------------------------------------------------

static void BzipListFree()
{
	if (List) {
		for (INT32 i = 0; i < nListCount; i++) {
			if (List[i].szName) {
				free(List[i].szName);
				List[i].szName = NULL;
			}
		}
		free(List);
		List = NULL;
	}

	nListCount = 0;
}

static void SubDirsListFree()
{
	if (NULL != _SubDirsZip.pszZipName) {
		for (INT32 i = 0; i < _SubDirsZip.nCount; i++) {
			if (NULL != _SubDirsZip.pszZipName[i]) {
				free(_SubDirsZip.pszZipName[i]);
				_SubDirsZip.pszZipName[i] = NULL;
			}
		}
		free(_SubDirsZip.pszZipName);
		_SubDirsZip.pszZipName = NULL;
		_SubDirsZip.nCount = 0;
	}
}

static char* GetFilenameA(char* szFull)
{
	INT32 nLen = strlen(szFull);

	if (nLen <= 0) {
		return szFull;
	}
	for (INT32 i = nLen - 1; i >= 0; i--) {
		if (szFull[i] == '\\' || szFull[i] == '/') {
			return szFull + i + 1;
		}
	}

	return szFull;
}

static TCHAR* GetFilenameW(TCHAR* szFull)
{
	INT32 nLen = _tcslen(szFull);

	if (nLen <= 0) {
		return szFull;
	}
	for (INT32 i = nLen - 1; i >= 0; i--) {
		if (szFull[i] == _T('\\') || szFull[i] == _T('/')) {
			return szFull + i + 1;
		}
	}

	return szFull;
}

static INT32 FileExists(TCHAR *szName)
{
	return GetFileAttributes(szName) != INVALID_FILE_ATTRIBUTES;
}

static INT32 RomArchiveExists(TCHAR *szName)
{
	TCHAR szFileName[MAX_PATH];
	INT32 ret = 0;

	_stprintf(szFileName, _T("%s.zip"), szName);
	ret = FileExists(szFileName);

	if (ret) return ret;

#ifdef INCLUDE_7Z_SUPPORT
	_stprintf(szFileName, _T("%s.7z"), szName);
	ret = FileExists(szFileName);
#endif

	return ret;
}

static INT32 FindRomByName(TCHAR* szName)
{
	struct ZipEntry* pl;
	INT32 i;

	// Find the rom named szName in the List
	for (i = 0, pl = List; i < nListCount; i++, pl++) {
		TCHAR szCurrentName[MAX_PATH];
		if (pl->szName) {
			if (_tcsicmp(szName, GetFilenameW(ANSIToTCHAR(pl->szName, szCurrentName, MAX_PATH))) == 0) {
				return i;
			}
		}
	}
	return -1;													// couldn't find the rom
}

static INT32 FindRomByCrc(UINT32 nCrc)
{
	struct ZipEntry* pl;
	INT32 i;

	// Find the rom named szName in the List
	for (i = 0, pl = List; i< nListCount; i++, pl++)	{
		if (nCrc == pl->nCrc) {
			return i;
		}
	}

	return -1;													// couldn't find the rom
}

// Find rom number i from the pBzipDriver game
static INT32 FindRom(INT32 i)
{
	struct BurnRomInfo ri;
	INT32 nRet;

	memset(&ri, 0, sizeof(ri));

	nRet = BurnDrvGetRomInfo(&ri, i);
	if (nRet != 0) {											// Failure: no such rom
		return -2;
	}

	if (ri.nCrc) {												// Search by crc first
		nRet = FindRomByCrc(ri.nCrc);
		if (nRet >= 0) {
			return nRet;
		}
	}

	for (INT32 nAka = 0; nAka < 0x10000; nAka++) {				// Failing that, search for possible names
		char *szPossibleName = NULL;

		nRet = BurnDrvGetRomName(&szPossibleName, i, nAka);
		if (nRet) {												// No more rom names
			break;
		}
		nRet = FindRomByName(ANSIToTCHAR(szPossibleName, NULL, 0));
		if (nRet >= 0) {
			return nRet;
		}
	}

	return -1;													// Couldn't find the rom
}

static INT32 RomDescribe(struct BurnRomInfo* pri)
{
	if (nBzipError == 0) {
		nBzipError |= 0x8000;

		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_INVALID));
	}

	FBAPopupAddText(PUF_TEXT_DEFAULT, _T(" ") _T(SEPERATOR_1));
	if (pri->nType & BRF_ESS) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_ESS));
	}
	if (pri->nType & BRF_BIOS) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_BIOS));
	}
	if (pri->nType & BRF_PRG) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_PRG));
	}
	if (pri->nType & BRF_GRA) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_GRA));
	}
	if (pri->nType & BRF_SND) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_SND));
	}
	FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_ROM));

	return 0;
}

static INT32 CheckRomsBoot()
{
	INT32 nRet = 0;

	for (INT32 i = 0; i < nRomCount; i++) {
		struct BurnRomInfo ri;
		INT32 nState;

		memset(&ri, 0, sizeof(ri));
		BurnDrvGetRomInfo(&ri, i);			// Find information about the wanted rom
		nState = RomFind[i].nState;			// Get the state of the rom in the zip file

		if (nState != 1 && ri.nType && ri.nCrc) {
			if (!(ri.nType & BRF_OPT) && !(ri.nType & BRF_NODUMP)) {
				return 1;
			}
			nRet = 2;
		}
	}

	return nRet;
}

static INT32 GetBZipError(INT32 nState)
{
	switch (nState) {
		case 1:								// OK
			return 0x0000;
		case 0:								// Not present
			return 0x0001;
		case 3:								// Incomplete
			return 0x0001;
		default:							// CRC wrong or too large
			return 0x0100;
	}

	return 0x0100;
}

// Check the roms to see if they code, graphics etc are complete
static INT32 CheckRoms()
{
	for (INT32 i = 0; i < nRomCount; i++) {
		struct BurnRomInfo ri;

		memset(&ri, 0, sizeof(ri));
		BurnDrvGetRomInfo(&ri, i);							// Find information about the wanted rom
		if ( /*ri.nCrc &&*/ (ri.nType & BRF_OPT) == 0 && (ri.nType & BRF_NODUMP) == 0) {
			INT32 nState = RomFind[i].nState;				// Get the state of the rom in the zip file
			INT32 nError = GetBZipError(nState);

			if (nState == 0 && ri.nType) {					// (A type of 0 means empty slot - no rom)
				char* szName = "Unknown";
				RomDescribe(&ri);
				BurnDrvGetRomName(&szName, i, 0);
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_NOTFOUND), szName);
			}

			if (nError == 0) {
				nBzipError |= 0x2000;
			}

			if (ri.nType & BRF_ESS) {						// essential rom - without it the game may not run at all
				nBzipError |= nError << 0;
			}
			if (ri.nType & BRF_PRG) {						// rom which contains program information
				nBzipError |= nError << 1;
			}
			if (ri.nType & BRF_GRA) {						// rom which contains graphics information
				nBzipError |= nError << 2;
			}
			if (ri.nType & BRF_SND) {						// rom which contains sound information
				nBzipError |= nError << 3;
			}
		}
	}

	if (nBzipError & 0x0F0F) {
		nBzipError |= 0x4000;
	}

	return 0;
}

// ----------------------------------------------------------------------------

static INT32 __cdecl BzipBurnLoadRom(UINT8* Dest, INT32* pnWrote, INT32 i)
{
#if defined (BUILD_WIN32)
	MSG Msg;
#endif

	struct BurnRomInfo ri;
	INT32 nWantZip = 0;
	TCHAR szText[128];
	char* pszRomName = NULL;
	INT32 nRet = 0;

	if (i < 0 || i >= nRomCount) {
		return 1;
	}

	ri.nLen = 0;
	BurnDrvGetRomInfo(&ri, i);								// Get info

	// show what we're doing
	BurnDrvGetRomName(&pszRomName, i, 0);
	if (pszRomName == NULL) {
		TCHAR szTempName[100];
		_stprintf(szTempName, _T("%s"), FBALoadStringEx(hAppInst, IDS_ERR_UNKNOWN, true));
		sprintf(pszRomName, "%s", TCHARToANSI(szTempName, NULL, 0));
	}

	TCHAR szTempLoading[100];
	_stprintf(szTempLoading, _T("%s"), FBALoadStringEx(hAppInst, IDS_PROGRESS_LOADING_ONLY, true));
	_stprintf(szText, _T("%s"), szTempLoading);

	if (ri.nType & (BRF_PRG | BRF_GRA | BRF_SND | BRF_BIOS)) {
		if (ri.nType & BRF_BIOS) {
			TCHAR szTempBios[100];
			_stprintf(szTempBios, _T("%s"), FBALoadStringEx(hAppInst, IDS_ERR_LOAD_DET_BIOS, true));
			_stprintf(szText + _tcslen(szText), _T(" %s"), szTempBios);
		}
		if (ri.nType & BRF_PRG) {
			TCHAR szTempPrg[100];
			_stprintf(szTempPrg, _T("%s"), FBALoadStringEx(hAppInst, IDS_ERR_LOAD_DET_PRG, true));
			_stprintf(szText + _tcslen(szText), _T(" %s"), szTempPrg);
		}
		if (ri.nType & BRF_GRA) {
			TCHAR szTempGra[100];
			_stprintf(szTempGra, _T("%s"), FBALoadStringEx(hAppInst, IDS_ERR_LOAD_DET_GRA, true));
			_stprintf (szText + _tcslen(szText), _T(" %s"), szTempGra);
		}
		if (ri.nType & BRF_SND) {
			TCHAR szTempSnd[100];
			_stprintf(szTempSnd, _T("%s"), FBALoadStringEx(hAppInst, IDS_ERR_LOAD_DET_SND, true));
			_stprintf (szText + _tcslen(szText), _T(" %s"), szTempSnd);
		}
		_stprintf(szText + _tcslen(szText), _T("(%hs)..."), pszRomName);
	} else {
		_stprintf(szText + _tcslen(szText), _T(" %hs..."),  pszRomName);
	}
	ProgressUpdateBurner(ri.nLen ? 1.0 / ((double)nTotalSize / ri.nLen) : 0, szText, 0);

#if defined (BUILD_WIN32)
	// Check for messages:
	if (bNoPopups == false) {
		while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) {
			DispatchMessage(&Msg);
		}
	}
#endif

	if (RomFind[i].nState == 0) {							// Rom not found in zip at all
		return 1;
	}

	nWantZip = RomFind[i].nZip;								// Which zip file it is in
	if (nCurrentZip != nWantZip) {							// If we haven't got the right zip file currently open
		ZipClose();
		nCurrentZip = -1;

		const TCHAR* pszBzipName = (nWantZip < BZIP_MAX) ? szBzipName[nWantZip] : _SubDirsZip.pszZipName[nWantZip - BZIP_MAX];
		if (ZipOpen(TCHARToANSI(pszBzipName, NULL, 0))) {
			return 1;
		}
		nCurrentZip = nWantZip;
	}

	// Read in file and return how many bytes we read
	if (ZipLoadFile(Dest, ri.nLen, pnWrote, RomFind[i].nPos)) {

		// Error loading from the zip file
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(nRet == 2 ? IDS_ERR_LOAD_DISK_CRC : IDS_ERR_LOAD_DISK), pszRomName, GetFilenameW(szBzipName[nCurrentZip]));
		FBAPopupDisplay(PUF_TYPE_WARNING);

		return 1;
	}

	return 0;
}

INT32 ArchiveNameFindDrv(const TCHAR* pszSelArc)
{
	const UINT32 nOldDrvSel = nBurnDrvActive, nArcNameLen = _tcslen(szAppQuickPath);
	const TCHAR* p = pszSelArc + nArcNameLen, * pszExt = _tcsrchr((TCHAR*)pszSelArc, _T('.'));
	INT32 nDrvIdx = -1;

	TCHAR szArcName[64] = { 0 }, szArcNoExt[MAX_PATH] = { 0 };
	_tcsncpy(szArcName,  p,         pszExt - p);
	_tcsncpy(szArcNoExt, pszSelArc, pszExt - pszSelArc);

	for (UINT32 i = 0; (i < nBurnDrvCount) && (nDrvIdx < 0); i++) {
		nBurnDrvActive = i;
		const UINT32 nFlag = BurnDrvGetFlags();

		if (nFlag & BDF_BOARDROM)
			continue;

		for (int z = 0; z < BZIP_MAX; z++) {
			char* szName = NULL;
			if (BurnDrvGetZipName(&szName, z))
				break;

			const TCHAR* pszName = _AtoT(szName);
			if (0 != _tcsicmp(szArcName, pszName))
				continue;

			if (nFlag & BDF_CLONE) {
				const TCHAR* pszParent = BurnDrvGetText(DRV_PARENT);
				if ((NULL != pszParent) && (0 == _tcsicmp(szArcName, pszParent)))
					continue;
			}

			char* pszArcNoExt = _TtoA(szArcNoExt);
			if (0 != ZipOpen(pszArcNoExt))
				continue;	// Make sure nothing is open

			ZipGetList(&List, &nListCount);
			for (INT32 y = 0; 0 == BurnDrvGetRomInfo(NULL, y); y++) {
				if (FindRom(y) >= 0) {
					nDrvIdx = i;
					z = BZIP_MAX;
					break;
				}
			}
			BzipListFree();
			ZipClose();
		}
	}

	nBurnDrvActive = nOldDrvSel;

	if (nDrvIdx < 0) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, _T("Zip / 7z:\n\n"));
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_FILE_CONTENT), pszSelArc);
		FBAPopupDisplay(PUF_TYPE_ERROR);
	}

	return nDrvIdx;
}

// ----------------------------------------------------------------------------

INT32 BzipStatus()
{
	if (!(nBzipError & 0x0F0F)) {
		return BZIP_STATUS_OK;
	}
	if (nBzipError & 1) {
		return BZIP_STATUS_ERROR;
	}

	return BZIP_STATUS_BADDATA;
}

INT32 BzipOpen(bool bootApp)
{
	INT32 nMemLen;														// Zip name number

	nTotalSize = 0;
	nBzipError = 0;

	BzipClose();														// Make sure nothing is open

	// Count the number of roms needed
	for (nRomCount = 0; ; nRomCount++) {
		if (BurnDrvGetRomInfo(NULL, nRomCount)) {
			break;
		}
	}
	if (nRomCount <= 0) {
		return 1;
	}

	// Create an array for holding lookups for each rom -> zip entries
	nMemLen = nRomCount * sizeof(struct RomFind);
	RomFind = (struct RomFind*)malloc(nMemLen);
	if (RomFind == NULL) {
		return 1;
	}
	memset(RomFind, 0, nMemLen);

	for (INT32 y = 0; y < BZIP_MAX; y++) {
		if (szBzipName[y]) {
			free(szBzipName[y]);
			szBzipName[y] = NULL;
		}
	}

	SubDirsListFree();

	const char* pszDrvName = (NULL != pDataRomDesc) ? pRDI->szDrvName : NULL;	// romdata mode, let's pick up the forgotten DrvName ROMs again.
	const INT32 nAppRomPaths = (nQuickOpen > 0) ? DIRS_MAX + 1 : DIRS_MAX;

	// Locate each zip file
	for (INT32 y = 0, z = 0; y < BZIP_MAX && z < BZIP_MAX; y++) {
		char* szName = NULL;
		bool bFound = false;

		if (BurnDrvGetZipName(&szName, y)) {
			break;
		}

		for (INT32 d = 0; d < nAppRomPaths; d++) {	// Traverse the user-configured rom paths
			TCHAR szFullName[MAX_PATH] = { 0 }, szDrvName[MAX_PATH] = { 0 };
			const TCHAR* pszAppRomPaths = (d < DIRS_MAX) ? szAppRomPaths[d] : szAppQuickPath;

			if (nLoadMenuShowY & (1 << 26)) {
				for (UINT32 nCount = 0; nCount < _SubDirInfo[d].nCount; nCount++) {
					// like: c:\1st_dir\2nd_dir\1 + ".zip" + '\0' = 5 chars
					if ((_tcslen(_SubDirInfo[d].SubDirs[nCount]) + strlen(szName)) > (MAX_PATH - 5))
						continue;
					_stprintf(szFullName, _T("%s%hs"), _SubDirInfo[d].SubDirs[nCount], szName);
					if (NULL != pszDrvName) {
						_stprintf(szDrvName, _T("%s%hs"), _SubDirInfo[d].SubDirs[nCount], pszDrvName);
					}

					if (RomArchiveExists(szFullName)) {
						bFound = true;

						TCHAR** newArray = (TCHAR**)realloc(_SubDirsZip.pszZipName, (_SubDirsZip.nCount + 1) * sizeof(TCHAR*));
						_SubDirsZip.pszZipName = newArray;

						_SubDirsZip.pszZipName[_SubDirsZip.nCount] = (TCHAR*)malloc(MAX_PATH * sizeof(TCHAR));
						_tcscpy(_SubDirsZip.pszZipName[_SubDirsZip.nCount], szFullName);

						if (!bootApp) {
							FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_FOUND), szName, _SubDirsZip.pszZipName[_SubDirsZip.nCount]);
						}

						_SubDirsZip.nCount++;
					}
					if (RomArchiveExists(szDrvName)) {
						bFound = true;

						TCHAR** newArray = (TCHAR**)realloc(_SubDirsZip.pszZipName, (_SubDirsZip.nCount + 1) * sizeof(TCHAR*));
						_SubDirsZip.pszZipName = newArray;

						_SubDirsZip.pszZipName[_SubDirsZip.nCount] = (TCHAR*)malloc(MAX_PATH * sizeof(TCHAR));
						_tcscpy(_SubDirsZip.pszZipName[_SubDirsZip.nCount], szDrvName);

						if (!bootApp) {
							FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_FOUND), szName, _SubDirsZip.pszZipName[_SubDirsZip.nCount]);
						}

						_SubDirsZip.nCount++;
					}
				}
			}

			_stprintf(szFullName, _T("%s%hs"), pszAppRomPaths, szName);
			if (NULL != pszDrvName) {
				_stprintf(szDrvName, _T("%s%hs"), pszAppRomPaths, pszDrvName);
			}

			if (RomArchiveExists(szFullName)) {	// Check existence of the rom zip/7z archive file
				bFound = true;

				szBzipName[z] = (TCHAR*)malloc(MAX_PATH * sizeof(TCHAR));
				_tcscpy(szBzipName[z], szFullName);

				if (!bootApp) {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_FOUND), szName, szBzipName[z]);
				}

				z++;
				if (z >= BZIP_MAX) break;
			}
			if (RomArchiveExists(szDrvName)) {	// Append DrvName ROMs
				bFound = true;

				szBzipName[z] = (TCHAR*)malloc(MAX_PATH * sizeof(TCHAR));
				_tcscpy(szBzipName[z], szDrvName);

				if (!bootApp) {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_FOUND), szName, szBzipName[z]);
				}

				z++;
				if (z >= BZIP_MAX) break;
			}
		}

		if (!bootApp && !bFound) {
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_NOTFOUND), szName);
		}
	}

	if (!bootApp) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, _T("\n"));
	}

	const INT32 nAllCount = (nLoadMenuShowY & (1 << 26)) ? BZIP_MAX + _SubDirsZip.nCount : BZIP_MAX;

	// Locate the ROM data in the zip files
	for (INT32 z = 0; z < nAllCount; z++) {
		const TCHAR* pszBzipName = (z < BZIP_MAX) ? szBzipName[z] : _SubDirsZip.pszZipName[z - BZIP_MAX];

		if (pszBzipName == NULL) {
			continue;
		}

		if (ZipOpen(TCHARToANSI(pszBzipName, NULL, 0)) == 0) {		// Open the rom zip file
			nCurrentZip = z;

			ZipGetList(&List, &nListCount);								// Get the list of entries

			for (INT32 i = 0; i < nRomCount; i++) {
				struct BurnRomInfo ri;
				INT32 nFind;

				if (RomFind[i].nState == 1) {							// Already found this and it's okay
					continue;
				}

				memset(&ri, 0, sizeof(ri));

				nFind = FindRom(i);

				if (nFind < 0) {										// Couldn't find this rom at all
					continue;
				}

				RomFind[i].nZip = z;									// Remember which zip file it is in
				RomFind[i].nPos = nFind;
				RomFind[i].nState = 1;									// Set to found okay

				BurnDrvGetRomInfo(&ri, i);								// Get info about the rom

				if ((ri.nType & BRF_OPT) == 0 && (ri.nType & BRF_NODUMP) == 0)	{
					nTotalSize += ri.nLen;
				}

				if (List[nFind].nLen == ri.nLen) {
					if (ri.nCrc) {										// If we know the CRC
						if (List[nFind].nCrc != ri.nCrc) {				// Length okay, but CRC wrong
							RomFind[i].nState = 2;
						}
					}
				} else {
					if (List[nFind].nLen < ri.nLen) {
						RomFind[i].nState = 3;							// Too small
					} else {
						RomFind[i].nState = 4;							// Too big
					}
				}

				if (!bootApp) {
					if (RomFind[i].nState != 1) {
						RomDescribe(&ri);

						if (RomFind[i].nState == 2) {
							FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_CRC), GetFilenameA(List[nFind].szName), List[nFind].nCrc, ri.nCrc);
						}
						if (RomFind[i].nState == 3) {
							FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_SMALL), GetFilenameA(List[nFind].szName), List[nFind].nLen >> 10, ri.nLen >> 10);
						}
						if (RomFind[i].nState == 4) {
							FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_LARGE), GetFilenameA(List[nFind].szName), List[nFind].nLen >> 10, ri.nLen >> 10);
						}
					}
				}
			}

			BzipListFree();
		}

		ZipClose();														// Close the last zip file if open
		nCurrentZip = -1;
	}

	if (!bootApp) {
		// Check the roms to see if the code, graphics etc are complete
		CheckRoms();

		if (nBzipError & 0x2000) {
			if (!(nBzipError & 0x0F0F)) {
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_OK));
			} else {
				FBAPopupAddText(PUF_TEXT_DEFAULT, _T("\n"));
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_PROBLEM));
			}

			if (nBzipError & 0x0101) {
				FBAPopupAddText(PUF_TEXT_DEFAULT, _T("\n ") _T(SEPERATOR_1));
				if (nBzipError & 0x0001) {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_ESS_MISS));
				} else {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_ESS_BAD));
				}
			}
			if (nBzipError & 0x0202) {
				FBAPopupAddText(PUF_TEXT_DEFAULT, _T("\n ") _T(SEPERATOR_1));
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_PRG));
				if (nBzipError & 0x0002) {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DATA_MISS));
				} else {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DATA_BAD));
				}
			}
			if (nBzipError & 0x0404) {
				FBAPopupAddText(PUF_TEXT_DEFAULT, _T("\n ") _T(SEPERATOR_1));
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_GRA));
				if (nBzipError & 0x0004) {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DATA_MISS));
				} else {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DATA_BAD));
				}
			}
			if (nBzipError & 0x0808) {
				FBAPopupAddText(PUF_TEXT_DEFAULT, _T("\n ") _T(SEPERATOR_1));
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_SND));
				if (nBzipError & 0x0008) {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DATA_MISS));
				} else {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DATA_BAD));
				}
			}

			// Catch non-categorised ROMs
			if ((nBzipError & 0x0F0F) == 0) {
				if (nBzipError & 0x0010) {
					FBAPopupAddText(PUF_TEXT_DEFAULT, _T("\n ") _T(SEPERATOR_1));
					if (nBzipError & 0x1000) {
						FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DATA_MISS));
					} else {
						FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DATA_BAD));
					}
				}
			}
		} else {
			FBAPopupAddText(PUF_TEXT_DEFAULT, _T("\n"));
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_NODATA));
		}

		// check for hard drive images (assumed max one per game)
		char *szHDDNameTmp = NULL;
		BurnDrvGetHDDName(&szHDDNameTmp, 0, 0);

		if (szHDDNameTmp) {
			char *szHddFolderName = NULL;

			if (BurnDrvGetTextA(DRV_PARENT)) {
				szHddFolderName = BurnDrvGetTextA(DRV_PARENT);
			} else {
				szHddFolderName = BurnDrvGetTextA(DRV_NAME);
			}

			char szHDDPath[MAX_PATH];
			sprintf(szHDDPath, "%s%s/%s", _TtoA(szAppHDDPath), szHddFolderName, szHDDNameTmp);

			FILE *test = fopen(szHDDPath, "rb");
			if (test) {
				fclose(test);
			} else {
				FBAPopupAddText(PUF_TEXT_DEFAULT, _T("\n"));
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_NOHDDDATA));

				nBzipError = 1;
			}
		}

		BurnExtLoadRom = BzipBurnLoadRom;								// Okay to call our function to load each rom

	} else {
		// check for hard drive images (assumed max one per game)
		char *szHDDNameTmp = NULL;
		BurnDrvGetHDDName(&szHDDNameTmp, 0, 0);

		if (szHDDNameTmp) {
			char *szHddFolderName = NULL;

			if (BurnDrvGetTextA(DRV_PARENT)) {
				szHddFolderName = BurnDrvGetTextA(DRV_PARENT);
			} else {
				szHddFolderName = BurnDrvGetTextA(DRV_NAME);
			}

			char szHDDPath[MAX_PATH];
			sprintf(szHDDPath, "%s%s/%s", _TtoA(szAppHDDPath), szHddFolderName, szHDDNameTmp);

			FILE *test = fopen(szHDDPath, "rb");
			if (test) {
				fclose(test);
			} else {
				return 1;
			}
		}

		return CheckRomsBoot();
	}

	return 0;
}

INT32 BzipClose()
{
	ZipClose();
	nCurrentZip = -1;													// Close the last zip file if open

	BurnExtLoadRom = NULL;												// Can't call our function to load each rom anymore
	nBzipError = 0;														// reset romset errors

	if (RomFind) {
		free(RomFind);
		RomFind = NULL;
	}
	nRomCount = 0;

	for (INT32 z = 0; z < BZIP_MAX; z++) {
		if (szBzipName[z]) {
			free(szBzipName[z]);
			szBzipName[z] = NULL;
		}
	}

	SubDirsListFree();

	return 0;
}
