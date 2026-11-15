// burn_ost: replace a game's soundtrack with external audio files (via BurnSample)
// see burn_ost.h and support/ost/example.ost

#include "burnint.h"
#include "samples.h"
#include "burn_ost.h"
#include <ctype.h>

INT32 bBurnOstEnable = 1;
INT32 bBurnOstLog = 0;

char* TCHARToANSI(const TCHAR* pszInString, char* pszOutString, INT32 nOutSize);

#define OST_MAX_TRACKS	256
#define OST_MAX_RULES	512
#define OST_MAX_FLAGS	32

enum { ACT_PLAY = 0, ACT_STOP, ACT_FADE, ACT_PASS, ACT_PAUSE, ACT_RESUME, ACT_VOLUME };
enum { THEN_STOP = 0, THEN_RESUME, THEN_NATIVE, THEN_CODE };
enum { FWD_DEFAULT = 0, FWD_DROP, FWD_ORIG, FWD_CODE };

struct OstTime {
	double sec;			// seconds, or
	INT32 smp;			// samples at the file's own rate (-1 = use sec)
};

struct OstRule {
	UINT32 code;
	INT32 action;
	INT32 track;			// ACT_PLAY
	INT32 loop;
	OstTime loop_at;
	OstTime start;
	INT32 then;
	UINT32 then_code;
	INT32 restart;
	INT32 if_playing, if_not_playing;	// track, -1 = n/a
	UINT32 if_flags, if_not_flags, set_flags, clear_flags;
	double fadein, fadeout, xfade, time;	// seconds ("time": ACT_FADE / ACT_VOLUME duration)
	double volume;			// 0.0 - 1.0 (ACT_PLAY: track volume, ACT_VOLUME: music volume)
	INT32 speed;			// %
	INT32 forward;
	UINT32 forward_code;
};

struct OstVoice {			// a playing (or paused) track
	INT32 track;			// sample #, -1 = none
	INT32 rule;
	double fade, fade_target, fade_step;	// fade in/out, per frame
	INT32 stop_at_target;		// stop the track when fade reaches target
	INT32 fading_end;		// "fadeout=" at the end of a track has started
	UINT32 paused_pos;
};

// --- pack (from the .ost file) ---
static INT32 bOstActive = 0;
static INT32 bOstSamples = 0;		// BurnSample in use (a pack with no tracks only logs)
static char  szTrack[OST_MAX_TRACKS][64];
static INT32 nTracks = 0;
static OstRule Rules[OST_MAX_RULES];
static INT32 nRules = 0;
static char  szFlag[OST_MAX_FLAGS][32];
static INT32 nFlags = 0;
static char  szArchive[MAX_PATH * 2];
static char  szFolder[MAX_PATH * 2];
static BurnSampleSource OstSource;
static void  (*pOstSend)(UINT32) = NULL;

static double fOptVolume = 1.0;
static INT32  nOptSilence = -1;		// default replacement for a replaced command, -1 = drop
static INT32  bOptTriggerChange = 1;	// repeated writes of the same value aren't new commands
static INT32  nOptFollowFade = 0;	// 0 off, 1 gain = latch / 255, -1 gain = 1 - latch / 255
static INT32  bOptPreload = 0;

// --- state (scanned) ---
static OstVoice Cur, Out, Prev;		// current, fading out (crossfade), paused for "then=resume"
static UINT32 nFlagBits = 0;
static double fMusicVolume = 1.0, fMusicVolumeTarget = 1.0, fMusicVolumeStep = 0.0;
static double fLatchVolume = 1.0;
static INT32  nLastIn = -1, nLastOut = -1;
static INT32  nPendingSend = -1;	// "then=native"/"then=<code>": sent from BurnOstFrame()

// ---------------------------------------------------------------------------
// .ost parsing

static const char *OstGetName(INT32 i) { return szTrack[i]; }
static INT32 OstGetFlags(INT32 /*i*/) { return bOptPreload ? SAMPLE_NOLOOP /* any non-zero flag: "stored" */ : SAMPLE_NOSTOREF; }

