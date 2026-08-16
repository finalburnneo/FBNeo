#include "burner.h"
#include "cdlistcache.h"
#include <io.h>

#define CDLIBRARY_SCHEMA_VERSION   (1)
#define CDLIBRARY_IDENTIFY_VERSION (1)
#define CDLIBRARY_MAX_ENTRIES      (100000)
#define CDLIBRARY_MAX_STAMPS       (CDIMAGE_MAX_SOURCE_FILES)
#define CDLIBRARY_MAX_PAYLOAD      (64 * 1024 * 1024)
#define CDLIBRARY_INITIAL_CAPACITY (32)

static const UINT8 CDLibraryMagic[8]   = { 'F', 'B', 'N', 'C', 'D', 'L', 'S', 'T' };
static const UINT8 CDLibraryTrailer[8] = { 'C', 'D', 'L', 'S', 'T', 'E', 'N', 'D' };

struct CDLibraryBuffer {
	UINT8* pData;
	UINT32 nLength;
	UINT32 nCapacity;
};

struct CDLibraryReader {
	const UINT8* pData;
	UINT32 nLength;
	UINT32 nOffset;
};

struct CDLibraryCandidates {
	TCHAR (*pPaths)[MAX_PATH];
	UINT32 nCount;
	UINT32 nCapacity;
};

struct CDLibraryIdentifyContext {
	CDLibraryEntry* pEntry;
	CDLibraryCancelCallback pCancelCallback;
	void* pUser;
	INT32 bOutOfMemory;
	INT32 bStampError;
};

static void CDLibraryCopyText(TCHAR* pszDest, UINT32 nDestCount, const TCHAR* pszSource)
{
	if (!pszDest || !nDestCount)
		return;

	pszDest[0] = _T('\0');
	if (!pszSource)
		return;

	_tcsncpy(pszDest, pszSource, nDestCount - 1);
	pszDest[nDestCount - 1] = _T('\0');
}

static INT32 CDLibraryIsCancelled(CDLibraryCancelCallback pCallback, void* pUser)
{
	return pCallback && pCallback(pUser);
}

static INT32 CDLibraryMultiplySize(UINT32 nCount, size_t nItemSize, size_t* pnSize)
{
	if (!pnSize || (nItemSize && nCount > ((size_t)-1) / nItemSize))
		return 0;

	*pnSize = (size_t)nCount * nItemSize;
	return 1;
}

void CDLibraryEntryFree(CDLibraryEntry* pEntry)
{
	if (!pEntry)
		return;

	free(pEntry->pStamps);
	memset(pEntry, 0, sizeof(*pEntry));
}

CDLibrary* CDLibraryCreate(const TCHAR* pszRoot, INT32 bRecursive)
{
	CDLibrary* pLibrary = (CDLibrary*)calloc(1, sizeof(CDLibrary));
	if (!pLibrary)
		return NULL;

	CDLibraryCopyText(pLibrary->szRoot, MAX_PATH, pszRoot);
	pLibrary->bRecursive = bRecursive ? 1 : 0;
	return pLibrary;
}

void CDLibraryFree(CDLibrary* pLibrary)
{
	if (!pLibrary)
		return;

	for (UINT32 i = 0; i < pLibrary->nCount; i++)
		CDLibraryEntryFree(&pLibrary->pEntries[i]);

	free(pLibrary->pEntries);
	free(pLibrary);
}

static INT32 CDLibraryReserveEntries(CDLibrary* pLibrary, UINT32 nRequired)
{
	if (!pLibrary || nRequired > CDLIBRARY_MAX_ENTRIES)
		return 0;
	if (nRequired <= pLibrary->nCapacity)
		return 1;

	UINT32 nCapacity = pLibrary->nCapacity ? pLibrary->nCapacity : CDLIBRARY_INITIAL_CAPACITY;
	while (nCapacity < nRequired) {
		if (nCapacity > CDLIBRARY_MAX_ENTRIES / 2) {
			nCapacity = CDLIBRARY_MAX_ENTRIES;
			break;
		}
		nCapacity *= 2;
	}

	size_t nSize = 0;
	if (!CDLibraryMultiplySize(nCapacity, sizeof(CDLibraryEntry), &nSize))
		return 0;

	CDLibraryEntry* pEntries = (CDLibraryEntry*)realloc(pLibrary->pEntries, nSize);
	if (!pEntries)
		return 0;

	memset(pEntries + pLibrary->nCapacity, 0, (nCapacity - pLibrary->nCapacity) * sizeof(CDLibraryEntry));
	pLibrary->pEntries  = pEntries;
	pLibrary->nCapacity = nCapacity;
	return 1;
}

INT32 CDLibraryAddEntryMove(CDLibrary* pLibrary, CDLibraryEntry* pEntry)
{
	if (!pLibrary || !pEntry || !CDLibraryReserveEntries(pLibrary, pLibrary->nCount + 1))
		return 0;

	pLibrary->pEntries[pLibrary->nCount++] = *pEntry;
	memset(pEntry, 0, sizeof(*pEntry));
	return 1;
}

