// Driver Save State module
#include "burner.h"

// from dynhuff.cpp
INT32 FreezeDecode(UINT8 **buffer, INT32 *size);
INT32 UnfreezeDecode(const UINT8* buffer, INT32 size);
INT32 FreezeEncode(UINT8 **buffer, INT32 *size);
INT32 UnfreezeEncode(const UINT8* buffer, INT32 size);

// from replay.cpp
INT32 FreezeInput(UINT8** buf, int* size);
INT32 UnfreezeInput(const UINT8* buf, INT32 size);

extern INT32 nReplayStatus;
extern bool bReplayReadOnly;
extern INT32 nReplayUndoCount;
extern UINT32 nReplayCurrentFrame;
extern UINT32 nStartFrame;

// If bAll=0 save/load all non-volatile ram to .fs
// If bAll=1 save/load all ram to .fs

// ------------ State len --------------------
static INT32 nTotalLen = 0;

static INT32 __cdecl StateLenAcb(struct BurnArea* pba)
{
	nTotalLen += pba->nLen;

	return 0;
}

static INT32 StateInfo(int* pnLen, int* pnMinVer, INT32 bAll)
{
	INT32 nMin = 0;
	nTotalLen = 0;
	BurnAcb = StateLenAcb;

	BurnAreaScan(ACB_NVRAM, &nMin);						// Scan nvram
	if (bAll) {
		INT32 m;
		BurnAreaScan(ACB_MEMCARD, &m);					// Scan memory card
		if (m > nMin) {									// Up the minimum, if needed
			nMin = m;
		}
		BurnAreaScan(ACB_VOLATILE, &m);					// Scan volatile ram
		if (m > nMin) {									// Up the minimum, if needed
			nMin = m;
		}
	}
	*pnLen = nTotalLen;
	*pnMinVer = nMin;

	return 0;
}

// State load
INT32 BurnStateLoadEmbed(FILE* fp, INT32 nOffset, INT32 bAll, INT32 (*pLoadGame)())
{
	const char* szHeader = "FS1 ";						// Chunk identifier

	INT32 nLen = 0;
	INT32 nMin = 0, nFileVer = 0, nFileMin = 0;
	INT32 t1 = 0, t2 = 0;
	char ReadHeader[4];
	char szForName[33];
	INT32 nChunkSize = 0;
	UINT8 *Def = NULL;
	INT32 nDefLen = 0;									// Deflated version
	INT32 nRet = 0;

	if (nOffset >= 0) {
		fseek(fp, nOffset, SEEK_SET);
	} else {
		if (nOffset == -2) {
			fseek(fp, 0, SEEK_END);
		} else {
			fseek(fp, 0, SEEK_CUR);
		}
	}

	memset(ReadHeader, 0, 4);
	fread(ReadHeader, 1, 4, fp);						// Read identifier
	if (memcmp(ReadHeader, szHeader, 4)) {				// Not the right file type
		return -2;
	}

	fread(&nChunkSize, 1, 4, fp);
	if (nChunkSize <= 0x40) {							// Not big enough
		return -1;
	}

	INT32 nChunkData = ftell(fp);

	fread(&nFileVer, 1, 4, fp);							// Version of FB that this file was saved from

	fread(&t1, 1, 4, fp);								// Min version of FB that NV  data will work with
	fread(&t2, 1, 4, fp);								// Min version of FB that All data will work with

	if (bAll) {											// Get the min version number which applies to us
		nFileMin = t2;
	} else {
		nFileMin = t1;
	}

	fread(&nDefLen, 1, 4, fp);							// Get the size of the compressed data block

	memset(szForName, 0, sizeof(szForName));
	fread(szForName, 1, 32, fp);

	if (nBurnVer < nFileMin) {							// Error - emulator is too old to load this state
		return -5;
	}

	// Check the game the savestate is for, and load it if needed.
	{
		bool bLoadGame = false;

		if (nBurnDrvActive < nBurnDrvCount) {
			if (strcmp(szForName, BurnDrvGetTextA(DRV_NAME))) {	// The save state is for the wrong game
				bLoadGame = true;
			}
		} else {										// No game loaded
			bLoadGame = true;
		}

		if (bLoadGame) {
			UINT32 nCurrentGame = nBurnDrvActive;
			UINT32 i;
			for (i = 0; i < nBurnDrvCount; i++) {
				nBurnDrvActive = i;
				if (strcmp(szForName, BurnDrvGetTextA(DRV_NAME)) == 0) {
					break;
				}
			}
			if (i == nBurnDrvCount) {
				nBurnDrvActive = nCurrentGame;
				return -3;
			} else {
				if (nCurrentGame != nBurnDrvActive) {
					INT32 nOldActive = nBurnDrvActive;  // Exit current game if loading a state from another game
					nBurnDrvActive = nCurrentGame;
					DrvExit();
					nBurnDrvActive = nOldActive;
				}
				if (pLoadGame == NULL) {
					return -1;
				}
				if (pLoadGame()) {
					return -1;
				}
			}
		}
	}

	StateInfo(&nLen, &nMin, bAll);
	if (nLen <= 0) {									// No memory to load
		return -1;
	}

	// Check if the save state is okay
	if (nFileVer < nMin) {								// Error - this state is too old and cannot be loaded.
		return -4;
	}

	fseek(fp, nChunkData + 0x30, SEEK_SET);				// Read current frame
	fread(&nReplayCurrentFrame, 1, 4, fp);
	nCurrentFrame = nStartFrame + nReplayCurrentFrame;

	fseek(fp, 0x0C, SEEK_CUR);							// Move file pointer to the start of the compressed block
	Def = (UINT8*)malloc(nDefLen);
	if (Def == NULL) {
		return -1;
	}
	memset(Def, 0, nDefLen);
	fread(Def, 1, nDefLen, fp);							// Read in deflated block

	nRet = BurnStateDecompress(Def, nDefLen, bAll);		// Decompress block into driver
	free(Def);											// free deflated block

	fseek(fp, nChunkData + nChunkSize, SEEK_SET);

	if (nRet) {
		return -1;
	} else {
		return 0;
	}
}

