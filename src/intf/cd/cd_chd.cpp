// Platform-independent CHD CD/GD-ROM backend built on libchdr.
// TOC model and sector-type conversion matrix are ported from MAME's
// src/lib/util/cdrom.cpp (BSD-3-Clause) to stay bug-compatible with chdman.

#include "burnint.h"
#include "cd_chd.h"
#include "chd.h"
#include "cdrom.h"

#define CHD_MAX_TRACKS      (99)
#define CHD_FRAME_SIZE      (2352 + 96)   // libchdr stores every frame at this stride
#define CHD_TRACK_PADDING   (4)           // chdman pads tracks to a 4-frame boundary

struct ChdImage {
	chd_file* pChd;
	FILE*     pFile;
	INT32     nContainer;
	INT32     nNumTracks;
	INT32     nTotalFrames;
	INT32     nFlags;                        // CD_FLAG_GDROM / CD_FLAG_GDROMLE
	UINT8*    pHunkBuf;
	INT32     nHunkBytes;
	INT32     nFramesPerHunk;
	INT32     nVersion;
	INT32     nCachedHunk;                   // -1 = none
	ChdTrack  Tracks[CHD_MAX_TRACKS + 1];    // +1 dummy lead-out entry
};

// LBA -> MSF in BCD, matching the address stored in a real CD sync header.
static UINT32 ChdLbaToMsf(INT32 nLba)
{
	INT32 m = nLba / (60 * 75);
	INT32 s = (nLba / 75) % 60;
	INT32 f = nLba % 75;
	return (((m / 10) << 20) | ((m % 10) << 16) |
	        ((s / 10) << 12) | ((s % 10) <<  8) |
	        ((f / 10) <<  4) | ((f % 10) <<  0));
}

// Map a MAME/chdman TYPE string to (track type, data size).
static void ChdTypeFromString(const char* szType, INT32* pnType, INT32* pnDataSize)
{
	static const struct { const char* szName; INT32 nType; INT32 nDataSize; } Table[] = {
		{ "MODE1",          CHD_TRACK_MODE1,          2048 },
		{ "MODE1/2048",     CHD_TRACK_MODE1,          2048 },
		{ "MODE1_RAW",      CHD_TRACK_MODE1_RAW,      2352 },
		{ "MODE1/2352",     CHD_TRACK_MODE1_RAW,      2352 },
		{ "MODE2",          CHD_TRACK_MODE2,          2336 },
		{ "MODE2/2336",     CHD_TRACK_MODE2,          2336 },
		{ "MODE2_FORM1",    CHD_TRACK_MODE2_FORM1,    2048 },
		{ "MODE2/2048",     CHD_TRACK_MODE2_FORM1,    2048 },
		{ "MODE2_FORM2",    CHD_TRACK_MODE2_FORM2,    2324 },
		{ "MODE2/2324",     CHD_TRACK_MODE2_FORM2,    2324 },
		{ "MODE2_FORM_MIX", CHD_TRACK_MODE2_FORM_MIX, 2336 },
		{ "MODE2_RAW",      CHD_TRACK_MODE2_RAW,      2352 },
		{ "MODE2/2352",     CHD_TRACK_MODE2_RAW,      2352 },
		{ "CDI/2352",       CHD_TRACK_MODE2_RAW,      2352 },
		{ "AUDIO",          CHD_TRACK_AUDIO,          2352 },
	};

	for (INT32 i = 0; i < (INT32)(sizeof(Table) / sizeof(Table[0])); i++) {
		if (!strcmp(szType, Table[i].szName)) {
			*pnType     = Table[i].nType;
			*pnDataSize = Table[i].nDataSize;
			return;
		}
	}
}

static INT32 ChdSubSizeFromString(const char* szSub)
{
	if (!strcmp(szSub, "RW") || !strcmp(szSub, "RW_RAW")) {
		return 96;
	}
	return 0;
}

