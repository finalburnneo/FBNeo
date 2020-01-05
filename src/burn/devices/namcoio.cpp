#include "burnint.h"
#include "namcoio.h"
#include "driver.h"

struct ChipData
{
	UINT8 (*in_0_cb)(UINT8);
	UINT8 (*in_1_cb)(UINT8);
	UINT8 (*in_2_cb)(UINT8);
	UINT8 (*in_3_cb)(UINT8);
	void (*out_0_cb)(UINT8, UINT8);
	void (*out_1_cb)(UINT8, UINT8);
	void (*run_func)(INT32);
	INT32 type;

	UINT8	ram[16];
	INT32	reset;
	INT32	lastcoins;
	INT32	lastbuttons;
	INT32	credits;
	INT32	coins[2];
	INT32	coins_per_cred[2];
	INT32	creds_per_coin[2];
	INT32	in_count;
};

static struct ChipData Chips[5];

static UINT8 fakeIn(UINT8) { return 0; }
static void fakeOut(UINT8,UINT8) { }

void namcoio_init(INT32 chip, INT32 type, UINT8 (*in0)(UINT8), UINT8 (*in1)(UINT8), UINT8 (*in2)(UINT8), UINT8 (*in3)(UINT8), void (*out0)(UINT8, UINT8), void (*out1)(UINT8, UINT8))
{
	ChipData *ptr = &Chips[chip];

	ptr->type = type;

	ptr->in_0_cb = (in0 == NULL) ? fakeIn : in0;
	ptr->in_1_cb = (in1 == NULL) ? fakeIn : in1;
	ptr->in_2_cb = (in2 == NULL) ? fakeIn : in2;
	ptr->in_3_cb = (in3 == NULL) ? fakeIn : in3;
	ptr->out_0_cb = (out0 == NULL) ? fakeOut : out0;
	ptr->out_1_cb = (out1 == NULL) ? fakeOut : out1;

	switch (type)
	{
		case NAMCO56xx: ptr->run_func = namco56xx_customio_run; break;
		case NAMCO58xx: ptr->run_func = namco58xx_customio_run; break;
		case NAMCO59xx: ptr->run_func = namco59xx_customio_run; break;
	}
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void namcoio_set_reset_line(INT32 chip, INT32 state)
{
	ChipData *ptr = &Chips[chip];

	ptr->reset = (state == ASSERT_LINE) ? 1 : 0;
	if (state != CLEAR_LINE)
	{
		/* reset internal registers */
		ptr->credits = 0;
		ptr->coins[0] = 0;
		ptr->coins_per_cred[0] = 1;
		ptr->creds_per_coin[0] = 1;
		ptr->coins[1] = 0;
		ptr->coins_per_cred[1] = 1;
		ptr->creds_per_coin[1] = 1;
		ptr->in_count = 0;
	}
}

void namcoio_reset(INT32 chip)
{
	namcoio_set_reset_line(chip, ASSERT_LINE);
	namcoio_set_reset_line(chip, CLEAR_LINE);
}

/*****************************************************************************
    DEVICE HANDLERS
*****************************************************************************/

#define IORAM_READ(offset) (ptr->ram[offset] & 0x0f)
#define IORAM_WRITE(offset,data) {ptr->ram[offset] = (data) & 0x0f;}

static void handle_coins( INT32 chip, int swap )
{
	int val, toggled;
	int credit_add = 0;
	int credit_sub = 0;
	int button;

	ChipData *ptr = &Chips[chip];

	val = ~ptr->in_0_cb(0 & 0x0f);    // pins 38-41
	toggled = val ^ ptr->lastcoins;
	ptr->lastcoins = val;

	/* check coin insertion */
	if (val & toggled & 0x01)
	{
		ptr->coins[0]++;
		if (ptr->coins[0] >= (ptr->coins_per_cred[0] & 7))
		{
			credit_add = ptr->creds_per_coin[0] - (ptr->coins_per_cred[0] >> 3);
			ptr->coins[0] -= ptr->coins_per_cred[0] & 7;
		}
		else if (ptr->coins_per_cred[0] & 8)
			credit_add = 1;
	}
	if (val & toggled & 0x02)
	{
		ptr->coins[1]++;
		if (ptr->coins[1] >= (ptr->coins_per_cred[1] & 7))
		{
			credit_add = ptr->creds_per_coin[1] - (ptr->coins_per_cred[1] >> 3);
			ptr->coins[1] -= ptr->coins_per_cred[1] & 7;
		}
		else if (ptr->coins_per_cred[1] & 8)
			credit_add = 1;
	}
	if (val & toggled & 0x08)
	{
		credit_add = 1;
	}

	val = ~ptr->in_3_cb(0 & 0x0f);    // pins 30-33
	toggled = val ^ ptr->lastbuttons;
	ptr->lastbuttons = val;

	/* check start buttons, only if the game allows */
	if (IORAM_READ(9) == 0)
	// the other argument is IORAM_READ(10) = 1, meaning unknown
	{
		if (val & toggled & 0x04)
		{
			if (ptr->credits >= 1) credit_sub = 1;
		}
		else if (val & toggled & 0x08)
		{
			if (ptr->credits >= 2) credit_sub = 2;
		}
	}

	ptr->credits += credit_add - credit_sub;

	IORAM_WRITE(0 ^ swap, ptr->credits / 10);   // BCD credits
	IORAM_WRITE(1 ^ swap, ptr->credits % 10);   // BCD credits
	IORAM_WRITE(2 ^ swap, credit_add);  // credit increment (coin inputs)
	IORAM_WRITE(3 ^ swap, credit_sub);  // credit decrement (start buttons)
	IORAM_WRITE(4, ~ptr->in_1_cb(0 & 0x0f));  // pins 22-25
	button = ((val & 0x05) << 1) | (val & toggled & 0x05);
	IORAM_WRITE(5, button); // pins 30 & 32 normal and impulse
	IORAM_WRITE(6, ~ptr->in_2_cb(0 & 0x0f));  // pins 26-29
	button = (val & 0x0a) | ((val & toggled & 0x0a) >> 1);
	IORAM_WRITE(7, button); // pins 31 & 33 normal and impulse
}


void namco56xx_customio_run(INT32 chip)
{
	ChipData *ptr = &Chips[chip];

	switch (IORAM_READ(8))
	{
		case 1: // read switch inputs
			IORAM_WRITE(0, ~ptr->in_0_cb(0 & 0x0f));  // pins 38-41
			IORAM_WRITE(1, ~ptr->in_1_cb(0 & 0x0f));  // pins 22-25
			IORAM_WRITE(2, ~ptr->in_2_cb(0 & 0x0f));  // pins 26-29
			IORAM_WRITE(3, ~ptr->in_3_cb(0 & 0x0f));  // pins 30-33
			ptr->out_0_cb((UINT8)0, IORAM_READ(9));   // output to pins 13-16 (motos, pacnpal, gaplus)
			ptr->out_1_cb((UINT8)0, IORAM_READ(10));  // output to pins 17-20 (gaplus)
		break;

		case 2: // initialize coinage settings
			ptr->coins_per_cred[0] = IORAM_READ(9);
			ptr->creds_per_coin[0] = IORAM_READ(10);
			ptr->coins_per_cred[1] = IORAM_READ(11);
			ptr->creds_per_coin[1] = IORAM_READ(12);
		break;

		case 4:
			handle_coins(chip,0);
		break;

		case 7: // bootup check (liblrabl only)
			{
				IORAM_WRITE(2, 0xe);
				IORAM_WRITE(7, 0x6);
			}
		break;

		case 8: // bootup check
			{
				int i, sum;

				sum = 0;
				for (i = 9; i < 16; i++)
					sum += IORAM_READ(i);
				IORAM_WRITE(0, sum >> 4);
				IORAM_WRITE(1, sum & 0xf);
			}
		break;

		case 9: // read dip switches and inputs
			ptr->out_0_cb((UINT8)0, 0 & 0x0f);   // set pin 13 = 0
			IORAM_WRITE(0, ~ptr->in_0_cb(0 & 0x0f));  // pins 38-41, pin 13 = 0
			IORAM_WRITE(2, ~ptr->in_1_cb(0 & 0x0f));  // pins 22-25, pin 13 = 0
			IORAM_WRITE(4, ~ptr->in_2_cb(0 & 0x0f));  // pins 26-29, pin 13 = 0
			IORAM_WRITE(6, ~ptr->in_3_cb(0 & 0x0f));  // pins 30-33, pin 13 = 0
			ptr->out_0_cb((UINT8)0, 1 & 0x0f);   // set pin 13 = 1
			IORAM_WRITE(1, ~ptr->in_0_cb(0 & 0x0f));  // pins 38-41, pin 13 = 1
			IORAM_WRITE(3, ~ptr->in_1_cb(0 & 0x0f));  // pins 22-25, pin 13 = 1
			IORAM_WRITE(5, ~ptr->in_2_cb(0 & 0x0f));  // pins 26-29, pin 13 = 1
			IORAM_WRITE(7, ~ptr->in_3_cb(0 & 0x0f));  // pins 30-33, pin 13 = 1
		break;
	}
}

void namco59xx_customio_run(INT32 chip)
{
	ChipData *ptr = &Chips[chip];

	switch (IORAM_READ(8))
	{
		case 3: // pacnpal chip #1: read dip switches and inputs
			IORAM_WRITE(4, ~ptr->in_0_cb(0 & 0x0f));  // pins 38-41, pin 13 = 0 ?
			IORAM_WRITE(5, ~ptr->in_2_cb(0 & 0x0f));  // pins 26-29 ?
			IORAM_WRITE(6, ~ptr->in_1_cb(0 & 0x0f));  // pins 22-25 ?
			IORAM_WRITE(7, ~ptr->in_3_cb(0 & 0x0f));  // pins 30-33
		break;
	}
}

void namco58xx_customio_run(INT32 chip)
{
	ChipData *ptr = &Chips[chip];

	switch (IORAM_READ(8))
	{
		case 1: // read switch inputs
			IORAM_WRITE(4, ~ptr->in_0_cb(0 & 0x0f));  // pins 38-41
			IORAM_WRITE(5, ~ptr->in_1_cb(0 & 0x0f));  // pins 22-25
			IORAM_WRITE(6, ~ptr->in_2_cb(0 & 0x0f));  // pins 26-29
			IORAM_WRITE(7, ~ptr->in_3_cb(0 & 0x0f));  // pins 30-33
			ptr->out_0_cb((UINT8)0, IORAM_READ(9));   // output to pins 13-16 (toypop)
			ptr->out_1_cb((UINT8)0, IORAM_READ(10));  // output to pins 17-20 (toypop)
		break;

		case 2: // initialize coinage settings
			ptr->coins_per_cred[0] = IORAM_READ(9);
			ptr->creds_per_coin[0] = IORAM_READ(10);
			ptr->coins_per_cred[1] = IORAM_READ(11);
			ptr->creds_per_coin[1] = IORAM_READ(12);
		break;

		case 3: // process coin and start inputs, read switch inputs
			handle_coins(chip,2);
		break;

		case 4: // read dip switches and inputs
			ptr->out_0_cb((UINT8)0, 0 & 0x0f);   // set pin 13 = 0
			IORAM_WRITE(0, ~ptr->in_0_cb(0 & 0x0f));  // pins 38-41, pin 13 = 0
			IORAM_WRITE(2, ~ptr->in_1_cb(0 & 0x0f));  // pins 22-25, pin 13 = 0
			IORAM_WRITE(4, ~ptr->in_2_cb(0 & 0x0f));  // pins 26-29, pin 13 = 0
			IORAM_WRITE(6, ~ptr->in_3_cb(0 & 0x0f));  // pins 30-33, pin 13 = 0
			ptr->out_0_cb((UINT8)0, 1 & 0x0f);   // set pin 13 = 1
			IORAM_WRITE(1, ~ptr->in_0_cb(0 & 0x0f));  // pins 38-41, pin 13 = 1
			IORAM_WRITE(3, ~ptr->in_1_cb(0 & 0x0f));  // pins 22-25, pin 13 = 1
			IORAM_WRITE(5, ~ptr->in_2_cb(0 & 0x0f));  // pins 26-29, pin 13 = 1
			IORAM_WRITE(7, ~ptr->in_3_cb(0 & 0x0f));  // pins 30-33, pin 13 = 1
		break;

		case 5: // bootup check
			{
				int i, n, rng, seed;
				#define NEXT(n) ((((n) & 1) ? (n) ^ 0x90 : (n)) >> 1)

				/* initialize the LFSR depending on the first two arguments */
				n = (IORAM_READ(9) * 16 + IORAM_READ(10)) & 0x7f;
				seed = 0x22;
				for (i = 0; i < n; i++)
					seed = NEXT(seed);

				/* calculate the answer */
				for (i = 1; i < 8; i++)
				{
					n = 0;
					rng = seed;
					if (rng & 1) { n ^= ~IORAM_READ(11); }
					rng = NEXT(rng);
					seed = rng; // save state for next loop
					if (rng & 1) { n ^= ~IORAM_READ(10); }
					rng = NEXT(rng);
					if (rng & 1) { n ^= ~IORAM_READ(9); }
					rng = NEXT(rng);
					if (rng & 1) { n ^= ~IORAM_READ(15); }
					rng = NEXT(rng);
					if (rng & 1) { n ^= ~IORAM_READ(14); }
					rng = NEXT(rng);
					if (rng & 1) { n ^= ~IORAM_READ(13); }
					rng = NEXT(rng);
					if (rng & 1) { n ^= ~IORAM_READ(12); }

					IORAM_WRITE(i, ~n);
				}
				IORAM_WRITE(0, 0x0);
				/* kludge for gaplus */
				if (IORAM_READ(9) == 0xf) IORAM_WRITE(0, 0xf);
			}
		break;
	}
}

void namcoio_run(INT32 chip)
{
	ChipData *ptr = &Chips[chip];

	ptr->run_func(chip);
}	

UINT8 namcoio_read(INT32 chip, UINT8 offset)
{
	ChipData *ptr = &Chips[chip];

	// RAM is 4-bit wide; Pac & Pal requires the | 0xf0 otherwise Easter egg doesn't work
	offset &= 0x3f;

	return 0xf0 | ptr->ram[offset];
}

void namcoio_write(INT32 chip, UINT8 offset, UINT8 data)
{
	ChipData *ptr = &Chips[chip];

	offset &= 0x3f;
	data &= 0x0f;   // RAM is 4-bit wide

	ptr->ram[offset] = data;
}

UINT8 namcoio_read_reset_line(INT32 chip)
{
	ChipData *ptr = &Chips[chip];

	return ptr->reset;
}

INT32 namcoio_scan(INT32 chip)
{
	ChipData *ptr = &Chips[chip];

	SCAN_VAR(ptr->ram);
	SCAN_VAR(ptr->reset);
	SCAN_VAR(ptr->lastcoins);
	SCAN_VAR(ptr->lastbuttons);
	SCAN_VAR(ptr->coins);
	SCAN_VAR(ptr->credits);
	SCAN_VAR(ptr->coins_per_cred);
	SCAN_VAR(ptr->creds_per_coin);
	SCAN_VAR(ptr->in_count);

	return 0;
}