static INT32 OstTrackIndex(const char *name, INT32 bAdd)
{
	for (INT32 i = 0; i < nTracks; i++) {
		if (strcmp(szTrack[i], name) == 0) return i;
	}
	if (!bAdd || nTracks >= OST_MAX_TRACKS) return -1;

	snprintf(szTrack[nTracks], sizeof(szTrack[0]), "%s", name);
	return nTracks++;
}

static UINT32 OstFlagBit(const char *name)
{
	for (INT32 i = 0; i < nFlags; i++) {
		if (strcmp(szFlag[i], name) == 0) return 1 << i;
	}
	if (nFlags >= OST_MAX_FLAGS) return 0;

	snprintf(szFlag[nFlags], sizeof(szFlag[0]), "%s", name);
	return 1 << nFlags++;
}

static OstTime OstParseTime(const char *s)	// "7.415", "1:23.5", "327168smp"
{
	OstTime t = { 0.0, -1 };

	if (strstr(s, "smp")) {
		t.smp = atoi(s);
	} else if (strchr(s, ':')) {
		t.sec = atoi(s) * 60.0 + atof(strchr(s, ':') + 1);
	} else {
		t.sec = atof(s);
	}

	return t;
}

static UINT32 OstTimeToFrames(OstTime t, INT32 track)
{
	if (t.smp >= 0) {
		UINT32 rate = BurnSampleGetSourceRate(track);
		return rate ? (UINT32)((UINT64)t.smp * nBurnSoundRate / rate) : 0;
	}

	return (UINT32)(t.sec * nBurnSoundRate);
}

static INT32 OstYes(const char *v) { return v[0] == 'y' || v[0] == '1' || v[0] == 'o'; }

static void OstParseOption(char *key, char *val)
{
	static const char *known[] = { "volume", "silence", "trigger", "follow_fade", "preload", "log", "audio" };
	INT32 k;
	for (k = 0; k < (INT32)(sizeof(known) / sizeof(known[0])); k++) if (!strcmp(key, known[k])) break;
	if (k == (INT32)(sizeof(known) / sizeof(known[0]))) bprintf(PRINT_ERROR, _T("OST: unknown option \"%S\"\n"), key);

	if (!strcmp(key, "volume"))      fOptVolume = atof(val) / 100.0;
	if (!strcmp(key, "silence"))     nOptSilence = (INT32)strtoul(val, NULL, 0);
	if (!strcmp(key, "trigger"))     bOptTriggerChange = (strcmp(val, "write") != 0);
	if (!strcmp(key, "follow_fade")) nOptFollowFade = !strcmp(val, "inverted") ? -1 : OstYes(val);
	if (!strcmp(key, "preload"))     bOptPreload = OstYes(val);
	if (!strcmp(key, "log"))         bBurnOstLog |= OstYes(val);
	if (!strcmp(key, "audio")) {
		snprintf(szArchive, sizeof(szArchive), "%s%s", TCHARToANSI(szAppOstPath, NULL, 0), val);
		snprintf(szFolder, sizeof(szFolder), "%s%s/", TCHARToANSI(szAppOstPath, NULL, 0), val);
	}
}

