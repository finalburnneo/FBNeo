/*****************************************************************************

  MAME/MESS NES APU CORE

  Based on the Nofrendo/Nosefart NES N2A03 sound emulation core written by
  Matthew Conte (matt@conte.com) and redesigned for use in MAME/MESS by
  Who Wants to Know? (wwtk@mail.com)

  This core is written with the advise and consent of Matthew Conte and is
  released under the GNU Public License.  This core is freely avaiable for
  use in any freeware project, subject to the following terms:

  Any modifications to this code must be duly noted in the source and
  approved by Matthew Conte and myself prior to public submission.

  timing notes:
  master = 21477270
  2A03 clock = master/12
  sequencer = master/89490 or CPU/7457

 *****************************************************************************

   NES_APU.C

   Actual NES APU interface.

   LAST MODIFIED 02/29/2004

   - Based on Matthew Conte's Nofrendo/Nosefart core and redesigned to
     use MAME system calls and to enable multiple APUs.  Sound at this
     point should be just about 100% accurate, though I cannot tell for
     certain as yet.

     A queue interface is also available for additional speed.  However,
     the implementation is not yet 100% (DPCM sounds are inaccurate),
     so it is disabled by default.

 *****************************************************************************

   BUGFIXES:

   - Various bugs concerning the DPCM channel fixed. (Oliver Achten)
   - Fixed $4015 read behaviour. (Oliver Achten) :: Actually, this broke it -dink
   - Oct xx, 2019 (dink)
	   completely re-impl dmc (dpcm) channel because fixing it was not an option
   - Nov 13, 2019 (dink)
	   MMC5 2x square: read/write address | 0x80
   - Feb 20, 2020 (dink)
       fix sweep unit
       add sweep unit augmented negation mode for first square channel

 *****************************************************************************/

/*
	Ported from MAME 0.120
	01/02/14
*/

#include "burnint.h"
#include "m6502_intf.h"
#include "nes_apu.h"
#include "nes_defs.h"
#include "downsample.h"

#define CHIP_NUM	2

#define LEFT	0
#define RIGHT	1

/* GLOBAL CONSTANTS */
#define  SYNCS_MAX1     0x20
#define  SYNCS_MAX2     0x80

/* GLOBAL VARIABLES */
struct nesapu_info
{
	apu_t   APU;			       /* Actual APUs */
	INT32   apu_incsize;           /* Adjustment increment */
	UINT32 samps_per_sync;        /* Number of samples per vsync */
	UINT32 buffer_size;           /* Actual buffer size in bytes */
	UINT32 real_rate;             /* Actual playback rate */
	UINT8   noise_lut[NOISE_LONG]; /* Noise sample lookup table */
	UINT32 vbl_times[0x20];       /* VBL durations in samples */
	UINT32 sync_times1[SYNCS_MAX1]; /* Samples per sync table */
	UINT32 sync_times2[SYNCS_MAX2]; /* Samples per sync table */

	float  tnd_table[0x100]; // see create_tndtab() for info
	float  square_table[0x100];

	// FBA-specific variables
	INT16 *stream;
	INT32 samples_per_frame;

	Downsampler resamp;

	UINT32 (*pSyncCallback)(INT32 samples_per_frame);
	INT32 current_position;
	INT32 fill_buffer_hack;
	INT32 dmc_direction_normal;
	double gain[2];
	INT32 output_dir[2];
	INT32 bAdd;
};

static nesapu_info nesapu_chips[CHIP_NUM];

static INT32 cycles_per_frame;
static UINT32 arcade_mode = 0; // not cycle accurate dpcm/dmc (needed for arcade)

#if 0
INT32 nes_scanline();
#endif

void nesapuSetArcade(INT32 mode)
{
	arcade_mode = mode;
}

static UINT32 nes_nesapu_sync(INT32 samples_rate)
{
	return (samples_rate * M6502TotalCycles()) / cycles_per_frame; /* ntsc: (341*262 / 3) + 0.5 pal: (341*312 / 3.2) + 0.5*/
}

//enum nesapu_mixermodes { MIXER_APU = 0x01, MIXER_EXT = 0x02 };
INT32 nesapu_mixermode;

/* Frame-IRQ stuff -dink */
static INT32 frame_irq_flag;
static INT32 mode4017;
static INT32 step4017;
static INT32 clocky;
static INT32 _4017hack = 0;

static UINT8 *dmc_buffer;
INT16 (*nes_ext_sound_cb)();

static const INT32 *noise_clocks;
static const INT32 *dpcm_clocks;

static int8 apu_dpcm(struct nesapu_info *info, dpcm_t *chan);

void nesapuSetDMCBitDirection(INT32 reversed)
{
	struct nesapu_info *info = &nesapu_chips[0];
	info->dmc_direction_normal = !reversed;
}

