#include "burner.h"

/* todo implement barcade joystick(arcade panel support)  and ipac keyboad device mapping
*/


//static int GameInpConfig(int nPlayer, int nPcDev, int nAnalog);
static void GameInpConfigOne(int nPlayer, int nPcDev, int nAnalog, struct GameInp* pgi, char* szi);
//INT32 display_set_controls();
INT32 MapJoystick(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nDevice);
extern int buttons [4][8];

#define KEY(x) { pgi->nInput = GIT_SWITCH; pgi->Input.Switch.nCode = (UINT16)(x); }


static bool usesStreetFighterLayout()
{
   int fireButtons = 0;
   bool Volume = false;
   struct BurnInputInfo bii;

   for (UINT32 i = 0; i < 0x1000; i++) {
      bii.szName = NULL;
      if (BurnDrvGetInputInfo(&bii, i)) {
         break;
      }

      BurnDrvGetInputInfo(&bii, i);
      if (bii.szName == NULL) {
         bii.szName = "";
      }

      bool bPlayerInInfo = (toupper(bii.szInfo[0]) == 'P' && bii.szInfo[1] >= '1' && bii.szInfo[1] <= '4'); // Because some of the older drivers don't use the standard input naming.
      bool bPlayerInName = (bii.szName[0] == 'P' && bii.szName[1] >= '1' && bii.szName[1] <= '4');

      if (bPlayerInInfo || bPlayerInName) {
         INT32 nPlayer = 0;

         if (bPlayerInName) {
            nPlayer = bii.szName[1] - '1';
         }
         if (bPlayerInInfo && nPlayer == 0) {
            nPlayer = bii.szInfo[1] - '1';
         }
         if (nPlayer == 0) {
            if (strncmp(" fire", bii.szInfo + 2, 5) == 0) {
               fireButtons++;
            }
         }
         if ((strncmp("Volume", bii.szName, 6) == 0) && (strncmp(" fire", bii.szInfo + 2, 5) == 0)) {
            Volume = true;
         }
      }
   }

   if (nFireButtons >= 5 && (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_CAPCOM_CPS2 && !Volume) {
      bStreetFighterLayout = true;
      printf("Using SF2 Layout\n");
   }
   return  bStreetFighterLayout;
}

static int GameInpConfig(int nPlayer, int nPcDev, int nAnalog) {
   struct GameInp* pgi = NULL;
   unsigned int i;
   for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
      struct BurnInputInfo bii;
      // Get the extra info about the input
      bii.szInfo = NULL;
      BurnDrvGetInputInfo(&bii, i);
      if (bii.pVal == NULL) {
         continue;
      }
      if (bii.szInfo == NULL) {
         bii.szInfo = "";
      }
      GameInpConfigOne(nPlayer, nPcDev, nAnalog, pgi, bii.szInfo);
   }
   for (i = 0; i < nMacroCount; i++, pgi++) {
      GameInpConfigOne(nPlayer, nPcDev, nAnalog, pgi, pgi->Macro.szName);
   }
   GameInpCheckLeftAlt();
   return 0;
}

static void GameInpConfigOne(int nPlayer, int nPcDev, int nAnalog, struct GameInp* pgi, char* szi) {
   switch (nPcDev) {
   case 0:
      GamcPlayer(pgi, szi, nPlayer, -1); // Keyboard
      GamcAnalogKey(pgi, szi, nPlayer, nAnalog);
      GamcMisc(pgi, szi, nPlayer);
      MapJoystick(pgi, szi, nPlayer, nPcDev);
      break;
   case 1:
      GamcPlayer(pgi, szi, nPlayer, 0); // Joystick 1
      GamcAnalogJoy(pgi, szi, nPlayer, 0, nAnalog);
      GamcMisc(pgi, szi, nPlayer);
      MapJoystick(pgi, szi, nPlayer, nPcDev);
      break;
   case 2:
      GamcPlayer(pgi, szi, nPlayer, 1); // Joystick 2
      GamcAnalogJoy(pgi, szi, nPlayer, 1, nAnalog);
      GamcMisc(pgi, szi, nPlayer);
      MapJoystick(pgi, szi, nPlayer, nPcDev);

      break;
   case 3:
      GamcPlayer(pgi, szi, nPlayer, 2); // Joystick 3
      GamcAnalogJoy(pgi, szi, nPlayer, 2, nAnalog);
      GamcMisc(pgi, szi, nPlayer);
      break;
   case 4:
      GamcPlayerHotRod(pgi, szi, nPlayer, 0x10, nAnalog); // X-Arcade left side
      GamcMisc(pgi, szi, -1);
      break;
   case 5:
      GamcPlayerHotRod(pgi, szi, nPlayer, 0x11, nAnalog); // X-Arcade right side
      GamcMisc(pgi, szi, -1);
      break;
   case 6:
      GamcPlayerHotRod(pgi, szi, nPlayer, 0x00, nAnalog); // HotRod left side
      GamcMisc(pgi, szi, -1);
      break;
   case 7:
      GamcPlayerHotRod(pgi, szi, nPlayer, 0x01, nAnalog); // HotRod right size
      GamcMisc(pgi, szi, -1);
      break;
   }
}

