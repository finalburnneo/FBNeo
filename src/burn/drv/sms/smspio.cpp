/*
    pio.c --
    I/O chip and peripheral emulation.
*/
#include "smsshared.h"
#include "z80_intf.h"

io_state io_lut[2][256];
io_state *io_current;
UINT8 *hc_ntsc_256 = NULL;

void pio_init(void)
{
	INT32 i, j;

	/* Make pin state LUT */
	for(j = 0; j < 2; j++)
	{
		for(i = 0; i < 0x100; i++)
		{
			/* Common control: pin direction */
			io_lut[j][i].tr_dir[0]   = (i & 0x01) ? PIN_DIR_IN : PIN_DIR_OUT;
			io_lut[j][i].th_dir[0]   = (i & 0x02) ? PIN_DIR_IN : PIN_DIR_OUT;
			io_lut[j][i].tr_dir[1]   = (i & 0x04) ? PIN_DIR_IN : PIN_DIR_OUT;
			io_lut[j][i].th_dir[1]   = (i & 0x08) ? PIN_DIR_IN : PIN_DIR_OUT;

			if(j == 1)
			{
				/* Programmable output state (Export machines only) */
				io_lut[j][i].tr_level[0] = (i & 0x01) ? PIN_LVL_HI : (i & 0x10) ? PIN_LVL_HI : PIN_LVL_LO;
				io_lut[j][i].th_level[0] = (i & 0x02) ? PIN_LVL_HI : (i & 0x20) ? PIN_LVL_HI : PIN_LVL_LO;
				io_lut[j][i].tr_level[1] = (i & 0x04) ? PIN_LVL_HI : (i & 0x40) ? PIN_LVL_HI : PIN_LVL_LO;
				io_lut[j][i].th_level[1] = (i & 0x08) ? PIN_LVL_HI : (i & 0x80) ? PIN_LVL_HI : PIN_LVL_LO;
			}
			else
			{
				/* Fixed output state (Domestic machines only) */
				io_lut[j][i].tr_level[0] = (i & 0x01) ? PIN_LVL_HI : PIN_LVL_LO;
				io_lut[j][i].th_level[0] = (i & 0x02) ? PIN_LVL_HI : PIN_LVL_LO;
				io_lut[j][i].tr_level[1] = (i & 0x04) ? PIN_LVL_HI : PIN_LVL_LO;
				io_lut[j][i].th_level[1] = (i & 0x08) ? PIN_LVL_HI : PIN_LVL_LO;
			}
		}
	}

	// Generate h-counts table
	if (hc_ntsc_256 == NULL) {
		hc_ntsc_256 = (UINT8*)BurnMalloc(0x100);
		for (i = 0; i < 256; i++) {
			// Algo by Rupert Carmichael
			hc_ntsc_256[i] = ((i + 244) - (i / 4) - ((i + 1) % 4 == 0 ? 1 : 0) + (i > 212 ? 85 : 0)) % 256;
		}
	}

	// hack dos code doesn't call system_reset
	pio_reset();

	UINT8 vc = vc_table[0][0][0]; vc++; // avoid warning (this does nothing)
}


void pio_reset(void)
{
	/* GG SIO power-on defaults */
	sms.sio.pdr     = 0x7F;
	sms.sio.ddr     = 0xFF;
	sms.sio.txdata  = 0x00;
	sms.sio.rxdata  = 0xFF;
	sms.sio.sctrl   = 0x00;

	/* SMS I/O power-on defaults */
	ZetOpen(0);
	ioctrl_w(0xFF);
	ZetClose();
}


void pio_shutdown(void)
{
	BurnFree(hc_ntsc_256);
	hc_ntsc_256 = NULL;

	/* Nothing else to do */
}


void system_assign_device(INT32 port, INT32 type)
{
	sms.device[port].type = type;
}

