// FBNeo cd-img, TruRip .ccd/.sub/.img support by Jan Klaassen
// .bin/.cue re-work by dink
// .chd support: compressed CD image backend via libchdr (MAME project, BSD-3-Clause).
// The image IS the .chd file; no CUE parsing required.  Sector layout is
// detected from CHD header hunkbytes; 2048/2352/2448 byte sectors are all
// exposed to upper layers as 2352-byte raw mode-1 sectors.

#include "burner.h"
#include "burnint.h"
#include "chd.h"
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <malloc.h>
#endif

const INT32 MAXIMUM_NUMBER_TRACKS = 100;

const INT32 CD_FRAMES_MINUTE = 60 * 75;
const INT32 CD_FRAMES_SECOND =      75;

const INT32 CD_TYPE_NONE     = 1 << 0;
const INT32 CD_TYPE_BINCUE   = 1 << 1;
const INT32 CD_TYPE_CCD      = 1 << 2;
const INT32 CD_TYPE_CHD      = 1 << 3;

static INT32 cd_pregap;

struct MSF { UINT8 M; UINT8 S; UINT8 F; };

struct cdimgTRACK_DATA { UINT8 Control; UINT8 TrackNumber; UINT8 Address[4]; UINT8 EndAddress[4]; };
struct cdimgCDROM_TOC { UINT8 FirstTrack; UINT8 LastTrack; UINT8 ImageType; TCHAR Image[MAX_PATH]; cdimgTRACK_DATA TrackData[MAXIMUM_NUMBER_TRACKS]; };

static cdimgCDROM_TOC* cdimgTOC;

static FILE*  cdimgFile  = NULL;
static FILE*  cdimgChdFp = NULL;
static chd_file* cdimgChdFile = NULL;
static UINT32 cdimgChdBytesPerSector = 0;  // bytes per CD sector in CHD (2352 or 2448)
static UINT32 cdimgChdSectorsPerHunk = 0;  // number of CD sectors per hunk
static UINT8* cdimgChdHunkBuf = NULL;      // cached hunk buffer
static INT32  cdimgChdCurHunk = -1;        // currently cached hunk, -1 = none
static INT32  cdimgChdTrackStart[MAXIMUM_NUMBER_TRACKS];  // CHD start sector for each track
static INT32  cdimgTrack = 0;
static INT32  cdimgLBA   = 0;

static INT32  cdimgSamples = 0;

static INT32  re_sync = 0;

// identical to the format used in clonecd .sub files, can use memcpy
struct QData { UINT8 Control; char track; char index; MSF MSFrel; char unused; MSF MSFabs; UINT16 CRC; };

static QData* QChannel = NULL;

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

