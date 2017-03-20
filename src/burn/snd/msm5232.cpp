// Based on MAME sources by Jarek Burczynski, Hiromitsu Shioya

#include "burnint.h"
#include "msm5232.h"

/*
    OKI MSM5232RS
    8 channel tone generator
*/

#define CLOCK_RATE_DIVIDER 16

struct VOICE {
	UINT8 mode;

	INT32     TG_count_period;
	INT32     TG_count;

	UINT8   TG_cnt;     /* 7 bits binary counter (frequency output) */
	UINT8   TG_out16;   /* bit number (of TG_cnt) for 16' output */
	UINT8   TG_out8;    /* bit number (of TG_cnt) for  8' output */
	UINT8   TG_out4;    /* bit number (of TG_cnt) for  4' output */
	UINT8   TG_out2;    /* bit number (of TG_cnt) for  2' output */

	INT32     egvol;
	INT32     eg_sect;
	INT32     counter;
	INT32     eg;

	UINT8   eg_arm;     /* attack/release mode */

	double  ar_rate;
	double  dr_rate;
	double  rr_rate;

	INT32     pitch;          /* current pitch data */

	INT32 GF;
};

static VOICE   m_voi[8];
static UINT32 m_EN_out16[2]; /* enable 16' output masks for both groups (0-disabled ; ~0 -enabled) */
static UINT32 m_EN_out8[2];  /* enable 8'  output masks */
static UINT32 m_EN_out4[2];  /* enable 4'  output masks */
static UINT32 m_EN_out2[2];  /* enable 2'  output masks */
static INT32 m_noise_cnt;
static INT32 m_noise_step;
static INT32 m_noise_rng;
static INT32 m_noise_clocks;   /* number of the noise_rng (output) level changes */
static UINT32 m_UpdateStep;

static double volume[11];

/* rate tables */
static double  m_ar_tbl[8];
static double  m_dr_tbl[16];

static UINT8   m_control1;
static UINT8   m_control2;

static INT32     m_gate;       /* current state of the GATE output */

static INT32     m_add;
static INT32     m_chip_clock;      /* chip clock in Hz */
static INT32     m_rate;       /* sample rate in Hz */

static double  m_external_capacity[8]; /* in Farads, eg 0.39e-6 = 0.36 uF (microFarads) */
static void (*m_gate_handler_cb)(INT32 state) = NULL;/* callback called when the GATE output pin changes state */

static INT32 *sound_buffer[11];

//-------------------------------------------------
//  set gate handler
//-------------------------------------------------

static void gate_update()
{
	INT32 new_state = (m_control2 & 0x20) ? m_voi[7].GF : 0;

	if (m_gate != new_state && m_gate_handler_cb)
	{
		m_gate = new_state;
		m_gate_handler_cb(new_state);
	}
}

void MSM5232SetGateCallback(void (*callback)(INT32))
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM5232Initted) bprintf(PRINT_ERROR, _T("MSM5232SetGateCallback called without init\n"));
#endif

	m_gate_handler_cb = callback;
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void MSM5232Reset()
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM5232Initted) bprintf(PRINT_ERROR, _T("MSM5232Reset called without init\n"));
#endif

	INT32 i;

	for (i=0; i<8; i++)
	{
		MSM5232Write(i,0x80);
		MSM5232Write(i,0x00);
	}
	m_noise_cnt     = 0;
	m_noise_rng     = 1;
	m_noise_clocks  = 0;

	m_control1      = 0;
	m_EN_out16[0]   = 0;
	m_EN_out8[0]    = 0;
	m_EN_out4[0]    = 0;
	m_EN_out2[0]    = 0;

	m_control2      = 0;
	m_EN_out16[1]   = 0;
	m_EN_out8[1]    = 0;
	m_EN_out4[1]    = 0;
	m_EN_out2[1]    = 0;

	gate_update();
}

//-------------------------------------------------

