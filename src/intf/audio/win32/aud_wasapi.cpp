// WASAPI (Windows Audio Session API) audio output module for FBNeo
// - dink 2026
#include "burner.h"
#include "aud_dsp.h"

#include <mmdeviceapi.h>
#include <audioclient.h>

#ifdef _MSC_VER
#pragma comment(lib, "ole32")
#endif

// These flags are missing from some older SDK / MinGW headers
#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM		0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY	0x08000000
#endif
#ifndef AUDCLNT_E_DEVICE_INVALIDATED
#define AUDCLNT_E_DEVICE_INVALIDATED			((HRESULT)0x88890004L)
#endif
#ifndef AUDCLNT_E_RESOURCES_INVALIDATED
#define AUDCLNT_E_RESOURCES_INVALIDATED			((HRESULT)0x88890026L)
#endif
#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT					0x0003
#endif
#ifndef WAVE_FORMAT_EXTENSIBLE
#define WAVE_FORMAT_EXTENSIBLE					0xFFFE
#endif

// GUIDs are defined locally so we don't depend on __uuidof() or the uuid/ksuser libs
static const GUID FBN_CLSID_MMDeviceEnumerator	= { 0xbcde0395, 0xe52f, 0x467c, { 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e } };
static const GUID FBN_IID_IMMDeviceEnumerator	= { 0xa95664d2, 0x9614, 0x4f35, { 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6 } };
static const GUID FBN_IID_IAudioClient			= { 0x1cb9ad4c, 0xdbfa, 0x4c32, { 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2 } };
static const GUID FBN_IID_IAudioRenderClient	= { 0xf294acfc, 0x3146, 0x4483, { 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2 } };
static const GUID FBN_SUBTYPE_PCM				= { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID FBN_SUBTYPE_IEEE_FLOAT		= { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
// PKEY_Device_FriendlyName
static const PROPERTYKEY FBN_PKEY_Device_FriendlyName = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };

#define WASAPI_PLUGIN		2							// our index in nAudSampleRate[] / nAudDSPModule[]
#define REFTIMES_PER_SEC	10000000

static IMMDeviceEnumerator* pEnumerator = NULL;
static IMMDevice* pDevice = NULL;
static IAudioClient* pAudioClient = NULL;
static IAudioRenderClient* pRenderClient = NULL;
static HANDLE hWasapiEvent = NULL;

int (*WasapiGetNextSound)(int);						// Callback used to request more sound
static int WasapiSetVolume(); // forward

static int nWasapiFps = 0;							// Application fps * 100
static float fWasapiVol = 1.0f;						// Software volume (allows > 100%)

static UINT32 nBufferFrames = 0;					// Size of the WASAPI buffer (frames)
static UINT32 nTargetFrames = 0;					// How full we keep the buffer (frames) = latency

static bool bStreamStarted = false;
static bool bEventDriven = false;					// false: fall back to polling
static bool bBlanked = false;						// AudBlankSound() was called during SoundCheck

// Output format actually used by the stream
enum { FMT_S16 = 0, FMT_S24, FMT_S32, FMT_F32 };
static int nOutFormat = FMT_S16;
static int nOutChannels = 2;
static int nOutBlockAlign = 4;
static bool bAutoConvert = false;

static TCHAR szDeviceName[128] = _T("");

// Device-lost handling
static bool bDeviceLost = false;
static DWORD nLastReopenTry = 0;
static DWORD nLostLastFrame = 0;

static int WasapiGetNextSoundFiller(int)
{
	if (nAudNextSound == NULL) {
		return 1;
	}
	AudWriteSilence();

	return 0;
}

static int WasapiSetCallback(int (*pCallback)(int))
{
	if (pCallback == NULL) {
		WasapiGetNextSound = WasapiGetNextSoundFiller;
	} else {
		WasapiGetNextSound = pCallback;
	}

	return 0;
}

// Sample conversion: 16-bit stereo -> device format, with software volume

static inline INT32 WasapiClip16(INT32 s)
{
	if (s > 32767) return 32767;
	if (s < -32768) return -32768;
	return s;
}

static void WasapiConvert(BYTE* pDst, const INT16* pSrc, UINT32 nFrames)
{
	const bool bFullVol = (fWasapiVol == 1.0f);
	const INT32 nVol = (INT32)(fWasapiVol * 65536.0f);	// 16.16 fixed point

	for (UINT32 i = 0; i < nFrames; i++) {
		INT32 l = pSrc[i * 2 + 0];
		INT32 r = pSrc[i * 2 + 1];

		if (!bFullVol) {
			l = WasapiClip16((INT32)(((INT64)l * nVol) >> 16));
			r = WasapiClip16((INT32)(((INT64)r * nVol) >> 16));
		}

		switch (nOutFormat) {
			case FMT_S16: {
				INT16* d = (INT16*)pDst;
				d[0] = (INT16)l;
				d[1] = (INT16)r;
				for (int c = 2; c < nOutChannels; c++) d[c] = 0;
				break;
			}
			case FMT_S24: {
				BYTE* d = pDst;
				INT32 v[2] = { l << 8, r << 8 };
				for (int c = 0; c < 2; c++) {
					d[c * 3 + 0] = (BYTE)(v[c] & 0xff);
					d[c * 3 + 1] = (BYTE)((v[c] >> 8) & 0xff);
					d[c * 3 + 2] = (BYTE)((v[c] >> 16) & 0xff);
				}
				memset(d + 6, 0, (nOutChannels - 2) * 3);
				break;
			}
			case FMT_S32: {
				INT32* d = (INT32*)pDst;
				d[0] = l << 16;
				d[1] = r << 16;
				for (int c = 2; c < nOutChannels; c++) d[c] = 0;
				break;
			}
			case FMT_F32: {
				float* d = (float*)pDst;
				d[0] = (float)l * (1.0f / 32768.0f);
				d[1] = (float)r * (1.0f / 32768.0f);
				for (int c = 2; c < nOutChannels; c++) d[c] = 0.0f;
				break;
			}
		}

		pDst += nOutBlockAlign;
	}
}

// Device / stream setup

static void WasapiReleaseStream()
{
	if (pAudioClient && bStreamStarted) {
		pAudioClient->Stop();
	}
	bStreamStarted = false;

	RELEASE(pRenderClient);
	RELEASE(pAudioClient);
	RELEASE(pDevice);
	RELEASE(pEnumerator);
}

static void WasapiGetDeviceName()
{
	szDeviceName[0] = 0;

	IPropertyStore* pProps = NULL;
	if (pDevice && SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
		PROPVARIANT varName;
		PropVariantInit(&varName);
		if (SUCCEEDED(pProps->GetValue(FBN_PKEY_Device_FriendlyName, &varName)) && varName.vt == VT_LPWSTR && varName.pwszVal) {
#if defined (_UNICODE)
			wcsncpy(szDeviceName, varName.pwszVal, 127);
#else
			WideCharToMultiByte(CP_ACP, 0, varName.pwszVal, -1, szDeviceName, 128, NULL, NULL);
#endif
			szDeviceName[127] = 0;
		}
		PropVariantClear(&varName);
		pProps->Release();
	}
}

// Work out how to feed a given (mix) format; returns 0 if we can handle it
static int WasapiParseFormat(const WAVEFORMATEX* pwfx)
{
	if (pwfx->nChannels < 2) {
		return 1;
	}

	bool bFloat = false;
	bool bPCM = false;

	if (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
		bFloat = true;
	} else if (pwfx->wFormatTag == WAVE_FORMAT_PCM) {
		bPCM = true;
	} else if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && pwfx->cbSize >= 22) {
		const WAVEFORMATEXTENSIBLE* pwfxe = (const WAVEFORMATEXTENSIBLE*)pwfx;
		if (IsEqualGUID(pwfxe->SubFormat, FBN_SUBTYPE_IEEE_FLOAT)) bFloat = true;
		if (IsEqualGUID(pwfxe->SubFormat, FBN_SUBTYPE_PCM))        bPCM = true;
	}

	if (bFloat && pwfx->wBitsPerSample == 32) {
		nOutFormat = FMT_F32;
	} else if (bPCM && pwfx->wBitsPerSample == 16) {
		nOutFormat = FMT_S16;
	} else if (bPCM && pwfx->wBitsPerSample == 24) {
		nOutFormat = FMT_S24;
	} else if (bPCM && pwfx->wBitsPerSample == 32) {
		nOutFormat = FMT_S32;
	} else {
		return 1;
	}

	nOutChannels = pwfx->nChannels;
	nOutBlockAlign = pwfx->nBlockAlign;

	return 0;
}

