#include "burner.h"

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
#define GAL_ROM_Z80_PROG1				1
#define GAL_ROM_Z80_PROG2				2
#define GAL_ROM_Z80_PROG3				3
#define GAL_ROM_TILES_SHARED			4
#define GAL_ROM_TILES_CHARS				5
#define GAL_ROM_TILES_SPRITES			6
#define GAL_ROM_PROM					7
#define GAL_ROM_S2650_PROG1				8

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
#define SYS16_ROM_PROG_FLAT									25
#define SYS16_ROM_PROG										1
#define SYS16_ROM_TILES										2
#define SYS16_ROM_SPRITES									3
#define SYS16_ROM_Z80PROG									4
#define SYS16_ROM_KEY										5
#define SYS16_ROM_7751PROG									6
#define SYS16_ROM_7751DATA									7
#define SYS16_ROM_UPD7759DATA								8
#define SYS16_ROM_PROG2										9
#define SYS16_ROM_ROAD										10
#define SYS16_ROM_PCMDATA									11
#define SYS16_ROM_Z80PROG2									12
#define SYS16_ROM_Z80PROG3									13
#define SYS16_ROM_Z80PROG4									14
#define SYS16_ROM_PCM2DATA									15
#define SYS16_ROM_PROM 										16
#define SYS16_ROM_PROG3										17
#define SYS16_ROM_SPRITES2									18
#define SYS16_ROM_RF5C68DATA								19
#define SYS16_ROM_I8751										20
#define SYS16_ROM_MSM6295									21
#define SYS16_ROM_TILES_20000								22

// taito.h
#define TAITO_68KROM1										1
#define TAITO_68KROM1_BYTESWAP								2
#define TAITO_68KROM1_BYTESWAP_JUMPING						3
#define TAITO_68KROM1_BYTESWAP32							4
#define TAITO_68KROM2										5
#define TAITO_68KROM2_BYTESWAP								6
#define TAITO_68KROM3										7
#define TAITO_68KROM3_BYTESWAP								8
#define TAITO_Z80ROM1										9
#define TAITO_Z80ROM2										10
#define TAITO_CHARS											11
#define TAITO_CHARS_BYTESWAP								12
#define TAITO_CHARSB										13
#define TAITO_CHARSB_BYTESWAP								14
#define TAITO_SPRITESA										15
#define TAITO_SPRITESA_BYTESWAP								16
#define TAITO_SPRITESA_BYTESWAP32							17
#define TAITO_SPRITESA_TOPSPEED								18
#define TAITO_SPRITESB										19
#define TAITO_SPRITESB_BYTESWAP								20
#define TAITO_SPRITESB_BYTESWAP32							21
#define TAITO_ROAD											22
#define TAITO_SPRITEMAP										23
#define TAITO_YM2610A										24
#define TAITO_YM2610B										25
#define TAITO_MSM5205										26
#define TAITO_MSM5205_BYTESWAP								27
#define TAITO_CHARS_PIVOT									28
#define TAITO_MSM6295										29
#define TAITO_ES5505										30
#define TAITO_ES5505_BYTESWAP								31
#define TAITO_DEFAULT_EEPROM								32
#define TAITO_CHARS_BYTESWAP32								33
#define TAITO_CCHIP_BIOS									34
#define TAITO_CCHIP_EEPROM									35



struct DatListInfo {
	TCHAR szRomSet[100];
	TCHAR szFullName[1024];
	TCHAR szDrvName[100];
	TCHAR szHardware[256];
	UINT32 nMarker;
};

struct LVCOMPAREINFO {
	HWND  hWnd;
	INT32 nColumn;
	BOOL  bAscending;
};

struct HardwareIcon {
	UINT32 nHardwareCode;
	INT32  nIconIndex;
};

struct PNGRESOLUTION {
	INT32 nWidth;
	INT32 nHeight;
};

struct RDMacroMap {
	const TCHAR* pszDRMacro;
	const UINT32 nRDMacro;
};

enum {
	SORT_ASCENDING = 0,
	SORT_DESCENDING = 1
};

struct BurnRomInfo* pDataRomDesc = NULL;

TCHAR szRomdataName[MAX_PATH] = _T("");
TCHAR szUtf16leFile[21]       = { 0 };
bool  bRDListScanSub          = false;

static struct BurnRomInfo* pDRD = NULL;
static struct RomDataInfo RDI = { 0 };

static LVCOMPAREINFO lv_compare;
static HIMAGELIST hHardwareIconList = NULL;

RomDataInfo*  pRDI = &RDI;

static HBRUSH hWhiteBGBrush;
static INT32  sort_direction = 0;
static INT32 nSelItem = -1;

static HWND hRDMgrWnd   = NULL;
static HWND hRDListView = NULL;
static HWND hRdCoverDlg = NULL;

static TCHAR szCover[MAX_PATH] = { 0 };


static struct HardwareIcon IconTable[] =
{
	{	HARDWARE_SEGA_MEGADRIVE,		-1	},
	{	HARDWARE_PCENGINE_PCENGINE,		-1	},
	{	HARDWARE_PCENGINE_SGX,			-1	},
	{	HARDWARE_PCENGINE_TG16,			-1	},
	{	HARDWARE_SEGA_SG1000,			-1	},
	{	HARDWARE_COLECO,				-1	},
	{	HARDWARE_SEGA_MASTER_SYSTEM,	-1	},
	{	HARDWARE_SEGA_GAME_GEAR,		-1	},
	{	HARDWARE_MSX,					-1	},
	{	HARDWARE_SPECTRUM,				-1	},
	{	HARDWARE_NES,					-1	},
	{	HARDWARE_FDS,					-1	},
	{	HARDWARE_SNES,					-1	},
	{	HARDWARE_SNK_NGPC,				-1	},
	{	HARDWARE_SNK_NGP,				-1	},
	{	HARDWARE_CHANNELF,				-1	},
	{	0,								-1	},

	{	~0U,							-1	}	// End Marker
};

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

static UINT32 RDGetRomsType(const TCHAR* pszDrvName, const UINT32 nBaseType, const INT32 nMinIdx, const INT32 nMaxIdx)
{
	char* pRomName;
	struct BurnRomInfo ri = { 0 };
	const UINT32 nOldDrvSel = nBurnDrvActive;	// Backup

	nBurnDrvActive = BurnDrvGetIndex(TCHARToANSI(pszDrvName, NULL, 0));

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & nBaseType) && ((ri.nType & 0x0f) >= nMinIdx) && ((ri.nType & 0x0f) <= nMaxIdx)) {
			nBurnDrvActive = nOldDrvSel;		// Restore
			return ri.nType;
		}
	}

	nBurnDrvActive = nOldDrvSel;				// Restore
	return nBaseType;
}

// Consoles will automatically get the Type of the ROMs
static UINT32 RDGetConsoleRomsType(const TCHAR* pszDrvName)
{
	char* pRomName = NULL;
	struct BurnRomInfo ri = { 0 };
	const UINT32 nOldDrvSel = nBurnDrvActive;	// Backup

	nBurnDrvActive = BurnDrvGetIndex(TCHARToANSI(pszDrvName, NULL, 0));

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if (ri.nType & BRF_PRG) {
			nBurnDrvActive = nOldDrvSel;		// Restore
			return ri.nType;
		}
	}

	nBurnDrvActive = nOldDrvSel;				// Restore
	return BRF_PRG;
}

// FBNeo style is preferred, universal solutions are not recommended
static UINT32 RDGetUniversalRomsType(const TCHAR* pszDrvName, const TCHAR* pszMask)
{
	UINT32 nBaseType = 0;

	if (0 == _tcsicmp(pszMask, _T("Program")) || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Z80")) || 0 == _tcsicmp(pszMask, _T("SNDA"))) {
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Samples")) || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nBaseType = BRF_SND;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("BoardPLD")) || 0 == _tcsicmp(pszMask, _T("OPT1"))) {
		nBaseType = BRF_OPT;
	}

	char* pRomName = NULL;
	struct BurnRomInfo ri = { 0 };
	const UINT32 nOldDrvSel = nBurnDrvActive;	// Backup

	nBurnDrvActive = BurnDrvGetIndex(TCHARToANSI(pszDrvName, NULL, 0));

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if (ri.nType & nBaseType) {
			nBurnDrvActive = nOldDrvSel;		// Restore
			return ri.nType;
		}
	}

	nBurnDrvActive = nOldDrvSel;				// Restore
	return nBaseType;
}

