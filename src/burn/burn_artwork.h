// FinalBurn Neo MAME-style artwork (.lay) support
//
// Loads <szAppArtworkPath><game>.zip (or a <game>/ folder, or the parent's),
// reads default.lay and composites the emulated screen into the first view
// that contains a screen.  While artwork is active, the frontend sees the
// size/aspect of the artwork view; drivers keep seeing their native size.
//
// Drivers can drive dynamic artwork (lamps, dioramas, 7-segment digits...)
// through named outputs, exactly like MAME's output_finder names:
//   BurnOutputSet("diorama0", 1);
//   BurnOutputSetIndexed("digit", 3, 0x3f);   // "digit3"

#ifndef _BURN_ARTWORK_H_
#define _BURN_ARTWORK_H_

// settings (bBurnArtworkEnable, nBurnArtworkView, nBurnArtworkMaxHeight, nBurnArtworkScale) are declared in burn.h

// outputs (usable with or without artwork loaded)
INT32 BurnOutputFind(const char *name);				// returns handle (creates if needed), -1 on failure
void  BurnOutputSetHandle(INT32 handle, INT32 value);
void  BurnOutputSet(const char *name, INT32 value);
void  BurnOutputSetIndexed(const char *prefix, INT32 index, INT32 value);
INT32 BurnOutputGet(const char *name);

// status, for drivers with built-in overlays (they should step aside)
INT32 BurnArtworkActive();

// --- internal (burn.cpp) ---
void  BurnOutputReset();
void  BurnArtworkInit();
void  BurnArtworkExit();
void  BurnArtworkFrameBegin();
void  BurnArtworkFrameEnd();
void  BurnArtworkScan(INT32 nAction);
INT32 BurnArtworkGetDisplaySize(INT32 *pnWidth, INT32 *pnHeight);	// returns 1 if artwork overrides
INT32 BurnArtworkGetAspect(INT32 *pnXAspect, INT32 *pnYAspect);		// returns 1 if artwork overrides
INT32 BurnArtworkGetBaseSize(INT32 *pnWidth, INT32 *pnHeight);		// window-sizing size, returns 1 if artwork active
INT32 BurnArtworkSetTargetSize(INT32 nWidth, INT32 nHeight);		// frontend output area, returns 1 if the size changed
void  BurnArtworkNativeSizeChanged();								// driver changed its visible size

#endif