// Activate a fresh IAudioClient on pDevice and initialise it. A client can only be
// Initialize()d once, so every attempt gets its own instance.
static HRESULT WasapiTryInit(const WAVEFORMATEX* pwfx, DWORD nFlags, REFERENCE_TIME hnsBuffer)
{
	RELEASE(pAudioClient);

	HRESULT hr = pDevice->Activate(FBN_IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
	if (FAILED(hr)) {
		return hr;
	}

	hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, nFlags, hnsBuffer, 0, pwfx, NULL);
	if (FAILED(hr)) {
		RELEASE(pAudioClient);
	}

	return hr;
}

static int WasapiOpenStream()
{
	HRESULT hr;

	if (FAILED(hr = CoCreateInstance(FBN_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, FBN_IID_IMMDeviceEnumerator, (void**)&pEnumerator))) {
		return 1;
	}

	if (FAILED(hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice))) {
		WasapiReleaseStream();
		return 1;
	}

	WasapiGetDeviceName();

	// The buffer we ask for holds nAudSegCount frames of emulation - same latency as the other plugins.
	REFERENCE_TIME hnsBuffer = (REFERENCE_TIME)REFTIMES_PER_SEC * nAudSegLen * nAudSegCount / nAudSampleRate[WASAPI_PLUGIN];

	// Our native format: 16-bit stereo PCM at the selected rate
	WAVEFORMATEX wfx;
	memset(&wfx, 0, sizeof(wfx));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = nAudSampleRate[WASAPI_PLUGIN];
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = 4;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.cbSize = 0;

	const DWORD nAutoConv = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

	bEventDriven = true;
	bAutoConvert = true;
	nOutFormat = FMT_S16; nOutChannels = 2; nOutBlockAlign = 4;

	// 1) 16-bit stereo, let Windows resample/convert to the mix format (Win7+)
	hr = WasapiTryInit(&wfx, AUDCLNT_STREAMFLAGS_EVENTCALLBACK | nAutoConv, hnsBuffer);

	// 2) Device mix format, converted by us (only possible if the rate matches)
	if (FAILED(hr)) {
		WAVEFORMATEX* pMix = NULL;
		IAudioClient* pTmp = NULL;

		bAutoConvert = false;

		if (SUCCEEDED(pDevice->Activate(FBN_IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pTmp))) {
			if (SUCCEEDED(pTmp->GetMixFormat(&pMix)) && pMix) {
				if (pMix->nSamplesPerSec == (DWORD)nAudSampleRate[WASAPI_PLUGIN] && WasapiParseFormat(pMix) == 0) {
					hr = WasapiTryInit(pMix, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsBuffer);
					if (FAILED(hr)) {
						bEventDriven = false;
						hr = WasapiTryInit(pMix, 0, hnsBuffer);
					}
				}
				CoTaskMemFree(pMix);
			}
			pTmp->Release();
		}
	}

	// 3) Last resort: auto-convert without event callback (polling)
	if (FAILED(hr)) {
		bEventDriven = false;
		bAutoConvert = true;
		nOutFormat = FMT_S16; nOutChannels = 2; nOutBlockAlign = 4;
		hr = WasapiTryInit(&wfx, nAutoConv, hnsBuffer);
	}

	if (FAILED(hr) || pAudioClient == NULL) {
#ifdef PRINT_DEBUG_INFO
		bprintf(0, _T("*** WASAPI: unable to initialise audio client (0x%08x).\n"), hr);
#endif
		WasapiReleaseStream();
		return 1;
	}

	if (bEventDriven) {
		if (FAILED(pAudioClient->SetEventHandle(hWasapiEvent))) {
			WasapiReleaseStream();
			return 1;
		}
	}

	if (FAILED(pAudioClient->GetBufferSize(&nBufferFrames))) {
		WasapiReleaseStream();
		return 1;
	}

	if (FAILED(pAudioClient->GetService(FBN_IID_IAudioRenderClient, (void**)&pRenderClient))) {
		WasapiReleaseStream();
		return 1;
	}

	// Keep the buffer filled up to nAudSegCount segs (or as much as fits)
	nTargetFrames = nAudSegLen * nAudSegCount;
	if (nTargetFrames > nBufferFrames) {
		nTargetFrames = nBufferFrames;
	}
	if (nTargetFrames < (UINT32)nAudSegLen) {
		// WASAPI gave us a buffer smaller than one frame of sound - can't work with that
		WasapiReleaseStream();
		return 1;
	}

	return 0;
}