// State load
INT32 BurnStateLoad(TCHAR* szName, INT32 bAll, INT32 (*pLoadGame)())
{
	const char szHeader[] = "FB1 ";						// File identifier
	char szReadHeader[4] = "";
	INT32 nRet = 0;

	FILE* fp = _tfopen(szName, _T("rb"));
	if (fp == NULL) {
		return 1;
	}

	fread(szReadHeader, 1, 4, fp);						// Read identifier
	if (memcmp(szReadHeader, szHeader, 4) == 0) {		// Check filetype
		nRet = BurnStateLoadEmbed(fp, -1, bAll, pLoadGame);
	}

	// load movie extra info
	if(nReplayStatus)
	{
		const char szMovieExtra[] = "MOV ";
		const char szDecodeChunk[] = "HUFF";
		const char szInputChunk[] = "INP ";

		INT32 nChunkSize;
		UINT8* buf=NULL;

		do
		{
			fread(szReadHeader, 1, 4, fp);
			if(memcmp(szReadHeader, szMovieExtra, 4))           { nRet = -1;  break; }
			fread(&nChunkSize, 1, 4, fp);

			fread(szReadHeader, 1, 4, fp);
			if(memcmp(szReadHeader, szDecodeChunk, 4))          { nRet = -1;  break; }
			fread(&nChunkSize, 1, 4, fp);

			if((buf=(UINT8*)malloc(nChunkSize))==NULL)  { nRet = -1;  break; }
			fread(buf, 1, nChunkSize, fp);

			INT32 ret=-1;
			if(nReplayStatus == 1)
			{
				ret = UnfreezeEncode(buf, nChunkSize);
				++nReplayUndoCount;
			}
			else if(nReplayStatus == 2)
			{
				ret = UnfreezeDecode(buf, nChunkSize);
			}
			if(ret)                                             { nRet = -1;  break; }

			free(buf);
			buf = NULL;

			fread(szReadHeader, 1, 4, fp);
			if(memcmp(szReadHeader, szInputChunk, 4))           { nRet = -1;  break; }
			fread(&nChunkSize, 1, 4, fp);

			if((buf=(UINT8*)malloc(nChunkSize))==NULL)  { nRet = -1;  break; }
			fread(buf, 1, nChunkSize, fp);
			if(UnfreezeInput(buf, nChunkSize))                  { nRet = -1;  break; }

			free(buf);
			buf = NULL;
		}
		while(false);

		if(buf) free(buf);
	}

	fclose(fp);

	if (nRet == 0) { // Force the palette to recalculate on state load
		BurnRecalcPal();
	}

	if (nRet < 0) {
		return -nRet;
	} else {
		return 0;
	}
}

