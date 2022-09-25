#include "burner.h"

/* todo implement barcade joystick(arcade panel support)  and ipac keyboad device mapping
*/


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

INT32 MapJoystick(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nDevice)
{
	if (nDevice == 0) return 0; // using keyboard for this player
	nDevice--;
	INT32 nJoyBase = 0x4000;
	nJoyBase |= nDevice << 8;

	bStreetFighterLayout = usesStreetFighterLayout();
	char szPlay[4][4]={"p1 ", "p2 ", "p3 ", "p4 "};

	if (_strnicmp(szPlay[nPlayer & 3], szi, 3)) {
		 return 1; // Not our player
	} else {
		szi += 3;
		if (strcmp(szi, "up") == 0) {
			KEY(nJoyBase + 0x02);
		} else if (strcmp(szi, "down") == 0) {
			KEY(nJoyBase + 0x03);
		} else if (strcmp(szi, "left") == 0) {
			KEY(nJoyBase + 0x00);
		} else if (strcmp(szi, "right") == 0) {
			KEY(nJoyBase + 0x01);
		} else if (strcmp(szi, "coin") == 0) {
			if (buttons[nDevice][6] > -1) {
				KEY(nJoyBase + 0x80 + buttons[nDevice][6]);
			} else {
				KEY(nJoyBase + 0x86); // joystick not mapped or is not plugged, use fbneo default settings
			}
		} else if (strcmp(szi, "start") == 0) {
			if (buttons[nDevice][7] > -1) {
				KEY(nJoyBase + 0x80 + buttons[nDevice][7]);
			} else {
				KEY(nJoyBase + 0x87); // joystick not mapped or is not plugged, use fbneo default settings
			}
		} else if (strncmp(szi, "fire ", 5) == 0) {
			szi += 5;
			INT32 nButton = strtol(szi, NULL, 0);

			if (nButton > 0) nButton--;
			if (buttons[nDevice][nButton] != -1) {
				if (bStreetFighterLayout) {
					switch (nButton)
					{
						case 0:
							KEY(nJoyBase + 0x80 + buttons[nDevice][2]);
							break;
						case 1:
							KEY(nJoyBase + 0x80 + buttons[nDevice][3]);
							break;
						case 2:
							KEY(nJoyBase + 0x80 + buttons[nDevice][4]);
							break;
						case 3:
							KEY(nJoyBase + 0x80 + buttons[nDevice][0]);
							break;
						case 4:
							KEY(nJoyBase + 0x80 + buttons[nDevice][1]);
							break;
						case 5:
							KEY(nJoyBase + 0x80 + buttons[nDevice][5]);
							break;
					}
				} else KEY(nJoyBase + 0x80 + buttons[nDevice][nButton]);
			} else KEY(nJoyBase + 0x80 + nButton); // joystick not mapped or is not plugged, use fbneo default settings
		} else return 1; // Nothing mapped
	}
	return 0;
}

static void GameInpConfigOne(int nPlayer, int nPcDev, int nAnalog, struct GameInp* pgi, char* szi)
{
	GamcPlayer(pgi, szi, nPlayer, nPcDev - 1);
	// nPcDev == 0 is Keyboard, nPcDev == 1 is joy0, etc.
	if (nPcDev) GamcAnalogJoy(pgi, szi, nPlayer, nPcDev - 1, nAnalog);
	else GamcAnalogKey(pgi, szi, nPlayer, nAnalog);
	GamcMisc(pgi, szi, nPlayer);
	MapJoystick(pgi, szi, nPlayer, nPcDev);
}

static int GameInpConfig(int nPlayer, int nPcDev, int nAnalog)
{
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

/*
INT32 Init_Joysticks(int p_one_use_joystick)
{
	if (p_one_use_joystick) {
		// init all joysticks (4 max atm) to map or the default will think 
		// p1 is keyboard and p2 p3 p4 is joy 0 1 2 instead of joy 1 2 3
		for (int i = 0; i < nMaxPlayers; ++i) {
			GameInpConfig(i, i+1, 1);
		}
	} else {
		// keyboard p1, joy0 p2, joy1 p3, joy2 p4)
		for (int i = 0; i < nMaxPlayers; ++i) {
			GameInpConfig(i, i, 1);
		}
	}
	return 0;
}
*/