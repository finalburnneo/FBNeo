// =============================================================================
//  MSU-1 media coprocessor for FBNeo
// =============================================================================

#include "msu1.h"
#include <string.h>

//======================
// media backend
//======================

static MsuDataOpen   s_dataOpenCb  = NULL;
static MsuAudioOpen  s_audioOpenCb = NULL;

static MsuFile s_dataFile;
static MsuFile s_audioFile;

static UINT8  closed_read( void* /*ctx*/)				{ return 0x00; }
static void   closed_seek( void* /*ctx*/, UINT32 /*o*/)	{              }
static INT32  closed_end ( void* /*ctx*/)				{ return 1;    }
static UINT32 closed_size( void* /*ctx*/)				{ return 0;    }
static INT32  closed_isOpen(void* /*ctx*/)				{ return 0;    }

static void msu1_closeFile(MsuFile* f)
{
	f->ctx    = NULL;
	f->read   = closed_read;
	f->seek   = closed_seek;
	f->end    = closed_end;
	f->size   = closed_size;
	f->isOpen = closed_isOpen;
}

//======================
// registers / state
//======================

// $2000 status flags (bit positions match the hardware read of $2000).
enum {
	FLAG_REVISION       = 0x02,
	FLAG_AUDIO_ERROR    = 0x08,
	FLAG_AUDIO_PLAYING  = 0x10,
	FLAG_AUDIO_REPEATING= 0x20,
	FLAG_AUDIO_BUSY     = 0x40,
	FLAG_DATA_BUSY      = 0x80
};

static UINT32 io_dataSeekOffset;	// $2000-$2003 write latch (seek target)
static UINT32 io_dataReadOffset;	// current data-ROM read cursor

static UINT32 io_audioPlayOffset;	// current byte offset into the PCM track
static UINT32 io_audioLoopOffset;	// loop point (byte offset), from track header

static UINT16 io_audioTrack;		// $2004-$2005 selected track number
static UINT8  io_audioVolume;		// $2006 playback volume (0-255)

static UINT32 io_audioResumeTrack;	// track saved for resume; ~0 = none pending
static UINT32 io_audioResumeOffset;	// byte offset saved for resume

static UINT8  io_audioError;		// media / header error on last track open
static UINT8  io_audioPlay;			// audio channel currently playing
static UINT8  io_audioRepeat;		// loop at end instead of stopping
static UINT8  io_audioBusy;			// audio subsystem busy (unused: always 0 here)
static UINT8  io_dataBusy;			// data subsystem busy (unused: always 0 here)

static UINT32 s_mixPhase;			// 16.16 fractional native-sample position
static INT16  s_mixCurL;			// current native MSU frame (phase integer part)
static INT16  s_mixCurR;
static INT16  s_mixNextL;			// following native frame, for interpolation
static INT16  s_mixNextR;
static INT32  s_mixPrimed;			// 0 until the first Cur/Next pair is loaded
#if MSU1_DEBUG
static INT32  s_msuAudiblePrev;		// last reported "MSU track audible" state (-1 = unknown)
static INT32  s_msuLevelFrames;		// frame counter for periodic mixed-level debug report
#endif

//======================
// media open helpers
//======================

static void msu1_dataOpen()
{
	msu1_closeFile(&s_dataFile);
	if (s_dataOpenCb && s_dataOpenCb(&s_dataFile) && s_dataFile.isOpen(s_dataFile.ctx)) {
		s_dataFile.seek(s_dataFile.ctx, io_dataReadOffset);
	} else {
		msu1_closeFile(&s_dataFile);
	}
}

