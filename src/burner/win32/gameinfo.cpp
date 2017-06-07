// FBAlpha Game Info and Favorites handling
#include "burner.h"

static HWND hGameInfoDlg	= NULL;
static HWND hParent			= NULL;
static HWND hTabControl		= NULL;
static HMODULE hRiched		= NULL;
static HBITMAP hPreview		= NULL;
HBITMAP hGiBmp				= NULL;
static TCHAR szFullName[1024];
static int nGiDriverSelected;
static HBRUSH hWhiteBGBrush;

static FILE* OpenPreview(TCHAR *szPath)
{
	TCHAR szBaseName[MAX_PATH];
	TCHAR szFileName[MAX_PATH];

	FILE* fp = NULL;

	// Try to load a .PNG preview image
	_sntprintf(szBaseName, sizeof(szBaseName), _T("%s%s"), szPath, BurnDrvGetText(DRV_NAME));
	_stprintf(szFileName, _T("%s.png"), szBaseName);
	fp = _tfopen(szFileName, _T("rb"));

	if (!fp && BurnDrvGetText(DRV_PARENT)) {						// Try the parent
		_sntprintf(szBaseName, sizeof(szBaseName), _T("%s%s"), szPath, BurnDrvGetText(DRV_PARENT));
		_stprintf(szFileName, _T("%s.png"), szBaseName);
		fp = _tfopen(szFileName, _T("rb"));
	}

	return fp;
}

#define IMG_MAX_HEIGHT			380
#define IMG_MAX_WIDTH			700
#define IMG_DEFAULT_WIDTH		506
#define IMG_DEFAULT_WIDTH_V		285
#define IMG_ASPECT_4_3			1
#define IMG_ASPECT_PRESERVE		2

static void SetPreview(TCHAR* szPreviewDir, int nAspectFlag)
{
	HWND hDlg = hGameInfoDlg;

	HBITMAP hNewImage	= NULL;

	if (hGiBmp) {
		DeleteObject((HGDIOBJ)hGiBmp);
		hGiBmp = NULL;
	}
	
	// get image dimensions and work out what to resize to (default to 4:3)
	IMAGE img = { 0, 0, 0, 0, NULL, NULL, 0};
	int img_width = IMG_DEFAULT_WIDTH;
	int img_height = IMG_MAX_HEIGHT;
	
	FILE *fp = OpenPreview(szPreviewDir);
	if (fp) {
		PNGGetInfo(&img, fp);
		
		// vertical 3:4
		if (img.height > img.width) {
			img_width = IMG_DEFAULT_WIDTH_V;
		}
		
		// preserve aspect support
		if (nAspectFlag == IMG_ASPECT_PRESERVE) {
			double nAspect = (double)img.width / img.height;
			img_width = (int)((double)IMG_MAX_HEIGHT * nAspect);
			
			if (img_width > IMG_MAX_WIDTH) {
				img_width = IMG_MAX_WIDTH;
				img_height = (int)((double)IMG_MAX_WIDTH / nAspect);
			}
		}
		
		img_free(&img);
		fclose(fp);
	}
	
	fp = OpenPreview(szPreviewDir);
	if (fp) {
		hNewImage = PNGLoadBitmap(hDlg, fp, img_width, img_height, 3);
		fclose(fp);
	}

	if (hNewImage) {

		if(hGiBmp) DeleteObject((HGDIOBJ)hGiBmp);

		hGiBmp = hNewImage;

		if (bPngImageOrientation == 0) {
			SendDlgItemMessage(hDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hGiBmp);
			SendDlgItemMessage(hDlg, IDC_SCREENSHOT_V, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
			ShowWindow(GetDlgItem(hDlg, IDC_SCREENSHOT_V), SW_HIDE);
		} else {
			SendDlgItemMessage(hDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
			ShowWindow(GetDlgItem(hDlg, IDC_SCREENSHOT_V), SW_SHOW);
			SendDlgItemMessage(hDlg, IDC_SCREENSHOT_V, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hGiBmp);
		}

	} else {
		SendDlgItemMessage(hDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)LoadBitmap(hAppInst, MAKEINTRESOURCE(BMP_SPLASH)));
		SendDlgItemMessage(hDlg, IDC_SCREENSHOT_V, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
		ShowWindow(GetDlgItem(hDlg, IDC_SCREENSHOT_V), SW_HIDE);
	}
}

static int DisplayRomInfo()
{
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_H), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST1), SW_SHOW);
	UpdateWindow(hGameInfoDlg);
	
	return 0;
}