// Write a savestate as a chunk of an "FB1 " file
// nOffset is the absolute offset from the beginning of the file
// -1: Append at current position
// -2: Append at EOF
INT32 BurnStateSaveEmbed(FILE* fp, INT32 nOffset, INT32 bAll)
{
	const char* szHeader = "FS1 ";						// Chunk identifier

	INT32 nLen = 0;
	INT32 nNvMin = 0, nAMin = 0;
	INT32 nZero = 0;
	char szGame[33];
	UINT8 *Def = NULL;
	INT32 nDefLen = 0;									// Deflated version
	INT32 nRet = 0;

	if (fp == NULL) {
		return -1;
	}

	StateInfo(&nLen, &nNvMin, 0);						// Get minimum version for NV part
	nAMin = nNvMin;
	if (bAll) {											// Get minimum version for All data
		StateInfo(&nLen, &nAMin, 1);
	}

	if (nLen <= 0) {									// No memory to save
		return -1;
	}

	if (nOffset >= 0) {
		fseek(fp, nOffset, SEEK_SET);
	} else {
		if (nOffset == -2) {
			fseek(fp, 0, SEEK_END);
		} else {
			fseek(fp, 0, SEEK_CUR);
		}
	}

	fwrite(szHeader, 1, 4, fp);							// Chunk identifier
	INT32 nSizeOffset = ftell(fp);						// Reserve space to write the size of this chunk
	fwrite(&nZero, 1, 4, fp);							//

	fwrite(&nBurnVer, 1, 4, fp);						// Version of FB this was saved from
	fwrite(&nNvMin, 1, 4, fp);							// Min version of FB NV  data will work with
	fwrite(&nAMin, 1, 4, fp);							// Min version of FB All data will work with

	fwrite(&nZero, 1, 4, fp);							// Reserve space to write the compressed data size

	memset(szGame, 0, sizeof(szGame));					// Game name
	sprintf(szGame, "%.32s", BurnDrvGetTextA(DRV_NAME));			//
	fwrite(szGame, 1, 32, fp);							//

	nReplayCurrentFrame = GetCurrentFrame() - nStartFrame;
	fwrite(&nReplayCurrentFrame, 1, 4, fp);					// Current frame

	fwrite(&nZero, 1, 4, fp);							// Reserved
	fwrite(&nZero, 1, 4, fp);							//
	fwrite(&nZero, 1, 4, fp);							//

	nRet = BurnStateCompress(&Def, &nDefLen, bAll);		// Compress block from driver and return deflated buffer
	if (Def == NULL) {
		return -1;
	}

	nRet = fwrite(Def, 1, nDefLen, fp);					// Write block to disk
	free(Def);											// free deflated block and close file

	if (nRet != nDefLen) {								// error writing block to disk
		return -1;
	}

	if (nDefLen & 3) {									// Chunk size must be a multiple of 4
		fwrite(&nZero, 1, 4 - (nDefLen & 3), fp);		// Pad chunk if needed
	}

	fseek(fp, nSizeOffset + 0x10, SEEK_SET);			// Write size of the compressed data
	fwrite(&nDefLen, 1, 4, fp);							//

	nDefLen = (nDefLen + 0x43) & ~3;					// Add for header size and align

	fseek(fp, nSizeOffset, SEEK_SET);					// Write size of the chunk
	fwrite(&nDefLen, 1, 4, fp);							//

	fseek (fp, 0, SEEK_END);							// Set file pointer to the end of the chunk

	return nDefLen;
}