static UINT32 RDGetCps1RomsType(const TCHAR* pszDrvName, const TCHAR* pszMask, const UINT32 nPrgLen)
{
	INT32 nMinIdx = 0, nMaxIdx = 0;
	UINT32 nBaseType = 0;

	if (0 == _tcsicmp(pszMask, _T("Program")) || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nMinIdx = CPS1_68K_PROGRAM_BYTESWAP, nMaxIdx = CPS1_68K_PROGRAM_NO_BYTESWAP;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("ProgramS")) || 0 == _tcsicmp(pszMask, _T("PRGS"))) {
		return (BRF_ESS | BRF_PRG | CPS1_68K_PROGRAM_BYTESWAP);
	}
	else
	if (0 == _tcsicmp(pszMask, _T("ProgramN")) || 0 == _tcsicmp(pszMask, _T("PRGN"))) {
		return (BRF_ESS | BRF_PRG | CPS1_68K_PROGRAM_NO_BYTESWAP);
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Z80")) || 0 == _tcsicmp(pszMask, _T("SNDA"))) {
		nMinIdx = nMaxIdx = CPS1_Z80_PROGRAM;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nMinIdx = nMaxIdx = CPS1_TILES;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Samples")) || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nMinIdx = CPS1_OKIM6295_SAMPLES, nMaxIdx = CPS1_QSOUND_SAMPLES;
		nBaseType = BRF_SND;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Pic")) || 0 == _tcsicmp(pszMask, _T("SNDB"))) {
		nMinIdx = nMaxIdx = CPS1_PIC;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Tiles")) || 0 == _tcsicmp(pszMask, _T("Extras")) || 0 == _tcsicmp(pszMask, _T("PRG2"))) {
		nMinIdx = CPS1_EXTRA_TILES_SF2EBBL_400000, nMaxIdx = CPS1_EXTRA_TILES_SF2MKOT_400000;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("BoardPLD")) || 0 == _tcsicmp(pszMask, _T("OPT1"))) {
		return BRF_OPT;
	}

	char* pRomName = NULL;
	struct BurnRomInfo ri = { 0 };
	const UINT32 nOldDrvSel = nBurnDrvActive;	// Backup

	nBurnDrvActive = BurnDrvGetIndex(TCHARToANSI(pszDrvName, NULL, 0));

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & nBaseType) && ((ri.nType & 0x0f) >= nMinIdx) && ((ri.nType & 0x0f) <= nMaxIdx)) {
			if (nPrgLen > 0 && (CPS1_68K_PROGRAM_BYTESWAP == nMinIdx || CPS1_68K_PROGRAM_NO_BYTESWAP == nMaxIdx)) {
				if (nPrgLen >= 0x40000 && (CPS1_68K_PROGRAM_BYTESWAP == (ri.nType & 0x0f))) {
					ri.nType = (ri.nType & ~CPS1_68K_PROGRAM_BYTESWAP) | CPS1_68K_PROGRAM_NO_BYTESWAP;
				}
				else
				if (nPrgLen < 0x40000 && (CPS1_68K_PROGRAM_NO_BYTESWAP == (ri.nType & 0x0f))) {
					ri.nType = (ri.nType & ~CPS1_68K_PROGRAM_NO_BYTESWAP) | CPS1_68K_PROGRAM_BYTESWAP;
				}
			}
			nBurnDrvActive = nOldDrvSel;		// Restore
			return ri.nType;
		}
	}

	nBurnDrvActive = nOldDrvSel;				// Restore
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

static UINT32 RDGetCps2RomsType(const TCHAR* pszDrvName, const TCHAR* pszMask)
{
	INT32 nMinIdx = 0, nMaxIdx = 0;
	UINT32 nBaseType = 0;

	if (0 == _tcsicmp(pszMask, _T("Program")) || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nMinIdx = CPS2_PRG_68K, nMaxIdx = CPS2_PRG_68K_XOR_TABLE;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nMinIdx = CPS2_GFX, nMaxIdx = CPS2_GFX_19XXJ;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Z80")) || 0 == _tcsicmp(pszMask, _T("SNDA"))) {
		nMinIdx = nMaxIdx = CPS2_PRG_Z80;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Samples")) || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nMinIdx = CPS2_QSND, nMaxIdx = CPS2_QSND_SIMM_BYTESWAP;
		nBaseType = BRF_SND;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Decryption")) || 0 == _tcsicmp(pszMask, _T("KEY1"))) {
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
	if (0 == _tcsicmp(pszMask, _T("Bios")) || 0 == _tcsicmp(pszMask, _T("BIOS"))) {
		return (BRF_ESS | BRF_BIOS);
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Program")) || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		return (BRF_ESS | BRF_PRG);
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		return BRF_GRA;
	}
}

static INT32 RDGetPgmRomsType(const TCHAR* pszDrvName, const TCHAR* pszMask)
{
	INT32 nMinIdx = 0, nMaxIdx = 0;
	UINT32 nBaseType = 0;

	if (0 == _tcsicmp(pszMask, _T("Program")) || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nMinIdx = nMaxIdx = 1;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Tile")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nMinIdx = nMaxIdx = 2;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("SpriteData")) || 0 == _tcsicmp(pszMask, _T("GRA2"))) {
		nMinIdx = nMaxIdx = 3;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("SpriteMasks")) || 0 == _tcsicmp(pszMask, _T("GRA3"))) {
		nMinIdx = nMaxIdx = 4;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Samples")) || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nMinIdx = nMaxIdx = 5;
		nBaseType = BRF_SND;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("InternalARM7")) || 0 == _tcsicmp(pszMask, _T("PRG2"))) {
		nMinIdx = nMaxIdx = 7;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Ramdump"))) {
		//What's the point of existing when the rom length is 0 and the Nodump marker won't load?
		return (7 | BRF_PRG | BRF_ESS | BRF_NODUMP);
	}
	else
	if (0 == _tcsicmp(pszMask, _T("ExternalARM7")) || 0 == _tcsicmp(pszMask, _T("PRG3"))) {
		nMinIdx = nMaxIdx = 8;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("ProtectionRom")) || 0 == _tcsicmp(pszMask, _T("PRG4"))) {
		nMinIdx = nMaxIdx = 9;
		nBaseType = BRF_PRG;
	}

	return RDGetRomsType(pszDrvName, nBaseType, nMinIdx, nMaxIdx);
}

static UINT32 RDGetNeoGeoRomsType(const TCHAR* pszDrvName, const TCHAR* pszMask)
{
	INT32 nMinIdx = 0, nMaxIdx = 0;
	UINT32 nBaseType = 0;

	if (0 == _tcsicmp(pszMask, _T("Program")) || 0 == _tcsicmp(pszMask, _T("PRG1"))) {
		nMinIdx = nMaxIdx = 1;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Text")) || 0 == _tcsicmp(pszMask, _T("GRA1"))) {
		nMinIdx = nMaxIdx = 2;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Graphics")) || 0 == _tcsicmp(pszMask, _T("GRA2"))) {
		nMinIdx = nMaxIdx = 3;
		nBaseType = BRF_GRA;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Z80")) || 0 == _tcsicmp(pszMask, _T("SNDA"))) {
		nMinIdx = nMaxIdx = 4;
		nBaseType = BRF_PRG;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("Samples")) || 0 == _tcsicmp(pszMask, _T("SND1"))) {
		nMinIdx = nMaxIdx = 5;
		nBaseType = BRF_SND;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("SamplesAES")) || 0 == _tcsicmp(pszMask, _T("SND2"))) {
		// See nam1975RomDesc
		nMinIdx = nMaxIdx = 6;
		nBaseType = BRF_SND;
	}
	else
	if (0 == _tcsicmp(pszMask, _T("BoardPLD")) || 0 == _tcsicmp(pszMask, _T("OPT1"))) {
		return 0 | BRF_OPT;
	}

	return RDGetRomsType(pszDrvName, nBaseType, nMinIdx, nMaxIdx);
}

static INT32 GetDrvHardwareMask(const TCHAR* pszDrvName)
{
	const UINT32 nOldDrvSel = nBurnDrvActive;	// Backup
	nBurnDrvActive = BurnDrvGetIndex(TCHARToANSI(pszDrvName, NULL, 0));

	const INT32 nHardwareCode = BurnDrvGetHardwareCode();
	nBurnDrvActive = nOldDrvSel;				// Restore

	return (nHardwareCode & HARDWARE_PUBLIC_MASK);
}

static UINT32 RDSetRomsType(const TCHAR* pszDrvName, const TCHAR* pszMask, const UINT32 nPrgLen)
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
			return RDGetCps3RomsType(pszDrvName);

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

