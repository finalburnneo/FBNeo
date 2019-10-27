#include <vector>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>

#include "libretro.h"
#include "burner.h"
#include "burnint.h"

#include "retro_common.h"
#include "retro_cdemu.h"
#include "retro_input.h"
#include "retro_memory.h"

#include <file/file_path.h>

#include <streams/file_stream.h>

#define FBNEO_VERSION "v0.2.97.44"

int counter;           // General purpose variable used when debugging
struct MovieExtInfo
{
	// date & time
	UINT32 year, month, day;
	UINT32 hour, minute, second;
};
struct MovieExtInfo MovieInfo = { 0, 0, 0, 0, 0, 0 };

static void log_dummy(enum retro_log_level level, const char *fmt, ...) { }

static bool apply_dipswitch_from_variables();

static void init_audio_buffer(INT32 sample_rate, INT32 fps);

retro_log_printf_t log_cb = log_dummy;
retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;

#define BPRINTF_BUFFER_SIZE 512
char bprintf_buf[BPRINTF_BUFFER_SIZE];
static INT32 __cdecl libretro_bprintf(INT32 nStatus, TCHAR* szFormat, ...)
{
	va_list vp;
	va_start(vp, szFormat);
	int rc = vsnprintf(bprintf_buf, BPRINTF_BUFFER_SIZE, szFormat, vp);
	va_end(vp);

	if (rc >= 0)
	{
#ifdef FBNEO_DEBUG
		retro_log_level retro_log = RETRO_LOG_INFO;
#else
		retro_log_level retro_log = RETRO_LOG_DEBUG;
#endif
		if (nStatus == PRINT_UI)
			retro_log = RETRO_LOG_INFO;
		else if (nStatus == PRINT_IMPORTANT)
			retro_log = RETRO_LOG_WARN;
		else if (nStatus == PRINT_ERROR)
			retro_log = RETRO_LOG_ERROR;

		log_cb(retro_log, bprintf_buf);
	}

	return rc;
}

INT32 (__cdecl *bprintf) (INT32 nStatus, TCHAR* szFormat, ...) = libretro_bprintf;

int kNetGame = 0;
INT32 nReplayStatus = 0;
INT32 nIpsMaxFileLen = 0;
unsigned nGameType = 0;
static INT32 nGameWidth, nGameHeight;

static unsigned int BurnDrvGetIndexByName(const char* name);
char* DecorateGameName(UINT32 nBurnDrv);

extern INT32 EnableHiscores;

#define STAT_NOFIND  0
#define STAT_OK      1
#define STAT_CRC     2
#define STAT_SMALL   3
#define STAT_LARGE   4

struct ROMFIND
{
	unsigned int nState;
	int nArchive;
	int nPos;
	BurnRomInfo ri;
};

static std::vector<std::string> g_find_list_path;
static ROMFIND g_find_list[1024];
static unsigned g_rom_count;

INT32 nAudSegLen = 0;

static UINT8* pVidImage = NULL;
static bool bVidImageNeedRealloc = false;
static int16_t *g_audio_buf = NULL;

// Mapping of PC inputs to game inputs
struct GameInp* GameInp = NULL;
UINT32 nGameInpCount = 0;
INT32 nAnalogSpeed = 0x0100;
INT32 nFireButtons = 0;

// libretro globals
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t) {}
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }

void retro_set_environment(retro_environment_t cb)
{
	environ_cb = cb;

	// Subsystem (needs to be called now, or it won't work on command line)
	static const struct retro_subsystem_rom_info subsystem_rom[] = {
		{ "Rom", "zip|7z", true, true, true, NULL, 0 },
	};
	static const struct retro_subsystem_rom_info subsystem_iso[] = {
		{ "Iso", "ccd|cue", true, true, true, NULL, 0 },
	};
	static const struct retro_subsystem_info subsystems[] = {
		{ "CBS ColecoVision", "cv", subsystem_rom, 1, RETRO_GAME_TYPE_CV },
		{ "MSX 1", "msx", subsystem_rom, 1, RETRO_GAME_TYPE_MSX },
		{ "Nec PC-Engine", "pce", subsystem_rom, 1, RETRO_GAME_TYPE_PCE },
		{ "Nec SuperGrafX", "sgx", subsystem_rom, 1, RETRO_GAME_TYPE_SGX },
		{ "Nec TurboGrafx-16", "tg16", subsystem_rom, 1, RETRO_GAME_TYPE_TG },
		{ "Sega GameGear", "gg", subsystem_rom, 1, RETRO_GAME_TYPE_GG },
		{ "Sega Master System", "sms", subsystem_rom, 1, RETRO_GAME_TYPE_SMS },
		{ "Sega Megadrive", "md", subsystem_rom, 1, RETRO_GAME_TYPE_MD },
		{ "Sega SG-1000", "sg1k", subsystem_rom, 1, RETRO_GAME_TYPE_SG1K },
		{ "ZX Spectrum", "spec", subsystem_rom, 1, RETRO_GAME_TYPE_SPEC },
		{ "Neogeo CD", "neocd", subsystem_iso, 1, RETRO_GAME_TYPE_NEOCD },
		{ NULL },
	};

	environ_cb(RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO, (void*)subsystems);
}

char g_driver_name[128];
char g_rom_dir[MAX_PATH];
char g_rom_parent_dir[MAX_PATH];
char g_save_dir[MAX_PATH];
char g_system_dir[MAX_PATH];
char g_autofs_path[MAX_PATH];
extern unsigned int (__cdecl *BurnHighCol) (signed int r, signed int g, signed int b, signed int i);

static bool driver_inited;

void retro_get_system_info(struct retro_system_info *info)
{
	info->library_name = APP_TITLE;
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
	info->library_version = FBNEO_VERSION GIT_VERSION;
	info->need_fullpath = true;
	info->block_extract = true;
	info->valid_extensions = "zip|7z";
}

// FBNEO stubs
unsigned ArcadeJoystick;

int bDrvOkay;
int bRunPause;
bool bAlwaysProcessKeyboardInput;

bool bDoIpsPatch;
void IpsApplyPatches(UINT8 *, char *) {}

TCHAR szAppEEPROMPath[MAX_PATH];
TCHAR szAppHiscorePath[MAX_PATH];
TCHAR szAppSamplesPath[MAX_PATH];
TCHAR szAppBlendPath[MAX_PATH];
TCHAR szAppHDDPath[MAX_PATH];
TCHAR szAppBurnVer[16];

static int nDIPOffset;

static void InpDIPSWGetOffset (void)
{
	BurnDIPInfo bdi;
	nDIPOffset = 0;

	for(int i = 0; BurnDrvGetDIPInfo(&bdi, i) == 0; i++)
	{
		if (bdi.nFlags == 0xF0)
		{
			nDIPOffset = bdi.nInput;
			log_cb(RETRO_LOG_INFO, "DIP switches offset: %d.\n", bdi.nInput);
			break;
		}
	}
}

void InpDIPSWResetDIPs (void)
{
	int i = 0;
	BurnDIPInfo bdi;
	struct GameInp * pgi = NULL;

	InpDIPSWGetOffset();

	while (BurnDrvGetDIPInfo(&bdi, i) == 0)
	{
		if (bdi.nFlags == 0xFF)
		{
			pgi = GameInp + bdi.nInput + nDIPOffset;
			if (pgi)
			pgi->Input.Constant.nConst = (pgi->Input.Constant.nConst & ~bdi.nMask) | (bdi.nSetting & bdi.nMask);
		}
		i++;
	}
}

