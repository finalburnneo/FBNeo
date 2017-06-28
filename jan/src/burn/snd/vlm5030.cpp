/*
    vlm5030.c

    VLM5030 emulator

    Written by Tatsuyuki Satoh
    Based on TMS5220 simulator (tms5220.c)

  note:
    memory read cycle(==sampling rate) = 122.9u(440clock)
    interpolator (LC8109 = 2.5ms)      = 20 * samples(125us)
    frame time (20ms)                  =  4 * interpolator
    9bit DAC is composed of 5bit Physical and 3bitPWM.

  todo:
    Noise Generator circuit without 'rand()' function.

----------- command format (Analytical result) ----------

1)end of speech (8bit)
:00000011:

2)silent some frame (8bit)
:????SS01:

SS : number of silent frames
   00 = 2 frame
   01 = 4 frame
   10 = 6 frame
   11 = 8 frame

3)-speech frame (48bit)
function:   6th  :  5th   :   4th  :   3rd  :   2nd  : 1st    :
end     :   ---  :  ---   :   ---  :   ---  :   ---  :00000011:
silent  :   ---  :  ---   :   ---  :   ---  :   ---  :0000SS01:
speech  :11111122:22233334:44455566:67778889:99AAAEEE:EEPPPPP0:

EEEEE  : energy : volume 0=off,0x1f=max
PPPPP  : pitch  : 0=noize , 1=fast,0x1f=slow
111111 : K1     : 48=off
22222  : K2     : 0=off,1=+min,0x0f=+max,0x10=off,0x11=+max,0x1f=-min
                : 16 == special function??
3333   : K3     : 0=off,1=+min,0x07=+max,0x08=-max,0x0f=-min
4444   : K4     :
555    : K5     : 0=off,1=+min,0x03=+max,0x04=-max,0x07=-min
666    : K6     :
777    : K7     :
888    : K8     :
999    : K9     :
AAA    : K10    :

 ---------- chirp table information ----------

DAC PWM cycle == 88system clock , (11clock x 8 pattern) = 40.6KHz
one chirp     == 5 x PWM cycle == 440systemclock(8,136Hz)

chirp  0   : volume 10- 8 : with filter
chirp  1   : volume  8- 6 : with filter
chirp  2   : volume  6- 4 : with filter
chirp  3   : volume   4   : no filter ??
chirp  4- 5: volume  4- 2 : with filter
chirp  6-11: volume  2- 0 : with filter
chirp 12-..: volume   0   : silent

 ---------- digial output information ----------
 when ME pin = high , some status output to A0..15 pins

  A0..8   : DAC output value (abs)
  A9      : DAC sign flag , L=minus,H=Plus
  A10     : energy reload flag (pitch pulse)
  A11..15 : unknown

  [DAC output value(signed 6bit)] = A9 ? A0..8 : -(A0..8)

*/
/*
	Ported from MAME 0.120
	29/01/14
*/

#include "burnint.h"
#include "vlm5030.h"

#define CHIP_COUNT	1
#define LEFT		0x00
#define RIGHT		0x01

/* interpolator per frame   */
#define FR_SIZE 4
/* samples per interpolator */
#define IP_SIZE_SLOWER  (240/FR_SIZE)
#define IP_SIZE_SLOW    (200/FR_SIZE)
#define IP_SIZE_NORMAL  (160/FR_SIZE)
#define IP_SIZE_FAST    (120/FR_SIZE)
#define IP_SIZE_FASTER  ( 80/FR_SIZE)

struct vlm5030_info
{
	/* need to save state */

	UINT8 *rom;
	INT32 address_mask;
	UINT16 address;
	UINT8 pin_BSY;
	UINT8 pin_ST;
	UINT8 pin_VCU;
	UINT8 pin_RST;
	UINT8 latch_data;
	UINT16 vcu_addr_h;
	UINT8 parameter;
	UINT8 phase;

