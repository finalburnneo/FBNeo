#include "burner.h"
#include <shlobj.h>
#include <process.h>

static HWND hTabControl = NULL;
static HWND hParent     = NULL;

HWND hRomsDlg = NULL;

char* gameAv = NULL;
bool avOk    = false;

bool bSkipStartupCheck = false;

static UINT32 ScanThreadId = 0;
static HANDLE hScanThread  = NULL;
static INT32 nOldSelect    = 0;

static HANDLE hEvent = NULL;

static const TCHAR szAppDefaultPaths[DIRS_MAX][MAX_PATH] = {
	{ _T("roms/")			},
	{ _T("roms/arcade/")	},
	{ _T("roms/megadrive/")	},
	{ _T("roms/pce/")		},
	{ _T("roms/sgx/")		},
	{ _T("roms/tg16/")		},
	{ _T("roms/coleco/")	},
	{ _T("roms/sg1000/")	},
	{ _T("roms/gamegear/")	},
	{ _T("roms/sms/")		},
	{ _T("roms/msx/")		},
	{ _T("roms/spectrum/")	},
	{ _T("roms/nes/")		},
	{ _T("roms/fds/")		},
	{ _T("roms/snes/")		},
	{ _T("roms/gba/")		},
	{ _T("roms/ngp/")		},
	{ _T("roms/channelf/")	},
	{ _T("roms/astrocade/")	},
	{ _T("roms/romdata/")	},
	{ _T("")				}
};

INT32 nRomsDlgWidth  = 0x1d2;
INT32 nRomsDlgHeight = 0x291;
static INT32 nDlgCaptionHeight;
static INT32 nDlgInitialWidth;
static INT32 nDlgInitialHeight;
static INT32 nDlgTabCtrlInitialPos[4];
static INT32 nDlgGroupCtrlInitialPos[4];
static INT32 nDlgOKBtnInitialPos[4];
static INT32 nDlgCancelBtnInitialPos[4];
static INT32 nDlgDefaultsBtnInitialPos[4];
static INT32 nDlgTextCtrlInitialPos[20][4];
static INT32 nDlgEditCtrlInitialPos[20][4];
static INT32 nDlgBtnCtrlInitialPos[20][4];

// Dialog sizing support functions and macros (everything working in client co-ords)
#define GetInititalControlPos(a, b)								\
	GetWindowRect(GetDlgItem(hRomsDlg, a), &rect);				\
	memset(&point, 0, sizeof(POINT));							\
	point.x = rect.left;										\
	point.y = rect.top;											\
	ScreenToClient(hRomsDlg, &point);							\
	b[0] = point.x;												\
	b[1] = point.y;												\
	GetClientRect(GetDlgItem(hRomsDlg, a), &rect);				\
	b[2] = rect.right;											\
	b[3] = rect.bottom;

#define SetControlPosAlignTopLeft(a, b)							\
	SetWindowPos(GetDlgItem(hRomsDlg, a), hRomsDlg, b[0], b[1], 0, 0, SWP_NOZORDER | SWP_NOSIZE);

#define SetControlPosAlignTopLeftResizeHor(a, b)				\
	SetWindowPos(GetDlgItem(hRomsDlg, a), hRomsDlg, b[0], b[1], b[2] + 4 - xDelta, b[3] + 4, SWP_NOZORDER);

#define SetControlPosAlignTopLeftResizeHorVert(a, b)			\
	SetWindowPos(GetDlgItem(hRomsDlg, a), hRomsDlg, b[0], b[1], b[2] - xDelta, b[3] - yDelta, SWP_NOZORDER);

#define SetControlPosAlignTopRight(a, b)						\
	SetWindowPos(GetDlgItem(hRomsDlg, a), hRomsDlg, b[0] - xDelta, b[1], 0, 0, SWP_NOZORDER | SWP_NOSIZE);

#define SetControlPosAlignBottomLeft(a, b)						\
	SetWindowPos(GetDlgItem(hRomsDlg, a), hRomsDlg, b[0], b[1] - yDelta, b[2], b[3], SWP_NOZORDER);

