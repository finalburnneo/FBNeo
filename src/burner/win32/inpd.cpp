// Burner Input Editor Dialog module
#include "burner.h"

HWND hInpdDlg = NULL;							// Handle to the Input Dialog
static HWND hInpdList = NULL;
static unsigned short *LastVal = NULL;			// Last input values/defined
static int bLastValDefined = 0;					//

static HWND hInpdGi = NULL, hInpdPci = NULL, hInpdAnalog = NULL;	// Combo boxes

int bClearInputIgnoreCheckboxMessage = 0;		// For clear input on afire macro.

static int bInittingCheckboxes = 0;

static HWND hToolTipWnd = NULL;

#if 0
// experi-mental stuff
#define TRBN_FIRST (0U-1501U)
#define TRBN_THUMBPOSCHANGING (TRBN_FIRST-1)

  typedef struct tagTRBTHUMBPOSCHANGING {
    NMHDR hdr;
    DWORD dwPos;
    int nReason;
  } NMTRBTHUMBPOSCHANGING,*PNMTRBTHUMBPOSCHANGING;
#endif

// Update which input is using which PC input
static int InpdUseUpdate()
{
	unsigned int i, j = 0;
	struct GameInp* pgi = NULL;
	if (hInpdList == NULL) {
		return 1;
	}

	// Update the values of all the inputs
	for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
		LVITEM LvItem;
		TCHAR* pszVal = NULL;

		if (pgi->Input.pVal == NULL) {
			continue;
		}

		pszVal = InpToDesc(pgi);

		if (_tcscmp(pszVal, _T("code 0x00")) == 0)
			pszVal = _T("Unassigned (locked)");

		memset(&LvItem, 0, sizeof(LvItem));
		LvItem.mask = LVIF_TEXT;
		LvItem.iItem = j;
		LvItem.iSubItem = 1;
		LvItem.pszText = pszVal;

		SendMessage(hInpdList, LVM_SETITEM, 0, (LPARAM)&LvItem);

		j++;
	}

	for (i = 0, pgi = GameInp + nGameInpCount; i < nMacroCount; i++, pgi++) {
		LVITEM LvItem;
		TCHAR* pszVal = NULL;

		if (pgi->nInput & GIT_GROUP_MACRO) {
			pszVal = InpMacroToDesc(pgi);

			if (_tcscmp(pszVal, _T("code 0x00")) == 0)
				pszVal = _T("Unassigned (locked)");

			memset(&LvItem, 0, sizeof(LvItem));
			LvItem.mask = LVIF_TEXT;
			LvItem.iItem = j;
			LvItem.iSubItem = 1;
			LvItem.pszText = pszVal;

			SendMessage(hInpdList, LVM_SETITEM, 0, (LPARAM)&LvItem);
		}

		j++;
	}

	return 0;
}

int InpdUpdate()
{
	unsigned int i, j = 0;
	struct GameInp* pgi = NULL;
	unsigned short* plv = NULL;
	unsigned short nThisVal;
	if (hInpdList == NULL) {
		return 1;
	}
	if (LastVal == NULL) {
		return 1;
	}

	// Update the values of all the inputs
	for (i = 0, pgi = GameInp, plv = LastVal; i < nGameInpCount; i++, pgi++, plv++) {
		LVITEM LvItem;
		TCHAR szVal[16];

		if (pgi->nType == 0) {
			continue;
		}

		if (pgi->nType & BIT_GROUP_ANALOG) {
			if (bRunPause) {														// Update LastVal
				nThisVal = pgi->Input.nVal;
			} else {
				nThisVal = *pgi->Input.pShortVal;
			}

			if (bLastValDefined && (pgi->nType != BIT_ANALOG_REL || nThisVal) && pgi->Input.nVal == *plv) {
				j++;
				continue;
			}

			*plv = nThisVal;
		} else {
			if (bRunPause) {														// Update LastVal
				nThisVal = pgi->Input.nVal;
			} else {
				nThisVal = *pgi->Input.pVal;
			}

			if (bLastValDefined && pgi->Input.nVal == *plv) {						// hasn't changed
				j++;
				continue;
			}

			*plv = nThisVal;
		}

		switch (pgi->nType) {
			case BIT_DIGITAL: {
				if (nThisVal == 0) {
					szVal[0] = 0;
				} else {
					if (nThisVal == 1) {
						_tcscpy(szVal, _T("ON"));
					} else {
						_stprintf(szVal, _T("0x%02X"), nThisVal);
					}
				}
				break;
			}
			case BIT_ANALOG_ABS: {
				_stprintf(szVal, _T("0x%02X"), nThisVal >> 8);
				break;
			}
			case BIT_ANALOG_REL: {
				if (nThisVal == 0) {
					szVal[0] = 0;
				}
				if ((short)nThisVal < 0) {
					_stprintf(szVal, _T("%d"), ((short)nThisVal) >> 8);
				}
				if ((short)nThisVal > 0) {
					_stprintf(szVal, _T("+%d"), ((short)nThisVal) >> 8);
				}
				break;
			}
			default: {
				_stprintf(szVal, _T("0x%02X"), nThisVal);
			}
		}

		memset(&LvItem, 0, sizeof(LvItem));
		LvItem.mask = LVIF_TEXT;
		LvItem.iItem = j;
		LvItem.iSubItem = 2;
		LvItem.pszText = szVal;

		SendMessage(hInpdList, LVM_SETITEM, 0, (LPARAM)&LvItem);

		j++;
	}

	bLastValDefined = 1;										// LastVal is now defined

	return 0;
}

