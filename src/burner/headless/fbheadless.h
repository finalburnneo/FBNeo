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

// Save/load a full machine state to/from a file. Returns 0 on success.
int FBHeadlessSaveState(const char *path);
int FBHeadlessLoadState(const char *path);

// Memory access for cheats, cheat search, and achievements. Addresses are in the
// main CPU (cpu 0) hardware address space. size is 1..4 bytes; littleEndian
// controls byte order for multi-byte values. A value found via a memory-range
// snapshot can be written back at the same address with FBHeadlessWriteMemory.
unsigned int FBHeadlessGetMainMemorySize(void);                                              // 0 until a driver is loaded
unsigned int FBHeadlessReadMemoryRange(unsigned int address, unsigned char *out, unsigned int length); // returns bytes read
unsigned int FBHeadlessReadMemory(unsigned int address, int size, int littleEndian);
int FBHeadlessWriteMemory(unsigned int address, unsigned int value, int size, int littleEndian); // 1 on success
int FBHeadlessIsAddressValid(unsigned int address);

// Achievements memory. RetroAchievements sets are authored against FBNeo's flat
// main-RAM exposure (per driver hardware type), not the CPU address space.
// FBHeadlessCheevosInit rebuilds the map (call after load/reset); the read is
// gated to the mapped regions. Returns bytes read (stops at the first unmapped byte).
void FBHeadlessCheevosInit(void);
unsigned int FBHeadlessReadAchievementMemory(unsigned int address, unsigned char *out, unsigned int length);

// Cheats: write into the achievement memory map (same flat RAM the search reads), and
// enumerate the mapped regions so the host can present cheat search. bigEndian follows
// the libretro/RetroAchievements convention the host matches: 0 (little-endian) for every
// board, including the 68K ones. See GetMainMemoryBigEndian.
int FBHeadlessWriteAchievementMemory(unsigned int address, unsigned int value, int size, int littleEndian);
int FBHeadlessGetMemoryRegionCount(void);
int FBHeadlessGetMemoryRegion(int index, unsigned int *start, unsigned int *length, int *bigEndian);

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
	FBHeadlessButtonService,   // test switch (settings menu); "service" when there is none
	FBHeadlessButtonConfigure, // "service" input of drivers that also have a test switch
	FBHeadlessButtonCount
} FBHeadlessButton;
void FBHeadlessSetInput(int player, int button, int value);

#ifdef __cplusplus
}
#endif

#endif // BURNER_HEADLESS_H