static INT32 cdimgSkip(FILE* h, INT32 samples)
{
	fseek(h, samples * 4, SEEK_CUR);

	return samples * 4;
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

static void cdimgAddLastTrack()
{ // Make a fake last-track w/total image size (for bounds checking)
	FILE* h = _wfopen(cdimgTOC->Image, _T("rb"));
	if (h)
	{
		fseek(h, 0, SEEK_END);
		const UINT8* address = cdimgLBAToMSF(((ftell(h) + 2351) / 2352) + cd_pregap);
		fclose(h);

		cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[1] = address[1];
		cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[2] = address[2];
		cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[3] = address[3];
	}
}

// parse .sub file and build a TOC based in Q sub channel data
static INT32 cdimgParseSubFile()
{
	TCHAR  filename_sub[MAX_PATH];
	INT32  length = 0;
	QData* Q      = 0;
	INT32  Qsize  = 0;
	FILE*  h;
	INT32  track  = 1;

	cdimgTOC->ImageType  = CD_TYPE_CCD;
	cdimgTOC->FirstTrack = 1;

	_tcscpy(filename_sub, CDEmuImage);
	length = _tcslen(filename_sub);

	if (length <= 4 ||
		(!IsFileExt(filename_sub, _T(".ccd")) &&
		 !IsFileExt(filename_sub, _T(".img")) &&
		 !IsFileExt(filename_sub, _T(".sub")))) {
		dprintf(_T("*** Bad image: %s\n"), filename_sub);
		return 1;
	}

	_tcscpy(cdimgTOC->Image, CDEmuImage);
	_tcscpy(cdimgTOC->Image + length - 4, _T(".img"));
	//bprintf(0, _T("Image file: %s\n"),cdimgTOC->Image);
	if (_waccess(cdimgTOC->Image, 4) == -1) {
		dprintf(_T("*** Bad image: %s\n"), cdimgTOC->Image);
		return 1;
	}

	_tcscpy(filename_sub + length - 4, _T(".sub"));
	//bprintf(0, _T("filename_sub: %s\n"),filename_sub);
	h = _wfopen(filename_sub, _T("rb"));
	if (h == 0) {
		dprintf(_T("*** Bad image: %s\n"), filename_sub);
		return 1;
	}

	fseek(h, 0, SEEK_END);

	INT32 subQidx   = 0;
	UINT32 subQsize = ftell(h);
	UINT8 *subQdata = (UINT8*)calloc(subQsize, sizeof(UINT8));

	//bprintf(0, _T("raw .sub data size: %d\n"), subQsize);
	fseek(h, 0, SEEK_SET);
	fread(subQdata, subQsize, 1, h);
	fclose(h);

	Qsize = (subQsize + 95) / 96 * sizeof(QData);
	Q = QChannel = (QData*)calloc(Qsize, sizeof(QData));

	INT32 track_linear = 1;

	while (1) {
		subQidx += 12;
		if (subQidx >= subQsize) break;
		memcpy(Q, &subQdata[subQidx], 12);
		subQidx += 12;
		subQidx += 6*12;

		if (Q->index && (Q->Control & 1) && (cdimgTOC->TrackData[bcd(Q->track) - 1].TrackNumber == 0)) {
			// new track
			track = bcd(Q->track);

			if (track == track_linear) {
				//dprintf(_T("  - Track %i found starting at %02X:%02X:%02X\n"), track, Q->MSFabs.M, Q->MSFabs.S, Q->MSFabs.F);
				//bprintf(0, _T("              contrl: %X  track %X(%d)   indx %X\n"),Q->Control,Q->track,track,Q->index);

				cdimgTOC->TrackData[track - 1].Control = Q->Control; // >> 4;
				cdimgTOC->TrackData[track - 1].TrackNumber = Q->track;
				cdimgTOC->TrackData[track - 1].Address[1] = Q->MSFabs.M;
				cdimgTOC->TrackData[track - 1].Address[2] = Q->MSFabs.S;
				cdimgTOC->TrackData[track - 1].Address[3] = Q->MSFabs.F;
				track_linear++;
			} else {
				//bprintf(0, _T("skipped weird track: %X (%X)\n"), track, Q->track);
			}
		}

		Q++;
	}

	cdimgTOC->LastTrack = track;

	free_s((void**)&subQdata);

	cd_pregap = QChannel[0].MSFabs.F + QChannel[0].MSFabs.S * CD_FRAMES_SECOND + QChannel[0].MSFabs.M * CD_FRAMES_MINUTE;
	//bprintf(0, _T("pregap lba: %d MSF: %d:%d:%d\n"), cd_pregap, QChannel[0].MSFabs.M, QChannel[0].MSFabs.S, QChannel[0].MSFabs.F);

	cdimgAddLastTrack();

	return 0;
}

static INT32 cdimgParseCueFile()
{
	TCHAR  szLine[1024];
	TCHAR  szFile[1024];
	TCHAR* s;
	TCHAR* t;
	FILE*  h;
	INT32  track = 1;
	INT32  length;

	cdimgTOC->ImageType  = CD_TYPE_BINCUE;
	cdimgTOC->FirstTrack = 1;
	cdimgTOC->LastTrack  = 1;

	cdimgTOC->TrackData[0].Address[1] = 0;
	cdimgTOC->TrackData[0].Address[2] = 2;
	cdimgTOC->TrackData[0].Address[3] = 0;

	cd_pregap = 150; // default for bin/cue?

	// derive .bin name from .cue filename (it gets the actual name from the .cue, below)
	length = _tcslen(CDEmuImage);
	_tcscpy(cdimgTOC->Image, CDEmuImage);
	_tcscpy(cdimgTOC->Image + length - 4, _T(".bin"));
	//bprintf(0, _T("Image file: %s\n"),cdimgTOC->Image);

	h = _tfopen(CDEmuImage, _T("rt"));
	if (!h) {
		return 1;
	}

	while (1) {
		if (!_fgetts(szLine, sizeof(szLine), h)) {
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

			TCHAR* szQuote;

			// read filename
			QuoteRead(&szQuote, NULL, s);

			_sntprintf(szFile, ExtractFilename(CDEmuImage) - CDEmuImage, _T("%s"), CDEmuImage);
			_sntprintf(szFile + (ExtractFilename(CDEmuImage) - CDEmuImage), 1024 - (ExtractFilename(CDEmuImage) - CDEmuImage), _T("\\%s"), szQuote);

			if (track == 1) {
				//bprintf(0, _T("Image file (from .CUE): %s\n"), szFile);
				_tcscpy(cdimgTOC->Image, szFile);
			}
			continue;
		}

		// track info
		if ((t = LabelCheck(s, _T("TRACK"))) != 0) {
			s = t;

			// track number
			track = _tcstol(s, &t, 10);

			if (track < 1 || track > MAXIMUM_NUMBER_TRACKS) {
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

		// PREGAP (not handled)
		if ((t = LabelCheck(s, _T("PREGAP"))) != 0) {
			continue;
		}

		// TRACK Index
		if ((t = LabelCheck(s, _T("INDEX 01"))) != 0) {
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

			continue;
		}
	}

	fclose(h);

	cdimgAddLastTrack();

	return 0;
}

// -----------------------------------------------------------------------------

static INT32 cdimgExit()
{
	cdimgExitStream();

	if (cdimgFile)
		fclose(cdimgFile);
	cdimgFile = NULL;

	if (cdimgChdFile)
		chd_close(cdimgChdFile);
	cdimgChdFile = NULL;

	if (cdimgChdFp)
		fclose(cdimgChdFp);
	cdimgChdFp = NULL;

	free_s((void**)&cdimgChdHunkBuf);
	cdimgChdCurHunk = -1;

	cdimgTrack = 0;
	cdimgLBA   = 0;

	free_s((void**)&cdimgTOC);
	free_s((void**)&QChannel);
	QChannel = NULL;

	return 0;
}

// ---------------------------------------------------------------------------
// Low-level CHD sector decompressor.  Uses libchdr (chd_open_file / chd_read /
// chd_close / chd_get_header) to translate a CHD-relative sector index into
// a 2352-byte raw mode-1 sector.  The caller is responsible for translating
// system LBAs to CHD-relative sector offsets.
//
//   sector : 0-based sector index inside the CHD file (no pregap offset)
//   dest   : caller-provided buffer, must hold at least 2352 bytes
//   return : 0 on success, non-zero on error
static INT32 cdimgChdReadSector(INT32 sector, UINT8* dest)
{
	// Decompress a single CHD sector into a 2352-byte raw mode-1 sector.
	// Preconditions (enforced by callers — cdimgParseChdFile must have succeeded):
	//   cdimgChdFile != NULL, cdimgChdHunkBuf != NULL, cdimgChdSectorsPerHunk > 0
	//   sector is within the image bounds (header->totalhunks * cdimgChdSectorsPerHunk)
	if (!cdimgChdFile || !cdimgChdHunkBuf || cdimgChdSectorsPerHunk == 0)
		return 1;

	INT32 hunk = sector / (INT32)cdimgChdSectorsPerHunk;
	INT32 offset_in_hunk = (sector % (INT32)cdimgChdSectorsPerHunk) * (INT32)cdimgChdBytesPerSector;

	// One-slot hunk cache: decompress on miss, reuse on hit.
	if (cdimgChdCurHunk != hunk) {
		chd_error err = chd_read(cdimgChdFile, (UINT32)hunk, cdimgChdHunkBuf);
		if (err != CHDERR_NONE)
			return 1;
		cdimgChdCurHunk = hunk;
	}

	// Always deliver exactly 2352 bytes to callers. If the CHD stores
	// sub-channel data (2448 byte/sector layout) we simply drop those trailing
	// bytes — the upper layers do not need them.
	UINT32 copy_size = (cdimgChdBytesPerSector < 2352) ? cdimgChdBytesPerSector : 2352;
	memcpy(dest, cdimgChdHunkBuf + offset_in_hunk, copy_size);
	return 0;
}

// ---------------------------------------------------------------------------
// Unified sector reader for the cd_img backend.  Regardless of whether the
// underlying image is a plain .bin/.cue or a compressed .chd, callers invoke
// this function with a data-track-relative LBA (LBA 0 == first user sector,
// same coordinate system as fseek(offset*2352) in a .bin file).  The returned
// buffer is always a 2352-byte raw mode-1 sector.
//
//   lba    : sector index relative to data-track start (0-based)
//   dest   : caller-provided 2352-byte buffer
//   return : 0 on success, non-zero on error
static INT32 cdimgReadRawSector(INT32 lba, UINT8* dest)
{
	if (!cdimgTOC)
		return 1;

	if (cdimgTOC->ImageType == CD_TYPE_CHD) {
		// CHD sector numbering has the same origin as a .bin file:
		// sector 0 of the CHD is the first sector of the data track.
		return cdimgChdReadSector(lba, dest);
	}

	// Fall-through: standard raw image (.bin/.cue, .ccd/.img).
	if (!cdimgFile)
		return 1;
	if (fseek(cdimgFile, (long)lba * 2352, SEEK_SET) != 0)
		return 1;
	size_t n = fread(dest, 1, 2352, cdimgFile);
	return (n == 2352) ? 0 : 1;
}

// Parse a "FRAMES:%d" pattern in a CHD metadata string.
// Returns the integer value, or -1 if not found.
static INT32 chd_meta_get_frames(const char* meta)
{
	const char* p = strstr(meta, "FRAMES:");
	if (!p) return -1;
	return atoi(p + 7);
}

// Determ
// Determine if a track described by CHD metadata is a data track.
// TYPE field starting with "MODE" (e.g. "MODE1_RAW", "MODE1/2352") = data track.
// TYPE = "AUDIO" = audio track.  Defaults to data track on parse error.
static bool chd_meta_is_data_track(const char* meta)
{
	const char* p = strstr(meta, "TYPE:");
	if (!p) return true; // assume data if no type found
	const char* type_start = p + 5;
	// Audio tracks: TYPE=AUDIO (or VAUDIO, etc.)
	if (strncmp(type_start, "AUDIO", 5) == 0) return false;
	// Data tracks: TYPE starts with "MODE"
	if (strncmp(type_start, "MODE", 4) == 0) return true;
	// Fallback: treat unknown types as data tracks
	return true;
}

// Count the number of audio tracks (TYPE:AUDIO) in a CHD file.
// pszFile — TCHAR path to the .chd file (pass NULL to use CDEmuImage)
// Returns the number of audio tracks found in the CHD metadata.
// Returns 0 if no audio tracks are found or if the file cannot be opened.
INT32 cdimgCountChdAudioTracks(TCHAR* pszFile)
{
    TCHAR* pszPath = pszFile ? pszFile : CDEmuImage;
    if (!pszPath || _tcslen(pszPath) < 5) {
        bprintf(PRINT_ERROR, _T("cdimgCountChdAudioTracks: invalid path\n"));
        return 0;
    }

    FILE* fp = _tfopen(pszPath, _T("rb"));
    if (!fp) {
        bprintf(PRINT_ERROR, _T("cdimgCountChdAudioTracks: _tfopen failed for %s\n"), pszPath);
        return 0;
    }

    chd_file* chd = NULL;
    chd_error err = chd_open_file(fp, CHD_OPEN_READ, NULL, &chd);
    if (err != CHDERR_NONE) {
        bprintf(PRINT_ERROR, _T("cdimgCountChdAudioTracks: chd_open_file failed for %s, error=%d\n"), pszPath, (int)err);
        fclose(fp);
        return 0;
    }

    INT32 audio_track_count = 0;
    char meta_buf[256];
    UINT32 meta_resultlen = 0;
    UINT8  meta_flags = 0;

    for (int idx = 0; idx < 99; idx++) {
        err = chd_get_metadata(chd, CDROM_TRACK_METADATA2_TAG, idx,
            meta_buf, sizeof(meta_buf) - 1, &meta_resultlen, NULL, &meta_flags);
        if (err != CHDERR_NONE || meta_resultlen == 0) {
            err = chd_get_metadata(chd, CDROM_TRACK_METADATA_TAG, idx,
                meta_buf, sizeof(meta_buf) - 1, &meta_resultlen, NULL, &meta_flags);
        }
		// GDROM Reserved
        if (err != CHDERR_NONE || meta_resultlen == 0) {
            err = chd_get_metadata(chd, GDROM_TRACK_METADATA_TAG, idx,
                meta_buf, sizeof(meta_buf) - 1, &meta_resultlen, NULL, &meta_flags);
        }
        if (err != CHDERR_NONE || meta_resultlen == 0)
            break;

        meta_buf[meta_resultlen] = '\0';

        if (!chd_meta_is_data_track(meta_buf))
            audio_track_count++;
    }

    chd_close(chd);
    fclose(fp);  // core_stdio_nonowner: chd_close does not close FILE*, we must
    return audio_track_count;
}

// Parse a "PREGAP:%d" pattern.  Returns 0 if not present.
static INT32 chd_meta_get_pregap(const char* meta)
{
	const char* p = strstr(meta, "PREGAP:");
	if (!p) return 0;
	return atoi(p + 7);
}

// Parse a "POSTGAP:%d" pattern.  Returns 0 if not present.
static INT32 chd_meta_get_postgap(const char* meta)
{
	const char* p = strstr(meta, "POSTGAP:");
	if (!p) return 0;
	return atoi(p + 7);
}

static INT32 cdimgParseChdFile()
{
	FILE* fp = _tfopen(CDEmuImage, _T("rb"));
	if (!fp) {
		dprintf(_T("*** Couldn't open .chd file\n"));
		return 1;
	}
	chd_error err = chd_open_file(fp, CHD_OPEN_READ, NULL, &cdimgChdFile);
	if (err != CHDERR_NONE) {
		dprintf(_T("*** Couldn't open .chd file\n"));
		fclose(fp);
		return 1;
	}
	cdimgChdFp = fp;

	const chd_header* header = chd_get_header(cdimgChdFile);
	if (!header) {
		dprintf(_T("*** Couldn't read CHD header\n"));
		chd_close(cdimgChdFile);
		cdimgChdFile = NULL;
		if (cdimgChdFp)
			fclose(cdimgChdFp);
		cdimgChdFp = NULL;
		return 1;
	}

	cdimgTOC->ImageType  = CD_TYPE_CHD;
	_tcscpy(cdimgTOC->Image, CDEmuImage);

	// Standard CD pregap is 150 sectors (2 seconds).
	// CHD stores complete CD image data, so the disc pregap is implicit.
	cd_pregap = 150;

	// Detect sector layout: CHD hunk size may contain multiple CD sectors.
	// Common formats: 2352 bytes/sector (raw mode 1), 2448 bytes/sector (raw mode 1 + subchannel).
	if (       header->hunkbytes % 2448 == 0) {
		cdimgChdBytesPerSector = 2448;
	} else if (header->hunkbytes % 2352 == 0) {
		cdimgChdBytesPerSector = 2352;
	} else if (header->hunkbytes % 2048 == 0) {
		cdimgChdBytesPerSector = 2048;
	} else {
		cdimgChdBytesPerSector = header->hunkbytes;
	}
	cdimgChdSectorsPerHunk = header->hunkbytes / cdimgChdBytesPerSector;

	// Allocate hunk decompression buffer.
	cdimgChdHunkBuf = (UINT8*)malloc(header->hunkbytes);
	if (!cdimgChdHunkBuf) {
		dprintf(_T("*** Out of memory for CHD hunk buffer\n"));
		chd_close(cdimgChdFile);
		cdimgChdFile = NULL;
		
		if (cdimgChdFp)
			fclose(cdimgChdFp);
		cdimgChdFp = NULL;
		return 1;
	}
	cdimgChdCurHunk = -1;

	// ----------------------------------------------------------------
	// Parse CHD metadata to build the CD TOC, mirroring the approach
	// used by neocd_libretro.  Try V2 metadata first, fall back to V1.
	// ----------------------------------------------------------------
	char   meta_buf[256];
	UINT32 meta_resultlen = 0;
	UINT8  meta_flags     = 0;
	INT32  total_sectors  = 0;
	INT32  trk            = 0;
	UINT32 chd_sector_pos = 0;   // sector position within the CHD file
	UINT32 cd_lba         = 0;   // absolute disc LBA

	// Scan for up to 99 track metadata entries.
	for (INT32 idx = 0; idx < 99; idx++) {
		// Try V2 metadata first, then V1.
		err = chd_get_metadata(cdimgChdFile, CDROM_TRACK_METADATA2_TAG, idx,
			meta_buf, sizeof(meta_buf) - 1, &meta_resultlen, NULL, &meta_flags);
		if (err != CHDERR_NONE || meta_resultlen == 0) {
			err = chd_get_metadata(cdimgChdFile, CDROM_TRACK_METADATA_TAG, idx,
				meta_buf, sizeof(meta_buf) - 1, &meta_resultlen, NULL, &meta_flags);
		}
		if (err != CHDERR_NONE || meta_resultlen == 0)
			break;

		meta_buf[meta_resultlen] = '\0';

		INT32 frames  = chd_meta_get_frames(meta_buf);
		if (frames <= 0) frames = 0;

		INT32 pregap  = chd_meta_get_pregap(meta_buf);
		INT32 postgap = chd_meta_get_postgap(meta_buf);

		bool is_data  = chd_meta_is_data_track(meta_buf);

		// Align CHD position to 4-sector boundary (required by some rips).
		if (chd_sector_pos % 4)
			chd_sector_pos += 4 - (chd_sector_pos % 4);

		// Pregap (index 0) — counted in disc LBA but not in CHD position
		// unless PGTYPE indicates VAUDIO (virtual audio pregap stored in CHD).
		// For simplicity, treat all pregaps as non-data and consume them
		// from the disc LBA only.  If the CHD really has pregap data the
		// difference is at most 150 sectors which is acceptable.
		if (pregap > 0) {
			cd_lba += pregap;
		}

		// Set up the TOC entry for this track.
		cdimgTOC->TrackData[trk].Control     = is_data ? 0x41 : 0x01;
		cdimgTOC->TrackData[trk].TrackNumber = tobcd(trk + 1);

		// Record the CHD file position (sector index) for this track so that
		// audio/data playback can translate disc-LBA to CHD-sector correctly,
		// accounting for per-track pregaps/postgaps that only advance cd_lba.
		cdimgChdTrackStart[trk] = (INT32)chd_sector_pos;

		const UINT8* track_start_msf = cdimgLBAToMSF(cd_lba);
		cdimgTOC->TrackData[trk].Address[0] = 0;
		cdimgTOC->TrackData[trk].Address[1] = track_start_msf[1];
		cdimgTOC->TrackData[trk].Address[2] = track_start_msf[2];
		cdimgTOC->TrackData[trk].Address[3] = track_start_msf[3];

		// Advance disc LBA and CHD sector position.
		cd_lba += frames;
		chd_sector_pos += frames;

		// Postgap — counted in disc LBA.
		if (postgap > 0)
			cd_lba += postgap;

		total_sectors = cd_lba;
		trk++;

		if (trk >= MAXIMUM_NUMBER_TRACKS)
			break;
	}

	// If no metadata was found, fall back to a single data track.
	if (trk == 0) {
		trk = 1;
		total_sectors = (INT32)(header->totalhunks) * cdimgChdSectorsPerHunk;

		cdimgTOC->TrackData[0].Control     = 0x41;
		cdimgTOC->TrackData[0].TrackNumber = tobcd(1);
		cdimgChdTrackStart[0] = 0;
		const UINT8* start_msf = cdimgLBAToMSF(cd_pregap);
		cdimgTOC->TrackData[0].Address[0]  = 0;
		cdimgTOC->TrackData[0].Address[1]  = start_msf[1];
		cdimgTOC->TrackData[0].Address[2]  = start_msf[2];
		cdimgTOC->TrackData[0].Address[3]  = start_msf[3];

		const UINT8* end_msf = cdimgLBAToMSF(total_sectors + cd_pregap);
		cdimgTOC->TrackData[0].EndAddress[0] = 0;
		cdimgTOC->TrackData[0].EndAddress[1] = end_msf[1];
		cdimgTOC->TrackData[0].EndAddress[2] = end_msf[2];
		cdimgTOC->TrackData[0].EndAddress[3] = end_msf[3];
	} else {
		// Fill EndAddress for all parsed tracks.
		for (INT32 i = 0; i < trk; i++) {
			UINT32 end_lba = 0;
			if (i + 1 < trk) {
				end_lba = cdimgMSFToLBA(cdimgTOC->TrackData[i + 1].Address);
			} else {
				end_lba = total_sectors;
			}
			const UINT8* end_msf = cdimgLBAToMSF(end_lba);
			cdimgTOC->TrackData[i].EndAddress[0] = 0;
			cdimgTOC->TrackData[i].EndAddress[1] = end_msf[1];
			cdimgTOC->TrackData[i].EndAddress[2] = end_msf[2];
			cdimgTOC->TrackData[i].EndAddress[3] = end_msf[3];
		}
	}

	cdimgTOC->FirstTrack = 1;
	cdimgTOC->LastTrack  = trk;

	// Lead-out track — position just past the last valid sector on disc.
	// This ensures cdimgFindTrack correctly identifies the last audio/data
	// track when LBA is within the disc, and the lead-out only when LBA
	// is strictly beyond all tracks.
	UINT32 leadout_lba = total_sectors + cd_pregap;
	const UINT8* leadout_msf = cdimgLBAToMSF(leadout_lba);
	cdimgTOC->TrackData[trk].Control     = 0x41;
	cdimgTOC->TrackData[trk].TrackNumber = 0xAA;
	cdimgTOC->TrackData[trk].Address[0]  = 0;
	cdimgTOC->TrackData[trk].Address[1]  = leadout_msf[1];
	cdimgTOC->TrackData[trk].Address[2]  = leadout_msf[2];
	cdimgTOC->TrackData[trk].Address[3]  = leadout_msf[3];

	bprintf(0, _T("CHD: %u hunks, %u bytes/hunk (%u sectors/hunk, %u bytes/sector), %d tracks parsed from metadata\n"),
		(UINT32)header->totalhunks, header->hunkbytes,
		cdimgChdSectorsPerHunk, cdimgChdBytesPerSector, trk);

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

	if (_tcslen(filename) < 4)
		return 1;

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

	cdimgInitStream();

	// Buffer for sector data - always 2352 for CD sectors
	char* buf = (char*)malloc(2352);
	if (!buf) {
		dprintf(_T("*** Out of memory for CD buffer\n"));
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
		FILE* h = _wfopen(cdimgTOC->Image, _T("rb"));

		if (h) {
			if (fseek(h, 16 * 2352 + 16, SEEK_SET) == 0) {
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
	// cdimgChdFile is NOT closed here - it stays open for the lifetime of the CD session,
	// until cdimgExit()
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
		if (QChannel[LBA].Control & 0x40)
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

	if (cdimgTOC->ImageType == CD_TYPE_CHD) {
		// CHD: translate disc LBA to CHD sector using per-track start.
		// Simply subtracting cd_pregap is wrong for tracks 2+ because
		// pregap/postgap entries only advance cd_lba (disc) without
		// advancing chd_sector_pos (CHD file position).
		INT32 track_start_lba = cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address);
		INT32 offset_in_track = cdimgLBA - track_start_lba;
		base = cdimgChdTrackStart[cdimgTrack] + offset_in_track;
	} else {
		// .bin/.cue — file-relative sector index
		base = cdimgLBA - cd_pregap;
	}

	// Initialize audio file position tracker for subsequent buffer refills
	cdimgAudioFilePos = base;

	if (cdimgTOC->ImageType == CD_TYPE_CHD) {

		UINT8 sector_buf[2352];
		INT32 read_count = 0;
		if (base < 0) base = 0;
		for (INT32 i = 0; i < sectors_to_read; i++) {
			if (cdimgReadRawSector(base + i, sector_buf) != 0)
				break;

			// Convert big-endian CD-DA data to native byte order
			for (INT32 j = 0; j < 2352; j += 4) {
				INT32 dst_idx = (i * 2352 + j) / 2;
				cdimgOutputbuffer[dst_idx] = (sector_buf[j] << 8) | sector_buf[j + 1];
				cdimgOutputbuffer[dst_idx + 1] = (sector_buf[j + 2] << 8) | sector_buf[j + 3];
			}
			read_count++;
		}
		// each CHD sector == 2352 bytes == 588 stereo 16-bit samples (4 bytes each)
		cdimgOutputbufferSize = read_count * (2352 / 4);
		if (read_count == 0) {
			cdimgStop();
			return 1;
		}

	} else {

		cdimgFile = _wfopen(cdimgTOC->Image, _T("rb"));
		if (cdimgFile == NULL)
			return 1;

		// advance if we're not starting at the beginning of a CD
		if (cdimgLBA > cd_pregap)
			cdimgSkip(cdimgFile, (cdimgLBA - cd_pregap) * (44100 / CD_FRAMES_SECOND));

		// fill the input buffer (size returned as count of 4-byte sample units)
		cdimgOutputbufferSize = (int)fread(cdimgOutputbuffer, 4, cdimgOUT_SIZE, cdimgFile);
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

			cdimgFile = _wfopen(cdimgTOC->Image, _T("rb"));
			if (!cdimgFile)
				return 0;
		}

		//bprintf(PRINT_IMPORTANT, _T("    loading data at LBA %08u 0x%08X\n"), (LBA - cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address)) * 2352, LBA * 2352);

		if (fseek(cdimgFile, (LBA) * 2352, SEEK_SET)) {
			dprintf(_T("*** couldn't seek (LBA %08u)\n"), LBA);

			//cdimgStop(); // stopping here will break ssrpg,
			// game will seek away & recover from this.

			return 0;
		}

		CDEmuStatus = reading;
	}

	//dprintf(_T("    reading LBA %08i 0x%08X"), LBA, ftell(cdimgFile));

	// Raw-image path (.cue / .ccd / .bin).  Keep cdimgLBA semantics
	// exactly as before (file sector index) — this historically produces
	// the correct standby-screen output.
	cdimgLBA = cdimgMSFToLBA(cdimgTOC->TrackData[0].Address) + (ftell(cdimgFile) + 2351) / 2352 - cd_pregap;

	bool status = (fread(pBuffer, 1, 2352, cdimgFile) <= 0);

	if (status) {
		dprintf(_T("*** couldn't read from file - iso corrupt or truncated?\n"));

		//cdimgStop(); - stopping here will break puzzle bobble!  game needs fail @ end of image w/o stopping :)

		return 0;
	}
	// dprintf(_T("    [ %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X ]\n"), pBuffer[0], pBuffer[1], pBuffer[2], pBuffer[3], pBuffer[4], pBuffer[5], pBuffer[6], pBuffer[7], pBuffer[8], pBuffer[9], pBuffer[10], pBuffer[11], pBuffer[12], pBuffer[13], pBuffer[14], pBuffer[15]);

	return cdimgLBA;
}

static UINT8* cdimgReadTOC(INT32 track)
{
	static UINT8 TOCEntry[4];

	memset(&TOCEntry, 0, sizeof(TOCEntry));

	if (track == CDEmuTOC_FIRSTLAST) {
		TOCEntry[0] = tobcd(cdimgTOC->FirstTrack - 1);
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
	if (track >= cdimgTOC->FirstTrack - 1 && track <= cdimgTOC->LastTrack) {
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
			if (QChannel != NULL) { // .CCD/.SUB
				QChannelData[0] = QChannel[cdimgLBA].track;

				QChannelData[1] = QChannel[cdimgLBA].MSFrel.M;
				QChannelData[2] = QChannel[cdimgLBA].MSFrel.S;
				QChannelData[3] = QChannel[cdimgLBA].MSFrel.F;

				QChannelData[4] = QChannel[cdimgLBA].MSFrel.M;
				QChannelData[5] = QChannel[cdimgLBA].MSFrel.S;
				QChannelData[6] = QChannel[cdimgLBA].MSFrel.F;

				QChannelData[7] = QChannel[cdimgLBA].Control;
			} else { // .BIN/.ISO / .CHD
				// cdimgLBA is the file-relative sector index for .cue
				// (data-track origin). For .chd we additionally keep
				// cdimgTrack set by cdimgFindTrack to identify which contains
				// the current track.
				const UINT8* AddressAbs = cdimgLBAToMSF(cdimgLBA);
				const UINT8* AddressRel = cdimgLBAToMSF(cdimgLBA - cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address));

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

static INT32 cdimgGetSoundBuffer(INT16* buffer, INT32 samples)
{

#define CLIP(A) ((A) < -0x8000 ? -0x8000 : (A) > 0x7fff ? 0x7fff : (A))

	if (CDEmuStatus != playing) {
		memset(cdimgOutputbuffer, 0x00, cdimgOUT_SIZE * 2 * sizeof(INT16));
		return 0;
	}

	cdimgSamples += samples;
	while (cdimgSamples > (44100 / CD_FRAMES_SECOND)) {
		cdimgSamples -= (44100 / CD_FRAMES_SECOND);
		cdimgLBA++;

/*		if (cdimgFile == NULL) // play next track?  bad idea. -dink
			if (cdimgLBA >= cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1].Address))
				cdimgPlayLBA(cdimgLBA); */
	}

#if 0
	extern INT32 counter;
	if (counter) {
		const UINT8* displayaddress = dinkLBAToMSF(cdimgLBA);
		dprintf(_T("  index  %02i:%02i:%02i"), displayaddress[1], displayaddress[2], displayaddress[3]);
		INT32 endt = cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1 /* next track */].Address);
		const UINT8* displayaddressend = dinkLBAToMSF(endt);
		dprintf(_T("    end  %02i:%02i:%02i\n"), displayaddressend[1], displayaddressend[2], displayaddressend[3]);
	}
#endif

	// --- End-of-track check (shared for both .bin and .chd — both use TOC).
	if (cdimgLBA >= cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1].Address)) {
		bprintf(0, _T("End of audio track %d reached!! stopping.\n"), cdimgTrack + 1);
		cdimgStop();
		return 0;
	}

	// --- Buffer-underflow refill.  Drain whatever is left, then refill from disk/chd.
	if ((cdimgOutputPosition + samples) >= cdimgOutputbufferSize) {
		INT16* src = cdimgOutputbuffer + cdimgOutputPosition * 2;
		INT16* dst = buffer;

		for (INT32 i = (cdimgOutputbufferSize - cdimgOutputPosition) * 2 - 1; i > 0; ) {
			dst[i] = CLIP((src[i]) + dst[i]); i--;
			dst[i] = CLIP((src[i]) + dst[i]); i--;
		}

		buffer += (cdimgOutputbufferSize - cdimgOutputPosition) * 2;
		samples -= (cdimgOutputbufferSize - cdimgOutputPosition);

		cdimgOutputPosition = 0;

		// Refill the buffer: data source selected by image container type.
		if (cdimgTOC->ImageType == CD_TYPE_CHD) {
			// CHD: decompress sectors one at a time, each 2352 bytes.
			// CD-DA audio is stored as big-endian 16-bit stereo samples,
			// so we need to convert from big-endian to native byte order.
			INT32 sectors_to_read = (cdimgOUT_SIZE * 4) / 2352;
			INT32 read_count = 0;
			INT32 base = cdimgAudioFilePos;  // Use audio file position tracker
			if (base < 0) base = 0;

			UINT8 sector_buf[2352];

			for (INT32 i = 0; i < sectors_to_read; i++) {
				if (cdimgReadRawSector(base + i, sector_buf) != 0)
					break;

				// Convert big-endian CD-DA data to native byte order
				for (INT32 j = 0; j < 2352; j += 4) {
					INT32 dst_idx = (i * 2352 + j) / 2;
					cdimgOutputbuffer[dst_idx] = (sector_buf[j] << 8) | sector_buf[j + 1];
					cdimgOutputbuffer[dst_idx + 1] = (sector_buf[j + 2] << 8) | sector_buf[j + 3];
				}
				read_count++;
			}
			// Advance audio file position for next buffer refill
			cdimgAudioFilePos += read_count;

			cdimgOutputbufferSize = read_count * (2352 / 4);
			if (cdimgOutputbufferSize <= 0)
				cdimgStop();
		} else {
			// BIN / raw: rely on the FILE* stream.  If it was lost (rare),
			// attempt to re-open at the current LBA.
			if (cdimgFile == NULL) {
				bprintf(0, _T("CDDA file pointer lost, re-starting @ %d!\n"), cdimgLBA);
				if (cdimgLBA < cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1].Address))
					cdimgPlayLBA(cdimgLBA);
			}
			if (cdimgFile == NULL) {
				cdimgStop();
				return 0;
			}
			if ((cdimgOutputbufferSize = fread(cdimgOutputbuffer, 4, cdimgOUT_SIZE, cdimgFile)) <= 0)
				cdimgStop();
		}
	}

	if ((cdimgOutputPosition + samples) < cdimgOutputbufferSize) {
		INT16* src = cdimgOutputbuffer + cdimgOutputPosition * 2;
		INT16* dst = buffer;

		for (INT32 i = samples * 2 - 1; i > 0; ) {
			dst[i] = CLIP((src[i]) + dst[i]); i--;
			dst[i] = CLIP((src[i]) + dst[i]); i--;
		}

		cdimgOutputPosition += samples;
	}

	return 0;

#undef CLIP

}

static INT32 cdimgScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_VOLATILE) {
		SCAN_VAR(CDEmuStatus);
		SCAN_VAR(cdimgTrack);
		SCAN_VAR(cdimgLBA);

		SCAN_VAR(cdimgOutputPosition);
		SCAN_VAR(cdimgSamples);
		SCAN_VAR(cdimgOutputbufferSize);
	}

	if (nAction & ACB_WRITE && nAction & ACB_RUNAHEAD) { // run-ahead system state load
		re_sync = 1;
	}

	if (nAction & ACB_WRITE && ~nAction & ACB_RUNAHEAD) { // regular state load, close file - it will recover
		cdimgCloseFile();
	}

	return 0;
}

static INT32 cdimgGetSettings(InterfaceInfo* pInfo)
{
	return 0;
}

struct CDEmuDo cdimgDo = { cdimgExit, cdimgInit, cdimgStop, cdimgPlay, cdimgLoadSector, cdimgReadTOC, cdimgReadQChannel, cdimgGetSoundBuffer, cdimgScan, cdimgGetSettings, _T("raw image CD emulation") };