static INT32 CDLibraryReserveStamps(CDLibraryEntry* pEntry, UINT32 nRequired)
{
	if (!pEntry || nRequired > CDLIBRARY_MAX_STAMPS)
		return 0;
	if (nRequired <= pEntry->nStampCapacity)
		return 1;

	UINT32 nCapacity = pEntry->nStampCapacity ? pEntry->nStampCapacity : 4;
	while (nCapacity < nRequired) {
		if (nCapacity > CDLIBRARY_MAX_STAMPS / 2) {
			nCapacity = CDLIBRARY_MAX_STAMPS;
			break;
		}
		nCapacity *= 2;
	}

	size_t nSize = 0;
	if (!CDLibraryMultiplySize(nCapacity, sizeof(CDLibraryFileStamp), &nSize))
		return 0;

	CDLibraryFileStamp* pStamps = (CDLibraryFileStamp*)realloc(pEntry->pStamps, nSize);
	if (!pStamps)
		return 0;

	pEntry->pStamps        = pStamps;
	pEntry->nStampCapacity = nCapacity;
	return 1;
}

INT32 CDLibraryAddEntryCopy(CDLibrary* pLibrary, const CDLibraryEntry* pEntry)
{
	if (!pLibrary || !pEntry || (pEntry->nStampCount && !pEntry->pStamps) || pEntry->nStampCount > CDLIBRARY_MAX_STAMPS)
		return 0;

	CDLibraryEntry Entry = *pEntry;
	Entry.pStamps        = NULL;
	Entry.nStampCapacity = 0;
	if (pEntry->nStampCount) {
		if (!CDLibraryReserveStamps(&Entry, pEntry->nStampCount))
			return 0;
		memcpy(Entry.pStamps, pEntry->pStamps, pEntry->nStampCount * sizeof(CDLibraryFileStamp));
	}

	if (!CDLibraryAddEntryMove(pLibrary, &Entry)) {
		CDLibraryEntryFree(&Entry);
		return 0;
	}
	return 1;
}