static int DisplaySampleInfo()
{
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_H), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST2), SW_SHOW);
	UpdateWindow(hGameInfoDlg);
	
	return 0;
}

static int DisplayHistory()
{
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_H), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST1), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST2), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), SW_SHOW);
	UpdateWindow(hGameInfoDlg);
	
	return 0;
}

static int GameInfoInit()
{
	// Get the games full name
	TCHAR szText[1024] = _T("");
	TCHAR* pszPosition = szText;
	TCHAR* pszName = BurnDrvGetText(DRV_FULLNAME);

	pszPosition += _sntprintf(szText, 1024, pszName);
	
	pszName = BurnDrvGetText(DRV_FULLNAME);
	while ((pszName = BurnDrvGetText(DRV_NEXTNAME | DRV_FULLNAME)) != NULL) {
		if (pszPosition + _tcslen(pszName) - 1024 > szText) {
			break;
		}
		pszPosition += _stprintf(pszPosition, _T(SEPERATOR_2) _T("%s"), pszName);
	}
	
	_tcscpy(szFullName, szText);
	
	_stprintf(szText, _T("%s") _T(SEPERATOR_1) _T("%s"), FBALoadStringEx(hAppInst, IDS_GAMEINFO_DIALOGTITLE, true), szFullName);
	
	// Set the window caption
	SetWindowText(hGameInfoDlg, szText);
	
	// Setup the tabs
	hTabControl = GetDlgItem(hGameInfoDlg, IDC_TAB1);
    TC_ITEM TCI; 
    TCI.mask = TCIF_TEXT; 

	UINT idsString[16] = {  IDS_GAMEINFO_ROMINFO, IDS_GAMEINFO_SAMPLES, IDS_GAMEINFO_HISTORY, IDS_GAMEINFO_INGAME, IDS_GAMEINFO_TITLE, IDS_GAMEINFO_SELECT, IDS_GAMEINFO_VERSUS, IDS_GAMEINFO_HOWTO, IDS_GAMEINFO_SCORES, IDS_GAMEINFO_BOSSES, IDS_GAMEINFO_GAMEOVER, IDS_GAMEINFO_FLYER, IDS_GAMEINFO_CABINET, IDS_GAMEINFO_MARQUEE, IDS_GAMEINFO_CONTROLS, IDS_GAMEINFO_PCB };
	
	for(int i = 0; i < 16; i++) {
		TCI.pszText = FBALoadStringEx(hAppInst, idsString[i], true);
		SendMessage(hTabControl, TCM_INSERTITEM, (WPARAM) i, (LPARAM) &TCI);
	}

	// Load the preview image
	hPreview = LoadBitmap(hAppInst, MAKEINTRESOURCE(BMP_SPLASH));
	
	// Display preview image
	SendDlgItemMessage(hGameInfoDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
	SendDlgItemMessage(hGameInfoDlg, IDC_SCREENSHOT_V, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);

	// Favorite game checkbox
	CheckDlgButton(hGameInfoDlg,IDFAVORITESET,(CheckFavorites(BurnDrvGetTextA(DRV_NAME)) != -1) ? BST_CHECKED : BST_UNCHECKED);

	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST1), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST2), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_H), SW_SHOW);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_V), SW_SHOW);
	UpdateWindow(hGameInfoDlg);

	nBurnDrvActive = nGiDriverSelected;
	DisplayRomInfo();
	
	// Display the game title
	TCHAR szItemText[1024];
	HWND hInfoControl = GetDlgItem(hGameInfoDlg, IDC_TEXTCOMMENT);
	SendMessage(hInfoControl, WM_SETTEXT, (WPARAM)0, (LPARAM)szFullName);
	
	// Display the romname
	bool bBracket = false;
	hInfoControl = GetDlgItem(hGameInfoDlg, IDC_TEXTROMNAME);
	_stprintf(szItemText, _T("%s"), BurnDrvGetText(DRV_NAME));
	if ((BurnDrvGetFlags() & BDF_CLONE) && BurnDrvGetTextA(DRV_PARENT)) {
		int nOldDrvSelect = nBurnDrvActive;
		pszName = BurnDrvGetText(DRV_PARENT);

		_stprintf(szItemText + _tcslen(szItemText), _T(" (clone of %s"), BurnDrvGetText(DRV_PARENT));

		for (nBurnDrvActive = 0; nBurnDrvActive < nBurnDrvCount; nBurnDrvActive++) {
			if (!_tcsicmp(pszName, BurnDrvGetText(DRV_NAME))) {
				break;
			}
		}
		if (nBurnDrvActive < nBurnDrvCount) {
			if (BurnDrvGetText(DRV_PARENT)) {
				_stprintf(szItemText + _tcslen(szItemText), _T(", uses ROMs from %s"), BurnDrvGetText(DRV_PARENT));
			}
		}
		nBurnDrvActive = nOldDrvSelect;
		bBracket = true;
	} else {
		if (BurnDrvGetTextA(DRV_PARENT)) {
			_stprintf(szItemText + _tcslen(szItemText), _T("%suses ROMs from %s"), bBracket ? _T(", ") : _T(" ("), BurnDrvGetText(DRV_PARENT));
			bBracket = true;
		}
	}
	if (BurnDrvGetTextA(DRV_SAMPLENAME)) {
		_stprintf(szItemText + _tcslen(szItemText), _T("%suses samples from %s"), bBracket ? _T(", ") : _T(" ("), BurnDrvGetText(DRV_SAMPLENAME));
		bBracket = true;
	}
	if (bBracket) {
		_stprintf(szItemText + _tcslen(szItemText), _T(")"));
	}
	SendMessage(hInfoControl, WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
	
	//Display the rom info
	bool bUseInfo = false;
	szItemText[0] = _T('\0');
	hInfoControl = GetDlgItem(hGameInfoDlg, IDC_TEXTROMINFO);
	if (BurnDrvGetFlags() & BDF_PROTOTYPE) {
		_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_SEL_PROTOTYPE, true));
		bUseInfo = true;
	}
	if (BurnDrvGetFlags() & BDF_BOOTLEG) {
		_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_SEL_BOOTLEG, true), bUseInfo ? _T(", ") : _T(""));
		bUseInfo = true;
	}
	if (BurnDrvGetFlags() & BDF_HACK) {
		_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_SEL_HACK, true), bUseInfo ? _T(", ") : _T(""));
		bUseInfo = true;
	}
	if (BurnDrvGetFlags() & BDF_HOMEBREW) {
		_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_SEL_HOMEBREW, true), bUseInfo ? _T(", ") : _T(""));
		bUseInfo = true;
	}						
	if (BurnDrvGetFlags() & BDF_DEMO) {
		_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_SEL_DEMO, true), bUseInfo ? _T(", ") : _T(""));
		bUseInfo = true;
	}
	TCHAR szPlayersMax[100];
	_stprintf(szPlayersMax, FBALoadStringEx(hAppInst, IDS_NUM_PLAYERS_MAX, true));
	_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_NUM_PLAYERS, true), bUseInfo ? _T(", ") : _T(""), BurnDrvGetMaxPlayers(), (BurnDrvGetMaxPlayers() != 1) ? szPlayersMax : _T(""));
	bUseInfo = true;
	if (BurnDrvGetText(DRV_BOARDROM)) {
		_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_BOARD_ROMS_FROM, true), bUseInfo ? _T(", ") : _T(""), BurnDrvGetText(DRV_BOARDROM));
		SendMessage(hInfoControl, WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
		bUseInfo = true;
	}
	SendMessage(hInfoControl, WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
	
	// Display the release info
	szItemText[0] = _T('\0');
	hInfoControl = GetDlgItem(hGameInfoDlg, IDC_TEXTSYSTEM);
	TCHAR szUnknown[100];
	TCHAR szCartridge[100];
	_stprintf(szUnknown, FBALoadStringEx(hAppInst, IDS_ERR_UNKNOWN, true));
	_stprintf(szCartridge, FBALoadStringEx(hAppInst, IDS_MVS_CARTRIDGE, true));
	_stprintf(szItemText, FBALoadStringEx(hAppInst, IDS_HARDWARE_DESC, true), BurnDrvGetTextA(DRV_MANUFACTURER) ? BurnDrvGetText(DRV_MANUFACTURER) : szUnknown, BurnDrvGetText(DRV_DATE), (((BurnDrvGetHardwareCode() & HARDWARE_SNK_MVS) == HARDWARE_SNK_MVS) && ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)) == HARDWARE_SNK_NEOGEO)? szCartridge : BurnDrvGetText(DRV_SYSTEM));
	SendMessage(hInfoControl, WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
	
	// Display any comments
	szItemText[0] = _T('\0');
	hInfoControl = GetDlgItem(hGameInfoDlg, IDC_TEXTNOTES);
	_stprintf(szItemText, _T("%s"), BurnDrvGetTextA(DRV_COMMENT) ? BurnDrvGetText(DRV_COMMENT) : _T(""));
	if (BurnDrvGetFlags() & BDF_HISCORE_SUPPORTED) {
		_stprintf(szItemText + _tcslen(szItemText), _T("%shigh scores supported"), _tcslen(szItemText) ? _T(", ") : _T(""));
	}
	SendMessage(hInfoControl, WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
	
	// Display the genre
	szItemText[0] = _T('\0');
	hInfoControl = GetDlgItem(hGameInfoDlg, IDC_TEXTGENRE);
	_stprintf(szItemText, _T("%s"), DecorateGenreInfo());
	SendMessage(hInfoControl, WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
	
	// Set up the rom info list
	HWND hList = GetDlgItem(hGameInfoDlg, IDC_LIST1);
	LV_COLUMN LvCol;
	LV_ITEM LvItem;
	
	ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT);
	
	memset(&LvCol, 0, sizeof(LvCol));
	LvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
	LvCol.cx = 200;
	LvCol.pszText = _T("Name");	
	SendMessage(hList, LVM_INSERTCOLUMN , 0, (LPARAM)&LvCol);
	LvCol.cx = 100;
	LvCol.pszText = _T("Size (bytes)");	
	SendMessage(hList, LVM_INSERTCOLUMN , 1, (LPARAM)&LvCol);
	LvCol.cx = 100;
	LvCol.pszText = _T("CRC32");	
	SendMessage(hList, LVM_INSERTCOLUMN , 2, (LPARAM)&LvCol);
	LvCol.cx = 200;
	LvCol.pszText = _T("Type");	
	SendMessage(hList, LVM_INSERTCOLUMN , 3, (LPARAM)&LvCol);
	LvCol.cx = 100;
	LvCol.pszText = _T("Flags");	
	SendMessage(hList, LVM_INSERTCOLUMN , 4, (LPARAM)&LvCol);
	LvCol.cx = 100;
	
	memset(&LvItem, 0, sizeof(LvItem));
	LvItem.mask=  LVIF_TEXT;
	LvItem.cchTextMax = 256;
	int RomPos = 0;
	for (int i = 0; i < 0x100; i++) { // assume max 0x100 roms per game
		int nRet;
		struct BurnRomInfo ri;
		char nLen[10] = "";
		char nCrc[10] = "";
		char *szRomName = NULL;
		char Type[100] = "";
		char FormatType[100] = "";

		memset(&ri, 0, sizeof(ri));

		nRet = BurnDrvGetRomInfo(&ri, i);
		nRet += BurnDrvGetRomName(&szRomName, i, 0);
		
		if (ri.nLen == 0) continue;		
		if (ri.nType & BRF_BIOS) continue;
		
		LvItem.iItem = RomPos;
		LvItem.iSubItem = 0;
		LvItem.pszText = ANSIToTCHAR(szRomName, NULL, 0);
		SendMessage(hList, LVM_INSERTITEM, 0, (LPARAM)&LvItem);
		
		sprintf(nLen, "%d", ri.nLen);
		LvItem.iSubItem = 1;
		LvItem.pszText = ANSIToTCHAR(nLen, NULL, 0);
		SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);
		
		sprintf(nCrc, "%08X", ri.nCrc);
		if (!(ri.nType & BRF_NODUMP)) {
			LvItem.iSubItem = 2;
			LvItem.pszText = ANSIToTCHAR(nCrc, NULL, 0);
			SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);
		}
		
		if (ri.nType & BRF_ESS) sprintf(Type, "%s, Essential", Type);
		if (ri.nType & BRF_OPT) sprintf(Type, "%s, Optional", Type);
		if (ri.nType & BRF_PRG)	sprintf(Type, "%s, Program", Type);
		if (ri.nType & BRF_GRA) sprintf(Type, "%s, Graphics", Type);
		if (ri.nType & BRF_SND) sprintf(Type, "%s, Sound", Type);
		if (ri.nType & BRF_BIOS) sprintf(Type, "%s, BIOS", Type);
		
		for (int j = 0; j < 98; j++) {
			FormatType[j] = Type[j + 2];
		}
		
		LvItem.iSubItem = 3;
		LvItem.pszText = ANSIToTCHAR(FormatType, NULL, 0);
		SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);
		
		LvItem.iSubItem = 4;
		LvItem.pszText = _T("");
		if (ri.nType & BRF_NODUMP) LvItem.pszText = _T("No Dump");
		SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);
		
		RomPos++;
	}
	
	// Check for board roms
	if (BurnDrvGetTextA(DRV_BOARDROM)) {
		char szBoardName[12] = "";
		unsigned int nOldDrvSelect = nBurnDrvActive;
		strcpy(szBoardName, BurnDrvGetTextA(DRV_BOARDROM));
			
		for (unsigned int i = 0; i < nBurnDrvCount; i++) {
			nBurnDrvActive = i;
			if (!strcmp(szBoardName, BurnDrvGetTextA(DRV_NAME))) break;
		}
			
		for (int j = 0; j < 0x100; j++) {
			int nRetBoard;
			struct BurnRomInfo riBoard;
			char nLenBoard[10] = "";
			char nCrcBoard[10] = "";
			char *szBoardRomName = NULL;
			char BoardType[100] = "";
			char BoardFormatType[100] = "";

			memset(&riBoard, 0, sizeof(riBoard));

			nRetBoard = BurnDrvGetRomInfo(&riBoard, j);
			nRetBoard += BurnDrvGetRomName(&szBoardRomName, j, 0);
		
			if (riBoard.nLen == 0) continue;
				
			LvItem.iItem = RomPos;
			LvItem.iSubItem = 0;
			LvItem.pszText = ANSIToTCHAR(szBoardRomName, NULL, 0);
			SendMessage(hList, LVM_INSERTITEM, 0, (LPARAM)&LvItem);
		
			sprintf(nLenBoard, "%d", riBoard.nLen);
			LvItem.iSubItem = 1;
			LvItem.pszText = ANSIToTCHAR(nLenBoard, NULL, 0);
			SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);
		
			sprintf(nCrcBoard, "%08X", riBoard.nCrc);
			if (!(riBoard.nType & BRF_NODUMP)) {
				LvItem.iSubItem = 2;
				LvItem.pszText = ANSIToTCHAR(nCrcBoard, NULL, 0);
				SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);
			}
			
			if (riBoard.nType & BRF_ESS) sprintf(BoardType, "%s, Essential", BoardType);
			if (riBoard.nType & BRF_OPT) sprintf(BoardType, "%s, Optional", BoardType);
			if (riBoard.nType & BRF_PRG) sprintf(BoardType, "%s, Program", BoardType);
			if (riBoard.nType & BRF_GRA) sprintf(BoardType, "%s, Graphics", BoardType);
			if (riBoard.nType & BRF_SND) sprintf(BoardType, "%s, Sound", BoardType);
			if (riBoard.nType & BRF_BIOS) sprintf(BoardType, "%s, BIOS", BoardType);
		
			for (int k = 0; k < 98; k++) {
				BoardFormatType[k] = BoardType[k + 2];
			}
		
			LvItem.iSubItem = 3;
			LvItem.pszText = ANSIToTCHAR(BoardFormatType, NULL, 0);
			SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);
		
			LvItem.iSubItem = 4;
			LvItem.pszText = _T("");
			if (riBoard.nType & BRF_NODUMP) LvItem.pszText = _T("No Dump");
			SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);
			
			RomPos++;
		}
		
		nBurnDrvActive = nOldDrvSelect;
	}
	
	// Set up the sample info list
	hList = GetDlgItem(hGameInfoDlg, IDC_LIST2);
	
	ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT);
	
	memset(&LvCol, 0, sizeof(LvCol));
	LvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
	LvCol.cx = 200;
	LvCol.pszText = _T("Name");	
	SendMessage(hList, LVM_INSERTCOLUMN , 0, (LPARAM)&LvCol);
		
	memset(&LvItem, 0, sizeof(LvItem));
	LvItem.mask=  LVIF_TEXT;
	LvItem.cchTextMax = 256;
	int SamplePos = 0;
	if (BurnDrvGetTextA(DRV_SAMPLENAME) != NULL) {
		for (int i = 0; i < 0x100; i++) { // assume max 0x100 samples per game
			int nRet;
			struct BurnSampleInfo si;
			char *szSampleName = NULL;

			memset(&si, 0, sizeof(si));

			nRet = BurnDrvGetSampleInfo(&si, i);
			nRet += BurnDrvGetSampleName(&szSampleName, i, 0);
		
			if (si.nFlags == 0) continue;		
		
			LvItem.iItem = SamplePos;
			LvItem.iSubItem = 0;
			LvItem.pszText = ANSIToTCHAR(szSampleName, NULL, 0);
			SendMessage(hList, LVM_INSERTITEM, 0, (LPARAM)&LvItem);
		
			SamplePos++;
		}
	}
	
	// Get the history info
	CHAR szFileName[MAX_PATH] = "";
	sprintf(szFileName, "%shistory.dat", TCHARToANSI(szAppHistoryPath, NULL, 0));
	
	FILE *fp = fopen(szFileName, "rt");	
	char Temp[10000];
	int inGame = 0;

