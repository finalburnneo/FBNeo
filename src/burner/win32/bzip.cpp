// Burner Zip module
#include "burner.h"
#include "burnint.h"

// State is non-zero if found. 1 = found totally okay
struct RomFind {
	INT32 nState;							// 0 = not found, 1 = found and valid, 2 = CRC mismatch, 3 = size too small, 4 = size too large
	INT32 nZip;								// Index of ZIP file where ROM was found (if nState is non-zero)
	INT32 nPos;								// Position/index of the ROM within the ZIP file (if nState is non-zero)
};

// ROM statistics: tracks counts and sizes of ROMs during scanning/loading
struct RomStatistics {
	INT32 nRomCount;						// Final count of valid ROMs
	INT32 nTotalSize;						// Total size of all required ROMs
	INT32 nValidRom;						// Temp counter for valid ROMs during scan
	INT32 nValidRomCount;					// Final count of valid ROMs found in ZIP files
};

// Archive tracking info: stores driver-defined archive names and found status
struct ArchiveTrackInfo {
	char* arcNames[BZIP_MAX];				// Archive filenames required by the driver
	bool  arcFound[BZIP_MAX];				// True = corresponding archive was found during scan
	INT32 arcCount;							// Total number of archives for current driver
};

// Scan context: global state for entire ROM scanning/loading process
struct ScanContext {
	// ZIP file management
	TCHAR* szBzipName[BZIP_MAX];			// Path buffers for open ZIP files
	char*  pszDrvName;						// Driver name (for RomData module)
	INT32* pZipIdx;							// Pointer to current ZIP index
	INT32  nCurrentZip;						// Index of currently active ZIP
	INT32  nBzipError;						// non-zero if there is a problem with the opened romset

	struct ZipEntry* List;					// Array of ROM entries from the currently opened ZIP file
	INT32  nListCount;						// Count of entries in the List array

	// Driver & mode flags
	bool bootApp;							// True = direct boot (no UI popups)
	bool bAllRomFound;						// Flag: true = all required ROMs are found

	// ROM metadata cache
	struct BurnRomInfo* pRomCache;			// Cached ROM info from driver
	struct RomFind* pRomFind;				// Per-ROM scan result tracking

	// ROM statistics
	struct RomStatistics* pRomStats;		// ROM statistics tracking

	// Nested archive tracking
	struct ArchiveTrackInfo* archiveTrack;	// Driver-defined archive tracking
};

#if defined(__cplusplus) && __cplusplus >= 201103L
# define THREAD_LOCAL thread_local
#elif defined(_MSC_VER)
# define THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
# define THREAD_LOCAL __thread
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define THREAD_LOCAL _Thread_local
#else
# error "Unsupported compiler for thread local storage"
#endif

// Thread-local scan context to maintain state across recursive calls and support multi-threading
static THREAD_LOCAL struct ScanContext ScanCtx;

// ----------------------------------------------------------------------------

// Release all memory allocated for Bzip list
static void BzipListFree()
{
	struct ZipEntry*& List = ScanCtx.List;
	INT32& nListCount      = ScanCtx.nListCount;

	if (!List)
		return;

	// Free each name string in the list
	for (INT32 i = 0; i < nListCount; i++)
		free_s((void**)&List[i].szName);

	// Free the list array and reset pointer
	free_s((void**)&List);

	// Reset list count
	nListCount = 0;
}

// Extracts the filename from a full path (ANSI version)
static inline const char* GetFilenameA(const char* szFull)
{
	// Check for NULL input pointer
	if (IsStrEmptyA(szFull))
		return NULL;

	const char* pLastSep = NULL;

	// Scan string forward to find last path separator
	for (const char* p = szFull; '\0' != *p; p++) {
		if ('\\' == *p || '/' == *p)
			pLastSep = p;
	}

	// Return filename or original string if no separator found
	return pLastSep ? (pLastSep + 1) : szFull;
}

// Extracts the filename from a full path (TCHAR version)
static inline const TCHAR* GetFilename(const TCHAR* szFull)
{
	// Check for NULL input pointer
	if (IsStrEmpty(szFull))
		return NULL;

	const TCHAR* pLastSep = NULL;

	// Scan string forward to find last path separator
	for (const TCHAR* p = szFull; _T('\0') != *p; p++) {
		if (_T('\\') == *p || _T('/') == *p)
			pLastSep = p;
	}

	// Return filename or original string if no separator found
	return pLastSep ? (pLastSep + 1) : szFull;
}

