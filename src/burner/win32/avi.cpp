/////////////////////////////////////////////////////////////////////////////
//
// This code has been modified to work with FinalBurn Alpha.
//
//---------------------------------------------------------------------------
//
// AVI Recorder for FBA version 0.7 by Gangta
// - massive re-work by dink in fall 2015 -
//
// Limitations:
//
//  - Supported bitdepths are 15, 16, 24, and 32.
//
//  - Avi will be recorded at original game resolution w/1x, 2x, 3x pixels.
//
//  - Video effects will not be recorded.
//    (i.e. stretch, scanline, 3D effects, software effects)
//
//  - Audio compression is not supported.
//
//---------------------------------------------------------------------------
//
// Version History:
//
// 0.9 - Added 1x, 2x, 3x (selectable w/nAvi3x) pixel recording. -dink
//
// 0.8 - Automatically split the video before the 2gig mark, to prevent
//       corruption of the video. -dink
//
// 0.7 - completely re-worked the image-conversion process, using
//       burner/sshot.cpp as an example. as a result of this:
//       + source data is now supplied to the encoder at 32bpp
//       + fixes some crash issues in the previous 24bpp convertor when
//         the source data is 16bpp or the blitter being forced to 16bit mode
//       + fixes the skew problem when lines weren't a multiple of 4
//
// 0.6 - added flipped screen support, fixed a bunch of bugs
//       added to FBAlpha 2.97.37
//
// 0.5 - fixed a minor bug regarding video stream header
//       (some applications chose wrong decompressor while opening avi)
//
// 0.4 - fixed avi video/audio desync in cave games
//
// 0.3 - fixed the garbled bottom line in resulting avi
//
// 0.2 - interleaved audio
//     - fixed rotation
//     - fixed 24-bit mode
//     - cleaned up some comments, re-wrote some codes
//
// 0.1 - initial release
//
//---------------------------------------------------------------------------
//
// Credits:
//
// - FBA Team (http://fba.emuunlim.com)
//
//   for helping development and making cygwin build possible
//
// - VBA Team (http://vboy.emuhq.com)
//
//   part of this code is incorporated from VBA source
//
// - {Gambit}
//
//   for testing and reporting bugs
//
/////////////////////////////////////////////////////////////////////////////

#include "burner.h"
#define AVI_DEBUG
#ifdef _MSC_VER
 #include <vfw.h> // use the headers from MS Platform SDK
#endif
#ifdef __GNUC__
 #include "vfw.h" // use the modified idheaders included in the src
#endif

// defines
#define FBAVI_VFW_INIT 0x0001 // avi file library initialized
#define FBAVI_VID_SET  0x0002 // avi video compression set
#define FBAVI_AUD_SET  0x0004 // avi audio compression set (not used for now)

#define TAVI_DIRECTORY ".\\avi\\"

INT32 nAviStatus = 0;       // 1 (recording started), 0 (recording stopped)
INT32 nAviIntAudio = 1;     // 1 (interleave audio aka audio enabled), 0 (do not interleave aka disable audio)
INT32 nAvi3x = 0;           // set 1 - 3 for 1x - 3x pixel expansion

INT32 nAviSplit; // number of the split (@2gig) video
COMPVARS compvarsave;        // compression options, for split
char szAviFileName[MAX_PATH];

static INT32 nAviFlags = 0;

static struct FBAVI {
	PAVIFILE pFile; // avi file
	// formats
	BITMAPINFOHEADER bih; // video format
	WAVEFORMATEX wfx;     // audio format
	// headers
	AVISTREAMINFO vidh; // header for a single video stream
	AVISTREAMINFO audh; // header for a single audio stream
	// streams
	PAVISTREAM psVid;         // uncompressed video stream
	PAVISTREAM psVidCompressed; // compressed video stream
	PAVISTREAM psAud;         // uncompressed audio stream
	PAVISTREAM psAudCompressed; // compressed audio stream (not supported yet)
	// compression options
	COMPVARS compvar;        // compression options
	AVICOMPRESSOPTIONS opts; // compression options
	// other 
	UINT32 nFrameNum; // frame number for each compressed frame
	LONG nAviSize;    // current bytes written, used to break up the avi @ the 2gig mark
	INT32 nWidth;
	INT32 nHeight;
	UINT8 nLastDest; // number of the last pBitmapBuf# written to - for chaining effects
	UINT8 *pBitmap; // pointer for buffer for bitmap
	UINT8 *pBitmapBuf1; // buffer #1
	UINT8 *pBitmapBuf2; // buffer #2 (flippy, pBitmapBufX points to one of these depending on last effect used)
} FBAvi;

