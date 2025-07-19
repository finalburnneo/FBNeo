// Driver Selector module
// TreeView Version by HyperYagami
#include "burner.h"
#include <process.h>

// reduce the total number of sets by this number - (isgsm, neogeo, nmk004, pgm, skns, ym2608, coleco, msx_msx, spectrum, spec128, spec1282a, decocass, midssio, cchip, fdsbios, ngp, bubsys, channelf, namcoc69, namcoc70, namcoc75, snes stuff)
// don't reduce for these as we display them in the list (neogeo, neocdz)
#define REDUCE_TOTAL_SETS_BIOS      28

UINT_PTR nTimer					= 0;
UINT_PTR nInitPreviewTimer		= 0;
int nDialogSelect				= -1;										// The driver which this dialog selected
int nOldDlgSelected				= -1;
bool bDialogCancel				= false;

bool bDrvSelected				= false;
static bool bSelOkay			= false;									// true: About to run the non-RomData game from the list

static int nShowMVSCartsOnly	= 0;

HBITMAP hPrevBmp				= NULL;
HBITMAP hTitleBmp				= NULL;

HWND hSelDlg					= NULL;
static HWND hSelList			= NULL;
static HWND hParent				= NULL;
static HWND hInfoLabel[6]		= { NULL, NULL, NULL, NULL, NULL };			// 4 things in our Info-box
static HWND hInfoText[6]		= { NULL, NULL, NULL, NULL, NULL };			// 4 things in our Info-box

TCHAR szSearchString[256]       = { _T("") };                               // Stores the search text between game loads
bool bSearchStringInit          = false;                                    // Used (internally) while dialog is initting

static HBRUSH hWhiteBGBrush;
static HICON hExpand, hCollapse;
static HICON hNotWorking, hNotFoundEss, hNotFoundNonEss;

static HICON hDrvIconMiss;

static char TreeBuilding		= 0;										// if 1, ignore TVN_SELCHANGED messages

static int bImageOrientation;
static int UpdatePreview(bool bReset, TCHAR *szPath, int HorCtrl, int VerCrtl);

int	nIconsSize					= ICON_16x16;
int	nIconsSizeXY				= 16;
bool bEnableIcons				= 0;
bool bIconsLoaded				= 0;
bool bIconsOnlyParents			= 1;
bool bIconsByHardwares			= 0;
int nIconsXDiff;
int nIconsYDiff;
static HICON *hDrvIcon;
bool bGameInfoOpen				= false;

HICON* pIconsCache              = NULL;

// Dialog Sizing
int nSelDlgWidth = 750;
int nSelDlgHeight = 588;
static int nDlgInitialWidth;
static int nDlgInitialHeight;
static int nDlgOptionsGrpInitialPos[4];
static int nDlgAvailableChbInitialPos[4];
static int nDlgUnavailableChbInitialPos[4];
static int nDlgAlwaysClonesChbInitialPos[4];
static int nDlgZipnamesChbInitialPos[4];
static int nDlgLatinTextChbInitialPos[4];
static int nDlgSearchSubDirsChbInitialPos[4];
static int nDlgRomDirsBtnInitialPos[4];
static int nDlgScanRomsBtnInitialPos[4];
static int nDlgFilterGrpInitialPos[4];
static int nDlgFilterTreeInitialPos[4];
static int nDlgIpsGrpInitialPos[4];
static int nDlgApplyIpsChbInitialPos[4];
static int nDlgIpsManBtnInitialPos[4];
static int nDlgSearchGrpInitialPos[4];
static int nDlgSearchTxtInitialPos[4];
static int nDlgCancelBtnInitialPos[4];
static int nDlgPlayBtnInitialPos[4];
static int nDlgTitleGrpInitialPos[4];
static int nDlgTitleImgHInitialPos[4];
static int nDlgTitleImgVInitialPos[4];
static int nDlgPreviewGrpInitialPos[4];
static int nDlgPreviewImgHInitialPos[4];
static int nDlgPreviewImgVInitialPos[4];
static int nDlgWhiteBoxInitialPos[4];
static int nDlgGameInfoLblInitialPos[4];
static int nDlgRomNameLblInitialPos[4];
static int nDlgRomInfoLblInitialPos[4];
static int nDlgReleasedByLblInitialPos[4];
static int nDlgGenreLblInitialPos[4];
static int nDlgNotesLblInitialPos[4];
static int nDlgGameInfoTxtInitialPos[4];
static int nDlgRomNameTxtInitialPos[4];
static int nDlgRomInfoTxtInitialPos[4];
static int nDlgReleasedByTxtInitialPos[4];
static int nDlgGenreTxtInitialPos[4];
static int nDlgNotesTxtInitialPos[4];
static int nDlgDrvCountTxtInitialPos[4];
static int nDlgDrvRomInfoBtnInitialPos[4];
static int nDlgSelectGameGrpInitialPos[4];
static int nDlgSelectGameLstInitialPos[4];

static int _213 = 213;
static int _160 = 160;
static int dpi_x = 96;

// Filter TreeView
HWND hFilterList					= NULL;
HTREEITEM hFilterCapcomMisc			= NULL;
HTREEITEM hFilterCave				= NULL;
HTREEITEM hFilterCps1				= NULL;
HTREEITEM hFilterCps2				= NULL;
HTREEITEM hFilterCps3				= NULL;
HTREEITEM hFilterDataeast			= NULL;
HTREEITEM hFilterGalaxian			= NULL;
HTREEITEM hFilterIrem				= NULL;
HTREEITEM hFilterKaneko				= NULL;
HTREEITEM hFilterKonami				= NULL;
HTREEITEM hFilterNeogeo				= NULL;
HTREEITEM hFilterPacman				= NULL;
HTREEITEM hFilterPgm				= NULL;
HTREEITEM hFilterPsikyo				= NULL;
HTREEITEM hFilterSega				= NULL;
HTREEITEM hFilterSeta				= NULL;
HTREEITEM hFilterTaito				= NULL;
HTREEITEM hFilterTechnos			= NULL;
HTREEITEM hFilterToaplan			= NULL;
HTREEITEM hFilterMiscPre90s			= NULL;
HTREEITEM hFilterMiscPost90s		= NULL;
HTREEITEM hFilterMegadrive			= NULL;
HTREEITEM hFilterPce				= NULL;
HTREEITEM hFilterMsx				= NULL;
HTREEITEM hFilterNes				= NULL;
HTREEITEM hFilterFds				= NULL;
HTREEITEM hFilterSnes				= NULL;
HTREEITEM hFilterNgp				= NULL;
HTREEITEM hFilterChannelF			= NULL;
HTREEITEM hFilterSms				= NULL;
HTREEITEM hFilterGg					= NULL;
HTREEITEM hFilterSg1000				= NULL;
HTREEITEM hFilterColeco				= NULL;
HTREEITEM hFilterSpectrum			= NULL;
HTREEITEM hFilterMidway				= NULL;
HTREEITEM hFilterBootleg			= NULL;
HTREEITEM hFilterDemo				= NULL;
HTREEITEM hFilterHack				= NULL;
HTREEITEM hFilterHomebrew			= NULL;
HTREEITEM hFilterPrototype			= NULL;
HTREEITEM hFilterGenuine			= NULL;
HTREEITEM hFilterHorshoot			= NULL;
HTREEITEM hFilterVershoot			= NULL;
HTREEITEM hFilterScrfight			= NULL;
HTREEITEM hFilterVsfight			= NULL;
HTREEITEM hFilterBios				= NULL;
HTREEITEM hFilterBreakout			= NULL;
HTREEITEM hFilterCasino				= NULL;
HTREEITEM hFilterBallpaddle			= NULL;
HTREEITEM hFilterMaze				= NULL;
HTREEITEM hFilterMinigames			= NULL;
HTREEITEM hFilterPinball			= NULL;
HTREEITEM hFilterPlatform			= NULL;
HTREEITEM hFilterPuzzle				= NULL;
HTREEITEM hFilterQuiz				= NULL;
HTREEITEM hFilterSportsmisc			= NULL;
HTREEITEM hFilterSportsfootball 	= NULL;
HTREEITEM hFilterMisc				= NULL;
HTREEITEM hFilterMahjong			= NULL;
HTREEITEM hFilterRacing				= NULL;
HTREEITEM hFilterShoot				= NULL;
HTREEITEM hFilterRungun 			= NULL;
HTREEITEM hFilterAction				= NULL;
HTREEITEM hFilterStrategy   		= NULL;
HTREEITEM hFilterRpg        		= NULL;
HTREEITEM hFilterSim        		= NULL;
HTREEITEM hFilterAdv        		= NULL;
HTREEITEM hFilterCard        		= NULL;
HTREEITEM hFilterBoard        		= NULL;
HTREEITEM hFilterOtherFamily		= NULL;
HTREEITEM hFilterMslug				= NULL;
HTREEITEM hFilterSf					= NULL;
HTREEITEM hFilterKof				= NULL;
HTREEITEM hFilterDstlk				= NULL;
HTREEITEM hFilterFatfury			= NULL;
HTREEITEM hFilterSamsho				= NULL;
HTREEITEM hFilter19xx				= NULL;
HTREEITEM hFilterSonicwi			= NULL;
HTREEITEM hFilterPwrinst			= NULL;
HTREEITEM hFilterSonic				= NULL;
HTREEITEM hFilterDonpachi			= NULL;
HTREEITEM hFilterMahou				= NULL;
HTREEITEM hFilterCapcomGrp			= NULL;
HTREEITEM hFilterSegaGrp			= NULL;

HTREEITEM hRoot						= NULL;
HTREEITEM hBoardType				= NULL;
HTREEITEM hFamily					= NULL;
HTREEITEM hGenre					= NULL;
HTREEITEM hFavorites				= NULL;
HTREEITEM hHardware					= NULL;

// GCC doesn't seem to define these correctly.....
#define _TreeView_SetItemState(hwndTV, hti, data, _mask) \
{ TVITEM _ms_TVi;\
  _ms_TVi.mask = TVIF_STATE; \
  _ms_TVi.hItem = hti; \
  _ms_TVi.stateMask = _mask;\
  _ms_TVi.state = data;\
  SNDMSG((hwndTV), TVM_SETITEM, 0, (LPARAM)(TV_ITEM *)&_ms_TVi);\
}

#define _TreeView_SetCheckState(hwndTV, hti, fCheck) \
  _TreeView_SetItemState(hwndTV, hti, INDEXTOSTATEIMAGEMASK((fCheck)?2:1), TVIS_STATEIMAGEMASK)

// -----------------------------------------------------------------------------------------------------------------

#define DISABLE_NON_AVAILABLE_SELECT	0						// Disable selecting non-available sets
#define NON_WORKING_PROMPT_ON_LOAD		1						// Prompt user on loading non-working sets

static UINT64 CapcomMiscValue		= HARDWARE_PREFIX_CAPCOM_MISC >> 24;
static UINT64 MASKCAPMISC			= 1 << CapcomMiscValue;
static UINT64 CaveValue				= HARDWARE_PREFIX_CAVE >> 24;
static UINT64 MASKCAVE				= 1 << CaveValue;
static UINT64 CpsValue				= HARDWARE_PREFIX_CAPCOM >> 24;
static UINT64 MASKCPS				= 1 << CpsValue;
static UINT64 Cps2Value				= HARDWARE_PREFIX_CPS2 >> 24;
static UINT64 MASKCPS2				= 1 << Cps2Value;
static UINT64 Cps3Value				= HARDWARE_PREFIX_CPS3 >> 24;
static UINT64 MASKCPS3				= 1 << Cps3Value;
static UINT64 DataeastValue			= HARDWARE_PREFIX_DATAEAST >> 24;
static UINT64 MASKDATAEAST			= 1 << DataeastValue;
static UINT64 GalaxianValue			= HARDWARE_PREFIX_GALAXIAN >> 24;
static UINT64 MASKGALAXIAN			= 1 << GalaxianValue;
static UINT64 IremValue				= HARDWARE_PREFIX_IREM >> 24;
static UINT64 MASKIREM				= 1 << IremValue;
static UINT64 KanekoValue			= HARDWARE_PREFIX_KANEKO >> 24;
static UINT64 MASKKANEKO			= 1 << KanekoValue;
static UINT64 KonamiValue			= HARDWARE_PREFIX_KONAMI >> 24;
static UINT64 MASKKONAMI			= 1 << KonamiValue;
static UINT64 NeogeoValue			= HARDWARE_PREFIX_SNK >> 24;
static UINT64 MASKNEOGEO			= 1 << NeogeoValue;
static UINT64 PacmanValue			= HARDWARE_PREFIX_PACMAN >> 24;
static UINT64 MASKPACMAN			= 1 << PacmanValue;
static UINT64 PgmValue				= HARDWARE_PREFIX_IGS_PGM >> 24;
static UINT64 MASKPGM				= 1 << PgmValue;
static UINT64 PsikyoValue			= HARDWARE_PREFIX_PSIKYO >> 24;
static UINT64 MASKPSIKYO			= 1 << PsikyoValue;
static UINT64 SegaValue				= HARDWARE_PREFIX_SEGA >> 24;
static UINT64 MASKSEGA				= 1 << SegaValue;
static UINT64 SetaValue				= HARDWARE_PREFIX_SETA >> 24;
static UINT64 MASKSETA				= 1 << SetaValue;
static UINT64 TaitoValue			= HARDWARE_PREFIX_TAITO >> 24;
static UINT64 MASKTAITO				= 1 << TaitoValue;
static UINT64 TechnosValue			= HARDWARE_PREFIX_TECHNOS >> 24;
static UINT64 MASKTECHNOS			= 1 << TechnosValue;
static UINT64 ToaplanValue			= HARDWARE_PREFIX_TOAPLAN >> 24;
static UINT64 MASKTOAPLAN			= 1 << ToaplanValue;
static UINT64 MiscPre90sValue		= HARDWARE_PREFIX_MISC_PRE90S >> 24;
static UINT64 MASKMISCPRE90S		= 1 << MiscPre90sValue;
static UINT64 MiscPost90sValue		= HARDWARE_PREFIX_MISC_POST90S >> 24;
static UINT64 MASKMISCPOST90S		= 1 << MiscPost90sValue;
static UINT64 MegadriveValue		= HARDWARE_PREFIX_SEGA_MEGADRIVE >> 24;
static UINT64 MASKMEGADRIVE			= 1 << MegadriveValue;
static UINT64 PCEngineValue			= HARDWARE_PREFIX_PCENGINE >> 24;
static UINT64 MASKPCENGINE			= 1 << PCEngineValue;
static UINT64 SmsValue				= HARDWARE_PREFIX_SEGA_MASTER_SYSTEM >> 24;
static UINT64 MASKSMS				= 1 << SmsValue;
static UINT64 GgValue				= HARDWARE_PREFIX_SEGA_GAME_GEAR >> 24;
static UINT64 MASKGG				= 1 << GgValue;
static UINT64 Sg1000Value			= HARDWARE_PREFIX_SEGA_SG1000 >> 24;
static UINT64 MASKSG1000			= 1 << Sg1000Value;
static UINT64 ColecoValue			= HARDWARE_PREFIX_COLECO >> 24;
static UINT64 MASKCOLECO			= 1 << ColecoValue;
static UINT64 MsxValue				= HARDWARE_PREFIX_MSX >> 24;
static UINT64 MASKMSX				= 1 << MsxValue;
static UINT64 SpecValue				= HARDWARE_PREFIX_SPECTRUM >> 24;
static UINT64 MASKSPECTRUM			= 1 << SpecValue;
static UINT64 MidwayValue			= HARDWARE_PREFIX_MIDWAY >> 24;
static UINT64 MASKMIDWAY			= 1 << MidwayValue;
static UINT64 NesValue				= HARDWARE_PREFIX_NES >> 24;
static UINT64 MASKNES				= 1 << NesValue;

// this is where things start going above the 32bit-zone. *solved w/64bit UINT?*
static UINT64 FdsValue				= (UINT64)HARDWARE_PREFIX_FDS >> 24;
static UINT64 MASKFDS				= (UINT64)1 << FdsValue;            // 1 << 0x1f - needs casting or.. bonkers!
static UINT64 SnesValue				= (UINT64)HARDWARE_PREFIX_SNES >> 24;
static UINT64 MASKSNES				= (UINT64)1 << SnesValue;
static UINT64 NgpValue				= (UINT64)HARDWARE_PREFIX_NGP >> 24;
static UINT64 MASKNGP				= (UINT64)1 << NgpValue;
static UINT64 ChannelFValue			= (UINT64)HARDWARE_PREFIX_CHANNELF >> 24;
static UINT64 MASKCHANNELF			= (UINT64)1 << ChannelFValue;

static UINT64 MASKALL				= ((UINT64)MASKCAPMISC | MASKCAVE | MASKCPS | MASKCPS2 | MASKCPS3 | MASKDATAEAST | MASKGALAXIAN | MASKIREM | MASKKANEKO | MASKKONAMI | MASKNEOGEO | MASKPACMAN | MASKPGM | MASKPSIKYO | MASKSEGA | MASKSETA | MASKTAITO | MASKTECHNOS | MASKTOAPLAN | MASKMISCPRE90S | MASKMISCPOST90S | MASKMEGADRIVE | MASKPCENGINE | MASKSMS | MASKGG | MASKSG1000 | MASKCOLECO | MASKMSX | MASKSPECTRUM | MASKMIDWAY | MASKNES | MASKFDS | MASKSNES | MASKNGP | MASKCHANNELF );

#define SEARCHSUBDIRS			(1 << 26)
#define UNAVAILABLE				(1 << 27)
#define AVAILABLE				(1 << 28)
#define AUTOEXPAND				(1 << 29)
#define SHOWSHORT				(1 << 30)
#define ASCIIONLY				(1 << 31)

#define MASKBOARDTYPEGENUINE	(1)
#define MASKFAMILYOTHER			0x10000000

#define MASKALLGENRE			(GBF_HORSHOOT | GBF_VERSHOOT | GBF_SCRFIGHT | GBF_VSFIGHT | GBF_BIOS | GBF_BREAKOUT | GBF_CASINO | GBF_BALLPADDLE | GBF_MAZE | GBF_MINIGAMES | GBF_PINBALL | GBF_PLATFORM | GBF_PUZZLE | GBF_QUIZ | GBF_SPORTSMISC | GBF_SPORTSFOOTBALL | GBF_MISC | GBF_MAHJONG | GBF_RACING | GBF_SHOOT | GBF_ACTION | GBF_RUNGUN | GBF_STRATEGY | GBF_RPG | GBF_SIM | GBF_ADV | GBF_CARD | GBF_BOARD)
#define MASKALLFAMILY			(MASKFAMILYOTHER | FBF_MSLUG | FBF_SF | FBF_KOF | FBF_DSTLK | FBF_FATFURY | FBF_SAMSHO | FBF_19XX | FBF_SONICWI | FBF_PWRINST | FBF_SONIC | FBF_DONPACHI | FBF_MAHOU)
#define MASKALLBOARD			(MASKBOARDTYPEGENUINE | BDF_BOOTLEG | BDF_DEMO | BDF_HACK | BDF_HOMEBREW | BDF_PROTOTYPE)

#define MASKCAPGRP				(MASKCAPMISC | MASKCPS | MASKCPS2 | MASKCPS3)
#define MASKSEGAGRP				(MASKSEGA | MASKSG1000 | MASKSMS | MASKMEGADRIVE | MASKGG)

UINT64 nLoadMenuShowX			= 0; // hardware etc
int nLoadMenuShowY				= AVAILABLE; // selector options, default to show available
int nLoadMenuBoardTypeFilter	= 0;
int nLoadMenuGenreFilter		= 0;
int nLoadMenuFavoritesFilter	= 0;
int nLoadMenuFamilyFilter		= 0;

// expanded/collapsed state of filter nodes. default: expand main (0x01) & hardware (0x10)
int nLoadMenuExpand             = 0x10 | 0x01;

struct NODEINFO {
	int nBurnDrvNo;
	bool bIsParent;
	char* pszROMName;
	HTREEITEM hTreeHandle;
};

static NODEINFO* nBurnDrv = NULL;
static NODEINFO* nBurnZipListDrv = NULL;
static unsigned int nTmpDrvCount;

// prototype  -----------------------
static void RebuildEverything();
// ----------------------------------

// Dialog sizing support functions and macros (everything working in client co-ords)
#define GetInititalControlPos(a, b)								\
	GetWindowRect(GetDlgItem(hSelDlg, a), &rect);				\
	memset(&point, 0, sizeof(POINT));							\
	point.x = rect.left;										\
	point.y = rect.top;											\
	ScreenToClient(hSelDlg, &point);							\
	b[0] = point.x;												\
	b[1] = point.y;												\
	GetClientRect(GetDlgItem(hSelDlg, a), &rect);				\
	b[2] = rect.right;											\
	b[3] = rect.bottom;

#define SetControlPosAlignTopRight(a, b)						\
	SetWindowPos(GetDlgItem(hSelDlg, a), hSelDlg, b[0] - xDelta, b[1], 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOSENDCHANGING);

#define SetControlPosAlignTopLeft(a, b)							\
	SetWindowPos(GetDlgItem(hSelDlg, a), hSelDlg, b[0], b[1], 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOSENDCHANGING);

#define SetControlPosAlignBottomRight(a, b)						\
	SetWindowPos(GetDlgItem(hSelDlg, a), hSelDlg, b[0] - xDelta, b[1] - yDelta, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOSENDCHANGING);

