// ---------------------------------------------------------------------------------------
// NeoGeo CD Game Info Module (by CaptainCPS-X)
// ---------------------------------------------------------------------------------------
#include "burner.h"
#include "neocdlist.h"
#include "burnint.h"
#include "cd_chd.h"

#ifdef BUILD_NEOGEO
#define DEBUG_CD    0

const INT32 nSectorLength = 2352;

static NGCDGAME* game;

#include "neocdlist_games.h"

// Distinguish between plain file and CHD I/O types
enum IO_TYPE {
	IO_TYPE_NONE = 0,
	IO_TYPE_FILE,
	IO_TYPE_CHD
};

union IO_UNION {
	FILE					*pFile;				// plain file handle
	ChdImage				*pChd;				// CHD image (cd_chd module)
};

struct ISO_CTX {
	enum IO_TYPE			ioType;				// current I/O type
	union IO_UNION			ioUnion;			// I/O handle union
	char					szVolumeID[64];		// disc volume label (moved from former global)
	INT32					bValid;				// context validity flag

	// Full-image common metadata (parsed once, reused throughout)
	UINT32					sectorSize;			// bytes per sector
	UINT32					totalSectors;		// total image sectors
};

struct IO_STRATEGY
{
	enum IO_TYPE ioType;
	// Open image, initialize context
	INT32 (*fnOpen )(struct ISO_CTX* pIsoCtx, TCHAR* pszFile);
	// Close image, release resources
	void  (*fnClose)(struct ISO_CTX* pIsoCtx);
	// Read data by offset
	void  (*fnRead )(struct ISO_CTX* pIsoCtx, UINT8* Dest, UINT32 lOffset, UINT32 lSize, UINT32 lLength);
};

// TCHAR safe string duplicate (ANSI / Unicode auto-adaptive)
// Allocates heap memory and copies source TCHAR string safely
// Allocated memory must be released by free() or free_s()
static TCHAR* _tcsdup_s(const TCHAR* pSrc)
{
	if (IsStrEmpty(pSrc))
		return NULL;

	size_t srcLen = _tcslen(pSrc);
	size_t allocSize = (srcLen + 1) * sizeof(TCHAR);
	TCHAR* pDst = (TCHAR*)malloc(allocSize);

	if (!pDst)
		return NULL;

	_tcsncpy(pDst, pSrc, srcLen);
	pDst[srcLen] = _T('\0');

	return pDst;
}

static void iso9660_ReadOffset(FILE* fp, UINT8* Dest, UINT32 lOffset, UINT32 lSize, UINT32 lLength)
{
	if (!fp || !Dest) return;
	fseek(fp, lOffset + ((nSectorLength == 2352) ? 16 : 0), SEEK_SET);
	fread(Dest, lLength, lSize, fp);
}

static INT32 FileIO_Open(struct ISO_CTX* pIsoCtx, TCHAR* pszFile)
{
	if (!pIsoCtx || !pszFile) return 0;

	FILE* fp = _tfopen(pszFile, _T("rb"));
	if (!fp) return 0;

	pIsoCtx->ioType        = IO_TYPE_FILE;
	pIsoCtx->ioUnion.pFile = fp;
	pIsoCtx->bValid        = 1;

	pIsoCtx->sectorSize    = nSectorLength;
	fseek(fp, 0, SEEK_END);
	pIsoCtx->totalSectors  = (UINT32)(ftell(fp) / nSectorLength);
	fseek(fp, 0, SEEK_SET);
	return 1;
}

static void FileIO_Close(struct ISO_CTX* pIsoCtx)
{
	if (!pIsoCtx) return;
	if (pIsoCtx->ioType == IO_TYPE_FILE && pIsoCtx->ioUnion.pFile) {
		fclose(pIsoCtx->ioUnion.pFile);
		pIsoCtx->ioUnion.pFile = NULL;
	}
	pIsoCtx->bValid = 0;
}