static void OstParseRuleKey(OstRule *r, char *key, char *val)
{
	static const char *known[] = { "loop", "once", "start", "restart", "fadein", "fadeout", "xfade", "time", "volume", "speed",
		"if_playing", "if_not_playing", "if", "set", "clear", "then", "forward" };
	INT32 k;
	for (k = 0; k < (INT32)(sizeof(known) / sizeof(known[0])); k++) if (!strcmp(key, known[k])) break;
	if (k == (INT32)(sizeof(known) / sizeof(known[0]))) bprintf(PRINT_ERROR, _T("OST: unknown setting \"%S\" (code %x)\n"), key, r->code);

	if (!strcmp(key, "loop"))           { r->loop = 1; if (val[0]) r->loop_at = OstParseTime(val); }
	if (!strcmp(key, "once"))           r->loop = 0;
	if (!strcmp(key, "start"))          r->start = OstParseTime(val);
	if (!strcmp(key, "restart"))        r->restart = OstYes(val);
	if (!strcmp(key, "fadein"))         r->fadein = atof(val);
	if (!strcmp(key, "fadeout"))        r->fadeout = atof(val);
	if (!strcmp(key, "xfade"))          r->xfade = atof(val);
	if (!strcmp(key, "time"))           r->time = atof(val);
	if (!strcmp(key, "volume"))         r->volume = atof(val) / 100.0;
	if (!strcmp(key, "speed"))          r->speed = atoi(val);
	if (!strcmp(key, "if_playing"))     r->if_playing = OstTrackIndex(val, 1);
	if (!strcmp(key, "if_not_playing")) r->if_not_playing = OstTrackIndex(val, 1);
	if (!strcmp(key, "if"))             { if (val[0] == '!') r->if_not_flags |= OstFlagBit(val + 1); else r->if_flags |= OstFlagBit(val); }
	if (!strcmp(key, "set"))            r->set_flags |= OstFlagBit(val);
	if (!strcmp(key, "clear"))          r->clear_flags |= OstFlagBit(val);
	if (!strcmp(key, "then")) {
		if      (!strcmp(val, "stop"))   r->then = THEN_STOP;
		else if (!strcmp(val, "resume")) r->then = THEN_RESUME;
		else if (!strcmp(val, "native")) r->then = THEN_NATIVE;
		else { r->then = THEN_CODE; r->then_code = strtoul(val, NULL, 0); }
	}
	if (!strcmp(key, "forward")) {
		if      (!strcmp(val, "drop")) r->forward = FWD_DROP;
		else if (!strcmp(val, "orig")) r->forward = FWD_ORIG;
		else { r->forward = FWD_CODE; r->forward_code = strtoul(val, NULL, 0); }
	}
}

// next space-separated token, quotes allowed anywhere in it:  "ghouls map"  if_playing="ghouls map"
static char *OstToken(char **pp)
{
	char *p = *pp;
	while (*p == ' ' || *p == '\t') p++;
	if (*p == 0) { *pp = p; return NULL; }

	char *tok = p, *dst = p;
	INT32 quoted = 0;
	while (*p && (quoted || (*p != ' ' && *p != '\t'))) {
		if (*p == '"') quoted ^= 1; else *dst++ = *p;
		p++;
	}
	if (*p) p++;
	*dst = 0;
	*pp = p;

	return tok;
}

// <code> <action> [arg] [key[=value] ...]
static void OstParseRule(char *line)
{
	if (nRules >= OST_MAX_RULES) return;

	OstRule *r = &Rules[nRules];
	memset(r, 0, sizeof(*r));
	r->track = r->if_playing = r->if_not_playing = -1;
	r->loop_at.smp = r->start.smp = -1;
	r->volume = 1.0;
	r->speed = 100;

	char *tok = OstToken(&line);
	if (tok == NULL) return;
	r->code = strtoul(tok, NULL, 0);

	if ((tok = OstToken(&line)) == NULL) return;

	if      (!strcmp(tok, "play"))   r->action = ACT_PLAY;
	else if (!strcmp(tok, "stop"))   r->action = ACT_STOP;
	else if (!strcmp(tok, "fade"))   r->action = ACT_FADE;
	else if (!strcmp(tok, "pass"))   r->action = ACT_PASS;
	else if (!strcmp(tok, "pause"))  r->action = ACT_PAUSE;
	else if (!strcmp(tok, "resume")) r->action = ACT_RESUME;
	else if (!strcmp(tok, "volume")) r->action = ACT_VOLUME;
	else { bprintf(PRINT_ERROR, _T("OST: unknown action \"%S\" (code %x)\n"), tok, r->code); return; }

	if (r->action == ACT_PLAY) {
		if ((tok = OstToken(&line)) == NULL) return;
		r->track = OstTrackIndex(tok, 1);
		r->forward = FWD_DEFAULT;
	} else {
		// stop/fade/pause/resume/volume: the game's own command still goes to the sound cpu
		r->forward = FWD_ORIG;
	}

	while ((tok = OstToken(&line)) != NULL) {
		char *val = strchr(tok, '=');
		if (val) *val++ = 0; else val = (char*)"";
		OstParseRuleKey(r, tok, val);
	}

	nRules++;
}