#define SetControlPosAlignBottomLeftResizeHor(a, b)				\
	SetWindowPos(GetDlgItem(hSelDlg, a), hSelDlg, b[0], b[1] - yDelta, b[2] - xDelta, b[3], SWP_NOZORDER | SWP_NOSENDCHANGING);

#define SetControlPosAlignTopRightResizeVert(a, b)				\
	xScrollBarDelta = (a == IDC_TREE2) ? -18 : 0;				\
	SetWindowPos(GetDlgItem(hSelDlg, a), hSelDlg, b[0] - xDelta, b[1], b[2] - xScrollBarDelta, b[3] - yDelta, SWP_NOZORDER | SWP_NOSENDCHANGING);

#define SetControlPosAlignTopLeftResizeHorVert(a, b)			\
	xScrollBarDelta = (a == IDC_TREE1) ? -18 : 0;				\
	SetWindowPos(GetDlgItem(hSelDlg, a), hSelDlg, b[0], b[1], b[2] - xDelta - xScrollBarDelta, b[3] - yDelta, SWP_NOZORDER | SWP_NOSENDCHANGING);

#define SetControlPosAlignTopLeftResizeHorVertALT(a, b)			\
	SetWindowPos(GetDlgItem(hSelDlg, a), hSelDlg, b[0], b[1], b[2] - xDelta, b[3] - yDelta, SWP_NOZORDER | SWP_NOSENDCHANGING);

static void GetTitlePreviewScale()
{
	RECT rect;
	GetWindowRect(GetDlgItem(hSelDlg, IDC_STATIC2), &rect);
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;

	w = w * 90 / 100; // make W 90% of the "Preview / Title" windowpane
	h = w * 75 / 100; // make H 75% of w (4:3)

	_213 = w;
	_160 = h;

	HDC hDc = GetDC(0);

	dpi_x = GetDeviceCaps(hDc, LOGPIXELSX);
	//bprintf(0, _T("dpi_x is %d\n"), dpi_x);
	ReleaseDC(0, hDc);
}

static void GetInitialPositions()
{
	RECT rect;
	POINT point;

	GetClientRect(hSelDlg, &rect);
	nDlgInitialWidth = rect.right;
	nDlgInitialHeight = rect.bottom;

	GetInititalControlPos(IDC_STATIC_OPT, nDlgOptionsGrpInitialPos);
	GetInititalControlPos(IDC_CHECKAVAILABLE, nDlgAvailableChbInitialPos);
	GetInititalControlPos(IDC_CHECKUNAVAILABLE, nDlgUnavailableChbInitialPos);
	GetInititalControlPos(IDC_CHECKAUTOEXPAND, nDlgAlwaysClonesChbInitialPos);
	GetInititalControlPos(IDC_SEL_SHORTNAME, nDlgZipnamesChbInitialPos);
	GetInititalControlPos(IDC_SEL_ASCIIONLY, nDlgLatinTextChbInitialPos);
	GetInititalControlPos(IDC_SEL_SUBDIRS, nDlgSearchSubDirsChbInitialPos);
	GetInititalControlPos(IDROM, nDlgRomDirsBtnInitialPos);
	GetInititalControlPos(IDRESCAN, nDlgScanRomsBtnInitialPos);
	GetInititalControlPos(IDC_STATIC_SYS, nDlgFilterGrpInitialPos);
	GetInititalControlPos(IDC_TREE2, nDlgFilterTreeInitialPos);
	GetInititalControlPos(IDC_SEL_IPSGROUP, nDlgIpsGrpInitialPos);
	GetInititalControlPos(IDC_SEL_APPLYIPS, nDlgApplyIpsChbInitialPos);
	GetInititalControlPos(IDC_SEL_IPSMANAGER, nDlgIpsManBtnInitialPos);
	GetInititalControlPos(IDC_SEL_SEARCHGROUP, nDlgSearchGrpInitialPos);
	GetInititalControlPos(IDC_SEL_SEARCH, nDlgSearchTxtInitialPos);
	GetInititalControlPos(IDCANCEL, nDlgCancelBtnInitialPos);
	GetInititalControlPos(IDOK, nDlgPlayBtnInitialPos);
	GetInititalControlPos(IDC_STATIC3, nDlgTitleGrpInitialPos);
	GetInititalControlPos(IDC_SCREENSHOT2_H, nDlgTitleImgHInitialPos);
	GetInititalControlPos(IDC_SCREENSHOT2_V, nDlgTitleImgVInitialPos);
	GetInititalControlPos(IDC_STATIC2, nDlgPreviewGrpInitialPos);
	GetInititalControlPos(IDC_SCREENSHOT_H, nDlgPreviewImgHInitialPos);
	GetInititalControlPos(IDC_SCREENSHOT_V, nDlgPreviewImgVInitialPos);
	GetInititalControlPos(IDC_STATIC_INFOBOX, nDlgWhiteBoxInitialPos);
	GetInititalControlPos(IDC_LABELCOMMENT, nDlgGameInfoLblInitialPos);
	GetInititalControlPos(IDC_LABELROMNAME, nDlgRomNameLblInitialPos);
	GetInititalControlPos(IDC_LABELROMINFO, nDlgRomInfoLblInitialPos);
	GetInititalControlPos(IDC_LABELSYSTEM, nDlgReleasedByLblInitialPos);
	GetInititalControlPos(IDC_LABELGENRE, nDlgGenreLblInitialPos);
	GetInititalControlPos(IDC_LABELNOTES, nDlgNotesLblInitialPos);
	GetInititalControlPos(IDC_TEXTCOMMENT, nDlgGameInfoTxtInitialPos);
	GetInititalControlPos(IDC_TEXTROMNAME, nDlgRomNameTxtInitialPos);
	GetInititalControlPos(IDC_TEXTROMINFO, nDlgRomInfoTxtInitialPos);
	GetInititalControlPos(IDC_TEXTSYSTEM, nDlgReleasedByTxtInitialPos);
	GetInititalControlPos(IDC_TEXTGENRE, nDlgGenreTxtInitialPos);
	GetInititalControlPos(IDC_TEXTNOTES, nDlgNotesTxtInitialPos);
	GetInititalControlPos(IDC_DRVCOUNT, nDlgDrvCountTxtInitialPos);
	GetInititalControlPos(IDGAMEINFO, nDlgDrvRomInfoBtnInitialPos);
	GetInititalControlPos(IDC_STATIC1, nDlgSelectGameGrpInitialPos);
	GetInititalControlPos(IDC_TREE1, nDlgSelectGameLstInitialPos);

	GetTitlePreviewScale();

	// When the window is created with too few entries for TreeView to warrant the
	// use of a vertical scrollbar, the right side will be slightly askew. -dink

	if (nDlgSelectGameGrpInitialPos[2] - nDlgSelectGameLstInitialPos[2] < 30 ) {
		nDlgSelectGameLstInitialPos[2] = nDlgSelectGameGrpInitialPos[2] - 34;
	}

	if (nDlgFilterGrpInitialPos[2] - nDlgFilterTreeInitialPos[2] < 30 ) {
		nDlgFilterTreeInitialPos[2] = nDlgFilterGrpInitialPos[2] - 34;
	}
}

// Check if a specified driver is working
static bool CheckWorkingStatus(int nDriver)
{
	int nOldnBurnDrvActive = nBurnDrvActive;
	nBurnDrvActive = nDriver;
	bool bStatus = BurnDrvIsWorking();
	nBurnDrvActive = nOldnBurnDrvActive;

	return bStatus;
}

static TCHAR* MangleGamename(const TCHAR* szOldName, bool /*bRemoveArticle*/)
{
	static TCHAR szNewName[256] = _T("");

#if 0
	TCHAR* pszName = szNewName;

	if (_tcsnicmp(szOldName, _T("the "), 4) == 0) {
		int x = 0, y = 0;
		while (szOldName[x] && szOldName[x] != _T('(') && szOldName[x] != _T('-')) {
			x++;
		}
		y = x;
		while (y && szOldName[y - 1] == _T(' ')) {
			y--;
		}
		_tcsncpy(pszName, szOldName + 4, y - 4);
		pszName[y - 4] = _T('\0');
		pszName += y - 4;

		if (!bRemoveArticle) {
			pszName += _stprintf(pszName, _T(", the"));
		}
		if (szOldName[x]) {
			_stprintf(pszName, _T(" %s"), szOldName + x);
		}
	} else {
		_tcscpy(pszName, szOldName);
	}
#endif

#if 0
	static TCHAR szPrefix[256] = _T("");
	// dink's prefix-izer - it's useless...
	_tcscpy(szPrefix, BurnDrvGetText(DRV_NAME));
	int use_prefix = 0;
	int nn_len = 0;
	for (int i = 0; i < 6; i++) {
		if (szPrefix[i] == _T('_')) {
			szNewName[nn_len++] = _T(' ');
			use_prefix = 1;
			break;
		} else {
			szNewName[nn_len++] = towupper(szPrefix[i]);
		}
	}
	if (use_prefix == 0) nn_len = 0;
#endif

#if 1
	_tcscpy(szNewName, szOldName);
#endif

	return szNewName;
}

static TCHAR* RemoveSpace(const TCHAR* szOldName)
{
	if (NULL == szOldName) return NULL;

	static TCHAR szNewName[256] = _T("");
	int j = 0;
	int i = 0;

	for (i = 0; szOldName[i]; i++) {
		if (!iswspace(szOldName[i])) {
			szNewName[j++] = szOldName[i];
		}
	}

	szNewName[j] = _T('\0');

	return szNewName;
}

static int DoExtraFilters()
{
	if (nLoadMenuFavoritesFilter) {
		return (CheckFavorites(BurnDrvGetTextA(DRV_NAME)) != -1) ? 0 : 1;
	}

	if (nShowMVSCartsOnly && ((BurnDrvGetHardwareCode() & HARDWARE_PREFIX_CARTRIDGE) != HARDWARE_PREFIX_CARTRIDGE)) return 1;

	if ((nLoadMenuBoardTypeFilter & BDF_BOOTLEG)			&& (BurnDrvGetFlags() & BDF_BOOTLEG))				return 1;
	if ((nLoadMenuBoardTypeFilter & BDF_DEMO)				&& (BurnDrvGetFlags() & BDF_DEMO))					return 1;
	if ((nLoadMenuBoardTypeFilter & BDF_HACK)				&& (BurnDrvGetFlags() & BDF_HACK))					return 1;
	if ((nLoadMenuBoardTypeFilter & BDF_HOMEBREW)			&& (BurnDrvGetFlags() & BDF_HOMEBREW))				return 1;
	if ((nLoadMenuBoardTypeFilter & BDF_PROTOTYPE)			&& (BurnDrvGetFlags() & BDF_PROTOTYPE))				return 1;

	if ((nLoadMenuBoardTypeFilter & MASKBOARDTYPEGENUINE)	&& (!(BurnDrvGetFlags() & BDF_BOOTLEG))
															&& (!(BurnDrvGetFlags() & BDF_DEMO))
															&& (!(BurnDrvGetFlags() & BDF_HACK))
															&& (!(BurnDrvGetFlags() & BDF_HOMEBREW))
															&& (!(BurnDrvGetFlags() & BDF_PROTOTYPE)))	return 1;

	if ((nLoadMenuFamilyFilter & FBF_MSLUG)					&& (BurnDrvGetFamilyFlags() & FBF_MSLUG))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_SF)					&& (BurnDrvGetFamilyFlags() & FBF_SF))				return 1;
	if ((nLoadMenuFamilyFilter & FBF_KOF)					&& (BurnDrvGetFamilyFlags() & FBF_KOF))				return 1;
	if ((nLoadMenuFamilyFilter & FBF_DSTLK)					&& (BurnDrvGetFamilyFlags() & FBF_DSTLK))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_FATFURY)				&& (BurnDrvGetFamilyFlags() & FBF_FATFURY))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_SAMSHO)				&& (BurnDrvGetFamilyFlags() & FBF_SAMSHO))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_19XX)					&& (BurnDrvGetFamilyFlags() & FBF_19XX))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_SONICWI)				&& (BurnDrvGetFamilyFlags() & FBF_SONICWI))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_PWRINST)				&& (BurnDrvGetFamilyFlags() & FBF_PWRINST))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_SONIC)					&& (BurnDrvGetFamilyFlags() & FBF_SONIC))			return 1;
	if ((nLoadMenuFamilyFilter & FBF_DONPACHI)				&& (BurnDrvGetFamilyFlags() & FBF_DONPACHI))		return 1;
	if ((nLoadMenuFamilyFilter & FBF_MAHOU)					&& (BurnDrvGetFamilyFlags() & FBF_MAHOU))			return 1;

	if ((nLoadMenuFamilyFilter & MASKFAMILYOTHER)			&& (!(BurnDrvGetFamilyFlags() & FBF_MSLUG))
															&& (!(BurnDrvGetFamilyFlags() & FBF_SF))
															&& (!(BurnDrvGetFamilyFlags() & FBF_KOF))
															&& (!(BurnDrvGetFamilyFlags() & FBF_DSTLK))
															&& (!(BurnDrvGetFamilyFlags() & FBF_FATFURY))
															&& (!(BurnDrvGetFamilyFlags() & FBF_SAMSHO))
															&& (!(BurnDrvGetFamilyFlags() & FBF_19XX))
															&& (!(BurnDrvGetFamilyFlags() & FBF_SONICWI))
															&& (!(BurnDrvGetFamilyFlags() & FBF_PWRINST))
															&& (!(BurnDrvGetFamilyFlags() & FBF_SONIC))
															&& (!(BurnDrvGetFamilyFlags() & FBF_DONPACHI))
															&& (!(BurnDrvGetFamilyFlags() & FBF_MAHOU)) )		return 1;


	// This filter supports multiple genre's per game
	int bGenreOk = 0;
	if ((~nLoadMenuGenreFilter & GBF_HORSHOOT)				&& (BurnDrvGetGenreFlags() & GBF_HORSHOOT))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_VERSHOOT)				&& (BurnDrvGetGenreFlags() & GBF_VERSHOOT))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_SCRFIGHT)				&& (BurnDrvGetGenreFlags() & GBF_SCRFIGHT))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_VSFIGHT)				&& (BurnDrvGetGenreFlags() & GBF_VSFIGHT))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_BIOS)					&& (BurnDrvGetGenreFlags() & GBF_BIOS))				bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_BREAKOUT)				&& (BurnDrvGetGenreFlags() & GBF_BREAKOUT))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_CASINO)				&& (BurnDrvGetGenreFlags() & GBF_CASINO))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_BALLPADDLE)			&& (BurnDrvGetGenreFlags() & GBF_BALLPADDLE))		bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_MAZE)					&& (BurnDrvGetGenreFlags() & GBF_MAZE))				bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_MINIGAMES)				&& (BurnDrvGetGenreFlags() & GBF_MINIGAMES))		bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_PINBALL)				&& (BurnDrvGetGenreFlags() & GBF_PINBALL))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_PLATFORM)				&& (BurnDrvGetGenreFlags() & GBF_PLATFORM))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_PUZZLE)				&& (BurnDrvGetGenreFlags() & GBF_PUZZLE))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_QUIZ)					&& (BurnDrvGetGenreFlags() & GBF_QUIZ))				bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_SPORTSMISC)			&& (BurnDrvGetGenreFlags() & GBF_SPORTSMISC))		bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_SPORTSFOOTBALL) 		&& (BurnDrvGetGenreFlags() & GBF_SPORTSFOOTBALL))	bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_MISC)					&& (BurnDrvGetGenreFlags() & GBF_MISC))				bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_MAHJONG)				&& (BurnDrvGetGenreFlags() & GBF_MAHJONG))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_RACING)				&& (BurnDrvGetGenreFlags() & GBF_RACING))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_SHOOT)					&& (BurnDrvGetGenreFlags() & GBF_SHOOT))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_ACTION)				&& (BurnDrvGetGenreFlags() & GBF_ACTION))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_RUNGUN)				&& (BurnDrvGetGenreFlags() & GBF_RUNGUN))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_STRATEGY)				&& (BurnDrvGetGenreFlags() & GBF_STRATEGY))			bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_RPG)					&& (BurnDrvGetGenreFlags() & GBF_RPG))				bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_SIM)					&& (BurnDrvGetGenreFlags() & GBF_SIM))				bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_ADV)					&& (BurnDrvGetGenreFlags() & GBF_ADV))				bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_CARD)					&& (BurnDrvGetGenreFlags() & GBF_CARD))				bGenreOk = 1;
	if ((~nLoadMenuGenreFilter & GBF_BOARD)					&& (BurnDrvGetGenreFlags() & GBF_BOARD))			bGenreOk = 1;
	if (bGenreOk == 0) return 1;

	return 0;
}

static int ZipNames_qs_cmp_desc(const void *p0, const void *p1) {
	struct NODEINFO *ni0 = (struct NODEINFO*) p0;
	struct NODEINFO *ni1 = (struct NODEINFO*) p1;

	return stricmp(ni1->pszROMName, ni0->pszROMName);
}