void ioctrl_w(UINT8 data)
{
	UINT8 th_level_previous = (io_current) ? io_current->th_level[0] : 0;

	io_current = &io_lut[sms.territory][data];

	if ( (io_current->th_dir[0]   == PIN_DIR_IN) &&
		(io_current->th_level[0] == PIN_LVL_HI) &&
		(th_level_previous 	  == PIN_LVL_LO) )
	{
		sms.hlatch = hc_ntsc_256[ZetTotalCycles() % CYCLES_PER_LINE];
	}

	sms.ioctrl = data;
}

UINT8 device_r(INT32 port)
{
	UINT8 temp = 0x7F;

	switch(sms.device[port].type)
	{
		case DEVICE_NONE:
			break;
		case DEVICE_PAD2B:
			if(input.pad[port] & INPUT_UP)    temp &= ~0x01;
			if(input.pad[port] & INPUT_DOWN)  temp &= ~0x02;
			if(input.pad[port] & INPUT_LEFT)  temp &= ~0x04;
			if(input.pad[port] & INPUT_RIGHT)   temp &= ~0x08;
			if(input.pad[port] & INPUT_BUTTON1) temp &= ~0x10;  /* TL */
			if(input.pad[port] & INPUT_BUTTON2) temp &= ~0x20;  /* TR */
			break;

		case DEVICE_PHASER:
			if (io_current->th_dir[port] == PIN_DIR_IN) {
				const int h = hc_ntsc_256[ZetTotalCycles() % CYCLES_PER_LINE];
				const int gunX = input.analog[0] / 2;
				const int gunY = input.analog[1];

				if ( (gunX > h - 25 && gunX < h + 25) &&
					 (gunY > vdp.line - 8 && gunY < vdp.line + 8)
				   ) {
					if (sms.paddle_ff[port] == 0) {
						sms.hlatch = gunX + 22;
						sms.paddle_ff[port] = 1;
					}
					temp &= ~0x40;
				} else {
					sms.paddle_ff[port] = 0;
				}
			}

			if (input.pad[port] & INPUT_BUTTON1) temp &= ~0x10;
			break;

		case DEVICE_PADDLE:
			if (sms.territory == TERRITORY_EXPORT) {
				sms.paddle_ff[port] = (io_current->th_level[0] == PIN_LVL_LO);
			} else {
				sms.paddle_ff[port] ^= 1;
			}

			if (sms.paddle_ff[port]) {
				temp = (temp & 0xf0) | (input.analog[port] & 0x0f);
				temp &= ~0x20;
			} else {
				temp = (temp & 0xf0) | ((input.analog[port] >> 4) & 0x0f);
			}

			if (input.pad[port] & INPUT_BUTTON1) temp &= ~0x10;

			break;
	}

	return temp;
}

