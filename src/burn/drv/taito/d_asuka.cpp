// FB Neo Asuka & Asuka driver module
// Based on MAME driver by David Graves and Brian Troha

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "burn_ym2610.h"
#include "msm5205.h"
#include "taito_ic.h"
#include "taito.h"
#include "upd7810_intf.h"

static UINT8 TaitoInputConfig;

static INT32 AsukaADPCMPos;
static INT32 AsukaADPCMData;
static HoldCoin<2> hold_coin;

static INT32 nCyclesExtra[3];

static struct BurnInputInfo CadashInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	TC0220IOCInputPort2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	TC0220IOCInputPort2 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	TC0220IOCInputPort0 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	TC0220IOCInputPort0 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	TC0220IOCInputPort0 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	TC0220IOCInputPort0 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	TC0220IOCInputPort0 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	TC0220IOCInputPort0 + 2,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	TC0220IOCInputPort2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	TC0220IOCInputPort2 + 2,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	TC0220IOCInputPort1 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	TC0220IOCInputPort1 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	TC0220IOCInputPort1 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	TC0220IOCInputPort1 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	TC0220IOCInputPort1 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	TC0220IOCInputPort1 + 2,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&TaitoReset,				"reset"		},
	{"Service",			BIT_DIGITAL,	TC0220IOCInputPort2 + 4,	"service"	},
	{"Tilt",			BIT_DIGITAL,	TC0220IOCInputPort2 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	TC0220IOCDip + 0,			"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	TC0220IOCDip + 1,			"dip"		},
};

STDINPUTINFO(Cadash)

static struct BurnInputInfo AsukaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	TC0220IOCInputPort2 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	TC0220IOCInputPort2 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	TC0220IOCInputPort0 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	TC0220IOCInputPort0 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	TC0220IOCInputPort0 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	TC0220IOCInputPort0 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	TC0220IOCInputPort0 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	TC0220IOCInputPort0 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	TC0220IOCInputPort2 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	TC0220IOCInputPort2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	TC0220IOCInputPort1 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	TC0220IOCInputPort1 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	TC0220IOCInputPort1 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	TC0220IOCInputPort1 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	TC0220IOCInputPort1 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	TC0220IOCInputPort1 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&TaitoReset,				"reset"		},
	{"Service",			BIT_DIGITAL,	TC0220IOCInputPort2 + 1,	"service"	},
	{"Tilt",			BIT_DIGITAL,	TC0220IOCInputPort2 + 0,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	TC0220IOCDip + 0,			"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	TC0220IOCDip + 1,			"dip"		},
};

STDINPUTINFO(Asuka)

static struct BurnInputInfo BonzeadvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	TaitoInputPort1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	TaitoInputPort0 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	TaitoInputPort2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	TaitoInputPort2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	TaitoInputPort2 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	TaitoInputPort2 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	TaitoInputPort2 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	TaitoInputPort2 + 7,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	TaitoInputPort1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	TaitoInputPort0 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	TaitoInputPort3 + 1,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	TaitoInputPort3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	TaitoInputPort3 + 7,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	TaitoInputPort3 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	TaitoInputPort3 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	TaitoInputPort3 + 6,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&TaitoReset,			"reset"		},
	{"Service",			BIT_DIGITAL,	TaitoInputPort0 + 7,	"service"	},
	{"Tilt",			BIT_DIGITAL,	TaitoInputPort2 + 0,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	TaitoDip + 0,			"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	TaitoDip + 1,			"dip"		},
};

STDINPUTINFO(Bonzeadv)

static struct BurnDIPInfo CadashDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Starting Time"	},
	{0x14, 0x01, 0x0c, 0x00, "5:00"			},
	{0x14, 0x01, 0x0c, 0x04, "6:00"			},
	{0x14, 0x01, 0x0c, 0x0c, "7:00"			},
	{0x14, 0x01, 0x0c, 0x08, "8:00"			},

	{0   , 0xfe, 0   ,    4, "Add Time (after round clear)"		},
	{0x14, 0x01, 0x30, 0x00, "Default - 2:00"	},
	{0x14, 0x01, 0x30, 0x10, "Default - 1:00"	},
	{0x14, 0x01, 0x30, 0x30, "Default"		},
	{0x14, 0x01, 0x30, 0x20, "Default + 1:00"	},

	{0   , 0xfe, 0   ,    3, "Communication Mode"	},
	{0x14, 0x01, 0xc0, 0xc0, "Stand alone"		},
	{0x14, 0x01, 0xc0, 0x80, "Master"		},
	{0x14, 0x01, 0xc0, 0x00, "Slave"		},
};

STDDIPINFO(Cadash)

static struct BurnDIPInfo CadashjDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coins 2 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "2 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Starting Time"	},
	{0x14, 0x01, 0x0c, 0x00, "5:00"			},
	{0x14, 0x01, 0x0c, 0x04, "6:00"			},
	{0x14, 0x01, 0x0c, 0x0c, "7:00"			},
	{0x14, 0x01, 0x0c, 0x08, "8:00"			},

	{0   , 0xfe, 0   ,    4, "Add Time (after round clear)"		},
	{0x14, 0x01, 0x30, 0x00, "Default - 2:00"	},
	{0x14, 0x01, 0x30, 0x10, "Default - 1:00"	},
	{0x14, 0x01, 0x30, 0x30, "Default"		},
	{0x14, 0x01, 0x30, 0x20, "Default + 1:00"	},

	{0   , 0xfe, 0   ,    3, "Communication Mode"	},
	{0x14, 0x01, 0xc0, 0xc0, "Stand alone"		},
	{0x14, 0x01, 0xc0, 0x80, "Master"		},
	{0x14, 0x01, 0xc0, 0x00, "Slave"		},
};

STDDIPINFO(Cadashj)

static struct BurnDIPInfo CadashuDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Price to Continue"	},
	{0x13, 0x01, 0xc0, 0xc0, "Same as Start"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "2 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "3 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Starting Time"	},
	{0x14, 0x01, 0x0c, 0x00, "5:00"			},
	{0x14, 0x01, 0x0c, 0x04, "6:00"			},
	{0x14, 0x01, 0x0c, 0x0c, "7:00"			},
	{0x14, 0x01, 0x0c, 0x08, "8:00"			},

	{0   , 0xfe, 0   ,    4, "Add Time (after round clear)"},
	{0x14, 0x01, 0x30, 0x00, "Default - 2:00"	},
	{0x14, 0x01, 0x30, 0x10, "Default - 1:00"	},
	{0x14, 0x01, 0x30, 0x30, "Default"		},
	{0x14, 0x01, 0x30, 0x20, "Default + 1:00"	},

	{0   , 0xfe, 0   ,    3, "Communication Mode"	},
	{0x14, 0x01, 0xc0, 0xc0, "Stand alone"		},
	{0x14, 0x01, 0xc0, 0x80, "Master"		},
	{0x14, 0x01, 0xc0, 0x00, "Slave"		},
};

STDDIPINFO(Cadashu)


static struct BurnDIPInfo EtoDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0x03, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},
};

STDDIPINFO(Eto)

static struct BurnDIPInfo AsukaDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xbf, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Points"		},
	{0x14, 0x01, 0x0c, 0x0c, "500"			},
	{0x14, 0x01, 0x0c, 0x08, "1500"			},
	{0x14, 0x01, 0x0c, 0x04, "2000"			},
	{0x14, 0x01, 0x0c, 0x00, "2500"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x10, "1"			},
	{0x14, 0x01, 0x30, 0x20, "2"			},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x00, "4"			},

	{0   , 0xfe, 0   ,    4, "Allow Continue"	},
	{0x14, 0x01, 0xc0, 0x00, "No"			},
	{0x14, 0x01, 0xc0, 0xc0, "Up To Level 2"	},
	{0x14, 0x01, 0xc0, 0x80, "Up To Level 3"	},
	{0x14, 0x01, 0xc0, 0x40, "Yes"			},
};

STDDIPINFO(Asuka)

static struct BurnDIPInfo MofflottDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x00, "Upright"		},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},


	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x0c, "20k And Every 50k"	},
	{0x14, 0x01, 0x0c, 0x08, "50k And Every 100k"	},
	{0x14, 0x01, 0x0c, 0x04, "100k Only"		},
	{0x14, 0x01, 0x0c, 0x00, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x00, "2"			},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x10, "4"			},
	{0x14, 0x01, 0x30, 0x20, "5"			},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x14, 0x01, 0x40, 0x40, "Off"			},
	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Number Of Keys"	},
	{0x14, 0x01, 0x80, 0x00, "B 14"			},
	{0x14, 0x01, 0x80, 0x80, "A 16"			},
};

STDDIPINFO(Mofflott)

static struct BurnDIPInfo GalmedesDIPList[]=
{
	{0x13, 0xff, 0xff, 0xf7, NULL			},
	{0x14, 0xff, 0xff, 0xfb, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x08, "Every 100k"		},
	{0x14, 0x01, 0x0c, 0x0c, "100k And Every 200k"	},
	{0x14, 0x01, 0x0c, 0x04, "150k And Every 200k"	},
	{0x14, 0x01, 0x0c, 0x00, "Every 200k"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x20, "1"			},
	{0x14, 0x01, 0x30, 0x10, "2"			},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x00, "4"			},

	{0   , 0xfe, 0   ,    2, "Coin Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Mode A (Japan)"	},
	{0x14, 0x01, 0x80, 0x00, "Mode B (World)"	},
};

STDDIPINFO(Galmedes)

static struct BurnDIPInfo EarthjkrDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xf3, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x00, "100k and 300k"	},
	{0x14, 0x01, 0x0c, 0x08, "100k only"		},
	{0x14, 0x01, 0x0c, 0x04, "200k only"		},
	{0x14, 0x01, 0x0c, 0x0c, "None"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x00, "1"			},
	{0x14, 0x01, 0x30, 0x30, "2"			},
	{0x14, 0x01, 0x30, 0x20, "3"			},
	{0x14, 0x01, 0x30, 0x10, "4"			},

	{0   , 0xfe, 0   ,    2, "Copyright"		},
	{0x14, 0x01, 0x40, 0x40, "Visco"		},
	{0x14, 0x01, 0x40, 0x00, "Visco (distributed by Romstar)"	},
};

STDDIPINFO(Earthjkr)

static struct BurnDIPInfo BonzeadvDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xbf, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x00, "Upright"		},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x08, "40k 100k"		},
	{0x14, 0x01, 0x0c, 0x0c, "50k 150k"		},
	{0x14, 0x01, 0x0c, 0x04, "60k 200k"		},
	{0x14, 0x01, 0x0c, 0x00, "80k 250k"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x20, "2"			},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x10, "4"			},
	{0x14, 0x01, 0x30, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x40, 0x40, "No"			},
	{0x14, 0x01, 0x40, 0x00, "Yes"			},
};

STDDIPINFO(Bonzeadv)

static struct BurnDIPInfo JigkmgriDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xbc, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x00, "Upright"		},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x08, "40k 100k"		},
	{0x14, 0x01, 0x0c, 0x0c, "50k 150k"		},
	{0x14, 0x01, 0x0c, 0x04, "60k 200k"		},
	{0x14, 0x01, 0x0c, 0x00, "80k 250k"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x20, "2"			},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x10, "4"			},
	{0x14, 0x01, 0x30, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x14, 0x01, 0x40, 0x40, "No"			},
	{0x14, 0x01, 0x40, 0x00, "Yes"			},
};

STDDIPINFO(Jigkmgri)

//--------------------------------------------------------------------------------------------------------------------------

static void __fastcall cadash_write_byte(UINT32 a, UINT8 d)
{
	TC0220IOCHalfWordWrite_Map(0x900000)
	TC0100SCN0ByteWrite_Map(0xc00000, 0xc0ffff)
}

static void __fastcall cadash_write_word(UINT32 a, UINT16 d)
{
	TC0220IOCHalfWordWrite_Map(0x900000)
	TC0100SCN0WordWrite_Map(0xc00000, 0xc0ffff)
	TC0100SCN0CtrlWordWrite_Map(0xc20000)

	switch (a)
	{
		case 0x080000:
		case 0x080002:
			PC090OJSpriteCtrl = (d & 0x3c) >> 2;
		return;

		case 0xa00000:
		case 0xa00002:
		case 0xa00004:
			TC0110PCRStep14rbgWordWrite(0, (a & 0x0f) / 2, d);
		return;

		case 0x0c0000:
			TC0140SYTPortWrite(d);
		return;

		case 0x0c0002:
			ZetClose();
			TC0140SYTCommWrite(d);
			ZetOpen(0);
		return;
	}
}

static UINT8 __fastcall cadash_read_byte(UINT32 a)
{
	TC0220IOCHalfWordRead_Map(0x900000)

	return 0;
}

