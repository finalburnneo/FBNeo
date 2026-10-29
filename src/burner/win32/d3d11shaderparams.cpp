// Shader parameters dialog for the Direct3D 11 blitter
//
// Lists every "#pragma parameter" of the loaded shader with a slider. Changes are applied
// immediately; OK saves them (config/shaders/<shader>.ini), Cancel restores the old values.
// The dialog is built in code because the number of parameters varies (from 0 to 100+).

#include "burner.h"
#include <commctrl.h>
#include <math.h>

#define IDC_SHADERPARAM_DEFAULTS	1001
#define SHADERPARAM_PANEL_CLASS		_T("FBNeoShaderParamPanel")

struct ShaderParamRow {
	HWND hLabel;
	HWND hSlider;
	HWND hValue;
	float fMin, fMax, fStep, fDefault, fOriginal, fValue;
	int nSteps;
	int nDecimals;
};

static ShaderParamRow* pRows = NULL;
static int nRows = 0;
static HWND hPanel = NULL;
static int nScrollPos = 0;
static int nContentHeight = 0;
static int nRowHeight = 24;
static HFONT hDialogFont = NULL;

static void ShaderParamFormat(int n, TCHAR* szBuf, int nSize)
{
	_sntprintf(szBuf, nSize, _T("%.*f"), pRows[n].nDecimals, pRows[n].fValue);
	szBuf[nSize - 1] = 0;
}

static int ShaderParamToPos(int n, float fValue)
{
	ShaderParamRow& r = pRows[n];
	if (r.nSteps <= 0) return 0;
	int nPos = (int)floor((fValue - r.fMin) / r.fStep + 0.5f);
	return (nPos < 0) ? 0 : (nPos > r.nSteps) ? r.nSteps : nPos;
}

static float ShaderParamFromPos(int n, int nPos)
{
	ShaderParamRow& r = pRows[n];
	if (nPos >= r.nSteps) return r.fMax;
	float f = r.fMin + nPos * r.fStep;
	return (f > r.fMax) ? r.fMax : f;
}

static void ShaderParamSet(int n, float fValue, bool bMoveSlider)
{
	TCHAR szValue[32];

	pRows[n].fValue = fValue;
	ShaderParamFormat(n, szValue, 32);
	SetWindowText(pRows[n].hValue, szValue);
	if (bMoveSlider) {
		SendMessage(pRows[n].hSlider, TBM_SETPOS, TRUE, ShaderParamToPos(n, fValue));
	}

	VidD3D11ShaderSetParam(n, fValue);
}

