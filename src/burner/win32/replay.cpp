// Functions for recording & replaying input
#include "burner.h"
#include <commdlg.h>
#include "inputbuf.h"
#include "neocdlist.h"

#include <io.h>

#ifndef W_OK
#define W_OK 4
#endif

#define MAX_METADATA 1024
wchar_t wszMetadata[MAX_METADATA];
wchar_t wszStartupGame[MAX_PATH];
wchar_t wszAuthorInfo[MAX_METADATA-64];

INT32 nReplayStatus = 0; // 1 record, 2 replay, 0 nothing
bool bReplayStartPaused = false;
bool bReplayReadOnly = false;
bool bReplayShowMovement = false;
bool bReplayDontClose = false;
INT32 nReplayUndoCount = 0;
bool bReplayFrameCounterDisplay = 1;
TCHAR szFilter[1024];
INT32 movieFlags = 0;
bool bStartFromReset = true;
bool bWithNVRAM = false;
extern bool bWithEEPROM;
TCHAR szCurrentMovieFilename[MAX_PATH] = _T("");
UINT32 nTotalFrames = 0;
UINT32 nReplayCurrentFrame;

// If a driver has external data that needs to be recorded every frame.
INT32 nReplayExternalDataCount = 0;
UINT8 *ReplayExternalData = NULL;

#define MOVIE_FLAG_FROM_POWERON			(1<<1)
#define MOVIE_FLAG_WITH_NVRAM			(1<<2) // to be used w/FROM_POWERON (if selected)

const UINT32 nMovieVersion = 0x0404;
const UINT32 nMinimumMovieVersion = 0x0404; // anything lower will not work w/this version of FB Neo
UINT32 nThisMovieVersion = 0;
UINT32 nThisFBVersion = 0;

UINT32 nStartFrame = 0;
static UINT32 nEndFrame;

static FILE* fp = NULL;
static INT32 nSizeOffset;

static short nPrevInputs[0x0100];

static INT32 nPrintInputsActive[2] = { 0, 0 };

struct MovieExtInfo
{
	// date & time
	UINT32 year, month;
	UINT16 day, dayofweek;
	UINT32 hour, minute, second;
};

struct MovieExtInfo MovieInfo = { 0, 0, 0, 0, 0, 0, 0 };

static char *ReplayDecodeDateTime()
{
	static char ttime[64];
	int fixhour = (MovieInfo.hour>12) ? MovieInfo.hour-12 : MovieInfo.hour;
	if (fixhour == 0) fixhour = 12;
	sprintf(ttime, "%02d/%02d/%04d @ %02d:%02d:%02d%s", MovieInfo.month+1, MovieInfo.day, 2000 + (MovieInfo.year%100), fixhour, MovieInfo.minute, MovieInfo.second, (MovieInfo.hour>12) ? "pm" : "am");

	return &ttime[0];
}

static INT32 ReplayDialog();
static INT32 RecordDialog();

INT32 RecordInput()
{
	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));

	for (UINT32 i = 0; i < nGameInpCount; i++) {
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal) {
			if (bii.nType & BIT_GROUP_ANALOG) {
				if (*bii.pShortVal != nPrevInputs[i]) {
					inputbuf_addbuffer(i);
					inputbuf_addbuffer(*bii.pShortVal >> 8);
					inputbuf_addbuffer(*bii.pShortVal & 0xFF);
					nPrevInputs[i] = *bii.pShortVal;
				}
			} else {
				if (*bii.pVal != nPrevInputs[i]) {
					inputbuf_addbuffer(i);
					inputbuf_addbuffer(*bii.pVal);
					nPrevInputs[i] = *bii.pVal;
				}
			}
		}
	}
	inputbuf_addbuffer(0xFF);

	if (nReplayExternalDataCount && ReplayExternalData) {
		for (INT32 i = 0; i < nReplayExternalDataCount; i++) {
			inputbuf_addbuffer(ReplayExternalData[i]);
		}
	}

	if (bReplayFrameCounterDisplay) {
		wchar_t framestring[15];
		swprintf(framestring, L"%d", GetCurrentFrame() - nStartFrame);
		VidSNewTinyMsg(framestring);
	}

	return 0;
}

static void CheckRedraw()
{
	if (bRunPause) {
		VidRedraw();                                // Redraw screen so status doesn't get clobbered
		VidPaint(0);                                // ""
	}
}

static void PrintInputsReset()
{
	nPrintInputsActive[0] = nPrintInputsActive[1] = -(60 * 5 * 2);
}

static inline void PrintInputsSetActive(UINT8 plyrnum)
{
	nPrintInputsActive[plyrnum] = GetCurrentFrame();
}