// Fill the buffer up to the target with silence
static void WasapiPrefill()
{
	UINT32 nPadding = 0;
	BYTE* pData = NULL;

	if (pAudioClient == NULL || pRenderClient == NULL) {
		return;
	}
	if (FAILED(pAudioClient->GetCurrentPadding(&nPadding))) {
		return;
	}

	// leave room for one seg so the first SoundCheck can run a frame immediately
	INT32 nFill = (INT32)nTargetFrames - (INT32)nAudSegLen - (INT32)nPadding;
	if (nFill > 0 && SUCCEEDED(pRenderClient->GetBuffer(nFill, &pData))) {
		pRenderClient->ReleaseBuffer(nFill, AUDCLNT_BUFFERFLAGS_SILENT);
	}
}

static int WasapiStartStream()
{
	if (pAudioClient == NULL) {
		return 1;
	}

	WasapiPrefill();

	if (FAILED(pAudioClient->Start())) {
		return 1;
	}
	bStreamStarted = true;

	return 0;
}

// Default device changed / unplugged etc. Try to reopen on the (new) default device.
static int WasapiReopen()
{
	WasapiReleaseStream();
	ResetEvent(hWasapiEvent);

	if (WasapiOpenStream()) {
		return 1;
	}

	if (bAudPlaying) {
		if (WasapiStartStream()) {
			WasapiReleaseStream();
			return 1;
		}
	}

	return 0;
}