// Check if a file exists and is not a directory
static inline INT32 FileExists(const TCHAR* pszName)
{
	// Fast path: empty string is not a valid file, avoid expensive kernel call
	if (IsStrEmpty(pszName))
		return 0;

	DWORD dwAttrib = GetFileAttributes(pszName);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

// Check if a ROM archive with the given base name exists (either .zip or .7z)
static INT32 RomArchiveExists(TCHAR *pszName)
{
	TCHAR szFileName[MAX_PATH] = { 0 };
	INT32 ret = 0;

	_sntprintf(szFileName, MAX_PATH, _T("%s.zip"), pszName);
	szFileName[MAX_PATH - 1] = _T('\0');
	ret = FileExists(szFileName);

	if (ret) return ret;

#ifdef INCLUDE_7Z_SUPPORT
	_sntprintf(szFileName, MAX_PATH, _T("%s.7z"),  pszName);
	szFileName[MAX_PATH - 1] = _T('\0');
	ret = FileExists(szFileName);
#endif

	return ret;
}

// Find ROM index by name in the currently opened ZIP file's List
static inline INT32 FindRomByName(const char* szName)
{
	const struct ZipEntry* List = ScanCtx.List;
	const INT32 nListCount      = ScanCtx.nListCount;
	INT32 i;

	// Find the rom named szName in the List
	for (i = 0; i < nListCount; i++, List++) {
		if (List->szName) {
			if (0 == stricmp(szName, List->szName))
				return i;
		}
	}

	return -1;													// couldn't find the rom
}

// Find ROM index by CRC in the currently opened ZIP file's List
static inline INT32 FindRomByCrc(const UINT32 nCrc)
{
	const struct ZipEntry* List = ScanCtx.List;
	const INT32 nListCount      = ScanCtx.nListCount;
	INT32 i;

	// Find the rom named szName in the List
	for (i = 0; i < nListCount; i++, List++) {
		if (nCrc == List->nCrc)
			return i;
	}

	return -1;													// couldn't find the rom
}

// Unified ROM finding function: tries CRC first, then possible names (if CRC is not available)
static INT32 FindRom(const INT32 i, const struct BurnRomInfo* pRomCache)
{
	// Validate input
	if (!pRomCache || 0 > i)
		return -1;

	const struct BurnRomInfo* pri = &pRomCache[i];

	// Fast path: search by CRC
	if (0 != pri->nCrc) {
		const INT32 nRet = FindRomByCrc(pri->nCrc);
		if (0 <= nRet)
			return nRet;
	}

	// Search by name aliases
	const INT32 MAX_ALIASES = 32;
	char* pszRomName = NULL;

	// Realistically max 8-10 aliases should be enough, but we can allow up to 32 just in case
	for (INT32 nAka = 0; nAka < MAX_ALIASES; nAka++) {
		if (0 != BurnDrvGetRomName(&pszRomName, i, nAka))
			break;

		if (IsStrEmptyA(pszRomName))
			continue;

		const INT32 nRet = FindRomByName(pszRomName);
		if (nRet >= 0)
			return nRet;
	}

	return -1;
}

// Describe ROM type and error in UI mode (only shows if there is a problem with the ROM)
static INT32 RomDescribe(const struct BurnRomInfo* pri)
{
	if (!pri)
		return -1;

	const UINT32 nType = pri->nType;
	INT32& nBzipError  = ScanCtx.nBzipError;

	if (0 == nBzipError) {
		nBzipError |= 0x8000;
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_INVALID));
	}

	FBAPopupAddText(PUF_TEXT_DEFAULT, _T(" ") _T(SEPERATOR_1));
	if (nType & BRF_ESS) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_ESS));
	}
	if (nType & BRF_BIOS) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_BIOS));
	}
	if (nType & BRF_PRG) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_PRG));
	}
	if (nType & BRF_GRA) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_GRA));
	}
	if (nType & BRF_SND) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_SND));
	}
	FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DET_ROM));

	return 0;
}

// Validates ROM integrity within a single open archive
static void VerifySingleArchive(const TCHAR* noextArc, struct RomFind* pRomFind, struct BurnRomInfo* pRomCache, const INT32 nCount)
{
	// Safety check
	if (IsStrEmpty(noextArc) || !pRomFind || !pRomCache || nCount <= 0)
		return;

	bool   bAddZip         = false;
	INT32& nIndex          = *ScanCtx.pZipIdx;
	INT32& nCurrentZip     = ScanCtx.nCurrentZip;
	struct ZipEntry*& List = ScanCtx.List;
	INT32& nListCount      = ScanCtx.nListCount;

	char* pszAnsiPath = NULL;
	if (tchar_to_ansi(noextArc, &pszAnsiPath) <= 0)
		return;

	if (0 == ZipOpen(pszAnsiPath)) {
		ZipGetList(&List, &nListCount);
		for (INT32 i = 0; i < nCount; i++) {
			INT32& nState = pRomFind[i].nState;
			const struct BurnRomInfo* pri = &pRomCache[i];
			// Skip already valid ROMs, optional ROMs, no-dump ROMs, and empty slots
			if (1 == nState || (pri->nType & (BRF_OPT | BRF_NODUMP)) || !pri->nType)
				continue;

			INT32 nFind = FindRom(i, pRomCache);
			// ROM not found in this archive
			if (nFind < 0)
				continue;

			const UINT32 nListLen = List[nFind].nLen;
			const UINT32 nListCrc = List[nFind].nCrc;

			// Accumulate total size for non-optional, non-dump ROMs
			ScanCtx.pRomStats->nTotalSize += pri->nLen;

			// Validate size and crc, and set state accordingly
			if (nListLen != pri->nLen) {
				nState = (nListLen < pri->nLen) ? 3 : 4;		// Size mismatch
			} else if (pri->nCrc && nListCrc != pri->nCrc) {
				nState = 2;										// CRC mismatch
			} else {
				// Add archive to list only once
				if (!bAddZip) {
					TCHAR* pCopy = _tcsdup(noextArc);
					if (!pCopy)
						continue;								// Memory allocation failed, skip adding this archive
					if (nIndex >= BZIP_MAX) {
						free_s((void**)&pCopy);
						continue;
					}

					ScanCtx.szBzipName[nIndex] = pCopy;
					nCurrentZip = nIndex++;						// Increment after assignment to ensure index is correct for next addition
					bAddZip = true;								// Mark that we've added this archive to the list
				}

				// Save ROM location and mark as valid
				pRomFind[i].nZip = nCurrentZip;
				pRomFind[i].nPos = nFind;
				nState = 1;

				ScanCtx.pRomStats->nValidRom++;					// Valid ROM found, increment count
			}

			// Show error messages in UI mode
			if (!ScanCtx.bootApp && 1 != nState) {
				const char* pszName = GetFilenameA(List[nFind].szName);
				RomDescribe(pri);
				if (     2 == nState)
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_CRC),   pszName, nListCrc,       pri->nCrc);
				else if (3 == nState)
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_SMALL), pszName, nListLen >> 10, pri->nLen >> 10);
				else if (4 == nState)
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_LARGE), pszName, nListLen >> 10, pri->nLen >> 10);
			}
		}
		BzipListFree();
		ZipClose();
	}

	free_s((void**)&pszAnsiPath);
}

