// FB Neo IPS Mangler^H^H^H^H^H^H^HManager
#include "burner.h"
#include "burnint.h"

#define NUM_LANGUAGES		12
#define MAX_NODES			1024
#define MAX_ACTIVE_PATCHES	1024

static HWND hIpsDlg			= NULL;
static HWND hParent			= NULL;
static HWND hIpsList		= NULL;

INT32 nIpsSelectedLanguage	= 0;
static TCHAR szFullName[1024];
static TCHAR szLanguages[NUM_LANGUAGES][32];
static TCHAR szLanguageCodes[NUM_LANGUAGES][6];
static TCHAR szPngName[MAX_PATH];

static HTREEITEM hItemHandles[MAX_NODES];

// Global handle position counter
static INT32 nHandlePos     = 0;

static INT32 nPatchIndex	= 0;
static INT32 nNumPatches	= 0;
static HTREEITEM hPatchHandlesIndex[MAX_NODES];
static TCHAR szPatchFileNames[MAX_NODES][MAX_PATH];

static HBRUSH hWhiteBGBrush;
static HBITMAP hBmp			= NULL;
static HBITMAP hPreview		= NULL;

static TCHAR szDriverName[100];

static INT32 nRomOffset		= 0;
UINT32 nIpsDrvDefine		= 0;
UINT32 nIpsMemExpLen[SND2_ROM + 1] = { 0 };

static TCHAR szIpsActivePatches[MAX_ACTIVE_PATCHES][MAX_PATH];

// =========================================================================
// Conditional compilation: check if SDK provides TreeView functions
// If not provided, use SendMessage to implement manually
// This ensures compilation across different Windows SDK versions
// =========================================================================

#ifndef TreeView_InsertItem
#define TreeView_InsertItem(hwnd, pinsert)							\
	((HTREEITEM)SendMessage((hwnd), TVM_INSERTITEM, 0, (LPARAM)(pinsert)))
#endif

#ifndef TreeView_DeleteAllItems
#define TreeView_DeleteAllItems(hwnd)								\
	((BOOL)SendMessage((hwnd), TV_FIRST + 1, 0, (LPARAM)TVI_ROOT))
#endif

#ifndef TreeView_Expand
#define TreeView_Expand(hwnd, hitem, code)							\
	((BOOL)SendMessage((hwnd), TV_FIRST + 2, (WPARAM)(code), (LPARAM)(hitem)))
#endif

#ifndef TreeView_GetNextSibling
#define TreeView_GetNextSibling(hwnd, hitem)						\
	((HTREEITEM)SendMessage((hwnd), TV_FIRST + 10, 0x0001, (LPARAM)(hitem)))
#endif

#ifndef TreeView_GetChild
#define TreeView_GetChild(hwnd, hitem)								\
	((HTREEITEM)SendMessage((hwnd), TV_FIRST + 10, 0x0004, (LPARAM)(hitem)))
#endif

#ifndef TreeView_GetNextItem
#define TreeView_GetNextItem(hwnd, hitem, flag)						\
	(HTREEITEM)SendMessage((hwnd), TV_FIRST + 10, (WPARAM)(flag), (LPARAM)(hitem))
#endif

#ifndef TreeView_GetSelection
#define TreeView_GetSelection(hwnd)									\
	TreeView_GetNextItem((hwnd), NULL, 0x0009)
#endif

#ifndef TreeView_SelectItem
#define TreeView_SelectItem(hwnd, hitem)							\
	(HTREEITEM)SendMessage((hwnd), TV_FIRST + 11, 0, (LPARAM)(hitem))
#endif

#ifndef TreeView_GetItem
#define TreeView_GetItem(hwnd, pitem)								\
	((BOOL)SendMessage((hwnd), TVM_GETITEM, 0, (LPARAM)(pitem)))
#endif

#ifndef TreeView_HitTest
#define TreeView_HitTest(hwnd, lpht)								\
	(HTREEITEM)SNDMSG((hwnd), TV_FIRST + 17, 0, (LPARAM)(LPTV_HITTESTINFO)(lpht))
#endif

#ifndef TreeView_SetItemState
#define TreeView_SetItemState(hwndTV, hti, data, _mask)				\
{																	\
	TVITEM _ms_TVi;													\
	_ms_TVi.mask      = TVIF_STATE;									\
	_ms_TVi.hItem     = hti;										\
	_ms_TVi.stateMask = _mask;										\
	_ms_TVi.state     = data;										\
	SNDMSG((hwndTV), TVM_SETITEM, 0, (LPARAM)(TV_ITEM *)&_ms_TVi);	\
}
#endif

#ifndef TreeView_SetCheckState
#define TreeView_SetCheckState(hwndTV, hti, fCheck)					\
  TreeView_SetItemState(hwndTV, hti, INDEXTOSTATEIMAGEMASK((fCheck)?2:1), TVIS_STATEIMAGEMASK)
#endif

#ifndef TreeView_GetCheckState
#define TreeView_GetCheckState(hwndTV, hti)							\
   ((((UINT)(SNDMSG((hwndTV), TVM_GETITEMSTATE, (WPARAM)(hti), TVIS_STATEIMAGEMASK))) >> 12) -1)
#endif


