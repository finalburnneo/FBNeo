#ifndef CDLISTCACHE_H_
#define CDLISTCACHE_H_

#include "cdlist.h"

enum CDLibraryScanStatus {
	CDLIBRARY_SCAN_OK = 0,
	CDLIBRARY_SCAN_CANCELLED,
	CDLIBRARY_SCAN_INVALID_ARGUMENT,
	CDLIBRARY_SCAN_OUT_OF_MEMORY,
	CDLIBRARY_SCAN_IO_ERROR
};

enum CDLibraryScanPhase {
	CDLIBRARY_PHASE_ENUMERATE = 0,
	CDLIBRARY_PHASE_IDENTIFY,
	CDLIBRARY_PHASE_COMPLETE
};

typedef INT32 (*CDLibraryCancelCallback)(void* pUser);
typedef void (*CDLibraryProgressCallback)(INT32 nPhase, UINT32 nCandidateTotal, UINT32 nCompleted, void* pUser);

struct CDLibraryFileStamp {
	TCHAR szPath[MAX_PATH];
	UINT64 nSize;
	UINT64 nWriteTime;
};

struct CDLibraryEntry {
	INT32  nPlatform;
	INT32  nSource;
	INT32  nConfidence;
	UINT32 nNeoID;
	TCHAR  szPath[MAX_PATH];
	TCHAR  szDataPath[MAX_PATH];
	TCHAR  szShortName[CDLIST_TEXT_SIZE];
	TCHAR  szTitle[CDLIST_TEXT_SIZE];
	TCHAR  szCompany[CDLIST_TEXT_SIZE];
	TCHAR  szYear[32];
	TCHAR  szRegion[64];
	TCHAR  szRequirement[CDLIST_TEXT_SIZE];
	INT32  nAudioTrackCount;
	INT32  nIcon;
	CDLibraryFileStamp* pStamps;
	UINT32 nStampCount;
	UINT32 nStampCapacity;
};

struct CDLibrary {
	TCHAR  szRoot[MAX_PATH];
	INT32  bRecursive;
	CDLibraryEntry* pEntries;
	UINT32 nCount;
	UINT32 nCapacity;
};

CDLibrary* CDLibraryCreate(const TCHAR* pszRoot, INT32 bRecursive);
void  CDLibraryEntryFree(CDLibraryEntry* pEntry);
void  CDLibraryFree(CDLibrary* pLibrary);
INT32 CDLibraryAddEntryMove(CDLibrary* pLibrary, CDLibraryEntry* pEntry);
INT32 CDLibraryAddEntryCopy(CDLibrary* pLibrary, const CDLibraryEntry* pEntry);
INT32 CDLibraryScan(const TCHAR* pszRoot, INT32 bRecursive, CDLibraryCancelCallback pCancelCallback, CDLibraryProgressCallback pProgressCallback, void* pUser, CDLibrary** ppLibrary);

#endif
