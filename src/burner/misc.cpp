// Misc functions module
#include <math.h>
#include "burner.h"

// ---------------------------------------------------------------------------
// Software gamma

INT32 bDoGamma = 0;
INT32 bHardwareGammaOnly = 1;
double nGamma = 1.25;
const INT32 nMaxRGB = 255;

static UINT8 GammaLUT[256];

void ComputeGammaLUT()
{
	for (INT32 i = 0; i < 256; i++) {
		GammaLUT[i] = (UINT8)((double)nMaxRGB * pow((i / 255.0), nGamma));
	}
}

// Standard callbacks for 16/24/32 bit color:
static UINT32 __cdecl HighCol15(INT32 r, INT32 g, INT32 b, INT32  /* i */)
{
	UINT32 t;
	t =(r<<7)&0x7c00; // 0rrr rr00 0000 0000
	t|=(g<<2)&0x03e0; // 0000 00gg ggg0 0000
	t|=(b>>3)&0x001f; // 0000 0000 000b bbbb
	return t;
}

static UINT32 __cdecl HighCol16(INT32 r, INT32 g, INT32 b, INT32 /* i */)
{
	UINT32 t;
	t =(r<<8)&0xf800; // rrrr r000 0000 0000
	t|=(g<<3)&0x07e0; // 0000 0ggg ggg0 0000
	t|=(b>>3)&0x001f; // 0000 0000 000b bbbb
	return t;
}

// 24-bit/32-bit
static UINT32 __cdecl HighCol24(INT32 r, INT32 g, INT32 b, INT32  /* i */)
{
	UINT32 t;
	t =(r<<16)&0xff0000;
	t|=(g<<8 )&0x00ff00;
	t|=(b    )&0x0000ff;

	return t;
}

static UINT32 __cdecl HighCol15Gamma(INT32 r, INT32 g, INT32 b, INT32  /* i */)
{
	UINT32 t;
	t = (GammaLUT[r] << 7) & 0x7C00; // 0rrr rr00 0000 0000
	t |= (GammaLUT[g] << 2) & 0x03E0; // 0000 00gg ggg0 0000
	t |= (GammaLUT[b] >> 3) & 0x001F; // 0000 0000 000b bbbb
	return t;
}

static UINT32 __cdecl HighCol16Gamma(INT32 r, INT32 g ,INT32 b, INT32  /* i */)
{
	UINT32 t;
	t = (GammaLUT[r] << 8) & 0xF800; // rrrr r000 0000 0000
	t |= (GammaLUT[g] << 3) & 0x07E0; // 0000 0ggg ggg0 0000
	t |= (GammaLUT[b] >> 3) & 0x001F; // 0000 0000 000b bbbb
	return t;
}

// 24-bit/32-bit
static UINT32 __cdecl HighCol24Gamma(INT32 r, INT32 g, INT32 b, INT32  /* i */)
{
	UINT32 t;
	t = (GammaLUT[r] << 16);
	t |= (GammaLUT[g] << 8);
	t |= GammaLUT[b];

	return t;
}

INT32 SetBurnHighCol(INT32 nDepth)
{
	VidRecalcPal();

	if (bDoGamma && ((nVidFullscreen && !bVidUseHardwareGamma) || (!nVidFullscreen && !bHardwareGammaOnly))) {
		if (nDepth == 15) {
			VidHighCol = HighCol15Gamma;
		}
		if (nDepth == 16) {
			VidHighCol = HighCol16Gamma;
		}
		if (nDepth > 16) {
			VidHighCol = HighCol24Gamma;
		}
	} else {
		if (nDepth == 15) {
			VidHighCol = HighCol15;
		}
		if (nDepth == 16) {
			VidHighCol = HighCol16;
		}
		if (nDepth > 16) {
			VidHighCol = HighCol24;
		}
	}
	if ((bDrvOkay && !(BurnDrvGetFlags() & BDF_16BIT_ONLY)) || nDepth <= 16) {
		BurnHighCol = VidHighCol;
	}

	return 0;
}

