// only internal functions, no deps to interface.cpp/cd_interface.cpp

const INT32 MAXIMUM_NUMBER_TRACKS = 99;

const INT32 CD_FRAMES_MINUTE = 60 * 75;
const INT32 CD_FRAMES_SECOND =      75;

const INT32 CD_TYPE_NONE     = 1 << 0;
const INT32 CD_TYPE_BINCUE   = 1 << 1;
const INT32 CD_TYPE_CCD      = 1 << 2;
const INT32 CD_TYPE_CHD      = 1 << 3;

struct CDImageFile {
	TCHAR szPath[MAX_PATH];
	FILE* pFile;
	INT64 nSectors;
};

struct CDImage {
	TCHAR szPath[MAX_PATH];
	INT32 nType;
	INT32 nTrackCount;
	INT32 nAudioTrackCount;
	INT32 nSectorCount;
	INT32 nFileCount;
	INT32 nSourceFileCount;
	TCHAR SourceFiles[CDIMAGE_MAX_SOURCE_FILES][MAX_PATH];
	CDImageFile  Files[MAXIMUM_NUMBER_TRACKS];
	CDImageTrack Tracks[MAXIMUM_NUMBER_TRACKS];
	INT32 nFile[MAXIMUM_NUMBER_TRACKS];
	INT32 nFileLBA[MAXIMUM_NUMBER_TRACKS];
	INT32 nIndex0FileLBA[MAXIMUM_NUMBER_TRACKS];
	INT32 nExplicitPregap[MAXIMUM_NUMBER_TRACKS];
	INT32 nChdType[MAXIMUM_NUMBER_TRACKS];
	ChdImage* pChd;
};

static INT32 cdimgCopyPath(TCHAR* dst, size_t count, const TCHAR* src);
static INT32 cdimgReplaceExtension(TCHAR* path, size_t count, const TCHAR* extension);

static INT32 CDImageAddSourceFile(CDImage* pImage, const TCHAR* szPath)
{
	if (!pImage || !szPath || !*szPath)
		return 1;

	for (INT32 i = 0; i < pImage->nSourceFileCount; i++)
		if (!_tcsicmp(pImage->SourceFiles[i], szPath))
			return 0;

	if (pImage->nSourceFileCount >= CDIMAGE_MAX_SOURCE_FILES || cdimgCopyPath(pImage->SourceFiles[pImage->nSourceFileCount], MAX_PATH, szPath))
		return 1;

	pImage->nSourceFileCount++;
	return 0;
}

static INT32 CDImageParseMSF(const TCHAR* szText, INT32* pnLba)
{
	TCHAR* pEnd;
	INT32  nMinute = _tcstol(szText, &pEnd, 10);
	if (pEnd == szText || *pEnd++ != _T(':'))
		return 1;

	TCHAR* pSecond = pEnd;
	INT32  nSecond = _tcstol(pSecond, &pEnd, 10);
	if (pEnd == pSecond || *pEnd++ != _T(':'))
		return 1;

	TCHAR* pFrame = pEnd;
	INT32 nFrame  = _tcstol(pFrame, &pEnd, 10);
	while (*pEnd == _T(' ') || *pEnd == _T('\t'))
		pEnd++;

	if (pEnd == pFrame || *pEnd || nMinute < 0 || nSecond < 0 || nSecond > 59 || nFrame < 0 || nFrame > 74)
		return 1;

	*pnLba = nMinute * CD_FRAMES_MINUTE + nSecond * CD_FRAMES_SECOND + nFrame;
	return 0;
}

static INT32 CDImageResolvePath(TCHAR* szDest, const TCHAR* szImage, const TCHAR* szFile)
{
	if (!szDest || !szImage || !szFile)
		return 1;

	if (szFile[0] == _T('/') || szFile[0] == _T('\\') || (_tcslen(szFile) > 1 && szFile[1] == _T(':'))) {
		return cdimgCopyPath(szDest, MAX_PATH, szFile);
	}
	const TCHAR* pSlash     = _tcsrchr(szImage, _T('/'));
	const TCHAR* pBackslash = _tcsrchr(szImage, _T('\\'));
	if (!pSlash || (pBackslash && pBackslash > pSlash))
		pSlash = pBackslash;

	size_t nDirectory = pSlash ? (size_t)(pSlash - szImage + 1) : 0;
	if (nDirectory + _tcslen(szFile) >= MAX_PATH)
		return 1;

	if (nDirectory)
		memcpy(szDest, szImage, nDirectory * sizeof(TCHAR));

	_tcscpy(szDest + nDirectory, szFile);
	return 0;
}

static void CDImageCloseFiles(CDImage* pImage)
{
	if (!pImage)
		return;

	for (INT32 i = 0; i < pImage->nFileCount; i++) {
		if (pImage->Files[i].pFile)
			fclose(pImage->Files[i].pFile);
		pImage->Files[i].pFile = NULL;
	}
}

static INT32 CDImageOpenBinary(CDImageFile* pFile)
{
	pFile->pFile = _tfopen(pFile->szPath, _T("rb"));
	if (!pFile->pFile)
		return 1;

	if (cdimgFseek(pFile->pFile, 0, SEEK_END)) {
		fclose(pFile->pFile);
		pFile->pFile = NULL;
		return 1;
	}
	INT64 nSize = cdimgFtell(pFile->pFile);
	if (nSize < 0 || cdimgFseek(pFile->pFile, 0, SEEK_SET)) {
		fclose(pFile->pFile);
		pFile->pFile = NULL;
		return 1;
	}
	pFile->nSectors = nSize;
	return 0;
}

static INT32 CDImageParseCue(CDImage* pImage)
{
	FILE* pCue = _tfopen(pImage->szPath, _T("rt"));
	if (!pCue)
		return 1;

	TCHAR szLine[1024];
	INT32 nFile        = -1;
	INT32 nTrack       = -1;
	INT32 nDiscBase    = 0;
	INT32 nCurrentFile = -1;
	INT32 nFileBase    = 0;
	INT32 nVirtualGap  = 0;
	UINT8 Index1Seen[MAXIMUM_NUMBER_TRACKS] = { 0 };
	for (INT32 i = 0; i < MAXIMUM_NUMBER_TRACKS; i++)
		pImage->nIndex0FileLBA[i] = -1;

	while (_fgetts(szLine, ARRAY_SIZE(szLine), pCue)) {
		INT32 nLength = (INT32)_tcslen(szLine);
		while (nLength && (szLine[nLength - 1] == _T('\r') || szLine[nLength - 1] == _T('\n')))
			szLine[--nLength] = 0;

		TCHAR* p = szLine;
		TCHAR* pLabel;
		if ((pLabel = LabelCheck(p, _T("FILE"))) != NULL) {
			TCHAR* pName = NULL;
			TCHAR* pEnd  = NULL;
			if (pImage->nFileCount >= MAXIMUM_NUMBER_TRACKS || QuoteRead(&pName, &pEnd, pLabel) || !pName || !pEnd)
				goto fail;

			while (*pEnd == _T(' ') || *pEnd == _T('\t'))
				pEnd++;
			if (_tcsncmp(pEnd, _T("BINARY"), 6))
				goto fail;

			pEnd += 6;
			while (*pEnd == _T(' ') || *pEnd == _T('\t'))
				pEnd++;
			if (*pEnd)
				goto fail;

			nFile = pImage->nFileCount++;
			if (CDImageResolvePath(pImage->Files[nFile].szPath, pImage->szPath, pName) || CDImageOpenBinary(&pImage->Files[nFile]))
				goto fail;

			continue;
		}
		if ((pLabel = LabelCheck(p, _T("TRACK"))) != NULL) {
			if (nFile < 0 || pImage->nTrackCount >= MAXIMUM_NUMBER_TRACKS)
				goto fail;
			TCHAR* pEnd;
			INT32  nNumber = _tcstol(pLabel, &pEnd, 10);
			if (nNumber != pImage->nTrackCount + 1)
				goto fail;

			while (*pEnd == _T(' ') || *pEnd == _T('\t')) pEnd++;
			nTrack = pImage->nTrackCount++;
			CDImageTrack* pTrack = &pImage->Tracks[nTrack];
			pTrack->nNumber    = nNumber;
			pTrack->nIndex0LBA = -1;
			pTrack->nControl   = 0x41;
			if (!_tcscmp(pEnd, _T("AUDIO"))) {
				pTrack->nType       = CDIMAGE_TRACK_AUDIO;
				pTrack->nControl    = 0x01;
				pTrack->nSectorSize = pTrack->nUserSize = 2352;
				pImage->nAudioTrackCount++;
			} else if (!_tcscmp(pEnd, _T("MODE1/2048"))) {
				pTrack->nType       = CDIMAGE_TRACK_MODE1_2048;
				pTrack->nSectorSize = pTrack->nUserSize = 2048;
				pTrack->nUserOffset = 16;
			} else if (!_tcscmp(pEnd, _T("MODE1/2352"))) {
				pTrack->nType       = CDIMAGE_TRACK_MODE1_2352;
				pTrack->nSectorSize = 2352;
				pTrack->nUserOffset = 16;
				pTrack->nUserSize   = 2048;
			} else if (!_tcscmp(pEnd, _T("MODE2/2336"))) {
				pTrack->nType       = CDIMAGE_TRACK_MODE2_2336;
				pTrack->nSectorSize = pTrack->nUserSize = 2336;
				pTrack->nUserOffset = 16;
			} else if (!_tcscmp(pEnd, _T("MODE2/2352")) || !_tcscmp(pEnd, _T("CDI/2352"))) {
				pTrack->nType       = CDIMAGE_TRACK_MODE2_2352;
				pTrack->nSectorSize = 2352;
				pTrack->nUserOffset = 16;
				pTrack->nUserSize   = 2336;
			} else
				goto fail;

			pImage->nFile[nTrack] = nFile;
			pTrack->szPath = pImage->Files[nFile].szPath;
			continue;
		}
		if ((pLabel = LabelCheck(p, _T("INDEX 00"))) != NULL) {
			if (nTrack < 0 || CDImageParseMSF(pLabel, &pImage->nIndex0FileLBA[nTrack]))
				goto fail;

			continue;
		}
		if ((pLabel = LabelCheck(p, _T("INDEX 01"))) != NULL) {
			if (nTrack < 0 || Index1Seen[nTrack] || CDImageParseMSF(pLabel, &pImage->nFileLBA[nTrack]))
				goto fail;

			Index1Seen[nTrack] = 1;
			continue;
		}
		if ((pLabel = LabelCheck(p, _T("PREGAP"))) != NULL) {
			if (nTrack < 0 || CDImageParseMSF(pLabel, &pImage->nExplicitPregap[nTrack]))
				goto fail;
		}
	}
	fclose(pCue);
	if (!pImage->nTrackCount)
		return 1;

	for (INT32 i = 0; i < pImage->nTrackCount; i++)
		if (!Index1Seen[i])
			return 1;

	for (INT32 i = 0; i < pImage->nFileCount; i++) {
		INT32 nSectorSize = 0;
		for (INT32 j = 0; j < pImage->nTrackCount; j++) {
			if (pImage->nFile[j] != i)
				continue;
			if (nSectorSize && nSectorSize != pImage->Tracks[j].nSectorSize)
				return 1;
			nSectorSize = pImage->Tracks[j].nSectorSize;
		}

		if (!nSectorSize || pImage->Files[i].nSectors % nSectorSize)
			return 1;

		pImage->Files[i].nSectors /= nSectorSize;
	}
	for (INT32 i = 0; i < pImage->nTrackCount; i++) {
		if (pImage->nFile[i] != nCurrentFile) {
			nCurrentFile = pImage->nFile[i];
			nFileBase    = nDiscBase;
			nVirtualGap  = 0;
		}
		INT32 nPreviousVirtualGap = nVirtualGap;
		nVirtualGap += pImage->nExplicitPregap[i];
		CDImageTrack* pTrack = &pImage->Tracks[i];
		pTrack->nIndex1LBA = nFileBase + pImage->nFileLBA[i] + nVirtualGap;
		pTrack->nStartLBA  = pTrack->nIndex1LBA;
		if (pImage->nIndex0FileLBA[i] >= 0) {
			if (pImage->nIndex0FileLBA[i] > pImage->nFileLBA[i])
				return 1;
			pTrack->nIndex0LBA = nFileBase + pImage->nIndex0FileLBA[i] + nPreviousVirtualGap;
		} else {
			pTrack->nIndex0LBA = pTrack->nIndex1LBA - pImage->nExplicitPregap[i];
		}

		pTrack->nPregap = pTrack->nIndex1LBA - pTrack->nIndex0LBA;
		nDiscBase = nFileBase + (INT32)pImage->Files[nCurrentFile].nSectors + nVirtualGap;
	}
	pImage->nSectorCount = nDiscBase;
	for (INT32 i = 0; i < pImage->nTrackCount; i++) {
		INT32 nEnd = i + 1 < pImage->nTrackCount ? pImage->Tracks[i + 1].nIndex0LBA : pImage->nSectorCount;
		pImage->Tracks[i].nSectors = nEnd - pImage->Tracks[i].nStartLBA;
	}
	return 0;

fail:
	fclose(pCue);
	return 1;
}