	/* state of option paramter */
	INT32 frame_size;
	INT32 pitch_offset;
	UINT8 interp_step;

	UINT8 interp_count;       /* number of interp periods    */
	UINT8 sample_count;       /* sample number within interp */
	UINT8 pitch_count;

	/* these contain data describing the current and previous voice frames */
	UINT16 old_energy;
	UINT8 old_pitch;
	INT16  old_k[10];
	UINT16 target_energy;
	UINT8 target_pitch;
	INT16 target_k[10];

	UINT16 new_energy;
	UINT8 new_pitch;
	INT16 new_k[10];

	/* these are all used to contain the current state of the sound generation */
	UINT32 current_energy;
	UINT32 current_pitch;
	INT32 current_k[10];

	INT32 x[10];

	// FBA-specific variables
	INT16 *output;
	INT32 samples_per_frame;
	UINT32 (*pSyncCallback)(int samples_per_frame);
	INT32 current_position;
	INT32 fill_buffer_hack;
	double gain[2];
	INT32 output_dir[2];
	INT32 bAdd;
};

static struct vlm5030_info vlm5030_chips[CHIP_COUNT];

/* phase value */
enum {
	PH_RESET,
	PH_IDLE,
	PH_SETUP,
	PH_WAIT,
	PH_RUN,
	PH_STOP,
	PH_END
};

/*
  speed parameter
SPC SPB SPA
 1   0   1  more slow (05h)     : 42ms   (150%) : 60sample
 1   1   x  slow      (06h,07h) : 34ms   (125%) : 50sample
 x   0   0  normal    (00h,04h) : 25.6ms (100%) : 40samplme
 0   0   1  fast      (01h)     : 20.2ms  (75%) : 30sample
 0   1   x  more fast (02h,03h) : 12.2ms  (50%) : 20sample
*/
static const INT32 vlm5030_speed_table[8] =
{
 IP_SIZE_NORMAL,
 IP_SIZE_FAST,
 IP_SIZE_FASTER,
 IP_SIZE_FASTER,
 IP_SIZE_NORMAL,
 IP_SIZE_SLOWER,
 IP_SIZE_SLOW,
 IP_SIZE_SLOW
};

static const char VLM_NAME[] = "VLM5030";

/* ROM Tables */

/* This is the energy lookup table */

/* sampled from real chip */
static const UINT16 energytable[0x20] =
{
	  0,  2,  4,  6, 10, 12, 14, 18, /*  0-7  */
	 22, 26, 30, 34, 38, 44, 48, 54, /*  8-15 */
	 62, 68, 76, 84, 94,102,114,124, /* 16-23 */
	136,150,164,178,196,214,232,254  /* 24-31 */
};

/* This is the pitch lookup table */
static const UINT8 pitchtable [0x20]=
{
   1,                               /* 0     : random mode */
   22,                              /* 1     : start=22    */
   23, 24, 25, 26, 27, 28, 29, 30,  /*  2- 9 : 1step       */
   32, 34, 36, 38, 40, 42, 44, 46,  /* 10-17 : 2step       */
   50, 54, 58, 62, 66, 70, 74, 78,  /* 18-25 : 4step       */
   86, 94, 102,110,118,126          /* 26-31 : 8step       */
};