// Human-readable names for debug output.
const TCHAR* ChdContainerName(INT32 nContainer)
{
	switch (nContainer) {
		case CHD_CONTAINER_CD:    return _T("CD");
		case CHD_CONTAINER_GDROM: return _T("GD-ROM");
		case CHD_CONTAINER_HDD:   return _T("HDD");
		default:                  return _T("unknown");
	}
}

const TCHAR* ChdTrackTypeName(INT32 nType)
{
	switch (nType) {
		case CHD_TRACK_MODE1:          return _T("MODE1");
		case CHD_TRACK_MODE1_RAW:      return _T("MODE1_RAW");
		case CHD_TRACK_MODE2:          return _T("MODE2");
		case CHD_TRACK_MODE2_FORM1:    return _T("MODE2_FORM1");
		case CHD_TRACK_MODE2_FORM2:    return _T("MODE2_FORM2");
		case CHD_TRACK_MODE2_FORM_MIX: return _T("MODE2_FORM_MIX");
		case CHD_TRACK_MODE2_RAW:      return _T("MODE2_RAW");
		case CHD_TRACK_AUDIO:          return _T("AUDIO");
		default:                       return _T("?");
	}
}

// Fetch a track's metadata string (tries CHTR, then CHT2, then CHGD).
// Returns the parsed metadata tag on success, or 0 on failure.
static UINT32 ChdReadTrackMeta(chd_file* pChd, INT32 nIndex, char* szOut, INT32 nOutLen)
{
	static const UINT32 Tags[] = {
		CDROM_TRACK_METADATA_TAG, CDROM_TRACK_METADATA2_TAG, GDROM_TRACK_METADATA_TAG
	};

	for (INT32 i = 0; i < 3; i++) {
		UINT32 nResultLen = 0;
		UINT8  nFlags     = 0;
		chd_error err = chd_get_metadata(pChd, Tags[i], nIndex, szOut, nOutLen - 1,
			&nResultLen, NULL, &nFlags);
		if (err == CHDERR_NONE && nResultLen > 0) {
			szOut[nResultLen] = '\0';
			return Tags[i];
		}
	}
	return 0;
}