static void FileIO_Read(struct ISO_CTX* pIsoCtx, UINT8* Dest, UINT32 lOffset, UINT32 lSize, UINT32 lLength)
{
	if (!pIsoCtx || pIsoCtx->ioType != IO_TYPE_FILE) return;
	iso9660_ReadOffset(pIsoCtx->ioUnion.pFile, Dest, lOffset, lSize, lLength);
}

// Read from a CHD via cd_chd.  lOffset uses the 2352-stride convention shared
// with the FileIO path (sector*2352 + in-sector byte offset), but the offset
// always lands inside the 2048-byte user area.  We translate to a logical LBA
// and read the mode-1 user data, so both 2048 and 2352 source CHDs work.
static void iso9660_ReadOffset_CHD(UINT8* Dest, ChdImage* pChd, UINT32 lOffset, UINT32 lSize, UINT32 lLength)
{
	if (!Dest || !pChd) return;

	UINT32 total_bytes = lSize * lLength;
	UINT32 bytes_read  = 0;
	UINT8  sector[2048];

	while (bytes_read < total_bytes) {
		UINT32 lba             = lOffset / nSectorLength;
		UINT32 offset_in_user  = lOffset % nSectorLength;
		if (offset_in_user >= 2048) {
			// beyond the user data area (raw sync/ECC region) - pad with zero
			memset(Dest + bytes_read, 0, total_bytes - bytes_read);
			return;
		}

		if (ChdReadSector(pChd, (INT32)lba, CHD_TRACK_MODE1, sector) != 0) {
			memset(Dest + bytes_read, 0, total_bytes - bytes_read);
			return;
		}

		UINT32 avail   = 2048 - offset_in_user;
		UINT32 to_read = total_bytes - bytes_read;
		if (to_read > avail) to_read = avail;
		memcpy(Dest + bytes_read, sector + offset_in_user, to_read);

		bytes_read += to_read;
		lOffset    += to_read;
	}
}

static INT32 ChdIO_Open(struct ISO_CTX* pIsoCtx, TCHAR* pszFile)
{
	if (!pIsoCtx || !pszFile) return 0;

	ChdImage* pChd = ChdOpenFile(pszFile);
	if (!pChd) {
		bprintf(PRINT_ERROR, _T("ChdIO_Open: ChdOpenFile failed for %s\n"), pszFile);
		return 0;
	}

	INT32 nContainer = ChdGetContainerType(pChd);
	if (nContainer != CHD_CONTAINER_CD && nContainer != CHD_CONTAINER_GDROM) {
		ChdClose(pChd);
		return 0;
	}

	pIsoCtx->ioType       = IO_TYPE_CHD;
	pIsoCtx->ioUnion.pChd = pChd;
	pIsoCtx->bValid       = 1;

	// Report user-data sector size and total logical sectors.
	pIsoCtx->sectorSize   = 2048;
	pIsoCtx->totalSectors = (UINT32)ChdGetTotalFrames(pChd);

	return 1;
}

static void ChdIO_Close(struct ISO_CTX* pIsoCtx)
{
	if (!pIsoCtx || pIsoCtx->ioType != IO_TYPE_CHD) return;

	if (pIsoCtx->ioUnion.pChd) {
		ChdClose(pIsoCtx->ioUnion.pChd);
		pIsoCtx->ioUnion.pChd = NULL;
	}
	pIsoCtx->bValid = 0;
}

static void ChdIO_Read(struct ISO_CTX* pIsoCtx, UINT8* Dest, UINT32 lOffset, UINT32 lSize, UINT32 lLength)
{
	if (!pIsoCtx || pIsoCtx->ioType != IO_TYPE_CHD) return;
	iso9660_ReadOffset_CHD(Dest, pIsoCtx->ioUnion.pChd, lOffset, lSize, lLength);
}

// Match I/O strategy by file extension
static const struct IO_STRATEGY ioStrategyList[] =
{
	{ IO_TYPE_FILE,  FileIO_Open,  FileIO_Close,  FileIO_Read },
	{ IO_TYPE_CHD,   ChdIO_Open,   ChdIO_Close,   ChdIO_Read  }
};

