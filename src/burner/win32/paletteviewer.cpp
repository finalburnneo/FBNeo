#include "burner.h"

static HWND hPaletteViewerDlg = nullptr;
static HWND hParent = nullptr;
static HWND PaletteControl[256] = {nullptr,};
static HBRUSH PaletteBrush[256] = {nullptr,};

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

	for (int i = 0; i < 256; i++)
	{
		DeleteObject(PaletteBrush[i]);
		PaletteBrush[i] = nullptr;

		if (i + nStartColour < nPaletteEntries)
		{
			if (is_CapcomCPS1or2())
			{
				Colour = pBurnDrvPalette[(i + nStartColour) ^ 0xf];
			}
			else
			{
				Colour = pBurnDrvPalette[i + nStartColour];
			}

			if (nVidImageDepth < 16 || (BurnDrvGetFlags() & BDF_16BIT_ONLY))
			{
				// 15-bit
				r = (Colour & 0x7c00) >> 7;
				g = (Colour & 0x03e0) >> 2;
				b = (Colour & 0x001f) << 3;
			}
			else if (nVidImageDepth == 16)
			{
				// 16-bit
				r = (Colour & 0xf800) >> 8;
				g = (Colour & 0x07e0) >> 3;
				b = (Colour & 0x001f) << 3;
			}
			else
			{
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

	for (int i = 0; i < 16; i++)
	{
		int nLabel = nPalettePosition + (i * 16);
		szItemText[0] = _T('\0');
		_stprintf(szItemText, _T("%05X"), nLabel);
		SendMessage(GetDlgItem(hPaletteViewerDlg, IDC_GFX_VIEWER_VERT_1 + i), WM_SETTEXT, 0, (LPARAM)szItemText);
	}
}

static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_INITDIALOG)
	{
		hPaletteViewerDlg = hDlg;

		if (bDrvOkay)
		{
			if (!kNetGame && bAutoPause) bRunPause = 1;
			AudSoundStop();
		}

		for (int y = 0; y < 16; y++)
		{
			for (int x = 0; x < 16; x++)
			{
				PaletteControl[(y * 16) + x] = CreateWindowEx(0, _T("STATIC"), nullptr,
				                                              WS_CHILD | WS_VISIBLE | SS_NOTIFY, (x * 21) + 38,
				                                              (y * 21) + 21, 20, 20, hPaletteViewerDlg, nullptr,
				                                              nullptr, nullptr);
			}
		}

		nPaletteEntries = BurnDrvGetPaletteEntries();
		nPalettePosition = 0x0000;
		CalcBrushes(nPalettePosition);

		WndInMid(hDlg, hParent);
		SetFocus(hDlg);

		return TRUE;
	}

	if (Msg == WM_CTLCOLORSTATIC)
	{
		for (int i = 0; i < 256; i++)
		{
			if ((HWND)lParam == PaletteControl[i])
			{
				return (INT_PTR)PaletteBrush[i];
			}
		}
	}

	if (Msg == WM_CLOSE)
	{
		for (int i = 0; i < 256; i++)
		{
			DeleteObject(PaletteBrush[i]);
			PaletteBrush[i] = nullptr;
			PaletteControl[i] = nullptr;
		}

		nPalettePosition = 0;
		nPaletteEntries = 0;

		EndDialog(hPaletteViewerDlg, 0);

		EnableWindow(hScrnWnd, TRUE);
		DestroyWindow(hPaletteViewerDlg);

		if (bDrvOkay)
		{
			if (!bAltPause && bRunPause) bRunPause = 0;
			AudSoundPlay();
		}

		return 0;
	}

	if (Msg == WM_COMMAND)
	{
		int Id = LOWORD(wParam);
		int Notify = HIWORD(wParam);

		if (Notify == STN_CLICKED)
		{
			TCHAR szText[100];

			for (int i = 0; i < 256; i++)
			{
				if ((HWND)lParam == PaletteControl[i])
				{
					int Colour, r, g, b;

					if (is_CapcomCPS1or2())
					{
						Colour = pBurnDrvPalette[(i + nPalettePosition) ^ 0xf];
					}
					else
					{
						Colour = pBurnDrvPalette[i + nPalettePosition];
					}

					if (nVidImageDepth < 16 || (BurnDrvGetFlags() & BDF_16BIT_ONLY))
					{
						// 15-bit
						r = (Colour & 0x7c00) >> 7;
						g = (Colour & 0x03e0) >> 2;
						b = (Colour & 0x001f) << 3;
					}
					else if (nVidImageDepth == 16)
					{
						// 16-bit
						r = (Colour & 0xf800) >> 8;
						g = (Colour & 0x07e0) >> 3;
						b = (Colour & 0x001f) << 3;
					}
					else
					{
						// 24/32-bit
						r = (Colour & 0xff0000) >> 16;
						g = (Colour & 0x00ff00) >> 8;
						b = (Colour & 0x0000ff);
					}

					szText[0] = _T('\0');
					_stprintf(szText, _T("Selected colour: #%X  RGB #%02X%02X%02X"), i + nPalettePosition, r, g, b);
					SendMessage(GetDlgItem(hPaletteViewerDlg, IDC_GFX_VIEWER_SEL_COL), WM_SETTEXT, 0, (LPARAM)szText);
					return 0;
				}
			}
		}

		if (Id == IDCANCEL && Notify == BN_CLICKED)
		{
			SendMessage(hPaletteViewerDlg, WM_CLOSE, 0, 0);
			return 0;
		}

		if (Id == IDC_GFX_VIEWER_PREV && Notify == BN_CLICKED)
		{
			nPalettePosition -= 0x100;
			if (nPalettePosition < 0) nPalettePosition = nPaletteEntries - 0x100; // last page
			if (nPalettePosition < 0) nPalettePosition = 0x0000; // no last page, stay on first
			CalcBrushes(nPalettePosition);
			RedrawWindow(hPaletteViewerDlg, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
			UpdateLabels();
			return 0;
		}

		if (Id == IDC_GFX_VIEWER_NEXT && Notify == BN_CLICKED)
		{
			nPalettePosition += 0x100;
			if (nPalettePosition >= nPaletteEntries) nPalettePosition = 0x0000;
			CalcBrushes(nPalettePosition);
			RedrawWindow(hPaletteViewerDlg, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
			UpdateLabels();
			return 0;
		}
	}

	return 0;
}

int PaletteViewerDialogCreate(HWND hParentWND)
{
	if (pBurnDrvPalette == nullptr)
	{
		bprintf(PRINT_IMPORTANT, _T("No DrvPalette associated with this game.\n"));
		return 1;
	}

	hParent = hParentWND;
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_PALETTEVIEWER), hParent, DialogProc);

	hParent = nullptr;
	hPaletteViewerDlg = nullptr;

	return 0;
}
