// Based on MAME driver by Alex Pasadyn, Zsolt Vasvari, Ernesto Corvi, Aaron Giles

#include "driver.h"
#include "burnint.h"
#include "midwunit.h"

static UINT8 DrvReset = 0;  // needs hooked-up :)

static struct BurnInputInfo Mk3InputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 2,	"p1 start"},
	{"P1 Up",		    BIT_DIGITAL,	nWolfUnitJoy1 + 0,	"p1 up"},
	{"P1 Down",		    BIT_DIGITAL,	nWolfUnitJoy1 + 1,	"p1 down"},
	{"P1 Left",		    BIT_DIGITAL,	nWolfUnitJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	nWolfUnitJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	nWolfUnitJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	nWolfUnitJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	nWolfUnitJoy1 + 6,	"p1 fire 3"},
	{"P1 Button 4",		BIT_DIGITAL,	nWolfUnitJoy2 + 0,	"p1 fire 4"},
	{"P1 Button 5",		BIT_DIGITAL,	nWolfUnitJoy2 + 1,	"p1 fire 5"},
	{"P1 Button 6",		BIT_DIGITAL,	nWolfUnitJoy2 + 2,	"p1 fire 6"},

	{"P2 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 5,	"p2 start"},
	{"P2 Up",		    BIT_DIGITAL,	nWolfUnitJoy1 + 8,	"p2 up"},
	{"P2 Down",		    BIT_DIGITAL,	nWolfUnitJoy1 + 9,	"p2 down"},
	{"P2 Left",		    BIT_DIGITAL,	nWolfUnitJoy1 + 10,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	nWolfUnitJoy1 + 11,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	nWolfUnitJoy1 + 12,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	nWolfUnitJoy1 + 13,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	nWolfUnitJoy1 + 14,	"p2 fire 3"},
	{"P2 Button 4",		BIT_DIGITAL,	nWolfUnitJoy2 + 4,	"p2 fire 4"},
	{"P2 Button 5",		BIT_DIGITAL,	nWolfUnitJoy2 + 5,	"p2 fire 5"},
	{"P2 Button 6",		BIT_DIGITAL,	nWolfUnitJoy2 + 6,	"p2 fire 6"},

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	        "reset"},
	{"Service",		    BIT_DIGITAL,	nWolfUnitJoy3 + 6,	"service"},
	{"Tilt",		    BIT_DIGITAL,	nWolfUnitJoy3 + 3,	"tilt"},
	{"Dip A",		    BIT_DIPSWITCH,	nWolfUnitDSW + 0,	"dip"},
	{"Dip B",		    BIT_DIPSWITCH,	nWolfUnitDSW + 1,	"dip"},
	{"Dip C",		    BIT_DIPSWITCH,	nWolfUnitDSW + 2,	"dip"},
	{"Dip D",		    BIT_DIPSWITCH,	nWolfUnitDSW + 3,	"dip"},
	{"Dip E",		    BIT_DIPSWITCH,	nWolfUnitDSW + 4,	"dip"},
};

STDINPUTINFO(Mk3)


static struct BurnDIPInfo Mk3DIPList[]=
{
	{0x1b, 0xff, 0xff, 0x7d, NULL		},
	{0x1c, 0xff, 0xff, 0x04, NULL		},
	{0x1d, 0xff, 0xff, 0x10, NULL		},
	{0x1e, 0xff, 0xff, 0xc0, NULL		},
	{0x1f, 0xff, 0xff, 0x10, NULL		},