static int InpdListBegin()
{
	LVCOLUMN LvCol;
	if (hInpdList == NULL) {
		return 1;
	}

	// Full row select style: Add checkbox for marco Autofire.
	SendMessage(hInpdList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);

	// Make column headers
	memset(&LvCol, 0, sizeof(LvCol));
	LvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

	LvCol.cx = 0xa0;
	LvCol.pszText = FBALoadStringEx(hAppInst, IDS_INPUT_INPUT, true);
	SendMessage(hInpdList, LVM_INSERTCOLUMN, 0, (LPARAM)&LvCol);

	LvCol.cx = 0xa0;
	LvCol.pszText = FBALoadStringEx(hAppInst, IDS_INPUT_MAPPING, true);
	SendMessage(hInpdList, LVM_INSERTCOLUMN, 1, (LPARAM)&LvCol);

	LvCol.cx = 0x38;
	LvCol.pszText = FBALoadStringEx(hAppInst, IDS_INPUT_STATE, true);
	SendMessage(hInpdList, LVM_INSERTCOLUMN, 2, (LPARAM)&LvCol);

	return 0;
}

// Make a list view of the game inputs
int InpdListMake(int bBuild)
{
	unsigned int j = 0;

	if (hInpdList == NULL) {
		return 1;
	}

	bLastValDefined = 0;
	if (bBuild)	{
		SendMessage(hInpdList, LVM_DELETEALLITEMS, 0, 0);
	}
	
	// Add all the input names to the list
	for (unsigned int i = 0; i < nGameInpCount; i++) {
		struct BurnInputInfo bii;
		LVITEM LvItem;

		// Get the name of the input
		bii.szName = NULL;
		BurnDrvGetInputInfo(&bii, i);

		// skip unused inputs
		if (bii.pVal == NULL) {
			continue;
		}
		if (bii.szName == NULL)	{
			bii.szName = "";
		}

		memset(&LvItem, 0, sizeof(LvItem));
		LvItem.mask = LVIF_TEXT |  LVIF_PARAM;
		LvItem.iItem = j;
		LvItem.iSubItem = 0;
		LvItem.pszText = ANSIToTCHAR(bii.szName, NULL, 0);
		LvItem.lParam = (LPARAM)i;

		SendMessage(hInpdList, bBuild ? LVM_INSERTITEM : LVM_SETITEM, 0, (LPARAM)&LvItem);

		j++;
	}

	// Init Autofire checkboxes.
	bInittingCheckboxes = 1;

	struct GameInp* pgi = GameInp + nGameInpCount;
	for (unsigned int i = 0; i < nMacroCount; i++, pgi++) {
		LVITEM LvItem;

		if (pgi->nInput & GIT_GROUP_MACRO) {
			memset(&LvItem, 0, sizeof(LvItem));
			LvItem.mask = LVIF_TEXT | LVIF_PARAM;
			LvItem.iItem = j;
			LvItem.iSubItem = 0;
			LvItem.pszText = ANSIToTCHAR(pgi->Macro.szName, NULL, 0);
			LvItem.lParam = (LPARAM)j;

			SendMessage(hInpdList, bBuild ? LVM_INSERTITEM : LVM_SETITEM, 0, (LPARAM)&LvItem);

			// When Macro is auto-fire, the checkbox is checked.
			if (pgi->Macro.nSysMacro != 1) { // only non-system Macros!
				ListView_SetCheckState(hInpdList, j, (pgi->Macro.nSysMacro == 15 && pgi->Input.pVal) ? TRUE : FALSE);
			}
		}

		j++;
	}

	bInittingCheckboxes = 0;

	InpdUseUpdate();

	return 0;
}

static void DisablePresets()
{
	EnableWindow(hInpdPci, FALSE);
	EnableWindow(hInpdAnalog, FALSE);
	EnableWindow(GetDlgItem(hInpdDlg, IDC_INPD_DEFAULT), FALSE);
	EnableWindow(GetDlgItem(hInpdDlg, IDC_INPD_USE), FALSE);
}

static void InitComboboxes()
{
	TCHAR szLabel[1024];
	HANDLE search;
	WIN32_FIND_DATA findData;

	for (int i = 0; i < 4; i++) {
		_stprintf(szLabel, FBALoadStringEx(hAppInst, IDS_INPUT_INP_PLAYER, true), i + 1);
		SendMessage(hInpdGi, CB_ADDSTRING, 0, (LPARAM)szLabel);
	}

	SendMessage(hInpdPci, CB_ADDSTRING, 0, (LPARAM)FBALoadStringEx(hAppInst, IDS_INPUT_INP_KEYBOARD, true));
	for (int i = 0; i < 3; i++) {
		_stprintf(szLabel, FBALoadStringEx(hAppInst, IDS_INPUT_INP_JOYSTICK, true), i);
		SendMessage(hInpdPci, CB_ADDSTRING, 0, (LPARAM)szLabel);
	}
	SendMessage(hInpdPci, CB_ADDSTRING, 0, (LPARAM)FBALoadStringEx(hAppInst, IDS_INPUT_INP_XARCADEL, true));
	SendMessage(hInpdPci, CB_ADDSTRING, 0, (LPARAM)FBALoadStringEx(hAppInst, IDS_INPUT_INP_XARCADER, true));
	SendMessage(hInpdPci, CB_ADDSTRING, 0, (LPARAM)FBALoadStringEx(hAppInst, IDS_INPUT_INP_HOTRODL, true));
	SendMessage(hInpdPci, CB_ADDSTRING, 0, (LPARAM)FBALoadStringEx(hAppInst, IDS_INPUT_INP_HOTRODR, true));

	// Scan presets directory for .ini files and add them to the list
	if ((search = FindFirstFile(_T("config/presets/*.ini"), &findData)) != INVALID_HANDLE_VALUE) {
		do {
			findData.cFileName[_tcslen(findData.cFileName) - 4] = 0;
			SendMessage(hInpdPci, CB_ADDSTRING, 0, (LPARAM)findData.cFileName);
		} while (FindNextFile(search, &findData) != 0);

		FindClose(search);
	}
}

