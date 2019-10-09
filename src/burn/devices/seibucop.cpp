#include "burnint.h"
#include "m68000_intf.h"
#include "nec_intf.h"

#if defined (_MSC_VER)
#define _USE_MATH_DEFINES
#endif
#include <math.h>

//#define HARDSEK - hard-config seibucop to use 68k only!

static void (*cop_paletteram_cb)(INT32 entry, UINT16 val);
static void (*cop_videoram_cb)(INT32 offset, UINT16 val, UINT16 mask);

#ifdef HARDSEK
#define cpu_write_long(x,y)	SekWriteLong(x,y)
#define cpu_write_word(x,y) SekWriteWord(x,y)
#define cpu_write_byte(x,y) SekWriteByte(x,y)
#define cpu_read_long(x) SekReadLong(x)
#define cpu_read_word(x) SekReadWord(x)
#define cpu_read_byte(x) SekReadByte(x)
#else
static void (*cpu_write_long)(UINT32,UINT32);
static void (*cpu_write_word)(UINT32,UINT16);
static void (*cpu_write_byte)(UINT32,UINT8);
static UINT32 (*cpu_read_long)(UINT32);
static UINT16 (*cpu_read_word)(UINT32);
static UINT8 (*cpu_read_byte)(UINT32);
#endif

static INT32 host_endian = 0;
static INT32 byte_endian_val = 0;
static INT32 word_endian_val = 0;

// variables!!
static UINT32 cop_sprite_dma_param;
static INT32  cop_sprite_dma_size;
static UINT32 cop_sprite_dma_src;
static INT32 cop_sprite_dma_abs_y;
static INT32 cop_sprite_dma_abs_x;
static INT32 cop_sprite_dma_rel_x;
static INT32 cop_sprite_dma_rel_y;
static UINT16 cop_status;
static UINT16 cop_angle_target;
static UINT16 cop_angle_step;
static UINT16 cop_angle;
static UINT16 cop_dist;
static UINT32 cop_itoa;
static UINT16 cop_itoa_mode;
static UINT8 cop_itoa_digits[10];

static UINT32 cop_regs[8];

static UINT16 cop_dma_v1;
static UINT16 cop_dma_v2;
static UINT16 cop_rng_max_value;

static UINT16 cop_hit_baseadr;
static UINT8 cop_scale;

static UINT16 cop_rom_addr_hi;
static UINT16 cop_rom_addr_lo;
static UINT16 cop_precmd;

static UINT32 cop_sort_ram_addr;
static UINT32 cop_sort_lookup;
static UINT16 cop_sort_param;

static UINT16 pal_brightness_val;
static UINT16 pal_brightness_mode;

static UINT16 cop_dma_adr_rel;
static UINT16 cop_dma_mode;
static UINT16 cop_dma_src[0x200];
static UINT16 cop_dma_size[0x200];
static UINT16 cop_dma_dst[0x200];

static UINT16 cop_hit_status;
static UINT16 cop_hit_val_stat;
static INT16 cop_hit_val[3];

static UINT16 cop_latch_value;
static UINT16 cop_latch_addr;
static UINT16 cop_latch_mask;
static UINT16 cop_latch_trigger;
static UINT16 cop_func_value[256/8];
static UINT16 cop_func_mask[256/8];
static UINT16 cop_func_trigger[256/8];
static UINT16 cop_program[256];

static INT32 LEGACY_r0;
static INT32 LEGACY_r1;

struct colinfo {
	INT16 pos[3];
	INT8 dx[3];
	UINT8 size[3];
	bool allow_swap;
	UINT16 flags_swap;
	UINT32 spradr;
	INT16 min[3], max[3];
};

static colinfo cop_collision_info[3];

static void cop_write_word(UINT32 offset, UINT16 data)
{
	cpu_write_word(offset ^ word_endian_val, data);
}

static void cop_write_byte(UINT32 offset, UINT8 data)
{
	cpu_write_byte(offset ^ byte_endian_val, data);
}

static UINT16 cop_read_word(UINT32 offset)
{
	return cpu_read_word(offset ^ word_endian_val);
}

static UINT8 cop_read_byte(UINT32 offset)
{
	return cpu_read_byte(offset ^ byte_endian_val);
}

static void execute_0205(INT32 offset, UINT16 )
{
	INT32 ppos =        cpu_read_long(cop_regs[0] + 0x04 + offset * 4);
	INT32 npos = ppos + cpu_read_long(cop_regs[0] + 0x10 + offset * 4);
	INT32 delta = (npos >> 16) - (ppos >> 16);

	cpu_write_long(cop_regs[0] + 4 + offset * 4, npos);
	cop_write_word(cop_regs[0] + 0x1e + offset * 4, cop_read_word(cop_regs[0] + 0x1e + offset * 4) + delta);
}

static void execute_0904(INT32 offset, UINT16 data)
{
	if (data&0x0001)
		cpu_write_long(cop_regs[0] + 16 + offset * 4, cpu_read_long(cop_regs[0] + 16 + offset * 4) + cpu_read_long(cop_regs[0] + 0x28 + offset * 4));
	else /* X Se Dae and Zero Team uses this variant */
		cpu_write_long(cop_regs[0] + 16 + offset * 4, cpu_read_long(cop_regs[0] + 16 + offset * 4) - cpu_read_long(cop_regs[0] + 0x28 + offset * 4));
}

static void LEGACY_execute_130e_cupsoc(INT32 , UINT16 data)
{
	INT32 dy = cpu_read_long(cop_regs[1] + 4) - cpu_read_long(cop_regs[0] + 4);
	INT32 dx = cpu_read_long(cop_regs[1] + 8) - cpu_read_long(cop_regs[0] + 8);

	cop_status = 7;

	if (!dx) {
		cop_status |= 0x8000;
		cop_angle = 0;
	}
	else
	{
		cop_angle = (int)(atan(double(dy) / double(dx)) * 128.0 / M_PI);
		if (dx < 0)
			cop_angle += 0x80;

		cop_angle &= 0xff;
	}

	LEGACY_r0 = dy;
	LEGACY_r1 = dx;

	if (data & 0x80)
		cop_write_byte(cop_regs[0] + (0x34), cop_angle);
}

static void execute_2288(INT32 , UINT16 data)
{
	INT32 dx = cpu_read_word(cop_regs[0] + 0x12);
	INT32 dy = cpu_read_word(cop_regs[0] + 0x16);

	if (!dy) {
		cop_status |= 0x8000;
		cop_angle = 0;
	}
	else {
		cop_angle = (int)(atan(double(dx) / double(dy)) * 128 / M_PI);
		if (dy < 0)
			cop_angle += 0x80;
	}

	if (data & 0x0080) {
		cpu_write_byte(cop_regs[0] + 0x34, cop_angle);
	}
}

