#include "sys16.h"

#define MAX_MIRRORS		256
#define LOG_MAPPER		0

// Laser Ghost does a write byte of 00 to fe0008 triggering IRQ7 during gun calibration - this trashes the memory map - disable it for now
bool LaserGhost = false;

typedef struct
{
	UINT8 regs[0x20];
	
	UINT32 tile_ram_start, tile_ram_end;
	UINT32 tile_ram_start_mirror[MAX_MIRRORS], tile_ram_end_mirror[MAX_MIRRORS];
	UINT32 tile_ram_num_mirrors;
	
	UINT32 io_start, io_end;
	UINT32 io_start_mirror[MAX_MIRRORS], io_end_mirror[MAX_MIRRORS];
	UINT32 io_num_mirrors;

	UINT32 bank_5704_start, bank_5704_end;
	UINT32 bank_5704_start_mirror[MAX_MIRRORS], bank_5704_end_mirror[MAX_MIRRORS];
	UINT32 bank_5704_num_mirrors;
	
	UINT32 korean_sound_start, korean_sound_end;
	UINT32 korean_sound_start_mirror[MAX_MIRRORS], korean_sound_end_mirror[MAX_MIRRORS];
	UINT32 korean_sound_num_mirrors;
	
	UINT32 math1_5797_start, math1_5797_end;
	UINT32 math1_5797_start_mirror[MAX_MIRRORS], math1_5797_end_mirror[MAX_MIRRORS];
	UINT32 math1_5797_num_mirrors;
	
	UINT32 math2_5797_start, math2_5797_end;
	UINT32 math2_5797_start_mirror[MAX_MIRRORS], math2_5797_end_mirror[MAX_MIRRORS];
	UINT32 math2_5797_num_mirrors;
	
	UINT32 genesis_vdp_start, genesis_vdp_end;
	UINT32 genesis_vdp_start_mirror[MAX_MIRRORS], genesis_vdp_end_mirror[MAX_MIRRORS];
	UINT32 genesis_vdp_num_mirrors;
	
	UINT32 bank_5987_start, bank_5987_end;
	UINT32 bank_5987_start_mirror[MAX_MIRRORS], bank_5987_end_mirror[MAX_MIRRORS];
	UINT32 bank_5987_num_mirrors;
	
	UINT32 bank_7525_start, bank_7525_end;
	UINT32 bank_7525_start_mirror[MAX_MIRRORS], bank_7525_end_mirror[MAX_MIRRORS];
	UINT32 bank_7525_num_mirrors;
	
	UINT32 road_ctrl_start, road_ctrl_end;
	UINT32 road_ctrl_start_mirror[MAX_MIRRORS], road_ctrl_end_mirror[MAX_MIRRORS];
	UINT32 road_ctrl_num_mirrors;
} sega_315_5195_struct;

static sega_315_5195_struct chip;

static bool mapper_in_use = false;
static UINT32 region_start = 0;
static UINT32 region_end = 0;
static UINT32 region_mirror = 0;
static bool open_bus_recurse = false;

sega_315_5195_custom_io sega_315_5195_custom_io_do = NULL;
sega_315_5195_custom_io_write sega_315_5195_custom_io_write_do = NULL;

static void compute_region(INT32 index, UINT32 length, UINT32 mirror, UINT32 offset)
{
	static const UINT32 region_size_map[4] = { 0x00ffff, 0x01ffff, 0x07ffff, 0x1fffff };
	
	UINT32 size_mask = region_size_map[chip.regs[0x10 + 2 * index] & 3];
	UINT32 base = (chip.regs[0x11 + 2 * index] << 16) & ~size_mask;
	mirror = mirror & size_mask;
	UINT32 start = base + (offset & size_mask);
	
	UINT32 final_length = length - 1;
	if (size_mask < final_length) final_length = size_mask;
	UINT32 end = start + final_length;
	
	region_start = start;
	region_end = end;
	region_mirror = mirror;
}

static void map_mirrors(UINT8 *data, UINT32 start, UINT32 end, UINT32 mirror, UINT32 map_mode)
{
	if (!mirror) return;
	
	INT32 lmirrorbit[18], lmirrorbits, hmirrorbit[14], hmirrorbits, lmirrorcount, hmirrorcount;
						
	hmirrorbits = lmirrorbits = 0;
	for (INT32 i = 0; i < 18; i++) {
		if (mirror & (1 << i)) lmirrorbit[lmirrorbits++] = 1 << i;
	}
	for (INT32 i = 18; i < 32; i++) {
		if (mirror & (1 << i)) hmirrorbit[hmirrorbits++] = 1 << i;
	}
	
	for (hmirrorcount = 0; hmirrorcount < (1 << hmirrorbits); hmirrorcount++) {
		INT32 hmirrorbase = 0;
		for (INT32 i = 0; i < hmirrorbits; i++) {
			if (hmirrorcount & (1 << i)) hmirrorbase |= hmirrorbit[i];
		}
		
		for (lmirrorcount = 0; lmirrorcount < (1 << lmirrorbits); lmirrorcount++) {
			INT32 lmirrorbase = hmirrorbase;
			for (INT32 i = 0; i < lmirrorbits; i++) {
				if (lmirrorcount & (1 << i)) lmirrorbase |= lmirrorbit[i];
			}
			
			SekMapMemory(data, start + lmirrorbase, end + lmirrorbase, map_mode);
			
			if (LOG_MAPPER) bprintf(PRINT_IMPORTANT, _T("MIRROR: %x, %x\n"), start + lmirrorbase, end + lmirrorbase);
		}
	}
}

static void store_mirrors(UINT32 *store_start, UINT32* store_end, UINT32 start, UINT32 end, UINT32 mirror, UINT32 *num_mirrors)
{
	if (!mirror) return;
	
	INT32 lmirrorbit[18], lmirrorbits, hmirrorbit[14], hmirrorbits, lmirrorcount = 0, hmirrorcount;
						
	hmirrorbits = lmirrorbits = 0;
	for (INT32 i = 0; i < 18; i++) {
		if (mirror & (1 << i)) lmirrorbit[lmirrorbits++] = 1 << i;
	}
	for (INT32 i = 18; i < 32; i++) {
		if (mirror & (1 << i)) hmirrorbit[hmirrorbits++] = 1 << i;
	}
	
	for (hmirrorcount = 0; hmirrorcount < (1 << hmirrorbits); hmirrorcount++) {
		INT32 hmirrorbase = 0;
		for (INT32 i = 0; i < hmirrorbits; i++) {
			if (hmirrorcount & (1 << i)) hmirrorbase |= hmirrorbit[i];
		}
		
		for (lmirrorcount = 0; lmirrorcount < (1 << lmirrorbits); lmirrorcount++) {
			INT32 lmirrorbase = hmirrorbase;
			for (INT32 i = 0; i < lmirrorbits; i++) {
				if (lmirrorcount & (1 << i)) lmirrorbase |= lmirrorbit[i];
			}
			
			store_start[lmirrorcount] = start + lmirrorbase;
			store_end[lmirrorcount] = end + lmirrorbase;
			
			if (LOG_MAPPER) bprintf(PRINT_IMPORTANT, _T("MIRROR %i: %x, %x\n"), lmirrorcount, start + lmirrorbase, end + lmirrorbase);
		}
	}
	
	if (lmirrorcount > MAX_MIRRORS) {
		*num_mirrors = MAX_MIRRORS;
	} else {
		*num_mirrors = lmirrorcount;
	}
}