static int InpdInit()
{
	int nMemLen;

	hInpdList = GetDlgItem(hInpdDlg, IDC_INPD_LIST);

	bClearInputIgnoreCheckboxMessage = 0;

	// Allocate a last val array for the last input values
	nMemLen = nGameInpCount * sizeof(char);
	LastVal = (unsigned short*)malloc(nMemLen * sizeof(unsigned short));
	if (LastVal == NULL) {
		return 1;
	}
	memset(LastVal, 0, nMemLen * sizeof(unsigned short));

	InpdListBegin();
	InpdListMake(1);

	// Init the Combo boxes
	hInpdGi = GetDlgItem(hInpdDlg, IDC_INPD_GI);
	hInpdPci = GetDlgItem(hInpdDlg, IDC_INPD_PCI);
	hInpdAnalog = GetDlgItem(hInpdDlg, IDC_INPD_ANALOG);
	InitComboboxes();

	DisablePresets();

	return 0;
}

static int InpdExit()
{
	// Exit the Combo boxes
	hInpdPci = NULL;
	hInpdGi = NULL;
	hInpdAnalog = NULL;

	if (LastVal != NULL) {
		free(LastVal);
		LastVal = NULL;
	}
	hInpdList = NULL;
	hInpdDlg = NULL;
	if (!bAltPause && bRunPause) {
		bRunPause=0;
	}
	GameInpCheckMouse();

	return 0;
}

