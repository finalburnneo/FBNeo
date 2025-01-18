// ---------------------------------------------------------------------------------------
// NeoGeo CD Game Info Module (by CaptainCPS-X)
// ---------------------------------------------------------------------------------------
#include "burner.h"
#include "neocdlist.h"

#define DEBUG_CD    0

const int nSectorLength = 2352;

NGCDGAME* game;

struct NGCDGAME games[] =
{
// ---------------------------------------------------------------------------------------------------------------------------------------------//
//	* Name				* Title														* Year			* Company					* Game ID		//
// ---------------------------------------------------------------------------------------------------------------------------------------------//
	{ _T("placeholder")	, _T("Empty Placeholder")									, _T("")		, _T("")					, 0x0000 },		//
	{ _T("nam1975")		, _T("NAM-1975")											, _T("1990")	, _T("SNK")					, 0x0001 },		//
	{ _T("bstars")		, _T("Baseball Stars Professional")							, _T("1991")	, _T("SNK")					, 0x0002 },		//
	{ _T("tpgolf")		, _T("Top Player's Golf")									, _T("1990")	, _T("SNK")					, 0x0003 },		//
	{ _T("mahretsu")	, _T("Mahjong Kyo Retsuden - Nishi Nihon Hen")				, _T("1990")	, _T("SNK")					, 0x0004 },		//
	{ _T("maglord")		, _T("Magician Lord")										, _T("1990")	, _T("ADK")					, 0x0005 },		//
	{ _T("ridhero")		, _T("Riding Hero")											, _T("1991")	, _T("SNK")					, 0x0006 },		//
	{ _T("alpham2")		, _T("Alpha Mission II / ASO II - Last Guardian")			, _T("1994")	, _T("SNK")					, 0x0007 },		//
	{ _T("ncombat")		, _T("Ninja Combat")										, _T("1990")	, _T("SNK/ADK")				, 0x0009 },		//
	{ _T("cyberlip")	, _T("Cyber-Lip")											, _T("1991")	, _T("SNK")					, 0x0010 },		//
	{ _T("superspy")	, _T("The Super Spy")										, _T("1990")	, _T("SNK")					, 0x0011 },		//
	{ _T("mutnat")		, _T("Mutation Nation")										, _T("1995")	, _T("SNK")					, 0x0014 },		//
	{ _T("sengoku")		, _T("Sengoku / Sengoku Denshou")							, _T("1994")	, _T("SNK")					, 0x0017 },		//
	{ _T("burningf")	, _T("Burning Fight")										, _T("1994")	, _T("SNK")					, 0x0018 },		//
	{ _T("lbowling")	, _T("League Bowling")										, _T("1994")	, _T("SNK")					, 0x0019 },		//
	{ _T("gpilots")		, _T("Ghost Pilots")										, _T("1991")	, _T("SNK")					, 0x0020 },		//
	{ _T("joyjoy")		, _T("Puzzled / Joy Joy Kid")								, _T("1990")	, _T("SNK")					, 0x0021 },		//
	{ _T("bjourney")	, _T("Blue's Journey / Raguy")								, _T("1990")	, _T("SNK/ADK")				, 0x0022 },		//
	{ _T("lresort")		, _T("Last Resort")											, _T("1994")	, _T("SNK")					, 0x0024 },		//
	{ _T("2020bb")		, _T("2020 Super Baseball")									, _T("1994")	, _T("SNK")					, 0x0030 },		//
	{ _T("socbrawl")	, _T("Soccer Brawl")										, _T("1991")	, _T("SNK")					, 0x0031 },		//
	{ _T("roboarmy")	, _T("Robo Army")											, _T("1991")	, _T("SNK")					, 0x0032 },		//
	{ _T("fatfury")		, _T("Fatal Fury - The Battle of Fury")						, _T("1994")	, _T("SNK")					, 0x0033 },		//
	{ _T("fbfrenzy")	, _T("Football Frenzy")										, _T("1994")	, _T("SNK")					, 0x0034 },		//
	{ _T("crswords")	, _T("Crossed Swords")										, _T("1994")	, _T("SNK/ADK")				, 0x0037 },		//
	{ _T("rallych")		, _T("Rally Chase")											, _T("1991")	, _T("SNK/ADK")				, 0x0038 },		//
	{ _T("kotm2")		, _T("King of the Monsters 2")								, _T("1992")	, _T("SNK")					, 0x0039 },		//
	{ _T("sengoku2")	, _T("Sengoku 2 / Sengoku Denshou 2")						, _T("1994")	, _T("SNK")					, 0x0040 },		//
	{ _T("bstars2")		, _T("Baseball Stars 2")									, _T("1992")	, _T("SNK")					, 0x0041 },		//
	{ _T("3countb")		, _T("3 Count Bout / Fire Suplex")							, _T("1995")	, _T("SNK")					, 0x0043 },		//
	{ _T("aof")			, _T("Art of Fighting / Ryuuko no Ken")						, _T("1994")	, _T("SNK")					, 0x0044 },		//
	{ _T("samsho")		, _T("Samurai Shodown / Samurai Spirits")					, _T("1993")	, _T("SNK")					, 0x0045 },		//
	{ _T("tophuntr")	, _T("Top Hunter - Roddy & Cathy")							, _T("1994")	, _T("SNK")					, 0x0046 },		//
	{ _T("fatfury2")	, _T("Fatal Fury 2 / Garou Densetsu 2 - Aratanaru Tatakai")	, _T("1994")	, _T("SNK")					, 0x0047 },		//
	{ _T("janshin")		, _T("Janshin Densetsu - Quest of the Jongmaster")			, _T("1995")	, _T("Yubis")				, 0x0048 },		//
	{ _T("androdunos")	, _T("Andro Dunos")											, _T("1992")	, _T("Visco")				, 0x0049 },		//
	{ _T("treascb")		, _T("Treasure of the Caribbean")							, _T("1994")	, _T("FACE/NCI")			, 0x1048 },		// custom id
	{ _T("ncommand")	, _T("Ninja Commando")										, _T("1992")	, _T("SNK")					, 0x0050 },		//
	{ _T("viewpoin")	, _T("Viewpoint")											, _T("1992")	, _T("Sammy/Aicom")			, 0x0051 },		//
	{ _T("ssideki")		, _T("Super Sidekicks / Tokuten Oh")						, _T("1993")	, _T("SNK")					, 0x0052 },		//
	{ _T("wh1")			, _T("World Heroes")										, _T("1992")	, _T("ADK")					, 0x0053 },		//
	{ _T("crsword2")	, _T("Crossed Swords II")									, _T("1995")	, _T("SNK/ADK")				, 0x0054 },		//
	{ _T("kof94")		, _T("The King of Fighters '94 (JP)")						, _T("1994")	, _T("SNK")					, 0x0055 },		//
	{ _T("kof94ju")		, _T("The King of Fighters '94 (JP-US)")					, _T("1994")	, _T("SNK")					, 0x1055 },		// custom id
	{ _T("aof2")		, _T("Art of Fighting 2 / Ryuuko no Ken 2")					, _T("1994")	, _T("SNK")					, 0x0056 },		//
	{ _T("wh2")			, _T("World Heroes 2")										, _T("1995")	, _T("SNK/ADK")				, 0x0057 },		//
	{ _T("fatfursp")	, _T("Fatal Fury Special / Garou Densetsu Special")			, _T("1994")	, _T("SNK")					, 0x0058 },		//
	{ _T("fatfurspr1")	, _T("Fatal Fury Special / Garou Densetsu Special (Rev 1)")	, _T("1994")	, _T("SNK")					, 0x1058 },		//
	{ _T("savagere")	, _T("Savage Reign / Fu'un Mokujiroku - Kakutou Sousei")	, _T("1995")	, _T("SNK")					, 0x0059 },		//
	{ _T("savagerer1")	, _T("Savage Reign / Fu'un Mokujiroku - Kakutou Sousei (Rev 1)"), _T("1995")	, _T("SNK")					, 0x1059 },		//
	{ _T("ssideki2")	, _T("Super Sidekicks 2 / Tokuten Oh 2")					, _T("1994")	, _T("SNK")					, 0x0061 },		//
	{ _T("samsho2")		, _T("Samurai Shodown 2 / Shin Samurai Spirits")			, _T("1994")	, _T("SNK")					, 0x0063 },		//
	{ _T("wh2j")		, _T("World Heroes 2 Jet")									, _T("1995")	, _T("SNK/ADK")				, 0x0064 },		//
	{ _T("wjammers")	, _T("Windjammers / Flying Power Disc")						, _T("1994")	, _T("Data East")			, 0x0065 },		//
	{ _T("karnovr")		, _T("Karnov's Revenge / Fighters History Dynamite")		, _T("1994")	, _T("Data East")			, 0x0066 },		//
	{ _T("pspikes2")	, _T("Power Spikes II")										, _T("1994")	, _T("Video System")		, 0x0068 },		//
	{ _T("b2b")			, _T("Bang Bang Busters")									, _T("2000")	, _T("Visco")				, 0x0071 },		//
	{ _T("aodk")		, _T("Aggressors of Dark Kombat / Tsuukai GanGan Koushinkyoku")	, _T("1994"), _T("SNK/ADK")				, 0x0074 },		//
	{ _T("sonicwi2")	, _T("Aero Fighters 2 / Sonic Wings 2")						, _T("1994")	, _T("Video System")		, 0x0075 },		//
	{ _T("galaxyfg")	, _T("Galaxy Fight - Universal Warriors")					, _T("1995")	, _T("Sunsoft")				, 0x0078 },		//
	{ _T("strhoop")		, _T("Street Hoop / Dunk Dream")							, _T("1994")	, _T("Data East")			, 0x0079 },		//
	{ _T("quizkof")		, _T("Quiz King of Fighters")								, _T("1993")	, _T("SNK/Saurus")			, 0x0080 },		//
	{ _T("ssideki3")	, _T("Super Sidekicks 3 - The Next Glory / Tokuten Oh 3 - Eikoue No Chousen"), _T("1995") , _T("SNK")	, 0x0081 },		//
	{ _T("doubledr")	, _T("Double Dragon")										, _T("1995")	, _T("Technos")				, 0x0082 },		//
	{ _T("doubledrr1")	, _T("Double Dragon (Rev 1)")								, _T("1995")	, _T("Technos")				, 0x1082 },		//
	{ _T("doubledrps1")	, _T("Double Dragon (PS1 OST)")								, _T("1995")	, _T("Technos")				, 0x2082 },		//
	{ _T("pbobblen")	, _T("Puzzle Bobble / Bust-A-Move")							, _T("1994")	, _T("SNK")					, 0x0083 },		//
	{ _T("kof95")		, _T("The King of Fighters '95 (JP-US)")					, _T("1995")	, _T("SNK")					, 0x0084 },		//
	{ _T("kof95r1")		, _T("The King of Fighters '95 (JP-US)(Rev 1)")				, _T("1995")	, _T("SNK")					, 0x1084 },		//
	{ _T("ssrpg")		, _T("Samurai Shodown RPG / Shinsetsu Samurai Spirits - Bushidohretsuden")		, _T("1997")	, _T("SNK")					, 0x0085 },		//
	{ _T("ssrpgen")		, _T("Samurai Shodown RPG (English Translation)")			, _T("1997")	, _T("SNK")					, 0x1085 },		//
	{ _T("ssrpgfr")		, _T("Samurai Shodown RPG (French Translation)")			, _T("1997")	, _T("SNK")					, 0x2085 },		//
	{ _T("samsho3")		, _T("Samurai Shodown 3 / Samurai Spirits 3")				, _T("1995")	, _T("SNK")					, 0x0087 },		//
	{ _T("stakwin")		, _T("Stakes Winner - GI Kanzen Seiha Heno Machi")			, _T("1995")	, _T("Saurus")				, 0x0088 },		//
	{ _T("pulstar")		, _T("Pulstar")												, _T("1995")	, _T("Aicom")				, 0x0089 },		//
	{ _T("whp")			, _T("World Heroes Perfect")								, _T("1995")	, _T("ADK")					, 0x0090 },		//
	{ _T("whpr1")		, _T("World Heroes Perfect (Rev 1)")						, _T("1995")	, _T("ADK")					, 0x1090 },		//
	{ _T("kabukikl")	, _T("Kabuki Klash / Tengai Makyou Shinden - Far East of Eden")	, _T("1995"), _T("Hudson")				, 0x0092 },		//
	{ _T("gowcaizr")	, _T("Voltage Fighter Gowcaizer / Choujin Gakuen Gowcaizer"), _T("1995")	, _T("Technos")				, 0x0094 },		//
	{ _T("rbff1")		, _T("Real Bout Fatal Fury")								, _T("1995")	, _T("SNK")					, 0x0095 },		//
	{ _T("aof3")		, _T("Art of Fighting 3: Path of the Warrior")				, _T("1996")	, _T("SNK")					, 0x0096 },		//
	{ _T("sonicwi3")	, _T("Aero Fighters 3 / Sonic Wings 3")						, _T("1995")	, _T("SNK")					, 0x0097 },		//
	{ _T("fromanc2")	, _T("Idol Mahjong Final Romance 2")						, _T("1995")	, _T("Video Systems")		, 0x0098 },		//
	{ _T("turfmast")	, _T("Neo Turf Masters / Big Tournament Golf")				, _T("1996")	, _T("Nazca")				, 0x0200 },		//
	{ _T("mslug")		, _T("Metal Slug - Super Vehicle-001")						, _T("1996")	, _T("Nazca")				, 0x0201 },		//
	{ _T("mosyougi")	, _T("Shougi no Tatsujin - Master of Syougi")				, _T("1995")	, _T("ADK")					, 0x0203 },		//
	{ _T("adkworld")	, _T("ADK World / ADK Special")								, _T("1995")	, _T("ADK")					, 0x0204 },		//
	{ _T("ngcdsp")		, _T("Neo Geo CD Special")									, _T("1995")	, _T("SNK")					, 0x0205 },		//
	{ _T("zintrick")	, _T("Zintrick / Oshidashi Zintrick")						, _T("1996")	, _T("ADK")					, 0x0211 },		//
	{ _T("overtop")		, _T("OverTop")												, _T("1996")	, _T("ADK")					, 0x0212 },		//
	{ _T("neodrift")	, _T("Neo DriftOut")										, _T("1996")	, _T("Visco")				, 0x0213 },		//
	{ _T("kof96")		, _T("The King of Fighters '96")							, _T("1996")	, _T("SNK")					, 0x0214 },		//
	{ _T("ninjamas")	, _T("Ninja Master's - Haou Ninpou-Chou")					, _T("1996")	, _T("ADK/SNK")				, 0x0217 },		//
	{ _T("ragnagrd")	, _T("Ragnagard / Shinouken")								, _T("1996")	, _T("Saurus")				, 0x0218 },		//
	{ _T("pgoal")		, _T("Pleasuregoal - 5 on 5 Mini Soccer / Futsal")			, _T("1996")	, _T("Saurus")				, 0x0219 },		//
	{ _T("ironclad")	, _T("Ironclad / Choutetsu Brikin'ger")						, _T("1996")	, _T("Saurus")				, 0x0220 },		//
	{ _T("magdrop2")	, _T("Magical Drop 2")										, _T("1996")	, _T("Data East")			, 0x0221 },		//
	{ _T("samsho4")		, _T("Samurai Shodown IV - Amakusa's Revenge")				, _T("1996")	, _T("SNK")					, 0x0222 },		//
	{ _T("rbffspec")	, _T("Real Bout Fatal Fury Special")						, _T("1996")	, _T("SNK")					, 0x0223 },		//
	{ _T("twinspri")	, _T("Twinkle Star Sprites")								, _T("1996")	, _T("ADK")					, 0x0224 },		//
	{ _T("kof96ngc")	, _T("The King of Fighters '96 NEOGEO Collection")			, _T("1996")	, _T("SNK")					, 0x0229 },		//
	{ _T("breakers")	, _T("Breakers")											, _T("1996")	, _T("Visco")				, 0x0230 },		//
	{ _T("kof97")		, _T("The King of Fighters '97")							, _T("1997")	, _T("SNK")					, 0x0232 },		//
	{ _T("lastblad")	, _T("The Last Blade / Bakumatsu Roman - Gekka no Kenshi")	, _T("1997")	, _T("SNK")					, 0x0234 },		//
	{ _T("rbff2")		, _T("Real Bout Fatal Fury 2 - The Newcomers / Real Bout Garou Densetsu 2 - The Newcomers"), _T("1998"), _T("SNK") , 0x0240 },		//
	{ _T("mslug2")		, _T("Metal Slug 2 - Super Vehicle-001/II")					, _T("1998")	, _T("SNK")					, 0x0241 },		//
	{ _T("mslug2t")		, _T("Metal Slug 2 Turbo - Super Vehicle-001/II")			, _T("1998")	, _T("SNK")					, 0x0941 },		//
	{ _T("kof98")		, _T("The King of Fighters '98 - The Slugfest")				, _T("1998")	, _T("SNK")					, 0x0242 },		//
	{ _T("lastbld2")	, _T("The Last Blade 2")									, _T("1998")	, _T("SNK")					, 0x0243 },		//
	{ _T("kof99")		, _T("The King of Fighters '99 - Millennium Battle")		, _T("1999")	, _T("SNK")					, 0x0251 },		//
	{ _T("fatfury3")	, _T("Fatal Fury 3 - Road to the Final Victory / Garou Densetsu 3"), _T("1995"), _T("SNK"), 0x069c },		//
	{ _T("fatfury3r1")	, _T("Fatal Fury 3 - Road to the Final Victory / Garou Densetsu 3 (Rev 1)"), _T("1995"), _T("SNK"), 0x169c },		//
	{ _T("fatfury3r2")	, _T("Fatal Fury 3 - Road to the Final Victory / Garou Densetsu 3 (Rev 2)"), _T("1995"), _T("SNK"), 0x269c },		//
	{ _T("fatfury3r3")	, _T("Fatal Fury 3 - Road to the Final Victory / Garou Densetsu 3 (Rev 3)"), _T("1995"), _T("SNK"), 0x369c },		//
	{ _T("lasthope")	, _T("Last Hope")									        , _T("2007")	, _T("NG.DEV.TEAM")			, 0x0666 },		//
	{ _T("xenocrisis")	, _T("Xeno Crisis")									        , _T("2019")	, _T("Bitmap Bureau")		, 0xbb01 },		//
	{ _T("neon")		, _T("Project Neon: Caravan Demo")							, _T("2019")	, _T("Team Project Neon")	, 0x7777 },		//
	{ _T("looptris")	, _T("Looptris")											, _T("2019")	, _T("Blastar")				, 0x2019 },		//
	{ _T("looptrsp")	, _T("Looptris Plus")										, _T("2022")	, _T("Blastar")				, 0x2119 },		// not really nID == 2119, see szVolumeID check below
	{ _T("flappychick")	, _T("Flappy Chicken")										, _T("2023")	, _T("Blastar")				, 0x2022 },		//
	{ _T("hypernoid")	, _T("Hypernoid")											, _T("2022")	, _T("NeoHomeBrew.com")		, 0x0600 },		//
	{ _T("timesup")		, _T("Time's Up")											, _T("2012")	, _T("NGF DEV. INC.")		, 0x0276 },		//
	{ _T("pow")			, _T("P.O.W. - Prisoners of War")							, _T("2024")	, _T("SNK (iq_132 conversion)")	, 0x1324 },		//
	{ _T("karnov")		, _T("Karnov")												, _T("2024")	, _T("Data East (iq_132 conversion)")	, 0x0283 },		//
	{ _T("spinmast")	, _T("Spin Master")											, _T("2024")	, _T("Data East (iq_132 conversion)")	, 0x0062 },		//
	{ _T("chelnov")		, _T("Atomic Runner Chelnov")								, _T("2024")	, _T("Data East (iq_132 conversion)")	, 0x1320 },		//
	{ _T("eightman")	, _T("Eight Man")											, _T("2024")	, _T("SNK / Pallas (iq_132 conversion)")	, 0x0025 },		//
	{ _T("gururin")		, _T("Gururin")												, _T("2024")	, _T("Face (iq_132 conversion)")	, 0x0067 },		//
	{ _T("kotm")		, _T("King of the Monsters")								, _T("2024")	, _T("SNK (iq_132 conversion)")	, 0x0016 },		//
	{ _T("legendos")	, _T("Legend of Success Joe")								, _T("2024")	, _T("SNK / Wave (iq_132 conversion)")	, 0x0029 },		//
	{ _T("neomrdo")		, _T("Neo Mr. Do!")											, _T("2024")	, _T("Visco (iq_132 conversion)")	, 0x0207 },		//
	{ _T("pnyaa")		, _T("Pochi and Nyaa")										, _T("2024")	, _T("Aiky / Taito (iq_132 conversion)")	, 0x0267 },		//
	{ _T("sbp")			, _T("Super Bubble Pop")									, _T("2024")	, _T("Vektorlogic (iq_132 conversion)")	, 0xfedc },		//
	{ _T("zupapa")		, _T("Zupapa!")												, _T("2024")	, _T("SNK (iq_132 conversion)")	, 0x0070 },		//
	{ _T("zedblade")	, _T("Zed Blade - Operation Ragnarok")						, _T("2024")	, _T("NMK (iq_132 conversion)")	, 0x0076 },		//
	{ _T("shinobi")		, _T("Shinobi")												, _T("2024")	, _T("(Hoffman conversion)")	, 0x1337 },		//
	{ _T("cbarrel")		, _T("Captain Barrel")										, _T("2024")	, _T("Ozzy Ouzo)")	, 0x14A1 },		//
};

