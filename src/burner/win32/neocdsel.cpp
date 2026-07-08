// ----------------------------------------------------------------------------------------------------------
// NEOCDSEL.CPP
#include "burner.h"
#include "burnint.h"
#include "neocdlist.h"
#include "chd.h"
#include <process.h>

#ifdef BUILD_NEOGEO

INT32			NeoCDList_Init();
/*
static TCHAR*	NeoCDList_ParseCUE(TCHAR* pszFile);
static bool		bProcessingList		= false;
bool bNeoCDListScanOnlyISO			= false;
#define MAX_NGCD	200
static struct GAMELIST ngcd_list[MAX_NGCD];
static INT32 nListItems = 0;
*/
struct PNGRESOLUTION { INT32 nWidth; INT32 nHeight; };

static HWND		hNeoCDWnd			= NULL;
static HWND		hListView			= NULL;
static HBITMAP	hCoverBMPs[3] 		= { 0, 0 };
static HBRUSH	hWhiteBGBrush;
static INT32	nSelectedItem		= -1;

bool bNeoCDListScanSub				= false;

TCHAR szNeoCDCoverDir[  MAX_PATH]	= _T("support/neocdzcovers/");
TCHAR szNeoCDPreviewDir[MAX_PATH]	= _T("support/neocdzpreviews/");
TCHAR szNeoCDGamesDir[  MAX_PATH]	= _T("neocdiso/");

static HIMAGELIST NeoCD_ImageList	= NULL;
static HICON hIconCue				= NULL;
static HICON hIconChd				= NULL;


// CD image stuff
const INT32 nSectorLength			= 2352;

// Stores single NeoGeo CD game metadata
struct GAMELIST {
	bool   bFoundCUE;						// True if game loaded from .cue sheet file
	TCHAR* szPathCUE;						// Full absolute path of cue file
	TCHAR* szPath;							// Full absolute path of real image file (chd/bin)
	TCHAR* szISOFile;						// Raw image filename extracted from cue
	TCHAR* szGameId;						// Game identification number string
	TCHAR* szShortName;						// Game short name for cover matching
	TCHAR* szTitle;							// Game title
	TCHAR* szPublisher;						// Publisher + release year combined string
	TCHAR* szAudioTracks;					// Audio tracks string
//	TCHAR szTracks[99][256];				// Unused track storage array, commented out
	INT32  nIconIdx;						// index inside NeoCD_ImageList
};

// Central game library container, thread-safe with critical section lock
struct GAME_LIB {
	HWND   hGLDlg;							// Handle of attached dialog window
	HANDLE hGLThread;						// Handle of background scan thread
	HANDLE hGLEvent;						// Event handle for thread synchronization
	CRITICAL_SECTION csLock;				// Mutex lock for multi-thread read/write protection
	struct GAMELIST* pGameData;				// Dynamic array stores all scanned game raw data
	INT32  dataCount;						// Total valid game entries stored in pGameData
	INT32  progressCount;
};

static struct GAME_LIB* pGameLib	= NULL;