void nesapu_runclock(INT32 cycle)
{
	struct nesapu_info *info = &nesapu_chips[0];

	dmc_buffer[cycle] = dmc_buffer[cycle + 1] = apu_dpcm(info, &info->APU.dpcm);
	// [cycle + 1] = get around a bug in the mixer, evident when pausing ninja gaiden 1 or 2.
	// a proper fix would need to move the mixing into nesapuUpdate() (TBA). -dink

	// Frame-IRQ: we only emulate the frame-irq mode which is used by a few
	// games to split the screen or other neat tricks (Qix, Bee52...)
	clocky -= 2;
	if (clocky <= 0) {
		clocky += 14915;

		if (step4017 == 0 && (mode4017 & 0xc0) == 0x00) {
			frame_irq_flag = 1;
			//bprintf(0, _T("FRAME IRQ-apu.%d\n"), nCurrentFrame);
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		}

		step4017 = (step4017 + 1) & 3;
	}
}


/* INTERNAL FUNCTIONS */

/* INITIALIZE WAVE TIMES RELATIVE TO SAMPLE RATE */
static void create_vbltimes(UINT32* table,const UINT8 *vbl,UINT32 rate)
{
  INT32 i;

  for (i=0;i<0x20;i++)
    table[i]=vbl[i]*rate;
}

/* INITIALIZE SAMPLE TIMES IN TERMS OF VSYNCS */
static void create_syncs(struct nesapu_info *info, UINT64 sps)
{
  INT32 i;
  UINT64 val=sps;

  for (i=0;i<SYNCS_MAX1;i++)
  {
    info->sync_times1[i]=val;
    val+=sps;
  }

  val=0;
  for (i=0;i<SYNCS_MAX2;i++)
  {
    info->sync_times2[i]=val;
    info->sync_times2[i]>>=2;
    val+=sps;
  }
}

static void create_tndtab(float *buf)
{
	// triangle / noise / dmc lookup table
	// C/O Blargg's APU REF @ http://nesdev.com/apu_ref.txt
	for (int i = 0; i < 0x100; i++)
		buf[i] = 0.00;

	for (int i = 1; i < 254; i++)
		buf[i] = 163.67 / (24329.00 / i + 100.00);

}

static void create_squaretab(float *buf)
{
	// square lookup table
	// C/O Blargg's APU REF @ http://nesdev.com/apu_ref.txt
	for (int i = 0; i < 0x100; i++)
		buf[i] = 0.00;

	for (int i = 1; i < 254; i++)
		buf[i] = 95.52 / (8128.00 / i + 100.00);
}

/* TODO: sound channels should *ALL* have DC volume decay */

const UINT8 pulse_dty[4][8] = {
	{ 0, 0, 0, 0, 0, 0, 0, 1 },
	{ 0, 0, 0, 0, 0, 0, 1, 1 },
	{ 0, 0, 0, 0, 1, 1, 1, 1 },
	{ 1, 1, 1, 1, 1, 1, 0, 0 }
};

static void clock_square_sweep_single(struct nesapu_info *info, square_t *chan, int first_channel)
{
   /* freqsweeps */
   if ((chan->regs[1] & 0x80) && (chan->regs[1] & 7))
   {
      INT32 sweep_delay = info->sync_times1[((chan->regs[1] >> 4) & 7) + 1];
      chan->sweep_phase -= info->sync_times1[1];
	  while (chan->sweep_phase <= 0)
      {
         chan->sweep_phase += sweep_delay;
         if (chan->regs[1] & 8)
            chan->freq -= (chan->freq >> (chan->regs[1] & 7)) + (first_channel << 16);
         else
            chan->freq += chan->freq >> (chan->regs[1] & 7);
      }
   }
}

static void clock_square_sweep(struct nesapu_info *info)
{
	clock_square_sweep_single(info, &info->APU.squ[0], 1);
	clock_square_sweep_single(info, &info->APU.squ[1], 0);
}

