
#include "burner.h"
#include "resource.h"

HWND LuaConsoleHWnd = NULL;
HFONT hFont = NULL;
LOGFONT LuaConsoleLogFont;

struct ControlLayoutInfo
{
	int controlID;
	
	enum LayoutType // what to do when the containing window resizes
	{
		NONE, // leave the control where it was
		RESIZE_END, // resize the control
		MOVE_START // move the control
	};
	LayoutType horizontalLayout;
	LayoutType verticalLayout;
};
struct ControlLayoutState
{
	int x,y,width,height;
	bool valid;
	ControlLayoutState() : valid(false) {}
};

static ControlLayoutInfo controlLayoutInfos [] = {
	{IDC_LUACONSOLE, ControlLayoutInfo::RESIZE_END, ControlLayoutInfo::RESIZE_END},
	{IDC_EDIT_LUAPATH, ControlLayoutInfo::RESIZE_END, ControlLayoutInfo::NONE},
	{IDC_BUTTON_LUARUN, ControlLayoutInfo::MOVE_START, ControlLayoutInfo::NONE},
	{IDC_BUTTON_LUASTOP, ControlLayoutInfo::MOVE_START, ControlLayoutInfo::NONE},
};
static const int numControlLayoutInfos = sizeof(controlLayoutInfos)/sizeof(*controlLayoutInfos);

struct WindowInfo {
	int width; int height;
	ControlLayoutState layoutState [numControlLayoutInfos];
} windowInfo;

void PrintToWindowConsole(INT64 hDlgAsInt, const char* str)
{
	HWND hDlg = (HWND)hDlgAsInt;
	HWND hConsole = GetDlgItem(hDlg, IDC_LUACONSOLE);

	int length = GetWindowTextLength(hConsole);
	if(length >= 250000)
	{
		// discard first half of text if it's getting too long
		SendMessage(hConsole, EM_SETSEL, 0, length/2);
		SendMessage(hConsole, EM_REPLACESEL, false, (LPARAM)"");
		length = GetWindowTextLength(hConsole);
	}
	SendMessage(hConsole, EM_SETSEL, length, length);

	//LuaPerWindowInfo& info = LuaWindowInfo[hDlg];

	{
		SendMessage(hConsole, EM_REPLACESEL, false, (LPARAM)_AtoT(str));
	}
}

void WinLuaOnStart(INT64 hDlgAsInt)
{
	HWND hDlg = (HWND)hDlgAsInt;
	//LuaPerWindowInfo& info = LuaWindowInfo[hDlg];
	//info.started = true;
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUABROWSE), false); // disable browse while running because it misbehaves if clicked in a frameadvance loop
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUASTOP), true);
	SetWindowTextA(GetDlgItem(hDlg, IDC_BUTTON_LUARUN), "Restart");
	SetWindowTextA(GetDlgItem(hDlg, IDC_LUACONSOLE), ""); // clear the console
//	Show_Genesis_Screen(HWnd); // otherwise we might never show the first thing the script draws
}

void WinLuaOnStop(INT64 hDlgAsInt)
{
	HWND hDlg = (HWND)hDlgAsInt;
	//LuaPerWindowInfo& info = LuaWindowInfo[hDlg];

	HWND prevWindow = GetActiveWindow();
	SetActiveWindow(hDlg); // bring to front among other script/secondary windows, since a stopped script will have some message for the user that would be easier to miss otherwise
	if(prevWindow == hScrnWnd) SetActiveWindow(prevWindow);

	//info.started = false;
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUABROWSE), true);
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_LUASTOP), false);
	SetWindowTextA(GetDlgItem(hDlg, IDC_BUTTON_LUARUN), "Run");
//	if(statusOK)
//		Show_Genesis_Screen(MainWindow->getHWnd()); // otherwise we might never show the last thing the script draws
	//if(info.closeOnStop)
	//	PostMessage(hDlg, WM_CLOSE, 0, 0);
}

