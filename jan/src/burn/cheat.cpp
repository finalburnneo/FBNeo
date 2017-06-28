// Cheat module

#include "burnint.h"

#define CHEAT_MAXCPU	8 // enough?


bool bCheatsAllowed;
CheatInfo* pCheatInfo = NULL;

static bool bCheatsEnabled = false;
static INT32 cheat_core_init_pointer = 0;

struct cheat_core {
	cpu_core_config *cpuconfig;

	INT32 nCPU;			// which cpu
};

static struct cheat_core cpus[CHEAT_MAXCPU];
static cheat_core *cheat_ptr;
static cpu_core_config *cheat_subptr;

void CpuCheatRegister(INT32 nCPU, cpu_core_config *config)
{
	cheat_core *s_ptr = &cpus[cheat_core_init_pointer];

	s_ptr->cpuconfig = config;
	s_ptr->nCPU = nCPU;

	cheat_core_init_pointer++;
}

INT32 CheatUpdate()
{
	bCheatsEnabled = false;

	if (bCheatsAllowed) {
		CheatInfo* pCurrentCheat = pCheatInfo;
		CheatAddressInfo* pAddressInfo;

		while (pCurrentCheat) {
			if (pCurrentCheat->nStatus > 1) {
				pAddressInfo = pCurrentCheat->pOption[pCurrentCheat->nCurrent]->AddressInfo;
				if (pAddressInfo->nAddress) {
					bCheatsEnabled = true;
				}
			}
			pCurrentCheat = pCurrentCheat->pNext;
		}
	}

	return 0;
}

INT32 CheatEnable(INT32 nCheat, INT32 nOption) // -1 / 0 - disable, -2 disable & don't undo memory
{
	INT32 nCurrentCheat = 0;
	CheatInfo* pCurrentCheat = pCheatInfo;
	CheatAddressInfo* pAddressInfo;
	INT32 nOpenCPU = -1;
	INT32 no_undo = 0;

	if (!bCheatsAllowed) {
		return 1;
	}

	if (nOption >= CHEAT_MAX_OPTIONS) {
		return 1;
	}

	cheat_ptr = &cpus[0]; // first cpu...
	cheat_subptr = cheat_ptr->cpuconfig;

	while (pCurrentCheat && nCurrentCheat <= nCheat) {
		if (nCurrentCheat == nCheat) { // Cheat found, let's process it.
			INT32 deactivate = 0;

			if (nOption == -1 || nOption == 0 || nOption == -2) { // -2 = dont write back previous
				if (nOption == -2) no_undo = 1;
				nOption = pCurrentCheat->nDefault;
				deactivate = 1;
			}

			// Return OK if the cheat is already active with the same option
			if (pCurrentCheat->nCurrent == nOption) {
				return 0;
			}

			if (deactivate) { // disable cheat option
				if (pCurrentCheat->nType != 1) {
					nOption = 1; // Set to the first option as there is no addressinfo associated with default (disabled) cheat entry. -dink

					// Deactivate old option (if any)
					pAddressInfo = pCurrentCheat->pOption[nOption]->AddressInfo;

					while (pAddressInfo->nAddress) {
						if (pAddressInfo->nCPU != nOpenCPU) {

							if (nOpenCPU != -1) {
								cheat_subptr->close();
							}

							nOpenCPU = pAddressInfo->nCPU;
							cheat_ptr = &cpus[nOpenCPU];
							cheat_subptr = cheat_ptr->cpuconfig;
							cheat_subptr->open(cheat_ptr->nCPU);
						}

						if (!no_undo) {
							// Write back original values to memory
							bprintf(0, _T("Cheat #%d, option #%d. action: "), nCheat, nOption);
							bprintf(0, _T("Undo cheat @ 0x%X -> 0x%X.\n"), pAddressInfo->nAddress, pAddressInfo->nOriginalValue);
							cheat_subptr->write(pAddressInfo->nAddress, pAddressInfo->nOriginalValue);
						}
						pAddressInfo++;
					}
					nOption = 0; // Set back to 0 (see above line: nOption = 1;)
				}
			} else { // activate cheat option
				pAddressInfo = pCurrentCheat->pOption[nOption]->AddressInfo;

				while (pAddressInfo->nAddress) {
					if (pAddressInfo->nCPU != nOpenCPU) {
						if (nOpenCPU != -1) {
							cheat_subptr->close();
						}

						nOpenCPU = pAddressInfo->nCPU;
						cheat_ptr = &cpus[nOpenCPU];
						cheat_subptr = cheat_ptr->cpuconfig;
						cheat_subptr->open(cheat_ptr->nCPU);
					}

					pCurrentCheat->bModified = 0;

					// Copy the original values
					pAddressInfo->nOriginalValue = cheat_subptr->read(pAddressInfo->nAddress);

					bprintf(0, _T("Cheat #%d, option #%d. action: "), nCheat, nOption);
					if (pCurrentCheat->bWatchMode) {
						bprintf(0, _T("Watch memory @ 0x%X (0x%X)\n"), pAddressInfo->nAddress, pAddressInfo->nOriginalValue);
					} else
					if (pCurrentCheat->bOneShot) {
						bprintf(0, _T("Apply cheat @ 0x%X -> 0x%X. (Before 0x%X - One-Shot mode)\n"), pAddressInfo->nAddress, pAddressInfo->nValue, pAddressInfo->nOriginalValue);
						pCurrentCheat->bOneShot = 3; // re-load the one-shot frame counter
					} else {
						bprintf(0, _T("Apply cheat @ 0x%X -> 0x%X. (Undo 0x%X)\n"), pAddressInfo->nAddress, pAddressInfo->nValue, pAddressInfo->nOriginalValue);
					}
					if (pCurrentCheat->bWaitForModification)
						bprintf(0, _T(" - Triggered by: Waiting for modification!\n"));

					if (pCurrentCheat->nType != 0) {
						if (pAddressInfo->nCPU != nOpenCPU) {
							if (nOpenCPU != -1) {
								cheat_subptr->close();
							}

							nOpenCPU = pAddressInfo->nCPU;
							cheat_ptr = &cpus[nOpenCPU];
							cheat_subptr = cheat_ptr->cpuconfig;
							cheat_subptr->open(cheat_ptr->nCPU);
						}

						if (!pCurrentCheat->bWatchMode && !pCurrentCheat->bWaitForModification) {
							// Activate the cheat
							cheat_subptr->write(pAddressInfo->nAddress, pAddressInfo->nValue);
						}
					}

					pAddressInfo++;
				}
			}

			// Set cheat status and active option
			if (pCurrentCheat->nType != 1) {
				pCurrentCheat->nCurrent = nOption;
			}
			if (pCurrentCheat->nType == 0) {
				pCurrentCheat->nStatus = 2;
			}
			if (pCurrentCheat->nType == 2) {
				pCurrentCheat->nStatus = 1;
			}

			break;
		}
		pCurrentCheat = pCurrentCheat->pNext;
		nCurrentCheat++;
	}

	if (nOpenCPU != -1) {
		cheat_subptr->close();
	}

	CheatUpdate();

	if (nCurrentCheat == nCheat && pCurrentCheat) {
		return 0;
	}

	return 1;
}