// Opens an avi file for writing.
// Returns: 0 (successful), 1 (failed)
static INT32 AviCreateFile()
{
	HRESULT hRet;

	char szFilePath[MAX_PATH];
    time_t currentTime;
    tm* tmTime;

	// Get the time
	time(&currentTime);
    tmTime = localtime(&currentTime);

	// construct our filename -> "romname-mm-dd-hms.avi"

	if (nAviSplit == 0) { // Create the filename @ the first file in our set
		sprintf(szAviFileName, "%s%s-%.2d-%.2d-%.2d%.2d%.2d", TAVI_DIRECTORY, BurnDrvGetTextA(DRV_NAME), tmTime->tm_mon + 1, tmTime->tm_mday, tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec);
	}

	// add the set# and extension to our file.
	sprintf(szFilePath, "%s_%X.avi", szAviFileName, nAviSplit);

	hRet = AVIFileOpenA(
				&FBAvi.pFile,           // returned file pointer
				szFilePath,             // file path
				OF_CREATE | OF_WRITE,   // mode
				NULL);                  // use handler determined from file extension
	if (hRet) {
#ifdef AVI_DEBUG
		bprintf(0, _T("    AVI Error: AVIFileOpen() failed.\n"));
		switch (hRet) {
			case AVIERR_BADFORMAT:
				bprintf(0, _T("\t           Cannot read file (corrupt file or unrecognized format).\n"));
				break;
			case AVIERR_MEMORY:
				bprintf(0, _T("\t           Cannot open file (insufficient memory).\n"));
				break;
			case AVIERR_FILEREAD:
				bprintf(0, _T("\t           Cannot read file (disk error).\n"));
				break;
			case AVIERR_FILEOPEN:
				bprintf(0, _T("\t           Cannot open file (disk error).\n"));
				break;
			case REGDB_E_CLASSNOTREG:
				bprintf(0, _T("\t           Windows registry does not have a handler for this file type (.avi).\n"));
				break;
			default:
				bprintf(0, _T("\t           Unknown error).\n"));
				break;
		}
#endif
		return 1;
	}

	return 0;
}

