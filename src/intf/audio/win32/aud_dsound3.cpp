// DirectSound module
#include "burner.h"
#include "aud_dsp.h"
#include <math.h>

#include <InitGuid.h>
#define DIRECTSOUND_VERSION  0x0300			// Only need version from DirectX 3
#include <dsound.h>

#include "dsound_core.h"

// Sound is split into a series of 'segs', one seg for each frame
// The Loop buffer is a multiple of this seg length.
static IDirectSound* pDS = NULL;			// DirectSound interface
static IDirectSoundBuffer* pdsbPrim = NULL;	// Primary buffer
static IDirectSoundBuffer* pdsbLoop = NULL;	// (Secondary) Loop buffer

static int nAudBufferCount = 10;
static int nAudBufferLen = 0;
static int nAudBufferRead = 0;
static int nAudBufferWrite = 0;

static int nDSoundFps;								// Application fps * 100
static long nDSoundVol = 0;

int DxBlankSound()
{
	void *pData1 = NULL, *pData2 = NULL;
	DWORD nLen1 = 0, nLen2 = 0;

	// Blank the whole loop buffer
	if (FAILED(pdsbLoop->Lock(0, nAudBufferLen, &pData1, &nLen1, &pData2, &nLen2, 0))) {
		return 1;
	}
	memset(pData1, 0, nLen1);
	pdsbLoop->Unlock(pData1, nLen1, pData2, nLen2);

	// Blank the current frame buffer
	if (nAudNextSound) {
		AudWriteSilence();
	}

	pdsbLoop->SetCurrentPosition(0);
	nAudBufferRead = 0;
	nAudBufferWrite = 0;

	return 0;
}

static int DxSoundInit()
{
	if (nAudSampleRate[0] <= 0) {
		return 1;
	}

	nDSoundFps = nAppVirtualFps;

	// Calculate the Seg Length and Loop length (round to nearest sample)
	nAudSegLen = (nAudSampleRate[0] * 100 + (nDSoundFps >> 1)) / nDSoundFps;
	nAudAllocSegLen = nAudSegLen << 2;
	nAudBufferLen = nAudBufferCount * nAudAllocSegLen;

	// Make the format of the sound
	WAVEFORMATEX wfx;
	memset(&wfx, 0, sizeof(wfx));
	wfx.cbSize = sizeof(wfx);
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;										  // stereo
	wfx.nSamplesPerSec = nAudSampleRate[0];	// sample rate
	wfx.wBitsPerSample = 16;								// 16-bit
	wfx.nBlockAlign = 4;									  // bytes per sample
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	// Create the DirectSound interface
	if (FAILED(_DirectSoundCreate(NULL, &pDS, NULL))) {
		return 1;
	}

	// Set the coop level
	pDS->SetCooperativeLevel(hScrnWnd, DSSCL_PRIORITY);

	// Make the primary sound buffer
	DSBUFFERDESC dsbd;
	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize = sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
	if (FAILED(pDS->CreateSoundBuffer(&dsbd, &pdsbPrim, NULL))) {
		AudSoundExit();
		return 1;
	}

	// Set the format of the primary sound buffer (not critical if it fails)
	if (nAudSampleRate[0] < 44100) {
		wfx.nSamplesPerSec = 44100;
	}
	pdsbPrim->SetFormat(&wfx);
	wfx.nSamplesPerSec = nAudSampleRate[0];

	// Make the loop sound buffer
	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize = sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME;
	dsbd.dwBufferBytes = nAudBufferLen;
	dsbd.lpwfxFormat = &wfx;								// Same format as the primary buffer
	if (FAILED(pDS->CreateSoundBuffer(&dsbd, &pdsbLoop, NULL))) {
		AudSoundExit();
		return 1;
	}

	// The next sound block to put in the stream
	nAudNextSound = (short*)malloc(nAudAllocSegLen);
	if (nAudNextSound == NULL) {
		AudSoundExit();
		return 1;
	}

	DspInit();

	return 0;
}

static int DxSoundExit()
{
	DspExit();

	if (nAudNextSound) {
		free(nAudNextSound);
		nAudNextSound = NULL;
	}

	// Release the (Secondary) Loop Sound Buffer
	RELEASE(pdsbLoop);
	// Release the Primary Sound Buffer
	RELEASE(pdsbPrim);
	// Release the DirectSound interface
	RELEASE(pDS);

	return 0;
}

