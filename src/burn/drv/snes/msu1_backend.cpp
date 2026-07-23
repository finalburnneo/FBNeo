// =============================================================================
//  FBNeo SNES  -  MSU-1 media backend
// =============================================================================

#include "burnint.h"
#include "msu1.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef _MSC_VER
#include "dirent.h"
#else
#include <dirent.h>
#endif

#include "dr_flac.h"
#include "dr_mp3.h"
#include "dr_wav.h"
extern "C" int stb_vorbis_decode_memory(const unsigned char* mem, int len, int* channels, int* sample_rate, short** output);

#if defined(BUILD_WIN32) || defined(__LIBRETRO__) || defined(__x86_64__) || defined(__i386__)
#define MSU1_USE_R8B_RESAMPLE
#endif
#ifdef MSU1_USE_R8B_RESAMPLE
#include <new>
#include "CDSPResampler.h"
#endif

char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, INT32 nOutSize);

//======================
//  game short name
//======================
static char s_gameName[128]   = { 0 };
static char s_parentName[128] = { 0 };

void snes_msu1_backend_setGame(const char* shortName, const char* parentName)
{
	if (shortName == NULL) { s_gameName[0] = 0; }
	else { strncpy(s_gameName, shortName, sizeof(s_gameName) - 1); s_gameName[sizeof(s_gameName) - 1] = 0; }

	if (parentName == NULL) { s_parentName[0] = 0; }
	else { strncpy(s_parentName, parentName, sizeof(s_parentName) - 1); s_parentName[sizeof(s_parentName) - 1] = 0; }
}

static INT32 msu1_dirExists(const char* name)
{
	if (name == NULL || name[0] == 0) return 0;
	char base[MAX_PATH];
	char dir[MAX_PATH];
	snprintf(base, sizeof(base), "%s", TCHARToANSI(szAppSnesMsu1Path, NULL, 0));
	snprintf(dir, sizeof(dir), "%s%s", base, name);
	DIR* dp = opendir(dir);
	if (dp == NULL) return 0;
	closedir(dp);
	return 1;
}

static const char* msu1_effectiveName()
{
	if (msu1_dirExists(s_gameName))   return s_gameName;
	if (msu1_dirExists(s_parentName)) return s_parentName;
	return s_gameName;
}

static void msu1_buildPath(char* dst, INT32 dstLen, const char* leaf)
{
	char base[MAX_PATH];
	snprintf(base, sizeof(base), "%s", TCHARToANSI(szAppSnesMsu1Path, NULL, 0));
	snprintf(dst, dstLen, "%s%s/%s", base, msu1_effectiveName(), leaf);
}

static void msu1_buildDir(char* dst, INT32 dstLen)
{
	char base[MAX_PATH];
	snprintf(base, sizeof(base), "%s", TCHARToANSI(szAppSnesMsu1Path, NULL, 0));
	snprintf(dst, dstLen, "%s%s", base, msu1_effectiveName());
}

//======================
//  filename matching (shared by strict + loose paths)
//======================

static const char* const kAudioExts[] = { "pcm", "flac", "mp3", "wav", "ogg" };
static const INT32       kAudioExtCount = 5;

static void lowerExt(const char* name, char* ext, INT32 extLen)
{
	ext[0] = 0;
	const char* dot = strrchr(name, '.');
	if (dot == NULL || dot[1] == 0) return;
	INT32 j = 0;
	for (const char* p = dot + 1; *p && j < extLen - 1; p++) ext[j++] = (char)tolower((unsigned char)*p);
	ext[j] = 0;
}

static INT32 audioExtIndex(const char* ext)
{
	for (INT32 i = 0; i < kAudioExtCount; i++) if (strcmp(ext, kAudioExts[i]) == 0) return i;
	return -1;
}