static inline UINT8 CDImageBcd(UINT8 nValue)
{
	return ((nValue >> 4) * 10) + (nValue & 15);
}

static INT32 CDImageParseCcd(CDImage* pImage)
{
	TCHAR szSub[MAX_PATH];
	if (cdimgCopyPath(szSub, MAX_PATH, pImage->szPath) || cdimgReplaceExtension(szSub, MAX_PATH, _T(".sub")))
		return 1;
	if (pImage->nFileCount >= 1 || cdimgCopyPath(pImage->Files[0].szPath, MAX_PATH, pImage->szPath) || cdimgReplaceExtension(pImage->Files[0].szPath, MAX_PATH, _T(".img")) || CDImageOpenBinary(&pImage->Files[0]))
		return 1;

	pImage->nFileCount = 1;
	FILE* pSub = _tfopen(szSub, _T("rb"));
	if (!pSub || cdimgFseek(pSub, 0, SEEK_END)) {
		if (pSub)
			fclose(pSub);

		return 1;
	}
	INT64 nSize = cdimgFtell(pSub);
	if (nSize < 96 || nSize % 96 || cdimgFseek(pSub, 0, SEEK_SET)) {
		fclose(pSub);
		return 1;
	}
	UINT8 Sub[96];
	INT32 nLastTrack  = 0;
	INT32 nIndex1Base = -1;
	while (fread(Sub, 1, sizeof(Sub), pSub) == sizeof(Sub)) {
		UINT8* pQ = Sub + 12;
		INT32  nAbsolute = CDImageBcd(pQ[7]) * CD_FRAMES_MINUTE + CDImageBcd(pQ[8]) * CD_FRAMES_SECOND + CDImageBcd(pQ[9]);
		INT32  nTrack    = CDImageBcd(pQ[1]);
		INT32  nIndex    = CDImageBcd(pQ[2]);
		if (nIndex == 1 && nTrack == nLastTrack + 1 && nTrack <= MAXIMUM_NUMBER_TRACKS) {
			CDImageTrack* pTrack = &pImage->Tracks[nLastTrack];
			pTrack->nNumber     = nTrack; pTrack->nControl = pQ[0]; pTrack->nType = (pQ[0] & 0x40) ? CDIMAGE_TRACK_MODE1_2352 : CDIMAGE_TRACK_AUDIO;
			pTrack->nSectorSize = 2352; pTrack->nUserOffset = pTrack->nType == CDIMAGE_TRACK_AUDIO ? 0 : 16; pTrack->nUserSize = pTrack->nType == CDIMAGE_TRACK_AUDIO ? 2352 : 2048;
			if (nIndex1Base < 0)
				nIndex1Base = nAbsolute;
			pTrack->nStartLBA   = pTrack->nIndex1LBA = nAbsolute - nIndex1Base;
			pTrack->nIndex0LBA  = pTrack->nStartLBA; pTrack->szPath = pImage->Files[0].szPath; pImage->nFile[nLastTrack] = 0; pImage->nFileLBA[nLastTrack] = nAbsolute - nIndex1Base;
			if (pTrack->nType == CDIMAGE_TRACK_AUDIO)
				pImage->nAudioTrackCount++;

			nLastTrack++;
		}
	}
	fclose(pSub);
	pImage->nTrackCount  = nLastTrack;
	pImage->Files[0].nSectors /= 2352;
	pImage->nSectorCount = (INT32)pImage->Files[0].nSectors;
	for (INT32 i = 0; i < nLastTrack; i++)
		pImage->Tracks[i].nSectors = (i + 1 < nLastTrack ? pImage->Tracks[i + 1].nStartLBA : pImage->nSectorCount) - pImage->Tracks[i].nStartLBA;

	return nLastTrack ? 0 : 1;
}

static INT32 CDImageParseRaw(CDImage* pImage)
{
	pImage->nFileCount = 1;
	if (cdimgCopyPath(pImage->Files[0].szPath, MAX_PATH, pImage->szPath) || CDImageOpenBinary(&pImage->Files[0]) || pImage->Files[0].nSectors % 2352)
		return 1;

	pImage->Files[0].nSectors /= 2352;
	pImage->nTrackCount  = 1;
	pImage->nSectorCount = (INT32)pImage->Files[0].nSectors;

	CDImageTrack* pTrack = &pImage->Tracks[0];
	pTrack->nNumber     = 1;
	pTrack->nType       = CDIMAGE_TRACK_MODE1_2352;
	pTrack->nControl    = 0x41;
	pTrack->nIndex0LBA  = pTrack->nIndex1LBA = pTrack->nStartLBA = 0;
	pTrack->nSectors    = pImage->nSectorCount;
	pTrack->nSectorSize = 2352;
	pTrack->nUserOffset = 16;
	pTrack->nUserSize   = 2048;
	pTrack->szPath      = pImage->Files[0].szPath;
	return 0;
}

static INT32 CDImageCollectSourceFiles(CDImage* pImage)
{
	if (CDImageAddSourceFile(pImage, pImage->szPath))
		return 1;

	for (INT32 i = 0; i < pImage->nFileCount; i++)
		if (CDImageAddSourceFile(pImage, pImage->Files[i].szPath))
			return 1;

	if (pImage->nType == CD_TYPE_CCD) {
		TCHAR szSub[MAX_PATH];
		if (cdimgCopyPath(szSub, MAX_PATH, pImage->szPath) || cdimgReplaceExtension(szSub, MAX_PATH, _T(".sub")) || CDImageAddSourceFile(pImage, szSub))
			return 1;
	}
	return 0;
}