// Make a 32bit Bitmap of the gamescreen and flip it depending on orientation.
static INT MakeSSBitmap()
{
	INT32 w = FBAvi.nWidth, h = FBAvi.nHeight;
	UINT8 *pTemp = FBAvi.pBitmapBuf1;
	UINT8* pSShot = NULL;

	if (pVidImage == NULL) {
		return 1; // video buffer is empty
	}

	pSShot = pVidImage;

	FBAvi.nLastDest = 1;

	// Convert the image to 32-bit
	if (nVidImageBPP < 4) {
		if (nVidImageBPP == 2) {
			for (INT32 i = 0; i < h * w; i++) {
				UINT16 nColour = ((UINT16*)pSShot)[i];

				// Red
		        *(pTemp + i * 4 + 0) = (UINT8)((nColour & 0x1F) << 3);
			    *(pTemp + i * 4 + 0) |= *(pTemp + 4 * i + 0) >> 5;

				if (nVidImageDepth == 15) {
					// Green
					*(pTemp + i * 4 + 1) = (UINT8)(((nColour >> 5) & 0x1F) << 3);
					*(pTemp + i * 4 + 1) |= *(pTemp + i * 4 + 1) >> 5;
					// Blue
					*(pTemp + i * 4 + 2) = (UINT8)(((nColour >> 10)& 0x1F) << 3);
					*(pTemp + i * 4 + 2) |= *(pTemp + i * 4 + 2) >> 5;
				}

				if (nVidImageDepth == 16) {
					// Green
					*(pTemp + i * 4 + 1) = (UINT8)(((nColour >> 5) & 0x3F) << 2);
					*(pTemp + i * 4 + 1) |= *(pTemp + i * 4 + 1) >> 6;
					// Blue
					*(pTemp + i * 4 + 2) = (UINT8)(((nColour >> 11) & 0x1F) << 3);
					*(pTemp + i * 4 + 2) |= *(pTemp + i * 4 + 2) >> 5;
				}
			}
        } else { // 24-bit
			memset(pTemp, 0, w * h * sizeof(INT32));
			for (INT32 i = 0; i < h * w; i++) {
		        *(pTemp + i * 4 + 0) = *(pSShot + i * 3 + 0);
		        *(pTemp + i * 4 + 1) = *(pSShot + i * 3 + 1);
		        *(pTemp + i * 4 + 2) = *(pSShot + i * 3 + 2);
			}
        }

		pSShot = pTemp;
		FBAvi.pBitmap = FBAvi.pBitmapBuf1;
	} else if (nVidImageBPP == 4) {
		// source is 32bit already, just copy the buffer
		memmove(pTemp, pVidImage, h * w * 4);
		FBAvi.pBitmap = FBAvi.pBitmapBuf1;
		pSShot = pTemp;
	} else {
		return 1; // unsupported BPP (unlikely)
	}

	// Rotate and flip the image
	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		pTemp = FBAvi.pBitmapBuf2;

		for (INT32 x = 0; x < h; x++) {
			if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
				for (INT32 y = 0; y < w; y++) {
					((UINT32*)pTemp)[(w - y - 1) + x * w] = ((UINT32*)pSShot)[x + y * h];
				}
			} else {
				for (INT32 y = 0; y < w; y++) {
					((UINT32*)pTemp)[y + (h - x - 1) * w] = ((UINT32*)pSShot)[x + y * h];
				}
			}
		}

		FBAvi.pBitmap = FBAvi.pBitmapBuf2;
		FBAvi.nLastDest = 2;
        pSShot = pTemp;
	}
	else if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) { // fixed rotation by regret
		pTemp = FBAvi.pBitmapBuf2;

		for (INT32 y = h - 1; y >= 0; y--) {
			for (INT32 x = w - 1; x >= 0; x--) {
				((UINT32*)pTemp)[(w - x - 1) + (h - y - 1) * w] = ((UINT32*)pSShot)[x + y * w];
			}
		}

		FBAvi.pBitmap = FBAvi.pBitmapBuf2;
		FBAvi.nLastDest = 2;
        pSShot = pTemp;
	}

	return 0; // success!
}

#define aviBPP 4 // BYTES per pixel

// Flips the image the way the encoder expects it to be
static INT32 MakeBitmapFlippedForEncoder()
{
	INT32 nWidth = FBAvi.nWidth;
	INT32 nHeight = FBAvi.nHeight;
	UINT8 *pDest = (FBAvi.nLastDest == 2) ? FBAvi.pBitmapBuf1 : FBAvi.pBitmapBuf2;
	UINT8 *pSrc = (FBAvi.nLastDest == 2) ? FBAvi.pBitmapBuf2 : FBAvi.pBitmapBuf1;
	FBAvi.pBitmap = pDest;

	UINT8 *pxl = pSrc + (nWidth*(nHeight-1)<<2);

	for (INT32 h = nHeight-1; h >- 1; h--) {
		for (INT32 w = nWidth-1; w>-1; w--) {
			memcpy(pDest, pxl, 4);  // just copy 24 bits straight into bitmap
			pDest += 4;             // next pixel (dest)
			pxl += 4;               // next pixel (src)
		}
		pxl -= nWidth<<3;           // go to next line up
	}

	FBAvi.nLastDest = (FBAvi.pBitmap == FBAvi.pBitmapBuf1) ? 1 : 2;

	return 0;
}