static char szVolumeID[64];

NGCDGAME* GetNeoGeoCDInfo(unsigned int nID)
{
	for(unsigned int nGame = 0; nGame < (sizeof(games) / sizeof(NGCDGAME)); nGame++) {
		if(nID == games[nGame].id ) {
			return &games[nGame];
		}
	}

	return NULL;
}

// Update the main window title
static void SetNeoCDTitle(TCHAR* pszTitle)
{
	TCHAR szText[1024] = _T("");
	_stprintf(szText, _T(APP_TITLE) _T( " v%.20s") _T(SEPERATOR_1) _T("%s") _T(SEPERATOR_1) _T("%s"), szAppBurnVer, BurnDrvGetText(DRV_FULLNAME), pszTitle);

	SetWindowText(hScrnWnd, szText);
}

void NeoCDInfo_SetTitle()
{
	if (IsNeoGeoCD() && game && game->id) {
		SetNeoCDTitle(game->pszTitle);
	} else if (IsNeoGeoCD()) {
		SetNeoCDTitle(FBALoadStringEx(hAppInst, IDS_UNIDENTIFIED_CD, true));
	}
}

// Get the title
int GetNeoCDTitle(unsigned int nGameID)
{
	game = (NGCDGAME*)malloc(sizeof(NGCDGAME));
	memset(game, 0, sizeof(NGCDGAME));

	if(GetNeoGeoCDInfo(nGameID)) {
		memcpy(game, GetNeoGeoCDInfo(nGameID), sizeof(NGCDGAME));

		bprintf(PRINT_NORMAL, _T("    Title: %s \n")		, game->pszTitle);
		bprintf(PRINT_NORMAL, _T("    Shortname: %s \n")	, game->pszName);
		bprintf(PRINT_NORMAL, _T("    Year: %s \n")			, game->pszYear);
		bprintf(PRINT_NORMAL, _T("    Company: %s \n")		, game->pszCompany);

		// Update the main window title
		//SetNeoCDTitle(game->pszTitle);

		return 1;
	} else {
		bprintf(0, _T("NeoGeoCD Unknown GAME ID %x\n"), nGameID);
		//SetNeoCDTitle(FBALoadStringEx(hAppInst, IDS_UNIDENTIFIED_CD, true));
	}

	return 0;
}