static INT32 CDImageParseChd(CDImage* pImage)
{
	pImage->pChd = ChdOpenFile(pImage->szPath);
	if (!pImage->pChd || (ChdGetContainerType(pImage->pChd) != CHD_CONTAINER_CD && ChdGetContainerType(pImage->pChd) != CHD_CONTAINER_GDROM))
		return 1;

	pImage->nTrackCount  = ChdGetNumTracks(pImage->pChd);
	pImage->nSectorCount = ChdGetTotalFrames(pImage->pChd);
	if (pImage->nTrackCount < 1 || pImage->nTrackCount > MAXIMUM_NUMBER_TRACKS)
		return 1;

	for (INT32 i = 0; i < pImage->nTrackCount; i++) {
		const ChdTrack* pChdTrack = ChdGetTrack(pImage->pChd, i);
		CDImageTrack* pTrack = &pImage->Tracks[i];
		pTrack->nNumber     = i + 1; pTrack->nControl = pChdTrack->nControl; pTrack->nStartLBA = pTrack->nIndex1LBA = pChdTrack->nLogFrameOfs;
		pTrack->nIndex0LBA  = pTrack->nIndex1LBA - pChdTrack->nPregap; pTrack->nPregap = pChdTrack->nPregap; pTrack->nSectors = pChdTrack->nLogFrames;
		pTrack->nSectorSize = pChdTrack->nDataSize; pTrack->szPath = pImage->szPath;

		pImage->nChdType[i] = pChdTrack->nType;
		if (pChdTrack->nType == CHD_TRACK_AUDIO) {
			pTrack->nType       = CDIMAGE_TRACK_AUDIO;
			pTrack->nUserSize   = 2352;
			pImage->nAudioTrackCount++;
		} else if (pChdTrack->nType == CHD_TRACK_MODE1) {
			pTrack->nType       = CDIMAGE_TRACK_MODE1_2048;
			pTrack->nUserSize   = 2048;
		} else if (pChdTrack->nType == CHD_TRACK_MODE1_RAW) {
			pTrack->nType = CDIMAGE_TRACK_MODE1_2352;
			pTrack->nUserOffset = 16;
			pTrack->nUserSize   = 2048;
		} else if (pChdTrack->nType == CHD_TRACK_MODE2) {
			pTrack->nType       = CDIMAGE_TRACK_MODE2_2336;
			pTrack->nUserSize   = 2336;
		} else {
			pTrack->nType       = CDIMAGE_TRACK_MODE2_2352;
			pTrack->nUserOffset = pChdTrack->nType == CHD_TRACK_MODE2_RAW ? 16 : 0;
			pTrack->nUserSize   = pChdTrack->nDataSize; }
	}
	for (INT32 i = 0; i < pImage->nTrackCount; i++) {
		pImage->Tracks[i].nSectors = (i + 1 < pImage->nTrackCount ? pImage->Tracks[i + 1].nStartLBA : pImage->nSectorCount) - pImage->Tracks[i].nStartLBA;
	}
	return 0;
}

void CDImageClose(CDImage* pImage)
{
	if (!pImage)
		return;

	CDImageCloseFiles(pImage);
	if (pImage->pChd)
		ChdClose(pImage->pChd);

	free(pImage);
}

CDImage* CDImageOpen(const TCHAR* szPath)
{
	if (!szPath || !*szPath)
		return NULL;

	CDImage* pImage = (CDImage*)calloc(1, sizeof(CDImage));
	if (!pImage || cdimgCopyPath(pImage->szPath, MAX_PATH, szPath)) {
		free(pImage);
		return NULL;
	}

	INT32 nError = 1;
	if (IsFileExt(pImage->szPath, _T(".cue"))) {
		pImage->nType = CD_TYPE_BINCUE;
		nError = CDImageParseCue(pImage);
	} else if (IsFileExt(pImage->szPath, _T(".ccd"))) {
		pImage->nType = CD_TYPE_CCD;
		nError = CDImageParseCcd(pImage);
	} else if (IsFileExt(pImage->szPath, _T(".chd"))) {
		pImage->nType = CD_TYPE_CHD;
		nError = CDImageParseChd(pImage);
	} else if (IsFileExt(pImage->szPath, _T(".bin")) || IsFileExt(pImage->szPath, _T(".img"))) {
		pImage->nType = CD_TYPE_BINCUE;
		nError = CDImageParseRaw(pImage);
	}
	if (nError || CDImageCollectSourceFiles(pImage)) {
		CDImageClose(pImage);
		return NULL;
	}
	return pImage;
}

INT32 CDImageGetTrackCount(const CDImage* pImage)
{
	return pImage ? pImage->nTrackCount : 0;
}

INT32 CDImageGetAudioTrackCount(const CDImage* pImage)
{
	return pImage ? pImage->nAudioTrackCount : 0;
}

INT32 CDImageGetSectorCount(const CDImage* pImage)
{
	return pImage ? pImage->nSectorCount : 0;
}

const TCHAR* CDImageGetPath(const CDImage* pImage)
{
	return pImage ? pImage->szPath : NULL;
}

INT32 CDImageGetSourceFileCount(const CDImage* pImage)
{
	return pImage ? pImage->nSourceFileCount : 0;
}

const TCHAR* CDImageGetSourceFile(const CDImage* pImage, INT32 nFile)
{
	return pImage && nFile >= 0 && nFile < pImage->nSourceFileCount ? pImage->SourceFiles[nFile] : NULL;
}

const CDImageTrack* CDImageGetTrack(const CDImage* pImage, INT32 nTrack)
{
	return pImage && nTrack >= 0 && nTrack < pImage->nTrackCount ? &pImage->Tracks[nTrack] : NULL;
}

static INT32 CDImageFindTrack(const CDImage* pImage, INT32 nLba)
{
	if (!pImage || nLba < 0 || nLba >= pImage->nSectorCount)
		return -1;

	for (INT32 i = pImage->nTrackCount - 1; i >= 0; i--)
		if (nLba >= pImage->Tracks[i].nIndex0LBA)
			return i;

	return -1;
}

INT32 CDImageReadRawSector(CDImage* pImage, INT32 nLba, UINT8* pDest)
{
	if (!pImage || !pDest)
		return 1;

	INT32 nTrack = CDImageFindTrack(pImage, nLba);
	if (nTrack < 0)
		return 1;

	CDImageTrack* pTrack = &pImage->Tracks[nTrack];
	if (pImage->pChd) {
		INT32 nType = pImage->nChdType[nTrack];
		if (nType == CHD_TRACK_AUDIO)
			return ChdReadSector(pImage->pChd, nLba, CHD_TRACK_RAW_DONTCARE, pDest);
		if (nType == CHD_TRACK_MODE1 || nType == CHD_TRACK_MODE1_RAW)
			return ChdReadSector(pImage->pChd, nLba, CHD_TRACK_MODE1_RAW,    pDest);
		if (nType == CHD_TRACK_MODE2_RAW)
			return ChdReadSector(pImage->pChd, nLba, CHD_TRACK_RAW_DONTCARE, pDest);

		memset(pDest, 0, 2352);
		pDest[15] = 2;
		INT32 nOffset = nType == CHD_TRACK_MODE2_FORM1 || nType == CHD_TRACK_MODE2_FORM2 ? 24 : 16;
		return ChdReadSector(pImage->pChd, nLba, CHD_TRACK_RAW_DONTCARE, pDest + nOffset);
	}
	if (nLba < pTrack->nIndex0LBA + pImage->nExplicitPregap[nTrack]) {
		memset(pDest, 0, 2352);
		return 0;
	}
	INT32 nFileLba = pImage->nFileLBA[nTrack] + nLba - pTrack->nIndex1LBA;
	if (nFileLba < 0)
		return 1;

	CDImageFile* pFile = &pImage->Files[pImage->nFile[nTrack]];
	if (cdimgFseek(pFile->pFile, (INT64)nFileLba * pTrack->nSectorSize, SEEK_SET))
		return 1;

	if (pTrack->nSectorSize == 2352)
		return fread(pDest, 1, 2352, pFile->pFile) == 2352 ? 0 : 1;

	memset(pDest, 0, 2352);
	pDest[15] = pTrack->nType == CDIMAGE_TRACK_MODE2_2336 ? 2 : 1;
	return fread(pDest + 16, 1, pTrack->nSectorSize, pFile->pFile) == (size_t)pTrack->nSectorSize ? 0 : 1;
}

INT32 CDImageReadDataSector(CDImage* pImage, INT32 nLba, UINT8* pDest)
{
	if (!pImage || !pDest)
		return 1;

	INT32 nTrack = CDImageFindTrack(pImage, nLba);
	if (nTrack < 0)
		return 1;

	CDImageTrack* pTrack = &pImage->Tracks[nTrack];
	if (pTrack->nType == CDIMAGE_TRACK_AUDIO)
		return 1;

	UINT8 Raw[2352];
	if (pImage->pChd) {
		switch (pImage->nChdType[nTrack]) {
			case CHD_TRACK_MODE1:
			case CHD_TRACK_MODE1_RAW:
				return ChdReadSector(pImage->pChd, nLba, CHD_TRACK_MODE1, pDest);

			case CHD_TRACK_MODE2_FORM1:
				return ChdReadSector(pImage->pChd, nLba, CHD_TRACK_MODE2_FORM1, pDest);

			case CHD_TRACK_MODE2_FORM2:
				return 1;

			case CHD_TRACK_MODE2:
			case CHD_TRACK_MODE2_FORM_MIX:
				if (ChdReadSector(pImage->pChd, nLba, CHD_TRACK_RAW_DONTCARE, Raw) || (Raw[2] & 0x20))
					return 1;
				memcpy(pDest, Raw + 8, 2048);
				return 0;

			case CHD_TRACK_MODE2_RAW:
				if (ChdReadSector(pImage->pChd, nLba, CHD_TRACK_MODE2_RAW, Raw) || Raw[15] != 2 || (Raw[18] & 0x20))
					return 1;
				memcpy(pDest, Raw + 24, 2048);
				return 0;
		}
		return 1;
	}

	if (CDImageReadRawSector(pImage, nLba, Raw))
		return 1;

	if (Raw[15] == 2) {
		if (Raw[18] & 0x20)
			return 1;
		memcpy(pDest, Raw + 24, 2048);
		return 0;
	}
	if (pTrack->nType == CDIMAGE_TRACK_MODE2_2336 || pTrack->nType == CDIMAGE_TRACK_MODE2_2352)
		return 1;

	memcpy(pDest, Raw + 16, 2048);
	return 0;
}