INT32 MapJoystick(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nDevice)
{
   INT32 nJoyBase = 0;
   if (nDevice == 0) return 0; // using keyboard for this player
   nDevice--;
   nJoyBase = 0x4000;
   nJoyBase |= nDevice << 8;
   bStreetFighterLayout = usesStreetFighterLayout();
   switch (nPlayer)
   {
   case 0:
      if (strcmp(szi, "p1 coin" ) == 0 )
      {
         if ( buttons[nDevice][6] != -1) KEY(nJoyBase + 0x80 +  buttons[nDevice][6]);
         return 0;
      }
      if (strcmp(szi, "p1 start") == 0 )
      {
         if ( buttons[nDevice][7] != -1) KEY(nJoyBase + 0x80 + buttons[nDevice][7]);
         return 0;
      }

      if (strncmp(szi, "p1 fire ", 7) == 0)
      {
         char* szb = szi + 7;
         INT32 nButton = strtol(szb, NULL, 0);
         if (nButton >= 1)
         {
            nButton--;
         }
         if (buttons[nDevice][nButton] != -1 && !bStreetFighterLayout) { KEY(nJoyBase + 0x80 + buttons[nDevice][nButton]); }
         else if (buttons[nDevice][nButton] != -1 && bStreetFighterLayout)
         {
            if (nButton == 0) KEY(nJoyBase + 0x80 + buttons[nDevice][2]);
            if (nButton == 1) KEY(nJoyBase + 0x80 + buttons[nDevice][3]);
            if (nButton == 2) KEY(nJoyBase + 0x80 + buttons[nDevice][4]);
            if (nButton == 3) KEY(nJoyBase + 0x80 + buttons[nDevice][0]);
            if (nButton == 4) KEY(nJoyBase + 0x80 + buttons[nDevice][1]);
            if (nButton == 5) KEY(nJoyBase + 0x80 + buttons[nDevice][5]);
         }
         else KEY(nJoyBase + 0x80 + nButton); // joystick not mapped or isint plugged  use fba default settings
      }
      break;
   case 1:
      if (strcmp(szi, "p2 coin" ) == 0 )
      {
         if ( buttons[nDevice][6] != -1) KEY(nJoyBase + 0x80 +  buttons[nDevice][6]);
         return 0;
      }
      if (strcmp(szi, "p2 start") == 0 )
      {
         if ( buttons[nDevice][7] != -1) KEY(nJoyBase + 0x80 + buttons[nDevice][7]);
         return 0;
      }

      if (strncmp(szi, "p2 fire ", 7) == 0)
      {
         char* szb = szi + 7;
         INT32 nButton = strtol(szb, NULL, 0);
         if (nButton >= 1)
         {
            nButton--;
         }
         if (buttons[nDevice][nButton] != -1 && !bStreetFighterLayout) { KEY(nJoyBase + 0x80 + buttons[nDevice][nButton]); }
         else if (buttons[nDevice][nButton] != -1 && bStreetFighterLayout)
         {
            if (nButton == 0) KEY(nJoyBase + 0x80 + buttons[nDevice][2]);
            if (nButton == 1) KEY(nJoyBase + 0x80 + buttons[nDevice][3]);
            if (nButton == 2) KEY(nJoyBase + 0x80 + buttons[nDevice][4]);
            if (nButton == 3) KEY(nJoyBase + 0x80 + buttons[nDevice][0]);
            if (nButton == 4) KEY(nJoyBase + 0x80 + buttons[nDevice][1]);
            if (nButton == 5) KEY(nJoyBase + 0x80 + buttons[nDevice][5]);
         }
         else KEY(nJoyBase + 0x80 + nButton); // joystick not mapped or isint plugged  use fba default settings
      }
      break;
   case 2:
      if (strcmp(szi, "p3 coin" ) == 0 )
      {
         if ( buttons[nDevice][6] != -1) KEY(nJoyBase + 0x80 +  buttons[nDevice][6]);
         return 0;
      }
      if (strcmp(szi, "p3 start") == 0 )
      {
         if ( buttons[nDevice][7] != -1) KEY(nJoyBase + 0x80 + buttons[nDevice][7]);
         return 0;
      }

      if (strncmp(szi, "p3 fire ", 7) == 0)
      {
         char* szb = szi + 7;
         INT32 nButton = strtol(szb, NULL, 0);
         if (nButton >= 1)
         {
            nButton--;
         }
         if (buttons[nDevice][nButton] != -1 && !bStreetFighterLayout) { KEY(nJoyBase + 0x80 + buttons[nDevice][nButton]); }
         else if (buttons[nDevice][nButton] != -1 && bStreetFighterLayout)
         {
            if (nButton == 0) KEY(nJoyBase + 0x80 + buttons[nDevice][2]);
            if (nButton == 1) KEY(nJoyBase + 0x80 + buttons[nDevice][3]);
            if (nButton == 2) KEY(nJoyBase + 0x80 + buttons[nDevice][4]);
            if (nButton == 3) KEY(nJoyBase + 0x80 + buttons[nDevice][0]);
            if (nButton == 4) KEY(nJoyBase + 0x80 + buttons[nDevice][1]);
            if (nButton == 5) KEY(nJoyBase + 0x80 + buttons[nDevice][5]);
         }
         else KEY(nJoyBase + 0x80 + nButton); // joystick not mapped or isint plugged  use fba default settings
      }
      break;
   case 3:
      if (strcmp(szi, "p4 coin" ) == 0 )
      {
         if ( buttons[nDevice][6] != -1) KEY(nJoyBase + 0x80 +  buttons[nDevice][6]);
         return 0;
      }
      if (strcmp(szi, "p4 start") == 0 )
      {
         if ( buttons[nDevice][7] != -1) KEY(nJoyBase + 0x80 + buttons[nDevice][7]);
         return 0;
      }

      if (strncmp(szi, "p4 fire ", 7) == 0)
      {
         char* szb = szi + 7;
         INT32 nButton = strtol(szb, NULL, 0);
         if (nButton >= 1)
         {
            nButton--;
         }
         if (buttons[nDevice][nButton] != -1 && !bStreetFighterLayout) { KEY(nJoyBase + 0x80 + buttons[nDevice][nButton]); }
         else if (buttons[nDevice][nButton] != -1 && bStreetFighterLayout)
         {
            if (nButton == 0) KEY(nJoyBase + 0x80 + buttons[nDevice][2]);
            if (nButton == 1) KEY(nJoyBase + 0x80 + buttons[nDevice][3]);
            if (nButton == 2) KEY(nJoyBase + 0x80 + buttons[nDevice][4]);
            if (nButton == 3) KEY(nJoyBase + 0x80 + buttons[nDevice][0]);
            if (nButton == 4) KEY(nJoyBase + 0x80 + buttons[nDevice][1]);
            if (nButton == 5) KEY(nJoyBase + 0x80 + buttons[nDevice][5]);
         }
         else KEY(nJoyBase + 0x80 + nButton); // joystick not mapped or isint plugged  use fba default settings
      }
      break;
   }
   return 0;
}


INT32 display_set_controls()
{
   struct GameInp* pgi = NULL;
   unsigned int i;
   for (i = 0, pgi = GameInp; i < nGameInpCount; i++, pgi++) {
      struct BurnInputInfo bii;
      // Get the extra info about the input
      bii.szInfo = NULL;
      BurnDrvGetInputInfo(&bii, i);
      if (bii.pVal == NULL)
      {
         continue;
      }
      if (bii.szInfo == NULL) {
         bii.szInfo = "";
      }
      printf("%s %s\n", bii.szInfo, InputCodeDesc(pgi->Input.Switch.nCode));
   }
   return 0;
}


INT32 Init_Joysticks(int p_one_use_joystick)
{
   if (!p_one_use_joystick)
   {
      /*keyboard p1, joy0 p2) */
      GameInpConfig(0, 0, 1);
      GameInpConfig(1, 1, 1);
   }
   else
   {
      /*init all joysticks (4 max atm) to map or the default will think 
      p1 is keyboard and p2 p3 p4 is joy 0 1 2 instead of joy 1 2 3 */
      
      for (int i = 0; i < 4; ++i)
      {
         GameInpConfig(i, i+1, 1);
      }
   }
   display_set_controls();
   return 0;
};