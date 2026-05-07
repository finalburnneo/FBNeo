#include "burner.h"
#include "burnint.h"
#include "m68000_intf.h"
#include <shlobj.h>

// d_cps1.cpp
#define CPS1_68K_PROGRAM_BYTESWAP			1
#define CPS1_68K_PROGRAM_NO_BYTESWAP		2
#define CPS1_Z80_PROGRAM					3
#define CPS1_TILES							4
#define CPS1_OKIM6295_SAMPLES				5
#define CPS1_QSOUND_SAMPLES					6
#define CPS1_PIC							7
#define CPS1_EXTRA_TILES_SF2EBBL_400000		8
#define CPS1_EXTRA_TILES_400000				9
#define CPS1_EXTRA_TILES_SF2KORYU_400000	10
#define CPS1_EXTRA_TILES_SF2B_400000		11
#define CPS1_EXTRA_TILES_SF2MKOT_400000		12

// d_cps2.cpp
#define CPS2_PRG_68K						1
#define CPS2_PRG_68K_SIMM					2
#define CPS2_PRG_68K_XOR_TABLE				3
#define CPS2_GFX							5
#define CPS2_GFX_SIMM						6
#define CPS2_GFX_SPLIT4						7
#define CPS2_GFX_SPLIT8						8
#define CPS2_GFX_19XXJ						9
#define CPS2_PRG_Z80						10
#define CPS2_QSND							12
#define CPS2_QSND_SIMM						13
#define CPS2_QSND_SIMM_BYTESWAP				14
#define CPS2_ENCRYPTION_KEY					15

// gal.h
#define GAL_ROM_Z80_PROG1					1
#define GAL_ROM_Z80_PROG2					2
#define GAL_ROM_Z80_PROG3					3
#define GAL_ROM_TILES_SHARED				4
#define GAL_ROM_TILES_CHARS					5
#define GAL_ROM_TILES_SPRITES				6
#define GAL_ROM_PROM						7
#define GAL_ROM_S2650_PROG1					8

// megadrive.h
#define SEGA_MD_ROM_LOAD_NORMAL								0x10
#define SEGA_MD_ROM_LOAD16_WORD_SWAP						0x20
#define SEGA_MD_ROM_LOAD16_BYTE								0x30
#define SEGA_MD_ROM_LOAD16_WORD_SWAP_CONTINUE_040000_100000	0x40
#define SEGA_MD_ROM_LOAD_NORMAL_CONTINUE_020000_080000		0x50
#define SEGA_MD_ROM_OFFS_000000								0x01
#define SEGA_MD_ROM_OFFS_000001								0x02
#define SEGA_MD_ROM_OFFS_020000								0x03
#define SEGA_MD_ROM_OFFS_080000								0x04
#define SEGA_MD_ROM_OFFS_100000								0x05
#define SEGA_MD_ROM_OFFS_100001								0x06
#define SEGA_MD_ROM_OFFS_200000								0x07
#define SEGA_MD_ROM_OFFS_300000								0x08
#define SEGA_MD_ROM_RELOAD_200000_200000					0x09
#define SEGA_MD_ROM_RELOAD_100000_300000					0x0a
#define SEGA_MD_ARCADE_SUNMIXING							(0x4000)

// sys16.h
#define SYS16_ROM_PROG_FLAT					25
#define SYS16_ROM_PROG						1
#define SYS16_ROM_TILES						2
#define SYS16_ROM_SPRITES					3
#define SYS16_ROM_Z80PROG					4
#define SYS16_ROM_KEY						5
#define SYS16_ROM_7751PROG					6
#define SYS16_ROM_7751DATA					7
#define SYS16_ROM_UPD7759DATA				8
#define SYS16_ROM_PROG2						9
#define SYS16_ROM_ROAD						10
#define SYS16_ROM_PCMDATA					11
#define SYS16_ROM_Z80PROG2					12
#define SYS16_ROM_Z80PROG3					13
#define SYS16_ROM_Z80PROG4					14
#define SYS16_ROM_PCM2DATA					15
#define SYS16_ROM_PROM 						16
#define SYS16_ROM_PROG3						17
#define SYS16_ROM_SPRITES2					18
#define SYS16_ROM_RF5C68DATA				19
#define SYS16_ROM_I8751						20
#define SYS16_ROM_MSM6295					21
#define SYS16_ROM_TILES_20000				22

// taito.h
#define TAITO_68KROM1						1
#define TAITO_68KROM1_BYTESWAP				2
#define TAITO_68KROM1_BYTESWAP_JUMPING		3
#define TAITO_68KROM1_BYTESWAP32			4
#define TAITO_68KROM2						5
#define TAITO_68KROM2_BYTESWAP				6
#define TAITO_68KROM3						7
#define TAITO_68KROM3_BYTESWAP				8
#define TAITO_Z80ROM1						9
#define TAITO_Z80ROM2						10
#define TAITO_CHARS							11
#define TAITO_CHARS_BYTESWAP				12
#define TAITO_CHARSB						13
#define TAITO_CHARSB_BYTESWAP				14
#define TAITO_SPRITESA						15
#define TAITO_SPRITESA_BYTESWAP				16
#define TAITO_SPRITESA_BYTESWAP32			17
#define TAITO_SPRITESA_TOPSPEED				18
#define TAITO_SPRITESB						19
#define TAITO_SPRITESB_BYTESWAP				20
#define TAITO_SPRITESB_BYTESWAP32			21
#define TAITO_ROAD							22
#define TAITO_SPRITEMAP						23
#define TAITO_YM2610A						24
#define TAITO_YM2610B						25
#define TAITO_MSM5205						26
#define TAITO_MSM5205_BYTESWAP				27
#define TAITO_CHARS_PIVOT					28
#define TAITO_MSM6295						29
#define TAITO_ES5505						30
#define TAITO_ES5505_BYTESWAP				31
#define TAITO_DEFAULT_EEPROM				32
#define TAITO_CHARS_BYTESWAP32				33
#define TAITO_CCHIP_BIOS					34
#define TAITO_CCHIP_EEPROM					35

#ifndef ERANGE
  #define ERANGE 34
#endif

// All items passed inspection: 0011 1111 (0x3f)
struct RomDataDrv {					// status    err codes:
	char* pszZipName;				// 0000 0001 0000 0000
	char* pszParent;				// 0000 0010 0000 0000
	char* pszDrvName;				// 0000 0000 0000 0000
	char* pszDate;					// 0000 0000 0000 0000
	char* pszExtraRom;				// 0000 0000 0000 0000
	TCHAR* pszFullName;				// 0000 0100 0000 0000
	struct BurnDriver*  pDriver;	// 0000 1000 0000 0000
	struct BurnRomInfo* pRomInfo;	// 0001 0000 0000 0000
	UINT32 nRomInfoCount;			// 0010 0000 0000 0000
};

static struct RomDataDrv** pRDDrv = NULL;
static UINT32 nRDDrvCount = 0U;

struct RDMacroMap {
	const TCHAR* pszDRMacro;
	const UINT32 nRDMacro;
};

bool  bRDListScanSub = false;

#define X(a) { _T(#a), a }
static const RDMacroMap RDMacroTable[] = {
	X(BRF_PRG),
	X(BRF_GRA),
	X(BRF_SND),
	X(BRF_ESS),
	X(BRF_BIOS),
	X(BRF_SELECT),
	X(BRF_OPT),
	X(BRF_NODUMP),
	X(CPS1_68K_PROGRAM_BYTESWAP),
	X(CPS1_68K_PROGRAM_NO_BYTESWAP),
	X(CPS1_Z80_PROGRAM),
	X(CPS1_TILES),
	X(CPS1_OKIM6295_SAMPLES),
	X(CPS1_QSOUND_SAMPLES),
	X(CPS1_PIC),
	X(CPS1_EXTRA_TILES_SF2EBBL_400000),
	X(CPS1_EXTRA_TILES_400000),
	X(CPS1_EXTRA_TILES_SF2KORYU_400000),
	X(CPS1_EXTRA_TILES_SF2B_400000),
	X(CPS1_EXTRA_TILES_SF2MKOT_400000),
	X(CPS2_PRG_68K),
	X(CPS2_PRG_68K_SIMM),
	X(CPS2_PRG_68K_XOR_TABLE),
	X(CPS2_GFX),
	X(CPS2_GFX_SIMM),
	X(CPS2_GFX_SPLIT4),
	X(CPS2_GFX_SPLIT8),
	X(CPS2_GFX_19XXJ),
	X(CPS2_PRG_Z80),
	X(CPS2_QSND),
	X(CPS2_QSND_SIMM),
	X(CPS2_QSND_SIMM_BYTESWAP),
	X(CPS2_ENCRYPTION_KEY),
	X(GAL_ROM_Z80_PROG1),
	X(GAL_ROM_Z80_PROG2),
	X(GAL_ROM_Z80_PROG3),
	X(GAL_ROM_TILES_SHARED),
	X(GAL_ROM_TILES_CHARS),
	X(GAL_ROM_TILES_SPRITES),
	X(GAL_ROM_PROM),
	X(GAL_ROM_S2650_PROG1),
	X(SEGA_MD_ROM_LOAD_NORMAL),
	X(SEGA_MD_ROM_LOAD16_WORD_SWAP),
	X(SEGA_MD_ROM_LOAD16_BYTE),
	X(SEGA_MD_ROM_LOAD16_WORD_SWAP_CONTINUE_040000_100000),
	X(SEGA_MD_ROM_LOAD_NORMAL_CONTINUE_020000_080000),
	X(SEGA_MD_ROM_OFFS_000000),
	X(SEGA_MD_ROM_OFFS_000001),
	X(SEGA_MD_ROM_OFFS_020000),
	X(SEGA_MD_ROM_OFFS_080000),
	X(SEGA_MD_ROM_OFFS_100000),
	X(SEGA_MD_ROM_OFFS_100001),
	X(SEGA_MD_ROM_OFFS_200000),
	X(SEGA_MD_ROM_OFFS_300000),
	X(SEGA_MD_ROM_RELOAD_200000_200000),
	X(SEGA_MD_ROM_RELOAD_100000_300000),
	X(SEGA_MD_ARCADE_SUNMIXING),
	X(SYS16_ROM_PROG_FLAT),
	X(SYS16_ROM_PROG),
	X(SYS16_ROM_TILES),
	X(SYS16_ROM_SPRITES),
	X(SYS16_ROM_Z80PROG),
	X(SYS16_ROM_KEY),
	X(SYS16_ROM_7751PROG),
	X(SYS16_ROM_7751DATA),
	X(SYS16_ROM_UPD7759DATA),
	X(SYS16_ROM_PROG2),
	X(SYS16_ROM_ROAD),
	X(SYS16_ROM_PCMDATA),
	X(SYS16_ROM_Z80PROG2),
	X(SYS16_ROM_Z80PROG3),
	X(SYS16_ROM_Z80PROG4),
	X(SYS16_ROM_PCM2DATA),
	X(SYS16_ROM_PROM),
	X(SYS16_ROM_PROG3),
	X(SYS16_ROM_SPRITES2),
	X(SYS16_ROM_RF5C68DATA),
	X(SYS16_ROM_I8751),
	X(SYS16_ROM_MSM6295),
	X(SYS16_ROM_TILES_20000),
	X(TAITO_68KROM1),
	X(TAITO_68KROM1_BYTESWAP),
	X(TAITO_68KROM1_BYTESWAP_JUMPING),
	X(TAITO_68KROM1_BYTESWAP32),
	X(TAITO_68KROM2),
	X(TAITO_68KROM2_BYTESWAP),
	X(TAITO_68KROM3),
	X(TAITO_68KROM3_BYTESWAP),
	X(TAITO_Z80ROM1),
	X(TAITO_Z80ROM2),
	X(TAITO_CHARS),
	X(TAITO_CHARS_BYTESWAP),
	X(TAITO_CHARSB),
	X(TAITO_CHARSB_BYTESWAP),
	X(TAITO_SPRITESA),
	X(TAITO_SPRITESA_BYTESWAP),
	X(TAITO_SPRITESA_BYTESWAP32),
	X(TAITO_SPRITESA_TOPSPEED),
	X(TAITO_SPRITESB),
	X(TAITO_SPRITESB_BYTESWAP),
	X(TAITO_SPRITESB_BYTESWAP32),
	X(TAITO_ROAD),
	X(TAITO_SPRITEMAP),
	X(TAITO_YM2610A),
	X(TAITO_YM2610B),
	X(TAITO_MSM5205),
	X(TAITO_MSM5205_BYTESWAP),
	X(TAITO_CHARS_PIVOT),
	X(TAITO_MSM6295),
	X(TAITO_ES5505),
	X(TAITO_ES5505_BYTESWAP),
	X(TAITO_DEFAULT_EEPROM),
	X(TAITO_CHARS_BYTESWAP32),
	X(TAITO_CCHIP_BIOS),
	X(TAITO_CCHIP_EEPROM)
};
#undef X

