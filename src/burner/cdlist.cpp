#include "burner.h"
#include "cdlist.h"
#include "neocdlist.h"
#include "pcecdlist.h"
#include "cd_img.h"
#include "neocdlist_games.h"
#include "pcecdlist_games.h"

static NGCDGAME* pNeoGame;

static void CopyText(TCHAR* pszDest, UINT32 nDest, const TCHAR* pszSource)
{
	if (!pszDest || !nDest)
		return;

	if (!pszSource)
		pszSource = _T("");
	_tcsncpy(pszDest, pszSource, nDest - 1);
	pszDest[nDest - 1] = 0;
}

static TCHAR* DuplicateText(const TCHAR* pszSource)
{
	if (!pszSource)
		return NULL;

	UINT32 nLength = (UINT32)_tcslen(pszSource) + 1;
	TCHAR* pszDest = (TCHAR*)malloc(nLength * sizeof(TCHAR));
	if (pszDest)
		memcpy(pszDest, pszSource, nLength * sizeof(TCHAR));
	return pszDest;
}

static INT32 FindTextI(const TCHAR* pszText, const TCHAR* pszNeedle)
{
	if (!pszText || !pszNeedle || !*pszNeedle)
		return 0;

	for (; *pszText; pszText++) {
		const TCHAR* pText   = pszText;
		const TCHAR* pNeedle = pszNeedle;
		while (*pText && *pNeedle && _totlower(*pText) == _totlower(*pNeedle)) {
			pText++; pNeedle++;
		}
		if (!*pNeedle)
			return 1;
	}
	return 0;
}

static UINT32 ReadLE32(const UINT8* pData)
{
	return (UINT32)pData[0] | ((UINT32)pData[1] << 8) | ((UINT32)pData[2] << 16) | ((UINT32)pData[3] << 24);
}

static INT32 ReadSector(CDImage* pImage, INT32 nLba, UINT8* pData, INT32* pnSize)
{
	INT32 nSize = 0;
	if (CDImageReadUserSector(pImage, nLba, pData, &nSize))
		return 0;

	if (pnSize)
		*pnSize = nSize;
	return nSize > 0;
}

static const CDImageTrack* FindDataTrack(const CDImage* pImage)
{
	for (INT32 i = 0; i < CDImageGetTrackCount(pImage); i++) {
		const CDImageTrack* pTrack = CDImageGetTrack(pImage, i);
		if (pTrack && pTrack->nType != CDIMAGE_TRACK_AUDIO)
			return pTrack;
	}
	return NULL;
}

NGCDGAME* GetNeoGeoCDInfo(UINT32 nID)
{
	for (UINT32 i = 0; i < sizeof(games) / sizeof(games[0]); i++) {
		if (games[i].id == nID)
			return &games[i];
	}
	return NULL;
}

void FreeNGCDGame(NGCDGAME** ppGame)
{
	if (!ppGame || !*ppGame)
		return;

	free_s((void**)&((*ppGame)->pszName));
	free_s((void**)&((*ppGame)->pszTitle));
	free_s((void**)&((*ppGame)->pszYear));
	free_s((void**)&((*ppGame)->pszCompany));
	free(*ppGame); *ppGame = NULL;
}