void iso9660_ReadOffset(UINT8 *Dest, FILE* fp, unsigned int lOffset, unsigned int lSize, unsigned int lLength)
{
	if (fp == NULL || Dest == NULL) return;

	fseek(fp, lOffset + ((nSectorLength == 2352) ? 16 : 0), SEEK_SET);
	fread(Dest, lLength, lSize, fp);
}

static void NeoCDList_iso9660_CheckDirRecord(void (*pfEntryCallBack)(INT32, TCHAR*), TCHAR *pszFile, FILE* fp, int nSector)
{
	unsigned int	lBytesRead			= 0;
	unsigned int	lOffset				= 0;
	bool	bNewSector			= false;
	bool	bRevisionQueve		= false;
	int		nRevisionQueveID	= 0;

	bool    bGotDDPRG_ACM = false;

	lOffset = (nSector * nSectorLength);

	UINT8 nLenDR;
	UINT8 Flags;
	UINT8 LEN_FI;
	UINT8* ExtentLoc = (UINT8*)malloc(8 + 64 + 32);
	UINT8* Data = (UINT8*)malloc(0x10b + 32);
	char *File = (char*)malloc(32 + 32);

	while(1)
	{
		iso9660_ReadOffset(&nLenDR, fp, lOffset, 1, sizeof(UINT8));

		if(nLenDR == 0x22) {
			lOffset		+= nLenDR;
			lBytesRead	+= nLenDR;
			continue;
		}

		if(nLenDR < 0x22)
		{
			if(bNewSector)
			{
				if(bRevisionQueve) {
					bRevisionQueve = false;
					pfEntryCallBack(nRevisionQueveID, pszFile);
				}
				return;
			}

			nLenDR = 0;
			iso9660_ReadOffset(&nLenDR, fp, lOffset + 1, 1, sizeof(UINT8));

			if(nLenDR < 0x22) {
				lOffset += (nSectorLength - lBytesRead);
				lBytesRead = 0;
				bNewSector = true;
				continue;
			}
		}

		bNewSector = false;

		iso9660_ReadOffset(&Flags, fp, lOffset + 25, 1, sizeof(UINT8));

		if(!(Flags & (1 << 1)))
		{
			// each file entry record(struct) is 54 bytes
			iso9660_ReadOffset(ExtentLoc, fp, lOffset + 2, 54, sizeof(UINT8));

			// extract filename
			char szFname[64];
			UINT8 nDate[3] = { 0, 0, 0 };
			memset(szFname, 0, sizeof(szFname));
			int fname_len = (ExtentLoc[30] < 32) ? ExtentLoc[30] : 32;
			strncpy(szFname, (char*)&ExtentLoc[31], fname_len);

			// detecting doubledr rev 1
			if (!strcmp(szFname, "DDPRG.ACM")) bGotDDPRG_ACM = true;

			// Check for end of directory entries
			if (fname_len == 0) break;

			nDate[0] = ExtentLoc[16];
			nDate[1] = ExtentLoc[17];
			nDate[2] = ExtentLoc[18];
#if DEBUG_CD
			bprintf(0, _T("Checking File  [%S]  %d-%d-%d\n"), szFname, nDate[0],nDate[1],nDate[2]);
#endif
			char szValue[32];
			sprintf(szValue, "%02x%02x%02x%02x", ExtentLoc[4], ExtentLoc[5], ExtentLoc[6], ExtentLoc[7]);

			unsigned int nValue = 0;
			sscanf(szValue, "%x", &nValue);

			iso9660_ReadOffset(Data, fp, nValue * nSectorLength, 0x10a, sizeof(UINT8));

			char szData[32];
			sprintf(szData, "%c%c%c%c%c%c%c", Data[0x100], Data[0x101], Data[0x102], Data[0x103], Data[0x104], Data[0x105], Data[0x106]);

			if(!strncmp(szData, "NEO-GEO", 7))
			{
				char id[32];
				sprintf(id, "%02X%02X",  Data[0x108], Data[0x109]);

				unsigned int nID = 0;
				sscanf(id, "%x", &nID);
#if DEBUG_CD
				bprintf(0, _T("----- ID:  %x\n"), nID);
#endif
				iso9660_ReadOffset(&LEN_FI, fp, lOffset + 32, 1, sizeof(UINT8));

				iso9660_ReadOffset((UINT8*)File, fp, lOffset + 33, LEN_FI, sizeof(UINT8));
				File[LEN_FI] = 0;

				// Savage Reign Rev 1
				if (nID == 0x0059 && nDate[0]==95 && nDate[1]==6 && nDate[2]==20) {
					nID |= 0x1000;
				}

				// Fatal Fury 3 Rev 1
				if (nID == 0x069c && nDate[0]==95 && nDate[1]==4 && nDate[2]==29) {
					nID |= 0x1000;
				}

				// Fatal Fury 3 Rev 2
				if (nID == 0x069c && nDate[0]==95 && nDate[1]==6 && nDate[2]==1) {
					nID |= 0x2000;
				}

				// Fatal Fury 3 Rev 3
				if (nID == 0x069c && nDate[0]==95 && nDate[1]==7 && nDate[2]==10) {
					nID |= 0x3000;
				}

				// World Heroes Perfect
				if (nID == 0x0090 && nDate[0]==95 && nDate[1]==7 && nDate[2]==21) {
					nID |= 0x1000;
				}

				// Fatal Fury Special Rev 1
				if (nID == 0x0058 && nDate[0]==94 && nDate[1]==10 && nDate[2]==14) {
					nID |= 0x1000;
				}

				// Samurai Shodown RPG (English Translation)
				if (nID == 0x0085 && nDate[0]==123 && nDate[1]==11 && nDate[2]==29) {
					nID |= 0x1000;
				}

				// Double Dragon Rev 1
				if (nID == 0x0082 && bGotDDPRG_ACM) {
					nID |= 0x1000;
				}

				// Double Dragon PS1 OST
				if (nID == 0x0082 && wcsstr(pszFile, _T("OST"))) {
					nID |= 0x2000;
				}

				// Looptris Plus
				if (nID == 0x2019 && !strcmp(szVolumeID, "LOOPTRSP")) {
					nID |= 0x0100;
				}

				// Samurai Shodown RPG (FR)
				if (nID == 0x0085 && wcsstr(pszFile, _T("(FR)"))) {
					nID |= 0x2000;
				}

				// Treasure of Caribbean (c) 1994 / (c) 2011 NCI
				if(nID == 0x0048 && Data[0x67] == 0x08) {
					nID |= 0x1000;
				}

				// King of Fighters '94, The (1994)(SNK)(JP)
				// 10-6-1994 (P1.PRG)
				if(nID == 0x0055 && Data[0x67] == 0xDE) {
					// ...continue
				}

				// King of Fighters '94, The (1994)(SNK)(JP-US)
				// 11-21-1994 (P1.PRG)
				if(nID == 0x0055 && Data[0x67] == 0xE6) {
					// Change to custom revision id
					nID |= 0x1000;
				}

				// King of Fighters '95, The (1995)(SNK)(JP-US)[!][NGCD-084 MT B01, B03-B06, NGCD-084E MT B01]
				// 9-11-1995 (P1.PRG)
				if(nID == 0x0084 && Data[0x6C] == 0xC0) {
					// ... continue
				}

				// King of Fighters '95, The (1995)(SNK)(JP-US)[!][NGCD-084 MT B10, NGCD-084E MT B03]
				// 10-5-1995 (P1.PRG)
				if(nID == 0x0084 && Data[0x6C] == 0xFF) {
					// Change to custom revision id
					nID |= 0x1000;
				}

				// King of Fighters '96 NEOGEO Collection, The
				if(nID == 0x0229) {
					bRevisionQueve = false;

					pfEntryCallBack(nID, pszFile);
					break;
				}

				// Sometimes there's multiple .prg entries in the
				// list, .old / .bak files which preceed the proper one..
				// For these, we have to keep searching the file list:

				if (nID == 0x0214 // King of Fighters '96, The
					|| (nID == 0x0058 && !(nDate[0]==94 && nDate[1]==8 && nDate[2]==5)) )
					{ // !Fatal Fury Special Rev 0
					// continue checking other files...
					bRevisionQueve		= true;
					nRevisionQueveID	= nID;
					lOffset		+= nLenDR;
					lBytesRead	+= nLenDR;
					continue;
				}

				pfEntryCallBack(nID, pszFile);
				break;
			}
		}

		lOffset		+= nLenDR;
		lBytesRead	+= nLenDR;
	}

	if (ExtentLoc) {
		free(ExtentLoc);
		ExtentLoc = NULL;
	}

	if (Data) {
		free(Data);
		Data = NULL;
	}

	if (File) {
		free(File);
		File = NULL;
	}
}