TCHAR* _strqtoken(TCHAR* s, const TCHAR* delims)
{
	static TCHAR* prev_str = NULL;
	TCHAR* token = NULL;

	if (!s) {
		if (!prev_str) return NULL;

		s = prev_str;
	}

	s += _tcsspn(s, delims);

	if (s[0] == _T('\0')) {
		prev_str = NULL;
		return NULL;
	}

	if (s[0] == _T('\"')) { // time to grab quoted string!
		token = ++s;
		if ((s = _tcspbrk(token, _T("\"")))) {
			*(s++) = '\0';
		}
		if (!s) {
			prev_str = NULL;
			return NULL;
		}
	} else {
		token = s;
	}

	if ((s = _tcspbrk(s, delims))) {
		*(s++) = _T('\0');
		prev_str = s;
	} else {
		// we're at the end of the road
#if defined (_UNICODE)
		prev_str = (TCHAR*)wmemchr(token, _T('\0'), MAX_PATH);
#else
		prev_str = (char*)memchr((void*)token, '\0', MAX_PATH);
#endif
	}

	return token;
}

static INT32 FileExists(const TCHAR* szName)
{
	return GetFileAttributes(szName) != INVALID_FILE_ATTRIBUTES;
}

static HIMAGELIST HardwareIconListInit()
{
	if (!bEnableIcons) return NULL;

	INT32 cx = 0, cy = 0, nIdx = 0;

	switch (nIconsSize) {
		case ICON_16x16: cx = cy = 16;	break;
		case ICON_24x24: cx = cy = 24;	break;
		case ICON_32x32: cx = cy = 32;	break;
	}

	hHardwareIconList = ImageList_Create(cx, cy, ILC_COLOR32 | ILC_MASK, (sizeof(IconTable) / sizeof(HardwareIcon)) - 1, 0);
	if (NULL == hHardwareIconList) return NULL;
	ListView_SetImageList(hRDListView, hHardwareIconList, LVSIL_SMALL);

	struct HardwareIcon* _it = &IconTable[0];

	while (~0U != _it->nHardwareCode) {
		_it->nIconIndex = ImageList_AddIcon(hHardwareIconList, pIconsCache[nBurnDrvCount + nIdx]);
		_it++, nIdx++;
	}

	return hHardwareIconList;
}

static void DestroyHardwareIconList()
{
	struct HardwareIcon* _it = &IconTable[0];

	while (~0U != _it->nHardwareCode) {
		_it->nIconIndex = -1; _it++;
	}

	if (NULL != hHardwareIconList) {
		ImageList_Destroy(hHardwareIconList); hHardwareIconList = NULL;
	}
}

static INT32 FindHardwareIconIndex(const TCHAR* pszDrvName)
{
	if (!bEnableIcons) return 0;

	const UINT32 nOldDrvSel    = nBurnDrvActive;	// Backup
	nBurnDrvActive             = BurnDrvGetIndex(TCHARToANSI(pszDrvName, NULL, 0));
	const UINT32 nHardwareCode = BurnDrvGetHardwareCode();

	struct HardwareIcon* _it = &IconTable[0];
	INT32 nIdx = 0;

	while (~0U != _it->nHardwareCode) {
		if (_it->nHardwareCode > 0) {				// Consoles
			if (_it->nHardwareCode == (nHardwareCode & HARDWARE_SNK_NGPC)) {
				nBurnDrvActive = nOldDrvSel;		// Restore
				return _it->nIconIndex;				// NeoGeo Pocket Color
			}
			if (_it->nHardwareCode == (nHardwareCode & HARDWARE_PUBLIC_MASK)) {
				nBurnDrvActive = nOldDrvSel;		// Restore
				return _it->nIconIndex;
			}
		}
		_it++, nIdx++;
	}

	nBurnDrvActive = nOldDrvSel;					// Restore
	return IconTable[--nIdx].nIconIndex;			// Arcade
}

typedef enum {
	ENCODING_ANSI,
	ENCODING_UTF8,
	ENCODING_UTF8_BOM,
	ENCODING_UTF16_LE,
	ENCODING_UTF16_BE,
	ENCODING_ERROR
} EncodingType;

// Verify that the byte stream conforms to UTF-8 encoding rules
static bool IsUtf8Encoding(const UINT8* data, UINT32 len)
{
	INT32 nBytes = 0;

	for (UINT32 i = 0; i < len; i++) {
		UINT8 c = data[i];
		if (0 == nBytes) {
			if (0x00 == (c & 0x80)) continue;		// ASCII
			else
			if (0xc0 == (c & 0xe0)) nBytes = 1;		// 2-byte
			else
			if (0xe0 == (c & 0xf0)) nBytes = 2;		// 3-byte
			else
			if (0xf0 == (c & 0xf8)) nBytes = 3;		// 4-byte
			else                    return false;	// Invalid UTF-8 start byte
		} else {
			if (0x80 != (c & 0xc0)) return false;	// Subsequent byte format error
			nBytes--;
		}
	}

	return (0 == nBytes);							// Checking the completeness of a multibyte sequence
}

static EncodingType DetectEncoding(const TCHAR* pszDatFile)
{
	FILE* fp = _tfopen(pszDatFile, _T("rb"));
	if (NULL == fp) return ENCODING_ERROR;

	EncodingType encType = ENCODING_UTF8;

	// Read BOM
	char cBom[3] = { 0 };
	const UINT32 nBomSize = fread(cBom, 1, 3, fp);

	if (0 == nBomSize) {	// Empty file or read error
		fclose(fp);
		return ENCODING_ERROR;
	}
	if ((nBomSize >= 3) && ('\xEF' == cBom[0]) && ('\xBB' == cBom[1]) && ('\xBF' == cBom[2])) {
		fclose(fp);
		return ENCODING_UTF8_BOM;
	}
	if (nBomSize >= 2) {
		if (('\xFF' == cBom[0]) && ('\xFE' == cBom[1])) {
			fclose(fp);
			return ENCODING_UTF16_LE;
		}
		if (('\xFE' == cBom[0]) && ('\xFF' == cBom[1])) {
			fclose(fp);
			return ENCODING_UTF16_BE;
		}
	}
	rewind(fp);

	UINT8 szBuf[4096];
	UINT32 nLen;

	while ((nLen = fread(szBuf, 1, sizeof(szBuf), fp)) > 0) {
		if (!IsUtf8Encoding(szBuf, nLen)) {
			fclose(fp); fp = NULL;
			return ENCODING_ANSI;
		}
	}

	fclose(fp); fp = NULL;
	return encType;
}

static TCHAR* Utf16beToUtf16le(const TCHAR* pszDatFile)
{
	FILE* fpInFile = _tfopen(pszDatFile, _T("rb"));
	if (NULL == fpInFile) return NULL;

	fseek(fpInFile, 2, SEEK_SET);

	// Generate date file name (UTF16LE_YYYYMMDD.dat)
	time_t now = time(NULL);
	struct tm* local = localtime(&now);
	_tcsftime(szUtf16leFile, sizeof(szUtf16leFile), _T("UTF16LE%Y%m%d.tmp"), local);


	// Create target file (UTF-16LE)
	FILE* fpOutFile = _tfopen(szUtf16leFile, _T("wb"));
	if (NULL == fpOutFile) {
		fclose(fpInFile); fpInFile = NULL;
		return NULL;
	}

	// Write to UTF-16LE BOM (0xfffe)
	UINT8 cLeBom[2] = { 0xff, 0xfe };
	if (2 != fwrite(cLeBom, 1, 2, fpOutFile)) {
		fclose(fpInFile);  fpInFile  = NULL;
		fclose(fpOutFile); fpOutFile = NULL;
		return NULL;
	}

	// Buffer
	UINT8 szBuf[4096];
	UINT32 nRead;

	while ((nRead = fread(szBuf, 1, sizeof(szBuf), fpInFile)) > 0) {
		// Ensure that even bytes are handled
		if (0 != (nRead % 2)) {
			nRead--; // Discard last byte
		}

		// Swap the order of each double byte (BE -> LE)
		for (UINT32 i = 0; i < nRead; i += 2) {
			UINT8 cTemp  = szBuf[i];
			szBuf[i + 0] = szBuf[i + 1];
			szBuf[i + 1] = cTemp;
		}

		// Write converted data
		if (nRead != fwrite(szBuf, 1, nRead, fpOutFile)){
			fclose(fpInFile);  fpInFile  = NULL;
			fclose(fpOutFile); fpOutFile = NULL;
			return NULL;
		}
	}
	fclose(fpInFile);  fpInFile  = NULL;
	fclose(fpOutFile); fpOutFile = NULL;

	MoveFileEx(szUtf16leFile, pszDatFile, MOVEFILE_REPLACE_EXISTING);

	return (TCHAR*)pszDatFile;
}

static bool StrToUint(const TCHAR* str, UINT32* result) {
	if (NULL == str || _T('\0') == *str || NULL == result) return false;

	errno = 0;

	TCHAR* endPtr;
	unsigned long value = _tcstoul(str, &endPtr, 16);

	if (str == endPtr)       return false;
	if (_T('\0') != *endPtr) return false;
	if (value > UINT_MAX) {
		errno = ERANGE;
		return false;
	}

	*result = (UINT32)value;
	return true;
}