static INT32 OstLoadFile(const char *name)
{
	char path[MAX_PATH * 2];
	snprintf(path, sizeof(path), "%s%s.ost", TCHARToANSI(szAppOstPath, NULL, 0), name);

	FILE *fp = fopen(path, "rt");
	if (fp == NULL) return 0;

	bprintf(0, _T("OST: using %S\n"), path);

	// defaults: audio named after the .ost file
	snprintf(szArchive, sizeof(szArchive), "%s%s", TCHARToANSI(szAppOstPath, NULL, 0), name);
	snprintf(szFolder, sizeof(szFolder), "%s%s/", TCHARToANSI(szAppOstPath, NULL, 0), name);

	char line[1024];
	while (fgets(line, sizeof(line), fp)) {
		char *p, *d;
		INT32 quoted = 0;
		for (p = d = line; *p; p++) {
			if (*p == '"') quoted ^= 1;
			if (!quoted) {
				// strip # and // comments, but not inside "quotes"
				if (*p == '#' || (p[0] == '/' && p[1] == '/')) break;
				// "key = value" -> "key=value"
				if (*p == '=') { while (d > line && (d[-1] == ' ' || d[-1] == '\t')) d--; }
				if ((*p == ' ' || *p == '\t') && d > line && d[-1] == '=') continue;
			}
			*d++ = *p;
		}
		*d = 0;
		for (p = line + strlen(line); p > line && isspace((UINT8)p[-1]); ) *--p = 0;
		for (p = line; isspace((UINT8)*p); p++) {}
		if (*p == 0) continue;

		if (!strncmp(p, "option", 6) && isspace((UINT8)p[6])) p += 6;
		while (isspace((UINT8)*p)) p++;

		if (isdigit((UINT8)*p)) {
			OstParseRule(p);			// 0x10 play ...
		} else {
			char *tok;				// option key=value ...  (the "option" is optional)
			while ((tok = OstToken(&p)) != NULL) {
				char *val = strchr(tok, '=');
				if (val) *val++ = 0; else val = (char*)"yes";
				OstParseOption(tok, val);
			}
		}
	}

	fclose(fp);

	return 1;
}

// ---------------------------------------------------------------------------
// playback

static void OstVoiceClear(OstVoice *v)
{
	memset(v, 0, sizeof(*v));
	v->track = v->rule = -1;
	v->fade = v->fade_target = 1.0;
}

static void OstVoiceApplyGain(OstVoice *v)
{
	if (v->track < 0) return;

	double gain = v->fade * Rules[v->rule].volume * fOptVolume * fMusicVolume * fLatchVolume;

	BurnSampleSetRoute(v->track, BURN_SND_SAMPLE_ROUTE_1, gain, BURN_SND_ROUTE_LEFT);
	BurnSampleSetRoute(v->track, BURN_SND_SAMPLE_ROUTE_2, gain, BURN_SND_ROUTE_RIGHT);
}

static void OstVoiceFade(OstVoice *v, double target, double seconds, INT32 stop)
{
	v->fade_target = target;
	v->stop_at_target = stop;
	v->fade_step = (seconds > 0.0) ? ((target - v->fade) / (seconds * nBurnFPS / 100.0)) : (target - v->fade);
}

static void OstVoiceStop(OstVoice *v)
{
	// a track still used by another voice (ie. replayed during its own crossfade) keeps playing
	if (v->track >= 0 && !(v != &Cur && v->track == Cur.track)) {
		BurnSampleStop(v->track);
	}
	OstVoiceClear(v);
}

