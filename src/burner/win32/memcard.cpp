// Memory card support module

#include "burner.h"

// PGM2 card slot variables (defined in pgm2_run.cpp)
extern INT32 Pgm2MaxCardSlots;
extern INT32 Pgm2ActiveCardSlot;
extern bool  Pgm2CardInserted[4];
extern INT32 pgm2GetCardRomTemplate(UINT8* buffer, INT32 maxSize);

static TCHAR szMemoryCardFile[MAX_PATH];

int nMemoryCardStatus = 0;
int nMemoryCardSize;

// PGM2 per-slot state
TCHAR szPgm2CardFile[4][MAX_PATH];
int nPgm2CardStatus[4] = {0, 0, 0, 0};  // per-slot: bit0=file selected, bit1=inserted

static int nMinVersion;
static bool bMemCardFC1Format;

static int MakeOfn()
{
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hScrnWnd;
	ofn.lpstrFile = szMemoryCardFile;
	ofn.nMaxFile = sizeof(szMemoryCardFile);
	ofn.lpstrInitialDir = _T(".");
	ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = _T("fc");

	return 0;
}

static int MemCardRead(TCHAR* szFilename, unsigned char* pData, int nSize)
{
	const char* szHeader  = "FB1 FC1 ";				// File + chunk identifier
	char szReadHeader[8] = "";

	bMemCardFC1Format = false;

	FILE* fp = _tfopen(szFilename, _T("rb"));
	if (fp == NULL) {
		return 1;
	}

	fread(szReadHeader, 1, 8, fp);					// Read identifiers
	if (memcmp(szReadHeader, szHeader, 8) == 0) {

		// FB Alpha memory card file

		int nChunkSize = 0;
		int nVersion = 0;

		bMemCardFC1Format = true;

		fread(&nChunkSize, 1, 4, fp);				// Read chunk size
		if (nSize < nChunkSize - 32) {
			fclose(fp);
			return 1;
		}

		fread(&nVersion, 1, 4, fp);					// Read version
		if (nVersion < nMinVersion) {
			fclose(fp);
			return 1;
		}
		fread(&nVersion, 1, 4, fp);
#if 0
		if (nVersion < nBurnVer) {
			fclose(fp);
			return 1;
		}
#endif

		fseek(fp, 0x0C, SEEK_CUR);					// Move file pointer to the start of the data block

		fread(pData, 1, nChunkSize - 32, fp);		// Read the data
	} else {

		// Check for raw binary card file (MAME PGM2 SLE4442 format: 264 or 256 bytes)
		fseek(fp, 0, SEEK_END);
		long nFileSize = ftell(fp);
		fseek(fp, 0x00, SEEK_SET);

		if (nFileSize == 0x108 || nFileSize == 0x100) {
			// Raw binary card: read directly
			bMemCardFC1Format = false;
			memset(pData, 0xFF, nSize);
			int nReadSize = (nFileSize <= nSize) ? (int)nFileSize : nSize;
			fread(pData, 1, nReadSize, fp);
			// If only 256 bytes (main data), init protection/security to defaults
			if (nFileSize == 0x100 && nSize >= 0x108) {
				memset(pData + 0x100, 0xFF, 4);   // protection: all writable
				pData[0x104] = 0x07;               // sec[0]: 3 auth attempts
				pData[0x105] = 0xFF;               // sec[1]: default PSC
				pData[0x106] = 0xFF;               // sec[2]: default PSC
				pData[0x107] = 0xFF;               // sec[3]: default PSC
			}
		} else {
			// MAME Neo Geo or old FB Alpha memory card file (half-size interleaved)
			unsigned char* pTemp = (unsigned char*)malloc(nSize >> 1);

			memset(pData, 0, nSize);

			if (pTemp) {
				fread(pTemp, 1, nSize >> 1, fp);

				for (int i = 1; i < nSize; i += 2) {
					pData[i] = pTemp[i >> 1];
				}

				free(pTemp);
				pTemp = NULL;
			}
		}
	}

	fclose(fp);

	return 0;
}