static void execute_2a05(INT32 offset, UINT16 )
{
	INT32 delta = cpu_read_word(cop_regs[1] + 0x1e + offset * 4);
	cpu_write_long(cop_regs[0] + 4 + 2 + offset * 4, cpu_read_word(cop_regs[0] + 4 + 2 + offset * 4) + delta);
	cpu_write_long(cop_regs[0] + 0x1e + offset * 4, cpu_read_word(cop_regs[0] + 0x1e + offset * 4) + delta);
}

static void execute_338e(INT32 , UINT16 data, bool is_yflip)
{
	INT32 dx = cpu_read_long(cop_regs[1] + 4) - cpu_read_long(cop_regs[0] + 4);
	INT32 dy = cpu_read_long(cop_regs[1] + 8) - cpu_read_long(cop_regs[0] + 8);

	cop_status = 7;

	if (!dy) {
		cop_status |= 0x8000;
		cop_angle = 0;
	}
	else {
		cop_angle = (int)((double)atan(double(dx) / double(dy)) * 128 / M_PI);
		if (dy < 0)
			cop_angle += 0x80;

		cop_angle &= 0xff;
	}

	if (data & 0x0080) {
		// TODO: byte or word?
		if(is_yflip == true)
			cop_write_byte(cop_regs[0] + 0x34, cop_angle ^ 0x80);
		else
			cop_write_byte(cop_regs[0] + 0x34, cop_angle);
	}
}

static void execute_130e(INT32 offset, UINT16 data, bool is_yflip)
{
	// this can't be right, or bits 15-12 from mask have different meaning ...
	execute_338e(offset, data, is_yflip);
}

static void execute_3b30(INT32 , UINT16 data)
{
	/* TODO: these are actually internally loaded via 0x130e command */
	INT32 dx, dy;

	dx = cpu_read_long(cop_regs[1] + 4) - cpu_read_long(cop_regs[0] + 4);
	dy = cpu_read_long(cop_regs[1] + 8) - cpu_read_long(cop_regs[0] + 8);

	dx = dx >> 16;
	dy = dy >> 16;
	cop_dist = (UINT16)sqrt((double)(dx*dx + dy*dy));

	if (data & 0x0080)
		cop_write_word(cop_regs[0] + (data & 0x200 ? 0x3a : 0x38), cop_dist);
}

static void execute_42c2(INT32 , UINT16 )
{
	INT32 div = cop_read_word(cop_regs[0] + (0x36));

	if (!div)
	{
		cop_status |= 0x8000;
		cop_write_word(cop_regs[0] + (0x38), 0);
		return;
	}

	/* TODO: bits 5-6-15 */
	cop_status = 7;

	cop_write_word(cop_regs[0] + (0x38), (cop_dist << (5 - cop_scale)) / div);
}


static void execute_4aa0(INT32 , UINT16 )
{
	INT32 div = cpu_read_word(cop_regs[0] + (0x38));
	if (!div)
		div = 1;

	/* TODO: bits 5-6-15 */
	cop_status = 7;

	cpu_write_word(cop_regs[0] + (0x36), (cop_dist << (5 - cop_scale)) / div);
}

static void execute_5205(INT32 , UINT16 )
{
	cpu_write_long(cop_regs[1], cpu_read_long(cop_regs[0]));
}

static void execute_5a05(INT32 , UINT16 )
{
	cpu_write_long(cop_regs[1], cpu_read_long(cop_regs[0]));
}

static void execute_6200(INT32 , UINT16 )
{
	INT32 primary_reg = 0;
	INT32 primary_offset = 0x34;

	UINT8 angle = cop_read_byte(cop_regs[primary_reg] + primary_offset);
	UINT16 flags = cop_read_word(cop_regs[primary_reg]);
	cop_angle_target &= 0xff;
	cop_angle_step &= 0xff;
	flags &= ~0x0004;
	INT32 delta = angle - cop_angle_target;
	if (delta >= 128)
		delta -= 256;
	else if (delta < -128)
		delta += 256;
	if (delta < 0) {
		if (delta >= -cop_angle_step) {
			angle = cop_angle_target;
			flags |= 0x0004;
		}
		else
			angle += cop_angle_step;
	}
	else {
		if (delta <= cop_angle_step) {
			angle = cop_angle_target;
			flags |= 0x0004;
		}
		else
			angle -= cop_angle_step;
	}

	cop_write_word(cop_regs[primary_reg], flags);

	if (!host_endian)
		cop_write_byte(cop_regs[primary_reg] + primary_offset, angle);
	else // angle is a byte, but grainbow (cave mid-boss) is only happy with write-word, could be more endian weirdness, or it always writes a word?
		cop_write_word(cop_regs[primary_reg] + primary_offset, angle);

}

static void LEGACY_execute_6200(INT32 , UINT16 ) // this is for cupsoc, different sequence, works on different registers
{
	INT32 primary_reg = 1;
	INT32 primary_offset = 0xc;

	UINT8 angle = cop_read_byte(cop_regs[primary_reg] + primary_offset);
	UINT16 flags = cop_read_word(cop_regs[primary_reg]);
	cop_angle_target &= 0xff;
	cop_angle_step &= 0xff;
	flags &= ~0x0004;
	INT32 delta = angle - cop_angle_target;
	if (delta >= 128)
		delta -= 256;
	else if (delta < -128)
		delta += 256;
	if (delta < 0) {
		if (delta >= -cop_angle_step) {
			angle = cop_angle_target;
			flags |= 0x0004;
		}
		else
			angle += cop_angle_step;
	}
	else {
		if (delta <= cop_angle_step) {
			angle = cop_angle_target;
			flags |= 0x0004;
		}
		else
			angle -= cop_angle_step;
	}

	cop_write_word(cop_regs[primary_reg], flags);

	if (!host_endian)
		cop_write_byte(cop_regs[primary_reg] + primary_offset, angle);
	else // angle is a byte, but grainbow (cave mid-boss) is only happy with write-word, could be more endian weirdness, or it always writes a word?
		cop_write_word(cop_regs[primary_reg] + primary_offset, angle);
}


static void LEGACY_execute_6980(INT32 offset, UINT16 )
{
	UINT8 offs;
	INT32 rel_xy;

	offs = (offset & 3) * 4;

	rel_xy = cpu_read_word(cop_sprite_dma_src + 4 + offs);

	cop_sprite_dma_rel_x = (rel_xy & 0xff);
	cop_sprite_dma_rel_y = ((rel_xy & 0xff00) >> 8);
}

static void execute_7e05(INT32 , UINT16 ) // raidendx
{
	cpu_write_byte(0x470, cpu_read_byte(cop_regs[4]));
}

