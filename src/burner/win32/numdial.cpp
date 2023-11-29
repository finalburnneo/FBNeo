#include "burner.h"
#include <ctype.h>

constexpr double PI = 3.14159265358979323846f; // Pi
constexpr double DEGTORAD = 0.01745329251994329547f; // Degrees to Radians
constexpr double RADTODEG = 57.29577951308232286465f; // Radians to Degrees

static int nExitStatus;

// -----------------------------------------------------------------------------

static INT_PTR CALLBACK DefInpProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM)
{
	int nRet = 0;
	BOOL fTrue = FALSE;

	switch (Msg)
	{
	case WM_INITDIALOG:
		nRet = nAudSegCount;
		SetWindowText(hDlg, FBALoadStringEx(hAppInst, IDS_NUMDIAL_NUM_FRAMES, true));
		SetDlgItemInt(hDlg, IDC_VALUE_EDIT, nRet, TRUE);
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == ID_VALUE_CLOSE)
		{
			SendMessage(hDlg, WM_CLOSE, 0, 0);
		}
		else
		{
			if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDCANCEL)
			{
				SendMessage(hDlg, WM_CLOSE, 0, 0);
			}
		}
		break;

	case WM_CLOSE:
		nRet = GetDlgItemInt(hDlg, IDC_VALUE_EDIT, &fTrue, TRUE);
		if (fTrue)
		{
			nAudSegCount = nRet;
		}
		EndDialog(hDlg, 0);
	}

	return 0;
}

int NumDialCreate(int)
{
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_VALUE), hScrnWnd, DefInpProc);

	return 1;
}

// -----------------------------------------------------------------------------
// Gamma dialog

static INT_PTR CALLBACK GammaProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM) // LPARAM lParam
{
	static double nPrevGamma;

	switch (Msg)
	{
	case WM_INITDIALOG:
		{
			TCHAR szText[16];
			int nPos;

			nPrevGamma = nGamma;
			nExitStatus = 0;

			WndInMid(hDlg, hScrnWnd);

			// Initialise gamma slider
			SendDlgItemMessage(hDlg, IDC_GAMMA_SLIDER, TBM_SETRANGE, 0, MAKELONG(1, 20000));
			SendDlgItemMessage(hDlg, IDC_GAMMA_SLIDER, TBM_SETLINESIZE, 0, 200);
			SendDlgItemMessage(hDlg, IDC_GAMMA_SLIDER, TBM_SETPAGESIZE, 0, 250);
			SendDlgItemMessage(hDlg, IDC_GAMMA_SLIDER, TBM_SETTIC, 0, 7500);
			SendDlgItemMessage(hDlg, IDC_GAMMA_SLIDER, TBM_SETTIC, 0, 10001);
			SendDlgItemMessage(hDlg, IDC_GAMMA_SLIDER, TBM_SETTIC, 0, 12500);

			// Set slider to value of nGamma
			if (nGamma > 1.0)
			{
				// Gamma > 1
				nPos = static_cast<int>((nGamma - 1.0) * 10000.0 + 10001.0);
			}
			else
			{
				// Gamma < 1
				nPos = static_cast<int>(10000.0f - ((1.0 / nGamma) * 10000.0 - 10000.0));
			}
			SendDlgItemMessage(hDlg, IDC_GAMMA_SLIDER, TBM_SETPOS, true, nPos);

			// Set the edit control to the value of nGamma
			_stprintf(szText, _T("%0.2f"), nGamma);
			SendDlgItemMessage(hDlg, IDC_GAMMA_EDIT, WM_SETTEXT, 0, (LPARAM)szText);

			// Update the screen
			ComputeGammaLUT();
			if (bVidOkay)
			{
				VidRedraw();
				VidPaint(0);
			}

			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				{
					if (LOWORD(wParam) == IDOK)
					{
						nExitStatus = 1;
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					if (LOWORD(wParam) == IDCANCEL)
					{
						nExitStatus = -1;
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					break;
				}
			case EN_UPDATE:
				{
					if (nExitStatus == 0)
					{
						TCHAR szText[16] = _T("");
						bool bPoint = false;
						bool bValid = true;

						if (SendDlgItemMessage(hDlg, IDC_GAMMA_EDIT, WM_GETTEXTLENGTH, 0, 0) < 16)
						{
							SendDlgItemMessage(hDlg, IDC_GAMMA_EDIT, WM_GETTEXT, 16, (LPARAM)szText);
						}

						// Scan string in the edit control for illegal characters
						for (int i = 0; szText[i]; i++)
						{
							if (szText[i] == _T('.'))
							{
								if (bPoint)
								{
									bValid = false;
									break;
								}
							}
							else
							{
								if (!_istdigit(szText[i]))
								{
									bValid = false;
									break;
								}
							}
						}

						if (bValid)
						{
							int nPos;

							nGamma = _tcstod(szText, nullptr);

							// Set slider to value of nGamma
							if (nGamma > 1.0)
							{
								// Gamma > 1
								nPos = static_cast<int>((nGamma - 1.0) * 10000.0 + 10001.0);
							}
							else
							{
								// Gamma < 1
								nPos = static_cast<int>(10000.0 - ((1.0 / nGamma) * 10000.0 - 10000.0));
							}
							SendDlgItemMessage(hDlg, IDC_GAMMA_SLIDER, TBM_SETPOS, true, nPos);

							// Update the screen
							ComputeGammaLUT();
							if (bVidOkay)
							{
								VidRedraw();
								VidPaint(0);
							}
						}
					}
					break;
				}
				break;
			}
		}

	case WM_HSCROLL:
		{
			switch (LOWORD(wParam))
			{
			case TB_BOTTOM:
			case TB_ENDTRACK:
			case TB_LINEDOWN:
			case TB_LINEUP:
			case TB_PAGEDOWN:
			case TB_PAGEUP:
			case TB_THUMBPOSITION:
			case TB_THUMBTRACK:
			case TB_TOP:
				{
					if (nExitStatus == 0)
					{
						TCHAR szText[16];
						int nPos;

						// Update the contents of the edit control
						nPos = SendDlgItemMessage(hDlg, IDC_GAMMA_SLIDER, TBM_GETPOS, 0, 0);
						if (nPos > 10000)
						{
							// Gamma > 1
							nGamma = 1.0 + (nPos - 10000.0) / 10000.0;
						}
						else
						{
							// Gamma < 1
							nGamma = 1.0 / (1.0 + ((10001.0 - nPos) / 10000.0));
						}
						_stprintf(szText, _T("%0.2f"), nGamma);
						SendDlgItemMessage(hDlg, IDC_GAMMA_EDIT, WM_SETTEXT, 0, (LPARAM)szText);

						// Update the screen
						ComputeGammaLUT();
						if (bVidOkay)
						{
							VidRedraw();
							VidPaint(0);
						}
					}
					break;
				}
			}
			break;
		}

	case WM_CLOSE:
		if (nExitStatus != 1)
		{
			nGamma = nPrevGamma;
		}
		EndDialog(hDlg, 0);
	}

	return 0;
}

void GammaDialog()
{
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_GAMMA), hScrnWnd, GammaProc);
}

