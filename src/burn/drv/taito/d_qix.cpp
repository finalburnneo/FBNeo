// FB Alpha Taito Qix driver module
// Based on MAME driver by Aaron Giles and Zsolt Vasvari

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "m6800_intf.h"
#include "taito_m68705.h"
#include "6821pia.h"
#include "dac.h"
#include "sn76496.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *DrvM6802ROM;
static UINT8 *DrvM68705ROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvM6809RAM0;
static UINT8 *DrvM6802RAM;
static UINT8 *DrvM68705RAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 videoaddress[2];
static UINT8 palettebank;
static INT32 flipscreen;
static INT32 bankaddress;
static INT32 qix_coinctrl;
static INT32 videoram_mask;

static INT32 scanline;
static INT32 lastline;

static INT32 has_mcu = 0;
static INT32 has_soundcpu = 0;
static INT32 has_4way = 0;

static INT32 is_slither = 0;
static INT32 is_zookeep = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvInputs[5];
static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;
static INT16 DrvAnalogPort2 = 0;
static INT16 DrvAnalogPort3 = 0;
static UINT8 DrvReset;

static INT32 nExtraCycles[4] = { 0, 0, 0, 0 };

static struct BurnInputInfo QixInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy2 + 4,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",				BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",				BIT_DIGITAL,	DrvJoy5 + 0,	"p2 up"		},
	{"P2 Down",				BIT_DIGITAL,	DrvJoy5 + 2,	"p2 down"	},
	{"P2 Left",				BIT_DIGITAL,	DrvJoy5 + 3,	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	DrvJoy5 + 1,	"p2 right"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy5 + 7,	"p2 fire 1"	},
	{"P2 Button 2",			BIT_DIGITAL,	DrvJoy5 + 4,	"p2 fire 2"	},

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy2 + 6,	"p3 coin"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Test Advance",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Test Next Line",		BIT_DIGITAL,	DrvJoy2 + 1,	"service2"	},
	{"Test Slew Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"service3"	},
	{"Test Slew Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"service4"	},
	{"Tilt",				BIT_DIGITAL,	DrvJoy2 + 7,	"tilt"		},
};

STDINPUTINFO(Qix)

static struct BurnInputInfo ZookeepInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy2 + 4,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",				BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",				BIT_DIGITAL,	DrvJoy5 + 0,	"p2 up"		},
	{"P2 Down",				BIT_DIGITAL,	DrvJoy5 + 2,	"p2 down"	},
	{"P2 Left",				BIT_DIGITAL,	DrvJoy5 + 3,	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	DrvJoy5 + 1,	"p2 right"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy5 + 6,	"p2 fire 1"	},

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy2 + 6,	"p3 coin"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Test Advance",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Test Next Line",		BIT_DIGITAL,	DrvJoy2 + 1,	"service2"	},
	{"Test Slew Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"service3"	},
	{"Test Slew Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"service4"	},
	{"Tilt",				BIT_DIGITAL,	DrvJoy2 + 7,	"tilt"		},
};

STDINPUTINFO(Zookeep)

static struct BurnInputInfo SdungeonInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy2 + 4,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 start"	},
	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left Stick Left",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Left Stick Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Right Stick Up",	BIT_DIGITAL,	DrvJoy1 + 4,	"p3 up"		},
	{"P1 Right Stick Down",	BIT_DIGITAL,	DrvJoy1 + 6,	"p3 down"	},
	{"P1 Right Stick Left",	BIT_DIGITAL,	DrvJoy1 + 7,	"p3 left"	},
	{"P1 Right Stick Right",BIT_DIGITAL,	DrvJoy1 + 5,	"p3 right"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Left Stick Start",	BIT_DIGITAL,	DrvJoy4 + 1,	"p2 start"	},
	{"P2 Left Stick Up",	BIT_DIGITAL,	DrvJoy5 + 0,	"p2 up"		},
	{"P2 Left Stick Down",	BIT_DIGITAL,	DrvJoy5 + 2,	"p2 down"	},
	{"P2 Left Stick Left",	BIT_DIGITAL,	DrvJoy5 + 3,	"p2 left"	},
	{"P2 Left Stick Right",	BIT_DIGITAL,	DrvJoy5 + 1,	"p2 right"	},
	{"P2 Right Stick Up",	BIT_DIGITAL,	DrvJoy5 + 4,	"p4 up"		},
	{"P2 Right Stick Down",	BIT_DIGITAL,	DrvJoy5 + 6,	"p4 down"	},
	{"P2 Right Stick Left",	BIT_DIGITAL,	DrvJoy5 + 7,	"p4 left"	},
	{"P2 Right Stick Right",BIT_DIGITAL,	DrvJoy5 + 5,	"p4 right"	},

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy2 + 6,	"p3 coin"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Test Advance",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Test Next Line",		BIT_DIGITAL,	DrvJoy2 + 1,	"service2"	},
	{"Test Slew Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"service3"	},
	{"Test Slew Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"service4"	},
	{"Tilt",				BIT_DIGITAL,	DrvJoy2 + 7,	"tilt"		},
};

STDINPUTINFO(Sdungeon)

static struct BurnInputInfo ElecyoyoInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy2 + 4,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",				BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",				BIT_DIGITAL,	DrvJoy5 + 0,	"p2 up"		},
	{"P2 Down",				BIT_DIGITAL,	DrvJoy5 + 2,	"p2 down"	},
	{"P2 Left",				BIT_DIGITAL,	DrvJoy5 + 3,	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	DrvJoy5 + 1,	"p2 right"	},

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy2 + 6,	"p3 coin"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Test Advance",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Test Next Line",		BIT_DIGITAL,	DrvJoy2 + 1,	"service2"	},
	{"Test Slew Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"service3"	},
	{"Test Slew Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"service4"	},
	{"Tilt",				BIT_DIGITAL,	DrvJoy2 + 7,	"tilt"		},
};

STDINPUTINFO(Elecyoyo)

static struct BurnInputInfo KramInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy2 + 4,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",				BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",				BIT_DIGITAL,	DrvJoy5 + 0,	"p2 up"		},
	{"P2 Down",				BIT_DIGITAL,	DrvJoy5 + 2,	"p2 down"	},
	{"P2 Left",				BIT_DIGITAL,	DrvJoy5 + 3,	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	DrvJoy5 + 1,	"p2 right"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy5 + 7,	"p2 fire 1"	},
	{"P2 Button 2",			BIT_DIGITAL,	DrvJoy5 + 4,	"p2 fire 2"	},

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy2 + 6,	"p3 coin"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Test Advance",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Test Next Line",		BIT_DIGITAL,	DrvJoy2 + 1,	"service2"	},
	{"Test Slew Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"service3"	},
	{"Test Slew Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"service4"	},
	{"Tilt",				BIT_DIGITAL,	DrvJoy2 + 7,	"tilt"		},
};

STDINPUTINFO(Kram)

static struct BurnInputInfo ComplexxInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy2 + 4,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left Stick Left",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Left Stick Right",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P2 Right Stick Up",	BIT_DIGITAL,	DrvJoy5 + 4,	"p2 up"		},
	{"P2 Right Stick Down",	BIT_DIGITAL,	DrvJoy5 + 6,	"p2 down"	},
	{"P2 Right Stick Left",	BIT_DIGITAL,	DrvJoy5 + 7,	"p2 left"	},
	{"P2 Right Stick Right",BIT_DIGITAL,	DrvJoy5 + 5,	"p2 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P3 Coin",				BIT_DIGITAL,	DrvJoy2 + 6,	"p3 coin"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Test Advance",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Test Next Line",		BIT_DIGITAL,	DrvJoy2 + 1,	"service2"	},
	{"Test Slew Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"service3"	},
	{"Test Slew Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"service4"	},
	{"Tilt",				BIT_DIGITAL,	DrvJoy2 + 7,	"tilt"		},
};

STDINPUTINFO(Complexx)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo SlitherInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy2 + 4,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},
	A("P1 Trackball X", 	BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis" ),
	A("P1 Trackball Y", 	BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis" ),

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 1"	},
	{"P2 Button 2",			BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"	},
	A("P2 Trackball X", 	BIT_ANALOG_REL, &DrvAnalogPort2,"p2 x-axis" ),
	A("P2 Trackball Y", 	BIT_ANALOG_REL, &DrvAnalogPort3,"p2 y-axis" ),

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy2 + 6,	"p3 coin"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy1 + 0,	"diag"		},
	{"Tilt",				BIT_DIGITAL,	DrvJoy2 + 7,	"tilt"		},
};

STDINPUTINFO(Slither)
#undef A

static void partial_update();

static void mcu_sync()
{
	if (!has_mcu) return;
	INT32 cyc = (M6809TotalCycles() * 100 / 125) - m6805TotalCycles();
	if (cyc > 0) {
		m6805Run(cyc);
	}
}

static UINT8 qix_main_read(UINT16 address)
{
	address |= is_zookeep << 15;

	if ((address & 0xfc00) == 0x8800) return 0; // nop (ACIA)

	if ((address & 0xfc00) == 0x9000) {
		return pia_read(3, address & 0x3ff); // or & 3????
	}

	if ((address & 0xfc00) == 0x9400) {
		mcu_sync();
		return pia_read(0, address & 0x3ff);
	}

	if ((address & 0xfc00) == 0x9800) {
		return pia_read(1, address & 0x3ff);
	}

	if ((address & 0xfc00) == 0x9c00) {
		mcu_sync();
		return pia_read(2, address & 0x3ff);
	}

	if ((address & 0xfc00) == 0x8c00) address &= ~0x3fe; // mirror

	switch (address)
	{
		case 0x8c00:
			M6809Close();
			M6809Open(1);
			M6809SetIRQLine(1, CPU_IRQSTATUS_ACK);
			M6809Close();
			M6809Open(0);
			return 0xff;

		case 0x8c01:
			M6809SetIRQLine(1, CPU_IRQSTATUS_NONE);
			return 0xff;
	}

	//bprintf (0, _T("MR: %4.4x\n"), address);

	return 0;
}

static void qix_main_write(UINT16 address, UINT8 data)
{
	address |= is_zookeep << 15;

	if ((address & 0xfc00) == 0x9000) {
		pia_write(3, address & 0x3ff, data);
		return;
	}

	if ((address & 0xfc00) == 0x9400) {
		mcu_sync();
		if ((address & 0x3ff) == 0 && has_mcu) data = 0;
		pia_write(0, address & 0x3ff, data);
		return;
	}

	if ((address & 0xfc00) == 0x9800) {
		pia_write(1, address & 0x3ff, data);
		return;
	}

	if ((address & 0xfc00) == 0x9c00) {
		mcu_sync();
		if ((address & 0x3ff) == 0 && has_mcu) data = 0;
		pia_write(2, address & 0x3ff, data);
		return;
	}

	if ((address & 0xfc00) == 0x8c00) address &= ~0x3fe; // mirror

	switch (address)
	{
		case 0x8c00:
			M6809Close();
			M6809Open(1);
			M6809SetIRQLine(1, CPU_IRQSTATUS_ACK);
			M6809Close();
			M6809Open(0);
		return;

		case 0x8c01:
			M6809SetIRQLine(1, CPU_IRQSTATUS_NONE);
		return;
	}

	//bprintf (0, _T("MW: %4.4x, %2.2x\n"), address, data);
}

static void bankswitch()
{
	INT32 bank = bankaddress ? 0 : 0xa000;

	M6809MapMemory(DrvM6809ROM1 + bank, 0xa000, 0xbfff, MAP_ROM);
}

static void videobank()
{
	INT32 bank = (videoaddress[0] & 0x80) << 8;

	M6809MapMemory(DrvVidRAM + bank, 0x0000, 0x7fff, MAP_ROM);
}

static UINT8 qix_video_read(UINT16 address)
{
	if (address < 0x8000) {
		INT32 offset = address + ((videoaddress[0] & 0x80) << 8);
		return DrvVidRAM[offset];
	}

	switch (address)
	{
		case 0x8800:
		case 0x8801:
			return 0; // ??

		case 0x8c00:
			M6809Close();
			M6809Open(0);
			M6809SetIRQLine(1, CPU_IRQSTATUS_ACK);
			M6809Close();
			M6809Open(1);
			return 0xff;

		case 0x8c01:
			M6809SetIRQLine(1, CPU_IRQSTATUS_NONE);
			return 0xff;

		case 0x9400:
			return DrvVidRAM[videoaddress[1] + (videoaddress[0] * 256)];

		case 0x9402:
			return 0; // ??

		case 0x9800:
			return (scanline < 256) ? scanline : 0;

		case 0x9c00:
		case 0x9c01: // mc6845 registers
			return 0;
	}

	//bprintf (0, _T("SR: %4.4x\n"), address);

	return 0;
}

static void qix_video_write(UINT16 address, UINT8 data)
{
	if (address < 0x8000) {
		partial_update();
		INT32 offset = address + ((videoaddress[0] & 0x80) << 8);
		DrvVidRAM[offset] &= ~videoram_mask;
		DrvVidRAM[offset] |= data & videoram_mask;
		return;
	}

	if ((address & 0xfc00) == 0x9000) {
		partial_update();
		DrvPalRAM[address & 0x3ff] = data;
		DrvRecalc = 1;
		return;
	}

	switch (address & ~0x3ff)
	{
		case 0x8800:
			partial_update();
			palettebank = data & 3;

			if ((address & ~0x3fe) == 0x8801 && is_zookeep) {
				bankaddress = data & 4;
				bankswitch();
			}
		return;
	}

	switch (address)
	{
		case 0x8c00:
			M6809Close();
			M6809Open(0);
			M6809SetIRQLine(1, CPU_IRQSTATUS_ACK);
			M6809Close();
			M6809Open(1);
		return;

		case 0x8c01:
			M6809SetIRQLine(1, CPU_IRQSTATUS_NONE);
		return;

		case 0x9400: {
			partial_update();
			UINT16 offset = videoaddress[1] + (videoaddress[0] * 256);
			DrvVidRAM[offset] &= ~videoram_mask;
			DrvVidRAM[offset] |= data & videoram_mask;
		}
		return;

		case 0x9401:
			if (is_slither)
				videoram_mask = data;
		return;

		case 0x9402:
			partial_update();
			videoaddress[0] = data;
			videobank();
		return;

		case 0x9403:
			videoaddress[1] = data;
		return;

		case 0x9c00:
		case 0x9c01:
		return; // mc6845 registers

		case 0x9ffc: // zookepr
		case 0x9ffd:
		return; // nop
	}
	//bprintf (0, _T("SW: %4.4x, %2.2x\n"), address, data);
}

static UINT8 qix_sound_read(UINT16 address)
{
	switch (address & ~3)
	{
		case 0x2000:
			return pia_read(5, address & 3);

		case 0x4000:
			return pia_read(4, address & 3);
	}

	return 0;
}

static void qix_sound_write(UINT16 address, UINT8 data)
{
	switch (address & ~3)
	{
		case 0x2000:
			pia_write(5, address & 3, data);
		return;

		case 0x4000:
			pia_write(4, address & 3, data);
		return;
	}
}

static UINT8 input0_read(UINT16)
{
	return DrvInputs[0];
}

static UINT8 input1_read(UINT16)
{
	return DrvInputs[1];
}

static pia6821_interface pia_0 =
{
	input0_read, input1_read, 0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0
};

static UINT8 mcu_coin_read(UINT16)
{
	return portA_out;
}

static void mcu_coin_write(UINT16, UINT8 data)
{
	portA_in = data;
}

static pia6821_interface mcu_pia_0 =
{
	input0_read, mcu_coin_read, 0, 0, 0, 0,
	0, mcu_coin_write, 0, 0,
	0, 0
};

static UINT8 input2_read(UINT16)
{
	return DrvInputs[2];
}

static UINT8 input3_read(UINT16)
{
	return DrvInputs[3];
}

static pia6821_interface pia_1 =
{
	input2_read, input3_read, 0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0
};

static UINT8 input4_read(UINT16)
{
	return DrvInputs[4];
}

static void coins_write(UINT16, UINT8 /*data*/)
{
}

static pia6821_interface pia_2 =
{
	input4_read, 0, 0, 0, 0, 0,
	0, coins_write, 0, 0,
	0, 0
};

static void mcu_coinctrl_write(UINT16, UINT8 data)
{
	m68705SetIrqLine(M68705_IRQ_LINE, (data & 0x04) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	qix_coinctrl = data;
}

static pia6821_interface mcu_pia_2 =
{
	input4_read, 0, 0, 0, 0, 0,
	0, mcu_coinctrl_write, 0, 0,
	0, 0
};

static void sync_pia4_porta_write(UINT16, UINT8 data)
{
	pia_set_input_a(4, data);
}

static void qix_vol_write(UINT16, UINT8 data)
{
	double vol = 1 / ((data < 0x10) ? 0x10 : data);

	DACSetRoute(0, 0.04-vol, BURN_SND_ROUTE_BOTH);
}

static void pia_4_ca1_write(UINT16, UINT8 data)
{
	pia_set_input_ca1(4, data);
}

static void qix_inv_flag_write(UINT16, UINT8 data)
{
	flipscreen = data ? 1 : 0;
}

static void pia_dint(INT32 state)
{
	M6809SetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static pia6821_interface pia_3 =
{
	0, 0, 0, 0, 0, 0,
	sync_pia4_porta_write, qix_vol_write, pia_4_ca1_write, qix_inv_flag_write,
	pia_dint, pia_dint
};

static UINT8 pia_4_porta_read(UINT16)
{
	return pia_read_porta(4);
}

static void pia_3_porta_write(UINT16, UINT8 data)
{
	pia_set_input_a(3, data);
}

static void qix_dac_write(UINT16, UINT8 data)
{
	DACSignedWrite(0, data);
}

static void pia_3_ca1_write(UINT16, UINT8 data)
{
	pia_set_input_ca1(3, data);
}

static void pia_sint(INT32 state)
{
	M6800SetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static pia6821_interface pia_4 =
{
	pia_4_porta_read, 0, 0, 0, 0, 0,
	pia_3_porta_write, qix_dac_write, pia_3_ca1_write, 0,
	pia_sint, pia_sint
};

static pia6821_interface pia_5 =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void mcu_portb_write(UINT8 */*data*/)
{
}

static void mcu_portb_read()
{
	portB_in = (DrvInputs[1] & 0x0f) | ((DrvInputs[1] & 0x80) >> 3);
}

static void mcu_portc_read()
{
	portC_in = (qix_coinctrl & 0x08) | ((DrvInputs[1] & 0x70) >> 4);
}

static m68705_interface zoo_mcu_inf =
{
	NULL, mcu_portb_write, NULL,
	NULL, NULL, NULL,
	NULL, mcu_portb_read, mcu_portc_read
};


static UINT8 trackball_lr_read(UINT16)
{
	return BurnTrackballRead(flipscreen, 0); // flipscreen selects between 1 & 2p
}

static UINT8 trackball_ud_read(UINT16)
{
	return BurnTrackballRead(flipscreen, 1); // flipscreen selects between 1 & 2p
}

static void slither_sn76489_0_write(UINT16, UINT8 data)
{
	SN76496Write(0, data);

	pia_set_input_cb1(1, 0);
	pia_set_input_cb1(1, 1);
}

static void slither_sn76489_1_write(UINT16, UINT8 data)
{
	SN76496Write(1, data);

	pia_set_input_cb1(2, 0);
	pia_set_input_cb1(2, 1);
}

static void slither_coin_write(UINT16, UINT8 /*data*/)
{
}

static pia6821_interface slither_pia_1 =
{
	trackball_ud_read, 0, 0, 0, 0, 0,
	0, slither_sn76489_0_write, 0, 0,
	0, 0
};

static pia6821_interface slither_pia_2 =
{
	trackball_lr_read, 0, 0, 0, 0, 0,
	0, slither_sn76489_1_write, 0, 0,
	0, 0
};

static pia6821_interface slither_pia_3 =
{
	input2_read, 0, 0, 0, 0, 0,
	0, slither_coin_write, 0, qix_inv_flag_write,
	pia_dint, pia_dint
};

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	videoaddress[0] = videoaddress[1] = 0;
	palettebank = 0;
	flipscreen = 0;
	bankaddress = 0;
	qix_coinctrl = 0;
	videoram_mask = 0xff;

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	bankswitch();
	videobank();
	M6809Close();

	M6800Open(0);
	M6800Reset();
	DACReset();
	M6800Close();

	SN76496Reset();

	m67805_taito_reset();

	pia_reset();

	nExtraCycles[0] = nExtraCycles[1] = nExtraCycles[2] = nExtraCycles[3] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0	= Next; Next += 0x010000;
	DrvM6809ROM1	= Next; Next += 0x010000;
	DrvM6802ROM		= Next; Next += 0x010000;
	DrvM68705ROM	= Next; Next += 0x000800;

	DrvPalette		= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000400;

	AllRam			= Next;

	DrvM6809RAM0	= Next; Next += 0x000400;
	DrvM6802RAM		= Next; Next += 0x000100;

	DrvM68705RAM	= Next; Next += 0x000080;

	DrvShareRAM		= Next; Next += 0x000400;
	DrvPalRAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x010000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvRomLoad(INT32 *banked_prg)
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *bankedrom = DrvM6809ROM1;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 0xf) == 1) {
			memmove (DrvM6809ROM0, DrvM6809ROM0 + ri.nLen, 0x10000 - ri.nLen);
			if (BurnLoadRom(DrvM6809ROM0 + (0x10000 - ri.nLen), i, 1)) return 1;
			continue;
		}

		if ((ri.nType & 0xf) == 2) {
			memmove (DrvM6809ROM1 + 0x8000, DrvM6809ROM1 + ri.nLen + 0x8000, 0x8000 - ri.nLen);
			if (BurnLoadRom(DrvM6809ROM1 + (0x10000 - ri.nLen), i, 1)) return 1;
			continue;
		}

		if ((ri.nType & 0xf) == 3) {
			has_soundcpu = 1;
			memmove (DrvM6802ROM, DrvM6802ROM + ri.nLen, 0x10000 - ri.nLen);
			if (BurnLoadRom(DrvM6802ROM + (0x10000 - ri.nLen), i, 1)) return 1;
			continue;
		}
		
		if ((ri.nType & 0xf) == 4) {
			if (BurnLoadRom(DrvM68705ROM, i, 1)) return 1;
			has_mcu = 1;
			continue;
		}

		if ((ri.nType & 0xf) == 5) {
			if (BurnLoadRom(bankedrom, i, 1)) return 1;
			bankedrom += ri.nLen;
			*banked_prg = 1;
			continue;
		}	
	}

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	INT32 banked_prog = 0;
	if (DrvRomLoad(&banked_prog)) return 1;

	bprintf (0, _T("banked: %d, sndcpu: %d, mcu: %d\n"),banked_prog, has_soundcpu, has_mcu);

	BurnSetRefreshRate(55.84);

	M6809Init(0);
	M6809Open(0);
	if (banked_prog) // zookeep
	{
		is_zookeep = 1;

		M6809MapMemory(DrvShareRAM,				0x0000, 0x03ff, MAP_RAM);
		M6809MapMemory(DrvM6809RAM0,			0x0400, 0x07ff, MAP_RAM);
		M6809MapMemory(DrvM6809ROM0 + 0x8000,	0x8000, 0xffff, MAP_ROM);
	}
	else
	{
		M6809MapMemory(DrvShareRAM,				0x8000, 0x83ff, MAP_RAM);
		M6809MapMemory(DrvM6809RAM0,			0x8400, 0x87ff, MAP_RAM);
		M6809MapMemory(DrvM6809ROM0 + 0xa000,	0xa000, 0xffff, MAP_ROM);
	}

	M6809SetWriteHandler(qix_main_write);
	M6809SetReadHandler(qix_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvVidRAM,				0x0000, 0x7fff, MAP_ROM); // banked!!
	M6809MapMemory(DrvShareRAM,				0x8000, 0x83ff, MAP_RAM);
	M6809MapMemory(DrvNVRAM,				0x8400, 0x87ff, MAP_RAM);
	M6809MapMemory(DrvPalRAM,				0x9000, 0x93ff, MAP_ROM); // handler w/recalc
	M6809MapMemory(DrvM6809ROM1 + 0xa000,	0xa000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(qix_video_write);
	M6809SetReadHandler(qix_video_read);
	M6809Close();

	M6800Init(0);
	M6800Open(0);
	M6800MapMemory(DrvM6802RAM, 			0x0000, 0x007f, MAP_RAM);
	M6800MapMemory(DrvM6802ROM + 0xd000,	0xd000, 0xffff, MAP_ROM);
	M6800SetWriteHandler(qix_sound_write);
	M6800SetReadHandler(qix_sound_read);
	M6800Close();

	m67805_taito_init(DrvM68705ROM, DrvM68705RAM, &zoo_mcu_inf);

	pia_init();
	pia_config(0, 0, has_mcu ? &mcu_pia_0 : &pia_0);

	if (has_soundcpu)
	{
		pia_config(1, 0, &pia_1);
		pia_config(2, 0, has_mcu ? & mcu_pia_2 : &pia_2);
		pia_config(3, 0, &pia_3);
	}
	else
	{
		pia_config(1, 0, &slither_pia_1);
		pia_config(2, 0, &slither_pia_2);
		pia_config(3, 0, &slither_pia_3);
	}

	pia_config(4, 0, &pia_4);
	pia_config(5, 0, &pia_5);

	DACInit(0, 0, 0, M6800TotalCycles, 920000);
	DACSetRoute(0, 0.04, BURN_SND_ROUTE_BOTH);

	SN76496Init(0, 1331250, 0); // slither
	SN76496Init(1, 1331250, 1);
	SN76496SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.50, BURN_SND_ROUTE_BOTH);

	BurnTrackballInit(2);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	M6809Exit();
	M6800Exit();
	m67805_taito_exit();
	pia_exit();
	DACExit();
	SN76496Exit();
	BurnTrackballExit();

	has_mcu = 0;
	has_soundcpu = 0;
	has_4way = 0;

	is_slither = 0;
	is_zookeep = 0;

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	const UINT8 table[16] = {
		0x00, 0x12, 0x24, 0x49, 0x12, 0x24, 0x49, 0x92,
		0x5b, 0x6d, 0x92, 0xdb, 0x7f, 0x91, 0xb6, 0xff
	};

	for (INT32 i = 0; i < 0x400; i++)
	{
		UINT8 data = DrvPalRAM[i];

		INT32 intensity = data & 3;
		UINT8 r = table[((data >> 4) & 0xc) | intensity];
		UINT8 g = table[((data >> 2) & 0xc) | intensity];
		UINT8 b = table[((data >> 0) & 0xc) | intensity];

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer(INT32 draw_to)
{
	INT32 flip = flipscreen ? 0xff : 0;
	INT32 color = palettebank * 256;
	INT32 start_y = (256 - nScreenHeight) / 2;

	if (draw_to > nScreenHeight) draw_to = nScreenHeight;

	for (INT32 y = lastline; y < draw_to; y++)
	{
		UINT8 *vram = DrvVidRAM + ((y + start_y) ^ flip) * 256;
		UINT16 *dest = pTransDraw + y * nScreenWidth;

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			dest[x] = vram[x^flip] | color;
		}
	}
}

static void DrvDrawBegin()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	lastline = 0;
}

static void partial_update()
{
	if (!pBurnDraw) return;

	if (scanline < 0 || scanline > nScreenHeight || scanline == lastline || lastline > scanline) return;

	draw_layer(scanline);

	lastline = scanline;
}

static void DrvDrawEnd()
{
	draw_layer(256 + 0x10);

	BurnTransferCopy(DrvPalette);
}

static INT32 DrvDraw() // driver redraw
{
	DrvDrawBegin();

	DrvDrawEnd();

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();
	M6800NewFrame();
	m6805NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}

		if (has_4way) {
			ProcessJoystick(&DrvInputs[0], 0, 0,2,3,1, INPUT_4WAY | INPUT_ISACTIVELOW);
			ProcessJoystick(&DrvInputs[4], 1, 0,2,3,1, INPUT_4WAY | INPUT_ISACTIVELOW);
		}

		if (is_slither) {
			BurnTrackballConfig(0, AXIS_NORMAL, AXIS_REVERSED);
			BurnTrackballConfig(1, AXIS_NORMAL, AXIS_REVERSED);
			BurnTrackballFrame(0, DrvAnalogPort0, DrvAnalogPort1, 2, 3);
			BurnTrackballFrame(1, DrvAnalogPort2, DrvAnalogPort3, 2, 3);
			//	BurnTrackballUDLR(0, DrvJoy2[0], DrvJoy2[1], DrvJoy2[2], DrvJoy2[3]);
			//	BurnTrackballUDLR(0, DrvJoy2[0], DrvJoy2[1], DrvJoy2[2], DrvJoy2[3]);
			BurnTrackballUpdateSlither(0);
			BurnTrackballUpdateSlither(1);
		}
	}

	INT32 nInterleave = 267*8;
	INT32 nCyclesTotal[4] = { (INT32)(1250000 / 55.84), (INT32)(1250000 / 55.84), (INT32)(920000 / 55.84), (INT32)(1000000 / 55.84) };
	INT32 nCyclesDone[4] = { nExtraCycles[0], nExtraCycles[1], nExtraCycles[2], nExtraCycles[3] };

	if (has_soundcpu == 0) {
		nCyclesTotal[0] = 1340000 / 60;
		nCyclesTotal[1] = 1340000 / 60;
	}

	M6800Open(0);
	m6805Open(0);

	DrvDrawBegin();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i / 8;

		M6809Open(0);
		if (i == 0*8) {
			pia_set_input_cb1(3, 0);
		}
		if (i == 248*8) {
			pia_set_input_cb1(3, 1);

			if (pBurnDraw) {
				DrvDrawEnd();
			}
		}
		nCyclesDone[0] += M6809Run(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		M6809Close();

		M6809Open(1);
		nCyclesDone[1] += M6809Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		M6809Close();

		if (has_soundcpu) {
			nCyclesDone[2] += M6800Run(((i + 1) * nCyclesTotal[2] / nInterleave) - nCyclesDone[2]);
		}

		if (has_mcu) {
			nCyclesDone[3] += m6805Run(((i + 1) * nCyclesTotal[3] / nInterleave) - m6805TotalCycles());
		}

		if (scanline - lastline >= 4) {
			partial_update();
		}

		if ((i%(64*8)) == (64*8)-1) {
			BurnTrackballUpdateSlither(0);
			BurnTrackballUpdateSlither(1);
		}
	}

	if (pBurnSoundOut) {
		if (has_soundcpu) {
			DACUpdate(pBurnSoundOut, nBurnSoundLen);
			BurnSoundDCFilter();
		}
		else
		{
			SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
			SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
		}
	}

	m6805Close();
	M6800Close();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];
	nExtraCycles[2] = nCyclesDone[2] - nCyclesTotal[2];
	nExtraCycles[3] = nCyclesDone[3] - nCyclesTotal[3];

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);
		M6800Scan(nAction);
		m68705_taito_scan(nAction);
		pia_scan(nAction, pnMin);
		DACScan(nAction, pnMin);
		SN76496Scan(nAction, pnMin);

		if (is_slither)
			BurnTrackballScan();

		SCAN_VAR(videoaddress);
		SCAN_VAR(palettebank);
		SCAN_VAR(flipscreen);
		SCAN_VAR(bankaddress);
		SCAN_VAR(qix_coinctrl);
		SCAN_VAR(videoram_mask);
		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00400;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE)
	{
		M6809Open(1);
		bankswitch();
		videobank();
		M6809Close();
	}

	return 0;
}


