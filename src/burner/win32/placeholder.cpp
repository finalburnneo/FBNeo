#include "burner.h"

UINT uiBackGroundColor = 0xFFFFFF;
UINT uiMenuItemColor   = 0xf4f4f4;
UINT uiSelectedMenuItemColor = 0x9fc5e8;
UINT uiTextFontColor = 0x000000;
UINT nUiColorTheme = 0;


static void MakeOfn()
{
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hScrnWnd;
	ofn.lpstrFilter = _T("FB Alpha skin files (*.bmp,*.png)\0*.bmp;*.png\0\0)");
	ofn.lpstrFile = szChoice;
	ofn.nMaxFile = sizeof(szChoice) / sizeof(TCHAR);
	ofn.lpstrInitialDir = _T("");
	ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = _T("png");
	return;
}

int SelectPlaceHolder()
{
	int nRet;
	int bOldPause;

	MakeOfn();
	ofn.lpstrTitle = FBALoadStringEx(hAppInst, IDS_PLACEHOLDER_LOAD, true);

	bOldPause = bRunPause;
	bRunPause = 1;
	nRet = GetOpenFileName(&ofn);
	bRunPause = bOldPause;

	if (nRet == 0) {		// Error
		return 1;
	}

	szPlaceHolder[0] = _T('\0');
	memcpy(szPlaceHolder, szChoice, sizeof(szChoice));

	return nRet;
}

void ResetPlaceHolder()
{
	szPlaceHolder[0] = _T('\0');
}


void UpdateUiColorMode(UINT uiColorMode)
{
	if(uiColorMode == 0)
	{
		ApplyLightMode();
	}
	else
	{
		ApplyDarkMode();
	}
}

void ApplyLightMode()
{
	uiBackGroundColor = 0xFFFFFF;
	uiMenuItemColor   = 0xf4f4f4;
	uiTextFontColor = 0x000000;
}

void ApplyDarkMode()
{
	uiBackGroundColor = 0x262626;
	uiMenuItemColor   = 0x1a1a1a;
	uiTextFontColor   = 0xFFFFFF;
}