	{0   , 0xfe, 0   ,    2, "Test Switch"		},
	{0x1b, 0x01, 0x01, 0x01, "Off"		},
	{0x1b, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Counters"		},
	{0x1b, 0x01, 0x02, 0x02, "One"		},
	{0x1b, 0x01, 0x02, 0x00, "Two"		},

	{0   , 0xfe, 0   ,    19, "Coinage"		},
	{0x1b, 0x01, 0x7c, 0x7c, "USA-1"		},
	{0x1b, 0x01, 0x7c, 0x3c, "USA-2"		},
	{0x1b, 0x01, 0x7c, 0x5c, "USA-3"		},
	{0x1b, 0x01, 0x7c, 0x1c, "USA-4"		},
	{0x1b, 0x01, 0x7c, 0x6c, "USA-ECA"		},
	{0x1b, 0x01, 0x7c, 0x0c, "USA-Free Play"		},
	{0x1b, 0x01, 0x7c, 0x74, "German-1"		},
	{0x1b, 0x01, 0x7c, 0x34, "German-2"		},
	{0x1b, 0x01, 0x7c, 0x54, "German-3"		},
	{0x1b, 0x01, 0x7c, 0x14, "German-4"		},
	{0x1b, 0x01, 0x7c, 0x64, "German-5"		},
	{0x1b, 0x01, 0x7c, 0x24, "German-ECA"		},
	{0x1b, 0x01, 0x7c, 0x04, "German-Free Play"		},
	{0x1b, 0x01, 0x7c, 0x78, "French-1"		},
	{0x1b, 0x01, 0x7c, 0x38, "French-2"		},
	{0x1b, 0x01, 0x7c, 0x58, "French-3"		},
	{0x1b, 0x01, 0x7c, 0x18, "French-4"		},
	{0x1b, 0x01, 0x7c, 0x68, "French-ECA"		},
	{0x1b, 0x01, 0x7c, 0x08, "French-Free Play"		},

	{0   , 0xfe, 0   ,    2, "Coinage Source"		},
	{0x1b, 0x01, 0x80, 0x80, "Dipswitch"		},
	{0x1b, 0x01, 0x80, 0x00, "CMOS"		},

	{0   , 0xfe, 0   ,    2, "Powerup Test"		},
	{0x1c, 0x01, 0x02, 0x00, "Off"		},
	{0x1c, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Bill Validator"		},
	{0x1c, 0x01, 0x04, 0x04, "Off"		},
	{0x1c, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Attract Sound"		},
	{0x1c, 0x01, 0x10, 0x00, "Off"		},
	{0x1c, 0x01, 0x10, 0x10, "On"		},

	{0   , 0xfe, 0   ,    2, "Blood"		},
	{0x1c, 0x01, 0x40, 0x00, "Off"		},
	{0x1c, 0x01, 0x40, 0x40, "On"		},

	{0   , 0xfe, 0   ,    2, "Violence"		},
	{0x1c, 0x01, 0x80, 0x00, "Off"		},
	{0x1c, 0x01, 0x80, 0x80, "On"		},

	{0   , 0xfe, 0   ,    1, "Service Mode (No Toggle)"		},
	{0x1d, 0x01, 0x10, 0x10, "Off"		},
};

STDDIPINFO(Mk3)

static struct BurnInputInfo OpeniceInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 2,	"p1 start"},
	{"P1 Up",		    BIT_DIGITAL,	nWolfUnitJoy1 + 0,	"p1 up"},
	{"P1 Down",		    BIT_DIGITAL,	nWolfUnitJoy1 + 1,	"p1 down"},
	{"P1 Left",		    BIT_DIGITAL,	nWolfUnitJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	nWolfUnitJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	nWolfUnitJoy1 + 6,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	nWolfUnitJoy1 + 4,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	nWolfUnitJoy1 + 5,	"p1 fire 3"},

	{"P2 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 5,	"p2 start"},
	{"P2 Up",		    BIT_DIGITAL,	nWolfUnitJoy1 + 8,	"p2 up"},
	{"P2 Down",		    BIT_DIGITAL,	nWolfUnitJoy1 + 9,	"p2 down"},
	{"P2 Left",		    BIT_DIGITAL,	nWolfUnitJoy1 + 10,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	nWolfUnitJoy1 + 11,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	nWolfUnitJoy1 + 14,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	nWolfUnitJoy1 + 12,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	nWolfUnitJoy1 + 13,	"p2 fire 3"},

	{"P3 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 7,	"p3 coin"},
	{"P3 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 9,	"p3 start"},
	{"P3 Up",		    BIT_DIGITAL,	nWolfUnitJoy2 + 0,	"p3 up"},
	{"P3 Down",		    BIT_DIGITAL,	nWolfUnitJoy2 + 1,	"p3 down"},
	{"P3 Left",		    BIT_DIGITAL,	nWolfUnitJoy2 + 2,	"p3 left"},
	{"P3 Right",		BIT_DIGITAL,	nWolfUnitJoy2 + 3,	"p3 right"},
	{"P3 Button 1",		BIT_DIGITAL,	nWolfUnitJoy2 + 6,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	nWolfUnitJoy2 + 4,	"p3 fire 2"},
	{"P3 Button 3",		BIT_DIGITAL,	nWolfUnitJoy2 + 5,	"p3 fire 3"},

	{"P4 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 8,	"p4 coin"},
	{"P4 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 10,	"p4 start"},
	{"P4 Up",		    BIT_DIGITAL,	nWolfUnitJoy2 + 8,	"p4 up"},
	{"P4 Down",		    BIT_DIGITAL,	nWolfUnitJoy2 + 9,	"p4 down"},
	{"P4 Left",		    BIT_DIGITAL,	nWolfUnitJoy2 + 10,	"p4 left"},
	{"P4 Right",		BIT_DIGITAL,	nWolfUnitJoy2 + 11,	"p4 right"},
	{"P4 Button 1",		BIT_DIGITAL,	nWolfUnitJoy2 + 14,	"p4 fire 1"},
	{"P4 Button 2",		BIT_DIGITAL,	nWolfUnitJoy2 + 12,	"p4 fire 2"},
	{"P4 Button 3",		BIT_DIGITAL,	nWolfUnitJoy2 + 13,	"p4 fire 3"},

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	        "reset"},
	{"Service",		    BIT_DIGITAL,	nWolfUnitJoy3 + 6,	"service"},
	{"Tilt",		    BIT_DIGITAL,	nWolfUnitJoy3 + 3,	"tilt"},
	{"Dip A",		    BIT_DIPSWITCH,	nWolfUnitDSW + 0,	"dip"},
	{"Dip B",		    BIT_DIPSWITCH,	nWolfUnitDSW + 1,	"dip"},
	{"Dip C",		    BIT_DIPSWITCH,	nWolfUnitDSW + 2,	"dip"},
	{"Dip D",		    BIT_DIPSWITCH,	nWolfUnitDSW + 3,	"dip"},
};

STDINPUTINFO(Openice)


static struct BurnDIPInfo OpeniceDIPList[]=
{
	{0x27, 0xff, 0xff, 0xbe, NULL		},
	{0x28, 0xff, 0xff, 0x0b, NULL		},
	{0x29, 0xff, 0xff, 0x80, NULL		},
	{0x2a, 0xff, 0xff, 0x10, NULL		},

	{0   , 0xfe, 0   ,    2, "Coinage Source"		},
	{0x27, 0x01, 0x01, 0x01, "Dipswitch"		},
	{0x27, 0x01, 0x01, 0x00, "CMOS"		},

	{0   , 0xfe, 0   ,    23, "Coinage"		},
	{0x27, 0x01, 0x3e, 0x3e, "USA-1"		},
	{0x27, 0x01, 0x3e, 0x3c, "USA-2"		},
	{0x27, 0x01, 0x3e, 0x3a, "USA-3"		},
	{0x27, 0x01, 0x3e, 0x38, "USA-4"		},
	{0x27, 0x01, 0x3e, 0x34, "USA-9"		},
	{0x27, 0x01, 0x3e, 0x32, "USA-10"		},
	{0x27, 0x01, 0x3e, 0x36, "USA-ECA"		},
	{0x27, 0x01, 0x3e, 0x30, "USA-Free Play"		},
	{0x27, 0x01, 0x3e, 0x2e, "German-1"		},
	{0x27, 0x01, 0x3e, 0x2c, "German-2"		},
	{0x27, 0x01, 0x3e, 0x2a, "German-3"		},
	{0x27, 0x01, 0x3e, 0x28, "German-4"		},
	{0x27, 0x01, 0x3e, 0x24, "German-5"		},
	{0x27, 0x01, 0x3e, 0x26, "German-ECA"		},
	{0x27, 0x01, 0x3e, 0x20, "German-Free Play"		},
	{0x27, 0x01, 0x3e, 0x1e, "French-1"		},
	{0x27, 0x01, 0x3e, 0x1c, "French-2"		},
	{0x27, 0x01, 0x3e, 0x1a, "French-3"		},
	{0x27, 0x01, 0x3e, 0x18, "French-4"		},
	{0x27, 0x01, 0x3e, 0x14, "French-11"		},
	{0x27, 0x01, 0x3e, 0x12, "French-12"		},
	{0x27, 0x01, 0x3e, 0x16, "French-ECA"		},
	{0x27, 0x01, 0x3e, 0x10, "French-Free Play"		},

	{0   , 0xfe, 0   ,    2, "Counters"		},
	{0x27, 0x01, 0x40, 0x40, "One"		},
	{0x27, 0x01, 0x40, 0x00, "Two"		},

	{0   , 0xfe, 0   ,    2, "Bill Validator"		},
	{0x28, 0x01, 0x01, 0x01, "Off"		},
	{0x28, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Attract Sound"		},
	{0x28, 0x01, 0x02, 0x00, "Off"		},
	{0x28, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Powerup Test"		},
	{0x28, 0x01, 0x04, 0x00, "Off"		},
	{0x28, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Head Size"		},
	{0x28, 0x01, 0x08, 0x08, "Normal"		},
	{0x28, 0x01, 0x08, 0x00, "Large"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x28, 0x01, 0x10, 0x00, "2-player"		},
	{0x28, 0x01, 0x10, 0x10, "4-player"		},

	{0   , 0xfe, 0   ,    2, "Test Switch"		},
	{0x28, 0x01, 0x80, 0x80, "Off"		},
	{0x28, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    1, "Service Mode (No Toggle)"		},
	{0x29, 0x01, 0x10, 0x10, "Off"		},
};

STDDIPINFO(Openice)

static struct BurnInputInfo NbahangtInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 2,	"p1 start"},
	{"P1 Up",		    BIT_DIGITAL,	nWolfUnitJoy1 + 0,	"p1 up"},
	{"P1 Down",		    BIT_DIGITAL,	nWolfUnitJoy1 + 1,	"p1 down"},
	{"P1 Left",		    BIT_DIGITAL,	nWolfUnitJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	nWolfUnitJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	nWolfUnitJoy1 + 6,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	nWolfUnitJoy1 + 4,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	nWolfUnitJoy1 + 5,	"p1 fire 3"},

	{"P2 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 5,	"p2 start"},
	{"P2 Up",		    BIT_DIGITAL,	nWolfUnitJoy1 + 8,	"p2 up"},
	{"P2 Down",		    BIT_DIGITAL,	nWolfUnitJoy1 + 9,	"p2 down"},
	{"P2 Left",		    BIT_DIGITAL,	nWolfUnitJoy1 + 10,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	nWolfUnitJoy1 + 11,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	nWolfUnitJoy1 + 14,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	nWolfUnitJoy1 + 12,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	nWolfUnitJoy1 + 13,	"p2 fire 3"},

	{"P3 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 7,	"p3 coin"},
	{"P3 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 9,	"p3 start"},
	{"P3 Up",		    BIT_DIGITAL,	nWolfUnitJoy2 + 0,	"p3 up"},
	{"P3 Down",		    BIT_DIGITAL,	nWolfUnitJoy2 + 1,	"p3 down"},
	{"P3 Left",		    BIT_DIGITAL,	nWolfUnitJoy2 + 2,	"p3 left"},
	{"P3 Right",		BIT_DIGITAL,	nWolfUnitJoy2 + 3,	"p3 right"},
	{"P3 Button 1",		BIT_DIGITAL,	nWolfUnitJoy2 + 6,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	nWolfUnitJoy2 + 4,	"p3 fire 2"},
	{"P3 Button 3",		BIT_DIGITAL,	nWolfUnitJoy2 + 5,	"p3 fire 3"},

	{"P4 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 8,	"p4 coin"},
	{"P4 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 10,	"p4 start"},
	{"P4 Up",		    BIT_DIGITAL,	nWolfUnitJoy2 + 8,	"p4 up"},
	{"P4 Down",		    BIT_DIGITAL,	nWolfUnitJoy2 + 9,	"p4 down"},
	{"P4 Left",		    BIT_DIGITAL,	nWolfUnitJoy2 + 10,	"p4 left"},
	{"P4 Right",		BIT_DIGITAL,	nWolfUnitJoy2 + 11,	"p4 right"},
	{"P4 Button 1",		BIT_DIGITAL,	nWolfUnitJoy2 + 14,	"p4 fire 1"},
	{"P4 Button 2",		BIT_DIGITAL,	nWolfUnitJoy2 + 12,	"p4 fire 2"},
	{"P4 Button 3",		BIT_DIGITAL,	nWolfUnitJoy2 + 13,	"p4 fire 3"},

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	        "reset"},
	{"Service",		    BIT_DIGITAL,	nWolfUnitJoy3 + 6,	"service"},
	{"Tilt",		    BIT_DIGITAL,	nWolfUnitJoy3 + 3,	"tilt"},
	{"Dip A",		    BIT_DIPSWITCH,	nWolfUnitDSW + 0,	"dip"},
	{"Dip B",		    BIT_DIPSWITCH,	nWolfUnitDSW + 1,	"dip"},
	{"Dip C",		    BIT_DIPSWITCH,	nWolfUnitDSW + 2,	"dip"},
};

STDINPUTINFO(Nbahangt)


static struct BurnDIPInfo NbahangtDIPList[]=
{
	{0x27, 0xff, 0xff, 0x7d, NULL		},
	{0x28, 0xff, 0xff, 0x7f, NULL		},
	{0x29, 0xff, 0xff, 0x10, NULL		},

	{0   , 0xfe, 0   ,    2, "Test Switch"		},
	{0x27, 0x01, 0x01, 0x01, "Off"		},
	{0x27, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Powerup Test"		},
	{0x27, 0x01, 0x02, 0x00, "Off"		},
	{0x27, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Bill Validator"		},
	{0x27, 0x01, 0x40, 0x40, "Off"		},
	{0x27, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x27, 0x01, 0x80, 0x00, "2-player"		},
	{0x27, 0x01, 0x80, 0x80, "4-player"		},

	{0   , 0xfe, 0   ,    3, "Counters"		},
	{0x28, 0x01, 0x03, 0x03, "One, 1/1"		},
	{0x28, 0x01, 0x03, 0x02, "One, Totalizing"		},
	{0x28, 0x01, 0x03, 0x01, "Two, 1/1"		},

	{0   , 0xfe, 0   ,    3, "Country"		},
	{0x28, 0x01, 0x0c, 0x0c, "USA"		},
	{0x28, 0x01, 0x0c, 0x08, "French"		},
	{0x28, 0x01, 0x0c, 0x04, "German"		},

	{0   , 0xfe, 0   ,    6, "Coinage"		},
	{0x28, 0x01, 0x70, 0x70, "1"		},
	{0x28, 0x01, 0x70, 0x30, "2"		},
	{0x28, 0x01, 0x70, 0x50, "3"		},
	{0x28, 0x01, 0x70, 0x10, "4"		},
	{0x28, 0x01, 0x70, 0x60, "ECA"		},
	{0x28, 0x01, 0x70, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Coinage Source"		},
	{0x28, 0x01, 0x80, 0x80, "Dipswitch"		},
	{0x28, 0x01, 0x80, 0x00, "CMOS"		},

	{0   , 0xfe, 0   ,    1, "Service Mode (No Toggle)"		},
	{0x29, 0x01, 0x10, 0x10, "Off"		},
};

STDDIPINFO(Nbahangt)

static struct BurnInputInfo RmpgwtInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 2,	"p1 start"},
	{"P1 Up",		    BIT_DIGITAL,	nWolfUnitJoy1 + 0,	"p1 up"},
	{"P1 Down",		    BIT_DIGITAL,	nWolfUnitJoy1 + 1,	"p1 down"},
	{"P1 Left",		    BIT_DIGITAL,	nWolfUnitJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	nWolfUnitJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	nWolfUnitJoy1 + 6,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	nWolfUnitJoy1 + 4,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	nWolfUnitJoy1 + 5,	"p1 fire 3"},

	{"P2 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 5,	"p2 start"},
	{"P2 Up",		    BIT_DIGITAL,	nWolfUnitJoy1 + 8,	"p2 up"},
	{"P2 Down",		    BIT_DIGITAL,	nWolfUnitJoy1 + 9,	"p2 down"},
	{"P2 Left",		    BIT_DIGITAL,	nWolfUnitJoy1 + 10,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	nWolfUnitJoy1 + 11,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	nWolfUnitJoy1 + 14,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	nWolfUnitJoy1 + 12,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	nWolfUnitJoy1 + 13,	"p2 fire 3"},

	{"P3 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 7,	"p3 coin"},
	{"P3 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 9,	"p3 start"},
	{"P3 Up",		    BIT_DIGITAL,	nWolfUnitJoy2 + 0,	"p3 up"},
	{"P3 Down",		    BIT_DIGITAL,	nWolfUnitJoy2 + 1,	"p3 down"},
	{"P3 Left",		    BIT_DIGITAL,	nWolfUnitJoy2 + 2,	"p3 left"},
	{"P3 Right",		BIT_DIGITAL,	nWolfUnitJoy2 + 3,	"p3 right"},
	{"P3 Button 1",		BIT_DIGITAL,	nWolfUnitJoy2 + 6,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	nWolfUnitJoy2 + 4,	"p3 fire 2"},
	{"P3 Button 3",		BIT_DIGITAL,	nWolfUnitJoy2 + 5,	"p3 fire 3"},

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	        "reset"},
	{"Service",		    BIT_DIGITAL,	nWolfUnitJoy3 + 6,	"service"},
	{"Tilt",		    BIT_DIGITAL,	nWolfUnitJoy3 + 3,	"tilt"},
	{"Dip A",		    BIT_DIPSWITCH,	nWolfUnitDSW + 0,	"dip"},
	{"Dip B",		    BIT_DIPSWITCH,	nWolfUnitDSW + 1,	"dip"},
	{"Dip C",		    BIT_DIPSWITCH,	nWolfUnitDSW + 2,	"dip"},
	{"Dip D",		    BIT_DIPSWITCH,	nWolfUnitDSW + 3,	"dip"},
	{"Dip E",		    BIT_DIPSWITCH,	nWolfUnitDSW + 4,	"dip"},
};

STDINPUTINFO(Rmpgwt)


static struct BurnDIPInfo RmpgwtDIPList[]=
{
	{0x1e, 0xff, 0xff, 0xbe, NULL		},
	{0x1f, 0xff, 0xff, 0x01, NULL		},
	{0x20, 0xff, 0xff, 0x00, NULL		},
	{0x21, 0xff, 0xff, 0x80, NULL		},
	{0x22, 0xff, 0xff, 0x10, NULL		},

	{0   , 0xfe, 0   ,    2, "Coinage Source"		},
	{0x1e, 0x01, 0x01, 0x01, "Dipswitch"		},
	{0x1e, 0x01, 0x01, 0x00, "CMOS"		},

	{0   , 0xfe, 0   ,    23, "Coinage"		},
	{0x1e, 0x01, 0x3e, 0x3e, "USA-1"		},
	{0x1e, 0x01, 0x3e, 0x3c, "USA-2"		},
	{0x1e, 0x01, 0x3e, 0x3a, "USA-3"		},
	{0x1e, 0x01, 0x3e, 0x38, "USA-4"		},
	{0x1e, 0x01, 0x3e, 0x34, "USA-9"		},
	{0x1e, 0x01, 0x3e, 0x32, "USA-10"		},
	{0x1e, 0x01, 0x3e, 0x36, "USA-ECA"		},
	{0x1e, 0x01, 0x3e, 0x30, "USA-Free Play"		},
	{0x1e, 0x01, 0x3e, 0x2e, "German-1"		},
	{0x1e, 0x01, 0x3e, 0x2c, "German-2"		},
	{0x1e, 0x01, 0x3e, 0x2a, "German-3"		},
	{0x1e, 0x01, 0x3e, 0x28, "German-4"		},
	{0x1e, 0x01, 0x3e, 0x24, "German-5"		},
	{0x1e, 0x01, 0x3e, 0x26, "German-ECA"		},
	{0x1e, 0x01, 0x3e, 0x20, "German-Free Play"		},
	{0x1e, 0x01, 0x3e, 0x1e, "French-1"		},
	{0x1e, 0x01, 0x3e, 0x1c, "French-2"		},
	{0x1e, 0x01, 0x3e, 0x1a, "French-3"		},
	{0x1e, 0x01, 0x3e, 0x18, "French-4"		},
	{0x1e, 0x01, 0x3e, 0x14, "French-11"		},
	{0x1e, 0x01, 0x3e, 0x12, "French-12"		},
	{0x1e, 0x01, 0x3e, 0x16, "French-ECA"		},
	{0x1e, 0x01, 0x3e, 0x10, "French-Free Play"		},

	{0   , 0xfe, 0   ,    2, "Counters"		},
	{0x1e, 0x01, 0x40, 0x40, "One"		},
	{0x1e, 0x01, 0x40, 0x00, "Two"		},

	{0   , 0xfe, 0   ,    2, "Bill Validator"		},
	{0x1f, 0x01, 0x01, 0x01, "Off"		},
	{0x1f, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Powerup Test"		},
	{0x1f, 0x01, 0x04, 0x00, "Off"		},
	{0x1f, 0x01, 0x04, 0x04, "On"		},

	{0   , 0xfe, 0   ,    2, "Test Switch"		},
	{0x1f, 0x01, 0x80, 0x80, "Off"		},
	{0x1f, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Service Mode (No Toggle)"		},
	{0x20, 0x01, 0x10, 0x10, "Off"		},
};

STDDIPINFO(Rmpgwt)

static struct BurnInputInfo WwfmaniaInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 2,	"p1 start"},
	{"P1 Up",		    BIT_DIGITAL,	nWolfUnitJoy1 + 0,	"p1 up"},
	{"P1 Down",		    BIT_DIGITAL,	nWolfUnitJoy1 + 1,	"p1 down"},
	{"P1 Left",		    BIT_DIGITAL,	nWolfUnitJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	nWolfUnitJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	nWolfUnitJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	nWolfUnitJoy1 + 5,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	nWolfUnitJoy1 + 6,	"p1 fire 3"},
	{"P1 Button 4",		BIT_DIGITAL,	nWolfUnitJoy2 + 0,	"p1 fire 4"},
	{"P1 Button 5",		BIT_DIGITAL,	nWolfUnitJoy2 + 1,	"p1 fire 5"},

	{"P2 Coin",		    BIT_DIGITAL,	nWolfUnitJoy3 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	nWolfUnitJoy3 + 5,	"p2 start"},
	{"P2 Up",		    BIT_DIGITAL,	nWolfUnitJoy1 + 8,	"p2 up"},
	{"P2 Down",		    BIT_DIGITAL,	nWolfUnitJoy1 + 9,	"p2 down"},
	{"P2 Left",		    BIT_DIGITAL,	nWolfUnitJoy1 + 10,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	nWolfUnitJoy1 + 11,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	nWolfUnitJoy1 + 12,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	nWolfUnitJoy1 + 13,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	nWolfUnitJoy1 + 14,	"p2 fire 3"},
	{"P2 Button 4",		BIT_DIGITAL,	nWolfUnitJoy2 + 4,	"p2 fire 4"},
	{"P2 Button 5",		BIT_DIGITAL,	nWolfUnitJoy2 + 5,	"p2 fire 5"},

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	        "reset"},
	{"Service",		    BIT_DIGITAL,	nWolfUnitJoy3 + 6,	"service"},
	{"Tilt",		    BIT_DIGITAL,	nWolfUnitJoy3 + 3,	"tilt"},
	{"Dip A",		    BIT_DIPSWITCH,	nWolfUnitDSW + 0,	"dip"},
	{"Dip B",		    BIT_DIPSWITCH,	nWolfUnitDSW + 1,	"dip"},
	{"Dip C",		    BIT_DIPSWITCH,	nWolfUnitDSW + 2,	"dip"},
};

STDINPUTINFO(Wwfmania)


static struct BurnDIPInfo WwfmaniaDIPList[]=
{
	{0x19, 0xff, 0xff, 0xfd, NULL		},
	{0x1a, 0xff, 0xff, 0x7f, NULL		},
	{0x1b, 0xff, 0xff, 0x10, NULL		},

	{0   , 0xfe, 0   ,    2, "Test Switch"		},
	{0x19, 0x01, 0x01, 0x01, "Off"		},
	{0x19, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Powerup Test"		},
	{0x19, 0x01, 0x02, 0x00, "Off"		},
	{0x19, 0x01, 0x02, 0x02, "On"		},

	{0   , 0xfe, 0   ,    2, "Realtime Clock"		},
	{0x19, 0x01, 0x08, 0x08, "No"		},
	{0x19, 0x01, 0x08, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Bill Validator"		},
	{0x19, 0x01, 0x40, 0x40, "Off"		},
	{0x19, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    3, "Counters"		},
	{0x1a, 0x01, 0x03, 0x03, "One, 1/1"		},
	{0x1a, 0x01, 0x03, 0x02, "One, Totalizing"		},
	{0x1a, 0x01, 0x03, 0x01, "Two, 1/1"		},

	{0   , 0xfe, 0   ,    3, "Country"		},
	{0x1a, 0x01, 0x0c, 0x0c, "USA"		},
	{0x1a, 0x01, 0x0c, 0x08, "French"		},
	{0x1a, 0x01, 0x0c, 0x04, "German"		},

	{0   , 0xfe, 0   ,    6, "Coinage"		},
	{0x1a, 0x01, 0x70, 0x70, "1"		},
	{0x1a, 0x01, 0x70, 0x30, "2"		},
	{0x1a, 0x01, 0x70, 0x50, "3"		},
	{0x1a, 0x01, 0x70, 0x10, "4"		},
	{0x1a, 0x01, 0x70, 0x60, "ECA"		},
	{0x1a, 0x01, 0x70, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Coinage Source"		},
	{0x1a, 0x01, 0x80, 0x80, "Dipswitch"		},
	{0x1a, 0x01, 0x80, 0x00, "CMOS"		},

	{0   , 0xfe, 0   ,    1, "Service Mode (No Toggle)"		},
	{0x1b, 0x01, 0x10, 0x10, "Off"		},
};

STDDIPINFO(Wwfmania)

// Mortal Kombat 3 (rev 2.1)

static struct BurnRomInfo mk3RomDesc[] = {
	{ "mk321u54.bin",	0x080000, 0x9e344401, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "mk321u63.bin",	0x080000, 0x64d34776, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "l1_mortal_kombat_3_u2_music_spch.u2",		0x100000, 0x5273436f, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_3_u3_music_spch.u3",		0x100000, 0x856fe411, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_3_u4_music_spch.u4",		0x100000, 0x428a406f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_3_u5_music_spch.u5",		0x100000, 0x3b98a09f, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "l1_mortal_kombat_3_u133_game_rom.u133",	0x100000, 0x79b94667, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "l1_mortal_kombat_3_u132_game_rom.u132",	0x100000, 0x13e95228, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "l1_mortal_kombat_3_u131_game_rom.u131",	0x100000, 0x41001e30, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "l1_mortal_kombat_3_u130_game_rom.u130",	0x100000, 0x49379dd7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "l1_mortal_kombat_3_u129_game_rom.u129",	0x100000, 0xa8b41803, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "l1_mortal_kombat_3_u128_game_rom.u128",	0x100000, 0xb410d72f, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "l1_mortal_kombat_3_u127_game_rom.u127",	0x100000, 0xbd985be7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "l1_mortal_kombat_3_u126_game_rom.u126",	0x100000, 0xe7c32cf4, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "l1_mortal_kombat_3_u125_game_rom.u125",	0x100000, 0x9a52227e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "l1_mortal_kombat_3_u124_game_rom.u124",	0x100000, 0x5c750ebc, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "l1_mortal_kombat_3_u123_game_rom.u123",	0x100000, 0xf0ab88a8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "l1_mortal_kombat_3_u122_game_rom.u122",	0x100000, 0x9b87cdac, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "l1_mortal_kombat_3_u121_game_rom.u121",	0x100000, 0xb6c6296a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "l1_mortal_kombat_3_u120_game_rom.u120",	0x100000, 0x8d1ccc3b, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "l1_mortal_kombat_3_u119_game_rom.u119",	0x100000, 0x63215b59, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "l1_mortal_kombat_3_u118_game_rom.u118",	0x100000, 0x8b681e34, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21

	{ "l1_mortal_kombat_3_u117_game_rom.u117",	0x080000, 0x1ab20377, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 0) }, // 22
	{ "l1_mortal_kombat_3_u116_game_rom.u116",	0x080000, 0xba246ad0, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 1) }, // 23
	{ "l1_mortal_kombat_3_u115_game_rom.u115",	0x080000, 0x3ee8b124, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 2) }, // 24
	{ "l1_mortal_kombat_3_u114_game_rom.u114",	0x080000, 0xa8d99922, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 3) }, // 25
};

STD_ROM_PICK(mk3)
STD_ROM_FN(mk3)

struct BurnDriver BurnDrvMk3 = {
	"mk3", NULL, NULL, NULL, "1994",
	"Mortal Kombat 3 (rev 2.1)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mk3RomInfo, mk3RomName, NULL, NULL, Mk3InputInfo, Mk3DIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// Mortal Kombat 3 (rev 2.0)

static struct BurnRomInfo mk3r20RomDesc[] = {
	{ "mk320u54.bin",	0x080000, 0x453da302, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "mk320u63.bin",	0x080000, 0xf8dc0600, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "l1_mortal_kombat_3_u2_music_spch.u2",		0x100000, 0x5273436f, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_3_u3_music_spch.u3",		0x100000, 0x856fe411, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_3_u4_music_spch.u4",		0x100000, 0x428a406f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_3_u5_music_spch.u5",		0x100000, 0x3b98a09f, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "l1_mortal_kombat_3_u133_game_rom.u133",	0x100000, 0x79b94667, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "l1_mortal_kombat_3_u132_game_rom.u132",	0x100000, 0x13e95228, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "l1_mortal_kombat_3_u131_game_rom.u131",	0x100000, 0x41001e30, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "l1_mortal_kombat_3_u130_game_rom.u130",	0x100000, 0x49379dd7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "l1_mortal_kombat_3_u129_game_rom.u129",	0x100000, 0xa8b41803, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "l1_mortal_kombat_3_u128_game_rom.u128",	0x100000, 0xb410d72f, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "l1_mortal_kombat_3_u127_game_rom.u127",	0x100000, 0xbd985be7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "l1_mortal_kombat_3_u126_game_rom.u126",	0x100000, 0xe7c32cf4, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "l1_mortal_kombat_3_u125_game_rom.u125",	0x100000, 0x9a52227e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "l1_mortal_kombat_3_u124_game_rom.u124",	0x100000, 0x5c750ebc, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "l1_mortal_kombat_3_u123_game_rom.u123",	0x100000, 0xf0ab88a8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "l1_mortal_kombat_3_u122_game_rom.u122",	0x100000, 0x9b87cdac, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "l1_mortal_kombat_3_u121_game_rom.u121",	0x100000, 0xb6c6296a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "l1_mortal_kombat_3_u120_game_rom.u120",	0x100000, 0x8d1ccc3b, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "l1_mortal_kombat_3_u119_game_rom.u119",	0x100000, 0x63215b59, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "l1_mortal_kombat_3_u118_game_rom.u118",	0x100000, 0x8b681e34, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21

	{ "l1_mortal_kombat_3_u117_game_rom.u117",	0x080000, 0x1ab20377, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 0) }, // 22
	{ "l1_mortal_kombat_3_u116_game_rom.u116",	0x080000, 0xba246ad0, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 1) }, // 23
	{ "l1_mortal_kombat_3_u115_game_rom.u115",	0x080000, 0x3ee8b124, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 2) }, // 24
	{ "l1_mortal_kombat_3_u114_game_rom.u114",	0x080000, 0xa8d99922, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 3) }, // 25
};

STD_ROM_PICK(mk3r20)
STD_ROM_FN(mk3r20)

struct BurnDriver BurnDrvMk3r20 = {
	"mk3r20", "mk3", NULL, NULL, "1994",
	"Mortal Kombat 3 (rev 2.0)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mk3r20RomInfo, mk3r20RomName, NULL, NULL, Mk3InputInfo, Mk3DIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// Mortal Kombat 3 (rev 1.0)

static struct BurnRomInfo mk3r10RomDesc[] = {
	{ "mk310u54.bin",	0x080000, 0x41829228, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "mk310u63.bin",	0x080000, 0xb074e1e8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "l1_mortal_kombat_3_u2_music_spch.u2",		0x100000, 0x5273436f, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_3_u3_music_spch.u3",		0x100000, 0x856fe411, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_3_u4_music_spch.u4",		0x100000, 0x428a406f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_3_u5_music_spch.u5",		0x100000, 0x3b98a09f, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "l1_mortal_kombat_3_u133_game_rom.u133",	0x100000, 0x79b94667, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "l1_mortal_kombat_3_u132_game_rom.u132",	0x100000, 0x13e95228, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "l1_mortal_kombat_3_u131_game_rom.u131",	0x100000, 0x41001e30, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "l1_mortal_kombat_3_u130_game_rom.u130",	0x100000, 0x49379dd7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "l1_mortal_kombat_3_u129_game_rom.u129",	0x100000, 0xa8b41803, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "l1_mortal_kombat_3_u128_game_rom.u128",	0x100000, 0xb410d72f, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "l1_mortal_kombat_3_u127_game_rom.u127",	0x100000, 0xbd985be7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "l1_mortal_kombat_3_u126_game_rom.u126",	0x100000, 0xe7c32cf4, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "l1_mortal_kombat_3_u125_game_rom.u125",	0x100000, 0x9a52227e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "l1_mortal_kombat_3_u124_game_rom.u124",	0x100000, 0x5c750ebc, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "l1_mortal_kombat_3_u123_game_rom.u123",	0x100000, 0xf0ab88a8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "l1_mortal_kombat_3_u122_game_rom.u122",	0x100000, 0x9b87cdac, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "l1_mortal_kombat_3_u121_game_rom.u121",	0x100000, 0xb6c6296a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "l1_mortal_kombat_3_u120_game_rom.u120",	0x100000, 0x8d1ccc3b, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "l1_mortal_kombat_3_u119_game_rom.u119",	0x100000, 0x63215b59, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "l1_mortal_kombat_3_u118_game_rom.u118",	0x100000, 0x8b681e34, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21

	{ "l1_mortal_kombat_3_u117_game_rom.u117",	0x080000, 0x1ab20377, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 0) }, // 22
	{ "l1_mortal_kombat_3_u116_game_rom.u116",	0x080000, 0xba246ad0, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 1) }, // 23
	{ "l1_mortal_kombat_3_u115_game_rom.u115",	0x080000, 0x3ee8b124, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 2) }, // 24
	{ "l1_mortal_kombat_3_u114_game_rom.u114",	0x080000, 0xa8d99922, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 3) }, // 25
};

STD_ROM_PICK(mk3r10)
STD_ROM_FN(mk3r10)

struct BurnDriver BurnDrvMk3r10 = {
	"mk3r10", "mk3", NULL, NULL, "1994",
	"Mortal Kombat 3 (rev 1.0)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mk3r10RomInfo, mk3r10RomName, NULL, NULL, Mk3InputInfo, Mk3DIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// Mortal Kombat 3 (rev 1 chip label p4.0)

static struct BurnRomInfo mk3p40RomDesc[] = {
	{ "mk3p40.u54",		0x080000, 0x4dfb0748, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "mk3p40.u63",		0x080000, 0xf25a8083, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "l1_mortal_kombat_3_u2_music_spch.u2",		0x100000, 0x5273436f, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_3_u3_music_spch.u3",		0x100000, 0x856fe411, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_3_u4_music_spch.u4",		0x100000, 0x428a406f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_3_u5_music_spch.u5",		0x100000, 0x3b98a09f, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "l1_mortal_kombat_3_u133_game_rom.u133",	0x100000, 0x79b94667, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "l1_mortal_kombat_3_u132_game_rom.u132",	0x100000, 0x13e95228, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "l1_mortal_kombat_3_u131_game_rom.u131",	0x100000, 0x41001e30, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "l1_mortal_kombat_3_u130_game_rom.u130",	0x100000, 0x49379dd7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "l1_mortal_kombat_3_u129_game_rom.u129",	0x100000, 0xa8b41803, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "l1_mortal_kombat_3_u128_game_rom.u128",	0x100000, 0xb410d72f, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "l1_mortal_kombat_3_u127_game_rom.u127",	0x100000, 0xbd985be7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "l1_mortal_kombat_3_u126_game_rom.u126",	0x100000, 0xe7c32cf4, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "l1_mortal_kombat_3_u125_game_rom.u125",	0x100000, 0x9a52227e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "l1_mortal_kombat_3_u124_game_rom.u124",	0x100000, 0x5c750ebc, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "l1_mortal_kombat_3_u123_game_rom.u123",	0x100000, 0xf0ab88a8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "l1_mortal_kombat_3_u122_game_rom.u122",	0x100000, 0x9b87cdac, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "l1_mortal_kombat_3_u121_game_rom.u121",	0x100000, 0xb6c6296a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "l1_mortal_kombat_3_u120_game_rom.u120",	0x100000, 0x8d1ccc3b, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "l1_mortal_kombat_3_u119_game_rom.u119",	0x100000, 0x63215b59, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "l1_mortal_kombat_3_u118_game_rom.u118",	0x100000, 0x8b681e34, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21

	{ "l1_mortal_kombat_3_u117_game_rom.u117",	0x080000, 0x1ab20377, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 0) }, // 22
	{ "l1_mortal_kombat_3_u116_game_rom.u116",	0x080000, 0xba246ad0, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 1) }, // 23
	{ "l1_mortal_kombat_3_u115_game_rom.u115",	0x080000, 0x3ee8b124, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 2) }, // 24
	{ "l1_mortal_kombat_3_u114_game_rom.u114",	0x080000, 0xa8d99922, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 3) }, // 25
};

STD_ROM_PICK(mk3p40)
STD_ROM_FN(mk3p40)

struct BurnDriver BurnDrvMk3p40 = {
	"mk3p40", "mk3", NULL, NULL, "1994",
	"Mortal Kombat 3 (rev 1 chip label p4.0)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, mk3p40RomInfo, mk3p40RomName, NULL, NULL, Mk3InputInfo, Mk3DIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// Ultimate Mortal Kombat 3 (rev 1.2)

static struct BurnRomInfo umk3RomDesc[] = {
	{ "l1.2_mortal_kombat_3_u54_ultimate.u54",	0x080000, 0x712b4db6, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "l1.2_mortal_kombat_3_u63_ultimate.u63",	0x080000, 0x6d301faf, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "l2.0_mortal_kombat_3_u2_ultimate.u2",	0x100000, 0x3838cfe5, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_3_u3_music_spch.u3",	0x100000, 0x856fe411, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_3_u4_music_spch.u4",	0x100000, 0x428a406f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_3_u5_music_spch.u5",	0x100000, 0x3b98a09f, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "l1_mortal_kombat_3_u133_game_rom.u133",	0x100000, 0x79b94667, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "l1_mortal_kombat_3_u132_game_rom.u132",	0x100000, 0x13e95228, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "l1_mortal_kombat_3_u131_game_rom.u131",	0x100000, 0x41001e30, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "l1_mortal_kombat_3_u130_game_rom.u130",	0x100000, 0x49379dd7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "l1_mortal_kombat_3_u129_game_rom.u129",	0x100000, 0xa8b41803, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "l1_mortal_kombat_3_u128_game_rom.u128",	0x100000, 0xb410d72f, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "l1_mortal_kombat_3_u127_game_rom.u127",	0x100000, 0xbd985be7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "l1_mortal_kombat_3_u126_game_rom.u126",	0x100000, 0xe7c32cf4, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "l1_mortal_kombat_3_u125_game_rom.u125",	0x100000, 0x9a52227e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "l1_mortal_kombat_3_u124_game_rom.u124",	0x100000, 0x5c750ebc, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "l1_mortal_kombat_3_u123_game_rom.u123",	0x100000, 0xf0ab88a8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "l1_mortal_kombat_3_u122_game_rom.u122",	0x100000, 0x9b87cdac, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "umk-u121.bin",	0x100000, 0xcc4b95db, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "umk-u120.bin",	0x100000, 0x1c8144cd, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "umk-u119.bin",	0x100000, 0x5f10c543, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "umk-u118.bin",	0x100000, 0xde0c4488, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21

	{ "umk-u113.bin",	0x100000, 0x99d74a1e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 0) }, // 22
	{ "umk-u112.bin",	0x100000, 0xb5a46488, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 1) }, // 23
	{ "umk-u111.bin",	0x100000, 0xa87523c8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 2) }, // 24
	{ "umk-u110.bin",	0x100000, 0x0038f205, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 3) }, // 25
	
	{ "463_MK3_Ultimate.u64", 0x1009, 0x4f425218, 0 | BRF_OPT },
};

STD_ROM_PICK(umk3)
STD_ROM_FN(umk3)

struct BurnDriver BurnDrvUmk3 = {
	"umk3", NULL, NULL, NULL, "1994",
	"Ultimate Mortal Kombat 3 (rev 1.2)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, umk3RomInfo, umk3RomName, NULL, NULL, Mk3InputInfo, Mk3DIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// Ultimate Mortal Kombat 3 (rev 1.1)

static struct BurnRomInfo umk3r11RomDesc[] = {
	{ "l1.1_mortal_kombat_3_u54_ultimate.u54",	0x080000, 0x8bb27659, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "l1.1_mortal_kombat_3_u63_ultimate.u63",	0x080000, 0xea731783, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "l2.0_mortal_kombat_3_u2_ultimate.u2",	0x100000, 0x3838cfe5, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_3_u3_music_spch.u3",	0x100000, 0x856fe411, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_3_u4_music_spch.u4",	0x100000, 0x428a406f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_3_u5_music_spch.u5",	0x100000, 0x3b98a09f, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "l1_mortal_kombat_3_u133_game_rom.u133",	0x100000, 0x79b94667, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "l1_mortal_kombat_3_u132_game_rom.u132",	0x100000, 0x13e95228, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "l1_mortal_kombat_3_u131_game_rom.u131",	0x100000, 0x41001e30, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "l1_mortal_kombat_3_u130_game_rom.u130",	0x100000, 0x49379dd7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "l1_mortal_kombat_3_u129_game_rom.u129",	0x100000, 0xa8b41803, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "l1_mortal_kombat_3_u128_game_rom.u128",	0x100000, 0xb410d72f, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "l1_mortal_kombat_3_u127_game_rom.u127",	0x100000, 0xbd985be7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "l1_mortal_kombat_3_u126_game_rom.u126",	0x100000, 0xe7c32cf4, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "l1_mortal_kombat_3_u125_game_rom.u125",	0x100000, 0x9a52227e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "l1_mortal_kombat_3_u124_game_rom.u124",	0x100000, 0x5c750ebc, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "l1_mortal_kombat_3_u123_game_rom.u123",	0x100000, 0xf0ab88a8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "l1_mortal_kombat_3_u122_game_rom.u122",	0x100000, 0x9b87cdac, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "mortal_kombat_iii_ultimate_u121_video_image.u121",	0x100000, 0xcc4b95db, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "mortal_kombat_iii_ultimate_u120_video_image.u120",	0x100000, 0x1c8144cd, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "mortal_kombat_iii_ultimate_u119_video_image.u119",	0x100000, 0x5f10c543, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "mortal_kombat_iii_ultimate_u118_video_image.u118",	0x100000, 0xde0c4488, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21

	{ "mortal_kombat_iii_ultimate_u113_video_image.u113",	0x100000, 0x99d74a1e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 0) }, // 22
	{ "mortal_kombat_iii_ultimate_u112_video_image.u112",	0x100000, 0xb5a46488, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 1) }, // 23
	{ "mortal_kombat_iii_ultimate_u111_video_image.u111",	0x100000, 0xa87523c8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 2) }, // 24
	{ "mortal_kombat_iii_ultimate_u110_video_image.u110",	0x100000, 0x0038f205, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 3) }, // 25
	
	{ "463_MK3_Ultimate.u64", 0x1009, 0x4f425218, 0 | BRF_OPT },
};

STD_ROM_PICK(umk3r11)
STD_ROM_FN(umk3r11)

struct BurnDriver BurnDrvUmk3r11 = {
	"umk3r11", "umk3", NULL, NULL, "1994",
	"Ultimate Mortal Kombat 3 (rev 1.1)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, umk3r11RomInfo, umk3r11RomName, NULL, NULL, Mk3InputInfo, Mk3DIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// Ultimate Mortal Kombat 3 (rev 1.0)

static struct BurnRomInfo umk3r10RomDesc[] = {
	{ "l1.0_mortal_kombat_3_u54_ultimate.u54",	0x080000, 0xdfd735da, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "l1.0_mortal_kombat_3_u63_ultimate.u63",	0x080000, 0x2dff0c83, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "l1.0_mortal_kombat_3_u2_ultimate.u2",	0x100000, 0x3838cfe5, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "l1_mortal_kombat_3_u3_music_spch.u3",	0x100000, 0x856fe411, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "l1_mortal_kombat_3_u4_music_spch.u4",	0x100000, 0x428a406f, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "l1_mortal_kombat_3_u5_music_spch.u5",	0x100000, 0x3b98a09f, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "l1_mortal_kombat_3_u133_game_rom.u133",	0x100000, 0x79b94667, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "l1_mortal_kombat_3_u132_game_rom.u132",	0x100000, 0x13e95228, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "l1_mortal_kombat_3_u131_game_rom.u131",	0x100000, 0x41001e30, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "l1_mortal_kombat_3_u130_game_rom.u130",	0x100000, 0x49379dd7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "l1_mortal_kombat_3_u129_game_rom.u129",	0x100000, 0xa8b41803, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "l1_mortal_kombat_3_u128_game_rom.u128",	0x100000, 0xb410d72f, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "l1_mortal_kombat_3_u127_game_rom.u127",	0x100000, 0xbd985be7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "l1_mortal_kombat_3_u126_game_rom.u126",	0x100000, 0xe7c32cf4, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "l1_mortal_kombat_3_u125_game_rom.u125",	0x100000, 0x9a52227e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "l1_mortal_kombat_3_u124_game_rom.u124",	0x100000, 0x5c750ebc, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "l1_mortal_kombat_3_u123_game_rom.u123",	0x100000, 0xf0ab88a8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "l1_mortal_kombat_3_u122_game_rom.u122",	0x100000, 0x9b87cdac, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "mortal_kombat_iii_ultimate_u121_video_image.u121",	0x100000, 0xcc4b95db, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "mortal_kombat_iii_ultimate_u120_video_image.u120",	0x100000, 0x1c8144cd, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "mortal_kombat_iii_ultimate_u119_video_image.u119",	0x100000, 0x5f10c543, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "mortal_kombat_iii_ultimate_u118_video_image.u118",	0x100000, 0xde0c4488, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21

	{ "mortal_kombat_iii_ultimate_u113_video_image.u113",	0x100000, 0x99d74a1e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 0) }, // 22
	{ "mortal_kombat_iii_ultimate_u112_video_image.u112",	0x100000, 0xb5a46488, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 1) }, // 23
	{ "mortal_kombat_iii_ultimate_u111_video_image.u111",	0x100000, 0xa87523c8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 2) }, // 24
	{ "mortal_kombat_iii_ultimate_u110_video_image.u110",	0x100000, 0x0038f205, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 3) }, // 25
	