// Check if a file exists and is not a directory
static inline INT32 FileExists(const TCHAR* pszName)
{
	if (IsStrEmpty(pszName))
		return 0;

	DWORD dwAttrib = GetFileAttributes(pszName);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
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

// Safely combine a directory path and a relative file path
static TCHAR* GameIpsConfigName()
{
	// Check if driver name is empty
	if (_T('\0') == szDriverName[0])
		return NULL;

	// Calculate required buffer size: "config\ips\" + driver + ".ini" + null terminator
	// Using _sctprintf to calculate exact size needed
	INT32 nRequiredSize = _sctprintf(_T("config\\ips\\%s.ini"), szDriverName);
	if (nRequiredSize < 0)
		return NULL;

	// Add 1 for null terminator
	nRequiredSize++;

	// Allocate memory for the configuration file path
	TCHAR* pszConfigPath = (TCHAR*)malloc(nRequiredSize * sizeof(TCHAR));
	if (!pszConfigPath)
		return NULL;

	// Use _sntprintf for safe string formatting
	INT32 nResult = _sntprintf(pszConfigPath, nRequiredSize, _T("config\\ips\\%s.ini"), szDriverName);

	// Check for formatting errors
	if (nResult < 0) {
		free_s((void**)&pszConfigPath);
		return NULL;
	}

	return pszConfigPath;
}

// Check if a file is a .dat file
static INT32 IsDatFile(const TCHAR* filePath)
{
	return IsFileExtensionMatch(filePath, _T(".dat")) ? 1 : 0;
}

// Get the number of IPS patch files in the patch directory
INT32 GetIpsNumPatches()
{
	const TCHAR* pszDrvName = BurnDrvGetText(DRV_NAME);
	if (IsStrEmpty(pszDrvName))
		return 0;

	if (_T('\0') == szAppIpsPath[0])
		return 0;

	TCHAR filePath[MAX_PATH] = { 0 };

	// Safely concatenate the path
	INT32 nResult = _sntprintf(filePath, MAX_PATH, _T("%s%s\\"), szAppIpsPath, pszDrvName);

	// Path truncation/failure check
	if (nResult < 0 || nResult >= MAX_PATH)
		return 0;

	return TraverseDirectoryFiles(filePath, IsDatFile, false);
}

// Removes trailing \r and \n characters from a string
static size_t remove_trailing_crlf(TCHAR* str)
{
	if (!str)
		return 0;

	size_t len = _tcslen(str);
	while (len > 0) {
		if (_T('\n') == str[len - 1] || _T('\r') == str[len - 1]) {
			str[len - 1] = _T('\0');
			len--;
		} else
			break;
	}
	return len;
}

// Skip leading spaces and tabs in a string
static TCHAR* skip_spaces_and_tabs(TCHAR* str)
{
	if (!str)
		return NULL;

	TCHAR* p = str;
	// Skip leading spaces and tabs
	while (_T(' ') == *p || _T('\t') == *p)
		p++;

	return p;
}

// Reads patch description from file by language code
static TCHAR* GetPatchDescByLangcode(FILE* fp, INT32 nLang)
{
	// Parameter validation
	if (!fp || nLang < 0 || nLang >= NUM_LANGUAGES)
		return NULL;

	TCHAR langtag[32] = { 0 };
	// Construct language tag
	INT32 written = _sntprintf(langtag, ARRAY_SIZE(langtag), _T("[%s]"), szLanguageCodes[nLang]);
	langtag[ARRAY_SIZE(langtag) - 1] = _T('\0');

	size_t tagLen = _tcslen(langtag);

	// Reset file pointer to beginning
	if (0 != fseek(fp, 0, SEEK_SET))
		return NULL;

	TCHAR  line_buffer[4096] = { 0 };
	TCHAR* desc              = NULL;
	bool   found             = false;
	bool   in_target_section = false;


	// Read file line by line
	while (_fgetts(line_buffer, ARRAY_SIZE(line_buffer), fp)) {
		remove_trailing_crlf(line_buffer);

		// Skip empty lines
		if (_T('\0') == line_buffer[0])
			continue;

		// Check for section tag [xxx]
		if (_T('[') == line_buffer[0]) {
			// If already in target section, new tag means end
			if (in_target_section)
				break;

			// Check if this is the target language tag
			if (0 == _tcsncmp(langtag, line_buffer, tagLen))
				found = in_target_section = true;

			continue;
		}

		// Read target section content
		if (in_target_section) {
			size_t current_len = (desc)        ? _tcslen(desc)        : 0;
			size_t line_len    = (line_buffer) ? _tcslen(line_buffer) : 0;
			size_t new_len     = current_len + line_len + ((current_len > 0) ? 3 : 1);

			TCHAR* new_desc = (TCHAR*)malloc(new_len * sizeof(TCHAR));
			if (!new_desc) {
				free_s((void**)&desc);
				return NULL;
			}

			if (desc)
				_sntprintf(new_desc, new_len, _T("%s\r\n%s"), desc, line_buffer);
			else
				_tcsncpy(new_desc, line_buffer, new_len - 1);

			new_desc[new_len - 1] = _T('\0');

			free_s((void**)&desc);
			desc = new_desc;
		}
	}

	// Language section not found
	if (!found) {
		free_s((void**)&desc);
		return NULL;
	}

	// Return empty string for empty content
	if (!desc)
		desc = (TCHAR*)calloc(1, sizeof(TCHAR));
	
	return desc;		// Caller must free with free(_s)
}

// Unified Context Structure - Shared by FillListBox and RefreshPatch
struct DescParams {
	TCHAR* patchDesc;	// Pointer to store the retrieved patch description
	INT32  langCode;	// Target language code for description lookup
};

// Callback to retrieve patch description from file with language fallback logic
static void PatchDescriptionCallback(const TCHAR*, FILE* fp, void* userData)
{
	DescParams* params = (DescParams*)userData;
	params->patchDesc = NULL;

	// Invalid file handle, return directly
	if (!fp)
		return;

	// Step 1: First try the currently selected language
	params->patchDesc = GetPatchDescByLangcode(fp, params->langCode);
	if (params->patchDesc)
		return;

	// Step 2: Fallback - iterate all available languages (skip the failed one)
	for (INT32 i = 0; i < NUM_LANGUAGES; i++) {
		// Skip the language we already tried and failed
		if (params->langCode == i)
			continue;

		params->patchDesc = GetPatchDescByLangcode(fp, i);
		if (params->patchDesc)
			break;
	}
}

// Unified Safe Wrapper - Call this to read patch descriptions
static TCHAR* SafeReadPatchDescription(TCHAR* fileName, INT32 langCode)
{
	DescParams params = { 0 };
	params.langCode = langCode;

	// Safe auto open, process, and close file
	SafeProcessTextFile(fileName, PatchDescriptionCallback, &params);
	return params.patchDesc;
}

// Find child node under specified parent
static HTREEITEM FindChildNode(HWND hTree, HTREEITEM hParentItem, const TCHAR* pszText)
{
	if (!hTree || IsStrEmpty(pszText))
		return NULL;

	HTREEITEM hChild = TreeView_GetChild(hTree, hParentItem);
	TCHAR szBuf[256] = { 0 };

	while (hChild) {
		TVITEM ti     = { 0 };
		ti.hItem      = hChild;
		ti.mask       = TVIF_TEXT;
		ti.pszText    = szBuf;
		ti.cchTextMax = 255;

		if (TreeView_GetItem(hTree, &ti) && (0 == _tcsicmp(szBuf, pszText)))
			return hChild;

		hChild = TreeView_GetNextSibling(hTree, hChild);
	}
	return NULL;
}

// Process .dat file callback
static INT32 ProcessPatchFile(const TCHAR* path)
{
	// Null check
	if (IsStrEmpty(path))
		return 0;

	const TCHAR* ext = _tcsrchr(path, _T('.'));
	if (!ext || (0 != _tcsicmp(ext, _T(".dat"))) || (nNumPatches >= 1024))
		return 0;

	TCHAR* desc = SafeReadPatchDescription((TCHAR*)path, nIpsSelectedLanguage);
	if (!desc) {
		desc = (TCHAR*)malloc(MAX_PATH * sizeof(TCHAR));
		if (!desc)
			return 0;

		const TCHAR* fn  = _tcsrchr(path, _T('\\'));
		const TCHAR* src = (fn) ? (fn + 1) : path;
		_tcsncpy(desc, src, MAX_PATH - 1);
		desc[MAX_PATH - 1] = _T('\0');
	}

	TCHAR name[256] = { 0 };
	for (INT32 i = 0; desc[i] && (i < 255); i++) {
		if ((_T('\r') == desc[i]) || (_T('\n') == desc[i]))
			break;
		name[i] = desc[i];
	}
	name[255] = _T('\0');
	free_s((void**)&desc);

	if (0 == _tcslen(name))
		return 0;

	TCHAR buf[256] = { 0 };
	_tcsncpy(buf, name, 255);
	buf[255] = _T('\0');

	TCHAR* tok = _strqtoken(buf, _T("/"));
	HTREEITEM parent = TVI_ROOT;
	HTREEITEM node   = NULL;

	TV_INSERTSTRUCT is = { 0 };
	is.item.mask       = TVIF_TEXT | TVIF_PARAM;
	is.hInsertAfter    = TVI_LAST;

	while (tok) {
		node = FindChildNode(hIpsList, parent, tok);
		if (!node) {
			is.hParent      = parent;
			is.item.pszText = tok;
			node = TreeView_InsertItem(hIpsList, &is);

			if (node && (nHandlePos < 1024))
				hItemHandles[nHandlePos++] = node;
		}

		if (!node)
			break;

		parent = node;
		tok    = _strqtoken(NULL, _T("/"));
	}

	if (node && (nNumPatches < 1024)) {
		hPatchHandlesIndex[nNumPatches] = node;
		_tcsncpy(szPatchFileNames[nNumPatches], path, MAX_PATH - 1);
		szPatchFileNames[nNumPatches][MAX_PATH - 1] = _T('\0');
		nNumPatches++;
		return 1;
	}

	return 0;
}

//  Load IPS patch files and populate the tree view
static void FillListBox()
{
	TCHAR dir[MAX_PATH] = { 0 };
	INT32 nLen = _sntprintf(dir, MAX_PATH, _T("%s%s\\"), szAppIpsPath, szDriverName);
	dir[MAX_PATH - 1] = _T('\0');

	DWORD attr = GetFileAttributes(dir);
	if ((INVALID_FILE_ATTRIBUTES == attr) || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
		TreeView_DeleteAllItems(hIpsList);
		nNumPatches = nHandlePos = 0;
		return;
	}

	TreeView_DeleteAllItems(hIpsList);
	memset(hItemHandles, 0, sizeof(hItemHandles));
	nNumPatches = nHandlePos = 0;

	(void)TraverseDirectoryFiles(dir, ProcessPatchFile, false);

	for (INT32 i = 0; i < nHandlePos; i++) {
		if (hItemHandles[i])
			TreeView_Expand(hIpsList, hItemHandles[i], TVE_EXPAND);
	}
}

// Get the number of active IPS patches by counting non-empty entries in the active patch array
static INT32 GetIpsNumActivePatches()
{
	INT32 nActivePatches = 0;

	for (INT32 i = 0; i < MAX_ACTIVE_PATCHES; i++) {
		if (_T('\0') != szIpsActivePatches[i][0])
			nActivePatches++;
	}

	return nActivePatches;
}

// Safe path concatenation with buffer overflow protection
static bool SafePathConcatenate(TCHAR* dest, const TCHAR* src, UINT32 destSize) {
	if (!dest || !src || 0 == destSize)
		return false;

	dest[destSize - 1] = _T('\0');

	size_t currentLen = _tcslen(dest);
	size_t remain     = destSize - currentLen - 1;

	// Check if there's enough space (including null terminator)
	if (0 == remain)
		return false;

	_tcsncat(dest, src, remain);
	return true;
}

// Callback function to process each line of the patch configuration file
static void LoadIpsPatchLine(const TCHAR* fileName, FILE* fp, void* userData)
{
	if (!fp)
		return;

	INT32* pCount = (INT32*)userData;
	TCHAR  szLine[MAX_PATH] = { 0 };

	while (_fgetts(szLine, MAX_PATH, fp) && *pCount < MAX_ACTIVE_PATCHES) {
		size_t len = remove_trailing_crlf(szLine);

		// Skip comment lines
		if (0 == _tcsnicmp(szLine, _T("//"), 2))
			continue;
		if (0 == len)
			continue;

		// Skip leading spaces and tabs
		TCHAR* pStart = skip_spaces_and_tabs(szLine);
		if (_T('\0') == *pStart)
			continue;

		// Concatenate path
		TCHAR* dest = szIpsActivePatches[*pCount];
		dest[0] = _T('\0');

		// Use safe path concatenation
		if (!SafePathConcatenate(dest, szAppIpsPath, MAX_PATH)) {
			continue;						// Path too long, skip
		}
		if (!SafePathConcatenate(dest, szDriverName, MAX_PATH)) {
			dest[0] = _T('\0');				// Clear destination string
			continue;
		}
		if (!SafePathConcatenate(dest, _T("\\"), MAX_PATH)) {
			dest[0] = _T('\0');				// Clear destination string
			continue;
		}
		if (!SafePathConcatenate(dest, pStart, MAX_PATH)) {
			dest[0] = _T('\0');
			continue;
		}

		// Ensure null termination
		dest[MAX_PATH - 1] = _T('\0');
		(*pCount)++;
	}
}

// Load active IPS patches from configuration file
INT32 LoadIpsActivePatches()
{
	const size_t nSize = ARRAY_SIZE(szDriverName) - 1;

	// 1. Copy driver name
	szDriverName[0] = _T('\0');
	_tcsncat(szDriverName, BurnDrvGetText(DRV_NAME), nSize);
	szDriverName[nSize] = _T('\0');			// Ensure null termination

	// 2. Clear patch array
	for (INT32 i = 0; i < MAX_ACTIVE_PATCHES; i++)
		szIpsActivePatches[i][0] = _T('\0');

	// 3. Initialize patch count
	INT32 nActivePatches = 0;

	// 4. Generate configuration file name (dynamic allocation)
	TCHAR* pszConfigFile = GameIpsConfigName();
	if (!pszConfigFile)
		return 0;							// Failed to generate config file name

	// 5. Process the config file
	SafeProcessTextFile(pszConfigFile, LoadIpsPatchLine, &nActivePatches);

	// 6. Free the dynamically allocated configuration file name
	free_s((void**)&pszConfigFile);

	return nActivePatches;
}

// Load active IPS patches and auto‑check corresponding items in TreeView
// Compares pure filename extracted from full path with patch name list
static void CheckActivePatches()
{
	INT32 nActivePatches = LoadIpsActivePatches();

	// Skip if no active patches or patch list is empty
	if (nActivePatches <= 0 || nNumPatches <= 0)
		return;

	for (INT32 i = 0; i < nActivePatches; i++) {
		const TCHAR* activePatch = szIpsActivePatches[i];
		// Skip invalid filename
		if (IsStrEmpty(activePatch))
			continue;

		// Match with patch name list
		for (INT32 j = 0; j < nNumPatches; j++) {
			const TCHAR* patchFile = szPatchFileNames[j];
			if (IsStrEmpty(patchFile))
				continue;

			// Case‑insensitive filename comparison
			if (0 == _tcsicmp(activePatch, patchFile)) {
				// Check matched TreeView item
				TreeView_SetCheckState(hIpsList, hPatchHandlesIndex[j], TRUE);
				break;						// Exit inner loop once matched
			}
		}
	}
}

// Initialize IPS Manager Dialog
static INT32 IpsManagerInit()
{
	TCHAR szText[1024]   = { 0 };
	const size_t bufSize = ARRAY_SIZE(szText);
	size_t currentLen    = 0;

	// Build combined game full names safely
	const TCHAR* pszName = BurnDrvGetText(DRV_FULLNAME);
	if (!IsStrEmpty(pszName)) {
		_tcsncat(szText, pszName, bufSize - 1);
		currentLen = _tcslen(szText);
	}

	const size_t sepLen = _tcslen(_T(SEPERATOR_2));
	while (pszName = BurnDrvGetText(DRV_NEXTNAME | DRV_FULLNAME)) {
		if (IsStrEmpty(pszName))
			continue;

		const size_t nameLen = _tcslen(pszName);
		const size_t remain  = bufSize - currentLen - 1;

		// Stop if no enough space for separator + name
		if (sepLen + nameLen >= remain)
			break;

		// Append safely
		_tcsncat(szText, _T(SEPERATOR_2), remain);
		_tcsncat(szText, pszName, remain - sepLen);

		// Update length (optimized, avoid repeated _tcslen)
		currentLen += sepLen + nameLen;
	}

	// Safe copy to global full name buffer
	_tcsncpy(szFullName, szText, ARRAY_SIZE(szFullName) - 1);
	szFullName[ARRAY_SIZE(szFullName) - 1] = _T('\0');

	// Set dialog window title
	const TCHAR* pszTitle = FBALoadStringEx(hAppInst, IDS_IPSMANAGER_TITLE, TRUE);
	if (IsStrEmpty(pszTitle))
		pszTitle = _T("IPS Manager");

	_sntprintf(szText, bufSize, _T("%s") _T(SEPERATOR_1) _T("%s"), pszTitle, szFullName);
	szText[bufSize - 1] = _T('\0');

	if (hIpsDlg)
		SetWindowText(hIpsDlg, szText);

	// Initialize language list (ID += 2 per iteration, valid design)
	static const TCHAR* const langCodes[NUM_LANGUAGES] = {
		_T("en_US"), _T("zh_CN"), _T("zh_TW"), _T("ja_JP"),
		_T("ko_KR"), _T("fr_FR"), _T("es_ES"), _T("it_IT"),
		_T("de_DE"), _T("pt_BR"), _T("pl_PL"), _T("hu_HU")
	};

	static const TCHAR* const defaultLangNames[NUM_LANGUAGES] = {
		_T("English (US)"), _T("Simplified Chinese"), _T("Traditional Chinese"), _T("Japanese"),
		_T("Korean"),       _T("French"),             _T("Spanish"),             _T("Italian"),
		_T("German"),       _T("Portuguese"),         _T("Polish"),              _T("Hungarian")
	};

	for (INT32 i = 0; i < NUM_LANGUAGES; i++) {
		const UINT32 resID    = IDS_LANG_ENGLISH_US + (i * 2);
		const TCHAR* langName = FBALoadStringEx(hAppInst, resID, true);

		// Fallback to default name if resource loading fails
		if (IsStrEmpty(langName))
			langName = defaultLangNames[i];

		// Safe copy language name
		_sntprintf(szLanguages[i], ARRAY_SIZE(szLanguages[i]), _T("%s"), langName);
		szLanguages[i][ARRAY_SIZE(szLanguages[i]) - 1] = _T('\0');

		// Safe copy language code
		_tcsncpy(szLanguageCodes[i], langCodes[i], ARRAY_SIZE(szLanguageCodes[i]) - 1);
		szLanguageCodes[i][ARRAY_SIZE(szLanguageCodes[i]) - 1] = _T('\0');

		// Add to combo box
		if (hIpsDlg)
			SendDlgItemMessage(hIpsDlg, IDC_CHOOSE_LIST, CB_ADDSTRING, 0, (LPARAM)szLanguages[i]);
	}

	// Restore selected language
	if (hIpsDlg) {
		const INT32 selIndex = (nIpsSelectedLanguage >= 0 && nIpsSelectedLanguage < NUM_LANGUAGES) ? nIpsSelectedLanguage : 0;
		SendDlgItemMessage(hIpsDlg, IDC_CHOOSE_LIST, CB_SETCURSEL, selIndex, 0);
		nIpsSelectedLanguage = selIndex;
	}

	// Initialize controls
	if (hIpsDlg)
		hIpsList = GetDlgItem(hIpsDlg, IDC_TREE1);

	// Safe copy driver name
	const TCHAR* driverName = BurnDrvGetText(DRV_NAME);
	if (!IsStrEmpty(driverName)) {
		_tcsncpy(szDriverName, driverName, ARRAY_SIZE(szDriverName) - 1);
		szDriverName[ARRAY_SIZE(szDriverName) - 1] = _T('\0');
	} else
		_tcscpy(szDriverName, _T("Unknown"));

	// Fill patch list and check active states
	FillListBox();
	CheckActivePatches();

	return 0;
}

// Safely replace file extension (e.g. dat → png)
static bool ReplaceFileExtension(TCHAR* path, size_t bufSize, const TCHAR* newExt)
{
	if (IsStrEmpty(path) || IsStrEmpty(newExt) || bufSize < 4)
		return false;

	TCHAR* dotPos = _tcsrchr(path, _T('.'));
	if (!dotPos)
		return false;

	size_t extLen = _tcslen(newExt);
	size_t remain = bufSize - (size_t)(dotPos - path) - 1;

	if (extLen >= remain)
		return false;

	_tcsncpy(dotPos + 1, newExt, remain);
	path[bufSize - 1] = _T('\0');
	return true;
}


// Purpose:  Show details for the selected patch (description + preview)
static void RefreshPatch()
{
	// Release old bitmap first
	if (hBmp) {
		DeleteObject(hBmp);
		hBmp = NULL;
	}

	// Reset preview filename
	szPngName[0] = _T('\0');

	// Get controls
	HWND hCommentCtrl = GetDlgItem(hIpsDlg, IDC_TEXTCOMMENT);
	HWND hPreviewCtrl = GetDlgItem(hIpsDlg, IDC_SCREENSHOT_H);

	// Clear UI
	if (hCommentCtrl)
		SendMessage(hCommentCtrl, WM_SETTEXT, 0, (LPARAM)NULL);
	if (hPreviewCtrl)
		SendMessage(hPreviewCtrl, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hPreview);

	// Get selected item
	HTREEITEM hSelItem = NULL;
	if (hIpsList)
		hSelItem = TreeView_GetSelection(hIpsList);

	// Process selected patch
	for (INT32 i = 0; i < nNumPatches; i++) {
		if (hSelItem != hPatchHandlesIndex[i])
			continue;
		// Read description
		TCHAR* pDesc = SafeReadPatchDescription(szPatchFileNames[i], nIpsSelectedLanguage);
		if (!pDesc) {
			pDesc = (TCHAR*)malloc(MAX_PATH * sizeof(TCHAR));
			if (pDesc) {
				_tcsncpy(pDesc, szPatchFileNames[i], MAX_PATH - 1);
				pDesc[MAX_PATH - 1] = _T('\0');
			} else if (hCommentCtrl)
				SendMessage(hCommentCtrl, WM_SETTEXT, 0, (LPARAM)szPatchFileNames[i]);
		}
		// Show description
		if (!IsStrEmpty(pDesc) && hCommentCtrl)
			SendMessage(hCommentCtrl, WM_SETTEXT, 0, (LPARAM)pDesc);

		free_s((void**)&pDesc);

		// Build image path
		TCHAR szImgPath[MAX_PATH] = { 0 };
		_tcsncpy(szImgPath, szPatchFileNames[i], MAX_PATH - 1);
		ReplaceFileExtension(szImgPath, ARRAY_SIZE(szImgPath), _T("png"));

		// Load PNG
		FILE* pFile = _tfopen(szImgPath, _T("rb"));
		if (pFile) {
			_tcsncpy(szPngName, szImgPath, MAX_PATH - 1);

			HBITMAP hNewBmp = PNGLoadBitmap(hIpsDlg, pFile, 304, 228, 3);
			fclose(pFile);

			if (hNewBmp && hPreviewCtrl) {
				hBmp = hNewBmp;
				SendMessage(hPreviewCtrl, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
			}
		}
		break;
	}
}

// Save enabled patch list to configuration file
static void SavePatches()
{
	INT32 nActivePatches = 0;

	// Initialize patch array
	for (INT32 i = 0; i < MAX_ACTIVE_PATCHES; i++)
		szIpsActivePatches[i][0] = _T('\0');

	// Collect checked patches
	for (INT32 i = 0; i < nNumPatches && nActivePatches < MAX_ACTIVE_PATCHES; i++) {
		INT32 nChecked = TreeView_GetCheckState(hIpsList, hPatchHandlesIndex[i]);
		if (!nChecked)
			continue;

		_tcsncpy(szIpsActivePatches[nActivePatches], szPatchFileNames[i], MAX_PATH - 1);
		szIpsActivePatches[nActivePatches][MAX_PATH - 1] = _T('\0');
		nActivePatches++;
	}

	TCHAR* configPath = GameIpsConfigName();
	if (IsStrEmpty(configPath)) {
		free_s((void**)&configPath);
		return;
	}

	FILE* fp = _tfopen(configPath, _T("wt"));
	if (!fp) {
		free_s((void**)&configPath);
		return;
	}

	// Clean, safe format string
	_ftprintf(fp, _T("// %s v%s --- IPS Config File for %s (%s)\n\n"), _T(APP_TITLE), szAppBurnVer, szDriverName, szFullName);

	// Write patch filenames
	for (INT32 i = 0; i < nActivePatches; i++) {
		const TCHAR* fileName = ExtractFileNameFromPath(szIpsActivePatches[i]);
		if (!IsStrEmpty(fileName))
			_ftprintf(fp, _T("%s\n"), fileName);
	}

	fclose(fp);
	free_s((void**)&configPath);
}

// Cleanup and exit IPS manager dialog
static void IpsManagerExit()
{
	// Safely clear preview image (avoid NULL dialog handle)
	if (hIpsDlg)
		SendDlgItemMessage(hIpsDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);

	// Clear language buffers
	for (INT32 i = 0; i < NUM_LANGUAGES; i++) {
		szLanguages[i][0]     = _T('\0');
		szLanguageCodes[i][0] = _T('\0');
	}

	// Reset tree view handles and patch data
	memset(hItemHandles,       0, MAX_NODES * sizeof(HTREEITEM));
	memset(hPatchHandlesIndex, 0, MAX_NODES * sizeof(HTREEITEM));
	nPatchIndex = nNumPatches = 0;

	// Clear patch filename array
	for (INT32 i = 0; i < MAX_NODES; i++)
		szPatchFileNames[i][0] = _T('\0');

	// Safe release of GDI objects
	if (hBmp) {
		DeleteObject(hBmp);
		hBmp = NULL;
	}
	if (hPreview) {
		DeleteObject(hPreview);
		hPreview = NULL;
	}

	// Cleanup custom background brush
	if (hWhiteBGBrush) {
		DeleteObject(hWhiteBGBrush);
		hWhiteBGBrush = NULL;
	}

	hParent = NULL;

	// Close dialog safely
	if (hIpsDlg) {
		EndDialog(hIpsDlg, 0);
		hIpsDlg = NULL;
	}
}

static void IpsOkay()
{
	SavePatches();
	IpsManagerExit();
}

// Main dialog procedure for IPS Manager
static INT_PTR CALLBACK DefInpProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg) {
		case WM_INITDIALOG: {
			hIpsDlg  = hDlg;
			hIpsList = GetDlgItem(hDlg, IDC_TREE1);

			// Create solid white background brush for comment static control
			hWhiteBGBrush = CreateSolidBrush(RGB(0xFF, 0xFF, 0xFF));

			// Load default preview bitmap and assign to static image control
			hPreview = PNGLoadBitmap(hDlg, NULL, 304, 228, 2);
			if (hPreview)
				SendDlgItemMessage(hDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hPreview);

			// Enable checkbox style for tree‑view control
			HWND hTree = GetDlgItem(hDlg, IDC_TREE1);
			if (hTree) {
				LONG_PTR style = GetWindowLongPtr(hTree, GWL_STYLE);
				style         |= TVS_CHECKBOXES;
				SetWindowLongPtr(hTree, GWL_STYLE, style);
			}

			// Initialize IPS manager core data & UI
			IpsManagerInit();

			// Preserve original driver active state temporarily
			INT32 nBurnDrvActiveOld = nBurnDrvActive;		// RockyWall Add

			// Center dialog window on screen
			WndInMid(hDlg, hScrnWnd);

			// Set keyboard focus to dialog
			SetFocus(hDlg);									// Enable Esc=close

			// Restore original driver active state
			nBurnDrvActive = nBurnDrvActiveOld;				// RockyWall Add

			return TRUE;									// Standard return for successful initialization
		}

		case WM_LBUTTONDBLCLK: {
			POINT pt;
			RECT rc;

			// Detect double‑click on preview image area
			if (GetCursorPos(&pt) && GetWindowRect(GetDlgItem(hDlg, IDC_SCREENSHOT_H), &rc)) {
				if (PtInRect(&rc, pt) && !IsStrEmpty(szPngName)) {
					// Verify PNG file exists before opening
					FILE* fp = _tfopen(szPngName, _T("rb"));
					if (fp) {
						fclose(fp);
						// Open preview image with system‑associated viewer
						ShellExecute(hDlg, NULL, szPngName, NULL, NULL, SW_SHOWNORMAL);
					}
				}
			}
			return TRUE;
		}

		case WM_COMMAND: {
			INT32 wID    = LOWORD(wParam);
			INT32 notify = HIWORD(wParam);

			// Handle button click events
			if (BN_CLICKED == notify) {
				switch (wID) {
					case IDOK:
						IpsOkay();							// Save changes and close manager
						break;

					case IDCANCEL:
						SendMessage(hDlg, WM_CLOSE, 0, 0);	// Trigger close flow
						break;

					case IDC_IPSMAN_DESELECTALL:
						// Deselect all patches with single‑loop optimization (O(n))
						for (INT32 i = 0; i < nNumPatches; i++)
							TreeView_SetCheckState(hIpsList, hPatchHandlesIndex[i], FALSE);
						break;
				}
			}

			// Handle language combobox selection change
			if (IDC_CHOOSE_LIST == wID && CBN_SELCHANGE == notify) {
				nIpsSelectedLanguage = (INT32)SendMessage(GetDlgItem(hDlg, IDC_CHOOSE_LIST), CB_GETCURSEL, 0, 0);
				TreeView_DeleteAllItems(hIpsList);
				FillListBox();								// Rebuild patch tree‑view
				RefreshPatch();								// Refresh preview & description
				return TRUE;
			}
			break;
		}

		case WM_NOTIFY: {
			NMHDR* pNmHdr = (NMHDR*)lParam;

			// Process tree‑view control notifications
			if (IDC_TREE1 == LOWORD(wParam)) {
				switch (pNmHdr->code) {
					case TVN_SELCHANGED:
						RefreshPatch();						// Update preview when patch selection changes
						return TRUE;

					case NM_DBLCLK:
						// Disable auto‑expand on tree‑item double‑click
						SetWindowLongPtr(hDlg, DWLP_MSGRESULT, TRUE);
						return TRUE;

					case NM_CLICK: {
						POINT pt;
						GetCursorPos(&pt);
						ScreenToClient(hIpsList, &pt);

						TVHITTESTINFO thi = { 0 };
						thi.pt = pt;
						TreeView_HitTest(hIpsList, &thi);

						// Select tree item when clicking its checkbox state icon
						if (thi.flags & TVHT_ONITEMSTATEICON)
							TreeView_SelectItem(hIpsList, thi.hItem);
						return TRUE;
					}
				}
			}

			// Disable combobox double‑click behavior
			if (IDC_CHOOSE_LIST == LOWORD(wParam) && NM_DBLCLK == pNmHdr->code) {
				SetWindowLongPtr(hDlg, DWLP_MSGRESULT, TRUE);
				return TRUE;
			}

			break;
		}

		case WM_CTLCOLORSTATIC: {
			HWND hCtrl = (HWND)lParam;
			// Assign custom white background brush to patch description static text
			if (hCtrl == GetDlgItem(hDlg, IDC_TEXTCOMMENT))
				return (INT_PTR)hWhiteBGBrush;
			break;
		}

		case WM_CLOSE:
			// Central cleanup: release all GDI objects, reset states, close dialog
			IpsManagerExit();
			return TRUE;
	}

	return FALSE;
}