static INT32 FourWayInit()
{
	has_4way = 1;

	return DrvInit();
}

// Qix (Rev 2)

static struct BurnRomInfo qixRomDesc[] = {
	{ "qq12_rev2.u12",	0x0800, 0xaad35508, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "qq13_rev2.u13",	0x0800, 0x46c13504, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qq14_rev2.u14",	0x0800, 0x5115e896, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "qq15_rev2.u15",	0x0800, 0xccd52a1b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "qq16_rev2.u16",	0x0800, 0xcd1c36ee, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "qq17_rev2.u17",	0x0800, 0x1acb682d, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "qq18_rev2.u18",	0x0800, 0xde77728b, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "qq19_rev2.u19",	0x0800, 0xc0994776, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "qq4_rev2.u4",	0x0800, 0x5b906a09, 2 | BRF_PRG | BRF_ESS }, //  8 Video M6809 Code
	{ "qq5_rev2.u5",	0x0800, 0x254a3587, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "qq6_rev2.u6",	0x0800, 0xace30389, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "qq7_rev2.u7",	0x0800, 0x8ebcfa7c, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "qq8_rev2.u8",	0x0800, 0xb8a3c8f9, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "qq9_rev2.u9",	0x0800, 0x26cbcd55, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "qq10_rev2.u10",	0x0800, 0x568be942, 2 | BRF_PRG | BRF_ESS }, // 14