static void GameInpConfigOne(int nPlayer, int nPcDev, int nAnalog, struct GameInp* pgi, char* szi)
{
	switch (nPcDev) {
		case  0:
			GamcPlayer(pgi, szi, nPlayer, -1);						// Keyboard
			GamcAnalogKey(pgi, szi, nPlayer, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
			break;
		case  1:
			GamcPlayer(pgi, szi, nPlayer, 0);						// Joystick 1
			GamcAnalogJoy(pgi, szi, nPlayer, 0, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
			break;
		case  2:
			GamcPlayer(pgi, szi, nPlayer, 1);						// Joystick 2
			GamcAnalogJoy(pgi, szi, nPlayer, 1, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
			break;
		case  3:
			GamcPlayer(pgi, szi, nPlayer, 2);						// Joystick 3
			GamcAnalogJoy(pgi, szi, nPlayer, 2, nAnalog);
			GamcMisc(pgi, szi, nPlayer);
			break;
		case  4:
			GamcPlayerHotRod(pgi, szi, nPlayer, 0x10, nAnalog);		// X-Arcade left side
			GamcMisc(pgi, szi, -1);
			break;
		case  5:
			GamcPlayerHotRod(pgi, szi, nPlayer, 0x11, nAnalog);		// X-Arcade right side
			GamcMisc(pgi, szi, -1);
			break;
		case  6:
			GamcPlayerHotRod(pgi, szi, nPlayer, 0x00, nAnalog);		// HotRod left side
			GamcMisc(pgi, szi, -1);
			break;
		case  7:
			GamcPlayerHotRod(pgi, szi, nPlayer, 0x01, nAnalog);		// HotRod right size
			GamcMisc(pgi, szi, -1);
			break;
	}
}

// Configure some of the game input
static int GameInpConfig(int nPlayer, int nPcDev, int nAnalog)
{
	struct GameInp* pgi = NULL;
	unsigned int i;

	for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
		struct BurnInputInfo bii;

		// Get the extra info about the input
		bii.szInfo = NULL;
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal == NULL) {
			continue;
		}
		if (bii.szInfo == NULL) {
			bii.szInfo = "";
		}
		GameInpConfigOne(nPlayer, nPcDev, nAnalog, pgi, bii.szInfo);
	}

	for (i = 0; i < nMacroCount; i++, pgi++) {
		GameInpConfigOne(nPlayer, nPcDev, nAnalog, pgi, pgi->Macro.szName);
	}

	GameInpCheckLeftAlt();

	return 0;
}

// List item activated; find out which one
static int ListItemActivate()
{
	struct BurnInputInfo bii;
	LVITEM LvItem;

	memset(&LvItem, 0, sizeof(LvItem));
	int nSel = SendMessage(hInpdList, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
	if (nSel < 0) {
		return 1;
	}

	// Get the corresponding input
	LvItem.mask = LVIF_PARAM;
	LvItem.iItem = nSel;
	LvItem.iSubItem = 0;
	SendMessage(hInpdList, LVM_GETITEM, 0, (LPARAM)&LvItem);
	nSel = LvItem.lParam;

	if (nSel >= (int)(nGameInpCount + nMacroCount)) {	// out of range
		return 1;
	}

	memset(&bii, 0, sizeof(bii));
	bii.nType = 0;
	int rc = BurnDrvGetInputInfo(&bii, nSel);
	if (bii.pVal == NULL && rc != 1) {                  // rc == 1 for a macro or system macro.
		return 1;
	}

	DestroyWindow(hInpsDlg);							// Make sure any existing dialogs are gone
	DestroyWindow(hInpcDlg);							//

	if (bii.nType & BIT_GROUP_CONSTANT) {
		// Dip switch is a constant - change it
		nInpcInput = nSel;
		InpcCreate();
	} else {
		if (GameInp[nSel].nInput == GIT_MACRO_CUSTOM) {
#if 0
			InpMacroCreate(nSel);
#endif
		} else {
			// Assign to a key
			nInpsInput = nSel;
			InpsCreate();
		}
	}

	GameInpCheckLeftAlt();

	return 0;
}

#if 0
static int NewMacroButton()
{
	LVITEM LvItem;
	int nSel;

	DestroyWindow(hInpsDlg);							// Make sure any existing dialogs are gone
	DestroyWindow(hInpcDlg);							//

	nSel = SendMessage(hInpdList, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
	if (nSel < 0) {
		nSel = -1;
	}

	// Get the corresponding input
	LvItem.mask = LVIF_PARAM;
	LvItem.iItem = nSel;
	LvItem.iSubItem = 0;
	SendMessage(hInpdList, LVM_GETITEM, 0, (LPARAM)&LvItem);
	nSel = LvItem.lParam;

	if (nSel >= (int)nGameInpCount && nSel < (int)(nGameInpCount + nMacroCount)) {
		if (GameInp[nSel].nInput != GIT_MACRO_CUSTOM) {
			nSel = -1;
		}
	} else {
		nSel = -1;
	}

	InpMacroCreate(nSel);

	return 0;
}
#endif

static int DeleteInput(unsigned int i)
{
	struct BurnInputInfo bii;

	if (i >= nGameInpCount) {

		if (i < nGameInpCount + nMacroCount) {	// Macro
			GameInp[i].Macro.nMode = 0;
		} else { 								// out of range
			return 1;
		}
	} else {									// "True" input
		bii.nType = BIT_DIGITAL;
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal == NULL) {
			return 1;
		}
		if (bii.nType & BIT_GROUP_CONSTANT) {	// Don't delete dip switches
			return 1;
		}

		GameInp[i].nInput = 0;
	}

	GameInpCheckLeftAlt();

	return 0;
}

// List item(s) deleted; find out which one(s)
static int ListItemDelete()
{
	int nStart = -1;
	LVITEM LvItem;
	int nRet;

	while ((nRet = SendMessage(hInpdList, LVM_GETNEXTITEM, (WPARAM)nStart, LVNI_SELECTED)) != -1) {
		nStart = nRet;

		// Get the corresponding input
		LvItem.mask = LVIF_PARAM;
		LvItem.iItem = nRet;
		LvItem.iSubItem = 0;
		SendMessage(hInpdList, LVM_GETITEM, 0, (LPARAM)&LvItem);
		nRet = LvItem.lParam;

		DeleteInput(nRet);
	}

	InpdListMake(0);							// refresh view
	return 0;
}

static int InitAnalogOptions(int nGi, int nPci)
{
	// init analog options dialog
	int nAnalog = -1;
	if (nPci == (nPlayerDefaultControls[nGi] & 0x0F)) {
		nAnalog = nPlayerDefaultControls[nGi] >> 4;
	}

	SendMessage(hInpdAnalog, CB_RESETCONTENT, 0, 0);
	if (nPci >= 1 && nPci <= 3) {
		// Absolute mode only for joysticks
		SendMessage(hInpdAnalog, CB_ADDSTRING, 0, (LPARAM)(LPARAM)FBALoadStringEx(hAppInst, IDS_INPUT_ANALOG_ABS, true));
	} else {
		if (nAnalog > 0) {
			nAnalog--;
		}
	}
	SendMessage(hInpdAnalog, CB_ADDSTRING, 0, (LPARAM)(LPARAM)FBALoadStringEx(hAppInst, IDS_INPUT_ANALOG_AUTO, true));
	SendMessage(hInpdAnalog, CB_ADDSTRING, 0, (LPARAM)(LPARAM)FBALoadStringEx(hAppInst, IDS_INPUT_ANALOG_NORMAL, true));

	SendMessage(hInpdAnalog, CB_SETCURSEL, (WPARAM)nAnalog, 0);

	return 0;
}

static void SaveHardwarePreset()
{
	TCHAR *szFileName = _T("config\\presets\\preset.ini");
	TCHAR *szHardwareString = _T("Generic hardware");

	int nHardwareFlag = GameInputGetHWFlag();
	bool bSaveDIPs = false;

	// See if nHardwareFlag belongs to any systems (nes.ini, neogeo.ini, etc) in gamehw_config (see: burner/gami.cpp)
	for (INT32 i = 0; gamehw_cfg[i].ini[0] != '\0'; i++) {
		for (INT32 hw = 0; gamehw_cfg[i].hw[hw] != 0; hw++) {
			if (gamehw_cfg[i].hw[hw] == nHardwareFlag)
			{
				szFileName = gamehw_cfg[i].ini;
				szHardwareString = gamehw_cfg[i].system;
				bSaveDIPs = gamehw_cfg[i].dips_in_preset;
				break;
			}
		}
	}

	FILE *fp = _tfopen(szFileName, _T("wt"));
	if (fp) {
		_ftprintf(fp, _T(APP_TITLE) _T(" - Hardware Default Preset\n\n"));
		_ftprintf(fp, _T("%s\n\n"), szHardwareString);
		_ftprintf(fp, _T("version 0x%06X\n\n"), nBurnVer);
		GameInpWrite(fp, bSaveDIPs);
		fclose(fp);
	}

	// add to dropdown (if not already there)
	TCHAR szPresetName[MAX_PATH] = _T("");
	int iCBItem = 0;

	memcpy(szPresetName, szFileName + 15, (_tcslen(szFileName) - 19) * sizeof(TCHAR));
	iCBItem = SendMessage(hInpdPci, CB_FINDSTRING, -1, (LPARAM)szPresetName);
	if (iCBItem == -1) SendMessage(hInpdPci, CB_ADDSTRING, 0, (LPARAM)szPresetName);

	// confirm to user
	FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_PRESET_SAVED), szFileName);
	FBAPopupDisplay(PUF_TYPE_INFO);
}