#define DELIM_TOKENS_NAME	_T(" \t\r\n,%:|{}")

static INT32 LoadRomdata()
{
	EncodingType nType = DetectEncoding(szRomdataName);
	if (ENCODING_ERROR == nType) return -1;

	const TCHAR* pszReadMode = NULL;

	switch (nType) {
		case ENCODING_ANSI: {
			pszReadMode = _T("rt");
			break;
		}
		case ENCODING_UTF8:
		case ENCODING_UTF8_BOM: {
			pszReadMode = _T("rt, ccs=UTF-8");
			break;
		}

		case ENCODING_UTF16_LE: {
			pszReadMode = _T("rt, ccs=UTF-16LE");
			break;
		}

		case ENCODING_UTF16_BE: {
			const TCHAR* pszConvert = Utf16beToUtf16le(szRomdataName);
			if (NULL == pszConvert) return -1;
			pszReadMode = _T("rt, ccs=UTF-16LE");
			break;
		}
	}

	RDI.nDescCount = -1;					// Failed

	FILE* fp = _tfopen(szRomdataName, pszReadMode);
	if (NULL == fp) return -1;

	TCHAR szBuf[MAX_PATH] = { 0 }, szRomMask[30] = { 0 }, szDrvName[100] = { 0 };
	TCHAR* pszBuf = NULL, * pszLabel = NULL, * pszInfo = NULL;
	INT32 nDatMode = 0;						// FBNeo 0, Nebula 1

	memset(RDI.szExtraRom, 0, sizeof(RDI.szExtraRom));
	memset(RDI.szFullName, 0, sizeof(RDI.szFullName));

	while (!feof(fp)) {
		if (_fgetts(szBuf, MAX_PATH, fp) != NULL) {
			pszBuf = szBuf;

			pszLabel = _strqtoken(pszBuf, DELIM_TOKENS_NAME);
			if (NULL == pszLabel) continue;
			if ((_T('/') == pszLabel[0]) && (_T('/') == pszLabel[1])) continue;

			UINT32 nLen = _tcslen(pszLabel);
			if ((nLen > 2) && (_T('[') == pszLabel[0]) && (_T(']') == pszLabel[nLen - 1])) {
				pszLabel++, nLen -= 2;
				if (0 == _tcsicmp(_T("System"), pszLabel)) break;
				memset(szRomMask, 0, sizeof(szRomMask));
				_tcsncpy(szRomMask, pszLabel, nLen);
				szRomMask[nLen] = _T('\0');
				continue;
			}
			if (0 == _tcsicmp(_T("System"), pszLabel)) {
				nDatMode = 1;				// Nebula
				continue;
			}
			if (0 == _tcsicmp(_T("ZipName"), pszLabel) || 0 == _tcsicmp(_T("RomName"), pszLabel)) {
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL == pszInfo) break;	// No romset specified
				if (NULL != pDRD) {
					strcpy(RDI.szZipName, TCHARToANSI(pszInfo, NULL, 0));
				}
				continue;
			}
			if (0 == _tcsicmp(_T("DrvName"), pszLabel) || 0 == _tcsicmp(_T("Parent"), pszLabel)) {
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL == pszInfo) break;	// No driver specified
				if (NULL != pDRD) {
					strcpy(RDI.szDrvName, TCHARToANSI(pszInfo, NULL, 0));
				}
				memset(szDrvName, 0, sizeof(szDrvName));
				_tcscpy(szDrvName, pszInfo);
				continue;
			}
			if (0 == _tcsicmp(_T("ExtraRom"), pszLabel)) {
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if ((NULL != pszInfo) && (NULL != pDRD)) {
					strcpy(RDI.szExtraRom, TCHARToANSI(pszInfo, NULL, 0));
				}
				continue;
			}
			if (0 == _tcsicmp(_T("FullName"), pszLabel) || 0 == _tcsicmp(_T("Game"), pszLabel)) {
				TCHAR szMerger[260] = { 0 };
				INT32 nAdd = 0;
				while (NULL != (pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME))) {
					_stprintf(szMerger + nAdd, _T("%s "), pszInfo);
					nAdd = _tcslen(szMerger);
				}
				szMerger[nAdd - 1] = _T('\0');
				if (NULL != pDRD) {
#ifdef UNICODE
					_tcscpy(RDI.szFullName, szMerger);
#else
					wcscpy(RDI.szFullName, ANSIToTCHAR(szMerger, NULL, 0));
#endif
				}
				continue;
			}

			{
				// romname, len, crc, type
				struct BurnRomInfo ri = { 0 };
				ri.nLen  = UINT_MAX;
				ri.nCrc  = UINT_MAX;
				ri.nType = 0U;

				if (1 == nDatMode) {
					// Skips content when recognized as a Nebula style
					pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				}
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL != pszInfo) {
					StrToUint(pszInfo, &(ri.nLen));
					if ((UINT_MAX == ri.nLen) || (0 == ri.nLen)) continue;

					pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
					if (NULL != pszInfo) {
						StrToUint(pszInfo, &(ri.nCrc));
						if (UINT_MAX == ri.nCrc) continue;

						while (NULL != (pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME))) {
							UINT32 nValue = UINT_MAX;

							for (UINT32 i = 0; i < (sizeof(RDMacroTable) / sizeof(RDMacroMap)); i++) {
								if (0 == _tcscmp(pszInfo, RDMacroTable[i].pszDRMacro)) {
									ri.nType |= RDMacroTable[i].nRDMacro;
									continue;
								}
							}

							StrToUint(pszInfo, &nValue);
							if ((nValue >= 0) && (nValue < UINT_MAX)) {
								ri.nType |= nValue;
							}
						}
						// FBNeo style ROMs have a higher priority for nType than Nebula style
						if (0 == ri.nType) {
							ri.nType = RDSetRomsType(szDrvName, szRomMask, ri.nLen);
						}
						if (ri.nType > 0U) {
							RDI.nDescCount++;

							if (NULL != pDRD) {
								pDRD[RDI.nDescCount].szName = (char*)malloc(512);
								memset(pDRD[RDI.nDescCount].szName, 0, sizeof(char) * 512);
								strcpy(pDRD[RDI.nDescCount].szName, TCHARToANSI(pszLabel, NULL, 0));

								pDRD[RDI.nDescCount].nLen  = ri.nLen;
								pDRD[RDI.nDescCount].nCrc  = ri.nCrc;
								pDRD[RDI.nDescCount].nType = ri.nType;
							}
						}
					}
				}
			}
		}
	}
	fclose(fp);

	if (NULL != pDRD) {
		pDataRomDesc = (struct BurnRomInfo*)malloc((RDI.nDescCount + 1) * sizeof(BurnRomInfo));
		if (NULL == pDataRomDesc) return -1;

		for (INT32 i = 0; i <= RDI.nDescCount; i++) {
			pDataRomDesc[i].szName = (char*)malloc(512);
			memset(pDataRomDesc[i].szName, 0, sizeof(char) * 512);
			strcpy(pDataRomDesc[i].szName, pDRD[i].szName);
			free(pDRD[i].szName); pDRD[i].szName = NULL;

			pDataRomDesc[i].nLen  = pDRD[i].nLen;
			pDataRomDesc[i].nCrc  = pDRD[i].nCrc;
			pDataRomDesc[i].nType = pDRD[i].nType;
		}
		free(pDRD); pDRD = NULL;
	}

	return RDI.nDescCount;
}

char* RomdataGetDrvName()
{
	EncodingType nType = DetectEncoding(szRomdataName);
	if (ENCODING_ERROR == nType) return NULL;

	const TCHAR* pszReadMode = NULL;

	switch (nType) {
		case ENCODING_ANSI: {
			pszReadMode = _T("rt");
			break;
		}
		case ENCODING_UTF8:
		case ENCODING_UTF8_BOM: {
			pszReadMode = _T("rt, ccs=UTF-8");
			break;
		}

		case ENCODING_UTF16_LE: {
			pszReadMode = _T("rt, ccs=UTF-16LE");
			break;
		}

		case ENCODING_UTF16_BE: {
			const TCHAR* pszConvert = Utf16beToUtf16le(szRomdataName);
			if (NULL == pszConvert) return NULL;
			pszReadMode = _T("rt, ccs=UTF-16LE");
			break;
		}
	}

	FILE* fp = _tfopen(szRomdataName, pszReadMode);
	if (NULL == fp) return NULL;

	TCHAR szBuf[MAX_PATH] = { 0 };
	TCHAR* pszBuf = NULL, * pszLabel = NULL, * pszInfo = NULL;

	while (!feof(fp)) {
		if (_fgetts(szBuf, MAX_PATH, fp) != NULL) {
			pszBuf = szBuf;

			pszLabel = _strqtoken(pszBuf, DELIM_TOKENS_NAME);
			if (NULL == pszLabel) continue;
			if ((_T('/') == pszLabel[0]) && (_T('/') == pszLabel[1])) continue;

			if (0 == _tcsicmp(_T("DrvName"), pszLabel) || 0 == _tcsicmp(_T("Parent"), pszLabel)) {
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL == pszInfo) break;	// No driver specified
				fclose(fp);
				return TCHARToANSI(pszInfo, NULL, 0);
			}
		}
	}
	fclose(fp);

	return NULL;
}

