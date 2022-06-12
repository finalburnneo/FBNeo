#ifndef __RETRO_COMMON__
#define __RETRO_COMMON__

#include <string>
#include <sstream>
#include <algorithm>
#include <vector>
#include "burner.h"

#define SSTR( x ) static_cast< const std::ostringstream & >(( std::ostringstream() << std::dec << x ) ).str()

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
#define RETRO_GAME_TYPE_CHF		15

#define PERCENT_VALUES \
      {"25%", NULL }, \
      {"26%", NULL }, \
      {"27%", NULL }, \
      {"28%", NULL }, \
      {"29%", NULL }, \
      {"30%", NULL }, \
      {"31%", NULL }, \
      {"32%", NULL }, \
      {"33%", NULL }, \
      {"34%", NULL }, \
      {"35%", NULL }, \
      {"36%", NULL }, \
      {"37%", NULL }, \
      {"38%", NULL }, \
      {"39%", NULL }, \
      {"40%", NULL }, \
      {"41%", NULL }, \
      {"42%", NULL }, \
      {"43%", NULL }, \
      {"44%", NULL }, \
      {"45%", NULL }, \
      {"46%", NULL }, \
      {"47%", NULL }, \
      {"48%", NULL }, \
      {"49%", NULL }, \
      {"50%", NULL }, \
      {"51%", NULL }, \
      {"52%", NULL }, \
      {"53%", NULL }, \
      {"54%", NULL }, \
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
      {"205%", NULL }, \
      {"210%", NULL }, \
      {"215%", NULL }, \
      {"220%", NULL }, \
      {"225%", NULL }, \
      {"230%", NULL }, \
      {"235%", NULL }, \
      {"240%", NULL }, \
      {"245%", NULL }, \
      {"250%", NULL }, \
      {"255%", NULL }, \
      {"260%", NULL }, \
      {"265%", NULL }, \
      {"270%", NULL }, \
      {"275%", NULL }, \
      {"280%", NULL }, \
      {"285%", NULL }, \
      {"290%", NULL }, \
      {"295%", NULL }, \
      {"300%", NULL }, \
      {"305%", NULL }, \
      {"310%", NULL }, \
      {"315%", NULL }, \
      {"320%", NULL }, \
      {"325%", NULL }, \
      {"330%", NULL }, \
      {"335%", NULL }, \
      {"340%", NULL }, \
      {"345%", NULL }, \
      {"350%", NULL }, \
      {"355%", NULL }, \
      {"360%", NULL }, \
      {"365%", NULL }, \
      {"370%", NULL }, \
      {"375%", NULL }, \
      {"380%", NULL }, \
      {"385%", NULL }, \
      {"390%", NULL }, \
      {"395%", NULL }, \
      {"400%", NULL }, \
      { NULL, NULL },

struct dipswitch_core_option_value
{
	struct GameInp *pgi;
	BurnDIPInfo bdi;
	std::string friendly_name;
	struct GameInp *cond_pgi;
	int nCondMask;
	int nCondSetting;
};

struct dipswitch_core_option
{
	std::string option_name;
	std::string friendly_name;
	std::string friendly_name_categorized;
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
	std::string friendly_name_categorized;
	std::vector<cheat_core_option_value> values;
};

enum neogeo_bios_categories
{
	NEOGEO_MVS = 1<<0,
	NEOGEO_AES = 1<<1,
	NEOGEO_UNI = 1<<2,
	NEOGEO_EUR = 1<<3,
	NEOGEO_USA = 1<<4,
	NEOGEO_JAP = 1<<5,
};

struct RomBiosInfo {
	char* filename;
	uint32_t crc;
	uint8_t NeoSystem;
	char* friendly_name;
	uint32_t categories;
	uint32_t available;
};

extern retro_log_printf_t log_cb;
extern retro_environment_t environ_cb;
extern std::vector<dipswitch_core_option> dipswitch_core_options;
extern std::vector<cheat_core_option> cheat_core_options;
extern struct GameInp *pgi_reset;
extern struct GameInp *pgi_diag;
extern struct GameInp *pgi_debug_dip_1;
extern struct GameInp *pgi_debug_dip_2;
extern bool bIsNeogeoCartGame;
extern bool allow_neogeo_mode;
extern bool core_aspect_par;
extern bool bAllowDepth32;
extern bool bPatchedRomsetsEnabled;
extern bool bLibretroSupportsAudioBuffStatus;
extern bool bLowPassFilterEnabled;
extern UINT32 nVerticalMode;
extern UINT32 nFrameskip;
extern UINT32 nFrameskipType;
extern UINT32 nFrameskipThreshold;
extern UINT32 nMemcardMode;
extern UINT32 nLightgunCrosshairEmulation;
extern UINT8 NeoSystem;
extern INT32 g_audio_samplerate;
extern UINT8 *diag_input;
extern unsigned nGameType;
extern char g_rom_dir[MAX_PATH];

char* str_char_replace(char* destination, char c_find, char c_replace);
void set_neo_system_bios();
void set_neogeo_bios_availability(char *szName, uint32_t crc, bool ignoreCrc);
void evaluate_neogeo_bios_mode(const char* drvname);
void set_environment();
void check_variables(void);
int HandleMessage(enum retro_log_level level, TCHAR* szFormat, ...);
#ifdef USE_CYCLONE
void SetSekCpuCore();
#endif

#endif