static void NeoCDList_Callback(INT32 nID, TCHAR *pszFile)
{
	GetNeoCDTitle(nID);
}


// Check the specified ISO, and proceed accordingly
int NeoCDList_CheckISO(TCHAR* pszFile, void (*pfEntryCallBack)(INT32, TCHAR*))
{
	if(!pszFile) {
		// error
		return 0;
	}

	// Make sure we have a valid ISO file extension...
	if (IsFileExt(pszFile, _T(".img")) || IsFileExt(pszFile, _T(".bin")))
	{
		FILE* fp = _tfopen(pszFile, _T("rb"));
		if(fp)
		{
			// Read ISO and look for 68K ROM standard program header, ID is always there
			// Note: This function works very quick, doesn't compromise performance :)
			// it just read each sector first 264 bytes aproximately only.

			// Get ISO Size (bytes)
			fseek(fp, 0, SEEK_END);
			unsigned int lSize = 0;
			lSize = ftell(fp);
			fseek(fp, 0, SEEK_SET);

			// If it has at least 16 sectors proceed
			if(lSize > (nSectorLength * 16))
			{
				// Check for Standard ISO9660 Identifier
				UINT8 IsoCheck[64];
				memset(IsoCheck, 0, sizeof(IsoCheck));
				memset(szVolumeID, 0, sizeof(szVolumeID));

				// advance to sector 16 and PVD Field 2
				iso9660_ReadOffset(&IsoCheck[0], fp, nSectorLength * 16 + 1, 1, sizeof(IsoCheck));//5); // get Standard Identifier Field from PVD

				// Verify that we have indeed a valid ISO9660 MODE1/2352
				if(!memcmp(IsoCheck, "CD001", 5))
				{
					// copy out volume ID & trim ending whitespace
					strncpy(szVolumeID, (char*)&IsoCheck[39], 24);
					int l = strlen(szVolumeID);
					while (l > 0 && szVolumeID[l - 1] == ' ') szVolumeID[l - 1] = '\0', l--;
#if DEBUG_CD
					bprintf(0, _T("VolumeID is [%S]\n"), szVolumeID);
#endif
					//bprintf(PRINT_NORMAL, _T("    Standard ISO9660 Identifier Found. \n"));
					iso9660_VDH vdh;

					// Get Volume Descriptor Header
					memset(&vdh, 0, sizeof(vdh));
					//memcpy(&vdh, iso9660_ReadOffset(fp, (2048 * 16), sizeof(vdh)), sizeof(vdh));
					iso9660_ReadOffset((UINT8*)&vdh, fp, 16 * nSectorLength, 1, sizeof(vdh));

					// Check for a valid Volume Descriptor Type
					if(vdh.vdtype == 0x01)
					{
#if 0
// This will fail on 64-bit due to differing variable sizes in the pvd struct
						// Get Primary Volume Descriptor
						iso9660_PVD pvd;
						memset(&pvd, 0, sizeof(pvd));
						//memcpy(&pvd, iso9660_ReadOffset(fp, (2048 * 16), sizeof(pvd)), sizeof(pvd));
						iso9660_ReadOffset((UINT8*)&pvd, fp, nSectorLength * 16, 1, sizeof(pvd));

						// ROOT DIRECTORY RECORD

						// Handle Path Table Location
						char szRootSector[32];
						unsigned int nRootSector = 0;

						sprintf(szRootSector, "%02X%02X%02X%02X",
							pvd.root_directory_record.location_of_extent[4],
							pvd.root_directory_record.location_of_extent[5],
							pvd.root_directory_record.location_of_extent[6],
							pvd.root_directory_record.location_of_extent[7]);

						// Convert HEX string to Decimal
						sscanf(szRootSector, "%X", &nRootSector);
#else
// Just read from the file directly at the correct offset (SECTOR_SIZE * 16 + 0x9e for the start of the root directory record)
						UINT8 buffer[32];
						char szRootSector[32];
						unsigned int nRootSector = 0;

						iso9660_ReadOffset(&buffer[0], fp, nSectorLength * 16 + 0x9e, 1, 8);

						sprintf(szRootSector, "%02x%02x%02x%02x", buffer[4], buffer[5], buffer[6], buffer[7]);

						sscanf(szRootSector, "%x", &nRootSector);
#endif

						// Process the Root Directory Records
						NeoCDList_iso9660_CheckDirRecord(pfEntryCallBack, pszFile, fp, nRootSector);

						// Path Table Records are not processed, since NeoGeo CD should not have subdirectories
						// ...
					}
				} else {

					//bprintf(PRINT_NORMAL, _T("    Standard ISO9660 Identifier Not Found, cannot continue. \n"));
					return 0;
				}
			}
		} else {

			//bprintf(PRINT_NORMAL, _T("    Couldn't open %s \n"), GetIsoPath());
			return 0;
		}

		if(fp) fclose(fp);

	} else {

		//bprintf(PRINT_NORMAL, _T("    File doesn't have a valid ISO extension [ .iso / .ISO ] \n"));
		return 0;
	}

	return 1;
}