void MSM5232SetCapacitors(double cap1, double cap2, double cap3, double cap4, double cap5, double cap6, double cap7, double cap8)
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM5232Initted) bprintf(PRINT_ERROR, _T("MSM5232SetCapacitors called without init\n"));
#endif

	m_external_capacity[0] = cap1;
	m_external_capacity[1] = cap2;
	m_external_capacity[2] = cap3;
	m_external_capacity[3] = cap4;
	m_external_capacity[4] = cap5;
	m_external_capacity[5] = cap6;
	m_external_capacity[6] = cap7;
	m_external_capacity[7] = cap8;
}


/* Default chip clock is 2119040 Hz */
/* At this clock chip generates exactly 440.0 Hz signal on 8' output when pitch data=0x21 */

/* ROM table to convert from pitch data into data for programmable counter and binary counter */
/* Chip has 88x12bits ROM   (addressing (in hex) from 0x00 to 0x57) */
#define ROM(counter,bindiv) (counter|(bindiv<<9))

static const UINT16 MSM5232_ROM[88]={
/* higher values are Programmable Counter data (9 bits) */
/* lesser values are Binary Counter shift data (3 bits) */

/* 0 */ ROM (506, 7),

/* 1 */ ROM (478, 7),/* 2 */ ROM (451, 7),/* 3 */ ROM (426, 7),/* 4 */ ROM (402, 7),
/* 5 */ ROM (379, 7),/* 6 */ ROM (358, 7),/* 7 */ ROM (338, 7),/* 8 */ ROM (319, 7),
/* 9 */ ROM (301, 7),/* A */ ROM (284, 7),/* B */ ROM (268, 7),/* C */ ROM (253, 7),

/* D */ ROM (478, 6),/* E */ ROM (451, 6),/* F */ ROM (426, 6),/*10 */ ROM (402, 6),
/*11 */ ROM (379, 6),/*12 */ ROM (358, 6),/*13 */ ROM (338, 6),/*14 */ ROM (319, 6),
/*15 */ ROM (301, 6),/*16 */ ROM (284, 6),/*17 */ ROM (268, 6),/*18 */ ROM (253, 6),

/*19 */ ROM (478, 5),/*1A */ ROM (451, 5),/*1B */ ROM (426, 5),/*1C */ ROM (402, 5),
/*1D */ ROM (379, 5),/*1E */ ROM (358, 5),/*1F */ ROM (338, 5),/*20 */ ROM (319, 5),
/*21 */ ROM (301, 5),/*22 */ ROM (284, 5),/*23 */ ROM (268, 5),/*24 */ ROM (253, 5),

/*25 */ ROM (478, 4),/*26 */ ROM (451, 4),/*27 */ ROM (426, 4),/*28 */ ROM (402, 4),
/*29 */ ROM (379, 4),/*2A */ ROM (358, 4),/*2B */ ROM (338, 4),/*2C */ ROM (319, 4),
/*2D */ ROM (301, 4),/*2E */ ROM (284, 4),/*2F */ ROM (268, 4),/*30 */ ROM (253, 4),

/*31 */ ROM (478, 3),/*32 */ ROM (451, 3),/*33 */ ROM (426, 3),/*34 */ ROM (402, 3),
/*35 */ ROM (379, 3),/*36 */ ROM (358, 3),/*37 */ ROM (338, 3),/*38 */ ROM (319, 3),
/*39 */ ROM (301, 3),/*3A */ ROM (284, 3),/*3B */ ROM (268, 3),/*3C */ ROM (253, 3),

/*3D */ ROM (478, 2),/*3E */ ROM (451, 2),/*3F */ ROM (426, 2),/*40 */ ROM (402, 2),
/*41 */ ROM (379, 2),/*42 */ ROM (358, 2),/*43 */ ROM (338, 2),/*44 */ ROM (319, 2),
/*45 */ ROM (301, 2),/*46 */ ROM (284, 2),/*47 */ ROM (268, 2),/*48 */ ROM (253, 2),

/*49 */ ROM (478, 1),/*4A */ ROM (451, 1),/*4B */ ROM (426, 1),/*4C */ ROM (402, 1),
/*4D */ ROM (379, 1),/*4E */ ROM (358, 1),/*4F */ ROM (338, 1),/*50 */ ROM (319, 1),
/*51 */ ROM (301, 1),/*52 */ ROM (284, 1),/*53 */ ROM (268, 1),/*54 */ ROM (253, 1),

/*55 */ ROM (253, 1),/*56 */ ROM (253, 1),

/*57 */ ROM (13, 7)
};
#undef ROM