/* OUTPUT SQUARE WAVE SAMPLE (VALUES FROM -16 to +15) */
static int8 apu_square(struct nesapu_info *info, square_t *chan, INT32 first_channel)
{
   INT32 env_delay;
   INT32 sweep_delay;
   int8 output;

   /* reg0: 0-3=volume, 4=envelope, 5=hold, 6-7=duty cycle
   ** reg1: 0-2=sweep shifts, 3=sweep inc/dec, 4-6=sweep length, 7=sweep on
   ** reg2: 8 bits of freq
   ** reg3: 0-2=high freq, 7-4=vbl length counter
   */

   if (false == chan->enabled)
      return 0;

   /* enveloping */
   env_delay = info->sync_times1[chan->regs[0] & 0x0F] + 1;

   /* decay is at a rate of (env_regs + 1) / 240 secs */
   if (chan->env_vol >= 0x10) {
	   chan->env_vol = 0;
	   chan->env_phase = env_delay; // restart env phase
	   chan->phaseacc = (chan->freq >> 16);
   }
   else
   {
	   chan->env_phase -= 4;
	   while (chan->env_phase < 0)
	   {
		   chan->env_phase += env_delay;
		   if (chan->regs[0] & 0x20)
			   chan->env_vol = (chan->env_vol + 1) & 15;
		   else if (chan->env_vol < 15)
			   chan->env_vol++;
	   }
   }

   /* vbl length counter */
   if (chan->vbl_length > 0 && 0 == (chan->regs[0] & 0x20))
      chan->vbl_length--;

   if (0 == chan->vbl_length)
      return 0;

   /* freqsweeps */
   if ((chan->regs[1] & 0x80) && (chan->regs[1] & 7))
   {
      sweep_delay = info->sync_times1[(chan->regs[1] >> 4) & 7];
      chan->sweep_phase -= 2;
      while (chan->sweep_phase <= 0)
      {
         chan->sweep_phase += sweep_delay;
         if (chan->regs[1] & 8)
            chan->freq -= (chan->freq >> (chan->regs[1] & 7)) + (first_channel << 16);
         else
            chan->freq += chan->freq >> (chan->regs[1] & 7);
      }
   }

   if ((0 == (chan->regs[1] & 8) && (chan->freq >> 16) > freq_limit[chan->regs[1] & 7])
       || (chan->freq >> 16) < 8)
      return 0;

   chan->phaseacc -= 4;

   while (chan->phaseacc < 0)
   {
      chan->phaseacc += (chan->freq >> 16);
      chan->adder = (chan->adder - 1) & 0x0F;
   }

   if (chan->regs[0] & 0x10) /* fixed volume */
      output = chan->regs[0] & 0x0F;
   else {
	   if (chan->env_vol > 0xf) output = 0;
	   else
		   output = 0x0f - (chan->env_vol & 0xf);
   }

   if (pulse_dty[(chan->regs[0] & 0xc0) >> 6][chan->adder >> 1] == 0)
	   output = 0;

   return (int8) output;
}

