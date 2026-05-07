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

char* GameDecoration(UINT32 nBurnDrv)
{
	static char szGameDecoration[256];
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

	if ((BurnDrvGetFlags() & BDF_DEMO) || (BurnDrvGetFlags() & BDF_HACK) || (BurnDrvGetFlags() & BDF_HOMEBREW) || (BurnDrvGetFlags() & BDF_PROTOTYPE) || (BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
		if (BurnDrvGetFlags() & BDF_DEMO) {
			s1 = "Demo";
			if ((BurnDrvGetFlags() & BDF_HACK) || (BurnDrvGetFlags() & BDF_HOMEBREW) || (BurnDrvGetFlags() & BDF_PROTOTYPE) || (BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
				s2 = ", ";
			}
		}
		if (BurnDrvGetFlags() & BDF_HACK) {
			s3 = "Hack";
			if ((BurnDrvGetFlags() & BDF_HOMEBREW) || (BurnDrvGetFlags() & BDF_PROTOTYPE) || (BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
				s4 = ", ";
			}
		}
		if (BurnDrvGetFlags() & BDF_HOMEBREW) {
			s5 = "Homebrew";
			if ((BurnDrvGetFlags() & BDF_PROTOTYPE) || (BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
				s6 = ", ";
			}
		}
		if (BurnDrvGetFlags() & BDF_PROTOTYPE) {
			s7 = "Prototype";
			if ((BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
				s8 = ", ";
			}
		}		
		if (BurnDrvGetFlags() & BDF_BOOTLEG) {
			s9 = "Bootleg";
			if (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0) {
				s10 = ", ";
			}
		}
		if (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0) {
			s11 = BurnDrvGetTextA(DRV_COMMENT);
		}
	}

	snprintf(szGameDecoration, sizeof(szGameDecoration), "%s%s%s%s%s%s%s%s%s%s%s", s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11);

	nBurnDrvActive = nOldBurnDrv;
	return szGameDecoration;
}

char* DecorateGameName(UINT32 nBurnDrv)
{
	static char szDecoratedName[256];
	UINT32 nOldBurnDrv = nBurnDrvActive;

	nBurnDrvActive = nBurnDrv;

	const char* s1 = "";
	const char* s2 = "";
	const char* s3 = "";
	const char* s4 = "";

	s1 = BurnDrvGetTextA(DRV_FULLNAME);

	s3 = GameDecoration(nBurnDrv);
	if (strlen(s3) > 0) {
		s2 = " [";
		s4 = "]";
	}

	snprintf(szDecoratedName, sizeof(szDecoratedName), "%s%s%s%s", s1, s2, s3, s4);

	nBurnDrvActive = nOldBurnDrv;
	return szDecoratedName;
}

TCHAR* DecorateGenreInfo()
{
	INT32 nGenre  = BurnDrvGetGenreFlags();
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

		if (nGenre & GBF_MULTISHOOT) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_MULTISHOOT, true));
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

		if (nGenre & GBF_ADV) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_ADV, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_CARD) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_CARD, true));
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), _T(", "));
		}

		if (nGenre & GBF_BOARD) {
			_stprintf(szDecoratedGenre + _tcslen(szDecoratedGenre), FBALoadStringEx(hAppInst, IDS_GENRE_BOARD, true));
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

// Safe free function that checks for null pointers and sets them to null after freeing
void free_s(void** p)
{
	if (p && *p) {
		free(*p); *p = NULL;
	}
}

// Check if a TCHAR string is null or empty
bool IsStrEmpty(const TCHAR* s)
{
	return (!s || _T('\0') == *s);
}

// ANSI version of IsStrEmpty
bool IsStrEmptyA(const char* s)
{
	return (!s || '\0' == *s);
}

// Check if the file name has the specified extension (case-insensitive, with or without leading dot)
bool IsFileExtensionMatch(const TCHAR* fileName, const TCHAR* ext)
{
	// Null check
	if (IsStrEmpty(fileName) || IsStrEmpty(ext))
		return false;

	// Get lengths
	INT32 fileNameLen = (INT32)_tcslen(fileName);
	INT32 extLen      = (INT32)_tcslen(ext);

	// Empty extension
	if (extLen <= 0)
		return false;

	// Handle dot in extension parameter
	const TCHAR* actualExt = ext;
	INT32 actualExtLen     = extLen;

	// Skip leading dot if present
	if (_T('.') == ext[0]) {
		// If extension is just a dot, return false
		if (1 == extLen)
			return false;

		actualExt    = ext + 1;
		actualExtLen = extLen - 1;
	}

	// Extension longer than file name, cannot match
	if (actualExtLen >= fileNameLen)
		return false;

	// Calculate where extension should start in file name
	const TCHAR* fileExtStart = fileName + (fileNameLen - actualExtLen);

	// Case-insensitive comparison
	if (0 != _tcsicmp(fileExtStart, actualExt))
		return false;

	// Verify there's a dot before the extension
	// (unless the whole file name is the extension, e.g., ".gitignore")
	if (fileNameLen > actualExtLen) {
		TCHAR charBeforeExt = *(fileExtStart - 1);
		if (_T('.') != charBeforeExt)
			return false;
	}

	return true;
}

// Cross-platform thread-local storage
#if defined(__cplusplus) && __cplusplus >= 201103L
#define THREAD_LOCAL thread_local
#elif defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__)
#define THREAD_LOCAL __thread
#else
#define THREAD_LOCAL _Thread_local
#endif

// Thread-safe quoted string tokenizer
// Supports: regular strings, quoted strings, delimiters inside quotes, unclosed quotes
TCHAR* _strqtoken(TCHAR* s, const TCHAR* delims)
{
	static THREAD_LOCAL TCHAR* prev_str = NULL;
	TCHAR* token = NULL;

	// Parameter validation
	if (!delims)
		return NULL;

	// Restore context
	if (!s) {
		s = prev_str;
		if (!s)
			return NULL;
	}

	// Skip leading delimiters
	s += _tcsspn(s, delims);
	if (_T('\0') == *s) {
		prev_str = NULL;
		return NULL;
	}

	token = s;

	// Handle quoted string
	if (_T('"') == *s) {
		s++;
		// Remove opening quote
		token = s;
		TCHAR* endquote = _tcschr(s, _T('"'));

		if (endquote) {
			*endquote = _T('\0');
			// Point to position after closing quote
			s = endquote + 1;
		} else
			// No closing quote: read to end of string
			s = _tcschr(s, _T('\0'));
	} else
		// Regular field: stop at first delimiter
		s = _tcspbrk(s, delims);

	// Truncate and save context for next call
	if (s && _T('\0') != *s) {
		*s = _T('\0');
		prev_str = s + 1;
	} else
		prev_str = NULL;

	return token;
}
#undef THREAD_LOCAL

enum EncodingType {
	ENCODING_ANSI,
	ENCODING_UTF8,
	ENCODING_UTF8_BOM,
	ENCODING_UTF16_LE,
	ENCODING_UTF16_BE,
	ENCODING_EMPTY,
	ENCODING_ERROR
};

// Detects text file encoding (UTF-8 BOM, UTF-8, UTF16-LE/BE, ANSI)
static EncodingType DetectEncoding(const TCHAR* pszFileName)
{
	// Open file in binary mode
	FILE* fp = _tfopen(pszFileName, _T("rb"));
	if (!fp)
		return ENCODING_ERROR;

	// Read first 3 bytes to check BOM
	UINT8 cBom[3] = { 0 };
	const UINT32 nBomSize = fread(cBom, 1, 3, fp);

	// Empty file is NOT an error
	if (0 == nBomSize) {
		fclose(fp);
		return ENCODING_EMPTY;
	}

	// Check UTF-8 BOM (EF BB BF)
	if ((nBomSize >= 3) && (0xef == cBom[0]) && (0xbb == cBom[1]) && (0xbf == cBom[2])) {
		fclose(fp);
		return ENCODING_UTF8_BOM;
	}
	// Check UTF-16 LE BOM (FF FE)
	if (nBomSize >= 2) {
		if ((0xff == cBom[0]) && (0xfe == cBom[1])) {
			fclose(fp);
			return ENCODING_UTF16_LE;
		}
		// Check UTF-16 BE BOM (FE FF)
		if ((0xfe == cBom[0]) && (0xff == cBom[1])) {
			fclose(fp);
			return ENCODING_UTF16_BE;
		}
	}

	// reset file pointer to START after reading BOM
	rewind(fp);

	// Get total file length
	fseek(fp, 0, SEEK_END);
	long nLen = ftell(fp);
	rewind(fp);

	// Only check for negative value (I/O failure)
	// Empty file was already handled by nBomSize == 0 check above
	// Avoid malloc(-1) → undefined behavior / crash
	if (nLen < 0) {
		fclose(fp);
		return ENCODING_ERROR;
	}

	// Allocate buffer for full file content
	UINT8* pBuf = (UINT8*)malloc(nLen);
	if (!pBuf) {
		fclose(fp);
		return ENCODING_ERROR;
	}

	// Read entire file into buffer
	UINT32 nRead = fread(pBuf, 1, nLen, fp);
	fclose(fp);

	if (nRead != (UINT32)nLen) {
		free_s((void**)&pBuf);
		return ENCODING_ERROR;
	}

	// Validate UTF-8 encoding format
	UINT32 p = 0;
	while (p < nRead) {
		// Single-byte ASCII (0~0x7F) → valid
		if (pBuf[p] < 0x80) {
			p++;
			continue;
		}

		// Invalid UTF-8 start byte → treat as ANSI
		if (!((pBuf[p] >= 0xc2) && (pBuf[p] <= 0xf4))) {
			free_s((void**)&pBuf);;
			return ENCODING_ANSI;
		}

		// Use else-if to avoid overwriting nBytes value
		INT32 nBytes = 0;
		if (     (pBuf[p] >= 0xc2) && (pBuf[p] <= 0xdf))
			nBytes = 1;					// 2-byte UTF-8
		else if ((pBuf[p] >= 0xe0) && (pBuf[p] <= 0xef))
			nBytes = 2;					// 3-byte UTF-8
		else if ((pBuf[p] >= 0xf0) && (pBuf[p] <= 0xf4))
			nBytes = 3;					// 4-byte UTF-8

		// Not enough remaining bytes → invalid UTF-8
		if ((p + nBytes) >= nRead) {
			free_s((void**)&pBuf);;
			return ENCODING_ANSI;
		}

		// Check trailing bytes must be 10xxxxxx
		for (int i = 1; i <= nBytes; i++) {
			if (!(0x80 == (pBuf[p + i] & 0xc0))) {
				free_s((void**)&pBuf);;
				return ENCODING_ANSI;
			}
		}
		p += (nBytes + 1);
	}
	free_s((void**)&pBuf);;
	return ENCODING_UTF8;
}

// Converts UTF-16BE file to UTF-16LE (byte order swap)
// Returns true if successful, pszConv receives the path of the converted temp file
static bool Utf16beToUtf16le(const TCHAR* pszFileName, TCHAR** pszConv)
{
	if (IsStrEmpty(pszFileName))
		return false;					// Invalid filename parameter

	FILE* fpRead   = NULL, * fpWrite = NULL;
	UINT8* pBuffer = NULL;

	// Open input file as READ-ONLY (binary mode)
	fpRead = _tfopen(pszFileName, _T("rb"));
	if (!fpRead)
		return false;					// Failed to open input file

	// Get file length
	if (0 != fseek(fpRead, 0, SEEK_END)) {
		fclose(fpRead);
		return false;					// Failed to seek file
	}

	long nLen = ftell(fpRead);
	rewind(fpRead);

	// Reject empty / invalid file
	if (nLen <= 0) {
		fclose(fpRead);
		return false;					// Empty or invalid file
	}

	// Allocate buffer to hold file content
	pBuffer = (UINT8*)malloc(nLen);
	if (!pBuffer) {
		fclose(fpRead);
		return false;					// Memory allocation failed
	}
	memset(pBuffer, 0, nLen);

	// Read file content into buffer
	UINT32 nRead = (UINT32)fread(pBuffer, 1, nLen, fpRead);
	fclose(fpRead);
	fpRead = NULL;

	if (0 == nRead) {
		free_s((void**)&pBuffer);
		return false;					// Failed to read file data
	}

	// Ensure even number of bytes (UTF-16 uses 2 bytes per character)
	if (0 != nRead % 2)
		nRead--;

	// Convert UTF-16BE to UTF-16LE by swapping bytes
	for (UINT32 i = 0; i < nRead; i += 2) {
		UINT8 cTemp = pBuffer[i];
		pBuffer[i + 0] = pBuffer[i + 1];
		pBuffer[i + 1] = cTemp;
	}

	TCHAR* pOut = NULL;

	if (pszConv) {
		INT32 fileLen  = (INT32)_tcslen(pszFileName);
		INT32 totalLen = fileLen + (INT32)_tcslen(_T(".conv")) + 1;

		if (totalLen > MAX_PATH) {		// Prevent path buffer overflow
			free_s((void**)&pBuffer);
			return false;
		}

		pOut = (TCHAR*)malloc(totalLen * sizeof(TCHAR));
		if (!pOut) {
			free_s((void**)&pBuffer);
			return false;				// Allocation failed for output path
		}
		// Create temp converted file: "xxx.conv"
		_sntprintf(pOut, totalLen, _T("%s.conv"), pszFileName);
	}

	const TCHAR* pWriteFile = pszConv ? pOut : pszFileName;

	// Write converted UTF-16LE data (binary mode)
	fpWrite = _tfopen(pWriteFile, _T("wb"));
	if (!fpWrite) {
		free_s((void**)&pBuffer);
		free_s((void**)&pOut);
		return false;					// Failed to create output file
	}

	// Write converted data
	UINT32 nWritten = fwrite(pBuffer, 1, nRead, fpWrite);
	fflush(fpWrite);
	fclose(fpWrite);
	fpWrite = NULL;

	// Free read buffer
	free_s((void**)&pBuffer);

	// Check if write was successful
	if (nWritten != nRead) {
		if (pOut) {
#ifdef BUILD_WIN32
			DeleteFile(pOut);
#else
			_tremove(pOut);				// Use _tremove for cross-platform compatibility
#endif // BUILD_WIN32
			free_s((void**)&pOut);
		}
		return false;
	}

	if (pszConv)
		*pszConv = pOut;

	return true;
}

// Opens a text file with automatic encoding detection and adaptation
// Return value: Converted temp file path (needs to be freed + deleted by caller) if UTF-16BE, else NULL
// file:        Output pointer to the opened file (CANNOT be NULL)
// encType:     Output detected encoding type
static TCHAR* SmartOpenTextFile(const TCHAR* inputText, FILE** file, EncodingType* encType)
{
	// Validate input parameters
	if (IsStrEmpty(inputText) || !file)
		return NULL;

	// Initialize output parameters
	*file = NULL;
	if (encType)
		*encType = ENCODING_ERROR;

	bool bConverted = false;
	TCHAR* convertedFilePath = NULL;
	const TCHAR* pszReadMode = NULL;
	const TCHAR* pszTargetFile = inputText;

	// Detect file encoding type
	EncodingType detectedType = DetectEncoding(inputText);
	if (encType)
		*encType = detectedType;

	// Configure file open mode based on encoding
	switch (detectedType) {
	case ENCODING_ANSI:
		pszReadMode = _T("rt");
		break;

	case ENCODING_UTF8:
	case ENCODING_UTF8_BOM:
		pszReadMode = _T("rt, ccs=UTF-8");
		break;

	case ENCODING_UTF16_LE:
		pszReadMode = _T("rt, ccs=UTF-16LE");
		break;

	case ENCODING_UTF16_BE:
		// Windows does not support UTF-16BE natively, convert to UTF-16LE first
		if (!Utf16beToUtf16le(inputText, &convertedFilePath))
			return NULL;

		bConverted = true;
		pszReadMode = _T("rt, ccs=UTF-16LE");
		break;

	default:
		// Unsupported encoding type
		return NULL;
	}

	// Use converted temporary file if conversion was performed
	if (bConverted && convertedFilePath)
		pszTargetFile = convertedFilePath;

	// Open the file with the correct encoding mode
	FILE* fp = _tfopen(pszTargetFile, pszReadMode);
	if (!fp) {
		// Clean up allocated memory if file opening failed
		free_s((void**)&convertedFilePath);
		return NULL;
	}

	// Return the valid file pointer to the caller
	*file = fp;

	// Return converted file path (caller must free and delete it)
	return convertedFilePath;
}

// Safely closes the text file opened by SmartOpenTextFile, cleans up temporary files and memory
static void SafeCloseTextFile(FILE** file, TCHAR** tempText)
{
	// Close file pointer safely
	if (file && *file) {
		fclose(*file);
		*file = NULL;
	}

	// Delete temporary converted file and free memory safely
	if (tempText && *tempText) {
#ifdef BUILD_WIN32
		DeleteFile(*tempText);
#else
		_tremove(*tempText);					// Use _tremove for cross-platform compatibility
#endif // BUILD_WIN32
		free_s((void**)tempText);
	}
}

// ==================== Safe Process Text File (Wrapper) ====================
// Automatically opens file, invokes callback, then cleans up
void SafeProcessTextFile(const TCHAR* inputText, void (*TextFileProcess)(const TCHAR*, FILE*, void*), void* userData)
{
	if (IsStrEmpty(inputText) || !TextFileProcess)
		return;

	FILE*  file = NULL;
	TCHAR* tempFile = NULL;

	// Open file with auto encoding detection; encoding output is NULL
	tempFile = SmartOpenTextFile(inputText, &file, NULL);

	// Invoke callback regardless of open result (file may be NULL)
	TextFileProcess(inputText, file, userData);

	// Always clean up resources
	SafeCloseTextFile(&file, &tempFile);
}
