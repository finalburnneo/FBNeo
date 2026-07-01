// Burner cheat-file loader
#include "burner.h"
#include "neocdlist.h"

// GameGenie stuff is handled a little differently..
#define HW_NES ( ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_NES) || ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_FDS) )
#define HW_SNES ( ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SNES) )
#define HW_GGENIE ( HW_NES || HW_SNES )

static CheatInfo* pCurrentCheat = NULL;
static CheatInfo* pPreviousCheat = NULL;

static bool SkipComma(TCHAR** s)
{
	while (**s && **s != _T(',')) {
		(*s)++;
	}

	if (**s == _T(',')) {
		(*s)++;
	}

	if (**s) {
		return true;
	}

	return false;
}

static void CheatError(TCHAR* pszFilename, INT32 nLineNumber, CheatInfo* pCheat, TCHAR* pszInfo, TCHAR* pszLine)
{
#if defined (BUILD_WIN32)
	FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("Cheat file %s is malformed.\nPlease remove or repair the file.\n\n"), pszFilename);
	if (pCheat) {
		FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("Parse error at line %i, in cheat \"%s\".\n"), nLineNumber, pCheat->szCheatName);
	} else {
		FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("Parse error at line %i.\n"), nLineNumber);
	}

	if (pszInfo) {
		FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("Problem:\t%s.\n"), pszInfo);
	}
	if (pszLine) {
		FBAPopupAddText(PUF_TEXT_NO_TRANSLATE, _T("Text:\t%s\n"), pszLine);
	}

	FBAPopupDisplay(PUF_TYPE_ERROR);
#endif

#if defined(BUILD_SDL2)
	printf("Cheat file %s is malformed.\nPlease remove or repair the file.\n\n", pszFilename);
	if (pCheat) {
		printf("Parse error at line %i, in cheat \"%s\".\n", nLineNumber, pCheat->szCheatName);
	} else {
		printf("Parse error at line %i.\n", nLineNumber);
	}

	if (pszInfo) {
		printf("Problem:\t%s.\n", pszInfo);
	}
	if (pszLine) {
		printf("Text:\t%s\n", pszLine);
	}
#endif
}

static TCHAR* getFilenameFromPath(TCHAR* path) {
	if (!path) {
		return NULL;
	}

    TCHAR* filename = path;

    for (TCHAR* p = path; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
    }
    return filename;
}

static void CheatLinkNewNode(TCHAR *szDerp)
{
	// Link new node into the list
	pPreviousCheat = pCurrentCheat;
	pCurrentCheat = (CheatInfo*)malloc(sizeof(CheatInfo));
	if (pCheatInfo == NULL) {
		pCheatInfo = pCurrentCheat;
	}

	memset(pCurrentCheat, 0, sizeof(CheatInfo));
	pCurrentCheat->pPrevious = pPreviousCheat;
	if (pPreviousCheat) {
		pPreviousCheat->pNext = pCurrentCheat;
	}

	// Fill in defaults
	pCurrentCheat->nType = 0;							    // Default to cheat type 0 (apply each frame)
	pCurrentCheat->nStatus = -1;							// Disable cheat
	pCurrentCheat->nDefault = 0;							// Set default option
	pCurrentCheat->bOneShot = 0;							// Set default option (off)
	pCurrentCheat->bWatchMode = 0;							// Set default option (off)

	_tcsncpy (pCurrentCheat->szCheatName, szDerp, QUOTE_MAX);
}

//=========================================================================
// Forward declaration for safe recursive calls
//=========================================================================
static INT32 ConfigParseFile(TCHAR* pszFilename);

struct ConfigParseContext {
	INT32  result;		// 0 = success, 1 = error
	TCHAR* filename;	// Target cheat file path
	INT32  is_wayder;	// Flag for wayder cheat set
};