static const INT16 K1_table[] = {
  -24898,  -25672,  -26446,  -27091,  -27736,  -28252,  -28768,  -29155,
  -29542,  -29929,  -30316,  -30574,  -30832,  -30961,  -31219,  -31348,
  -31606,  -31735,  -31864,  -31864,  -31993,  -32122,  -32122,  -32251,
  -32251,  -32380,  -32380,  -32380,  -32509,  -32509,  -32509,  -32509,
   24898,   23995,   22963,   21931,   20770,   19480,   18061,   16642,
   15093,   13416,   11610,    9804,    7998,    6063,    3999,    1935,
       0,   -1935,   -3999,   -6063,   -7998,   -9804,  -11610,  -13416,
  -15093,  -16642,  -18061,  -19480,  -20770,  -21931,  -22963,  -23995
};
static const INT16 K2_table[] = {
       0,   -3096,   -6321,   -9417,  -12513,  -15351,  -18061,  -20770,
  -23092,  -25285,  -27220,  -28897,  -30187,  -31348,  -32122,  -32638,
       0,   32638,   32122,   31348,   30187,   28897,   27220,   25285,
   23092,   20770,   18061,   15351,   12513,    9417,    6321,    3096
};
static const INT16 K3_table[] = {
       0,   -3999,   -8127,  -12255,  -16384,  -20383,  -24511,  -28639,
   32638,   28639,   24511,   20383,   16254,   12255,    8127,    3999
};
static const INT16 K5_table[] = {
       0,   -8127,  -16384,  -24511,   32638,   24511,   16254,    8127
};

static INT32 get_bits(struct vlm5030_info *chip, INT32 sbit,INT32 bits)
{
	INT32 offset = chip->address + (sbit>>3);
	INT32 data;

	data = chip->rom[offset&chip->address_mask] +
	       (((INT32)chip->rom[(offset+1)&chip->address_mask])*256);
	data >>= (sbit&7);
	data &= (0xff>>(8-bits));

	return data;
}

/* get next frame */
static INT32 parse_frame (struct vlm5030_info *chip)
{
	UINT8 cmd;
	INT32 i;

	/* remember previous frame */
	chip->old_energy = chip->new_energy;
	chip->old_pitch = chip->new_pitch;
	for(i=0;i<=9;i++)
		chip->old_k[i] = chip->new_k[i];

	/* command byte check */
	cmd = chip->rom[chip->address&chip->address_mask];
	if( cmd & 0x01 )
	{	/* extend frame */
		chip->new_energy = chip->new_pitch = 0;
		for(i=0;i<=9;i++)
			chip->new_k[i] = 0;
		chip->address++;
		if( cmd & 0x02 )
		{	/* end of speech */

			/* logerror("VLM5030 %04X end \n",chip->address ); */
			return 0;
		}
		else
		{	/* silent frame */
			int nums = ( (cmd>>2)+1 )*2;
			/* logerror("VLM5030 %04X silent %d frame\n",chip->address,nums ); */
			return nums * FR_SIZE;
		}
	}
	/* pitch */
	chip->new_pitch  = ( pitchtable[get_bits(chip, 1,5)] + chip->pitch_offset )&0xff;
	/* energy */
	chip->new_energy = energytable[get_bits(chip, 6,5)];

	/* 10 K's */
	chip->new_k[9] = K5_table[get_bits(chip,11,3)];
	chip->new_k[8] = K5_table[get_bits(chip,14,3)];
	chip->new_k[7] = K5_table[get_bits(chip,17,3)];
	chip->new_k[6] = K5_table[get_bits(chip,20,3)];
	chip->new_k[5] = K5_table[get_bits(chip,23,3)];
	chip->new_k[4] = K5_table[get_bits(chip,26,3)];
	chip->new_k[3] = K3_table[get_bits(chip,29,4)];
	chip->new_k[2] = K3_table[get_bits(chip,33,4)];
	chip->new_k[1] = K2_table[get_bits(chip,37,5)];
	chip->new_k[0] = K1_table[get_bits(chip,42,6)];

	chip->address+=6;

	return FR_SIZE;
}

