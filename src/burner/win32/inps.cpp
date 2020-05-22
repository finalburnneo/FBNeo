// Burner Input Set dialog module
#include "burner.h"

HWND hInpsDlg = NULL;						// Handle to the Input Set Dialog
static HBRUSH hWhiteBGBrush;

unsigned int nInpsInput = 0;				// The input number we are redefining
static struct BurnInputInfo bii;			// Info about the input
static int nDlgState = 0;
static int nInputCode = -1;					// If in state 3, code N was nInputCode
static int nCounter = 0;					// Counter of frames since the dialog was made
static struct GameInp* pgi = NULL;			// Current GameInp
static struct GameInp OldInp;				// Old GameInp
static int bOldPush = 0;					// 1 if the push button was pressed last time

static bool bGrabMouse = false;
static bool bClearLock = false;				// ClearLock checkbox
static bool bDigitalAnalog = false;				// DigitalAnalog checkbox
static bool bGrewDialog = false;

static bool bOldLeftAltkeyMapped;

static int InpsInit()
{
	TCHAR szTitle[128];
	memset(&OldInp, 0, sizeof(OldInp));

	pgi = NULL;
	if (nInpsInput >= nGameInpCount + nMacroCount) {		// input out of range
		return 1;
	}
	pgi = GameInp + nInpsInput;

	memset(&bii,0,sizeof(bii));
	BurnDrvGetInputInfo(&bii, nInpsInput);

	if (bii.nType & BIT_GROUP_CONSTANT) {					// This dialog doesn't handle constants
		return 1;
	}

	OldInp = *pgi;
	bOldPush = 0;

	bGrabMouse = false;
	bClearLock = false;
	bDigitalAnalog = false;
	bGrewDialog = false;

	bOldLeftAltkeyMapped = bLeftAltkeyMapped;
	bLeftAltkeyMapped = true;

	// Set the dialog title
	if (nInpsInput >= nGameInpCount) {
		// Macro
		_stprintf(szTitle, FBALoadStringEx(hAppInst, IDS_INPSET_MOVENAME, true), pgi->Macro.szName);
	} else {
		// Normal input
		if (bii.szName == NULL || bii.szName[0] == _T('\0')) {
			_stprintf(szTitle, FBALoadStringEx(hAppInst, IDS_INPSET_MOVE, true));
		} else {
			_stprintf(szTitle, FBALoadStringEx(hAppInst, IDS_INPSET_MOVENAME, true), bii.szName);
		}
	}
	SetWindowText(hInpsDlg, szTitle);

	if (bii.nType & BIT_GROUP_ANALOG) {
		ShowWindow(GetDlgItem(hInpsDlg, IDC_INPS_PUSH), SW_HIDE);
	} else {
		ShowWindow(GetDlgItem(hInpsDlg, IDC_INPS_MAPDIG), SW_HIDE);
	}

	InputFind(2);

	nDlgState = 4;
	nInputCode = -1;
	nCounter = 0;

	return 0;
}

static int InpsExit()
{
	bOldPush = 0;
	if (pgi != NULL) {
		*pgi=OldInp;
	}
	memset(&OldInp, 0, sizeof(OldInp));

	bLeftAltkeyMapped = bOldLeftAltkeyMapped;

	nDlgState = 0;

	return 0;
}