static int InpDIPSWInit()
{
	log_cb(RETRO_LOG_INFO, "Initialize DIP switches.\n");

	dipswitch_core_options.clear(); 

	BurnDIPInfo bdi;
	struct GameInp *pgi;

	const char * drvname = BurnDrvGetTextA(DRV_NAME);

	if (!drvname)
		return 0;

	for (int i = 0, j = 0; BurnDrvGetDIPInfo(&bdi, i) == 0; i++)
	{
		/* 0xFE is the beginning label for a DIP switch entry */
		/* 0xFD are region DIP switches */
		if ((bdi.nFlags == 0xFE || bdi.nFlags == 0xFD) && bdi.nSetting > 0)
		{
			dipswitch_core_options.push_back(dipswitch_core_option());
			dipswitch_core_option *dip_option = &dipswitch_core_options.back();

			// Clean the dipswitch name to creation the core option name (removing space and equal characters)
			char option_name[100];

			// Some dipswitch has no name...
			if (bdi.szText)
			{
				strcpy(option_name, bdi.szText);
			}
			else // ... so, to not hang, we will generate a name based on the position of the dip (DIPSWITCH 1, DIPSWITCH 2...)
			{
				sprintf(option_name, "DIPSWITCH %d", (int)dipswitch_core_options.size());
				log_cb(RETRO_LOG_WARN, "Error in %sDIPList : The DIPSWITCH '%d' has no name. '%s' name has been generated\n", drvname, dipswitch_core_options.size(), option_name);
			}

			strncpy(dip_option->friendly_name, option_name, sizeof(dip_option->friendly_name));

			str_char_replace(option_name, ' ', '_');
			str_char_replace(option_name, '=', '_');

			snprintf(dip_option->option_name, sizeof(dip_option->option_name), "fbneo-dipswitch-%s-%s", drvname, option_name);

			// Search for duplicate name, and add number to make them unique in the core-options file
			for (int dup_idx = 0, dup_nbr = 1; dup_idx < dipswitch_core_options.size() - 1; dup_idx++) // - 1 to exclude the current one
			{
				if (strcmp(dip_option->option_name, dipswitch_core_options[dup_idx].option_name) == 0)
				{
					dup_nbr++;
					snprintf(dip_option->option_name, sizeof(dip_option->option_name), "fbneo-dipswitch-%s-%s_%d", drvname, option_name, dup_nbr);
				}
			}

			dip_option->values.reserve(bdi.nSetting);
			dip_option->values.assign(bdi.nSetting, dipswitch_core_option_value());

			int values_count = 0;
			bool skip_unusable_option = false;
			for (int k = 0; values_count < bdi.nSetting; k++)
			{
				BurnDIPInfo bdi_value;
				if (BurnDrvGetDIPInfo(&bdi_value, k + i + 1) != 0)
				{
					log_cb(RETRO_LOG_WARN, "Error in %sDIPList for DIPSWITCH '%s': End of the struct was reached too early\n", drvname, dip_option->friendly_name);
					break;
				}

				if (bdi_value.nFlags == 0xFE || bdi_value.nFlags == 0xFD)
				{
					log_cb(RETRO_LOG_WARN, "Error in %sDIPList for DIPSWITCH '%s': Start of next DIPSWITCH is too early\n", drvname, dip_option->friendly_name);
					break;
				}

				struct GameInp *pgi_value = GameInp + bdi_value.nInput + nDIPOffset;

				// When the pVal of one value is NULL => the DIP switch is unusable. So it will be skipped by removing it from the list
				if (pgi_value->Input.pVal == 0)
				{
					skip_unusable_option = true;
					break;
				}

				// Filter away NULL entries
				if (bdi_value.nFlags == 0)
				{
					log_cb(RETRO_LOG_WARN, "Error in %sDIPList for DIPSWITCH '%s': the line '%d' is useless\n", drvname, dip_option->friendly_name, k + 1);
					continue;
				}

				dipswitch_core_option_value *dip_value = &dip_option->values[values_count];

				BurnDrvGetDIPInfo(&(dip_value->bdi), k + i + 1);
				dip_value->pgi = pgi_value;
				strncpy(dip_value->friendly_name, dip_value->bdi.szText, sizeof(dip_value->friendly_name));

				bool is_default_value = (dip_value->pgi->Input.Constant.nConst & dip_value->bdi.nMask) == (dip_value->bdi.nSetting);

				if (is_default_value)
				{
					snprintf(dip_option->default_value, sizeof(dip_option->default_value), "%s", dip_value->bdi.szText);
				}

				values_count++;
			}

			if (bdi.nSetting > values_count)
			{
				// Truncate the list at the values_count found to not have empty values
				dip_option->values.resize(values_count); // +1 for default value
				log_cb(RETRO_LOG_WARN, "Error in %sDIPList for DIPSWITCH '%s': '%d' values were intended and only '%d' were found\n", drvname, dip_option->friendly_name, bdi.nSetting, values_count);
			}

			// Skip the unusable option by removing it from the list
			if (skip_unusable_option)
			{
				dipswitch_core_options.pop_back();
				continue;
			}

			pgi = GameInp + bdi.nInput + nDIPOffset;

			j++;
		}
	}

	evaluate_neogeo_bios_mode(drvname);

	return 0;
}

// Update DIP switches value  depending of the choice the user made in core options
static bool apply_dipswitch_from_variables()
{
	bool dip_changed = false;
#if 0
	log_cb(RETRO_LOG_INFO, "Apply DIP switches value from core options.\n");
#endif
	struct retro_variable var = {0};

	for (int dip_idx = 0; dip_idx < dipswitch_core_options.size(); dip_idx++)
	{
		dipswitch_core_option *dip_option = &dipswitch_core_options[dip_idx];

		// Games which needs a specific bios don't handle alternative bioses very well
		if (is_neogeo_game && !allow_neogeo_mode && strcasecmp(dip_option->friendly_name, "BIOS") == 0)
			continue;

		var.key = dip_option->option_name;
		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) == false)
			continue;

		for (int dip_value_idx = 0; dip_value_idx < dip_option->values.size(); dip_value_idx++)
		{
			dipswitch_core_option_value *dip_value = &(dip_option->values[dip_value_idx]);

			if (strcasecmp(var.value, dip_value->friendly_name) != 0)
				continue;

			int old_nConst = dip_value->pgi->Input.Constant.nConst;

			dip_value->pgi->Input.Constant.nConst = (dip_value->pgi->Input.Constant.nConst & ~dip_value->bdi.nMask) | (dip_value->bdi.nSetting & dip_value->bdi.nMask);
			dip_value->pgi->Input.nVal = dip_value->pgi->Input.Constant.nConst;
			if (dip_value->pgi->Input.pVal)
				*(dip_value->pgi->Input.pVal) = dip_value->pgi->Input.nVal;

			if (dip_value->pgi->Input.Constant.nConst == old_nConst)
			{
#if 0
				log_cb(RETRO_LOG_INFO, "DIP switch at PTR: [%-10d] [0x%02x] -> [0x%02x] - No change - '%s' '%s' [0x%02x]\n",
				dip_value->pgi->Input.pVal, old_nConst, dip_value->pgi->Input.Constant.nConst, dip_option->friendly_name, dip_value->friendly_name, dip_value->bdi.nSetting);
#endif
			}
			else
			{
				dip_changed = true;
#if 0
				log_cb(RETRO_LOG_INFO, "DIP switch at PTR: [%-10d] [0x%02x] -> [0x%02x] - Changed   - '%s' '%s' [0x%02x]\n",
				dip_value->pgi->Input.pVal, old_nConst, dip_value->pgi->Input.Constant.nConst, dip_option->friendly_name, dip_value->friendly_name, dip_value->bdi.nSetting);
#endif
			}
		}
	}

	// Override the NeoGeo bios DIP Switch by the main one (for the moment)
	if (is_neogeo_game)
		set_neo_system_bios();

	return dip_changed;
}

int InputSetCooperativeLevel(const bool bExclusive, const bool bForeGround) { return 0; }

void Reinitialise(void)
{
	// Update the geometry, some games (sfiii2) and systems (megadrive) need it.
	BurnDrvGetFullSize(&nGameWidth, &nGameHeight);
	nBurnPitch = nGameWidth * nBurnBpp;
	struct retro_system_av_info av_info;
	retro_get_system_av_info(&av_info);
	environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &av_info);
}