// -----------------------------------------------------------------------------
// Scanline intensity dialog

static INT_PTR CALLBACK ScanlineProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM) // LPARAM lParam
{
	static int nPrevIntensity;

	switch (Msg)
	{
	case WM_INITDIALOG:
		{
			TCHAR szText[16];
			nPrevIntensity = nVidScanIntensity;
			nExitStatus = 0;

			WndInMid(hDlg, hScrnWnd);

			// Initialise slider
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETRANGE, 0, MAKELONG(0, 255));
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETPAGESIZE, 0, 16);
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETTIC, 0, 191);
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETTIC, 0, 127);
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETTIC, 0, 63);

			// Set slider to current value
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETPOS, true,
			                   static_cast<LPARAM>(nVidScanIntensity) & 0xFF);

			// Set the edit control to current value
			_stprintf(szText, _T("%i"), nVidScanIntensity & 0xFF);
			SendDlgItemMessage(hDlg, IDC_SCANLINE_EDIT, WM_SETTEXT, 0, (LPARAM)szText);

			// Update the screen
			if (bDrvOkay)
			{
				VidPaint(2);
			}

			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				{
					if (LOWORD(wParam) == IDOK)
					{
						nExitStatus = 1;
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					if (LOWORD(wParam) == IDCANCEL)
					{
						nExitStatus = -1;
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					break;
				}
			case EN_UPDATE:
				{
					if (nExitStatus == 0)
					{
						TCHAR szText[16] = _T("");
						bool bValid = true;

						if (SendDlgItemMessage(hDlg, IDC_SCANLINE_EDIT, WM_GETTEXTLENGTH, 0, 0) < 16)
						{
							SendDlgItemMessage(hDlg, IDC_SCANLINE_EDIT, WM_GETTEXT, 16, (LPARAM)szText);
						}

						// Scan string in the edit control for illegal characters
						for (int i = 0; szText[i]; i++)
						{
							if (!_istdigit(szText[i]))
							{
								bValid = false;
								break;
							}
						}

						if (bValid)
						{
							nVidScanIntensity = _tcstol(szText, nullptr, 0);
							if (nVidScanIntensity < 0)
							{
								nVidScanIntensity = 0;
							}
							else
							{
								if (nVidScanIntensity > 255)
								{
									nVidScanIntensity = 255;
								}
							}

							// Set slider to current value
							SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETPOS, true, nVidScanIntensity);

							nVidScanIntensity |= (nVidScanIntensity << 8) | (nVidScanIntensity << 16);

							// Update the screen
							if (bVidOkay)
							{
								VidPaint(2);
							}
						}
					}
					break;
				}
			}
			break;
		}

	case WM_HSCROLL:
		{
			switch (LOWORD(wParam))
			{
			case TB_BOTTOM:
			case TB_ENDTRACK:
			case TB_LINEDOWN:
			case TB_LINEUP:
			case TB_PAGEDOWN:
			case TB_PAGEUP:
			case TB_THUMBPOSITION:
			case TB_THUMBTRACK:
			case TB_TOP:
				{
					if (nExitStatus == 0)
					{
						TCHAR szText[16];

						// Update the contents of the edit control
						nVidScanIntensity = SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_GETPOS, 0, 0);
						_stprintf(szText, _T("%i"), nVidScanIntensity);
						SendDlgItemMessage(hDlg, IDC_SCANLINE_EDIT, WM_SETTEXT, 0, (LPARAM)szText);

						nVidScanIntensity |= (nVidScanIntensity << 8) | (nVidScanIntensity << 16);
						// Update the screen
						if (bVidOkay)
						{
							//							VidRedraw();
							VidPaint(2);
						}
					}
					break;
				}
			}
			break;
		}

	case WM_CLOSE:
		if (nExitStatus != 1)
		{
			nVidScanIntensity = nPrevIntensity;
		}
		EndDialog(hDlg, 0);
		break;
	}

	return 0;
}