// Make a tree-view control with all drivers
static int SelListMake()
{
	int i, j;
	unsigned int nMissingDrvCount = 0;

	if (nBurnDrv) {
		free(nBurnDrv);
		nBurnDrv = NULL;
	}
	nBurnDrv = (NODEINFO*)malloc(nBurnDrvCount * sizeof(NODEINFO));
	memset(nBurnDrv, 0, nBurnDrvCount * sizeof(NODEINFO));

	nTmpDrvCount = 0;

	if (hSelList == NULL) {
		return 1;
	}

	LoadFavorites();

	// Get dialog search string
	j = GetDlgItemText(hSelDlg, IDC_SEL_SEARCH, szSearchString, sizeof(szSearchString));
	for (UINT32 k = 0; k < j; k++)
		szSearchString[k] = _totlower(szSearchString[k]);

	// pre-sort the list if using zip names
	if (nLoadMenuShowY & SHOWSHORT)	{
		// make a list of everything
		if (nBurnZipListDrv) {
			free(nBurnZipListDrv);
			nBurnZipListDrv = NULL;
		}
		nBurnZipListDrv = (NODEINFO*)malloc(nBurnDrvCount * sizeof(NODEINFO));
		memset(nBurnZipListDrv, 0, nBurnDrvCount * sizeof(NODEINFO));

		for (i = nBurnDrvCount-1; i >= 0; i--) {
			nBurnDrvActive = i;
			nBurnZipListDrv[i].pszROMName = BurnDrvGetTextA(DRV_NAME);
			nBurnZipListDrv[i].nBurnDrvNo = i;
		}

		// sort it in descending order (we add in descending order)
		qsort(nBurnZipListDrv, nBurnDrvCount, sizeof(NODEINFO), ZipNames_qs_cmp_desc);
	}

	// Add all the driver names to the list
	// 1st: parents
	for (i = nBurnDrvCount-1; i >= 0; i--) {
		TV_INSERTSTRUCT TvItem;

		nBurnDrvActive = i;																// Switch to driver i

		// if showing zip names get active entry from our sorted list
		if (nLoadMenuShowY & SHOWSHORT) nBurnDrvActive = nBurnZipListDrv[nBurnDrvCount - 1 - i].nBurnDrvNo;

		if (BurnDrvGetFlags() & BDF_BOARDROM) {
			continue;
		}

		if (BurnDrvGetText(DRV_PARENT) != NULL && (BurnDrvGetFlags() & BDF_CLONE)) {	// Skip clones
			continue;
		}

		if(!gameAv[nBurnDrvActive]) nMissingDrvCount++;

		UINT64 nHardware = (UINT64)1 << (((UINT32)BurnDrvGetHardwareCode() >> 24) & 0x3f);
		if ((nHardware & MASKALL) && ((nHardware & nLoadMenuShowX) || (nHardware & MASKALL) == 0)) {
			continue;
		}

		if (avOk && (!(nLoadMenuShowY & UNAVAILABLE)) && !gameAv[nBurnDrvActive]) {						// Skip non-available games if needed
			continue;
		}

		if (avOk && (!(nLoadMenuShowY & AVAILABLE)) && gameAv[nBurnDrvActive]) {						// Skip available games if needed
			continue;
		}

		if (DoExtraFilters()) continue;

		if (szSearchString[0]) {
			TCHAR *StringFound = NULL;
			TCHAR *StringFound1 = NULL;
			TCHAR *StringFound2 = NULL;
			TCHAR *StringFound3 = NULL;
			TCHAR szDriverName[256] = { _T("") };
			TCHAR szDriverNameA[256] = { _T("") };
			TCHAR szManufacturerName[256] = { _T("") };
			wcscpy(szDriverName, BurnDrvGetText(DRV_FULLNAME));
			swprintf(szDriverNameA, _T("%S"), BurnDrvGetTextA(DRV_FULLNAME));
			swprintf(szManufacturerName, _T("%s %s"), BurnDrvGetText(DRV_MANUFACTURER), BurnDrvGetText(DRV_SYSTEM));
			for (int k = 0; k < 256; k++) {
				szDriverName[k] = _totlower(szDriverName[k]);
				szDriverNameA[k] = _totlower(szDriverNameA[k]);
				szManufacturerName[k] = _totlower(szManufacturerName[k]);
			}
			StringFound = wcsstr(szDriverName, szSearchString);
			StringFound1 = wcsstr(szDriverNameA, szSearchString);
			StringFound2 = wcsstr(BurnDrvGetText(DRV_NAME), szSearchString);
			StringFound3 = wcsstr(szManufacturerName, szSearchString);

			if (!StringFound && !StringFound1 && !StringFound2 && !StringFound3) continue;
		}

		memset(&TvItem, 0, sizeof(TvItem));
		TvItem.item.mask = TVIF_TEXT | TVIF_PARAM;
		TvItem.hInsertAfter = TVI_FIRST;
		TvItem.item.pszText = (nLoadMenuShowY & SHOWSHORT) ? BurnDrvGetText(DRV_NAME) : RemoveSpace(BurnDrvGetText(DRV_ASCIIONLY | DRV_FULLNAME));
		TvItem.item.lParam = (LPARAM)&nBurnDrv[nTmpDrvCount];
		nBurnDrv[nTmpDrvCount].hTreeHandle = (HTREEITEM)SendMessage(hSelList, TVM_INSERTITEM, 0, (LPARAM)&TvItem);
		nBurnDrv[nTmpDrvCount].nBurnDrvNo = nBurnDrvActive;
		nBurnDrv[nTmpDrvCount].pszROMName = BurnDrvGetTextA(DRV_NAME);
		nBurnDrv[nTmpDrvCount].bIsParent = true;
		nTmpDrvCount++;
	}

	// 2nd: clones
	for (i = nBurnDrvCount-1; i >= 0; i--) {
		TV_INSERTSTRUCT TvItem;

		nBurnDrvActive = i;																// Switch to driver i

		// if showing zip names get active entry from our sorted list
		if (nLoadMenuShowY & SHOWSHORT) nBurnDrvActive = nBurnZipListDrv[nBurnDrvCount - 1 - i].nBurnDrvNo;

		if (BurnDrvGetFlags() & BDF_BOARDROM) {
			continue;
		}

		if (BurnDrvGetTextA(DRV_PARENT) == NULL || !(BurnDrvGetFlags() & BDF_CLONE)) {	// Skip parents
			continue;
		}

		if(!gameAv[nBurnDrvActive]) nMissingDrvCount++;

		UINT64 nHardware = (UINT64)1 << (((UINT32)BurnDrvGetHardwareCode() >> 24) & 0x3f);
		if ((nHardware & MASKALL) && ((nHardware & nLoadMenuShowX) || ((nHardware & MASKALL) == 0))) {
			continue;
		}

		if (avOk && (!(nLoadMenuShowY & UNAVAILABLE)) && !gameAv[nBurnDrvActive]) {						// Skip non-available games if needed
			continue;
		}

		if (avOk && (!(nLoadMenuShowY & AVAILABLE)) && gameAv[nBurnDrvActive]) {						// Skip available games if needed
			continue;
		}

		if (DoExtraFilters()) continue;

		if (szSearchString[0]) {
			TCHAR *StringFound = NULL;
			TCHAR *StringFound1 = NULL;
			TCHAR *StringFound2 = NULL;
			TCHAR *StringFound3 = NULL;
			TCHAR szDriverName[256] = { _T("") };
			TCHAR szDriverNameA[256] = { _T("") };
			TCHAR szManufacturerName[256] = { _T("") };
			wcscpy(szDriverName, BurnDrvGetText(DRV_FULLNAME));
			swprintf(szDriverNameA, _T("%S"), BurnDrvGetTextA(DRV_FULLNAME));
			swprintf(szManufacturerName, _T("%s %s"), BurnDrvGetText(DRV_MANUFACTURER), BurnDrvGetText(DRV_SYSTEM));
			for (int k = 0; k < 256; k++) {
				szDriverName[k] = _totlower(szDriverName[k]);
				szDriverNameA[k] = _totlower(szDriverNameA[k]);
				szManufacturerName[k] = _totlower(szManufacturerName[k]);
			}
			StringFound = wcsstr(szDriverName, szSearchString);
			StringFound1 = wcsstr(szDriverNameA, szSearchString);
			StringFound2 = wcsstr(BurnDrvGetText(DRV_NAME), szSearchString);
			StringFound3 = wcsstr(szManufacturerName, szSearchString);

			if (!StringFound && !StringFound1 && !StringFound2 && !StringFound3) continue;
		}

		memset(&TvItem, 0, sizeof(TvItem));
		TvItem.item.mask = TVIF_TEXT | TVIF_PARAM;
		TvItem.hInsertAfter = TVI_FIRST;
		TvItem.item.pszText = (nLoadMenuShowY & SHOWSHORT) ? BurnDrvGetText(DRV_NAME) : RemoveSpace(BurnDrvGetText(DRV_ASCIIONLY | DRV_FULLNAME));

		// Find the parent's handle
		for (j = 0; j < nTmpDrvCount; j++) {
			if (nBurnDrv[j].bIsParent) {
				if (!strcmp(BurnDrvGetTextA(DRV_PARENT), nBurnDrv[j].pszROMName)) {
					TvItem.hParent = nBurnDrv[j].hTreeHandle;
					break;
				}
			}
		}

		// Find the parent and add a branch to the tree
		if (!TvItem.hParent) {
			char szTempName[32];
			strcpy(szTempName, BurnDrvGetTextA(DRV_PARENT));
			int nTempBurnDrvSelect = nBurnDrvActive;
			for (j = 0; j < nBurnDrvCount; j++) {
				nBurnDrvActive = j;
				if (!strcmp(szTempName, BurnDrvGetTextA(DRV_NAME))) {
					TV_INSERTSTRUCT TempTvItem;
					memset(&TempTvItem, 0, sizeof(TempTvItem));
					TempTvItem.item.mask = TVIF_TEXT | TVIF_PARAM;
					//TempTvItem.hInsertAfter = TVI_FIRST;
					TempTvItem.hInsertAfter = TVI_SORT; // only use the horribly-slow TVI_SORT for missing parents!
					TempTvItem.item.pszText = (nLoadMenuShowY & SHOWSHORT) ? BurnDrvGetText(DRV_NAME) : RemoveSpace(BurnDrvGetText(DRV_ASCIIONLY | DRV_FULLNAME));
					TempTvItem.item.lParam = (LPARAM)&nBurnDrv[nTmpDrvCount];
					nBurnDrv[nTmpDrvCount].hTreeHandle = (HTREEITEM)SendMessage(hSelList, TVM_INSERTITEM, 0, (LPARAM)&TempTvItem);
					nBurnDrv[nTmpDrvCount].nBurnDrvNo = j;
					nBurnDrv[nTmpDrvCount].bIsParent = true;
					nBurnDrv[nTmpDrvCount].pszROMName = BurnDrvGetTextA(DRV_NAME);
					TvItem.item.lParam = (LPARAM)&nBurnDrv[nTmpDrvCount];
					TvItem.hParent = nBurnDrv[nTmpDrvCount].hTreeHandle;
					nTmpDrvCount++;
					break;
				}
			}
			nBurnDrvActive = nTempBurnDrvSelect;
		}

		TvItem.item.lParam = (LPARAM)&nBurnDrv[nTmpDrvCount];
		nBurnDrv[nTmpDrvCount].hTreeHandle = (HTREEITEM)SendMessage(hSelList, TVM_INSERTITEM, 0, (LPARAM)&TvItem);
		nBurnDrv[nTmpDrvCount].pszROMName = BurnDrvGetTextA(DRV_NAME);
		nBurnDrv[nTmpDrvCount].nBurnDrvNo = nBurnDrvActive;
		nTmpDrvCount++;
	}

	for (i = 0; i < nTmpDrvCount; i++) {
		// See if we need to expand the branch of an unavailable or non-working parent
		if (nBurnDrv[i].bIsParent && ((nLoadMenuShowY & AUTOEXPAND) || !gameAv[nBurnDrv[i].nBurnDrvNo] || !CheckWorkingStatus(nBurnDrv[i].nBurnDrvNo))) {
			for (j = 0; j < nTmpDrvCount; j++) {

				// Expand the branch only if a working clone is available -or-
				// If ROM Scanning is disabled (bSkipStartupCheck==1) and expand clones is set. -dink March 22, 2020
				if ( gameAv[nBurnDrv[j].nBurnDrvNo] || (bSkipStartupCheck && (nLoadMenuShowY & AUTOEXPAND)) ) {
					nBurnDrvActive = nBurnDrv[j].nBurnDrvNo;
					if (BurnDrvGetTextA(DRV_PARENT)) {
						if (strcmp(nBurnDrv[i].pszROMName, BurnDrvGetTextA(DRV_PARENT)) == 0) {
							SendMessage(hSelList, TVM_EXPAND, TVE_EXPAND, (LPARAM)nBurnDrv[i].hTreeHandle);
							break;
						}
					}
				}
			}
		}
	}

	// Update the status info
	TCHAR szRomsAvailableInfo[128] = _T("");

	_stprintf(szRomsAvailableInfo, FBALoadStringEx(hAppInst, IDS_SEL_SETSTATUS, true), nTmpDrvCount, nBurnDrvCount - REDUCE_TOTAL_SETS_BIOS, nMissingDrvCount);
	SendDlgItemMessage(hSelDlg, IDC_DRVCOUNT, WM_SETTEXT, 0, (LPARAM)(LPCTSTR)szRomsAvailableInfo);

	return 0;
}