static void execute_8100(INT32 , UINT16 )
{
	INT32 raw_angle = (cop_read_word(cop_regs[0] + (0x34)) & 0xff);
	double angle = raw_angle * M_PI / 128;
	double amp = (65536 >> 5)*(cop_read_word(cop_regs[0] + (0x36)) & 0xff);
	INT32 res;
	// TODO: up direction needs to be doubled, happens on bootleg too, why is that?
	if (raw_angle == 0xc0)
		amp *= 2;
	res = int(amp*sin(angle)) << cop_scale;

	cpu_write_long(cop_regs[0] + 16, res);
}

static void execute_8900(INT32 , UINT16 )
{
	INT32 raw_angle = (cop_read_word(cop_regs[0] + (0x34)) & 0xff);
	double angle = raw_angle * M_PI / 128;
	double amp = (65536 >> 5)*(cop_read_word(cop_regs[0] + (0x36)) & 0xff);
	INT32 res;
	// TODO: left direction needs to be doubled, happens on bootleg too, why is that?
	if (raw_angle == 0x80)
		amp *= 2;
	res = int(amp*cos(angle)) << cop_scale;

	cpu_write_long(cop_regs[0] + 20, res);
}

static void cop_collision_read_pos(INT32 slot, UINT32 spradr, bool allow_swap)
{
	cop_collision_info[slot].allow_swap = allow_swap;
	cop_collision_info[slot].flags_swap = cop_read_word(spradr+2);
	cop_collision_info[slot].spradr = spradr;
	for(INT32 i=0; i<3; i++)
		cop_collision_info[slot].pos[i] = cop_read_word(spradr+6+4*i);
}

static void cop_collision_update_hitbox(UINT16 data, INT32 slot, UINT32 hitadr)
{
	UINT32 hitadr2 = cpu_read_word(hitadr) | (cop_hit_baseadr << 16); // DON'T use cop_read_word here, doesn't need endian fixing?!
	INT32 num_axis = 2;
	INT32 extraxor = 0;
	if (host_endian) extraxor = 1;

	if (data & 0x0100) num_axis = 3;

	INT32 i;

	for(i=0; i<3; i++) {
		cop_collision_info[slot].dx[i] = 0;
		cop_collision_info[slot].size[i] = 0;
	}

	for(i=0; i<num_axis; i++) {
		cop_collision_info[slot].dx[i] = cpu_read_byte(extraxor^ (hitadr2++));
		cop_collision_info[slot].size[i] = cpu_read_byte(extraxor^ (hitadr2++));
	}

	INT16 dx[3],size[3];

	for (i = 0; i < num_axis; i++)
	{
		size[i] = UINT8(cop_collision_info[slot].size[i]);
		dx[i] = INT8(cop_collision_info[slot].dx[i]);
	}

	INT32 j = slot;

	UINT8 res;

	if (num_axis==3) res = 7;
	else res = 3;

	//for (j = 0; j < 2; j++)
	for (i = 0; i < num_axis;i++)
	{
		if (cop_collision_info[j].allow_swap && (cop_collision_info[j].flags_swap & (1 << i)))
		{
			cop_collision_info[j].max[i] = (cop_collision_info[j].pos[i]) - dx[i];
			cop_collision_info[j].min[i] = cop_collision_info[j].max[i] - size[i];
		}
		else
		{
			cop_collision_info[j].min[i] = (cop_collision_info[j].pos[i]) + dx[i];
			cop_collision_info[j].max[i] = cop_collision_info[j].min[i] + size[i];
		}

		if(cop_collision_info[0].max[i] > cop_collision_info[1].min[i] && cop_collision_info[0].min[i] < cop_collision_info[1].max[i])
			res &= ~(1 << i);

		if(cop_collision_info[1].max[i] > cop_collision_info[0].min[i] && cop_collision_info[1].min[i] < cop_collision_info[0].max[i])
			res &= ~(1 << i);

		cop_hit_val[i] = (cop_collision_info[0].pos[i] - cop_collision_info[1].pos[i]);
	}

	cop_hit_val_stat = res; // TODO: there's also bit 2 and 3 triggered in the tests, no known meaning
	cop_hit_status = res;
}

static void execute_a100(INT32 , UINT16 data)
{
	cop_collision_read_pos(0, cop_regs[0], data & 0x0080);
}

static void execute_a900(INT32 , UINT16 data)
{
	cop_collision_read_pos(1, cop_regs[1], data & 0x0080);
}

static void execute_b100(INT32 , UINT16 data)
{
	cop_collision_update_hitbox(data, 0, cop_regs[2]);
}

static void execute_b900(INT32 , UINT16 data)
{
	cop_collision_update_hitbox(data, 1, cop_regs[3]);
}

static void LEGACY_execute_c480(INT32 offset, UINT16 )
{
	UINT8 offs;
	UINT16 sprite_info;
	UINT16 sprite_x, sprite_y;
	INT32 abs_x, abs_y;

	offs = (offset & 3) * 4;

	sprite_info = cpu_read_word(cop_sprite_dma_src + offs) + (cop_sprite_dma_param & 0x3f);
	cpu_write_word(cop_regs[4] + offs + 0, sprite_info);

	abs_x = cpu_read_word(cop_regs[0] + 8) - cop_sprite_dma_abs_x;
	abs_y = cpu_read_word(cop_regs[0] + 4) - cop_sprite_dma_abs_y;

	const UINT8 fx     =  (sprite_info & 0x4000) >> 14;
	const UINT8 fy     =  (sprite_info & 0x2000) >> 13;
	const UINT8 dx     = ((sprite_info & 0x1c00) >> 10) + 1;

	if (fx)
	{
		sprite_x = (0x100 - (dx*16));
		sprite_x += (abs_x)-(cop_sprite_dma_rel_x & 0x78);
		if (cop_sprite_dma_rel_x & 0x80)
			sprite_x -= 0x80;
		else
			sprite_x -= 0x100;
	}
	else
		sprite_x = (cop_sprite_dma_rel_x & 0x78) + (abs_x)-((cop_sprite_dma_rel_x & 0x80) ? 0x80 : 0);

	if (fy)
		sprite_y = (abs_y)+((cop_sprite_dma_rel_y & 0x80) ? 0x80 : 0)-(cop_sprite_dma_rel_y & 0x78);
	else
		sprite_y = (cop_sprite_dma_rel_y & 0x78) + (abs_y)-((cop_sprite_dma_rel_y & 0x80) ? 0x80 : 0);

	cpu_write_word(cop_regs[4] + offs + 4, sprite_x);
	cpu_write_word(cop_regs[4] + offs + 6, sprite_y);
}