/* decode and buffering data */
static void vlm5030_update(struct vlm5030_info *chip)
{
	INT32 buf_count=0;
	INT32 interp_effect;
	INT32 i;
	INT32 u[11];

//------------------------------------------------------------------------------------------------------
	if (chip->pSyncCallback == NULL) return;
	INT32 position;

	if (chip->fill_buffer_hack) {
		position = chip->samples_per_frame;
		chip->fill_buffer_hack = 0;
	} else {
		position = chip->pSyncCallback(chip->samples_per_frame);
	}

	if (position > chip->samples_per_frame) position = chip->samples_per_frame;
	if (position == chip->current_position) return;

	INT16 *buffer = chip->output + chip->current_position;

	INT32 samples = position - chip->current_position;

	chip->current_position = position;

	if (samples <= 0) return;
//-------------------------------------------------------------------------------------------------------

	/* running */
	if( chip->phase == PH_RUN || chip->phase == PH_STOP )
	{
		/* playing speech */
		while (samples > 0)
		{
			INT32 current_val;

			/* check new interpolator or  new frame */
			if( chip->sample_count == 0 )
			{
				if( chip->phase == PH_STOP )
				{
					chip->phase = PH_END;
					chip->sample_count = 1;
					goto phase_stop; /* continue to end phase */
				}
				chip->sample_count = chip->frame_size;
				/* interpolator changes */
				if ( chip->interp_count == 0 )
				{
					/* change to new frame */
					chip->interp_count = parse_frame(chip); /* with change phase */
					if ( chip->interp_count == 0 )
					{	/* end mark found */
						chip->interp_count = FR_SIZE;
						chip->sample_count = chip->frame_size; /* end -> stop time */
						chip->phase = PH_STOP;
					}
					/* Set old target as new start of frame */
					chip->current_energy = chip->old_energy;
					chip->current_pitch = chip->old_pitch;
					for(i=0;i<=9;i++)
						chip->current_k[i] = chip->old_k[i];
					/* is this a zero energy frame? */
					if (chip->current_energy == 0)
					{
						chip->target_energy = 0;
						chip->target_pitch = chip->current_pitch;
						for(i=0;i<=9;i++)
							chip->target_k[i] = chip->current_k[i];
					}
					else
					{
						chip->target_energy = chip->new_energy;
						chip->target_pitch = chip->new_pitch;
						for(i=0;i<=9;i++)
							chip->target_k[i] = chip->new_k[i];
					}
				}
				/* next interpolator */
				/* Update values based on step values 25% , 50% , 75% , 100% */
				chip->interp_count -= chip->interp_step;
				/* 3,2,1,0 -> 1,2,3,4 */
				interp_effect = FR_SIZE - (chip->interp_count%FR_SIZE);
				chip->current_energy = chip->old_energy + (chip->target_energy - chip->old_energy) * interp_effect / FR_SIZE;
				if (chip->old_pitch > 1)
					chip->current_pitch = chip->old_pitch + (chip->target_pitch - chip->old_pitch) * interp_effect / FR_SIZE;
				for (i = 0; i <= 9 ; i++)
					chip->current_k[i] = chip->old_k[i] + (chip->target_k[i] - chip->old_k[i]) * interp_effect / FR_SIZE;
			}
			/* calcrate digital filter */
			if (chip->old_energy == 0)
			{
				/* generate silent samples here */
				current_val = 0x00;
			}
			else if (chip->old_pitch <= 1)
			{	/* generate unvoiced samples here */
				current_val = (rand()&1) ? chip->current_energy : -chip->current_energy;
			}
			else
			{
				/* generate voiced samples here */
				current_val = ( chip->pitch_count == 0) ? chip->current_energy : 0;
			}

			/* Lattice filter here */
			u[10] = current_val;
			for (i = 9; i >= 0; i--)
				u[i] = u[i+1] - ((chip->current_k[i] * chip->x[i]) / 32768);
			for (i = 9; i >= 1; i--)
				chip->x[i] = chip->x[i-1] + ((chip->current_k[i-1] * u[i-1]) / 32768);
			chip->x[0] = u[0];

			/* clipping, buffering */
			if (u[0] > 511)
				buffer[buf_count] = 511<<6;
			else if (u[0] < -511)
				buffer[buf_count] = -511<<6;
			else
				buffer[buf_count] = (u[0] << 6);
			buf_count++;

			/* sample count */
			chip->sample_count--;
			/* pitch */
			chip->pitch_count++;
			if (chip->pitch_count >= chip->current_pitch )
				chip->pitch_count = 0;
			/* size */
			samples--;
		}
	}
	/* stop phase */
phase_stop:
	switch( chip->phase )
	{
	case PH_SETUP:
		if( chip->sample_count <= samples)
		{
			chip->sample_count = 0;
			/* logerror("VLM5030 BSY=H\n" ); */
			/* pin_BSY = 1; */
			chip->phase = PH_WAIT;
		}
		else
		{
			chip->sample_count -= samples;
		}
		break;
	case PH_END:
		if( chip->sample_count <= samples)
		{
			chip->sample_count = 0;
			/* logerror("VLM5030 BSY=L\n" ); */
			chip->pin_BSY = 0;
			chip->phase = PH_IDLE;
		}
		else
		{
			chip->sample_count -= samples;
		}
	}
	/* silent buffering */
	while (samples > 0)
	{
		buffer[buf_count++] = 0x00;
		samples--;
	}
}

