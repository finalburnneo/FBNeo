// FB Neo IPS Mangler^H^H^H^H^H^H^HManager
#include "burner.h"

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

static INT32 nPatchIndex	= 0;
static INT32 nNumPatches	= 0;
static HTREEITEM hPatchHandlesIndex[MAX_NODES];
static TCHAR szPatchFileNames[MAX_NODES][MAX_PATH];

static HBRUSH hWhiteBGBrush;
static HBITMAP hBmp			= NULL;
static HBITMAP hPreview		= NULL;

static TCHAR szDriverName[64];

static INT32 nRomOffset		= 0;
UINT32 nIpsDrvDefine		= 0, nIpsMemExpLen[SND2_ROM + 1] = { 0 };

static TCHAR szIpsActivePatches[MAX_ACTIVE_PATCHES][MAX_PATH];

struct IpsExtraPatch {
	TCHAR** pszDat;
	UINT32 nCount;
};

// When including #include *.dat, inserts a timely patch depending on where it is before or after the patch
static IpsExtraPatch _IpsEarly = { 0 }, _IpsLast = { 0 };

// GCC doesn't seem to define these correctly.....
#define _TreeView_SetItemState(hwndTV, hti, data, _mask)			\
{																	\
	TVITEM _ms_TVi;													\
	_ms_TVi.mask      = TVIF_STATE;									\
	_ms_TVi.hItem     = hti;										\
	_ms_TVi.stateMask = _mask;										\
	_ms_TVi.state     = data;										\
	SNDMSG((hwndTV), TVM_SETITEM, 0, (LPARAM)(TV_ITEM *)&_ms_TVi);	\
}

#define _TreeView_SetCheckState(hwndTV, hti, fCheck) \
  _TreeView_SetItemState(hwndTV, hti, INDEXTOSTATEIMAGEMASK((fCheck)?2:1), TVIS_STATEIMAGEMASK)

#define _TreeView_GetCheckState(hwndTV, hti) \
   ((((UINT)(SNDMSG((hwndTV), TVM_GETITEMSTATE, (WPARAM)(hti), TVIS_STATEIMAGEMASK))) >> 12) -1)