static int SetInput(int nCode)
{
	static unsigned int slider1;

	if (nDlgState & 0x0100)
	{
		unsigned int slider = SendDlgItemMessage(hInpsDlg, IDC_INPS_DIGSLIDER, TBM_GETPOS, 0, 0);

		pgi->nInput = GIT_KEYSLIDER;
		pgi->Input.Slider.nSliderSpeed = slider;

		// x1000 - xe000 = 2, e001 - x2000 = 0  <------------ DO
		if (slider <= 0x1000) {
			pgi->Input.Slider.nSliderCenter = (0x1100 - slider) / 0xdf; // 0xa @ 0x800 (default slider speed)
		} else if (slider <= 0x1800) {
			pgi->Input.Slider.nSliderCenter = 2;
		} else {
			pgi->Input.Slider.nSliderCenter = 1;
		}

		pgi->Input.Slider.SliderAxis.nSlider[1] = nCode;
		pgi->Input.Slider.SliderAxis.nSlider[0] = slider1;

		OldInp = *pgi;
		InpdListMake(0);												// Update list with new input type

		return 0;
	}

	if ((bii.nType & BIT_GROUP_ANALOG) && bDigitalAnalog)				// map analog controls to digital input #1
	{
		slider1 = nCode;

		return 1;
	}

	if ((pgi->nInput & GIT_GROUP_MACRO) == 0) {

		if (bii.nType & BIT_GROUP_CONSTANT) {							// Don't change dip switches!
			DestroyWindow(hInpsDlg);
			return 0;
		}

		if ((bii.nType & BIT_GROUP_ANALOG) && (nCode & 0xFF) < 0x10) {	// Analog controls
			if (strcmp(bii.szInfo + 4, "-axis-neg") == 0 || strcmp(bii.szInfo + 4, "-axis-pos") == 0) {
				if ((nCode & 0xF000) == 0x4000) {
					if (nCode & 1) {
						pgi->nInput = GIT_JOYAXIS_POS;
					} else {
						pgi->nInput = GIT_JOYAXIS_NEG;
					}
					pgi->Input.JoyAxis.nJoy = (nCode & 0x0F00) >> 8;
					pgi->Input.JoyAxis.nAxis = (nCode & 0x0F) >> 1;
				}
			} else {													// Map entire axis
				if ((nCode & 0xF000) == 0x4000) {
					pgi->nInput = GIT_JOYAXIS_FULL;
					pgi->Input.JoyAxis.nJoy = (nCode & 0x0F00) >> 8;
					pgi->Input.JoyAxis.nAxis = (nCode & 0x0F) >> 1;
				} else {
					pgi->nInput = GIT_MOUSEAXIS;
					pgi->Input.MouseAxis.nMouse = (nCode & 0x0F00) >> 8;
					pgi->Input.MouseAxis.nAxis = (nCode & 0x0F) >> 1;
				}
			}

			if (nCode == 0) {                                           // Clear Input button pressed (for Analogue/Mouse)
				pgi->nInput = 0;
				pgi->Input.JoyAxis.nJoy = 0;
				pgi->Input.JoyAxis.nAxis = 0;
				pgi->Input.MouseAxis.nMouse = 0;
				pgi->Input.MouseAxis.nAxis = 0;

				if (bClearLock) { // If ClearLock is checked, morph the analogue/mouse input to a switch (switch w/nCode of 0 == ClearLock)
					pgi->nInput = GIT_SWITCH;
					pgi->Input.Switch.nCode = (unsigned short)nCode;
				}
			}

		} else {
			pgi->nInput = GIT_SWITCH;
			if (nCode == 0 && !bClearLock) pgi->nInput = 0;				// Clear Input button pressed (for buttons)
			pgi->Input.Switch.nCode = (unsigned short)nCode;
		}
	} else {
		pgi->Macro.nMode = 0x01;										// Mark macro as in use
		if (nCode == 0 && !bClearLock) pgi->Macro.nMode = 0;			// Clear Input button pressed (for Macros)
		pgi->Macro.Switch.nCode = (unsigned short)nCode;				// Assign switch
	}

	OldInp = *pgi;

	InpdListMake(0);													// Update list with new input type

	return 0;
}

