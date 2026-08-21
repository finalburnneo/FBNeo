// FBNeo GBA core — central header: MMIO map, core state, host glue, public API.
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#if defined(__GNUC__) || defined(__clang__)
#define SB_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define SB_LIKELY(x)   __builtin_expect(!!(x), 1)
#elif defined(_MSC_VER)
#define SB_UNLIKELY(x) (x)
#define SB_LIKELY(x)   (x)
#else
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
#define SE_KEY_R						9
#define SE_KEY_START					10
#define SE_KEY_SELECT					11
#define SB_MODE_PAUSE					0

typedef struct {
	float inputs[SE_NUM_KEYBINDS];
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

static inline UINT32 sb_ring_buffer_size(sb_ring_buffer_t* buff)
{
	if (buff->read_ptr > SB_AUDIO_RING_BUFFER_SIZE) {
		buff->write_ptr -= SB_AUDIO_RING_BUFFER_SIZE;
		buff->read_ptr  -= SB_AUDIO_RING_BUFFER_SIZE;
	}
	return (buff->write_ptr - buff->read_ptr) % SB_AUDIO_RING_BUFFER_SIZE;
}

typedef struct {
	INT32				run_mode;
	bool				rom_loaded;
	sb_joy_t			joy;
	bool				render_frame;
	bool				capture_audio;
	double				audio_sample_rate;
	sb_ring_buffer_t	audio_ring_buff;
	char				save_file_path[SB_FILE_PATH_SIZE];
	float				screen_ghosting_strength;
	size_t				rom_size;
	UINT8*				rom_data;
	const UINT8*		bios_data;
	size_t				bios_size;
	char				rom_path[SB_FILE_PATH_SIZE];
} sb_emu_state_t;



#include "cpu.h"
//////////////////////////////////////////////////////////////////////////////////////////
// MMIO Register listing from GBATEK (https://problemkaputt.de/gbatek.htm#gbamemorymap) //
//////////////////////////////////////////////////////////////////////////////////////////
// LCD MMIO Registers
#define GBA_DISPCNT		0x4000000	/* R/W LCD Control */
#define GBA_GREENSWP	0x4000002	/* R/W Undocumented - Green Swap */
#define GBA_DISPSTAT	0x4000004	/* R/W General LCD Status (STAT,LYC) */
#define GBA_VCOUNT		0x4000006	/* R   Vertical Counter (LY) */
#define GBA_BG0CNT		0x4000008	/* R/W BG0 Control */
#define GBA_BG1CNT		0x400000A	/* R/W BG1 Control */
#define GBA_BG2CNT		0x400000C	/* R/W BG2 Control */
#define GBA_BG3CNT		0x400000E	/* R/W BG3 Control */
#define GBA_BG0HOFS		0x4000010	/* W   BG0 X-Offset */
#define GBA_BG0VOFS		0x4000012	/* W   BG0 Y-Offset */
#define GBA_BG1HOFS		0x4000014	/* W   BG1 X-Offset */
#define GBA_BG1VOFS		0x4000016	/* W   BG1 Y-Offset */
#define GBA_BG2HOFS		0x4000018	/* W   BG2 X-Offset */
#define GBA_BG2VOFS		0x400001A	/* W   BG2 Y-Offset */
#define GBA_BG3HOFS		0x400001C	/* W   BG3 X-Offset */
#define GBA_BG3VOFS		0x400001E	/* W   BG3 Y-Offset */
#define GBA_BG2PA		0x4000020	/* W   BG2 Rotation/Scaling Parameter A (dx) */
#define GBA_BG2PB		0x4000022	/* W   BG2 Rotation/Scaling Parameter B (dmx) */
#define GBA_BG2PC		0x4000024	/* W   BG2 Rotation/Scaling Parameter C (dy) */
#define GBA_BG2PD		0x4000026	/* W   BG2 Rotation/Scaling Parameter D (dmy) */
#define GBA_BG2X		0x4000028	/* W   BG2 Reference Point X-Coordinate */
#define GBA_BG2Y		0x400002C	/* W   BG2 Reference Point Y-Coordinate */
#define GBA_BG3PA		0x4000030	/* W   BG3 Rotation/Scaling Parameter A (dx) */
#define GBA_BG3PB		0x4000032	/* W   BG3 Rotation/Scaling Parameter B (dmx) */
#define GBA_BG3PC		0x4000034	/* W   BG3 Rotation/Scaling Parameter C (dy) */
#define GBA_BG3PD		0x4000036	/* W   BG3 Rotation/Scaling Parameter D (dmy) */
#define GBA_BG3X		0x4000038	/* W   BG3 Reference Point X-Coordinate */
#define GBA_BG3Y		0x400003C	/* W   BG3 Reference Point Y-Coordinate */
#define GBA_WIN0H		0x4000040	/* W   Window 0 Horizontal Dimensions */
#define GBA_WIN1H		0x4000042	/* W   Window 1 Horizontal Dimensions */
#define GBA_WIN0V		0x4000044	/* W   Window 0 Vertical Dimensions */
#define GBA_WIN1V		0x4000046	/* W   Window 1 Vertical Dimensions */
#define GBA_WININ		0x4000048	/* R/W Inside of Window 0 and 1 */
#define GBA_WINOUT		0x400004A	/* R/W Inside of OBJ Window & Outside of Windows */
#define GBA_MOSAIC		0x400004C	/* W   Mosaic Size */
#define GBA_BLDCNT		0x4000050	/* R/W Color Special Effects Selection */
#define GBA_BLDALPHA	0x4000052	/* R/W Alpha Blending Coefficients */
#define GBA_BLDY		0x4000054	/* W   Brightness (Fade-In/Out) Coefficient */

// Sound Registers
#define GBA_SOUND1CNT_L	0x4000060	/* R/W  Channel 1 Sweep register       (NR10) */
#define GBA_SOUND1CNT_H	0x4000062	/* R/W  Channel 1 Duty/Length/Envelope (NR11, NR12) */
#define GBA_SOUND1CNT_X	0x4000064	/* R/W  Channel 1 Frequency/Control    (NR13, NR14) */
#define GBA_SOUND2CNT_L	0x4000068	/* R/W  Channel 2 Duty/Length/Envelope (NR21, NR22) */
#define GBA_SOUND2CNT_H	0x400006C	/* R/W  Channel 2 Frequency/Control    (NR23, NR24) */
#define GBA_SOUND3CNT_L	0x4000070	/* R/W  Channel 3 Stop/Wave RAM select (NR30) */
#define GBA_SOUND3CNT_H	0x4000072	/* R/W  Channel 3 Length/Volume        (NR31, NR32) */
#define GBA_SOUND3CNT_X	0x4000074	/* R/W  Channel 3 Frequency/Control    (NR33, NR34) */
#define GBA_SOUND4CNT_L	0x4000078	/* R/W  Channel 4 Length/Envelope      (NR41, NR42) */
#define GBA_SOUND4CNT_H	0x400007C	/* R/W  Channel 4 Frequency/Control    (NR43, NR44) */
#define GBA_SOUNDCNT_L	0x4000080	/* R/W  Control Stereo/Volume/Enable   (NR50, NR51) */
#define GBA_SOUNDCNT_H	0x4000082	/* R/W  Control Mixing/DMA Control */
#define GBA_SOUNDCNT_X	0x4000084	/* R/W  Control Sound on/off           (NR52) */
#define GBA_SOUNDBIAS	0x4000088	/* BIOS Sound PWM Control */
#define GBA_WAVE_RAM	0x4000090	/* R/W  Channel 3 Wave Pattern RAM (2 banks!!) */
#define GBA_FIFO_A		0x40000A0	/* W    Channel A FIFO, Data 0-3 */
#define GBA_FIFO_B		0x40000A4	/* W    Channel B FIFO, Data 0-3 */

// DMA Transfer Channels
#define GBA_DMA0SAD		0x40000B0	/* W    DMA 0 Source Address */
#define GBA_DMA0DAD		0x40000B4	/* W    DMA 0 Destination Address */
#define GBA_DMA0CNT_L	0x40000B8	/* W    DMA 0 Word Count */
#define GBA_DMA0CNT_H	0x40000BA	/* R/W  DMA 0 Control */
#define GBA_DMA1SAD		0x40000BC	/* W    DMA 1 Source Address */
#define GBA_DMA1DAD		0x40000C0	/* W    DMA 1 Destination Address */
#define GBA_DMA1CNT_L	0x40000C4	/* W    DMA 1 Word Count */
#define GBA_DMA1CNT_H	0x40000C6	/* R/W  DMA 1 Control */
#define GBA_DMA2SAD		0x40000C8	/* W    DMA 2 Source Address */
#define GBA_DMA2DAD		0x40000CC	/* W    DMA 2 Destination Address */
#define GBA_DMA2CNT_L	0x40000D0	/* W    DMA 2 Word Count */
#define GBA_DMA2CNT_H	0x40000D2	/* R/W  DMA 2 Control */
#define GBA_DMA3SAD		0x40000D4	/* W    DMA 3 Source Address */
#define GBA_DMA3DAD		0x40000D8	/* W    DMA 3 Destination Address */
#define GBA_DMA3CNT_L	0x40000DC	/* W    DMA 3 Word Count */
#define GBA_DMA3CNT_H	0x40000DE	/* R/W  DMA 3 Control */

// Timer Registers
#define GBA_TM0CNT_L	0x4000100	/* R/W  Timer 0 Counter/Reload */
#define GBA_TM0CNT_H	0x4000102	/* R/W  Timer 0 Control */
#define GBA_TM1CNT_L	0x4000104	/* R/W  Timer 1 Counter/Reload */
#define GBA_TM1CNT_H	0x4000106	/* R/W  Timer 1 Control */
#define GBA_TM2CNT_L	0x4000108	/* R/W  Timer 2 Counter/Reload */
#define GBA_TM2CNT_H	0x400010A	/* R/W  Timer 2 Control */
#define GBA_TM3CNT_L	0x400010C	/* R/W  Timer 3 Counter/Reload */
#define GBA_TM3CNT_H	0x400010E	/* R/W  Timer 3 Control */

// Serial Communication (1)
#define GBA_SIODATA32	0x4000120	/*R/W   SIO Data (Normal-32bit Mode; shared with below) */
#define GBA_SIOMULTI0	0x4000120	/*R/W   SIO Data 0 (Parent)    (Multi-Player Mode) */
#define GBA_SIOMULTI1	0x4000122	/*R/W   SIO Data 1 (1st Child) (Multi-Player Mode) */
#define GBA_SIOMULTI2	0x4000124	/*R/W   SIO Data 2 (2nd Child) (Multi-Player Mode) */
#define GBA_SIOMULTI3	0x4000126	/*R/W   SIO Data 3 (3rd Child) (Multi-Player Mode) */
#define GBA_SIOCNT		0x4000128	/*R/W   SIO Control Register */
#define GBA_SIOMLT_SEND	0x400012A	/*R/W   SIO Data (Local of MultiPlayer; shared below) */
#define GBA_SIODATA8	0x400012A	/*R/W   SIO Data (Normal-8bit and UART Mode) */

// Keypad Input
#define GBA_KEYINPUT	0x4000130	/* R    Key Status */
#define GBA_KEYCNT		0x4000132	/* R/W  Key Interrupt Control */

// Serial Communication (2)
#define GBA_RCNT		0x4000134	/* R/W  SIO Mode Select/General Purpose Data */
#define GBA_IR			0x4000136	/* -    Ancient - Infrared Register (Prototypes only) */
#define GBA_JOYCNT		0x4000140	/* R/W  SIO JOY Bus Control */
#define GBA_JOY_RECV	0x4000150	/* R/W  SIO JOY Bus Receive Data */
#define GBA_JOY_TRANS	0x4000154	/* R/W  SIO JOY Bus Transmit Data */
#define GBA_JOYSTAT		0x4000158	/* R/?  SIO JOY Bus Receive Status */

// Interrupt, Waitstate, and Power-Down Control
#define GBA_IE			0x4000200	/* R/W  IE       Interrupt Enable Register */
#define GBA_IF			0x4000202	/* R/W  IF       Interrupt Request Flags / IRQ Acknowledge */
#define GBA_WAITCNT		0x4000204	/* R/W  WAITCNT  Game Pak Waitstate Control */
#define GBA_IME			0x4000208	/* R/W  IME      Interrupt Master Enable Register */
#define GBA_POSTFLG		0x4000300	/* R/W  POSTFLG  Undocumented - Post Boot Flag */
#define GBA_HALTCNT		0x4000301	/* W    HALTCNT  Undocumented - Power Down Control */
// #define GBA_?		0x4000410	/* ?    ?        Undocumented - Purpose Unknown / Bug ??? 0FFh */
// #define GBA_?		0x4000800	/* R/W  ?        Undocumented - Internal Memory Control (R/W) */
// #define GBA_?		0x4xx0800	/* R/W  ?        Mirrors of 4000800h (repeated each 64K) */
// #define GBA_(3DS)	0x4700000	/* W    (3DS)    Disable ARM7 bootrom overlay (3DS only) */

#define GBA_LCD_W							240
#define GBA_LCD_H							160
#define GBA_SWAPCHAIN_SIZE					4
#define GBA_AUDIO_DMA_ACTIVATE_THRESHOLD	12

// Interrupt sources
#define GBA_INT_LCD_VBLANK		0
#define GBA_INT_LCD_HBLANK		1
#define GBA_INT_LCD_VCOUNT		2
#define GBA_INT_TIMER0			3
#define GBA_INT_TIMER1			4
#define GBA_INT_TIMER2			5
#define GBA_INT_TIMER3			6
#define GBA_INT_SERIAL			7
#define GBA_INT_DMA0			8
#define GBA_INT_DMA1			9
#define GBA_INT_DMA2			10
#define GBA_INT_DMA3			11
#define GBA_INT_KEYPAD			12
#define GBA_INT_GAMEPAK			13

#define GBA_BG_PALETTE			0x00000000
#define GBA_OBJ_PALETTE			0x00000200
#define GBA_OBJ_TILES0_2		0x00010000
#define GBA_OBJ_TILES3_5		0x00014000
#define GBA_OAM					0x07000000

#define GBA_BACKUP_NONE			0
#define GBA_BACKUP_EEPROM		1
#define GBA_BACKUP_EEPROM_512B	2
#define GBA_BACKUP_EEPROM_8KB	3
#define GBA_BACKUP_SRAM			4
#define GBA_BACKUP_FLASH_64K	5
#define GBA_BACKUP_FLASH_128K	6
#define GBA_BACKUP_FORCE_NONE	7

#define GBA_REQ_1B				0x01
#define GBA_REQ_2B				0x02
#define GBA_REQ_4B				0x04
#define GBA_REQ_READ			0x40
#define GBA_REQ_WRITE			0x80

#include <time.h>

#define GBA_RTC_DAYS_1970_TO_2000 10957
#define GBA_RTC_STATUS_MASK 0x6a

#define GBA_RTC_NOW_SECONDS() ((INT64)time(NULL))

enum gba_rtc_phase {
	GBA_RTC_IDLE = 0,
	GBA_RTC_COMMAND,
	GBA_RTC_RECEIVE,
	GBA_RTC_SEND,
	GBA_RTC_COMPLETE,
};

enum gba_rtc_register {
	GBA_RTC_RESET = 0,
	GBA_RTC_UNUSED,
	GBA_RTC_DATE_TIME,
	GBA_RTC_FORCE_IRQ,
	GBA_RTC_STATUS,
	GBA_RTC_UNUSED2,
	GBA_RTC_TIME,
	GBA_RTC_UNUSED3,
};

typedef struct {
	UINT16 year;
	UINT8 month;
	UINT8 day;
	UINT8 weekday;
	UINT8 hour;
	UINT8 minute;
	UINT8 second;
} gba_rtc_civil_t;

typedef struct {
	INT64 rtc_seconds;
	INT64 host_seconds;
	UINT8 status;
	UINT8 phase;
	UINT8 command;
	UINT8 command_register;
	UINT8 command_read;
	UINT8 bit_index;
	UINT8 byte_index;
	UINT8 byte_count;
	UINT8 buffer[7];
	UINT8 last_pins;
	UINT8 sio_out;
} gba_rtc_t;

typedef struct {
	UINT8* bios;
	UINT8  wram0[256 * 1024];
	UINT8  wram1[32 * 1024];
	UINT8  io[1024];
	UINT8  palette[1024];
	UINT8  vram[128 * 1024];
	UINT8  oam[1024];
	UINT8* cart_rom;
	UINT8  cart_backup[128 * 1024];
	UINT8  matrix_window[0x2000];
	UINT8  flash_chip_id[4];
	UINT32 openbus_word;
	UINT32 eeprom_word;
	UINT32 eeprom_addr;
	UINT32 prefetch_en;
	UINT32 prefetch_size;
	UINT32 requests;
	UINT32 bios_word;
	UINT32 sram_word;
	UINT32 mmio_word;
	UINT8  wait_state_table[16 * 4];
	UINT32 mmio_data_mask_lookup[256];
	UINT8  mmio_reg_valid_lookup[256];
} gba_mem_t;

typedef struct {
	UINT8 data;
	UINT8 direction;
	UINT8 control;
	UINT8 input;
	UINT8 rumble;
} gba_gpio_t;

typedef struct {
	INT32  type;		// 0: none, 1: standard, 2: george, 3: alternate
	INT32  sram_mode;
	UINT8  write_sequence[5];
	bool   accepting_mode_change;
} gba_fcmini_t;

typedef struct {
	UINT32 rom_size;
	UINT8  backup_type;
	bool   backup_is_dirty;
	bool   in_chip_id_mode;
	INT32  flash_state;
	INT32  flash_bank;
	UINT32 features;
	struct {
		bool   active;
		UINT32 cmd;
		UINT32 paddr;
		UINT32 vaddr;
		UINT32 size;
	} matrix;
	INT32  eeprom_read_bits_remaining;
	gba_gpio_t   gpio;
	gba_fcmini_t fcmini;
} gba_cartridge_t;

typedef struct {
	INT32  source_addr;
	INT32  dest_addr;
	INT32  current_transaction;
	bool   last_enable;
	bool   last_vblank;
	bool   last_hblank;
	UINT32 latched_transfer;
	INT32  startup_delay;
	bool   activate_audio_dma;
	bool   video_dma_active;
} gba_dma_t;

typedef struct {
	INT32  scan_clock;
	bool   last_vblank;
	bool   last_hblank;
	INT32  last_lcd_y;
	struct {
		INT32 render_bgx;
		INT32 render_bgy;
		bool  wrote_bgx;
		bool  wrote_bgy;
	} aff[2];
	UINT16 dispcnt_pipeline[3];
	INT32  fast_forward_ticks;
	float  ghosting_strength;
	UINT32 mosaic_y_counter;
} gba_ppu_t;

typedef struct {
	bool   last_enable;
	UINT16 reload_value;
	UINT16 pending_reload_value;
	INT32  startup_delay;
} gba_timer_t;

typedef struct {
	UINT32 step_counter;
	INT32  length[4];
	UINT32 volume[4];
	UINT32 frequency[4];
	INT32  env_direction[4];	//1: increase 0: nochange -1: decrease
	UINT32 env_period[4];
	UINT32 env_period_timer[4];
	bool   env_overflow[4];
	//Only channel 1
	UINT32 sweep_period;
	UINT32 sweep_timer;
	INT32  sweep_direction;
	UINT32 sweep_shift;
	bool   sweep_enable;
	bool   sweep_subtracted;
	bool   use_length[4];
	bool   active[4];
	bool   powered[4];
	float  chan_t[4];
	UINT16 lfsr4;
} gba_frame_sequencer_t;

typedef struct {
	struct {
		INT8  data[64];
		INT32 read_ptr;
		INT32 write_ptr;
	} fifo[2];
	double current_sim_time;
	double current_sample_generated_time;
	UINT16 wave_freq_timer;
	UINT16 wave_sample_offset;
	UINT8  curr_wave_sample;
	UINT8  curr_wave_data;
	float  capacitor_l;
	float  capacitor_r;
	gba_frame_sequencer_t sequencer;
	UINT32 audio_clock;
} gba_audio_t;

typedef struct {
	INT32 ticks_till_transfer_done;
	bool  last_active;
} gba_sio_t;


typedef struct {
	UINT16 dac;
	UINT16 value;
	UINT8  light_edge;
	UINT8  pending_value;
} gba_solar_sensor_t;

typedef struct {
	UINT16 pending_sample;
	UINT16 shift_sample;
	UINT8  clock_high;
} gba_gyro_sensor_t;

typedef struct {
	UINT16 sample_x;
	UINT16 sample_y;
	UINT16 pending_x;
	UINT16 pending_y;
	UINT8  state;
} gba_tilt_sensor_t;

typedef struct gba_t {
	gba_mem_t			mem;
	arm7_t				cpu;
	gba_cartridge_t		cart;
	gba_ppu_t			ppu;
	gba_rtc_t			rtc;
	gba_dma_t			dma[4];
	gba_sio_t			sio;
	//There is a 2 cycle penalty when the CPU takes over from the DMA
	bool				last_transaction_dma;
	bool				activate_dmas;
	bool				dma_wait_ppu;
	gba_timer_t			timers[4];
	UINT32				timer_ticks_before_event;
	UINT32				deferred_timer_ticks;
	UINT32				global_timer;
	gba_audio_t			audio;
	bool				prev_key_interrupt;
	UINT32				first_target_buffer[GBA_LCD_W];
	UINT32				second_target_buffer[GBA_LCD_W];
	UINT8				window[GBA_LCD_W];
	UINT8*				framebuffer;
	// Some HW has up to a 4 cycle delay before its IF propagates. 
	// This array acts as a FIFO to keep track of that. 
	UINT16				pipelined_if[5];
	INT32				active_if_pipe_stages;
	INT32				last_cpu_tick;
	INT32				residual_dma_ticks;
	bool				stop_mode;
	bool				frame_in_progress;
	bool				pause_after_frame;
	gba_solar_sensor_t	solar_sensor;
	gba_gyro_sensor_t	gyro_sensor;
	gba_tilt_sensor_t	tilt_sensor;
} gba_t;

typedef struct {
	UINT8 framebuffer[GBA_LCD_W * GBA_LCD_H * 4];
	UINT8 bios[16 * 1024];
	bool  skip_bios_intro;
	char  save_file_path[SB_FILE_PATH_SIZE];
} gba_scratch_t;

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
	GBA_BUTTON_DIP1,
	GBA_BUTTON_DIP2,
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
INT32  GbaCoreWriteRom(GbaCore *core, UINT32 offset, const UINT8 *data, UINT32 length);
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
