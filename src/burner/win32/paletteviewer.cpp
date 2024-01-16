#include "burner.h"
#include "winuser.h"

static HWND hPaletteViewerDlg	= NULL;
static HWND hParent		= NULL;
static HWND PaletteControl[256] = {NULL,};
static HBRUSH PaletteBrush[256] = {NULL,};

static HWND hYHeadings[0x10];

static int nPalettePosition;
static int nPaletteEntries;

static int is_CapcomCPS1or2()
{
	UINT32 cap =
		((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_CAPCOM_CPS1) ||
		((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_CAPCOM_CPS1_QSOUND) ||
		((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_CAPCOM_CPS1_GENERIC) ||
		((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_CAPCOM_CPSCHANGER) ||
		((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_CAPCOM_CPS2);

	return cap != 0;
}

static void CalcBrushes(int nStartColour)
{
	int Colour, r, g, b;

	for (int i = 0; i < 256; i++) {
		DeleteObject(PaletteBrush[i]);
		PaletteBrush[i] = NULL;

		if (i + nStartColour < nPaletteEntries) {
			if (is_CapcomCPS1or2()) {
				Colour = pBurnDrvPalette[(i + nStartColour) ^ 0xf];
			} else {
				Colour = pBurnDrvPalette[i + nStartColour];
			}

			if (nVidImageDepth < 16 || (BurnDrvGetFlags() & BDF_16BIT_ONLY)) {
				// 15-bit
				r = (Colour & 0x7c00) >> 7;
				g = (Colour & 0x03e0) >> 2;
				b = (Colour & 0x001f) << 3;
			} else
			if (nVidImageDepth == 16) {
				// 16-bit
				r = (Colour & 0xf800) >> 8;
				g = (Colour & 0x07e0) >> 3;
				b = (Colour & 0x001f) << 3;

			} else {
				// 24/32-bit
				r = (Colour & 0xff0000) >> 16;
				g = (Colour & 0x00ff00) >> 8;
				b = (Colour & 0x0000ff);
			}

			PaletteBrush[i] = CreateSolidBrush(RGB(r, g, b));
		}
	}
}

static void UpdateLabels()
{
	TCHAR szItemText[10];

	for (int i = 0; i < 16; i++) {
		int nLabel = nPalettePosition + (i * 16);
		szItemText[0] = _T('\0');
		_stprintf(szItemText, _T("%05X"), nLabel);
//		SendMessage(GetDlgItem(hPaletteViewerDlg, IDC_GFX_VIEWER_VERT_1 + i), WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
		SetWindowText(hYHeadings[i], szItemText);
	}
}

static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_INITDIALOG) {
		hPaletteViewerDlg = hDlg;

		if (bDrvOkay) {
			if (!kNetGame && bAutoPause) bRunPause = 1;
			AudSoundStop();
		}

		HDC hDc = GetDC(0);

		int dpi_x = GetDeviceCaps(hDc, LOGPIXELSX);
		int dpi_y = GetDeviceCaps(hDc, LOGPIXELSY);
		bprintf(0, _T("DPi x,y %d,%d\n"), dpi_x, dpi_y);

		double _02 = ((double) 3 * dpi_x / 96) + 0.5;
		double _20 = (double)19 * dpi_x / 96;
		double _21 = (double)20 * dpi_x / 96;
		double _st = (double)38 * dpi_x / 96; // start margin!
		double _lh = (double)33 * dpi_x / 96; // left heading

		int nHeight = -MulDiv(8, GetDeviceCaps(hDc, LOGPIXELSY), 72);
		HFONT hFont = CreateFont(nHeight, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, DEFAULT_QUALITY/*ANTIALIASED_QUALITY*/, FF_MODERN, _T("MS Shell Dlg"));

		ReleaseDC(0, hDc);

		for (int i = 0; i < 16; i++) { // X - headings
			char t[1024];
			sprintf(t, "%X", i);
			HWND hW =
				CreateWindowEx(0, _T("STATIC"), ANSIToTCHAR(t, NULL, 0), ES_CENTER | WS_CHILD | WS_VISIBLE | SS_NOTIFY, (i * _21) + _st, _02, _20, _20, hPaletteViewerDlg, NULL, NULL, NULL);
			SendMessage(hW, WM_SETFONT, (LPARAM)hFont, TRUE);
		}
		for (int i = 0; i < 16; i++) { // Y - headings
			char t[1024];
			sprintf(t, "%04X0", i);
			hYHeadings[i] =
				CreateWindowEx(0, _T("STATIC"), ANSIToTCHAR(t, NULL, 0), ES_LEFT | WS_CHILD | WS_VISIBLE | SS_NOTIFY, _02, (i * _21) + _21, _lh, _20, hPaletteViewerDlg, NULL, NULL, NULL);
			SendMessage(hYHeadings[i], WM_SETFONT, (LPARAM)hFont, TRUE);
		}

		for (int y = 0; y < 16; y++) {
			for (int x = 0; x < 16; x++) {
				PaletteControl[(y * 16) + x] =
					CreateWindowEx(0, _T("STATIC"), NULL, WS_CHILD | WS_VISIBLE | SS_NOTIFY, (x * _21) + _st, (y * _21) + _21, _20, _20, hPaletteViewerDlg, NULL, NULL, NULL);
			}
		}

		nPaletteEntries = BurnDrvGetPaletteEntries();
		nPalettePosition = 0x0000;
		CalcBrushes(nPalettePosition);

		WndInMid(hDlg, hParent);
		SetFocus(hDlg);

		return TRUE;
	}

	if (Msg == WM_CTLCOLORSTATIC) {
		for (int i = 0; i < 256; i++) {
			if ((HWND)lParam == PaletteControl[i]) {
				return (INT_PTR)PaletteBrush[i];
			}

		}
	}

	if (Msg == WM_CLOSE) {
		for (int i = 0; i < 256; i++) {
			DeleteObject(PaletteBrush[i]);
			PaletteBrush[i] = NULL;
			PaletteControl[i] = NULL;
		}

		nPalettePosition = 0;
		nPaletteEntries = 0;

		EndDialog(hPaletteViewerDlg, 0);

		EnableWindow(hScrnWnd, TRUE);
		DestroyWindow(hPaletteViewerDlg);

		if (bDrvOkay) {
			if(!bAltPause && bRunPause) bRunPause = 0;
			AudSoundPlay();
		}

		return 0;
	}

	if (Msg == WM_COMMAND) {
		int Id = LOWORD(wParam);
		int Notify = HIWORD(wParam);

		if (Notify == STN_CLICKED) {
			TCHAR szText[100];

			for (int i = 0; i < 256; i++) {
				if ((HWND)lParam == PaletteControl[i]) {
					int Colour, r, g, b;

					if (is_CapcomCPS1or2()) {
						Colour = pBurnDrvPalette[(i + nPalettePosition) ^ 0xf];
					} else {
						Colour = pBurnDrvPalette[i + nPalettePosition];
					}

					if (nVidImageDepth < 16 || (BurnDrvGetFlags() & BDF_16BIT_ONLY)) {
						// 15-bit
						r = (Colour & 0x7c00) >> 7;
						g = (Colour & 0x03e0) >> 2;
						b = (Colour & 0x001f) << 3;
					} else
					if (nVidImageDepth == 16) {
						// 16-bit
						r = (Colour & 0xf800) >> 8;
						g = (Colour & 0x07e0) >> 3;
						b = (Colour & 0x001f) << 3;
					} else {
						// 24/32-bit
						r = (Colour & 0xff0000) >> 16;
						g = (Colour & 0x00ff00) >> 8;
						b = (Colour & 0x0000ff);
					}

					szText[0] = _T('\0');
					_stprintf(szText, _T("Selected colour: #%X  RGB #%02X%02X%02X"), i + nPalettePosition, r, g, b);
					SendMessage(GetDlgItem(hPaletteViewerDlg, IDC_GFX_VIEWER_SEL_COL), WM_SETTEXT, (WPARAM)0, (LPARAM)szText);
					return 0;
				}
			}
		}

		if (Id == IDCANCEL && Notify == BN_CLICKED) {
			SendMessage(hPaletteViewerDlg, WM_CLOSE, 0, 0);
			return 0;
		}

		if (Id == IDC_GFX_VIEWER_PREV && Notify == BN_CLICKED) {
			nPalettePosition -= 0x100;
			if (nPalettePosition < 0) nPalettePosition = nPaletteEntries - 0x100; // last page
			if (nPalettePosition < 0) nPalettePosition = 0x0000; // no last page, stay on first
			CalcBrushes(nPalettePosition);
			RedrawWindow(hPaletteViewerDlg, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
			UpdateLabels();
			return 0;
		}

		if (Id == IDC_GFX_VIEWER_NEXT && Notify == BN_CLICKED) {
			nPalettePosition += 0x100;
			if (nPalettePosition >= nPaletteEntries) nPalettePosition = 0x0000;
			CalcBrushes(nPalettePosition);
			RedrawWindow(hPaletteViewerDlg, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
			UpdateLabels();
			return 0;
		}
	}

	return 0;
}

int PaletteViewerDialogCreate(HWND hParentWND)
{
	if (pBurnDrvPalette == NULL) {
		bprintf(PRINT_IMPORTANT, _T("No DrvPalette associated with this game.\n"));
		return 1;
	}

	hParent = hParentWND;
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_PALETTEVIEWER), hParent, (DLGPROC)DialogProc);

	hParent = NULL;
	hPaletteViewerDlg = NULL;

	return 0;
}
