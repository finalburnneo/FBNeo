// FB Neo Game Info and Favorites handling
#include "burner.h"
#include "richedit.h"

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
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_LOCAL), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST1), SW_SHOW);
	UpdateWindow(hGameInfoDlg);

	return 0;
}

static int DisplayHDDInfo()
{
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_H), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_LOCAL), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST3), SW_SHOW);
	UpdateWindow(hGameInfoDlg);

	return 0;
}

static int DisplaySampleInfo()
{
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_H), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_LOCAL), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST2), SW_SHOW);
	UpdateWindow(hGameInfoDlg);

	return 0;
}

static int DisplayCommands()
{
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_H), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST1), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST2), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST3), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_LOCAL), SW_SHOW);
	UpdateWindow(hGameInfoDlg);

	// set focus, so mousewheel can scroll the text
	// set to beginning of text (richedit likes to go to the bottom on SetFocus)
	SetFocus(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_LOCAL));
	SendMessage(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_LOCAL), EM_SETSEL, 0, 0);

	return 0;
}

static int DisplayHistory()
{
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_H), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST1), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST2), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST3), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), SW_SHOW);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_LOCAL), SW_HIDE);
	UpdateWindow(hGameInfoDlg);

	// set focus, so mousewheel can scroll the text
	// set to beginning of text (richedit likes to go to the bottom on SetFocus)
	SetFocus(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG));
	SendMessage(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), EM_SETSEL, 0, 0);

	return 0;
}

#define ASCIIONLY				(1 << 31)

wchar_t *repl_wcs(const wchar_t *orig, const wchar_t *search, const wchar_t *replace)
{
    if (!orig || !search || !replace)
        return NULL;

    size_t orig_len = wcslen(orig);
    size_t search_len = wcslen(search);
    size_t replace_len = wcslen(replace);

    if (search_len == 0)
        return wcsdup(orig); // nothing to replace

    // Count occurrences of 'search' in 'orig'
    size_t count = 0;
    const wchar_t *pos = orig;
    while ((pos = wcsstr(pos, search)) != NULL) {
        count++;
        pos += search_len;
    }

    // Compute final length and allocate memory
    size_t new_len = orig_len + count * (replace_len - search_len);
    wchar_t *result = (wchar_t*)malloc((new_len + 1) * sizeof(wchar_t));
    if (!result)
        return NULL;

    // Build the new string
    const wchar_t *src = orig;
    wchar_t *dst = result;
    while ((pos = wcsstr(src, search)) != NULL) {
        size_t chunk_len = pos - src;
        wmemcpy(dst, src, chunk_len);
        dst += chunk_len;
        wmemcpy(dst, replace, replace_len);
        dst += replace_len;
        src = pos + search_len;
    }

    // Copy any remaining part
    wcscpy(dst, src);
    return result;
}

// command.dat macro expansion blyat
struct fooreplace {
	TCHAR *a;
	TCHAR *b;
};