static int MemCardWrite(TCHAR* szFilename, unsigned char* pData, int nSize)
{
	FILE* fp = _tfopen(szFilename, _T("wb"));
	if (fp == NULL) {
		return 1;
	}

	// Check if filename ends with .bin — write raw binary (MAME-compatible)
	bool bRawBin = false;
	if (szFilename) {
		int len = (int)_tcslen(szFilename);
		if (len >= 4 && _tcsicmp(szFilename + len - 4, _T(".bin")) == 0) {
			bRawBin = true;
		}
	}

	if (bRawBin) {
		// Raw binary format (MAME PGM2 compatible)
		fwrite(pData, 1, nSize, fp);
	} else if (bMemCardFC1Format) {

		// FB Alpha memory card file

		const char* szFileHeader  = "FB1 ";				// File identifier
		const char* szChunkHeader = "FC1 ";				// Chunk identifier
		const int nZero = 0;

		int nChunkSize = nSize + 32;

		fwrite(szFileHeader, 1, 4, fp);
		fwrite(szChunkHeader, 1, 4, fp);

		fwrite(&nChunkSize, 1, 4, fp);					// Chunk size

		fwrite(&nBurnVer, 1, 4, fp);					// Version of FBA this was saved from
		fwrite(&nMinVersion, 1, 4, fp);					// Min version of FBA data will work with

		fwrite(&nZero, 1, 4, fp);						// Reserved
		fwrite(&nZero, 1, 4, fp);						//
		fwrite(&nZero, 1, 4, fp);						//

		fwrite(pData, 1, nSize, fp);
	} else {

		// MAME or old FB Alpha memory card file

		unsigned char* pTemp = (unsigned char*)malloc(nSize >> 1);
		if (pTemp) {
			for (int i = 1; i < nSize; i += 2) {
				pTemp[i >> 1] = pData[i];
			}

			fwrite(pTemp, 1, nSize >> 1, fp);

			free(pTemp);
			pTemp = NULL;
		}
	}

	fclose(fp);

	return 0;
}

static int __cdecl MemCardDoGetSize(struct BurnArea* pba)
{
	nMemoryCardSize = pba->nLen;

	return 0;
}

static int MemCardGetSize()
{
	BurnAcb = MemCardDoGetSize;
	BurnAreaScan(ACB_MEMCARD | ACB_MEMCARD_ACTION, &nMinVersion);

	return 0;
}

int	MemCardCreate()
{
	TCHAR szFilter[1024];
	int nRet;

	_stprintf(szFilter, FBALoadStringEx(hAppInst, IDS_DISK_FILE_CARD, true), _T(APP_TITLE));
	memcpy(szFilter + _tcslen(szFilter), _T(" (*.fc)\0*.fc\0\0"), 14 * sizeof(TCHAR));

	_stprintf (szMemoryCardFile, _T("memorycard"));
	MakeOfn();
	ofn.lpstrTitle = FBALoadStringEx(hAppInst, IDS_MEMCARD_CREATE, true);
	ofn.lpstrFilter = szFilter;
	ofn.Flags |= OFN_OVERWRITEPROMPT;

	int bOldPause = bRunPause;
	bRunPause = 1;
	nRet = GetSaveFileName(&ofn);
	bRunPause = bOldPause;

	if (nRet == 0) {
		return 1;
	}

	{
		unsigned char* pCard;

		MemCardGetSize();

		pCard = (unsigned char*)malloc(nMemoryCardSize);
		memset(pCard, 0, nMemoryCardSize);

		bMemCardFC1Format = true;

		if (MemCardWrite(szMemoryCardFile, pCard, nMemoryCardSize)) {
			return 1;
		}

		if (pCard) {
			free(pCard);
			pCard = NULL;
		}
	}

	nMemoryCardStatus = 1;
	MenuEnableItems();

	return 0;
}

int	MemCardSelect()
{
	TCHAR szFilter[1024];
	TCHAR* pszTemp = szFilter;
	int nRet;

	pszTemp += _stprintf(pszTemp, FBALoadStringEx(hAppInst, IDS_DISK_ALL_CARD, true));
	memcpy(pszTemp, _T(" (*.fc, MEMCARD.\?\?\?)\0*.fc;MEMCARD.\?\?\?\0"), 38 * sizeof(TCHAR));
	pszTemp += 38;
	pszTemp += _stprintf(pszTemp, FBALoadStringEx(hAppInst, IDS_DISK_FILE_CARD, true), _T(APP_TITLE));
	memcpy(pszTemp, _T(" (*.fc)\0*.fc\0"), 13 * sizeof(TCHAR));
	pszTemp += 13;
	pszTemp += _stprintf(pszTemp, FBALoadStringEx(hAppInst, IDS_DISK_FILE_CARD, true), _T("MAME"));
	memcpy(pszTemp, _T(" (MEMCARD.\?\?\?)\0MEMCARD.\?\?\?\0\0"), 28 * sizeof(TCHAR));

	MakeOfn();
	ofn.lpstrTitle = FBALoadStringEx(hAppInst, IDS_MEMCARD_SELECT, true);
	ofn.lpstrFilter = szFilter;

	int bOldPause = bRunPause;
	bRunPause = 1;
	nRet = GetOpenFileName(&ofn);
	bRunPause = bOldPause;

	if (nRet == 0) {
		return 1;
	}

	MemCardGetSize();

	if (nMemoryCardSize <= 0) {
		return 1;
	}

	nMemoryCardStatus = 1;
	MenuEnableItems();

	return 0;
}

