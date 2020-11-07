#ifndef __RETRO_COMMON__
#define __RETRO_COMMON__

#include <string>
#include <sstream>
#include <algorithm>
#include <vector>
#include "burner.h"

#define SSTR( x ) static_cast< std::ostringstream & >(( std::ostringstream() << std::dec << x ) ).str()

#define RETRO_GAME_TYPE_CV		1
#define RETRO_GAME_TYPE_GG		2
#define RETRO_GAME_TYPE_MD		3
#define RETRO_GAME_TYPE_MSX		4
#define RETRO_GAME_TYPE_PCE		5
#define RETRO_GAME_TYPE_SG1K	6
#define RETRO_GAME_TYPE_SGX		7
#define RETRO_GAME_TYPE_SMS		8
#define RETRO_GAME_TYPE_TG		9
#define RETRO_GAME_TYPE_SPEC	10
#define RETRO_GAME_TYPE_NES		11
#define RETRO_GAME_TYPE_FDS		12
#define RETRO_GAME_TYPE_NEOCD	13
#define RETRO_GAME_TYPE_NGP		14

#define PERCENT_VALUES \
      {"25%", NULL }, \
      {"30%", NULL }, \
      {"35%", NULL }, \
      {"40%", NULL }, \
      {"45%", NULL }, \
      {"50%", NULL }, \
      {"55%", NULL }, \
      {"60%", NULL }, \
      {"65%", NULL }, \
      {"70%", NULL }, \
      {"75%", NULL }, \
      {"80%", NULL }, \
      {"85%", NULL }, \
      {"90%", NULL }, \
      {"95%", NULL }, \
      {"100%", NULL }, \
      {"105%", NULL }, \
      {"110%", NULL }, \
      {"115%", NULL }, \
      {"120%", NULL }, \
      {"125%", NULL }, \
      {"130%", NULL }, \
      {"135%", NULL }, \
      {"140%", NULL }, \
      {"145%", NULL }, \
      {"150%", NULL }, \
      {"155%", NULL }, \
      {"160%", NULL }, \
      {"165%", NULL }, \
      {"170%", NULL }, \
      {"175%", NULL }, \
      {"180%", NULL }, \
      {"185%", NULL }, \
      {"190%", NULL }, \
      {"195%", NULL }, \
      {"200%", NULL }, \
      { NULL, NULL },

struct dipswitch_core_option_value
{
	struct GameInp *pgi;
	BurnDIPInfo bdi;
	std::string friendly_name;
};

struct dipswitch_core_option
{
	std::string option_name;
	std::string friendly_name;
	BurnDIPInfo default_bdi;
	std::vector<dipswitch_core_option_value> values;
};

struct cheat_core_option_value
{
	int nValue;
	std::string friendly_name;
};

struct cheat_core_option
{
	int num;
	std::string default_value;
	std::string option_name;
	std::string friendly_name;
	std::vector<cheat_core_option_value> values;
};

enum neo_geo_modes
{
	NEO_GEO_MODE_MVS = 0,
	NEO_GEO_MODE_AES = 1,
	NEO_GEO_MODE_UNIBIOS = 2,
	NEO_GEO_MODE_DIPSWITCH = 3
};

struct RomBiosInfo {
	char* filename;
	uint32_t crc;
	uint8_t NeoSystem;
	char* friendly_name;
	uint8_t priority;
};

extern struct RomBiosInfo mvs_bioses[];
extern struct RomBiosInfo aes_bioses[];
extern struct RomBiosInfo uni_bioses[];

extern retro_log_printf_t log_cb;
extern retro_environment_t environ_cb;
extern RomBiosInfo *available_mvs_bios;
extern RomBiosInfo *available_aes_bios;
extern RomBiosInfo *available_uni_bios;
extern std::vector<dipswitch_core_option> dipswitch_core_options;
extern std::vector<cheat_core_option> cheat_core_options;
extern struct GameInp *pgi_reset;
extern struct GameInp *pgi_diag;
extern bool is_neogeo_game;
extern bool allow_neogeo_mode;
extern bool core_aspect_par;
extern bool bAllowDepth32;
extern bool bLightgunHideCrosshairEnabled;
extern bool bPatchedRomsetsEnabled;
extern bool bLibretroSupportsAudioBuffStatus;
extern UINT32 nVerticalMode;
extern UINT32 nFrameskip;
extern UINT32 nFrameskipType;
extern UINT32 nFrameskipThreshold;
extern UINT32 nMemcardMode;
extern UINT8 NeoSystem;
extern INT32 g_audio_samplerate;
extern UINT8 *diag_input;
extern neo_geo_modes g_opt_neo_geo_mode;
extern unsigned nGameType;
extern char g_rom_dir[MAX_PATH];

char* str_char_replace(char* destination, char c_find, char c_replace);
void set_neo_system_bios();
void evaluate_neogeo_bios_mode(const char* drvname);
void set_environment();
void check_variables(void);
int HandleMessage(enum retro_log_level level, TCHAR* szFormat, ...);
#ifdef USE_CYCLONE
void SetSekCpuCore();
#endif

#endif