static INT32 FileExists(const TCHAR* pszName)
{
	DWORD dwAttrib = GetFileAttributes(pszName);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
		!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static TCHAR* GameIpsConfigName()
{
	// Return the path of the config file for this game
	static TCHAR szName[100];
	_stprintf(szName, _T("config\\ips\\%s.ini"), szDriverName);
	return szName;
}

INT32 GetIpsNumPatches()
{
	WIN32_FIND_DATA wfd;
	HANDLE hSearch;
	TCHAR szFilePath[MAX_PATH];
	INT32 Count = 0;

	_stprintf(szFilePath, _T("%s%s\\*.dat"), szAppIpsPath, BurnDrvGetText(DRV_NAME));

	hSearch = FindFirstFile(szFilePath, &wfd);

	if (hSearch != INVALID_HANDLE_VALUE) {
		INT32 Done = 0;

		while (!Done ) {
			Count++;
			Done = !FindNextFile(hSearch, &wfd);
		}

		FindClose(hSearch);
	}

	return Count;
}

static TCHAR* GetPatchDescByLangcode(FILE* fp, INT32 nLang)
{
	static TCHAR* result = NULL;
	TCHAR* desc = NULL, langtag[10] = { 0 };

	_stprintf(langtag, _T("[%s]"), szLanguageCodes[nLang]);

	fseek(fp, 0, SEEK_SET);

	while (!feof(fp)) {
		TCHAR s[4096];

		if (_fgetts(s, sizeof(s), fp) != NULL) {
			if (_tcsncmp(langtag, s, _tcslen(langtag)) != 0)
				continue;

			while (_fgetts(s, sizeof(s), fp) != NULL) {
				TCHAR* p;

				if (*s == _T('[')) {
					if (desc) {
						result = (TCHAR*)malloc((_tcslen(desc) + 1) * sizeof(TCHAR));
						_tcscpy(result, desc);
						free(desc); desc = NULL;
						return result;
					} else
						return NULL;
				}

				for (p = s; *p; p++) {
					if (*p == _T('\r') || *p == _T('\n')) {
						*p = _T('\0');
						break;
					}
				}

				if (desc) {
					TCHAR* p1 = NULL;
					INT32 len = _tcslen(desc);

					len += _tcslen(s) + 2;
					p1 = (TCHAR*)malloc((len + 1) * sizeof(TCHAR));
					_stprintf(p1, _T("%s\r\n%s"), desc, s);
					free(desc);
					desc = p1;
				} else {
					desc = (TCHAR*)malloc((_tcslen(s) + 1) * sizeof(TCHAR));
					if (desc != NULL)
						_tcscpy(desc, s);
				}
			}
		}
	}

	if (desc) {
		result = (TCHAR*)malloc((_tcslen(desc) + 1) * sizeof(TCHAR));
		_tcscpy(result, desc);
		free(desc); desc = NULL;
		return result;
	} else
		return NULL;
}

static void FillListBox()
{
	WIN32_FIND_DATA wfd;
	HANDLE hSearch;
	TCHAR szFilePath[MAX_PATH]       = { 0 };
	TCHAR szFilePathSearch[MAX_PATH] = { 0 };
	TCHAR szFileName[MAX_PATH]       = { 0 };
	TCHAR PatchName[256]             = { 0 };
	TCHAR* PatchDesc = NULL;
	INT32 nHandlePos = 0;

	TV_INSERTSTRUCT TvItem;

	memset(&TvItem, 0, sizeof(TvItem));
	TvItem.item.mask    = TVIF_TEXT | TVIF_PARAM;
	TvItem.hInsertAfter = TVI_LAST;

	_stprintf(szFilePath, _T("%s%s\\"), szAppIpsPath, szDriverName);
	_stprintf(szFilePathSearch, _T("%s*.dat"), szFilePath);

	hSearch = FindFirstFile(szFilePathSearch, &wfd);

	if (hSearch != INVALID_HANDLE_VALUE) {
		INT32 Done = 0;

		while (!Done ) {
			memset(szFileName, '\0', MAX_PATH * sizeof(TCHAR));
			_stprintf(szFileName, _T("%s%s"), szFilePath, wfd.cFileName);

			const TCHAR* pszReadMode = AdaptiveEncodingReads(szFileName);
			if (NULL == pszReadMode) continue;

			FILE *fp = _tfopen(szFileName, pszReadMode);
			if (fp) {
				PatchDesc = NULL;
				memset(PatchName, '\0', 256 * sizeof(TCHAR));

				PatchDesc = GetPatchDescByLangcode(fp, nIpsSelectedLanguage);
				// If not available - try English first
				if (PatchDesc == NULL) PatchDesc = GetPatchDescByLangcode(fp, 0);
				// Simplified Chinese is the reference language (should always be available!!)
				if (PatchDesc == NULL) PatchDesc = GetPatchDescByLangcode(fp, 1);
				if (PatchDesc == NULL) {
					PatchDesc = (TCHAR*)malloc(MAX_PATH * sizeof(TCHAR));
					if (NULL == PatchDesc) {
						fclose(fp); fp = NULL;
						continue;
					}
					memset(PatchDesc, 0, MAX_PATH * sizeof(TCHAR));
					_tcscpy(PatchDesc, wfd.cFileName);
				}

				bprintf(0, _T("PatchDesc [%s]\n"), PatchDesc);

				for (UINT32 i = 0; i < _tcslen(PatchDesc); i++) {
					if (PatchDesc[i] == _T('\r') || PatchDesc[i] == _T('\n')) break;
					PatchName[i] = PatchDesc[i];
				}
				if (NULL != PatchDesc) {
					free(PatchDesc); PatchDesc = NULL;
				}

				// Check for categories
				TCHAR *Tokens;
				INT32 nNumTokens = 0, nNumNodes = 0;
				TCHAR szCategory[256];
				UINT32 nPatchNameLength = _tcslen(PatchName);

				Tokens = _tcstok(PatchName, _T("/"));
				while (Tokens != NULL) {
					if (nNumTokens == 0) {
						INT32 bAddItem = 1;
						// Check if item already exists
						nNumNodes = SendMessage(hIpsList, TVM_GETCOUNT, (WPARAM)0, (LPARAM)0);
						for (INT32 i = 0; i < nNumNodes; i++) {
							TCHAR Temp[256];
							TVITEM Tvi;
							memset(&Tvi, 0, sizeof(Tvi));
							Tvi.hItem      = hItemHandles[i];
							Tvi.mask       = TVIF_TEXT | TVIF_HANDLE;
							Tvi.pszText    = Temp;
							Tvi.cchTextMax = 256;
							SendMessage(hIpsList, TVM_GETITEM, (WPARAM)0, (LPARAM)&Tvi);

							if (!_tcsicmp(Tvi.pszText, Tokens)) bAddItem = 0;
						}
						if (bAddItem) {
							TvItem.hParent           = TVI_ROOT;
							TvItem.item.pszText      = Tokens;
							hItemHandles[nHandlePos] = (HTREEITEM)SendMessage(hIpsList, TVM_INSERTITEM, 0, (LPARAM)&TvItem);
							nHandlePos++;
						}
						if (_tcslen(Tokens) == nPatchNameLength) {
							hPatchHandlesIndex[nPatchIndex] = hItemHandles[nHandlePos - 1];
							_tcscpy(szPatchFileNames[nPatchIndex], szFileName);

							nPatchIndex++;
						}
						_tcscpy(szCategory, Tokens);
					} else {
						HTREEITEM hNode = TVI_ROOT;
						// See which category we should be in
						nNumNodes = SendMessage(hIpsList, TVM_GETCOUNT, (WPARAM)0, (LPARAM)0);
						for (INT32 i = 0; i < nNumNodes; i++) {
							TCHAR Temp[256];
							TVITEM Tvi;
							memset(&Tvi, 0, sizeof(Tvi));
							Tvi.hItem      = hItemHandles[i];
							Tvi.mask       = TVIF_TEXT | TVIF_HANDLE;
							Tvi.pszText    = Temp;
							Tvi.cchTextMax = 256;
							SendMessage(hIpsList, TVM_GETITEM, (WPARAM)0, (LPARAM)&Tvi);

							if (!_tcsicmp(Tvi.pszText, szCategory)) hNode = Tvi.hItem;
						}

						TvItem.hParent           = hNode;
						TvItem.item.pszText      = Tokens;
						hItemHandles[nHandlePos] = (HTREEITEM)SendMessage(hIpsList, TVM_INSERTITEM, 0, (LPARAM)&TvItem);

						hPatchHandlesIndex[nPatchIndex] = hItemHandles[nHandlePos];
						_tcscpy(szPatchFileNames[nPatchIndex], szFileName);

						nHandlePos++;
						nPatchIndex++;
					}

					// Only one file path can be bound to a DAT file.
					// A maximum of root and secondary nodes are required.
					// The use of '/' here will potentially create useless multi-level nodes.
					Tokens = _tcstok(NULL, _T("\0"));
					nNumTokens++;
				}

				fclose(fp);
			}

			Done = !FindNextFile(hSearch, &wfd);
		}

		FindClose(hSearch);
	}

	nNumPatches = nPatchIndex;

	// Expand all branches
	INT32 nNumNodes = SendMessage(hIpsList, TVM_GETCOUNT, (WPARAM)0, (LPARAM)0);;
	for (INT32 i = 0; i < nNumNodes; i++) {
		SendMessage(hIpsList, TVM_EXPAND, TVE_EXPAND, (LPARAM)hItemHandles[i]);
	}
}

static INT32 GetIpsNumActivePatches()
{
	INT32 nActivePatches = 0;

	for (INT32 i = 0; i < MAX_ACTIVE_PATCHES; i++) {
		if (_tcsicmp(szIpsActivePatches[i], _T(""))) nActivePatches++;
	}

	return nActivePatches;
}

INT32 LoadIpsActivePatches()
{
	_tcscpy(szDriverName, BurnDrvGetText(DRV_NAME));

	for (INT32 i = 0; i < MAX_ACTIVE_PATCHES; i++) {
		_stprintf(szIpsActivePatches[i], _T(""));
	}

	FILE* fp = _tfopen(GameIpsConfigName(), _T("rt"));
	TCHAR szLine[MAX_PATH];
	INT32 nActivePatches = 0;

	if (fp) {
		while (_fgetts(szLine, MAX_PATH, fp)) {
			INT32 nLen = _tcslen(szLine);

			// Get rid of the linefeed at the end
			if (nLen > 0 && szLine[nLen - 1] == 10) {
				szLine[nLen - 1] = 0;
				nLen--;
			}

			if (!_tcsnicmp(szLine, _T("//"), 2)) continue;
			if (!_tcsicmp(szLine, _T("")))       continue;

			_stprintf(szIpsActivePatches[nActivePatches], _T("%s%s\\%s"), szAppIpsPath, szDriverName, szLine);
			nActivePatches++;
		}
		fclose(fp);
	}

	return nActivePatches;
}

static void CheckActivePatches()
{
	INT32 nActivePatches = LoadIpsActivePatches();

	for (INT32 i = 0; i < nActivePatches; i++) {
		for (INT32 j = 0; j < nNumPatches; j++) {
			if (!_tcsicmp(szIpsActivePatches[i], szPatchFileNames[j])) {
				_TreeView_SetCheckState(hIpsList, hPatchHandlesIndex[j], TRUE);
			}
		}
	}
}

static INT32 IpsManagerInit()
{
	// Get the games full name
	TCHAR szText[1024] = _T("");
	TCHAR* pszPosition = szText;
	TCHAR* pszName     = BurnDrvGetText(DRV_FULLNAME);

	pszPosition += _sntprintf(szText, 1024, pszName);

	pszName = BurnDrvGetText(DRV_FULLNAME);
	while ((pszName = BurnDrvGetText(DRV_NEXTNAME | DRV_FULLNAME)) != NULL) {
		if (pszPosition + _tcslen(pszName) - 1024 > szText) {
			break;
		}
		pszPosition += _stprintf(pszPosition, _T(SEPERATOR_2) _T("%s"), pszName);
	}

	_tcscpy(szFullName, szText);

	_stprintf(szText, _T("%s") _T(SEPERATOR_1) _T("%s"), FBALoadStringEx(hAppInst, IDS_IPSMANAGER_TITLE, true), szFullName);

	// Set the window caption
	SetWindowText(hIpsDlg, szText);

	// Fill the combo box
	_stprintf(szLanguages[ 0], FBALoadStringEx(hAppInst, IDS_LANG_ENGLISH_US,   true));
	_stprintf(szLanguages[ 1], FBALoadStringEx(hAppInst, IDS_LANG_SIMP_CHINESE, true));
	_stprintf(szLanguages[ 2], FBALoadStringEx(hAppInst, IDS_LANG_TRAD_CHINESE, true));
	_stprintf(szLanguages[ 3], FBALoadStringEx(hAppInst, IDS_LANG_JAPANESE,     true));
	_stprintf(szLanguages[ 4], FBALoadStringEx(hAppInst, IDS_LANG_KOREAN,       true));
	_stprintf(szLanguages[ 5], FBALoadStringEx(hAppInst, IDS_LANG_FRENCH,       true));
	_stprintf(szLanguages[ 6], FBALoadStringEx(hAppInst, IDS_LANG_SPANISH,      true));
	_stprintf(szLanguages[ 7], FBALoadStringEx(hAppInst, IDS_LANG_ITALIAN,      true));
	_stprintf(szLanguages[ 8], FBALoadStringEx(hAppInst, IDS_LANG_GERMAN,       true));
	_stprintf(szLanguages[ 9], FBALoadStringEx(hAppInst, IDS_LANG_PORTUGUESE,   true));
	_stprintf(szLanguages[10], FBALoadStringEx(hAppInst, IDS_LANG_POLISH,       true));
	_stprintf(szLanguages[11], FBALoadStringEx(hAppInst, IDS_LANG_HUNGARIAN,    true));

	_stprintf(szLanguageCodes[ 0], _T("en_US"));
	_stprintf(szLanguageCodes[ 1], _T("zh_CN"));
	_stprintf(szLanguageCodes[ 2], _T("zh_TW"));
	_stprintf(szLanguageCodes[ 3], _T("ja_JP"));
	_stprintf(szLanguageCodes[ 4], _T("ko_KR"));
	_stprintf(szLanguageCodes[ 5], _T("fr_FR"));
	_stprintf(szLanguageCodes[ 6], _T("es_ES"));
	_stprintf(szLanguageCodes[ 7], _T("it_IT"));
	_stprintf(szLanguageCodes[ 8], _T("de_DE"));
	_stprintf(szLanguageCodes[ 9], _T("pt_BR"));
	_stprintf(szLanguageCodes[10], _T("pl_PL"));
	_stprintf(szLanguageCodes[11], _T("hu_HU"));

	for (INT32 i = 0; i < NUM_LANGUAGES; i++) {
		SendDlgItemMessage(hIpsDlg, IDC_CHOOSE_LIST, CB_ADDSTRING, 0, (LPARAM)&szLanguages[i]);
	}

	SendDlgItemMessage(hIpsDlg, IDC_CHOOSE_LIST, CB_SETCURSEL, (WPARAM)nIpsSelectedLanguage, (LPARAM)0);

	hIpsList = GetDlgItem(hIpsDlg, IDC_TREE1);

	_tcscpy(szDriverName, BurnDrvGetText(DRV_NAME));

	FillListBox();
	CheckActivePatches();

	return 0;
}

static void RefreshPatch()
{
	szPngName[0] = _T('\0');  // Reset the file name of the preview picture
	SendMessage(GetDlgItem(hIpsDlg, IDC_TEXTCOMMENT), WM_SETTEXT, (WPARAM)0, (LPARAM)NULL);
	SendDlgItemMessage(hIpsDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hPreview);

	HTREEITEM hSelectHandle = (HTREEITEM)SendMessage(hIpsList, TVM_GETNEXTITEM, TVGN_CARET, ~0U);

	if (hBmp) {
		DeleteObject((HGDIOBJ)hBmp);
		hBmp = NULL;
	}

	for (INT32 i = 0; i < nNumPatches; i++) {
		if (hSelectHandle == hPatchHandlesIndex[i]) {
			TCHAR *PatchDesc = NULL;

			const TCHAR* pszReadMode = AdaptiveEncodingReads(szPatchFileNames[i]);
			if (NULL == pszReadMode) continue;

			FILE *fp = _tfopen(szPatchFileNames[i], pszReadMode);
			if (fp) {
				PatchDesc = GetPatchDescByLangcode(fp, nIpsSelectedLanguage);
				// If not available - try English first
				if (PatchDesc == NULL) PatchDesc = GetPatchDescByLangcode(fp, 0);
				// Simplified Chinese is the reference language (should always be available!!)
				if (PatchDesc == NULL) PatchDesc = GetPatchDescByLangcode(fp, 1);
				if (PatchDesc == NULL) {
					PatchDesc = (TCHAR*)malloc(MAX_PATH * sizeof(TCHAR));
					if (NULL == PatchDesc) {
						fclose(fp); fp = NULL;
						continue;
					}
					memset(PatchDesc, 0, MAX_PATH * sizeof(TCHAR));
					_tcscpy(PatchDesc, szPatchFileNames[i]);
				}

				SendMessage(GetDlgItem(hIpsDlg, IDC_TEXTCOMMENT), WM_SETTEXT, (WPARAM)0, (LPARAM)PatchDesc);

				if (NULL != PatchDesc) {
					free(PatchDesc); PatchDesc = NULL;
				}
				fclose(fp);
			}
			fp = NULL;

			TCHAR szImageFileName[MAX_PATH];
			szImageFileName[0] = _T('\0');

			_tcscpy(szImageFileName, szPatchFileNames[i]);
			szImageFileName[_tcslen(szImageFileName) - 3] = _T('p');
			szImageFileName[_tcslen(szImageFileName) - 2] = _T('n');
			szImageFileName[_tcslen(szImageFileName) - 1] = _T('g');

			fp = _tfopen(szImageFileName, _T("rb"));
			HBITMAP hNewImage = NULL;
			if (fp) {
				_tcscpy(szPngName, szImageFileName);  // Associated preview picture
				hNewImage = PNGLoadBitmap(hIpsDlg, fp, 304, 228, 3);
				fclose(fp);
			}
			if (hNewImage) {
				DeleteObject((HGDIOBJ)hBmp);
				hBmp = hNewImage;
				SendDlgItemMessage(hIpsDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBmp);
			} else {
				SendDlgItemMessage(hIpsDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hPreview);
			}
		}
	}
}

static void SavePatches()
{
	INT32 nActivePatches = 0;

	for (INT32 i = 0; i < MAX_ACTIVE_PATCHES; i++) {
		_stprintf(szIpsActivePatches[i], _T(""));
	}

	for (INT32 i = 0; i < nNumPatches; i++) {
		INT32 nChecked = _TreeView_GetCheckState(hIpsList, hPatchHandlesIndex[i]);

		if (nChecked) {
			_tcscpy(szIpsActivePatches[nActivePatches], szPatchFileNames[i]);
			nActivePatches++;
		}
	}

	FILE* fp = _tfopen(GameIpsConfigName(), _T("wt"));

	if (fp) {
		_ftprintf(fp, _T("// ") _T(APP_TITLE) _T(" v%s --- IPS Config File for %s (%s)\n\n"), szAppBurnVer, szDriverName, szFullName);
		for (INT32 i = 0; i < nActivePatches; i++) {
			TCHAR *Tokens;
			TCHAR szFileName[MAX_PATH];
			Tokens = _tcstok(szIpsActivePatches[i], _T("\\"));
			while (Tokens != NULL) {
				szFileName[0] = _T('\0');
				_tcscpy(szFileName, Tokens);
				Tokens = _tcstok(NULL, _T("\\"));
			}

			_ftprintf(fp, _T("%s\n"), szFileName);
		}
		fclose(fp);
	}
}

static void IpsManagerExit()
{
	SendDlgItemMessage(hIpsDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);

	for (INT32 i = 0; i < NUM_LANGUAGES; i++) {
		szLanguages[i][0]     = _T('\0');
		szLanguageCodes[i][0] = _T('\0');
	}

	memset(hItemHandles,       0, MAX_NODES * sizeof(HTREEITEM));
	memset(hPatchHandlesIndex, 0, MAX_NODES * sizeof(HTREEITEM));

	nPatchIndex = 0;
	nNumPatches = 0;

	for (INT32 i = 0; i < MAX_NODES; i++) {
		szPatchFileNames[i][0] = _T('\0');
	}
	if (hBmp) {
		DeleteObject((HGDIOBJ)hBmp);
		hBmp = NULL;
	}
	if (hPreview) {
		DeleteObject((HGDIOBJ)hPreview);
		hPreview = NULL;
	}

	hParent = NULL;
	DeleteObject(hWhiteBGBrush);
	EndDialog(hIpsDlg, 0);
}

static void IpsOkay()
{
	SavePatches();
	IpsManagerExit();
}

static INT_PTR CALLBACK DefInpProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg) {
		case WM_INITDIALOG: {
			hIpsDlg = hDlg;

			hWhiteBGBrush = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));
			hPreview = PNGLoadBitmap(hIpsDlg, NULL, 304, 228, 2);
			SendDlgItemMessage(hIpsDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hPreview);

			LONG_PTR Style;
			Style = GetWindowLongPtr (GetDlgItem(hIpsDlg, IDC_TREE1), GWL_STYLE);
			Style |= TVS_CHECKBOXES;
			SetWindowLongPtr (GetDlgItem(hIpsDlg, IDC_TREE1), GWL_STYLE, Style);

			IpsManagerInit();
			INT32 nBurnDrvActiveOld = nBurnDrvActive;	// RockyWall Add
			WndInMid(hDlg, hScrnWnd);
			SetFocus(hDlg);								// Enable Esc=close
			nBurnDrvActive = nBurnDrvActiveOld;			// RockyWall Add
			break;
		}

		case WM_LBUTTONDBLCLK: {
			RECT PreviewRect;
			POINT Point;

			memset(&PreviewRect, 0, sizeof(RECT));
			memset(&Point,       0, sizeof(POINT));

			if (GetCursorPos(&Point) && GetWindowRect(GetDlgItem(hIpsDlg, IDC_SCREENSHOT_H), &PreviewRect)) {
				if (PtInRect(&PreviewRect, Point)) {
					FILE* fp = NULL;

					fp = _tfopen(szPngName, _T("rb"));
					if (fp) {
						fclose(fp);
						ShellExecute(  // Open the image with the associated program
							GetDlgItem(hIpsDlg, IDC_SCREENSHOT_H),
							NULL, szPngName, NULL, NULL, SW_SHOWNORMAL
						);
					}
				}
			}
			return 0;
		}

		case WM_COMMAND: {
			INT32 wID = LOWORD(wParam), Notify = HIWORD(wParam);

			if (Notify == BN_CLICKED) {
				switch (wID) {
					case IDOK: {
						IpsOkay();
						break;
					}

					case IDCANCEL: {
						SendMessage(hDlg, WM_CLOSE, 0, 0);
						return 0;
					}

					case IDC_IPSMAN_DESELECTALL: {
						for (INT32 i = 0; i < nNumPatches; i++) {
							for (INT32 j = 0; j < nNumPatches; j++) {
								_TreeView_SetCheckState(hIpsList, hPatchHandlesIndex[j], FALSE);
							}
						}
						break;
					}
				}
			}

			if (wID == IDC_CHOOSE_LIST && Notify == CBN_SELCHANGE) {
				nIpsSelectedLanguage = SendMessage(GetDlgItem(hIpsDlg, IDC_CHOOSE_LIST), CB_GETCURSEL, 0, 0);
				TreeView_DeleteAllItems(hIpsList);
				FillListBox();
				RefreshPatch();
				return 0;
			}
			break;
		}

		case WM_NOTIFY: {
			NMHDR* pNmHdr = (NMHDR*)lParam;

			if (LOWORD(wParam) == IDC_TREE1 && pNmHdr->code == TVN_SELCHANGED) {
				RefreshPatch();
				return 1;
			}

			if (LOWORD(wParam) == IDC_TREE1 && pNmHdr->code == NM_DBLCLK) {
				// disable double-click node-expand
				SetWindowLongPtr(hIpsDlg, DWLP_MSGRESULT, 1);
				return 1;
			}

			if (LOWORD(wParam) == IDC_TREE1 && pNmHdr->code == NM_CLICK) {
				POINT cursorPos;
				GetCursorPos(&cursorPos);
				ScreenToClient(hIpsList, &cursorPos);

				TVHITTESTINFO thi;
				thi.pt = cursorPos;
				TreeView_HitTest(hIpsList, &thi);

				if (thi.flags == TVHT_ONITEMSTATEICON) {
					TreeView_SelectItem(hIpsList, thi.hItem);
				}

				return 1;
			}

			if (LOWORD(wParam) == IDC_CHOOSE_LIST && pNmHdr->code == NM_DBLCLK) {
				// disable double-click node-expand
				SetWindowLongPtr(hIpsDlg, DWLP_MSGRESULT, 1);
				return 1;
			}

			SetWindowLongPtr(hIpsDlg, DWLP_MSGRESULT, CDRF_DODEFAULT);
			return 1;
		}

		case WM_CTLCOLORSTATIC: {
			if ((HWND)lParam == GetDlgItem(hIpsDlg, IDC_TEXTCOMMENT)) {
				return (INT_PTR)hWhiteBGBrush;
			}
			break;
		}

		case WM_CLOSE: {
			IpsManagerExit();
			break;
		}
	}

	return 0;
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
*/
#define IPS_SIGNATURE  "PATCH"
#define IPS_TAG_EOF    "EOF"
#define IPS_EXT        _T(".ips")

