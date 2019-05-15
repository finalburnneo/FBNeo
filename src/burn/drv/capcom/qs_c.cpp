/*

	Capcom DL-1425 QSound emulator
	==============================

	by superctr (Ian Karlsson)
	with thanks to Valley Bell

	2018-05-12 - 2018-05-15

*/

#include <math.h>
#include <stddef.h>
#include <string.h>	// for memset
#include "cps.h"

static const INT32 nQscClock = 60000000;
static const INT32 nQscClockDivider = 2496;

static INT32 nQscRate = 0;

static INT32 nPos;
static INT32 nDelta;

static double QsndGain[2];
static INT32 QsndOutputDir[2];

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

struct qsound_voice {
	UINT16 bank;
	INT16 addr; // top word is the sample address
	UINT16 phase;
	UINT16 rate;
	INT16 loop_len;
	INT16 end_addr;
	INT16 volume;
	INT16 echo;
};

struct qsound_adpcm {
	UINT16 start_addr;
	UINT16 end_addr;
	UINT16 bank;
	INT16 volume;
	UINT16 flag;
	INT16 cur_vol;
	INT16 step_size;
	UINT16 cur_addr;
};

// Q1 Filter
struct qsound_fir {
	int tap_count;	// usually 95
	int delay_pos;
	INT16 table_pos;
	INT16 taps[95];
	INT16 delay_line[95];
};

// Delay line
struct qsound_delay {
	INT16 delay;
	INT16 volume;
	INT16 write_pos;
	INT16 read_pos;
	INT16 delay_line[51];
};

struct qsound_echo {
	UINT16 end_pos;

	INT16 feedback;
	INT16 length;
	INT16 last_sample;
	INT16 delay_line[1024];
	INT16 delay_pos;
};

struct qsound_chip {
	INT16 out[2];

	struct qsound_voice voice[16];
	struct qsound_adpcm adpcm[3];

	UINT16 voice_pan[16+3];
	INT16 voice_output[16+3];

	struct qsound_echo echo;

	struct qsound_fir filter[2];
	struct qsound_fir alt_filter[2];

	struct qsound_delay wet[2];
	struct qsound_delay dry[2];

	UINT16 state;
	UINT16 next_state;

	UINT16 delay_update;

	int state_counter;
	UINT8 ready_flag;
};

static UINT16 *register_map[256];
static INT16 pan_tables[2][2][98];

static INT16 interpolate_buffer[2][4];

static void init_pan_tables();
static void init_register_map();
static void update_sample();

static void state_init();
static void state_refresh_filter_1();
static void state_refresh_filter_2();
static void state_normal_update();

static inline INT16 get_sample(UINT16 bank,UINT16 address);
static inline const INT16* get_filter_table(UINT16 offset);
static inline INT16 pcm_update(int voice_no, INT32 *echo_out);
static inline void adpcm_update(int voice_no, int nibble);
static inline INT16 echo(struct qsound_echo *r,INT32 input);
static inline INT32 fir(struct qsound_fir *f, INT16 input);
static inline INT32 delay(struct qsound_delay *d, INT32 input);
static inline void delay_update(struct qsound_delay *d);

static qsound_chip chip;

static const INT16 qsound_dry_mix_table[33] = {
	-16384,-16384,-16384,-16384,-16384,-16384,-16384,-16384,
	-16384,-16384,-16384,-16384,-16384,-16384,-16384,-16384,
	-16384,-14746,-13107,-11633,-10486,-9175,-8520,-7209,
	-6226,-5226,-4588,-3768,-3277,-2703,-2130,-1802,
	0
};

static const INT16 qsound_wet_mix_table[33] = {
	0,-1638,-1966,-2458,-2949,-3441,-4096,-4669,
	-4915,-5120,-5489,-6144,-7537,-8831,-9339,-9830,
	-10240,-10322,-10486,-10568,-10650,-11796,-12288,-12288,
	-12534,-12648,-12780,-12829,-12943,-13107,-13418,-14090,
	-16384
};