INT32 GetNGCDGameTitle(const UINT32 nGameID, NGCDGAME** ppOutGame, bool bPrintLog)
{
	if (!ppOutGame)
		return 0;

	*ppOutGame = NULL;
	NGCDGAME* pSource = GetNeoGeoCDInfo(nGameID);
	if (!pSource)
		return 0;

	NGCDGAME* pGame = (NGCDGAME*)calloc(1, sizeof(NGCDGAME));
	if (!pGame)
		return 0;

	pGame->id         = pSource->id;
	pGame->pszName    = DuplicateText(pSource->pszName);
	pGame->pszTitle   = DuplicateText(pSource->pszTitle);
	pGame->pszYear    = DuplicateText(pSource->pszYear);
	pGame->pszCompany = DuplicateText(pSource->pszCompany);
	if (!pGame->pszName || !pGame->pszTitle || !pGame->pszYear || !pGame->pszCompany) {
		FreeNGCDGame(&pGame);
		return 0;
	}
	if (bPrintLog) {
		bprintf(PRINT_NORMAL, _T("    Title: %s\n"    ), pGame->pszTitle);
		bprintf(PRINT_NORMAL, _T("    Shortname: %s\n"), pGame->pszName);
		bprintf(PRINT_NORMAL, _T("    Year: %s\n"     ), pGame->pszYear);
		bprintf(PRINT_NORMAL, _T("    Company: %s\n"  ), pGame->pszCompany);
	}
	*ppOutGame = pGame;
	return 1;
}

static UINT32 ReviseNeoID(UINT32 nID, const UINT8* pDate, const UINT8* pData, INT32 bGotDDPRG, const char* pszVolume, const TCHAR* pszPath)
{
	if ((nID == 0x0016 || nID == 0x0025 || nID == 0x0076 || nID == 0x0062) && pDate[0] == 94 && pDate[1] == 12 && pDate[2] == 20) nID |= 0x1000;	// Justin Gibbons Hacks (kotm | eightman |zedblade | spinmast)
	if ( nID == 0x0059 && pDate[0] ==  95 && pDate[1] ==  6 && pDate[2] == 20) nID |= 0x1000;														// Savage Reign Rev 1
	if ( nID == 0x0066 && pDate[0] == 125 && pDate[1] ==  4 && pDate[2] == 10) nID |= 0x1200;														// Digger Man (Prototype)
	if ( nID == 0x069c && pDate[0] ==  95 && pDate[1] ==  4 && pDate[2] == 29) nID |= 0x1000;														// Fatal Fury 3 Rev 1
	if ( nID == 0x069c && pDate[0] ==  95 && pDate[1] ==  6 && pDate[2] ==  1) nID |= 0x2000;														// Fatal Fury 3 Rev 2
	if ( nID == 0x069c && pDate[0] ==  95 && pDate[1] ==  7 && pDate[2] == 10) nID |= 0x3000;														// Fatal Fury 3 Rev 3
	if ( nID == 0x0090 && pDate[0] ==  95 && pDate[1] ==  7 && pDate[2] == 21) nID |= 0x1000;														// World Heroes Perfect
	if ( nID == 0x0058 && pDate[0] ==  94 && pDate[1] == 10 && pDate[2] == 14) nID |= 0x1000;														// Fatal Fury Special Rev 1
	if ( nID == 0x0052 && pDate[0] == 123 && pDate[1] ==  7 && pDate[2] ==  1) nID |= 0x1000;														// Abyssal Infants
	if ( nID == 0x0052 && pDate[0] == 123 && pDate[1] ==  6 && pDate[2] == 20) nID |= 0x1001;														// Neo Fight
	if ( nID == 0x0082) {
		if (bGotDDPRG) nID |= 0x1000;		// Double Dragon Rev 1
		else if (FindTextI(pszPath, _T("OST")) || FindTextI(pszPath, _T("PS")) || FindTextI(pszPath, _T("PlayStation"))) nID |= 0x2000;				// Double Dragon PS1 OST
	}
	if (nID == 0x0085) {
		if (     pDate[0] == 123 && pDate[1] == 11 && pDate[2] == 29) nID |= 0x1000;																// Samurai Shodown RPG (English Translation)
		else if (pDate[0] == 124 && pDate[1] ==  1 && pDate[2] == 26) nID |= 0x3000;																// Samurai Shodown RPG (English Translation v1.1)
		else if (pDate[0] == 126 && pDate[1] ==  6 && pDate[2] ==  1) nID |= 0x4000;																// Samurai Shodown RPG (Simplified Chinese Translation v1.1)
		else if (FindTextI(pszPath, _T("FR")) || FindTextI(pszPath, _T("French"))) nID |= 0x2000;													// Samurai Shodown RPG (FR)
	}
	if (nID == 0x5345 && (!strcmp(pszVolume, "BLUEANDREDFIGHTTHEROBOTS") || FindTextI(pszPath, _T("NTSC")))) nID = 0x5346;							// Blue And Red - Fight The Robots! (NTSC)
	if (nID == 0x1234 && pDate[0] == 105 && pDate[1] ==  4 && pDate[2] == 25) nID = 0x2234;															// Neo Puzzle League
	if (nID == 0x1234 && pDate[0] == 124 && pDate[1] == 12 && pDate[2] ==  2) nID = 0x2235;															// Neo Tetris
	if (nID == 0x1234 && pDate[0] == 112 && pDate[1] ==  3 && pDate[2] ==  4) nID = 0x2236;															// NGD::ARK
	if (nID == 0x1234 && pDate[0] == 112 && pDate[1] == 12 && pDate[2] ==  4) nID = 0x2237;															// Santa Ball
	if (nID == 0x2000 && !strcmp(pszVolume, "COLUMNS")) nID |= 0x1000;																				// Columns
	if (nID == 0xffff && !strcmp(pszVolume, "CODENAME BLUT ENGEL")) nID = 0xfffe;																	// Codename Blut Engel
	if (nID == 0x7777 && pDate[0] == 114 && pDate[1] ==  8 && pDate[2] == 14) nID = 0x7778;															// Puzzle de Pon! CD Collection
	if (nID == 0x2019 && !strcmp(pszVolume, "LOOPTRSP")) nID |= 0x0100;																				// Looptris Plus
	if (nID == 0x0048 && pData[0x67] == 0x08) nID |= 0x1000;																						// Treasure of Caribbean (c) 1994 / (c) 2011 NCI
	if (nID == 0x0055 && pData[0x67] == 0xdE)	/* 10-6-1994 (P1.PRG)  */	{/* ...continue*/ }														// King of Fighters '94, The (1994)(SNK)(JP)
	if (nID == 0x0055 && pData[0x67] == 0xe6)	/* 11-21-1994 (P1.PRG) */	nID |= 0x1000;															// King of Fighters '94, The (1994)(SNK)(JP-US)
	if (nID == 0x0084 && pData[0x6C] == 0xc0)	/* 9-11-1995 (P1.PRG)  */	{/* ...continue*/ }														// King of Fighters '95, The (1995)(SNK)(JP-US)[!][NGCD-084 MT B01, B03-B06, NGCD-084E MT B01]
	if (nID == 0x0084 && pData[0x6c] == 0xff)	/* 10-5-1995 (P1.PRG)  */	nID |= 0x1000;															// King of Fighters '95, The (1995)(SNK)(JP-US)[!][NGCD-084 MT B10, NGCD-084E MT B03]
	return nID;
}