static void MyEndDialog()
{
	if (nTimer) {
		KillTimer(hSelDlg, nTimer);
		nTimer = 0;
	}

	if (nInitPreviewTimer) {
		KillTimer(hSelDlg, nInitPreviewTimer);
		nInitPreviewTimer = 0;
	}

	SendDlgItemMessage(hSelDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
	SendDlgItemMessage(hSelDlg, IDC_SCREENSHOT_V, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);

	SendDlgItemMessage(hSelDlg, IDC_SCREENSHOT2_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
	SendDlgItemMessage(hSelDlg, IDC_SCREENSHOT2_V, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);

	if (hPrevBmp) {
		DeleteObject((HGDIOBJ)hPrevBmp);
		hPrevBmp = NULL;
	}
	if(hTitleBmp) {
		DeleteObject((HGDIOBJ)hTitleBmp);
		hTitleBmp = NULL;
	}

	if (hExpand) {
		DestroyIcon(hExpand);
		hExpand = NULL;
	}
	if (hCollapse) {
		DestroyIcon(hCollapse);
		hCollapse = NULL;
	}
	if (hNotWorking) {
		DestroyIcon(hNotWorking);
		hNotWorking = NULL;
	}
	if (hNotFoundEss) {
		DestroyIcon(hNotFoundEss);
		hNotFoundEss = NULL;
	}
	if (hNotFoundNonEss) {
		DestroyIcon(hNotFoundNonEss);
		hNotFoundNonEss = NULL;
	}
	if(hDrvIconMiss) {
		DestroyIcon(hDrvIconMiss);
		hDrvIconMiss = NULL;
	}

	RECT rect;

	GetWindowRect(hSelDlg, &rect);
	nSelDlgWidth  = rect.right - rect.left;
	nSelDlgHeight = rect.bottom - rect.top;

	if (!bSelOkay) {
		RomDataStateRestore();
	}
	bSelOkay = false;

	EndDialog(hSelDlg, 0);
}

// User clicked ok for a driver in the list
static void SelOkay()
{
	TV_ITEM TvItem;
	unsigned int nSelect = 0;
	HTREEITEM hSelectHandle = (HTREEITEM)SendMessage(hSelList, TVM_GETNEXTITEM, TVGN_CARET, ~0U);

	if (!hSelectHandle)	{			// Nothing is selected, return without closing the window
		return;
	}

	TvItem.hItem = hSelectHandle;
	TvItem.mask = TVIF_PARAM;
	SendMessage(hSelList, TVM_GETITEM, 0, (LPARAM)&TvItem);
	nSelect = ((NODEINFO*)TvItem.lParam)->nBurnDrvNo;

#if DISABLE_NON_AVAILABLE_SELECT
	if (!gameAv[nSelect]) {			// Game not available, return without closing the window
		return;
	}
#endif

#if NON_WORKING_PROMPT_ON_LOAD
	if (!CheckWorkingStatus(nSelect)) {
		TCHAR szWarningText[1024];
		_stprintf(szWarningText, _T("%s"), FBALoadStringEx(hAppInst, IDS_ERR_WARNING, true));
		if (MessageBox(hSelDlg, FBALoadStringEx(hAppInst, IDS_ERR_NON_WORKING, true), szWarningText, MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) == IDNO) {
			return;
		}
	}
#endif
	nDialogSelect = nSelect;
	bSelOkay      = true;			// Non-RomData game will be running soon
	IpsPatchInit();					// Entry point : SelOkay

	bDialogCancel = false;
	MyEndDialog();
}

static void RefreshPanel()
{
	// clear preview shot
	if (hPrevBmp) {
		DeleteObject((HGDIOBJ)hPrevBmp);
		hPrevBmp = NULL;
	}
	if (hTitleBmp) {
		DeleteObject((HGDIOBJ)hTitleBmp);
		hTitleBmp = NULL;
	}
	if (nTimer) {
		KillTimer(hSelDlg, nTimer);
		nTimer = 0;
	}

	GetTitlePreviewScale();

	hPrevBmp  = PNGLoadBitmap(hSelDlg, NULL, _213, _160, 2);
	hTitleBmp = PNGLoadBitmap(hSelDlg, NULL, _213, _160, 2);

	SendDlgItemMessage(hSelDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hPrevBmp);
	SendDlgItemMessage(hSelDlg, IDC_SCREENSHOT_V, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
	ShowWindow(GetDlgItem(hSelDlg, IDC_SCREENSHOT_H), SW_SHOW);

	SendDlgItemMessage(hSelDlg, IDC_SCREENSHOT2_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hTitleBmp);
	SendDlgItemMessage(hSelDlg, IDC_SCREENSHOT2_V, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
	ShowWindow(GetDlgItem(hSelDlg, IDC_SCREENSHOT2_H), SW_SHOW);

	// Clear the things in our Info-box
	for (int i = 0; i < 6; i++) {
		SendMessage(hInfoText[i], WM_SETTEXT, (WPARAM)0, (LPARAM)_T(""));
		EnableWindow(hInfoLabel[i], FALSE);
	}

	CheckDlgButton(hSelDlg, IDC_CHECKAUTOEXPAND,  (nLoadMenuShowY & AUTOEXPAND)  ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hSelDlg, IDC_CHECKAVAILABLE,   (nLoadMenuShowY & AVAILABLE)   ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hSelDlg, IDC_CHECKUNAVAILABLE, (nLoadMenuShowY & UNAVAILABLE) ? BST_CHECKED : BST_UNCHECKED);

	CheckDlgButton(hSelDlg, IDC_SEL_SHORTNAME, nLoadMenuShowY & SHOWSHORT     ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hSelDlg, IDC_SEL_ASCIIONLY, nLoadMenuShowY & ASCIIONLY     ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hSelDlg, IDC_SEL_SUBDIRS,   nLoadMenuShowY & SEARCHSUBDIRS ? BST_CHECKED : BST_UNCHECKED);
}

FILE* OpenPreview(int nIndex, TCHAR *szPath)
{
	static bool bTryParent;

	TCHAR szBaseName[MAX_PATH];
	TCHAR szFileName[MAX_PATH];

	FILE* fp = NULL;

	// Try to load a .PNG preview image
	_sntprintf(szBaseName, sizeof(szBaseName), _T("%s%s"), szPath, BurnDrvGetText(DRV_NAME));
	if (nIndex <= 1) {
		_stprintf(szFileName, _T("%s.png"), szBaseName);
		fp = _tfopen(szFileName, _T("rb"));
	}
	if (!fp) {
		_stprintf(szFileName, _T("%s [%02i].png"), szBaseName, nIndex);
		fp = _tfopen(szFileName, _T("rb"));
	}

	if (nIndex <= 1) {
		bTryParent = fp ? false : true;
	}

	if (!fp && BurnDrvGetText(DRV_PARENT) && bTryParent) {						// Try the parent
		_sntprintf(szBaseName, sizeof(szBaseName), _T("%s%s"), szPath, BurnDrvGetText(DRV_PARENT));
		if (nIndex <= 1) {
			_stprintf(szFileName, _T("%s.png"), szBaseName);
			fp = _tfopen(szFileName, _T("rb"));
		}
		if (!fp) {
			_stprintf(szFileName, _T("%s [%02i].png"), szBaseName, nIndex);
			fp = _tfopen(szFileName, _T("rb"));
		}
	}

	return fp;
}

static VOID CALLBACK PreviewTimerProc(HWND, UINT, UINT_PTR, DWORD)
{
	UpdatePreview(false, szAppPreviewsPath, IDC_SCREENSHOT_H, IDC_SCREENSHOT_V);
}

static VOID CALLBACK InitPreviewTimerProc(HWND, UINT, UINT_PTR, DWORD)
{
	UpdatePreview(true, szAppPreviewsPath, IDC_SCREENSHOT_H, IDC_SCREENSHOT_V);

	if (GetIpsNumPatches()) {
		if (!nShowMVSCartsOnly) {
			EnableWindow(GetDlgItem(hSelDlg, IDC_SEL_IPSMANAGER), TRUE);
			INT32 nActivePatches = LoadIpsActivePatches();

			// Whether IDC_SEL_APPLYIPS is enabled must be subordinate to IDC_SEL_IPSMANAGER
			// to verify that xxx.dat is not removed after saving config.
			// Reduce useless array lookups.
			EnableWindow(GetDlgItem(hSelDlg, IDC_SEL_APPLYIPS), nActivePatches);
		}
	} else {
		EnableWindow(GetDlgItem(hSelDlg, IDC_SEL_IPSMANAGER), FALSE);
		EnableWindow(GetDlgItem(hSelDlg, IDC_SEL_APPLYIPS),   FALSE);	// xxx.dat path not found, must be disabled.
	}

	KillTimer(hSelDlg, nInitPreviewTimer);
	nInitPreviewTimer = 0;
}

static int UpdatePreview(bool bReset, TCHAR *szPath, int HorCtrl, int VerCtrl)
{
	static int nIndex;
	int nOldIndex = 0;

	FILE* fp = NULL;
	HBITMAP hNewImage = NULL;

	if (HorCtrl == IDC_SCREENSHOT_H) {
		nOldIndex = nIndex;
		nIndex++;
		if (bReset) {
			nIndex = 1;
			nOldIndex = -1;
			if (nTimer) {
				KillTimer(hSelDlg, 1);
				nTimer = 0;
			}
		}
	}

	nBurnDrvActive = nDialogSelect;

	if ((nIndex != nOldIndex) || (HorCtrl == IDC_SCREENSHOT2_H)) {
		int x, y, ax, ay;

		BurnDrvGetAspect(&ax, &ay);

		// If a game uses pixel aspect ratio (aspect ratio == pixel size) default to 4:3
		// for Titles and Previews (otherwise the pictures weirdly draw ontop of the UI)
		if (!_tcsncmp(BurnDrvGetText(DRV_NAME), _T("wrally2"), 7) || ax > 100) {
			ax = 4;
			ay = 3;
		}

		//if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		if (ay > ax) {
			bImageOrientation = TRUE;

			y = _160;
			x = y * ax / ay;
		} else {
			bImageOrientation = FALSE;

			x = _213;
			y = x * ay / ax;
		}

		if (HorCtrl == IDC_SCREENSHOT_H) {
			fp = OpenPreview(nIndex, szPath);
		} else {
			fp = OpenPreview(0, szPath);
		}
		if (!fp && nIndex > 1 && HorCtrl == IDC_SCREENSHOT_H) {
			if (nIndex == 2) {

				// There's only a single preview image, stop timer

				if (nTimer) {
					KillTimer(hSelDlg, 1);
					nTimer = 0;
				}

				return 0;
			}

			nIndex = 1;
			fp = OpenPreview(nIndex, szPath);
		}
		if (fp) {
			if (ax > 4) {
				// Check if title/preview image is captured from 1 monitor on a
				// multi-monitor game, then make the proper adjustments so it
				// looks correct.
				IMAGE img;
				INT32 game_x, game_y;

				PNGGetInfo(&img, fp);
				rewind(fp);

				BurnDrvGetFullSize(&game_x, &game_y);

				if (img.width <= game_x / 2) {
					ax = 4;
					ay = 3;
					x = _213;
					y = x * ay / ax;
				}
			}
			hNewImage = PNGLoadBitmap(hSelDlg, fp, x, y, 3);
		}
	}

	if (fp) {
		fclose(fp);

		if (HorCtrl == IDC_SCREENSHOT_H) nTimer = SetTimer(hSelDlg, 1, 2500, PreviewTimerProc);
	} else {

		// We couldn't load a new image for this game, so kill the timer (it will be restarted when a new game is selected)

		if (HorCtrl == IDC_SCREENSHOT_H) {
			if (nTimer) {
				KillTimer(hSelDlg, 1);
				nTimer = 0;
			}
		}

		bImageOrientation = FALSE;
		hNewImage = PNGLoadBitmap(hSelDlg, NULL, _213, _160, 2);
	}

	if (hPrevBmp && (HorCtrl == IDC_SCREENSHOT_H || VerCtrl == IDC_SCREENSHOT_V)) {
		DeleteObject((HGDIOBJ)hPrevBmp);
		*&hPrevBmp = NULL;
		hPrevBmp = hNewImage;
	}

	if (hTitleBmp && (HorCtrl == IDC_SCREENSHOT2_H || VerCtrl == IDC_SCREENSHOT2_V)) {
		DeleteObject((HGDIOBJ)hTitleBmp);
		*&hTitleBmp = NULL;
		hTitleBmp = hNewImage;
	}

	if (bImageOrientation == 0) {
		SendDlgItemMessage(hSelDlg, HorCtrl, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hNewImage);
		SendDlgItemMessage(hSelDlg, VerCtrl, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
		ShowWindow(GetDlgItem(hSelDlg, HorCtrl), SW_SHOW);
		ShowWindow(GetDlgItem(hSelDlg, VerCtrl), SW_HIDE);
	} else {
		SendDlgItemMessage(hSelDlg, HorCtrl, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
		SendDlgItemMessage(hSelDlg, VerCtrl, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hNewImage);
		ShowWindow(GetDlgItem(hSelDlg, HorCtrl), SW_HIDE);
		ShowWindow(GetDlgItem(hSelDlg, VerCtrl), SW_SHOW);
	}

	UpdateWindow(hSelDlg);

	return 0;
}

// Sometimes we have different setnames than other emu's, the table below
// will translate our set to their set to keep hiscore.dat/arcadedb happy
// NOTE: asciiz version repeated in burn/hiscore.cpp

struct game_replace_entry {
	TCHAR fb_name[80];
	TCHAR mame_name[80];
};

static game_replace_entry replace_table[] = {
	{ _T("vsraidbbay"),			_T("bnglngby")		},
	{ _T("vsrbibbal"),			_T("rbibb")			},
	{ _T("vsbattlecity"),		_T("btlecity")		},
	{ _T("vscastlevania"),		_T("cstlevna")		},
	{ _T("vsclucluland"),		_T("cluclu")		},
	{ _T("vsdrmario"),			_T("drmario")		},
	{ _T("vsduckhunt"),			_T("duckhunt")		},
	{ _T("vsexcitebike"),		_T("excitebk")		},
	{ _T("vsfreedomforce"),		_T("vsfdf")			},
	{ _T("vsgoonies"),			_T("goonies")		},
	{ _T("vsgradius"),			_T("vsgradus")		},
	{ _T("vsgumshoe"),			_T("vsgshoe")		},
	{ _T("vshogansalley"),		_T("hogalley")		},
	{ _T("vsiceclimber"),		_T("iceclimb")		},
	{ _T("vsmachrider"),		_T("nvs_machrider")	},
	{ _T("vsmightybomjack"),	_T("nvs_mightybj")	},
	{ _T("vsninjajkun"),		_T("jajamaru")		},
	{ _T("vspinball"),			_T("vspinbal")		},
	{ _T("vsplatoon"),			_T("nvs_platoon")	},
	{ _T("vsslalom"),			_T("vsslalom")		},
	{ _T("vssoccer"),			_T("vssoccer")		},
	{ _T("vsstarluster"),		_T("starlstr")		},
	{ _T("vssmgolf"),			_T("smgolf")		},
	{ _T("vssmgolfla"),			_T("ladygolf")		},
	{ _T("vssmb"),				_T("suprmrio")		},
	{ _T("vssuperskykid"),		_T("vsskykid")		},
	{ _T("vssuperxevious"),		_T("supxevs")		},
	{ _T("vstetris"),			_T("vstetris")		},
	{ _T("vstkoboxing"),		_T("tkoboxng")		},
	{ _T("vstopgun"),			_T("topgun")		},
	{ _T("\0"), 				_T("\0")			}
};

static TCHAR *fbn_to_mame(TCHAR *name)
{
	TCHAR *game = name; // default to passed name

	// Check the above table to see if we should use an alias
	for (INT32 i = 0; replace_table[i].fb_name[0] != '\0'; i++) {
		if (!_tcscmp(replace_table[i].fb_name, name)) {
			game = replace_table[i].mame_name;
			break;
		}
	}

	return game;
}

static unsigned __stdcall DoShellExThread(void *arg)
{
	ShellExecute(NULL, _T("open"), (TCHAR*)arg, NULL, NULL, SW_SHOWNORMAL);

	return 0;
}

static void ViewEmma()
{
	HANDLE hThread = NULL;
	unsigned ThreadID = 0;
	static TCHAR szShellExURL[MAX_PATH];

	_stprintf(szShellExURL, _T("http://adb.arcadeitalia.net/dettaglio_mame.php?game_name=%s"), fbn_to_mame(BurnDrvGetText(DRV_NAME)));

	hThread = (HANDLE)_beginthreadex(NULL, 0, DoShellExThread, (void*)szShellExURL, 0, &ThreadID);
}

static void RebuildEverything()
{
	RefreshPanel();

	bDrvSelected = false;

	TreeBuilding = 1;
	SendMessage(hSelList, WM_SETREDRAW, (WPARAM)FALSE,(LPARAM)TVI_ROOT);	// disable redraw
	SendMessage(hSelList, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);				// Destory all nodes
	SelListMake();
	SendMessage(hSelList, WM_SETREDRAW, (WPARAM)TRUE, (LPARAM)TVI_ROOT);	// enable redraw

	// Clear the things in our Info-box
	for (int i = 0; i < 6; i++) {
		SendMessage(hInfoText[i], WM_SETTEXT, (WPARAM)0, (LPARAM)_T(""));
		EnableWindow(hInfoLabel[i], FALSE);
	}

	TreeBuilding = 0;
}

static int TvhFilterToBitmask(HTREEITEM hHandle)
{
	if (hHandle == hRoot)				return (1 << 0);
	if (hHandle == hBoardType)			return (1 << 1);
	if (hHandle == hFamily)				return (1 << 2);
	if (hHandle == hGenre)				return (1 << 3);
	if (hHandle == hHardware)			return (1 << 4);
	if (hHandle == hFilterCapcomGrp)	return (1 << 5);
	if (hHandle == hFilterSegaGrp)		return (1 << 6);

	return 0;
}

static HTREEITEM TvBitTohFilter(int nBit)
{
	switch (nBit) {
		case (1 << 0): return hRoot;
		case (1 << 1): return hBoardType;
		case (1 << 2): return hFamily;
		case (1 << 3): return hGenre;
		case (1 << 4): return hHardware;
		case (1 << 5): return hFilterCapcomGrp;
		case (1 << 6): return hFilterSegaGrp;
	}

	return 0;
}

#define _TVCreateFiltersA(a, b, c, d)								\
{																	\
	TvItem.hParent = a;												\
	TvItem.item.pszText = FBALoadStringEx(hAppInst, b, true);		\
	c = TreeView_InsertItem(hFilterList, &TvItem);					\
	_TreeView_SetCheckState(hFilterList, c, (d) ? FALSE : TRUE);	\
}

#define _TVCreateFiltersB(a, b, c)								\
{																\
	TvItem.hParent = a;												\
	TvItem.item.pszText = FBALoadStringEx(hAppInst, b, true);		\
	c = TreeView_InsertItem(hFilterList, &TvItem);					\
}

#define _TVCreateFiltersC(a, b, c, d)								\
{																	\
	TvItem.hParent = a;												\
	TvItem.item.pszText = FBALoadStringEx(hAppInst, b, true);		\
	c = TreeView_InsertItem(hFilterList, &TvItem);					\
	_TreeView_SetCheckState(hFilterList, c, (d) ? FALSE : TRUE);	\
}

#define _TVCreateFiltersD(a, b, c, d)								\
{																	\
	TvItem.hParent = a;												\
	TvItem.item.pszText = FBALoadStringEx(hAppInst, b, true);		\
	c = TreeView_InsertItem(hFilterList, &TvItem);					\
	_TreeView_SetCheckState(hFilterList, c, (d) ? FALSE : TRUE);	\
}

static void CreateFilters()
{
	TV_INSERTSTRUCT TvItem;
	memset(&TvItem, 0, sizeof(TvItem));

	hFilterList			= GetDlgItem(hSelDlg, IDC_TREE2);

	TvItem.item.mask	= TVIF_TEXT | TVIF_PARAM;
	TvItem.hInsertAfter = TVI_LAST;

	_TVCreateFiltersA(TVI_ROOT		, IDS_FAVORITES			, hFavorites            , !nLoadMenuFavoritesFilter                         );

	_TVCreateFiltersB(TVI_ROOT		, IDS_SEL_FILTERS		, hRoot						);

	_TVCreateFiltersC(hRoot			, IDS_SEL_BOARDTYPE		, hBoardType			, nLoadMenuBoardTypeFilter & MASKALLBOARD	);

	_TVCreateFiltersA(hBoardType	, IDS_SEL_GENUINE		, hFilterGenuine		, nLoadMenuBoardTypeFilter & MASKBOARDTYPEGENUINE	);
	_TVCreateFiltersA(hBoardType	, IDS_SEL_BOOTLEG		, hFilterBootleg		, nLoadMenuBoardTypeFilter & BDF_BOOTLEG			);
	_TVCreateFiltersA(hBoardType	, IDS_SEL_DEMO			, hFilterDemo			, nLoadMenuBoardTypeFilter & BDF_DEMO				);
	_TVCreateFiltersA(hBoardType	, IDS_SEL_HACK			, hFilterHack			, nLoadMenuBoardTypeFilter & BDF_HACK				);
	_TVCreateFiltersA(hBoardType	, IDS_SEL_HOMEBREW		, hFilterHomebrew		, nLoadMenuBoardTypeFilter & BDF_HOMEBREW			);
	_TVCreateFiltersA(hBoardType	, IDS_SEL_PROTOTYPE		, hFilterPrototype		, nLoadMenuBoardTypeFilter & BDF_PROTOTYPE			);

	_TVCreateFiltersC(hRoot			, IDS_FAMILY			, hFamily				, nLoadMenuFamilyFilter & MASKALLFAMILY	);

	_TVCreateFiltersA(hFamily		, IDS_FAMILY_19XX		, hFilter19xx			, nLoadMenuFamilyFilter & FBF_19XX					);
	_TVCreateFiltersA(hFamily		, IDS_FAMILY_DONPACHI	, hFilterDonpachi		, nLoadMenuFamilyFilter & FBF_DONPACHI				);
	_TVCreateFiltersA(hFamily		, IDS_FAMILY_SONICWI	, hFilterSonicwi		, nLoadMenuFamilyFilter & FBF_SONICWI				);
	_TVCreateFiltersA(hFamily		, IDS_FAMILY_DSTLK		, hFilterDstlk			, nLoadMenuFamilyFilter & FBF_DSTLK					);
	_TVCreateFiltersA(hFamily		, IDS_FAMILY_FATFURY	, hFilterFatfury		, nLoadMenuFamilyFilter & FBF_FATFURY				);
	_TVCreateFiltersA(hFamily		, IDS_FAMILY_KOF		, hFilterKof			, nLoadMenuFamilyFilter & FBF_KOF					);
	_TVCreateFiltersA(hFamily		, IDS_FAMILY_MSLUG		, hFilterMslug			, nLoadMenuFamilyFilter & FBF_MSLUG					);
	_TVCreateFiltersA(hFamily		, IDS_FAMILY_MAHOU		, hFilterMahou			, nLoadMenuFamilyFilter & FBF_MAHOU					);
	_TVCreateFiltersA(hFamily		, IDS_FAMILY_PWRINST	, hFilterPwrinst		, nLoadMenuFamilyFilter & FBF_PWRINST				);
	_TVCreateFiltersA(hFamily		, IDS_FAMILY_SAMSHO		, hFilterSamsho			, nLoadMenuFamilyFilter & FBF_SAMSHO				);
	_TVCreateFiltersA(hFamily		, IDS_FAMILY_SONIC		, hFilterSonic			, nLoadMenuFamilyFilter & FBF_SONIC					);
	_TVCreateFiltersA(hFamily		, IDS_FAMILY_SF			, hFilterSf				, nLoadMenuFamilyFilter & FBF_SF					);
	_TVCreateFiltersA(hFamily		, IDS_FAMILY_OTHER		, hFilterOtherFamily	, nLoadMenuFamilyFilter & MASKFAMILYOTHER			);

	_TVCreateFiltersC(hRoot			, IDS_GENRE				, hGenre				, nLoadMenuGenreFilter & MASKALLGENRE	);

	_TVCreateFiltersA(hGenre		, IDS_GENRE_ACTION		, hFilterAction			, nLoadMenuGenreFilter & GBF_ACTION					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_BALLPADDLE	, hFilterBallpaddle		, nLoadMenuGenreFilter & GBF_BALLPADDLE				);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_BREAKOUT	, hFilterBreakout		, nLoadMenuGenreFilter & GBF_BREAKOUT				);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_CASINO		, hFilterCasino			, nLoadMenuGenreFilter & GBF_CASINO					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_SCRFIGHT	, hFilterScrfight		, nLoadMenuGenreFilter & GBF_SCRFIGHT				);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_VSFIGHT		, hFilterVsfight		, nLoadMenuGenreFilter & GBF_VSFIGHT				);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_MAHJONG		, hFilterMahjong		, nLoadMenuGenreFilter & GBF_MAHJONG				);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_MAZE		, hFilterMaze			, nLoadMenuGenreFilter & GBF_MAZE					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_MINIGAMES	, hFilterMinigames		, nLoadMenuGenreFilter & GBF_MINIGAMES				);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_MISC		, hFilterMisc			, nLoadMenuGenreFilter & GBF_MISC					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_PINBALL		, hFilterPinball		, nLoadMenuGenreFilter & GBF_PINBALL				);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_PLATFORM	, hFilterPlatform		, nLoadMenuGenreFilter & GBF_PLATFORM				);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_PUZZLE		, hFilterPuzzle			, nLoadMenuGenreFilter & GBF_PUZZLE					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_QUIZ		, hFilterQuiz			, nLoadMenuGenreFilter & GBF_QUIZ					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_RACING		, hFilterRacing			, nLoadMenuGenreFilter & GBF_RACING					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_RUNGUN		, hFilterRungun			, nLoadMenuGenreFilter & GBF_RUNGUN 				);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_SHOOT		, hFilterShoot			, nLoadMenuGenreFilter & GBF_SHOOT					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_HORSHOOT	, hFilterHorshoot		, nLoadMenuGenreFilter & GBF_HORSHOOT				);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_VERSHOOT	, hFilterVershoot		, nLoadMenuGenreFilter & GBF_VERSHOOT				);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_SPORTSMISC	, hFilterSportsmisc		, nLoadMenuGenreFilter & GBF_SPORTSMISC				);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_SPORTSFOOTBALL, hFilterSportsfootball, nLoadMenuGenreFilter & GBF_SPORTSFOOTBALL		);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_STRATEGY	, hFilterStrategy   	, nLoadMenuGenreFilter & GBF_STRATEGY   			);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_RPG			, hFilterRpg   			, nLoadMenuGenreFilter & GBF_RPG   					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_SIM			, hFilterSim   			, nLoadMenuGenreFilter & GBF_SIM   					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_ADV			, hFilterAdv   			, nLoadMenuGenreFilter & GBF_ADV   					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_CARD		, hFilterCard   		, nLoadMenuGenreFilter & GBF_CARD					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_BOARD		, hFilterBoard   		, nLoadMenuGenreFilter & GBF_BOARD					);
	_TVCreateFiltersA(hGenre		, IDS_GENRE_BIOS		, hFilterBios			, nLoadMenuGenreFilter & GBF_BIOS					);

	_TVCreateFiltersC(hRoot			, IDS_SEL_HARDWARE		, hHardware				, nLoadMenuShowX & MASKALL					);

	_TVCreateFiltersD(hHardware		, IDS_SEL_CAPCOM_GRP	, hFilterCapcomGrp			, nLoadMenuShowX & MASKCAPGRP				);

	_TVCreateFiltersA(hFilterCapcomGrp	, IDS_SEL_CPS1			, hFilterCps1			, nLoadMenuShowX & MASKCPS							);
	_TVCreateFiltersA(hFilterCapcomGrp	, IDS_SEL_CPS2			, hFilterCps2			, nLoadMenuShowX & MASKCPS2							);
	_TVCreateFiltersA(hFilterCapcomGrp	, IDS_SEL_CPS3			, hFilterCps3			, nLoadMenuShowX & MASKCPS3							);
	_TVCreateFiltersA(hFilterCapcomGrp	, IDS_SEL_CAPCOM_MISC	, hFilterCapcomMisc		, nLoadMenuShowX & MASKCAPMISC						);

	_TVCreateFiltersA(hHardware		, IDS_SEL_CAVE			, hFilterCave			, nLoadMenuShowX & MASKCAVE							);
	_TVCreateFiltersA(hHardware		, IDS_SEL_DATAEAST		, hFilterDataeast		, nLoadMenuShowX & MASKDATAEAST						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_GALAXIAN		, hFilterGalaxian		, nLoadMenuShowX & MASKGALAXIAN						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_IREM			, hFilterIrem			, nLoadMenuShowX & MASKIREM							);
	_TVCreateFiltersA(hHardware		, IDS_SEL_KANEKO		, hFilterKaneko			, nLoadMenuShowX & MASKKANEKO						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_KONAMI		, hFilterKonami			, nLoadMenuShowX & MASKKONAMI						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_MIDWAY		, hFilterMidway			, nLoadMenuShowX & MASKMIDWAY						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_NEOGEO		, hFilterNeogeo			, nLoadMenuShowX & MASKNEOGEO						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_PACMAN		, hFilterPacman			, nLoadMenuShowX & MASKPACMAN						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_PGM			, hFilterPgm			, nLoadMenuShowX & MASKPGM							);
	_TVCreateFiltersA(hHardware		, IDS_SEL_PSIKYO		, hFilterPsikyo			, nLoadMenuShowX & MASKPSIKYO						);

	_TVCreateFiltersD(hHardware		, IDS_SEL_SEGA_GRP		, hFilterSegaGrp				, nLoadMenuShowX & MASKSEGAGRP				);

	_TVCreateFiltersA(hFilterSegaGrp	, IDS_SEL_SG1000		, hFilterSg1000			, nLoadMenuShowX & MASKSG1000						);
	_TVCreateFiltersA(hFilterSegaGrp	, IDS_SEL_SMS			, hFilterSms			, nLoadMenuShowX & MASKSMS							);
	_TVCreateFiltersA(hFilterSegaGrp	, IDS_SEL_MEGADRIVE		, hFilterMegadrive		, nLoadMenuShowX & MASKMEGADRIVE					);
	_TVCreateFiltersA(hFilterSegaGrp	, IDS_SEL_GG			, hFilterGg				, nLoadMenuShowX & MASKGG							);
	_TVCreateFiltersA(hFilterSegaGrp	, IDS_SEL_SEGA			, hFilterSega			, nLoadMenuShowX & MASKSEGA							);

	_TVCreateFiltersA(hHardware		, IDS_SEL_SETA			, hFilterSeta			, nLoadMenuShowX & MASKSETA							);
	_TVCreateFiltersA(hHardware		, IDS_SEL_TAITO			, hFilterTaito			, nLoadMenuShowX & MASKTAITO						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_TECHNOS		, hFilterTechnos		, nLoadMenuShowX & MASKTECHNOS						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_TOAPLAN		, hFilterToaplan		, nLoadMenuShowX & MASKTOAPLAN						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_PCE			, hFilterPce			, nLoadMenuShowX & MASKPCENGINE						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_COLECO		, hFilterColeco			, nLoadMenuShowX & MASKCOLECO						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_MSX			, hFilterMsx			, nLoadMenuShowX & MASKMSX							);
	_TVCreateFiltersA(hHardware		, IDS_SEL_SPECTRUM		, hFilterSpectrum		, nLoadMenuShowX & MASKSPECTRUM						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_NES			, hFilterNes			, nLoadMenuShowX & MASKNES							);
	_TVCreateFiltersA(hHardware		, IDS_SEL_FDS			, hFilterFds			, nLoadMenuShowX & MASKFDS							);
	_TVCreateFiltersA(hHardware		, IDS_SEL_SNES			, hFilterSnes			, nLoadMenuShowX & MASKSNES							);
	_TVCreateFiltersA(hHardware		, IDS_SEL_NGP			, hFilterNgp			, nLoadMenuShowX & MASKNGP							);
	_TVCreateFiltersA(hHardware		, IDS_SEL_CHANNELF		, hFilterChannelF		, nLoadMenuShowX & MASKCHANNELF						);
	_TVCreateFiltersA(hHardware		, IDS_SEL_MISCPRE90S	, hFilterMiscPre90s		, nLoadMenuShowX & MASKMISCPRE90S					);
	_TVCreateFiltersA(hHardware		, IDS_SEL_MISCPOST90S	, hFilterMiscPost90s	, nLoadMenuShowX & MASKMISCPOST90S					);

	// restore expanded filter nodes
	for (INT32 i = 0; i < 16; i++)
	{
		if (nLoadMenuExpand & (1 << i))
			SendMessage(hFilterList, TVM_EXPAND, TVE_EXPAND, (LPARAM)TvBitTohFilter(1 << i));
	}
	//SendMessage(hFilterList	, TVM_EXPAND,TVE_EXPAND, (LPARAM)hRoot);
	//SendMessage(hFilterList	, TVM_EXPAND,TVE_EXPAND, (LPARAM)hHardware);
	//SendMessage(hFilterList	, TVM_EXPAND,TVE_EXPAND, (LPARAM)hFavorites);
	TreeView_SelectSetFirstVisible(hFilterList, hFavorites);
}

enum {
	ICON_MEGADRIVE,
	ICON_PCE,
	ICON_SGX,
	ICON_TG16,
	ICON_SG1000,
	ICON_COLECO,
	ICON_SMS,
	ICON_GG,
	ICON_MSX,
	ICON_SPECTRUM,
	ICON_NES,
	ICON_FDS,
	ICON_SNES,
	ICON_NGPC,
	ICON_NGP,
	ICON_CHANNELF,
	ICON_ENUMEND	// arcade
};

static HWND hIconDlg      = NULL;
static HANDLE hICThread   = NULL;	// IconsCache
static HANDLE hICEvent    = NULL;

static CRITICAL_SECTION cs;

static INT32 xClick, yClick;