int UsePreset(bool bMakeDefault)
{
	int nGi, nPci, nAnalog = 0;
	TCHAR szFilename[MAX_PATH] = _T("config\\presets\\");

	nGi = SendMessage(hInpdGi, CB_GETCURSEL, 0, 0);
	if (nGi == CB_ERR) {
		return 1;
	}
	nPci = SendMessage(hInpdPci, CB_GETCURSEL, 0, 0);
	if (nPci == CB_ERR) {
		return 1;
	}
	if (nPci <= 7) {
		// Determine analog option
		nAnalog = SendMessage(hInpdAnalog, CB_GETCURSEL, 0, 0);
		if (nAnalog == CB_ERR) {
			return 1;
		}

		if (nPci == 0 || nPci > 3) {				// No "Absolute" option for keyboard or X-Arcade/HotRod controls
			nAnalog++;
		}

		GameInpConfig(nGi, nPci, nAnalog);			// Re-configure inputs
	} else {
		// Find out the filename of the preset ini
		SendMessage(hInpdPci, CB_GETLBTEXT, nPci, (LPARAM)(szFilename + _tcslen(szFilename)));
		_tcscat(szFilename, _T(".ini"));

		GameInputAutoIni(nGi, szFilename, true);	// Read inputs from file

		// Make sure all inputs are defined
		for (unsigned int i = 0, j = 0; i < nGameInpCount; i++) {
			if (GameInp[i].Input.pVal == NULL) {
				continue;
			}

			if (GameInp[i].nInput == 0) {
				DeleteInput(j);
			}

			j++;
		}

		nPci = 0x0F;
	}

	SendMessage(hInpdAnalog, CB_SETCURSEL, (WPARAM)-1, 0);
	SendMessage(hInpdPci, CB_SETCURSEL, (WPARAM)-1, 0);
	SendMessage(hInpdGi, CB_SETCURSEL, (WPARAM)-1, 0);

	DisablePresets();

	if (bMakeDefault) {
		nPlayerDefaultControls[nGi] = nPci | (nAnalog << 4);
		_tcscpy(szPlayerDefaultIni[nGi], szFilename);
	}

	GameInpCheckLeftAlt();

	return 0;
}

static HICON hIcon = NULL;

static void ToolTipAndIconInit()
{
	// re-detect controllers (magnify glass) icon
	hIcon = (HICON)LoadImage(hAppInst, MAKEINTRESOURCE(IDI_INPD_DETECT_GAMEPADS), IMAGE_ICON, 25, 25, 0);
	SendDlgItemMessage(hInpdDlg, IDC_INPD_DETECT_GAMEPADS, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)hIcon);

	// Pop-up bubble (tooltip) for the "re-detect controllers" icon   - dink (dec. 2025)
	hToolTipWnd = CreateWindow(TOOLTIPS_CLASS, NULL, WS_POPUP, 0,0,0,0, hInpdDlg, NULL, hAppInst, 0);

	if (hToolTipWnd == NULL) return;

	TOOLINFO toolinfo;
	memset(&toolinfo, 0, sizeof(TOOLINFO));

	toolinfo.cbSize = sizeof(TOOLINFO);
	toolinfo.hwnd = hInpdDlg;
	toolinfo.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
	toolinfo.uId = (UINT_PTR)GetDlgItem(hInpdDlg, IDC_INPD_DETECT_GAMEPADS);
	toolinfo.hinst = NULL;
	toolinfo.lpszText = _T("Re-Detect Controllers");

	SendMessage(hToolTipWnd, TTM_ACTIVATE, TRUE, 0);
	SendMessage(hToolTipWnd, TTM_ADDTOOL, 0, (LPARAM)&toolinfo);
}