// gal.h
#undef GAL_ROM_Z80_PROG1
#undef GAL_ROM_Z80_PROG2
#undef GAL_ROM_Z80_PROG3
#undef GAL_ROM_TILES_SHARED
#undef GAL_ROM_TILES_CHARS
#undef GAL_ROM_TILES_SPRITES
#undef GAL_ROM_PROM
#undef GAL_ROM_S2650_PROG1

// megadrive.h
#undef SEGA_MD_ROM_LOAD_NORMAL
#undef SEGA_MD_ROM_LOAD16_WORD_SWAP
#undef SEGA_MD_ROM_LOAD16_BYTE
#undef SEGA_MD_ROM_LOAD16_WORD_SWAP_CONTINUE_040000_100000
#undef SEGA_MD_ROM_LOAD_NORMAL_CONTINUE_020000_080000
#undef SEGA_MD_ROM_OFFS_000000
#undef SEGA_MD_ROM_OFFS_000001
#undef SEGA_MD_ROM_OFFS_020000
#undef SEGA_MD_ROM_OFFS_080000
#undef SEGA_MD_ROM_OFFS_100000
#undef SEGA_MD_ROM_OFFS_100001
#undef SEGA_MD_ROM_OFFS_200000
#undef SEGA_MD_ROM_OFFS_300000
#undef SEGA_MD_ROM_RELOAD_200000_200000
#undef SEGA_MD_ROM_RELOAD_100000_300000
#undef SEGA_MD_ARCADE_SUNMIXING

// sys16.h
#undef SYS16_ROM_PROG_FLAT
#undef SYS16_ROM_PROG
#undef SYS16_ROM_TILES
#undef SYS16_ROM_SPRITES
#undef SYS16_ROM_Z80PROG
#undef SYS16_ROM_KEY
#undef SYS16_ROM_7751PROG
#undef SYS16_ROM_7751DATA
#undef SYS16_ROM_UPD7759DATA
#undef SYS16_ROM_PROG2
#undef SYS16_ROM_ROAD
#undef SYS16_ROM_PCMDATA
#undef SYS16_ROM_Z80PROG2
#undef SYS16_ROM_Z80PROG3
#undef SYS16_ROM_Z80PROG4
#undef SYS16_ROM_PCM2DATA
#undef SYS16_ROM_PROM
#undef SYS16_ROM_PROG3
#undef SYS16_ROM_SPRITES2
#undef SYS16_ROM_RF5C68DATA
#undef SYS16_ROM_I8751
#undef SYS16_ROM_MSM6295
#undef SYS16_ROM_TILES_20000

// taito.h
#undef TAITO_68KROM1
#undef TAITO_68KROM1_BYTESWAP
#undef TAITO_68KROM1_BYTESWAP_JUMPING
#undef TAITO_68KROM1_BYTESWAP32
#undef TAITO_68KROM2
#undef TAITO_68KROM2_BYTESWAP
#undef TAITO_68KROM3
#undef TAITO_68KROM3_BYTESWAP
#undef TAITO_Z80ROM1
#undef TAITO_Z80ROM2
#undef TAITO_CHARS
#undef TAITO_CHARS_BYTESWAP
#undef TAITO_CHARSB
#undef TAITO_CHARSB_BYTESWAP
#undef TAITO_SPRITESA
#undef TAITO_SPRITESA_BYTESWAP
#undef TAITO_SPRITESA_BYTESWAP32
#undef TAITO_SPRITESA_TOPSPEED
#undef TAITO_SPRITESB
#undef TAITO_SPRITESB_BYTESWAP
#undef TAITO_SPRITESB_BYTESWAP32
#undef TAITO_ROAD
#undef TAITO_SPRITEMAP
#undef TAITO_YM2610A
#undef TAITO_YM2610B
#undef TAITO_MSM5205
#undef TAITO_MSM5205_BYTESWAP
#undef TAITO_CHARS_PIVOT
#undef TAITO_MSM6295
#undef TAITO_ES5505
#undef TAITO_ES5505_BYTESWAP
#undef TAITO_DEFAULT_EEPROM
#undef TAITO_CHARS_BYTESWAP32
#undef TAITO_CCHIP_BIOS
#undef TAITO_CCHIP_EEPROM

// Check if the active driver is a RomData driver based on the active driver index and driver flags
extern "C" bool IsRomDataDrv()
{
#ifdef BUILD_WIN32
	return (nBurnDrvActive >= nIntlDrvCount && (BurnDrvGetFlags() & BDF_ROMDATA_DRIVER));
#else
	return (NULL != pDataRomDesc);
#endif // BUILD_WIN32
}

// Get the active RomData driver index by subtracting the number of internal drivers from the active driver index
static UINT32 RomDataDrvGetActive()
{
	// Active driver index is greater than or equal to the number of internal drivers, and has the RomData driver flag set
	if (!IsRomDataDrv())
		return ~0U;

	return (nBurnDrvActive - nIntlDrvCount);
}

// Get the driver name from the active RomData driver
extern "C" char* RomDataDrvGetDrvName()
{
#ifdef BUILD_WIN32
	if (!pRDDrv)
		return NULL;									// RomData driver array is not available

	// The RomData driver index is calculated by subtracting the number of internal drivers from the active driver index
	const UINT32 nDataDrvActive = RomDataDrvGetActive();
	if (~0U == nDataDrvActive || nDataDrvActive >= nRDDrvCount || !pRDDrv[nDataDrvActive])
		return NULL;			// Invalid RomData driver index

	return pRDDrv[nDataDrvActive]->pszDrvName;
#else
	return RomdataGetDrvName();
#endif // BUILD_WIN32
}

// Get the extra ROM name from the active RomData driver
static char* RomDataDrvGetExtraRom()
{
	if (!pRDDrv)
		return NULL;									// RomData driver array is not available

	// The RomData driver index is calculated by subtracting the number of internal drivers from the active driver index
	const UINT32 nDataDrvActive = RomDataDrvGetActive();
	if (~0U == nDataDrvActive || nDataDrvActive >= nRDDrvCount || !pRDDrv[nDataDrvActive])
		return NULL;									// Invalid RomData driver index

	return pRDDrv[nDataDrvActive]->pszExtraRom;
}

// Get the ROM info array from the active RomData driver, and optionally set the ROM count if the pointer is provided
extern "C" struct BurnRomInfo* RomDataDrvGetRomInfo(UINT32* pRomCount)
{
#ifdef BUILD_WIN32
	if (!pRDDrv)
		return NULL;									// RomData driver array is not available

	// The RomData driver index is calculated by subtracting the number of internal drivers from the active driver index
	const UINT32 nDataDrvActive = RomDataDrvGetActive();
	if (~0U == nDataDrvActive || nDataDrvActive >= nRDDrvCount || !pRDDrv[nDataDrvActive])
		return NULL;									// Invalid RomData driver index

	if (pRomCount)
		// Set the output ROM count if the pointer is provided
		*pRomCount = pRDDrv[nDataDrvActive]->nRomInfoCount;

	return pRDDrv[nDataDrvActive]->pRomInfo;
#else
	if (!pDataRomDesc)
		return NULL;
	if (pRomCount && pRDI)
		*pRomCount = pRDI->nDescCount;

	return pDataRomDesc;
#endif // BUILD_WIN32
}

// Convert a hexadecimal string to UINT32 with error checking
static bool StrToUint(const TCHAR* str, UINT32* result)
{
	if (IsStrEmpty(str) || !result)
		return false;									// Invalid input parameters

	errno = 0;											// Reset errno before conversion

	TCHAR* endPtr;
	unsigned long value = _tcstoul(str, &endPtr, 16);

	if (str == endPtr)
		return false;									// No digits were found
	if (_T('\0') != *endPtr)
		return false;									// Trailing characters after number
	if (value > UINT_MAX) {
		errno = ERANGE;
		return false;									// Value exceeds UINT32 range
	}

	*result = (UINT32)value;							// Store result
	return true;										// Successful conversion
}

// Convert TCHAR string to ANSI (MBCS) string with safe buffer check
INT32 tchar_to_ansi(const TCHAR* src, char** dst)
{
	// Input validation
	if (IsStrEmpty(src) || !dst)
		return 0;

	*dst      = NULL;
	char* buf = NULL;

#if defined(UNICODE)
	// Get required buffer size
	INT32 size = WideCharToMultiByte(CP_ACP, 0, src, -1, NULL, 0, NULL, NULL);
	if (size <= 0)
		return 0;

	// Allocate temporary buffer
	buf = (char*)malloc(size);
	if (!buf)
		return 0;
	memset(buf, 0, size);

	// Perform conversion
	if (WideCharToMultiByte(CP_ACP, 0, src, -1, buf, size, NULL, NULL) <= 0) {
		free_s((void**)&buf);
		return 0;
	}
#else
	// For ANSI projects, simply copy the string
	buf = strdup(src);
	if (!buf)
		return 0;
#endif
	// Return the result
	* dst = buf;
	return (INT32)strlen(buf);
}