// Check if system >= Windows XP SP2
static BOOL IsOSXPSP2OrGreater_Verify()
{
	OSVERSIONINFOEX osvi;
	DWORDLONG mask = 0;
	ZeroMemory(&osvi, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(osvi);

	// Target: Windows XP SP2 (5.1.2)
	osvi.dwMajorVersion    = 5;
	osvi.dwMinorVersion    = 1;
	osvi.wServicePackMajor = 2;

	VER_SET_CONDITION(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(mask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

	return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, mask);
}

// Check if system >= Windows Vista
static BOOL IsVistaOrGreater_Verify()
{
	OSVERSIONINFOEX osvi;
	DWORDLONG mask = 0;
	ZeroMemory(&osvi, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	// Vista = NT 6.0
	osvi.dwMajorVersion = 6;
	osvi.dwMinorVersion = 0;
	VER_SET_CONDITION(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	return VerifyVersionInfo(&osvi, VER_MAJORVERSION, mask);
}

// --- Start: Functions to be removed in the future ---
// Check if a character is a Windows path separator (\ or /)
static inline bool IsPathSeparator(TCHAR c)
{
	// Windows supports both backslash and forward slash
	return (_T('\\') == c) || (_T('/') == c);
}

// Check if the path string ends with a valid path separator
static inline bool ends_with_slash(const TCHAR* s)
{
	// Return false if string is empty
	if (IsStrEmpty(s))
		return false;

	// Get the last character of the string
	UINT32 len = _tcslen(s);
	TCHAR c = s[len - 1];

	// Check if last char is a separator
	return IsPathSeparator(c);
}

// Check if the given path is an absolute Windows path
static inline bool IsAbsolutePath(const TCHAR* path)
{
	if (IsStrEmpty(path))
		return false;

	// Check pattern: C:\... or C:/...
	if (_tcslen(path) >= 3) {
		TCHAR drive = path[0];
		if ((drive >= _T('A') && drive <= _T('Z')) || (drive >= _T('a') && drive <= _T('z'))) {
			if ((_T(':') == path[1]) && ((_T('\\') == path[2]) || (_T('/') == path[2])))
				return true;
		}
	}

	// Check pattern: \\server or //server
	if (_tcslen(path) >= 2) {
		if ((_T('\\') == path[0] || _T('/') == path[0]) && (_T('\\') == path[1] || _T('/') == path[1]))
			return true;
	}

	return false;
}

// Safely concatenate directory path and file name
// dest Output double pointer for combined full path (heap-allocated buffer)
// dir Input source directory path string
// file Input source file name or relative path string
// return Valid character length of concatenated path on success; returns -1 if memory allocation fails
// note The allocated output buffer must be released externally via free_s()
// Uses _tcsncpy for cross-compiler ANSI/Unicode compiler compatibility, no unsafe _s series functions
static INT32 PathCombine_s(TCHAR** dest, const TCHAR* dir, const TCHAR* file)
{
	// Top-level null pointer validation for all input pointers
	if (!dest || !dir || !file)
		return -1;

	*dest = NULL;

	// Normalize empty strings once and cache pointers to avoid repeated function calls
	const TCHAR* pDir = IsStrEmpty(dir) ? _T("") : dir;
	const TCHAR* pFile = IsStrEmpty(file) ? _T("") : file;

	size_t dirLen  = _tcslen(pDir);
	size_t fileLen = _tcslen(pFile);
	size_t totalLen;
	TCHAR* pTemp   = NULL;

	// Scenario 1: input file path is absolute, copy it directly
	if (IsAbsolutePath(pFile))
		totalLen = fileLen;
	// Scenario 2: empty directory, keep filename only
	else if (0 == dirLen)
		totalLen = fileLen;
	// Scenario 3: empty filename, keep directory path only
	else if (0 == fileLen)
		totalLen = dirLen;
	// Scenario 4: standard concatenation of directory + file, append backslash if needed
	else {
		bool needSlash = !ends_with_slash(pDir);
		totalLen = dirLen + (needSlash ? 1 : 0) + fileLen;
	}

	// Single centralized memory allocation logic, eliminate duplicated malloc & null-check branches
	pTemp = (TCHAR*)calloc((totalLen + 1), sizeof(TCHAR));
	if (NULL == pTemp)
		return -1;

	// Copy path content branch by branch
	if (IsAbsolutePath(pFile)) {
		_tcsncpy(pTemp, pFile, totalLen);
	}
	else if (0 == dirLen) {
		if (totalLen > 0)
			_tcsncpy(pTemp, pFile, totalLen);
	}
	else if (0 == fileLen) {
		_tcsncpy(pTemp, pDir, totalLen);
	}
	else {
		bool needSlash = !ends_with_slash(pDir);
		size_t offset = dirLen;

		_tcsncpy(pTemp, pDir, dirLen);
		if (needSlash)
			pTemp[offset++] = _T('\\');

		_tcsncpy(pTemp + offset, pFile, fileLen);
	}

	// Explicitly append string terminator to prevent truncated string issues
	pTemp[totalLen] = _T('\0');
	*dest = pTemp;

	return (INT32)totalLen;
}

// Internal recursive directory traversal implementation
// dirPath Current directory path
// pFoundCallBack User-defined callback function
// bDirectoriesOnly Enumerate directories only if true
// bScanSubdirs Enable subdirectory recursion if true
// curDepth Current recursion depth
// maxDepth Maximum allowed recursion depth
// return Positive = valid item count; Negative = traversal interrupted
static INT32 TraverseDirectoryRecurse(const TCHAR* dirPath, INT32(*pFoundCallBack)(const TCHAR*), bool bDirectoriesOnly, bool bScanSubdirs, INT32 curDepth, INT32 maxDepth)
{
	// Stop if exceeding maximum recursion depth
	if (curDepth > maxDepth)
		return 0;

	TCHAR searchPath[MAX_PATH] = { 0 };
	const TCHAR* searchFormat = ends_with_slash(dirPath) ? _T("%s*") : _T("%s\\*");

	// Build search pattern string
	INT32 retPrint = _sntprintf(searchPath, MAX_PATH, searchFormat, dirPath);
	// Check format failure or buffer overflow
	if (retPrint < 0 || (size_t)retPrint >= MAX_PATH - 1)
		return 0;
	// Force string terminator for safety
	searchPath[MAX_PATH - 1] = _T('\0');

	WIN32_FIND_DATA findData = { 0 };
	INT32 itemCount = 0;

	HANDLE hFind = FindFirstFileEx(searchPath, FindExInfoBasic,    &findData, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
	if (hFind == INVALID_HANDLE_VALUE) {
		hFind    = FindFirstFileEx(searchPath, FindExInfoStandard, &findData, FindExSearchNameMatch, NULL, 0);

		if (hFind == INVALID_HANDLE_VALUE) {
			return 0;
		} else {
			bprintf(PRINT_ERROR, _T("TraverseDirectoryRecurse: Is Windows XP\n"));
		}
	}

	bool bInterrupted = false;

	do {
		// Skip current directory "." and parent directory ".."
		if (_tcscmp(findData.cFileName, _T(".")) == 0 || _tcscmp(findData.cFileName, _T("..")) == 0)
			continue;

		TCHAR* fullPath = NULL;
		INT32 pathRet = PathCombine_s(&fullPath, dirPath, findData.cFileName);
		if (pathRet < 0 || !fullPath)
			continue;

		BOOL isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		BOOL isReparsePoint = (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
		INT32 cbRet = 0;

		if (isDirectory) {
			// Skip symbolic links / junction points
			if (isReparsePoint) {
				free_s((void**)&fullPath);
				continue;
			}

			// Trigger callback for directory entries
			if (bDirectoriesOnly && pFoundCallBack) {
				cbRet = pFoundCallBack(fullPath);
				itemCount++;
				if (cbRet < 0) {
					free_s((void**)&fullPath);
					bInterrupted = true;
					break;
				}
			}

			// Recurse into subdirectories
			if (bScanSubdirs) {
				INT32 subCnt = TraverseDirectoryRecurse(fullPath, pFoundCallBack, bDirectoriesOnly, bScanSubdirs, curDepth + 1, maxDepth);
				if (subCnt < 0) {
					free_s((void**)&fullPath);
					bInterrupted = true;
					break;
				}
				itemCount += subCnt;
			}
		} else {
			// Trigger callback for file entries
			if (!bDirectoriesOnly && pFoundCallBack) {
				cbRet = pFoundCallBack(fullPath);
				itemCount++;
				if (cbRet < 0) {
					free_s((void**)&fullPath);
					bInterrupted = true;
					break;
				}
			}
		}

		free_s((void**)&fullPath);

	} while (FindNextFile(hFind, &findData) != 0);

	// Release file search handle
	FindClose(hFind);

	// Return negative value to mark traversal interrupted
	if (bInterrupted)
		return -itemCount;

	return itemCount;
}

// Generic enhanced directory traversal entry function
// dirPath Root directory path to start traversal
// pFoundCallBack Callback function invoked when a file/directory is found
// note Symbolic links and junction points are skipped to avoid infinite recursion.
// Recursion depth is limited to prevent stack overflow. Fully thread-safe & reentrant.
static INT32 TraverseDirectoryExt(const TCHAR* dirPath, INT32(*pFoundCallBack)(const TCHAR*), bool bDirectoriesOnly, bool bScanSubdirs)
{
	const INT32 MAX_RECURSION_DEPTH = 4;
	if (IsStrEmpty(dirPath))
		return 0;

	TCHAR normRoot[MAX_PATH] = { 0 };
	_tcsncpy(normRoot, dirPath, MAX_PATH - 1);
	normRoot[MAX_PATH - 1] = _T('\0');
	size_t len = _tcslen(normRoot);
	if (len > 0 && normRoot[len - 1] == _T('/'))
		normRoot[len - 1] = _T('\\');

	DWORD dwAttr = GetFileAttributes(normRoot);
	if (dwAttr == INVALID_FILE_ATTRIBUTES || !(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
		return 0;

	// Start recursive traversal with initial depth 0
	return TraverseDirectoryRecurse(normRoot, pFoundCallBack, bDirectoriesOnly, bScanSubdirs, 0, MAX_RECURSION_DEPTH);
}

// For files: keep subdir control
#define TraverseDirectoryFilesEx(dir, cb, sub)  TraverseDirectoryExt(dir, cb, false, sub)

// TCHAR safe string duplicate (ANSI / Unicode auto-adaptive)
// Allocates heap memory and copies source TCHAR string safely
// note Uses _tcsncpy to prevent buffer overflow, manually appends string terminator _T('\0')
// Allocated memory must be released by free() or free_s()
static TCHAR* _tcsdup_s(const TCHAR* pSrc)
{
	if (IsStrEmpty(pSrc))
		return NULL;

	size_t srcLen    = _tcslen(pSrc);
	size_t allocSize = (srcLen + 1) * sizeof(TCHAR);
	TCHAR* pDst = (TCHAR*)malloc(allocSize);

	if (!pDst)
		return NULL;

	_tcsncpy(pDst, pSrc, srcLen);
	pDst[srcLen] = _T('\0');

	return pDst;
}

// --- End: Functions to be removed in the future ---

// Release all dynamic TCHAR string members inside one GAMELIST entry
static void FreeGameItem(struct GAMELIST* pItem)
{
	if (!pItem)
		return;

	free_s((void**)&pItem->szPathCUE);
	free_s((void**)&pItem->szPath);
	free_s((void**)&pItem->szISOFile);
	free_s((void**)&pItem->szShortName);
	free_s((void**)&pItem->szPublisher);
	free_s((void**)&pItem->szTitle);
	free_s((void**)&pItem->szAudioTracks);
	free_s((void**)&pItem->szGameId);
}

// Create empty heap-allocated GAME_LIB instance and initialize critical section
// Return: Created GAME_LIB pointer, NULL if memory allocation failed
static struct GAME_LIB* GameLib_Create()
{
	struct GAME_LIB* pLib = (struct GAME_LIB*)calloc(1, sizeof(struct GAME_LIB));
	if (!pLib) return NULL;

	InitializeCriticalSection(&pLib->csLock);
	return pLib;
}

// Append one game entry to dynamic game data array
// Return TRUE on memory allocation success; return FALSE if realloc failed
static BOOL GameLib_AddGame(struct GAME_LIB* pLib, struct GAMELIST* pSrcTemplate)
{
	// Validate input pointers
	if (!pLib || !pSrcTemplate)
		return FALSE;

	EnterCriticalSection(&pLib->csLock);

	// Calculate new total item count after appending one entry
	INT32 newCnt = pLib->dataCount + 1;

	// Reallocate dynamic game array to hold new entry
	// Use temporary pointer pNew to avoid corrupting original array on allocation failure
	struct GAMELIST* pNew = (struct GAMELIST*)realloc(pLib->pGameData, sizeof(struct GAMELIST) * newCnt);
	if (!pNew) {
		LeaveCriticalSection(&pLib->csLock);
		return FALSE;
	}

	// Get pointer to the empty new slot at the end of reallocated array
	struct GAMELIST* pDst = &pNew[pLib->dataCount];

	// Copy numeric value fields from source template
	pDst->bFoundCUE     = pSrcTemplate->bFoundCUE;
	pDst->nIconIdx      = pSrcTemplate->nIconIdx;

	// Transfer string pointer ownership (share original heap memory, no duplication)
	pDst->szPathCUE     = pSrcTemplate->szPathCUE;
	pDst->szPath        = pSrcTemplate->szPath;
	pDst->szISOFile     = pSrcTemplate->szISOFile;
	pDst->szShortName   = pSrcTemplate->szShortName;
	pDst->szPublisher   = pSrcTemplate->szPublisher;
	pDst->szTitle       = pSrcTemplate->szTitle;
	pDst->szGameId      = pSrcTemplate->szGameId;
	pDst->szAudioTracks = pSrcTemplate->szAudioTracks;

	// Replace old game data array with reallocated buffer and update item counter
	pLib->pGameData = pNew;
	pLib->dataCount = newCnt;

	LeaveCriticalSection(&pLib->csLock);
	return TRUE;
}

// Destroy entire GAME_LIB container, release all thread handles, heap strings and dynamic game array
// Do NOT use this function to abort scan halfway; use GameLib_CancelScan for scan cancellation only
static void GameLib_Destroy(struct GAME_LIB* pLib)
{
	if (!pLib)
		return;

	// Step 1: Terminate background scan thread first to prevent wild pointer access
	if (pLib->hGLThread) {
		DWORD dwExitCode = 0;
		GetExitCodeThread(pLib->hGLThread, &dwExitCode);
		if (dwExitCode == STILL_ACTIVE) {
			// Send abort signal to scan loop ahead of waiting
			if (pLib->hGLEvent)
				SetEvent(pLib->hGLEvent);
			// Wait up to 5 seconds for thread self-exit
			WaitForSingleObject(pLib->hGLThread, 5000);
		}
		// Do NOT call CloseHandle here, thread releases handle on its own exit
		// Clear reference to avoid repeated judgment later
		pLib->hGLThread = NULL;
	}

	// Step 2: Release thread synchronization event handle
	if (pLib->hGLEvent) {
		CloseHandle(pLib->hGLEvent);
		pLib->hGLEvent = NULL;
	}

	// Step 3: Only clear dialog handle reference, dialog self-managed by its own message loop
	// Do NOT call DestroyWindow here, avoid cross-thread window destroy crash
	pLib->hGLDlg = NULL;

	// Step 4: Safely acquire critical section to release game data memory
	EnterCriticalSection(&pLib->csLock);
	// Free all heap string members inside every GAMELIST entry
	for (INT32 i = 0; i < pLib->dataCount; i++)
		FreeGameItem(&pLib->pGameData[i]);

	// Release dynamic game data array buffer
	free_s((void**)&pLib->pGameData);
	pLib->pGameData     = NULL;
	pLib->dataCount     = 0;
	pLib->progressCount = 0;
	LeaveCriticalSection(&pLib->csLock);
	// Critical section can be deleted safely whether locked or not
	DeleteCriticalSection(&pLib->csLock);

	// Step 5: Release top-level GAME_LIB heap memory
	free_s((void**)&pLib);
	// Clear global lib pointer to avoid dangling pointer
	if (pGameLib == pLib)
		pGameLib = NULL;
}

// ParseCueGetImageFile
// Return NULL if file open failed / no valid FILE BINARY segment found
TCHAR* ParseCueGetImageFile(const TCHAR* cueFullPath)
{
	if (IsStrEmpty(cueFullPath))
		return NULL;

	FILE* fp = _tfopen(cueFullPath, _T("r"));
	if (!fp)
		return NULL;

	TCHAR szCueDir[2048] = { 0 };
	size_t nBufSize = ARRAY_SIZE(szCueDir);
	_tcsncpy(szCueDir, cueFullPath, nBufSize - 1);
	szCueDir[nBufSize - 1] = _T('\0');

	TCHAR* pSep = _tcsrchr(szCueDir, _T('\\'));
	if (pSep)
		*(pSep + 1) = _T('\0');

	bool  bFoundBinary = false;
	TCHAR szImgName[2048] = { 0 };
	TCHAR szBuffer[ 2048] = { 0 };

	while (_fgetts(szBuffer, ARRAY_SIZE(szBuffer), fp)) {
		// Trim trailing whitespace & control characters (\r \n space etc.)
		INT32 nLen = _tcslen(szBuffer);
		while (nLen > 0 && szBuffer[nLen - 1] < 32)
		{
			szBuffer[nLen - 1] = _T('\0');
			nLen--;
		}

		// Match FILE "filename" BINARY line
		if (!_tcsncmp(szBuffer, _T("FILE"), 4)) {
			TCHAR* pEndQ = _tcsrchr(szBuffer, _T('"'));
			if (!pEndQ)
				continue;
			*pEndQ = _T('\0');

			TCHAR* pStartQ = _tcschr(szBuffer, _T('"'));
			if (!pStartQ)
				continue;
			// Skip if segment is not BINARY type
			if (_tcsstr(pEndQ + 1, _T("BINARY")) == NULL)
				continue;

			// Extract image file name inside quotes
			nBufSize = ARRAY_SIZE(szImgName);
			_tcsncpy(szImgName, pStartQ + 1, nBufSize - 1);
			szImgName[nBufSize - 1] = _T('\0');
			bFoundBinary = true;
			break;
		}
	}
	fclose(fp);

	if (!bFoundBinary)
		return NULL;

	// Combine cue directory and image filename to full absolute path
	TCHAR* pFullImagePath = NULL;
	PathCombine_s(&pFullImagePath, szCueDir, szImgName);
	return pFullImagePath;
}

// Parse cue sheet file, fill path info & audio track count
// Return heap-allocated GAMELIST entry on success, NULL if failed
static struct GAMELIST* ParseCueCreateGameItem(const TCHAR* cueFullPath)
{
	// Params validated upstream; skip duplicate check here
	// Allocate zero-init struct, all pointers default NULL
	struct GAMELIST* pItem = (struct GAMELIST*)calloc(1, sizeof(struct GAMELIST));
	if (!pItem)
		return NULL;

	pItem->nIconIdx = 0;

	FILE* fp = _tfopen(cueFullPath, _T("r"));
	if (!fp) {
		free_s((void**)&pItem);
		return NULL;
	}

	// Extract directory folder from full cue path, use safe _tcsncpy
	TCHAR szCueDir[2048] = { 0 };
	size_t nStrLen = ARRAY_SIZE(szCueDir);
	_tcsncpy(szCueDir, cueFullPath, nStrLen - 1);
	szCueDir[nStrLen - 1] = _T('\0');

	TCHAR* pSep = _tcsrchr(szCueDir, _T('\\'));
	if (pSep)
		*(pSep + 1) = _T('\0');

	bool bHasBinary      = false;
	TCHAR szBuffer[2048] = { 0 };
	const size_t nBufSize = ARRAY_SIZE(szBuffer);

	INT32 nAudioTracks = 0;
	while (_fgetts(szBuffer, (INT32)nBufSize, fp)) {
		// Trim trailing ASCII control chars (\r \n etc.)
		INT32 nLen = _tcslen(szBuffer);
		while (nLen > 0 && szBuffer[nLen - 1] < 32) {
			szBuffer[nLen - 1] = _T('\0');
			nLen--;
		}

		// Parse FILE "filename" BINARY segment
		if (!_tcsncmp(szBuffer, _T("FILE"), 4)) {
			TCHAR* pEndQ  = _tcsrchr(szBuffer, _T('"'));
			if (!pEndQ)
				continue;
			*pEndQ = _T('\0');

			TCHAR* pStartQ = _tcschr(szBuffer, _T('"'));
			if (!pStartQ)
				continue;
			if (!_tcsstr(pEndQ + 1, _T("BINARY")))
				continue;

			pItem->szISOFile = _tcsdup_s(pStartQ + 1);
			pItem->szPathCUE = _tcsdup_s(cueFullPath);

			TCHAR szFullImg[2048] = { 0 };
			nStrLen = ARRAY_SIZE(szFullImg);
			_sntprintf(szFullImg, nStrLen, _T("%s%s"), szCueDir, pStartQ + 1);
			szFullImg[nStrLen - 1] = _T('\0');
			pItem->szPath    = _tcsdup_s(szFullImg);

			pItem->bFoundCUE = true;
			bHasBinary       = true;
		}

		// Parse TRACK definition, same pointer logic as original source
		TCHAR* t = LabelCheck(szBuffer, _T("TRACK"));
		if (t) {
			TCHAR* s = t;
			// Parse track numeric id, advance t past digits
			_tcstol(s, &t, 10);
			s = t;

			// Data track MODE1/2352, skip audio counter
			if ((t = LabelCheck(s, _T("MODE1/2352"))))
				continue;
			// CD-DA audio track, increment counter
			if ((t = LabelCheck(s, _T("AUDIO")))) {
				nAudioTracks++;
				continue;
			}
		}
	}
	fclose(fp);

	memset(szBuffer, 0, nBufSize);
	_sntprintf(szBuffer, nBufSize, _T("%d"), nAudioTracks);
	szBuffer[nBufSize - 1] = _T('\0');
	pItem->szAudioTracks = _tcsdup_s(szBuffer);

	// No valid binary found in cue sheet, release resource
	if (!bHasBinary) {
		FreeGameItem(pItem);
		free_s((void**)&pItem);
		return NULL;
	}

	return pItem;
}

// Create base GAMELIST entry for CHD file, fill path & audio track count from CHD header
// Param chdFullPath: full absolute path of target chd file
// Return heap-allocated GAMELIST on success, NULL on failure
static struct GAMELIST* CreateChdBaseGameItem(TCHAR* chdFullPath)
{
	struct GAMELIST* pItem = (struct GAMELIST*)calloc(1, sizeof(struct GAMELIST));
	if (!pItem)
		return NULL;

	pItem->nIconIdx = 1;

	// Store full CHD absolute path
	pItem->szPath = _tcsdup_s(chdFullPath);
	if (!pItem->szPath) {
		free_s((void**)&pItem);
		return NULL;
	}

	// Extract short file name for szISOFile
	TCHAR szFileName[2048] = { 0 };
	size_t nLen = ARRAY_SIZE(szFileName) - 1;
	_tcsncpy(szFileName, chdFullPath, nLen);
	szFileName[nLen] = _T('\0');

	TCHAR* pNameStart = _tcsrchr(szFileName, _T('\\'));
	if (pNameStart)
		pNameStart++;
	else
		pNameStart = szFileName;

	pItem->szISOFile = _tcsdup_s(pNameStart);
	if (!pItem->szISOFile) {
		FreeGameItem(pItem);
		free_s((void**)&pItem);
		return NULL;
	}

	pItem->bFoundCUE = FALSE;

	// Get audio track count from CHD metadata
	const INT32 nAudioTracks = cdimgCountChdAudioTracks(chdFullPath);

	TCHAR szBuffer[10] = { 0 };
	size_t nSize = ARRAY_SIZE(szBuffer);
	_sntprintf(szBuffer, nSize, _T("%d"), nAudioTracks);
	szBuffer[nSize - 1] = _T('\0');
	pItem->szAudioTracks = _tcsdup_s(szBuffer);

	return pItem;
}

// Unified entry: build game item for both .cue and .chd file
// Return TRUE if entry created successfully, FALSE on any failure / unsupported extension
static BOOL NeoCD_BuildGameEntry(UINT32 nGameID, TCHAR* filePath, struct GAMELIST** ppOutItem)
{
	if (IsStrEmpty(filePath) || !ppOutItem)
		return FALSE;

	*ppOutItem = NULL;
	INT32 nImgIdx = -1;

	struct GAMELIST* pItem = NULL;
	if (       IsFileExt(filePath, _T(".cue"))) {
		pItem   = ParseCueCreateGameItem(filePath);
		nImgIdx = 0;
	} else if (IsFileExt(filePath, _T(".chd"))) {
		pItem   = CreateChdBaseGameItem( filePath);
		nImgIdx = 1;
	} else {
		// Unsupported format
		return FALSE;
	}

	if (!pItem)
		return FALSE;

	NGCDGAME* pMeta = NULL;
	if (!GetNGCDGameTitle(nGameID, &pMeta)) {
		FreeGameItem(pItem);
		free_s((void**)&pItem);
		return FALSE;
	}

	// Combine publisher & release year string
	TCHAR szBuffer[256] = { 0 };
	const size_t nLen = ARRAY_SIZE(szBuffer);
	_sntprintf(szBuffer, nLen, _T("%s (%s)"), pMeta->pszCompany, pMeta->pszYear);
	szBuffer[nLen - 1] = _T('\0');
	pItem->szPublisher = _tcsdup_s(szBuffer);
	if (!pItem->szPublisher) {
		FreeNGCDGame(&pMeta);
		FreeGameItem(pItem);
		free_s((void**)&pItem);
		return FALSE;
	}

	// Fill game identity & display text fields
	pItem->szShortName = _tcsdup_s(pMeta->pszName);
	if (!pItem->szShortName) {
		FreeNGCDGame(&pMeta);
		FreeGameItem(pItem);
		free_s((void**)&pItem);
		return FALSE;
	}

	if (nGameID == 0x0000)
		pItem->szTitle = _tcsdup_s(pItem->szPath);
	else
		pItem->szTitle = _tcsdup_s(pMeta->pszTitle);
	if (!pItem->szTitle) {
		FreeNGCDGame(&pMeta);
		FreeGameItem(pItem);
		free_s((void**)&pItem);
		return FALSE;
	}

	memset(szBuffer, 0, sizeof(szBuffer));
	_sntprintf(szBuffer, nLen, _T("%04X"), nGameID);
	szBuffer[nLen - 1] = _T('\0');
	pItem->szGameId    = _tcsdup_s(szBuffer);
	if (!pItem->szGameId) {
		FreeNGCDGame(&pMeta);
		FreeGameItem(pItem);
		free_s((void**)&pItem);
		return FALSE;
	}

	// Assign icon index based on file type
	pItem->nIconIdx    = nImgIdx;

	FreeNGCDGame(&pMeta);

	*ppOutItem = pItem;
	return TRUE;
}

static bool NeoCD_GameLibInit()
{
	// Lazy init game lib
	if (!pGameLib) {
		pGameLib = GameLib_Create();
		if (!pGameLib) {
			bprintf(PRINT_ERROR, _T("NeoCD_GameLibInit: GameLib_Create failed\n"));
			return false;
		}
	}
	return true;
}

// Callback function for directory scan, construct and store game metadata into game library
static void NeoCD_AddGameLib_Callback(INT32 nGameID, TCHAR* filePath)
{
	struct GAMELIST* pTempItem = NULL;
	// Build temporary game metadata entry from cue/chd file
	if (!NeoCD_BuildGameEntry((UINT32)nGameID, filePath, &pTempItem) || !pTempItem) {
		bprintf(PRINT_ERROR, _T("NeoCD_AddGameLib_Callback: build entry fail ID=0x%04X %s\n"), nGameID, filePath);
		return;
	}

	// Transfer string pointer ownership to global game library array
	BOOL bOk = GameLib_AddGame(pGameLib, pTempItem);
	if (!bOk) {
		bprintf(PRINT_ERROR, _T("NeoCD_AddGameLib_Callback: add to lib memory fail\n"));
	}

	// Only release temporary GAMELIST struct shell, do NOT free internal string buffers
	// Strings are now owned by GAME_LIB and will be released in GameLib_Destroy
	free_s((void**)&pTempItem);
}

// Add cue/chd game to static global game lib
static INT32 NeoCD_AddGameLib(const TCHAR* filePath)
{
	// Check user cancel signal before processing current file
	if (pGameLib && pGameLib->hGLEvent) {
		// Non-blocking wait for abort event
		if (WaitForSingleObject(pGameLib->hGLEvent, 0) == WAIT_OBJECT_0) {
			// Return negative value to tell directory traversal to stop scan
			return -1;
		}
	}

	if (IsStrEmpty(filePath)) {
		bprintf(PRINT_ERROR, _T("NeoCD_AddGameLib: empty or invalid path\n"));
		return 0;
	}

	if (!IsFileExt((TCHAR*)filePath, _T(".cue")) && !IsFileExt((TCHAR*)filePath, _T(".chd")))
		return 0;

	SendDlgItemMessage(pGameLib->hGLDlg, IDC_WAIT_PROG, PBM_STEPIT, 0, 0);

	NeoCDList_CheckISO((TCHAR*)filePath, NeoCD_AddGameLib_Callback);

	return 1;
}

static INT32 NeoCD_GetCDImageCount(const TCHAR* filePath)
{
	if (!IsFileExt((TCHAR*)filePath, _T(".cue")) && !IsFileExt((TCHAR*)filePath, _T(".chd")))
		return 0;

	pGameLib->progressCount++;

	return 1;
}

// dirPath: root scan folder
// bScanSubdirs: enable recursive subfolder traversal
// bTestMode: true = only count files for progress; false = parse & store game data
static INT32 NeoCD_ScanDirectoryRecurse(TCHAR* dirPath, bool bScanSubdirs, bool bTestMode = false)
{
	if (IsStrEmpty(dirPath))
		return 0;

	INT32 ret;
	if (bTestMode) {
		// Count mode, no real parsing
		ret = TraverseDirectoryFilesEx(dirPath, NeoCD_GetCDImageCount, bScanSubdirs);
	} else {
		// Real scan mode, each file enters NeoCD_AddGameLib with cancel check
		ret = TraverseDirectoryFilesEx(dirPath, NeoCD_AddGameLib,      bScanSubdirs);
	}

	// Negative return value means scan aborted by user cancel
	if (ret < 0) {
		bprintf(PRINT_NORMAL, _T("Game directory scan aborted by user\n"));
		return 0;
	}
	return ret;
}

static UINT32 __stdcall CacheGameLibProc(void* lpParam)
{
	// Local stack pointer copy, points to same heap GAME_LIB as global pGameLib
	// This local pLib will not overwrite global pGameLib variable itself
	struct GAME_LIB* pLib = (struct GAME_LIB*)lpParam;

	// Set normal priority for background scanning
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

	// Execute full directory scan and parse game metadata
	NeoCD_ScanDirectoryRecurse(szNeoCDGamesDir, bNeoCDListScanSub);

	// Post close notification to progress dialog window
	if (pLib && pLib->hGLDlg)
		PostMessage(pLib->hGLDlg, WM_CLOSE, 0, 0);

	// Thread self-cleanup: release thread handle and clear reference inside heap struct
	// Local pLib pointer variable itself remains valid, only struct member cleared
	if (pLib->hGLThread) {
		CloseHandle(pLib->hGLThread);
		pLib->hGLThread = NULL;
	}

	return 0;
}

// Gracefully stop scan thread, NO CloseHandle (handled by thread itself)
static void GameLibThreadExit()
{
	if (!pGameLib)
		return;

	// If thread already exited, do nothing
	if (!pGameLib->hGLThread)
		return;

	DWORD dwExitCode = 0;
	GetExitCodeThread(pGameLib->hGLThread, &dwExitCode);
	if (dwExitCode == STILL_ACTIVE) {
		// Send cancel signal
		if (pGameLib->hGLEvent)
			SetEvent(pGameLib->hGLEvent);

		// Wait thread to exit by itself
		WaitForSingleObject(pGameLib->hGLThread, 10000);
	}

	// Only clear pointer, DO NOT close handle
	pGameLib->hGLThread = NULL;

	// Close event
	if (pGameLib->hGLEvent) {
		CloseHandle(pGameLib->hGLEvent);
		pGameLib->hGLEvent = NULL;
	}
}

// Start silent background scan thread and initialize progress UI, no popup window
static void NeoCD_ScanWithProgress(HWND hDlg)
{
	// Initialize global game library singleton if not created
	if (!NeoCD_GameLibInit() || !pGameLib) {
		if (!pGameLib)
		{
			bprintf(PRINT_ERROR, _T("NeoCD_ScanWithProgress: Invalid game library\n"));
		} else {
			bprintf(PRINT_ERROR, _T("NeoCD_ScanWithProgress: NeoCD_GameLibInit failed\n"));
		}
		return;
	}

	// Bind current dialog handle to library for progress message sending
	pGameLib->hGLDlg = hDlg;

	// First pass: traverse directory only to count total image files
	NeoCD_ScanDirectoryRecurse(szNeoCDGamesDir, bNeoCDListScanSub, true);
	if (pGameLib->progressCount <= 0) {
		PostMessage(hDlg, WM_CLOSE, 0, 0);
		return;
	}

	// Initialize progress bar range and step
	SendDlgItemMessage(hDlg, IDC_WAIT_PROG, PBM_SETRANGE,        0, MAKELPARAM(0, pGameLib->progressCount));
	SendDlgItemMessage(hDlg, IDC_WAIT_PROG, PBM_SETSTEP, (WPARAM)1, 0);

	// Show scanning text and cancel button
	ShowWindow(GetDlgItem( hDlg, IDC_WAIT_LABEL_A), TRUE);
	SendMessage(GetDlgItem(hDlg, IDC_WAIT_LABEL_A), WM_SETTEXT, 0, (LPARAM)FBALoadStringEx(hAppInst, IDS_SCANNING_IMGS, true));
	ShowWindow(GetDlgItem( hDlg, IDCANCEL),         TRUE);

	// Create manual-reset abort event for scan thread
	HANDLE hNewEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!hNewEvent) {
		bprintf(PRINT_ERROR, _T("NeoCD_ScanWithProgress: CreateEvent failed\n"));
		return;
	}
	pGameLib->hGLEvent = hNewEvent;

	// Spawn background scan thread
	HANDLE hNewThread = (HANDLE)_beginthreadex(NULL, 0, CacheGameLibProc, pGameLib, 0, NULL);
	if (!hNewThread) {
		bprintf(PRINT_ERROR, _T("NeoCD_ScanWithProgress: Create scan thread failed\n"));
		CloseHandle(hNewEvent);
		pGameLib->hGLEvent = NULL;
		return;
	}
	pGameLib->hGLThread = hNewThread;
}

// Modal wait dialog procedure: only render progress UI, no scan thread creation
static INT_PTR CALLBACK CacheGameLibWaitProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	INT32 xClick = 0;
	INT32 yClick = 0;

	switch (Msg) {
		case WM_INITDIALOG: {
			// Initialize scan thread and progress bar UI
			NeoCD_ScanWithProgress(hDlg);
			// Center modal dialog on main screen
			WndInMid(hDlg, hScrnWnd);
			SetFocus(hDlg);
			break;
		}

		case WM_LBUTTONDOWN: {
			// Capture mouse for window drag, record start position
			SetCapture(hDlg);
			xClick = GET_X_LPARAM(lParam);
			yClick = GET_Y_LPARAM(lParam);
			break;
		}

		case WM_LBUTTONUP: {
			// Release mouse capture after drag finish
			ReleaseCapture();
			break;
		}

		case WM_MOUSEMOVE: {
			// Drag dialog window if mouse captured
			if (GetCapture() == hDlg) {
				RECT rcWindow = { 0 };
				GetWindowRect(hDlg, &rcWindow);

				INT32 xMouse  = GET_X_LPARAM(lParam);
				INT32 yMouse  = GET_Y_LPARAM(lParam);
				INT32 xNewPos = rcWindow.left + xMouse - xClick;
				INT32 yNewPos = rcWindow.top  + yMouse - yClick;

				SetWindowPos(hDlg, NULL, xNewPos, yNewPos, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			}
			break;
		}

		case WM_COMMAND: {
			// Handle cancel button click
			if (LOWORD(wParam) == IDCANCEL) {
				PostMessage(hDlg, WM_CLOSE, 0, 0);
			}
			break;
		}

		case WM_CLOSE: {
			// Gracefully abort background scan thread
			GameLibThreadExit();
			// Clear dialog handle reference if library still exists
			if (pGameLib) {
				pGameLib->hGLDlg = NULL;
			}
			EndDialog(hDlg, 0);
			break;
		}

		// Return default window message processing for unhandled messages
		default:
			return FALSE;
	}
	return TRUE;
}

// Render all cached game entries into ListView control
static void NeoCDList_AddGame(struct GAME_LIB* pLib)
{
	if (!pLib || pLib->dataCount <= 0 || !hListView) {
		if (!pLib)
		{
			bprintf(PRINT_ERROR, _T("NeoCDList_Add: Invalid game library\n"));
		}
		if (pLib->dataCount <= 0)
		{
			bprintf(PRINT_ERROR, _T("NeoCDList_Add: Empty game library\n"));
		}
		if (!hListView)
		{
			bprintf(PRINT_ERROR, _T("NeoCDList_Add: Invalid ListView handle\n"));
		}
//		bprintf(PRINT_ERROR, _T("NeoCDList_Add: Empty game library or invalid ListView handle\n"));
		return;
	}
	const INT32 total = pLib->dataCount;

	// Disable redraw to avoid flicker during batch insertion
	SendMessage(hListView, WM_SETREDRAW, FALSE, 0);
	ListView_DeleteAllItems(hListView);

	LVITEM lvi = { 0 };

	for (INT32 i = 0; i < total; i++) {
		struct GAMELIST* pEntry = &pLib->pGameData[i];
		memset(&lvi, 0, sizeof(LVITEM));

		// Base mask: text content + custom item index storage
		lvi.mask = LVIF_TEXT | LVIF_PARAM;
		// Attach small icon if icon feature & imagelist are enabled
		if (bEnableIcons && NeoCD_ImageList) {
			lvi.mask  |= LVIF_IMAGE;
			lvi.iImage = pEntry->nIconIdx;
		}

		// Store original game array index into lParam for later selection lookup
		lvi.lParam     = (LPARAM)i;
		lvi.iItem      = ListView_GetItemCount(hListView);
		lvi.cchTextMax = 256;
		lvi.pszText    = pEntry->szTitle;

		// Fix: must pass LVITEM pointer as second argument
		INT32 nRow = ListView_InsertItem(hListView, &lvi);
		if (nRow < 0)
			continue;

		// Fill second column: hex game ID
		memset(&lvi, 0, sizeof(LVITEM));
		lvi.mask     = LVIF_TEXT;
		lvi.iItem    = nRow;
		lvi.iSubItem = 1;
		lvi.pszText  = pEntry->szGameId;
		ListView_SetItem(hListView, &lvi);
	}

	// Restore redraw and refresh whole list view
	SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(hListView, NULL, TRUE);
	UpdateWindow(hListView);

	bprintf(PRINT_NORMAL, _T("NeoCDList_AddGame: Render total %d games\n"), total);
}

static INT32 sort_direction = 0;
enum {
	SORT_ASCENDING = 0,
	SORT_DESCENDING = 1
};

struct LVCOMPAREINFO {
	HWND  hWnd;
	INT32 nColumn;
	BOOL  bAscending;
};

static LVCOMPAREINFO lv_compare;

static int CALLBACK ListViewCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	TCHAR buf1[MAX_PATH];
	TCHAR buf2[MAX_PATH];
	LVCOMPAREINFO* lpsd = (struct LVCOMPAREINFO*)lParamSort;

	ListView_GetItemText(lpsd->hWnd, (int)lParam1, lpsd->nColumn, buf1, sizeof(buf1));
	ListView_GetItemText(lpsd->hWnd, (int)lParam2, lpsd->nColumn, buf2, sizeof(buf2));

	switch (lpsd->bAscending) {
		case SORT_ASCENDING:
			return (_tcsicmp(buf1, buf2));
		case SORT_DESCENDING:
			return (0 - _tcsicmp(buf1, buf2));
	}

	return 0;
}

static void ListViewSort(int nDirection, int nColumn)
{
	// sort the list
	lv_compare.hWnd       = hListView;
	lv_compare.nColumn    = nColumn;
	lv_compare.bAscending = nDirection;
	ListView_SortItemsEx(hListView, ListViewCompareFunc, &lv_compare);
}

// Trigger full rescan when binary verification fails, wipe all invalid partial scan data
void CreateNGCDListCache()
{
	// Lazy initialize top-level GAME_LIB struct if not allocated
	// Only alloc outer lib container, will NOT touch existing game data buffer
	if (!NeoCD_GameLibInit() || !pGameLib) {
		bprintf(PRINT_ERROR, _T("CreateNGCDListCache: GAME_LIB init failed\n"));
		return;
	}

	// Step1: Stop leftover scan thread (modal dialog blocks re-entry, just safety defense)
	if (pGameLib->hGLThread) {
		GameLibThreadExit();
	}

	// Step2: Exclusive lock to clear old game entry data (keep top GAME_LIB struct)
	BOOL bLocked = TryEnterCriticalSection(&pGameLib->csLock);
	if (!bLocked) {
		bprintf(PRINT_ERROR, _T("CreateNGCDListCache: Cannot lock data buffer, abort rescan\n"));
		return;
	}

	// Release all old game item heap strings
	for (INT32 i = 0; i < pGameLib->dataCount; i++)
		FreeGameItem(&pGameLib->pGameData[i]);

	// Free dynamic game data array only, DO NOT free top GAME_LIB struct
	free_s((void**)&pGameLib->pGameData);
	pGameLib->pGameData     = NULL;
	pGameLib->dataCount     = 0;
	pGameLib->progressCount = 0;

	LeaveCriticalSection(&pGameLib->csLock);

	// Keep the modal window always on top
	HWND hParent = hNeoCDWnd ? hNeoCDWnd : hScrnWnd;

	// Step3: Pop modal progress dialog to start brand new full scan
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_WAIT), hParent, (DLGPROC)CacheGameLibWaitProc);
	NeoCDList_AddGame(pGameLib);
	ListViewSort(SORT_ASCENDING, 0);
}

static void NeoCDList_InitListView()
{
	LVCOLUMN LvCol;

	DWORD exStyle = LVS_EX_FULLROWSELECT;
	if (IsVistaOrGreater_Verify())
		exStyle |= LVS_EX_INFOTIP;

	ListView_SetExtendedListViewStyle(hListView, exStyle);

	memset(&LvCol, 0, sizeof(LvCol));
	LvCol.mask		= LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

	LvCol.cx		= 430;
	LvCol.pszText	= FBALoadStringEx(hAppInst, IDS_NGCD_TITLE, true);
	SendMessage(hListView, LVM_INSERTCOLUMN , 0, (LPARAM)&LvCol);

	LvCol.cx		= 70;
	LvCol.pszText	= FBALoadStringEx(hAppInst, IDS_NGCD_NGCDID, true);
	SendMessage(hListView, LVM_INSERTCOLUMN , 1, (LPARAM)&LvCol);

	sort_direction = SORT_ASCENDING; // dink

	if (!bEnableIcons)
		return;

	switch (nIconsSize) {
		case ICON_16x16:
			nIconsSizeXY = 16;
			break;
		case ICON_24x24:
			nIconsSizeXY = 24;
			break;
		case ICON_32x32:
			nIconsSizeXY = 32;
			break;
		default:
			break;
	}

	UINT flags = ILC_MASK;
	if (IsOSXPSP2OrGreater_Verify())
		flags |= ILC_COLOR32;
	else
		flags |= ILC_COLOR16;

	NeoCD_ImageList = ImageList_Create(nIconsSizeXY, nIconsSizeXY, flags, 2, 4);
	if (NeoCD_ImageList) {
		// Load cue icon
		hIconCue = (HICON)LoadImage(hAppInst, MAKEINTRESOURCE(IDI_LV_CDIMAGE_CUE), IMAGE_ICON, nIconsSizeXY, nIconsSizeXY, LR_DEFAULTCOLOR);
		if (hIconCue) {
			ImageList_AddIcon(NeoCD_ImageList, hIconCue);
			DestroyIcon(hIconCue);
		}
		// Load chd icon
		hIconChd = (HICON)LoadImage(hAppInst, MAKEINTRESOURCE(IDI_LV_CDIMAGE_CHD), IMAGE_ICON, nIconsSizeXY, nIconsSizeXY, LR_DEFAULTCOLOR);
		if (hIconChd) {
			ImageList_AddIcon(NeoCD_ImageList, hIconChd);
			DestroyIcon(hIconChd);
		}
		// Bind imagelist to listview icon slot
		ListView_SetImageList(hListView, NeoCD_ImageList, LVSIL_SMALL);
	}

// Setup ListView Icons
//	HIMAGELIST hImageList = ImageList_Create(16, 16, ILC_MASK | ILC_COLOR16, 0, 1);
//	ListView_SetImageList(hListView, hImageList, LVSIL_SMALL);
//	ImageList_AddMasked(hImageList, LoadBitmap(hAppInst, MAKEINTRESOURCE(BMP_CD)), RGB(255, 0, 255));
}

/*
*	No need to rely on GAMEID to locate games; duplicate GAMEIDs are allowed in the list.
static INT32 NeoCDList_CheckDuplicates(HWND hList, UINT32 nID)
{
	INT32 nItemCount = ListView_GetItemCount(hList);

	for(INT32 nItem = 0; nItem < nItemCount; nItem++)
	{
		UINT32 nItemVal = 0;
		TCHAR szText[32];

		ListView_GetItemText(hList, nItem, 1, szText, 5);

		_stscanf(szText, _T("%x"), &nItemVal);

		if(nItemVal == nID) {
			ngcd_list[nItemCount].nAudioTracks = 0;
			return 1; // let's get out of here...
		}
	}

	return 0;
}

static void NeoCDSel_Callback(INT32 nID, TCHAR *pszFile)
{
	// make sure we don't add duplicates to the list
	if(NeoCDList_CheckDuplicates(hListView, nID)) {
		return;
	}

	NeoCDList_AddGame(pszFile, nID);
}

static INT32 ScanDir_RecursionCount = 0;

// Scan the specified directory for ISO / CUE files, recurse if desired.
static void NeoCDList_ScanDir_Internal(HWND hList, TCHAR* pszDirectory)
{
	if (ScanDir_RecursionCount > 8) {
		bprintf(PRINT_ERROR, _T("*** Recursion too deep, can't traverse \"%s\"!\n"), pszDirectory);
		return;
	}

	ScanDir_RecursionCount++;

	WIN32_FIND_DATA ffdDirectory;
	HANDLE hDirectory = NULL;
	memset(&ffdDirectory, 0, sizeof(WIN32_FIND_DATA));

	// Scan directory for CUE
	TCHAR szSearch[512] = _T("\0");

	_stprintf(szSearch, _T("%s*.*"), pszDirectory);

	hDirectory = FindFirstFile(szSearch, &ffdDirectory);

	if (hDirectory != INVALID_HANDLE_VALUE)
	{
		do
		{
			// Directories only
			if (ffdDirectory.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (!_tcscmp(ffdDirectory.cFileName, _T(".")) || !_tcscmp(ffdDirectory.cFileName, _T("..")))
				{
					// lets ignore " . " and " .. "
				}
				else if (bNeoCDListScanSub)
				{
					// Traverse & recurse this subdir
					TCHAR szNewDir[512] = _T("\0");
					_stprintf(szNewDir, _T("%s%s\\"), pszDirectory, ffdDirectory.cFileName);

					NeoCDList_ScanDir_Internal(hList, szNewDir);
				}
			}
			// Files only
			if (!(ffdDirectory.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				if (IsFileExt(ffdDirectory.cFileName, _T(".cue")))
				{
					// Found CUE, Parse it
					TCHAR szParse[512] = _T("\0");
					_stprintf(szParse, _T("%s%s"), pszDirectory, ffdDirectory.cFileName);

					TCHAR *pszISO = NeoCDList_ParseCUE( szParse );

					NeoCDList_ParseCUE( szParse );

					TCHAR szISO[512] =_T("\0");
					_stprintf(szISO, _T("%s%s"), pszDirectory, pszISO);
					free(pszISO);

					NeoCDList_CheckISO(szISO, NeoCDSel_Callback);
				}
				else if (IsFileExt(ffdDirectory.cFileName, _T(".chd")))
				{
					TCHAR szFile[512] = _T("\0");
					_stprintf(szFile, _T("%s%s"), pszDirectory, ffdDirectory.cFileName);
					NeoCDList_CheckISO(szFile, NeoCDSel_Callback);
				}
			}
		} while (FindNextFile(hDirectory, &ffdDirectory));
	}
	FindClose(hDirectory);

	ScanDir_RecursionCount--;
}

static void NeoCDList_ScanDir(HWND hList, TCHAR* pszDirectory)
{
	ScanDir_RecursionCount = 0;

	NeoCDList_ScanDir_Internal(hList, pszDirectory);
}
*/
static bool FileExists(TCHAR *tcszFile)
{
	if (IsStrEmpty(tcszFile))
		return 0;

	DWORD dwAttrib = GetFileAttributes(tcszFile);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

#if 0
// This will parse the specified CUE file and return the ISO path, if found
static TCHAR* NeoCDList_ParseCUE(TCHAR* pszFile)
{
	//if(!pszFile) return NULL;

	TCHAR* szISO = NULL;
	szISO = (TCHAR*)malloc(sizeof(TCHAR) * 2048);
	if(!szISO) return NULL;

	// open file
	FILE* fp = NULL;
	fp = _tfopen(pszFile, _T("r"));

	if(!fp) {
		if (szISO)
		{
			free(szISO);
			return NULL;
		}
	}

	while(!feof(fp))
	{
		TCHAR szBuffer[2048];
		TCHAR szOriginal[2048];
		TCHAR* s;
		TCHAR* t;

		_fgetts(szBuffer, 2048, fp);

		INT32 nLength = 0;
		nLength = _tcslen(szBuffer);

		// Remove ASCII control characters from the string (including the 'space' character)
		while (szBuffer[nLength-1] < 32 && nLength > 0)
		{
			szBuffer[nLength-1] = 0;
			nLength--;
		}

		_tcscpy(szOriginal, szBuffer);

		if(!_tcsncmp(szBuffer, _T("FILE"), 4))
		{
			TCHAR* pEnd = _tcsrchr(szBuffer, '"');
			if (!pEnd)	{
				break;	// Invalid CUE format
			}

			*pEnd = 0;

			TCHAR* pStart = _tcschr(szBuffer, '"');

			if(!pStart)	{
				break;	// Invalid CUE format
			}

			if(!_tcsncmp(pEnd + 2, _T("BINARY"), 6))
			{
				_tcscpy(szISO,  pStart + 1);
				ngcd_list[nListItems].bFoundCUE = true;
				_tcscpy(ngcd_list[nListItems].szPathCUE,  pszFile);
				_tcscpy(ngcd_list[nListItems].szISOFile,  pStart + 1);
			}
		}
		// track info
		if ((t = LabelCheck(szBuffer, _T("TRACK"))) != 0) {
			s = t;

			// track number
			/*UINT8 track = */ _tcstol(s, &t, 10);

			s = t;

			// type of track

			if ((t = LabelCheck(s, _T("MODE1/2352"))) != 0) {
				//bprintf(0, _T(".cue: Track #%d, data.\n"), track);
				continue;
			}
			if ((t = LabelCheck(s, _T("AUDIO"))) != 0) {
				//bprintf(0, _T(".cue: Track #%d, AUDIO.\n"), track);
				ngcd_list[nListItems].nAudioTracks++;
				continue;
			}

			fclose(fp);
			return szISO;
		}
	}
	if(fp) fclose(fp);

	return szISO;
}
#endif // 0

static PNGRESOLUTION GetPNGResolutionBuf(void *pPngBuf, size_t nPngSize)
{
	PNGRESOLUTION nResolution = { 0, 0 };
	IMAGE img = { 0, 0, 0, 0, NULL, NULL, 0 };

	if (!pPngBuf || !nPngSize) {
		return nResolution;
	}

	PNGGetInfoBuffer(&img, pPngBuf, nPngSize);

	nResolution.nWidth = img.width;
	nResolution.nHeight = img.height;

	return nResolution;
}

static void NeoCDList_ShowPreviewBuf(HWND hDlg, void *pPngBuf, size_t nPngSize, INT32 nCtrID, INT32 nFrameCtrID, float maxw, float maxh)
{
	PNGRESOLUTION PNGRes = { 0, 0 };
	if(!pPngBuf || !nPngSize)
	{
		HRSRC hrsrc			= FindResource(NULL, MAKEINTRESOURCE(BMP_SPLASH), RT_BITMAP);
		HGLOBAL hglobal		= LoadResource(NULL, (HRSRC)hrsrc);

		BITMAPINFOHEADER* pbmih = (BITMAPINFOHEADER*)LockResource(hglobal);

		PNGRes.nWidth	= pbmih->biWidth;
		PNGRes.nHeight	= pbmih->biHeight;

		FreeResource(hglobal);

	} else {
		PNGRes = GetPNGResolutionBuf(pPngBuf, nPngSize);
	}

	// ------------------------------------------------------
	// PROPER ASPECT RATIO CALCULATIONS

	float w = (float)PNGRes.nWidth;
	float h = (float)PNGRes.nHeight;

	if (maxw == 0 && maxh == 0) {
		// derive max w/h from dialog size
		RECT rect = { 0, 0, 0, 0 };
		GetWindowRect(GetDlgItem(hDlg, IDC_STATIC2), &rect);
		INT32 dw = rect.right - rect.left;
		INT32 dh = rect.bottom - rect.top;

		dw = dw * 90 / 100; // make W 90% of the "Preview / Title" windowpane
		dh = dw * 75 / 100; // make H 75% of w (4:3)

		maxw = dw;
		maxh = dh;
	}

	// max WIDTH
	if(w > maxw) {
		float nh = maxw * (float)(h / w);
		float nw = maxw;

		// max HEIGHT
		if(nh > maxh) {
			nw = maxh * (float)(nw / nh);
			nh = maxh;
		}

		w = nw;
		h = nh;
	}

	// max HEIGHT
	if(h > maxh) {
		float nw = maxh * (float)(w / h);
		float nh = maxh;

		// max WIDTH
		if(nw > maxw) {
			nh = maxw * (float)(nh / nw);
			nw = maxw;
		}

		w = nw;
		h = nh;
	}

	// Proper centering of preview
	float x = ((maxw - w) / 2);
	float y = ((maxh - h) / 2);

	RECT rc = {0, 0, 0, 0};
	GetWindowRect(GetDlgItem(hDlg, nFrameCtrID), &rc);

	POINT pt;
	pt.x = rc.left;
	pt.y = rc.top;

	ScreenToClient(hDlg, &pt);

	// ------------------------------------------------------

	INT32 cover_idx = nCtrID - IDC_NCD_FRONT_PIC;
	if (cover_idx != 0 && cover_idx != 1) cover_idx = 2; // for IDC_NCD_COVER_PREVIEW_PIC

	bprintf(0, _T("cover_idx %d\n"), cover_idx);
	if (hCoverBMPs[cover_idx]) {
		DeleteObject(hCoverBMPs[cover_idx]);
		hCoverBMPs[cover_idx] = 0;
	}

	HBITMAP hCoverBmp = PNGLoadBitmapBuffer(hDlg, pPngBuf, nPngSize, (int)w, (int)h, 0);
	hCoverBMPs[cover_idx] = hCoverBmp;
	if (pPngBuf) free(pPngBuf);

	SetWindowPos(GetDlgItem(hDlg, nCtrID), NULL, (int)(pt.x + x), (int)(pt.y + y), 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
	SendDlgItemMessage(hDlg, nCtrID, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hCoverBmp);
}

//#define IDC_NCD_FRONT_PIC					20801
//#define IDC_NCD_BACK_PIC					20802

static void NeoCDList_Clean(bool bExit)
{
	// Case 1: Only clear preview panel UI without destroying global data & GDI resources
	// Used for refresh list / rescan operation
	if (bExit == false) {
		// Clear preview panels
		NeoCDList_ShowPreviewBuf(hNeoCDWnd, NULL, 0, IDC_NCD_FRONT_PIC, IDC_NCD_FRONT_PIC_FRAME, 0, 0);
		NeoCDList_ShowPreviewBuf(hNeoCDWnd, NULL, 0, IDC_NCD_BACK_PIC,  IDC_NCD_BACK_PIC_FRAME,  0, 0);

		SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTSHORT),     _T(""));
		SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTPUBLISHER), _T(""));
		SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTIMAGE),     _T(""));
		SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTAUDIO),     _T(""));

		return;
	}

	// Case 2: Full resource cleanup when closing window
	// GameLib_Destroy internally terminates scan thread, no duplicate call needed
	if (pGameLib) {
		GameLib_Destroy(pGameLib);
		pGameLib = NULL;
	}

	const INT32 nMax = ARRAY_SIZE(hCoverBMPs);
	// Release all preview cover bitmaps
	for (INT32 i = 0; i < nMax; i++) {
		if (hCoverBMPs[i]) {
			DeleteObject(hCoverBMPs[i]);
			hCoverBMPs[i] = 0;
		}
	}

	// Release listview icon imagelist & icon handles
	if (bEnableIcons && NeoCD_ImageList) {
		ImageList_Destroy(NeoCD_ImageList);
		NeoCD_ImageList = NULL;
	}
	if (hIconCue) {
		DestroyIcon(hIconCue);
		hIconCue = NULL;
	}
	if (hIconChd) {
		DestroyIcon(hIconChd);
		hIconChd = NULL;
	}

	// Release static background brush GDI object
	if (hWhiteBGBrush) {
		DeleteObject(hWhiteBGBrush);
		hWhiteBGBrush = NULL;
	}

	// Reset all global window & thread state variables
	hNeoCDWnd     = NULL;
	hListView     = NULL;
	nSelectedItem = -1;
}