/* setup parameteroption when RST=H */
static void vlm5030_setup_parameter(struct vlm5030_info *chip, UINT8 param)
{
	/* latch parameter value */
	chip->parameter = param;

	/* bit 0,1 : 4800bps / 9600bps , interporator step */
	if(param&2) /* bit 1 = 1 , 9600bps */
		chip->interp_step = 4; /* 9600bps : no interporator */
	else if(param&1) /* bit1 = 0 & bit0 = 1 , 4800bps */
		chip->interp_step = 2; /* 4800bps : 2 interporator */
	else	/* bit1 = bit0 = 0 : 2400bps */
		chip->interp_step = 1; /* 2400bps : 4 interporator */

	/* bit 3,4,5 : speed (frame size) */
	chip->frame_size = vlm5030_speed_table[(param>>3) &7];

	/* bit 6,7 : low / high pitch */
	if(param&0x80)	/* bit7=1 , high pitch */
		chip->pitch_offset = -8;
	else if(param&0x40)	/* bit6=1 , low pitch */
		chip->pitch_offset = 8;
	else
		chip->pitch_offset = 0;
}

static void vlm5030_reset(struct vlm5030_info *chip)
{
	memset (chip->output, 0, chip->samples_per_frame * sizeof(INT16));
	chip->current_position = 0;
	chip->fill_buffer_hack = 0;

	srand (132);

	chip->phase = PH_RESET;
	chip->address = 0;
	chip->vcu_addr_h = 0;
	chip->pin_BSY = 0;

	chip->old_energy = chip->old_pitch = 0;
	chip->new_energy = chip->new_pitch = 0;
	chip->current_energy = chip->current_pitch = 0;
	chip->target_energy = chip->target_pitch = 0;
	memset(chip->old_k, 0, sizeof(chip->old_k));
	memset(chip->new_k, 0, sizeof(chip->new_k));
	memset(chip->current_k, 0, sizeof(chip->current_k));
	memset(chip->target_k, 0, sizeof(chip->target_k));
	chip->interp_count = chip->sample_count = chip->pitch_count = 0;
	memset(chip->x, 0, sizeof(chip->x));
	/* reset parameters */
	vlm5030_setup_parameter(chip, 0x00);
}

void vlm5030Reset(INT32 nChip)
{
#if defined FBA_DEBUG
	if (!DebugSnd_VLM5030Initted) bprintf(PRINT_ERROR, _T("vlm5030Reset called without init\n"));
#endif

	struct vlm5030_info *chip = &vlm5030_chips[nChip];

	vlm5030_reset(chip);
}