INT32 RomDataCheck(const TCHAR* pszDatFile)
{
	if (NULL == pszDatFile) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_NODATA));
		FBAPopupDisplay(PUF_TYPE_ERROR);
		return -1;
	}
	if (!FileExists(pszDatFile)) {
		FBAPopupAddText(PUF_TEXT_DEFAULT, _T("%s: %s\n\n"), FBALoadStringEx(hAppInst, IDS_ROMDATA_DATPATH, true), pszDatFile);
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_NODATA));
		FBAPopupDisplay(PUF_TYPE_ERROR);
		return -2;
	}

	TCHAR szBackup[MAX_PATH] = { 0 };
	_tcscpy(szBackup, szRomdataName);			// Backup szRomdataName
	_tcscpy(szRomdataName, pszDatFile);

	const UINT32 nOldDrvSel = nBurnDrvActive;	// Backup nBurnDrvActive

	char* szDrvName = RomdataGetDrvName();
	nBurnDrvActive  = BurnDrvGetIndex(szDrvName);

	INT32 nRet = 0;
	if (NULL == szDrvName)    nRet = -3;
	if (-1 == nBurnDrvActive) nRet = -4;

	if (nRet < 0) {
		memset(szRomdataName, 0, sizeof(szRomdataName));
		_tcscpy(szRomdataName, szBackup);
		nBurnDrvActive = nOldDrvSel;
		FBAPopupAddText(PUF_TEXT_DEFAULT, _T("%s: %s\n\n"), FBALoadStringEx(hAppInst, IDS_ROMDATA_DATPATH, true), pszDatFile);
		UINT32 nStrId = (-3 == nRet) ? IDS_ERR_NO_DRIVER_SELECTED : IDS_ERR_DRIVER_NOT_EXIST;
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(nStrId));
		FBAPopupDisplay(PUF_TYPE_ERROR);
		return nRet;
	}

	RomDataInit();								// Replace DrvName##RomDesc
	nRet = BzipOpen(1);

	if (1 == nRet) {
		BzipClose();
		BzipOpen(0);
		FBAPopupDisplay(PUF_TYPE_ERROR);
	}

	BzipClose();
	RomDataExit();								// Restore DrvName##RomDesc
	nBurnDrvActive = nOldDrvSel;				// Restore nBurnDrvActive
	_tcscpy(szRomdataName, szBackup);

	return nRet;
}

static DatListInfo* RomdataGetListInfo(const TCHAR* pszDatFile)
{
	EncodingType nType = DetectEncoding(pszDatFile);
	if (ENCODING_ERROR == nType) return NULL;

	memset(szRomdataName, 0, sizeof(szRomdataName));
	_tcscpy(szRomdataName, pszDatFile);

	const TCHAR* pszReadMode = NULL;

	switch (nType) {
		case ENCODING_ANSI: {
			pszReadMode = _T("rt");
			break;
		}
		case ENCODING_UTF8:
		case ENCODING_UTF8_BOM: {
			pszReadMode = _T("rt, ccs=UTF-8");
			break;
		}
		case ENCODING_UTF16_LE: {
			pszReadMode = _T("rt, ccs=UTF-16LE");
			break;
		}
		case ENCODING_UTF16_BE: {
			const TCHAR* pszConvert = Utf16beToUtf16le(szRomdataName);
			if (NULL == pszConvert) return NULL;
			pszReadMode = _T("rt, ccs=UTF-16LE");
			break;
		}
	}

	FILE* fp = _tfopen(szRomdataName, pszReadMode);
	if (NULL == fp) return NULL;

	DatListInfo* pDatListInfo = (DatListInfo*)malloc(sizeof(DatListInfo));
	if (NULL == pDatListInfo) {
		fclose(fp); return NULL;
	}
	memset(pDatListInfo, 0, sizeof(DatListInfo));


	TCHAR szBuf[MAX_PATH] = { 0 };
	TCHAR* pszBuf = NULL, * pszLabel = NULL, * pszInfo = NULL;

	while (!feof(fp)) {
		if (NULL != _fgetts(szBuf, MAX_PATH, fp)) {
			pszBuf = szBuf;

			pszLabel = _strqtoken(pszBuf, DELIM_TOKENS_NAME);
			if (NULL == pszLabel) continue;
			if ((_T('/') == pszLabel[0]) && (_T('/') == pszLabel[1])) continue;

			if (0 == _tcsicmp(_T("ZipName"), pszLabel) || 0 == _tcsicmp(_T("RomName"), pszLabel)) {	// Romset
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL == pszInfo) {						// No romset specified
					FBAPopupAddText(PUF_TEXT_DEFAULT, _T("%s: %s\n\n"), FBALoadStringEx(hAppInst, IDS_ROMDATA_DATPATH, true), pszDatFile);
					FBAPopupAddText(PUF_TEXT_DEFAULT, _T("%s:?\n\n"),   FBALoadStringEx(hAppInst, IDS_ROMDATA_ROMSET,  true));
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_LOAD_NOTFOUND), _T("ROM"));
					FBAPopupDisplay(PUF_TYPE_ERROR);

					free(pDatListInfo); fclose(fp);
					return NULL;
				}
				_tcscpy(pDatListInfo->szRomSet, pszInfo);
				pDatListInfo->nMarker |= (1 << 0);
			}
			if (0 == _tcsicmp(_T("DrvName"), pszLabel) || 0 == _tcsicmp(_T("Parent"), pszLabel)) {	// Driver
				pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME);
				if (NULL == pszInfo) {						// No driver specified
					FBAPopupAddText(PUF_TEXT_DEFAULT, _T("%s: %s\n\n"), FBALoadStringEx(hAppInst, IDS_ROMDATA_DATPATH, true), pszDatFile);
					FBAPopupAddText(PUF_TEXT_DEFAULT, _T("%s:?\n\n"),   FBALoadStringEx(hAppInst, IDS_ROMDATA_DRIVER,  true));
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_NO_DRIVER_SELECTED));
					FBAPopupDisplay(PUF_TYPE_ERROR);

					free(pDatListInfo); fclose(fp);
					return NULL;
				}
				_tcscpy(pDatListInfo->szDrvName, pszInfo);
				pDatListInfo->nMarker |= (1 << 1);

				{											// Hardware
					UINT32 nOldDrvSel = nBurnDrvActive;		// Backup
					nBurnDrvActive = BurnDrvGetIndex(TCHARToANSI(pDatListInfo->szDrvName, NULL, 0));
					if (-1 == nBurnDrvActive) {
						FBAPopupAddText(PUF_TEXT_DEFAULT, _T("%s: %s\n\n"), FBALoadStringEx(hAppInst, IDS_ROMDATA_DATPATH, true), pszDatFile);
						FBAPopupAddText(PUF_TEXT_DEFAULT, _T("%s: %s\n\n"), FBALoadStringEx(hAppInst, IDS_ROMDATA_DRIVER,  true), pDatListInfo->szDrvName);
						FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DRIVER_NOT_EXIST));
						FBAPopupDisplay(PUF_TYPE_ERROR);

						nBurnDrvActive = nOldDrvSel;		// Restore
						free(pDatListInfo); fclose(fp);
						return NULL;
					}

					const INT32 nGetTextFlags = nLoadMenuShowY & (1 << 31) ? DRV_ASCIIONLY : 0;
					TCHAR szCartridge[100] = { 0 };

					_stprintf(szCartridge, FBALoadStringEx(hAppInst, IDS_MVS_CARTRIDGE, true));
					_stprintf(
						pDatListInfo->szHardware,
						FBALoadStringEx(hAppInst, IDS_ROMDATA_HARDWARE_DESC, true),
						(HARDWARE_SNK_MVS == (BurnDrvGetHardwareCode() & HARDWARE_SNK_MVS) && HARDWARE_SNK_NEOGEO == (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)) ? szCartridge : BurnDrvGetText(nGetTextFlags | DRV_SYSTEM)
					);

					pDatListInfo->nMarker |= (1 << 2);
					nBurnDrvActive = nOldDrvSel;			// Restore
				}
			}
			if (0 == _tcsicmp(_T("FullName"), pszLabel) || 0 == _tcsicmp(_T("Game"), pszLabel)) {
				TCHAR szMerger[260] = { 0 };
				INT32 nAdd = 0;
				while (NULL != (pszInfo = _strqtoken(NULL, DELIM_TOKENS_NAME))) {
					_stprintf(szMerger + nAdd, _T("%s "), pszInfo);
					nAdd = _tcslen(szMerger);
				}
				szMerger[nAdd - 1] = _T('\0');
				_tcscpy(pDatListInfo->szFullName, szMerger);
				pDatListInfo->nMarker |= (1 << 3);
			}
		}
	}
	fclose(fp);

	if (!(pDatListInfo->nMarker & (UINT32)0x03)) {			// No romset specified or No driver specified
		free(pDatListInfo); pDatListInfo = NULL;
		return NULL;
	}
	if (!(pDatListInfo->nMarker & (UINT32)0x08)) {			// No FullName specified
		_tcscpy(pDatListInfo->szFullName, pDatListInfo->szRomSet);
	}

	return pDatListInfo;
}