static void ToolTipAndIconExit()
{
	// Icon clean-up
	DestroyIcon((HICON)hIcon);

	// ToolTip cleanup
	// Nothing to do here!

	// Note: the tooltip window handle is automatically cleaned up
	// by the tooltip's parent window, 'hInpdDlg', is destroyed.
}

static void SliderInit() // Analog sensitivity slider
{
	// Initialise slider
	SendDlgItemMessage(hInpdDlg, IDC_INPD_ANSLIDER, TBM_SETRANGE, (WPARAM)0, (LPARAM)MAKELONG(0x01, 0x0400));
	SendDlgItemMessage(hInpdDlg, IDC_INPD_ANSLIDER, TBM_SETLINESIZE, (WPARAM)0, (LPARAM)0x05);
	SendDlgItemMessage(hInpdDlg, IDC_INPD_ANSLIDER, TBM_SETPAGESIZE, (WPARAM)0, (LPARAM)0x10);
	SendDlgItemMessage(hInpdDlg, IDC_INPD_ANSLIDER, TBM_SETTIC, (WPARAM)0, (LPARAM)0x0100);
	SendDlgItemMessage(hInpdDlg, IDC_INPD_ANSLIDER, TBM_SETTIC, (WPARAM)0, (LPARAM)0x0200);
	SendDlgItemMessage(hInpdDlg, IDC_INPD_ANSLIDER, TBM_SETTIC, (WPARAM)0, (LPARAM)0x0300);
	SendDlgItemMessage(hInpdDlg, IDC_INPD_ANSLIDER, TBM_SETTIC, (WPARAM)0, (LPARAM)0x0400);

	// Set slider to current value
	SendDlgItemMessage(hInpdDlg, IDC_INPD_ANSLIDER, TBM_SETPOS, (WPARAM)true, (LPARAM)nAnalogSpeed);

	// Set the edit control to current value
	TCHAR szText[16];
	_stprintf(szText, _T("%i"), nAnalogSpeed * 100 / 256);
	SendDlgItemMessage(hInpdDlg, IDC_INPD_ANEDIT, WM_SETTEXT, (WPARAM)0, (LPARAM)szText);
}

static void SliderUpdate()
{
	TCHAR szText[16] = _T("");
	bool bValid = 1;
	int nValue;

	if (SendDlgItemMessage(hInpdDlg, IDC_INPD_ANEDIT, WM_GETTEXTLENGTH, (WPARAM)0, (LPARAM)0) < 16) {
		SendDlgItemMessage(hInpdDlg, IDC_INPD_ANEDIT, WM_GETTEXT, (WPARAM)16, (LPARAM)szText);
	}

	// Scan string in the edit control for illegal characters
	for (int i = 0; szText[i]; i++) {
		if (!_istdigit(szText[i])) {
			bValid = 0;
			break;
		}
	}

	if (bValid) {
		nValue = _tcstol(szText, NULL, 0);
		if (nValue < 1) {
			nValue = 1;
		} else {
			if (nValue > 400) {
				nValue = 400;
			}
		}

		nValue = (int)((double)nValue * 256.0 / 100.0 + 0.5);

		// Set slider to current value
		SendDlgItemMessage(hInpdDlg, IDC_INPD_ANSLIDER, TBM_SETPOS, (WPARAM)true, (LPARAM)nValue);
	}
}

static void SliderExit()
{
	TCHAR szText[16] = _T("");
	INT32 nVal = 0;

	SendDlgItemMessage(hInpdDlg, IDC_INPD_ANEDIT, WM_GETTEXT, (WPARAM)16, (LPARAM)szText);
	nVal = _tcstol(szText, NULL, 0);
	if (nVal < 1) {
		nVal = 1;
	} else {
		if (nVal > 400) {
			nVal = 400;
		}
	}

	nAnalogSpeed = (int)((double)nVal * 256.0 / 100.0 + 0.5);
	//bprintf(0, _T("  * Analog Speed: %X\n"), nAnalogSpeed);
}

