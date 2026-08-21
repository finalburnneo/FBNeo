#include "burnint.h"
#include "gba.h"

// ---------------------------------------------------------------------------
// Cross-subsystem forward declarations.  Every subsystem header is aggregated
// into the single translation unit of gba.cpp, so these declarations let a
// function in one header call a static function defined in another regardless
// of include order.  Specifiers mirror each definition exactly.
// ---------------------------------------------------------------------------

// apu.h
static inline void    gba_process_audio_writes(gba_t* gba);
static inline UINT8   gba_audio_process_byte_write(gba_t* gba, UINT32 addr, UINT8 value);
static inline void    gba_audio_fifo_push(gba_t* gba, INT32 fifo, INT8 data);

// bus.h
static inline void    gba_recompute_waitstate_table(gba_t* gba, UINT16 waitcnt);
static inline void    gba_recompute_mmio_mask_table(gba_t* gba);
static inline UINT32  gba_read32(gba_t* gba, UINT32 baddr);
static inline UINT16  gba_read16(gba_t* gba, UINT32 baddr);
static inline UINT8   gba_read8(gba_t* gba, UINT32 baddr);
static inline void    gba_store32(gba_t* gba, UINT32 baddr, UINT32 data);
static inline void    gba_store16(gba_t* gba, UINT32 baddr, UINT32 data);
static inline void    gba_store8(gba_t* gba, UINT32 baddr, UINT32 data);
static inline void    gba_io_store16(gba_t* gba, UINT32 baddr, UINT16 data);
static inline UINT32* gba_dword_lookup(gba_t* gba, UINT32 baddr, INT32 req_type);
static inline void    gba_process_mmio_read(gba_t* gba, UINT32 address);
static inline bool    gba_process_mmio_write(gba_t* gba, UINT32 address, UINT32 data, INT32 req_size_bytes);
static inline void    arm7_write32(void* user_data, UINT32 address, UINT32 data);
static inline void    gba_tick_keypad(sb_joy_t* joy, gba_t* gba);

// cart.h
static inline UINT16  gba_rom_read16(const gba_t* gba, UINT32 address);

// timer.h
static inline void                  gba_compute_timers(gba_t* gba);
static inline void    gba_tick_timers(gba_t* gba);
static inline void    gba_send_interrupt(gba_t* gba, INT32 pipe_stage, INT32 if_bit);

#include "gpio.h"
#include "cart.h"
#include "bus.h"
#include "timer.h"
#include "ppu.h"
#include "dma.h"
#include "sio.h"
#include "apu.h"