#define BYTE3_TO_UINT(bp)						\
	 (((UINT32)(bp)[0] << 16) & 0x00FF0000) |	\
	 (((UINT32)(bp)[1] <<  8) & 0x0000FF00) |	\
	 (( UINT32)(bp)[2]        & 0x000000FF)

#define BYTE2_TO_UINT(bp)						\
	(((UINT32)(bp)[0] << 8) & 0xFF00) |			\
	(( UINT32)(bp)[1]       & 0x00FF)

bool bDoIpsPatch = false;

static void PatchFile(const char* ips_path, UINT8* base, bool readonly)
{
	char buf[6];
	FILE* f      = NULL;
	INT32 Offset = 0, Size = 0;
	UINT8* mem8  = NULL;

	if (NULL == (f = fopen(ips_path, "rb"))) {
		bprintf(0, _T("IPS - Can't open file %S!  Aborting.\n"), ips_path);
		return;
	}

	memset(buf, 0, sizeof(buf));
	fread(buf, 1, 5, f);
	if (strcmp(buf, IPS_SIGNATURE)) {
		bprintf(0, _T("IPS - Bad IPS-Signature in: %S.\n"), ips_path);
		if (f)
		{
			fclose(f);
		}
		return;
	} else {
		bprintf(0, _T("IPS - Patching with: %S. (%S)\n"), ips_path, (readonly) ? "Read-Only" : "Write");
		UINT8 ch   = 0;
		INT32 bRLE = 0;
		while (!feof(f)) {
			// read patch address offset
			fread(buf, 1, 3, f);
			buf[3] = 0;
			if (strcmp(buf, IPS_TAG_EOF) == 0)
				break;

			Offset = BYTE3_TO_UINT(buf);

			// read patch length
			fread(buf, 1, 2, f);
			Size = BYTE2_TO_UINT(buf);

			bRLE = (Size == 0);
			if (bRLE) {
				fread(buf, 1, 2, f);
				Size = BYTE2_TO_UINT(buf);
				ch   = fgetc(f);
			}

			while (Size--) {
				if (!readonly) mem8 = base + Offset + nRomOffset;
				Offset++;
				if (readonly) {
					if (!bRLE) fgetc(f);
				} else {
					*mem8 = bRLE ? ch : fgetc(f);
				}
			}
		}
	}

	// Avoid memory out-of-bounds due to ips offset greater than rom length.
	if (readonly && (0 == nIpsMemExpLen[EXP_FLAG])) {	// Unspecified length.
		nIpsMemExpLen[LOAD_ROM] = Offset;
	}

	fclose(f);
}