// Fast ROM validation for direct boot mode
// Returns: 0 = OK, 1 = critical error, 2 = warning (optional ROM missing)
static INT32 CheckRomsBoot()
{
	INT32 nRet = 0;

	const INT32 nRomCount         = ScanCtx.pRomStats->nRomCount;
	const struct BurnRomInfo* pri = ScanCtx.pRomCache;
	const struct RomFind* pFind   = ScanCtx.pRomFind;

	for (INT32 i = 0; i < nRomCount; i++, pri++, pFind++) {
		// Skip ROMs with no type defined
		if (!pri->nType)
			continue;

		// Check for missing ROM (not found in archive)
		if (1 != pFind->nState && pri->nCrc) {
			// If this is a REQUIRED ROM (not optional/nodump), boot fails immediately
			if (0 == (pri->nType & (BRF_OPT | BRF_NODUMP)))
				return 1;

			// Missing optional ROM: set warning flag
			nRet = 2;
		}
	}

	return nRet;
}

// Convert ROM load state to corresponding error flag
static inline INT32 GetBZipError(INT32 nState)
{
	switch (nState) {
		case 1:												// OK
			return 0x0000;
		case 0:												// Not present
			return 0x0001;
		case 3:												// Incomplete
			return 0x0001;
		default:											// CRC wrong or too large
			return 0x0100;
	}
}

// Check the roms to see if they code, graphics etc are complete
static INT32 CheckRoms()
{
	INT32& nBzipError = ScanCtx.nBzipError;
	const struct BurnRomInfo* pri  = ScanCtx.pRomCache;
	const struct RomFind* pFind    = ScanCtx.pRomFind;

	for (INT32 i = 0; i < ScanCtx.pRomStats->nRomCount; i++, pri++, pFind++) {
		if ( /*pri->nCrc &&*/ 0 == (pri->nType & (BRF_OPT | BRF_NODUMP))) {
			const INT32 nState = pFind->nState;				// Get the state of the rom in the zip file
			const INT32 nError = GetBZipError(nState);

			if (0 == nState && pri->nType) {				// (A type of 0 means empty slot - no rom)
				char* pszName = pri->szName;
				if (IsStrEmptyA(pszName))
					pszName = "unknown";					// Fallback name if conversion failed or name is empty (0%)

				RomDescribe(pri);
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_NOTFOUND), pszName);
			}

			if (0 == nError) {
				nBzipError |= 0x2000;
			}

			if (pri->nType & BRF_ESS) {						// essential rom - without it the game may not run at all
				nBzipError |= nError << 0;
			}
			if (pri->nType & BRF_PRG) {						// rom which contains program information
				nBzipError |= nError << 1;
			}
			if (pri->nType & BRF_GRA) {						// rom which contains graphics information
				nBzipError |= nError << 2;
			}
			if (pri->nType & BRF_SND) {						// rom which contains sound information
				nBzipError |= nError << 3;
			}
		}
	}

	if (nBzipError & 0x0F0F) {
		nBzipError |= 0x4000;
	}

	return 0;
}

