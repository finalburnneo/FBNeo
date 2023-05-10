// FinalBurn Neo Hyperstone-based game driver module
// Based on MAME driver by Angelo Salese, David Haywood, Pierpaolo Prazzoli, Tomasz Slanina

// mrkickera	- not working
// yorijori		- unemulated

#include "tiles_generic.h"
#include "e132xs_intf.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "qs1000.h"
#include "eeprom.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvQSROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM[2];
static UINT8 *DrvEEPROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvTileRAM;
static UINT8 *DrvNVRAM;

static UINT8 DrvRecalc;

static INT32 soundlatch;
static INT32 flipscreen;
static INT32 okibank;
static INT32 nvram_bank;
static INT32 protection_index;
static INT32 protection_which;

static INT32 sound_type = 0; // ym2151+msm6295, 1 = ym2151 + 2x msm6295, 2 = qs1000 Samples
static INT32 graphics_size = 0;
static INT32 sound_size[2] = { 0, 0 };
static INT32 palette_bit = 0; // (shift palette control word in ram)
static INT32 cpu_clock = 50000000;
static INT32 protection_data[2];

static UINT32 speedhack_address = ~0;
static UINT32 speedhack_pc = 0;
static void (*speedhack_callback)(UINT32);

static UINT8 DrvJoy1[32];
static UINT8 DrvJoy2[32];
static UINT8 DrvJoy3[32];
static UINT32 DrvInputs[3];
static UINT8 DrvReset;

static INT32 is_boongga = 0;

static INT16 DrvPaddle; // butt-smacking paddle (..not the usual DrvPaddle)
static UINT8 PaddleVal;

static INT32 nCyclesExtra;

static struct BurnInputInfo CommonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 4,	"diag"		},
};

STDINPUTINFO(Common)

static struct BurnInputInfo AohInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 16,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 17,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 22,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 23,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 5,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 20,	"diag"		},
};

STDINPUTINFO(Aoh)

static struct BurnInputInfo FinalgdrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 16,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 23,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 22,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 17,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 31,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 24,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 25,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 26,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 28,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 29,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 30,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 22,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 23,	"diag"		},
};

STDINPUTINFO(Finalgdr)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo BoonggabInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	A("P1 Paddle",		BIT_ANALOG_REL, &DrvPaddle,		"p1 z-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 6"	},
	{"P1 Button 7",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 7"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 4,	"diag"		},
};
#undef A
STDINPUTINFO(Boonggab)

static void vamphalf_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x0c0:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x140:
		case 0x144:
			BurnYM2151Write((address / 4) & 1, data & 0xff);
		return;

		case 0x240:
			flipscreen = data & 0x80;
		return;

		case 0x608:
			EEPROMWriteBit(data & 0x01);
			EEPROMSetCSLine((~data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE );
		return;
	}
}

static UINT32 vamphalf_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x0c0:
			return MSM6295Read(0);

		case 0x144:
			return BurnYM2151Read();

		case 0x1c0:
			return EEPROMRead();

		case 0x600:
			return DrvInputs[1];

		case 0x604:
			return DrvInputs[0];
	}

	return 0;
}

static void set_okibank(INT32 data)
{
	okibank = data & ((sound_size[0] / 0x20000) - 1);

	MSM6295SetBank(0, DrvSndROM[0] + (okibank * 0x20000), 0x20000, 0x3ffff);
}

static void coolmini_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x000: // mrkicker
			set_okibank(data);
		return;

		case 0x200:
			flipscreen = data & 1;
		return;

		case 0x308:
			EEPROMWriteBit(data & 0x01);
			EEPROMSetCSLine((~data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE );
		return;

		case 0x4c0:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x540:
		case 0x544:
			BurnYM2151Write((address / 4) & 1, data & 0xff);
		return;
	}
}

static UINT32 coolmini_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x300:
			return DrvInputs[1];

		case 0x304:
			return DrvInputs[0];

		case 0x0c0: // nop
			return 0;

		case 0x4c0:
			return MSM6295Read(0);

		case 0x544:
			return BurnYM2151Read();

		case 0x1c0:
			return 0; // nop

		case 0x7c0:
			return EEPROMRead();
	}

	return 0;
}

static void jmpbreak_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x0c0: // nop
		case 0x100:

		case 0x440:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x280:
			EEPROMWriteBit(data & 0x01);
			EEPROMSetCSLine((~data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x680:
		case 0x684:
			BurnYM2151Write((address / 2) & 1, data & 0xff);
		return;

		case 0x240:
			flipscreen = address & 2;
		return;

		case 0x608:
		return; // nop
	}
}

static UINT32 jmpbreak_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x0c0: // nop
			return 0;

		case 0x240:
			return DrvInputs[0];

		case 0x2c0:
			return EEPROMRead();

		case 0x440:
			return MSM6295Read(0);

		case 0x540:
			return DrvInputs[1];

		case 0x684:
			return BurnYM2151Read();
	}

	return 0;
}

static void suplup_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x020:
			EEPROMWriteBit(data & 0x01);
			EEPROMSetCSLine((~data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x080:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x0c0:
		case 0x0c4:
			BurnYM2151Write((address / 2) & 1, data & 0xff);
		return;

		case 0x240:
			flipscreen = address & 2;
		return;
	}
}

static UINT32 suplup_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x040:
			return DrvInputs[0];

		case 0x060:
			return DrvInputs[1];

		case 0x080:
			return MSM6295Read(0);

		case 0x0c4:
			return BurnYM2151Read();

		case 0x100:
			return EEPROMRead();
	}

	return 0;
}

static void mrdig_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x080:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x0c0:
		case 0x0c4:
			BurnYM2151Write((address / 2) & 1, data & 0xff);
		return;

		case 0x3c0:
			EEPROMWriteBit(data & 0x01);
			EEPROMSetCSLine((~data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;
	}
}

static UINT32 mrdig_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x500:
			return DrvInputs[0];

		case 0x280:
			return DrvInputs[1];

		case 0x080:
			return MSM6295Read(0);

		case 0x0c4:
			return BurnYM2151Read();

		case 0x180:
			return EEPROMRead();
	}

	return 0;
}

static void worldadv_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x180:
			EEPROMWriteBit(data & 0x01);
			EEPROMSetCSLine((~data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x640:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x700:
		case 0x704:
			BurnYM2151Write((address / 2) & 1, data & 0xff);
		return;
	}
}

static UINT32 worldadv_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x280:
			return DrvInputs[0];

		case 0x340:
			return DrvInputs[1];

		case 0x640:
			return MSM6295Read(0);

		case 0x704:
			return BurnYM2151Read();

		case 0x780:
			return EEPROMRead();
	}

	return 0;
}

static void boonggab_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x300:
			flipscreen = data & 1;
		return;

		case 0x408:
			EEPROMWriteBit(data & 0x01);
			EEPROMSetCSLine((~data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x410:
			// prize
		return;

		case 0x414:
		case 0x418:
			// lamps
		return;

		case 0x600:
			set_okibank(data);
		return;

		case 0x700:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x740:
		case 0x744:
			BurnYM2151Write((address / 2) & 1, data & 0xff);
		return;
	}
}

static UINT32 boonggab_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x0c0:
			return EEPROMRead();

		case 0x400:
			return DrvInputs[1];

		case 0x404:
			return DrvInputs[0];

		case 0x700:
			return MSM6295Read(0);

		case 0x740:
		case 0x744:
			return BurnYM2151Read();
	}

	return 0xffffffff;
}

static inline void DrvMCUSync()
{
	INT32 todo = ((double)E132XSTotalCycles() * (24000000/12) / cpu_clock) - mcs51TotalCycles();

	if (todo > 0) mcs51Run(todo);
}

static void misncrft_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x100:
			flipscreen = data & 1;
		return;

		case 0x3c0:
			EEPROMWriteBit(data & 0x01);
			EEPROMSetCSLine((~data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE );
		return;

		case 0x400:
			DrvMCUSync();
			soundlatch = data;
			qs1000_set_irq(1);
		return;
	}
}

static UINT32 misncrft_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x200:
			return DrvInputs[0];

		case 0x240:
			return DrvInputs[1];

		case 0x580:
			return EEPROMRead();
	}

	return 0;
}

static void aoh_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x480:
			EEPROMWriteBit(data & 0x01);
			EEPROMSetCSLine((~data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x620:
			MSM6295Write(0, (data >> 8) & 0xff);
		return;

		case 0x660:
			MSM6295Write(1, (data >> 8) & 0xff);
		return;

		case 0x640:
		case 0x644:
			BurnYM2151Write((address / 4) & 1, (data >> 8) & 0xff);
		return;

		case 0x680:
			set_okibank(data);
		return;
	}
}

static UINT32 aoh_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x620:
			return MSM6295Read(0) << 8;

		case 0x660:
			return MSM6295Read(1) << 8;

		case 0x640:
		case 0x644:
			return BurnYM2151Read() << 8;
	}

	return 0;
}

static void wyvernwg_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x1800:
			protection_which = data & 1;
			protection_index = 8;
		return;

		case 0x2000:
			flipscreen = data & 1;
		return;

		case 0x5400:
			DrvMCUSync();
			soundlatch = data & 0xff;
			qs1000_set_irq(1);
		return;

		case 0x7000:
			EEPROMWriteBit(data & 0x01);
			EEPROMSetCSLine((~data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;
	}
}

static UINT32 wyvernwg_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x1800:
		{
			protection_index--;
			return (protection_data[protection_which] >> protection_index) & 1;
		}

		case 0x2800:
			return DrvInputs[0];

		case 0x3000:
			return DrvInputs[1];

		case 0x7c00:
			return EEPROMRead();
	}

	return 0;
}