	{ "qq27.u27",		0x0800, 0xf3782bd0, 3 | BRF_PRG | BRF_ESS }, // 15 Sound M6802 Code
};

STD_ROM_PICK(qix)
STD_ROM_FN(qix)

struct BurnDriver BurnDrvQix = {
	"qix", NULL, NULL, NULL, "1981",
	"Qix (Rev 2)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, qixRomInfo, qixRomName, NULL, NULL, NULL, NULL, QixInputInfo, NULL,
	FourWayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};

// Qix (set 2, smaller roms)

static struct BurnRomInfo qixaRomDesc[] = {
	{ "u12_",			0x0800, 0x5adc046d, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "u13_",			0x0800, 0xe63283c6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u14_",			0x0800, 0xa2fd4c28, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "u15_",			0x0800, 0x0e5e17d6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "u16_",			0x0800, 0x6acfa3b8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "u17_",			0x0800, 0xbc091f21, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "u18_",			0x0800, 0x610b19ce, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "u19_",			0x0800, 0x11f957f4, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "u3_",			0x0800, 0x79cf997c, 2 | BRF_PRG | BRF_ESS }, //  8 Video M6809 Code
	{ "u4_",			0x0800, 0xe5ee74cd, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "u5_",			0x0800, 0x4e939d87, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "u6_",			0x0800, 0xb42ca7d8, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "u7_",			0x0800, 0xd6733019, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "u8_",			0x0800, 0x8cab8fb2, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "u9_",			0x0800, 0x98fbac76, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "u10_",			0x0800, 0xb40f084a, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "qq27.u27",		0x0800, 0xf3782bd0, 3 | BRF_PRG | BRF_ESS }, // 16 Sound M6802 Code
};

STD_ROM_PICK(qixa)
STD_ROM_FN(qixa)

struct BurnDriver BurnDrvQixa = {
	"qixa", "qix", NULL, NULL, "1981",
	"Qix (set 2, smaller roms)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, qixaRomInfo, qixaRomName, NULL, NULL, NULL, NULL, QixInputInfo, NULL,
	FourWayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Qix (set 2, larger roms)

static struct BurnRomInfo qixbRomDesc[] = {
	{ "lk14.bin",		0x1000, 0x6d164986, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "lk15.bin",		0x1000, 0x16c6ce0f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "lk16.bin",		0x1000, 0x698b1f9c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "lk17.bin",		0x1000, 0x7e3adde6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "lk10.bin",		0x1000, 0x7eac67d0, 2 | BRF_PRG | BRF_ESS }, //  4 Video M6809 Code
	{ "lk11.bin",		0x1000, 0x90ccbb6a, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "lk12.bin",		0x1000, 0xbe9b9f7d, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "lk13.bin",		0x1000, 0x51c9853b, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "qq27.u27",		0x0800, 0xf3782bd0, 3 | BRF_PRG | BRF_ESS }, //  8 Sound M6802 Code
};

STD_ROM_PICK(qixb)
STD_ROM_FN(qixb)

struct BurnDriver BurnDrvQixb = {
	"qixb", "qix", NULL, NULL, "1981",
	"Qix (set 2, larger roms)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, qixbRomInfo, qixbRomName, NULL, NULL, NULL, NULL, QixInputInfo, NULL,
	FourWayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Qix (set 3, earlier)

static struct BurnRomInfo qixoRomDesc[] = {
	{ "qu12",			0x0800, 0x1c55b44d, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "qu13",			0x0800, 0x20279e8c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qu14",			0x0800, 0xbafe3ce3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "qu16",			0x0800, 0xdb560753, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "qu17",			0x0800, 0x8c7aeed8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "qu18",			0x0800, 0x353be980, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "qu19",			0x0800, 0xf46a69ca, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "qu3",			0x0800, 0x8b4c0ef0, 2 | BRF_PRG | BRF_ESS }, //  7 Video M6809 Code
	{ "qu4",			0x0800, 0x66a5c260, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "qu5",			0x0800, 0x70160ea3, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "qu7",			0x0800, 0xd6733019, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "qu8",			0x0800, 0x66870dcc, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "qu9",			0x0800, 0xc99bf94d, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "qu10",			0x0800, 0x88b45037, 2 | BRF_PRG | BRF_ESS }, // 13

