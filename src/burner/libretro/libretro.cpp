#include "libretro.h"
#include "burner.h"
#include "inp_keys.h"
#include "state.h"
#include <string.h>
#include <stdio.h>

#include <vector>
#include <string>
#include <ctype.h>

#include "cd_interface.h"

static unsigned int BurnDrvGetIndexByName(const char* name);

#define STAT_NOFIND	0
#define STAT_OK		1
#define STAT_CRC	   2
#define STAT_SMALL	3
#define STAT_LARGE	4

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

#define AUDIO_SEGMENT_LENGTH 534 // <-- Hardcoded value that corresponds well to 32kHz audio.
#define AUDIO_SEGMENT_LENGTH_TIMES_CHANNELS (534 * 2)

static uint16_t g_fba_frame[1024 * 1024];
static int16_t g_audio_buf[AUDIO_SEGMENT_LENGTH_TIMES_CHANNELS];

// libretro globals

static retro_environment_t environ_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t poll_cb;
static retro_input_state_t input_cb;
static retro_audio_sample_batch_t audio_batch_cb;
void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t) {}
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }
void retro_set_environment(retro_environment_t cb) { environ_cb = cb; }

static char g_rom_dir[1024];

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "FB Alpha";
   info->library_version = "0.2.97.26";
   info->need_fullpath = true;
   info->block_extract = true;
   info->valid_extensions = "zip|ZIP";
}

/////
static void poll_input();
static bool init_input();

// FBA stubs
unsigned ArcadeJoystick;

int bDrvOkay;
int bRunPause;
bool bAlwaysProcessKeyboardInput;

bool bDoIpsPatch;
void IpsApplyPatches(UINT8 *, char *) {}

TCHAR szAppHiscorePath[MAX_PATH];
TCHAR szAppSamplesPath[MAX_PATH];
TCHAR szAppBurnVer[16];

CDEmuStatusValue CDEmuStatus;

const char* isowavLBAToMSF(const int LBA) { return ""; }
int isowavMSFToLBA(const char* address) { return 0; }
TCHAR* GetIsoPath() { return NULL; }
INT32 CDEmuInit() { return 0; }
INT32 CDEmuExit() { return 0; }
INT32 CDEmuStop() { return 0; }
INT32 CDEmuPlay(UINT8 M, UINT8 S, UINT8 F) { return 0; }
INT32 CDEmuLoadSector(INT32 LBA, char* pBuffer) { return 0; }
UINT8* CDEmuReadTOC(INT32 track) { return 0; }
UINT8* CDEmuReadQChannel() { return 0; }
INT32 CDEmuGetSoundBuffer(INT16* buffer, INT32 samples) { return 0; }

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

int InputSetCooperativeLevel(const bool bExclusive, const bool bForeGround) { return 0; }

void Reinitialise(void)
{
#if 0 // ?!
	int width, height;
	BurnDrvGetVisibleSize(&width, &height);
	unsigned drv_flags = BurnDrvGetFlags();
	if (drv_flags & BDF_ORIENTATION_VERTICAL)
		nBurnPitch = height * sizeof(uint16_t);
	else
		nBurnPitch = width * sizeof(uint16_t);

	if (environ_cb)
	{
		BurnDrvGetVisibleSize(&width, &height);
		retro_geometry geom = { width, height, width, height };
		environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geom);
		environ_cb(RETRO_ENVIRONMENT_SET_PITCH, &nBurnPitch);
	}
#endif
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

int QuoteRead(char **, char **, char*) { return 1; }
char *LabelCheck(char *, char *) { return 0; }
const int nConfigMinVersion = 0x020921;