void ScanlineDialog()
{
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_SCANLINE), hScrnWnd, ScanlineProc);
}

// -----------------------------------------------------------------------------
// Feedback intensity dialog
static INT_PTR CALLBACK PhosphorProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam) // LPARAM lParam
{
	static int nPrevIntensity;
	static int nPrevSaturation;

	switch (Msg)
	{
	case WM_INITDIALOG:
		{
			TCHAR szText[16];
			nPrevIntensity = nVidFeedbackIntensity;
			nPrevSaturation = nVidFeedbackOverSaturation;
			nExitStatus = 0;

			WndInMid(hDlg, hScrnWnd);

			// Initialise sliders
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_SLIDER, TBM_SETRANGE, 0, MAKELONG(0, 255));
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_SLIDER, TBM_SETPAGESIZE, 0, 8);
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_SLIDER, TBM_SETTIC, 0, 127);
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_SLIDER, TBM_SETTIC, 0, 63);
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_SLIDER, TBM_SETTIC, 0, 31);
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_SLIDER, TBM_SETTIC, 0, 15);

			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_2_SLIDER, TBM_SETRANGE, 0, MAKELONG(0, 127));
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_2_SLIDER, TBM_SETPAGESIZE, 0, 4);
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_2_SLIDER, TBM_SETTIC, 0, 63);
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_2_SLIDER, TBM_SETTIC, 0, 31);
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_2_SLIDER, TBM_SETTIC, 0, 15);

			// Set slider to current values
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_SLIDER, TBM_SETPOS, true, nVidFeedbackIntensity);
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_2_SLIDER, TBM_SETPOS, true, nVidFeedbackOverSaturation);

			// Set the edit control to current values
			_stprintf(szText, _T("%i"), nVidFeedbackIntensity);
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_EDIT, WM_SETTEXT, 0, (LPARAM)szText);
			_stprintf(szText, _T("%i"), nVidFeedbackOverSaturation);
			SendDlgItemMessage(hDlg, IDC_PHOSPHOR_2_EDIT, WM_SETTEXT, 0, (LPARAM)szText);

			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				{
					if (LOWORD(wParam) == IDOK)
					{
						nExitStatus = 1;
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					if (LOWORD(wParam) == IDCANCEL)
					{
						nExitStatus = -1;
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					break;
				}
			case EN_UPDATE:
				{
					if (nExitStatus == 0)
					{
						TCHAR szText[16] = _T("");
						bool bValid = true;

						switch (LOWORD(wParam))
						{
						case IDC_PHOSPHOR_1_EDIT:
							if (SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_EDIT, WM_GETTEXTLENGTH, 0, 0) < 16)
							{
								SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_EDIT, WM_GETTEXT, 16, (LPARAM)szText);
							}

						// Scan string in the edit control for illegal characters
							for (int i = 0; szText[i]; i++)
							{
								if (!_istdigit(szText[i]))
								{
									bValid = false;
									break;
								}
							}

							if (bValid)
							{
								nVidFeedbackIntensity = _tcstol(szText, nullptr, 0);
								if (nVidFeedbackIntensity < 0)
								{
									nVidFeedbackIntensity = 0;
								}
								else
								{
									if (nVidFeedbackIntensity > 255)
									{
										nVidFeedbackIntensity = 255;
									}
								}

								// Set slider to current value
								SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_SLIDER, TBM_SETPOS, true,
								                   nVidFeedbackIntensity);
							}
							break;
						case IDC_PHOSPHOR_2_EDIT:
							if (SendDlgItemMessage(hDlg, IDC_PHOSPHOR_2_EDIT, WM_GETTEXTLENGTH, 0, 0) < 16)
							{
								SendDlgItemMessage(hDlg, IDC_PHOSPHOR_2_EDIT, WM_GETTEXT, 16, (LPARAM)szText);
							}

						// Scan string in the edit control for illegal characters
							for (int i = 0; szText[i]; i++)
							{
								if (!_istdigit(szText[i]))
								{
									bValid = false;
									break;
								}
							}

							if (bValid)
							{
								nVidFeedbackOverSaturation = _tcstol(szText, nullptr, 0);
								if (nVidFeedbackOverSaturation < 0)
								{
									nVidFeedbackOverSaturation = 0;
								}
								else
								{
									if (nVidFeedbackOverSaturation > 255)
									{
										nVidFeedbackOverSaturation = 255;
									}
								}

								// Set slider to current value
								SendDlgItemMessage(hDlg, IDC_PHOSPHOR_2_SLIDER, TBM_SETPOS, true,
								                   nVidFeedbackOverSaturation);

								// Update the screen
								if (bVidOkay)
								{
									VidPaint(2);
								}
							}
							break;
						}
					}
					break;
				}
			}
			break;
		}

	case WM_HSCROLL:
		{
			switch (LOWORD(wParam))
			{
			case TB_BOTTOM:
			case TB_ENDTRACK:
			case TB_LINEDOWN:
			case TB_LINEUP:
			case TB_PAGEDOWN:
			case TB_PAGEUP:
			case TB_THUMBPOSITION:
			case TB_THUMBTRACK:
			case TB_TOP:
				{
					if (nExitStatus == 0)
					{
						TCHAR szText[16];

						// Update the contents of the edit control
						switch (GetDlgCtrlID((HWND)lParam))
						{
						case IDC_PHOSPHOR_1_SLIDER:
							nVidFeedbackIntensity = SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_SLIDER, TBM_GETPOS, 0, 0);
							_stprintf(szText, _T("%i"), nVidFeedbackIntensity);
							SendDlgItemMessage(hDlg, IDC_PHOSPHOR_1_EDIT, WM_SETTEXT, 0, (LPARAM)szText);
							break;
						case IDC_PHOSPHOR_2_SLIDER:
							nVidFeedbackOverSaturation = SendDlgItemMessage(
								hDlg, IDC_PHOSPHOR_2_SLIDER, TBM_GETPOS, 0, 0);
							_stprintf(szText, _T("%i"), nVidFeedbackOverSaturation);
							SendDlgItemMessage(hDlg, IDC_PHOSPHOR_2_EDIT, WM_SETTEXT, 0, (LPARAM)szText);
							break;
						}
					}
					break;
				}
			}
			break;
		}

	case WM_CLOSE:
		if (nExitStatus != 1)
		{
			nVidFeedbackIntensity = nPrevIntensity;
			nVidFeedbackOverSaturation = nPrevSaturation;
		}
		EndDialog(hDlg, 0);
		break;
	}

	return 0;
}

