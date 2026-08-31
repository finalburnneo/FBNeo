#include "burner.h"
#include "burnint.h"
#include "cdlistcache.h"
#include <process.h>

bool bNeoCDListScanSub = false;
TCHAR szNeoCDCoverDir[MAX_PATH]   = _T("support/cdcovers/");
TCHAR szNeoCDPreviewDir[MAX_PATH] = _T("support/cdpreviews/");
TCHAR szNeoCDGamesDir[MAX_PATH]   = _T("cdiso/");

#if defined(BUILD_NEOGEO) || defined(BUILD_PCE)

struct PNGRESOLUTION { INT32 nWidth; INT32 nHeight; };

struct CDScanContext {
	HWND   hDialog;
	HANDLE hThread;
	HANDLE hCancelEvent;
	CDLibrary* pResult;
	INT32  nStatus;
	UINT32 nGeneration;
	volatile LONG nTotal;
	volatile LONG nCompleted;
	volatile LONG nProgressPending;
};

#define WM_CDLIST_PROGRESS (WM_APP + 0x241)
#define WM_CDLIST_COMPLETE (WM_APP + 0x242)

static HWND hNeoCDWnd = NULL;
static HWND hListView = NULL;
static HBITMAP hCoverBMPs[3] = { NULL, NULL, NULL };
static HBRUSH  hWhiteBGBrush = NULL;
static HIMAGELIST hCDImageList = NULL;
static INT32 nCDIconIndex[2] = { -1, -1 };
static CDLibrary* pCDLibrary = NULL;
static CDScanContext* pActiveScan = NULL;
static UINT32 nScanGeneration = 0;
static INT32  nSelectedItem   = -1;
static INT32  nSortColumn     = 0;
static INT32  nSortDescending = 0;
static INT32  nRunPlatform    = CDLIST_PLATFORM_UNKNOWN;

static void* pZoomPng = NULL;
static size_t nZoomPngSize = 0;