static void PrintInputs()
{
	struct BurnInputInfo bii;

	UINT8 UDLR[2][4]; // P1 & P2 joystick movements
	UINT8 BUTTONS[2][8]; // P1 and P2 buttons
	UINT8 OFFBUTTONS[2][8]; // P1 and P2 buttons
	wchar_t lines[3][64];

	memset(&lines, 0, sizeof(lines));
	memset(UDLR, 0, sizeof(UDLR));
	memset(BUTTONS, ' ', sizeof(BUTTONS));
	memset(OFFBUTTONS, ' ', sizeof(OFFBUTTONS));

	for (UINT32 i = 0; i < nGameInpCount; i++) {
		memset(&bii, 0, sizeof(bii));
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal && bii.szInfo && bii.szInfo[0]) {
			// Translate X/Y axis to UDLR, TODO: (maybe) support mouse axis/buttons
			if ((bii.nType & BIT_GROUP_ANALOG) && bii.pShortVal && *bii.pShortVal) {
				if (stricmp(bii.szInfo+2, " x-axis")==0 && bii.nType == BIT_GROUP_ANALOG) {
					if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
						if ((INT16)*bii.pShortVal > 0x80)
							UDLR[(bii.szInfo[1] - '1')][3] = 1;  // Right
						if ((INT16)*bii.pShortVal < -0x80)
							UDLR[(bii.szInfo[1] - '1')][2] = 1;  // Left
						PrintInputsSetActive(bii.szInfo[1] - '1');
					}
				}
				if (stricmp(bii.szInfo+2, " y-axis")==0 && bii.nType == BIT_GROUP_ANALOG) {
					if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
						if ((INT16)*bii.pShortVal > 0x80)
							UDLR[(bii.szInfo[1] - '1')][1] = 1;  // Down
						if ((INT16)*bii.pShortVal < -0x80)
							UDLR[(bii.szInfo[1] - '1')][0] = 1;  // Up
						PrintInputsSetActive(bii.szInfo[1] - '1');
					}
				}
			}
			if (*bii.pVal) { // Button pressed
				if (stricmp(bii.szInfo+2, " Up")==0) {
					if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
						UDLR[(bii.szInfo[1] - '1')][0] = 1;
						PrintInputsSetActive(bii.szInfo[1] - '1');
					}
				}
				if (stricmp(bii.szInfo+2, " Down")==0) {
					if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
						UDLR[(bii.szInfo[1] - '1')][1] = 1;
						PrintInputsSetActive(bii.szInfo[1] - '1');
					}
				}
				if (stricmp(bii.szInfo+2, " Left")==0) {
					if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
						UDLR[(bii.szInfo[1] - '1')][2] = 1;
						PrintInputsSetActive(bii.szInfo[1] - '1');
					}
				}
				if (stricmp(bii.szInfo+2, " Right")==0) {
					if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2')  {
						UDLR[(bii.szInfo[1] - '1')][3] = 1;
						PrintInputsSetActive(bii.szInfo[1] - '1');
					}
				}
				if (strnicmp(bii.szInfo+2, " fire ", 6)==0) {
					if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
						if (bii.szInfo[8] - '1' < 6) { // avoid overflow
							BUTTONS[(bii.szInfo[1] - '1')][bii.szInfo[8] - '1'] = bii.szInfo[8];
							PrintInputsSetActive(bii.szInfo[1] - '1');
						}
					}
				}
			} else { // get "off" buttons
				if (strnicmp(bii.szInfo+2, " fire ", 6)==0) {
					if (bii.szInfo[1] == '1' || bii.szInfo[1] == '2') {
						if (bii.szInfo[8] - '1' < 6) // avoid overflow
							OFFBUTTONS[(bii.szInfo[1] - '1')][bii.szInfo[8] - '1'] = bii.szInfo[8];
					}
				}
			}
		}
	}

	VidSNewJoystickMsg(NULL); // Clear surface.

	// Draw shadows
	int got_shadow = 0;
	for (INT32 player = 0; player < 2; player++) {
		if (GetCurrentFrame() < nPrintInputsActive[player] + (60*5)) {  // time out np2active after 300 frames or so...
			INT32 nLen = (player == 1) ? _tcslen(lines[0]) : 0;
			swprintf(lines[0] + nLen, L"  ^  %c%c%c  ", OFFBUTTONS[player][0], OFFBUTTONS[player][1], OFFBUTTONS[player][2]);
			swprintf(lines[1] + nLen, L" < >      ");
			swprintf(lines[2] + nLen, L"  v  %c%c%c  ", OFFBUTTONS[player][3], OFFBUTTONS[player][4], OFFBUTTONS[player][5]);
			got_shadow |= 1 << (4 + player);
		}
	}

	if (got_shadow) {
		VidSNewJoystickMsg(lines[0], 0x404040, 20, 0 | 4 | got_shadow); // got_shadow encodes player# active
		VidSNewJoystickMsg(lines[1], 0x404040, 20, 1 | 4 | got_shadow);
		VidSNewJoystickMsg(lines[2], 0x404040, 20, 2 | 4 | got_shadow);
	}

	// Draw active buttons
	INT32 nLen = 0;
	for (INT32 player = 0; player < 2; player++) {
		if (player == 1) nLen = _tcslen(lines[0]); // Create the textual mini-joystick icons
		swprintf(lines[0] + nLen, L"  %c  %c%c%c  ", UDLR[player][0] ? '^' : ' ', BUTTONS[player][0], BUTTONS[player][1], BUTTONS[player][2]);
		swprintf(lines[1] + nLen, L" %c %c      ", UDLR[player][2] ? '<' : ' ', UDLR[player][3] ? '>' : ' ');
		swprintf(lines[2] + nLen, L"  %c  %c%c%c  ", UDLR[player][1] ? 'v' : ' ', BUTTONS[player][3], BUTTONS[player][4], BUTTONS[player][5]);
	}

	if (got_shadow) {
		VidSNewJoystickMsg(lines[0], 0xffffff, 20, 0 | got_shadow); // Draw them
		VidSNewJoystickMsg(lines[1], 0xffffff, 20, 1 | got_shadow);
		VidSNewJoystickMsg(lines[2], 0xffffff, 20, 2 | got_shadow);
	}
}

static void DisplayPlayingFrame()
{
	wchar_t framestring[32];
	swprintf(framestring, L"%d / %d", GetCurrentFrame() - nStartFrame,nTotalFrames);
	VidSNewTinyMsg(framestring);
}

INT32 ReplayInput()
{
	UINT8 n;
	struct BurnInputInfo bii;
	memset(&bii, 0, sizeof(bii));

	if (inputbuf_eof()) {
		bprintf(0, _T("eof at beginning of ReplayInput()!\n"));
		StopReplay();
		return 1;
	}

	// Just to be safe, restore the inputs to the known correct settings
	for (UINT32 i = 0; i < nGameInpCount; i++) {
		BurnDrvGetInputInfo(&bii, i);
		if (bii.pVal) {
			if (bii.nType & BIT_GROUP_ANALOG) {
				*bii.pShortVal = nPrevInputs[i];
			} else {
				*bii.pVal = nPrevInputs[i];
			}
		}
	}

	// Now read all inputs that need to change from the .fr file
	while ((n = inputbuf_getbuffer()) != 0xFF) {
		BurnDrvGetInputInfo(&bii, n);
		if (bii.pVal) {
			if (bii.nType & BIT_GROUP_ANALOG) {
				*bii.pShortVal = nPrevInputs[n] = (inputbuf_getbuffer() << 8) | inputbuf_getbuffer();
			} else {
				*bii.pVal = nPrevInputs[n] = inputbuf_getbuffer();
			}
		} else {
			inputbuf_getbuffer();
		}
	}

	if (nReplayExternalDataCount && ReplayExternalData) {
		for (INT32 i = 0; i < nReplayExternalDataCount; i++) {
			ReplayExternalData[i] = inputbuf_getbuffer();
		}
	}

	if (bReplayFrameCounterDisplay) {
		DisplayPlayingFrame();
	}

	if (bReplayShowMovement) {
		PrintInputs();
	}

	if ( (GetCurrentFrame()-nStartFrame) == (nTotalFrames-1) ) {
		bprintf(0, _T("*** Replay: pausing before end of video!  Input-eof?: %x\n"), inputbuf_eof());
		SetPauseMode(1);
	}

	if (inputbuf_eof()) {
		StopReplay();
		return 1;
	} else {
		return 0;
	}
}

static void MakeOfn(TCHAR* pszFilter)
{
	_stprintf(pszFilter, FBALoadStringEx(hAppInst, IDS_DISK_FILE_REPLAY, true), _T(APP_TITLE));
	memcpy(pszFilter + _tcslen(pszFilter), _T(" (*.fr)\0*.fr\0\0"), 14 * sizeof(TCHAR));

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hScrnWnd;
	ofn.lpstrFilter = pszFilter;
	ofn.lpstrFile = szChoice;
	ofn.nMaxFile = sizeof(szChoice) / sizeof(TCHAR);
	ofn.lpstrInitialDir = _T(".\\recordings");
	ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = _T("fr");

	return;
}

