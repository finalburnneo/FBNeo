#include "retro_common.h"
#include "retro_cdemu.h"

#define DPRINTF_BUFFER_SIZE 512
char dprintf_buf[DPRINTF_BUFFER_SIZE];
static INT32 __cdecl libretro_dprintf(TCHAR* szFormat, ...)
{
	va_list vp;
	va_start(vp, szFormat);
	int rc = vsnprintf(dprintf_buf, DPRINTF_BUFFER_SIZE, szFormat, vp);
	va_end(vp);

	if (rc >= 0)
		log_cb(RETRO_LOG_INFO, dprintf_buf);

	return rc;
}
#define dprintf libretro_dprintf

const int MAXIMUM_NUMBER_TRACKS = 100;

const int CD_FRAMES_MINUTE = 60 * 75;
const int CD_FRAMES_SECOND =      75;

const int CD_TYPE_NONE     = 1 << 0;
const int CD_TYPE_BINCUE   = 1 << 1;
const int CD_TYPE_CCD      = 1 << 2;

static int cd_pregap;

struct MSF { UINT8 M; UINT8 S; UINT8 F; };

struct cdimgTRACK_DATA { UINT8 Control; UINT8 TrackNumber; UINT8 Address[4]; UINT8 EndAddress[4]; };
struct cdimgCDROM_TOC { UINT8 FirstTrack; UINT8 LastTrack; UINT8 ImageType; TCHAR Image[MAX_PATH]; cdimgTRACK_DATA TrackData[MAXIMUM_NUMBER_TRACKS]; };

static cdimgCDROM_TOC* cdimgTOC;

static FILE*  cdimgFile = NULL;
static int    cdimgFileSize = 0;
static int    cdimgTrack = 0;
static int    cdimgLBA = 0;

bool bCDEmuOkay = false;
CDEmuStatusValue CDEmuStatus;
TCHAR CDEmuImage[MAX_PATH];

static int    cdimgSamples = 0;

// identical to the format used in clonecd .sub files, can use memcpy
struct QData { UINT8 Control; char track; char index; MSF MSFrel; char unused; MSF MSFabs; unsigned short CRC; };

static QData* QChannel = NULL;

// -----------------------------------------------------------------------------

const int cdimgOUT_SIZE = 2352;
static int  cdimgOutputbufferSize = 0;

static short* cdimgOutputbuffer = NULL;

static int cdimgOutputPosition;

void NeoCDInfo_Exit() {}

/**
 * see src/intf/cd/win32/cd_img.cpp
 */

TCHAR* GetIsoPath()
{
	if (cdimgTOC) {
		return cdimgTOC->Image;
	}

	return NULL;
}

static UINT8 bcd(const UINT8 v)
{
	return ((v >> 4) * 10) + (v & 0x0F);
}

static UINT8 tobcd(const UINT8 v)
{
	return ((v / 10) << 4) | (v % 10);
}

static const UINT8* cdimgLBAToMSF(int LBA)
{
	static UINT8 address[4];

	address[0] = 0;
	address[1] = tobcd(LBA / CD_FRAMES_MINUTE);
	address[2] = tobcd(LBA % CD_FRAMES_MINUTE / CD_FRAMES_SECOND);
	address[3] = tobcd(LBA % CD_FRAMES_SECOND);

	return address;
}

static int cdimgMSFToLBA(const UINT8* address)
{
	int LBA;

	LBA  = bcd(address[3]);
	LBA += bcd(address[2]) * CD_FRAMES_SECOND;
	LBA += bcd(address[1]) * CD_FRAMES_MINUTE;

	return LBA;
}

static const UINT8* dinkLBAToMSF(const int LBA) // not BCD version
{
	static UINT8 address[4];

	address[0] = 0;
	address[1] = LBA                    / CD_FRAMES_MINUTE;
	address[2] = LBA % CD_FRAMES_MINUTE / CD_FRAMES_SECOND;
	address[3] = LBA % CD_FRAMES_SECOND;

	return address;
}

static int dinkMSFToLBA(const UINT8* address)
{
	int LBA;

	LBA  = address[3];
	LBA += address[2] * CD_FRAMES_SECOND;
	LBA += address[1] * CD_FRAMES_MINUTE;

	return LBA;
}

// -----------------------------------------------------------------------------