#undef DELIM_TOKENS_NAME

void RomDataInit()
{
	INT32 nLen = LoadRomdata();

	if ((nLen >= 0) && (NULL == pDRD)) {
		pDRD = (struct BurnRomInfo*)malloc((nLen + 1) * sizeof(BurnRomInfo));
		if (NULL == pDRD) return;

		LoadRomdata();
		RDI.nDriverId = BurnDrvGetIndex(RDI.szDrvName);

		if (RDI.nDriverId >= 0) {
			BurnDrvSetZipName(RDI.szZipName, RDI.nDriverId);
		}
	}
}

void RomDataSetFullName()
{
	// At this point, the driver's default ZipName has been replaced with the ZipName from the rom data
	RDI.nDriverId = BurnDrvGetIndex(RDI.szZipName);

	if (RDI.nDriverId >= 0) {
		wchar_t* szOldName = BurnDrvGetFullNameW(RDI.nDriverId);
		memset(RDI.szOldName, '\0', sizeof(RDI.szOldName));

		if (0 != wcscmp(szOldName, RDI.szOldName)) {
			wcscpy(RDI.szOldName, szOldName);
		}

		BurnDrvSetFullNameW(RDI.szFullName, RDI.nDriverId);
	}
}

void RomDataExit()
{
	if (NULL != pDataRomDesc) {
		for (INT32 i = 0; i < RDI.nDescCount + 1; i++) {
			free(pDataRomDesc[i].szName);
		}
		free(pDataRomDesc); pDataRomDesc = NULL;

		if (RDI.nDriverId >= 0) {
			BurnDrvSetZipName(RDI.szDrvName, RDI.nDriverId);
			if (0 != wcscmp(BurnDrvGetFullNameW(RDI.nDriverId), RDI.szOldName)) {
				BurnDrvSetFullNameW(RDI.szOldName, RDI.nDriverId);
			}
		}

		memset(&RDI,          0, sizeof(RomDataInfo));
		memset(szRomdataName, 0, sizeof(szRomdataName));

		RDI.nDescCount = -1;
	}
}

// Add DatListInfo to List
static INT32 RomdataAddListItem(TCHAR* pszDatFile)
{
	DatListInfo* pDatListInfo = RomdataGetListInfo(pszDatFile);
	if (NULL == pDatListInfo) return -1;

	LVITEM lvi;
	memset(&lvi, 0, sizeof(LVITEM));

	// Full name
	lvi.iImage     = FindHardwareIconIndex(pDatListInfo->szDrvName);
	lvi.iItem      = ListView_GetItemCount(hRDListView);
	lvi.mask       = LVIF_TEXT | LVIF_IMAGE;
	lvi.cchTextMax = 1024;
	lvi.pszText    = pDatListInfo->szFullName;

	INT32 nItem = ListView_InsertItem(hRDListView, &lvi);

	ListView_SetItem(hRDListView, &lvi);

	// Romset
	memset(&lvi, 0, sizeof(LVITEM));
	lvi.iSubItem   = 1;
	lvi.iItem      = nItem;
	lvi.cchTextMax = 100;
	lvi.mask       = LVIF_TEXT | LVIF_IMAGE;
	lvi.pszText    = pDatListInfo->szRomSet;
	ListView_SetItem(hRDListView, &lvi);

	// Driver
	memset(&lvi, 0, sizeof(LVITEM));
	lvi.iSubItem   = 2;
	lvi.iItem      = nItem;
	lvi.cchTextMax = 100;
	lvi.mask       = LVIF_TEXT | LVIF_IMAGE;
	lvi.pszText    = pDatListInfo->szDrvName;
	ListView_SetItem(hRDListView, &lvi);


	// Dat path
	memset(&lvi, 0, sizeof(LVITEM));
	lvi.iSubItem   = 3;
	lvi.iItem      = nItem;
	lvi.cchTextMax = MAX_PATH;
	lvi.mask       = LVIF_TEXT | LVIF_IMAGE;
	lvi.pszText    = pszDatFile;
	ListView_SetItem(hRDListView, &lvi);

	free(pDatListInfo); pDatListInfo = NULL;

	return nItem;
}

static INT32 ends_with_slash(const TCHAR* dirPath)
{
	UINT32 len = _tcslen(dirPath);
	if (0 == len) return 0;

	TCHAR last_char = dirPath[len - 1];
	return (last_char == _T('/') || last_char == _T('\\'));
}

#define IS_STRING_EMPTY(s) (NULL == (s) || (s)[0] == _T('\0'))

static void RomdataListFindDats(const TCHAR* dirPath)
{
	if (IS_STRING_EMPTY(dirPath)) return;

	TCHAR searchPath[MAX_PATH] = { 0 };

	const TCHAR* szFormatA = ends_with_slash(dirPath) ? _T("%s*") : _T("%s\\*.*");
	const TCHAR* szFormatB = ends_with_slash(dirPath) ? _T("%s%s") : _T("%s\\%s");

	_stprintf(searchPath, szFormatA, dirPath);

	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile(searchPath, &findFileData);
	if (INVALID_HANDLE_VALUE == hFind) return;

	do {
		if (0 == _tcscmp(findFileData.cFileName, _T(".")) || 0 == _tcscmp(findFileData.cFileName, _T("..")))
			continue;
		// like: c:\1st_dir + '\' + 2nd_dir + '\' + "1.dat" + '\0' = 8 chars
		if ((_tcslen(dirPath) + _tcslen(findFileData.cFileName)) > (MAX_PATH - 8))
			continue;

		TCHAR szFullPath[MAX_PATH] = { 0 };
		_stprintf(szFullPath, szFormatB, dirPath, findFileData.cFileName);

		if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (bRDListScanSub)
				RomdataListFindDats(szFullPath);
		} else {
			if (IsFileExt(findFileData.cFileName, _T(".dat")))
				RomdataAddListItem(szFullPath);
		}
	} while (FindNextFile(hFind, &findFileData));

	FindClose(hFind);
}

#undef IS_STRING_EMPTY

static INT32 CALLBACK ListViewCompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	TCHAR buf1[MAX_PATH];
	TCHAR buf2[MAX_PATH];
	LVCOMPAREINFO* lpsd = (struct LVCOMPAREINFO*)lParamSort;

	ListView_GetItemText(lpsd->hWnd, (INT32)lParam1, lpsd->nColumn, buf1, sizeof(buf1));
	ListView_GetItemText(lpsd->hWnd, (INT32)lParam2, lpsd->nColumn, buf2, sizeof(buf2));

	switch (lpsd->bAscending) {
		case SORT_ASCENDING:
			return (_wcsicmp(buf1, buf2));
		case SORT_DESCENDING:
			return (0 - _wcsicmp(buf1, buf2));
	}

	return 0;
}

static void ListViewSort(INT32 nDirection, INT32 nColumn)
{
	// sort the list
	lv_compare.hWnd       = hRDMgrWnd;
	lv_compare.nColumn    = nColumn;
	lv_compare.bAscending = nDirection;
	ListView_SortItemsEx(hRDListView, ListViewCompareFunc, &lv_compare);
}

static void RomDataInitListView()
{
	LVCOLUMN LvCol;
	ListView_SetExtendedListViewStyle(hRDListView, LVS_EX_FULLROWSELECT);

	memset(&LvCol, 0, sizeof(LvCol));
	LvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

	LvCol.cx       = 317;
	LvCol.pszText  = FBALoadStringEx(hAppInst, IDS_ROMDATA_FULLNAME, true);
	SendMessage(hRDListView, LVM_INSERTCOLUMN, 0, (LPARAM)&LvCol);

	LvCol.cx       = 100;
	LvCol.pszText  = FBALoadStringEx(hAppInst, IDS_ROMDATA_ROMSET, true);
	SendMessage(hRDListView, LVM_INSERTCOLUMN, 1, (LPARAM)&LvCol);

	LvCol.cx       = 100;
	LvCol.pszText  = FBALoadStringEx(hAppInst, IDS_ROMDATA_DRIVER, true);
	SendMessage(hRDListView, LVM_INSERTCOLUMN, 2, (LPARAM)&LvCol);

	LvCol.cx       = 0;
	LvCol.pszText  = FBALoadStringEx(hAppInst, IDS_ROMDATA_DATPATH, true);
	SendMessage(hRDListView, LVM_INSERTCOLUMN, 3, (LPARAM)&LvCol);

	sort_direction = SORT_ASCENDING; // dink
}