// Parse all track metadata into the TOC and compute the three-coordinate
// offsets, mirroring MAME's cdrom_file(chd) constructor.
static INT32 ChdParseToc(ChdImage* pImage)
{
	INT32 nTrk = 0;

	for (; nTrk < CHD_MAX_TRACKS; nTrk++) {
		char szMeta[256];
		UINT32 nTag = ChdReadTrackMeta(pImage->pChd, nTrk, szMeta, sizeof(szMeta));
		if (nTag == 0) {
			break;
		}

		INT32 nTrackNum = -1, nFrames = 0, nPregap = 0, nPostgap = 0, nPad = 0;
		char szType[16] = { 0 }, szSub[16] = { 0 }, szPgType[16] = { 0 }, szPgSub[16] = { 0 };

		if (nTag == CDROM_TRACK_METADATA_TAG) {
			if (sscanf(szMeta, CDROM_TRACK_METADATA_FORMAT,
				&nTrackNum, szType, szSub, &nFrames) != 4) {
				return 1;
			}
		} else if (nTag == CDROM_TRACK_METADATA2_TAG) {
			if (sscanf(szMeta, CDROM_TRACK_METADATA2_FORMAT,
				&nTrackNum, szType, szSub, &nFrames, &nPregap, szPgType, szPgSub, &nPostgap) != 8) {
				return 1;
			}
		} else {
			if (sscanf(szMeta, GDROM_TRACK_METADATA_FORMAT,
				&nTrackNum, szType, szSub, &nFrames, &nPad, &nPregap, szPgType, szPgSub, &nPostgap) != 9) {
				return 1;
			}
			pImage->nFlags |= CD_FLAG_GDROM;
		}

		if (nTrackNum < 1 || nTrackNum > CHD_MAX_TRACKS) {
			return 1;
		}

		ChdTrack* pT = &pImage->Tracks[nTrackNum - 1];
		pT->nType     = CHD_TRACK_MODE1;
		pT->nDataSize = 0;
		ChdTypeFromString(szType, &pT->nType, &pT->nDataSize);
		if (pT->nDataSize == 0) {
			return 1;
		}

		pT->nSubSize  = ChdSubSizeFromString(szSub);
		pT->nSubType  = (pT->nSubSize != 0) ? 1 : 0;
		pT->nFrames   = nFrames;
		pT->nPregap   = nPregap;
		pT->nPostgap  = nPostgap;
		pT->nPadFrames = nPad;
		// chdman pads each track up to a 4-frame boundary in the CHD.
		INT32 nPadded = (nFrames + CHD_TRACK_PADDING - 1) / CHD_TRACK_PADDING;
		pT->nExtraFrames = nPadded * CHD_TRACK_PADDING - nFrames;
		pT->nControl  = (pT->nType == CHD_TRACK_AUDIO) ? 0x01 : 0x41;
	}

	if (nTrk == 0) {
		return 1;
	}
	pImage->nNumTracks = nTrk;

	// Compute physical / chd / logical frame offsets (MAME cdrom_file(chd)).
	INT32 nPhysOfs = 0, nChdOfs = 0, nLogOfs = 0;
	for (INT32 i = 0; i < nTrk; i++) {
		ChdTrack* pT = &pImage->Tracks[i];
		pT->nLogFrameOfs = 0;

		if (pT->nDataSize != 0) {
			// pregap data lives in the CHD, offset this track to index 1
			pT->nLogFrameOfs = pT->nPregap;
		}

		pT->nPhysFrameOfs = nPhysOfs;
		pT->nChdFrameOfs  = nChdOfs;
		pT->nLogFrameOfs += nLogOfs;
		pT->nLogFrames    = pT->nFrames - pT->nPregap;

		nLogOfs  += pT->nPostgap;
		nPhysOfs += pT->nFrames;
		nChdOfs  += pT->nFrames;
		nChdOfs  += pT->nExtraFrames;   // 4-frame boundary padding (CD)
		nChdOfs  += pT->nPadFrames;     // explicit PAD (GD-ROM)
		nLogOfs  += pT->nFrames;
	}

	// dummy lead-out entry for range searches
	ChdTrack* pEnd = &pImage->Tracks[nTrk];
	pEnd->nPhysFrameOfs = nPhysOfs;
	pEnd->nLogFrameOfs  = nLogOfs;
	pEnd->nChdFrameOfs  = nChdOfs;
	pEnd->nLogFrames    = 0;

	pImage->nTotalFrames = nLogOfs;
	return 0;
}

static INT32 ChdDetectContainer(ChdImage* pImage)
{
	char szMeta[256];
	if (ChdReadTrackMeta(pImage->pChd, 0, szMeta, sizeof(szMeta)) != 0) {
		return (pImage->nFlags & CD_FLAG_GDROM) ? CHD_CONTAINER_GDROM : CHD_CONTAINER_CD;
	}

	UINT32 nResultLen = 0;
	UINT8  nFlags     = 0;
	if (chd_get_metadata(pImage->pChd, HARD_DISK_METADATA_TAG, 0, szMeta, sizeof(szMeta) - 1,
		&nResultLen, NULL, &nFlags) == CHDERR_NONE) {
		return CHD_CONTAINER_HDD;
	}
	return CHD_CONTAINER_NONE;
}