	{ "463_MK3_Ultimate.u64", 0x1009, 0x4f425218, 0 | BRF_OPT },
};

STD_ROM_PICK(umk3r10)
STD_ROM_FN(umk3r10)

struct BurnDriver BurnDrvUmk3r10 = {
	"umk3r10", "umk3", NULL, NULL, "1994",
	"Ultimate Mortal Kombat 3 (rev 1.0)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, umk3r10RomInfo, umk3r10RomName, NULL, NULL, Mk3InputInfo, Mk3DIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// 2 On 2 Open Ice Challenge (rev 1.21)

static struct BurnRomInfo openiceRomDesc[] = {
	{ "open_ice_l1.21.u54",	0x080000, 0xe4225284, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "open_ice_l1.21.u63",	0x080000, 0x97d308a3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "open_ice_l1.2.u2",	0x100000, 0x8adb5aab, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "open_ice_l1.u3",		0x100000, 0x11c61ad6, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "open_ice_l1.u4",		0x100000, 0x04279290, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "open_ice_l1.u5",		0x100000, 0xe90ad61f, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "open_ice_l1.2.u133",	0x100000, 0x8a81605c, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "open_ice_l1.2.u132",	0x100000, 0xcfdd6702, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "open_ice_l1.2.u131",	0x100000, 0xcc428eb7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "open_ice_l1.2.u130",	0x100000, 0x74c2d50c, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "open_ice_l1.2.u129",	0x100000, 0x9e2ff012, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "open_ice_l1.2.u128",	0x100000, 0x35d2e610, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "open_ice_l1.2.u127",	0x100000, 0xbcbf19fe, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "open_ice_l1.2.u126",	0x100000, 0x8e3106ae, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "open_ice_l1.u125",	0x100000, 0xa7b54550, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "open_ice_l1.u124",	0x100000, 0x7c02cb50, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "open_ice_l1.u123",	0x100000, 0xd543bd9d, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "open_ice_l1.u122",	0x100000, 0x3744d291, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "open_ice_l1.2.u121",	0x100000, 0xacd2f7c7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "open_ice_l1.2.u120",	0x100000, 0x4295686a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "open_ice_l1.2.u119",	0x100000, 0x948b9b27, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "open_ice_l1.2.u118",	0x100000, 0x9eaaf93e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21
};

STD_ROM_PICK(openice)
STD_ROM_FN(openice)

struct BurnDriver BurnDrvOpenice = {
	"openice", NULL, NULL, NULL, "1995",
	"2 On 2 Open Ice Challenge (rev 1.21)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, openiceRomInfo, openiceRomName, NULL, NULL, OpeniceInputInfo, OpeniceDIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// NBA Hangtime (rev L1.1 04/16/96)

static struct BurnRomInfo nbahangtRomDesc[] = {
	{ "htime54.bin",	0x080000, 0xc2875d98, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "htime63.bin",	0x080000, 0x6f4728c3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mhtu2.bin",		0x100000, 0x3f0b0d0a, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "mhtu3.bin",		0x100000, 0xec1db988, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "mhtu4.bin",		0x100000, 0xc7f847a3, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mhtu5.bin",		0x100000, 0xef19316a, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "mhtu133.bin",	0x100000, 0x3163feed, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "mhtu132.bin",	0x100000, 0x428eaf44, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "mhtu131.bin",	0x100000, 0x5f7c5111, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "mhtu130.bin",	0x100000, 0xc7c0c514, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "mhtu129.bin",	0x100000, 0xb3d0daa0, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "mhtu128.bin",	0x100000, 0x3704ee69, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "mhtu127.bin",	0x100000, 0x4ea64d5a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "mhtu126.bin",	0x100000, 0x0c5c19b7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "mhtu125.bin",	0x100000, 0x46c43d67, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "mhtu124.bin",	0x100000, 0xed495156, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "mhtu123.bin",	0x100000, 0xb48aa5da, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "mhtu122.bin",	0x100000, 0xb18cd181, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "mhtu121.bin",	0x100000, 0x5acb267a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "mhtu120.bin",	0x100000, 0x28e05f86, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "mhtu119.bin",	0x100000, 0xb4f604ea, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "mhtu118.bin",	0x100000, 0xa257b973, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21

