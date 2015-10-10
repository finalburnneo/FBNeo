/**************************************************************************
 *
 *  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
 *  KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
 *  PURPOSE.
 *
 *  Copyright (C) 1992 - 1997 Microsoft Corporation.  All Rights Reserved.
 *
 **************************************************************************/
/****************************************************************************
 *
 *  WRITEAVI.C
 *
 *  Creates the file OUTPUT.AVI, an AVI file consisting of a rotating clock
 *  face.  This program demonstrates using the functions in AVIFILE.DLL
 *  to make writing AVI files simple.
 *
 *  This is a stripped-down example; a real application would have a user
 *  interface and check for errors.
 *
 ***************************************************************************/
/////////////////////////////////////////////////////////////////////////////
//
// This code has been modified to work with FinalBurn Alpha.
//
//---------------------------------------------------------------------------
//
// AVI Recorder for FBA version 0.5 by Gangta
//
// Limitations:
//
//  - Supported bitdepths are 15, 16, 24, and 32.
//
//  - Avi will be recorded at original game resolution.
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

//int BurnDrvGetScreen(int *pnWidth, int *pnHeight);
unsigned char *pAviBuffer = NULL; // pointer to raw pixel data
int nAviStatus = 0; // 1 (recording started), 0 (recording stopped)
int nAviIntAudio = 1; // 1 (interleave audio), 0 (do not interleave)
//int nBurnBitDepth=16;

static int nAviFlags = 0;

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
	UINT8 flippedmode;
	UINT8 *pBitmap; // pointer for buffer for bitmap
	UINT8 *pBitmapBuf1; // buffer #1
	UINT8 *pBitmapBuf2; // buffer #2 (flippy)
	int (*MakeBitmap) (); // MakeBitmapNoRotate, MakeBitmapRotateCW, MakeBitmapRotateCCW
} FBAvi;