static void update_mapping()
{
	if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Remapping\n"));
	
	SekMapHandler(0, 0x000000, 0xffffff, MAP_RAM);
	chip.tile_ram_start = chip.tile_ram_end = 0x000000;
	chip.io_start = chip.io_end = 0x000000;
	chip.bank_5704_start = chip.bank_5704_end = 0x000000;
	chip.korean_sound_start = chip.korean_sound_end = 0x000000;
	chip.math1_5797_start = chip.math1_5797_end = 0x000000;
	chip.math2_5797_start = chip.math2_5797_end = 0x000000;
	chip.genesis_vdp_start = chip.genesis_vdp_end = 0x000000;
	chip.bank_5987_start = chip.bank_5987_end = 0x000000;
	chip.bank_7525_start = chip.bank_7525_end = 0x000000;
	chip.road_ctrl_start = chip.road_ctrl_end = 0x000000;
	
	chip.tile_ram_num_mirrors = 0;
	chip.io_num_mirrors = 0;
	chip.bank_5704_num_mirrors = 0;
	chip.korean_sound_num_mirrors = 0;
	chip.math1_5797_num_mirrors = 0;
	chip.math2_5797_num_mirrors = 0;
	chip.genesis_vdp_num_mirrors = 0;
	chip.bank_5987_num_mirrors = 0;
	chip.bank_7525_num_mirrors = 0;
	chip.road_ctrl_num_mirrors = 0;
	
	memset(chip.tile_ram_start_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.tile_ram_end_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.io_start_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.io_end_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.bank_5704_start_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.bank_5704_end_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.korean_sound_start_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.korean_sound_end_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.math1_5797_start_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.math1_5797_end_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.math2_5797_start_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.math2_5797_end_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.genesis_vdp_start_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.genesis_vdp_end_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.bank_5987_start_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.bank_5987_end_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.bank_7525_start_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.bank_7525_end_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.road_ctrl_start_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	memset(chip.road_ctrl_end_mirror, 0, MAX_MIRRORS * sizeof(UINT32));
	
	for (INT32 index = 7; index >= 0; index--) {
		if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM16B) {
			switch (index) {
				case 7: {
					compute_region(index, 0x004000, 0xffc000, 0x000000);
					chip.io_start = region_start;
					chip.io_end = region_end;
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("IO: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					store_mirrors(chip.io_start_mirror, chip.io_end_mirror, region_start, region_end, region_mirror, &chip.io_num_mirrors);
					
					break;
				}
				
				case 6: {
					compute_region(index, 0x001000, 0xfff000, 0x000000);
					SekMapMemory(System16PaletteRam, region_start, region_end, MAP_RAM);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Palette RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					map_mirrors(System16PaletteRam, region_start, region_end, region_mirror, MAP_RAM);
					
					break;
				}
				
				case 5: {
					compute_region(index, 0x001000, 0xfef000, 0x010000);
					SekMapMemory(System16TextRam, region_start, region_end, MAP_RAM);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Text RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					map_mirrors(System16TextRam, region_start, region_end, region_mirror, MAP_RAM);
					
					compute_region(index, 0x010000, 0xfe0000, 0x000000);
					SekMapMemory(System16TileRam, region_start, region_end, MAP_READ);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Tile RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					chip.tile_ram_start = region_start;
					chip.tile_ram_end = region_end;
					
					store_mirrors(chip.tile_ram_start_mirror, chip.tile_ram_end_mirror, region_start, region_end, region_mirror, &chip.tile_ram_num_mirrors);
					
					break;
				}
			
				case 4: {
					compute_region(index, 0x000800, 0xfff800, 0x000000);
					SekMapMemory(System16SpriteRam, region_start, region_end, MAP_RAM);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Sprite RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					map_mirrors(System16SpriteRam, region_start, region_end, region_mirror, MAP_RAM);
					
					break;
				}
				
				case 3: {
					if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5704_PS2) {
						compute_region(index, 0x040000, ~(0x040000 - 1), 0x000000);
						SekMapMemory(System16Ram, region_start, region_end, MAP_RAM);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Work RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					} else {
						compute_region(index, 0x004000, ~(0x004000 - 1), 0x000000);
						SekMapMemory(System16Ram, region_start, region_end, MAP_RAM);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Work RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						map_mirrors(System16Ram, region_start, region_end, region_mirror, MAP_RAM);
					}
					
					break;
				}
				
				case 2: {
					if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5358_SMALL) {
						compute_region(index, 0x020000, 0xfe0000, 0x000000);
						SekMapMemory(System16Rom + 0x20000, region_start, region_end, MAP_READ);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 2: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
							SekMapMemory(((UINT8*)fd1094_userregion) + 0x20000, region_start, region_end, MAP_FETCH);
						} else {
							SekMapMemory(System16Code + 0x20000, region_start, region_end, MAP_FETCH);
						}
					}
					
					if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5358) {
						compute_region(index, 0x020000, 0xfe0000, 0x000000);
						SekMapMemory(System16Rom + 0x40000, region_start, region_end, MAP_READ);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 2: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
							SekMapMemory(((UINT8*)fd1094_userregion) + 0x40000, region_start, region_end, MAP_FETCH);
						} else {
							SekMapMemory(System16Code + 0x40000, region_start, region_end, MAP_FETCH);
						}
					}
					
					if (((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5521) || ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5704) || ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5704_PS2)) {
						compute_region(index, 0x010000, 0xff0000, 0x000000);
						chip.bank_5704_start = region_start;
						chip.bank_5704_end = region_end;
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 2: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						store_mirrors(chip.bank_5704_start_mirror, chip.bank_5704_end_mirror, region_start, region_end, region_mirror, &chip.bank_5704_num_mirrors);
					}
					
					if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_YM2413) {
						break;
					}
					
					if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5797) {
						compute_region(index, 0x010000, 0xff0000, 0x000000);
						chip.math2_5797_start = region_start;
						chip.math2_5797_end = region_end;
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 2: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						store_mirrors(chip.math2_5797_start_mirror, chip.math2_5797_end_mirror, region_start, region_end, region_mirror, &chip.math2_5797_num_mirrors);
					}
				
					break;
				}
				
				case 1: {
					if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5358_SMALL) {
						compute_region(index, 0x020000, 0xfe0000, 0x000000);
						SekMapMemory(System16Rom + 0x10000, region_start, region_end, MAP_READ);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 1: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						map_mirrors(System16Rom + 0x10000, region_start, region_end, region_mirror, MAP_READ);
						
						if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
							SekMapMemory(((UINT8*)fd1094_userregion) + 0x10000, region_start, region_end, MAP_FETCH);
							map_mirrors(((UINT8*)fd1094_userregion) + 0x10000, region_start, region_end, region_mirror, MAP_FETCH);
						} else {
							SekMapMemory(System16Code + 0x10000, region_start, region_end, MAP_FETCH);
							map_mirrors(System16Code + 0x10000, region_start, region_end, region_mirror, MAP_FETCH);
						}
					}
					
					if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5358) {
						compute_region(index, 0x020000, 0xfe0000, 0x000000);
						SekMapMemory(System16Rom + 0x20000, region_start, region_end, MAP_READ);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 1: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						map_mirrors(System16Rom + 0x20000, region_start, region_end, region_mirror, MAP_READ);
						
						if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
							SekMapMemory(((UINT8*)fd1094_userregion) + 0x20000, region_start, region_end, MAP_FETCH);
							map_mirrors(((UINT8*)fd1094_userregion) + 0x20000, region_start, region_end, region_mirror, MAP_FETCH);
						} else {
							SekMapMemory(System16Code + 0x20000, region_start, region_end, MAP_FETCH);
							map_mirrors(System16Code + 0x20000, region_start, region_end, region_mirror, MAP_FETCH);
						}
					}
					
					if (((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5521) || ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5704) || ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5704_PS2)) {
						compute_region(index, 0x040000, 0xfc0000, 0x000000);
						SekMapMemory(System16Rom + 0x40000, region_start, region_end, MAP_READ);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 1: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						map_mirrors(System16Rom + 0x40000, region_start, region_end, region_mirror, MAP_READ);
						
						if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
							SekMapMemory(((UINT8*)fd1094_userregion) + 0x40000, region_start, region_end, MAP_FETCH);
							map_mirrors(((UINT8*)fd1094_userregion) + 0x40000, region_start, region_end, region_mirror, MAP_FETCH);
						} else {
							SekMapMemory(System16Code + 0x40000, region_start, region_end, MAP_FETCH);
							map_mirrors(System16Code + 0x40000, region_start, region_end, region_mirror, MAP_FETCH);
						}
					}
					
					if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_YM2413) {
						compute_region(index, 0x010000, 0xff0000, 0x000000);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 1: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						chip.korean_sound_start = region_start;
						chip.korean_sound_end = region_end;
						
						store_mirrors(chip.korean_sound_start_mirror, chip.korean_sound_end_mirror, region_start, region_end, region_mirror, &chip.korean_sound_num_mirrors);
					}
					
					if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5797) {
						compute_region(index, 0x004000, 0xffc000, 0x000000);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 1: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						chip.math1_5797_start = region_start;
						chip.math1_5797_end = region_end;
						
						store_mirrors(chip.math1_5797_start_mirror, chip.math1_5797_end_mirror, region_start, region_end, region_mirror, &chip.math1_5797_num_mirrors);
					}
					
					break;
				}
				
				case 0: {
					if (((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5358) || ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5358_SMALL)) {
						compute_region(index, 0x020000, 0xfe0000, 0x000000);
						SekMapMemory(System16Rom, region_start, region_end, MAP_READ);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 0: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						map_mirrors(System16Rom, region_start, region_end, region_mirror, MAP_READ);
						
						if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
							SekMapMemory(((UINT8*)fd1094_userregion), region_start, region_end, MAP_FETCH);
							map_mirrors(((UINT8*)fd1094_userregion), region_start, region_end, region_mirror, MAP_FETCH);
						} else {
							SekMapMemory(System16Code, region_start, region_end, MAP_FETCH);
							map_mirrors(System16Code, region_start, region_end, region_mirror, MAP_FETCH);
						}
					}
					
					if (((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5521) || ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5704) || ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5704_PS2)) {
						compute_region(index, 0x040000, 0xfc0000, 0x000000);
						SekMapMemory(System16Rom, region_start, region_end, MAP_READ);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 0: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						map_mirrors(System16Rom, region_start, region_end, region_mirror, MAP_READ);
						
						if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
							SekMapMemory(((UINT8*)fd1094_userregion), region_start, region_end, MAP_FETCH);
							map_mirrors(((UINT8*)fd1094_userregion), region_start, region_end, region_mirror, MAP_FETCH);
						} else {
							SekMapMemory(System16Code, region_start, region_end, MAP_FETCH);
							map_mirrors(System16Code, region_start, region_end, region_mirror, MAP_FETCH);
						}
					}
					
					if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_YM2413) {
						compute_region(index, 0x040000, 0xfc0000, 0x000000);
						SekMapMemory(System16Rom, region_start, region_end, MAP_READ);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 0: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						map_mirrors(System16Rom, region_start, region_end, region_mirror, MAP_READ);
						
						if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
							SekMapMemory(((UINT8*)fd1094_userregion), region_start, region_end, MAP_FETCH);
							map_mirrors(((UINT8*)fd1094_userregion), region_start, region_end, region_mirror, MAP_FETCH);
						} else {
							SekMapMemory(System16Code, region_start, region_end, MAP_FETCH);
							map_mirrors(System16Code, region_start, region_end, region_mirror, MAP_FETCH);
						}
					}
					
					if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_5797) {
						compute_region(index, 0x080000, 0xf80000, 0x000000);
						SekMapMemory(System16Rom, region_start, region_end, MAP_READ);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 0: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						map_mirrors(System16Rom, region_start, region_end, region_mirror, MAP_READ);
						
						if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
							SekMapMemory(((UINT8*)fd1094_userregion), region_start, region_end, MAP_FETCH);
							map_mirrors(((UINT8*)fd1094_userregion), region_start, region_end, region_mirror, MAP_FETCH);
						} else {
							SekMapMemory(System16Code, region_start, region_end, MAP_FETCH);
							map_mirrors(System16Code, region_start, region_end, region_mirror, MAP_FETCH);
						}
					}
					
					break;
				}
			}
		}
		
		if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM18) {
			switch (index) {
				case 7: {
					compute_region(index, 0x004000, 0xffc000, 0x000000);
					chip.io_start = region_start;
					chip.io_end = region_end;
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("IO: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					store_mirrors(chip.io_start_mirror, chip.io_end_mirror, region_start, region_end, region_mirror, &chip.io_num_mirrors);
					
					break;
				}
				
				case 6: {
					compute_region(index, 0x001000, 0xfff000, 0x000000);
					SekMapMemory(System16PaletteRam, region_start, region_end, MAP_RAM);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Palette RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					map_mirrors(System16PaletteRam, region_start, region_end, region_mirror, MAP_RAM);
					
					break;
				}
				
				case 5: {
					compute_region(index, 0x001000, 0xfef000, 0x010000);
					SekMapMemory(System16TextRam, region_start, region_end, MAP_RAM);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Text RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					map_mirrors(System16TextRam, region_start, region_end, region_mirror, MAP_RAM);
					
					compute_region(index, 0x010000, 0xfe0000, 0x000000);
					SekMapMemory(System16TileRam, region_start, region_end, MAP_READ);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Tile RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					chip.tile_ram_start = region_start;
					chip.tile_ram_end = region_end;
					
					store_mirrors(chip.tile_ram_start_mirror, chip.tile_ram_end_mirror, region_start, region_end, region_mirror, &chip.tile_ram_num_mirrors);
					
					break;
				}
				
				case 4: {
					compute_region(index, 0x004000, 0xffc000, 0x000000);
					SekMapMemory(System16SpriteRam, region_start, region_end, MAP_RAM);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Sprite RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					map_mirrors(System16SpriteRam, region_start, region_end, region_mirror, MAP_RAM);
					
					break;
				}
				
				case 3: {
					compute_region(index, 0x004000, 0xffc000, 0x000000);
					SekMapMemory(System16Ram, region_start, region_end, MAP_RAM);
						
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Work RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
					map_mirrors(System16Ram, region_start, region_end, region_mirror, MAP_RAM);
					
					break;
				}
				
				case 2: {
					if (((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_171_5874) || ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_171_5987) || ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_837_7525)) {
						compute_region(index, 0x000010, 0xfffff0, 0x000000);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 2: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						chip.genesis_vdp_start = region_start;
						chip.genesis_vdp_end = region_end;
						
						//store_mirrors(chip.genesis_vdp_start_mirror, chip.genesis_vdp_end_mirror, region_start, region_end, region_mirror, &chip.genesis_vdp_num_mirrors);
					}
					
					break;
				}
				
				case 1: {
					if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_171_5874) {
						compute_region(index, 0x080000, 0xf80000, 0x000000);
						SekMapMemory(System16Rom + 0x80000, region_start, region_end, MAP_READ);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 1: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						map_mirrors(System16Rom + 0x80000, region_start, region_end, region_mirror, MAP_READ);
						
						if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
							SekMapMemory(((UINT8*)fd1094_userregion) + 0x80000, region_start, region_end, MAP_FETCH);
							map_mirrors(((UINT8*)fd1094_userregion) + 0x80000, region_start, region_end, region_mirror, MAP_FETCH);
						} else {
							SekMapMemory(System16Code + 0x80000, region_start, region_end, MAP_FETCH);
							map_mirrors(System16Code + 0x80000, region_start, region_end, region_mirror, MAP_FETCH);
						}
					}
					
					if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_171_5987) {
						if (System16RomSize <= 0x100000) {
							compute_region(index, 0x080000, 0xf80000, 0x000000);
							SekMapMemory(System16Rom + 0x80000, region_start, region_end, MAP_READ);
							
							if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 1: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
							
							map_mirrors(System16Rom + 0x80000, region_start, region_end, region_mirror, MAP_READ);
							
							if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
								SekMapMemory(((UINT8*)fd1094_userregion) + 0x80000, region_start, region_end, MAP_FETCH);
								map_mirrors(((UINT8*)fd1094_userregion) + 0x80000, region_start, region_end, region_mirror, MAP_FETCH);
							} else {
								SekMapMemory(System16Code + 0x80000, region_start, region_end, MAP_FETCH);
								map_mirrors(System16Code + 0x80000, region_start, region_end, region_mirror, MAP_FETCH);
							}
						} else {
							compute_region(index, 0x100000, 0xf00000, 0x000000);
							SekMapMemory(System16Rom + 0x100000, region_start, region_end, MAP_READ);
							
							if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 1: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
							
							map_mirrors(System16Rom + 0x100000, region_start, region_end, region_mirror, MAP_READ);
							
							if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
								SekMapMemory(((UINT8*)fd1094_userregion) + 0x100000, region_start, region_end, MAP_FETCH);
								map_mirrors(((UINT8*)fd1094_userregion) + 0x100000, region_start, region_end, region_mirror, MAP_FETCH);
							} else {
								SekMapMemory(System16Code, region_start + 0x100000, region_end, MAP_FETCH);
								map_mirrors(System16Code, region_start + 0x100000, region_end, region_mirror, MAP_FETCH);
							}
						}
						
						chip.bank_5987_start = region_start;
						chip.bank_5987_end = region_end;
						
						store_mirrors(chip.bank_5987_start_mirror, chip.bank_5987_end_mirror, region_start, region_end, region_mirror, &chip.bank_5987_num_mirrors);
					}
					
					if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_837_7525) {
						compute_region(index, 0x080000, 0xf80000, 0x000000);
						SekMapMemory(System16Rom + 0x80000, region_start, region_end, MAP_READ);
							
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 1: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
							
						map_mirrors(System16Rom + 0x80000, region_start, region_end, region_mirror, MAP_READ);
							
						if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
							SekMapMemory(((UINT8*)fd1094_userregion) + 0x80000, region_start, region_end, MAP_FETCH);
							map_mirrors(((UINT8*)fd1094_userregion) + 0x80000, region_start, region_end, region_mirror, MAP_FETCH);
						} else {
							SekMapMemory(System16Code + 0x80000, region_start, region_end, MAP_FETCH);
							map_mirrors(System16Code + 0x80000, region_start, region_end, region_mirror, MAP_FETCH);
						}
						
						chip.bank_7525_start = region_start;
						chip.bank_7525_end = region_end;
						
						store_mirrors(chip.bank_7525_start_mirror, chip.bank_7525_end_mirror, region_start, region_end, region_mirror, &chip.bank_7525_num_mirrors);
					}
					
					if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_171_SHADOW) {
						compute_region(index, 0x000010, 0xfffff0, 0x000000);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 2: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						chip.genesis_vdp_start = region_start;
						chip.genesis_vdp_end = region_end;
						
						//store_mirrors(chip.genesis_vdp_start_mirror, chip.genesis_vdp_end_mirror, region_start, region_end, region_mirror, &chip.genesis_vdp_num_mirrors);
					}
				
					break;
				}
			
				case 0: {
					if (((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_171_5874) || ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_171_SHADOW)) {
						compute_region(index, 0x080000, 0xf80000, 0x000000);
						SekMapMemory(System16Rom, region_start, region_end, MAP_READ);
						
						if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 0: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
						map_mirrors(System16Rom, region_start, region_end, region_mirror, MAP_READ);
						
						if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
							SekMapMemory(((UINT8*)fd1094_userregion), region_start, region_end, MAP_FETCH);
							map_mirrors(((UINT8*)fd1094_userregion), region_start, region_end, region_mirror, MAP_FETCH);
						} else {
							SekMapMemory(System16Code, region_start, region_end, MAP_FETCH);
							map_mirrors(System16Code, region_start, region_end, region_mirror, MAP_FETCH);
						}
					}
					
					if (((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_171_5987) || ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_PCB_MASK) == HARDWARE_SEGA_837_7525)) {
						if (System16RomSize <= 0x100000) {
							compute_region(index, 0x080000, 0xf80000, 0x000000);
							SekMapMemory(System16Rom, region_start, region_end, MAP_READ);
							
							if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 0: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
							
							map_mirrors(System16Rom, region_start, region_end, region_mirror, MAP_READ);
							
							if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
								SekMapMemory(((UINT8*)fd1094_userregion), region_start, region_end, MAP_FETCH);
								map_mirrors(((UINT8*)fd1094_userregion), region_start, region_end, region_mirror, MAP_FETCH);
							} else {
								SekMapMemory(System16Code, region_start, region_end, MAP_FETCH);
								map_mirrors(System16Code, region_start, region_end, region_mirror, MAP_FETCH);
							}
						} else {
							compute_region(index, 0x100000, 0xf00000, 0x000000);
							SekMapMemory(System16Rom, region_start, region_end, MAP_READ);
							
							if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 0: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
							
							map_mirrors(System16Rom, region_start, region_end, region_mirror, MAP_READ);
							
							if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
								SekMapMemory(((UINT8*)fd1094_userregion), region_start, region_end, MAP_FETCH);
								map_mirrors(((UINT8*)fd1094_userregion), region_start, region_end, region_mirror, MAP_FETCH);
							} else {
								SekMapMemory(System16Code, region_start, region_end, MAP_FETCH);
								map_mirrors(System16Code, region_start, region_end, region_mirror, MAP_FETCH);
							}
						}
					}
					
					break;
				}
			}
		}
		
		if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_OUTRUN) {
			switch (index) {
				case 7: {
					break;
				}
				
				case 6: {
					break;
				}
				
				case 5: {
					// road control
					compute_region(index, 0x010000, 0xf00000, 0x090000);
					chip.road_ctrl_start = region_start;
					chip.road_ctrl_end = region_end;
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Road Ctrl: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					store_mirrors(chip.road_ctrl_start_mirror, chip.road_ctrl_end_mirror, region_start, region_end, region_mirror, &chip.road_ctrl_num_mirrors);
					
					// road ram
					compute_region(index, 0x001000, 0xf0f000, 0x080000);
					SekMapMemory(System16RoadRam, region_start, region_end, MAP_RAM);
						
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Road RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
					map_mirrors(System16RoadRam, region_start, region_end, region_mirror, MAP_RAM);
					
					// shared ram
					compute_region(index, 0x008000, 0xf18000, 0x060000);
					SekMapMemory(System16Ram, region_start, region_end, MAP_RAM);
						
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Shared RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
					map_mirrors(System16Ram, region_start, region_end, region_mirror, MAP_RAM);
					
					// second 68k rom
					compute_region(index, 0x060000, 0xf00000, 0x000000);
					SekMapMemory(System16Rom2, region_start, region_end, MAP_ROM);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("68K #2 ROM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					map_mirrors(System16Rom2, region_start, region_end, region_mirror, MAP_ROM);
					
					break;
				}
				
				case 4: {
					compute_region(index, 0x010000, 0xf00000, 0x090000);
					chip.io_start = region_start;
					chip.io_end = region_end;
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("IO: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					store_mirrors(chip.io_start_mirror, chip.io_end_mirror, region_start, region_end, region_mirror, &chip.io_num_mirrors);
					
					break;
				}
				
				case 3: {
					compute_region(index, 0x001000, 0xff0000, 0x000000);
					SekMapMemory(System16SpriteRam, region_start, region_end, MAP_RAM);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Sprite RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					map_mirrors(System16SpriteRam, region_start, region_end, region_mirror, MAP_RAM);
					
					break;
				}
				
				case 2: {
					compute_region(index, 0x002000, 0xffe000, 0x000000);
					SekMapMemory(System16PaletteRam, region_start, region_end, MAP_RAM);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Palette RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					map_mirrors(System16PaletteRam, region_start, region_end, region_mirror, MAP_RAM);
					
					break;
				}
				
				case 1: {
					compute_region(index, 0x001000, 0xfef000, 0x010000);
					SekMapMemory(System16TextRam, region_start, region_end, MAP_RAM);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Text RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					map_mirrors(System16TextRam, region_start, region_end, region_mirror, MAP_RAM);
					
					compute_region(index, 0x010000, 0xfe0000, 0x000000);
					SekMapMemory(System16TileRam, region_start, region_end, MAP_READ);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Tile RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					chip.tile_ram_start = region_start;
					chip.tile_ram_end = region_end;
					
					store_mirrors(chip.tile_ram_start_mirror, chip.tile_ram_end_mirror, region_start, region_end, region_mirror, &chip.tile_ram_num_mirrors);
					
					break;
				}
				
				case 0: {
					// work ram
					compute_region(index, 0x008000, 0xf98000, 0x060000);
					SekMapMemory(System16ExtraRam, region_start, region_end, MAP_RAM);
						
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Work RAM: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
						
					map_mirrors(System16ExtraRam, region_start, region_end, region_mirror, MAP_RAM);
					
					// rom
					compute_region(index, 0x060000, 0xf80000, 0x000000);
					SekMapMemory(System16Rom, region_start, region_end, MAP_READ);
					
					if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("ROM 0: %x, %x, %x, %x\n"), index, region_start, region_end, region_mirror);
					
					map_mirrors(System16Rom, region_start, region_end, region_mirror, MAP_READ);
					
					if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
						SekMapMemory(((UINT8*)fd1094_userregion), region_start, region_end, MAP_FETCH);
						map_mirrors(((UINT8*)fd1094_userregion), region_start, region_end, region_mirror, MAP_FETCH);
					} else {
						SekMapMemory(System16Code, region_start, region_end, MAP_FETCH);
						map_mirrors(System16Code, region_start, region_end, region_mirror, MAP_FETCH);
					}
					
					break;
				}
			}
		}
	}
}