static void ForceFrameStep(int bDraw)
{
#ifdef FBNEO_DEBUG
	nFramesEmulated++;
#endif
	nCurrentFrame++;

	if (!bDraw)
		pBurnDraw = NULL;
#ifdef FBNEO_DEBUG
	else
		nFramesRendered++;
#endif
	BurnDrvFrame();
}

// Non-idiomatic (OutString should be to the left to match strcpy())
// Seems broken to not check nOutSize.
char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int /*nOutSize*/)
{
	if (pszOutString)
	{
		strcpy(pszOutString, pszInString);
		return pszOutString;
	}

	return (char*)pszInString;
}

const int nConfigMinVersion = 0x020921;

// addition to support loading of roms without crc check
static int find_rom_by_name(char *name, const ZipEntry *list, unsigned elems)
{
	unsigned i = 0;
	for (i = 0; i < elems; i++)
	{
		if( strcmp(list[i].szName, name) == 0 )
		{
			return i;
		}
	}

#if 0
	log_cb(RETRO_LOG_ERROR, "Not found: %s (name = %s)\n", list[i].szName, name);
#endif

	return -1;
}

static int find_rom_by_crc(uint32_t crc, const ZipEntry *list, unsigned elems)
{
	unsigned i = 0;
	for (i = 0; i < elems; i++)
	{
		if (list[i].nCrc == crc)
		{
			return i;
		}
	}

#if 0
	log_cb(RETRO_LOG_ERROR, "Not found: 0x%X (crc: 0x%X)\n", list[i].nCrc, crc);
#endif

	return -1;
}

static RomBiosInfo* find_bios_info(char *szName, uint32_t crc, struct RomBiosInfo* bioses)
{
	for (int i = 0; bioses[i].filename != NULL; i++)
	{
		if (strcmp(bioses[i].filename, szName) == 0 || bioses[i].crc == crc)
		{
			return &bioses[i];
		}
	}

#if 0
	log_cb(RETRO_LOG_ERROR, "Bios not found: %s (crc: 0x%08x)\n", szName, crc);
#endif

	return NULL;
}

static void free_archive_list(ZipEntry *list, unsigned count)
{
	if (list)
	{
		for (unsigned i = 0; i < count; i++)
			free(list[i].szName);
		free(list);
	}
}

static int archive_load_rom(uint8_t *dest, int *wrote, int i)
{
	if (i < 0 || i >= g_rom_count)
		return 1;

	int archive = g_find_list[i].nArchive;

	if (ZipOpen((char*)g_find_list_path[archive].c_str()) != 0)
		return 1;

	BurnRomInfo ri = {0};
	BurnDrvGetRomInfo(&ri, i);

	if (ZipLoadFile(dest, ri.nLen, wrote, g_find_list[i].nPos) != 0)
	{
		ZipClose();
		return 1;
	}

	ZipClose();
	return 0;
}

static void locate_archive(std::vector<std::string>& pathList, const char* const romName)
{
	static char path[MAX_PATH];

	// Search rom dir
	snprintf(path, sizeof(path), "%s%c%s", g_rom_dir, path_default_slash_c(), romName);
	if (ZipOpen(path) == 0)
	{
		g_find_list_path.push_back(path);
		return;
	}
	// Search system fbneo subdirectory (where samples/hiscore are stored)
	snprintf(path, sizeof(path), "%s%cfbneo%c%s", g_system_dir, path_default_slash_c(), path_default_slash_c(), romName);
	if (ZipOpen(path) == 0)
	{
		g_find_list_path.push_back(path);
		return;
	}
	// Search system directory
	snprintf(path, sizeof(path), "%s%c%s", g_system_dir, path_default_slash_c(), romName);
	if (ZipOpen(path) == 0)
	{
		g_find_list_path.push_back(path);
		return;
	}

	log_cb(RETRO_LOG_ERROR, "[FBNEO] Couldn't locate the %s archive anywhere, this game probably won't boot.\n", romName);
}

// This code is very confusing. The original code is even more confusing :(
static bool open_archive()
{
	memset(g_find_list, 0, sizeof(g_find_list));

	// FBNEO wants some roms ... Figure out how many.
	g_rom_count = 0;
	while (!BurnDrvGetRomInfo(&g_find_list[g_rom_count].ri, g_rom_count))
		g_rom_count++;

	g_find_list_path.clear();

	// Check if we have said archives.
	// Check if archives are found. These are relative to g_rom_dir.
	char *rom_name;
	for (unsigned index = 0; index < 32; index++)
	{
		if (BurnDrvGetZipName(&rom_name, index))
			continue;

		log_cb(RETRO_LOG_INFO, "[FBNEO] Archive: %s\n", rom_name);

		locate_archive(g_find_list_path, rom_name);

		// Handle bios for pgm single pcb board (special case)
		if (strcmp(rom_name, "thegladpcb") == 0 || strcmp(rom_name, "dmnfrntpcb") == 0 || strcmp(rom_name, "svgpcb") == 0)
			locate_archive(g_find_list_path, "pgm");

		ZipClose();
	}

	for (unsigned z = 0; z < g_find_list_path.size(); z++)
	{
		if (ZipOpen((char*)g_find_list_path[z].c_str()) != 0)
		{
			log_cb(RETRO_LOG_ERROR, "[FBNEO] Failed to open archive %s\n", g_find_list_path[z].c_str());
			return false;
		}

		ZipEntry *list = NULL;
		int count;
		ZipGetList(&list, &count);

		// Try to map the ROMs FBNEO wants to ROMs we find inside our pretty archives ...
		for (unsigned i = 0; i < g_rom_count; i++)
		{
			if (g_find_list[i].nState == STAT_OK)
				continue;

			if (g_find_list[i].ri.nType == 0 || g_find_list[i].ri.nLen == 0 || g_find_list[i].ri.nCrc == 0)
			{
				g_find_list[i].nState = STAT_OK;
				continue;
			}

			int index = find_rom_by_crc(g_find_list[i].ri.nCrc, list, count);

			BurnDrvGetRomName(&rom_name, i, 0);

			bool bad_crc = false;

			if (index < 0 && strncmp("uni-bios_", rom_name, 9) == 0)
			{
				index = find_rom_by_name(rom_name, list, count);
				if (index >= 0)
					bad_crc = true;
			}

			if (index >= 0)
			{
				if (bad_crc)
					log_cb(RETRO_LOG_WARN, "[FBNEO] Using ROM with bad CRC and name %s from archive %s\n", rom_name, g_find_list_path[z].c_str());
				else
					log_cb(RETRO_LOG_INFO, "[FBNEO] Using ROM with good CRC and name %s from archive %s\n", rom_name, g_find_list_path[z].c_str());
			}
			else
			{
				continue;
			}

			// Search for the best bios available by category
			if (is_neogeo_game)
			{
				RomBiosInfo *bios;

				// MVS BIOS
				bios = find_bios_info(list[index].szName, list[index].nCrc, mvs_bioses);
				if (bios)
				{
					if (!available_mvs_bios || (available_mvs_bios && bios->priority < available_mvs_bios->priority))
						available_mvs_bios = bios;
				}

				// AES BIOS
				bios = find_bios_info(list[index].szName, list[index].nCrc, aes_bioses);
				if (bios)
				{
					if (!available_aes_bios || (available_aes_bios && bios->priority < available_aes_bios->priority))
						available_aes_bios = bios;
				}

				// Universe BIOS
				bios = find_bios_info(list[index].szName, list[index].nCrc, uni_bioses);
				if (bios)
				{
					if (!available_uni_bios || (available_uni_bios && bios->priority < available_uni_bios->priority))
						available_uni_bios = bios;
				}
			}

			// Yay, we found it!
			g_find_list[i].nArchive = z;
			g_find_list[i].nPos = index;
			g_find_list[i].nState = STAT_OK;

			if (list[index].nLen < g_find_list[i].ri.nLen)
				g_find_list[i].nState = STAT_SMALL;
			else if (list[index].nLen > g_find_list[i].ri.nLen)
				g_find_list[i].nState = STAT_LARGE;
		}

		free_archive_list(list, count);
		ZipClose();
	}

	bool is_neogeo_bios_available = false;
	if (is_neogeo_game)
	{
		if (!available_mvs_bios && !available_aes_bios && !available_uni_bios)
			log_cb(RETRO_LOG_WARN, "[FBNEO] NeoGeo BIOS missing ...\n");

		set_neo_system_bios();

		// if we have a least one type of bios, we will be able to skip the asia-s3.rom non optional bios
		if (available_mvs_bios || available_aes_bios || available_uni_bios)
			is_neogeo_bios_available = true;
	}

	// Going over every rom to see if they are properly loaded before we continue ...
	for (unsigned i = 0; i < g_rom_count; i++)
	{
		if (g_find_list[i].nState != STAT_OK)
		{
			if(!(g_find_list[i].ri.nType & BRF_OPT))
			{
				// make the asia-s3.rom [0x91B64BE3] (mvs_bioses[0]) optional if we have another bios available
				if (is_neogeo_game && g_find_list[i].ri.nCrc == mvs_bioses[0].crc && is_neogeo_bios_available)
					continue;

				log_cb(RETRO_LOG_ERROR, "[FBNEO] ROM at index %d with CRC 0x%08x is required ...\n", i, g_find_list[i].ri.nCrc);
				return false;
			}
		}
	}

	BurnExtLoadRom = archive_load_rom;
	return true;
}

