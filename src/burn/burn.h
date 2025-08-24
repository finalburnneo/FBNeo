// FinalBurn Neo - Emulator for MC68000/Z80 based arcade games
//            Refer to the "license.txt" file for more info

// Burner emulation library
#ifndef _BURNH_H
#define _BURNH_H

#ifdef __cplusplus
 extern "C" {
#endif

#if !defined (_WIN32)
 #define __cdecl
#endif

#if !defined (_MSC_VER) && defined(FASTCALL)
 #undef __fastcall
 #define __fastcall __attribute__((fastcall))
#endif

#ifndef MAX_PATH
 #define MAX_PATH 	260
#endif

#include <time.h>

extern TCHAR szAppHiscorePath[MAX_PATH];
extern TCHAR szAppSamplesPath[MAX_PATH];
extern TCHAR szAppHDDPath[MAX_PATH];
extern TCHAR szAppBlendPath[MAX_PATH];
extern TCHAR szAppEEPROMPath[MAX_PATH];

// Macro to determine the size of a struct up to and including "member"
#define STRUCT_SIZE_HELPER(type, member) offsetof(type, member) + sizeof(((type*)0)->member)

// Enable the MAME logerror() function in debug builds
// #define MAME_USE_LOGERROR

// Give access to the CPUID function for various compilers
#if defined (__GNUC__)
 #define CPUID(f,ra,rb,rc,rd) __asm__ __volatile__ ("cpuid"											\
													: "=a" (ra), "=b" (rb), "=c" (rc), "=d" (rd)	\
													: "a"  (f)										\
												   );
#elif defined (_MSC_VER)
 #define CPUID(f,ra,rb,rc,rd) __asm { __asm mov		eax, f		\
									  __asm cpuid				\
									  __asm mov		ra, eax		\
									  __asm mov		rb, ebx		\
									  __asm mov		rc, ecx		\
									  __asm mov		rd, edx }
#else
 #define CPUID(f,ra,rb,rc,rd)
#endif

#ifndef BUILD_X86_ASM
 #undef CPUID
 #define CPUID(f,ra,rb,rc,rd)
#endif

#ifdef _UNICODE
 #define SEPERATOR_1 " \u2022 "
 #define SEPERATOR_2 " \u25E6 "
#else
 #define SEPERATOR_1 " ~ "
 #define SEPERATOR_2 " ~ "
#endif

#ifdef _UNICODE
 #define WRITE_UNICODE_BOM(file) { UINT16 BOM[] = { 0xFEFF }; fwrite(BOM, 2, 1, file); }
#else
 #define WRITE_UNICODE_BOM(file)
#endif

typedef unsigned char						UINT8;
typedef signed char 						INT8;
typedef unsigned short						UINT16;
typedef signed short						INT16;
typedef unsigned int						UINT32;
typedef signed int							INT32;
#ifdef _MSC_VER
typedef signed __int64						INT64;
typedef unsigned __int64					UINT64;
#else
__extension__ typedef unsigned long long	UINT64;
__extension__ typedef long long				INT64;
#endif

#include "state.h"
#include "cheat.h"
#include "hiscore.h"

extern INT32 nBurnVer;						// Version number of the library

enum BurnCartrigeCommand { CART_INIT_START, CART_INIT_END, CART_EXIT };

// ---------------------------------------------------------------------------
// Callbacks

// Application-defined rom loading function
extern INT32 (__cdecl *BurnExtLoadRom)(UINT8* Dest, INT32* pnWrote, INT32 i);

// Application-defined progress indicator functions
extern INT32 (__cdecl *BurnExtProgressRangeCallback)(double dProgressRange);
extern INT32 (__cdecl *BurnExtProgressUpdateCallback)(double dProgress, const TCHAR* pszText, bool bAbs);

// Application-defined catridge initialisation function
extern INT32 (__cdecl *BurnExtCartridgeSetupCallback)(BurnCartrigeCommand nCommand);

// Application-defined colour conversion function
extern UINT32 (__cdecl *BurnHighCol) (INT32 r, INT32 g, INT32 b, INT32 i);

// ---------------------------------------------------------------------------

extern UINT32 nCurrentFrame;

inline static INT32 GetCurrentFrame() {
	return nCurrentFrame;
}

inline static void SetCurrentFrame(const UINT32 n) {
	nCurrentFrame = n;
}

// ---------------------------------------------------------------------------
// Driver info structures

// ROMs

#define BRF_PRG				(1 << 20)
#define BRF_GRA				(1 << 21)
#define BRF_SND				(1 << 22)

#define BRF_ESS				(1 << 24)
#define BRF_BIOS			(1 << 25)
#define BRF_SELECT			(1 << 26)
#define BRF_OPT				(1 << 27)
#define BRF_NODUMP			(1 << 28)

struct BurnRomInfo {
	char *szName;
	UINT32 nLen;
	UINT32 nCrc;
	UINT32 nType;
};

struct BurnSampleInfo {
	char *szName;
	UINT32 nFlags;
};

struct BurnHDDInfo {
	char *szName;
	UINT32 nLen;
	UINT32 nCrc;
};

// ---------------------------------------------------------------------------

// Rom Data

struct RomDataInfo {
	char szZipName[MAX_PATH];
	char szDrvName[MAX_PATH];
	char szExtraRom[MAX_PATH];
	wchar_t szOldName[MAX_PATH];
	wchar_t szFullName[MAX_PATH];
	INT32 nDriverId;
	INT32 nDescCount;
};

extern RomDataInfo* pRDI;
extern BurnRomInfo* pDataRomDesc;

char* RomdataGetDrvName();
void RomDataSetFullName();
void RomDataInit();
void RomDataExit();

// ---------------------------------------------------------------------------

// Inputs

#define BIT_DIGITAL			(1)

#define BIT_GROUP_ANALOG	(4)
#define BIT_ANALOG_REL		(4)
#define BIT_ANALOG_ABS		(5)