static int AudioBufferDiff(int write, int read, int len)
{
	if (write >= read) {
		return (write - read) > (len/2) ? read + len - write : write - read;
	}
	else {
		return (read - write) > (len/2) ? write + len - read : read - write;
	}
}

static int DxSoundCheck()
{
	DWORD nPlay = 0, nWrite = 0;
	pdsbLoop->GetCurrentPosition(&nPlay, &nWrite);
	nAudBufferRead = nPlay / nAudAllocSegLen;

	int nDiff = AudioBufferDiff(nAudBufferWrite, nAudBufferRead, nAudBufferCount);
	if (nDiff < 0) {
		//VidDebug("Dry Buffers", nDiff, nAudBufferRead);
		nAudBufferWrite = nAudBufferRead;
	}

	if (nDiff < 4) {
		//VidDebug("Close to Dry Buffers", nDiff, 0);
		
		// Lock and fill next write buffer segment
		void *pData1 = NULL, *pData2 = NULL;
		DWORD nLen1 = 0, nLen2 = 0;
		if (SUCCEEDED(pdsbLoop->Lock(nAudBufferWrite * nAudAllocSegLen, nAudAllocSegLen, &pData1, &nLen1, &pData2, &nLen2, 0))) {
			memcpy(pData1, nAudNextSound, nLen1);
			pdsbLoop->Unlock(pData1, nLen1, pData2, nLen2);
			nAudBufferWrite = (nAudBufferWrite + 1) % nAudBufferCount;
		}
	}

	return 0;
}

static int DxSoundFrame()
{
	DWORD nPlay = 0, nWrite = 0;
	pdsbLoop->GetCurrentPosition(&nPlay, &nWrite);
	nAudBufferRead = nPlay / nAudAllocSegLen;

	int nDiff = AudioBufferDiff(nAudBufferWrite, nAudBufferRead, nAudBufferCount);
	if (nDiff > (nAudSegCount - 1)) {
		//VidDebug("Writing too fast", nDiff, 0);
		return 0;
	}

	// apply DSP to recent frame
	if (nAudDSPModule[0])	{
		DspDo(nAudNextSound, nAudSegLen);
	}

	// Lock and fill next write buffer segment
	void *pData1 = NULL, *pData2 = NULL;
	DWORD nLen1 = 0, nLen2 = 0;
	if (SUCCEEDED(pdsbLoop->Lock(nAudBufferWrite * nAudAllocSegLen, nAudAllocSegLen, &pData1, &nLen1, &pData2, &nLen2, 0))) {
		memcpy(pData1, nAudNextSound, nLen1);
		pdsbLoop->Unlock(pData1, nLen1, pData2, nLen2);
		nAudBufferWrite = (nAudBufferWrite + 1) % nAudBufferCount;
	}

	return 0;
}

static int DxSoundPlay()
{
	pdsbLoop->SetVolume(nDSoundVol);
	if (FAILED(pdsbLoop->Play(0, 0, DSBPLAY_LOOPING))) {
		return 1;
	}
	bAudPlaying = 1;

	return 0;
}

static int DxSoundStop()
{
	bAudPlaying = 0;

	if (bAudOkay == 0) {
		return 1;
	}

	pdsbLoop->Stop();

	return 0;
}

static int DxSoundSetVolume()
{
	if (nAudVolume == 10000) {
		nDSoundVol = DSBVOLUME_MAX;
	} else {
		if (nAudVolume == 0) {
			nDSoundVol = DSBVOLUME_MIN;
		} else {
			nDSoundVol = DSBVOLUME_MAX - (long)(10000.0 * pow(10.0, nAudVolume / -5000.0)) + 100;
		}
	}

	if (nDSoundVol < DSBVOLUME_MIN) {
		nDSoundVol = DSBVOLUME_MIN;
	}

	if (FAILED(pdsbLoop->SetVolume(nDSoundVol))) {
		return 0;
	}

	return 1;
}

static int DxGetSettings(InterfaceInfo* pInfo)
{
	TCHAR szString[MAX_PATH] = _T("");

	_sntprintf(szString, MAX_PATH, _T("Audio is delayed by approx. %ims"), int(100000.0 / (nDSoundFps / (nAudSegCount - 1.0))));
	IntInfoAddStringModule(pInfo, szString);

	return 0;
}

struct AudOut AudOutDx = { DxBlankSound, DxSoundInit, DxSoundExit, DxSoundCheck, DxSoundFrame, DxSoundPlay, DxSoundStop, DxSoundSetVolume, DxGetSettings, _T("DirectSound3 audio output") };
