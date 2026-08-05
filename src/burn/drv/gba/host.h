#pragma once

#ifndef GBA_HOST_H
#define GBA_HOST_H

#ifndef GBA_STANDALONE_TYPES
#include "burn.h"
#else
#include "gba.h"
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE inline __attribute__((always_inline))
#define SB_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define SB_LIKELY(x)   __builtin_expect(!!(x), 1)
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#define SB_UNLIKELY(x) (x)
#define SB_LIKELY(x)   (x)
#else
#define FORCE_INLINE inline
#define SB_UNLIKELY(x) (x)
#define SB_LIKELY(x)   (x)
#endif

#define SB_FILE_PATH_SIZE				1024
#define SB_BFE(VALUE, BITOFFSET, SIZE)	(((VALUE) >> (BITOFFSET)) & ((1llu << (SIZE)) - 1))
#define SB_AUDIO_RING_BUFFER_SIZE		(2048 * 8)
#define SE_NUM_KEYBINDS					36
#define SE_KEY_A						0
#define SE_KEY_B						1
#define SE_KEY_X						2
#define SE_KEY_Y						3
#define SE_KEY_UP						4
#define SE_KEY_DOWN						5
#define SE_KEY_LEFT						6
#define SE_KEY_RIGHT					7
#define SE_KEY_L						8
#define SE_KEY_R 						9
#define SE_KEY_START					10
#define SE_KEY_SELECT					11
#define SB_MODE_PAUSE					0

typedef struct {
	float inputs[SE_NUM_KEYBINDS];
	float touch_pos[2];
	float rumble;
	float solar_sensor;
	INT32 gyro_z;
	INT32 tilt_x;
	INT32 tilt_y;
} sb_joy_t;

typedef struct {
	INT16  data[SB_AUDIO_RING_BUFFER_SIZE];
	UINT32 read_ptr;
	UINT32 write_ptr;
} sb_ring_buffer_t;

static FORCE_INLINE UINT32 sb_ring_buffer_size(sb_ring_buffer_t* buff)
{
	if (buff->read_ptr > SB_AUDIO_RING_BUFFER_SIZE) {
		buff->write_ptr -= SB_AUDIO_RING_BUFFER_SIZE;
		buff->read_ptr  -= SB_AUDIO_RING_BUFFER_SIZE;
	}
	return (buff->write_ptr - buff->read_ptr) % SB_AUDIO_RING_BUFFER_SIZE;
}

typedef struct {
	INT32				run_mode;
	INT32				step_instructions;
	INT32				step_frames;
	bool				rom_loaded;
	INT32				system;
	sb_joy_t			joy;
	sb_joy_t			prev_frame_joy;
	INT32				frame;
	bool				render_frame;
	bool				capture_audio;
	double				audio_sample_rate;
	sb_ring_buffer_t	audio_ring_buff;
	UINT32				frames_since_rewind_push;
	char				save_file_path[SB_FILE_PATH_SIZE];
	float				screen_ghosting_strength;
	size_t				rom_size;
	UINT8*				rom_data;
	const UINT8*		bios_data;
	size_t				bios_size;
	char				rom_path[SB_FILE_PATH_SIZE];
} sb_emu_state_t;

typedef struct {
	bool read_since_reset;
	bool read_in_tick;
	bool write_since_reset;
	bool write_in_tick;
	bool trigger_breakpoint;
} sb_debug_mmio_access_t;

typedef struct {
	UINT32			addr;
	const char*		name;
	struct {
		UINT8		start;
		UINT8		size;
		const char*	name;
	} bits[32];
} mmio_reg_t;

static inline bool sb_path_has_file_ext(const char* path, const char* ext)
{
	if (ext[0] == '*')
		ext++;
	if (ext[0] == '.')
		ext++;
	if (ext[0] == '*')
		return true;
	const size_t extLen  = strlen(ext);
	const size_t pathLen = strlen(path);
	if (pathLen < extLen)
		return false;
	for (size_t i = 0; i < extLen; i++) {
		if (tolower((unsigned char)path[pathLen - extLen + i]) != tolower((unsigned char)ext[i]))
			return false;
	}
	return true;
}

static UINT8* sb_load_file_data(const char*, size_t* fileSize)
{
	if (fileSize)
		*fileSize = 0;
	return NULL;
}

static void sb_free_file_data(UINT8 *data)
{
	free(data);
}

static sb_emu_state_t *gba_host_loading;

static bool se_load_bios_file(const char*, const char*, const char*, UINT8* data, size_t dataSize)
{
	if (gba_host_loading == NULL || gba_host_loading->bios_data == NULL || gba_host_loading->bios_size != dataSize)
		return false;
	memcpy(data, gba_host_loading->bios_data, dataSize);
	return true;
}

static FILE* se_load_log_file(const char*, const char*)
{
	return NULL;
}

#endif