	{ "mhtu113.bin",	0x100000, 0xd712a779, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 0) }, // 22
	{ "mhtu112.bin",	0x100000, 0x644e1bca, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 1) }, // 23
	{ "mhtu111.bin",	0x100000, 0x10d3b768, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 2) }, // 24
	{ "mhtu110.bin",	0x100000, 0x8575aeb2, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 3) }, // 25
};

STD_ROM_PICK(nbahangt)
STD_ROM_FN(nbahangt)

struct BurnDriver BurnDrvNbahangt = {
	"nbahangt", NULL, NULL, NULL, "1996",
	"NBA Hangtime (rev L1.1 04/16/96)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, nbahangtRomInfo, nbahangtRomName, NULL, NULL, NbahangtInputInfo, NbahangtDIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// NBA Maximum Hangtime (rev 1.03 06/09/97)

static struct BurnRomInfo nbamhtRomDesc[] = {
	{ "mhtu54_v103.bin",	0x080000, 0x21b0d9e1, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "mhtu63_v103.bin",	0x080000, 0xc6fdbb97, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mhtu2.bin",		0x100000, 0x3f0b0d0a, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "mhtu3.bin",		0x100000, 0xec1db988, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "mhtu4.bin",		0x100000, 0xc7f847a3, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mhtu5.bin",		0x100000, 0xef19316a, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "mhtu133.bin",	0x100000, 0x3163feed, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "mhtu132.bin",	0x100000, 0x428eaf44, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "mhtu131.bin",	0x100000, 0x5f7c5111, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "mhtu130.bin",	0x100000, 0xc7c0c514, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "mhtu129.bin",	0x100000, 0xb3d0daa0, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "mhtu128.bin",	0x100000, 0x3704ee69, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "mhtu127.bin",	0x100000, 0x4ea64d5a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "mhtu126.bin",	0x100000, 0x0c5c19b7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "mhtu125.bin",	0x100000, 0x46c43d67, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "mhtu124.bin",	0x100000, 0xed495156, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "mhtu123.bin",	0x100000, 0xb48aa5da, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "mhtu122.bin",	0x100000, 0xb18cd181, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "mhtu121.bin",	0x100000, 0x5acb267a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "mhtu120.bin",	0x100000, 0x28e05f86, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "mhtu119.bin",	0x100000, 0xb4f604ea, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "mhtu118.bin",	0x100000, 0xa257b973, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21