void PhosphorDialog()
{
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_PHOSPHOR), hScrnWnd, PhosphorProc);
}

// -----------------------------------------------------------------------------
// Screen Angle dialog
static INT_PTR CALLBACK ScreenAngleProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static float fPrevScreenAngle, fPrevScreenCurvature;
	static HWND hScreenAngleSlider, hScreenAngleEdit;

	switch (Msg)
	{
	case WM_INITDIALOG:
		{
			TCHAR szText[16];
			fPrevScreenAngle = fVidScreenAngle;
			fPrevScreenCurvature = fVidScreenCurvature;
			hScreenAngleSlider = GetDlgItem(hDlg, IDC_SCREENANGLE_SLIDER);
			hScreenAngleEdit = GetDlgItem(hDlg, IDC_SCREENANGLE_EDIT);
			nExitStatus = 0;

			WndInMid(hDlg, hScrnWnd);

			// Initialise sliders
			SendDlgItemMessage(hDlg, IDC_SCREENANGLE_SLIDER, TBM_SETRANGE, 0, MAKELONG(0, 12000));
			SendDlgItemMessage(hDlg, IDC_SCREENANGLE_SLIDER, TBM_SETLINESIZE, 0, 100);
			SendDlgItemMessage(hDlg, IDC_SCREENANGLE_SLIDER, TBM_SETPAGESIZE, 0, 250);
			SendDlgItemMessage(hDlg, IDC_SCREENANGLE_SLIDER, TBM_SETTIC, 0, 1000);
			SendDlgItemMessage(hDlg, IDC_SCREENANGLE_SLIDER, TBM_SETTIC, 0, 2250);
			SendDlgItemMessage(hDlg, IDC_SCREENANGLE_SLIDER, TBM_SETTIC, 0, 6000);

			SendDlgItemMessage(hDlg, IDC_SCREENCURVATURE_SLIDER, TBM_SETRANGE, 0, MAKELONG(0, 12000));
			SendDlgItemMessage(hDlg, IDC_SCREENCURVATURE_SLIDER, TBM_SETLINESIZE, 0, 150);
			SendDlgItemMessage(hDlg, IDC_SCREENCURVATURE_SLIDER, TBM_SETPAGESIZE, 0, 375);
			SendDlgItemMessage(hDlg, IDC_SCREENCURVATURE_SLIDER, TBM_SETTIC, 0, 3000);
			SendDlgItemMessage(hDlg, IDC_SCREENCURVATURE_SLIDER, TBM_SETTIC, 0, 4500);
			SendDlgItemMessage(hDlg, IDC_SCREENCURVATURE_SLIDER, TBM_SETTIC, 0, 6000);

			// Set sliders to current value
			SendDlgItemMessage(hDlg, IDC_SCREENANGLE_SLIDER, TBM_SETPOS, true,
			                   static_cast<LPARAM>(fVidScreenAngle * RADTODEG * 100.0f));
			SendDlgItemMessage(hDlg, IDC_SCREENCURVATURE_SLIDER, TBM_SETPOS, true,
			                   static_cast<LPARAM>(fVidScreenCurvature * RADTODEG * 150.0f));

			// Set the edit controls to current value
			_stprintf(szText, _T("%0.2f"), fVidScreenAngle * RADTODEG);
			SendDlgItemMessage(hDlg, IDC_SCREENANGLE_EDIT, WM_SETTEXT, 0, (LPARAM)szText);

			_stprintf(szText, _T("%0.2f"), fVidScreenCurvature * RADTODEG);
			SendDlgItemMessage(hDlg, IDC_SCREENCURVATURE_EDIT, WM_SETTEXT, 0, (LPARAM)szText);

			// Update the screen
			if (bVidOkay)
			{
				VidPaint(2);
			}

			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				{
					if (LOWORD(wParam) == IDOK)
					{
						nExitStatus = 1;
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					if (LOWORD(wParam) == IDCANCEL)
					{
						nExitStatus = -1;
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					break;
				}
			case EN_UPDATE:
				{
					if (nExitStatus == 0)
					{
						TCHAR szText[16] = _T("");
						bool bPoint = false;
						bool bValid = true;

						if ((HWND)lParam == hScreenAngleEdit)
						{
							if (SendDlgItemMessage(hDlg, IDC_SCREENANGLE_EDIT, WM_GETTEXTLENGTH, 0, 0) < 16)
							{
								SendDlgItemMessage(hDlg, IDC_SCREENANGLE_EDIT, WM_GETTEXT, 16, (LPARAM)szText);
							}
						}
						else
						{
							if (SendDlgItemMessage(hDlg, IDC_SCREENCURVATURE_EDIT, WM_GETTEXTLENGTH, 0, 0) < 16)
							{
								SendDlgItemMessage(hDlg, IDC_SCREENCURVATURE_EDIT, WM_GETTEXT, 16, (LPARAM)szText);
							}
						}

						// Scan string in the edit control for illegal characters
						for (int i = 0; szText[i]; i++)
						{
							if (szText[i] == '.')
							{
								if (bPoint)
								{
									bValid = false;
									break;
								}
							}
							else
							{
								if (!_istdigit(szText[i]))
								{
									bValid = false;
									break;
								}
							}
						}

						if (bValid)
						{
							if ((HWND)lParam == hScreenAngleEdit)
							{
								fVidScreenAngle = _tcstod(szText, nullptr) * DEGTORAD;
								if (fVidScreenAngle < 0.0f)
								{
									fVidScreenAngle = 0.0f;
								}
								else
								{
									if (fVidScreenAngle > PI / 1.5f)
									{
										fVidScreenAngle = PI / 1.5f;
									}
								}

								// Set slider to current value
								SendDlgItemMessage(hDlg, IDC_SCREENANGLE_SLIDER, TBM_SETPOS, true,
								                   static_cast<LPARAM>(fVidScreenAngle * RADTODEG * 100.0f));
							}
							else
							{
								fVidScreenCurvature = _tcstod(szText, nullptr) * DEGTORAD;
								if (fVidScreenCurvature < 0.0f)
								{
									fVidScreenCurvature = 0.0f;
								}
								else
								{
									if (fVidScreenCurvature > PI / 2.25f)
									{
										fVidScreenCurvature = PI / 2.25f;
									}
								}

								// Set slider to current value
								SendDlgItemMessage(hDlg, IDC_SCREENCURVATURE_SLIDER, TBM_SETPOS, true,
								                   static_cast<LPARAM>(fVidScreenCurvature * RADTODEG * 150.0f));
							}

							// Update the screen
							if (bVidOkay)
							{
								VidPaint(2);
							}
						}
					}
					break;
				}
			}
			break;
		}

	case WM_HSCROLL:
		{
			switch (LOWORD(wParam))
			{
			case TB_BOTTOM:
			case TB_ENDTRACK:
			case TB_LINEDOWN:
			case TB_LINEUP:
			case TB_PAGEDOWN:
			case TB_PAGEUP:
			case TB_THUMBPOSITION:
			case TB_THUMBTRACK:
			case TB_TOP:
				{
					if (nExitStatus == 0)
					{
						TCHAR szText[16];

						// Update the contents of the edit control
						if ((HWND)lParam == hScreenAngleSlider)
						{
							fVidScreenAngle = static_cast<float>(SendDlgItemMessage(
								hDlg, IDC_SCREENANGLE_SLIDER, TBM_GETPOS, 0, 0)) * DEGTORAD / 100.0f;
							_stprintf(szText, _T("%0.2f"), fVidScreenAngle * RADTODEG);
							SendDlgItemMessage(hDlg, IDC_SCREENANGLE_EDIT, WM_SETTEXT, 0, (LPARAM)szText);
						}
						else
						{
							fVidScreenCurvature = static_cast<float>(SendDlgItemMessage(
								hDlg, IDC_SCREENCURVATURE_SLIDER, TBM_GETPOS, 0,
								0)) * DEGTORAD / 150.0f;
							_stprintf(szText, _T("%0.2f"), fVidScreenCurvature * RADTODEG);
							SendDlgItemMessage(hDlg, IDC_SCREENCURVATURE_EDIT, WM_SETTEXT, 0, (LPARAM)szText);
						}

						// Update the screen
						if (bVidOkay)
						{
							VidPaint(0);
						}
					}
					break;
				}
			}
			break;
		}

	case WM_CLOSE:
		if (nExitStatus != 1)
		{
			fVidScreenAngle = fPrevScreenAngle;
			fVidScreenCurvature = fPrevScreenCurvature;
		}
		EndDialog(hDlg, 0);
		break;
	}

	return 0;
}