//=========================================================================
// Callback: Main config file parsing logic
//=========================================================================
static void ConfigParseFileCallback(const TCHAR* inputFile, FILE* h, void* userData)
{
#define INSIDE_NOTHING (0xFFFF & (1 << ((sizeof(TCHAR) * 8) - 1)))

	ConfigParseContext* ctx = (ConfigParseContext*)userData;
	ctx->result = 1;	// Default to error

	// Fail if file handle is invalid
	if (!h)
		return;

	TCHAR szLine[8192];
	TCHAR* s, * t;
	INT32 nLen;
	bool bFirst = true;

	INT32 nLine = 0;
	TCHAR nInside = INSIDE_NOTHING;

	// Get display name from file path
	TCHAR* pszFileHeading = getFilenameFromPath(ctx->filename);

	// Read each line of the file
	while (_fgetts(szLine, 8192, h)) {
		nLine++;
		nLen = _tcslen(szLine);

		// Strip line breaks (CR/LF)
		while (nLen > 0 && (szLine[nLen - 1] == _T('\n') || szLine[nLen - 1] == _T('\r'))) {
			szLine[--nLen] = 0;
		}

		s = szLine;

		// Skip comment lines (// ...)
		if (s[0] == _T('/') && s[1] == _T('/'))
			continue;

		// Parse [include] directive - recursively load another file
		if ((t = LabelCheck(s, _T("include"))) != NULL) {
			s = t;
			TCHAR szFilename[MAX_PATH] = { 0 };
			TCHAR* szQuote = NULL;
			QuoteRead(&szQuote, NULL, s);

			// Try .dat first
			_stprintf(szFilename, _T("%s%s.dat"), szAppCheatsPath, szQuote);
			if (ConfigParseFile(szFilename)) {
				// If failed, try .ini
				_stprintf(szFilename, _T("%s%s.ini"), szAppCheatsPath, szQuote);
				if (ConfigParseFile(szFilename)) {
					CheatError(ctx->filename, nLine, NULL, _T("included file doesn't exist"), szLine);
				}
			}
			continue;
		}

		// Parse [cheat] entry start
		if ((t = LabelCheck(s, _T("cheat"))) != 0) {
			s = t;
			TCHAR* szQuote = NULL;
			TCHAR* szEnd = NULL;
			QuoteRead(&szQuote, &szEnd, s);
			s = szEnd;

			// Check for advanced modifier
			if ((t = LabelCheck(s, _T("advanced"))) != 0)
				s = t;

			SKIP_WS(s);

			// Missing closing bracket from previous cheat
			if (nInside == _T('{')) {
				CheatError(ctx->filename, nLine, pCurrentCheat, _T("missing closing bracket"), NULL);
				ctx->result = 1;
				return;
			}
#if 0
			if (*s != _T('\0') && *s != _T('{')) {
				CheatError(pszFilename, nLine, NULL, _T("malformed cheat declaration"), szLine);
				break;
			}
#endif
			nInside = *s;

			// Create cheat list header once
#ifndef __LIBRETRO__
			if (bFirst) {
				TCHAR szHeading[256];
				_stprintf(szHeading, _T("[ Cheats \"%s\" ]"), pszFileHeading);
				CheatLinkNewNode(szHeading);
				bFirst = false;
			}
#endif
			// Create new cheat node
			CheatLinkNewNode(szQuote);
			continue;
		}

		// Set cheat source filename (LibRetro)
#ifdef __LIBRETRO__
		_tcsncpy(pCurrentCheat->szCheatFilename, pszFileHeading, QUOTE_MAX);
#endif

		// Parse cheat [type]
		if ((t = LabelCheck(s, _T("type"))) != 0) {
			if (nInside == INSIDE_NOTHING || pCurrentCheat == NULL) {
				CheatError(ctx->filename, nLine, pCurrentCheat, _T("rogue cheat type"), szLine);
				ctx->result = 1;
				return;
			}
			s = t;
			pCurrentCheat->nType = _tcstol(s, NULL, 0);
			continue;
		}

		// Parse cheat [default] selection
		if ((t = LabelCheck(s, _T("default"))) != 0) {
			if (nInside == INSIDE_NOTHING || pCurrentCheat == NULL) {
				CheatError(ctx->filename, nLine, pCurrentCheat, _T("rogue default"), szLine);
				ctx->result = 1;
				return;
			}
			s = t;
			pCurrentCheat->nDefault = _tcstol(s, NULL, 0);
			continue;
		}

		// Parse cheat option (numeric index followed by name)
		INT32 n = _tcstol(s, &t, 0);
		if (t != s) {
			if (nInside == INSIDE_NOTHING || pCurrentCheat == NULL) {
				CheatError(ctx->filename, nLine, pCurrentCheat, _T("rogue option"), szLine);
				ctx->result = 1;
				return;
			}

			if (n < CHEAT_MAX_OPTIONS) {
				s = t;
				TCHAR* szQuote = NULL;
				TCHAR* szEnd = NULL;
				if (QuoteRead(&szQuote, &szEnd, s)) {
					CheatError(ctx->filename, nLine, pCurrentCheat, _T("option name omitted"), szLine);
					ctx->result = 1;
					return;
				}
				s = szEnd;

				// Allocate new option if needed
				if (pCurrentCheat->pOption[n] == NULL)
					pCurrentCheat->pOption[n] = (CheatOption*)malloc(sizeof(CheatOption));
				memset(pCurrentCheat->pOption[n], 0, sizeof(CheatOption));
				memcpy(pCurrentCheat->pOption[n]->szOptionName, szQuote, QUOTE_MAX * sizeof(TCHAR));

				INT32 nCurrentAddress = 0;
				bool bOK = true;
				while (nCurrentAddress < CHEAT_MAX_ADDRESS) {
					INT32 nCPU = 0, nAddress = 0, nValue = 0;

					if (SkipComma(&s)) {
						// Game Genie code parsing
						if (HW_GGENIE) {
							t = s;
							INT32 newlen = 0;
#if defined(BUILD_WIN32)
							for (INT32 z = 0; z < lstrlen(t); z++) {
#else
							for (INT32 z = 0; z < strlen(t); z++) {
#endif
								char c = toupper((char)*s);
								if (((c >= _T('A') && c <= _T('Z')) || (c >= _T('0') && c <= _T('9') || c == _T('-') || c == _T(':'))) && newlen < 10)
									pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].szGenieCode[newlen++] = c;
								s++;
								if (*s == _T(',')) break;
							}
							nAddress = 0xFFFF;
						} else {
							// Read CPU, address, value
							nCPU = _tcstol(s, &t, 0);
							if (t == s) { bOK = false; break; }
							s = t;

							SkipComma(&s);
							nAddress = _tcstol(s, &t, 0);
							if (t == s) { bOK = false; break; }
							s = t;

							SkipComma(&s);
							nValue = _tcstol(s, &t, 0);
							if (t == s) { bOK = false; break; }
						}
					} else {
						if (nCurrentAddress) break;
						if (n) { bOK = false; break; }
					}

					// Store parsed address info
					pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nCPU = nCPU;
					pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nAddress = nAddress;
					pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nValue = nValue;
					nCurrentAddress++;
				}

				if (!bOK) { ctx->result = 1; return; }
			}
			continue;
		}

		SKIP_WS(s);

		// Closing bracket for cheat entry
		if (*s == _T('}')) {
			if (nInside != _T('{')) {
				CheatError(ctx->filename, nLine, pCurrentCheat, _T("missing opening bracket"), NULL);
				ctx->result = 1;
				return;
			}
			nInside = INSIDE_NOTHING;
		}
	}

	// Parsing completed successfully
	ctx->result = 0;
}