static void LEGACY_execute_dde5(INT32 offset, UINT16 )
{
	UINT8 offs;
	INT32 div;
	INT16 dir_offset;

	offs = (offset & 3) * 4;

	div = cpu_read_word(cop_regs[4] + offs);
	dir_offset = cpu_read_word(cop_regs[4] + offs + 8);

	if (div == 0) { div = 1; }


	cpu_write_word((cop_regs[6] + offs + 4), ((cpu_read_word(cop_regs[5] + offs + 4) + dir_offset) / div));
}

static void LEGACY_execute_e30e(INT32 , UINT16 data)
{
	INT32 dy = cpu_read_long(cop_regs[2] + 4) - cpu_read_long(cop_regs[0] + 4);
	INT32 dx = cpu_read_long(cop_regs[2] + 8) - cpu_read_long(cop_regs[0] + 8);

	cop_status = 7;
	if (!dx)
	{
		cop_status |= 0x8000;
		cop_angle = 0;
	}
	else {
		cop_angle = (int)(atan(double(dy) / double(dx)) * 128.0 / M_PI);
		if (dx < 0)
			cop_angle += 0x80;

		cop_angle &= 0xff;
	}

	// TODO: byte or word?
	if (data & 0x0080)
		cop_write_byte(cop_regs[0] + 0x34, cop_angle);
}

static void execute_f205(INT32 , UINT16 )
{
	cpu_write_long(cop_regs[2], cpu_read_long(cop_regs[0]+4));
}

static INT32 find_trigger_match(UINT16 triggerval, UINT16 mask)
{
	INT32 matched = 0;
	INT32 command = -1;

	for (INT32 i = 0; i < 32; i++)
	{
		if ((triggerval & mask) == (cop_func_trigger[i] & mask) && cop_func_trigger[i] != 0)
		{
			command = i;
			matched++;
		}
	}

	if (matched == 1)
	{
		return command;
	}

	return -1;
}

static INT32 check_command_matches(INT32 command, UINT16 seq0, UINT16 seq1, UINT16 seq2, UINT16 seq3, UINT16 seq4, UINT16 seq5, UINT16 seq6, UINT16 seq7, UINT16 _funcval_, UINT16 _funcmask_)
{
	command *= 8;

	if (cop_program[command+0] == seq0 && cop_program[command+1] == seq1 && cop_program[command+2] == seq2 && cop_program[command+3] == seq3 &&
		cop_program[command+4] == seq4 && cop_program[command+5] == seq5 && cop_program[command+6] == seq6 && cop_program[command+7] == seq7 &&
		cop_func_value[command/8] == _funcval_ &&
		cop_func_mask[command/8] == _funcmask_)
		return 1;
	else
		return 0;
}

static void LEGACY_cop_cmd_write(INT32 offset, UINT16 data)
{
	INT32 command = find_trigger_match(data, 0xf800);

	if (command == -1)
	{
		return;
	}

	UINT16 funcval = cop_func_value[command];
	UINT16 funcmask = cop_func_mask[command];

	if (check_command_matches(command, 0x188, 0x282, 0x082, 0xb8e, 0x98e, 0x000, 0x000, 0x000, 6, 0xffeb))
	{
		execute_0205(offset, data);
		return;
	}

	if (check_command_matches(command, 0x194, 0x288, 0x088, 0x000, 0x000, 0x000, 0x000, 0x000, 6, 0xfbfb))
	{
		execute_0904(offset, data);
		return;
	}

	if (check_command_matches(command, 0xb9a, 0xb88, 0x888, 0x000, 0x000, 0x000, 0x000, 0x000, 7, 0xfdfb))
	{
		execute_8100(offset, data); // SIN
		return;
	}

	if (check_command_matches(command, 0xb9a, 0xb8a, 0x88a, 0x000, 0x000, 0x000, 0x000, 0x000, 7, 0xfdfb))
	{
		execute_8900(offset, data); // COS
		return;
	}

	if (check_command_matches(command, 0x984, 0xaa4, 0xd82, 0xaa2, 0x39b, 0xb9a, 0xb9a, 0xa9a, 5, 0xbf7f))
	{
		LEGACY_execute_130e_cupsoc(offset, data);
		return;
	}

	if (check_command_matches(command, 0x984, 0xaa4, 0xd82, 0xaa2, 0x39b, 0xb9a, 0xb9a, 0xb9a, 5, 0xbf7f))
	{
		execute_130e(offset, data, true);
		return;
	}

	if (check_command_matches(command, 0xf9c, 0xb9c, 0xb9c, 0xb9c, 0xb9c, 0xb9c, 0xb9c, 0x99c, 4, 0x007f))
	{
		execute_3b30(offset, data);
		return;
	}

	if (check_command_matches(command, 0xf9a, 0xb9a, 0xb9c, 0xb9c, 0xb9c, 0x29c, 0x000, 0x000, 5, 0xfcdd))
	{
		execute_42c2(offset, data);
		return;
	}

	if (check_command_matches(command, 0xb80, 0xb82, 0xb84, 0xb86, 0x000, 0x000, 0x000, 0x000, funcval, funcmask))
	{
		execute_a100(offset, data);
		return;
	}

	if (check_command_matches(command, 0xb40, 0xbc0, 0xbc2, 0x000, 0x000, 0x000, 0x000, 0x000, funcval, funcmask))
	{
		execute_b100(offset, data);
		return;
	}

	if (check_command_matches(command, 0xba0, 0xba2, 0xba4, 0xba6, 0x000, 0x000, 0x000, 0x000, funcval, funcmask))
	{
		execute_a900(offset, data);
		return;
	}

	if (check_command_matches(command, 0xb60, 0xbe0, 0xbe2, 0x000, 0x000, 0x000, 0x000, 0x000, funcval, funcmask))
	{
		execute_b900(offset, data);
		return;
	}

	if (check_command_matches(command, 0xb80, 0xba0, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 10, 0xfff3))
	{
		LEGACY_execute_6980(offset, data);
		return;
	}

	if (check_command_matches(command, 0x080, 0x882, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 10, 0xff00))
	{
		LEGACY_execute_c480(offset, data);
		return;
	}

	if (check_command_matches(command, 0xf80, 0xaa2, 0x984, 0x0c2, 0x000, 0x000, 0x000, 0x000, 5, 0x7ff7))
	{
		LEGACY_execute_dde5(offset, data);
		return;
	}

	if (check_command_matches(command, 0x3a0, 0x3a6, 0x380, 0xaa0, 0x2a6, 0x000, 0x000, 0x000, 8, 0xf3e7))
	{
		LEGACY_execute_6200(offset, data);
		return;
	}

	if (check_command_matches(command, 0x380, 0x39a, 0x380, 0xa80, 0x29a, 0x000, 0x000, 0x000, 8, 0xf3e7))
	{
		execute_6200(offset, data);
		return;
	}

	if (check_command_matches(command, 0x984, 0xac4, 0xd82, 0xac2, 0x39b, 0xb9a, 0xb9a, 0xa9a, 5, 0xb07f))
	{
		LEGACY_execute_e30e(offset, data);
		return;
	}

	if (check_command_matches(command, 0xac2, 0x9e0, 0x0a2, 0x000, 0x000, 0x000, 0x000, 0x000, 5, 0xfffb))
	{
	//	LEGACY_execute_d104(offset, data);
		return;
	}

	if (check_command_matches(command, 0xa80, 0x984, 0x082, 0x000, 0x000, 0x000, 0x000, 0x000, 5, 0xfefb))
	{
		return;
	}

	if (check_command_matches(command, 0x9c8, 0xa84, 0x0a2, 0x000, 0x000, 0x000, 0x000, 0x000, 5, 0xfffb))
	{
		return;
	}

	if (check_command_matches(command, 0xa88, 0x994, 0x088, 0x000, 0x000, 0x000, 0x000, 0x000, 5, 0xfefb))
	{
	//	execute_f105(offset,data);
		return;
	}
}