#define BIT_GROUP_CONSTANT	(8)
#define BIT_CONSTANT		(8)
#define BIT_DIPSWITCH		(9)

struct BurnInputInfo {
	char* szName;
	UINT8 nType;
	union {
		UINT8* pVal;					// Most inputs use a char*
		UINT16* pShortVal;				// All analog inputs use a short*
	};
	char* szInfo;
};

// DIPs

struct BurnDIPInfo {
	INT32 nInput;
	UINT8 nFlags;
	UINT8 nMask;
	UINT8 nSetting;
	char* szText;
};

#define DIP_OFFSET(x) {x, 0xf0, 0xff, 0xff, NULL},

// ---------------------------------------------------------------------------
// Common CPU definitions

// sync to nCyclesDone[]
#define CPU_RUN(num,proc) do { nCyclesDone[num] += proc ## Run(((i + 1) * nCyclesTotal[num] / nInterleave) - nCyclesDone[num]); } while (0)
#define CPU_IDLE(num,proc) do { nCyclesDone[num] += proc ## Idle(((i + 1) * nCyclesTotal[num] / nInterleave) - nCyclesDone[num]); } while (0)
#define CPU_IDLE_NULL(num) do { nCyclesDone[num] += ((i + 1) * nCyclesTotal[num] / nInterleave) - nCyclesDone[num]; } while (0)
// sync to cpuTotalCycles()
#define CPU_RUN_SYNCINT(num,proc) do { nCyclesDone[num] += proc ## Run(((i + 1) * nCyclesTotal[num] / nInterleave) - proc ## TotalCycles()); } while (0)
#define CPU_IDLE_SYNCINT(num,proc) do { nCyclesDone[num] += proc ## Idle(((i + 1) * nCyclesTotal[num] / nInterleave) - proc ## TotalCycles()); } while (0)
// sync to timer
#define CPU_RUN_TIMER(num) do { BurnTimerUpdate((i + 1) * nCyclesTotal[num] / nInterleave); if (i == nInterleave - 1) BurnTimerEndFrame(nCyclesTotal[num]); } while (0)
#define CPU_RUN_TIMER_YM3812(num) do { BurnTimerUpdateYM3812((i + 1) * nCyclesTotal[num] / nInterleave); if (i == nInterleave - 1) BurnTimerEndFrameYM3812(nCyclesTotal[num]); } while (0)
#define CPU_RUN_TIMER_YM3526(num) do { BurnTimerUpdateYM3526((i + 1) * nCyclesTotal[num] / nInterleave); if (i == nInterleave - 1) BurnTimerEndFrameYM3526(nCyclesTotal[num]); } while (0)
#define CPU_RUN_TIMER_Y8950(num) do { BurnTimerUpdateY8950((i + 1) * nCyclesTotal[num] / nInterleave); if (i == nInterleave - 1) BurnTimerEndFrameY8950(nCyclesTotal[num]); } while (0)
// speed adjuster
INT32 BurnSpeedAdjust(INT32 cyc);

#define CPU_IRQSTATUS_NONE	0
#define CPU_IRQSTATUS_ACK	1
#define CPU_IRQSTATUS_AUTO	2
#define CPU_IRQSTATUS_HOLD	4

#define CPU_IRQLINE0		0
#define CPU_IRQLINE1		1
#define CPU_IRQLINE2		2
#define CPU_IRQLINE3		3
#define CPU_IRQLINE4		4
#define CPU_IRQLINE5		5
#define CPU_IRQLINE6		6
#define CPU_IRQLINE7		7

#define CPU_IRQLINE_IRQ		CPU_IRQLINE0
#define CPU_IRQLINE_FIRQ	CPU_IRQLINE1
#define CPU_IRQLINE_NMI		0x20

#define MAP_READ		1
#define MAP_WRITE		2
#define MAP_FETCHOP		4
#define MAP_FETCHARG		8
#define MAP_FETCH		(MAP_FETCHOP|MAP_FETCHARG)
#define MAP_ROM			(MAP_READ|MAP_FETCH)
#define MAP_RAM			(MAP_ROM|MAP_WRITE)

// Macros to Allocate and Free MemIndex
#define BurnAllocMemIndex() do {                				\
	AllMem = NULL;                                 				\
	MemIndex();                                 				\
	INT32 nLen = MemEnd - (UINT8 *)0;           				\
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;	\
	memset(AllMem, 0, nLen);                       				\
	MemIndex();                                 				\
} while (0)

#define BurnFreeMemIndex() do { BurnFree(AllMem); } while (0)

// ---------------------------------------------------------------------------

extern bool bBurnUseMMX;
#ifdef BUILD_A68K
extern bool bBurnUseASMCPUEmulation;
#endif

extern UINT32 nFramesEmulated;
extern UINT32 nFramesRendered;
extern clock_t starttime;			// system time when emulation started and after roms loaded

extern bool bForce60Hz;
extern bool bSpeedLimit60hz;
extern double dForcedFrameRate;

extern bool bBurnUseBlend;

extern INT32 nBurnFPS;
extern INT32 nBurnCPUSpeedAdjust;

extern UINT32 nBurnDrvCount;		// Count of game drivers
extern UINT32 nBurnDrvActive;		// Which game driver is selected
extern INT32 nBurnDrvSubActive;		// Which sub-game driver is selected
extern UINT32 nBurnDrvSelect[8];	// Which games are selected (i.e. loaded but not necessarily active)

extern char* pszCustomNameA;
extern char szBackupNameA[MAX_PATH];
extern TCHAR szBackupNameW[MAX_PATH];

extern char** szShortNamesExArray;
extern TCHAR** szLongNamesExArray;
extern UINT32 nNamesExArray;

extern INT32 nMaxPlayers;

extern UINT8 *pBurnDraw;			// Pointer to correctly sized bitmap
extern INT32 nBurnPitch;			// Pitch between each line
extern INT32 nBurnBpp;				// Bytes per pixel (2, 3, or 4)

