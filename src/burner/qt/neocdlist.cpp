// ---------------------------------------------------------------------------------------
// NeoGeo CD Game Info Module (by CaptainCPS-X)
// ---------------------------------------------------------------------------------------
#include "burner.h"
#include "../neocdlist.h"
#include "../neocdlist_games.h"

NGCDGAME* GetNeoGeoCDInfo(unsigned int nID)
{
	for(unsigned int nGame = 0; nGame < (sizeof(games) / sizeof(NGCDGAME)); nGame++) {
		if(nID == games[nGame].id ) {
			return &games[nGame];
		}
	}

	return NULL;
}

NGCDGAME* game;

// Get the title 
int GetNeoCDTitle(unsigned int nGameID) 
{
	game = (NGCDGAME*)malloc(sizeof(NGCDGAME));
	memset(game, 0, sizeof(NGCDGAME));
	
	if(GetNeoGeoCDInfo(nGameID))
	{		
		memcpy(game, GetNeoGeoCDInfo(nGameID), sizeof(NGCDGAME));
		return 1;
	}

	game = NULL;
	return 0;
}

void iso9660_ReadOffset(unsigned char *Dest, FILE* fp, unsigned int lOffset, unsigned int lSize, unsigned int lLength)
{
	if(fp == NULL) return;
	if (Dest == NULL) return;
	
	fseek(fp, lOffset, SEEK_SET);
	fread(Dest, lLength, lSize, fp);
}

