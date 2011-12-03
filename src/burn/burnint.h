// Burn - Arcade emulator library - internal code

// Standard headers
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "tchar.h"
#include "burn.h"

// ---------------------------------------------------------------------------
// CPU emulation interfaces

// sek.cpp
#include "sek.h"

// zet.cpp
#include "zet.h"

typedef union
{
	struct { UINT8 l,h,h2,h3; } b;
	struct { UINT16 l,h; } w;
	UINT32 d;
} PAIR;

// ---------------------------------------------------------------------------
// Driver information

struct BurnDriver {
	char* szShortName;			// The filename of the zip file (without extension)
	char* szParent;				// The filename of the parent (without extension, NULL if not applicable)
	char* szBoardROM;			// The filename of the board ROMs (without extension, NULL if not applicable)
	char* szSampleName;			// The filename of the samples zip file (without extension, NULL if not applicable)
	char* szDate;

	// szFullNameA, szCommentA, szManufacturerA and szSystemA should always contain valid info
	// szFullNameW, szCommentW, szManufacturerW and szSystemW should be used only if characters or scripts are needed that ASCII can't handle
	char*    szFullNameA; char*    szCommentA; char*    szManufacturerA; char*    szSystemA;
	wchar_t* szFullNameW; wchar_t* szCommentW; wchar_t* szManufacturerW; wchar_t* szSystemW;

	INT32 Flags;			// See burn.h
	INT32 Players;		// Max number of players a game supports (so we can remove single player games from netplay)
	INT32 Hardware;		// Which type of hardware the game runs on
	INT32 Genre;
	INT32 Family;
	INT32 (*GetZipName)(char** pszName, UINT32 i);				// Function to get possible zip names
	INT32 (*GetRomInfo)(struct BurnRomInfo* pri, UINT32 i);		// Function to get the length and crc of each rom
	INT32 (*GetRomName)(char** pszName, UINT32 i, INT32 nAka);	// Function to get the possible names for each rom
	INT32 (*GetSampleInfo)(struct BurnSampleInfo* pri, UINT32 i);		// Function to get the sample flags
	INT32 (*GetSampleName)(char** pszName, UINT32 i, INT32 nAka);	// Function to get the possible names for each sample
	INT32 (*GetInputInfo)(struct BurnInputInfo* pii, UINT32 i);	// Function to get the input info for the game
	INT32 (*GetDIPInfo)(struct BurnDIPInfo* pdi, UINT32 i);		// Function to get the input info for the game
	INT32 (*Init)(); INT32 (*Exit)(); INT32 (*Frame)(); INT32 (*Redraw)(); INT32 (*AreaScan)(INT32 nAction, INT32* pnMin);
	UINT8* pRecalcPal; UINT32 nPaletteEntries;										// Set to 1 if the palette needs to be fully re-calculated
	INT32 nWidth, nHeight; INT32 nXAspect, nYAspect;					// Screen width, height, x/y aspect
};

#define BurnDriverD BurnDriver		// Debug status
#define BurnDriverX BurnDriver		// Exclude from build

// Standard functions for dealing with ROM and input info structures
#include "stdfunc.h"

// ---------------------------------------------------------------------------

// burn.cpp
INT32 BurnSetRefreshRate(double dRefreshRate);
INT32 BurnByteswap(UINT8* pm,INT32 nLen);
INT32 BurnClearScreen();

// load.cpp
INT32 BurnLoadRom(UINT8* Dest, INT32 i, INT32 nGap);
INT32 BurnXorRom(UINT8* Dest, INT32 i, INT32 nGap);
INT32 BurnLoadBitField(UINT8* pDest, UINT8* pSrc, INT32 nField, INT32 nSrcLen);

// ---------------------------------------------------------------------------
// Colour-depth independant image transfer

extern UINT16* pTransDraw;

void BurnTransferClear();
INT32 BurnTransferCopy(UINT32* pPalette);
void BurnTransferExit();
INT32 BurnTransferInit();

// ---------------------------------------------------------------------------
// Plotting pixels

inline static void PutPix(UINT8* pPix, UINT32 c)
{
	if (nBurnBpp >= 4) {
		*((UINT32*)pPix) = c;
	} else {
		if (nBurnBpp == 2) {
			*((UINT16*)pPix) = (UINT16)c;
		} else {
			pPix[0] = (UINT8)(c >>  0);
			pPix[1] = (UINT8)(c >>  8);
			pPix[2] = (UINT8)(c >> 16);
		}
	}
}

// ---------------------------------------------------------------------------
// Setting up cpus for cheats

void CpuCheatRegister(INT32 type, INT32 num);

// burn_memory.cpp
void BurnInitMemoryManager();
UINT8 *BurnMalloc(INT32 size);
void _BurnFree(void *ptr);
#define BurnFree(x)		_BurnFree(x); x = NULL;
void BurnExitMemoryManager();

// ---------------------------------------------------------------------------
// Sound clipping macro
#define BURN_SND_CLIP(A) ((A) < -0x8000 ? -0x8000 : (A) > 0x7fff ? 0x7fff : (A))