//=========================================================================
// Main API: Safe wrapper for config file parsing
//=========================================================================
static INT32 ConfigParseFile(TCHAR* pszFilename)
{
	ConfigParseContext ctx = { 0 };
	ctx.filename = pszFilename;

	// Safe auto open/process/close
	SafeProcessTextFile(pszFilename, ConfigParseFileCallback, &ctx);
	if (ctx.result == 0)
		return 0;

	// Fallback: try parent driver file if this is a clone
	if ((BurnDrvGetFlags() & BDF_CLONE) && BurnDrvGetText(DRV_PARENT)) {
		TCHAR szAlternative[MAX_PATH] = { 0 };
		_stprintf(szAlternative, _T("%s%s.ini"), szAppCheatsPath, BurnDrvGetText(DRV_PARENT));

		ctx.result = 1;
		ctx.filename = szAlternative;

		SafeProcessTextFile(szAlternative, ConfigParseFileCallback, &ctx);
		return ctx.result;
	}

	return 1;
}

//=========================================================================
// Forward Declaration for safe recursion
//=========================================================================
static INT32 ConfigParseNebulaFile(TCHAR* pszFilename);

//=========================================================================
// Callback: Nebula cheat file parsing logic
//=========================================================================
static void ConfigParseNebulaFileCallback(const TCHAR* inputFile, FILE* fp, void* userData)
{
	ConfigParseContext* ctx = (ConfigParseContext*)userData;
	ctx->result = 1;	// Default to error state
	
	// Exit if file handle is invalid
	if (!fp)
		return;

	INT32 nLen, i, j, n = 0;
	TCHAR tmp[32];
	TCHAR szLine[1024];
	bool bFirst = true;

	// Extract the filename for the header display
	TCHAR* pszFileHeading = getFilenameFromPath(ctx->filename);

	// Read file line by line
	while (_fgetts(szLine, 1024, fp)) {
		nLen = _tcslen(szLine);

		// Skip short lines and section headers [..]
		if (nLen < 3 || szLine[0] == _T('['))
			continue;

		// Parse cheat name (starts with "Name=")
		if (!_tcsncmp(_T("Name="), szLine, 5)) {
			n = 0; // Reset option index

			// Add header node for the cheat list
#ifndef __LIBRETRO__
			if (bFirst) {
				TCHAR szHeading[256];
				_stprintf(szHeading, _T("[ Cheats \"%s\" (Nebula) ]"), pszFileHeading);
				CheatLinkNewNode(szHeading);
				bFirst = false;
			}
#endif
			// Create new cheat entry
			CheatLinkNewNode(szLine + 5);
			pCurrentCheat->szCheatName[nLen - 6] = _T('\0');
			continue;
		}

		// Set filename for LibRetro context
#ifdef __LIBRETRO__
		_tcsncpy(pCurrentCheat->szCheatFilename, pszFileHeading, QUOTE_MAX);
#endif

		// Parse default selection index
		if (!_tcsncmp(_T("Default="), szLine, 8) && n >= 0) {
			_tcsncpy(tmp, szLine + 8, nLen - 9);
			tmp[nLen - 9] = _T('\0');
#if defined(BUILD_WIN32)
			_stscanf(tmp, _T("%d"), &(pCurrentCheat->nDefault));
#else
			sscanf(  tmp, _T("%d"), &(pCurrentCheat->nDefault));
#endif
			continue;
		}

		// Parse cheat option name
		i = 0, j = 0;
		while (i < nLen) {
			if (szLine[i] == _T('=') && i < 4) j = i + 1;
			if (szLine[i] == _T(',') || szLine[i] == _T('\r') || szLine[i] == _T('\n')) {
				// Allocate memory for a new option if needed
				if (pCurrentCheat->pOption[n] == NULL) {
					pCurrentCheat->pOption[n] = (CheatOption*)malloc(sizeof(CheatOption));
				}
				memset(pCurrentCheat->pOption[n], 0, sizeof(CheatOption));

				// Copy the option name
				_tcsncpy(pCurrentCheat->pOption[n]->szOptionName, szLine + j, QUOTE_MAX * sizeof(TCHAR));
				pCurrentCheat->pOption[n]->szOptionName[i - j] = _T('\0');

				i++;
				j = i;
				break;
			}
			i++;
		}

		// Parse hex address and value pairs
		INT32 nAddress = -1, nValue = 0, nCurrentAddress = 0;
		while (nCurrentAddress < CHEAT_MAX_ADDRESS) {
			if (i == nLen) break;

			if (szLine[i] == _T(',') || szLine[i] == _T('\r') || szLine[i] == _T('\n')) {
				_tcsncpy(tmp, szLine + j, i - j);
				tmp[i - j] = _T('\0');

				// First value is Address, second is Value
				if (nAddress == -1) {
#if defined(BUILD_WIN32)
					_stscanf(tmp, _T("%x"), &nAddress);
#else
					sscanf(  tmp, _T("%x"), &nAddress);
#endif
				} else {
#if defined(BUILD_WIN32)
					_stscanf(tmp, _T("%x"), &nValue);
#else
					sscanf(  tmp, _T("%x"), &nValue);
#endif
					// Set cheat data (CPU 0, address XOR 1 for Nebula format)
					pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nCPU = 0;
					pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nAddress = nAddress ^ 1;
					pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nValue = nValue;
					nCurrentAddress++;

					// Reset for next pair
					nAddress = -1;
					nValue   = 0;
				}
				j = i + 1;
			}
			i++;
		}
		n++; // Move to next option index
	}

	// Parsing completed successfully
	ctx->result = 0;
}