INT32 StartRecord()
{
	INT32 nRet;
	INT32 bOldPause;

	fp = NULL;
	movieFlags = 0;

	bOldPause = bRunPause;
	bRunPause = 1;
	nRet = RecordDialog();
	bRunPause = bOldPause;

	if (nRet == 0) {
		return 1;
	}

	bReplayReadOnly = false;
	bReplayShowMovement = false;

	{
		// Get date/time -before- starting game.
		memset(&MovieInfo, 0, sizeof(MovieInfo));
		time_t nLocalTime = time(NULL);
		tm* tmLocalTime = localtime(&nLocalTime);

		MovieInfo.hour		= tmLocalTime->tm_hour;
		MovieInfo.minute	= tmLocalTime->tm_min;
		MovieInfo.second	= tmLocalTime->tm_sec;
		MovieInfo.month		= tmLocalTime->tm_mon;
		MovieInfo.day		= tmLocalTime->tm_mday;
		MovieInfo.dayofweek	= tmLocalTime->tm_wday;
		MovieInfo.year		= tmLocalTime->tm_year;
	}

	if (bStartFromReset) {
		movieFlags |= MOVIE_FLAG_FROM_POWERON;
		if (bWithNVRAM) { // only applies w/FROM_POWERON
			movieFlags |= MOVIE_FLAG_WITH_NVRAM;
		}

		if(!StartFromReset(NULL, bWithNVRAM)) {
			bprintf(0, _T("*** Replay(record): error starting game.\n"));
			movieFlags = 0;
			return 1;
		}
	} else {
		// If we're starting a recording from savestate, clear rewind buffer.
		// This makes it impossible to rewind to a time before the recording
		// started, which will totally break the recording.
		StateRewindReset();
	}

	{
		const char szFileHeader[] = "FB1 ";				// File identifier
		fp = _tfopen(szChoice, _T("w+b"));
		_tcscpy(szCurrentMovieFilename, szChoice);

		nRet = 0;
		if (fp == NULL) {
			nRet = 1;
		} else {
			fwrite(&szFileHeader, 1, 4, fp);
			INT32 flags_offset = ftell(fp);
			fwrite(&movieFlags, 1, 4, fp);
			if (bStartFromReset) {
				if (movieFlags & MOVIE_FLAG_WITH_NVRAM) {
					bWithEEPROM = true;
					nRet = BurnStateSaveEmbed(fp, -1, 0); // Embed contents of NVRAM
					bWithEEPROM = false;
					if (nRet == -1) {
						// game doesn't have NVRAM
						fseek(fp, flags_offset, SEEK_SET);
						movieFlags &= ~MOVIE_FLAG_WITH_NVRAM; // remove NVRAM flag.
						fwrite(&movieFlags, 1, 4, fp);
					}
					nRet = 1;
				} else {
					nRet = 1; // don't embed nvram, but say everything's OK
				}
			}
			else
			{
				bWithEEPROM = true;
				nRet = BurnStateSaveEmbed(fp, -1, 1);	// Embed Save-State + NVRAM
				bWithEEPROM = false;
			}
			if (nRet >= 0) {
				const char szChunkHeader[] = "FR1 ";	// Chunk identifier
				INT32 nZero = 0;

				fwrite(&szChunkHeader, 1, 4, fp);		// Write chunk identifier

				nSizeOffset = ftell(fp);

				fwrite(&nZero, 1, 4, fp);				// reserve space for chunk size

				fwrite(&nZero, 1, 4, fp);				// reserve space for number of frames

				fwrite(&nZero, 1, 4, fp);				// undo count
				fwrite(&nMovieVersion, 1, 4, fp);		// ThisMovieVersion

				fwrite(&MovieInfo, 1, sizeof(MovieInfo), fp); // date structure (starting point for timer chips)

				fwrite(&nBurnVer, 1, 4, fp);			// fb version#

				INT32 derp = ftell(fp);
				bprintf(0, _T("embedding inputbuf at %d\n"), derp);

				nRet = inputbuf_embed(fp);
			}
		}
	}

	if (nRet) {
		if (fp) {
			fclose(fp);
			fp = NULL;
		}

		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_CREATE));
		FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_REPLAY));
		FBAPopupDisplay(PUF_TYPE_ERROR);
		movieFlags = 0;

		return 1;
	} else {
		struct BurnInputInfo bii;
		memset(&bii, 0, sizeof(bii));

		nReplayStatus = 1;								// Set record status
		CheckRedraw();
		MenuEnableItems();

		nStartFrame = GetCurrentFrame();
		nReplayUndoCount = 0;

		// Create a baseline so we can just record the deltas afterwards
		for (UINT32 i = 0; i < nGameInpCount; i++) {
			BurnDrvGetInputInfo(&bii, i);
			if (bii.pVal) {
				if (bii.nType & BIT_GROUP_ANALOG) {
					inputbuf_addbuffer(*bii.pShortVal >> 8);
					inputbuf_addbuffer(*bii.pShortVal & 0xFF);
					nPrevInputs[i] = *bii.pShortVal;
				} else {
					inputbuf_addbuffer(*bii.pVal);
					nPrevInputs[i] = *bii.pVal;
				}
			} else {
				inputbuf_addbuffer(0);
			}
		}

#ifdef FBNEO_DEBUG
		dprintf(_T("*** Recording of file %s started.\n"), szChoice);
#endif

		return 0;
	}
}