extern UINT8 nBurnLayer;			// Can be used externally to select which layers to show
extern UINT8 nSpriteEnable;			// Can be used externally to select which Sprites to show

extern INT32 bRunAhead;				// "Run Ahead" lag-reduction technique UI option (on/off)

extern INT32 bBurnRunAheadFrame;	// for drivers, hiscore, etc, to recognize that this is the "runahead frame"
									// for instance, you wouldn't want to apply hi-score data on a "runahead frame"

extern INT32 nBurnSoundRate;		// Samplerate of sound
extern INT32 nBurnSoundLen;			// Length in samples per frame
extern INT16* pBurnSoundOut;		// Pointer to output buffer

extern INT32 nInterpolation;		// Desired interpolation level for ADPCM/PCM sound
extern INT32 nFMInterpolation;		// Desired interpolation level for FM sound

extern UINT32 *pBurnDrvPalette;

#define PRINT_NORMAL	(0)
#define PRINT_UI		(1)
#define PRINT_IMPORTANT (2)
#define PRINT_ERROR		(3)
#define PRINT_LEVEL1	(4)
#define PRINT_LEVEL2	(5)
#define PRINT_LEVEL3	(6)
#define PRINT_LEVEL4	(7)
#define PRINT_LEVEL5	(8)
#define PRINT_LEVEL6	(9)
#define PRINT_LEVEL7	(10)
#define PRINT_LEVEL8	(11)
#define PRINT_LEVEL9	(12)
#define PRINT_LEVEL10	(13)

#ifndef bprintf
extern INT32 (__cdecl *bprintf) (INT32 nStatus, TCHAR* szFormat, ...);
#endif

INT32 BurnLibInit();
INT32 BurnLibExit();

INT32 BurnDrvInit();
INT32 BurnDrvExit();

INT32 BurnDrvCartridgeSetup(BurnCartrigeCommand nCommand);

INT32 BurnDrvFrame();
INT32 BurnDrvRedraw();
INT32 BurnRecalcPal();
INT32 BurnDrvGetPaletteEntries();

INT32 BurnSetProgressRange(double dProgressRange);
INT32 BurnUpdateProgress(double dProgressStep, const TCHAR* pszText, bool bAbs);

void BurnLocalisationSetName(char *szName, TCHAR *szLongName);

void BurnGetLocalTime(tm *nTime);                   // Retrieve local-time of machine w/tweaks for netgame and input recordings
UINT16 BurnRandom();                                // State-able Random Number Generator (0-32767)
void BurnRandomScan(INT32 nAction);                 // Must be called in driver's DrvScan() if BurnRandom() is used
void BurnRandomInit();                              // Called automatically in BurnDrvInit() / Internal use only
void BurnRandomSetSeed(UINT64 nSeed);               // Set the seed - useful for netgames / input recordings

// Handy FM default callbacks
INT32 BurnSynchroniseStream(INT32 nSoundRate);
double BurnGetTime();

// Handy debug binary-file dumper
#if defined (FBNEO_DEBUG)
void BurnDump_(char *filename, UINT8 *buffer, INT32 bufsize, INT32 append);
#define BurnDump(fn, b, bs) do { \
	bprintf(0, _T("Dumping %S (0x%x bytes) to %S\n"), #b, bs, #fn); \
	BurnDump_(fn, b, bs, 0); } while (0)

#define BurnDumpAppend(fn, b, bs) do { \
	bprintf(0, _T("Dumping %S (0x%x bytes) to %S (append)\n"), #b, bs, #fn); \
	BurnDump_(fn, b, bs, 1); } while (0)

void BurnDumpLoad_(char *filename, UINT8 *buffer, INT32 bufsize);
#define BurnDumpLoad(fn, b, bs) do { \
	bprintf(0, _T("Loading Dump %S (0x%x bytes) to %S\n"), #fn, bs, #b); \
	BurnDumpLoad_(fn, b, bs); } while (0)

#endif

// ---------------------------------------------------------------------------
// Retrieve driver information

#define DRV_NAME		 (0)
#define DRV_DATE		 (1)
#define DRV_FULLNAME	 (2)
//#define DRV_MEDIUMNAME	 (3)
#define DRV_COMMENT		 (4)
#define DRV_MANUFACTURER (5)
#define DRV_SYSTEM		 (6)
#define DRV_PARENT		 (7)
#define DRV_BOARDROM	 (8)
#define DRV_SAMPLENAME	 (9)

#define DRV_NEXTNAME	 (1 << 8)
#define DRV_ASCIIONLY	 (1 << 12)
#define DRV_UNICODEONLY	 (1 << 13)

TCHAR* BurnDrvGetText(UINT32 i);
char* BurnDrvGetTextA(UINT32 i);
wchar_t* BurnDrvGetFullNameW(UINT32 i);

INT32 BurnDrvGetIndex(char* szName);
INT32 BurnDrvGetZipName(char** pszName, UINT32 i);
INT32 BurnDrvSetZipName(char* szName, INT32 i);
INT32 BurnDrvGetRomInfo(struct BurnRomInfo *pri, UINT32 i);
INT32 BurnDrvGetRomName(char** pszName, UINT32 i, INT32 nAka);
INT32 BurnDrvGetInputInfo(struct BurnInputInfo* pii, UINT32 i);
INT32 BurnDrvGetDIPInfo(struct BurnDIPInfo* pdi, UINT32 i);
INT32 BurnDrvGetVisibleSize(INT32* pnWidth, INT32* pnHeight);
INT32 BurnDrvGetVisibleOffs(INT32* pnLeft, INT32* pnTop);
INT32 BurnDrvGetFullSize(INT32* pnWidth, INT32* pnHeight);
INT32 BurnDrvGetAspect(INT32* pnXAspect, INT32* pnYAspect);
INT32 BurnDrvGetHardwareCode();
INT32 BurnDrvGetFlags();
bool BurnDrvIsWorking();
INT32 BurnDrvGetMaxPlayers();
INT32 BurnDrvSetVisibleSize(INT32 pnWidth, INT32 pnHeight);
INT32 BurnDrvSetAspect(INT32 pnXAspect, INT32 pnYAspect);
INT32 BurnDrvGetGenreFlags();
INT32 BurnDrvGetFamilyFlags();
INT32 BurnDrvGetSampleInfo(struct BurnSampleInfo *pri, UINT32 i);
INT32 BurnDrvGetSampleName(char** pszName, UINT32 i, INT32 nAka);
INT32 BurnDrvGetHDDInfo(struct BurnHDDInfo *pri, UINT32 i);
INT32 BurnDrvGetHDDName(char** pszName, UINT32 i, INT32 nAka);
char* BurnDrvGetSourcefile();