void ScreenAngleDialog()
{
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_SCREENANGLE), hScrnWnd, ScreenAngleProc);
}

// -----------------------------------------------------------------------------
// CPU clock dialog

static INT_PTR CALLBACK CPUClockProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM) // LPARAM lParam
{
	switch (Msg)
	{
	case WM_INITDIALOG:
		{
			TCHAR szText[16];
			nExitStatus = 0;

			WndInMid(hDlg, hScrnWnd);

			// Initialise slider
			SendDlgItemMessage(hDlg, IDC_CPUCLOCK_SLIDER, TBM_SETRANGE, 0, MAKELONG(0x80, 0x0200));
			SendDlgItemMessage(hDlg, IDC_CPUCLOCK_SLIDER, TBM_SETLINESIZE, 0, 0x05);
			SendDlgItemMessage(hDlg, IDC_CPUCLOCK_SLIDER, TBM_SETPAGESIZE, 0, 0x10);
			SendDlgItemMessage(hDlg, IDC_CPUCLOCK_SLIDER, TBM_SETTIC, 0, 0x0100);
			SendDlgItemMessage(hDlg, IDC_CPUCLOCK_SLIDER, TBM_SETTIC, 0, 0x0120);
			SendDlgItemMessage(hDlg, IDC_CPUCLOCK_SLIDER, TBM_SETTIC, 0, 0x0140);
			SendDlgItemMessage(hDlg, IDC_CPUCLOCK_SLIDER, TBM_SETTIC, 0, 0x0180);

			// Set slider to current value
			SendDlgItemMessage(hDlg, IDC_CPUCLOCK_SLIDER, TBM_SETPOS, true, nBurnCPUSpeedAdjust);

			// Set the edit control to current value
			_stprintf(szText, _T("%i"), static_cast<int>(static_cast<double>(nBurnCPUSpeedAdjust) * 100 / 256 + 0.5));
			SendDlgItemMessage(hDlg, IDC_CPUCLOCK_EDIT, WM_SETTEXT, 0, (LPARAM)szText);

			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				{
					if (LOWORD(wParam) == IDOK)
					{
						nExitStatus = 1;
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					if (LOWORD(wParam) == IDCANCEL)
					{
						nExitStatus = -1;
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					break;
				}
			case EN_UPDATE:
				{
					if (nExitStatus == 0)
					{
						TCHAR szText[16] = _T("");
						bool bValid = true;
						int nValue;

						if (SendDlgItemMessage(hDlg, IDC_CPUCLOCK_EDIT, WM_GETTEXTLENGTH, 0, 0) < 16)
						{
							SendDlgItemMessage(hDlg, IDC_CPUCLOCK_EDIT, WM_GETTEXT, 16, (LPARAM)szText);
						}

						// Scan string in the edit control for illegal characters
						for (int i = 0; szText[i]; i++)
						{
							if (!_istdigit(szText[i]))
							{
								bValid = false;
								break;
							}
						}

						if (bValid)
						{
							nValue = _tcstol(szText, nullptr, 0);
							if (nValue < 1)
							{
								nValue = 1;
							}
							else
							{
								if (nValue > 400)
								{
									nValue = 400;
								}
							}

							nValue = static_cast<int>(static_cast<double>(nValue) * 256.0 / 100.0 + 0.5);

							// Set slider to current value
							SendDlgItemMessage(hDlg, IDC_CPUCLOCK_SLIDER, TBM_SETPOS, true, nValue);
						}
					}
					break;
				}
			}
			break;
		}

	case WM_HSCROLL:
		{
			switch (LOWORD(wParam))
			{
			case TB_BOTTOM:
			case TB_ENDTRACK:
			case TB_LINEDOWN:
			case TB_LINEUP:
			case TB_PAGEDOWN:
			case TB_PAGEUP:
			case TB_THUMBPOSITION:
			case TB_THUMBTRACK:
			case TB_TOP:
				{
					if (nExitStatus == 0)
					{
						TCHAR szText[16];
						int nValue;

						// Update the contents of the edit control
						nValue = SendDlgItemMessage(hDlg, IDC_CPUCLOCK_SLIDER, TBM_GETPOS, 0, 0);
						nValue = static_cast<int>(static_cast<double>(nValue) * 100.0 / 256.0 + 0.5);
						_stprintf(szText, _T("%i"), nValue);
						SendDlgItemMessage(hDlg, IDC_CPUCLOCK_EDIT, WM_SETTEXT, 0, (LPARAM)szText);
					}
					break;
				}
			}
			break;
		}

	case WM_CLOSE:
		if (nExitStatus == 1)
		{
			TCHAR szText[16] = _T("");

			SendDlgItemMessage(hDlg, IDC_CPUCLOCK_EDIT, WM_GETTEXT, 16, (LPARAM)szText);
			nBurnCPUSpeedAdjust = _tcstol(szText, nullptr, 0);
			if (nBurnCPUSpeedAdjust < 1)
			{
				nBurnCPUSpeedAdjust = 1;
			}
			else
			{
				if (nBurnCPUSpeedAdjust > 400)
				{
					nBurnCPUSpeedAdjust = 400;
				}
			}

			nBurnCPUSpeedAdjust = static_cast<int>(static_cast<double>(nBurnCPUSpeedAdjust) * 256.0 / 100.0 + 0.5);
		}
		EndDialog(hDlg, 0);
		break;
	}

	return 0;
}

void CPUClockDialog()
{
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_CPUCLOCK), hScrnWnd, CPUClockProc);
}