// Doubles the pixels on an already converted buffer
static INT32 MakeBitmap2x()
{
	INT32 nWidth = FBAvi.nWidth;
	INT32 nHeight = FBAvi.nHeight;
	UINT8 *pSrc = (FBAvi.nLastDest == 2) ? FBAvi.pBitmapBuf2 : FBAvi.pBitmapBuf1;
	UINT8 *pDest = (FBAvi.nLastDest == 2) ? FBAvi.pBitmapBuf1 : FBAvi.pBitmapBuf2;
	UINT8 *pDestL2;
	FBAvi.pBitmap = pDest;
	INT32 lctr = 0;

	for (INT32 i = 0; i < nHeight * nWidth; i++) {
		pDestL2 = pDest + (nWidth * (aviBPP * 2)); // next line down
		memcpy(pDest, pSrc, aviBPP);
		pDest += aviBPP;
		memcpy(pDest, pSrc, aviBPP);
		pDest += aviBPP;
		memcpy(pDestL2, pSrc, aviBPP);
		pDestL2 += aviBPP;
		memcpy(pDestL2, pSrc, aviBPP);
		lctr++;
		if(lctr >= nWidth) {
			lctr = 0;
			pDest += nWidth*(aviBPP * 2);
		}
		pSrc += aviBPP;             // source a new pixel
	}

	FBAvi.nLastDest = (FBAvi.pBitmap == FBAvi.pBitmapBuf1) ? 1 : 2;

	return 0;
}

// Doubles the pixels on an already converted buffer
static INT32 MakeBitmap3x()
{
	INT32 nWidth = FBAvi.nWidth;
	INT32 nHeight = FBAvi.nHeight;
	UINT8 *pSrc = (FBAvi.nLastDest == 2) ? FBAvi.pBitmapBuf2 : FBAvi.pBitmapBuf1;
	UINT8 *pDest = (FBAvi.nLastDest == 2) ? FBAvi.pBitmapBuf1 : FBAvi.pBitmapBuf2;
	UINT8 *pDestL2, *pDestL3;
	FBAvi.pBitmap = pDest;
	INT32 lctr = 0;

	for (INT32 i = 0; i < nHeight * nWidth; i++) {
		pDestL2 = pDest + (nWidth * (aviBPP * 3)); // next line down, aviBPP * x = # pixels expanding
		pDestL3 = pDest + ((nWidth * (aviBPP * 3)) * 2); // next line down + 1
		memcpy(pDest, pSrc, aviBPP);
		pDest += aviBPP;
		memcpy(pDest, pSrc, aviBPP);
		pDest += aviBPP;
		memcpy(pDest, pSrc, aviBPP);
		pDest += aviBPP;
		memcpy(pDestL2, pSrc, aviBPP);
		pDestL2 += aviBPP;
		memcpy(pDestL2, pSrc, aviBPP);
		pDestL2 += aviBPP;
		memcpy(pDestL2, pSrc, aviBPP);

		memcpy(pDestL3, pSrc, aviBPP);
		pDestL3 += aviBPP;
		memcpy(pDestL3, pSrc, aviBPP);
		pDestL3 += aviBPP;
		memcpy(pDestL3, pSrc, aviBPP);
		lctr++;
		if(lctr >= nWidth) { // end of line 1
			lctr = 0;
			pDest += nWidth*(aviBPP * 3); // line 2
			pDest += nWidth*(aviBPP * 3); // line 3
		}
		pSrc += aviBPP;             // source a new pixel
	}

	FBAvi.nLastDest = (FBAvi.pBitmap == FBAvi.pBitmapBuf1) ? 1 : 2;

	return 0;
}

// Sets the format for video stream.
static void AviSetVidFormat()
{
	// set the format of bitmap
	memset(&FBAvi.bih,0,sizeof(BITMAPINFOHEADER));
	FBAvi.bih.biSize = sizeof(BITMAPINFOHEADER);

	BurnDrvGetVisibleSize(&FBAvi.nWidth, &FBAvi.nHeight);
	FBAvi.bih.biWidth = FBAvi.nWidth * nAvi3x;
	FBAvi.bih.biHeight = FBAvi.nHeight * nAvi3x;

	FBAvi.pBitmap = FBAvi.pBitmapBuf1;

	FBAvi.bih.biPlanes = 1;
	FBAvi.bih.biBitCount = 32;
	FBAvi.bih.biCompression = BI_RGB;           // uncompressed RGB source
	FBAvi.bih.biSizeImage = aviBPP * FBAvi.bih.biWidth * FBAvi.bih.biHeight;
}