void Reinitialise(); // re-inits everything, including UI window
void ReinitialiseVideo(); // re-init's video w/ new resolution/aspect ratio (see drv/megadrive.cpp)

// ---------------------------------------------------------------------------
// IPS Control

#define IPS_NOT_PROTECT		(1 <<  0)	// Protection switche for NeoGeo, etc.
#define IPS_PGM_SPRHACK		(1 <<  1)	// For PGM hacks...
#define IPS_PGM_MAPHACK		(1 <<  2)	// Id.
#define IPS_PGM_SNDOFFS		(1 <<  3)	// Id.
#define IPS_LOAD_EXPAND		(1 <<  4)	// Change temporary cache size on double load.
#define IPS_EXTROM_INCL		(1 <<  5)	// Extra rom.
#define IPS_PRG1_EXPAND		(1 <<  6)	// Additional request for prg length.
#define IPS_PRG2_EXPAND		(1 <<  7)	// Id.
#define IPS_GRA1_EXPAND		(1 <<  8)	// Additional request for gfx length.
#define IPS_GRA2_EXPAND		(1 <<  9)	// Id.
#define IPS_GRA3_EXPAND		(1 << 10)	// Id.
#define IPS_ACPU_EXPAND		(1 << 11)	// Additional request for audio cpu length.
#define IPS_SND1_EXPAND		(1 << 12)	// Additional request for snd length.
#define IPS_SND2_EXPAND		(1 << 13)	// Id.
#define IPS_SNES_VRAMHK		(1 << 14)	// Allow invalid vram writes.
#define IPS_NEO_RAMHACK		(1 << 15)	// Neo Geo 68kram hack.

enum IpsRomTypes { EXP_FLAG, LOAD_ROM, EXTR_ROM, PRG1_ROM, PRG2_ROM, GRA1_ROM, GRA2_ROM, GRA3_ROM, ACPU_ROM, SND1_ROM, SND2_ROM };
extern UINT32 nIpsDrvDefine, nIpsMemExpLen[SND2_ROM + 1];

extern bool bDoIpsPatch;

void IpsApplyPatches(UINT8* base, char* rom_name, UINT32 rom_crc, bool readonly = false);

// ---------------------------------------------------------------------------
// MISC Helper / utility functions, etc
int BurnComputeSHA1(const UINT8 *buffer, int buffer_size, char *hash_str);
//int BurnComputeSHA1(const char *filename, char *hash_str);

// ---------------------------------------------------------------------------
// Flags used with the Burndriver structure

// Flags for the flags member
#define BDF_GAME_NOT_WORKING							(0)
#define BDF_GAME_WORKING								(1 << 0)
#define BDF_ORIENTATION_FLIPPED							(1 << 1)
#define BDF_ORIENTATION_VERTICAL						(1 << 2)
#define BDF_BOARDROM									(1 << 3)
#define BDF_CLONE										(1 << 4)
#define BDF_BOOTLEG										(1 << 5)
#define BDF_PROTOTYPE									(1 << 6)
#define BDF_HACK										(1 << 7)
#define BDF_HOMEBREW									(1 << 8)
#define BDF_DEMO										(1 << 9)
#define BDF_16BIT_ONLY									(1 << 10)
#define BDF_32BIT_ONLY									(1 << 11)
#define BDF_HISCORE_SUPPORTED							(1 << 12)
#define BDF_RUNAHEAD_DRAWSYNC							(1 << 13)
#define BDF_RUNAHEAD_DISABLED							(1 << 14)

// Flags for the hardware member
// Format: 0xDDEEFFFF, where DD: Manufacturer, EE: Hardware platform, FFFF: Flags (used by driver)

#define HARDWARE_PUBLIC_MASK							(0x7FFF0000)

#define HARDWARE_PREFIX_CARTRIDGE				 ((INT32)0x80000000)

#define HARDWARE_PREFIX_MISC_PRE90S						(0x00000000)
#define HARDWARE_PREFIX_CAPCOM							(0x01000000)
#define HARDWARE_PREFIX_SEGA							(0x02000000)
#define HARDWARE_PREFIX_KONAMI							(0x03000000)
#define HARDWARE_PREFIX_TOAPLAN							(0x04000000)
#define HARDWARE_PREFIX_SNK								(0x05000000)
#define HARDWARE_PREFIX_CAVE							(0x06000000)
#define HARDWARE_PREFIX_CPS2							(0x07000000)
#define HARDWARE_PREFIX_IGS_PGM							(0x08000000)
#define HARDWARE_PREFIX_CPS3							(0x09000000)
#define HARDWARE_PREFIX_MISC_POST90S					(0x0a000000)
#define HARDWARE_PREFIX_TAITO							(0x0b000000)
#define HARDWARE_PREFIX_SEGA_MEGADRIVE					(0x0c000000)
#define HARDWARE_PREFIX_PSIKYO							(0x0d000000)
#define HARDWARE_PREFIX_KANEKO							(0x0e000000)
#define HARDWARE_PREFIX_PACMAN							(0x0f000000)
#define HARDWARE_PREFIX_GALAXIAN						(0x10000000)
#define HARDWARE_PREFIX_IREM							(0x11000000)
#define HARDWARE_PREFIX_DATAEAST						(0x13000000)
#define HARDWARE_PREFIX_CAPCOM_MISC						(0x14000000)
#define HARDWARE_PREFIX_SETA							(0x15000000)
#define HARDWARE_PREFIX_TECHNOS							(0x16000000)
#define HARDWARE_PREFIX_PCENGINE						(0x17000000)
#define HARDWARE_PREFIX_SEGA_MASTER_SYSTEM				(0x18000000)
#define HARDWARE_PREFIX_SEGA_SG1000						(0x19000000)
#define HARDWARE_PREFIX_COLECO							(0x1A000000)
#define HARDWARE_PREFIX_MIDWAY							(0x1B000000)
#define HARDWARE_PREFIX_SEGA_GAME_GEAR					(0x12000000)
#define HARDWARE_PREFIX_MSX                             (0x1C000000)
#define HARDWARE_PREFIX_SPECTRUM                        (0x1D000000)
#define HARDWARE_PREFIX_NES                             (0x1E000000)
#define HARDWARE_PREFIX_FDS                             (0x1F000000)
#define HARDWARE_PREFIX_NGP                             (0x20000000)
#define HARDWARE_PREFIX_CHANNELF                        (0x21000000)
#define HARDWARE_PREFIX_SNES                            (0x22000000)