static const UINT8 tri_steps[0x20] = { // 15 - 0, 0 - 15
	15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

/* OUTPUT TRIANGLE WAVE SAMPLE (VALUES FROM -16 to +15) */
static int8 apu_triangle(struct nesapu_info *info, triangle_t *chan)
{
   INT32 freq;
   int8 output;
   /* reg0: 7=holdnote, 6-0=linear length counter
   ** reg2: low 8 bits of frequency
   ** reg3: 7-3=length counter, 2-0=high 3 bits of frequency
   */

   if (false == chan->enabled)
      return chan->output_vol;

   if (false == chan->counter_started && 0 == (chan->regs[0] & 0x80))
   {
      if (chan->write_latency)
         chan->write_latency--;
      if (0 == chan->write_latency)
         chan->counter_started = TRUE;
   }

   if (chan->counter_started)
   {
      if (chan->linear_length > 0)
         chan->linear_length--;
      if (chan->vbl_length && 0 == (chan->regs[0] & 0x80))
            chan->vbl_length--;

      if (0 == chan->vbl_length)
         return chan->output_vol;
   }

   if (0 == chan->linear_length)
      return chan->output_vol;

   freq = (((chan->regs[3] & 7) << 8) + chan->regs[2]);// + 1;

   if ((freq > 1 && freq <= 0x7fd) == 0) /* inaudible */
      return chan->output_vol;

   chan->phaseacc -= 4;
   while (chan->phaseacc < 0)
   {
	   chan->phaseacc += freq;
	   chan->adder = (chan->adder + 1) & 0x1F;

	   output = tri_steps[chan->adder];

	   chan->output_vol = output;
   }

   return (int8) chan->output_vol;
}

/* OUTPUT NOISE WAVE SAMPLE (VALUES FROM -16 to +15) */
static int8 apu_noise(struct nesapu_info *info, noise_t *chan)
{
   UINT8 outvol;
   UINT8 output;

   /* reg0: 0-3=volume, 4=envelope, 5=hold
   ** reg2: 7=small(93 byte) sample,3-0=freq lookup
   ** reg3: 7-4=vbl length counter
   */

   if (false == chan->enabled)
      return 0;

   if (chan->env_phase & 0x80000) {
	   // env start-up
	   // reset phase for env -dink (fixes clicks in Eggerland FDS title music)
	   chan->env_phase = info->sync_times1[chan->regs[0] & 0x0F];
	   chan->env_vol = 0x00;
   } else {
	   /* decay is at a rate of (env_regs + 1) / 240 secs */
	   chan->env_phase -= 4;
	   if (chan->env_phase < 0)
	   {
		   chan->env_phase += info->sync_times1[chan->regs[0] & 0x0F];
		   if (chan->regs[0] & 0x20)
			   chan->env_vol = (chan->env_vol + 1) & 15;
		   else if (chan->env_vol < 15)
			   chan->env_vol++;
	   }
   }

   /* length counter */
   if (0 == (chan->regs[0] & 0x20))
   {
      if (chan->vbl_length > 0)
         chan->vbl_length--;
   }

   if (chan->vbl_length <= 0)
      return 0;

   chan->phaseacc -= 4;
   if (chan->phaseacc < 0)
   {
      chan->phaseacc += noise_clocks[chan->regs[2] & 0x0F];

	  UINT16 feedback = (chan->lfsr & 0x01) ^ ((chan->lfsr >> ((chan->regs[2] & 0x80) ? 6 : 1)) & 0x01);
	  chan->lfsr = ((chan->lfsr >> 1) | (feedback << 14)) & 0x7fff;
   }

   if (chan->regs[0] & 0x10) /* fixed volume */
	   outvol = chan->regs[0] & 0x0F;
   else
	   outvol = 0x0F - chan->env_vol;

   output = (chan->lfsr & 1) ? 0 : outvol;

   return output;
}

// parts from dink's unfinished nes-apu
static inline void apu_dpcmreset(dpcm_t *chan)
{
	chan->address = 0xC000 + (UINT16)(chan->regs[2] << 6);
	chan->length = (UINT16)(chan->regs[3] << 4) + 1;
}

static INT32 apu_dpcm_loadbyte(dpcm_t *chan)
{
	if (chan->dmc_buffer_filled) return 0;
	M6502Stall(4);

	chan->dmc_buffer = M6502ReadByte(chan->address);
	chan->dmc_buffer_filled = 1;

	chan->address++;
	chan->length--;

	if (chan->length == 0)
	{
		if (chan->regs[0] & 0x40) {
			apu_dpcmreset(chan);
		} else {
			if (chan->regs[0] & 0x80) {
				chan->irq_occurred = TRUE;
				M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			}
		}
	}

	return 0;
}

static int8 apu_dpcm(struct nesapu_info *info, dpcm_t *chan)
{
   chan->phaseacc--; // 1-cycle per

   if (chan->phaseacc < 0) {
	   chan->phaseacc += chan->freq;

	   if (chan->enabled) {

		   if (info->dmc_direction_normal) {
			   if (chan->cur_byte & 1) {
				   if (chan->vol < (0x7f - 2))
					   chan->vol += 2;
			   } else {
				   if (chan->vol > 0)
					   chan->vol -= 2;
			   }
			   chan->cur_byte >>= 1;
		   } else {
			   // reversed!
			   if (chan->cur_byte & 0x80) {
				   if (chan->vol < (0x7f - 2))
					   chan->vol += 2;
			   } else {
				   if (chan->vol > 0)
					   chan->vol -= 2;
			   }
			   chan->cur_byte <<= 1;
		   }
		   if (chan->vol >= 0x7f)
			   chan->vol = 0x7f;
		   else
			   if (chan->vol < 0) chan->vol = 0;
	   }

	   chan->bits_left--;

	   if (chan->bits_left == 0) {
		   chan->bits_left = 8;

		   chan->enabled = chan->dmc_buffer_filled;
		   if (chan->enabled) {
			   chan->cur_byte = chan->dmc_buffer;
			   chan->dmc_buffer_filled = 0;
		   }
		   if (chan->length > 0) {
			   apu_dpcm_loadbyte(chan);
		   }
	   }
   }

   return (chan->vol);
}

/* WRITE REGISTER VALUE */
static inline void apu_regwrite(struct nesapu_info *info,INT32 address, UINT8 value)
{
   INT32 chan = (address & 4) ? 1 : 0;
   if (address & 0x80) {
	   chan |= 2; // 2x mmc5 square channels -dink
   }

   switch (address)
   {
   /* squares */
   case APU_WRA0:         // nesapu
   case APU_WRB0:
   case APU_WRA0|0x80:    // mmc5
   case APU_WRB0|0x80:
      info->APU.squ[chan].regs[0] = value;
	  //if (chan==1) bprintf(0, _T("sq1.r0: %X\n"), value);
	  break;

   case APU_WRA1:         // nesapu
   case APU_WRB1:
   case APU_WRA1|0x80:    // mmc5
   case APU_WRB1|0x80:
	  info->APU.squ[chan].regs[1] = value;
	  //if (chan==1) bprintf(0, _T("sq1.r1: %X\n"), value);
      break;

   case APU_WRA2:         // nesapu
   case APU_WRB2:
   case APU_WRA2|0x80:    // mmc5
   case APU_WRB2|0x80:
      info->APU.squ[chan].regs[2] = value;
      if (info->APU.squ[chan].enabled)
         info->APU.squ[chan].freq = (info->APU.squ[chan].freq & 0xff00ffff) + (value << 16);
	  //if (chan==1) bprintf(0, _T("sq1.r2: %X      enabled %x\n"), value, info->APU.squ[chan].enabled);
      break;

   case APU_WRA3:         // nesapu
   case APU_WRB3:
   case APU_WRA3|0x80:    // mmc5
   case APU_WRB3|0x80:
      info->APU.squ[chan].regs[3] = value;
	  //if (chan==1) bprintf(0, _T("sq1.r3: %X      enabled %x\n"), value, info->APU.squ[chan].enabled);

      if (info->APU.squ[chan].enabled)
      {
		  info->APU.squ[chan].vbl_length = info->vbl_times[value >> 3];
		  info->APU.squ[chan].env_vol = 0xff; // env-restart
		  info->APU.squ[chan].freq = (info->APU.squ[chan].freq & 0x00ffffff) + ((value & 0x7) << (16+8));
		  info->APU.squ[chan].adder = 0; // restart pulse phase
	  }

      break;

   /* triangle */
   case APU_WRC0:
      info->APU.tri.regs[0] = value;

      if (info->APU.tri.enabled)
      {                                          /* ??? */
         if (false == info->APU.tri.counter_started)
            info->APU.tri.linear_length = info->sync_times2[value & 0x7F];
      }

      break;

   case 0x4009:
      /* unused */
      info->APU.tri.regs[1] = value;
      break;

   case APU_WRC2:
      info->APU.tri.regs[2] = value;
      break;

   case APU_WRC3:
      info->APU.tri.regs[3] = value;

      /* this is somewhat of a hack.  there is some latency on the Real
      ** Thing between when trireg0 is written to and when the linear
      ** length counter actually begins its countdown.  we want to prevent
      ** the case where the program writes to the freq regs first, then
      ** to reg 0, and the counter accidentally starts running because of
      ** the sound queue's timestamp processing.
      **
      ** set to a few NES sample -- should be sufficient
      **
      **     3 * (1789772.727 / 44100) = ~122 cycles, just around one scanline
      **
      ** should be plenty of time for the 6502 code to do a couple of table
      ** dereferences and load up the other triregs
      */

	/* used to be 3, but now we run the clock faster, so base it on samples/sync */
      info->APU.tri.write_latency = (info->samps_per_sync + 239) / 240;

      if (info->APU.tri.enabled)
      {
         info->APU.tri.counter_started = false;
         info->APU.tri.vbl_length = info->vbl_times[value >> 3];
         info->APU.tri.linear_length = info->sync_times2[info->APU.tri.regs[0] & 0x7F];
      }

      break;

   /* noise */
   case APU_WRD0:
      info->APU.noi.regs[0] = value;
      break;

   case 0x400D:
      /* unused */
      info->APU.noi.regs[1] = value;
      break;

   case APU_WRD2:
      info->APU.noi.regs[2] = value;
      break;

   case APU_WRD3:
      info->APU.noi.regs[3] = value;

      if (info->APU.noi.enabled)
      {
		  info->APU.noi.vbl_length = info->vbl_times[value >> 3];
		  info->APU.noi.env_phase = 0x80000; // force restart
	  }
      break;

   /* DMC */
   case APU_WRE0:
      info->APU.dpcm.regs[0] = value;
	  info->APU.dpcm.freq = dpcm_clocks[value & 0x0F];
	  if (0 == (value & 0x80)) {
		  M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		  info->APU.dpcm.irq_occurred = false;
	  }
      break;

   case APU_WRE1: /* 7-bit DAC */
      info->APU.dpcm.regs[1] = value & 0x7F;
	  info->APU.dpcm.vol = info->APU.dpcm.regs[1];
      break;

   case APU_WRE2:
	   info->APU.dpcm.regs[2] = value;
      break;

   case APU_WRE3:
	   info->APU.dpcm.regs[3] = value;
      break;

   case APU_IRQCTRL: // 0x4017
	   mode4017 = value;
	   step4017 = 1;
	   clocky = 14915;

	   if (_4017hack && mode4017 & 0x80) { // clock the frame counter (used by Sam's Journey)
		   // this core doesn't have a proper frame counter, it's simulated, but this game expects
		   // to be able to do this to get the correct pitch.
		   clock_square_sweep(info);
	   }

	   M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
	   frame_irq_flag = 0;

	   break;

   case APU_SMASK|0x80: // 0x4015 FOR MMC5 -dink
      if (value & 0x01)
         info->APU.squ[2].enabled = TRUE;
      else
      {
         info->APU.squ[2].enabled = false;
         info->APU.squ[2].vbl_length = 0;
      }

	  if (value & 0x02)
		  info->APU.squ[3].enabled = TRUE;
	  else
      {
         info->APU.squ[3].enabled = false;
         info->APU.squ[3].vbl_length = 0;
	  }
	  break;

   case APU_SMASK: // 0x4015
      if (value & 0x01)
         info->APU.squ[0].enabled = TRUE;
      else
      {
         info->APU.squ[0].enabled = false;
         info->APU.squ[0].vbl_length = 0;
      }

	  if (value & 0x02)
		  info->APU.squ[1].enabled = TRUE;
	  else
      {
         info->APU.squ[1].enabled = false;
         info->APU.squ[1].vbl_length = 0;
	  }

      if (value & 0x04)
         info->APU.tri.enabled = TRUE;
      else
      {
         info->APU.tri.enabled = false;
         info->APU.tri.vbl_length = 0;
         info->APU.tri.linear_length = 0;
         info->APU.tri.counter_started = false;
         info->APU.tri.write_latency = 0;
      }

      if (value & 0x08)
         info->APU.noi.enabled = TRUE;
      else
      {
         info->APU.noi.enabled = false;
         info->APU.noi.vbl_length = 0;
      }

	  info->APU.dpcm.irq_occurred = false;
	  M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);

      if (value & 0x10)
      {
		  /* only reset dpcm values if DMA is finished */
		  if (0 == info->APU.dpcm.length)
		  {
			  //bprintf(0, _T("dmc START. (scanline %d)\n"), nes_scanline());
			  apu_dpcmreset(&info->APU.dpcm);
			  if (info->APU.dpcm.dmc_buffer_filled == 0) {
				  apu_dpcm_loadbyte(&info->APU.dpcm);
			  }
		  }
	  } else {
		  //bprintf(0, _T("dmc STOP. (scanline %d)\n"), nes_scanline());
		  info->APU.dpcm.length = 0;
	  }

	  break;
   default:
#ifdef MAME_DEBUG
logerror("invalid apu write: $%02X at $%04X\n", value, address);
#endif
      break;
   }
}