static void mrkickera_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x4000:
			EEPROMWriteBit(data & 0x04000);
			EEPROMSetCSLine((~data & 0x01000) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x02000) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x4040:
			protection_which = (data == 0x41c6 || data == 0x446b) ? 0 : 1;
			protection_index = 8;
		return;

		case 0x40a0:
			set_okibank(data);
		return;

		case 0x7000:
		case 0x7004:
			BurnYM2151Write((address / 4) & 1, (data >> 8) & 0xff);
		return;

		case 0x7400:
			MSM6295Write(0, (data >> 8) & 0xff);
		return;
	}
}

static UINT32 mrkickera_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x2400:
			return EEPROMRead();

		case 0x6400:
		{
			protection_index--;
			return ((protection_data[protection_which] >> protection_index) & 1) ? 0x80008000 : 0;
		}

		case 0x7000:
		case 0x7004:
		{
			UINT32 ret = BurnYM2151Read();
			return ret | (ret << 8) | (ret << 16) | (ret << 24);
		}
		case 0x7400:
		{
			UINT32 ret = MSM6295Read(0);
			return ret | (ret << 8) | (ret << 16) | (ret << 24);
		}

		case 0x7800:
			return DrvInputs[0];

		case 0x7c00:
			return DrvInputs[1];
	}

	return 0;
}

static void finalgdr_io_write(UINT32 address, UINT32 data)
{
	if ((address & 0x7e00) == 0x2c00) {
		DrvNVRAM[((address / 4) & 0x7f) + (nvram_bank * 0x80)] = data >> 24;
		return;
	}

	switch (address)
	{
		case 0x2800:
			nvram_bank = (data >> 24);
		return;

		case 0x3000:
		case 0x3004:
			BurnYM2151Write((address / 4) & 1, (data >> 8) & 0xff);
		return;

		case 0x3400:
			MSM6295Write(0, (data >> 8) & 0xff);
		return;

		case 0x6000:
			EEPROMWriteBit(data & 0x01);
			EEPROMSetCSLine((~data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x6040:
			protection_which = (data == 0x41c6 || data == 0x446b) ? 0 : 1;
			protection_index = 8;
		return;

		case 0x6060:
			// prize?
		return;

		case 0x60a0:
			set_okibank(data);
		return;
	}
}

static UINT32 finalgdr_io_read(UINT32 address)
{
	if ((address & 0x7e00) == 0x2c00) {
		return DrvNVRAM[((address / 4) & 0x7f) + (nvram_bank * 0x80)] << 24;
	}

	switch (address)
	{
		case 0x2400:
		{
			protection_index--;
			return ((protection_data[protection_which] >> protection_index) & 1) ? 0x80008000 : 0;
		}

		case 0x3000:
		case 0x3004:
			return BurnYM2151Read() << 8;

		case 0x3400:
			return MSM6295Read(0) << 8;

		case 0x3800:
			return DrvInputs[0];

		case 0x3c00:
			return DrvInputs[1];

		case 0x4400:
			return EEPROMRead();
	}

	return 0;
}

static inline void do_speedhack(UINT32 address)
{
	if (address == speedhack_address) {
		if (E132XSGetPC(0) == speedhack_pc) {
			if (E132XSInterruptActive()) {
				E132XSRunEndBurnAllCycles();
			} else {
				E132XSBurnCycles(50);
			}
		}
	}
}

static UINT32 common_read_long(UINT32 address)
{
	if (address < 0x400000) {
		speedhack_callback(address);
		UINT32 ret = *((UINT32*)(DrvMainRAM + address));
		return BURN_ENDIAN_SWAP_INT32((ret << 16) | (ret >> 16));
	}

	switch (address)	// aoh
	{
		case 0x80210000:
		{
			UINT32 ret = DrvInputs[1] & ~0x10;
			if (EEPROMRead()) ret |= 0x10;
			return ret;
		}

		case 0x80220000:
			return DrvInputs[0];
	}

	return 0;
}

static UINT16 common_read_word(UINT32 address)
{
	if (address < 0x400000) {
		speedhack_callback(address);
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvMainRAM + address)));
	}

	switch (address & ~3)	// aoh
	{
		case 0x80210000:
		{
			UINT32 ret = DrvInputs[1] & ~0x10;
			if (EEPROMRead()) ret |= 0x10;
			if (~address & 2) return ret >> 16; // ?
			return ret;
		}

		case 0x80220000:
		{
			UINT32 ret = DrvInputs[0];
			if (~address & 2) return ret >> 16;
			return ret;
		}

	}

	return 0;
}

static UINT8 common_read_byte(UINT32 address)
{
	if (address < 0x400000) {
		speedhack_callback(address);
		return DrvMainRAM[address^1];
	}

	return 0;
}

static UINT8 qs1000_p1_read()
{
	return soundlatch & 0xff;
}

static void qs1000_p3_write(UINT8 data)
{
	if (~data & 0x20) qs1000_set_irq(0);

	qs1000_set_bankedrom(DrvQSROM + (data & 7) * 0x7f00);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	E132XSOpen(0);
	E132XSReset();
	E132XSClose();

	switch (sound_type)
	{
		case 0: // ym2151 + 1x msm
		case 1: // ym2151 + 2x msm
			MSM6295Reset();
			BurnYM2151Reset();
			set_okibank(1); // default to first for non-banked games
		break;

		case 2: // qs1000
			qs1000_reset();
		break;
	}

	EEPROMReset();
	if (EEPROMAvailable() == 0) {
		EEPROMFill(DrvEEPROM, 0, 0x80);
	}

	soundlatch = 0;
	flipscreen = 0;

	protection_index = 8;
	protection_which = 0;
	nvram_bank = 1;

	nCyclesExtra = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x400000;

	DrvQSROM		= Next; Next += 0x080000;

	DrvGfxROM		= Next; Next += graphics_size;

	DrvSndROM[0]	= Next; Next += sound_size[0];
	DrvSndROM[1]	= Next; Next += sound_size[1];

	BurnPalette		= (UINT32*)Next; Next += 0x8000 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000080 * 0x100;
	DrvEEPROM		= Next; Next += 0x000080;

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x400000;
	BurnPalRAM		= Next; Next += 0x010000;
	DrvTileRAM		= Next; Next += 0x040000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoadRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *gLoad = DrvGfxROM;
	UINT8 *qLoad = DrvQSROM;
	UINT8 *sLoad[2] = { DrvSndROM[0], DrvSndROM[1] };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 1) {
			if (bLoad) {
				memmove (DrvMainROM, DrvMainROM + ri.nLen, 0x400000 - ri.nLen);
				if (BurnLoadRom(DrvMainROM + 0x400000 - ri.nLen, i, 1)) return 1;
			}
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 2) {
			if (bLoad) {
				if (BurnLoadRomExt(gLoad + 0, i + 0, 4, LD_GROUP(2))) return 1;
				if (BurnLoadRomExt(gLoad + 2, i + 1, 4, LD_GROUP(2))) return 1;
			}
			gLoad += ri.nLen * 2;
			i++;
			continue;
		}

		if ((ri.nType & BRF_SND) && ((ri.nType & 0x0f) == 3 || (ri.nType & 0x0f) == 6)) {
			if (bLoad) {
				if (BurnLoadRom(sLoad[0], i, 1)) return 1;
			}
			sLoad[0] += ((ri.nType & 0x0f) == 6) ? 0x200000 : ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_SND) && (ri.nType & 0x0f) == 4) {
			if (bLoad) {
				if (BurnLoadRom(sLoad[1], i, 1)) return 1;
			}
			sLoad[1] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 8) {
			if (bLoad) {
				if (BurnLoadRom(qLoad, i, 1)) return 1;
			}
			qLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 9) {
			if (bLoad) {
				if (BurnLoadRom(DrvEEPROM, i, 1)) return 1;
			}
			continue;
		}
	}

	if (!bLoad) {
		graphics_size = gLoad - DrvGfxROM;
		sound_size[0] = sLoad[0] - DrvSndROM[0];
		sound_size[1] = sLoad[1] - DrvSndROM[1];
	} else {
		INT32 qlen = qLoad - DrvQSROM;
		if (qlen > 0) {
			for (INT32 i = qlen; i < 0x80000; i+=qlen, qLoad += qlen) {
				memcpy (qLoad, DrvQSROM, qlen);
			}
		}
	}

	return 0;
}

static void sound_type_0_init()
{
	BurnYM2151Init(3500000);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.45, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.45, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 1750000 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetBank(0, DrvSndROM[0], 0x00000, 0x3ffff);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	sound_type = 0;
}

static void sound_type_1_init() // aoh
{
	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295Init(1, 4000000 / MSM6295_PIN7_HIGH, 1);
	MSM6295SetBank(0, DrvSndROM[0], 0x00000, 0x3ffff);
	MSM6295SetBank(1, DrvSndROM[1], 0x00000, 0x3ffff);
	MSM6295SetRoute(0, 0.55, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.55, BURN_SND_ROUTE_BOTH);

	sound_type = 1;
}

static void sound_type_2_init()
{
	qs1000_init(DrvQSROM, DrvSndROM[0], sound_size[0]);
	qs1000_set_write_handler(3, qs1000_p3_write);
	qs1000_set_read_handler(1, qs1000_p1_read);
	qs1000_set_volume(4.00);
	sound_type = 2;
}