/* set speech rom address */
void vlm5030_set_rom(INT32 nChip, void *speech_rom)
{
#if defined FBA_DEBUG
	if (!DebugSnd_VLM5030Initted) bprintf(PRINT_ERROR, _T("vlm5030_set_rom called without init\n"));
#endif

	struct vlm5030_info *chip = &vlm5030_chips[nChip];;
	chip->rom = (UINT8 *)speech_rom;
}

/* get BSY pin level */
INT32 vlm5030_bsy(INT32 nChip)
{
#if defined FBA_DEBUG
	if (!DebugSnd_VLM5030Initted) bprintf(PRINT_ERROR, _T("vlm5030_bsy called without init\n"));
#endif

	struct vlm5030_info *chip = &vlm5030_chips[nChip];
	vlm5030_update(chip);
	return chip->pin_BSY;
}

/* latch contoll data */
void vlm5030_data_write(INT32 nChip, UINT8 data)
{
#if defined FBA_DEBUG
	if (!DebugSnd_VLM5030Initted) bprintf(PRINT_ERROR, _T("vlm5030_data_write called without init\n"));
#endif

	struct vlm5030_info *chip = &vlm5030_chips[nChip];

	chip->latch_data = (UINT8)data;
}

/* set RST pin level : reset / set table address A8-A15 */
void vlm5030_rst(INT32 nChip, INT32 pin)
{
#if defined FBA_DEBUG
	if (!DebugSnd_VLM5030Initted) bprintf(PRINT_ERROR, _T("vlm5030_rst called without init\n"));
#endif

	struct vlm5030_info *chip = &vlm5030_chips[nChip];
	if( chip->pin_RST )
	{
		if( !pin )
		{	/* H -> L : latch parameters */
			chip->pin_RST = 0;
			vlm5030_setup_parameter(chip, chip->latch_data);
		}
	}
	else
	{
		if( pin )
		{	/* L -> H : reset chip */
			chip->pin_RST = 1;
			if( chip->pin_BSY )
			{
				vlm5030_reset(chip);
			}
		}
	}
}

/* set VCU pin level : ?? unknown */
void vlm5030_vcu(INT32 nChip, INT32 pin)
{
#if defined FBA_DEBUG
	if (!DebugSnd_VLM5030Initted) bprintf(PRINT_ERROR, _T("vlm5030_vcu called without init\n"));
#endif

	struct vlm5030_info *chip = &vlm5030_chips[nChip];
	/* direct mode / indirect mode */
	chip->pin_VCU = pin;
	return;
}

/* set ST pin level  : set table address A0-A7 / start speech */
void vlm5030_st(INT32 nChip, INT32 pin)
{
#if defined FBA_DEBUG
	if (!DebugSnd_VLM5030Initted) bprintf(PRINT_ERROR, _T("vlm5030_st called without init\n"));
#endif

	struct vlm5030_info *chip = &vlm5030_chips[nChip];
	INT32 table;

	if( chip->pin_ST != pin )
	{
		/* pin level is change */
		if( !pin )
		{	/* H -> L */
			chip->pin_ST = 0;

			if( chip->pin_VCU )
			{	/* direct access mode & address High */
				chip->vcu_addr_h = ((INT32)chip->latch_data<<8) + 0x01;
			}
			else
			{
				/* start speech */
				/* check access mode */
				if( chip->vcu_addr_h )
				{	/* direct access mode */
					chip->address = (chip->vcu_addr_h&0xff00) + chip->latch_data;
					chip->vcu_addr_h = 0;
				}
				else
				{	/* indirect accedd mode */
					table = (chip->latch_data&0xfe) + (((INT32)chip->latch_data&1)<<8);
					chip->address = (((INT32)chip->rom[table&chip->address_mask])<<8)
					                |        chip->rom[(table+1)&chip->address_mask];
				}
				vlm5030_update(chip);
				/* logerror("VLM5030 %02X start adr=%04X\n",table/2,chip->address ); */
				/* reset process status */
				chip->sample_count = chip->frame_size;
				chip->interp_count = FR_SIZE;
				/* clear filter */
				/* start after 3 sampling cycle */
				chip->phase = PH_RUN;
			}
		}
		else
		{	/* L -> H */
			chip->pin_ST = 1;
			/* setup speech , BSY on after 30ms? */
			chip->phase = PH_SETUP;
			chip->sample_count = 1; /* wait time for busy on */
			chip->pin_BSY = 1; /* */
		}
	}
}

