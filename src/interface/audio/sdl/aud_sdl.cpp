// SDL_Sound module

#ifdef BUILD_SDL

//#include "burner.h"
#include "aud_dsp.h"

class AudioSDL : public Audio {
public:
	SDL_AudioSpec SDLAudioSpec;

	short* SDLAudBuffer;
	int nSDLPlayPos;
	int nSDLFillSeg;

	int (*SDLGetNextSound)(int);

	int set(int (*callback)(int))
	{
		if (callback == NULL) {
			SDLGetNextSound = AudWriteSlience;
		} else {
			SDLGetNextSound = callback;
			dprintf(_T("SDL callback set\n"));
		}
		return 0;
	}

	int blank()
	{
		dprintf (_T("SDLBlankSound\n"));

		if (SDLAudBuffer) {
			memset(SDLAudBuffer, 0, loopLen);
		}

		if (pAudNextSound) {
			dprintf (_T("blanking pAudNextSound\n"));
			AudWriteSlience();
		}
		return 0;
	}

	int check()
	{
		#define WRAP_INC(x) { x++; if (x >= nAudSegCount) x = 0; }

		// Since the SDL buffer is smaller than a segment,
		// only fill the buffer up to the start of the currently playing segment
		int nPlaySeg = nSDLPlayPos / nAudAllocSegLen - 1;

		if (nPlaySeg >= nAudSegCount) {
			nPlaySeg -= nAudSegCount;
		}
		if (nPlaySeg < 0) {
			nPlaySeg = nAudSegCount - 1;
		}

	//	dprintf(_T("SDLSoundCheck (seg %i)\n"), nPlaySeg);

		if (nSDLFillSeg == nPlaySeg) {
			SDL_Delay(1);
			return 0;
		}

		// work out which seg we will fill next
		int nFollowingSeg = nSDLFillSeg;
		WRAP_INC(nFollowingSeg);

		while (nSDLFillSeg != nPlaySeg) {
			// fill nSDLFillSeg
	//		dprintf(_T("Filling seg %i at %i\n"), nSDLFillSeg, nSDLFillSeg * (nAudAllocSegLen));

			memcpy((char*)SDLAudBuffer + nSDLFillSeg * nAudAllocSegLen, pAudNextSound, nAudAllocSegLen);

			// If this is the last seg of sound, flag bDraw (to draw the graphics)
			int bDraw = (nFollowingSeg == nPlaySeg);// || !autoFrameSkip;

	//		pAudNextSound = SDLAudBuffer + nSDLFillSeg * (nAudSegLen << 1);
			SDLGetNextSound(bDraw);					// get more sound into pAudNextSound

			if (nAudDSPModule & 1) {
				if (bRunPause)
					AudWriteSlience();
				else
					DspDo(pAudNextSound, nAudSegLen);
			}

			nSDLFillSeg = nFollowingSeg;
			WRAP_INC(nFollowingSeg);
		}

		return 0;
	}

	int exit()
	{
		dprintf(_T("SDLSoundExit\n"));

		DspExit();

		SDL_CloseAudio();

		free(SDLAudBuffer);
		SDLAudBuffer = NULL;

		free(pAudNextSound);
		pAudNextSound = NULL;

		return 0;
	}

	void audiospec_callback(void* /* data */, Uint8* stream, int len)
	{
	//	dprintf(_T("audiospec_callback %i"), len);

		int end = nSDLPlayPos + len;
		if (end > loopLen) {
	//		dprintf(_T(" %i - %i"), nSDLPlayPos, nSDLPlayPos + loopLen - nSDLPlayPos);

			SDL_MixAudio(stream, (Uint8*)SDLAudBuffer + nSDLPlayPos, loopLen - nSDLPlayPos, volume);
			end -= loopLen;

	//		dprintf(_T(", %i - %i (%i)"), 0, end, loopLen - nSDLPlayPos + end);

			SDL_MixAudio(stream + loopLen - nSDLPlayPos, (Uint8*)SDLAudBuffer, end, volume);
			nSDLPlayPos = end;
		} else {
			SDL_MixAudio(stream, (Uint8*)SDLAudBuffer + nSDLPlayPos, len, volume);
			nSDLPlayPos = end;

			if (nSDLPlayPos == loopLen) {
				nSDLPlayPos = 0;
			}
		}

	//	dprintf(_T("\n"));
	}

	int init()
	{
		dprintf(_T("SDLSoundInit (%dHz)"), nAudSampleRate);

		if (nAudSampleRate <= 0) {
			return 1;
		}

		fps = nAppVirtualFps;
		nAudSegLen = (nAudSampleRate * 100 + (fps >> 1)) / fps;
		nAudAllocSegLen = nAudSegLen << 2;
		loopLen = (nAudSegLen * nAudSegCount) << 2;

		int nSDLBufferSize;
		for (nSDLBufferSize = 64; nSDLBufferSize < (nAudSegLen >> 1); nSDLBufferSize <<= 1) { }

		SDL_AudioSpec audiospec_req;
		audiospec_req.freq = nAudSampleRate;
		audiospec_req.format = AUDIO_S16;
		audiospec_req.channels = 2;
		audiospec_req.samples = nSDLBufferSize;
		audiospec_req.callback = audiospec_callback;

		SDLAudBuffer = (short*)malloc(loopLen);
		if (SDLAudBuffer == NULL) {
			dprintf(_T("Couldn't malloc SDLAudBuffer\n"));
			exit();
			return 1;
		}
		memset(SDLAudBuffer, 0, loopLen);

		pAudNextSound = (short*)malloc(nAudAllocSegLen);
		if (pAudNextSound == NULL) {
			exit();
			return 1;
		}

		nSDLPlayPos = 0;
		nSDLFillSeg = nAudSegCount - 1;

		if (SDL_OpenAudio(&audiospec_req, &SDLAudioSpec)) {
			fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
			dprintf(_T("Couldn't open audio: %s\n"), SDL_GetError());
			return 1;
		}

		set(NULL);

		DspInit();

		return 0;
	}

	int play()
	{
		dprintf(_T("SDLSoundPlay\n"));

		SDL_PauseAudio(0);

		return 0;
	}

	int stop()
	{
		dprintf(_T("SDLSoundStop\n"));

		SDL_PauseAudio(1);

		return 0;
	}

	int setvolume(int vol)
	{
		dprintf(_T("SDLSoundSetVolume\n"));

		if (vol == 10000) {
			volume = SDL_MIX_MAXVOLUME;
		} else {
			if (vol == 0) {
				volume = 0;
			} else {
				volume = SDL_MIX_MAXVOLUME * vol / 10000;
			}
		}

		if (volume < 0) {
			volume = 0;
		}

		return 0;
	}

	int get(void* info)
	{
		return 0;
	}

	int setfps()
	{
		if (nAudSampleRate <= 0) {
			return 0;
		}

		return 0;
	}

	AudioSDL() {
		SDLAudBuffer = NULL;
		nSDLPlayPos = 0;
		nSDLFillSeg = 0;

		loopLen = 0;
		fps = 0;
		volume = SDL_MIX_MAXVOLUME;
	}

	~AudioSDL() {
		exit();
	}
};

#endif