static void IsoRead(struct ISO_CTX* pIsoCtx, UINT8* Dest, UINT32 lOffset, UINT32 lSize, UINT32 lLength)
{
	if (!pIsoCtx || !pIsoCtx->bValid) return;

	// Dispatch by I/O type; caller does not need to distinguish between file and CHD
	switch (pIsoCtx->ioType) {
		case IO_TYPE_FILE:
			FileIO_Read(pIsoCtx, Dest, lOffset, lSize, lLength);
			break;
		case IO_TYPE_CHD:
			ChdIO_Read(pIsoCtx, Dest, lOffset, lSize, lLength);
			break;
		default:
			break;
	}
}

static INT32 IsoCheckAndReadLabel(struct ISO_CTX* pIsoCtx)
{
	if (!pIsoCtx || !pIsoCtx->bValid) return 0;

	UINT8 IsoCheck[64] = { 0 };
	// Read CD001 identifier from the PVD area
	IsoRead(pIsoCtx, IsoCheck, nSectorLength * 16 + 1, 1, sizeof(IsoCheck));

	if (!memcmp(IsoCheck, "CD001", 5)) {
		// Read and trim the volume label (stored in ISO_CTX)
		strncpy(pIsoCtx->szVolumeID, (char*)&IsoCheck[39], 24);
		pIsoCtx->szVolumeID[sizeof(pIsoCtx->szVolumeID) - 1] = '\0';
		INT32 l = strlen(pIsoCtx->szVolumeID);
		while (l > 0 && pIsoCtx->szVolumeID[l - 1] == ' ')
			pIsoCtx->szVolumeID[--l] = '\0';

#if DEBUG_CD
		bprintf(0, _T("VolumeID is [%S]\n"), pIsoCtx->szVolumeID);
#endif
		return 1;
	}
	bprintf(PRINT_ERROR, _T("ISO9660 CD001 check failed\n"));
	return 0;
}

static UINT32 IsoReadRootSector(struct ISO_CTX* pIsoCtx)
{
	UINT8 buffer[32]      = { 0 };
	char szRootSector[32] = { 0 };
	UINT32 nRootSector    =   0;

	// Read root directory address at fixed offset 0x9E
	IsoRead(pIsoCtx, buffer, nSectorLength * 16 + 0x9e, 1, 8);
	snprintf(szRootSector, sizeof(szRootSector), "%02x%02x%02x%02x", buffer[4], buffer[5], buffer[6], buffer[7]);
	szRootSector[sizeof(szRootSector) - 1] = '\0';
	sscanf(szRootSector, "%x", &nRootSector);

	return nRootSector;
}