static INT32 IdentifyNeo(CDImage* pImage, const TCHAR* pszPath, CDListResult* pResult)
{
	const CDImageTrack* pTrack = FindDataTrack(pImage);
	if (!pTrack)
		return 0;

	UINT8 pvd[2448] = { 0 };
	INT32 nSize = 0;
	if (!ReadSector(pImage, pTrack->nIndex1LBA + 16, pvd, &nSize) || nSize < 190 || memcmp(pvd + 1, "CD001", 5))
		return 0;

	char szVolume[33] = { 0 };
	memcpy(szVolume, pvd + 40, 32);
	for (INT32 i = 31; i >= 0 && szVolume[i] == ' '; i--)
		szVolume[i] = 0;
	UINT32 nRootLba  = ReadLE32(pvd + 158);
	UINT32 nRootSize = ReadLE32(pvd + 166);
	if (!nRootLba || !nRootSize)
		return 0;

	INT32  bGotDDPRG = 0;
	INT32  bQueued   = 0;
	UINT32 nQueuedID = 0;
	for (UINT32 nPosition = 0; nPosition < nRootSize;) {
		UINT8  sector[2448] = { 0 };
		INT32  nSectorSize  = 0;
		UINT32 nSector = nPosition / 2048;
		UINT32 nOffset = nPosition % 2048;
		if (!ReadSector(pImage, pTrack->nIndex1LBA + (INT32)nRootLba + (INT32)nSector, sector, &nSectorSize)) break;
		if (nOffset >= (UINT32)nSectorSize)
			break;
		UINT8 nLength = sector[nOffset];
		if (!nLength) {
			nPosition = (nPosition + 2048) & ~2047U;
			continue;
		}
		if (nOffset + nLength > (UINT32)nSectorSize || nLength < 34)
			break;
		const UINT8* pRecord = sector + nOffset;
		UINT8 nNameLength = pRecord[32];
		if (!(pRecord[25] & 2) && nNameLength && 33 + nNameLength <= nLength) {
			char szName[256] = { 0 };
			UINT32 nCopy = nNameLength < sizeof(szName) - 1 ? nNameLength : sizeof(szName) - 1;
			memcpy(szName, pRecord + 33, nCopy);
			char* pVersion = strchr(szName, ';');
			if (pVersion)
				*pVersion = 0;
			if (!strcmp(szName, "DDPRG.ACM"))
				bGotDDPRG = 1;
			UINT32 nFileLba   = ReadLE32(pRecord + 2);
			UINT8  data[2448] = { 0 };
			INT32  nDataSize  = 0;
			if (ReadSector(pImage, pTrack->nIndex1LBA + (INT32)nFileLba, data, &nDataSize) && nDataSize >= 0x10a && !memcmp(data + 0x100, "NEO-GEO", 7)) {
				UINT32 nID = ((UINT32)data[0x108] << 8) | data[0x109];
				nID = ReviseNeoID(nID, pRecord + 18, data, bGotDDPRG, szVolume, pszPath);
				if (nID == 0x0229) {
					pResult->nNeoID = nID;
					return 1;
				}
				if ( nID == 0x0214 ||																		// King of Fighters '96, The
					(nID == 0x0058 && !(pRecord[18] == 94 && pRecord[19] == 8 && pRecord[20] == 5))) {		// !Fatal Fury Special Rev 0
					// continue checking other files...
					bQueued = 1;
					nQueuedID = nID;
				} else {
					pResult->nNeoID = nID;
					return 1;
				}
			}
		}
		nPosition += nLength;
	}
	if (bQueued) {
		pResult->nNeoID = nQueuedID;
		return 1;
	}
	return 0;
}

