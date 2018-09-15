// FB Alpha Battlezone / Bradley Tank Trainer / Red Baron driver module
// Based on MAME driver by Brad Oliver and Nicola Salmoria

// to do:
//	fix bzone / bradley
// 	hook up analog inputs
//	hook up custom sounds_w
//	bug fix

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "burn_gun.h"
#include "mathbox.h"
#include "vector.h"
#include "avgdvg.h"
#include "pokey.h"
#include "watchdog.h"
#include "earom.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVectorRAM;
static UINT8 *DrvVectorROM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles;
static INT32 avgletsgo = 0;
static UINT8 analog_data = 0;
static INT32 input_select = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;
static INT16 DrvAnalogPort2 = 0;
static INT16 DrvAnalogPort3 = 0;

static INT32 redbaron = 0;

static struct BurnInputInfo BzoneInputList[] = {
	{"Coin 1",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"Coin 2",				BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"Start 1",				BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"	},
	{"Start 2",				BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"Left Stick Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"Left Stick Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"Right Stick Up",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 up"		},
	{"Right Stick Down",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 down"	},
	{"Fire",				BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Bzone)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo RedbaronInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 start"	},
	{"P1 Up",				BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",				BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 fire 1"	},

	A("P1 Stick X",         BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis" ),
	A("P1 Stick Y",         BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis" ),

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Diagnostic Step",		BIT_DIGITAL,	DrvJoy1 + 5,	"service2"	},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};
#undef A

STDINPUTINFO(Redbaron)

static struct BurnInputInfo BradleyInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 6"	},
	{"P1 Button 7",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 fire 7"	},
	{"P1 Button 8",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 fire 8"	},
	{"P1 Button 9",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 9"	},
	{"P1 Button 10",	BIT_DIGITAL,	DrvJoy4 + 4,	"p1 fire 10"},

	// analog placeholders
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 fire 7"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bradley)

static struct BurnDIPInfo BzoneDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x15, NULL					},
	{0x0b, 0xff, 0xff, 0x03, NULL					},
	{0x0c, 0xff, 0xff, 0x10, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0a, 0x01, 0x03, 0x00, "2"					},
	{0x0a, 0x01, 0x03, 0x01, "3"					},
	{0x0a, 0x01, 0x03, 0x02, "4"					},
	{0x0a, 0x01, 0x03, 0x03, "5"					},

	{0   , 0xfe, 0   ,    4, "Missile appears at"	},
	{0x0a, 0x01, 0x0c, 0x00, "5000"					},
	{0x0a, 0x01, 0x0c, 0x04, "10000"				},
	{0x0a, 0x01, 0x0c, 0x08, "20000"				},
	{0x0a, 0x01, 0x0c, 0x0c, "30000"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x0a, 0x01, 0x30, 0x10, "15k and 100k"			},
	{0x0a, 0x01, 0x30, 0x20, "25k and 100k"			},
	{0x0a, 0x01, 0x30, 0x30, "50k and 100k"			},
	{0x0a, 0x01, 0x30, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x0a, 0x01, 0xc0, 0x00, "English"				},
	{0x0a, 0x01, 0xc0, 0x40, "German"				},
	{0x0a, 0x01, 0xc0, 0x80, "French"				},
	{0x0a, 0x01, 0xc0, 0xc0, "Spanish"				},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0b, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{0x0b, 0x01, 0x03, 0x02, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x0b, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x0b, 0x01, 0x0c, 0x00, "*1"					},
	{0x0b, 0x01, 0x0c, 0x04, "*4"					},
	{0x0b, 0x01, 0x0c, 0x08, "*5"					},
	{0x0b, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Coin A"				},
	{0x0b, 0x01, 0x10, 0x00, "*1"					},
	{0x0b, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    5, "Bonus Coins"			},
	{0x0b, 0x01, 0xe0, 0x00, "None"					},
	{0x0b, 0x01, 0xe0, 0x20, "3 credits/2 coins"	},
	{0x0b, 0x01, 0xe0, 0x40, "5 credits/4 coins"	},
	{0x0b, 0x01, 0xe0, 0x60, "6 credits/4 coins"	},
	{0x0b, 0x01, 0xe0, 0x80, "6 credits/5 coins"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0c, 0x01, 0x10, 0x00, "On"					},
	{0x0c, 0x01, 0x10, 0x10, "Off"					},
};

STDDIPINFO(Bzone)

static struct BurnDIPInfo RedbaronDIPList[]=
{
	{0x0b, 0xff, 0xff, 0xfd, NULL					},
	{0x0c, 0xff, 0xff, 0xe7, NULL					},
	{0x0d, 0xff, 0xff, 0x10, NULL					},

	{0   , 0xfe, 0   ,    1, "Coinage"				},
	{0x0b, 0x01, 0xff, 0xfd, "Normal"				},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x0c, 0x01, 0x03, 0x00, "German"				},
	{0x0c, 0x01, 0x03, 0x01, "French"				},
	{0x0c, 0x01, 0x03, 0x02, "Spanish"				},
	{0x0c, 0x01, 0x03, 0x03, "English"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x0c, 0x01, 0x0c, 0x0c, "2k 10k 30k"			},
	{0x0c, 0x01, 0x0c, 0x08, "4k 15k 40k"			},
	{0x0c, 0x01, 0x0c, 0x04, "6k 20k 50k"			},
	{0x0c, 0x01, 0x0c, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0c, 0x01, 0x30, 0x30, "2"					},
	{0x0c, 0x01, 0x30, 0x20, "3"					},
	{0x0c, 0x01, 0x30, 0x10, "4"					},
	{0x0c, 0x01, 0x30, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "One Play Minimum"		},
	{0x0c, 0x01, 0x40, 0x40, "Off"					},
	{0x0c, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Self Adjust Diff"		},
	{0x0c, 0x01, 0x80, 0x80, "Off"					},
	{0x0c, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0d, 0x01, 0x10, 0x00, "On"					},
	{0x0d, 0x01, 0x10, 0x10, "Off"					},
};

STDDIPINFO(Redbaron)

static struct BurnDIPInfo BradleyDIPList[]=
{
	{0x12, 0xff, 0xff, 0x15, NULL					},
	{0x13, 0xff, 0xff, 0x03, NULL					},
	{0x14, 0xff, 0xff, 0x10, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x12, 0x01, 0x03, 0x00, "2"					},
	{0x12, 0x01, 0x03, 0x01, "3"					},
	{0x12, 0x01, 0x03, 0x02, "4"					},
	{0x12, 0x01, 0x03, 0x03, "5"					},

	{0   , 0xfe, 0   ,    4, "Missile appears at"	},
	{0x12, 0x01, 0x0c, 0x00, "5000"					},
	{0x12, 0x01, 0x0c, 0x04, "10000"				},
	{0x12, 0x01, 0x0c, 0x08, "20000"				},
	{0x12, 0x01, 0x0c, 0x0c, "30000"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x12, 0x01, 0x30, 0x10, "15k and 100k"			},
	{0x12, 0x01, 0x30, 0x20, "25k and 100k"			},
	{0x12, 0x01, 0x30, 0x30, "50k and 100k"			},
	{0x12, 0x01, 0x30, 0x00, "None"					},

	{0   , 0xfe, 0   ,    4, "Language"				},
	{0x12, 0x01, 0xc0, 0x00, "English"				},
	{0x12, 0x01, 0xc0, 0x40, "German"				},
	{0x12, 0x01, 0xc0, 0x80, "French"				},
	{0x12, 0x01, 0xc0, 0xc0, "Spanish"				},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x13, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x03, 0x02, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x13, 0x01, 0x0c, 0x00, "*1"					},
	{0x13, 0x01, 0x0c, 0x04, "*4"					},
	{0x13, 0x01, 0x0c, 0x08, "*5"					},
	{0x13, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Coin A"				},
	{0x13, 0x01, 0x10, 0x00, "*1"					},
	{0x13, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    5, "Bonus Coins"			},
	{0x13, 0x01, 0xe0, 0x00, "None"					},
	{0x13, 0x01, 0xe0, 0x20, "3 credits/2 coins"	},
	{0x13, 0x01, 0xe0, 0x40, "5 credits/4 coins"	},
	{0x13, 0x01, 0xe0, 0x60, "6 credits/4 coins"	},
	{0x13, 0x01, 0xe0, 0x80, "6 credits/5 coins"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x10, 0x00, "On"					},
	{0x14, 0x01, 0x10, 0x10, "Off"					},
};

STDDIPINFO(Bradley)


static int m_latch;
static int m_poly_counter;
static int m_poly_shift;
static int m_filter_counter;
static int m_crash_amp;
static int m_shot_amp;
static int m_shot_amp_counter;
static int m_squeal_amp;
static int m_squeal_amp_counter;
static int m_squeal_off_counter;
static int m_squeal_on_counter;
static int m_squeal_out;
static INT16 *m_vol_lookup;
static INT16 m_vol_crash[16];

void redbaron_sound_reset()
{
	m_latch = 0;
	m_poly_counter = 0;
	m_poly_shift = 0;
	m_filter_counter = 0;
	m_crash_amp = 0;
	m_shot_amp = 0;
	m_shot_amp_counter = 0;
	m_squeal_amp = 0;
	m_squeal_amp_counter = 0;
	m_squeal_off_counter = 0;
	m_squeal_on_counter = 0;
	m_squeal_out = 0;
}

void redbaron_sound_init()
{
	int i;

	m_vol_lookup = (INT16*)BurnMalloc(0x8000 * 2);

	for( i = 0; i < 0x8000; i++ )
		m_vol_lookup[0x7fff-i] = (int16_t) (0x7fff/exp(1.0*i/4096));

	for( i = 0; i < 16; i++ )
	{
		/* r0 = R18 and R24, r1 = open */
		double r0 = 1.0/(5600 + 680), r1 = 1/6e12;

		/* R14 */
		if( i & 1 )
			r1 += 1.0/8200;
		else
			r0 += 1.0/8200;
		/* R15 */
		if( i & 2 )
			r1 += 1.0/3900;
		else
			r0 += 1.0/3900;
		/* R16 */
		if( i & 4 )
			r1 += 1.0/2200;
		else
			r0 += 1.0/2200;
		/* R17 */
		if( i & 8 )
			r1 += 1.0/1000;
		else
			r0 += 1.0/1000;

		r0 = 1.0/r0;
		r1 = 1.0/r1;
		m_vol_crash[i] = (INT16)(32767 * r0 / (r0 + r1));
	}
}

void redbaron_sound_exit()
{
	BurnFree(m_vol_lookup);
}

void redbaron_sound_update(INT16 *buffer, int samples)
{
#define OUTPUT_RATE	nBurnSoundRate

	while( samples-- )
	{
		int sum = 0;

		/* polynomial shifter E5 and F4 (LS164) clocked with 12kHz */
		m_poly_counter -= 12000;
		while( m_poly_counter <= 0 )
		{
			m_poly_counter += OUTPUT_RATE;
			if( ((m_poly_shift & 0x0001) == 0) == ((m_poly_shift & 0x4000) == 0) )
				m_poly_shift = (m_poly_shift << 1) | 1;
			else
				m_poly_shift <<= 1;
		}

		/* What is the exact low pass filter frequency? */
		m_filter_counter -= 330;
		while( m_filter_counter <= 0 )
		{
			m_filter_counter += OUTPUT_RATE;
			m_crash_amp = (m_poly_shift & 1) ? m_latch >> 4 : 0;
		}
		/* mix crash sound at 35% */
		sum += m_vol_crash[m_crash_amp] * 35 / 100;

		/* shot not active: charge C32 (0.1u) */
		if( (m_latch & 0x04) == 0 )
			m_shot_amp = 32767;
		else
		if( (m_poly_shift & 0x8000) == 0 )
		{
			if( m_shot_amp > 0 )
			{
				/* I think this is too short. Is C32 really 1u? */
				#define C32_DISCHARGE_TIME (int)(32767 / 0.03264);
				m_shot_amp_counter -= C32_DISCHARGE_TIME;
				while( m_shot_amp_counter <= 0 )
				{
					m_shot_amp_counter += OUTPUT_RATE;
					if( --m_shot_amp == 0 )
						break;
				}
				/* mix shot sound at 35% */
				sum += m_vol_lookup[m_shot_amp] * 35 / 100;
			}
		}


		if( (m_latch & 0x02) == 0 )
			m_squeal_amp = 0;
		else
		{
			if( m_squeal_amp < 32767 )
			{
				/* charge C5 (22u) over R3 (68k) and CR1 (1N914)
				 * time = 0.68 * C5 * R3 = 1017280us
				 */
				#define C5_CHARGE_TIME (int)(32767 / 1.01728);
				m_squeal_amp_counter -= C5_CHARGE_TIME;
				while( m_squeal_amp_counter <= 0 )
				{
					m_squeal_amp_counter += OUTPUT_RATE;
					if( ++m_squeal_amp == 32767 )
						break;
				}
			}

			if( m_squeal_out )
			{
				/* NE555 setup as pulse position modulator
				 * C = 0.01u, Ra = 33k, Rb = 47k
				 * frequency = 1.44 / ((33k + 2*47k) * 0.01u) = 1134Hz
				 * modulated by squeal_amp
				 */
				m_squeal_off_counter -= (1134 + 1134 * m_squeal_amp / 32767) / 3;
				while( m_squeal_off_counter <= 0 )
				{
					m_squeal_off_counter += OUTPUT_RATE;
					m_squeal_out = 0;
				}
			}
			else
			{
				m_squeal_on_counter -= 1134;
				while( m_squeal_on_counter <= 0 )
				{
					m_squeal_on_counter += OUTPUT_RATE;
					m_squeal_out = 1;
				}
			}
		}

		/* mix sequal sound at 40% */
		if( m_squeal_out )
			sum += 32767 * 40 / 100;

		*buffer = BURN_SND_CLIP(sum + *buffer++);
		*buffer = BURN_SND_CLIP(sum + *buffer++);
	}
}

void redbaron_sound_write(UINT8 data)
{
	if( data == m_latch )
		return;

//	m_channel->update();
	m_latch = data;
}



static UINT8 bzone_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x1820) {
		return pokey_read(0, address & 0x0f);
	}
	
	if ((address & 0xffe0) == 0x1860) {
		return 0; // reads a lot from here.. why? writes are mathbox_go_write
	}

	switch (address)
	{
		case 0x0800: {
			UINT8 ret = DrvInputs[0] ^ 0xff;
			ret &= ~0x10;
			ret |= DrvDips[2] & 0x10; // 0x10 as a test!
			ret &= ~0x40;
			ret |= (avgdvg_done() ? 0x40 : 0);
			ret &= ~0x80;
			ret |= (M6502TotalCycles() & 0x100) ? 0x80 : 0;
			return ret;
		}		
		
		case 0x0a00:
			return DrvDips[0];

		case 0x0c00:
			return DrvDips[1];

		case 0x1800:
			return mathbox_status_read();
			
		case 0x1808:
			return DrvInputs[2]; // bradley

		case 0x1809:
			return DrvInputs[3]; // bradley
			
		case 0x180a:
			return analog_data; // bradley

		case 0x1810:
			return mathbox_lo_read();

		case 0x1818:
			return mathbox_hi_read();
			
		case 0x1848:  // bradley (?)
		case 0x1849:
		case 0x184a:
			return 0; 
	}

	//bprintf (0, _T("R: %4.4x\n"), address);
	//bprintf (0, _T("Unmapped!!!!!!!!!\n"));
	return 0;
}

static void bzone_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x1820) {
		pokey_write(0, address & 0x0f, data);
		return;
	}

	if ((address & 0xffe0) == 0x1860) {
		mathbox_go_write(address & 0x1f, data);
		return;
	}

	switch (address)
	{
		case 0x1000: 
			// coin counter
		return;

		case 0x1200:
			avgdvg_go();
			avgletsgo = 1;
		return;

		case 0x1400:
			BurnWatchdogWrite();
		return;

		case 0x1600:
			avgdvg_reset();
		return;
		
		case 0x1808: // bradley (?)
		return;

		case 0x1840:
			// bzone_sounds_w
		return;
		
		case 0x1848: // bradley
		case 0x1849:
		case 0x184a:
		case 0x184b:
		case 0x184c:
		case 0x184d:
		case 0x184e:
		case 0x184f:
		case 0x1850:
			if (address <= 0x184a) analog_data = 0; // bradley-------attach to analog inputs!!
		return;
	}
	//if (address != 0x1400) bprintf (0, _T("W: %4.4x, %2.2x\n"), address,data);
	//bprintf (0, _T("Unmapped!!!!!!!!!\n"));
}

static UINT8 redbaron_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x1810) {
		return pokey_read(0, address & 0x0f);
	}

	if (address >= 0x1820 && address <= 0x185f) {
		return earom_read(address - 0x1820);
	}
	
	if ((address & 0xffe0) == 0x1860) {
		return 0; // reads a lot from here.. why? writes are mathbox_go_write
	}

	switch (address)
	{
		case 0x0800: {
			UINT8 ret = DrvInputs[0] ^ 0xff;
			ret &= ~0x10;
			ret |= DrvDips[2] & 0x10; // 0x10 as a test!
			ret &= ~0x40;
			ret |= (avgdvg_done() ? 0x40 : 0);
			ret &= ~0x80;
			ret |= (M6502TotalCycles() & 0x100) ? 0x80 : 0;
			return ret;
		}		
		
		case 0x0a00:
			return DrvDips[0];

		case 0x0c00:
			return DrvDips[1];

		case 0x1800:
			return mathbox_status_read();
			
		case 0x1802:
			return DrvInputs[2]; // in4

		case 0x1804:
			return mathbox_lo_read();

		case 0x1806:
			return mathbox_hi_read();
	}

	//bprintf (0, _T("R: %4.4x\n"), address);
	//bprintf (0, _T("Unmapped!!!!!!!!!\n"));
	return 0;
}

static void redbaron_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x1810) {
		pokey_write(0, address & 0x0f, data);
		return;
	}