static void NeoCDList_iso9660_CheckDirRecord(FILE* fp, int nSector) 
{
	int		nSectorLength		= 2048;	
	//int		nFile				= 0;
	unsigned int	lBytesRead			= 0;
	//int		nLen				= 0;
	unsigned int	lOffset				= 0;
	bool	bNewSector			= false;
	bool	bRevisionQueve		= false;
	int		nRevisionQueveID	= 0;	
	
	lOffset = (nSector * nSectorLength);
	
	unsigned char* nLenDR = (unsigned char*)malloc(1 * sizeof(unsigned char));
	unsigned char* Flags = (unsigned char*)malloc(1 * sizeof(unsigned char));
	unsigned char* ExtentLoc = (unsigned char*)malloc(8 * sizeof(unsigned char));
	unsigned char* Data = (unsigned char*)malloc(0x10b * sizeof(unsigned char));
	unsigned char* LEN_FI = (unsigned char*)malloc(1 * sizeof(unsigned char));
	char *File = (char*)malloc(32 * sizeof(char));

	while(1) 
	{
		iso9660_ReadOffset(nLenDR, fp, lOffset, 1, sizeof(unsigned char));
				
		if(nLenDR[0] == 0x22) {
			lOffset		+= nLenDR[0];
			lBytesRead	+= nLenDR[0];
			continue;
		}

		if(nLenDR[0] < 0x22) 
		{
			if(bNewSector) 
			{
				if(bRevisionQueve) {
					bRevisionQueve		= false;

					GetNeoCDTitle(nRevisionQueveID);
				}
				return;
			}

			nLenDR[0] = 0;
			iso9660_ReadOffset(nLenDR, fp, lOffset + 1, 1, sizeof(unsigned char));
			
			if(nLenDR[0] < 0x22) {
				lOffset += (2048 - lBytesRead);
				lBytesRead = 0;
				bNewSector = true;
				continue;
			}
		}

		bNewSector = false;
		
		iso9660_ReadOffset(Flags, fp, lOffset + 25, 1, sizeof(unsigned char));

		if(!(Flags[0] & (1 << 1))) 
		{
			iso9660_ReadOffset(ExtentLoc, fp, lOffset + 2, 8, sizeof(unsigned char));

			char szValue[9];
			sprintf(szValue, "%02x%02x%02x%02x", ExtentLoc[4], ExtentLoc[5], ExtentLoc[6], ExtentLoc[7]);

			unsigned int nValue = 0;
			sscanf(szValue, "%x", &nValue); 

			iso9660_ReadOffset(Data, fp, nValue * 2048, 0x10a, sizeof(unsigned char));

			char szData[8];
			sprintf(szData, "%c%c%c%c%c%c%c", Data[0x100], Data[0x101], Data[0x102], Data[0x103], Data[0x104], Data[0x105], Data[0x106]);

			if(!strncmp(szData, "NEO-GEO", 7)) 
			{
				char id[] = "0000";
				sprintf(id, "%02X%02X",  Data[0x108], Data[0x109]);

				unsigned int nID = 0;
				sscanf(id, "%x", &nID);

				iso9660_ReadOffset(LEN_FI, fp, lOffset + 32, 1, sizeof(unsigned char));

				iso9660_ReadOffset((unsigned char*)File, fp, lOffset + 33, LEN_FI[0], sizeof(char));
				strncpy(File, File, LEN_FI[0]);				
				File[LEN_FI[0]] = 0;

				// King of Fighters '94, The (1994)(SNK)(JP)
				// 10-6-1994 (P1.PRG)
				if(nID == 0x0055 && Data[0x67] == 0xDE) {
					// ...continue
				}
				
				// King of Fighters '94, The (1994)(SNK)(JP-US)
				// 11-21-1994 (P1.PRG)
				if(nID == 0x0055 && Data[0x67] == 0xE6) {
					// Change to custom revision id
					nID = 0x1055;
				}
				
				// King of Fighters '95, The (1995)(SNK)(JP-US)[!][NGCD-084 MT B01, B03-B06, NGCD-084E MT B01]
				// 9-11-1995 (P1.PRG)
				if(nID == 0x0084 && Data[0x6C] == 0xC0) {
					// ... continue
				}

				// King of Fighters '95, The (1995)(SNK)(JP-US)[!][NGCD-084 MT B10, NGCD-084E MT B03]
				// 10-5-1995 (P1.PRG)
				if(nID == 0x0084 && Data[0x6C] == 0xFF) {
					// Change to custom revision id
					nID = 0x1084;
				}

				// King of Fighters '96 NEOGEO Collection, The
				if(nID == 0x0229) {
					bRevisionQueve = false;

					GetNeoCDTitle(nID);
					break;
				}

				// King of Fighters '96, The
				if(nID == 0x0214) {
					bRevisionQueve		= true;
					nRevisionQueveID	= nID;
					lOffset		+= nLenDR[0];
					lBytesRead	+= nLenDR[0];
					continue;
				}

				GetNeoCDTitle(nID);

				break;
			}
		}	
		
		lOffset		+= nLenDR[0];
		lBytesRead	+= nLenDR[0];		
	}
	
	if (nLenDR) {
		free(nLenDR);
		nLenDR = NULL;
	}
	
	if (Flags) {
		free(Flags);
		Flags = NULL;
	}
	
	if (ExtentLoc) {
		free(ExtentLoc);
		ExtentLoc = NULL;
	}
	
	if (Data) {
		free(Data);
		Data = NULL;
	}
	
	if (LEN_FI) {
		free(LEN_FI);
		LEN_FI = NULL;
	}
	
	if (File) {
		free(File);
		File = NULL;
	}
}