INT32 StartReplay(const TCHAR* szFileName)					// const char* szFileName = NULL
{
	INT32 nRet;
	INT32 bOldPause;

	PrintInputsReset();

	fp = NULL;

	if (szFileName) {
		_tcscpy(szChoice, szFileName);
		if (!bReplayDontClose) {
			// if bStartFromReset, get file "wszStartupGame" from metadata!!
			DisplayReplayProperties(0, false);
		}
	} else {
		bOldPause = bRunPause;
		bRunPause = 1;
		nRet = ReplayDialog();
		bRunPause = bOldPause;

		if (nRet == 0) {
			return 1;
		}

	}
	_tcscpy(szCurrentMovieFilename, szChoice);

	// init metadata
	wszMetadata[0] = L'\0';
	{
		const char szFileHeader[] = "FB1 ";					// File identifier
		char ReadHeader[] = "    ";
		fp = _tfopen(szChoice, _T("r+b"));
		if (!fp) return 1;
		memset(ReadHeader, 0, 4);
		fread(ReadHeader, 1, 4, fp);						// Read identifier
		if (memcmp(ReadHeader, szFileHeader, 4)) {			// Not the right file type
			fclose(fp);
			fp = NULL;
			nRet = 2;
		} else {
			memset(ReadHeader, 0, 4);
			fread(&movieFlags, 1, 4, fp); // Read movie flags
			if (movieFlags&MOVIE_FLAG_FROM_POWERON) { // Starts from reset
				bStartFromReset = 1;
				if (!bReplayDontClose)
					if(!StartFromReset(wszStartupGame, (movieFlags & MOVIE_FLAG_WITH_NVRAM))) {
						bprintf(0, _T("*** Replay(playback): error starting game.\n"));
						movieFlags = 0;
						return 0;
					}
				if (movieFlags & MOVIE_FLAG_WITH_NVRAM) { // Get NVRAM
					bWithEEPROM = true;
					nRet = BurnStateLoadEmbed(fp, -1, 0, &DrvInitCallback);
					bWithEEPROM = false;
				} else {
					nRet = 0;
				}
			}
			else {// Load the savestate associated with the recording
				bStartFromReset = 0;
				bWithEEPROM = true;
				nRet = BurnStateLoadEmbed(fp, -1, 1, &DrvInitCallback);
				bWithEEPROM = false;
			}
			if (nRet == 0) {
				const char szChunkHeader[] = "FR1 ";		// Chunk identifier
				fread(ReadHeader, 1, 4, fp);				// Read identifier
				if (memcmp(ReadHeader, szChunkHeader, 4)) {	// Not the right file type
					fclose(fp);
					fp = NULL;
					nRet = 2;
				} else {
					INT32 nChunkSize = 0;
					// Open the recording itself
					nSizeOffset = ftell(fp);				// Save chunk size offset in case the file is re-recorded
					fread(&nChunkSize, 1, 0x04, fp);		// Read chunk size
					INT32 nChunkPosition = ftell(fp);			// For seeking to the metadata
					fread(&nEndFrame, 1, 4, fp);			// Read framecount
					nTotalFrames = nEndFrame;

					nStartFrame = GetCurrentFrame();
					bReplayDontClose = 0; // we don't need it anymore from this point
					nEndFrame += nStartFrame;
					fread(&nReplayUndoCount, 1, 4, fp);
					fread(&nThisMovieVersion, 1, 4, fp);

					memset(&MovieInfo, 0, sizeof(MovieInfo));

					fread(&MovieInfo, 1, sizeof(MovieInfo), fp);
					bprintf(0, _T("Ext Info: %S\n"), ReplayDecodeDateTime());

					fread(&nThisFBVersion, 1, 4, fp);
					INT32 nEmbedPosition = ftell(fp);

					// Read metadata
					const char szMetadataHeader[] = "FRM1";
					fseek(fp, nChunkPosition + nChunkSize, SEEK_SET);
					memset(ReadHeader, 0, 4);
					fread(ReadHeader, 1, 4, fp);
					if(memcmp(ReadHeader, szMetadataHeader, 4) == 0) {
						INT32 nMetaSize;
						fread(&nMetaSize, 1, 4, fp);
						INT32 nMetaLen = nMetaSize >> 1;
						if(nMetaLen >= MAX_METADATA) {
							nMetaLen = MAX_METADATA-1;
						}
						INT32 i;
						for(i=0; i<nMetaLen; ++i) {
							wchar_t c = 0;
							c |= fgetc(fp) & 0xff;
							c |= (fgetc(fp) & 0xff) << 8;
							wszMetadata[i] = c;
						}
						wszMetadata[i] = L'\0';
					}

					// Seek back to the beginning of inputbuf data
					fseek(fp, nEmbedPosition, SEEK_SET);
					nRet = inputbuf_embed(fp);
				}
			}
		}

		// Describe any possible errors:
		if (nRet == 3) {
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_THIS_REPLAY));
			FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_UNAVAIL));
		} else {
			if (nRet == 4) {
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_THIS_REPLAY));
				FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_TOOOLD), _T(APP_TITLE));
			} else {
				if (nRet == 5) {
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_THIS_REPLAY));
					FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_TOONEW), _T(APP_TITLE));
				} else {
					if (nRet) {
						FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_ERR_DISK_LOAD));
						FBAPopupAddText(PUF_TEXT_DEFAULT, MAKEINTRESOURCE(IDS_DISK_REPLAY));
					}
				}
			}
		}

		if (nRet) {
			if (fp) {
				fclose(fp);
				fp = NULL;
			}

			FBAPopupDisplay(PUF_TYPE_ERROR);
			movieFlags = 0;

			return 1;
		}
	}

	nReplayStatus = 2;							// Set replay status

	SetPauseMode(bReplayStartPaused);           // Start Paused?

	DisplayPlayingFrame();

	CheckRedraw();

	MenuEnableItems();

	{
		struct BurnInputInfo bii;
		memset(&bii, 0, sizeof(bii));

		inputbuf_load();

		// Get the baseline
		for (UINT32 i = 0; i < nGameInpCount; i++) {
			BurnDrvGetInputInfo(&bii, i);
			if (bii.pVal) {
				if (bii.nType & BIT_GROUP_ANALOG) {
					*bii.pShortVal = nPrevInputs[i] = (inputbuf_getbuffer() << 8) | inputbuf_getbuffer();

				} else {
					*bii.pVal = nPrevInputs[i] = inputbuf_getbuffer();
				}
			} else {
				inputbuf_getbuffer();
			}
		}
	}

#ifdef FBNEO_DEBUG
	dprintf(_T("*** Replay of file %s started.\n"), szChoice);
#endif

	return 0;
}

static void CloseRecord()
{
	INT32 nFrames = GetCurrentFrame() - nStartFrame;

	inputbuf_save();

	INT32 nMetadataOffset = ftell(fp); // save FRM1 chunk after inputbuf.
	INT32 nChunkSize = ftell(fp) - 4 - nSizeOffset;		// Fill in chunk size and no of recorded frames
	fseek(fp, nSizeOffset, SEEK_SET);
	fwrite(&nChunkSize, 1, 4, fp);
	fwrite(&nFrames, 1, 4, fp);
	fwrite(&nReplayUndoCount, 1, 4, fp);

	// NOTE: chunk should be aligned here, since the compressed
	// file code writes 4 bytes at a time

	// write metadata
	INT32 nMetaLen = wcslen(wszMetadata);
	if(nMetaLen > 0) {
		fseek(fp, nMetadataOffset, SEEK_SET);
		const char szChunkHeader[] = "FRM1";
		fwrite(szChunkHeader, 1, 4, fp);
		INT32 nMetaSize = nMetaLen * 2;
		fwrite(&nMetaSize, 1, 4, fp);
		UINT8* metabuf = (UINT8*)malloc(nMetaSize);
		INT32 i;
		for(i=0; i<nMetaLen; ++i) {
			metabuf[i*2 + 0] = wszMetadata[i] & 0xff;
			metabuf[i*2 + 1] = (wszMetadata[i] >> 8) & 0xff;
		}
		fwrite(metabuf, 1, nMetaSize, fp);
		free(metabuf);
	}

	fclose(fp);
	fp = NULL;
	if (bReplayDontClose) {
		if(!StartReplay(szCurrentMovieFilename)) return;
	}
}