#define SetControlPosAlignBottomRight(a, b)						\
	SetWindowPos(GetDlgItem(hRomsDlg, a), hRomsDlg, b[0] - xDelta, b[1] - yDelta, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

static INT32 GetCaptionHight()
{
	RECT rect;

	GetWindowRect(hRomsDlg, &rect);
	const INT32 nWndHight = rect.bottom - rect.top;

	GetClientRect(hRomsDlg, &rect);

	return nWndHight - (rect.bottom - rect.top);
}

static void GetInitialPositions()
{
	RECT rect;
	POINT point;

	nDlgCaptionHeight = GetCaptionHight();

	GetClientRect(hRomsDlg, &rect);
	nDlgInitialWidth  = rect.right;
	nDlgInitialHeight = rect.bottom;

	GetInititalControlPos(IDC_ROMPATH_TAB, nDlgTabCtrlInitialPos);
	GetInititalControlPos(IDC_STATIC1,     nDlgGroupCtrlInitialPos);
	GetInititalControlPos(IDOK,            nDlgOKBtnInitialPos);
	GetInititalControlPos(IDCANCEL,        nDlgCancelBtnInitialPos);
	GetInititalControlPos(IDDEFAULT,       nDlgDefaultsBtnInitialPos);

	for (INT32 i = 0; i < 20; i++) {
		GetInititalControlPos(IDC_ROMSDIR_TEXT1 + i, nDlgTextCtrlInitialPos[i]);
		GetInititalControlPos(IDC_ROMSDIR_EDIT1 + i, nDlgEditCtrlInitialPos[i]);
		GetInititalControlPos(IDC_ROMSDIR_BR1   + i, nDlgBtnCtrlInitialPos[i]);
	}
}

static void CreateRomDatName(TCHAR* szRomDat)
{
	_stprintf(szRomDat, _T("config/%s.roms.dat"), szAppExeName);

	return;
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

//Select Directory Dialog//////////////////////////////////////////////////////////////////////////

static INT_PTR CALLBACK DefInpProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HFONT  hFont         = NULL;
	HBRUSH hWhiteBGBrush = NULL;
	TCHAR szAbsolutePath[MAX_PATH] = {0};

	INT32 var;
	static bool chOk;

	switch (Msg) {
		case WM_INITDIALOG: {
			hRomsDlg = hDlg;
			chOk = false;

			// add WS_MAXIMIZEBOX button;
			SetWindowLongPtr(hDlg, GWL_STYLE, GetWindowLongPtr(hDlg, GWL_STYLE) | WS_MAXIMIZEBOX);

			HICON hIcon = LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_APP));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);

			hWhiteBGBrush = CreateSolidBrush(RGB(0xff, 0xff, 0xff));

			TCHAR szDialogTitle[200];
			_stprintf(szDialogTitle, FBALoadStringEx(hAppInst, IDS_ROMS_EDIT_PATHS, true), _T(APP_TITLE), _T(SEPERATOR_1));
			SetWindowText(hDlg, szDialogTitle);

			// Setup edit controls values (ROMs Paths)
			for (INT32 x = 0; x < 20; x++) {
				SetDlgItemText(hDlg, IDC_ROMSDIR_EDIT1 + x, szAppRomPaths[x]);
			}

			// Setup the tabs
			hTabControl = GetDlgItem(hDlg, IDC_ROMPATH_TAB);
			TC_ITEM tcItem = { 0 };
			tcItem.mask = TCIF_TEXT;
			tcItem.pszText = FBALoadStringEx(hAppInst, IDS_ROMS_PATHS, true);
			TabCtrl_InsertItem(hTabControl, 0, &tcItem);

			// Font
			LOGFONT lf = { 0 };
			SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(LOGFONT), &lf, 0);
			lf.lfHeight = -MulDiv(9, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);
			lf.lfWeight = FW_BOLD;
			hFont = CreateFontIndirect(&lf);
			SendMessage(hTabControl, WM_SETFONT, (WPARAM)hFont, TRUE);

			GetInitialPositions();
			SetWindowPos(hDlg, NULL, 0, 0, nRomsDlgWidth, nRomsDlgHeight, SWP_NOZORDER);

			UpdateWindow(hDlg);
			WndInMid(hDlg, hParent);
			SetFocus(hDlg);														// Enable Esc=close
			break;
		}
		case WM_CTLCOLORSTATIC: {
			HDC hDc = (HDC)wParam;
			for (int x = 0; x < 20; x++) {
				if ((HWND)lParam == GetDlgItem(hDlg, IDC_ROMSDIR_TEXT1 + x)) {
					SetBkMode(hDc, TRANSPARENT);								// LTEXT control with transparent background
					return (LRESULT)GetStockObject(HOLLOW_BRUSH);				// Return to empty brush
				}
			}
			break;
		}
		case WM_GETMINMAXINFO: {
			MINMAXINFO *info = (MINMAXINFO*)lParam;
			info->ptMinTrackSize.x = nDlgInitialWidth;
			info->ptMinTrackSize.y = nDlgInitialHeight + nDlgCaptionHeight;
			return 0;
		}
		case WM_SIZE: {
			if (nDlgInitialWidth == 0 || nDlgInitialHeight == 0) return 0;

			RECT rc;
			GetClientRect(hDlg, &rc);

			const INT32 xDelta = nDlgInitialWidth  - rc.right;
			const INT32 yDelta = nDlgInitialHeight - rc.bottom;
			if (xDelta == 0 && yDelta == 0) return 0;

			SetControlPosAlignTopLeftResizeHorVert(IDC_ROMPATH_TAB, nDlgTabCtrlInitialPos);
			SetControlPosAlignTopLeftResizeHorVert(IDC_STATIC1,     nDlgGroupCtrlInitialPos);

			SetControlPosAlignBottomRight(IDOK,     nDlgOKBtnInitialPos);
			SetControlPosAlignBottomRight(IDCANCEL, nDlgCancelBtnInitialPos);

			SetControlPosAlignBottomLeft(IDDEFAULT, nDlgDefaultsBtnInitialPos);

			for (INT32 i = 0; i < 20; i++) {
				SetControlPosAlignTopLeft(IDC_ROMSDIR_TEXT1          + i, nDlgTextCtrlInitialPos[i]);
				SetControlPosAlignTopLeftResizeHor(IDC_ROMSDIR_EDIT1 + i, nDlgEditCtrlInitialPos[i]);
				SetControlPosAlignTopRight(IDC_ROMSDIR_BR1           + i, nDlgBtnCtrlInitialPos[i]);
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
				for (int i = 0; i < 20; i++) {
//					if (GetDlgItemText(hDlg, IDC_ROMSDIR_EDIT1 + i, buffer, sizeof(buffer)) && lstrcmp(szAppRomPaths[i], buffer)) {
					GetDlgItemText(hDlg, IDC_ROMSDIR_EDIT1 + i, buffer, sizeof(buffer));
					if (lstrcmp(szAppRomPaths[i], buffer)) chOk = true;

					// add trailing backslash
					INT32 strLen = _tcslen(buffer);
					if (strLen) {
						if (buffer[strLen - 1] != _T('\\') && buffer[strLen - 1] != _T('/') ) {
							buffer[strLen + 0]  = _T('\\');
							buffer[strLen + 1]  = _T('\0');
						}
					}
					_tcscpy(szAppRomPaths[i], buffer);
//					}
				}

				SendMessage(hDlg, WM_CLOSE, 0, 0);
				break;
			} else {
				if (LOWORD(wParam) >= IDC_ROMSDIR_BR1 && LOWORD(wParam) <= IDC_ROMSDIR_BR20) {
					var = IDC_ROMSDIR_EDIT1 + LOWORD(wParam) - IDC_ROMSDIR_BR1;

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
							FBALoadStringEx(hAppInst, IDS_ERR_WARNING,   true),
							MB_OKCANCEL | MB_ICONEXCLAMATION | MB_DEFBUTTON2)
						) {
							for (INT32 x = 0; x < 20; x++) {
								if (lstrcmp(szAppRomPaths[x], szAppDefaultPaths[x])) chOk = true;
								_tcscpy(szAppRomPaths[x], szAppDefaultPaths[x]);
								SetDlgItemText(hDlg, IDC_ROMSDIR_EDIT1 + x, szAppRomPaths[x]);
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

			GetWindowRect(hDlg, &rect);
			nRomsDlgWidth  = rect.right - rect.left;
			nRomsDlgHeight = rect.bottom - rect.top;

			EndDialog(hDlg, 0);
			if (chOk && bSkipStartupCheck == false) {
				bRescanRoms = true;
				CreateROMInfo(hDlg);
			}
			if (NULL != hFont) {
				DeleteObject(hFont);         hFont         = NULL;
			}
			if (NULL != hWhiteBGBrush) {
				DeleteObject(hWhiteBGBrush); hWhiteBGBrush = NULL;
			}
			hParent  = NULL;
			hRomsDlg = NULL;
		}
	}

	return 0;
}