#define STEP_SH (16)    /* step calculations accuracy */

/*
 * resistance values are guesswork, default capacity is mentioned in the datasheets
 *
 * charges external capacitor (default is 0.39uF) via R51
 * in approx. 5*1400 * 0.39e-6
 *
 * external capacitor is discharged through R52
 * in approx. 5*28750 * 0.39e-6
 */


#define R51 1400    /* charge resistance */
#define R52 28750   /* discharge resistance */

static void init_tables()
{
	INT32 i;
	double scale;

	/* sample rate = chip clock !!!  But : */
	/* highest possible frequency is chipclock/13/16 (pitch data=0x57) */
	/* at 2MHz : 2000000/13/16 = 9615 Hz */

	i = (INT32)(((double)(1<<STEP_SH) * (double)m_rate) / (double)m_chip_clock);
	m_UpdateStep = i;

	scale = ((double)m_chip_clock) / (double)m_rate;
	m_noise_step = (INT32)(((1<<STEP_SH)/128.0) * scale); /* step of the rng reg in 16.16 format */

	for (i=0; i<8; i++)
	{
		double clockscale = (double)m_chip_clock / 2119040.0;
		m_ar_tbl[i]   = ((1<<i) / clockscale) * (double)R51;
	}

	for (i=0; i<8; i++)
	{
		double clockscale = (double)m_chip_clock / 2119040.0;
		m_dr_tbl[i]   = (     (1<<i) / clockscale) * (double)R52;
		m_dr_tbl[i+8] = (6.25*(1<<i) / clockscale) * (double)R52;
	}
}

static void init_voice(INT32 i)
{
	m_voi[i].ar_rate= m_ar_tbl[0] * m_external_capacity[i];
	m_voi[i].dr_rate= m_dr_tbl[0] * m_external_capacity[i];
	m_voi[i].rr_rate= m_dr_tbl[0] * m_external_capacity[i]; /* this is constant value */
	m_voi[i].eg_sect= -1;
	m_voi[i].eg     = 0;
	m_voi[i].eg_arm = 0;
	m_voi[i].pitch  = -1;
}

void MSM5232Init(INT32 clock, INT32 bAdd)
{
	DebugSnd_MSM5232Initted = 1;
	
	INT32 j, rate;

	m_add = bAdd;

	rate = ((clock / CLOCK_RATE_DIVIDER) * 100) / nBurnFPS;

	m_chip_clock = (clock * 100) / nBurnFPS;
	m_rate  = rate;  /* avoid division by 0 */

	if (!rate) return; // freak out!

	init_tables();

	for (j=0; j<8; j++)
	{
		memset(&m_voi[j],0,sizeof(VOICE));
		init_voice(j);
	}

	for (j = 0; j < 11; j++) {
		sound_buffer[j] = (INT32*)BurnMalloc(m_rate * sizeof(INT32));
	}

	volume[BURN_SND_MSM5232_ROUTE_0] = 1.00;
	volume[BURN_SND_MSM5232_ROUTE_1] = 1.00;
	volume[BURN_SND_MSM5232_ROUTE_2] = 1.00;
	volume[BURN_SND_MSM5232_ROUTE_3] = 1.00;
	volume[BURN_SND_MSM5232_ROUTE_4] = 1.00;
	volume[BURN_SND_MSM5232_ROUTE_5] = 1.00;
	volume[BURN_SND_MSM5232_ROUTE_6] = 1.00;
	volume[BURN_SND_MSM5232_ROUTE_7] = 1.00;
	volume[BURN_SND_MSM5232_ROUTE_SOLO8] = 0;
	volume[BURN_SND_MSM5232_ROUTE_SOLO16] = 0;
	volume[BURN_SND_MSM5232_ROUTE_NOISE] = 0;
}

void MSM5232Exit()
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM5232Initted) bprintf(PRINT_ERROR, _T("MSM5232Exit called without init\n"));
#endif

	if (!DebugSnd_MSM5232Initted) return;

	for (INT32 j = 0; j < 11; j++) {
		BurnFree(sound_buffer[j]);
		sound_buffer[j] = NULL;
	}

	m_gate_handler_cb = NULL;
	
	DebugSnd_MSM5232Initted = 0;
}