#define HARDWARE_SNK_NGP								(HARDWARE_PREFIX_NGP | 0x00000000)
#define HARDWARE_SNK_NGPC								(HARDWARE_PREFIX_NGP | 0x00000001) // must not be 0x10000

#define HARDWARE_MISC_PRE90S							(HARDWARE_PREFIX_MISC_PRE90S)
#define HARDWARE_NVS									(HARDWARE_PREFIX_MISC_PRE90S | 0x00010000)
#define HARDWARE_MISC_POST90S							(HARDWARE_PREFIX_MISC_POST90S)

#define HARDWARE_CAPCOM_CPS1							(HARDWARE_PREFIX_CAPCOM | 0x00010000)
#define HARDWARE_CAPCOM_CPS1_QSOUND 					(HARDWARE_PREFIX_CAPCOM | 0x00020000)
#define HARDWARE_CAPCOM_CPS1_GENERIC 					(HARDWARE_PREFIX_CAPCOM | 0x00030000)
#define HARDWARE_CAPCOM_CPSCHANGER						(HARDWARE_PREFIX_CAPCOM | 0x00040000)
#define HARDWARE_CAPCOM_CPS2							(HARDWARE_PREFIX_CPS2 | 0x00010000)
#define HARDWARE_CAPCOM_CPS2_SIMM						(0x0002)

#define HARDWARE_SEGA_SYSTEMX							(HARDWARE_PREFIX_SEGA | 0x00010000)
#define HARDWARE_SEGA_SYSTEMY							(HARDWARE_PREFIX_SEGA | 0x00020000)
#define HARDWARE_SEGA_SYSTEM16A							(HARDWARE_PREFIX_SEGA | 0x00030000)
#define HARDWARE_SEGA_SYSTEM16B 						(HARDWARE_PREFIX_SEGA | 0x00040000)
#define HARDWARE_SEGA_SYSTEM16M							(HARDWARE_PREFIX_SEGA | 0x00050000)
#define HARDWARE_SEGA_SYSTEM18							(HARDWARE_PREFIX_SEGA | 0x00060000)
#define HARDWARE_SEGA_HANGON							(HARDWARE_PREFIX_SEGA | 0x00070000)
#define HARDWARE_SEGA_OUTRUN							(HARDWARE_PREFIX_SEGA | 0x00080000)
#define HARDWARE_SEGA_SYSTEM1							(HARDWARE_PREFIX_SEGA | 0x00090000)
#define HARDWARE_SEGA_MISC								(HARDWARE_PREFIX_SEGA | 0x000a0000)
#define HARDWARE_SEGA_SYSTEM24							(HARDWARE_PREFIX_SEGA | 0x000b0000)
#define HARDWARE_SEGA_SYSTEM32							(HARDWARE_PREFIX_SEGA | 0x000c0000)

#define HARDWARE_SEGA_PCB_MASK							(0x0f)
#define HARDWARE_SEGA_5358								(0x01)
#define HARDWARE_SEGA_5358_SMALL						(0x02)
#define HARDWARE_SEGA_5704								(0x03)
#define HARDWARE_SEGA_5521								(0x04)
#define HARDWARE_SEGA_5797								(0x05)
#define HARDWARE_SEGA_5704_PS2							(0x06)
#define HARDWARE_SEGA_171_SHADOW						(0x07)
#define HARDWARE_SEGA_171_5874							(0x08)
#define HARDWARE_SEGA_171_5987							(0x09)
#define HARDWARE_SEGA_837_7525							(0x0a)

#define HARDWARE_SEGA_FD1089A_ENC						(0x0010)
#define HARDWARE_SEGA_FD1089B_ENC						(0x0020)
#define HARDWARE_SEGA_MC8123_ENC						(0x0040)
#define HARDWARE_SEGA_FD1094_ENC						(0x0080)
#define HARDWARE_SEGA_SPRITE_LOAD32						(0x0100)
#define HARDWARE_SEGA_YM2203							(0x0200)
#define HARDWARE_SEGA_INVERT_TILES						(0x0400)
#define HARDWARE_SEGA_YM2413							(0x0800)
#define HARDWARE_SEGA_FD1094_ENC_CPU2					(0x1000)
#define HARDWARE_SEGA_ISGSM								(0x2000)

#define HARDWARE_KONAMI_68K_Z80							(HARDWARE_PREFIX_KONAMI | 0x00010000)
#define HARDWARE_KONAMI_68K_ONLY						(HARDWARE_PREFIX_KONAMI | 0x00020000)