//=========================================================================
// Main function: Safe wrapper for Nebula file parsing
//=========================================================================
static INT32 ConfigParseNebulaFile(TCHAR* pszFilename)
{
	ConfigParseContext ctx = { 0 };
	ctx.filename = pszFilename;

	// Safely open file, process, and clean up
	SafeProcessTextFile(pszFilename, ConfigParseNebulaFileCallback, &ctx);
	if (ctx.result == 0)
		return 0;

	// Fallback: Try parent driver file if current is a clone
	if ((BurnDrvGetFlags() & BDF_CLONE) && BurnDrvGetText(DRV_PARENT)) {
		TCHAR szAlternative[MAX_PATH] = { 0 };
		_stprintf(szAlternative, _T("%s%s.dat"), szAppCheatsPath, BurnDrvGetText(DRV_PARENT));

		ctx.result   = 1;
		ctx.filename = szAlternative;

		SafeProcessTextFile(szAlternative, ConfigParseNebulaFileCallback, &ctx);
		return ctx.result;
	}

	return 1;
}

#define IS_MIDWAY ((BurnDrvGetHardwareCode() & HARDWARE_PREFIX_MIDWAY) == HARDWARE_PREFIX_MIDWAY)

static INT32 ConfigParseMAMEFile_internal(FILE *fz, const TCHAR *pszFileHeading, const TCHAR *name)
{
#define AddressInfo()	\
	INT32 k = (flags >> 20) & 3;	\
	INT32 cpu = (flags >> 24) & 0x1f; \
	if (cpu > 3) cpu = 0; \
	for (INT32 i = 0; i < k+1; i++) {	\
		pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nCPU = cpu;	\
		if ((flags & 0xf0000000) == 0x80000000) { \
			pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].bRelAddress = 1; \
			pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nRelAddressOffset = nAttrib; \
			pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nRelAddressBits = (flags & 0x3000000) >> 24; \
		} \
		pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nAddress = (pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].bRelAddress) ? nAddress : nAddress + i;	\
		pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nExtended = nAttrib; \
		pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nValue = (nValue >> ((k*8)-(i*8))) & 0xff;	\
		pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nMask = (nAttrib >> ((k*8)-(i*8))) & 0xff;	\
		pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nMultiByte = i;	\
		pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nTotalByte = k+1;	\
		nCurrentAddress++;	\
	}	\

#define AddressInfoGameGenie() { \
		pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nTotalByte = 1;	\
		pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nAddress = 0xffff; \
		strcpy(pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].szGenieCode, szGGenie); \
		nCurrentAddress++;	\
	}

#define OptionName(a)	\
	if (pCurrentCheat->pOption[n] == NULL) {						\
		pCurrentCheat->pOption[n] = (CheatOption*)malloc(sizeof(CheatOption));		\
	}											\
	memset(pCurrentCheat->pOption[n], 0, sizeof(CheatOption));				\
	_tcsncpy (pCurrentCheat->pOption[n]->szOptionName, a, QUOTE_MAX * sizeof(TCHAR));	\