	if (address >= 0x1820 && address <= 0x185f) {
		earom_write(address - 0x1820, data);
		return;
	}

	if ((address & 0xffe0) == 0x1860) {
		mathbox_go_write(address & 0x1f, data);
		return;
	}

	switch (address)
	{
		case 0x1000:
		return;

		case 0x1200:
			avgdvg_go();
			avgletsgo = 1;
		return;

		case 0x1400:
			BurnWatchdogWrite();
		return;

		case 0x1600:
			avgdvg_reset();
		return;

		case 0x1808:
			redbaron_sound_write(data);
			input_select = data & 1;
		return;

		case 0x180a:
		return; // nop
		
		case 0x180c:
			earom_ctrl_write(address, data);
		return;
	}

	//if (address != 0x1400) bprintf (0, _T("W: %4.4x, %2.2x\n"), address,data);
	//bprintf (0, _T("Unmapped!!!!!!!!!\n"));
}

static INT32 bzone_port0_read(INT32 /*offset*/)
{
	return DrvInputs[1];
}

static INT32 redbaron_port0_read(INT32 /*offset*/)
{
	static INT16 analog[2] = { DrvAnalogPort0, DrvAnalogPort1 };
	return ProcessAnalog(analog[input_select], 0, 1, 0x40, 0xc0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	M6502Reset();
	M6502Close();

	if (redbaron) {
		redbaron_sound_reset();
	}

	PokeyReset();

	BurnWatchdogReset();

	mathbox_reset();
	avgdvg_reset();
	
	earom_reset();

	avgletsgo = 0;
	analog_data = 0;
	nExtraCycles = 0;
	input_select = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x008000;

	DrvPalette		= (UINT32*)Next; Next += 0x20 * 256 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x000800;
	DrvVectorRAM	= Next; Next += 0x001000;

	RamEnd			= Next;

	DrvVectorROM	= Next; Next += 0x001000; // needs to be after DrvVectorRAM(!)

	MemEnd			= Next;

	return 0;
}

static INT32 BzoneInit()
{
	BurnSetRefreshRate(41.05);
	
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM  + 0x5000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x5800,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x6000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x6800,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x7000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x7800,  5, 1)) return 1;

		if (BurnLoadRom(DrvVectorROM + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x0800,  7, 1)) return 1;
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(DrvM6502RAM,		        0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,		    0x2000, 0x2fff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,            0x3000, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x4000,	0x4000, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(bzone_write);
	M6502SetReadHandler(bzone_read);
	M6502Close();
	
	earom_init(); // not used in bzone
	
	BurnWatchdogInit(DrvDoReset, -1);

	PokeyInit(12096000/8, 2, 2.40, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, bzone_port0_read);

	avgdvg_init(USE_AVG_BZONE, DrvVectorRAM, 0x5000, M6502TotalCycles, 580, 400);

	DrvDoReset(1);

	return 0;
}