static fooreplace foor[] = {
	{L"_A", L" \u24B6 " }, // Circle'd A-D
	{L"_B", L" \u24B7 " },
	{L"_C", L" \u24B8 " },
	{L"_D", L" \u24B9 " },
	{L"_H", L" \u24BD " },
	{L"_Z", L" \u24CF " },

	{L"_a", L" \u2460 " }, // Circle'd 1-10
	{L"_b", L" \u2461 " },
	{L"_c", L" \u2462 " },
	{L"_d", L" \u2463 " },
	{L"_e", L" \u2464 " },
	{L"_f", L" \u2465 " },
	{L"_g", L" \u2466 " },
	{L"_h", L" \u2467 " },
	{L"_i", L" \u2468 " },
	{L"_j", L" \u2469 " },

	{L"_k", L" [Half Circle Back] " },
	{L"_l", L" [Half Circle Front Up] " },
	{L"_m", L" [Half Circle Front] " },
	{L"_n", L" [Half Circle Back Up] " },
	{L"_o", L" [1/4 Circle Forward 2 Down] " },
	{L"_p", L" [1/4 Circle Down 2 Back] " },
	{L"_q", L" [1/4 Circle Back 2 Up] " },
	{L"_r", L" [1/4 Circle Up 2 Forward] " },
	{L"_s", L" [1/4 Circle Back 2 Down] " },
	{L"_t", L" [1/4 Circlecle Down 2 Forward] " },
	{L"_u", L" [1/4 Circle Forward 2 Up] " },
	{L"_v", L" [1/4 Circle Up 2 Back] " },
	{L"_w", L" [Full Clock Forward] " },
	{L"_x", L" [Full Clock Back] " },
	{L"_y", L" [Full Count Forward] " },
	{L"_z", L" [Full Count Back] " },
	{L"_L", L" [2x Forward] " },
	{L"_M", L" [2x Back] " },
	{L"_Q", L" [Dragon Screw Forward] " },
	{L"_R", L" [Dragon Screw Back] " },

	{L"_S", L"[Start]" },
	{L"_P", L"[Punch]" },
	{L"_K", L"[Kick]" },
	{L"_G", L"[Guard]" },
	{L"^S", L"[Select]"},
	{L"^s", L" \u24C8 "}, // Circle'd S

	{L"@A-button", L" \u24B6 " }, // Circle'd A-Z
	{L"@B-button", L" \u24B7 " },
	{L"@C-button", L" \u24B8 " },
	{L"@D-button", L" \u24B9 " },
	{L"@E-button", L" \u24BA " },
	{L"@F-button", L" \u24BB " },
	{L"@G-button", L" \u24BC " },
	{L"@H-button", L" \u24BD " },
	{L"@I-button", L" \u24BE " },
	{L"@J-button", L" \u24BF " },
	{L"@K-button", L" \u24C0 " },
	{L"@L-button", L" \u24C1 " },
	{L"@M-button", L" \u24C2 " },
	{L"@N-button", L" \u24C3 " },
	{L"@O-button", L" \u24C4 " },
	{L"@P-button", L" \u24C5 " },
	{L"@Q-button", L" \u24C6 " },
	{L"@R-button", L" \u24C7 " },
	{L"@S-button", L" \u24C8 " },
	{L"@T-button", L" \u24C9 " },
	{L"@U-button", L" \u24CA " },
	{L"@V-button", L" \u24CB " },
	{L"@W-button", L" \u24CC " },
	{L"@X-button", L" \u24CD " },
	{L"@Y-button", L" \u24CE " },
	{L"@Z-button", L" \u24CF " },

	{L"_P", L"[Punch]" },
	{L"_K", L"[Kick]" },
	{L"_G", L"[Guard]" },

	{L"_N", L"N" },         // Just 'N'(?)

	{L"_`", L"." },         // dot .
	{L"_)", L" \u25CB " },    // circle
	{L"_@", L" \u25CE " },    // bullseye
	{L"_(", L" \u25CF " },	// circle black
	{L"_&", L" \u2605 " },    // five-pointed Star
	{L"_*", L" \u2606 " },    // five-pointed Star
	{L"_`", L" \u00B7 " },    // middle dot
	{L"_#", L" \u25A3 " },    // double-square
	{L"_]", L" \u25A1 " },    // square
	{L"_[", L" \u25A0 " },    // square black
	{L"_%", L" \u25B3 " },    // triangle
	{L"_$", L" \u25B2 " },    // triangle black
	{L"_<", L" \u25C6 " },    // diamond
	{L"_>", L" \u25C7 " },    // diamond black

	{L"_+", L" + " },
	{L"_^", L"[air]" },
	{L"_?", L"[dir]" },
	{L"_X", L"[tap]" },
	{L"_|", L"[Jump]" },
	{L"_O", L"[Hold]" },
	{L"_-", L"[Air]" },
	{L"_=", L"[Squatting]" },
	{L"_~", L"[Charge]" },
	{L"_!", L" \u21C9 " }, // continue arrow

	// arrows
	{L"_9", L" \u2197 " },
	{L"_8", L" \u2191 " },
	{L"_7", L" \u2196 " },
	{L"_6", L" \u2192 " },
	{L"_5", L" \u25CF " }, // black dot
	{L"_4", L" \u2190 " },
	{L"_3", L" \u2198 " },
	{L"_2", L" \u2193 " },
	{L"_1", L" \u2199 " },
	{L"_.", L" \u2026 " }, // "..."
	// double arrows
	{L"^9", L" \u21D7 . " },
	{L"^8", L" \u21D1 . " },
	{L"^7", L" \u21D6 . " },
	{L"^6", L" \u21D2 . " },
	{L"^4", L" \u21D0 . " },
	{L"^3", L" \u21D8 . " },
	{L"^2", L" \u21D3 . " },
	{L"^1", L" \u21D9 . " },

	{L"^-", L" [Close] " },
	{L"^=", L" [Away] " },
	{L"^*", L" [Spam Button] " },
	{L"^?", L" [Any Button] " },

	{L"^E", L"[LP]" },
	{L"^F", L"[MP]" },
	{L"^G", L"[SP]" },
	{L"^H", L"[LK]" },
	{L"^I", L"[MK]" },
	{L"^J", L"[SK]" },
	{L"^T", L"[3x Kick]" },
	{L"^U", L"[3x Punch]" },
	{L"^V", L"[2x Kick]" },
	{L"^W", L"[2x Punch]" },
	{L"^!", L" \u21B3 " }, // down-right arrow
	{L"^M", L" MAX " },
#if 0
	{L"_", L"\u" },
	{L"_", L"\u" },
	{L"_", L"\u" },
#endif

	{NULL, NULL}

};