static INT32 OstStart(INT32 rule, UINT32 resume_pos) // returns 0 if the track's file is missing
{
	OstRule *r = &Rules[rule];
	INT32 t = r->track;

	BurnSampleSetLoop(t, r->loop);
	BurnSampleSetPlaybackRate(t, r->speed);
	BurnSamplePlay(t);					// loads it, if needed

	if (BurnSampleGetLength(t) == 0) {
		bprintf(0, _T("OST: track \"%S\" missing, using the game's music\n"), szTrack[t]);
		BurnSampleStop(t);
		return 0;
	}

	BurnSampleSetLoopStart(t, r->loop ? OstTimeToFrames(r->loop_at, t) : 0);
	if (resume_pos) {
		BurnSampleSetPosition(t, resume_pos);
	} else if (r->start.sec > 0.0 || r->start.smp > 0) {
		BurnSampleSetPosition(t, OstTimeToFrames(r->start, t));
	}

	OstVoiceClear(&Cur);
	Cur.track = t;
	Cur.rule = rule;
	if (r->fadein > 0.0) {
		Cur.fade = 0.0;
		OstVoiceFade(&Cur, 1.0, r->fadein, 0);
	}
	OstVoiceApplyGain(&Cur);

	return 1;
}

static INT32 OstRuleMatches(OstRule *r, UINT32 code)
{
	if (r->code != code) return 0;
	if (r->if_playing >= 0     && Cur.track != r->if_playing) return 0;
	if (r->if_not_playing >= 0 && Cur.track == r->if_not_playing) return 0;
	if ((nFlagBits & r->if_flags) != r->if_flags) return 0;
	if (nFlagBits & r->if_not_flags) return 0;

	return 1;
}

static INT32 OstForward(OstRule *r, UINT32 code)
{
	switch (r->forward) {
		case FWD_DROP: return OST_DROP;
		case FWD_ORIG: return code;
		case FWD_CODE: return r->forward_code;
	}

	return nOptSilence;	// FWD_DEFAULT: option silence=, or drop
}

static INT32 OstDoCommand(UINT32 code)
{
	INT32 i;
	for (i = 0; i < nRules; i++) {
		if (OstRuleMatches(&Rules[i], code)) break;
	}
	if (i == nRules) return code;		// no rule: hand it over untouched

	OstRule *r = &Rules[i];

	nFlagBits = (nFlagBits | r->set_flags) & ~r->clear_flags;

	switch (r->action) {
		case ACT_PASS:
			return code;

		case ACT_PLAY: {
			if (r->track == Cur.track && BurnSampleGetStatus(Cur.track) == SAMPLE_PLAYING && !r->restart) {
				Cur.rule = i;			// already playing: keep going
				break;
			}

			if (Prev.track >= 0 && Prev.track != Cur.track) OstVoiceStop(&Prev);

			if (r->then == THEN_RESUME && !r->loop && Cur.track >= 0 && Rules[Cur.rule].loop && Cur.track != r->track) {
				Prev = Cur;			// jingle: park the current track
				Prev.paused_pos = BurnSampleGetPosition(Cur.track);
				BurnSamplePause(Cur.track);
			} else if (Cur.track >= 0 && Cur.track != r->track) {
				if (Out.track >= 0) OstVoiceStop(&Out);
				Out = Cur;
				OstVoiceClear(&Cur);
				if (r->xfade > 0.0) {
					OstVoiceFade(&Out, 0.0, r->xfade, 1);
				} else {
					OstVoiceStop(&Out);
				}
			}

			if (OstStart(i, 0) == 0) {
				OstVoiceClear(&Cur);
				return code;			// missing file: let the game play its own music
			}
			break;
		}

		case ACT_STOP:
		case ACT_FADE:
			if (Cur.track >= 0) {
				if (r->action == ACT_FADE || r->time > 0.0) {
					OstVoiceFade(&Cur, 0.0, (r->time > 0.0) ? r->time : 1.0, 1);
				} else {
					OstVoiceStop(&Cur);
				}
			}
			if (Prev.track >= 0) OstVoiceStop(&Prev);
			break;

		case ACT_PAUSE:
			if (Cur.track >= 0) BurnSamplePause(Cur.track);
			break;

		case ACT_RESUME:
			if (Cur.track >= 0) BurnSampleResume(Cur.track);
			break;

		case ACT_VOLUME:
			fMusicVolumeTarget = r->volume;
			fMusicVolumeStep = (r->time > 0.0) ? ((fMusicVolumeTarget - fMusicVolume) / (r->time * nBurnFPS / 100.0)) : (fMusicVolumeTarget - fMusicVolume);
			break;
	}

	return OstForward(r, code);
}