// Sets the format for the audio stream.
static void AviSetAudFormat()
{
	memset(&FBAvi.wfx, 0, sizeof(WAVEFORMATEX));
	FBAvi.wfx.cbSize = sizeof(WAVEFORMATEX);
	FBAvi.wfx.wFormatTag = WAVE_FORMAT_PCM;
	FBAvi.wfx.nChannels = 2;                    // stereo
	FBAvi.wfx.nSamplesPerSec = nBurnSoundRate;  // sample rate
	FBAvi.wfx.wBitsPerSample = 16;              // 16-bit
	FBAvi.wfx.nBlockAlign = 4;                  // bytes per sample
	FBAvi.wfx.nAvgBytesPerSec = FBAvi.wfx.nSamplesPerSec * FBAvi.wfx.nBlockAlign;
}

// Creates the video stream.
// Returns: 0 (successful), 1 (failed)
static INT32 AviCreateVidStream()
{
	INT32 nRet;
	HRESULT hRet;

	/*
	Problem:
	Must set the compressor option prior to creating the video
	stream in the file because fccHandler is unknown until the
	compressor is chosen.  However, AVISaveOptions() requires
	the stream to be created prior to the function call.
	
	Solution:
	Use ICCompressorChoose() to set the	COMPVARS, create the
	video stream in the file using the returned fccHandler,
	manually convert COMPVARS to AVICOMPRESSOPTIONS, and
	then, make the compressed stream.
	*/

	// initialize COMPVARS
	memset(&FBAvi.compvar, 0, sizeof(COMPVARS));
	FBAvi.compvar.cbSize = sizeof(COMPVARS); 
	FBAvi.compvar.dwFlags = ICMF_COMPVARS_VALID;
	//FBAvi.compvar.fccHandler = comptypeDIB; // default compressor = uncompressed full frames
	FBAvi.compvar.fccHandler = 0; // default compressor = uncompressed full frames
	FBAvi.compvar.lQ = ICQUALITY_DEFAULT;

	if (nAviSplit>0) { // next 2gig chunk, don't ask user for video type.
		memmove(&FBAvi.compvar, &compvarsave, sizeof(COMPVARS));
	} else {// let user choose the compressor and compression options
		nRet = ICCompressorChoose(
								  hScrnWnd,
								  ICMF_CHOOSE_DATARATE | ICMF_CHOOSE_KEYFRAME,
								  (LPVOID) &FBAvi.bih, // uncompressed data input format
								  NULL,                // no preview window
								  &FBAvi.compvar,      // returned info
								  "Set video compression option");
		if (!nRet) {
#ifdef AVI_DEBUG
			bprintf(0, _T("    AVI Error: ICCompressorChoose() failed.\n"));
			ICCompressorFree(&FBAvi.compvar);
#endif
			return 1;
		}
		memmove(&compvarsave, &FBAvi.compvar, sizeof(COMPVARS));
	}

	nAviFlags |= FBAVI_VID_SET; // flag |= video compression option is set

	// fill in the header for the video stream
	memset(&FBAvi.vidh, 0, sizeof(FBAvi.vidh));
	FBAvi.vidh.fccType                = streamtypeVIDEO; // stream type
	FBAvi.vidh.fccHandler             = FBAvi.compvar.fccHandler;
	FBAvi.vidh.dwScale                = 100;      // scale
	FBAvi.vidh.dwRate                 = nBurnFPS; // rate, (fps = dwRate/dwScale)
	FBAvi.vidh.dwSuggestedBufferSize  = FBAvi.bih.biSizeImage;

	// set rectangle for stream
	nRet = SetRect(
		&FBAvi.vidh.rcFrame,
		0, // x-coordinate of the rectangle's upper left corner
		0, // y-coordinate of the rectangle's upper left corner
		(INT32) FBAvi.bih.biWidth,
		(INT32) FBAvi.bih.biHeight);
	if (nRet == 0) {
#ifdef AVI_DEBUG
		bprintf(0, _T("    AVI Error: SetRect() failed.\n"));
#endif
		return 1;
	}

	// create the video stream in the avi file
	hRet = AVIFileCreateStream(
		FBAvi.pFile,  // file pointer
		&FBAvi.psVid, // returned stream pointer
		&FBAvi.vidh); // stream header
	if (hRet) {
#ifdef AVI_DEBUG
		bprintf(0, _T("    AVI Error: AVIFileCreateStream() failed.\n"));
#endif
		return 1;
	}

	// convert COMPVARS to AVICOMPRESSOPTIONS
	memset(&FBAvi.opts, 0, sizeof(FBAvi.opts));
	FBAvi.opts.fccType = streamtypeVIDEO;
	FBAvi.opts.fccHandler = FBAvi.compvar.fccHandler;
	FBAvi.opts.dwKeyFrameEvery = FBAvi.compvar.lKey;
	FBAvi.opts.dwQuality = FBAvi.compvar.lQ;
	FBAvi.opts.dwBytesPerSecond = FBAvi.compvar.lDataRate * 1024L;
	FBAvi.opts.dwFlags = AVICOMPRESSF_VALID | 
		(FBAvi.compvar.lDataRate ? AVICOMPRESSF_DATARATE:0L) |
		(FBAvi.compvar.lKey ? AVICOMPRESSF_KEYFRAMES:0L);
	FBAvi.opts.lpFormat = &FBAvi.bih;
	FBAvi.opts.cbFormat = FBAvi.bih.biSize + FBAvi.bih.biClrUsed * sizeof(RGBQUAD);
	FBAvi.opts.lpParms = FBAvi.compvar.lpState;
	FBAvi.opts.cbParms = FBAvi.compvar.cbState; 
	FBAvi.opts.dwInterleaveEvery = 0; 

	// make the compressed video stream
	hRet = AVIMakeCompressedStream(
		&FBAvi.psVidCompressed,
		FBAvi.psVid,
		&FBAvi.opts,
		NULL);
	if (hRet != AVIERR_OK) {
#ifdef AVI_DEBUG
		bprintf(0, _T("    AVI Error: AVIMakeCompressedStream() failed.\n"));
		switch (hRet) {
			case AVIERR_NOCOMPRESSOR:
				bprintf(0, _T("\t           A suitable compressor cannot be found.\n"));
				break;
			case AVIERR_MEMORY:
				bprintf(0, _T("\t           Not enough memory to complete the operation.\n"));
				break;
			case AVIERR_UNSUPPORTED:
				bprintf(0, _T("\t           Compression is not supported for this type of data.\n"));
				break;
			default:
				bprintf(0, _T("\t           Unknown error.\n"));
				break;
		}
#endif
		return 1;
	}

	// set the stream format for compressed video
	hRet = AVIStreamSetFormat(
		FBAvi.psVidCompressed,  // pointer to the opened video output stream
		0,                      // starting position
		&FBAvi.bih,             // stream format
		FBAvi.bih.biSize + FBAvi.bih.biClrUsed * sizeof(RGBQUAD)); // format size
	if (hRet) {
#ifdef AVI_DEBUG
		bprintf(0, _T("    AVI Error: AVIStreamSetFormat() failed.\n"));
#endif
		return 1;
	}

	return 0;
}

