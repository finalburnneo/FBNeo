#ifndef CD_IMG_H_
#define CD_IMG_H_

#define CDIMAGE_MAX_TRACKS       (99)
#define CDIMAGE_MAX_SOURCE_FILES (CDIMAGE_MAX_TRACKS + 2)
#define CDIMAGE_SHA1_SIZE        (20)

enum {
	CDIMAGE_TRACK_AUDIO = 0,
	CDIMAGE_TRACK_MODE1_2048,
	CDIMAGE_TRACK_MODE1_2352,
	CDIMAGE_TRACK_MODE2_2336,
	CDIMAGE_TRACK_MODE2_2352
};

struct CDImageTrack {
	INT32 nNumber;
	INT32 nType;
	INT32 nControl;
	INT32 nStartLBA;
	INT32 nIndex0LBA;
	INT32 nIndex1LBA;
	INT32 nPregap;
	INT32 nSectors;
	INT32 nSectorSize;
	INT32 nUserOffset;
	INT32 nUserSize;
	const TCHAR* szPath;
};

struct CDImage;

CDImage* CDImageOpen(const TCHAR* szPath);
void  CDImageClose(CDImage* pImage);
INT32 CDImageGetTrackCount(const CDImage* pImage);
INT32 CDImageGetAudioTrackCount(const CDImage* pImage);
INT32 CDImageGetSectorCount(const CDImage* pImage);
const TCHAR* CDImageGetPath(const CDImage* pImage);
INT32 CDImageGetSourceFileCount(const CDImage* pImage);
const TCHAR* CDImageGetSourceFile(const CDImage* pImage, INT32 nFile);
const CDImageTrack* CDImageGetTrack(const CDImage* pImage, INT32 nTrack);
INT32 CDImageReadRawSector(CDImage* pImage, INT32 nLba, UINT8* pDest);
INT32 CDImageReadDataSector(CDImage* pImage, INT32 nLba, UINT8* pDest);
INT32 CDImageReadUserSector(CDImage* pImage, INT32 nLba, UINT8* pDest, INT32* pnSize);
INT32 CDImageGetChdSha1(const CDImage* pImage, UINT8* pSha1);

INT32 cdimgCountChdAudioTracks(TCHAR* pszFile);
extern struct CDEmuDo cdimgDo;

#endif