static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_INITDIALOG) {
		hInpdDlg = hDlg;

		ToolTipAndIconInit();
		InpdInit();
		SliderInit();
		if (!kNetGame && bAutoPause) {
			bRunPause = 1;
		}

		return TRUE;
	}

	if (Msg == WM_CLOSE) {
		SliderExit();
		ToolTipAndIconExit();
		EnableWindow(hScrnWnd, TRUE);
		DestroyWindow(hInpdDlg);
		return 0;
	}

	if (Msg == WM_DESTROY) {
		InpdExit();
		return 0;
	}

	if (Msg == WM_COMMAND) {
		int Id = LOWORD(wParam);
		int Notify = HIWORD(wParam);

		if (Id == IDC_INPD_DETECT_GAMEPADS && Notify == BN_CLICKED) {
			// re-init directinput (detect gamepads)
			InputExit();
			InputInit();
			return 0;
		}
		if (Id == IDOK && Notify == BN_CLICKED) {
			ListItemActivate();
			return 0;
		}
		if (Id == IDCANCEL && Notify == BN_CLICKED) {

			SendMessage(hDlg, WM_CLOSE, 0, 0);

			return 0;
		}

		if (Id == IDC_INPD_NEWMACRO && Notify == BN_CLICKED) {

//			NewMacroButton();

			return 0;
		}

		if (Id == IDC_INPD_SAVE_AS_PRESET && Notify == BN_CLICKED) {
			SaveHardwarePreset();
			return 0;
		}

		if (Id == IDC_INPD_USE && Notify == BN_CLICKED) {

			UsePreset(false);

			InpdListMake(0);								// refresh view

			return 0;
		}

		if (Id == IDC_INPD_DEFAULT && Notify == BN_CLICKED) {

			UsePreset(true);

			InpdListMake(0);								// refresh view

			return 0;
		}

		if (Notify == EN_UPDATE) {                          // analog slider update
			SliderUpdate();

			return 0;
		}

		if (Id == IDC_INPD_GI && Notify == CBN_SELCHANGE) {
			int nGi;
			nGi = SendMessage(hInpdGi, CB_GETCURSEL, 0, 0);
			if (nGi == CB_ERR) {
				SendMessage(hInpdPci, CB_SETCURSEL, (WPARAM)-1, 0);
				SendMessage(hInpdAnalog, CB_SETCURSEL, (WPARAM)-1, 0);

				DisablePresets();

				return 0;
			}
			int nPci = nPlayerDefaultControls[nGi] & 0x0F;
			SendMessage(hInpdPci, CB_SETCURSEL, nPci, 0);
			EnableWindow(hInpdPci, TRUE);

			if (nPci > 5) {
				SendMessage(hInpdAnalog, CB_SETCURSEL, (WPARAM)-1, 0);
				EnableWindow(hInpdAnalog, FALSE);
			} else {
				InitAnalogOptions(nGi, nPci);
				EnableWindow(hInpdAnalog, TRUE);
			}

			EnableWindow(GetDlgItem(hInpdDlg, IDC_INPD_DEFAULT), TRUE);
			EnableWindow(GetDlgItem(hInpdDlg, IDC_INPD_USE), TRUE);

			return 0;
		}

		if (Id == IDC_INPD_PCI && Notify == CBN_SELCHANGE) {
			int nGi, nPci;
			nGi = SendMessage(hInpdGi, CB_GETCURSEL, 0, 0);
			if (nGi == CB_ERR) {
				return 0;
			}
			nPci = SendMessage(hInpdPci, CB_GETCURSEL, 0, 0);
			if (nPci == CB_ERR) {
				return 0;
			}

			if (nPci > 7) {
				EnableWindow(GetDlgItem(hInpdDlg, IDC_INPD_DEFAULT), TRUE);
				EnableWindow(GetDlgItem(hInpdDlg, IDC_INPD_USE), TRUE);

				SendMessage(hInpdAnalog, CB_SETCURSEL, (WPARAM)-1, 0);
				EnableWindow(hInpdAnalog, FALSE);
			} else {
				EnableWindow(hInpdAnalog, TRUE);
				InitAnalogOptions(nGi, nPci);

				if (SendMessage(hInpdAnalog, CB_GETCURSEL, 0, 0) != CB_ERR) {
					EnableWindow(GetDlgItem(hInpdDlg, IDC_INPD_DEFAULT), TRUE);
					EnableWindow(GetDlgItem(hInpdDlg, IDC_INPD_USE), TRUE);
				} else {
					EnableWindow(GetDlgItem(hInpdDlg, IDC_INPD_DEFAULT), FALSE);
					EnableWindow(GetDlgItem(hInpdDlg, IDC_INPD_USE), FALSE);
				}
			}

			return 0;
		}

		if (Id == IDC_INPD_ANALOG && Notify == CBN_SELCHANGE) {
			if (SendMessage(hInpdAnalog, CB_GETCURSEL, 0, 0) != CB_ERR) {
				EnableWindow(GetDlgItem(hInpdDlg, IDC_INPD_DEFAULT), TRUE);
				EnableWindow(GetDlgItem(hInpdDlg, IDC_INPD_USE), TRUE);
			}

			return 0;
		}

	}

	if (Msg == WM_HSCROLL) { // Analog Slider updates
		switch (LOWORD(wParam)) {
			case TB_BOTTOM:
			case TB_ENDTRACK:
			case TB_LINEDOWN:
			case TB_LINEUP:
			case TB_PAGEDOWN:
			case TB_PAGEUP:
			case TB_THUMBPOSITION:
			case TB_THUMBTRACK:
			case TB_TOP: {
				TCHAR szText[16] = _T("");
				int nValue;

				// Update the contents of the edit control
				nValue = SendDlgItemMessage(hDlg, IDC_INPD_ANSLIDER, TBM_GETPOS, (WPARAM)0, (LPARAM)0);
				nValue = (int)((double)nValue * 100.0 / 256.0 + 0.5);
				_stprintf(szText, _T("%i"), nValue);
				SendDlgItemMessage(hDlg, IDC_INPD_ANEDIT, WM_SETTEXT, (WPARAM)0, (LPARAM)szText);
				break;
			}
		}

		return 0;
	}

	if (Msg == WM_NOTIFY && lParam != 0) {
		int Id = LOWORD(wParam);
		NMHDR* pnm = (NMHDR*)lParam;
#if 0
		// save for later -dink
		if (pnm->code == TRBN_THUMBPOSCHANGING) {
			PNMTRBTHUMBPOSCHANGING pnmh = (PNMTRBTHUMBPOSCHANGING)lParam;
			int originalPos = pnmh->dwPos;
			int snappedPos = originalPos + 0x20;

			if (snappedPos != originalPos) {
				bprintf(0, _T("snappy dappy %x -> %x\n"), originalPos, snappedPos);
				SendDlgItemMessage(hInpdDlg, IDC_INPD_ANSLIDER, TBM_SETPOS, (WPARAM)true, (LPARAM)snappedPos);
				return TRUE;
			}
			return FALSE;
		}
#endif
		if (Id == IDC_INPD_LIST && pnm->code == LVN_ITEMACTIVATE) {
			ListItemActivate();
		}
		if (Id == IDC_INPD_LIST && pnm->code == LVN_KEYDOWN) {
			NMLVKEYDOWN *pnmkd = (NMLVKEYDOWN*)lParam;
			if (pnmkd->wVKey == VK_DELETE) {
				ListItemDelete();
			}
		}
		if (Id == IDC_INPD_LIST && pnm->code == LVN_ITEMCHANGED) {
			/* Clear the checkboxs before the non Macro buttons
			   After that, you should not access these checkboxes that have been eliminated
			   Otherwise, the program will throw an exception due to incorrect access */
			NMLISTVIEW* pNMListView = (NMLISTVIEW*)pnm;
			struct GameInp *pgi = GameInp + pNMListView->iItem;

			if (pNMListView->iItem < nGameInpCount || pgi->Macro.nSysMacro == 1) {
				// Item is a normal game input or system macro, tell system not to draw
				// checkbox.
				LVITEM LvItem;
				memset(&LvItem, 0, sizeof(LvItem));

				LvItem.iItem = pNMListView->iItem;
				LvItem.mask = LVIF_STATE;
				LvItem.stateMask = LVIS_STATEIMAGEMASK;
				LvItem.state = 0;

				SendMessage(hInpdList, LVM_SETITEM, 0, (LPARAM)&LvItem);
				return 0;
			}

			// Avoid setting checkboxes that haven't been mapped yet
			if (!pgi->Input.pVal && pgi->Macro.nSysMacro != 1 && pgi->Macro.szName) {
				// Check that the checkbox is properly checked
				if (ListView_GetCheckState(hInpdList, pNMListView->iItem)) {
					ListView_SetCheckState(hInpdList, pNMListView->iItem, 0);
					if (bClearInputIgnoreCheckboxMessage == 0) {
						MessageBox(hInpdDlg, FBALoadStringEx(hAppInst, IDS_ERR_MACRO_NOT_MAPPING, true), NULL, MB_ICONWARNING);
					}
					bClearInputIgnoreCheckboxMessage = 0;
				}
			} else {
				if (bInittingCheckboxes == 0) { // Avoid race-condition w/InpdListMake()
					// Checkbox value changed, update input struct
					if (pgi->Macro.szName && pgi->Macro.nSysMacro != 1) { // Exclude System Macro's
						pgi->Macro.nSysMacro = ListView_GetCheckState(hInpdList, pNMListView->iItem) ? 15 : 0;
					}
				}
			}
		}
		if (Id == IDC_INPD_LIST && pnm->code == NM_CUSTOMDRAW) {
			NMLVCUSTOMDRAW* plvcd = (NMLVCUSTOMDRAW*)lParam;

			switch (plvcd->nmcd.dwDrawStage) {
				case CDDS_PREPAINT:
					SetWindowLongPtr(hInpdDlg, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
					return 1;
				case CDDS_ITEMPREPAINT:
					if (plvcd->nmcd.dwItemSpec < nGameInpCount) {
						if (GameInp[plvcd->nmcd.dwItemSpec].nType & BIT_GROUP_CONSTANT) {

							if (GameInp[plvcd->nmcd.dwItemSpec].nInput == 0) {
								plvcd->clrTextBk = RGB(0xDF, 0xDF, 0xDF);

								SetWindowLongPtr(hInpdDlg, DWLP_MSGRESULT, CDRF_NEWFONT);
								return 1;
							}

							if (GameInp[plvcd->nmcd.dwItemSpec].nType == BIT_DIPSWITCH) {
								plvcd->clrTextBk = RGB(0xFF, 0xEF, 0xD7);

								SetWindowLongPtr(hInpdDlg, DWLP_MSGRESULT, CDRF_NEWFONT);
								return 1;
							}
						}
					}

					if (plvcd->nmcd.dwItemSpec >= nGameInpCount) {
						if (GameInp[plvcd->nmcd.dwItemSpec].Macro.nMode) {
							plvcd->clrTextBk = RGB(0xFF, 0xCF, 0xCF);
						} else {
							plvcd->clrTextBk = RGB(0xFF, 0xEF, 0xEF);
						}

						SetWindowLongPtr(hInpdDlg, DWLP_MSGRESULT, CDRF_NEWFONT);
						return 1;
					}
					return 1;
			}
		}

		return 0;
	}

	return 0;
}

int InpdCreate()
{
	if (bDrvOkay == 0) {
		return 1;
	}

	DestroyWindow(hInpdDlg);										// Make sure exitted

	hInpdDlg = FBACreateDialog(hAppInst, MAKEINTRESOURCE(IDD_INPD), hScrnWnd, (DLGPROC)DialogProc);
	if (hInpdDlg == NULL) {
		return 1;
	}

	WndInMid(hInpdDlg, hScrnWnd);
	ShowWindow(hInpdDlg, SW_NORMAL);

	return 0;
}