// Creates the audio stream.
// Returns: 0 (successful), 1 (failed)
static INT32 AviCreateAudStream()
{
	HRESULT hRet;

	// fill in the header for the audio stream
	//
	// - dwInitialFrames specifies how much to skew the
	//   audio data ahead of the video frames in interleaved
	//   files (typically 0.75 sec).
	//  
	memset(&FBAvi.audh, 0, sizeof(FBAvi.audh));
	FBAvi.audh.fccType                = streamtypeAUDIO;    // stream type
	FBAvi.audh.dwScale                = FBAvi.wfx.nBlockAlign;
	FBAvi.audh.dwRate                 = FBAvi.wfx.nAvgBytesPerSec;
	FBAvi.audh.dwInitialFrames        = 1;                  // audio skew
	FBAvi.audh.dwSuggestedBufferSize  = nBurnSoundLen<<2;
	FBAvi.audh.dwSampleSize           = FBAvi.wfx.nBlockAlign;

	// create the audio stream
	hRet = AVIFileCreateStream(
		FBAvi.pFile,  // file pointer
		&FBAvi.psAud, // returned stream pointer
		&FBAvi.audh); // stream header
	if (hRet) {
#ifdef AVI_DEBUG
		bprintf(0, _T("    AVI Error: AVIFileCreateStream() failed.\n"));
#endif
		return 1;
	}

	// set the format for audio stream
	hRet = AVIStreamSetFormat(
		FBAvi.psAud,    // pointer to the opened audio output stream
		0,              // starting position
		&FBAvi.wfx,     // stream format
		sizeof(WAVEFORMATEX)); // format size
	if (hRet) {
#ifdef AVI_DEBUG
		bprintf(0, _T("    AVI Error: AVIStreamSetFormat() failed.\n"));
#endif
		return 1;
	}
				
	return 0;
}