static void check_expands(TCHAR *str)
{
	for (int i = 0; foor[i].a != NULL; i++) {
		TCHAR *sz = repl_wcs(str, foor[i].a, foor[i].b);
		wcscpy(str, sz);
		free(sz);
	}
}

static int GameInfoInit()
{
	// Get the games full name
	TCHAR szText[1024] = _T("");
	TCHAR* pszPosition = szText;

	int nGetTextFlags = nLoadMenuShowY & ASCIIONLY ? DRV_ASCIIONLY : 0;

	TCHAR* pszName = BurnDrvGetText(nGetTextFlags | DRV_FULLNAME);

	pszPosition += _sntprintf(szText, 1024, pszName);

	pszName = BurnDrvGetText(nGetTextFlags | DRV_FULLNAME);
	while ((pszName = BurnDrvGetText(nGetTextFlags | DRV_NEXTNAME | DRV_FULLNAME)) != NULL) {
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

	UINT idsString[18] = {  IDS_GAMEINFO_ROMINFO, IDS_GAMEINFO_HDD, IDS_GAMEINFO_SAMPLES, IDS_GAMEINFO_HISTORY, IDS_GAMEINFO_COMMANDS, IDS_GAMEINFO_INGAME, IDS_GAMEINFO_TITLE, IDS_GAMEINFO_SELECT, IDS_GAMEINFO_VERSUS, IDS_GAMEINFO_HOWTO, IDS_GAMEINFO_SCORES, IDS_GAMEINFO_BOSSES, IDS_GAMEINFO_GAMEOVER, IDS_GAMEINFO_FLYER, IDS_GAMEINFO_CABINET, IDS_GAMEINFO_MARQUEE, IDS_GAMEINFO_CONTROLS, IDS_GAMEINFO_PCB };

	for(int i = 0; i < 18; i++) {
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
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST3), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), SW_HIDE);
	ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_LOCAL), SW_HIDE);
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
#ifdef FBNEO_DEBUG
	_stprintf(szItemText, _T("%s\t(%S)"), szItemText, BurnDrvGetSourcefile());