void MSM5232SetRoute(double vol, INT32 route)
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM5232Initted) bprintf(PRINT_ERROR, _T("MSM5232SetRoute called without init\n"));
#endif

	volume[route] = vol;
}

void MSM5232Write(INT32 offset, UINT8 data)
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM5232Initted) bprintf(PRINT_ERROR, _T("MSM5232Write called without init\n"));
#endif

	offset &= 0x0f;

	if (offset > 0x0d)
		return;

	if (offset < 0x08) /* pitch */
	{
		int ch = offset&7;

		m_voi[ch].GF = ((data&0x80)>>7);
		if (ch == 7)
			gate_update();

		if(data&0x80)
		{
			if(data >= 0xd8)
			{
				/*if ((data&0x7f) != 0x5f) logerror("MSM5232: WRONG PITCH CODE = %2x\n",data&0x7f);*/
				m_voi[ch].mode = 1;     /* noise mode */
				m_voi[ch].eg_sect = 0;  /* Key On */
			}
			else
			{
				if ( m_voi[ch].pitch != (data&0x7f) )
				{
					int n;
					UINT16 pg;

					m_voi[ch].pitch = data&0x7f;

					pg = MSM5232_ROM[ data&0x7f ];

					m_voi[ch].TG_count_period = (pg & 0x1ff) * m_UpdateStep / 2;

					n = (pg>>9) & 7;    /* n = bit number for 16' output */
					m_voi[ch].TG_out16 = 1<<n;
										/* for 8' it is bit n-1 (bit 0 if n-1<0) */
										/* for 4' it is bit n-2 (bit 0 if n-2<0) */
										/* for 2' it is bit n-3 (bit 0 if n-3<0) */
					n = (n>0)? n-1: 0;
					m_voi[ch].TG_out8  = 1<<n;

					n = (n>0)? n-1: 0;
					m_voi[ch].TG_out4  = 1<<n;

					n = (n>0)? n-1: 0;
					m_voi[ch].TG_out2  = 1<<n;
				}
				m_voi[ch].mode = 0;     /* tone mode */
				m_voi[ch].eg_sect = 0;  /* Key On */
			}
		}
		else
		{
			if ( !m_voi[ch].eg_arm )    /* arm = 0 */
				m_voi[ch].eg_sect = 2;  /* Key Off -> go to release */
			else                            /* arm = 1 */
				m_voi[ch].eg_sect = 1;  /* Key Off -> go to decay */
		}
	}
	else
	{
		INT32 i;
		switch(offset)
		{
		case 0x08:  /* group1 attack */
			for (i=0; i<4; i++)
				m_voi[i].ar_rate   = m_ar_tbl[data&0x7] * m_external_capacity[i];
			break;

		case 0x09:  /* group2 attack */
			for (i=0; i<4; i++)
				m_voi[i+4].ar_rate = m_ar_tbl[data&0x7] * m_external_capacity[i+4];
			break;

		case 0x0a:  /* group1 decay */
			for (i=0; i<4; i++)
				m_voi[i].dr_rate   = m_dr_tbl[data&0xf] * m_external_capacity[i];
			break;

		case 0x0b:  /* group2 decay */
			for (i=0; i<4; i++)
				m_voi[i+4].dr_rate = m_dr_tbl[data&0xf] * m_external_capacity[i+4];
			break;

		case 0x0c:  /* group1 control */
			m_control1 = data;

			for (i=0; i<4; i++)
				m_voi[i].eg_arm = data&0x10;

			m_EN_out16[0] = (data&1) ? ~0:0;
			m_EN_out8[0]  = (data&2) ? ~0:0;
			m_EN_out4[0]  = (data&4) ? ~0:0;
			m_EN_out2[0]  = (data&8) ? ~0:0;

			break;

		case 0x0d:  /* group2 control */
			m_control2 = data;
			gate_update();

			for (i=0; i<4; i++)
				m_voi[i+4].eg_arm = data&0x10;

			m_EN_out16[1] = (data&1) ? ~0:0;
			m_EN_out8[1]  = (data&2) ? ~0:0;
			m_EN_out4[1]  = (data&4) ? ~0:0;
			m_EN_out2[1]  = (data&8) ? ~0:0;

			break;
		}
	}
}