static INT32 looseTrackMatch(const char* name, UINT16 track, char* extOut, INT32 extLen)
{
	char ext[16];
	lowerExt(name, ext, sizeof(ext));
	if (audioExtIndex(ext) < 0) return 0;

	const char* dot = strrchr(name, '.');			// guaranteed non-NULL: lowerExt found one
	// walk back over the digit run immediately before the dot
	const char* p = dot;
	if (p == name) return 0;
	const char* digitsEnd = dot;
	const char* d = dot;
	while (d > name && isdigit((unsigned char)d[-1])) d--;
	if (d == digitsEnd) return 0;					// no digits before the dot
	if (d == name || d[-1] != '-') return 0;		// digits must be preceded by '-'

	// parse the decimal number in [d, digitsEnd)
	unsigned long n = 0;
	for (const char* q = d; q < digitsEnd; q++) n = n * 10u + (unsigned long)(*q - '0');
	if (n != (unsigned long)track) return 0;

	snprintf(extOut, extLen, "%s", ext);
	return 1;
}

static INT32 resolveDataPath(char* path, INT32 pathLen)
{
	FILE* fp;

	msu1_buildPath(path, pathLen, "data.rom");
	if ((fp = fopen(path, "rb")) != NULL) { fclose(fp); return 1; }

	msu1_buildPath(path, pathLen, "msu1.data.rom");
	if ((fp = fopen(path, "rb")) != NULL) { fclose(fp); return 1; }

	char dir[MAX_PATH];
	msu1_buildDir(dir, sizeof(dir));
	DIR* dp = opendir(dir);
	if (dp != NULL) {
		struct dirent* de;
		char found[256]; found[0] = 0;
		while ((de = readdir(dp)) != NULL) {
			char ext[16];
			lowerExt(de->d_name, ext, sizeof(ext));
			if (strcmp(ext, "msu") == 0) { snprintf(found, sizeof(found), "%s", de->d_name); break; }
		}
		closedir(dp);
		if (found[0]) { snprintf(path, pathLen, "%s/%s", dir, found); return 1; }
	}

	return 0;
}


//======================
//  streamed FILE* MsuFile  (data.rom + native .pcm)
//======================

typedef struct {
	FILE*  fp;
	UINT32 size;
	UINT32 pos;		// mirror of ftell, so end() is cheap and matches the chip's cursor model
} StreamCtx;

static UINT8 stream_read(void* c)
{
	StreamCtx* s = (StreamCtx*)c;
	if (s->fp == NULL || s->pos >= s->size) return 0x00;
	int ch = fgetc(s->fp);
	if (ch == EOF) return 0x00;
	s->pos++;
	return (UINT8)ch;
}

static void stream_seek(void* c, UINT32 off)
{
	StreamCtx* s = (StreamCtx*)c;
	if (s->fp == NULL) return;
	if (off > s->size) off = s->size;
	fseek(s->fp, (long)off, SEEK_SET);
	s->pos = off;
}

static INT32  stream_end (  void* c) { StreamCtx* s = (StreamCtx*)c; return (s->fp == NULL) || (s->pos >= s->size); }
static UINT32 stream_size(  void* c) { return ((StreamCtx*)c)->size; }
static INT32  stream_isOpen(void* c) { return ((StreamCtx*)c)->fp != NULL; }

static StreamCtx s_dataCtx  = { NULL, 0, 0 };
static StreamCtx s_pcmCtx   = { NULL, 0, 0 };

static void stream_close(StreamCtx* s)
{
	if (s->fp) { fclose(s->fp); s->fp = NULL; }
	s->size = 0;
	s->pos  = 0;
}

static INT32 stream_openInto(const char* path, StreamCtx* ctx, MsuFile* out)
{
	stream_close(ctx);
	FILE* fp = fopen(path, "rb");
	if (fp == NULL) return 0;
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (sz < 0) { fclose(fp); return 0; }
	ctx->fp     = fp;
	ctx->size   = (UINT32)sz;
	ctx->pos    = 0;
	out->ctx    = ctx;
	out->read   = stream_read;
	out->seek   = stream_seek;
	out->end    = stream_end;
	out->size   = stream_size;
	out->isOpen = stream_isOpen;
	return 1;
}

