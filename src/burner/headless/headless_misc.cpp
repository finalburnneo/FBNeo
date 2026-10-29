// Headless frontend helpers for FinalBurn Neo.
// Author: Leonardo Kasperavičius
//
// Built originally to support OpenEmu-Silicon
// (https://github.com/OpenEmu-Silicon/OpenEmu-Silicon).
//
// Small frontend functions/globals the shared burner code expects, provided by
// each platform (the headless counterpart of burner/macos/misc.cpp). Subject to
// the FB Neo license (see src/license.txt).

#include "burner.h"

// General-purpose debug global referenced by various drivers.
int counter = 0;

TCHAR* ANSIToTCHAR(const char* pszInString, TCHAR* pszOutString, int /* nOutSize */)
{
	if (pszOutString) {
		_tcscpy(pszOutString, pszInString);
		return pszOutString;
	}
	return (TCHAR*)pszInString;
}

char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int /* nOutSize */)
{
	if (pszOutString) {
		_tcscpy(pszOutString, pszInString);
		return pszOutString;
	}
	return (char*)pszInString;
}

TCHAR* AdaptiveEncodingReads(const TCHAR* /* pszFileName */)
{
	return NULL;
}

bool AppProcessKeyboardInput()
{
	// Input is always live for the headless core (the host gates pause/focus),
	// and CinpState() zeroes keyboard-range codes (< 0x4000, which ours are)
	// unless this returns true.
	return true;
}

void UpdateMessage(char* /* message */)
{
}