INT32 BurnOstCommand(UINT32 nCode)
{
	if (!bOstActive) return nCode;

	if (bOptTriggerChange && (INT32)nCode == nLastIn) {
		return nLastOut;			// same value re-written: same answer, no new action
	}

	INT32 nOut = OstDoCommand(nCode);

	if (bBurnOstLog && (INT32)nCode != nLastIn) {
		bprintf(0, _T("OST: frame %6d  cmd %04x -> %s %04x  %S\n"), nCurrentFrame, nCode,
			(nOut == OST_DROP) ? _T("drop") : _T("send"), (nOut == OST_DROP) ? 0 : nOut,
			(Cur.track >= 0) ? szTrack[Cur.track] : "-");
	}

	nLastIn = nCode;
	nLastOut = nOut;

	return nOut;
}

void BurnOstFadeLatch(UINT8 nValue)
{
	if (!bOstActive || nOptFollowFade == 0) return;

	fLatchVolume = (nOptFollowFade > 0) ? (nValue / 255.0) : (1.0 - nValue / 255.0);
}

static void OstVoiceFrame(OstVoice *v)
{
	if (v->track < 0) return;

	if (v->fade != v->fade_target) {
		v->fade += v->fade_step;
		if ((v->fade_step >= 0.0 && v->fade >= v->fade_target) || (v->fade_step < 0.0 && v->fade <= v->fade_target)) {
			v->fade = v->fade_target;
			if (v->stop_at_target) {
				OstVoiceStop(v);
				return;
			}
		}
	}

	OstVoiceApplyGain(v);
}

void BurnOstFrame()
{
	if (!bOstActive) return;

	if (nPendingSend >= 0) {
		if (pOstSend) pOstSend(nPendingSend);
		nPendingSend = -1;
	}

	if (fMusicVolume != fMusicVolumeTarget) {
		fMusicVolume += fMusicVolumeStep;
		if ((fMusicVolumeStep >= 0.0 && fMusicVolume >= fMusicVolumeTarget) || (fMusicVolumeStep < 0.0 && fMusicVolume <= fMusicVolumeTarget)) {
			fMusicVolume = fMusicVolumeTarget;
		}
	}

	OstVoiceFrame(&Out);

	if (Cur.track < 0) return;

	OstRule *r = &Rules[Cur.rule];
	INT32 status = BurnSampleGetStatus(Cur.track);

	// "fadeout=": start fading this many seconds before the end of a non-looping track
	if (!r->loop && r->fadeout > 0.0 && !Cur.fading_end && status == SAMPLE_PLAYING) {
		UINT32 len = BurnSampleGetLength(Cur.track);
		UINT32 fade_len = (UINT32)(r->fadeout * nBurnSoundRate);
		if (len && (UINT32)BurnSampleGetPosition(Cur.track) + fade_len >= len) {
			Cur.fading_end = 1;
			OstVoiceFade(&Cur, 0.0, r->fadeout, 0);
		}
	}

	OstVoiceFrame(&Cur);

	// end of a non-looping track
	if (Cur.track >= 0 && status == SAMPLE_STOPPED) {
		INT32 then = r->then;
		UINT32 then_code = (then == THEN_CODE) ? r->then_code : r->code;

		OstVoiceClear(&Cur);

		switch (then) {
			case THEN_RESUME:
				if (Prev.track >= 0) {
					OstVoice p = Prev;
					OstVoiceClear(&Prev);
					OstStart(p.rule, p.paused_pos);
				}
				break;
			case THEN_NATIVE:			// hand the original command to the sound cpu after all
				nPendingSend = then_code;
				break;
			case THEN_CODE: {			// act as if the game sent then_code
				INT32 nOut = OstDoCommand(then_code);
				if (nOut != OST_DROP) nPendingSend = nOut;
				break;
			}
		}
	}
}