static INT32 CommonInit(INT32 cputype, void (*io_write)(UINT32,UINT32), UINT32 (*io_read)(UINT32), void (*sound_init)(), INT32 palbit, INT32 gfx_size_override)
{
	DrvLoadRoms(false);

	if (gfx_size_override > 0) graphics_size = gfx_size_override;

	BurnAllocMemIndex();

	if (DrvLoadRoms(true)) return 1;

	if (sound_init != sound_type_1_init) { // not aoh
		BurnByteswap(DrvMainROM, 0x400000);
		speedhack_callback = do_speedhack;
	}

	E132XSInit(0, cputype, cpu_clock);
	E132XSOpen(0);
	E132XSMapMemory(DrvMainRAM,			0x00000000, 0x003fffff, MAP_RAM); // 2x for AOH
	E132XSMapMemory(DrvTileRAM,			0x40000000, 0x4003ffff, MAP_RAM);
	E132XSMapMemory(BurnPalRAM,			0x80000000, 0x8000ffff, MAP_RAM);
	E132XSMapMemory(DrvMainROM,			0xffc00000, 0xffffffff, MAP_ROM); // variable-sized!
	E132XSSetReadLongHandler(common_read_long);
	E132XSSetReadWordHandler(common_read_word);
	E132XSSetReadByteHandler(common_read_byte);
	E132XSSetIOWriteHandler(io_write);
	E132XSSetIOReadHandler(io_read);

	if (speedhack_pc != 0)
	{
		E132XSMapMemory(NULL,			speedhack_address & ~0xfff, speedhack_address | 0xfff, MAP_READ);
	}

	E132XSClose();

	EEPROMInit(&eeprom_interface_93C46);
	EEPROMIgnoreErrMessage(1);

	sound_init();

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM, 8, 16, 16, graphics_size, 0, 0x7f);

	palette_bit = palbit;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	switch (sound_type)
	{
		case 0:
		case 1:
			BurnYM2151Exit();
			MSM6295Exit();
		break;

		case 2:
			qs1000_exit();
		break;
	}

	E132XSExit();
	EEPROMExit();

	BurnFreeMemIndex();

	MSM6295ROM = NULL;
	speedhack_address = ~0;
	speedhack_pc = 0;
	cpu_clock = 50000000;

	is_boongga = 0;

	return 0;
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvTileRAM;

	for (INT32 y = (16 & ~15); y <= ((256 - 1) | 15); y += 16)
	{
		GenericTilesSetClip(-1, -1, y-16, y+15+1-16); // +1 ?

		INT32 block = (flipscreen ? (y / 16) : (16 - (y / 16))) * 0x800;

		for (INT32 cnt = 0; cnt < 0x800; cnt += 8)
		{
			INT32 offs = (block + cnt) / 2;

			if (BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0x0100) continue;

			INT32 code  = BURN_ENDIAN_SWAP_INT16(ram[offs+1]) | ((BURN_ENDIAN_SWAP_INT16(ram[offs+2]) & 0x100) << 8);
			INT32 color = BURN_ENDIAN_SWAP_INT16(ram[offs+2]) >> palette_bit;
			INT32 sx    = BURN_ENDIAN_SWAP_INT16(ram[offs+3]) & 0x01ff;
			INT32 sy 	= 256 - (BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0x00ff);
			INT32 flipx = BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0x8000;
			INT32 flipy = BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0x4000;

			if (flipscreen)
			{
				flipx = !flipx;
				flipy = !flipy;
				sx = 366 - sx;
				sy = 256 - sy;
			}

			DrawGfxMaskTile(0, 0, code, sx - 31, sy-16, flipx, flipy, color, 0);
		}
	}
}

static void aoh_draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvTileRAM;

	for (INT32 y = (16 & ~15); y <= ((240 - 1) | 15); y += 16)
	{
		GenericTilesSetClip(-1, -1, y-16, y+15+1-16); // +1 ?

		INT32 block = (flipscreen ? (y / 16) : (16 - (y / 16))) * 0x800;

		for (INT32 cnt = 0; cnt < 0x800; cnt += 8)
		{
			INT32 offs = (block + cnt) / 2;

			INT32 code  = BURN_ENDIAN_SWAP_INT16(ram[offs+1]) | ((BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0x300) << 8);
			INT32 color = BURN_ENDIAN_SWAP_INT16(ram[offs+2]) >> palette_bit;
			INT32 sx    = BURN_ENDIAN_SWAP_INT16(ram[offs+3]) & 0x01ff;
			INT32 sy 	= 256 - (BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0x00ff);
			INT32 flipx = BURN_ENDIAN_SWAP_INT16(ram[offs]) & 0x0400;
			INT32 flipy = 0;

			if (flipscreen)
			{
				flipx = !flipx;
				flipy = !flipy;
				sx = 366 - sx;
				sy = 256 - sy;
			}

			DrawGfxMaskTile(0, 0, code, sx - 64, sy-16, flipx, flipy, color, 0);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_xRRRRRGGGGGBBBBB();
		DrvRecalc = 1; // force update
	}

	BurnTransferClear();

	draw_sprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 AohDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_xRRRRRGGGGGBBBBB();
		DrvRecalc = 1; // force update
	}

	BurnTransferClear();

	aoh_draw_sprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		DrvInputs[2] = 0x00;

		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			if (DrvJoy3[i]) DrvInputs[2] = i+1; // 1 - 7
		}

		if (is_boongga) {
			PaddleVal = ProcessAnalog(DrvPaddle, 0, INPUT_DEADZONE | INPUT_LINEAR, 0, 8);
			if (PaddleVal == 0) {
				PaddleVal = DrvInputs[2]; // maybe digital?
			}
			PaddleVal = 7 - PaddleVal;
			DrvInputs[0] &= ~0x3f00;
			DrvInputs[0] |= ((PaddleVal << 3) & 0x3f) << 8;
		}
	}

	E132XSNewFrame();
	if (sound_type == 2) mcs51NewFrame();

	INT32 nSegment;
	INT32 nInterleave = 10;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { cpu_clock / 59, 24000000 / 12 / 59 };
	INT32 nCyclesDone[2] = { nCyclesExtra, 0 };

	E132XSOpen(0);
	if (sound_type == 2) mcs51Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, E132XS);

		switch (sound_type)
		{
			case 0:
			case 1:
				if (pBurnSoundOut) {
					nSegment = nBurnSoundLen / nInterleave;
					BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
					nSoundBufferPos += nSegment;
				}
			break;

			case 2:
				CPU_RUN_SYNCINT(1, mcs51); // syncs to mcs51TotalCycles
			break;
		}
	}

	E132XSSetIRQLine(1, CPU_IRQSTATUS_HOLD);

	if (pBurnSoundOut)
	{
		switch (sound_type)
		{
			case 0:
			case 1:
				nSegment = nBurnSoundLen - nSoundBufferPos;
				if (nSegment > 0) {
					BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
				}
				MSM6295Render(pBurnSoundOut, nBurnSoundLen);
			break;

			case 2:
				if (pBurnSoundOut) {
					qs1000_update(pBurnSoundOut, nBurnSoundLen);
				}
			break;
		}
	}

	if (sound_type == 2) mcs51Close();
	E132XSClose();

	nCyclesExtra = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		E132XSScan(nAction);

		switch (sound_type)
		{
			case 0:
			case 1:
				BurnYM2151Scan(nAction, pnMin);
				MSM6295Scan(nAction, pnMin);
				SCAN_VAR(okibank);
			break;

			case 2:
				qs1000_scan(nAction, pnMin);
			break;
		}
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(okibank);
		SCAN_VAR(nvram_bank);
		SCAN_VAR(protection_index);
		SCAN_VAR(protection_which);

		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_WRITE) {
		switch (sound_type)
		{
			case 0:
				set_okibank(okibank);
			break;
		}
	}

	if (nAction & ACB_NVRAM)
	{
		EEPROMScan(nAction, pnMin);
	}

	return 0;
}


// Cool Minigame Collection

static struct BurnRomInfo coolminiRomDesc[] = {
	{ "cm-rom1",				0x080000, 0x9688fa98, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "cm-rom2",				0x080000, 0x9d588fef, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "roml00",					0x200000, 0x4b141f31, 2 | BRF_GRA },           //  2 Sprites
	{ "romu00",					0x200000, 0x9b2fb12a, 2 | BRF_GRA },           //  3
	{ "roml01",					0x200000, 0x1e3a04bb, 2 | BRF_GRA },           //  4
	{ "romu01",					0x200000, 0x06dd1a6c, 2 | BRF_GRA },           //  5
	{ "roml02",					0x200000, 0x1e8c12cb, 2 | BRF_GRA },           //  6
	{ "romu02",					0x200000, 0x4551d4fc, 2 | BRF_GRA },           //  7
	{ "roml03",					0x200000, 0x231650bf, 2 | BRF_GRA },           //  8
	{ "romu03",					0x200000, 0x273d5654, 2 | BRF_GRA },           //  9

	{ "cm-vrom1",				0x040000, 0xfcc28081, 3 | BRF_SND },           // 10 Samples
};

STD_ROM_PICK(coolmini)
STD_ROM_FN(coolmini)

static INT32 CoolminiInit()
{
	speedhack_address = 0xd2df8;
	speedhack_pc = 0x75f88;

	return CommonInit(TYPE_E116T, coolmini_io_write, coolmini_io_read, sound_type_0_init, 0, 0);
}

struct BurnDriver BurnDrvCoolmini = {
	"coolmini", NULL, NULL, NULL, "1999",
	"Cool Minigame Collection\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MINIGAMES, 0,
	NULL, coolminiRomInfo, coolminiRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	CoolminiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Cool Minigame Collection (Italy)

static struct BurnRomInfo coolminiiRomDesc[] = {
	{ "cm-rom1.040",			0x080000, 0xaa94bb86, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "cm-rom2.040",			0x080000, 0xbe7d02c8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "roml00",					0x200000, 0x4b141f31, 2 | BRF_GRA },           //  2 Sprites
	{ "romu00",					0x200000, 0x9b2fb12a, 2 | BRF_GRA },           //  3
	{ "roml01",					0x200000, 0x1e3a04bb, 2 | BRF_GRA },           //  4
	{ "romu01",					0x200000, 0x06dd1a6c, 2 | BRF_GRA },           //  5
	{ "roml02",					0x200000, 0x1e8c12cb, 2 | BRF_GRA },           //  6
	{ "romu02",					0x200000, 0x4551d4fc, 2 | BRF_GRA },           //  7
	{ "roml03.l03",				0x200000, 0x30a7fe2f, 2 | BRF_GRA },           //  8
	{ "romu03.u03",				0x200000, 0xeb7c943d, 2 | BRF_GRA },           //  9

