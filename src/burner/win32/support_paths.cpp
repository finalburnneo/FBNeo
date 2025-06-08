#include "burner.h"
#include <shlobj.h>

static HWND hTabControl = NULL;
static HWND hSupportDlg = NULL;
static HWND hParent     = NULL;

TCHAR szAppPreviewsPath[MAX_PATH]	= _T("support/previews/");
TCHAR szAppTitlesPath[MAX_PATH]		= _T("support/titles/");
TCHAR szAppCheatsPath[MAX_PATH]		= _T("support/cheats/");
TCHAR szAppHiscorePath[MAX_PATH]	= _T("support/hiscores/");
TCHAR szAppSamplesPath[MAX_PATH]	= _T("support/samples/");
TCHAR szAppHDDPath[MAX_PATH]		= _T("support/hdd/");
TCHAR szAppIpsPath[MAX_PATH]		= _T("support/ips/");
TCHAR szAppRomdataPath[MAX_PATH]	= _T("support/romdata/");
TCHAR szAppIconsPath[MAX_PATH]		= _T("support/icons/");
TCHAR szAppBlendPath[MAX_PATH]		= _T("support/blend/");
TCHAR szAppSelectPath[MAX_PATH]		= _T("support/select/");
TCHAR szAppVersusPath[MAX_PATH]		= _T("support/versus/");
TCHAR szAppHowtoPath[MAX_PATH]		= _T("support/howto/");
TCHAR szAppScoresPath[MAX_PATH]		= _T("support/scores/");
TCHAR szAppBossesPath[MAX_PATH]		= _T("support/bosses/");
TCHAR szAppGameoverPath[MAX_PATH]	= _T("support/gameover/");
TCHAR szAppFlyersPath[MAX_PATH]		= _T("support/flyers/");
TCHAR szAppMarqueesPath[MAX_PATH]	= _T("support/marquees/");
TCHAR szAppControlsPath[MAX_PATH]	= _T("support/cpanel/");
TCHAR szAppCabinetsPath[MAX_PATH]	= _T("support/cabinets/");
TCHAR szAppPCBsPath[MAX_PATH]		= _T("support/pcbs/");
TCHAR szAppHistoryPath[MAX_PATH]	= _T("support/history/");
TCHAR szAppEEPROMPath[MAX_PATH]		= _T("config/games/");

static TCHAR* pszSupportPath[25][2] = {
	{ szAppPreviewsPath, _T("support/previews/") },
	{ szAppTitlesPath,   _T("support/titles/")   },
	{ szAppIconsPath,    _T("support/icons/")    },
	{ szAppCheatsPath,   _T("support/cheats/")   },
	{ szAppHiscorePath,  _T("support/hiscores/") },
	{ szAppSamplesPath,  _T("support/samples/")  },
	{ szAppIpsPath,      _T("support/ips/")      },
	{ szAppRomdataPath,  _T("support/romdata/")  },
	{ szNeoCDGamesDir,   _T("neocdiso/")         },
	{ szNeoCDCoverDir,   _T("support/neocdz/")   },
	{ szAppBlendPath,    _T("support/blend/")    },
	{ szAppSelectPath,   _T("support/select/")   },
	{ szAppVersusPath,   _T("support/versus/")   },
	{ szAppHowtoPath,    _T("support/howto/")    },
	{ szAppScoresPath,   _T("support/scores/")   },
	{ szAppBossesPath,   _T("support/bosses/")   },
	{ szAppGameoverPath, _T("support/gameover/") },
	{ szAppFlyersPath,   _T("support/flyers/")   },
	{ szAppMarqueesPath, _T("support/marquees/") },
	{ szAppControlsPath, _T("support/cpanel/")   },
	{ szAppCabinetsPath, _T("support/cabinets/") },
	{ szAppPCBsPath,     _T("support/pcbs/")     },
	{ szAppHistoryPath  ,_T("support/history/")  },
	{ szAppEEPROMPath,   _T("config/games/")     },
	{ szAppHDDPath,      _T("support/hdd/")      }
};

static TCHAR szCheckIconsPath[MAX_PATH];