//======================
//  in-memory MsuFile  (decoded flac/mp3/wav/ogg)
//======================

typedef struct {
	UINT8* buf;		// 8-byte MSU1 header + interleaved S16 stereo PCM
	UINT32 size;	// total bytes in buf
	UINT32 pos;
} MemCtx;

static UINT8 mem_read(void* c)
{
	MemCtx* m = (MemCtx*)c;
	if (m->buf == NULL || m->pos >= m->size) return 0x00;
	return m->buf[m->pos++];
}
static void mem_seek(void* c, UINT32 off)
{
	MemCtx* m = (MemCtx*)c;
	if (off > m->size) off = m->size;
	m->pos = off;
}
static INT32  mem_end (  void* c) { MemCtx* m = (MemCtx*)c; return (m->buf == NULL) || (m->pos >= m->size); }
static UINT32 mem_size(  void* c) { return ((MemCtx*)c)->size; }
static INT32  mem_isOpen(void* c) { return ((MemCtx*)c)->buf != NULL; }

static MemCtx s_memCtx = { NULL, 0, 0 };

static void mem_close(MemCtx* m)
{
	if (m->buf) { BurnFree(m->buf); m->buf = NULL; }
	m->size = 0;
	m->pos  = 0;
}

//======================
//  decode helpers
//======================

static UINT32 resampleTo44100_linear(INT16** pcm, UINT32 frames, UINT32 srcRate, INT32 freeWithBurnFree)
{
	INT16* src = *pcm;

	// out frames = frames * 44100 / srcRate  (round)
	UINT64 outFrames64 = ((UINT64)frames * 44100u + srcRate / 2) / srcRate;
	UINT32 outFrames   = (UINT32)outFrames64;
	if (outFrames == 0) outFrames = 1;

	INT16* dst = (INT16*)BurnMalloc((INT32)outFrames * 2 * (INT32)sizeof(INT16));
	if (dst == NULL) return frames;   // OOM: leave source untouched

	// step in 16.16 source-frames per output-frame
	UINT64 step  = ((UINT64)srcRate << 16) / 44100u;
	UINT64 phase = 0;
	for (UINT32 i = 0; i < outFrames; i++) {
		UINT32 idx  = (UINT32)(phase >> 16);
		UINT32 frac = (UINT32)(phase & 0xffff);
		UINT32 idx1 = idx + 1;
		if (idx  >= frames) idx  = frames - 1;
		if (idx1 >= frames) idx1 = frames - 1;
		INT32 l0 = src[idx  * 2 + 0], r0 = src[idx  * 2 + 1];
		INT32 l1 = src[idx1 * 2 + 0], r1 = src[idx1 * 2 + 1];
		dst[i * 2 + 0] = (INT16)(l0 + (((l1 - l0) * (INT32)frac) >> 16));
		dst[i * 2 + 1] = (INT16)(r0 + (((r1 - r0) * (INT32)frac) >> 16));
		phase += step;
	}

	if (freeWithBurnFree) BurnFree(src); else free(src);
	*pcm = dst;
	return outFrames;
}

#ifdef MSU1_USE_R8B_RESAMPLE

static inline INT16 r8b_clampS16(double v)
{
	INT32 i = (INT32)(v >= 0.0 ? v + 0.5 : v - 0.5);
	if (i >  32767) i =  32767;
	if (i < -32768) i = -32768;
	return (INT16)i;
}