#define HARDWARE_TOAPLAN_RAIZING						(HARDWARE_PREFIX_TOAPLAN | 0x00010000)
#define HARDWARE_TOAPLAN_68K_Zx80						(HARDWARE_PREFIX_TOAPLAN | 0x00020000)
#define HARDWARE_TOAPLAN_68K_ONLY						(HARDWARE_PREFIX_TOAPLAN | 0x00030000)
#define HARDWARE_TOAPLAN_MISC							(HARDWARE_PREFIX_TOAPLAN | 0x00040000)

#define HARDWARE_SNK_NEOGEO								(HARDWARE_PREFIX_SNK | 0x00010000)
#define HARDWARE_SNK_SWAPP								(0x0001)	// Swap code roms
#define HARDWARE_SNK_SWAPV								(0x0002)	// Swap sound roms
#define HARDWARE_SNK_SWAPC								(0x0004)	// Swap sprite roms
#define HARDWARE_SNK_CMC42								(0x0008)	// CMC42 encryption chip
#define HARDWARE_SNK_CMC50								(0x0010)	// CMC50 encryption chip
#define HARDWARE_SNK_ALTERNATE_TEXT						(0x0020)	// KOF2000 text layer banks
#define HARDWARE_SNK_SMA_PROTECTION						(0x0040)	// SMA protection
#define HARDWARE_SNK_KOF2K3								(0x0080)	// KOF2K3 hardware
#define HARDWARE_SNK_ENCRYPTED_M1						(0x0100)	// M1 encryption
#define HARDWARE_SNK_P32								(0x0200)	// SWAP32 P ROMs
#define HARDWARE_SNK_SPRITE32							(0x0400)

#define HARDWARE_SNK_CONTROLMASK						(0xF000)
#define HARDWARE_SNK_JOYSTICK							(0x0000)	// Uses joysticks
#define HARDWARE_SNK_PADDLE								(0x1000)	// Uses joysticks or paddles
#define HARDWARE_SNK_TRACKBALL							(0x2000)	// Uses a trackball
#define HARDWARE_SNK_4_JOYSTICKS						(0x3000)	// Uses 4 joysticks
#define HARDWARE_SNK_MAHJONG							(0x4000)	// Uses a special mahjong controller
#define HARDWARE_SNK_GAMBLING							(0x5000)	// Uses gambling controls

#define HARDWARE_SNK_MVS								(HARDWARE_PREFIX_SNK | 0x00020000)
#define HARDWARE_SNK_NEOCD								(HARDWARE_PREFIX_SNK | 0x00030000)
#define HARDWARE_SNK_DEDICATED_PCB						(HARDWARE_PREFIX_SNK | 0x00040000)

#define HARDWARE_CAVE_68K_ONLY							(HARDWARE_PREFIX_CAVE)
#define HARDWARE_CAVE_68K_Z80							(HARDWARE_PREFIX_CAVE | 0x0001)
#define HARDWARE_CAVE_M6295								(0x0002)
#define HARDWARE_CAVE_YM2151							(0x0004)
#define HARDWARE_CAVE_CV1000							(HARDWARE_PREFIX_CAVE | 0x00010000)

#define HARDWARE_IGS_PGM								(HARDWARE_PREFIX_IGS_PGM)
#define HARDWARE_IGS_USE_ARM_CPU						(0x0001)

#define HARDWARE_CAPCOM_CPS3							(HARDWARE_PREFIX_CPS3)
#define HARDWARE_CAPCOM_CPS3_NO_CD   					(0x0001)

#define HARDWARE_TAITO_TAITOZ							(HARDWARE_PREFIX_TAITO | 0x00010000)
#define HARDWARE_TAITO_TAITOF2							(HARDWARE_PREFIX_TAITO | 0x00020000)
#define HARDWARE_TAITO_MISC								(HARDWARE_PREFIX_TAITO | 0x00030000)
#define HARDWARE_TAITO_TAITOX							(HARDWARE_PREFIX_TAITO | 0x00040000)
#define HARDWARE_TAITO_TAITOB							(HARDWARE_PREFIX_TAITO | 0x00050000)

#define HARDWARE_IREM_M62								(HARDWARE_PREFIX_IREM | 0x00010000)
#define HARDWARE_IREM_M63								(HARDWARE_PREFIX_IREM | 0x00020000)
#define HARDWARE_IREM_M72								(HARDWARE_PREFIX_IREM | 0x00030000)
#define HARDWARE_IREM_M90								(HARDWARE_PREFIX_IREM | 0x00040000)
#define HARDWARE_IREM_M92								(HARDWARE_PREFIX_IREM | 0x00050000)
#define HARDWARE_IREM_MISC								(HARDWARE_PREFIX_IREM | 0x00060000)

#define HARDWARE_SEGA_MASTER_SYSTEM						(HARDWARE_PREFIX_SEGA_MASTER_SYSTEM)

#define HARDWARE_SMS_MAPPER_CODIES						(0x01)
#define HARDWARE_SMS_MAPPER_MSX							(0x02)
#define HARDWARE_SMS_MAPPER_MSX_NEMESIS					(0x03)
#define HARDWARE_SMS_MAPPER_KOREA    					(0x04)
#define HARDWARE_SMS_MAPPER_KOREA8K 					(0x05)
#define HARDWARE_SMS_MAPPER_KOREA16K 					(0x06)
#define HARDWARE_SMS_MAPPER_4PAK     					(0x07)
#define HARDWARE_SMS_MAPPER_XIN1     					(0x08)
#define HARDWARE_SMS_MAPPER_WONDERKID					(0x09)
#define HARDWARE_SMS_MAPPER_NONE     					(0x0F)

#define HARDWARE_SMS_CONTROL_PADDLE						(0x00010)

#define HARDWARE_SMS_NO_CART_HEADER						(0x01000)
#define HARDWARE_SMS_GG_SMS_MODE						(0x02000)
#define HARDWARE_SMS_DISPLAY_PAL						(0x04000)
#define HARDWARE_SMS_JAPANESE							(0x08000)

#define HARDWARE_SEGA_GAME_GEAR							(HARDWARE_PREFIX_SEGA_GAME_GEAR)