ChdImage* ChdOpenFile(const TCHAR* szPath)
{
	if (!szPath || _tcslen(szPath) < 1) {
		return NULL;
	}

	FILE* pFile = _tfopen(szPath, _T("rb"));
	if (!pFile) {
		return NULL;
	}

	chd_file* pChd = NULL;
	if (chd_open_file(pFile, CHD_OPEN_READ, NULL, &pChd) != CHDERR_NONE) {
		fclose(pFile);
		return NULL;
	}

	ChdImage* pImage = (ChdImage*)calloc(1, sizeof(ChdImage));
	if (!pImage) {
		chd_close(pChd);
		fclose(pFile);
		return NULL;
	}
	pImage->pChd        = pChd;
	pImage->pFile       = pFile;
	pImage->nCachedHunk = -1;

	const chd_header* pHeader = chd_get_header(pChd);
	if (!pHeader) {
		ChdClose(pImage);
		return NULL;
	}
	pImage->nHunkBytes     = (INT32)pHeader->hunkbytes;
	pImage->nFramesPerHunk = pImage->nHunkBytes / CHD_FRAME_SIZE;
	pImage->nVersion       = (INT32)pHeader->version;

	pImage->nContainer = ChdDetectContainer(pImage);

	if (pImage->nContainer == CHD_CONTAINER_CD || pImage->nContainer == CHD_CONTAINER_GDROM) {
		if (pImage->nHunkBytes % CHD_FRAME_SIZE != 0 || pImage->nFramesPerHunk == 0) {
			ChdClose(pImage);
			return NULL;
		}
		if (ChdParseToc(pImage) != 0) {
			ChdClose(pImage);
			return NULL;
		}
	}

	pImage->pHunkBuf = (UINT8*)malloc(pImage->nHunkBytes);
	if (!pImage->pHunkBuf) {
		ChdClose(pImage);
		return NULL;
	}

	return pImage;
}

void ChdClose(ChdImage* pImage)
{
	if (!pImage) {
		return;
	}
	if (pImage->pChd) {
		chd_close(pImage->pChd);
	}
	if (pImage->pFile) {
		fclose(pImage->pFile);   // libchdr's core_stdio_nonowner does not close it
	}
	free_s((void**)&pImage->pHunkBuf);
	free(pImage);
}

INT32 ChdGetContainerType(ChdImage* pImage) { return pImage ? pImage->nContainer : CHD_CONTAINER_NONE; }
INT32 ChdGetNumTracks(ChdImage* pImage)     { return pImage ? pImage->nNumTracks : 0; }
INT32 ChdGetTotalFrames(ChdImage* pImage)   { return pImage ? pImage->nTotalFrames : 0; }
INT32 ChdGetVersion(ChdImage* pImage)       { return pImage ? pImage->nVersion : 0; }
INT32 ChdGetHunkBytes(ChdImage* pImage)     { return pImage ? pImage->nHunkBytes : 0; }
INT32 ChdGetFramesPerHunk(ChdImage* pImage) { return pImage ? pImage->nFramesPerHunk : 0; }

const ChdTrack* ChdGetTrack(ChdImage* pImage, INT32 nTrack)
{
	if (!pImage || nTrack < 0 || nTrack >= pImage->nNumTracks) {
		return NULL;
	}
	return &pImage->Tracks[nTrack];
}

// Copy nLength bytes from CHD frame nChdFrame at byte nOffset into pDest,
// decompressing the covering hunk on cache miss.
static INT32 ChdReadFrameBytes(ChdImage* pImage, INT32 nChdFrame, INT32 nOffset, INT32 nLength, UINT8* pDest)
{
	if (!pImage || !pImage->pHunkBuf || pImage->nFramesPerHunk == 0) {
		return 1;
	}

	INT32 nHunk = nChdFrame / pImage->nFramesPerHunk;
	INT32 nFrameInHunk = nChdFrame % pImage->nFramesPerHunk;

	if (pImage->nCachedHunk != nHunk) {
		if (chd_read(pImage->pChd, (UINT32)nHunk, pImage->pHunkBuf) != CHDERR_NONE) {
			return 1;
		}
		pImage->nCachedHunk = nHunk;
	}

	memcpy(pDest, pImage->pHunkBuf + nFrameInHunk * CHD_FRAME_SIZE + nOffset, nLength);
	return 0;
}

INT32 ChdReadRaw(ChdImage* pImage, INT32 nChdFrame, INT32 nOffset, INT32 nLength, UINT8* pDest)
{
	return ChdReadFrameBytes(pImage, nChdFrame, nOffset, nLength, pDest);
}