	{ "qu27.u27",		0x0800, 0xf3782bd0, 3 | BRF_PRG | BRF_ESS }, // 14 Sound M6802 Code
};

STD_ROM_PICK(qixo)
STD_ROM_FN(qixo)

static INT32 QixoInit()
{
	INT32 rc = FourWayInit();

	if (!rc) {
		if (BurnLoadRom(DrvM6809ROM0 + 0xc000, 0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0xc800, 1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0xd000, 2, 1)) return 1;
		// empty space (d800 - dfff)
		memset(DrvM6809ROM0 + 0xd800, 0, 0x800); // previous loader wrote stuff here.
		if (BurnLoadRom(DrvM6809ROM0 + 0xe000, 3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0xe800, 4, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0xf000, 5, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0xf800, 6, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0xc000, 7, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0xc800, 8, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0xd000, 9, 1)) return 1;
		// empty space (d800 - dfff)
		memset(DrvM6809ROM1 + 0xd800, 0, 0x800);
		if (BurnLoadRom(DrvM6809ROM1 + 0xe000,10, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0xe800,11, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0xf000,12, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0xf800,13, 1)) return 1;
	}

	return rc;
}

struct BurnDriver BurnDrvQixo = {
	"qixo", "qix", NULL, NULL, "1981",
	"Qix (set 3, earlier)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, qixoRomInfo, qixoRomName, NULL, NULL, NULL, NULL, QixInputInfo, NULL,
	QixoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Qix II (Tournament)

static struct BurnRomInfo qix2RomDesc[] = {
	{ "u12.rmb",		0x0800, 0x484280fd, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "u13.rmb",		0x0800, 0x3d089fcb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u14.rmb",		0x0800, 0x362123a9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "u15.rmb",		0x0800, 0x60f3913d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "u16.rmb",		0x0800, 0xcc139e34, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "u17.rmb",		0x0800, 0xcf31dc49, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "u18.rmb",		0x0800, 0x1f91ed7a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "u19.rmb",		0x0800, 0x68e8d5a6, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "u3.rmb",			0x0800, 0x19cebaca, 2 | BRF_PRG | BRF_ESS }, //  8 Video M6809 Code
	{ "u4.rmb",			0x0800, 0x6cfb4185, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "u5.rmb",			0x0800, 0x948f53f3, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "u6.rmb",			0x0800, 0x8630120e, 2 | BRF_PRG | BRF_ESS }, // 11
	{ "u7.rmb",			0x0800, 0xbad037c9, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "u8.rmb",			0x0800, 0x3159bc00, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "u9.rmb",			0x0800, 0xe80e9b1d, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "u10.rmb",		0x0800, 0x9a55d360, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "u27",			0x0800, 0xf3782bd0, 3 | BRF_PRG | BRF_ESS }, // 16 Sound M6802 Code
};

STD_ROM_PICK(qix2)
STD_ROM_FN(qix2)

struct BurnDriver BurnDrvQix2 = {
	"qix2", "qix", NULL, NULL, "1981",
	"Qix II (Tournament)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, qix2RomInfo, qix2RomName, NULL, NULL, NULL, NULL, QixInputInfo, NULL,
	FourWayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Zoo Keeper (set 1)

static struct BurnRomInfo zookeepRomDesc[] = {
	{ "za12.u12",		0x1000, 0x4e40d8dc, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "za13.u13",		0x1000, 0xeebd5248, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "za14.u14",		0x1000, 0xfab43297, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "za15.u15",		0x1000, 0xef8cd67c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "za16.u16",		0x1000, 0xccfc15bc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "za17.u17",		0x1000, 0x358013f4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "za18.u18",		0x1000, 0x37886afe, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "za19.u19",		0x1000, 0xbbfb30d9, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "za5.u5",			0x1000, 0xdc0c3cbd, 2 | BRF_PRG | BRF_ESS }, //  8 Video M6809 Code
	{ "za3.u3",			0x1000, 0xcc4d0aee, 5 | BRF_PRG | BRF_ESS }, //  9
	{ "za6.u6",			0x1000, 0x27c787dd, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "za4.u4",			0x1000, 0xec3b10b1, 5 | BRF_PRG | BRF_ESS }, // 11
	{ "za7.u7",			0x1000, 0x1479f480, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "za8.u8",			0x1000, 0x4c96cdb2, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "za9.u9",			0x1000, 0xa4f7d9e0, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "za10.u10",		0x1000, 0x05df1a5a, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "za25.u25",		0x1000, 0x779b8558, 3 | BRF_PRG | BRF_ESS }, // 16 Sound M6802 Code
	{ "za26.u26",		0x1000, 0x60a810ce, 3 | BRF_PRG | BRF_ESS }, // 17
	{ "za27.u27",		0x1000, 0x99ed424e, 3 | BRF_PRG | BRF_ESS }, // 18

