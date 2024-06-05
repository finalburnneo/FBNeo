#include "burner.h"
#include <string.h>

#define MAX_LST_GAMES		25000
#define MAX_LST_LINE_LEN	256

TCHAR szGamelistLocalisationTemplate[MAX_PATH] = _T("");
bool nGamelistLocalisationActive = false;
static INT32 nCodePage = CP_ACP;
static TCHAR* szLongNamesArray[MAX_LST_GAMES];

static void BurnerDoGameListExLocalisation()
{
	if (!nGamelistLocalisationActive) return;

	TCHAR szGamelistExLocalisationTemplate[MAX_PATH] = _T(""), szGamelistEx[MAX_PATH] = _T(""), * pszPos = NULL;

	_tcscpy(szGamelistExLocalisationTemplate, szGamelistLocalisationTemplate);
	pszPos = _tcsstr(szGamelistExLocalisationTemplate, _T(".glt"));

	if (NULL != pszPos) {
		pszPos[0] = _T('\0');
		pszPos = NULL;
	}

	_stprintf(szGamelistEx, _T("%sex.glt"), szGamelistExLocalisationTemplate);

	FILE* gl = _tfopen(szGamelistEx, _T("rt"));

	if (NULL == gl) return;

	INT32 nArrayPos = 0;
	INT32 nTokenPos = 0;
	char szTemp[MAX_LST_LINE_LEN] = { '\0' };

	while (fgets(szTemp, sizeof(szTemp), gl)) {
		if (szTemp[0] == '/' && szTemp[1] == '/') {
			continue;
		}

		if (!strncmp(szTemp, "codepage=", 9)) {
			if ((strlen(szTemp) - 10) == 4) {
				nCodePage =  (szTemp[ 9] - '0') * 1000;
				nCodePage += (szTemp[10] - '0') * 100;
				nCodePage += (szTemp[11] - '0') * 10;
				nCodePage += (szTemp[12] - '0');
			}
			if ((strlen(szTemp) - 10) == 3) {
				nCodePage =  (szTemp[ 9] - '0') * 100;
				nCodePage += (szTemp[10] - '0') * 10;
				nCodePage += (szTemp[11] - '0');
			}
			continue;
		}

		// Get rid of the linefeed at the end
		INT32 nLen = strlen(szTemp);
		if ((nLen > 0) && (szTemp[nLen - 1] == 10)) {
			szTemp[nLen - 1] = 0;
			nLen--;
		}

		TCHAR szLine[MAX_LST_LINE_LEN * sizeof(TCHAR)];

		MultiByteToWideChar(nCodePage, 0, szTemp, -1, szLine, sizeof(szLine) / sizeof(TCHAR));

		TCHAR* Tokens = _tcstok(szLine, _T("\t"));
		while (Tokens != NULL) {
			Tokens = _tcstok(NULL, _T("\t"));
		}
		nArrayPos++;
	}
	nNamesExArray = nArrayPos;
	nArrayPos = 0;
	rewind(gl);
	memset(szTemp, 0, sizeof(szTemp));

	szShortNamesExArray = (char** )malloc(nNamesExArray * sizeof(char*));
	szLongNamesExArray  = (TCHAR**)malloc(nNamesExArray * sizeof(TCHAR*));

	if ((NULL != szShortNamesExArray) && (NULL != szLongNamesExArray)) {
		// Allocate arrays to read the file into
		for (INT32 i = 0; i < nNamesExArray; i++) {
			szShortNamesExArray[i] = (char* )malloc(100);
			szLongNamesExArray[i]  = (TCHAR*)malloc(MAX_LST_LINE_LEN * sizeof(TCHAR));
			memset(szShortNamesExArray[i], '\0', 100);
			memset(szLongNamesExArray[i],  '\0', MAX_LST_LINE_LEN * sizeof(TCHAR));
		}
		while (NULL != fgets(szTemp, sizeof(szTemp), gl)) {
			if (szTemp[0] == '/' && szTemp[1] == '/') {
				continue;
			}

			// Get rid of the linefeed at the end
			INT32 nLen = strlen(szTemp);
			if ((nLen > 0) && (szTemp[nLen - 1] == 10)) {
				szTemp[nLen - 1] = 0;
				nLen--;
			}

			TCHAR szLine[MAX_LST_LINE_LEN * sizeof(TCHAR)];

			MultiByteToWideChar(nCodePage, 0, szTemp, -1, szLine, sizeof(szLine) / sizeof(TCHAR));

			// Read the file into arrays
			TCHAR* Tokens = _tcstok(szLine, _T("\t"));
			while (NULL != Tokens) {
				if (nTokenPos == 0) {
					szShortNamesExArray[nArrayPos] = (char*)malloc(100);
					memset(szShortNamesExArray[nArrayPos], '\0', 100);
					strcpy(szShortNamesExArray[nArrayPos], TCHARToANSI(Tokens, NULL, 0));
				}

				if (nTokenPos == 1) {
					szLongNamesExArray[nArrayPos] = (TCHAR*)malloc( MAX_LST_LINE_LEN * sizeof(TCHAR));
					memset(szLongNamesExArray[nArrayPos], '\0', MAX_LST_LINE_LEN * sizeof(TCHAR));
					wcscpy(szLongNamesExArray[nArrayPos], Tokens);
				}

				Tokens = _tcstok(NULL, _T("\t"));
				nTokenPos++;
			}
			nTokenPos = 0;
			nArrayPos++;
		}
	}

	fclose(gl);
}

