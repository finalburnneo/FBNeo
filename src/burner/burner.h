// FinalBurn Neo - Emulator for MC68000/Z80 based arcade games
//            Refer to the "license.txt" file for more info
#pragma once
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <ctype.h>

#include "tchar.h"

// Macro to make quoted strings
#define MAKE_STRING_2(s) #s
#define MAKE_STRING(s) MAKE_STRING_2(s)

#define BZIP_MAX (20)								// Maximum zip files to search through
#if defined (BUILD_QT)
 #define DIRS_MAX (4)								// Maximum number of directories to search
#else
 #define DIRS_MAX (20)								// Maximum number of directories to search
#endif

#include "title.h"
#include "burn.h"

// ---------------------------------------------------------------------------
// OS dependent functionality
typedef struct tagIMAGE {
	unsigned int	width;
	unsigned int	height;
	unsigned int	rowbytes;
	unsigned int	imgbytes;
	unsigned char**	rowptr;
	unsigned char*	bmpbits;
	unsigned int	flags;
} IMAGE;

#if defined (BUILD_WIN32)
 #include "burner_win32.h"
#elif defined (BUILD_MACOS)
 #include "burner_macos.h"
#elif defined (BUILD_SDL) || defined (BUILD_SDL2)
 #include "burner_sdl.h"
#elif defined (_XBOX) && !defined(__LIBRETRO__)
 #include "burner_xbox.h"
#elif defined(__LIBRETRO__)
#include "burner_libretro.h"
#elif defined(BUILD_QT)
 #include "burner_qt.h"
#endif

#if defined (INCLUDE_LIB_PNGH)
 #include "png.h"
#endif

// ---------------------------------------------------------------------------
// OS independent functionality

#ifndef __LIBRETRO__
#include "interface.h"
#endif

#if defined (INCLUDE_LUA_SUPPORT) || defined (BUILD_WIN32)
#include "luaengine.h"
#endif

#define IMG_FREE		(1 << 0)

// Macros for parsing text
#define SKIP_WS(s) while (_istspace(*s)) { s++; }			// Skip whitespace
#define FIND_WS(s) while (*s && !_istspace(*s)) { s++; }	// Find whitespace
#define FIND_QT(s) while (*s && *s != _T('\"')) { s++; }	// Find quote

// burn/burn.cpp
extern INT32 bRewindEnabled;
extern INT32 nRewindMemory;


// image.cpp
extern int bPngImageOrientation;
void img_free(IMAGE* img);
INT32 img_alloc(IMAGE* img);

bool PNGIsImage(FILE* fp);
bool PNGIsImageBuffer(unsigned char* buffer, int bufferLength);
INT32 PNGLoad(IMAGE* img, FILE* fp, INT32 nPreset);
INT32 PNGLoadBuffer(IMAGE* img, unsigned char* buffer, int bufferLength, INT32 nPreset);
INT32 PNGGetInfo(IMAGE* img, FILE *fp);
INT32 PNGGetInfoBuffer(IMAGE* img, unsigned char* buffer, int bufferLength);

// gami.cpp
extern struct GameInp* GameInp;
extern UINT32 nGameInpCount;
extern UINT32 nMacroCount;
extern UINT32 nMaxMacro;

extern INT32 nAnalogSpeed;

extern INT32 nFireButtons;

extern bool bStreetFighterLayout;
extern bool bLeftAltkeyMapped;
extern bool bResetDrv;