static void NeoCDList_CheckDirCommon(void (*pfEntryCallBack)(INT32, TCHAR*), TCHAR* pszFile, struct ISO_CTX* pIsoCtx, INT32 nSector)
{
	UINT32 lBytesRead       = 0;
	UINT32 lOffset          = (UINT32)nSector * nSectorLength;
	bool   bNewSector       = false;
	bool   bRevisionQueve   = false;
	INT32  nRevisionQueveID = 0;
	bool   bGotDDPRG_ACM    = false;

	TCHAR* pIsoPath = NULL;
	if (IsFileExt(pszFile, _T(".cue"))) {
		pIsoPath = ParseCueGetImageFile(pszFile);
		if (!pIsoPath) return;

		TCHAR szIsoPath[MAX_PATH] = { 0 };
		_tcsncpy(szIsoPath, pIsoPath, MAX_PATH - 1);
		szIsoPath[MAX_PATH - 1] = _T('\0');
		free_s((void**)&pIsoPath);
		pIsoPath = szIsoPath;
	}

	pIsoPath = pszFile;

	UINT8  nLenDR;
	UINT8  Flags;
	UINT8  LEN_FI;
	UINT8* ExtentLoc = (UINT8*)malloc(8 + 64 + 32);
	UINT8* Data      = (UINT8*)malloc(0x10b  + 32);
	char*  File      = (char* )malloc(32     + 32);

	if (!ExtentLoc || !Data || !File) {
		free_s((void**)&ExtentLoc);
		free_s((void**)&Data);
		free_s((void**)&File);
		return;
	}

	while (1) {
		IsoRead(pIsoCtx, &nLenDR, lOffset, 1, sizeof(UINT8));

		if (nLenDR == 0x22) {
			lOffset    += nLenDR;
			lBytesRead += nLenDR;
			continue;
		}
		if (nLenDR < 0x22) {
			if (bNewSector) {
				if (bRevisionQueve) {
					bRevisionQueve = false;
					pfEntryCallBack(nRevisionQueveID, pIsoPath);
				}
				return;
			}
			nLenDR = 0;
			IsoRead(pIsoCtx, &nLenDR, lOffset + 1, 1, sizeof(UINT8));
			if (nLenDR < 0x22) {
				lOffset   += (nSectorLength - lBytesRead);
				lBytesRead = 0;
				bNewSector = true;
				continue;
			}
		}
		bNewSector = false;

		IsoRead(pIsoCtx, &Flags, lOffset + 25, 1, sizeof(UINT8));

		if (!(Flags & (1 << 1))) {
			IsoRead(pIsoCtx, ExtentLoc, lOffset + 2, 54, sizeof(UINT8));

			char szFname[64];
			UINT8 nDate[3]  = { 0, 0, 0 };
			memset(szFname, 0, sizeof(szFname));
			INT32 fname_len = (ExtentLoc[30] < 32) ? ExtentLoc[30] : 32;
			strncpy(szFname, (char*)&ExtentLoc[31], fname_len);

			if (!strcmp(szFname, "DDPRG.ACM")) bGotDDPRG_ACM = true;
			if (fname_len == 0) break;

			nDate[0] = ExtentLoc[16];
			nDate[1] = ExtentLoc[17];
			nDate[2] = ExtentLoc[18];

#if 0
			bprintf(0, _T("Checking File  [%S]  %d-%d-%d\n"), szFname, nDate[0], nDate[1], nDate[2]);
#endif
			// Restore original multi-byte composition logic (reverted custom modifications)
			UINT32 nValue =
				((UINT32)ExtentLoc[0])       |
				((UINT32)ExtentLoc[1] <<  8) |
				((UINT32)ExtentLoc[2] << 16) |
				((UINT32)ExtentLoc[3] << 24);

			IsoRead(pIsoCtx, Data, nValue * nSectorLength, 0x10a, sizeof(UINT8));

			char szData[32];
			snprintf(szData, sizeof(szData), "%c%c%c%c%c%c%c", Data[0x100], Data[0x101], Data[0x102], Data[0x103], Data[0x104], Data[0x105], Data[0x106]);
			szData[sizeof(szData) - 1] = '\0';

			if (!strncmp(szData, "NEO-GEO", 7)) {
				char id[32];
				snprintf(id, sizeof(id), "%02X%02X", Data[0x108], Data[0x109]);
				id[sizeof(id) - 1] = '\0';
				UINT32 nID = 0;
				sscanf(id, "%x", &nID);
#if 0
				bprintf(0, _T("----- ID:  %x\n"), nID);
#endif
				IsoRead(pIsoCtx, &LEN_FI,      lOffset + 32, 1,      sizeof(UINT8));
				IsoRead(pIsoCtx, (UINT8*)File, lOffset + 33, LEN_FI, sizeof(UINT8));
				File[LEN_FI] = 0;

				if (nID == 0x0016 && nDate[0] ==  94 && nDate[1] == 12 && nDate[2] == 20) { nID |= 0x1000; }	// Justin Gibbons Hacks (kotm)
				if (nID == 0x0025 && nDate[0] ==  94 && nDate[1] == 12 && nDate[2] == 20) { nID |= 0x1000; }	// Justin Gibbons Hacks (eightman)
				if (nID == 0x0076 && nDate[0] ==  94 && nDate[1] == 12 && nDate[2] == 20) { nID |= 0x1000; }	// Justin Gibbons Hacks (zedblade)
				if (nID == 0x0062 && nDate[0] ==  94 && nDate[1] == 12 && nDate[2] == 20) { nID |= 0x1000; }	// Justin Gibbons Hacks (spinmast)
				if (nID == 0x0059 && nDate[0] ==  95 && nDate[1] ==  6 && nDate[2] == 20) { nID |= 0x1000; }	// Savage Reign Rev 1
				if (nID == 0x0066 && nDate[0] == 125 && nDate[1] ==  4 && nDate[2] == 10) { nID |= 0x1200; }	// Digger Man (Prototype)
				if (nID == 0x069c && nDate[0] ==  95 && nDate[1] ==  4 && nDate[2] == 29) { nID |= 0x1000; }	// Fatal Fury 3 Rev 1
				if (nID == 0x069c && nDate[0] ==  95 && nDate[1] ==  6 && nDate[2] ==  1) { nID |= 0x2000; }	// Fatal Fury 3 Rev 2
				if (nID == 0x069c && nDate[0] ==  95 && nDate[1] ==  7 && nDate[2] == 10) { nID |= 0x3000; }	// Fatal Fury 3 Rev 3
				if (nID == 0x0090 && nDate[0] ==  95 && nDate[1] ==  7 && nDate[2] == 21) { nID |= 0x1000; }	// World Heroes Perfect
				if (nID == 0x0058 && nDate[0] ==  94 && nDate[1] == 10 && nDate[2] == 14) { nID |= 0x1000; }	// Fatal Fury Special Rev 1
				if (nID == 0x0052 && nDate[0] == 123 && nDate[1] ==  7 && nDate[2] ==  1) { nID |= 0x1000; }	// Abyssal Infants
				if (nID == 0x0052 && nDate[0] == 123 && nDate[1] ==  6 && nDate[2] == 20) { nID |= 0x1001; }	// Neo Fight
				if (nID == 0x0082) {
					if (bGotDDPRG_ACM)                                                    { nID |= 0x1000; }	// Double Dragon Rev 1
					else if (StrStrI(pIsoPath, _T("OST")) ||
						     StrStrI(pIsoPath, _T("PS"))  ||
						     StrStrI(pIsoPath, _T("PlayStation")))                        { nID |= 0x2000; }	// Double Dragon PS1 OST
				}
				if (nID == 0x0085) {
					if (nDate[0] == 123 && nDate[1] == 11 && nDate[2] == 29)              { nID |= 0x1000; }	// Samurai Shodown RPG (English Translation)
					else if (nDate[0] == 124 && nDate[1] == 1 && nDate[2] == 26)          { nID |= 0x3000; }	// Samurai Shodown RPG (English Translation v1.1)
					else if (nDate[0] == 126 && nDate[1] == 6 && nDate[2] ==  1)          { nID |= 0x4000; }	// Samurai Shodown RPG (Simplified Chinese Translation v1.1)
					else if (StrStrI(pIsoPath, _T("FR")) ||
						     StrStrI(pIsoPath, _T("French")))                             { nID |= 0x2000; }	// Samurai Shodown RPG (FR)
				}
				if (nID == 0x5345) {
					if (!strcmp(pIsoCtx->szVolumeID, "BLUEANDREDFIGHTTHEROBOTS") ||
						(StrStrI(pIsoPath, _T("NTSC"))))                                  { nID  = 0x5346; }	// Blue And Red - Fight The Robots! (NTSC)
				}
				if (nID == 0x1234 && nDate[0] == 105 && nDate[1] ==  4 && nDate[2] == 25) { nID  = 0x2234; }	// Neo Puzzle League
				if (nID == 0x1234 && nDate[0] == 124 && nDate[1] == 12 && nDate[2] ==  2) { nID  = 0x2235; }	// Neo Tetris
				if (nID == 0x1234 && nDate[0] == 112 && nDate[1] ==  3 && nDate[2] ==  4) { nID  = 0x2236; }	// NGD::ARK
				if (nID == 0x1234 && nDate[0] == 112 && nDate[1] == 12 && nDate[2] ==  4) { nID  = 0x2237; }	// Santa Ball
				if (nID == 0x2000 && !strcmp(pIsoCtx->szVolumeID, "COLUMNS"))             { nID |= 0x1000; }	// Columns
				if (nID == 0xFFFF && !strcmp(pIsoCtx->szVolumeID, "CODENAME BLUT ENGEL")) { nID  = 0xFFFE; }	// Codename Blut Engel
				if (nID == 0x7777 && nDate[0] == 114 && nDate[1] ==  8 && nDate[2] == 14) { nID  = 0x7778; }	// Puzzle de Pon! CD Collection
				if (nID == 0x2019 && !strcmp(pIsoCtx->szVolumeID, "LOOPTRSP"))            { nID |= 0x0100; }	// Looptris Plus
				if (nID == 0x0048 && Data[0x67] == 0x08)                                  { nID |= 0x1000; }	// Treasure of Caribbean (c) 1994 / (c) 2011 NCI
				if (nID == 0x0055 && Data[0x67] == 0xDE)	/* 10-6-1994 (P1.PRG)  */     {/* ...continue*/}	// King of Fighters '94, The (1994)(SNK)(JP)
				if (nID == 0x0055 && Data[0x67] == 0xE6)	/* 11-21-1994 (P1.PRG) */     { nID |= 0x1000; }	// King of Fighters '94, The (1994)(SNK)(JP-US)
				if (nID == 0x0084 && Data[0x6C] == 0xC0)	/* 9-11-1995 (P1.PRG)  */     {/* ...continue*/}	// King of Fighters '95, The (1995)(SNK)(JP-US)[!][NGCD-084 MT B01, B03-B06, NGCD-084E MT B01]
				if (nID == 0x0084 && Data[0x6C] == 0xFF)	/* 10-5-1995 (P1.PRG)  */     { nID |= 0x1000; }	// King of Fighters '95, The (1995)(SNK)(JP-US)[!][NGCD-084 MT B10, NGCD-084E MT B03]
				if (nID == 0x0229) {																			// King of Fighters '96 NEOGEO Collection, The
					bRevisionQueve = false;
					pfEntryCallBack(nID, pszFile);
					break;
				}
				// Sometimes there's multiple .prg entries in the
				// list, .old / .bak files which preceed the proper one..
				// For these, we have to keep searching the file list:
				if (nID == 0x0214																				// King of Fighters '96, The
					|| (nID == 0x0058 && !(nDate[0] == 94 && nDate[1] == 8 && nDate[2] == 5))) {				// !Fatal Fury Special Rev 0
					// continue checking other files...
					bRevisionQueve   = true;
					nRevisionQueveID = nID;
					lOffset    += nLenDR;
					lBytesRead += nLenDR;
					continue;
				}
				pfEntryCallBack(nID, pszFile);
				break;
			}
		}
		lOffset    += nLenDR;
		lBytesRead += nLenDR;
	}

	// Unified safe release
	free_s((void**)&ExtentLoc);
	free_s((void**)&Data);
	free_s((void**)&File);
}