static UINT16 open_bus_read()
{
	if (open_bus_recurse) return 0xffff;
	
	open_bus_recurse = true;
	UINT16 result = (System16Rom[SekGetPC(0) + 1] << 8) | System16Rom[SekGetPC(0) + 0];
	open_bus_recurse = false;
	return result;
}

static UINT8 chip_read(UINT32 offset, INT32 data_width)
{
	offset &= 0x1f;
	
	switch (offset) {
		case 0x00:
		case 0x01: {
			return chip.regs[offset];
		}
		
		case 0x02: {
			return ((chip.regs[0x02] & 3) == 3) ? 0x00 : 0x0f;
		}
		
		case 0x03: {
			if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM18) {
				return System16MCUData;
			}
			
			return 0xff;
		}
	}
	
	if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Read %x, %x\n"), offset, SekGetPC(0));
	
	if (data_width == 16) return open_bus_read();
	
	return 0xff;
}

static void chip_write(UINT32 offset, UINT8 data)
{
	offset &= 0x1f;
	
	UINT8 oldval = chip.regs[offset];
	chip.regs[offset] = data;
	
	switch (offset) {
		case 0x02: {
			if ((oldval ^ chip.regs[offset]) & 3)
			{
				if ((chip.regs[offset] & 3) == 3) {
					if (LOG_MAPPER) bprintf(PRINT_IMPORTANT, _T("Write forced reset\n"));
					
					System1668KEnable = false;
					
					if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_FD1094_ENC) {
						SekClose();
						fd1094_machine_init();
						SekOpen(0);
					}
					
					INT32 active_cpu = SekGetActive();
					if (active_cpu == -1) {
						SekOpen(0);
						SekReset();
						SekClose();
					} else {
						SekReset();
					}
				} else {
					if (LOG_MAPPER) bprintf(PRINT_IMPORTANT, _T("Write cleared reset\n"));
					System1668KEnable = true;
				}
			}
			break;
		}
		
		case 0x03: {
			System16SoundLatch = data;
			
			if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM16B) {
				if ((BurnDrvGetHardwareCode() & HARDWARE_SEGA_YM2413) == 0) {
					ZetOpen(0);
					ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
					ZetClose();
				}
			}
			
			if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM18) {
				ZetOpen(0);
				ZetNmi();
				ZetClose();
			}
			
			if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_OUTRUN) {
				ZetOpen(0);
				ZetNmi();
				nSystem16CyclesDone[2] += ZetRun(200);
				ZetClose();
			}
			
			break;
		}
		
		case 0x04: {
			if ((chip.regs[offset] & 7) != 7) {
				if (System1668KEnable) {
					for (INT32 irqnum = 0; irqnum < 8; irqnum++) {
						if (irqnum == (~chip.regs[offset] & 7)) {
							if (LOG_MAPPER) bprintf(PRINT_IMPORTANT, _T("Mapper: Triggering IRQ %i \n"), irqnum);
							
							if (System16I8751RomNum && irqnum == 4) {
								// the I8751 does the vblank IRQ
								SekSetIRQLine(irqnum, CPU_IRQSTATUS_ACK);
								nSystem16CyclesDone[0] += SekRun(200);
								SekSetIRQLine(irqnum, CPU_IRQSTATUS_NONE);
							} else {
								// normal - just raise it
								SekSetIRQLine(irqnum, CPU_IRQSTATUS_ACK);
							}
						} else {
							SekSetIRQLine(irqnum, CPU_IRQSTATUS_NONE);
							
							if (LOG_MAPPER) bprintf(PRINT_IMPORTANT, _T("Mapper: Clearing IRQ %i\n"), irqnum);
						}
					}
				}
			}
			break;
		}
		
		case 0x05: {
			if (data == 0x01) {
				UINT32 addr = (chip.regs[0x0a] << 17) | (chip.regs[0x0b] << 9) | (chip.regs[0x0c] << 1);
				SekWriteWord(addr, (chip.regs[0x00] << 8) | chip.regs[0x01]);
				
				if (LOG_MAPPER) bprintf(PRINT_IMPORTANT, _T("Mapper Chip Write Word %06x . %04x\n"), addr, (chip.regs[0x00] << 8) | chip.regs[0x01]);
			} else if (data == 0x02) {
				UINT32 addr = (chip.regs[0x07] << 17) | (chip.regs[0x08] << 9) | (chip.regs[0x09] << 1);
				UINT16 result = SekReadWord(addr);
				
				if (LOG_MAPPER) bprintf(PRINT_IMPORTANT, _T("Mapper Chip Read Word %06x\n"), addr);
				
				chip.regs[0x00] = result >> 8;
				chip.regs[0x01] = result;
			}
			break;
		}
		
		case 0x07:
		case 0x08:
		case 0x09: {
			// writes here latch a 68000 address for writing
			break;
		}
		
		case 0x0a:
		case 0x0b:
		case 0x0c: {
			// writes here latch a 68000 address for reading
			break;
		}
		
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x1f: {
			if (oldval != data) update_mapping();
			break;
		}
		
		default: {
			//bprintf(PRINT_NORMAL, _T("Write %x, %x\n"), offset, data);
		}
	}
}