static PNGRESOLUTION GetPNGResolution(TCHAR* szFile)
{
	PNGRESOLUTION nResolution = { 0, 0 };
	IMAGE img = { 0, 0, 0, 0, NULL, NULL, 0 };

	FILE* fp = _tfopen(szFile, _T("rb"));

	if (!fp) {
		return nResolution;
	}

	PNGGetInfo(&img, fp);

	nResolution.nWidth  = img.width;
	nResolution.nHeight = img.height;

	fclose(fp);

	return nResolution;
}

static void RomDataShowPreview(HWND hDlg, TCHAR* szFile, INT32 nCtrID, INT32 nFrameCtrID, float maxw, float maxh)
{
	PNGRESOLUTION PNGRes = { 0, 0 };
	if (!_tfopen(szFile, _T("rb"))) {
		HRSRC hrsrc     = FindResource(NULL, MAKEINTRESOURCE(BMP_SPLASH), RT_BITMAP);
		HGLOBAL hglobal = LoadResource(NULL, (HRSRC)hrsrc);

		BITMAPINFOHEADER* pbmih = (BITMAPINFOHEADER*)LockResource(hglobal);

		PNGRes.nWidth  = pbmih->biWidth;
		PNGRes.nHeight = pbmih->biHeight;

		FreeResource(hglobal);
	} else {
		PNGRes = GetPNGResolution(szFile);
	}

	// ------------------------------------------------------
	// PROPER ASPECT RATIO CALCULATIONS

	float w = (float)PNGRes.nWidth;
	float h = (float)PNGRes.nHeight;

	if (maxw == 0 && maxh == 0) {
		// derive max w/h from dialog size
		RECT rect = { 0, 0, 0, 0 };
		GetWindowRect(GetDlgItem(hDlg, IDC_STATIC2), &rect);
		INT32 dw = rect.right  - rect.left;
		INT32 dh = rect.bottom - rect.top;

		dw = dw * 90 / 100; // make W 90% of the "Preview / Title" windowpane
		dh = dw * 75 / 100; // make H 75% of w (4:3)

		maxw = dw;
		maxh = dh;
	}

	// max WIDTH
	if (w > maxw) {
		float nh = maxw * (float)(h / w);
		float nw = maxw;

		// max HEIGHT
		if (nh > maxh) {
			nw = maxh * (float)(nw / nh);
			nh = maxh;
		}

		w = nw;
		h = nh;
	}

	// max HEIGHT
	if (h > maxh) {
		float nw = maxh * (float)(w / h);
		float nh = maxh;

		// max WIDTH
		if (nw > maxw) {
			nh = maxw * (float)(nh / nw);
			nw = maxw;
		}

		w = nw;
		h = nh;
	}

	// Proper centering of preview
	float x = ((maxw - w) / 2);
	float y = ((maxh - h) / 2);

	RECT rc = { 0, 0, 0, 0 };
	GetWindowRect(GetDlgItem(hDlg, nFrameCtrID), &rc);

	POINT pt;
	pt.x = rc.left;
	pt.y = rc.top;

	ScreenToClient(hDlg, &pt);

	// ------------------------------------------------------

	FILE* fp = _tfopen(szFile, _T("rb"));

	HBITMAP hCoverBmp = PNGLoadBitmap(hDlg, fp, (int)w, (int)h, 0);

	SetWindowPos(GetDlgItem(hDlg, nCtrID), NULL, (int)(pt.x + x), (int)(pt.y + y), 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
	SendDlgItemMessage(hDlg, nCtrID, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hCoverBmp);

	if (fp) fclose(fp);

}

static void RomDataClearList()
{
	ListView_DeleteAllItems(hRDListView);

	RomDataShowPreview(hRDMgrWnd, _T(""), IDC_ROMDATA_TITLE,   IDC_ROMDATA_TITLE_FRAME,    0, 0);
	RomDataShowPreview(hRDMgrWnd, _T(""), IDC_ROMDATA_PREVIEW, IDC_ROMDATA_PREVIEW_FRAME,  0, 0);

	SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDATPATH),     _T(""));
	SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDRIVER),   _T(""));
	SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTHARDWARE), _T(""));

	nSelItem = -1;
}

static INT_PTR CALLBACK RomDataCoveProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM /*lParam*/)
{
	if (Msg == WM_INITDIALOG) {
		hRdCoverDlg = hDlg;
		RomDataShowPreview(hDlg, szCover, IDC_ROMDATA_COVER_PREVIEW_PIC, IDC_ROMDATA_COVER_PREVIEW_PIC, 580, 415);

		return TRUE;
	}

	if (Msg == WM_CLOSE) {
		EndDialog(hDlg, 0); hRdCoverDlg = NULL;
	}

	if (Msg == WM_COMMAND) {
		if (LOWORD(wParam) == WM_DESTROY) SendMessage(hDlg, WM_CLOSE, 0, 0);
	}

	return 0;
}

static void RomdataCoverInit()
{
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_ROMDATA_COVER_DLG), hRDMgrWnd, (DLGPROC)RomDataCoveProc);
}

static void RomDataManagerExit()
{
	RomDataClearList();
	DestroyHardwareIconList();
	DeleteObject(hWhiteBGBrush);

	hRDMgrWnd = hRDListView = NULL;
}