// Callback: Scan a single directory for ROM archives
static INT32 ScanDirectoryROMs(const TCHAR* dirPath)
{
	// Early exit if all ROMs are already found
	if (ScanCtx.bAllRomFound)
		return 0;

	TCHAR szDrvPath[MAX_PATH] = { 0 };
	TCHAR** pBzipArray      = ScanCtx.szBzipName;
	const INT32 nIndex      = *ScanCtx.pZipIdx;
	const char* pszDrvName  = ScanCtx.pszDrvName;
	bool  bootApp           = ScanCtx.bootApp;
	INT32& nRomCount        = ScanCtx.pRomStats->nRomCount;
	struct RomFind* RomFind = ScanCtx.pRomFind;
	struct BurnRomInfo* pRomCache   = ScanCtx.pRomCache;
	struct ArchiveTrackInfo* pTrack = ScanCtx.archiveTrack;

	bool bMatched = false;

	// Iterate all required ROM names
	for (INT32 i = 0; i < pTrack->arcCount; i++) {
		if (nIndex >= BZIP_MAX)
			goto CHECK_DONE;
		if (!pTrack->arcNames[i])
			continue;
		if (!bMatched && pszDrvName && 0 == stricmp(pTrack->arcNames[i], pszDrvName))
			// The archive resides within the driver, indicating that the RomData driver originates from a parent set (logical duplication);
			// accordingly, no additional Clone sets for search traceability should be added.
			bMatched = true;

		TCHAR szArcName[MAX_PATH] = { 0 };
		_sntprintf(szArcName, MAX_PATH, _T("%s%hs"), dirPath, pTrack->arcNames[i]);
		szArcName[MAX_PATH - 1] = _T('\0');

		bool bFound = false;
		// Skip duplicate archives: avoid reprocessing the same file if duplicate scan paths were specified
		for (INT32 j = 0; j < nIndex; j++) {
			if (0 == _tcscmp(pBzipArray[j], szArcName)) {
				bFound = true;
				break;
			}
		}

		// Check if main ROM file exists
		if (!bFound && RomArchiveExists(szArcName)) {
			// Validate ROMs in this archive
			VerifySingleArchive(szArcName, RomFind, pRomCache, nRomCount);

			// Log found file (skip in boot mode)
			if (!bootApp) {
				pTrack->arcFound[i] = true;
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_FOUND), pTrack->arcNames[i], szArcName);
			}
		}
	}

	// The RomData driver originates from a Clone set; add search coverage for the archive of this Clone se
	if (!bMatched && pszDrvName && nIndex < BZIP_MAX) {
		_sntprintf(szDrvPath, MAX_PATH, _T("%s%hs"), dirPath, pszDrvName);
		szDrvPath[MAX_PATH - 1] = _T('\0');

		// Skip duplicate archives: avoid reprocessing the same file if duplicate scan paths were specified
		for (INT32 i = 0; i < nIndex; i++) {
			if (0 == _tcscmp(pBzipArray[i], szDrvPath))
				goto CHECK_DONE;
		}

		// When the driver of RomData is derived from a Clone instead of a Parent, make the RomData driver acquire all files of the Clone
		// No error alert will pop up if the compressed file cannot be found
		if (szDrvPath && RomArchiveExists(szDrvPath)) {
			// Validate ROMs in this archive
			VerifySingleArchive(szDrvPath, RomFind, pRomCache, nRomCount);

			// Log found file (skip in boot mode)
			if (!bootApp)
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_FOUND), pszDrvName, szDrvPath);

		}
	}

CHECK_DONE:
	// Count the number of valid ROMs that passed verification (state = 1)
	// If found valid ROM count matches the REQUIRED valid ROM count, we're done
	ScanCtx.bAllRomFound = (ScanCtx.pRomStats->nValidRom >= nRomCount);

	// Return -1 to stop directory scanning early, 0 to continue
	return ScanCtx.bAllRomFound ? -1 : 0;
}

// Macro to append ROM type text (eliminates duplicate code)
#define APPEND_ROM_TYPE(szBuffer, bufSize, typeFlag, stringId)									\
	if (pri->nType & typeFlag) {																\
		TCHAR szTemp[100];																		\
		_tcsncpy(szTemp, FBALoadStringEx(hAppInst, stringId, true), ARRAY_SIZE(szTemp) - 1);	\
		szTemp[ARRAY_SIZE(szTemp) - 1] = _T('\0');												\
		_tcsncat(szBuffer, szTemp,  bufSize - _tcslen(szBuffer) - 1);							\
		_tcsncat(szBuffer, _T(" "), bufSize - _tcslen(szBuffer) - 1);							\
	}

// Build progress text and update UI
static void UpdateRomLoadProgress(const struct BurnRomInfo* pri, const char* pszRomName)
{
	TCHAR szText[128] = { 0 };
	size_t bufSize = ARRAY_SIZE(szText);

	// Build progress text: Loading...
	TCHAR szTempLoading[100];
	_tcsncpy(szTempLoading, FBALoadStringEx(hAppInst, IDS_PROGRESS_LOADING_ONLY, true), ARRAY_SIZE(szTempLoading) - 1);
	szTempLoading[ARRAY_SIZE(szTempLoading) - 1] = _T('\0');
	_tcsncpy(szText, szTempLoading, bufSize - 1);
	szText[bufSize - 1] = _T('\0');

	// Append ROM type labels using macro (NO DUPLICATE CODE!)
	APPEND_ROM_TYPE(szText, bufSize, BRF_BIOS, IDS_ERR_LOAD_DET_BIOS);
	APPEND_ROM_TYPE(szText, bufSize, BRF_PRG,  IDS_ERR_LOAD_DET_PRG);
	APPEND_ROM_TYPE(szText, bufSize, BRF_GRA,  IDS_ERR_LOAD_DET_GRA);
	APPEND_ROM_TYPE(szText, bufSize, BRF_SND,  IDS_ERR_LOAD_DET_SND);

	// Choose format string: (ROMNAME) vs  ROMNAME
	const TCHAR* pszFormat = (pri->nType & (BRF_PRG | BRF_GRA | BRF_SND | BRF_BIOS)) ? _T("(%hs)...") : _T(" %hs...");
	// Write ROM name (single call, no duplicate code)
	_sntprintf(szText + _tcslen(szText), bufSize - _tcslen(szText), pszFormat, pszRomName);

	// Cache global value to local / register variable
	const INT32 totalSize = ScanCtx.pRomStats->nTotalSize;
	double dProgress = 0.0;
	if (totalSize > 0 && pri->nLen > 0) {
		dProgress = (double)pri->nLen / totalSize;
	}
	// Update progress bar with new text and calculated progress
	ProgressUpdateBurner(dProgress, szText, false);
}