static UINT32 __stdcall CacheDrvIconsProc(void* lpParam)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	HICON* pCache = (HICON*)lpParam;
	TCHAR szIcon[MAX_PATH] = { 0 };

	switch (nIconsSize) {
		case ICON_16x16: nIconsSizeXY = 16;	nIconsYDiff =  2;	break;
		case ICON_24x24: nIconsSizeXY = 24;	nIconsYDiff =  6;	break;
		case ICON_32x32: nIconsSizeXY = 32;	nIconsYDiff = 10;	break;
	}

	const UINT32 nDrvCount = nBurnDrvCount;
	const UINT32 nAllCount = nDrvCount + ICON_ENUMEND + 1;

	for (UINT32 nDrvIndex = 0; nDrvIndex < nAllCount; nDrvIndex++) {
		// See if we need to abort
		if (WaitForSingleObject(hICEvent, 0) == WAIT_OBJECT_0) {
			ExitThread(0);
		}

		SendDlgItemMessage(hIconDlg, IDC_WAIT_PROG, PBM_STEPIT, 0, 0);

		// By games
		if (nDrvIndex < nDrvCount) {
			// Occasional anomaly in debugging, suspected resource contention
			EnterCriticalSection(&cs);
			const UINT32 nBackup  = nBurnDrvActive;

			// Prevents nBurnDrvActive from being modified externally under certain circumstances
			nBurnDrvActive        = nDrvIndex;

			const INT32 nFlag     = BurnDrvGetFlags();
			const char* pszParent = BurnDrvGetTextA(DRV_PARENT);
			const TCHAR* pszName  = BurnDrvGetText(DRV_NAME);

			// Now we can safely restore the data (if modified)
			nBurnDrvActive        = nBackup;
			LeaveCriticalSection(&cs);

			// GDI limits the number of objects and does not cache Clone.
			if ((NULL != pszParent) && (nFlag & BDF_CLONE)) {
				pCache[nDrvIndex] = NULL; continue;
			}

			_stprintf(szIcon, _T("%s%s.ico"), szAppIconsPath, pszName);
			pCache[nDrvIndex] = (HICON)LoadImage(NULL, szIcon, IMAGE_ICON, nIconsSizeXY, nIconsSizeXY, LR_LOADFROMFILE | LR_SHARED);
		}
		// By hardware
		// The start of the hardwares icon is immediately after the end of the games icon
		else {
			const TCHAR szConsIcon[ICON_ENUMEND + 1][20] = {
				_T("icon_md"),
				_T("icon_pce"),
				_T("icon_sgx"),
				_T("icon_tg"),
				_T("icon_sg1k"),
				_T("icon_cv"),
				_T("icon_sms"),
				_T("icon_gg"),
				_T("icon_msx"),
				_T("icon_spec"),
				_T("icon_nes"),
				_T("icon_fds"),
				_T("icon_snes"),
				_T("icon_ngpc"),
				_T("icon_ngp"),
				_T("icon_chf"),
				_T("icon_arc")
			};

			const INT32 nConsIndex = nDrvIndex - nDrvCount;

			_stprintf(szIcon, _T("%s%s.ico"), szAppIconsPath, szConsIcon[nConsIndex]);
			pCache[nDrvIndex] = (HICON)LoadImage(NULL, szIcon, IMAGE_ICON, nIconsSizeXY, nIconsSizeXY, LR_LOADFROMFILE | LR_SHARED);
		}
	}

	PostMessage(hIconDlg, WM_CLOSE, 0, 0);
	return 0;
}

static void IconsCacheThreadExit()
{
	DWORD dwExitCode = 0;
	GetExitCodeThread(hICThread, &dwExitCode);

	if (dwExitCode == STILL_ACTIVE) {

		// Signal the scan thread to abort
		SetEvent(hICEvent);

		// Wait for the thread to finish
		if (WaitForSingleObject(hICThread, 10000) != WAIT_OBJECT_0) {
			// If the thread doesn't finish within 10 seconds, forcibly kill it
			TerminateThread(hICThread, 1);
		}
	}

	DeleteCriticalSection(&cs);
	CloseHandle(hICThread); hICThread = NULL;
	CloseHandle(hICEvent);  hICEvent  = NULL;
	dwExitCode = 0;
}

static INT_PTR CALLBACK CacheDrvIconsWaitProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)		// LPARAM lParam
{
	switch (Msg) {
		case WM_INITDIALOG: {
			hIconDlg = hDlg;
			SendDlgItemMessage(hDlg, IDC_WAIT_PROG, PBM_SETRANGE, 0, MAKELPARAM(0, nBurnDrvCount + ICON_ENUMEND + 1));
			SendDlgItemMessage(hDlg, IDC_WAIT_PROG, PBM_SETSTEP, (WPARAM)1, 0);

			ShowWindow( GetDlgItem(hDlg, IDC_WAIT_LABEL_A), TRUE);
			SendMessage(GetDlgItem(hDlg, IDC_WAIT_LABEL_A), WM_SETTEXT, (WPARAM)0, (LPARAM)FBALoadStringEx(hAppInst, IDS_CACHING_ICONS, true));
			ShowWindow( GetDlgItem(hDlg, IDCANCEL),         TRUE);

			hICThread = (HANDLE)_beginthreadex(NULL, 0, CacheDrvIconsProc, pIconsCache, 0, NULL);
			hICEvent  = CreateEvent(NULL, TRUE, FALSE, NULL);

			WndInMid(hDlg, hParent);
			SetFocus(hDlg);	// Enable Esc=close
			break;
		}

		case WM_LBUTTONDOWN: {
			SetCapture(hDlg);

			xClick = GET_X_LPARAM(lParam);
			yClick = GET_Y_LPARAM(lParam);
			break;
		}

		case WM_LBUTTONUP: {
			ReleaseCapture();
			break;
		}

		case WM_MOUSEMOVE: {
			if (GetCapture() == hDlg) {
				RECT rcWindow;
				GetWindowRect(hDlg, &rcWindow);

				INT32 xMouse = GET_X_LPARAM(lParam);
				INT32 yMouse = GET_Y_LPARAM(lParam);
				INT32 xWindow = rcWindow.left + xMouse - xClick;
				INT32 yWindow = rcWindow.top  + yMouse - yClick;

				SetWindowPos(hDlg, NULL, xWindow, yWindow, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
			}
			break;
		}

		case WM_COMMAND: {
			if (LOWORD(wParam) == IDCANCEL) {
				PostMessage(hDlg, WM_CLOSE, 0, 0);
			}
			break;
		}

		case WM_CLOSE: {
			IconsCacheThreadExit();
			EndDialog(hDlg, 0);
			hIconDlg = hParent = NULL;
			LoadDrvIcons();
		}
	}

	return 0;
}

void DestroyDrvIconsCache()
{
	if (NULL == pIconsCache) return;

	for (UINT32 i = 0; i < (nBurnDrvCount + ICON_ENUMEND + 1); i++) {
		if (NULL == pIconsCache[i]) continue;	// LoadImage failed and returned NULL.
		DestroyIcon(pIconsCache[i]);
	}
	free(pIconsCache); pIconsCache = NULL;
	nIconsSizeXY = 16; nIconsYDiff = 2;
}

void CreateDrvIconsCache()
{
	if (!bEnableIcons) {
//		nIconsSize   = ICON_16x16;
		nIconsSizeXY = 16;
		nIconsYDiff  = 2;
		return;
	}

	if (NULL != pIconsCache) DestroyDrvIconsCache();
	pIconsCache = (HICON*)malloc((nBurnDrvCount + ICON_ENUMEND + 1) * sizeof(HICON));

	InitializeCriticalSection(&cs);
	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_WAIT), hParent, (DLGPROC)CacheDrvIconsWaitProc);
}

void LoadDrvIcons()
{
	if (!bEnableIcons) return;

	bIconsLoaded = 0;

	if (NULL == hDrvIcon) {
		hDrvIcon = (HICON*)malloc((nBurnDrvCount + ICON_ENUMEND + 1) * sizeof(HICON));
	}

	const UINT32 nDrvCount = nBurnDrvCount;

	for (UINT32 nDrvIndex = 0; nDrvIndex < nDrvCount; nDrvIndex++) {
		const UINT32 nBackup = nBurnDrvActive;
		nBurnDrvActive       = nDrvIndex;
		const INT32 nFlag    = BurnDrvGetFlags();
		const INT32 nCode    = BurnDrvGetHardwareCode();
		const TCHAR* pszName = BurnDrvGetText(DRV_NAME);
		char* pszParent      = BurnDrvGetTextA(DRV_PARENT);
		nBurnDrvActive       = nBackup;

		// Skip Clone when only the parent item is selected nBurnDrvCount + ICON_ENUMEND
		if (bIconsOnlyParents && (NULL != pszParent) && (nFlag & BDF_CLONE)) {
			hDrvIcon[nDrvIndex] = NULL;											continue;
		}

		// By hardwares
		if (bIconsByHardwares) {
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_MEGADRIVE) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_MEGADRIVE];	continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_PCENGINE_PCENGINE) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_PCE];		continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_PCENGINE_TG16) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_TG16];		continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_PCENGINE_SGX) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_SGX];		continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SG1000) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_SG1000];		continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_COLECO) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_COLECO];		continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_MASTER_SYSTEM) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_SMS];		continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_GAME_GEAR) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_GG];			continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_MSX) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_MSX];		continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SPECTRUM) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_SPECTRUM];	continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_NES) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_NES];		continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_FDS) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_FDS];		continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SNES) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_SNES];		continue;
			}
			else
			if ((nCode & HARDWARE_SNK_NGPC)    == HARDWARE_SNK_NGPC) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_NGPC];		continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_SNK_NGP) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_NGP];		continue;
			}
			else
			if ((nCode & HARDWARE_PUBLIC_MASK) == HARDWARE_CHANNELF) {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_CHANNELF];	continue;
			}
			else {
				hDrvIcon[nDrvIndex] = pIconsCache[nDrvCount + ICON_ENUMEND];	continue;
			}
		}
		// By games
		else {
			// When allowed and Clone is checked, loads the icon of the parent item when checking that the icon file does not exist
			if ((NULL != pszParent) && (nFlag & BDF_CLONE)) {
				TCHAR szIcon[MAX_PATH] = { 0 };
				_stprintf(szIcon, _T("%s%s.ico"), szAppIconsPath, pszName);

				// The icon file exists, and given the GDI cap, now is not the time to deal with it
				if (GetFileAttributes(szIcon) != INVALID_FILE_ATTRIBUTES) {
					// Must be NULL or it will be recognized as having an icon and ignored in message processing
					hDrvIcon[nDrvIndex] = NULL;									continue;
				}
				INT32 nParentDrv = BurnDrvGetIndex(pszParent);

				// Clone icon file does not exist, use parent item icon
				// Icons are reused and do not take up GDI resources
				hDrvIcon[nDrvIndex] = pIconsCache[nParentDrv];					continue;
			}
			// Associate all non-Clone icons
			hDrvIcon[nDrvIndex] = pIconsCache[nDrvIndex];
		}
	}

	bIconsLoaded = 1;
}

void UnloadDrvIcons()
{
	free(hDrvIcon); hDrvIcon = NULL;
}

#define UM_CHECKSTATECHANGE (WM_USER + 100)
#define UM_CLOSE			(WM_USER + 101)

#define _ToggleGameListing(nShowX, nMASK)													\
{																							\
	nShowX ^= nMASK;																		\
	_TreeView_SetCheckState(hFilterList, hItemChanged, (nShowX & nMASK) ? FALSE : TRUE);	\
}