/* start VLM5030 with sound rom              */
/* speech_rom == 0 -> use sampling data mode */
void vlm5030Init(INT32 nChip, INT32 clock, UINT32 (*pSyncCallback)(INT32 samples_per_frame), UINT8 *rom, INT32 rom_size, INT32 bAdd)
{
	DebugSnd_VLM5030Initted = 1;
	
	struct vlm5030_info *chip = &vlm5030_chips[nChip];
	memset (chip, 0, sizeof(vlm5030_info));

	/* reset input pins */
	chip->pin_RST = chip->pin_ST = chip->pin_VCU= 0;

	chip->samples_per_frame = (clock * 100) / 440 / nBurnFPS;

	chip->latch_data = 0;
	chip->output = (INT16*)BurnMalloc(chip->samples_per_frame * sizeof(INT16));

	vlm5030_reset(chip);

	chip->phase = PH_IDLE;

	chip->bAdd = bAdd;
	chip->rom = rom;
	chip->address_mask = rom_size - 1;
	chip->pSyncCallback = pSyncCallback;
	chip->gain[BURN_SND_VLM5030_ROUTE_1] = 1.00;
	chip->gain[BURN_SND_VLM5030_ROUTE_2] = 1.00;
	chip->output_dir[BURN_SND_VLM5030_ROUTE_1] = BURN_SND_ROUTE_BOTH;
	chip->output_dir[BURN_SND_VLM5030_ROUTE_2] = BURN_SND_ROUTE_BOTH;
}

void vlm5030SetRoute(INT32 nChip, INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBA_DEBUG
	if (!DebugSnd_VLM5030Initted) bprintf(PRINT_ERROR, _T("vlm5030SetRoute called without init\n"));
#endif

	struct vlm5030_info *chip = &vlm5030_chips[nChip];
	
	chip->gain[nIndex] = nVolume;
	chip->output_dir[nIndex] = nRouteDir;
}

void vlm5030Exit()
{
#if defined FBA_DEBUG
	if (!DebugSnd_VLM5030Initted) bprintf(PRINT_ERROR, _T("vlm5030Exit called without init\n"));
#endif

	if (!DebugSnd_VLM5030Initted) return;

	struct vlm5030_info *chip;

	for (INT32 i = 0; i < CHIP_COUNT; i++) {
		chip = &vlm5030_chips[i];

		if (chip->output) {
			BurnFree(chip->output);
			chip->output = NULL;
		}
	}
	
	DebugSnd_VLM5030Initted = 0;
}

