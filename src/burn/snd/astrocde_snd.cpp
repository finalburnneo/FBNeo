// license:BSD-3-Clause
// copyright-holders:Aaron Giles,Frank Palazzolo
/***********************************************************

    Astrocade custom 'IO' chip sound chip driver
    Aaron Giles
    based on original work by Frank Palazzolo

************************************************************

    Register Map
    ============

    Register 0:
        D7..D0: Master oscillator frequency

    Register 1:
        D7..D0: Tone generator A frequency

    Register 2:
        D7..D0: Tone generator B frequency

    Register 3:
        D7..D0: Tone generator C frequency

    Register 4:
        D7..D6: Vibrato speed
        D5..D0: Vibrato depth

    Register 5:
            D5: Noise AM enable
            D4: Mux source (0=vibrato, 1=noise)
        D3..D0: Tone generator C volume

    Register 6:
        D7..D4: Tone generator B volume
        D3..D0: Tone generator A volume

    Register 7:
        D7..D0: Noise volume

************************************************************

    The device has active high(!) SO strobes triggered by
    read accesses, which transfer data from the 8 SI lines
    to the bus. Logically SO0-7 and SI0-7 ought to be hooked
    up to the same input matrix, but this only appears to be
    the case with the Astrocade home systems.  The arcade
    games instead channel the SI inputs through a quartet of
    MC14539B (pin-compatible with 74153) CMOS multiplexers
    and connect the SO strobes to unrelated outputs which
    generally use the upper 8 address bits as data.

***********************************************************/

#include "burnint.h"
#include "bitswap.h"
#include "astrocde_snd.h"
#include "stream.h"

struct AstroReg {
	UINT8       m_reg[8];         /* 8 control registers */

	UINT8       m_master_count;   /* current master oscillator count */
	UINT16      m_vibrato_clock;  /* current vibrato clock */

	UINT8       m_noise_clock;    /* current noise generator clock */
	UINT16      m_noise_state;    /* current noise LFSR state */

	UINT8       m_a_count;        /* current tone generator A count */
	UINT8       m_a_state;        /* current tone generator A state */

	UINT8       m_b_count;        /* current tone generator B count */
	UINT8       m_b_state;        /* current tone generator B state */

	UINT8       m_c_count;        /* current tone generator C count */
	UINT8       m_c_state;        /* current tone generator C state */
};

static UINT8 m_bitswap[256];   /* bitswap table */

static Stream stream[2]; // dink's stream

static AstroReg r[2];
static INT32 ndevs = 0;
static INT32 stream_id = 0;

static void sound_stream_update(INT16 **streams, INT32 samples);

void astrocde_snd_init(INT32 ndev, INT32 route)
{
	if (ndev > 1) {
		bprintf(PRINT_ERROR, _T("astrocde_snd: only 2 devices possible.\n"));
		return;
	}

	ndevs = ndev;

	stream[ndev].init((14318181)/2/4, nBurnSoundRate, 1, ndev, sound_stream_update);
	stream[ndev].set_volume(1.00);
	stream[ndev].set_route(route);

	for (int i = 0; i < 256; i++)
		m_bitswap[i] = BITSWAP08(i, 0,1,2,3,4,5,6,7);
}

void astrocde_snd_reset()
{
	for (int i = 0; i <= ndevs; i++) {
		bprintf(0, _T("resetting astrocde snd #%d\n"), i);
		memset(&r[i], 0, sizeof(AstroReg));
	}
}

void astrocde_snd_exit()
{
	for (int i = 0; i <= ndevs; i++) {
		stream[i].exit();
	}
	ndevs = 0;
}