#ifdef USE_SEGOE
	TCHAR szBuffer[50000] = _T("{\\rtf1\\ansi{\\fonttbl(\\f0\\fnil\\fcharset0 Verdana;)}{\\colortbl;\\red220\\green0\\blue0;\\red0\\green0\\blue0;}");
#else
	TCHAR szBuffer[50000] = _T("{\\rtf1\\ansi{\\fonttbl(\\f0\\fnil\\fcharset0 Segoe UI;)}{\\colortbl;\\red220\\green0\\blue0;\\red0\\green0\\blue0;}");
#endif
	
	if (fp) {		
		while (!feof(fp)) {
			char *Tokens;
			
			fgets(Temp, 10000, fp);
			if (!strncmp("$info=", Temp, 6)) {
				Tokens = strtok(Temp, "=,");
				while (Tokens != NULL) {
					if (!strcmp(Tokens, BurnDrvGetTextA(DRV_NAME))) {
						inGame = 1;
						break;
					}

					Tokens = strtok(NULL, "=,");
				}
			}
			
			if (inGame) {
				int nTitleWrote = 0;
				while (strncmp("$end", Temp, 4)) {
					fgets(Temp, 10000, fp);

					if (!strncmp("$", Temp, 1)) continue;
						
					if (!nTitleWrote) {
						_stprintf(szBuffer, _T("%s{\\b\\f0\\fs28\\cf1 %s}"), szBuffer, ANSIToTCHAR(Temp, NULL, 0));
					} else {
						_stprintf(szBuffer, _T("%s\\line"), szBuffer);	
						if (!strncmp("- ", Temp, 2)) {
							_stprintf(szBuffer, _T("%s{\\b\\f0\\fs16\\cf1 %s}"), szBuffer, ANSIToTCHAR(Temp, NULL, 0));
						} else {
							_stprintf(szBuffer, _T("%s{\\f0\\fs16\\cf2 %s}"), szBuffer, ANSIToTCHAR(Temp, NULL, 0));
						}
					}
						
					if (strcmp("\n", Temp)) nTitleWrote = 1;
				}
				break;
			}
		}
		fclose(fp);
	}
	
	_stprintf(szBuffer, _T("%s}"), szBuffer);
	SendMessage(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), WM_SETTEXT, (WPARAM)0, (LPARAM)szBuffer);
	
	// Make a white brush
	hWhiteBGBrush = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));
	
	return 0;
}

