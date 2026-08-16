#ifndef NEOCDLIST_H_
#define NEOCDLIST_H_

#include "cdlist.h"

struct NGCDGAME
{
	TCHAR* pszName;			// Short name
	TCHAR* pszTitle;		// Title
	TCHAR* pszYear;			// Release Year
	TCHAR* pszCompany;		// Developer
	UINT32 id;				// Game ID
};

NGCDGAME* GetNeoGeoCDInfo(UINT32 nID);
INT32 GetNGCDGameTitle(const UINT32 nGameID, NGCDGAME** ppOutGame, bool bPrintLog);
void  FreeNGCDGame(NGCDGAME** ppGame);
INT32 NeoCDList_CheckISO(TCHAR* pszFile, void (*pfEntryCallBack)(INT32, TCHAR*));
INT32 GetNeoCDTitle(UINT32 nGameID);
INT32 GetNeoGeoCD_Identifier();

#ifdef BUILD_NEOGEO
bool   IsNeoGeoCD();		// neo_run.cpp
TCHAR* GetIsoPath();		// cd_isowav.cpp
INT32  NeoCDInfo_Init();
TCHAR* NeoCDInfo_Text(INT32 nText);
INT32  NeoCDInfo_ID();
void   NeoCDInfo_SetTitle();
void   NeoCDInfo_Exit();
#else
static inline bool   IsNeoGeoCD()          { return false; }
static inline TCHAR* GetIsoPath()          { return NULL; }
static inline INT32  NeoCDInfo_Init()      { return 0; }
static inline TCHAR* NeoCDInfo_Text(INT32) { return NULL; }
static inline INT32  NeoCDInfo_ID()        { return 0; }
static inline void   NeoCDInfo_SetTitle()  { }
static inline void   NeoCDInfo_Exit()      { }
#endif

#endif