// Convert ANSI string to TCHAR string with safe buffer check
INT32 ansi_to_tchar(const char* src, TCHAR** dst)
{
	// Input validation
	if (IsStrEmptyA(src) || !dst)
		return 0;

	*dst       = NULL;
	TCHAR* buf = NULL;

#if defined(UNICODE)
	// Get required wide character length
	INT32 size = MultiByteToWideChar(CP_ACP, 0, src, -1, NULL, 0);
	if (size <= 0)
		return 0;

	// Allocate memory
	buf = (TCHAR*)malloc(size * sizeof(TCHAR));
	if (!buf)
		return 0;
	memset(buf, 0, size * sizeof(TCHAR));

	// Perform conversion
	if (MultiByteToWideChar(CP_ACP, 0, src, -1, buf, size) <= 0) {
		free_s((void**)&buf);
		return 0;
	}
#else
	// For ANSI projects, simply copy the string
	buf = strdup(src);
	if (!buf)
		return 0;
#endif
	// Return the result
	* dst = buf;
	return (INT32)_tcslen(buf);
}

// Check if a character is a Windows path separator (\ or /)
static inline bool IsPathSeparator(TCHAR c)
{
	// Windows supports both backslash and forward slash
	return (_T('\\') == c) || (_T('/') == c);
}

// Check if the path string ends with a valid path separator
static inline bool ends_with_slash(const TCHAR* s)
{
	// Return false if string is empty
	if (IsStrEmpty(s))
		return false;

	// Get the last character of the string
	UINT32 len = _tcslen(s);
	TCHAR c = s[len - 1];

	// Check if last char is a separator
	return IsPathSeparator(c);
}

// Check if the given path is an absolute Windows path
static inline bool IsAbsolutePath(const TCHAR* path)
{
	if (IsStrEmpty(path))
		return false;

	// Check pattern: C:\... or C:/...
	if (_tcslen(path) >= 3) {
		TCHAR drive = path[0];
		if ((drive >= _T('A') && drive <= _T('Z')) || (drive >= _T('a') && drive <= _T('z'))) {
			if ((_T(':') == path[1]) && ((_T('\\') == path[2]) || (_T('/') == path[2])))
				return true;
		}
	}

	// Check pattern: \\server or //server
	if (_tcslen(path) >= 2) {
		if ((_T('\\') == path[0] || _T('/') == path[0]) && (_T('\\') == path[1] || _T('/') == path[1]))
			return true;
	}

	return false;
}

// Safely combine a directory path and a relative file path
static INT32 PathCombine_s(TCHAR** dest, const TCHAR* dir, const TCHAR* file)
{
	if (NULL == dest)
		return -1;

	*dest = NULL;
	TCHAR* pTemp = NULL;

	INT32 dirLen   = (INT32)_tcslen(IsStrEmpty(dir) ? _T("") : dir);
	INT32 fileLen  = (INT32)_tcslen(IsStrEmpty(file) ? _T("") : file);
	INT32 totalLen = 0;

	// Use absolute file path directly
	if (!IsStrEmpty(file) && IsAbsolutePath(file)) {
		totalLen = fileLen;
		pTemp = (TCHAR*)malloc((totalLen + 1) * sizeof(TCHAR));
		if (NULL == pTemp)
			return -1;

		_tcsncpy(pTemp, file, totalLen);
		pTemp[totalLen] = _T('\0');
		*dest = pTemp;
		return totalLen;
	}

	// Only file, no directory
	if (IsStrEmpty(dir)) {
		totalLen = fileLen;
		pTemp = (TCHAR*)malloc((totalLen + 1) * sizeof(TCHAR));
		if (NULL == pTemp)
			return -1;

		if (totalLen > 0)
			_tcsncpy(pTemp, file, totalLen);

		pTemp[totalLen] = _T('\0');
		*dest = pTemp;
		return totalLen;
	}

	// Only directory, no file
	if (IsStrEmpty(file)) {
		totalLen = dirLen;
		pTemp = (TCHAR*)malloc((totalLen + 1) * sizeof(TCHAR));
		if (NULL == pTemp)
			return -1;

		_tcsncpy(pTemp, dir, dirLen);
		pTemp[totalLen] = _T('\0');
		*dest = pTemp;
		return totalLen;
	}

	// Combine directory + file
	BOOL needSlash = (BOOL)!ends_with_slash(dir);
	totalLen = dirLen + (needSlash ? 1 : 0) + fileLen;

	pTemp = (TCHAR*)malloc((totalLen + 1) * sizeof(TCHAR));
	if (NULL == pTemp)
		return -1;

	_tcsncpy(pTemp, dir, dirLen);

	if (needSlash)
		pTemp[dirLen] = _T('\\');

	_tcsncpy(pTemp + dirLen + (needSlash ? 1 : 0), file, fileLen);
	pTemp[totalLen] = _T('\0');

	*dest = pTemp;
	return totalLen;
}

// Recursive Directory Creation (Supports UNC Paths)
static bool CreateDirectoryRecursive(const TCHAR* szPath)
{
	if (IsStrEmpty(szPath))
		return false;									// Invalid input path

	TCHAR buffer[MAX_PATH] = { 0 };
	TCHAR currentPath[MAX_PATH] = { 0 };

	if (_tcslen(szPath) >= MAX_PATH)
		return false;									// Input path is too long for buffer

	_tcscpy(buffer, szPath);
	UINT32 len = _tcslen(buffer);

	// Handle UNC network paths
	bool bUncPath = (_T('\\') == buffer[0] && _T('\\') == buffer[1]) || (_T('/') == buffer[0] && _T('/') == buffer[1]);

	UINT32 startIndex = 0;
	if (bUncPath) {
		startIndex = 2;
		// Skip server name
		for (UINT32 i = startIndex; i < len; ++i) {
			if (_T('\\' == buffer[i]) || _T('/' == buffer[i])) {
				startIndex = i;
				break;									// Invalid UNC path if we can't find server component
			}
		}
		// Skip share name
		for (UINT32 i = startIndex + 1; i < len; ++i) {
			if (_T('\\' == buffer[i]) || _T('/' == buffer[i])) {
				startIndex = i;
				break;									// Invalid UNC path if we can't find server/share components
			}
		}
	}

	// Normalize path separators
	for (UINT32 i = 0; i < len; ++i) {
		if (_T('/') == buffer[i])
			buffer[i] = _T('\\');
	}

	// Create directories level by level
	for (UINT32 i = startIndex; i <= len; ++i) {
		if (_T('\\') == buffer[i] || _T('\0') == buffer[i]) {
			TCHAR saveChar = buffer[i];
			buffer[i] = _T('\0');

			if (_T('\0') != buffer[0]) {
				_tcscpy(currentPath, buffer);
				DWORD dwAttrib = GetFileAttributes(currentPath);

				if (INVALID_FILE_ATTRIBUTES == dwAttrib) {
					if (!CreateDirectory(currentPath, NULL)) {
						DWORD err = GetLastError();
						// Failed to create directory, likely due to read-only disk or permission error
						if (ERROR_ALREADY_EXISTS != err)
							return false;	
					}
				} else if (!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
					return false;						// Path exists but is not a directory
				}
			}
			buffer[i] = saveChar;
		}
	}
	return true;										// Successfully created all directories (or they already exist)
}

// Generate a unique file name in the specified directory based on the source file name
static INT32 GetUniqueFileName(const TCHAR* szDir, const TCHAR* szSrcFile, TCHAR** szOutPath)
{
	// Validate input pointers
	if (IsStrEmpty(szDir) || IsStrEmpty(szSrcFile) || !szOutPath)
		return -1;

	*szOutPath = NULL;

	// Locate the last slash to extract pure filename from full path
	const TCHAR* name = szSrcFile;
	const TCHAR* bs   = _tcsrchr(szSrcFile, _T('\\'));
	const TCHAR* sl   = _tcsrchr(szSrcFile, _T('/'));

	// Extract filename only (remove directory part)
	if (bs > sl)
		name = bs + 1;
	else if (sl)
		name = sl + 1;

	TCHAR base[MAX_PATH] = { 0 };
	TCHAR ext[MAX_PATH] = { 0 };

	// Safe copy filename to buffer and ensure null termination
	_tcsncpy(base, name, MAX_PATH - 1);
	base[MAX_PATH - 1] = _T('\0');

	// Split filename into base name and extension
	TCHAR* dot = _tcsrchr(base, _T('.'));
	if (dot) {
		_tcsncpy(ext, dot, MAX_PATH - 1);
		ext[MAX_PATH - 1] = _T('\0');
		*dot = _T('\0');
	}

	// Try up to 10000 variations to find a non-existing file
	for (INT32 i = 0; i < 10000; i++) {
		TCHAR buf[MAX_PATH] = { 0 };

		// First try: original filename
		if (i == 0)
			_sntprintf(buf, MAX_PATH - 1, _T("%s%s"), base, ext);
		// Subsequent tries: filename_1, filename_2, ...
		else
			_sntprintf(buf, MAX_PATH - 1, _T("%s_%d%s"), base, i, ext);
		buf[MAX_PATH - 1] = _T('\0');

		// Build full path
		TCHAR* fullPath = NULL;
		if (PathCombine_s(&fullPath, szDir, buf) < 0)
			continue;

		// Check if file does NOT exist
		if (GetFileAttributes(fullPath) == INVALID_FILE_ATTRIBUTES) {
			// Return unique path
			*szOutPath = fullPath;
			return (INT32)_tcslen(fullPath);
		}

		// Free temporary path and try next
		free_s((void**)&fullPath);
	}

	// No available filename found
	return -1;
}

// Extract directory from file path by finding the last slash and truncating the file name
static INT32 GetDirectoryFromFilePath(const TCHAR* szFilePath, TCHAR** pszDir)
{
	// Check input parameters
	if (IsStrEmpty(szFilePath) || !pszDir)
		return -1;

	*pszDir = NULL;
	TCHAR szTemp[MAX_PATH] = { 0 };

	// Safe copy to local buffer
	_tcsncpy(szTemp, szFilePath, MAX_PATH - 1);
	szTemp[MAX_PATH - 1] = _T('\0');

	for (INT32 i = 0; szTemp[i] != _T('\0'); i++) {
		if (szTemp[i] == _T('/'))
			szTemp[i] =  _T('\\');
	}

	// Find LAST backslash (now all are \)
	TCHAR* pSlash = _tcsrchr(szTemp, _T('\\'));

	// Truncate string at slash to remove filename
	if (pSlash)
		*pSlash = _T('\0');
	else
		// No directory found
		szTemp[0] = _T('\0');

	// Allocate buffer and return directory path
	INT32 len   = (INT32)_tcslen(szTemp);
	TCHAR* pBuf = (TCHAR*)malloc((len + 1) * sizeof(TCHAR));
	if (!pBuf)
		return -1;

	_tcsncpy(pBuf, szTemp, len);
	pBuf[len] = _T('\0');
	*pszDir   = pBuf;

	return len;
}