UINT8 sega_315_5195_io_read(UINT32 offset)
{
	offset &= 0x1fff;
			
	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM16B) {
		if (BurnDrvGetHardwareCode() & HARDWARE_SEGA_YM2413) {
			switch (offset & (0x3000 / 2)) {
				case 0x1000 / 2: {
					if ((offset & 3) == 1) return 0xff - System16Input[1];
					if ((offset & 3) == 2) return System16Dip[0];
					if ((offset & 3) == 3) return System16Dip[1];
					return 0xff - System16Input[0];
				}
			}
		} else {
			switch (offset & (0x3000 / 2)) {
				case 0x1000 / 2: {
					if ((offset & 3) == 1) return 0xff - System16Input[1];
					if ((offset & 3) == 2) return System16Dip[2];
					if ((offset & 3) == 3) return 0xff - System16Input[2];
					return 0xff - System16Input[0];
				}
				
				case 0x2000 / 2: {
					if ((offset & 1) == 1) return System16Dip[1];
					return System16Dip[0];
				}
			}
		}
	}
	
	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM18) {
		switch (offset & (0x3000 / 2)) {
			case 0x0000 / 2:
			case 0x1000 / 2: {
				return system18_io_chip_r(offset);
			}
		}
	}
	
	return open_bus_read();
}