#if defined (BUILD_WIN32)
extern INT32 VidSNewTinyMsg(const TCHAR* pText, INT32 nRGB = 0, INT32 nDuration = 0, INT32 nPriority = 5);
#endif

INT32 CheatApply()
{
	if (!bCheatsEnabled) {
		return 0;
	}

	INT32 nOpenCPU = -1;
	INT32 nCurrentCheat = 0;

	CheatInfo* pCurrentCheat = pCheatInfo;
	CheatAddressInfo* pAddressInfo;
	while (pCurrentCheat) {
		if (pCurrentCheat->nStatus > 1) {
			pAddressInfo = pCurrentCheat->pOption[pCurrentCheat->nCurrent]->AddressInfo;

			while (pAddressInfo->nAddress) {

				if (pAddressInfo->nCPU != nOpenCPU) {
					if (nOpenCPU != -1) {
						cheat_subptr->close();
					}

					nOpenCPU = pAddressInfo->nCPU;
					cheat_ptr = &cpus[nOpenCPU];
					cheat_subptr = cheat_ptr->cpuconfig;
					cheat_subptr->open(cheat_ptr->nCPU);
				}

				if (pCurrentCheat->bWatchMode) {
					// Watch address mode, Win32-only for now.
#if defined (BUILD_WIN32)
					pAddressInfo->nOriginalValue = cheat_subptr->read(pAddressInfo->nAddress);
					wchar_t framestring[32];
					swprintf(framestring, L"%X", pAddressInfo->nOriginalValue);
					VidSNewTinyMsg(framestring, 0, 5, 5);
#endif
				} else {
					// update the cheat
					if (pCurrentCheat->bWaitForModification) {
						UINT32 nValNow = cheat_subptr->read(pAddressInfo->nAddress);
						if (nValNow != pAddressInfo->nOriginalValue) {
							bprintf(0, _T(" - Address modified! old = %X new = %X\n"),pAddressInfo->nOriginalValue, nValNow);
							cheat_subptr->write(pAddressInfo->nAddress, pAddressInfo->nValue);
							pCurrentCheat->bModified = 1;
							pAddressInfo->nOriginalValue = pAddressInfo->nValue;
						}
					} else {
						// Write the value.
						cheat_subptr->write(pAddressInfo->nAddress, pAddressInfo->nValue);
						pCurrentCheat->bModified = 1;
					}
				}
				pAddressInfo++;
			}
			if (pCurrentCheat->bModified) {
				if (pCurrentCheat->bOneShot == 2) {
					if (nOpenCPU != -1) {
						cheat_subptr->close();
						nOpenCPU = -1;
					}
					bprintf(0, _T("One-Shot cheat #%d ends.\n"), nCurrentCheat);
					CheatEnable(nCurrentCheat, -2);
				}
				if (pCurrentCheat->bOneShot > 1) pCurrentCheat->bOneShot--;
			}
		}
		pCurrentCheat = pCurrentCheat->pNext;
		nCurrentCheat++;
	}

	if (nOpenCPU != -1) {
		cheat_subptr->close();
	}

	return 0;
}