UINT8 input_r(INT32 offset)
{
	UINT8 temp = 0xFF;

	/*
	 If I/O chip is disabled, reads return last byte of instruction that
	 read the I/O port.
	 */
	if(sms.memctrl & 0x04)
		return z80_read_unmapped();

	offset &= 1;
	if(offset == 0)
	{
		/* read I/O port A pins */
		temp = device_r(0) & 0x3f;

		/* read I/O port B low pins (Game Gear is special case) */
		if (IS_GG) temp |= ((sio_r(1) & 0x03) << 6);
		else temp |= ((device_r(1) & 0x03) << 6);

		/* Adjust TR state if it is an output */
		if(io_current->tr_dir[0] == PIN_DIR_OUT)
		{
			temp &= ~0x20;
			temp |= (io_current->tr_level[0] == PIN_LVL_HI) ? 0x20 : 0x00;
		}
	}
	else
	{
		/* read I/O port B low pins (Game Gear is special case) */
		if (IS_GG)
		{
			UINT8 state = sio_r(0x01);
			temp = (state & 0x3C) >> 2;     /* Insert TR,TL,D3,D2       */
			temp |= ((state & 0x40) << 1);  /* Insert TH2               */
			temp |= 0x40;                   /* Insert TH1 (unconnected) */
		}
		else
		{
			UINT8 state = device_r(1);
			temp = (state & 0x3C) >> 2;   /* Insert TR,TL,D3,D2 */
			temp |= ((state & 0x40) << 1);  /* Insert TH2 */
			temp |= (device_r(0) & 0x40);   /* Insert TH1 */
		}

		/* Adjust TR state if it is an output */
		if(io_current->tr_dir[1] == PIN_DIR_OUT)
		{
			temp &= ~0x08;
			temp |= (io_current->tr_level[1] == PIN_LVL_HI) ? 0x08 : 0x00;
		}

		/* Adjust TH1 state if it is an output */
		if(io_current->th_dir[0] == PIN_DIR_OUT)
		{
			temp &= ~0x40;
			temp |= (io_current->th_level[0] == PIN_LVL_HI) ? 0x40 : 0x00;
		}

		/* Adjust TH2 state if it is an output */
		if(io_current->th_dir[1] == PIN_DIR_OUT)
		{
			temp &= ~0x80;
			temp |= (io_current->th_level[1] == PIN_LVL_HI) ? 0x80 : 0x00;
		}

		/* RESET and /CONT */
		temp |= 0x30;
		if (input.system & INPUT_RESET) temp &= ~0x10;
		if(IS_MD) temp &= ~0x20;
	}
	return temp;
}

UINT8 sio_r(INT32 offset)
{
	UINT8 temp;

	switch(offset & 0xFF)
	{
		case 0: /* Input port #2 */
			temp = 0xE0;
			if(input.system & INPUT_START)          temp &= ~0x80;
			if(sms.territory == TERRITORY_DOMESTIC) temp &= ~0x40;
			if(sms.display == DISPLAY_NTSC)         temp &= ~0x20;
			return temp;

		case 1: /* Parallel data register */
			temp = 0x00;
			temp |= (sms.sio.ddr & 0x01) ? 0x01 : (sms.sio.pdr & 0x01);
			temp |= (sms.sio.ddr & 0x02) ? 0x02 : (sms.sio.pdr & 0x02);
			temp |= (sms.sio.ddr & 0x04) ? 0x04 : (sms.sio.pdr & 0x04);
			temp |= (sms.sio.ddr & 0x08) ? 0x08 : (sms.sio.pdr & 0x08);
			temp |= (sms.sio.ddr & 0x10) ? 0x10 : (sms.sio.pdr & 0x10);
			temp |= (sms.sio.ddr & 0x20) ? 0x20 : (sms.sio.pdr & 0x20);
			temp |= (sms.sio.ddr & 0x40) ? 0x40 : (sms.sio.pdr & 0x40);
			temp |= (sms.sio.pdr & 0x80);
			return temp;

		case 2: /* Data direction register and NMI enable */
			return sms.sio.ddr;

		case 3: /* Transmit data buffer */
			return sms.sio.txdata;

		case 4: /* Receive data buffer */
			return sms.sio.rxdata;

		case 5: /* Serial control */
			return sms.sio.sctrl;

		case 6: /* Stereo sound control */
			return 0xFF;
	}

	/* Just to please compiler */
	return 0xff;
}

void sio_w(INT32 offset, INT32 data)
{
	switch(offset & 0xFF)
	{
		case 0: /* Input port #2 (read-only) */
			return;

		case 1: /* Parallel data register */
			sms.sio.pdr = data;
			return;

		case 2: /* Data direction register and NMI enable */
			sms.sio.ddr = data;
			return;

		case 3: /* Transmit data buffer */
			sms.sio.txdata = data;
			return;

		case 4: /* Receive data buffer */
			return;

		case 5: /* Serial control */
			sms.sio.sctrl = data & 0xF8;
			return;

		case 6: /* Stereo output control */
			psg_stereo_w(data);
			return;
	}
}