INT32 CDImageReadUserSector(CDImage* pImage, INT32 nLba, UINT8* pDest, INT32* pnSize)
{
	if (!pImage || !pDest)
		return 1;

	INT32 nTrack = CDImageFindTrack(pImage, nLba);
	if (nTrack < 0)
		return 1;

	CDImageTrack* pTrack = &pImage->Tracks[nTrack];
	if (pTrack->nType == CDIMAGE_TRACK_AUDIO)
		return 1;

	UINT8 Raw[2352];
	if (pImage->pChd) {
		INT32 nType = pImage->nChdType[nTrack];
		if (pTrack->nUserSize == 2048 && (nType == CHD_TRACK_MODE1 || nType == CHD_TRACK_MODE1_RAW))
			nType = CHD_TRACK_MODE1;
		else if (pTrack->nUserSize == 2336)
			nType = CHD_TRACK_MODE2;
		else
			nType = CHD_TRACK_RAW_DONTCARE;

		if (ChdReadSector(pImage->pChd, nLba, nType, pDest))
			return 1;
	} else {
		if (CDImageReadRawSector(pImage, nLba, Raw))
			return 1;

		memcpy(pDest, Raw + pTrack->nUserOffset, pTrack->nUserSize);
	}
	if (pnSize)
		*pnSize = pTrack->nUserSize;

	return 0;
}

INT32 CDImageGetChdSha1(const CDImage* pImage, UINT8* pSha1)
{
	return pImage && pImage->pChd ? ChdGetSha1(pImage->pChd, pSha1) : 1;
}

static CDImage* cdimgImage = NULL;
static INT32    cd_pregap;
static double   cd_volume  = 100.0;

struct MSF {
	UINT8 M;
	UINT8 S;
	UINT8 F;
};

struct cdimgTRACK_DATA {
	UINT8 Control;
	UINT8 TrackNumber;
	UINT8 Address[4];
	UINT8 EndAddress[4];
};

struct cdimgCDROM_TOC {
	UINT8 FirstTrack;
	UINT8 LastTrack;
	UINT8 ImageType;
	TCHAR Image[MAX_PATH];
	cdimgTRACK_DATA TrackData[MAXIMUM_NUMBER_TRACKS + 1];
};

static cdimgCDROM_TOC* cdimgTOC;

static FILE*     cdimgFile    = NULL;
static ChdImage* cdimgChd     = NULL;
static INT32     cdimgChdTrackLBA[MAXIMUM_NUMBER_TRACKS];	// logical LBA of each track start
static INT32     cdimgTrack   = 0;
static INT32     cdimgLBA     = 0;

static INT32     cdimgSamples = 0;

static INT32     re_sync      = 0;

// identical to the format used in clonecd .sub files, can use memcpy
struct QData {
	UINT8  Control;
	char   track;
	char   index;
	MSF    MSFrel;
	char   unused;
	MSF    MSFabs;
	UINT16 CRC;
};

static QData* QChannel      = NULL;
static INT32  QChannelCount = 0;

// -----------------------------------------------------------------------------

const  INT32 cdimgOUT_SIZE         = 2352;
static INT32 cdimgOutputbufferSize = 0;

static INT16* cdimgOutputbuffer    = NULL;

static INT32 cdimgOutputPosition;
static INT32 cdimgAudioFilePos;		// Track audio read position within file (file sector index)

// -----------------------------------------------------------------------------

// neocdlist.h declares GetIsoPath() extern only under BUILD_NEOGEO and provides
// a static-inline stub otherwise. Guard the definition to match, so a build
// without the Neo Geo driver (e.g. a single non-neogeo driver) does not hit a
// redefinition. All callers live in BUILD_NEOGEO-guarded code.
#ifdef BUILD_NEOGEO
TCHAR* GetIsoPath()
{
	if (cdimgImage) {
		return (TCHAR*)CDImageGetPath(cdimgImage);
	}
	if (cdimgTOC) {
		return cdimgTOC->Image;
	}

	// CHD files don't need CUE parsing - the .chd file IS the complete image
	if (_tcslen(CDEmuImage) > 4 && IsFileExt(CDEmuImage, _T(".chd"))) {
		return CDEmuImage;
	}

	return NULL;
}
#endif

static inline UINT8 bcd(const UINT8 v)
{
	return ((v >> 4) * 10) + (v & 0x0F);
}

static inline UINT8 tobcd(const UINT8 v)
{
	return ((v / 10) << 4) | (v % 10);
}

static const UINT8* cdimgLBAToMSF(INT32 LBA)
{
	static UINT8 address[4];

	address[0] = 0;
	address[1] = tobcd(LBA                    / CD_FRAMES_MINUTE);
	address[2] = tobcd(LBA % CD_FRAMES_MINUTE / CD_FRAMES_SECOND);
	address[3] = tobcd(LBA % CD_FRAMES_SECOND);

	return address;
}

static INT32 cdimgMSFToLBA(const UINT8* address)
{
	INT32 LBA;

	LBA  = bcd(address[3]);
	LBA += bcd(address[2]) * CD_FRAMES_SECOND;
	LBA += bcd(address[1]) * CD_FRAMES_MINUTE;

	return LBA;
}

static const UINT8* dinkLBAToMSF(const INT32 LBA) // not BCD version
{
	static UINT8 address[4];

	address[0] = 0;
	address[1] = LBA                    / CD_FRAMES_MINUTE;
	address[2] = LBA % CD_FRAMES_MINUTE / CD_FRAMES_SECOND;
	address[3] = LBA % CD_FRAMES_SECOND;

	return address;
}

static INT32 dinkMSFToLBA(const UINT8* address)
{
	INT32 LBA;

	LBA  = address[3];
	LBA += address[2] * CD_FRAMES_SECOND;
	LBA += address[1] * CD_FRAMES_MINUTE;

	return LBA;
}

// -----------------------------------------------------------------------------

static void cdimgExitStream()
{
	free_s((void**)&cdimgOutputbuffer);
}

static INT32 cdimgInitStream()
{
	cdimgExitStream();

	cdimgOutputbuffer = (INT16*)malloc(cdimgOUT_SIZE * 2 * sizeof(INT16));
	if (cdimgOutputbuffer == NULL)
		return 1;

	return 0;
}

static INT32 cdimgCopyPath(TCHAR* dst, size_t count, const TCHAR* src)
{
	if (!dst || !src || count == 0 || _tcslen(src) >= count)
		return 1;

	_tcscpy(dst, src);
	return 0;
}

static INT32 cdimgReplaceExtension(TCHAR* path, size_t count, const TCHAR* extension)
{
	size_t length;

	if (!path || !extension || count == 0)
		return 1;

	length = _tcslen(path);
	if (length < 4 || _tcslen(extension) != 4)
		return 1;

	_tcscpy(path + length - 4, extension);
	return 0;
}

static INT32 cdimgSkip(FILE* h, INT64 samples)
{
	return cdimgFseek(h, samples * 4, SEEK_CUR) == 0 ? 0 : 1;
}

// -----------------------------------------------------------------------------

static void cdimgPrintImageInfo()
{
	bprintf(0, _T("Image file: %s\n"), cdimgTOC->Image);

	bprintf(0, _T("   CD image TOC - "));
	if (cdimgTOC->ImageType == CD_TYPE_CCD)
		bprintf(0, _T("TruRip (.CCD/.SUB/.IMG) format\n"));
	if (cdimgTOC->ImageType == CD_TYPE_BINCUE)
		bprintf(0, _T("Disk At Once (.BIN/.CUE) format\n"));

	for (INT32 trk = cdimgTOC->FirstTrack - 1; trk <= cdimgTOC->LastTrack; trk++) {
		const UINT8* addressUNBCD = dinkLBAToMSF(cdimgMSFToLBA(cdimgTOC->TrackData[trk].Address));

		if (trk != cdimgTOC->LastTrack) {
			bprintf(0, _T("Track %02d: %02d:%02d:%02d\n"), trk + 1,   addressUNBCD[1], addressUNBCD[2], addressUNBCD[3]);
		} else {
			bprintf(0, _T("    total running time %02i:%02i:%02i\n"), addressUNBCD[1], addressUNBCD[2], addressUNBCD[3]);
		}
	}
}

static INT32 cdimgAddLastTrack()
{
	FILE* h = _tfopen(cdimgTOC->Image, _T("rb"));
	if (!h) {
		return 1;
	}

	if (cdimgFseek(h, 0, SEEK_END) != 0) {
		fclose(h);
		return 1;
	}

	INT64 size = cdimgFtell(h);
	fclose(h);
	if (size < 0) {
		return 1;
	}

	const UINT8* address = cdimgLBAToMSF((INT32)((size + 2351) / 2352) + cd_pregap);
	cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[1] = address[1];
	cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[2] = address[2];
	cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[3] = address[3];

	return 0;
}