#define HARDWARE_SEGA_MEGADRIVE							(HARDWARE_PREFIX_SEGA_MEGADRIVE)

#define HARDWARE_SEGA_SG1000                            (HARDWARE_PREFIX_SEGA_SG1000)
#define HARDWARE_SEGA_SG1000_RAMEXP_A                   (0x1000)
#define HARDWARE_SEGA_SG1000_RAMEXP_B                   (0x2000)
#define HARDWARE_SEGA_SG1000_RAMEXP_2K                  (0x4000)
#define HARDWARE_SEGA_SG1000_RAMEXP_8K                  (0x8000)
#define HARDWARE_COLECO                                 (HARDWARE_PREFIX_COLECO)

#define HARDWARE_MSX                                    (HARDWARE_PREFIX_MSX)
#define HARDWARE_MSX_MAPPER_ASCII8                      (0x01)
#define HARDWARE_MSX_MAPPER_ASCII8_SRAM                 (0x01)
#define HARDWARE_MSX_MAPPER_ASCII16                     (0x02)
#define HARDWARE_MSX_MAPPER_ASCII16_SRAM                (0x02)
#define HARDWARE_MSX_MAPPER_KONAMI                      (0x03)
#define HARDWARE_MSX_MAPPER_KONAMI_SCC                  (0x04)
#define HARDWARE_MSX_MAPPER_BASIC                       (0x05)
#define HARDWARE_MSX_MAPPER_DOOLY                       (0x06)
#define HARDWARE_MSX_MAPPER_RTYPE                       (0x07)
#define HARDWARE_MSX_MAPPER_CROSS_BLAIM                 (0x08)
#define HARDWARE_MSX_MAPPER_48K                         (0x09)

#define HARDWARE_SEGA_MEGADRIVE_PCB_SEGA_EEPROM			(1)
#define HARDWARE_SEGA_MEGADRIVE_PCB_SEGA_SRAM			(2)
#define HARDWARE_SEGA_MEGADRIVE_PCB_SEGA_FRAM			(3)
#define HARDWARE_SEGA_MEGADRIVE_PCB_CM_JCART			(4)
#define HARDWARE_SEGA_MEGADRIVE_PCB_CM_JCART_SEPROM		(5)
#define HARDWARE_SEGA_MEGADRIVE_PCB_CODE_MASTERS		(6)
#define HARDWARE_SEGA_MEGADRIVE_PCB_SSF2				(7)
#define HARDWARE_SEGA_MEGADRIVE_PCB_GAME_KANDUME		(8)
#define HARDWARE_SEGA_MEGADRIVE_PCB_BEGGAR				(9)
#define HARDWARE_SEGA_MEGADRIVE_PCB_NBA_JAM				(10)
#define HARDWARE_SEGA_MEGADRIVE_PCB_NBA_JAM_TE			(11)
#define HARDWARE_SEGA_MEGADRIVE_PCB_NFL_QB_96			(12)
#define HARDWARE_SEGA_MEGADRIVE_PCB_C_SLAM				(13)
#define HARDWARE_SEGA_MEGADRIVE_PCB_EA_NHLPA			(14)
#define HARDWARE_SEGA_MEGADRIVE_PCB_LIONK3				(15)
#define HARDWARE_SEGA_MEGADRIVE_PCB_SDK99				(16)
#define HARDWARE_SEGA_MEGADRIVE_PCB_SKINGKONG			(17)
#define HARDWARE_SEGA_MEGADRIVE_PCB_REDCL_EN			(18)
#define HARDWARE_SEGA_MEGADRIVE_PCB_RADICA				(19)
#define HARDWARE_SEGA_MEGADRIVE_PCB_KOF98				(20)
#define HARDWARE_SEGA_MEGADRIVE_PCB_KOF99				(21)
#define HARDWARE_SEGA_MEGADRIVE_PCB_SOULBLAD			(22)
#define HARDWARE_SEGA_MEGADRIVE_PCB_MJLOVER				(23)
#define HARDWARE_SEGA_MEGADRIVE_PCB_SQUIRRELK			(24)
#define HARDWARE_SEGA_MEGADRIVE_PCB_SMOUSE				(25)
#define HARDWARE_SEGA_MEGADRIVE_PCB_SMB					(26)
#define HARDWARE_SEGA_MEGADRIVE_PCB_SMB2				(27)
#define HARDWARE_SEGA_MEGADRIVE_PCB_KAIJU				(28)
#define HARDWARE_SEGA_MEGADRIVE_PCB_CHINFIGHT3			(29)
#define HARDWARE_SEGA_MEGADRIVE_PCB_LIONK2				(30)
#define HARDWARE_SEGA_MEGADRIVE_PCB_BUGSLIFE			(31)
#define HARDWARE_SEGA_MEGADRIVE_PCB_ELFWOR				(32)
#define HARDWARE_SEGA_MEGADRIVE_PCB_ROCKMANX3			(33)
#define HARDWARE_SEGA_MEGADRIVE_PCB_SBUBBOB				(34)
#define HARDWARE_SEGA_MEGADRIVE_PCB_REALTEC				(35)
#define HARDWARE_SEGA_MEGADRIVE_PCB_MC_SUP19IN1			(36)
#define HARDWARE_SEGA_MEGADRIVE_PCB_MC_SUP15IN1			(37)
#define HARDWARE_SEGA_MEGADRIVE_PCB_MC_12IN1			(38)
#define HARDWARE_SEGA_MEGADRIVE_PCB_TOPFIGHTER			(39)
#define HARDWARE_SEGA_MEGADRIVE_PCB_POKEMON				(40)
#define HARDWARE_SEGA_MEGADRIVE_PCB_POKEMON2			(41)
#define HARDWARE_SEGA_MEGADRIVE_PCB_MULAN				(42)
#define HARDWARE_SEGA_MEGADRIVE_PCB_16ZHANG             (43)
#define HARDWARE_SEGA_MEGADRIVE_PCB_CHAOJIMJ            (44)
#define HARDWARE_SEGA_MEGADRIVE_TEAMPLAYER              (0x40)
#define HARDWARE_SEGA_MEGADRIVE_TEAMPLAYER_PORT2        (0x80)
#define HARDWARE_SEGA_MEGADRIVE_FOURWAYPLAY             (0xc0)

