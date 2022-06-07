#include "burner.h"

const int MAXPLAYER = 4;
static int nPlayerInputs[MAXPLAYER], nCommonInputs, nDIPInputs;
static int nPlayerOffset[MAXPLAYER], nCommonOffset, nDIPOffset;

const int INPUTSIZE = 8 * (4 + 8 + 0x40); // + 0x40 ZX Spectrum keyboard
static unsigned char nControls[INPUTSIZE];

// Inputs are assumed to be in the following order:
// All player 1 controls
// All player 2 controls (if any)
// All player 3 controls (if any)
// All player 4 controls (if any)
// All common controls
// All DIP switches

int KailleraInitInput()
{
	if (nGameInpCount == 0) {
		return 1;
	}

	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));

	unsigned int i = 0;

	nPlayerOffset[0] = 0;
	do {
		BurnDrvGetInputInfo(&bii, i);
		i++;
	} while (!_strnicmp(bii.szName, "P1", 2) && i <= nGameInpCount);
	i--;
	nPlayerInputs[0] = i - nPlayerOffset[0];

	for (int j = 1; j < MAXPLAYER; j++) {
		char szString[3] = "P?";
		szString[1] = j + '1';
		nPlayerOffset[j] = i;
		while (!_strnicmp(bii.szName, szString, 2) && i < nGameInpCount) {
			i++;
			BurnDrvGetInputInfo(&bii, i);
		}
		nPlayerInputs[j] = i - nPlayerOffset[j];
	}

	nCommonOffset = i;
	while ((bii.nType & BIT_GROUP_CONSTANT) == 0 && i < nGameInpCount){
		i++;
		BurnDrvGetInputInfo(&bii, i);
	};
	nCommonInputs = i - nCommonOffset;

	nDIPOffset = i;
	nDIPInputs = nGameInpCount - nDIPOffset;

#if 1 && defined FBNEO_DEBUG
	dprintf(_T("  * Kaillera inputs configured as follows --\n"));
	for (int j = 0; j < MAXPLAYER; j++) {
		dprintf(_T("    p%d offset %d, inputs %d.\n"), j + 1, nPlayerOffset[j], nPlayerInputs[j]);
	}
	dprintf(_T("    common offset %d, inputs %d.\n"), nCommonOffset, nCommonInputs);
	dprintf(_T("    dip offset %d, inputs %d.\n"), nDIPOffset, nDIPInputs);
#endif

	return 0;
}