// Normalize to absolute path, unified slashes, no trailing slash, lowercase
static INT32 NormalizeAbsolute(const TCHAR* szInput, TCHAR** pszOutput)
{
	// Validate input parameters
	if (IsStrEmpty(szInput) || (!pszOutput))
		return -1;

	*pszOutput = NULL;
	TCHAR szTemp[MAX_PATH] = { 0 };
	TCHAR szAbs[MAX_PATH]  = { 0 };

	// Safely copy input to local buffer
	_tcsncpy(szTemp, szInput, MAX_PATH - 1);
	szTemp[MAX_PATH - 1] = _T('\0');

	// Unify all slashes to Windows backslash
	for (INT32 i = 0; szTemp[i] != _T('\0'); i++) {
		if (_T('/') == szTemp[i])
			szTemp[i] = _T('\\');
	}

	// Convert relative path to full absolute path
	if (0 == GetFullPathName(szTemp, MAX_PATH, szAbs, NULL))
		return -1;

	// Remove trailing backslash for consistent path comparison
	INT32 len = (INT32)_tcslen(szAbs);
	while ((len > 0) && (_T('\\') == szAbs[len - 1]))
		szAbs[--len] = _T('\0');

	// Convert to lowercase for case-insensitive comparison
	_tcslwr(szAbs);
	len = (INT32)_tcslen(szAbs);

	// Allocate final buffer and return
	TCHAR* pBuf = (TCHAR*)malloc((len + 1) * sizeof(TCHAR));
	if (NULL == pBuf)
		return -1;

	_tcsncpy(pBuf, szAbs, len);
	pBuf[len] = _T('\0');
	*pszOutput = pBuf;

	return len;
}

// Get the directory path of the current running executable
static INT32 GetAppDirectory(TCHAR** pszAppDir)
{
	if (!pszAppDir)
		return -1;

	*pszAppDir = NULL;
	TCHAR szExePath[MAX_PATH] = { 0 };

	if (0 == GetModuleFileName(NULL, szExePath, MAX_PATH))
		return -1;

	TCHAR* pSlash = _tcsrchr(szExePath, _T('\\'));
	if (!pSlash)
		pSlash = _tcsrchr(szExePath, _T('/'));

	if (!pSlash)
		return -1;

	INT32 dirLen = (INT32)(pSlash - szExePath);
	TCHAR* pDir = (TCHAR*)malloc((dirLen + 1) * sizeof(TCHAR));
	if (!pDir)
		return -1;

	_tcsncpy(pDir, szExePath, dirLen);
	pDir[dirLen] = _T('\0');
	*pszAppDir = pDir;

	return dirLen;
}

//===================================================================================
// Get full absolute path of ROM data:
// 1. If input is absolute, use it directly
// 2. If relative, combine with current EXE directory and normalize
//===================================================================================
static INT32 GetRomDataFullPath(TCHAR** pszFullPath)
{
	// Validate output pointer
	if (NULL == pszFullPath)
		return -1;

	*pszFullPath = NULL;

	// Use path directly if it's already absolute
	if (IsAbsolutePath(szAppRomdataPath)) {
		INT32 len   = (INT32)_tcslen(szAppRomdataPath);
		TCHAR* pBuf = (TCHAR*)malloc((len + 1) * sizeof(TCHAR));

		if (!pBuf)
			return -1;

		_tcsncpy(pBuf, szAppRomdataPath, len);
		pBuf[len] = _T('\0');
		*pszFullPath = pBuf;

		return len;
	}

	// For relative path: combine with EXE directory
	TCHAR* szAppDir = NULL;
	if (GetAppDirectory(&szAppDir) < 0)
		return -1;

	TCHAR* pCombined = NULL;
	INT32 combineRet = PathCombine_s(&pCombined, szAppDir, szAppRomdataPath);

	// No longer need app directory
	free_s((void**)&szAppDir);

	// Check combine result
	if (combineRet < 0 || !pCombined)
		return -1;

	// Normalize to final absolute path (handles ../ ./ .\ ..\ automatically)
	INT32 normRet = NormalizeAbsolute(pCombined, pszFullPath);
	free_s((void**)&pCombined);

	return normRet;
}

// Copy File With Auto-Rename & Skip Same-Directory Copy
static bool RomDataAutoBackup(const TCHAR* szSrcFilePath, const TCHAR* szDestDir)
{
	if (IsStrEmpty(szSrcFilePath) || IsStrEmpty(szDestDir))
		return false;

	// Get source file directory
	TCHAR* szRawDir = NULL;
	if (GetDirectoryFromFilePath(szSrcFilePath, &szRawDir) < 0)
		return false;

	// Normalize source directory
	TCHAR* szSrcAbs = NULL;
	if (NormalizeAbsolute(szRawDir, &szSrcAbs) < 0) {
		free_s((void**)&szRawDir);
		return false;
	}
	free_s((void**)&szRawDir);

	// Get ROM directory
	TCHAR* szRomPath = NULL;
	if (GetRomDataFullPath(&szRomPath) < 0) {
		free_s((void**)&szSrcAbs);
		return false;
	}

	// Normalize ROM directory
	TCHAR* szRomAbs = NULL;
	if (NormalizeAbsolute(szRomPath, &szRomAbs) < 0) {
		free_s((void**)&szSrcAbs);
		free_s((void**)&szRomPath);
		return false;
	}
	free_s((void**)&szRomPath);

	// Skip backup if already in ROM directory
	bool skip = (0 == _tcscmp(szSrcAbs, szRomAbs));
	free_s((void**)&szSrcAbs);
	free_s((void**)&szRomAbs);

	if (skip)
		return true;

	// Create target directory
	if (!CreateDirectoryRecursive(szDestDir))
		return false;

	// Get unique backup path
	TCHAR* szDestPath = NULL;
	if (GetUniqueFileName(szDestDir, szSrcFilePath, &szDestPath) < 0)
		return false;

	// Perform backup
	BOOL bSuccess = CopyFile(szSrcFilePath, szDestPath, FALSE);
	free_s((void**)&szDestPath);
	return (bSuccess != FALSE);
}

// ==================== Cleanup Functions ====================
// err is the error code to return after cleanup
static INT32 romdata_rominfo_clearup(FILE* fp, struct BurnRomInfo** pri, UINT32 pri_count, INT32 err)
{
	if (pri) {
		for (UINT32 i = 0; i < pri_count; i++) {
			if (pri[i]) {
				free_s((void**)&pri[i]->szName);
				free_s((void**)&pri[i]);
			}
		}
		free(pri);
	}
	if (fp)
		fclose(fp);

	return err;
}

// RomDataDrv structure cleanup
static void romdata_drv_clearup(struct RomDataDrv* drv)
{
	if (!drv)
		return;

	free_s((void**)&drv->pszZipName);
	free_s((void**)&drv->pszParent);
	free_s((void**)&drv->pszDrvName);
	free_s((void**)&drv->pszDate);
	free_s((void**)&drv->pszFullName);
	free_s((void**)&drv->pDriver);
	free_s((void**)&drv->pszExtraRom);

	free_s((void**)&drv->pDriver);

	if (drv->pRomInfo) {
		for (UINT32 i = 0; i < drv->nRomInfoCount; i++) {
			free_s((void**)&drv->pRomInfo[i].szName);
		}
		free(drv->pRomInfo);
	}
	free_s((void**)&drv);
}

bool RomDataExportTemplate(HWND hWnd, const INT32 nDrvSelect)
{
	if (-1 == nDrvSelect)
		return false;

	TCHAR strFile[MAX_PATH] = { 0 };
	_sntprintf(strFile, MAX_PATH, _T("template_%s.dat"), BurnDrvGetText(DRV_NAME));

	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = hWnd;
	ofn.lpstrFilter     = _T("RomData (*.dat)\0*.dat\0");;
	ofn.lpstrFile       = strFile;
	ofn.nMaxFile        = MAX_PATH;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags           = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

	if (!GetSaveFileName(&ofn))
		return false;

	FILE* fp = _tfopen(strFile, _T("wb"));
	if (!fp)
		return false;

	_ftprintf(fp, _T("// RomData template for FinalBurn Neo\n\n"));
	_ftprintf(fp, _T("// The *** in the ZipName field cannot be empty and cannot be duplicated with DrvName\n"));
	_ftprintf(fp, _T("ZipName: ***\n"));
	_ftprintf(fp, _T("DrvName: %s\n"),        BurnDrvGetText(DRV_NAME));
	_ftprintf(fp, _T("Date: %s\n"),           BurnDrvGetText(DRV_DATE));
	_ftprintf(fp, _T("FullName: \"%s\"\n\n"), BurnDrvGetText(DRV_FULLNAME));
	_ftprintf(fp, _T("// Name\t\tLen\t\tCrc32\t\tType\n"));

	char* romName = NULL;
	for (INT32 i = 0; !BurnDrvGetRomName(&romName, i, 0); i++) {
		struct BurnRomInfo ri = { 0 };
		BurnDrvGetRomInfo(&ri, i);

		if (IsStrEmptyA(romName))
			continue;
		if ((ri.nType & BRF_BIOS) && (i >= 0x80))
			continue;

		_ftprintf(fp, _T("\"%hs\",\t0x%08x,\t0x%08x,\t0x%08x\n"), romName, ri.nLen, ri.nCrc, ri.nType);
	}

	fclose(fp);
	return true;
}

static UINT32 RDGetRomsType(const char* pszDrvName, const UINT32 nBaseType, const INT32 nMinIdx, const INT32 nMaxIdx)
{
	char* pRomName;
	struct BurnRomInfo ri = { 0 };
	const UINT32 nOldDrvSel = nBurnDrvActive;			// Backup

	nBurnDrvActive = BurnDrvGetIndex(pszDrvName);

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & nBaseType) && ((ri.nType & 0x0f) >= nMinIdx) && ((ri.nType & 0x0f) <= nMaxIdx)) {
			nBurnDrvActive = nOldDrvSel;				// Restore
			return ri.nType;
		}
	}

	nBurnDrvActive = nOldDrvSel;						// Restore
	return nBaseType;
}

// Consoles will automatically get the Type of the ROMs
static UINT32 RDGetConsoleRomsType(const char* pszDrvName)
{
	char* pRomName = NULL;
	struct BurnRomInfo ri = { 0 };
	const UINT32 nOldDrvSel = nBurnDrvActive;			// Backup

	nBurnDrvActive = BurnDrvGetIndex(pszDrvName);

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if (ri.nType & BRF_PRG) {
			nBurnDrvActive = nOldDrvSel;				// Restore
			return ri.nType;
		}
	}

	nBurnDrvActive = nOldDrvSel;						// Restore
	return BRF_PRG;
}