	{ "za_coin.bin",	0x0800, 0x364d3557, 4 | BRF_PRG | BRF_ESS }, // 19 MCU (M68705) Code
};

STD_ROM_PICK(zookeep)
STD_ROM_FN(zookeep)

struct BurnDriver BurnDrvZookeep = {
	"zookeep", NULL, NULL, NULL, "1982",
	"Zoo Keeper (set 1)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, zookeepRomInfo, zookeepRomName, NULL, NULL, NULL, NULL, ZookeepInputInfo, NULL,
	FourWayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Zoo Keeper (set 2)

static struct BurnRomInfo zookeep2RomDesc[] = {
	{ "za12.u12",		0x1000, 0x4e40d8dc, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "za13.u13",		0x1000, 0xeebd5248, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "za14.u14",		0x1000, 0xfab43297, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "za15.u15",		0x1000, 0xef8cd67c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "za16.u16",		0x1000, 0xccfc15bc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "za17.u17",		0x1000, 0x358013f4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "za18.u18",		0x1000, 0x37886afe, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "za19.red.u19",	0x1000, 0xec01760e, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "za5.u5",			0x1000, 0xdc0c3cbd, 2 | BRF_PRG | BRF_ESS }, //  8 Video M6809 Code
	{ "za3.u3",			0x1000, 0xcc4d0aee, 5 | BRF_PRG | BRF_ESS }, //  9
	{ "za6.u6",			0x1000, 0x27c787dd, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "za4.u4",			0x1000, 0xec3b10b1, 5 | BRF_PRG | BRF_ESS }, // 11
	{ "za7.u7",			0x1000, 0x1479f480, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "za8.u8",			0x1000, 0x4c96cdb2, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "za9.u9",			0x1000, 0xa4f7d9e0, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "za10.u10",		0x1000, 0x05df1a5a, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "za25.u25",		0x1000, 0x779b8558, 3 | BRF_PRG | BRF_ESS }, // 16 Sound M6802 Code
	{ "za26.u26",		0x1000, 0x60a810ce, 3 | BRF_PRG | BRF_ESS }, // 17
	{ "za27.u27",		0x1000, 0x99ed424e, 3 | BRF_PRG | BRF_ESS }, // 18