void cop_cmd_write(INT32 offset, UINT16 data)
{
	find_trigger_match(data, 0xf800);

	cop_status &= 0x7fff;

	switch(data) {
	case 0x0205: {  // 0205 0006 ffeb 0000 - 0188 0282 0082 0b8e 098e 0000 0000 0000
		execute_0205(offset, data); // angle from dx/dy
		break;
	}

	case 0x0904: /* X Se Dae and Zero Team uses this variant */
	case 0x0905: //  0905 0006 fbfb 0008 - 0194 0288 0088 0000 0000 0000 0000 0000
		execute_0904(offset, data);
		break;

	case 0x130e:   // 130e 0005 bf7f 0010 - 0984 0aa4 0d82 0aa2 039b 0b9a 0b9a 0a9a
	case 0x138e:
		execute_130e(offset, data, false); // angle from dx/dy
		break;

	case 0x338e: { // 338e 0005 bf7f 0030 - 0984 0aa4 0d82 0aa2 039c 0b9c 0b9c 0a9a
		execute_338e(offset, data, false); // angle from dx/dy
		break;
	}

	case 0x2208:
	case 0x2288: { // 2208 0005 f5df 0020 - 0f8a 0b8a 0388 0b9a 0b9a 0a9a 0000 0000
		execute_2288(offset, data); // angle from dx/dy
		break;
	}

	case 0x2a05: { // 2a05 0006 ebeb 0028 - 09af 0a82 0082 0a8f 018e 0000 0000 0000
		execute_2a05(offset, data);
		break;
	}

	case 0x39b0:
	case 0x3b30:
	case 0x3bb0: { // 3bb0 0004 007f 0038 - 0f9c 0b9c 0b9c 0b9c 0b9c 0b9c 0b9c 099c
		execute_3b30(offset, data);

		break;
	}

	case 0x42c2: { // 42c2 0005 fcdd 0040 - 0f9a 0b9a 0b9c 0b9c 0b9c 029c 0000 0000
		execute_42c2(offset, data); // DIVIDE
		break;
	}

	case 0x4aa0: { // 4aa0 0005 fcdd 0048 - 0f9a 0b9a 0b9c 0b9c 0b9c 099b 0000 0000
		execute_4aa0(offset, data); // DIVIDE
		break;
	}

	case 0x6200: {
		execute_6200(offset, data); // Target Angle calcs
		break;
	}

	case 0x8100: { // 8100 0007 fdfb 0080 - 0b9a 0b88 0888 0000 0000 0000 0000 0000
		execute_8100(offset, data); // SIN
		break;
	}

	case 0x8900: { // 8900 0007 fdfb 0088 - 0b9a 0b8a 088a 0000 0000 0000 0000 0000
		execute_8900(offset, data); // COS
		break;
	}

	case 0x5205:   // 5205 0006 fff7 0050 - 0180 02e0 03a0 00a0 03a0 0000 0000 0000
		//      fprintf(stderr, "sprcpt 5205 %04x %04x %04x %08x %08x\n", cop_regs[0], cop_regs[1], cop_regs[3], m_host_space.read_dword(cop_regs[0]), m_host_space.read_dword(cop_regs[3]));
		execute_5205(offset, data);
		break;

	case 0x5a05:   // 5a05 0006 fff7 0058 - 0180 02e0 03a0 00a0 03a0 0000 0000 0000
		//      fprintf(stderr, "sprcpt 5a05 %04x %04x %04x %08x %08x\n", cop_regs[0], cop_regs[1], cop_regs[3], m_host_space.read_dword(cop_regs[0]), m_host_space.read_dword(cop_regs[3]));
		execute_5a05(offset, data);

		break;

	case 0xf205:   // f205 0006 fff7 00f0 - 0182 02e0 03c0 00c0 03c0 0000 0000 0000
		//      fprintf(stderr, "sprcpt f205 %04x %04x %04x %08x %08x\n", cop_regs[0]+4, cop_regs[1], cop_regs[3], m_host_space.read_dword(cop_regs[0]+4), m_host_space.read_dword(cop_regs[3]));
		execute_f205(offset, data);
		break;

		// raidendx only
	case 0x7e05:
		execute_7e05(offset, data);
		break;

	case 0xa100:
	case 0xa180:
		execute_a100(offset, data); // collisions
		break;

	case 0xa900:
	case 0xa980:
		execute_a900(offset, data); // collisions
		break;

	case 0xb100: {
		execute_b100(offset, data);// collisions
		break;
	}

	case 0xb900: {
		execute_b900(offset, data); // collisions
		break;
	}
	}
}

static UINT8 fade_table(INT32 v)
{
	INT32 low  = v & 0x001f;
	INT32 high = v & 0x03e0;

	return (low * (high | (high >> 5)) + 0x210) >> 10;
}