static char* stristr_int(const char* str1, const char* str2)
{
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
}

static UINT32 hexto32(const char *s)
{
	UINT32 val = 0;
	char c;

	while ((c = *s++)) {
		UINT8 v = ((c & 0xf) + (c >> 6)) | ((c >> 3) & 0x8);
		val = (val << 4) | (UINT32)v;
	}

	return val;
}

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

static void DoPatchGame(const TCHAR* patch_name, const TCHAR* game_name, const UINT32 crc, UINT8* base, bool readonly)
{
	TCHAR s[MAX_PATH] = { 0 };
	TCHAR* p          = NULL;
	TCHAR* rom_name   = NULL;
	TCHAR* ips_name   = NULL;
	TCHAR* ips_offs   = NULL;
	TCHAR* ips_crc    = NULL;
	UINT32 nIps_crc   = 0;
	FILE* fp          = NULL;

	//bprintf(0, _T("DoPatchGame [%S][%S]\n"), patch_name, game_name);

	const TCHAR* pszReadMode = AdaptiveEncodingReads(patch_name);
	if (NULL == pszReadMode) return;

	const TCHAR* pszAppRomPaths = (2 == nQuickOpen) ? szAppQuickPath : szAppIpsPath;
	const TCHAR* pszDriverName  = (2 == nQuickOpen) ? szDriverName   : BurnDrvGetText(DRV_NAME);

	if ((fp = _tfopen(patch_name, pszReadMode)) != NULL) {
		bool bTarget = false;

		while (!feof(fp)) {
			if (_fgetts(s, sizeof(s), fp) != NULL) {
				p = s;
#if 0
				// skip UTF-8 sig
				if (strncmp(p, UTF8_SIGNATURE, strlen(UTF8_SIGNATURE)) == 0)
					p += strlen(UTF8_SIGNATURE);
#endif // 0
				if (p[0] == _T('['))	// reached info-section of .dat file, time to leave.
					break;

				// Can support linetypes: (space or tab)
				// "rom name.bin" "patch file.ips" CRC(abcd1234)
				// romname.bin patchfile CRC(abcd1234)
#define DELIM_TOKENS_NAME _T(" \t\r\n")
#define DELIM_TOKENS      _T(" \t\r\n()")

				rom_name = _strqtoken(p, DELIM_TOKENS_NAME);

				if (!rom_name)
					continue;
				if (*rom_name == _T('#'))
					continue;

				ips_name = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (!ips_name)
					continue;

				nIps_crc = 0;
				nRomOffset = 0; // Reset to 0
				if (NULL != (ips_offs = _strqtoken(NULL, DELIM_TOKENS))) {	// Parameters of the offset increment
					if (     0 == _tcscmp(ips_offs, _T("IPS_OFFSET_016"))) nRomOffset = 0x1000000;
					else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_032"))) nRomOffset = 0x2000000;
					else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_048"))) nRomOffset = 0x3000000;
					else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_064"))) nRomOffset = 0x4000000;
					else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_080"))) nRomOffset = 0x5000000;
					else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_096"))) nRomOffset = 0x6000000;
					else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_112"))) nRomOffset = 0x7000000;
					else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_128"))) nRomOffset = 0x8000000;
					else if (0 == _tcscmp(ips_offs, _T("IPS_OFFSET_144"))) nRomOffset = 0x9000000;

					if (nRomOffset != 0) { // better get next token (crc)
						ips_offs = _strqtoken(NULL, DELIM_TOKENS);
					}
				}

				if ((ips_offs != NULL) && stristr_int(_TtoA(ips_offs), "crc")) {
					ips_crc = _strqtoken(NULL, DELIM_TOKENS);
					if (ips_crc) {
						nIps_crc = hexto32(_TtoA(ips_crc));
					}
				}

#undef DELIM_TOKENS_NAME
#undef DELIM_TOKENS

				char* has_ext = stristr_int(_TtoA(ips_name), ".ips");

				if (_tcsicmp(rom_name, game_name))	// name don't match?
					if (nIps_crc != crc)			// crc don't match?
						continue;					// not our file. next!

				bTarget = true;

				if (!readonly) {
					bprintf(0, _T("ips name:[%S]\n"), ips_name);
					bprintf(0, _T("rom name:[%S]\n"), rom_name);
					bprintf(0, _T("rom crc :[%x]\n"), nIps_crc);
				}

				TCHAR ips_path[MAX_PATH] = { 0 };
				_stprintf(ips_path, _T("%s%s"), ips_name, (has_ext) ? _T("") : IPS_EXT);

				// Prioritize the use of ips files with custom correct paths
				if (!FileExists(ips_path)) {
					_stprintf(ips_path, _T(""));

					// Traditional IPS path
					if ((_T('\\') == ips_name[0]) || (_T('/') == ips_name[0])) {
						// ips in parent's folder
						// app_path: xxxxx/yyy/ or xxxxx\yyy\
						// ips_name: /aaa/n.ips or \aaa\n.ips, (ips_name + 1) -> aaa/n.ips or aaa\n.ips
						// app_path + ips_name: xxxxx/yyy/aaa/n.ips or xxxxx\yyy\aaa\n.ips
						_stprintf(ips_path, _T("%s%s%s"), pszAppRomPaths, ips_name + 1, (has_ext) ? _T("") : IPS_EXT);
					} else {
						// ips in drv_name's folder (Same folder as dat)
						// xxxxx/yyy/drv_name/.../n.ips or xxxxx\yyy\drv_name\...\n.ips
						_stprintf(ips_path, _T("%s%s\\%s%s"), pszAppRomPaths, pszDriverName, ips_name, (has_ext) ? _T("") : IPS_EXT);
					}
				}

				PatchFile(_TtoA(ips_path), base, readonly);
			}
		}
		fclose(fp);

		if (!bTarget && (0 == nIpsMemExpLen[EXP_FLAG])) {
			// Must be reset to 0!
			nIpsMemExpLen[LOAD_ROM] = 0;
		}
	}
}