INT32 CheatInit()
{
	CheatExit();

	bCheatsEnabled = false;

	return 0;
}

void CheatExit()
{
	if (pCheatInfo) {
		CheatInfo* pCurrentCheat = pCheatInfo;
		CheatInfo* pNextCheat;

		do {
			pNextCheat = pCurrentCheat->pNext;
			for (INT32 i = 0; i < CHEAT_MAX_OPTIONS; i++) {
				BurnFree(pCurrentCheat->pOption[i]);
			}
			BurnFree(pCurrentCheat);
		} while ((pCurrentCheat = pNextCheat) != 0);
	}

	memset (cpus, 0, sizeof(cheat_core));

	cheat_core_init_pointer = 0;

	pCheatInfo = NULL;
	
	CheatSearchInitCallbackFunction = NULL;
}

// Cheat search

static UINT8 *MemoryValues = NULL;
static UINT8 *MemoryStatus = NULL;
static UINT32 nMemorySize = 0;
CheatSearchInitCallback CheatSearchInitCallbackFunction = NULL;

#define NOT_IN_RESULTS	0
#define IN_RESULTS	1

UINT32 CheatSearchShowResultAddresses[CHEATSEARCH_SHOWRESULTS];
UINT32 CheatSearchShowResultValues[CHEATSEARCH_SHOWRESULTS];

INT32 CheatSearchInit()
{
	return 1;
}

void CheatSearchExit()
{
	BurnFree(MemoryValues);
	BurnFree(MemoryStatus);
	
	nMemorySize = 0;
	
	memset(CheatSearchShowResultAddresses, 0, CHEATSEARCH_SHOWRESULTS);
	memset(CheatSearchShowResultValues, 0, CHEATSEARCH_SHOWRESULTS);
}

void CheatSearchStart()
{
	UINT32 nAddress;
	
	INT32 nActiveCPU = 0;
	cheat_ptr = &cpus[nActiveCPU];
	cheat_subptr = cheat_ptr->cpuconfig;
	cheat_subptr->open(cheat_ptr->nCPU);

	nActiveCPU = cheat_subptr->active();
	if (nActiveCPU >= 0) cheat_subptr->close();
	cheat_subptr->open(cheat_ptr->nCPU);
	nMemorySize = cheat_subptr->nMemorySize;

	MemoryValues = (UINT8*)BurnMalloc(nMemorySize);
	MemoryStatus = (UINT8*)BurnMalloc(nMemorySize);
	
	memset(MemoryStatus, IN_RESULTS, nMemorySize);
	
	if (CheatSearchInitCallbackFunction) CheatSearchInitCallbackFunction();

	for (nAddress = 0; nAddress < nMemorySize; nAddress++) {
		if (MemoryStatus[nAddress] == NOT_IN_RESULTS) continue;
		MemoryValues[nAddress] = cheat_subptr->read(nAddress);
	}
	
	cheat_subptr->close();
	if (nActiveCPU >= 0) cheat_subptr->open(nActiveCPU);
}

static void CheatSearchGetResults()
{
	UINT32 nAddress;
	UINT32 nResultsPos = 0;
	
	memset(CheatSearchShowResultAddresses, 0, CHEATSEARCH_SHOWRESULTS);
	memset(CheatSearchShowResultValues, 0, CHEATSEARCH_SHOWRESULTS);
	
	for (nAddress = 0; nAddress < nMemorySize; nAddress++) {		
		if (MemoryStatus[nAddress] == IN_RESULTS) {
			CheatSearchShowResultAddresses[nResultsPos] = nAddress;
			CheatSearchShowResultValues[nResultsPos] = MemoryValues[nAddress];
			nResultsPos++;
		}
	}
}