INT32 nSupportDlgWidth  = 0x21c;
INT32 nSupportDlgHeight = 0x2ec;
static INT32 nDlgInitialWidth;
static INT32 nDlgInitialHeight;
static INT32 nDlgTabCtrlInitialPos[4];
static INT32 nDlgGroupCtrlInitialPos[4];
static INT32 nDlgOKBtnInitialPos[4];
static INT32 nDlgCancelBtnInitialPos[4];
static INT32 nDlgDefaultsBtnInitialPos[4];
static INT32 nDlgTextCtrlInitialPos[25][4];
static INT32 nDlgEditCtrlInitialPos[25][4];
static INT32 nDlgBtnCtrlInitialPos[25][4];

// Dialog sizing support functions and macros (everything working in client co-ords)
#define GetInititalControlPos(a, b)								\
	GetWindowRect(GetDlgItem(hSupportDlg, a), &rect);			\
	memset(&point, 0, sizeof(POINT));							\
	point.x = rect.left;										\
	point.y = rect.top;											\
	ScreenToClient(hSupportDlg, &point);						\
	b[0] = point.x;												\
	b[1] = point.y;												\
	GetClientRect(GetDlgItem(hSupportDlg, a), &rect);			\
	b[2] = rect.right;											\
	b[3] = rect.bottom;

#define SetControlPosAlignTopLeft(a, b)							\
	SetWindowPos(GetDlgItem(hSupportDlg, a), hSupportDlg, b[0], b[1], 0, 0, SWP_NOZORDER | SWP_NOSIZE);

#define SetControlPosAlignTopLeftResizeHor(a, b)				\
	SetWindowPos(GetDlgItem(hSupportDlg, a), hSupportDlg, b[0], b[1], b[2] + 4 - xDelta, b[3] + 4, SWP_NOZORDER);

#define SetControlPosAlignTopLeftResizeHorVert(a, b)			\
	SetWindowPos(GetDlgItem(hSupportDlg, a), hSupportDlg, b[0], b[1], b[2] - xDelta, b[3] - yDelta, SWP_NOZORDER);

#define SetControlPosAlignTopRight(a, b)						\
	SetWindowPos(GetDlgItem(hSupportDlg, a), hSupportDlg, b[0] - xDelta, b[1], 0, 0, SWP_NOZORDER | SWP_NOSIZE);

#define SetControlPosAlignBottomLeft(a, b)						\
	SetWindowPos(GetDlgItem(hSupportDlg, a), hSupportDlg, b[0], b[1] - yDelta, b[2], b[3], SWP_NOZORDER);