static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_INITDIALOG) {
		hSelDlg = hDlg;

		// add WS_MAXIMIZEBOX button;
		SetWindowLongPtr(hSelDlg, GWL_STYLE, GetWindowLongPtr(hSelDlg, GWL_STYLE) | WS_MAXIMIZEBOX);

		SendDlgItemMessage(hDlg, IDC_SCREENSHOT_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
		SendDlgItemMessage(hDlg, IDC_SCREENSHOT_V, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);

		SendDlgItemMessage(hDlg, IDC_SCREENSHOT2_H, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);
		SendDlgItemMessage(hDlg, IDC_SCREENSHOT2_V, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)NULL);

		hWhiteBGBrush	= CreateSolidBrush(RGB(0xFF,0xFF,0xFF));

		hExpand			= (HICON)LoadImage(hAppInst, MAKEINTRESOURCE(IDI_TV_PLUS),			IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
		hCollapse		= (HICON)LoadImage(hAppInst, MAKEINTRESOURCE(IDI_TV_MINUS),			IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

		hNotWorking		= (HICON)LoadImage(hAppInst, MAKEINTRESOURCE(IDI_TV_NOTWORKING),	IMAGE_ICON, nIconsSizeXY, nIconsSizeXY, LR_DEFAULTCOLOR);
		hNotFoundEss	= (HICON)LoadImage(hAppInst, MAKEINTRESOURCE(IDI_TV_NOTFOUND_ESS),	IMAGE_ICON, nIconsSizeXY, nIconsSizeXY, LR_DEFAULTCOLOR);
		hNotFoundNonEss = (HICON)LoadImage(hAppInst, MAKEINTRESOURCE(IDI_TV_NOTFOUND_NON),	IMAGE_ICON, nIconsSizeXY, nIconsSizeXY, LR_DEFAULTCOLOR);

		hDrvIconMiss	= (HICON)LoadImage(hAppInst, MAKEINTRESOURCE(IDI_APP),	IMAGE_ICON, nIconsSizeXY, nIconsSizeXY, LR_DEFAULTCOLOR);

		TCHAR szOldTitle[1024] = _T(""), szNewTitle[1024] = _T("");
		GetWindowText(hSelDlg, szOldTitle, 1024);
		_sntprintf(szNewTitle, 1024, _T(APP_TITLE) _T(SEPERATOR_1) _T("%s"), szOldTitle);
		SetWindowText(hSelDlg, szNewTitle);

		hSelList		= GetDlgItem(hSelDlg, IDC_TREE1);

		hInfoLabel[0]	= GetDlgItem(hSelDlg, IDC_LABELROMNAME);
		hInfoLabel[1]	= GetDlgItem(hSelDlg, IDC_LABELROMINFO);
		hInfoLabel[2]	= GetDlgItem(hSelDlg, IDC_LABELSYSTEM);
		hInfoLabel[3]	= GetDlgItem(hSelDlg, IDC_LABELCOMMENT);
		hInfoLabel[4]	= GetDlgItem(hSelDlg, IDC_LABELNOTES);
		hInfoLabel[5]	= GetDlgItem(hSelDlg, IDC_LABELGENRE);
		hInfoText[0]	= GetDlgItem(hSelDlg, IDC_TEXTROMNAME);
		hInfoText[1]	= GetDlgItem(hSelDlg, IDC_TEXTROMINFO);
		hInfoText[2]	= GetDlgItem(hSelDlg, IDC_TEXTSYSTEM);
		hInfoText[3]	= GetDlgItem(hSelDlg, IDC_TEXTCOMMENT);
		hInfoText[4]	= GetDlgItem(hSelDlg, IDC_TEXTNOTES);
		hInfoText[5]	= GetDlgItem(hSelDlg, IDC_TEXTGENRE);

#if !defined _UNICODE
		EnableWindow(GetDlgItem(hDlg, IDC_SEL_ASCIIONLY), FALSE);
#endif

		bSearchStringInit = true; // so we don't set off the search timer during init w/SetDlgItemText() below
		SetDlgItemText(hSelDlg, IDC_SEL_SEARCH, szSearchString); // Re-populate the search text

		bool bFoundROMs = false;
		for (unsigned int i = 0; i < nBurnDrvCount; i++) {
			if (gameAv[i]) {
				bFoundROMs = true;
				break;
			}
		}
		if (!bFoundROMs && bSkipStartupCheck == false) {
			RomsDirCreate(hSelDlg);
		}

		// Icons size related -----------------------------------------
		SHORT cyItem = nIconsSizeXY + 4;								// height (in pixels) of each item on the TreeView list
		TreeView_SetItemHeight(hSelList, cyItem);

		SetFocus(hSelList);
		RebuildEverything();

		TreeView_SetItemHeight(hSelList, cyItem);

		if (nDialogSelect == -1) nDialogSelect = nOldDlgSelected;
		if (nDialogSelect > -1) {
			for (unsigned int i = 0; i < nTmpDrvCount; i++) {
				if (nBurnDrv[i].nBurnDrvNo == nDialogSelect) {
					nBurnDrvActive	= nBurnDrv[i].nBurnDrvNo;
					TreeView_EnsureVisible(hSelList, nBurnDrv[i].hTreeHandle);
					TreeView_Select(hSelList, nBurnDrv[i].hTreeHandle, TVGN_CARET);
					TreeView_SelectSetFirstVisible(hSelList, nBurnDrv[i].hTreeHandle);
					break;
				}
			}

			// hack to load the preview image after a delay
			nInitPreviewTimer = SetTimer(hSelDlg, 1, 20, InitPreviewTimerProc);
		}

		LONG_PTR Style;
		Style = GetWindowLongPtr (GetDlgItem(hSelDlg, IDC_TREE2), GWL_STYLE);
		Style |= TVS_CHECKBOXES;
		SetWindowLongPtr (GetDlgItem(hSelDlg, IDC_TREE2), GWL_STYLE, Style);

		CreateFilters();

		EnableWindow(GetDlgItem(hDlg, IDC_SEL_APPLYIPS), FALSE);
		EnableWindow(GetDlgItem(hDlg, IDC_SEL_IPSMANAGER), FALSE);
		IpsPatchExit();	// bDoIpsPatch = false;

		WndInMid(hDlg, hParent);

		HICON hIcon = LoadIcon(hAppInst, MAKEINTRESOURCE(IDI_APP));
		SendMessage(hSelDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);		// Set the Game Selection dialog icon.

		GetInitialPositions();

		SetWindowPos(hDlg, NULL, 0, 0, nSelDlgWidth, nSelDlgHeight, SWP_NOZORDER);

		WndInMid(hDlg, hParent);

		return TRUE;
	}

	if(Msg == UM_CHECKSTATECHANGE) {
		HTREEITEM hItemChanged = (HTREEITEM)lParam;

		if (hItemChanged == hHardware) {
			if ((nLoadMenuShowX & MASKALL) == 0) {
				_TreeView_SetCheckState(hFilterList, hItemChanged, FALSE);

				_TreeView_SetCheckState(hFilterList, hFilterSegaGrp, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterCapcomGrp, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterCapcomMisc, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterCave, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterCps1, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterCps2, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterCps3, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterDataeast, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterGalaxian, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterIrem, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterKaneko, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterKonami, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterNeogeo, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterPacman, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterPgm, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterPsikyo, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSega, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSeta, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterTaito, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterTechnos, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterToaplan, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterMiscPre90s, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterMiscPost90s, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterMegadrive, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterPce, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSms, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterGg, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSg1000, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterColeco, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterMsx, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSpectrum, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterNes, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterFds, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSnes, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterNgp, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterChannelF, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterMidway, FALSE);

				nLoadMenuShowX |= MASKALL;
			} else {
				_TreeView_SetCheckState(hFilterList, hItemChanged, TRUE);

				_TreeView_SetCheckState(hFilterList, hFilterSegaGrp, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterCapcomGrp, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterCapcomMisc, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterCave, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterCps1, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterCps2, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterCps3, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterDataeast, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterGalaxian, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterIrem, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterKaneko, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterKonami, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterNeogeo, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterPacman, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterPgm, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterPsikyo, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSega, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSeta, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterTaito, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterTechnos, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterToaplan, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterMiscPre90s, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterMiscPost90s, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterMegadrive, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterPce, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSms, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterGg, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSg1000, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterColeco, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterMsx, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSpectrum, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterNes, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterFds, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSnes, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterNgp, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterChannelF, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterMidway, TRUE);

				nLoadMenuShowX &= ~MASKALL; //0xf8000000; make this dynamic for future hardware additions -dink
			}
		}

		if (hItemChanged == hBoardType) {
			if ((nLoadMenuBoardTypeFilter & MASKALLBOARD) == 0) {
				_TreeView_SetCheckState(hFilterList, hItemChanged, FALSE);

				_TreeView_SetCheckState(hFilterList, hFilterBootleg, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterDemo, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterHack, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterHomebrew, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterPrototype, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterGenuine, FALSE);

				nLoadMenuBoardTypeFilter = MASKALLBOARD;
			} else {
				_TreeView_SetCheckState(hFilterList, hItemChanged, TRUE);

				_TreeView_SetCheckState(hFilterList, hFilterBootleg, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterDemo, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterHack, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterHomebrew, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterPrototype, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterGenuine, TRUE);

				nLoadMenuBoardTypeFilter = 0;
			}
		}

		if (hItemChanged == hFamily) {
			if ((nLoadMenuFamilyFilter & MASKALLFAMILY) == 0) {
				_TreeView_SetCheckState(hFilterList, hItemChanged, FALSE);

				_TreeView_SetCheckState(hFilterList, hFilterOtherFamily, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterMslug, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSf, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterKof, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterDstlk, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterFatfury, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSamsho, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilter19xx, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSonicwi, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterPwrinst, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSonic, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterDonpachi, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterMahou, FALSE);

				nLoadMenuFamilyFilter = MASKALLFAMILY;
			} else {
				_TreeView_SetCheckState(hFilterList, hItemChanged, TRUE);

				_TreeView_SetCheckState(hFilterList, hFilterOtherFamily, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterMslug, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSf, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterKof, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterDstlk, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterFatfury, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSamsho, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilter19xx, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSonicwi, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterPwrinst, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSonic, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterDonpachi, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterMahou, TRUE);

				nLoadMenuFamilyFilter = 0;
			}
		}

		if (hItemChanged == hFavorites) {
			if (nLoadMenuFavoritesFilter) {
				_TreeView_SetCheckState(hFilterList, hItemChanged, FALSE);

				nLoadMenuFavoritesFilter = 0;
			} else {
				_TreeView_SetCheckState(hFilterList, hItemChanged, TRUE);

				nLoadMenuFavoritesFilter = 0xff;
			}
		}

		if (hItemChanged == hFilterSegaGrp) {
			if ((nLoadMenuShowX & MASKSEGAGRP) == 0) {
				_TreeView_SetCheckState(hFilterList, hItemChanged, FALSE);

				_TreeView_SetCheckState(hFilterList, hFilterSega, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSg1000, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSms, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterMegadrive, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterGg, FALSE);

				nLoadMenuShowX &= ~MASKSEGAGRP;
			} else {
				_TreeView_SetCheckState(hFilterList, hItemChanged, TRUE);


				_TreeView_SetCheckState(hFilterList, hFilterSega, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSg1000, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSms, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterMegadrive, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterGg, TRUE);

				nLoadMenuShowX |= MASKSEGAGRP;
			}
		}

		if (hItemChanged == hFilterCapcomGrp) {
			if ((nLoadMenuShowX & MASKCAPGRP) == 0) {
				_TreeView_SetCheckState(hFilterList, hItemChanged, FALSE);

				_TreeView_SetCheckState(hFilterList, hFilterCapcomMisc, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterCps1, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterCps2, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterCps3, FALSE);

				nLoadMenuShowX &= ~MASKCAPGRP;
			} else {
				_TreeView_SetCheckState(hFilterList, hItemChanged, TRUE);

				_TreeView_SetCheckState(hFilterList, hFilterCapcomMisc, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterCps1, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterCps2, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterCps3, TRUE);

				nLoadMenuShowX |= MASKCAPGRP;
			}
		}

		if (hItemChanged == hGenre) {
			if ((nLoadMenuGenreFilter & MASKALLGENRE) == 0) {
				_TreeView_SetCheckState(hFilterList, hItemChanged, FALSE);

				_TreeView_SetCheckState(hFilterList, hFilterHorshoot, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterVershoot, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterScrfight, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterVsfight, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterBios, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterBreakout, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterCasino, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterBallpaddle, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterMaze, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterMinigames, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterPinball, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterPlatform, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterPuzzle, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterQuiz, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSportsmisc, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSportsfootball, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterMisc, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterMahjong, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterRacing, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterShoot, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterAction, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterRungun, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterStrategy, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterRpg, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterSim, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterAdv, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterCard, FALSE);
				_TreeView_SetCheckState(hFilterList, hFilterBoard, FALSE);

				nLoadMenuGenreFilter = MASKALLGENRE;
			} else {
				_TreeView_SetCheckState(hFilterList, hItemChanged, TRUE);

				_TreeView_SetCheckState(hFilterList, hFilterHorshoot, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterVershoot, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterScrfight, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterVsfight, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterBios, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterBreakout, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterCasino, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterBallpaddle, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterMaze, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterMinigames, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterPinball, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterPlatform, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterPuzzle, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterQuiz, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSportsmisc, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSportsfootball, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterMisc, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterMahjong, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterRacing, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterShoot, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterAction, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterRungun, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterStrategy, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterRpg, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterSim, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterAdv, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterCard, TRUE);
				_TreeView_SetCheckState(hFilterList, hFilterBoard, TRUE);

				nLoadMenuGenreFilter = 0;
			}
		}

		if (hItemChanged == hFilterCapcomGrp)		_ToggleGameListing(nLoadMenuShowX, MASKCAPMISC);
		if (hItemChanged == hFilterCapcomGrp)		_ToggleGameListing(nLoadMenuShowX, MASKCPS);
		if (hItemChanged == hFilterCapcomGrp)		_ToggleGameListing(nLoadMenuShowX, MASKCPS2);
		if (hItemChanged == hFilterCapcomGrp)		_ToggleGameListing(nLoadMenuShowX, MASKCPS3);

		if (hItemChanged == hFilterSegaGrp)			_ToggleGameListing(nLoadMenuShowX, MASKSG1000);
		if (hItemChanged == hFilterSegaGrp)			_ToggleGameListing(nLoadMenuShowX, MASKSMS);
		if (hItemChanged == hFilterSegaGrp)			_ToggleGameListing(nLoadMenuShowX, MASKMEGADRIVE);
		if (hItemChanged == hFilterSegaGrp)			_ToggleGameListing(nLoadMenuShowX, MASKGG);
		if (hItemChanged == hFilterSegaGrp)			_ToggleGameListing(nLoadMenuShowX, MASKSEGA);

		if (hItemChanged == hFilterCapcomMisc)		_ToggleGameListing(nLoadMenuShowX, MASKCAPMISC);
		if (hItemChanged == hFilterCave)			_ToggleGameListing(nLoadMenuShowX, MASKCAVE);
		if (hItemChanged == hFilterCps1)			_ToggleGameListing(nLoadMenuShowX, MASKCPS);
		if (hItemChanged == hFilterCps2)			_ToggleGameListing(nLoadMenuShowX, MASKCPS2);
		if (hItemChanged == hFilterCps3)			_ToggleGameListing(nLoadMenuShowX, MASKCPS3);
		if (hItemChanged == hFilterDataeast)		_ToggleGameListing(nLoadMenuShowX, MASKDATAEAST);
		if (hItemChanged == hFilterGalaxian)		_ToggleGameListing(nLoadMenuShowX, MASKGALAXIAN);
		if (hItemChanged == hFilterIrem)			_ToggleGameListing(nLoadMenuShowX, MASKIREM);
		if (hItemChanged == hFilterKaneko)			_ToggleGameListing(nLoadMenuShowX, MASKKANEKO);
		if (hItemChanged == hFilterKonami)			_ToggleGameListing(nLoadMenuShowX, MASKKONAMI);
		if (hItemChanged == hFilterNeogeo)			_ToggleGameListing(nLoadMenuShowX, MASKNEOGEO);
		if (hItemChanged == hFilterPacman)			_ToggleGameListing(nLoadMenuShowX, MASKPACMAN);
		if (hItemChanged == hFilterPgm)				_ToggleGameListing(nLoadMenuShowX, MASKPGM);
		if (hItemChanged == hFilterPsikyo)			_ToggleGameListing(nLoadMenuShowX, MASKPSIKYO);
		if (hItemChanged == hFilterSega)			_ToggleGameListing(nLoadMenuShowX, MASKSEGA);
		if (hItemChanged == hFilterSeta)			_ToggleGameListing(nLoadMenuShowX, MASKSETA);
		if (hItemChanged == hFilterTaito)			_ToggleGameListing(nLoadMenuShowX, MASKTAITO);
		if (hItemChanged == hFilterTechnos)			_ToggleGameListing(nLoadMenuShowX, MASKTECHNOS);
		if (hItemChanged == hFilterToaplan)			_ToggleGameListing(nLoadMenuShowX, MASKTOAPLAN);
		if (hItemChanged == hFilterMiscPre90s)		_ToggleGameListing(nLoadMenuShowX, MASKMISCPRE90S);
		if (hItemChanged == hFilterMiscPost90s)		_ToggleGameListing(nLoadMenuShowX, MASKMISCPOST90S);
		if (hItemChanged == hFilterMegadrive)		_ToggleGameListing(nLoadMenuShowX, MASKMEGADRIVE);
		if (hItemChanged == hFilterPce)				_ToggleGameListing(nLoadMenuShowX, MASKPCENGINE);
		if (hItemChanged == hFilterSms)				_ToggleGameListing(nLoadMenuShowX, MASKSMS);
		if (hItemChanged == hFilterGg)				_ToggleGameListing(nLoadMenuShowX, MASKGG);
		if (hItemChanged == hFilterSg1000)			_ToggleGameListing(nLoadMenuShowX, MASKSG1000);
		if (hItemChanged == hFilterColeco)			_ToggleGameListing(nLoadMenuShowX, MASKCOLECO);
		if (hItemChanged == hFilterMsx)				_ToggleGameListing(nLoadMenuShowX, MASKMSX);
		if (hItemChanged == hFilterSpectrum)		_ToggleGameListing(nLoadMenuShowX, MASKSPECTRUM);
		if (hItemChanged == hFilterNes)				_ToggleGameListing(nLoadMenuShowX, MASKNES);
		if (hItemChanged == hFilterFds)				_ToggleGameListing(nLoadMenuShowX, MASKFDS);
		if (hItemChanged == hFilterSnes)			_ToggleGameListing(nLoadMenuShowX, MASKSNES);
		if (hItemChanged == hFilterNgp)				_ToggleGameListing(nLoadMenuShowX, MASKNGP);
		if (hItemChanged == hFilterChannelF)		_ToggleGameListing(nLoadMenuShowX, MASKCHANNELF);
		if (hItemChanged == hFilterMidway)			_ToggleGameListing(nLoadMenuShowX, MASKMIDWAY);

		if (hItemChanged == hFilterBootleg)			_ToggleGameListing(nLoadMenuBoardTypeFilter, BDF_BOOTLEG);
		if (hItemChanged == hFilterDemo)			_ToggleGameListing(nLoadMenuBoardTypeFilter, BDF_DEMO);
		if (hItemChanged == hFilterHack)			_ToggleGameListing(nLoadMenuBoardTypeFilter, BDF_HACK);
		if (hItemChanged == hFilterHomebrew)		_ToggleGameListing(nLoadMenuBoardTypeFilter, BDF_HOMEBREW);
		if (hItemChanged == hFilterPrototype)		_ToggleGameListing(nLoadMenuBoardTypeFilter, BDF_PROTOTYPE);
		if (hItemChanged == hFilterGenuine)			_ToggleGameListing(nLoadMenuBoardTypeFilter, MASKBOARDTYPEGENUINE);

		if (hItemChanged == hFilterOtherFamily)		_ToggleGameListing(nLoadMenuFamilyFilter, MASKFAMILYOTHER);
		if (hItemChanged == hFilterMahou)			_ToggleGameListing(nLoadMenuFamilyFilter, FBF_MAHOU);
		if (hItemChanged == hFilterDonpachi)		_ToggleGameListing(nLoadMenuFamilyFilter, FBF_DONPACHI);
		if (hItemChanged == hFilterMslug)			_ToggleGameListing(nLoadMenuFamilyFilter, FBF_MSLUG);
		if (hItemChanged == hFilterSf)				_ToggleGameListing(nLoadMenuFamilyFilter, FBF_SF);
		if (hItemChanged == hFilterKof)				_ToggleGameListing(nLoadMenuFamilyFilter, FBF_KOF);
		if (hItemChanged == hFilterDstlk)			_ToggleGameListing(nLoadMenuFamilyFilter, FBF_DSTLK);
		if (hItemChanged == hFilterFatfury)			_ToggleGameListing(nLoadMenuFamilyFilter, FBF_FATFURY);
		if (hItemChanged == hFilterSamsho)			_ToggleGameListing(nLoadMenuFamilyFilter, FBF_SAMSHO);
		if (hItemChanged == hFilter19xx)			_ToggleGameListing(nLoadMenuFamilyFilter, FBF_19XX);
		if (hItemChanged == hFilterSonicwi)			_ToggleGameListing(nLoadMenuFamilyFilter, FBF_SONICWI);
		if (hItemChanged == hFilterPwrinst)			_ToggleGameListing(nLoadMenuFamilyFilter, FBF_PWRINST);
		if (hItemChanged == hFilterSonic)			_ToggleGameListing(nLoadMenuFamilyFilter, FBF_SONIC);

		if (hItemChanged == hFilterHorshoot)		_ToggleGameListing(nLoadMenuGenreFilter, GBF_HORSHOOT);
		if (hItemChanged == hFilterVershoot)		_ToggleGameListing(nLoadMenuGenreFilter, GBF_VERSHOOT);
		if (hItemChanged == hFilterScrfight)		_ToggleGameListing(nLoadMenuGenreFilter, GBF_SCRFIGHT);
		if (hItemChanged == hFilterVsfight)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_VSFIGHT);
		if (hItemChanged == hFilterBios)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_BIOS);
		if (hItemChanged == hFilterBreakout)		_ToggleGameListing(nLoadMenuGenreFilter, GBF_BREAKOUT);
		if (hItemChanged == hFilterCasino)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_CASINO);
		if (hItemChanged == hFilterBallpaddle)		_ToggleGameListing(nLoadMenuGenreFilter, GBF_BALLPADDLE);
		if (hItemChanged == hFilterMaze)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_MAZE);
		if (hItemChanged == hFilterMinigames)		_ToggleGameListing(nLoadMenuGenreFilter, GBF_MINIGAMES);
		if (hItemChanged == hFilterPinball)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_PINBALL);
		if (hItemChanged == hFilterPlatform)		_ToggleGameListing(nLoadMenuGenreFilter, GBF_PLATFORM);
		if (hItemChanged == hFilterPuzzle)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_PUZZLE);
		if (hItemChanged == hFilterQuiz)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_QUIZ);
		if (hItemChanged == hFilterSportsmisc)		_ToggleGameListing(nLoadMenuGenreFilter, GBF_SPORTSMISC);
		if (hItemChanged == hFilterSportsfootball)	_ToggleGameListing(nLoadMenuGenreFilter, GBF_SPORTSFOOTBALL);
		if (hItemChanged == hFilterMisc)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_MISC);
		if (hItemChanged == hFilterMahjong)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_MAHJONG);
		if (hItemChanged == hFilterRacing)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_RACING);
		if (hItemChanged == hFilterShoot)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_SHOOT);
		if (hItemChanged == hFilterAction)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_ACTION);
		if (hItemChanged == hFilterRungun)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_RUNGUN);
		if (hItemChanged == hFilterStrategy)		_ToggleGameListing(nLoadMenuGenreFilter, GBF_STRATEGY);
		if (hItemChanged == hFilterRpg)				_ToggleGameListing(nLoadMenuGenreFilter, GBF_RPG);
		if (hItemChanged == hFilterSim)				_ToggleGameListing(nLoadMenuGenreFilter, GBF_SIM);
		if (hItemChanged == hFilterAdv)				_ToggleGameListing(nLoadMenuGenreFilter, GBF_ADV);
		if (hItemChanged == hFilterCard)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_CARD);
		if (hItemChanged == hFilterBoard)			_ToggleGameListing(nLoadMenuGenreFilter, GBF_BOARD);

		RebuildEverything();
	}

	if (Msg == WM_COMMAND) {
		if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDC_SEL_SEARCH) {
			if (bSearchStringInit) {
				bSearchStringInit = false;
				return 0;
			}

			KillTimer(hDlg, IDC_SEL_SEARCHTIMER);
			SetTimer(hDlg, IDC_SEL_SEARCHTIMER, 500, (TIMERPROC)NULL);
		}

		if (HIWORD(wParam) == BN_CLICKED) {
			int wID = LOWORD(wParam);
			switch (wID) {
				case IDOK:
					SelOkay();
					break;
				case IDROM:
					RomsDirCreate(hSelDlg);
					RebuildEverything();
					break;
				case IDRESCAN:
					LookupSubDirThreads();
					bRescanRoms = true;
					CreateROMInfo(hSelDlg);
					RebuildEverything();
					break;
				case IDCANCEL:
					bDialogCancel = true;
					SendMessage(hDlg, WM_CLOSE, 0, 0);
					return 0;
				case IDC_CHECKAVAILABLE:
					nLoadMenuShowY ^= AVAILABLE;
					RebuildEverything();
					break;
				case IDC_CHECKUNAVAILABLE:
					nLoadMenuShowY ^= UNAVAILABLE;
					RebuildEverything();
					break;
				case IDC_CHECKAUTOEXPAND:
					nLoadMenuShowY ^= AUTOEXPAND;
					RebuildEverything();
					break;
				case IDC_SEL_SHORTNAME:
					nLoadMenuShowY ^= SHOWSHORT;
					RebuildEverything();
					break;
				case IDC_SEL_ASCIIONLY:
					nLoadMenuShowY ^= ASCIIONLY;
					RebuildEverything();
					break;
				case IDC_SEL_SUBDIRS:
					nLoadMenuShowY ^= SEARCHSUBDIRS;
					LookupSubDirThreads();
					break;
				case IDGAMEINFO:
					if (bDrvSelected) {
						GameInfoDialogCreate(hSelDlg, nBurnDrvActive);
						SetFocus(hSelList); // Update list for Rescan Romset button
					} else {
						MessageBox(hSelDlg, FBALoadStringEx(hAppInst, IDS_ERR_NO_DRIVER_SELECTED, true), FBALoadStringEx(hAppInst, IDS_ERR_ERROR, true), MB_OK);
					}
					break;
				case IDC_SEL_IPSMANAGER:
					if (bDrvSelected) {
						UINT32 nOldnBurnDrvActive = nBurnDrvActive;
						IpsManagerCreate(hSelDlg);
						nBurnDrvActive = nOldnBurnDrvActive; // due to some weird bug in sel.cpp, nBurnDrvActive can sometimes change when clicking in new dialogs.
						INT32 nActivePatches = LoadIpsActivePatches();
						EnableWindow(GetDlgItem(hDlg, IDC_SEL_APPLYIPS), nActivePatches);
						SetFocus(hSelList);
					} else {
						MessageBox(hSelDlg, FBALoadStringEx(hAppInst, IDS_ERR_NO_DRIVER_SELECTED, true), FBALoadStringEx(hAppInst, IDS_ERR_ERROR, true), MB_OK);
					}
					break;
				case IDC_SEL_APPLYIPS:
					bDoIpsPatch = !bDoIpsPatch;
					break;
			}
		}

		int id = LOWORD(wParam);

		switch (id) {
			case GAMESEL_MENU_PLAY: {
				SelOkay();
				break;
			}

			case GAMESEL_MENU_GAMEINFO: {
				/*UpdatePreview(true, hSelDlg, szAppPreviewsPath);
				if (nTimer) {
					KillTimer(hSelDlg, nTimer);
					nTimer = 0;
				}
				GameInfoDialogCreate(hSelDlg, nBurnDrvSelect);*/
				if (bDrvSelected) {
					GameInfoDialogCreate(hSelDlg, nBurnDrvActive);
					SetFocus(hSelList); // Update list for Rescan Romset button
				} else {
					MessageBox(hSelDlg, FBALoadStringEx(hAppInst, IDS_ERR_NO_DRIVER_SELECTED, true), FBALoadStringEx(hAppInst, IDS_ERR_ERROR, true), MB_OK);
				}
				break;
			}

			case GAMESEL_MENU_VIEWEMMA: {
				if (!nVidFullscreen) {
					ViewEmma();
				}
				break;
			}

			case GAMESEL_MENU_FAVORITE: { // toggle favorite status.
				if (bDrvSelected) {
					AddFavorite_Ext((CheckFavorites(BurnDrvGetTextA(DRV_NAME)) == -1) ? 1 : 0);
				} else {
					MessageBox(hSelDlg, FBALoadStringEx(hAppInst, IDS_ERR_NO_DRIVER_SELECTED, true), FBALoadStringEx(hAppInst, IDS_ERR_ERROR, true), MB_OK);
				}

				break;
			}

			case GAMESEL_MENU_ROMDATA: { // Export to RomData template
				if (bDrvSelected) {
					RomDataExportTemplate(hSelDlg, nDialogSelect);
				}
				else {
					MessageBox(hSelDlg, FBALoadStringEx(hAppInst, IDS_ERR_NO_DRIVER_SELECTED, true), FBALoadStringEx(hAppInst, IDS_ERR_ERROR, true), MB_OK);
				}

				break;
			}
		}
	}

	if (Msg == WM_TIMER) {
		switch (wParam) {
			case IDC_SEL_SEARCHTIMER:
				KillTimer(hDlg, IDC_SEL_SEARCHTIMER);
				RebuildEverything();
				break;
		}
	}

	if (Msg == UM_CLOSE) {
		nDialogSelect = nOldDlgSelected;
		MyEndDialog();
		DeleteObject(hWhiteBGBrush);
		return 0;
	}

	if (Msg == WM_CLOSE) {
		bDialogCancel = true;
		nDialogSelect = nOldDlgSelected;
		MyEndDialog();
		DeleteObject(hWhiteBGBrush);
		return 0;
	}

	if (Msg == WM_GETMINMAXINFO) {
		MINMAXINFO *info = (MINMAXINFO*)lParam;

		info->ptMinTrackSize.x = nDlgInitialWidth;
		info->ptMinTrackSize.y = nDlgInitialHeight;

		return 0;
	}