static UINT16 math1_5797_read(UINT32 offset)
{
	offset &= 0x1fff;
			
	switch (offset & (0x3000 / 2)) {
		case 0x0000 / 2: {
			return System16MultiplyChipRead(0, offset);
		}
		
		case 0x1000 / 2: {
			return System16CompareTimerChipRead(0, offset);
		}
	}
	
	return open_bus_read();
}

void sega_315_5195_io_write(UINT32 offset, UINT8 d)
{
	offset &= 0x1fff;
	
	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM16B) {
		switch (offset & (0x3000 / 2)) {
			case 0x0000 / 2: {
				System16VideoEnable = d & 0x20;
				System16ScreenFlip = d & 0x40;
				return;
			}
		}
	}
	
	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM18) {
		switch (offset & (0x3000 / 2)) {
			case 0x0000 / 2:
			case 0x1000 / 2: {
				system18_io_chip_w(offset, d);
				return;
			}
			
			case 0x2000 / 2: {
				System18VdpMixing = d & 0xff;
				return;
			}
		}
	}
}

static void bank_5704_write(UINT32 offset, UINT8 d)
{
	offset &= 0x01;
			
	if (System16TileBanks[offset] != (d & 0x07)) {
		System16TileBanks[offset] = d & 0x07;
		System16RecalcBgTileMap = 1;
		System16RecalcBgAltTileMap = 1;
		System16RecalcFgTileMap = 1;
		System16RecalcFgAltTileMap = 1;
	}
}

static void korean_sound_write(UINT32 offset, UINT8 d)
{
	switch (offset) {
		case 0x00: {
			BurnYM2413Write(0, d);
			return;
		}
		
		case 0x01: {
			BurnYM2413Write(1, d);
			return;
		}
	}
}

static void math1_5797_write(UINT32 offset, UINT16 d)
{
	offset &= 0x1fff;
			
	switch (offset & (0x3000 / 2)) {
		case 0x0000 / 2: {
			System16MultiplyChipWrite(0, offset, d);
			return;
		}
		
		case 0x1000 / 2: {
			System16CompareTimerChipWrite(0, offset, d);
			return;
		}
		
		case 0x2000 / 2: {
			offset &= 0x01;
	
			if (System16TileBanks[offset] != (d & 0x07)) {
				System16TileBanks[offset] = d & 0x07;
				System16RecalcBgTileMap = 1;
				System16RecalcBgAltTileMap = 1;
				System16RecalcFgTileMap = 1;
				System16RecalcFgAltTileMap = 1;
			}
			return;
		}
	}
}