static int find_rom_by_crc(uint32_t crc, const ZipEntry *list, unsigned elems)
{
   for (unsigned i = 0; i < elems; i++)
   {
      if (list[i].nCrc == crc)
         return i;
   }

   return -1;
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

// This code is very confusing. The original code is even more confusing :(
static bool open_archive()
{
   memset(g_find_list, 0, sizeof(g_find_list));

   // FBA wants some roms ... Figure out how many.
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

      fprintf(stderr, "[FBA] Archive: %s\n", rom_name);

      char path[1024];
#ifdef _XBOX
      snprintf(path, sizeof(path), "%s\\%s", g_rom_dir, rom_name);
#else
      snprintf(path, sizeof(path), "%s/%s", g_rom_dir, rom_name);
#endif

      if (ZipOpen(path) != 0)
      {
         fprintf(stderr, "[FBA] Failed to find archive: %s\n", path);
         return false;
      }
      ZipClose();

      g_find_list_path.push_back(path);
   }

   for (unsigned z = 0; z < g_find_list_path.size(); z++)
   {
      if (ZipOpen((char*)g_find_list_path[z].c_str()) != 0)
      {
         fprintf(stderr, "[FBA] Failed to open archive %s\n", g_find_list_path[z].c_str());
         return false;
      }

      ZipEntry *list = NULL;
      int count;
      ZipGetList(&list, &count);

      // Try to map the ROMs FBA wants to ROMs we find inside our pretty archives ...
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
         if (index < 0)
            continue;

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

   // Going over every rom to see if they are properly loaded before we continue ...
   for (unsigned i = 0; i < g_rom_count; i++)
   {
      if (g_find_list[i].nState != STAT_OK)
      {
         fprintf(stderr, "[FBA] ROM index %i was not found ... CRC: 0x%08x\n",
               i, g_find_list[i].ri.nCrc);
         return false;
      }
   }

   BurnExtLoadRom = archive_load_rom;
   return true;
}

void retro_init()
{
   BurnLibInit();
}

void retro_deinit()
{
   BurnDrvExit();
   BurnLibExit();
}

static bool g_reset;
void retro_reset() { g_reset = true; }

void retro_run()
{
   int width, height;
   BurnDrvGetVisibleSize(&width, &height);
   pBurnDraw = (uint8_t*)g_fba_frame;

   nBurnLayer = 0xff;
   pBurnSoundOut = g_audio_buf;
   nBurnSoundRate = 32000;
   nBurnSoundLen = AUDIO_SEGMENT_LENGTH;
   nCurrentFrame++;

   poll_input();

   BurnDrvFrame();
   unsigned drv_flags = BurnDrvGetFlags();
   uint32_t height_tmp = height;
   if (drv_flags & (BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED))
   {
      nBurnPitch = height * sizeof(uint16_t);
      height = width;
      width = height_tmp;
   }
   else
      nBurnPitch = width * sizeof(uint16_t);

   video_cb(g_fba_frame, width, height, nBurnPitch);
   audio_batch_cb(g_audio_buf, AUDIO_SEGMENT_LENGTH);
}

static uint8_t *write_state_ptr;
static const uint8_t *read_state_ptr;
static unsigned state_size;

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
   state_size += pba->nLen;
   return 0;
}

size_t retro_serialize_size()
{
   if (state_size)
      return state_size;

   BurnAcb = burn_dummy_state_cb;
   state_size = 0;
   BurnAreaScan(ACB_VOLATILE | ACB_WRITE, 0);
   return state_size;
}

bool retro_serialize(void *data, size_t size)
{
   if (size != state_size)
      return false;

   BurnAcb = burn_write_state_cb;
   write_state_ptr = (uint8_t*)data;
   BurnAreaScan(ACB_VOLATILE | ACB_WRITE, 0);

   return true;
}

bool retro_unserialize(const void *data, size_t size)
{
   if (size != state_size)
      return false;
   BurnAcb = burn_read_state_cb;
   read_state_ptr = (const uint8_t*)data;
   BurnAreaScan(ACB_VOLATILE | ACB_READ, 0);

   return true;
}

void retro_cheat_reset() {}
void retro_cheat_set(unsigned, bool, const char*) {}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   int width, height;
   BurnDrvGetVisibleSize(&width, &height);
   int maximum = width > height ? width : height;
   struct retro_game_geometry geom = { width, height, maximum, maximum };

   struct retro_system_timing timing = { 60.0, 32000.0 };

   info->geometry = geom;
   info->timing   = timing;
}

static bool fba_init(unsigned driver)
{
   nBurnDrvActive = driver;

   if (!open_archive())
      return false;

   nFMInterpolation = 3;
   nInterpolation = 3;

   BurnDrvInit();

   int width, height;
   BurnDrvGetVisibleSize(&width, &height);
   unsigned drv_flags = BurnDrvGetFlags();
   if (drv_flags & BDF_ORIENTATION_VERTICAL)
      nBurnPitch = height * sizeof(uint16_t);
   else
      nBurnPitch = width * sizeof(uint16_t);

   return true;
}

static unsigned int HighCol15(int r, int g, int b, int  /* i */)
{
   unsigned int t = 0;
   t |= (r << 7) & 0x7c00;
   t |= (g << 2) & 0x03e0;
   t |= (b >> 3) & 0x001f;
   return t;
}