/* UPDATE SOUND BUFFER USING CURRENT DATA */
static void apu_update(struct nesapu_info *info)
{
	INT32 square1, square2, square3, square4;

//------------------------------------------------------------------------------------------------------
	if (info->pSyncCallback == NULL || pBurnSoundOut == NULL) return;
	INT32 position;

	if (info->fill_buffer_hack) {
		position = info->samples_per_frame;
		info->fill_buffer_hack = 0;
	} else {
		position = info->pSyncCallback(info->samples_per_frame);
	}

	if (position > info->samples_per_frame) position = info->samples_per_frame;
	if (position == info->current_position) return;

	INT16 *buffer16 = info->stream + 5 + info->current_position;

	INT32 samples = position - info->current_position;
	INT32 startpos = (info->current_position > 1) ? (info->current_position - 2) : 0;
	info->current_position = position;

	if (samples <= 0) return;
//------------------------------------------------------------------------------------------------------

	for (INT32 i = 0; i < samples; i++)
	{
		square1 = apu_square(info, &info->APU.squ[0], 1);
		square2 = apu_square(info, &info->APU.squ[1], 0);
		square3 = apu_square(info, &info->APU.squ[2], 0); // mmc5
		square4 = apu_square(info, &info->APU.squ[3], 0); // mmc5
		INT32 triangle = apu_triangle(info, &info->APU.tri);
		INT32 noise = apu_noise(info, &info->APU.noi);

		//DINKNOTES:
		// Super Mario Bros.
		//  clicks in square channels is _normal_ (aka: not a bug)
		//  slight/faint buzz when level starts is _normal_ (aka: not a bug)

		// mix new dmc engine (29781 samples/frame) with the rest  MIXER
		INT32 dmcoffs = (cycles_per_frame * (startpos + i)) / info->samps_per_sync;
		INT32 dmc = dmc_buffer[dmcoffs];
		INT32 ext = 0;
		if (nes_ext_sound_cb) {
			ext  = nes_ext_sound_cb();
			ext += nes_ext_sound_cb();
			ext += nes_ext_sound_cb();
			ext += nes_ext_sound_cb();
			ext /= 4;
		}


		if (arcade_mode) dmc = apu_dpcm(info, &info->APU.dpcm);

		INT32 out = (INT32)(((float)info->tnd_table[3*triangle + 2*noise + dmc] +
							  info->square_table[square1 + square2] + info->square_table[square3 + square4]) * 0x3fff);

		if (~nesapu_mixermode & MIXER_APU) out = 0;
		if (~nesapu_mixermode & MIXER_EXT) ext = 0;

		*(buffer16++) = BURN_SND_CLIP(out + ext);
	}
}

