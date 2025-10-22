// ----------------------------------------------------------------------------------------------------------
// NEOCDSEL.CPP
#include "burner.h"
#include "neocdlist.h"
#include <process.h>

int				NeoCDList_Init();
static void		NeoCDList_InitListView();
static int		NeoCDList_AddGame(TCHAR* pszFile, unsigned int nGameID);
static void		NeoCDList_ScanDir(HWND hList, TCHAR* pszDirectory);
static TCHAR*	NeoCDList_ParseCUE(TCHAR* pszFile);
static void		NeoCDList_ShowPreview(HWND hDlg, TCHAR* szFile, int nCtrID, int nFrameCtrID, float maxw, float maxh);
struct PNGRESOLUTION { int nWidth; int nHeight; };
static PNGRESOLUTION GetPNGResolution(TCHAR* szFile);

static INT_PTR	CALLBACK	NeoCDList_WndProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
static unsigned __stdcall	NeoCDList_DoProc(void*);

static HWND		hNeoCDWnd			= NULL;
static HWND		hListView			= NULL;
static HWND		hProcParent			= NULL;
static bool		bProcessingList		= false;
static HANDLE   hProcessThread = NULL;
static unsigned ProcessThreadID = 0;

static HBRUSH hWhiteBGBrush;

bool bNeoCDListScanSub			= false;
bool bNeoCDListScanOnlyISO		= false;
TCHAR szNeoCDCoverDir[MAX_PATH] = _T("support/neocdz/");
TCHAR szNeoCDPreviewDir[MAX_PATH] = _T("support/neocdzpreviews/");
TCHAR szNeoCDGamesDir[MAX_PATH] = _T("neocdiso/");

static int nSelectedItem = -1;

struct GAMELIST {
	unsigned int nID;
	bool	bFoundCUE;
	TCHAR	szPathCUE[256];
	TCHAR	szPath[256];
	TCHAR	szISOFile[256];
	int		nAudioTracks;
	TCHAR	szTracks[99][256];
	TCHAR	szShortName[32];
	TCHAR	szPublisher[100];
};

#define MAX_NGCD	200
static GAMELIST ngcd_list[MAX_NGCD];
static int nListItems = 0;

// CD image stuff
const int nSectorLength		= 2352;

// Add game to List
static int NeoCDList_AddGame(TCHAR* pszFile, unsigned int nGameID)
{
	NGCDGAME* game;

	if(GetNeoGeoCDInfo(nGameID) && nListItems < MAX_NGCD)
	{
		game = (NGCDGAME*)malloc(sizeof(NGCDGAME));
		memset(game, 0, sizeof(NGCDGAME));

		memcpy(game, GetNeoGeoCDInfo(nGameID), sizeof(NGCDGAME));

		TCHAR szNGCDID[12] = _T("");
		if (nGameID != 0x0000) {
			_stprintf(szNGCDID, _T("%04X"), nGameID);
		}

		LVITEM lvi;
		ZeroMemory(&lvi, sizeof(lvi));

		// NGCD-ID [Insert]
		lvi.iImage			= 0;
		lvi.iItem			= ListView_GetItemCount(hListView) + 1;
		lvi.mask			= LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
		lvi.cchTextMax		= 255;
		TCHAR szTitle[256];
		if (nGameID == 0x0000) {
			// placeholder entry, argument (pszFile) instead of game name
			_stprintf(szTitle, _T("%s"), pszFile);
		} else {
			_stprintf(szTitle, _T("%s"), game->pszTitle);
		}
		lvi.pszText			= szTitle;

		int nItem = ListView_InsertItem(hListView, &lvi);

		// TITLE
		ZeroMemory(&lvi, sizeof(lvi));

		lvi.iSubItem		= 1;
		lvi.iItem			= nItem;
		lvi.cchTextMax		= _tcslen(game->pszTitle);
		lvi.mask			= LVIF_TEXT | LVIF_IMAGE;
		lvi.pszText			= szNGCDID;

		ListView_SetItem(hListView, &lvi);

		ngcd_list[nListItems].nID = nGameID;

		_tcscpy(ngcd_list[nListItems].szPath, pszFile);
		_tcscpy(ngcd_list[nListItems].szShortName, game->pszName);
		_stprintf(ngcd_list[nListItems].szPublisher, _T("%s (%s)"), game->pszCompany, game->pszYear);

		nListItems++;

	} else {
		// error

		return 0;
	}

	if(game) {
		free(game);
		game = NULL;
	}
	return 1;
}

