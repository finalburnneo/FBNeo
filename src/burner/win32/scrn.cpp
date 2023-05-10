// Screen Window
#include "burner.h"
#include <shlobj.h>

#define		HORIZONTAL_ORIENTED_RES		0
#define		VERTICAL_ORIENTED_RES		1

int nActiveGame;

static bool bLoading = 0;

int OnMenuSelect(HWND, HMENU, int, HMENU, UINT);
int OnInitMenuPopup(HWND, HMENU, UINT, BOOL);
int OnUnInitMenuPopup(HWND, HMENU, UINT, BOOL);
void DisplayPopupMenu(int nMenu);

RECT SystemWorkArea = { 0, 0, 640, 480 };				// Work area on the desktop
int nWindowPosX = -1, nWindowPosY = -1;					// Window position

int bAutoPause = 1;

bool bMenuEnabled = true;
bool bHasFocus = false;

int nSavestateSlot = 1;

static TCHAR* szClass = _T("FinalBurn Neo");					// Window class name
HWND hScrnWnd = NULL;									// Handle to the screen window
HWND hRebar = NULL;										// Handle to the Rebar control containing the menu

static bool bMaximized;
static int nPrevWidth, nPrevHeight;

static int bBackFromHibernation = 0;

#define ID_NETCHAT 999
HWND hwndChat = NULL;
WNDPROC pOldWndProc = NULL;

bool bRescanRoms = false;

static bool bDrag = false;
static int nDragX, nDragY;
static int nOldWindowX, nOldWindowY;
static int nLeftButtonX, nLeftButtonY;

static int OnCreate(HWND, LPCREATESTRUCT);
static void OnActivateApp(HWND, BOOL, DWORD);
static void OnPaint(HWND);
static void OnClose(HWND);
static void OnDestroy(HWND);
static void OnCommand(HWND, int, HWND, UINT);
static int OnSysCommand(HWND, UINT, int, int);
static void OnSize(HWND, UINT, int, int);
static void OnEnterSizeMove(HWND);
static void OnExitSizeMove(HWND);
static void OnEnterIdle(HWND, UINT, HWND);
static void OnEnterMenuLoop(HWND, BOOL);
static void OnExitMenuLoop(HWND, BOOL);
static int OnMouseMove(HWND, int, int, UINT);
static int OnLButtonUp(HWND, int, int, UINT);
static int OnLButtonDown(HWND, BOOL, int, int, UINT);
static int OnLButtonDblClk(HWND, BOOL, int, int, UINT);
static int OnRButtonUp(HWND, int, int, UINT);
static int OnRButtonDown(HWND, BOOL, int, int, UINT);

static int OnDisplayChange(HWND, UINT, UINT, UINT);

int OnNotify(HWND, int, NMHDR* lpnmhdr);

static bool UseDialogs()
{
	if (/*!bDrvOkay ||*/ !nVidFullscreen) {
		return true;
	}

	return false;
}

void SetPauseMode(bool bPause)
{
	bRunPause = bPause;
	bAltPause = bPause;

	if (bPause) {
		AudBlankSound();
		if (UseDialogs()) {
			InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
		}
	} else {
		GameInpCheckMouse();
	}
}

static char* CreateKailleraList()
{
	unsigned int nOldDrvSelect = nBurnDrvActive;
	int nSize = 256 * 1024;
	char* pList = (char*)malloc(nSize);
	char* pName = pList;

	if (pList == NULL) {
		return NULL;
	}

	// Add chat option to the gamelist
	pName += sprintf(pName, "* Chat only");
	pName++;

	// Put games in the Favorites list at the top of the list.
	{ // (don't check avOk, this should work even if romlist isn't scanned! -dink)
		LoadFavorites();

		for (nBurnDrvActive = 0; nBurnDrvActive < nBurnDrvCount; nBurnDrvActive++) {
			if (CheckFavorites(BurnDrvGetTextA(DRV_NAME)) != -1) {
				char* szDecoratedName = DecorateGameName(nBurnDrvActive);

				if (pName + strlen(szDecoratedName) >= pList + nSize) {
					char* pNewList;
					nSize <<= 1;
					pNewList = (char*)realloc(pList, nSize);
					if (pNewList == NULL) {
						return NULL;
					}
					pName -= (INT_PTR)pList;
					pList = pNewList;
					pName += (INT_PTR)pList;
				}
				pName += sprintf(pName, "%s", szDecoratedName);
				pName++;
			}
		}
	}

	if (avOk) {
		// Add all the driver names to the list
		for (nBurnDrvActive = 0; nBurnDrvActive < nBurnDrvCount; nBurnDrvActive++) {
			if(BurnDrvGetFlags() & BDF_GAME_WORKING && gameAv[nBurnDrvActive]) {
				char* szDecoratedName = DecorateGameName(nBurnDrvActive);

				if (pName + strlen(szDecoratedName) >= pList + nSize) {
					char* pNewList;
					nSize <<= 1;
					pNewList = (char*)realloc(pList, nSize);
					if (pNewList == NULL) {
						return NULL;
					}
					pName -= (INT_PTR)pList;
					pList = pNewList;
					pName += (INT_PTR)pList;
				}
				pName += sprintf(pName, "%s", szDecoratedName);
				pName++;
			}
		}
	}

	*pName = '\0';
	pName++;

	nBurnDrvActive = nOldDrvSelect;

	return pList;
}

void DeActivateChat()
{
	bEditActive = false;
	DestroyWindow(hwndChat);
	hwndChat = NULL;
}

int ActivateChat()
{
	RECT rect;
	GetClientRect(hScrnWnd, &rect);

	DeActivateChat();

	// Create an invisible edit control
	hwndChat = CreateWindow(
		_T("EDIT"), NULL,
		WS_CHILD | ES_LEFT,
		0, rect.bottom - 32, rect.right, 32,
		hScrnWnd, (HMENU)ID_NETCHAT, (HINSTANCE)GetWindowLongPtr(hScrnWnd, GWLP_HINSTANCE), NULL);                // pointer not needed

	EditText[0] = 0;
	bEditTextChanged = true;
	bEditActive = true;

	SendMessage(hwndChat, EM_LIMITTEXT, MAX_CHAT_SIZE, 0);			// Limit the amount of text

	SetFocus(hwndChat);

	return 0;
}

INT32 is_netgame_or_recording() // returns: 1 = netgame, 2 = recording/playback
{
	return (kNetGame | (movieFlags & (1<<1))); // netgame or recording from power_on
}

static int WINAPI gameCallback(char* game, int player, int numplayers)
{
	bool bFound = false;
	HWND hActive;

	for (nBurnDrvActive = 0; nBurnDrvActive < nBurnDrvCount; nBurnDrvActive++) {

		char* szDecoratedName = DecorateGameName(nBurnDrvActive);

		if (!strcmp(szDecoratedName, game)) {
			bFound = true;
			break;
		}
	}

	if (!bFound) {
		Kaillera_End_Game();
		return 1;
	}

	kNetGame = 1;
	hActive = GetActiveWindow();

	bCheatsAllowed = false;								// Disable cheats during netplay
	AudSoundStop();										// Stop while we load roms
	DrvInit(nBurnDrvActive, false);						// Init the game driver

	// w/Kaillera: DrvInit() can't post it's restart message because we're not in the message loop yet.
	// so we must MediaExit() / MediaInit() here.  -dink
	MediaExit();
	MediaInit();
	AudSoundPlay();										// Restart sound
	SetFocus(hScrnWnd);

//	dprintf(_T(" ** OSD startnet text sent.\n"));

	TCHAR szTemp1[256];
	TCHAR szTemp2[256];
	VidSAddChatMsg(FBALoadStringEx(hAppInst, IDS_NETPLAY_START, true), 0xFFFFFF, BurnDrvGetText(DRV_FULLNAME), 0xFFBFBF);
	_sntprintf(szTemp1, 256, FBALoadStringEx(hAppInst, IDS_NETPLAY_START_YOU, true), player);
	_sntprintf(szTemp2, 256, FBALoadStringEx(hAppInst, IDS_NETPLAY_START_TOTAL, true), numplayers);
	VidSAddChatMsg(szTemp1, 0xFFFFFF, szTemp2, 0xFFBFBF);

	RunMessageLoop();

	DrvExit();
	if (kNetGame) {
		kNetGame = 0;
		Kaillera_End_Game();
	}
	DeActivateChat();

	bCheatsAllowed = true;								// reenable cheats netplay has ended

	SetFocus(hActive);
	return 0;
}

static void WINAPI kChatCallback(char* nick, char* text)
{
	TCHAR szTemp[128];
	_sntprintf(szTemp, 128, _T("%.32hs "), nick);
	VidSAddChatMsg(szTemp, 0xBFBFFF, ANSIToTCHAR(text, NULL, 0), 0x7F7FFF);
}

static void WINAPI kDropCallback(char *nick, int playernb)
{
	TCHAR szTemp[128];
	_sntprintf(szTemp, 128, FBALoadStringEx(hAppInst, IDS_NETPLAY_DROP, true), playernb, nick);
	VidSAddChatMsg(szTemp, 0xFFFFFF, NULL, 0);
}

static void DoNetGame()
{
	kailleraInfos ki;
	char tmpver[128];
	char* gameList;

	if(bDrvOkay) {
		DrvExit();
		ScrnTitle();
	}
	MenuEnableItems();

#ifdef _UNICODE
	_snprintf(tmpver, 128, APP_TITLE " v%.20ls", szAppBurnVer);
#else
	_snprintf(tmpver, 128, APP_TITLE " v%.20s", szAppBurnVer);
#endif

	gameList = CreateKailleraList();

	ki.appName = tmpver;
	ki.gameList = gameList;
	ki.gameCallback = &gameCallback;
	ki.chatReceivedCallback = &kChatCallback;
	ki.clientDroppedCallback = &kDropCallback;
	ki.moreInfosCallback = NULL;

	Kaillera_Set_Infos(&ki);

	Kaillera_Select_Server_Dialog(NULL);

	if (gameList) {
		free(gameList);
		gameList = NULL;
	}

	End_Network();

	POST_INITIALISE_MESSAGE;
}

int CreateDatfileWindows(int bType)
{
	TCHAR szTitle[1024];
	TCHAR szFilter[1024];

	TCHAR szConsoleString[64];
	_sntprintf(szConsoleString, 64, _T(""));
	if (bType == DAT_MEGADRIVE_ONLY) _sntprintf(szConsoleString, 64, _T(", Megadrive only"));
	if (bType == DAT_PCENGINE_ONLY) _sntprintf(szConsoleString, 64, _T(", PC-Engine only"));
	if (bType == DAT_TG16_ONLY) _sntprintf(szConsoleString, 64, _T(", TurboGrafx16 only"));
	if (bType == DAT_SGX_ONLY) _sntprintf(szConsoleString, 64, _T(", SuprGrafx only"));
	if (bType == DAT_SG1000_ONLY) _sntprintf(szConsoleString, 64, _T(", Sega SG-1000 only"));
	if (bType == DAT_COLECO_ONLY) _sntprintf(szConsoleString, 64, _T(", ColecoVision only"));
	if (bType == DAT_MASTERSYSTEM_ONLY) _sntprintf(szConsoleString, 64, _T(", Master System only"));
	if (bType == DAT_GAMEGEAR_ONLY) _sntprintf(szConsoleString, 64, _T(", Game Gear only"));
	if (bType == DAT_MSX_ONLY) _sntprintf(szConsoleString, 64, _T(", MSX 1 Games only"));
	if (bType == DAT_SPECTRUM_ONLY) _sntprintf(szConsoleString, 64, _T(", ZX Spectrum Games only"));
	if (bType == DAT_NES_ONLY) _sntprintf(szConsoleString, 64, _T(", NES Games only"));
	if (bType == DAT_FDS_ONLY) _sntprintf(szConsoleString, 64, _T(", FDS Games only"));
	if (bType == DAT_NGP_ONLY) _sntprintf(szConsoleString, 64, _T(", NeoGeo Pocket Games only"));
	if (bType == DAT_CHANNELF_ONLY) _sntprintf(szConsoleString, 64, _T(", Fairchild Channel F Games only"));

	TCHAR szProgramString[25];
	_sntprintf(szProgramString, 25, _T("ClrMame Pro XML"));

	_sntprintf(szChoice, MAX_PATH, _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), szAppBurnVer, szProgramString, szConsoleString);
	_sntprintf(szTitle, 256, FBALoadStringEx(hAppInst, IDS_DAT_GENERATE, true), szProgramString);

	_stprintf(szFilter, FBALoadStringEx(hAppInst, IDS_DISK_ALL_DAT, true), _T(APP_TITLE));
	memcpy(szFilter + _tcslen(szFilter), _T(" (*.dat)\0*.dat\0\0"), 16 * sizeof(TCHAR));

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

	if (GetSaveFileName(&ofn) == 0)
		return -1;

	return create_datfile(szChoice, bType);
}

int CreateAllDatfilesWindows()
{
	INT32 nRet = 0;

	LPMALLOC pMalloc = NULL;
	BROWSEINFO bInfo;
	ITEMIDLIST* pItemIDList = NULL;
	TCHAR buffer[MAX_PATH];
	TCHAR szFilename[MAX_PATH];
	TCHAR szProgramString[25];

	_sntprintf(szProgramString, 25, _T("ClrMame Pro XML"));

	SHGetMalloc(&pMalloc);

	memset(&bInfo, 0, sizeof(bInfo));
	bInfo.hwndOwner = hScrnWnd;
	bInfo.pszDisplayName = buffer;
	bInfo.lpszTitle = FBALoadStringEx(hAppInst, IDS_ROMS_SELECT_DIR, true);
	bInfo.ulFlags = BIF_EDITBOX | BIF_RETURNONLYFSDIRS;

	pItemIDList = SHBrowseForFolder(&bInfo);

	if (!pItemIDList) {	// User clicked 'Cancel'
		pMalloc->Release();
		return nRet;
	}

	if (!SHGetPathFromIDList(pItemIDList, buffer)) {	// Browse dialog returned non-filesystem path
		pMalloc->Free(pItemIDList);
		pMalloc->Release();
		return nRet;
	}

	int strLen = _tcslen(buffer);
	if (strLen) {
		if (buffer[strLen - 1] != _T('\\')) {
			buffer[strLen]		= _T('\\');
			buffer[strLen + 1]	= _T('\0');
		}
	}

	pMalloc->Free(pItemIDList);
	pMalloc->Release();

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(""));
	create_datfile(szFilename, DAT_ARCADE_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", Megadrive only"));
	create_datfile(szFilename, DAT_MEGADRIVE_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", PC-Engine only"));
	create_datfile(szFilename, DAT_PCENGINE_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", TurboGrafx16 only"));
	create_datfile(szFilename, DAT_TG16_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", SuprGrafx only"));
	create_datfile(szFilename, DAT_SGX_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", Sega SG-1000 only"));
	create_datfile(szFilename, DAT_SG1000_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", ColecoVision only"));
	create_datfile(szFilename, DAT_COLECO_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", Master System only"));
	create_datfile(szFilename, DAT_MASTERSYSTEM_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", Game Gear only"));
	create_datfile(szFilename, DAT_GAMEGEAR_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", MSX 1 Games only"));
	create_datfile(szFilename, DAT_MSX_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", ZX Spectrum Games only"));
	create_datfile(szFilename, DAT_SPECTRUM_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", NES Games only"));
	create_datfile(szFilename, DAT_NES_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", FDS Games only"));
	create_datfile(szFilename, DAT_FDS_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", Neo Geo Pocket Games only"));
	create_datfile(szFilename, DAT_NGP_ONLY);

	_sntprintf(szFilename, MAX_PATH, _T("%s") _T(APP_TITLE) _T(" v%.20s (%s%s).dat"), buffer, szAppBurnVer, szProgramString, _T(", Fairchild Channel F Games only"));
	create_datfile(szFilename, DAT_CHANNELF_ONLY);

	return nRet;
}