int KailleraGetInput()
{
	int i, j, k;

	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));

	// Initialize controls to 0
	memset(nControls, 0, INPUTSIZE);

	// Pack all player 1 controls, common controls, analog controls & DIP switches
	for (i = 0, j = 0; i < nPlayerInputs[0]; i++, j++) {
		BurnDrvGetInputInfo(&bii, i + nPlayerOffset[0]);
		if (bii.pVal && *bii.pVal && bii.nType == BIT_DIGITAL) {
			nControls[j >> 3] |= (1 << (j & 7));
		}
	}
	for (i = 0; i < nCommonInputs; i++, j++) {
		BurnDrvGetInputInfo(&bii, i + nCommonOffset);
		if (bii.pVal && *bii.pVal) {
			nControls[j >> 3] |= (1 << (j & 7));
		}
	}

	// Convert j to byte count
	j = (j + 7) >> 3;

	// Analog controls/constants
	for (i = 0; i < nPlayerInputs[0]; i++) {
		BurnDrvGetInputInfo(&bii, i + nPlayerOffset[0]);
		if (bii.nType != BIT_DIGITAL) {
			if (bii.nType & BIT_GROUP_ANALOG) {
				if (bii.pVal && *bii.pShortVal) {
					nControls[j] = *bii.pShortVal >> 8;
					nControls[j+1] = *bii.pShortVal & 0xFF;
				}
				j += 2;
			} else {
				if (bii.pVal && *bii.pVal) {
					nControls[j] = *bii.pVal;
				}
				j += 1;
			}
		}
	}

	// DIP switches
	for (i = 0; i < nDIPInputs; i++, j++) {
		BurnDrvGetInputInfo(&bii, i + nDIPOffset);
		nControls[j] = *bii.pVal;
	}

	// k has the size of all inputs for one player
	k = j + 1;

	// Send the control block to the Kaillera DLL & retrieve all controls
	if (Kaillera_Modify_Play_Values(nControls, k) == -1) {
		kNetGame = 0;
		return 1;
	}

	// Decode Player 1 input block
	for (i = 0, j = 0; i < nPlayerInputs[0]; i++, j++) {
		BurnDrvGetInputInfo(&bii, i + nPlayerOffset[0]);
		if (bii.nType == BIT_DIGITAL) {
			if (nControls[j >> 3] & (1 << (j & 7))) {
				if (bii.pVal) *bii.pVal = 0x01;
			} else {
				if (bii.pVal) *bii.pVal = 0x00;
			}
		}
	}
	for (i = 0; i < nCommonInputs; i++, j++) {
		BurnDrvGetInputInfo(&bii, i + nCommonOffset);
		if (nControls[j >> 3] & (1 << (j & 7))) {
			if (bii.pVal) *bii.pVal = 0x01;
		} else {
			if (bii.pVal) *bii.pVal = 0x00;
		}
	}

	// Convert j to byte count
	j = (j + 7) >> 3;

	// Analog inputs
	for (i = 0; i < nPlayerInputs[0]; i++) {
		BurnDrvGetInputInfo(&bii, i + nPlayerOffset[0]);

		if (bii.nType != BIT_DIGITAL) {
			if (bii.nType & BIT_GROUP_ANALOG) {
				if (bii.pShortVal) *bii.pShortVal = (nControls[j] << 8) | nControls[j + 1];
				j += 2;
			} else {
				if (bii.pVal) *bii.pVal = nControls[j];
				j++;
			}
		}
	}

	// DIP switches
	for (i = 0; i < nDIPInputs; i++, j++) {
		BurnDrvGetInputInfo(&bii, i + nDIPOffset);
		if (bii.pVal) *bii.pVal = nControls[j];
		//bprintf(0, _T("remote dip %d:   %x\n"), i, nControls[j]);
	}

	// Decode other player's input blocks
	for (int l = 1; l < MAXPLAYER; l++) {
		if (nPlayerInputs[l]) {
			for (i = 0, j = k * (l << 3); i < nPlayerInputs[l]; i++, j++) {
				BurnDrvGetInputInfo(&bii, i + nPlayerOffset[l]);
				if (bii.nType == BIT_DIGITAL) {
					if (nControls[j >> 3] & (1 << (j & 7))) {
						if (bii.pVal) *bii.pVal = 0x01;
					} else {
						if (bii.pVal) *bii.pVal = 0x00;
					}
				}
			}

			for (i = 0; i < nCommonInputs; i++, j++) {
				if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SPECTRUM) {
					// Allow other players to use common inputs: Only ZX Spectrum
					BurnDrvGetInputInfo(&bii, i + nCommonOffset);
					if (nControls[j >> 3] & (1 << (j & 7))) {
						if (bii.pVal) *bii.pVal |= 0x01;
					}
				}
			}

			// Convert j to byte count
			j = (j + 7) >> 3;

			// Analog inputs/constants
			for (i = 0; i < nPlayerInputs[l]; i++) {
				BurnDrvGetInputInfo(&bii, i + nPlayerOffset[l]);
				if (bii.nType != BIT_DIGITAL) {
					if (bii.nType & BIT_GROUP_ANALOG) {
						if (bii.pShortVal) *bii.pShortVal = (nControls[j] << 8) | nControls[j + 1];
						j += 2;
					} else {
						if (bii.pVal) *bii.pVal = nControls[j];
						j++;
					}
				}
			}

#if 0
			// For a DIP switch to be set to 1, ALL players must set it
			for (i = 0; i < nDIPInputs; i++, j++) {
				BurnDrvGetInputInfo(&bii, i + nDIPOffset);
				*bii.pVal &= nControls[j];
			}
#endif
		}
	}

	return 0;
}