static void dma_palette_brightness()
{
	UINT32 src = (cop_dma_src[cop_dma_mode] << 6);
	UINT32 dst = (cop_dma_dst[cop_dma_mode] << 6);
	UINT32 size = ((cop_dma_size[cop_dma_mode] << 5) - (cop_dma_dst[cop_dma_mode] << 6) + 0x20) / 2;

	for (UINT32 i = 0; i < size; i++)
	{
		UINT16 pal_val;
		INT32 r, g, b;
		INT32 rt, gt, bt;

		if (pal_brightness_mode == 5)
		{
			UINT16 paldata = cpu_read_word(src);
			if (paldata & (1 << 15))
				pal_val = paldata; // fade me not
			else
			{
				UINT16 targetpaldata = cpu_read_word(src + (cop_dma_adr_rel * 0x400));
				bt = (targetpaldata & 0x7c00) >> 5;
				bt = fade_table(bt | (pal_brightness_val ^ 0));
				b = (paldata & 0x7c00) >> 5;
				b = fade_table(b | (pal_brightness_val ^ 0x1f));
				pal_val = ((b + bt) & 0x1f) << 10;
				gt = (targetpaldata & 0x03e0);
				gt = fade_table(gt | (pal_brightness_val ^ 0));
				g = (paldata & 0x03e0);
				g = fade_table(g | (pal_brightness_val ^ 0x1f));
				pal_val |= ((g + gt) & 0x1f) << 5;
				rt = (targetpaldata & 0x001f) << 5;
				rt = fade_table(rt | (pal_brightness_val ^ 0));
				r = (paldata & 0x001f) << 5;
				r = fade_table(r | (pal_brightness_val ^ 0x1f));
				pal_val |= ((r + rt) & 0x1f);
			}
		}
		else if (pal_brightness_mode == 4) //Denjin Makai & Godzilla
		{
			UINT16 targetpaldata = cpu_read_word((src + (cop_dma_adr_rel * 0x400)));
			UINT16 paldata = cpu_read_word(src);

			bt = (targetpaldata & 0x7c00) >> 10;
			b = (paldata & 0x7c00) >> 10;
			gt = (targetpaldata & 0x03e0) >> 5;
			g = (paldata & 0x03e0) >> 5;
			rt = (targetpaldata & 0x001f) >> 0;
			r = (paldata & 0x001f) >> 0;

			if (pal_brightness_val == 0x10)
				pal_val = bt << 10 | gt << 5 | rt << 0;
			else if (pal_brightness_val == 0xffff) // level transitions
				pal_val = bt << 10 | gt << 5 | rt << 0;
			else
			{
				bt = fade_table(bt << 5 | ((pal_brightness_val * 2) ^ 0));
				b = fade_table(b << 5 | ((pal_brightness_val * 2) ^ 0x1f));
				pal_val = ((b + bt) & 0x1f) << 10;
				gt = fade_table(gt << 5 | ((pal_brightness_val * 2) ^ 0));
				g = fade_table(g << 5 | ((pal_brightness_val * 2) ^ 0x1f));
				pal_val |= ((g + gt) & 0x1f) << 5;
				rt = fade_table(rt << 5 | ((pal_brightness_val * 2) ^ 0));
				r = fade_table(r << 5 | ((pal_brightness_val * 2) ^ 0x1f));
				pal_val |= ((r + rt) & 0x1f);
			}
		}
		else
		{
			pal_val = cpu_read_word(src);
		}

		cpu_write_word(dst, pal_val);
		src += 2;
		dst += 2;
	}
}

static void cop_dma_trigger()
{
	switch (cop_dma_mode)
	{
		case 0x14:
		{
			UINT32 src = cop_dma_src[cop_dma_mode] << 6;
			if (src == 0xcfc0) src = 0xd000;

			for (INT32 i = 0; i < 0x2800 / 2; i++)
			{
				UINT16 tileval = cpu_read_word(src);
				src += 2;
				if (cop_videoram_cb) {
					cop_videoram_cb(i, tileval, 0xffff);
				}
			}

			break;
		}

		case 0x15:
		{
			UINT32 src = cop_dma_src[cop_dma_mode] << 6;

			for (INT32 i = 0; i < 0x1000 / 2; i++) // todo, use length register
			{
				UINT16 palval = cpu_read_word(src);
				src += 2;
				if (cop_paletteram_cb) {
					cop_paletteram_cb(i, palval);
				}
			}

			break;
		}

		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87:
		{
			dma_palette_brightness();
			break;
		}

		case 0x09:
		case 0x0e:  // Godzilla / Seibu Cup Soccer
		{
			UINT32 src = (cop_dma_src[cop_dma_mode] << 6);
			UINT32 dst = (cop_dma_dst[cop_dma_mode] << 6);
			UINT32 size = ((cop_dma_size[cop_dma_mode] << 5) - (cop_dma_dst[cop_dma_mode] << 6) + 0x20) / 2;

			for (UINT32 i = 0; i < size; i++)
			{
				cpu_write_word(dst, cpu_read_word(src));
				src += 2;
				dst += 2;
			}

			break;
		}

		case 0x116: // Godzilla
		{
			UINT32 address = (cop_dma_src[cop_dma_mode] << 6);
			UINT32 length = ((cop_dma_size[cop_dma_mode] + 1) << 4);

			for (UINT32 i = address; i < address + length; i += 4)
			{
				cpu_write_long(i, cop_dma_v1 | (cop_dma_v2 << 16));
			}

			break;
		}

		case 0x118:
		case 0x119:
		case 0x11a:
		case 0x11b:
		case 0x11c:
		case 0x11d:
		case 0x11e:
		case 0x11f:
		{
			if (cop_dma_dst[cop_dma_mode] != 0x0000) // Invalid?
				return;

			UINT32 address = (cop_dma_src[cop_dma_mode] << 6);
			UINT32 length = (cop_dma_size[cop_dma_mode] + 1) << 5;

			for (UINT32 i = address; i < address + length; i += 4)
			{
				cpu_write_long(i, cop_dma_v1 | (cop_dma_v2 << 16));
			}
			break;
		}
	}
}

static void dma_zsorting(UINT16 sort_size)
{
	UINT8 xchg_flag;

	for(INT32 i = 2;i < sort_size;i+=2)
	{
		UINT16 vali = cpu_read_word(cop_sort_ram_addr+cpu_read_word(cop_sort_lookup+i));

		for(INT32 j = i-2;j<sort_size;j+=2)
		{
			UINT16 valj = cpu_read_word(cop_sort_ram_addr+cpu_read_word(cop_sort_lookup+j));

			switch (cop_sort_param)
			{
				case 2: xchg_flag = (vali > valj); break;
				case 1: xchg_flag = (vali < valj); break;
				default: xchg_flag = 0; break;
			}

			if (xchg_flag)
			{
				cpu_write_word(cop_sort_lookup+i,cpu_read_word(cop_sort_lookup+j));
				cpu_write_word(cop_sort_lookup+j,cpu_read_word(cop_sort_lookup+i));
			}
		}
	}
}

static void bcd_update()
{
	UINT32 val = cop_itoa;

	for (INT32 i = 0; i < 9; i++)
	{
		if (!val && i)
		{
			cop_itoa_digits[i] = (cop_itoa_mode == 3) ? 0x30 : 0x20;
		}
		else
		{
			cop_itoa_digits[i] = 0x30 | (val % 10);
			val = val / 10;
		}
	}

	cop_itoa_digits[9] = 0;
}