// FBNeo style is preferred, universal solutions are not recommended
static UINT32 RDGetUniversalRomsType(const char* pszDrvName, const TCHAR* pszMask)
{
	UINT32 nBaseType = 0;

	if (     0 == _tcsicmp(pszMask, _T("Program"))  || 0 == _tcsicmp(pszMask, _T("PRG1")))
		nBaseType = BRF_PRG;
	else if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA1")))
		nBaseType = BRF_GRA;
	else if (0 == _tcsicmp(pszMask, _T("Z80"))      || 0 == _tcsicmp(pszMask, _T("SNDA")))
		nBaseType = BRF_PRG;
	else if (0 == _tcsicmp(pszMask, _T("Samples"))  || 0 == _tcsicmp(pszMask, _T("SND1")))
		nBaseType = BRF_SND;
	else if (0 == _tcsicmp(pszMask, _T("BoardPLD")) || 0 == _tcsicmp(pszMask, _T("OPT1")))
		nBaseType = BRF_OPT;

	char* pRomName = NULL;
	struct BurnRomInfo ri = { 0 };
	const UINT32 nOldDrvSel = nBurnDrvActive;			// Backup

	nBurnDrvActive = BurnDrvGetIndex(pszDrvName);

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if (ri.nType & nBaseType) {
			nBurnDrvActive = nOldDrvSel;				// Restore
			return ri.nType;
		}
	}

	nBurnDrvActive = nOldDrvSel;						// Restore
	return nBaseType;
}

static UINT32 RDGetCps1RomsType(const char* pszDrvName, const TCHAR* pszMask, const UINT32 nPrgLen)
{
	INT32 nMinIdx = 0, nMaxIdx = 0;
	UINT32 nBaseType = 0;

	if (       0 == _tcsicmp(pszMask, _T("Program"))  || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nMinIdx = CPS1_68K_PROGRAM_BYTESWAP, nMaxIdx = CPS1_68K_PROGRAM_NO_BYTESWAP;
		nBaseType = BRF_PRG;
	} else if (0 == _tcsicmp(pszMask, _T("ProgramS")) || 0 == _tcsicmp(pszMask, _T("PRGS"))) {
		return (BRF_ESS | BRF_PRG | CPS1_68K_PROGRAM_BYTESWAP);
	} else if (0 == _tcsicmp(pszMask, _T("ProgramN")) || 0 == _tcsicmp(pszMask, _T("PRGN"))) {
		return (BRF_ESS | BRF_PRG | CPS1_68K_PROGRAM_NO_BYTESWAP);
	} else if (0 == _tcsicmp(pszMask, _T("Z80"))      || 0 == _tcsicmp(pszMask, _T("SNDA"))) {
		nMinIdx = nMaxIdx = CPS1_Z80_PROGRAM;
		nBaseType = BRF_PRG;
	} else if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nMinIdx = nMaxIdx = CPS1_TILES;
		nBaseType = BRF_GRA;
	} else if (0 == _tcsicmp(pszMask, _T("Samples"))  || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nMinIdx = CPS1_OKIM6295_SAMPLES, nMaxIdx = CPS1_QSOUND_SAMPLES;
		nBaseType = BRF_SND;
	} else if (0 == _tcsicmp(pszMask, _T("Pic"))      || 0 == _tcsicmp(pszMask, _T("SNDB"))) {
		nMinIdx = nMaxIdx = CPS1_PIC;
		nBaseType = BRF_PRG;
	} else if (0 == _tcsicmp(pszMask, _T("Tiles"))    || 0 == _tcsicmp(pszMask, _T("Extras")) || 0 == _tcsicmp(pszMask, _T("GRA2"))) {
		nMinIdx = CPS1_EXTRA_TILES_SF2EBBL_400000, nMaxIdx = CPS1_EXTRA_TILES_SF2MKOT_400000;
		nBaseType = BRF_GRA;
	} else if (0 == _tcsicmp(pszMask, _T("BoardPLD")) || 0 == _tcsicmp(pszMask, _T("OPT1"))) {
		return BRF_OPT;
	}

	char* pRomName = NULL;
	struct BurnRomInfo ri = { 0 };
	const UINT32 nOldDrvSel = nBurnDrvActive;			// Backup

	nBurnDrvActive = BurnDrvGetIndex(pszDrvName);

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);
		if ((ri.nType & nBaseType) && ((ri.nType & 0x0f) >= nMinIdx) && ((ri.nType & 0x0f) <= nMaxIdx)) {
			if (nPrgLen > 0 && (CPS1_68K_PROGRAM_BYTESWAP == nMinIdx || CPS1_68K_PROGRAM_NO_BYTESWAP == nMaxIdx)) {
				if (     nPrgLen >= 0x40000 && (CPS1_68K_PROGRAM_BYTESWAP   == (ri.nType & 0x0f)))
					ri.nType = (ri.nType & ~CPS1_68K_PROGRAM_BYTESWAP)    | CPS1_68K_PROGRAM_NO_BYTESWAP;
				else if (nPrgLen < 0x40000 && (CPS1_68K_PROGRAM_NO_BYTESWAP == (ri.nType & 0x0f)))
					ri.nType = (ri.nType & ~CPS1_68K_PROGRAM_NO_BYTESWAP) | CPS1_68K_PROGRAM_BYTESWAP;
			}
			nBurnDrvActive = nOldDrvSel;				// Restore
			return ri.nType;
		}
	}
	nBurnDrvActive = nOldDrvSel;						// Restore
	return 0;
}

// d_cps1.cpp
#undef CPS1_68K_PROGRAM_BYTESWAP
#undef CPS1_68K_PROGRAM_NO_BYTESWAP
#undef CPS1_Z80_PROGRAM
#undef CPS1_TILES
#undef CPS1_OKIM6295_SAMPLES
#undef CPS1_QSOUND_SAMPLES
#undef CPS1_PIC
#undef CPS1_EXTRA_TILES_SF2EBBL_400000
#undef CPS1_EXTRA_TILES_400000
#undef CPS1_EXTRA_TILES_SF2KORYU_400000
#undef CPS1_EXTRA_TILES_SF2B_400000
#undef CPS1_EXTRA_TILES_SF2MKOT_400000

static UINT32 RDGetCps2RomsType(const char* pszDrvName, const TCHAR* pszMask)
{
	INT32 nMinIdx = 0, nMaxIdx = 0;
	UINT32 nBaseType = 0;

	if (       0 == _tcsicmp(pszMask, _T("Program"))    || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nMinIdx = CPS2_PRG_68K, nMaxIdx = CPS2_PRG_68K_XOR_TABLE;
		nBaseType = BRF_PRG;
	} else if (0 == _tcsicmp(pszMask, _T("Graphics"))   || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nMinIdx = CPS2_GFX, nMaxIdx = CPS2_GFX_19XXJ;
		nBaseType = BRF_GRA;
	} else if (0 == _tcsicmp(pszMask, _T("Z80"))        || 0 == _tcsicmp(pszMask, _T("SNDA"))) {
		nMinIdx = nMaxIdx = CPS2_PRG_Z80;
		nBaseType = BRF_PRG;
	} else if (0 == _tcsicmp(pszMask, _T("Samples"))    || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nMinIdx = CPS2_QSND, nMaxIdx = CPS2_QSND_SIMM_BYTESWAP;
		nBaseType = BRF_SND;
	} else if (0 == _tcsicmp(pszMask, _T("Decryption")) || 0 == _tcsicmp(pszMask, _T("KEY1"))) {
		return CPS2_ENCRYPTION_KEY;
	}

	return RDGetRomsType(pszDrvName, nBaseType, nMinIdx, nMaxIdx);
}

// d_cps2.cpp
#undef CPS2_PRG_68K
#undef CPS2_PRG_68K_SIMM
#undef CPS2_PRG_68K_XOR_TABLE
#undef CPS2_GFX
#undef CPS2_GFX_SIMM
#undef CPS2_GFX_SPLIT4
#undef CPS2_GFX_SPLIT8
#undef CPS2_GFX_19XXJ
#undef CPS2_PRG_Z80
#undef CPS2_QSND
#undef CPS2_QSND_SIMM
#undef CPS2_QSND_SIMM_BYTESWAP
#undef CPS2_ENCRYPTION_KEY

static UINT32 RDGetCps3RomsType(const TCHAR* pszMask)
{
	if (     0 == _tcsicmp(pszMask, _T("Bios")))
		return (BRF_ESS | BRF_BIOS);
	else if (0 == _tcsicmp(pszMask, _T("Program"))  || 0 == _tcsicmp(pszMask, _T("PRG1")))
		return (BRF_ESS | BRF_PRG);
	else if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA1")))
		return BRF_GRA;

	return 0;
}

static INT32 RDGetPgmRomsType(const char* pszDrvName, const TCHAR* pszMask)
{
	INT32 nMinIdx = 0, nMaxIdx = 0;
	UINT32 nBaseType = 0;

	if (       0 == _tcsicmp(pszMask, _T("Program"))       || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nMinIdx = nMaxIdx = 1;
		nBaseType = BRF_PRG;
	} else if (0 == _tcsicmp(pszMask, _T("Tile"))          || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nMinIdx = nMaxIdx = 2;
		nBaseType = BRF_GRA;
	} else if (0 == _tcsicmp(pszMask, _T("SpriteData"))    || 0 == _tcsicmp(pszMask, _T("GRA2"))) {
		nMinIdx = nMaxIdx = 3;
		nBaseType = BRF_GRA;
	} else if (0 == _tcsicmp(pszMask, _T("SpriteMasks"))   || 0 == _tcsicmp(pszMask, _T("GRA3"))) {
		nMinIdx = nMaxIdx = 4;
		nBaseType = BRF_GRA;
	} else if (0 == _tcsicmp(pszMask, _T("Samples"))       || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nMinIdx = nMaxIdx = 5;
		nBaseType = BRF_SND;
	} else if (0 == _tcsicmp(pszMask, _T("InternalARM7"))  || 0 == _tcsicmp(pszMask, _T("PRG2"))) {
		nMinIdx = nMaxIdx = 7;
		nBaseType = BRF_PRG;
	} else if (0 == _tcsicmp(pszMask, _T("Ramdump"))) {
		//What's the point of existing when the rom length is 0 and the Nodump marker won't load?
		return (7 | BRF_PRG | BRF_ESS | BRF_NODUMP);
	} else if (0 == _tcsicmp(pszMask, _T("ExternalARM7"))  || 0 == _tcsicmp(pszMask, _T("PRG3"))) {
		nMinIdx = nMaxIdx = 8;
		nBaseType = BRF_PRG;
	} else if (0 == _tcsicmp(pszMask, _T("ProtectionRom")) || 0 == _tcsicmp(pszMask, _T("PRG4"))) {
		nMinIdx = nMaxIdx = 9;
		nBaseType = BRF_PRG;
	}

	return RDGetRomsType(pszDrvName, nBaseType, nMinIdx, nMaxIdx);
}