static UINT32 CalculateCrc32(const UINT8* pData, UINT32 nLength)
{
	UINT32 nCrc = 0xffffffff;
	for (UINT32 i = 0; i < nLength; i++) {
		nCrc ^= pData[i];
		for (INT32 j = 0; j < 8; j++)
			nCrc = (nCrc >> 1) ^ (0xedb88320U & (0U - (nCrc & 1)));
	}
	return nCrc ^ 0xffffffff;
}

static INT32 IsGamesExpress(CDImage* pImage, const CDImageTrack* pTrack, UINT8* pSector)
{
	INT32 nSize = 0;
	if (!ReadSector(pImage, pTrack->nIndex1LBA + 0x10, pSector, &nSize) || nSize < 0x1c)	return 0;
	if (!memcmp(pSector + 8, "HACKER CD ROM SYSTEM", 20))									return 1;
	if (memcmp(pSector + 1, "CD001", 5))													return 0;
	if (!ReadSector(pImage, pTrack->nIndex1LBA + 0x14, pSector, &nSize) || nSize != 2048)	return 0;
	UINT32 nCrc = CalculateCrc32(pSector, 2048);
	return nCrc == 0xd7b47c06 || nCrc == 0x86aec522 || nCrc == 0xc8d1b5ef || nCrc == 0x0bdbde64;
}