int VidRecalcPal()
{
   return BurnRecalcPal();
}

static void init_video()
{
   nBurnBpp = 2;
   VidRecalcPal();
   BurnHighCol = HighCol15;
}

static void init_audio()
{
   pBurnSoundOut = g_audio_buf;
   nBurnSoundRate = 32000;
   nBurnSoundLen = AUDIO_SEGMENT_LENGTH;
}

static void extract_basename(char *buf, const char *path, size_t size)
{
   const char *base = strrchr(path, '/');
   if (!base)
      base = strrchr(path, '\\');
   if (!base)
      base = path;

   if (*base == '\\' || *base == '/')
      base++;

   strncpy(buf, base, size - 1);
   buf[size - 1] = '\0';

   char *ext = strrchr(buf, '.');
   if (ext)
      *ext = '\0';
}

static void extract_directory(char *buf, const char *path, size_t size)
{
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   char *base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
      buf[0] = '\0';
}

bool retro_load_game(const struct retro_game_info *info)
{
   char basename[128];
   extract_basename(basename, info->path, sizeof(basename));
   extract_directory(g_rom_dir, info->path, sizeof(g_rom_dir));

   unsigned i = BurnDrvGetIndexByName(basename);
   if (i < nBurnDrvCount)
   {
      init_video();
      init_audio();

      if (!fba_init(i))
         return false;

      init_input();

      return true;
   }
   else
   {
      fprintf(stderr, "[FBA] Cannot find driver.\n");
      return false;
   }
}

bool retro_load_game_special(unsigned, const struct retro_game_info*, size_t) { return false; }

void retro_unload_game(void) {}

unsigned retro_get_region() { return RETRO_REGION_NTSC; }

void *retro_get_memory_data(unsigned) { return 0; }
size_t retro_get_memory_size(unsigned) { return 0; }

unsigned retro_api_version() { return RETRO_API_VERSION; }

void retro_set_controller_port_device(unsigned, unsigned) {}

// Input stuff.

// Ref GamcPlayer() in ../gamc.cpp
#define P1_COIN	FBK_5
#define P1_START  FBK_1
#define P1_LEFT   FBK_LEFTARROW
#define P1_RIGHT  FBK_RIGHTARROW
#define P1_UP     FBK_UPARROW
#define P1_DOWN   FBK_DOWNARROW
#define P1_FIRE1  FBK_A
#define P1_FIRE2  FBK_S
#define P1_FIRE3  FBK_D
#define P1_FIRE4  FBK_Z
#define P1_FIRE5  FBK_X
#define P1_FIRE6  FBK_C
#define P1_FIRED  FBK_V
#define P1_SERVICE FBK_F2

#define P2_COIN 0x07
#define P2_START 0x03
#define P2_LEFT 0x4000
#define P2_RIGHT 0x4001
#define P2_UP 0x4002
#define P2_DOWN 0x4003
#define P2_FIRE1 0x4080
#define P2_FIRE2 0x4081
#define P2_FIRE3 0x4082
#define P2_FIRE4 0x4083
#define P2_FIRE5 0x4084
#define P2_FIRE6 0x4085
#define P2_FIRED 0x4086

#define P3_COIN 0x08
#define P3_START 0x04
#define P3_LEFT 0x4100
#define P3_RIGHT 0x4101
#define P3_UP 0x4102
#define P3_DOWN 0x4103
#define P3_FIRE1 0x4180
#define P3_FIRE2 0x4181
#define P3_FIRE3 0x4182
#define P3_FIRE4 0x4183
#define P3_FIRE5 0x4184
#define P3_FIRE6 0x4185

#define P4_COIN 0x09
#define P4_START 0x05
#define P4_LEFT 0x4200
#define P4_RIGHT 0x4201
#define P4_UP 0x4202
#define P4_DOWN 0x4203
#define P4_FIRE1 0x4280
#define P4_FIRE2 0x4281
#define P4_FIRE3 0x4282
#define P4_FIRE4 0x4283
#define P4_FIRE5 0x4284
#define P4_FIRE6 0x4285