INT32 IpsManagerCreate(HWND hParentWND)
{
	hParent = hParentWND;
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_IPS_MANAGER), hParent, (DLGPROC)DefInpProc);
	return 1;
}

// Game patching
/*
#define UTF8_SIGNATURE "\xef\xbb\xbf"

#define BYTE3_TO_UINT(bp)						\
	 (((UINT32)(bp)[0] << 16) & 0x00FF0000) |	\
	 (((UINT32)(bp)[1] <<  8) & 0x0000FF00) |	\
	 (( UINT32)(bp)[2]        & 0x000000FF)

#define BYTE2_TO_UINT(bp)						\
	(((UINT32)(bp)[0] << 8) & 0xFF00) |			\
	(( UINT32)(bp)[1]       & 0x00FF)
*/
#define IPS_SIGNATURE  "PATCH"
#define IPS_TAG_EOF    "EOF"
#define IPS_EXT        _T(".ips")

bool bDoIpsPatch = false;

// Type-safe big-endian conversion functions
static inline UINT16 be16_to_uint(const UINT8* bp)
{
	return (UINT16)(((UINT16)bp[0] << 8) | bp[1]);
}

static inline UINT32 be24_to_uint(const UINT8* bp)
{
	return ((UINT32)bp[0] << 16) | ((UINT32)bp[1] << 8) | ((UINT32)bp[2]);
}