#undef APPEND_ROM_TYPE

static INT32 __cdecl BzipBurnLoadRom(UINT8* Dest, INT32* pnWrote, INT32 i)
{
	if (!Dest || i < 0 || i >= ScanCtx.pRomStats->nRomCount || !ScanCtx.pRomCache || !ScanCtx.pRomFind)
		return 1;

#if defined (BUILD_WIN32)
	MSG Msg;
#endif

	struct BurnRomInfo* pri = &ScanCtx.pRomCache[i];
	char* pszRomName  = pri->szName;

/*
	if (pszRomName == NULL) {
		TCHAR szTempName[100];
		_stprintf(szTempName, _T("%s"), FBALoadStringEx(hAppInst, IDS_ERR_UNKNOWN, true));
		sprintf(pszRomName, "%s", TCHARToANSI(szTempName, NULL, 0));
	}
*/
	UpdateRomLoadProgress(pri, pszRomName);

#if defined (BUILD_WIN32)
	// Check for messages:
	if (bNoPopups == false) {
		while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) {
			DispatchMessage(&Msg);
		}
	}
#endif

	struct RomFind* pFind = &ScanCtx.pRomFind[i];
	TCHAR** pBzipArray    = ScanCtx.szBzipName;
	INT32& nCurrentZip    = ScanCtx.nCurrentZip;
	INT32  nWantZip       = 0;
	INT32  nRet           = 0;

	// Rom not found in zip at all
	if (0 == pFind->nState && nCurrentZip >= 0 && !IsStrEmpty(pBzipArray[nCurrentZip])) {
		// Error not found in the zip file
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_DISK), pszRomName, GetFilename(pBzipArray[nCurrentZip]));
		FBAPopupAddText(PUF_TEXT_DEFAULT, _T("\n\n"));
		return 1;
	}

	nWantZip = pFind->nZip;												// Which zip file it is in
	if (nCurrentZip != nWantZip) {										// If we haven't got the right zip file currently open
		ZipClose();
		nCurrentZip = -1;

		char* pszBzipName = NULL;
		if (tchar_to_ansi(ScanCtx.szBzipName[nWantZip], &pszBzipName) <= 0)
			return 1;
		if (ZipOpen(pszBzipName)) {
			free_s((void**)&pszBzipName);
			return 1;
		}
		free_s((void**)&pszBzipName);
		nCurrentZip = nWantZip;
	}

	// Read in file and return how many bytes we read
	nRet = ZipLoadFile(Dest, pri->nLen, pnWrote, pFind->nPos);
	if (nRet > 0 && nCurrentZip >= 0 && !IsStrEmpty(pBzipArray[nCurrentZip])) {
		// Error loading from the zip file
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(2 == nRet ? IDS_ERR_LOAD_DISK_CRC : IDS_ERR_LOAD_DISK), pszRomName, GetFilename(pBzipArray[nCurrentZip]));
		FBAPopupDisplay(PUF_TYPE_WARNING);

		return 1;
	}

	return 0;
}

