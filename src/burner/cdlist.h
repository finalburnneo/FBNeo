#ifndef CDLIST_H_
#define CDLIST_H_

#define CDLIST_SHA1_SIZE (20)
#define CDLIST_TEXT_SIZE (256)

enum CDListPlatform {
	CDLIST_PLATFORM_UNKNOWN = 0,
	CDLIST_PLATFORM_NEOCD,
	CDLIST_PLATFORM_PCECD,
	CDLIST_PLATFORM_PCFX
};

enum CDListSource {
	CDLIST_SOURCE_NONE = 0,
	CDLIST_SOURCE_MAGIC,
	CDLIST_SOURCE_NEO_ID,
	CDLIST_SOURCE_CHD_SHA1,
	CDLIST_SOURCE_RAW_SHA1
};

enum CDListConfidence {
	CDLIST_CONFIDENCE_NONE = 0,
	CDLIST_CONFIDENCE_POSSIBLE,
	CDLIST_CONFIDENCE_STRONG,
	CDLIST_CONFIDENCE_EXACT
};

enum CDListIdentifyFlags {
	CDLIST_IDENTIFY_NONE = 0,
	CDLIST_IDENTIFY_FAST = 1 << 0
};

typedef INT32 (*CDListCancelCallback)(void* pUser);
typedef void (*CDListSourceCallback)(const TCHAR* pszPath, void* pUser);

struct CDListMetadata {
	TCHAR szName[CDLIST_TEXT_SIZE];
	TCHAR szTitle[CDLIST_TEXT_SIZE];
	TCHAR szYear[32];
	TCHAR szCompany[CDLIST_TEXT_SIZE];
	TCHAR szRegion[64];
	TCHAR szRequirement[CDLIST_TEXT_SIZE];
	TCHAR szDescriptor[CDLIST_TEXT_SIZE];
};

struct CDListResult {
	INT32 nPlatform;
	INT32 nSource;
	INT32 nConfidence;
	UINT32 nNeoID;
	INT32 nAudioTrackCount;
	TCHAR szFirstDataTrackPath[MAX_PATH];
	CDListMetadata Metadata;
	INT32 bHasChdSha1;
	UINT8 ChdSha1[CDLIST_SHA1_SIZE];
	INT32 bChdMameMatch;
	CDListMetadata ChdMetadata;
	INT32 bHasRawSha1;
	INT32 bRawSha1Unsupported;
	UINT8 RawSha1[CDLIST_SHA1_SIZE];
};

INT32 CDListIdentifyEx(const TCHAR* pszPath, CDListResult* pResult, UINT32 nFlags, CDListCancelCallback pCancelCallback, CDListSourceCallback pSourceCallback, void* pUser);
INT32 CDListIdentify(const TCHAR* pszPath, CDListResult* pResult);

#endif