INT32 AviStart_INT();
void AviStop_INT();

// Records 1 frame worth of data to the output stream
// Returns: 0 (successful), 1 (failed)
INT32 AviRecordFrame(INT32 bDraw)
{
	HRESULT hRet;
	LONG wrote;
	/*
	When FBA is frame skipping (bDraw == 0), we don't
	need to	build a new bitmap, nor encode video frame.
	Just increment the frame number.  However, if avi
	is in vid/aud interleaved mode, audio must be recorded
	every frame regardless of frameskip.
	*/
	if(bDraw) {
		if(MakeSSBitmap()) {
#ifdef AVI_DEBUG
			bprintf(0, _T("    AVI Error: MakeBitmap() failed.\n"));
#endif
			return 1;
		}

		MakeBitmapFlippedForEncoder();  // Mandatory, encoder needs image data to be flipped.

		switch (nAvi3x) {
			case 2: MakeBitmap2x(); break; // Double the pixels, this must always happen otherwise the compression size and video quality - especially when uploaded to yt - will suck. -dink
			case 3: MakeBitmap3x(); break; // Triple the pixels, for 720p/60hz* yt videos (yay!) *) if * 3 >= 720 && hz >= 60
		}

		wrote = 0;

		// compress the bitmap and write to AVI output stream
		hRet = AVIStreamWrite(
			FBAvi.psVidCompressed,      // stream pointer
			FBAvi.nFrameNum,            // time of this frame
			1,                          // number to write
			(LPBYTE) FBAvi.pBitmap,     // pointer to data
			FBAvi.bih.biSizeImage,      // size of this frame
			AVIIF_KEYFRAME,             // flags
			NULL,
			&wrote);
		if (hRet != AVIERR_OK) {
#ifdef AVI_DEBUG
			bprintf(0, _T("    AVI Error: AVIStreamWrite() failed.\n"));
#endif
			return 1;
		}

		FBAvi.nAviSize += wrote;
	}

	// interleave audio
	if (nAviIntAudio) {
		wrote = 0;

		// write the PCM audio data to AVI output stream
		hRet = AVIStreamWrite(
			FBAvi.psAud,                // stream pointer
			FBAvi.nFrameNum,            // time of this frame
			1,                          // number to write
			(LPBYTE) nAudNextSound,     // pointer to data
			nBurnSoundLen<<2,           // size of data
			AVIIF_KEYFRAME,             // flags
			NULL,
			&wrote);
		if (hRet != AVIERR_OK) {
#ifdef AVI_DEBUG
			bprintf(0, _T("    AVI Error: AVIStreamWrite() failed.\n"));
#endif
			return 1;
		}
		FBAvi.nAviSize += wrote;
	}

	FBAvi.nFrameNum++;

	if (FBAvi.nAviSize >= 0x79000000) {
		nAviSplit++;
		bprintf(0, _T("    AVI Writer Split-Point 0x%X reached, creating new file.\n"), nAviSplit);
		AviStop_INT();
		AviStart_INT();
	}

	return 0;
}