static int sort_direction = 0;
enum {
	SORT_ASCENDING	= 0,
	SORT_DESCENDING	= 1
};

struct LVCOMPAREINFO
{
	HWND hWnd;
	int nColumn;
	BOOL bAscending;
};

static LVCOMPAREINFO lv_compare;

static int CALLBACK ListViewCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	TCHAR buf1[MAX_PATH];
	TCHAR buf2[MAX_PATH];
	LVCOMPAREINFO* lpsd = (struct LVCOMPAREINFO*)lParamSort;

	ListView_GetItemText(lpsd->hWnd, (int)lParam1, lpsd->nColumn, buf1, sizeof(buf1));
	ListView_GetItemText(lpsd->hWnd, (int)lParam2, lpsd->nColumn, buf2, sizeof(buf2));

	switch (lpsd->bAscending) {
		case SORT_ASCENDING:
			return (_wcsicmp(buf1, buf2));
		case SORT_DESCENDING:
			return (0 - _wcsicmp(buf1, buf2));
	}

	return 0;
}

static void ListViewSort(int nDirection, int nColumn)
{
	// sort the list
	lv_compare.hWnd = hListView;
	lv_compare.nColumn = nColumn;
	lv_compare.bAscending = nDirection;
	ListView_SortItemsEx(hListView, ListViewCompareFunc, &lv_compare);
}

static void NeoCDList_InitListView()
{
	LVCOLUMN LvCol;
	ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT);

	memset(&LvCol, 0, sizeof(LvCol));
	LvCol.mask		= LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

	LvCol.cx		= 445;
	LvCol.pszText	= FBALoadStringEx(hAppInst, IDS_NGCD_TITLE, true);
	SendMessage(hListView, LVM_INSERTCOLUMN , 0, (LPARAM)&LvCol);

	LvCol.cx		= 70;
	LvCol.pszText	= FBALoadStringEx(hAppInst, IDS_NGCD_NGCDID, true);
	SendMessage(hListView, LVM_INSERTCOLUMN , 1, (LPARAM)&LvCol);

	sort_direction = SORT_ASCENDING; // dink

	// Setup ListView Icons
//	HIMAGELIST hImageList = ImageList_Create(16, 16, ILC_MASK | ILC_COLOR16, 0, 1);
//	ListView_SetImageList(hListView, hImageList, LVSIL_SMALL);
//	ImageList_AddMasked(hImageList, LoadBitmap(hAppInst, MAKEINTRESOURCE(BMP_CD)), RGB(255, 0, 255));
}

static int NeoCDList_CheckDuplicates(HWND hList, unsigned int nID)
{
	int nItemCount = ListView_GetItemCount(hList);

	for(int nItem = 0; nItem < nItemCount; nItem++)
	{
		unsigned int nItemVal = 0;
		TCHAR szText[32];

		ListView_GetItemText(hList, nItem, 1, szText, 5);

		_stscanf(szText, _T("%x"), &nItemVal);

		if(nItemVal == nID) {
			ngcd_list[nItemCount].nAudioTracks = 0;
			return 1; // let's get out of here...
		}
	}

	return 0;
}

static void NeoCDSel_Callback(INT32 nID, TCHAR *pszFile)
{
	// make sure we don't add duplicates to the list
	if(NeoCDList_CheckDuplicates(hListView, nID)) {
		return;
	}

	NeoCDList_AddGame(pszFile, nID);
}

static int ScanDir_RecursionCount = 0;