static INT32 IpsVerifyDat(const TCHAR* pszDatFile)
{
	TCHAR s[MAX_PATH] = { 0 }, * p = NULL, * rom_name = NULL, * ips_name = NULL;
	FILE* fp = NULL;

	const TCHAR* pszReadMode = AdaptiveEncodingReads(pszDatFile);
	if (NULL == pszReadMode) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, _T("IPS: %s\n\n"), pszDatFile);
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_FILE_ENCODING), pszDatFile);
		FBAPopupDisplay(PUF_TYPE_ERROR);
		return -1;
	}
	if (NULL== (fp = _tfopen(pszDatFile, pszReadMode))) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, _T("IPS: %s\n\n"), pszDatFile);
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_FILE_OPEN), pszDatFile);
		FBAPopupDisplay(PUF_TYPE_ERROR);
		return -2;
	}

	const TCHAR* pszAppRomPaths = (2 == nQuickOpen) ? szAppQuickPath : szAppIpsPath;
	const TCHAR* pszDriverName  = (2 == nQuickOpen) ? szDriverName   : BurnDrvGetText(DRV_NAME);
	INT32 nLoop = 0, nError = 0, nFind = 0;

	while (!feof(fp)) {
		if (_fgetts(s, sizeof(s), fp) != NULL) {
			p = s;

			if (p[0] == _T('[')) {
				if (0 == nLoop) {
					nError++;
					FBAPopupAddText(PUF_TEXT_DEFAULT, _T("IPS: %s\n\n"), pszDatFile);
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_FILE_CONTENT), pszDatFile);
				}
				break;
			}