UINT8 __fastcall sega_315_5195_read_byte(UINT32 a)
{
	if (chip.io_start > 0) {
		if (a >= chip.io_start && a <= chip.io_end) {
			UINT16 offset = (a - chip.io_start) >> 1;
			if (sega_315_5195_custom_io_do) return sega_315_5195_custom_io_do(offset);
			return sega_315_5195_io_read(offset);
		}
	}
	
	if (chip.genesis_vdp_start > 0) {
		if (a >= chip.genesis_vdp_start && a <= chip.genesis_vdp_end) {
			UINT16 offset = (a - chip.genesis_vdp_start) >> 1;
			return GenesisVDPRead(offset) & 0xff;
		}
	}
	
	if (chip.road_ctrl_start > 0) {
		if (a >= chip.road_ctrl_start && a <= chip.road_ctrl_end) {
			UINT16 offset = (a - chip.road_ctrl_start) >> 1;
			return System16RoadControlRead(offset) & 0xff;
		}
	}
	
	for (INT32 i = 0; i < chip.io_num_mirrors; i++) {
		if (chip.io_start_mirror[i] > 0) {
			if (a >= chip.io_start_mirror[i] && a <= chip.io_end_mirror[i]) {
				UINT16 offset = (a - chip.io_start_mirror[i]) >> 1;
				if (sega_315_5195_custom_io_do) return sega_315_5195_custom_io_do(offset);
				return sega_315_5195_io_read(offset);
			}
		}
	}
		
	for (INT32 i = 0; i < chip.genesis_vdp_num_mirrors; i++) {
		if (chip.genesis_vdp_start_mirror[i] > 0) {
			if (a >= chip.genesis_vdp_start_mirror[i] && a <= chip.genesis_vdp_end_mirror[i]) {
				UINT16 offset = (a - chip.genesis_vdp_start_mirror[i]) >> 1;
				return GenesisVDPRead(offset) & 0xff;
			}
		}
	}
		
	for (INT32 i = 0; i < chip.road_ctrl_num_mirrors; i++) {	
		if (chip.road_ctrl_start_mirror[i] > 0) {
			if (a >= chip.road_ctrl_start_mirror[i] && a <= chip.road_ctrl_end_mirror[i]) {
				UINT16 offset = (a - chip.road_ctrl_start_mirror[i]) >> 1;
				return System16RoadControlRead(offset) & 0xff;
			}
		}
	}
	
	if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Read Byte 0x%06X\n"), a);

	return chip_read(a >> 1, 16);
}

UINT16 __fastcall sega_315_5195_read_word(UINT32 a)
{
	if (chip.io_start > 0) {
		if (a >= chip.io_start && a <= chip.io_end) {
			UINT16 offset = (a - chip.io_start) >> 1;
			if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM18) {
				if (sega_315_5195_custom_io_do) return sega_315_5195_custom_io_do(offset) | (open_bus_read() & 0xff00);
				return sega_315_5195_io_read(offset) | (open_bus_read() & 0xff00);
			}
			if (sega_315_5195_custom_io_do) return sega_315_5195_custom_io_do(offset);
			return sega_315_5195_io_read(offset);
		}
	}
	
	if (chip.math1_5797_start > 0) {
		if (a >= chip.math1_5797_start && a <= chip.math1_5797_end) {
			UINT16 offset = (a - chip.math1_5797_start) >> 1;
			return math1_5797_read(offset);
		}
	}
	
	if (chip.math2_5797_start > 0) {
		if (a >= chip.math2_5797_start && a <= chip.math2_5797_end) {
			UINT16 offset = (a - chip.math2_5797_start) >> 1;
			return System16CompareTimerChipRead(1, offset);
		}
	}
	
	if (chip.genesis_vdp_start > 0) {
		if (a >= chip.genesis_vdp_start && a <= chip.genesis_vdp_end) {
			UINT16 offset = (a - chip.genesis_vdp_start) >> 1;
			return GenesisVDPRead(offset);
		}
	}
	
	if (chip.road_ctrl_start > 0) {
		if (a >= chip.road_ctrl_start && a <= chip.road_ctrl_end) {
			UINT16 offset = (a - chip.road_ctrl_start) >> 1;
			return System16RoadControlRead(offset);
		}
	}
	
	for (INT32 i = 0; i < chip.io_num_mirrors; i++) {
		if (chip.io_start_mirror[i] > 0) {
			if (a >= chip.io_start_mirror[i] && a <= chip.io_end_mirror[i]) {
				UINT16 offset = (a - chip.io_start_mirror[i]) >> 1;
				if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM18) {
					if (sega_315_5195_custom_io_do) return sega_315_5195_custom_io_do(offset) | (open_bus_read() & 0xff00);
					return sega_315_5195_io_read(offset) | (open_bus_read() & 0xff00);
				}
				if (sega_315_5195_custom_io_do) return sega_315_5195_custom_io_do(offset);
				return sega_315_5195_io_read(offset);
			}
		}
	}
	
	for (INT32 i = 0; i < chip.math1_5797_num_mirrors; i++) {
		if (chip.math1_5797_start_mirror[i] > 0) {
			if (a >= chip.math1_5797_start_mirror[i] && a <= chip.math1_5797_end_mirror[i]) {
				UINT16 offset = (a - chip.math1_5797_start_mirror[i]) >> 1;
				return math1_5797_read(offset);
			}
		}
	}
	
	for (INT32 i = 0; i < chip.math2_5797_num_mirrors; i++) {
		if (chip.math2_5797_start_mirror[i] > 0) {
			if (a >= chip.math2_5797_start_mirror[i] && a <= chip.math2_5797_end_mirror[i]) {
				UINT16 offset = (a - chip.math2_5797_start_mirror[i]) >> 1;
				return System16CompareTimerChipRead(1, offset);
			}
		}
	}
		
	for (INT32 i = 0; i < chip.genesis_vdp_num_mirrors; i++) {
		if (chip.genesis_vdp_start_mirror[i] > 0) {
			if (a >= chip.genesis_vdp_start_mirror[i] && a <= chip.genesis_vdp_end_mirror[i]) {
				UINT16 offset = (a - chip.genesis_vdp_start_mirror[i]) >> 1;
				return GenesisVDPRead(offset);
			}
		}
	}
		
	for (INT32 i = 0; i < chip.road_ctrl_num_mirrors; i++) {
		if (chip.road_ctrl_start_mirror[i] > 0) {
			if (a >= chip.road_ctrl_start_mirror[i] && a <= chip.road_ctrl_end_mirror[i]) {
				UINT16 offset = (a - chip.road_ctrl_start_mirror[i]) >> 1;
				return System16RoadControlRead(offset) & 0xff;
			}
		}
	}
	
	if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Read Word 0x%06X\n"), a);

	return chip_read(a >> 1, 16);
}

