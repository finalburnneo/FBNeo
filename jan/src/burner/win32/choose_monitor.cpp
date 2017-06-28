#include "burner.h"

#define MAX_MONITORS	8

static TCHAR *MonitorIDs[MAX_MONITORS];

static INT_PTR CALLBACK ResProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM)
{
	static bool bOK;

	switch (Msg) {
		case WM_INITDIALOG: {
			int i;
			int nHorSelected = 0, nVerSelected = 0;
			
			for (i = 0; i < MAX_MONITORS; i++) {
				MonitorIDs[i] = (TCHAR*)malloc(32 * sizeof(TCHAR));
				memset(MonitorIDs[i], 0, 32 * sizeof(TCHAR));
			}
			
			DISPLAY_DEVICE dd;
			ZeroMemory(&dd, sizeof(dd));
			dd.cb = sizeof(dd);
			
			for (i = 0; EnumDisplayDevices(NULL, i, &dd, 0); i++) {
				if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
					DISPLAY_DEVICE dd2;
					ZeroMemory(&dd2, sizeof(dd2));
					dd2.cb = sizeof(dd2);
					EnumDisplayDevices(dd.DeviceName, 0, &dd2, 0);
					
					// Add to dropdowns
					TCHAR szTemp[256];
					_stprintf(szTemp, _T("%s"), dd2.DeviceString);
					if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) _stprintf(szTemp, _T("%s (default)"), szTemp);
					
					SendDlgItemMessage(hDlg, IDC_CHOOSE_MONITOR_HOR_LIST, CB_ADDSTRING, 0, (LPARAM)&szTemp);
					SendDlgItemMessage(hDlg, IDC_CHOOSE_MONITOR_VER_LIST, CB_ADDSTRING, 0, (LPARAM)&szTemp);
					
					// Store id for later
					_stprintf(MonitorIDs[i], _T("%s"), dd.DeviceName);
					
					// Select if this is our value or the default
					if (nHorSelected == 0) {
						if (!_tcsicmp(HorScreen, MonitorIDs[i])) {
							nHorSelected = 1;
							SendDlgItemMessage(hDlg, IDC_CHOOSE_MONITOR_HOR_LIST, CB_SETCURSEL, i, 0);
						} else {
							if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) SendDlgItemMessage(hDlg, IDC_CHOOSE_MONITOR_HOR_LIST, CB_SETCURSEL, i, 0);
						}
					}
					
					if (nVerSelected == 0) {
						if (!_tcsicmp(VerScreen, MonitorIDs[i])) {
							nVerSelected = 1;
							SendDlgItemMessage(hDlg, IDC_CHOOSE_MONITOR_VER_LIST, CB_SETCURSEL, i, 0);
						} else {
							if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) SendDlgItemMessage(hDlg, IDC_CHOOSE_MONITOR_VER_LIST, CB_SETCURSEL, i, 0);
						}
					}
				}
			}
			
			WndInMid(hDlg, hScrnWnd);
			SetFocus(hDlg);											// Enable Esc=close
			break;
		}
		
		case WM_COMMAND: {
			if (LOWORD(wParam) == IDOK) {
				bOK = 1;
				SendMessage(hDlg, WM_CLOSE, 0, 0);
			}
			if (LOWORD(wParam) == IDCANCEL) {
				SendMessage(hDlg, WM_CLOSE, 0, 0);
			}
			break;
		}
		
		case WM_CLOSE: {
			if (bOK) {
				// save selected values
				int nItem = (int)SendDlgItemMessage(hDlg, IDC_CHOOSE_MONITOR_HOR_LIST, CB_GETCURSEL, 0, 0);
				if (MonitorIDs[(int)nItem]) _stprintf(HorScreen, _T("%s"), MonitorIDs[(int)nItem]);
				
				nItem = (int)SendDlgItemMessage(hDlg, IDC_CHOOSE_MONITOR_VER_LIST, CB_GETCURSEL, 0, 0);
				if (MonitorIDs[(int)nItem]) _stprintf(VerScreen, _T("%s"), MonitorIDs[(int)nItem]);
			}
			EndDialog(hDlg, 0);
			break;
		}
	}
	
	return 0;
}

int ChooseMonitorCreate()
{
	FBADialogBox(hAppInst,MAKEINTRESOURCE(IDD_CHOOSEMONITOR),hScrnWnd,(DLGPROC)ResProc);
	return 0;
}