static void CloseReplay()
{
	if(fp) {
		fclose(fp);
		fp = NULL;
	}
}

void StopReplay()
{
	PrintInputsReset();

	if (nReplayStatus) {
		if (nReplayStatus == 1) {

#ifdef FBNEO_DEBUG
			dprintf(_T(" ** Recording stopped, recorded %d frames.\n"), GetCurrentFrame() - nStartFrame);
#endif
			CloseRecord();
#ifdef FBNEO_DEBUG
			//PrintResult();
#endif
		} else {
#ifdef FBNEO_DEBUG
			dprintf(_T(" ** Replay stopped, replayed %d frames.\n"), GetCurrentFrame() - nStartFrame);
#endif

			CloseReplay();
		}

		inputbuf_exit();

		nReplayStatus = 0;
		nStartFrame = 0;
		memset(&MovieInfo, 0, sizeof(MovieInfo));
		CheckRedraw();
		MenuEnableItems();
	}
}


//#
//#             Input Status Freezing
//#
//##############################################################################

static inline void Write32(UINT8*& ptr, const unsigned long v)
{
	*ptr++ = (UINT8)(v&0xff);
	*ptr++ = (UINT8)((v>>8)&0xff);
	*ptr++ = (UINT8)((v>>16)&0xff);
	*ptr++ = (UINT8)((v>>24)&0xff);
}

static inline UINT32 Read32(const UINT8*& ptr)
{
	UINT32 v;
	v = (UINT32)(*ptr++);
	v |= (UINT32)((*ptr++)<<8);
	v |= (UINT32)((*ptr++)<<16);
	v |= (UINT32)((*ptr++)<<24);
	return v;
}

static inline void Write16(UINT8*& ptr, const UINT16 v)
{
	*ptr++ = (UINT8)(v&0xff);
	*ptr++ = (UINT8)((v>>8)&0xff);
}

static inline UINT16 Read16(const UINT8*& ptr)
{
	UINT16 v;
	v = (UINT16)(*ptr++);
	v |= (UINT16)((*ptr++)<<8);
	return v;
}

INT32 FreezeInputSize()
{
	return 4 + 2*nGameInpCount;
}

INT32 FreezeInput(UINT8** buf, INT32* size)
{
	*size = 4 + 2*nGameInpCount;
	*buf = (UINT8*)malloc(*size);
	if(!*buf)
	{
		bprintf(0, _T("error in FreezeInput()\n"));
		return -1;
	}

	UINT8* ptr=*buf;
	Write32(ptr, nGameInpCount);
	for (UINT32 i = 0; i < nGameInpCount; i++)
	{
		Write16(ptr, nPrevInputs[i]);
	}

	return 0;
}

INT32 UnfreezeInput(const UINT8* buf, INT32 size)
{
	UINT32 n=Read32(buf);
	if(n>0x100 || (unsigned)size < (4 + 2*n))
	{
		bprintf(0, _T("error in UnfreezeInput()\n"));
		return -1;
	}

	for (UINT32 i = 0; i < n; i++)
	{
		nPrevInputs[i]=Read16(buf);
	}

	return 0;
}

//------------------------------------------------------

static void GetRecordingPath(wchar_t* szPath)
{
	wchar_t szDrive[MAX_PATH];
	wchar_t szDirectory[MAX_PATH];
	wchar_t szFilename[MAX_PATH];
	wchar_t szExt[MAX_PATH];
	szDrive[0] = '\0';
	szDirectory[0] = '\0';
	szFilename[0] = '\0';
	szExt[0] = '\0';
	_wsplitpath(szPath, szDrive, szDirectory, szFilename, szExt);
	if (szDrive[0] == '\0' && szDirectory[0] == '\0') {
		wchar_t szTmpPath[MAX_PATH];
		wcscpy(szTmpPath, L"recordings\\");
		wcsncpy(szTmpPath + wcslen(szTmpPath), szPath, MAX_PATH - wcslen(szTmpPath));
		szTmpPath[MAX_PATH-1] = '\0';
		wcscpy(szPath, szTmpPath);
	}
}

static void DisplayPropertiesError(HWND hDlg, INT32 nErrType)
{
	if (hDlg != 0) {
		switch (nErrType) {
			case 0:
				SetDlgItemTextW(hDlg, IDC_METADATA, _T("ERROR: Not a FBNeo input recording file.\0"));
				break;
			case 1:
				SetDlgItemTextW(hDlg, IDC_METADATA, _T("ERROR: Incompatible file-type.  Try playback with an earlier version of FBNeo.\0"));
				break;
			case 2:
				SetDlgItemTextW(hDlg, IDC_METADATA, _T("ERROR: Recording is corrupt :(\0"));
				break;
		}
	}
}