static void cdimgExitStream()
{
	free(cdimgOutputbuffer);
	cdimgOutputbuffer = NULL;
}

static int cdimgInitStream()
{
	cdimgExitStream();

	cdimgOutputbuffer = (short*)malloc(cdimgOUT_SIZE * 2 * sizeof(short));
	if (cdimgOutputbuffer == NULL)
		return 1;

	return 0;
}

static int cdimgSkip(FILE* h, int samples)
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
	FILE* h = fopen(cdimgTOC->Image, _T("rb"));
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
static int cdimgParseSubFile()
{
	TCHAR  filename_sub[MAX_PATH];
	int    length = 0;
	QData* Q = 0;
	int    Qsize = 0;
	FILE*  h;
	int    track = 1;

	cdimgTOC->ImageType  = CD_TYPE_CCD;
	cdimgTOC->FirstTrack = 1;

	_tcscpy(filename_sub, CDEmuImage);
	length = _tcslen(filename_sub);

	if (length <= 4 ||
		(!IsFileExt(filename_sub, _T(".ccd")) &&
		 !IsFileExt(filename_sub, _T(".img")) &&
		 !IsFileExt(filename_sub, _T(".sub"))))
	{
		dprintf(_T("*** Bad image: %s\n"), filename_sub);
		return 1;
	}

	_tcscpy(cdimgTOC->Image, CDEmuImage);
	_tcscpy(cdimgTOC->Image + length - 4, _T(".img"));
	//bprintf(0, _T("Image file: %s\n"),cdimgTOC->Image);

	_tcscpy(filename_sub + length - 4, _T(".sub"));
	//bprintf(0, _T("filename_sub: %s\n"),filename_sub);
	h = fopen(filename_sub, _T("rb"));
	if (h == 0)
	{
		dprintf(_T("*** Bad image: %s\n"), filename_sub);
		return 1;
	}

	fseek(h, 0, SEEK_END);

	INT32 subQidx = 0;
	INT32 subQsize = ftell(h);
	UINT8 *subQdata = (UINT8*)malloc(subQsize);
	memset(subQdata, 0, subQsize);

	//bprintf(0, _T("raw .sub data size: %d\n"), subQsize);
	fseek(h, 0, SEEK_SET);
	fread(subQdata, subQsize, 1, h);
	fclose(h);

	Qsize = (subQsize + 95) / 96 * sizeof(QData);
	Q = QChannel = (QData*)malloc(Qsize);
	memset(Q, 0, Qsize);

	INT32 track_linear = 1;

	while (1)
	{
		subQidx += 12;
		if (subQidx >= subQsize) break;
		memcpy(Q, &subQdata[subQidx], 12);
		subQidx += 12;
		subQidx += 6*12;

		if (Q->index && (Q->Control & 1) && (cdimgTOC->TrackData[bcd(Q->track) - 1].TrackNumber == 0))
		{
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

	free(subQdata);

	cd_pregap = QChannel[0].MSFabs.F + QChannel[0].MSFabs.S * CD_FRAMES_SECOND + QChannel[0].MSFabs.M * CD_FRAMES_MINUTE;
	//bprintf(0, _T("pregap lba: %d MSF: %d:%d:%d\n"), cd_pregap, QChannel[0].MSFabs.M, QChannel[0].MSFabs.S, QChannel[0].MSFabs.F);

	cdimgAddLastTrack();

	return 0;
}

static int cdimgParseCueFile()
{
	TCHAR  szLine[1024];
	TCHAR  szFile[1024];
	TCHAR* s;
	TCHAR* t;
	FILE*  h;
	int    track = 1;
	int    length;

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

	h = fopen(CDEmuImage, _T("rt"));
	if (h == NULL) {
		return 1;
	}

	while (1) {
		if (fgets(szLine, sizeof(szLine), h) == NULL) {
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

			sprintf(szFile, "%s/%s", g_rom_dir, szQuote);

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

			int M, S, F;

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

			const UINT8 address[] = { 0, (UINT8)M, (UINT8)S, (UINT8)F };
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

static int cdimgExit()
{
	cdimgExitStream();

	if (cdimgFile)
		fclose(cdimgFile);
	cdimgFile = NULL;

	cdimgFileSize = 0;
	cdimgTrack = 0;
	cdimgLBA = 0;

	if (cdimgTOC)
		free(cdimgTOC);
	cdimgTOC = NULL;

	free(QChannel);
	QChannel = NULL;

	return 0;
}

static int cdimgInit()
{
	cdimgTOC = (cdimgCDROM_TOC*)malloc(sizeof(cdimgCDROM_TOC));
	if (cdimgTOC == NULL)
		return 1;

	memset(cdimgTOC, 0, sizeof(cdimgCDROM_TOC));

	cdimgTOC->ImageType = CD_TYPE_NONE;

	TCHAR* filename = ExtractFilename(CDEmuImage);

	if (_tcslen(filename) < 4)
		return 1;

	if (IsFileExt(filename, _T(".cue")))
	{
		if (cdimgParseCueFile())
		{
			dprintf(_T("*** Couldn't parse .cue file\n"));
			cdimgExit();

			return 1;
		}

	} else
	if (IsFileExt(filename, _T(".ccd")))
	{
		if (cdimgParseSubFile())
		{
			dprintf(_T("*** Couldn't parse .sub file\n"));
			cdimgExit();

			return 1;
		}

	}
	else
	{
		dprintf(_T("*** Couldn't find .img / .bin file\n"));
		cdimgExit();

		return 1;
	}

	cdimgPrintImageInfo();

	CDEmuStatus = idle;

	cdimgInitStream();

	{
		char buf[2048];
		FILE* h = fopen(cdimgTOC->Image, _T("rb"));	cdimgLBA++;

		if (h)
		{
			if (fseek(h, 16 * 2352 + 16, SEEK_SET) == 0)
			{
				if (fread(buf, 1, 2048, h) == 2048)
				{
					if (strncmp("CD001", buf + 1, 5) == 0)
					{
						buf[48] = 0;
						/* BurnDrvFindMedium(buf + 40); */
					}
					else
						dprintf(_T("*** Bad CD!\n"));
				}
			}

			fclose(h);
		}

		//CDEmuPrintCDName();
	}

	return 0;
}

static void cdimgCloseFile()
{
	if (cdimgFile)
	{
		fclose(cdimgFile);
		cdimgFile = NULL;
	}
}

static int cdimgStop()
{
	cdimgCloseFile();
	CDEmuStatus = idle;

	return 0;
}

static int cdimgFindTrack(int LBA)
{
	int trk = 0;
	for (trk = cdimgTOC->FirstTrack - 1; trk < cdimgTOC->LastTrack; trk++)
		if (LBA < cdimgMSFToLBA(cdimgTOC->TrackData[trk + 1].Address))
			break;
	return trk;
}

static int cdimgPlayLBA(int LBA)
{
	cdimgStop();

	if (QChannel != NULL) { // .CCD dump w/.SUB
		if (QChannel[LBA].Control & 0x40)
			return 1;
	} else { // .BIN/.CUE dump
		if (cdimgTOC->TrackData[cdimgFindTrack(LBA)].Control & 0x40)
			return 1;
	}

	cdimgLBA = LBA;

	cdimgTrack = cdimgFindTrack(cdimgLBA);

	if (cdimgTrack >= cdimgTOC->LastTrack)
		return 1;

	bprintf(PRINT_IMPORTANT, _T("    playing track %2i\n"), cdimgTrack + 1);

	cdimgFile = fopen(cdimgTOC->Image, _T("rb"));
	if (cdimgFile == NULL)
		return 1;

	// advance if we're not starting at the beginning of a CD
	if (cdimgLBA > cd_pregap)
		cdimgSkip(cdimgFile, (cdimgLBA - cd_pregap) * (44100 / CD_FRAMES_SECOND));

	// fill the input buffer
	if ((cdimgOutputbufferSize = fread(cdimgOutputbuffer, 4, cdimgOUT_SIZE, cdimgFile)) <= 0)
		return 1;

	cdimgOutputPosition = 0;

	cdimgSamples = 0;
	// this breaks states, commenting for now just in-case. -dink
	//cdimgLBA = cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address); // start at the beginning of track
	CDEmuStatus = playing;

	return 0;
}

static int cdimgPlay(UINT8 M, UINT8 S, UINT8 F)
{
	const UINT8 address[] = { 0, M, S, F };

	const UINT8* displayaddress = dinkLBAToMSF(cdimgMSFToLBA(address));
	dprintf(_T("    play %02i:%02i:%02i\n"), displayaddress[1], displayaddress[2], displayaddress[3]);

	return cdimgPlayLBA(cdimgMSFToLBA(address));
}

static int cdimgLoadSector(int LBA, char* pBuffer)
{
	//dprintf(_T("    read LBA %i\n"), LBA);
	if (CDEmuStatus == playing) return 0; // this might cause problems? - dink

	if (LBA != cdimgLBA)
	{
		if (cdimgFile == NULL)
		{
			cdimgStop();

			cdimgFile = fopen(cdimgTOC->Image, _T("rb"));
			if (cdimgFile == NULL)
				return 0;
		}

		//bprintf(PRINT_IMPORTANT, _T("    loading data at LBA %08u 0x%08X\n"), (LBA - cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack].Address)) * 2352, LBA * 2352);

		if (fseek(cdimgFile, (LBA - cd_pregap) * 2352, SEEK_SET))
		{
			dprintf(_T("*** couldn't seek (LBA %08u)\n"), LBA);

			//cdimgStop(); // stopping here will break ssrpg

			return 0;
		}

		CDEmuStatus = reading;
	}

	//dprintf(_T("    reading LBA %08i 0x%08X"), LBA, ftell(cdimgFile));

	cdimgLBA = cdimgMSFToLBA(cdimgTOC->TrackData[0].Address) + (ftell(cdimgFile) + 2351) / 2352 - cd_pregap;

	bool status = (fread(pBuffer, 1, 2352, cdimgFile) <= 0);

	if (status)
	{
		dprintf(_T("*** couldn't read from file\n"));

		cdimgStop();

		return 0;
	}
	// dprintf(_T("    [ %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X  %02X %02X %02X %02X ]\n"), pBuffer[0], pBuffer[1], pBuffer[2], pBuffer[3], pBuffer[4], pBuffer[5], pBuffer[6], pBuffer[7], pBuffer[8], pBuffer[9], pBuffer[10], pBuffer[11], pBuffer[12], pBuffer[13], pBuffer[14], pBuffer[15]);

	cdimgLBA++;

	return cdimgLBA;
}

static UINT8* cdimgReadTOC(int track)
{
	static UINT8 TOCEntry[4];

	memset(&TOCEntry, 0, sizeof(TOCEntry));

	if (track == CDEmuTOC_FIRSTLAST)
	{
		TOCEntry[0] = tobcd(cdimgTOC->FirstTrack - 1);
		TOCEntry[1] = tobcd(cdimgTOC->LastTrack);
		TOCEntry[2] = 0;
		TOCEntry[3] = 0;

		return TOCEntry;
	}
	if (track == CDEmuTOC_LASTMSF)
	{
		TOCEntry[0] = cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[1];
		TOCEntry[1] = cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[2];
		TOCEntry[2] = cdimgTOC->TrackData[cdimgTOC->LastTrack].Address[3];

		TOCEntry[3] = 0;

		return TOCEntry;
	}
	if (track == CDEmuTOC_FIRSTINDEX)
	{
		if (cdimgLBA < cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTOC->FirstTrack].Address))
		{
			const UINT8* addressUNBCD = dinkLBAToMSF(cdimgLBA);
			UINT8 index = ((addressUNBCD[1] * 60) + (addressUNBCD[2] + 4)) / 4;
			TOCEntry[0] = tobcd((index < 100) ? index : 99);
		}
		else
		{
			TOCEntry[0] = tobcd(1);
		}

		return TOCEntry;
	}
	if (track == CDEmuTOC_ENDOFDISC)
	{
		if (cdimgLBA >= cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTOC->LastTrack].Address))
		{
			bprintf(0, _T("END OF DISC: curr.lba %06d end lba: %06d\n"), cdimgLBA, cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTOC->LastTrack].Address));
			TOCEntry[0] = 1;
		}

		return TOCEntry;
	}

	track = bcd(track);
	if (track >= cdimgTOC->FirstTrack - 1 && track <= cdimgTOC->LastTrack)
	{
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

	switch (CDEmuStatus)
	{
		case reading:
		case playing:
		{
			if (QChannel != NULL) { // .CCD/.SUB
				QChannelData[0] = QChannel[cdimgLBA].track;

				QChannelData[1] = QChannel[cdimgLBA].MSFrel.M;
				QChannelData[2] = QChannel[cdimgLBA].MSFrel.S;
				QChannelData[3] = QChannel[cdimgLBA].MSFrel.F;

				QChannelData[4] = QChannel[cdimgLBA].MSFrel.M;
				QChannelData[5] = QChannel[cdimgLBA].MSFrel.S;
				QChannelData[6] = QChannel[cdimgLBA].MSFrel.F;

				QChannelData[7] = QChannel[cdimgLBA].Control;
			} else { // .BIN/.ISO
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

static int cdimgGetSoundBuffer(short* buffer, int samples)
{

#define CLIP(A) ((A) < -0x8000 ? -0x8000 : (A) > 0x7fff ? 0x7fff : (A))

	if (CDEmuStatus != playing) {
		memset(cdimgOutputbuffer, 0x00, cdimgOUT_SIZE * 2 * sizeof(short));
		return 0;
	}

	cdimgSamples += samples;
	while (cdimgSamples > (44100 / CD_FRAMES_SECOND))
	{
		cdimgSamples -= (44100 / CD_FRAMES_SECOND);
		cdimgLBA++;

/*		if (cdimgFile == NULL) // play next track?  bad idea. -dink
			if (cdimgLBA >= cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1].Address))
				cdimgPlayLBA(cdimgLBA); */
	}

#if 0
	extern int counter;
	if (counter) {
		const UINT8* displayaddress = dinkLBAToMSF(cdimgLBA);
		dprintf(_T("  index  %02i:%02i:%02i"), displayaddress[1], displayaddress[2], displayaddress[3]);
		INT32 endt = cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1 /* next track */].Address);
		const UINT8* displayaddressend = dinkLBAToMSF(endt);
		dprintf(_T("    end  %02i:%02i:%02i\n"), displayaddressend[1], displayaddressend[2], displayaddressend[3]);
	}
#endif

	if (cdimgFile == NULL) { // restart play if fileptr lost
		bprintf(0, _T("CDDA file pointer lost, re-starting @ %d!\n"), cdimgLBA);
		if (cdimgLBA < cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1].Address))
			cdimgPlayLBA(cdimgLBA);
	}

	if (cdimgFile == NULL) { // restart failed (really?) - time to give up.
		cdimgStop();
		return 0;
	}

	if (cdimgLBA >= cdimgMSFToLBA(cdimgTOC->TrackData[cdimgTrack + 1 /* next track */].Address)) {
		bprintf(0, _T("End of audio track %d reached!! stopping.\n"), cdimgTrack + 1);
		cdimgStop();
		return 0;
	}

	if ((cdimgOutputPosition + samples) >= cdimgOutputbufferSize)
	{
		short* src = cdimgOutputbuffer + cdimgOutputPosition * 2;
		short* dst = buffer;

		for (int i = (cdimgOutputbufferSize - cdimgOutputPosition) * 2 - 1; i > 0; )
		{
			dst[i] = CLIP((src[i]) + dst[i]); i--;
			dst[i] = CLIP((src[i]) + dst[i]); i--;
		}

		buffer += (cdimgOutputbufferSize - cdimgOutputPosition) * 2;
		samples -= (cdimgOutputbufferSize - cdimgOutputPosition);

		cdimgOutputPosition = 0;
		if ((cdimgOutputbufferSize = fread(cdimgOutputbuffer, 4, cdimgOUT_SIZE, cdimgFile)) <= 0)
			cdimgStop();
	}

	if ((cdimgOutputPosition + samples) < cdimgOutputbufferSize)
	{
		short* src = cdimgOutputbuffer + cdimgOutputPosition * 2;
		short* dst = buffer;

		for (int i = samples * 2 - 1; i > 0; )
		{
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
	}

	if (nAction & ACB_WRITE) {
		cdimgCloseFile();
	}

	return 0;
}

/**
 * see src/burner/misc.cpp
 */
TCHAR* ExtractFilename(TCHAR* fullname)
{
	TCHAR* filename = fullname + _tcslen(fullname);

	do {
		filename--;
	} while (filename >= fullname && *filename != _T('\\') && *filename != _T('/') && *filename != _T(':'));

	return filename;
}

TCHAR* LabelCheck(TCHAR* s, TCHAR* pszLabel)
{
	INT32 nLen;
	if (s == NULL) {
		return NULL;
	}
	if (pszLabel == NULL) {
		return NULL;
	}
	nLen = _tcslen(pszLabel);

	SKIP_WS(s);													// Skip whitespace

	if (_tcsncmp(s, pszLabel, nLen)){							// Doesn't match
		return NULL;
	}
	return s + nLen;
}

INT32 QuoteRead(TCHAR** ppszQuote, TCHAR** ppszEnd, TCHAR* pszSrc)	// Read a (quoted) string from szSrc and poINT32 to the end
{
	static TCHAR szQuote[QUOTE_MAX];
	TCHAR* s = pszSrc;
	TCHAR* e;

	// Skip whitespace
	SKIP_WS(s);

	e = s;

	if (*s == _T('\"')) {										// Quoted string
		s++;
		e++;
		// Find end quote
		FIND_QT(e);
		_tcsncpy(szQuote, s, e - s);
		// Zero-terminate
		szQuote[e - s] = _T('\0');
		e++;
	} else {													// Non-quoted string
		// Find whitespace
		FIND_WS(e);
		_tcsncpy(szQuote, s, e - s);
		// Zero-terminate
		szQuote[e - s] = _T('\0');
	}

	if (ppszQuote) {
		*ppszQuote = szQuote;
	}
	if (ppszEnd)	{
		*ppszEnd = e;
	}

	return 0;
}

TCHAR *FileExt(TCHAR *str)
{
	TCHAR *dot = strrchr(str, _T('.'));

	return (dot) ? StrLower(dot) : str;
}

bool IsFileExt(TCHAR *str, TCHAR *ext)
{
	return (_tcsicmp(ext, FileExt(str)) == 0);
}

TCHAR *StrReplace(TCHAR *str, TCHAR find, TCHAR replace)
{
	INT32 length = _tcslen(str);

	for (INT32 i = 0; i < length; i++) {
		if (str[i] == find) str[i] = replace;
	}

	return str;
}

// StrLower() - leaves str untouched, returns modified string
TCHAR *StrLower(TCHAR *str)
{
	static TCHAR szBuffer[256] = _T("");
	INT32 length = _tcslen(str);

	if (length > 255) length = 255;

	for (INT32 i = 0; i < length; i++) {
		if (str[i] >= _T('A') && str[i] <= _T('Z'))
			szBuffer[i] = (str[i] + _T(' '));
		else
			szBuffer[i] = str[i];
	}
	szBuffer[length] = 0;

	return &szBuffer[0];
}

/**
 * see src/intf/cd/cd_interface.cpp
 */

INT32 CDEmuInit() {
	INT32 nRet;
	CDEmuStatus = idle;
	if ((nRet = cdimgInit()) == 0) {
		bCDEmuOkay = true;
	}
	return nRet;
}
INT32 CDEmuExit() {
	if (!bCDEmuOkay) {
		return 1;
	}
	bCDEmuOkay = false;
	return cdimgExit();
}
INT32 CDEmuStop() {
	if (!bCDEmuOkay) {
		return 1;
	}
	return cdimgStop();
}
INT32 CDEmuPlay(UINT8 M, UINT8 S, UINT8 F) {
	if (!bCDEmuOkay) {
		return 1;
	}
	return cdimgPlay(M, S, F);
}
INT32 CDEmuLoadSector(INT32 LBA, char* pBuffer) {
	if (!bCDEmuOkay) {
		return 0;
	}
	return cdimgLoadSector(LBA, pBuffer);
}
UINT8* CDEmuReadTOC(INT32 track) {
	if (!bCDEmuOkay) {
		return NULL;
	}
	return cdimgReadTOC(track);
}
UINT8* CDEmuReadQChannel() {
	if (!bCDEmuOkay) {
		return NULL;
	}
	return cdimgReadQChannel();
}
INT32 CDEmuGetSoundBuffer(INT16* buffer, INT32 samples) {
	if (!bCDEmuOkay) {
		return 1;
	}
	return cdimgGetSoundBuffer(buffer, samples);
}
INT32 CDEmuScan(INT32 nAction, INT32 *pnMin)
{
	if (!bCDEmuOkay) {
		return 1;
	}
	return cdimgScan(nAction, pnMin);
}