// Check the specified ISO, and proceed accordingly
static int NeoCDList_CheckISO(TCHAR* pszFile)
{
	if(!pszFile) {
		// error
		return 0;
	}

	// Make sure we have a valid ISO file extension...
	if(_tcsstr(pszFile, _T(".iso")) || _tcsstr(pszFile, _T(".ISO")) ) 
	{
		FILE* fp = _tfopen(pszFile, _T("rb"));
		if(fp) 
		{
			// Read ISO and look for 68K ROM standard program header, ID is always there
			// Note: This function works very quick, doesn't compromise performance :)
			// it just read each sector first 264 bytes aproximately only.

			// Get ISO Size (bytes)
			fseek(fp, 0, SEEK_END);
			unsigned int lSize = 0;
			lSize = ftell(fp);
			//rewind(fp);
			fseek(fp, 0, SEEK_SET);

			// If it has at least 16 sectors proceed
			if(lSize > (2048 * 16)) 
			{	
				// Check for Standard ISO9660 Identifier
				unsigned char IsoHeader[2048 * 16 + 1];
				unsigned char IsoCheck[6];
		
				fread(IsoHeader, 1, 2048 * 16 + 1, fp); // advance to sector 16 and PVD Field 2
				fread(IsoCheck, 1, 5, fp);	// get Standard Identifier Field from PVD
				
				// Verify that we have indeed a valid ISO9660 MODE1/2048
				if(!memcmp(IsoCheck, "CD001", 5))
				{
					//bprintf(PRINT_NORMAL, _T("    Standard ISO9660 Identifier Found. \n"));
					iso9660_VDH vdh;

					// Get Volume Descriptor Header			
					memset(&vdh, 0, sizeof(vdh));
					//memcpy(&vdh, iso9660_ReadOffset(fp, (2048 * 16), sizeof(vdh)), sizeof(vdh));
					iso9660_ReadOffset((unsigned char*)&vdh, fp, 2048 * 16, 1, sizeof(vdh));

					// Check for a valid Volume Descriptor Type
					if(vdh.vdtype == 0x01) 
					{
#if 0
// This will fail on 64-bit due to differing variable sizes in the pvd struct
						// Get Primary Volume Descriptor
						iso9660_PVD pvd;
						memset(&pvd, 0, sizeof(pvd));
						//memcpy(&pvd, iso9660_ReadOffset(fp, (2048 * 16), sizeof(pvd)), sizeof(pvd));
						iso9660_ReadOffset((unsigned char*)&pvd, fp, 2048 * 16, 1, sizeof(pvd));

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
// Just read from the file directly at the correct offset (0x8000 + 0x9e for the start of the root directory record)
						unsigned char buffer[8];
						char szRootSector[4];
						unsigned int nRootSector = 0;
						
						fseek(fp, 0x809e, SEEK_SET);
						fread(buffer, 1, 8, fp);
						
						sprintf(szRootSector, "%02x%02x%02x%02x", buffer[4], buffer[5], buffer[6], buffer[7]);
						
						sscanf(szRootSector, "%x", &nRootSector);
#endif						
						
						// Process the Root Directory Records
						NeoCDList_iso9660_CheckDirRecord(fp, nRootSector);

						// Path Table Records are not processed, since NeoGeo CD should not have subdirectories
						// ...
					}
				} else {

					//bprintf(PRINT_NORMAL, _T("    Standard ISO9660 Identifier Not Found, cannot continue. \n"));
					return 0;
				}
			}
			fclose(fp);
		} else {

			//bprintf(PRINT_NORMAL, _T("    Couldn't open %s \n"), GetIsoPath());
			return 0;
		}
	} else {

		//bprintf(PRINT_NORMAL, _T("    File doesn't have a valid ISO extension [ .iso / .ISO ] \n"));
		return 0;
	}

	return 1;
}

// This will do everything
int GetNeoGeoCD_Identifier()
{
	if(!GetIsoPath() || !IsNeoGeoCD()) {
		return 0;
	}

	// Make sure we have a valid ISO file extension...
	if(_tcsstr(GetIsoPath(), _T(".iso")) || _tcsstr(GetIsoPath(), _T(".ISO")) ) 
	{
		if(_tfopen(GetIsoPath(), _T("rb"))) 
		{
			// Read ISO and look for 68K ROM standard program header, ID is always there
			// Note: This function works very quick, doesn't compromise performance :)
			// it just read each sector first 264 bytes aproximately only.
			NeoCDList_CheckISO(GetIsoPath());

		} else {

			bprintf(PRINT_NORMAL, _T("    Couldn't open %s \n"), GetIsoPath());
			return 0;
		}

	} else {

		bprintf(PRINT_NORMAL, _T("    File doesn't have a valid ISO extension [ .iso / .ISO ] \n"));
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
	if(!game || !IsNeoGeoCD() || !bDrvOkay) return NULL;

	switch(nText) 
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
	if(!game || !IsNeoGeoCD() || !bDrvOkay) return 0;
	return game->id;
}

void NeoCDInfo_Exit() 
{
	if(game) {
		free(game);
		game = NULL;
	}
}