static BOOL IsOSXPSP2OrGreater_Verify()
{
	OSVERSIONINFOEX osvi;
	DWORDLONG mask = 0;
	ZeroMemory(&osvi, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	osvi.dwMajorVersion = 5;
	osvi.dwMinorVersion = 1;
	osvi.wServicePackMajor = 2;
	VER_SET_CONDITION(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(mask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
	return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, mask);
}

static BOOL IsVistaOrGreater_Verify()
{
	OSVERSIONINFOEX osvi;
	DWORDLONG mask = 0;
	ZeroMemory(&osvi, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	osvi.dwMajorVersion      = 6;
	VER_SET_CONDITION(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	return VerifyVersionInfo(&osvi, VER_MAJORVERSION, mask);
}

static PNGRESOLUTION GetPNGResolutionBuf(void* pPngBuf, size_t nPngSize)
{
	PNGRESOLUTION Resolution = { 0, 0 };
	IMAGE Image = { 0, 0, 0, 0, NULL, NULL, 0 };
	if (pPngBuf && nPngSize) {
		PNGGetInfoBuffer(&Image, pPngBuf, nPngSize);
		Resolution.nWidth  = Image.width;
		Resolution.nHeight = Image.height;
	}
	return Resolution;
}

static void NeoCDList_ShowPreviewBuf(HWND hDlg, void* pPngBuf, size_t nPngSize, INT32 nControlID, INT32 nFrameID, float nMaxWidth, float nMaxHeight)
{
	PNGRESOLUTION Resolution = GetPNGResolutionBuf(pPngBuf, nPngSize);
	if (!Resolution.nWidth || !Resolution.nHeight) {
		HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(BMP_SPLASH), RT_BITMAP);
		HGLOBAL hGlobal = hResource ? LoadResource(NULL, hResource) : NULL;
		BITMAPINFOHEADER* pHeader = hGlobal ? (BITMAPINFOHEADER*)LockResource(hGlobal) : NULL;
		if (pHeader) {
			Resolution.nWidth  = pHeader->biWidth;
			Resolution.nHeight = pHeader->biHeight;
		}
	}

	HWND hControl = GetDlgItem(hDlg, nControlID);
	HWND hFrame   = GetDlgItem(hDlg, nFrameID);
	if (!hControl || !hFrame || !Resolution.nWidth || !Resolution.nHeight) {
		free_s((void**)&pPngBuf);
		return;
	}

	if (!nMaxWidth && !nMaxHeight) {
		RECT Pane  = { 0, 0, 0, 0 };
		HWND hPane = GetDlgItem(hDlg, IDC_STATIC2);
		if (!hPane || !GetWindowRect(hPane, &Pane)) {
			free_s((void**)&pPngBuf);
			return;
		}
		nMaxWidth = (float)((Pane.right - Pane.left) * 90 / 100);
		nMaxHeight = nMaxWidth * 0.75f;
	}

	if (Resolution.nWidth/Resolution.nHeight >= 4) {
		// pce screenshots are 1024x240 (usually)
		// we need to 4:3-ize it -dink
		Resolution.nWidth = 320;
		Resolution.nHeight = 240; // if this is larger than 240, it would lead to artifacts caused by the
		// shrink code in ../image.cpp, ie: it can't shrink x and expand y (TOFIX-dink)
	}

	float nWidth  = (float)Resolution.nWidth;
	float nHeight = (float)Resolution.nHeight;
	float nScale  = 1.0f;
	if (nWidth > nMaxWidth) nScale = nMaxWidth / nWidth;
	if (nHeight * nScale > nMaxHeight) nScale = nMaxHeight / nHeight;
	nWidth  *= nScale;
	nHeight *= nScale;

	RECT Frame = { 0, 0, 0, 0 };
	GetWindowRect(hFrame, &Frame);
	POINT Position = { Frame.left, Frame.top };
	ScreenToClient(hDlg, &Position);
	Position.x += (INT32)((nMaxWidth  - nWidth ) / 2.0f);
	Position.y += (INT32)((nMaxHeight - nHeight) / 2.0f);

	INT32 nBitmapIndex = nControlID - IDC_NCD_FRONT_PIC;
	if (nBitmapIndex < 0 || nBitmapIndex > 1)
		nBitmapIndex = 2;
	if (hCoverBMPs[nBitmapIndex])
		DeleteObject(hCoverBMPs[nBitmapIndex]);

	hCoverBMPs[nBitmapIndex] = PNGLoadBitmapBuffer(hDlg, pPngBuf, (INT32)nPngSize, (INT32)nWidth, (INT32)nHeight, 0);
	free_s((void**)&pPngBuf);
	SetWindowPos(hControl, NULL, Position.x, Position.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);
	SendMessage(hControl, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hCoverBMPs[nBitmapIndex]);
}

static bool LoadZipToBuffer(const TCHAR* pszDirectory, const char* pszArchive, const TCHAR* pszImageName, const char* pszExtension, void** ppBuffer, size_t* pnSize)
{
	if (!pszDirectory || !pszArchive || !pszImageName || !pszExtension || !ppBuffer || !pnSize)
		return false;

	char szDirectory[MAX_PATH];
	char szName[MAX_PATH];
	char szImage[MAX_PATH];
	char szZip[MAX_PATH];
	_snprintf(szDirectory, sizeof(szDirectory), "%s", TCHARToANSI((TCHAR*)pszDirectory, NULL, 0));
	_snprintf(szName,      sizeof(szName     ), "%s", TCHARToANSI((TCHAR*)pszImageName, NULL, 0));
	szDirectory[sizeof(szDirectory) - 1] = '\0';
	szName[sizeof(szName)           - 1] = '\0';
	_snprintf(szImage, sizeof(szImage), "%s/%s%s", pszArchive, szName, pszExtension);
	_snprintf(szZip,   sizeof(szZip  ), "%s%s.zip", szDirectory, pszArchive);
	szImage[sizeof(szImage) - 1] = '\0';
	szZip[sizeof(szZip)     - 1] = '\0';
	return unzip(szZip, szImage, ppBuffer, pnSize);
}

static bool LoadFileToBuffer(const TCHAR* pszName, void** ppBuffer, size_t* pnSize)
{
	if (!pszName || !ppBuffer || !pnSize)
		return false;

	*ppBuffer = NULL;
	*pnSize   = 0;
	FILE* pFile = _tfopen(pszName, _T("rb"));
	if (!pFile)
		return false;

	fseek(pFile, 0, SEEK_END);
	long nLength = ftell(pFile);
	rewind(pFile);
	if (nLength > 0) {
		*ppBuffer = malloc((size_t)nLength);
		if (*ppBuffer && fread(*ppBuffer, 1, (size_t)nLength, pFile) == (size_t)nLength)
			*pnSize = (size_t)nLength;
		else
			free_s(ppBuffer);
	}
	fclose(pFile);
	return *ppBuffer != NULL;
}

static const CDLibraryEntry* GetLibraryEntry(INT32 nIndex)
{
	if (!pCDLibrary || nIndex < 0 || (UINT32)nIndex >= pCDLibrary->nCount)
		return NULL;

	return &pCDLibrary->pEntries[nIndex];
}

static const TCHAR* GetPreviewName(const CDLibraryEntry* pEntry)
{
	if (!pEntry)
		return _T("");

	return pEntry->szShortName[0] ? pEntry->szShortName : pEntry->szTitle;
}

static void ClearSelectionUI()
{
	nSelectedItem = -1;
	if (!hNeoCDWnd)
		return;

	EnableWindow(GetDlgItem(hNeoCDWnd, IDC_NCD_PLAY_BUTTON), FALSE);
	SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTSHORT    ), _T(""));
	SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTPUBLISHER), _T(""));
	SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTIMAGE    ), _T(""));
	SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTAUDIO    ), _T(""));
	NeoCDList_ShowPreviewBuf(hNeoCDWnd, NULL, 0, IDC_NCD_FRONT_PIC, IDC_NCD_FRONT_PIC_FRAME, 0, 0);
	NeoCDList_ShowPreviewBuf(hNeoCDWnd, NULL, 0, IDC_NCD_BACK_PIC,  IDC_NCD_BACK_PIC_FRAME,  0, 0);
}