static UINT16 __fastcall cadash_read_word(UINT32 a)
{
	TC0220IOCHalfWordRead_Map(0x900000)
	if ((a & 0xffffff0) == 0xc20000) return TC0100SCNCtrl[0][(a & 0x0f)/2];

	switch (a)
	{
		case 0xa00002:
			return TC0110PCRWordRead(0);

		case 0x0c0002:
			return TC0140SYTCommRead();
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------------------

static void __fastcall asuka_write_byte(UINT32 a, UINT8 d)
{
	TC0220IOCHalfWordWrite_Map(0x400000)
	TC0100SCN0ByteWrite_Map(0xc00000, 0xc0ffff)

	switch (a)
	{
		case 0x3a0001:
			PC090OJSpriteCtrl = ((d & 0x3c) >> 2) | ((d & 0x01) << 15);
		return;

		case 0x3e0001:
			TC0140SYTPortWrite(d);
		return;

		case 0x3e0002:
		case 0x3e0003:
			ZetClose();
			TC0140SYTCommWrite(d);
			ZetOpen(0);
		return;
	}
}

static void __fastcall asuka_write_word(UINT32 a, UINT16 d)
{
	TC0220IOCHalfWordWrite_Map(0x400000)
	TC0100SCN0WordWrite_Map(0xc00000, 0xc0ffff)
	TC0100SCN0CtrlWordWrite_Map(0xc20000)

	switch (a)
	{
		case 0x3a0000:
			PC090OJSpriteCtrl = ((d & 0x3c) >> 2) | ((d & 0x01) << 15);
		return;

		case 0x200000:
		case 0x200002:
		case 0x200004:
			TC0110PCRStep1WordWrite(0, (a & 0x0f) / 2, d);
		return;

		case 0x3e0000: // galmedes
			TC0140SYTPortWrite(d);
		return;

		case 0x3e0002:
			ZetClose();
			TC0140SYTCommWrite(d);
			ZetOpen(0);
		return;
	}
}

static UINT8 __fastcall asuka_read_byte(UINT32 a)
{
	TC0220IOCHalfWordRead_Map(0x400000)

	switch (a)
	{
		case 0x3e0002:
		case 0x3e0003:
			return TC0140SYTCommRead();
	}

	return 0;
}

static UINT16 __fastcall asuka_read_word(UINT32 a)
{
	TC0220IOCHalfWordRead_Map(0x400000)
	if ((a & 0xffffff0) == 0xc20000) return TC0100SCNCtrl[0][(a & 0x0f)/2];

	switch (a)
	{
		case 0x1076f0:
			return 0; // maze of flott

		case 0x200002:
			return TC0110PCRWordRead(0);

		case 0x3e0002:
			return TC0140SYTCommRead();
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------------------

static void __fastcall eto_write_byte(UINT32 a, UINT8 d)
{
	TC0220IOCHalfWordWrite_Map(0x300000)
	TC0220IOCHalfWordWrite_Map(0x400000)
	TC0100SCN0ByteWrite_Map(0xd00000, 0xd0ffff)
	
	if (a >= 0xc04000 && a <= 0xc0ffff) {
		INT32 Offset = (a - 0xc00000) ^ 1;
		if (TC0100SCNRam[0][Offset] != d) {
			TC0100SCNBgLayerUpdate[0] = 1;
			TC0100SCNFgLayerUpdate[0] = 1;
		}
		TC0100SCNRam[0][Offset] = d;
		return;
	}

	switch (a)
	{
		case 0x4a0000:
		case 0x4a0001:
			PC090OJSpriteCtrl = ((d & 0x3c) >> 2) | ((d & 0x01) << 15);
		return;
	}
}

static void __fastcall eto_write_word(UINT32 a, UINT16 d)
{
	TC0220IOCHalfWordWrite_Map(0x300000)
	TC0220IOCHalfWordWrite_Map(0x400000)
	TC0100SCN0WordWrite_Map(0xd00000, 0xd0ffff)
	TC0100SCN0CtrlWordWrite_Map(0xd20000)
	
	if (a >= 0xc04000 && a <= 0xc0ffff) {
		UINT16 *Ram = (UINT16*)TC0100SCNRam[0];
		INT32 Offset = (a - 0xc00000) >> 1;
		if (Ram[Offset] != d) {
			TC0100SCNBgLayerUpdate[0] = 1;
			TC0100SCNFgLayerUpdate[0] = 1;
		}
		Ram[Offset] = d;
		return;
	}

	switch (a)
	{
		case 0x100000:
		case 0x100002:
		case 0x100004:
			TC0110PCRStep1WordWrite(0, (a & 0x0f) / 2, d);
		return;

		case 0x4e0000:
			TC0140SYTPortWrite(d);
		return;

		case 0x4e0002:
			ZetClose();
			TC0140SYTCommWrite(d);
			ZetOpen(0);
		return;
	}
}

static UINT8 __fastcall eto_read_byte(UINT32 a)
{
	TC0220IOCHalfWordRead_Map(0x300000)
	TC0220IOCHalfWordRead_Map(0x400000)

	switch (a)
	{
		case 0x4e0002:
		case 0x4e0003:
			return TC0140SYTCommRead();
	}

	return 0;
}

static UINT16 __fastcall eto_read_word(UINT32 a)
{
	TC0220IOCHalfWordRead_Map(0x300000)
	TC0220IOCHalfWordRead_Map(0x400000)
	if ((a & 0xffffff0) == 0xd20000) return TC0100SCNCtrl[0][(a & 0x0f)/2];

	switch (a)
	{
		case 0x100002:
			return TC0110PCRWordRead(0);

		case 0x4e0002:
			return TC0140SYTCommRead();
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------------------

static void __fastcall bonze_write_byte(UINT32 a, UINT8 d)
{
	CCHIP_WRITE(0x800000)

	TC0100SCN0ByteWrite_Map(0xc00000, 0xc0ffff)
	
	switch (a)
	{
		case 0x3a0001:
			PC090OJSpriteCtrl = (d & 0x3c) >> 2;
		return;

		case 0x3e0001:
			TC0140SYTPortWrite(d);
		return;

		case 0x3e0003:
			ZetClose();
			TC0140SYTCommWrite(d);
			ZetOpen(0);
		return;
	}
}

static void __fastcall bonze_write_word(UINT32 a, UINT16 d)
{
	CCHIP_WRITE(0x800000)

	TC0100SCN0WordWrite_Map(0xc00000, 0xc0ffff)
	TC0100SCN0CtrlWordWrite_Map(0xc20000)

	switch (a)
	{
		case 0x200000:
		case 0x200002:
		case 0x200004:
			TC0110PCRStep1WordWrite(0, (a & 0x0f) / 2, d);
		return;

		case 0x3c0000:
			TaitoWatchdog = 0;
		return;
	}

	if ((a & 0xffffc00) == 0xc10000) return; // nop
}

static UINT8 __fastcall bonze_read_byte(UINT32 a)
{
	CCHIP_READ(0x800000)

	switch (a)
	{
		case 0x3e0003:
			return TC0140SYTCommRead();

		case 0x390001:
			return TaitoDip[0];

		case 0x3b0001:
			return TaitoDip[1];
	}

	return 0;
}

static UINT16 __fastcall bonze_read_word(UINT32 a)
{
	CCHIP_READ(0x800000)

	if ((a & 0xffffff0) == 0xc20000) return TC0100SCNCtrl[0][(a & 0x0f)/2];

	switch (a)
	{
		case 0x200002:
			return TC0110PCRWordRead(0);

		case 0x390000:
			return TaitoDip[0];

		case 0x3b0000:
			return TaitoDip[1];

		case 0x3d0000:
			return 0; // nop
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------------------

static void __fastcall cadash_sound_write(UINT16 a, UINT8 d)
{
	switch (a)
	{
		case 0x9000:
			BurnYM2151SelectRegister(d);
		return;

		case 0x9001:
			BurnYM2151WriteRegister(d);
		return;

		case 0xa000:
			TC0140SYTSlavePortWrite(d);
		return;

		case 0xa001:
			TC0140SYTSlaveCommWrite(d);
		return;

		case 0xb000:  {
			if (TaitoNumMSM5205) AsukaADPCMPos = (AsukaADPCMPos & 0x00ff) | (d << 8);		
			return;
		}
		
		case 0xc000: {
			if (TaitoNumMSM5205) MSM5205ResetWrite(0, 0);			
			return;
		}
		
		case 0xd000: {
			if (TaitoNumMSM5205) {
				MSM5205ResetWrite(0, 1);
				AsukaADPCMPos &= 0xff00;
			}			
			return;
		}
	}
}

static UINT8 __fastcall cadash_sound_read(UINT16 a)
{
	switch (a)
	{
		case 0x9000:
		case 0x9001:
			return BurnYM2151Read();

		case 0xa001:
			return TC0140SYTSlaveCommRead();
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------------------

static void DrvSoundBankSwitch(UINT32, UINT32 bank)
{
	if (ZetGetActive() == -1) return; // prevent crash from TaitoDoReset
	TaitoZ80Bank = bank & 0x03;

	ZetMapArea(0x4000, 0x7fff, 0, TaitoZ80Rom1 + TaitoZ80Bank * 0x4000);
	ZetMapArea(0x4000, 0x7fff, 2, TaitoZ80Rom1 + TaitoZ80Bank * 0x4000);
}

static void __fastcall bonze_sound_write(UINT16 a, UINT8 d)
{
	switch (a)
	{
		case 0xe000:
		case 0xe001:
		case 0xe002:
		case 0xe003:
			BurnYM2610Write(a & 3, d);
		return;

		case 0xe200:
			TC0140SYTSlavePortWrite(d);
		return;

		case 0xe201:
			TC0140SYTSlaveCommWrite(d);
		return;

		case 0xf200:
			DrvSoundBankSwitch(0, d);
		return;
	}
}

static UINT8 __fastcall bonze_sound_read(UINT16 a)
{
	switch (a)
	{
		case 0xe000:
		case 0xe001:
		case 0xe002:
		case 0xe003:
			return BurnYM2610Read(a & 3);

		case 0xe201:
			return TC0140SYTSlaveCommRead();
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------------------

static void DrvMakeInputs()
{
	memset (TC0220IOCInput, 0xff, sizeof ( TC0220IOCInput ));

	TC0220IOCInput[2] &= ~TaitoInputConfig; // asuka

	for (INT32 i = 0; i < 8; i++) {
		TC0220IOCInput[0] ^= (TC0220IOCInputPort0[i] & 1) << i;
		TC0220IOCInput[1] ^= (TC0220IOCInputPort1[i] & 1) << i;
		TC0220IOCInput[2] ^= (TC0220IOCInputPort2[i] & 1) << i;
	}
}

static void CadashYM2151IRQHandler(INT32 Irq)
{
	ZetSetIRQLine(0, (Irq) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate) // msm5205
{
	return (INT64)(double)ZetTotalCycles() * nSoundRate / 4000000;
}

static void AsukaMSM5205Vck()
{
	if (AsukaADPCMData != -1) {
		MSM5205DataWrite(0, AsukaADPCMData & 0x0f);
		AsukaADPCMData = -1;
	} else {
		AsukaADPCMData = TaitoMSM5205Rom[AsukaADPCMPos];
		AsukaADPCMPos = (AsukaADPCMPos + 1) & 0xffff;
		MSM5205DataWrite(0, AsukaADPCMData >> 4);
	}
}

static INT32 DrvDoReset()
{
	memset (TaitoRamStart, 0, TaitoRamEnd - TaitoRamStart);

	TaitoDoReset();

	ZetOpen(0);
	DrvSoundBankSwitch(0, 1);
	ZetClose();

	AsukaADPCMPos = 0;
	AsukaADPCMData = -1;

	hold_coin.reset();

	nCyclesExtra[0] = nCyclesExtra[1] = nCyclesExtra[2] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = TaitoMem;

	Taito68KRom1		= Next; Next += 0x100000;
	TaitoZ80Rom1		= Next; Next += 0x010000;

	cchip_rom           = Next; Next += TaitoCCHIPBIOSSize;
	cchip_eeprom        = Next; Next += TaitoCCHIPEEPROMSize;

	TaitoChars		    = Next; Next += TaitoCharRomSize * 2;
	TaitoSpritesA		= Next; Next += TaitoSpriteARomSize * 2;

	TaitoMSM5205Rom		= Next; Next += TaitoMSM5205RomSize;
	TaitoYM2610ARom		= Next; Next += TaitoYM2610ARomSize;

	TaitoRamStart		= Next;

	Taito68KRam1		= Next; Next += 0x008000;
	Taito68KRam2		= Next; Next += 0x001000;

	TaitoZ80Ram1		= Next; Next += 0x002000;

	TaitoRamEnd			= Next;
	TaitoMemEnd			= Next;

	return 0;
}

static void expand_graphics(UINT8 *src, INT32 len)
{
	for (INT32 i = (len * 2) - 2; i >= 0; i -= 2) {
		src[i + 0] = src[(i / 2) ^ 1] >> 4;
		src[i + 1] = src[(i / 2) ^ 1] & 0x0f;
	}
}

static void Cadash68KSetup()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Taito68KRom1,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Taito68KRam1,		0x100000, 0x107fff, MAP_RAM);
	SekMapMemory(Taito68KRam2,		0x800000, 0x800fff, MAP_RAM); // Network RAM
	SekMapMemory(PC090OJRam,		0xb00000, 0xb03fff, MAP_RAM);
	SekMapMemory(TC0100SCNRam[0],		0xc00000, 0xc0ffff, MAP_READ);
	SekSetWriteByteHandler(0,		cadash_write_byte);
	SekSetWriteWordHandler(0,		cadash_write_word);
	SekSetReadByteHandler(0,		cadash_read_byte);
	SekSetReadWordHandler(0,		cadash_read_word);
	SekClose();
}

static void Eto68KSetup()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Taito68KRom1,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Taito68KRom1 + 0x40000,	0x080000, 0x0fffff, MAP_ROM);
	SekMapMemory(Taito68KRam1,		0x200000, 0x203fff, MAP_RAM);
	SekMapMemory(PC090OJRam,		0xc00000, 0xc03fff, MAP_RAM);
	SekMapMemory(TC0100SCNRam[0] + 0x4000,	0xc04000, 0xc0ffff, MAP_READ); // mirror
	SekMapMemory(TC0100SCNRam[0],		0xd00000, 0xd0ffff, MAP_READ);
	SekSetWriteByteHandler(0,		eto_write_byte);
	SekSetWriteWordHandler(0,		eto_write_word);
	SekSetReadByteHandler(0,		eto_read_byte);
	SekSetReadWordHandler(0,		eto_read_word);
	SekClose();
}

static void Asuka68KSetup()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Taito68KRom1,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Taito68KRom1 + 0x40000,	0x080000, 0x0fffff, MAP_ROM);
	SekMapMemory(Taito68KRam1,		0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(TC0100SCNRam[0],		0xc00000, 0xc0ffff, MAP_READ);
	SekMapMemory(PC090OJRam,		0xd00000, 0xd03fff, MAP_RAM);
	SekSetWriteByteHandler(0,		asuka_write_byte);
	SekSetWriteWordHandler(0,		asuka_write_word);
	SekSetReadByteHandler(0,		asuka_read_byte);
	SekSetReadWordHandler(0,		asuka_read_word);
	SekClose();
}

static void Bonze68KSetup()
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Taito68KRom1,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Taito68KRom1 + 0x40000,	0x080000, 0x0fffff, MAP_ROM);
	SekMapMemory(Taito68KRam1,		0x10c000, 0x10ffff, MAP_RAM);
	SekMapMemory(TC0100SCNRam[0],		0xc00000, 0xc0ffff, MAP_READ);
	SekMapMemory(PC090OJRam,		0xd00000, 0xd03fff, MAP_RAM);
	SekSetWriteByteHandler(0,		bonze_write_byte);
	SekSetWriteWordHandler(0,		bonze_write_word);
	SekSetReadByteHandler(0,		bonze_read_byte);
	SekSetReadWordHandler(0,		bonze_read_word);
	SekClose();

	cchip_init();
}

static void CadashZ80Setup()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x3fff, 0, TaitoZ80Rom1);
	ZetMapArea(0x0000, 0x3fff, 2, TaitoZ80Rom1);
	ZetMapArea(0x8000, 0x8fff, 0, TaitoZ80Ram1);
	ZetMapArea(0x8000, 0x8fff, 1, TaitoZ80Ram1);
	ZetMapArea(0x8000, 0x8fff, 2, TaitoZ80Ram1);
	ZetSetWriteHandler(cadash_sound_write);
	ZetSetReadHandler(cadash_sound_read);
	ZetClose();
}

static void BonzeZ80Setup()
{
	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x3fff, 0, TaitoZ80Rom1);
	ZetMapArea(0x0000, 0x3fff, 2, TaitoZ80Rom1);
	ZetMapArea(0xc000, 0xdfff, 0, TaitoZ80Ram1);
	ZetMapArea(0xc000, 0xdfff, 1, TaitoZ80Ram1);
	ZetMapArea(0xc000, 0xdfff, 2, TaitoZ80Ram1);
	ZetSetWriteHandler(bonze_sound_write);
	ZetSetReadHandler(bonze_sound_read);
	ZetClose();
}

static void CadashSoundSetup()
{
	BurnYM2151InitBuffered(4000000, 1, NULL, 0);
	BurnYM2151SetIrqHandler(&CadashYM2151IRQHandler);
	BurnYM2151SetPortHandler(&DrvSoundBankSwitch);
	BurnYM2151SetAllRoutes(0.50, BURN_SND_ROUTE_BOTH);
	BurnTimerAttachZet(4000000);

	TaitoNumYM2151  = 1;
	TaitoNumYM2610  = 0;
	TaitoNumMSM5205 = 0;
}

static void AsukaSoundSetup()
{
	BurnYM2151InitBuffered(4000000, 1, NULL, 0);
	BurnYM2151SetIrqHandler(&CadashYM2151IRQHandler);
	BurnYM2151SetPortHandler(&DrvSoundBankSwitch);
	BurnYM2151SetAllRoutes(0.50, BURN_SND_ROUTE_BOTH);
	BurnTimerAttachZet(4000000);

	MSM5205Init(0, DrvSynchroniseStream, 384000, AsukaMSM5205Vck, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	TaitoNumYM2151  = 1;
	TaitoNumMSM5205 = 1;
	TaitoNumYM2610  = 0;
}

static void BonzeSoundSetup()
{
	INT32 DrvSndROMLen = 0x80000;
	BurnYM2610Init(8000000, TaitoYM2610ARom, &DrvSndROMLen, TaitoYM2610ARom, &DrvSndROMLen, &DrvFMIRQHandler, 0);
	BurnTimerAttachZet(4000000);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 0.25, BURN_SND_ROUTE_BOTH);

	TaitoNumYM2151  = 0;
	TaitoNumYM2610  = 1;
	TaitoNumMSM5205 = 0;
}

static INT32 CommonInit(void (*Cpu68KSetup)(), void (*CpuZ80Setup)(), void (*SoundSetup)(), INT32 buffer_sprites)
{
	TaitoNum68Ks = 1;
	TaitoNumZ80s = 1;
	TaitoInputConfig = 0;

	TaitoLoadRoms(false);

	TaitoMem = NULL;
	MemIndex();
	INT32 nLen = TaitoMemEnd - (UINT8 *)0;
	if ((TaitoMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(TaitoMem, 0, nLen);
	MemIndex();

	TaitoLoadRoms(true);

	expand_graphics(TaitoChars, TaitoCharRomSize);  // bonzeadvp2's TaitoCharRomSize is 0x060000, all others are 0x080000
	expand_graphics(TaitoSpritesA, TaitoSpriteARomSize);

	GenericTilesInit();

	PC090OJInit((TaitoSpriteARomSize * 2) / 0x100, 0, (256 - nScreenHeight) / 2, buffer_sprites);
	TC0100SCNInit(0, (0x80000 * 2) / 0x40, 0, (256 - nScreenHeight) / 2, 0, NULL);
	TC0110PCRInit(1, 0x1000);
	TC0220IOCInit();
	TaitoMakeInputsFunction = DrvMakeInputs;
	TC0140SYTInit(0);

	Cpu68KSetup();
	CpuZ80Setup();
	SoundSetup();

	DrvDoReset();

	return 0;
}

static INT32 DrvDraw()
{
	INT32 Disable = TC0100SCNCtrl[0][6] & 0xf7;

	BurnTransferClear();

	if (TC0100SCNBottomLayer(0)) {
		if (!(Disable & 0x02) && nBurnLayer & 1) TC0100SCNRenderFgLayer(0, 1, TaitoChars);
		if ((PC090OJSpriteCtrl & 0x8000) && nSpriteEnable & 1) PC090OJDrawSprites(TaitoSpritesA);
		if (!(Disable & 0x01) && nBurnLayer & 2) TC0100SCNRenderBgLayer(0, 0, TaitoChars);
	} else {
		if (!(Disable & 0x01) && nBurnLayer & 1) TC0100SCNRenderBgLayer(0, 1, TaitoChars);
		if ((PC090OJSpriteCtrl & 0x8000) && nSpriteEnable & 1) PC090OJDrawSprites(TaitoSpritesA);
		if (!(Disable & 0x02) && nBurnLayer & 2) TC0100SCNRenderFgLayer(0, 0, TaitoChars);
	}

	if (!(PC090OJSpriteCtrl & 0x8000) && nSpriteEnable & 2) PC090OJDrawSprites(TaitoSpritesA);

	if (!(Disable & 0x04) && nBurnLayer & 4) TC0100SCNRenderCharLayer(0);

	if (TC0100SCNGetFlipped(0)) BurnTransferFlip(1, 1);

	BurnTransferCopy(TC0110PCRPalette);

	return 0;
}

static INT32 CadashFrame()
{
	if (TaitoReset) {
		DrvDoReset();
	}

	TaitoMakeInputsFunction();

	SekNewFrame();
	ZetNewFrame();

	SekOpen(0);
	ZetOpen(0);

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[2] = { 16000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		CPU_RUN_TIMER(1);
		if (i == (nInterleave - 1)) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		if (i == 0) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
	}

	ZetClose();
	SekClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	PC090OJBufferSprites();

	return 0;
}

static INT32 EtoFrame() // Using for asuka too, but needs msm5205
{
	if (TaitoReset) {
		DrvDoReset();
	}

	TaitoMakeInputsFunction();

	SekNewFrame();
	ZetNewFrame();

	SekOpen(0);
	ZetOpen(0);

	INT32 nInterleave = 100;
	if (TaitoNumMSM5205) nInterleave = MSM5205CalcInterleave(0, 4000000);
	INT32 nCyclesTotal[2] = { 8000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		CPU_RUN_TIMER(1);

		if (TaitoNumMSM5205) MSM5205Update();
	}

	SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);

	ZetClose();
	SekClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		BurnYM2151Render(pBurnSoundOut, nBurnSoundLen);
		if (TaitoNumMSM5205) MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 BonzeFrame()
{
	TaitoWatchdog++;
	if (TaitoReset || TaitoWatchdog >= 180) {
		DrvDoReset();
	}

	{
		memset (TaitoInput, 0xff, 4);
		TaitoInput[0] = 0x80 + 0x20 + 0x40;
		TaitoInput[1] = 0;

		for (INT32 i = 0; i < 8; i++) {
			TaitoInput[0] ^= (TaitoInputPort0[i] & 1) << i;
			TaitoInput[1] ^= (TaitoInputPort1[i] & 1) << i;
			TaitoInput[2] ^= (TaitoInputPort2[i] & 1) << i;
			TaitoInput[3] ^= (TaitoInputPort3[i] & 1) << i;
		}

		hold_coin.check(0, TaitoInput[1], 1<<0, 2);
		hold_coin.check(1, TaitoInput[1], 1<<1, 2);

		cchip_loadports(TaitoInput[0], TaitoInput[1], TaitoInput[2], TaitoInput[3]);
	}

	SekNewFrame();
	ZetNewFrame();
	upd7810NewFrame();

	SekOpen(0);
	ZetOpen(0);

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 8000000 / 60, 4000000 / 60, 12000000 / 60 };
	INT32 nCyclesDone[3] = { nCyclesExtra[0], 0, nCyclesExtra[2] };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == 248) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		CPU_RUN_TIMER(1);

		if (cchip_active) {
			CPU_RUN(2, cchip_);
			if (i == 248) cchip_interrupt();
		}
	}

	ZetClose();
	SekClose();

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	// timer - not needed! (BurnTimer keeps track)
	nCyclesExtra[2] = nCyclesDone[2] - nCyclesTotal[2];

	if (pBurnSoundOut) {
		BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = TaitoRamStart;
		ba.nLen	  = TaitoRamEnd - TaitoRamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		ZetScan(nAction);

		TaitoICScan(nAction);

		hold_coin.scan();

		SCAN_VAR(AsukaADPCMPos);
		SCAN_VAR(AsukaADPCMData);
		SCAN_VAR(TaitoWatchdog);
		SCAN_VAR(TaitoZ80Bank);

		SCAN_VAR(nCyclesExtra);

		if (TaitoNumYM2151) BurnYM2151Scan(nAction, pnMin);
		if (TaitoNumYM2610) BurnYM2610Scan(nAction, pnMin);
		if (TaitoNumMSM5205) MSM5205Scan(nAction, pnMin);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		DrvSoundBankSwitch(0, TaitoZ80Bank);
		ZetClose();
	}

	return 0;
}

// Taito C-Chip BIOS

static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

static struct BurnRomInfo cchipRomDesc[] = {
#if !defined ROM_VERIFY
	{ "cchip_upd78c11.bin",		0x01000, 0x43021521, BRF_BIOS | TAITO_CCHIP_BIOS},
#endif
};

STD_ROM_PICK(cchip)
STD_ROM_FN(cchip)

struct BurnDriver BurnDrvCCHIP = {
	"cchip", NULL, NULL, NULL, "1989",
	"C-Chip Internal ROM\0", "Internal ROM only", "Taito Corporation Japan", "C-Chip Internal ROM",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_BOARDROM, 0, HARDWARE_TAITO_MISC, GBF_BIOS, 0,
	NULL, cchipRomInfo, cchipRomName, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, 0x1000,
	320, 240, 4, 3
};


// Cadash (World)

static struct BurnRomInfo cadashRomDesc[] = {
	{ "c21_14.ic11",			0x20000, 0x5daf13fb, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "c21_16.ic15",			0x20000, 0xcbaa2e75, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "c21_13.ic10",			0x20000, 0x6b9e0ee9, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "c21_17.ic14",			0x20000, 0xbf9a578a, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3

	{ "c21-08.38",				0x10000, 0xdca495a0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  4 Z80 Code

	{ "c21-02.9",				0x80000, 0x205883b9, BRF_GRA | TAITO_CHARS },						//  5 Characters

	{ "c21-01.1",				0x80000, 0x1ff6f39c, BRF_GRA | TAITO_SPRITESA },					//  6 Sprites

	{ "c21-07.57",				0x08000, 0xf02292bd, BRF_PRG | BRF_OPT },							//  7 HD64180RP8 code (link)

	{ "pal16l8b-c21-09.ic34",	0x00104, 0x4b296700, BRF_OPT },										//  8 plds
	{ "pal16l8b-c21-10.ic45",	0x00104, 0x35642f00, BRF_OPT },										//  9
	{ "pal16l8b-c21-11-1.ic46",	0x00104, 0xf4791e24, BRF_OPT },										// 10
	{ "pal20l8b-c21-12.ic47",	0x00144, 0xbbc2cc97, BRF_OPT },										// 11
};

STD_ROM_PICK(cadash)
STD_ROM_FN(cadash)

static INT32 CadashInit()
{
	return CommonInit(Cadash68KSetup, CadashZ80Setup, CadashSoundSetup, 1);
}

struct BurnDriver BurnDrvCadash = {
	"cadash", NULL, NULL, NULL, "1989",
	"Cadash (World)\0", NULL, "Taito Corporation Japan", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, cadashRomInfo, cadashRomName, NULL, NULL, NULL, NULL, CadashInputInfo, CadashDIPInfo,
	CadashInit, TaitoExit, CadashFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Cadash (Japan, version 2)

static struct BurnRomInfo cadashjRomDesc[] = {
	{ "c21_04-2.ic11",			0x20000, 0x7a9c1828, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "c21_06-2.ic15",			0x20000, 0xc9d6440a, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "c21_03-2.ic10",			0x20000, 0x30afc320, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "c21_05-2.ic14",			0x20000, 0x2bc93209, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3

	{ "c21-08.38",				0x10000, 0xdca495a0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  4 Z80 Code

	{ "c21-02.9",				0x80000, 0x205883b9, BRF_GRA | TAITO_CHARS },						//  5 Characters

	{ "c21-01.1",				0x80000, 0x1ff6f39c, BRF_GRA | TAITO_SPRITESA },					//  6 Sprites

	{ "c21-07.57",				0x08000, 0xf02292bd, BRF_PRG | BRF_OPT },							//  7 HD64180RP8 code (link)
	
	{ "pal16l8b-c21-09.ic34",	0x00104, 0x4b296700, BRF_OPT },										//  8 plds
	{ "pal16l8b-c21-10.ic45",	0x00104, 0x35642f00, BRF_OPT },										//  9
	{ "pal16l8b-c21-11-1.ic46",	0x00104, 0xf4791e24, BRF_OPT },										// 10
	{ "pal20l8b-c21-12.ic47",	0x00144, 0xbbc2cc97, BRF_OPT },										// 11
};

STD_ROM_PICK(cadashj)
STD_ROM_FN(cadashj)

struct BurnDriver BurnDrvCadashj = {
	"cadashj", "cadash", NULL, NULL, "1989",
	"Cadash (Japan, version 2)\0", NULL, "Taito Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, cadashjRomInfo, cadashjRomName, NULL, NULL, NULL, NULL, CadashInputInfo, CadashjDIPInfo,
	CadashInit, TaitoExit, CadashFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Cadash (Japan, version 1)

static struct BurnRomInfo cadashj1RomDesc[] = {
	{ "c21_04-1.ic11",			0x20000, 0xcc22ebe5, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "c21_06-1.ic15",			0x20000, 0x26e03304, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "c21_03-1.ic10",			0x20000, 0xc54888ed, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "c21_05-1.ic14",			0x20000, 0x834018d2, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3

	{ "c21-08.38",				0x10000, 0xdca495a0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  4 Z80 Code

	{ "c21-02.9",				0x80000, 0x205883b9, BRF_GRA | TAITO_CHARS },						//  5 Characters

	{ "c21-01.1",				0x80000, 0x1ff6f39c, BRF_GRA | TAITO_SPRITESA },					//  6 Sprites

	{ "c21-07.57",				0x08000, 0xf02292bd, BRF_PRG | BRF_OPT },							//  7 HD64180RP8 code (link)
	
	{ "pal16l8b-c21-09.ic34",	0x00104, 0x4b296700, BRF_OPT },										//  8 plds
	{ "pal16l8b-c21-10.ic45",	0x00104, 0x35642f00, BRF_OPT },										//  9
	{ "pal16l8b-c21-11-1.ic46",	0x00104, 0xf4791e24, BRF_OPT },										// 10
	{ "pal20l8b-c21-12.ic47",	0x00144, 0xbbc2cc97, BRF_OPT },										// 11
};

STD_ROM_PICK(cadashj1)
STD_ROM_FN(cadashj1)

struct BurnDriver BurnDrvCadashj1 = {
	"cadashj1", "cadash", NULL, NULL, "1989",
	"Cadash (Japan, version 1)\0", NULL, "Taito Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, cadashj1RomInfo, cadashj1RomName, NULL, NULL, NULL, NULL, CadashInputInfo, CadashjDIPInfo,
	CadashInit, TaitoExit, CadashFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Cadash (Japan, oldest version)

static struct BurnRomInfo cadashjoRomDesc[] = {
	{ "c21_04.ic11",			0x20000, 0xbe7d3f12, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "c21_06.ic15",			0x20000, 0x1db3fe02, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "c21_03.ic10",			0x20000, 0x7e31c5a3, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "c21_05.ic14",			0x20000, 0xa4f4901d, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3

	{ "c21-08.38",				0x10000, 0xdca495a0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  4 Z80 Code

	{ "c21-02.9",				0x80000, 0x205883b9, BRF_GRA | TAITO_CHARS },						//  5 Characters

	{ "c21-01.1",				0x80000, 0x1ff6f39c, BRF_GRA | TAITO_SPRITESA },					//  6 Sprites

	{ "c21-07.57",				0x08000, 0xf02292bd, BRF_PRG | BRF_OPT },							//  7 HD64180RP8 code (link)
	
	{ "pal16l8b-c21-09.ic34",	0x00104, 0x4b296700, BRF_OPT },										//  8 plds
	{ "pal16l8b-c21-10.ic45",	0x00104, 0x35642f00, BRF_OPT },										//  9
	{ "pal16l8b-c21-11-1.ic46",	0x00104, 0xf4791e24, BRF_OPT },										// 10
	{ "pal20l8b-c21-12.ic47",	0x00144, 0xbbc2cc97, BRF_OPT },										// 11
};

STD_ROM_PICK(cadashjo)
STD_ROM_FN(cadashjo)

struct BurnDriver BurnDrvCadashjo = {
	"cadashjo", "cadash", NULL, NULL, "1989",
	"Cadash (Japan, oldest version)\0", NULL, "Taito Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, cadashjoRomInfo, cadashjoRomName, NULL, NULL, NULL, NULL, CadashInputInfo, CadashjDIPInfo,
	CadashInit, TaitoExit, CadashFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Cadash (US, version 2)

static struct BurnRomInfo cadashuRomDesc[] = {
	{ "c21_14-2.ic11",			0x20000, 0xf823d418, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "c21_16-2.ic15",			0x20000, 0x90165577, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "c21_13-2.ic10",			0x20000, 0x92dcc3ae, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "c21_15-2.ic14",			0x20000, 0xf915d26a, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3

	{ "c21-08.38",				0x10000, 0xdca495a0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  4 Z80 Code

	{ "c21-02.9",				0x80000, 0x205883b9, BRF_GRA | TAITO_CHARS },						//  5 Characters

	{ "c21-01.1",				0x80000, 0x1ff6f39c, BRF_GRA | TAITO_SPRITESA },					//  6 Sprites

	{ "c21-07.57",				0x08000, 0xf02292bd, BRF_PRG | BRF_OPT },							//  7 HD64180RP8 code (link)
	
	{ "pal16l8b-c21-09.ic34",	0x00104, 0x4b296700, BRF_OPT },										//  8 plds
	{ "pal16l8b-c21-10.ic45",	0x00104, 0x35642f00, BRF_OPT },										//  9
	{ "pal16l8b-c21-11-1.ic46",	0x00104, 0xf4791e24, BRF_OPT },										// 10
	{ "pal20l8b-c21-12.ic47",	0x00144, 0xbbc2cc97, BRF_OPT },										// 11
};

STD_ROM_PICK(cadashu)
STD_ROM_FN(cadashu)

struct BurnDriver BurnDrvCadashu = {
	"cadashu", "cadash", NULL, NULL, "1989",
	"Cadash (US, version 2)\0", NULL, "Taito America Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, cadashuRomInfo, cadashuRomName, NULL, NULL, NULL, NULL, CadashInputInfo, CadashuDIPInfo,
	CadashInit, TaitoExit, CadashFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Cadash (US, version 1?)

static struct BurnRomInfo cadashu1RomDesc[] = {
	{ "c21_14-x.ic11",			0x20000, 0x64f22e5e, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "c21_16-x.ic15",			0x20000, 0x77f5d79f, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "c21_13-x.ic10",			0x20000, 0x488fd6d6, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "c21_15-x.ic14",			0x20000, 0x3a44a8b4, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3

	{ "c21-08.38",				0x10000, 0xdca495a0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  4 Z80 Code

	{ "c21-02.9",				0x80000, 0x205883b9, BRF_GRA | TAITO_CHARS },						//  5 Characters

	{ "c21-01.1",				0x80000, 0x1ff6f39c, BRF_GRA | TAITO_SPRITESA },					//  6 Sprites

	{ "c21-07.57",				0x08000, 0xf02292bd, BRF_PRG | BRF_OPT },							//  7 HD64180RP8 code (link)
	
	{ "pal16l8b-c21-09.ic34",	0x00104, 0x4b296700, BRF_OPT },										//  8 plds
	{ "pal16l8b-c21-10.ic45",	0x00104, 0x35642f00, BRF_OPT },										//  9
	{ "pal16l8b-c21-11-1.ic46",	0x00104, 0xf4791e24, BRF_OPT },										// 10
	{ "pal20l8b-c21-12.ic47",	0x00144, 0xbbc2cc97, BRF_OPT },										// 11
};

STD_ROM_PICK(cadashu1)
STD_ROM_FN(cadashu1)

struct BurnDriver BurnDrvCadashu1 = {
	"cadashu1", "cadash", NULL, NULL, "1989",
	"Cadash (US, version 1?)\0", NULL, "Taito America Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, cadashu1RomInfo, cadashu1RomName, NULL, NULL, NULL, NULL, CadashInputInfo, CadashuDIPInfo,
	CadashInit, TaitoExit, CadashFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Cadash (Italy)

static struct BurnRomInfo cadashiRomDesc[] = {
	{ "c21_27-1.ic11",			0x20000, 0xd1d9e613, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "c21_29-1.ic15",			0x20000, 0x142256ef, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "c21_26-1.ic10",			0x20000, 0xc9cf6e30, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "c21_28-1.ic14",			0x20000, 0x641fc9dd, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3

	{ "c21-08.38",				0x10000, 0xdca495a0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  4 Z80 Code

	{ "c21-02.9",				0x80000, 0x205883b9, BRF_GRA | TAITO_CHARS },						//  5 Characters

	{ "c21-01.1",				0x80000, 0x1ff6f39c, BRF_GRA | TAITO_SPRITESA },					//  6 Sprites

	{ "c21-07.57",				0x08000, 0xf02292bd, BRF_PRG | BRF_OPT },							//  7 HD64180RP8 code (link)
	
	{ "pal16l8b-c21-09.ic34",	0x00104, 0x4b296700, BRF_OPT },										//  8 plds
	{ "pal16l8b-c21-10.ic45",	0x00104, 0x35642f00, BRF_OPT },										//  9
	{ "pal16l8b-c21-11-1.ic46",	0x00104, 0xf4791e24, BRF_OPT },										// 10
	{ "pal20l8b-c21-12.ic47",	0x00144, 0xbbc2cc97, BRF_OPT },										// 11
};

STD_ROM_PICK(cadashi)
STD_ROM_FN(cadashi)

struct BurnDriver BurnDrvCadashi = {
	"cadashi", "cadash", NULL, NULL, "1989",
	"Cadash (Italy)\0", NULL, "Taito Corporation Japan", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, cadashiRomInfo, cadashiRomName, NULL, NULL, NULL, NULL, CadashInputInfo, CadashDIPInfo,
	CadashInit, TaitoExit, CadashFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Cadash (France)

static struct BurnRomInfo cadashfRomDesc[] = {
	{ "c21_19.ic11",			0x20000, 0x4d70543b, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "c21_21.ic15",			0x20000, 0x0e5b9950, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "c21_18.ic10",			0x20000, 0x8a19e59b, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "c21_20.ic14",			0x20000, 0xb96acfd9, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3

	{ "c21-08.38",				0x10000, 0xdca495a0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  4 Z80 Code

	{ "c21-02.9",				0x80000, 0x205883b9, BRF_GRA | TAITO_CHARS },						//  5 Characters

	{ "c21-01.1",				0x80000, 0x1ff6f39c, BRF_GRA | TAITO_SPRITESA },					//  6 Sprites

	{ "c21-07.57",				0x08000, 0xf02292bd, BRF_PRG | BRF_OPT },							//  7 HD64180RP8 code (link)
	
	{ "pal16l8b-c21-09.ic34",	0x00104, 0x4b296700, BRF_OPT },										//  8 plds
	{ "pal16l8b-c21-10.ic45",	0x00104, 0x35642f00, BRF_OPT },										//  9
	{ "pal16l8b-c21-11-1.ic46",	0x00104, 0xf4791e24, BRF_OPT },										// 10
	{ "pal20l8b-c21-12.ic47",	0x00144, 0xbbc2cc97, BRF_OPT },										// 11
};

STD_ROM_PICK(cadashf)
STD_ROM_FN(cadashf)

struct BurnDriver BurnDrvCadashf = {
	"cadashf", "cadash", NULL, NULL, "1989",
	"Cadash (France)\0", NULL, "Taito Corporation Japan", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, cadashfRomInfo, cadashfRomName, NULL, NULL, NULL, NULL, CadashInputInfo, CadashDIPInfo,
	CadashInit, TaitoExit, CadashFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Cadash (Germany, version 1)

static struct BurnRomInfo cadashgRomDesc[] = {
	{ "c21_23-1.ic11",			0x20000, 0x30ddbabe, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "c21_25-1.ic15",			0x20000, 0x24e10611, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "c21_22-1.ic10",			0x20000, 0xdaf58b2d, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "c21_24-1.ic14",			0x20000, 0x2359b93e, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3

	{ "c21-08.38",				0x10000, 0xdca495a0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  4 Z80 Code

	{ "c21-02.9",				0x80000, 0x205883b9, BRF_GRA | TAITO_CHARS },						//  5 Characters

	{ "c21-01.1",				0x80000, 0x1ff6f39c, BRF_GRA | TAITO_SPRITESA },					//  6 Sprites

	{ "c21-07.57",				0x08000, 0xf02292bd, BRF_PRG | BRF_OPT },							//  7 HD64180RP8 code (link)
	
	{ "pal16l8b-c21-09.ic34",	0x00104, 0x4b296700, BRF_OPT },										//  8 plds
	{ "pal16l8b-c21-10.ic45",	0x00104, 0x35642f00, BRF_OPT },										//  9
	{ "pal16l8b-c21-11-1.ic46",	0x00104, 0xf4791e24, BRF_OPT },										// 10
	{ "pal20l8b-c21-12.ic47",	0x00144, 0xbbc2cc97, BRF_OPT },										// 11
};

STD_ROM_PICK(cadashg)
STD_ROM_FN(cadashg)

struct BurnDriver BurnDrvCadashg = {
	"cadashg", "cadash", NULL, NULL, "1989",
	"Cadash (Germany, version 1)\0", NULL, "Taito Corporation Japan", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, cadashgRomInfo, cadashgRomName, NULL, NULL, NULL, NULL, CadashInputInfo, CadashDIPInfo,
	CadashInit, TaitoExit, CadashFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Cadash (Germany)

static struct BurnRomInfo cadashgoRomDesc[] = {
	{ "c21_23.ic11",			0x20000, 0xfad37785, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "c21_25.ic15",			0x20000, 0x594dda9f, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "c21_22.ic10",			0x20000, 0x7610a9b4, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "c21_24.ic14",			0x20000, 0x551d947e, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3

	{ "c21-08.38",				0x10000, 0xdca495a0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  4 Z80 Code

	{ "c21-02.9",				0x80000, 0x205883b9, BRF_GRA | TAITO_CHARS },						//  5 Characters

	{ "c21-01.1",				0x80000, 0x1ff6f39c, BRF_GRA | TAITO_SPRITESA },					//  6 Sprites

	{ "c21-07.57",				0x08000, 0xf02292bd, BRF_PRG | BRF_OPT },							//  7 HD64180RP8 code (link)
	
	{ "pal16l8b-c21-09.ic34",	0x00104, 0x4b296700, BRF_OPT },										//  8 plds
	{ "pal16l8b-c21-10.ic45",	0x00104, 0x35642f00, BRF_OPT },										//  9
	{ "pal16l8b-c21-11-1.ic46",	0x00104, 0xf4791e24, BRF_OPT },										// 10
	{ "pal20l8b-c21-12.ic47",	0x00144, 0xbbc2cc97, BRF_OPT },										// 11
};

STD_ROM_PICK(cadashgo)
STD_ROM_FN(cadashgo)

struct BurnDriver BurnDrvCadashgo = {
	"cadashgo", "cadash", NULL, NULL, "1989",
	"Cadash (Germany)\0", NULL, "Taito Corporation Japan", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, cadashgoRomInfo, cadashgoRomName, NULL, NULL, NULL, NULL, CadashInputInfo, CadashDIPInfo,
	CadashInit, TaitoExit, CadashFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Cadash (World, prototype)

static struct BurnRomInfo cadashpRomDesc[] = {
	{ "euro main h.ic11",		0x20000, 0x9dae00ca, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "euro main l.ic15",		0x20000, 0xba66b6a5, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "euro data h.bin",		0x20000, 0xbcce9d44, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "euro data l.bin",		0x20000, 0x21f5b591, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3

	{ "c21-08.38",				0x10000, 0xdca495a0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  4 Z80 Code

	{ "c21-02.9",				0x80000, 0x205883b9, BRF_GRA | TAITO_CHARS },						//  5 Characters

	{ "c21-01.1",				0x80000, 0x1ff6f39c, BRF_GRA | TAITO_SPRITESA },					//  6 Sprites

	{ "com.ic57",				0x08000, 0xbae1a92f, BRF_PRG | BRF_OPT },							//  7 HD64180RP8 code (link)
	
	{ "pal16l8b-c21-09.ic34",	0x00104, 0x4b296700, BRF_OPT },										//  8 plds
	{ "pal16l8b-c21-10.ic45",	0x00104, 0x35642f00, BRF_OPT },										//  9
	{ "pal16l8b-c21-11-1.ic46",	0x00104, 0xf4791e24, BRF_OPT },										// 10
	{ "pal20l8b-c21-12.ic47",	0x00144, 0xbbc2cc97, BRF_OPT },										// 11
};

STD_ROM_PICK(cadashp)
STD_ROM_FN(cadashp)

struct BurnDriver BurnDrvCadashp = {
	"cadashp", "cadash", NULL, NULL, "1989",
	"Cadash (World, prototype)\0", NULL, "Taito Corporation Japan", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, cadashpRomInfo, cadashpRomName, NULL, NULL, NULL, NULL, CadashInputInfo, CadashjDIPInfo,
	CadashInit, TaitoExit, CadashFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Cadash (Spain, version 1)
// no labels on the program ROMs

static struct BurnRomInfo cadashsRomDesc[] = {
	{ "ic11",				0x20000, 0x6c11743e, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "ic15",				0x20000, 0x73224356, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "ic10",				0x20000, 0x57d659d9, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "ic14",				0x20000, 0x53c1b195, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3

	{ "c21-08.38",			0x10000, 0xdca495a0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  4 Z80 Code

	{ "c21-02.9",			0x80000, 0x205883b9, BRF_GRA | TAITO_CHARS },						//  5 Characters

	{ "c21-01.1",			0x80000, 0x1ff6f39c, BRF_GRA | TAITO_SPRITESA },					//  6 Sprites

	{ "c21-07.57",			0x08000, 0xf02292bd, BRF_PRG | BRF_OPT },							//  7 HD64180RP8 code (link)
	
	{ "pal16l8b-c21-09.ic34",	0x00104, 0x4b296700, BRF_OPT },									//  8 plds
	{ "pal16l8b-c21-10.ic45",	0x00104, 0x35642f00, BRF_OPT },									//  9
	{ "pal16l8b-c21-11-1.ic46",	0x00104, 0xf4791e24, BRF_OPT },									// 10
	{ "pal20l8b-c21-12.ic47",	0x00144, 0xbbc2cc97, BRF_OPT },									// 11
};

STD_ROM_PICK(cadashs)
STD_ROM_FN(cadashs)

struct BurnDriver BurnDrvCadashs = {
	"cadashs", "cadash", NULL, NULL, "1989",
	"Cadash (Spain, version 1)\0", NULL, "Taito Corporation Japan", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, cadashsRomInfo, cadashsRomName, NULL, NULL, NULL, NULL, CadashInputInfo, CadashDIPInfo,
	CadashInit, TaitoExit, CadashFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Kokontouzai Eto Monogatari (Japan)

static struct BurnRomInfo etoRomDesc[] = {
	{ "eto-1.ic23",			0x20000, 0x44286597, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "eto-0.ic8",			0x20000, 0x57b79370, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "eto-2.ic30",			0x80000, 0x12f46fb5, BRF_PRG | BRF_ESS | TAITO_68KROM1 },			//  2

	{ "eto-5.ic27",			0x10000, 0xb3689da0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  3 Z80 Code

	{ "eto-4.ic3",			0x80000, 0xa8768939, BRF_GRA | TAITO_CHARS },						//  4 Characters

	{ "eto-3.ic6",			0x80000, 0xdd247397, BRF_GRA | TAITO_SPRITESA },					//  5 Sprites
};

STD_ROM_PICK(eto)
STD_ROM_FN(eto)

static INT32 EtoInit()
{
	INT32 nRet = CommonInit(Eto68KSetup, CadashZ80Setup, CadashSoundSetup, 0);

	if (nRet == 0) {
		BurnByteswap(Taito68KRom1 + 0x40000, 0x80000);
	}

	return nRet;
}

struct BurnDriver BurnDrvEto = {
	"eto", NULL, NULL, NULL, "1994",
	"Kokontouzai Eto Monogatari (Japan)\0", NULL, "Visco", "Taito Misc",
	L"\u53E4\u4ECA\u6771\u897F\u5E72\u652F\u7269\u8A9E\0Kokontouzai Eto Monogatari (Japan)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, etoRomInfo, etoRomName, NULL, NULL, NULL, NULL, AsukaInputInfo, EtoDIPInfo,
	EtoInit, TaitoExit, EtoFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Kokontouzai Eto Monogatari (Japan, prototype?)

static struct BurnRomInfo etoaRomDesc[] = {
	{ "pe.ic23",			0x20000, 0x36a6a742, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "po.ic8",				0x20000, 0xbc86f328, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "pd.ic30",			0x80000, 0x39e6a0f3, BRF_PRG | BRF_ESS | TAITO_68KROM1 },			//  2

	{ "sd.ic27",			0x10000, 0xb3689da0, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  3 Z80 Code

	{ "sc.ic3",				0x80000, 0xa8768939, BRF_GRA | TAITO_CHARS },						//  4 Characters

	{ "ob.ic6",				0x80000, 0xdd247397, BRF_GRA | TAITO_SPRITESA },					//  5 Sprites
};

STD_ROM_PICK(etoa)
STD_ROM_FN(etoa)

struct BurnDriver BurnDrvEtoa = {
	"etoa", "eto", NULL, NULL, "1994",
	"Kokontouzai Eto Monogatari (Japan, prototype?)\0", NULL, "Visco", "Taito Misc",
	L"\u53E4\u4ECA\u6771\u897F\u5E72\u652F\u7269\u8A9E\0Kokontouzai Eto Monogatari (Japan, prototype?)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, etoaRomInfo, etoaRomName, NULL, NULL, NULL, NULL, AsukaInputInfo, EtoDIPInfo,
	EtoInit, TaitoExit, EtoFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 240, 4, 3
};


// Asuka & Asuka (World)

static struct BurnRomInfo asukaRomDesc[] = {
	{ "b68-13.ic23",		0x20000, 0x855efb3e, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "b68-12.ic8",			0x20000, 0x271eeee9, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "b68-03.ic30",		0x80000, 0xd3a59b10, BRF_PRG | BRF_ESS | TAITO_68KROM1 },			//  2

	{ "b68-11.ic27",		0x10000, 0xc378b508, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  3 Z80 Code

	{ "b68-01.ic3",			0x80000, 0x89f32c94, BRF_GRA | TAITO_CHARS },						//  4 Characters

	{ "b68-02.ic6",			0x80000, 0xf5018cd3, BRF_GRA | TAITO_SPRITESA },					//  5 Sprites
	{ "b68-07.ic5",			0x10000, 0xc113acc8, BRF_GRA | TAITO_SPRITESA_BYTESWAP },			//  6
	{ "b68-06.ic4",			0x10000, 0xf517e64d, BRF_GRA | TAITO_SPRITESA_BYTESWAP },			//  7

	{ "b68-10.ic24",		0x10000, 0x387aaf40, BRF_SND | TAITO_MSM5205 },						//  8 MSM5205 Samples
	
	{ "b68-04.ic32",		0x00144, 0x9be618d1, BRF_OPT },										//  9 plds
	{ "b68-05.ic43",		0x00104, 0xd6524ccc, BRF_OPT },										// 10
};

STD_ROM_PICK(asuka)
STD_ROM_FN(asuka)

static INT32 AsukaInit()
{
	INT32 nRet = CommonInit(Asuka68KSetup, CadashZ80Setup, AsukaSoundSetup, 0);

	if (nRet == 0) {
		BurnByteswap(Taito68KRom1 + 0x40000, 0x80000);
	}

	TaitoInputConfig = 0x30;

	return nRet;
}

struct BurnDriver BurnDrvAsuka = {
	"asuka", NULL, NULL, NULL, "1988",
	"Asuka & Asuka (World)\0", NULL, "Taito Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, asukaRomInfo, asukaRomName, NULL, NULL, NULL, NULL, AsukaInputInfo, AsukaDIPInfo,
	AsukaInit, TaitoExit, EtoFrame, DrvDraw, DrvScan, NULL, 0x1000,
	240, 320, 3, 4
};


// Asuka & Asuka (Japan, version 1)

static struct BurnRomInfo asukajRomDesc[] = {
	{ "b68-09-1.ic23",		0x20000, 0xd61be555, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "b68-08-1.ic8",		0x20000, 0xe916f17b, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "b68-03.ic30",		0x80000, 0xd3a59b10, BRF_PRG | BRF_ESS | TAITO_68KROM1 },			//  2

	{ "b68-11.ic27",		0x10000, 0xc378b508, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  3 Z80 Code

	{ "b68-01.ic3",			0x80000, 0x89f32c94, BRF_GRA | TAITO_CHARS },					//  4 Characters

	{ "b68-02.ic6",			0x80000, 0xf5018cd3, BRF_GRA | TAITO_SPRITESA },				//  5 Sprites
	{ "b68-07.ic5",			0x10000, 0xc113acc8, BRF_GRA | TAITO_SPRITESA_BYTESWAP },		//  6
	{ "b68-06.ic4",			0x10000, 0xf517e64d, BRF_GRA | TAITO_SPRITESA_BYTESWAP },		//  7

	{ "b68-10.ic24",		0x10000, 0x387aaf40, BRF_SND | TAITO_MSM5205 },					//  8 MSM5205 Samples
	
	{ "b68-04.ic32",		0x00144, 0x9be618d1, BRF_OPT },									//  9 plds
	{ "b68-05.ic43",		0x00104, 0xd6524ccc, BRF_OPT },									// 10
};

STD_ROM_PICK(asukaj)
STD_ROM_FN(asukaj)

struct BurnDriver BurnDrvAsukaj = {
	"asukaj", "asuka", NULL, NULL, "1988",
	"Asuka & Asuka (Japan, version 1)\0", NULL, "Taito Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, asukajRomInfo, asukajRomName, NULL, NULL, NULL, NULL, AsukaInputInfo, AsukaDIPInfo,
	AsukaInit, TaitoExit, EtoFrame, DrvDraw, DrvScan, NULL, 0x1000,
	240, 320, 3, 4
};


// Asuka & Asuka (Japan)

static struct BurnRomInfo asukajaRomDesc[] = {
	{ "b68-09.ic23",		0x20000, 0x1eaa1bbb, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "b68-08.ic8",			0x20000, 0x8cc96e60, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "b68-03.ic30",		0x80000, 0xd3a59b10, BRF_PRG | BRF_ESS | TAITO_68KROM1 },			//  2

	{ "b68-11.ic27",		0x10000, 0xc378b508, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },			//  3 Z80 Code

	{ "b68-01.ic3",			0x80000, 0x89f32c94, BRF_GRA | TAITO_CHARS },						//  4 Characters

	{ "b68-02.ic6",			0x80000, 0xf5018cd3, BRF_GRA | TAITO_SPRITESA },					//  5 Sprites
	{ "b68-07.ic5",			0x10000, 0xc113acc8, BRF_GRA | TAITO_SPRITESA_BYTESWAP },			//  6
	{ "b68-06.ic4",			0x10000, 0xf517e64d, BRF_GRA | TAITO_SPRITESA_BYTESWAP },			//  7

	{ "b68-10.ic24",		0x10000, 0x387aaf40, BRF_SND | TAITO_MSM5205 },						//  8 MSM5205 Samples
	
	{ "b68-04.ic32",		0x00144, 0x9be618d1, BRF_OPT },										//  9 plds
	{ "b68-05.ic43",		0x00104, 0xd6524ccc, BRF_OPT },										// 10
};

STD_ROM_PICK(asukaja)
STD_ROM_FN(asukaja)

struct BurnDriver BurnDrvAsukaja = {
	"asukaja", "asuka", NULL, NULL, "1988",
	"Asuka & Asuka (Japan)\0", NULL, "Taito Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, asukajaRomInfo, asukajaRomName, NULL, NULL, NULL, NULL, AsukaInputInfo, AsukaDIPInfo,
	AsukaInit, TaitoExit, EtoFrame, DrvDraw, DrvScan, NULL, 0x1000,
	240, 320, 3, 4
};


// Maze of Flott (Japan)

static struct BurnRomInfo mofflottRomDesc[] = {
	{ "c17-09.bin",			0x20000, 0x05ee110f, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "c17-08.bin",			0x20000, 0xd0aacffd, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "c17-03.bin",			0x80000, 0x27047fc3, BRF_PRG | BRF_ESS | TAITO_68KROM1 },		//  2

	{ "c17-07.bin",			0x10000, 0xcdb7bc2c, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  3 Z80 Code

	{ "c17-01.bin",			0x80000, 0xe9466d42, BRF_GRA | TAITO_CHARS },				//  4 Characters

	{ "c17-02.bin",			0x80000, 0x8860a8db, BRF_GRA | TAITO_SPRITESA },			//  5 Sprites
	{ "c17-05.bin",			0x10000, 0x57ac4741, BRF_GRA | TAITO_SPRITESA_BYTESWAP },		//  6
	{ "c17-04.bin",			0x10000, 0xf4250410, BRF_GRA | TAITO_SPRITESA_BYTESWAP },		//  7

	{ "c17-06.bin",			0x10000, 0x5c332125, BRF_SND | TAITO_MSM5205 },				//  8 MSM5205 Samples
};

STD_ROM_PICK(mofflott)
STD_ROM_FN(mofflott)

struct BurnDriver BurnDrvMofflott = {
	"mofflott", NULL, NULL, NULL, "1989",
	"Maze of Flott (Japan)\0", NULL, "Taito Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_MAZE, 0,
	NULL, mofflottRomInfo, mofflottRomName, NULL, NULL, NULL, NULL, AsukaInputInfo, MofflottDIPInfo,
	AsukaInit, TaitoExit, EtoFrame, DrvDraw, DrvScan, NULL, 0x1000,
	240, 320, 3, 4
};


// Galmedes (Japan)

static struct BurnRomInfo galmedesRomDesc[] = {
	{ "gm-prg1.ic23",		0x20000, 0x32a70753, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "gm-prg0.ic8",		0x20000, 0xfae546a4, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "gm-30.ic30",			0x80000, 0x4da2a407, BRF_PRG | BRF_ESS | TAITO_68KROM1 },		//  2

	{ "gm-snd.ic27",		0x10000, 0xd6f56c21, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  3 Z80 Code

	{ "gm-scn.ic3",			0x80000, 0x3bab0581, BRF_GRA | TAITO_CHARS },				//  4 Characters

	{ "gm-obj.ic6",			0x80000, 0x7a4a1315, BRF_GRA | TAITO_SPRITESA },			//  5 Sprites
	
	{ "b68-04.ic32",		0x00144, 0x9be618d1, BRF_OPT },						//  6 plds
	{ "b68-05.ic43",		0x00104, 0xd6524ccc, BRF_OPT },						//  7
};

STD_ROM_PICK(galmedes)
STD_ROM_FN(galmedes)

static INT32 GalmedesInit()
{
	INT32 nRet = CommonInit(Asuka68KSetup, CadashZ80Setup, CadashSoundSetup, 0);

	if (nRet == 0) {
		BurnByteswap(Taito68KRom1 + 0x40000, 0x80000);
	}

	return nRet;
}

struct BurnDriver BurnDrvGalmedes = {
	"galmedes", NULL, NULL, NULL, "1992",
	"Galmedes (Japan)\0", NULL, "Visco", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, galmedesRomInfo, galmedesRomName, NULL, NULL, NULL, NULL, AsukaInputInfo, GalmedesDIPInfo,
	GalmedesInit, TaitoExit, EtoFrame, DrvDraw, DrvScan, NULL, 0x1000,
	240, 320, 3, 4
};


// U.N. Defense Force: Earth Joker (US / Japan, set 1)

static struct BurnRomInfo earthjkrRomDesc[] = {
	{ "ej_3b.ic23",			0x20000, 0xbdd86fc2, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "ej_3a.ic8",			0x20000, 0x9c8050c6, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "ej_30e.ic30",		0x80000, 0x49d1f77f, BRF_PRG | BRF_ESS | TAITO_68KROM1 },		//  2

	{ "ej_2.ic27",			0x10000, 0x42ba2566, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  3 Z80 Code

	{ "ej_chr-0.ic3",		0x80000, 0xac675297, BRF_GRA | TAITO_CHARS },				//  4 Characters

	{ "ej_obj-0.ic6",		0x80000, 0x5f21ac47, BRF_GRA | TAITO_SPRITESA },			//  5 Sprites
	{ "ej_1.ic5",			0x10000, 0xcb4891db, BRF_GRA | TAITO_SPRITESA_BYTESWAP },		//  6
	{ "ej_0.ic4",			0x10000, 0xb612086f, BRF_GRA | TAITO_SPRITESA_BYTESWAP },		//  7
	
	{ "b68-04.ic32",		0x00144, 0x9be618d1, BRF_OPT },						//  8 plds
	{ "b68-05.ic43",		0x00104, 0xd6524ccc, BRF_OPT },						//  9
};

STD_ROM_PICK(earthjkr)
STD_ROM_FN(earthjkr)

static INT32 EarthjkrInit()
{
	INT32 nRet = CommonInit(Asuka68KSetup, CadashZ80Setup, CadashSoundSetup, 0);

	if (nRet == 0) {
		BurnByteswap(Taito68KRom1 + 0x40000, 0x80000);

		{ // remove when earthjkr redumped
			// bitrot patch from Haze, fixes scrolling in middle-part of last level
			UINT16 *rom = (UINT16*)Taito68KRom1;
			rom[0x7aaa/2] = BURN_ENDIAN_SWAP_INT16(0x317c);
		}
	}

	return nRet;
}

struct BurnDriver BurnDrvEarthjkr = {
	"earthjkr", NULL, NULL, NULL, "1993",
	"U.N. Defense Force: Earth Joker (US / Japan, set 1)\0", NULL, "Visco", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, earthjkrRomInfo, earthjkrRomName, NULL, NULL, NULL, NULL, AsukaInputInfo, EarthjkrDIPInfo,
	EarthjkrInit, TaitoExit, EtoFrame, DrvDraw, DrvScan, NULL, 0x1000,
	240, 320, 3, 4
};


// U.N. Defense Force: Earth Joker (US / Japan, set 2)

static struct BurnRomInfo earthjkraRomDesc[] = {
	{ "ejok_ic23",			0x20000, 0xcbd29731, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "ejok_ic8",			0x20000, 0xcfd4953c, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "ejok_ic30",		    0x80000, 0x49d1f77f, BRF_PRG | BRF_ESS | TAITO_68KROM1 },		//  2

	{ "ejok_ic28",			0x10000, 0x42ba2566, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  3 Z80 Code

	{ "ej_chr-0.ic3",		0x80000, 0xac675297, BRF_GRA | TAITO_CHARS },				//  4 Characters

	{ "ej_obj-0.ic6",		0x80000, 0x5f21ac47, BRF_GRA | TAITO_SPRITESA },			//  5 Sprites
	{ "ejok_ic5",			0x10000, 0xcb4891db, BRF_GRA | TAITO_SPRITESA_BYTESWAP },		//  6
	{ "ejok_ic4",			0x10000, 0xb612086f, BRF_GRA | TAITO_SPRITESA_BYTESWAP },		//  7
	
	{ "b68-04.ic32",		0x00144, 0x9be618d1, BRF_OPT },						//  8 plds
	{ "b68-05.ic43",		0x00104, 0xd6524ccc, BRF_OPT },						//  9
};

STD_ROM_PICK(earthjkra)
STD_ROM_FN(earthjkra)

struct BurnDriver BurnDrvEarthjkra = {
	"earthjkra", "earthjkr", NULL, NULL, "1993",
	"U.N. Defense Force: Earth Joker (US / Japan, set 2)\0", NULL, "Visco", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, earthjkraRomInfo, earthjkraRomName, NULL, NULL, NULL, NULL, AsukaInputInfo, EarthjkrDIPInfo,
	EarthjkrInit, TaitoExit, EtoFrame, DrvDraw, DrvScan, NULL, 0x1000,
	240, 320, 3, 4
};


// U.N. Defense Force: Earth Joker (US / Japan, set 3)
/* Taito PCB: K1100726A / J1100169B */

static struct BurnRomInfo earthjkrbRomDesc[] = {
	{ "4.ic23",				0x20000, 0x250f09f8, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "3.ic8",				0x20000, 0x88fc1c5d, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "ic30e.ic30",		    0x80000, 0x49d1f77f, BRF_PRG | BRF_ESS | TAITO_68KROM1 },		//  2

	{ "2.ic27",				0x10000, 0x42ba2566, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  3 Z80 Code

	{ "ej_chr-0.ic3",		0x80000, 0xac675297, BRF_GRA | TAITO_CHARS },				//  4 Characters

	{ "ej_obj-0.ic6",		0x80000, 0x5f21ac47, BRF_GRA | TAITO_SPRITESA },			//  5 Sprites
	{ "1.ic5",				0x10000, 0xcb4891db, BRF_GRA | TAITO_SPRITESA_BYTESWAP },		//  6
	{ "0.ic4",				0x10000, 0xb612086f, BRF_GRA | TAITO_SPRITESA_BYTESWAP },		//  7
	
	{ "b68-04.ic32",		0x00144, 0x9be618d1, BRF_OPT },						//  8 plds
	{ "b68-05.ic43",		0x00104, 0xd6524ccc, BRF_OPT },						//  9
};

STD_ROM_PICK(earthjkrb)
STD_ROM_FN(earthjkrb)

struct BurnDriver BurnDrvEarthjkrb = {
	"earthjkrb", "earthjkr", NULL, NULL, "1993",
	"U.N. Defense Force: Earth Joker (US / Japan, set 3)\0", NULL, "Visco", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, earthjkrbRomInfo, earthjkrbRomName, NULL, NULL, NULL, NULL, AsukaInputInfo, EarthjkrDIPInfo,
	EarthjkrInit, TaitoExit, EtoFrame, DrvDraw, DrvScan, NULL, 0x1000,
	240, 320, 3, 4
};


// Known to exist (not dumped) a Japanese version with ROMs 3 & 4 also stamped "A" same as above or different version??
// Also known to exist (not dumped) a US version of Earth Joker, title screen shows "DISTRIBUTED BY ROMSTAR, INC."  ROMs were numbered
// from 0 through 4 and the fix ROM at IC30 is labeled 1 even though IC5 is also labled as 1 similar to the below set:
// (ROMSTAR license is set by a dipswitch, is set mentioned above really undumped?)

// U.N. Defense Force: Earth Joker (Japan, prototype?)

static struct BurnRomInfo earthjkrpRomDesc[] = {
	{ "4.ic8",				0x20000, 0xe9b1ef0c, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "3.ic23",				0x20000, 0x26c33225, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "5.ic30",				0x80000, 0xbf760b2d, BRF_PRG | BRF_ESS | TAITO_68KROM1 },		//  2

	{ "2.ic27",				0x10000, 0x42ba2566, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  3 Z80 Code

	{ "ej_chr-0.ic3",		0x80000, 0xac675297, BRF_GRA | TAITO_CHARS },				//  4 Characters

	{ "ej_obj-0.ic6",		0x80000, 0x5f21ac47, BRF_GRA | TAITO_SPRITESA },			//  5 Sprites
	{ "1.ic5",				0x10000, 0xcb4891db, BRF_GRA | TAITO_SPRITESA_BYTESWAP },		//  6
	{ "0.ic4",				0x10000, 0xb612086f, BRF_GRA | TAITO_SPRITESA_BYTESWAP },		//  7
	
	{ "b68-04.ic32",		0x00144, 0x9be618d1, BRF_OPT },						//  8 plds
	{ "b68-05.ic43",		0x00104, 0xd6524ccc, BRF_OPT },						//  9
};

STD_ROM_PICK(earthjkrp)
STD_ROM_FN(earthjkrp)

struct BurnDriver BurnDrvEarthjkrp = {
	"earthjkrp", "earthjkr", NULL, NULL, "1993",
	"U.N. Defense Force: Earth Joker (Japan, prototype?)\0", NULL, "Visco", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, earthjkrpRomInfo, earthjkrpRomName, NULL, NULL, NULL, NULL, AsukaInputInfo, EarthjkrDIPInfo,
	GalmedesInit, TaitoExit, EtoFrame, DrvDraw, DrvScan, NULL, 0x1000,
	240, 320, 3, 4
};


// Bonze Adventure (World, newer)

static struct BurnRomInfo bonzeadvRomDesc[] = {
	{ "b41-09-1.17",		0x10000, 0xaf821fbc, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "b41-11-1.26",		0x10000, 0x823fff00, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "b41-10.16",			0x10000, 0x4ca94d77, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "b41-15.25",			0x10000, 0xaed7a0d0, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3
	{ "b41-01.15",			0x80000, 0x5d072fa4, BRF_PRG | BRF_ESS | TAITO_68KROM1 },		//  4

	{ "b41-13.20",			0x10000, 0x9e464254, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  5 Z80 Code

	{ "b41-03.1",			0x80000, 0x736d35d0, BRF_GRA | TAITO_CHARS },				//  6 Characters

	{ "b41-02.7",			0x80000, 0x29f205d9, BRF_GRA | TAITO_SPRITESA },			//  7 Sprites

	{ "b41-04.48",			0x80000, 0xc668638f, BRF_SND | TAITO_YM2610A },				//  8 YM2610 Samples
	
	{ "cchip_b41-05.43",	0x02000, 0x75c52553, BRF_ESS | BRF_PRG | TAITO_CCHIP_EEPROM },
};

STDROMPICKEXT(bonzeadv, bonzeadv, cchip)
STD_ROM_FN(bonzeadv)

static INT32 BonzeInit()
{
	INT32 rc = CommonInit(Bonze68KSetup, BonzeZ80Setup, BonzeSoundSetup, 0);

	if (!rc) {
		TC0100SCNSetFlippedOffsets(6, -16);
	}

	return rc;
}

struct BurnDriver BurnDrvBonzeadv = {
	"bonzeadv", NULL, "cchip", NULL, "1988",
	"Bonze Adventure (World, newer)\0", NULL, "Taito Corporation Japan", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, bonzeadvRomInfo, bonzeadvRomName, NULL, NULL, NULL, NULL, BonzeadvInputInfo, BonzeadvDIPInfo,
	BonzeInit, TaitoExit, BonzeFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 224, 4, 3
};


// Bonze Adventure (World, older)

static struct BurnRomInfo bonzeadvoRomDesc[] = {
	{ "b41-09.17",			0x10000, 0x06818710, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "b41-11.26",			0x10000, 0x33c4c2f4, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "b41-10.16",			0x10000, 0x4ca94d77, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "b41-15.25",			0x10000, 0xaed7a0d0, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3
	{ "b41-01.15",			0x80000, 0x5d072fa4, BRF_PRG | BRF_ESS | TAITO_68KROM1 },		//  4

	{ "b41-13.20",			0x10000, 0x9e464254, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  5 Z80 Code

	{ "b41-03.1",			0x80000, 0x736d35d0, BRF_GRA | TAITO_CHARS },				//  6 Characters

	{ "b41-02.7",			0x80000, 0x29f205d9, BRF_GRA | TAITO_SPRITESA },			//  7 Sprites

	{ "b41-04.48",			0x80000, 0xc668638f, BRF_SND | TAITO_YM2610A },				//  8 YM2610 Samples
	
	{ "cchip_b41-05.43",	0x02000, 0x75c52553, BRF_ESS | BRF_PRG | TAITO_CCHIP_EEPROM },
};

STDROMPICKEXT(bonzeadvo, bonzeadvo, cchip)
STD_ROM_FN(bonzeadvo)

struct BurnDriver BurnDrvBonzeadvo = {
	"bonzeadvo", "bonzeadv", "cchip", NULL, "1988",
	"Bonze Adventure (World, older)\0", NULL, "Taito Corporation Japan", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, bonzeadvoRomInfo, bonzeadvoRomName, NULL, NULL, NULL, NULL, BonzeadvInputInfo, BonzeadvDIPInfo,
	BonzeInit, TaitoExit, BonzeFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 224, 4, 3
};


// Bonze Adventure (US)

static struct BurnRomInfo bonzeadvuRomDesc[] = {
	{ "b41-09-1.17",		0x10000, 0xaf821fbc, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "b41-11-1.26",		0x10000, 0x823fff00, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "b41-10.16",			0x10000, 0x4ca94d77, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "b41-14.25",			0x10000, 0x37def16a, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3
	{ "b41-01.15",			0x80000, 0x5d072fa4, BRF_PRG | BRF_ESS | TAITO_68KROM1 },		//  4

	{ "b41-13.20",			0x10000, 0x9e464254, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  5 Z80 Code

	{ "b41-03.1",			0x80000, 0x736d35d0, BRF_GRA | TAITO_CHARS },				//  6 Characters

	{ "b41-02.7",			0x80000, 0x29f205d9, BRF_GRA | TAITO_SPRITESA },			//  7 Sprites

	{ "b41-04.48",			0x80000, 0xc668638f, BRF_SND | TAITO_YM2610A },				//  8 YM2610 Samples
	
	{ "cchip_b41-05.43",	0x02000, 0x75c52553, BRF_ESS | BRF_PRG | TAITO_CCHIP_EEPROM },
};

STDROMPICKEXT(bonzeadvu, bonzeadvu, cchip)
STD_ROM_FN(bonzeadvu)

struct BurnDriver BurnDrvBonzeadvu = {
	"bonzeadvu", "bonzeadv", "cchip", NULL, "1988",
	"Bonze Adventure (US)\0", NULL, "Taito America Corporation", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, bonzeadvuRomInfo, bonzeadvuRomName, NULL, NULL, NULL, NULL, BonzeadvInputInfo, JigkmgriDIPInfo,
	BonzeInit, TaitoExit, BonzeFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 224, 4, 3
};


// Bonze Adventure (World, prototype, newer)

static struct BurnRomInfo bonzeadvpRomDesc[] = {
	{ "0l.ic17",			0x10000, 0x9e046e6f, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP }, 	//  0 68K Code
	{ "0h.ic26",			0x10000, 0x3e2b2628, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP }, 	//  1
	{ "1h.ic16",			0x10000, 0x52f31b98, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP }, 	//  2
	{ "1l.ic25",			0x10000, 0xc7e79b98, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP }, 	//  3
	{ "49eb.ic26",			0x20000, 0xc747650b, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP }, 	//  4
	{ "fd65.ic20",			0x20000, 0xc32f3bd5, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP }, 	//  5
	{ "0e7e.ic28",			0x20000, 0xdc1f9fd0, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP }, 	//  6
	{ "a418.ic23",			0x20000, 0x51b02be6, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP }, 	//  7

	{ "b41-13.20",			0x10000, 0x9e464254, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  8 Z80 Code

	{ "abbe.ic9",			0x20000, 0x50e6581c, BRF_GRA | TAITO_CHARS_BYTESWAP }, 			//  9 Characters
	{ "0ac8.ic15",			0x20000, 0x29002fc4, BRF_GRA | TAITO_CHARS_BYTESWAP }, 			// 10
	{ "5ebf.ic5",			0x20000, 0xdac6f11f, BRF_GRA | TAITO_CHARS_BYTESWAP }, 			// 11
	{ "77c8.ic12",			0x20000, 0xd8aaae12, BRF_GRA | TAITO_CHARS_BYTESWAP }, 			// 12

	{ "9369.ic19",			0x20000, 0xa9dd7f90, BRF_GRA | TAITO_SPRITESA_BYTESWAP },               // 13 Sprites
	{ "e3ed.ic25",			0x20000, 0x7cc66ee2, BRF_GRA | TAITO_SPRITESA_BYTESWAP },               // 14
	{ "03eb.ic16",			0x20000, 0x39f32715, BRF_GRA | TAITO_SPRITESA_BYTESWAP },               // 15
	{ "b8e1.ic22",			0x20000, 0x15b836cf, BRF_GRA | TAITO_SPRITESA_BYTESWAP },               // 16

	{ "6089.ic17",			0x20000, 0xb092783c, BRF_SND | TAITO_YM2610A },				// 17 YM2610 Samples
	{ "2e1f.ic14",			0x20000, 0xdf1f87c0, BRF_SND | TAITO_YM2610A },				// 18
	{ "f66e.ic11",			0x20000, 0xc6df1b3e, BRF_SND | TAITO_YM2610A },				// 19
	{ "49d7.ic7",			0x20000, 0x5584c02c, BRF_SND | TAITO_YM2610A },				// 20
	
	{ "cchip_b41-05.43",	0x02000, 0x75c52553, BRF_ESS | BRF_PRG | TAITO_CCHIP_EEPROM },
};

STDROMPICKEXT(bonzeadvp, bonzeadvp, cchip)
STD_ROM_FN(bonzeadvp)

struct BurnDriver BurnDrvBonzeadvp = {
	"bonzeadvp", "bonzeadv", "cchip", NULL, "1988",
	"Bonze Adventure (World, prototype, newer)\0", NULL, "Taito Corporation Japan", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, bonzeadvpRomInfo, bonzeadvpRomName, NULL, NULL, NULL, NULL, BonzeadvInputInfo, JigkmgriDIPInfo,
	BonzeInit, TaitoExit, BonzeFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 224, 4, 3
};


// Bonze Adventure (World, prototype, older)

static struct BurnRomInfo bonzeadvp2RomDesc[] = {
	{ "prg 0h.ic17",			0x10000, 0xce530615, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },  //  0 68K Code
	{ "prg 0l.ic26",			0x10000, 0x048a0dcb, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },  //  1
	{ "prg 1h.ic16",			0x10000, 0xe5d63e9b, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },  //  2
	{ "prg 1l europe.ic25",		0x10000, 0xd04b8e2b, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },  //  3
	{ "map 0h ad2e.ic7",		0x10000, 0xca894028, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },  //  4
	{ "map 0l 676f.ic2",		0x10000, 0x956bc558, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },  //  5
	{ "map 1h 9cbd.ic8",		0x10000, 0x08a5320f, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },  //  6
	{ "map 1l 95f6.ic3",		0x10000, 0xf65988c0, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },  //  7
	{ "map 2h 0e7e.ic9",		0x10000, 0x4513dcf7, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },  //  8
	{ "map 2l a418.ic4",		0x10000, 0x106475e3, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },  //  9

	{ "sound main 3-9.ic20",	0x10000, 0x2b4fc69a, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },           // 10 Z80 Code

	// The TaitoCharRomSize of this set is only 0x060000, must be ...
	{ "scn 0l 7fe0.ic2",		0x10000, 0xc6413f92, BRF_GRA | TAITO_CHARS_BYTESWAP },              // 11 Characters
	{ "scn 0h 8711.ic7",		0x10000, 0x17466da0, BRF_GRA | TAITO_CHARS_BYTESWAP },              // 12
	{ "scn 1l 2bde.ic3",		0x10000, 0xca459623, BRF_GRA | TAITO_CHARS_BYTESWAP },              // 13
	{ "scn 1h 83b7.ic8",		0x10000, 0x35fde3c7, BRF_GRA | TAITO_CHARS_BYTESWAP },              // 14
	{ "scn 2l db08.ic4",		0x10000, 0x2431e8db, BRF_GRA | TAITO_CHARS_BYTESWAP },              // 15
	{ "scn 2h f411.ic9",		0x10000, 0x229debcf, BRF_GRA | TAITO_CHARS_BYTESWAP },              // 16

	{ "obj 0l c67e.ic2",		0x10000, 0x2f9615a8, BRF_GRA | TAITO_SPRITESA_BYTESWAP },           // 17 Sprites
	{ "obj 0h 6ce2.ic7",		0x10000, 0xd5bff0fd, BRF_GRA | TAITO_SPRITESA_BYTESWAP },           // 18
	{ "obj 1l ccf7.ic3",		0x10000, 0xc7a654d9, BRF_GRA | TAITO_SPRITESA_BYTESWAP },           // 19
	{ "obj 1h 7708.ic8",		0x10000, 0x81357279, BRF_GRA | TAITO_SPRITESA_BYTESWAP },           // 20
	{ "obj 2l 6096.ic4",		0x10000, 0x6dd67af8, BRF_GRA | TAITO_SPRITESA_BYTESWAP },           // 21
	{ "obj 2h 818b.ic9",		0x10000, 0xd614732b, BRF_GRA | TAITO_SPRITESA_BYTESWAP },           // 22
	{ "obj 3l a355.ic5",		0x10000, 0x173ddd11, BRF_GRA | TAITO_SPRITESA_BYTESWAP },           // 23
	{ "obj 3h 3756.ic10",		0x10000, 0x981e66a9, BRF_GRA | TAITO_SPRITESA_BYTESWAP },           // 24

	{ "sound 0h 3-8.ic2",		0x10000, 0x2996e756, BRF_SND | TAITO_YM2610A },                     // 25 YM2610 Samples
	{ "sound 1h 3-8.ic3",		0x10000, 0x780368ac, BRF_SND | TAITO_YM2610A },                     // 26
	{ "sound 2h 3-8.ic4",		0x10000, 0x8f3b9fa5, BRF_SND | TAITO_YM2610A },                     // 27
	{ "sound 3h 3-8.ic5",		0x10000, 0x1a8be621, BRF_SND | TAITO_YM2610A },                     // 28
	{ "sound 0l 3-8.ic7",		0x10000, 0x3711abfa, BRF_SND | TAITO_YM2610A },                     // 29
	{ "sound 1l 3-8.ic8",		0x10000, 0xf24a3d1a, BRF_SND | TAITO_YM2610A },                     // 30
	{ "sound 2l 3-8.ic9",		0x10000, 0x5987900c, BRF_SND | TAITO_YM2610A },                     // 31
	{ "sound 3l 3-8.ic10",		0x10000, 0xe8a6a9e6, BRF_SND | TAITO_YM2610A },                     // 32

	{ "generic 10-9 f3eb.ic43",			0x02000, 0x75c52553, BRF_ESS | BRF_PRG | TAITO_CCHIP_EEPROM },      // 33 C-Chip (BAD DUMP)
};

STDROMPICKEXT(bonzeadvp2, bonzeadvp2, cchip)
STD_ROM_FN(bonzeadvp2)

struct BurnDriver BurnDrvBonzeadvp2 = {
	"bonzeadvp2", "bonzeadv", "cchip", NULL, "1988",
	"Bonze Adventure (World, prototype, older)\0", NULL, "Taito Corporation Japan", "Taito Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, bonzeadvp2RomInfo, bonzeadvp2RomName, NULL, NULL, NULL, NULL, BonzeadvInputInfo, JigkmgriDIPInfo,
	BonzeInit, TaitoExit, BonzeFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 224, 4, 3
};


// Jigoku Meguri (Japan)

static struct BurnRomInfo jigkmgriRomDesc[] = {
	{ "b41-09-1.17",		0x10000, 0xaf821fbc, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "b41-11-1.26",		0x10000, 0x823fff00, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "b41-10.16",			0x10000, 0x4ca94d77, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "b41-12.25",			0x10000, 0x40d9c1fc, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3
	{ "b41-01.15",			0x80000, 0x5d072fa4, BRF_PRG | BRF_ESS | TAITO_68KROM1 },		//  4

	{ "b41-13.20",			0x10000, 0x9e464254, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  5 Z80 Code

	{ "b41-03.1",			0x80000, 0x736d35d0, BRF_GRA | TAITO_CHARS },				//  6 Characters

	{ "b41-02.7",			0x80000, 0x29f205d9, BRF_GRA | TAITO_SPRITESA },			//  7 Sprites

	{ "b41-04.48",			0x80000, 0xc668638f, BRF_SND | TAITO_YM2610A },				//  8 YM2610 Samples
	
	{ "cchip_b41-05.43",	0x02000, 0x75c52553, BRF_ESS | BRF_PRG | TAITO_CCHIP_EEPROM },
};

STDROMPICKEXT(jigkmgri, jigkmgri, cchip)
STD_ROM_FN(jigkmgri)

struct BurnDriver BurnDrvJigkmgri = {
	"jigkmgri", "bonzeadv", "cchip", NULL, "1988",
	"Jigoku Meguri (Japan)\0", NULL, "Taito Corporation", "Taito Misc",
	L"\u5730\u7344\u3081\u3050\u308A\0Jigoku Meguri (Japan)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, jigkmgriRomInfo, jigkmgriRomName, NULL, NULL, NULL, NULL, BonzeadvInputInfo, JigkmgriDIPInfo,
	BonzeInit, TaitoExit, BonzeFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 224, 4, 3
};


// Jigoku Meguri (Japan, hack?)
/* Copyright year has been removed */

static struct BurnRomInfo jigkmgriaRomDesc[] = {
	{ "blank_label.ic17",			0x10000, 0x5d3a5283, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  0 68K Code
	{ "bonzi__(ic26)__11a.ic26",	0x10000, 0xe1f2f205, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  1
	{ "b41__10.ic16",				0x10000, 0x4ca94d77, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  2
	{ "b41__12.ic25",				0x10000, 0x40d9c1fc, BRF_PRG | BRF_ESS | TAITO_68KROM1_BYTESWAP },	//  3
	{ "b41-01.15",					0x80000, 0x5d072fa4, BRF_PRG | BRF_ESS | TAITO_68KROM1 },		//  4

	{ "b41__13.ic20",		0x10000, 0x9e464254, BRF_PRG | BRF_ESS | TAITO_Z80ROM1 },		//  5 Z80 Code

	{ "b41-03.1",			0x80000, 0x736d35d0, BRF_GRA | TAITO_CHARS },				//  6 Characters

	{ "b41-02.7",			0x80000, 0x29f205d9, BRF_GRA | TAITO_SPRITESA },			//  7 Sprites

	{ "b41-04.48",			0x80000, 0xc668638f, BRF_SND | TAITO_YM2610A },				//  8 YM2610 Samples

	{ "cchip_b41-05.43",	0x02000, 0x75c52553, BRF_ESS | BRF_PRG | TAITO_CCHIP_EEPROM },
};

STDROMPICKEXT(jigkmgria, jigkmgria, cchip)
STD_ROM_FN(jigkmgria)

struct BurnDriver BurnDrvJigkmgria = {
	"jigkmgria", "bonzeadv", "cchip", NULL, "19??",
	"Jigoku Meguri (Japan, hack?)\0", NULL, "Taito Corporation", "Taito Misc",
	L"\u5730\u7344\u3081\u3050\u308A\0Jigoku Meguri (Japan, hack?)\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_RUNGUN, 0,
	NULL, jigkmgriaRomInfo, jigkmgriaRomName, NULL, NULL, NULL, NULL, BonzeadvInputInfo, JigkmgriDIPInfo,
	BonzeInit, TaitoExit, BonzeFrame, DrvDraw, DrvScan, NULL, 0x1000,
	320, 224, 4, 3
};