// Scan the specified directory for ISO / CUE files, recurse if desired.
static void NeoCDList_ScanDir_Internal(HWND hList, TCHAR* pszDirectory)
{
	if (ScanDir_RecursionCount > 8) {
		bprintf(PRINT_ERROR, _T("*** Recursion too deep, can't traverse \"%s\"!\n"), pszDirectory);
		return;
	}

	ScanDir_RecursionCount++;

	WIN32_FIND_DATA ffdDirectory;
	HANDLE hDirectory = NULL;
	memset(&ffdDirectory, 0, sizeof(WIN32_FIND_DATA));

	// Scan directory for CUE
	TCHAR szSearch[512] = _T("\0");

	_stprintf(szSearch, _T("%s*.*"), pszDirectory);

	hDirectory = FindFirstFile(szSearch, &ffdDirectory);

	if (hDirectory != INVALID_HANDLE_VALUE)
	{
		do
		{
			// Directories only
			if (ffdDirectory.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (!_tcscmp(ffdDirectory.cFileName, _T(".")) || !_tcscmp(ffdDirectory.cFileName, _T("..")))
				{
					// lets ignore " . " and " .. "
				}
				else if (bNeoCDListScanSub)
				{
					// Traverse & recurse this subdir
					TCHAR szNewDir[512] = _T("\0");
					_stprintf(szNewDir, _T("%s%s\\"), pszDirectory, ffdDirectory.cFileName);

					NeoCDList_ScanDir_Internal(hList, szNewDir);
				}
			}
			// Files only
			if (!(ffdDirectory.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				if (IsFileExt(ffdDirectory.cFileName, _T(".cue")))
				{
					// Found CUE, Parse it
					TCHAR szParse[512] = _T("\0");
					_stprintf(szParse, _T("%s%s"), pszDirectory, ffdDirectory.cFileName);

					TCHAR *pszISO = NeoCDList_ParseCUE( szParse );

					TCHAR szISO[512] =_T("\0");
					_stprintf(szISO, _T("%s%s"), pszDirectory, pszISO);
					free(pszISO);

					NeoCDList_CheckISO(szISO, NeoCDSel_Callback);
				}
			}
		} while (FindNextFile(hDirectory, &ffdDirectory));

		FindClose(hDirectory);
	}

	ScanDir_RecursionCount--;
}

static void NeoCDList_ScanDir(HWND hList, TCHAR* pszDirectory)
{
	ScanDir_RecursionCount = 0;

	NeoCDList_ScanDir_Internal(hList, pszDirectory);
}

static bool FileExists(TCHAR *tcszFile)
{
	FILE *fp = _tfopen(tcszFile, _T("rb"));
	const bool bExists = fp != NULL;
	if (bExists) fclose(fp);

	return bExists;
}

// This will parse the specified CUE file and return the ISO path, if found
static TCHAR* NeoCDList_ParseCUE(TCHAR* pszFile)
{
	//if(!pszFile) return NULL;

	TCHAR* szISO = NULL;
	szISO = (TCHAR*)malloc(sizeof(TCHAR) * 2048);
	if(!szISO) return NULL;

	// open file
	FILE* fp = NULL;
	fp = _tfopen(pszFile, _T("r"));

	if(!fp) {
		if (szISO)
		{
			free(szISO);
			return NULL;
		}
	}

	while(!feof(fp))
	{
		TCHAR szBuffer[2048];
		TCHAR szOriginal[2048];
		TCHAR* s;
		TCHAR* t;

		_fgetts(szBuffer, 2048, fp);

		int nLength = 0;
		nLength = _tcslen(szBuffer);

		// Remove ASCII control characters from the string (including the 'space' character)
		while (szBuffer[nLength-1] < 32 && nLength > 0)
		{
			szBuffer[nLength-1] = 0;
			nLength--;
		}

		_tcscpy(szOriginal, szBuffer);

		if(!_tcsncmp(szBuffer, _T("FILE"), 4))
		{
			TCHAR* pEnd = _tcsrchr(szBuffer, '"');
			if (!pEnd)	{
				break;	// Invalid CUE format
			}

			*pEnd = 0;

			TCHAR* pStart = _tcschr(szBuffer, '"');

			if(!pStart)	{
				break;	// Invalid CUE format
			}

			if(!_tcsncmp(pEnd + 2, _T("BINARY"), 6))
			{
				_tcscpy(szISO,  pStart + 1);
				ngcd_list[nListItems].bFoundCUE = true;
				_tcscpy(ngcd_list[nListItems].szPathCUE,  pszFile);
				_tcscpy(ngcd_list[nListItems].szISOFile,  pStart + 1);
			}
		}
		// track info
		if ((t = LabelCheck(szBuffer, _T("TRACK"))) != 0) {
			s = t;

			// track number
			/*UINT8 track = */ _tcstol(s, &t, 10);

			s = t;

			// type of track

			if ((t = LabelCheck(s, _T("MODE1/2352"))) != 0) {
				//bprintf(0, _T(".cue: Track #%d, data.\n"), track);
				continue;
			}
			if ((t = LabelCheck(s, _T("AUDIO"))) != 0) {
				//bprintf(0, _T(".cue: Track #%d, AUDIO.\n"), track);
				ngcd_list[nListItems].nAudioTracks++;
				continue;
			}

			fclose(fp);
			return szISO;
		}
	}
	if(fp) fclose(fp);

	return szISO;
}

static PNGRESOLUTION GetPNGResolution(TCHAR* szFile)
{
	PNGRESOLUTION nResolution = { 0, 0 };
	IMAGE img = { 0, 0, 0, 0, NULL, NULL, 0 };

	FILE *fp = _tfopen(szFile, _T("rb"));

	if (!fp) {
		return nResolution;
	}

	PNGGetInfo(&img, fp);

	nResolution.nWidth = img.width;
	nResolution.nHeight = img.height;

	fclose(fp);

	return nResolution;
}

static void NeoCDList_ShowPreview(HWND hDlg, TCHAR* szFile, int nCtrID, int nFrameCtrID, float maxw, float maxh)
{
	PNGRESOLUTION PNGRes = { 0, 0 };
	if(!FileExists(szFile))
	{
		HRSRC hrsrc			= FindResource(NULL, MAKEINTRESOURCE(BMP_SPLASH), RT_BITMAP);
		HGLOBAL hglobal		= LoadResource(NULL, (HRSRC)hrsrc);

		BITMAPINFOHEADER* pbmih = (BITMAPINFOHEADER*)LockResource(hglobal);

		PNGRes.nWidth	= pbmih->biWidth;
		PNGRes.nHeight	= pbmih->biHeight;

		FreeResource(hglobal);

	} else {
		PNGRes = GetPNGResolution(szFile);
	}

	// ------------------------------------------------------
	// PROPER ASPECT RATIO CALCULATIONS

	float w = (float)PNGRes.nWidth;
	float h = (float)PNGRes.nHeight;

	if (maxw == 0 && maxh == 0) {
		// derive max w/h from dialog size
		RECT rect = { 0, 0, 0, 0 };
		GetWindowRect(GetDlgItem(hDlg, IDC_STATIC2), &rect);
		int dw = rect.right - rect.left;
		int dh = rect.bottom - rect.top;

		dw = dw * 90 / 100; // make W 90% of the "Preview / Title" windowpane
		dh = dw * 75 / 100; // make H 75% of w (4:3)

		maxw = dw;
		maxh = dh;
	}

	// max WIDTH
	if(w > maxw) {
		float nh = maxw * (float)(h / w);
		float nw = maxw;

		// max HEIGHT
		if(nh > maxh) {
			nw = maxh * (float)(nw / nh);
			nh = maxh;
		}

		w = nw;
		h = nh;
	}

	// max HEIGHT
	if(h > maxh) {
		float nw = maxh * (float)(w / h);
		float nh = maxh;

		// max WIDTH
		if(nw > maxw) {
			nh = maxw * (float)(nh / nw);
			nw = maxw;
		}

		w = nw;
		h = nh;
	}

	// Proper centering of preview
	float x = ((maxw - w) / 2);
	float y = ((maxh - h) / 2);

	RECT rc = {0, 0, 0, 0};
	GetWindowRect(GetDlgItem(hDlg, nFrameCtrID), &rc);

	POINT pt;
	pt.x = rc.left;
	pt.y = rc.top;

	ScreenToClient(hDlg, &pt);

	// ------------------------------------------------------

	FILE* fp = _tfopen(szFile, _T("rb"));

	HBITMAP hCoverBmp = PNGLoadBitmap(hDlg, fp, (int)w, (int)h, 0);

	SetWindowPos(GetDlgItem(hDlg, nCtrID), NULL, (int)(pt.x + x), (int)(pt.y + y), 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
	SendDlgItemMessage(hDlg, nCtrID, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hCoverBmp);

	if(fp) fclose(fp);

}

static void NeoCDList_Clean()
{
	NeoCDList_ShowPreview(hNeoCDWnd, _T(""), IDC_NCD_FRONT_PIC, IDC_NCD_FRONT_PIC_FRAME, 0, 0);
	NeoCDList_ShowPreview(hNeoCDWnd, _T(""), IDC_NCD_BACK_PIC, IDC_NCD_BACK_PIC_FRAME, 0, 0);

	SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTSHORT), _T(""));
	SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTPUBLISHER), _T(""));
	SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTIMAGE), _T(""));
	SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTAUDIO), _T(""));

	hProcessThread = NULL;
	ProcessThreadID = 0;

	memset(&ngcd_list, 0, sizeof(ngcd_list));

	hProcParent			= NULL;
	bProcessingList		= false;

	nListItems = 0;

	nSelectedItem = -1;
}