#define HARDWARE_SEGA_MEGADRIVE_SRAM_00400				(0x0100)
#define HARDWARE_SEGA_MEGADRIVE_SRAM_00800				(0x0200)
#define HARDWARE_SEGA_MEGADRIVE_SRAM_01000				(0x0400)
#define HARDWARE_SEGA_MEGADRIVE_SRAM_04000				(0x0800)
#define HARDWARE_SEGA_MEGADRIVE_SRAM_10000				(0x1000)
#define HARDWARE_SEGA_MEGADRIVE_FRAM_00400				(0x2000)

#define HARDWARE_PSIKYO									(HARDWARE_PREFIX_PSIKYO)

#define HARDWARE_KANEKO16								(HARDWARE_PREFIX_KANEKO | 0x10000)
#define HARDWARE_KANEKO_MISC							(HARDWARE_PREFIX_KANEKO | 0x20000)
#define HARDWARE_KANEKO_SKNS							(HARDWARE_PREFIX_KANEKO | 0x30000)

#define HARDWARE_PACMAN									(HARDWARE_PREFIX_PACMAN)

#define HARDWARE_GALAXIAN								(HARDWARE_PREFIX_GALAXIAN)

#define HARWARE_CAPCOM_MISC								(HARDWARE_PREFIX_CAPCOM_MISC)

#define HARDWARE_SETA1									(HARDWARE_PREFIX_SETA | 0x10000)
#define HARDWARE_SETA2									(HARDWARE_PREFIX_SETA | 0x20000)
#define HARDWARE_SETA_SSV								(HARDWARE_PREFIX_SETA | 0x30000)

#define HARDWARE_TECHNOS								(HARDWARE_PREFIX_TECHNOS)

#define HARDWARE_PCENGINE_PCENGINE						(HARDWARE_PREFIX_PCENGINE | 0x00010000)
#define HARDWARE_PCENGINE_TG16							(HARDWARE_PREFIX_PCENGINE | 0x00020000)
#define HARDWARE_PCENGINE_SGX							(HARDWARE_PREFIX_PCENGINE | 0x00030000)

#define HARDWARE_SPECTRUM								(HARDWARE_PREFIX_SPECTRUM)

#define HARDWARE_MIDWAY_KINST							(HARDWARE_PREFIX_MIDWAY | 0x00010000)
#define HARDWARE_MIDWAY_TUNIT							(HARDWARE_PREFIX_MIDWAY | 0x00020000)
#define HARDWARE_MIDWAY_WUNIT							(HARDWARE_PREFIX_MIDWAY | 0x00030000)
#define HARDWARE_MIDWAY_YUNIT							(HARDWARE_PREFIX_MIDWAY | 0x00040000)
#define HARDWARE_MIDWAY_XUNIT							(HARDWARE_PREFIX_MIDWAY | 0x00050000)

#define HARDWARE_NES									(HARDWARE_PREFIX_NES)
#define HARDWARE_FDS									(HARDWARE_PREFIX_FDS)
#define HARDWARE_SNES                                   (HARDWARE_PREFIX_SNES)
#define HARDWARE_SNES_ZAPPER                            (HARDWARE_PREFIX_SNES | 0x0000001)
#define HARDWARE_SNES_JUSTIFIER                         (HARDWARE_PREFIX_SNES | 0x0000002)

#define HARDWARE_CHANNELF                               (HARDWARE_PREFIX_CHANNELF)

// flags for the genre member
#define GBF_HORSHOOT									(1 << 0)
#define GBF_VERSHOOT									(1 << 1)
#define GBF_SCRFIGHT									(1 << 2)
#define GBF_VSFIGHT										(1 << 3)
#define GBF_BIOS										(1 << 4)
#define GBF_BREAKOUT									(1 << 5)
#define GBF_CASINO										(1 << 6)
#define GBF_BALLPADDLE									(1 << 7)
#define GBF_MAZE										(1 << 8)
#define GBF_MINIGAMES									(1 << 9)
#define GBF_PINBALL										(1 << 10)
#define GBF_PLATFORM									(1 << 11)
#define GBF_PUZZLE										(1 << 12)
#define GBF_QUIZ										(1 << 13)
#define GBF_SPORTSMISC									(1 << 14)
#define GBF_SPORTSFOOTBALL								(1 << 15)
#define GBF_MISC										(1 << 16)
#define GBF_MAHJONG										(1 << 17)
#define GBF_RACING										(1 << 18)
#define GBF_SHOOT										(1 << 19)
#define GBF_MULTISHOOT									(1 << 20)
#define GBF_ACTION  									(1 << 21)
#define GBF_RUNGUN  									(1 << 22)
#define GBF_STRATEGY									(1 << 23)
#define GBF_VECTOR                                      (1 << 24)
#define GBF_RPG                                         (1 << 25)
#define GBF_SIM                                         (1 << 26)
#define GBF_ADV                                         (1 << 27)
#define GBF_CARD                                        (1 << 28)
#define GBF_BOARD                                       (1 << 29)

// flags for the family member
#define FBF_MSLUG										(1 << 0)
#define FBF_SF											(1 << 1)
#define FBF_KOF											(1 << 2)
#define FBF_DSTLK										(1 << 3)
#define FBF_FATFURY										(1 << 4)
#define FBF_SAMSHO										(1 << 5)
#define FBF_19XX										(1 << 6)
#define FBF_SONICWI										(1 << 7)
#define FBF_PWRINST										(1 << 8)
#define FBF_SONIC										(1 << 9)
#define FBF_DONPACHI                                    (1 << 10)
#define FBF_MAHOU                                       (1 << 11)

#ifdef __cplusplus
 } // End of extern "C"
#endif

#endif