// Opens an avi file for writing.
// Returns: 0 (successful), 1 (failed)
static int AviCreateFile()
{
	HRESULT hRet;

	char szFilePath[MAX_PATH];
    time_t currentTime;
    tm* tmTime;

	// Get the time
	time(&currentTime);
    tmTime = localtime(&currentTime);
	// construct our filename -> "romname-mm-dd-hms.avi"
    sprintf(szFilePath, "%s%s-%.2d-%.2d-%.2d%.2d%.2d.avi", TAVI_DIRECTORY, BurnDrvGetTextA(DRV_NAME), tmTime->tm_mon + 1, tmTime->tm_mday, tmTime->tm_hour, tmTime->tm_min, tmTime->tm_sec);

	// temporary fix for AVIFileOpen()
	//
	// AVIFileOpen() doesn't truncate file size to zero on
	// existing files with OR_CREATE | OF_WRITE flags ?????
	{
		FILE *pFile = NULL;
		pFile = fopen(szFilePath, "wb");
		if (pFile) {
			fclose(pFile);
		}
		pFile = NULL;
	}

	hRet = AVIFileOpenA(
				&FBAvi.pFile, // returned file pointer
				szFilePath, // file path
				OF_CREATE | OF_WRITE /*| OF_SHARE_EXCLUSIVE*/, // mode
				NULL); // use handler determined from file extension
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

// Converts video buffer to bitmap, no rotation
// Returns: 0 (successful), 1 (failed)
static int MakeBitmapNoRotate()
{
	int w,h;
	int nWidth = FBAvi.bih.biWidth;
	int nHeight = FBAvi.bih.biHeight;
	unsigned char *pTemp = FBAvi.pBitmapBuf1; // walks through and fills the bitmap


	if (pAviBuffer == NULL) {
		return 1; // video buffer is empty
	}

	switch (nVidImageDepth) {
		case 15: { // top to bottom 15-bit RGB --> bottom to top 24-bit RGB
			/*
			initial =  rrrrr ggggg bbbbb x (5 5 5 x)
			(last 1 bit not used)

			r r r r r         g g g g g        b b b b b x
			|\|\|\| |         |\|\|\| |        |\|\|\| |
			| |\|\|\|         | |\|\|\|        | |\|\|\|
			| | |\|\|\        | | |\|\|\       | | |\|\|\
			| | | |\|\ \      | | | |\|\ \     | | | |\|\ \
			| | | | |\ \ \    | | | | |\ \ \   | | | | |\ \ \
			| | | | | | | |   | | | | | | | |  | | | | | | | |
			R R R R R R R R   G G G G G G G G  B B B B B B B B

			final = RRRRRRRR GGGGGGGG BBBBBBBB (8 8 8)
			*/

			// start at the bottom line
			short *p16 = (short *)pAviBuffer + nWidth*(nHeight-1);

			for (h=nHeight-1;h>-1;h--) {
				for (w=nWidth-1;w>-1;w--) {
					short n16 = *p16++; // go to next pixel
					*pTemp = (unsigned char)((n16&0x1f)<<3);  // R
					*pTemp |= (*pTemp++)>>5;                  // R
					*pTemp = (unsigned char)((n16>>2)&0xf8);  // G
					*pTemp |= (*pTemp++)>>5;                  // G
					*pTemp = (unsigned char)((n16>>7)&0xf8);  // B
					*pTemp |= (*pTemp++)>>5;                  // B
				}
				p16 -= nWidth<<1; // go to next line up
			}
			break;
		}
		case 16: { // top to bottom 16-bit RGB --> bottom to top 24-bit RGB (tested)
			/*
			initial = rrrrr gggggg bbbbb (5 6 5)

			r r r r r         g g g g g g      b b b b b
			|\|\|\| |         |\|\| | | |      |\|\|\| |
			| |\|\|\|         | |\|\| | |      | |\|\|\|
			| | |\|\|\        | | |\|\| |      | | |\|\|\
			| | | |\|\ \      | | | |\|\|      | | | |\|\ \
			| | | | |\ \ \    | | | | |\|\     | | | | |\ \ \
			| | | | | | | |   | | | | | |\ \   | | | | | | | |
			| | | | | | | |   | | | | | | | |  | | | | | | | |
			R R R R R R R R   G G G G G G G G  B B B B B B B B

			final = RRRRRRRR GGGGGGGG BBBBBBBB (8 8 8)
			*/

			// start at the bottom line
			short *p16 = (short *)pAviBuffer + nWidth*(nHeight-1);

			for (h=nHeight-1;h>-1;h--) {
				for (w=nWidth-1;w>-1;w--) {
					short n16 = *p16++; // go to next pixel
					*pTemp = (unsigned char)((n16&0x1f)<<3);  // R
					*pTemp |= (*pTemp++)>>5;                  // R
					*pTemp = (unsigned char)((n16>>3)&0xfc);  // G
					*pTemp |= (*pTemp++)>>6;                  // G
					*pTemp = (unsigned char)((n16>>8)&0xf8);  // B
					*pTemp |= (*pTemp++)>>5;                  // B
				}
				p16 -= nWidth<<1; // go to next line up
			}
			break;
		}
		case 24: { // top to bottom 24-bit RGB --> bottom to top 24-bit RGB (not tested)
			/*
			initial = rrrrrrrr gggggggg bbbbbbbb (8 8 8)

			rrrrrrrr gggggggg bbbbbbbb
			|||||||| |||||||| ||||||||
			RRRRRRRR GGGGGGGG BBBBBBBB

			final = RRRRRRRR GGGGGGGG BBBBBBBB (8 8 8)
			*/

			// start at the bottom line
			unsigned char *p8 = pAviBuffer + 3*nWidth*(nHeight-1);

			for (h=nHeight-1;h>-1;h--) {
				memcpy(pTemp,p8,nWidth*3); // just copy the whole line straight into bitmap
				pTemp += nWidth*3; // go to next line down
				p8 -= nWidth*3; // go to next line up
			}
			break;
		}
		case 32: { // top to bottom 32-bit RGB --> bottom to top 24-bit RGB (tested)
			/*
			initial = rrrrrrrr gggggggg bbbbbbbb xxxxxxxx (8 8 8 x)
			(last 8 bits not used)

			rrrrrrrr gggggggg bbbbbbbbbb xxxxxxxx
			|||||||| |||||||| ||||||||||
			RRRRRRRR GGGGGGGG BBBBBBBBBB

			final = RRRRRRRR GGGGGGGG BBBBBBBB (8 8 8)
			*/

			// start at the bottom line
			unsigned char *p8 = pAviBuffer + (nWidth*(nHeight-1)<<2);

			for (h=nHeight-1;h>-1;h--) {
				for (w=nWidth-1;w>-1;w--) {
					memcpy(pTemp,p8,3); // just copy 24 bits straight into bitmap
					pTemp += 3; // go to next pixel
					p8 += 4; // go to next pixel
				}
				p8 -= nWidth<<3; // go to next line up
			}
			break;
		}
		default:
			return 1; // unsupported bitdepth
	} // end of switch

	return 0;
}

// Flips an already converted buffer
static int MakeBitmapFlipped()
{
	INT32 nWidth = FBAvi.bih.biWidth;
	INT32 nHeight = FBAvi.bih.biHeight;
	UINT8 *pTemp = FBAvi.pBitmapBuf2; // walks through and fills the bitmap

	// start at the bottom line
	UINT8 *p8 = FBAvi.pBitmapBuf1 + (3 * nWidth * nHeight) - 3;

	for (INT32 i = 0; i < nHeight * nWidth; i++) {
		memcpy(pTemp, p8, 3);   // just copy 24 bits straight into bitmap
		pTemp += 3;             // go to next pixel
		p8 -= 3;                // go to prev pixel
	}
	FBAvi.pBitmap = FBAvi.pBitmapBuf2;

	return 0;
}

// Converts video buffer to bitmap, rotate counter clockwise
// Returns: 0 (sucessful), 1 (failed)
static int MakeBitmapRotateCCW()
{
	int w,h;
	int nWidth = FBAvi.bih.biWidth;
	int nHeight = FBAvi.bih.biHeight;
	unsigned char *pTemp = FBAvi.pBitmapBuf1; // walks through and fills the bitmap


	if (pAviBuffer == NULL) {
		return 1; // video buffer is empty
	}
	//bprintf(0, _T("%d,"), nVidImageDepth);
	switch (nVidImageDepth) {
		case 15: { // top to bottom 15-bit RGB --> bottom to top 24-bit RGB

			// start at upper left corner
			short *p16 = (short *)pAviBuffer;
			short *p16start = p16;

			for (h=1;h<=nHeight;h++) {
				for (w=nWidth-1;w>-1;w--) {
					short n16 = *p16; // get next pixel
					*pTemp = (unsigned char)((n16&0x1f)<<3);  // R
					*pTemp |= (*pTemp++)>>5;                  // R
					*pTemp = (unsigned char)((n16>>2)&0xf8);  // G
					*pTemp |= (*pTemp++)>>5;                  // G
					*pTemp = (unsigned char)((n16>>7)&0xf8);  // B
					*pTemp |= (*pTemp++)>>5;                  // B
					p16 += nHeight; // go to next pixel
				}
				p16 = p16start + h; // go to next column
			}
			break;
		}
		case 16: { // top to bottom 16-bit RGB --> bottom to top 24-bit RGB

			// start at upper left corner
			short *p16 = (short *)pAviBuffer;
			short *p16start = p16;

			for (h=1;h<=nHeight;h++) {
				for (w=nWidth-1;w>-1;w--) {
					short n16 = *p16; // get next pixel
					*pTemp = (unsigned char)((n16&0x1f)<<3);  // R
					*pTemp |= (*pTemp++)>>5;                  // R
					*pTemp = (unsigned char)((n16>>3)&0xfc);  // G
					*pTemp |= (*pTemp++)>>6;                  // G
					*pTemp = (unsigned char)((n16>>8)&0xf8);  // B
					*pTemp |= (*pTemp++)>>5;                  // B
					p16 += nHeight; // go to next pixel
				}
				p16 = p16start + h; // go to next column
			}
			break;
		}
		case 24: { // top to bottom 24-bit RGB --> bottom to top 24-bit RGB

			// start at upper left corner
			unsigned char *p8 = pAviBuffer;
			unsigned char *p8start = p8;

			for (h=1;h<=nHeight;h++) {
				for (w=nWidth-1;w>-1;w--) {
					memcpy(pTemp,p8,3); // just copy 24 bits straight into bitmap
					pTemp += 3; // go to next pixel
					p8 += nHeight*3; // go to next pixel
				}
				p8 = p8start + h*3; // go to next column
			}
			break;
		}
		case 32: { // top to bottom 32-bit RGB --> bottom to top 24-bit RGB

			// start at upper left corner
			unsigned char *p8 = pAviBuffer;
			unsigned char *p8start = p8;

			for (h=1;h<=nHeight;h++) {
				for (w=nWidth-1;w>-1;w--) {
					memcpy(pTemp,p8,3); // just copy 24 bits straight into bitmap
					pTemp += 3; // go to next pixel
					p8 += nHeight<<2; // go to next pixel
				}
				p8 = p8start + (h<<2); // go to next column
			}
			break;
		}
		default:
			return 1; // unsupported bitdepth
	}

	return 0;
}

// Sets the format for video stream.
static void AviSetVidFormat()
{
	// set the format of bitmap
	memset(&FBAvi.bih,0,sizeof(BITMAPINFOHEADER));
	FBAvi.bih.biSize = sizeof(BITMAPINFOHEADER);

	//BurnDrvGetScreen((int *)&FBAvi.bih.biWidth,(int *)&FBAvi.bih.biHeight);
	INT32 ww,hh;
	BurnDrvGetVisibleSize(&ww, &hh);
	FBAvi.bih.biWidth = ww;
	FBAvi.bih.biHeight = hh;

	FBAvi.pBitmap = FBAvi.pBitmapBuf1;

	// check for rotation and choose bitmap function
	if(BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		// counter-clockwise rotation
		FBAvi.MakeBitmap = MakeBitmapRotateCCW;
	} else {
		// no rotation
		FBAvi.MakeBitmap = MakeBitmapNoRotate;
	}

	if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
		// flipped.
		FBAvi.flippedmode = 1;
	}

	FBAvi.bih.biPlanes = 1;
	// use 24 bpp since most compressors support 24 bpp for input
	FBAvi.bih.biBitCount = 24;
	FBAvi.bih.biCompression = BI_RGB; // uncompressed RGB
	FBAvi.bih.biSizeImage = 3 * FBAvi.bih.biWidth * FBAvi.bih.biHeight;
}

