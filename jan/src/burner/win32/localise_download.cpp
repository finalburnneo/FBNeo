#include "burner.h"
#include <wininet.h>
#include <shlobj.h>

#define MAX_LANGUAGES		64

static HWND hLocalDownDlg	= NULL;
static HWND hParent			= NULL;

static int langCodes[MAX_LANGUAGES];

static void LocaliseDownloadExit()
{
	hParent = NULL;
	
	memset(langCodes, 0, MAX_LANGUAGES * sizeof(int));
	
	EndDialog(hLocalDownDlg, 0);
}

static INT32 LocaliseDownloadInit()
{
	HINTERNET connect = InternetOpen(_T("MyBrowser"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	
	if (!connect) {
		MessageBox(hLocalDownDlg, FBALoadStringEx(hAppInst, IDS_ERR_LOCAL_FAIL_CONNECT, true), FBALoadStringEx(hAppInst, IDS_ERR_ERROR, true), MB_OK);
		return 1;
	}
   
	HINTERNET OpenAddress = InternetOpenUrl(connect, _T("https://www.fbalpha.com/localisationinfo/"), NULL, 0, INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_KEEP_CONNECTION, 0);
	if (!OpenAddress) {
		MessageBox(hLocalDownDlg, FBALoadStringEx(hAppInst, IDS_ERR_LOCAL_FAIL_OPEN_URL, true), FBALoadStringEx(hAppInst, IDS_ERR_ERROR, true), MB_OK);
		InternetCloseHandle(connect);
		return 1;
	}
   
	char DataReceived[4096];
	DWORD NumberOfBytesRead = 0;
	InternetReadFile(OpenAddress, DataReceived, 4096, &NumberOfBytesRead);
	
	DataReceived[NumberOfBytesRead] = '\0';
	
	char *Tokens;
	int i = 0;
	int j = 0;
	Tokens = strtok(DataReceived, ":^");
	while (Tokens != NULL) {
		if (i & 1) {
			SendDlgItemMessage(hLocalDownDlg, IDC_CHOOSE_LIST, CB_ADDSTRING, 0, (LPARAM)ANSIToTCHAR(Tokens, NULL, 0));
		} else {
			langCodes[j] = atoi(Tokens);
			j++;
		}
		
		i++;
		Tokens = strtok(NULL, ":^");
	}
	
	SendDlgItemMessage(hLocalDownDlg, IDC_CHOOSE_LIST, CB_SETCURSEL, (WPARAM)0, (LPARAM)0);
	
	InternetCloseHandle(OpenAddress);
	InternetCloseHandle(connect);
	
	return 0;
}

static int LocaliseDownloadOkay()
{
	TCHAR szTitle[256];
	TCHAR szFilter[256];
	
	int nIndex = SendMessage(GetDlgItem(hLocalDownDlg, IDC_CHOOSE_LIST), CB_GETCURSEL, 0, 0);
	SendMessage(GetDlgItem(hLocalDownDlg, IDC_CHOOSE_LIST), CB_GETLBTEXT, (WPARAM)nIndex, (LPARAM)szChoice);
	
	_sntprintf(szTitle, 256, FBALoadStringEx(hAppInst, IDS_LOCAL_GL_SELECT, true));

	_stprintf(szFilter, FBALoadStringEx(hAppInst, IDS_LOCAL_GL_FILTER, true));
	memcpy(szFilter + _tcslen(szFilter), _T(" (*.flt)\0*.flt\0\0"), 16 * sizeof(TCHAR));
	
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hScrnWnd;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = szChoice;
	ofn.nMaxFile = sizeof(szChoice) / sizeof(TCHAR);
	ofn.lpstrInitialDir = _T(".");
	ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = _T("dat");
	ofn.lpstrTitle = szTitle;
	ofn.Flags |= OFN_OVERWRITEPROMPT;

	if (GetSaveFileName(&ofn) == 0)	return 1;
	
	FILE *fFlt = NULL;
	
	if ((fFlt = _tfopen(szChoice, _T("wb"))) == 0) return 1;
	
	HINTERNET connect = InternetOpen(_T("MyBrowser"), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	
	if (!connect) {
		MessageBox(hLocalDownDlg, FBALoadStringEx(hAppInst, IDS_ERR_LOCAL_FAIL_CONNECT, true), FBALoadStringEx(hAppInst, IDS_ERR_ERROR, true), MB_OK);
		return 1;
	}
	
	TCHAR url[256];
	_sntprintf(url, 256, _T("https://www.fbalpha.com/localisation/downloadtemplates/%i/"), langCodes[nIndex]);
   
	HINTERNET OpenAddress = InternetOpenUrl(connect, url, NULL, 0, INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_KEEP_CONNECTION, 0);
	if (!OpenAddress) {
		MessageBox(hLocalDownDlg, FBALoadStringEx(hAppInst, IDS_ERR_LOCAL_FAIL_OPEN_URL, true), FBALoadStringEx(hAppInst, IDS_ERR_ERROR, true), MB_OK);
		InternetCloseHandle(connect);
		return 1;
	}
   
	char DataReceived[4096];
	DWORD NumberOfBytesRead = 0;
	while (InternetReadFile(OpenAddress, DataReceived, 4096, &NumberOfBytesRead) && NumberOfBytesRead) {
		fwrite(DataReceived, sizeof(char), NumberOfBytesRead, fFlt);
	}
	
	InternetCloseHandle(OpenAddress);
	InternetCloseHandle(connect);

	fclose(fFlt);
	
	FBALocaliseInit(szChoice);
	POST_INITIALISE_MESSAGE;
	
	return 0;
}

static INT_PTR CALLBACK DefInpProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM /*lParam*/)
{
	switch (Msg) {
		case WM_INITDIALOG: {
			hLocalDownDlg = hDlg;
			
			if (LocaliseDownloadInit() == 1) SendMessage(hDlg, WM_CLOSE, 0, 0);

			WndInMid(hDlg, hScrnWnd);
			SetFocus(hDlg);											// Enable Esc=close
			break;
		}
		
		case WM_COMMAND: {
			int wID = LOWORD(wParam);
			int Notify = HIWORD(wParam);
						
			if (Notify == BN_CLICKED) {
				switch (wID) {
					case IDOK: {
						LocaliseDownloadOkay();
						SendMessage(hDlg, WM_CLOSE, 0, 0);
						break;
					}
				
					case IDCANCEL: {
						SendMessage(hDlg, WM_CLOSE, 0, 0);
						return 0;
					}
				}
			}
			
			break;
		}
		
		case WM_CLOSE: {
			LocaliseDownloadExit();
			break;
		}
	}

	return 0;
}

int LocaliseDownloadCreate(HWND hParentWND)
{
	hParent = hParentWND;
	
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_DOWNLOAD_LOCAL), hParent, (DLGPROC)DefInpProc);
	return 1;
}