// This will do everything
int GetNeoGeoCD_Identifier()
{
	if(!GetIsoPath() || !IsNeoGeoCD()) {
		return 0;
	}

	// Make sure we have a valid ISO file extension...
	if (IsFileExt(GetIsoPath(), _T(".img")) || IsFileExt(GetIsoPath(), _T(".bin")))
	{
		if(_tfopen(GetIsoPath(), _T("rb")))
		{
			bprintf(0, _T("NeoCDList: checking %s\n"), GetIsoPath());
			// Read ISO and look for 68K ROM standard program header, ID is always there
			// Note: This function works very quick, doesn't compromise performance :)
			// it just read each sector first 264 bytes aproximately only.
			NeoCDList_CheckISO(GetIsoPath(), NeoCDList_Callback);

		} else {

			bprintf(PRINT_NORMAL, _T("    Couldn't open %s \n"), GetIsoPath());
			return 0;
		}

	} else {

		bprintf(PRINT_NORMAL, _T("    File doesn't have a valid ISO extension [ .img / .bin ] \n"));
		return 0;
	}

	return 1;
}

int NeoCDInfo_Init()
{
	NeoCDInfo_Exit();
	return GetNeoGeoCD_Identifier();
}

TCHAR* NeoCDInfo_Text(int nText)
{
	if(!game || !IsNeoGeoCD()) return NULL;

	switch(nText)
	{
		case DRV_NAME:			return game->pszName;
		case DRV_FULLNAME:		return game->pszTitle;
		case DRV_MANUFACTURER:	return game->pszCompany;
		case DRV_DATE:			return game->pszYear;
	}

	return NULL;
}

int NeoCDInfo_ID()
{
	if(!game || !IsNeoGeoCD()) return 0;
	return game->id;
}

void NeoCDInfo_Exit()
{
	if(game) {
		free(game);
		game = NULL;
	}
}
