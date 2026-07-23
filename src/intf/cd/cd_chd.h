// Platform-independent CHD (MAME Compressed Hunks of Data) CD/GD-ROM backend.
// Built on libchdr.  Presents a MAME-faithful three-coordinate TOC and a
// type-aware sector reader so front-ends never deal with hunk geometry.

#ifndef CD_CHD_H
#define CD_CHD_H

// Track types (mirror libchdr cdrom.h so consumers need not include it).
enum {
	CHD_TRACK_MODE1 = 0,      // mode 1, 2048 data bytes
	CHD_TRACK_MODE1_RAW,      // mode 1, 2352 raw bytes
	CHD_TRACK_MODE2,          // mode 2, 2336 data bytes
	CHD_TRACK_MODE2_FORM1,    // mode 2 form 1, 2048 data bytes
	CHD_TRACK_MODE2_FORM2,    // mode 2 form 2, 2324 data bytes
	CHD_TRACK_MODE2_FORM_MIX, // mode 2 mixed, 2336 data bytes
	CHD_TRACK_MODE2_RAW,      // mode 2, 2352 raw bytes
	CHD_TRACK_AUDIO,          // redbook audio, 2352 bytes

	CHD_TRACK_RAW_DONTCARE    // read: hand back whatever the track stores
};

// Container class, decided at open time.
enum {
	CHD_CONTAINER_NONE = 0,
	CHD_CONTAINER_CD,     // CHT2 / CHTR metadata
	CHD_CONTAINER_GDROM,  // CHGD metadata
	CHD_CONTAINER_HDD     // GDDD / hard-disk metadata (e.g. CPS-2 data)
};

struct ChdTrack {
	INT32 nType;        // CHD_TRACK_*
	INT32 nSubType;     // 0 none / 1 normal / 2 raw
	INT32 nDataSize;    // 2048 / 2324 / 2336 / 2352
	INT32 nSubSize;     // 0 or 96
	INT32 nFrames;      // sectors in the track (incl. pregap)
	INT32 nPregap;      // pregap sectors
	INT32 nPostgap;     // postgap sectors
	INT32 nPadFrames;   // GD-ROM padding sectors
	INT32 nExtraFrames; // chdman 4-frame-boundary padding
	INT32 nControl;     // Q-channel control nibble << 4 flags (0x41 data / 0x01 audio)
	INT32 nPhysFrameOfs;
	INT32 nChdFrameOfs;
	INT32 nLogFrameOfs;
	INT32 nLogFrames;
};

struct ChdImage;

// Open a CHD by path.  Owns the underlying FILE* and closes it on ChdClose.
// Returns NULL on failure.
ChdImage* ChdOpenFile(const TCHAR* szPath);
void      ChdClose(ChdImage* pImage);

INT32 ChdGetContainerType(ChdImage* pImage);
INT32 ChdGetNumTracks(ChdImage* pImage);
const ChdTrack* ChdGetTrack(ChdImage* pImage, INT32 nTrack);

// Human-readable names for debug output.
const TCHAR* ChdContainerName(INT32 nContainer);
const TCHAR* ChdTrackTypeName(INT32 nType);

// CHD format version (1..5), 0 if unavailable.
INT32 ChdGetVersion(ChdImage* pImage);
INT32 ChdGetHunkBytes(ChdImage* pImage);
INT32 ChdGetFramesPerHunk(ChdImage* pImage);

// Total logical sectors on the disc (lead-out LBA).
INT32 ChdGetTotalFrames(ChdImage* pImage);

// Read one logical sector, converting to nDataType (a CHD_TRACK_* value, or
// CHD_TRACK_RAW_DONTCARE for the track's native form).  pDest must hold the
// requested type's byte count.  Returns 0 on success.
INT32 ChdReadSector(ChdImage* pImage, INT32 nLba, INT32 nDataType, UINT8* pDest);

// Read nLength raw bytes at CHD frame nChdFrame + nOffset (no conversion).
// For GD-ROM / HDD consumers that address the CHD directly.  Returns 0 on success.
INT32 ChdReadRaw(ChdImage* pImage, INT32 nChdFrame, INT32 nOffset, INT32 nLength, UINT8* pDest);

// Count audio tracks without keeping the image open.
INT32 ChdCountAudioTracks(const TCHAR* szPath);

#endif // CD_CHD_H