void vlm5030Update(INT32 nChip, INT16 *buf, INT32 samples)
{
#if defined FBA_DEBUG
	if (!DebugSnd_VLM5030Initted) bprintf(PRINT_ERROR, _T("vlm5030Update called without init\n"));
#endif

	struct vlm5030_info *chip = &vlm5030_chips[nChip];

	if (pBurnSoundOut == NULL) {
		chip->current_position = 0;
		return;
	}

	chip->fill_buffer_hack = 1;
	vlm5030_update(chip);

	INT32 nAdd = chip->bAdd;
	INT16 *stream = chip->output;

	UINT32 step = (chip->samples_per_frame << 12) / nBurnSoundLen;
	INT32 pos = 0;

	for (INT32 i = 0; i < samples; i++) {
		if ((pos>>12) >= chip->samples_per_frame) pos = (chip->samples_per_frame-1) << 12;

		INT16 output = stream[pos>>12];

		/* write sound data to the buffer */
		INT32 nLeftSample = 0, nRightSample = 0;
			
		if ((chip->output_dir[BURN_SND_VLM5030_ROUTE_1] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)((output / 6) * chip->gain[BURN_SND_VLM5030_ROUTE_1]);
		}
		if ((chip->output_dir[BURN_SND_VLM5030_ROUTE_1] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)((output / 6) * chip->gain[BURN_SND_VLM5030_ROUTE_1]);
		}
		if ((chip->output_dir[BURN_SND_VLM5030_ROUTE_2] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
			nLeftSample += (INT32)((output / 6) * chip->gain[BURN_SND_VLM5030_ROUTE_2]);
		}
		if ((chip->output_dir[BURN_SND_VLM5030_ROUTE_2] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
			nRightSample += (INT32)((output / 6) * chip->gain[BURN_SND_VLM5030_ROUTE_2]);
		}
		
		nLeftSample = BURN_SND_CLIP(nLeftSample);
		nRightSample = BURN_SND_CLIP(nRightSample);
			
		if (nAdd) {
			buf[LEFT] = BURN_SND_CLIP(buf[LEFT] + nLeftSample);
			buf[RIGHT] = BURN_SND_CLIP(buf[RIGHT] + nRightSample);
		} else {
			buf[LEFT] = nLeftSample;
			buf[RIGHT] = nRightSample;
		}

		buf += 2;
		pos += step;
	}

	chip->current_position = 0;
	memset (chip->output, 0, chip->samples_per_frame * sizeof(INT16));
}

static void vlm5030_restore_state(INT32 nChip)
{
	struct vlm5030_info *chip = &vlm5030_chips[nChip];
	INT32 i;

	INT32 interp_effect = FR_SIZE - (chip->interp_count%FR_SIZE);
	/* restore parameter data */
	vlm5030_setup_parameter(chip, chip->parameter);

	/* restore current energy,pitch & filter */
	chip->current_energy = chip->old_energy + (chip->target_energy - chip->old_energy) * interp_effect / FR_SIZE;
	if (chip->old_pitch > 1)
		chip->current_pitch = chip->old_pitch + (chip->target_pitch - chip->old_pitch) * interp_effect / FR_SIZE;
	for (i = 0; i <= 9 ; i++)
		chip->current_k[i] = chip->old_k[i] + (chip->target_k[i] - chip->old_k[i]) * interp_effect / FR_SIZE;
}

INT32 vlm5030Scan(INT32 nAction)
{
#if defined FBA_DEBUG
	if (!DebugSnd_VLM5030Initted) bprintf(PRINT_ERROR, _T("vlm5030Scan called without init\n"));
#endif

	if (nAction & ACB_DRIVER_DATA) {
		for (INT32 i = 0; i < CHIP_COUNT; i++) {
			struct vlm5030_info *chip = &vlm5030_chips[i];

			SCAN_VAR(chip->address);
			SCAN_VAR(chip->pin_BSY);
			SCAN_VAR(chip->pin_ST);
			SCAN_VAR(chip->pin_VCU);
			SCAN_VAR(chip->pin_RST);
			SCAN_VAR(chip->latch_data);
			SCAN_VAR(chip->vcu_addr_h);
			SCAN_VAR(chip->parameter);
			SCAN_VAR(chip->phase);
			SCAN_VAR(chip->interp_count);
			SCAN_VAR(chip->sample_count);
			SCAN_VAR(chip->pitch_count);
			SCAN_VAR(chip->old_energy);
			SCAN_VAR(chip->old_pitch);
			SCAN_VAR(chip->old_k);
			SCAN_VAR(chip->target_energy);
			SCAN_VAR(chip->target_pitch);
			SCAN_VAR(chip->target_k);
			SCAN_VAR(chip->x);

			vlm5030_restore_state(i);
		}
	}

	return 0;
}
