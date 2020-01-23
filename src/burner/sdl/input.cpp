#include "burner.h"

/* todo implement barcade joystick(arcade panel support)  and ipac keyboad device mapping
*/


//static int GameInpConfig(int nPlayer, int nPcDev, int nAnalog);
static void GameInpConfigOne(int nPlayer, int nPcDev, int nAnalog, struct GameInp* pgi, char* szi);
//INT32 display_set_controls();
INT32 Mapcoins(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nDevice);

#define KEY(x) { pgi->nInput = GIT_SWITCH; pgi->Input.Switch.nCode = (UINT16)(x); }

int usebarcade = 0;

static unsigned int capcom6Layout[] = {
   2, 3, 4, 0, 1, 5,
};


/* keep the snes layout on xbox360 controller
Phscial button layout on the encoder should be
X Y L
2 3 4
0 1 5
A B R
no need to change this for sf2 as use the capcom6Layout when for it when its needed its the same as that with the rows reversed
*/

static unsigned int barcade_Layout[] = {

   0, 1, 5, 2, 3, 4,

};


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
		Mapcoins(pgi, szi, nPlayer, nPcDev);
		break;
	case 1:
		GamcPlayer(pgi, szi, nPlayer, 0); // Joystick 1
		GamcAnalogJoy(pgi, szi, nPlayer, 0, nAnalog);
		GamcMisc(pgi, szi, nPlayer);
		Mapcoins(pgi, szi, nPlayer, nPcDev);
		break;
	case 2:
		GamcPlayer(pgi, szi, nPlayer, 1); // Joystick 2
		GamcAnalogJoy(pgi, szi, nPlayer, 1, nAnalog);
		GamcMisc(pgi, szi, nPlayer);
		Mapcoins(pgi, szi, nPlayer, nPcDev);

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

INT32 Mapcoins(struct GameInp* pgi, char* szi, INT32 nPlayer, INT32 nDevice)
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
		/* dont map coins and start until mapping joysicks is implemented
		if (strcmp(szi, "p1 coin" ) == 0 && nPlayer == 0 )
		{
		   KEY(nJoyBase + 0x80 + 8);
		   return 0;

		}
		if (strcmp(szi, "p1 start") == 0 && nPlayer == 0 )
		{
		   KEY(nJoyBase + 0x80 + 9);
		   return 0;
		}
  */
		if (strncmp(szi, "p1 fire ", 7) == 0)
		{
			char* szb = szi + 7;
			INT32 nButton = strtol(szb, NULL, 0);
			if (nButton >= 1)
			{
				nButton--;
			}
			if (bStreetFighterLayout) KEY(nJoyBase + 0x80 + capcom6Layout[nButton]);
			if (!bStreetFighterLayout && usebarcade)  KEY(nJoyBase + 0x80 + barcade_Layout[nButton])
		}
		break;
	case 1:
		/* dont map coins and start until mapping joysicks is implemented
		if (strcmp(szi, "p2 coin" ) == 0 && nPlayer == 1 )
		{
		   KEY(nJoyBase + 0x80 + 8);
		   return 0;

		}
		if (strcmp(szi, "p2 start") == 0 && nPlayer == 1 )
		{
		   KEY(nJoyBase + 0x80 + 9);
		   return 0;
		}
  */
		if (strncmp(szi, "p2 fire ", 7) == 0 && bStreetFighterLayout)
		{
			char* szb = szi + 7;
			INT32 nButton = strtol(szb, NULL, 0);
			if (nButton >= 1)
			{
				nButton--;
			}
			if (bStreetFighterLayout) KEY(nJoyBase + 0x80 + capcom6Layout[nButton]);
			if (!bStreetFighterLayout && usebarcade)  KEY(nJoyBase + 0x80 + barcade_Layout[nButton])

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
		/* p1 joy0 , p2 joy1 */
		GameInpConfig(0, 1, 1);
		GameInpConfig(1, 2, 1);
	}
	display_set_controls();
	return 0;
};