	{ "mhtu113.bin",	0x100000, 0xd712a779, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 0) }, // 22
	{ "mhtu112.bin",	0x100000, 0x644e1bca, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 1) }, // 23
	{ "mhtu111.bin",	0x100000, 0x10d3b768, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 2) }, // 24
	{ "mhtu110.bin",	0x100000, 0x8575aeb2, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 3) }, // 25
};

STD_ROM_PICK(nbamht)
STD_ROM_FN(nbamht)

struct BurnDriver BurnDrvNbamht = {
	"nbamht", NULL, NULL, NULL, "1996",
	"NBA Maximum Hangtime (rev 1.03 06/09/97)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, nbamhtRomInfo, nbamhtRomName, NULL, NULL, NbahangtInputInfo, NbahangtDIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// NBA Maximum Hangtime (rev 1.0 11/08/96)

static struct BurnRomInfo nbamht1RomDesc[] = {
	{ "mhtu54_v10.bin",	0x080000, 0xdfb6b3ae, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "mhtu63_v10.bin",	0x080000, 0x78da472c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mhtu2.bin",		0x100000, 0x3f0b0d0a, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "mhtu3.bin",		0x100000, 0xec1db988, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "mhtu4.bin",		0x100000, 0xc7f847a3, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "mhtu5.bin",		0x100000, 0xef19316a, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "mhtu133.bin",	0x100000, 0x3163feed, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "mhtu132.bin",	0x100000, 0x428eaf44, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "mhtu131.bin",	0x100000, 0x5f7c5111, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "mhtu130.bin",	0x100000, 0xc7c0c514, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "mhtu129.bin",	0x100000, 0xb3d0daa0, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "mhtu128.bin",	0x100000, 0x3704ee69, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "mhtu127.bin",	0x100000, 0x4ea64d5a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "mhtu126.bin",	0x100000, 0x0c5c19b7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "mhtu125.bin",	0x100000, 0x46c43d67, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "mhtu124.bin",	0x100000, 0xed495156, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "mhtu123.bin",	0x100000, 0xb48aa5da, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "mhtu122.bin",	0x100000, 0xb18cd181, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "mhtu121.bin",	0x100000, 0x5acb267a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "mhtu120.bin",	0x100000, 0x28e05f86, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "mhtu119.bin",	0x100000, 0xb4f604ea, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "mhtu118.bin",	0x100000, 0xa257b973, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21