/* UPDATE APU SYSTEM */
void nesapuUpdate(INT32 chip, INT16 *buffer, INT32 samples)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_NESAPUSndInitted) bprintf(PRINT_ERROR, _T("nesapuUpdate called without init\n"));
#endif

	struct nesapu_info *info = &nesapu_chips[chip];

	if (pBurnSoundOut == NULL) {
		info->current_position = 0;
		return;
	}

	info->fill_buffer_hack = 1;
	apu_update(info);

	INT16 *source = info->stream + 5;
	info->resamp.resample(source, buffer, samples, info->gain[BURN_SND_NESAPU_ROUTE_1], BURN_SND_ROUTE_BOTH);

	info->current_position = 0;

}

/* READ VALUES FROM REGISTERS */
UINT8 nesapuRead(INT32 chip, INT32 address, UINT8 open_bus)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_NESAPUSndInitted) bprintf(PRINT_ERROR, _T("nesapuRead called without init\n"));
#endif

	struct nesapu_info *info = &nesapu_chips[chip];

	apu_update(info);

	if (address == (0x15|0x80)) // mmc5 pulse channels status -dink
	{
		INT32 readval = 0x20;
		if (info->APU.squ[2].vbl_length > 0)
			readval |= 0x01;

		if (info->APU.squ[3].vbl_length > 0)
			readval |= 0x02;
	}

	if (address == 0x15)
	{
		INT32 readval = open_bus & 0x20; // 0x20 open_bus -dink
		if (info->APU.squ[0].vbl_length > 0)
			readval |= 0x01;

		if (info->APU.squ[1].vbl_length > 0)
			readval |= 0x02;

		if (info->APU.tri.vbl_length > 0)
			readval |= 0x04;

		if (info->APU.noi.vbl_length > 0)
			readval |= 0x08;

		if (info->APU.dpcm.length > 0) {
			//bprintf(0, _T("0x4015 status - dpcm in DMA (len = %d  scanline %d)\n"), info->APU.dpcm.length, nes_scanline());
			readval |= 0x10;
		} else {
			//bprintf(0, _T("0x4015 status: dpcm NOT in DMA. (len = %d  scanline %d)\n"), info->APU.dpcm.length, nes_scanline());
		}

		if (frame_irq_flag) {
			readval |= 0x40;
			frame_irq_flag = 0;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		}

		if (info->APU.dpcm.irq_occurred == TRUE)
			readval |= 0x80;

		return readval;
	} else {
		return info->APU.regs[address&0x1f];
	}
}

