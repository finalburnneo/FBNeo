// FBNeo cd-img, TruRip .ccd/.sub/.img support by Jan Klaassen
// .bin/.cue re-work by dink
// .chd support: compressed CD image backend via libchdr (MAME project, BSD-3-Clause).
// The image IS the .chd file; no CUE parsing required.  Sector layout is
// detected from CHD header hunkbytes; 2048/2352/2448 byte sectors are all
// exposed to upper layers as 2352-byte raw mode-1 sectors.

#include "burner.h"
#include "burnint.h"
#include "cd_chd.h"
#include "cd_img.h"
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <malloc.h>
#define cdimgFseek _fseeki64
#define cdimgFtell _ftelli64
#else
#define cdimgFseek fseeko
#define cdimgFtell ftello
#endif

const INT32 MAXIMUM_NUMBER_TRACKS = 99;

const INT32 CD_FRAMES_MINUTE = 60 * 75;
const INT32 CD_FRAMES_SECOND =      75;

const INT32 CD_TYPE_NONE     = 1 << 0;
const INT32 CD_TYPE_BINCUE   = 1 << 1;
const INT32 CD_TYPE_CCD      = 1 << 2;
const INT32 CD_TYPE_CHD      = 1 << 3;

static INT32 cd_pregap;
static double cd_volume = 100.0;

struct MSF { UINT8 M; UINT8 S; UINT8 F; };

struct cdimgTRACK_DATA { UINT8 Control; UINT8 TrackNumber; UINT8 Address[4]; UINT8 EndAddress[4]; };
struct cdimgCDROM_TOC { UINT8 FirstTrack; UINT8 LastTrack; UINT8 ImageType; TCHAR Image[MAX_PATH]; cdimgTRACK_DATA TrackData[MAXIMUM_NUMBER_TRACKS + 1]; };

static cdimgCDROM_TOC* cdimgTOC;

static FILE*  cdimgFile  = NULL;
static ChdImage* cdimgChd = NULL;
static INT32  cdimgChdTrackLBA[MAXIMUM_NUMBER_TRACKS];  // logical LBA of each track start
static INT32  cdimgTrack = 0;
static INT32  cdimgLBA   = 0;

static INT32  cdimgSamples = 0;

static INT32  re_sync = 0;

// identical to the format used in clonecd .sub files, can use memcpy
struct QData { UINT8 Control; char track; char index; MSF MSFrel; char unused; MSF MSFabs; UINT16 CRC; };

static QData* QChannel = NULL;
static INT32 QChannelCount = 0;

// -----------------------------------------------------------------------------

const  INT32 cdimgOUT_SIZE = 2352;
static INT32 cdimgOutputbufferSize = 0;

static INT16* cdimgOutputbuffer = NULL;

static INT32 cdimgOutputPosition;
static INT32 cdimgAudioFilePos;  // Track audio read position within file (file sector index)

// -----------------------------------------------------------------------------

// neocdlist.h declares GetIsoPath() extern only under BUILD_NEOGEO and provides
// a static-inline stub otherwise. Guard the definition to match, so a build
// without the Neo Geo driver (e.g. a single non-neogeo driver) does not hit a
// redefinition. All callers live in BUILD_NEOGEO-guarded code.
#ifdef BUILD_NEOGEO
TCHAR* GetIsoPath()
{
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
	if (!dst || !src || count == 0 || _tcslen(src) >= count) {
		return 1;
	}

	_tcscpy(dst, src);
	return 0;
}

