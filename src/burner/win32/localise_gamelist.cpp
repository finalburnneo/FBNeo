#include "burner.h"
#include "burnint.h"
#include <string.h>

#define MAX_LST_GAMES		35000
#define MAX_LST_LINE_LEN	256

TCHAR szGamelistLocalisationTemplate[MAX_PATH] = _T("");
bool nGamelistLocalisationActive = false;
static INT32 nCodePage = CP_ACP;
static TCHAR* szLongNamesArray[MAX_LST_GAMES];

// Structure for passing game list data to the parsing callback
struct GameListParseData {
	char** pShortNames;		// Pointer to array of ANSI driver names (char*)
	TCHAR** pLongNames;		// Pointer to array of localized game names (TCHAR*)
	INT32 nTotalGames;		// Total number of valid game entries
};

// Callback: Count valid game list lines using existing parse data structure
static void GameListCountLineCallback(const TCHAR*, FILE* file, void* pData)
{
	struct GameListParseData* pParseData = (struct GameListParseData*)pData;

	// Safety checks
	if (!file || !pParseData)
		return;

	// Move file pointer to the beginning
	fseek(file, 0, SEEK_SET);

	TCHAR szTemp[MAX_LST_LINE_LEN];

	// Read and validate each line
	while (_fgetts(szTemp, ARRAY_SIZE(szTemp), file)) {
		// Skip comment lines starting with "//"
		if (_T('/') == szTemp[0] && _T('/') == szTemp[1])
			continue;

		// Skip codepage definition line
		// This is a legacy issue. It should no longer exist after the new code is adopted
		if (0 == _tcsncmp(szTemp, _T("codepage="), 9))
			continue;

		// Skip empty lines and line breaks
		TCHAR c = szTemp[0];
		if (_T('\n') == c || _T('\r') == c || 0 == c)
			continue;

		// Increment count for valid data line
		pParseData->nTotalGames++;
	}

	// Reset file pointer to the beginning for later operations
	fseek(file, 0, SEEK_SET);
}

// Count valid lines in gamelist translation file
static INT32 GetGamelistValidLineCount(const TCHAR* pszFilePath)
{
	if (IsStrEmpty(pszFilePath))
		return 0;
	
	// Reuse existing parse structure for line counting
	struct GameListParseData parseData = { 0 };

	// Process file with auto encoding detection and resource management
	SafeProcessTextFile(pszFilePath, GameListCountLineCallback, &parseData);

	// Return total count of valid game entries
	return parseData.nTotalGames;
}

// Extended game list localization parsing callback
static void GameListParse(const TCHAR*, FILE* fp, void* userData)
{
	struct GameListParseData* pData = (struct GameListParseData*)userData;

	// Validate input pointers and state
	if (!fp || !pData || !pData->pShortNames || !pData->pLongNames || pData->nTotalGames <= 0)
		return;

	TCHAR szLine[MAX_LST_LINE_LEN] = { 0 };
	INT32 nIndex = 0;

	// Read lines until buffer full or end of file
	while (_fgetts(szLine, ARRAY_SIZE(szLine), fp) && nIndex < pData->nTotalGames)
	{
		TCHAR* pCurr = szLine;

		// Skip comment lines
		if (_T('/') == pCurr[0] && _T('/') == pCurr[1])
			continue;

		// Skip codepage definition
		if (!_tcsncmp(pCurr, _T("codepage="), 9))
			continue;

		// Skip leading whitespace (spaces / tabs)
		while (_T(' ') == *pCurr || _T('\t') == *pCurr)
			pCurr++;

		// Skip empty lines
		if (!*pCurr)
			continue;

		// Extract short name (continuous non-whitespace)
		TCHAR* pShortPart = pCurr;
		while (*pCurr && _T(' ') != *pCurr && _T('\t') != *pCurr)
			pCurr++;

		// Terminate short name string
		if (*pCurr)
			*pCurr++ = _T('\0');

		// Skip delimiters between short and long name
		while (_T(' ') == *pCurr || _T('\t') == *pCurr)
			pCurr++;

		// Point to long name start
		TCHAR* pLongPart = pCurr;

		// Trim trailing whitespace from long name
		TCHAR* pLongEnd = pLongPart + _tcslen(pLongPart);
		while (pLongEnd > pLongEnd && (_T(' ') == *(pLongEnd - 1) || _T('\t') == *(pLongEnd - 1) || _T('\n') == *(pLongEnd - 1) || _T('\r') == *(pLongEnd - 1)))
			*(--pLongEnd) = _T('\0');

		// Skip invalid entries
		if (!*pShortPart || !*pLongPart)
			continue;

		char*  pTmpShort = NULL;
		TCHAR* pTmpLong  = NULL;

		// Convert short name to ANSI
		if (tchar_to_ansi(pShortPart, &pTmpShort) <= 0)
			goto CLEANUP;

		// Duplicate long name string
		pTmpLong = _tcsdup(pLongPart);
		if (!pTmpLong)
			goto CLEANUP;

		// Store valid entries
		pData->pShortNames[nIndex] = pTmpShort;
		pData->pLongNames[ nIndex] = pTmpLong;
		nIndex++;
		continue;

	CLEANUP:
		// Release allocated resources on failure
		free_s((void**)&pTmpShort);
		free_s((void**)&pTmpLong);
	}
}