static INT_PTR CALLBACK RomDataManagerProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_INITDIALOG) {
		hRDMgrWnd = hDlg;

		INITCOMMONCONTROLSEX icc;
		icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
		icc.dwICC  = ICC_LISTVIEW_CLASSES;
		InitCommonControlsEx(&icc);

		hRDListView   = GetDlgItem(hDlg, IDC_ROMDATA_LIST);
		ListView_SetExtendedListViewStyle(hRDListView, LVS_EX_FULLROWSELECT);

		RomDataInitListView();
		HardwareIconListInit();

		HICON hIcon   = LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_APP));
		SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);			// Set the Game Selection dialog icon.

		hWhiteBGBrush = CreateSolidBrush(RGB(0xff, 0xff, 0xff));

		RomDataShowPreview(hRDMgrWnd, _T(""), IDC_ROMDATA_TITLE,   IDC_ROMDATA_TITLE_FRAME,    0, 0);
		RomDataShowPreview(hRDMgrWnd, _T(""), IDC_ROMDATA_PREVIEW, IDC_ROMDATA_PREVIEW_FRAME,  0, 0);

		SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDATPATH),  _T(""));
		SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDRIVER),   _T(""));
		SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTHARDWARE), _T(""));

		CheckDlgButton(hRDMgrWnd, IDC_ROMDATA_SUBDIR_CHECK, bRDListScanSub ? BST_CHECKED : BST_UNCHECKED);

		TreeView_SetItemHeight(hRDListView, 40);

		TCHAR szDialogTitle[200] = { 0 };
		_stprintf(szDialogTitle, FBALoadStringEx(hAppInst, IDS_ROMDATAMANAGER_TITLE, true), _T(APP_TITLE), _T(SEPERATOR_1), _T(SEPERATOR_1));
		SetWindowText(hDlg, szDialogTitle);

		RomdataListFindDats(szAppRomdataPath);
		WndInMid(hDlg, hScrnWnd);
		SetFocus(hRDListView);

		return TRUE;
	}

	if (Msg == WM_CLOSE) {
		RomDataManagerExit();
		EndDialog(hDlg, 0);
	}

	if (Msg == WM_CTLCOLORSTATIC) {
		if ((HWND)lParam == GetDlgItem(hRDMgrWnd, IDC_ROMDATA_LABELDAT))      return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hRDMgrWnd, IDC_ROMDATA_LABELDRIVER))   return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hRDMgrWnd, IDC_ROMDATA_LABELHARDWARE)) return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDATPATH))   return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDRIVER))    return (INT_PTR)hWhiteBGBrush;
		if ((HWND)lParam == GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTHARDWARE))  return (INT_PTR)hWhiteBGBrush;
	}

	if (Msg == WM_NOTIFY) {
		NMLISTVIEW* pNMLV = (NMLISTVIEW*)lParam;

		// Dat selected
		if (pNMLV->hdr.code == LVN_ITEMCHANGED && pNMLV->hdr.idFrom == IDC_ROMDATA_LIST) {
			INT32 nCount    = SendMessage(hRDListView, LVM_GETITEMCOUNT,     0, 0);
			INT32 nSelCount = SendMessage(hRDListView, LVM_GETSELECTEDCOUNT, 0, 0);

			if (0 == nCount || 0 == nSelCount) return 1;

			TCHAR szSelDat[MAX_PATH] = { 0 };

			nSelItem = SendMessage(hRDListView, LVM_GETNEXTITEM, -1, LVNI_SELECTED);

			if (-1 != nSelItem) {
				LVITEM LvItem     = { 0 };
				LvItem.iSubItem   = 3;	// Dat Path column
				LvItem.pszText    = szSelDat;
				LvItem.cchTextMax = sizeof(szSelDat);

				SendMessage(hRDListView, LVM_GETITEMTEXT, (WPARAM)nSelItem, (LPARAM)&LvItem);

				SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDATPATH),  _T(""));
				SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDRIVER),   _T(""));
				SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTHARDWARE), _T(""));

				DatListInfo* pDatListInfo = RomdataGetListInfo(szSelDat);
				if (NULL != pDatListInfo) {
					SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDATPATH),  szSelDat);
					SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTDRIVER),   pDatListInfo->szDrvName);
					SetWindowText(GetDlgItem(hRDMgrWnd, IDC_ROMDATA_TEXTHARDWARE), pDatListInfo->szHardware);

					_stprintf(szCover, _T("%s%s.png"), szAppTitlesPath, pDatListInfo->szRomSet);
					if (!FileExists(szCover))
						_stprintf(szCover, _T("%s%s.png"), szAppTitlesPath, pDatListInfo->szDrvName);
					if (!FileExists(szCover))
						szCover[0] = 0;

					RomDataShowPreview(hRDMgrWnd, szCover, IDC_ROMDATA_TITLE, IDC_ROMDATA_TITLE_FRAME, 0, 0);

					_stprintf(szCover, _T("%s%s.png"), szAppPreviewsPath, pDatListInfo->szRomSet);
					if (!FileExists(szCover))
						_stprintf(szCover, _T("%s%s.png"), szAppPreviewsPath, pDatListInfo->szDrvName);
					if (!FileExists(szCover))
						szCover[0] = 0;

					RomDataShowPreview(hRDMgrWnd, szCover, IDC_ROMDATA_PREVIEW, IDC_ROMDATA_PREVIEW_FRAME, 0, 0);

					free(pDatListInfo); pDatListInfo = NULL;
				}
			}
		}

		// Sort Change
		if (pNMLV->hdr.code == LVN_COLUMNCLICK && pNMLV->hdr.idFrom == IDC_ROMDATA_LIST) {
			sort_direction ^= 1;
			ListViewSort(sort_direction, pNMLV->iSubItem);
		}

		// Double Click
		if (pNMLV->hdr.code == NM_DBLCLK && pNMLV->hdr.idFrom == IDC_ROMDATA_LIST) {
			if (nSelItem >= 0) {
				memset(szRomdataName, 0, sizeof(szRomdataName));

				LVITEM LvItem     = { 0 };
				LvItem.iSubItem   = 3;	// Dat path column
				LvItem.pszText    = szRomdataName;
				LvItem.cchTextMax = sizeof(szRomdataName);

				SendMessage(hRDListView, LVM_GETITEMTEXT, (WPARAM)nSelItem, (LPARAM)&LvItem);

				if (0 == RomDataCheck(szRomdataName)) {
					RomDataManagerExit();
					EndDialog(hDlg, 0);

					DrvInit(BurnDrvGetIndex(RomdataGetDrvName()), true);
					MenuEnableItems();
					bAltPause = 0;
					if (bVidAutoSwitchFull) {
						nVidFullscreen = 1;
						POST_INITIALISE_MESSAGE;
					}
					POST_INITIALISE_MESSAGE;
				}
			}
		}
	}

	if (Msg == WM_COMMAND) {
		if (HIWORD(wParam) == STN_CLICKED) {
			INT32 nCtrlID = LOWORD(wParam);

			if (nCtrlID == IDC_ROMDATA_TITLE) {
				if (nSelItem >= 0) {
					TCHAR szSelDat[MAX_PATH] = { 0 };

					LVITEM LvItem     = { 0 };
					LvItem.iSubItem   = 3;	// Dat path column
					LvItem.pszText    = szSelDat;
					LvItem.cchTextMax = sizeof(szSelDat);

					SendMessage(hRDListView, LVM_GETITEMTEXT, (WPARAM)nSelItem, (LPARAM)&LvItem);

					DatListInfo* pDatListInfo = RomdataGetListInfo(szSelDat);
					if (NULL != pDatListInfo) {
						_stprintf(szCover, _T("%s%s.png"), szAppTitlesPath, pDatListInfo->szRomSet);
						if (!FileExists(szCover))
							_stprintf(szCover, _T("%s%s.png"), szAppTitlesPath, pDatListInfo->szDrvName);
						if (!FileExists(szCover)) {
							szCover[0] = 0;
							free(pDatListInfo); pDatListInfo = NULL;

							return 0;
						}

						free(pDatListInfo); pDatListInfo = NULL;
					}
					RomdataCoverInit();
				}
				return 0;
			}

			if (nCtrlID == IDC_ROMDATA_PREVIEW) {
				if (nSelItem >= 0) {
					TCHAR szSelDat[MAX_PATH] = { 0 };

					LVITEM LvItem     = { 0 };
					LvItem.iSubItem   = 3;	// Dat path column
					LvItem.pszText    = szSelDat;
					LvItem.cchTextMax = sizeof(szSelDat);

					SendMessage(hRDListView, LVM_GETITEMTEXT, (WPARAM)nSelItem, (LPARAM)&LvItem);

					DatListInfo* pDatListInfo = RomdataGetListInfo(szSelDat);
					if (NULL != pDatListInfo) {
						_stprintf(szCover, _T("%s%s.png"), szAppPreviewsPath, pDatListInfo->szRomSet);
						if (!FileExists(szCover))
							_stprintf(szCover, _T("%s%s.png"), szAppPreviewsPath, pDatListInfo->szDrvName);
						if (!FileExists(szCover)) {
							szCover[0] = 0;
							free(pDatListInfo); pDatListInfo = NULL;

							return 0;
						}
						free(pDatListInfo); pDatListInfo = NULL;
					}

					RomdataCoverInit();
				}
				return 0;
			}
		}

		if (LOWORD(wParam) == WM_DESTROY) {
			SendMessage(hDlg, WM_CLOSE, 0, 0);
		}

		if (HIWORD(wParam) == BN_CLICKED) {
			INT32 nCtrlID = LOWORD(wParam);

			switch (nCtrlID) {
				case IDC_ROMDATA_PLAY_BUTTON: {
					if (nSelItem >= 0) {
						memset(szRomdataName, 0, sizeof(szRomdataName));

						LVITEM LvItem     = { 0 };
						LvItem.iSubItem   = 3;	// Dat path column
						LvItem.pszText    = szRomdataName;
						LvItem.cchTextMax = sizeof(szRomdataName);

						SendMessage(hRDListView, LVM_GETITEMTEXT, (WPARAM)nSelItem, (LPARAM)&LvItem);

						if (0 == RomDataCheck(szRomdataName)) {
							RomDataManagerExit();
							EndDialog(hDlg, 0);

							DrvInit(BurnDrvGetIndex(RomdataGetDrvName()), true);	// Init the game driver
							MenuEnableItems();
							bAltPause = 0;
							if (bVidAutoSwitchFull) {
								nVidFullscreen = 1;
								POST_INITIALISE_MESSAGE;
							}
							POST_INITIALISE_MESSAGE;
						}
					}
					break;
				}

				case IDC_ROMDATA_SCAN_BUTTON: {
					RomDataClearList();
					RomdataListFindDats(szAppRomdataPath);
					SetFocus(hRDListView);
					break;
				}

				case IDC_ROMDATA_SELDIR_BUTTON: {
					RomDataClearList();
					SupportDirCreateTab(IDC_SUPPORTDIR_EDIT25, hRDMgrWnd);
					RomdataListFindDats(szAppRomdataPath);
					SetFocus(hRDListView);
					break;
				}

				case IDC_ROMDATA_SUBDIR_CHECK: {
					bRDListScanSub = (BST_CHECKED == IsDlgButtonChecked(hDlg, IDC_ROMDATA_SUBDIR_CHECK));
					RomDataClearList();
					RomdataListFindDats(szAppRomdataPath);
					SetFocus(hRDListView);
					break;
				}

				case IDC_ROMDATA_CANCEL_BUTTON: {
					RomDataManagerExit();
					EndDialog(hDlg, 0);
					break;
				}
			}
		}
	}
	return 0;
}

INT32 RomDataManagerInit()
{
	return FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_ROMDATA_MANAGER), hScrnWnd, (DLGPROC)RomDataManagerProc);
}
