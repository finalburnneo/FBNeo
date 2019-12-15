/*----------------
 * Stuff to finish:
 *
 * It wouldn't be a stretch of the imagination to think the whole of the sdl 'port' needs a redo but here are the main things wrong with this version:
 *
 *
 * The audio output code code could maybe do with some work, but it's getting there...
 * There are lots of problems with the opengl renderer, but you should just use the SDL2 build anyway
 *
 * TODO for SDL2:
 * Make OSD a bit better as it seems a bit hack doing it how FFWD is done...
 * Add autostart to menu as an ini config option
 * Rom scanning to filter out missing games (as an option)
 * Add previews, etc to menu
 * Add menu for options e.g. dips, mapping, saves, IPS patches
 * Maybe a better font output setup with some sort of scaling, maybe add sdl1 support?
 * Bind quick save and load to key
 * Add joypad support to menu
 * ------------------*/

#include "burner.h"

INT32 Init_Joysticks(int p1_use_joystick);

int  nAppVirtualFps = 6000;         // App fps * 100
bool bRunPause      = 0;
bool bAppFullscreen = 0;
bool bAlwaysProcessKeyboardInput = 0;
int  usemenu = 0, usejoy = 0, vsync = 0, dat = 0;

TCHAR szAppBurnVer[16];
char videofiltering[3];

#ifdef BUILD_SDL2
static char* szSDLeepromPath = NULL;
static char* szSDLhiscorePath = NULL;
#endif


int parseSwitches(int argc, char *argv[])
{
   for (int i = 1; i < argc; i++)
   {
      if (*argv[i] != '-')
      {
         continue;
      }

      if (strcmp(argv[i] + 1, "joy") == 0)
      {
         usejoy = 1;
      }

      if (strcmp(argv[i] + 1, "menu") == 0)
      {
         usemenu = 1;
      }

      if (strcmp(argv[i] + 1, "vsync") == 0)
      {
         vsync = 1; // might need to change this to bit flags if there is more than one gfx option
      }

      if (strcmp(argv[i] + 1, "dat") == 0)
      {
         dat = 1;
      }

      if (strcmp(argv[i] + 1, "fullscreen") == 0)
      {
         bAppFullscreen = 1;
      }

      if (strcmp(argv[i] + 1, "nearest") == 0)
      {
         snprintf(videofiltering, 3, "0");
      }

      if (strcmp(argv[i] + 1, "linear") == 0)
      {
        snprintf(videofiltering, 3, "1");
      }

      if (strcmp(argv[i] + 1, "best") == 0)
      {
        snprintf(videofiltering, 3, "2");
      }


   }
   return 0;
}

void DoGame(int gameToRun)
{
   if (!DrvInit(gameToRun, 0))
   {
      MediaInit();
      Init_Joysticks(usejoy);
      RunMessageLoop();
   }
   else
   {
      printf("There was an error loading your selected game.\n");
   }

   ConfigAppSave();
   DrvExit();
   MediaExit();
}