	{ "cm-vrom1",				0x040000, 0xfcc28081, 3 | BRF_SND },           // 10 Samples
};

STD_ROM_PICK(coolminii)
STD_ROM_FN(coolminii)

static INT32 CoolminiiInit()
{
	speedhack_address = 0xd30a8;
	speedhack_pc = 0x76024;

	return CommonInit(TYPE_E116T, coolmini_io_write, coolmini_io_read, sound_type_0_init, 0, 0);
}

struct BurnDriver BurnDrvCoolminii = {
	"coolminii", "coolmini", NULL, NULL, "1999",
	"Cool Minigame Collection (Italy)\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MINIGAMES, 0,
	NULL, coolminiiRomInfo, coolminiiRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	CoolminiiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Jumping Break (set 1)
/* Released February 1999 */

static struct BurnRomInfo jmpbreakRomDesc[] = {
	{ "rom1.bin",				0x080000, 0x7e237f7d, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "rom2.bin",				0x080000, 0xc722f7be, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "roml00.bin",				0x200000, 0x4b99190a, 2 | BRF_GRA },           //  2 Sprites
	{ "romu00.bin",				0x200000, 0xe93762f8, 2 | BRF_GRA },           //  3
	{ "roml01.bin",				0x200000, 0x6796a104, 2 | BRF_GRA },           //  4
	{ "romu01.bin",				0x200000, 0x0cc907c8, 2 | BRF_GRA },           //  5