void gba_cpu_trigger_breakpoint(void* data);
void gba_ptrs_init(gba_t* gba, gba_scratch_t* scratch, UINT8* rom_data);
void gba_tick(sb_emu_state_t* emu, gba_t* gba, gba_scratch_t* scratch);
struct GbaCore {
	gba_t			state;
	gba_scratch_t	scratch;
	sb_emu_state_t	host;
	UINT8*			rom;
	size_t			romSize;
	bool			ownsRom;
	UINT8			externalBios[16 * 1024];
	bool			externalBiosLoaded;
	UINT32			cartridgeFeatures;
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

// Cartridge profiles keyed by the 4-character game code at ROM offset 0xac:
// explicit hardware devices and backup types, everything else resolves at runtime.
static const GbaCartridgeProfile GbaCartridgeProfiles[] = {
	// Advance Wars
	{ {'A', 'W', 'R', 'E'}, 0,								GBA_BACKUP_FLASH_64K  },
	{ {'A', 'W', 'R', 'P'}, 0,								GBA_BACKUP_FLASH_64K  },
	// Advance Wars 2: Black Hole Rising
	{ {'A', 'W', '2', 'E'}, 0,								GBA_BACKUP_FLASH_64K  },
	{ {'A', 'W', '2', 'P'}, 0,								GBA_BACKUP_FLASH_64K  },
	// Boktai: The Sun is in Your Hand
	{ {'U', '3', 'I', 'J'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM     },
	{ {'U', '3', 'I', 'E'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM     },
	{ {'U', '3', 'I', 'P'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM     },
	// Boktai 2: Solar Boy Django
	{ {'U', '3', '2', 'J'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM     },
	{ {'U', '3', '2', 'E'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM     },
	{ {'U', '3', '2', 'P'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM     },
	// Crash Bandicoot 2 - N-Tranced
	{ {'A', 'C', '8', 'J'}, 0,								GBA_BACKUP_EEPROM     },
	{ {'A', 'C', '8', 'E'}, 0,								GBA_BACKUP_EEPROM     },
	{ {'A', 'C', '8', 'P'}, 0,								GBA_BACKUP_EEPROM     },
	// DigiCommunication Nyo - Datou! Black Gemagema Dan
	{ {'B', 'D', 'K', 'J'}, 0,								GBA_BACKUP_EEPROM     },
	// Dragon Ball Z - The Legacy of Goku
	{ {'A', 'L', 'G', 'P'}, 0,								GBA_BACKUP_EEPROM     },
	// Dragon Ball Z - The Legacy of Goku II
	{ {'A', 'L', 'F', 'J'}, 0,								GBA_BACKUP_EEPROM     },
	{ {'A', 'L', 'F', 'E'}, 0,								GBA_BACKUP_EEPROM     },
	{ {'A', 'L', 'F', 'P'}, 0,								GBA_BACKUP_EEPROM     },
	// Dragon Ball Z - Taiketsu
	{ {'B', 'D', 'B', 'E'}, 0,								GBA_BACKUP_EEPROM     },
	{ {'B', 'D', 'B', 'P'}, 0,								GBA_BACKUP_EEPROM     },
	// Drill Dozer
	{ {'V', '4', '9', 'J'}, GBA_CART_RUMBLE,				GBA_BACKUP_SRAM       },
	{ {'V', '4', '9', 'E'}, GBA_CART_RUMBLE,				GBA_BACKUP_SRAM       },
	{ {'V', '4', '9', 'P'}, GBA_CART_RUMBLE,				GBA_BACKUP_SRAM       },
	// e-Reader
	{ {'P', 'E', 'A', 'J'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'P', 'S', 'A', 'J'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'P', 'S', 'A', 'E'}, 0,								GBA_BACKUP_FLASH_128K },
	// Final Fantasy Tactics Advance
	{ {'A', 'F', 'X', 'E'}, 0,								GBA_BACKUP_FLASH_64K  },
	// F-Zero - Climax
	{ {'B', 'F', 'T', 'J'}, 0,								GBA_BACKUP_FLASH_128K },
	// Goodboy Galaxy
	{ {'2', 'G', 'B', 'P'}, GBA_CART_RUMBLE,				GBA_BACKUP_SRAM       },
	// Iridion II
	{ {'A', 'I', '2', 'E'}, 0,								GBA_BACKUP_FORCE_NONE },
	{ {'A', 'I', '2', 'P'}, 0,								GBA_BACKUP_FORCE_NONE },
	// Game Boy Wars Advance 1+2
	{ {'B', 'G', 'W', 'J'}, 0,								GBA_BACKUP_FLASH_128K },
	// Golden Sun: The Lost Age
	{ {'A', 'G', 'F', 'E'}, 0,								GBA_BACKUP_FLASH_64K  },
	// Koro Koro Puzzle - Happy Panechu!
	{ {'K', 'H', 'P', 'J'}, GBA_CART_TILT,					GBA_BACKUP_EEPROM     },
	// Legendz - Yomigaeru Shiren no Shima
	{ {'B', 'L', 'J', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_64K  },
	{ {'B', 'L', 'J', 'K'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_64K  },
	// Legendz - Sign of Nekuromu
	{ {'B', 'L', 'V', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_64K  },
	// Mega Man Battle Network
	{ {'A', 'R', 'E', 'E'}, 0,								GBA_BACKUP_SRAM       },
	// Mega Man Zero
	{ {'A', 'Z', 'C', 'E'}, 0,								GBA_BACKUP_SRAM       },
	// Metal Slug Advance
	{ {'B', 'S', 'M', 'E'}, 0,								GBA_BACKUP_EEPROM     },
	// Pokemon Ruby
	{ {'A', 'X', 'V', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', 'V', 'E'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', 'V', 'P'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', 'V', 'I'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', 'V', 'S'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', 'V', 'D'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', 'V', 'F'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	// Pokemon Sapphire
	{ {'A', 'X', 'P', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', 'P', 'E'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', 'P', 'P'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', 'P', 'I'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', 'P', 'S'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', 'P', 'D'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', 'P', 'F'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	// Pokemon Emerald
	{ {'B', 'P', 'E', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'E', 'E'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'E', 'P'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'E', 'I'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'E', 'S'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'E', 'D'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'E', 'F'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	// Pokemon Mystery Dungeon
	{ {'B', '2', '4', 'E'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', '2', '4', 'P'}, 0,								GBA_BACKUP_FLASH_128K },
	// Pokemon FireRed
	{ {'B', 'P', 'R', 'J'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'R', 'E'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'R', 'P'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'R', 'I'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'R', 'S'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'R', 'D'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'R', 'F'}, 0,								GBA_BACKUP_FLASH_128K },
	// Pokemon LeafGreen
	{ {'B', 'P', 'G', 'J'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'G', 'E'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'G', 'P'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'G', 'I'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'G', 'S'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'G', 'D'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'B', 'P', 'G', 'F'}, 0,								GBA_BACKUP_FLASH_128K },
	// RockMan EXE 4.5 - Real Operation
	{ {'B', 'R', '4', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_64K  },
	// Rocky
	{ {'A', 'R', '8', 'E'}, 0,								GBA_BACKUP_EEPROM     },
	{ {'A', 'R', 'O', 'P'}, 0,								GBA_BACKUP_EEPROM     },
	// Sennen Kazoku
	{ {'B', 'K', 'A', 'J'}, GBA_CART_RTC,					GBA_BACKUP_FLASH_128K },
	// Shin Bokura no Taiyou: Gyakushuu no Sabata
	{ {'U', '3', '3', 'J'}, GBA_CART_RTC | GBA_CART_SOLAR,	GBA_BACKUP_EEPROM     },
	// Stuart Little 2
	{ {'A', 'S', 'L', 'E'}, 0,								GBA_BACKUP_FORCE_NONE },
	{ {'A', 'S', 'L', 'F'}, 0,								GBA_BACKUP_FORCE_NONE },
	// Super Mario Advance 2
	{ {'A', 'A', '2', 'J'}, 0,								GBA_BACKUP_EEPROM     },
	{ {'A', 'A', '2', 'E'}, 0,								GBA_BACKUP_EEPROM     },
	// Super Mario Advance 3
	{ {'A', '3', 'A', 'J'}, 0,								GBA_BACKUP_EEPROM     },
	{ {'A', '3', 'A', 'E'}, 0,								GBA_BACKUP_EEPROM     },
	{ {'A', '3', 'A', 'P'}, 0,								GBA_BACKUP_EEPROM     },
	// Super Mario Advance 4
	{ {'A', 'X', '4', 'J'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', '4', 'E'}, 0,								GBA_BACKUP_FLASH_128K },
	{ {'A', 'X', '4', 'P'}, 0,								GBA_BACKUP_FLASH_128K },
	// Super Monkey Ball Jr.
	{ {'A', 'L', 'U', 'E'}, 0,								GBA_BACKUP_EEPROM     },
	{ {'A', 'L', 'U', 'P'}, 0,								GBA_BACKUP_EEPROM     },
	// Top Gun - Combat Zones
	{ {'A', '2', 'Y', 'E'}, 0,								GBA_BACKUP_FORCE_NONE },
	// Ueki no Housoku - Jingi Sakuretsu! Nouryokusha Battle
	{ {'B', 'U', 'H', 'J'}, 0,								GBA_BACKUP_EEPROM     },
	// Wario Ware Twisted
	{ {'R', 'Z', 'W', 'J'}, GBA_CART_RUMBLE | GBA_CART_GYRO,	GBA_BACKUP_SRAM    },
	{ {'R', 'Z', 'W', 'E'}, GBA_CART_RUMBLE | GBA_CART_GYRO,	GBA_BACKUP_SRAM    },
	{ {'R', 'Z', 'W', 'P'}, GBA_CART_RUMBLE | GBA_CART_GYRO,	GBA_BACKUP_SRAM    },
	// Yoshi's Universal Gravitation
	{ {'K', 'Y', 'G', 'J'}, GBA_CART_TILT,					GBA_BACKUP_EEPROM     },
	{ {'K', 'Y', 'G', 'E'}, GBA_CART_TILT,					GBA_BACKUP_EEPROM     },
	{ {'K', 'Y', 'G', 'P'}, GBA_CART_TILT,					GBA_BACKUP_EEPROM     },
	// Aging cartridge
	{ {'T', 'C', 'H', 'K'}, 0,								GBA_BACKUP_EEPROM     },
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
	return profile ? profile->backupType : GBA_BACKUP_NONE;
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
	if (core->state.cart.fcmini.type)
		return;		// FC Mini carts own their SRAM mapper
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
	if (core == NULL || rom == NULL || romSize == 0 || romSize > 64 * 1024 * 1024)
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

INT32 GbaCoreWriteRom(GbaCore *core, UINT32 offset, const UINT8 *data, UINT32 length)
{
	if (core == NULL || core->rom == NULL || data == NULL || length == 0)
		return 1;
	if ((UINT64)offset + (UINT64)length > (UINT64)core->romSize)
		return 1;
	memcpy(core->rom + offset, data, length);
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

INT32 GbaCoreSaveState(const GbaCore *core, void *data, size_t size)
{
	if (core == NULL || data == NULL || size < sizeof(gba_t))
		return 1;
	memcpy(data, &core->state, sizeof(gba_t));
	UINT8 *state = (UINT8 *)data;
// pointer fields: nulls the pointer, not the pointee
#define GBA_CLEAR_STATE_FIELD(type, base, field)	memset(state + (base) + offsetof(type, field), 0, sizeof(((type *)0)->field))
	const size_t mem = offsetof(gba_t, mem);
	const size_t cpu = offsetof(gba_t, cpu);
	GBA_CLEAR_STATE_FIELD(gba_mem_t, mem, bios);
	GBA_CLEAR_STATE_FIELD(gba_mem_t, mem, cart_rom);
	GBA_CLEAR_STATE_FIELD(gba_mem_t, mem, cart_backup);
	GBA_CLEAR_STATE_FIELD(gba_t,     0,   framebuffer);
	GBA_CLEAR_STATE_FIELD(arm7_t,    cpu, user_data);
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
	if (core == NULL || data == NULL || size < sizeof(gba_t) || core->rom == NULL)
		return 1;
	UINT8 battery[GBA_BATTERY_CAPACITY];
	memcpy(battery, core->state.mem.cart_backup, sizeof(battery));
	memcpy(&core->state, data, sizeof(gba_t));
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
void gba_cpu_trigger_breakpoint(void* data)
{
	gba_t* gba = (gba_t*)data;
	gba->frame_in_progress = false;
	gba->pause_after_frame = true;
}

void gba_ptrs_init(gba_t* gba, gba_scratch_t* scratch, UINT8* rom_data)
{
	gba->framebuffer      = scratch->framebuffer;
	gba->mem.bios         = scratch->bios;
	gba->mem.cart_rom     = rom_data;
	gba->cpu.read8        = arm7_read8;
	gba->cpu.read16       = arm7_read16;
	gba->cpu.read32       = arm7_read32;
	gba->cpu.read16_seq   = arm7_read16_seq;
	gba->cpu.read32_seq   = arm7_read32_seq;
	gba->cpu.write8       = arm7_write8;
	gba->cpu.write16      = arm7_write16;
	gba->cpu.write32      = arm7_write32;
	gba->cpu.user_data    = gba;
}

void gba_tick(sb_emu_state_t* emu, gba_t* gba, gba_scratch_t* scratch)
{
	gba_ptrs_init(gba, scratch, emu->rom_data);
	gba->cpu.user_data          = gba;
	gba->cpu.trigger_breakpoint = gba_cpu_trigger_breakpoint;


	gba_tick_keypad(&emu->joy, gba);
	gba->frame_in_progress = true;
	float solar_value = emu->joy.solar_sensor;
	if (!(solar_value < 1.00))
		solar_value = 1.00;
	if (!(solar_value > 0.00))
		solar_value = 0.00;
	gba->solar_sensor.pending_value = 0xE9 - solar_value * (0xE9 - 0x32);	// latched into value when the game resets the sensor
	gba->gyro_sensor.pending_sample = gba_gyro_sample(emu->joy.gyro_z);
	gba->tilt_sensor.pending_x = gba_tilt_sample(emu->joy.tilt_x);
	gba->tilt_sensor.pending_y = gba_tilt_sample(emu->joy.tilt_y);
	gba->ppu.ghosting_strength = emu->screen_ghosting_strength;
	while (gba->frame_in_progress) {
		INT32 ticks = gba->activate_dmas ? gba_tick_dma(gba, gba->last_cpu_tick) : 0;
		if (!ticks && gba->residual_dma_ticks) {
			ticks = gba->residual_dma_ticks;
			gba->residual_dma_ticks = 0;
		}
		if (!ticks) {
			gba->cpu.i_cycles = 0;
			gba->mem.requests = 0;
			if (!gba->cpu.phased_op_id) {
				UINT16 int_if = gba_io_read16(gba, GBA_IF);
				if (SB_UNLIKELY(int_if)) {
					int_if &= gba_io_read16(gba, GBA_IE);
					UINT32 ime = gba_io_read32(gba, GBA_IME);
					int_if *= SB_BFE(ime, 0, 1);
					if (int_if)
						arm7_process_interrupts(&gba->cpu);
				}
			}
			arm7_exec_instruction(&gba->cpu);
			gba->last_cpu_tick = ticks = gba->mem.requests + gba->cpu.i_cycles;
		}
		gba_tick_sio(gba);
		INT32 ppu_fast_forward   = gba->ppu.fast_forward_ticks;
		INT32 timer_fast_forward = gba->timer_ticks_before_event - gba->deferred_timer_ticks;
		INT32 fast_forward_ticks = ppu_fast_forward < timer_fast_forward ? ppu_fast_forward : timer_fast_forward;
		if (fast_forward_ticks > ticks) {
			if (gba->cpu.wait_for_interrupt)
				ticks = fast_forward_ticks;
			else
				fast_forward_ticks = ticks;
		}
		if (SB_UNLIKELY(gba->active_if_pipe_stages)) {
			for (INT32 i = 0;i < fast_forward_ticks;++i)
				gba_tick_interrupts(gba);
		}
		// RTC advances with host wall time, not emulated master clocks
		gba->deferred_timer_ticks   += fast_forward_ticks;
		gba->ppu.fast_forward_ticks -= fast_forward_ticks;
		ticks -= fast_forward_ticks > ticks ? ticks : fast_forward_ticks;
		double delta_t = ((double)ticks + fast_forward_ticks) / (16 * 1024 * 1024);
		gba_tick_audio(gba, emu, delta_t, ticks + fast_forward_ticks);

		bool last_activate_dmas = gba->activate_dmas;
		for (INT32 t = 0;t < ticks;++t) {
			if (gba->activate_dmas && !last_activate_dmas) {
				gba->residual_dma_ticks = ticks - t - 1;
				gba->last_cpu_tick = t + 1;
			}
			gba_tick_interrupts(gba);
			gba_tick_timers(gba);
			gba_tick_ppu(gba, emu->render_frame);
		}
	}
	gba_gpio_update_rumble(gba);
	emu->joy.rumble = gba->cart.gpio.rumble;
	//LCD turns off in stop mode
	if (gba->stop_mode)
		memset(scratch->framebuffer, 0, sizeof(scratch->framebuffer));
	if (gba->pause_after_frame) {
		emu->run_mode          = SB_MODE_PAUSE;
		gba->pause_after_frame = false;
	}
}