static void PatchFile(const char* ips_path, UINT8* base, bool readonly)
{
	FILE* f = fopen(ips_path, "rb");
	// Open IPS file in binary read mode
	if (!f) {
		bprintf(0, _T("IPS - Can't open file %s!  Aborting.\n"), ips_path);
		return;
	}

	char buf[6] = { 0 };
	// Read and validate 5-byte IPS header signature
	size_t bytes_read = fread(buf, 1, 5, f);
	if (5 != bytes_read || 0 != strcmp(buf, IPS_SIGNATURE)) {
		bprintf(0, _T("IPS - Bad IPS-Signature in: %S.\n"), ips_path);
		fclose(f);
		return;
	}

	bprintf(0, _T("IPS - Patching with: %S. (%S)\n"), ips_path, (readonly) ? "Read-Only" : "Write");

	UINT8 ch     = 0;
	INT32 bRLE   = 0;
	INT32 Offset = 0;
	INT32 Size   = 0;
	UINT8* mem8  = NULL;

	// Process all IPS records until EOF marker or end of file
	while (!feof(f)) {
		// Read 3-byte address offset
		bytes_read = fread(buf, 1, 3, f);
		if (3 != bytes_read)
			break;

		buf[3] = 0;

		// Check for end marker "EOF"
		if (0 == strcmp(buf, IPS_TAG_EOF))
			break;

		// Convert 3-byte big-endian to absolute offset
		Offset = (INT32)be24_to_uint((UINT8*)buf);

		// Read 2-byte data length
		bytes_read = fread(buf, 1, 2, f);
		if (2 != bytes_read)
			break;

		Size = (INT32)be16_to_uint((UINT8*)buf);

		// Determine if this record uses RLE compression
		bRLE = (0 == Size);

		// Read RLE header if enabled
		if (bRLE) {
			bytes_read = fread(buf, 1, 2, f);
			if (2 != bytes_read)
				break;

			Size = (INT32)be16_to_uint((UINT8*)buf);
			ch   = (UINT8)fgetc(f);
		}

		// Write or skip data bytes (merged logic)
		while (Size--) {
			// Apply patch byte or skip it
			if (!readonly) {
				mem8  = base + Offset + nRomOffset;
				*mem8 = bRLE ? ch : (UINT8)fgetc(f);
			} else if (!bRLE)
				fgetc(f);				// Read and discard in read-only mode
			Offset++;
		}
	}

	// Update memory expansion length during readonly scan
	if (readonly && 0 == nIpsMemExpLen[EXP_FLAG]) {
		nIpsMemExpLen[LOAD_ROM] = Offset;
	}

	// Clean up file handle
	fclose(f);
}
#undef IPS_EXT
#undef IPS_TAG_EOF
#undef IPS_SIGNATURE