#if 0
	if (Msg == WM_WINDOWPOSCHANGED) {	// All controls blink when dragging the window
#endif // 0

	if (Msg == WM_SIZE) {
		RECT rc;
		int xDelta;
		int yDelta;
		int xScrollBarDelta;

		if (nDlgInitialWidth == 0 || nDlgInitialHeight == 0) return 0;

		GetClientRect(hDlg, &rc);

		xDelta = nDlgInitialWidth - rc.right;
		yDelta = nDlgInitialHeight - rc.bottom;

		if (xDelta == 0 && yDelta == 0) return 0;

		SetControlPosAlignTopRight(IDC_STATIC_OPT, nDlgOptionsGrpInitialPos);
		SetControlPosAlignTopRight(IDC_CHECKAVAILABLE, nDlgAvailableChbInitialPos);
		SetControlPosAlignTopRight(IDC_CHECKUNAVAILABLE, nDlgUnavailableChbInitialPos);
		SetControlPosAlignTopRight(IDC_CHECKAUTOEXPAND, nDlgAlwaysClonesChbInitialPos);
		SetControlPosAlignTopRight(IDC_SEL_SHORTNAME, nDlgZipnamesChbInitialPos);
		SetControlPosAlignTopRight(IDC_SEL_ASCIIONLY, nDlgLatinTextChbInitialPos);
		SetControlPosAlignTopRight(IDC_SEL_SUBDIRS, nDlgSearchSubDirsChbInitialPos);
		SetControlPosAlignTopRight(IDROM, nDlgRomDirsBtnInitialPos);
		SetControlPosAlignTopRight(IDRESCAN, nDlgScanRomsBtnInitialPos);

		SetControlPosAlignTopRightResizeVert(IDC_STATIC_SYS, nDlgFilterGrpInitialPos);
		SetControlPosAlignTopRightResizeVert(IDC_TREE2, nDlgFilterTreeInitialPos);

		SetControlPosAlignBottomRight(IDC_SEL_IPSGROUP, nDlgIpsGrpInitialPos);
		SetControlPosAlignBottomRight(IDC_SEL_APPLYIPS, nDlgApplyIpsChbInitialPos);
		SetControlPosAlignBottomRight(IDC_SEL_IPSMANAGER, nDlgIpsManBtnInitialPos);
		SetControlPosAlignBottomRight(IDC_SEL_SEARCHGROUP, nDlgSearchGrpInitialPos);
		SetControlPosAlignBottomRight(IDC_SEL_SEARCH, nDlgSearchTxtInitialPos);
		SetControlPosAlignBottomRight(IDCANCEL, nDlgCancelBtnInitialPos);
		SetControlPosAlignBottomRight(IDOK, nDlgPlayBtnInitialPos);

		SetControlPosAlignTopLeft(IDC_STATIC3, nDlgTitleGrpInitialPos);
		SetControlPosAlignTopLeft(IDC_SCREENSHOT2_H, nDlgTitleImgHInitialPos);
		SetControlPosAlignTopLeft(IDC_SCREENSHOT2_V, nDlgTitleImgVInitialPos);
		SetControlPosAlignTopLeft(IDC_STATIC2, nDlgPreviewGrpInitialPos);
		SetControlPosAlignTopLeft(IDC_SCREENSHOT_H, nDlgPreviewImgHInitialPos);
		SetControlPosAlignTopLeft(IDC_SCREENSHOT_V, nDlgPreviewImgVInitialPos);

		SetControlPosAlignBottomLeftResizeHor(IDC_STATIC_INFOBOX, nDlgWhiteBoxInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_LABELCOMMENT, nDlgGameInfoLblInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_LABELROMNAME, nDlgRomNameLblInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_LABELROMINFO, nDlgRomInfoLblInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_LABELSYSTEM, nDlgReleasedByLblInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_LABELGENRE, nDlgGenreLblInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_LABELNOTES, nDlgNotesLblInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_TEXTCOMMENT, nDlgGameInfoTxtInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_TEXTROMNAME, nDlgRomNameTxtInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_TEXTROMINFO, nDlgRomInfoTxtInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_TEXTSYSTEM, nDlgReleasedByTxtInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_TEXTGENRE, nDlgGenreTxtInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_TEXTNOTES, nDlgNotesTxtInitialPos);
		SetControlPosAlignBottomLeftResizeHor(IDC_DRVCOUNT, nDlgDrvCountTxtInitialPos);
		SetControlPosAlignBottomRight(IDGAMEINFO, nDlgDrvRomInfoBtnInitialPos);

		SetControlPosAlignTopLeftResizeHorVert(IDC_STATIC1, nDlgSelectGameGrpInitialPos);

		if (bIsWindowsXP && nTmpDrvCount < 12) { // Fix an issue on WinXP where the scrollbar overwrites past the tree size when less then 12 items are in the list
			SetControlPosAlignTopLeftResizeHorVertALT(IDC_TREE1, nDlgSelectGameLstInitialPos);
		} else {
			SetControlPosAlignTopLeftResizeHorVert(IDC_TREE1, nDlgSelectGameLstInitialPos);
		}

		InvalidateRect(hSelDlg, NULL, true);
		UpdateWindow(hSelDlg);

		return 0;
	}

//	if (Msg == WM_TIMER) {
//		UpdatePreview(false, szAppPreviewsPath, IDC_SCREENSHOT_H, IDC_SCREENSHOT_V);
//		return 0;
//	}

	if (Msg == WM_CTLCOLORSTATIC) {
		for (int i = 0; i < 6; i++) {
			if ((HWND)lParam == hInfoLabel[i])	{ return (INT_PTR)hWhiteBGBrush; }
			if ((HWND)lParam == hInfoText[i])	{ return (INT_PTR)hWhiteBGBrush; }
		}
	}

	NMHDR* pNmHdr = (NMHDR*)lParam;
	if (Msg == WM_NOTIFY)
	{
		if ((pNmHdr->code == TVN_ITEMEXPANDED) && (pNmHdr->idFrom == IDC_TREE2))
		{
			// save the expanded state of the filter nodes
			NM_TREEVIEW *pnmtv = (NM_TREEVIEW *)lParam;
			TV_ITEM curItem = pnmtv->itemNew;

			if (pnmtv->action == TVE_COLLAPSE)
			{
				nLoadMenuExpand &= ~TvhFilterToBitmask(curItem.hItem);
			}
			else if (pnmtv->action == TVE_EXPAND)
			{
				nLoadMenuExpand |= TvhFilterToBitmask(curItem.hItem);
			}
		}

		if ((pNmHdr->code == NM_CLICK) && (pNmHdr->idFrom == IDC_TREE2))
		{
			TVHITTESTINFO thi;
			DWORD dwpos = GetMessagePos();
			thi.pt.x	= GET_X_LPARAM(dwpos);
			thi.pt.y	= GET_Y_LPARAM(dwpos);
			MapWindowPoints(HWND_DESKTOP, pNmHdr->hwndFrom, &thi.pt, 1);
			TreeView_HitTest(pNmHdr->hwndFrom, &thi);

			if(TVHT_ONITEMSTATEICON & thi.flags) {
				PostMessage(hSelDlg, UM_CHECKSTATECHANGE, 0, (LPARAM)thi.hItem);
			}

			return 1;
		}

		NMTREEVIEW* pnmtv = (NMTREEVIEW*)lParam;

		if (!TreeBuilding && pnmtv->hdr.code == NM_DBLCLK && pnmtv->hdr.idFrom == IDC_TREE1)
		{
			DWORD dwpos = GetMessagePos();

			TVHITTESTINFO thi;
			thi.pt.x	= GET_X_LPARAM(dwpos);
			thi.pt.y	= GET_Y_LPARAM(dwpos);

			MapWindowPoints(HWND_DESKTOP, pNmHdr->hwndFrom, &thi.pt, 1);

			TreeView_HitTest(pNmHdr->hwndFrom, &thi);

			HTREEITEM hSelectHandle = thi.hItem;
			if(hSelectHandle == NULL) return 1;

			TreeView_SelectItem(hSelList, hSelectHandle);

			// Search through nBurnDrv[] for the nBurnDrvNo according to the returned hSelectHandle
			for (unsigned int i = 0; i < nTmpDrvCount; i++) {
				if (hSelectHandle == nBurnDrv[i].hTreeHandle) {
					nBurnDrvActive = nBurnDrv[i].nBurnDrvNo;
					break;
				}
			}

			nDialogSelect	= nBurnDrvActive;
			bDrvSelected	= true;

			SelOkay();

			// disable double-click node-expand
			SetWindowLongPtr(hSelDlg, DWLP_MSGRESULT, 1);

			return 1;
		}

		if (pNmHdr->code == NM_CUSTOMDRAW && LOWORD(wParam) == IDC_TREE1) {
			LPNMTVCUSTOMDRAW lptvcd = (LPNMTVCUSTOMDRAW)lParam;
			int nGetTextFlags = nLoadMenuShowY & ASCIIONLY ? DRV_ASCIIONLY : 0;
			HTREEITEM hSelectHandle;

			switch (lptvcd->nmcd.dwDrawStage) {
				case CDDS_PREPAINT: {
					SetWindowLongPtr(hSelDlg, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
					return 1;
				}

				case CDDS_ITEMPREPAINT:	{
					hSelectHandle = (HTREEITEM)(lptvcd->nmcd.dwItemSpec);
					RECT rect;

					{
						RECT rcClip;
						TreeView_GetItemRect(lptvcd->nmcd.hdr.hwndFrom, hSelectHandle, &rect, TRUE);

						// Check if the current item is in the area to be redrawn(only the visible part is drawn)
						if (!IntersectRect(&rcClip, &lptvcd->nmcd.rc, &rect)) {
//							SetWindowLongPtr(hSelDlg, DWLP_MSGRESULT, CDRF_SKIPDEFAULT);
							return 1;
						}
					}

					// TVITEM (msdn.microsoft.com) This structure is identical to the TV_ITEM structure, but it has been renamed to
					// follow current naming conventions. New applications should use this structure.

					//TV_ITEM TvItem;
					TVITEM TvItem;
					TvItem.hItem = hSelectHandle;
					TvItem.mask  = TVIF_PARAM | TVIF_STATE | TVIF_CHILDREN;
					SendMessage(hSelList, TVM_GETITEM, 0, (LPARAM)&TvItem);

//					dprintf(_T("  - Item (%i?i) - (%i?i) %hs\n"), lptvcd->nmcd.rc.left, lptvcd->nmcd.rc.top, lptvcd->nmcd.rc.right, lptvcd->nmcd.rc.bottom, ((NODEINFO*)TvItem.lParam)->pszROMName);

					// Set the foreground and background colours unless the item is highlighted
					if (!(TvItem.state & (TVIS_SELECTED | TVIS_DROPHILITED))) {

						// Set less contrasting colours for clones
						if (!((NODEINFO*)TvItem.lParam)->bIsParent) {
							lptvcd->clrTextBk = RGB(0xD7, 0xD7, 0xD7);
							lptvcd->clrText   = RGB(0x3F, 0x3F, 0x3F);
						}

						// For parents, change the colour of the background, for clones, change only the text colour
						if (!CheckWorkingStatus(((NODEINFO*)TvItem.lParam)->nBurnDrvNo)) {
							lptvcd->clrText = RGB(0x7F, 0x7F, 0x7F);
						}

						// Slightly different color for favorites (key lime pie anyone?)
						if (CheckFavorites(((NODEINFO*)TvItem.lParam)->pszROMName) != -1) {
							if (!((NODEINFO*)TvItem.lParam)->bIsParent) {
								lptvcd->clrTextBk = RGB(0xd7, 0xe7, 0xd7);
							} else {
								lptvcd->clrTextBk = RGB(0xe6, 0xff, 0xe6);

								// Both parent and clone are in favorites
								if (!(TvItem.state & TVIS_EXPANDED) && TvItem.cChildren) {
									HTREEITEM hChild = TreeView_GetChild(hSelList, TvItem.hItem);
									while (NULL != hChild) {
										TVITEM tvi = { 0 };
										tvi.mask   = TVIF_PARAM | TVIF_HANDLE;
										tvi.hItem  = hChild;
										if (TreeView_GetItem(hSelList, &tvi)) {
											if (-1 != CheckFavorites(((NODEINFO*)tvi.lParam)->pszROMName)) {
												lptvcd->clrTextBk = RGB(0xe6, 0xe6, 0xfa);		// Lavender
												break;
											}
										}
										hChild = TreeView_GetNextSibling(hSelList, hChild);
									}
								}
							}
						} else {
							// Only clones are favorites
							if (((NODEINFO*)TvItem.lParam)->bIsParent) {
								if (!(TvItem.state & TVIS_EXPANDED) && TvItem.cChildren) {
									HTREEITEM hChild = TreeView_GetChild(hSelList, TvItem.hItem);
									while (NULL != hChild) {
										TVITEM tvi = { 0 };
										tvi.mask   = TVIF_PARAM | TVIF_HANDLE;
										tvi.hItem  = hChild;
										if (TreeView_GetItem(hSelList, &tvi)) {
											if (-1 != CheckFavorites(((NODEINFO*)tvi.lParam)->pszROMName)) {
												lptvcd->clrTextBk = RGB(0xff, 0xf0, 0xf5);		// Lavender blush
												break;
											}
										}
										hChild = TreeView_GetNextSibling(hSelList, hChild);
									}
								}
							}
						}
					}

					rect.left   = lptvcd->nmcd.rc.left;
					rect.right  = lptvcd->nmcd.rc.right;
					rect.top    = lptvcd->nmcd.rc.top;
					rect.bottom = lptvcd->nmcd.rc.bottom;

					nBurnDrvActive = ((NODEINFO*)TvItem.lParam)->nBurnDrvNo;

					{
						// Fill background
						HBRUSH hBackBrush = CreateSolidBrush(lptvcd->clrTextBk);
						FillRect(lptvcd->nmcd.hdc, &lptvcd->nmcd.rc, hBackBrush);
						DeleteObject(hBackBrush);
					}

					{
						// Draw plus and minus buttons
						if (((NODEINFO*)TvItem.lParam)->bIsParent && TvItem.cChildren) {
							HICON hIcon = (TvItem.state & TVIS_EXPANDED) ? hCollapse : hExpand;
							DrawIconEx(lptvcd->nmcd.hdc, rect.left + 4, rect.top + nIconsYDiff, hIcon, 16, 16, 0, NULL, DI_NORMAL);
						}
						rect.left += 16 + 8;
					}

					{
						// Draw text

						TCHAR  szText[1024];
						TCHAR* pszPosition = szText;
						TCHAR* pszName;
						SIZE   size = { 0, 0 };

						SetTextColor(lptvcd->nmcd.hdc, lptvcd->clrText);
						SetBkMode(lptvcd->nmcd.hdc, TRANSPARENT);

						// Display the short name if needed
						if (nLoadMenuShowY & SHOWSHORT) {
							// calculate field expansion (between romname & description)
							int expansion = (rect.right / dpi_x) - 4;
							if (expansion < 0) expansion = 0;

							const int FIELD_SIZE = 16 + 40 + 20 + 20 + (expansion*8);
							const int EXPAND_ICON_SIZE = 16 + 8;
							const int temp_right = rect.right;
							rect.right = EXPAND_ICON_SIZE + FIELD_SIZE - 2;

							DrawText(lptvcd->nmcd.hdc, BurnDrvGetText(DRV_NAME), -1, &rect, DT_NOPREFIX | DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
							rect.right = temp_right;
							rect.left += FIELD_SIZE;
						}

						rect.top += 2;

						bool bParentExp = false;	// If true, Item is clone and parent is expanded
						if (!((NODEINFO*)TvItem.lParam)->bIsParent) {
							HTREEITEM hParent = TreeView_GetParent(hSelList, TvItem.hItem);
							if (NULL != hParent) {
								bParentExp = (TreeView_GetItemState(hSelList, hParent, TVIS_EXPANDED) & TVIS_EXPANDED);
							}
						}

						{
							// Draw icons if needed
							if (((NODEINFO*)TvItem.lParam)->bIsParent || bParentExp) {
								if (!CheckWorkingStatus(nBurnDrvActive)) {
									DrawIconEx(lptvcd->nmcd.hdc, rect.left, rect.top, hNotWorking, nIconsSizeXY, nIconsSizeXY, 0, NULL, DI_NORMAL);
									rect.left += nIconsSizeXY + 4;
								} else {
									if (!(gameAv[nBurnDrvActive]) && !bSkipStartupCheck) {
										DrawIconEx(lptvcd->nmcd.hdc, rect.left, rect.top, hNotFoundEss, nIconsSizeXY, nIconsSizeXY, 0, NULL, DI_NORMAL);
										rect.left += nIconsSizeXY + 4;
									} else {
										if (!(nLoadMenuShowY & AVAILABLE) && !(gameAv[nBurnDrvActive] & 2)) {
											DrawIconEx(lptvcd->nmcd.hdc, rect.left, rect.top, hNotFoundNonEss, nIconsSizeXY, nIconsSizeXY, 0, NULL, DI_NORMAL);
											rect.left += nIconsSizeXY + 4;
										}
									}
								}
							}
						}

						// Driver Icon drawing code...
						if (bEnableIcons && bIconsLoaded && (((NODEINFO*)TvItem.lParam)->bIsParent || bParentExp)) {
							// Windows GDI limitation, can not cache all icons, can only cache the following icons
							// All hardware icon exist (By hardware)
							// All non-Clone icon exist (By game)
							// When the Clone icon option is turned on, the parent item has an icon and Clone does not (By game, They do not take up GDI resources)
							if (hDrvIcon[nBurnDrvActive]) {
								DrawIconEx(lptvcd->nmcd.hdc, rect.left, rect.top, hDrvIcon[nBurnDrvActive], nIconsSizeXY, nIconsSizeXY, 0, NULL, DI_NORMAL);
							}

							if (!hDrvIcon[nBurnDrvActive]) {
								// Non-Clone
								if ((NULL == BurnDrvGetText(DRV_PARENT)) && !(BurnDrvGetFlags() & BDF_CLONE)) {
									DrawIconEx(lptvcd->nmcd.hdc, rect.left, rect.top, hDrvIconMiss, nIconsSizeXY, nIconsSizeXY, 0, NULL, DI_NORMAL);
								}
								// Clone
								else {
									if (!bIconsOnlyParents) {
										// By hardware
										if (bIconsByHardwares) {
											DrawIconEx(lptvcd->nmcd.hdc, rect.left, rect.top, hDrvIconMiss, nIconsSizeXY, nIconsSizeXY, 0, NULL, DI_NORMAL);
										}
										// By game
										else {
											TCHAR szIcon[MAX_PATH] = { 0 };
											_stprintf(szIcon, _T("%s%s.ico"), szAppIconsPath, BurnDrvGetText(DRV_NAME));

											// Find the icons that meet the conditions, load and redraw them one by one and then recycle the resources to avoid memory leakage due to GDI resource overflow
											// Exclude all parent set
											// Exclude all hardware icons
											// All the Clones where you can find icons
											// Creates a temporary HICON object, which is destroyed immediately upon completion of the redraw.
											HICON hTempIcon = (HICON)LoadImage(NULL, szIcon, IMAGE_ICON, nIconsSizeXY, nIconsSizeXY, LR_LOADFROMFILE);
											HICON hGameIcon = (NULL != hTempIcon) ? hTempIcon : hDrvIconMiss;
											DrawIconEx(lptvcd->nmcd.hdc, rect.left, rect.top, hGameIcon, nIconsSizeXY, nIconsSizeXY, 0, NULL, DI_NORMAL);

											if (NULL != hTempIcon) {	// Clone icon exist (By game)
												DestroyIcon(hTempIcon); hTempIcon = NULL;
											}
										}
									}
								}
							}
							rect.left += nIconsSizeXY + 4;
						}

						_tcsncpy(szText, MangleGamename(BurnDrvGetText(nGetTextFlags | DRV_FULLNAME), false), 1024);
						szText[1023] = _T('\0');

						GetTextExtentPoint32(lptvcd->nmcd.hdc, szText, _tcslen(szText), &size);

						DrawText(lptvcd->nmcd.hdc, szText, -1, &rect, DT_NOPREFIX | DT_SINGLELINE | DT_LEFT | DT_VCENTER);

						// Display extra info if needed
						szText[0] = _T('\0');

						pszName = BurnDrvGetText(nGetTextFlags | DRV_FULLNAME);
						while ((pszName = BurnDrvGetText(nGetTextFlags | DRV_NEXTNAME | DRV_FULLNAME)) != NULL) {
							if (pszPosition + _tcslen(pszName) - 1024 > szText) {
								break;
							}
							pszPosition += _stprintf(pszPosition, _T(SEPERATOR_2) _T("%s"), pszName);
						}
						if (szText[0]) {
							szText[255] = _T('\0');

							unsigned int r = ((lptvcd->clrText >> 16 & 255) * 2 + (lptvcd->clrTextBk >> 16 & 255)) / 3;
							unsigned int g = ((lptvcd->clrText >>  8 & 255) * 2 + (lptvcd->clrTextBk >>  8 & 255)) / 3;
							unsigned int b = ((lptvcd->clrText >>  0 & 255) * 2 + (lptvcd->clrTextBk >>  0 & 255)) / 3;

							rect.left += size.cx;
							SetTextColor(lptvcd->nmcd.hdc, (r << 16) | (g <<  8) | (b <<  0));
							DrawText(lptvcd->nmcd.hdc, szText, -1, &rect, DT_NOPREFIX | DT_SINGLELINE | DT_LEFT | DT_VCENTER);
						}
					}

					SetWindowLongPtr(hSelDlg, DWLP_MSGRESULT, CDRF_SKIPDEFAULT);
					return 1;
				}

				default: {
					SetWindowLongPtr(hSelDlg, DWLP_MSGRESULT, CDRF_DODEFAULT);
					return 1;
				}
			}
		}

		if (pNmHdr->code == TVN_ITEMEXPANDING && !TreeBuilding && LOWORD(wParam) == IDC_TREE1) {
			SendMessage(hSelList, TVM_SELECTITEM, TVGN_CARET, (LPARAM)((LPNMTREEVIEW)lParam)->itemNew.hItem);
			return FALSE;
		}

		if (pNmHdr->code == TVN_SELCHANGED && !TreeBuilding && LOWORD(wParam) == IDC_TREE1) {
			HTREEITEM hSelectHandle = (HTREEITEM)SendMessage(hSelList, TVM_GETNEXTITEM, TVGN_CARET, ~0U);

			// Search through nBurnDrv[] for the nBurnDrvNo according to the returned hSelectHandle
			for (unsigned int i = 0; i < nTmpDrvCount; i++) {
				if (hSelectHandle == nBurnDrv[i].hTreeHandle)
				{
					nBurnDrvActive	= nBurnDrv[i].nBurnDrvNo;
					nDialogSelect	= nBurnDrvActive;
					bDrvSelected	= true;
					UpdatePreview(true, szAppPreviewsPath, IDC_SCREENSHOT_H, IDC_SCREENSHOT_V);
					UpdatePreview(false, szAppTitlesPath, IDC_SCREENSHOT2_H, IDC_SCREENSHOT2_V);
					break;
				}
			}

			CheckDlgButton(hDlg, IDC_SEL_APPLYIPS, BST_UNCHECKED);

			if (GetIpsNumPatches()) {
				if (!nShowMVSCartsOnly) {
					EnableWindow(GetDlgItem(hDlg, IDC_SEL_IPSMANAGER), TRUE);
					INT32 nActivePatches = LoadIpsActivePatches();

					// Whether IDC_SEL_APPLYIPS is enabled must be subordinate to IDC_SEL_IPSMANAGER
					// to verify that xxx.dat is not removed after saving config.
					// Reduce useless array lookups.
					EnableWindow(GetDlgItem(hDlg, IDC_SEL_APPLYIPS), nActivePatches);
				}
			} else {
				EnableWindow(GetDlgItem(hDlg, IDC_SEL_IPSMANAGER), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_SEL_APPLYIPS),   FALSE);	// xxx.dat path not found, must be disabled.
			}

			// Get the text from the drivers via BurnDrvGetText()
			for (int i = 0; i < 6; i++) {
				int nGetTextFlags = nLoadMenuShowY & ASCIIONLY ? DRV_ASCIIONLY : 0;
				TCHAR szItemText[256];
				szItemText[0] = _T('\0');

				switch (i) {
					case 0: {
						bool bBracket = false;

						_stprintf(szItemText, _T("%s"), BurnDrvGetText(DRV_NAME));
						if ((BurnDrvGetFlags() & BDF_CLONE) && BurnDrvGetTextA(DRV_PARENT)) {
							int nOldDrvSelect = nBurnDrvActive;
							TCHAR* pszName = BurnDrvGetText(DRV_PARENT);

							_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_CLONE_OF, true), BurnDrvGetText(DRV_PARENT));

							for (nBurnDrvActive = 0; nBurnDrvActive < nBurnDrvCount; nBurnDrvActive++) {
								if (!_tcsicmp(pszName, BurnDrvGetText(DRV_NAME))) {
									break;
								}
							}
							if (nBurnDrvActive < nBurnDrvCount) {
								if (BurnDrvGetText(DRV_PARENT)) {
									_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_ROMS_FROM_1, true), BurnDrvGetText(DRV_PARENT));
								}
							}
							nBurnDrvActive = nOldDrvSelect;
							bBracket = true;
						} else {
							if (BurnDrvGetTextA(DRV_PARENT)) {
								_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_ROMS_FROM_2, true), bBracket ? _T(", ") : _T(" ("), BurnDrvGetText(DRV_PARENT));
								bBracket = true;
							}
						}
						if (BurnDrvGetTextA(DRV_SAMPLENAME)) {
							_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_SAMPLES_FROM, true), bBracket ? _T(", ") : _T(" ("), BurnDrvGetText(DRV_SAMPLENAME));
							bBracket = true;
						}
						if (bBracket) {
							_stprintf(szItemText + _tcslen(szItemText), _T(")"));
						}
						SendMessage(hInfoText[i], WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
						EnableWindow(hInfoLabel[i], TRUE);
						break;
					}
					case 1: {
						bool bUseInfo = false;

						if (BurnDrvGetFlags() & BDF_PROTOTYPE) {
							_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_SEL_PROTOTYPE, true));
							bUseInfo = true;
						}
						if (BurnDrvGetFlags() & BDF_BOOTLEG) {
							_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_SEL_BOOTLEG, true), bUseInfo ? _T(", ") : _T(""));
							bUseInfo = true;
						}
						if (BurnDrvGetFlags() & BDF_HACK) {
							_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_SEL_HACK, true), bUseInfo ? _T(", ") : _T(""));
							bUseInfo = true;
						}
						if (BurnDrvGetFlags() & BDF_HOMEBREW) {
							_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_SEL_HOMEBREW, true), bUseInfo ? _T(", ") : _T(""));
							bUseInfo = true;
						}
						if (BurnDrvGetFlags() & BDF_DEMO) {
							_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_SEL_DEMO, true), bUseInfo ? _T(", ") : _T(""));
							bUseInfo = true;
						}
						TCHAR szPlayersMax[100];
						_stprintf(szPlayersMax, FBALoadStringEx(hAppInst, IDS_NUM_PLAYERS_MAX, true));
						_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_NUM_PLAYERS, true), bUseInfo ? _T(", ") : _T(""), BurnDrvGetMaxPlayers(), (BurnDrvGetMaxPlayers() != 1) ? szPlayersMax : _T(""));
						bUseInfo = true;
						if (BurnDrvGetText(DRV_BOARDROM)) {
							_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_BOARD_ROMS_FROM, true), bUseInfo ? _T(", ") : _T(""), BurnDrvGetText(DRV_BOARDROM));
							SendMessage(hInfoText[i], WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
							EnableWindow(hInfoLabel[i], TRUE);
							bUseInfo = true;
						}
						SendMessage(hInfoText[i], WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
						EnableWindow(hInfoLabel[i], bUseInfo);
						break;
					}
					case 2: {
						TCHAR szUnknown[100];
						TCHAR szCartridge[100];
						_stprintf(szUnknown, FBALoadStringEx(hAppInst, IDS_ERR_UNKNOWN, true));
						_stprintf(szCartridge, FBALoadStringEx(hAppInst, IDS_MVS_CARTRIDGE, true));
						_stprintf(szItemText, FBALoadStringEx(hAppInst, IDS_HARDWARE_DESC, true), BurnDrvGetTextA(DRV_MANUFACTURER) ? BurnDrvGetText(nGetTextFlags | DRV_MANUFACTURER) : szUnknown, BurnDrvGetText(DRV_DATE), (((BurnDrvGetHardwareCode() & HARDWARE_SNK_MVS) == HARDWARE_SNK_MVS) && ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK)) == HARDWARE_SNK_NEOGEO)? szCartridge : BurnDrvGetText(nGetTextFlags | DRV_SYSTEM));
						SendMessage(hInfoText[i], WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
						EnableWindow(hInfoLabel[i], TRUE);
						break;
					}
					case 3: {
						TCHAR szText[1024] = _T("");
						TCHAR* pszPosition = szText;
						TCHAR* pszName = BurnDrvGetText(nGetTextFlags | DRV_FULLNAME);

						pszPosition += _sntprintf(szText, 1024, pszName);

						pszName = BurnDrvGetText(nGetTextFlags | DRV_FULLNAME);
						while ((pszName = BurnDrvGetText(nGetTextFlags | DRV_NEXTNAME | DRV_FULLNAME)) != NULL) {
							if (pszPosition + _tcslen(pszName) - 1024 > szText) {
								break;
							}
							pszPosition += _stprintf(pszPosition, _T(SEPERATOR_2) _T("%s"), pszName);
						}
						SendMessage(hInfoText[i], WM_SETTEXT, (WPARAM)0, (LPARAM)szText);
						if (szText[0]) {
							EnableWindow(hInfoLabel[i], TRUE);
						} else {
							EnableWindow(hInfoLabel[i], FALSE);
						}
						break;
					}
					case 4: {
						_stprintf(szItemText, _T("%s"), BurnDrvGetTextA(DRV_COMMENT) ? BurnDrvGetText(nGetTextFlags | DRV_COMMENT) : _T(""));
						if (BurnDrvGetFlags() & BDF_HISCORE_SUPPORTED) {
							_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_HISCORES_SUPPORTED, true), _tcslen(szItemText) ? _T(", ") : _T(""));
						}
						SendMessage(hInfoText[i], WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
						EnableWindow(hInfoLabel[i], TRUE);
						break;
					}

					case 5: {
						_stprintf(szItemText, _T("%s"), DecorateGenreInfo());
						SendMessage(hInfoText[i], WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
						EnableWindow(hInfoLabel[i], TRUE);
						break;
					}
				}
			}
		}

		if(!TreeBuilding && pnmtv->hdr.code == NM_RCLICK && pnmtv->hdr.idFrom == IDC_TREE1)
		{
			DWORD dwpos = GetMessagePos();

			TVHITTESTINFO thi;
			thi.pt.x	= GET_X_LPARAM(dwpos);
			thi.pt.y	= GET_Y_LPARAM(dwpos);

			MapWindowPoints(HWND_DESKTOP, pNmHdr->hwndFrom, &thi.pt, 1);

			TreeView_HitTest(pNmHdr->hwndFrom, &thi);

			HTREEITEM hSelectHandle = thi.hItem;
			if(hSelectHandle == NULL) return 1;

			TreeView_SelectItem(hSelList, hSelectHandle);

			// Search through nBurnDrv[] for the nBurnDrvNo according to the returned hSelectHandle
			for (unsigned int i = 0; i < nTmpDrvCount; i++) {
				if (hSelectHandle == nBurnDrv[i].hTreeHandle) {
					nBurnDrvSelect[0] = nBurnDrv[i].nBurnDrvNo;
					break;
				}
			}

			nDialogSelect	= nBurnDrvSelect[0];
			bDrvSelected	= true;
			UpdatePreview(true, szAppPreviewsPath, IDC_SCREENSHOT_H, IDC_SCREENSHOT_V);
			UpdatePreview(false, szAppTitlesPath, IDC_SCREENSHOT2_H, IDC_SCREENSHOT2_V);

			// Menu
			POINT oPoint;
			GetCursorPos(&oPoint);

			HMENU hMenuLoad = FBALoadMenu(hAppInst, MAKEINTRESOURCE(IDR_MENU_GAMESEL));
			HMENU hMenuX = GetSubMenu(hMenuLoad, 0);

			CheckMenuItem(hMenuX, GAMESEL_MENU_FAVORITE, (CheckFavorites(BurnDrvGetTextA(DRV_NAME)) != -1) ? MF_CHECKED : MF_UNCHECKED);

			TrackPopupMenu(hMenuX, TPM_LEFTALIGN | TPM_RIGHTBUTTON, oPoint.x, oPoint.y, 0, hSelDlg, NULL);
			DestroyMenu(hMenuLoad);

			return 1;
		}
	}
	return 0;
}

