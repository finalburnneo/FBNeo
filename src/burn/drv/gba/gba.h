// FBNeo GBA core host API.
// Core implementation derived from SkyEmu/Cult-of-GBA; see license.txt.

#pragma once

#ifndef GBA_CORE_H
#define GBA_CORE_H

#ifndef GBA_STANDALONE_TYPES
#include "burn.h"
#else
#include <stdint.h>
typedef uint8_t		UINT8;
typedef int8_t		INT8;
typedef int16_t		INT16;
typedef uint16_t	UINT16;
typedef int32_t		INT32;
typedef uint32_t	UINT32;
typedef int64_t		INT64;
typedef uint64_t	UINT64;
#endif
#include <stddef.h>

#define GBA_WIDTH				240
#define GBA_HEIGHT				160
#define GBA_MASTER_CLOCK		16777216
#define GBA_BATTERY_CAPACITY	(128 * 1024)
#define GBA_SOLAR_LEVEL_MAX		10

struct GbaCore;

enum GbaButton {
	GBA_BUTTON_A = 0,
	GBA_BUTTON_B,
	GBA_BUTTON_SELECT,
	GBA_BUTTON_START,
	GBA_BUTTON_RIGHT,
	GBA_BUTTON_LEFT,
	GBA_BUTTON_UP,
	GBA_BUTTON_DOWN,
	GBA_BUTTON_R,
	GBA_BUTTON_L,
};

enum GbaCartridgeFeature {
	GBA_CART_RTC    = 1 << 0,
	GBA_CART_SOLAR  = 1 << 1,
	GBA_CART_RUMBLE = 1 << 2,
	GBA_CART_GYRO   = 1 << 3,
	GBA_CART_TILT   = 1 << 4,
};

struct GbaInput {
	UINT16 buttons;
	UINT8  solar;
	INT32  gyroZ;
	INT32  tiltX;
	INT32  tiltY;
};

INT32 GbaMotionAxisToInput(UINT16 axis);
UINT8 GbaSolarLevelToInput(UINT8 level);
UINT8 GbaSolarLegacyToLevel(UINT16 legacy);

struct GbaRtcSeed {
	UINT16 year;
	UINT8  month;
	UINT8  day;
	UINT8  weekday;
	UINT8  hour;
	UINT8  minute;
	UINT8  second;
};

INT32  GbaCoreInit(GbaCore **core);
void   GbaCoreExit(GbaCore **core);
INT32  GbaCoreLoadRom(GbaCore *core, const UINT8 *rom, size_t romSize, const GbaRtcSeed *rtcSeed);
INT32  GbaCoreLoadBios(GbaCore *core, const UINT8 *bios, size_t biosSize);
INT32  GbaCoreReset(GbaCore *core);
void   GbaCoreSetInput(GbaCore *core, const GbaInput *input);
INT32  GbaCoreConfigureAudio(GbaCore *core, double sourceRate, INT32 outputFrames, INT32 captureAudio);
INT32  GbaCoreRunFrame(GbaCore *core);
UINT32 GbaCoreGetCartridgeFeatures(const GbaCore *core);
UINT8  GbaCoreGetRumbleOutput(const GbaCore *core);

const UINT32* GbaCoreGetFramebuffer(const GbaCore* core);
UINT32 GbaCoreGetFramebufferPitch();
INT32  GbaCoreRenderAudio(GbaCore *core, INT16 *stereo, INT32 frames);
void   GbaCoreClearAudio(GbaCore *core);

UINT8* GbaCoreGetBatteryData(GbaCore* core);
const UINT8* GbaCoreGetBatteryDataConst(const GbaCore* core);
size_t GbaCoreGetBatteryCapacity();
size_t GbaCoreGetBatterySize(const GbaCore *core);
INT32  GbaCoreLoadBattery(GbaCore *core, const UINT8 *data, size_t size);
INT32  GbaCoreBatteryDirty(const GbaCore *core);
void   GbaCoreClearBatteryDirty(GbaCore *core);

size_t GbaCoreStateSize();
INT32  GbaCoreSaveState(const GbaCore *core, void *data, size_t size);
INT32  GbaCoreLoadState(GbaCore *core, const void *data, size_t size, INT32 preserveAudio = 0);
void   GbaCoreRebind(GbaCore *core);

#endif