// Load extended game list localization (ex.glt)
void BurnerDoGameListExLocalisation()
{
	if (!nGamelistLocalisationActive)
		return;

	TCHAR szBasePath[MAX_PATH]   = { 0 };
	TCHAR szGamelistEx[MAX_PATH] = { 0 };

	// Safely copy base template path
	_tcsncpy(szBasePath, szGamelistLocalisationTemplate, MAX_PATH - 1);
	szBasePath[MAX_PATH - 1] = _T('\0');

	// Remove .glt extension if present
	TCHAR* pszExt = _tcsstr(szBasePath, _T(".glt"));
	if (pszExt)
		*pszExt = _T('\0');

	// Build full path to ex.glt
	_sntprintf(szGamelistEx, ARRAY_SIZE(szGamelistEx), _T("%sex.glt"), szBasePath);
	szGamelistEx[ARRAY_SIZE(szGamelistEx) - 1] = _T('\0');

	INT32 nGameCount = GetGamelistValidLineCount(szGamelistEx);
	if (nGameCount <= 0)
		return;

	// Allocate arrays for parsed strings
	char**  pShortNames = (char** )calloc(nGameCount, sizeof(char*));
	TCHAR** pLongNames  = (TCHAR**)calloc(nGameCount, sizeof(TCHAR*));

	if (!pShortNames || !pLongNames) {
		free_s((void**)&pShortNames);
		free_s((void**)&pLongNames);
		return;
	}

	// Parse the file
	struct GameListParseData parseData = {
		pShortNames,
		pLongNames,
		nGameCount
	};

	SafeProcessTextFile(szGamelistEx, GameListParse, &parseData);

	// Free OLD global buffers safely
	if (szShortNamesExArray && szLongNamesExArray && nNamesExArray > 0) {
		for (INT32 i = 0; i < nNamesExArray; i++) {
			free_s((void**)&szShortNamesExArray[i]);
			free_s((void**)&szLongNamesExArray[i]);
		}
		free_s((void**)&szShortNamesExArray);
		free_s((void**)&szLongNamesExArray);
	}

	// Assign new parsed data to global variables
	szShortNamesExArray = pShortNames;
	szLongNamesExArray  = pLongNames;
	nNamesExArray       = nGameCount;
}

// Main function to load and apply game list localization
void BurnerDoGameListLocalisation()
{
	if (!nGamelistLocalisationActive)
		return;

	INT32 nGameCount = GetGamelistValidLineCount(szGamelistLocalisationTemplate);
	if (nGameCount <= 0)
		return;

	// Allocate pointer arrays with zero initialization
	// Prevent dirty data from interfering with memory deallocation
	char**  pShortNames = (char** )calloc(nGameCount, sizeof(char*));
	TCHAR** pLongNames  = (TCHAR**)calloc(nGameCount, sizeof(TCHAR*));

	if (!pShortNames || !pLongNames) {
		free_s((void**)&pShortNames);
		free_s((void**)&pLongNames);
		return;
	}

	// Fill parse data structure
	struct GameListParseData parseData = {
		pShortNames,
		pLongNames,
		nGameCount
	};

	// Parse file with automatic encoding detection
	SafeProcessTextFile(szGamelistLocalisationTemplate, GameListParse, &parseData);

	// Reset found markers to ensure all entries are applied correctly
	BurnLocalisationResetFound();

	// Apply localized names to drivers
	for (INT32 i = 0; i < nGameCount; i++) {
		char*  pShort = pShortNames[i];
		TCHAR* pLong  = pLongNames[i];

		if (pShort && pLong)
			BurnLocalisationSetName(pShort, pLong);
	}

	// Safe memory cleanup
	for (INT32 i = 0; i < nGameCount; i++) {
		free_s((void**)&pShortNames[i]);
		free_s((void**)&pLongNames[i]);
	}

	free_s((void**)&pShortNames);
	free_s((void**)&pLongNames);

	// Process additional localization
	BurnerDoGameListExLocalisation();
}

