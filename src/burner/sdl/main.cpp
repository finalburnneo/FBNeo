/*----------------
* Stuff to finish:
*
* It wouldn't be a stretch of the imagination to think the whole of the sdl 'port' needs a redo but here are the main things wrong with this version:
*
*
* There are lots of problems with the audio output code.
* There are lots of problems with the opengl renderer
* probably many other things.
* ------------------*/

#include "burner.h"

INT32 Init_Joysticks(int p1_use_joystick);

int  nAppVirtualFps = 6000;         // App fps * 100
bool bRunPause      = 0;
bool bAlwaysProcessKeyboardInput = 0;
int usemenu=0, usejoy=0, vsync =0; 

int parseSwitches(int argc, char *argv[])
{
   for (int i = 1; i < argc; i++) {
      if (*argv[i] != '-') {
         continue;
      }

      if (strcmp(argv[i] + 1, "joy") == 0) {
         usejoy=1;
      }

      if (strcmp(argv[i] + 1, "menu") == 0) {
         usemenu=1;
      }

      if (strcmp(argv[i] + 1, "vsync") == 0) {
         vsync=1; // might need to change this to bit flags if there is more than one gfx option
      }
   }
   return 0;
}

int main(int argc, char *argv[])
{
   const char *romname = NULL;
   UINT32 i = 0;


   for (int i = 1; i < argc; i++) {
      if (*argv[i] != '-') {
         romname = argv[i];
      }
   }

   parseSwitches(argc, argv);

   if (romname == NULL) {
      printf("Usage: %s [-joy] [-menu]  [-vsync] <romname>\n", argv[0]);
      printf("Note the -menu switch does not require a romname\n");
      printf("e.g.: %s mslug\n", argv[0]);
      printf("e.g.: %s -menu -joy\n", argv[0]);
      if (!usemenu) return 0;
   }
   
   ConfigAppLoad();

   BurnLibInit();
   #ifdef BUILD_SDL
   SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

   SDL_WM_SetCaption("FinalBurn Neo", "FinalBurn Neo");
   SDL_ShowCursor(SDL_DISABLE);
   #endif

   #ifdef BUILD_SDL2
   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0)
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
      gui_init();
      int selectedOk = gui_process();
      gui_exit();
      if (selectedOk == -1)
      {
         BurnLibExit();
         SDL_Quit();
         return 0;
      }
      #endif
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
         printf("%s is not supported by FinalBurn Neo.\n",  romname);
         return 1;
      }
   }

   bCheatsAllowed   = false;
   nAudDSPModule[0] = 1;
   
   if (!DrvInit(i, 0))
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

   BurnLibExit();

   SDL_Quit();

   return 0;
}

/* const */ TCHAR *ANSIToTCHAR(const char *pszInString, TCHAR *pszOutString, int nOutSize)
{
#if defined (UNICODE)
   static TCHAR szStringBuffer[1024];

   TCHAR *pszBuffer = pszOutString ? pszOutString : szStringBuffer;
   int nBufferSize  = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

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

   char *pszBuffer = pszOutString ? pszOutString : szStringBuffer;
   int nBufferSize = pszOutString ? nOutSize * 2 : sizeof(szStringBuffer);

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