#endif
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
		if (ri.nType & BRF_BIOS && i >= 0x80) continue;

		LvItem.iItem = RomPos;
		LvItem.iSubItem = 0;
		LvItem.pszText = ANSIToTCHAR(szRomName, NULL, 0);
		SendMessage(hList, LVM_INSERTITEM, 0, (LPARAM)&LvItem);

		snprintf(nLen, sizeof(nLen), "%d", ri.nLen);
		LvItem.iSubItem = 1;
		LvItem.pszText = ANSIToTCHAR(nLen, NULL, 0);
		SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);

		snprintf(nCrc, sizeof(nCrc), "%08X", ri.nCrc);
		if (!(ri.nType & BRF_NODUMP)) {
			LvItem.iSubItem = 2;
			LvItem.pszText = ANSIToTCHAR(nCrc, NULL, 0);
			SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);
		}

		if (ri.nType & BRF_ESS) snprintf(Type, sizeof(Type), "%s, Essential", Type);
		if (ri.nType & BRF_OPT) snprintf(Type, sizeof(Type), "%s, Optional", Type);
		if (ri.nType & BRF_PRG)	snprintf(Type, sizeof(Type), "%s, Program", Type);
		if (ri.nType & BRF_GRA) snprintf(Type, sizeof(Type), "%s, Graphics", Type);
		if (ri.nType & BRF_SND) snprintf(Type, sizeof(Type), "%s, Sound", Type);
		if (ri.nType & BRF_BIOS) snprintf(Type, sizeof(Type), "%s, BIOS", Type);

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
		// spec_spectrum (14 characters)
		// spec_spec128  (13 characters)
		char szBoardName[14] = "";
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

			snprintf(nLenBoard, sizeof(nLenBoard), "%d", riBoard.nLen);
			LvItem.iSubItem = 1;
			LvItem.pszText = ANSIToTCHAR(nLenBoard, NULL, 0);
			SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);

			snprintf(nCrcBoard, sizeof(nCrcBoard), "%08X", riBoard.nCrc);
			if (!(riBoard.nType & BRF_NODUMP)) {
				LvItem.iSubItem = 2;
				LvItem.pszText = ANSIToTCHAR(nCrcBoard, NULL, 0);
				SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);
			}

			if (riBoard.nType & BRF_ESS) snprintf(BoardType, sizeof(BoardType), "%s, Essential", BoardType);
			if (riBoard.nType & BRF_OPT) snprintf(BoardType, sizeof(BoardType), "%s, Optional", BoardType);
			if (riBoard.nType & BRF_PRG) snprintf(BoardType, sizeof(BoardType), "%s, Program", BoardType);
			if (riBoard.nType & BRF_GRA) snprintf(BoardType, sizeof(BoardType), "%s, Graphics", BoardType);
			if (riBoard.nType & BRF_SND) snprintf(BoardType, sizeof(BoardType), "%s, Sound", BoardType);
			if (riBoard.nType & BRF_BIOS) snprintf(BoardType, sizeof(BoardType), "%s, BIOS", BoardType);

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

	// Set up the hdd info list
	hList = GetDlgItem(hGameInfoDlg, IDC_LIST3);

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
	LvCol.cx = 100;

	memset(&LvItem, 0, sizeof(LvItem));
	LvItem.mask=  LVIF_TEXT;
	LvItem.cchTextMax = 256;
	int HDDPos = 0;
	for (int i = 0; i < 0x100; i++) { // assume max 0x100 hdds per game
		int nRet;
		struct BurnHDDInfo hddi;
		char nLen[10] = "";
		char nCrc[10] = "";
		char *szHDDName = NULL;

		memset(&hddi, 0, sizeof(hddi));

		nRet = BurnDrvGetHDDInfo(&hddi, i);
		nRet += BurnDrvGetHDDName(&szHDDName, i, 0);

		if (hddi.nLen == 0) continue;

		LvItem.iItem = HDDPos;
		LvItem.iSubItem = 0;
		LvItem.pszText = ANSIToTCHAR(szHDDName, NULL, 0);
		SendMessage(hList, LVM_INSERTITEM, 0, (LPARAM)&LvItem);

		snprintf(nLen, sizeof(nLen), "%d", hddi.nLen);
		LvItem.iSubItem = 1;
		LvItem.pszText = ANSIToTCHAR(nLen, NULL, 0);
		SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);

		snprintf(nCrc, sizeof(nCrc), "%08X", hddi.nCrc);
		LvItem.iSubItem = 2;
		LvItem.pszText = ANSIToTCHAR(nCrc, NULL, 0);
		SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&LvItem);

		HDDPos++;
	}

	// Get the history info
	CHAR szFileName[MAX_PATH] = "";
	snprintf(szFileName, sizeof(szFileName), "%shistory.xml", TCHARToANSI(szAppHistoryPath, NULL, 0));

	FILE *fp = fopen(szFileName, "rt");
	char Temp[10000];
	char rom_token[255];
	char DRIVER_NAME[255];
	char to_find[255*2];
	int inGame = 0;

	TCHAR *szBuffer;
	szBuffer = (TCHAR*)malloc(0x80000);

	wcscpy(szBuffer, _T("{\\rtf1\\ansi{\\fonttbl(\\f0\\fnil\\fcharset0 Verdana;)}{\\colortbl;\\red220\\green0\\blue0;\\red0\\green0\\blue0;}"));

	GetHistoryDatHardwareToken(&rom_token[0]);
	strcpy(DRIVER_NAME, BurnDrvGetTextA(DRV_NAME));

	if (strncmp("\t\t\t<system name=\"", rom_token, 17)) { // non-arcade game detected. (token not "$info=" !)
		bprintf(0, _T("not arcade\n"));
		char *p = strchr(DRIVER_NAME, '_');
		if (p) strcpy(DRIVER_NAME, p + 1); // change "nes_smb" -> "smb"
	}

	sprintf(to_find, "%s%s\"", rom_token, DRIVER_NAME);

	if (fp) {
		while (!feof(fp)) {
			fgets(Temp, 10000, fp);
			if (!strncmp(to_find, Temp, strlen(to_find))) {
				inGame = 1;
				continue;
			}

			if (inGame == 1) {
				if (strstr(Temp, "<text>")) {
					inGame = 2;
					continue;
				}
			}

			if (inGame == 2) {
				int nTitleWrote = 0;
				while (!feof(fp)) {
					// at this point, the "game was released XX years ago.." string is thrown out
					fgets(Temp, 10000, fp);

					if (strstr(Temp, "</text>")) {
						break;
					}

					TCHAR *cnvTemp = wstring_from_utf8(Temp);

					tcharstrreplace(cnvTemp, _T("&quot;"), _T("\""));
					tcharstrreplace(cnvTemp, _T("&amp;"), _T("&"));
					tcharstrreplace(cnvTemp, _T("&apos;"), _T("'"));
					tcharstrreplace(cnvTemp, _T("&gt;"), _T(">"));
					tcharstrreplace(cnvTemp, _T("&lt;"), _T("<"));

					if (!nTitleWrote) {
						_stprintf(szBuffer, _T("%s{\\b\\f0\\fs28\\cf1\\f0 %s}"), szBuffer, cnvTemp);
					} else {
						_stprintf(szBuffer, _T("%s\\line"), szBuffer);
						if (!strncmp("- ", Temp, 2)) {
							_stprintf(szBuffer, _T("%s{\\b\\f0\\fs16\\cf1\\f0 %s}"), szBuffer, cnvTemp);
						} else {
							_stprintf(szBuffer, _T("%s{\\f0\\fs16\\cf2\\f0 %s}"), szBuffer, cnvTemp);
						}
					}
					free(cnvTemp);
					if (strcmp("\n", Temp)) nTitleWrote = 1;
				}
				break;
			}
		}
		fclose(fp);
	}

	_stprintf(szBuffer, _T("%s}"), szBuffer);

	// convert wchar (utf16) to utf8.  RICHED20W control requires utf8 for rtf1 parsing! -dink april 27, 2021
	char *pszBufferUTF8 = utf8_from_wstring(szBuffer);

	// tell riched control to expect utf8 instead of utf16
	SETTEXTEX TextInfo;
	TextInfo.flags = ST_SELECTION;
	TextInfo.codepage = CP_UTF8;

	SendMessage(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), EM_SETTEXTEX, (WPARAM)&TextInfo, (LPARAM)pszBufferUTF8);
	free(pszBufferUTF8);

	// ------------ command.dat processing ------------
	// Get the command.dat info
	snprintf(szFileName, sizeof(szFileName), "%scommand.dat", TCHARToANSI(szAppCommandPath, NULL, 0));

	fp = fopen(szFileName, "rt");
	inGame = 0;

	strcpy(rom_token, "$info=");