void BurnOstRender(INT16 *pSoundBuf, INT32 nLen)
{
	if (!bOstSamples || pSoundBuf == NULL) return;

	BurnSampleRender(pSoundBuf, nLen);
}

// ---------------------------------------------------------------------------

void BurnOstReset()
{
	if (!bOstActive) return;

	if (bOstSamples) BurnSampleReset();

	OstVoiceClear(&Cur);
	OstVoiceClear(&Out);
	OstVoiceClear(&Prev);
	nFlagBits = 0;
	fMusicVolume = fMusicVolumeTarget = 1.0;
	fMusicVolumeStep = 0.0;
	fLatchVolume = 1.0;
	nLastIn = nLastOut = -1;
	nPendingSend = -1;
}

INT32 BurnOstInit(const char *szSetName, const char *szParentName, INT32 nType, void (*pSendCommand)(UINT32 nCode))
{
	bOstActive = bOstSamples = 0;
	nTracks = nRules = nFlags = 0;
	fOptVolume = 1.0;
	nOptSilence = -1;
	bOptTriggerChange = (nType == OST_LATCH);
	nOptFollowFade = 0;
	bOptPreload = 0;
	pOstSend = pSendCommand;

	if (!bBurnOstEnable || nBurnSoundRate == 0) return 0;

	if (!(szSetName && OstLoadFile(szSetName)) && !(szParentName && OstLoadFile(szParentName))) {
		return 0;
	}

	bOstActive = 1;

	if (nTracks == 0) {
		// nothing to play (ie. all rules still commented out): log-only, to find the codes
		BurnOstReset();
		bprintf(0, _T("OST: no tracks, logging sound commands only\n"));
		bBurnOstLog = 1;
		return 1;
	}

	OstSource.szArchive = szArchive;
	OstSource.szFolder = szFolder;
	OstSource.nCount = nTracks;
	OstSource.GetName = OstGetName;
	OstSource.GetFlags = OstGetFlags;

	BurnSampleSetSource(&OstSource);
	BurnSampleInit(1);	// add to the driver's sound
	bOstSamples = 1;

	BurnOstReset();

	bprintf(0, _T("OST: %d tracks, %d rules\n"), nTracks, nRules);

	return 1;
}

void BurnOstExit()
{
	if (bOstSamples) {
		BurnSampleExit();
	}

	bOstActive = 0;
	bOstSamples = 0;
	nTracks = nRules = nFlags = 0;
	pOstSend = NULL;
}

INT32 BurnOstActive()
{
	return bOstActive;
}

void BurnOstScan(INT32 nAction, INT32 *pnMin)
{
	if (!bOstActive) return;

	if (bOstSamples) BurnSampleScan(nAction, pnMin);

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(Cur);
		SCAN_VAR(Out);
		SCAN_VAR(Prev);
		SCAN_VAR(nFlagBits);
		SCAN_VAR(fMusicVolume);
		SCAN_VAR(fMusicVolumeTarget);
		SCAN_VAR(fMusicVolumeStep);
		SCAN_VAR(fLatchVolume);
		SCAN_VAR(nLastIn);
		SCAN_VAR(nLastOut);
		SCAN_VAR(nPendingSend);

		if (nAction & ACB_WRITE) {
			// guard against states made with a different .ost
			if (Cur.track >= nTracks || Cur.rule >= nRules)   OstVoiceClear(&Cur);
			if (Out.track >= nTracks || Out.rule >= nRules)   OstVoiceClear(&Out);
			if (Prev.track >= nTracks || Prev.rule >= nRules) OstVoiceClear(&Prev);
		}
	}
}