static void PrintNGCDGameInfo(const NGCDGAME* pGame)
{
	if (!pGame) return;
	bprintf(PRINT_NORMAL, _T("    Title: %s \n"), pGame->pszTitle);
	bprintf(PRINT_NORMAL, _T("    Shortname: %s \n"), pGame->pszName);
	bprintf(PRINT_NORMAL, _T("    Year: %s \n"), pGame->pszYear);
	bprintf(PRINT_NORMAL, _T("    Company: %s \n"), pGame->pszCompany);
}

// Internal reusable tool: deep copy game meta by ID, output via ppOutGame
// Return 1 success, 0 fail
INT32 GetNGCDGameTitle(const UINT32 nGameID, NGCDGAME** ppOutGame, bool bPrintLog)
{
	if (!ppOutGame)
		return 0;

	*ppOutGame = NULL;
	NGCDGAME* pGameInfo = GetNeoGeoCDInfo(nGameID);
	if (!pGameInfo) {
		bprintf(0, _T("NeoGeoCD Unknown GAME ID %x\n"), nGameID);
		return 0;
	}

	NGCDGAME* pnew = (NGCDGAME*)calloc(1, sizeof(NGCDGAME));
	if (!pnew)
		return 0;

	// Deep copy all fields
	pnew->id         = pGameInfo->id;
	pnew->pszName    = _tcsdup_s(pGameInfo->pszName);
	if (!pnew->pszName)
		goto CLEAN_FAIL;

	pnew->pszTitle   = _tcsdup_s(pGameInfo->pszTitle);
	if (!pnew->pszTitle)
		goto CLEAN_FAIL;

	pnew->pszYear    = _tcsdup_s(pGameInfo->pszYear);
	if (!pnew->pszYear)
		goto CLEAN_FAIL;

	pnew->pszCompany = _tcsdup_s(pGameInfo->pszCompany);
	if (!pnew->pszCompany)
		goto CLEAN_FAIL;

	// Print only when switch enabled
	if (bPrintLog)
		PrintNGCDGameInfo(pnew);

	*ppOutGame = pnew;
	return 1;

CLEAN_FAIL:
	free_s((void**)&pnew->pszName);
	free_s((void**)&pnew->pszTitle);
	free_s((void**)&pnew->pszYear);
	free_s((void**)&pnew->pszCompany);
	free_s((void**)&pnew);
	return 0;
}