static INT32 cdimgReplaceExtension(TCHAR* path, size_t count, const TCHAR* extension)
{
	size_t length;

	if (!path || !extension || count == 0) {
		return 1;
	}

	length = _tcslen(path);
	if (length < 4 || _tcslen(extension) != 4) {
		return 1;
	}

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
			bprintf(0, _T("Track %02d: %02d:%02d:%02d\n"), trk + 1, addressUNBCD[1], addressUNBCD[2], addressUNBCD[3]);
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
	TCHAR filenameSub[MAX_PATH];
	FILE* h = NULL;
	UINT8* subData = NULL;
	INT32 track = 0;

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
		if (h) fclose(h);
		return 1;
	}

	INT64 size64 = cdimgFtell(h);
	if (size64 < 96 || size64 > 0x7fffffff || (size64 % 96) != 0 || cdimgFseek(h, 0, SEEK_SET) != 0) {
		fclose(h);
		return 1;
	}

	QChannelCount = (INT32)(size64 / 96);
	subData = (UINT8*)malloc((size_t)size64);
	QChannel = (QData*)calloc(QChannelCount, sizeof(QData));
	if (!subData || !QChannel || fread(subData, 1, (size_t)size64, h) != (size_t)size64) {
		fclose(h);
		free_s((void**)&subData);
		free_s((void**)&QChannel);
		QChannelCount = 0;
		return 1;
	}
	fclose(h);

	cdimgTOC->ImageType = CD_TYPE_CCD;
	cdimgTOC->FirstTrack = 1;
	for (INT32 i = 0; i < QChannelCount; i++) {
		memcpy(&QChannel[i], subData + i * 96 + 12, 12);
		INT32 current = bcd((UINT8)QChannel[i].track);
		if (QChannel[i].index && (QChannel[i].Control & 1) && current >= 1 && current <= MAXIMUM_NUMBER_TRACKS &&
			current == track + 1 && cdimgTOC->TrackData[current - 1].TrackNumber == 0) {
			track = current;
			cdimgTOC->TrackData[track - 1].Control = QChannel[i].Control;
			cdimgTOC->TrackData[track - 1].TrackNumber = (UINT8)QChannel[i].track;
			cdimgTOC->TrackData[track - 1].Address[1] = QChannel[i].MSFabs.M;
			cdimgTOC->TrackData[track - 1].Address[2] = QChannel[i].MSFabs.S;
			cdimgTOC->TrackData[track - 1].Address[3] = QChannel[i].MSFabs.F;
		}
	}
	free_s((void**)&subData);

	if (track == 0) {
		free_s((void**)&QChannel);
		QChannelCount = 0;
		return 1;
	}

	cdimgTOC->LastTrack = track;
	cd_pregap = bcd(QChannel[0].MSFabs.F) + bcd(QChannel[0].MSFabs.S) * CD_FRAMES_SECOND + bcd(QChannel[0].MSFabs.M) * CD_FRAMES_MINUTE;
	return cdimgAddLastTrack();
}