	{ "vrom1.bin",				0x040000, 0x1b6e3671, 3 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(jmpbreak)
STD_ROM_FN(jmpbreak)

static INT32 JmpbreakInit()
{
	speedhack_address = 0x906f4;
	speedhack_pc = 0x984a;

	return CommonInit(TYPE_E116T, jmpbreak_io_write, jmpbreak_io_read, sound_type_0_init, 0, 0);
}

struct BurnDriver BurnDrvJmpbreak = {
	"jmpbreak", NULL, NULL, NULL, "1999",
	"Jumping Break (set 1)\0", NULL, "F2 System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_BREAKOUT, 0,
	NULL, jmpbreakRomInfo, jmpbreakRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	JmpbreakInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Jumping Break (set 2)
// PCB has a New Impeuropex sticker, so sold in the Italian market. There also an hand-written IMP 28.04.99

static struct BurnRomInfo jmpbreakaRomDesc[] = {
	{ "2.rom1",					0x080000, 0x553af133, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "3.rom2",					0x080000, 0xbd0a5eed, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "roml00.bin",				0x200000, 0x4b99190a, 2 | BRF_GRA },           //  2 Sprites
	{ "romu00.bin",				0x200000, 0xe93762f8, 2 | BRF_GRA },           //  3
	{ "roml01.bin",				0x200000, 0x6796a104, 2 | BRF_GRA },           //  4
	{ "romu01.bin",				0x200000, 0x0cc907c8, 2 | BRF_GRA },           //  5

	{ "1.vrom1",				0x040000, 0x1b6e3671, 3 | BRF_SND },           //  6 Samples
	
	{ "palce22v10h.gal1",		0x0002dd, 0x0ff86470, 0 | BRF_OPT },		   //  7 PLDs
};

STD_ROM_PICK(jmpbreaka)
STD_ROM_FN(jmpbreaka)

static INT32 JmpbreakaInit()
{
	speedhack_address = 0xe1dfc;
	speedhack_pc = 0x909ac;

	return CommonInit(TYPE_E116T, jmpbreak_io_write, jmpbreak_io_read, sound_type_0_init, 0, 0);
}

struct BurnDriver BurnDrvJmpbreaka = {
	"jmpbreaka", "jmpbreak", NULL, NULL, "1999",
	"Jumping Break (set 2)\0", NULL, "F2 System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_BREAKOUT, 0,
	NULL, jmpbreakaRomInfo, jmpbreakaRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	JmpbreakaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Poosho Poosho

static struct BurnRomInfo pooshoRomDesc[] = {
	{ "rom1.bin",				0x080000, 0x2072c120, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "rom2.bin",				0x080000, 0x80e70d7a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "roml00.bin",				0x200000, 0x9efb0673, 2 | BRF_GRA },           //  2 Sprites
	{ "romu00.bin",				0x200000, 0xfe1d6a02, 2 | BRF_GRA },           //  3
	{ "roml01.bin",				0x200000, 0x05e81ca0, 2 | BRF_GRA },           //  4
	{ "romu01.bin",				0x200000, 0xfd2d02c7, 2 | BRF_GRA },           //  5

	{ "vrom1.bin",				0x040000, 0x1b6e3671, 3 | BRF_SND },           //  6 Samples

	{ "gal1.bin",				0x0002e5, 0x90352c93, 0 | BRF_OPT },           //  7 PLDs
};

STD_ROM_PICK(poosho)
STD_ROM_FN(poosho)

static INT32 PooshoInit()
{
	speedhack_address = 0xc8b58;
	speedhack_pc = 0xa8c78;

	return CommonInit(TYPE_E116T, jmpbreak_io_write, jmpbreak_io_read, sound_type_0_init, 0, 0);
}

struct BurnDriver BurnDrvPoosho = {
	"poosho", NULL, NULL, NULL, "1999",
	"Poosho Poosho\0", NULL, "F2 System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_BREAKOUT, 0,
	NULL, pooshoRomInfo, pooshoRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	PooshoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// New Cross Pang (set 1)

static struct BurnRomInfo newxpangRomDesc[] = {
	{ "rom2.bin",				0x080000, 0x6d69c799, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code

	{ "roml00.bin",				0x200000, 0x4f8253d3, 2 | BRF_GRA },           //  1 Sprites
	{ "romu00.bin",				0x200000, 0x0ac8f8e4, 2 | BRF_GRA },           //  2
	{ "roml01.bin",				0x200000, 0x66e6e05e, 2 | BRF_GRA },           //  3
	{ "romu01.bin",				0x200000, 0x73907b33, 2 | BRF_GRA },           //  4

	{ "vrom1.bin",				0x040000, 0x0f339d68, 3 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(newxpang)
STD_ROM_FN(newxpang)

static INT32 NewxpangInit()
{
	speedhack_address = 0x61218;
	speedhack_pc = 0x8b8e;

	return CommonInit(TYPE_E116T, mrdig_io_write, mrdig_io_read, sound_type_0_init, 0, 0);
}

struct BurnDriver BurnDrvNewxpang = {
	"newxpang", NULL, NULL, NULL, "1999",
	"New Cross Pang (set 1)\0", NULL, "F2 System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, newxpangRomInfo, newxpangRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	NewxpangInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// New Cross Pang (set 2)

static struct BurnRomInfo newxpangaRomDesc[] = {
	{ "rom2.bin",				0x080000, 0x325c2c4f, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code

	{ "roml00.bin",				0x200000, 0x4f8253d3, 2 | BRF_GRA },           //  1 Sprites
	{ "romu00.bin",				0x200000, 0x0ac8f8e4, 2 | BRF_GRA },           //  2
	{ "roml01.bin",				0x200000, 0x66e6e05e, 2 | BRF_GRA },           //  3
	{ "romu01.bin",				0x200000, 0x73907b33, 2 | BRF_GRA },           //  4

	{ "vrom1.bin",				0x040000, 0x0f339d68, 3 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(newxpanga)
STD_ROM_FN(newxpanga)

static INT32 NewxpangaInit()
{
//	speedhack_address = 0x906f4;
//	speedhack_pc = 0x984a;

	return CommonInit(TYPE_E116T, jmpbreak_io_write, jmpbreak_io_read, sound_type_0_init, 0, 0);
}

struct BurnDriver BurnDrvNewxpanga = {
	"newxpanga", "newxpang", NULL, NULL, "1999",
	"New Cross Pang (set 2)\0", NULL, "F2 System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, newxpangaRomInfo, newxpangaRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	NewxpangaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// World Adventure

static struct BurnRomInfo worldadvRomDesc[] = {
	{ "rom1.bin",				0x080000, 0x1855c235, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "rom2.bin",				0x080000, 0x671ddbb0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "roml00.bin",				0x200000, 0xfe422890, 2 | BRF_GRA },           //  2 Sprites
	{ "romu00.bin",				0x200000, 0xdd1066f5, 2 | BRF_GRA },           //  3
	{ "roml01.bin",				0x200000, 0x9ab76649, 2 | BRF_GRA },           //  4
	{ "romu01.bin",				0x200000, 0x62132228, 2 | BRF_GRA },           //  5

	{ "vrom1.bin",				0x040000, 0xc87cce3b, 3 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(worldadv)
STD_ROM_FN(worldadv)

static INT32 WorldadvInit()
{
	speedhack_address = 0xc5e78;
	speedhack_pc = 0x93ae;

	return CommonInit(TYPE_E116T, worldadv_io_write, worldadv_io_read, sound_type_0_init, 0, 0);
}

struct BurnDriver BurnDrvWorldadv = {
	"worldadv", NULL, NULL, NULL, "1999",
	"World Adventure\0", NULL, "Logic / F2 System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, worldadvRomInfo, worldadvRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	WorldadvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Super Lup Lup Puzzle / Zhuan Zhuan Puzzle (version 4.0 / 990518)

static struct BurnRomInfo suplupRomDesc[] = {
	{ "suplup-rom1.bin",		0x080000, 0x61fb2dbe, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "suplup-rom2.bin",		0x080000, 0x0c176c57, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "suplup-roml00.bin",		0x200000, 0x7848e183, 2 | BRF_GRA },           //  2 Sprites
	{ "suplup-romu00.bin",		0x200000, 0x13e3ab7f, 2 | BRF_GRA },           //  3
	{ "suplup-roml01.bin",		0x200000, 0x15769f55, 2 | BRF_GRA },           //  4
	{ "suplup-romu01.bin",		0x200000, 0x6687bc6f, 2 | BRF_GRA },           //  5

	{ "vrom1.bin",				0x040000, 0x34a56987, 3 | BRF_SND },           //  6 Samples

	{ "eeprom-suplup.bin",		0x000080, 0xe60c9883, 9 | BRF_PRG | BRF_ESS }, //  7 Default EEPROM
};

STD_ROM_PICK(suplup)
STD_ROM_FN(suplup)

static INT32 SuplupInit()
{
	speedhack_address = 0x11605c;
	speedhack_pc = 0xaf184;

	return CommonInit(TYPE_E116T, suplup_io_write, suplup_io_read, sound_type_0_init, 8, 0);
}

struct BurnDriver BurnDrvSuplup = {
	"suplup", NULL, NULL, NULL, "1999",
	"Super Lup Lup Puzzle / Zhuan Zhuan Puzzle (version 4.0 / 990518)\0", NULL, "Omega System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, suplupRomInfo, suplupRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	SuplupInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Lup Lup Puzzle / Zhuan Zhuan Puzzle (version 3.0 / 990128)

static struct BurnRomInfo luplupRomDesc[] = {
	{ "luplup-rom1.v30",		0x080000, 0x9ea67f87, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "luplup-rom2.v30",		0x080000, 0x99840155, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "luplup-roml00",			0x200000, 0x08b2aa75, 2 | BRF_GRA },           //  2 Sprites
	{ "luplup-romu00",			0x200000, 0xb57f4ca5, 2 | BRF_GRA },           //  3
	{ "luplup30-roml01",		0x200000, 0x40e85f94, 2 | BRF_GRA },           //  4
	{ "luplup30-romu01",		0x200000, 0xf2645b78, 2 | BRF_GRA },           //  5

	{ "vrom1.bin",				0x040000, 0x34a56987, 3 | BRF_SND },           //  6 Samples

	{ "gal22v10b.gal1",			0x0002e5, 0x00000000, 0 | BRF_NODUMP | BRF_OPT },           //  7 PLDs
};

STD_ROM_PICK(luplup)
STD_ROM_FN(luplup)

static INT32 LuplupInit()
{
	speedhack_address = 0x115e84;
	speedhack_pc = 0xaefac;

	return CommonInit(TYPE_E116T, suplup_io_write, suplup_io_read, sound_type_0_init, 8, 0);
}

struct BurnDriver BurnDrvLuplup = {
	"luplup", "suplup", NULL, NULL, "1999",
	"Lup Lup Puzzle / Zhuan Zhuan Puzzle (version 3.0 / 990128)\0", NULL, "Omega System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, luplupRomInfo, luplupRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	LuplupInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Lup Lup Puzzle / Zhuan Zhuan Puzzle (version 2.9 / 990108)

static struct BurnRomInfo luplup29RomDesc[] = {
	{ "luplup-rom1.v29",		0x080000, 0x36a8b8c1, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "luplup-rom2.v29",		0x080000, 0x50dac70f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "luplup-roml00",			0x200000, 0x08b2aa75, 2 | BRF_GRA },           //  2 Sprites
	{ "luplup-romu00",			0x200000, 0xb57f4ca5, 2 | BRF_GRA },           //  3
	{ "luplup-roml01",			0x200000, 0x41c7ca8c, 2 | BRF_GRA },           //  4
	{ "luplup-romu01",			0x200000, 0x16746158, 2 | BRF_GRA },           //  5

	{ "vrom1.bin",				0x040000, 0x34a56987, 3 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(luplup29)
STD_ROM_FN(luplup29)

static INT32 Luplup29Init()
{
	speedhack_address = 0x113f08;
	speedhack_pc = 0xae6c0;

	return CommonInit(TYPE_E116T, suplup_io_write, suplup_io_read, sound_type_0_init, 8, 0);
}

struct BurnDriver BurnDrvLuplup29 = {
	"luplup29", "suplup", NULL, NULL, "1999",
	"Lup Lup Puzzle / Zhuan Zhuan Puzzle (version 2.9 / 990108)\0", NULL, "Omega System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, luplup29RomInfo, luplup29RomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	Luplup29Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Lup Lup Puzzle / Zhuan Zhuan Puzzle (version 1.05 / 981214)

static struct BurnRomInfo luplup10RomDesc[] = {
	{ "p0_rom1.rom1",			0x080000, 0xa2684e3c, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "p1_rom2.rom2",			0x080000, 0x1043ce44, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "roml00.roml00",			0x200000, 0xe2eeb61e, 2 | BRF_GRA },           //  2 Sprites
	{ "romu00.romu00",			0x200000, 0x9ee855b9, 2 | BRF_GRA },           //  3
	{ "roml01.roml01",			0x200000, 0x7182864c, 2 | BRF_GRA },           //  4
	{ "romu01.romu01",			0x200000, 0x44f76640, 2 | BRF_GRA },           //  5

	{ "vrom1.bin",				0x040000, 0x34a56987, 3 | BRF_SND },           //  6 Samples

	{ "gal22v10b.gal1",			0x0002e5, 0x776c5137, 0 | BRF_OPT },           //  7 PLDs
};

STD_ROM_PICK(luplup10)
STD_ROM_FN(luplup10)

static INT32 Luplup10Init()
{
	speedhack_address = 0x113b78;
	speedhack_pc = 0xb1128;

	return CommonInit(TYPE_E116T, suplup_io_write, suplup_io_read, sound_type_0_init, 8, 0);
}

struct BurnDriver BurnDrvLuplup10 = {
	"luplup10", "suplup", NULL, NULL, "1999",
	"Lup Lup Puzzle / Zhuan Zhuan Puzzle (version 1.05 / 981214)\0", NULL, "Omega System (Adko license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, luplup10RomInfo, luplup10RomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	Luplup10Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Puzzle Bang Bang (Korea, version 2.9 / 990108)

static struct BurnRomInfo puzlbangRomDesc[] = {
	{ "pbb-rom1.v29",			0x080000, 0xeb829586, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "pbb-rom2.v29",			0x080000, 0xfb84c793, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "luplup-roml00",			0x200000, 0x08b2aa75, 2 | BRF_GRA },           //  2 Sprites
	{ "luplup-romu00",			0x200000, 0xb57f4ca5, 2 | BRF_GRA },           //  3
	{ "luplup-roml01",			0x200000, 0x41c7ca8c, 2 | BRF_GRA },           //  4
	{ "luplup-romu01",			0x200000, 0x16746158, 2 | BRF_GRA },           //  5

	{ "vrom1.bin",				0x040000, 0x34a56987, 3 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(puzlbang)
STD_ROM_FN(puzlbang)

static INT32 PuzlbangInit()
{
	speedhack_address = 0x113f14;
	speedhack_pc = 0xae6cc;

	return CommonInit(TYPE_E116T, suplup_io_write, suplup_io_read, sound_type_0_init, 8, 0);
}

struct BurnDriver BurnDrvPuzlbang = {
	"puzlbang", "suplup", NULL, NULL, "1999",
	"Puzzle Bang Bang (Korea, version 2.9 / 990108)\0", NULL, "Omega System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, puzlbangRomInfo, puzlbangRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	PuzlbangInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Puzzle Bang Bang (Korea, version 2.8 / 990106)

static struct BurnRomInfo puzlbangaRomDesc[] = {
	{ "pbb-rom1.v28",			0x080000, 0xfd21c5ff, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "pbb-rom2.v28",			0x080000, 0x490ecaeb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "luplup-roml00",			0x200000, 0x08b2aa75, 2 | BRF_GRA },           //  2 Sprites
	{ "luplup-romu00",			0x200000, 0xb57f4ca5, 2 | BRF_GRA },           //  3
	{ "luplup-roml01",			0x200000, 0x41c7ca8c, 2 | BRF_GRA },           //  4
	{ "luplup-romu01",			0x200000, 0x16746158, 2 | BRF_GRA },           //  5

	{ "vrom1.bin",				0x040000, 0x34a56987, 3 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(puzlbanga)
STD_ROM_FN(puzlbanga)

static INT32 PuzlbangaInit()
{
	speedhack_address = 0x113ecc;
	speedhack_pc = 0xae6cc;

	return CommonInit(TYPE_E116T, suplup_io_write, suplup_io_read, sound_type_0_init, 8, 0);
}

struct BurnDriver BurnDrvPuzlbanga = {
	"puzlbanga", "suplup", NULL, NULL, "1999",
	"Puzzle Bang Bang (Korea, version 2.8 / 990106)\0", NULL, "Omega System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, puzlbangaRomInfo, puzlbangaRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	PuzlbangaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Vamf x1/2 (Europe, version 1.1.0908)

static struct BurnRomInfo vamphalfRomDesc[] = {
	{ "prg.rom1",				0x080000, 0x9b1fc6c5, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code

	{ "eur.roml00",				0x200000, 0xbdee9a46, 2 | BRF_GRA },           //  1 Sprites
	{ "eur.romu00",				0x200000, 0xfa79e8ea, 2 | BRF_GRA },           //  2
	{ "eur.roml01",				0x200000, 0xa7995b06, 2 | BRF_GRA },           //  3
	{ "eur.romu01",				0x200000, 0xe269f5fe, 2 | BRF_GRA },           //  4

	{ "snd.vrom1",				0x040000, 0xee9e371e, 3 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(vamphalf)
STD_ROM_FN(vamphalf)

static INT32 VamphalfInit()
{
	speedhack_address = 0x4a7b8;
	speedhack_pc = 0x82ec;

	return CommonInit(TYPE_E116T, vamphalf_io_write, vamphalf_io_read, sound_type_0_init, 0, 0);
}

struct BurnDriver BurnDrvVamphalf = {
	"vamphalf", NULL, NULL, NULL, "1999",
	"Vamf x1/2 (Europe, version 1.1.0908)\0", NULL, "Danbi / F2 System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, vamphalfRomInfo, vamphalfRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	VamphalfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Vamf x1/2 (Europe, version 1.0.0903)

static struct BurnRomInfo vamphalfr1RomDesc[] = {
	{ "ws1-01201.rom1",			0x080000, 0xafa75c19, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code

	{ "elc.roml01",				0x400000, 0x19df4056, 2 | BRF_GRA },           //  1 Sprites
	{ "evi.romu01",				0x400000, 0xf9803923, 2 | BRF_GRA },           //  2

	{ "ws1-01202.vrom1",		0x040000, 0xee9e371e, 3 | BRF_SND },           //  3 Samples
};

STD_ROM_PICK(vamphalfr1)
STD_ROM_FN(vamphalfr1)

static INT32 Vamphalfr1Init()
{
	speedhack_address = 0x4a468;
	speedhack_pc = 0x82ec;

	return CommonInit(TYPE_E116T, vamphalf_io_write, vamphalf_io_read, sound_type_0_init, 0, 0);
}

struct BurnDriver BurnDrvVamphalfr1 = {
	"vamphalfr1", "vamphalf", NULL, NULL, "1999",
	"Vamf x1/2 (Europe, version 1.0.0903)\0", NULL, "Danbi / F2 System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, vamphalfr1RomInfo, vamphalfr1RomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	Vamphalfr1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Vamp x1/2 (Korea, version 1.1.0908)

static struct BurnRomInfo vamphalfkRomDesc[] = {
	{ "prom1",					0x080000, 0xf05e8e96, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code

	{ "roml00",					0x200000, 0xcc075484, 2 | BRF_GRA },           //  1 Sprites
	{ "romu00",					0x200000, 0x711c8e20, 2 | BRF_GRA },           //  2
	{ "roml01",					0x200000, 0x626c9925, 2 | BRF_GRA },           //  3
	{ "romu01",					0x200000, 0xd5be3363, 2 | BRF_GRA },           //  4

	{ "snd.vrom1",				0x040000, 0xee9e371e, 3 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(vamphalfk)
STD_ROM_FN(vamphalfk)

static INT32 VamphalfkInit()
{
	speedhack_address = 0x4a648;
	speedhack_pc = 0x82ec;

	return CommonInit(TYPE_E116T, vamphalf_io_write, vamphalf_io_read, sound_type_0_init, 0, 0);
}

struct BurnDriver BurnDrvVamphalfk = {
	"vamphalfk", "vamphalf", NULL, NULL, "1999",
	"Vamp x1/2 (Korea, version 1.1.0908)\0", NULL, "Danbi / F2 System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, vamphalfkRomInfo, vamphalfkRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	VamphalfkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Date Quiz Go Go Episode 2

static struct BurnRomInfo dquizgo2RomDesc[] = {
	{ "rom1",					0x080000, 0x81eef038, 1 | BRF_PRG | BRF_ESS }, //  0 EX116T Code
	{ "rom2",					0x080000, 0xe8789d8a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "roml00",					0x200000, 0xde811dd7, 2 | BRF_GRA },           //  2 Sprites
	{ "romu00",					0x200000, 0x2bdbfc6b, 2 | BRF_GRA },           //  3
	{ "roml01",					0x200000, 0xf574a2a3, 2 | BRF_GRA },           //  4
	{ "romu01",					0x200000, 0xd05cf02f, 2 | BRF_GRA },           //  5
	{ "roml02",					0x200000, 0x43ca2cff, 2 | BRF_GRA },           //  6
	{ "romu02",					0x200000, 0xb8218222, 2 | BRF_GRA },           //  7

	{ "vrom1",					0x040000, 0x24d5b55f, 3 | BRF_SND },           //  8 Samples
};

STD_ROM_PICK(dquizgo2)
STD_ROM_FN(dquizgo2)

static INT32 Dquizgo2Init()
{
	speedhack_address = 0xcdde8;
	speedhack_pc = 0xaa630;

	return CommonInit(TYPE_E116T, coolmini_io_write, coolmini_io_read, sound_type_0_init, 0, 0xc00000);
}

struct BurnDriver BurnDrvDquizgo2 = {
	"dquizgo2", NULL, NULL, NULL, "2000",
	"Date Quiz Go Go Episode 2\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, dquizgo2RomInfo, dquizgo2RomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	Dquizgo2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Mission Craft (version 2.7)

static struct BurnRomInfo misncrftRomDesc[] = {
	{ "prg-rom2.bin",			0x080000, 0x04d22da6, 1 | BRF_PRG | BRF_ESS }, //  0 GMS30C2116 Code

	{ "snd-rom2.us1",			0x020000, 0x8821e5b9, 8 | BRF_PRG | BRF_ESS }, //  1 QS1000 Code

	{ "roml00",					0x200000, 0x748c5ae5, 2 | BRF_GRA },           //  2 Sprites
	{ "romh00",					0x200000, 0xf34ae697, 2 | BRF_GRA },           //  3
	{ "roml01",					0x200000, 0xe37ece7b, 2 | BRF_GRA },           //  4
	{ "romh01",					0x200000, 0x71fe4bc3, 2 | BRF_GRA },           //  5

	{ "snd-rom1.u15",			0x080000, 0xfb381da9, 6 | BRF_SND },           //  6 qs1000
	{ "qs1001a.u17",			0x080000, 0xd13c6407, 6 | BRF_SND },           //  7

	{ "93c46-eeprom-misncrft",	0x000080, 0x83c813eb, 9 | BRF_PRG | BRF_ESS }, //  8 Default EEPROM
};

STD_ROM_PICK(misncrft)
STD_ROM_FN(misncrft)

static INT32 MisncrftInit()
{
	speedhack_address = 0x741e8;
	speedhack_pc = 0xff5a;

	INT32 rc = CommonInit(TYPE_GMS30C2116, misncrft_io_write, misncrft_io_read, sound_type_2_init, 0, 0);
	if (!rc) {
		qs1000_set_volume(1.00);
	}
	return rc;
}

struct BurnDriver BurnDrvMisncrft = {
	"misncrft", NULL, NULL, NULL, "2000",
	"Mission Craft (version 2.7)\0", NULL, "Sun", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, misncrftRomInfo, misncrftRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	MisncrftInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	236, 320, 3, 4
};


// Mission Craft (version 2.4)

static struct BurnRomInfo misncrftaRomDesc[] = {
	{ "prg-rom2.bin",			0x080000, 0x059ae8c1, 1 | BRF_PRG | BRF_ESS }, //  0 GMS30C2116 Code

	{ "snd-rom2.us1",			0x020000, 0x8821e5b9, 8 | BRF_PRG | BRF_ESS }, //  1 QS1000 Code

	{ "roml00",					0x200000, 0x748c5ae5, 2 | BRF_GRA },           //  2 Sprites
	{ "romh00",					0x200000, 0xf34ae697, 2 | BRF_GRA },           //  3
	{ "roml01",					0x200000, 0xe37ece7b, 2 | BRF_GRA },           //  4
	{ "romh01",					0x200000, 0x71fe4bc3, 2 | BRF_GRA },           //  5

	{ "snd-rom1.u15",			0x080000, 0xfb381da9, 6 | BRF_SND },           //  6 qs1000
	{ "qs1001a.u17",			0x080000, 0xd13c6407, 6 | BRF_SND },           //  7

	{ "93c46-eeprom-misncrfta",	0x000080, 0x9ad27077, 9 | BRF_PRG | BRF_ESS }, //  8 Default EEPROM
};

STD_ROM_PICK(misncrfta)
STD_ROM_FN(misncrfta)

static INT32 MisncrftaInit()
{
	speedhack_address = 0x72e2c;
	speedhack_pc = 0xecd6;

	INT32 rc = CommonInit(TYPE_GMS30C2116, misncrft_io_write, misncrft_io_read, sound_type_2_init, 0, 0);
	if (!rc) {
		qs1000_set_volume(2.00);
	}
	return rc;
}

struct BurnDriver BurnDrvMisncrfta = {
	"misncrfta", "misncrft", NULL, NULL, "2000",
	"Mission Craft (version 2.4)\0", NULL, "Sun", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, misncrftaRomInfo, misncrftaRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	MisncrftaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	236, 320, 3, 4
};


// Mr. Dig

static struct BurnRomInfo mrdigRomDesc[] = {
	{ "rom1.bin",				0x080000, 0x5b960320, 1 | BRF_PRG | BRF_ESS }, //  0 GMS30C2116 Code
	{ "rom2.bin",				0x080000, 0x75d48b64, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "roml00.bin",				0x200000, 0xf6b161ea, 2 | BRF_GRA },           //  2 Sprites
	{ "romh00.bin",				0x200000, 0x5477efed, 2 | BRF_GRA },           //  3

	{ "vrom1.bin",				0x040000, 0x5fd9e1c6, 3 | BRF_SND },           //  4 Samples
};

STD_ROM_PICK(mrdig)
STD_ROM_FN(mrdig)

static INT32 MrdigInit()
{
	speedhack_address = 0x0e0768;
	speedhack_pc = 0xae38;

	return CommonInit(TYPE_GMS30C2116, mrdig_io_write, mrdig_io_read, sound_type_0_init, 0, 0x800000);
}

struct BurnDriver BurnDrvMrdig = {
	"mrdig", NULL, NULL, NULL, "2000",
	"Mr. Dig\0", NULL, "Sun", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, mrdigRomInfo, mrdigRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	MrdigInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Diet Family

static struct BurnRomInfo dtfamilyRomDesc[] = {
	{ "rom1",					0x080000, 0x738636d2, 1 | BRF_PRG | BRF_ESS }, //  0 E116T Code
	{ "rom2",					0x080000, 0x0953f5e4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "roml00",					0x200000, 0x7e2a7520, 2 | BRF_GRA },           //  2 Sprites
	{ "romu00",					0x200000, 0xc170755f, 2 | BRF_GRA },           //  3
	{ "roml01",					0x200000, 0x3d487ffe, 2 | BRF_GRA },           //  4
	{ "romu01",					0x200000, 0x716efedb, 2 | BRF_GRA },           //  5
	{ "roml02",					0x200000, 0xc3dd3c96, 2 | BRF_GRA },           //  6
	{ "romu02",					0x200000, 0x80830961, 2 | BRF_GRA },           //  7

	{ "vrom1",					0x080000, 0x4aacaef3, 3 | BRF_SND },           //  8 Samples
};

STD_ROM_PICK(dtfamily)
STD_ROM_FN(dtfamily)

static INT32 DtfamilyInit()
{
	speedhack_address = 0xcc2a8;
	speedhack_pc = 0x12fa6;

	return CommonInit(TYPE_E116T, coolmini_io_write, coolmini_io_read, sound_type_0_init, 0, 0xc00000);
}

struct BurnDriver BurnDrvDtfamily = {
	"dtfamily", NULL, NULL, NULL, "2001",
	"Diet Family\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, dtfamilyRomInfo, dtfamilyRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	DtfamilyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Final Godori (Korea, version 2.20.5915)

static struct BurnRomInfo finalgdrRomDesc[] = {
	{ "rom1",					0x080000, 0x45815931, 1 | BRF_PRG | BRF_ESS }, //  0 E132T Code

	{ "roml00",					0x200000, 0x8334459d, 2 | BRF_GRA },           //  1 Sprites
	{ "romh00",					0x200000, 0xf28578a5, 2 | BRF_GRA },           //  2

	{ "u7",						0x080000, 0x080f61f8, 3 | BRF_SND },           //  3 Samples
};

STD_ROM_PICK(finalgdr)
STD_ROM_FN(finalgdr)

static INT32 FinalgdrInit()
{
	speedhack_address = 0x5e870;
	speedhack_pc = 0x1c20c;

	protection_data[0] = 2;
	protection_data[1] = 3;

	return CommonInit(TYPE_E132T, finalgdr_io_write, finalgdr_io_read, sound_type_0_init, 0, 0x800000);
}

struct BurnDriver BurnDrvFinalgdr = {
	"finalgdr", NULL, NULL, NULL, "2001",
	"Final Godori (Korea, version 2.20.5915)\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, finalgdrRomInfo, finalgdrRomName, NULL, NULL, NULL, NULL, FinalgdrInputInfo, NULL,
	FinalgdrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Mr. Kicker (F-E1-16-010 PCB)

static struct BurnRomInfo mrkickerRomDesc[] = {
	{ "3-semicom.rom2",			0x080000, 0x3f7fa08b, 1 | BRF_PRG | BRF_ESS }, //  0 E116T Code

	{ "roml00",					0x200000, 0xc677aac3, 2 | BRF_GRA },           //  1 Sprites
	{ "romh00",					0x200000, 0xb6337d4a, 2 | BRF_GRA },           //  2

	{ "11-semicom.vrom1",		0x080000, 0xe8141fcd, 3 | BRF_SND },           //  3 Samples
};

STD_ROM_PICK(mrkicker)
STD_ROM_FN(mrkicker)

static INT32 MrkickerInit()
{
	speedhack_address = 0x63fc0;
	speedhack_pc = 0x41ec6;

	return CommonInit(TYPE_E116T, coolmini_io_write, coolmini_io_read, sound_type_0_init, 0, 0x800000);
}

struct BurnDriver BurnDrvMrkicker = {
	"mrkicker", NULL, NULL, NULL, "2001",
	"Mr. Kicker (F-E1-16-010 PCB)\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, mrkickerRomInfo, mrkickerRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	MrkickerInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Mr. Kicker (SEMICOM-003b PCB)

static struct BurnRomInfo mrkickeraRomDesc[] = {
	{ "2-semicom.rom1",			0x080000, 0xd3da29ca, 1 | BRF_PRG | BRF_ESS }, //  0 E132T Code

	{ "roml00",					0x200000, 0xc677aac3, 2 | BRF_GRA },           //  1 Sprites
	{ "romh00",					0x200000, 0xb6337d4a, 2 | BRF_GRA },           //  2

	{ "at27c040.u7",			0x080000, 0xe8141fcd, 3 | BRF_SND },           //  3 Samples

	{ "eeprom-mrkicker.bin",	0x000080, 0x87afb8f7, 9 | BRF_PRG | BRF_ESS }, //  4 Default EEPROM
};

STD_ROM_PICK(mrkickera)
STD_ROM_FN(mrkickera)

static INT32 MrkickeraInit()
{
	speedhack_address = 0x701a0;
	speedhack_pc = 0x46a30;

	protection_data[0] = 2;
	protection_data[1] = 3;

	return CommonInit(TYPE_E132T, mrkickera_io_write, mrkickera_io_read, sound_type_0_init, 0, 0x800000);
}

struct BurnDriverD BurnDrvMrkickera = {
	"mrkickera", "mrkicker", NULL, NULL, "2001",
	"Mr. Kicker (SEMICOM-003b PCB)\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, mrkickeraRomInfo, mrkickeraRomName, NULL, NULL, NULL, NULL, FinalgdrInputInfo, NULL,
	MrkickeraInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Toy Land Adventure

static struct BurnRomInfo toylandRomDesc[] = {
	{ "rom2.bin",				0x080000, 0xe3455002, 1 | BRF_PRG | BRF_ESS }, //  0 E116T Code

	{ "roml00.bin",				0x200000, 0x06f5673d, 2 | BRF_GRA },           //  1 Sprites
	{ "romu00.bin",				0x200000, 0x8c3db0e4, 2 | BRF_GRA },           //  2
	{ "roml01.bin",				0x200000, 0x076a84e1, 2 | BRF_GRA },           //  3
	{ "romu01.bin",				0x200000, 0x1bc33d01, 2 | BRF_GRA },           //  4

	{ "vrom1.bin",				0x040000, 0xd7e6fc5d, 3 | BRF_SND },           //  5 Samples

	{ "epr1.ic3",				0x000080, 0x812f3d87, 9 | BRF_PRG | BRF_ESS }, //  6 Default EEPROM
};

STD_ROM_PICK(toyland)
STD_ROM_FN(toyland)

static INT32 ToylandInit()
{
	speedhack_address = 0x780d8;
	speedhack_pc = 0x130c2;

	return CommonInit(TYPE_E116T, coolmini_io_write, coolmini_io_read, sound_type_0_init, 0, 0);
}

struct BurnDriver BurnDrvToyland = {
	"toyland", NULL, NULL, NULL, "2001",
	"Toy Land Adventure\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, toylandRomInfo, toylandRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	ToylandInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};


// Wivern Wings

static struct BurnRomInfo wivernwgRomDesc[] = {
	{ "rom1",					0x080000, 0x83eb9a36, 1 | BRF_PRG | BRF_ESS }, //  0 E132T Code
	{ "rom2",					0x080000, 0x5d657055, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u7",						0x020000, 0x00a3f705, 8 | BRF_PRG | BRF_ESS }, //  2 QS1000 Code

	{ "roml00",					0x200000, 0xfb3541b6, 2 | BRF_GRA },           //  3 Sprites
	{ "romh00",					0x200000, 0x516aca48, 2 | BRF_GRA },           //  4
	{ "roml01",					0x200000, 0x1c764f95, 2 | BRF_GRA },           //  5
	{ "romh01",					0x200000, 0xfee42c63, 2 | BRF_GRA },           //  6
	{ "roml02",					0x200000, 0xfc846707, 2 | BRF_GRA },           //  7
	{ "romh02",					0x200000, 0x86141c7d, 2 | BRF_GRA },           //  8
	{ "l03",					0x200000, 0x85aa8db8, 2 | BRF_GRA },           //  9
	{ "h03",					0x200000, 0xade8af9f, 2 | BRF_GRA },           // 10

	{ "romsnd.u15a",			0x200000, 0xfc89eedc, 6 | BRF_SND },           // 11 QS1000 Samples
	{ "qs1001a",				0x080000, 0xd13c6407, 6 | BRF_SND },           // 12
};

STD_ROM_PICK(wivernwg)
STD_ROM_FN(wivernwg)

static INT32 WivernwgInit()
{
	speedhack_address = 0xb4cc4;
	speedhack_pc = 0x10766;

	protection_data[0] = 2;
	protection_data[1] = 1;

	return CommonInit(TYPE_E132T, wyvernwg_io_write, wyvernwg_io_read, sound_type_2_init, 0, 0);
}

struct BurnDriver BurnDrvWivernwg = {
	"wivernwg", NULL, NULL, NULL, "2001",
	"Wivern Wings\0", NULL, "SemiCom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, wivernwgRomInfo, wivernwgRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	WivernwgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	236, 320, 3, 4
};


// Wyvern Wings (set 1)

static struct BurnRomInfo wyvernwgRomDesc[] = {
	{ "rom1.bin",				0x080000, 0x66bf3a5c, 1 | BRF_PRG | BRF_ESS }, //  0 E132T Code
	{ "rom2.bin",				0x080000, 0xfd9b5911, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u7",						0x020000, 0x00a3f705, 8 | BRF_PRG | BRF_ESS }, //  2 QS1000 Code

	{ "roml00",					0x200000, 0xfb3541b6, 2 | BRF_GRA },           //  3 Sprites
	{ "romh00",					0x200000, 0x516aca48, 2 | BRF_GRA },           //  4
	{ "roml01",					0x200000, 0x1c764f95, 2 | BRF_GRA },           //  5
	{ "romh01",					0x200000, 0xfee42c63, 2 | BRF_GRA },           //  6
	{ "roml02",					0x200000, 0xfc846707, 2 | BRF_GRA },           //  7
	{ "romh02",					0x200000, 0x86141c7d, 2 | BRF_GRA },           //  8
	{ "roml03",					0x200000, 0xb10bf37c, 2 | BRF_GRA },           //  9
	{ "romh03",					0x200000, 0xe01c2a92, 2 | BRF_GRA },           // 10

	{ "romsnd.u15a",			0x200000, 0xfc89eedc, 6 | BRF_SND },           // 11 QS1000 Samples
	{ "qs1001a",				0x080000, 0xd13c6407, 6 | BRF_SND },           // 12
};

STD_ROM_PICK(wyvernwg)
STD_ROM_FN(wyvernwg)

static INT32 WyvernwgInit()
{
	speedhack_address = 0xb56f4;
	speedhack_pc = 0x10766;

	protection_data[0] = 2;
	protection_data[1] = 1;

	return CommonInit(TYPE_E132T, wyvernwg_io_write, wyvernwg_io_read, sound_type_2_init, 0, 0);
}

struct BurnDriver BurnDrvWyvernwg = {
	"wyvernwg", "wivernwg", NULL, NULL, "2001",
	"Wyvern Wings (set 1)\0", NULL, "SemiCom (Game Vision license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, wyvernwgRomInfo, wyvernwgRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	WyvernwgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	236, 320, 3, 4
};


// Wyvern Wings (set 2)

static struct BurnRomInfo wyvernwgaRomDesc[] = {
	{ "rom1.rom",				0x080000, 0x586881fd, 1 | BRF_PRG | BRF_ESS }, //  0 E132T Code
	{ "rom2.rom",				0x080000, 0x938049ec, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "u7",						0x020000, 0x00a3f705, 8 | BRF_PRG | BRF_ESS }, //  2 QS1000 Code

	{ "roml00",					0x200000, 0xfb3541b6, 2 | BRF_GRA },           //  3 Sprites
	{ "romh00",					0x200000, 0x516aca48, 2 | BRF_GRA },           //  4
	{ "roml01",					0x200000, 0x1c764f95, 2 | BRF_GRA },           //  5
	{ "romh01",					0x200000, 0xfee42c63, 2 | BRF_GRA },           //  6
	{ "roml02",					0x200000, 0xfc846707, 2 | BRF_GRA },           //  7
	{ "romh02",					0x200000, 0x86141c7d, 2 | BRF_GRA },           //  8
	{ "roml03",					0x200000, 0xb10bf37c, 2 | BRF_GRA },           //  9
	{ "romh03",					0x200000, 0xe01c2a92, 2 | BRF_GRA },           // 10

	{ "romsnd.u15a",			0x200000, 0xfc89eedc, 6 | BRF_SND },           // 11 QS1000 Samples
	{ "qs1001a",				0x080000, 0xd13c6407, 6 | BRF_SND },           // 12
};

STD_ROM_PICK(wyvernwga)
STD_ROM_FN(wyvernwga)

static INT32 WyvernwgaInit()
{
	speedhack_address = 0xb74f0;
	speedhack_pc = 0x10766;

	protection_data[0] = 2;
	protection_data[1] = 1;

	return CommonInit(TYPE_E132T, wyvernwg_io_write, wyvernwg_io_read, sound_type_2_init, 0, 0);
}

struct BurnDriver BurnDrvWyvernwga = {
	"wyvernwga", "wivernwg", NULL, NULL, "2001",
	"Wyvern Wings (set 2)\0", NULL, "SemiCom (Game Vision license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, wyvernwgaRomInfo, wyvernwgaRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	WyvernwgaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	236, 320, 3, 4
};


// Age Of Heroes - Silkroad 2 (v0.63 - 2001/02/07)

static struct BurnRomInfo aohRomDesc[] = {
	{ "rom1",					0x200000, 0x2e55ff55, 1 | BRF_PRG | BRF_ESS }, //  0 E132XN Code
	{ "rom2",					0x200000, 0x50f8a409, 1 | BRF_PRG | BRF_ESS }, //  1

	// swapped from mame
	{ "g09",					0x800000, 0xc359febb, 2 | BRF_GRA },           //  2 Sprites
	{ "g05",					0x800000, 0x64c8f493, 2 | BRF_GRA },           //  3
	{ "g10",					0x800000, 0x08217573, 2 | BRF_GRA },           //  4
	{ "g06",					0x800000, 0xffbc9fe5, 2 | BRF_GRA },           //  5
	{ "g11",					0x800000, 0x5f0461b8, 2 | BRF_GRA },           //  6
	{ "g07",					0x800000, 0x5cb3c86a, 2 | BRF_GRA },           //  7
	{ "g12",					0x800000, 0xe437b35f, 2 | BRF_GRA },           //  8
	{ "g08",					0x800000, 0x1fd08aa0, 2 | BRF_GRA },           //  9

	// swapped from MAME!
	{ "rom4",					0x080000, 0xbba47755, 3 | BRF_SND },           // 11 OKI #0 Samples

	{ "rom3",					0x040000, 0xdb8cb455, 4 | BRF_SND },           // 10 OKI #1 Samples
};

STD_ROM_PICK(aoh)
STD_ROM_FN(aoh)

static void aoh_speedhack_callback(UINT32 address)
{
	if (address == speedhack_address) {
		if (E132XSGetPC(0) == 0xb994 || E132XSGetPC(0) == 0xba40) {
			E132XSBurnCycles(500);
		}
	}
}

static INT32 AohInit()
{
	cpu_clock = 80000000;
	speedhack_address = 0x28a09c;
	speedhack_pc = 0xb994; // & ba40
	speedhack_callback = aoh_speedhack_callback;

	return CommonInit(TYPE_E132XN, aoh_io_write, aoh_io_read, sound_type_1_init, 0, 0);
}

struct BurnDriver BurnDrvAoh = {
	"aoh", NULL, NULL, NULL, "2001",
	"Age Of Heroes - Silkroad 2 (v0.63 - 2001/02/07)\0", NULL, "Unico", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, aohRomInfo, aohRomName, NULL, NULL, NULL, NULL, AohInputInfo, NULL,
	AohInit, DrvExit, DrvFrame, AohDraw, DrvScan, &DrvRecalc, 0x8000,
	384, 224, 4, 3
};


// Boong-Ga Boong-Ga (Spank'em!)

static struct BurnRomInfo boonggabRomDesc[] = {
	{ "1.rom1",					0x080000, 0x50522da1, 1 | BRF_PRG | BRF_ESS }, //  0 E116T Code
	{ "2.rom0",					0x080000, 0x3395541b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "boong-ga.roml00",		0x200000, 0x18be5f92, 2 | BRF_GRA },           //  2 Sprites
	{ "boong-ga.romu00",		0x200000, 0x0158ba9e, 2 | BRF_GRA },           //  3
	{ "boong-ga.roml05",		0x200000, 0x76d60553, 2 | BRF_GRA },           //  4
	{ "boong-ga.romu05",		0x200000, 0x35ee8fb5, 2 | BRF_GRA },           //  5
	{ "boong-ga.roml01",		0x200000, 0x636e9d5d, 2 | BRF_GRA },           //  6
	{ "boong-ga.romu01",		0x200000, 0xb8dcf6b7, 2 | BRF_GRA },           //  7
	{ "boong-ga.roml06",		0x200000, 0x8dc521b7, 2 | BRF_GRA },           //  8
	{ "boong-ga.romu06",		0x200000, 0xf6b83270, 2 | BRF_GRA },           //  9
	{ "boong-ga.roml02",		0x200000, 0xd0661c69, 2 | BRF_GRA },           // 10
	{ "boong-ga.romu02",		0x200000, 0xeac01eb8, 2 | BRF_GRA },           // 11
	{ "boong-ga.roml07",		0x200000, 0x3301813a, 2 | BRF_GRA },           // 12
	{ "boong-ga.romu07",		0x200000, 0x3f1c3682, 2 | BRF_GRA },           // 13
	{ "boong-ga.roml03",		0x200000, 0x4d4260b3, 2 | BRF_GRA },           // 14
	{ "boong-ga.romu03",		0x200000, 0x4ba00032, 2 | BRF_GRA },           // 15

	{ "3.vrom1",				0x080000, 0x0696bfcb, 3 | BRF_SND },           // 16 Samples
	{ "4.vrom2",				0x080000, 0x305c2b16, 3 | BRF_SND },           // 17
};

STD_ROM_PICK(boonggab)
STD_ROM_FN(boonggab)

static INT32 BoonggabInit()
{
	speedhack_address = 0xf1b74;
	speedhack_pc = 0x131a6;
	is_boongga = 1;

	return CommonInit(TYPE_E116T, boonggab_io_write, boonggab_io_read, sound_type_0_init, 0, 0x2000000);
}

struct BurnDriver BurnDrvBoonggab = {
	"boonggab", NULL, NULL, NULL, "2001",
	"Boong-Ga Boong-Ga (Spank'em!)\0", NULL, "Taff System", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, boonggabRomInfo, boonggabRomName, NULL, NULL, NULL, NULL, BoonggabInputInfo, NULL,
	BoonggabInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	236, 320, 3, 4
};


// Yori Jori Kuk Kuk

static struct BurnRomInfo yorijoriRomDesc[] = {
	{ "prg1",					0x200000, 0x0e04eb40, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu

	{ "snd5",					0x020000, 0x79067367, 8 | BRF_PRG | BRF_ESS }, //  1 QS1000 Code

	{ "roml00",					0x200000, 0x9299ce36, 3 | BRF_GRA },           //  2 Sprites
	{ "romh00",					0x200000, 0x16584ff2, 3 | BRF_GRA },           //  3
	{ "roml01",					0x200000, 0xb5d1892f, 3 | BRF_GRA },           //  4
	{ "romh01",					0x200000, 0xfe0485ef, 3 | BRF_GRA },           //  5

	{ "snd2",					0x200000, 0x8d9a8795, 6 | BRF_SND },           //  6 QS1000 Samples
	{ "qs1001a.snd3",			0x080000, 0xd13c6407, 6 | BRF_SND },           //  7
};

STD_ROM_PICK(yorijori)
STD_ROM_FN(yorijori)

static INT32 YorijoriInit()
{
	return 0; // game not working!
}

struct BurnDriverD BurnDrvYorijori = {
	"yorijori", NULL, NULL, NULL, "199?",
	"Yori Jori Kuk Kuk\0", NULL, "Golden Bell Entertainment", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, yorijoriRomInfo, yorijoriRomName, NULL, NULL, NULL, NULL, CommonInputInfo, NULL,
	YorijoriInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	320, 236, 4, 3
};