	{ "za_coin.bin",	0x0800, 0x364d3557, 4 | BRF_PRG | BRF_ESS }, // 19 MCU (M68705) Code
};

STD_ROM_PICK(zookeep2)
STD_ROM_FN(zookeep2)

struct BurnDriver BurnDrvZookeep2 = {
	"zookeep2", "zookeep", NULL, NULL, "1982",
	"Zoo Keeper (set 2)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, zookeep2RomInfo, zookeep2RomName, NULL, NULL, NULL, NULL, ZookeepInputInfo, NULL,
	FourWayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Zoo Keeper (set 3)

static struct BurnRomInfo zookeep3RomDesc[] = {
	{ "za12.u12",		0x1000, 0x4e40d8dc, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "za13.u13",		0x1000, 0xeebd5248, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "za14.u14",		0x1000, 0xfab43297, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "za15.u15",		0x1000, 0xef8cd67c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "za16.u16",		0x1000, 0xccfc15bc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "za17.u17",		0x1000, 0x358013f4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "za18.u18",		0x1000, 0x37886afe, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "za19.u19",		0x1000, 0xbbfb30d9, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "za5.u5",			0x1000, 0xdc0c3cbd, 2 | BRF_PRG | BRF_ESS }, //  8 Video M6809 Code
	{ "za3.u3",			0x1000, 0xcc4d0aee, 5 | BRF_PRG | BRF_ESS }, //  9
	{ "za6.u6",			0x1000, 0x27c787dd, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "za4.u4",			0x1000, 0xec3b10b1, 5 | BRF_PRG | BRF_ESS }, // 11
	{ "za7.u7",			0x1000, 0x1479f480, 2 | BRF_PRG | BRF_ESS }, // 12
	{ "za8.u8",			0x1000, 0x4c96cdb2, 2 | BRF_PRG | BRF_ESS }, // 13
	{ "zv35.9.u9",		0x1000, 0xd14123b7, 2 | BRF_PRG | BRF_ESS }, // 14
	{ "zv36.10.u10",	0x1000, 0x23705777, 2 | BRF_PRG | BRF_ESS }, // 15