static INT32 IdentifyPlatform(CDImage* pImage, CDListResult* pResult)
{
	static const UINT8 pPceMagic[32] = {
		0x82, 0xb1, 0x82, 0xcc, 0x83, 0x76, 0x83, 0x8d,
		0x83, 0x4f, 0x83, 0x89, 0x83, 0x80, 0x82, 0xcc,
		0x92, 0x98, 0x8d, 0xec, 0x8c, 0xa0, 0x82, 0xcd,
		0x8a, 0x94, 0x8e, 0xae, 0x89, 0xef, 0x8e, 0xd0
	};
	UINT8 sector[2448] = { 0 };
	for (INT32 i = 0; i < CDImageGetTrackCount(pImage); i++) {
		const CDImageTrack* pTrack = CDImageGetTrack(pImage, i);
		INT32 nSize = 0;
		if (pTrack && pTrack->nType != CDIMAGE_TRACK_AUDIO && ReadSector(pImage, pTrack->nIndex1LBA, sector, &nSize) && nSize >= 15 && !memcmp(sector, "PC-FX:Hu_CD-ROM", 15)) {
			pResult->nPlatform   = CDLIST_PLATFORM_PCFX;
			pResult->nSource     = CDLIST_SOURCE_MAGIC;
			pResult->nConfidence = CDLIST_CONFIDENCE_EXACT;
			CopyText(pResult->Metadata.szDescriptor, CDLIST_TEXT_SIZE, _T("PC-FX boot signature"));
			return 1;
		}
	}
	const CDImageTrack* pTrack = FindDataTrack(pImage);
	if (!pTrack)
		return 0;

	INT32 nSize = 0;
	if (ReadSector(pImage, pTrack->nIndex1LBA, sector, &nSize) && nSize >= 32 && !memcmp(sector, pPceMagic, sizeof(pPceMagic))) {
		pResult->nPlatform   = CDLIST_PLATFORM_PCECD;
		pResult->nSource     = CDLIST_SOURCE_MAGIC;
		pResult->nConfidence = CDLIST_CONFIDENCE_STRONG;
		CopyText(pResult->Metadata.szRequirement, CDLIST_TEXT_SIZE, _T("PC Engine CD System Card"));
		CopyText(pResult->Metadata.szDescriptor,  CDLIST_TEXT_SIZE, _T("PC Engine program copyright header"));
		return 1;
	}
	if (ReadSector(pImage, pTrack->nIndex1LBA + 1, sector, &nSize) && nSize >= 55 && !memcmp(sector + 0x20, "PC Engine CD-ROM SYSTEM", 23)) {
		pResult->nPlatform   = CDLIST_PLATFORM_PCECD;
		pResult->nSource     = CDLIST_SOURCE_MAGIC;
		pResult->nConfidence = CDLIST_CONFIDENCE_EXACT;
		CopyText(pResult->Metadata.szRequirement, CDLIST_TEXT_SIZE, _T("PC Engine CD System Card"));
		CopyText(pResult->Metadata.szDescriptor,  CDLIST_TEXT_SIZE, _T("PC Engine CD-ROM SYSTEM header"));
		return 1;
	}
	if (IsGamesExpress(pImage, pTrack, sector)) {
		pResult->nPlatform   = CDLIST_PLATFORM_PCECD;
		pResult->nSource     = CDLIST_SOURCE_MAGIC;
		pResult->nConfidence = CDLIST_CONFIDENCE_EXACT;
		CopyText(pResult->Metadata.szRequirement, CDLIST_TEXT_SIZE, _T("Games Express CD Card"));
		CopyText(pResult->Metadata.szDescriptor,  CDLIST_TEXT_SIZE, _T("Games Express disc signature"));
		return 1;
	}
	return 0;
}

