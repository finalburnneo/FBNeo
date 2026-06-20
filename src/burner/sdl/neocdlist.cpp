// ---------------------------------------------------------------------------------------
// NeoGeo CD Game Info Module (by CaptainCPS-X)
// ---------------------------------------------------------------------------------------
#include "burner.h"
#include "neocdlist.h"

const int nSectorLength = 2352;

NGCDGAME* game;

#include "neocdlist_games.h"

NGCDGAME* GetNeoGeoCDInfo(unsigned int nID)
{
	for (unsigned int nGame = 0; nGame < (sizeof(games) / sizeof(NGCDGAME)); nGame++) {
		if (nID == games[nGame].id) {
			return &games[nGame];
		}
	}

	return NULL;
}

// Update the main window title
static void SetNeoCDTitle(TCHAR* pszTitle)
{
	TCHAR szText[1024] = _T("");
	_stprintf(szText, _T(APP_TITLE) _T(" v%.20s") _T(SEPERATOR_1) _T("%s") _T(SEPERATOR_1) _T("%s"), szAppBurnVer, BurnDrvGetText(DRV_FULLNAME), pszTitle);

	//	SetWindowText(hScrnWnd, szText);
}

void NeoCDInfo_SetTitle()
{
	if (IsNeoGeoCD() && game && game->id) {
		SetNeoCDTitle(game->pszTitle);
	}
	else if (IsNeoGeoCD()) {
		SetNeoCDTitle("Unknown CD");
	}
}

// Get the title
int GetNeoCDTitle(unsigned int nGameID)
{
	game = (NGCDGAME*)malloc(sizeof(NGCDGAME));
	memset(game, 0, sizeof(NGCDGAME));

	if (GetNeoGeoCDInfo(nGameID)) {
		memcpy(game, GetNeoGeoCDInfo(nGameID), sizeof(NGCDGAME));

		bprintf(PRINT_NORMAL, _T("    Title: %s \n"), game->pszTitle);
		bprintf(PRINT_NORMAL, _T("    Shortname: %s \n"), game->pszName);
		bprintf(PRINT_NORMAL, _T("    Year: %s \n"), game->pszYear);
		bprintf(PRINT_NORMAL, _T("    Company: %s \n"), game->pszCompany);

		// Update the main window title
		//SetNeoCDTitle(game->pszTitle);

		return 1;
	}
	else {
		//SetNeoCDTitle(FBALoadStringEx(hAppInst, IDS_UNIDENTIFIED_CD, true));
	}

	return 0;
}

void iso9660_ReadOffset(UINT8* Dest, FILE* fp, unsigned int lOffset, unsigned int lSize, unsigned int lLength)
{
	if (fp == NULL || Dest == NULL) return;

	fseek(fp, lOffset + ((nSectorLength == 2352) ? 16 : 0), SEEK_SET);
	fread(Dest, lLength, lSize, fp);
}