static void BuildPreviewPath(TCHAR* pszPath, UINT32 nPathCount, const TCHAR* pszDirectory, const TCHAR* pszName)
{
	if (!pszPath || !nPathCount)
		return;

	_sntprintf(pszPath, nPathCount, _T("%s%s.png"), pszDirectory ? pszDirectory : _T(""), pszName ? pszName : _T(""));
	pszPath[nPathCount - 1] = _T('\0');
}

static void LoadEntryPreview(const CDLibraryEntry* pEntry, INT32 nSide, void** ppBuffer, size_t* pnSize)
{
	*ppBuffer = NULL;
	*pnSize   = 0;
	if (!pEntry)
		return;

	const TCHAR* pszName = GetPreviewName(pEntry);
	TCHAR szPath[MAX_PATH];
	TCHAR tszName[MAX_PATH];
	_stprintf(tszName, _T("%s%s"), (pEntry->nPlatform == CDLIST_PLATFORM_PCECD) ? _T("pcecd_") : _T("ngcd_"), pszName);

	if (!nSide) {
		BuildPreviewPath(szPath, MAX_PATH, szNeoCDCoverDir, tszName);
		if (!LoadFileToBuffer(szPath, ppBuffer, pnSize))
			LoadZipToBuffer(szNeoCDCoverDir, "cdcovers", tszName, ".png", ppBuffer, pnSize);

		return;
	}

	BuildPreviewPath(szPath, MAX_PATH, szNeoCDPreviewDir, tszName);
	if (LoadFileToBuffer(szPath, ppBuffer, pnSize))
		return;

	if (LoadZipToBuffer(szNeoCDPreviewDir, "cdpreviews", tszName, ".png", ppBuffer, pnSize))
		return;

	BuildPreviewPath(szPath, MAX_PATH, szAppPreviewsPath, tszName);
	if (!LoadFileToBuffer(szPath, ppBuffer, pnSize))
		LoadZipToBuffer(szAppPreviewsPath, "previews", tszName, ".png", ppBuffer, pnSize);
}

static void ShowEntryDetails(INT32 nIndex)
{
	const CDLibraryEntry* pEntry = GetLibraryEntry(nIndex);
	if (!pEntry) {
		ClearSelectionUI();
		return;
	}

	TCHAR szPublisher[CDLIST_TEXT_SIZE + 40];
	if (pEntry->szCompany[0] && pEntry->szYear[0])
		_sntprintf(szPublisher, ARRAY_SIZE(szPublisher), _T("%s (%s)"), pEntry->szCompany, pEntry->szYear);
	else
		_sntprintf(szPublisher, ARRAY_SIZE(szPublisher), _T("%s%s"   ), pEntry->szCompany, pEntry->szYear);

	szPublisher[ARRAY_SIZE(szPublisher) - 1] = _T('\0');

	TCHAR szAudio[32];
	_sntprintf(szAudio, ARRAY_SIZE(szAudio), _T("%d"), pEntry->nAudioTrackCount);
	szAudio[ARRAY_SIZE(szAudio) - 1] = _T('\0');

	SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTSHORT    ), pEntry->szShortName);
	SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTPUBLISHER), szPublisher);
	SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTIMAGE    ), pEntry->szDataPath[0] ? pEntry->szDataPath : pEntry->szPath);
	SetWindowText(GetDlgItem(hNeoCDWnd, IDC_NCD_TEXTAUDIO    ), szAudio);

	void* pFront = NULL;
	void* pBack  = NULL;
	size_t nFrontSize = 0;
	size_t nBackSize  = 0;
	LoadEntryPreview(pEntry, 0, &pFront, &nFrontSize);
	LoadEntryPreview(pEntry, 1, &pBack,  &nBackSize);
	NeoCDList_ShowPreviewBuf(hNeoCDWnd, pFront, nFrontSize, IDC_NCD_FRONT_PIC, IDC_NCD_FRONT_PIC_FRAME, 0, 0);
	NeoCDList_ShowPreviewBuf(hNeoCDWnd, pBack,  nBackSize,  IDC_NCD_BACK_PIC,  IDC_NCD_BACK_PIC_FRAME,  0, 0);
	nSelectedItem = nIndex;
	EnableWindow(GetDlgItem(hNeoCDWnd, IDC_NCD_PLAY_BUTTON), TRUE);
}

static const TCHAR* PlatformText(INT32 nPlatform)
{
	if (nPlatform == CDLIST_PLATFORM_NEOCD)
		return _T("Neo Geo CD");
	if (nPlatform == CDLIST_PLATFORM_PCECD)
		return _T("PCE CD");

	return _T("Unknown");
}