static int InpsPushUpdate()
{
	int nPushState = 0;

	if (pgi == NULL || nInpsInput >= nGameInpCount) {
		return 0;
	}

	// See if the push button is pressed
	nPushState = SendDlgItemMessage(hInpsDlg, IDC_INPS_PUSH, BM_GETSTATE, 0, 0);

	if (nPushState & BST_PUSHED) {
		nPushState = 1;
	} else {
		nPushState = 0;
	}

	if (nPushState) {
		switch (OldInp.nType) {
			case BIT_DIGITAL:								// Set digital inputs to 1
				pgi->nInput = GIT_CONSTANT;
				pgi->Input.Constant.nConst = 1;
				break;
			case BIT_DIPSWITCH:								// Set dipswitch block to 0xFF
				pgi->nInput = GIT_CONSTANT;
				pgi->Input.Constant.nConst = 0xFF;
				break;
		}
	} else {
		// Change back
		*pgi = OldInp;
	}
	if (nPushState != bOldPush) {							// refresh view
		InpdListMake(0);
	}

	bOldPush = nPushState;

	return nPushState;
}

static void InpsUpdateControl(int nCode)
{
	TCHAR szString[MAX_PATH] = _T("");
	TCHAR szDevice[MAX_PATH] = _T("");
	TCHAR szControl[MAX_PATH] = _T("");

	_stprintf(szString, _T("%s"), InputCodeDesc(nCode));
	SetWindowText(GetDlgItem(hInpsDlg, (nDlgState & 0x0100) ? IDC_INPS_CONTROL_S2: IDC_INPS_CONTROL), szString);

	InputGetControlName(nCode, szDevice, szControl);
	_sntprintf(szString, MAX_PATH, _T("%s %s"), szDevice, szControl);
	SetWindowText(GetDlgItem(hInpsDlg, IDC_INPS_CONTROL_NAME), szString);
}