#define MAXFAVORITES 2000
char szFavorites[MAXFAVORITES][28];
INT32 nFavorites = 0;

void LoadFavorites()
{
	char szTemp[255];
	char szFileName[] = "config/favorites.dat";

	nFavorites = 0;
	memset(szFavorites, 0, sizeof(szFavorites));

	FILE *fp = fopen(szFileName, "rb");

	if (fp) {
		while (!feof(fp)) {
			memset(szTemp, 0, sizeof(szTemp));
			fgets(szTemp, 255, fp);

			// Get rid of the linefeed at the end
			INT32 nLen = strlen(szTemp);
			while (nLen > 0 && (szTemp[nLen - 1] == 0x0A || szTemp[nLen - 1] == 0x0D)) {
				szTemp[nLen - 1] = 0;
				nLen--;
			}

			if (strlen(szTemp) < 25 && strlen(szTemp) > 2) {
				strncpy(szFavorites[nFavorites++], szTemp, 25);
				//bprintf(0, _T("Loaded: %S.\n"), szFavorites[nFavorites-1]);
			}
		}
		fclose(fp);
	}
}

static void SaveFavorites()
{
	char szFileName[] = "config/favorites.dat";
	FILE *fp = fopen(szFileName, "wb");
	INT32 nSaved = 0;

	if (fp) {
		for (INT32 i = 0; i < nFavorites; i++) {
			if (strlen(szFavorites[i]) > 0) {
				fprintf(fp, "%s\n", szFavorites[i]);
				nSaved++;
			}
		}
		//bprintf(0, _T("Saved %X favorites.\n"), nSaved);
		fclose(fp);
	}
}