static void msu1_audioOpen()
{
	msu1_closeFile(&s_audioFile);

	s_mixPrimed = 0;

	if (s_audioOpenCb && s_audioOpenCb(io_audioTrack, &s_audioFile) && s_audioFile.isOpen(s_audioFile.ctx)) {
		if (s_audioFile.size(s_audioFile.ctx) >= 8) {
			s_audioFile.seek(s_audioFile.ctx, 0);

			UINT32 header = 0;
			header  = (UINT32)s_audioFile.read(s_audioFile.ctx) << 24;
			header |= (UINT32)s_audioFile.read(s_audioFile.ctx) << 16;
			header |= (UINT32)s_audioFile.read(s_audioFile.ctx) << 8;
			header |= (UINT32)s_audioFile.read(s_audioFile.ctx);
			if (header == 0x4d535531) {		// "MSU1"
				UINT32 loopSample = 0;
				loopSample  = (UINT32)s_audioFile.read(s_audioFile.ctx);
				loopSample |= (UINT32)s_audioFile.read(s_audioFile.ctx) << 8;
				loopSample |= (UINT32)s_audioFile.read(s_audioFile.ctx) << 16;
				loopSample |= (UINT32)s_audioFile.read(s_audioFile.ctx) << 24;
				io_audioLoopOffset = 8 + loopSample * 4;
				if (io_audioLoopOffset > s_audioFile.size(s_audioFile.ctx)) io_audioLoopOffset = 8;
				io_audioError = 0;
				s_audioFile.seek(s_audioFile.ctx, io_audioPlayOffset);
				return;
			}
		}
		msu1_closeFile(&s_audioFile);
	}
	io_audioError = 1;
}

//======================
// bus I/O  ($2000-$2007)
//======================

UINT8 snes_msu1_read(UINT32 address, UINT8 openbus)
{
	switch (0x2000 | (address & 7)) {
		case 0x2000: {
			UINT8 data = 0;
			data |= (FLAG_REVISION & 0x07);
			if (io_audioError)  data |= FLAG_AUDIO_ERROR;
			if (io_audioPlay)   data |= FLAG_AUDIO_PLAYING;
			if (io_audioRepeat) data |= FLAG_AUDIO_REPEATING;
			if (io_audioBusy)   data |= FLAG_AUDIO_BUSY;
			if (io_dataBusy)    data |= FLAG_DATA_BUSY;
			return data;
		}
		case 0x2001:
			if (io_dataBusy) return 0x00;
			if (!s_dataFile.isOpen(s_dataFile.ctx)) return 0x00;
			if (s_dataFile.end(s_dataFile.ctx)) return 0x00;
			io_dataReadOffset++;
			return s_dataFile.read(s_dataFile.ctx);
		case 0x2002: return 'S';
		case 0x2003: return '-';
		case 0x2004: return 'M';
		case 0x2005: return 'S';
		case 0x2006: return 'U';
		case 0x2007: return '1';
	}

	return openbus;		// unreachable
}

void snes_msu1_write(UINT32 address, UINT8 data)
{
	switch (0x2000 | (address & 7)) {
		case 0x2000:
			io_dataSeekOffset = (io_dataSeekOffset & 0xffffff00u) | ((UINT32)data << 0);
			break;
		case 0x2001:
			io_dataSeekOffset = (io_dataSeekOffset & 0xffff00ffu) | ((UINT32)data << 8);
			break;
		case 0x2002:
			io_dataSeekOffset = (io_dataSeekOffset & 0xff00ffffu) | ((UINT32)data << 16);
			break;
		case 0x2003:
			io_dataSeekOffset = (io_dataSeekOffset & 0x00ffffffu) | ((UINT32)data << 24);
			io_dataReadOffset = io_dataSeekOffset;
			if (s_dataFile.isOpen(s_dataFile.ctx)) s_dataFile.seek(s_dataFile.ctx, io_dataReadOffset);
			break;
		case 0x2004:
			io_audioTrack = (io_audioTrack & 0xff00) | ((UINT16)data << 0);
			break;
		case 0x2005:
			io_audioTrack = (io_audioTrack & 0x00ff) | ((UINT16)data << 8);
			io_audioPlay   = 0;
			io_audioRepeat = 0;
			io_audioPlayOffset = 8;
			if (io_audioTrack == io_audioResumeTrack) {
				io_audioPlayOffset  = io_audioResumeOffset;
				io_audioResumeTrack = ~0u;		// consume the pending resume
				io_audioResumeOffset = 0;
			}
			msu1_audioOpen();
			break;
		case 0x2006:
			io_audioVolume = data;
			break;
		case 0x2007:
			if (io_audioBusy)  break;
			if (io_audioError) break;
			io_audioPlay   = (data & 0x01) ? 1 : 0;
			io_audioRepeat = (data & 0x02) ? 1 : 0;
			if (!io_audioPlay && (data & 0x04)) {
				io_audioResumeTrack  = io_audioTrack;
				io_audioResumeOffset = io_audioPlayOffset;
			}
			break;
	}
}