static void SetRotation()
{
	unsigned rotation;
	switch (BurnDrvGetFlags() & (BDF_ORIENTATION_FLIPPED | BDF_ORIENTATION_VERTICAL))
	{
		case BDF_ORIENTATION_VERTICAL:
			rotation = (bVerticalMode ? 0 : 1);
			break;
		case BDF_ORIENTATION_FLIPPED:
			rotation = (bVerticalMode ? 1 : 2);
			break;
		case BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED:
			rotation = (bVerticalMode ? 2 : 3);
			break;
		default:
			rotation = (bVerticalMode ? 3 : 0);;
			break;
	}
	environ_cb(RETRO_ENVIRONMENT_SET_ROTATION, &rotation);
}

#ifdef AUTOGEN_DATS
int CreateAllDatfiles()
{
	INT32 nRet = 0;
	TCHAR szFilename[MAX_PATH];

	snprintf(szFilename, sizeof(szFilename), "%s%c%s (%s).dat", "dats", path_default_slash_c(), APP_TITLE, "ClrMame Pro XML, Arcade only");
	create_datfile(szFilename, DAT_ARCADE_ONLY);

	snprintf(szFilename, sizeof(szFilename), "%s%c%s (%s).dat", "dats", path_default_slash_c(), APP_TITLE, "ClrMame Pro XML, Megadrive only");
	create_datfile(szFilename, DAT_MEGADRIVE_ONLY);

	snprintf(szFilename, sizeof(szFilename), "%s%c%s (%s).dat", "dats", path_default_slash_c(), APP_TITLE, "ClrMame Pro XML, PC-Engine only");
	create_datfile(szFilename, DAT_PCENGINE_ONLY);

	snprintf(szFilename, sizeof(szFilename), "%s%c%s (%s).dat", "dats", path_default_slash_c(), APP_TITLE, "ClrMame Pro XML, TurboGrafx16 only");
	create_datfile(szFilename, DAT_TG16_ONLY);

	snprintf(szFilename, sizeof(szFilename), "%s%c%s (%s).dat", "dats", path_default_slash_c(), APP_TITLE, "ClrMame Pro XML, SuprGrafx only");
	create_datfile(szFilename, DAT_SGX_ONLY);

	snprintf(szFilename, sizeof(szFilename), "%s%c%s (%s).dat", "dats", path_default_slash_c(), APP_TITLE, "ClrMame Pro XML, Sega SG-1000 only");
	create_datfile(szFilename, DAT_SG1000_ONLY);

	snprintf(szFilename, sizeof(szFilename), "%s%c%s (%s).dat", "dats", path_default_slash_c(), APP_TITLE, "ClrMame Pro XML, ColecoVision only");
	create_datfile(szFilename, DAT_COLECO_ONLY);

	snprintf(szFilename, sizeof(szFilename), "%s%c%s (%s).dat", "dats", path_default_slash_c(), APP_TITLE, "ClrMame Pro XML, Master System only");
	create_datfile(szFilename, DAT_MASTERSYSTEM_ONLY);

	snprintf(szFilename, sizeof(szFilename), "%s%c%s (%s).dat", "dats", path_default_slash_c(), APP_TITLE, "ClrMame Pro XML, Game Gear only");
	create_datfile(szFilename, DAT_GAMEGEAR_ONLY);

	snprintf(szFilename, sizeof(szFilename), "%s%c%s (%s).dat", "dats", path_default_slash_c(), APP_TITLE, "ClrMame Pro XML, MSX 1 Games only");
	create_datfile(szFilename, DAT_MSX_ONLY);

	snprintf(szFilename, sizeof(szFilename), "%s%c%s (%s).dat", "dats", path_default_slash_c(), APP_TITLE, "ClrMame Pro XML, ZX Spectrum Games only");
	create_datfile(szFilename, DAT_SPECTRUM_ONLY);

	snprintf(szFilename, sizeof(szFilename), "%s%c%s (%s).dat", "dats", path_default_slash_c(), APP_TITLE, "ClrMame Pro XML, Neogeo only");
	create_datfile(szFilename, DAT_NEOGEO_ONLY);

	return nRet;
}
#endif

void retro_init()
{
	struct retro_log_callback log;

	if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
		log_cb = log.log;
	else
		log_cb = log_dummy;

	if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
		bLibretroSupportsBitmasks = true;

	snprintf(szAppBurnVer, sizeof(szAppBurnVer), "%x.%x.%x.%02x", nBurnVer >> 20, (nBurnVer >> 16) & 0x0F, (nBurnVer >> 8) & 0xFF, nBurnVer & 0xFF);
	BurnLibInit();
#ifdef AUTOGEN_DATS
	CreateAllDatfiles();
#endif
}

void retro_deinit()
{
	BurnLibExit();
	bLibretroSupportsBitmasks = false;
}

void retro_reset()
{
	// restore the NeoSystem because it was changed during the gameplay
	if (is_neogeo_game)
		set_neo_system_bios();

	if (pgi_reset)
	{
		pgi_reset->Input.nVal = 1;
		*(pgi_reset->Input.pVal) = pgi_reset->Input.nVal;
	}

	check_variables();

	apply_dipswitch_from_variables();

	ForceFrameStep(1);
}

void retro_run()
{
	pBurnDraw = pVidImage;

	InputMake();

	ForceFrameStep(nCurrentFrame % nFrameskip == 0);

	audio_batch_cb(g_audio_buf, nBurnSoundLen);
	bool updated = false;

	if (bVidImageNeedRealloc)
	{
		bVidImageNeedRealloc = false;
		if (pVidImage)
			pVidImage = (UINT8*)realloc(pVidImage, nGameWidth * nGameHeight * nBurnBpp);
		else
			pVidImage = (UINT8*)malloc(nGameWidth * nGameHeight * nBurnBpp);
	}

	video_cb(pVidImage, nGameWidth, nGameHeight, nBurnPitch);

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
	{
		neo_geo_modes old_g_opt_neo_geo_mode = g_opt_neo_geo_mode;
		bool old_bVerticalMode = bVerticalMode;

		check_variables();

		apply_dipswitch_from_variables();

		// change orientation/geometry if vertical mode was toggled on/off
		if (old_bVerticalMode != bVerticalMode)
		{
			SetRotation();
			struct retro_system_av_info av_info;
			retro_get_system_av_info(&av_info);
			environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &av_info);
		}

		// reset the game if the user changed the bios
		if (old_g_opt_neo_geo_mode != g_opt_neo_geo_mode)
		{
			retro_reset();
		}
	}
}