int InpsUpdate()
{
	TCHAR szTemp[MAX_PATH] = _T("");
	int nButtonState;
	int nFind = -1;

	if (hInpsDlg == NULL) {										// Don't do anything if the dialog isn't created
		return 1;
	}
	if (nCounter < 0x100000) {									// advance frames since dialog was created
		nCounter++;
	}

	if (InpsPushUpdate()) {
		return 0;
	}

	nButtonState = SendDlgItemMessage(hInpsDlg, IDC_INPS_CLEARLOCK, BM_GETSTATE, 0, 0); // Lock Input = If checked: clear an input, and don't let a default value or default preset re-fill it in.
	bClearLock = (nButtonState & BST_CHECKED);

	bool bPrevDigitalAnalog = bDigitalAnalog;
	nButtonState = SendDlgItemMessage(hInpsDlg, IDC_INPS_MAPDIG, BM_GETSTATE, 0, 0); // Lock Input = If checked: clear an input, and don't let a default value or default preset re-fill it in.
	bDigitalAnalog = (nButtonState & BST_CHECKED);

	if (bDigitalAnalog) {
		if (!bPrevDigitalAnalog) {
			nDlgState = 4;

			if (bGrewDialog == false) {
				ShowWindow(GetDlgItem(hInpsDlg, IDC_INPS_CONTROL), SW_HIDE);
				ShowWindow(GetDlgItem(hInpsDlg, IDC_INPS_CONTROL_S1), SW_SHOW);
				ShowWindow(GetDlgItem(hInpsDlg, IDC_INPS_CONTROL_S2), SW_SHOW);
				ShowWindow(GetDlgItem(hInpsDlg, IDC_INPS_DIGLABEL), SW_SHOW);

				// Make dialog longer to make room for the Speed: slider
				RECT rect = { 0, 0, 4, 8 };
				MapDialogRect(hInpsDlg, &rect);
				int nSize = rect.bottom * 2;
				GetWindowRect(hInpsDlg, &rect);
				MoveWindow(hInpsDlg, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top + nSize, TRUE);
				bGrewDialog = true;
			}
		}
	} else {
		if (bPrevDigitalAnalog) {
			nDlgState = 4;

			if (bGrewDialog) {
				ShowWindow(GetDlgItem(hInpsDlg, IDC_INPS_CONTROL), SW_SHOW);
				ShowWindow(GetDlgItem(hInpsDlg, IDC_INPS_CONTROL_S1), SW_HIDE);
				ShowWindow(GetDlgItem(hInpsDlg, IDC_INPS_CONTROL_S2), SW_HIDE);
				ShowWindow(GetDlgItem(hInpsDlg, IDC_INPS_DIGLABEL), SW_HIDE);

				wchar_t szString[MAX_PATH] = _T("");
				SetWindowText(GetDlgItem(hInpsDlg, IDC_INPS_CONTROL_S1), szString);

				RECT rect = { 0, 0, 4, 8 };
				MapDialogRect(hInpsDlg, &rect);
				int nSize = 0-(rect.bottom * 2);
				GetWindowRect(hInpsDlg, &rect);
				MoveWindow(hInpsDlg, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top + nSize, TRUE);
				bGrewDialog = false;
			}
		}
	}

	nButtonState = SendDlgItemMessage(hInpsDlg, IDC_INPS_GRABMOUSE, BM_GETSTATE, 0, 0);
	if (bGrabMouse) {
		if ((nButtonState & BST_CHECKED) == 0) {
			bGrabMouse = false;
			nDlgState = (nDlgState & 0xFF00) | 2;
			return 0;
		}
	} else {
		if (nButtonState & BST_CHECKED) {
			bGrabMouse = true;
			nDlgState = (nDlgState & 0xFF00) | 4;
			return 0;
		}
	}
	// This doesn't work properly
	if (nButtonState & BST_PUSHED) {
		return 0;
	}

	nButtonState = SendDlgItemMessage(hInpsDlg, IDCANCEL, BM_GETSTATE, 0, 0);
	if (nButtonState & BST_PUSHED) {
		return 0;
	}

	nFind = InputFind(nDlgState & 0xFF);

	if (nDlgState & 4) {										//  4 = Waiting for all controls to be released

		if (bGrabMouse ? nFind >= 0 : (nFind >= 0 && nFind < 0x8000)) {

			if (nCounter >= 60) {

				// Alert the user that a control is stuck

				_stprintf(szTemp, FBALoadStringEx(hAppInst, IDS_INPSET_WAITING, true), InputCodeDesc(nFind));
				SetWindowText(GetDlgItem(hInpsDlg, IDC_INPS_CONTROL), szTemp);

				nCounter = 0;
			}
			return 0;
		}

		// All keys released
		SetWindowText(GetDlgItem(hInpsDlg, IDC_INPS_CONTROL), _T(""));
		SetWindowText(GetDlgItem(hInpsDlg, IDC_INPS_CONTROL_NAME), _T(""));
		nDlgState = (nDlgState & 0xFF00) | 8;
		return 0;
	}

	if (nDlgState & 8) {										//  8 = Waiting for a control to be activated

		if (bGrabMouse ? nFind < 0 : (nFind < 0 || (nFind >= 0x8000 && nFind < 0xC000))) {
			return 0;
		}

		// Key pressed

		nInputCode = nFind;
		InpsUpdateControl(nInputCode);

		nDlgState = (nDlgState & 0xFF00) | 16;

		return 0;
	}

	if (nDlgState & 16) {										// 16 = waiting for control to be released

		if (bGrabMouse ? nFind >= 0 : (nFind >= 0 && nFind < 0x8000)) {
			if (nInputCode != nFind) {
				nInputCode = nFind;
				InpsUpdateControl(nInputCode);
			}
			return 0;
		}

		// Key released (Normal input mapping)
		if (SetInput(nInputCode) == 0)
		{
			nDlgState = 0;
			SendMessage(hInpsDlg, WM_CLOSE, 0, 0);

			return 0;
		}

		// We end up here if "Map Digital" is checked, and the first key has been pressed.
		wchar_t szString[MAX_PATH] = _T("");

		swprintf(szString, _T("%s"), InputCodeDesc(nInputCode));
		SetWindowText(GetDlgItem(hInpsDlg, IDC_INPS_CONTROL_S1), szString);

		nDlgState = 0x0100 | 4;
	}

	return 0;
}