#define SetControlPosAlignBottomRight(a, b)						\
	SetWindowPos(GetDlgItem(hSupportDlg, a), hSupportDlg, b[0] - xDelta, b[1] - yDelta, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

static void GetInitialPositions()
{
	RECT rect;
	POINT point;

	GetClientRect(hSupportDlg, &rect);
	nDlgInitialWidth = rect.right;
	nDlgInitialHeight = rect.bottom;

	GetInititalControlPos(IDC_TAB1,    nDlgTabCtrlInitialPos);
	GetInititalControlPos(IDC_STATIC1, nDlgGroupCtrlInitialPos);
	GetInititalControlPos(IDOK,        nDlgOKBtnInitialPos);
	GetInititalControlPos(IDCANCEL,    nDlgCancelBtnInitialPos);
	GetInititalControlPos(IDDEFAULT,   nDlgDefaultsBtnInitialPos);

	for (INT32 i = 0; i < 25; i++) {
		GetInititalControlPos(IDC_SUPPORTDIR_TEXT1 + i, nDlgTextCtrlInitialPos[i]);
		GetInititalControlPos(IDC_SUPPORTDIR_EDIT1 + i, nDlgEditCtrlInitialPos[i]);
		GetInititalControlPos(IDC_SUPPORTDIR_BR1   + i, nDlgBtnCtrlInitialPos[i]);
	}
}

static void IconsDirPathChanged() {
	CreateDrvIconsCache();
}

static bool IsRelativePath(const TCHAR* pszPath)
{
	// Invalid input
	if ((NULL == pszPath) || (_T('\0' == pszPath[0]))) {
		return false;
	}

	// Check for absolute path flags (drive letter or root / network path)
	if (
		((_tcslen(pszPath) >= 3) && _istalpha(pszPath[0]) && (_T(':') == pszPath[1]) && (_T('\\' == pszPath[2]) || (_T('/') == pszPath[2]))) ||
		((_T('\\') == pszPath[0]) || (_T('/') == pszPath[0]))
	) {
		return false;
	}

	// Check that it is not a '.' or '..' starts with (e.g. . \folder or . \parent)
	if (
		(pszPath[0] == _T('.') && (pszPath[1] == _T('\0') || pszPath[1] == _T('\\') || pszPath[1] == _T('/'))) ||
		(pszPath[0] == _T('.') && pszPath[1] == _T('.') && (pszPath[2] == _T('\0') || pszPath[2] == _T('\\') || pszPath[2] == _T('/')))
	) {
		return true;
	}

	return true;
}

static UINT32 GetAppDirectory(TCHAR* pszAppPath)
{
	TCHAR szExePath[MAX_PATH] = { 0 };

	// Get executable file path
	if (0 == GetModuleFileName(NULL, szExePath, MAX_PATH))
		return 0;

	// Find last slash or backslash
	TCHAR* pLastSlash = _tcsrchr(szExePath, _T('\\'));
	if (NULL == pLastSlash) {
		pLastSlash = _tcsrchr(szExePath, _T('/'));
		if (NULL == pLastSlash)
			return 0;
	}

	// Calculate directory length (with slash or backslash)
	UINT32 nLen = pLastSlash - szExePath + 1;
	if (nLen >= MAX_PATH)
		return 0;

	if (NULL != pszAppPath) {
		_tcsncpy(pszAppPath, szExePath, nLen);
		pszAppPath[nLen] = _T('\0');
	}

	return nLen;
}

static INT32 ConvertToAbsolutePath(const TCHAR* pszPath, TCHAR* pszAbsolutePath)
{
	if (NULL == pszPath)
		return -1;

	INT32 nRet = -1;
//	TCHAR szAbsolutePath[MAX_PATH] = { 0 };

	if (IsRelativePath(pszPath)) {
		TCHAR szAppPath[MAX_PATH] = { 0 };
		UINT32 nAppPathLen = GetAppDirectory(szAppPath);
		if (0 == nAppPathLen)
			return -1;

		UINT32 nPathLen = _tcslen(pszPath);
		if ((nRet = nAppPathLen + nPathLen) >= MAX_PATH)
			return -1;

		if (NULL != pszAbsolutePath) {
			_stprintf(pszAbsolutePath, _T("%s%s"), szAppPath, pszPath);

			for (UINT32 i = 0; _T('\0') != pszAbsolutePath[i]; i++) {
				if (pszAbsolutePath[i] == _T('/')) pszAbsolutePath[i] = _T('\\');
			}
		}

		return nRet;
	}

	if ((nRet = _tcslen(pszPath)) >= MAX_PATH)
		return -1;

	if (NULL != pszAbsolutePath) {
		_tcscpy(pszAbsolutePath, pszPath);
	}

	return nRet;
}

static INT32 CALLBACK BRProc(HWND hWnd, UINT uMsg, LPARAM /*lParam*/, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED) {
		SendMessage(hWnd, BFFM_SETSELECTION, (WPARAM)TRUE, (LPARAM)lpData);
	}

	return 0;
}