static void FillNeoMetadata(CDListResult* pResult)
{
	NGCDGAME* pGame = GetNeoGeoCDInfo(pResult->nNeoID);
	if (!pGame)
		return;

	CopyText(pResult->Metadata.szName,       CDLIST_TEXT_SIZE, pGame->pszName);
	CopyText(pResult->Metadata.szTitle,      CDLIST_TEXT_SIZE, pGame->pszTitle);
	CopyText(pResult->Metadata.szYear,       32,               pGame->pszYear);
	CopyText(pResult->Metadata.szCompany,    CDLIST_TEXT_SIZE, pGame->pszCompany);
	CopyText(pResult->Metadata.szDescriptor, CDLIST_TEXT_SIZE, _T("Neo Geo CD program header"));
}

INT32 CDListIdentifyEx(const TCHAR* pszPath, CDListResult* pResult, UINT32 nFlags, CDListCancelCallback pCancelCallback, CDListSourceCallback pSourceCallback, void* pUser)
{
	if (!pszPath || !pResult)
		return 0;

	memset(pResult, 0, sizeof(*pResult));
	if (pCancelCallback && pCancelCallback(pUser))
		return 0;

	CDImage* pImage = CDImageOpen(pszPath);
	if (!pImage)
		return 0;

	pResult->nAudioTrackCount = CDImageGetAudioTrackCount(pImage);
	const CDImageTrack* pDataTrack = FindDataTrack(pImage);
	if (pDataTrack)
		CopyText(pResult->szFirstDataTrackPath, MAX_PATH, pDataTrack->szPath);
	for (INT32 i = 0; i < CDImageGetSourceFileCount(pImage); i++) {
		if (pSourceCallback)
			pSourceCallback(CDImageGetSourceFile(pImage, i), pUser);
	}

	INT32 bCancelled = 0;

	UINT8 sha1s[40] = { 0, };
	if (!CDImageGetTOCSha1(pImage, sha1s)) {
		_tcscpy(pResult->szTOCSha1, _AtoT((char *)sha1s));
		//bprintf(0, _T("CDImageGetTOCSha1() res %x  sha1s[%s]  %s\n"), res, pResult->szTOCSha1, pszPath);
	}

	if (bCancelled || (pCancelCallback && pCancelCallback(pUser))) {
		CDImageClose(pImage);
		return 0;
	}

	if (IdentifyNeo(pImage, pszPath, pResult)) {
		pResult->nPlatform   = CDLIST_PLATFORM_NEOCD;
		pResult->nSource     = CDLIST_SOURCE_GAMEDB;
		pResult->nConfidence = CDLIST_CONFIDENCE_EXACT;
		FillNeoMetadata(pResult);
	} else {
		IdentifyPlatform(pImage, pResult);

		for (UINT32 i = 0; i < sizeof(pcengine_cd_games) / sizeof(pce_cd_dink); i++) {
			if ((pcengine_cd_games[i].sha1[0] != '\0') && !_tcscmp(pcengine_cd_games[i].sha1, pResult->szTOCSha1)) {
				//bprintf(0, _T("->>found entry at idx %d\n"), i);

				_tcscpy(pResult->Metadata.szTitle, pcengine_cd_games[i].name);
				_tcscpy(pResult->Metadata.szName, pcengine_cd_games[i].id);
				_tcscpy(pResult->Metadata.szYear, pcengine_cd_games[i].year);
				_tcscpy(pResult->Metadata.szCompany, pcengine_cd_games[i].company);

				pResult->nPlatform   = CDLIST_PLATFORM_PCECD;
				pResult->nSource     = CDLIST_SOURCE_GAMEDB;
				pResult->nConfidence = CDLIST_CONFIDENCE_EXACT;
				break;
			}
		}
	}
	CDImageClose(pImage);
	return pResult->nPlatform != CDLIST_PLATFORM_UNKNOWN;
}

INT32 CDListIdentify(const TCHAR* pszPath, CDListResult* pResult)
{
	return CDListIdentifyEx(pszPath, pResult, CDLIST_IDENTIFY_NONE, NULL, NULL, NULL);
}