void DisplayReplayProperties(HWND hDlg, bool bClear)
{
	if (hDlg != 0) {
		// save status of read only checkbox
		static bool bReadOnlyStatus = true;
		if (IsWindowEnabled(GetDlgItem(hDlg, IDC_READONLY))) {
			bReadOnlyStatus = (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_READONLY, BM_GETCHECK, 0, 0));
		}

		//bReplayShowMovement = false;
		if (IsWindowEnabled(GetDlgItem(hDlg, IDC_SHOWMOVEMENT))) {
			if (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_SHOWMOVEMENT, BM_GETCHECK, 0, 0)) {
				bReplayShowMovement = true;
			}
		}

		bReplayReadOnly = bReadOnlyStatus;

		// set default values
		SetDlgItemTextA(hDlg, IDC_LENGTH, "");
		SetDlgItemTextA(hDlg, IDC_FRAMES, "");
		SetDlgItemTextA(hDlg, IDC_UNDO, "");
		SetDlgItemTextA(hDlg, IDC_METADATA, "");
		SetDlgItemTextA(hDlg, IDC_REPLAYRESET, "");
		SetDlgItemTextA(hDlg, IDC_REPLAYTIME, "");
		EnableWindow(GetDlgItem(hDlg, IDC_READONLY), FALSE);
		SendDlgItemMessage(hDlg, IDC_READONLY, BM_SETCHECK, BST_UNCHECKED, 0);

		EnableWindow(GetDlgItem(hDlg, IDC_SHOWMOVEMENT), FALSE);
		SendDlgItemMessage(hDlg, IDC_SHOWMOVEMENT, BM_SETCHECK, BST_UNCHECKED, 0);

		EnableWindow(GetDlgItem(hDlg, IDC_STARTPAUSED), FALSE);
		SendDlgItemMessage(hDlg, IDC_STARTPAUSED, BM_SETCHECK, BST_UNCHECKED, 0);

		EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);

		// hide neocdz warning for now
		ShowWindow(GetDlgItem(hDlg, IDC_NGCD_WARN), SW_HIDE);

		if(bClear) {
			return;
		}

		long lCount = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCOUNT, 0, 0);
		long lIndex = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCURSEL, 0, 0);
		if (lIndex == CB_ERR) {
			return;
		}

		if (lIndex == lCount - 1) {							// Last item is "Browse..."
			EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);		// Browse is selectable
			return;
		}

		long lStringLength = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETLBTEXTLEN, (WPARAM)lIndex, 0);
		if(lStringLength + 1 > MAX_PATH) {
			return;
		}

		SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETLBTEXT, (WPARAM)lIndex, (LPARAM)szChoice);

		// check relative path
		GetRecordingPath(szChoice);
	}

	const char szFileHeader[] = "FB1 ";					// File identifier
	const char szSavestateHeader[] = "FS1 ";			// Chunk identifier
	const char szRecordingHeader[] = "FR1 ";			// Chunk identifier
	const char szMetadataHeader[] = "FRM1";				// Chunk identifier
	char ReadHeader[4];
	INT32 nChunkSize = 0;
	INT32 nChunkDataPosition = 0;
	INT32 nFileVer = 0;
	INT32 nFileMin = 0;
	INT32 t1 = 0, t2 = 0;
	INT32 nFrames = 0;
	INT32 nUndoCount = 0;
	wchar_t* local_metadata = NULL;

	memset(&wszStartupGame, 0, sizeof(wszStartupGame));
	memset(&wszAuthorInfo, 0, sizeof(wszAuthorInfo));

	FILE* fd = _wfopen(szChoice, L"r+b");
	if (!fd) {
		return;
	}

	if (hDlg != 0) {
		if (_waccess(szChoice, W_OK)) {
			SendDlgItemMessage(hDlg, IDC_READONLY, BM_SETCHECK, BST_CHECKED, 0);
		} else {
			EnableWindow(GetDlgItem(hDlg, IDC_READONLY), TRUE);
			SendDlgItemMessage(hDlg, IDC_READONLY, BM_SETCHECK, (bReplayReadOnly) ? BST_CHECKED : BST_UNCHECKED, 0); //read-only by default
		}

		EnableWindow(GetDlgItem(hDlg, IDC_SHOWMOVEMENT), TRUE);
		SendDlgItemMessage(hDlg, IDC_SHOWMOVEMENT, BM_SETCHECK, (bReplayShowMovement) ? BST_CHECKED : BST_UNCHECKED, 0);

		EnableWindow(GetDlgItem(hDlg, IDC_STARTPAUSED), TRUE);
		SendDlgItemMessage(hDlg, IDC_STARTPAUSED, BM_SETCHECK, (bReplayStartPaused) ? BST_CHECKED : BST_UNCHECKED, 0);
	}

	memset(ReadHeader, 0, 4);
	fread(ReadHeader, 1, 4, fd);						// Read identifier
	if (memcmp(ReadHeader, szFileHeader, 4)) {			// Not the right file type
		fclose(fd);
		DisplayPropertiesError(hDlg, 0 /* not our file */);
		return;
	}
	INT32 movieFlagsTemp = 0;
	fread(&movieFlagsTemp, 1, 4, fd);						// Read identifier

	bStartFromReset = (movieFlagsTemp&MOVIE_FLAG_FROM_POWERON) ? 1 : 0; // Starts from reset

	if (!bStartFromReset || (bStartFromReset && movieFlagsTemp & MOVIE_FLAG_WITH_NVRAM)) {
		memset(ReadHeader, 0, 4);
		fread(ReadHeader, 1, 4, fd);						// Read identifier
		if (memcmp(ReadHeader, szSavestateHeader, 4)) {		// Not the chunk type
			fclose(fd);
			DisplayPropertiesError(hDlg, 1 /* most likely recorded w/ an earlier version */);

			return;
		}

		fread(&nChunkSize, 1, 4, fd);
		if (nChunkSize <= 0x40) {							// Not big enough
			fclose(fd);
			DisplayPropertiesError(hDlg, 2 /* corrupt. */);
			return;
		}

		nChunkDataPosition = ftell(fd);

		fread(&nFileVer, 1, 4, fd);							// Version of FB that this file was saved from

		fread(&t1, 1, 4, fd);								// Min version of FB that NV  data will work with
		fread(&t2, 1, 4, fd);								// Min version of FB that All data will work with

		nFileMin = t2;										// Replays require a full state

//		if (nBurnVer < nFileMin) {							// Error - emulator is too old to load this state
//			fclose(fd);
//			return;
//		}

		fseek(fd, nChunkDataPosition + nChunkSize, SEEK_SET);
	}

	memset(ReadHeader, 0, 4);
	fread(ReadHeader, 1, 4, fd);						// Read identifier
	if (memcmp(ReadHeader, szRecordingHeader, 4)) {		// Not the chunk type
		fclose(fd);
		DisplayPropertiesError(hDlg, 1 /* most likely recorded w/ an earlier version */);
		return;
	}

	nChunkSize = 0;
	fread(&nChunkSize, 1, 4, fd);
	if (nChunkSize <= 0x10) {							// Not big enough
		fclose(fd);
		DisplayPropertiesError(hDlg, 2 /* corrupt. */);
		return;
	}

	nChunkDataPosition = ftell(fd);
	fread(&nFrames, 1, 4, fd);
	fread(&nUndoCount, 1, 4, fd);

	fread(&nThisMovieVersion, 1, 4, fd);

	memset(&MovieInfo, 0, sizeof(MovieInfo));

	fread(&MovieInfo, 1, sizeof(MovieInfo), fd);
	bprintf(0, _T("Movie Version %X\n"), nThisMovieVersion);
	bprintf(0, _T("Ext Info: %S\n"), ReplayDecodeDateTime());

	if (nThisMovieVersion < nMinimumMovieVersion) { // Uhoh, wrong format!
		fclose(fd);
		DisplayPropertiesError(hDlg, 1 /* most likely recorded w/ an earlier version */);
		return;
	}
	fread(&nThisFBVersion, 1, 4, fd);

	// read metadata
	fseek(fd, nChunkDataPosition + nChunkSize, SEEK_SET);
	memset(ReadHeader, 0, 4);
	fread(ReadHeader, 1, 4, fd);						// Read identifier
	if (memcmp(ReadHeader, szMetadataHeader, 4) == 0) {
		nChunkSize = 0;
		fread(&nChunkSize, 1, 4, fd);
		INT32 nMetaLen = nChunkSize >> 1;
		if(nMetaLen >= MAX_METADATA) {
			nMetaLen = MAX_METADATA-1;
		}

		local_metadata = (wchar_t*)malloc((nMetaLen+1)*sizeof(wchar_t));
		memset(local_metadata, 0, (nMetaLen+1)*sizeof(wchar_t));

		INT32 i;
		for(i=0; i<nMetaLen; ++i) {
			wchar_t c = 0;
			c |= fgetc(fd) & 0xff;
			c |= (fgetc(fd) & 0xff) << 8;
			local_metadata[i] = c;
		}
		local_metadata[i] = L'\0';

		if (bStartFromReset) {
			swscanf(local_metadata, L"%[^','],%959c", wszStartupGame, wszAuthorInfo);
			bprintf(0, _T("startup game: %s.\n"), wszStartupGame);
			bprintf(0, _T("author info: %s.\n"), wszAuthorInfo);
		} else {
			wcsncpy(wszAuthorInfo, local_metadata, MAX_METADATA-64-1);
		}
	}

	// done reading file
	fclose(fd);
	free(local_metadata);

	if (hDlg != 0) {
		// file exists and is the correct format,
		// so enable the "Ok" button
		EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);

		// turn nFrames into a length string
		INT32 nSeconds = (nFrames * 100 + (nBurnFPS>>1)) / nBurnFPS;
		INT32 nMinutes = nSeconds / 60;
		INT32 nHours = nSeconds / 3600;

		// write strings to dialog
		char szFramesString[32];
		char szLengthString[32];
		char szUndoCountString[32];
		char szRecordedFrom[256];
		char szRecordedTime[32] = { 0 };
		char szStartType[32];

		sprintf(szFramesString, "%d", nFrames);
		sprintf(szLengthString, "%02d:%02d:%02d", nHours, nMinutes % 60, nSeconds % 60);
		sprintf(szUndoCountString, "%d", nUndoCount);
		if (nThisFBVersion && !nFileVer) nFileVer = nThisFBVersion;

		sprintf(szStartType, "%s %s", (bStartFromReset) ? "Power-On" : "Savestate", (bStartFromReset && movieFlagsTemp & MOVIE_FLAG_WITH_NVRAM) ? "w/NVRAM" : "");

		if (nFileVer)
			sprintf(szRecordedFrom, "%s, v%x.%x.%x.%02x", szStartType, nFileVer >> 20, (nFileVer >> 16) & 0x0F, (nFileVer >> 8) & 0xFF, nFileVer & 0xFF);
		else
			sprintf(szRecordedFrom, "%s", szStartType);

		strcpy(szRecordedTime, ReplayDecodeDateTime());

		SetDlgItemTextA(hDlg, IDC_LENGTH, szLengthString);
		SetDlgItemTextA(hDlg, IDC_FRAMES, szFramesString);
		SetDlgItemTextA(hDlg, IDC_UNDO, szUndoCountString);
		SetDlgItemTextW(hDlg, IDC_METADATA, wszAuthorInfo);
		SetDlgItemTextA(hDlg, IDC_REPLAYRESET, szRecordedFrom);
		SetDlgItemTextA(hDlg, IDC_REPLAYTIME, szRecordedTime);

		if (!_tcsncmp(wszStartupGame, _T("neocdz"), 6) || !_tcsncmp(wszStartupGame, _T("ngcd_"), 5) ||
		    _tcsstr(szChoice, _T("ngcd_")) ) {
			// Neo Geo CD game, show nice warning :)
			ShowWindow(GetDlgItem(hDlg, IDC_NGCD_WARN), SW_SHOW);
		}
	}
}