static HWND hNeoCDList_CoverDlg = NULL;
TCHAR szBigCover[512] = _T("");

static INT_PTR CALLBACK NeoCDList_CoverWndProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM /*lParam*/)
{
	if(Msg == WM_INITDIALOG)
	{
		hNeoCDList_CoverDlg = hDlg;

		NeoCDList_ShowPreview(hDlg, szBigCover, IDC_NCD_COVER_PREVIEW_PIC, IDC_NCD_COVER_PREVIEW_PIC, 580, 415);

		return TRUE;
	}

	if(Msg == WM_CLOSE)
	{
		EndDialog(hDlg, 0);
		hNeoCDList_CoverDlg	= NULL;
	}

	if(Msg == WM_COMMAND)
	{
		if (LOWORD(wParam) == WM_DESTROY) {
			SendMessage(hDlg, WM_CLOSE, 0, 0);
		}
	}

	return 0;
}

void NeoCDList_InitCoverDlg()
{
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_NCD_COVER_DLG), hNeoCDWnd, (DLGPROC)NeoCDList_CoverWndProc);
}

static INT_PTR CALLBACK NeoCDList_WndProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if(Msg == WM_INITDIALOG)
	{
		hNeoCDWnd = hDlg;
		hListView = GetDlgItem(hDlg, IDC_NCD_LIST);
		NeoCDList_InitListView();

		HICON hIcon = LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_APP));
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);		// Set the Game Selection dialog icon.

		hWhiteBGBrush	= CreateSolidBrush(RGB(0xFF,0xFF,0xFF));

		NeoCDList_ShowPreview(hNeoCDWnd, _T(""), IDC_NCD_FRONT_PIC, IDC_NCD_FRONT_PIC_FRAME, 0, 0);
		NeoCDList_ShowPreview(hNeoCDWnd, _T(""), IDC_NCD_BACK_PIC, IDC_NCD_BACK_PIC_FRAME, 0, 0);

		SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTSHORT), _T(""));
		SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTPUBLISHER), _T(""));
		SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTIMAGE), _T(""));
		SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTAUDIO), _T(""));

		CheckDlgButton(hNeoCDWnd, IDC_NCD_SSUBDIR_CHECK, bNeoCDListScanSub ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hNeoCDWnd, IDC_NCD_SISO_ONLY_CHECK, bNeoCDListScanOnlyISO ? BST_CHECKED : BST_UNCHECKED);

		TreeView_SetItemHeight(hListView, 40);

		TCHAR szDialogTitle[200];
		_stprintf(szDialogTitle, FBALoadStringEx(hAppInst, IDS_NGCD_DIAG_TITLE, true), _T(APP_TITLE), _T(SEPERATOR_1), _T(SEPERATOR_1));
		SetWindowText(hDlg, szDialogTitle);

		hProcessThread = (HANDLE)_beginthreadex(NULL, 0, NeoCDList_DoProc, NULL, 0, &ProcessThreadID);

		WndInMid(hDlg, hScrnWnd);
		SetFocus(hListView);

		return TRUE;
	}

	if(Msg == WM_CLOSE)
	{
		NeoCDList_Clean();

		DeleteObject(hWhiteBGBrush);

		hNeoCDWnd	= NULL;
		hListView	= NULL;

		EndDialog(hDlg, 0);
	}

	if (Msg == WM_CTLCOLORSTATIC)
	{
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_LABELSHORT))	return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_LABELPUBLISHER))	return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_LABELIMAGE))	return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_LABELAUDIO))	return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTSHORT))	return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTPUBLISHER))	return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTIMAGE))	return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTAUDIO))	return (INT_PTR)hWhiteBGBrush;
	}

	if (Msg == WM_NOTIFY)
	{
		NMLISTVIEW* pNMLV	= (NMLISTVIEW*)lParam;

		// Game Selected
		if (pNMLV->hdr.code == LVN_ITEMCHANGED && pNMLV->hdr.idFrom == IDC_NCD_LIST)
		{
			int iCount		= SendMessage(hListView, LVM_GETITEMCOUNT, 0, 0);
			int iSelCount	= SendMessage(hListView, LVM_GETSELECTEDCOUNT, 0, 0);

			if(iCount == 0 || iSelCount == 0) return 1;

			TCHAR szID[] = _T("0000");

			int iItem = pNMLV->iItem;

			LVITEM LvItem;
			memset(&LvItem, 0, sizeof(LvItem));

			LvItem.iItem		= iItem;
			LvItem.mask			= LVIF_TEXT;
			LvItem.iSubItem		= 1;				// id column
			LvItem.pszText		= szID;
			LvItem.cchTextMax	= sizeof(szID) + 1;

			SendMessage(hListView, LVM_GETITEMTEXT, (WPARAM)iItem, (LPARAM)&LvItem);

			//MessageBox(NULL, LvItem.pszText, _T(""), MB_OK);

			SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTSHORT), _T(""));
			SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTPUBLISHER), _T(""));
			SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTIMAGE), _T(""));
			SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTAUDIO), _T(""));

			for(int nItem = 0; nItem < nListItems; nItem++)
			{
				unsigned int nID = 0;
				_stscanf(szID, _T("%x"), &nID);

				TCHAR szAudioTracks[10] = _T("0");

				if(nID == ngcd_list[nItem].nID) {
					SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTSHORT), ngcd_list[nItem].szShortName);
					SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTPUBLISHER), ngcd_list[nItem].szPublisher);
					SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTIMAGE), ngcd_list[nItem].szISOFile);
					_stprintf(szAudioTracks, _T("%d"), ngcd_list[nItem].nAudioTracks);
					SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTAUDIO), szAudioTracks);

					TCHAR szFront[512];
					TCHAR szBack[512];

					_stprintf(szFront, _T("%s%s-front.png"), szNeoCDCoverDir, ngcd_list[nItem].szShortName );

					_stprintf(szBack, _T("%s%s.png"), szNeoCDPreviewDir, ngcd_list[nItem].szShortName );
					if (!FileExists(szBack)) {
						// no neocd-specific preview? fall-back to regular previews
						_stprintf(szBack, _T("%s%s.png"), szAppPreviewsPath, ngcd_list[nItem].szShortName );
					}

					// Front / Back Cover preview
					NeoCDList_ShowPreview(hNeoCDWnd, szFront, IDC_NCD_FRONT_PIC, IDC_NCD_FRONT_PIC_FRAME, 0, 0);
					NeoCDList_ShowPreview(hNeoCDWnd, szBack, IDC_NCD_BACK_PIC, IDC_NCD_BACK_PIC_FRAME, 0, 0);

					nSelectedItem = nItem;
					break;
				}
			}


		}

		// Sort Change
		if (pNMLV->hdr.code == LVN_COLUMNCLICK && pNMLV->hdr.idFrom == IDC_NCD_LIST)
		{
			sort_direction ^= 1;
			ListViewSort(sort_direction, pNMLV->iSubItem);
		}

		// Double Click
		if (pNMLV->hdr.code == NM_DBLCLK && pNMLV->hdr.idFrom == IDC_NCD_LIST)
		{
			if(nSelectedItem >= 0) {
				nCDEmuSelect = 0;
				if(ngcd_list[nSelectedItem].bFoundCUE) {
					_tcscpy(CDEmuImage, ngcd_list[nSelectedItem].szPathCUE);
				} else {
					_tcscpy(CDEmuImage, ngcd_list[nSelectedItem].szPath);
				}
			} else {
				return 0;
			}

			NeoCDList_Clean();

			hNeoCDWnd	= NULL;
			hListView	= NULL;

			EndDialog(hDlg, 0);

			BurnerLoadDriver(_T("neocdz"));
		}
	}

	if(Msg == WM_COMMAND)
	{
		if(HIWORD(wParam) == STN_CLICKED)
		{
			int nCtrlID = LOWORD(wParam);

			if(nCtrlID == IDC_NCD_FRONT_PIC)
			{
				if(nSelectedItem >= 0) {
					_stprintf(szBigCover, _T("%s%s-front.png"), szNeoCDCoverDir, ngcd_list[nSelectedItem].szShortName );

					if(!FileExists(szBigCover)) {
						szBigCover[0] = 0;
						return 0;
					}

					NeoCDList_InitCoverDlg();
				}
				return 0;
			}

			if(nCtrlID == IDC_NCD_BACK_PIC)
			{
				if(nSelectedItem >= 0) {
					_stprintf(szBigCover, _T("%s%s.png"), szNeoCDPreviewDir, ngcd_list[nSelectedItem].szShortName );
					if (!FileExists(szBigCover)) {
						// no neocd-specific preview? fall-back to regular previews
						_stprintf(szBigCover, _T("%s%s.png"), szAppPreviewsPath, ngcd_list[nSelectedItem].szShortName );
					}

					if(!FileExists(szBigCover)) {
						szBigCover[0] = 0;
						return 0;
					}

					NeoCDList_InitCoverDlg();
				}
				return 0;
			}
		}

		if (LOWORD(wParam) == WM_DESTROY) {
			SendMessage(hDlg, WM_CLOSE, 0, 0);
		}

		if(HIWORD(wParam) == BN_CLICKED)
		{
			int nCtrlID	= LOWORD(wParam);

			switch(nCtrlID)
			{
				case IDC_NCD_PLAY_BUTTON:
				{
					if(nSelectedItem >= 0) {
						nCDEmuSelect = 0;
						if(ngcd_list[nSelectedItem].bFoundCUE) {
							_tcscpy(CDEmuImage, ngcd_list[nSelectedItem].szPathCUE);
						} else {
							_tcscpy(CDEmuImage, ngcd_list[nSelectedItem].szPath);
						}
					} else {
						break;
					}

					NeoCDList_Clean();

					DeleteObject(hWhiteBGBrush);

					hNeoCDWnd	= NULL;
					hListView	= NULL;

					EndDialog(hDlg, 0);

					BurnerLoadDriver(_T("neocdz"));
					break;
				}

				case IDC_NCD_SCAN_BUTTON:
				{
					if(bProcessingList) break;

					NeoCDList_Clean();
					hProcessThread = (HANDLE)_beginthreadex(NULL, 0, NeoCDList_DoProc, NULL, 0, &ProcessThreadID);
					SetFocus(hListView);
					break;
				}

				case IDC_NCD_SEL_DIR_BUTTON:
				{
					if(bProcessingList) break;

					NeoCDList_Clean();

					SupportDirCreate(hNeoCDWnd);
					hProcessThread = (HANDLE)_beginthreadex(NULL, 0, NeoCDList_DoProc, NULL, 0, &ProcessThreadID);
					SetFocus(hListView);
					break;
				}

				case IDC_NCD_SSUBDIR_CHECK:
				{
					if(BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_NCD_SSUBDIR_CHECK)) {
						bNeoCDListScanSub = true;
					} else {
						bNeoCDListScanSub = false;
					}

					if(bProcessingList) break;

					NeoCDList_Clean();

					hProcessThread = (HANDLE)_beginthreadex(NULL, 0, NeoCDList_DoProc, NULL, 0, &ProcessThreadID);
					SetFocus(hListView);
					break;
				}
#if 0
				case IDC_NCD_SISO_ONLY_CHECK:
				{
					if(BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_NCD_SISO_ONLY_CHECK)) {
						bNeoCDListScanOnlyISO = true;
					} else {
						bNeoCDListScanOnlyISO = false;
					}

					if(bProcessingList) break;

					NeoCDList_Clean();

					hProcessThread = (HANDLE)_beginthreadex(NULL, 0, NeoCDList_DoProc, NULL, 0, &ProcessThreadID);
					SetFocus(hListView);
					break;
				}
#endif
				case IDC_NCD_CANCEL_BUTTON:
				{
					NeoCDList_Clean();

					DeleteObject(hWhiteBGBrush);

					hNeoCDWnd	= NULL;
					hListView	= NULL;

					EndDialog(hDlg, 0);
					break;
				}
			}
		}
	}
	return 0;
}

static unsigned __stdcall NeoCDList_DoProc(void*)
{
	if(bProcessingList) return 0;

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	bProcessingList = true;
	ListView_DeleteAllItems(hListView);
	NeoCDList_AddGame(_T("--- [  Please Wait: Searching for games!  ] ---"), 0x0000);
	NeoCDList_ScanDir(hListView, szNeoCDGamesDir);

	ListView_DeleteItem(hListView, 0); // delete "Please Wait" line :)

	ListViewSort(SORT_ASCENDING, 0);
	SetFocus(hListView);

	bProcessingList = false;
	hProcessThread = NULL;
	ProcessThreadID = 0;

	return 0;
}

int NeoCDList_Init()
{
	return FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_NCD_DLG), hScrnWnd, (DLGPROC)NeoCDList_WndProc);
}