// Returns true if a VidInit is needed when the window is resized
static bool VidInitNeeded()
{
	// D3D blitter needs to re-initialise only when auto-size RGB effects are enabled
	if (nVidSelect == 1 && (nVidBlitterOpt[nVidSelect] & 0x00030000) == 0x00030000) {
		return true;
	}
	if (nVidSelect == 3) {
		return true;
	}

	return false;
}

// Refresh the contents of the window when re-sizing it
static void RefreshWindow(bool bInitialise)
{
	if (nVidFullscreen) {
		return;
	}

	if (bInitialise && VidInitNeeded()) {
		VidInit();
		if (bVidOkay && (bRunPause || !bDrvOkay)) {
			VidRedraw();
			VidPaint(0);
		}
	}
}

static LRESULT CALLBACK ScrnProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg) {
		HANDLE_MSG(hWnd, WM_CREATE,			OnCreate);
		HANDLE_MSG(hWnd, WM_ACTIVATEAPP,	OnActivateApp);
		HANDLE_MSGB(hWnd,WM_PAINT,			OnPaint);
		HANDLE_MSG(hWnd, WM_CLOSE,			OnClose);
		HANDLE_MSG(hWnd, WM_DESTROY,		OnDestroy);
		HANDLE_MSG(hWnd, WM_COMMAND,		OnCommand);

		// We can't use the macro from windowsx.h macro for this one
		case WM_SYSCOMMAND: {
			if (OnSysCommand(hWnd,(UINT)wParam,(int)(short)LOWORD(lParam),(int)(short)HIWORD(lParam))) {
				return 0;
			}
			break;
		}
		// - dink - handle return from Hibernation
		case WM_POWERBROADCAST: {
			if (wParam == PBT_APMRESUMESUSPEND || wParam == PBT_APMSUSPEND) {
				bBackFromHibernation = 1;
			}
			break;
		}
		// - dink - end
		HANDLE_MSG(hWnd, WM_SIZE,			OnSize);
		HANDLE_MSG(hWnd, WM_ENTERSIZEMOVE,	OnEnterSizeMove);
		HANDLE_MSG(hWnd, WM_EXITSIZEMOVE,	OnExitSizeMove);
		HANDLE_MSG(hWnd, WM_ENTERIDLE,		OnEnterIdle);

		HANDLE_MSG(hWnd, WM_MOUSEMOVE,		OnMouseMove);
		HANDLE_MSG(hWnd, WM_LBUTTONUP,		OnLButtonUp);
		HANDLE_MSG(hWnd, WM_LBUTTONDOWN,	OnLButtonDown);

		HANDLE_MSG(hWnd, WM_LBUTTONDBLCLK,	OnLButtonDblClk);

		HANDLE_MSG(hWnd, WM_RBUTTONUP,		OnRButtonUp);
		HANDLE_MSG(hWnd, WM_RBUTTONDBLCLK,	OnRButtonDown);
		HANDLE_MSG(hWnd, WM_RBUTTONDOWN,	OnRButtonDown);

		HANDLE_MSG(hWnd, WM_NOTIFY,			OnNotify);
		HANDLE_MSG(hWnd, WM_MENUSELECT,		OnMenuSelect);
		HANDLE_MSG(hWnd, WM_ENTERMENULOOP,	OnEnterMenuLoop);
		HANDLE_MSGB(hWnd,WM_EXITMENULOOP,	OnExitMenuLoop);
		HANDLE_MSGB(hWnd,WM_INITMENUPOPUP,	OnInitMenuPopup);
		HANDLE_MSG(hWnd, WM_UNINITMENUPOPUP,OnUnInitMenuPopup);

		HANDLE_MSG(hWnd, WM_DISPLAYCHANGE,	OnDisplayChange);
	}

	return DefWindowProc(hWnd, Msg, wParam, lParam);
}

static int OnDisplayChange(HWND, UINT, UINT, UINT)
{
	if (!nVidFullscreen) {
		POST_INITIALISE_MESSAGE;
	}

	return 0;
}

/******************************************************************************/
/*	Fixed right click mouse events, now they work properly without interfering
	with other applications in the background [CaptainCPS-X]				  */
//----------------------------------------------------------------------------//
bool bRDblClick = false;

static int OnRButtonDown(HWND hwnd, BOOL bDouble, int, int, UINT)
{
	if (hwnd != hScrnWnd && !nVidFullscreen) return 1;

	if (bDouble) {
		// Right double-click at fullcreen happened, turn this flag 'true' so
		// when right double-click ends (OnRButtonUp) it doesn't do anything else.
		bRDblClick = true;
	}
	return 1;
}

static int OnRButtonUp(HWND hwnd, int, int, UINT)
{
	// If fullscreen and right double-click
	if (hwnd == hScrnWnd && nVidFullscreen && bRDblClick)
	{
		// game running
		if (bDrvOkay)
		{
			// toggle fullscreen
			nVidFullscreen = !nVidFullscreen;
			bRDblClick = false;
			POST_INITIALISE_MESSAGE;
			return 0;
		}
	}

	// If not fullscreen and this event is not related to 'toggle fullscreen' right double-click event
	if (!nVidFullscreen && !bRDblClick) {
		bMenuEnabled = !bMenuEnabled;
		POST_INITIALISE_MESSAGE;
		return 0;
	}

	return 1;
}
/*************************************************************************/

static int OnMouseMove(HWND hwnd, int x, int y, UINT keyIndicators)
{
	if (bDrag && hwnd == hScrnWnd && keyIndicators == MK_LBUTTON && !nVidFullscreen && !bMenuEnabled) {
		RECT clientRect;

		GetWindowRect(hScrnWnd, &clientRect);

		if ((nLeftButtonX - (clientRect.left + x)) < nDragX && (nLeftButtonX - (clientRect.left + x)) > -nDragX && (nLeftButtonY - (clientRect.top + y)) < nDragY && (nLeftButtonY - (clientRect.top + y)) > -nDragY) {
			SetWindowPos(hScrnWnd, NULL, nOldWindowX, nOldWindowY, 0, 0, SWP_NOREPOSITION | SWP_NOSIZE);
		} else {
			nWindowPosX = nOldWindowX - (nLeftButtonX - (clientRect.left + x));
			nWindowPosY = nOldWindowY - (nLeftButtonY - (clientRect.top + y));

			SetWindowPos(hScrnWnd, NULL, nWindowPosX, nWindowPosY, 0, 0, SWP_NOREPOSITION | SWP_NOSIZE);
		}

		return 0;
	}

	return 1;
}

static int OnLButtonDown(HWND hwnd, BOOL, int x, int y, UINT)
{
	if (hwnd == hScrnWnd && !nVidFullscreen && !bMenuEnabled) {
		RECT clientRect;

		GetWindowRect(hScrnWnd, &clientRect);

		nOldWindowX = clientRect.left;
		nOldWindowY = clientRect.top;

		nLeftButtonX = clientRect.left + x;
		nLeftButtonY = clientRect.top + y;

		bDrag = true;

		return 0;
	}

	return 1;
}

static int OnLButtonUp(HWND hwnd, int x, int y, UINT)
{
	bDrag = false;

	if (nVidFullscreen) {

		if (hwnd != hScrnWnd) {
			return 1;
		}

		if (UseDialogs()) {
			RECT clientRect;
			GetWindowRect(hScrnWnd, &clientRect);

			TrackPopupMenuEx(hMenuPopup, TPM_LEFTALIGN | TPM_TOPALIGN, clientRect.left + x, clientRect.top + y, hScrnWnd, NULL);
			return 0;
		}
	} else {
		if (!bMenuEnabled) {
			RECT clientRect;
			GetWindowRect(hScrnWnd, &clientRect);

			if ((nLeftButtonX - (clientRect.left + x)) < nDragX && (nLeftButtonX - (clientRect.left + x)) > -nDragX && (nLeftButtonY - (clientRect.top + y)) < nDragY && (nLeftButtonY - (clientRect.top + y)) > -nDragY) {
				TrackPopupMenuEx(hMenuPopup, TPM_LEFTALIGN | TPM_TOPALIGN, clientRect.left + x, clientRect.top + y, hScrnWnd, NULL);
				return 0;
			}
		}
	}

	return 1;
}

static int OnLButtonDblClk(HWND hwnd, BOOL, int, int, UINT)
{
	if (bDrvOkay) {
		nVidFullscreen = !nVidFullscreen;
		POST_INITIALISE_MESSAGE;
		return 0;
	}

	return 1;
}

static int OnCreate(HWND, LPCREATESTRUCT)	// HWND hwnd, LPCREATESTRUCT lpCreateStruct
{
	return 1;
}

static void OnActivateApp(HWND hwnd, BOOL fActivate, DWORD /* dwThreadId */)
{
	bHasFocus = fActivate;
	if (!kNetGame && bAutoPause && !bAltPause && hInpdDlg == NULL && hInpCheatDlg == NULL && hInpDIPSWDlg == NULL) {
		bRunPause = fActivate? 0 : 1;
	}
	if (fActivate == false && hwnd == hScrnWnd) {
		EndMenu();
	}
	if (fActivate == false && bRunPause) {
		AudBlankSound();
	}

	if (fActivate) {
		if (hInpdDlg || hInpCheatDlg || hInpDIPSWDlg || hDbgDlg) {
			InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
		} else {
			GameInpCheckMouse();
		}
	}
}

extern HWND hSelDlg;

void PausedRedraw(void)
{
    if (bVidOkay && bRunPause && bDrvOkay && (hSelDlg == NULL)) { // Redraw the screen to show certain messages while paused. - dink
        INT16 *pBtemp = pBurnSoundOut;
        pBurnSoundOut = NULL;

		VidRedraw();
		VidPaint(0);

        pBurnSoundOut = pBtemp;
    }
}