#define tmpcpy(a)	\
	_tcsncpy (tmp, szLine + c0[a] + 1, c0[a+1] - (c0[a]+1));	\
	tmp[c0[a+1] - (c0[a]+1)] = '\0';				\

	TCHAR tmp[256];
	TCHAR tmp2[256];
	TCHAR gName[64];
	TCHAR szLine[1024];
	char szGGenie[128] = { 0, };

	INT32 nLen;
	INT32 n = 0;
	INT32 menu = 0;
	INT32 nFound = 0;
	INT32 nCurrentAddress = 0;
	UINT32 flags = 0;
	UINT32 nAddress = 0;
	UINT32 nValue = 0;
	UINT32 nAttrib = 0;
	bool bFirst = true;

	_stprintf(gName, _T(":%s:"), name);

	while (1)
	{
		if (_fgetts(szLine, 1024, fz) == NULL)
			break;

		nLen = _tcslen (szLine);

		if (szLine[0] == ';') continue;

		/*
		 // find the cheat flags & 0x80000000 cheats (for debugging) -dink
		 int derpy = 0;
		 for (INT32 i = 0; i < nLen; i++) {
		 	if (szLine[i] == ':') {
		 		derpy++;
		 		if (derpy == 2 && szLine[i+1] == '8') {
					bprintf(0, _T("%s\n"), szLine);
				}
			}
		}
		*/

#if defined(BUILD_WIN32)
		if (_tcsncmp (szLine, gName, lstrlen(gName))) {
#else
		if (_tcsncmp (szLine, gName, strlen(gName))) {
#endif
			if (nFound) break;
			else continue;
		}

		if (_tcsstr(szLine, _T("----:REASON"))) {
			// reason to leave!
			break;
		}

		nFound = 1;

		INT32 c0[16], c1 = 0;					// find colons / break
		for (INT32 i = 0; i < nLen; i++)
			if (szLine[i] == ':' || szLine[i] == '\r' || szLine[i] == '\n')
				c0[c1++] = i;

		tmpcpy(1);						// control flags
#if defined(BUILD_WIN32)
		_stscanf (tmp, _T("%x"), &flags);
#else
		sscanf (tmp, _T("%x"), &flags);
#endif

		tmpcpy(2);						// cheat address
#if defined(BUILD_WIN32)
		_stscanf (tmp, _T("%x"), &nAddress);
		strcpy(szGGenie, TCHARToANSI(tmp, NULL, 0));
#else
		sscanf (tmp, _T("%x"), &nAddress);
		strcpy(szGGenie, tmp);
#endif

		tmpcpy(3);						// cheat value
#if defined(BUILD_WIN32)
		_stscanf (tmp, _T("%x"), &nValue);
#else
		sscanf (tmp, _T("%x"), &nValue);
#endif

		tmpcpy(4);						// cheat attribute
#if defined(BUILD_WIN32)
		_stscanf (tmp, _T("%x"), &nAttrib);
#else
		sscanf (tmp, _T("%x"), &nAttrib);
#endif

		tmpcpy(5);						// cheat name

		// & 0x4000 = don't add to list
		// & 0x0800 = BCD
		if (flags & 0x00004800) continue;			// skip various cheats (unhandled methods at this time)

		if ((flags & 0xff000000) == 0x39000000 && IS_MIDWAY) {
			nAddress |= 0xff800000 >> 3; // 0x39 = address is relative to system's ROM block, only midway uses this kinda cheats
		}

		if ( flags & 0x00008000 || (flags & 0x00010000 && !menu)) { // Linked cheat "(2/2) etc.."
			if (nCurrentAddress < CHEAT_MAX_ADDRESS) {
				if (HW_GGENIE) {
					AddressInfoGameGenie();
				} else {
					AddressInfo();
				}
			}

			continue;
		}

		if (~flags & 0x00010000) {
			n = 0;
			menu = 0;
			nCurrentAddress = 0;

#ifndef __LIBRETRO__
			if (bFirst) {
				TCHAR szHeading[256];
				_stprintf(szHeading, _T("[ Cheats \"%s\" ]"), pszFileHeading);
				CheatLinkNewNode(szHeading);
				bFirst = false;
			}
#endif

			CheatLinkNewNode(tmp);

#ifdef __LIBRETRO__
			_tcsncpy (pCurrentCheat->szCheatFilename, pszFileHeading, QUOTE_MAX);
#endif

#if defined(BUILD_WIN32)
			if (lstrlen(tmp) <= 0 || flags == 0x60000000) {
#else
			if (strlen(tmp) <= 0 || flags == 0x60000000) {
#endif
				n++;
				continue;
			}

			OptionName(_T("Disabled"));

			if (nAddress || HW_GGENIE) {
				if ((flags & 0x80018) == 0 && nAttrib != 0xffffffff) {
					pCurrentCheat->bWriteWithMask = 1; // nAttrib field is the mask
				}
				if (flags & 0x1) {
					pCurrentCheat->bOneShot = 1; // apply once and stop
				}
				if (flags & 0x2) {
					pCurrentCheat->bWaitForModification = 1; // wait for modification before changing
				}
				if (flags & 0x80000) {
					pCurrentCheat->bWaitForModification = 2; // check address against extended field before changing
				}
				if (flags & 0x800000) {
					pCurrentCheat->bRestoreOnDisable = 1; // restore previous value on disable
				}
				if (flags & 0x3000) {
					pCurrentCheat->nPrefillMode = (flags & 0x3000) >> 12;
				}
				if ((flags & 0x6) == 0x6) {
					pCurrentCheat->bWatchMode = 1; // display value @ address
				}
				if (flags & 0x100) { // add options
					INT32 nTotal = nValue + 1;
					INT32 nPlus1 = (flags & 0x200) ? 1 : 0; // displayed value +1?
					INT32 nStartValue = (flags & 0x400) ? 1 : 0; // starting value

					//bprintf(0, _T("adding .. %X. options\n"), nTotal);
					if (nTotal > 0xff) continue; // bad entry (roughrac has this)
					for (nValue = nStartValue; nValue < nTotal; nValue++) {
#if defined(UNICODE)
						swprintf(tmp2, L"# %d.", nValue + nPlus1);
#else
						sprintf(tmp2, _T("# %d."), nValue + nPlus1);
#endif
						n++;
						nCurrentAddress = 0;
						OptionName(tmp2);
						if (HW_GGENIE) {
							AddressInfoGameGenie();
						} else {
							AddressInfo();
						}
					}
				} else {
					n++;
					OptionName(tmp);
					if (HW_GGENIE) {
						AddressInfoGameGenie();
					} else {
						AddressInfo();
					}
				}
			} else {
				menu = 1;
			}

			continue;
		}

		if ( flags & 0x00010000 && menu) {
			n++;
			nCurrentAddress = 0;

			if ((flags & 0x80018) == 0 && nAttrib != 0xffffffff) {
				pCurrentCheat->bWriteWithMask = 1; // nAttrib field is the mask
			}
			if (flags & 0x1) {
				pCurrentCheat->bOneShot = 1; // apply once and stop
			}
			if (flags & 0x2) {
				pCurrentCheat->bWaitForModification = 1; // wait for modification before changing
			}
			if (flags & 0x80000) {
				pCurrentCheat->bWaitForModification = 2; // check address against extended field before changing
			}
			if (flags & 0x800000) {
				pCurrentCheat->bRestoreOnDisable = 1; // restore previous value on disable
			}
			if (flags & 0x3000) {
				pCurrentCheat->nPrefillMode = (flags & 0x3000) >> 12;
			}
			if ((flags & 0x6) == 0x6) {
				pCurrentCheat->bWatchMode = 1; // display value @ address
			}

			OptionName(tmp);
			if (HW_GGENIE) {
				AddressInfoGameGenie();
			} else {
				AddressInfo();
			}

			continue;
		}
	}

	// if no cheat was found, don't return success code
	if (pCurrentCheat == NULL) return 1;

	return 0;
}

//=========================================================================
// Forward Declaration for safe internal calls
//=========================================================================
static INT32 ConfigParseMAMEFile(int is_wayder);

//=========================================================================
// Callback: MAME file processing logic (safe handle, auto clean-up)
//=========================================================================
static void ConfigParseMAMEFileCallback(const TCHAR* inputFile, FILE* fz, void* userData)
{
	ConfigParseContext* ctx = (ConfigParseContext*)userData;
	ctx->result = 1;	// Default to error state

	// Exit if file handle is invalid
	if (!fz)
		return;

	// Extract filename for UI header display
	TCHAR* pszFileHeading   = getFilenameFromPath(ctx->filename);
	const TCHAR* pszDrvName = BurnDrvGetText(DRV_NAME);

	// First pass: parse using current driver name
	ctx->result = ConfigParseMAMEFile_internal(fz, pszFileHeading, pszDrvName);

	// Fallback: if failed and this is a clone, retry with parent driver name
	if (ctx->result != 0 &&
		(BurnDrvGetFlags() & BDF_CLONE) &&
		BurnDrvGetText(DRV_PARENT)) {
		// Reset file pointer to start
		fseek(fz, 0, SEEK_SET);
		ctx->result = ConfigParseMAMEFile_internal(fz, pszFileHeading, BurnDrvGetText(DRV_PARENT));
	}

	// SafeProcessTextFile will auto-close the file, no need to call fclose() here
}

//=========================================================================
// Main API: Safe MAME cheat file loader (cross-platform)
//=========================================================================
static INT32 ConfigParseMAMEFile(int is_wayder)
{
	TCHAR szFileName[MAX_PATH] = _T("");

	// Build target cheat file path
	if (is_wayder) {
		// Skip wayder cheats for NES/SNES
		if (HW_NES || HW_SNES)
			return 1;

		_stprintf(szFileName, _T("%swayder_cheat.dat"), szAppCheatsPath);
	} else {
		// Use platform-specific cheat file
		if (HW_NES)
			_stprintf(szFileName, _T("%scheatnes.dat"), szAppCheatsPath);
		else if (HW_SNES)
			_stprintf(szFileName, _T("%scheatsnes.dat"), szAppCheatsPath);
		else
			_stprintf(szFileName, _T("%scheat.dat"), szAppCheatsPath);
	}
	
	// Initialize context
	ConfigParseContext ctx = { 0 };
	ctx.filename  = szFileName;
	ctx.is_wayder = is_wayder;

	// Safely open, process, and clean up the file
	SafeProcessTextFile(szFileName, ConfigParseMAMEFileCallback, &ctx);

	return ctx.result;
}

static int encodeNES(int address, int value, int compare, char *result) {
	const char ALPHABET_NES[2][16+1] = { { "APZLGITYEOXUKSVN" }, { "apzlgityeoxuksvn" } };

	bool address_lower = !(address & 0x8000);

	unsigned int genie = ((value & 0x80) >> 4) | (value & 0x7);
    unsigned int temp = ((address & 0x80) >> 4) | ((value & 0x70) >> 4);
	genie <<= 4;
	genie |= temp;

	temp = (address & 0x70) >> 4;

	if (compare != -1) {
		temp |= 0x8;
	}

	genie <<= 4;
	genie |= temp;

	temp = (address & 0x8) | ((address & 0x7000) >> 12);
	genie <<= 4;
	genie |= temp;

	temp = ((address & 0x800) >> 8) | (address & 0x7);
	genie <<= 4;
	genie |= temp;

	if (compare != -1) {

		temp = (compare & 0x8) | ((address & 0x700) >> 8);
		genie <<= 4;
		genie |= temp;

		temp = ((compare & 0x80) >> 4) | (compare & 0x7);
		genie <<= 4;
		genie |= temp;

		temp = (value & 0x8) | ((compare & 0x70) >> 4);
		genie <<= 4;
		genie |= temp;
	} else {
		temp = (value & 0x8) | ((address & 0x700) >> 8);
		genie <<= 4;
		genie |= temp;
	}

	result[6] = result[7] = result[8] = 0;

	for (int i = 0; i < ((compare != -1) ? 8 : 6); i++) {
		result[((compare != -1) ? 8 : 6) - 1 - i] = ALPHABET_NES[address_lower][(genie >> (i * 4)) & 0xF];
	}

	return 0;
}

// VirtuaNES .vct format
//=========================================================================
// Forward Declaration for safe recursion
//=========================================================================
static INT32 ConfigParseVCT(TCHAR* pszFilename);

//=========================================================================
// Callback: VCT cheat file parsing logic
//=========================================================================
static void ConfigParseVCTCallback(const TCHAR* inputFile, FILE* h, void* userData)
{
#define AddressInfoGameGenie() {															\
    pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nTotalByte = 1;					\
    pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].nAddress = 0xffff;				\
    strcpy(pCurrentCheat->pOption[n]->AddressInfo[nCurrentAddress].szGenieCode, szGGenie);	\
    nCurrentAddress++;																		\
}