static INT32 BradleyInit()
{
	BurnSetRefreshRate(41.05);
	
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM  + 0x4000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x4800,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x5000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x5800,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x6000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x6800,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x7000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x7800,  7, 1)) return 1;

		if (BurnLoadRom(DrvVectorROM + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x0800,  9, 1)) return 1;
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(DrvM6502RAM,		        0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,		    0x2000, 0x2fff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,            0x3000, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x4000,	0x4000, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(bzone_write);
	M6502SetReadHandler(bzone_read);
	M6502Close();
	
	earom_init(); // not used in bzone
	
	BurnWatchdogInit(DrvDoReset, -1);

	PokeyInit(12096000/8, 2, 2.40, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, bzone_port0_read);

	avgdvg_init(USE_AVG_BZONE, DrvVectorRAM, 0x5000, M6502TotalCycles, 580, 400);

	DrvDoReset(1);

	return 0;
}

static INT32 RedbaronInit()
{
	BurnSetRefreshRate(41.05);
	
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM  + 0x4800,  0, 1)) return 1;
		memcpy (DrvM6502ROM + 0x5800, DrvM6502ROM + 0x5000, 0x0800);
		if (BurnLoadRom(DrvM6502ROM  + 0x5000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x6000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x6800,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x7000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM  + 0x7800,  5, 1)) return 1;

		if (BurnLoadRom(DrvVectorROM + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvVectorROM + 0x0800,  7, 1)) return 1;
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetAddressMask(0x7fff);
	M6502MapMemory(DrvM6502RAM,		        0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVectorRAM,		    0x2000, 0x2fff, MAP_RAM);
	M6502MapMemory(DrvVectorROM,            0x3000, 0x3fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x4000,	0x4000, 0x7fff, MAP_ROM);
	M6502SetWriteHandler(redbaron_write);
	M6502SetReadHandler(redbaron_read);
	M6502Close();
	
	earom_init();

	BurnWatchdogInit(DrvDoReset, -1); // why is this being triggered?

	PokeyInit(12096000/8, 2, 2.40, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeyAllPotCallback(0, redbaron_port0_read);

	redbaron_sound_init();

	avgdvg_init(USE_AVG_RBARON, DrvVectorRAM, 0x5000, M6502TotalCycles, 520, 400);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	avgdvg_exit();

	PokeyExit();
	if (redbaron) {
		redbaron_sound_exit();
		redbaron = 0;
	}
	M6502Exit();

	earom_exit();
	
	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
    for (INT32 i = 0; i < 0x20; i++) // color
	{		
		for (INT32 j = 0; j < 256; j++) // intensity
		{
			INT32 c = (0xff * j) / 0xff;

			DrvPalette[i * 256 + j] = (c << 16) | (c << 8) | c; // must be 32bit palette! -dink (see vector.cpp)
		}
	}
}