void __fastcall sega_315_5195_write_byte(UINT32 a, UINT8 d)
{
	if (chip.tile_ram_start > 0) {
		if (a >= chip.tile_ram_start && a <= chip.tile_ram_end) {
			System16BTileByteWrite((a - chip.tile_ram_start) ^ 1, d);
			return;
		}
	}
	
	if (chip.io_start > 0) {
		if (a >= chip.io_start && a <= chip.io_end) {
			UINT16 offset = (a - chip.io_start) >> 1;
			if (sega_315_5195_custom_io_write_do) {
				sega_315_5195_custom_io_write_do(offset, d);
			} else {
				sega_315_5195_io_write(offset, d);
			}
			return;
		}
	}
	
	if (chip.bank_5704_start > 0) {
		if (a >= chip.bank_5704_start && a <= chip.bank_5704_end) {
			UINT16 offset = (a - chip.bank_5704_start) >> 1;
			bank_5704_write(offset, d);
			return;
		}	
	}
	
	if (chip.korean_sound_start > 0) {
		if (a >= chip.korean_sound_start && a <= chip.korean_sound_end) {
			UINT16 offset = (a - chip.korean_sound_start) >> 1;
			korean_sound_write(offset, d);
			return;
		}
	}
	
	if (chip.math1_5797_start > 0) {
		if (a >= chip.math1_5797_start && a <= chip.math1_5797_end) {
			UINT16 offset = (a - chip.math1_5797_start) >> 1;
			math1_5797_write(offset, d);
			return;
		}
	}
	
	if (chip.math2_5797_start > 0) {
		if (a >= chip.math2_5797_start && a <= chip.math2_5797_end) {
			UINT16 offset = (a - chip.math2_5797_start) >> 1;
			System16CompareTimerChipWrite(1, offset, d);
			return;
		}
	}
	
	if (chip.genesis_vdp_start > 0) {
		if (a >= chip.genesis_vdp_start && a <= chip.genesis_vdp_end) {
			UINT16 offset = (a - chip.genesis_vdp_start) >> 1;
			GenesisVDPWrite(offset, d);
			return;
		}
	}
	
	if (chip.bank_5987_start > 0) {
		if (a >= chip.bank_5987_start && a <= chip.bank_5987_end) {
			UINT16 offset = (a - chip.bank_5987_start) >> 1;
			System18GfxBankWrite(offset, d);
			return;
		}	
	}
	
	if (chip.bank_7525_start > 0) {
		if (a >= chip.bank_7525_start && a <= chip.bank_7525_end) {
			UINT16 offset = (a - chip.bank_7525_start) >> 1;
			HamawayGfxBankWrite(offset, d);
			return;
		}	
	}
	
	if (chip.road_ctrl_start > 0) {
		if (a >= chip.road_ctrl_start && a <= chip.road_ctrl_end) {
			UINT16 offset = (a - chip.road_ctrl_start) >> 1;
			System16RoadControlWrite(offset, d);
			return;
		}
	}
	
	for (INT32 i = 0; i < chip.io_num_mirrors; i++) {
		if (chip.io_start_mirror[i] > 0) {
			if (a >= chip.io_start_mirror[i] && a <= chip.io_end_mirror[i]) {
				UINT16 offset = (a - chip.io_start_mirror[i]) >> 1;
				if (sega_315_5195_custom_io_write_do) {
					sega_315_5195_custom_io_write_do(offset, d);
				} else {
					sega_315_5195_io_write(offset, d);
				}
				return;
			}
		}
	}
		
	for (INT32 i = 0; i < chip.tile_ram_num_mirrors; i++) {
		if (chip.tile_ram_start_mirror[i] > 0) {
			if (a >= chip.tile_ram_start_mirror[i] && a <= chip.tile_ram_end_mirror[i]) {
				System16BTileByteWrite((a - chip.tile_ram_start_mirror[i]) ^ 1, d);
				return;
			}
		}
	}
		
	for (INT32 i = 0; i < chip.bank_5704_num_mirrors; i++) {
		if (chip.bank_5704_start_mirror[i] > 0) {
			if (a >= chip.bank_5704_start_mirror[i] && a <= chip.bank_5704_end_mirror[i]) {
				UINT16 offset = (a - chip.bank_5704_start_mirror[i]) >> 1;
				bank_5704_write(offset, d);
				return;
			}	
		}
	}
		
	for (INT32 i = 0; i < chip.korean_sound_num_mirrors; i++) {
		if (chip.korean_sound_start > 0) {
			if (a >= chip.korean_sound_start_mirror[i] && a <= chip.korean_sound_end_mirror[i]) {
				UINT16 offset = (a - chip.korean_sound_start_mirror[i]) >> 1;
				korean_sound_write(offset, d);
				return;
			}
		}
	}
		
	for (INT32 i = 0; i < chip.math1_5797_num_mirrors; i++) {
		if (chip.math1_5797_start_mirror[i] > 0) {
			if (a >= chip.math1_5797_start_mirror[i] && a <= chip.math1_5797_end_mirror[i]) {
				UINT16 offset = (a - chip.math1_5797_start_mirror[i]) >> 1;
				math1_5797_write(offset, d);
				return;
			}
		}
	}
		
	for (INT32 i = 0; i < chip.math2_5797_num_mirrors; i++) {
		if (chip.math2_5797_start_mirror[i] > 0) {
			if (a >= chip.math2_5797_start_mirror[i] && a <= chip.math2_5797_end_mirror[i]) {
				UINT16 offset = (a - chip.math2_5797_start_mirror[i]) >> 1;
				System16CompareTimerChipWrite(1, offset, d);
				return;
			}
		}
	}
		
	for (INT32 i = 0; i < chip.genesis_vdp_num_mirrors; i++) {
		if (chip.genesis_vdp_start_mirror[i] > 0) {
			if (a >= chip.genesis_vdp_start_mirror[i] && a <= chip.genesis_vdp_end_mirror[i]) {
				UINT16 offset = (a - chip.genesis_vdp_start_mirror[i]) >> 1;
				GenesisVDPWrite(offset, d);
				return;
			}
		}
	}
		
	for (INT32 i = 0; i < chip.bank_5987_num_mirrors; i++) {
		if (chip.bank_5987_start_mirror[i] > 0) {
			if (a >= chip.bank_5987_start_mirror[i] && a <= chip.bank_5987_end_mirror[i]) {
				UINT16 offset = (a - chip.bank_5987_start_mirror[i]) >> 1;
				System18GfxBankWrite(offset, d);
				return;
			}	
		}
	}
		
	for (INT32 i = 0; i < chip.bank_7525_num_mirrors; i++) {
		if (chip.bank_7525_start_mirror[i] > 0) {
			if (a >= chip.bank_7525_start_mirror[i] && a <= chip.bank_7525_end_mirror[i]) {
				UINT16 offset = (a - chip.bank_7525_start_mirror[i]) >> 1;
				HamawayGfxBankWrite(offset, d);
				return;
			}	
		}
	}
		
	for (INT32 i = 0; i < chip.road_ctrl_num_mirrors; i++) {
		if (chip.road_ctrl_start_mirror[i] > 0) {
			if (a >= chip.road_ctrl_start_mirror[i] && a <= chip.road_ctrl_end_mirror[i]) {
				UINT16 offset = (a - chip.road_ctrl_start_mirror[i]) >> 1;
				System16RoadControlWrite(offset, d);
				return;
			}
		}
	}
	
	if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Write Byte 0x%06X, 0x%02X\n"), a, d);
	
	if (LaserGhost && a == 0xfe0008 && d == 0x00) return;
	
	chip_write(a >> 1, d & 0xff);
}