static void EntryIDText(const CDLibraryEntry* pEntry, TCHAR* pszText, UINT32 nTextCount)
{
	if (!pszText || !nTextCount)
		return;

	if (pEntry && pEntry->nPlatform == CDLIST_PLATFORM_NEOCD && pEntry->nNeoID)
		_sntprintf(pszText, nTextCount, _T("%04X"), pEntry->nNeoID);
	else
		_sntprintf(pszText, nTextCount, _T("%s"  ), pEntry ? pEntry->szShortName : _T(""));

	pszText[nTextCount - 1] = _T('\0');
}

static int CompareEntries(const void* pLeft, const void* pRight)
{
	const CDLibraryEntry* pA = (const CDLibraryEntry*)pLeft;
	const CDLibraryEntry* pB = (const CDLibraryEntry*)pRight;

	INT32 nResult = 0;
	switch (nSortColumn) {
		case 1:
			nResult = _tcsicmp(PlatformText(pA->nPlatform), PlatformText(pB->nPlatform));
			break;
		case 2:
			if (pA->nPlatform != pB->nPlatform)
				nResult = pA->nPlatform - pB->nPlatform;
			else if (pA->nPlatform == CDLIST_PLATFORM_NEOCD)
				nResult = pA->nNeoID < pB->nNeoID ? -1 : pA->nNeoID > pB->nNeoID;
			else
				nResult = _tcsicmp(pA->szShortName, pB->szShortName);
			break;
		case 3:
			nResult = pA->nAudioTrackCount - pB->nAudioTrackCount;
			break;
		default:
			nResult = _tcsicmp(pA->szTitle, pB->szTitle);
			break;
	}

	if (!nResult)
		nResult = _tcsicmp(pA->szPath, pB->szPath);

	return nSortDescending ? -nResult : nResult;
}