static const INT16 qsound_linear_mix_table[33] = {
	-16379,-16338,-16257,-16135,-15973,-15772,-15531,-15251,
	-14934,-14580,-14189,-13763,-13303,-12810,-12284,-11729,
	-11729,-11144,-10531,-9893,-9229,-8543,-7836,-7109,
	-6364,-5604,-4829,-4043,-3246,-2442,-1631,-817,
	0
};

static const INT16 qsound_filter_data[5][95] = {
	{	// d53 - 0
		0,0,0,6,44,-24,-53,-10,59,-40,-27,1,39,-27,56,127,174,36,-13,49,
		212,142,143,-73,-20,66,-108,-117,-399,-265,-392,-569,-473,-71,95,-319,-218,-230,331,638,
		449,477,-180,532,1107,750,9899,3828,-2418,1071,-176,191,-431,64,117,-150,-274,-97,-238,165,
		166,250,-19,4,37,204,186,-6,140,-77,-1,1,18,-10,-151,-149,-103,-9,55,23,
		-102,-97,-11,13,-48,-27,5,18,-61,-30,64,72,0,0,0,
	},
	{	// db2 - 1 - default left filter
		0,0,0,85,24,-76,-123,-86,-29,-14,-20,-7,6,-28,-87,-89,-5,100,154,160,
		150,118,41,-48,-78,-23,59,83,-2,-176,-333,-344,-203,-66,-39,2,224,495,495,280,
		432,1340,2483,5377,1905,658,0,97,347,285,35,-95,-78,-82,-151,-192,-171,-149,-147,-113,
		-22,71,118,129,127,110,71,31,20,36,46,23,-27,-63,-53,-21,-19,-60,-92,-69,
		-12,25,29,30,40,41,29,30,46,39,-15,-74,0,0,0,
	},
	{	// e11 - 2 - default right filter
		0,0,0,23,42,47,29,10,2,-14,-54,-92,-93,-70,-64,-77,-57,18,94,113,
		87,69,67,50,25,29,58,62,24,-39,-131,-256,-325,-234,-45,58,78,223,485,496,
		127,6,857,2283,2683,4928,1328,132,79,314,189,-80,-90,35,-21,-186,-195,-99,-136,-258,
		-189,82,257,185,53,41,84,68,38,63,77,14,-60,-71,-71,-120,-151,-84,14,29,
		-8,7,66,69,12,-3,54,92,52,-6,-15,-2,0,0,0,
	},
	{	// e70 - 3
		0,0,0,2,-28,-37,-17,0,-9,-22,-3,35,52,39,20,7,-6,2,55,121,
		129,67,8,1,9,-6,-16,16,66,96,118,130,75,-47,-92,43,223,239,151,219,
		440,475,226,206,940,2100,2663,4980,865,49,-33,186,231,103,42,114,191,184,116,29,
		-47,-72,-21,60,96,68,31,32,63,87,76,39,7,14,55,85,67,18,-12,-3,
		21,34,29,6,-27,-49,-37,-2,16,0,-21,-16,0,0,0,
	},
	{	// ecf - 4
		0,0,0,48,7,-22,-29,-10,24,54,59,29,-36,-117,-185,-213,-185,-99,13,90,
		83,24,-5,23,53,47,38,56,67,57,75,107,16,-242,-440,-355,-120,-33,-47,152,
		501,472,-57,-292,544,1937,2277,6145,1240,153,47,200,152,36,64,134,74,-82,-208,-266,
		-268,-188,-42,65,74,56,89,133,114,44,-3,-1,17,29,29,-2,-76,-156,-187,-151,
		-85,-31,-5,7,20,32,24,-5,-20,6,48,62,0,0,0,
	}
};