// parse .sub file and build a TOC based in Q sub channel data
static INT32 cdimgParseSubFile()
{
	TCHAR  filenameSub[MAX_PATH];
	FILE*  h       = NULL;
	UINT8* subData = NULL;
	INT32  track   = 0;

	if (cdimgCopyPath(filenameSub, MAX_PATH, CDEmuImage) ||
		(!IsFileExt(filenameSub, _T(".ccd")) && !IsFileExt(filenameSub, _T(".img")) && !IsFileExt(filenameSub, _T(".sub"))) ||
		cdimgCopyPath(cdimgTOC->Image, MAX_PATH, CDEmuImage) ||
		cdimgReplaceExtension(cdimgTOC->Image, MAX_PATH, _T(".img")) ||
		cdimgReplaceExtension(filenameSub, MAX_PATH, _T(".sub"))) {
		return 1;
	}

	FILE* image = _tfopen(cdimgTOC->Image, _T("rb"));
	if (!image) {
		return 1;
	}
	fclose(image);

	h = _tfopen(filenameSub, _T("rb"));
	if (!h || cdimgFseek(h, 0, SEEK_END) != 0) {
		if (h)
			fclose(h);

		return 1;
	}

	INT64 size64 = cdimgFtell(h);
	if (size64 < 96 || size64 > 0x7fffffff || (size64 % 96) != 0 || cdimgFseek(h, 0, SEEK_SET) != 0) {
		fclose(h);
		return 1;
	}

	QChannelCount = (INT32)(size64 / 96);
	subData  = (UINT8*)malloc((size_t)size64);
	QChannel = (QData*)calloc(QChannelCount, sizeof(QData));
	if (!subData || !QChannel || fread(subData, 1, (size_t)size64, h) != (size_t)size64) {
		fclose(h);
		free_s((void**)&subData);
		free(QChannel);
		QChannel = NULL;
		QChannelCount = 0;
		return 1;
	}
	fclose(h);

	cdimgTOC->ImageType  = CD_TYPE_CCD;
	cdimgTOC->FirstTrack = 1;
	for (INT32 i = 0; i < QChannelCount; i++) {
		memcpy(&QChannel[i], subData + i * 96 + 12, 12);
		INT32 current = bcd((UINT8)QChannel[i].track);
		if (QChannel[i].index && (QChannel[i].Control & 1) && current >= 1 && current <= MAXIMUM_NUMBER_TRACKS &&
			current == track + 1 && cdimgTOC->TrackData[current - 1].TrackNumber == 0) {
			track = current;
			cdimgTOC->TrackData[track - 1].Control     = QChannel[i].Control;
			cdimgTOC->TrackData[track - 1].TrackNumber = (UINT8)QChannel[i].track;
			cdimgTOC->TrackData[track - 1].Address[1]  = QChannel[i].MSFabs.M;
			cdimgTOC->TrackData[track - 1].Address[2]  = QChannel[i].MSFabs.S;
			cdimgTOC->TrackData[track - 1].Address[3]  = QChannel[i].MSFabs.F;
		}
	}
	free_s((void**)&subData);

	if (track == 0) {
		free(QChannel);
		QChannel = NULL;
		QChannelCount = 0;
		return 1;
	}

	cdimgTOC->LastTrack = track;
	cd_pregap = bcd(QChannel[0].MSFabs.F) + bcd(QChannel[0].MSFabs.S) * CD_FRAMES_SECOND + bcd(QChannel[0].MSFabs.M) * CD_FRAMES_MINUTE;
	return cdimgAddLastTrack();
}

#if 0
// dead code ?
static INT32 cdimgParseCueFile()
{
	TCHAR  szLine[1024];
	TCHAR  szFile[MAX_PATH];
	TCHAR* s;
	TCHAR* t;
	FILE*  h;
	INT32  track = 0;
	INT32  length;
	INT32  fileCount = 0;
	UINT8  indexSeen[MAXIMUM_NUMBER_TRACKS] = { 0 };

	cdimgTOC->ImageType  = CD_TYPE_BINCUE;
	cdimgTOC->FirstTrack = 1;
	cdimgTOC->LastTrack  = 0;

	cdimgTOC->TrackData[0].Address[1] = 0;
	cdimgTOC->TrackData[0].Address[2] = 2;
	cdimgTOC->TrackData[0].Address[3] = 0;

	cd_pregap = 150;	// default for bin/cue?

	if (cdimgCopyPath(cdimgTOC->Image, MAX_PATH, CDEmuImage) || cdimgReplaceExtension(cdimgTOC->Image, MAX_PATH, _T(".bin"))) {
		return 1;
	}

	h = _tfopen(CDEmuImage, _T("rt"));
	if (!h) {
		return 1;
	}

	while (1) {
		if (!_fgetts(szLine, sizeof(szLine) / sizeof(szLine[0]), h)) {
			break;
		}

		length = _tcslen(szLine);
		// get rid of the linefeed at the end
		while (length && (szLine[length - 1] == _T('\r') || szLine[length - 1] == _T('\n'))) {
			szLine[length - 1] = 0;
			length--;
		}

		s = szLine;

		// file info
		if ((t = LabelCheck(s, _T("FILE"))) != 0) {
			s = t;

			TCHAR* szQuote = NULL;
			TCHAR* szEnd   = NULL;
			const TCHAR* separator = _tcsrchr(CDEmuImage, _T('/'));
			const TCHAR* backslash = _tcsrchr(CDEmuImage, _T('\\'));
			if (!separator || (backslash && backslash > separator)) separator = backslash;
			size_t directoryLength = separator ? (size_t)(separator - CDEmuImage + 1) : 0;

			if (QuoteRead(&szQuote, &szEnd, s) || !szQuote || !szEnd || ++fileCount > 1) {
				fclose(h);
				return 1;
			}
			while (*szEnd == _T(' ') || *szEnd == _T('\t')) szEnd++;
			if (_tcsncmp(szEnd, _T("BINARY"), 6) != 0) {
				fclose(h);
				return 1;
			}
			szEnd += 6;
			while (*szEnd == _T(' ') || *szEnd == _T('\t')) szEnd++;
			if (*szEnd != 0) {
				fclose(h);
				return 1;
			}

			bool absolute = szQuote[0] == _T('/') || szQuote[0] == _T('\\') || (_tcslen(szQuote) > 1 && szQuote[1] == _T(':'));
			if (absolute)
				directoryLength = 0;
			if (directoryLength + _tcslen(szQuote) >= MAX_PATH) {
				fclose(h);
				return 1;
			}
			if (directoryLength)
				memcpy(szFile, CDEmuImage, directoryLength * sizeof(TCHAR));

			_tcscpy(szFile + directoryLength, szQuote);
			if (cdimgCopyPath(cdimgTOC->Image, MAX_PATH, szFile)) {
				fclose(h);
				return 1;
			}
			continue;
		}

		// track info
		if ((t = LabelCheck(s, _T("TRACK"))) != 0) {
			s = t;

			// track number
			track = _tcstol(s, &t, 10);

			if (track < 1 || track > MAXIMUM_NUMBER_TRACKS || track != cdimgTOC->LastTrack + 1) {
				fclose(h);
				return 1;
			}
			if (track < cdimgTOC->FirstTrack) {
				cdimgTOC->FirstTrack = track;
			}
			if (track > cdimgTOC->LastTrack) {
				cdimgTOC->LastTrack  = track;
			}
			cdimgTOC->TrackData[track - 1].TrackNumber = tobcd(track);

			s = t;

			// type of track

			if ((t = LabelCheck(s, _T("MODE1/2352"))) != 0) {
				cdimgTOC->TrackData[track - 1].Control = 0x41;
				//bprintf(0, _T(".cue: Track #%d, data.\n"), track);
				continue;
			}
			if ((t = LabelCheck(s, _T("AUDIO"))) != 0) {
				cdimgTOC->TrackData[track - 1].Control = 0x01;
				//bprintf(0, _T(".cue: Track #%d, AUDIO.\n"), track);

				continue;
			}

			fclose(h);
			return 1;
		}

		if (LabelCheck(s, _T("PREGAP")) != 0) {
			fclose(h);
			return 1;
		}

		// TRACK Index
		if ((t = LabelCheck(s, _T("INDEX 01"))) != 0) {
			if (track == 0 || indexSeen[track - 1]) {
				fclose(h);
				return 1;
			}
			s = t;

			INT32 M, S, F;

			// index M
			M = _tcstol(s, &t, 10);
			s = t + 1;
			// index S
			S = _tcstol(s, &t, 10);
			s = t + 1;
			// index F
			F = _tcstol(s, &t, 10);

			if (M < 0 || M > 100 || S < 0 || S > 59 || F < 0 || F > 74) {
				bprintf(0, _T("Bad M:S:F!\n"));
				fclose(h);
				return 1;
			}

			const UINT8  address[]  = { 0, (UINT8)M, (UINT8)S, (UINT8)F };
			const UINT8* newaddress = cdimgLBAToMSF(dinkMSFToLBA(address) + cd_pregap);
			//const UINT8* newaddressUNBCD = dinkLBAToMSF(dinkMSFToLBA(address) + cd_pregap);
			//bprintf(0, _T("Track MSF: %02d:%02d:%02d "), newaddressUNBCD[1], newaddressUNBCD[2], newaddressUNBCD[3]);

			cdimgTOC->TrackData[track - 1].Address[1] = newaddress[1];
			cdimgTOC->TrackData[track - 1].Address[2] = newaddress[2];
			cdimgTOC->TrackData[track - 1].Address[3] = newaddress[3];
			indexSeen[track - 1] = 1;

			continue;
		}
	}

	fclose(h);

	if (fileCount != 1 || track == 0) {
		return 1;
	}
	for (INT32 i = 0; i < track; i++) {
		if (!indexSeen[i]) return 1;
	}

	return cdimgAddLastTrack();
}
#endif

// -----------------------------------------------------------------------------

static INT32 cdimgExit()
{
	cdimgExitStream();

	if (cdimgFile)
		fclose(cdimgFile);

	cdimgFile  = NULL;
	if (cdimgChd)
		ChdClose(cdimgChd);

	cdimgChd   = NULL;
	CDImageClose(cdimgImage);
	cdimgImage = NULL;

	cdimgTrack = 0;
	cdimgLBA   = 0;
	cd_volume  = 100.0;

	free(cdimgTOC);
	cdimgTOC   = NULL;
	free(QChannel);
	QChannel   = NULL;
	QChannelCount = 0;

	return 0;
}