// Zoom the cover / preview image
static HWND   hNeoCDList_CoverDlg = NULL;
static void  *pZoomPng            = NULL;
static size_t nZoomPngSize        = 0;

static INT_PTR CALLBACK NeoCDList_CoverWndProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM /*lParam*/)
{
	if(Msg == WM_INITDIALOG)
	{
		hNeoCDList_CoverDlg = hDlg;

		NeoCDList_ShowPreviewBuf(hDlg, pZoomPng, nZoomPngSize, IDC_NCD_COVER_PREVIEW_PIC, IDC_NCD_COVER_PREVIEW_PIC, 580, 415);

		return TRUE;
	}

	if(Msg == WM_CLOSE)
	{
		EndDialog(hDlg, 0);
		hNeoCDList_CoverDlg	= NULL;
	}

	if(Msg == WM_COMMAND)
	{
		if (LOWORD(wParam) == WM_DESTROY) {
			SendMessage(hDlg, WM_CLOSE, 0, 0);
		}
	}

	return 0;
}

static bool LoadZipToBuffer(TCHAR *szDir, char *szName, TCHAR *szDrvName, char *szPngExt, void **buf, size_t *bufsize)
{
	char szImageName[MAX_PATH];
	char szZipName[MAX_PATH];

	char aszDir[MAX_PATH];
	strcpy(aszDir, TCHARToANSI(szDir, NULL, 0));

	char aszDrvName[MAX_PATH];
	strcpy(aszDrvName, TCHARToANSI(szDrvName, NULL, 0));

	snprintf(szImageName, sizeof(szImageName), "%s/%s%s", szName, aszDrvName, szPngExt);
	snprintf(szZipName, sizeof(szZipName), "%s%s.zip", aszDir, szName);

	const bool retval = unzip(szZipName, szImageName, buf, bufsize);
	bprintf(0, _T("%S:  bFromZip %d  %S  %S\n"), szName, retval, szZipName, szImageName);

	return retval;
}