#define DELIM_TOKENS_NAME _T(" \t\r\n")
#define DELIM_TOKENS      _T(" \t\r\n()")

			rom_name = _strqtoken(p, DELIM_TOKENS_NAME);
			if (!rom_name)
				continue;
			if (*rom_name == _T('#'))
				continue;

			ips_name = _strqtoken(NULL, DELIM_TOKENS_NAME);
			if (!ips_name)
				continue;

#undef DELIM_TOKENS_NAME
#undef DELIM_TOKENS

			char* has_ext = stristr_int(_TtoA(ips_name), ".ips");
			TCHAR ips_path[MAX_PATH] = { 0 };
			_stprintf(ips_path, _T("%s%s"), ips_name, (has_ext) ? _T("") : IPS_EXT);

			if (!FileExists(ips_path)) {
				_stprintf(ips_path, _T(""));

				if ((_T('\\') == ips_name[0]) || (_T('/') == ips_name[0])) {
					_stprintf(ips_path, _T("%s%s%s"), pszAppRomPaths, ips_name + 1, (has_ext) ? _T("") : IPS_EXT);
				} else {
					_stprintf(ips_path, _T("%s%s\\%s%s"), pszAppRomPaths, pszDriverName, ips_name, (has_ext) ? _T("") : IPS_EXT);
				}
				if (!FileExists(ips_path)) {
					nError++;
					if (1 == nError) {
						FBAPopupAddText(PUF_TEXT_DEFAULT, _T("IPS: %s\n\n"), pszDatFile);
					}
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_FILE_EXIST), ips_path);
				} else {
					nFind++;
				}
			} else {
				nFind++;
			}
		}
		nLoop++;
	}
	fclose(fp);

	if (nError > 0) {
		FBAPopupDisplay(PUF_TYPE_ERROR);
	}

	return (nFind > 0) ? 0 : -3;
}

#undef IPS_EXT
#undef IPS_TAG_EOF
#undef IPS_SIGNATURE

static UINT32 GetIpsDefineExpValue(TCHAR* pszTmp)
{
	if (NULL == (pszTmp = _strqtoken(NULL, _T(" \t\r\n"))))
		return 0U;

	INT32 nRet = 0;

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
	else if (EOF != (_stscanf(pszTmp, _T("%x"), &nRet))) return nRet;

	return nRet;
}

static void ExtraPatchesExit()
{
	if (_IpsEarly.nCount > 0) {
		for (UINT32 i = 0; i < _IpsEarly.nCount; i++) {
			if (NULL != _IpsEarly.pszDat[i]) {
				free(_IpsEarly.pszDat[i]);
				_IpsEarly.pszDat[i] = NULL;
			}
		}
	}
	free(_IpsEarly.pszDat);
	_IpsEarly.pszDat = NULL;
	_IpsEarly.nCount = 0;

	if (_IpsLast.nCount > 0) {
		for (UINT32 i = 0; i < _IpsLast.nCount; i++) {
			if (NULL != _IpsLast.pszDat[i]) {
				free(_IpsLast.pszDat[i]);
				_IpsLast.pszDat[i] = NULL;
			}
		}
	}
	free(_IpsLast.pszDat);
	_IpsLast.pszDat = NULL;
	_IpsLast.nCount = 0;
}

#if 0
static bool NormalizePath(const TCHAR* pSrcPath, TCHAR* pDestPath, UINT32 nDestSize) {
	if ((NULL == pSrcPath) || (NULL == pDestPath) || (0 == nDestSize))
		return false;

	TCHAR szFullPath[MAX_PATH] = { 0 };
	DWORD dwRet = GetFullPathName(pSrcPath, MAX_PATH, szFullPath, NULL);
	if ((0 == dwRet) || (dwRet >= MAX_PATH))
		return false;

	for (UINT32 i = 0; i < dwRet; i++) {
		if (_T('\\') == szFullPath[i])
			pDestPath[i] = _T('/');
		else
			pDestPath[i] = _tolower(szFullPath[i]);
	}
	pDestPath[dwRet] = _T('\0');
	return true;
}

static bool CompareTwoPaths(const TCHAR* pszPathA, const TCHAR* pszPathB)
{
	TCHAR szPathA[MAX_PATH] = { 0 }, szPathB[MAX_PATH] = { 0 };

	if (!NormalizePath(pszPathA, szPathA, MAX_PATH) ||
		!NormalizePath(pszPathB, szPathB, MAX_PATH))
		return false;

	return (0 == _tcsicmp(szPathA, szPathB));
}
#endif