// Convert TCHAR (Unicode) to UTF-8 encoded string
// Return value: >= 0 = length of UTF-8 string (excluding null terminator), -1 = failure
// Output buffer: must be freed with free_s() after use
static INT32 unicode_to_utf8(const TCHAR* src, char** dst)
{
	// Validate input parameters
	if (IsStrEmpty(src) || !dst)
		return -1;

	// Initialize output pointer to NULL for safety
	*dst = NULL;

	// Get required buffer size for conversion (including null terminator)
	INT32 len = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
	if (len <= 0)
		return -1;

	// Allocate buffer (zero-initialized for safety)
	char* pBuf = (char*)calloc(1, len);
	if (!pBuf)
		return -1;

	// Perform Unicode to UTF-8 conversion
	if (WideCharToMultiByte(CP_UTF8, 0, src, -1, pBuf, len, NULL, NULL) <= 0) {
		free_s((void**)&pBuf);
		return -1;
	}

	// Assign output buffer pointer
	*dst = pBuf;

	// Return length of converted UTF-8 string (excluding null terminator)
	return (len - 1);
}

// Create UTF-8 game list translation template file
// ANSI combined with code pages is an absolute disaster for non-English environments
// Moreover, no one on Windows wants to use code pages outside the system default — they only cause garbled text
static void BurnerGameListCreateTemplate()
{
	unsigned int nOldDrvSelect = nBurnDrvActive;

	// Open file in BINARY mode (critical for UTF-8, no CRLF mangling)
	FILE* fp = _tfopen(szGamelistLocalisationTemplate, _T("wb"));
	if (!fp)
		return;

	// Write UTF-8 header (NO BOM)
	const char* szHeaderVersion  = "// game list translation template for FinalBurn Neo version 0x%06X\n\n";
	const char* szHeaderCodepage = "// Note: Formats such as [codepage=65001] are deprecated. Please use UTF-8 or UTF-16LE encoding instead.\n\n";
	const char* szLineFormat     = "%s\t%s\n";

	fprintf(fp, szHeaderVersion, nBurnVer);
	fprintf(fp, szHeaderCodepage);

	// Temporarily disable localization to export ORIGINAL driver names
	bool nOldLocalizeEnabled = nGamelistLocalisationActive;
	nGamelistLocalisationActive = false;

	// Iterate over all built-in drivers (exclude RomData drivers)
	for (UINT32 i = 0; i < nIntlDrvCount; i++) {
		nBurnDrvActive = i;

		// Get original (non-localized) driver names
		const TCHAR* pShortName = BurnDrvGetText(DRV_NAME);
		const TCHAR* pLongName  = BurnDrvGetText(DRV_ASCIIONLY | DRV_FULLNAME);

		char* pShortUTF8 = NULL;
		char* pLongUTF8  = NULL;

		if (unicode_to_utf8(pShortName, &pShortUTF8) <= 0 || unicode_to_utf8(pLongName, &pLongUTF8) <= 0) {
			free_s((void**)&pShortUTF8);
			free_s((void**)&pLongUTF8);
			continue;
		}

		// Write UTF-8 encoded line to file
		fprintf(fp, szLineFormat, pShortUTF8, pLongUTF8);

		// Safe cleanup
		free_s((void**)&pShortUTF8);
		free_s((void**)&pLongUTF8);
	}

	// Restore localization state
	nGamelistLocalisationActive = nOldLocalizeEnabled;

	// Cleanup
	fclose(fp);
	nBurnDrvActive = nOldDrvSelect;
}

void BurnerExitGameListLocalisation()
{
	for (int i = 0; i < MAX_LST_GAMES; i++) {
		free_s((void**)&szLongNamesArray[i]);
		if (i < nNamesExArray) {
			free_s((void**)&szShortNamesExArray[i]);
			free_s((void**)&szLongNamesExArray[i]);
		}
	}
	free_s((void**)&szShortNamesExArray);
	free_s((void**)&szLongNamesExArray);
}

// ----------------------------------------------------------------------------
// Dialog box to load/save a template