	{ "za25.u25",		0x1000, 0x779b8558, 3 | BRF_PRG | BRF_ESS }, // 16 Sound M6802 Code
	{ "za26.u26",		0x1000, 0x60a810ce, 3 | BRF_PRG | BRF_ESS }, // 17
	{ "za27.u27",		0x1000, 0x99ed424e, 3 | BRF_PRG | BRF_ESS }, // 18

	{ "za_coin.bin",	0x0800, 0x364d3557, 4 | BRF_PRG | BRF_ESS }, // 19 MCU (M68705) Code
};

STD_ROM_PICK(zookeep3)
STD_ROM_FN(zookeep3)

struct BurnDriver BurnDrvZookeep3 = {
	"zookeep3", "zookeep", NULL, NULL, "1982",
	"Zoo Keeper (set 3)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, zookeep3RomInfo, zookeep3RomName, NULL, NULL, NULL, NULL, ZookeepInputInfo, NULL,
	FourWayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Space Dungeon

static struct BurnRomInfo sdungeonRomDesc[] = {
	{ "sd14.u14",		0x1000, 0x7024b55a, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "sd15.u15",		0x1000, 0xa3ac9040, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sd16.u16",		0x1000, 0xcc20b580, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sd17.u17",		0x1000, 0x4663e4b8, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sd18.u18",		0x1000, 0x7ef1ffc0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sd19.u19",		0x1000, 0x7b20b7ac, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "sd05.u5",		0x1000, 0x0b2bf48e, 2 | BRF_PRG | BRF_ESS }, //  6 Video M6809 Code
	{ "sd06.u6",		0x1000, 0xf86db512, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "sd07.u7",		0x1000, 0x7b796831, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "sd08.u8",		0x1000, 0x5fbe7068, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "sd09.u9",		0x1000, 0x89bc51ea, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "sd10.u10",		0x1000, 0x754de734, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "sd26.u26",		0x0800, 0x3df8630d, 3 | BRF_PRG | BRF_ESS }, // 12 Sound M6802 Code
	{ "sd27.u27",		0x0800, 0x0386f351, 3 | BRF_PRG | BRF_ESS }, // 13

	{ "sd101",			0x0800, 0xe255af9a, 4 | BRF_PRG | BRF_ESS }, // 14 MCU (M68705) Code
};

STD_ROM_PICK(sdungeon)
STD_ROM_FN(sdungeon)

struct BurnDriver BurnDrvSdungeon = {
	"sdungeon", NULL, NULL, NULL, "1981",
	"Space Dungeon\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 4, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, sdungeonRomInfo, sdungeonRomName, NULL, NULL, NULL, NULL, SdungeonInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Space Dungeon (larger roms)

static struct BurnRomInfo sdungeonaRomDesc[] = {
	{ "1514.1j",		0x2000, 0x8fe3e3c6, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "1716.1k",		0x2000, 0x9976fe14, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1819.1l",		0x2000, 0xd56a40b0, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "65.1h",			0x2000, 0x4b567837, 2 | BRF_PRG | BRF_ESS }, //  3 Video M6809 Code
	{ "87.1f",			0x2000, 0x9298778b, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "109.1e",			0x2000, 0x7437a6f1, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "2627.2d",		0x1000, 0x1f830dff, 3 | BRF_PRG | BRF_ESS }, //  6 Sound M6802 Code

	{ "sd101",			0x0800, 0xe255af9a, 4 | BRF_PRG | BRF_ESS }, //  7 MCU (M68705) Code
};

STD_ROM_PICK(sdungeona)
STD_ROM_FN(sdungeona)

struct BurnDriver BurnDrvSdungeona = {
	"sdungeona", "sdungeon", NULL, NULL, "1981",
	"Space Dungeon (larger roms)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 4, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, sdungeonaRomInfo, sdungeonaRomName, NULL, NULL, NULL, NULL, SdungeonInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// The Electric Yo-Yo (set 1)

static struct BurnRomInfo elecyoyoRomDesc[] = {
	{ "yy14",			0x1000, 0x0d2edcb9, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "yy15",			0x1000, 0xa91f01e3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "yy16-1",			0x1000, 0x2710f360, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "yy17",			0x1000, 0x25fd489d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "yy18",			0x1000, 0x0b6661c0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "yy19-1",			0x1000, 0x95b8b244, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "yy5",			0x1000, 0x3793fec5, 2 | BRF_PRG | BRF_ESS }, //  6 Video M6809 Code
	{ "yy6",			0x1000, 0x2e8b1265, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "yy7",			0x1000, 0x20f93411, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "yy8",			0x1000, 0x926f90c8, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "yy9",			0x1000, 0x2f999480, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "yy10",			0x1000, 0xb31d20e2, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "yy27",			0x0800, 0x5a2aa0f3, 3 | BRF_PRG | BRF_ESS }, // 12 Sound M6802 Code

	{ "yy101",			0x0800, 0x3cf13038, 4 | BRF_PRG | BRF_ESS }, // 13 MCU (M68705) Code
};

STD_ROM_PICK(elecyoyo)
STD_ROM_FN(elecyoyo)

struct BurnDriver BurnDrvElecyoyo = {
	"elecyoyo", NULL, NULL, NULL, "1982",
	"The Electric Yo-Yo (set 1)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, elecyoyoRomInfo, elecyoyoRomName, NULL, NULL, NULL, NULL, ElecyoyoInputInfo, NULL,
	FourWayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// The Electric Yo-Yo (set 2)

static struct BurnRomInfo elecyoyo2RomDesc[] = {
	{ "yy14",			0x1000, 0x0d2edcb9, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "yy15",			0x1000, 0xa91f01e3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "yy16",			0x1000, 0xcab19f3a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "yy17",			0x1000, 0x25fd489d, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "yy18",			0x1000, 0x0b6661c0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "yy19",			0x1000, 0xd0215d2e, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "yy5",			0x1000, 0x3793fec5, 2 | BRF_PRG | BRF_ESS }, //  6 Video M6809 Code
	{ "yy6",			0x1000, 0x2e8b1265, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "yy7",			0x1000, 0x20f93411, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "yy8",			0x1000, 0x926f90c8, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "yy9",			0x1000, 0x2f999480, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "yy10",			0x1000, 0xb31d20e2, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "yy27",			0x0800, 0x5a2aa0f3, 3 | BRF_PRG | BRF_ESS }, // 12 Sound M6802 Code

	{ "yy101",			0x0800, 0x3cf13038, 4 | BRF_PRG | BRF_ESS }, // 13 MCU (M68705) Code
};

STD_ROM_PICK(elecyoyo2)
STD_ROM_FN(elecyoyo2)

struct BurnDriver BurnDrvElecyoyo2 = {
	"elecyoyo2", "elecyoyo", NULL, NULL, "1982",
	"The Electric Yo-Yo (set 2)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, elecyoyo2RomInfo, elecyoyo2RomName, NULL, NULL, NULL, NULL, ElecyoyoInputInfo, NULL,
	FourWayInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Kram (set 1)

static struct BurnRomInfo kramRomDesc[] = {
	{ "ks14-1",			0x1000, 0xfe69ac79, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "ks15",			0x1000, 0x4b2c175e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ks16",			0x1000, 0x9500a05d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ks17",			0x1000, 0xc752a3a1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ks18",			0x1000, 0x79158b03, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ks19-1",			0x1000, 0x759ea6ce, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ks5",			0x1000, 0x1c472080, 2 | BRF_PRG | BRF_ESS }, //  6 Video M6809 Code
	{ "ks6",			0x1000, 0xb8926622, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "ks7",			0x1000, 0xc98a7485, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "ks8",			0x1000, 0x1127c4e4, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "ks9",			0x1000, 0xd3bc8b5e, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "ks10",			0x1000, 0xe0426444, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "ks27",			0x0800, 0xc46530c8, 3 | BRF_PRG | BRF_ESS }, // 12 Sound M6802 Code