// Frees bitmap.
static void FreeBMP()
{
	if (FBAvi.pBitmapBuf1) {
		free(FBAvi.pBitmapBuf1);
		FBAvi.pBitmapBuf1 = NULL;
	}
	if (FBAvi.pBitmapBuf2) {
		free(FBAvi.pBitmapBuf2);
		FBAvi.pBitmapBuf2 = NULL;
	}
	FBAvi.pBitmap = NULL;
}

// Stops AVI recording.
void AviStop_INT()
{
	if (nAviFlags & FBAVI_VFW_INIT) {
		if (FBAvi.psAud) {
			AVIStreamRelease(FBAvi.psAud);
		}

		if (FBAvi.psVidCompressed) {
			AVIStreamRelease(FBAvi.psVidCompressed);
		}

		if (FBAvi.psVid) {
			AVIStreamRelease(FBAvi.psVid);
		}

		if (FBAvi.pFile) {
			AVIFileRelease(FBAvi.pFile);
		}

		if (nAviFlags & FBAVI_VID_SET) {
			if (nAviSplit == -1)
				ICCompressorFree(&FBAvi.compvar);
		}

		AVIFileExit();
		Sleep(150); // Allow compressor-codec threads/windows/etc time to finish gracefully
		FreeBMP();

#ifdef AVI_DEBUG
		if (nAviStatus) {
			bprintf(0, _T(" ** AVI recording finished.\n"));
			bprintf(0, _T("    total frames recorded = %u\n"), FBAvi.nFrameNum+1);
		}
#endif
		nAviStatus = 0;
		nAviFlags = 0;

		memset(&FBAvi, 0, sizeof(FBAVI));
		MenuEnableItems();
	}
}

void AviStop()
{
	nAviSplit = -1;
	AviStop_INT();
}


// Starts AVI recording.
// Returns: 0 (successful), 1 (failed)
INT32 AviStart_INT()
{
	// initialize local variables
	memset (&FBAvi, 0, sizeof(FBAVI));

	// initialize avi file library
	// - we need VFW version 1.1 or greater
	if (HIWORD(VideoForWindowsVersion()) < 0x010a){
		// VFW verison is too old, disable AVI recording
		return 1;
	}

	if (nAvi3x < 1 || nAvi3x > 3) {
		nAvi3x = 2; // bad value, default to 2x pixels
	}

	AVIFileInit();

	nAviFlags |= FBAVI_VFW_INIT; // avi file library initialized

	// create the avi file
	if(AviCreateFile()) {
		return 1;
	}

	// set video format
	AviSetVidFormat();

	// allocate memory for 2x 32bpp bitmap buffers
	FBAvi.pBitmapBuf1 = (UINT8 *)malloc(FBAvi.bih.biSizeImage);
	if (FBAvi.pBitmapBuf1 == NULL) {
		return 1; // not enough memory to create allocate bitmap
	}
	FBAvi.pBitmapBuf2 = (UINT8 *)malloc(FBAvi.bih.biSizeImage);
	if (FBAvi.pBitmapBuf2 == NULL) {
		free(FBAvi.pBitmapBuf1);
		return 1; // not enough memory to create allocate bitmap
	}
	FBAvi.pBitmap = FBAvi.pBitmapBuf1;

	// create the video stream
	if(AviCreateVidStream()) {
		return 1;
	}

	// interleave audio / add audio to the stream
	if (nAviIntAudio) {
		// set audio format
		AviSetAudFormat();
		// create the audio stream
		if(AviCreateAudStream()) {
			return 1;
		}
	}

	// record the first frame (at init only, not on splits init)
	if(nAviSplit == 0 && AviRecordFrame(1)) {
		return 1;
	}

	nAviStatus = 1; // recording started
	MenuEnableItems();
	return 0;
}

INT32 AviStart()
{
	nAviSplit = 0;

	return AviStart_INT();
}

