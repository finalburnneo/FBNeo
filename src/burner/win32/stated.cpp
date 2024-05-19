// State dialog module
#include "burner.h"
#include "neocdlist.h"

extern bool bReplayDontClose;
int bDrvSaveAll = 0;

static void MakeOfn(TCHAR* pszFilter)
{
	_stprintf(pszFilter, FBALoadStringEx(hAppInst, IDS_DISK_FILE_STATE, true), _T(APP_TITLE));
	memcpy(pszFilter + _tcslen(pszFilter), _T(" (*.fs, *.fr)\0*.fs;*.fr\0\0"), 25 * sizeof(TCHAR));

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hScrnWnd;
	ofn.lpstrFilter = pszFilter;
	ofn.lpstrFile = szChoice;
	ofn.nMaxFile = sizeof(szChoice) / sizeof(TCHAR);
	ofn.lpstrInitialDir = _T(".\\savestates");
	ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = _T("fs");
	return;
}

// The automatic save (nvram or nvram+state if restore state on load is enabled.)
int StatedAuto(int bSave)
{
	static TCHAR szName[MAX_PATH] = _T("");
	int nRet;

	if (NeoCDInfo_ID() && bDrvSaveAll != 0) {
		_stprintf(szName, _T("%sngcd_%s.fs"), szAppEEPROMPath, NeoCDInfo_Text(DRV_NAME));
	} else {
		_stprintf(szName, _T("%s%s.fs"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
	}

	if (bSave == 0) {
		nRet = BurnStateLoad(szName, bDrvSaveAll, NULL);		// Load ram
		if (nRet && bDrvSaveAll)	{
			nRet = BurnStateLoad(szName, 0, NULL);				// Couldn't get all - okay just try the nvram
		}
	} else {
		nRet = BurnStateSave(szName, bDrvSaveAll);				// Save ram
	}

	return nRet;
}

static void CreateStateName(int nSlot)
{
	if (NeoCDInfo_ID()) {
		_stprintf(szChoice, _T("./savestates/ngcd_%s slot %02x.fs"), NeoCDInfo_Text(DRV_NAME), nSlot);
	} else {
		_stprintf(szChoice, _T("./savestates/%s slot %02x.fs"), BurnDrvGetText(DRV_NAME), nSlot);
	}
}

int StatedUNDO(int nSlot)
{
	if (nSlot) {
		CreateStateName(nSlot);
		return BurnStateUNDO(szChoice);
	}
	return 1;
}

int StatedLoad(int nSlot)
{
	TCHAR szFilter[1024];
	int nRet;
	int bOldPause;

	// if rewinding during playback, and readonly is not set,
	// then transition from decoding to encoding (recording)
	if(!bReplayReadOnly && nReplayStatus == 2)
	{
		nReplayStatus = 1;
	}
	if(bReplayReadOnly && nReplayStatus == 1)
	{
		bReplayDontClose = 1;
		StopReplay();
		nReplayStatus = 2;
	}

	if (nSlot) {
		CreateStateName(nSlot);
	} else {
		if (bDrvOkay) {
			if (NeoCDInfo_ID()) {
				_stprintf(szChoice, _T("ngcd_%s*.fs"), NeoCDInfo_Text(DRV_NAME));
			} else {
				_stprintf(szChoice, _T("%s*.fs"), BurnDrvGetText(DRV_NAME));
			}
		} else {
			_stprintf(szChoice, _T("savestate"));
		}
		MakeOfn(szFilter);
		ofn.lpstrTitle = FBALoadStringEx(hAppInst, IDS_STATE_LOAD, true);

		bOldPause = bRunPause;
		bRunPause = 1;
		nRet = GetOpenFileName(&ofn);
		bRunPause = bOldPause;

		if (nRet == 0) {		// Error
			return 1;
		}
	}

	nRet = BurnStateLoad(szChoice, 1, &DrvInitCallback);

	if (nSlot) {
		return nRet;
	}

	// Describe any possible errors:
	if (nRet == 3) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_THIS_STATE));
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_UNAVAIL));
	} else {
		if (nRet == 4) {
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_THIS_STATE));
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_TOOOLD), _T(APP_TITLE));
		} else {
			if (nRet == 5) {
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_THIS_STATE));
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_TOONEW), _T(APP_TITLE));
			} else {
				if (nRet && !nSlot) {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_LOAD));
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_STATE));
				}
			}
		}
	}

	if (nRet) {
		FBAPopupDisplay(PUF_TYPE_ERROR);
	}

	return nRet;
}

int StatedSave(int nSlot)
{
	TCHAR szFilter[1024];
	int nRet;
	int bOldPause;

	if (bDrvOkay == 0) {
		return 1;
	}

	if (nSlot) {
		CreateStateName(nSlot);
	} else {
		if (NeoCDInfo_ID()) {
			_stprintf(szChoice, _T("ngcd_%s"), NeoCDInfo_Text(DRV_NAME));
		} else {
			_stprintf(szChoice, _T("%s"), BurnDrvGetText(DRV_NAME));
		}
		MakeOfn(szFilter);
		ofn.lpstrTitle = FBALoadStringEx(hAppInst, IDS_STATE_SAVE, true);
		ofn.Flags |= OFN_OVERWRITEPROMPT;

		bOldPause = bRunPause;
		bRunPause = 1;
		nRet = GetSaveFileName(&ofn);
		bRunPause = bOldPause;

		if (nRet == 0) {		// Error
			return 1;
		}
	}

	nRet = BurnStateSave(szChoice, 1);

	if (nRet && !nSlot) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_CREATE));
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_STATE));
		FBAPopupDisplay(PUF_TYPE_ERROR);
	}

	return nRet;
}