// ---------------------------------------------------------------------------

char* DecorateGameName(UINT32 nBurnDrv)
{
	static char szDecoratedName[256];
	UINT32 nOldBurnDrv = nBurnDrvActive;

	nBurnDrvActive = nBurnDrv;

	const char* s1 = "";
	const char* s2 = "";
	const char* s3 = "";
	const char* s4 = "";
	const char* s5 = "";
	const char* s6 = "";
	const char* s7 = "";
	const char* s8 = "";
	const char* s9 = "";
	const char* s10 = "";
	const char* s11 = "";
	const char* s12 = "";
	const char* s13 = "";
	const char* s14 = "";

	s1 = BurnDrvGetTextA(DRV_FULLNAME);
	if ((BurnDrvGetFlags() & BDF_DEMO) || (BurnDrvGetFlags() & BDF_HACK) || (BurnDrvGetFlags() & BDF_HOMEBREW) || (BurnDrvGetFlags() & BDF_PROTOTYPE) || (BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
		s2 = " [";
		if (BurnDrvGetFlags() & BDF_DEMO) {
			s3 = "Demo";
			if ((BurnDrvGetFlags() & BDF_HACK) || (BurnDrvGetFlags() & BDF_HOMEBREW) || (BurnDrvGetFlags() & BDF_PROTOTYPE) || (BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
				s4 = ", ";
			}
		}
		if (BurnDrvGetFlags() & BDF_HACK) {
			s5 = "Hack";
			if ((BurnDrvGetFlags() & BDF_HOMEBREW) || (BurnDrvGetFlags() & BDF_PROTOTYPE) || (BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
				s6 = ", ";
			}
		}
		if (BurnDrvGetFlags() & BDF_HOMEBREW) {
			s7 = "Homebrew";
			if ((BurnDrvGetFlags() & BDF_PROTOTYPE) || (BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
				s8 = ", ";
			}
		}
		if (BurnDrvGetFlags() & BDF_PROTOTYPE) {
			s9 = "Prototype";
			if ((BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
				s10 = ", ";
			}
		}
		if (BurnDrvGetFlags() & BDF_BOOTLEG) {
			s11 = "Bootleg";
			if (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0) {
				s12 = ", ";
			}
		}
		if (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0) {
			s13 = BurnDrvGetTextA(DRV_COMMENT);
		}
		s14 = "]";
	}

	sprintf(szDecoratedName, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s", s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14);

	nBurnDrvActive = nOldBurnDrv;
	return szDecoratedName;
}

TCHAR* DecorateGenreInfo()
{
	INT32 nGenre = BurnDrvGetGenreFlags();
	INT32 nFamily = BurnDrvGetFamilyFlags();

	static TCHAR szDecoratedGenre[256];
	TCHAR szFamily[256];

	_stprintf(szDecoratedGenre, _T(""));
	_stprintf(szFamily, _T(""));

#ifdef BUILD_WIN32
//TODO: Translations are not working in non-win32 builds. This needs to be fixed

	if (nGenre) {
		if (nGenre & GBF_HORSHOOT) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_HORSHOOT, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_VERSHOOT) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_VERSHOOT, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_SCRFIGHT) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_SCRFIGHT, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_VSFIGHT) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_VSFIGHT, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_BIOS) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_BIOS, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_BREAKOUT) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_BREAKOUT, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_CASINO) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_CASINO, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_BALLPADDLE) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_BALLPADDLE, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_MAZE) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_MAZE, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_MINIGAMES) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_MINIGAMES, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_PINBALL) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_PINBALL, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_PLATFORM) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_PLATFORM, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_PUZZLE) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_PUZZLE, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_QUIZ) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_QUIZ, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_SPORTSMISC) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_SPORTSMISC, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_SPORTSFOOTBALL) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_SPORTSFOOTBALL, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_MISC) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_MISC, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_MAHJONG) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_MAHJONG, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_RACING) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_RACING, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_SHOOT) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_SHOOT, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_ACTION) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_ACTION, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_RUNGUN) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_RUNGUN, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_STRATEGY) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_STRATEGY, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_RPG) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_RPG, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_SIM) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_SIM, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		szDecoratedGenre[_tcslen(szDecoratedGenre) - 2] = _T('\0');
	}

	if (nFamily) {
		_stprintf(szFamily, _T(" ("));

		if (nFamily & FBF_MSLUG) {
			_stprintf(szFamily + _tcslen(szFamily), FBALoadStringEx(hAppInst, IDS_FAMILY_MSLUG, true));
			_stprintf(szFamily + _tcslen(szFamily), _T(", "));
		}

		if (nFamily & FBF_SF) {
			_stprintf(szFamily + _tcslen(szFamily), FBALoadStringEx(hAppInst, IDS_FAMILY_SF, true));
			_stprintf(szFamily + _tcslen(szFamily), _T(", "));
		}

		if (nFamily & FBF_KOF) {
			_stprintf(szFamily + _tcslen(szFamily), FBALoadStringEx(hAppInst, IDS_FAMILY_KOF, true));
			_stprintf(szFamily + _tcslen(szFamily), _T(", "));
		}

		if (nFamily & FBF_DSTLK) {
			_stprintf(szFamily + _tcslen(szFamily), FBALoadStringEx(hAppInst, IDS_FAMILY_DSTLK, true));
			_stprintf(szFamily + _tcslen(szFamily), _T(", "));
		}

		if (nFamily & FBF_FATFURY) {
			_stprintf(szFamily + _tcslen(szFamily), FBALoadStringEx(hAppInst, IDS_FAMILY_FATFURY, true));
			_stprintf(szFamily + _tcslen(szFamily), _T(", "));
		}

		if (nFamily & FBF_SAMSHO) {
			_stprintf(szFamily + _tcslen(szFamily), FBALoadStringEx(hAppInst, IDS_FAMILY_SAMSHO, true));
			_stprintf(szFamily + _tcslen(szFamily), _T(", "));
		}

		if (nFamily & FBF_19XX) {
			_stprintf(szFamily + _tcslen(szFamily), FBALoadStringEx(hAppInst, IDS_FAMILY_19XX, true));
			_stprintf(szFamily + _tcslen(szFamily), _T(", "));
		}

		if (nFamily & FBF_SONICWI) {
			_stprintf(szFamily + _tcslen(szFamily), FBALoadStringEx(hAppInst, IDS_FAMILY_SONICWI, true));
			_stprintf(szFamily + _tcslen(szFamily), _T(", "));
		}

		if (nFamily & FBF_PWRINST) {
			_stprintf(szFamily + _tcslen(szFamily), FBALoadStringEx(hAppInst, IDS_FAMILY_PWRINST, true));
			_stprintf(szFamily + _tcslen(szFamily), _T(", "));
		}

		if (nFamily & FBF_SONIC) {
			_stprintf(szFamily + _tcslen(szFamily), FBALoadStringEx(hAppInst, IDS_FAMILY_SONIC, true));
			_stprintf(szFamily + _tcslen(szFamily), _T(", "));
		}

		if (nFamily & FBF_DONPACHI) {
			_stprintf(szFamily + _tcslen(szFamily), FBALoadStringEx(hAppInst, IDS_FAMILY_DONPACHI, true));
			_stprintf(szFamily + _tcslen(szFamily), _T(", "));
		}

		if (nFamily & FBF_MAHOU) {
			_stprintf(szFamily + _tcslen(szFamily), FBALoadStringEx(hAppInst, IDS_FAMILY_MAHOU, true));
			_stprintf(szFamily + _tcslen(szFamily), _T(", "));
		}

		szFamily[_tcslen(szFamily) - 2] = _T(')');
		szFamily[_tcslen(szFamily) - 1] = _T('\0');

		_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), szFamily);
	}
