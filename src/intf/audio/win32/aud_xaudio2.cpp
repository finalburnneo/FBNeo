#include "burner.h"
#include "aud_dsp.h"

#include <InitGuid.h>
#include <xaudio2.h>
#include <xaudio2fx.h>

static IXAudio2* pXAudio2 = NULL;
static IXAudio2MasteringVoice* pMasterVoice = NULL;
static IXAudio2SourceVoice* pSourceVoice = NULL;
static XAUDIO2_BUFFER sAudioBuffer;

static BYTE* pAudioBuffers = NULL;
static int nAudioBuffer = 0;

static int nXAudio2Fps = 0;					// Application fps * 100
static float nXAudio2Vol = 1.0f;

struct StreamingVoiceContext : public IXAudio2VoiceCallback
{
	HANDLE hBufferEndEvent;

	StreamingVoiceContext() {
		hBufferEndEvent = NULL;
		hBufferEndEvent = CreateEvent( NULL, FALSE, FALSE, NULL);
	}
	virtual ~StreamingVoiceContext() {
		CloseHandle(hBufferEndEvent);
		hBufferEndEvent = NULL;
	}

	STDMETHOD_(void, OnBufferEnd) (void * /*pBufferContext*/) {
		SetEvent(hBufferEndEvent);
	}

	// dummies:
	STDMETHOD_(void, OnVoiceProcessingPassStart) (UINT32 /*BytesRequired*/) {}
	STDMETHOD_(void, OnVoiceProcessingPassEnd) () {}
	STDMETHOD_(void, OnStreamEnd) () {}
	STDMETHOD_(void, OnBufferStart) (void * /*pBufferContext*/) {}
	STDMETHOD_(void, OnLoopEnd) (void * /*pBufferContext*/) {}
	STDMETHOD_(void, OnVoiceError) (void * /*pBufferContext*/, HRESULT /*Error*/) {};
};
StreamingVoiceContext voiceContext;

static int XAudio2Exit();

static int XAudio2BlankSound()
{
	if (pAudioBuffers) {
		memset(pAudioBuffers, 0, nAudSegCount * nAudAllocSegLen);
	}

	// Also blank the nAudNextSound buffer
	if (nAudNextSound) {
		AudWriteSilence();
	}

	return 0;
}

static int XAudio2InitVoices()
{
	HRESULT hr;

	// Create a mastering voice
	if (FAILED(hr = pXAudio2->CreateMasteringVoice(&pMasterVoice, XAUDIO2_DEFAULT_CHANNELS, nAudSampleRate[1], 0, 0, NULL))) {
		return 1;
	}

	// Make the format of the sound
	WAVEFORMATEX wfx;
	memset(&wfx, 0, sizeof(wfx));
	wfx.cbSize = sizeof(wfx);
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;												// stereo
	wfx.nSamplesPerSec = nAudSampleRate[1];		// sample rate
	wfx.wBitsPerSample = 16;									// 16-bit
	wfx.nBlockAlign = wfx.wBitsPerSample * wfx.nChannels / 8;	// bytes per sample
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	// Create the source voice
	if (FAILED(hr = pXAudio2->CreateSourceVoice(&pSourceVoice, &wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, &voiceContext, NULL, NULL))) {
		return 1;
	}

	return 0;
}

static int XAudio2Init()
{
	if (nAudSampleRate[1] <= 0) {
		return 0;
	}

	nXAudio2Fps = nAppVirtualFps;

	// Calculate the Seg Length and Loop length (round to nearest sample)
	nAudSegLen = (nAudSampleRate[1] * 100 + (nXAudio2Fps >> 1)) / nXAudio2Fps;
	nAudAllocSegLen = nAudSegLen << 2;

	// Initialize XAudio2
	if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
		return 1;
	}

	HRESULT hr;
	if (FAILED(hr = XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
		CoUninitialize();
		return 1;
	}

	if (XAudio2InitVoices()) {
		XAudio2Exit();
		return 1;
	}

	memset(&sAudioBuffer, 0, sizeof(sAudioBuffer));

	nAudNextSound = (short*)malloc(nAudAllocSegLen);		// The next sound block to put in the stream
	if (nAudNextSound == NULL) {
		XAudio2Exit();
		return 1;
	}

	// create own buffers to store sound data because it must not be
	// manipulated while the voice plays from it
	pAudioBuffers = (BYTE *)malloc(nAudSegCount * nAudAllocSegLen);
	if (pAudioBuffers == NULL) {
		XAudio2Exit();
		return 1;
	}
	nAudioBuffer = 0;

	DspInit();

	return 0;
}