INT32 NeoCDList_CheckISO(TCHAR* pszFile, void (*pfEntryCallBack)(INT32, TCHAR*))
{
	if (!pszFile)
		return 0;

	TCHAR* pIsoPath = NULL;
	if (IsFileExt(pszFile, _T(".cue"))) {
		pIsoPath = ParseCueGetImageFile(pszFile);
		if (!pIsoPath) return 0;

		TCHAR szIsoPath[MAX_PATH] = { 0 };
		_tcsncpy(szIsoPath, pIsoPath, MAX_PATH - 1);
		szIsoPath[MAX_PATH - 1] = _T('\0');
		free_s((void**)&pIsoPath);
		pIsoPath = szIsoPath;
	}

	if (!pIsoPath) pIsoPath = pszFile;

	INT32 ret = 0;
	// Match I/O strategy
	const struct IO_STRATEGY* pStrategy = NULL;
	if (       IsFileExt(pIsoPath, _T(".img")) ||
		       IsFileExt(pIsoPath, _T(".bin"))) {
		pStrategy = &ioStrategyList[0];
	} else if (IsFileExt(pIsoPath, _T(".chd"))) {
		pStrategy = &ioStrategyList[1];
	} else {
		// Unsupported format, exit directly
		return 0;
	}

	// Allocate top-level context
	struct ISO_CTX* pIsoCtx = (struct ISO_CTX*)calloc(1, sizeof(struct ISO_CTX));
	if (!pIsoCtx) return 0;

	// Open image and initialize context
	if (!pStrategy->fnOpen(pIsoCtx, pIsoPath)) {
		free_s((void**)&pIsoCtx);
		return 0;
	}

	// Minimum sector count check
	if (pIsoCtx->totalSectors <= 16) {
		pStrategy->fnClose(pIsoCtx);
		free_s((void**)&pIsoCtx);
		return 0;
	}

	// ISO identifier and volume label check
	if (IsoCheckAndReadLabel(pIsoCtx)) {
		UINT32 nRootSector = IsoReadRootSector(pIsoCtx);
		// Call the single unified directory function
		NeoCDList_CheckDirCommon(pfEntryCallBack, pszFile, pIsoCtx, (INT32)nRootSector);
		ret = 1;
	}

	// Unified resource cleanup and context release
	pStrategy->fnClose(pIsoCtx);
	free_s((void**)&pIsoCtx);
	return ret;
}

