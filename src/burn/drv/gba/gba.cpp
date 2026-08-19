#ifndef GBA_STANDALONE
#include "burnint.h"
#else
#include <stdlib.h>
#define BurnMalloc(x) malloc(x)
#define BurnFree(x) do { free(x); (x) = NULL; } while (0)
#endif
#include "gba.h"
#include "gba_impl.h"

struct GbaCore {
	gba_t			state;
	gba_scratch_t	scratch;
	sb_emu_state_t	host;
	UINT8*			rom;
	size_t			romSize;
	bool			ownsRom;
	UINT8			externalBios[16 * 1024];
	bool			externalBiosLoaded;
	UINT32 			cartridgeFeatures;
	UINT8			cartridgeBackupType;
	double			sourceRate;
	INT32			outputFrames;
};

struct GbaCartridgeProfile {
	UINT8  gameCode[4];
	UINT32 features;
	UINT8  backupType;
};

static const UINT8 GbaSolarCounts[GBA_SOLAR_LEVEL_MAX + 1] = {
	233, 228, 222, 215, 206, 191, 171, 149, 124, 94, 50
};

INT32 GbaMotionAxisToInput(UINT16 axis)
{
	return (INT32)(((INT64)(INT32(axis) - 0x8000)) * 0x10000);
}

UINT8 GbaSolarLevelToInput(UINT8 level)
{
	if (level > GBA_SOLAR_LEVEL_MAX)
		level = GBA_SOLAR_LEVEL_MAX;
	return (UINT8)(((233 - GbaSolarCounts[level]) * 255) / 183);
}

UINT8 GbaSolarLegacyToLevel(UINT16 legacy)
{
	UINT8 input = legacy >> 8;
	INT32 count = 233 - (input * 183) / 255;
	UINT8 closest = 0;
	INT32 closestDistance = count - GbaSolarCounts[0];
	if (closestDistance < 0)
		closestDistance = -closestDistance;

	for (UINT8 level = 1; level <= GBA_SOLAR_LEVEL_MAX; level++) {
		INT32 distance = count - GbaSolarCounts[level];
		if (distance < 0)
			distance = -distance;
		if (distance < closestDistance) {
			closest = level;
			closestDistance = distance;
		}
	}
	return closest;
}