#define MSU1_MAX_R8B_BLK 8192
static UINT32 resampleTo44100_r8b(INT16** pcm, UINT32 frames, UINT32 srcRate, INT32 freeWithBurnFree)
{
	INT16* src = *pcm;

	UINT64 outFrames64 = ((UINT64)frames * 44100u + srcRate / 2) / srcRate;
	UINT32 outFrames   = (UINT32)outFrames64;
	if (outFrames == 0) outFrames = 1;

	INT16* dst = (INT16*)BurnMalloc((INT32)outFrames * 2 * (INT32)sizeof(INT16));
	if (dst == NULL) return 0;

	// TransBand 0.5 = highest quality; this is a one-shot offline conversion.
	r8b::CDSPResampler16* rsL = ::new(std::nothrow) r8b::CDSPResampler16((double)srcRate, 44100.0, MSU1_MAX_R8B_BLK, 0.5);
	r8b::CDSPResampler16* rsR = ::new(std::nothrow) r8b::CDSPResampler16((double)srcRate, 44100.0, MSU1_MAX_R8B_BLK, 0.5);
	double* inL  = ::new(std::nothrow) double[MSU1_MAX_R8B_BLK];
	double* inR  = ::new(std::nothrow) double[MSU1_MAX_R8B_BLK];
	if (rsL == NULL || rsR == NULL || inL == NULL || inR == NULL) {
		delete rsL; delete rsR; delete[] inL; delete[] inR;
		BurnFree(dst);
		return 0;
	}

	UINT32 outPos    = 0;
	UINT32 remaining = frames;
	const INT16* p   = src;
	INT32 flushing   = 0;

	while (outPos < outFrames) {
		INT32 blk;
		if (remaining > 0) {
			blk = (INT32)((remaining < (UINT32)MSU1_MAX_R8B_BLK) ? remaining : (UINT32)MSU1_MAX_R8B_BLK);
			for (INT32 i = 0; i < blk; i++) {
				inL[i] = (double)p[i * 2 + 0];
				inR[i] = (double)p[i * 2 + 1];
			}
			p         += (UINT32)blk * 2;
			remaining -= (UINT32)blk;
		} else {
			flushing = 1;					// feed silence to drain filter latency
			blk = MSU1_MAX_R8B_BLK;
			for (INT32 i = 0; i < blk; i++) { inL[i] = 0.0; inR[i] = 0.0; }
		}

		double* opL = NULL;
		double* opR = NULL;
		INT32 gotL  = rsL->process(inL, blk, opL);
		INT32 gotR  = rsR->process(inR, blk, opR);
		INT32 got   = (gotL < gotR) ? gotL : gotR;

		for (INT32 i = 0; i < got && outPos < outFrames; i++, outPos++) {
			dst[outPos * 2 + 0] = r8b_clampS16(opL[i]);
			dst[outPos * 2 + 1] = r8b_clampS16(opR[i]);
		}

		if (flushing && got == 0) break;	// dry: don't spin forever
	}

	for (; outPos < outFrames; outPos++) {	// pad any (rare) shortfall
		dst[outPos * 2 + 0] = 0;
		dst[outPos * 2 + 1] = 0;
	}

	delete rsL; delete rsR; delete[] inL; delete[] inR;
	if (freeWithBurnFree) BurnFree(src); else free(src);
	*pcm = dst;
	return outFrames;
}
#endif // MSU1_USE_R8B_RESAMPLE

static UINT32 resampleTo44100(INT16** pcm, UINT32 frames, UINT32 srcRate, INT32 freeWithBurnFree)
{
	if (srcRate == 44100 || frames == 0 || srcRate == 0) {
#if MSU1_DEBUG
		bprintf(PRINT_IMPORTANT, _T("[MSU1] resample: source %uHz, %u frames -> native 44100, no conversion\n"), srcRate, frames);
#endif
		return frames;
	}

#ifdef MSU1_USE_R8B_RESAMPLE
	UINT32 got = resampleTo44100_r8b(pcm, frames, srcRate, freeWithBurnFree);
	if (got != 0) {
#if MSU1_DEBUG
		bprintf(PRINT_IMPORTANT, _T("[MSU1] resample: %uHz -> 44100 via r8brain (sinc), %u -> %u frames\n"), srcRate, frames, got);
#endif
		return got;
	}
	// else fall through, *pcm still the original
#if MSU1_DEBUG
	bprintf(PRINT_IMPORTANT, _T("[MSU1] resample: r8brain alloc failed, falling back to linear\n"));
#endif
#endif
	UINT32 lin = resampleTo44100_linear(pcm, frames, srcRate, freeWithBurnFree);
#if MSU1_DEBUG
	bprintf(PRINT_IMPORTANT, _T("[MSU1] resample: %uHz -> 44100 via linear, %u -> %u frames\n"), srcRate, frames, lin);
#endif
	return lin;
}