static void OnPaint(HWND hWnd)
{
	if (hWnd == hScrnWnd)
	{
		VidPaint(1);

		if (bBackFromHibernation) {
			PausedRedraw(); // redraw game screen if paused and returning from hibernation - dink
			bBackFromHibernation = 0;
		}

		// draw menu
		if (!nVidFullscreen) {
			RedrawWindow(hRebar, NULL, NULL, RDW_FRAME | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

static void OnClose(HWND)
{
#ifdef INCLUDE_AVI_RECORDING
	AviStop();
#endif
    PostQuitMessage(0);					// Quit the program if the window is closed
}

static void OnDestroy(HWND)
{
    VidExit();							// Stop using video with the Window
    hScrnWnd = NULL;					// Make sure handle is not used again
}

OPENFILENAME	bgFn;
TCHAR			szFile[MAX_PATH];

static void UpdatePreviousGameList()
{
	int nRecentIdenticalTo = -1;

	// check if this game is identical to any of the listed in the recent menu
	for(int x = 0; x < SHOW_PREV_GAMES; x++) {
		if(!_tcscmp(BurnDrvGetText(DRV_NAME), szPrevGames[x])) {
			nRecentIdenticalTo = x;
		}
	}

	// Declare temporary array
	TCHAR szTmp[SHOW_PREV_GAMES][32];

	// Backup info for later use
	for(int x = 0; x < SHOW_PREV_GAMES; x++) {
		_tcscpy(szTmp[x], szPrevGames[x]);
	}

	switch(nRecentIdenticalTo)
	{
		case -1:
			// Normal rotation when recent game is not identical to any of the ones listed
			// - - -
			_tcscpy(szPrevGames[9], szPrevGames[8]);			// Recent 10 = 9
			_tcscpy(szPrevGames[8], szPrevGames[7]);			// Recent 9 = 8
			_tcscpy(szPrevGames[7], szPrevGames[6]);			// Recent 8 = 7
			_tcscpy(szPrevGames[6], szPrevGames[5]);			// Recent 7 = 6
			_tcscpy(szPrevGames[5], szPrevGames[4]);			// Recent 6 = 5
			_tcscpy(szPrevGames[4], szPrevGames[3]);			// Recent 5 = 4
			_tcscpy(szPrevGames[3], szPrevGames[2]);			// Recent 4 = 3
			_tcscpy(szPrevGames[2], szPrevGames[1]);			// Recent 3 = 2
			_tcscpy(szPrevGames[1], szPrevGames[0]);			// Recent 2 = 1
			_tcscpy(szPrevGames[0], BurnDrvGetText(DRV_NAME));	// Update most recent game played (Recent 1)
			break;
		case 0:
			break;												// Nothing Change
		case 1:
			_tcscpy(szPrevGames[0], szTmp[1]);					// Update most recent game played (Recent 1 = 2)
			_tcscpy(szPrevGames[1], szTmp[0]);					// Recent 2 = 1
			break;
		case 2:
			_tcscpy(szPrevGames[0], szTmp[2]);					// Update most recent game played (Recent 1 = 3)
			_tcscpy(szPrevGames[1], szTmp[0]);					// Recent 2 = 1
			_tcscpy(szPrevGames[2], szTmp[1]);					// Recent 3 = 2
			break;
		case 3:
			_tcscpy(szPrevGames[0], szTmp[3]);					// Update most recent game played (Recent 1 = 4)
			_tcscpy(szPrevGames[1], szTmp[0]);					// Recent 2 = 1
			_tcscpy(szPrevGames[2], szTmp[1]);					// Recent 3 = 2
			_tcscpy(szPrevGames[3], szTmp[2]);					// Recent 4 = 3
			break;
		case 4:
			_tcscpy(szPrevGames[0], szTmp[4]);					// Update most recent game played (Recent 1 = 5)
			_tcscpy(szPrevGames[1], szTmp[0]);					// Recent 2 = 1
			_tcscpy(szPrevGames[2], szTmp[1]);					// Recent 3 = 2
			_tcscpy(szPrevGames[3], szTmp[2]);					// Recent 4 = 3
			_tcscpy(szPrevGames[4], szTmp[3]);					// Recent 5 = 4
			break;
		case 5:
			_tcscpy(szPrevGames[0], szTmp[5]);					// Update most recent game played (Recent 1 = 6)
			_tcscpy(szPrevGames[1], szTmp[0]);					// Recent 2 = 1
			_tcscpy(szPrevGames[2], szTmp[1]);					// Recent 3 = 2
			_tcscpy(szPrevGames[3], szTmp[2]);					// Recent 4 = 3
			_tcscpy(szPrevGames[4], szTmp[3]);					// Recent 5 = 4
			_tcscpy(szPrevGames[5], szTmp[4]);					// Recent 6 = 5
			break;
		case 6:
			_tcscpy(szPrevGames[0], szTmp[6]);					// Update most recent game played (Recent 1 = 7)
			_tcscpy(szPrevGames[1], szTmp[0]);					// Recent 2 = 1
			_tcscpy(szPrevGames[2], szTmp[1]);					// Recent 3 = 2
			_tcscpy(szPrevGames[3], szTmp[2]);					// Recent 4 = 3
			_tcscpy(szPrevGames[4], szTmp[3]);					// Recent 5 = 4
			_tcscpy(szPrevGames[5], szTmp[4]);					// Recent 6 = 5
			_tcscpy(szPrevGames[6], szTmp[5]);					// Recent 7 = 6
			break;
		case 7:
			_tcscpy(szPrevGames[0], szTmp[7]);					// Update most recent game played (Recent 1 = 8)
			_tcscpy(szPrevGames[1], szTmp[0]);					// Recent 2 = 1
			_tcscpy(szPrevGames[2], szTmp[1]);					// Recent 3 = 2
			_tcscpy(szPrevGames[3], szTmp[2]);					// Recent 4 = 3
			_tcscpy(szPrevGames[4], szTmp[3]);					// Recent 5 = 4
			_tcscpy(szPrevGames[5], szTmp[4]);					// Recent 6 = 5
			_tcscpy(szPrevGames[6], szTmp[5]);					// Recent 7 = 6
			_tcscpy(szPrevGames[7], szTmp[6]);					// Recent 8 = 7
			break;
		case 8:
			_tcscpy(szPrevGames[0], szTmp[8]);					// Update most recent game played (Recent 1 = 9)
			_tcscpy(szPrevGames[1], szTmp[0]);					// Recent 2 = 1
			_tcscpy(szPrevGames[2], szTmp[1]);					// Recent 3 = 2
			_tcscpy(szPrevGames[3], szTmp[2]);					// Recent 4 = 3
			_tcscpy(szPrevGames[4], szTmp[3]);					// Recent 5 = 4
			_tcscpy(szPrevGames[5], szTmp[4]);					// Recent 6 = 5
			_tcscpy(szPrevGames[6], szTmp[5]);					// Recent 7 = 6
			_tcscpy(szPrevGames[7], szTmp[6]);					// Recent 8 = 7
			_tcscpy(szPrevGames[8], szTmp[7]);					// Recent 9 = 8
			break;
		case 9:
			_tcscpy(szPrevGames[0], szTmp[9]);					// Update most recent game played (Recent 1 = 10)
			_tcscpy(szPrevGames[1], szTmp[0]);					// Recent 2 = 1
			_tcscpy(szPrevGames[2], szTmp[1]);					// Recent 3 = 2
			_tcscpy(szPrevGames[3], szTmp[2]);					// Recent 4 = 3
			_tcscpy(szPrevGames[4], szTmp[3]);					// Recent 5 = 4
			_tcscpy(szPrevGames[5], szTmp[4]);					// Recent 6 = 5
			_tcscpy(szPrevGames[6], szTmp[5]);					// Recent 7 = 6
			_tcscpy(szPrevGames[7], szTmp[6]);					// Recent 8 = 7
			_tcscpy(szPrevGames[8], szTmp[7]);					// Recent 9 = 8
			_tcscpy(szPrevGames[9], szTmp[8]);					// Recent 10 = 9
			break;
	}
}

static bool bSramLoad = true; // always true, unless BurnerLoadDriver() is called from StartFromReset()

// Compact driver loading module
int BurnerLoadDriver(TCHAR *szDriverName)
{
	unsigned int j;

	int nOldDrvSelect = nBurnDrvActive;

#ifdef INCLUDE_AVI_RECORDING
	AviStop();
#endif

	DrvExit();
	bLoading = 1;

	for (j = 0; j < nBurnDrvCount; j++) {
		nBurnDrvActive = j;
		if (!_tcscmp(szDriverName, BurnDrvGetText(DRV_NAME)) && (!(BurnDrvGetFlags() & BDF_BOARDROM))) {
			nBurnDrvActive = nOldDrvSelect;
			nDialogSelect = nOldDlgSelected = j;
			SplashDestroy(1);
			StopReplay();

			DrvExit();
			DrvInit(j, bSramLoad);	// Init the game driver
			MenuEnableItems();
			bAltPause = 0;
			AudSoundPlay();			// Restart sound
			bLoading = 0;
			UpdatePreviousGameList();
			if (bVidAutoSwitchFull) {
				nVidFullscreen = 1;
				POST_INITIALISE_MESSAGE;
			}
			break;
		}
	}

	return 0;
}

int StartFromReset(TCHAR *szDriverName, bool bLoadSram)
{
	if (!bDrvOkay || (szDriverName && _tcscmp(szDriverName, BurnDrvGetText(DRV_NAME))) ) {
		bSramLoad = bLoadSram;
		BurnerLoadDriver(szDriverName);
		bSramLoad = true; // back to default
		return 1;
	}
	//if(nBurnDrvActive < 1) return 0;

	int nOldDrvSelect = nBurnDrvActive;

	DrvExit();
	bLoading = 1;

	nBurnDrvActive = nOldDrvSelect;
	nDialogSelect = nOldDlgSelected = nOldDrvSelect;
	SplashDestroy(1);
	StopReplay();

	DrvInit(nOldDrvSelect, bLoadSram);	// Init the game driver, load SRAM?
	MenuEnableItems();
	bAltPause = 0;
	AudSoundPlay();			// Restart sound
	bLoading = 0;
	UpdatePreviousGameList();
	if (bVidAutoSwitchFull) {
		nVidFullscreen = 1;
		POST_INITIALISE_MESSAGE;
	}
	return 1;
}

void scrnSSUndo() // called from the menu (shift+F8) and CheckSystemMacros() in run.cpp
{
	if (bDrvOkay) {
		TCHAR szString[256] = _T("state undo");
		TCHAR szStringFailed[256] = _T("state: nothing to undo");
		if (!StatedUNDO(nSavestateSlot)) {
			VidSNewShortMsg(szString);
		} else {
			VidSNewShortMsg(szStringFailed);
		}
		PausedRedraw();
	}
}

void ScrnInitLua() {
	if (UseDialogs()) {
		if (!LuaConsoleHWnd) {
			InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
			LuaConsoleHWnd = CreateDialog(hAppInst, MAKEINTRESOURCE(IDD_LUA), NULL, (DLGPROC)DlgLuaScriptDialog);
		}
		else
		{
			SetForegroundWindow(LuaConsoleHWnd);
		}
	}
}

void ScrnExitLua() {
	if (LuaConsoleHWnd) {
		PostMessage(LuaConsoleHWnd, WM_CLOSE, 0, 0);

		LuaConsoleHWnd = NULL;
	}
}

static void OnCommand(HWND /*hDlg*/, int id, HWND /*hwndCtl*/, UINT codeNotify)
{
	//if(id >= ID_MDI_START_CHILD) {
	//	DefFrameProc(hwndCtl, hWndChildFrame, WM_COMMAND, wParam, lParam);
	//	return;
	//} else {
	//	HWND hWndCurrent = (HWND)SendMessage(hWndChildFrame, WM_MDIGETACTIVE,0,0);
	//	if(hWndCurrent) {
	//		SendMessage(hWndCurrent, WM_COMMAND, wParam, lParam);
	//		return;
	//	}
	//}

	if (bLoading) {
		return;
	}

	switch (id) {
		case MENU_LOAD: {
			int nGame;

			if(kNetGame || !UseDialogs() || bLoading) {
				break;
			}

			SplashDestroy(1);
			StopReplay();

#ifdef INCLUDE_AVI_RECORDING
			AviStop();
#endif

			InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);

			bLoading = 1;
			AudSoundStop();						// Stop while the dialog is active or we're loading ROMs

			nGame = SelDialog(0, hScrnWnd);		// Bring up select dialog to pick a driver

			extern bool bDialogCancel;

			if (nGame >= 0 && bDialogCancel == false) {
				DrvExit();
				DrvInit(nGame, true);			// Init the game driver
				MenuEnableItems();
				bAltPause = 0;
				AudSoundPlay();					// Restart sound
				bLoading = 0;
				UpdatePreviousGameList();
				if (bVidAutoSwitchFull) {
					nVidFullscreen = 1;
					POST_INITIALISE_MESSAGE;
				}

				POST_INITIALISE_MESSAGE;
				break;
			} else {
				GameInpCheckMouse();
				AudSoundPlay();					// Restart sound
				bLoading = 0;
				break;
			}
		}

		case MENU_PREVIOUSGAMES1:
		case MENU_PREVIOUSGAMES2:
		case MENU_PREVIOUSGAMES3:
		case MENU_PREVIOUSGAMES4:
		case MENU_PREVIOUSGAMES5:
		case MENU_PREVIOUSGAMES6:
		case MENU_PREVIOUSGAMES7:
		case MENU_PREVIOUSGAMES8:
		case MENU_PREVIOUSGAMES9:
		case MENU_PREVIOUSGAMES10: {
			BurnerLoadDriver(szPrevGames[id - MENU_PREVIOUSGAMES1]);
			break;
		}

		case MENU_START_NEOGEO_MVS: {
			BurnerLoadDriver(_T("neogeo"));
			break;
		}

		case MENU_START_NEOGEO_CD: {
			BurnerLoadDriver(_T("neocdz"));
			break;
		}

		case MENU_LOAD_NEOCD: {
			AudBlankSound();
			if (UseDialogs()) {
				NeoCDList_Init();
			}
			break;
		}

		case MENU_CDIMAGE: {
			nCDEmuSelect = 0;
			TCHAR szFilter[100];
			_stprintf(szFilter, _T("%s"), FBALoadStringEx(hAppInst, IDS_CD_SELECT_FILTER, true));
			memcpy(szFilter + _tcslen(szFilter), _T(" (*.ccd,*.cue)\0*.ccd;*.cue\0\0"), 28 * sizeof(TCHAR));
			TCHAR szTitle[100];
			_stprintf(szTitle, _T("%s"), FBALoadStringEx(hAppInst, IDS_CD_SELECT_IMAGE_TITLE, true));
			if (UseDialogs() && !bDrvOkay) {
				memset(&ofn, 0, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hScrnWnd;
				ofn.lpstrFile = StrReplace(CDEmuImage, _T('/'), _T('\\'));
				ofn.nMaxFile = MAX_PATH;
				ofn.lpstrTitle = szTitle;
				ofn.lpstrFilter = szFilter;
				ofn.lpstrInitialDir = _T(".");
				ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
				ofn.lpstrDefExt = _T("cue");

				GetOpenFileName(&ofn);
			}
			break;
		}

		case MENU_STARTNET:
			if (Init_Network()) {
				MessageBox(hScrnWnd, FBALoadStringEx(hAppInst, IDS_ERR_NO_NETPLAYDLL, true), FBALoadStringEx(hAppInst, IDS_ERR_ERROR, true), MB_OK);
				break;
			}
#ifdef BUILD_A68K
			if (bBurnUseASMCPUEmulation) {
				FBAPopupAddText(PUF_TEXT_DEFAULT, _T("Please uncheck \"Misc -> Options -> Use Assembly MC68000 Core\" before starting a netgame!"));
				FBAPopupDisplay(PUF_TYPE_ERROR);
				break;
			}
#endif
			if (!kNetGame) {
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				AudBlankSound();
				SplashDestroy(1);
				StopReplay();
#ifdef INCLUDE_AVI_RECORDING
				AviStop();
#endif
				DrvExit();
				DoNetGame();
				MenuEnableItems();
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
			}
			break;

		case MENU_STARTREPLAY:
			if (UseDialogs()) {
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				AudSoundStop();
				SplashDestroy(1);
				StopReplay();
				StartReplay();
				GameInpCheckMouse();
				AudSoundPlay();
			}
			break;
		case MENU_STARTRECORD:
			if (UseDialogs() && nReplayStatus != 1) {
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				AudBlankSound();
				StopReplay();
				StartRecord();
				GameInpCheckMouse();
			}
			break;
		case MENU_STOPREPLAY:
			StopReplay();
			SetPauseMode(1);
			break;

		case ID_LUA_OPEN:
			ScrnInitLua();
			MenuEnableItems();
			break;
		case ID_LUA_CLOSE_ALL:
			ScrnExitLua();
			MenuEnableItems();
			break;

#ifdef INCLUDE_AVI_RECORDING
		case MENU_AVISTART:
			if (AviStart()) {
				AviStop();
			} else {
				VidSNewShortMsg(FBALoadStringEx(hAppInst, IDS_REC_AVI, true), 0x0000FF);
			}
			break;
		case MENU_AVISTOP:
			AviStop();
			VidSNewShortMsg(FBALoadStringEx(hAppInst, IDS_STOP_AVI, true), 0xFF3F3F);
			break;
#endif

		case MENU_QUIT:
			AudBlankSound();
			if (nVidFullscreen) {
				nVidFullscreen = 0;
				VidExit();
			}
			if (bDrvOkay) {
				StopReplay();
#ifdef INCLUDE_AVI_RECORDING
				AviStop();
#endif
				DrvExit();
  				if (kNetGame) {
					kNetGame = 0;
					Kaillera_End_Game();
					DeActivateChat();
					PostQuitMessage(0);
				}
				bCheatsAllowed = true;						// reenable cheats netplay has ended

				ScrnSize();
				ScrnTitle();
				MenuEnableItems();
				//nDialogSelect = -1;
				nBurnDrvActive = ~0U;

				POST_INITIALISE_MESSAGE;
			}
			break;

		case MENU_EXIT:
			StopReplay();
#ifdef INCLUDE_AVI_RECORDING
			AviStop();
#endif
			if (kNetGame) {
				kNetGame = 0;
				Kaillera_End_Game();
				DeActivateChat();
			}
			PostQuitMessage(0);
			return;

		case MENU_PAUSE:
			if (bDrvOkay && !kNetGame) {
				SetPauseMode(!bRunPause);
			} else {
				SetPauseMode(0);
			}

			break;

		case MENU_RESET:
			if (bRunPause)
				SetPauseMode(0);

			bResetDrv = true;
			break;

		case MENU_INPUT:
			AudBlankSound();
			if (UseDialogs()) {
				InputSetCooperativeLevel(false, false);
				InpdCreate();
			}
			break;

		case MENU_DIPSW:
			AudBlankSound();
			if (UseDialogs()) {
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				InpDIPSWCreate();
			}
			break;

		case MENU_SETCPUCLOCK:
			AudBlankSound();
			CPUClockDialog();
			MenuEnableItems();
			GameInpCheckMouse();
			break;
		case MENU_RESETCPUCLOCK:
			nBurnCPUSpeedAdjust = 0x0100;
			MenuEnableItems();
			break;

		case MENU_MEMCARD_CREATE:
			if (bDrvOkay && UseDialogs() && !kNetGame && (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_NEOGEO) {
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				AudBlankSound();
				MemCardEject();
				MemCardCreate();
				MemCardInsert();
				GameInpCheckMouse();
			}
			break;
		case MENU_MEMCARD_SELECT:
			if (bDrvOkay && UseDialogs() && !kNetGame && (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_NEOGEO) {
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				AudBlankSound();
				MemCardEject();
				MemCardSelect();
				MemCardInsert();
				GameInpCheckMouse();
			}
			break;
		case MENU_MEMCARD_INSERT:
			if (!kNetGame && (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_NEOGEO) {
				MemCardInsert();
			}
			break;
		case MENU_MEMCARD_EJECT:
			if (!kNetGame && (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_NEOGEO) {
				MemCardEject();
			}
			break;

		case MENU_MEMCARD_TOGGLE:
			if (bDrvOkay && !kNetGame && (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_NEOGEO) {
				MemCardToggle();
			}
			break;

		case MENU_STATE_LOAD_DIALOG:
			if (UseDialogs() && !kNetGame) {
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				AudSoundStop();
				SplashDestroy(1);
				StatedLoad(0);
				GameInpCheckMouse();
				AudSoundPlay();
			}
			break;
		case MENU_STATE_SAVE_DIALOG:
			if (UseDialogs()) {
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				AudBlankSound();
				StatedSave(0);
				GameInpCheckMouse();
			}
			return;
		case MENU_STATE_PREVSLOT: {
			TCHAR szString[256];

			nSavestateSlot--;
			if (nSavestateSlot < 1) {
				nSavestateSlot = 1;
			}
			_sntprintf(szString, 256, FBALoadStringEx(hAppInst, IDS_STATE_ACTIVESLOT, true), nSavestateSlot);
			VidSNewShortMsg(szString);
			PausedRedraw();
			break;
		}
		case MENU_STATE_NEXTSLOT: {
			TCHAR szString[256];

			nSavestateSlot++;
			if (nSavestateSlot > 8) {
				nSavestateSlot = 8;
			}
			_sntprintf(szString, 256, FBALoadStringEx(hAppInst, IDS_STATE_ACTIVESLOT, true), nSavestateSlot);
			VidSNewShortMsg(szString);
			PausedRedraw();
			break;
		}
		case MENU_STATE_UNDO:
			scrnSSUndo();
			break;
		case MENU_STATE_LOAD_SLOT:
			if (bDrvOkay && !kNetGame) {
				if (StatedLoad(nSavestateSlot) == 0) {
					VidSNewShortMsg(FBALoadStringEx(hAppInst, IDS_STATE_LOADED, true), 0, 40);
				} else {
					VidSNewShortMsg(FBALoadStringEx(hAppInst, IDS_STATE_LOAD_ERROR, true), 0xFF3F3F);
				}
				PausedRedraw();
			}
			break;
		case MENU_STATE_SAVE_SLOT:
			if (bDrvOkay) {
				if (StatedSave(nSavestateSlot) == 0) {
					VidSNewShortMsg(FBALoadStringEx(hAppInst, IDS_STATE_SAVED, true), 0, 40);
				} else {
					VidSNewShortMsg(FBALoadStringEx(hAppInst, IDS_STATE_SAVE_ERROR, true), 0xFF3F3F);
					SetPauseMode(1);
				}
				PausedRedraw();
			}
			break;

		case MENU_ALLRAM:
			bDrvSaveAll = !bDrvSaveAll;
			break;

		case MENU_NOSTRETCH:
			bVidCorrectAspect = 0;
			bVidFullStretch = 0;
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_STRETCH:
			bVidFullStretch = true;
			if (bVidFullStretch) {
				bVidCorrectAspect = 0;
			}
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_ASPECT:
			bVidCorrectAspect = true;
			if (bVidCorrectAspect) {
				bVidFullStretch = 0;
			}
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_TRIPLE:
			bVidTripleBuffer = !bVidTripleBuffer;
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_DWMFIX:
			bVidDWMSync = !bVidDWMSync;
			if (bVidDWMSync && bVidVSync)
				bVidVSync = 0;

			POST_INITIALISE_MESSAGE;
			break;

		case MENU_BLITTER_1:
			VidSelect(0);
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_BLITTER_2:
			VidSelect(1);
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_BLITTER_3:
			VidSelect(2);
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_BLITTER_4:
			VidSelect(3);
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_BLITTER_5:
			VidSelect(4);
			POST_INITIALISE_MESSAGE;
			break;
#if 0
			case MENU_BLITTER_6:
			VidSelect(5);
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_BLITTER_7:
			VidSelect(6);
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_BLITTER_8:
			VidSelect(7);
			POST_INITIALISE_MESSAGE;
			break;
#endif

		case MENU_RES_ARCADE:
			bVidArcaderesHor = !bVidArcaderesHor;
			nScreenSizeHor = 0;
			if ((bDrvOkay) && !(BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeHor;
				bVidArcaderes = bVidArcaderesHor;
			}
			break;

		case MENU_SINGLESIZESCREEN:
			nScreenSizeHor = 1;
			bVidArcaderesHor = false;
			if ((bDrvOkay) && !(BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeHor;
				bVidArcaderes = bVidArcaderesHor;
			}
			break;
		case MENU_DOUBLESIZESCREEN:
			nScreenSizeHor = 2;
			bVidArcaderesHor = false;
			if ((bDrvOkay) && !(BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeHor;
				bVidArcaderes = bVidArcaderesHor;
			}
			break;
		case MENU_TRIPLESIZESCREEN:
			nScreenSizeHor = 3;
			bVidArcaderesHor = false;
			if ((bDrvOkay) && !(BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeHor;
				bVidArcaderes = bVidArcaderesHor;
			}
			break;
		case MENU_QUADSIZESCREEN:
			nScreenSizeHor = 4;
			bVidArcaderesHor = false;
			if ((bDrvOkay) && !(BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeHor;
				bVidArcaderes = bVidArcaderesHor;
			}
			break;

		case MENU_RES_1:
			nVidHorWidth = VidPreset[0].nWidth;
			nVidHorHeight = VidPreset[0].nHeight;
			bVidArcaderesHor = false;
			nScreenSizeHor = 0;
			if ((bDrvOkay) && !(BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeHor;
				bVidArcaderes = bVidArcaderesHor;
				nVidWidth	= nVidHorWidth;
				nVidHeight	= nVidHorHeight;
			}
			break;
		case MENU_RES_2:
			nVidHorWidth = VidPreset[1].nWidth;
			nVidHorHeight = VidPreset[1].nHeight;
			bVidArcaderesHor = false;
			nScreenSizeHor = 0;
			if ((bDrvOkay) && !(BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeHor;
				bVidArcaderes = bVidArcaderesHor;
				nVidWidth	= nVidHorWidth;
				nVidHeight	= nVidHorHeight;
			}
			break;
		case MENU_RES_3:
			nVidHorWidth = VidPreset[2].nWidth;
			nVidHorHeight = VidPreset[2].nHeight;
			bVidArcaderesHor = false;
			nScreenSizeHor = 0;
			if ((bDrvOkay) && !(BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeHor;
				bVidArcaderes = bVidArcaderesHor;
				nVidWidth	= nVidHorWidth;
				nVidHeight	= nVidHorHeight;
			}
			break;
		case MENU_RES_4:
			nVidHorWidth = VidPreset[3].nWidth;
			nVidHorHeight = VidPreset[3].nHeight;
			bVidArcaderesHor = false;
			nScreenSizeHor = 0;
			if ((bDrvOkay) && !(BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeHor;
				bVidArcaderes = bVidArcaderesHor;
				nVidWidth	= nVidHorWidth;
				nVidHeight	= nVidHorHeight;
			}
			break;


		case MENU_RES_OTHER:
			bVidArcaderesHor = false;
			nScreenSizeHor = 0;
			if ((bDrvOkay) && !(BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeHor;
				bVidArcaderes = bVidArcaderesHor;
			}
			AudBlankSound();
			InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
			ResCreate(HORIZONTAL_ORIENTED_RES);
			GameInpCheckMouse();
			break;

		case MENU_FULLSCREEN_MONITOR:
			if (UseDialogs()) {
				AudBlankSound();
				ChooseMonitorCreate();
				GameInpCheckMouse();
			}
			break;

		// Vertical
		case MENU_RES_ARCADE_VERTICAL:
			bVidArcaderesVer = !bVidArcaderesVer;
			nScreenSizeVer = 0;
			if ((bDrvOkay) && (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeVer;
				bVidArcaderes = bVidArcaderesVer;
			}
			break;

		case MENU_SINGLESIZESCREEN_VERTICAL:
			nScreenSizeVer = 1;
			bVidArcaderesVer = false;
			if ((bDrvOkay) && (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeVer;
				bVidArcaderes = bVidArcaderesVer;
			}
			break;
		case MENU_DOUBLESIZESCREEN_VERTICAL:
			nScreenSizeVer = 2;
			bVidArcaderesVer = false;
			if ((bDrvOkay) && (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeVer;
				bVidArcaderes = bVidArcaderesVer;
			}
			break;
		case MENU_TRIPLESIZESCREEN_VERTICAL:
			nScreenSizeVer = 3;
			bVidArcaderesVer = false;
			if ((bDrvOkay) && (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeVer;
				bVidArcaderes = bVidArcaderesVer;
			}
			break;
		case MENU_QUADSIZESCREEN_VERTICAL:
			nScreenSizeVer = 4;
			bVidArcaderesVer = false;
			if ((bDrvOkay) && (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeVer;
				bVidArcaderes = bVidArcaderesVer;
			}
			break;

		case MENU_RES_1_VERTICAL:
			nVidVerWidth = VidPresetVer[0].nWidth;
			nVidVerHeight = VidPresetVer[0].nHeight;
			bVidArcaderesVer = false;
			nScreenSizeVer = 0;
			if ((bDrvOkay) && (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeVer;
				bVidArcaderes = bVidArcaderesVer;
				nVidWidth	= nVidVerWidth;
				nVidHeight	= nVidVerHeight;
			}
			break;
		case MENU_RES_2_VERTICAL:
			nVidVerWidth = VidPresetVer[1].nWidth;
			nVidVerHeight = VidPresetVer[1].nHeight;
			bVidArcaderesVer = false;
			nScreenSizeVer = 0;
			if ((bDrvOkay) && (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeVer;
				bVidArcaderes = bVidArcaderesVer;
				nVidWidth	= nVidVerWidth;
				nVidHeight	= nVidVerHeight;
			}
			break;
		case MENU_RES_3_VERTICAL:
			nVidVerWidth = VidPresetVer[2].nWidth;
			nVidVerHeight = VidPresetVer[2].nHeight;
			bVidArcaderesVer = false;
			nScreenSizeVer = 0;
			if ((bDrvOkay) && (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeVer;
				bVidArcaderes = bVidArcaderesVer;
				nVidWidth	= nVidVerWidth;
				nVidHeight	= nVidVerHeight;
			}
			break;
		case MENU_RES_4_VERTICAL:
			nVidVerWidth = VidPresetVer[3].nWidth;
			nVidVerHeight = VidPresetVer[3].nHeight;
			bVidArcaderesVer = false;
			nScreenSizeVer = 0;
			if ((bDrvOkay) && (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeVer;
				bVidArcaderes = bVidArcaderesVer;
				nVidWidth	= nVidVerWidth;
				nVidHeight	= nVidVerHeight;
			}
			break;

		case MENU_RES_OTHER_VERTICAL:
			bVidArcaderesVer = false;
			nScreenSizeVer = 0;
			if ((bDrvOkay) && (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL)) {
				nScreenSize = nScreenSizeVer;
				bVidArcaderes = bVidArcaderesVer;
			}
			AudBlankSound();
			InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
			ResCreate(VERTICAL_ORIENTED_RES);
			GameInpCheckMouse();
			break;

		case MENU_16:
			nVidDepth = 16;
			break;

		case MENU_24:
			nVidDepth = 24;
			break;

		case MENU_32:
			nVidDepth = 32;
			break;

		case MENU_GAMMA_DO:
			bDoGamma = !bDoGamma;
			if (bDrvOkay) {
				if (nVidSelect == 1) {
					VidInit();
				}
				SetBurnHighCol(nVidImageDepth);
				if (bRunPause) {
					VidRedraw();
				}
			}
			break;

		case MENU_GAMMA_DARKER:
			nGamma = 1.25;
			ComputeGammaLUT();
			bDoGamma = 1;
			if (bDrvOkay) {
				if (nVidSelect == 1) {
					VidInit();
				}
				SetBurnHighCol(nVidImageDepth);
				if (bRunPause) {
					VidRedraw();
				}
			}
			break;

		case MENU_GAMMA_LIGHTER:
			nGamma = 0.80;
			ComputeGammaLUT();
			bDoGamma = 1;
			if (bDrvOkay) {
				if (nVidSelect == 1) {
					VidInit();
				}
				SetBurnHighCol(nVidImageDepth);
				if (bRunPause) {
					VidRedraw();
				}
			}
			break;

		case MENU_GAMMA_OTHER: {
			if (UseDialogs()) {
				double nOldGamma = nGamma;
				bDoGamma = 1;
				if (bDrvOkay) {
					if (nVidSelect == 1) {
						VidInit();
					}
					SetBurnHighCol(nVidImageDepth);
				}
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				AudBlankSound();
				GammaDialog();
				if (nGamma > 0.999 && nGamma < 1.001) {
					nGamma = nOldGamma;
					bDoGamma = 0;
					if (nVidSelect == 1) {
						VidInit();
					}
					SetBurnHighCol(nVidImageDepth);
				} else {
					bDoGamma = 1;
					ComputeGammaLUT();
				}
				if (bDrvOkay) {
					VidRecalcPal();
				}
				GameInpCheckMouse();
			}
			break;
		}

		case MENU_GAMMA_USE_HARDWARE:
			bVidUseHardwareGamma = 1;
			bHardwareGammaOnly = 0;
			if (bDrvOkay) {
				if (nVidSelect == 1) {
					VidInit();
				}
				SetBurnHighCol(nVidImageDepth);
				VidRecalcPal();
				if (bRunPause) {
					VidRedraw();
				}
			}
			break;
		case MENU_GAMMA_HARDWARE_ONLY:
			bVidUseHardwareGamma = 1;
			bHardwareGammaOnly = 1;
			if (bDrvOkay) {
				if (nVidSelect == 1) {
					VidInit();
				}
				SetBurnHighCol(nVidImageDepth);
				VidRecalcPal();
				if (bRunPause) {
					VidRedraw();
				}
			}
			break;
		case MENU_GAMMA_SOFTWARE_ONLY:
			bVidUseHardwareGamma = 0;
			bHardwareGammaOnly = 0;
			if (bDrvOkay) {
				if (nVidSelect == 1) {
					VidInit();
				}
				SetBurnHighCol(nVidImageDepth);
				VidRecalcPal();
				if (bRunPause) {
					VidRedraw();
				}
			}
			break;

		case MENU_FULL:
			if (bDrvOkay || nVidFullscreen) {
				nVidFullscreen = !nVidFullscreen;
				POST_INITIALISE_MESSAGE;
			}
			return;

		case MENU_AUTOSWITCHFULL:
			bVidAutoSwitchFull = !bVidAutoSwitchFull;
			break;

		case MENU_BASIC_MEMAUTO:
		case MENU_SOFTFX_MEMAUTO:
			nVidTransferMethod = -1;
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_BASIC_VIDEOMEM:
		case MENU_SOFTFX_VIDEOMEM:
			nVidTransferMethod = 0;
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_BASIC_SYSMEM:
		case MENU_SOFTFX_SYSMEM:
			nVidTransferMethod = 1;
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_AUTOSIZE:
			if (nWindowSize != 0) {
				nWindowSize = 0;
				POST_INITIALISE_MESSAGE;
			}
			break;
		case MENU_SINGLESIZEWINDOW:
			if (nWindowSize != 1) {
				nWindowSize = 1;
				POST_INITIALISE_MESSAGE;
			}
			break;
		case MENU_DOUBLESIZEWINDOW:
			if (nWindowSize != 2) {
				nWindowSize = 2;
				POST_INITIALISE_MESSAGE;
			}
			break;
		case MENU_TRIPLESIZEWINDOW:
			if (nWindowSize != 3) {
				nWindowSize = 3;
				POST_INITIALISE_MESSAGE;
			}
			break;
		case MENU_QUADSIZEWINDOW:
			if (nWindowSize != 4) {
				nWindowSize = 4;
				POST_INITIALISE_MESSAGE;
			}
			break;
		case MENU_MAXIMUMSIZEWINDOW:
			if (nWindowSize <= 4) {
				nWindowSize = 9999;
				POST_INITIALISE_MESSAGE;
			}
			break;

		case MENU_MONITORAUTOCHECK:
			bMonitorAutoCheck = !bMonitorAutoCheck;
			if (bMonitorAutoCheck) MonitorAutoCheck();
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_ASPECTNORMAL:
			bMonitorAutoCheck = false; nVidScrnAspectX = 4; nVidScrnAspectY = 3;
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_ASPECTLCD:
			bMonitorAutoCheck = false; nVidScrnAspectX = 5; nVidScrnAspectY = 4;
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_ASPECTWIDE:
			bMonitorAutoCheck = false; nVidScrnAspectX = 16; nVidScrnAspectY = 9;
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_ASPECTWIDELCD:
			bMonitorAutoCheck = false; nVidScrnAspectX = 16; nVidScrnAspectY = 10;
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_ASPECTNORMALVERL:
			bMonitorAutoCheck = false; nVidVerScrnAspectX = 4; nVidVerScrnAspectY = 3;
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_ASPECTLCDVERL:
			bMonitorAutoCheck = false; nVidVerScrnAspectX = 5; nVidVerScrnAspectY = 4;
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_ASPECTWIDEVERL:
			bMonitorAutoCheck = false; nVidVerScrnAspectX = 16; nVidVerScrnAspectY = 9;
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_ASPECTWIDELCDVERL:
			bMonitorAutoCheck = false; nVidVerScrnAspectX = 16; nVidVerScrnAspectY = 10;
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_ASPECTNORMALVERP:
			bMonitorAutoCheck = false; nVidVerScrnAspectX = 3; nVidVerScrnAspectY = 4;
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_ASPECTLCDVERP:
			bMonitorAutoCheck = false; nVidVerScrnAspectX = 4; nVidVerScrnAspectY = 5;
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_ASPECTWIDEVERP:
			bMonitorAutoCheck = false; nVidVerScrnAspectX = 9; nVidVerScrnAspectY = 16;
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_ASPECTWIDELCDVERP:
			bMonitorAutoCheck = false; nVidVerScrnAspectX = 10; nVidVerScrnAspectY = 16;
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_MONITORMIRRORVERT:
			nVidRotationAdjust ^= 2;
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_ROTATEVERTICAL:
			nVidRotationAdjust ^= 1;
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_FORCE60HZ:
			bForce60Hz = !bForce60Hz;
			break;

		case MENU_VIDEOVSYNC:
			bVidVSync = !bVidVSync;
			if (bVidVSync && bVidDWMSync)
				bVidDWMSync = 0;

			POST_INITIALISE_MESSAGE;
			break;

		case MENU_AUTOFRAMESKIP:
			bAlwaysDrawFrames = !bAlwaysDrawFrames;
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_RUNAHEAD:
			bRunAhead = !bRunAhead;
			break;

		case MENU_AUD_PLUGIN_1:
			AudSelect(0);
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_AUD_PLUGIN_2:
			AudSelect(1);
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_DSOUND_NOSOUND:
			if (!bDrvOkay) {
				nAudSampleRate[0] = 0;
				POST_INITIALISE_MESSAGE;
			}
			break;
		case MENU_DSOUND_44100:
			if (!bDrvOkay) {
				nAudSampleRate[0] = 44100;
				POST_INITIALISE_MESSAGE;
			}
			break;
		case MENU_DSOUND_48000:
			if (!bDrvOkay) {
				nAudSampleRate[0] = 48000;
				POST_INITIALISE_MESSAGE;
			}
			break;

		case MENU_XAUDIO_NOSOUND:
			if (!bDrvOkay) {
				nAudSampleRate[1] = 0;
				POST_INITIALISE_MESSAGE;
			}
			break;
		case MENU_XAUDIO_44100:
			if (!bDrvOkay) {
				nAudSampleRate[1] = 44100;
				POST_INITIALISE_MESSAGE;
			}
			break;
		case MENU_XAUDIO_48000:
			if (!bDrvOkay) {
				nAudSampleRate[1] = 48000;
				POST_INITIALISE_MESSAGE;
			}
			break;

		case MENU_FRAMES:
			if (UseDialogs()) {
				if (!bDrvOkay) {
//					AudBlankSound();
					NumDialCreate(0);
					POST_INITIALISE_MESSAGE;
				}
			}
			break;

		case MENU_INTERPOLATE_0:
			nInterpolation = 0;
			break;
		case MENU_INTERPOLATE_1:
			nInterpolation = 1;
			break;
		case MENU_INTERPOLATE_3:
			nInterpolation = 3;
			break;

		case MENU_INTERPOLATE_FM_0:
			nFMInterpolation = 0;
			break;
		case MENU_INTERPOLATE_FM_1:
			nFMInterpolation = 1;
			break;
		case MENU_INTERPOLATE_FM_3:
			nFMInterpolation = 3;
			break;

		case MENU_DSOUND_BASS:
			nAudDSPModule[0] = !nAudDSPModule[0];
			break;

		case MENU_XAUDIO_BASS:
			nAudDSPModule[1] ^= 1;
			break;

		case MENU_XAUDIO_REVERB:
			nAudDSPModule[1] ^= 2;
			break;

		case MENU_WLOGSTART:
			AudBlankSound();
			WaveLogStart();
			break;

		case MENU_WLOGEND:
			AudBlankSound();
			WaveLogStop();
			break;

		case MENU_AUTOPAUSE:
			bAutoPause = !bAutoPause;
			break;

		case MENU_PROCESSINPUT:
			bAlwaysProcessKeyboardInput = !bAlwaysProcessKeyboardInput;
			break;

		case MENU_DISPLAYINDICATOR:
			nVidSDisplayStatus = !nVidSDisplayStatus;
//			VidRedraw();
			VidPaint(2);
			break;

		case MENU_MODELESS:
			bModelessMenu = !bModelessMenu;
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_NOCHANGENUMLOCK:
			bNoChangeNumLock = !bNoChangeNumLock;
			break;

		case MENU_HIGHRESTIMER:
			bEnableHighResTimer = !bEnableHighResTimer;
			DisableHighResolutionTiming();  // disable if active
			EnableHighResolutionTiming();   // use new setting.
			break;

#if defined (FBNEO_DEBUG)
		case MENU_DEBUGCONSOLE:
			bDisableDebugConsole = !bDisableDebugConsole;
			break;
#endif

		case MENU_CREATEDIRS:
			bAlwaysCreateSupportFolders = !bAlwaysCreateSupportFolders;
			break;

		case MENU_AUTOLOADGAMELIST:
			bAutoLoadGameList = !bAutoLoadGameList;
			break;

		case MENU_AUTOSCANGAMELIST:
			bSkipStartupCheck = !bSkipStartupCheck;
			break;

		case MENU_SAVEHISCORES:
			EnableHiscores = !EnableHiscores;
			break;

		case MENU_USEBLEND:
			bBurnUseBlend = !bBurnUseBlend;
			break;

		case MENU_GEARSHIFT:
			BurnShiftEnabled = !BurnShiftEnabled;
			break;

		case MENU_LIGHTGUNRETICLES:
			bBurnGunDrawReticles = !bBurnGunDrawReticles;
			break;

		case ID_SLOMO_0:
		case ID_SLOMO_1:
		case ID_SLOMO_2:
		case ID_SLOMO_3:
		case ID_SLOMO_4:
			nSlowMo = id - ID_SLOMO_0;
			break;

#ifdef INCLUDE_AVI_RECORDING
		case MENU_AVI1X:
			nAvi3x = 1;
			break;

		case MENU_AVI2X:
			nAvi3x = 2;
			break;

		case MENU_AVI3X:
			nAvi3x = 3;
			break;
#endif

		case MENU_ROMDIRS:
			RomsDirCreate(hScrnWnd);
			break;

		case MENU_SUPPORTDIRS:
			SupportDirCreate(hScrnWnd);
			break;

		case MENU_SELECTPLACEHOLDER:
			if (UseDialogs()) {
				SelectPlaceHolder();
				POST_INITIALISE_MESSAGE;
			}
			break;

		case MENU_DISABLEPLACEHOLDER:
			ResetPlaceHolder();
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_LANGUAGE_SELECT:
			if (UseDialogs()) {
				FBALocaliseLoadTemplate();
				POST_INITIALISE_MESSAGE;
			}
			break;
		case MENU_LANGUAGE_EXPORT:
			if (UseDialogs()) {
				FBALocaliseCreateTemplate();
			}
			break;
		case MENU_LANGUAGE_RESET:
			szLocalisationTemplate[0] = _T('\0');
			FBALocaliseInit(szLocalisationTemplate);
			POST_INITIALISE_MESSAGE;
			break;
		case MENU_LANGUAGE_DOWNLOAD:
			if (UseDialogs()) {
				LocaliseDownloadCreate(hScrnWnd);
			}
			break;

		case MENU_LANGUAGE_GL_SELECT:
			if (UseDialogs()) {
				FBALocaliseGamelistLoadTemplate();
			}
			break;
		case MENU_LANGUAGE_GL_EXPORT:
			if (UseDialogs()) {
				FBALocaliseGamelistCreateTemplate();
			}
			break;
		case MENU_LANGUAGE_GL_RESET:
			szGamelistLocalisationTemplate[0] = _T('\0');
			nGamelistLocalisationActive = false;
			break;

		case MENU_ENABLEICONS: {
			bEnableIcons = !bEnableIcons;
			if(!bEnableIcons && bIconsLoaded) {
				// unload icons
				UnloadDrvIcons();
				bIconsLoaded = 0;
			}
			if(bEnableIcons && !bIconsLoaded) {
				// load icons
				LoadDrvIcons();
				bIconsLoaded = 1;
			}
			break;
		}

		case MENU_ICONS_PARENTSONLY: {
			bIconsOnlyParents = !bIconsOnlyParents;
			if(bEnableIcons && bIconsLoaded) {
				// unload icons
				UnloadDrvIcons();
				bIconsLoaded = 0;
				// load icons
				LoadDrvIcons();
				bIconsLoaded = 1;
			}
			break;
		}

		case MENU_ICONS_SIZE_16: {
			nIconsSize = ICON_16x16;
			if(bEnableIcons && bIconsLoaded) {
				// unload icons
				UnloadDrvIcons();
				bIconsLoaded = 0;
				// load icons
				LoadDrvIcons();
				bIconsLoaded = 1;
			}
			if(bEnableIcons && !bIconsLoaded) {
				// load icons
				LoadDrvIcons();
				bIconsLoaded = 1;
			}
			break;
		}

		case MENU_ICONS_SIZE_24: {
			nIconsSize = ICON_24x24;
			if(bEnableIcons && bIconsLoaded) {
				// unload icons
				UnloadDrvIcons();
				bIconsLoaded = 0;
				// load icons
				LoadDrvIcons();
				bIconsLoaded = 1;
			}
			if(bEnableIcons && !bIconsLoaded) {
				// load icons
				LoadDrvIcons();
				bIconsLoaded = 1;
			}
			break;
		}

		case MENU_ICONS_SIZE_32: {
			nIconsSize = ICON_32x32;
			if(bEnableIcons && bIconsLoaded) {
				// unload icons
				UnloadDrvIcons();
				bIconsLoaded = 0;
				// load icons
				LoadDrvIcons();
				bIconsLoaded = 1;
			}
			if(bEnableIcons && !bIconsLoaded) {
				// load icons
				LoadDrvIcons();
				bIconsLoaded = 1;
			}
			break;
		}

		case MENU_AUDIO_VOLUME_0:
		case MENU_AUDIO_VOLUME_10:
		case MENU_AUDIO_VOLUME_20:
		case MENU_AUDIO_VOLUME_30:
		case MENU_AUDIO_VOLUME_40:
		case MENU_AUDIO_VOLUME_50:
		case MENU_AUDIO_VOLUME_60:
		case MENU_AUDIO_VOLUME_70:
		case MENU_AUDIO_VOLUME_80:
		case MENU_AUDIO_VOLUME_90:
		case MENU_AUDIO_VOLUME_100:
			nAudVolume = (id - MENU_AUDIO_VOLUME_0) * 1000;
			AudSoundSetVolume();
			break;

		case MENU_INPUT_AUTOFIRE_RATE_1: nAutoFireRate = 24; break;
		case MENU_INPUT_AUTOFIRE_RATE_2: nAutoFireRate = 12; break;
		case MENU_INPUT_AUTOFIRE_RATE_3: nAutoFireRate =  8; break;
		case MENU_INPUT_AUTOFIRE_RATE_4: nAutoFireRate =  6; break;
		case MENU_INPUT_AUTOFIRE_RATE_5: nAutoFireRate =  4; break;
		case MENU_INPUT_AUTOFIRE_RATE_6: nAutoFireRate =  2; break;

		case MENU_INPUT_REWIND_ENABLED:
			bRewindEnabled = !bRewindEnabled;
			StateRewindReInit();
			break;
		case MENU_INPUT_REWIND_128MB: nRewindMemory = 128; StateRewindReInit(); break;
		case MENU_INPUT_REWIND_256MB: nRewindMemory = 256; StateRewindReInit(); break;
		case MENU_INPUT_REWIND_512MB: nRewindMemory = 512; StateRewindReInit(); break;
		case MENU_INPUT_REWIND_768MB: nRewindMemory = 768; StateRewindReInit(); break;
		case MENU_INPUT_REWIND_1GB: nRewindMemory = 1024; StateRewindReInit(); break;

		case MENU_PRIORITY_REALTIME: // bad idea, this will freeze the entire system.
			break;
		case MENU_PRIORITY_HIGH:
			nAppProcessPriority = HIGH_PRIORITY_CLASS;
			SetPriorityClass(GetCurrentProcess(), nAppProcessPriority);
			break;
		case MENU_PRIORITY_ABOVE_NORMAL:
			nAppProcessPriority = ABOVE_NORMAL_PRIORITY_CLASS;
			SetPriorityClass(GetCurrentProcess(), nAppProcessPriority);
			break;
		case MENU_PRIORITY_NORMAL:
			nAppProcessPriority = NORMAL_PRIORITY_CLASS;
			SetPriorityClass(GetCurrentProcess(), nAppProcessPriority);
			break;
		case MENU_PRIORITY_BELOW_NORMAL:
			nAppProcessPriority = BELOW_NORMAL_PRIORITY_CLASS;
			SetPriorityClass(GetCurrentProcess(), nAppProcessPriority);
			break;
		case MENU_PRIORITY_LOW:
			nAppProcessPriority = IDLE_PRIORITY_CLASS;
			SetPriorityClass(GetCurrentProcess(), nAppProcessPriority);
			break;

		case MENU_CLRMAME_PRO_XML:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_ARCADE_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_XML_MD_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_MEGADRIVE_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_XML_PCE_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_PCENGINE_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_XML_TG16_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_TG16_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_XML_SGX_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_SGX_ONLY);
			}
                        break;

		case MENU_CLRMAME_PRO_XML_SG1000_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_SG1000_ONLY);
			}
                        break;

		case MENU_CLRMAME_PRO_XML_COLECO_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_COLECO_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_XML_SMS_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_MASTERSYSTEM_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_XML_GG_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_GAMEGEAR_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_XML_MSX_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_MSX_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_XML_SPECTRUM_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_SPECTRUM_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_XML_NES_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_NES_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_XML_FDS_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_FDS_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_XML_NGP_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_NGP_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_XML_CHANNELF_ONLY:
			if (UseDialogs()) {
				CreateDatfileWindows(DAT_CHANNELF_ONLY);
			}
			break;

		case MENU_CLRMAME_PRO_ALL_DATS:
			if (UseDialogs()) {
				CreateAllDatfilesWindows();
			}
			break;

		case MENU_ENABLECHEAT:
			AudBlankSound();
			InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
			InpCheatCreate();
			break;

		case MENU_DEBUG:
			if (UseDialogs()) {
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				DebugCreate();
			}
			break;

		case MENU_PALETTEVIEWER: {
			AudBlankSound();
			InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
			PaletteViewerDialogCreate(hScrnWnd);
			break;
		}

#ifdef BUILD_A68K
		case MENU_ASSEMBLYCORE:
			bBurnUseASMCPUEmulation = !bBurnUseASMCPUEmulation;
			break;
#endif

		case MENU_SAVESNAP: {
			if (bDrvOkay) {
				int status = MakeScreenShot();

				if (!status) {
					VidSNewShortMsg(FBALoadStringEx(hAppInst, IDS_SSHOT_SAVED, true));
				} else {
					TCHAR tmpmsg[256];

					_sntprintf(tmpmsg, 256, FBALoadStringEx(hAppInst, IDS_SSHOT_ERROR, true), status);
					VidSNewShortMsg(tmpmsg, 0xFF3F3F);
				}
			}
			break;
		}

		case MENU_SNAPFACT:
			if (UseDialogs()) {
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				SFactdCreate();
			}
			break;

		case MENU_CHEATSEARCH_START: {
			if (CheatSearchStart() == 0) {
				TCHAR szText[100];
				_stprintf(szText, FBALoadStringEx(hAppInst, IDS_CHEAT_SEARCH_NEW, true));
				VidSAddChatMsg(NULL, 0xFFFFFF, szText, 0xFFBFBF);

				EnableMenuItem(hMenu, MENU_CHEATSEARCH_NOCHANGE, MF_ENABLED | MF_BYCOMMAND);
				EnableMenuItem(hMenu, MENU_CHEATSEARCH_CHANGE, MF_ENABLED | MF_BYCOMMAND);
				EnableMenuItem(hMenu, MENU_CHEATSEARCH_DECREASE, MF_ENABLED | MF_BYCOMMAND);
				EnableMenuItem(hMenu, MENU_CHEATSEARCH_INCREASE, MF_ENABLED | MF_BYCOMMAND);
				EnableMenuItem(hMenu, MENU_CHEATSEARCH_DUMPFILE, MF_ENABLED | MF_BYCOMMAND);
				EnableMenuItem(hMenu, MENU_CHEATSEARCH_EXIT, MF_ENABLED | MF_BYCOMMAND);
			}
			break;
		}

		case MENU_CHEATSEARCH_NOCHANGE: {
			TCHAR tmpmsg[256];
			unsigned int nValues = CheatSearchValueNoChange();

			_stprintf(tmpmsg, FBALoadStringEx(hAppInst, IDS_CHEAT_SEARCH_ADD_MATCH, true), nValues);
			VidSAddChatMsg(NULL, 0xFFFFFF, tmpmsg, 0xFFBFBF);

			if (nValues <= CHEATSEARCH_SHOWRESULTS) {
				for (unsigned int i = 0; i < nValues; i++) {
					_stprintf(tmpmsg, FBALoadStringEx(hAppInst, IDS_CHEAT_SEARCH_RESULTS, true), CheatSearchShowResultAddresses[i], CheatSearchShowResultValues[i]);
					VidSAddChatMsg(NULL, 0xFFFFFF, tmpmsg, 0xFFBFBF);
				}
			}
			break;
		}

		case MENU_CHEATSEARCH_CHANGE: {
			TCHAR tmpmsg[256];
			unsigned int nValues = CheatSearchValueChange();

			_stprintf(tmpmsg, FBALoadStringEx(hAppInst, IDS_CHEAT_SEARCH_ADD_MATCH, true), nValues);
			VidSAddChatMsg(NULL, 0xFFFFFF, tmpmsg, 0xFFBFBF);

			if (nValues <= CHEATSEARCH_SHOWRESULTS) {
				for (unsigned int i = 0; i < nValues; i++) {
					_stprintf(tmpmsg, FBALoadStringEx(hAppInst, IDS_CHEAT_SEARCH_RESULTS, true), CheatSearchShowResultAddresses[i], CheatSearchShowResultValues[i]);
					VidSAddChatMsg(NULL, 0xFFFFFF, tmpmsg, 0xFFBFBF);
				}
			}
			break;
		}

		case MENU_CHEATSEARCH_DECREASE: {
			TCHAR tmpmsg[256];
			unsigned int nValues = CheatSearchValueDecreased();

			_stprintf(tmpmsg, FBALoadStringEx(hAppInst, IDS_CHEAT_SEARCH_ADD_MATCH, true), nValues);
			VidSAddChatMsg(NULL, 0xFFFFFF, tmpmsg, 0xFFBFBF);

			if (nValues <= CHEATSEARCH_SHOWRESULTS) {
				for (unsigned int i = 0; i < nValues; i++) {
					_stprintf(tmpmsg, FBALoadStringEx(hAppInst, IDS_CHEAT_SEARCH_RESULTS, true), CheatSearchShowResultAddresses[i], CheatSearchShowResultValues[i]);
					VidSAddChatMsg(NULL, 0xFFFFFF, tmpmsg, 0xFFBFBF);
				}
			}
			break;
		}

		case MENU_CHEATSEARCH_INCREASE: {
			TCHAR tmpmsg[256];

			unsigned int nValues = CheatSearchValueIncreased();

			_stprintf(tmpmsg, FBALoadStringEx(hAppInst, IDS_CHEAT_SEARCH_ADD_MATCH, true), nValues);
			VidSAddChatMsg(NULL, 0xFFFFFF, tmpmsg, 0xFFBFBF);

			if (nValues <= CHEATSEARCH_SHOWRESULTS) {
				for (unsigned int i = 0; i < nValues; i++) {
					_stprintf(tmpmsg, FBALoadStringEx(hAppInst, IDS_CHEAT_SEARCH_RESULTS, true), CheatSearchShowResultAddresses[i], CheatSearchShowResultValues[i]);
					VidSAddChatMsg(NULL, 0xFFFFFF, tmpmsg, 0xFFBFBF);
				}
			}
			break;
		}

		case MENU_CHEATSEARCH_DUMPFILE: {
			CheatSearchDumptoFile();
			break;
		}

		case MENU_CHEATSEARCH_EXIT: {
			CheatSearchExit();

			TCHAR szText[100];
			_stprintf(szText, FBALoadStringEx(hAppInst, IDS_CHEAT_SEARCH_EXIT, true));
			VidSAddChatMsg(NULL, 0xFFFFFF, szText, 0xFFBFBF);

			EnableMenuItem(hMenu, MENU_CHEATSEARCH_NOCHANGE, MF_GRAYED | MF_BYCOMMAND);
			EnableMenuItem(hMenu, MENU_CHEATSEARCH_CHANGE, MF_GRAYED | MF_BYCOMMAND);
			EnableMenuItem(hMenu, MENU_CHEATSEARCH_DECREASE, MF_GRAYED | MF_BYCOMMAND);
			EnableMenuItem(hMenu, MENU_CHEATSEARCH_INCREASE, MF_GRAYED | MF_BYCOMMAND);
			EnableMenuItem(hMenu, MENU_CHEATSEARCH_DUMPFILE, MF_GRAYED | MF_BYCOMMAND);
			EnableMenuItem(hMenu, MENU_CHEATSEARCH_EXIT, MF_GRAYED | MF_BYCOMMAND);
			break;
		}

		case MENU_ASSOCIATE:
			RegisterExtensions(true);
			break;
        case MENU_DISASSOCIATE:
			RegisterExtensions(false);
			break;

		case MENU_SAVEGAMEINPUTNOW:
			ConfigGameSave(true);
			break;

		case MENU_SAVEGAMEINPUT:
			bSaveInputs = !bSaveInputs;
			break;

		case MENU_SAVESET:
			ConfigAppSave();
			break;

		case MENU_LOADSET:
			ConfigAppLoad();
			POST_INITIALISE_MESSAGE;
			break;

		case MENU_ABOUT:
			if (UseDialogs()) {
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				AudBlankSound();
				AboutCreate();
				GameInpCheckMouse();
			}
			break;
		case MENU_SYSINFO:
			if (UseDialogs()) {
				InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
				AudBlankSound();
				SystemInfoCreate();
				GameInpCheckMouse();
			}
			break;

		case MENU_CONTENTS: {
			if (!nVidFullscreen) {
				FILE* fp = _tfopen(_T("fbneo.chm"), _T("r"));
				if (fp) {
					fclose(fp);
					ShellExecute(NULL, _T("open"), _T("fbneo.chm"), NULL, NULL, SW_SHOWNORMAL);
				}
			}
			break;
		}

		case MENU_WHATSNEW: {
			if (!nVidFullscreen) {
				FILE* fp = _tfopen(_T("whatsnew.html"), _T("r"));
				if (fp) {
					fclose(fp);
					ShellExecute(NULL, _T("open"), _T("whatsnew.html"), NULL, NULL, SW_SHOWNORMAL);
				}
			}
			break;
		}

		case MENU_WWW_HOME:
			if (!nVidFullscreen) {
				ShellExecute(NULL, _T("open"), _T("https://neo-source.com/"), NULL, NULL, SW_SHOWNORMAL);
			}
			break;

		case MENU_WWW_NSFORUM:
			if (!nVidFullscreen) {
				ShellExecute(NULL, _T("open"), _T("https://neo-source.com/"), NULL, NULL, SW_SHOWNORMAL);
			}
			break;

		case MENU_WWW_GITHUB:
			if (!nVidFullscreen) {
				ShellExecute(NULL, _T("open"), _T("https://github.com/finalburnneo/FBNeo"), NULL, NULL, SW_SHOWNORMAL);
			}
			break;

//		default:
//			printf("  * Command %i sent.\n");

	}

	switch (nVidSelect) {
		case 0: {
			switch (id) {
				// Options for the Default DirectDraw blitter
				case MENU_BASIC_NORMAL:
					bVidScanlines = 0;
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_BASIC_SCAN:
					bVidScanlines = 1;
					bVidScanHalf = 0;
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_SCAN50:
					bVidScanlines = 1;
					bVidScanHalf = 1;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_BASIC_ROTSCAN:
					bVidScanRotate = !bVidScanRotate;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_FORCE_FLIP:
					bVidForceFlip = !bVidForceFlip;
					POST_INITIALISE_MESSAGE;
					break;
			}
			break;
		}
		case 1: {
			switch (id) {
				//	Options for the Direct3D blitter
				case MENU_DISABLEFX:
					bVidBilinear = 0;
					bVidScanlines = 0;
					nVidBlitterOpt[nVidSelect] &= 0xF40200FF;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_BILINEAR:
					bVidBilinear = !bVidBilinear;
					if (bVidOkay && (bRunPause || !bDrvOkay)) {
						VidRedraw();
					}
					break;

				case MENU_PHOSPHOR:
					bVidScanDelay = !bVidScanDelay;
					break;

				case MENU_ENHANCED_NORMAL:
					nVidBlitterOpt[nVidSelect] &= ~0x00110000;
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_ENHANCED_SCAN:
					bVidScanlines = !bVidScanlines;
					nVidBlitterOpt[nVidSelect] &= ~0x00010000;
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_RGBEFFECTS:
					//nVidBlitterOpt[nVidSelect] &= ~0x00100000;
					nVidBlitterOpt[nVidSelect] |= 0x00010000;
					//bVidScanlines = 0;
					ScrnSize();
					VidInit();
					if (bVidScanlines) {
						ScrnSize();
						VidInit();
					}
					if (bVidOkay && (bRunPause || !bDrvOkay)) {
						VidRedraw();
					}
					break;
				case MENU_3DPROJECTION:
					//nVidBlitterOpt[nVidSelect] &= ~0x00010000;
					nVidBlitterOpt[nVidSelect] |= 0x00100000;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_EFFECT_AUTO:
					nVidBlitterOpt[nVidSelect] &= ~0x001000000;
					nVidBlitterOpt[nVidSelect] |= 0x00030000;
					POST_INITIALISE_MESSAGE;
					if (bVidOkay && (bRunPause || !bDrvOkay)) {
						VidRedraw();
					}
					break;
				case MENU_EFFECT_01:
				case MENU_EFFECT_02:
				case MENU_EFFECT_03:
				case MENU_EFFECT_04:
				case MENU_EFFECT_05:
				case MENU_EFFECT_06:
				case MENU_EFFECT_07:
				case MENU_EFFECT_08:
				case MENU_EFFECT_09:
				case MENU_EFFECT_0A:
				case MENU_EFFECT_0B:
				case MENU_EFFECT_0C:
				case MENU_EFFECT_0D:
				case MENU_EFFECT_0E:
				case MENU_EFFECT_0F:
				case MENU_EFFECT_10:
					nVidBlitterOpt[nVidSelect] &= ~0x001300FF;
					nVidBlitterOpt[nVidSelect] |= 0x00010008 + id - MENU_EFFECT_01;
					POST_INITIALISE_MESSAGE;
					if (bVidOkay && (bRunPause || !bDrvOkay)) {
						VidRedraw();
					}
					break;

				case MENU_ENHANCED_ROTSCAN:
					bVidScanRotate = !bVidScanRotate;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_PRESCALE:
					nVidBlitterOpt[nVidSelect] ^= 0x01000000;
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_SOFTFX:
					nVidBlitterOpt[nVidSelect] ^= 0x02000000;
					nVidBlitterOpt[nVidSelect] |= 0x01000000;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_ENHANCED_SOFT_STRETCH:
				case MENU_ENHANCED_SOFT_SCALE2X:
				case MENU_ENHANCED_SOFT_SCALE3X:
				case MENU_ENHANCED_SOFT_2XPM_LQ:
				case MENU_ENHANCED_SOFT_2XPM_HQ:
				case MENU_ENHANCED_SOFT_EAGLE:
				case MENU_ENHANCED_SOFT_SUPEREAGLE:
				case MENU_ENHANCED_SOFT_2XSAI:
				case MENU_ENHANCED_SOFT_SUPER2XSAI:
				case MENU_ENHANCED_SOFT_SUPEREAGLE_VBA:
				case MENU_ENHANCED_SOFT_2XSAI_VBA:
				case MENU_ENHANCED_SOFT_SUPER2XSAI_VBA:
				case MENU_ENHANCED_SOFT_SUPERSCALE:
				case MENU_ENHANCED_SOFT_SUPERSCALE75:
				case MENU_ENHANCED_SOFT_HQ2X:
				case MENU_ENHANCED_SOFT_HQ3X:
				case MENU_ENHANCED_SOFT_HQ4X:
				case MENU_ENHANCED_SOFT_HQ2XS_VBA:
				case MENU_ENHANCED_SOFT_HQ3XS_VBA:
				case MENU_ENHANCED_SOFT_HQ2XS_SNES9X:
				case MENU_ENHANCED_SOFT_HQ3XS_SNES9X:
				case MENU_ENHANCED_SOFT_HQ2XBOLD:
				case MENU_ENHANCED_SOFT_HQ3XBOLD:
				case MENU_ENHANCED_SOFT_EPXB:
				case MENU_ENHANCED_SOFT_EPXC:
				case MENU_ENHANCED_SOFT_2XBR_A:
				case MENU_ENHANCED_SOFT_2XBR_B:
				case MENU_ENHANCED_SOFT_2XBR_C:
				case MENU_ENHANCED_SOFT_3XBR_A:
				case MENU_ENHANCED_SOFT_3XBR_B:
				case MENU_ENHANCED_SOFT_3XBR_C:
				case MENU_ENHANCED_SOFT_4XBR_A:
				case MENU_ENHANCED_SOFT_4XBR_B:
				case MENU_ENHANCED_SOFT_4XBR_C:
				case MENU_ENHANCED_SOFT_DDT3X:
				case MENU_ENHANCED_SOFT_CRTx22:
				case MENU_ENHANCED_SOFT_CRTx33:
				case MENU_ENHANCED_SOFT_CRTx44: {
					nVidBlitterOpt[nVidSelect] &= 0x0FFFFFFF;
					nVidBlitterOpt[nVidSelect] |= 0x03000000 + ((long long)(id - MENU_ENHANCED_SOFT_STRETCH) << 32);
					POST_INITIALISE_MESSAGE;
					break;
				}
				case MENU_ENHANCED_SOFT_AUTOSIZE:
					nVidBlitterOpt[nVidSelect] ^= 0x04000000;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_ENHANCED_SCANINTENSITY:
					if (UseDialogs()) {
						InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
						AudBlankSound();
						if (!bVidScanlines) {
							bVidScanlines = 1;
							ScrnSize();
							VidInit();
							VidRedraw();
						}
						if (nVidBlitterOpt[nVidSelect] & 0x00010000) {
							nVidBlitterOpt[nVidSelect] &= ~0x00010000;

							ScrnSize();
							VidInit();
							VidRedraw();
						}
						ScanlineDialog();
						GameInpCheckMouse();
					}
					break;

				case MENU_PHOSPHORINTENSITY:
					if (UseDialogs()) {
						InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
						AudBlankSound();
						PhosphorDialog();
						bVidScanDelay = 1;
					}
					break;

				case MENU_3DSCREENANGLE:
					if (UseDialogs()) {
						InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
						AudBlankSound();
						if ((nVidBlitterOpt[nVidSelect] & 0x00100000) == 0) {
							nVidBlitterOpt[nVidSelect] &= ~0x00010000;
							nVidBlitterOpt[nVidSelect] |= 0x00100000;
							ScrnSize();
							VidInit();
							VidRedraw();
						}
						ScreenAngleDialog();
						if (!bRunPause) {
							GameInpCheckMouse();
						}
					}
					break;

				case MENU_FORCE_16BIT:
					bVidForce16bit = !bVidForce16bit;
					VidInit();
					if (bVidOkay && (bRunPause || !bDrvOkay)) {
						VidRedraw();
					}
					break;
				case MENU_TEXTUREMANAGE:
					if (nVidTransferMethod != 0) {
						nVidTransferMethod = 0;
					} else {
						nVidTransferMethod = 1;
					}
					VidInit();
					if (bVidOkay && (bRunPause || !bDrvOkay)) {
						VidRedraw();
					}
					break;
			}
			break;
		}
		case 2: {
			switch (id) {
				// Options for the DirectDraw Software Effects blitter
				case MENU_SOFTFX_SOFT_STRETCH:
				case MENU_SOFTFX_SOFT_SCALE2X:
				case MENU_SOFTFX_SOFT_SCALE3X:
				case MENU_SOFTFX_SOFT_2XPM_LQ:
				case MENU_SOFTFX_SOFT_2XPM_HQ:
				case MENU_SOFTFX_SOFT_EAGLE:
				case MENU_SOFTFX_SOFT_SUPEREAGLE:
				case MENU_SOFTFX_SOFT_2XSAI:
				case MENU_SOFTFX_SOFT_SUPER2XSAI:
				case MENU_SOFTFX_SOFT_SUPEREAGLE_VBA:
				case MENU_SOFTFX_SOFT_2XSAI_VBA:
				case MENU_SOFTFX_SOFT_SUPER2XSAI_VBA:
				case MENU_SOFTFX_SOFT_SUPERSCALE:
				case MENU_SOFTFX_SOFT_SUPERSCALE75:
				case MENU_SOFTFX_SOFT_HQ2X:
				case MENU_SOFTFX_SOFT_HQ3X:
				case MENU_SOFTFX_SOFT_HQ4X:
				case MENU_SOFTFX_SOFT_HQ2XS_VBA:
				case MENU_SOFTFX_SOFT_HQ3XS_VBA:
				case MENU_SOFTFX_SOFT_HQ2XS_SNES9X:
				case MENU_SOFTFX_SOFT_HQ3XS_SNES9X:
				case MENU_SOFTFX_SOFT_HQ2XBOLD:
				case MENU_SOFTFX_SOFT_HQ3XBOLD:
				case MENU_SOFTFX_SOFT_EPXB:
				case MENU_SOFTFX_SOFT_EPXC:
				case MENU_SOFTFX_SOFT_2XBR_A:
				case MENU_SOFTFX_SOFT_2XBR_B:
				case MENU_SOFTFX_SOFT_2XBR_C:
				case MENU_SOFTFX_SOFT_3XBR_A:
				case MENU_SOFTFX_SOFT_3XBR_B:
				case MENU_SOFTFX_SOFT_3XBR_C:
				case MENU_SOFTFX_SOFT_4XBR_A:
				case MENU_SOFTFX_SOFT_4XBR_B:
				case MENU_SOFTFX_SOFT_4XBR_C:
				case MENU_SOFTFX_SOFT_DDT3X:
				case MENU_SOFTFX_SOFT_CRTx22:
				case MENU_SOFTFX_SOFT_CRTx33:
				case MENU_SOFTFX_SOFT_CRTx44:
					nVidBlitterOpt[nVidSelect] &= ~0xFF;
					nVidBlitterOpt[nVidSelect] |= id - MENU_SOFTFX_SOFT_STRETCH;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_SOFTFX_SOFT_AUTOSIZE:
					nVidBlitterOpt[nVidSelect] ^= 0x0100;
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_SOFT_DIRECTACCESS:
					nVidBlitterOpt[nVidSelect] ^= 0x0200;
					POST_INITIALISE_MESSAGE;
					break;
			}
			break;
		}
		case 3:
			switch (id) {
				// Options for the DirectX Graphics 9 blitter
				case MENU_DX9_POINT:
					nVidBlitterOpt[nVidSelect] &= ~(3 << 24);
					nVidBlitterOpt[nVidSelect] |=  (0 << 24);
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_DX9_LINEAR:
					nVidBlitterOpt[nVidSelect] &= ~(3 << 24);
					nVidBlitterOpt[nVidSelect] |=  (1 << 24);
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_DX9_CUBIC:
					nVidBlitterOpt[nVidSelect] &= ~(3 << 24);
					nVidBlitterOpt[nVidSelect] |=  (2 << 24);
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_DX9_CUBIC_LIGHT:
					dVidCubicB = 0.0;
					dVidCubicC = 0.0;
					VidRedraw();
					break;
				case MENU_DX9_CUBIC_BSPLINE:
					dVidCubicB = 1.0;
					dVidCubicC = 0.0;
					VidRedraw();
					break;
				case MENU_DX9_CUBIC_NOTCH:
					dVidCubicB =  3.0 / 2.0;
					dVidCubicC = -0.25;
					VidRedraw();
					break;
				case MENU_DX9_CUBIC_OPTIMAL:
					dVidCubicB = 1.0 / 3.0;
					dVidCubicC = 1.0 / 3.0;
					VidRedraw();
					break;
				case MENU_DX9_CUBIC_CATMULL:
					dVidCubicB = 0.0;
					dVidCubicC = 0.5;
					VidRedraw();
					break;
				case MENU_DX9_CUBIC_SHARP:
					dVidCubicB = 0.0;
					dVidCubicC = 1.0;
					VidRedraw();
					break;

/*
					if (UseDialogs()) {
						InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
						AudBlankSound();
						if ((nVidBlitterOpt[nVidSelect] & (3 << 24)) !=  (2 << 24)) {
							nVidBlitterOpt[nVidSelect] &= ~(3 << 24);
							nVidBlitterOpt[nVidSelect] |=  (2 << 24);
							ScrnSize();
							VidInit();
							VidRedraw();
						}
						CubicSharpnessDialog();
						GameInpCheckMouse();
					}
					break;
*/

				case MENU_EXP_SCAN:
					bVidScanlines = !bVidScanlines;
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_EXP_SCANINTENSITY:
					if (UseDialogs()) {
						InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
						AudBlankSound();
						if (!bVidScanlines) {
							bVidScanlines = 1;
							ScrnSize();
							VidInit();
							VidRedraw();
						}
						ScanlineDialog();
						GameInpCheckMouse();
					}
					break;

				case MENU_DX9_FPTERXTURES:
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_DX9_FORCE_PS14:
					nVidBlitterOpt[nVidSelect] ^=  (1 <<  9);
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_DX9_CUBIC0:
					nVidBlitterOpt[nVidSelect] &= ~(7 << 28);
					nVidBlitterOpt[nVidSelect] |=  (0 << 28);

					nVidBlitterOpt[nVidSelect] |=  (1 <<  8);
					nVidBlitterOpt[nVidSelect] |=  (1 <<  9);
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_DX9_CUBIC1:
					nVidBlitterOpt[nVidSelect] &= ~(7 << 28);
					nVidBlitterOpt[nVidSelect] |=  (1 << 28);

					nVidBlitterOpt[nVidSelect] |=  (1 <<  8);
					nVidBlitterOpt[nVidSelect] |=  (1 <<  9);
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_DX9_CUBIC2:
					nVidBlitterOpt[nVidSelect] &= ~(7 << 28);
					nVidBlitterOpt[nVidSelect] |=  (2 << 28);

					nVidBlitterOpt[nVidSelect] |=  (1 <<  8);
					nVidBlitterOpt[nVidSelect] |=  (1 <<  9);
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_DX9_CUBIC3:
					nVidBlitterOpt[nVidSelect] &= ~(7 << 28);
					nVidBlitterOpt[nVidSelect] |=  (3 << 28);

					nVidBlitterOpt[nVidSelect] |=  (1 <<  8);
					nVidBlitterOpt[nVidSelect] |=  (1 <<  9);
					POST_INITIALISE_MESSAGE;
					break;
				case MENU_DX9_CUBIC4:
					nVidBlitterOpt[nVidSelect] &= ~(7 << 28);
					nVidBlitterOpt[nVidSelect] |=  (4 << 28);

					nVidBlitterOpt[nVidSelect] &= ~(1 <<  8);
					nVidBlitterOpt[nVidSelect] &= ~(1 <<  9);
					POST_INITIALISE_MESSAGE;
					break;

			}
			break;
		case 4:
			switch (id) {
				// Options for the DirectX Graphics 9 Alternate blitter
				case MENU_DX9_ALT_POINT:
					bVidDX9Bilinear = 0;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_DX9_ALT_LINEAR:
					bVidDX9Bilinear = 1;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_DX9_ALT_SOFT_STRETCH:
				case MENU_DX9_ALT_SOFT_SCALE2X:
				case MENU_DX9_ALT_SOFT_SCALE3X:
				case MENU_DX9_ALT_SOFT_2XPM_LQ:
				case MENU_DX9_ALT_SOFT_2XPM_HQ:
				case MENU_DX9_ALT_SOFT_EAGLE:
				case MENU_DX9_ALT_SOFT_SUPEREAGLE:
				case MENU_DX9_ALT_SOFT_2XSAI:
				case MENU_DX9_ALT_SOFT_SUPER2XSAI:
				case MENU_DX9_ALT_SOFT_SUPEREAGLE_VBA:
				case MENU_DX9_ALT_SOFT_2XSAI_VBA:
				case MENU_DX9_ALT_SOFT_SUPER2XSAI_VBA:
				case MENU_DX9_ALT_SOFT_SUPERSCALE:
				case MENU_DX9_ALT_SOFT_SUPERSCALE75:
				case MENU_DX9_ALT_SOFT_HQ2X:
				case MENU_DX9_ALT_SOFT_HQ3X:
				case MENU_DX9_ALT_SOFT_HQ4X:
				case MENU_DX9_ALT_SOFT_HQ2XS_VBA:
				case MENU_DX9_ALT_SOFT_HQ3XS_VBA:
				case MENU_DX9_ALT_SOFT_HQ2XS_SNES9X:
				case MENU_DX9_ALT_SOFT_HQ3XS_SNES9X:
				case MENU_DX9_ALT_SOFT_HQ2XBOLD:
				case MENU_DX9_ALT_SOFT_HQ3XBOLD:
				case MENU_DX9_ALT_SOFT_EPXB:
				case MENU_DX9_ALT_SOFT_EPXC:
				case MENU_DX9_ALT_SOFT_2XBR_A:
				case MENU_DX9_ALT_SOFT_2XBR_B:
				case MENU_DX9_ALT_SOFT_2XBR_C:
				case MENU_DX9_ALT_SOFT_3XBR_A:
				case MENU_DX9_ALT_SOFT_3XBR_B:
				case MENU_DX9_ALT_SOFT_3XBR_C:
				case MENU_DX9_ALT_SOFT_4XBR_A:
				case MENU_DX9_ALT_SOFT_4XBR_B:
				case MENU_DX9_ALT_SOFT_4XBR_C:
				case MENU_DX9_ALT_SOFT_DDT3X:
				case MENU_DX9_ALT_SOFT_CRTx22:
				case MENU_DX9_ALT_SOFT_CRTx33:
				case MENU_DX9_ALT_SOFT_CRTx44:
					nVidBlitterOpt[nVidSelect] &= ~0xFF;
					nVidBlitterOpt[nVidSelect] |= id - MENU_DX9_ALT_SOFT_STRETCH;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_DX9_ALT_SOFT_AUTOSIZE:
					nVidBlitterOpt[nVidSelect] ^= 0x0100;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_DX9_ALT_HARDWAREVERTEX:
					bVidHardwareVertex = !bVidHardwareVertex;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_DX9_ALT_MOTIONBLUR:
					bVidMotionBlur = !bVidMotionBlur;
					POST_INITIALISE_MESSAGE;
					break;

				case MENU_DX9_ALT_HARD_FX_NONE:
				case MENU_DX9_ALT_HARD_FX_CRT_APERTURE:
				case MENU_DX9_ALT_HARD_FX_CRT_CALIGARI:
				case MENU_DX9_ALT_HARD_FX_CRT_CGWG_FAST:
				case MENU_DX9_ALT_HARD_FX_CRT_EASY_MODE:
				case MENU_DX9_ALT_HARD_FX_CRT_STANDARD:
				case MENU_DX9_ALT_HARD_FX_CRT_BICUBIC:
				case MENU_DX9_ALT_HARD_FX_CRT_CGA:
					nVidDX9HardFX = id - MENU_DX9_ALT_HARD_FX_NONE;
					break;

				case MENU_DX9_ALT_FORCE_16BIT:
					bVidForce16bitDx9Alt = !bVidForce16bitDx9Alt;
					POST_INITIALISE_MESSAGE;
					break;
			}
			break;
	}

	if (hwndChat) {
		switch (codeNotify) {
			case EN_CHANGE: {
				bEditTextChanged = true;
				SendMessage(hwndChat, WM_GETTEXT, (WPARAM)MAX_CHAT_SIZE + 1, (LPARAM)EditText);
				break;
			}
			case EN_KILLFOCUS: {
				SetFocus(hwndChat);
				break;
			}
			case EN_MAXTEXT: {
				VidSNewShortMsg(FBALoadStringEx(hAppInst, IDS_NETPLAY_TOOMUCH, true), 0xFF3F3F);
				break;
			}
		}
	}

	MenuUpdate();
}

// Block screensaver and windows menu if needed
static int OnSysCommand(HWND, UINT sysCommand, int, int)
{
	switch (sysCommand) {
		case SC_MONITORPOWER:
		case SC_SCREENSAVE: {
			if (!bRunPause && bDrvOkay) {
				return 1;
			}
			break;
		}
		case SC_KEYMENU:
		case SC_MOUSEMENU: {
			if (kNetGame && !bModelessMenu) {
				return 1;
			}
			break;
		}
	}

	return 0;
}

static void OnSize(HWND, UINT state, int cx, int cy)
{
	if (state == SIZE_MINIMIZED) {
		bMaximized = false;
	} else {
		bool bSizeChanged = false;

		MoveWindow(hRebar, 0, 0, cx, nMenuHeight, TRUE);

		if (hwndChat) {
			MoveWindow(hwndChat, 0, cy - 32, cx, 32, FALSE);
		}

		if (state == SIZE_MAXIMIZED) {
			if (!bMaximized) {
				bSizeChanged = true;
			}
			bMaximized = true;
		}
		if (state == SIZE_RESTORED) {
			if (bMaximized) {
				bSizeChanged = true;
			}
			bMaximized = false;
		}

		if (bSizeChanged) {
			RefreshWindow(true);
		} else {
			RefreshWindow(false);
		}
	}
}

static void OnEnterSizeMove(HWND)
{
	RECT rect;

	AudBlankSound();

	GetClientRect(hScrnWnd, &rect);
	nPrevWidth = rect.right;
	nPrevHeight = rect.bottom;
}

static void OnExitSizeMove(HWND)
{
	RECT rect;

	GetClientRect(hScrnWnd, &rect);
	if (rect.right != nPrevWidth || rect.bottom != nPrevHeight) {
		RefreshWindow(true);
	}

	GetWindowRect(hScrnWnd, &rect);
	nWindowPosX = rect.left;
	nWindowPosY = rect.top;
}

static void OnEnterIdle(HWND /*hwnd*/, UINT /*source*/, HWND /*hwndSource*/)
{
	MSG Message;

    // Modeless dialog is idle
    while (kNetGame && !PeekMessage(&Message, NULL, 0, 0, PM_NOREMOVE)) {
		RunIdle();
	}
}

static void OnEnterMenuLoop(HWND, BOOL)
{
	if (!bModelessMenu) {
		InputSetCooperativeLevel(false, bAlwaysProcessKeyboardInput);
		AudBlankSound();
	} else {
		if (!kNetGame && bAutoPause) {
			bRunPause = 1;
		}
	}
}

static void OnExitMenuLoop(HWND, BOOL)
{
	if (!bModelessMenu) {
		GameInpCheckMouse();
	}
}

static int ScrnRegister()
{
	WNDCLASSEX WndClassEx;
	ATOM Atom = 0;

	// Register the window class
	memset(&WndClassEx, 0, sizeof(WndClassEx)); 		// Init structure to all zeros
	WndClassEx.cbSize			= sizeof(WndClassEx);
	WndClassEx.style			= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_CLASSDC;// These cause flicker in the toolbar
	WndClassEx.lpfnWndProc		= ScrnProc;
	WndClassEx.hInstance		= hAppInst;
	WndClassEx.hIcon			= LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_APP));
	WndClassEx.hCursor			= LoadCursor(NULL, IDC_ARROW);
	WndClassEx.hbrBackground	= static_cast<HBRUSH>( GetStockObject ( BLACK_BRUSH ));
	WndClassEx.lpszClassName	= szClass;

	// Register the window class with the above information:
	Atom = RegisterClassEx(&WndClassEx);
	if (Atom) {
		return 0;
	} else {
		return 1;
	}
}

int ScrnSize()
{
	int x, y, w, h, ew, eh;
	int nScrnWidth, nScrnHeight;
	int nWorkWidth, nWorkHeight;
	int nBmapWidth = nVidImageWidth, nBmapHeight = nVidImageHeight;
	int nGameAspectX = 4, nGameAspectY = 3;
	int nMaxSize;

	// SystemWorkArea = resolution of desktop
	// RealWorkArea = resolution of desktop - taskbar (if avail)
	RECT RealWorkArea;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &RealWorkArea, 0);	// Find the size of the visible WorkArea

	HMONITOR monitor = MonitorFromRect(&RealWorkArea, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO mi;

	memset(&mi, 0, sizeof(mi));
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(monitor, &mi);
	SystemWorkArea = mi.rcMonitor; // needs to be set to monitor's resolution for proper aspect calculation

	if (hScrnWnd == NULL || nVidFullscreen) {
		return 1;
	}

	if (bDrvOkay) {
		if ((BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) && (nVidRotationAdjust & 1)) {
			BurnDrvGetVisibleSize(&nBmapHeight, &nBmapWidth);
			BurnDrvGetAspect(&nGameAspectY, &nGameAspectX);
		} else {
			BurnDrvGetVisibleSize(&nBmapWidth, &nBmapHeight);
			BurnDrvGetAspect(&nGameAspectX, &nGameAspectY);
		}

		if (nBmapWidth <= 0 || nBmapHeight <= 0) {
			return 1;
		}
	}

	nDragX = GetSystemMetrics(SM_CXDRAG) / 2;
	nDragY = GetSystemMetrics(SM_CYDRAG) / 2;

	nScrnWidth = SystemWorkArea.right - SystemWorkArea.left;
	nScrnHeight = SystemWorkArea.bottom - SystemWorkArea.top;

	nWorkWidth = RealWorkArea.right - RealWorkArea.left;
	nWorkHeight = RealWorkArea.bottom - RealWorkArea.top;

	if (nVidSelect == 2 && nVidBlitterOpt[2] & 0x0100) {								// The Software effects blitter uses a fixed size
		nMaxSize = 9;
	} else {
		if (nWindowSize) {
			nMaxSize = nWindowSize;
			if (bDrvOkay && nWindowSize == 2 && nBmapWidth >= 400 && nBmapHeight >= 400) {
				// For Popeye, Hole Land and Syvalion, MCR..etc. when running Windowed: Double Size
				bprintf(PRINT_NORMAL, _T("  * Game is double-sized to begin with.\n"));
				nMaxSize = 1;
			}
		} else {
			if (nBmapWidth < nBmapHeight) {
				if (nWorkHeight <= 600) {
					nMaxSize = 1;
				} else {
					if (nWorkHeight <= 960) {
						nMaxSize = 2;
					} else {
						if (nWorkHeight <= 1280) {
							nMaxSize = 3;
						} else {
							nMaxSize = 4;
						}
					}
				}
			} else {
				if (nWorkWidth <= 640) {
					nMaxSize = 1;
				} else {
					if (nWorkWidth <= 1152) {
						nMaxSize = 2;
					} else {
						if (nWorkWidth <= 1600) {
							nMaxSize = 3;
						} else {
							nMaxSize = 4;
						}
					}
				}
			}
		}
	}

	// Find the width and height
	w = nWorkWidth;
	h = nWorkHeight;

	// Find out how much space is taken up by the borders
	ew = GetSystemMetrics(SM_CXSIZEFRAME) << 1;
	eh = GetSystemMetrics(SM_CYSIZEFRAME) << 1;

	// Visual Studio 2012 (seems to have an issue with these, other reports on the web about it too
#if defined _MSC_VER
	#if _MSC_VER >= 1700
		// using the old XP supporting SDK we don't need to alter anything
		#if !defined BUILD_VS_XP_TARGET
			ew <<= 1;
			eh <<= 1;
		#endif
	#endif
#endif

	if (bMenuEnabled) {
		eh += GetSystemMetrics(SM_CYCAPTION);
		eh += nMenuHeight;
	} else {
		eh += 1 << 1;
		ew += 1 << 1;
	}

	if (bMenuEnabled || !bVidScanlines || nVidSelect == 2) {
		// Subtract the border space
		w -= ew;
		h -= eh;
	}

	if ((bVidCorrectAspect || bVidFullStretch) && !(nVidSelect == 2 && (nVidBlitterOpt[2] & 0x0100) == 0)) {

		int ww = w;
		int hh = h;

		do {
			if (nBmapWidth < nBmapHeight && bVidScanRotate) {
				if (ww > nBmapWidth * nMaxSize) {
					ww = nBmapWidth * nMaxSize;
				}
				if (hh > (INT64)ww * nVidScrnAspectX * nGameAspectY * nScrnHeight / (nScrnWidth * nVidScrnAspectY * nGameAspectX)) {
					hh = (INT64)ww * nVidScrnAspectX * nGameAspectY * nScrnHeight / (nScrnWidth * nVidScrnAspectY * nGameAspectX);
				}
			} else {
				if (hh > nBmapHeight * nMaxSize) {
					hh = nBmapHeight * nMaxSize;
				}
				if (ww > (INT64)hh * nVidScrnAspectY * nGameAspectX * nScrnWidth / (nScrnHeight * nVidScrnAspectX * nGameAspectY)) {
					ww = (INT64)hh * nVidScrnAspectY * nGameAspectX * nScrnWidth / (nScrnHeight * nVidScrnAspectX * nGameAspectY);
				}
			}
		} while ((ww > w || hh > h) && nMaxSize-- > 1);
		w =	ww;
		h = hh;
	} else {
		while ((nBmapWidth * nMaxSize > w || nBmapHeight * nMaxSize > h) && nMaxSize > 1) {
			nMaxSize--;
		}

		if (w > nBmapWidth * nMaxSize || h > nBmapHeight * nMaxSize) {
			w = nBmapWidth * nMaxSize;
			h = nBmapHeight * nMaxSize;
		}
	}

	if (!bDrvOkay) {
		if (w < 304) w = 304;
		if (h < 224) h = 224;
	}

	RECT rect = { 0, 0, w, h };
	VidImageSize(&rect, nBmapWidth, nBmapHeight);
	w = rect.right - rect.left + ew;
	h = rect.bottom - rect.top + eh;

	x = nWindowPosX; y = nWindowPosY;
	if (x + w > SystemWorkArea.right || y + h > SystemWorkArea.bottom) {
		// Find the midpoint for the window
		x = SystemWorkArea.left + SystemWorkArea.right;
		x /= 2;
		y = SystemWorkArea.bottom + SystemWorkArea.top;
		y /= 2;

		x -= w / 2;
		y -= h / 2;
	}

	MenuUpdate();

	if (bMaximized) {
		// At this point: game window is maximized
		// different game is selected
		// usually the game window would be re-created using the "Window Size" selection
		// -but- since the game window was maximized, it should stay that way.
		PostMessage(hScrnWnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
	}

	MoveWindow(hScrnWnd, x, y, w, h, true);

//	SetWindowPos(hScrnWnd, NULL, x, y, w, h, SWP_NOREDRAW | SWP_NOACTIVATE | SWP_NOSENDCHANGING | SWP_NOZORDER);

	nWindowPosX = x; nWindowPosY = y;

  	return 0;
}

#include "neocdlist.h" // IsNeoGeoCD()

int ScrnTitle()
{
	TCHAR szText[1024] = _T("");

	// Create window title
	if (bDrvOkay) {
		TCHAR* pszPosition = szText;
		TCHAR* pszName = BurnDrvGetText(DRV_FULLNAME);

		pszPosition += _sntprintf(szText, 1024, _T(APP_TITLE) _T( " v%.20s") _T(SEPERATOR_1) _T("%s"), szAppBurnVer, pszName);
		while ((pszName = BurnDrvGetText(DRV_NEXTNAME | DRV_FULLNAME)) != NULL) {
			if (pszPosition + _tcslen(pszName) - 1024 > szText) {
				break;
			}
			pszPosition += _stprintf(pszPosition, _T(SEPERATOR_2) _T("%s"), pszName);
		}

		if (IsNeoGeoCD()) {
			NeoCDInfo_SetTitle();
			return 0;
		}

	} else {
		_stprintf(szText, _T(APP_TITLE) _T( " v%.20s") _T(SEPERATOR_1) _T("[%s]"), szAppBurnVer, FBALoadStringEx(hAppInst, IDS_SCRN_NOGAME, true));
	}

	SetWindowText(hScrnWnd, szText);
	return 0;
}


// Init the screen window (create it)
int ScrnInit()
{
	REBARINFO rebarInfo;
	REBARBANDINFO rebarBandInfo;
	RECT rect;
	int nWindowStyles, nWindowExStyles;

	ScrnExitLua();
	ScrnExit();

	if (ScrnRegister() != 0) {
		return 1;
	}

	if (nVidFullscreen) {
		nWindowStyles = WS_POPUP;
		nWindowExStyles = 0;
	} else {
		if (bMenuEnabled) {
			nWindowStyles = WS_OVERLAPPEDWINDOW;
			nWindowExStyles = 0;
		} else {
			nWindowStyles = WS_MINIMIZEBOX | WS_POPUP | WS_SYSMENU | WS_THICKFRAME;
			nWindowExStyles = WS_EX_CLIENTEDGE;
		}
	}

	hScrnWnd = CreateWindowEx(nWindowExStyles, szClass, _T(APP_TITLE), nWindowStyles,
		0, 0, 0, 0,									   			// size of window
		NULL, NULL, hAppInst, NULL);

	if (hScrnWnd == NULL) {
		ScrnExit();
		return 1;
	}

	nMenuHeight = 0;
	if (!nVidFullscreen) {

		// Create the menu toolbar itself
		MenuCreate();

		// Create the toolbar
		if (bMenuEnabled) {
			// Create the Rebar control that will contain the menu toolbar
			hRebar = CreateWindowEx(WS_EX_TOOLWINDOW,
				REBARCLASSNAME, NULL,
				RBS_BANDBORDERS | CCS_NOPARENTALIGN | CCS_NODIVIDER | WS_BORDER | WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
				0, 0, 0, 0,
				hScrnWnd, NULL, hAppInst, NULL);

			rebarInfo.cbSize = sizeof(REBARINFO);
			rebarInfo.fMask = 0;
			rebarInfo.himl = NULL;

			SendMessage(hRebar, RB_SETBARINFO, 0, (LPARAM)&rebarInfo);

			// Add the menu toolbar to the rebar
			GetWindowRect(hMenubar, &rect);

			rebarBandInfo.cbSize		= sizeof(REBARBANDINFO);
			rebarBandInfo.fMask			= RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE | RBBIM_STYLE;// | RBBIM_BACKGROUND;
			rebarBandInfo.fStyle		= 0;//RBBS_GRIPPERALWAYS;// | RBBS_FIXEDBMP;
			rebarBandInfo.hwndChild		= hMenubar;
			rebarBandInfo.cxMinChild	= 100;
			rebarBandInfo.cyMinChild	= ((SendMessage(hMenubar, TB_GETBUTTONSIZE, 0, 0)) >> 16) + 1;
			rebarBandInfo.cx			= rect.right - rect.left;

			SendMessage(hRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rebarBandInfo);

			GetWindowRect(hRebar, &rect);
			nMenuHeight = rect.bottom - rect.top;

		}

		ScrnTitle();
		ScrnSize();
	}

	return 0;
}

// Exit the screen window (destroy it)
int ScrnExit()
{
	// Ensure the window is destroyed
	DeActivateChat();

	if (hRebar) {
		DestroyWindow(hRebar);
		hRebar = NULL;
	}

	if (hScrnWnd) {
		DestroyWindow(hScrnWnd);
		hScrnWnd = NULL;
	}

	UnregisterClass(szClass, hAppInst);		// Unregister the scrn class

	MenuDestroy();

	return 0;
}

void Reinitialise()
{
	POST_INITIALISE_MESSAGE;
	VidReInitialise();
}