// Cache ROM information from the driver for faster access during scanning/loading
static INT32 CacheROMsInfo(struct BurnRomInfo** pOutRoms, struct RomFind** pOutFind, INT32* pValidCount)
{
	// Validate input pointers
	if (!pOutRoms || !pOutFind || !pValidCount)
		return -1;

	// Initialize output values to default state
	*pOutRoms    = NULL;
	*pOutFind    = NULL;
	*pValidCount = 0;

	// Count total number of ROM entries from the driver
	UINT32 nRomCount = 0;
	while (0 == BurnDrvGetRomInfo(NULL, nRomCount))
		nRomCount++;

	// No ROMs found
	if (0 == nRomCount)
		return -2;

	// Allocate memory for ROM cache and find structures (keep original index alignment)
	struct BurnRomInfo* pRomCache = (struct BurnRomInfo* )calloc(nRomCount, sizeof(struct BurnRomInfo));
	struct RomFind*     pRomFind  = (struct RomFind*     )calloc(nRomCount, sizeof(struct RomFind));

	// Check allocation failure
	if (!pRomCache || !pRomFind) {
		free_s((void**)&pRomCache);
		free_s((void**)&pRomFind);
		return -3;
	}

	INT32  validCount = 0;
	UINT32 i;

	// Fill ROM cache while preserving original driver indices
	for (i = 0; i < nRomCount; i++) {
		struct BurnRomInfo ri;
		if (0 != BurnDrvGetRomInfo(&ri, i))
			break;

		// Skip optional, nodump, and invalid ROM types
		if ((ri.nType & (BRF_OPT | BRF_NODUMP)) || !ri.nType)
			continue;

		char* pRomName = NULL;
		if (0 != BurnDrvGetRomName(&pRomName, i, 0) || !pRomName)
			pRomName = "unknown";										// Probability of current triggering: 0%

		// Preserve original index i (critical for driver compatibility)
		pRomCache[i] = ri;
		pRomCache[i].szName = strdup(pRomName);

		// Handle string duplication failure
		if (!pRomCache[i].szName)
			goto CLEANUP_FAILED;

		validCount++;
	}

	// Verify all ROM entries were enumerated correctly
	if (i != nRomCount)
		goto CLEANUP_FAILED;

	// Output valid results
	*pOutRoms    = pRomCache;
	*pOutFind    = pRomFind;
	*pValidCount = validCount;

	return nRomCount;

	// Unified cleanup for all error paths
CLEANUP_FAILED:
	// Free allocated ROM names
	for (UINT32 k = 0; k < i; k++) {
		free_s((void**)&pRomCache[k].szName);
	}

	// Free main cache structures
	free_s((void**)&pRomCache);
	free_s((void**)&pRomFind);
	return -4;
}

// Allocates and loads driver-defined archive names into a new ArchiveTrackInfo
INT32 PreloadDriverArchiveNames(ArchiveTrackInfo** pArchiveTrack)
{
	// Validate input and set output pointer to NULL upfront (safe practice)
	if (!pArchiveTrack)
		return -1;

	*pArchiveTrack = NULL;

	// Allocate and initialize archive track structure
	ArchiveTrackInfo* pTrack = (ArchiveTrackInfo*)calloc(1, sizeof(ArchiveTrackInfo));
	if (!pTrack)
		return -1;

	// Load all available archive names from the driver
	for (INT32 i = 0; i < BZIP_MAX; i++) {
		// Stop if no more valid archive names
		char* arcName = NULL;
		if (0 != BurnDrvGetZipName(&arcName, i) || IsStrEmptyA(arcName))
			break;

		// Check duplicates against ALL previously loaded entries (j < i)
		bool bDuplicate = false;
		for (INT32 j = 0; j < i; j++) {
			// Only check valid entries that were actually saved
			if (j < pTrack->arcCount && pTrack->arcNames[j] && 0 == strcmp(pTrack->arcNames[j], arcName)) {
				bDuplicate = true;
				break;
			}
		}

		// Skip duplicate archive name
		if (bDuplicate)
			continue;

		// Duplicate archive name string
		char* pCopy = strdup(arcName);
		if (!pCopy) {
			// Cleanup all allocated memory on failure
			for (INT32 k = 0; k < i; k++)
				free_s((void**)&pTrack->arcNames[k]);

			free_s((void**)&pTrack);
			return -1;
		}

		pTrack->arcNames[pTrack->arcCount] = pCopy;
		pTrack->arcCount++;
	}

	// Success: return the loaded archive count
	*pArchiveTrack = pTrack;
	return pTrack->arcCount;
}

INT32 QuickVerifyZip(const char* noextPath, const TCHAR* pszSelArc)
{
	// Extract filename only once (fast path processing)
	char noextArc[100] = { 0 };
	const char* pFileName = GetFilenameA(noextPath);

	size_t nameLen = strlen(pFileName);
	size_t copyLen = (nameLen < 100 - 1) ? nameLen : (100 - 1);
	strncpy(noextArc, pFileName, copyLen);
	noextArc[copyLen] = '\0';

	struct ZipEntry*& List = ScanCtx.List;
	INT32& nListCount      = ScanCtx.nListCount;
	INT32  nDrvIdx         = -1;

	const UINT32 nOldDrvSel = nBurnDrvActive;

	// Single condition for loop -> faster execution
	for (UINT32 i = 0; i < nBurnDrvCount && nDrvIdx < 0; i++) {
		nBurnDrvActive = i;

		char* zipName = NULL;
		// Merge all checks into one line, reduce branch miss
		if (0 != BurnDrvGetZipName(&zipName, 0) || !zipName || 0 != strcmp(noextArc, zipName))
			continue;

		// Open ZIP only when name matches (huge performance gain)
		if (0 != ZipOpen((char*)noextPath))
			continue;

		ZipGetList(&List, &nListCount);
		// Early exit loop when found
		for (INT32 j = 0; j < nListCount && nDrvIdx < 0; j++) {
			struct BurnRomInfo ri = { 0 };
			if (0 != BurnDrvGetRomInfo(&ri, j))
				break;

			char* romName = NULL;
			if (0 != BurnDrvGetRomName(&romName, j, 0) || !romName)
				break;
			if (0 == (ri.nType & (BRF_OPT | BRF_NODUMP)) && 0 != ri.nType) {
				// Fast match CRC or filename
				if ((0 != ri.nCrc && List[j].nCrc == ri.nCrc) || (0 == strcmp(romName, List[j].szName))) {
					nDrvIdx = (INT32)i;
					break;
				}
			}

		}

		// Fast resource release
		BzipListFree();
		ZipClose();
	}

	nBurnDrvActive = nOldDrvSel;

	// Error popup only if not found
	if (nDrvIdx < 0 && pszSelArc) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, _T("Zip / 7z:\n\n"));
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_FILE_CONTENT), pszSelArc);
		FBAPopupDisplay(PUF_TYPE_ERROR);
	}

	return nDrvIdx;
}

