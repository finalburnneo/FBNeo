#ifndef CDLIST_H_
#define CDLIST_H_

#include "burn.h"

#define CDLIST_SHA1_SIZE (20)
#define CDLIST_TEXT_SIZE (256)

// CD game type of the active driver, derived from the hardware code
// (usable at any time while a driver is selected, no disc required)
enum {
	CDGAME_NONE = 0,								// not a CD game
	CDGAME_NEOGEO,									// Neo Geo CD
	CDGAME_PCE										// PC Engine CD
};

static inline INT32 CDGameType()
{
	INT32 nHardware = BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK;
	if (nHardware == HARDWARE_SNK_NEOCD) {
		return CDGAME_NEOGEO;
	}
	if (nHardware == HARDWARE_PCENGINE_PCE_CD) {
		return CDGAME_PCE;
	}
	return CDGAME_NONE;
}

static inline bool IsCDGame()
{
	return CDGameType() != CDGAME_NONE;
}

TCHAR *CDInfo_GamePrefix();
TCHAR* CDInfo_Text(INT32 nText);

enum CDListPlatform {
	CDLIST_PLATFORM_UNKNOWN = 0,
	CDLIST_PLATFORM_NEOCD,
	CDLIST_PLATFORM_PCECD,
	CDLIST_PLATFORM_PCFX
};

enum CDListSource {
	CDLIST_SOURCE_NONE = 0, // not detected
	CDLIST_SOURCE_MAGIC, // identify'd by byte pattern in cd
	CDLIST_SOURCE_GAMEDB // identify'd via neocdlist_games.h / pcecdlist_games.h
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
	TCHAR szTOCSha1[41];
	CDListMetadata Metadata;
};

INT32 CDListIdentifyEx(const TCHAR* pszPath, CDListResult* pResult, UINT32 nFlags, CDListCancelCallback pCancelCallback, CDListSourceCallback pSourceCallback, void* pUser);
INT32 CDListIdentify(const TCHAR* pszPath, CDListResult* pResult);

#endif