static INT32 IpsDatPathGetInfo(const TCHAR* pszSelDat, TCHAR** pszDrvName, TCHAR** pszIpsPath)
{
	*pszDrvName = *pszIpsPath = NULL;

	if ((NULL == pszSelDat) || !FileExists(pszSelDat)) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, _T("IPS %s:\n\n"), pszSelDat);
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_FILE_EXIST), pszSelDat);
		FBAPopupDisplay(PUF_TYPE_ERROR);
		return -2;
	}

	const TCHAR* pszExt = _tcsrchr(pszSelDat, _T('.'));
	if (NULL == pszExt || (0 != _tcsicmp(_T(".dat"), pszExt))) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, _T("IPS %s:\n\n"), pszSelDat);
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_FILE_EXTENSION), pszSelDat, _T(".dat"));
		FBAPopupDisplay(PUF_TYPE_ERROR);
		return -5;
	}

	const TCHAR* p = pszSelDat + _tcslen(pszSelDat), * drv_start = NULL, * drv_end = NULL, * dir_end = NULL;
	INT32 nCount = 0;
	while (p > pszSelDat) {
		if ((_T('/') == *p) || (_T('\\') == *p)) {
			TCHAR c = *(p - 1);
			if ((_T('/') == c) ||
				(_T('\\') == c)) {		// xxxx//ssss\\...
				FBAPopupAddText(PUF_TEXT_DEFAULT, _T("IPS %s:\n\n"), pszSelDat);
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_FILE_EXIST), pszSelDat);
				FBAPopupDisplay(PUF_TYPE_ERROR);
				return -2;
			}

			nCount++;
			if (1 == nCount) {
				drv_end = p;			// Intentionally add 1
			}
			if (2 == nCount) {
				dir_end = drv_start = p + 1;
			}
		}
		p--;
	}

	TCHAR szDrvName[64] = { 0 }, szIpsPath[MAX_PATH] = { 0 };
	switch (nCount) {
		case 0:							// xxx.dat
			FBAPopupAddText(PUF_TEXT_DEFAULT, _T("IPS %s:\n\n"), pszSelDat);
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DRIVER_NOT_EXIST), pszSelDat);
			FBAPopupDisplay(PUF_TYPE_ERROR);
			return -4;
		case 1:							// drv_name/xxx.dat
			drv_start = dir_end = p;
			_tcscpy(szIpsPath, _T(""));
			break;
		default:
			_tcsncpy(szIpsPath, pszSelDat, dir_end - pszSelDat);
			break;
	}

	const UINT32 nDrvLen = drv_end - drv_start;
	if (0 == nDrvLen) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, _T("IPS %s:\n\n"), pszSelDat);
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DRIVER_NOT_EXIST), pszSelDat);
		FBAPopupDisplay(PUF_TYPE_ERROR);
		return -4;
	}

	_tcsncpy(szDrvName, drv_start, nDrvLen);

	const INT32 nDrvIdx = BurnDrvGetIndex(_TtoA(szDrvName));
	if (nDrvIdx < 0) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, _T("IPS %s:\n\n"), pszSelDat);
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DRIVER_NOT_EXIST), pszSelDat);
		FBAPopupDisplay(PUF_TYPE_ERROR);
		return -4;
	}

	*pszDrvName = _tcsdup(szDrvName);	// The caller needs to free it
	*pszIpsPath = _tcsdup(szIpsPath);	// free(pszIpsPath)

	return nDrvIdx;
}

INT32 IpsGetDrvForQuickOpen(const TCHAR* pszSelDat)
{
	TCHAR* pszDrv, * pszPath;
	const INT32 nDrvIdx = IpsDatPathGetInfo(pszSelDat, &pszDrv, &pszPath);
	if (nDrvIdx < 0)
		return nDrvIdx;

	_tcscpy(szDriverName,   pszDrv);  free(pszDrv);
	_tcscpy(szAppQuickPath, pszPath); free(pszPath);

	INT32 nVerify = IpsVerifyDat(pszSelDat);
	if (0 != nVerify) {
		memset(szDriverName, 0, sizeof(szDriverName));
		return nVerify;
	}

	_tcscpy(szIpsActivePatches[0], pszSelDat);

	for (INT32 i = 1; i < MAX_ACTIVE_PATCHES; i++) {
		_stprintf(szIpsActivePatches[i], _T(""));
	}

	return nDrvIdx;
}

static void ExtraPatchesInit(const INT32 nActivePatches)
{
	ExtraPatchesExit();

	const TCHAR* pszAppRomPaths = (2 == nQuickOpen) ? szAppQuickPath : szAppIpsPath;
	const TCHAR* pszDriverName  = (2 == nQuickOpen) ? szDriverName   : BurnDrvGetText(DRV_NAME);

	for (INT32 i = 0; i < nActivePatches; i++) {
		TCHAR str[MAX_PATH] = { 0 }, * ptr = NULL, * tmp = NULL;
		FILE* fp = NULL;
		INT32 nNumbers = 0;

		const TCHAR* pszReadMode = AdaptiveEncodingReads(szIpsActivePatches[i]);
		if (NULL == pszReadMode) continue;

		if (NULL != (fp = _tfopen(szIpsActivePatches[i], pszReadMode))) {
			while (!feof(fp)) {
				if (NULL != _fgetts(str, sizeof(str), fp)) {
					ptr = str;

					if (NULL == (tmp = _strqtoken(ptr, _T(" \t\r\n"))))
						continue;
					if (_T('[') == tmp[0])
						break;
					if ((_T('/') == tmp[0]) && (_T('/') == tmp[1]))
						continue;
					if ((0 != _tcscmp(tmp, _T("#define"))) && (0 != _tcscmp(tmp, _T("#include")))) {
						nNumbers++;
						continue;
					} else {
						if (0 == _tcscmp(tmp, _T("#define"))) {
							continue;
						}
						if (0 == _tcscmp(tmp, _T("#include"))) {
							if (NULL == (tmp = _strqtoken(NULL, _T(" \t\r\n"))))
								continue;
							/*
								#inclede "kof97"
								#inclede "kof97.dat"
							*/
							// Only dat files in the same directory are allowed
							// Only accepts ips with the same driver
							if ((NULL != _tcsrchr(tmp, _T('/'))) || (NULL != _tcsrchr(tmp, _T('\\'))))
								continue;

							// ips_path/drv_name/game.dat
							TCHAR szInclde[MAX_PATH] = { 0 };
							const TCHAR* pszFormat = (NULL == _tcsrchr(tmp, _T('.'))) ? _T("%s%s\\%s.dat") : _T("%s%s\\%s");
							_stprintf(szInclde, pszFormat, pszAppRomPaths, pszDriverName, tmp);

							// Check if the file exists
							if (!FileExists(szInclde))
								continue;

							TCHAR** newArray = (TCHAR**)realloc((nNumbers > 0) ? _IpsLast.pszDat : _IpsEarly.pszDat, (((nNumbers > 0) ? _IpsLast.nCount : _IpsEarly.nCount) + 1) * sizeof(TCHAR*));
							((nNumbers > 0) ? _IpsLast.pszDat : _IpsEarly.pszDat) = newArray;

							((nNumbers > 0) ? _IpsLast.pszDat : _IpsEarly.pszDat)[(nNumbers > 0) ? _IpsLast.nCount : _IpsEarly.nCount] = (TCHAR*)malloc(MAX_PATH * sizeof(TCHAR));
							_tcscpy(((nNumbers > 0) ? _IpsLast.pszDat : _IpsEarly.pszDat)[(nNumbers > 0) ? _IpsLast.nCount : _IpsEarly.nCount], szInclde);
							((nNumbers > 0) ? _IpsLast.nCount : _IpsEarly.nCount)++;
						}
					}
				}
			}
			fclose(fp);
		}
	}
}