static INT32 DrvDraw()
{
	DrvPaletteInit();

	draw_vector(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();
	
	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0, 5);
		
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}

	}

	INT32 nCyclesTotal = 1512000 / 41;
	INT32 nInterleave = 20;
	INT32 nCyclesDone = nExtraCycles;
	INT32 nSoundBufferPos = 0;

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone += M6502Run((nCyclesTotal * (i + 1) / nInterleave) - nCyclesDone);
		if ((i % 5) == 4 && (DrvDips[2] & 0x10)) {
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		}

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			pokey_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	nExtraCycles = nCyclesDone - nCyclesTotal;

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			pokey_update(pSoundBuf, nSegmentLength);
		}
		
		if (redbaron) {
			redbaron_sound_update(pBurnSoundOut, nBurnSoundLen);
		}
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	M6502Close();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		BurnWatchdogScan(nAction);

		pokey_scan(nAction, pnMin);

		SCAN_VAR(nExtraCycles);
		SCAN_VAR(avgletsgo);
		SCAN_VAR(analog_data);
		SCAN_VAR(input_select);
	}

	earom_scan(nAction, pnMin); // here.
	
	return 0;
}


// Battle Zone (rev 2)

static struct BurnRomInfo bzoneRomDesc[] = {
	{ "036414-02.e1",	0x0800, 0x13de36d5, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "036413-01.h1",	0x0800, 0x5d9d9111, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "036412-01.j1",	0x0800, 0xab55cbd2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "036411-01.k1",	0x0800, 0xad281297, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "036410-01.lm1",	0x0800, 0x0b7bfaa4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "036409-01.n1",	0x0800, 0x1e14e919, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "036422-01.bc3",	0x0800, 0x7414177b, 2 | BRF_PRG | BRF_ESS }, //  6 Vector Data
	{ "036421-01.a3",	0x0800, 0x8ea8f939, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "036408-01.k7",	0x0100, 0x5903af03, 3 | BRF_GRA },           //  8 AVG PROM