	{ "ks101.dat",		0x0800, 0xe53d97b7, 4 | BRF_PRG | BRF_ESS }, // 13 MCU (M68705) Code
};

STD_ROM_PICK(kram)
STD_ROM_FN(kram)

struct BurnDriver BurnDrvKram = {
	"kram", NULL, NULL, NULL, "1982",
	"Kram (set 1)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, kramRomInfo, kramRomName, NULL, NULL, NULL, NULL, KramInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Kram (set 2)

static struct BurnRomInfo kram2RomDesc[] = {
	{ "ks14",			0x1000, 0xa2eac1ff, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "ks15",			0x1000, 0x4b2c175e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ks16",			0x1000, 0x9500a05d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ks17",			0x1000, 0xc752a3a1, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ks18",			0x1000, 0x79158b03, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "ks19",			0x1000, 0x053c5e09, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "ks5",			0x1000, 0x1c472080, 2 | BRF_PRG | BRF_ESS }, //  6 Video M6809 Code
	{ "ks6",			0x1000, 0xb8926622, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "ks7",			0x1000, 0xc98a7485, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "ks8",			0x1000, 0x1127c4e4, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "ks9",			0x1000, 0xd3bc8b5e, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "ks10",			0x1000, 0xe0426444, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "ks27",			0x0800, 0xc46530c8, 3 | BRF_PRG | BRF_ESS }, // 12 Sound M6802 Code

	{ "ks101.dat",		0x0800, 0xe53d97b7, 4 | BRF_PRG | BRF_ESS }, // 13 MCU (M68705) Code
};

STD_ROM_PICK(kram2)
STD_ROM_FN(kram2)

struct BurnDriver BurnDrvKram2 = {
	"kram2", "kram", NULL, NULL, "1982",
	"Kram (set 2)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, kram2RomInfo, kram2RomName, NULL, NULL, NULL, NULL, KramInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Kram (encrypted)

static struct BurnRomInfo kram3RomDesc[] = {
	{ "kr-u14",			0x1000, 0x02c1bd1e, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "kr-u15",			0x1000, 0x46b3ff33, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kr-u16",			0x1000, 0xf202b9cf, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kr-u17",			0x1000, 0x257cea23, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "kr-u18",			0x1000, 0xda3aed8c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "kr-u19",			0x1000, 0x496ab571, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "kr-u5",			0x1000, 0x9e63c2bc, 2 | BRF_PRG | BRF_ESS }, //  6 Video M6809 Code
	{ "kr-u6",			0x1000, 0xa0ff1244, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "kr-u7",			0x1000, 0x20a15024, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "ks8",			0x1000, 0x1127c4e4, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "ks9",			0x1000, 0xd3bc8b5e, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "kr-u10",			0x1000, 0x0a8adbd8, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "ks27",			0x0800, 0xc46530c8, 3 | BRF_PRG | BRF_ESS }, // 12 Sound M6802 Code
};

STD_ROM_PICK(kram3)
STD_ROM_FN(kram3)

struct BurnDriverX BurnDrvKram3 = {
	"kram3", "kram", NULL, NULL, "1982",
	"Kram (encrypted)\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, kram3RomInfo, kram3RomName, NULL, NULL, NULL, NULL, KramInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Complex X

static struct BurnRomInfo complexxRomDesc[] = {
	{ "cx14.bin",		0x1000, 0xf123a0de, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "cx15.bin",		0x1000, 0x0fcb966f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cx16.bin",		0x1000, 0xaa11e0e3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cx17.bin",		0x1000, 0xf610856e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cx18.bin",		0x1000, 0x8f8c3984, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cx19.bin",		0x1000, 0x13af3ba8, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "cx5.bin",		0x1000, 0x62a2b87b, 2 | BRF_PRG | BRF_ESS }, //  6 Video M6809 Code
	{ "cx6.bin",		0x1000, 0xdfa7c088, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "cx7.bin",		0x1000, 0xc8bd6759, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "cx8.bin",		0x1000, 0x14a57221, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "cx9.bin",		0x1000, 0xfc2d4a9f, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "cx10.bin",		0x1000, 0x96e0c1ad, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "cx26.bin",		0x1000, 0xf4f19c9b, 3 | BRF_PRG | BRF_ESS }, // 12 Sound M6802 Code
	{ "cx27.bin",		0x1000, 0x7e177328, 3 | BRF_PRG | BRF_ESS }, // 13
};

STD_ROM_PICK(complexx)
STD_ROM_FN(complexx)

struct BurnDriver BurnDrvComplexx = {
	"complexx", NULL, NULL, NULL, "1984",
	"Complex X\0", NULL, "Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, complexxRomInfo, complexxRomName, NULL, NULL, NULL, NULL, ComplexxInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};


// Slither (set 1)

static struct BurnRomInfo slitherRomDesc[] = {
	{ "u31.cpu",		0x0800, 0x3dfff970, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "u30.cpu",		0x0800, 0x8cbc5af8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u29.cpu",		0x0800, 0x98c14510, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "u28.cpu",		0x0800, 0x2762f612, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "u27.cpu",		0x0800, 0x9306d5b1, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "u41.cpu",		0x0800, 0xe4c60a57, 2 | BRF_PRG | BRF_ESS }, //  5 Video M6809 Code
	{ "u40.cpu",		0x0800, 0x5dcec622, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "u39.cpu",		0x0800, 0x69829c2a, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "u38.cpu",		0x0800, 0x6adc64c6, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "u37.cpu",		0x0800, 0x55d31c96, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "u36.cpu",		0x0800, 0xd5ffc013, 2 | BRF_PRG | BRF_ESS }, // 10
};

STD_ROM_PICK(slither)
STD_ROM_FN(slither)

static INT32 SlitherInit()
{
	is_slither = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvSlither = {
	"slither", NULL, NULL, NULL, "1982",
	"Slither (set 1)\0", NULL, "Century II", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, slitherRomInfo, slitherRomName, NULL, NULL, NULL, NULL, SlitherInputInfo, NULL,
	SlitherInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 256, 3, 4
};


// Slither (set 2)

static struct BurnRomInfo slitheraRomDesc[] = {
	{ "u31.cpu",		0x0800, 0x3dfff970, 1 | BRF_PRG | BRF_ESS }, //  0 Main M6809 Code
	{ "u30.cpu",		0x0800, 0x8cbc5af8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "u29.cpu",		0x0800, 0x98c14510, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "u28.cpu",		0x0800, 0x2762f612, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "u27.cpu",		0x0800, 0x9306d5b1, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "u41.cpu",		0x0800, 0xe4c60a57, 2 | BRF_PRG | BRF_ESS }, //  5 Video M6809 Code
	{ "u40.cpu",		0x0800, 0x5dcec622, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "u39.cpu",		0x0800, 0x69829c2a, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "u38a.cpu",		0x0800, 0x423adfef, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "u37.cpu",		0x0800, 0x55d31c96, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "u36a.cpu",		0x0800, 0x5ac4e244, 2 | BRF_PRG | BRF_ESS }, // 10
};

STD_ROM_PICK(slithera)
STD_ROM_FN(slithera)

struct BurnDriver BurnDrvSlithera = {
	"slithera", "slither", NULL, NULL, "1982",
	"Slither (set 2)\0", NULL, "Century II", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, slitheraRomInfo, slitheraRomName, NULL, NULL, NULL, NULL, SlitherInputInfo, NULL,
	SlitherInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 256, 3, 4
};
