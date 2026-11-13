// burn_ost: replace a game's soundtrack with external audio files (via BurnSample)
//
// A driver hands every sound command the main cpu sends to its sound cpu to BurnOstCommand().
// If support/ost/<set>.ost (or <parent>.ost) has a rule for that command, a track is played /
// stopped / faded, and the driver gets back what to give the sound cpu instead: the original
// command, a replacement (ie. the game's own "stop music" code so its music engine goes quiet
// while sound effects keep working), or OST_DROP.  Commands without a rule are passed untouched,
// so a pack only needs to list the music.
//
// Audio for the pack is looked up in support/ost/<pack>.zip (or .7z), then support/ost/<pack>/,
// as .wav / .flac / .mp3.  <pack> is the .ost file's name unless "option audio=" says otherwise.
//
// See support/ost/example.ost for the file format.

#ifndef BURN_OST_H
#define BURN_OST_H

#define OST_DROP		(-1)	// BurnOstCommand(): don't pass anything to the sound cpu

extern INT32 bBurnOstEnable;		// frontend: use OST packs when found (keep 0 for netplay)
extern INT32 bBurnOstLog;		// frontend/debug: bprintf every new sound command (to write .ost files)
// szAppOstPath (burn.h): "support/ost/", defined by the frontend

// pSendCommand: lets the OST hand a command to the sound cpu outside of BurnOstCommand(),
// for "then=native" and "then=<code>" (may be NULL, those then do nothing)
// Uses BurnSample, so not for drivers that use samples themselves.
// nType: OST_LATCH   - the command sits in a latch the game may re-write with the same value
//                      (repeats aren't new commands, ie. CPS1 0x800181)
//        OST_MAILBOX - every write is a command (ie. CPS2 QSound shared ram)
// ("option trigger=change|write" in the .ost file overrides this)
// returns 1 if a pack was loaded
#define OST_LATCH		0
#define OST_MAILBOX		1
INT32 BurnOstInit(const char *szSetName, const char *szParentName, INT32 nType, void (*pSendCommand)(UINT32 nCode));
void  BurnOstExit();
void  BurnOstReset();
INT32 BurnOstActive();

// call where the main cpu writes the sound latch / mailbox.
// returns the command to give to the sound cpu, or OST_DROP
INT32 BurnOstCommand(UINT32 nCode);

// optional: a music volume/fade register the game writes (ie. CPS1 0x800189), see "option follow_fade"
void  BurnOstFadeLatch(UINT8 nValue);

void  BurnOstFrame();					// once per frame: fades, end-of-track actions
void  BurnOstRender(INT16 *pSoundBuf, INT32 nLen);	// mix into pSoundBuf (adds, stereo)
void  BurnOstScan(INT32 nAction, INT32 *pnMin);

#endif