INT32 GameInpInit();
INT32 GameInpExit();
TCHAR* InputCodeDesc(INT32 c);
TCHAR* InpToDesc(struct GameInp* pgi);
TCHAR* InpMacroToDesc(struct GameInp* pgi);
TCHAR* InputNumToName(UINT32 i);
TCHAR* InpToString(struct GameInp* pgi);
TCHAR* InpMacroToString(struct GameInp* pgi);
#ifndef __LIBRETRO__
void GameInpCheckLeftAlt();
void GameInpCheckMouse();
#endif
INT32 GameInpBlank(INT32 bDipSwitch);
INT32 GameInputAutoIni(INT32 nPlayer, TCHAR* lpszFile, bool bOverWrite);
INT32 ConfigGameLoadHardwareDefaults();
INT32 GameInpDefault();
INT32 GameInpWrite(FILE* h);
INT32 GameInpRead(TCHAR* szVal, bool bOverWrite);
INT32 GameInpMacroRead(TCHAR* szVal, bool bOverWrite);
INT32 GameMacroAutofireRead(TCHAR* szVal, bool bOverWrite);
INT32 GameInpCustomRead(TCHAR* szVal, bool bOverWrite);

struct tIniStruct {
	TCHAR system[80];
	TCHAR ini[MAX_PATH];
	INT32 hw[8];
	char gameinfotoken[80];
};

extern tIniStruct gamehw_cfg[];

void GetHistoryDatHardwareToken(char *to_string);

// inp_interface.cpp
extern INT32 nAutoFireRate;

// Player Default Controls
extern INT32 nPlayerDefaultControls[8];
extern TCHAR szPlayerDefaultIni[5][MAX_PATH];

// mappable System Macros for the Input Dialogue
extern UINT8 macroSystemPause;
extern UINT8 macroSystemFFWD;
extern UINT8 macroSystemFrame;
extern UINT8 macroSystemSaveState;
extern UINT8 macroSystemLoadState;
extern UINT8 macroSystemUNDOState;
extern UINT8 macroSystemRewind;
extern UINT8 macroSystemRewindCancel;
extern UINT8 macroSystemSlowMo[5];
extern UINT8 macroSystemLuaHotkey1;
extern UINT8 macroSystemLuaHotkey2;
extern UINT8 macroSystemLuaHotkey3;
extern UINT8 macroSystemLuaHotkey4;
extern UINT8 macroSystemLuaHotkey5;
extern UINT8 macroSystemLuaHotkey6;
extern UINT8 macroSystemLuaHotkey7;
extern UINT8 macroSystemLuaHotkey8;
extern UINT8 macroSystemLuaHotkey9;

// scrn.cpp
extern void scrnSSUndo();
extern bool bHasFocus;

// cong.cpp
extern const INT32 nConfigMinVersion;					// Minimum version of application for which input files are valid
extern bool bSaveInputs;
INT32 ConfigGameLoad(bool bOverWrite);				// char* lpszName = NULL
INT32 ConfigGameSave(bool bSave);

// conc.cpp
INT32 ConfigCheatLoad();

// gamc.cpp
INT32 GamcMisc(struct GameInp* pgi, char* szi, INT32 nPlayer);
INT32 GamcAnalogKey(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nSlide);
INT32 GamcAnalogJoy(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nJoy, INT32 nSlide);
INT32 GamcPlayer(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nDevice);
INT32 GamcPlayerHotRod(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nFlags, INT32 nSlide);

// misc.cpp
#define QUOTE_MAX (128)															// Maximum length of "quoted strings"
INT32 QuoteRead(TCHAR** ppszQuote, TCHAR** ppszEnd, TCHAR* pszSrc);					// Read a quoted string from szSrc and poINT32 to the end
TCHAR* LabelCheck(TCHAR* s, TCHAR* pszLabel);

TCHAR* ExtractFilename(TCHAR* fullname);
TCHAR* DriverToName(UINT32 nDrv);
UINT32 NameToDriver(TCHAR* szName);
TCHAR *StrReplace(TCHAR *str, TCHAR find, TCHAR replace);
TCHAR *StrLower(TCHAR *str);
TCHAR *FileExt(TCHAR *str);
bool IsFileExt(TCHAR *str, TCHAR *ext);

extern INT32 bDoGamma;
extern INT32 bHardwareGammaOnly;
extern double nGamma;

INT32 SetBurnHighCol(INT32 nDepth);
char* GameDecoration(UINT32 nBurnDrv);
char* DecorateGameName(UINT32 nBurnDrv);
TCHAR* DecorateGenreInfo();
void ComputeGammaLUT();