static uint8_t *write_state_ptr;
static const uint8_t *read_state_ptr;
static unsigned state_sizes[2];

static int burn_write_state_cb(BurnArea *pba)
{
	memcpy(write_state_ptr, pba->Data, pba->nLen);
	write_state_ptr += pba->nLen;
	return 0;
}

static int burn_read_state_cb(BurnArea *pba)
{
	memcpy(pba->Data, read_state_ptr, pba->nLen);
	read_state_ptr += pba->nLen;
	return 0;
}

static int burn_dummy_state_cb(BurnArea *pba)
{
#ifdef FBNEO_DEBUG
	log_cb(RETRO_LOG_INFO, "state debug: name %s, len %d\n", pba->szName, pba->nLen);
#endif
	state_sizes[kNetGame] += pba->nLen;
	return 0;
}

size_t retro_serialize_size()
{
	int result = -1;
	environ_cb(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &result);
	kNetGame = result & 4 ? 1 : 0;
	// hiscores are causing desync in netplay
	if (kNetGame == 1)
		EnableHiscores = false;
	if (state_sizes[kNetGame])
		return state_sizes[kNetGame];

	BurnAcb = burn_dummy_state_cb;
	BurnAreaScan(ACB_FULLSCAN, 0);
	return state_sizes[kNetGame];
}

bool retro_serialize(void *data, size_t size)
{
	int result = -1;
	environ_cb(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &result);
	kNetGame = result & 4 ? 1 : 0;
	// hiscores are causing desync in netplay
	if (kNetGame == 1)
		EnableHiscores = false;
	if (!state_sizes[kNetGame])
	{
		BurnAcb = burn_dummy_state_cb;
		BurnAreaScan(ACB_FULLSCAN, 0);
	}
	if (size != state_sizes[kNetGame])
		return false;

	BurnAcb = burn_write_state_cb;
	write_state_ptr = (uint8_t*)data;
	BurnAreaScan(ACB_FULLSCAN | ACB_READ, 0);   
	return true;
}

bool retro_unserialize(const void *data, size_t size)
{
	int result = -1;
	environ_cb(RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE, &result);
	kNetGame = result & 4 ? 1 : 0;
	// hiscores are causing desync in netplay
	if (kNetGame == 1)
		EnableHiscores = false;
	if (!state_sizes[kNetGame])
	{
		BurnAcb = burn_dummy_state_cb;
		BurnAreaScan(ACB_FULLSCAN, 0);
	}
	if (size != state_sizes[kNetGame])
		return false;

	BurnAcb = burn_read_state_cb;
	read_state_ptr = (const uint8_t*)data;
	BurnAreaScan(ACB_FULLSCAN | ACB_WRITE, 0);
	BurnRecalcPal();
	return true;
}

void retro_cheat_reset() {}
void retro_cheat_set(unsigned, bool, const char*) {}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
	int game_aspect_x, game_aspect_y;
	bVidImageNeedRealloc = true;
	BurnDrvGetAspect(&game_aspect_x, &game_aspect_y);

	int maximum = nGameWidth > nGameHeight ? nGameWidth : nGameHeight;
	struct retro_game_geometry geom = { (unsigned)nGameWidth, (unsigned)nGameHeight, (unsigned)maximum, (unsigned)maximum };

	geom.aspect_ratio = (bVerticalMode ? ((float)game_aspect_y / (float)game_aspect_x) : ((float)game_aspect_x / (float)game_aspect_y));

	struct retro_system_timing timing = { (nBurnFPS / 100.0), (nBurnFPS / 100.0) * nAudSegLen };

	info->geometry = geom;
	info->timing   = timing;
}

static UINT32 __cdecl HighCol15(INT32 r, INT32 g, INT32 b, INT32  /* i */)
{
	UINT32 t;
	t =(r<<7)&0x7c00;
	t|=(g<<2)&0x03e0;
	t|=(b>>3)&0x001f;
	return t;
}

// 16-bit
static UINT32 __cdecl HighCol16(INT32 r, INT32 g, INT32 b, INT32 /* i */)
{
	UINT32 t;
	t =(r<<8)&0xf800;
	t|=(g<<3)&0x07e0;
	t|=(b>>3)&0x001f;
	return t;
}

// 32-bit
static UINT32 __cdecl HighCol32(INT32 r, INT32 g, INT32 b, INT32  /* i */)
{
	UINT32 t;
	t =(r<<16)&0xff0000;
	t|=(g<<8 )&0x00ff00;
	t|=(b    )&0x0000ff;

	return t;
}