NGCDGAME* GetNeoGeoCDInfo(UINT32 nID)
{
	for(UINT32 nGame = 0; nGame < (sizeof(games) / sizeof(NGCDGAME)); nGame++) {
		if(nID == games[nGame].id ) {
			return &games[nGame];
		}
	}

	return NULL;
}

// Update the main window title
static void SetNeoCDTitle(TCHAR* pszTitle)
{
	TCHAR szText[1024] = _T("");
	_sntprintf(szText, ARRAY_SIZE(szText), _T(APP_TITLE) _T( " v%.20s") _T(SEPERATOR_1) _T("%s") _T(SEPERATOR_1) _T("%s"), szAppBurnVer, BurnDrvGetText(DRV_FULLNAME), pszTitle);
	szText[ARRAY_SIZE(szText) - 1] = '\0';
	SetWindowText(hScrnWnd, szText);
}

void NeoCDInfo_SetTitle()
{
	if (IsNeoGeoCD() && game && game->id) {
		SetNeoCDTitle(game->pszTitle);
	} else if (IsNeoGeoCD()) {
		SetNeoCDTitle(FBALoadStringEx(hAppInst, IDS_UNIDENTIFIED_CD, true));
	}
}

void FreeNGCDGame(NGCDGAME** ppGame)
{
	if (!ppGame || !*ppGame)
		return;

	NGCDGAME* p = *ppGame;
	free_s((void**)&p->pszName);
	free_s((void**)&p->pszTitle);
	free_s((void**)&p->pszYear);
	free_s((void**)&p->pszCompany);
	free_s((void**)ppGame);
}