// -----------------------------------------------------------------------------
// Cubic filter quality dialog

static INT_PTR CALLBACK CubicProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM) // LPARAM lParam
{
	static double dPrevB, dPrevC;

	switch (Msg)
	{
	case WM_INITDIALOG:
		{
			TCHAR szText[16];
			dPrevB = dVidCubicB;
			dPrevC = dVidCubicC;
			nExitStatus = 0;

			WndInMid(hDlg, hScrnWnd);

			// Initialise slider
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETRANGE, 0, MAKELONG(0, 10000));
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETLINESIZE, 0, 100);
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETPAGESIZE, 0, 500);
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETTIC, 0, 3333);
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETTIC, 0, 5000);
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETTIC, 0, 7500);

			// Set slider to current value
			SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETPOS, true,
			                   static_cast<LPARAM>((1.0 - dVidCubicB) * 10000));

			// Set the edit control to current value
			_stprintf(szText, _T("%.3lf"), 1.0 - dVidCubicB);
			SendDlgItemMessage(hDlg, IDC_SCANLINE_EDIT, WM_SETTEXT, 0, (LPARAM)szText);

			SetWindowText(hDlg, FBALoadStringEx(hAppInst, IDS_NUMDIAL_FILTER_SHARP, true));

			// Update the screen
			if (bDrvOkay)
			{
				VidPaint(2);
			}

			return TRUE;
		}
	case WM_COMMAND:
		{
			switch (HIWORD(wParam))
			{
			case BN_CLICKED:
				{
					if (LOWORD(wParam) == IDOK)
					{
						nExitStatus = 1;
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					if (LOWORD(wParam) == IDCANCEL)
					{
						nExitStatus = -1;
						SendMessage(hDlg, WM_CLOSE, 0, 0);
					}
					break;
				}
			case EN_UPDATE:
				{
					if (nExitStatus == 0)
					{
						TCHAR szText[16] = _T("");
						bool bPoint = false;
						bool bValid = true;

						if (SendDlgItemMessage(hDlg, IDC_SCANLINE_EDIT, WM_GETTEXTLENGTH, 0, 0) < 16)
						{
							SendDlgItemMessage(hDlg, IDC_SCANLINE_EDIT, WM_GETTEXT, 16, (LPARAM)szText);
						}

						// Scan string in the edit control for illegal characters
						for (int i = 0; szText[i]; i++)
						{
							if (szText[i] == _T('.'))
							{
								if (bPoint)
								{
									bValid = false;
									break;
								}
							}
							else
							{
								if (!_istdigit(szText[i]))
								{
									bValid = false;
									break;
								}
							}
						}

						if (bValid)
						{
							dVidCubicB = 1.0 - _tcstod(szText, nullptr);
							if (dVidCubicB < 0.0)
							{
								dVidCubicB = 0.0;
							}
							else
							{
								if (dVidCubicB > 1.0)
								{
									dVidCubicB = 1.0;
								}
							}

							// Set slider to current value
							SendDlgItemMessage(hDlg, IDC_SCANLINE_SLIDER, TBM_SETPOS, true,
							                   static_cast<LPARAM>((1.0 - dVidCubicB) * 10000));

							// Update the screen
							if (bVidOkay)
							{
								VidPaint(2);
							}
						}
					}
					break;
				}
			}
			break;
		}

	case WM_HSCROLL:
		{
			switch (LOWORD(wParam))
			{
			case TB_BOTTOM:
			case TB_ENDTRACK:
			case TB_LINEDOWN:
			case TB_LINEUP:
			case TB_PAGEDOWN:
			case TB_PAGEUP:
			case TB_THUMBPOSITION:
			case TB_THUMBTRACK:
			case TB_TOP:
				{
					if (nExitStatus == 0)
					{
						TCHAR szText[16];

						// Update the contents of the edit control
						dVidCubicB = 1.0 - static_cast<double>(SendDlgItemMessage(
							hDlg, IDC_SCANLINE_SLIDER, TBM_GETPOS, 0, 0)) / 10000;
						_stprintf(szText, _T("%.3lf"), 1.0 - dVidCubicB);
						SendDlgItemMessage(hDlg, IDC_SCANLINE_EDIT, WM_SETTEXT, 0, (LPARAM)szText);

						// Update the screen
						if (bVidOkay)
						{
							//							VidRedraw();
							VidPaint(2);
						}
					}
					break;
				}
			}
			break;
		}

	case WM_CLOSE:
		if (nExitStatus != 1)
		{
			dVidCubicB = dPrevB;
			dVidCubicC = dPrevC;
		}
		EndDialog(hDlg, 0);
		break;
	}

	return 0;
}

void CubicSharpnessDialog()
{
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_SCANLINE), hScrnWnd, CubicProc);
}