#define VMIN    0
#define VMAX    32768

static void EG_voices_advance()
{
	VOICE *voi = &m_voi[0];
	INT32 samplerate = m_rate;
	INT32 i;

	i = 8;
	do
	{
		switch(voi->eg_sect)
		{
		case 0: /* attack */

			/* capacitor charge */
			if (voi->eg < VMAX)
			{
				voi->counter -= (INT32)((VMAX - voi->eg) / voi->ar_rate);
				if ( voi->counter <= 0 )
				{
					INT32 n = -voi->counter / samplerate + 1;
					voi->counter += n * samplerate;
					if ( (voi->eg += n) > VMAX )
						voi->eg = VMAX;
				}
			}

			/* when ARM=0, EG switches to decay as soon as cap is charged to VT (EG inversion voltage; about 80% of MAX) */
			if (!voi->eg_arm)
			{
				if(voi->eg >= VMAX * 80/100 )
				{
					voi->eg_sect = 1;
				}
			}
			else
			/* ARM=1 */
			{
				/* when ARM=1, EG stays at maximum until key off */
			}

			voi->egvol = voi->eg / 16; /*32768/16 = 2048 max*/

			break;

		case 1: /* decay */

			/* capacitor discharge */
			if (voi->eg > VMIN)
			{
				voi->counter -= (INT32)((voi->eg - VMIN) / voi->dr_rate);
				if ( voi->counter <= 0 )
				{
					INT32 n = -voi->counter / samplerate + 1;
					voi->counter += n * samplerate;
					if ( (voi->eg -= n) < VMIN )
						voi->eg = VMIN;
				}
			}
			else /* voi->eg <= VMIN */
			{
				voi->eg_sect =-1;
			}

			voi->egvol = voi->eg / 16; /*32768/16 = 2048 max*/

			break;

		case 2: /* release */

			/* capacitor discharge */
			if (voi->eg > VMIN)
			{
				voi->counter -= (INT32)((voi->eg - VMIN) / voi->rr_rate);
				if ( voi->counter <= 0 )
				{
					INT32 n = -voi->counter / samplerate + 1;
					voi->counter += n * samplerate;
					if ( (voi->eg -= n) < VMIN )
						voi->eg = VMIN;
				}
			}
			else /* voi->eg <= VMIN */
			{
				voi->eg_sect =-1;
			}

			voi->egvol = voi->eg / 16; /*32768/16 = 2048 max*/

			break;

		default:
			break;
		}

		voi++;
		i--;
	} while (i>0);

}

static INT32 o2,o4,o8,o16,solo8,solo16;