static INT32 mem_buildFromStereoPCM(INT16* pcm, UINT32 frames, INT32 pcmFromBurnFree, MsuFile* out)
{
	mem_close(&s_memCtx);

	UINT32 pcmBytes = frames * 4u;		// 2ch * 2 bytes
	UINT32 total    = 8u + pcmBytes;
	UINT8* buf = (UINT8*)BurnMalloc((INT32)total);
	if (buf == NULL) {
		if (pcmFromBurnFree) BurnFree(pcm); else free(pcm);
		return 0;
	}
	// 8-byte MSU1 header: big-endian "MSU1" magic + little-endian loopSample(=0)
	buf[0] = 'M'; buf[1] = 'S'; buf[2] = 'U'; buf[3] = '1';
	buf[4] = 0; buf[5] = 0; buf[6] = 0; buf[7] = 0;
	memcpy(buf + 8, pcm, pcmBytes);
	if (pcmFromBurnFree) BurnFree(pcm); else free(pcm);

	s_memCtx.buf  = buf;
	s_memCtx.size = total;
	s_memCtx.pos  = 0;
	out->ctx    = &s_memCtx;
	out->read   = mem_read;
	out->seek   = mem_seek;
	out->end    = mem_end;
	out->size   = mem_size;
	out->isOpen = mem_isOpen;
	return 1;
}

static INT16* toStereo(const INT16* src, UINT32 frames, UINT32 channels)
{
	INT16* dst = (INT16*)BurnMalloc((INT32)frames * 2 * (INT32)sizeof(INT16));
	if (dst == NULL) return NULL;
	if (channels == 2) {
		memcpy(dst, src, (size_t)frames * 4u);
	} else if (channels == 1) {
		for (UINT32 i = 0; i < frames; i++) { INT16 s = src[i]; dst[i*2+0] = s; dst[i*2+1] = s; }
	} else {
		// downmix first two channels of an N-channel interleaved buffer
		for (UINT32 i = 0; i < frames; i++) {
			dst[i*2+0] = src[i*channels + 0];
			dst[i*2+1] = src[i*channels + (channels > 1 ? 1 : 0)];
		}
	}
	return dst;
}

static UINT8* slurp(const char* path, UINT32* outLen)
{
	*outLen = 0;
	FILE* fp = fopen(path, "rb");
	if (fp == NULL) return NULL;
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	if (sz <= 0) { fclose(fp); return NULL; }
	UINT8* data = (UINT8*)BurnMalloc((INT32)sz);
	if (data == NULL) { fclose(fp); return NULL; }
	size_t rd = fread(data, 1, (size_t)sz, fp);
	fclose(fp);
	if (rd != (size_t)sz) { BurnFree(data); return NULL; }
	*outLen = (UINT32)sz;
	return data;
}