static const INT16 qsound_filter_data2[209] = {
	// f2e - following 95 values used for "disable output" filter
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,

	// f73 - following 45 values used for "mode 2" filter (overlaps with f2e)
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,
	-371,-196,-268,-512,-303,-315,-184,-76,276,-256,298,196,990,236,1114,-126,4377,6549,791,

	// fa0 - filtering disabled (for 95-taps) (use fa3 or fa4 for mode2 filters)
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,-16384,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const INT16 adpcm_step_table[16] = {
	154, 154, 128, 102, 77, 58, 58, 58,
	58, 58, 58, 58, 77, 102, 128, 154
};

// DSP states
enum {
	STATE_INIT1		= 0x288,
	STATE_INIT2		= 0x61a,
	STATE_REFRESH1	= 0x039,
	STATE_REFRESH2	= 0x04f,
	STATE_NORMAL1	= 0x314,
	STATE_NORMAL2 	= 0x6b2
};

enum {
	PANTBL_LEFT		= 0,
	PANTBL_RIGHT	= 1,
	PANTBL_DRY		= 0,
	PANTBL_WET		= 1
};

static void init_pan_tables()
{
	int i;
	for(i=0;i<33;i++)
	{
		// dry mixing levels
		pan_tables[PANTBL_LEFT][PANTBL_DRY][i] = qsound_dry_mix_table[i];
		pan_tables[PANTBL_RIGHT][PANTBL_DRY][i] = qsound_dry_mix_table[32-i];
		// wet mixing levels
		pan_tables[PANTBL_LEFT][PANTBL_WET][i] = qsound_wet_mix_table[i];
		pan_tables[PANTBL_RIGHT][PANTBL_WET][i] = qsound_wet_mix_table[32-i];
		// linear panning, only for dry component. wet component is muted.
		pan_tables[PANTBL_LEFT][PANTBL_DRY][i+0x30] = qsound_linear_mix_table[i];
		pan_tables[PANTBL_RIGHT][PANTBL_DRY][i+0x30] = qsound_linear_mix_table[32-i];
	}
}

static void init_register_map()
{
	int i;

	// unused registers
	for(i=0;i<256;i++)
		register_map[i] = NULL;

	// PCM registers
	for(i=0;i<16;i++) // PCM voices
	{
		register_map[(i<<3)+0] = (UINT16*)&chip.voice[(i+1)%16].bank; // Bank applies to the next channel
		register_map[(i<<3)+1] = (UINT16*)&chip.voice[i].addr; // Current sample position and start position.
		register_map[(i<<3)+2] = (UINT16*)&chip.voice[i].rate; // 4.12 fixed point decimal.
		register_map[(i<<3)+3] = (UINT16*)&chip.voice[i].phase;
		register_map[(i<<3)+4] = (UINT16*)&chip.voice[i].loop_len;
		register_map[(i<<3)+5] = (UINT16*)&chip.voice[i].end_addr;
		register_map[(i<<3)+6] = (UINT16*)&chip.voice[i].volume;
		register_map[(i<<3)+7] = NULL;	// unused
		register_map[i+0x80] = (UINT16*)&chip.voice_pan[i];
		register_map[i+0xba] = (UINT16*)&chip.voice[i].echo;
	}

	// ADPCM registers
	for(i=0;i<3;i++) // ADPCM voices
	{
		// ADPCM sample rate is fixed to 8khz. (one channel is updated every third sample)
		register_map[(i<<2)+0xca] = (UINT16*)&chip.adpcm[i].start_addr;
		register_map[(i<<2)+0xcb] = (UINT16*)&chip.adpcm[i].end_addr;
		register_map[(i<<2)+0xcc] = (UINT16*)&chip.adpcm[i].bank;
		register_map[(i<<2)+0xcd] = (UINT16*)&chip.adpcm[i].volume;
		register_map[i+0xd6] = (UINT16*)&chip.adpcm[i].flag; // non-zero to start ADPCM playback
		register_map[i+0x90] = (UINT16*)&chip.voice_pan[16+i];
	}

	// QSound registers
	register_map[0x93] = (UINT16*)&chip.echo.feedback;
	register_map[0xd9] = (UINT16*)&chip.echo.end_pos;
	register_map[0xe2] = (UINT16*)&chip.delay_update; // non-zero to update delays
	register_map[0xe3] = (UINT16*)&chip.next_state;
	for(i=0;i<2;i++)  // left, right
	{
		// Wet
		register_map[(i<<1)+0xda] = (UINT16*)&chip.filter[i].table_pos;
		register_map[(i<<1)+0xde] = (UINT16*)&chip.wet[i].delay;
		register_map[(i<<1)+0xe4] = (UINT16*)&chip.wet[i].volume;
		// Dry
		register_map[(i<<1)+0xdb] = (UINT16*)&chip.alt_filter[i].table_pos;
		register_map[(i<<1)+0xdf] = (UINT16*)&chip.dry[i].delay;
		register_map[(i<<1)+0xe5] = (UINT16*)&chip.dry[i].volume;
	}
}

inline INT16 get_sample(UINT16 bank, UINT16 address)
{
	UINT32 rom_addr;
	UINT32 rom_mask = 0;
	UINT8 sample_data;

	if(nCpsQSamLen)
		rom_mask = nCpsQSamLen - 1;

	if (! rom_mask)
		return 0;	// no ROM loaded
	if (! (bank & 0x8000))
		return 0;	// ignore attempts to read from DSP program ROM

	bank &= 0x7FFF;
	rom_addr = (bank << 16) | (address << 0);

	sample_data = CpsQSam[rom_addr & rom_mask];

	return (INT16)((sample_data << 8) & 0xff00);
}

INLINE const INT16* get_filter_table(UINT16 offset)
{
	int index;

	if (offset >= 0xf2e && offset < 0xfff)
		return &qsound_filter_data2[offset-0xf2e];	// overlapping filter data

	index = (offset-0xd53)/95;
	if(index >= 0 && index < 5)
		return qsound_filter_data[index];	// normal tables

	return NULL;	// no filter found.
}

/********************************************************************/

// updates one DSP sample
static void update_sample()
{
	switch(chip.state)
	{
		default:
		case STATE_INIT1:
		case STATE_INIT2:
			state_init();
			return;
		case STATE_REFRESH1:
			state_refresh_filter_1();
			return;
		case STATE_REFRESH2:
			state_refresh_filter_2();
			return;
		case STATE_NORMAL1:
		case STATE_NORMAL2:
			state_normal_update();
			return;
	}
}

// Initialization routine
static void state_init()
{
	int mode = (chip.state == STATE_INIT2) ? 1 : 0;
	int i;

	// we're busy for 4 samples, including the filter refresh.
	if(chip.state_counter >= 2)
	{
		chip.state_counter = 0;
		chip.state = chip.next_state;
		return;
	}
	else if(chip.state_counter == 1)
	{
		chip.state_counter++;
		return;
	}

	memset(chip.voice, 0, sizeof(chip.voice));
	memset(chip.adpcm, 0, sizeof(chip.adpcm));
	memset(chip.filter, 0, sizeof(chip.filter));
	memset(chip.alt_filter, 0, sizeof(chip.alt_filter));
	memset(chip.wet, 0, sizeof(chip.wet));
	memset(chip.dry, 0, sizeof(chip.dry));
	memset(&chip.echo, 0, sizeof(chip.echo));

	for(i=0;i<19;i++)
	{
		chip.voice_pan[i] = 0x120;
		chip.voice_output[i] = 0;
	}

	for(i=0;i<16;i++)
		chip.voice[i].bank = 0x8000;
	for(i=0;i<3;i++)
		chip.adpcm[i].bank = 0x8000;

	if(mode == 0)
	{
		// mode 1
		chip.wet[0].delay = 0;
		chip.dry[0].delay = 46;
		chip.wet[1].delay = 0;
		chip.dry[1].delay = 48;
		chip.filter[0].table_pos = 0xdb2;
		chip.filter[1].table_pos = 0xe11;
		chip.echo.end_pos = 0x554 + 6;
		chip.next_state = STATE_REFRESH1;
	}
	else
	{
		// mode 2
		chip.wet[0].delay = 1;
		chip.dry[0].delay = 0;
		chip.wet[1].delay = 0;
		chip.dry[1].delay = 0;
		chip.filter[0].table_pos = 0xf73;
		chip.filter[1].table_pos = 0xfa4;
		chip.alt_filter[0].table_pos = 0xf73;
		chip.alt_filter[1].table_pos = 0xfa4;
		chip.echo.end_pos = 0x53c + 6;
		chip.next_state = STATE_REFRESH2;
	}

	chip.wet[0].volume = 0x3fff;
	chip.dry[0].volume = 0x3fff;
	chip.wet[1].volume = 0x3fff;
	chip.dry[1].volume = 0x3fff;

	chip.delay_update = 1;
	chip.ready_flag = 0;
	chip.state_counter = 1;
}

// Updates filter parameters for mode 1
static void state_refresh_filter_1()
{
	const INT16 *table;
	int ch;

	for(ch=0; ch<2; ch++)
	{
		chip.filter[ch].delay_pos = 0;
		chip.filter[ch].tap_count = 95;

		table = get_filter_table(chip.filter[ch].table_pos);
		if (table != NULL)
			memcpy(chip.filter[ch].taps, table, 95 * sizeof(INT16));
	}

	chip.state = chip.next_state = STATE_NORMAL1;
}

// Updates filter parameters for mode 2
static void state_refresh_filter_2()
{
	const INT16 *table;
	int ch;

	for(ch=0; ch<2; ch++)
	{
		chip.filter[ch].delay_pos = 0;
		chip.filter[ch].tap_count = 45;

		table = get_filter_table(chip.filter[ch].table_pos);
		if (table != NULL)
			memcpy(chip.filter[ch].taps, table, 45 * sizeof(INT16));

		chip.alt_filter[ch].delay_pos = 0;
		chip.alt_filter[ch].tap_count = 44;

		table = get_filter_table(chip.alt_filter[ch].table_pos);
		if (table != NULL)
			memcpy(chip.alt_filter[ch].taps, table, 44 * sizeof(INT16));
	}

	chip.state = chip.next_state = STATE_NORMAL2;
}

// Updates a PCM voice. There are 16 voices, each are updated every sample
// with full rate and volume control.
INLINE INT16 pcm_update(int voice_no, INT32 *echo_out)
{
	struct qsound_voice *v = &chip.voice[voice_no];
	INT32 new_phase;
	INT16 output;

	// Read sample from rom and apply volume
	output = (v->volume * get_sample(v->bank, v->addr))>>14;

	*echo_out += (output * v->echo)<<2;

	// Add delta to the phase and loop back if required
	new_phase = v->rate + ((v->addr<<12) | (v->phase>>4));

	if((new_phase>>12) >= v->end_addr)
		new_phase -= (v->loop_len<<12);

	new_phase = CLAMP(new_phase, -0x8000000, 0x7FFFFFF);
	v->addr = new_phase>>12;
	v->phase = (new_phase<<4)&0xffff;

	return output;
}

// Updates an ADPCM voice. There are 3 voices, one is updated every sample
// (effectively making the ADPCM rate 1/3 of the master sample rate), and
// volume is set when starting samples only.
// The ADPCM algorithm is supposedly similar to Yamaha ADPCM. It also seems
// like Capcom never used it, so this was not emulated in the earlier QSound
// emulators.
INLINE void adpcm_update(int voice_no, int nibble)
{
	struct qsound_adpcm *v = &chip.adpcm[voice_no];

	INT32 delta;
	INT8 step;

	if(!nibble)
	{
		// Mute voice when it reaches the end address.
		if(v->cur_addr == v->end_addr)
			v->cur_vol = 0;

		// Playback start flag
		if(v->flag)
		{
			chip.voice_output[16+voice_no] = 0;
			v->flag = 0;
			v->step_size = 10;
			v->cur_vol = v->volume;
			v->cur_addr = v->start_addr;
		}

		// get top nibble
		step = get_sample(v->bank, v->cur_addr) >> 8;
	}
	else
	{
		// get bottom nibble
		step = get_sample(v->bank, v->cur_addr++) >> 4;
	}

	// shift with sign extend
	step >>= 4;

	// delta = (0.5 + abs(v->step)) * v->step_size
	delta = ((1+abs(step<<1)) * v->step_size)>>1;
	if(step <= 0)
		delta = -delta;
	delta += chip.voice_output[16+voice_no];
	delta = CLAMP(delta,-32768,32767);

	chip.voice_output[16+voice_no] = (delta * v->cur_vol)>>16;

	v->step_size = (adpcm_step_table[8+step] * v->step_size) >> 6;
	v->step_size = CLAMP(v->step_size, 1, 2000);
}

// The echo effect is pretty simple. A moving average filter is used on
// the output from the delay line to smooth samples over time.
INLINE INT16 echo(struct qsound_echo *r,INT32 input)
{
	// get average of last 2 samples from the delay line
	INT32 new_sample;
	INT32 old_sample = r->delay_line[r->delay_pos];
	INT32 last_sample = r->last_sample;

	r->last_sample = old_sample;
	old_sample = (old_sample+last_sample) >> 1;

	// add current sample to the delay line
	new_sample = input + ((old_sample * r->feedback)<<2);
	r->delay_line[r->delay_pos++] = new_sample>>16;

	if(r->delay_pos >= r->length)
		r->delay_pos = 0;

	return old_sample;
}

// Process a sample update
static void state_normal_update()
{
	int v, ch;
	INT32 echo_input = 0;
	INT16 echo_output;

	chip.ready_flag = 0x80;

	// recalculate echo length
	if(chip.state == STATE_NORMAL2)
		chip.echo.length = chip.echo.end_pos - 0x53c;
	else
		chip.echo.length = chip.echo.end_pos - 0x554;

	chip.echo.length = CLAMP(chip.echo.length, 0, 1024);

	// update PCM voices
	for(v=0; v<16; v++)
		chip.voice_output[v] = pcm_update(v, &echo_input);

	// update ADPCM voices (one every third sample)
	adpcm_update(chip.state_counter % 3, chip.state_counter / 3);

	echo_output = echo(&chip.echo,echo_input);

	// now, we do the magic stuff
	for(ch=0; ch<2; ch++)
	{
		// Echo is output on the unfiltered component of the left channel and
		// the filtered component of the right channel.
		INT32 wet = (ch == 1) ? echo_output<<14 : 0;
		INT32 dry = (ch == 0) ? echo_output<<14 : 0;
		INT32 output = 0;

		for(v=0; v<19; v++)
		{
			UINT16 pan_index = chip.voice_pan[v]-0x110;
			if(pan_index > 97)
				pan_index = 97;

			// Apply different volume tables on the dry and wet inputs.
			dry -= (chip.voice_output[v] * pan_tables[ch][PANTBL_DRY][pan_index]);
			wet -= (chip.voice_output[v] * pan_tables[ch][PANTBL_WET][pan_index]);
		}

		// Saturate accumulated voices
		dry = CLAMP(dry, -0x1fffffff, 0x1fffffff) << 2;
		wet = CLAMP(wet, -0x1fffffff, 0x1fffffff) << 2;

		// Apply FIR filter on 'wet' input
		wet = fir(&chip.filter[ch], wet >> 16);

		// in mode 2, we do this on the 'dry' input too
		if(chip.state == STATE_NORMAL2)
			dry = fir(&chip.alt_filter[ch], dry >> 16);

		// output goes through a delay line and attenuation
		output = (delay(&chip.wet[ch], wet) + delay(&chip.dry[ch], dry));

		// DSP round function
		output = ((output + 0x2000) & ~0x3fff) >> 14;
		chip.out[ch] = CLAMP(output, -0x7fff, 0x7fff);

		if(chip.delay_update)
		{
			delay_update(&chip.wet[ch]);
			delay_update(&chip.dry[ch]);
		}
	}

	chip.delay_update = 0;

	// after 6 samples, the next state is executed.
	chip.state_counter++;
	if(chip.state_counter > 5)
	{
		chip.state_counter = 0;
		chip.state = chip.next_state;
	}
}

// Apply the FIR filter used as the Q1 transfer function
INLINE INT32 fir(struct qsound_fir *f, INT16 input)
{
	INT32 output = 0, tap = 0;

	for(; tap < (f->tap_count-1); tap++)
	{
		output -= (f->taps[tap] * f->delay_line[f->delay_pos++])<<2;

		if(f->delay_pos >= f->tap_count-1)
			f->delay_pos = 0;
	}

	output -= (f->taps[tap] * input)<<2;

	f->delay_line[f->delay_pos++] = input;
	if(f->delay_pos >= f->tap_count-1)
		f->delay_pos = 0;

	return output;
}

// Apply delay line and component volume
INLINE INT32 delay(struct qsound_delay *d, INT32 input)
{
	INT32 output;

	d->delay_line[d->write_pos++] = input>>16;
	if(d->write_pos >= 51)
		d->write_pos = 0;

	output = d->delay_line[d->read_pos++]*d->volume;
	if(d->read_pos >= 51)
		d->read_pos = 0;

	return output;
}

// Update the delay read position to match new delay length
INLINE void delay_update(struct qsound_delay *d)
{
	INT16 new_read_pos = (d->write_pos - d->delay) % 51;
	if(new_read_pos < 0)
		new_read_pos += 51;

	d->read_pos = new_read_pos;
}

//==========================================

static INT64 CalcAdvance()
{
	if (nQscRate) {
		return (INT64)0x1000 * nQscClock / nQscClockDivider / nQscRate;
	}
	return 0;
}

void QscReset()
{
	chip.ready_flag = 0;
	chip.out[0] = chip.out[1] = 0;
	chip.state = 0;
	chip.state_counter = 0;
	nDelta = 0;
	memset(interpolate_buffer, 0, sizeof(interpolate_buffer));
}

void QscExit()
{
	nQscRate = 0;
}

INT32 QscInit(INT32 nRate)
{
	nQscRate = nRate;
	nDelta = 0;

	init_pan_tables();
	init_register_map();

	QsndGain[BURN_SND_QSND_OUTPUT_1] = 1.00;
	QsndGain[BURN_SND_QSND_OUTPUT_2] = 1.00;
	QsndOutputDir[BURN_SND_QSND_OUTPUT_1] = BURN_SND_ROUTE_LEFT;
	QsndOutputDir[BURN_SND_QSND_OUTPUT_2] = BURN_SND_ROUTE_RIGHT;

	QscReset();

	return 0;
}

void QscSetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
	QsndGain[nIndex] = nVolume;
	QsndOutputDir[nIndex] = nRouteDir;
}