int main(int argc, char *argv[])
{
   const char *romname = NULL;
   UINT32      i       = 0;

   // Make version string
   if (nBurnVer & 0xFF)
   {
      // private version (alpha)
      _stprintf(szAppBurnVer, _T("%x.%x.%x.%02x"), nBurnVer >> 20, (nBurnVer >> 16) & 0x0F, (nBurnVer >> 8) & 0xFF, nBurnVer & 0xFF);
   }
   else
   {
      // public version
      _stprintf(szAppBurnVer, _T("%x.%x.%x"), nBurnVer >> 20, (nBurnVer >> 16) & 0x0F, (nBurnVer >> 8) & 0xFF);
   }

   // set default videofiltering
   snprintf(videofiltering, 3, "0");

   printf("FBNeo v%s\n", szAppBurnVer);
   for (int i = 1; i < argc; i++)
   {
      if (*argv[i] != '-')
      {
         romname = argv[i];
      }
   }

   parseSwitches(argc, argv);

   if (romname == NULL)
   {
      printf("Usage: %s [-joy] [-menu] [-vsync] [-fullscreen] [-dat] [-nearest] [-linear] [-best] <romname>\n", argv[0]);
      printf("Note the -menu switch does not require a romname\n");
      printf("e.g.: %s mslug\n", argv[0]);
      printf("e.g.: %s -menu -joy\n", argv[0]);
      if (!usemenu && !dat)
      {
         return 0;
      }
   }

   // Do these bits before override via ConfigAppLoad
   bCheatsAllowed   = false;
   nAudDSPModule[0] = 1;
   EnableHiscores   = 1;

   #ifdef BUILD_SDL2
   szSDLhiscorePath = SDL_GetPrefPath("fbneo", "hiscore");
   szSDLeepromPath = SDL_GetPrefPath("fbneo", "eeprom");

   _stprintf(szAppHiscorePath, _T("%s"), szSDLhiscorePath);
   _stprintf(szAppEEPROMPath, _T("%s"), szSDLeepromPath);
   #endif

   ConfigAppLoad();
   ComputeGammaLUT();
   BurnLibInit();
   #ifdef BUILD_SDL
   SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

   SDL_WM_SetCaption("FinalBurn Neo", "FinalBurn Neo");
   SDL_ShowCursor(SDL_DISABLE);
   #endif

   #ifdef BUILD_SDL2
   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK | SDL_INIT_AUDIO) < 0)
   {
      printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
   }
   #ifdef _WIN32
   SDL_setenv("SDL_AUDIODRIVER", "directsound", true);        // fix audio for windows
   #endif
   #endif

   if (usemenu)
   {
      #ifdef BUILD_SDL2
      bool quit = 0;
      while (!quit)
      {
         gui_init();
         int selectedOk = gui_process();
         gui_exit();

         switch (selectedOk)
         {
         case -1:
            BurnLibExit();
            SDL_Quit();
            quit = 1;
            return 0;

         default:
            DoGame(selectedOk);
            break;
         }
      }
      #endif
   }
   else if (dat)
   {
      printf("Creating fbneo.dat\n");
      create_datfile(_T("fbneo.dat"), -1);
   }
   else
   {
      for (i = 0; i < nBurnDrvCount; i++)
      {
         nBurnDrvActive = i;
         if (strcmp(BurnDrvGetTextA(DRV_NAME), romname) == 0)
         {
            break;
         }
      }

      if (i == nBurnDrvCount)
      {
         printf("%s is not supported by FinalBurn Neo.\n", romname);
         return 1;
      }

      DoGame(i);
   }
   BurnLibExit();

   SDL_Quit();

   return 0;
}

/* const */ TCHAR *ANSIToTCHAR(const char *pszInString, TCHAR *pszOutString, int nOutSize)
{
#if defined (UNICODE)
   static TCHAR szStringBuffer[1024];

   TCHAR *pszBuffer   = pszOutString ? pszOutString : szStringBuffer;
   int    nBufferSize = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

   if (MultiByteToWideChar(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize))
   {
      return pszBuffer;
   }

   return NULL;
#else
   if (pszOutString)
   {
      _tcscpy(pszOutString, pszInString);
      return pszOutString;
   }

   return (TCHAR *)pszInString;
#endif
}

/* const */ char *TCHARToANSI(const TCHAR *pszInString, char *pszOutString, int nOutSize)
{
#if defined (UNICODE)
   static char szStringBuffer[1024];
   memset(szStringBuffer, 0, sizeof(szStringBuffer));

   char *pszBuffer   = pszOutString ? pszOutString : szStringBuffer;
   int   nBufferSize = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

   if (WideCharToMultiByte(CP_ACP, 0, pszInString, -1, pszBuffer, nBufferSize, NULL, NULL))
   {
      return pszBuffer;
   }

   return NULL;
#else
   if (pszOutString)
   {
      strcpy(pszOutString, pszInString);
      return pszOutString;
   }

   return (char *)pszInString;
#endif
}

bool AppProcessKeyboardInput()
{
   return true;
}