static INT32 CDLibraryGetStamp(const TCHAR* pszPath, CDLibraryFileStamp* pStamp)
{
	WIN32_FILE_ATTRIBUTE_DATA Data;
	if (!pszPath || !pStamp || !GetFileAttributesEx(pszPath, GetFileExInfoStandard, &Data) || (Data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		return 0;

	memset(pStamp, 0, sizeof(*pStamp));
	CDLibraryCopyText(pStamp->szPath, MAX_PATH, pszPath);
	pStamp->nSize      = ((UINT64)Data.nFileSizeHigh << 32                 ) | Data.nFileSizeLow;
	pStamp->nWriteTime = ((UINT64)Data.ftLastWriteTime.dwHighDateTime << 32) | Data.ftLastWriteTime.dwLowDateTime;
	return 1;
}

static INT32 CDLibraryAddStamp(CDLibraryEntry* pEntry, const TCHAR* pszPath)
{
	if (!pEntry || !pszPath || !pszPath[0])
		return 0;

	for (UINT32 i = 0; i < pEntry->nStampCount; i++)
		if (!_tcsicmp(pEntry->pStamps[i].szPath, pszPath))
			return 1;

	if (!CDLibraryReserveStamps(pEntry, pEntry->nStampCount + 1))
		return 0;
	if (!CDLibraryGetStamp(pszPath, &pEntry->pStamps[pEntry->nStampCount]))
		return -1;

	pEntry->nStampCount++;
	return 1;
}

static INT32 CDLibraryIdentifyCancel(void* pUser)
{
	CDLibraryIdentifyContext* pContext = (CDLibraryIdentifyContext*)pUser;
	return pContext && CDLibraryIsCancelled(pContext->pCancelCallback, pContext->pUser);
}

static void CDLibraryIdentifySource(const TCHAR* pszPath, void* pUser)
{
	CDLibraryIdentifyContext* pContext = (CDLibraryIdentifyContext*)pUser;
	if (!pContext || pContext->bOutOfMemory || pContext->bStampError)
		return;

	INT32 nResult = CDLibraryAddStamp(pContext->pEntry, pszPath);
	if (!nResult)
		pContext->bOutOfMemory = 1;
	else if (nResult < 0)
		pContext->bStampError = 1;
}

static INT32 CDLibraryPlatformEnabled(INT32 nPlatform)
{
#ifdef BUILD_NEOGEO
	if (nPlatform == CDLIST_PLATFORM_NEOCD)
		return 1;
#endif
#ifdef BUILD_PCE
	if (nPlatform == CDLIST_PLATFORM_PCECD)
		return 1;
#endif
	return 0;
}

static void CDLibraryGetFallbackName(const TCHAR* pszPath, TCHAR* pszName, UINT32 nNameCount)
{
	TCHAR szPath[MAX_PATH];
	CDLibraryCopyText(szPath, MAX_PATH, pszPath);
	for (TCHAR* p = szPath; *p; p++)
		if (*p == _T('/'))
			*p = _T('\\');

	TCHAR* pszBase = _tcsrchr(szPath, _T('\\'));
	pszBase = pszBase ? pszBase + 1 : szPath;
	TCHAR* pszDot = _tcsrchr(pszBase, _T('.'));
	if (pszDot)
		*pszDot = _T('\0');

	if (!_tcsicmp(pszBase, _T("disc")) || !_tcsicmp(pszBase, _T("track")) || !_tcsicmp(pszBase, _T("image"))) {
		TCHAR* pszSeparator = pszBase > szPath ? pszBase - 1 : NULL;
		if (pszSeparator) {
			*pszSeparator = _T('\0');
			TCHAR* pszParent = _tcsrchr(szPath, _T('\\'));
			if (pszParent && pszParent[1])
				pszBase = pszParent + 1;
			else if (szPath[0])
				pszBase = szPath;
		}
	}
	CDLibraryCopyText(pszName, nNameCount, pszBase);
}

static void CDLibraryFillEntry(CDLibraryEntry* pEntry, const TCHAR* pszPath, const CDListResult* pResult)
{
	TCHAR szFallback[CDLIST_TEXT_SIZE];
	CDLibraryGetFallbackName(pszPath, szFallback, CDLIST_TEXT_SIZE);
	pEntry->nPlatform   = pResult->nPlatform;
	pEntry->nSource     = pResult->nSource;
	pEntry->nConfidence = pResult->nConfidence;
	pEntry->nNeoID      = pResult->nNeoID;
	pEntry->nAudioTrackCount = pResult->nAudioTrackCount;
	pEntry->nIcon       = IsFileExt((TCHAR*)pszPath, (TCHAR*)_T(".chd")) ? 1 : 0;
	CDLibraryCopyText(pEntry->szPath,      MAX_PATH,         pszPath);
	CDLibraryCopyText(pEntry->szDataPath,  MAX_PATH,         pResult->szFirstDataTrackPath[0] ? pResult->szFirstDataTrackPath : pszPath);
	CDLibraryCopyText(pEntry->szShortName, CDLIST_TEXT_SIZE, pResult->Metadata.szName[0]      ? pResult->Metadata.szName      : szFallback);
	CDLibraryCopyText(pEntry->szTitle,     CDLIST_TEXT_SIZE, pResult->Metadata.szTitle[0]     ? pResult->Metadata.szTitle     : szFallback);
	CDLibraryCopyText(pEntry->szCompany,   CDLIST_TEXT_SIZE, pResult->Metadata.szCompany);
	CDLibraryCopyText(pEntry->szYear,      32,               pResult->Metadata.szYear);
	CDLibraryCopyText(pEntry->szRegion,    64,               pResult->Metadata.szRegion);
	CDLibraryCopyText(pEntry->szRequirement, CDLIST_TEXT_SIZE, pResult->Metadata.szRequirement);
}

static INT32 CDLibraryStampsValid(const CDLibraryEntry* pEntry)
{
	if (!pEntry || !pEntry->nStampCount || pEntry->nStampCount > CDLIBRARY_MAX_STAMPS)
		return 0;

	for (UINT32 i = 0; i < pEntry->nStampCount; i++) {
		CDLibraryFileStamp Stamp;
		if (!CDLibraryGetStamp(pEntry->pStamps[i].szPath, &Stamp) || Stamp.nSize != pEntry->pStamps[i].nSize || Stamp.nWriteTime != pEntry->pStamps[i].nWriteTime)
			return 0;
	}
	return 1;
}

static int CDLibraryComparePath(const void* pLeft, const void* pRight)
{
	const CDLibraryEntry* pA = (const CDLibraryEntry*)pLeft;
	const CDLibraryEntry* pB = (const CDLibraryEntry*)pRight;
	return _tcsicmp(pA->szPath, pB->szPath);
}

static const CDLibraryEntry* CDLibraryFindCached(const CDLibrary* pLibrary, const TCHAR* pszPath)
{
	if (!pLibrary || !pszPath)
		return NULL;

	UINT32 nLow  = 0;
	UINT32 nHigh = pLibrary->nCount;
	while (nLow < nHigh) {
		UINT32 nMiddle = nLow + (nHigh - nLow) / 2;
		INT32  nResult = _tcsicmp(pLibrary->pEntries[nMiddle].szPath, pszPath);
		if (nResult < 0)
			nLow  = nMiddle + 1;
		else if (nResult > 0)
			nHigh = nMiddle;
		else
			return &pLibrary->pEntries[nMiddle];
	}
	return NULL;
}

static INT32 CDLibraryCandidatesReserve(CDLibraryCandidates* pCandidates, UINT32 nRequired)
{
	if (!pCandidates || nRequired > CDLIBRARY_MAX_ENTRIES)
		return 0;
	if (nRequired <= pCandidates->nCapacity)
		return 1;

	UINT32 nCapacity = pCandidates->nCapacity ? pCandidates->nCapacity : CDLIBRARY_INITIAL_CAPACITY;
	while (nCapacity < nRequired) {
		if (nCapacity > CDLIBRARY_MAX_ENTRIES / 2) {
			nCapacity = CDLIBRARY_MAX_ENTRIES;
			break;
		}
		nCapacity *= 2;
	}

	size_t nSize = 0;
	if (!CDLibraryMultiplySize(nCapacity, sizeof(*pCandidates->pPaths), &nSize))
		return 0;
	void* pPaths = realloc(pCandidates->pPaths, nSize);
	if (!pPaths)
		return 0;
	pCandidates->pPaths    = (TCHAR (*)[MAX_PATH])pPaths;
	pCandidates->nCapacity = nCapacity;
	return 1;
}

static INT32 CDLibraryAddCandidate(CDLibraryCandidates* pCandidates, const TCHAR* pszPath)
{
	if (!CDLibraryCandidatesReserve(pCandidates, pCandidates->nCount + 1))
		return 0;

	CDLibraryCopyText(pCandidates->pPaths[pCandidates->nCount++], MAX_PATH, pszPath);
	return 1;
}

static INT32 CDLibraryCombinePath(TCHAR* pszDest, const TCHAR* pszDirectory, const TCHAR* pszName)
{
	if (!pszDest || !pszDirectory || !pszName)
		return 0;

	UINT32 nDirectoryLength = (UINT32)_tcslen(pszDirectory);
	UINT32 nNameLength      = (UINT32)_tcslen(pszName);
	INT32  bSeparator       = nDirectoryLength && pszDirectory[nDirectoryLength - 1] != _T('\\') && pszDirectory[nDirectoryLength - 1] != _T('/');
	if (nDirectoryLength >= MAX_PATH || nNameLength >= MAX_PATH || nDirectoryLength + bSeparator + nNameLength >= MAX_PATH)
		return 0;

	CDLibraryCopyText(pszDest, MAX_PATH, pszDirectory);
	if (bSeparator)
		_tcscat(pszDest, _T("\\"));

	_tcscat(pszDest, pszName);
	return 1;
}

static INT32 CDLibraryEnumerate(const TCHAR* pszDirectory, INT32 bRecursive, CDLibraryCandidates* pCandidates, CDLibraryCancelCallback pCancelCallback, CDLibraryProgressCallback pProgressCallback, void* pUser)
{
	if (CDLibraryIsCancelled(pCancelCallback, pUser))
		return CDLIBRARY_SCAN_CANCELLED;

	TCHAR szPattern[MAX_PATH];
	if (!CDLibraryCombinePath(szPattern, pszDirectory, _T("*")))
		return CDLIBRARY_SCAN_IO_ERROR;

	WIN32_FIND_DATA FindData;
	HANDLE hFind = FindFirstFileEx(szPattern, FindExInfoBasic, &FindData, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
	if (hFind == INVALID_HANDLE_VALUE)
		hFind = FindFirstFileEx(szPattern, FindExInfoStandard, &FindData, FindExSearchNameMatch, NULL, 0);
	if (hFind == INVALID_HANDLE_VALUE) {
		DWORD nError = GetLastError();
		return nError == ERROR_FILE_NOT_FOUND ? CDLIBRARY_SCAN_OK : CDLIBRARY_SCAN_IO_ERROR;
	}

	INT32 nStatus = CDLIBRARY_SCAN_OK;
	do {
		if (CDLibraryIsCancelled(pCancelCallback, pUser)) {
			nStatus = CDLIBRARY_SCAN_CANCELLED;
			break;
		}
		if (!_tcscmp(FindData.cFileName, _T(".")) || !_tcscmp(FindData.cFileName, _T("..")))
			continue;

		TCHAR szPath[MAX_PATH];
		if (!CDLibraryCombinePath(szPath, pszDirectory, FindData.cFileName))
			continue;

		if (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (bRecursive && !(FindData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
				nStatus = CDLibraryEnumerate(szPath, bRecursive, pCandidates, pCancelCallback, pProgressCallback, pUser);
				if (nStatus != CDLIBRARY_SCAN_OK)
					break;
			}
		} else if (IsFileExt(szPath, (TCHAR*)_T(".cue")) || IsFileExt(szPath, (TCHAR*)_T(".ccd")) || IsFileExt(szPath, (TCHAR*)_T(".chd"))) {
			if (!CDLibraryAddCandidate(pCandidates, szPath)) {
				nStatus = CDLIBRARY_SCAN_OUT_OF_MEMORY;
				break;
			}
			if (pProgressCallback)
				pProgressCallback(CDLIBRARY_PHASE_ENUMERATE, pCandidates->nCount, pCandidates->nCount, pUser);
		}
	} while (FindNextFile(hFind, &FindData));

	DWORD nError = GetLastError();
	FindClose(hFind);
	if (nStatus == CDLIBRARY_SCAN_OK && nError != ERROR_NO_MORE_FILES)
		nStatus = CDLIBRARY_SCAN_IO_ERROR;

	return nStatus;
}

static UINT32 CDLibraryCrc32(const UINT8* pData, UINT32 nLength)
{
	UINT32 nCrc = 0xffffffff;
	for (UINT32 i = 0; i < nLength; i++) {
		nCrc ^= pData[i];
		for (INT32 bit = 0; bit < 8; bit++)
			nCrc = (nCrc >> 1) ^ (0xedb88320 & (0 - (nCrc & 1)));
	}
	return nCrc ^ 0xffffffff;
}

static INT32 CDLibraryBufferReserve(CDLibraryBuffer* pBuffer, UINT32 nAdditional)
{
	if (!pBuffer || nAdditional > CDLIBRARY_MAX_PAYLOAD - pBuffer->nLength)
		return 0;
	UINT32 nRequired = pBuffer->nLength + nAdditional;
	if (nRequired <= pBuffer->nCapacity)
		return 1;

	UINT32 nCapacity = pBuffer->nCapacity ? pBuffer->nCapacity : 4096;
	while (nCapacity < nRequired) {
		if (nCapacity > CDLIBRARY_MAX_PAYLOAD / 2) {
			nCapacity = CDLIBRARY_MAX_PAYLOAD;
			break;
		}
		nCapacity *= 2;
	}
	UINT8* pData = (UINT8*)realloc(pBuffer->pData, nCapacity);
	if (!pData)
		return 0;

	pBuffer->pData     = pData;
	pBuffer->nCapacity = nCapacity;
	return 1;
}

static INT32 CDLibraryBufferBytes(CDLibraryBuffer* pBuffer, const void* pData, UINT32 nLength)
{
	if (!CDLibraryBufferReserve(pBuffer, nLength))
		return 0;

	if (nLength)
		memcpy(pBuffer->pData + pBuffer->nLength, pData, nLength);

	pBuffer->nLength += nLength;
	return 1;
}

static INT32 CDLibraryBufferU32(CDLibraryBuffer* pBuffer, UINT32 nValue)
{
	UINT8 Data[4] = {
		(UINT8)nValue,
		(UINT8)(nValue >>  8),
		(UINT8)(nValue >> 16),
		(UINT8)(nValue >> 24)
	};
	return CDLibraryBufferBytes(pBuffer, Data, sizeof(Data));
}

static INT32 CDLibraryBufferU64(CDLibraryBuffer* pBuffer, UINT64 nValue)
{
	UINT8 Data[8];
	for (INT32 i = 0; i < 8; i++)
		Data[i] = (UINT8)(nValue >> (i * 8));

	return CDLibraryBufferBytes(pBuffer, Data, sizeof(Data));
}

static INT32 CDLibraryBufferString(CDLibraryBuffer* pBuffer, const TCHAR* pszText)
{
	char* pszUtf8 = utf8_from_tstring(pszText ? pszText : _T(""));
	if (!pszUtf8)
		return 0;

	size_t nLengthSize = strlen(pszUtf8);
	INT32  nResult     = nLengthSize <= 0x7fffffff && CDLibraryBufferU32(pBuffer, (UINT32)nLengthSize) && CDLibraryBufferBytes(pBuffer, pszUtf8, (UINT32)nLengthSize);

	free_s((void**)&pszUtf8);
	return nResult;
}

static INT32 CDLibraryReaderBytes(CDLibraryReader* pReader, void* pDest, UINT32 nLength)
{
	if (!pReader || pReader->nOffset > pReader->nLength || nLength > pReader->nLength - pReader->nOffset)
		return 0;

	if (pDest && nLength)
		memcpy(pDest, pReader->pData + pReader->nOffset, nLength);

	pReader->nOffset += nLength;
	return 1;
}

static INT32 CDLibraryReaderU32(CDLibraryReader* pReader, UINT32* pnValue)
{
	UINT8 Data[4];
	if (!pnValue || !CDLibraryReaderBytes(pReader, Data, sizeof(Data)))
		return 0;

	*pnValue = (UINT32)Data[0] | ((UINT32)Data[1] << 8) | ((UINT32)Data[2] << 16) | ((UINT32)Data[3] << 24);
	return 1;
}

static INT32 CDLibraryReaderU64(CDLibraryReader* pReader, UINT64* pnValue)
{
	UINT8 Data[8];
	if (!pnValue || !CDLibraryReaderBytes(pReader, Data, sizeof(Data)))
		return 0;

	*pnValue = 0;
	for (INT32 i = 0; i < 8; i++)
		*pnValue |= (UINT64)Data[i] << (i * 8);

	return 1;
}

static INT32 CDLibraryReaderString(CDLibraryReader* pReader, TCHAR* pszDest, UINT32 nDestCount)
{
	UINT32 nLength = 0;
	if (!pszDest || !nDestCount || !CDLibraryReaderU32(pReader, &nLength) || pReader->nOffset > pReader->nLength || nLength > pReader->nLength - pReader->nOffset || nLength > CDLIBRARY_MAX_PAYLOAD - 1)
		return 0;

	char* pszUtf8 = (char*)malloc((size_t)nLength + 1);
	if (!pszUtf8)
		return 0;
	if (!CDLibraryReaderBytes(pReader, pszUtf8, nLength)) {
		free_s((void**)&pszUtf8);
		return 0;
	}
	pszUtf8[nLength] = '\0';
	TCHAR* pszText = tstring_from_utf8(pszUtf8);
	free_s((void**)&pszUtf8);
	if (!pszText)
		return 0;

	INT32 nResult = _tcslen(pszText) < nDestCount;
	if (nResult)
		CDLibraryCopyText(pszDest, nDestCount, pszText);

	free_s((void**)&pszText);
	return nResult;
}

static INT32 CDLibrarySerializeEntry(CDLibraryBuffer* pBuffer, const CDLibraryEntry* pEntry)
{
	if (!CDLibraryBufferU32(pBuffer, (UINT32)pEntry->nPlatform       ) || !CDLibraryBufferU32(pBuffer, (UINT32)pEntry->nSource) ||
		!CDLibraryBufferU32(pBuffer, (UINT32)pEntry->nConfidence     ) || !CDLibraryBufferU32(pBuffer, pEntry->nNeoID         ) ||
		!CDLibraryBufferU32(pBuffer, (UINT32)pEntry->nAudioTrackCount) || !CDLibraryBufferU32(pBuffer, (UINT32)pEntry->nIcon  ) ||
		!CDLibraryBufferString(pBuffer, pEntry->szPath     ) || !CDLibraryBufferString(pBuffer, pEntry->szDataPath   ) ||
		!CDLibraryBufferString(pBuffer, pEntry->szShortName) || !CDLibraryBufferString(pBuffer, pEntry->szTitle      ) ||
		!CDLibraryBufferString(pBuffer, pEntry->szCompany  ) || !CDLibraryBufferString(pBuffer, pEntry->szYear       ) ||
		!CDLibraryBufferString(pBuffer, pEntry->szRegion   ) || !CDLibraryBufferString(pBuffer, pEntry->szRequirement) ||
		!CDLibraryBufferU32(pBuffer, pEntry->nStampCount))
		return 0;

	for (UINT32 i = 0; i < pEntry->nStampCount; i++)
		if (!CDLibraryBufferString(pBuffer, pEntry->pStamps[i].szPath) || !CDLibraryBufferU64(pBuffer, pEntry->pStamps[i].nSize) || !CDLibraryBufferU64(pBuffer, pEntry->pStamps[i].nWriteTime))
			return 0;
	return 1;
}

static INT32 CDLibraryDeserializeEntry(CDLibraryReader* pReader, CDLibraryEntry* pEntry)
{
	UINT32 nValue = 0;
	memset(pEntry, 0, sizeof(*pEntry));
	if (!CDLibraryReaderU32(pReader, &nValue))
		return 0;
	pEntry->nPlatform = (INT32)nValue;
	if (!CDLibraryReaderU32(pReader, &nValue))
		return 0;
	pEntry->nSource = (INT32)nValue;
	if (!CDLibraryReaderU32(pReader, &nValue))
		return 0;
	pEntry->nConfidence = (INT32)nValue;
	if (!CDLibraryReaderU32(pReader, &pEntry->nNeoID))
		return 0;
	if (!CDLibraryReaderU32(pReader, &nValue))
		return 0;
	pEntry->nAudioTrackCount = (INT32)nValue;
	if (!CDLibraryReaderU32(pReader, &nValue))
		return 0;
	pEntry->nIcon = (INT32)nValue;
	if (!CDLibraryReaderString(pReader, pEntry->szPath,      MAX_PATH        ) || !CDLibraryReaderString(pReader, pEntry->szDataPath,    MAX_PATH        ) ||
		!CDLibraryReaderString(pReader, pEntry->szShortName, CDLIST_TEXT_SIZE) || !CDLibraryReaderString(pReader, pEntry->szTitle,       CDLIST_TEXT_SIZE) ||
		!CDLibraryReaderString(pReader, pEntry->szCompany,   CDLIST_TEXT_SIZE) || !CDLibraryReaderString(pReader, pEntry->szYear,        32              ) ||
		!CDLibraryReaderString(pReader, pEntry->szRegion,    64              ) || !CDLibraryReaderString(pReader, pEntry->szRequirement, CDLIST_TEXT_SIZE) ||
		!CDLibraryReaderU32(pReader, &pEntry->nStampCount                    ) || !pEntry->nStampCount || pEntry->nStampCount > CDLIBRARY_MAX_STAMPS)
		return 0;

	if (!CDLibraryReserveStamps(pEntry, pEntry->nStampCount))
		return 0;
	for (UINT32 i = 0; i < pEntry->nStampCount; i++)
		if (!CDLibraryReaderString(pReader, pEntry->pStamps[i].szPath, MAX_PATH) || !CDLibraryReaderU64(pReader, &pEntry->pStamps[i].nSize) || !CDLibraryReaderU64(pReader, &pEntry->pStamps[i].nWriteTime))
			return 0;
	return 1;
}

static INT32 CDLibraryGetCachePath(TCHAR* pszPath, UINT32 nPathCount)
{
	TCHAR szModule[MAX_PATH];
	DWORD nLength = GetModuleFileName(NULL, szModule, MAX_PATH);
	if (!nLength || nLength >= MAX_PATH)
		return 0;

	TCHAR* pszSeparator = _tcsrchr(szModule, _T('\\'));
	if (!pszSeparator)
		return 0;

	pszSeparator[1] = _T('\0');

	TCHAR szName[EXE_NAME_SIZE + 32];
	INT32 nNameLength = _sntprintf(szName, sizeof(szName) / sizeof(szName[0]), _T("config\\%s.cdlist.dat"), szAppExeName);
	if (nNameLength < 0 || (UINT32)nNameLength >= sizeof(szName) / sizeof(szName[0]))
		return 0;

	return CDLibraryCombinePath(pszPath, szModule, szName) && _tcslen(pszPath) < nPathCount;
}

static CDLibrary* CDLibraryLoadCache(const TCHAR* pszCachePath, const TCHAR* pszRoot, INT32 bRecursive)
{
	HANDLE hFile = CreateFile(pszCachePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;

	LARGE_INTEGER Size;
	if (!GetFileSizeEx(hFile, &Size) || Size.QuadPart < 40 || Size.QuadPart > CDLIBRARY_MAX_PAYLOAD + 40) {
		CloseHandle(hFile);
		return NULL;
	}
	UINT32 nFileLength = (UINT32)Size.QuadPart;
	UINT8* pFile = (UINT8*)malloc(nFileLength);
	DWORD  nRead = 0;
	INT32  bRead = pFile && ReadFile(hFile, pFile, nFileLength, &nRead, NULL) && nRead == nFileLength;
	CloseHandle(hFile);
	if (!bRead) {
		free_s((void**)&pFile);
		return NULL;
	}

	CDLibraryReader FileReader = { pFile, nFileLength, 0 };
	UINT8 Magic[8], Trailer[8];
	UINT32 nSchema, nIdentify, nStoredRecursive, nEntryCount, nPayloadLength, nCrc;
	INT32 bHeader = CDLibraryReaderBytes(&FileReader, Magic, 8) && CDLibraryReaderU32(&FileReader, &nSchema) &&
		CDLibraryReaderU32(&FileReader, &nIdentify  ) && CDLibraryReaderU32(&FileReader, &nStoredRecursive) &&
		CDLibraryReaderU32(&FileReader, &nEntryCount) && CDLibraryReaderU32(&FileReader, &nPayloadLength  ) && CDLibraryReaderU32(&FileReader, &nCrc);
	if (!bHeader || memcmp(Magic, CDLibraryMagic, 8) || nSchema != CDLIBRARY_SCHEMA_VERSION || nIdentify != CDLIBRARY_IDENTIFY_VERSION ||
		nStoredRecursive > 1 || nEntryCount > CDLIBRARY_MAX_ENTRIES || nPayloadLength > CDLIBRARY_MAX_PAYLOAD || nPayloadLength != nFileLength - 40 ||
		CDLibraryCrc32(pFile + 32, nPayloadLength) != nCrc || !CDLibraryReaderBytes(&FileReader, NULL, nPayloadLength) ||
		!CDLibraryReaderBytes(&FileReader, Trailer, 8) || memcmp(Trailer, CDLibraryTrailer, 8)) {
		free_s((void**)&pFile);
		return NULL;
	}

	CDLibraryReader Payload = { pFile + 32, nPayloadLength, 0 };
	TCHAR szStoredRoot[MAX_PATH];
	if (!CDLibraryReaderString(&Payload, szStoredRoot, MAX_PATH) || _tcsicmp(szStoredRoot, pszRoot) || nStoredRecursive != (UINT32)(bRecursive ? 1 : 0)) {
		free_s((void**)&pFile);
		return NULL;
	}

	CDLibrary* pLibrary = CDLibraryCreate(pszRoot, bRecursive);
	if (!pLibrary) {
		free_s((void**)&pFile);
		return NULL;
	}
	for (UINT32 i = 0; i < nEntryCount; i++) {
		CDLibraryEntry Entry;
		if (!CDLibraryDeserializeEntry(&Payload, &Entry) || !CDLibraryPlatformEnabled(Entry.nPlatform) || !CDLibraryAddEntryMove(pLibrary, &Entry)) {
			CDLibraryEntryFree(&Entry);
			CDLibraryFree(pLibrary);
			free_s((void**)&pFile);
			return NULL;
		}
	}
	if (Payload.nOffset != Payload.nLength) {
		CDLibraryFree(pLibrary);
		pLibrary = NULL;
	}
	free_s((void**)&pFile);
	return pLibrary;
}

static INT32 CDLibrarySaveCache(const TCHAR* pszCachePath, const CDLibrary* pLibrary)
{
	if (!pszCachePath || !pLibrary || pLibrary->nCount > CDLIBRARY_MAX_ENTRIES || (pLibrary->nCount && !pLibrary->pEntries))
		return 0;

	CDLibraryBuffer Payload = { NULL, 0, 0 };
	if (!CDLibraryBufferString(&Payload, pLibrary->szRoot))
		return 0;
	for (UINT32 i = 0; i < pLibrary->nCount; i++) {
		if (!CDLibrarySerializeEntry(&Payload, &pLibrary->pEntries[i])) {
			free_s((void**)&Payload.pData);
			return 0;
		}
	}

	CDLibraryBuffer File = { NULL, 0, 0 };
	UINT32 nCrc = CDLibraryCrc32(Payload.pData, Payload.nLength);
	INT32 bBuilt = CDLibraryBufferBytes(&File, CDLibraryMagic, 8) && CDLibraryBufferU32(&File, CDLIBRARY_SCHEMA_VERSION) &&
		CDLibraryBufferU32(&File, CDLIBRARY_IDENTIFY_VERSION) && CDLibraryBufferU32(&File, pLibrary->bRecursive ? 1 : 0) &&
		CDLibraryBufferU32(&File, pLibrary->nCount) && CDLibraryBufferU32(&File, Payload.nLength) && CDLibraryBufferU32(&File, nCrc) &&
		CDLibraryBufferBytes(&File, Payload.pData, Payload.nLength) && CDLibraryBufferBytes(&File, CDLibraryTrailer, 8);
	free_s((void**)&Payload.pData);
	if (!bBuilt) {
		free_s((void**)&File.pData);
		return 0;
	}

	TCHAR szDirectory[MAX_PATH];
	CDLibraryCopyText(szDirectory, MAX_PATH, pszCachePath);
	TCHAR* pszSeparator = _tcsrchr(szDirectory, _T('\\'));
	if (pszSeparator) {
		*pszSeparator = _T('\0');
		CreateDirectory(szDirectory, NULL);
	}

	TCHAR szTemp[MAX_PATH];
	INT32 nTempLength = _sntprintf(szTemp, MAX_PATH, _T("%s.%lu.tmp"), pszCachePath, GetCurrentProcessId());
	if (nTempLength < 0 || nTempLength >= MAX_PATH) {
		free_s((void**)&File.pData);
		return 0;
	}
	FILE* pFile = _tfopen(szTemp, _T("wb"));
	if (!pFile) {
		free_s((void**)&File.pData);
		return 0;
	}
	INT32 bWritten = fwrite(File.pData, 1, File.nLength, pFile) == File.nLength && fflush(pFile) == 0;
	if (bWritten) {
		intptr_t nHandle = _get_osfhandle(_fileno(pFile));
		bWritten = nHandle != -1 && FlushFileBuffers((HANDLE)nHandle);
	}
	free_s((void**)&File.pData);
	if (fclose(pFile))
		bWritten = 0;
	if (!bWritten || !MoveFileEx(szTemp, pszCachePath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		DeleteFile(szTemp);
		return 0;
	}
	return 1;
}

static INT32 CDLibraryNormalizeRoot(const TCHAR* pszRoot, TCHAR* pszNormalized)
{
	if (!pszRoot || !pszRoot[0] || !pszNormalized)
		return 0;

	DWORD nLength = GetFullPathName(pszRoot, MAX_PATH, pszNormalized, NULL);
	if (!nLength || nLength >= MAX_PATH)
		return 0;

	while (nLength > 3 && (pszNormalized[nLength - 1] == _T('\\') || pszNormalized[nLength - 1] == _T('/')))
		pszNormalized[--nLength] = _T('\0');

	DWORD nAttributes = GetFileAttributes(pszNormalized);
	return nAttributes != INVALID_FILE_ATTRIBUTES && (nAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

INT32 CDLibraryScan(const TCHAR* pszRoot, INT32 bRecursive, CDLibraryCancelCallback pCancelCallback, CDLibraryProgressCallback pProgressCallback, void* pUser, CDLibrary** ppLibrary)
{
	if (!ppLibrary)
		return CDLIBRARY_SCAN_INVALID_ARGUMENT;
	*ppLibrary = NULL;

	TCHAR szRoot[MAX_PATH];
	if (!CDLibraryNormalizeRoot(pszRoot, szRoot))
		return CDLIBRARY_SCAN_INVALID_ARGUMENT;
	if (CDLibraryIsCancelled(pCancelCallback, pUser))
		return CDLIBRARY_SCAN_CANCELLED;

	CDLibraryCandidates Candidates = { NULL, 0, 0 };
	if (pProgressCallback)
		pProgressCallback(CDLIBRARY_PHASE_ENUMERATE, 0, 0, pUser);
	INT32 nStatus = CDLibraryEnumerate(szRoot, bRecursive ? 1 : 0, &Candidates, pCancelCallback, pProgressCallback, pUser);
	if (nStatus != CDLIBRARY_SCAN_OK) {
		free_s((void**)&Candidates.pPaths);
		return nStatus;
	}

	TCHAR szCachePath[MAX_PATH];
	CDLibrary* pCache = CDLibraryGetCachePath(szCachePath, MAX_PATH) ? CDLibraryLoadCache(szCachePath, szRoot, bRecursive) : NULL;
	if (pCache && pCache->nCount > 1)
		qsort(pCache->pEntries, pCache->nCount, sizeof(CDLibraryEntry), CDLibraryComparePath);
	CDLibrary* pLibrary = CDLibraryCreate(szRoot, bRecursive);
	if (!pLibrary) {
		CDLibraryFree(pCache);
		free_s((void**)&Candidates.pPaths);
		return CDLIBRARY_SCAN_OUT_OF_MEMORY;
	}

	UINT32 nCacheHits = 0;
	INT32 bCacheDirty = pCache == NULL;
	if (pProgressCallback)
		pProgressCallback(CDLIBRARY_PHASE_IDENTIFY, Candidates.nCount, 0, pUser);
	for (UINT32 i = 0; i < Candidates.nCount; i++) {
		if (CDLibraryIsCancelled(pCancelCallback, pUser)) {
			nStatus = CDLIBRARY_SCAN_CANCELLED;
			break;
		}

		const CDLibraryEntry* pCachedEntry = CDLibraryFindCached(pCache, Candidates.pPaths[i]);
		if (pCachedEntry && CDLibraryStampsValid(pCachedEntry)) {
			nCacheHits++;
			if (!CDLibraryAddEntryCopy(pLibrary, pCachedEntry)) {
				nStatus = CDLIBRARY_SCAN_OUT_OF_MEMORY;
				break;
			}
		} else {
			if (pCachedEntry)
				bCacheDirty = 1;
			CDLibraryEntry Entry;
			CDListResult   Result;
			memset(&Entry, 0, sizeof(Entry));
			CDLibraryIdentifyContext Context = { &Entry, pCancelCallback, pUser, 0, 0 };
			INT32 bIdentified = CDListIdentifyEx(Candidates.pPaths[i], &Result, CDLIST_IDENTIFY_FAST, CDLibraryIdentifyCancel, CDLibraryIdentifySource, &Context);
			if (CDLibraryIsCancelled(pCancelCallback, pUser)) {
				CDLibraryEntryFree(&Entry);
				nStatus = CDLIBRARY_SCAN_CANCELLED;
				break;
			}
			if (Context.bOutOfMemory) {
				CDLibraryEntryFree(&Entry);
				nStatus = CDLIBRARY_SCAN_OUT_OF_MEMORY;
				break;
			}
			if (bIdentified && !Context.bStampError && Entry.nStampCount && CDLibraryPlatformEnabled(Result.nPlatform)) {
				CDLibraryFillEntry(&Entry, Candidates.pPaths[i], &Result);
				if (!pCachedEntry)
					bCacheDirty = 1;
				if (!CDLibraryAddEntryMove(pLibrary, &Entry)) {
					CDLibraryEntryFree(&Entry);
					nStatus = CDLIBRARY_SCAN_OUT_OF_MEMORY;
					break;
				}
			}
			CDLibraryEntryFree(&Entry);
		}
		if (pProgressCallback)
			pProgressCallback(CDLIBRARY_PHASE_IDENTIFY, Candidates.nCount, i + 1, pUser);
	}

	if (pCache && nCacheHits != pCache->nCount)
		bCacheDirty = 1;

	CDLibraryFree(pCache);
	free_s((void**)&Candidates.pPaths);

	if (nStatus != CDLIBRARY_SCAN_OK) {
		CDLibraryFree(pLibrary);
		return nStatus;
	}

	if (CDLibraryIsCancelled(pCancelCallback, pUser)) {
		CDLibraryFree(pLibrary);
		return CDLIBRARY_SCAN_CANCELLED;
	}
	if (bCacheDirty && CDLibraryGetCachePath(szCachePath, MAX_PATH))
		CDLibrarySaveCache(szCachePath, pLibrary);
	if (pProgressCallback)
		pProgressCallback(CDLIBRARY_PHASE_COMPLETE, Candidates.nCount, Candidates.nCount, pUser);
	*ppLibrary = pLibrary;
	return CDLIBRARY_SCAN_OK;
}