// Optimized case-insensitive substring search
#define _tolower(c)	((c >= 'A' && c <= 'Z') ? (c + 32) : (c))
static char* stristr_int(const char* str1, const char* str2)
{
/*
	const char* p1 = str1;
	const char* p2 = str2;
	const char* r = (!*p2) ? str1 : NULL;

	while (*p1 && *p2) {
		if (tolower((UINT8)*p1) == tolower((UINT8)*p2)) {
			if (!r) {
				r = p1;
			}

			p2++;
		} else {
			p2 = str2;
			if (r) {
				p1 = r + 1;
			}

			if (tolower((UINT8)*p1) == tolower((UINT8)*p2)) {
				r = p1;
				p2++;
			} else {
				r = NULL;
			}
		}

		p1++;
	}

	return (*p2) ? NULL : (char*)r;
*/
	// Early null checks
	if (!str1 || !str2)
		return NULL;

	// Empty substring returns start of main string
	if ('\0' == *str2)
		return (char*)str1;
	
	// Main loop through str1
	for (; '\0' != *str1; str1++) {
		const char* p1 = str1, * p2 = str2;

		// Compare characters case-insensitively
		while (_tolower((UINT8)*p1) == _tolower((UINT8)*p2)) {
			p1++, p2++;

			// If we reached end of substring, we found a match
			if ('\0' == *p2)
				return (char*)str1;

			// If we reached end of main string, no match
			if ('\0' == *p1)
				return NULL;
		}
	}
	return NULL;
}
#undef _tolower