//	GetHistoryDatHardwareToken(&rom_token[0]);
	strcpy(DRIVER_NAME, BurnDrvGetTextA(DRV_NAME));

	if (strncmp("$info=", rom_token, 6)) { // non-arcade game detected. (token not "$info=" !)
		char *p = strchr(DRIVER_NAME, '_');
		if (p) strcpy(DRIVER_NAME, p + 1); // change "nes_smb" -> "smb"
	}

	wcscpy(szBuffer, _T("{\\rtf1\\ansi\\ansicpg0{\\fonttbl(\\f0\\fnil\\fcharset0 Microsoft Sans Serif;)}{\\colortbl;\\red220\\green0\\blue0;\\red0\\green0\\blue0;}"));

	if (fp) {
		while (!feof(fp)) {
			char *Tokens;

			fgets(Temp, 10000, fp);
			if (!strncmp("$info=", Temp, 6)) {
				Tokens = strtok(Temp, "=,\n");
				while (Tokens != NULL) {
					if (!strcmp(Tokens, BurnDrvGetTextA(DRV_NAME))) {
						inGame = 1;
						break;
					}

					Tokens = strtok(NULL, "=,\n");
				}
			}

			if (inGame) {
				int nTitleWrote = 0;
				while (strncmp("$end", Temp, 4)) {
					fgets(Temp, 10000, fp);

					if (!strncmp("$", Temp, 1)) continue;

					TCHAR *cnvTemp = wstring_from_utf8(Temp);

					if (!nTitleWrote) {
						_stprintf(szBuffer, _T("%s{\\b\\f0\\fs28\\cf1\\f0 %s}"), szBuffer, cnvTemp);
					} else {
						_stprintf(szBuffer, _T("%s\\line"), szBuffer);
						if (!strncmp("- ", Temp, 2)) {
							_stprintf(szBuffer, _T("%s{\\b\\f0\\fs16\\cf1\\f0 %s}"), szBuffer, cnvTemp);
						} else {
							_stprintf(szBuffer, _T("%s{\\f0\\fs16\\cf2\\f0 %s}"), szBuffer, cnvTemp);
						}
					}
					free(cnvTemp);
					if (strcmp("\n", Temp)) nTitleWrote = 1;
				}
				break;
			}
		}
		fclose(fp);
	}

	_stprintf(szBuffer, _T("%s\\line}"), szBuffer);
	check_expands(szBuffer);
	pszBufferUTF8 = utf8_from_wstring(szBuffer);
	TextInfo.flags = ST_SELECTION;
	TextInfo.codepage = CP_UTF8;

	//BurnDump("utf8_.bin", pszBufferUTF8, strlen(pszBufferUTF8));

	// EM_EXLIMITTEXT doesn't always work above 90kbytes (or so),
	// Also send EM_LIMITTEXT just-in-case! (kof2002 commands text)
	SendMessage(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_LOCAL), EM_EXLIMITTEXT, (WPARAM)0x80000, (LPARAM)0); // 512kbyte limit
	SendMessage(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_LOCAL), EM_LIMITTEXT, (WPARAM)0x80000, (LPARAM)0); // 512kbyte limit
	SendMessage(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_LOCAL), EM_SETTEXTEX, (WPARAM)&TextInfo, (LPARAM)pszBufferUTF8);
	free(pszBufferUTF8);
	free(szBuffer);

	// Make a white brush
	hWhiteBGBrush = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));

	return 0;
}

