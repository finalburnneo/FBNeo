#ifndef __RETRO_COMMON__
#define __RETRO_COMMON__

#include <string>
#include <vector>
#include "burner.h"

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
#define RETRO_GAME_TYPE_NEOCD	11

struct dipswitch_core_option_value
{
	struct GameInp *pgi;
	BurnDIPInfo bdi;
	char friendly_name[100];
};

struct dipswitch_core_option
{
	char option_name[100];
	char friendly_name[100];
	std::string values_str;
	std::vector<dipswitch_core_option_value> values;
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
extern struct GameInp *pgi_reset;
extern struct GameInp *pgi_diag;
extern bool is_neogeo_game;
extern bool allow_neogeo_mode;
extern bool core_aspect_par;
extern bool bVerticalMode;
extern bool bAllowDepth32;
extern UINT32 nFrameskip;
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
#ifdef USE_CYCLONE
void SetSekCpuCore();
#endif

#endif
