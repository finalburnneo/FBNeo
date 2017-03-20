// Based on MAME sources by Fabio Priuli,Philip Bennett

#include "burnint.h"
#include "mb87078.h"

static INT32        m_gain[4];       /* gain index 0-63,64,65 */
static INT32        m_channel_latch; /* current channel */
static UINT8        m_latch[2][4];   /* 6bit+3bit 4 data latches */
static UINT8        m_reset_comp;
static void         (*m_gain_changed_cb)(INT32, INT32) = NULL;

static void reset_comp_w( int level );

static const float mb87078_gain_decibel[66] = {
	0.0, -0.5, -1.0, -1.5, -2.0, -2.5, -3.0, -3.5,
	-4.0, -4.5, -5.0, -5.5, -6.0, -6.5, -7.0, -7.5,
	-8.0, -8.5, -9.0, -9.5,-10.0,-10.5,-11.0,-11.5,
	-12.0,-12.5,-13.0,-13.5,-14.0,-14.5,-15.0,-15.5,
	-16.0,-16.5,-17.0,-17.5,-18.0,-18.5,-19.0,-19.5,
	-20.0,-20.5,-21.0,-21.5,-22.0,-22.5,-23.0,-23.5,
	-24.0,-24.5,-25.0,-25.5,-26.0,-26.5,-27.0,-27.5,
	-28.0,-28.5,-29.0,-29.5,-30.0,-30.5,-31.0,-31.5,
	-32.0, -256.0
	};

static const int mb87078_gain_percent[66] = {
	100,94,89,84,79,74,70,66,
	63,59,56,53,50,47,44,42,
	39,37,35,33,31,29,28,26,
	25,23,22,21,19,18,17,16,
	15,14,14,13,12,11,11,10,
	10, 9, 8, 8, 7, 7, 7, 6,
	6, 5, 5, 5, 5, 4, 4, 4,
	3, 3, 3, 3, 3, 2, 2, 2,
	2, 0
};

void mb87078_init(void (*callback)(INT32, INT32))
{
	m_gain_changed_cb = callback;
	mb87078_reset();
}

void mb87078_exit()
{
	// nothing to do here
}

void mb87078_scan()
{
	SCAN_VAR(m_channel_latch);
	SCAN_VAR(m_reset_comp);
	SCAN_VAR(m_gain);
	SCAN_VAR(m_latch);
}

void mb87078_reset()
{
	m_channel_latch = 0;
	m_reset_comp = 0;
	m_gain[0] = m_gain[1] = m_gain[2] = m_gain[3] = 0;
	memset(m_latch, 0, sizeof(m_latch));

	/* reset chip */
	reset_comp_w(0);
	reset_comp_w(1);
}

/*****************************************************************************
    IMPLEMENTATION
*****************************************************************************/

#define GAIN_MAX_INDEX 64
#define GAIN_INFINITY_INDEX 65


static INT32 calc_gain_index( INT32 data0, INT32 data1 )
{
//data 0: GD0-GD5
//data 1: 1  2  4  8  16
//        c1 c2 EN C0 C32

	if (!(data1 & 0x04))
	{
		return GAIN_INFINITY_INDEX;
	}
	else
	{
		if (data1 & 0x10)
		{
			return GAIN_MAX_INDEX;
		}
		else
		{
			if (data1 & 0x08)
			{
				return 0;
			}
			else
			{
				return (data0 ^ 0x3f);
			}
		}
	}
}


static void gain_recalc()
{
	for (INT32 i = 0; i < 4; i++)
	{
		INT32 old_index = m_gain[i];
		m_gain[i] = calc_gain_index(m_latch[0][i], m_latch[1][i]);
		if (old_index != m_gain[i])
			m_gain_changed_cb(i, mb87078_gain_percent[m_gain[i]]);
	}
}


void mb87078_write(INT32 dsel, INT32 data)
{
	if (m_reset_comp == 0)
		return;

	if (dsel == 0)  /* gd0 - gd5 */
	{
		m_latch[0][m_channel_latch] = data & 0x3f;
	}
	else        /* dcs1, dsc2, en, c0, c32, X */
	{
		m_channel_latch = data & 3;
		m_latch[1][m_channel_latch] = data & 0x1f; //always zero bit 5
	}
	gain_recalc();
}


float mb87078_gain_decibel_r( INT32 channel )
{
	return mb87078_gain_decibel[m_gain[channel]];
}


INT32 mb87078_gain_percent_r( INT32 channel )
{
	return mb87078_gain_percent[m_gain[channel]];
}

static void reset_comp_w( INT32 level )
{
	m_reset_comp = level;

	/*this seems to be true, according to the datasheets*/
	if (level == 0)
	{
		m_latch[0][0] = 0x3f;
		m_latch[0][1] = 0x3f;
		m_latch[0][2] = 0x3f;
		m_latch[0][3] = 0x3f;

		m_latch[1][0] = 0x0 | 0x4;
		m_latch[1][1] = 0x1 | 0x4;
		m_latch[1][2] = 0x2 | 0x4;
		m_latch[1][3] = 0x3 | 0x4;
	}

	gain_recalc();
}