static void PopulateList()
{
	if (!hListView)
		return;

	ClearSelectionUI();
	SendMessage(hListView, WM_SETREDRAW, FALSE, 0);
	ListView_DeleteAllItems(hListView);

	if (pCDLibrary && pCDLibrary->nCount) {
		qsort(pCDLibrary->pEntries, pCDLibrary->nCount, sizeof(CDLibraryEntry), CompareEntries);
		for (UINT32 i = 0; i < pCDLibrary->nCount; i++) {
			CDLibraryEntry* pEntry = &pCDLibrary->pEntries[i];
			LVITEM Item;
			memset(&Item, 0, sizeof(Item));

			Item.mask    = LVIF_TEXT | LVIF_PARAM;
			INT32 nImage = pEntry->nIcon >= 0 && pEntry->nIcon < (INT32)ARRAY_SIZE(nCDIconIndex) ? nCDIconIndex[pEntry->nIcon] : -1;
			if (bEnableIcons && hCDImageList && nImage >= 0) Item.mask |= LVIF_IMAGE;
			Item.iItem   = (INT32)i;
			Item.iImage  = nImage;
			Item.lParam  = (LPARAM)i;
			Item.pszText = pEntry->szTitle;

			INT32 nRow = ListView_InsertItem(hListView, &Item);
			if (nRow < 0)
				continue;

			ListView_SetItemText(hListView, nRow, 1, (TCHAR*)PlatformText(pEntry->nPlatform));
			TCHAR szID[CDLIST_TEXT_SIZE];
			EntryIDText(pEntry, szID, ARRAY_SIZE(szID));
			ListView_SetItemText(hListView, nRow, 2, szID);
			TCHAR szAudio[32];
			_sntprintf(szAudio, ARRAY_SIZE(szAudio), _T("%d"), pEntry->nAudioTrackCount);
			szAudio[ARRAY_SIZE(szAudio) - 1] = _T('\0');
			ListView_SetItemText(hListView, nRow, 3, szAudio);
		}
	}
	SendMessage(hListView, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(hListView, NULL, TRUE);
}

static void InitListView()
{
	DWORD nStyle = LVS_EX_FULLROWSELECT;
	if (IsVistaOrGreater_Verify()) nStyle |= LVS_EX_INFOTIP;
	ListView_SetExtendedListViewStyle(hListView, nStyle);

	const TCHAR* pszTitles[] = { _T("Title"), _T("Platform"), _T("ID / Short Name"), _T("Audio") };
	const INT32  nWidths[]   = { 250, 85, 115, 55 };
	for (INT32 i = 0; i < 4; i++) {
		LVCOLUMN Column;
		memset(&Column, 0, sizeof(Column));
		Column.mask     = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
		Column.iSubItem = i;
		Column.cx       = nWidths[i];
		Column.pszText  = (TCHAR*)pszTitles[i];
		ListView_InsertColumn(hListView, i, &Column);
	}

	if (!bEnableIcons)
		return;

	switch (nIconsSize) {
		case ICON_16x16: nIconsSizeXY = 16; break;
		case ICON_24x24: nIconsSizeXY = 24; break;
		case ICON_32x32: nIconsSizeXY = 32; break;
	}

	UINT nFlags = ILC_MASK | (IsOSXPSP2OrGreater_Verify() ? ILC_COLOR32 : ILC_COLOR16);
	nCDIconIndex[0] = nCDIconIndex[1] = -1;
	hCDImageList    = ImageList_Create(nIconsSizeXY, nIconsSizeXY, nFlags, 2, 2);

	if (!hCDImageList)
		return;

	HICON hIcon = (HICON)LoadImage(hAppInst, MAKEINTRESOURCE(IDI_LV_CDIMAGE_CUE), IMAGE_ICON, nIconsSizeXY, nIconsSizeXY, LR_DEFAULTCOLOR);
	if (hIcon) {
		nCDIconIndex[0] = ImageList_AddIcon(hCDImageList, hIcon);
		DestroyIcon(hIcon);
	}

	hIcon = (HICON)LoadImage(hAppInst, MAKEINTRESOURCE(IDI_LV_CDIMAGE_CHD), IMAGE_ICON, nIconsSizeXY, nIconsSizeXY, LR_DEFAULTCOLOR);
	if (hIcon) {
		nCDIconIndex[1] = ImageList_AddIcon(hCDImageList, hIcon);
		DestroyIcon(hIcon);
	}

	ListView_SetImageList(hListView, hCDImageList, LVSIL_SMALL);
}

static INT32 ScanCancelled(void* pUser)
{
	CDScanContext* pContext = (CDScanContext*)pUser;
	return !pContext || WaitForSingleObject(pContext->hCancelEvent, 0) == WAIT_OBJECT_0;
}

static void ScanProgress(INT32, UINT32 nTotal, UINT32 nCompleted, void* pUser)
{
	CDScanContext* pContext = (CDScanContext*)pUser;
	if (!pContext)
		return;

	InterlockedExchange(&pContext->nTotal,     (LONG)nTotal);
	InterlockedExchange(&pContext->nCompleted, (LONG)nCompleted);
	if (!InterlockedCompareExchange(&pContext->nProgressPending, 1, 0) &&
		!PostMessage(pContext->hDialog, WM_CDLIST_PROGRESS, pContext->nGeneration, 0))
		InterlockedExchange(&pContext->nProgressPending, 0);
}

static UINT32 __stdcall ScanThreadProc(void* pUser)
{
	CDScanContext* pContext = (CDScanContext*)pUser;
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
	pContext->nStatus = CDLibraryScan(szNeoCDGamesDir, bNeoCDListScanSub, ScanCancelled, ScanProgress, pContext, &pContext->pResult);
	PostMessage(pContext->hDialog, WM_CDLIST_COMPLETE, pContext->nGeneration, 0);
	return 0;
}

static void FinishScanThread(CDScanContext* pContext)
{
	if (!pContext)
		return;
	if (pContext->hThread) {
		WaitForSingleObject(pContext->hThread, INFINITE);
		CloseHandle(pContext->hThread);
		pContext->hThread = NULL;
	}
	if (pContext->hCancelEvent) {
		CloseHandle(pContext->hCancelEvent);
		pContext->hCancelEvent = NULL;
	}
}

static INT_PTR CALLBACK ScanWaitProc(HWND hDlg, UINT nMessage, WPARAM wParam, LPARAM)
{
	CDScanContext* pContext = pActiveScan;
	switch (nMessage) {
		case WM_INITDIALOG:
			if (!pContext) {
				EndDialog(hDlg, 0);
				return TRUE;
			}
			pContext->hDialog = hDlg;
			ShowWindow(GetDlgItem(hDlg, IDC_WAIT_LABEL_A), TRUE);
			SetWindowText(GetDlgItem(hDlg, IDC_WAIT_LABEL_A), FBALoadStringEx(hAppInst, IDS_SCANNING_IMGS, true));
			ShowWindow(GetDlgItem(hDlg, IDCANCEL), TRUE);
			SendDlgItemMessage(hDlg, IDC_WAIT_PROG, PBM_SETRANGE32, 0, 1);

			pContext->hThread = (HANDLE)_beginthreadex(NULL, 0, ScanThreadProc, pContext, 0, NULL);
			if (!pContext->hThread) {
				pContext->nStatus = CDLIBRARY_SCAN_OUT_OF_MEMORY;
				PostMessage(hDlg, WM_CDLIST_COMPLETE, pContext->nGeneration, 0);
			}
			WndInMid(hDlg, hNeoCDWnd ? hNeoCDWnd : hScrnWnd);
			return TRUE;

		case WM_CDLIST_PROGRESS:
			if (!pContext || (UINT32)wParam != pContext->nGeneration)
				return TRUE;
			{
				InterlockedExchange(&pContext->nProgressPending, 0);
				LONG nTotal     = InterlockedCompareExchange(&pContext->nTotal,     0, 0);
				LONG nCompleted = InterlockedCompareExchange(&pContext->nCompleted, 0, 0);
				if (nTotal < 1)
					nTotal = 1;
				if (nCompleted > nTotal)
					nCompleted = nTotal;

				SendDlgItemMessage(hDlg, IDC_WAIT_PROG, PBM_SETRANGE32, 0, nTotal);
				SendDlgItemMessage(hDlg, IDC_WAIT_PROG, PBM_SETPOS, nCompleted, 0);
			}
			return TRUE;

		case WM_CDLIST_COMPLETE:
			if (!pContext || (UINT32)wParam != pContext->nGeneration)
				return TRUE;

			FinishScanThread(pContext);
			EndDialog(hDlg, pContext->nStatus == CDLIBRARY_SCAN_OK ? 1 : 0);
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) != IDCANCEL)
				break;
			/* fall through */
		case WM_CLOSE:
			if (pContext && pContext->hCancelEvent)
				SetEvent(pContext->hCancelEvent);

			EnableWindow(GetDlgItem(hDlg, IDCANCEL), FALSE);
			SetWindowText(GetDlgItem(hDlg, IDC_WAIT_LABEL_A), _T("Cancelling..."));
			return TRUE;
	}
	return FALSE;
}