// Get the title
INT32 GetNeoCDTitle(UINT32 nGameID)
{
	// Release previous global data first to avoid memory leak
	FreeNGCDGame(&game);
	// Global scene needs log output, pass true
	return GetNGCDGameTitle(nGameID, &game, true);
}

static void NeoCDList_Callback(INT32 nID, TCHAR *pszFile)
{
	GetNeoCDTitle(nID);
}

void NeoCDInfo_Exit()
{
	FreeNGCDGame(&game);
}

// This function scans and identifies NeoGeo CD game information from supported image files
INT32 GetNeoGeoCD_Identifier()
{
	TCHAR* szIsoPath = GetIsoPath();
	// Skip if ISO path is empty or NeoGeo CD mode is disabled
	if (!szIsoPath || !IsNeoGeoCD())
		return 0;

	// Clear previous game global data to avoid old info pollution
	NeoCDInfo_Exit();

	// Check supported image extensions: img / bin / chd
	if (IsFileExt(szIsoPath, _T(".img")) || IsFileExt(szIsoPath, _T(".bin")) || IsFileExt(szIsoPath, _T(".chd"))) {
		bprintf(0, _T("NeoCDList: checking %s\n"), szIsoPath);
		INT32 ret = NeoCDList_CheckISO(szIsoPath, NeoCDList_Callback);
		if (!ret) {
			bprintf(PRINT_NORMAL, _T("    Failed to parse image file %s\n"), szIsoPath);
			return 0;
		}
		return 1;
	} else {
		bprintf(PRINT_NORMAL, _T("    File doesn't have a valid ISO extension [.img / .bin / .chd]\n"));
		return 0;
	}
}

INT32 NeoCDInfo_Init()
{
	NeoCDInfo_Exit();
	return GetNeoGeoCD_Identifier();
}

TCHAR* NeoCDInfo_Text(INT32 nText)
{
	if(!game || !IsNeoGeoCD())	return NULL;

	switch(nText) {
		case DRV_NAME:			return game->pszName;
		case DRV_FULLNAME:		return game->pszTitle;
		case DRV_MANUFACTURER:	return game->pszCompany;
		case DRV_DATE:			return game->pszYear;
	}

	return NULL;
}

INT32 NeoCDInfo_ID()
{
	if(!game || !IsNeoGeoCD()) return 0;
	return game->id;
}

#endif