/* WRITE VALUE TO TEMP REGISTRY AND QUEUE EVENT */
void nesapuWrite(INT32 chip, INT32 address, UINT8 value)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_NESAPUSndInitted) bprintf(PRINT_ERROR, _T("nesapuWrite called without init\n"));
#endif

	struct nesapu_info *info = &nesapu_chips[chip]; //sndti_token(SOUND_NES, chip);

	if (address == 0xff) {
		apu_update(info);
		return;
	}

	if ((address & 0x1f) > 0x17) return;

	if (~address&0x80) // only nesapu (not mmc5)
		info->APU.regs[address]=value;
	apu_update(info);
	apu_regwrite(info,address,value);
}

/* EXTERNAL INTERFACE FUNCTIONS */

void nesapuSetMode4017(UINT8 val)
{
	mode4017 = val;
}

void nesapu4017hack(INT32 enable)
{
	_4017hack = enable;
}

void nesapuReset()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_NESAPUSndInitted) bprintf(PRINT_ERROR, _T("nesapuReset called without init\n"));
#endif

	for (INT32 i = 0; i < CHIP_NUM; i++) {
		struct nesapu_info *info = &nesapu_chips[i];

		info->current_position = 0;
		info->fill_buffer_hack = 0;

		memset(&info->APU.squ, 0, sizeof(info->APU.squ));
		memset(&info->APU.tri, 0, sizeof(info->APU.tri));
		memset(&info->APU.noi, 0, sizeof(info->APU.noi));
		memset(&info->APU.dpcm, 0, sizeof(info->APU.dpcm));
		memset(&info->APU.regs, 0, sizeof(info->APU.regs));

		info->APU.dpcm.bits_left = 8;
		info->APU.dpcm.freq = dpcm_clocks[0];
		info->APU.noi.lfsr = 1;
	    clocky = 0;
		mode4017 = 0xc0; // disabled @ startup
		step4017 = 0;
		frame_irq_flag = 0;
	}
}

void nesapuInit(INT32 chip, INT32 clock, INT32 bAdd)
{
	nesapuInit(chip, clock, 0, nes_nesapu_sync, bAdd);
}