void seibu_cop_write(UINT16 offset, UINT16 data)
{
	switch (offset & 0x3fe)
	{
		case 0x000: // 400
			cop_sprite_dma_param = (cop_sprite_dma_param & 0xffff0000) | data;
		return;

		case 0x002: // 402
			cop_sprite_dma_param = (cop_sprite_dma_param & 0x0000ffff) | (data << 16);
		return;

		case 0x00c: // 40c
			cop_sprite_dma_size = data;
		return;

		case 0x010: // 410
			if (data == 0)
			{
				cop_regs[4] += 8;
				cop_sprite_dma_src += 6;
				cop_sprite_dma_size--;

				if (cop_sprite_dma_size > 0)
					cop_status &= ~2;
				else
					cop_status |= 2;
			}
		return;

		case 0x012: // 412
			cop_sprite_dma_src = (cop_sprite_dma_src & 0x0000ffff) | (data << 16);
		return;

		case 0x014: // 414
			cop_sprite_dma_src = (cop_sprite_dma_src & 0xffff0000) | data;
		return;

		case 0x01c: // 41c
			cop_angle_target = data;
		return;

		case 0x01e: // 41e
			cop_angle_step = data;
		return;

		case 0x020: // 420
			cop_itoa = (cop_itoa & 0xffff0000) | data;
			bcd_update();
		return;

		case 0x022: // 422
			cop_itoa = (cop_itoa & 0x0000ffff) | (data << 16);
			bcd_update();
		return;

		case 0x024: // 424
			cop_itoa_mode = data;
		return;

		case 0x028: // 428
			cop_dma_v1 = data;
		return;

		case 0x02a: // 42a
			cop_dma_v2 = data;
		return;

		case 0x02c: // 42c
			cop_rng_max_value = data;
		return;

		case 0x032:
		{
			INT32 idx = cop_latch_addr >> 3;
			cop_program[cop_latch_addr] = data;
			cop_func_trigger[idx] = cop_latch_trigger;
			cop_func_value[idx] = cop_latch_value;
			cop_func_mask[idx] = cop_latch_mask;
		}
		return;

		case 0x034: // 434
			if (data > 0xff) bprintf(0, _T("seibucop latch data overflow: %X\n"), data);
			cop_latch_addr = data & 0xff;
		return;

		case 0x036: // 436
			cop_hit_baseadr = data;
		return;

		case 0x038: // 438
			cop_latch_value = data;
		return;

		case 0x03a: // 43a
			cop_latch_mask = data;
		return;

		case 0x03c: // 43c
			cop_latch_trigger = data;
		return;

		case 0x040: // 440 - (unknown)
		return;

		case 0x042: // 442 - (unknown)
		return;

		case 0x044: // 444
			cop_scale = data & 3;
		return;

		case 0x046: // 446
			cop_rom_addr_hi = data;
		return;

		case 0x048: // 448
			cop_rom_addr_lo = data;
		return;

		case 0x04a: // 44a
			cop_precmd = data;
		return;

		case 0x050: // 450
			cop_sort_ram_addr = (cop_sort_ram_addr & 0x0000ffff) | (data << 16);
		return;

		case 0x052: // 452
			cop_sort_ram_addr = (cop_sort_ram_addr & 0xffff0000) | data;
		return;

		case 0x054: // 454
			cop_sort_lookup = (cop_sort_lookup & 0x0000ffff) | (data << 16);
		return;

		case 0x056: // 456
			cop_sort_lookup = (cop_sort_lookup & 0xffff0000) | data;
		return;

		case 0x058: // 458
			cop_sort_param = data;
		return;

		case 0x05a: // 45a
			pal_brightness_val = data;
		return;

		case 0x05c: // 45c
			pal_brightness_mode = data;
		return;

		case 0x076: // 476
			cop_dma_adr_rel = data;
		return;

		case 0x078: // 478
			cop_dma_src[cop_dma_mode & 0x1ff] = data;
		return;

		case 0x07a: // 47a
			cop_dma_size[cop_dma_mode & 0x1ff] = data;
		return;

		case 0x07c: // 47c
			cop_dma_dst[cop_dma_mode & 0x1ff] = data;
		return;

		case 0x07e: // 47e
			cop_dma_mode = data;
		return;

		case 0x08c: // 48c
			cop_sprite_dma_abs_y = data;
		return;

		case 0x08e: // 48d
			cop_sprite_dma_abs_x = data;
		return;

		case 0x0a0: // 4a0
		case 0x0a2:
		case 0x0a4:
		case 0x0a6:
		case 0x0a8:
		case 0x0aa:
		case 0x0ac:
			cop_regs[(offset / 2) & 7] = (cop_regs[(offset / 2) & 7] & 0x0000ffff) | (data << 16);
		return;

		case 0x0c0: // 4c0
		case 0x0c2:
		case 0x0c4:
		case 0x0c6:
		case 0x0c8:
		case 0x0ca:
		case 0x0cc:
			cop_regs[(offset / 2) & 7] = (cop_regs[(offset / 2) & 7] & 0xffff0000) | data;
		return;

		case 0x100: // 500
		case 0x102:
		case 0x104:
			LEGACY_cop_cmd_write((offset & 6)/2, data);
		return;

		case 0x2fc: // 6fc
			cop_dma_trigger();
		return;

		case 0x2fe:
			dma_zsorting(data);
		return;
	}
}

UINT16 seibu_cop_read(UINT16 offset)
{
	switch (offset & 0x3fe)
	{
		case 0x02c: // 42c
			return cop_rng_max_value;

		case 0x07e: // 47e
			return cop_dma_mode;

		case 0x0a0: // 4a0
		case 0x0a2:
		case 0x0a4:
		case 0x0a6:
		case 0x0a8:
		case 0x0aa:
		case 0x0ac:
			return cop_regs[(offset / 2) & 7] >> 16;

		case 0x0c0: // 4c0
		case 0x0c2:
		case 0x0c4:
		case 0x0c6:
		case 0x0c8:
		case 0x0ca:
		case 0x0cc:
			return cop_regs[(offset / 2) & 7];

		case 0x180: // 580
			return cop_hit_status;

		case 0x182: // 582
		case 0x184:
		case 0x186:
			return cop_hit_val[(offset - 0x182) / 2];

		case 0x188: // 588
			return cop_hit_val_stat;

		case 0x190: // 590
		case 0x192:
		case 0x194:
		case 0x196:
		case 0x198:
			return cop_itoa_digits[(offset & 0xe)] | (cop_itoa_digits[(offset & 0xe)+1] << 8);

		case 0x1a0: // 5a0
		case 0x1a2:
		case 0x1a4:
		case 0x1a6:
			return BurnRandom() % (cop_rng_max_value + 1);

		case 0x1b0: // 5b0
			return cop_status;

		case 0x1b2: // 5b2
			return cop_dist;

		case 0x1b4: // 5b4
			return cop_angle;
	}

	bprintf(0, _T("unmapped cop read: %X\n"), offset);

	return 0;
}

