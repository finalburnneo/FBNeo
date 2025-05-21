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
	{ _T("roms/snes/")		},
	{ _T("roms/fds/")		},
	{ _T("roms/nes/")		},
	{ _T("roms/ngp/")		},
	{ _T("roms/channelf/")	},
	{ _T("roms/romdata/")	},
	{ _T("")				},
	{ _T("")				}
};

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
			chOk = false;

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

			for(int x = 0; x < 20; x++) {
				ShowWindow(GetDlgItem(hDlg, IDC_ROMSDIR_BR1   + x), SW_SHOW);	// browse buttons
				ShowWindow(GetDlgItem(hDlg, IDC_ROMSDIR_EDIT1 + x), SW_SHOW);	// edit controls
				ShowWindow(GetDlgItem(hDlg, IDC_ROMSDIR_TEXT1 + x), SW_SHOW);	// text controls
			}

			UpdateWindow(hDlg);

			WndInMid(hDlg, hParent);
			SetFocus(hDlg);														// Enable Esc=close
			break;
		}
		case WM_CTLCOLORSTATIC: {
			HDC hDc = (HDC)wParam;
			SetBkMode(hDc, TRANSPARENT);										// LTEXT control with transparent background
			return (LRESULT)GetStockObject(HOLLOW_BRUSH);						// Return to empty brush
			for (int x = 0; x < 20; x++) {
				if ((HWND)lParam == GetDlgItem(hDlg, IDC_ROMSDIR_EDIT1 + x)) return (INT_PTR)hWhiteBGBrush;
				if ((HWND)lParam == GetDlgItem(hDlg, IDC_ROMSDIR_TEXT1 + x)) return (LRESULT)GetStockObject(HOLLOW_BRUSH);
			}
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
					int strLen = _tcslen(buffer);
					if (strLen) {
						if (buffer[strLen - 1] != _T('\\') && buffer[strLen - 1] != _T('/') ) {
							buffer[strLen + 0]  = _T('\\');
							buffer[strLen + 1]  = _T('\0');
						}
					}
					lstrcpy(szAppRomPaths[i], buffer);
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
					if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_ROMSDIR_DEFAULTS) {
						if (IDOK == MessageBox(
							hDlg,
							FBALoadStringEx(hAppInst, IDS_ROMS_EDIT_DEFAULTS, true),
							FBALoadStringEx(hAppInst, IDS_ERR_WARNING,        true),
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
			bInfo.lpszTitle      = FBALoadStringEx(hAppInst, IDS_ROMS_SELECT_DIR, true);
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
							buffer[strLen] = _T('\\');
							buffer[strLen + 1] = _T('\0');
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
			hParent = NULL;
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
		}
	}

	return 0;
}


int RomsDirCreate(HWND hParentWND)
{
	hParent = hParentWND;

	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_ROMSDIR), hParent, (DLGPROC)DefInpProc);
	return 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//Check Romsets Dialog/////////////////////////////////////////////////////////////////////////////

int WriteGameAvb()
{
	TCHAR szRomDat[MAX_PATH];
	FILE* h;

	CreateRomDatName(szRomDat);

	if ((h = _tfopen(szRomDat, _T("wt"))) == NULL) {
		return 1;
	}

	_ftprintf(h, _T(APP_TITLE) _T(" v%.20s ROMs"), szAppBurnVer);	// identifier
	_ftprintf(h, _T(" 0x%04X "), nBurnDrvCount);					// no of games

	for (unsigned int i = 0; i < nBurnDrvCount; i++) {
		if (gameAv[i] & 2) {
			_fputtc(_T('*'), h);
		} else {
			if (gameAv[i] & 1) {
				_fputtc(_T('+'), h);
			} else {
				_fputtc(_T('-'), h);
			}
		}
	}

	_ftprintf(h, _T(" END"));									// end marker

	fclose(h);

	return 0;
}

static int DoCheck(TCHAR* buffPos)
{
	TCHAR label[256];

	// Check identifier
	memset(label, 0, sizeof(label));
	_stprintf(label, _T(APP_TITLE) _T(" v%.20s ROMs"), szAppBurnVer);
	if ((buffPos = LabelCheck(buffPos, label)) == NULL) {
		return 1;
	}

	// Check no of supported games
	memset(label, 0, sizeof(label));
	memcpy(label, buffPos, 16);
	buffPos += 8;
	unsigned int n = _tcstol(label, NULL, 0);
	if (n != nBurnDrvCount) {
		return 1;
	}

	for (unsigned int i = 0; i < nBurnDrvCount; i++) {
		if (*buffPos == _T('*')) {
			gameAv[i] = 3;
		} else {
			if (*buffPos == _T('+')) {
				gameAv[i] = 1;
			} else {
				if (*buffPos == _T('-')) {
					gameAv[i] = 0;
				} else {
					return 1;
				}
			}
		}

		buffPos++;
	}

	memset(label, 0, sizeof(label));
	_stprintf(label, _T(" END"));
	if (LabelCheck(buffPos, label) == NULL) {
		avOk = true;
		return 0;
	} else {
		return 1;
	}
}

int CheckGameAvb()
{
	TCHAR szRomDat[MAX_PATH];
	FILE* h;
	int bOK;
	int nBufferSize = nBurnDrvCount + 256;
	TCHAR* buffer = (TCHAR*)malloc(nBufferSize * sizeof(TCHAR));
	if (buffer == NULL) {
		return 1;
	}

	memset(buffer, 0, nBufferSize * sizeof(TCHAR));
	CreateRomDatName(szRomDat);

	if ((h = _tfopen(szRomDat, _T("r"))) == NULL) {
		if (buffer)
		{
			free(buffer);
			buffer = NULL;
		}
		return 1;
	}

	_fgetts(buffer, nBufferSize, h);
	fclose(h);

	bOK = DoCheck(buffer);

	if (buffer) {
		free(buffer);
		buffer = NULL;
	}
	return bOK;
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
			TerminateThread(hScanThread, 1);
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
	bRescanRoms = false;

	if (avOk) {
		WriteGameAvb();
	}

	return 1;
}

static unsigned __stdcall AnalyzingRoms(void*)
{
	for (unsigned int z = 0; z < nBurnDrvCount; z++) {
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
			memset(gameAv, 0, nBurnDrvCount); // clear game list
			SendDlgItemMessage(hDlg, IDC_WAIT_PROG, PBM_SETRANGE, 0, MAKELPARAM(0, nBurnDrvCount));
			SendDlgItemMessage(hDlg, IDC_WAIT_PROG, PBM_SETSTEP, (WPARAM)1, 0);

			ShowWindow(GetDlgItem(hDlg, IDC_WAIT_LABEL_A), TRUE);
			SendMessage(GetDlgItem(hDlg, IDC_WAIT_LABEL_A), WM_SETTEXT, (WPARAM)0, (LPARAM)FBALoadStringEx(hAppInst, IDS_SCANNING_ROMS, true));
			ShowWindow(GetDlgItem(hDlg, IDCANCEL), TRUE);

			avOk = false;
			hScanThread = (HANDLE)_beginthreadex(NULL, 0, AnalyzingRoms, NULL, 0, &ScanThreadId);

			hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

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

int CreateROMInfo(HWND hParentWND)
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
		if (CheckGameAvb() || bRescanRoms) {
			if ((bStarting && bSkipStartupCheck == false) || bRescanRoms)
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
}