static void TG_group_advance(INT32 groupidx)
{
	VOICE *voi = &m_voi[groupidx*4];
	INT32 i;

	o2 = o4 = o8 = o16 = solo8 = solo16 = 0;

	i=4;
	do
	{
		INT32 out2, out4, out8, out16;

		out2 = out4 = out8 = out16 = 0;

		if (voi->mode==0)   /* generate square tone */
		{
			INT32 left = 1<<STEP_SH;
			do
			{
				INT32 nextevent = left;

				if (voi->TG_cnt&voi->TG_out16)  out16+=voi->TG_count;
				if (voi->TG_cnt&voi->TG_out8)   out8 +=voi->TG_count;
				if (voi->TG_cnt&voi->TG_out4)   out4 +=voi->TG_count;
				if (voi->TG_cnt&voi->TG_out2)   out2 +=voi->TG_count;

				voi->TG_count -= nextevent;

				while (voi->TG_count <= 0)
				{
					voi->TG_count += voi->TG_count_period;
					voi->TG_cnt++;
					if (voi->TG_cnt&voi->TG_out16)  out16+=voi->TG_count_period;
					if (voi->TG_cnt&voi->TG_out8 )  out8 +=voi->TG_count_period;
					if (voi->TG_cnt&voi->TG_out4 )  out4 +=voi->TG_count_period;
					if (voi->TG_cnt&voi->TG_out2 )  out2 +=voi->TG_count_period;

					if (voi->TG_count > 0)
						break;

					voi->TG_count += voi->TG_count_period;
					voi->TG_cnt++;
					if (voi->TG_cnt&voi->TG_out16)  out16+=voi->TG_count_period;
					if (voi->TG_cnt&voi->TG_out8 )  out8 +=voi->TG_count_period;
					if (voi->TG_cnt&voi->TG_out4 )  out4 +=voi->TG_count_period;
					if (voi->TG_cnt&voi->TG_out2 )  out2 +=voi->TG_count_period;
				}
				if (voi->TG_cnt&voi->TG_out16)  out16-=voi->TG_count;
				if (voi->TG_cnt&voi->TG_out8 )  out8 -=voi->TG_count;
				if (voi->TG_cnt&voi->TG_out4 )  out4 -=voi->TG_count;
				if (voi->TG_cnt&voi->TG_out2 )  out2 -=voi->TG_count;

				left -=nextevent;

			}while (left>0);
		}
		else    /* generate noise */
		{
			if (m_noise_clocks&8)   out16+=(1<<STEP_SH);
			if (m_noise_clocks&4)   out8 +=(1<<STEP_SH);
			if (m_noise_clocks&2)   out4 +=(1<<STEP_SH);
			if (m_noise_clocks&1)   out2 +=(1<<STEP_SH);
		}

		/* calculate signed output */
		o16 += ( (out16-(1<<(STEP_SH-1))) * voi->egvol) >> STEP_SH;
		o8  += ( (out8 -(1<<(STEP_SH-1))) * voi->egvol) >> STEP_SH;
		o4  += ( (out4 -(1<<(STEP_SH-1))) * voi->egvol) >> STEP_SH;
		o2  += ( (out2 -(1<<(STEP_SH-1))) * voi->egvol) >> STEP_SH;

		if (i == 1 && groupidx == 1)
		{
			solo16 += ( (out16-(1<<(STEP_SH-1))) << 11) >> STEP_SH;
			solo8  += ( (out8 -(1<<(STEP_SH-1))) << 11) >> STEP_SH;
		}

		voi++;
		i--;
	}while (i>0);

	/* cut off disabled output lines */
	o16 &= m_EN_out16[groupidx];
	o8  &= m_EN_out8 [groupidx];
	o4  &= m_EN_out4 [groupidx];
	o2  &= m_EN_out2 [groupidx];
}

void MSM5232SetClock(INT32 clock)
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM5232Initted) bprintf(PRINT_ERROR, _T("MSM5232SetClock called without init\n"));
#endif

	if (m_chip_clock != clock)
	{
		m_rate = ((clock/CLOCK_RATE_DIVIDER) * 100) / nBurnFPS;
		m_chip_clock = (clock * 100) / nBurnFPS;
		init_tables();
		for (INT32 j = 0; j < 11; j++) {
			if (sound_buffer[j]) {
				BurnFree(sound_buffer[j]);
			}
			sound_buffer[j] = (INT32*)BurnMalloc(m_rate * 2);
		}
	}
}


//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