INT_PTR CALLBACK DlgLuaScriptDialog(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	RECT r;
	RECT r2;
	int dx1, dy1, dx2, dy2;

	switch (msg) {

	case WM_INITDIALOG:
	{
		// remove the 30000 character limit from the console control
		SendMessage(GetDlgItem(hDlg, IDC_LUACONSOLE),EM_LIMITTEXT,0,0);

		GetWindowRect(hScrnWnd, &r);
		dx1 = (r.right - r.left) / 2;
		dy1 = (r.bottom - r.top) / 2;

		GetWindowRect(hDlg, &r2);
		dx2 = (r2.right - r2.left) / 2;
		dy2 = (r2.bottom - r2.top) / 2;

		int windowIndex = 0;//std::find(LuaScriptHWnds.begin(), LuaScriptHWnds.end(), hDlg) - LuaScriptHWnds.begin();
		int staggerOffset = windowIndex * 24;
		r.left += staggerOffset;
		r.right += staggerOffset;
		r.top += staggerOffset;
		r.bottom += staggerOffset;

		// push it away from the main window if we can
		const int width = (r.right-r.left); 
		const int width2 = (r2.right-r2.left); 
		if(r.left+width2 + width < GetSystemMetrics(SM_CXSCREEN))
		{
			r.right += width;
			r.left += width;
		}
		else if((int)r.left - (int)width2 > 0)
		{
			r.right -= width2;
			r.left -= width2;
		}

		SetWindowPos(hDlg, NULL, r.left, r.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_SHOWWINDOW);

		RECT r3;
		GetClientRect(hDlg, &r3);
		windowInfo.width = r3.right - r3.left;
		windowInfo.height = r3.bottom - r3.top;
		for(int i = 0; i < numControlLayoutInfos; i++) {
			ControlLayoutState& layoutState = windowInfo.layoutState[i];
			layoutState.valid = false;
		}

		DragAcceptFiles(hDlg, true);
		SetDlgItemTextA(hDlg, IDC_EDIT_LUAPATH, FBA_GetLuaScriptName());

		SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(LOGFONT), &LuaConsoleLogFont, 0); // reset with an acceptable font
		return true;
	}	break;

	case WM_SIZE:
	{
		// resize or move controls in the window as necessary when the window is resized

		//LuaPerWindowInfo& windowInfo = LuaWindowInfo[hDlg];
		int prevDlgWidth = windowInfo.width;
		int prevDlgHeight = windowInfo.height;

		int dlgWidth = LOWORD(lParam);
		int dlgHeight = HIWORD(lParam);

		int deltaWidth = dlgWidth - prevDlgWidth;
		int deltaHeight = dlgHeight - prevDlgHeight;

		for(int i = 0; i < numControlLayoutInfos; i++)
		{
			ControlLayoutInfo layoutInfo = controlLayoutInfos[i];
			ControlLayoutState& layoutState = windowInfo.layoutState[i];

			HWND hCtrl = GetDlgItem(hDlg,layoutInfo.controlID);

			int x,y,width,height;
			if(layoutState.valid)
			{
				x = layoutState.x;
				y = layoutState.y;
				width = layoutState.width;
				height = layoutState.height;
			}
			else
			{
				RECT rr;
				GetWindowRect(hCtrl, &rr);
				POINT p = {rr.left, rr.top};
				ScreenToClient(hDlg, &p);
				x = p.x;
				y = p.y;
				width = rr.right - rr.left;
				height = rr.bottom - rr.top;
			}

			switch(layoutInfo.horizontalLayout)
			{
				case ControlLayoutInfo::RESIZE_END: width += deltaWidth; break;
				case ControlLayoutInfo::MOVE_START: x += deltaWidth; break;
				default: break;
			}
			switch(layoutInfo.verticalLayout)
			{
				case ControlLayoutInfo::RESIZE_END: height += deltaHeight; break;
				case ControlLayoutInfo::MOVE_START: y += deltaHeight; break;
				default: break;
			}

			SetWindowPos(hCtrl, 0, x,y, width,height, 0);

			layoutState.x = x;
			layoutState.y = y;
			layoutState.width = width;
			layoutState.height = height;
			layoutState.valid = true;
		}

		windowInfo.width = dlgWidth;
		windowInfo.height = dlgHeight;

		RedrawWindow(hDlg, NULL, NULL, RDW_INVALIDATE);
	}	break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
			case IDOK:
			case IDCANCEL: {
				EndDialog(hDlg, true); // goto case WM_CLOSE;
			}	break;

			case IDC_BUTTON_LUARUN:
			{
				char filename[MAX_PATH];
				GetDlgItemTextA(hDlg, IDC_EDIT_LUAPATH, filename, MAX_PATH);
				FBA_LoadLuaCode(filename);
			}	break;

			case IDC_BUTTON_LUASTOP:
			{
				FBA_LuaStop();
			}	break;

			case IDC_BUTTON_LUAEDIT:
			{
				TCHAR temp[1024]; // shadow added because the global one is unreliable
				SendDlgItemMessage(hDlg,IDC_EDIT_LUAPATH,WM_GETTEXT,(WPARAM)512,(LPARAM)temp);
				// tell the OS to open the file with its associated editor,
				// without blocking on it or leaving a command window open.
				if((INT_PTR)ShellExecuteA(NULL, "edit", _TtoA(temp), NULL, NULL, SW_SHOWNORMAL) == SE_ERR_NOASSOC)
					if((INT_PTR)ShellExecuteA(NULL, "open", _TtoA(temp), NULL, NULL, SW_SHOWNORMAL) == SE_ERR_NOASSOC)
						ShellExecuteA(NULL, NULL, "notepad", _TtoA(temp), NULL, SW_SHOWNORMAL);
			}	break;

			case IDC_BUTTON_LUABROWSE:
			{
				TCHAR temp[1024];
				AudBlankSound();
				GetCurrentDirectory(1024, temp);
				OPENFILENAME  luaofn;
				TCHAR szFileName[MAX_PATH] = _T("\0");
				ZeroMemory( (LPVOID)&luaofn, sizeof(OPENFILENAME) );
				luaofn.lStructSize = sizeof(OPENFILENAME);
				luaofn.hwndOwner = hDlg;
				luaofn.lpstrFilter = _T("Lua scripts (*.lua)\0*.lua\0All files (*.*)\0*.*\0\0");
				luaofn.lpstrFile = szFileName;
				luaofn.lpstrDefExt = _T("lua");
				luaofn.nMaxFile = MAX_PATH;
				luaofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_EXPLORER; // hide previously-ignored read-only checkbox (the real read-only box is in the open-movie dialog itself)
				luaofn.lpstrInitialDir = _T(".\\support\\lua");
				if(GetOpenFileName( &luaofn ))
				{
					SetWindowText(GetDlgItem(hDlg, IDC_EDIT_LUAPATH), szFileName);
				}
				SetCurrentDirectory(temp);
				return true;
			}	break;

			case IDC_EDIT_LUAPATH:
			{
				char filename[MAX_PATH];
				GetDlgItemTextA(hDlg, IDC_EDIT_LUAPATH, filename, MAX_PATH);
				FILE* file = fopen(filename, "rb");
				EnableWindow(GetDlgItem(hDlg, IDOK), file != NULL);
				if(file)
					fclose(file);
			}	break;

			case IDC_LUACONSOLE_CHOOSEFONT:
			{
				CHOOSEFONT cf;

				ZeroMemory(&cf, sizeof(cf));
				cf.lStructSize = sizeof(CHOOSEFONT);
				cf.hwndOwner = hDlg;
				cf.lpLogFont = &LuaConsoleLogFont;
				cf.Flags = CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
				if (ChooseFont(&cf)) {
					if (hFont) {
						DeleteObject(hFont);
						hFont = NULL;
					}
					hFont = CreateFontIndirect(&LuaConsoleLogFont);
					if (hFont)
						SendDlgItemMessage(hDlg, IDC_LUACONSOLE, WM_SETFONT, (WPARAM)hFont, 0);
				}
			}	break;

			case IDC_LUACONSOLE_CLEAR:
			{
				SetWindowTextA(GetDlgItem(hDlg, IDC_LUACONSOLE), "");
			}	break;

		}
		break;

	case WM_CLOSE: {
		FBA_LuaStop();
		DragAcceptFiles(hDlg, FALSE);
		if (hFont) {
			DeleteObject(hFont);
			hFont = NULL;
		}
		LuaConsoleHWnd = NULL;
	}	break;

	case WM_DROPFILES: {
		HDROP hDrop;
		//UINT fileNo;
		UINT fileCount;
		WCHAR filename[MAX_PATH];

		hDrop = (HDROP)wParam;
		fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
		if (fileCount > 0) {
			DragQueryFile(hDrop, 0, filename, MAX_PATH);
			SetWindowTextW(GetDlgItem(hDlg, IDC_EDIT_LUAPATH), filename);
		}
		DragFinish(hDrop);
		return true;
	}	break;

	}

	return false;

}

void UpdateLuaConsole(const WCHAR* fname)
{
	if (!LuaConsoleHWnd) return;

	SetWindowText(GetDlgItem(LuaConsoleHWnd, IDC_EDIT_LUAPATH), fname);
}