// ---------------------------------------------------------------------------
// Unified sector reader for the cd_img backend.  Regardless of whether the
// underlying image is a plain .bin/.cue or a compressed .chd, callers invoke
// this function with a data-track-relative LBA (LBA 0 == first user sector,
// same coordinate system as cdimgFseek(offset*2352) in a .bin file).  The returned
// buffer is always a 2352-byte raw mode-1 sector.
//
//   lba    : logical sector index (matches the TOC addresses built at parse)
//   dest   : caller-provided 2352-byte buffer
//   bAudio : true to request the track's native form (audio), false for
//            a mode-1 raw sector (cooked 2048 tracks are promoted to raw)
//   return : 0 on success, non-zero on error
static INT32 cdimgReadRawSector(INT32 lba, UINT8* dest, bool bAudio = false)
{
	if (!cdimgTOC)
		return 1;
	if (cdimgImage)
		return CDImageReadRawSector(cdimgImage, lba, dest);
	if (!cdimgFile)
		return 1;
	if (cdimgFseek(cdimgFile, (INT64)lba * 2352, SEEK_SET) != 0)
		return 1;

	size_t n = fread(dest, 1, 2352, cdimgFile);
	return (n == 2352) ? 0 : 1;
}

// Count the number of audio tracks in a CHD file.
// pszFile — TCHAR path to the .chd file (pass NULL to use CDEmuImage)
INT32 cdimgCountChdAudioTracks(TCHAR* pszFile)
{
	const TCHAR* pszPath = pszFile ? pszFile : CDEmuImage;
	if (!pszPath || _tcslen(pszPath) < 5) {
		bprintf(PRINT_ERROR, _T("cdimgCountChdAudioTracks: invalid path\n"));
		return 0;
	}
	return ChdCountAudioTracks(pszPath);
}

static INT32 cdimgParseImage()
{
	INT32 nType = IsFileExt(CDEmuImage, _T(".chd")) ? CD_TYPE_CHD : IsFileExt(CDEmuImage, _T(".ccd")) ? CD_TYPE_CCD : CD_TYPE_BINCUE;

	cdimgImage = CDImageOpen(CDEmuImage);
	if (!cdimgImage)
		return 1;

	INT32 nTracks = CDImageGetTrackCount(cdimgImage);
	if (nTracks < 1 || nTracks > MAXIMUM_NUMBER_TRACKS)
		return 1;

	if (nType == CD_TYPE_CCD) {
		INT32 nError = cdimgParseSubFile();
		if (nError)
			return nError;

		cdimgTOC->TrackData[nTracks].Control = cdimgTOC->TrackData[nTracks - 1].Control;
		for (INT32 i = 0; i < nTracks; i++) {
			cdimgTOC->TrackData[i].EndAddress[0] = 0;
			const UINT8* pAddress = cdimgLBAToMSF((i + 1 < nTracks ? CDImageGetTrack(cdimgImage, i + 1)->nIndex0LBA : CDImageGetSectorCount(cdimgImage)) + cd_pregap);
			memcpy(cdimgTOC->TrackData[i].EndAddress, pAddress, 4);
		}
		return 0;
	}

	cdimgTOC->ImageType  = nType;
	cdimgTOC->FirstTrack = 1;
	cdimgTOC->LastTrack  = nTracks;

	cd_pregap = 150;
	for (INT32 i = 0; i < nTracks; i++) {
		const CDImageTrack* pTrack = CDImageGetTrack(cdimgImage, i);
		cdimgTOC->TrackData[i].Control     = (UINT8)pTrack->nControl;
		cdimgTOC->TrackData[i].TrackNumber = tobcd(i + 1);
		const UINT8* pAddress = cdimgLBAToMSF(pTrack->nStartLBA + cd_pregap);
		memcpy(cdimgTOC->TrackData[i].Address, pAddress, 4);
		INT32 nEnd = i + 1 < nTracks ? CDImageGetTrack(cdimgImage, i + 1)->nIndex0LBA : CDImageGetSectorCount(cdimgImage);
		pAddress = cdimgLBAToMSF(nEnd + cd_pregap);
		memcpy(cdimgTOC->TrackData[i].EndAddress, pAddress, 4);
	}

	const UINT8* pLeadout = cdimgLBAToMSF(CDImageGetSectorCount(cdimgImage) + cd_pregap);
	cdimgTOC->TrackData[nTracks].Control     = cdimgTOC->TrackData[nTracks - 1].Control;
	cdimgTOC->TrackData[nTracks].TrackNumber = 0xaa;
	memcpy(cdimgTOC->TrackData[nTracks].Address, pLeadout, 4);
	const CDImageTrack* pFirst = CDImageGetTrack(cdimgImage, 0);
	return cdimgCopyPath(cdimgTOC->Image, MAX_PATH, pFirst->szPath);
}

#if 0
// dead code ?
static INT32 cdimgParseChdFile()
{
	cdimgChd = ChdOpenFile(CDEmuImage);
	if (!cdimgChd) {
		dprintf(_T("*** Couldn't open .chd file\n"));
		return 1;
	}

	INT32 nContainer = ChdGetContainerType(cdimgChd);
	if (nContainer != CHD_CONTAINER_CD && nContainer != CHD_CONTAINER_GDROM) {
		dprintf(_T("*** CHD is not a CD/GD-ROM image\n"));
		ChdClose(cdimgChd);
		cdimgChd = NULL;
		return 1;
	}

	cdimgTOC->ImageType = CD_TYPE_CHD;
	if (cdimgCopyPath(cdimgTOC->Image, MAX_PATH, CDEmuImage)) {
		ChdClose(cdimgChd);
		cdimgChd = NULL;
		return 1;
	}

	// Standard CD pregap is 150 sectors; TOC addresses carry it so the
	// coordinate system matches .bin/.cue.
	cd_pregap = 150;

	INT32 nTracks = ChdGetNumTracks(cdimgChd);
	if (nTracks <= 0 || nTracks > MAXIMUM_NUMBER_TRACKS) {
		ChdClose(cdimgChd);
		cdimgChd = NULL;
		return 1;
	}

	for (INT32 trk = 0; trk < nTracks; trk++) {
		const ChdTrack* pT = ChdGetTrack(cdimgChd, trk);
		cdimgChdTrackLBA[trk] = pT->nLogFrameOfs;

		cdimgTOC->TrackData[trk].Control     = pT->nControl;
		cdimgTOC->TrackData[trk].TrackNumber = tobcd(trk + 1);

		const UINT8* msf = cdimgLBAToMSF(pT->nLogFrameOfs + cd_pregap);
		cdimgTOC->TrackData[trk].Address[0] = 0;
		cdimgTOC->TrackData[trk].Address[1] = msf[1];
		cdimgTOC->TrackData[trk].Address[2] = msf[2];
		cdimgTOC->TrackData[trk].Address[3] = msf[3];
	}

	// EndAddress per track (next track start, or lead-out for the last).
	INT32 nTotal = ChdGetTotalFrames(cdimgChd);
	for (INT32 trk = 0; trk < nTracks; trk++) {
		INT32 end = (trk + 1 < nTracks) ? cdimgChdTrackLBA[trk + 1] : nTotal;
		const UINT8* msf = cdimgLBAToMSF(end + cd_pregap);
		cdimgTOC->TrackData[trk].EndAddress[0] = 0;
		cdimgTOC->TrackData[trk].EndAddress[1] = msf[1];
		cdimgTOC->TrackData[trk].EndAddress[2] = msf[2];
		cdimgTOC->TrackData[trk].EndAddress[3] = msf[3];
	}

	cdimgTOC->FirstTrack = 1;
	cdimgTOC->LastTrack  = nTracks;

	// Lead-out entry, just past the last valid sector.
	const UINT8* leadout = cdimgLBAToMSF(nTotal + cd_pregap);
	cdimgTOC->TrackData[nTracks].Control     = cdimgTOC->TrackData[nTracks - 1].Control;
	cdimgTOC->TrackData[nTracks].TrackNumber = 0xaa;
	cdimgTOC->TrackData[nTracks].Address[0]  = 0;
	cdimgTOC->TrackData[nTracks].Address[1]  = leadout[1];
	cdimgTOC->TrackData[nTracks].Address[2]  = leadout[2];
	cdimgTOC->TrackData[nTracks].Address[3]  = leadout[3];

	// Debug: report container class, CHD geometry and per-track MODE / bytes-per-sector.
	bprintf(PRINT_IMPORTANT, _T("CHD: container=%s  v%d  hunkbytes=%d  frames/hunk=%d  tracks=%d  total frames=%d\n"),
		ChdContainerName(nContainer), ChdGetVersion(cdimgChd),
		ChdGetHunkBytes(cdimgChd), ChdGetFramesPerHunk(cdimgChd), nTracks, nTotal);
	for (INT32 trk = 0; trk < nTracks; trk++) {
		const ChdTrack* pT = ChdGetTrack(cdimgChd, trk);
		bprintf(PRINT_IMPORTANT, _T("CHD:   track %02d  type=%s  %d bytes/sector  sub=%d  frames=%d\n"),
			trk + 1, ChdTrackTypeName(pT->nType), pT->nDataSize, pT->nSubSize, pT->nFrames);
	}

	return 0;
}
#endif