void nesapuInitPal(INT32 chip, INT32 clock, INT32 bAdd)
{
	nesapuInit(chip, clock, 1, nes_nesapu_sync, bAdd);
}

/* INITIALIZE APU SYSTEM */
void nesapuInit(INT32 chip, INT32 clock, INT32 is_pal, UINT32 (*pSyncCallback)(INT32 samples_per_frame), INT32 bAdd)
{
	DebugSnd_NESAPUSndInitted = 1;

	struct nesapu_info *info = &nesapu_chips[chip];

	memset(info, 0, sizeof(nesapu_info));

	/* Initialize global variables */
	cycles_per_frame = (is_pal) ? 33248 : 29781;

	noise_clocks = &noise_freq[(is_pal) ? 1 : 0][0];
	dpcm_clocks = &dpcm_freq[(is_pal) ? 1 : 0][0];

	info->samps_per_sync = cycles_per_frame / 4; // 7445 ntsc, 8312 pal
	info->buffer_size = info->samps_per_sync;
	info->real_rate = (info->samps_per_sync * nBurnFPS) / 100;
	//info->apu_incsize = 4; //(float) (clock / (float) info->real_rate);
	//bprintf(0, _T("apu_incsize: %f\n"), info->apu_incsize);

	/* Use initializer calls */
	create_vbltimes(info->vbl_times,vbl_length,info->samps_per_sync);
	create_syncs(info, info->samps_per_sync);
	create_tndtab(info->tnd_table);
	create_squaretab(info->square_table);

	/* Adjust buffer size if 16 bits */
	info->buffer_size+=info->samps_per_sync;

	info->samples_per_frame = ((info->real_rate * 100) / nBurnFPS) + 1;

	if (nBurnSoundRate < 44100) info->samples_per_frame += 10; // deal w/ a bug in nBurnSoundRate's calculation for 22050 and lower.

	info->resamp.init(info->real_rate, nBurnSoundRate, bAdd);

	info->pSyncCallback = pSyncCallback;

	info->bAdd = bAdd;

	if (chip == 0) {
		// cycles per frame: 29781 ntsc, 33248 pal
		dmc_buffer = (UINT8*)BurnMalloc((cycles_per_frame + 5) * 2);
		nes_ext_sound_cb = NULL;
	}
	nesapu_mixermode = 0xff; // enable all

	_4017hack = 0;

	info->stream = NULL;
	info->stream = (INT16*)BurnMalloc(info->samples_per_frame * 2 * sizeof(INT16) + (0x10 * 2));
	info->gain[BURN_SND_NESAPU_ROUTE_1] = 1.00;
	info->gain[BURN_SND_NESAPU_ROUTE_2] = 1.00;
	info->output_dir[BURN_SND_NESAPU_ROUTE_1] = BURN_SND_ROUTE_BOTH;
	info->output_dir[BURN_SND_NESAPU_ROUTE_2] = BURN_SND_ROUTE_BOTH;
}

void nesapuSetRoute(INT32 nChip, INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_NESAPUSndInitted) bprintf(PRINT_ERROR, _T("nesapuSetRoute called without init\n"));
#endif

	struct nesapu_info *info = &nesapu_chips[nChip];
	
	info->gain[nIndex] = nVolume;
	info->output_dir[nIndex] = nRouteDir;
}

void nesapuExit()
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_NESAPUSndInitted) bprintf(PRINT_ERROR, _T("nesapuExit called without init\n"));
#endif

	if (!DebugSnd_NESAPUSndInitted) return;

	struct nesapu_info *info;
	for (INT32 i = 0; i < CHIP_NUM; i++)
	{
		info = &nesapu_chips[i];
		if (info->stream)
			BurnFree(info->stream);
		info->resamp.exit();
	}

	BurnFree(dmc_buffer);
	nes_ext_sound_cb = NULL;

	nesapuSetArcade(0);

	DebugSnd_NESAPUSndInitted = 0;
}

void nesapuScan(INT32 nAction, INT32 *)
{
#if defined FBNEO_DEBUG
	if (!DebugSnd_NESAPUSndInitted) bprintf(PRINT_ERROR, _T("nesapuScan called without init\n"));
#endif

	if (nAction & ACB_DRIVER_DATA)
	{
		for (INT32 i = 0; i < CHIP_NUM; i++)
		{
			struct nesapu_info *info = &nesapu_chips[i];

			SCAN_VAR(info->APU.squ);
			SCAN_VAR(info->APU.tri);
			SCAN_VAR(info->APU.noi);
			SCAN_VAR(info->APU.dpcm);
			SCAN_VAR(info->APU.regs);
		}

		// frame-irq stuff
		SCAN_VAR(frame_irq_flag);
		SCAN_VAR(mode4017);
		SCAN_VAR(step4017);
		SCAN_VAR(clocky);
	}
}