static UINT32 RDGetNeoGeoRomsType(const char* pszDrvName, const TCHAR* pszMask)
{
	INT32 nMinIdx = 0, nMaxIdx = 0;
	UINT32 nBaseType = 0;

	if (       0 == _tcsicmp(pszMask, _T("Program"))    || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nMinIdx = nMaxIdx = 1;
		nBaseType = BRF_PRG;
	} else if (0 == _tcsicmp(pszMask, _T("Text"))       || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nMinIdx = nMaxIdx = 2;
		nBaseType = BRF_GRA;
	} else if (0 == _tcsicmp(pszMask, _T("Graphics"))   || 0 == _tcsicmp(pszMask, _T("GRA2"))) {
		nMinIdx = nMaxIdx = 3;
		nBaseType = BRF_GRA;
	} else if (0 == _tcsicmp(pszMask, _T("Z80"))        || 0 == _tcsicmp(pszMask, _T("SNDA"))) {
		nMinIdx = nMaxIdx = 4;
		nBaseType = BRF_PRG;
	} else if (0 == _tcsicmp(pszMask, _T("Samples"))    || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nMinIdx = nMaxIdx = 5;
		nBaseType = BRF_SND;
	} else if (0 == _tcsicmp(pszMask, _T("SamplesAES")) || 0 == _tcsicmp(pszMask, _T("SND2"))) {
		// See nam1975RomDesc
		nMinIdx = nMaxIdx = 6;
		nBaseType = BRF_SND;
	} else if (0 == _tcsicmp(pszMask, _T("BoardPLD"))   || 0 == _tcsicmp(pszMask, _T("OPT1"))) {
		return 0 | BRF_OPT;
	}

	return RDGetRomsType(pszDrvName, nBaseType, nMinIdx, nMaxIdx);
}

static INT32 GetDrvHardwareMask(const char* pszDrvName)
{
	const UINT32 nOldDrvSel = nBurnDrvActive;			// Backup
	nBurnDrvActive = BurnDrvGetIndex(pszDrvName);

	const INT32 nHardwareCode = BurnDrvGetHardwareCode();
	nBurnDrvActive = nOldDrvSel;						// Restore

	return (nHardwareCode & HARDWARE_PUBLIC_MASK);
}

static UINT32 RDSetRomsType(const char* pszDrvName, const TCHAR* pszMask, const UINT32 nPrgLen)
{
	INT32 nHardwareMask = GetDrvHardwareMask(pszDrvName);
	switch (nHardwareMask){
		case HARDWARE_IGS_PGM:
			return RDGetPgmRomsType(pszDrvName, pszMask);

		case HARDWARE_SNK_NEOGEO:
			return RDGetNeoGeoRomsType(pszDrvName, pszMask);

		case HARDWARE_CAPCOM_CPS1:
		case HARDWARE_CAPCOM_CPS1_QSOUND:
		case HARDWARE_CAPCOM_CPS1_GENERIC:
		case HARDWARE_CAPCOM_CPSCHANGER:
			return RDGetCps1RomsType(pszDrvName, pszMask, nPrgLen);

		case HARDWARE_CAPCOM_CPS2:
		case HARDWARE_CAPCOM_CPS2 | HARDWARE_CAPCOM_CPS2_SIMM:
			return RDGetCps2RomsType(pszDrvName, pszMask);

		case HARDWARE_CAPCOM_CPS3:
		case HARDWARE_CAPCOM_CPS3 | HARDWARE_CAPCOM_CPS3_NO_CD:
			return RDGetCps3RomsType(pszMask);

		case HARDWARE_SEGA_MEGADRIVE:
		case HARDWARE_PCENGINE_PCENGINE:
		case HARDWARE_PCENGINE_SGX:
		case HARDWARE_PCENGINE_TG16:
		case HARDWARE_SEGA_SG1000:
		case HARDWARE_COLECO:
		case HARDWARE_SEGA_MASTER_SYSTEM:
		case HARDWARE_SEGA_GAME_GEAR:
		case HARDWARE_MSX:
		case HARDWARE_SPECTRUM:
		case HARDWARE_NES:
		case HARDWARE_FDS:
		case HARDWARE_SNES:
		case HARDWARE_SNK_NGP:
		case HARDWARE_CHANNELF:
			return RDGetConsoleRomsType(pszDrvName);

		default:
			return RDGetUniversalRomsType(pszDrvName, pszMask);
	}
}

#define DELIM_TOKENS_NAME	_T(" \t\r\n,%:|{}")

// Get the next token and convert it from TCHAR to ANSI, with error handling for missing token or conversion failure
#define GET_TOKEN_REQUIRED(info, buf)					\
do {													\
	info = _strqtoken(NULL, DELIM_TOKENS_NAME);			\
	if (!info) return 0x10;								\
	if (tchar_to_ansi(info, &buf) <= 0) return 0x40;	\
} while (0)

// Get the next token and convert it from TCHAR to ANSI, but allow it to be optional (no token or conversion failure results in empty string)
#define GET_TOKEN_OPTIONAL(info, buf)					\
do {													\
	info = _strqtoken(NULL, DELIM_TOKENS_NAME);			\
	if (info) {											\
		if (tchar_to_ansi(info, &buf) <= 0)				\
			return 0x40;								\
	}													\
} while (0)

// Copy string from temporary buffer to destination and free the temporary buffer
#define STR_COPY_AND_FREE(dst, size)					\
do {													\
	strncpy(dst, p##dst, size - 1);						\
	free_s((void**)&p##dst);							\
} while (0)

// ==================== Parse Label Lines ====================
static INT32 ParseRomDataLabel(const TCHAR* pszLabel, char* szZipName, char* szDrvName, char* szExtraRom, char* szDate, TCHAR* szFullName, TCHAR* szRomMask, INT32* pDatMode, INT32* pIndex, UINT32* pStatusFlags)
{
	if (IsStrEmpty(pszLabel)) {
		return 0x10;									// Empty label parameter
	}

	UINT32 nLen = _tcslen(pszLabel);

	// Handle [XXX] tags (section headers)
	if ((2 < nLen) && (_T('[') == pszLabel[0]) && (_T(']') == pszLabel[nLen - 1])) {
		if (!IsStrEmpty(szRomMask))
			return 0;									// Only once

		pszLabel++;
		nLen -= 2;
		if (0 == _tcsicmp(_T("System"), pszLabel))
			return 1;									// End parsing marker found

		_tcsncpy(szRomMask, pszLabel, sizeof(szRomMask) / sizeof(TCHAR) - 1);
		return 0;										// Section header processed
	}

	if (0 == _tcsicmp(_T("System"), pszLabel)) {
		*pDatMode = 1;									// Set Nebula mode
		return 0;
	}

	if (nLen < 100U && (0 == _tcsicmp(_T("ShortName"), pszLabel) || 0 == _tcsicmp(_T("ZipName"), pszLabel) || 0 == _tcsicmp(_T("RomName"), pszLabel))) {
		if (!IsStrEmptyA(szZipName))
			return 0;									// Only once

		// Check label length to prevent buffer overflow in token parsing and copying
		if (nLen > 99U)
			return 0x20;								// Zip name is too long

		TCHAR* pszInfo   = NULL;
		char* pszZipName = NULL;
		GET_TOKEN_REQUIRED(pszInfo, pszZipName);
		STR_COPY_AND_FREE(szZipName, 100);

		if (BurnDrvGetIndex(szZipName) >= 0)
			return 0x20;								// Find duplicate short names

		*pStatusFlags |= (0x01 << 8);					// Mark ROM set as specified
		return 0;
	}

	if (0 == _tcsicmp(_T("DrvName"), pszLabel) || 0 == _tcsicmp(_T("Parent"), pszLabel)) {
		if (!IsStrEmptyA(szDrvName))
			return 0;									// Only once

		// Check label length to prevent buffer overflow in token parsing and copying
		if (nLen > 99U)
			return 0x20;								// Driver name is too long

		TCHAR* pszInfo   = NULL;
		char* pszDrvName = NULL;
		GET_TOKEN_REQUIRED(pszInfo, pszDrvName);
		STR_COPY_AND_FREE(szDrvName, 100);

		*pIndex = BurnDrvGetIndex(szDrvName);
		if (-1 == *pIndex)
			return 0x20;								// Built-in driver not found

		*pStatusFlags |= (0x02 << 8);					// Mark driver as specified
		return 0;
	}

	if (0 == _tcsicmp(_T("Date"), pszLabel) || 0 == _tcsicmp(_T("Release"), pszLabel)) {
		if (!IsStrEmptyA(szDate))
			return 0;									// Only once

		// Check label length to prevent buffer overflow in token parsing and copying
		if (nLen > 99U)
			return 0x20;								// Date is too long

		TCHAR* pszInfo = NULL;
		char* pszDate  = NULL;
		GET_TOKEN_OPTIONAL(pszInfo, pszDate);
		if (pszInfo)
			STR_COPY_AND_FREE(szDate, 100);
		return 0;
	}

	if (0 == _tcsicmp(_T("ExtraRom"), pszLabel)) {
		if (!IsStrEmptyA(szExtraRom))
			return 0;									// Only once

		// Check label length to prevent buffer overflow in token parsing and copying
		if (nLen > 99U)
			return 0x20;								// Extra rom is too long

		TCHAR* pszInfo    = NULL;
		char* pszExtraRom = NULL;
		GET_TOKEN_OPTIONAL(pszInfo, pszExtraRom);
		if (pszInfo)
			STR_COPY_AND_FREE(szExtraRom, 100);
		return 0;
	}

	if (0 == _tcsicmp(_T("FullName"), pszLabel) || 0 == _tcsicmp(_T("Game"), pszLabel)) {
		if (!IsStrEmpty(szFullName))
			return 0;									// Only once

		// Check label length to prevent buffer overflow in token parsing and copying
		if (nLen > MAX_PATH)
			return 0x20;								// Full name is too long

		INT32 nAdd = 0;
		TCHAR* pszInfo;
		while (pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME)) {
			INT32 nRem = MAX_PATH - nAdd - 1;
			if (0 >= nRem)
				break;									// Buffer full, stop adding
			_sntprintf(szFullName + nAdd, nRem, _T("%s "), pszInfo);
			nAdd = (INT32)_tcslen(szFullName);
		}

		if (0 == nAdd)
			return 0x10;								// No full name specified

		szFullName[nAdd - 1] = _T('\0');				// Remove trailing space
		*pStatusFlags |= (0x04 << 8);					// Mark full name as specified
		return 0;
	}

	return 2;											// This is a ROM entry, not a label
}

#undef STR_COPY_AND_FREE
#undef GET_TOKEN_OPTIONAL
#undef GET_TOKEN_REQUIRED

// ==================== Parse ROM Entries ====================
static INT32 ParseRomEntry(const TCHAR* pszLabel, const INT32 nDatMode, const TCHAR* szRomMask, const char* szDrvName, struct BurnRomInfo*** pPri, UINT32* pPriCount)
{
	if (IsStrEmpty(pszLabel)) {
		return 0x10;									// Empty ROM name
	}

	TCHAR* pszInfo;
	struct BurnRomInfo	ri = { 0 };
	ri.nLen  = UINT_MAX;
	ri.nCrc  = UINT_MAX;
	ri.nType = 0U;

	if (1 == nDatMode)
		pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);	// Skip first field in Nebula mode

	pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
	if (!pszInfo)
		return 0x10;									// No ROM length specified
	StrToUint(pszInfo, &ri.nLen);
	if (UINT_MAX == ri.nLen || 0 == ri.nLen)
		return 0x10;									// Invalid ROM length

	pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
	if (!pszInfo)
		return 0x10;									// No ROM CRC specified
	StrToUint(pszInfo, &ri.nCrc);
	if (UINT_MAX == ri.nCrc)
		return 0x10;									// Invalid ROM CRC

	while (pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME)) {
		UINT32 nValue = UINT_MAX;
		for (UINT32 i = 0; i < ARRAY_SIZE(RDMacroTable); i++) {
			if (0 == _tcscmp(pszInfo, RDMacroTable[i].pszDRMacro)) {
				ri.nType |= RDMacroTable[i].nRDMacro;	// Apply macro flag
				continue;
			}
		}
		StrToUint(pszInfo, &nValue);
		if (UINT_MAX > nValue)
			ri.nType |= nValue;							// Apply numeric flag
	}

	if (0 == ri.nType && 1 == nDatMode)
		// Auto-detect type for Nebula mode
		ri.nType = RDSetRomsType(szDrvName, szRomMask, ri.nLen);

	if (0 == ri.nType)
		return 0x08;									// ROM type undetermined

	struct BurnRomInfo** new_pri = (struct BurnRomInfo**)realloc(*pPri, (*pPriCount + 1) * sizeof(struct BurnRomInfo*));
	if (!new_pri)
		return 0x04;									// Memory allocation error

	struct BurnRomInfo* rom = (struct BurnRomInfo*)malloc(sizeof(struct BurnRomInfo));
	if (!rom) {
		free(new_pri);
		return 0x04;									// Memory allocation error
	}
	memset(rom, 0, sizeof(struct BurnRomInfo));

	if (tchar_to_ansi(pszLabel, &rom->szName) <= 0) {
		free(rom);
		free(new_pri);
		return 0x04;									// Memory allocation error in string conversion
	}

	rom->nLen  = ri.nLen;
	rom->nCrc  = ri.nCrc;
	rom->nType = ri.nType;

	new_pri[*pPriCount] = rom;
	*pPri = new_pri;
	(*pPriCount)++;

	return 0;
}