static void ShaderParamScrollTo(int nPos)
{
	RECT rc;
	GetClientRect(hPanel, &rc);

	int nMax = nContentHeight - (rc.bottom - rc.top);
	if (nPos > nMax) nPos = nMax;
	if (nPos < 0) nPos = 0;

	if (nPos != nScrollPos) {
		ScrollWindowEx(hPanel, 0, nScrollPos - nPos, NULL, NULL, NULL, NULL, SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
		nScrollPos = nPos;
	}

	SCROLLINFO si;
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	si.nMin = 0;
	si.nMax = nContentHeight - 1;
	si.nPage = rc.bottom - rc.top;
	si.nPos = nScrollPos;
	SetScrollInfo(hPanel, SB_VERT, &si, TRUE);
}

static LRESULT CALLBACK ShaderParamPanelProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
		case WM_VSCROLL: {
			RECT rc;
			GetClientRect(hWnd, &rc);
			int nPage = rc.bottom - rc.top;
			int nPos = nScrollPos;

			switch (LOWORD(wParam)) {
				case SB_LINEUP:        nPos -= nRowHeight; break;
				case SB_LINEDOWN:      nPos += nRowHeight; break;
				case SB_PAGEUP:        nPos -= nPage; break;
				case SB_PAGEDOWN:      nPos += nPage; break;
				case SB_TOP:           nPos = 0; break;
				case SB_BOTTOM:        nPos = nContentHeight; break;
				case SB_THUMBTRACK:
				case SB_THUMBPOSITION: {
					SCROLLINFO si;
					memset(&si, 0, sizeof(si));
					si.cbSize = sizeof(si);
					si.fMask = SIF_TRACKPOS;
					GetScrollInfo(hWnd, SB_VERT, &si);
					nPos = si.nTrackPos;
					break;
				}
			}
			ShaderParamScrollTo(nPos);
			return 0;
		}

		case WM_MOUSEWHEEL: {
			int nDelta = GET_WHEEL_DELTA_WPARAM(wParam);
			ShaderParamScrollTo(nScrollPos - (nDelta / WHEEL_DELTA) * nRowHeight * 3);
			return 0;
		}

		case WM_HSCROLL: {
			HWND hSlider = (HWND)lParam;
			for (int i = 0; i < nRows; i++) {
				if (pRows[i].hSlider == hSlider) {
					int nPos = (int)SendMessage(hSlider, TBM_GETPOS, 0, 0);
					float fValue = ShaderParamFromPos(i, nPos);
					if (fValue != pRows[i].fValue) {
						ShaderParamSet(i, fValue, false);
					}
					break;
				}
			}
			return 0;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static void ShaderParamCreateRows(HWND hDlg)
{
	RECT rc;
	GetClientRect(hPanel, &rc);

	int nWidth = rc.right - rc.left - GetSystemMetrics(SM_CXVSCROLL);
	int nLabelWidth = nWidth * 50 / 100;
	int nValueWidth = nWidth * 13 / 100;
	int nSliderWidth = nWidth - nLabelWidth - nValueWidth - 16;

	RECT du = { 0, 0, 4, 14 };					// row height in dialog units
	MapDialogRect(hDlg, &du);
	nRowHeight = du.bottom;
	if (nRowHeight < 20) nRowHeight = 20;

	for (int i = 0; i < nRows; i++) {
		ShaderParamRow& r = pRows[i];
		TCHAR szDesc[256];
		int y = i * nRowHeight;

		VidD3D11ShaderGetParam(i, szDesc, 256, &r.fValue, &r.fMin, &r.fMax, &r.fStep, &r.fDefault);
		r.fOriginal = r.fValue;
		if (r.fStep <= 0.0f) r.fStep = (r.fMax - r.fMin) / 100.0f;
		if (r.fStep <= 0.0f) r.fStep = 1.0f;
		r.nSteps = (int)floor((r.fMax - r.fMin) / r.fStep + 0.5f);
		if (r.nSteps > 100000) r.nSteps = 100000;
		if (r.nSteps < 0) r.nSteps = 0;

		r.nDecimals = 0;
		float f = r.fStep;
		while (r.nDecimals < 4 && fabs(f - floor(f + 0.5f)) > 0.0001f) {
			f *= 10.0f;
			r.nDecimals++;
		}

		r.hLabel = CreateWindowEx(0, _T("STATIC"), szDesc, WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_ENDELLIPSIS | SS_NOTIFY,
			4, y, nLabelWidth - 4, nRowHeight, hPanel, NULL, hAppInst, NULL);
		r.hSlider = CreateWindowEx(0, TRACKBAR_CLASS, _T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS | TBS_BOTH,
			nLabelWidth + 4, y, nSliderWidth, nRowHeight, hPanel, NULL, hAppInst, NULL);
		r.hValue = CreateWindowEx(0, _T("STATIC"), _T(""), WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_CENTERIMAGE,
			nLabelWidth + nSliderWidth + 8, y, nValueWidth, nRowHeight, hPanel, NULL, hAppInst, NULL);

		SendMessage(r.hLabel, WM_SETFONT, (WPARAM)hDialogFont, FALSE);
		SendMessage(r.hValue, WM_SETFONT, (WPARAM)hDialogFont, FALSE);

		SendMessage(r.hSlider, TBM_SETRANGEMIN, FALSE, 0);
		SendMessage(r.hSlider, TBM_SETRANGEMAX, FALSE, r.nSteps);
		SendMessage(r.hSlider, TBM_SETLINESIZE, 0, 1);
		SendMessage(r.hSlider, TBM_SETPAGESIZE, 0, (r.nSteps >= 10) ? r.nSteps / 10 : 1);
		SendMessage(r.hSlider, TBM_SETPOS, TRUE, ShaderParamToPos(i, r.fValue));

		TCHAR szValue[32];
		ShaderParamFormat(i, szValue, 32);
		SetWindowText(r.hValue, szValue);
	}

	nContentHeight = nRows * nRowHeight;
	nScrollPos = 0;
	ShaderParamScrollTo(0);
}

static INT_PTR CALLBACK ShaderParamDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM)
{
	switch (uMsg) {
		case WM_INITDIALOG: {
			TCHAR szName[MAX_PATH], szTitle[MAX_PATH + 64];
			VidD3D11ShaderName(szName, MAX_PATH);
			_sntprintf(szTitle, MAX_PATH + 64, _T("Shader parameters - %s"), szName);
			szTitle[MAX_PATH + 63] = 0;
			SetWindowText(hDlg, szTitle);

			hDialogFont = (HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0);

			RECT rc;
			GetClientRect(hDlg, &rc);

			RECT du = { 4, 4, 50, 14 };				// margin, button width / height in dialog units
			MapDialogRect(hDlg, &du);
			int nMargin = du.left, nButtonWidth = du.right, nButtonHeight = du.bottom;

			hPanel = CreateWindowEx(WS_EX_CLIENTEDGE | WS_EX_CONTROLPARENT, SHADERPARAM_PANEL_CLASS, _T(""), WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN | WS_TABSTOP,
				nMargin, nMargin, rc.right - 2 * nMargin, rc.bottom - 3 * nMargin - nButtonHeight, hDlg, NULL, hAppInst, NULL);

			int y = rc.bottom - nMargin - nButtonHeight;
			HWND hDefaults = CreateWindowEx(0, _T("BUTTON"), _T("&Defaults"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
				nMargin, y, nButtonWidth, nButtonHeight, hDlg, (HMENU)IDC_SHADERPARAM_DEFAULTS, hAppInst, NULL);
			HWND hOK = CreateWindowEx(0, _T("BUTTON"), _T("OK"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
				rc.right - 2 * (nButtonWidth + nMargin), y, nButtonWidth, nButtonHeight, hDlg, (HMENU)IDOK, hAppInst, NULL);
			HWND hCancel = CreateWindowEx(0, _T("BUTTON"), _T("Cancel"), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
				rc.right - (nButtonWidth + nMargin), y, nButtonWidth, nButtonHeight, hDlg, (HMENU)IDCANCEL, hAppInst, NULL);

			SendMessage(hDefaults, WM_SETFONT, (WPARAM)hDialogFont, FALSE);
			SendMessage(hOK, WM_SETFONT, (WPARAM)hDialogFont, FALSE);
			SendMessage(hCancel, WM_SETFONT, (WPARAM)hDialogFont, FALSE);

			ShaderParamCreateRows(hDlg);

			if (nRows > 0) {
				SetFocus(pRows[0].hSlider);
			} else {
				SetFocus(hOK);
			}
			return FALSE;
		}

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_SHADERPARAM_DEFAULTS:
					for (int i = 0; i < nRows; i++) {
						ShaderParamSet(i, pRows[i].fDefault, true);
					}
					return TRUE;

				case IDOK:
					VidD3D11ShaderSaveParams();
					EndDialog(hDlg, IDOK);
					return TRUE;

				case IDCANCEL:
					for (int i = 0; i < nRows; i++) {
						if (pRows[i].fValue != pRows[i].fOriginal) {
							VidD3D11ShaderSetParam(i, pRows[i].fOriginal);
						}
					}
					EndDialog(hDlg, IDCANCEL);
					return TRUE;
			}
			break;

		case WM_CLOSE:
			SendMessage(hDlg, WM_COMMAND, IDCANCEL, 0);
			return TRUE;
	}

	return FALSE;
}

// Append a string to a dialog template
static WORD* ShaderParamTemplateString(WORD* p, const wchar_t* s)
{
	while (*s) {
		*p++ = (WORD)*s++;
	}
	*p++ = 0;
	return p;
}

int D3D11ShaderParamsDialog(HWND hParent)
{
	int nCount = VidD3D11ShaderParamCount();

	if (nCount < 0) {
		FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("No shader is loaded. Select the Direct3D 11 blitter and load a shader first."));
		FBAPopupDisplay(PUF_TYPE_INFO);
		return 1;
	}
	if (nCount == 0) {
		FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("This shader has no parameters."));
		FBAPopupDisplay(PUF_TYPE_INFO);
		return 1;
	}

	static bool bRegistered = false;
	if (!bRegistered) {
		WNDCLASSEX wc;
		memset(&wc, 0, sizeof(wc));
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = ShaderParamPanelProc;
		wc.hInstance = hAppInst;
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
		wc.lpszClassName = SHADERPARAM_PANEL_CLASS;
		RegisterClassEx(&wc);
		bRegistered = true;
	}

	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_BAR_CLASSES;
	InitCommonControlsEx(&icc);

	pRows = (ShaderParamRow*)calloc(nCount, sizeof(ShaderParamRow));
	if (pRows == NULL) {
		return 1;
	}
	nRows = nCount;

	// In-memory dialog template: no controls, they're created in WM_INITDIALOG
	DWORD Buffer[256];
	memset(Buffer, 0, sizeof(Buffer));
	DLGTEMPLATE* pTemplate = (DLGTEMPLATE*)Buffer;
	pTemplate->style = DS_MODALFRAME | DS_SETFONT | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	pTemplate->cdit = 0;
	pTemplate->x = 0;
	pTemplate->y = 0;
	pTemplate->cx = 340;
	pTemplate->cy = (nCount < 6) ? 30 + nCount * 14 + 6 : 260;

	WORD* p = (WORD*)(pTemplate + 1);
	*p++ = 0;								// no menu
	*p++ = 0;								// default class
	p = ShaderParamTemplateString(p, L"Shader parameters");
	*p++ = 8;								// font size
	p = ShaderParamTemplateString(p, L"MS Shell Dlg");

	int nRet = (int)DialogBoxIndirectParam(hAppInst, pTemplate, hParent, ShaderParamDialogProc, 0);

	free(pRows);
	pRows = NULL;
	nRows = 0;
	hPanel = NULL;

	return (nRet == IDOK) ? 0 : 1;
}