// ----------------------------------------------------------------------------
INT32 inline GetBzipStatus()
{
	const INT32 nBzipError = ScanCtx.nBzipError;
	if (!(nBzipError & 0x0F0F)) {
		return BZIP_STATUS_OK;
	}
	if (nBzipError & 1) {
		return BZIP_STATUS_ERROR;
	}

	return BZIP_STATUS_BADDATA;
}

INT32 BzipStatus()
{
	return GetBzipStatus();
}

// Verify if the required HDD image file exists for the current game
// 0 - HDD exists OR no HDD needed
// 1 - HDD is required but missing
static INT32 CheckHDDImageExists()
{
	char* szHDDNameTmp = NULL;

	// Get the HDD filename from the driver
	BurnDrvGetHDDName(&szHDDNameTmp, 0, 0);

	// If no HDD is defined for this game, return OK immediately
	if (!szHDDNameTmp)
		return 0;

	// Determine the root folder name (use parent ROM if available)
	char* szRomFolder = BurnDrvGetTextA(DRV_PARENT);
	if (!szRomFolder)
		szRomFolder   = BurnDrvGetTextA(DRV_NAME);

	// Build full path to the HDD image
	TCHAR szHDDPath[MAX_PATH] = { 0 };
	_sntprintf(szHDDPath, MAX_PATH, _T("%s%s/%hs"), szAppHDDPath, szRomFolder, szHDDNameTmp);

	// Check file existence using Win32 API (project standard)
	if (FileExists(szHDDPath))
		return 0;

	// HDD is required but not found
	return 1;
}

// Show ROM separator line with a newline
#define SHOW_ROM_SEPARATOR()																	\
	FBAPopupAddText(PUF_TEXT_DEFAULT, _T("\n ") _T(SEPERATOR_1))

// Show "Missing" or "Bad" localized message based on condition
#define SHOW_MISSING_OR_BAD(cond, missID, badID)												\
	if (cond)																					\
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(missID));								\
	else																						\
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(badID))

// Show a newline (for better readability in error messages)
#define SHOW_NEWLINE()																			\
	FBAPopupAddText(PUF_TEXT_DEFAULT, _T("\n"))

// Show a newline followed by a localized string
#define SHOW_NEWLINE_TEXT(id)																	\
	SHOW_NEWLINE();																				\
	FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(id))

// Show a single localized text line
#define SHOW_TEXT(id)																			\
	FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(id))

// Combined Macro for Standard ROM Types (PRG / GFX / SND)
#define ROM_TYPE_ERROR(mask, flag, titleID)														\
	if (nBzipError & mask) {																	\
		SHOW_ROM_SEPARATOR();																	\
		SHOW_TEXT(titleID);																		\
		SHOW_MISSING_OR_BAD(nBzipError & flag, IDS_ERR_LOAD_DATA_MISS, IDS_ERR_LOAD_DATA_BAD);	\
	}

// Show detailed ROM loading error messages
static void ShowRomErrorMessages()
{
	INT32& nBzipError = ScanCtx.nBzipError;

	if (nBzipError & 0x2000) {
		// Show overall status
		if (!(nBzipError & 0x0F0F))
			SHOW_TEXT(IDS_ERR_LOAD_OK);
		else
			SHOW_NEWLINE_TEXT(IDS_ERR_LOAD_PROBLEM);

		// BIOS ROM error messages
		if (nBzipError & 0x0101) {
			SHOW_ROM_SEPARATOR();
			SHOW_MISSING_OR_BAD(nBzipError & 0x0001, IDS_ERR_LOAD_ESS_MISS, IDS_ERR_LOAD_ESS_BAD);
		}

		// Program ROM, Graphics ROM, Sound ROM errors
		ROM_TYPE_ERROR(0x0202, 0x0002, IDS_ERR_LOAD_DET_PRG);
		ROM_TYPE_ERROR(0x0404, 0x0004, IDS_ERR_LOAD_DET_GRA);
		ROM_TYPE_ERROR(0x0808, 0x0008, IDS_ERR_LOAD_DET_SND);

		// Handle uncategorized ROM entries
		if ((nBzipError & 0x0F0F) == 0 && (nBzipError & 0x0010)) {
			SHOW_ROM_SEPARATOR();
			SHOW_MISSING_OR_BAD(nBzipError & 0x1000, IDS_ERR_LOAD_DATA_MISS, IDS_ERR_LOAD_DATA_BAD);
		}
	} else {
		// No valid ROM data found at all
		SHOW_NEWLINE_TEXT(IDS_ERR_LOAD_NODATA);
	}
}