static INT32 cdimgInit()
{
	re_sync = 0;

	cdimgTOC = (cdimgCDROM_TOC*)calloc(1, sizeof(cdimgCDROM_TOC));
	if (!cdimgTOC)
		return 1;

	cdimgTOC->ImageType = CD_TYPE_NONE;

	TCHAR* filename = ExtractFilename(CDEmuImage);

	if (_tcslen(filename) < 4) {
		cdimgExit();
		return 1;
	}

	if (IsFileExt(filename, _T(".cue")) || IsFileExt(filename, _T(".ccd")) || IsFileExt(filename, _T(".chd"))) {
		if (cdimgParseImage()) {
			dprintf(_T("*** Couldn't parse CD image\n"));
			cdimgExit();
			return 1;
		}
	} else {
		dprintf(_T("*** Couldn't find .img / .bin file\n"));
		cdimgExit();

		return 1;
	}

	cdimgPrintImageInfo();

	CDEmuStatus = idle;

	if (cdimgInitStream()) {
		cdimgExit();
		return 1;
	}

	UINT8 buf[2048];
	INT32 nDataTrack = -1;
	for (INT32 i = 0; i < CDImageGetTrackCount(cdimgImage); i++) {
		const CDImageTrack* pTrack = CDImageGetTrack(cdimgImage, i);
		if (pTrack->nType != CDIMAGE_TRACK_AUDIO) {
			nDataTrack = i;
			break;
		}
	}
	if (nDataTrack >= 0) {
		const CDImageTrack* pTrack = CDImageGetTrack(cdimgImage, nDataTrack);
		if (CDImageReadDataSector(cdimgImage, pTrack->nIndex1LBA + 16, buf) || memcmp(buf + 1, "CD001", 5))
			dprintf(_T("*** Bad CD!\n"));
	} else {
		dprintf(_T("*** CD image has no data track; skipping PVD check\n"));
	}
	//CDEmuPrintCDName();

	return 0;
}

static void cdimgCloseFile()
{
	if (cdimgFile) {
		fclose(cdimgFile);
		cdimgFile = NULL;
	}
	// cdimgChd stays open for the lifetime of the CD session, until cdimgExit()
}

static INT32 cdimgStop()
{
	cdimgCloseFile();
	CDEmuStatus = idle;
	return 0;
}

static INT32 cdimgFindTrack(INT32 LBA)
{
	INT32 trk = 0;
	for (trk = cdimgTOC->FirstTrack - 1; trk < cdimgTOC->LastTrack; trk++)
		if (LBA < cdimgMSFToLBA(cdimgTOC->TrackData[trk + 1].Address))
			break;

	return trk;
}

static INT32 cdimgPlayLBA(INT32 LBA)	// audio play start
{
	cdimgStop();

	if (QChannel != NULL) {		// .CCD dump w/.SUB
		if (LBA < 0 || LBA >= QChannelCount || (QChannel[LBA].Control & 0x40))
			return 1;
	} else {					// .BIN/.CUE dump or .CHD
		if (cdimgTOC->TrackData[cdimgFindTrack(LBA)].Control & 0x40)
			return 1;
	}

	cdimgLBA   = LBA;

	cdimgTrack = cdimgFindTrack(cdimgLBA);

	if (cdimgTrack >= cdimgTOC->LastTrack)
		return 1;

	bprintf(PRINT_IMPORTANT, _T("    playing track %2i\n"), cdimgTrack + 1);

	// ------------------------------------------------------------------
	// Prepare the audio output buffer.  Size and byte layout are always
	// the same regardless of image container (stereo 16-bit PCM).  The
	// only difference is how we source the raw bytes.
	// ------------------------------------------------------------------
	INT32 sectors_to_read = (cdimgOUT_SIZE * 4) / 2352;
	INT32 base;
	INT32 trackEnd = cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1].Address);
	if (sectors_to_read > trackEnd - cdimgLBA)
		sectors_to_read = trackEnd - cdimgLBA;
	if (sectors_to_read <= 0)
		return 1;

	// Logical LBA space is shared by CHD and .bin/.cue: subtract the disc
	// pregap.  For CHD, cd_chd maps the logical LBA to its CHD frame per track.
	base = cdimgLBA - cd_pregap;

	if (base < 0)
		base = 0;
	cdimgAudioFilePos = base;

	if (cdimgImage != NULL) {
		UINT8 sector_buf[2352];
		INT32 read_count = 0;
		for (INT32 i = 0; i < sectors_to_read; i++) {
			if (cdimgReadRawSector(base + i, sector_buf, true) != 0)
				break;

			if (cdimgTOC->ImageType == CD_TYPE_CHD) {
				for (INT32 j = 0; j < 2352; j += 4) {
					INT32 dst_idx = (i * 2352 + j) / 2;
					cdimgOutputbuffer[dst_idx + 0] = (sector_buf[j + 0] << 8) | sector_buf[j + 1];
					cdimgOutputbuffer[dst_idx + 1] = (sector_buf[j + 2] << 8) | sector_buf[j + 3];
				}
			} else {
				memcpy((UINT8*)cdimgOutputbuffer + i * 2352, sector_buf, 2352);
			}
			read_count++;
		}
		cdimgAudioFilePos     = base + read_count;
		cdimgOutputbufferSize = read_count * (2352 / 4);
		if (read_count == 0) {
			cdimgStop();
			return 1;
		}

	} else {
		cdimgFile = _tfopen(cdimgTOC->Image, _T("rb"));
		if (cdimgFile == NULL)
			return 1;

		// advance if we're not starting at the beginning of a CD
		if (cdimgLBA > cd_pregap && cdimgSkip(cdimgFile, (INT64)(cdimgLBA - cd_pregap) * (44100 / CD_FRAMES_SECOND))) {
			cdimgStop();
			return 1;
		}

		// fill the input buffer (size returned as count of 4-byte sample units)
		cdimgOutputbufferSize = (INT32)fread(cdimgOutputbuffer, 4, sectors_to_read * (2352 / 4), cdimgFile);
		cdimgAudioFilePos     = (INT32)(cdimgFtell(cdimgFile) / 2352);
		if (cdimgOutputbufferSize <= 0) {
			cdimgStop();
			return 1;
		}
	}
	cdimgOutputPosition = 0;
	cdimgSamples        = 0;
	// this breaks states, commenting for now just in-case. -dink
	//cdimgLBA = cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address); // start at the beginning of track
	CDEmuStatus         = playing;

	return 0;
}

static INT32 cdimgPlay(UINT8 M, UINT8 S, UINT8 F)
{
	const UINT8 address[] = { 0, M, S, F };

	const UINT8* displayaddress = dinkLBAToMSF(cdimgMSFToLBA(address));
	dprintf(_T("    play %02i:%02i:%02i\n"), displayaddress[1], displayaddress[2], displayaddress[3]);

	return cdimgPlayLBA(cdimgMSFToLBA(address));
}

static INT32 cdimgReadDataSector(INT32 nLba, UINT8* pBuffer)
{
	return cdimgImage ? CDImageReadDataSector(cdimgImage, nLba, pBuffer) : 1;
}

static INT32 cdimgLoadSector(INT32 LBA, char* pBuffer)
{
	if (CDEmuStatus == playing)
		return 0; // data loading

	INT32 originalLBA = LBA;
	if (CDEmuStatus == seeking) {
		LBA -= cd_pregap; // when seeking, we must account for pregap
		re_sync = 1;
	}

	// Unified sector reader: cdimgReadRawSector handles both .bin and .chd
	// transparently. For .bin files we still prefer the sequential-access
	// optimization below (avoids redundant fseek calls).
	if (cdimgTOC && cdimgImage != NULL) {
		if (cdimgReadRawSector(LBA, (UINT8*)pBuffer) != 0) {
			dprintf(_T("*** couldn't read sector (LBA %08u)\n"), originalLBA);
			return 0;
		}
		CDEmuStatus = reading;
		// cdimgLBA mirrors the .cue/.bin convention: file-relative sector
		// index of the next sector.  cdimgTrack is kept in sync using a
		// full-disc LBA (with pregap) so cdimgFindTrack returns the correct
		// track index for the BIOS standby-screen Q-channel query.
		cdimgLBA   = LBA + 1;
		cdimgTrack = cdimgFindTrack(LBA + cd_pregap);
		return cdimgLBA;
	}

	if (LBA != cdimgLBA || !cdimgFile || re_sync) {
		re_sync = 0;

		if (!cdimgFile) {
			cdimgStop();

			cdimgFile = _tfopen(cdimgTOC->Image, _T("rb"));
			if (!cdimgFile)
				return 0;
		}

		//bprintf(PRINT_IMPORTANT, _T("    loading data at LBA %08u 0x%08X\n"), (LBA - cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address)) * 2352, LBA * 2352);

		if (cdimgFseek(cdimgFile, (INT64)LBA * 2352, SEEK_SET)) {
			dprintf(_T("*** couldn't seek (LBA %08u)\n"), LBA);

			//cdimgStop(); // stopping here will break ssrpg,
			// game will seek away & recover from this.

			return 0;
		}

		CDEmuStatus = reading;
	}

	//dprintf(_T("    reading LBA %08i 0x%08X"), LBA, cdimgFtell(cdimgFile));

	// Raw-image path (.cue / .ccd / .bin).  Keep cdimgLBA semantics
	// exactly as before (file sector index) — this historically produces
	// the correct standby-screen output.
	cdimgLBA = cdimgMSFToLBA(cdimgTOC->TrackData[0].Address) + (cdimgFtell(cdimgFile) + 2351) / 2352 - cd_pregap;

	bool status = (fread(pBuffer, 1, 2352, cdimgFile) != 2352);

	if (status) {
		dprintf(_T("*** couldn't read from file - iso corrupt or truncated?\n"));

		//cdimgStop(); - stopping here will break puzzle bobble!  game needs fail @ end of image w/o stopping :)

		return 0;
	}
	// dprintf(_T("    [ %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X ]\n"), pBuffer[0], pBuffer[1], pBuffer[2], pBuffer[3], pBuffer[4], pBuffer[5], pBuffer[6], pBuffer[7], pBuffer[8], pBuffer[9], pBuffer[10], pBuffer[11], pBuffer[12], pBuffer[13], pBuffer[14], pBuffer[15]);

	cdimgLBA++;

	return cdimgLBA;
}