static void ScanLibrary()
{
	CDScanContext Context;
	memset(&Context, 0, sizeof(Context));
	Context.nGeneration  = ++nScanGeneration;
	Context.nStatus      = CDLIBRARY_SCAN_INVALID_ARGUMENT;
	Context.hCancelEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!Context.hCancelEvent)
		return;

	pActiveScan = &Context;
	INT_PTR nAccepted = FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_WAIT), hNeoCDWnd ? hNeoCDWnd : hScrnWnd, (DLGPROC)ScanWaitProc);
	pActiveScan = NULL;
	FinishScanThread(&Context);
	if (nAccepted && Context.nStatus == CDLIBRARY_SCAN_OK && Context.pResult) {
		CDLibraryFree(pCDLibrary);
		pCDLibrary = Context.pResult;
		Context.pResult = NULL;
		PopulateList();
	}
	CDLibraryFree(Context.pResult);
}

static INT_PTR CALLBACK CoverWndProc(HWND hDlg, UINT nMessage, WPARAM wParam, LPARAM)
{
	if (nMessage == WM_INITDIALOG) {
		NeoCDList_ShowPreviewBuf(hDlg, pZoomPng, nZoomPngSize, IDC_NCD_COVER_PREVIEW_PIC, IDC_NCD_COVER_PREVIEW_PIC, 580, 415);
		pZoomPng     = NULL;
		nZoomPngSize = 0;
		return TRUE;
	}
	if (nMessage == WM_CLOSE || (nMessage == WM_COMMAND && LOWORD(wParam) == IDCANCEL)) {
		EndDialog(hDlg, 0);
		return TRUE;
	}
	return FALSE;
}

static void ShowZoom(INT32 nSide)
{
	const CDLibraryEntry* pEntry = GetLibraryEntry(nSelectedItem);
	if (!pEntry) return;
	pZoomPng = NULL;
	nZoomPngSize = 0;
	LoadEntryPreview(pEntry, nSide, &pZoomPng, &nZoomPngSize);
	if (pZoomPng && nZoomPngSize)
		FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_NCD_COVER_DLG), hNeoCDWnd, (DLGPROC)CoverWndProc);

	free_s(&pZoomPng);
	nZoomPngSize = 0;
}