static INT32 decodeCompressed(const char* path, const char* ext, MsuFile* out)
{
	UINT32 fileLen = 0;
	UINT8* fileBuf = slurp(path, &fileLen);
	if (fileBuf == NULL) return 0;

	INT16*  pcm      = NULL;		// interleaved S16 (channels-wide), decoder-owned or Burn-owned
	UINT32  frames   = 0;
	UINT32  channels = 0;
	UINT32  srcRate  = 0;
	INT32   pcmFromBurnFree = 1;	// whether 'stereo' buffer below is BurnMalloc'd

	if (strcmp(ext, "flac") == 0) {
		drflac* f = drflac_open_memory(fileBuf, fileLen, NULL);
		if (f == NULL) { BurnFree(fileBuf); return 0; }
		channels = f->channels;
		srcRate  = f->sampleRate;
		frames   = (UINT32)f->totalPCMFrameCount;
		INT16* raw = (INT16*)BurnMalloc((INT32)frames * (INT32)channels * (INT32)sizeof(INT16));
		if (raw == NULL) { drflac_close(f); BurnFree(fileBuf); return 0; }
		drflac_read_pcm_frames_s16(f, frames, raw);
		drflac_close(f);
		INT16* stereo = toStereo(raw, frames, channels);
		BurnFree(raw);
		if (stereo == NULL) { BurnFree(fileBuf); return 0; }
		pcm = stereo;
	} else if (strcmp(ext, "mp3") == 0) {
		drmp3 mp3;
		if (!drmp3_init_memory(&mp3, fileBuf, fileLen, NULL)) { BurnFree(fileBuf); return 0; }
		channels = mp3.channels;
		srcRate  = mp3.sampleRate;
		frames   = (UINT32)drmp3_get_pcm_frame_count(&mp3);
		INT16* raw = (INT16*)BurnMalloc((INT32)frames * (INT32)channels * (INT32)sizeof(INT16));
		if (raw == NULL) { drmp3_uninit(&mp3); BurnFree(fileBuf); return 0; }
		drmp3_read_pcm_frames_s16(&mp3, frames, raw);
		drmp3_uninit(&mp3);
		INT16* stereo = toStereo(raw, frames, channels);
		BurnFree(raw);
		if (stereo == NULL) { BurnFree(fileBuf); return 0; }
		pcm = stereo;
	} else if (strcmp(ext, "wav") == 0) {
		drwav wav;
		if (!drwav_init_memory(&wav, fileBuf, fileLen, NULL)) { BurnFree(fileBuf); return 0; }
		channels = wav.channels;
		srcRate  = wav.sampleRate;
		frames   = (UINT32)wav.totalPCMFrameCount;
		INT16* raw = (INT16*)BurnMalloc((INT32)frames * (INT32)channels * (INT32)sizeof(INT16));
		if (raw == NULL) { drwav_uninit(&wav); BurnFree(fileBuf); return 0; }
		drwav_read_pcm_frames_s16(&wav, frames, raw);
		drwav_uninit(&wav);
		INT16* stereo = toStereo(raw, frames, channels);
		BurnFree(raw);
		if (stereo == NULL) { BurnFree(fileBuf); return 0; }
		pcm = stereo;
	} else if (strcmp(ext, "ogg") == 0) {
		int ch = 0, sr = 0;
		short* raw = NULL;
		int n = stb_vorbis_decode_memory(fileBuf, (int)fileLen, &ch, &sr, &raw);
		if (n < 0 || raw == NULL) { BurnFree(fileBuf); return 0; }
		channels = (UINT32)ch;
		srcRate  = (UINT32)sr;
		frames   = (UINT32)n;
		INT16* stereo = toStereo(raw, frames, channels);
		free(raw);                       // stb uses malloc
		if (stereo == NULL) { BurnFree(fileBuf); return 0; }
		pcm = stereo;
	} else {
		BurnFree(fileBuf);
		return 0;
	}

	BurnFree(fileBuf);
	if (pcm == NULL || frames == 0) return 0;

	// Normalise sample rate to 44100 (linear); MSU-1 streams are 44100 native.
	frames = resampleTo44100(&pcm, frames, srcRate, pcmFromBurnFree);

	return mem_buildFromStereoPCM(pcm, frames, 1 /*BurnMalloc*/, out);
}