static INT32 cdimgParseCueFile()
{
	TCHAR szLine[1024];
	TCHAR szFile[MAX_PATH];
	TCHAR* s;
	TCHAR* t;
	FILE* h;
	INT32 track = 0;
	INT32 length;
	INT32 fileCount = 0;
	UINT8 indexSeen[MAXIMUM_NUMBER_TRACKS] = { 0 };

	cdimgTOC->ImageType  = CD_TYPE_BINCUE;
	cdimgTOC->FirstTrack = 1;
	cdimgTOC->LastTrack  = 0;

	cdimgTOC->TrackData[0].Address[1] = 0;
	cdimgTOC->TrackData[0].Address[2] = 2;
	cdimgTOC->TrackData[0].Address[3] = 0;

	cd_pregap = 150; // default for bin/cue?

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
			TCHAR* szEnd = NULL;
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

			bool absolute = szQuote[0] == _T('/') || szQuote[0] == _T('\\') ||
				(_tcslen(szQuote) > 1 && szQuote[1] == _T(':'));
			if (absolute) directoryLength = 0;
			if (directoryLength + _tcslen(szQuote) >= MAX_PATH) {
				fclose(h);
				return 1;
			}
			if (directoryLength) memcpy(szFile, CDEmuImage, directoryLength * sizeof(TCHAR));
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
				cdimgTOC->LastTrack = track;
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

			const UINT8  address[] = { 0, (UINT8)M, (UINT8)S, (UINT8)F };
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

// -----------------------------------------------------------------------------

static INT32 cdimgExit()
{
	cdimgExitStream();

	if (cdimgFile)
		fclose(cdimgFile);
	cdimgFile = NULL;

	if (cdimgChd)
		ChdClose(cdimgChd);
	cdimgChd = NULL;

	cdimgTrack = 0;
	cdimgLBA   = 0;
	cd_volume  = 100.0;

	free_s((void**)&cdimgTOC);
	free_s((void**)&QChannel);
	QChannel = NULL;
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

	if (cdimgTOC->ImageType == CD_TYPE_CHD) {
		INT32 nType = bAudio ? CHD_TRACK_RAW_DONTCARE : CHD_TRACK_MODE1_RAW;
		return ChdReadSector(cdimgChd, lba, nType, dest);
	}

	// Fall-through: standard raw image (.bin/.cue, .ccd/.img).
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
	cdimgTOC->TrackData[nTracks].Control     = 0x41;
	cdimgTOC->TrackData[nTracks].TrackNumber = 0xAA;
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

	if (IsFileExt(filename, _T(".cue"))) {
		if (cdimgParseCueFile()) {
			dprintf(_T("*** Couldn't parse .cue file\n"));
			cdimgExit();

			return 1;
		}

	} else
	if (IsFileExt(filename, _T(".ccd"))) {
		if (cdimgParseSubFile()) {
			dprintf(_T("*** Couldn't parse .sub file\n"));
			cdimgExit();

			return 1;
		}

	} else
	if (IsFileExt(filename, _T(".chd"))) {
		if (cdimgParseChdFile()) {
			dprintf(_T("*** Couldn't parse .chd file\n"));
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

	// Buffer for sector data - always 2352 for CD sectors
	char* buf = (char*)malloc(2352);
	if (!buf) {
		dprintf(_T("*** Out of memory for CD buffer\n"));
		cdimgExit();
		return 1;
	}

	cdimgLBA++;

	// Validate the CD by scanning the ISO-9660 volume descriptor at sector 16.
	// Both .bin and .chd images expose sector 16 as the first usable data sector.
	if (cdimgTOC->ImageType == CD_TYPE_CHD) {
		// Unified sector reader gives us a complete 2352-byte raw sector.
		// CD001 identifier lives at byte 1 of the 2048-byte user data area,
		// which is byte 17 (16 + 1) of the full 2352-byte sector.
		if (cdimgReadRawSector(16, (UINT8*)buf) == 0) {
			if (strncmp("CD001", buf + 16 + 1, 5) == 0) {
				buf[16 + 48] = 0;
			} else
				dprintf(_T("*** Bad CD!\n"));
		}
	} else {
		FILE* h = _tfopen(cdimgTOC->Image, _T("rb"));

		if (h) {
			if (cdimgFseek(h, 16 * 2352 + 16, SEEK_SET) == 0) {
				if (fread(buf, 1, 2048, h) == 2048) {
					if (strncmp("CD001", buf + 1, 5) == 0) {
						buf[48] = 0;
						/* BurnDrvFindMedium(buf + 40); */
					}
					else
						dprintf(_T("*** Bad CD!\n"));
				}
			}

			fclose(h);
		}
	}

	free_s((void**)&buf);
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

static INT32 cdimgPlayLBA(INT32 LBA) // audio play start
{
	cdimgStop();

	if (QChannel != NULL) { // .CCD dump w/.SUB
		if (LBA < 0 || LBA >= QChannelCount || (QChannel[LBA].Control & 0x40))
			return 1;
	} else { // .BIN/.CUE dump or .CHD
		if (cdimgTOC->TrackData[cdimgFindTrack(LBA)].Control & 0x40)
			return 1;
	}

	cdimgLBA = LBA;

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
	if (sectors_to_read > trackEnd - cdimgLBA) sectors_to_read = trackEnd - cdimgLBA;
	if (sectors_to_read <= 0) return 1;

	// Logical LBA space is shared by CHD and .bin/.cue: subtract the disc
	// pregap.  For CHD, cd_chd maps the logical LBA to its CHD frame per track.
	base = cdimgLBA - cd_pregap;

	if (base < 0) base = 0;
	cdimgAudioFilePos = base;

	if (cdimgTOC->ImageType == CD_TYPE_CHD) {

		UINT8 sector_buf[2352];
		INT32 read_count = 0;
		for (INT32 i = 0; i < sectors_to_read; i++) {
			if (cdimgReadRawSector(base + i, sector_buf, true) != 0)
				break;

			// Convert big-endian CD-DA data to native byte order
			for (INT32 j = 0; j < 2352; j += 4) {
				INT32 dst_idx = (i * 2352 + j) / 2;
				cdimgOutputbuffer[dst_idx] = (sector_buf[j] << 8) | sector_buf[j + 1];
				cdimgOutputbuffer[dst_idx + 1] = (sector_buf[j + 2] << 8) | sector_buf[j + 3];
			}
			read_count++;
		}
		cdimgAudioFilePos = base + read_count;
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
		cdimgAudioFilePos = (INT32)(cdimgFtell(cdimgFile) / 2352);
		if (cdimgOutputbufferSize <= 0) {
			cdimgStop();
			return 1;
		}
	}
	cdimgOutputPosition = 0;
	cdimgSamples = 0;
	// this breaks states, commenting for now just in-case. -dink
	//cdimgLBA = cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address); // start at the beginning of track
	CDEmuStatus = playing;

	return 0;
}

static INT32 cdimgPlay(UINT8 M, UINT8 S, UINT8 F)
{
	const UINT8 address[] = { 0, M, S, F };

	const UINT8* displayaddress = dinkLBAToMSF(cdimgMSFToLBA(address));
	dprintf(_T("    play %02i:%02i:%02i\n"), displayaddress[1], displayaddress[2], displayaddress[3]);

	return cdimgPlayLBA(cdimgMSFToLBA(address));
}

static INT32 cdimgLoadSector(INT32 LBA, char* pBuffer)
{
	if (CDEmuStatus == playing) return 0; // data loading

	INT32 originalLBA = LBA;
	if (CDEmuStatus == seeking) {
		LBA -= cd_pregap; // when seeking, we must account for pregap
		re_sync = 1;
	}

	// Unified sector reader: cdimgReadRawSector handles both .bin and .chd
	// transparently. For .bin files we still prefer the sequential-access
	// optimization below (avoids redundant fseek calls).
	if (cdimgTOC && cdimgTOC->ImageType == CD_TYPE_CHD) {
		if (cdimgReadRawSector(LBA, (UINT8*)pBuffer) != 0) {
			dprintf(_T("*** couldn't read sector (LBA %08u)\n"), originalLBA);
			return 0;
		}
		CDEmuStatus = reading;
		// cdimgLBA mirrors the .cue/.bin convention: file-relative sector
		// index of the next sector.  cdimgTrack is kept in sync using a
		// full-disc LBA (with pregap) so cdimgFindTrack returns the correct
		// track index for the BIOS standby-screen Q-channel query.
		cdimgLBA = LBA + 1;
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

		TOCEntry[3] = 0;

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
			if (QChannel != NULL && cdimgLBA >= 0 && cdimgLBA < QChannelCount) { // .CCD/.SUB
				QChannelData[0] = QChannel[cdimgLBA].track;

				QChannelData[1] = QChannel[cdimgLBA].MSFabs.M;
				QChannelData[2] = QChannel[cdimgLBA].MSFabs.S;
				QChannelData[3] = QChannel[cdimgLBA].MSFabs.F;

				QChannelData[4] = QChannel[cdimgLBA].MSFrel.M;
				QChannelData[5] = QChannel[cdimgLBA].MSFrel.S;
				QChannelData[6] = QChannel[cdimgLBA].MSFrel.F;

				QChannelData[7] = QChannel[cdimgLBA].Control;
			} else { // .BIN/.ISO / .CHD
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
			INT32 sectorsToRead = (cdimgOUT_SIZE * 4) / 2352;
			if (sectorsToRead > remainingSectors) sectorsToRead = remainingSectors;
			if (sectorsToRead <= 0) {
				cdimgStop();
				break;
			}

			if (cdimgTOC->ImageType == CD_TYPE_CHD) {
				UINT8 sectorBuffer[2352];
				INT32 readCount = 0;
				for (INT32 i = 0; i < sectorsToRead; i++) {
					if (cdimgReadRawSector(cdimgAudioFilePos + i, sectorBuffer, true) != 0) break;
					for (INT32 j = 0; j < 2352; j += 4) {
						INT32 offset = (i * 2352 + j) / 2;
						cdimgOutputbuffer[offset] = (sectorBuffer[j] << 8) | sectorBuffer[j + 1];
						cdimgOutputbuffer[offset + 1] = (sectorBuffer[j + 2] << 8) | sectorBuffer[j + 3];
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
		if (count > samples) count = samples;
		if (count > remainingSamples) count = remainingSamples;
		INT16* src = cdimgOutputbuffer + cdimgOutputPosition * 2;
		for (INT32 i = count * 2 - 1; i >= 0; i--) {
			buffer[i] = CLIP((src[i] * (cd_volume / 100.0)) + buffer[i]);
		}

		buffer += count * 2;
		cdimgOutputPosition += count;
		samples -= count;
		cdimgSamples += count;
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

static INT32 cdimgGetSettings(InterfaceInfo* pInfo)
{
	return 0;
}

struct CDEmuDo cdimgDo = { cdimgExit, cdimgInit, cdimgStop, cdimgPlay, cdimgLoadSector, cdimgReadTOC, cdimgReadQChannel, cdimgSetVolume, cdimgGetCurrentLBA, cdimgGetSoundBuffer, cdimgScan, cdimgGetSettings, _T("raw image CD emulation") };