static void NeoCDList_iso9660_CheckDirRecord(FILE* fp, int nSector)
{
	unsigned int	lBytesRead = 0;
	unsigned int	lOffset = 0;
	bool	bNewSector = false;
	bool	bRevisionQueve = false;
	int		nRevisionQueveID = 0;

	lOffset = (nSector * nSectorLength);

	UINT8 nLenDR;
	UINT8 Flags;
	UINT8 LEN_FI;
	UINT8* ExtentLoc = (UINT8*)malloc(8 + 32);
	UINT8* Data = (UINT8*)malloc(0x10b + 32);
	char* File = (char*)malloc(32 + 32);

	while (1)
	{
		iso9660_ReadOffset(&nLenDR, fp, lOffset, 1, sizeof(UINT8));

		if (nLenDR == 0x22) {
			lOffset += nLenDR;
			lBytesRead += nLenDR;
			continue;
		}

		if (nLenDR < 0x22)
		{
			if (bNewSector)
			{
				if (bRevisionQueve) {
					bRevisionQueve = false;
					GetNeoCDTitle(nRevisionQueveID);
				}
				return;
			}

			nLenDR = 0;
			iso9660_ReadOffset(&nLenDR, fp, lOffset + 1, 1, sizeof(UINT8));

			if (nLenDR < 0x22) {
				lOffset += (nSectorLength - lBytesRead);
				lBytesRead = 0;
				bNewSector = true;
				continue;
			}
		}

		bNewSector = false;

		iso9660_ReadOffset(&Flags, fp, lOffset + 25, 1, sizeof(UINT8));

		if (!(Flags & (1 << 1)))
		{
			iso9660_ReadOffset(ExtentLoc, fp, lOffset + 2, 8, sizeof(UINT8));

			char szValue[32];
			sprintf(szValue, "%02x%02x%02x%02x", ExtentLoc[4], ExtentLoc[5], ExtentLoc[6], ExtentLoc[7]);

			unsigned int nValue = 0;
			sscanf(szValue, "%x", &nValue);

			iso9660_ReadOffset(Data, fp, nValue * nSectorLength, 0x10a, sizeof(UINT8));

			char szData[32];
			sprintf(szData, "%c%c%c%c%c%c%c", Data[0x100], Data[0x101], Data[0x102], Data[0x103], Data[0x104], Data[0x105], Data[0x106]);

			if (!strncmp(szData, "NEO-GEO", 7))
			{
				char id[32];
				sprintf(id, "%02X%02X", Data[0x108], Data[0x109]);

				unsigned int nID = 0;
				sscanf(id, "%x", &nID);

				iso9660_ReadOffset(&LEN_FI, fp, lOffset + 32, 1, sizeof(UINT8));

				iso9660_ReadOffset((UINT8*)File, fp, lOffset + 33, LEN_FI, sizeof(UINT8));
				strncpy(File, File, LEN_FI);
				File[LEN_FI] = 0;

				// King of Fighters '94, The (1994)(SNK)(JP)
				// 10-6-1994 (P1.PRG)
				if (nID == 0x0055 && Data[0x67] == 0xDE) {
					// ...continue
				}

				// King of Fighters '94, The (1994)(SNK)(JP-US)
				// 11-21-1994 (P1.PRG)
				if (nID == 0x0055 && Data[0x67] == 0xE6) {
					// Change to custom revision id
					nID = 0x1055;
				}

				// King of Fighters '95, The (1995)(SNK)(JP-US)[!][NGCD-084 MT B01, B03-B06, NGCD-084E MT B01]
				// 9-11-1995 (P1.PRG)
				if (nID == 0x0084 && Data[0x6C] == 0xC0) {
					// ... continue
				}

				// King of Fighters '95, The (1995)(SNK)(JP-US)[!][NGCD-084 MT B10, NGCD-084E MT B03]
				// 10-5-1995 (P1.PRG)
				if (nID == 0x0084 && Data[0x6C] == 0xFF) {
					// Change to custom revision id
					nID = 0x1084;
				}

				// King of Fighters '96 NEOGEO Collection, The
				if (nID == 0x0229) {
					bRevisionQueve = false;

					GetNeoCDTitle(nID);
					break;
				}

				// King of Fighters '96, The
				if (nID == 0x0214) {
					bRevisionQueve = true;
					nRevisionQueveID = nID;
					lOffset += nLenDR;
					lBytesRead += nLenDR;
					continue;
				}

				GetNeoCDTitle(nID);
				break;
			}
		}

		lOffset += nLenDR;
		lBytesRead += nLenDR;
	}

	if (ExtentLoc) {
		free(ExtentLoc);
		ExtentLoc = NULL;
	}

	if (Data) {
		free(Data);
		Data = NULL;
	}

	if (File) {
		free(File);
		File = NULL;
	}
}