#undef SHOW_ROM_SEPARATOR
#undef SHOW_MISSING_OR_BAD
#undef SHOW_TEXT
#undef ROM_TYPE_ERROR

// MAIN ENTRY POINT - Full ROM loader (scan + load + verify + error report)
INT32 BzipOpen(bool bootApp)
{
	// Close any previously opened ROM archives
	BzipClose();

	//----------------------------------------------------------------------------------------
	// Initialize scan parameters
	//----------------------------------------------------------------------------------------

	INT32 nZipIdx      = 0;
	ScanCtx.pZipIdx    = &nZipIdx;
	ScanCtx.pszDrvName = IsRomDataDrv() ? RomDataDrvGetDrvName() : NULL;

	ScanCtx.pRomStats = (struct RomStatistics*)calloc(1, sizeof(struct RomStatistics));
	if (!ScanCtx.pRomStats) {
		BzipClose();
		return 1;
	}

	INT32& nRomCount      = ScanCtx.pRomStats->nRomCount;
	INT32& nValidRomCount = ScanCtx.pRomStats->nValidRomCount;

	struct RomFind*&     pRomFind  = ScanCtx.pRomFind;
	struct BurnRomInfo*& pRomCache = ScanCtx.pRomCache;

	ScanCtx.bootApp   = bootApp;
	// Get count of valid ROMs required by driver (for early exit optimization)
	nRomCount = CacheROMsInfo(&pRomCache, &pRomFind, &nValidRomCount);
	if (!pRomCache || nRomCount <= 0 || nValidRomCount <= 0) {
		BzipClose();
		return 1;
	}

	struct ArchiveTrackInfo*& pTrack = ScanCtx.archiveTrack;
	if (PreloadDriverArchiveNames(&pTrack) <= 0) {
		BzipClose();
		return 1;
	}

	const INT32 nAppRomPaths = (nQuickOpen > 0) ? DIRS_MAX + 1 : DIRS_MAX;
	const bool  bScanSubDirs = (0 != (nLoadMenuShowY & (1 << 26)));

	//----------------------------------------------------------------------------------------
	// Main scan logic: Two paths
	//----------------------------------------------------------------------------------------
	for (INT32 i = 0; i < nAppRomPaths && !ScanCtx.bAllRomFound; i++) {
		const TCHAR* currPath = (i < DIRS_MAX) ? szAppRomPaths[i] : szAppQuickPath;
		if (IsStrEmpty(currPath))
			continue;													// Skip empty paths

		if (!bScanSubDirs) {
			ScanDirectoryROMs(currPath);
		} else {
			TraverseDirectoriesOnly(currPath, ScanDirectoryROMs);
		}
	}

	if (!bootApp) {
		for (INT32 i = 0; i < pTrack->arcCount; i++) {
			if (!pTrack->arcFound[i])
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_NOTFOUND), pTrack->arcNames[i]);
		}
		SHOW_NEWLINE();
	}

	ScanCtx.nCurrentZip = -1;

	//----------------------------------------------------------------------------------------
	// UI Error Reporting & Final ROM/HDD Validation
	//----------------------------------------------------------------------------------------
	if (!bootApp) {
		CheckRoms();
		ShowRomErrorMessages();

		// Check HDD image
		if (0 != CheckHDDImageExists()) {
			SHOW_NEWLINE_TEXT(IDS_ERR_LOAD_NOHDDDATA);
			ScanCtx.nBzipError = 1;
		}

		BurnExtLoadRom = BzipBurnLoadRom;
	} else {
		// Check HDD image before boot
		if (0 != CheckHDDImageExists())
			return 1;

		return CheckRomsBoot();
	}

	return 0;
}

#undef SHOW_NEWLINE
#undef SHOW_NEWLINE_TEXT

INT32 BzipClose()
{
	ZipClose();

	BurnExtLoadRom = NULL;												// Can't call our function to load each rom anymore

	for (INT32 i = 0; i < BZIP_MAX; i++) {
		free_s((void**)&ScanCtx.szBzipName[i]);
	}


	// Only free archiveTrack members if archiveTrack is valid
	if (ScanCtx.archiveTrack) {
		for (INT32 i = 0; i < ScanCtx.archiveTrack->arcCount; i++)
			free_s((void**)&ScanCtx.archiveTrack->arcNames[i]);
		free_s((void**)&ScanCtx.archiveTrack);
	}
	// Only free pRomCache members if pRomCache is valid
	if (ScanCtx.pRomCache) {
		for (INT32 i = 0; i < ScanCtx.pRomStats->nRomCount; i++)
			free_s((void**)&ScanCtx.pRomCache[i].szName);
		free_s((void**)&ScanCtx.pRomCache);
	}
	if (ScanCtx.pRomStats) {
		free_s((void**)&ScanCtx.pRomStats);
	}

	free_s((void**)&ScanCtx.pRomFind);

	BzipListFree();

	memset(&ScanCtx, 0, sizeof(struct ScanContext));
	ScanCtx.nCurrentZip = -1;											// Close the last zip file if open

	return 0;
}

#undef THREAD_LOCAL
