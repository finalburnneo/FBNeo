#ifndef _BURNINT_H
#define _BURNINT_H

// Burn - Arcade emulator library - internal code

// Standard headers
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "tchar.h"

#include "burn.h"
#include "burn_sound.h"
#include "joyprocess.h"
#include "burn_endian.h"

// macros to prevent misaligned address access
#define BURN_UNALIGNED_READ16(x) (\
	(UINT16) ((UINT8 *) (x))[1] << 8 | \
	(UINT16) ((UINT8 *) (x))[0])
#define BURN_UNALIGNED_READ32(x) (\
	(UINT32) ((UINT8 *) (x))[3] << 24 | \
	(UINT32) ((UINT8 *) (x))[2] << 16 | \
	(UINT32) ((UINT8 *) (x))[1] << 8 | \
	(UINT32) ((UINT8 *) (x))[0])
#define BURN_UNALIGNED_WRITE16(x, y) ( \
	((UINT8 *) (x))[0] = (UINT8) (y), \
	((UINT8 *) (x))[1] = (UINT8) ((y) >> 8))
#define BURN_UNALIGNED_WRITE32(x, y) ( \
	((UINT8 *) (x))[0] = (UINT8) (y), \
	((UINT8 *) (x))[1] = (UINT8) ((y) >> 8), \
	((UINT8 *) (x))[2] = (UINT8) ((y) >> 16), \
	((UINT8 *) (x))[3] = (UINT8) ((y) >> 24))

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
	INT32 (*GetHDDInfo)(struct BurnHDDInfo* pri, UINT32 i);			// Function to get hdd info
	INT32 (*GetHDDName)(char** pszName, UINT32 i, INT32 nAka);		// Function to get the possible names for each hdd
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
void BurnSetMouseDivider(INT32 nDivider);
INT32 BurnByteswap(UINT8* pMem, INT32 nLen);
void BurnNibbleExpand(UINT8 *source, UINT8 *dst, INT32 length, INT32 swap, UINT8 nxor);
INT32 BurnClearScreen();

// from intf/input/inp_interface.cpp
extern INT32 nInputIntfMouseDivider;

// recording / netgame helper
#ifndef __LIBRETRO__
INT32 is_netgame_or_recording();
#else
inline static INT32 is_netgame_or_recording()
{
	return kNetGame;
}
#endif


// load.cpp

/*
	Flags for use with BurnLoadRomExt

	GROUP(x)		load this many bytes, then skip by nGap from start position (flag is optional)
	REVERSE			load the bytes in a group in reverse order (0,1,2,3 -> 3,2,1,0)
	INVERT			Src ^= 0xff
	BYTESWAP		change order of bytes from 0,1,2,3 to 1,0,3,2
	NIBBLES			Dest[0] = (byte & & 0xf); Dest[1] = (byte >> 4) & 0xf;
	XOR			Dest ^= Src
*/

#define LD_GROUP(x)	((x) & 0xff) // 256 - plenty
#define LD_REVERSE	(1<<8)
#define LD_INVERT	(1<<9)
#define LD_BYTESWAP	(1<<10)
#define LD_NIBBLES	(1<<11)
#define LD_XOR		(1<<12)

INT32 BurnLoadRomExt(UINT8 *Dest, INT32 i, INT32 nGap, INT32 nFlags);
INT32 BurnLoadRom(UINT8* Dest, INT32 i, INT32 nGap);
INT32 BurnXorRom(UINT8 *Dest, INT32 i, INT32 nGap);
INT32 BurnLoadBitField(UINT8* pDest, UINT8* pSrc, INT32 nField, INT32 nSrcLen);

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

struct cpu_core_config {
	char cpu_name[32];
	void (*open)(INT32);		// cpu open
	void (*close)();		// cpu close

	UINT8 (*read)(UINT32);		// read
	void (*write)(UINT32, UINT8);	// write
	INT32 (*active)();		// active cpu
	INT32 (*totalcycles)();		// total cycles
	void (*newframe)();		// new frame'
	INT32 (*idle)(INT32);	// idle cycles

	void (*irq)(INT32, INT32, INT32);	// set irq, cpu, line, status

	INT32 (*run)(INT32);		// execute cycles
	void (*runend)();		// end run
	void (*reset)();		// reset cpu

	UINT64 nMemorySize;		// how large is our memory range?
	UINT32 nAddressFlags;	// fix endianness for some cpus
};

struct cheat_core {
	cpu_core_config *cpuconfig;

	INT32 nCPU;			// which cpu
};

#define MB_CHEAT_ENDI_SWAP 0x8000 // multibyte cheat needs swap on cheat-write

void CpuCheatRegister(INT32 type, cpu_core_config *config);

cheat_core *GetCpuCheatRegister(INT32 nCPU);
cpu_core_config *GetCpuCoreConfig(INT32 nCPU);