	{ "036174-01.b1",	0x0020, 0x8b04f921, 4 | BRF_GRA },           //  9 Mathbox PROM

	{ "036175-01.m1",	0x0100, 0x2af82e87, 5 | BRF_GRA },           // 10 user3
	{ "036176-01.l1",	0x0100, 0xb31f6e24, 5 | BRF_GRA },           // 11
	{ "036177-01.k1",	0x0100, 0x8119b847, 5 | BRF_GRA },           // 12
	{ "036178-01.j1",	0x0100, 0x09f5a4d5, 5 | BRF_GRA },           // 13
	{ "036179-01.h1",	0x0100, 0x823b61ae, 5 | BRF_GRA },           // 14
	{ "036180-01.f1",	0x0100, 0x276eadd5, 5 | BRF_GRA },           // 15
};

STD_ROM_PICK(bzone)
STD_ROM_FN(bzone)

struct BurnDriver BurnDrvBzone = {
	"bzone", NULL, NULL, NULL, "1980",
	"Battle Zone (rev 2)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, bzoneRomInfo, bzoneRomName, NULL, NULL, BzoneInputInfo, BzoneDIPInfo,
	BzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	580, 400, 4, 3
};


// Battle Zone (rev 1)

static struct BurnRomInfo bzoneaRomDesc[] = {
	{ "036414-01.e1",	0x0800, 0xefbc3fa0, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "036413-01.h1",	0x0800, 0x5d9d9111, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "036412-01.j1",	0x0800, 0xab55cbd2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "036411-01.k1",	0x0800, 0xad281297, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "036410-01.lm1",	0x0800, 0x0b7bfaa4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "036409-01.n1",	0x0800, 0x1e14e919, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "036422-01.bc3",	0x0800, 0x7414177b, 1 | BRF_PRG | BRF_ESS }, //  6 Vector Data
	{ "036421-01.a3",	0x0800, 0x8ea8f939, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "036408-01.k7",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  8 AVG PROM

	{ "036174-01.b1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           //  9 Mathbox PROM

	{ "036175-01.m1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 10 user3
	{ "036176-01.l1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 11
	{ "036177-01.k1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 12
	{ "036178-01.j1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 13
	{ "036179-01.h1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 14
	{ "036180-01.f1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 15
};

STD_ROM_PICK(bzonea)
STD_ROM_FN(bzonea)

struct BurnDriver BurnDrvBzonea = {
	"bzonea", "bzone", NULL, NULL, "1980",
	"Battle Zone (rev 1)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, bzoneaRomInfo, bzoneaRomName, NULL, NULL, BzoneInputInfo, BzoneDIPInfo,
	BzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	580, 400, 4, 3
};


// Battle Zone (cocktail)

static struct BurnRomInfo bzonecRomDesc[] = {
	{ "bz1g4800",		0x0800, 0xe228dd64, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "bz1f5000",		0x0800, 0xdddfac9a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bz1e5800",		0x0800, 0x7e00e823, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bz1d6000",		0x0800, 0xc0f8c068, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bz1c6800",		0x0800, 0x5adc64bd, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "bz1b7000",		0x0800, 0xed8a860e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "bz1a7800",		0x0800, 0x04babf45, 1 | BRF_PRG | BRF_ESS }, //  6
	
	{ "036422-01.bc3",	0x0800, 0x7414177b, 1 | BRF_PRG | BRF_ESS }, //  7 Vector Data
	{ "bz3b3800",		0x0800, 0x76cf57f6, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "036408-01.k7",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  9 AVG PROM

	{ "036174-01.b1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 10 Mathbox PROM

	{ "036175-01.m1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 11 user3
	{ "036176-01.l1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 12
	{ "036177-01.k1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 13
	{ "036178-01.j1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 14
	{ "036179-01.h1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 15
	{ "036180-01.f1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 16
};

STD_ROM_PICK(bzonec)
STD_ROM_FN(bzonec)

struct BurnDriver BurnDrvBzonec = {
	"bzonec", "bzone", NULL, NULL, "1980",
	"Battle Zone (cocktail)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, bzonecRomInfo, bzonecRomName, NULL, NULL, BzoneInputInfo, BzoneDIPInfo,
	BzoneInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	580, 400, 4, 3
};


// Bradley Trainer

static struct BurnRomInfo bradleyRomDesc[] = {
	{ "btc1.bin",		0x0800, 0x0bb8e049, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "btd1.bin",		0x0800, 0x9e0566d4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bte1.bin",		0x0800, 0x64ee6a42, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bth1.bin",		0x0800, 0xbaab67be, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "btj1.bin",		0x0800, 0x036adde4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "btk1.bin",		0x0800, 0xf5c2904e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "btlm.bin",		0x0800, 0x7d0313bf, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "btn1.bin",		0x0800, 0x182c8c64, 1 | BRF_PRG | BRF_ESS }, //  7
	
	{ "btb3.bin",		0x0800, 0x88206304, 1 | BRF_PRG | BRF_ESS }, //  8 Vector Data
	{ "bta3.bin",		0x0800, 0xd669d796, 1 | BRF_PRG | BRF_ESS }, //  9

	{ "036408-01.k7",	0x0100, 0x5903af03, 2 | BRF_GRA },           // 10 AVG PROM

	{ "036174-01.b1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 11 Mathbox PROM

	{ "036175-01.m1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 12 user3
	{ "036176-01.l1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 13
	{ "036177-01.k1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 14
	{ "036178-01.j1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 15
	{ "036179-01.h1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 16
	{ "036180-01.f1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 17
};

STD_ROM_PICK(bradley)
STD_ROM_FN(bradley)

struct BurnDriver BurnDrvBradley = {
	"bradley", NULL, NULL, NULL, "1980",
	"Bradley Trainer\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, bradleyRomInfo, bradleyRomName, NULL, NULL, BradleyInputInfo, BradleyDIPInfo,
	BradleyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	580, 400, 4, 3
};


// Red Baron (Revised Hardware)

static struct BurnRomInfo redbaronRomDesc[] = {
	{ "037587-01.fh1",	0x1000, 0x60f23983, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "037000-01.e1",	0x0800, 0x69bed808, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "036998-01.j1",	0x0800, 0xd1104dd7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "036997-01.k1",	0x0800, 0x7434acb4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "036996-01.lm1",	0x0800, 0xc0e7589e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "036995-01.n1",	0x0800, 0xad81d1da, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "037006-01.bc3",	0x0800, 0x9fcffea0, 1 | BRF_PRG | BRF_ESS }, //  6 Vector Data
	{ "037007-01.a3",	0x0800, 0x60250ede, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "036408-01.k7",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  8 AVG PROM

	{ "036174-01.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           //  9 user2

	{ "036175-01.e1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 10 Mathbox PROM
	{ "036176-01.f1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 11
	{ "036177-01.h1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 12
	{ "036178-01.j1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 13
	{ "036179-01.k1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 14
	{ "036180-01.l1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 15

	{ "036464-01.a5",	0x0020, 0x42875b18, 5 | BRF_GRA },           // 16 prom
};

STD_ROM_PICK(redbaron)
STD_ROM_FN(redbaron)

struct BurnDriver BurnDrvRedbaron = {
	"redbaron", NULL, NULL, NULL, "1980",
	"Red Baron (Revised Hardware)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, redbaronRomInfo, redbaronRomName, NULL, NULL, RedbaronInputInfo, RedbaronDIPInfo,
	RedbaronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	520, 400, 4, 3
};


// Red Baron

static struct BurnRomInfo redbaronaRomDesc[] = {
	{ "037001-01e.e1",	0x0800, 0xb9486a6a, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "037000-01e.fh1",	0x0800, 0x69bed808, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "036999-01e.j1",	0x0800, 0x48d49819, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "036998-01e.k1",	0x0800, 0xd1104dd7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "036997-01e.lm1",	0x0800, 0x7434acb4, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "036996-01e.n1",	0x0800, 0xc0e7589e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "036995-01e.p1",	0x0800, 0xad81d1da, 1 | BRF_PRG | BRF_ESS }, //  6
	
	{ "037006-01e.bc3",	0x0800, 0x9fcffea0, 1 | BRF_PRG | BRF_ESS }, //  7 Vector Data
	{ "037007-01e.a3",	0x0800, 0x60250ede, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "036408-01.k7",	0x0100, 0x5903af03, 2 | BRF_GRA },           //  9 AVG PROM

	{ "036174-01.a1",	0x0020, 0x8b04f921, 3 | BRF_GRA },           // 10 Mathbox PROM

	{ "036175-01.e1",	0x0100, 0x2af82e87, 4 | BRF_GRA },           // 11 user3
	{ "036176-01.f1",	0x0100, 0xb31f6e24, 4 | BRF_GRA },           // 12
	{ "036177-01.h1",	0x0100, 0x8119b847, 4 | BRF_GRA },           // 13
	{ "036178-01.j1",	0x0100, 0x09f5a4d5, 4 | BRF_GRA },           // 14
	{ "036179-01.k1",	0x0100, 0x823b61ae, 4 | BRF_GRA },           // 15
	{ "036180-01.l1",	0x0100, 0x276eadd5, 4 | BRF_GRA },           // 16

	{ "036464-01.a5",	0x0020, 0x42875b18, 5 | BRF_GRA },           // 17 Timing PROM
};

STD_ROM_PICK(redbarona)
STD_ROM_FN(redbarona)

struct BurnDriver BurnDrvRedbarona = {
	"redbarona", "redbaron", NULL, NULL, "1980",
	"Red Baron\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, redbaronaRomInfo, redbaronaRomName, NULL, NULL, RedbaronInputInfo, RedbaronDIPInfo,
	RedbaronInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	520, 400, 4, 3
};