//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------
#define d_min(a, b) (((a) < (b)) ? (a) : (b))
static void sound_stream_update(INT16 **streams, INT32 samples)
{
	INT16 *dest = streams[0];
	UINT16 noise_state;
	UINT8 master_count;
	UINT8 noise_clock;

	/* load some locals */
	master_count = r[stream_id].m_master_count;
	noise_clock = r[stream_id].m_noise_clock;
	noise_state = r[stream_id].m_noise_state;

	/* loop over samples */
	int samples_this_time;
//	constexpr stream_buffer::sample_t sample_scale = 1.0f / 60.0f;
	for (int sampindex = 0; sampindex < samples; sampindex += samples_this_time)
	{
		INT32 cursample = 0;

		/* compute the number of cycles until the next master oscillator reset */
		/* or until the next noise boundary */
		samples_this_time = d_min(samples - sampindex, 256 - master_count);
		samples_this_time = d_min(samples_this_time, 64 - noise_clock);

		/* sum the output of the tone generators */
		if (r[stream_id].m_a_state)
			cursample += r[stream_id].m_reg[6] & 0x0f;
		if (r[stream_id].m_b_state)
			cursample += r[stream_id].m_reg[6] >> 4;
		if (r[stream_id].m_c_state)
			cursample += r[stream_id].m_reg[5] & 0x0f;

		/* add in the noise if it is enabled, based on the top bit of the LFSR */
		if ((r[stream_id].m_reg[5] & 0x20) && (noise_state & 0x4000))
			cursample += r[stream_id].m_reg[7] >> 4;

		/* scale to max and output */
		cursample = cursample * 0x7fff / 60;
		for (int i = 0; i < samples_this_time; i++)
			*dest++ = cursample;

		/* clock the noise; a 2-bit counter clocks a 4-bit counter which clocks the LFSR */
		noise_clock += samples_this_time;
		if (noise_clock >= 64)
		{
			/* update the noise state; this is a 15-bit LFSR with feedback from */
			/* the XOR of the top two bits */
			noise_state = (noise_state << 1) | (~((noise_state >> 14) ^ (noise_state >> 13)) & 1);
			noise_clock -= 64;

			/* the same clock also controls the vibrato clock, which is a 13-bit counter */
			r[stream_id].m_vibrato_clock++;
		}

		/* clock the master oscillator; this is an 8-bit up counter */
		master_count += samples_this_time;
		if (master_count == 0)
		{
			/* reload based on mux value -- the value from the register is negative logic */
			master_count = ~r[stream_id].m_reg[0];

			/* mux value 0 means reload based on the vibrato control */
			if ((r[stream_id].m_reg[5] & 0x10) == 0)
			{
				/* vibrato speed (register 4 bits 6-7) selects one of the top 4 bits */
				/* of the 13-bit vibrato clock to use (0=highest freq, 3=lowest) */
				if (!((r[stream_id].m_vibrato_clock >> (r[stream_id].m_reg[4] >> 6)) & 0x0200))
				{
					/* if the bit is clear, we add the vibrato volume to the counter */
					master_count += r[stream_id].m_reg[4] & 0x3f;
				}
			}

			/* mux value 1 means reload based on the noise control */
			else
			{
				/* the top 8 bits of the noise LFSR are ANDed with the noise volume */
				/* register and added to the count */
				master_count += m_bitswap[(noise_state >> 7) & 0xff] & r[stream_id].m_reg[7];
			}

			/* clock tone A */
			if (++r[stream_id].m_a_count == 0)
			{
				r[stream_id].m_a_state ^= 1;
				r[stream_id].m_a_count = ~r[stream_id].m_reg[1];
			}

			/* clock tone B */
			if (++r[stream_id].m_b_count == 0)
			{
				r[stream_id].m_b_state ^= 1;
				r[stream_id].m_b_count = ~r[stream_id].m_reg[2];
			}

			/* clock tone C */
			if (++r[stream_id].m_c_count == 0)
			{
				r[stream_id].m_c_state ^= 1;
				r[stream_id].m_c_count = ~r[stream_id].m_reg[3];
			}
		}
	}

	/* put back the locals */
	r[stream_id].m_master_count = master_count;
	r[stream_id].m_noise_clock = noise_clock;
	r[stream_id].m_noise_state = noise_state;
}


/*************************************
 *
 *  Sound write accessors
 *
 *************************************/

void astrocde_snd_update(INT16 *output, INT32 samples_len)
{
	if (samples_len != nBurnSoundLen) {
		bprintf(0, _T("astrocde_snd_update(): once per frame, please!\n"));
		return;
	}

	for (int i = 0; i <= ndevs; i++) {
		stream_id = i;
		stream[i].render(output, samples_len);
	}
}

void astrocde_snd_set_buffered(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz)
{
	for (int i = 0; i <= ndevs; i++) {
		stream_id = i;
		stream[i].set_buffered(pCPUCyclesCB, nCPUMhz);
	}
}

void astrocde_snd_set_volume(double vol)
{
	for (int i = 0; i <= ndevs; i++) {
		stream[i].set_volume(vol);
	}
}

void astrocde_snd_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		for (int i = 0; i <= ndevs; i++) {
			SCAN_VAR(r[i]);
		}
	}
}

void astrocde_snd_write(INT32 dev, INT32 offset, UINT8 data)
{
	if ((offset & 8) != 0)
		offset = (offset >> 8) & 7;
	else
		offset &= 7;

	/* update */
	stream_id = dev;
	stream[dev].update();

	/* stash the new register value */
	r[dev].m_reg[offset & 7] = data;
}