#define MAXFAVORITES 2000
char szFavorites[MAXFAVORITES][68];
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

			if (strlen(szTemp) < 65 && strlen(szTemp) > 1) {
				strncpy(szFavorites[nFavorites++], szTemp, 65);
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
	char szBoardName[68] = "";

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
	memset(szFullName, 0, 1024 * sizeof(TCHAR));

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

		if (pNmHdr->code == HDN_ENDTRACK) {
			InvalidateRect(GetDlgItem(hGameInfoDlg, IDC_LIST1), NULL, TRUE);
			return TRUE;
		}

		if (pNmHdr->code == TCN_SELCHANGE) {
			int TabPage = TabCtrl_GetCurSel(hTabControl);

			ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST1), SW_HIDE);
			ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST2), SW_HIDE);
			ShowWindow(GetDlgItem(hGameInfoDlg, IDC_LIST3), SW_HIDE);
			ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_ENG), SW_HIDE);
			ShowWindow(GetDlgItem(hGameInfoDlg, IDC_MESSAGE_EDIT_LOCAL), SW_HIDE);
			ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_H), SW_SHOW);
			ShowWindow(GetDlgItem(hGameInfoDlg, IDC_SCREENSHOT_V), SW_SHOW);
			UpdateWindow(hGameInfoDlg);

			nBurnDrvActive = nGiDriverSelected;

			if (TabPage == 0) DisplayRomInfo();
			if (TabPage == 1) DisplayHDDInfo();
			if (TabPage == 2) DisplaySampleInfo();
			if (TabPage == 3) DisplayHistory();
			if (TabPage == 4) DisplayCommands();
			if (TabPage == 5) SetPreview(szAppPreviewsPath, IMG_ASPECT_4_3);
			if (TabPage == 6) SetPreview(szAppTitlesPath, IMG_ASPECT_4_3);
			if (TabPage == 7) SetPreview(szAppSelectPath, IMG_ASPECT_4_3);
			if (TabPage == 8) SetPreview(szAppVersusPath, IMG_ASPECT_4_3);
			if (TabPage == 9) SetPreview(szAppHowtoPath, IMG_ASPECT_4_3);
			if (TabPage == 10) SetPreview(szAppScoresPath, IMG_ASPECT_4_3);
			if (TabPage == 11) SetPreview(szAppBossesPath, IMG_ASPECT_4_3);
			if (TabPage == 12) SetPreview(szAppGameoverPath, IMG_ASPECT_4_3);
			if (TabPage == 13) SetPreview(szAppFlyersPath, IMG_ASPECT_PRESERVE);
			if (TabPage == 14) SetPreview(szAppCabinetsPath, IMG_ASPECT_PRESERVE);
			if (TabPage == 15) SetPreview(szAppMarqueesPath, IMG_ASPECT_PRESERVE);
			if (TabPage == 16) SetPreview(szAppControlsPath, IMG_ASPECT_PRESERVE);
			if (TabPage == 17) SetPreview(szAppPCBsPath, IMG_ASPECT_PRESERVE);

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

	hRiched = LoadLibrary(_T("RICHED20.DLL"));

	if (hRiched) {
		hParent = hParentWND;
		FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_GAMEINFO), hParent, (DLGPROC)DialogProc);
	}

	bGameInfoOpen = false;
	return 0;
}