static TCHAR *GetDrvName() // for both arcade & neocd
{
	static TCHAR szName[MAX_PATH] = _T("");

	if (NeoCDInfo_ID()) {
		_stprintf(szName, _T("ngcd_%s"), NeoCDInfo_Text(DRV_NAME));
	} else {
		_stprintf(szName, _T("%s"), BurnDrvGetText(DRV_NAME));
	}

	return szName;
}

static BOOL CALLBACK ReplayDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM)
{
	if (Msg == WM_INITDIALOG) {
		wchar_t szFindPath[MAX_PATH] = L"recordings\\*.fr";
		WIN32_FIND_DATA wfd;
		HANDLE hFind;
		INT32 i = 0;

		SendDlgItemMessage(hDlg, IDC_READONLY, BM_SETCHECK, BST_CHECKED, 0);

		memset(&wfd, 0, sizeof(WIN32_FIND_DATA));
		if (bDrvOkay) {
			_stprintf(szFindPath, _T("recordings\\%s*.fr"), GetDrvName());
		}

		hFind = FindFirstFile(szFindPath, &wfd);
		if (hFind != INVALID_HANDLE_VALUE) {
			do {
				if(!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
					SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_INSERTSTRING, i++, (LPARAM)wfd.cFileName);
			} while(FindNextFile(hFind, &wfd));
			FindClose(hFind);
		}
		SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_INSERTSTRING, i, (LPARAM)_T("Browse..."));
		SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_SETCURSEL, i, 0);

		// hide neocdz warning for now
		ShowWindow(GetDlgItem(hDlg, IDC_NGCD_WARN), SW_HIDE);

		if (i>=1) {
			DisplayReplayProperties(hDlg, false);
			SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_SETCURSEL, i, 0);
		}

		SetFocus(GetDlgItem(hDlg, IDC_CHOOSE_LIST));
		return FALSE;
	}

	if (Msg == WM_COMMAND) {
		if (HIWORD(wParam) == CBN_SELCHANGE) {
			LONG lCount = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCOUNT, 0, 0);
			LONG lIndex = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCURSEL, 0, 0);
			if (lIndex != CB_ERR) {
				DisplayReplayProperties(hDlg, (lIndex == lCount - 1));		// Selecting "Browse..." will clear the replay properties display
			}
		} else if (HIWORD(wParam) == CBN_CLOSEUP) {
			LONG lCount = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCOUNT, 0, 0);
			LONG lIndex = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCURSEL, 0, 0);
			if (lIndex != CB_ERR) {
				if (lIndex == lCount - 1) {
					// send an OK notification to open the file browser
					SendMessage(hDlg, WM_COMMAND, (WPARAM)IDOK, 0);
				}
			}
		} else {
			INT32 wID = LOWORD(wParam);
			switch (wID) {
				case IDOK:
					{
						LONG lCount = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCOUNT, 0, 0);
						LONG lIndex = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_GETCURSEL, 0, 0);
						if (lIndex != CB_ERR) {
							if (lIndex == lCount - 1) {
								MakeOfn(szFilter);
								ofn.lpstrTitle = FBALoadStringEx(hAppInst, IDS_REPLAY_REPLAY, true);
								//ofn.Flags &= ~OFN_HIDEREADONLY;

								INT32 nRet = GetOpenFileName(&ofn); // Browse...
								if (nRet != 0) {
									LONG lOtherIndex = SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_FINDSTRING, (WPARAM)-1, (LPARAM)szChoice);
									if (lOtherIndex != CB_ERR) {
										// select already existing string
										SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_SETCURSEL, lOtherIndex, 0);
									} else {
										SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_INSERTSTRING, lIndex, (LPARAM)szChoice);
										SendDlgItemMessage(hDlg, IDC_CHOOSE_LIST, CB_SETCURSEL, lIndex, 0);
									}
									// restore focus to the dialog
									SetFocus(GetDlgItem(hDlg, IDC_CHOOSE_LIST));
									DisplayReplayProperties(hDlg, false);
									if (ofn.Flags & OFN_READONLY || bReplayReadOnly) {
										SendDlgItemMessage(hDlg, IDC_READONLY, BM_SETCHECK, BST_CHECKED, 0);
									} else {
										SendDlgItemMessage(hDlg, IDC_READONLY, BM_SETCHECK, BST_UNCHECKED, 0);
									}
								}
							} else {
								// get readonly status
								bReplayReadOnly = false;
								if (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_READONLY, BM_GETCHECK, 0, 0)) {
									bReplayReadOnly = true;
								}

								// get show movements status
								bReplayShowMovement = false;
								if (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_SHOWMOVEMENT, BM_GETCHECK, 0, 0)) {
									bReplayShowMovement = true;
								}

								// get start paused status
								bReplayStartPaused = false;
								if (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_STARTPAUSED, BM_GETCHECK, 0, 0)) {
									bReplayStartPaused = true;
								}

								EndDialog(hDlg, 1);					// only allow OK if a valid selection was made
							}
						}
					}
					return TRUE;
				case IDCANCEL:
					szChoice[0] = '\0';
					EndDialog(hDlg, 0);
					return FALSE;
			}
		}
	}

	return FALSE;
}