//======================
// audio streaming / mix
//======================

static INT16 clamp16(INT32 v)
{
	if (v >  32767) return  32767;
	if (v < -32768) return -32768;
	return (INT16)v;
}

static void msu1_nextFrame(INT16* outL, INT16* outR, INT32 dspMuted)
{
	INT32 left = 0, right = 0;

	if (io_audioPlay) {
		if (s_audioFile.isOpen(s_audioFile.ctx)) {
			if (s_audioFile.end(s_audioFile.ctx)) {
				if (!io_audioRepeat) {
					io_audioPlay = 0;
					io_audioPlayOffset = 8;
					s_audioFile.seek(s_audioFile.ctx, io_audioPlayOffset);
				} else {
					io_audioPlayOffset = io_audioLoopOffset;
					s_audioFile.seek(s_audioFile.ctx, io_audioPlayOffset);
				}
			} else {
				io_audioPlayOffset += 4;
				// signed 16-bit little-endian, L then R
				UINT16 lo = s_audioFile.read(s_audioFile.ctx);
				UINT16 hi = s_audioFile.read(s_audioFile.ctx);
				INT16 l = (INT16)(lo | (hi << 8));
				lo = s_audioFile.read(s_audioFile.ctx);
				hi = s_audioFile.read(s_audioFile.ctx);
				INT16 r = (INT16)(lo | (hi << 8));
				left  = (INT32)((double)l * (double)io_audioVolume / 255.0);
				right = (INT32)((double)r * (double)io_audioVolume / 255.0);
			}
		} else {
			io_audioPlay = 0;
		}
	}

	if (dspMuted) { left = 0; right = 0; }
	*outL = clamp16(left);
	*outR = clamp16(right);
}

void snes_msu1_mixSamples(INT16* out, INT32 samples, INT32 outRate, INT32 dspMuted)
{
	if (out == NULL || samples <= 0) return;
	if (outRate <= 0) outRate = 44100;

	const UINT32 step = (UINT32)(((UINT64)44100 << 16) / (UINT32)outRate);

	if (!s_mixPrimed) {
		msu1_nextFrame(&s_mixCurL,  &s_mixCurR,  dspMuted);
		msu1_nextFrame(&s_mixNextL, &s_mixNextR, dspMuted);
		s_mixPhase  = 0;
		s_mixPrimed = 1;
#if MSU1_DEBUG
		bprintf(0, _T("[MSU1] mix: 44100 -> %dHz, %s (step=0x%05x)\n"),
			outRate, (outRate == 44100) ? _T("1:1") : _T("linear"), step);
#endif
	}

#if MSU1_DEBUG
	// Edge-triggered: is MSU-1 external audio actually playing (vs built-in only)?
	INT32 msuAudible = (io_audioPlay && !io_audioError) ? 1 : 0;
	if (msuAudible != s_msuAudiblePrev) {
		if (msuAudible)
			bprintf(PRINT_IMPORTANT, _T("[MSU1] >>> track %d PLAYING (over SNES audio)\n"), (INT32)io_audioTrack);
		else
			bprintf(PRINT_IMPORTANT, _T("[MSU1] <<< track stopped -- SNES built-in audio only\n"));
		s_msuAudiblePrev = msuAudible;
	}
	INT32 msuPeak = 0;		// peak MSU level mixed in this call
#endif

	for (INT32 i = 0; i < samples; i++) {
		UINT32 advance = s_mixPhase >> 16;
		s_mixPhase &= 0xffff;
		for (UINT32 a = 0; a < advance; a++) {
			s_mixCurL = s_mixNextL;
			s_mixCurR = s_mixNextR;
			msu1_nextFrame(&s_mixNextL, &s_mixNextR, dspMuted);
		}
		INT32 frac = (INT32)s_mixPhase;
		INT32 msuL = (INT32)s_mixCurL + (INT32)((((INT64)s_mixNextL - (INT64)s_mixCurL) * frac) >> 16);
		INT32 msuR = (INT32)s_mixCurR + (INT32)((((INT64)s_mixNextR - (INT64)s_mixCurR) * frac) >> 16);

#if MSU1_DEBUG
		INT32 aL = msuL < 0 ? -msuL : msuL;
		INT32 aR = msuR < 0 ? -msuR : msuR;
		if (aL > msuPeak) msuPeak = aL;
		if (aR > msuPeak) msuPeak = aR;
#endif

		INT32 l = (INT32)out[i * 2 + 0] + msuL;
		INT32 r = (INT32)out[i * 2 + 1] + msuR;
		out[i * 2 + 0] = clamp16(l);
		out[i * 2 + 1] = clamp16(r);
		s_mixPhase += step;
	}

#if MSU1_DEBUG
	// ~once per second while a track plays: peak>0 proves audio reached the buffer;
	// peak==0 with play==1 means opened-but-silent (check volume / track data).
	if (io_audioPlay && !io_audioError) {
		if (++s_msuLevelFrames >= 60) {
			bprintf(PRINT_IMPORTANT, _T("[MSU1] track %d: peak=%d/32767, volume=%d/255\n"),
				(INT32)io_audioTrack, msuPeak, (INT32)io_audioVolume);
			s_msuLevelFrames = 0;
		}
	} else {
		s_msuLevelFrames = 0;
	}
#endif
}