void MSM5232Update(INT16 *buffer, INT32 samples)
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM5232Initted) bprintf(PRINT_ERROR, _T("MSM5232Update called without init\n"));
#endif

	INT32 *buf1 = sound_buffer[0];
	INT32 *buf2 = sound_buffer[1];
	INT32 *buf3 = sound_buffer[2];
	INT32 *buf4 = sound_buffer[3];
	INT32 *buf5 = sound_buffer[4];
	INT32 *buf6 = sound_buffer[5];
	INT32 *buf7 = sound_buffer[6];
	INT32 *buf8 = sound_buffer[7];
	INT32 *bufsolo1 = sound_buffer[8];
	INT32 *bufsolo2 = sound_buffer[9];
	INT32 *bufnoise = sound_buffer[10];
	INT32 i;

	for (i = 0; i < m_rate; i++)
	{
		/* calculate all voices' envelopes */
		EG_voices_advance();

		TG_group_advance(0);   /* calculate tones group 1 */
		buf1[i] = o2;
		buf2[i] = o4;
		buf3[i] = o8;
		buf4[i] = o16;

		TG_group_advance(1);   /* calculate tones group 2 */
		buf5[i] = o2;
		buf6[i] = o4;
		buf7[i] = o8;
		buf8[i] = o16;

		bufsolo1[i] = solo8;
		bufsolo2[i] = solo16;

		/* update noise generator */
		{
			INT32 cnt = (m_noise_cnt+=m_noise_step) >> STEP_SH;
			m_noise_cnt &= ((1<<STEP_SH)-1);
			while (cnt > 0)
			{
				INT32 tmp = m_noise_rng & (1<<16);        /* store current level */

				if (m_noise_rng&1)
					m_noise_rng ^= 0x24000;
				m_noise_rng>>=1;

				if ( (m_noise_rng & (1<<16)) != tmp )   /* level change detect */
					m_noise_clocks++;

				cnt--;
			}
		}

		bufnoise[i] = (m_noise_rng & (1<<16)) ? 32767 : 0;
	}

	if (!m_add)
	{
		for (i = 0; i < samples; i++) {
			INT32 offs = (i * m_rate) / samples;
			if (offs >= m_rate) offs = m_rate - 1;
	
			INT32 sample = (INT32)(double)(BURN_SND_CLIP(sound_buffer[0][offs]) * volume[0]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[1][offs]) * volume[1]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[2][offs]) * volume[2]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[3][offs]) * volume[3]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[4][offs]) * volume[4]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[5][offs]) * volume[5]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[6][offs]) * volume[6]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[7][offs]) * volume[7]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[8][offs]) * volume[8]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[9][offs]) * volume[9]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[10][offs]) * volume[10]);
	
			sample = BURN_SND_CLIP(sample);
	
			buffer[0] = sample;
			buffer[1] = sample;
			buffer += 2;
		}
	} else {
		for (i = 0; i < samples; i++) {
			INT32 offs = (i * m_rate) / samples;
			if (offs >= m_rate) offs = m_rate - 1;
	
			INT32 sample = (INT32)(double)(BURN_SND_CLIP(sound_buffer[0][offs]) * volume[0]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[1][offs]) * volume[1]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[2][offs]) * volume[2]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[3][offs]) * volume[3]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[4][offs]) * volume[4]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[5][offs]) * volume[5]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[6][offs]) * volume[6]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[7][offs]) * volume[7]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[8][offs]) * volume[8]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[9][offs]) * volume[9]);
			sample += (INT32)(double)(BURN_SND_CLIP(sound_buffer[10][offs]) * volume[10]);
	
			sample = BURN_SND_CLIP(sample);
	
			buffer[0] += BURN_SND_CLIP(buffer[0]+sample);
			buffer[1] += BURN_SND_CLIP(buffer[1]+sample);
			buffer += 2;
		}
	}
}

INT32 MSM5232Scan(INT32 nAction, INT32 *)
{
#if defined FBA_DEBUG
	if (!DebugSnd_MSM5232Initted) bprintf(PRINT_ERROR, _T("MSM5232Scan called without init\n"));
#endif

	struct BurnArea ba;

	if (nAction & ACB_DRIVER_DATA) {		
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = m_voi;
		ba.nLen	  = sizeof(VOICE) * 8;
		ba.szName = "Voice data";
		BurnAcb(&ba);

		SCAN_VAR(m_EN_out16[0]);
		SCAN_VAR(m_EN_out16[1]);
		SCAN_VAR(m_EN_out8[0]);
		SCAN_VAR(m_EN_out8[1]);
		SCAN_VAR(m_EN_out4[0]);
		SCAN_VAR(m_EN_out4[1]);
		SCAN_VAR(m_EN_out2[0]);
		SCAN_VAR(m_EN_out2[1]);
		SCAN_VAR(m_noise_cnt);
		SCAN_VAR(m_noise_step);
		SCAN_VAR(m_noise_rng);
		SCAN_VAR(m_noise_clocks);
		SCAN_VAR(m_control1);
		SCAN_VAR(m_control2);
		SCAN_VAR(m_gate);
		SCAN_VAR(m_chip_clock);
		SCAN_VAR(m_rate);
	}

	if (nAction & ACB_WRITE) {
		init_tables();
	}

	return 0;
}