// Initialize OPENFILENAME structure for game localization file save dialog
static void MakeOfn()
{
	// Filter buffer for OPENFILENAME. Must be static/global to stay valid for the dialog.
	static TCHAR szFilter[256];
	_sntprintf(szFilter, ARRAY_SIZE(szFilter), _T("%s"), FBALoadStringEx(hAppInst, IDS_LOCAL_GL_FILTER, true));
	szFilter[ARRAY_SIZE(szFilter) - 1] = _T('\0');

	// Locate end of the current string
	TCHAR* pEnd = szFilter + _tcslen(szFilter);
	size_t remainingChars = ARRAY_SIZE(szFilter) - _tcslen(szFilter);

	// Filter suffix format: " (*.glt)\0*.glt\0\0" (required by Windows)
	const TCHAR szFilterSuffix[] = _T(" (*.glt)\0*.glt\0");

	// Safely append filter pattern with proper double null terminator
	size_t suffixLength = ARRAY_SIZE(szFilterSuffix);
	if (remainingChars > suffixLength)
		memcpy(pEnd, szFilterSuffix, suffixLength * sizeof(TCHAR));

	// Initialize OPENFILENAME structure
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = hScrnWnd;
	ofn.lpstrFilter     = szFilter;
	ofn.lpstrFile       = szGamelistLocalisationTemplate;
	ofn.nMaxFile        = ARRAY_SIZE(szGamelistLocalisationTemplate);
	ofn.lpstrInitialDir = _T(".\\config\\localisation");
	ofn.Flags           = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
	ofn.lpstrDefExt     = _T("glt");
}

// Load game list translation template file
INT32 FBALocaliseGamelistLoadTemplate()
{
	// Safely set default file name (GCC compatible, no unsafe functions)
	_sntprintf(szGamelistLocalisationTemplate, ARRAY_SIZE(szGamelistLocalisationTemplate), _T("template"));
	szGamelistLocalisationTemplate[ARRAY_SIZE(szGamelistLocalisationTemplate) - 1] = _T('\0');

	// Initialize OPENFILENAME structure
	MakeOfn();

	// STATIC buffer for dialog title (must stay valid during dialog lifetime)
	static TCHAR szTitle[256];
	_sntprintf(szTitle, ARRAY_SIZE(szTitle), _T("%s"), FBALoadStringEx(hAppInst, IDS_LOCAL_GL_SELECT, true));
	szTitle[ARRAY_SIZE(szTitle) - 1] = _T('\0');

	ofn.lpstrTitle = szTitle;

	// Enable overwrite prompt if needed
	ofn.Flags |= OFN_OVERWRITEPROMPT;

	// Pause emulation during dialog operation
	INT32 bOldPause = bRunPause;
	bRunPause = 1;

	// Open file dialog (correct for loading template)
	INT32 nRet = GetOpenFileName(&ofn);

	// Restore emulation state
	bRunPause = bOldPause;

	// Return if user cancelled the dialog
	if (0 == nRet)
		return 1;

	// Activate game list localization and reload
	nGamelistLocalisationActive = true;
	BurnerDoGameListLocalisation();

	return 0;
}

// Create game list translation template file
INT32 FBALocaliseGamelistCreateTemplate()
{
	// Safe filename initialization (TCHAR compatible, no overflow)
	_sntprintf(szGamelistLocalisationTemplate, ARRAY_SIZE(szGamelistLocalisationTemplate), _T("template"));
	szGamelistLocalisationTemplate[ARRAY_SIZE(szGamelistLocalisationTemplate) - 1] = _T('\0');

	// Initialize OPENFILENAME structure
	MakeOfn();

	// STATIC buffer: safe for system dialog (no dangling pointer)
	static TCHAR szTitle[256];
	_sntprintf(szTitle, ARRAY_SIZE(szTitle), _T("%s"), FBALoadStringEx(hAppInst, IDS_LOCAL_GL_CREATE, true));
	szTitle[ARRAY_SIZE(szTitle) - 1] = _T('\0');

	ofn.lpstrTitle = szTitle;

	// Enable overwrite prompt
	ofn.Flags |= OFN_OVERWRITEPROMPT;

	// Pause emulation during dialog
	INT32 bOldPause = bRunPause;
	bRunPause = 1;

	// Show save file dialog (CORRECT for creating template)
	INT32 nRet = GetSaveFileName(&ofn);

	// Restore emulation state
	bRunPause = bOldPause;

	// Canceled by user
	if (0 == nRet)
		return 1;

	// Create the actual template file
	BurnerGameListCreateTemplate();

	return 0;
}

#undef MAX_LST_LINE_LEN
#undef MAX_LST_GAMES
