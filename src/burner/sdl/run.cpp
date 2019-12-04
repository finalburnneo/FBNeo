// Run module
#include "burner.h"

#include "cfgpath.h"

bool bAltPause = 0;

int bAlwaysDrawFrames = 0;

int counter;                                // General purpose variable used when debugging

static unsigned int nNormalLast = 0;        // Last value of timeGetTime()
static int          nNormalFrac = 0;        // Extra fraction we did

static bool bAppDoStep = 0;
bool bAppDoFast = 0;
bool bAppShowFPS = 0;
static int  nFastSpeed = 6;

static void CreateSavePath(char *szConfig)
{
   char cfgdir[MAX_PATH]= { 0 } ;

   get_user_data_folder(cfgdir, sizeof(cfgdir), "fbneo");
   if (cfgdir[0] == 0)
   {
      printf("Unable to find home directory.\n");
      return;
   }

   memcpy(szConfig, cfgdir, sizeof(cfgdir));
   return;
}

int SaveNVRAM()
{
   char temp[MAX_PATH] { 0 };
   char cfgdir[MAX_PATH] { 0 };


   snprintf(temp, 255, "%s%s.nvr", cfgdir, BurnDrvGetTextA(0));
   CreateSavePath(cfgdir);
   fprintf(stderr, "Writing NVRAM to \"%s%s\"\n", cfgdir, temp);
   BurnStateSave(temp, 0);
   return 0;
}

int ReadNVRAM()
{
   char temp[MAX_PATH] = { 0 };
   char cfgdir[MAX_PATH] = { 0 } ;
    snprintf(temp, 255, "%s%s.nvr", cfgdir, BurnDrvGetTextA(0));
   CreateSavePath(cfgdir);
   fprintf(stderr, "Reading NVRAM from \"%s%s\"\n", cfgdir, temp);
   BurnStateLoad(temp, 0, NULL);
   return 0;
}

static int GetInput(bool bCopy)
{
   InputMake(bCopy);                                // get input
   return 0;
}

char   fpsstring[20];

static void DisplayFPS()
{
   static time_t       fpstimer;
   static unsigned int nPreviousFrames;


   time_t temptime = clock();
   float  fps      = static_cast <float>(nFramesRendered - nPreviousFrames) * CLOCKS_PER_SEC / (temptime - fpstimer);

   sprintf(fpsstring, "%2.1f", fps);

   fpstimer        = temptime;
   nPreviousFrames = nFramesRendered;
}

// define this function somewhere above RunMessageLoop()
void ToggleLayer(unsigned char thisLayer)
{
   nBurnLayer ^= thisLayer;                         // xor with thisLayer
   VidRedraw();
   VidPaint(0);
}

// With or without sound, run one frame.
// If bDraw is true, it's the last frame before we are up to date, and so we should draw the screen
static int RunFrame(int bDraw, int bPause)
{
   static int bPrevPause = 0;
   static int bPrevDraw  = 0;

   if (bPrevDraw && !bPause)
   {
      VidPaint(0);                                              // paint the screen (no need to validate)
   }

   if (!bDrvOkay)
   {
      return 1;
   }

   if (bPause)
   {
      GetInput(false);                                          // Update burner inputs, but not game inputs
      if (bPause != bPrevPause)
      {
         VidPaint(2);                                                   // Redraw the screen (to ensure mode indicators are updated)
      }
   }
   else
   {
      nFramesEmulated++;
      nCurrentFrame++;
      GetInput(true);                                   // Update inputs
   }
   if (bDraw)
   {
      nFramesRendered++;
      if (VidFrame())
      {                                     // Do one frame
         AudBlankSound();

      }
   }
   else
   {                                       // frame skipping
      pBurnDraw = NULL;                    // Make sure no image is drawn
      BurnDrvFrame();
   }

   DisplayFPS();
   bPrevPause = bPause;
   bPrevDraw  = bDraw;

   return 0;
}