static INT_PTR CALLBACK DefInpProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HFONT  hFont         = NULL;
	HBRUSH hWhiteBGBrush = NULL;
	TCHAR szAbsolutePath[MAX_PATH] = { 0 };
	INT32 var;

	switch (Msg) {
		case WM_INITDIALOG: {
			hSupportDlg = hDlg;

			// add WS_MAXIMIZEBOX button;
			SetWindowLongPtr(hDlg, GWL_STYLE, GetWindowLongPtr(hDlg, GWL_STYLE) | WS_MAXIMIZEBOX);

			HICON hIcon = LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_APP));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

			hWhiteBGBrush = CreateSolidBrush(RGB(0xff, 0xff, 0xff));

			TCHAR szDialogTitle[200];
			_stprintf(szDialogTitle, FBALoadStringEx(hAppInst, IDS_SUPPORTPATH_TITLE, true), _T(APP_TITLE), _T(SEPERATOR_1));
			SetWindowText(hDlg, szDialogTitle);

			// Setup edit controls values
			for (INT32 x = 0; x < 25; x++) {
				SetDlgItemText(hDlg, IDC_SUPPORTDIR_EDIT1 + x, pszSupportPath[x][0]);
			}

			// Setup the tabs
			hTabControl = GetDlgItem(hDlg, IDC_TAB1);
			TC_ITEM tcItem = { 0 };
			tcItem.mask = TCIF_TEXT;
			tcItem.pszText = FBALoadStringEx(hAppInst, IDS_SUPPORT_PATHS, true);
			TabCtrl_InsertItem(hTabControl, 0, &tcItem);

			// Font
			LOGFONT lf = { 0 };
			SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(LOGFONT), &lf, 0);
			lf.lfHeight = -MulDiv(9, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);
			lf.lfWeight = FW_BOLD;
			hFont = CreateFontIndirect(&lf);
			SendMessage(hTabControl, WM_SETFONT, (WPARAM)hFont, TRUE);

			for (int x = 0; x < 25; x++) {
				ShowWindow(GetDlgItem(hDlg, IDC_SUPPORTDIR_BR1   + x), SW_SHOW);	// browse buttons
				ShowWindow(GetDlgItem(hDlg, IDC_SUPPORTDIR_EDIT1 + x), SW_SHOW);	// edit controls
				ShowWindow(GetDlgItem(hDlg, IDC_SUPPORTDIR_TEXT1 + x), SW_SHOW);	// text controls
			}

			UpdateWindow(hDlg);
			GetInitialPositions();
			SetWindowPos(hDlg, NULL, 0, 0, nSupportDlgWidth, nSupportDlgWidth, SWP_NOZORDER);

			WndInMid(hDlg, hParent);
			SetFocus(hDlg);															// Enable Esc=close
			break;
		}
		case WM_CTLCOLORSTATIC: {
			HDC hDc = (HDC)wParam;
			SetBkMode(hDc, TRANSPARENT);											// LTEXT control with transparent background
			return (LRESULT)GetStockObject(HOLLOW_BRUSH);							// Return to empty brush
			for (int x = 0; x < 25; x++) {
				if ((HWND)lParam == GetDlgItem(hDlg, IDC_SUPPORTDIR_EDIT1 + x)) return (INT_PTR)hWhiteBGBrush;
				if ((HWND)lParam == GetDlgItem(hDlg, IDC_SUPPORTDIR_TEXT1 + x)) return (LRESULT)GetStockObject(HOLLOW_BRUSH);
			}
		}
		case WM_GETMINMAXINFO: {
			MINMAXINFO* info = (MINMAXINFO*)lParam;
			info->ptMinTrackSize.x = nDlgInitialWidth;
			info->ptMinTrackSize.y = nDlgInitialHeight + 40;
			return 0;
		}
		case WM_SIZE: {
			if (nDlgInitialWidth == 0 || nDlgInitialHeight == 0) return 0;

			RECT rc;
			GetClientRect(hDlg, &rc);

			const INT32 xDelta = nDlgInitialWidth  - rc.right;
			const INT32 yDelta = nDlgInitialHeight - rc.bottom;
			if (xDelta == 0 && yDelta == 0) return 0;

			SetControlPosAlignTopLeftResizeHorVert(IDC_TAB1,    nDlgTabCtrlInitialPos);
			SetControlPosAlignTopLeftResizeHorVert(IDC_STATIC1, nDlgGroupCtrlInitialPos);

			SetControlPosAlignBottomRight(IDOK,     nDlgOKBtnInitialPos);
			SetControlPosAlignBottomRight(IDCANCEL, nDlgCancelBtnInitialPos);

			SetControlPosAlignBottomLeft(IDDEFAULT, nDlgDefaultsBtnInitialPos);

			for (INT32 i = 0; i < 25; i++) {
				SetControlPosAlignTopLeft(IDC_SUPPORTDIR_TEXT1          + i, nDlgTextCtrlInitialPos[i]);
				SetControlPosAlignTopLeftResizeHor(IDC_SUPPORTDIR_EDIT1 + i, nDlgEditCtrlInitialPos[i]);
				SetControlPosAlignTopRight(IDC_SUPPORTDIR_BR1           + i, nDlgBtnCtrlInitialPos[i]);
			}

			InvalidateRect(hDlg, NULL, true);
			UpdateWindow(hDlg);

			return 0;
		}
		case WM_COMMAND: {
			LPMALLOC pMalloc = NULL;
			BROWSEINFO bInfo;
			ITEMIDLIST* pItemIDList = NULL;
			TCHAR buffer[MAX_PATH];

			if (LOWORD(wParam) == IDOK) {
				for (int i = 0; i < 25; i++) {
					GetDlgItemText(hDlg, IDC_SUPPORTDIR_EDIT1 + i, buffer, sizeof(buffer));
					// add trailing backslash
					INT32 strLen = _tcslen(buffer);
					if (strLen) {
						if (buffer[strLen - 1] != _T('\\') && buffer[strLen - 1] != _T('/') ) {
							buffer[strLen + 0]  = _T('\\');
							buffer[strLen + 1]  = _T('\0');
						}
					}
					_tcscpy(pszSupportPath[i][0], buffer);
				}

				SendMessage(hDlg, WM_CLOSE, 0, 0);
				break;
			} else {
				if (LOWORD(wParam) >= IDC_SUPPORTDIR_BR1 && LOWORD(wParam) <= IDC_SUPPORTDIR_BR25) {
					var = IDC_SUPPORTDIR_EDIT1 + LOWORD(wParam) - IDC_SUPPORTDIR_BR1;

					TCHAR szPath[MAX_PATH] = { 0 };
					GetDlgItemText(hDlg, var, szPath, sizeof(szPath));
					ConvertToAbsolutePath(szPath, szAbsolutePath);
				} else {
					if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDCANCEL) {
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					else
					if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDDEFAULT) {
						if (IDOK == MessageBox(
							hDlg,
							FBALoadStringEx(hAppInst, IDS_EDIT_DEFAULTS, true),
							FBALoadStringEx(hAppInst, IDS_ERR_WARNING, true),
							MB_OKCANCEL | MB_ICONEXCLAMATION | MB_DEFBUTTON2)
							) {
							for (INT32 x = 0; x < 25; x++) {
								if (lstrcmp(pszSupportPath[x][0], pszSupportPath[x][1])) {
									_tcscpy(pszSupportPath[x][0], pszSupportPath[x][1]);
									SetDlgItemText(hDlg, IDC_SUPPORTDIR_EDIT1 + x, pszSupportPath[x][0]);
								}
							}
							UpdateWindow(hDlg);
						}
					}
					break;
				}
			}

		    SHGetMalloc(&pMalloc);

			memset(&bInfo, 0, sizeof(bInfo));
			bInfo.hwndOwner      = hDlg;
			bInfo.pszDisplayName = buffer;
			bInfo.lpszTitle      = FBALoadStringEx(hAppInst, IDS_SELECT_DIR, true);
			bInfo.ulFlags        = BIF_EDITBOX | BIF_RETURNONLYFSDIRS;
			if (S_OK == nCOMInit) {
				bInfo.ulFlags   |= BIF_NEWDIALOGSTYLE;	// Caller needs to call CoInitialize() / OleInitialize() before using API (main.cpp)
			}
			bInfo.lpfn           = BRProc;
			bInfo.lParam         = (LPARAM)szAbsolutePath;

			pItemIDList = SHBrowseForFolder(&bInfo);

			if (pItemIDList) {
				if (SHGetPathFromIDList(pItemIDList, buffer)) {
					int strLen = _tcslen(buffer);
					if (strLen) {
						if (buffer[strLen - 1] != _T('\\')) {
							buffer[strLen]      = _T('\\');
							buffer[strLen + 1]  = _T('\0');
						}
						SetDlgItemText(hDlg, var, buffer);
					}
				}
				pMalloc->Free(pItemIDList);
			}
			pMalloc->Release();
			break;
		}
		case WM_CLOSE: {
			RECT rect;
			GetClientRect(hDlg, &rect);
			nSupportDlgWidth  = rect.right;
			nSupportDlgHeight = rect.bottom;

			if (NULL != hFont) {
				DeleteObject(hFont);         hFont = NULL;
			}
			if (NULL != hWhiteBGBrush) {
				DeleteObject(hWhiteBGBrush); hWhiteBGBrush = NULL;
			}
			EndDialog(hDlg, 0);
			// If Icons directory path has been changed do the proper action
			if(_tcscmp(szCheckIconsPath, szAppIconsPath)) {
				IconsDirPathChanged();
			}
			hParent     = NULL;
			hSupportDlg = NULL;
			break;
		}
	}

	return 0;
}

INT32 SupportDirCreate(HWND hParentWND)
{
	hParent = hParentWND;

	_stprintf(szCheckIconsPath, szAppIconsPath);

	return FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_SUPPORTDIR), hParentWND, (DLGPROC)DefInpProc);
}
/*
int SupportDirCreateTab(int nTab, HWND hParentWND)
{
	nInitTabSelect = nTab - IDC_SUPPORTDIR_EDIT1;

	hParent = hParentWND;

	_stprintf(szCheckIconsPath, szAppIconsPath);

	return FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_SUPPORTDIR), hParentWND, (DLGPROC)DefInpProc);
}
*/