	{ "mhtu113.bin",	0x100000, 0xd712a779, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 0) }, // 22
	{ "mhtu112.bin",	0x100000, 0x644e1bca, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 1) }, // 23
	{ "mhtu111.bin",	0x100000, 0x10d3b768, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 2) }, // 24
	{ "mhtu110.bin",	0x100000, 0x8575aeb2, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x14, 3) }, // 25
};

STD_ROM_PICK(nbamht1)
STD_ROM_FN(nbamht1)

struct BurnDriver BurnDrvNbamht1 = {
	"nbamht1", "nbamht", NULL, NULL, "1996",
	"NBA Maximum Hangtime (rev 1.0 11/08/96)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, nbamht1RomInfo, nbamht1RomName, NULL, NULL, NbahangtInputInfo, NbahangtDIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// Rampage: World Tour (rev 1.3)

static struct BurnRomInfo rmpgwtRomDesc[] = {
	{ "1.3_rampage_world_u54_game.u54",	0x080000, 0x2a8f6e1e, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "1.3_rampage_world_u63_game.u63",	0x080000, 0x403ae41e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.0_rampage_world_tour_u2_sound.u2",	0x100000, 0x0e82f83d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "1.0_rampage_world_tour_u3_sound.u3",	0x100000, 0x3ff54d15, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "1.0_rampage_world_tour_u4_sound.u4",	0x100000, 0x5c7f5656, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "1.0_rampage_world_tour_u5_sound.u5",	0x100000, 0xfd9aaf24, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "1.0_rampage_world_tour_u133_image.u133",	0x100000, 0x5b5ac449, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "1.0_rampage_world_tour_u132_image.u132",	0x100000, 0x7b3f09c6, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "1.0_rampage_world_tour_u131_image.u131",	0x100000, 0xfdecf12e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "1.0_rampage_world_tour_u130_image.u130",	0x100000, 0x4a983b05, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "1.0_rampage_world_tour_u129_image.u129",	0x100000, 0xdc495c6e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "1.0_rampage_world_tour_u128_image.u128",	0x100000, 0x5545503d, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "1.0_rampage_world_tour_u127_image.u127",	0x100000, 0x6e1756ba, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "1.0_rampage_world_tour_u126_image.u126",	0x100000, 0xc300eb1b, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "1.0_rampage_world_tour_u125_image.u125",	0x100000, 0x7369bf5d, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "1.0_rampage_world_tour_u124_image.u124",	0x100000, 0xc0bf88c8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "1.0_rampage_world_tour_u123_image.u123",	0x100000, 0xac4c712a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "1.0_rampage_world_tour_u122_image.u122",	0x100000, 0x609862a2, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "1.0_rampage_world_tour_u121_image.u121",	0x100000, 0xf65119b7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "1.0_rampage_world_tour_u120_image.u120",	0x100000, 0x6d643dee, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "1.0_rampage_world_tour_u119_image.u119",	0x100000, 0x4e49c133, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "1.0_rampage_world_tour_u118_image.u118",	0x100000, 0x43a6f51e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21
	