// Run once to get the definition & definition values of the DAT files.
// Suppress CPU usage caused by multiple runs.
// Two entry points: cmdline Launch & SelOkay.
static void GetIpsDrvDefine()
{
	if (!bDoIpsPatch)
		return;

	nIpsDrvDefine = 0;
	memset(nIpsMemExpLen, 0, sizeof(nIpsMemExpLen));

	INT32 nActivePatches = GetIpsNumActivePatches();
	ExtraPatchesInit(nActivePatches);

	INT32 nCount = _IpsEarly.nCount + nActivePatches + _IpsLast.nCount;

	for (INT32 i = 0; i < nCount; i++) {
		TCHAR str[MAX_PATH] = { 0 }, * ptr = NULL, * tmp = NULL, * pszDatName = szIpsActivePatches[i];
		FILE* fp = NULL;

		if ((_IpsEarly.nCount) > 0 || (_IpsLast.nCount > 0)) {
			if ((i >= 0) && (i < _IpsEarly.nCount)) {
				pszDatName = _IpsEarly.pszDat[i];
			}
			if ((i >= _IpsEarly.nCount) && (i < (_IpsEarly.nCount + nActivePatches))) {
				pszDatName = szIpsActivePatches[i - _IpsEarly.nCount];
			}
			if (i >= (_IpsEarly.nCount + nActivePatches)) {
				pszDatName = _IpsLast.pszDat[i - (_IpsEarly.nCount + nActivePatches)];
			}
		}

		const TCHAR* pszReadMode = AdaptiveEncodingReads(pszDatName);
		if (NULL == pszReadMode) continue;

		if (NULL != (fp = _tfopen(pszDatName, pszReadMode))) {
			while (!feof(fp)) {
				if (NULL != _fgetts(str, sizeof(str), fp)) {
					ptr = str;
#if 0
					// skip UTF-8 sig
					if (0 == strncmp(ptr, UTF8_SIGNATURE, strlen(UTF8_SIGNATURE)))
						ptr += strlen(UTF8_SIGNATURE);
#endif // 0
					if (NULL == (tmp = _strqtoken(ptr,  _T(" \t\r\n"))))
						continue;
					if (_T('[') == tmp[0])
						break;
					if ((_T('/') == tmp[0]) && (_T('/') == tmp[1]))
						continue;
					if (0 == _tcscmp(tmp, _T("#define"))) {
						if (NULL == (tmp = _strqtoken(NULL, _T(" \t\r\n"))))
							continue;

						UINT32 nNewValue = 0;

						if (0 == _tcscmp(tmp, _T("IPS_NOT_PROTECT"))) {
							nIpsDrvDefine |= IPS_NOT_PROTECT;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_PGM_SPRHACK"))) {
							nIpsDrvDefine |= IPS_PGM_SPRHACK;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_PGM_MAPHACK"))) {
							nIpsDrvDefine |= IPS_PGM_MAPHACK;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_PGM_SNDOFFS"))) {
							nIpsDrvDefine |= IPS_PGM_SNDOFFS;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_SNES_VRAMHK"))) {
							nIpsDrvDefine |= IPS_SNES_VRAMHK;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_NEO_RAMHACK"))) {
							nIpsDrvDefine |= IPS_NEO_RAMHACK;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_LOAD_EXPAND"))) {
							nIpsDrvDefine |= IPS_LOAD_EXPAND;
							nIpsMemExpLen[EXP_FLAG] = 1;
							if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[LOAD_ROM])
								nIpsMemExpLen[LOAD_ROM] = nNewValue;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_EXTROM_INCL"))) {
							nIpsDrvDefine |= IPS_EXTROM_INCL;
							if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[EXTR_ROM])
								nIpsMemExpLen[EXTR_ROM] = nNewValue;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_PRG1_EXPAND"))) {
							nIpsDrvDefine |= IPS_PRG1_EXPAND;
							if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[PRG1_ROM])
								nIpsMemExpLen[PRG1_ROM] = nNewValue;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_PRG2_EXPAND"))) {
							nIpsDrvDefine |= IPS_PRG2_EXPAND;
							if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[PRG2_ROM])
								nIpsMemExpLen[PRG2_ROM] = nNewValue;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_GRA1_EXPAND"))) {
							nIpsDrvDefine |= IPS_GRA1_EXPAND;
							if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[GRA1_ROM])
								nIpsMemExpLen[GRA1_ROM] = nNewValue;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_GRA2_EXPAND"))) {
							nIpsDrvDefine |= IPS_GRA2_EXPAND;
							if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[GRA2_ROM])
								nIpsMemExpLen[GRA2_ROM] = nNewValue;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_GRA3_EXPAND"))) {
							nIpsDrvDefine |= IPS_GRA3_EXPAND;
							if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[GRA3_ROM])
								nIpsMemExpLen[GRA3_ROM] = nNewValue;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_ACPU_EXPAND"))) {
							nIpsDrvDefine |= IPS_ACPU_EXPAND;
							if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[ACPU_ROM])
								nIpsMemExpLen[ACPU_ROM] = nNewValue;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_SND1_EXPAND"))) {
							nIpsDrvDefine |= IPS_SND1_EXPAND;
							if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[SND1_ROM])
								nIpsMemExpLen[SND1_ROM] = nNewValue;
							continue;
						}
						if (0 == _tcscmp(tmp, _T("IPS_SND2_EXPAND"))) {
							nIpsDrvDefine |= IPS_SND2_EXPAND;
							if ((nNewValue = GetIpsDefineExpValue(tmp)) > nIpsMemExpLen[SND2_ROM])
								nIpsMemExpLen[SND2_ROM] = nNewValue;
							continue;
						}
					}
				}
			}
			fclose(fp);
		}
	}
}

void IpsApplyPatches(UINT8* base, char* rom_name, UINT32 crc, bool readonly)
{
	if (!bDoIpsPatch)
		return;

	INT32 nActivePatches = GetIpsNumActivePatches();
	INT32 nCount = _IpsEarly.nCount + nActivePatches + _IpsLast.nCount;

	for (INT32 i = 0; i < nCount; i++) {
		TCHAR* pszDatName = szIpsActivePatches[i];
		if ((_IpsEarly.nCount) > 0 || (_IpsLast.nCount > 0)) {
			if ((i >= 0) && (i < _IpsEarly.nCount)) {
				pszDatName = _IpsEarly.pszDat[i];
			}
			if ((i >= _IpsEarly.nCount) && (i < (_IpsEarly.nCount + nActivePatches))) {
				pszDatName = szIpsActivePatches[i - _IpsEarly.nCount];
			}
			if (i >= (_IpsEarly.nCount + nActivePatches)) {
				pszDatName = _IpsLast.pszDat[i - (_IpsEarly.nCount + nActivePatches)];
			}
		}
		DoPatchGame(pszDatName, _AtoT(rom_name), crc, base, readonly);
	}
}

void IpsPatchInit()
{
	GetIpsDrvDefine();
}

void IpsPatchExit()
{
	ExtraPatchesExit();
	memset(nIpsMemExpLen, 0, sizeof(nIpsMemExpLen));

	nIpsDrvDefine = 0;
	bDoIpsPatch   = false;
}