INT32 NeoCDList_CheckISO(TCHAR* pszFile, void (*pfEntryCallBack)(INT32, TCHAR*))
{
	CDListResult Result;
	if (CDListIdentify(pszFile, &Result) && Result.nPlatform == CDLIST_PLATFORM_NEOCD) {
		if (pfEntryCallBack) pfEntryCallBack((INT32)Result.nNeoID, pszFile);
		return 1;
	}
	return 0;
}

INT32 GetNeoCDTitle(UINT32 nGameID)
{
	FreeNGCDGame(&pNeoGame);
	return GetNGCDGameTitle(nGameID, &pNeoGame, true);
}

INT32 GetNeoGeoCD_Identifier()
{
#ifdef BUILD_NEOGEO
	TCHAR* pszPath = GetIsoPath();
	if (!pszPath || !IsNeoGeoCD())
		return 0;

	CDListResult Result;
	if (CDListIdentify(pszPath, &Result) && Result.nPlatform == CDLIST_PLATFORM_NEOCD) {
		GetNeoCDTitle(Result.nNeoID);
		return 1;
	}
	return 0;
#else
	return 0;
#endif
}

#ifdef BUILD_NEOGEO
INT32 NeoCDInfo_Init()
{
	NeoCDInfo_Exit();
	return GetNeoGeoCD_Identifier();
}

TCHAR* NeoCDInfo_Text(INT32 nText)
{
	if (!pNeoGame || !IsNeoGeoCD())
		return NULL;

	switch (nText) {
		case DRV_NAME:         return pNeoGame->pszName;
		case DRV_FULLNAME:     return pNeoGame->pszTitle;
		case DRV_MANUFACTURER: return pNeoGame->pszCompany;
		case DRV_DATE:         return pNeoGame->pszYear;
	}
	return NULL;
}

INT32 NeoCDInfo_ID()
{
	if (!pNeoGame || !IsNeoGeoCD())
		return 0;
	return (INT32)pNeoGame->id;
}

void NeoCDInfo_Exit()
{
	FreeNGCDGame(&pNeoGame);
}
#endif

#ifdef BUILD_PCE
static CDListMetadata PceCdInfo;
static INT32 nPceCdInfoValid = 0;

