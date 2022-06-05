#include "burner.h"
#include "aud_dsp.h"
#include <mmdeviceapi.h>
#include <audioclient.h>

static IAudioClient* pSoundClient = NULL;
static IAudioRenderClient* pSoundRenderClient = NULL;
static ISimpleAudioVolume* pSoundVolume = NULL;

static int WasapiSoundExit();

int WasapiBlankSound()
{
	// Also blank the nAudNextSound buffer
	if (nAudNextSound) {
		AudWriteSilence();
	}

	return 0;
}

static int WasapiSoundInit()
{
	if (nAudSampleRate[2] <= 0) {
		return 1;
	}

	// Calculate the Seg Length and Loop length (round to nearest sample)
	nAudSegLen = (nAudSampleRate[2] * 100 + (nAppVirtualFps >> 1)) / nAppVirtualFps;
	nAudAllocSegLen = nAudSegLen << 2; // 16 bit, 2 channels

	if (FAILED(CoInitializeEx(0, COINIT_SPEED_OVER_MEMORY))) {
		return 1;
	}

	IMMDeviceEnumerator* enumerator;
	if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) {
		return 1;
	}

	IMMDevice* device;
	if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))) {
		return 1;
	}

	if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (LPVOID*)&pSoundClient))) {
		return 1;
	}


	int error = AUDCLNT_E_BUFFER_SIZE_ERROR;

	WAVEFORMATEXTENSIBLE WaveFormat;
	WaveFormat.Format.cbSize = sizeof(WaveFormat);
	WaveFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	WaveFormat.Format.wBitsPerSample = 16;
	WaveFormat.Format.nChannels = 2;
	WaveFormat.Format.nSamplesPerSec = nAudSampleRate[2];
	WaveFormat.Format.nBlockAlign = (WORD)(WaveFormat.Format.nChannels * WaveFormat.Format.wBitsPerSample / 8);
	WaveFormat.Format.nAvgBytesPerSec = WaveFormat.Format.nSamplesPerSec * WaveFormat.Format.nBlockAlign;
	WaveFormat.Samples.wValidBitsPerSample = 16;
	WaveFormat.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
	WaveFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

	REFERENCE_TIME bufferDuration = (REFERENCE_TIME)((10000.0 * 1000 * 100 / nAppVirtualFps * nAudSegCount[2]) + 0.5);
	if (FAILED(pSoundClient->Initialize(nAudExclusive ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_NOPERSIST, bufferDuration, 0, &WaveFormat.Format, NULL))) {
		return 1;
	}

	if (FAILED(pSoundClient->GetService(IID_PPV_ARGS(&pSoundRenderClient)))) {
		return 1;
	}

	if (FAILED(pSoundClient->GetService(IID_PPV_ARGS(&pSoundVolume)))) {
		return 1;
	}

	UINT32 soundFrameCount;
	if (FAILED(pSoundClient->GetBufferSize(&soundFrameCount))) {
		return 1;
	}

	// the next sound frame to put in the stream
	nAudNextSound = (short*)malloc(nAudAllocSegLen);
	if (nAudNextSound == NULL) {
		WasapiSoundExit();
		return 1;
	}

	DspInit();

	pSoundClient->Start();

	return 0;
}

static int WasapiSoundExit()
{
	if (pSoundRenderClient) {
		pSoundRenderClient->Release();
		pSoundRenderClient = NULL;
	}

	if (pSoundVolume) {
		pSoundVolume->Release();
		pSoundVolume= NULL;
	}

	if (pSoundClient) {
		pSoundClient->Stop();
		pSoundClient->Release();
		pSoundClient = NULL;
	}

	DspExit();

	if (nAudNextSound) {
		free(nAudNextSound);
		nAudNextSound = NULL;
	}

	return 0;
}

static int WasapiSoundCheck()
{
	return 0;
}

static int WasapiSoundFrame()
{
	// apply DSP to recent frame
	if (nAudDSPModule[2]) {
		DspDo(nAudNextSound, nAudSegLen);
	}

	BYTE* soundBufferData;
	if (SUCCEEDED(pSoundRenderClient->GetBuffer(nAudSegLen, &soundBufferData)))
	{
		memcpy(soundBufferData, nAudNextSound, nAudAllocSegLen);
		pSoundRenderClient->ReleaseBuffer(nAudSegLen, 0);
	}

	return 0;
}

static int WasapiSoundPlay()
{
	WasapiBlankSound();

	return 0;
}

static int WasapiSoundStop()
{
	return 0;
}

static int WasapiSoundSetVolume()
{
	if (pSoundVolume) {
		pSoundVolume->SetMasterVolume(nAudVolume / 10000.0f, 0);
	}

	return 1;
}

static int WasapiGetSettings(InterfaceInfo* pInfo)
{
	TCHAR szString[MAX_PATH] = _T("");
	_sntprintf(szString, MAX_PATH, _T("Audio is delayed by approx. %ims"), int(100000.0 / (60.0 / (nAudSegCount[2] - 1.0))));
	IntInfoAddStringModule(pInfo, szString);

	return 0;
}

struct AudOut AudOutWasapi = { WasapiBlankSound, WasapiSoundInit, WasapiSoundExit, WasapiSoundCheck, WasapiSoundFrame, WasapiSoundPlay, WasapiSoundStop, WasapiSoundSetVolume, WasapiGetSettings, _T("Wasapi audio output") };