static UINT8* cdimgReadTOC(INT32 track)
{
	static UINT8 TOCEntry[4];
	memset(&TOCEntry, 0, sizeof(TOCEntry));

	if (track == CDEmuTOC_FIRSTLAST) {
		TOCEntry[0] = tobcd(cdimgTOC->FirstTrack);
		TOCEntry[1] = tobcd(cdimgTOC->LastTrack);
		TOCEntry[2] = 0;
		TOCEntry[3] = 0;

		return TOCEntry;
	}
	if (track == CDEmuTOC_LASTMSF) {
		TOCEntry[0] = cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[1];
		TOCEntry[1] = cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[2];
		TOCEntry[2] = cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[3];
		TOCEntry[3] = cdimgTOC->TrackData[cdimgTOC->LastTrack].Control >> 4;

		return TOCEntry;
	}
	if (track == CDEmuTOC_FIRSTINDEX) {
		if (cdimgLBA < cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTOC->FirstTrack].Address)) {
			const UINT8* addressUNBCD = dinkLBAToMSF(cdimgLBA);
			UINT8 index = ((addressUNBCD[1] * 60) + (addressUNBCD[2] + 4)) / 4;
			TOCEntry[0] = tobcd((index < 100) ? index : 99);
		} else {
			TOCEntry[0] = tobcd(1);
		}

		return TOCEntry;
	}
	if (track == CDEmuTOC_ENDOFDISC) {
		if (cdimgLBA >= cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTOC->LastTrack].Address)) {
			bprintf(0, _T("END OF DISC: curr.lba %06d end lba: %06d\n"), cdimgLBA, cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTOC->LastTrack].Address));
			TOCEntry[0] = 1;
		}

		return TOCEntry;
	}

	track = bcd(track);
	if (track >= cdimgTOC->FirstTrack && track <= cdimgTOC->LastTrack) {
		TOCEntry[0] = cdimgTOC->TrackData[track - 1].Address[1];
		TOCEntry[1] = cdimgTOC->TrackData[track - 1].Address[2];
		TOCEntry[2] = cdimgTOC->TrackData[track - 1].Address[3];
		TOCEntry[3] = cdimgTOC->TrackData[track - 1].Control >> 4;
	}

	// dprintf(_T("    track %02i - %02x:%02x:%02x\n"), track, TOCEntry[0], TOCEntry[1], TOCEntry[2]);

	return TOCEntry;
}

static UINT8* cdimgReadQChannel()
{
	// Q channel format
	// byte 0: 41 = data, 1 = cdda ( flags described at https://en.wikipedia.org/wiki/Compact_Disc_subcode )
	// track, index, M rel, S rel, F rel, M to start, S to start, F to start, 0, CRC, CRC
	// if index is 0, MSF rel counts down to next track

	static UINT8 QChannelData[8];

	switch (CDEmuStatus) {
		case reading:
		case playing: {
			if (QChannel != NULL && cdimgLBA >= 0 && cdimgLBA < QChannelCount) {	// .CCD/.SUB
				QChannelData[0] = QChannel[cdimgLBA].track;

				QChannelData[1] = QChannel[cdimgLBA].MSFabs.M;
				QChannelData[2] = QChannel[cdimgLBA].MSFabs.S;
				QChannelData[3] = QChannel[cdimgLBA].MSFabs.F;

				QChannelData[4] = QChannel[cdimgLBA].MSFrel.M;
				QChannelData[5] = QChannel[cdimgLBA].MSFrel.S;
				QChannelData[6] = QChannel[cdimgLBA].MSFrel.F;

				QChannelData[7] = QChannel[cdimgLBA].Control;
			} else {																// .BIN/.ISO / .CHD
				// cdimgLBA is the file-relative sector index for .cue
				// (data-track origin). For .chd we additionally keep
				// cdimgTrack set by cdimgFindTrack to identify which contains
				// the current track.
				UINT8 AddressAbs[4];
				UINT8 AddressRel[4];
				memcpy(AddressAbs, cdimgLBAToMSF(cdimgLBA), sizeof(AddressAbs));
				memcpy(AddressRel, cdimgLBAToMSF(cdimgLBA - cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address)), sizeof(AddressRel));

				QChannelData[0] = cdimgTOC->TrackData[cdimgTrack].TrackNumber;

				QChannelData[1] = AddressAbs[1];
				QChannelData[2] = AddressAbs[2];
				QChannelData[3] = AddressAbs[3];

				QChannelData[4] = AddressRel[1];
				QChannelData[5] = AddressRel[2];
				QChannelData[6] = AddressRel[3];

				QChannelData[7] = cdimgTOC->TrackData[cdimgTrack].Control;
			}

			// dprintf(_T("    Q %02x %02x %02x:%02x:%02x %02x:%02x:%02x\n"), QChannel[cdimgLBA].track, QChannel[cdimgLBA].index, QChannel[cdimgLBA].MSFrel.M, QChannel[cdimgLBA].MSFrel.S, QChannel[cdimgLBA].MSFrel.F, QChannel[cdimgLBA].MSFabs.M, QChannel[cdimgLBA].MSFabs.S, QChannel[cdimgLBA].MSFabs.F);

			break;
		}
		case paused:
			break;

		default:
			memset(QChannelData, 0, sizeof(QChannelData));
	}

	return QChannelData;
}

static int cdimgSetVolume(double dVolume)
{
	cd_volume = dVolume;
	return 0;
}

INT32 cdimgGetCurrentLBA()
{
	return cdimgLBA;
}

static INT32 cdimgGetSoundBuffer(INT16* buffer, INT32 samples)
{
#define CLIP(A) ((A) < -0x8000 ? -0x8000 : (A) > 0x7fff ? 0x7fff : (A))

	if (CDEmuStatus != playing) {
		return 0;
	}

	INT32 trackEnd = cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1].Address);
	while (samples > 0 && CDEmuStatus == playing) {
		INT32 remainingSamples = (trackEnd - cdimgLBA) * (44100 / CD_FRAMES_SECOND) - cdimgSamples;
		if (remainingSamples <= 0) {
			cdimgStop();
			break;
		}

		if (cdimgOutputPosition >= cdimgOutputbufferSize) {
			cdimgOutputPosition = 0;
			INT32 remainingSectors = trackEnd - (cdimgAudioFilePos + cd_pregap);
			INT32 sectorsToRead    = (cdimgOUT_SIZE * 4) / 2352;
			if (sectorsToRead > remainingSectors) sectorsToRead = remainingSectors;
			if (sectorsToRead <= 0) {
				cdimgStop();
				break;
			}

			if (cdimgImage != NULL) {
				UINT8 sectorBuffer[2352];
				INT32 readCount = 0;
				for (INT32 i = 0; i < sectorsToRead; i++) {
					if (cdimgReadRawSector(cdimgAudioFilePos + i, sectorBuffer, true) != 0)
						break;
					if (cdimgTOC->ImageType == CD_TYPE_CHD) {
						for (INT32 j = 0; j < 2352; j += 4) {
							INT32 offset = (i * 2352 + j) / 2;
							cdimgOutputbuffer[offset + 0] = (sectorBuffer[j + 0] << 8) | sectorBuffer[j + 1];
							cdimgOutputbuffer[offset + 1] = (sectorBuffer[j + 2] << 8) | sectorBuffer[j + 3];
						}
					} else {
						memcpy((UINT8*)cdimgOutputbuffer + i * 2352, sectorBuffer, 2352);
					}
					readCount++;
				}
				cdimgAudioFilePos += readCount;
				cdimgOutputbufferSize = readCount * (2352 / 4);
			} else {
				if (!cdimgFile) {
					cdimgFile = _tfopen(cdimgTOC->Image, _T("rb"));
					if (cdimgFile && cdimgFseek(cdimgFile, (INT64)cdimgAudioFilePos * 2352, SEEK_SET) != 0) {
						cdimgCloseFile();
					}
				}
				cdimgOutputbufferSize = cdimgFile ? (INT32)fread(cdimgOutputbuffer, 4, sectorsToRead * (2352 / 4), cdimgFile) : 0;
				if (cdimgFile) cdimgAudioFilePos = (INT32)(cdimgFtell(cdimgFile) / 2352);
			}

			if (cdimgOutputbufferSize <= 0) {
				cdimgStop();
				break;
			}
		}

		INT32 count = cdimgOutputbufferSize - cdimgOutputPosition;
		if (count > samples)
			count = samples;
		if (count > remainingSamples)
			count = remainingSamples;
		INT16* src = cdimgOutputbuffer + cdimgOutputPosition * 2;
		for (INT32 i = count * 2 - 1; i >= 0; i--) {
			buffer[i] = CLIP((src[i] * (cd_volume / 100.0)) + buffer[i]);
		}

		buffer              += count * 2;
		cdimgOutputPosition += count;
		samples             -= count;
		cdimgSamples        += count;
		while (cdimgSamples >= (44100 / CD_FRAMES_SECOND)) {
			cdimgSamples -= 44100 / CD_FRAMES_SECOND;
			cdimgLBA++;
		}
	}

#undef CLIP
	return 0;
}

static INT32 cdimgScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_VOLATILE) {
		SCAN_VAR(CDEmuStatus);
		SCAN_VAR(cdimgTrack);
		SCAN_VAR(cdimgLBA);
		SCAN_VAR(cdimgAudioFilePos);
		SCAN_VAR(cdimgOutputPosition);
		SCAN_VAR(cdimgSamples);
		SCAN_VAR(cdimgOutputbufferSize);
		ScanVar(cdimgOutputbuffer, cdimgOUT_SIZE * 2 * sizeof(INT16), "CDDA buffer");
	}

	if (nAction & ACB_WRITE) {
		re_sync = 1;
		cdimgCloseFile();
	}

	return 0;
}