// dat.cpp
#define DAT_ARCADE_ONLY			0
#define DAT_MEGADRIVE_ONLY		1
#define DAT_PCENGINE_ONLY		2
#define DAT_TG16_ONLY			3
#define DAT_SGX_ONLY			4
#define DAT_SG1000_ONLY			5
#define DAT_COLECO_ONLY			6
#define DAT_MASTERSYSTEM_ONLY	7
#define DAT_GAMEGEAR_ONLY		8
#define DAT_MSX_ONLY			9
#define DAT_SPECTRUM_ONLY		10
#define DAT_NEOGEO_ONLY			11
#define DAT_NES_ONLY			12
#define DAT_FDS_ONLY			13
#define DAT_NGP_ONLY			14
#define DAT_CHANNELF_ONLY		15

INT32 write_datfile(INT32 bType, FILE* fDat);
INT32 create_datfile(TCHAR* szFilename, INT32 bType);

// sshot.cpp
INT32 MakeScreenShot();

// state.cpp
INT32 BurnStateLoadEmbed(FILE* fp, INT32 nOffset, INT32 bAll, INT32 (*pLoadGame)());
INT32 BurnStateLoad(TCHAR* szName, INT32 bAll, INT32 (*pLoadGame)());
INT32 BurnStateSaveEmbed(FILE* fp, INT32 nOffset, INT32 bAll);
INT32 BurnStateSave(TCHAR* szName, INT32 bAll);
INT32 BurnStateUNDO(TCHAR* szName);

// statec.cpp
INT32 BurnStateCompress(UINT8** pDef, INT32* pnDefLen, INT32 bAll);
INT32 BurnStateDecompress(UINT8* Def, INT32 nDefLen, INT32 bAll);

// nvram.cpp
INT32 BurnNvramLoad(TCHAR* szName);
INT32 BurnNvramSave(TCHAR* szName);

// zipfn.cpp
struct ZipEntry { char* szName;	UINT32 nLen; UINT32 nCrc; };

INT32 ZipOpen(char* szZip);
INT32 ZipClose();
INT32 ZipGetList(struct ZipEntry** pList, INT32* pnListCount);
INT32 ZipLoadFile(UINT8* Dest, INT32 nLen, INT32* pnWrote, INT32 nEntry);
INT32 __cdecl ZipLoadOneFile(char* arcName, const char* fileName, void** Dest, INT32* pnWrote);

// bzip.cpp

#define BZIP_STATUS_OK		(0)
#define BZIP_STATUS_BADDATA	(1)
#define BZIP_STATUS_ERROR	(2)

INT32 BzipOpen(bool);
INT32 BzipClose();
INT32 BzipInit();
INT32 BzipExit();
INT32 BzipStatus();

// support_paths.cpp
extern TCHAR szAppPreviewsPath[MAX_PATH];
extern TCHAR szAppTitlesPath[MAX_PATH];
extern TCHAR szAppCheatsPath[MAX_PATH];
extern TCHAR szAppIpsPath[MAX_PATH];
extern TCHAR szAppIconsPath[MAX_PATH];
extern TCHAR szAppSelectPath[MAX_PATH];
extern TCHAR szAppVersusPath[MAX_PATH];
extern TCHAR szAppHowtoPath[MAX_PATH];
extern TCHAR szAppScoresPath[MAX_PATH];
extern TCHAR szAppBossesPath[MAX_PATH];
extern TCHAR szAppGameoverPath[MAX_PATH];
extern TCHAR szAppFlyersPath[MAX_PATH];
extern TCHAR szAppMarqueesPath[MAX_PATH];
extern TCHAR szAppControlsPath[MAX_PATH];
extern TCHAR szAppCabinetsPath[MAX_PATH];
extern TCHAR szAppPCBsPath[MAX_PATH];
extern TCHAR szAppHistoryPath[MAX_PATH];