static int __cdecl MemCardDoInsert(struct BurnArea* pba)
{
	if (MemCardRead(szMemoryCardFile, (unsigned char*)pba->Data, pba->nLen)) {
		return 1;
	}

	nMemoryCardStatus |= 2;
	MenuEnableItems();

	return 0;
}

int	MemCardInsert()
{
	if ((nMemoryCardStatus & 1) && (nMemoryCardStatus & 2) == 0) {
		BurnAcb = MemCardDoInsert;
		BurnAreaScan(ACB_WRITE | ACB_MEMCARD | ACB_MEMCARD_ACTION, &nMinVersion);
	}

	return 0;
}

static int __cdecl MemCardDoEject(struct BurnArea* pba)
{
	if (MemCardWrite(szMemoryCardFile, (unsigned char*)pba->Data, pba->nLen) == 0) {
		nMemoryCardStatus &= ~2;
		MenuEnableItems();

		return 0;
	}

	return 1;
}

int	MemCardEject()
{
	if ((nMemoryCardStatus & 1) && (nMemoryCardStatus & 2)) {
		BurnAcb = MemCardDoEject;
		nMinVersion = 0;
		BurnAreaScan(ACB_READ | ACB_MEMCARD | ACB_MEMCARD_ACTION, &nMinVersion);
	}

	return 0;
}

int	MemCardToggle()
{
	if (nMemoryCardStatus & 1) {
		if (nMemoryCardStatus & 2) {
			return MemCardEject();
		} else {
			return MemCardInsert();
		}
	}

	return 1;
}

// PGM2-specific: capture current card data from driver
static unsigned char* pCapturedCardData;
static int nCapturedCardSize;

static int __cdecl MemCardDoCapture(struct BurnArea* pba)
{
	if (pba->Data && pba->nLen > 0 && nCapturedCardSize == 0) {
		pCapturedCardData = (unsigned char*)malloc(pba->nLen);
		if (pCapturedCardData) {
			memcpy(pCapturedCardData, pba->Data, pba->nLen);
			nCapturedCardSize = pba->nLen;
		}
	}
	return 0;
}

// PGM2-specific: auto-create a card file for the current game
// Uses the game's blank card data loaded from .pg2 ROM
int	MemCardCreatePGM2()
{
	TCHAR* gameName = BurnDrvGetText(DRV_NAME);
	if (!gameName) return 1;

	// Ensure directory exists
	CreateDirectory(_T("config"), NULL);
	CreateDirectory(_T("config\\memcards"), NULL);

	// Use .bin (MAME-compatible raw binary format)
	_stprintf(szMemoryCardFile, _T("config\\memcards\\%s_card.bin"), gameName);

	// Always load fresh ROM template
	unsigned char cardData[0x108];
	int cardLen = pgm2GetCardRomTemplate(cardData, sizeof(cardData));
	if (cardLen <= 0) return 1;

	// Write raw binary
	FILE* fp = _tfopen(szMemoryCardFile, _T("wb"));
	if (!fp) return 1;
	fwrite(cardData, 1, cardLen, fp);
	fclose(fp);

	nMemoryCardStatus = 1;
	MenuEnableItems();

	return 0;
}

// PGM2-specific: select a card file from the memcards directory
int	MemCardSelectPGM2()
{
	TCHAR szFilter[1024];
	TCHAR* pszTemp = szFilter;
	int nRet;

	pszTemp += _stprintf(pszTemp, FBALoadStringEx(hAppInst, IDS_DISK_ALL_CARD, true));
	memcpy(pszTemp, _T(" (*.fc, *.bin)\0*.fc;*.bin\0\0"), 28 * sizeof(TCHAR));

	MakeOfn();
	ofn.lpstrTitle = FBALoadStringEx(hAppInst, IDS_MEMCARD_SELECT, true);
	ofn.lpstrFilter = szFilter;
	ofn.lpstrInitialDir = _T("config\\memcards");

	int bOldPause = bRunPause;
	bRunPause = 1;
	nRet = GetOpenFileName(&ofn);
	bRunPause = bOldPause;

	if (nRet == 0) return 1;

	MemCardGetSize();
	if (nMemoryCardSize <= 0) return 1;

	nMemoryCardStatus = 1;
	MenuEnableItems();

	return 0;
}

// ---------------------------------------------------------------------------
// PGM2 per-slot card operations
// ---------------------------------------------------------------------------