static unsigned char keybinds[0x5000][2]; 
#define _BIND(x) RETRO_DEVICE_ID_JOYPAD_##x
#define RESET_BIND 12
#define SERVICE_BIND 13
static bool init_input()
{
   GameInpInit();
   GameInpDefault();

   bool has_analog = false;
   struct GameInp* pgi = GameInp;
   for (unsigned i = 0; i < nGameInpCount; i++, pgi++)
   {
      if (pgi->nType == BIT_ANALOG_REL)
      {
         has_analog = true;
         break;
      }
   }

   //needed for Neo Geo button mappings (and other drivers in future)
   const char * boardrom = BurnDrvGetTextA(DRV_BOARDROM);

   // Bind to nothing.
   for (unsigned i = 0; i < 0x5000; i++)
      keybinds[i][0] = 0xff;

   // Reset
   keybinds[FBK_F3		][0] = RESET_BIND;
   keybinds[FBK_F3		][1] = 0;
   keybinds[P1_SERVICE	][0] = SERVICE_BIND;
   keybinds[P1_SERVICE	][1] = 0;

   keybinds[P1_COIN	][0] = _BIND(SELECT);
   keybinds[P1_COIN	][1] = 0;
   keybinds[P1_START	][0] = _BIND(START);
   keybinds[P1_START	][1] = 0;
   keybinds[P1_UP	][0] = _BIND(UP);
   keybinds[P1_UP	][1] = 0;
   keybinds[P1_DOWN	][0] = _BIND(DOWN);
   keybinds[P1_DOWN	][1] = 0;
   keybinds[P1_LEFT	][0] = _BIND(LEFT);
   keybinds[P1_LEFT	][1] = 0;
   keybinds[P1_RIGHT	][0] = _BIND(RIGHT);
   keybinds[P1_RIGHT	][1] = 0;
   keybinds[P1_FIRE1	][0] = _BIND(Y);
   keybinds[P1_FIRE1	][1] = 0;
   keybinds[P1_FIRE2	][0] = _BIND(X);
   keybinds[P1_FIRE2	][1] = 0;
   keybinds[P1_FIRE3	][0] = _BIND(L);
   keybinds[P1_FIRE3	][1] = 0;
   keybinds[P1_FIRE4	][0] = _BIND(B);
   keybinds[P1_FIRE4	][1] = 0;
   keybinds[P1_FIRE5	][0] = _BIND(A);
   keybinds[P1_FIRE5	][1] = 0;

   if(boardrom && (strcmp(boardrom,"neogeo") == 0))
   {
      keybinds[P1_FIRE6][0] = _BIND(Y);
      keybinds[P1_FIRE6][1] = 0;
      keybinds[P1_FIRED][0] = _BIND(X);
      keybinds[P1_FIRED][1] = 0;
   }
   else
   {
      keybinds[P1_FIRE6	][0] = _BIND(R);
      keybinds[P1_FIRE6	][1] = 0;
   }

   keybinds[P2_COIN	][0] = _BIND(SELECT);
   keybinds[P2_COIN	][1] = 1;
   keybinds[P2_START	][0] = _BIND(START);
   keybinds[P2_START	][1] = 1;
   keybinds[P2_UP	][0] = _BIND(UP);
   keybinds[P2_UP	][1] = 1;
   keybinds[P2_DOWN	][0] = _BIND(DOWN);
   keybinds[P2_DOWN	][1] = 1;
   keybinds[P2_LEFT	][0] = _BIND(LEFT);
   keybinds[P2_LEFT	][1] = 1;
   keybinds[P2_RIGHT	][0] = _BIND(RIGHT);
   keybinds[P2_RIGHT	][1] = 1;
   keybinds[P2_FIRE1	][0] = _BIND(Y);

   if (boardrom && (strcmp(boardrom, "neogeo") == 0))
   {
      keybinds[P2_FIRE3][0] = _BIND(Y);
      keybinds[P2_FIRE3][1] = 1;
      keybinds[P2_FIRE4][0] = _BIND(X);
      keybinds[P2_FIRE4][1] = 1;
      keybinds[P2_FIRE1][0] = _BIND(B);
      keybinds[P2_FIRE1][1] = 1;
      keybinds[P2_FIRE2][0] = _BIND(A);
      keybinds[P2_FIRE2][1] = 1;
   }
   else
   {
      keybinds[P2_FIRE1	][1] = 1;
      keybinds[P2_FIRE2	][0] = _BIND(X);
      keybinds[P2_FIRE2	][1] = 1;
      keybinds[P2_FIRE3	][0] = _BIND(L);
      keybinds[P2_FIRE3	][1] = 1;
      keybinds[P2_FIRE4	][0] = _BIND(B);
      keybinds[P2_FIRE4	][1] = 1;
      keybinds[P2_FIRE5	][0] = _BIND(A);
      keybinds[P2_FIRE5	][1] = 1;
      keybinds[P2_FIRE6	][0] = _BIND(R);
      keybinds[P2_FIRE6	][1] = 1;
   }

#if 0
   keybinds[0x4088		][0] = L2;
   keybinds[0x4088		][1] = 1;
   keybinds[0x408A		][0] = R2;
   keybinds[0x408A		][1] = 1;
   keybinds[0x408b		][0] = L3;
   keybinds[0x408b		][1] = 1;
   keybinds[0x408c		][0] = R3;
   keybinds[0x408c		][1] = 1;
#endif

   keybinds[P3_COIN	][0] = _BIND(SELECT);
   keybinds[P3_COIN	][1] = 2;
   keybinds[P3_START	][0] = _BIND(START);
   keybinds[P3_START	][1] = 2;
   keybinds[P3_UP	][0] = _BIND(UP);
   keybinds[P3_UP	][1] = 2;
   keybinds[P3_DOWN	][0] = _BIND(DOWN);
   keybinds[P3_DOWN	][1] = 2;
   keybinds[P3_LEFT	][0] = _BIND(LEFT);
   keybinds[P3_LEFT	][1] = 2;
   keybinds[P3_RIGHT	][0] = _BIND(RIGHT);
   keybinds[P3_RIGHT	][1] = 2;
   keybinds[P3_FIRE1	][0] = _BIND(Y);
   keybinds[P3_FIRE1	][1] = 2;
   keybinds[P3_FIRE2	][0] = _BIND(X);
   keybinds[P3_FIRE2	][1] = 2;
   keybinds[P3_FIRE3	][0] = _BIND(L);
   keybinds[P3_FIRE3	][1] = 2;
   keybinds[P3_FIRE4	][0] = _BIND(B);
   keybinds[P3_FIRE4	][1] = 2;
   keybinds[P3_FIRE5	][0] = _BIND(A);
   keybinds[P3_FIRE5	][1] = 2;
   keybinds[P3_FIRE6	][0] = _BIND(R);
   keybinds[P3_FIRE6	][1] = 2;
#if 0
   keybinds[0x4188		][0] = L2;
   keybinds[0x4188		][1] = 2;
   keybinds[0x418A		][0] = R2;
   keybinds[0x418A		][1] = 2;
   keybinds[0x418b		][0] = L3;
   keybinds[0x418b		][1] = 2;
   keybinds[0x418c		][0] = R3;
   keybinds[0x418c		][1] = 2;
#endif

   keybinds[P4_COIN	][0] = _BIND(SELECT);
   keybinds[P4_COIN	][1] = 3;
   keybinds[P4_START	][0] = _BIND(START);
   keybinds[P4_START	][1] = 3;
   keybinds[P4_UP	][0] = _BIND(UP);
   keybinds[P4_UP	][1] = 3;
   keybinds[P4_DOWN	][0] = _BIND(DOWN);
   keybinds[P4_DOWN	][1] = 3;
   keybinds[P4_LEFT	][0] = _BIND(LEFT);
   keybinds[P4_LEFT	][1] = 3;
   keybinds[P4_RIGHT	][0] = _BIND(RIGHT);
   keybinds[P4_RIGHT	][1] = 3;
   keybinds[P4_FIRE1	][0] = _BIND(Y);
   keybinds[P4_FIRE1	][1] = 3;
   keybinds[P4_FIRE2	][0] = _BIND(X);
   keybinds[P4_FIRE2	][1] = 3;
   keybinds[P4_FIRE3	][0] = _BIND(L);
   keybinds[P4_FIRE3	][1] = 3;
   keybinds[P4_FIRE4	][0] = _BIND(B);
   keybinds[P4_FIRE4	][1] = 3;
   keybinds[P4_FIRE5	][0] = _BIND(A);
   keybinds[P4_FIRE5	][1] = 3;
   keybinds[P4_FIRE6	][0] = _BIND(R);
   keybinds[P4_FIRE6	][1] = 3;
#if 0
   keybinds[0x4288		][0] = L2;
   keybinds[0x4288		][1] = 3;
   keybinds[0x428A		][0] = R2;
   keybinds[0x428A		][1] = 3;
   keybinds[0x428b		][0] = L3;
   keybinds[0x428b		][1] = 3;
   keybinds[0x428c		][0] = R3;
   keybinds[0x428c		][1] = 3;
#endif

   return has_analog;
}