void BurnerDoGameListLocalisation()
{
	if (!nGamelistLocalisationActive) return;

	FILE* fp = _tfopen(szGamelistLocalisationTemplate, _T("rt"));
	if (fp) {
		TCHAR *szShortNamesArray[MAX_LST_GAMES];
		TCHAR szLine[MAX_LST_LINE_LEN * sizeof(TCHAR)];
		int nTokenPos = 0;
		int nArrayPos = 0;
		int nNumGames = 0;

		// Allocate arrays to read the file into
		for (int i = 0; i < MAX_LST_GAMES; i++) {
			szLongNamesArray[i]  = (TCHAR*)malloc( MAX_LST_LINE_LEN * sizeof(TCHAR));
			szShortNamesArray[i] = (TCHAR*)malloc( 100              * sizeof(TCHAR));
			memset(szLongNamesArray[i],  '\0', MAX_LST_LINE_LEN * sizeof(TCHAR));
			memset(szShortNamesArray[i], '\0', 100              * sizeof(TCHAR));
		}

		char szTemp[MAX_LST_LINE_LEN];

		while (fgets(szTemp, sizeof(szTemp), fp)) {
			if (szTemp[0] == '/' && szTemp[1] == '/') {
				continue;
			}

			if (!strncmp(szTemp, "codepage=", 9)) {
				if ((strlen(szTemp) - 10) == 4) {
					nCodePage  = (szTemp[ 9] - '0') * 1000;
					nCodePage += (szTemp[10] - '0') * 100;
					nCodePage += (szTemp[11] - '0') * 10;
					nCodePage += (szTemp[12] - '0');
				}
				if ((strlen(szTemp) - 10) == 3) {
					nCodePage  = (szTemp[ 9] - '0') * 100;
					nCodePage += (szTemp[10] - '0') * 10;
					nCodePage += (szTemp[11] - '0');
				}
				continue;
			}

			// Get rid of the linefeed at the end
			int nLen = strlen(szTemp);
			if (nLen > 0 && szTemp[nLen - 1] == 10) {
				szTemp[nLen - 1] = 0;
				nLen--;
			}

			MultiByteToWideChar(nCodePage, 0, szTemp, -1, szLine, sizeof(szLine) / sizeof(TCHAR));

			TCHAR *Tokens;

			// Read the file into arrays
			Tokens = _tcstok(szLine, _T("\t"));
			while (Tokens != NULL) {
				if (nTokenPos == 0) {
					wcscpy(szShortNamesArray[nArrayPos], Tokens);
//					szShortNamesArray[nArrayPos][_tcslen(Tokens)] = _T('\0');
				}

				if (nTokenPos == 1) {
					wcscpy(szLongNamesArray[nArrayPos], Tokens);
//					szLongNamesArray[nArrayPos][_tcslen(Tokens)] = _T('\0');
				}

				Tokens = _tcstok(NULL, _T("\t"));
				nTokenPos++;
			}

			nTokenPos = 0;
			nArrayPos++;
		}
		nNumGames = nArrayPos;

		for (int i = 0; i < nNumGames; i++) {
			BurnLocalisationSetName(TCHARToANSI(szShortNamesArray[i], NULL, 0), szLongNamesArray[i]);
		}

		// tidy up
		for (int i = 0; i < MAX_LST_GAMES; i++) {
			if (szShortNamesArray[i]) {
				free(szShortNamesArray[i]);
				szShortNamesArray[i] = NULL;
			}
		}

		fclose(fp);
	}

	BurnerDoGameListExLocalisation();
}