int MemCardCreatePGM2Slot(int slot)
{
	if (slot < 0 || slot >= 4) return 1;

	TCHAR* gameName = BurnDrvGetText(DRV_NAME);
	if (!gameName) return 1;

	CreateDirectory(_T("config"), NULL);
	CreateDirectory(_T("config\\memcards"), NULL);

	// Use .bin (MAME-compatible raw binary format)
	_stprintf(szPgm2CardFile[slot], _T("config\\memcards\\%s_card_p%d.bin"), gameName, slot + 1);

	// Always load fresh ROM template — not the current in-memory card data
	unsigned char cardData[0x108];
	int cardLen = pgm2GetCardRomTemplate(cardData, sizeof(cardData));
	if (cardLen <= 0) {
		// Fallback: blank SLE4442 card
		memset(cardData, 0xFF, sizeof(cardData));
		cardData[0x104] = 0x07;
		cardLen = 0x108;
	}

	// Write raw binary (MAME-compatible .bin format)
	FILE* fp = _tfopen(szPgm2CardFile[slot], _T("wb"));
	if (!fp) return 1;
	fwrite(cardData, 1, cardLen, fp);
	fclose(fp);

	nPgm2CardStatus[slot] |= 1;
	MenuEnableItems();
	return 0;
}

int MemCardSelectPGM2Slot(int slot)
{
	if (slot < 0 || slot >= 4) return 1;

	TCHAR szFilter[1024];
	TCHAR* pszTemp = szFilter;

	pszTemp += _stprintf(pszTemp, FBALoadStringEx(hAppInst, IDS_DISK_ALL_CARD, true));
	memcpy(pszTemp, _T(" (*.fc, *.bin)\0*.fc;*.bin\0\0"), 28 * sizeof(TCHAR));

	_tcscpy(szPgm2CardFile[slot], _T(""));
	MakeOfn();
	ofn.lpstrFile = szPgm2CardFile[slot];
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = FBALoadStringEx(hAppInst, IDS_MEMCARD_SELECT, true);
	ofn.lpstrFilter = szFilter;
	ofn.lpstrInitialDir = _T("config\\memcards");

	int bOldPause = bRunPause;
	bRunPause = 1;
	int nRet = GetOpenFileName(&ofn);
	bRunPause = bOldPause;

	if (nRet == 0) return 1;

	// Verify the file has valid size
	Pgm2ActiveCardSlot = slot;
	MemCardGetSize();
	if (nMemoryCardSize <= 0) {
		szPgm2CardFile[slot][0] = _T('\0');
		return 1;
	}

	nPgm2CardStatus[slot] |= 1;
	MenuEnableItems();
	return 0;
}

static int __cdecl MemCardDoInsertSlot(struct BurnArea* pba)
{
	// Determine which slot this is for (stored in global)
	int slot = Pgm2ActiveCardSlot;
	if (slot < 0 || slot >= 4) return 1;

	if (MemCardRead(szPgm2CardFile[slot], (unsigned char*)pba->Data, pba->nLen)) {
		return 1;
	}

	nPgm2CardStatus[slot] |= 2;
	MenuEnableItems();
	return 0;
}

int MemCardInsertPGM2Slot(int slot)
{
	if (slot < 0 || slot >= 4) return 1;
	if (!(nPgm2CardStatus[slot] & 1)) return 1;  // no file selected
	if (nPgm2CardStatus[slot] & 2) return 1;      // already inserted

	Pgm2ActiveCardSlot = slot;
	BurnAcb = MemCardDoInsertSlot;
	nMinVersion = 0;
	BurnAreaScan(ACB_WRITE | ACB_MEMCARD | ACB_MEMCARD_ACTION, &nMinVersion);

	return 0;
}

static int __cdecl MemCardDoEjectSlot(struct BurnArea* pba)
{
	int slot = Pgm2ActiveCardSlot;
	if (slot < 0 || slot >= 4) return 1;

	if (MemCardWrite(szPgm2CardFile[slot], (unsigned char*)pba->Data, pba->nLen) == 0) {
		nPgm2CardStatus[slot] &= ~2;
		MenuEnableItems();
	}
	return 0;
}

int MemCardEjectPGM2Slot(int slot)
{
	if (slot < 0 || slot >= 4) return 1;
	if (!(nPgm2CardStatus[slot] & 1)) return 1;
	if (!(nPgm2CardStatus[slot] & 2)) return 1;  // not inserted

	Pgm2ActiveCardSlot = slot;
	BurnAcb = MemCardDoEjectSlot;
	nMinVersion = 0;
	BurnAreaScan(ACB_READ | ACB_MEMCARD | ACB_MEMCARD_ACTION, &nMinVersion);

	return 0;
}
