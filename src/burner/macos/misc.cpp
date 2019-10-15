#include "burner.h"

int bDrvSaveAll=0;
int nAppVirtualFps = 6000; // App fps * 100
bool bDoIpsPatch;
bool bRunPause=0;
int bDrvOkay = 0;
bool bAlwaysProcessKeyboardInput = 0;
TCHAR szAppHiscorePath[MAX_PATH];
TCHAR szAppSamplesPath[MAX_PATH];
TCHAR szAppHDDPath[MAX_PATH] = _T("support/hdd/");
TCHAR szAppBlendPath[MAX_PATH] = _T("support/blend/");
TCHAR szAppEEPROMPath[MAX_PATH] = _T("config/games/");
TCHAR szAppCheatsPath[MAX_PATH] = _T("support/cheats/");
INT32 nReplayStatus = 0; // 1 record, 2 replay, 0 nothing
INT32 nReplayUndoCount = 0;
UINT32 nReplayCurrentFrame = 0;
UINT32 nStartFrame = 0;
INT32 nIpsMaxFileLen = 0;

bool AppProcessKeyboardInput()
{
	return true;
}

void IpsApplyPatches(UINT8 *, char *)
{
}

TCHAR* GetIsoPath()
{
    return NULL;
}

void Reinitialise(void)
{
}

void wav_pause(bool bResume)
{
}

INT32 FreezeInput(UINT8** buf, INT32* size)
{
    return 0;
}

INT32 UnfreezeInput(const UINT8* buf, INT32 size)
{
    return 0;
}

INT32 is_netgame_or_recording() // returns: 1 = netgame, 2 = recording/playback
{
    return 0;
}

int DrvExit()
{
    if (bDrvOkay) {
        if (nBurnDrvSelect[0] < nBurnDrvCount) {
            ConfigGameSave(bSaveInputs);
            GameInpExit();                // Exit game input
            BurnDrvExit();                // Exit the driver
        }
    }

    BurnExtLoadRom = NULL;

    bDrvOkay = 0;                    // Stop using the BurnDrv functions
    nBurnDrvSelect[0] = ~0U;            // no driver selected

    return 0;
}

/* const */ TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int nOutSize)
{
#if defined (UNICODE)
	static TCHAR szStringBuffer[1024];

	TCHAR* pszBuffer = pszOutString ? pszOutString : szStringBuffer;
	int nBufferSize  = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

	if (MultiByteToWideChar(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize)) {
		return pszBuffer;
	}

	return NULL;
#else
	if (pszOutString) {
		_tcscpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (TCHAR*)pszInString;
#endif
}


/* const */ char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize)
{
#if defined (UNICODE)
	static char szStringBuffer[1024];
	memset(szStringBuffer, 0, sizeof(szStringBuffer));

	char* pszBuffer = pszOutString ? pszOutString : szStringBuffer;
	int nBufferSize = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

	if (WideCharToMultiByte(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize, NULL, NULL)) {
		return pszBuffer;
	}

	return NULL;
#else
	if (pszOutString) {
		_tcscpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (char*)pszInString;
#endif
}