int SelDialog(int nMVSCartsOnly, HWND hParentWND)
{
	int nOldSelect = nBurnDrvActive;

	if(bDrvOkay) {
		nOldDlgSelected = nBurnDrvActive;
	}

	hParent = hParentWND;
	nShowMVSCartsOnly = nMVSCartsOnly;
	RomDataStateBackup();

	FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_SELNEW), hParent, (DLGPROC)DialogProc);

	hSelDlg = NULL;
	hSelList = NULL;

	if (nBurnDrv) {
		free(nBurnDrv);
		nBurnDrv = NULL;
	}

	nBurnDrvActive = nOldSelect;

	return nDialogSelect;
}

// -----------------------------------------------------------------------------

static HBITMAP hMVSpreview[6];
static unsigned int nPrevDrvSelect[6];

static void UpdateInfoROMInfo()
{
//	int nGetTextFlags = nLoadMenuShowY & ASCIIONLY ? DRV_ASCIIONLY : 0;
	TCHAR szItemText[256] = _T("");
	bool bBracket = false;

	_stprintf(szItemText, _T("%s"), BurnDrvGetText(DRV_NAME));
	if ((BurnDrvGetFlags() & BDF_CLONE) && BurnDrvGetTextA(DRV_PARENT)) {
		int nOldDrvSelect = nBurnDrvActive;
		TCHAR* pszName = BurnDrvGetText(DRV_PARENT);

		_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_CLONE_OF, true), BurnDrvGetText(DRV_PARENT));

		for (nBurnDrvActive = 0; nBurnDrvActive < nBurnDrvCount; nBurnDrvActive++) {
			if (!_tcsicmp(pszName, BurnDrvGetText(DRV_NAME))) {
				break;
			}
		}
		if (nBurnDrvActive < nBurnDrvCount) {
			if (BurnDrvGetText(DRV_PARENT)) {
				_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_ROMS_FROM_1, true), BurnDrvGetText(DRV_PARENT));
			}
		}
		nBurnDrvActive = nOldDrvSelect;
		bBracket = true;
	} else {
		if (BurnDrvGetTextA(DRV_PARENT)) {
			_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_ROMS_FROM_2, true), bBracket ? _T(", ") : _T(" ("), BurnDrvGetText(DRV_PARENT));
			bBracket = true;
		}
	}
	if (BurnDrvGetText(DRV_BOARDROM)) {
		_stprintf(szItemText + _tcslen(szItemText), FBALoadStringEx(hAppInst, IDS_BOARD_ROMS_FROM, true), bBracket ? _T(", ") : _T(" ("), BurnDrvGetText(DRV_BOARDROM));
		bBracket = true;
	}
	if (bBracket) {
		_stprintf(szItemText + _tcslen(szItemText), _T(")"));
	}

	if (hInfoText[0]) {
		SendMessage(hInfoText[0], WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
	}
	if (hInfoLabel[0]) {
		EnableWindow(hInfoLabel[0], TRUE);
	}

	return;
}

static void UpdateInfoRelease()
{
	int nGetTextFlags = nLoadMenuShowY & ASCIIONLY ? DRV_ASCIIONLY : 0;
	TCHAR szItemText[256] = _T("");

	TCHAR szUnknown[100];
	TCHAR szCartridge[100];
	TCHAR szHardware[100];
	_stprintf(szUnknown, FBALoadStringEx(hAppInst, IDS_ERR_UNKNOWN, true));
	_stprintf(szCartridge, FBALoadStringEx(hAppInst, IDS_CARTRIDGE, true));
	_stprintf(szHardware, FBALoadStringEx(hAppInst, IDS_HARDWARE, true));
	_stprintf(szItemText, _T("%s (%s, %s %s)"), BurnDrvGetTextA(DRV_MANUFACTURER) ? BurnDrvGetText(nGetTextFlags | DRV_MANUFACTURER) : szUnknown, BurnDrvGetText(DRV_DATE),
		BurnDrvGetText(nGetTextFlags | DRV_SYSTEM), (BurnDrvGetHardwareCode() & HARDWARE_PREFIX_CARTRIDGE) ? szCartridge : szHardware);

	if (hInfoText[2]) {
		SendMessage(hInfoText[2], WM_SETTEXT, (WPARAM)0, (LPARAM)szItemText);
	}
	if (hInfoLabel[2]) {
		EnableWindow(hInfoLabel[2], TRUE);
	}

	return;
}

static void UpdateInfoGameInfo()
{
	int nGetTextFlags = nLoadMenuShowY & ASCIIONLY ? DRV_ASCIIONLY : 0;
	TCHAR szText[1024] = _T("");
	TCHAR* pszPosition = szText;
	TCHAR* pszName = BurnDrvGetText(nGetTextFlags | DRV_FULLNAME);

	pszPosition += _sntprintf(szText, 1024, pszName);

	pszName = BurnDrvGetText(nGetTextFlags | DRV_FULLNAME);
	while ((pszName = BurnDrvGetText(nGetTextFlags | DRV_NEXTNAME | DRV_FULLNAME)) != NULL) {
		if (pszPosition + _tcslen(pszName) - 1024 > szText) {
			break;
		}
		pszPosition += _stprintf(pszPosition, _T(SEPERATOR_2) _T("%s"), pszName);
	}
	if (BurnDrvGetText(nGetTextFlags | DRV_COMMENT)) {
		pszPosition += _sntprintf(pszPosition, szText + 1024 - pszPosition, _T(SEPERATOR_1) _T("%s"), BurnDrvGetText(nGetTextFlags | DRV_COMMENT));
	}

	if (hInfoText[3]) {
		SendMessage(hInfoText[3], WM_SETTEXT, (WPARAM)0, (LPARAM)szText);
	}
	if (hInfoLabel[3]) {
		if (szText[0]) {
			EnableWindow(hInfoLabel[3], TRUE);
		} else {
			EnableWindow(hInfoLabel[3], FALSE);
		}
	}

	return;
}

static int MVSpreviewUpdateSlot(int nSlot, HWND hDlg)
{
	int nOldSelect = nBurnDrvActive;

	if (nSlot >= 0 && nSlot < 6) {
		hInfoLabel[0] = 0; hInfoLabel[1] = 0; hInfoLabel[2] = 0; hInfoLabel[3] = 0;
		hInfoText[0] = GetDlgItem(hDlg, IDC_MVS_TEXTROMNAME1 + nSlot);
		hInfoText[1] = 0;
		hInfoText[2] = GetDlgItem(hDlg, IDC_MVS_TEXTSYSTEM1 + nSlot);
		hInfoText[3] = GetDlgItem(hDlg, IDC_MVS_TEXTCOMMENT1 + nSlot);

		for (int j = 0; j < 4; j++) {
			if (hInfoText[j]) {
				SendMessage(hInfoText[j], WM_SETTEXT, (WPARAM)0, (LPARAM)_T(""));
			}
			if (hInfoLabel[j]) {
				EnableWindow(hInfoLabel[j], FALSE);
			}
		}

		nBurnDrvActive = nBurnDrvSelect[nSlot];
		if (nBurnDrvActive < nBurnDrvCount) {

			FILE* fp = OpenPreview(0, szAppTitlesPath);
			if (fp) {
				hMVSpreview[nSlot] = PNGLoadBitmap(hDlg, fp, 72, 54, 5);
				fclose(fp);
			} else {
				hMVSpreview[nSlot] = PNGLoadBitmap(hDlg, NULL, 72, 54, 4);
			}

			UpdateInfoROMInfo();
			UpdateInfoRelease();
			UpdateInfoGameInfo();
			nPrevDrvSelect[nSlot] = nBurnDrvActive;

		} else {
			hMVSpreview[nSlot] = PNGLoadBitmap(hDlg, NULL, 72, 54, 4);
			SendMessage(hInfoText[0], WM_SETTEXT, (WPARAM)0, (LPARAM)FBALoadStringEx(hAppInst, IDS_EMPTY, true));
		}

		SendDlgItemMessage(hDlg, IDC_MVS_CART1 + nSlot, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hMVSpreview[nSlot]);
	}

	nBurnDrvActive = nOldSelect;

	return 0;
}

static int MVSpreviewEndDialog()
{
	for (int i = 0; i < 6; i++) {
		DeleteObject((HGDIOBJ)hMVSpreview[i]);
		hMVSpreview[i] = NULL;
	}

	return 0;
}

static INT_PTR CALLBACK MVSpreviewProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM)
{
	switch (Msg) {
		case WM_INITDIALOG: {

			nDialogSelect = ~0U;

			for (int i = 0; i < 6; i++) {
				nPrevDrvSelect[i] = nBurnDrvSelect[i];
				MVSpreviewUpdateSlot(i, hDlg);
			}

			WndInMid(hDlg, hScrnWnd);

			return TRUE;
		}
		case WM_COMMAND:
			if (LOWORD(wParam) == IDC_VALUE_CLOSE) {
				SendMessage(hDlg, WM_CLOSE, 0, 0);
				break;
			}
			if (HIWORD(wParam) == BN_CLICKED) {
				if (LOWORD(wParam) == IDOK) {
					if (nPrevDrvSelect[0] == ~0U) {
						MessageBox(hSelDlg, FBALoadStringEx(hAppInst, IDS_ERR_NO_DRIVER_SEL_SLOT1, true), FBALoadStringEx(hAppInst, IDS_ERR_ERROR, true), MB_OK);
						break;
					}
					MVSpreviewEndDialog();
					for (int i = 0; i < 6; i++) {
						nBurnDrvSelect[i] = nPrevDrvSelect[i];
					}
					EndDialog(hDlg, 0);
					break;
				}
				if (LOWORD(wParam) == IDCANCEL) {
					SendMessage(hDlg, WM_CLOSE, 0, 0);
					break;
				}

				if (LOWORD(wParam) >= IDC_MVS_CLEAR1 && LOWORD(wParam) <= IDC_MVS_CLEAR6) {
					int nSlot = LOWORD(wParam) - IDC_MVS_CLEAR1;

					nBurnDrvSelect[nSlot] = ~0U;
					nPrevDrvSelect[nSlot] = ~0U;
					MVSpreviewUpdateSlot(nSlot, hDlg);
					break;
				}

				if (LOWORD(wParam) >= IDC_MVS_SELECT1 && LOWORD(wParam) <= IDC_MVS_SELECT6) {
					int nSlot = LOWORD(wParam) - IDC_MVS_SELECT1;

					nBurnDrvSelect[nSlot] = SelDialog(HARDWARE_PREFIX_CARTRIDGE | HARDWARE_SNK_NEOGEO, hDlg);
					MVSpreviewUpdateSlot(nSlot, hDlg);
					break;
				}
			}
			break;

		case WM_CLOSE: {
			MVSpreviewEndDialog();
			for (int i = 0; i < 6; i++) {
				nBurnDrvSelect[i] = nPrevDrvSelect[i];
			}
			EndDialog(hDlg, 1);
			break;
		}
	}

	return 0;
}

int SelMVSDialog()
{
	return FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_MVS_SELECT_CARTS), hScrnWnd, (DLGPROC)MVSpreviewProc);
}
