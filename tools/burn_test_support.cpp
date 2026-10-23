#include "burnint.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(BUILD_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "luaengine.h"
#endif

// The tests provide ROM bytes directly and do not initialize a frontend.
TCHAR szAppBlendPath[MAX_PATH] = _T("./");
TCHAR szAppEEPROMPath[MAX_PATH] = _T("./cps2plus_test_nvram/");
TCHAR szAppHiscorePath[MAX_PATH] = _T("./");
TCHAR szAppSamplesPath[MAX_PATH] = _T("./");
INT32 nInputIntfMouseDivider = 1;
bool bDrvOkay = false;
bool bDoIpsPatch = false;
UINT32 nIpsMemExpLen[SND2_ROM + 1] = { 0 };
RomDataInfo* pRDI = NULL;
BurnRomInfo* pDataRomDesc = NULL;

#if defined(BUILD_WIN32)
// Windows core builds also reference frontend replay, Lua and message services.
INT32 nReplayStatus = 0;
UINT32 nStartFrame = 0;
INT32 nReplayUndoCount = 0;
struct MovieExtInfo {
	UINT32 year, month;
	UINT16 day, dayofweek;
	UINT32 hour, minute, second;
};
MovieExtInfo MovieInfo = { 0, 0, 0, 0, 0, 0, 0 };

void CallRegisteredLuaMemHook(unsigned int, int, unsigned int, LuaMemHookType) { }
INT32 VidSNewShortMsg(const TCHAR*, INT32, INT32, INT32) { return 0; }
INT32 VidSNewTinyMsg(const TCHAR*, INT32, INT32, INT32) { return 0; }
INT32 FreezeInputSize() { abort(); }
INT32 FreezeInput(UINT8**, INT32*) { abort(); }
INT32 UnfreezeInput(const UINT8*, INT32) { abort(); }
INT32 inputbuf_freezer_size() { abort(); }
INT32 inputbuf_freeze(UINT8**, INT32*) { abort(); }
INT32 inputbuf_unfreeze(UINT8*, INT32) { abort(); }

INT32 _MultiByteToWideChar(UINT32 codepage, unsigned long flags, const char* text,
	INT32 length, wchar_t* output, INT32 capacity)
{
	return MultiByteToWideChar(codepage, flags, text, length, output, capacity);
}
#endif

INT32 is_netgame_or_recording() { return 0; }
void Reinitialise() { abort(); }
void IpsApplyPatches(UINT8*, char*, UINT32, bool) { abort(); }
INT32 ZipLoadOneFile(char*, const char*, void**, INT32*) { return 1; }
char* TCHARToANSI(const TCHAR* pszIn, char* pszOut, INT32 nOutSize)
{
	if (!pszOut || nOutSize < 1) return NULL;
#if defined(UNICODE) || defined(_UNICODE)
	if (wcstombs(pszOut, pszIn, nOutSize - 1) == (size_t)-1) return NULL;
	pszOut[nOutSize - 1] = 0;
#else
	snprintf(pszOut, nOutSize, "%s", pszIn);
#endif
	return pszOut;
}