static INT32 IsPceCD()
{
	return (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_PCENGINE_PCE_CD;
}

static void PceCdInfoUseFileName(TCHAR* pszDest, UINT32 nDest, const TCHAR* pszPath)
{
	// no database title (e.g. cue/ccd identified by filename only)
	const TCHAR* pszBase = _tcsrchr(pszPath, _T('\\'));
	pszBase = pszBase ? pszBase + 1 : pszPath;
	const TCHAR* pszSlash = _tcsrchr(pszBase, _T('/'));
	if (pszSlash) {
		pszBase = pszSlash + 1;
	}
	CopyText(pszDest, nDest, pszBase);
	TCHAR* pszDot = _tcsrchr(pszDest, _T('.'));
	if (pszDot) {
		*pszDot = _T('\0');
	}
}

INT32 PceCDInfo_Init()
{
	PceCDInfo_Exit();

	if (!IsPceCD() || !CDEmuImage[0]) {
		return 0;
	}

	CDListResult Result;
	if (!CDListIdentifyEx(CDEmuImage, &Result, CDLIST_IDENTIFY_FAST, NULL, NULL, NULL)) {
		return 0;
	}

	if (Result.nPlatform == CDLIST_PLATFORM_PCECD) {
		// platform identified by SHA1 of TOC on bin/cue/etc
		// create title from bin/cue name (for fallback)
		PceCdInfoUseFileName(PceCdInfo.szTitle, CDLIST_TEXT_SIZE, CDEmuImage);

		for (UINT32 i = 0; i < sizeof(pcengine_cd_games) / sizeof(pce_cd_dink); i++) {
			if ((pcengine_cd_games[i].sha1[0] != '\0') && !_tcscmp(pcengine_cd_games[i].sha1, ANSIToTCHAR((char*)CDEmuImageTOCSHA1, NULL, 0))) {
				bprintf(0, _T("pcecdlist_games db: found entry at idx %d\n"), i);

				_tcscpy(PceCdInfo.szTitle, pcengine_cd_games[i].name);
				_tcscpy(PceCdInfo.szName, pcengine_cd_games[i].id);
				_tcscpy(PceCdInfo.szYear, pcengine_cd_games[i].year);
				_tcscpy(PceCdInfo.szCompany, pcengine_cd_games[i].company);

				nPceCdInfoValid = 1;
				break;
			}
		}
	}

	if (nPceCdInfoValid) {
		bprintf(PRINT_NORMAL, _T("    Title: %s\n"    ), PceCdInfo.szTitle);
		bprintf(PRINT_NORMAL, _T("    Shortname: %s\n"), PceCdInfo.szName);
		bprintf(PRINT_NORMAL, _T("    Year: %s\n"     ), PceCdInfo.szYear);
		bprintf(PRINT_NORMAL, _T("    Company: %s\n"  ), PceCdInfo.szCompany);
	}

	return nPceCdInfoValid;
}

TCHAR* PceCDInfo_Text(INT32 nText)
{
	if (!IsPceCD()) {
		return NULL;
	}

	if (nText == DRV_FULLNAME && PceCdInfo.szTitle[0]) {
		// best-effort title: database title, or the file name for unidentified discs
		return PceCdInfo.szTitle;
	}

	if (!nPceCdInfoValid) {  // FALLBACK: create "DRV_NAME" from cue filename....
		if (nText == DRV_NAME) {
			TCHAR c;
			int src = 0;
			int dst = 0;
			int srclen = _tcslen(PceCdInfo.szTitle);
			while (src < srclen && dst < 32) {
				bool ok_to_copy = false;

				c = _totlower(PceCdInfo.szTitle[src++]);

				ok_to_copy = ((c >= _T('a') && c <= _T('z')) ||
							  (c >= _T('0') && c <= _T('9')));

				if (c == _T('-')) break;
				if (c == _T('(')) break;
				if (c == _T('[')) break;

				if (ok_to_copy) {
					PceCdInfo.szName[dst++] = c;
					PceCdInfo.szName[dst] = _T('\0');
				}
			}
			nPceCdInfoValid = 1; // DRV_NAME is now ok -dink
			return PceCdInfo.szName;
		}
		return NULL;
	}

	switch (nText) {
		case DRV_FULLNAME:     	return PceCdInfo.szTitle;
		case DRV_NAME:			return PceCdInfo.szName;
		case DRV_MANUFACTURER:	return PceCdInfo.szCompany;
		case DRV_DATE:			return PceCdInfo.szYear;
	}
	return NULL;
}

INT32 PceCDInfo_ID()
{
	return (IsPceCD() && nPceCdInfoValid) ? 1 : 0;
}

void PceCDInfo_Exit()
{
	memset(&PceCdInfo, 0, sizeof(PceCdInfo));
	nPceCdInfoValid = 0;
}
#endif

TCHAR* CDInfo_Text(INT32 nText)
{
	static TCHAR szNone[] = _T("NOGAME");
	const INT32 g_type = CDGameType();

	switch (g_type) {
		case CDGAME_NEOGEO:
			return NeoCDInfo_Text(nText);
		case CDGAME_PCE:
			return PceCDInfo_Text(nText);
		default:
			return szNone;
	}
}

TCHAR *CDInfo_GamePrefix()
{
	static TCHAR buffer[128];

	TCHAR g_sys_prefixes[3][20] = { _T("none_"), _T("ngcd_"), _T("pcecd_") };
	const INT32 g_type = CDGameType();

	_stprintf(buffer, _T("%s%s"), g_sys_prefixes[g_type], CDInfo_Text(DRV_NAME));

	return buffer;
}