INT32 SetBurnHighCol(INT32 nDepth)
{
	BurnRecalcPal();

	enum retro_pixel_format fmt;

	if (nDepth == 32) {
		fmt = RETRO_PIXEL_FORMAT_XRGB8888;
		if(environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
		{
			BurnHighCol = HighCol32;
			nBurnBpp = 4;
			return 0;
		}
	}

	fmt = RETRO_PIXEL_FORMAT_RGB565;
	if(environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
	{
		BurnHighCol = HighCol16;
		nBurnBpp = 2;
		return 0;
	}

	fmt = RETRO_PIXEL_FORMAT_0RGB1555;
	if(environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
	{
		BurnHighCol = HighCol15;
		nBurnBpp = 2;
		return 0;
	}

	return 0;
}

static void SetColorDepth()
{
	if ((BurnDrvGetFlags() & BDF_16BIT_ONLY) || !bAllowDepth32) {
		SetBurnHighCol(16);
	} else {
		SetBurnHighCol(32);
	}
	nBurnPitch = nGameWidth * nBurnBpp;
}

static void init_audio_buffer(INT32 sample_rate, INT32 fps)
{
	nAudSegLen = (sample_rate * 100 + (fps >> 1)) / fps;
	if (g_audio_buf)
		g_audio_buf = (int16_t*)realloc(g_audio_buf, nAudSegLen<<2 * sizeof(int16_t));
	else
		g_audio_buf = (int16_t*)calloc(nAudSegLen<<2, sizeof(int16_t));
	nBurnSoundLen = nAudSegLen;
	pBurnSoundOut = g_audio_buf;
}

static void extract_basename(char *buf, const char *path, size_t size, char *prefix)
{
	strcpy(buf, prefix);
	strncat(buf, path_basename(path), size - 1);
	buf[size - 1] = '\0';

	char *ext = strrchr(buf, '.');
	if (ext)
		*ext = '\0';
}

static void extract_directory(char *buf, const char *path, size_t size)
{
	strncpy(buf, path, size - 1);
	buf[size - 1] = '\0';

	char *base = strrchr(buf, path_default_slash_c());

	if (base)
		*base = '\0';
	else
	{
		buf[0] = '.';
		buf[1] = '\0';
	}
}

static bool retro_load_game_common()
{
	const char *dir = NULL;
	// If save directory is defined use it, ...
	if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir) && dir) {
		strncpy(g_save_dir, dir, sizeof(g_save_dir));
		log_cb(RETRO_LOG_INFO, "Setting save dir to %s\n", g_save_dir);
	} else {
		// ... otherwise use rom directory
		strncpy(g_save_dir, g_rom_dir, sizeof(g_save_dir));
		log_cb(RETRO_LOG_ERROR, "Save dir not defined => use roms dir %s\n", g_save_dir);
	}

	// If system directory is defined use it, ...
	if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir) {
		strncpy(g_system_dir, dir, sizeof(g_system_dir));
		log_cb(RETRO_LOG_INFO, "Setting system dir to %s\n", g_system_dir);
	} else {
		// ... otherwise use rom directory
		strncpy(g_system_dir, g_rom_dir, sizeof(g_system_dir));
		log_cb(RETRO_LOG_ERROR, "System dir not defined => use roms dir %s\n", g_system_dir);
	}

	// Initialize EEPROM path
	snprintf (szAppEEPROMPath, sizeof(szAppEEPROMPath), "%s%cfbneo%c", g_save_dir, path_default_slash_c(), path_default_slash_c());

	// Create EEPROM path if it does not exist
	path_mkdir(szAppEEPROMPath);

	// Initialize Hiscore path
	snprintf (szAppHiscorePath, sizeof(szAppHiscorePath), "%s%cfbneo%c", g_system_dir, path_default_slash_c(), path_default_slash_c());

	// Initialize Samples path
	snprintf (szAppSamplesPath, sizeof(szAppSamplesPath), "%s%cfbneo%csamples%c", g_system_dir, path_default_slash_c(), path_default_slash_c(), path_default_slash_c());

	// Initialize HDD path
	snprintf (szAppHDDPath, sizeof(szAppHDDPath), "%s%c", g_rom_dir, path_default_slash_c());

	// Intialize state_sizes (for serialization)
	state_sizes[0] = 0;
	state_sizes[1] = 0;

	nBurnDrvActive = BurnDrvGetIndexByName(g_driver_name);
	if (nBurnDrvActive < nBurnDrvCount) {
		// If the game is marked as not working, let's stop here
		if (!(BurnDrvIsWorking())) {
			log_cb(RETRO_LOG_ERROR, "[FBNEO] Can't launch this game, it is marked as not working\n");
			return false;
		}

		// If the game is a bios, let's stop here
		if ((BurnDrvGetFlags() & BDF_BOARDROM)) {
			log_cb(RETRO_LOG_ERROR, "[FBNEO] Can't launch a bios this way\n");
			return false;
		}

		const char * boardrom = BurnDrvGetTextA(DRV_BOARDROM);
		is_neogeo_game = (boardrom && strcmp(boardrom, "neogeo") == 0);

		// Define nMaxPlayers early;
		nMaxPlayers = BurnDrvGetMaxPlayers();
		SetControllerInfo();

		// Initialize inputs
		InputInit();

		// Initialize dipswitches
		InpDIPSWInit();

		// Initialize debug variables
		nBurnLayer = 0xff;
		nSpriteEnable = 0xff;

		set_environment();
		check_variables();

#ifdef USE_CYCLONE
		SetSekCpuCore();
#endif
		if (!open_archive()) {
			log_cb(RETRO_LOG_ERROR, "[FBNEO] Can't launch this game, some files are missing.\n");
			return false;
		}

		// Announcing to fbneo which samplerate we want
		nBurnSoundRate = g_audio_samplerate;

		// Some game drivers won't initialize with an undefined nBurnSoundLen
		init_audio_buffer(nBurnSoundRate, 6000);

		// Start CD reader emulation if needed
		if (nGameType == RETRO_GAME_TYPE_NEOCD) {
			if (CDEmuInit()) {
				log_cb(RETRO_LOG_INFO, "[FBNEO] Starting neogeo CD\n");
			}
		}

		// Apply dipswitches
		apply_dipswitch_from_variables();

		// Initialize game driver
		BurnDrvInit();

		// Now we know real game fps, let's initialize sound buffer again
		init_audio_buffer(nBurnSoundRate, nBurnFPS);

		// Get MainRam for RetroAchievements support
		INT32 nMin = 0;
		BurnAcb = StateGetMainRamAcb;
		BurnAreaScan(ACB_FULLSCAN, &nMin);
		if (bMainRamFound) {
			log_cb(RETRO_LOG_INFO, "[Cheevos] System RAM set to %p %zu\n", MainRamData, MainRamSize);
		}

		// Loading minimal savestate (not exactly sure why it is needed)
		snprintf (g_autofs_path, sizeof(g_autofs_path), "%s%cfbneo%c%s.fs", g_save_dir, path_default_slash_c(), path_default_slash_c(), BurnDrvGetTextA(DRV_NAME));
		if (BurnStateLoad(g_autofs_path, 0, NULL) == 0)
			log_cb(RETRO_LOG_INFO, "[FBNEO] EEPROM succesfully loaded from %s\n", g_autofs_path);

		if (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0) {
			log_cb(RETRO_LOG_WARN, "[FBNEO] %s\n", BurnDrvGetTextA(DRV_COMMENT));
		}

		// Initializing display, autorotate if needed
		BurnDrvGetFullSize(&nGameWidth, &nGameHeight);
		SetRotation();
		SetColorDepth();

		if (pVidImage)
			pVidImage = (UINT8*)realloc(pVidImage, nGameWidth * nGameHeight * nBurnBpp);
		else
			pVidImage = (UINT8*)malloc(nGameWidth * nGameHeight * nBurnBpp);

		// Initialization done
		log_cb(RETRO_LOG_INFO, "Driver %s was successfully started : game's full name is %s\n", g_driver_name, BurnDrvGetTextA(DRV_FULLNAME));
		driver_inited = true;

		return true;
	}
	return false;
}

bool retro_load_game(const struct retro_game_info *info)
{
	if (!info)
		return false;
	
	extract_basename(g_driver_name, info->path, sizeof(g_driver_name), "");
	extract_directory(g_rom_dir, info->path, sizeof(g_rom_dir));
	extract_basename(g_rom_parent_dir, g_rom_dir, sizeof(g_rom_parent_dir),"");
	char * prefix="";
	if (bLoadSubsystemByParent == true) {
		if(strcmp(g_rom_parent_dir, "coleco")==0) {
			log_cb(RETRO_LOG_INFO, "[FBNEO] subsystem cv identified from parent folder\n");
			prefix = "cv_";
		}
		if(strcmp(g_rom_parent_dir, "gamegear")==0) {
			log_cb(RETRO_LOG_INFO, "[FBNEO] subsystem gg identified from parent folder\n");
			prefix = "gg_";
		}
		if(strcmp(g_rom_parent_dir, "megadriv")==0) {
			log_cb(RETRO_LOG_INFO, "[FBNEO] subsystem md identified from parent folder\n");
			prefix = "md_";
		}
		if(strcmp(g_rom_parent_dir, "msx")==0) {
			log_cb(RETRO_LOG_INFO, "[FBNEO] subsystem msx identified from parent folder\n");
			prefix = "msx_";
		}
		if(strcmp(g_rom_parent_dir, "pce")==0) {
			log_cb(RETRO_LOG_INFO, "[FBNEO] subsystem pce identified from parent folder\n");
			prefix = "pce_";
		}
		if(strcmp(g_rom_parent_dir, "sg1000")==0) {
			log_cb(RETRO_LOG_INFO, "[FBNEO] subsystem sg1k identified from parent folder\n");
			prefix = "sg1k_";
		}
		if(strcmp(g_rom_parent_dir, "sgx")==0) {
			log_cb(RETRO_LOG_INFO, "[FBNEO] subsystem sgx identified from parent folder\n");
			prefix = "sgx_";
		}
		if(strcmp(g_rom_parent_dir, "sms")==0) {
			log_cb(RETRO_LOG_INFO, "[FBNEO] subsystem sms identified from parent folder\n");
			prefix = "sms_";
		}
		if(strcmp(g_rom_parent_dir, "spectrum")==0) {
			log_cb(RETRO_LOG_INFO, "[FBNEO] subsystem spec identified from parent folder\n");
			prefix = "spec_";
		}
		if(strcmp(g_rom_parent_dir, "tg16")==0) {
			log_cb(RETRO_LOG_INFO, "[FBNEO] subsystem tg identified from parent folder\n");
			prefix = "tg_";
		}
		if(strcmp(g_rom_parent_dir, "neocd")==0) {
			log_cb(RETRO_LOG_INFO, "[FBNEO] subsystem neocd identified from parent folder\n");
			prefix = "";
			nGameType = RETRO_GAME_TYPE_NEOCD;
			strcpy(CDEmuImage, info->path);
			extract_basename(g_driver_name, "neocdz", sizeof(g_driver_name), prefix);
		} else {
			extract_basename(g_driver_name, info->path, sizeof(g_driver_name), prefix);
		}
	} else {
		extract_basename(g_driver_name, info->path, sizeof(g_driver_name), "");
	}

	return retro_load_game_common();
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t)
{
	if (!info)
		return false;

	nGameType = game_type;

	char * prefix;
	switch (nGameType) {
		case RETRO_GAME_TYPE_CV:
			prefix = "cv_";
			break;
		case RETRO_GAME_TYPE_GG:
			prefix = "gg_";
			break;
		case RETRO_GAME_TYPE_MD:
			prefix = "md_";
			break;
		case RETRO_GAME_TYPE_MSX:
			prefix = "msx_";
			break;
		case RETRO_GAME_TYPE_PCE:
			prefix = "pce_";
			break;
		case RETRO_GAME_TYPE_SG1K:
			prefix = "sg1k_";
			break;
		case RETRO_GAME_TYPE_SGX:
			prefix = "sgx_";
			break;
		case RETRO_GAME_TYPE_SMS:
			prefix = "sms_";
			break;
		case RETRO_GAME_TYPE_SPEC:
			prefix = "spec_";
			break;
		case RETRO_GAME_TYPE_TG:
			prefix = "tg_";
			break;
		case RETRO_GAME_TYPE_NEOCD:
			prefix = "";
			strcpy(CDEmuImage, info->path);
			break;
		default:
			return false;
			break;
	}

	extract_basename(g_driver_name, info->path, sizeof(g_driver_name), prefix);
	extract_directory(g_rom_dir, info->path, sizeof(g_rom_dir));

	if(nGameType == RETRO_GAME_TYPE_NEOCD)
		extract_basename(g_driver_name, "neocdz", sizeof(g_driver_name), "");

	return retro_load_game_common();
}