static int XAudio2Exit()
{
	DspExit();

	if (nAudNextSound) {
		free(nAudNextSound);
		nAudNextSound = NULL;
	}

	if (pAudioBuffers) {
		free(pAudioBuffers);
		pAudioBuffers = NULL;
	}

	// Cleanup XAudio2
	if (pSourceVoice) {
		pSourceVoice->Stop(0);
		pSourceVoice->DestroyVoice();
		pSourceVoice = NULL;
	}
	if (pMasterVoice) {
		pMasterVoice->DestroyVoice();
		pMasterVoice = NULL;
	}

	RELEASE(pXAudio2);
	CoUninitialize();

	return 0;
}

static int XAudio2Check()
{
	if (!pSourceVoice || !pAudioBuffers) {
		return 1;
	}

	XAUDIO2_VOICE_STATE vState;
	pSourceVoice->GetState(&vState);

	if (vState.BuffersQueued < (unsigned int)(nAudSegCount - 1)) {
		if (vState.BuffersQueued < 3) {
			// buffers ran dry
			//VidDebug("Close to dry Buffers", vState.BuffersQueued, 0);

			// dsp update
			if (nAudDSPModule[1] & 1) {
				if (bRunPause)
					AudWriteSilence();
				else
					DspDo(nAudNextSound, nAudSegLen);
			}

			// copy & protect the audio data in own memory area while playing it
			if (kNetSpectator) {
				memset(&pAudioBuffers[nAudioBuffer * nAudAllocSegLen], 0, nAudAllocSegLen);
			}
			else {
				memcpy(&pAudioBuffers[nAudioBuffer * nAudAllocSegLen], nAudNextSound, nAudAllocSegLen);
			}

			sAudioBuffer.AudioBytes = nAudAllocSegLen;
			sAudioBuffer.pAudioData = &pAudioBuffers[nAudioBuffer * nAudAllocSegLen];

			nAudioBuffer++;
			nAudioBuffer %= (nAudSegCount);

			pSourceVoice->SubmitSourceBuffer(&sAudioBuffer); // send buffer to queue
		}
	}

	return 0;
}

static int XAudio2Frame()
{
	XAUDIO2_VOICE_STATE vState;
	pSourceVoice->GetState(&vState);
	if (vState.BuffersQueued == (nAudSegCount - 1)) {
		// No more buffers allowed to queue
		return 0;
	}

	// dsp update
	if (nAudDSPModule[1] & 1) {
		if (bRunPause)
			AudWriteSilence();
		else
			DspDo(nAudNextSound, nAudSegLen);
	}

	// copy & protect the audio data in own memory area while playing it
	memcpy(&pAudioBuffers[nAudioBuffer * nAudAllocSegLen], nAudNextSound, nAudAllocSegLen);

	sAudioBuffer.AudioBytes = nAudAllocSegLen;
	sAudioBuffer.pAudioData = &pAudioBuffers[nAudioBuffer * nAudAllocSegLen];

	nAudioBuffer++;
	nAudioBuffer %= (nAudSegCount);

	pSourceVoice->SubmitSourceBuffer(&sAudioBuffer); // send buffer to queue

	return 0;
}


static int XAudio2Play()
{
	if (pSourceVoice == NULL) {
		return 1;
	}

	XAudio2BlankSound();
	pSourceVoice->SetVolume(nXAudio2Vol);

	if (FAILED(pSourceVoice->Start(0))) {
		return 1;
	}
	return 0;
}

static int XAudio2Stop()
{
	if (!bAudOkay) {
		return 1;
	}

	if (pSourceVoice) {
		pSourceVoice->Stop(0);
	}

	return 0;
}

static int XAudio2SetVolume()
{
	if (nAudVolume == 10000) {
		nXAudio2Vol = 1.0f;
	} else {
		if (nAudVolume == 0) {
			nXAudio2Vol = 0.0f;
		} else {
			nXAudio2Vol = 1.0f - (1.0f * (float)pow(10.0, nAudVolume / -5000.0)) + 0.01f;
		}
	}

	if (nXAudio2Vol < 0.0f) {
		nXAudio2Vol = 0.0f;
	}

	if (!pSourceVoice || FAILED(pSourceVoice->SetVolume(nXAudio2Vol))) {
		return 1;
	}

	return 0;
}

static int XAudio2GetSettings(InterfaceInfo* pInfo)
{
	TCHAR szString[MAX_PATH] = _T("");
	_sntprintf(szString, MAX_PATH, _T("Audio is delayed by approx. %ims"), int(100000.0 / (nXAudio2Fps / (nAudSegCount - 1.0))));
	IntInfoAddStringModule(pInfo, szString);
	return 0;
}

struct AudOut AudOutXAudio2 = { XAudio2BlankSound, XAudio2Init, XAudio2Exit, XAudio2Check, XAudio2Frame, XAudio2Play, XAudio2Stop, XAudio2SetVolume, XAudio2GetSettings, _T("XAudio2 audio output") };