// burn_memory.cpp
void BurnInitMemoryManager();
UINT8 *_BurnMalloc(INT32 size, char *file, INT32 line); // internal use only :)
UINT8 *BurnRealloc(void *ptr, INT32 size);
void _BurnFree(void *ptr); // internal use only :)
#define BurnFree(x) do {_BurnFree(x); x = NULL; } while (0)
#define BurnMalloc(x) _BurnMalloc(x, __FILE__, __LINE__)
void BurnExitMemoryManager();

// ---------------------------------------------------------------------------
// sound routes
#define BURN_SND_ROUTE_LEFT			1
#define BURN_SND_ROUTE_RIGHT		2
#define BURN_SND_ROUTE_BOTH			(BURN_SND_ROUTE_LEFT | BURN_SND_ROUTE_RIGHT)
// the following 2 are only supported in ay8910 and flt_rc
#define BURN_SND_ROUTE_PANLEFT      4
#define BURN_SND_ROUTE_PANRIGHT     8

// ---------------------------------------------------------------------------
// Debug Tracker

extern UINT8 Debug_BurnTransferInitted;
extern UINT8 Debug_BurnGunInitted;
extern UINT8 Debug_BurnLedInitted;
extern UINT8 Debug_BurnShiftInitted;
extern UINT8 Debug_HiscoreInitted;
extern UINT8 Debug_GenericTilesInitted;

extern UINT8 DebugDev_8255PPIInitted;
extern UINT8 DebugDev_8257DMAInitted;
extern UINT8 DebugDev_EEPROMInitted;
extern UINT8 DebugDev_PandoraInitted;
extern UINT8 DebugDev_SeibuSndInitted;
extern UINT8 DebugDev_SknsSprInitted;
extern UINT8 DebugDev_SlapsticInitted;
extern UINT8 DebugDev_T5182Initted;
extern UINT8 DebugDev_TimeKprInitted;
extern UINT8 DebugDev_Tms34061Initted;
extern UINT8 DebugDev_V3021Initted;
extern UINT8 DebugDev_VDCInitted;

extern UINT8 DebugSnd_AY8910Initted;
extern UINT8 DebugSnd_Y8950Initted;
extern UINT8 DebugSnd_YM2151Initted;
extern UINT8 DebugSnd_YM2203Initted;
extern UINT8 DebugSnd_YM2413Initted;
extern UINT8 DebugSnd_YM2608Initted;
extern UINT8 DebugSnd_YM2610Initted;
extern UINT8 DebugSnd_YM2612Initted;
extern UINT8 DebugSnd_YM3526Initted;
extern UINT8 DebugSnd_YM3812Initted;
extern UINT8 DebugSnd_YMF278BInitted;
extern UINT8 DebugSnd_YMF262Initted;
extern UINT8 DebugSnd_YMF271Initted;
extern UINT8 DebugSnd_C6280Initted;
extern UINT8 DebugSnd_DACInitted;
extern UINT8 DebugSnd_ES5506Initted;
extern UINT8 DebugSnd_ES8712Initted;
extern UINT8 DebugSnd_FilterRCInitted;
extern UINT8 DebugSnd_ICS2115Initted;
extern UINT8 DebugSnd_IremGA20Initted;
extern UINT8 DebugSnd_K005289Initted;
extern UINT8 DebugSnd_K007232Initted;
extern UINT8 DebugSnd_K051649Initted;
extern UINT8 DebugSnd_K053260Initted;
extern UINT8 DebugSnd_K054539Initted;
extern UINT8 DebugSnd_MSM5205Initted;
extern UINT8 DebugSnd_MSM5232Initted;
extern UINT8 DebugSnd_MSM6295Initted;
extern UINT8 DebugSnd_NamcoSndInitted;
extern UINT8 DebugSnd_NESAPUSndInitted;
extern UINT8 DebugSnd_RF5C68Initted;
extern UINT8 DebugSnd_SAA1099Initted;
extern UINT8 DebugSnd_SamplesInitted;
extern UINT8 DebugSnd_SegaPCMInitted;
extern UINT8 DebugSnd_SN76496Initted;
extern UINT8 DebugSnd_UPD7759Initted;
extern UINT8 DebugSnd_VLM5030Initted;
extern UINT8 DebugSnd_X1010Initted;
extern UINT8 DebugSnd_YMZ280BInitted;

extern UINT8 DebugCPU_ARM7Initted;
extern UINT8 DebugCPU_ARMInitted;
extern UINT8 DebugCPU_H6280Initted;
extern UINT8 DebugCPU_HD6309Initted;
extern UINT8 DebugCPU_KonamiInitted;
extern UINT8 DebugCPU_M6502Initted;
extern UINT8 DebugCPU_M6800Initted;
extern UINT8 DebugCPU_M6805Initted;
extern UINT8 DebugCPU_M6809Initted;
extern UINT8 DebugCPU_S2650Initted;
extern UINT8 DebugCPU_SekInitted;
extern UINT8 DebugCPU_VezInitted;
extern UINT8 DebugCPU_ZetInitted;
extern UINT8 DebugCPU_PIC16C5XInitted;
extern UINT8 DebugCPU_I8039Initted;
extern UINT8 DebugCPU_SH2Initted;

void DebugTrackerExit();

#endif