static const GbaCartridgeProfile GbaCartridgeProfiles[] = {
	{{'U', '3', 'I', 'J'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM    },
	{{'U', '3', 'I', 'E'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM    },
	{{'U', '3', 'I', 'P'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM    },
	{{'U', '3', '2', 'J'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM    },
	{{'U', '3', '2', 'E'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM    },
	{{'U', '3', '2', 'P'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM    },
	{{'U', '3', '3', 'J'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM    },
	{{'V', '4', '9', 'J'}, GBA_CART_RUMBLE,					GBA_BACKUP_SRAM      },
	{{'V', '4', '9', 'E'}, GBA_CART_RUMBLE,					GBA_BACKUP_SRAM      },
	{{'V', '4', '9', 'P'}, GBA_CART_RUMBLE,					GBA_BACKUP_SRAM      },
	{{'2', 'G', 'B', 'P'}, GBA_CART_RUMBLE,					GBA_BACKUP_SRAM      },
	{{'R', 'Z', 'W', 'J'}, GBA_CART_RUMBLE | GBA_CART_GYRO,	GBA_BACKUP_SRAM      },
	{{'R', 'Z', 'W', 'E'}, GBA_CART_RUMBLE | GBA_CART_GYRO,	GBA_BACKUP_SRAM      },
	{{'R', 'Z', 'W', 'P'}, GBA_CART_RUMBLE | GBA_CART_GYRO,	GBA_BACKUP_SRAM      },
	{{'K', 'H', 'P', 'J'}, GBA_CART_TILT,					GBA_BACKUP_EEPROM    },
	{{'K', 'Y', 'G', 'J'}, GBA_CART_TILT,					GBA_BACKUP_EEPROM    },
	{{'K', 'Y', 'G', 'E'}, GBA_CART_TILT,					GBA_BACKUP_EEPROM    },
	{{'K', 'Y', 'G', 'P'}, GBA_CART_TILT,					GBA_BACKUP_EEPROM    },
	{{'B', 'L', 'J', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_64K },
	{{'B', 'L', 'J', 'K'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_64K },
	{{'B', 'L', 'V', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_64K },
	{{'A', 'X', 'V', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'V', 'E'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'V', 'P'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'V', 'I'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'V', 'S'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'V', 'D'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'V', 'F'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'P', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'P', 'E'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'P', 'P'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'P', 'I'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'P', 'S'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'P', 'D'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'A', 'X', 'P', 'F'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'B', 'P', 'E', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'B', 'P', 'E', 'E'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'B', 'P', 'E', 'P'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'B', 'P', 'E', 'I'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'B', 'P', 'E', 'S'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'B', 'P', 'E', 'D'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'B', 'P', 'E', 'F'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
	{{'B', 'R', '4', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_64K },
	{{'B', 'K', 'A', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K},
};

static const GbaCartridgeProfile *GbaFindCartridgeProfile(const UINT8 *rom, size_t romSize)
{
	if (rom == NULL || romSize < 0xb3 || rom[0xb2] != 0x96)
		return NULL;

	for (UINT32 i = 0; i < sizeof(GbaCartridgeProfiles) / sizeof(GbaCartridgeProfiles[0]); i++) {
		if (memcmp(rom + 0xac, GbaCartridgeProfiles[i].gameCode, 4) == 0) {
			return &GbaCartridgeProfiles[i];
		}
	}
	return NULL;
}

static UINT32 GbaDetectCartridgeFeatures(const UINT8 *rom, size_t romSize)
{
	const GbaCartridgeProfile *profile = GbaFindCartridgeProfile(rom, romSize);
	return profile ? profile->features : 0;
}

static UINT8 GbaDetectCartridgeBackupType(const UINT8 *rom, size_t romSize)
{
	const GbaCartridgeProfile *profile = GbaFindCartridgeProfile(rom, romSize);
	return profile ? profile->backupType : 0;
}

static void GbaCoreClearPresentation(GbaCore *core)
{
	if (core == NULL)
		return;
	core->host.audio_ring_buff.read_ptr  = 0;
	core->host.audio_ring_buff.write_ptr = 0;
}

static void GbaCoreApplyCartridgeFeatures(GbaCore *core)
{
	core->state.cart.features = core->cartridgeFeatures;
	if (core->cartridgeBackupType != GBA_BACKUP_NONE) {
		core->state.cart.backup_type = core->cartridgeBackupType;
		gba_setup_flash_id(&core->state);
	}
	gba_gpio_update_rumble(&core->state);
	core->host.joy.rumble = core->state.cart.gpio.rumble;
}

static void GbaCoreApplyInput(GbaCore *core, const GbaInput *input)
{
	memset(core->host.joy.inputs, 0, sizeof(core->host.joy.inputs));
	core->host.joy.gyro_z                = 0;
	core->host.joy.tilt_x                = 0;
	core->host.joy.tilt_y                = 0;
	if (input == NULL)
		return;
	core->host.joy.inputs[SE_KEY_A     ] = (input->buttons >> GBA_BUTTON_A     ) & 1;
	core->host.joy.inputs[SE_KEY_B     ] = (input->buttons >> GBA_BUTTON_B     ) & 1;
	core->host.joy.inputs[SE_KEY_SELECT] = (input->buttons >> GBA_BUTTON_SELECT) & 1;
	core->host.joy.inputs[SE_KEY_START ] = (input->buttons >> GBA_BUTTON_START ) & 1;
	core->host.joy.inputs[SE_KEY_RIGHT ] = (input->buttons >> GBA_BUTTON_RIGHT ) & 1;
	core->host.joy.inputs[SE_KEY_LEFT  ] = (input->buttons >> GBA_BUTTON_LEFT  ) & 1;
	core->host.joy.inputs[SE_KEY_UP    ] = (input->buttons >> GBA_BUTTON_UP    ) & 1;
	core->host.joy.inputs[SE_KEY_DOWN  ] = (input->buttons >> GBA_BUTTON_DOWN  ) & 1;
	core->host.joy.inputs[SE_KEY_R     ] = (input->buttons >> GBA_BUTTON_R     ) & 1;
	core->host.joy.inputs[SE_KEY_L     ] = (input->buttons >> GBA_BUTTON_L     ) & 1;
	core->host.joy.solar_sensor          = input->solar / 255.0f;
	core->host.joy.gyro_z                = input->gyroZ;
	core->host.joy.tilt_x                = input->tiltX;
	core->host.joy.tilt_y                = input->tiltY;
}

INT32 GbaCoreInit(GbaCore **core)
{
	if (core  == NULL)
		return 1;
	*core = (GbaCore *)BurnMalloc(sizeof(GbaCore));
	if (*core == NULL)
		return 1;
	memset(*core, 0, sizeof(GbaCore));
	(*core)->host.render_frame  = true;
	(*core)->host.capture_audio = true;
	return 0;
}

void GbaCoreExit(GbaCore **core)
{
	if (core == NULL || *core == NULL)
		return;
	gba_unload(&(*core)->state, &(*core)->scratch);
	if ((*core)->ownsRom)
		BurnFree((*core)->rom);
	BurnFree(*core);
}

INT32 GbaCoreLoadRom(GbaCore *core, const UINT8 *rom, size_t romSize, const GbaRtcSeed *rtcSeed)
{
	if (core == NULL || rom == NULL || romSize == 0 || romSize > 32 * 1024 * 1024)
		return 1;
	const size_t allocSize = romSize < 0x100 ? 0x100 : romSize;
	UINT8 *romCopy = (UINT8 *)BurnMalloc((INT32)allocSize);
	if (romCopy == NULL)
		return 1;
	memset(romCopy, 0xff, allocSize);
	memcpy(romCopy, rom,  romSize);
	if (core->ownsRom)
		BurnFree(core->rom);
	core->rom     = romCopy;
	core->romSize = romSize;
	core->ownsRom = true;
	core->cartridgeFeatures   = GbaDetectCartridgeFeatures(core->rom, core->romSize);
	core->cartridgeBackupType = GbaDetectCartridgeBackupType(core->rom, core->romSize);
	rom = core->rom;
	core->host.rom_data   = core->rom;
	core->host.rom_size   = romSize;
	core->host.bios_data  = core->externalBiosLoaded ? core->externalBios : NULL;
	core->host.bios_size  = core->externalBiosLoaded ? sizeof(core->externalBios) : 0;
	core->host.rom_loaded = true;
	strcpy(core->host.rom_path, "fbneo.gba");
	gba_host_loading  = &core->host;
	const bool loaded = gba_load_rom(&core->host, &core->state, &core->scratch);
	gba_host_loading  = NULL;
	if (!loaded)
		return 1;
	gba_rtc_civil_t seed;
	memset(&seed, 0, sizeof(seed));
	if (rtcSeed) {
		seed.year    = rtcSeed->year;
		seed.month   = rtcSeed->month;
		seed.day     = rtcSeed->day;
		seed.weekday = rtcSeed->weekday;
		seed.hour    = rtcSeed->hour;
		seed.minute  = rtcSeed->minute;
		seed.second  = rtcSeed->second;
	}
	gba_rtc_cold_init(&core->state.rtc, rtcSeed ? &seed : NULL);
	GbaCoreApplyCartridgeFeatures(core);
	GbaCoreRebind(core);
	GbaCoreClearPresentation(core);
	return 0;
}

INT32 GbaCoreLoadBios(GbaCore *core, const UINT8 *bios, size_t biosSize)
{
	if (core == NULL || bios == NULL || biosSize != sizeof(core->externalBios))
		return 1;
	memcpy(core->externalBios, bios, sizeof(core->externalBios));
	core->externalBiosLoaded = true;
	return 0;
}

INT32 GbaCoreReset(GbaCore *core)
{
	if (core == NULL || core->rom == NULL)
		return 1;
	UINT8 battery[GBA_BATTERY_CAPACITY];
	INT64 rtcSeconds      = core->state.rtc.rtc_seconds;
	INT64 rtcHostSeconds  = core->state.rtc.host_seconds;
	UINT8 rtcStatus       = core->state.rtc.status;
	memcpy(battery, core->state.mem.cart_backup, sizeof(battery));
	core->host.rom_data   = core->rom;
	core->host.rom_size   = core->romSize;
	core->host.bios_data  = core->externalBiosLoaded ? core->externalBios : NULL;
	core->host.bios_size  = core->externalBiosLoaded ? sizeof(core->externalBios) : 0;
	gba_host_loading      = &core->host;
	const bool loaded     = gba_load_rom(&core->host, &core->state, &core->scratch);
	gba_host_loading      = NULL;
	if (!loaded)
		return 1;
	memcpy(core->state.mem.cart_backup, battery, sizeof(battery));
	core->state.rtc.rtc_seconds  = rtcSeconds;
	core->state.rtc.host_seconds = rtcHostSeconds;
	core->state.rtc.status       = rtcStatus;
	gba_rtc_transport_reset(&core->state.rtc);
	core->state.rtc.last_pins    = 0;
	GbaCoreApplyCartridgeFeatures(core);
	GbaCoreRebind(core);
	GbaCoreClearAudio(core);
	return 0;
}

void GbaCoreSetInput(GbaCore *core, const GbaInput *input)
{
	if (core != NULL)
		GbaCoreApplyInput(core, input);
}

INT32 GbaCoreConfigureAudio(GbaCore *core, double sourceRate, INT32 outputFrames, INT32 captureAudio)
{
	if (core == NULL)
		return 1;
	if (sourceRate <   0.0 ||   outputFrames <  0)
		return 1;
	if ((sourceRate <= 0.0) != (outputFrames <= 0))
		return 1;
	core->host.capture_audio = captureAudio != 0;
	if (sourceRate != core->sourceRate || outputFrames != core->outputFrames) {
		core->sourceRate             = sourceRate;
		core->outputFrames           = outputFrames;
		core->host.audio_sample_rate = sourceRate;
		GbaCoreClearPresentation(core);
	}
	return 0;
}

INT32 GbaCoreRunFrame(GbaCore *core)
{
	if (core == NULL || core->rom == NULL)
		return 1;
	core->host.render_frame = true;
	gba_tick(&core->host, &core->state, &core->scratch);
	// Reference capture harness: set GBA_REF=1 in the environment to dump
	// deterministic state signatures at fixed frame numbers.
	if (getenv("GBA_REF")) {
		static INT32 ref_frame = 0;
		ref_frame++;
		if (ref_frame == 1) printf("[GBA_REF] f1 wall_ms=%u\n", (unsigned)clock());
		if (ref_frame == 300) printf("[GBA_REF] f300 wall_ms=%u\n", (unsigned)clock());
		static const INT32 ref_points[] = { 1, 2, 5, 10, 30, 60, 120, 300 };
		bool hit = false;
		for (UINT32 i = 0; i < sizeof(ref_points) / sizeof(ref_points[0]); i++) {
			if (ref_frame == ref_points[i]) {
				hit = true;
				break;
			}
		}
		if (hit) {
			UINT32 fnv = 2166136261u;
			const UINT32* fb = (const UINT32*)core->scratch.framebuffer;
			for (INT32 i = 0; i < GBA_WIDTH * GBA_HEIGHT; i++) {
				fnv ^= fb[i];
				fnv *= 16777619u;
			}
			UINT32 pram_crc = 0, vram_crc = 0, oam_crc = 0;
			for (INT32 i = 0; i < 1024; i++)
				pram_crc += core->state.mem.palette[i] * (i + 1);
			for (INT32 i = 0; i < 0x18000 / 4; i++)
				vram_crc += ((UINT32*)core->state.mem.vram)[i] * (i + 1);
			for (INT32 i = 0; i < 1024 / 4; i++)
				oam_crc += ((UINT32*)core->state.mem.oam)[i] * (i + 1);
			FILE* f = fopen("gba_reference.log", "ab");
			if (f) {
				fprintf(f, "f=%d fb=%08x pram=%08x vram=%08x oam=%08x ie=%04x if=%04x ime=%04x dispstat=%04x vcount=%04x t0=%04x t1=%04x t2=%04x t3=%04x\n",
					ref_frame, fnv, pram_crc, vram_crc, oam_crc,
					*(UINT16*)(core->state.mem.io + 0x200), *(UINT16*)(core->state.mem.io + 0x202),
					*(UINT16*)(core->state.mem.io + 0x208), *(UINT16*)(core->state.mem.io + 0x004),
					*(UINT16*)(core->state.mem.io + 0x006),
					*(UINT16*)(core->state.mem.io + 0x100), *(UINT16*)(core->state.mem.io + 0x104),
					*(UINT16*)(core->state.mem.io + 0x108), *(UINT16*)(core->state.mem.io + 0x10c));
				fclose(f);
			}
			if (ref_frame == 300) {
				UINT8* state = (UINT8*)malloc(sizeof(gba_t));
				if (state) {
					if (GbaCoreSaveState(core, state, sizeof(gba_t)) == 0) {
						FILE* s = fopen("gba_reference_state.bin", "wb");
						if (s) {
							fwrite(state, 1, sizeof(gba_t), s);
							fclose(s);
						}
					}
					free(state);
				}
			}
		}
	}
	return 0;
}

UINT32 GbaCoreGetCartridgeFeatures(const GbaCore *core)
{
	return core == NULL ? 0 : core->cartridgeFeatures;
}

UINT8 GbaCoreGetRumbleOutput(const GbaCore *core)
{
	if (core == NULL || !(core->state.cart.features & GBA_CART_RUMBLE))
		return 0;
	return core->state.cart.gpio.rumble;
}

const UINT32 *GbaCoreGetFramebuffer(const GbaCore *core)
{
	return core == NULL ? NULL : (const UINT32 *)core->scratch.framebuffer;
}

UINT32 GbaCoreGetFramebufferPitch()
{
	return GBA_WIDTH * sizeof(UINT32);
}

static INT32 GbaCoreSourceFramesAvailable(GbaCore *core)
{
	return (INT32)(sb_ring_buffer_size(&core->host.audio_ring_buff) / 2);
}

INT32 GbaCoreRenderAudio(GbaCore *core, INT16 *stereo, INT32 frames)
{
	if (core == NULL || frames <= 0)
		return 0;
	if (core->sourceRate <= 0.0) {
		if (stereo != NULL)
			memset(stereo, 0, frames * 2 * sizeof(INT16));
		return frames;
	}

	sb_ring_buffer_t *ring = &core->host.audio_ring_buff;
	INT32 available = GbaCoreSourceFramesAvailable(core);
	INT32 produced  = available < frames ? available : frames;
	for (INT32 i = 0; i < produced; i++) {
		UINT32 left  = ring->read_ptr++ % SB_AUDIO_RING_BUFFER_SIZE;
		UINT32 right = ring->read_ptr++ % SB_AUDIO_RING_BUFFER_SIZE;
		if (stereo != NULL) {
			stereo[i * 2 + 0] = ring->data[left];
			stereo[i * 2 + 1] = ring->data[right];
		}
	}
	if (produced < frames) {
		INT32 missing = frames - produced;
		if (stereo != NULL)
			memset(stereo + produced * 2, 0, missing * 2 * sizeof(INT16));
		ring->read_ptr  += (UINT32)missing * 2;
		ring->write_ptr += (UINT32)missing * 2;
	}
	return frames;
}

void GbaCoreClearAudio(GbaCore *core)
{
	GbaCoreClearPresentation(core);
}

UINT8 *GbaCoreGetBatteryData(GbaCore *core)
{
	return core == NULL ? NULL : core->state.mem.cart_backup;
}

const UINT8 *GbaCoreGetBatteryDataConst(const GbaCore *core)
{
	return core == NULL ? NULL : core->state.mem.cart_backup;
}

size_t GbaCoreGetBatteryCapacity()
{
	return GBA_BATTERY_CAPACITY;
}

size_t GbaCoreGetBatterySize(const GbaCore *core)
{
	if (core == NULL)                return   0;
	switch (core->state.cart.backup_type) {
		case GBA_BACKUP_EEPROM_512B: return 512;
		case GBA_BACKUP_EEPROM:
		case GBA_BACKUP_EEPROM_8KB:  return   8 * 1024;
		case GBA_BACKUP_SRAM:        return  32 * 1024;
		case GBA_BACKUP_FLASH_64K:   return  64 * 1024;
		case GBA_BACKUP_FLASH_128K:  return 128 * 1024;
		default:                     return   0;
	}
}

INT32 GbaCoreLoadBattery(GbaCore *core, const UINT8 *data, size_t size)
{
	if (core == NULL || data == NULL || size > GBA_BATTERY_CAPACITY)
		return 1;
	memset(core->state.mem.cart_backup, 0xff, GBA_BATTERY_CAPACITY);
	memcpy(core->state.mem.cart_backup, data, size);
	core->state.cart.backup_is_dirty = false;
	return 0;
}

INT32 GbaCoreBatteryDirty(const GbaCore *core)
{
	return core != NULL && core->state.cart.backup_is_dirty;
}

void GbaCoreClearBatteryDirty(GbaCore *core)
{
	if (core != NULL) core->state.cart.backup_is_dirty = false;
}

size_t GbaCoreStateSize()
{
	return sizeof(gba_t);
}

#define GBA_RAW_STATE_MAGIC 0x53414247u	// 'AGBS'

INT32 GbaCoreSaveState(const GbaCore *core, void *data, size_t size)
{
	if (core == NULL || data == NULL || size < sizeof(gba_t))
		return 1;
	memcpy(data, &core->state, sizeof(gba_t));
	((gba_t *)data)->raw_state_magic = GBA_RAW_STATE_MAGIC;
	UINT8 *state = (UINT8 *)data;
#define GBA_CLEAR_STATE_FIELD(type, base, field)	memset(state + (base) + offsetof(type, field), 0, sizeof(((type *)0)->field))
	const size_t mem = offsetof(gba_t, mem);
	const size_t cpu = offsetof(gba_t, cpu);
	GBA_CLEAR_STATE_FIELD(gba_mem_t, mem, bios);
	GBA_CLEAR_STATE_FIELD(gba_mem_t, mem, cart_rom);
	GBA_CLEAR_STATE_FIELD(gba_mem_t, mem, cart_backup);
	GBA_CLEAR_STATE_FIELD(gba_t,     0,   framebuffer);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, user_data);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, log_cmp_file);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, read8);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, read16);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, read32);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, read16_seq);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, read32_seq);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, write8);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, write16);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, write32);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, coprocessor_read);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, coprocessor_write);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, trigger_breakpoint);
#undef GBA_CLEAR_STATE_FIELD
	return 0;
}

INT32 GbaCoreLoadState(GbaCore *core, const void *data, size_t size, INT32 preserveAudio)
{
	if (core == NULL || data == NULL || size < sizeof(gba_t) - sizeof(UINT32) || core->rom == NULL)
		return 1;
	if (size == sizeof(gba_t) && ((const gba_t *)data)->raw_state_magic != GBA_RAW_STATE_MAGIC)
		return 1;
	UINT8 battery[GBA_BATTERY_CAPACITY];
	memcpy(battery, core->state.mem.cart_backup, sizeof(battery));
	memcpy(&core->state, data, size);
	memcpy(core->state.mem.cart_backup, battery, sizeof(battery));
	GbaCoreApplyCartridgeFeatures(core);
	GbaCoreRebind(core);
	if (!preserveAudio)
		GbaCoreClearAudio(core);
	return 0;
}

void GbaCoreRebind(GbaCore *core)
{
	if (core == NULL)
		return;
	core->host.rom_data  = core->rom;
	core->host.rom_size  = core->romSize;
	core->host.bios_data = core->externalBiosLoaded ? core->externalBios : NULL;
	core->host.bios_size = core->externalBiosLoaded ? sizeof(core->externalBios) : 0;
	gba_ptrs_init(&core->state, &core->scratch, core->rom);
	core->state.cpu.trigger_breakpoint = gba_cpu_trigger_breakpoint;
}