static INT32 ReplayDialog()
{
	return FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_REPLAYINP), hScrnWnd, (DLGPROC)ReplayDialogProc);
}

static INT32 VerifyRecordingAccessMode(wchar_t* szFilename, INT32 mode)
{
	GetRecordingPath(szFilename);
	if(_waccess(szFilename, mode)) {
		return 0;							// not writeable, return failure
	}

	return 1;
}

static void VerifyRecordingFilename(HWND hDlg)
{
	wchar_t szFilename[MAX_PATH];
	GetDlgItemText(hDlg, IDC_FILENAME, szFilename, MAX_PATH);

	// if filename null, or, file exists and is not writeable
	// then disable the dialog controls
	if(szFilename[0] == '\0' ||
		(VerifyRecordingAccessMode(szFilename, 0) != 0 && VerifyRecordingAccessMode(szFilename, W_OK) == 0)) {
		EnableWindow(GetDlgItem(hDlg, IDOK), FALSE);
		EnableWindow(GetDlgItem(hDlg, IDC_METADATA), FALSE);
	} else {
		EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDC_METADATA), TRUE);
	}
}

static BOOL CALLBACK RecordDialogProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM)
{
	wchar_t szAuthInfo[MAX_METADATA];

	if (Msg == WM_INITDIALOG) {
		// come up with a unique name
		wchar_t szPath[MAX_PATH];
		wchar_t szFilename[MAX_PATH];

		INT32 i = 0;
		_stprintf(szFilename, _T("%s.fr"), GetDrvName());
		wcscpy(szPath, szFilename);
		while(VerifyRecordingAccessMode(szPath, 0) == 1) {
			_stprintf(szFilename, _T("%s-%d.fr"), GetDrvName(), ++i);
			wcscpy(szPath, szFilename);
		}

		SetDlgItemText(hDlg, IDC_FILENAME, szFilename);
		SetDlgItemTextW(hDlg, IDC_METADATA, L"");
		CheckDlgButton(hDlg, IDC_REPLAYRESET, BST_UNCHECKED);
		CheckDlgButton(hDlg, IDC_REPLAYNVRAM, BST_CHECKED);

		EnableWindow(GetDlgItem(hDlg, IDC_REPLAYNVRAM), FALSE);

		VerifyRecordingFilename(hDlg);

		SetFocus(GetDlgItem(hDlg, IDC_METADATA));
		return FALSE;
	}

	if (Msg == WM_COMMAND) {
		if (HIWORD(wParam) == EN_CHANGE) {
			VerifyRecordingFilename(hDlg);
		} else {
			INT32 wID = LOWORD(wParam);
			switch (wID) {
				case IDC_REPLAYRESET:
					{
						// w/NVRAM option only available w/From Power-on
						if (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_REPLAYRESET, BM_GETCHECK, 0, 0)) {
							EnableWindow(GetDlgItem(hDlg, IDC_REPLAYNVRAM), TRUE);
						} else {
							EnableWindow(GetDlgItem(hDlg, IDC_REPLAYNVRAM), FALSE);
						}
					}
					return TRUE;
				case IDC_BROWSE:
					{
						_stprintf(szChoice, _T("%s"), GetDrvName());
						MakeOfn(szFilter);
						ofn.lpstrTitle = FBALoadStringEx(hAppInst, IDS_REPLAY_RECORD, true);
						ofn.Flags |= OFN_OVERWRITEPROMPT;
						INT32 nRet = GetSaveFileName(&ofn);
						if (nRet != 0) {
							// this should trigger an EN_CHANGE message
							SetDlgItemText(hDlg, IDC_FILENAME, szChoice);
						}
					}
					return TRUE;
				case IDOK:
					GetDlgItemText(hDlg, IDC_FILENAME, szChoice, MAX_PATH);
					GetDlgItemTextW(hDlg, IDC_METADATA, szAuthInfo, MAX_METADATA-64-1);
					bStartFromReset = false;
					bWithNVRAM = false;
					if (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_REPLAYRESET, BM_GETCHECK, 0, 0)) {
						bStartFromReset = true;

						if (BST_CHECKED == SendDlgItemMessage(hDlg, IDC_REPLAYNVRAM, BM_GETCHECK, 0, 0)) {
							bWithNVRAM = true;
						}

						// add "romset," to beginning of metadata
						_stprintf(wszMetadata, _T("%s,%s"), BurnDrvGetText(DRV_NAME), szAuthInfo);
					} else {
						_tcscpy(wszMetadata, szAuthInfo);
					}
					wszMetadata[MAX_METADATA-1] = L'\0';

					// ensure a relative path has the "recordings\" path in prepended to it
					GetRecordingPath(szChoice);
					EndDialog(hDlg, 1);
					return TRUE;
				case IDCANCEL:
					szChoice[0] = '\0';
					EndDialog(hDlg, 0);
					return FALSE;
			}
		}
	}

	return FALSE;
}

static INT32 RecordDialog()
{
	return FBADialogBox(hAppInst, MAKEINTRESOURCE(IDD_RECORDINP), hScrnWnd, (DLGPROC)RecordDialogProc);
}