INT32 RomsDirCreate(HWND hParentWND)
{
	hParent = hParentWND;

	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_ROMSDIR), hParent, (DLGPROC)DefInpProc);
	return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Check Romsets Dialog/////////////////////////////////////////////////////////////////////////////

// super ROM scan database, v2  @  config/<exe>.roms.dat
// super-caching function: on startup, only newly added drivers are scanned!
//
// per-line format:  <drivername> <status>
//   status: '*' = all roms ok
//           '+' = playable (but possible missing optional/useles roms)
//           '-' = missing
// example, "Ghosts'n Goblins", all roms ok:
// gng *

#define ROMDAT_HEADER	"FBNeo ROMs v2"

static char*   GameAvScanned = NULL;	// 1 = gameAv[i] holds a real scan result
static UINT32* ScanList      = NULL;	// drivers the scan thread will check
static UINT32  nScanCount    = 0;		// number of entries in ScanList

static void FreeScanArrays()
{
	if (GameAvScanned) { free(GameAvScanned); GameAvScanned = NULL; }
	if (ScanList)      { free(ScanList);      ScanList      = NULL; }
	nScanCount = 0;
}

static bool AllocScanArrays()
{
	FreeScanArrays();

	GameAvScanned = (char*)malloc(nBurnDrvCount);
	ScanList      = (UINT32*)malloc(nBurnDrvCount * sizeof(UINT32));

	if (GameAvScanned == NULL || ScanList == NULL) {
		FreeScanArrays();
		return false;
	}

	memset(GameAvScanned, 0, nBurnDrvCount);

	return true;
}