#endif

	return szDecoratedGenre;
}

// ---------------------------------------------------------------------------
// config file parsing

TCHAR* LabelCheck(TCHAR* s, TCHAR* pszLabel)
{
	INT32 nLen;
	if (s == NULL) {
		return NULL;
	}
	if (pszLabel == NULL) {
		return NULL;
	}
	nLen = _tcslen(pszLabel);

	SKIP_WS(s);													// Skip whitespace

	if (_tcsncmp(s, pszLabel, nLen)){							// Doesn't match
		return NULL;
	}
	return s + nLen;
}

INT32 QuoteRead(TCHAR** ppszQuote, TCHAR** ppszEnd, TCHAR* pszSrc)	// Read a (quoted) string from szSrc and poINT32 to the end
{
	static TCHAR szQuote[QUOTE_MAX];
	TCHAR* s = pszSrc;
	TCHAR* e;

	// Skip whitespace
	SKIP_WS(s);

	e = s;

	if (*s == _T('\"')) {										// Quoted string
		s++;
		e++;
		// Find end quote
		FIND_QT(e);
		_tcsncpy(szQuote, s, e - s);
		// Zero-terminate
		szQuote[e - s] = _T('\0');
		e++;
	} else {													// Non-quoted string
		// Find whitespace
		FIND_WS(e);
		_tcsncpy(szQuote, s, e - s);
		// Zero-terminate
		szQuote[e - s] = _T('\0');
	}

	if (ppszQuote) {
		*ppszQuote = szQuote;
	}
	if (ppszEnd)	{
		*ppszEnd = e;
	}

	return 0;
}

