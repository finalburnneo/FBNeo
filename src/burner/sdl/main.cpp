/*----------------
 * Stuff to finish:
 *
 * It wouldn't be a stretch of the imagination to think the whole of the sdl 'port' needs a redo but here are the main things wrong with this version:
 *
 *
 * There is no OSD of any kind which makes it hard to display info to the users.
 * There are lots of problems with the audio output code.
 * There are lots of problems with the opengl renderer
 * probably many other things.
 * ------------------*/
#include "burner.h"

int  nAppVirtualFps = 6000;         // App fps * 100
bool bRunPause      = 0;
bool bAlwaysProcessKeyboardInput = 0;

#undef main

int main(int argc, char *argv[])
{
   UINT32 i = 0;

   ConfigAppLoad();
   BurnLibInit();

   if (argc < 2)
   {
      printf("Usage: %1$s <romname>\n   ie: %1$s uopoko\n Note: no extension.\n\n", argv[0]);
      return 0;
   }

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

   if (argc == 2)
   {
     #ifdef BUILD_SDL2
      if (strcmp(argv[1], "menu") == 0)
      {
         gui_init();
         int selectedOk = gui_process();
         gui_exit();
         if (selectedOk == -1)
         {
            BurnLibExit();
            SDL_Quit();
            return 0;
         }
      }
      else
      {
     #endif


      for (i = 0; i < nBurnDrvCount; i++)
      {
         //nBurnDrvSelect[0] = i;
         nBurnDrvActive = i;
         if (strcmp(BurnDrvGetTextA(DRV_NAME), argv[1]) == 0)
         {
            break;
         }
      }

      if (i == nBurnDrvCount)
      {
         printf("%s is not supported by FinalBurn Neo.\n", argv[1]);
         return 1;
      }


      #ifdef BUILD_SDL2
   }
      #endif
   }



   bCheatsAllowed   = false;
   nAudDSPModule[0] = 1;
   ConfigAppLoad();
   ConfigAppSave();

   MediaInit();
   if (!DrvInit(i, 0))
   {

      RunMessageLoop();
   }
   else
   {
      printf("There was an error loading your selected game.\n");
   }

   DrvExit();
   MediaExit();

   ConfigAppSave();
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