// one parsed line of the cache file
struct RomDatEntry {
	char   szName[64];
	char   nStatus;
};

static int RomDatEntryCmp(const void* a, const void* b)
{
	return strcmp(((const RomDatEntry*)a)->szName, ((const RomDatEntry*)b)->szName);
}

static char StatusToChar(char nStatus)
{
	if (nStatus & 2) return '*';
	if (nStatus & 1) return '+';
	return '-';
}

static bool CharToStatus(char c, char* pnStatus)
{
	switch (c) {
		case '*': *pnStatus = 3; return true;
		case '+': *pnStatus = 1; return true;
		case '-': *pnStatus = 0; return true;
	}
	return false;
}

INT32 WriteGameAvb()
{
	if (gameAv == NULL || GameAvScanned == NULL) {
		return 1;
	}

	TCHAR szRomDat[MAX_PATH];
	TCHAR szTmpDat[MAX_PATH + 8];
	FILE* h;

	CreateRomDatName(szRomDat);
	_stprintf(szTmpDat, _T("%s.tmp"), szRomDat);

	// write to a temp file and swap it in, so a crash mid-write can't leave a truncated cache
	if ((h = _tfopen(szTmpDat, _T("wt"))) == NULL) {
		return 1;
	}

	fprintf(h, "%s\n", ROMDAT_HEADER);

	UINT32 nOldDrvSel = nBurnDrvActive;
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		if (!GameAvScanned[i]) continue;
		nBurnDrvActive = i;
		fprintf(h, "%s %c\n", BurnDrvGetTextA(DRV_NAME), StatusToChar(gameAv[i]));
	}
	nBurnDrvActive = nOldDrvSel;

	fprintf(h, "END\n");

	bool bOK = (ferror(h) == 0);
	if (fclose(h) != 0) bOK = false;

	if (!bOK || !MoveFileEx(szTmpDat, szRomDat, MOVEFILE_REPLACE_EXISTING)) {
		DeleteFile(szTmpDat);
		return 1;
	}

	return 0;
}

// Mark a game as scanned (e.g. after a single-set rescan, gameinfo dialog)
void MarkGameAvScanned(UINT32 nDrv)
{
	if (GameAvScanned && nDrv < nBurnDrvCount) {
		GameAvScanned[nDrv] = 1;
	}
}