// ==================== Build Final RomDataDrv Structure ====================
// Returns 0 on success, or an error code on failure
static INT32 BuildRomDataDrv(struct BurnRomInfo** pri, UINT32 pri_count, char* szZipName, char* szDrvName, char* szExtraRom, char* szDate, TCHAR* szFullName, INT32 nIndex)
{
	if (IsStrEmptyA(szZipName) || IsStrEmptyA(szDrvName) || IsStrEmpty(szFullName)) {
		return 0x10;									// Missing required parameters
	}

	struct RomDataDrv** new_drv = (struct RomDataDrv**)realloc(pRDDrv, (nRDDrvCount + 1) * sizeof(struct RomDataDrv*));
	if (!new_drv)
		return 0x04;									// Memory allocation error

	struct RomDataDrv* tmp = (struct RomDataDrv*)malloc(sizeof(struct RomDataDrv));
	if (!tmp) {
		free(new_drv);
		return 0x04;									// Memory allocation error
	}
	memset(tmp, 0, sizeof(struct RomDataDrv));

	UINT32 nOldDrvActive  = nBurnDrvActive;
	nBurnDrvActive        = nIndex;						// Temporarily switch active driver
	const char* pszRet    = BurnDrvGetTextA(DRV_PARENT);
	const char* pszParent = (pszRet && (BurnDrvGetFlags() & BDF_CLONE)) ? pszRet : szDrvName;
	const INT32 nfullName = (INT32)_tcslen(szFullName);

	tmp->pszZipName  = strdup(szZipName);
	tmp->pszParent   = strdup(pszParent);
	tmp->pszDrvName  = strdup(szDrvName);
	tmp->pszFullName = (TCHAR*)malloc((nfullName + 2) * sizeof(TCHAR));
	tmp->pDriver     = (struct BurnDriver*)malloc(sizeof(struct BurnDriver));
	tmp->pRomInfo    = (struct BurnRomInfo*)malloc(pri_count * sizeof(struct BurnRomInfo));
	if ('\0' != szDate[0])
		tmp->pszDate = strdup(szDate);
	if ('\0' != szExtraRom[0])
		tmp->pszExtraRom = strdup(szExtraRom);
	if (!tmp->pszZipName || !tmp->pszParent || !tmp->pszDrvName || !tmp->pszFullName || !tmp->pDriver || !tmp->pRomInfo ||
		('\0' != szDate[0] && !tmp->pszDate) || ('\0' != szExtraRom[0] && !tmp->pszExtraRom)) {
		romdata_drv_clearup(tmp);
		free(tmp);
		free(new_drv);
		nBurnDrvActive = nOldDrvActive;
		return 0x04;									// Memory allocation error
	}

	// It must end with two '\0' characters
	// otherwise, garbled characters will appear at the end of the string
	_tcsncpy(tmp->pszFullName, szFullName, nfullName);
	tmp->pszFullName[nfullName + 0] = _T('\0');
	tmp->pszFullName[nfullName + 1] = _T('\0');

	// Copy the existing driver as a base for the new RomData driver
	memcpy(tmp->pDriver, BurnGetDriver(szDrvName), sizeof(struct BurnDriver));

	for (UINT32 i = 0; i < pri_count; i++) {
		tmp->pRomInfo[i].szName = strdup(pri[i]->szName);
		if (!tmp->pRomInfo[i].szName) {
			for (UINT32 j = 0; j < i; j++)
				free_s((void**)&tmp->pRomInfo[j].szName);
			romdata_drv_clearup(tmp);
			free(tmp);
			free(new_drv);
			nBurnDrvActive = nOldDrvActive;
			return 0x04;								// Memory allocation error
		}
		tmp->pRomInfo[i].nLen  = pri[i]->nLen;
		tmp->pRomInfo[i].nCrc  = pri[i]->nCrc;
		tmp->pRomInfo[i].nType = pri[i]->nType;
	}
	tmp->nRomInfoCount        = pri_count;
	tmp->pDriver->szShortName = tmp->pszZipName;
	tmp->pDriver->szParent    = tmp->pszParent;
	if (!IsStrEmptyA(tmp->pszDate))
		tmp->pDriver->szDate  = tmp->pszDate;

#ifdef _UNICODE
	tmp->pDriver->szFullNameW = tmp->pszFullName;
#else
	tmp->pDriver->szFullNameA = tmp->pszFullName;
#endif
	// Mark as ROM data driver
	tmp->pDriver->Flags |= BDF_ROMDATA_DRIVER;
	// Set clone flag if not already a clone
	if (!(BurnDrvGetFlags() & BDF_CLONE))
		tmp->pDriver->Flags |= BDF_CLONE;

	nBurnDrvActive = nOldDrvActive;						// Restore original active driver

	// Link the new driver into the existing driver list
	if (~0U == LinkExtlDrivers(tmp->pDriver, &nBurnDrvCount)) {
		for (UINT32 i = 0; i < pri_count; i++) {
			free_s((void**)&tmp->pRomInfo[i].szName);
		}
		romdata_drv_clearup(tmp);
		free(new_drv);
		return 0x04;									// Memory allocation error
	}

	pRDDrv = new_drv;									// Update global driver array pointer
	pRDDrv[nRDDrvCount] = tmp;							// Add new driver to the array
	nRDDrvCount++;										// Increment driver count

	return 0;
}

struct LoadRomdataContext {
	INT32 result;
};

// ==================== Callback: Real Romdata Loading Logic ====================
static void LoadRomdataCallback(const TCHAR* inputFile, FILE* fp, void* userData)
{
	LoadRomdataContext* ctx = (LoadRomdataContext*)userData;
	ctx->result = 0x01;									// Default error

	// If file not opened, return error
	if (!fp)
		return;

	UINT32 nRet = 0U;
	struct BurnRomInfo** pri = NULL;
	UINT32 pri_count = 0;

	char szZipName[100]  = { 0 };
	char szDrvName[100]  = { 0 };
	char szDate[100]     = { 0 };
	char szExtraRom[100] = { 0 };

	TCHAR szBuf[MAX_PATH] = { 0 };
	TCHAR szRomMask[100]  = { 0 };
	TCHAR szFullName[MAX_PATH] = { 0 };

	INT32 nDatMode = 0;
	INT32 nIndex   = -1;
	INT32 parseRet = 0;

	// Parse file line by line
	while (_fgetts(szBuf, MAX_PATH, fp)) {
		TCHAR* pszLabel = _strqtoken(szBuf, DELIM_TOKENS_NAME);
		if (!pszLabel || (_T('/') == pszLabel[0] && _T('/') == pszLabel[1]))
			continue;

		parseRet = ParseRomDataLabel(pszLabel, szZipName, szDrvName, szExtraRom, szDate, szFullName, szRomMask, &nDatMode, &nIndex, &nRet);

		if (1 == parseRet)
			break;
		if (2 < parseRet) {
			ctx->result = romdata_rominfo_clearup(fp, pri, pri_count, parseRet);
			return;
		}
		if (0 == parseRet)
			continue;

		INT32 romRet = ParseRomEntry(pszLabel, nDatMode, szRomMask, szDrvName, &pri, &pri_count);
		if (0 != romRet) {
			ctx->result = romdata_rominfo_clearup(fp, pri, pri_count, romRet);
			return;
		}
	}

	// Check read error
	if (ferror(fp)) {
		ctx->result = romdata_rominfo_clearup(fp, pri, pri_count, 0x80);
		return;
	}

	// Set status flags
	if (pri)
		nRet |= (0x10 << 8);
	if (0U < pri_count)
		nRet |= (0x20 << 8);

	// Check required flags
	if ((0x3f & ~0x08) != (nRet >> 8)) {
		ctx->result = romdata_rominfo_clearup(fp, pri, pri_count, 0x10);
		return;
	}

	// Build driver
	INT32 buildRet = BuildRomDataDrv(pri, pri_count, szZipName, szDrvName, szExtraRom, szDate, szFullName, nIndex);
	if (buildRet) {
		ctx->result = romdata_rominfo_clearup(fp, pri, pri_count, buildRet);
		return;
	}
	nRet |= (0x08 << 8);

	// Cleanup temp structures
	romdata_rominfo_clearup(fp, pri, pri_count, 0);

	// Auto backup
	TCHAR* pszRomdataPath = NULL;
	if (GetRomDataFullPath(&pszRomdataPath) > 0) {
		RomDataAutoBackup(inputFile, pszRomdataPath);
		free_s((void**)&pszRomdataPath);
	}

	// Success
	ctx->result = (INT32)nRet;
}