TCHAR* ExtractFilename(TCHAR* fullname)
{
	TCHAR* filename = fullname + _tcslen(fullname);

	do {
		filename--;
	} while (filename >= fullname && *filename != _T('\\') && *filename != _T('/') && *filename != _T(':'));

	return filename;
}

TCHAR* DriverToName(UINT32 nDrv)
{
	TCHAR* szName = NULL;
	UINT32 nOldDrv;

	if (nDrv >= nBurnDrvCount) {
		return NULL;
	}

	nOldDrv = nBurnDrvActive;
	nBurnDrvActive = nDrv;
	szName = BurnDrvGetText(DRV_NAME);
	nBurnDrvActive = nOldDrv;

	return szName;
}

UINT32 NameToDriver(TCHAR* szName)
{
	UINT32 nOldDrv = nBurnDrvActive;
	UINT32 nDrv = 0;

	for (nBurnDrvActive = 0; nBurnDrvActive < nBurnDrvCount; nBurnDrvActive++) {
		if (_tcscmp(szName, BurnDrvGetText(DRV_NAME)) == 0 && !(BurnDrvGetFlags() & BDF_BOARDROM)) {
			break;
		}
	}
	nDrv = nBurnDrvActive;
	if (nDrv >= nBurnDrvCount) {
		nDrv = ~0U;
	}

	nBurnDrvActive = nOldDrv;

	return nDrv;
}

TCHAR *FileExt(TCHAR *str)
{
	TCHAR *dot = _tcsrchr(str, _T('.'));

	return (dot) ? StrLower(dot) : str;
}

bool IsFileExt(TCHAR *str, TCHAR *ext)
{
	return (_tcsicmp(ext, FileExt(str)) == 0);
}

TCHAR *StrReplace(TCHAR *str, TCHAR find, TCHAR replace)
{
	INT32 length = _tcslen(str);

	for (INT32 i = 0; i < length; i++) {
		if (str[i] == find) str[i] = replace;
	}

	return str;
}

// StrLower() - leaves str untouched, returns modified string
TCHAR *StrLower(TCHAR *str)
{
	static TCHAR szBuffer[256] = _T("");
	INT32 length = _tcslen(str);

	if (length > 255) length = 255;

	for (INT32 i = 0; i < length; i++) {
		if (str[i] >= _T('A') && str[i] <= _T('Z'))
			szBuffer[i] = (str[i] + _T(' '));
		else
			szBuffer[i] = str[i];
	}
	szBuffer[length] = 0;

	return &szBuffer[0];
}