// logical LBA -> (track, chd frame), MAME logical_to_chd_lba
static INT32 ChdLogicalToChd(ChdImage* pImage, INT32 nLogLba, INT32* pnTrack)
{
	for (INT32 i = 0; i < pImage->nNumTracks; i++) {
		if (nLogLba < pImage->Tracks[i + 1].nLogFrameOfs) {
			INT32 nPhys = pImage->Tracks[i].nPhysFrameOfs + (nLogLba - pImage->Tracks[i].nLogFrameOfs);
			INT32 nChd  = nPhys - pImage->Tracks[i].nPhysFrameOfs + pImage->Tracks[i].nChdFrameOfs;
			*pnTrack = i;
			return nChd;
		}
	}
	*pnTrack = 0;
	return nLogLba;
}

INT32 ChdReadSector(ChdImage* pImage, INT32 nLba, INT32 nDataType, UINT8* pDest)
{
	if (!pImage || pImage->nNumTracks == 0) {
		return 1;
	}

	INT32 nTrack = 0;
	INT32 nChdFrame = ChdLogicalToChd(pImage, nLba, &nTrack);
	ChdTrack* pT = &pImage->Tracks[nTrack];
	INT32 nTrackType = pT->nType;

	// pregap not physically present: hand back zeros
	if (pT->nDataSize != 0 && nLba < pT->nLogFrameOfs) {
		memset(pDest, 0, (nDataType == CHD_TRACK_RAW_DONTCARE) ? pT->nDataSize : 2352);
		return 0;
	}

	if (nDataType == nTrackType || nDataType == CHD_TRACK_RAW_DONTCARE) {
		return ChdReadFrameBytes(pImage, nChdFrame, 0, pT->nDataSize, pDest);
	}

	// 2048 mode-1 data out of a 2352 mode-1 raw sector
	if (nDataType == CHD_TRACK_MODE1 && nTrackType == CHD_TRACK_MODE1_RAW) {
		return ChdReadFrameBytes(pImage, nChdFrame, 16, 2048, pDest);
	}

	// 2352 mode-1 raw sector out of 2048 mode-1 data (synthesize sync + header)
	if (nDataType == CHD_TRACK_MODE1_RAW && nTrackType == CHD_TRACK_MODE1) {
		static const UINT8 SyncBytes[12] = { 0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00 };
		// Header address is the absolute MSF (BCD), which includes the
		// standard 150-frame (2 second) lead-in, matching real raw sectors.
		UINT32 nMsf = ChdLbaToMsf(nLba + 150);
		memcpy(pDest, SyncBytes, 12);
		pDest[12] = (UINT8)(nMsf >> 16);
		pDest[13] = (UINT8)(nMsf >> 8);
		pDest[14] = (UINT8)(nMsf >> 0);
		pDest[15] = 1;
		return ChdReadFrameBytes(pImage, nChdFrame, 0, 2048, pDest + 16);
	}

	// 2048 mode-1 data out of a mode-2 form1 or raw sector
	if (nDataType == CHD_TRACK_MODE1 &&
		(nTrackType == CHD_TRACK_MODE2_FORM1 || nTrackType == CHD_TRACK_MODE2_RAW)) {
		return ChdReadFrameBytes(pImage, nChdFrame, 24, 2048, pDest);
	}

	// 2048 mode-1 data out of a mode-2 form-mix sector
	if (nDataType == CHD_TRACK_MODE1 && nTrackType == CHD_TRACK_MODE2_FORM_MIX) {
		return ChdReadFrameBytes(pImage, nChdFrame, 8, 2048, pDest);
	}

	// 2336 mode-2 data out of a 2352 raw sector (skip the header)
	if (nDataType == CHD_TRACK_MODE2 &&
		(nTrackType == CHD_TRACK_MODE1_RAW || nTrackType == CHD_TRACK_MODE2_RAW)) {
		return ChdReadFrameBytes(pImage, nChdFrame, 16, 2336, pDest);
	}

	return 1;
}

INT32 ChdCountAudioTracks(const TCHAR* szPath)
{
	ChdImage* pImage = ChdOpenFile(szPath);
	if (!pImage) {
		return 0;
	}

	INT32 nCount = 0;
	for (INT32 i = 0; i < pImage->nNumTracks; i++) {
		if (pImage->Tracks[i].nType == CHD_TRACK_AUDIO) {
			nCount++;
		}
	}

	ChdClose(pImage);
	return nCount;
}