// Fill gameAv[] from the cache file. Drivers not found in it are added to ScanList. Returns the number of drivers to scan.
static INT32 LoadGameAvb()
{
	RomDatEntry* pCache = NULL;
	UINT32 nCacheCount = 0;
	UINT32 nCacheAlloc = 0;

	if (!AllocScanArrays()) {
		return 0;
	}

	TCHAR szRomDat[MAX_PATH];
	FILE* h;

	CreateRomDatName(szRomDat);

	if ((h = _tfopen(szRomDat, _T("rt"))) != NULL) {
		char szLine[512];

		// an old-format (v1) or unknown file simply yields an empty cache -> full scan
		if (fgets(szLine, sizeof(szLine), h) && strncmp(szLine, ROMDAT_HEADER, strlen(ROMDAT_HEADER)) == 0) {
			while (fgets(szLine, sizeof(szLine), h)) {
				RomDatEntry e;
				char cStatus;

				memset(&e, 0, sizeof(e));
				if (sscanf(szLine, "%63s %c", e.szName, &cStatus) != 2 || !CharToStatus(cStatus, &e.nStatus)) {
					continue;
				}

				if (nCacheCount == nCacheAlloc) {
					UINT32 nNewAlloc = nCacheAlloc ? nCacheAlloc * 2 : 4096;
					RomDatEntry* pNew = (RomDatEntry*)realloc(pCache, nNewAlloc * sizeof(RomDatEntry));
					if (pNew == NULL) break; // out of memory: just use what we have now, or probably crash later.  who knows?
					pCache = pNew;
					nCacheAlloc = nNewAlloc;
				}
				pCache[nCacheCount++] = e;
			}
		}

		fclose(h);
	}

	// achieve linearity through qsort-ing
	if (nCacheCount > 1) {
		qsort(pCache, nCacheCount, sizeof(RomDatEntry), RomDatEntryCmp);
	}

	nScanCount = 0;

	UINT32 nOldDrvSel = nBurnDrvActive;
	for (UINT32 i = 0; i < nBurnDrvCount; i++) {
		nBurnDrvActive = i;
		RomDatEntry* pFound = NULL;
		if (nCacheCount) {
			RomDatEntry key;
			memset(&key, 0, sizeof(key));
			strncpy(key.szName, BurnDrvGetTextA(DRV_NAME), sizeof(key.szName) - 1);
			pFound = (RomDatEntry*)bsearch(&key, pCache, nCacheCount, sizeof(RomDatEntry), RomDatEntryCmp);
		}

		if (pFound) {
			gameAv[i] = pFound->nStatus;
			GameAvScanned[i] = 1;
		} else {
			gameAv[i] = 0;
			ScanList[nScanCount++] = i;
		}
	}
	nBurnDrvActive = nOldDrvSel;

	if (pCache) {
		free(pCache);
	}

	avOk = (nScanCount == 0);

	return (INT32)nScanCount;
}

static int QuitRomsScan()
{
	DWORD dwExitCode;

	GetExitCodeThread(hScanThread, &dwExitCode);

	if (dwExitCode == STILL_ACTIVE) {

		// Signal the scan thread to abort
		SetEvent(hEvent);

		// Wait for the thread to finish
		if (WaitForSingleObject(hScanThread, 10000) != WAIT_OBJECT_0) {
			// If the thread doesn't finish within 10 seconds, forcibly kill it
			TerminateThread(hScanThread, 1); // if we do this, it's probably going to crash later. oh well..
		}

		CloseHandle(hScanThread);
	}

	CloseHandle(hEvent);

	hEvent = NULL;

	hScanThread = NULL;
	ScanThreadId = 0;

	BzipClose();

	nBurnDrvActive = nOldSelect;
	nOldSelect = 0;
	bRescanRoms  = false;

	nScanCount = 0;

	WriteGameAvb();

	return 1;
}

static unsigned __stdcall AnalyzingRoms(void*)
{
	for (UINT32 n = 0; n < nScanCount; n++) {
		UINT32 z = ScanList[n];
		nBurnDrvActive = z;

		// See if we need to abort
		if (WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0) {
			ExitThread(0);
		}

		SendDlgItemMessage(hRomsDlg, IDC_WAIT_PROG, PBM_STEPIT, 0, 0);

		switch (BzipOpen(TRUE))	{
			case 0:
				gameAv[z] = 3;
				break;
			case 2:
				gameAv[z] = 1;
				break;
			case 1:
				gameAv[z] = 0;
		}
		GameAvScanned[z] = 1;
		BzipClose();
	}

	avOk = true;

	PostMessage(hRomsDlg, WM_CLOSE, 0, 0);

	return 0;
}

//Add these two static variables
static int xClick;
static int yClick;