#define OptionName(a)																		\
    if (pCurrentCheat->pOption[n] == NULL) {												\
        pCurrentCheat->pOption[n] = (CheatOption*)malloc(sizeof(CheatOption));				\
    }																						\
    memset(pCurrentCheat->pOption[n], 0, sizeof(CheatOption));								\
    _tcsncpy (pCurrentCheat->pOption[n]->szOptionName, a, QUOTE_MAX * sizeof(TCHAR));		\

#define tmpcpy(a)																			\
    _tcsncpy (tmp, szLine + c0[a] + 1, c0[a+1] - (c0[a]+1));								\
    tmp[c0[a+1] - (c0[a]+1)] = '\0';														\

	ConfigParseContext* ctx = (ConfigParseContext*)userData;
	ctx->result = 1;	// Default to error state

	if (!h)
		return;

	TCHAR tmp[256];
	TCHAR szLine[1024];
	char szGGenie[256] = { 0 };

	INT32 nLen;
	INT32 n = 0;
	INT32 nCurrentAddress = 0;
	bool bFirst = true;
	TCHAR* pszFileHeading = getFilenameFromPath(ctx->filename);

	// Read file line by line
	while (_fgetts(szLine, 1024, h) != NULL) {
		nLen = _tcslen(szLine);

		// Skip comment lines
		if (szLine[0] == ';')
			continue;

		INT32 c0[16], c1 = 0, cprev = 0;
		for (INT32 i = 0; i < nLen; i++) {
			if ((szLine[i] == ' ' && c1 < 2) || szLine[i] == '\t' || szLine[i] == '\r' || szLine[i] == '\n') {
				if (cprev + 1 != i)
					c0[c1++] = i;
				cprev = i;
			}
		}

		// Parse Game Genie code and description
		tmpcpy(0);
		strcpy(szGGenie, TCHARToANSI(tmp, NULL, 0));
		szGGenie[255] = '\0';
		tmpcpy(1);

		if (HW_GGENIE) {
			char temp2[256] = { 0 };
			UINT32 fAddr = 0;
			UINT32 fCount = 0;
			UINT32 fAttr = 0;
			UINT32 fBytes = 0;

			strcpy(temp2, szGGenie);

			// Parse "address-count-bytes" format
			char* tok = strtok(temp2, "-");
			if (!tok) continue;
			sscanf(tok, "%x", &fAddr);

			tok = strtok(NULL, "-");
			if (!tok) continue;
			sscanf(tok, "%x", &fCount);
			fAttr = (fCount & 0x30) >> 4;
			fCount &= 0x07;
			if (fCount < 1 || fCount > 4)
				fCount = 1;

			tok = strtok(NULL, "-");
			if (!tok) continue;
			sscanf(tok, "%x", &fBytes);

			// Create cheat list header
#ifndef __LIBRETRO__
			if (bFirst) {
				TCHAR szHeading[256];
				_stprintf(szHeading, _T("[ Cheats \"%s\" ]"), pszFileHeading);
				CheatLinkNewNode(szHeading);
				bFirst = false;
			}
#endif

			// Initialize new cheat entry
			n = 0;
			nCurrentAddress = 0;
			CheatLinkNewNode(tmp);

#ifdef __LIBRETRO__
			_tcsncpy(pCurrentCheat->szCheatFilename, pszFileHeading, QUOTE_MAX);
#endif

			// Add default options
			OptionName(_T("Disabled"));
			n++;
			OptionName(_T("Enabled"));

			// Encode and add Game Genie codes
			for (int i = 0; i < fCount; i++) {
				memset(szGGenie, 0, sizeof(szGGenie));
				encodeNES(fAddr + i, fBytes >> (i * 8), -1, szGGenie);
				INT32 cLen = strlen(szGGenie);
				szGGenie[cLen] = '0' + fAttr;
				AddressInfoGameGenie();
			}
		}
	}

	// Return error if no cheats were loaded
	if (pCurrentCheat == NULL)
		ctx->result = 1;
	else
		ctx->result = 0;
}