static bool LoadFileToBuffer(TCHAR *szName, void **buf, size_t *bufsize)
{
	bool retval = false;

	FILE *fp = _tfopen(szName, _T("rb"));

	if (fp) {
		// get size
		fseek(fp, 0, SEEK_END);
		UINT32 nLen = ftell(fp);
		rewind(fp);
		if (nLen > 0) {
			// alloc & load it up!
			*bufsize = nLen;
			*buf = (char*)malloc(nLen);
			fread(*buf, 1, nLen, fp);
			retval = true;
		}
		fclose(fp);
	}

	return retval;
}

void NeoCDList_InitCoverDlg()
{
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_NCD_COVER_DLG), hNeoCDWnd, (DLGPROC)NeoCDList_CoverWndProc);
}

static INT_PTR CALLBACK NeoCDList_WndProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static bool  bNeedRun = false;
	static HICON hDlgIcon = NULL;

	if(Msg == WM_INITDIALOG)
	{
		hNeoCDWnd = hDlg;
		hListView = GetDlgItem(hDlg, IDC_NCD_LIST);
		NeoCDList_InitListView();

		CreateNGCDListCache();

		hDlgIcon = LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_APP));
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hDlgIcon);		// Set the Game Selection dialog icon.

		hWhiteBGBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));

		NeoCDList_ShowPreviewBuf(hNeoCDWnd, NULL, 0, IDC_NCD_FRONT_PIC, IDC_NCD_FRONT_PIC_FRAME, 0, 0);
		NeoCDList_ShowPreviewBuf(hNeoCDWnd, NULL, 0, IDC_NCD_BACK_PIC,  IDC_NCD_BACK_PIC_FRAME, 0, 0);

		SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTSHORT),     _T(""));
		SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTPUBLISHER), _T(""));
		SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTIMAGE),     _T(""));
		SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTAUDIO),     _T(""));

		CheckDlgButton(hNeoCDWnd, IDC_NCD_SSUBDIR_CHECK, bNeoCDListScanSub ? BST_CHECKED : BST_UNCHECKED);
