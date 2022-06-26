// Burner DipSwitches Dialog module
#include "burner.h"

// static unsigned char nPrevDIPSettings[4];

static int nDIPOffset;

static bool bOK;

struct GroupOfDIPSwitches GroupDIPSwitchesArray[MAXDIPSWITCHES];

static void InpDIPSWGetOffset()
{
	BurnDIPInfo bdi;
	nDIPOffset = 0;
	for(int i = 0; BurnDrvGetDIPInfo(&bdi, i) == 0; i++) {
		if (bdi.nFlags == 0xF0) {
			nDIPOffset = bdi.nInput;
			break;
		}
	}
}

void InpDIPSWResetDIPs()
{
	int i = 0;
	BurnDIPInfo bdi;
	struct GameInp* pgi;

	InpDIPSWGetOffset();

	while (BurnDrvGetDIPInfo(&bdi, i) == 0) {
		if (bdi.nFlags == 0xFF) {
			pgi = GameInp + bdi.nInput + nDIPOffset;
			pgi->Input.Constant.nConst = (pgi->Input.Constant.nConst & ~bdi.nMask) | (bdi.nSetting & bdi.nMask);
		}
		i++;
	}
}

bool setDIPSwitchOption(int dipgroup, int dipoption)		// Returns true if DIP is set other than default value
{
	struct GameInp *pgi = GameInp + GroupDIPSwitchesArray[dipgroup].dipSwitchesOptions[dipoption].nInput + nDIPOffset;
	pgi->Input.Constant.nConst = (pgi->Input.Constant.nConst & ~GroupDIPSwitchesArray[dipgroup].dipSwitchesOptions[dipoption].nMask) | (GroupDIPSwitchesArray[dipgroup].dipSwitchesOptions[dipoption].nSetting & GroupDIPSwitchesArray[dipgroup].dipSwitchesOptions[dipoption].nMask);

	// Change check boxes of DIP options
	GroupDIPSwitchesArray[dipgroup].OptionsNamesWithCheckBoxes[GroupDIPSwitchesArray[dipgroup].SelectedDIPOption][1] = ' ';
	GroupDIPSwitchesArray[dipgroup].OptionsNamesWithCheckBoxes[dipoption][1] = 'X';
	GroupDIPSwitchesArray[dipgroup].SelectedDIPOption = dipoption;
	return (dipoption != GroupDIPSwitchesArray[dipgroup].DefaultDIPOption);
}

static int InpDIPSWListBegin()
{
	return 0;
}

static bool CheckSetting(int i)
{
	BurnDIPInfo bdi;

	BurnDrvGetDIPInfo(&bdi, i);
	struct GameInp* pgi = GameInp + bdi.nInput + nDIPOffset;

	if ((pgi->Input.Constant.nConst & bdi.nMask) == bdi.nSetting)
	{
		unsigned char nFlags = bdi.nFlags;
		if ((nFlags & 0x0F) <= 1)
		{
			return true;
		}
		else
		{
			for (int j = 1; j < (nFlags & 0x0F); j++)
			{
				BurnDrvGetDIPInfo(&bdi, i + j);
				pgi = GameInp + bdi.nInput + nDIPOffset;
				if (nFlags & 0x80)
				{
					if ((pgi->Input.Constant.nConst & bdi.nMask) == bdi.nSetting)
					{
						return false;
					}
				}
				else
				{
					if ((pgi->Input.Constant.nConst & bdi.nMask) != bdi.nSetting)
					{
						return false;
					}
				}
			}
			return true;
		}
	}
	return false;
}

// Make a list of DIPswitches
static int InpDIPSWListMake()
{
	BurnDIPInfo tempDIPSwitch;
	int i = 0, j = 0;
	UINT16 dipcount = 0;

	InpDIPSWGetOffset();
	while ((dipcount < MAXDIPSWITCHES) && (!BurnDrvGetDIPInfo(&tempDIPSwitch, i))) {
		/* 0xFE is the beginning label for a DIP switch entry */
		/* 0xFD are region DIP switches */
		if (tempDIPSwitch.nFlags < 0xFF) {
			if (tempDIPSwitch.szText) {					// Ignore DIP switches without name
				if (tempDIPSwitch.nFlags & 0x40) {		// Chek if is a DIP group...
					GroupDIPSwitchesArray[dipcount].dipSwitch = tempDIPSwitch;
					dipcount++;
					j = 0;
				} else if (j < MAXDIPOPTIONS) {			// ... or a DIP option
					GroupDIPSwitchesArray[dipcount - 1].dipSwitchesOptions[j] = tempDIPSwitch;
					// This is needed to get default DIP value
					struct GameInp *pgi = GameInp + tempDIPSwitch.nInput + nDIPOffset;
					// Check if it is the default switch value
					if ((pgi->Input.Constant.nConst & tempDIPSwitch.nMask) == (tempDIPSwitch.nSetting)) {
						GroupDIPSwitchesArray[dipcount - 1].DefaultDIPOption = j;
						GroupDIPSwitchesArray[dipcount - 1].SelectedDIPOption = j;
						snprintf(GroupDIPSwitchesArray[dipcount - 1].OptionsNamesWithCheckBoxes[j], DIP_MAX_NAME, "[X] %s", tempDIPSwitch.szText);
					} else snprintf(GroupDIPSwitchesArray[dipcount - 1].OptionsNamesWithCheckBoxes[j], DIP_MAX_NAME, "[ ] %s", tempDIPSwitch.szText);
					j++;
				}
			}
		}
		i++;
	}
	return dipcount;		// Return number of DIPs
}

static int InpDIPSWInit()
{
	BurnDIPInfo     bdi;
	struct GameInp* pgi;

	InpDIPSWGetOffset();

	InpDIPSWListBegin();
	InpDIPSWListMake();

/*	for (int i = 0, j = 0; BurnDrvGetDIPInfo(&bdi, i) == 0; i++)
	{
		if (bdi.nInput >= 0 && bdi.nFlags == 0xFF)
		{
			pgi = GameInp + bdi.nInput + nDIPOffset;
			nPrevDIPSettings[j] = pgi->Input.Constant.nConst;
			j++;
		}
	}	// This is the same as DefaultDIPOption... ?
*/
	return 0;
}

static int InpDIPSWExit()
{
	GameInpCheckMouse();
	return 0;
}

static void InpDIPSWCancel()
{
	if (!bOK)
	{
/*		int             i = 0, j = 0;
		BurnDIPInfo     bdi;
		struct GameInp* pgi;
		while (BurnDrvGetDIPInfo(&bdi, i) == 0)
		{
			if (bdi.nInput >= 0 && bdi.nFlags == 0xFF)
			{
				pgi = GameInp + bdi.nInput + nDIPOffset;
				pgi->Input.Constant.nConst = nPrevDIPSettings[j];
				j++;
			}
			i++;
		}	// Is this just like InpDIPSWResetDIPs() ???
*/
		InpDIPSWResetDIPs();
	}
}

// Create the list of possible values for a DIPswitch
static void InpDIPSWSelect()
{
}

int InpDIPSWCreate()
{
	if (bDrvOkay == 0) {									// No game is loaded
		return -1;
	}
	return InpDIPSWListMake();
}