static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_INITDIALOG) {
		hInpsDlg = hDlg;
		hWhiteBGBrush = CreateSolidBrush(RGB(0xFF,0xFF,0xFF));
		if (InpsInit()) {
			DestroyWindow(hInpsDlg);
			return FALSE;
		}
		SendMessage(hDlg, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(hDlg, IDC_INPS_CONTROL), TRUE);

		// Initialise slider
		SendDlgItemMessage(hDlg, IDC_INPS_DIGSLIDER, TBM_SETRANGE, (WPARAM)0, (LPARAM)MAKELONG(0x700, 0x2000));
		SendDlgItemMessage(hDlg, IDC_INPS_DIGSLIDER, TBM_SETLINESIZE, (WPARAM)0, (LPARAM)0x200);
		SendDlgItemMessage(hDlg, IDC_INPS_DIGSLIDER, TBM_SETPAGESIZE, (WPARAM)0, (LPARAM)0x400);
		SendDlgItemMessage(hDlg, IDC_INPS_DIGSLIDER, TBM_SETTIC, (WPARAM)0, (LPARAM)0x2000);
		SendDlgItemMessage(hDlg, IDC_INPS_DIGSLIDER, TBM_SETTIC, (WPARAM)0, (LPARAM)0x1000);
		SendDlgItemMessage(hDlg, IDC_INPS_DIGSLIDER, TBM_SETTIC, (WPARAM)0, (LPARAM)0x0d00);
		SendDlgItemMessage(hDlg, IDC_INPS_DIGSLIDER, TBM_SETTIC, (WPARAM)0, (LPARAM)0x0a00);
		SendDlgItemMessage(hDlg, IDC_INPS_DIGSLIDER, TBM_SETTIC, (WPARAM)0, (LPARAM)0x0700);

		SendDlgItemMessage(hDlg, IDC_INPS_DIGSLIDER, TBM_SETPOS, (WPARAM)true, (LPARAM)0x800);

		// hide the label for the slider as it will be partially visible otherwise
		ShowWindow(GetDlgItem(hDlg, IDC_INPS_DIGLABEL), SW_HIDE);

		ShowWindow(GetDlgItem(hDlg, IDC_INPS_CONTROL_S1), SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, IDC_INPS_CONTROL_S2), SW_HIDE);

		WndInMid(hDlg, GetParent(hDlg));
		return false;
	}
	if (Msg == WM_CLOSE) {
		DestroyWindow(hInpsDlg);
		return 0;
	}
	if (Msg == WM_DESTROY) {
		DeleteObject(hWhiteBGBrush);
		InpsExit();
		hInpsDlg = NULL;
		return 0;
	}
	if (Msg == WM_COMMAND) {
		int Id = LOWORD(wParam);
		int Notify = HIWORD(wParam);
		if (Id == IDCANCEL && Notify == BN_CLICKED) { // Cancel button pressed
			SendMessage(hDlg, WM_CLOSE, 0, 0);
			return 0;
		}
		if (Id == IDCANCEL+1 && Notify == BN_CLICKED) { // Clear Input button pressed
			SetInput(0);
			SendMessage(hDlg, WM_CLOSE, 0, 0);
			return 0;
		}
	}
	if (Msg == WM_CTLCOLORSTATIC)
	{
		if ((HWND)lParam == GetDlgItem(hDlg, IDC_INPS_CONTROL) ||
			(HWND)lParam == GetDlgItem(hDlg, IDC_INPS_CONTROL_S2))
		{
			return (INT_PTR)hWhiteBGBrush;
		}
	}
	return 0;
}

int InpsCreate()
{
	DestroyWindow(hInpsDlg);					// Make sure exitted
	hInpsDlg = NULL;

	hInpsDlg = FBACreateDialog(hAppInst, MAKEINTRESOURCE(IDD_INPS), hInpdDlg, (DLGPROC)DialogProc);
	if (hInpsDlg == NULL) {
		return 1;
	}

	WndInMid(hInpsDlg, hInpdDlg);
	ShowWindow(hInpsDlg, SW_NORMAL);

	return 0;
}