// Converts a hexadecimal string to a 32-bit unsigned integer (UINT32)
// This function uses branchless bitwise operations to convert hex characters (0-9, A-F, a-f)
// into 4-bit nibbles with no conditional logic for maximum speed
// It supports up to 8 hex digits (the maximum for a UINT32 value)
static UINT32 hexto32(const char *s)
{
	// NULL pointer safety check
	if (!s)
		return 0;

	UINT32 val = 0;
	char c;

	while ((c = *s++)) {
		// Ultra-fast branchless hex conversion:
		// Converts any valid hex char (0-9, A-F, a-f) to 4-bit value (0-15)
		// Formula breakdown:
		// (c & 0xf)          -> Get lower 4 bits of ASCII value
		// (c >> 6)           -> Adjust for alphabetic characters (adds 9)
		// ((c >> 3) & 0x8)   -> Correct case offset for A-F / a-f
		if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')))
			return 0;

		UINT8 v = ((c & 0xf) + (c >> 6)) | ((c >> 3) & 0x8);
		val = (val << 4) | (UINT32)v;
	}

	return val;
}

#if 0
// strqtoken() - functionally identicle to strtok() w/ special treatment for
// quoted strings.  -dink april 2023
static char *strqtoken(char *s, const char *delims)
{
	static char *prev_str = NULL;
	char *token = NULL;

	if (!s) s = prev_str;

	s += strspn(s, delims);
	if (s[0] == '\0') {
		prev_str = s;
		return NULL;
	}

	if (s[0] == '\"') { // time to grab quoted string!
		token = ++s;
		if ((s = strpbrk(token, "\""))) {
			*(s++) = '\0';
		}
	} else {
		token = s;
	}

	if ((s = strpbrk(s, delims))) {
		*(s++) = '\0';
		prev_str = s;
	} else {
		// we're at the end of the road
		prev_str = (char*)memchr((void *)token, '\0', MAX_PATH);
	}

	return token;
}
#endif // 0

// Context structure passed to patch callback
struct PatchParams {
	UINT8* base;			// Pointer to loaded ROM buffer
	const char* rom_name;	// ROM display name
	UINT32 crc;				// ROM CRC32 checksum
	bool readonly;			// Read-only mode flag
	bool found;				// Set if matched patch found
};

// Callback function for parsing patch definition file line by line
static void DoPatchGameCallback(const TCHAR* inputFile, FILE* fp, void* userData)
{
	PatchParams* params = (PatchParams*)userData;
	TCHAR lineBuf[MAX_PATH] = { 0 };

	if (!fp || !params)
		return;
	
	TCHAR* tExt = NULL;
	ansi_to_tchar(".ips", &tExt);

	// Read each line of patch definition file
	while (_fgetts(lineBuf, ARRAY_SIZE(lineBuf), fp)) {
		TCHAR* pLine = lineBuf;

		// Stop parsing when reaching [info] section
		if (_T('[') == *pLine)
			break;

		// Parse rom name token, skip comment lines starting with '#'
		TCHAR* rom_name = _strqtoken(pLine, _T(" \t\r\n"));
		if (!rom_name || _T('#') == *rom_name)
			continue;

		// Parse patch file name token
		TCHAR* ips_name = _strqtoken(NULL, _T(" \t\r\n"));
		if (!ips_name)
			continue;

		// Parse ROM memory offset token
		UINT32 romOffset = 0;
		TCHAR* ips_offs  = _strqtoken(NULL, _T(" \t\r\n()"));

		if (ips_offs) {
			if      (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_016"))) romOffset = 0x1000000;
			else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_032"))) romOffset = 0x2000000;
			else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_048"))) romOffset = 0x3000000;
			else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_064"))) romOffset = 0x4000000;
			else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_080"))) romOffset = 0x5000000;
			else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_096"))) romOffset = 0x6000000;
			else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_112"))) romOffset = 0x7000000;
			else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_128"))) romOffset = 0x8000000;
			else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_144"))) romOffset = 0x9000000;

			// Skip offset token for subsequent CRC parsing
			if (0 != romOffset)
				ips_offs = _strqtoken(NULL, _T(" \t\r\n()"));
		}

		// Parse CRC value if keyword "crc" exists (case‑insensitive)
		UINT32 ipsCrc = 0;
		if (ips_offs) {
			char* ansiOffs = NULL;
			if (tchar_to_ansi(ips_offs, &ansiOffs) && ansiOffs) {
				if (stristr_int(ansiOffs, "crc")) {
					TCHAR* ips_crc = _strqtoken(NULL, _T(" \t\r\n()"));
					if (ips_crc) {
						char* ansiCrc = NULL;
						if (tchar_to_ansi(ips_crc, &ansiCrc) > 0 && ansiCrc) {
							ipsCrc = hexto32(ansiCrc);
							free_s((void**)&ansiCrc);
						}
					}
				}
				free_s((void**)&ansiOffs);
			}
		}

		// Match ROM by name or CRC value
		TCHAR* tRomName = NULL;
		ansi_to_tchar(params->rom_name, &tRomName);

		bool nameMatch = (tRomName && 0 == _tcsicmp(rom_name, tRomName));
		bool crcMatch  = (0 != ipsCrc && ipsCrc == params->crc);

		free_s((void**)&tRomName);
		if (!nameMatch && !crcMatch)
			continue;

		// Mark current patch as matched
		params->found = true;

		// In write mode, ROM buffer must be valid
		if (!params->readonly && !params->base)
			continue;

		// Get current driver name for path building
		const TCHAR* pszDrv = (1 == nQuickOpen) ? szDriverName : BurnDrvGetText(DRV_NAME);
		if (IsStrEmpty(pszDrv))
			continue;

		TCHAR primary_path[MAX_PATH]   = { 0 };
		TCHAR final_fallback[MAX_PATH] = { 0 };

		// Build primary patch file path
		if (!IsStrEmpty(ips_name) && IsAbsolutePath(ips_name))
			_tcsncpy(primary_path, ips_name, ARRAY_SIZE(primary_path) - 1);
		else {
			const TCHAR* baseDir = (1 == nQuickOpen) ? szAppQuickPath : szAppIpsPath;
			_sntprintf(primary_path, ARRAY_SIZE(primary_path), _T("%s%s\\%s"), baseDir, pszDrv, ips_name);
		}

		// Build fallback patch file path
		_sntprintf(final_fallback, ARRAY_SIZE(final_fallback), _T("%s%s\\%s"), szAppIpsPath, pszDrv, ips_name);

		// Normalize absolute file path
		TCHAR* szNorm = NULL;
		if (NormalizeAbsolute(primary_path,   &szNorm) > 0 && szNorm) {
			_tcsncpy(primary_path, szNorm,   ARRAY_SIZE(primary_path)   - 1);
			free_s((void**)&szNorm);
		}
		if (NormalizeAbsolute(final_fallback, &szNorm) > 0 && szNorm) {
			_tcsncpy(final_fallback, szNorm, ARRAY_SIZE(final_fallback) - 1);
			free_s((void**)&szNorm);
		}

		// Append .ips extension if missing
		if (tExt && !IsFileExtensionMatch(ips_name, tExt)) {// Safe concatenate with length limit
			_tcsncat(primary_path,   tExt, ARRAY_SIZE(primary_path)   - _tcslen(primary_path)   - 1);
			_tcsncat(final_fallback, tExt, ARRAY_SIZE(final_fallback) - _tcslen(final_fallback) - 1);
		}

		// Select existing valid patch file
		TCHAR* use_path = NULL;
		if (     !IsStrEmpty(primary_path)   && FileExists(primary_path))
			use_path = primary_path;
		else if (!IsStrEmpty(final_fallback) && FileExists(final_fallback))
			use_path = final_fallback;

		// Apply patch to ROM buffer (readonly mode also calls PatchFile)
		if (use_path) {
			char* ansiPath = NULL;
			if (tchar_to_ansi(use_path, &ansiPath) > 0 && ansiPath) {
				PatchFile(ansiPath, params->base, params->readonly);
				free_s((void**)&ansiPath);
			}
		}
	}
	// Free the pre-converted .ips extension
	free_s((void**)&tExt);
}

// Main entry point for ROM patching workflow
static void DoPatchGame(const TCHAR* patch_name, const char* game_name, const UINT32 crc, UINT8* base, bool readonly)
{
	PatchParams params;
	params.base     = base;
	params.rom_name = game_name;
	params.crc      = crc;
	params.readonly = readonly;
	params.found    = false;

	SafeProcessTextFile(patch_name, DoPatchGameCallback, &params);

	if (!params.found && 0 == nIpsMemExpLen[EXP_FLAG])
		nIpsMemExpLen[LOAD_ROM] = 0;
}

static UINT32 GetIpsDefineExpValue(TCHAR* pszTmp)
{
	if (!(pszTmp = _strqtoken(NULL, _T(" \t\r\n"))))
		return 0U;

	INT32 nRet = 0;

	// Predefined EXP_VALUE mapping (fast direct comparisons, constant-first for safety)
	if      (0 == _tcscmp(pszTmp, _T("EXP_VALUE_001"))) nRet = 0x0010000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_002"))) nRet = 0x0020000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_003"))) nRet = 0x0030000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_004"))) nRet = 0x0040000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_005"))) nRet = 0x0050000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_006"))) nRet = 0x0060000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_007"))) nRet = 0x0070000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_008"))) nRet = 0x0080000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_010"))) nRet = 0x0100000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_020"))) nRet = 0x0200000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_030"))) nRet = 0x0300000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_040"))) nRet = 0x0400000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_050"))) nRet = 0x0500000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_060"))) nRet = 0x0600000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_070"))) nRet = 0x0700000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_080"))) nRet = 0x0800000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_100"))) nRet = 0x1000000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_200"))) nRet = 0x2000000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_300"))) nRet = 0x3000000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_400"))) nRet = 0x4000000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_500"))) nRet = 0x5000000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_600"))) nRet = 0x6000000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_700"))) nRet = 0x7000000;
	else if (0 == _tcscmp(pszTmp, _T("EXP_VALUE_800"))) nRet = 0x8000000;
	// If not a predefined constant, parse input as hexadecimal value
	else if (EOF != (_stscanf(pszTmp, _T("%x"), &nRet))) return nRet;

	// Return mapped or parsed value
	return nRet;
}