//		CheckDlgButton(hNeoCDWnd, IDC_NCD_SISO_ONLY_CHECK, bNeoCDListScanOnlyISO ? BST_CHECKED : BST_UNCHECKED);

		TreeView_SetItemHeight(hListView, 40);

		TCHAR szDialogTitle[200] = { 0 };
		size_t nTitleLen = ARRAY_SIZE(szDialogTitle);
		_sntprintf(szDialogTitle, nTitleLen, FBALoadStringEx(hAppInst, IDS_NGCD_DIAG_TITLE, true), _T(APP_TITLE), _T(SEPERATOR_1), _T(SEPERATOR_1));
		szDialogTitle[nTitleLen - 1] = _T('\0');
		SetWindowText(hDlg, szDialogTitle);

		WndInMid(hDlg, hScrnWnd);
		SetFocus(hListView);

		return TRUE;
	}

	if(Msg == WM_CLOSE)
	{
		NeoCDList_Clean(true);

		hNeoCDWnd	= NULL;
		hListView	= NULL;

		EndDialog(hDlg, 0);

		if (bNeedRun) {
			bNeedRun = false;
			BurnerLoadDriver(_T("neocdz"));
		}
		return TRUE;
	}

	if (Msg == WM_DESTROY)
	{
		NeoCDList_Clean(true);

		if (hDlgIcon) {
			DestroyIcon(hDlgIcon);
			hDlgIcon = NULL;
		}
		return TRUE;
	}

	if (Msg == WM_CTLCOLORSTATIC)
	{
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_LABELSHORT))		return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_LABELPUBLISHER))	return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_LABELIMAGE))		return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_LABELAUDIO))		return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTSHORT))		return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTPUBLISHER))	return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTIMAGE))		return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTAUDIO))		return (INT_PTR)hWhiteBGBrush;
	}

	if (Msg == WM_NOTIFY)
	{
		NMHDR* pNMHDR = (NMHDR*)lParam;

		// Handle ListView item tooltip request (Vista and above only)
		if (pNMHDR->idFrom == IDC_NCD_LIST && pNMHDR->code == LVN_GETINFOTIP)
		{
			LPNMLVGETINFOTIP pTip = (LPNMLVGETINFOTIP)pNMHDR;

			if (!pTip)
				return TRUE;
			if (!pGameLib)
				return TRUE;

			// Clear tooltip text buffer to empty string
			if (pTip->pszText && pTip->cchTextMax > 0) {
				memset(pTip->pszText, 0, pTip->cchTextMax);
			}

			INT32 nRow = pTip->iItem;
			// Skip if item index is out of valid range, mark message as handled
			if (nRow < 0 || nRow >= pGameLib->dataCount)
				return TRUE;

			LVITEM lvi = { 0 };
			lvi.mask = LVIF_PARAM;
			lvi.iItem = nRow;
			// Retrieve custom array index stored in item lParam
			SendMessage(hListView, LVM_GETITEM, 0, (LPARAM)&lvi);
			INT32 nArrIdx = (INT_PTR)lvi.lParam;

			if (nArrIdx >= 0 && nArrIdx < pGameLib->dataCount) {
				GAMELIST* pEntry = &pGameLib->pGameData[nArrIdx];
				// Copy full image path to tooltip buffer if path exists
				if (pEntry->szPath && _tcslen(pEntry->szPath) > 0) {
					_tcsncpy(pTip->pszText, pEntry->szPath, pTip->cchTextMax - 1);
					pTip->pszText[pTip->cchTextMax - 1]= _T('\0');
				}
			}
			// Return TRUE: notify message fully processed, skip other WM_NOTIFY branches
			return TRUE;
		}

		NMLISTVIEW* pNMLV	= (NMLISTVIEW*)lParam;

		// Game Selected
		if (pNMLV->hdr.code == LVN_ITEMCHANGED && pNMLV->hdr.idFrom == IDC_NCD_LIST)
		{
			INT32 iCount    = SendMessage(hListView, LVM_GETITEMCOUNT,     0, 0);
			INT32 iSelCount = SendMessage(hListView, LVM_GETSELECTEDCOUNT, 0, 0);
			// Skip processing if list is empty or no item selected
			if (iCount == 0 || iSelCount == 0)
				return TRUE;

			INT32 iItem  = pNMLV->iItem;
			// Validate clicked row range
			if (iItem < 0 || iItem >= iCount)
				return TRUE;

			LVITEM LvItem = { 0 };
			LvItem.iItem  = iItem;
			LvItem.mask   = LVIF_PARAM;				// Get game data array index

			// Use LVM_GETITEM instead of GETITEMTEXT to fetch param data
			SendMessage(hListView, LVM_GETITEM, (WPARAM)iItem, (LPARAM)&LvItem);

			// Convert stored lParam to game data array index
			INT32 nArrIdx = (INT32)(INT_PTR)LvItem.lParam;
			// Check index boundary to prevent out-of-bounds access
			if (nArrIdx < 0 || nArrIdx >= pGameLib->dataCount)
				return TRUE;

			// Get target game entry pointer from dynamic game library array
			GAMELIST* pEntry = &pGameLib->pGameData[nArrIdx];
			// Clear all info text panels
			SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTSHORT    ), _T(""));
			SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTPUBLISHER), _T(""));
			SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTIMAGE    ), _T(""));
			SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTAUDIO    ), _T(""));

			// Fill game metadata to UI static text controls
			SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTSHORT    ), pEntry->szShortName);
			SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTPUBLISHER), pEntry->szPublisher);
			SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTIMAGE    ), pEntry->szISOFile);
			SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTAUDIO    ), pEntry->szAudioTracks);

			size_t nCoverSize  = 0;
			TCHAR szFront[512] = { 0 };
			nCoverSize = ARRAY_SIZE(szFront);
			// Build full path for front cover PNG
			_sntprintf(szFront, nCoverSize, _T("%s%s.png"), szNeoCDCoverDir, pEntry->szShortName);
			szFront[nCoverSize - 1] = _T('\0');

			TCHAR szBack[ 512] = { 0 };
			nCoverSize = ARRAY_SIZE(szBack);
			// Build full path for back preview PNG
			_sntprintf(szBack, nCoverSize, _T("%s%s.png"), szNeoCDPreviewDir, pEntry->szShortName);
			szFront[nCoverSize - 1] = _T('\0');

			// Fallback to global preview folder if neocd-specific preview missing
			if (!FileExists(szBack)) {
				_sntprintf(szBack, nCoverSize, _T("%s%s.png"), szAppPreviewsPath, pEntry->szShortName);
				szBack[nCoverSize - 1] = _T('\0');
			}

			void*  pngf     = NULL;
			size_t pngfsize = 0;

			void*  pngb     = NULL;
			size_t pngbsize = 0;

			// Load front cover from file, fallback to zip archive if missing
			if (!LoadFileToBuffer(szFront, &pngf, &pngfsize)) {
				LoadZipToBuffer(szNeoCDCoverDir, "neocdzcovers", pEntry->szShortName, ".png", &pngf, &pngfsize);
			}

			// Try load back preview from neocd zip, then global preview zip
			const bool bFromZip = LoadZipToBuffer(szNeoCDPreviewDir, "neocdzpreviews", pEntry->szShortName, ".png", &pngb, &pngbsize);
			if (!bFromZip && !FileExists(szBack)) {
				LoadZipToBuffer(szAppPreviewsPath, "previews", pEntry->szShortName, ".png", &pngb, &pngbsize);
			}

			// Render front and back cover previews
			NeoCDList_ShowPreviewBuf(hNeoCDWnd, pngf, pngfsize, IDC_NCD_FRONT_PIC, IDC_NCD_FRONT_PIC_FRAME, 0, 0);
			NeoCDList_ShowPreviewBuf(hNeoCDWnd, pngb, pngbsize, IDC_NCD_BACK_PIC, IDC_NCD_BACK_PIC_FRAME, 0, 0);

			// Save currently selected game array index globally
			nSelectedItem = nArrIdx;
			return TRUE;
		}

		// Sort Change
		if (pNMLV->hdr.code == LVN_COLUMNCLICK && pNMLV->hdr.idFrom == IDC_NCD_LIST)
		{
			sort_direction ^= 1;
			ListViewSort(sort_direction, pNMLV->iSubItem);
			return TRUE;
		}


		// Double Click
		if (pNMLV->hdr.code == NM_DBLCLK && pNMLV->hdr.idFrom == IDC_NCD_LIST)
		{
			if(nSelectedItem >= 0) {
				GAMELIST* pEntry = &pGameLib->pGameData[nSelectedItem];
				const TCHAR* targetFile = (pEntry->bFoundCUE) ? pEntry->szPathCUE : pEntry->szPath;
				nCDEmuSelect = 0;
				_tcsncpy(CDEmuImage, targetFile, MAX_PATH - 1);
				CDEmuImage[MAX_PATH - 1]= _T('\0');
				bNeedRun = true;
			} else {
				return TRUE;
			}

			PostMessage(hDlg, WM_CLOSE, 0, 0);
			return TRUE;
		}
	}

	if(Msg == WM_COMMAND)
	{
		if(HIWORD(wParam) == STN_CLICKED)
		{
			INT32 nCtrlID = LOWORD(wParam);

			if(nCtrlID == IDC_NCD_FRONT_PIC)
			{
				if(nSelectedItem >= 0)
				{
					GAMELIST* pEntry   = &pGameLib->pGameData[nSelectedItem];
					TCHAR szFront[512] = { 0 };
					size_t nFrontSize  = ARRAY_SIZE(szFront);
					_sntprintf(szFront, nFrontSize, _T("%s%s.png"), szNeoCDCoverDir, pEntry->szShortName);
					szFront[nFrontSize - 1] = _T('\0');

					pZoomPng = NULL;
					nZoomPngSize = 0;

					if (!LoadFileToBuffer(szFront, &pZoomPng, &nZoomPngSize)) {
						LoadZipToBuffer(szNeoCDCoverDir, "neocdzcovers", pEntry->szShortName, ".png", &pZoomPng, &nZoomPngSize);
					}

					if (pZoomPng && nZoomPngSize > 0) {
						NeoCDList_InitCoverDlg();
					}
				}
				return TRUE;
			}

			if(nCtrlID == IDC_NCD_BACK_PIC)
			{
				if(nSelectedItem >= 0)
				{
					GAMELIST* pEntry  = &pGameLib->pGameData[nSelectedItem];
					TCHAR szBack[512] = { 0 };
					size_t nBackSize  = ARRAY_SIZE(szBack);
					_sntprintf(szBack, nBackSize, _T("%s%s.png"), szNeoCDPreviewDir, pEntry->szShortName);
					szBack[nBackSize - 1] = _T('\0');
					if (!FileExists(szBack)) {
						// no neocd-specific preview? fall-back to regular previews
						_sntprintf(szBack, nBackSize, _T("%s%s.png"), szAppPreviewsPath, pEntry->szShortName);
						szBack[nBackSize - 1] = _T('\0');
					}

					pZoomPng     = NULL;
					nZoomPngSize = 0;

					if (!LoadFileToBuffer(szBack, &pZoomPng, &nZoomPngSize)) {
						const bool bFromZip = LoadZipToBuffer(szNeoCDPreviewDir, "neocdzpreviews", pEntry->szShortName, ".png", &pZoomPng, &nZoomPngSize);

						if (!bFromZip) {
							LoadZipToBuffer(szAppPreviewsPath, "previews", pEntry->szShortName, ".png", &pZoomPng, &nZoomPngSize);
						}
					}

					if (pZoomPng != NULL && nZoomPngSize > 0) {
						NeoCDList_InitCoverDlg();
					}
				}
				return TRUE;
			}
		}

#if 0
		if (LOWORD(wParam) == WM_DESTROY) {
			SendMessage(hDlg, WM_CLOSE, 0, 0);	// WM_CLOSE ???
		}
#endif // 0

		if(HIWORD(wParam) == BN_CLICKED)
		{
			INT32 nCtrlID	= LOWORD(wParam);

			switch(nCtrlID)
			{
				case IDC_NCD_PLAY_BUTTON:
				{
					if(nSelectedItem >= 0) {
						GAMELIST* pEntry = &pGameLib->pGameData[nSelectedItem];
						const TCHAR* targetFile = (pEntry->bFoundCUE) ? pEntry->szPathCUE : pEntry->szPath;
						nCDEmuSelect = 0;
						_tcsncpy(CDEmuImage, targetFile, MAX_PATH - 1);
						CDEmuImage[MAX_PATH - 1]= _T('\0');
						bNeedRun = true;
					} else {
						break;
					}

					PostMessage(hDlg, WM_CLOSE, 0, 0);
					return TRUE;
				}

				case IDC_NCD_SCAN_BUTTON:
				{
					NeoCDList_Clean(false);
					CreateNGCDListCache();
//					NeoCDList_AddGame(pGameLib);
					SetFocus(hListView);
					return TRUE;
				}

				case IDC_NCD_SEL_DIR_BUTTON:
				{
					if (pGameLib && pGameLib->hGLDlg)
						return TRUE;

					TCHAR szBackup[MAX_PATH] = { 0 };
					_tcsncpy(szBackup, szNeoCDGamesDir, MAX_PATH - 1);
					szBackup[MAX_PATH - 1] = _T('\0');

					SupportDirCreate(hNeoCDWnd); SendMessage(hListView, WM_SETREDRAW, TRUE, 0);

					if (_tcsicmp(szNeoCDGamesDir, szBackup) != 0) {
						NeoCDList_Clean(false);
						CreateNGCDListCache();
					}

//					NeoCDList_AddGame(pGameLib);
					SetFocus(hListView);
					return TRUE;
				}

				case IDC_NCD_SSUBDIR_CHECK:
				{
					if(BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_NCD_SSUBDIR_CHECK)) {
						bNeoCDListScanSub = true;
					} else {
						bNeoCDListScanSub = false;
					}
					SetFocus(hListView);
					return TRUE;
				}
#if 0
				case IDC_NCD_SISO_ONLY_CHECK:
				{
					if(BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_NCD_SISO_ONLY_CHECK)) {
						bNeoCDListScanOnlyISO = true;
					} else {
						bNeoCDListScanOnlyISO = false;
					}

					if(bProcessingList) break;

					NeoCDList_Clean(false);

					hProcessThread = (HANDLE)_beginthreadex(NULL, 0, NeoCDList_DoProc, NULL, 0, &ProcessThreadID);
					SetFocus(hListView);
					break;
				}
#endif
				case IDCANCEL:
				{
					PostMessage(hDlg, WM_CLOSE, 0, 0);
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

/*
*	ListView is not thread-safe; window controls must be manipulated by the main thread.
static UINT32 __stdcall NeoCDList_DoProc(void*)
{
	if(bProcessingList) return 0;

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	bProcessingList = true;
	ListView_DeleteAllItems(hListView);
	NeoCDList_AddGame(_T("--- [  Please Wait: Searching for games!  ] ---"), 0x0000);
	NeoCDList_ScanDir(hListView, szNeoCDGamesDir);

	ListView_DeleteItem(hListView, 0); // delete "Please Wait" line :)

	ListViewSort(SORT_ASCENDING, 0);
	SetFocus(hListView);

	bProcessingList = false;
	CloseHandle(hProcessThread);
	hProcessThread = NULL;
	ProcessThreadID = 0;

	return 0;
}
*/

INT32 NeoCDList_Init()
{
	return FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_NCD_DLG), hScrnWnd, (DLGPROC)NeoCDList_WndProc);
}

#else

bool bNeoCDListScanSub			= false;
bool bNeoCDListScanOnlyISO		= false;
TCHAR szNeoCDCoverDir[MAX_PATH] = _T("support/neocdzcovers/");
TCHAR szNeoCDPreviewDir[MAX_PATH] = _T("support/neocdzpreviews/");
TCHAR szNeoCDGamesDir[MAX_PATH] = _T("neocdiso/");

int NeoCDList_Init()
{
	return 0;
}

#endif