void retro_unload_game(void)
{
	if (driver_inited)
	{
		if (BurnStateSave(g_autofs_path, 0) == 0 && path_is_valid(g_autofs_path))
			log_cb(RETRO_LOG_INFO, "[FBNEO] EEPROM succesfully saved to %s\n", g_autofs_path);
		if (pVidImage) {
			free(pVidImage);
			pVidImage = NULL;
		}
		if (g_audio_buf) {
			free(g_audio_buf);
			g_audio_buf = NULL;
		}
		BurnDrvExit();
		if (nGameType == RETRO_GAME_TYPE_NEOCD)
			CDEmuExit();
	}
	InputDeInit();
	driver_inited = false;
}

unsigned retro_get_region() { return RETRO_REGION_NTSC; }

unsigned retro_api_version() { return RETRO_API_VERSION; }

static unsigned int BurnDrvGetIndexByName(const char* name)
{
	unsigned int ret = ~0U;
	for (unsigned int i = 0; i < nBurnDrvCount; i++) {
		nBurnDrvActive = i;
		if (strcmp(BurnDrvGetText(DRV_NAME), name) == 0) {
			ret = i;
			break;
		}
	}
	return ret;
}

#ifdef ANDROID
#include <wchar.h>

size_t mbstowcs(wchar_t *pwcs, const char *s, size_t n)
{
	if (pwcs == NULL)
		return strlen(s);
	return mbsrtowcs(pwcs, &s, n, NULL);
}

size_t wcstombs(char *s, const wchar_t *pwcs, size_t n)
{
	return wcsrtombs(s, &pwcs, n, NULL);
}

#endif

// Driver Save State module
// If bAll=0 save/load all non-volatile ram to .fs
// If bAll=1 save/load all ram to .fs

// ------------ State len --------------------
static INT32 nTotalLen = 0;

static INT32 __cdecl StateLenAcb(struct BurnArea* pba)
{
	nTotalLen += pba->nLen;

	return 0;
}

static INT32 StateInfo(INT32* pnLen, INT32* pnMinVer, INT32 bAll)
{
	INT32 nMin = 0;
	nTotalLen = 0;
	BurnAcb = StateLenAcb;

	BurnAreaScan(ACB_NVRAM, &nMin);                  // Scan nvram
	if (bAll) {
		INT32 m;
		BurnAreaScan(ACB_MEMCARD, &m);               // Scan memory card
		if (m > nMin) {                           // Up the minimum, if needed
			nMin = m;
		}
		BurnAreaScan(ACB_VOLATILE, &m);               // Scan volatile ram
		if (m > nMin) {                           // Up the minimum, if needed
			nMin = m;
		}
	}
	*pnLen = nTotalLen;
	*pnMinVer = nMin;

	return 0;
}

// State load
INT32 BurnStateLoadEmbed(FILE* fp, INT32 nOffset, INT32 bAll, INT32 (*pLoadGame)())
{
	const char* szHeader = "FS1 ";                  // Chunk identifier

	INT32 nLen = 0;
	INT32 nMin = 0, nFileVer = 0, nFileMin = 0;
	INT32 t1 = 0, t2 = 0;
	char ReadHeader[4];
	char szForName[33];
	INT32 nChunkSize = 0;
	UINT8 *Def = NULL;
	INT32 nDefLen = 0;                           // Deflated version
	INT32 nRet = 0;

	if (nOffset >= 0) {
		fseek(fp, nOffset, SEEK_SET);
	} else {
		if (nOffset == -2) {
			fseek(fp, 0, SEEK_END);
		} else {
			fseek(fp, 0, SEEK_CUR);
		}
	}

	memset(ReadHeader, 0, 4);
	fread(ReadHeader, 1, 4, fp);                  // Read identifier
	if (memcmp(ReadHeader, szHeader, 4)) {            // Not the right file type
		return -2;
	}

	fread(&nChunkSize, 1, 4, fp);
	if (nChunkSize <= 0x40) {                     // Not big enough
		return -1;
	}

	INT32 nChunkData = ftell(fp);

	fread(&nFileVer, 1, 4, fp);                     // Version of FB that this file was saved from

	fread(&t1, 1, 4, fp);                        // Min version of FB that NV  data will work with
	fread(&t2, 1, 4, fp);                        // Min version of FB that All data will work with

	if (bAll) {                                 // Get the min version number which applies to us
		nFileMin = t2;
	} else {
		nFileMin = t1;
	}

	fread(&nDefLen, 1, 4, fp);                     // Get the size of the compressed data block

	memset(szForName, 0, sizeof(szForName));
	fread(szForName, 1, 32, fp);

	if (nBurnVer < nFileMin) {                     // Error - emulator is too old to load this state
		return -5;
	}

	// Check the game the savestate is for, and load it if needed.
	{
		bool bLoadGame = false;

		if (nBurnDrvActive < nBurnDrvCount) {
			if (strcmp(szForName, BurnDrvGetTextA(DRV_NAME))) {   // The save state is for the wrong game
				bLoadGame = true;
			}
		} else {                              // No game loaded
			bLoadGame = true;
		}

		if (bLoadGame) {
			UINT32 nCurrentGame = nBurnDrvActive;
			UINT32 i;
			for (i = 0; i < nBurnDrvCount; i++) {
				nBurnDrvActive = i;
				if (strcmp(szForName, BurnDrvGetTextA(DRV_NAME)) == 0) {
					break;
				}
			}
			if (i == nBurnDrvCount) {
				nBurnDrvActive = nCurrentGame;
				return -3;
			} else {
				if (pLoadGame == NULL) {
					return -1;
				}
				if (pLoadGame()) {
					return -1;
				}
			}
		}
	}

	StateInfo(&nLen, &nMin, bAll);
	if (nLen <= 0) {                           // No memory to load
		return -1;
	}

	// Check if the save state is okay
	if (nFileVer < nMin) {                        // Error - this state is too old and cannot be loaded.
		return -4;
	}

	fseek(fp, nChunkData + 0x30, SEEK_SET);            // Read current frame
	fread(&nCurrentFrame, 1, 4, fp);               //

	fseek(fp, 0x0C, SEEK_CUR);                     // Move file pointer to the start of the compressed block
	Def = (UINT8*)malloc(nDefLen);
	if (Def == NULL) {
		return -1;
	}
	memset(Def, 0, nDefLen);
	fread(Def, 1, nDefLen, fp);                     // Read in deflated block

	nRet = BurnStateDecompress(Def, nDefLen, bAll);      // Decompress block into driver
	if (Def) {
		free(Def);                                 // free deflated block
		Def = NULL;
	}

	fseek(fp, nChunkData + nChunkSize, SEEK_SET);

	if (nRet) {
		return -1;
	} else {
		return 0;
	}
}