static void poll_input()
{
   poll_cb();

   struct GameInp* pgi = GameInp;
   unsigned controller_binds_count = nGameInpCount;

   for (int i = 0; i < controller_binds_count; i++, pgi++)
   {
      int nAdd = 0;
      if ((pgi->nInput & GIT_GROUP_SLIDER) == 0)                           // not a slider
         continue;

      if (pgi->nInput == GIT_KEYSLIDER)
      {
         // Get states of the two keys
         if (input_cb(0, RETRO_DEVICE_JOYPAD, 0,
                  keybinds[pgi->Input.Slider.SliderAxis.nSlider[0]][0]))
            nAdd -= 0x100;

         if (input_cb(0, RETRO_DEVICE_JOYPAD, 0,
                  keybinds[pgi->Input.Slider.SliderAxis.nSlider[1]][0]))
            nAdd += 0x100;
      }

      // nAdd is now -0x100 to +0x100

      // Change to slider speed
      nAdd *= pgi->Input.Slider.nSliderSpeed;
      nAdd /= 0x100;

      if (pgi->Input.Slider.nSliderCenter)
      {                                          // Attact to center
         int v = pgi->Input.Slider.nSliderValue - 0x8000;
         v *= (pgi->Input.Slider.nSliderCenter - 1);
         v /= pgi->Input.Slider.nSliderCenter;
         v += 0x8000;
         pgi->Input.Slider.nSliderValue = v;
      }

      pgi->Input.Slider.nSliderValue += nAdd;
      // Limit slider
      if (pgi->Input.Slider.nSliderValue < 0x0100)
         pgi->Input.Slider.nSliderValue = 0x0100;
      if (pgi->Input.Slider.nSliderValue > 0xFF00)
         pgi->Input.Slider.nSliderValue = 0xFF00;
   }

   pgi = GameInp;

   for (unsigned i = 0; i < controller_binds_count; i++, pgi++)
   {
      switch (pgi->nInput)
      {
         case GIT_CONSTANT: // Constant value
            pgi->Input.nVal = pgi->Input.Constant.nConst;
            *(pgi->Input.pVal) = pgi->Input.nVal;
            break;
         case GIT_SWITCH:
         {
            // Digital input
            unsigned id = keybinds[pgi->Input.Switch.nCode][0];
            unsigned port = keybinds[pgi->Input.Switch.nCode][1];

            bool state;
            if (id == RESET_BIND)
            {
               state = g_reset;
               g_reset = false;
	       Reinitialise();
            }
            else if (id == SERVICE_BIND)
            {
               state =
                  input_cb(0, RETRO_DEVICE_JOYPAD, 0, _BIND(START)) &&
                  input_cb(0, RETRO_DEVICE_JOYPAD, 0, _BIND(SELECT)) &&
                  input_cb(0, RETRO_DEVICE_JOYPAD, 0, _BIND(L)) &&
                  input_cb(0, RETRO_DEVICE_JOYPAD, 0, _BIND(R));
               Reinitialise();
            }
            else
               state = input_cb(port, RETRO_DEVICE_JOYPAD, 0, id);

            if (pgi->nType & BIT_GROUP_ANALOG)
            {
               // Set analog controls to full
               if (state)
                  pgi->Input.nVal = 0xFFFF;
               else
                  pgi->Input.nVal = 0x0001;
#ifdef LSB_FIRST
               *(pgi->Input.pShortVal) = pgi->Input.nVal;
#else
               *((int *)pgi->Input.pShortVal) = pgi->Input.nVal;
#endif
            }
            else
            {
               // Binary controls
               if (state)
                  pgi->Input.nVal = 1;
               else
                  pgi->Input.nVal = 0;
               *(pgi->Input.pVal) = pgi->Input.nVal;
            }
            break;
         }
         case GIT_KEYSLIDER:						// Keyboard slider
         {
            int nSlider = pgi->Input.Slider.nSliderValue;
            if (pgi->nType == BIT_ANALOG_REL) {
               nSlider -= 0x8000;
               nSlider >>= 4;
            }

            pgi->Input.nVal = (unsigned short)nSlider;
#ifdef LSB_FIRST
            *(pgi->Input.pShortVal) = pgi->Input.nVal;
#else
            *((int *)pgi->Input.pShortVal) = pgi->Input.nVal;
#endif
            break;
         }
      }
   }
}

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