INT32 CheckFavorites(char *name)
{
	for (INT32 i = 0; i < nFavorites; i++) {
		if (!strcmp(name, szFavorites[i])) {
			return i;
		}
	}
	return -1;
}

static void AddFavorite(UINT8 addf)
{
	UINT32 nOldDrvSelect = nBurnDrvActive;
	char szBoardName[28] = "";

	LoadFavorites();

	nBurnDrvActive = nGiDriverSelected; // get driver name under cursor
	strcpy(szBoardName, BurnDrvGetTextA(DRV_NAME));
	nBurnDrvActive = nOldDrvSelect;

	INT32 inthere = CheckFavorites(szBoardName);

	if (addf && inthere == -1) { // add favorite
		//bprintf(0, _T("[add %S]"), szBoardName);
		strcpy(szFavorites[nFavorites++], szBoardName);
	}

	if (addf == 0 && inthere != -1) { // remove favorite
		//bprintf(0, _T("[remove %S]"), szBoardName);
		szFavorites[inthere][0] = '\0';
	}

	SaveFavorites();
}

void AddFavorite_Ext(UINT8 addf)
{ // For use outside of the gameinfo window.
	nGiDriverSelected = nBurnDrvActive;
	AddFavorite(addf);
}

static void MyEndDialog()
{
	SendDlgItemMessage(hGameInfoDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
	SendDlgItemMessage(hGameInfoDlg, IDC_SCREENSHOT_V, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
	
	if (hGiBmp) {
		DeleteObject((HGDIOBJ)hGiBmp);
		hGiBmp = NULL;
	}
	if (hPreview) {
		DeleteObject((HGDIOBJ)hPreview);
		hPreview = NULL;
	}
	
	hTabControl = NULL;
	memset(szFullName, 0, 1024);
	
	EndDialog(hGameInfoDlg, 0);
}

static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_INITDIALOG) {
		hGameInfoDlg = hDlg;

		if (bDrvOkay) {
			if (!kNetGame && bAutoPause) bRunPause = 1;
			AudSoundStop();
		}
		
		GameInfoInit();

		WndInMid(hDlg, hParent);
		SetFocus(hDlg);											// Enable Esc=close

		return TRUE;
	}
	
	if (Msg == WM_CLOSE) {
		MyEndDialog();
		DeleteObject(hWhiteBGBrush);
		
		EnableWindow(hScrnWnd, TRUE);
		DestroyWindow(hGameInfoDlg);
		
		FreeLibrary(hRiched);
		hRiched = NULL;
		
		if (bDrvOkay) {
			if(!bAltPause && bRunPause) bRunPause = 0;
			AudSoundPlay();
		}
		
		return 0;
	}

	if (Msg == WM_COMMAND) {
		int Id = LOWORD(wParam);
		int Notify = HIWORD(wParam);
		
		if (Id == IDCANCEL && Notify == BN_CLICKED) {
			SendMessage(hGameInfoDlg, WM_CLOSE, 0, 0);
			return 0;
		}
		
		if (Id == IDFAVORITESET && Notify == BN_CLICKED) {
			INT32 nButtonState = SendDlgItemMessage(hGameInfoDlg, IDFAVORITESET, BM_GETSTATE, 0, 0);

			AddFavorite((nButtonState & BST_CHECKED) ? 1 : 0);
			return 0;
		}

		if (Id == IDRESCANSET && Notify == BN_CLICKED) {
			nBurnDrvActive = nGiDriverSelected;
			// use the nBurnDrvActive value from when the Rom Info button was clicked, because it can/will change
			// even though the selection list [window below it] doesn't have focus. -dink
			// for proof/symptoms - uncomment the line below and click the 'rescan' button after moving the window to different places on the screen.
			//bprintf(0, _T("nBurnDrvActive %d nGiDriverSelected %d\n"), nBurnDrvActive, nGiDriverSelected);
			
			switch (BzipOpen(1)) {
				case 0: {
					gameAv[nGiDriverSelected] = 3;
					break;
				}
				
				case 2: {
					gameAv[nGiDriverSelected] = 1;
					break;
				}
				
				case 1: {
					gameAv[nGiDriverSelected] = 0;
					BzipClose();
					BzipOpen(0); // this time, get the missing roms/error message.
					{
						HWND hScrnWndtmp = hScrnWnd; // Crash preventer: Make the gameinfo dialog (this one) the parent modal for the popup message
						hScrnWnd = hGameInfoDlg;
						FBAPopupDisplay(PUF_TYPE_ERROR);
						hScrnWnd = hScrnWndtmp;
					}
					break;
				}
			}
			
			if (gameAv[nGiDriverSelected] > 0) {
				int TabPage = TabCtrl_GetCurSel(hTabControl);
				if (TabPage != 0) {
					MessageBox(hGameInfoDlg, FBALoadStringEx(hAppInst, IDS_ERR_LOAD_OK, true), FBALoadStringEx(hAppInst, IDS_ERR_INFORMATION, true), MB_OK);
				}
				if (TabPage == 0) {
					// if the romset is OK, just say so in the rom info dialog (instead of popping up a window)
					HWND hList = GetDlgItem(hGameInfoDlg, IDC_LIST1);
					LV_ITEM LvItem;
					memset(&LvItem, 0, sizeof(LvItem));
					LvItem.mask=  LVIF_TEXT;
					LvItem.cchTextMax = 256;
					LvItem.iSubItem = 4;
					LvItem.pszText = _T("Romset OK!");
					SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);
					UpdateWindow(hGameInfoDlg);
				}
			}
			BzipClose();
			WriteGameAvb();
			return 0;
		}
	}
	
	if (Msg == WM_NOTIFY) {
		NMHDR* pNmHdr = (NMHDR*)lParam;

		if (pNmHdr->code == TCN_SELCHANGE) {
			int TabPage = TabCtrl_GetCurSel(hTabControl);
			
			ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST1), SW_HIDE);
			ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST2), SW_HIDE);
			ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), SW_HIDE);
			ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_H), SW_SHOW);
			ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_V), SW_SHOW);
			UpdateWindow(hGameInfoDlg);
			
			nBurnDrvActive = nGiDriverSelected;

			if (TabPage == 0) DisplayRomInfo();
			if (TabPage == 1) DisplaySampleInfo();
			if (TabPage == 2) DisplayHistory();
			if (TabPage == 3) SetPreview(szAppPreviewsPath, IMG_ASPECT_4_3);
			if (TabPage == 4) SetPreview(szAppTitlesPath, IMG_ASPECT_4_3);
			if (TabPage == 5) SetPreview(szAppSelectPath, IMG_ASPECT_4_3);
			if (TabPage == 6) SetPreview(szAppVersusPath, IMG_ASPECT_4_3);
			if (TabPage == 7) SetPreview(szAppHowtoPath, IMG_ASPECT_4_3);
			if (TabPage == 8) SetPreview(szAppScoresPath, IMG_ASPECT_4_3);
			if (TabPage == 9) SetPreview(szAppBossesPath, IMG_ASPECT_4_3);
			if (TabPage == 10) SetPreview(szAppGameoverPath, IMG_ASPECT_4_3);
			if (TabPage == 11) SetPreview(szAppFlyersPath, IMG_ASPECT_PRESERVE);
			if (TabPage == 12) SetPreview(szAppCabinetsPath, IMG_ASPECT_PRESERVE);
			if (TabPage == 13) SetPreview(szAppMarqueesPath, IMG_ASPECT_PRESERVE);
			if (TabPage == 14) SetPreview(szAppControlsPath, IMG_ASPECT_PRESERVE);
			if (TabPage == 15) SetPreview(szAppPCBsPath, IMG_ASPECT_PRESERVE);

			return FALSE;
		}
	}
	
	if (Msg == WM_CTLCOLORSTATIC) {
		if ((HWND)lParam == GetDlgItem(hGameInfoDlg, IDC_LABELCOMMENT) || (HWND)lParam == GetDlgItem(hGameInfoDlg, IDC_LABELROMNAME) || (HWND)lParam == GetDlgItem(hGameInfoDlg, IDC_LABELROMINFO) || (HWND)lParam == GetDlgItem(hGameInfoDlg, IDC_LABELSYSTEM) || (HWND)lParam == GetDlgItem(hGameInfoDlg, IDC_LABELNOTES) || (HWND)lParam == GetDlgItem(hGameInfoDlg, IDC_LABELGENRE) || (HWND)lParam == GetDlgItem(hGameInfoDlg, IDC_TEXTCOMMENT) || (HWND)lParam == GetDlgItem(hGameInfoDlg, IDC_TEXTROMNAME) || (HWND)lParam == GetDlgItem(hGameInfoDlg, IDC_TEXTROMINFO) || (HWND)lParam == GetDlgItem(hGameInfoDlg, IDC_TEXTSYSTEM) || (HWND)lParam == GetDlgItem(hGameInfoDlg, IDC_TEXTNOTES) || (HWND)lParam == GetDlgItem(hGameInfoDlg, IDC_TEXTGENRE)) {
			return (INT_PTR)hWhiteBGBrush;
		}
	}

	return 0;
}

int GameInfoDialogCreate(HWND hParentWND, int nDrvSel)
{
	bGameInfoOpen = true;
	nGiDriverSelected = nDrvSel;

#if defined (_UNICODE)
	hRiched = LoadLibrary(L"RICHED20.DLL");
#else
	hRiched = LoadLibrary("RICHED20.DLL");
#endif

	if (hRiched) {
		hParent = hParentWND;
		FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_GAMEINFO), hParent, (DLGPROC)DialogProc);
	}

	bGameInfoOpen = false;
	return 0;
}