// Sets the format for the audio stream.
static void AviSetAudFormat()
{
	memset(&FBAvi.wfx, 0, sizeof(WAVEFORMATEX));
	FBAvi.wfx.cbSize = sizeof(WAVEFORMATEX);
	FBAvi.wfx.wFormatTag = WAVE_FORMAT_PCM;
	FBAvi.wfx.nChannels = 2; // stereo
	FBAvi.wfx.nSamplesPerSec = nBurnSoundRate; // sample rate
	FBAvi.wfx.wBitsPerSample = 16; // 16-bit
	FBAvi.wfx.nBlockAlign = 4; // bytes per sample
	FBAvi.wfx.nAvgBytesPerSec = FBAvi.wfx.nSamplesPerSec * FBAvi.wfx.nBlockAlign;
}

// Creates the video stream.
// Returns: 0 (successful), 1 (failed)
static int AviCreateVidStream()
{
	int nRet;
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

	// let user choose the compressor and compression options
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
		(int) FBAvi.bih.biWidth,
		(int) FBAvi.bih.biHeight);
	if (nRet == 0) {
#ifdef AVI_DEBUG
		bprintf(0, _T("    AVI Error: SetRect() failed.\n"));
#endif
		return 1;
	}

	// create the video stream in the avi file
	hRet = AVIFileCreateStream(
		FBAvi.pFile, // file pointer
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
		FBAvi.psVidCompressed, // pointer to the opened video output stream
		0, // starting position
		&FBAvi.bih, // stream format
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
static int AviCreateAudStream()
{
	HRESULT hRet;

	// fill in the header for the audio stream
	//
	// - dwInitialFrames specifies how much to skew the
	//   audio data ahead of the video frames in interleaved
	//   files (typically 0.75 sec).
	//  
	memset(&FBAvi.audh, 0, sizeof(FBAvi.audh));
	FBAvi.audh.fccType                = streamtypeAUDIO; // stream type
	FBAvi.audh.dwScale                = FBAvi.wfx.nBlockAlign;
	FBAvi.audh.dwRate                 = FBAvi.wfx.nAvgBytesPerSec;
	FBAvi.audh.dwInitialFrames        = 1; // audio skew
	FBAvi.audh.dwSuggestedBufferSize  = nBurnSoundLen<<2;
	FBAvi.audh.dwSampleSize           = FBAvi.wfx.nBlockAlign;	

	// create the audio stream
	hRet = AVIFileCreateStream(
		FBAvi.pFile, // file pointer
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
		FBAvi.psAud, // pointer to the opened audio output stream
		0, // starting position
		&FBAvi.wfx, // stream format
		sizeof(WAVEFORMATEX)); // format size
	if (hRet) {
#ifdef AVI_DEBUG
		bprintf(0, _T("    AVI Error: AVIStreamSetFormat() failed.\n"));
#endif
		return 1;
	}
				
	return 0;
}


// Records 1 frame worth of data to the output stream
// Returns: 0 (successful), 1 (failed)
int AviRecordFrame(int bDraw)
{
	HRESULT hRet;
	/*
	When FBA is frame skipping (bDraw == 0), we don't
	need to	build a new bitmap, nor encode video frame.
	Just increment the frame number.  However, if avi
	is in vid/aud interleaved mode, audio must be recorded
	every frame regardless of frameskip.
	*/
	if(bDraw) {
		if(FBAvi.MakeBitmap()) {
#ifdef AVI_DEBUG
			bprintf(0, _T(" pburndraw = %X.\n"), pBurnDraw);
			bprintf(0, _T(" pAviBuffer = %X.\n"), pAviBuffer);

			bprintf(0, _T("    AVI Error: MakeBitmap() failed.\n"));
#endif
			return 1;
		}

		if (FBAvi.flippedmode) MakeBitmapFlipped();

		// compress the bitmap and write to AVI output stream
		hRet = AVIStreamWrite(
			FBAvi.psVidCompressed, // stream pointer
			FBAvi.nFrameNum, // time of this frame
			1, // number to write
			(LPBYTE) FBAvi.pBitmap, // pointer to data
			FBAvi.bih.biSizeImage, // size of this frame
			AVIIF_KEYFRAME, // flags
			NULL,
			NULL);
		if (hRet != AVIERR_OK) {
#ifdef AVI_DEBUG
			bprintf(0, _T("    AVI Error: AVIStreamWrite() failed.\n"));
#endif
			return 1;
		}
	}

	// interleave audio
	if (nAviIntAudio) {
		// write the PCM audio data to AVI output stream
		hRet = AVIStreamWrite(
			FBAvi.psAud, // stream pointer
			FBAvi.nFrameNum, // time of this frame
			1, // number to write
			(LPBYTE) nAudNextSound, // pointer to data
			nBurnSoundLen<<2, // size of data
			AVIIF_KEYFRAME, // flags
			NULL,
			NULL);
		if (hRet != AVIERR_OK) {
#ifdef AVI_DEBUG
			bprintf(0, _T("    AVI Error: AVIStreamWrite() failed.\n"));
#endif
			return 1;
		}
	}

	FBAvi.nFrameNum++;
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
void AviStop()
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
			ICCompressorFree(&FBAvi.compvar);
		}

		AVIFileExit();
		Sleep(150); // Allow compressor-codec threads/windows/etc time to finish gracefully
		FreeBMP();

#ifdef AVI_DEBUG
		bprintf(0, _T(" ** AVI recording finished.\n"));
		bprintf(0, _T("    total frames recorded = %u\n"),FBAvi.nFrameNum+1);
#endif
		nAviStatus = 0;
		nAviFlags = 0;

		memset(&FBAvi, 0, sizeof(FBAVI));
		MenuEnableItems();
	}
}

// Starts AVI recording.
// Returns: 0 (successful), 1 (failed)
int AviStart()
{
	// initialize local variables
	memset (&FBAvi, 0, sizeof(FBAVI));

	// initialize avi file library
	// - we need VFW version 1.1 or greater
	if (HIWORD(VideoForWindowsVersion()) < 0x010a){
		// VFW verison is too old, disable AVI recording
		return 1;
	}

	AVIFileInit();

	nAviFlags |= FBAVI_VFW_INIT; // avi file library initialized

	// create the avi file
	if(AviCreateFile()) {
		return 1;
	}

	// set video format
	AviSetVidFormat();

	// allocate memory for 2x 24bpp bitmap buffers
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

	// interleave audio
	if (nAviIntAudio) {
		// set audio format
		AviSetAudFormat();
		// create the audio stream
		if(AviCreateAudStream()) {
			return 1;
		}
	}

	// record the first frame
	if(AviRecordFrame(1)) {
		return 1;
	}

	nAviStatus = 1; // recording started
	MenuEnableItems();
	return 0;
}