#undef DELIM_TOKENS_NAME

// ==================== Top-level API (SAFE & AUTO-CLEAN) ====================
// Returns status flags (bits 8-13) or error code (bits 0-7)
static INT32 LoadRomdata(const TCHAR* pszFileName)
{
	// Validate input
	if (IsStrEmpty(pszFileName))
		return 0x01;
	if (!IsFileExtensionMatch(pszFileName, _T(".dat")))
		return 0x01;

	// Use callback system to auto open/close/cleanup
	LoadRomdataContext ctx = { 0 };
	SafeProcessTextFile(pszFileName, LoadRomdataCallback, &ctx);

	return ctx.result;
}

#define BUFFER_SIZE 65536
INT32 OpenFilesLoadRomData(HWND hWnd, INT32* perrCnt)
{
	TCHAR szBuffer[BUFFER_SIZE] = { 0 };
	OPENFILENAME ofname = { 0 };

	// Initialize OPENFILENAME structure for multi-select dialog
	ofname.lStructSize = sizeof(OPENFILENAME);
	ofname.hwndOwner   = hWnd;
	ofname.lpstrFile   = szBuffer;
	ofname.nMaxFile    = BUFFER_SIZE;
	ofname.lpstrFilter = _T("RomData (*.dat)\0*.dat\0");
	ofname.Flags       = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

	// Return 0 if dialog canceled or failed
	if (!GetOpenFileName(&ofname)) {
		if (perrCnt)
			*perrCnt = 0;

		return 0;
	}

	TCHAR* pBuf  = szBuffer;
	INT32 nCount = 0;
	INT32 errCnt = 0;

	UINT32 firstLen = _tcslen(pBuf);

	// SAFETY: Prevent buffer overflow - treat as single select if buffer is full
	if (firstLen >= BUFFER_SIZE - 1) {
		LoadRomdata(pBuf);
		nCount = 1;

		if (perrCnt)
			*perrCnt = errCnt;

		return nCount;
	}

	TCHAR* pNextChar = pBuf + firstLen + 1;

	// Single file selected
	if (_T('\0') == *pNextChar) {
		LoadRomdata(pBuf);
		nCount = 1;

		if (perrCnt)
			*perrCnt = errCnt;

		return nCount;
	}

	// Multiple files selected
	const TCHAR* const pFolder = pBuf;
	pBuf = pNextChar;

	// Iterate all selected files
	while (_T('\0') != *pBuf) {
		TCHAR* pszFullPath = NULL;

		// Safe path concatenation (allocates memory)
		INT32 pathLen = PathCombine_s(&pszFullPath, pFolder, pBuf);
		if (pathLen < 0) {
			errCnt++;
			pBuf += _tcslen(pBuf) + 1;

			if ((UINT32)(pBuf - szBuffer) >= BUFFER_SIZE)
				break;

			continue;
		}
		if (pathLen > 0) {
			LoadRomdata(pszFullPath);
			nCount++;
		} else
			errCnt++;

		free_s((void**)&pszFullPath);

		// Move to next file
		pBuf += _tcslen(pBuf) + 1;

		// Prevent buffer overflow
		if ((UINT32)(pBuf - szBuffer) >= BUFFER_SIZE)
			break;
	}

	// Output error count
	if (perrCnt)
		*perrCnt = errCnt;

	return nCount;
}
#undef BUFFER_SIZE

// Traverses a directory and invokes a callback for each file found
void TraverseDirectoryFiles(const TCHAR* dirPath, void (*pFoundCallBack)(const TCHAR*), bool scanSubdirs)
{
	// Check if input directory path is invalid
	if (IsStrEmpty(dirPath))
		return;

	TCHAR searchPath[MAX_PATH] = { 0 };

	// Build search path: "dir\\*" or "dir*"
	const TCHAR* searchFormat = ends_with_slash(dirPath) ? _T("%s*") : _T("%s\\*");
	int result = _sntprintf(searchPath, MAX_PATH, searchFormat, dirPath);

	// Handle buffer overflow safely
	if (result < 0 || result >= MAX_PATH) {
		searchPath[MAX_PATH - 1] = _T('\0');
		return;
	}

	WIN32_FIND_DATA findData = { 0 };
	HANDLE hFind = FindFirstFile(searchPath, &findData);

	// Exit if no files found or error
	if (INVALID_HANDLE_VALUE == hFind)
		return;

	BOOL bContinue = TRUE;
	while (bContinue) {
		// Skip current directory "." and parent directory ".."
		if (0 == _tcscmp(findData.cFileName, _T(".")) || 0 == _tcscmp(findData.cFileName, _T(".."))) {
			bContinue = FindNextFile(hFind, &findData);
			continue;
		}

		TCHAR* fullPath = NULL;
		INT32 pathLen = PathCombine_s(&fullPath, dirPath, findData.cFileName);
		if (pathLen < 0) {
			bContinue = FindNextFile(hFind, &findData);
			continue;
		}

		// Process directory or file
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			// Skip reparse points(junctions/symlinks), scan subdirs if enabled
			if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && scanSubdirs)
				TraverseDirectoryFiles(fullPath, pFoundCallBack, scanSubdirs);
		} else {
			// Invoke the user-defined callback for regular files
			// or others
			if (pFoundCallBack)
				pFoundCallBack(fullPath);
		}

		free_s((void**)&fullPath);

		// Get next file
		bContinue = FindNextFile(hFind, &findData);
	}

	// Close search handle to avoid resource leak
	FindClose(hFind);
}

// Callback that loads ROM data from a given file path
static void RomdataLoadCallback(const TCHAR* filePath)
{
	LoadRomdata(filePath);
}

// Scans a directory (and optional subdirectories) and loads all ROM data files
void ScanDirectoryLoadRomdata(const TCHAR* dirPath, bool scanSubdirs)
{
	TraverseDirectoryFiles(dirPath, RomdataLoadCallback, scanSubdirs);
}

INT32 OpenDirectoryLoadRomData(HWND hWnd)
{
	TCHAR szDirPath[MAX_PATH] = { 0 };					// Buffer to store selected folder path

	// Initialize folder dialog
	BROWSEINFO bi = { 0 };
	bi.hwndOwner = hWnd;
	bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;	// Allow folders only

	// Show folder selection dialog
	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (pidl == NULL)
		return 0;										// User canceled the dialog


	// Returns FALSE if conversion fails
	if (!SHGetPathFromIDList(pidl, szDirPath)) {
		CoTaskMemFree(pidl);							// Free memory before return
		return 0;
	}

	// Free allocated PIDL memory
	CoTaskMemFree(pidl);

	ScanDirectoryLoadRomdata(szDirPath, bRDListScanSub);

	return 1;
}

// Initializes RomData system by loading all ROM data files from the specified directory
void RomDataInit()
{
	TCHAR* pszRomdataFullPath = NULL;

	// Get full absolute path of ROM data directory
	if (GetRomDataFullPath(&pszRomdataFullPath) < 0)
		return;

	// Scan directory and load all ROM data files (recursively if enabled)
	ScanDirectoryLoadRomdata(pszRomdataFullPath, bRDListScanSub);

	// Free allocated path string
	free_s((void**)&pszRomdataFullPath);
}

// Cleans up all RomData drivers and frees associated memory
void RomDataExit()
{
	// RomData driver array is not available, nothing to clean up
	if (!pRDDrv || 0 == nRDDrvCount)
		return;

	// Clean up each RomData driver structure
	for (UINT32 i = 0; i < nRDDrvCount; i++)
		romdata_drv_clearup(pRDDrv[i]);

	// Free the RomData driver array
	free(pRDDrv); pRDDrv = NULL; nRDDrvCount = 0;
}

// For Neo Geo games with an extra ROM that needs to be mapped at 0x900000,
// this function maps it into memory after the main ROMs are loaded
void NeoProcessExtraRom(UINT8* rom)
{
#ifdef BUILD_WIN32
	if (!IsRomDataDrv())
		return;

	const char* extraRom = RomDataDrvGetExtraRom();
	if (!extraRom)
		return;

	char*  romName  = NULL;
	bool   found    = false;
	UINT32 romLen   = 0;
	UINT32 exromLen = 0;
	struct BurnRomInfo ri = { 0 };

	for (UINT32 i = 0; !BurnDrvGetRomName(&romName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);
		if (0 == strcmp(romName, extraRom)) {			// Find
			found    = true;
			exromLen = ri.nLen;
		}
		if (1 == (ri.nType & 7))						// P Roms
			romLen += ri.nLen;
	}

	if (0 == exromLen || romLen <= exromLen)
		return;

	const UINT32 startOffs = romLen - exromLen;
	const UINT32 mapEnd    = 0x900000 + (exromLen - 1);
	SekMapMemory(rom + startOffs, 0x900000, mapEnd, MAP_ROM);
#else
	if (pDataRomDesc && pRDI->szExtraRom) {
		UINT32 nRomLen = 0, nExtraRomLen = 0;
		for (INT32 i = 0; i < pRDI->nDescCount; i++) {
			if (1 == (pDataRomDesc[i].nType & 7)) {								// P Roms
				nRomLen += pDataRomDesc[i].nLen;

				if (0 == strcmp(pDataRomDesc[i].szName, pRDI->szExtraRom)) {	// Extra rom
					nExtraRomLen = pDataRomDesc[i].nLen;
				}
			}
		}
		if ((nExtraRomLen > 0) && (nExtraRomLen < nRomLen)) {
			SekMapMemory(rom + (nRomLen - nExtraRomLen), 0x900000, 0x900000 + (nExtraRomLen - 1), MAP_ROM);
		}
	}
#endif // BUILD_WIN32
}

//------------------- Deprecated in the Windows version ------------------
TCHAR szRomdataName[MAX_PATH] = { 0 };
RomDataInfo* pRDI;
BurnRomInfo* pDataRomDesc;

TCHAR* AdaptiveEncodingReads(const TCHAR* pszFileName)
{
	return NULL;
}

char* RomdataGetDrvName()
{
	return NULL;
}

void RomDataSetFullName()
{

}
//------------------- End of deprecated code ------------------