//======================
// backend install / lifecycle
//======================

void snes_msu1_setBackend(MsuDataOpen dataOpen, MsuAudioOpen audioOpen)
{
	s_dataOpenCb  = dataOpen;
	s_audioOpenCb = audioOpen;
}

void snes_msu1_init()
{
	msu1_closeFile(&s_dataFile);
	msu1_closeFile(&s_audioFile);
}

void snes_msu1_reset()
{
	io_dataSeekOffset = 0;
	io_dataReadOffset = 0;

	io_audioPlayOffset = 0;
	io_audioLoopOffset = 0;

	io_audioTrack  = 0;
	io_audioVolume = 0;

	io_audioResumeTrack  = ~0u;		// no resume pending
	io_audioResumeOffset = 0;

	io_audioError  = 0;
	io_audioPlay   = 0;
	io_audioRepeat = 0;
	io_audioBusy   = 0;
	io_dataBusy    = 0;

	s_mixPhase = 0;
	s_mixCurL  = 0;
	s_mixCurR  = 0;
	s_mixNextL = 0;
	s_mixNextR = 0;
	s_mixPrimed = 0;
#if MSU1_DEBUG
	s_msuAudiblePrev = -1;			// force a report on the first mix after reset
	s_msuLevelFrames = 0;
#endif

	msu1_dataOpen();
	msu1_audioOpen();
}

void snes_msu1_exit()
{
	msu1_closeFile(&s_dataFile);
	msu1_closeFile(&s_audioFile);
}

void snes_msu1_handleState(StateHandler* sh)
{
	sh_handleInts(sh,
		&io_dataSeekOffset, &io_dataReadOffset,
		&io_audioPlayOffset, &io_audioLoopOffset,
		&io_audioResumeTrack, &io_audioResumeOffset, NULL);
	sh_handleWords(sh, &io_audioTrack, NULL);
	sh_handleBytes(sh,
		&io_audioVolume,
		&io_audioError, &io_audioPlay, &io_audioRepeat, &io_audioBusy, &io_dataBusy, NULL);

	if (!sh->saving) {
		msu1_dataOpen();
		msu1_audioOpen();
		if (s_dataFile.isOpen(s_dataFile.ctx))  s_dataFile.seek(s_dataFile.ctx, io_dataReadOffset);
		if (s_audioFile.isOpen(s_audioFile.ctx)) s_audioFile.seek(s_audioFile.ctx, io_audioPlayOffset);
	}
}
