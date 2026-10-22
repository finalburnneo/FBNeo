// Headless C API for FinalBurn Neo.
// Author: Leonardo Kasperavičius
//
// Built originally to support OpenEmu-Silicon
// (https://github.com/OpenEmu-Silicon/OpenEmu-Silicon).
//
// Consumed by a thin host wrapper so the emulator-side glue stays in the fork.
// Subject to the FB Neo license (see src/license.txt).

#ifndef BURNER_HEADLESS_H
#define BURNER_HEADLESS_H

#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle
int  FBHeadlessInit(void);
void FBHeadlessExit(void);
void FBHeadlessSetRomPath(const char *path);
int  FBHeadlessLoadDriver(const char *shortName); // e.g. "mslug"; returns 0 on success
void FBHeadlessRunFrame(int draw);

// Video (filled by intf/video/headless)
const unsigned char *FBHeadlessGetVideoBuffer(void);
int FBHeadlessGetVideoWidth(void);
int FBHeadlessGetVideoHeight(void);
int FBHeadlessGetVideoPitch(void);
int FBHeadlessGetVideoBPP(void);

// Audio (filled each frame by the frontend)
int FBHeadlessGetAudioSampleRate(void);
const short *FBHeadlessGetAudioBuffer(void);
int FBHeadlessGetAudioFrames(void); // stereo sample frames produced last RunFrame

// Input: value is non-zero when pressed. code is an FBNeo input switch code.
void FBHeadlessSetInputState(int code, int value);

// Frame timing / geometry (valid after a driver is loaded).
int FBHeadlessGetFrameRate(void); // frames per second * 100 (e.g. 6000 = 60fps)
int FBHeadlessGetAspectX(void);
int FBHeadlessGetAspectY(void);

// Reset the loaded driver.
void FBHeadlessReset(void);

// High-level input. player is 0-based; button is an FBHeadlessButton value. The
// fork maps it to the active driver's inputs (parsed from its input names).
typedef enum {
	FBHeadlessButtonUp = 0,
	FBHeadlessButtonDown,
	FBHeadlessButtonLeft,
	FBHeadlessButtonRight,
	FBHeadlessButton1,
	FBHeadlessButton2,
	FBHeadlessButton3,
	FBHeadlessButton4,
	FBHeadlessButton5,
	FBHeadlessButton6,
	FBHeadlessButtonStart,
	FBHeadlessButtonCoin,
	FBHeadlessButtonService,
	FBHeadlessButtonCount
} FBHeadlessButton;
void FBHeadlessSetInput(int player, int button, int value);

#ifdef __cplusplus
}
#endif

#endif // BURNER_HEADLESS_H