void seibu_cop_reset()
{
	cop_sprite_dma_param = 0;
	cop_sprite_dma_size = 0;
	cop_sprite_dma_src = 0;
	cop_sprite_dma_abs_y = 0;
	cop_sprite_dma_abs_x = 0;
	cop_status = 0;
	cop_angle_target = 0;
	cop_angle_step = 0;
	cop_angle = 0;
	cop_dist = 0;
	cop_itoa = 0;
	cop_itoa_mode = 0;
	memset (cop_itoa_digits, 0, sizeof(cop_itoa_digits)); //[10];
	memset (cop_regs, 0, sizeof(cop_regs)); //[8];

	cop_dma_v1 = 0;
	cop_dma_v2 = 0;
	cop_rng_max_value = 0;
	cop_hit_baseadr = 0;
	cop_scale = 0;
	cop_rom_addr_hi = 0;
	cop_rom_addr_lo = 0;
	cop_precmd = 0;

	cop_sort_ram_addr = 0;
	cop_sort_lookup = 0;
	cop_sort_param = 0;

	pal_brightness_val = 0;
	pal_brightness_mode = 0;

	cop_dma_adr_rel = 0;
	cop_dma_mode = 0;
	memset (cop_dma_src, 0, sizeof(cop_dma_src)); //[0x200];
	memset (cop_dma_size, 0, sizeof(cop_dma_size)); //[0x200];
	memset (cop_dma_dst, 0, sizeof(cop_dma_dst)); //[0x200];

	cop_hit_status = 0;
	cop_hit_val_stat = 0;
	memset (cop_hit_val, 0, sizeof(cop_hit_val)); //[3];

	cop_latch_value = 0;
	cop_latch_addr = 0;
	cop_latch_mask = 0;
	cop_latch_trigger = 0;
	memset (cop_func_value, 0, sizeof(cop_func_value)); //[256/8];
	memset (cop_func_mask, 0, sizeof(cop_func_mask)); //[256/8];
	memset (cop_func_trigger, 0, sizeof(cop_func_trigger)); //[256/8];
	memset (cop_program, 0, sizeof(cop_program)); //[256];

	LEGACY_r0 = 0;
	LEGACY_r1 = 0;

	memset (cop_collision_info, 0, sizeof(cop_collision_info));
}

#ifndef HARDSEK
static UINT16 _68k_read_word(UINT32 a) { return SekReadWord(a); }
static UINT8 _68k_read_byte(UINT32 a) { return SekReadByte(a); }
#endif

void seibu_cop_config(INT32 is_68k, void (*vidram_write)(INT32,UINT16,UINT16), void (*pal_write)(INT32,UINT16))
{
	host_endian = is_68k;
#ifdef HARDSEK
	if (is_68k == 0) bprintf (0, _T("seibu_cop_config - cannot use any cpu type but 68k! HARDSEK defined!\n"));
#endif
	byte_endian_val = host_endian ? 3 : 0;
	word_endian_val = host_endian ? 2 : 0;

#ifndef HARDSEK
	cpu_write_long = (host_endian) ? SekWriteLong : VezWriteLong;
	cpu_write_word = (host_endian) ? SekWriteWord : VezWriteWord;
	cpu_write_byte = (host_endian) ? SekWriteByte : VezWriteByte;
	cpu_read_long = (host_endian) ? SekReadLong : VezReadLong;
	cpu_read_word = (host_endian) ? _68k_read_word : VezReadWord; // ??
	cpu_read_byte = (host_endian) ? _68k_read_byte : VezReadByte; // ?
#endif

	cop_paletteram_cb = pal_write;
	cop_videoram_cb = vidram_write;
}

INT32 seibu_cop_scan(INT32 nAction,INT32 *)
{
	if (nAction & ACB_DRIVER_DATA)
	{
		SCAN_VAR(cop_sprite_dma_param);
		SCAN_VAR(cop_sprite_dma_size);
		SCAN_VAR(cop_sprite_dma_src);
		SCAN_VAR(cop_sprite_dma_abs_y);
		SCAN_VAR(cop_sprite_dma_abs_x);
		SCAN_VAR(cop_sprite_dma_rel_y);
		SCAN_VAR(cop_sprite_dma_rel_x);

		SCAN_VAR(cop_status);
		SCAN_VAR(cop_angle_target);
		SCAN_VAR(cop_angle_step);
		SCAN_VAR(cop_angle);
		SCAN_VAR(cop_dist);
		SCAN_VAR(cop_itoa);
		SCAN_VAR(cop_itoa_mode);
		SCAN_VAR(cop_itoa_digits); //[10];
		SCAN_VAR(cop_regs); //[8];

		SCAN_VAR(cop_dma_v1);
		SCAN_VAR(cop_dma_v2);
		SCAN_VAR(cop_rng_max_value);
		SCAN_VAR(cop_hit_baseadr);
		SCAN_VAR(cop_scale);
		SCAN_VAR(cop_rom_addr_hi);
		SCAN_VAR(cop_rom_addr_lo);
		SCAN_VAR(cop_precmd);

		SCAN_VAR(cop_sort_ram_addr);
		SCAN_VAR(cop_sort_lookup);
		SCAN_VAR(cop_sort_param);

		SCAN_VAR(pal_brightness_val);
		SCAN_VAR(pal_brightness_mode);

		SCAN_VAR(cop_dma_adr_rel);
		SCAN_VAR(cop_dma_mode);
		SCAN_VAR(cop_dma_src); //[0x200];
		SCAN_VAR(cop_dma_size); //[0x200];
		SCAN_VAR(cop_dma_dst); //[0x200];

		SCAN_VAR(cop_hit_status);
		SCAN_VAR(cop_hit_val_stat);
		SCAN_VAR(cop_hit_val); //[3];

		SCAN_VAR(cop_latch_value);
		SCAN_VAR(cop_latch_addr);
		SCAN_VAR(cop_latch_mask);
		SCAN_VAR(cop_latch_trigger);
		SCAN_VAR(cop_func_value); //[256/8];
		SCAN_VAR(cop_func_mask); //[256/8];
		SCAN_VAR(cop_func_trigger); //[256/8];
		SCAN_VAR(cop_program); //[256];

		SCAN_VAR(LEGACY_r0);
		SCAN_VAR(LEGACY_r1);

		SCAN_VAR(cop_collision_info);

		BurnRandomScan(nAction);
	}

	return 0;
}