// Callback used when DSound needs more sound
static int RunGetNextSound(int bDraw)
{
   if (nAudNextSound == NULL)
   {
      return 1;
   }

   if (bRunPause)
   {
      if (bAppDoStep)
      {
         RunFrame(bDraw, 0);
         memset(nAudNextSound, 0, nAudSegLen << 2);                                        // Write silence into the buffer
      }
      else
      {
         RunFrame(bDraw, 1);
      }

      bAppDoStep = 0;                                                   // done one step
      return 0;
   }

   if (bAppDoFast)
   {                                            // do more frames
      for (int i = 0; i < nFastSpeed; i++)
      {
         RunFrame(0, 0);
      }
   }

   // Render frame with sound
   pBurnSoundOut = nAudNextSound;
   RunFrame(bDraw, 0);
   if (bAppDoStep)
   {
      memset(nAudNextSound, 0, nAudSegLen << 2);                // Write silence into the buffer
   }
   bAppDoStep = 0;                                              // done one step

   return 0;
}

int RunIdle()
{
   int nTime, nCount;

   if (bAudPlaying)
   {
      // Run with sound
      AudSoundCheck();
      return 0;
   }

   // Run without sound
   nTime  = SDL_GetTicks() - nNormalLast;
   nCount = (nTime * nAppVirtualFps - nNormalFrac) / 100000;
   if (nCount <= 0)
   {                                // No need to do anything for a bit
    //  SDL_Delay(3);

      return 0;
   }

   nNormalFrac += nCount * 100000;
   nNormalLast += nNormalFrac / nAppVirtualFps;
   nNormalFrac %= nAppVirtualFps;

   if (bAppDoFast)
   {                                // Temporarily increase virtual fps
      nCount *= nFastSpeed;
   }
   if (nCount > 100)
   {                                // Limit frame skipping
      nCount = 100;
   }
   if (bRunPause)
   {
      if (bAppDoStep)
      {                                     // Step one frame
         nCount = 10;
      }
      else
      {
         RunFrame(1, 1);                                            // Paused
         return 0;
      }
   }
   bAppDoStep = 0;

   for (int i = nCount / 10; i > 0; i--)
   {              // Mid-frames
      RunFrame(!bAlwaysDrawFrames, 0);
   }
   RunFrame(1, 0);                                  // End-frame
   // temp added for SDLFBA
   //VidPaint(0);
   return 0;
}

int RunReset()
{
   // Reset the speed throttling code
   nNormalLast = 0; nNormalFrac = 0;
   if (!bAudPlaying)
   {
      // run without sound
      nNormalLast = SDL_GetTicks();
   }
   return 0;
}

int RunInit()
{
   // Try to run with sound
   AudSetCallback(RunGetNextSound);
   AudSoundPlay();

   RunReset();
   ReadNVRAM();
   return 0;
}

int RunExit()
{
   nNormalLast = 0;
   SaveNVRAM();
   return 0;
}

#ifndef BUILD_MACOS
// The main message loop
int RunMessageLoop()
{
   int quit = 0;

   RunInit();
   GameInpCheckMouse();                                                                     // Hide the cursor

   while (!quit)
   {
      SDL_Event event;
      while (SDL_PollEvent(&event))
      {
         switch (event.type)
         {
         case SDL_QUIT:                                        /* Windows was closed */
            quit = 1;
            break;

         case SDL_KEYDOWN:                                                // need to find a nicer way of doing this...
            switch (event.key.keysym.sym)
            {
            case SDLK_F1:
               bAppDoFast = 1;
               break;
            case SDLK_F11:
               bAppShowFPS = !bAppShowFPS;
               break;
            default:
               break;
            }
            break;

         case SDL_KEYUP:                                                // need to find a nicer way of doing this...
            switch (event.key.keysym.sym)
            {
            case SDLK_F1:
               bAppDoFast = 0;
               break;
            case SDLK_F12:
               quit = 1;
               break;

            default:
               break;
            }
            break;
         }
      }
      RunIdle();
   }

   RunExit();

   return 0;
}

#endif