// Extract szAppQuickPath directory (without driver name) and validate driver existence
INT32 IpsGetDrvForQuickOpen(const TCHAR* pszSelDat)
{
	// Input safety check
	if (!pszSelDat || 1 != nQuickOpen)
		return -1;

	const INT32 len = (INT32)_tcslen(pszSelDat);
	INT32 lastSlash = -1;

	// Step 1: Find LAST slash (before filename)
	for (INT32 i = len - 1; i >= 0; --i) {
		if (_T('\\') == pszSelDat[i] || _T('/') == pszSelDat[i]) {
			lastSlash = i;
			break;
		}
	}
	if (lastSlash <= 0)
		return -1;

	INT32 baseSlash = -1;
	// Step 2: Find SECOND LAST slash (base path before driver folder)
	for (INT32 i = lastSlash - 1; i >= 0; --i) {
		if (_T('\\') == pszSelDat[i] || _T('/') == pszSelDat[i]) {
			baseSlash = i;
			break;
		}
	}
	if (baseSlash < 0)
		return -1;

	// Step 3: Extract driver name between two slashes
	INT32 drvLen = lastSlash - (baseSlash + 1);
	if (drvLen <= 0 || drvLen >= ARRAY_SIZE(szDriverName))
		return -1;

	_tcsncpy(szDriverName, pszSelDat + baseSlash + 1, drvLen);
	szDriverName[drvLen] = _T('\0');

	// Step 4: Validate driver via BurnDrvGetIndex
	char* ansiDrvName = NULL;
	if (tchar_to_ansi(szDriverName, &ansiDrvName) <= 0)
		return -1;

	const INT32 nDrvIdx = BurnDrvGetIndex(ansiDrvName);
	free_s((void**)&ansiDrvName);
	if (nDrvIdx < 0)
		return -1;

	// Step 5: Save base path (WITHOUT driver, WITH trailing slash)
	_tcsncpy(szAppQuickPath, pszSelDat, baseSlash + 1);
	szAppQuickPath[baseSlash + 1] = _T('\0');
	
	// Step 5: Clear szIpsActivePatches AND store pszSelDat to [0]
	memset(szIpsActivePatches, 0, sizeof(szIpsActivePatches));
	_tcsncpy(szIpsActivePatches[0], pszSelDat, MAX_PATH - 1);
	szIpsActivePatches[0][MAX_PATH - 1] = _T('\0');

	return nDrvIdx;
}

// Update ROM expansion size based on IPS #define value
static void UpdateRomExpansionSize(const TCHAR* t, UINT32 f, INT32 s)
{
	// Validate index is within valid range (SAFE & CORRECT)
	if (IsStrEmpty(t) || s < 0 || s >= ARRAY_SIZE(nIpsMemExpLen))
		return;

	nIpsDrvDefine |= f;
	UINT32 v = GetIpsDefineExpValue((TCHAR*)t);
	if (v > nIpsMemExpLen[s])
		nIpsMemExpLen[s] = v;
}

// Parse #include directive, resolve full path, then recursively trigger callback via SafeProcessTextFile
static void ProcessIncludeDirective(TCHAR* tkInclude, void (*ProcessCallback)(const TCHAR*, FILE*, void*), void* userData)
{
	// Input validation
	if (IsStrEmpty(tkInclude) || !ProcessCallback || !userData)
		return;

	TCHAR* szNormalizedPath = NULL;
	TCHAR  szTargetPath[MAX_PATH] = { 0 };
	bool   bValidPath = false;

	// Absolute path (contains \ or /): use LOCAL driver name (SAFE for recursion!)
	if (_tcschr(tkInclude, _T('\\')) || _tcschr(tkInclude, _T('/'))) {
		if (NormalizeAbsolute(tkInclude, &szNormalizedPath) > 0 && szNormalizedPath) {
			TCHAR* pFileSep = _tcsrchr(szNormalizedPath, _T('\\'));
			if (pFileSep && pFileSep > szNormalizedPath) {
				*pFileSep = _T('\0');
				TCHAR* pDrvSep = _tcsrchr(szNormalizedPath, _T('\\'));

				if (pDrvSep) {
					// Use LOCAL variable (safe for recursion)
					TCHAR szDrvLocal[100] = { 0 };
					_tcsncpy(szDrvLocal, pDrvSep + 1, ARRAY_SIZE(szDrvLocal) - 1);
					szDrvLocal[ARRAY_SIZE(szDrvLocal) - 1] = _T('\0');

					// Validate with LOCAL driver name
					char* ansiDrvName = NULL;
					if (tchar_to_ansi(szDrvLocal, &ansiDrvName) > 0) {
						INT32 nDrvIdx = BurnDrvGetIndex(ansiDrvName);
						free_s((void**)&ansiDrvName);

						if (nDrvIdx >= 0) {
							*pFileSep = _T('\\');
							_tcsncpy(szTargetPath, szNormalizedPath, MAX_PATH - 1);
							szTargetPath[MAX_PATH - 1] = _T('\0');
							bValidPath = true;
						}
					}
				}
				*pFileSep = _T('\\');
			}
		}
		free_s((void**)&szNormalizedPath);
	}
	// Relative path (filename only): use GLOBAL szDriverName
	else {
		const TCHAR* pszDrv = (1 == nQuickOpen) ? szDriverName : BurnDrvGetText(DRV_NAME);
		if (IsStrEmpty(pszDrv))
			return;

		const BOOL hasDat = IsDatFile(tkInclude);

		// First priority: QuickOpen path
		if (1 == nQuickOpen) {
			_sntprintf(szTargetPath, MAX_PATH, hasDat ? _T("%s%s\\%s") : _T("%s%s\\%s.dat"), szAppQuickPath, pszDrv, tkInclude);
			szTargetPath[MAX_PATH - 1] = _T('\0');
			if (FileExists(szTargetPath)) {
				bValidPath = true;
				goto process_include;
			}
		}

		// Fallback: default IPS path
		_sntprintf(szTargetPath, MAX_PATH, hasDat ? _T("%s%s\\%s") : _T("%s%s\\%s.dat"), szAppIpsPath, pszDrv, tkInclude);
		szTargetPath[MAX_PATH - 1] = _T('\0');
		bValidPath = true;
	}

process_include:
	// Process valid path
	if (bValidPath && !IsStrEmpty(szTargetPath))
		SafeProcessTextFile(szTargetPath, ProcessCallback, userData);
}