//======================
//  backend open callbacks (installed via snes_msu1_setBackend)
//======================

static INT32 backend_dataOpen(MsuFile* out)
{
	char path[MAX_PATH];
	if (!resolveDataPath(path, sizeof(path))) return 0;
	return stream_openInto(path, &s_dataCtx, out);
}

static INT32 backend_audioOpen(UINT16 track, MsuFile* out)
{
	char dir[MAX_PATH];
	msu1_buildDir(dir, sizeof(dir));
	DIR* dp = opendir(dir);
	if (dp == NULL) return 0;

	char foundName[256]; foundName[0] = 0;
	char foundExt[16];   foundExt[0]  = 0;
	struct dirent* de;
	while ((de = readdir(dp)) != NULL) {
		char ext[16];
		if (looseTrackMatch(de->d_name, track, ext, sizeof(ext))) {
			snprintf(foundName, sizeof(foundName), "%s", de->d_name);
			snprintf(foundExt,  sizeof(foundExt),  "%s", ext);
			break;		// first match wins
		}
	}
	closedir(dp);

	if (foundName[0]) {
		char path[MAX_PATH];
		snprintf(path, sizeof(path), "%s/%s", dir, foundName);
		if (strcmp(foundExt, "pcm") == 0) {
			// native .pcm  -> streamed (self-contained MSU1 header, byte-seekable)
			if (stream_openInto(path, &s_pcmCtx, out)) { mem_close(&s_memCtx); return 1; }
		} else {
			// compressed / wav  -> decode whole track into RAM (+ synthesised header)
			if (decodeCompressed(path, foundExt, out)) { stream_close(&s_pcmCtx); return 1; }
		}
	}

	return 0;			// no media for this track -> chip flags audioError
}

//======================
//  install / teardown
//======================

void snes_msu1_backend_install()
{
	snes_msu1_setBackend(backend_dataOpen, backend_audioOpen);
}

void snes_msu1_backend_free()
{
	stream_close(&s_dataCtx);
	stream_close(&s_pcmCtx);
	mem_close(&s_memCtx);
	s_gameName[0] = 0;
	s_parentName[0] = 0;
}

INT32 snes_msu1_backend_detect(const char* shortName)
{
	// Probe exactly this name (no parent fallback), then restore prior keys.
	char savedGame[128], savedParent[128];
	strncpy(savedGame,   s_gameName,   sizeof(savedGame)   - 1); savedGame[sizeof(savedGame)     - 1] = 0;
	strncpy(savedParent, s_parentName, sizeof(savedParent) - 1); savedParent[sizeof(savedParent) - 1] = 0;
	snes_msu1_backend_setGame(shortName, NULL);

	INT32 found = 0;

	// 1) any recognised data ROM name
	char path[MAX_PATH];
	if (resolveDataPath(path, sizeof(path))) found = 1;

	// 2) any audio track: scan for a filename ending in "-<digits>.<audioExt>"
	if (!found) {
		char dir[MAX_PATH];
		msu1_buildDir(dir, sizeof(dir));
		DIR* dp = opendir(dir);
		if (dp != NULL) {
			struct dirent* de;
			while (!found && (de = readdir(dp)) != NULL) {
				char ext[16];
				lowerExt(de->d_name, ext, sizeof(ext));
				if (audioExtIndex(ext) < 0) continue;
				// require a trailing "-<digits>" before the dot
				const char* dot = strrchr(de->d_name, '.');
				const char* d = dot;
				while (d > de->d_name && isdigit((unsigned char)d[-1])) d--;
				if (d != dot && d > de->d_name && d[-1] == '-') found = 1;
			}
			closedir(dp);
		}
	}

	// restore prior game keys (detection must be side-effect free)
	snes_msu1_backend_setGame(savedGame[0] ? savedGame : NULL,
	                          savedParent[0] ? savedParent : NULL);
	return found;
}