// State load
INT32 BurnStateLoad(TCHAR* szName, INT32 bAll, INT32 (*pLoadGame)())
{
	const char szHeader[] = "FB1 ";                  // File identifier
	char szReadHeader[4] = "";
	INT32 nRet = 0;

	FILE* fp = _tfopen(szName, _T("rb"));
	if (fp == NULL) {
		return 1;
	}

	fread(szReadHeader, 1, 4, fp);                  // Read identifier
	if (memcmp(szReadHeader, szHeader, 4) == 0) {      // Check filetype
		nRet = BurnStateLoadEmbed(fp, -1, bAll, pLoadGame);
	}
	fclose(fp);

	if (nRet < 0) {
		return -nRet;
	} else {
		return 0;
	}
}

// Write a savestate as a chunk of an "FB1 " file
// nOffset is the absolute offset from the beginning of the file
// -1: Append at current position
// -2: Append at EOF
INT32 BurnStateSaveEmbed(FILE* fp, INT32 nOffset, INT32 bAll)
{
	const char* szHeader = "FS1 ";                  // Chunk identifier

	INT32 nLen = 0;
	INT32 nNvMin = 0, nAMin = 0;
	INT32 nZero = 0;
	char szGame[33];
	UINT8 *Def = NULL;
	INT32 nDefLen = 0;                           // Deflated version
	INT32 nRet = 0;

	if (fp == NULL) {
		return -1;
	}

	StateInfo(&nLen, &nNvMin, 0);                  // Get minimum version for NV part
	nAMin = nNvMin;
	if (bAll) {                                 // Get minimum version for All data
		StateInfo(&nLen, &nAMin, 1);
	}

	if (nLen <= 0) {                           // No memory to save
		return -1;
	}

	if (nOffset >= 0) {
		fseek(fp, nOffset, SEEK_SET);
	} else {
		if (nOffset == -2) {
			fseek(fp, 0, SEEK_END);
		} else {
			fseek(fp, 0, SEEK_CUR);
		}
	}

	fwrite(szHeader, 1, 4, fp);                     // Chunk identifier
	INT32 nSizeOffset = ftell(fp);                  // Reserve space to write the size of this chunk
	fwrite(&nZero, 1, 4, fp);                     //

	fwrite(&nBurnVer, 1, 4, fp);                  // Version of FB this was saved from
	fwrite(&nNvMin, 1, 4, fp);                     // Min version of FB NV  data will work with
	fwrite(&nAMin, 1, 4, fp);                     // Min version of FB All data will work with

	fwrite(&nZero, 1, 4, fp);                     // Reserve space to write the compressed data size

	memset(szGame, 0, sizeof(szGame));               // Game name
	sprintf(szGame, "%.32s", BurnDrvGetTextA(DRV_NAME));         //
	fwrite(szGame, 1, 32, fp);                     //

	fwrite(&nCurrentFrame, 1, 4, fp);               // Current frame

	fwrite(&nZero, 1, 4, fp);                     // Reserved
	fwrite(&nZero, 1, 4, fp);                     //
	fwrite(&nZero, 1, 4, fp);                     //

	nRet = BurnStateCompress(&Def, &nDefLen, bAll);      // Compress block from driver and return deflated buffer
	if (Def == NULL) {
		return -1;
	}

	nRet = fwrite(Def, 1, nDefLen, fp);               // Write block to disk
	if (Def) {
		free(Def);                                 // free deflated block and close file
		Def = NULL;
	}

	if (nRet != nDefLen) {                        // error writing block to disk
		return -1;
	}

	if (nDefLen & 3) {                           // Chunk size must be a multiple of 4
		fwrite(&nZero, 1, 4 - (nDefLen & 3), fp);      // Pad chunk if needed
	}

	fseek(fp, nSizeOffset + 0x10, SEEK_SET);         // Write size of the compressed data
	fwrite(&nDefLen, 1, 4, fp);                     //

	nDefLen = (nDefLen + 0x43) & ~3;               // Add for header size and align

	fseek(fp, nSizeOffset, SEEK_SET);               // Write size of the chunk
	fwrite(&nDefLen, 1, 4, fp);                     //

	fseek (fp, 0, SEEK_END);                     // Set file pointer to the end of the chunk

	return nDefLen;
}

// State save
INT32 BurnStateSave(TCHAR* szName, INT32 bAll)
{
	const char szHeader[] = "FB1 ";                  // File identifier
	INT32 nLen = 0, nVer = 0;
	INT32 nRet = 0;

	if (bAll) {                                 // Get amount of data
		StateInfo(&nLen, &nVer, 1);
	} else {
		StateInfo(&nLen, &nVer, 0);
	}
	if (nLen <= 0) {                           // No data, so exit without creating a savestate
		return 0;                              // Don't return an error code
	}

	FILE* fp = fopen(szName, _T("wb"));
	if (fp == NULL) {
		return 1;
	}

	fwrite(&szHeader, 1, 4, fp);
	nRet = BurnStateSaveEmbed(fp, -1, bAll);
	fclose(fp);

	if (nRet < 0) {
		return 1;
	} else {
		return 0;
	}
}

char* DecorateGameName(UINT32 nBurnDrv)
{
	static char szDecoratedName[256];
	UINT32 nOldBurnDrv = nBurnDrvActive;

	nBurnDrvActive = nBurnDrv;

	const char* s1 = "";
	const char* s2 = "";
	const char* s3 = "";
	const char* s4 = "";
	const char* s5 = "";
	const char* s6 = "";
	const char* s7 = "";
	const char* s8 = "";
	const char* s9 = "";
	const char* s10 = "";
	const char* s11 = "";
	const char* s12 = "";
	const char* s13 = "";
	const char* s14 = "";

	s1 = BurnDrvGetTextA(DRV_FULLNAME);
	if ((BurnDrvGetFlags() & BDF_DEMO) || (BurnDrvGetFlags() & BDF_HACK) || (BurnDrvGetFlags() & BDF_HOMEBREW) || (BurnDrvGetFlags() & BDF_PROTOTYPE) || (BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
		s2 = " [";
		if (BurnDrvGetFlags() & BDF_DEMO) {
			s3 = "Demo";
			if ((BurnDrvGetFlags() & BDF_HACK) || (BurnDrvGetFlags() & BDF_HOMEBREW) || (BurnDrvGetFlags() & BDF_PROTOTYPE) || (BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
				s4 = ", ";
			}
		}
		if (BurnDrvGetFlags() & BDF_HACK) {
			s5 = "Hack";
			if ((BurnDrvGetFlags() & BDF_HOMEBREW) || (BurnDrvGetFlags() & BDF_PROTOTYPE) || (BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
				s6 = ", ";
			}
		}
		if (BurnDrvGetFlags() & BDF_HOMEBREW) {
			s7 = "Homebrew";
			if ((BurnDrvGetFlags() & BDF_PROTOTYPE) || (BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
				s8 = ", ";
			}
		}
		if (BurnDrvGetFlags() & BDF_PROTOTYPE) {
			s9 = "Prototype";
			if ((BurnDrvGetFlags() & BDF_BOOTLEG) || (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0)) {
				s10 = ", ";
			}
		}		
		if (BurnDrvGetFlags() & BDF_BOOTLEG) {
			s11 = "Bootleg";
			if (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0) {
				s12 = ", ";
			}
		}
		if (BurnDrvGetTextA(DRV_COMMENT) && strlen(BurnDrvGetTextA(DRV_COMMENT)) > 0) {
			s13 = BurnDrvGetTextA(DRV_COMMENT);
		}
		s14 = "]";
	}

	sprintf(szDecoratedName, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s", s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14);

	nBurnDrvActive = nOldBurnDrv;
	return szDecoratedName;
}