static void BurnerGameListCreateTemplate()
{
	unsigned int nOldDrvSelect = nBurnDrvActive;

	FILE* fp = _tfopen(szGamelistLocalisationTemplate, _T("wt"));
	if (fp) {
		_ftprintf(fp, _T("// game list translation template for FinalBurn Neo version 0x%06X\n\n"), nBurnVer);
		_ftprintf(fp, _T("// codepage=1252\n\n"));

		for (unsigned int i = 0; i < nBurnDrvCount; i++) {
			nBurnDrvActive = i;
			_ftprintf(fp, _T("%s\t%s\n"), BurnDrvGetText(DRV_NAME), BurnDrvGetText(DRV_ASCIIONLY | DRV_FULLNAME));
		}

		fclose(fp);
	}

	nBurnDrvActive = nOldDrvSelect;
}

void BurnerExitGameListLocalisation()
{
	for (int i = 0; i < MAX_LST_GAMES; i++) {
		if (NULL != szLongNamesArray[i]) {
			free(szLongNamesArray[i]);
			szLongNamesArray[i] = NULL;
		}
		if (i < nNamesExArray) {
			if (NULL != szShortNamesExArray[i]) {
				free(szShortNamesExArray[i]);
				szShortNamesExArray[i] = NULL;
			}
			if (NULL != szLongNamesExArray[i]) {
				free(szLongNamesExArray[i]);
				szLongNamesExArray[i] = NULL;
			}
		}
	}
	if (NULL != szShortNamesExArray) {
		free(szShortNamesExArray);
		szShortNamesExArray = NULL;
	}
	if (NULL != szLongNamesExArray) {
		free(szLongNamesExArray);
		szLongNamesExArray = NULL;
	}

	nCodePage = CP_ACP;
}

// ----------------------------------------------------------------------------
// Dialog box to load/save a template

static TCHAR szFilter[100];

static void MakeOfn()
{
	_stprintf(szFilter, _T("%s"), FBALoadStringEx(hAppInst, IDS_LOCAL_GL_FILTER, true));
	memcpy(szFilter + _tcslen(szFilter), _T(" (*.glt)\0*.glt\0\0"), 16 * sizeof(TCHAR));

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hScrnWnd;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = szGamelistLocalisationTemplate;
	ofn.nMaxFile = sizeof(szGamelistLocalisationTemplate) / sizeof(TCHAR);
	ofn.lpstrInitialDir = _T(".\\config\\localisation");
	ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = _T("glt");

	return;
}

int FBALocaliseGamelistLoadTemplate()
{
	_stprintf(szGamelistLocalisationTemplate, _T("template"));
	MakeOfn();
	TCHAR szTitle[100];
	_stprintf(szTitle, _T("%s"), FBALoadStringEx(hAppInst, IDS_LOCAL_GL_SELECT, true));
	ofn.lpstrTitle = szTitle;
	ofn.Flags |= OFN_OVERWRITEPROMPT;

	int bOldPause = bRunPause;
	bRunPause = 1;
	int nRet = GetOpenFileName(&ofn);
	bRunPause = bOldPause;

	if (nRet == 0) {
		return 1;
	}

	nGamelistLocalisationActive = true;
	BurnerDoGameListLocalisation();

	return 0;
}

int FBALocaliseGamelistCreateTemplate()
{
	_stprintf(szGamelistLocalisationTemplate, _T("template"));
	MakeOfn();
	TCHAR szTitle[100];
	_stprintf(szTitle, _T("%s"), FBALoadStringEx(hAppInst, IDS_LOCAL_GL_CREATE, true));
	ofn.lpstrTitle = szTitle;
	ofn.Flags |= OFN_OVERWRITEPROMPT;

	int bOldPause = bRunPause;
	bRunPause = 1;
	int nRet = GetSaveFileName(&ofn);
	bRunPause = bOldPause;

	if (nRet == 0) {
		return 1;
	}

	BurnerGameListCreateTemplate();

	return 0;
}

#undef MAX_LST_LINE_LEN
#undef MAX_LST_GAMES