static void WasapiDeviceLost()
{
#ifdef PRINT_DEBUG_INFO
	bprintf(0, _T("*** WASAPI: audio device lost, will try to reopen.\n"));
#endif
	WasapiReleaseStream();
	bDeviceLost = true;
	nLastReopenTry = timeGetTime();
	nLostLastFrame = nLastReopenTry;
}

// Plugin interface

static int WasapiBlankSound()
{
	if (nAudNextSound) {
		AudWriteSilence();
	}

	// Throw away whatever is queued in the device buffer and refill with silence
	if (pAudioClient && pRenderClient) {
		bBlanked = true;
		if (bStreamStarted) {
			pAudioClient->Stop();
			pAudioClient->Reset();
			WasapiPrefill();
			pAudioClient->Start();
		} else {
			pAudioClient->Reset();
		}
	}

	return 0;
}

// Write the seg in nAudNextSound to the device
static HRESULT WasapiWriteSeg()
{
	BYTE* pData = NULL;
	HRESULT hr = pRenderClient->GetBuffer(nAudSegLen, &pData);
	if (FAILED(hr)) {
		return hr;
	}

	WasapiConvert(pData, nAudNextSound, nAudSegLen);

	return pRenderClient->ReleaseBuffer(nAudSegLen, 0);
}