UINT32 CheatSearchValueNoChange()
{
	UINT32 nMatchedAddresses = 0;
	UINT32 nAddress;
	
	INT32 nActiveCPU = 0;
	
	nActiveCPU = cheat_subptr->active();
	if (nActiveCPU >= 0) cheat_subptr->close();
	cheat_subptr->open(0);
	
	for (nAddress = 0; nAddress < nMemorySize; nAddress++) {
		if (MemoryStatus[nAddress] == NOT_IN_RESULTS) continue;
		if (cheat_subptr->read(nAddress) == MemoryValues[nAddress]) {
			MemoryValues[nAddress] = cheat_subptr->read(nAddress);
			nMatchedAddresses++;
		} else {
			MemoryStatus[nAddress] = NOT_IN_RESULTS;
		}
	}

	cheat_subptr->close();
	if (nActiveCPU >= 0) cheat_subptr->open(nActiveCPU);
	
	if (nMatchedAddresses <= CHEATSEARCH_SHOWRESULTS) CheatSearchGetResults();
	
	return nMatchedAddresses;
}

UINT32 CheatSearchValueChange()
{
	UINT32 nMatchedAddresses = 0;
	UINT32 nAddress;
	
	INT32 nActiveCPU = 0;
	
	nActiveCPU = cheat_subptr->active();
	if (nActiveCPU >= 0) cheat_subptr->close();
	cheat_subptr->open(0);
	
	for (nAddress = 0; nAddress < nMemorySize; nAddress++) {
		if (MemoryStatus[nAddress] == NOT_IN_RESULTS) continue;
		if (cheat_subptr->read(nAddress) != MemoryValues[nAddress]) {
			MemoryValues[nAddress] = cheat_subptr->read(nAddress);
			nMatchedAddresses++;
		} else {
			MemoryStatus[nAddress] = NOT_IN_RESULTS;
		}
	}
	
	cheat_subptr->close();
	if (nActiveCPU >= 0) cheat_subptr->open(nActiveCPU);
	
	if (nMatchedAddresses <= CHEATSEARCH_SHOWRESULTS) CheatSearchGetResults();
	
	return nMatchedAddresses;
}

UINT32 CheatSearchValueDecreased()
{
	UINT32 nMatchedAddresses = 0;
	UINT32 nAddress;
	
	INT32 nActiveCPU = 0;
	
	nActiveCPU = cheat_subptr->active();
	if (nActiveCPU >= 0) cheat_subptr->close();
	cheat_subptr->open(0);

	for (nAddress = 0; nAddress < nMemorySize; nAddress++) {
		if (MemoryStatus[nAddress] == NOT_IN_RESULTS) continue;
		if (cheat_subptr->read(nAddress) < MemoryValues[nAddress]) {
			MemoryValues[nAddress] = cheat_subptr->read(nAddress);
			nMatchedAddresses++;
		} else {
			MemoryStatus[nAddress] = NOT_IN_RESULTS;
		}
	}

	cheat_subptr->close();
	if (nActiveCPU >= 0) cheat_subptr->open(nActiveCPU);
	
	if (nMatchedAddresses <= CHEATSEARCH_SHOWRESULTS) CheatSearchGetResults();
	
	return nMatchedAddresses;
}

UINT32 CheatSearchValueIncreased()
{
	UINT32 nMatchedAddresses = 0;
	UINT32 nAddress;
	
	INT32 nActiveCPU = 0;
	
	nActiveCPU = cheat_subptr->active();
	if (nActiveCPU >= 0) cheat_subptr->close();
	cheat_subptr->open(0);

	for (nAddress = 0; nAddress < nMemorySize; nAddress++) {
		if (MemoryStatus[nAddress] == NOT_IN_RESULTS) continue;
		if (cheat_subptr->read(nAddress) > MemoryValues[nAddress]) {
			MemoryValues[nAddress] = cheat_subptr->read(nAddress);
			nMatchedAddresses++;
		} else {
			MemoryStatus[nAddress] = NOT_IN_RESULTS;
		}
	}
	
	cheat_subptr->close();
	if (nActiveCPU >= 0) cheat_subptr->open(nActiveCPU);
	
	if (nMatchedAddresses <= CHEATSEARCH_SHOWRESULTS) CheatSearchGetResults();
	
	return nMatchedAddresses;
}

void CheatSearchDumptoFile()
{
	FILE *fp = fopen("cheatsearchdump.txt", "wt");
	UINT32 nAddress;
	
	if (fp) {
		char Temp[256];
		
		for (nAddress = 0; nAddress < nMemorySize; nAddress++) {
			if (MemoryStatus[nAddress] == IN_RESULTS) {
				sprintf(Temp, "Address %08X Value %02X\n", nAddress, MemoryValues[nAddress]);
				fwrite(Temp, 1, strlen(Temp), fp);
			}
		}
		
		fclose(fp);
	}
}

void CheatSearchExcludeAddressRange(UINT32 nStart, UINT32 nEnd)
{
	for (UINT32 nAddress = nStart; nAddress <= nEnd; nAddress++) {
		MemoryStatus[nAddress] = NOT_IN_RESULTS;
	}
}

#undef NOT_IN_RESULTS
#undef IN_RESULTS