// Check the specified ISO, and proceed accordingly
static int NeoCDList_CheckISO(TCHAR* pszFile)
{
	if (!pszFile) {
		// error
		return 0;
	}

	// Make sure we have a valid ISO file extension...
	if (IsFileExt(pszFile, _T(".img")) || IsFileExt(pszFile, _T(".bin")))
	{
		FILE* fp = _tfopen(pszFile, _T("rb"));
		if (fp)
		{
			// Read ISO and look for 68K ROM standard program header, ID is always there
			// Note: This function works very quick, doesn't compromise performance :)
			// it just read each sector first 264 bytes aproximately only.

			// Get ISO Size (bytes)
			fseek(fp, 0, SEEK_END);
			unsigned int lSize = 0;
			lSize = ftell(fp);
			fseek(fp, 0, SEEK_SET);

			// If it has at least 16 sectors proceed
			if (lSize > (nSectorLength * 16))
			{
				// Check for Standard ISO9660 Identifier
				UINT8 IsoCheck[32];

				// advance to sector 16 and PVD Field 2
				iso9660_ReadOffset(&IsoCheck[0], fp, nSectorLength * 16 + 1, 1, 5); // get Standard Identifier Field from PVD

				// Verify that we have indeed a valid ISO9660 MODE1/2352
				if (!memcmp(IsoCheck, "CD001", 5))
				{
					//bprintf(PRINT_NORMAL, _T("    Standard ISO9660 Identifier Found. \n"));
					iso9660_VDH vdh;

					// Get Volume Descriptor Header
					memset(&vdh, 0, sizeof(vdh));
					//memcpy(&vdh, iso9660_ReadOffset(fp, (2048 * 16), sizeof(vdh)), sizeof(vdh));
					iso9660_ReadOffset((UINT8*)&vdh, fp, 16 * nSectorLength, 1, sizeof(vdh));

					// Check for a valid Volume Descriptor Type
					if (vdh.vdtype == 0x01)
					{
#if 0
						// This will fail on 64-bit due to differing variable sizes in the pvd struct
												// Get Primary Volume Descriptor
						iso9660_PVD pvd;
						memset(&pvd, 0, sizeof(pvd));
						//memcpy(&pvd, iso9660_ReadOffset(fp, (2048 * 16), sizeof(pvd)), sizeof(pvd));
						iso9660_ReadOffset((UINT8*)&pvd, fp, nSectorLength * 16, 1, sizeof(pvd));

						// ROOT DIRECTORY RECORD

						// Handle Path Table Location
						char szRootSector[32];
						unsigned int nRootSector = 0;

						sprintf(szRootSector, "%02X%02X%02X%02X",
							pvd.root_directory_record.location_of_extent[4],
							pvd.root_directory_record.location_of_extent[5],
							pvd.root_directory_record.location_of_extent[6],
							pvd.root_directory_record.location_of_extent[7]);

						// Convert HEX string to Decimal
						sscanf(szRootSector, "%X", &nRootSector);
#else
						// Just read from the file directly at the correct offset (SECTOR_SIZE * 16 + 0x9e for the start of the root directory record)
						UINT8 buffer[32];
						char szRootSector[32];
						unsigned int nRootSector = 0;

						iso9660_ReadOffset(&buffer[0], fp, nSectorLength * 16 + 0x9e, 1, 8);

						sprintf(szRootSector, "%02x%02x%02x%02x", buffer[4], buffer[5], buffer[6], buffer[7]);

						sscanf(szRootSector, "%x", &nRootSector);
#endif

						// Process the Root Directory Records
						NeoCDList_iso9660_CheckDirRecord(fp, nRootSector);

						// Path Table Records are not processed, since NeoGeo CD should not have subdirectories
						// ...
					}
				}
				else {

					//bprintf(PRINT_NORMAL, _T("    Standard ISO9660 Identifier Not Found, cannot continue. \n"));
					return 0;
				}
			}
		}
		else {

			//bprintf(PRINT_NORMAL, _T("    Couldn't open %s \n"), GetIsoPath());
			return 0;
		}

		if (fp) fclose(fp);

	}
	else {

		//bprintf(PRINT_NORMAL, _T("    File doesn't have a valid ISO extension [ .iso / .ISO ] \n"));
		return 0;
	}

	return 1;
}

// This will do everything
int GetNeoGeoCD_Identifier()
{
	if (!GetIsoPath() || !IsNeoGeoCD()) {
		return 0;
	}

	// Make sure we have a valid ISO file extension...
	if (IsFileExt(GetIsoPath(), _T(".img")) || IsFileExt(GetIsoPath(), _T(".bin")))
	{
		if (_tfopen(GetIsoPath(), _T("rb")))
		{
			bprintf(0, _T("NeoCDList: checking %s\n"), GetIsoPath());
			// Read ISO and look for 68K ROM standard program header, ID is always there
			// Note: This function works very quick, doesn't compromise performance :)
			// it just read each sector first 264 bytes aproximately only.
			NeoCDList_CheckISO(GetIsoPath());

		}
		else {

			bprintf(PRINT_NORMAL, _T("    Couldn't open %s \n"), GetIsoPath());
			return 0;
		}

	}
	else {

		bprintf(PRINT_NORMAL, _T("    File doesn't have a valid ISO extension [ .img / .bin ] \n"));
		return 0;
	}

	return 1;
}

int NeoCDInfo_Init()
{
	NeoCDInfo_Exit();
	return GetNeoGeoCD_Identifier();
}

TCHAR* NeoCDInfo_Text(int nText)
{
	if (!game || !IsNeoGeoCD()) return NULL;

	switch (nText)
	{
	case DRV_NAME:			return game->pszName;
	case DRV_FULLNAME:		return game->pszTitle;
	case DRV_MANUFACTURER:	return game->pszCompany;
	case DRV_DATE:			return game->pszYear;
	}

	return NULL;
}

int NeoCDInfo_ID()
{
	if (!game || !IsNeoGeoCD()) return 0;
	return game->id;
}

void NeoCDInfo_Exit()
{
	if (game) {
		free(game);
		game = NULL;
	}
}