static bool SelectForPlay()
{
	const CDLibraryEntry* pEntry = GetLibraryEntry(nSelectedItem);
	if (!pEntry) {
		MessageBox(hNeoCDWnd, _T("Select a CD image first."), _T(APP_TITLE), MB_OK | MB_ICONWARNING);
		return false;
	}
#if !defined(BUILD_NEOGEO)
	if (pEntry->nPlatform != CDLIST_PLATFORM_PCECD) {
#elif !defined(BUILD_PCE)
	if (pEntry->nPlatform != CDLIST_PLATFORM_NEOCD) {
#else
	if (pEntry->nPlatform != CDLIST_PLATFORM_NEOCD && pEntry->nPlatform != CDLIST_PLATFORM_PCECD) {
#endif
		MessageBox(hNeoCDWnd, _T("The required CD driver is unavailable in this build."), _T(APP_TITLE), MB_OK | MB_ICONERROR);
		return false;
	}
#ifdef BUILD_NEOGEO
	if (pEntry->nPlatform == CDLIST_PLATFORM_NEOCD && BurnDrvGetIndex((char*)"neocdz") < 0) {
		MessageBox(hNeoCDWnd, _T("The Neo Geo CD driver is unavailable."), _T(APP_TITLE), MB_OK | MB_ICONERROR);
		return false;
	}
#endif
#ifdef BUILD_PCE
	if (pEntry->nPlatform == CDLIST_PLATFORM_PCECD && BurnDrvGetIndex((char*)"pce_scdsys") < 0) {
		MessageBox(hNeoCDWnd, _T("The PCE CD driver is unavailable."), _T(APP_TITLE), MB_OK | MB_ICONERROR);
		return false;
	}
#endif
	DWORD nAttributes = pEntry->szPath[0] ? GetFileAttributes(pEntry->szPath) : INVALID_FILE_ATTRIBUTES;
	if (nAttributes == INVALID_FILE_ATTRIBUTES || (nAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
		MessageBox(hNeoCDWnd, _T("The selected CD image path is no longer valid."), _T(APP_TITLE), MB_OK | MB_ICONERROR);
		return false;
	}
	nCDEmuSelect = 0;
	_tcsncpy(CDEmuImage, pEntry->szPath, MAX_PATH - 1);
	CDEmuImage[MAX_PATH - 1] = _T('\0');
	nRunPlatform = pEntry->nPlatform;
	return true;
}

static void CleanupDialog()
{
	if (pActiveScan && pActiveScan->hCancelEvent) {
		SetEvent(pActiveScan->hCancelEvent);
		FinishScanThread(pActiveScan);
	}
	CDLibraryFree(pCDLibrary);
	pCDLibrary = NULL;
	for (INT32 i = 0; i < (INT32)ARRAY_SIZE(hCoverBMPs); i++) {
		if (hCoverBMPs[i])
			DeleteObject(hCoverBMPs[i]);
		hCoverBMPs[i] = NULL;
	}
	if (hCDImageList)
		ImageList_Destroy(hCDImageList);
	if (hWhiteBGBrush)
		DeleteObject(hWhiteBGBrush);

	hCDImageList  = NULL;
	hWhiteBGBrush = NULL;
	nSelectedItem = -1;
	hListView     = NULL;
	hNeoCDWnd     = NULL;
}

static INT_PTR CALLBACK NeoCDList_WndProc(HWND hDlg, UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	static HICON hDialogIcon = NULL;
	switch (nMessage) {
		case WM_INITDIALOG: {
			hNeoCDWnd     = hDlg;
			hListView     = GetDlgItem(hDlg, IDC_NCD_LIST);
			nRunPlatform  = CDLIST_PLATFORM_UNKNOWN;
			InitListView();
			hWhiteBGBrush = CreateSolidBrush(RGB(255, 255, 255));
			hDialogIcon   = LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_APP));
			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hDialogIcon);
			CheckDlgButton(hDlg, IDC_NCD_SSUBDIR_CHECK, bNeoCDListScanSub ? BST_CHECKED : BST_UNCHECKED);
			TCHAR szTitle[200];
			_sntprintf(szTitle, ARRAY_SIZE(szTitle), FBALoadStringEx(hAppInst, IDS_NGCD_DIAG_TITLE, true), _T(APP_TITLE), _T(SEPERATOR_1), _T(SEPERATOR_1));
			szTitle[ARRAY_SIZE(szTitle) - 1] = _T('\0');
			SetWindowText(hDlg, szTitle);
			WndInMid(hDlg, hScrnWnd);
			ClearSelectionUI();
			ScanLibrary();
			SetFocus(hListView);
			return TRUE;
		}

		case WM_CLOSE: {
			INT32 nPlatform = nRunPlatform;
			CleanupDialog();
			EndDialog(hDlg, 0);
			if (hDialogIcon) {
				DestroyIcon(hDialogIcon);
				hDialogIcon = NULL;
			}
#ifdef BUILD_NEOGEO
			if (nPlatform == CDLIST_PLATFORM_NEOCD) BurnerLoadDriver(_T("neocdz"));
#endif
#ifdef BUILD_PCE
			if (nPlatform == CDLIST_PLATFORM_PCECD) BurnerLoadDriver(_T("pce_scdsys"));
#endif
			return TRUE;
		}

		case WM_DESTROY:
			if (hNeoCDWnd)
				CleanupDialog();
			if (hDialogIcon) {
				DestroyIcon(hDialogIcon);
				hDialogIcon = NULL;
			}
			return TRUE;

		case WM_CTLCOLORSTATIC:
			if ((HWND)lParam == GetDlgItem(hDlg, IDC_NCD_LABELSHORT) || (HWND)lParam == GetDlgItem(hDlg, IDC_NCD_LABELPUBLISHER) ||
				(HWND)lParam == GetDlgItem(hDlg, IDC_NCD_LABELIMAGE) || (HWND)lParam == GetDlgItem(hDlg, IDC_NCD_LABELAUDIO    ) ||
				(HWND)lParam == GetDlgItem(hDlg, IDC_NCD_TEXTSHORT ) || (HWND)lParam == GetDlgItem(hDlg, IDC_NCD_TEXTPUBLISHER ) ||
				(HWND)lParam == GetDlgItem(hDlg, IDC_NCD_TEXTIMAGE ) || (HWND)lParam == GetDlgItem(hDlg, IDC_NCD_TEXTAUDIO))
				return (INT_PTR)hWhiteBGBrush;
			break;

		case WM_NOTIFY: {
			NMHDR* pHeader = (NMHDR*)lParam;
			if (!pHeader || pHeader->idFrom != IDC_NCD_LIST)
				break;
			if (pHeader->code == LVN_GETINFOTIP) {
				LPNMLVGETINFOTIP pTip = (LPNMLVGETINFOTIP)pHeader;
				if (!pTip->pszText || pTip->cchTextMax <= 0)
					return TRUE;

				memset(pTip->pszText, 0, (size_t)pTip->cchTextMax * sizeof(TCHAR));
				LVITEM Item;
				memset(&Item, 0, sizeof(Item));
				Item.mask  = LVIF_PARAM;
				Item.iItem = pTip->iItem;
				if (pTip->iItem >= 0 && ListView_GetItem(hListView, &Item)) {
					const CDLibraryEntry* pEntry = GetLibraryEntry((INT32)(INT_PTR)Item.lParam);
					if (pEntry) {
						_tcsncpy(pTip->pszText, pEntry->szPath, pTip->cchTextMax - 1);
						pTip->pszText[pTip->cchTextMax - 1] = _T('\0');
					}
				}
				return TRUE;
			}
			NMLISTVIEW* pList = (NMLISTVIEW*)pHeader;
			if (pHeader->code == LVN_ITEMCHANGED) {
				if (!(pList->uChanged & LVIF_STATE))
					return TRUE;

				if (!(pList->uNewState & LVIS_SELECTED)) {
					if (!ListView_GetSelectedCount(hListView)) ClearSelectionUI();
					return TRUE;
				}

				LVITEM Item;
				memset(&Item, 0, sizeof(Item));
				Item.mask  = LVIF_PARAM;
				Item.iItem = pList->iItem;
				if (pList->iItem >= 0 && ListView_GetItem(hListView, &Item))
					ShowEntryDetails((INT32)(INT_PTR)Item.lParam);

				return TRUE;
			}
			if (pHeader->code == LVN_COLUMNCLICK) {
				if (nSortColumn == pList->iSubItem)
					nSortDescending ^= 1;
				else {
					nSortColumn = pList->iSubItem;
					nSortDescending = 0;
				}
				PopulateList();
				return TRUE;
			}
			if (pHeader->code == NM_DBLCLK) {
				NMITEMACTIVATE* pActivate = (NMITEMACTIVATE*)pHeader;
				if (pActivate->iItem >= 0 && (ListView_GetItemState(hListView, pActivate->iItem, LVIS_SELECTED) & LVIS_SELECTED) && SelectForPlay())
					PostMessage(hDlg, WM_CLOSE, 0, 0);
				return TRUE;
			}
			break;
		}

		case WM_COMMAND:
			if (HIWORD(wParam) == STN_CLICKED) {
				if (LOWORD(wParam) == IDC_NCD_FRONT_PIC) {
					ShowZoom(0);
					return TRUE;
				}
				if (LOWORD(wParam) == IDC_NCD_BACK_PIC) {
					ShowZoom(1);
					return TRUE;
				}
			}
			if (HIWORD(wParam) == BN_CLICKED) {
				switch (LOWORD(wParam)) {
					case IDC_NCD_PLAY_BUTTON:
						if (SelectForPlay())
							PostMessage(hDlg, WM_CLOSE, 0, 0);
						return TRUE;
					case IDC_NCD_SCAN_BUTTON:
						ClearSelectionUI();
						ScanLibrary();
						SetFocus(hListView);
						return TRUE;
					case IDC_NCD_SEL_DIR_BUTTON: {
						TCHAR szPrevious[MAX_PATH];
						_tcsncpy(szPrevious, szNeoCDGamesDir, MAX_PATH - 1);
						szPrevious[MAX_PATH - 1] = _T('\0');
						SupportDirCreate(hDlg);
						if (_tcsicmp(szPrevious, szNeoCDGamesDir)) {
							ClearSelectionUI();
							ScanLibrary();
						}
						SetFocus(hListView);
						return TRUE;
					}
					case IDC_NCD_SSUBDIR_CHECK:
						bNeoCDListScanSub = IsDlgButtonChecked(hDlg, IDC_NCD_SSUBDIR_CHECK) == BST_CHECKED;
						SetFocus(hListView);
						return TRUE;
					case IDCANCEL:
						PostMessage(hDlg, WM_CLOSE, 0, 0);
						return TRUE;
				}
			}
			break;
	}
	return FALSE;
}

INT32 NeoCDList_Init()
{
	return FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_NCD_DLG), hScrnWnd, (DLGPROC)NeoCDList_WndProc);
}

#else

INT32 NeoCDList_Init()
{
	return 0;
}

#endif