	{ "465 Rampage WT.u64",	0x001009, 0x5c14d850, 0 | BRF_OPT },
};

STD_ROM_PICK(rmpgwt)
STD_ROM_FN(rmpgwt)

struct BurnDriver BurnDrvRmpgwt = {
	"rmpgwt", NULL, NULL, NULL, "1997",
	"Rampage: World Tour (rev 1.3)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rmpgwtRomInfo, rmpgwtRomName, NULL, NULL, RmpgwtInputInfo, RmpgwtDIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// Rampage: World Tour (rev 1.1)

static struct BurnRomInfo rmpgwt11RomDesc[] = {
	{ "1.1_rampage_world_u54_game.u54",	0x080000, 0x3aa514eb, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "1.1_rampage_world_u63_game.u63",	0x080000, 0x031c908f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "1.0_rampage_world_tour_u2_sound.u2",	0x100000, 0x0e82f83d, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "1.0_rampage_world_tour_u3_sound.u3",	0x100000, 0x3ff54d15, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "1.0_rampage_world_tour_u4_sound.u4",	0x100000, 0x5c7f5656, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "1.0_rampage_world_tour_u5_sound.u5",	0x100000, 0xfd9aaf24, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "1.0_rampage_world_tour_u133_image.u133",	0x100000, 0x5b5ac449, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "1.0_rampage_world_tour_u132_image.u132",	0x100000, 0x7b3f09c6, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "1.0_rampage_world_tour_u131_image.u131",	0x100000, 0xfdecf12e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "1.0_rampage_world_tour_u130_image.u130",	0x100000, 0x4a983b05, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "1.0_rampage_world_tour_u129_image.u129",	0x100000, 0xdc495c6e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "1.0_rampage_world_tour_u128_image.u128",	0x100000, 0x5545503d, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "1.0_rampage_world_tour_u127_image.u127",	0x100000, 0x6e1756ba, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "1.0_rampage_world_tour_u126_image.u126",	0x100000, 0xc300eb1b, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "1.0_rampage_world_tour_u125_image.u125",	0x100000, 0x7369bf5d, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "1.0_rampage_world_tour_u124_image.u124",	0x100000, 0xc0bf88c8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "1.0_rampage_world_tour_u123_image.u123",	0x100000, 0xac4c712a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "1.0_rampage_world_tour_u122_image.u122",	0x100000, 0x609862a2, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "1.0_rampage_world_tour_u121_image.u121",	0x100000, 0xf65119b7, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "1.0_rampage_world_tour_u120_image.u120",	0x100000, 0x6d643dee, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "1.0_rampage_world_tour_u119_image.u119",	0x100000, 0x4e49c133, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "1.0_rampage_world_tour_u118_image.u118",	0x100000, 0x43a6f51e, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21
	