//=========================================================================
// Main API: Safe VCT file parser
//=========================================================================
static INT32 ConfigParseVCT(TCHAR* pszFilename)
{
	ConfigParseContext ctx = { 0 };
	ctx.filename = pszFilename;

	// Safe open, process, auto clean-up
	SafeProcessTextFile(pszFilename, ConfigParseVCTCallback, &ctx);
	if (ctx.result == 0)
		return 0;

	// Fallback to parent driver file
	if ((BurnDrvGetFlags() & BDF_CLONE) && BurnDrvGetText(DRV_PARENT))
	{
		TCHAR szAlternative[MAX_PATH] = { 0 };
		_stprintf(szAlternative, _T("%s%s.vct"), szAppCheatsPath, BurnDrvGetText(DRV_PARENT));

		ctx.result = 1;
		ctx.filename = szAlternative;

		SafeProcessTextFile(szAlternative, ConfigParseVCTCallback, &ctx);
		return ctx.result;
	}

	return 1;
}


INT32 ConfigCheatLoad()
{
	TCHAR szFilename[MAX_PATH] = _T("");
	TCHAR szDrvName[MAX_PATH] = _T("");

	if (NeoCDInfo_ID()) {
		_stprintf(szDrvName, _T("ngcd_%s"), NeoCDInfo_Text(DRV_NAME));
	} else {
		_stprintf(szDrvName, _T("%s"), BurnDrvGetText(DRV_NAME));
	}
	bprintf(0, _T("Cheat engine, game name: %s\n"), szDrvName);

	pCurrentCheat = NULL;
	pPreviousCheat = NULL;

	if (HW_NES) { // only for NES/FC!
		_stprintf(szFilename, _T("%s%s.vct"), szAppCheatsPath, szDrvName);
		ConfigParseVCT(szFilename);
	} // keep loading & adding stuff even if .vct file loads.

	// cheat.dat, cheatnes.dat, cheatsnes.dat, wayder_cheat.dat
	ConfigParseMAMEFile(0 /* cheat.dat, cheatnes.dat, cheatsnes.dat */);
	ConfigParseMAMEFile(1 /* wayder */);

	// ini-style file
	_stprintf(szFilename, _T("%s%s.ini"), szAppCheatsPath, szDrvName);
	ConfigParseFile(szFilename);

	// nebula-format .dat file
	_stprintf(szFilename, _T("%s%s.dat"), szAppCheatsPath, szDrvName);
	ConfigParseNebulaFile(szFilename);

	if (pCheatInfo) {
		INT32 nCurrentCheat = 0;
		while (CheatEnable(nCurrentCheat, -1) == 0) {
			nCurrentCheat++;
		}

		CheatUpdate();
	}

	return 0;
}