void __fastcall sega_315_5195_write_word(UINT32 a, UINT16 d)
{
	if (chip.tile_ram_start > 0) {
		if (a >= chip.tile_ram_start && a <= chip.tile_ram_end) {
			System16BTileWordWrite(a - chip.tile_ram_start, d);
			return;
		}
	}
	
	if (chip.io_start > 0) {
		if (a >= chip.io_start && a <= chip.io_end) {
			UINT16 offset = (a - chip.io_start) >> 1;
			if (sega_315_5195_custom_io_write_do) {
				sega_315_5195_custom_io_write_do(offset, d & 0xff);
			} else {
				sega_315_5195_io_write(offset, d & 0xff);
			}
			return;
		}
	}
	
	if (chip.bank_5704_start > 0) {
		if (a >= chip.bank_5704_start && a <= chip.bank_5704_end) {
			UINT16 offset = (a - chip.bank_5704_start) >> 1;
			bank_5704_write(offset, d & 0xff);
			return;
		}	
	}
	
	if (chip.math1_5797_start > 0) {
		if (a >= chip.math1_5797_start && a <= chip.math1_5797_end) {
			UINT16 offset = (a - chip.math1_5797_start) >> 1;
			math1_5797_write(offset, d);
			return;
		}
	}
	
	if (chip.math2_5797_start > 0) {
		if (a >= chip.math2_5797_start && a <= chip.math2_5797_end) {
			UINT16 offset = (a - chip.math2_5797_start) >> 1;
			System16CompareTimerChipWrite(1, offset, d);
			return;
		}
	}
	
	if (chip.genesis_vdp_start > 0) {
		if (a >= chip.genesis_vdp_start && a <= chip.genesis_vdp_end) {
			UINT16 offset = (a - chip.genesis_vdp_start) >> 1;
			GenesisVDPWrite(offset, d);
			return;
		}
	}
	
	if (chip.bank_5987_start > 0) {
		if (a >= chip.bank_5987_start && a <= chip.bank_5987_end) {
			UINT16 offset = (a - chip.bank_5987_start) >> 1;
			System18GfxBankWrite(offset, d);
			return;
		}	
	}
	
	if (chip.bank_7525_start > 0) {
		if (a >= chip.bank_7525_start && a <= chip.bank_7525_end) {
			UINT16 offset = (a - chip.bank_7525_start) >> 1;
			HamawayGfxBankWrite(offset, d);
			return;
		}	
	}
	
	if (chip.road_ctrl_start > 0) {
		if (a >= chip.road_ctrl_start && a <= chip.road_ctrl_end) {
			UINT16 offset = (a - chip.road_ctrl_start) >> 1;
			System16RoadControlWrite(offset, d);
			return;
		}
	}
	
	for (INT32 i = 0; i < chip.io_num_mirrors; i++) {
		if (chip.io_start_mirror[i] > 0) {
			if (a >= chip.io_start_mirror[i] && a <= chip.io_end_mirror[i]) {
				UINT16 offset = (a - chip.io_start_mirror[i]) >> 1;
				if (sega_315_5195_custom_io_write_do) {
					sega_315_5195_custom_io_write_do(offset, d & 0xff);
				} else {
					sega_315_5195_io_write(offset, d & 0xff);
				}
				return;
			}
		}
	}
		
	for (INT32 i = 0; i < chip.tile_ram_num_mirrors; i++) {
		if (chip.tile_ram_start_mirror[i] > 0) {
			if (a >= chip.tile_ram_start_mirror[i] && a <= chip.tile_ram_end_mirror[i]) {
				System16BTileWordWrite(a - chip.tile_ram_start_mirror[i], d);
				return;
			}
		}
	}
		
	for (INT32 i = 0; i < chip.bank_5704_num_mirrors; i++) {
		if (chip.bank_5704_start_mirror[i] > 0) {
			if (a >= chip.bank_5704_start_mirror[i] && a <= chip.bank_5704_end_mirror[i]) {
				UINT16 offset = (a - chip.bank_5704_start_mirror[i]) >> 1;
				bank_5704_write(offset, d & 0xff);
				return;
			}	
		}
	}
		
	for (INT32 i = 0; i < chip.math1_5797_num_mirrors; i++) {
		if (chip.math1_5797_start_mirror[i] > 0) {
			if (a >= chip.math1_5797_start_mirror[i] && a <= chip.math1_5797_end_mirror[i]) {
				UINT16 offset = (a - chip.math1_5797_start_mirror[i]) >> 1;
				math1_5797_write(offset, d);
				return;
			}
		}
	}
		
	for (INT32 i = 0; i < chip.math2_5797_num_mirrors; i++) {
		if (chip.math2_5797_start_mirror[i] > 0) {
			if (a >= chip.math2_5797_start_mirror[i] && a <= chip.math2_5797_end_mirror[i]) {
				UINT16 offset = (a - chip.math2_5797_start_mirror[i]) >> 1;
				System16CompareTimerChipWrite(1, offset, d);
				return;
			}
		}
	}
		
	for (INT32 i = 0; i < chip.genesis_vdp_num_mirrors; i++) {
		if (chip.genesis_vdp_start_mirror[i] > 0) {
			if (a >= chip.genesis_vdp_start_mirror[i] && a <= chip.genesis_vdp_end_mirror[i]) {
				UINT16 offset = (a - chip.genesis_vdp_start_mirror[i]) >> 1;
				GenesisVDPWrite(offset, d);
				return;
			}
		}
	}
	
	for (INT32 i = 0; i < chip.bank_5987_num_mirrors; i++) {
		if (chip.bank_5987_start_mirror[i] > 0) {
			if (a >= chip.bank_5987_start_mirror[i] && a <= chip.bank_5987_end_mirror[i]) {
				UINT16 offset = (a - chip.bank_5987_start_mirror[i]) >> 1;
				System18GfxBankWrite(offset, d);
				return;
			}	
		}
	}
	
	for (INT32 i = 0; i < chip.bank_7525_num_mirrors; i++) {
		if (chip.bank_7525_start_mirror[i] > 0) {
			if (a >= chip.bank_7525_start_mirror[i] && a <= chip.bank_7525_end_mirror[i]) {
				UINT16 offset = (a - chip.bank_7525_start_mirror[i]) >> 1;
				HamawayGfxBankWrite(offset, d);
				return;
			}	
		}
	}
	
	for (INT32 i = 0; i < chip.road_ctrl_num_mirrors; i++) {
		if (chip.road_ctrl_start_mirror[i] > 0) {
			if (a >= chip.road_ctrl_start_mirror[i] && a <= chip.road_ctrl_end_mirror[i]) {
				UINT16 offset = (a - chip.road_ctrl_start_mirror[i]) >> 1;
				System16RoadControlWrite(offset, d);
				return;
			}
		}
	}
	
	if (LOG_MAPPER) bprintf(PRINT_NORMAL, _T("Write Word 0x%06X, 0x%04X\n"), a, d);
	
	chip_write(a >> 1, d & 0xff);
}

UINT8 sega_315_5195_i8751_read_port(INT32 port)
{
	switch (port) {
		case MCS51_PORT_P1: {
			if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM16B) {
				return 0xff - System16Input[0];
			}
			return 0;
		}
		
		case 0xff00:
		case 0xff01:
		case 0xff02:
		case 0xff03:
		case 0xff04:
		case 0xff05:
		case 0xff06:
		case 0xff07:
		case 0xff08:
		case 0xff09:
		case 0xff0a:
		case 0xff0b:
		case 0xff0c:
		case 0xff0d:
		case 0xff0e:
		case 0xff0f:
		case 0xff10:
		case 0xff11:
		case 0xff12:
		case 0xff13:
		case 0xff14:
		case 0xff15:
		case 0xff16:
		case 0xff17:
		case 0xff18:
		case 0xff19:
		case 0xff1a:
		case 0xff1b:
		case 0xff1c:
		case 0xff1d:
		case 0xff1e:
		case 0xff1f: {
			return chip_read((UINT32)port, 8);
		}
	}
		
	return 0;
}

void sega_315_5195_i8751_write_port(INT32 port, UINT8 data)
{
	switch (port) {
		case MCS51_PORT_P1: {
			if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_SYSTEM16B) {
				if (System1668KEnable) {
					INT32 active_cpu = SekGetActive();
					if (active_cpu == -1) {
						SekOpen(0);
						nSystem16CyclesDone[0] += SekRun(10000);
						SekClose();
					} else {
						nSystem16CyclesDone[0] += SekRun(10000);
					}
				}
			}
			
			return;
		}
		
		case 0xff00:
		case 0xff01:
		case 0xff02:
		case 0xff03:
		case 0xff04:
		case 0xff05:
		case 0xff06:
		case 0xff07:
		case 0xff08:
		case 0xff09:
		case 0xff0a:
		case 0xff0b:
		case 0xff0c:
		case 0xff0d:
		case 0xff0e:
		case 0xff0f:
		case 0xff10:
		case 0xff11:
		case 0xff12:
		case 0xff13:
		case 0xff14:
		case 0xff15:
		case 0xff16:
		case 0xff17:
		case 0xff18:
		case 0xff19:
		case 0xff1a:
		case 0xff1b:
		case 0xff1c:
		case 0xff1d:
		case 0xff1e:
		case 0xff1f: {
			chip_write((UINT32)port, data);
			return;
		}
	}
}

void sega_315_5195_reset()
{
	if (mapper_in_use) {
		update_mapping();
	
		open_bus_recurse = false;
	}
}

void sega_315_5195_configure_explicit(UINT8 *map_data)
{
	memcpy(&chip.regs[0x10], map_data, 0x10);
	update_mapping();
}

void sega_315_5195_init()
{
	memset((void*)&chip, 0, sizeof(sega_315_5195_struct));
	
	mapper_in_use = true;
}

void sega_315_5195_exit()
{
	memset((void*)&chip, 0, sizeof(sega_315_5195_struct));
	
	region_start = 0;
	region_end = 0;
	region_mirror = 0;
	open_bus_recurse = false;
	mapper_in_use = false;
	
	sega_315_5195_custom_io_do = NULL;
	sega_315_5195_custom_io_write_do = NULL;
}

INT32 sega_315_5195_scan(INT32 nAction)
{
	if (!mapper_in_use) return 0;
	
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(chip.regs);
		SCAN_VAR(open_bus_recurse);
	
		if (nAction & ACB_WRITE) {
			SekOpen(0);
			update_mapping();
			SekClose();
		}
	}
	
	return 0;
}