INT32 QscScan(INT32 nAction)
{
	struct BurnArea ba;
	char szName[16];

	sprintf(szName, "QSound");

	ba.Data		= &chip;
	ba.nLen		= STRUCT_SIZE_HELPER(struct qsound_chip, ready_flag);
	ba.nAddress = 0;
	ba.szName	= szName;
	BurnAcb(&ba);

	return 0;
}

void QscNewFrame()
{
	nPos = 0;
}

static inline void QscSyncQsnd()
{
	if (pBurnSoundOut) QscUpdate(ZetTotalCycles() * nBurnSoundLen / nCpsZ80Cycles);
}

void QscWrite(INT32 a, INT32 d)
{
	UINT16 *destination = register_map[a];
	QscSyncQsnd();
	if(destination)
		*destination = d;
	chip.ready_flag = 0;
}

UINT8 QscRead()
{
	QscSyncQsnd();
	return chip.ready_flag;
}

INT32 QscUpdate(INT32 nEnd)
{
	INT32 nLen;

	if (nEnd > nBurnSoundLen) {
		nEnd = nBurnSoundLen;
	}

	nLen = nEnd - nPos;

	if (nLen <= 0) {
		return 0;
	}

	if (nInterpolation < 3) {

		INT16 *pDest = pBurnSoundOut + (nPos << 1);
		for (INT32 i = 0; i < nLen; i++) {
			INT32 nLeftSample = 0, nRightSample = 0;
			INT32 nLeftOut = 0, nRightOut = 0;

			nDelta += CalcAdvance();
			while(nDelta > 0xfff)
			{
				interpolate_buffer[0][0] = chip.out[0];
				interpolate_buffer[1][0] = chip.out[1];

				update_sample();
				nDelta -= 0x1000;
			}

			nLeftOut = interpolate_buffer[0][0] + (((chip.out[0] - interpolate_buffer[0][0]) * nDelta) >> 12);
			nRightOut = interpolate_buffer[1][0] + (((chip.out[1] - interpolate_buffer[1][0]) * nDelta) >> 12);

			if ((QsndOutputDir[BURN_SND_QSND_OUTPUT_1] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				nLeftSample += (INT32)(nLeftOut * QsndGain[BURN_SND_QSND_OUTPUT_1]);
			}
			if ((QsndOutputDir[BURN_SND_QSND_OUTPUT_1] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				nRightSample += (INT32)(nLeftOut * QsndGain[BURN_SND_QSND_OUTPUT_1]);
			}

			if ((QsndOutputDir[BURN_SND_QSND_OUTPUT_2] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
				nLeftSample += (INT32)(nRightOut * QsndGain[BURN_SND_QSND_OUTPUT_2]);
			}
			if ((QsndOutputDir[BURN_SND_QSND_OUTPUT_2] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
				nRightSample += (INT32)(nRightOut * QsndGain[BURN_SND_QSND_OUTPUT_2]);
			}

			pDest[(i << 1) + 0] = BURN_SND_CLIP(nLeftSample);
			pDest[(i << 1) + 1] = BURN_SND_CLIP(nRightSample);
		}
		nPos = nEnd;

		return 0;
	}

	INT16 *pDest = pBurnSoundOut + (nPos << 1);
	for (INT32 i = 0; i < nLen; i++) {
		INT32 nLeftSample = 0, nRightSample = 0;
		INT32 nLeftOut = 0, nRightOut = 0;

		nDelta += CalcAdvance();
		while(nDelta > 0xfff)
		{
			update_sample();
			interpolate_buffer[0][0] = interpolate_buffer[0][1];
			interpolate_buffer[1][0] = interpolate_buffer[1][1];
			interpolate_buffer[0][1] = interpolate_buffer[0][2];
			interpolate_buffer[1][1] = interpolate_buffer[1][2];
			interpolate_buffer[0][2] = interpolate_buffer[0][3];
			interpolate_buffer[1][2] = interpolate_buffer[1][3];
			interpolate_buffer[0][3] = chip.out[0];
			interpolate_buffer[1][3] = chip.out[1];
			nDelta -= 0x1000;
		}

		nLeftOut = INTERPOLATE4PS_16BIT(nDelta,
								  interpolate_buffer[0][0],
								  interpolate_buffer[0][1],
								  interpolate_buffer[0][2],
								  interpolate_buffer[0][3]);
		nRightOut = INTERPOLATE4PS_16BIT(nDelta,
								  interpolate_buffer[1][0],
								  interpolate_buffer[1][1],
								  interpolate_buffer[1][2],
								  interpolate_buffer[1][3]);

		if ((QsndOutputDir[BURN_SND_QSND_OUTPUT_1] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(nLeftOut * QsndGain[BURN_SND_QSND_OUTPUT_1]);
		}
		if ((QsndOutputDir[BURN_SND_QSND_OUTPUT_1] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)(nLeftOut * QsndGain[BURN_SND_QSND_OUTPUT_1]);
		}

		if ((QsndOutputDir[BURN_SND_QSND_OUTPUT_2] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)(nRightOut * QsndGain[BURN_SND_QSND_OUTPUT_2]);
		}
		if ((QsndOutputDir[BURN_SND_QSND_OUTPUT_2] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)(nRightOut * QsndGain[BURN_SND_QSND_OUTPUT_2]);
		}

		pDest[(i << 1) + 0] = BURN_SND_CLIP(nLeftSample);
		pDest[(i << 1) + 1] = BURN_SND_CLIP(nRightSample);
	}
	nPos = nEnd;

	return 0;
}