// Called continuously by the run loop: blocks until there's room for at least one
// seg of sound, then runs the emulation for each free seg (drawing only the last one,
// unless bAlwaysDrawFrames) - same pacing model as the DirectSound plugin.
static int WasapiSoundCheck()
{
	if (nAudNextSound == NULL) {
		return 1;
	}

	if (bDeviceLost) {
		DWORD nNow = timeGetTime();

		if (nNow - nLastReopenTry >= 1000) {
			nLastReopenTry = nNow;
			if (WasapiReopen() == 0) {
				bDeviceLost = false;
#ifdef PRINT_DEBUG_INFO
				bprintf(0, _T("*** WASAPI: audio device reopened.\n"));
#endif
				return 0;
			}
		}

		// No device: keep the emulation running at roughly the right speed, without sound
		DWORD nFrameMs = 100000 / (nWasapiFps ? nWasapiFps : 6000);
		if (nNow - nLostLastFrame < nFrameMs) {
			Sleep(1);
			return 0;
		}
		nLostLastFrame += nFrameMs;
		if (nNow - nLostLastFrame > nFrameMs * 4) nLostLastFrame = nNow;	// don't try to catch up forever

		WasapiGetNextSound(true);
		return 0;
	}

	if (pAudioClient == NULL || pRenderClient == NULL) {
		return 1;
	}

	if (!bAudPlaying) {
		return 1;
	}

	UINT32 nPadding = 0;
	HRESULT hr = pAudioClient->GetCurrentPadding(&nPadding);
	if (FAILED(hr)) {
		if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_RESOURCES_INVALIDATED) {
			WasapiDeviceLost();
		}
		return 1;
	}

	INT32 nFree = (INT32)nTargetFrames - (INT32)nPadding;

	if (nFree < nAudSegLen) {
		// Buffer is full enough - wait for the device to consume some
		if (bEventDriven) {
			// Event fires once per device period (~10ms); bail out after a while so the UI stays responsive
			DWORD nWait = WaitForSingleObject(hWasapiEvent, 200);
#ifdef PRINT_DEBUG_INFO
			if (nWait == WAIT_TIMEOUT) {
				bprintf(0, _T("*** WASAPI playback notification timeout.\n"));
			}
#else
			(void)nWait;
#endif
		} else {
			Sleep(1);
		}
		return 0;
	}

	INT32 nSegs = nFree / nAudSegLen;

	bBlanked = false;

	for (INT32 i = 0; i < nSegs; i++) {
		// get more sound into nAudNextSound
		WasapiGetNextSound((i == nSegs - 1) || bAlwaysDrawFrames);	// If this is the last seg of sound, draw the graphics (frameskipping)

		if (nAudNextSound == NULL || pRenderClient == NULL) {
			// sound was shut down from inside the callback
			return 1;
		}

		if (nAudDSPModule[WASAPI_PLUGIN] & 1) {
			if (bRunPause)
				AudWriteSilence();
			else
				DspDo(nAudNextSound, nAudSegLen);
		}

		hr = WasapiWriteSeg();
		if (FAILED(hr)) {
			if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_RESOURCES_INVALIDATED) {
				WasapiDeviceLost();
			}
			// AUDCLNT_E_BUFFER_TOO_LARGE: the callback blanked/refilled the buffer, just drop this seg
			return 0;
		}

		if (bBlanked) {
			// buffer was flushed & re-primed from inside the callback (frame step etc.), start over
			bBlanked = false;
			break;
		}
	}

	return 0;
}

static int WasapiExit()
{
	DspExit();

	WasapiReleaseStream();

	if (hWasapiEvent) {
		CloseHandle(hWasapiEvent);
		hWasapiEvent = NULL;
	}

	if (nAudNextSound) {
		free(nAudNextSound);
		nAudNextSound = NULL;
	}

	WasapiGetNextSound = NULL;

	bDeviceLost = false;
	nBufferFrames = 0;
	nTargetFrames = 0;
	szDeviceName[0] = 0;

	return 0;
}