	{ "465 Rampage WT.u64",	0x010009, 0x5c14d850, 0 | BRF_OPT },
};

STD_ROM_PICK(rmpgwt11)
STD_ROM_FN(rmpgwt11)

struct BurnDriver BurnDrvRmpgwt11 = {
	"rmpgwt11", "rmpgwt", NULL, NULL, "1997",
	"Rampage: World Tour (rev 1.1)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, rmpgwt11RomInfo, rmpgwt11RomName, NULL, NULL, RmpgwtInputInfo, RmpgwtDIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// WWF: Wrestlemania (rev 1.30 08/10/95)

static struct BurnRomInfo wwfmaniaRomDesc[] = {
	{ "wwf_game_rom_l1.30.u54",	0x080000, 0xeeb7bf58, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "wwf_game_rom_l1.30.u63",	0x080000, 0x09759529, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "wwf_music-spch_l1.u2",	0x100000, 0xa9acb250, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "wwf_music-spch_l1.u3",	0x100000, 0x9442b6c9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "wwf_music-spch_l1.u4",	0x100000, 0xcee78fac, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "wwf_music-spch_l1.u5",	0x100000, 0x5b31fd40, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "wwf_image_rom_l1.u133",	0x100000, 0x5e1b1e3d, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "wwf_image_rom_l1.u132",	0x100000, 0x5943b3b2, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "wwf_image_rom_l1.u131",	0x100000, 0x0815db22, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "wwf_image_rom_l1.u130",	0x100000, 0x9ee9a145, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "wwf_image_rom_l1.u129",	0x100000, 0xc644c2f4, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "wwf_image_rom_l1.u128",	0x100000, 0xfcda4e9a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "wwf_image_rom_l1.u127",	0x100000, 0x45be7428, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "wwf_image_rom_l1.u126",	0x100000, 0xeaa276a8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "wwf_image_rom_l1.u125",	0x100000, 0xa19ebeed, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "wwf_image_rom_l1.u124",	0x100000, 0xdc7d3dbb, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "wwf_image_rom_l1.u123",	0x100000, 0xe0ade56f, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "wwf_image_rom_l1.u122",	0x100000, 0x2800c78d, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "wwf_image_rom_l1.u121",	0x100000, 0xa28ffcba, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "wwf_image_rom_l1.u120",	0x100000, 0x3a05d371, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "wwf_image_rom_l1.u119",	0x100000, 0x97ffa659, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "wwf_image_rom_l1.u118",	0x100000, 0x46668e97, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21
};

STD_ROM_PICK(wwfmania)
STD_ROM_FN(wwfmania)

struct BurnDriver BurnDrvWwfmania = {
	"wwfmania", NULL, NULL, NULL, "1995",
	"WWF: Wrestlemania (rev 1.30 08/10/95)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wwfmaniaRomInfo, wwfmaniaRomName, NULL, NULL, WwfmaniaInputInfo, WwfmaniaDIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


// WWF: Wrestlemania (rev 1.20 08/02/95)

static struct BurnRomInfo wwfmaniabRomDesc[] = {
	{ "wwf_game_rom_l1.20.u54",	0x080000, 0x1b2dce48, 1 | BRF_PRG | BRF_ESS }, //  0 TMS34010
	{ "wwf_game_rom_l1.20.u63",	0x080000, 0x1262f0bb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "wwf_music-spch_l1.u2",	0x100000, 0xa9acb250, 2 | BRF_PRG | BRF_ESS }, //  2 DCS sound banks
	{ "wwf_music-spch_l1.u3",	0x100000, 0x9442b6c9, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "wwf_music-spch_l1.u4",	0x100000, 0xcee78fac, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "wwf_music-spch_l1.u5",	0x100000, 0x5b31fd40, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "wwf_image_rom_l1.u133",	0x100000, 0x5e1b1e3d, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 0) }, //  6 GFX
	{ "wwf_image_rom_l1.u132",	0x100000, 0x5943b3b2, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 1) }, //  7
	{ "wwf_image_rom_l1.u131",	0x100000, 0x0815db22, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 2) }, //  8
	{ "wwf_image_rom_l1.u130",	0x100000, 0x9ee9a145, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x00, 3) }, //  9

	{ "wwf_image_rom_l1.u129",	0x100000, 0xc644c2f4, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 0) }, // 10
	{ "wwf_image_rom_l1.u128",	0x100000, 0xfcda4e9a, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 1) }, // 11
	{ "wwf_image_rom_l1.u127",	0x100000, 0x45be7428, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 2) }, // 12
	{ "wwf_image_rom_l1.u126",	0x100000, 0xeaa276a8, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x04, 3) }, // 13

	{ "wwf_image_rom_l1.u125",	0x100000, 0xa19ebeed, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 0) }, // 14
	{ "wwf_image_rom_l1.u124",	0x100000, 0xdc7d3dbb, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 1) }, // 15
	{ "wwf_image_rom_l1.u123",	0x100000, 0xe0ade56f, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 2) }, // 16
	{ "wwf_image_rom_l1.u122",	0x100000, 0x2800c78d, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x08, 3) }, // 17

	{ "wwf_image_rom_l1.u121",	0x100000, 0xa28ffcba, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 0) }, // 18
	{ "wwf_image_rom_l1.u120",	0x100000, 0x3a05d371, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 1) }, // 19
	{ "wwf_image_rom_l1.u119",	0x100000, 0x97ffa659, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 2) }, // 20
	{ "wwf_image_rom_l1.u118",	0x100000, 0x46668e97, 3 | BRF_GRA | BRF_ESS | WUNIT_GFX(0x0c, 3) }, // 21
};

STD_ROM_PICK(wwfmaniab)
STD_ROM_FN(wwfmaniab)

struct BurnDriver BurnDrvWwfmaniab = {
	"wwfmaniab", "wwfmania", NULL, NULL, "1995",
	"WWF: Wrestlemania (rev 1.20 08/02/95)\0", NULL, "Midway", "MIDWAY Wolf-Unit",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_16BIT_ONLY, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, wwfmaniabRomInfo, wwfmaniabRomName, NULL, NULL, WwfmaniaInputInfo, WwfmaniaDIPInfo,
    WolfUnitInit, WolfUnitExit, WolfUnitFrame, WolfUnitDraw, WolfUnitScan, &nWolfUnitRecalc, 0x8000,
    512, 254, 4, 3
};