static INT_PTR CALLBACK WaitProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)		// LPARAM lParam
{
	switch (Msg) {
		case WM_INITDIALOG: {
			hRomsDlg = hDlg;
			nOldSelect = nBurnDrvActive;
			SendDlgItemMessage(hDlg, IDC_WAIT_PROG, PBM_SETRANGE, 0, MAKELPARAM(0, nScanCount));
			SendDlgItemMessage(hDlg, IDC_WAIT_PROG, PBM_SETSTEP, (WPARAM)1, 0);

			ShowWindow(GetDlgItem(hDlg, IDC_WAIT_LABEL_A), TRUE);
			SendMessage(GetDlgItem(hDlg, IDC_WAIT_LABEL_A), WM_SETTEXT, (WPARAM)0, (LPARAM)FBALoadStringEx(hAppInst, IDS_SCANNING_ROMS, true));
			ShowWindow(GetDlgItem(hDlg, IDCANCEL), TRUE);

			avOk = false;

			// create the abort event before the thread starts polling it
			hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			hScanThread = (HANDLE)_beginthreadex(NULL, 0, AnalyzingRoms, NULL, 0, &ScanThreadId);

			if (hParent == NULL) {
#if 0
				RECT rect;
				int x, y;

				SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);

				x = 315 + GetSystemMetrics(SM_CXDLGFRAME) * 2 + 6;
				y = 74 + GetSystemMetrics(SM_CYDLGFRAME) * 2 + 6;

				SetForegroundWindow(hDlg);
				SetWindowPos(hDlg, HWND_TOPMOST, (rect.right - rect.left) / 2 - x / 2, (rect.bottom - rect.top) / 2 - y / 2, x, y, 0);
				RedrawWindow(hDlg, NULL, NULL, 0);
				ShowWindow(hDlg, SW_SHOWNORMAL);
#endif
				WndInMid(hDlg, hParent);
				SetFocus(hDlg);		// Enable Esc=close
			} else {
				WndInMid(hDlg, hParent);
				SetFocus(hDlg);		// Enable Esc=close
			}

			break;
		}

		case WM_LBUTTONDOWN: {
			SetCapture(hDlg);

			xClick = GET_X_LPARAM(lParam);
			yClick = GET_Y_LPARAM(lParam);
			break;
		}

		case WM_LBUTTONUP: {
			ReleaseCapture();
			break;
		}

		case WM_MOUSEMOVE: {
			if (GetCapture() == hDlg) {
				RECT rcWindow;
				GetWindowRect(hDlg,&rcWindow);

				int xMouse = GET_X_LPARAM(lParam);
				int yMouse = GET_Y_LPARAM(lParam);
				int xWindow = rcWindow.left + xMouse - xClick;
				int yWindow = rcWindow.top + yMouse - yClick;

				SetWindowPos(hDlg,NULL,xWindow,yWindow,0,0,SWP_NOSIZE|SWP_NOZORDER);
			}
			break;
		}

		case WM_COMMAND: {
			if (LOWORD(wParam) == IDCANCEL) {
				PostMessage(hDlg, WM_CLOSE, 0, 0);
			}
			break;
		}

		case WM_CLOSE: {
			QuitRomsScan();
			EndDialog(hDlg, 0);
			hRomsDlg = NULL;
			hParent = NULL;
		}
	}

	return 0;
}

INT32 CreateROMInfo(HWND hParentWND)
{
	SubDirThreadExit();

	hParent = hParentWND;
	bool bStarting = 0;

	if (gameAv == NULL) {
		gameAv = (char*)malloc(nBurnDrvCount);
		memset(gameAv, 0, nBurnDrvCount);
		bStarting = 1;
	}

	if (gameAv) {
		// always load: this also sets up the arrays WriteGameAvb() needs
		INT32 nNeedScan = LoadGameAvb();

		if (bRescanRoms && ScanList != NULL) {
			// full rescan requested (menu, or rom paths changed): the files on disk
			// may have changed, so cached results can't be trusted
			for (UINT32 i = 0; i < nBurnDrvCount; i++) {
				ScanList[i] = i;
			}
			nScanCount = nBurnDrvCount;
			memset(gameAv, 0, nBurnDrvCount);
			memset(GameAvScanned, 0, nBurnDrvCount);
			avOk = false;
			nNeedScan = nBurnDrvCount;
		}

		if (nNeedScan > 0 && ((bStarting && bSkipStartupCheck == false) || bRescanRoms)) {
			FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_WAIT), hParent, (DLGPROC)WaitProc);
		}
	}

	return 1;
}

void FreeROMInfo()
{
	if (gameAv) {
		free(gameAv);
		gameAv = NULL;
	}

	FreeScanArrays();
}
