#include "burner.h"

static void MakeOfn()
{
	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hScrnWnd;
	ofn.lpstrFilter = _T("FB Neo skin files (*.bmp,*.png)\0*.bmp;*.png\0\0)");
	ofn.lpstrFile = szChoice;
	ofn.nMaxFile = sizeof(szChoice) / sizeof(TCHAR);
	ofn.lpstrInitialDir = _T("");
	ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = _T("png");
}

int SelectPlaceHolder()
{
	MakeOfn();
	ofn.lpstrTitle = FBNLoadStringEx(hAppInst, IDS_PLACEHOLDER_LOAD, true);

	int bOldPause = bRunPause;
	bRunPause = 1;
	int nRet = GetOpenFileName(&ofn);
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