static int WasapiInit()
{
	if (nAudSampleRate[WASAPI_PLUGIN] <= 0) {
		return 1;
	}

	if (FAILED(nCOMInit)) {
		return 1;
	}

	nWasapiFps = nAppVirtualFps;

	// Calculate the Seg Length (round to nearest sample)
	nAudSegLen = (nAudSampleRate[WASAPI_PLUGIN] * 100 + (nWasapiFps >> 1)) / nWasapiFps;
	nAudAllocSegLen = nAudSegLen << 2;

	bDeviceLost = false;
	bStreamStarted = false;

	hWasapiEvent = CreateEvent(NULL, FALSE, FALSE, NULL);			// auto-reset
	if (hWasapiEvent == NULL) {
		return 1;
	}

	if (WasapiOpenStream()) {
		WasapiExit();
		return 1;
	}

	// Note: +2 is a hacky work-around for crashy Midway games (mk2, etc) via Kaillera (same as DirectSound)
	nAudNextSound = (INT16*)malloc(nAudSegLen << (2 + 2));		// The next sound block to put in the stream
	if (nAudNextSound == NULL) {
		WasapiExit();
		return 1;
	}
	AudWriteSilence();

	WasapiSetCallback(NULL);

	DspInit();

	return 0;
}

static int WasapiPlay()
{
	if (bDeviceLost) {
		// will be started when the device comes back
		WasapiSetVolume();
		return 0;
	}

	if (pAudioClient == NULL) {
		return 1;
	}

	if (nAudNextSound) {
		AudWriteSilence();
	}

	WasapiSetVolume();

	if (bStreamStarted) {
		pAudioClient->Stop();
		bStreamStarted = false;
	}
	pAudioClient->Reset();

	if (WasapiStartStream()) {
		return 1;
	}

	return 0;
}

static int WasapiStop()
{
	if (!bAudOkay) {
		return 1;
	}

	if (pAudioClient && bStreamStarted) {
		pAudioClient->Stop();
		pAudioClient->Reset();
	}
	bStreamStarted = false;

	return 0;
}

static int WasapiSetVolume()
{
	// Same curve as the XAudio2 plugin, volume > 100% is allowed (samples are clipped)
	if (nAudVolume == 10000) {
		fWasapiVol = 1.0f;
	} else if (nAudVolume <= 0) {
		fWasapiVol = 0.0f;
	} else {
		fWasapiVol = (float)(((double)nAudVolume / 10000) * ((double)nAudVolume / 10000));
	}

	return 1; // 1 succeeds here (as XAudio2).
}

static int WasapiGetSettings(InterfaceInfo* pInfo)
{
	TCHAR szString[MAX_PATH] = _T("");

	if (bDeviceLost) {
		_sntprintf(szString, MAX_PATH, _T("Audio device lost, waiting for it to return"));
		IntInfoAddStringModule(pInfo, szString);
		return 0;
	}

	if (szDeviceName[0]) {
		_sntprintf(szString, MAX_PATH, _T("Device: %s"), szDeviceName);
		IntInfoAddStringModule(pInfo, szString);
	}

	_sntprintf(szString, MAX_PATH, _T("Shared mode, %s, %s"),
		bEventDriven ? _T("event driven") : _T("polled"),
		bAutoConvert ? _T("converted by Windows") : _T("converted to device mix format"));
	IntInfoAddStringModule(pInfo, szString);

	if (nAudSampleRate[WASAPI_PLUGIN] > 0) {
		_sntprintf(szString, MAX_PATH, _T("Audio is delayed by approx. %ims (device buffer %ims)"),
			(int)((INT64)nTargetFrames * 1000 / nAudSampleRate[WASAPI_PLUGIN]),
			(int)((INT64)nBufferFrames * 1000 / nAudSampleRate[WASAPI_PLUGIN]));
		IntInfoAddStringModule(pInfo, szString);
	}

	return 0;
}

struct AudOut AudOutWASAPI = { WasapiBlankSound, WasapiSoundCheck, WasapiInit, WasapiSetCallback, WasapiPlay, WasapiStop, WasapiExit, WasapiSetVolume, WasapiGetSettings, _T("WASAPI audio output") };