#if defined(__cplusplus) && __cplusplus >= 201103L
#define THREAD_LOCAL thread_local
#elif defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__)
#define THREAD_LOCAL __thread
#else
#define THREAD_LOCAL _Thread_local
#endif
// Callback to parse #define and #include directives in IPS .dat files
#define IPS_MAX_RECURSION_DEPTH		100
static void ParseIpsDatParameters(const TCHAR* currentFile, FILE* fp, void* userData)
{
	// Thread-local tracking: Prevent circular includes & duplicate parsing
	THREAD_LOCAL static TCHAR loadedFiles[100][MAX_PATH];
	THREAD_LOCAL static INT32 loadedCount = 0;

	if (!fp || !userData )                        return;

	INT32* pDepth = (INT32*)userData;
	// Recursion depth limit
	if (*pDepth >= IPS_MAX_RECURSION_DEPTH)       return;

	// Skip duplicate / circular includes
	for (int i = 0; i < loadedCount; i++) {
		const size_t bufSize = ARRAY_SIZE(loadedFiles[i]);
		if (0 == _tcsncmp(loadedFiles[i], currentFile, bufSize - 1))
			return;
	}

	// Safely add to loaded list
	if (loadedCount < ARRAY_SIZE(loadedFiles)) {
		const size_t bufSize = ARRAY_SIZE(loadedFiles[loadedCount]);
		_tcsncpy(loadedFiles[loadedCount], currentFile, bufSize - 1);
		loadedFiles[loadedCount][bufSize - 1] = _T('\0');
		loadedCount++;
	}

	TCHAR str[MAX_PATH];
	while (_fgetts(str, ARRAY_SIZE(str), fp)) {
		TCHAR* tk = _strqtoken(str, _T(" \t\r\n"));
		if (IsStrEmpty(tk))                       continue;
		if (_T('[') == *tk)                       break;
		if (_T('/') == tk[0] && _T('/') == tk[1]) continue;

		// Process #define
		if (0 == _tcscmp(tk, _T("#define"))) {
			tk = _strqtoken(NULL, _T(" \t\r\n"));
			if (IsStrEmpty(tk))                   continue;


			if (0 == _tcscmp(tk, _T("IPS_NOT_PROTECT"))) { nIpsDrvDefine |= IPS_NOT_PROTECT; continue; }
			if (0 == _tcscmp(tk, _T("IPS_PGM_SPRHACK"))) { nIpsDrvDefine |= IPS_PGM_SPRHACK; continue; }
			if (0 == _tcscmp(tk, _T("IPS_PGM_MAPHACK"))) { nIpsDrvDefine |= IPS_PGM_MAPHACK; continue; }
			if (0 == _tcscmp(tk, _T("IPS_PGM_SNDOFFS"))) { nIpsDrvDefine |= IPS_PGM_SNDOFFS; continue; }
			if (0 == _tcscmp(tk, _T("IPS_SNES_VRAMHK"))) { nIpsDrvDefine |= IPS_SNES_VRAMHK; continue; }
			if (0 == _tcscmp(tk, _T("IPS_NEO_RAMHACK"))) { nIpsDrvDefine |= IPS_NEO_RAMHACK; continue; }

			const TCHAR* valTok = _strqtoken(NULL, _T(" \t\r\n"));
			if (IsStrEmpty(valTok))               continue;

			if (0 == _tcscmp(tk, _T("IPS_LOAD_EXPAND"))) { nIpsDrvDefine |= IPS_LOAD_EXPAND;
				nIpsMemExpLen[EXP_FLAG] = 1;               UpdateRomExpansionSize(valTok, IPS_LOAD_EXPAND, LOAD_ROM); continue; }
			if (0 == _tcscmp(tk, _T("IPS_EXTROM_INCL"))) { UpdateRomExpansionSize(valTok, IPS_EXTROM_INCL, EXTR_ROM); continue; }
			if (0 == _tcscmp(tk, _T("IPS_PRG1_EXPAND"))) { UpdateRomExpansionSize(valTok, IPS_PRG1_EXPAND, PRG1_ROM); continue; }
			if (0 == _tcscmp(tk, _T("IPS_PRG2_EXPAND"))) { UpdateRomExpansionSize(valTok, IPS_PRG2_EXPAND, PRG2_ROM); continue; }
			if (0 == _tcscmp(tk, _T("IPS_GRA1_EXPAND"))) { UpdateRomExpansionSize(valTok, IPS_GRA1_EXPAND, GRA1_ROM); continue; }
			if (0 == _tcscmp(tk, _T("IPS_GRA2_EXPAND"))) { UpdateRomExpansionSize(valTok, IPS_GRA2_EXPAND, GRA2_ROM); continue; }
			if (0 == _tcscmp(tk, _T("IPS_GRA3_EXPAND"))) { UpdateRomExpansionSize(valTok, IPS_GRA3_EXPAND, GRA3_ROM); continue; }
			if (0 == _tcscmp(tk, _T("IPS_ACPU_EXPAND"))) { UpdateRomExpansionSize(valTok, IPS_ACPU_EXPAND, ACPU_ROM); continue; }
			if (0 == _tcscmp(tk, _T("IPS_SND1_EXPAND"))) { UpdateRomExpansionSize(valTok, IPS_SND1_EXPAND, SND1_ROM); continue; }
			if (0 == _tcscmp(tk, _T("IPS_SND2_EXPAND"))) { UpdateRomExpansionSize(valTok, IPS_SND2_EXPAND, SND2_ROM); continue; }

			continue;
		}
		// Process #include
		else if (0 == _tcscmp(tk, _T("#include"))) {
			tk = _strqtoken(NULL, _T(" \t\r\n"));
			INT32 newDepth = *pDepth + 1;
			ProcessIncludeDirective(tk, ParseIpsDatParameters, &newDepth);
		}
	}
}

// Main entry to parse all IPS configuration defines from active DAT files
// Only parses #define and #include, does NOT apply patches
static void GetIpsDrvDefine()
{
	// Reset global flags and expansion sizes
	nIpsDrvDefine = 0;
	memset(nIpsMemExpLen, 0, sizeof(nIpsMemExpLen));

	// Get number of active patch files
	INT32 n = GetIpsNumActivePatches();
	if (n <= 0)
		return;

	// Parse all active DAT files with recursion depth limit
	INT32 depth = 0;
	for (INT32 i = 0; i < n; i++)
		SafeProcessTextFile(szIpsActivePatches[i], ParseIpsDatParameters, &depth);
}

// Callback for applying IPS patches recursively
// Does NOT parse #define directives, only processes #include with depth-first order
static void PatchDatCallback(const TCHAR* currentFile, FILE* fp, void* userData)
{
	// Thread-local state: Independent per thread, safe for reentrancy
	THREAD_LOCAL static TCHAR loadedFiles[100][MAX_PATH];
	THREAD_LOCAL static INT32 loadedCount    = 0;
	THREAD_LOCAL static INT32 recursionDepth = 0;

	// Basic validation
	if (IsStrEmpty(currentFile) || !fp || !userData)
		return;

	PatchParams* params = (PatchParams*)userData;

	// Prevent stack overflow via excessive recursion
	if (recursionDepth >= IPS_MAX_RECURSION_DEPTH)
		return;

	// Prevent circular inclusion: Skip if file already processed
	for (INT32 i = 0; i < loadedCount; ++i) {
		if (0 == _tcsncmp(loadedFiles[i], currentFile, ARRAY_SIZE(loadedFiles[i]) - 1))
			return;
	}

	// Safely store the loaded file path (no buffer overflow)
	if (loadedCount < ARRAY_SIZE(loadedFiles)) {
		const size_t bufSize = ARRAY_SIZE(loadedFiles[loadedCount]);
		_tcsncpy(loadedFiles[loadedCount], currentFile, bufSize - 1);
		loadedFiles[loadedCount][bufSize - 1] = _T('\0');
		loadedCount++;
	}

	recursionDepth++;

	// Process file lines
	TCHAR str[MAX_PATH];
	while (_fgetts(str, ARRAY_SIZE(str), fp)) {
		TCHAR* tk = _strqtoken(str, _T(" \t\r\n"));
		if (IsStrEmpty(tk))                       continue;
		if (*tk == _T('['))                       break;
		if (tk[0] == _T('/') && tk[1] == _T('/')) continue;

		if (_tcscmp(tk, _T("#include")) == 0) {
			tk = _strqtoken(NULL, _T(" \t\r\n"));
			ProcessIncludeDirective(tk, PatchDatCallback, userData);
		}
	}

	DoPatchGame(currentFile, params->rom_name, params->crc, params->base, params->readonly);

	// Reset top-level state
	recursionDepth--;
	if (recursionDepth <= 0)
		loadedCount = 0;
}
#undef IPS_MAX_RECURSION_DEPTH
#undef THREAD_LOCAL

// Main entry for applying IPS patches
void IpsApplyPatches(UINT8* base, char* rom_name, UINT32 crc, bool readonly)
{
	// Top-level input validation
	if (!bDoIpsPatch || IsStrEmptyA(rom_name))
		return;

	// Critical safety check:
	// When readonly = false (real writing), base CANNOT be NULL
	if (!readonly && !base)
		return;

	INT32 nCount = GetIpsNumActivePatches();
	// Process each active patch DAT file in sequence
	for (INT32 i = 0; i < nCount; i++) {
		const TCHAR* datPath = szIpsActivePatches[i];
		if (IsStrEmpty(datPath))
			continue;

		PatchParams params;
		params.base     = base;
		params.rom_name = rom_name;
		params.crc      = crc;
		params.readonly = readonly;

		SafeProcessTextFile(datPath, PatchDatCallback, &params);
	}
}

void IpsPatchInit()
{
	GetIpsDrvDefine();
}

void IpsPatchExit()
{
	memset(nIpsMemExpLen, 0, sizeof(nIpsMemExpLen));
	nIpsDrvDefine = 0;
	bDoIpsPatch   = false;
}