#ifdef BUILD_WIN32
INT32 FileExists(const TCHAR *fileName)
{
    DWORD dwAttrib = GetFileAttributes(fileName);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
            !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
#endif

#define MAX_STATEBACKUPS 10
// SaveState Undo - restores the last savestate backup file. Windows-only at the moment.
INT32 BurnStateUNDO(TCHAR* szName)
{
#ifdef BUILD_WIN32
         /*
         Savestate Undo
         derp.fs.backup0 -> derp.fs
         derp.fs.backup1 -> derp.fs.backup0
         derp.fs.backup2 -> derpfs.backup1
         derp.fs.backup3 -> derpfs.backup2
         */
    INT32 cantundo = 0;

    for (INT32 i = 0; i <= MAX_STATEBACKUPS; i++) {
            TCHAR szBackupNameTo[1024] = _T("");
            TCHAR szBackupNameFrom[1024] = _T("");
            TCHAR szBackupNamePrev[1024] = _T("");

            if (i == 0) {
                _stprintf(szBackupNameTo, _T("%s.UNDO"), szName);// game.fs -> game.fs.UNDO
                _stprintf(szBackupNamePrev, _T("%s.backup0"), szName);
                if (FileExists(szName) && FileExists(szBackupNamePrev)) {
                    DeleteFileW(szBackupNameTo);
                    MoveFileW(szName, szBackupNameTo);
                } else {
                    cantundo = 1;
                }

                _stprintf(szBackupNameTo, _T("%s"), szName);// game.fs
            } else {
                _stprintf(szBackupNameTo, _T("%s.backup%d"), szName, i - 1); //game.fs.backup0
            }
            _stprintf(szBackupNameFrom, _T("%s.backup%d"), szName, i); //game.fs.backup1
            //bprintf(0, _T("%d: %s -> %s\n"), i, szBackupNameFrom, szBackupNameTo);
            MoveFileW(szBackupNameFrom, szBackupNameTo);
    }
    return cantundo;
#else
    return 0;
#endif

}

// State save
INT32 BurnStateSave(TCHAR* szName, INT32 bAll)
{
	const char szHeader[] = "FB1 ";						// File identifier
	INT32 nLen = 0, nVer = 0;
	INT32 nRet = 0;

	if (bAll) {											// Get amount of data
		StateInfo(&nLen, &nVer, 1);
	} else {
		StateInfo(&nLen, &nVer, 0);
	}
	if (nLen <= 0) {									// No data, so exit without creating a savestate
		return 0;										// Don't return an error code
	}

#ifdef BUILD_WIN32
	/*
	 Save State backups - used in conjunction with BurnStateUNDO();
	 derp.fs -> derp.fs.backup
	 derp.fs.backup -> derp.fs.backup1
	 derp.fs.backup1 -> derpfs.backup2
	 derp.fs.backup3 -> derpfs.backup4
	*/
	if (_tcsstr(szName, _T(" slot "))) {
		for (INT32 i=MAX_STATEBACKUPS;i>=0;i--) {
			TCHAR szBackupNameTo[1024] = _T("");
			TCHAR szBackupNameFrom[1024] = _T("");

			_stprintf(szBackupNameTo, _T("%s.backup%d"), szName, i + 1);
			_stprintf(szBackupNameFrom, _T("%s.backup%d"), szName, i);
			if (i == MAX_STATEBACKUPS) {
				DeleteFileW(szBackupNameFrom); // make sure there is only MAX_STATEBACKUPS :)
			} else {
				MoveFileW(szBackupNameFrom, szBackupNameTo); //derp.fs.backup0 -> derp.fs.backup1
				if (i == 0) {
					MoveFileW(szName, szBackupNameFrom); //derp.fs -> derp.fs.backup0
				}
			}
		}
	}
#endif

	FILE* fp = _tfopen(szName, _T("wb"));
	if (fp == NULL) {
		return 1;
	}

	fwrite(&szHeader, 1, 4, fp);
	nRet = BurnStateSaveEmbed(fp, -1, bAll);

	// save movie extra info
	if(nReplayStatus)
	{
		UINT8* huff_buf = NULL;
		INT32 huff_size;
		UINT8* input_buf = NULL;
		INT32 input_size;
		INT32 ret=-1;

		if(nReplayStatus == 1)
		{
			ret = FreezeEncode(&huff_buf, &huff_size);
		}
		else if(nReplayStatus == 2)
		{
			ret = FreezeDecode(&huff_buf, &huff_size);
		}

		if(!ret &&
			!FreezeInput(&input_buf, &input_size))
		{
			const char szMovieExtra[] = "MOV ";
			const char szDecodeChunk[] = "HUFF";
			const char szInputChunk[] = "INP ";

			INT32 nZero = 0;
			INT32 nAlign = 0;
			INT32 nChkLen = 0;
			INT32 nMovChunkLen = 0;

			fwrite(szMovieExtra, 1, 4, fp);
			INT32 nSizeOffset = ftell(fp);
			fwrite(&nZero, 1, 4, fp);						// Leave room for the chunk size
			nMovChunkLen = 0;

			// write Decode block
			nAlign = (huff_size&3) ? (4 - (huff_size&3)) : 0;
			nChkLen = huff_size + nAlign;

			fwrite(szDecodeChunk, 1, 4, fp);
			fwrite(&nChkLen, 1, 4, fp);
			fwrite(huff_buf, 1, huff_size, fp);
			if(nAlign)
			{
				fwrite(&nZero, 1, nAlign, fp);
			}
			nMovChunkLen += nChkLen + 8;

			// write Input block
			nAlign = (input_size&3) ? (4 - (input_size&3)) : 0;
			nChkLen = input_size + nAlign;

			fwrite(szInputChunk, 1, 4, fp);
			fwrite(&nChkLen, 1, 4, fp);
			fwrite(input_buf, 1, input_size, fp);
			if(nAlign)
			{
				fwrite(&nZero, 1, nAlign, fp);
			}
			nMovChunkLen += nChkLen + 8;

			fseek(fp, nSizeOffset, SEEK_SET);
			fwrite(&nMovChunkLen, 1, 4, fp);
			fseek(fp, nMovChunkLen, SEEK_CUR);
		}

		if(huff_buf)    free(huff_buf);
		if(input_buf)   free(input_buf);
	}

	fclose(fp);

	if (nRet < 0) {
		return 1;
	} else {
		return 0;
	}
}
