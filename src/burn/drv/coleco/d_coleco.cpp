// FB Neo ColecoVision console driver module
// Code by iq_132, fixups & bring up-to-date by dink Aug 19, 2014
// SGM added April 2019 -dink

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
#include "sn76496.h"
#include "ay8910.h" // sgm
#include "tms9928a.h"
#include "burn_gun.h" // trackball (Roller & Super Action controllers)
#include "i2ceeprom.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80BIOS;
static UINT8 *DrvCartROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSGM24kRAM;
static UINT8 *DrvSGM8kRAM;
static UINT8 *DrvEEPROM;

static INT32 joy_mode;
static INT32 joy_status[2];
static INT32 last_state;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvDips[2] = { 0, 0 };
static UINT16 DrvInputs[4];
static UINT8 DrvReset;

static INT16 Analog0;
static INT16 Analog1;
static UINT8 spinner[2] = { 0, 0 };

static UINT32 MegaCart; // MegaCart size
static UINT32 MegaCartBank; // current Bank
static UINT32 MegaCartBanks; // total banks
static INT32 OCMBanks[4];

// for use_I2C (i2c 2-wire 24c02, +)
static INT32 d_sda;
static INT32 d_scl;

static INT32 use_I2C = 0; // i2c eeprom
static INT32 use_SGM = 0;
static INT32 use_SAC = 0; // 1 = SuperAction, 2 = ROLLER
static INT32 use_OCM = 0;
static INT32 SGM_map_24k;
static INT32 SGM_map_8k;

static UINT8 dip_changed;

static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

static struct BurnInputInfo ColecoInputList[] = {
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Fire 1",	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 1"	},
	{"P1 Fire 2",	BIT_DIGITAL,	DrvJoy1 + 14,	"p1 fire 2"	},
	{"P1 0",		BIT_DIGITAL,	DrvJoy1 +  0,	"p1 0"		},
	{"P1 1",		BIT_DIGITAL,	DrvJoy1 +  1,	"p1 1"		},
	{"P1 2",		BIT_DIGITAL,	DrvJoy1 +  2,	"p1 2"		},
	{"P1 3",		BIT_DIGITAL,	DrvJoy1 +  3,	"p1 3"		},
	{"P1 4",		BIT_DIGITAL,	DrvJoy1 +  4,	"p1 4"		},
	{"P1 5",		BIT_DIGITAL,	DrvJoy1 +  5,	"p1 5"		},
	{"P1 6",		BIT_DIGITAL,	DrvJoy1 +  6,	"p1 6"		},
	{"P1 7",		BIT_DIGITAL,	DrvJoy1 +  7,	"p1 7"		},
	{"P1 8",		BIT_DIGITAL,	DrvJoy1 +  8,	"p1 8"		},
	{"P1 9",		BIT_DIGITAL,	DrvJoy1 +  9,	"p1 9"		},
	{"P1 #",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 #"		},
	{"P1 *",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 *"		},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"	},
	{"P2 Fire 1",	BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 1"	},
	{"P2 Fire 2",	BIT_DIGITAL,	DrvJoy3 + 14,	"p2 fire 2"	},
	{"P2 0",		BIT_DIGITAL,	DrvJoy3 +  0,	"p2 0"		},
	{"P2 1",		BIT_DIGITAL,	DrvJoy3 +  1,	"p2 1"		},
	{"P2 2",		BIT_DIGITAL,	DrvJoy3 +  2,	"p2 2"		},
	{"P2 3",		BIT_DIGITAL,	DrvJoy3 +  3,	"p2 3"		},
	{"P2 4",		BIT_DIGITAL,	DrvJoy3 +  4,	"p2 4"		},
	{"P2 5",		BIT_DIGITAL,	DrvJoy3 +  5,	"p2 5"		},
	{"P2 6",		BIT_DIGITAL,	DrvJoy3 +  6,	"p2 6"		},
	{"P2 7",		BIT_DIGITAL,	DrvJoy3 +  7,	"p2 7"		},
	{"P2 8",		BIT_DIGITAL,	DrvJoy3 +  8,	"p2 8"		},
	{"P2 9",		BIT_DIGITAL,	DrvJoy3 +  9,	"p2 9"		},
	{"P2 #",		BIT_DIGITAL,	DrvJoy3 + 10,	"p2 #"		},
	{"P2 *",		BIT_DIGITAL,	DrvJoy3 + 11,	"p2 *"		},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Bios Select",	BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Coleco)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo SACInputList[] = {
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Fire 1",	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 1"	},
	{"P1 Fire 2",	BIT_DIGITAL,	DrvJoy1 + 14,	"p1 fire 2"	},
	{"P1 Fire 3",	BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 3"	},
	{"P1 Fire 4",	BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 4"	},
	A("P1 Spinner", BIT_ANALOG_REL, &Analog0,		"p1 x-axis"),
	{"P1 0",		BIT_DIGITAL,	DrvJoy1 +  0,	"p1 0"		},
	{"P1 1",		BIT_DIGITAL,	DrvJoy1 +  1,	"p1 1"		},
	{"P1 2",		BIT_DIGITAL,	DrvJoy1 +  2,	"p1 2"		},
	{"P1 3",		BIT_DIGITAL,	DrvJoy1 +  3,	"p1 3"		},
	{"P1 4",		BIT_DIGITAL,	DrvJoy1 +  4,	"p1 4"		},
	{"P1 5",		BIT_DIGITAL,	DrvJoy1 +  5,	"p1 5"		},
	{"P1 6",		BIT_DIGITAL,	DrvJoy1 +  6,	"p1 6"		},
	{"P1 7",		BIT_DIGITAL,	DrvJoy1 +  7,	"p1 7"		},
	{"P1 8",		BIT_DIGITAL,	DrvJoy1 +  8,	"p1 8"		},
	{"P1 9",		BIT_DIGITAL,	DrvJoy1 +  9,	"p1 9"		},
	{"P1 #",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 #"		},
	{"P1 *",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 *"		},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"	},
	{"P2 Fire 1",	BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 1"	},
	{"P2 Fire 2",	BIT_DIGITAL,	DrvJoy3 + 14,	"p2 fire 2"	},
	{"P2 Fire 3",	BIT_DIGITAL,	DrvJoy3 + 12,	"p2 fire 3"	},
	{"P2 Fire 4",	BIT_DIGITAL,	DrvJoy3 + 13,	"p2 fire 4"	},
	A("P2 Spinner", BIT_ANALOG_REL, &Analog1,		"p2 x-axis"),
	{"P2 0",		BIT_DIGITAL,	DrvJoy3 +  0,	"p2 0"		},
	{"P2 1",		BIT_DIGITAL,	DrvJoy3 +  1,	"p2 1"		},
	{"P2 2",		BIT_DIGITAL,	DrvJoy3 +  2,	"p2 2"		},
	{"P2 3",		BIT_DIGITAL,	DrvJoy3 +  3,	"p2 3"		},
	{"P2 4",		BIT_DIGITAL,	DrvJoy3 +  4,	"p2 4"		},
	{"P2 5",		BIT_DIGITAL,	DrvJoy3 +  5,	"p2 5"		},
	{"P2 6",		BIT_DIGITAL,	DrvJoy3 +  6,	"p2 6"		},
	{"P2 7",		BIT_DIGITAL,	DrvJoy3 +  7,	"p2 7"		},
	{"P2 8",		BIT_DIGITAL,	DrvJoy3 +  8,	"p2 8"		},
	{"P2 9",		BIT_DIGITAL,	DrvJoy3 +  9,	"p2 9"		},
	{"P2 #",		BIT_DIGITAL,	DrvJoy3 + 10,	"p2 #"		},
	{"P2 *",		BIT_DIGITAL,	DrvJoy3 + 11,	"p2 *"		},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Bios Select",	BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(SAC)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo ROLLERInputList[] = {
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Fire 1",	BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 1"	},
	{"P1 Fire 2",	BIT_DIGITAL,	DrvJoy1 + 14,	"p1 fire 2"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog0,	"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog1,	"p1 y-axis"),
	{"P1 0",		BIT_DIGITAL,	DrvJoy1 +  0,	"p1 0"		},
	{"P1 1",		BIT_DIGITAL,	DrvJoy1 +  1,	"p1 1"		},
	{"P1 2",		BIT_DIGITAL,	DrvJoy1 +  2,	"p1 2"		},
	{"P1 3",		BIT_DIGITAL,	DrvJoy1 +  3,	"p1 3"		},
	{"P1 4",		BIT_DIGITAL,	DrvJoy1 +  4,	"p1 4"		},
	{"P1 5",		BIT_DIGITAL,	DrvJoy1 +  5,	"p1 5"		},
	{"P1 6",		BIT_DIGITAL,	DrvJoy1 +  6,	"p1 6"		},
	{"P1 7",		BIT_DIGITAL,	DrvJoy1 +  7,	"p1 7"		},
	{"P1 8",		BIT_DIGITAL,	DrvJoy1 +  8,	"p1 8"		},
	{"P1 9",		BIT_DIGITAL,	DrvJoy1 +  9,	"p1 9"		},
	{"P1 #",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 #"		},
	{"P1 *",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 *"		},

	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"	},
	{"P2 Fire 1",	BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 1"	},
	{"P2 Fire 2",	BIT_DIGITAL,	DrvJoy3 + 14,	"p2 fire 2"	},
	{"P2 0",		BIT_DIGITAL,	DrvJoy3 +  0,	"p2 0"		},
	{"P2 1",		BIT_DIGITAL,	DrvJoy3 +  1,	"p2 1"		},
	{"P2 2",		BIT_DIGITAL,	DrvJoy3 +  2,	"p2 2"		},
	{"P2 3",		BIT_DIGITAL,	DrvJoy3 +  3,	"p2 3"		},
	{"P2 4",		BIT_DIGITAL,	DrvJoy3 +  4,	"p2 4"		},
	{"P2 5",		BIT_DIGITAL,	DrvJoy3 +  5,	"p2 5"		},
	{"P2 6",		BIT_DIGITAL,	DrvJoy3 +  6,	"p2 6"		},
	{"P2 7",		BIT_DIGITAL,	DrvJoy3 +  7,	"p2 7"		},
	{"P2 8",		BIT_DIGITAL,	DrvJoy3 +  8,	"p2 8"		},
	{"P2 9",		BIT_DIGITAL,	DrvJoy3 +  9,	"p2 9"		},
	{"P2 #",		BIT_DIGITAL,	DrvJoy3 + 10,	"p2 #"		},
	{"P2 *",		BIT_DIGITAL,	DrvJoy3 + 11,	"p2 *"		},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Bios Select",	BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(ROLLER)

static struct BurnDIPInfo ColecoDIPList[]=
{
	DIP_OFFSET(0x25)
	{0x00, 0xff, 0xff, 0x00, NULL							},

	{0   , 0xfe, 0   ,    4, "Bios Select"					},
	{0x00, 0x01, 0x03, 0x00, "Standard"						},
	{0x00, 0x01, 0x03, 0x01, "Thick Characters"				},
	{0x00, 0x01, 0x03, 0x02, "SVI-603 Coleco Game Adapter"	},
	{0x00, 0x01, 0x03, 0x03, "Chuang Zao Zhe 50"            },

	{0   , 0xfe, 0,       2, "Bypass bios intro (hack)"		},
	{0x00, 0x01, 0x10, 0x00, "Off"							},
	{0x00, 0x01, 0x10, 0x10, "On"							},

	{0   , 0xfe, 0,       2, "Sprite Limit"					},
	{0x00, 0x01, 0x20, 0x00, "Enabled"						},
	{0x00, 0x01, 0x20, 0x20, "Disabled (hack)"				},
};

STDDIPINFO(Coleco)

static struct BurnDIPInfo SACDIPList[]=
{
	DIP_OFFSET(0x2b)
	{0x00, 0xff, 0xff, 0x00, NULL							},

	{0   , 0xfe, 0   ,    4, "Bios Select"					},
	{0x00, 0x01, 0x03, 0x00, "Standard"						},
	{0x00, 0x01, 0x03, 0x01, "Thick Characters"				},
	{0x00, 0x01, 0x03, 0x02, "SVI-603 Coleco Game Adapter"	},
	{0x00, 0x01, 0x03, 0x03, "Chuang Zao Zhe 50"            },

	{0   , 0xfe, 0,       2, "Bypass bios intro (hack)"		},
	{0x00, 0x01, 0x10, 0x00, "Off"							},
	{0x00, 0x01, 0x10, 0x10, "On"							},

	{0   , 0xfe, 0,       2, "Sprite Limit"					},
	{0x00, 0x01, 0x20, 0x00, "Enabled"						},
	{0x00, 0x01, 0x20, 0x20, "Disabled (hack)"				},

	{0   , 0xfe, 0,       2, "Spinner Sensitivity"			},
	{0x00, 0x01, 0x40, 0x40, "Low (Mouse)"					},
	{0x00, 0x01, 0x40, 0x00, "High (Joystick, Mouse Wheel)"	},
};

STDDIPINFO(SAC)

static struct BurnDIPInfo ROLLERDIPList[]=
{
	DIP_OFFSET(0x27)
	{0x00, 0xff, 0xff, 0x00, NULL							},

	{0   , 0xfe, 0   ,    4, "Bios Select"					},
	{0x00, 0x01, 0x03, 0x00, "Standard"						},
	{0x00, 0x01, 0x03, 0x01, "Thick Characters"				},
	{0x00, 0x01, 0x03, 0x02, "SVI-603 Coleco Game Adapter"	},
	{0x00, 0x01, 0x03, 0x03, "Chuang Zao Zhe 50"            },

	{0   , 0xfe, 0,       2, "Bypass bios intro (hack)"		},
	{0x00, 0x01, 0x10, 0x00, "Off"							},
	{0x00, 0x01, 0x10, 0x10, "On"							},

	{0   , 0xfe, 0,       2, "Sprite Limit"					},
	{0x00, 0x01, 0x20, 0x00, "Enabled"						},
	{0x00, 0x01, 0x20, 0x20, "Disabled (hack)"				},

	{0   , 0xfe, 0,       2, "Roller Sensitivity"			},
	{0x00, 0x01, 0x40, 0x40, "Low (Mouse)"					},
	{0x00, 0x01, 0x40, 0x00, "High (Joystick, Mouse Wheel)"	},
};

STDDIPINFO(ROLLER)



void update_map()
{
    if (!use_SGM) return;

	if (use_OCM) {
		ZetMapMemory(DrvCartROM + OCMBanks[3] * 0x2000, 0x8000, 0x9fff, MAP_ROM);
		ZetMapMemory(DrvCartROM + OCMBanks[0] * 0x2000, 0xa000, 0xbfff, MAP_ROM);
		ZetMapMemory(DrvCartROM + OCMBanks[1] * 0x2000, 0xc000, 0xdfff, MAP_ROM);
		// bank 2 / e000-ffff is mapped to rom or eeprom - see read handler!
		//ZetMapMemory(DrvCartROM + OCMBanks[2] * 0x2000, 0xe000, 0xffff, MAP_ROM);
	}

    if (SGM_map_24k) {
		ZetMapMemory(DrvSGM24kRAM, 0x2000, 0x7fff, MAP_RAM);
    } else {
        ZetUnmapMemory(0x2000, 0x7fff, MAP_RAM);
        // normal CV 1k ram +mirrored
        for (INT32 i = 0x6000; i < 0x8000; i+=0x0400) {
            ZetMapMemory(DrvZ80RAM, i + 0x0000, i + 0x03ff, MAP_RAM);
        }
    }

    if (SGM_map_8k) {
        ZetMapMemory(DrvSGM8kRAM, 0x0000, 0x1fff, MAP_RAM);
    } else {
        ZetUnmapMemory(0x0000, 0x1fff, MAP_RAM);
        ZetMapMemory(DrvZ80BIOS, 0x0000, 0x1fff, MAP_ROM);
    }
}

static UINT8 controller_read(INT32 port)
{
	//                        0     1     2     3     4     5     6     7     8     9     *     #   PUR   BLU  BUTR
	const UINT8 keys[] = { 0x05, 0x02, 0x08, 0x03, 0x0d, 0x0c, 0x01, 0x0a, 0x0e, 0x04, 0x09, 0x06, 0x07, 0x0b, 0x40 };
	UINT8 data = 0;

	if (joy_mode == 0)
	{ // keypad mode
		UINT16 input = DrvInputs[(2 * port) + 0];

		for (INT32 i = 0; i < 0x0f; i++) {
			if (~input & (1 << i))
				data |= keys[i];
		}

		data = ~(data | 0x80);
	}
	else
	{ // joypad mode
		data = (DrvInputs[(2 * port) + 1] & 0x7f) /*| 0x80*/; // Telebunny gets hung after bios if bit7 high.
		data = (data & ~0x30) | (~spinner[port] & 0x30);
	}

	return data;
}

static void __fastcall coleco_write_port(UINT16 port, UINT8 data)
{
#if 0
	// debug: ignore video/joy/ay8910 writes
	if ((port&0xfe) != 0xbe && (port&0xfe) != 0x80 &&
	    (port&0xfe) != 0x50 && (port&0xfe) != 0xc0) bprintf(0, _T("wp  %x  %x\n"), port, data);
#endif
	if (use_SGM) {
        switch (port & 0xff) // SGM
        {
            case 0x50:
                AY8910Write(0, 0, data);
            return;

            case 0x51:
                AY8910Write(0, 1, data);
            return;

			case 0x53:
				SGM_map_24k = data & 1;
                update_map();
            return;

            case 0x7f:
                SGM_map_8k = ~data & 2;
                update_map();
            return;
        }
    }

    switch (port & ~0xff1e)
	{
		case 0x80:
		case 0x81:
			joy_mode = 0;
		return;

		case 0xa0:
			TMS9928AWriteVRAM(data);
		return;

		case 0xa1:
			TMS9928AWriteRegs(data);
		return;

		case 0xc0:
		case 0xc1:
			joy_mode = 1;
		return;

		case 0xe0:
		case 0xe1:
			// SN76496 takes 32 cycles to become ready after CS
			// SN76496's READY line connected to z80's WAIT line on CV
			// Needs +22 to get it to sound exactly like recorded CV @
			// https://www.youtube.com/watch?v=Dg65_jCThKs
			// so, where's the other 22 come from?  other wait-states
			// or propogation delays?
			ZetIdle(32+22);
			SN76496Write(0, data);
		return;
	}

	bprintf(0, _T("unmapped port? %x   %x\n"), port, data);
}

static UINT8 __fastcall coleco_read_port(UINT16 port)
{
	port &= 0xff;

    if (use_SGM) {
        switch (port)
        {
            case 0x52: return AY8910Read(0);
        }
    }

	switch (port & ~0x1e)
	{
		case 0xa0:
			return TMS9928AReadVRAM();

		case 0xa1:
			return TMS9928AReadRegs();
	}

	switch (port & ~0x1d)
	{
		case 0xe0:
		case 0xe1:
			return controller_read(0);

		case 0xe2:
		case 0xe3:
			return controller_read(1);
	}

	bprintf(0, _T("unmapped port read: %x\n"), port);

	return 0xff;
}

static INT32 scanline;
static INT32 lets_nmi = -1;

static void coleco_vdp_interrupt(INT32 state)
{
	if (state && !last_state)
	{
		// delay nmi by 1 insn, prevents a race condition in Super Pac-Man which causes it to crash on boot.
		ZetRunEnd();
		lets_nmi = (scanline) % 262;
	}
	last_state = state;
}

static void CVFastLoadHack() {
    if (DrvDips[1] & 0x10) {
        DrvZ80BIOS[0x13f1] = 0x00;
        DrvZ80BIOS[0x13f2] = 0x00;
        DrvZ80BIOS[0x13f3] = 0x00;
    }
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	BurnLoadRom(DrvZ80BIOS, 0x80 + (DrvDips[1] & 3), 1);
	CVFastLoadHack();

    AY8910Reset(0);

	ZetOpen(0);
	ZetReset();
	ZetSetVector(0xff);
	ZetClose();

	TMS9928AReset();

	if (use_I2C) {
		i2c_reset();
		d_sda = 1;
		d_scl = 1;
	}

	memset (DrvZ80RAM, 0xff, 0x400); // ram initialized to 0xff
	if (!strncmp(BurnDrvGetTextA(DRV_NAME), "cv_heist", 8)) {
		bprintf(0, _T("*** The Heist kludge..\n"));
		// The Heist has a bug which expects certain memory location(s) to
		// be zero'd on boot - if not - the game crashes and returns to the
		// title screen (or just crashes).  This will often happen on CV consoles
		// with a certain manufacturer of ram chips.  Or on most consoles if
		// another game had been on shortly before changing the cart.
		memset (DrvZ80RAM, 0x00, 0x400);
	}

	last_state = 0; // irq state...
	MegaCartBank = 0;
    SGM_map_24k = 0;
    SGM_map_8k = 0;

	dip_changed = DrvDips[1];

	scanline = 0;
	lets_nmi = -1;

	if (use_OCM) {
		// Penguin Adventure expects this to be mapped-in by default
		SGM_map_24k = 1;
	}

	ZetOpen(0);
	update_map();
	ZetClose();

	return 0;
}

static const UINT8 O_EEPROM_OK = 0xff;
static INT32 O_EEPROM_CmdPos = 0;
static INT32 O_EEPROM_State = 0;
static INT32 O_EEPROM_ReadTimer = 0;

enum {
	EEP_NONE = 0 << 1,
	EEP_INIT = 1 << 1,
	EEP_STATUS = 2 << 1,
	EEP_WRITE = 3 << 1,
	EEP_STATUS_OK = 0xff
};

static void O_EepScan()
{
	SCAN_VAR(O_EEPROM_CmdPos);
	SCAN_VAR(O_EEPROM_State);
	SCAN_VAR(O_EEPROM_ReadTimer);
}

static void __fastcall main_write(UINT16 address, UINT8 data)
{
	// maybe we should support bankswitching on writes too?
	//bprintf(0, _T("%x  %x\t\tfr %d  cyc %d\n"), address, data, nCurrentFrame, ZetTotalCycles());
    if (use_I2C) { // for boxxle, black onyx, space shuttle, etc..
        switch (address)
        {
            case 0xff90:
            case 0xffa0:
            case 0xffb0:
                MegaCartBank = (address >> 4) & 3;
				return;
			case 0xffc0:
				d_scl = 0;
				i2c_write_bit(d_sda, d_scl);
				return;
            case 0xffd0:
				d_scl = 1;
				i2c_write_bit(d_sda, d_scl);
				return;
            case 0xffe0:
				d_sda = 0;
				i2c_write_bit(d_sda, d_scl);
				return;
            case 0xfff0:
				d_sda = 1;
				i2c_write_bit(d_sda, d_scl);
				return;
        }
	}

	if (use_OCM) {
		if (address >= 0xe000 && address <= 0xfffb) {

			if (data == 0xaa && O_EEPROM_CmdPos == 0) {
				O_EEPROM_CmdPos++;
			}
			else if (data == 0x55 && O_EEPROM_CmdPos == 1) {
				O_EEPROM_CmdPos++;
			}
			else if (O_EEPROM_CmdPos == 2) {
				switch (data) {
					case 0x80: {
						O_EEPROM_State = EEP_INIT;
						break;
					}
					case 0x30: {
						if (O_EEPROM_State == EEP_INIT) {
							O_EEPROM_State = EEP_STATUS;
						}
						break;
					}
					case 0xa0: {
						O_EEPROM_State = EEP_WRITE;
						break;
					}
					default: {
						bprintf(0, _T("--> EEP Unknown command-byte!  %x  @  %x\n"), data, address);
						break;
					}
				}
				O_EEPROM_CmdPos = 0;
			}
			else if (O_EEPROM_State == EEP_WRITE) {
				DrvEEPROM[address & 0x3ff] = data;
				O_EEPROM_State = EEP_NONE;
			}
		}
		switch (address) {
			case 0xfffe:
				O_EEPROM_ReadTimer = ((data & 0xf) == 0xf) ? 3 : 0;
				// fallthrough! (no break)
			case 0xfffc:
			case 0xfffd:
			case 0xffff:
				//bprintf(0, _T("bank %x  %x\t\tfr %d  cyc %d\n"), address, data, nCurrentFrame, ZetTotalCycles());
				OCMBanks[address & 0x03] = data & 0xf;
				update_map();
				return;
		}
	}

//	bprintf(0, _T("mw %x %x\n"), address, data);
}

// OCM eeprom debug stuff left in, because I'm not fully sure on the trigger
// for eeprom read-mode.
//
// *game usually sets bank 2 to 0xf when it goes into read mode.  In a few
// instances, it'll set to 0xf then expect to read ROM instead of EEPROM.
// The only way I can get it to work with -all- known dumps at this point
// (goonies, knightmare, mooncresta, penguin adv, gradius), is to set up
// a timer when 0xf is written to bank 2.  After 3 frames pass, it'll revert
// back to ROM-read mode on e000-ffff.
//                                                     - dink, july 12, 2024

static UINT8 __fastcall main_read(UINT16 address)
{
	if (use_OCM && address >= 0xe000 && address <= 0xffff) {
		if ((address & 0x0fff) == 0x0000 && O_EEPROM_State == EEP_STATUS) {
			//bprintf(0, _T("eeprom_ok\n"));
			return EEP_STATUS_OK;
		}
		if (/*OCMBanks[2] == 0xf*/ O_EEPROM_ReadTimer > 0) {// && (address & 0x1fff) < 0x3ff) {
//			bprintf(0, _T("eeprom_read %x\t\tfr: %d\n"), address, nCurrentFrame);
			return DrvEEPROM[address & 0x3ff];
		} else {
//			bprintf(0, _T("rom_read %x\t\tfr: %d\n"), address, nCurrentFrame);
			return DrvCartROM[(OCMBanks[2] * 0x2000) + (address & 0x1fff)];
		}
	}

	if (use_I2C && address == 0xff80) {
		return DrvCartROM[i2c_read() ? 0xff80 : 0xbf80];
	}

	if (address >= 0xffc0/* && address <= 0xffff*/) {
		MegaCartBank = (0xffff - address) & (MegaCartBanks - 1);

		MegaCartBank = (MegaCartBanks - MegaCartBank) - 1;

		return 0;
	}

	if (address >= 0xc000 && address <= 0xffbf)
		return DrvCartROM[(MegaCartBank * 0x4000) + (address - 0xc000)];

	//bprintf(0, _T("mr %X,"), address);
	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80BIOS		= Next; Next += 0x004000;
	DrvCartROM		= Next; Next += 0x100000;

	DrvEEPROM		= Next; Next += 0x000400; // 1k for OCM

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000400;

    DrvSGM24kRAM    = Next; Next += 0x006000; // SGM
    DrvSGM8kRAM		= Next; Next += 0x002000;

    RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	// refresh rate 59.92hz

	BurnAllocMemIndex();

	MegaCart = 0;

	{
		char* pRomName;
		struct BurnRomInfo ri;

		if (BurnLoadRom(DrvZ80BIOS, 0x80, 1)) return 1;

		for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
			BurnDrvGetRomInfo(&ri, i);

			if ((ri.nType & BRF_PRG) && (ri.nLen == 0x2000 || ri.nLen == 0x1000) && (i<10)) {
				BurnLoadRom(DrvCartROM+(i * 0x2000), i, 1);
				bprintf(0, _T("ColecoVision romload #%d\n"), i);
			} else if ((ri.nType & BRF_PRG) && (i<10)) { // Load rom thats not in 0x2000 (8k) chunks
				bprintf(0, _T("ColecoVision romload (unsegmented) #%d size: %X\n"), i, ri.nLen);
				BurnLoadRom(DrvCartROM, i, 1);
				if (ri.nLen >= 0x10000) MegaCart = ri.nLen;
			}
		}
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80BIOS, 0x0000, 0x1fff, MAP_ROM);

	for (INT32 i = 0x6000; i < 0x8000; i+=0x0400) {
		ZetMapMemory(DrvZ80RAM, i + 0x0000, i + 0x03ff, MAP_RAM);
	}

    if (use_I2C) {  // similar to MegaCart but with diff. mapper addresses
		// Boxxle
		MegaCartBanks = MegaCart / 0x4000;
		bprintf(0, _T("ColecoVision BoxxleCart mapping.\n"));
		i2c_init((use_I2C == 1) ? I2C_24C02 : I2C_24C256);
		ZetMapMemory(DrvCartROM, 0x8000, 0xbfff, MAP_ROM);
		ZetSetReadHandler(main_read);
        ZetSetWriteHandler(main_write);
	} else if (use_OCM) {
		MegaCart = 0;
		bprintf(0, _T("ColecoVision OCM mapper w/EEPROM.\n"));
		ZetSetReadHandler(main_read);
		ZetSetWriteHandler(main_write);
		OCMBanks[0] = 3;
		OCMBanks[1] = 2;
		OCMBanks[2] = 1;
		OCMBanks[3] = 0;
		update_map();
	}
    else if (MegaCart) {
		// MegaCart
		MegaCartBanks = MegaCart / 0x4000;
		UINT32 lastbank = (MegaCartBanks - 1) * 0x4000;
		bprintf(0, _T("ColecoVision MegaCart: mapping cartrom[%X] to 0x8000 - 0xbfff.\n"), lastbank);
		ZetMapMemory(DrvCartROM + lastbank, 0x8000, 0xbfff, MAP_ROM);
		ZetSetReadHandler(main_read);
		update_map();
	} else {
		// Regular CV Cart
		ZetMapMemory(DrvCartROM, 0x8000, 0xffff, MAP_ROM);
	}

	ZetSetOutHandler(coleco_write_port);
	ZetSetInHandler(coleco_read_port);
	ZetClose();

	TMS9928AInit(TMS99x8A, 0x4000, 0, 0, coleco_vdp_interrupt);
	TMS9928ASetSpriteslimit((DrvDips[1] & 0x20) ? 0 : 1);
	bprintf(0, _T("Sprite Limit: %S\n"), (DrvDips[1] & 0x20) ? "Disabled" : "Enabled");

	SN76489AInit(0, 3579545, 0);
    SN76496SetBuffered(ZetTotalCycles, 3579545);

    AY8910Init(0, 3579545 / 2, 1); // SGM
	AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
    AY8910SetBuffered(ZetTotalCycles, 3579545);

	BurnTrackballInit(2);
	BurnTrackballSetVelocityCurve(1);

	if (use_OCM) {
		memset(DrvEEPROM, 0xff, 0x10);
	}

	DrvDoReset();

	return 0;
}

static INT32 DrvInitSAC()
{
	use_SAC = 1;
    use_SGM = 1;

	return DrvInit();
}

static INT32 DrvInitROLLER()
{
	use_SAC = 2;
    use_SGM = 1; // kabooom!

	return DrvInit();
}

static INT32 DrvInitSGM() // w/SGM
{
    use_SGM = 1;

    return DrvInit();
}

static INT32 DrvInitI2CBoxxle() // w/EEPROM (boxxle)
{
    use_I2C = 2;

    return DrvInit();
}

static INT32 DrvInitI2C() // w/EEPROM (boxxle)
{
    use_I2C = 1;

    return DrvInit();
}

static INT32 DrvInitOCM()
{
	use_OCM = 1;
	use_SGM = 1;

    return DrvInit();
}

static INT32 DrvExit()
{
	TMS9928AExit();
	ZetExit();
	SN76496Exit();
	AY8910Exit(0);

	BurnFreeMemIndex();

	BurnTrackballExit();

	if (use_I2C) {
		i2c_exit();
	}

    use_SGM = 0;
    use_I2C = 0;
	use_SAC = 0;
	use_OCM = 0;

	return 0;
}

static INT32 SAC_vel[2] = { 0, 0 };

static void update_SAC(INT32 start)
{
	if (use_SAC) {
		BurnDialINF dial;
		BurnTrackballUpdate(0);

		for (INT32 pl = 0; pl < 2; pl++)
		{
			spinner[pl] = 0x00;

			if (start == 0 && SAC_vel[pl]-- <= 0) continue;
			if (start) SAC_vel[pl] = 0;

			BurnPaddleGetDial(dial, 0, pl);

			if (dial.Forward) {
			    if (start) SAC_vel[pl] += dial.Velocity;
				spinner[pl] = 0x30;
				ZetSetIRQLine(0, 0, CPU_IRQSTATUS_HOLD);
			}
			if (dial.Backward) {
				if (start) SAC_vel[pl] += dial.Velocity;
				spinner[pl] = 0x10;
				ZetSetIRQLine(0, 0, CPU_IRQSTATUS_HOLD);
			}

			if (start) {
				SAC_vel[pl] /= ((DrvDips[0] & 0x40) ? 0x24 : 0x0b); // (sensitivity) mouse: 0x1b, joy/mousewheel: 0xb
			}
		}
#if 0
		// debug :)
		if (start) {
			bprintf(0, _T("velocity %x\t%x\n"), SAC_vel[0], SAC_vel[1]);
		}
#endif
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	if (O_EEPROM_ReadTimer > 0) O_EEPROM_ReadTimer--; // eeprom read timer, good for 2-3 frames(?)

	{
		memset (DrvInputs, 0xff, 4 * sizeof(UINT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if ((dip_changed ^ DrvDips[1]) & 0x20) {
			TMS9928ASetSpriteslimit((DrvDips[1] & 0x20) ? 0 : 1);
			dip_changed = DrvDips[1];
		}

		if (use_SAC) {
			SAC_vel[0] = SAC_vel[1] = 0;
			BurnTrackballConfig(0, AXIS_NORMAL, (use_SAC == 2) ? AXIS_REVERSED : AXIS_NORMAL);
			BurnTrackballFrame(0, Analog0, Analog1, 0x02, 0x17);
			update_SAC(1);
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { (INT32)(3579545 / 59.92) };
	INT32 nCyclesDone[1]  = { 0 };

    ZetNewFrame();
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;
		CPU_RUN_SYNCINT(0, Zet);

		TMS9928AScanline(i);

		if (lets_nmi == i) {
			ZetNmi();
			lets_nmi = -1;
		}

		if (use_SAC && (i & 0x1f) == 0x1f && (SAC_vel[0] || SAC_vel[1]) ) {
		    update_SAC(0);
		}
	}

	ZetClose();

	if (pBurnSoundOut) {
        SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();
	}

	if (pBurnDraw) {
		TMS9928ADraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		SN76496Scan(nAction, pnMin);

		if (use_SGM) {
			AY8910Scan(nAction, pnMin);
		}

		if (use_OCM) {
			SCAN_VAR(OCMBanks);
			O_EepScan();
		}

		if (use_SAC) {
			BurnTrackballScan();
			SCAN_VAR(spinner);
		}

		TMS9928AScan(nAction, pnMin);

		SCAN_VAR(joy_mode);
		SCAN_VAR(joy_status);
		SCAN_VAR(last_state);
        SCAN_VAR(MegaCartBank);
        SCAN_VAR(SGM_map_24k);
		SCAN_VAR(SGM_map_8k);
		if (use_I2C) {
			SCAN_VAR(d_scl);
			SCAN_VAR(d_sda);
		}
	}

	if (nAction & ACB_NVRAM && use_OCM) {
		ScanVar(DrvEEPROM, 0x400, "NV RAM");
	}

	if (use_I2C) {
		i2c_scan(nAction, pnMin);
	}

	if (nAction & ACB_WRITE) {
		if (use_SGM) {
			ZetOpen(0);
			update_map();
			ZetClose();
		}
	}

	return 0;
}

INT32 CVGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		if (i == 1 && BurnDrvGetTextA(DRV_BOARDROM)) {
			pszGameName = BurnDrvGetTextA(DRV_BOARDROM);
		} else {
			pszGameName = BurnDrvGetTextA(DRV_PARENT);
		}
	}

	if (pszGameName == NULL || i > 2) {
		*pszName = NULL;
		return 1;
	}

	// remove the "CV_"
	memset(szFilename, 0, MAX_PATH);
	for (UINT32 j = 0; j < (strlen(pszGameName) - 3); j++) {
		szFilename[j] = pszGameName[j + 3];
	}

	*pszName = szFilename;

	return 0;
}

// ColecoVision

static struct BurnRomInfo cv_colecoRomDesc[] = {
    { "coleco.rom",     0x2000, 0x3aa93ef3, BRF_PRG | BRF_BIOS }, // 0x80 - Normal (Coleco, 1982)
    { "colecoa.rom",	0x2000, 0x39bb16fc, BRF_PRG | BRF_BIOS | BRF_OPT }, // 0x81 - Thick Characters (Coleco, 1982)
    { "svi603.rom", 	0x2000, 0x19e91b82, BRF_PRG | BRF_BIOS | BRF_OPT }, // 0x82 - SVI-603 Coleco Game Adapter (Spectravideo, 1983)
    { "czz50.rom",		0x4000, 0x4999abc6, BRF_PRG | BRF_BIOS | BRF_OPT }, // 0x83 - Chuang Zao Zhe 50 (Bit Corporation, 1986)
};

STD_ROM_PICK(cv_coleco)
STD_ROM_FN(cv_coleco)

struct BurnDriver BurnDrvcv_Coleco = {
	"cv_coleco", NULL, NULL, NULL, "1982",
	"ColecoVision System BIOS\0", "BIOS only", "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_COLECO, GBF_BIOS, 0,
	CVGetZipName, cv_colecoRomInfo, cv_colecoRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// End of driver, the following driver info. has been synthesized from hash/coleco.xml of MESS
// Note: when re-converting coleco.xml, make sure to change "&amp;" to "and"!
// Note2: don't forget to add notice to choplifter: "Corrupted sprites. Use (Alt) version!"


// Super Sketch

static struct BurnRomInfo cv_ssketchRomDesc[] = {
	{ "ssketch.bin",	0x02000, 0x8627300a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ssketch, cv_ssketch, cv_coleco)
STD_ROM_FN(cv_ssketch)

struct BurnDriver BurnDrvcv_ssketch = {
	"cv_ssketch", NULL, "cv_coleco", NULL, "1984",
	"Super Sketch\0", NULL, "Personal Peripherals", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_ssketchRomInfo, cv_ssketchRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Campaign '84

static struct BurnRomInfo cv_campaignRomDesc[] = {
	{ "campaign.1",	0x02000, 0xd657ab6b, BRF_PRG | BRF_ESS },
	{ "campaign.2",	0x02000, 0x844aefcf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_campaign, cv_campaign, cv_coleco)
STD_ROM_FN(cv_campaign)

struct BurnDriver BurnDrvcv_campaign = {
	"cv_campaign", NULL, "cv_coleco", NULL, "1983",
	"Campaign '84\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_campaignRomInfo, cv_campaignRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Cosmic Crisis

static struct BurnRomInfo cv_ccrisisRomDesc[] = {
	{ "ccrisis.1",	0x02000, 0xf8084e5a, BRF_PRG | BRF_ESS },
	{ "ccrisis.2",	0x02000, 0x8c841d6a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ccrisis, cv_ccrisis, cv_coleco)
STD_ROM_FN(cv_ccrisis)

struct BurnDriver BurnDrvcv_ccrisis = {
	"cv_ccrisis", NULL, "cv_coleco", NULL, "1983",
	"Cosmic Crisis\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_ccrisisRomInfo, cv_ccrisisRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Dr. Seuss's Fix-Up the Mix-Up Puzzler

static struct BurnRomInfo cv_drseussRomDesc[] = {
	{ "drseuss.1",	0x02000, 0x47cf6908, BRF_PRG | BRF_ESS },
	{ "drseuss.2",	0x02000, 0xb524f389, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_drseuss, cv_drseuss, cv_coleco)
STD_ROM_FN(cv_drseuss)

struct BurnDriver BurnDrvcv_drseuss = {
	"cv_drseuss", NULL, "cv_coleco", NULL, "1984",
	"Dr. Seuss's Fix-Up the Mix-Up Puzzler\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_drseussRomInfo, cv_drseussRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Illusions

static struct BurnRomInfo cv_illusionRomDesc[] = {
	{ "illusion.1",	0x02000, 0x2b694536, BRF_PRG | BRF_ESS },
	{ "illusion.2",	0x02000, 0x95a5dfa6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_illusion, cv_illusion, cv_coleco)
STD_ROM_FN(cv_illusion)

struct BurnDriver BurnDrvcv_illusion = {
	"cv_illusion", NULL, "cv_coleco", NULL, "1984",
	"Illusions\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_illusionRomInfo, cv_illusionRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// James Bond 007

static struct BurnRomInfo cv_jbondRomDesc[] = {
	{ "jbond.1",	0x02000, 0x3e8adbd1, BRF_PRG | BRF_ESS },
	{ "jbond.2",	0x02000, 0xd76746a3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jbond, cv_jbond, cv_coleco)
STD_ROM_FN(cv_jbond)

struct BurnDriver BurnDrvcv_jbond = {
	"cv_jbond", NULL, "cv_coleco", NULL, "1984",
	"James Bond 007\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_jbondRomInfo, cv_jbondRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Logic Levels

static struct BurnRomInfo cv_logiclvlRomDesc[] = {
	{ "logiclvl.1",	0x02000, 0xd54c581b, BRF_PRG | BRF_ESS },
	{ "logiclvl.2",	0x02000, 0x257aa944, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_logiclvl, cv_logiclvl, cv_coleco)
STD_ROM_FN(cv_logiclvl)

struct BurnDriver BurnDrvcv_logiclvl = {
	"cv_logiclvl", NULL, "cv_coleco", NULL, "1984",
	"Logic Levels\0", NULL, "Fisher-Price", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_logiclvlRomInfo, cv_logiclvlRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Linking Logic

static struct BurnRomInfo cv_linklogcRomDesc[] = {
	{ "lnklogic.1",	0x02000, 0x918f12c0, BRF_PRG | BRF_ESS },
	{ "lnklogic.2",	0x02000, 0xd8f49994, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_linklogc, cv_linklogc, cv_coleco)
STD_ROM_FN(cv_linklogc)

struct BurnDriver BurnDrvcv_linklogc = {
	"cv_linklogc", NULL, "cv_coleco", NULL, "1984",
	"Linking Logic\0", NULL, "Fisher-Price", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_linklogcRomInfo, cv_linklogcRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Looping

static struct BurnRomInfo cv_loopingRomDesc[] = {
	{ "looping.1",	0x02000, 0x205a9c61, BRF_PRG | BRF_ESS },
	{ "looping.2",	0x02000, 0x1b5ef49e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_looping, cv_looping, cv_coleco)
STD_ROM_FN(cv_looping)

struct BurnDriver BurnDrvcv_looping = {
	"cv_looping", NULL, "cv_coleco", NULL, "1983",
	"Looping\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_loopingRomInfo, cv_loopingRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Nova Blast

static struct BurnRomInfo cv_novablstRomDesc[] = {
	{ "novablst.1",	0x02000, 0x790433cf, BRF_PRG | BRF_ESS },
	{ "novablst.2",	0x02000, 0x4f4dd2dc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_novablst, cv_novablst, cv_coleco)
STD_ROM_FN(cv_novablst)

struct BurnDriver BurnDrvcv_novablst = {
	"cv_novablst", NULL, "cv_coleco", NULL, "1983",
	"Nova Blast\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_novablstRomInfo, cv_novablstRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// It's Only Rock 'n' Roll

static struct BurnRomInfo cv_onlyrockRomDesc[] = {
	{ "onlyrock.1",	0x02000, 0x93d46b70, BRF_PRG | BRF_ESS },
	{ "onlyrock.2",	0x02000, 0x2bfc5325, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_onlyrock, cv_onlyrock, cv_coleco)
STD_ROM_FN(cv_onlyrock)

struct BurnDriver BurnDrvcv_onlyrock = {
	"cv_onlyrock", NULL, "cv_coleco", NULL, "1984",
	"It's Only Rock 'n' Roll\0", NULL, "K-Tel", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_onlyrockRomInfo, cv_onlyrockRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Space Panic

static struct BurnRomInfo cv_panicRomDesc[] = {
	{ "panic.1",	0x02000, 0xe06fa55b, BRF_PRG | BRF_ESS },
	{ "panic.2",	0x02000, 0x66fcda90, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_panic, cv_panic, cv_coleco)
STD_ROM_FN(cv_panic)

struct BurnDriver BurnDrvcv_panic = {
	"cv_panic", NULL, "cv_coleco", NULL, "1983",
	"Space Panic\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_panicRomInfo, cv_panicRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Pepper II

static struct BurnRomInfo cv_pepper2RomDesc[] = {
	{ "pepper2.1",	0x02000, 0x2ea3deb5, BRF_PRG | BRF_ESS },
	{ "pepper2.2",	0x02000, 0xcd31ba03, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pepper2, cv_pepper2, cv_coleco)
STD_ROM_FN(cv_pepper2)

struct BurnDriver BurnDrvcv_pepper2 = {
	"cv_pepper2", NULL, "cv_coleco", NULL, "1983",
	"Pepper II\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MAZE | GBF_ACTION, 0,
	CVGetZipName, cv_pepper2RomInfo, cv_pepper2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Quest for Quintana Roo

static struct BurnRomInfo cv_quintanaRomDesc[] = {
	{ "quintana.1",	0x02000, 0x4e0c1380, BRF_PRG | BRF_ESS },
	{ "quintana.2",	0x02000, 0xb9a51e9d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_quintana, cv_quintana, cv_coleco)
STD_ROM_FN(cv_quintana)

struct BurnDriver BurnDrvcv_quintana = {
	"cv_quintana", NULL, "cv_coleco", NULL, "1983",
	"Quest for Quintana Roo\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_quintanaRomInfo, cv_quintanaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Quest for Quintana Roo (Alt)

static struct BurnRomInfo cv_quintanaaRomDesc[] = {
	{ "quintana.1",	0x02000, 0x4e0c1380, BRF_PRG | BRF_ESS },
	{ "quintanaa.2",	0x02000, 0x7a5fb32f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_quintanaa, cv_quintanaa, cv_coleco)
STD_ROM_FN(cv_quintanaa)

struct BurnDriver BurnDrvcv_quintanaa = {
	"cv_quintanaa", "cv_quintana", "cv_coleco", NULL, "1983",
	"Quest for Quintana Roo (Alt)\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_quintanaaRomInfo, cv_quintanaaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Rocky: Super Action Boxing

static struct BurnRomInfo cv_rockyRomDesc[] = {
	{ "rocky.1",	0x02000, 0xa51498f5, BRF_PRG | BRF_ESS },
	{ "rocky.2",	0x02000, 0x5a8f2336, BRF_PRG | BRF_ESS },
	{ "rocky.3",	0x02000, 0x56fc8d0a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rocky, cv_rocky, cv_coleco)
STD_ROM_FN(cv_rocky)

struct BurnDriver BurnDrvcv_rocky = {
	"cv_rocky", NULL, "cv_coleco", NULL, "1983",
	"Rocky: Super Action Boxing\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_VSFIGHT, 0,
	CVGetZipName, cv_rockyRomInfo, cv_rockyRomName, NULL, NULL, NULL, NULL, SACInputInfo, SACDIPInfo,
	DrvInitSAC, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Rolloverture

static struct BurnRomInfo cv_rolloverRomDesc[] = {
	{ "rollover.1",	0x02000, 0x668b6bcb, BRF_PRG | BRF_ESS },
	{ "rollover.2",	0x02000, 0xb3dc2195, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rollover, cv_rollover, cv_coleco)
STD_ROM_FN(cv_rollover)

struct BurnDriver BurnDrvcv_rollover = {
	"cv_rollover", NULL, "cv_coleco", NULL, "1983",
	"Rolloverture\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_rolloverRomInfo, cv_rolloverRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sammy Lightfoot

static struct BurnRomInfo cv_sammylfRomDesc[] = {
	{ "sammylf.1",	0x02000, 0x2492bac2, BRF_PRG | BRF_ESS },
	{ "sammylf.2",	0x02000, 0x7fee3b34, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sammylf, cv_sammylf, cv_coleco)
STD_ROM_FN(cv_sammylf)

struct BurnDriver BurnDrvcv_sammylf = {
	"cv_sammylf", NULL, "cv_coleco", NULL, "1983",
	"Sammy Lightfoot\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_sammylfRomInfo, cv_sammylfRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sammy Lightfoot (Alt)

static struct BurnRomInfo cv_sammylfaRomDesc[] = {
	{ "sammylf.1",	0x02000, 0x2492bac2, BRF_PRG | BRF_ESS },
	{ "sammylfa.2",	0x02000, 0x8f7b8944, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sammylfa, cv_sammylfa, cv_coleco)
STD_ROM_FN(cv_sammylfa)

struct BurnDriver BurnDrvcv_sammylfa = {
	"cv_sammylfa", "cv_sammylf", "cv_coleco", NULL, "1983",
	"Sammy Lightfoot (Alt)\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_sammylfaRomInfo, cv_sammylfaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Space Fury

static struct BurnRomInfo cv_spacfuryRomDesc[] = {
	{ "spacfury.1",	0x02000, 0x1850548f, BRF_PRG | BRF_ESS },
	{ "spacfury.2",	0x02000, 0x4d6866e1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spacfury, cv_spacfury, cv_coleco)
STD_ROM_FN(cv_spacfury)

struct BurnDriver BurnDrvcv_spacfury = {
	"cv_spacfury", NULL, "cv_coleco", NULL, "1983",
	"Space Fury\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_spacfuryRomInfo, cv_spacfuryRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Spectron

static struct BurnRomInfo cv_spectronRomDesc[] = {
	{ "spectron.1",	0x02000, 0x23e2afa0, BRF_PRG | BRF_ESS },
	{ "spectron.2",	0x02000, 0x01897fc1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spectron, cv_spectron, cv_coleco)
STD_ROM_FN(cv_spectron)

struct BurnDriver BurnDrvcv_spectron = {
	"cv_spectron", NULL, "cv_coleco", NULL, "1983",
	"Spectron\0", NULL, "Spectravideo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_spectronRomInfo, cv_spectronRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Cross Force

static struct BurnRomInfo cv_sprcrossRomDesc[] = {
	{ "sprcross.1",	0x02000, 0xe5a53b79, BRF_PRG | BRF_ESS },
	{ "sprcross.2",	0x02000, 0x5185bd94, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sprcross, cv_sprcross, cv_coleco)
STD_ROM_FN(cv_sprcross)

struct BurnDriver BurnDrvcv_sprcross = {
	"cv_sprcross", NULL, "cv_coleco", NULL, "1983",
	"Super Cross Force\0", NULL, "Spectravideo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_sprcrossRomInfo, cv_sprcrossRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Cross Force (Alt)

static struct BurnRomInfo cv_sprcrossaRomDesc[] = {
	{ "sprcross.1",	0x02000, 0xe5a53b79, BRF_PRG | BRF_ESS },
	{ "sprcrossa.2",	0x02000, 0xbd58d3e1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sprcrossa, cv_sprcrossa, cv_coleco)
STD_ROM_FN(cv_sprcrossa)

struct BurnDriver BurnDrvcv_sprcrossa = {
	"cv_sprcrossa", "cv_sprcross", "cv_coleco", NULL, "1983",
	"Super Cross Force (Alt)\0", NULL, "Spectravideo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_sprcrossaRomInfo, cv_sprcrossaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Squish 'em Sam!

static struct BurnRomInfo cv_squishemRomDesc[] = {
	{ "squishem.1",	0x02000, 0x2614d406, BRF_PRG | BRF_ESS },
	{ "squishem.2",	0x02000, 0xb1ce0286, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_squishem, cv_squishem, cv_coleco)
STD_ROM_FN(cv_squishem)

struct BurnDriver BurnDrvcv_squishem = {
	"cv_squishem", NULL, "cv_coleco", NULL, "1984",
	"Squish 'em Sam!\0", NULL, "Interphase", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_squishemRomInfo, cv_squishemRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Strike It!

static struct BurnRomInfo cv_strikeitRomDesc[] = {
	{ "strikeit.1",	0x02000, 0x7b26040d, BRF_PRG | BRF_ESS },
	{ "strikeit.2",	0x02000, 0x3a2d6226, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_strikeit, cv_strikeit, cv_coleco)
STD_ROM_FN(cv_strikeit)

struct BurnDriver BurnDrvcv_strikeit = {
	"cv_strikeit", NULL, "cv_coleco", NULL, "1983",
	"Strike It!\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_BREAKOUT, 0,
	CVGetZipName, cv_strikeitRomInfo, cv_strikeitRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Telly Turtle

static struct BurnRomInfo cv_tellyRomDesc[] = {
	{ "telly.1",	0x02000, 0x2d18a9f3, BRF_PRG | BRF_ESS },
	{ "telly.2",	0x02000, 0xc031f478, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_telly, cv_telly, cv_coleco)
STD_ROM_FN(cv_telly)

struct BurnDriver BurnDrvcv_telly = {
	"cv_telly", NULL, "cv_coleco", NULL, "1984",
	"Telly Turtle\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_tellyRomInfo, cv_tellyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Threshold

static struct BurnRomInfo cv_threshldRomDesc[] = {
	{ "threshld.1",	0x02000, 0x5575f9a7, BRF_PRG | BRF_ESS },
	{ "threshld.2",	0x02000, 0x502e5505, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_threshld, cv_threshld, cv_coleco)
STD_ROM_FN(cv_threshld)

struct BurnDriver BurnDrvcv_threshld = {
	"cv_threshld", NULL, "cv_coleco", NULL, "1983",
	"Threshold\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_threshldRomInfo, cv_threshldRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Wizard of Id's Wizmath

static struct BurnRomInfo cv_wizmathRomDesc[] = {
	{ "wizmath.1",	0x02000, 0xc0c6bda0, BRF_PRG | BRF_ESS },
	{ "wizmath.2",	0x02000, 0x4080c0a4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wizmath, cv_wizmath, cv_coleco)
STD_ROM_FN(cv_wizmath)

struct BurnDriver BurnDrvcv_wizmath = {
	"cv_wizmath", NULL, "cv_coleco", NULL, "1984",
	"Wizard of Id's Wizmath\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION | GBF_PUZZLE, 0,
	CVGetZipName, cv_wizmathRomInfo, cv_wizmathRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sector Alpha

static struct BurnRomInfo cv_secalphaRomDesc[] = {
	{ "secalpha.1",	0x02000, 0x9299539b, BRF_PRG | BRF_ESS },
	{ "secalpha.2",	0x02000, 0xc8d6e83d, BRF_PRG | BRF_ESS },
	{ "secalpha.3",	0x01000, 0x354a3b2f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_secalpha, cv_secalpha, cv_coleco)
STD_ROM_FN(cv_secalpha)

struct BurnDriver BurnDrvcv_secalpha = {
	"cv_secalpha", NULL, "cv_coleco", NULL, "1983",
	"Sector Alpha\0", NULL, "Spectravideo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_secalphaRomInfo, cv_secalphaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sewer Sam

static struct BurnRomInfo cv_sewersamRomDesc[] = {
	{ "sewersam.1",	0x02000, 0x7906db21, BRF_PRG | BRF_ESS },
	{ "sewersam.2",	0x02000, 0x9ae6324e, BRF_PRG | BRF_ESS },
	{ "sewersam.3",	0x02000, 0xa17fc15a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sewersam, cv_sewersam, cv_coleco)
STD_ROM_FN(cv_sewersam)

struct BurnDriver BurnDrvcv_sewersam = {
	"cv_sewersam", NULL, "cv_coleco", NULL, "1984",
	"Sewer Sam\0", NULL, "Interphase", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_sewersamRomInfo, cv_sewersamRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Star Trek: Strategic Operations Simulator

static struct BurnRomInfo cv_startrekRomDesc[] = {
	{ "startrek.1",	0x02000, 0x600e431e, BRF_PRG | BRF_ESS },
	{ "startrek.2",	0x02000, 0x1d1741aa, BRF_PRG | BRF_ESS },
	{ "startrek.3",	0x02000, 0x3fa88549, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_startrek, cv_startrek, cv_coleco)
STD_ROM_FN(cv_startrek)

struct BurnDriver BurnDrvcv_startrek = {
	"cv_startrek", NULL, "cv_coleco", NULL, "1984",
	"Star Trek: Strategic Operations Simulator \0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_SIM | GBF_SHOOT, 0,
	CVGetZipName, cv_startrekRomInfo, cv_startrekRomName, NULL, NULL, NULL, NULL, SACInputInfo, SACDIPInfo,
	DrvInitSAC, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Dukes of Hazzard, The

static struct BurnRomInfo cv_hazzardRomDesc[] = {
	{ "hazzard.1",	0x02000, 0x1971d9a2, BRF_PRG | BRF_ESS },
	{ "hazzard.2",	0x02000, 0x9821ea4a, BRF_PRG | BRF_ESS },
	{ "hazzard.3",	0x02000, 0xc3970e2e, BRF_PRG | BRF_ESS },
	{ "hazzard.4",	0x02000, 0x3433251a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_hazzard, cv_hazzard, cv_coleco)
STD_ROM_FN(cv_hazzard)

struct BurnDriver BurnDrvcv_hazzard = {
	"cv_hazzard", NULL, "cv_coleco", NULL, "1984",
	"Dukes of Hazzard, The\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_RACING, 0,
	CVGetZipName, cv_hazzardRomInfo, cv_hazzardRomName, NULL, NULL, NULL, NULL, ROLLERInputInfo, ROLLERDIPInfo,
	DrvInitROLLER, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Fortune Builder

static struct BurnRomInfo cv_fortuneRomDesc[] = {
	{ "fortune.1",	0x02000, 0xb55c5448, BRF_PRG | BRF_ESS },
	{ "fortune.2",	0x02000, 0x8d7deaff, BRF_PRG | BRF_ESS },
	{ "fortune.3",	0x02000, 0x039604ee, BRF_PRG | BRF_ESS },
	{ "fortune.4",	0x02000, 0xdfb4469e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_fortune, cv_fortune, cv_coleco)
STD_ROM_FN(cv_fortune)

struct BurnDriver BurnDrvcv_fortune = {
	"cv_fortune", NULL, "cv_coleco", NULL, "1984",
	"Fortune Builder\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_STRATEGY, 0,
	CVGetZipName, cv_fortuneRomInfo, cv_fortuneRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Escape from the Mindmaster (Prototype)

static struct BurnRomInfo cv_mindmstrRomDesc[] = {
	{ "mindmstr .1",	0x02000, 0xb8280950, BRF_PRG | BRF_ESS },
	{ "mindmstr.2",	0x02000, 0x6cade290, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mindmstr, cv_mindmstr, cv_coleco)
STD_ROM_FN(cv_mindmstr)

struct BurnDriver BurnDrvcv_mindmstr = {
	"cv_mindmstr", NULL, "cv_coleco", NULL, "1983",
	"Escape from the Mindmaster (Prototype)\0", NULL, "Epyx", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_MAZE, 0,
	CVGetZipName, cv_mindmstrRomInfo, cv_mindmstrRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Fall Guy (Prototype)

static struct BurnRomInfo cv_fallguyRomDesc[] = {
	{ "fallguy.1",	0x02000, 0xf77d3ffd, BRF_PRG | BRF_ESS },
	{ "fallguy.2",	0x02000, 0xc85c0f58, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_fallguy, cv_fallguy, cv_coleco)
STD_ROM_FN(cv_fallguy)

struct BurnDriver BurnDrvcv_fallguy = {
	"cv_fallguy", NULL, "cv_coleco", NULL, "1983",
	"Fall Guy (Prototype)\0", NULL, "Fox Video Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_RACING, 0,
	CVGetZipName, cv_fallguyRomInfo, cv_fallguyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Orbit (Prototype)

static struct BurnRomInfo cv_orbitRomDesc[] = {
	{ "orbit.bin",	0x02000, 0x2cc6f4aa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_orbit, cv_orbit, cv_coleco)
STD_ROM_FN(cv_orbit)

struct BurnDriver BurnDrvcv_orbit = {
	"cv_orbit", NULL, "cv_coleco", NULL, "1983",
	"Orbit (Prototype)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_orbitRomInfo, cv_orbitRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Power Lords: Quest for Volcan (Prototype)

static struct BurnRomInfo cv_powerRomDesc[] = {
	{ "power.1",	0x02000, 0x1137001f, BRF_PRG | BRF_ESS },
	{ "power.2",	0x02000, 0x4c872266, BRF_PRG | BRF_ESS },
	{ "power.3",	0x02000, 0x5f20b4e1, BRF_PRG | BRF_ESS },
	{ "power.4",	0x02000, 0x560207f3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_power, cv_power, cv_coleco)
STD_ROM_FN(cv_power)

struct BurnDriver BurnDrvcv_power = {
	"cv_power", NULL, "cv_coleco", NULL, "1984",
	"Power Lords: Quest for Volcan (Prototype)\0", NULL, "Probe 2000", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_powerRomInfo, cv_powerRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Smurf Play and Learn (Prototype)

static struct BurnRomInfo cv_smurfplyRomDesc[] = {
	{ "smurfply.1",	0x02000, 0x1397a474, BRF_PRG | BRF_ESS },
	{ "smurfply.2",	0x02000, 0xa51e1410, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_smurfply, cv_smurfply, cv_coleco)
STD_ROM_FN(cv_smurfply)

struct BurnDriver BurnDrvcv_smurfply = {
	"cv_smurfply", NULL, "cv_coleco", NULL, "1982",
	"Smurf Play and Learn (Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_smurfplyRomInfo, cv_smurfplyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Steamroller (Prototype)

static struct BurnRomInfo cv_steamRomDesc[] = {
	{ "steam.1",	0x02000, 0xdd4f0a05, BRF_PRG | BRF_ESS },
	{ "steam.2",	0x02000, 0x0bf43529, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_steam, cv_steam, cv_coleco)
STD_ROM_FN(cv_steam)

struct BurnDriver BurnDrvcv_steam = {
	"cv_steam", NULL, "cv_coleco", NULL, "1984",
	"Steamroller (Prototype)\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_MAZE | GBF_ACTION, 0,
	CVGetZipName, cv_steamRomInfo, cv_steamRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super DK! (Prototype)

static struct BurnRomInfo cv_superdkRomDesc[] = {
	{ "superdk.1",	0x02000, 0x36253de5, BRF_PRG | BRF_ESS },
	{ "superdk.2",	0x02000, 0x88819fa6, BRF_PRG | BRF_ESS },
	{ "superdk.3",	0x02000, 0x7e3889f6, BRF_PRG | BRF_ESS },
	{ "superdk.4",	0x02000, 0xcd99ed49, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_superdk, cv_superdk, cv_coleco)
STD_ROM_FN(cv_superdk)

struct BurnDriver BurnDrvcv_superdk = {
	"cv_superdk", NULL, "cv_coleco", NULL, "1982",
	"Super DK! (Prototype)\0", NULL, "Nintendo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_superdkRomInfo, cv_superdkRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Sword and Sorcerer (Prototype)

static struct BurnRomInfo cv_swordRomDesc[] = {
	{ "sword-chip1-a176.bin",	0x02000, 0xa1548994, BRF_PRG | BRF_ESS },
	{ "sword-chip2-d5ba.bin",	0x02000, 0x1d03dfae, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sword, cv_sword, cv_coleco)
STD_ROM_FN(cv_sword)

struct BurnDriver BurnDrvcv_sword = {
	"cv_sword", NULL, "cv_coleco", NULL, "1983",
	"Sword and Sorcerer (Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_swordRomInfo, cv_swordRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// CBS Colecovision Monitor Test

static struct BurnRomInfo cv_cbsmonRomDesc[] = {
	{ "cbsmon.1",	0x02000, 0x1cc13594, BRF_PRG | BRF_ESS },
	{ "cbsmon.2",	0x02000, 0x3aa93ef3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cbsmon, cv_cbsmon, cv_coleco)
STD_ROM_FN(cv_cbsmon)

struct BurnDriver BurnDrvcv_cbsmon = {
	"cv_cbsmon", NULL, "cv_coleco", NULL, "1982",
	"CBS Colecovision Monitor Test\0", NULL, "CBS", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_cbsmonRomInfo, cv_cbsmonRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Final Test Cartridge (Prototype)

static struct BurnRomInfo cv_finaltstRomDesc[] = {
	{ "colecovision final test (1982) (coleco).rom",	0x02000, 0x3ae4b58c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_finaltst, cv_finaltst, cv_coleco)
STD_ROM_FN(cv_finaltst)

struct BurnDriver BurnDrvcv_finaltst = {
	"cv_finaltst", NULL, "cv_coleco", NULL, "1982",
	"Final Test Cartridge (Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_finaltstRomInfo, cv_finaltstRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super Controller Test Cartridge

static struct BurnRomInfo cv_suprtestRomDesc[] = {
	{ "super action controller tester (1982) (nuvatec).rom",	0x02000, 0xd206fe58, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_suprtest, cv_suprtest, cv_coleco)
STD_ROM_FN(cv_suprtest)

struct BurnDriver BurnDrvcv_suprtest = {
	"cv_suprtest", NULL, "cv_coleco", NULL, "1983",
	"Super Controller Test Cartridge\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_suprtestRomInfo, cv_suprtestRomName, NULL, NULL, NULL, NULL, SACInputInfo, SACDIPInfo,
	DrvInitSAC, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Music Box (Demo)

static struct BurnRomInfo cv_musicboxRomDesc[] = {
	{ "musicbox.1",	0x02000, 0x4557ed22, BRF_PRG | BRF_ESS },
	{ "musicbox.2",	0x02000, 0x5f40d58b, BRF_PRG | BRF_ESS },
	{ "musicbox.3",	0x02000, 0x0ab26aaa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_musicbox, cv_musicbox, cv_coleco)
STD_ROM_FN(cv_musicbox)

struct BurnDriver BurnDrvcv_musicbox = {
	"cv_musicbox", NULL, "cv_coleco", NULL, "1987",
	"Music Box (Demo)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_DEMO, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_musicboxRomInfo, cv_musicboxRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Dance Fantasy

static struct BurnRomInfo cv_dncfntsyRomDesc[] = {
	{ "dncfntsy.1",	0x02000, 0xbff86a98, BRF_PRG | BRF_ESS },
	{ "dncfntsy.2",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dncfntsy, cv_dncfntsy, cv_coleco)
STD_ROM_FN(cv_dncfntsy)

struct BurnDriver BurnDrvcv_dncfntsy = {
	"cv_dncfntsy", NULL, "cv_coleco", NULL, "1984",
	"Dance Fantasy\0", NULL, "Fisher-Price", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_dncfntsyRomInfo, cv_dncfntsyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Fraction Fever

static struct BurnRomInfo cv_fracfevrRomDesc[] = {
	{ "fracfevr.1",	0x02000, 0x964db3bc, BRF_PRG | BRF_ESS },
	{ "fracfevr.2",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_fracfevr, cv_fracfevr, cv_coleco)
STD_ROM_FN(cv_fracfevr)

struct BurnDriver BurnDrvcv_fracfevr = {
	"cv_fracfevr", NULL, "cv_coleco", NULL, "1983",
	"Fraction Fever\0", NULL, "Spinnaker Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_fracfevrRomInfo, cv_fracfevrRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Memory Manor

static struct BurnRomInfo cv_memmanorRomDesc[] = {
	{ "memmanor.1",	0x02000, 0xbab520ea, BRF_PRG | BRF_ESS },
	{ "memmanor.2",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_memmanor, cv_memmanor, cv_coleco)
STD_ROM_FN(cv_memmanor)

struct BurnDriver BurnDrvcv_memmanor = {
	"cv_memmanor", NULL, "cv_coleco", NULL, "1984",
	"Memory Manor\0", NULL, "Fisher-Price", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_memmanorRomInfo, cv_memmanorRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Smurf Paint 'n' Play Workshop

static struct BurnRomInfo cv_smurfpntRomDesc[] = {
	{ "smurfpnt.1",	0x02000, 0xb5cd2fb4, BRF_PRG | BRF_ESS },
	{ "smurfpnt.2",	0x02000, 0xe4197181, BRF_PRG | BRF_ESS },
	{ "smurfpnt.3",	0x02000, 0xa0e1ab06, BRF_PRG | BRF_ESS },
	{ "smurfpnt.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_smurfpnt, cv_smurfpnt, cv_coleco)
STD_ROM_FN(cv_smurfpnt)

struct BurnDriver BurnDrvcv_smurfpnt = {
	"cv_smurfpnt", NULL, "cv_coleco", NULL, "1983",
	"Smurf Paint 'n' Play Workshop\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_smurfpntRomInfo, cv_smurfpntRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Super DK! Junior (Prototype)

static struct BurnRomInfo cv_suprdkjrRomDesc[] = {
	{ "suprdkjr.1",	0x02000, 0x85ecf7e3, BRF_PRG | BRF_ESS },
	{ "suprdkjr.2",	0x02000, 0x5f63bc3c, BRF_PRG | BRF_ESS },
	{ "suprdkjr.3",	0x02000, 0x6d5676fa, BRF_PRG | BRF_ESS },
	{ "suprdkjr.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_suprdkjr, cv_suprdkjr, cv_coleco)
STD_ROM_FN(cv_suprdkjr)

struct BurnDriver BurnDrvcv_suprdkjr = {
	"cv_suprdkjr", NULL, "cv_coleco", NULL, "1983",
	"Super DK! Junior (Prototype)\0", NULL, "Nintendo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_suprdkjrRomInfo, cv_suprdkjrRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Tunnels and Trolls (Demo)

static struct BurnRomInfo cv_tunnelsRomDesc[] = {
	{ "tunnels.1",	0x02000, 0x36a12145, BRF_PRG | BRF_ESS },
	{ "tunnels.2",	0x02000, 0xb0abdb32, BRF_PRG | BRF_ESS },
	{ "tunnels.3",	0x02000, 0x53cc4957, BRF_PRG | BRF_ESS },
	{ "tunnels.4",	0x02000, 0xb4293435, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tunnels, cv_tunnels, cv_coleco)
STD_ROM_FN(cv_tunnels)

struct BurnDriver BurnDrvcv_tunnels = {
	"cv_tunnels", NULL, "cv_coleco", NULL, "1983",
	"Tunnels and Trolls (Demo)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_DEMO, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_tunnelsRomInfo, cv_tunnelsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Smurfs Save the Day (Prototype)

static struct BurnRomInfo cv_smurfsavRomDesc[] = {
	{ "Smurfs Save the Day (Prototype).col",	0x04000, 0x9a3b8587, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_smurfsav, cv_smurfsav, cv_coleco)
STD_ROM_FN(cv_smurfsav)

struct BurnDriver BurnDrvcv_smurfsav = {
	"cv_smurfsav", "cv_smurfply", "cv_coleco", NULL, "1983",
	"Smurfs Save the Day (Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_smurfsavRomInfo, cv_smurfsavRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// 2010: The Graphic Action Game (USA)
static struct BurnRomInfo cv_2010RomDesc[] = {
	{ "2010 - The Graphic Action Game (USA)(1984)(Coleco).rom",	32768, 0xc575a831, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_2010, cv_2010, cv_coleco)
STD_ROM_FN(cv_2010)

struct BurnDriver BurnDrvcv_2010 = {
	"cv_2010", NULL, "cv_coleco", NULL, "1984",
	"2010: The Graphic Action Game (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION | GBF_PUZZLE, 0,
	CVGetZipName, cv_2010RomInfo, cv_2010RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// 2010: The Graphic Action Game (USA, Prototype)
static struct BurnRomInfo cv_2010pRomDesc[] = {
	{ "2010 - The Graphic Action Game (USA, Proto)(1984)(Coleco).rom",	32768, 0x3657f3cd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_2010p, cv_2010p, cv_coleco)
STD_ROM_FN(cv_2010p)

struct BurnDriver BurnDrvcv_2010p = {
	"cv_2010p", "cv_2010", "cv_coleco", NULL, "1984",
	"2010: The Graphic Action Game (USA, Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_ACTION | GBF_PUZZLE, 0,
	CVGetZipName, cv_2010pRomInfo, cv_2010pRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Alcazar: The Forgotten Fortress (USA)
static struct BurnRomInfo cv_alcazarRomDesc[] = {
	{ "Alcazar - The Forgotten Fortress (USA)(1985)(Activision).rom",	16384, 0xfce3aa06, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_alcazar, cv_alcazar, cv_coleco)
STD_ROM_FN(cv_alcazar)

struct BurnDriver BurnDrvcv_alcazar = {
	"cv_alcazar", NULL, "cv_coleco", NULL, "1985",
	"Alcazar: The Forgotten Fortress (USA)\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION | GBF_ADV, 0,
	CVGetZipName, cv_alcazarRomInfo, cv_alcazarRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Alphabet Zoo (USA)
static struct BurnRomInfo cv_alphazooRomDesc[] = {
	{ "Alphabet Zoo (USA)(1984)(Spinnaker Software).rom",	16384, 0x4ffb4e8c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_alphazoo, cv_alphazoo, cv_coleco)
STD_ROM_FN(cv_alphazoo)

struct BurnDriver BurnDrvcv_alphazoo = {
	"cv_alphazoo", NULL, "cv_coleco", NULL, "1984",
	"Alphabet Zoo (USA)\0", NULL, "Spinnaker Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION | GBF_QUIZ, 0,
	CVGetZipName, cv_alphazooRomInfo, cv_alphazooRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Amazing Bumpman (USA)
static struct BurnRomInfo cv_amazingRomDesc[] = {
	{ "Amazing Bumpman (USA)(1986)(Telegames).rom",	16384, 0x78a738af, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_amazing, cv_amazing, cv_coleco)
STD_ROM_FN(cv_amazing)

struct BurnDriver BurnDrvcv_amazing = {
	"cv_amazing", NULL, "cv_coleco", NULL, "1986",
	"Amazing Bumpman (USA)\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION | GBF_QUIZ, 0,
	CVGetZipName, cv_amazingRomInfo, cv_amazingRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Number Bumper (USA, Prototype)
static struct BurnRomInfo cv_numbumpRomDesc[] = {
	{ "Number Bumper (USA, Proto)(1984)(Sunrise Software).rom",	32768, 0x6af8b439, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_numbump, cv_numbump, cv_coleco)
STD_ROM_FN(cv_numbump)

struct BurnDriver BurnDrvcv_numbump = {
	"cv_numbump", "cv_amazing", "cv_coleco", NULL, "1984",
	"Number Bumper (USA, Prototype)\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_ACTION | GBF_QUIZ, 0,
	CVGetZipName, cv_numbumpRomInfo, cv_numbumpRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Antarctic Adventure (USA, Euro)
static struct BurnRomInfo cv_antarctRomDesc[] = {
	{ "Antarctic Adventure (USA, Euro)(1984)(Coleco - Konami).rom",	16384, 0x275c800e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_antarct, cv_antarct, cv_coleco)
STD_ROM_FN(cv_antarct)

struct BurnDriver BurnDrvcv_antarct = {
	"cv_antarct", NULL, "cv_coleco", NULL, "1984",
	"Antarctic Adventure (USA, Euro)\0", NULL, "Coleco - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_antarctRomInfo, cv_antarctRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Antarctic Adventure (USA, Prototype)
static struct BurnRomInfo cv_antarctpRomDesc[] = {
	{ "Antarctic Adventure (USA, Proto)(1984)(Konami).rom",	16384, 0xa66e5ed1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_antarctp, cv_antarctp, cv_coleco)
STD_ROM_FN(cv_antarctp)

struct BurnDriver BurnDrvcv_antarctp = {
	"cv_antarctp", "cv_antarct", "cv_coleco", NULL, "1984",
	"Antarctic Adventure (USA, Prototype)\0", NULL, "Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_antarctpRomInfo, cv_antarctpRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Aquattack (USA)
static struct BurnRomInfo cv_aquatackRomDesc[] = {
	{ "Aquattack (USA)(1984)(Interphase).rom",	16384, 0x275a7013, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_aquatack, cv_aquatack, cv_coleco)
STD_ROM_FN(cv_aquatack)

struct BurnDriver BurnDrvcv_aquatack = {
	"cv_aquatack", NULL, "cv_coleco", NULL, "1984",
	"Aquattack (USA)\0", NULL, "Interphase", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_aquatackRomInfo, cv_aquatackRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Aquattack (USA, Alt)
static struct BurnRomInfo cv_aquatackaRomDesc[] = {
	{ "Aquattack (USA, Alt)(1984)(Interphase).rom",	16384, 0x947437ec, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_aquatacka, cv_aquatacka, cv_coleco)
STD_ROM_FN(cv_aquatacka)

struct BurnDriver BurnDrvcv_aquatacka = {
	"cv_aquatacka", "cv_aquatack", "cv_coleco", NULL, "1984",
	"Aquattack (USA, Alt)\0", NULL, "Interphase", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_aquatackaRomInfo, cv_aquatackaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Artillery Duel (USA)
static struct BurnRomInfo cv_artduelRomDesc[] = {
	{ "Artillery Duel (USA)(1983)(Xonox).rom",	16384, 0x6f88fcf0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_artduel, cv_artduel, cv_coleco)
STD_ROM_FN(cv_artduel)

struct BurnDriver BurnDrvcv_artduel = {
	"cv_artduel", NULL, "cv_coleco", NULL, "1983",
	"Artillery Duel (USA)\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_artduelRomInfo, cv_artduelRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// BC's Quest for Tires (USA)
static struct BurnRomInfo cv_bcquestRomDesc[] = {
	{ "BC's Quest for Tires (USA)(1983)(Sierra On-Line).rom",	16384, 0x4359a3e5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bcquest, cv_bcquest, cv_coleco)
STD_ROM_FN(cv_bcquest)

struct BurnDriver BurnDrvcv_bcquest = {
	"cv_bcquest", NULL, "cv_coleco", NULL, "1983",
	"BC's Quest for Tires (USA)\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_bcquestRomInfo, cv_bcquestRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// BC's Quest for Tires (Canada)
static struct BurnRomInfo cv_bcquestcaRomDesc[] = {
	{ "BC's Quest for Tires (Canada)(1983)(Sierra On-Line).rom",	16384, 0x0ae39f2c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bcquestca, cv_bcquestca, cv_coleco)
STD_ROM_FN(cv_bcquestca)

struct BurnDriver BurnDrvcv_bcquestca = {
	"cv_bcquestca", "cv_bcquest", "cv_coleco", NULL, "1983",
	"BC's Quest for Tires (Canada)\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_bcquestcaRomInfo, cv_bcquestcaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// BC's Quest for Tires II: Grog's Revenge (USA)
static struct BurnRomInfo cv_bcquest2RomDesc[] = {
	{ "BC's Quest for Tires II - Grog's Revenge (USA)(1984)(Coleco - Sydney).rom",	24576, 0xd464e5e4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bcquest2, cv_bcquest2, cv_coleco)
STD_ROM_FN(cv_bcquest2)

struct BurnDriver BurnDrvcv_bcquest2 = {
	"cv_bcquest2", NULL, "cv_coleco", NULL, "1984",
	"BC's Quest for Tires II: Grog's Revenge (USA)\0", NULL, "Coleco - Sydney Dev.", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_bcquest2RomInfo, cv_bcquest2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// BC's Quest for Tires II: Grog's Revenge (Canada)
static struct BurnRomInfo cv_bcquest2caRomDesc[] = {
	{ "BC's Quest for Tires II - Grog's Revenge (Canada)(1984)(Coleco - Sydney).rom",	24576, 0x1ca853d5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bcquest2ca, cv_bcquest2ca, cv_coleco)
STD_ROM_FN(cv_bcquest2ca)

struct BurnDriver BurnDrvcv_bcquest2ca = {
	"cv_bcquest2ca", "cv_bcquest2", "cv_coleco", NULL, "1984",
	"BC's Quest for Tires II: Grog's Revenge (Canada)\0", NULL, "Coleco - Sydney Dev.", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_bcquest2caRomInfo, cv_bcquest2caRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Beamrider (USA)
static struct BurnRomInfo cv_beamridrRomDesc[] = {
	{ "Beamrider (USA)(1983)(Activision).rom",	16384, 0x7a93c6e5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_beamridr, cv_beamridr, cv_coleco)
STD_ROM_FN(cv_beamridr)

struct BurnDriver BurnDrvcv_beamridr = {
	"cv_beamridr", NULL, "cv_coleco", NULL, "1983",
	"Beamrider (USA)\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_beamridrRomInfo, cv_beamridrRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Berenstain Bears, The (USA, Prototype)
static struct BurnRomInfo cv_bbearsRomDesc[] = {
	{ "Berenstain Bears, The (USA, Proto)(1984)(Coleco).rom",	8192, 0x18864abc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bbears, cv_bbears, cv_coleco)
STD_ROM_FN(cv_bbears)

struct BurnDriver BurnDrvcv_bbears = {
	"cv_bbears", NULL, "cv_coleco", NULL, "1984",
	"Berenstain Bears, The (USA, Prototype)\0", "Not playable Demo", "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_bbearsRomInfo, cv_bbearsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Blockade Runner (USA)
static struct BurnRomInfo cv_blockrunRomDesc[] = {
	{ "Blockade Runner (USA)(1984)(Interphase).rom",	16384, 0xdf65fc87, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_blockrun, cv_blockrun, cv_coleco)
STD_ROM_FN(cv_blockrun)

struct BurnDriver BurnDrvcv_blockrun = {
	"cv_blockrun", NULL, "cv_coleco", NULL, "1984",
	"Blockade Runner (USA)\0", NULL, "Interphase", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_SHOOT | GBF_SIM, 0,
	CVGetZipName, cv_blockrunRomInfo, cv_blockrunRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Boulder Dash (USA)
static struct BurnRomInfo cv_bdashRomDesc[] = {
	{ "Boulder Dash (USA)(1984)(Telegames).rom",	16384, 0x9b547ba8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bdash, cv_bdash, cv_coleco)
STD_ROM_FN(cv_bdash)

struct BurnDriver BurnDrvcv_bdash = {
	"cv_bdash", NULL, "cv_coleco", NULL, "1984",
	"Boulder Dash (USA)\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_bdashRomInfo, cv_bdashRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Boulder Dash (USA, Prototype)
static struct BurnRomInfo cv_bdashpRomDesc[] = {
	{ "Boulder Dash (USA, Proto)(1984)(Micro Fun).rom",	16384, 0x1796de5e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bdashp, cv_bdashp, cv_coleco)
STD_ROM_FN(cv_bdashp)

struct BurnDriver BurnDrvcv_bdashp = {
	"cv_bdashp", "cv_bdash", "cv_coleco", NULL, "1984",
	"Boulder Dash (USA, Prototype)\0", NULL, "Micro Fun", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_bdashpRomInfo, cv_bdashpRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Brain Strainers (USA)
static struct BurnRomInfo cv_brainstrRomDesc[] = {
	{ "Brain Strainers (USA)(1984)(Coleco).rom",	16384, 0x829c967d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_brainstr, cv_brainstr, cv_coleco)
STD_ROM_FN(cv_brainstr)

struct BurnDriver BurnDrvcv_brainstr = {
	"cv_brainstr", NULL, "cv_coleco", NULL, "1984",
	"Brain Strainers (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_brainstrRomInfo, cv_brainstrRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Buck Rogers: Planet of Zoom (USA, Euro)
static struct BurnRomInfo cv_buckrogRomDesc[] = {
	{ "Buck Rogers - Planet of Zoom (USA, Euro)(1983)(Coleco).rom",	24576, 0x00b37475, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_buckrog, cv_buckrog, cv_coleco)
STD_ROM_FN(cv_buckrog)

struct BurnDriver BurnDrvcv_buckrog = {
	"cv_buckrog", NULL, "cv_coleco", NULL, "1983",
	"Buck Rogers: Planet of Zoom (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_buckrogRomInfo, cv_buckrogRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bump 'n' Jump (USA, Euro)
static struct BurnRomInfo cv_bnjRomDesc[] = {
	{ "Bump 'n' Jump (USA, Euro)(1984)(Coleco).rom",	20480, 0x9e1fab59, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bnj, cv_bnj, cv_coleco)
STD_ROM_FN(cv_bnj)

struct BurnDriver BurnDrvcv_bnj = {
	"cv_bnj", NULL, "cv_coleco", NULL, "1984",
	"Bump 'n' Jump (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_bnjRomInfo, cv_bnjRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bump 'n' Jump (USA, Prototype)
static struct BurnRomInfo cv_bnjpRomDesc[] = {
	{ "Bump 'n' Jump (USA, Proto)(1983)(Mattel Electronics).rom",	16384, 0xeb498f97, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bnjp, cv_bnjp, cv_coleco)
STD_ROM_FN(cv_bnjp)

struct BurnDriver BurnDrvcv_bnjp = {
	"cv_bnjp", "cv_bnj", "cv_coleco", NULL, "1983",
	"Bump 'n' Jump (USA, Prototype)\0", NULL, "Mattel Electronics", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_bnjpRomInfo, cv_bnjpRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// BurgerTime (USA, Euro)
static struct BurnRomInfo cv_btimeRomDesc[] = {
	{ "BurgerTime (USA, Euro)(1984)(Coleco).rom",	16384, 0x91346341, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_btime, cv_btime, cv_coleco)
STD_ROM_FN(cv_btime)

struct BurnDriver BurnDrvcv_btime = {
	"cv_btime", NULL, "cv_coleco", NULL, "1984",
	"BurgerTime (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_btimeRomInfo, cv_btimeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// BurgerTime (USA, Prototype)
static struct BurnRomInfo cv_btimemRomDesc[] = {
	{ "BurgerTime (USA, Proto)(1983)(Mattel Electronics).rom",	16384, 0xe5c5ea20, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_btimem, cv_btimem, cv_coleco)
STD_ROM_FN(cv_btimem)

struct BurnDriver BurnDrvcv_btimem = {
	"cv_btimem", "cv_btime", "cv_coleco", NULL, "1983",
	"BurgerTime (USA, Prototype)\0", NULL, "Mattel Electronics", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_btimemRomInfo, cv_btimemRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cabbage Patch Kids: Adventure in the Park (USA, Euro)
static struct BurnRomInfo cv_cabbageRomDesc[] = {
	{ "Cabbage Patch Kids - Adventures in the Park (USA, Euro)(1984)(Coleco - Konami).rom",	16384, 0x6af19e75, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cabbage, cv_cabbage, cv_coleco)
STD_ROM_FN(cv_cabbage)

struct BurnDriver BurnDrvcv_cabbage = {
	"cv_cabbage", NULL, "cv_coleco", NULL, "1984",
	"Cabbage Patch Kids: Adventure in the Park (USA. Euro)\0", NULL, "Coleco - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_cabbageRomInfo, cv_cabbageRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cabbage Patch Kids: Adventure in the Park (USA, Prototype)
static struct BurnRomInfo cv_cabbagep1RomDesc[] = {
	{ "Cabbage Patch Kids - Adventures in the Park (USA, Proto 1)(1984)(Coleco - Konami).rom",	16384, 0xbeacbff5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cabbagep1, cv_cabbagep1, cv_coleco)
STD_ROM_FN(cv_cabbagep1)

struct BurnDriver BurnDrvcv_cabbagep1 = {
	"cv_cabbagep1", "cv_cabbage", "cv_coleco", NULL, "1984",
	"Cabbage Patch Kids: Adventure in the Park (USA, Prototype)\0", NULL, "Coleco - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_cabbagep1RomInfo, cv_cabbagep1RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cabbage Patch Kids: Adventure in the Park (USA, Prototype, Alt)
static struct BurnRomInfo cv_cabbagep2RomDesc[] = {
	{ "Cabbage Patch Kids - Adventures in the Park (USA, Proto 2)(1984)(Coleco - Konami).rom",	16384, 0x5da13dfd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cabbagep2, cv_cabbagep2, cv_coleco)
STD_ROM_FN(cv_cabbagep2)

struct BurnDriver BurnDrvcv_cabbagep2 = {
	"cv_cabbagep2", "cv_cabbage", "cv_coleco", NULL, "1984",
	"Cabbage Patch Kids: Adventure in the Park (USA, Prototype, Alt)\0", NULL, "Coleco - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_cabbagep2RomInfo, cv_cabbagep2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cabbage Patch Kids: Picture Show (USA)
static struct BurnRomInfo cv_cabbshowRomDesc[] = {
	{ "Cabbage Patch Kids - Picture Show (USA)(1984)(Coleco).rom",	24576, 0x75b08d99, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cabbshow, cv_cabbshow, cv_coleco)
STD_ROM_FN(cv_cabbshow)

struct BurnDriver BurnDrvcv_cabbshow = {
	"cv_cabbshow", NULL, "cv_coleco", NULL, "1984",
	"Cabbage Patch Kids: Picture Show (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_cabbshowRomInfo, cv_cabbshowRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Carnival (USA, Euro)
static struct BurnRomInfo cv_carnivalRomDesc[] = {
	{ "Carnival (USA, Euro)(1982)(Coleco).rom",	16384, 0x70f315c2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_carnival, cv_carnival, cv_coleco)
STD_ROM_FN(cv_carnival)

struct BurnDriver BurnDrvcv_carnival = {
	"cv_carnival", NULL, "cv_coleco", NULL, "1982",
	"Carnival (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_carnivalRomInfo, cv_carnivalRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Castelo (Brazil) (Unl)
static struct BurnRomInfo cv_casteloRomDesc[] = {
	{ "Castelo (Brazil)(Unl)(1985)(Splice Vision).rom",	8192, 0x07a3d3db, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_castelo, cv_castelo, cv_coleco)
STD_ROM_FN(cv_castelo)

struct BurnDriver BurnDrvcv_castelo = {
	"cv_castelo", NULL, "cv_coleco", NULL, "1985",
	"Castelo (Brazil) (Unl)\0", NULL, "Splice Vision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_casteloRomInfo, cv_casteloRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Centipede (USA)
static struct BurnRomInfo cv_centipedRomDesc[] = {
	{ "Centipede (USA)(1983)(Atarisoft).rom",	16384, 0x17edbfd4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_centiped, cv_centiped, cv_coleco)
STD_ROM_FN(cv_centiped)

struct BurnDriver BurnDrvcv_centiped = {
	"cv_centiped", NULL, "cv_coleco", NULL, "1983",
	"Centipede (USA)\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_centipedRomInfo, cv_centipedRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Choplifter! (USA)
static struct BurnRomInfo cv_chopliftRomDesc[] = {
	{ "Choplifter! (USA)(1984)(Coleco).rom",	16384, 0x030e0d48, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_choplift, cv_choplift, cv_coleco)
STD_ROM_FN(cv_choplift)

struct BurnDriver BurnDrvcv_choplift = {
	"cv_choplift", NULL, "cv_coleco", NULL, "1984",
	"Choplifter! (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_chopliftRomInfo, cv_chopliftRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Chuck Norris Superkicks (USA)
static struct BurnRomInfo cv_chucknorRomDesc[] = {
	{ "Chuck Norris Superkicks (USA)(1983)(Xonox).rom",	16384, 0x5eeb81de, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_chucknor, cv_chucknor, cv_coleco)
STD_ROM_FN(cv_chucknor)

struct BurnDriver BurnDrvcv_chucknor = {
	"cv_chucknor", NULL, "cv_coleco", NULL, "1983",
	"Chuck Norris Superkicks (USA)\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_chucknorRomInfo, cv_chucknorRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Congo Bongo (USA)
static struct BurnRomInfo cv_congoRomDesc[] = {
	{ "Congo Bongo (USA)(1984)(Coleco).rom",	24576, 0x4e8b4c1d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_congo, cv_congo, cv_coleco)
STD_ROM_FN(cv_congo)

struct BurnDriver BurnDrvcv_congo = {
	"cv_congo", NULL, "cv_coleco", NULL, "1984",
	"Congo Bongo (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_congoRomInfo, cv_congoRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cosmic Avenger (USA, Euro)
static struct BurnRomInfo cv_cavengerRomDesc[] = {
	{ "Cosmic Avenger (USA, Euro)(1982)(Coleco).rom",	16384, 0x39d4215b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cavenger, cv_cavenger, cv_coleco)
STD_ROM_FN(cv_cavenger)

struct BurnDriver BurnDrvcv_cavenger = {
	"cv_cavenger", NULL, "cv_coleco", NULL, "1982",
	"Cosmic Avenger (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_cavengerRomInfo, cv_cavengerRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cosmic Avenger (USA, Alt)
static struct BurnRomInfo cv_cavengeraRomDesc[] = {
	{ "Cosmic Avenger (USA, Alt)(1982)(Coleco).rom",	16384, 0x14d6ced6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cavengera, cv_cavengera, cv_coleco)
STD_ROM_FN(cv_cavengera)

struct BurnDriver BurnDrvcv_cavengera = {
	"cv_cavengera", "cv_cavenger", "cv_coleco", NULL, "1982",
	"Cosmic Avenger (USA, Alt)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_cavengeraRomInfo, cv_cavengeraRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Dam Busters, The (USA)
static struct BurnRomInfo cv_dambustRomDesc[] = {
	{ "Dam Busters, The (USA)(1984)(Coleco - Sydney).rom",	32768, 0x890682b3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dambust, cv_dambust, cv_coleco)
STD_ROM_FN(cv_dambust)

struct BurnDriver BurnDrvcv_dambust = {
	"cv_dambust", NULL, "cv_coleco", NULL, "1984",
	"Dam Busters, The (USA)\0", NULL, "Coleco - Sydney Dev.", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_SIM, 0,
	CVGetZipName, cv_dambustRomInfo, cv_dambustRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Dam Busters, The (Canada)
static struct BurnRomInfo cv_dambustcaRomDesc[] = {
	{ "Dam Busters, The (Canada)(1984)(Sydney).rom",	32768, 0xc75fad5a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dambustca, cv_dambustca, cv_coleco)
STD_ROM_FN(cv_dambustca)

struct BurnDriver BurnDrvcv_dambustca = {
	"cv_dambustca", "cv_dambust", "cv_coleco", NULL, "1984",
	"Dam Busters, The (Canada)\0", NULL, "Sydney Dev.", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_COLECO, GBF_SIM, 0,
	CVGetZipName, cv_dambustcaRomInfo, cv_dambustcaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Decathlon (USA)
static struct BurnRomInfo cv_decathlnRomDesc[] = {
	{ "Decathlon (USA)(1984)(Activision).rom",	16384, 0x51fe49c8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_decathln, cv_decathln, cv_coleco)
STD_ROM_FN(cv_decathln)

struct BurnDriver BurnDrvcv_decathln = {
	"cv_decathln", NULL, "cv_coleco", NULL, "1984",
	"Decathlon (USA)\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SPORTSMISC, 0,
	CVGetZipName, cv_decathlnRomInfo, cv_decathlnRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Defender (USA)
static struct BurnRomInfo cv_defenderRomDesc[] = {
	{ "Defender (USA)(1983)(Atarisoft).rom",	24576, 0x6cf594e5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_defender, cv_defender, cv_coleco)
STD_ROM_FN(cv_defender)

struct BurnDriver BurnDrvcv_defender = {
	"cv_defender", NULL, "cv_coleco", NULL, "1983",
	"Defender (USA)\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_defenderRomInfo, cv_defenderRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Defender [Bug Fixed] (USA)
static struct BurnRomInfo cv_defenderfixRomDesc[] = {
	{ "Defender - Bug Fixed (USA)(1983)(Atarisoft).rom",	24576, 0x46a909cc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_defenderfix, cv_defenderfix, cv_coleco)
STD_ROM_FN(cv_defenderfix)

struct BurnDriver BurnDrvcv_defenderfix = {
	"cv_defenderfix", "cv_defender", "cv_coleco", NULL, "1983",
	"Defender [Bug Fixed] (USA)\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_defenderfixRomInfo, cv_defenderfixRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Destructor (USA, Euro)
static struct BurnRomInfo cv_destructRomDesc[] = {
	{ "Destructor (USA, Euro)(1984)(Coleco).rom",	32768, 0x56c358a6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_destruct, cv_destruct, cv_coleco)
STD_ROM_FN(cv_destruct)

struct BurnDriver BurnDrvcv_destruct = {
	"cv_destruct", NULL, "cv_coleco", NULL, "1984",
	"Destructor (USA, Euro)\0", "NB: select skill level with Controller #2", "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_destructRomInfo, cv_destructRomName, NULL, NULL, NULL, NULL, SACInputInfo, SACDIPInfo,
	DrvInitSAC, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Dig Dug (USA, Prototype)
static struct BurnRomInfo cv_digdugRomDesc[] = {
	{ "Dig Dug (USA, Proto)(1984)(Atarisoft).rom",	24576, 0x1038e0a1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_digdug, cv_digdug, cv_coleco)
STD_ROM_FN(cv_digdug)

struct BurnDriver BurnDrvcv_digdug = {
	"cv_digdug", NULL, "cv_coleco", NULL, "1984",
	"Dig Dug (USA, Prototype)\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_digdugRomInfo, cv_digdugRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Donkey Kong (USA)
static struct BurnRomInfo cv_dkongaRomDesc[] = {
	{ "Donkey Kong (USA)(1982)(Coleco).rom",	24576, 0x94c4cd4a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dkonga, cv_dkonga, cv_coleco)
STD_ROM_FN(cv_dkonga)

struct BurnDriver BurnDrvcv_dkonga = {
	"cv_dkonga", "cv_dkong", "cv_coleco", NULL, "1982",
	"Donkey Kong (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_dkongaRomInfo, cv_dkongaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Donkey Kong (USA, Euro, v1.1)
static struct BurnRomInfo cv_dkongRomDesc[] = {
	{ "Donkey Kong v1.1 (USA, Euro)(1982)(Coleco).rom",	16384, 0x3c77198c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dkong, cv_dkong, cv_coleco)
STD_ROM_FN(cv_dkong)

struct BurnDriver BurnDrvcv_dkong = {
	"cv_dkong", NULL, "cv_coleco", NULL, "1982",
	"Donkey Kong (USA, Euro, v1.1)\0", NULL, "Coleco - Nintendo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_dkongRomInfo, cv_dkongRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Donkey Kong Junior (USA, Euro)
static struct BurnRomInfo cv_dkongjrRomDesc[] = {
	{ "Donkey Kong Junior (USA, Euro)(1983)(Coleco).rom",	16384, 0xfd11508d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dkongjr, cv_dkongjr, cv_coleco)
STD_ROM_FN(cv_dkongjr)

struct BurnDriver BurnDrvcv_dkongjr = {
	"cv_dkongjr", NULL, "cv_coleco", NULL, "1983",
	"Donkey Kong Junior (USA, Euro)\0", NULL, "Coleco - Nintendo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_dkongjrRomInfo, cv_dkongjrRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Dragonfire (USA)
static struct BurnRomInfo cv_drgnfireRomDesc[] = {
	{ "Dragonfire (USA)(1984)(Imagic).rom",	16384, 0xdcb8f9e8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_drgnfire, cv_drgnfire, cv_coleco)
STD_ROM_FN(cv_drgnfire)

struct BurnDriver BurnDrvcv_drgnfire = {
	"cv_drgnfire", NULL, "cv_coleco", NULL, "1984",
	"Dragonfire (USA)\0", NULL, "Imagic", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_drgnfireRomInfo, cv_drgnfireRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Evolution (USA, Canada)
static struct BurnRomInfo cv_evolutioRomDesc[] = {
	{ "Evolution (USA, Canada)(1983)(Sydney).rom",	16384, 0x4cd4e185, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_evolutio, cv_evolutio, cv_coleco)
STD_ROM_FN(cv_evolutio)

struct BurnDriver BurnDrvcv_evolutio = {
	"cv_evolutio", NULL, "cv_coleco", NULL, "1983",
	"Evolution (USA, Canada)\0", NULL, "Sydney Dev.", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_evolutioRomInfo, cv_evolutioRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Facemaker (USA)
static struct BurnRomInfo cv_facemakrRomDesc[] = {
	{ "Facemaker (USA)(1983)(Spinnaker Software).rom",	8192, 0xec9dfb07, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_facemakr, cv_facemakr, cv_coleco)
STD_ROM_FN(cv_facemakr)

struct BurnDriver BurnDrvcv_facemakr = {
	"cv_facemakr", NULL, "cv_coleco", NULL, "1983",
	"Facemaker (USA)\0", NULL, "Spinnaker Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_facemakrRomInfo, cv_facemakrRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Fathom (USA)
static struct BurnRomInfo cv_fathomRomDesc[] = {
	{ "Fathom (USA)(1983)(Imagic).rom",	16384, 0xb3b767ae, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_fathom, cv_fathom, cv_coleco)
STD_ROM_FN(cv_fathom)

struct BurnDriver BurnDrvcv_fathom = {
	"cv_fathom", NULL, "cv_coleco", NULL, "1983",
	"Fathom (USA)\0", NULL, "Imagic", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_fathomRomInfo, cv_fathomRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Fireman (USA)
static struct BurnRomInfo cv_firemanRomDesc[] = {
    { "Fireman (USA)(1984)(Coleco).rom",	32768, 0x64ec761e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_fireman, cv_fireman, cv_coleco)
STD_ROM_FN(cv_fireman)

struct BurnDriver BurnDrvcv_fireman = {
    "cv_fireman", NULL, "cv_coleco", NULL, "1984",
    "Fireman (USA)\0", NULL, "Coleco", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_firemanRomInfo, cv_firemanRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Flipper Slipper (USA, Euro)
static struct BurnRomInfo cv_flipslipRomDesc[] = {
	{ "Flipper Slipper (USA, Euro)(1983)(Spectravideo).rom",	16384, 0x2a49f507, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_flipslip, cv_flipslip, cv_coleco)
STD_ROM_FN(cv_flipslip)

struct BurnDriver BurnDrvcv_flipslip = {
	"cv_flipslip", NULL, "cv_coleco", NULL, "1983",
	"Flipper Slipper (USA, Euro)\0", NULL, "Spectravideo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_BREAKOUT, 0,
	CVGetZipName, cv_flipslipRomInfo, cv_flipslipRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Frantic Freddy (USA)
static struct BurnRomInfo cv_ffreddyRomDesc[] = {
	{ "Frantic Freddy (USA)(1983)(Spectravideo).rom",	16384, 0x8615c6e8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ffreddy, cv_ffreddy, cv_coleco)
STD_ROM_FN(cv_ffreddy)

struct BurnDriver BurnDrvcv_ffreddy = {
	"cv_ffreddy", NULL, "cv_coleco", NULL, "1983",
	"Frantic Freddy (USA)\0", NULL, "Spectravideo", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_ffreddyRomInfo, cv_ffreddyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Frenzy (USA)
static struct BurnRomInfo cv_frenzyRomDesc[] = {
	{ "Frenzy (USA)(1983)(Coleco).rom",	20480, 0x3cacddfb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frenzy, cv_frenzy, cv_coleco)
STD_ROM_FN(cv_frenzy)

struct BurnDriver BurnDrvcv_frenzy = {
	"cv_frenzy", NULL, "cv_coleco", NULL, "1983",
	"Frenzy (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
	CVGetZipName, cv_frenzyRomInfo, cv_frenzyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Frenzy (USA, Alt 1)
static struct BurnRomInfo cv_frenzyaRomDesc[] = {
	{ "Frenzy (USA, Alt 1)(1983)(Coleco).rom",	24576, 0x960616af, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frenzya, cv_frenzya, cv_coleco)
STD_ROM_FN(cv_frenzya)

struct BurnDriver BurnDrvcv_frenzya = {
	"cv_frenzya", "cv_frenzy", "cv_coleco", NULL, "1983",
	"Frenzy (USA, Alt 1)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
	CVGetZipName, cv_frenzyaRomInfo, cv_frenzyaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Frenzy (USA, Alt 2)
static struct BurnRomInfo cv_frenzya2RomDesc[] = {
	{ "Frenzy (USA, Alt 2)(1983)(Coleco).rom",	24576, 0xbf1ccf04, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frenzya2, cv_frenzya2, cv_coleco)
STD_ROM_FN(cv_frenzya2)

struct BurnDriver BurnDrvcv_frenzya2 = {
	"cv_frenzya2", "cv_frenzy", "cv_coleco", NULL, "1983",
	"Frenzy (USA, Alt 2)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
	CVGetZipName, cv_frenzya2RomInfo, cv_frenzya2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Frogger (USA)
static struct BurnRomInfo cv_froggerRomDesc[] = {
	{ "Frogger (USA)(1983)(Parker Brothers).rom",	12288, 0x798002a2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frogger, cv_frogger, cv_coleco)
STD_ROM_FN(cv_frogger)

struct BurnDriver BurnDrvcv_frogger = {
	"cv_frogger", NULL, "cv_coleco", NULL, "1983",
	"Frogger (USA)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_froggerRomInfo, cv_froggerRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// Frogger II: Threedeep! (USA)
static struct BurnRomInfo cv_frogger2RomDesc[] = {
	{ "Frogger II - ThreeeDeep! (USA)(1984)(Parker Brothers).rom",	16384, 0x4229ea5a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frogger2, cv_frogger2, cv_coleco)
STD_ROM_FN(cv_frogger2)

struct BurnDriver BurnDrvcv_frogger2 = {
	"cv_frogger2", NULL, "cv_coleco", NULL, "1984",
	"Frogger II: Threedeep! (USA)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_frogger2RomInfo, cv_frogger2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Front Line (USA)
static struct BurnRomInfo cv_frontlinRomDesc[] = {
	{ "Front Line (USA)(1983)(Coleco).rom",	24576, 0xd0110137, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frontlin, cv_frontlin, cv_coleco)
STD_ROM_FN(cv_frontlin)

struct BurnDriver BurnDrvcv_frontlin = {
	"cv_frontlin", NULL, "cv_coleco", NULL, "1983",
	"Front Line (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_RUNGUN, 0,
	CVGetZipName, cv_frontlinRomInfo, cv_frontlinRomName, NULL, NULL, NULL, NULL, SACInputInfo, SACDIPInfo,
	DrvInitSAC, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Front Line (USA, Alt)
static struct BurnRomInfo cv_frontlinaRomDesc[] = {
	{ "Front Line (USA, Alt)(1983)(Coleco).rom",	24576, 0x9c551386, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frontlina, cv_frontlina, cv_coleco)
STD_ROM_FN(cv_frontlina)

struct BurnDriver BurnDrvcv_frontlina = {
	"cv_frontlina", "cv_frontlin", "cv_coleco", NULL, "1983",
	"Front Line (USA, Alt)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_RUNGUN, 0,
	CVGetZipName, cv_frontlinaRomInfo, cv_frontlinaRomName, NULL, NULL, NULL, NULL, SACInputInfo, SACDIPInfo,
	DrvInitSAC, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Galaxian (USA)
static struct BurnRomInfo cv_galaxianRomDesc[] = {
	{ "Galaxian (USA)(1983)(Atarisoft).rom",	32768, 0xfe1d0b6a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_galaxian, cv_galaxian, cv_coleco)
STD_ROM_FN(cv_galaxian)

struct BurnDriver BurnDrvcv_galaxian = {
	"cv_galaxian", NULL, "cv_coleco", NULL, "1983",
	"Galaxian (USA)\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_galaxianRomInfo, cv_galaxianRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Galaxian (USA, Alt)
static struct BurnRomInfo cv_galaxianaRomDesc[] = {
	{ "Galaxian (USA, Alt)(1983)(Atarisoft).rom",	32768, 0x987bc7ac, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_galaxiana, cv_galaxiana, cv_coleco)
STD_ROM_FN(cv_galaxiana)

struct BurnDriver BurnDrvcv_galaxiana = {
	"cv_galaxiana", "cv_galaxian", "cv_coleco", NULL, "1983",
	"Galaxian (USA, Alt)\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_galaxianaRomInfo, cv_galaxianaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Gateway to Apshai (USA, Euro)
static struct BurnRomInfo cv_apshaiRomDesc[] = {
	{ "Gateway to Apshai (USA, Euro)(1984)(Epyx).rom",	12288, 0xfdb75be6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_apshai, cv_apshai, cv_coleco)
STD_ROM_FN(cv_apshai)

struct BurnDriver BurnDrvcv_apshai = {
	"cv_apshai", NULL, "cv_coleco", NULL, "1984",
	"Gateway to Apshai (USA, Euro)\0", NULL, "Epyx", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION | GBF_RPG, 0,
	CVGetZipName, cv_apshaiRomInfo, cv_apshaiRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Gorf (USA, Euro)
static struct BurnRomInfo cv_gorfRomDesc[] = {
	{ "Gorf (USA, Euro)(1983)(Coleco).rom",	16384, 0xff46becc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gorf, cv_gorf, cv_coleco)
STD_ROM_FN(cv_gorf)

struct BurnDriver BurnDrvcv_gorf = {
	"cv_gorf", NULL, "cv_coleco", NULL, "1983",
	"Gorf (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_gorfRomInfo, cv_gorfRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Gust Buster (USA)
static struct BurnRomInfo cv_gustbustRomDesc[] = {
	{ "Gust Buster (USA)(1983)(Sunrise Software).rom",	16384, 0x29fea563, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gustbust, cv_gustbust, cv_coleco)
STD_ROM_FN(cv_gustbust)

struct BurnDriver BurnDrvcv_gustbust = {
	"cv_gustbust", NULL, "cv_coleco", NULL, "1983",
	"Gust Buster (USA)\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_gustbustRomInfo, cv_gustbustRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Gyruss (USA)
static struct BurnRomInfo cv_gyrussRomDesc[] = {
	{ "Gyruss (USA)(1984)(Parker Brothers).rom",	16384, 0x27cbf26d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gyruss, cv_gyruss, cv_coleco)
STD_ROM_FN(cv_gyruss)

struct BurnDriver BurnDrvcv_gyruss = {
	"cv_gyruss", NULL, "cv_coleco", NULL, "1984",
	"Gyruss (USA)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_gyrussRomInfo, cv_gyrussRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Gyruss (USA, Prototype)
static struct BurnRomInfo cv_gyrusspRomDesc[] = {
	{ "Gyruss (USA, Proto)(1984)(Parker Brothers).rom",	8448, 0xb35d4dc9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gyrussp, cv_gyrussp, cv_coleco)
STD_ROM_FN(cv_gyrussp)

struct BurnDriver BurnDrvcv_gyrussp = {
	"cv_gyrussp", "cv_gyruss", "cv_coleco", NULL, "1984",
	"Gyruss (USA, Prototype)\0", "Incomplete version", "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_gyrusspRomInfo, cv_gyrusspRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// H.E.R.O. - Helicopter Emergency Rescue Operation (USA)
static struct BurnRomInfo cv_heroRomDesc[] = {
	{ "H.E.R.O. - Helicopter Emergency Rescue Operation (USA)(1984)(Activision).rom",	16384, 0x685ab9b5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_hero, cv_hero, cv_coleco)
STD_ROM_FN(cv_hero)

struct BurnDriver BurnDrvcv_hero = {
	"cv_hero", NULL, "cv_coleco", NULL, "1984",
	"H.E.R.O. - Helicopter Emergency Rescue Operation (USA)\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_heroRomInfo, cv_heroRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Heist, The (USA)
static struct BurnRomInfo cv_heistRomDesc[] = {
	{ "Heist, The (USA)(1983)(Micro Fun).rom",	0x06000, 0x6f2e2d84, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_heist, cv_heist, cv_coleco)
STD_ROM_FN(cv_heist)

struct BurnDriver BurnDrvcv_heist = {
	"cv_heist", NULL, "cv_coleco", NULL, "1983",
	"Heist, The (USA)\0", NULL, "Micro Fun", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_heistRomInfo, cv_heistRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Joust (USA, Prototype)
static struct BurnRomInfo cv_joustRomDesc[] = {
	{ "Joust (USA, Proto)(1983)(Atarisoft).rom",	24576, 0x52228000, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_joust, cv_joust, cv_coleco)
STD_ROM_FN(cv_joust)

struct BurnDriver BurnDrvcv_joust = {
	"cv_joust", NULL, "cv_coleco", NULL, "1983",
	"Joust (USA, Prototype)\0", "No sound", "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_joustRomInfo, cv_joustRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Juke Box (USA)
static struct BurnRomInfo cv_jukeboxRomDesc[] = {
	{ "Jukebox (USA)(1984)(Spinnaker Software).rom",	8192, 0xa5511418, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jukebox, cv_jukebox, cv_coleco)
STD_ROM_FN(cv_jukebox)

struct BurnDriver BurnDrvcv_jukebox = {
	"cv_jukebox", NULL, "cv_coleco", NULL, "1984",
	"Juke Box (USA)\0", NULL, "Spinnaker Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_jukeboxRomInfo, cv_jukeboxRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Jumpman Junior (USA, Euro)
static struct BurnRomInfo cv_jmpmanjrRomDesc[] = {
	{ "Jumpman Junior (USA, Euro)(1984)(Epyx).rom",	16384, 0x060c69e8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jmpmanjr, cv_jmpmanjr, cv_coleco)
STD_ROM_FN(cv_jmpmanjr)

struct BurnDriver BurnDrvcv_jmpmanjr = {
	"cv_jmpmanjr", NULL, "cv_coleco", NULL, "1984",
	"Jumpman Junior (USA, Euro)\0", NULL, "Epyx", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_jmpmanjrRomInfo, cv_jmpmanjrRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Jumpman Junior (USA, Alt)
static struct BurnRomInfo cv_jmpmanjraRomDesc[] = {
	{ "Jumpman Junior (USA, Alt)(1984)(Epyx).rom",	16384, 0x9528949a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jmpmanjra, cv_jmpmanjra, cv_coleco)
STD_ROM_FN(cv_jmpmanjra)

struct BurnDriver BurnDrvcv_jmpmanjra = {
	"cv_jmpmanjra", "cv_jmpmanjr", "cv_coleco", NULL, "1984",
	"Jumpman Junior (USA, Alt)\0", NULL, "Epyx", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_jmpmanjraRomInfo, cv_jmpmanjraRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Jungle Hunt (USA)
static struct BurnRomInfo cv_junglehRomDesc[] = {
	{ "Jungle Hunt (USA)(1983)(Atarisoft).rom",	24576, 0x58e886d2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jungleh, cv_jungleh, cv_coleco)
STD_ROM_FN(cv_jungleh)

struct BurnDriver BurnDrvcv_jungleh = {
	"cv_jungleh", NULL, "cv_coleco", NULL, "1983",
	"Jungle Hunt (USA)\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_junglehRomInfo, cv_junglehRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Ken Uston Blackjack-Poker (USA, Euro)
static struct BurnRomInfo cv_kubjpokRomDesc[] = {
	{ "Ken Uston Blackjack-Poker (USA, Euro)(1983)(Coleco).rom",	16384, 0x8c7b7803, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_kubjpok, cv_kubjpok, cv_coleco)
STD_ROM_FN(cv_kubjpok)

struct BurnDriver BurnDrvcv_kubjpok = {
	"cv_kubjpok", NULL, "cv_coleco", NULL, "1983",
	"Ken Uston Blackjack-Poker (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_COLECO, GBF_CARD | GBF_CASINO, 0,
	CVGetZipName, cv_kubjpokRomInfo, cv_kubjpokRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Keystone Kapers (USA)
static struct BurnRomInfo cv_keykaperRomDesc[] = {
	{ "Keystone Kapers (USA)(1984)(Activision).rom",	16384, 0x69fc2966, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_keykaper, cv_keykaper, cv_coleco)
STD_ROM_FN(cv_keykaper)

struct BurnDriver BurnDrvcv_keykaper = {
	"cv_keykaper", NULL, "cv_coleco", NULL, "1984",
	"Keystone Kapers (USA)\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_keykaperRomInfo, cv_keykaperRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Lady Bug (USA, Euro)
static struct BurnRomInfo cv_ladybugRomDesc[] = {
	{ "Lady Bug (USA, Euro)(1982)(Coleco).rom",	16384, 0x2c3097b8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ladybug, cv_ladybug, cv_coleco)
STD_ROM_FN(cv_ladybug)

struct BurnDriver BurnDrvcv_ladybug = {
	"cv_ladybug", NULL, "cv_coleco", NULL, "1982",
	"Lady Bug (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_ladybugRomInfo, cv_ladybugRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Learning with Leeper (USA)
static struct BurnRomInfo cv_leeperRomDesc[] = {
	{ "Learning with Leeper (USA)(1983)(Sierra On-Line).rom",	16384, 0x1641044f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_leeper, cv_leeper, cv_coleco)
STD_ROM_FN(cv_leeper)

struct BurnDriver BurnDrvcv_leeper = {
	"cv_leeper", NULL, "cv_coleco", NULL, "1983",
	"Learning with Leeper (USA)\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MINIGAMES, 0,
	CVGetZipName, cv_leeperRomInfo, cv_leeperRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// M*A*S*H (Euro, Prototype)
static struct BurnRomInfo cv_mashRomDesc[] = {
	{ "M.A.S.H (Euro, Proto)(1983)(Fox Video Games).rom",	16384, 0xfc95b302, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mash, cv_mash, cv_coleco)
STD_ROM_FN(cv_mash)

struct BurnDriver BurnDrvcv_mash = {
	"cv_mash", NULL, "cv_coleco", NULL, "1983",
	"M*A*S*H (Euro, Prototype)\0", NULL, "Fox Video Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_mashRomInfo, cv_mashRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Masters of the Universe: The Power of He-Man (USA, Prototype)
static struct BurnRomInfo cv_mastersRomDesc[] = {
	{ "Masters of the Universe - The Power of He-Man (USA, Proto)(1983)(Mattel Electronics).rom",	32768, 0x2277f82b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_masters, cv_masters, cv_coleco)
STD_ROM_FN(cv_masters)

struct BurnDriver BurnDrvcv_masters = {
	"cv_masters", NULL, "cv_coleco", NULL, "1983",
	"Masters of the Universe: The Power of He-Man (USA, Prototype)\0", NULL, "Mattel Electronics", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_ACTION | GBF_HORSHOOT, 0,
	CVGetZipName, cv_mastersRomInfo, cv_mastersRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Meteoric Shower (USA)
static struct BurnRomInfo cv_meteoshoRomDesc[] = {
	{ "Meteoric Shower (USA)(1983)(Coleco).rom",	16384, 0x3beffc06, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_meteosho, cv_meteosho, cv_coleco)
STD_ROM_FN(cv_meteosho)

struct BurnDriver BurnDrvcv_meteosho = {
	"cv_meteosho", NULL, "cv_coleco", NULL, "1983",
	"Meteoric Shower (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_meteoshoRomInfo, cv_meteoshoRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Meteoric Shower (Euro)
static struct BurnRomInfo cv_meteoshoaRomDesc[] = {
	{ "Meteoric Shower (Euro)(1983)(Coleco).rom",	12288, 0xe51f464f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_meteoshoa, cv_meteoshoa, cv_coleco)
STD_ROM_FN(cv_meteoshoa)

struct BurnDriver BurnDrvcv_meteoshoa = {
	"cv_meteoshoa", "cv_meteosho", "cv_coleco", NULL, "1983",
	"Meteoric Shower (Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_meteoshoaRomInfo, cv_meteoshoaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Miner 2049er (USA, Euro, v1.1)
static struct BurnRomInfo cv_mine2049RomDesc[] = {
	{ "Miner 2049er v1.1 (USA, Euro)(1983)(Micro Fun).rom",	24576, 0xb24f10fd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mine2049, cv_mine2049, cv_coleco)
STD_ROM_FN(cv_mine2049)

struct BurnDriver BurnDrvcv_mine2049 = {
	"cv_mine2049", NULL, "cv_coleco", NULL, "1983",
	"Miner 2049er (USA, Euro, v1.1)\0", NULL, "Micro Fun", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_mine2049RomInfo, cv_mine2049RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Miner 2049er (USA, Euro)
static struct BurnRomInfo cv_mine2049aRomDesc[] = {
	{ "Miner 2049er (USA, Euro)(1983)(Micro Fun).rom",	24576, 0xf37af58c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mine2049a, cv_mine2049a, cv_coleco)
STD_ROM_FN(cv_mine2049a)

struct BurnDriver BurnDrvcv_mine2049a = {
	"cv_mine2049a", "cv_mine2049", "cv_coleco", NULL, "1983",
	"Miner 2049er (USA, Euro)\0", NULL, "Micro Fun", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_mine2049aRomInfo, cv_mine2049aRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Monkey Academy (USA)
static struct BurnRomInfo cv_monkeyRomDesc[] = {
	{ "Monkey Academy (USA)(1984)(Coleco - Konami).rom",	20480, 0xd92b09c8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_monkey, cv_monkey, cv_coleco)
STD_ROM_FN(cv_monkey)

struct BurnDriver BurnDrvcv_monkey = {
	"cv_monkey", NULL, "cv_coleco", NULL, "1984",
	"Monkey Academy (USA)\0", NULL, "Coleco - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_PLATFORM | GBF_PUZZLE, 0,
	CVGetZipName, cv_monkeyRomInfo, cv_monkeyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Monkey Academy (USA, Prototype)
static struct BurnRomInfo cv_monkeypRomDesc[] = {
	{ "Monkey Academy (USA, Proto)(1984)(Konami).rom",	16384, 0xd1e8df85, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_monkeyp, cv_monkeyp, cv_coleco)
STD_ROM_FN(cv_monkeyp)

struct BurnDriver BurnDrvcv_monkeyp = {
	"cv_monkeyp", "cv_monkey", "cv_coleco", NULL, "1984",
	"Monkey Academy (USA, Prototype)\0", NULL, "Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_PLATFORM | GBF_PUZZLE, 0,
	CVGetZipName, cv_monkeypRomInfo, cv_monkeypRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Montezuma's Revenge (USA)
static struct BurnRomInfo cv_montezumRomDesc[] = {
	{ "Montezuma's Revenge (USA)(1984)(Parker Brothers).rom",	12288, 0xced0d16e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_montezum, cv_montezum, cv_coleco)
STD_ROM_FN(cv_montezum)

struct BurnDriver BurnDrvcv_montezum = {
	"cv_montezum", NULL, "cv_coleco", NULL, "1984",
	"Montezuma's Revenge (USA)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_montezumRomInfo, cv_montezumRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Montezuma's Revenge (USA, Alt)
static struct BurnRomInfo cv_montezumaRomDesc[] = {
	{ "Montezuma's Revenge (USA, Alt)(1984)(Parker Brothers).rom",	16384, 0x0bd9d073, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_montezuma, cv_montezuma, cv_coleco)
STD_ROM_FN(cv_montezuma)

struct BurnDriver BurnDrvcv_montezuma = {
	"cv_montezuma", "cv_montezum", "cv_coleco", NULL, "1984",
	"Montezuma's Revenge (USA, Alt)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_montezumaRomInfo, cv_montezumaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Moon Patrol (USA)
static struct BurnRomInfo cv_mpatrolRomDesc[] = {
    { "Moon Patrol (USA)(1984)(Atarisoft).rom",	32768, 0x6a672443, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mpatrol, cv_mpatrol, cv_coleco)
STD_ROM_FN(cv_mpatrol)

struct BurnDriver BurnDrvcv_mpatrol = {
    "cv_mpatrol", NULL, "cv_coleco", NULL, "1984",
    "Moon Patrol (USA)\0", NULL, "Atarisoft", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
    CVGetZipName, cv_mpatrolRomInfo, cv_mpatrolRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Matt Patrol (USA, Prototype)
static struct BurnRomInfo cv_mpatrolpRomDesc[] = {
    { "Matt Patrol (USA, Proto)(1984)(Selma).rom",	32768, 0xfbe12de7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mpatrolp, cv_mpatrolp, cv_coleco)
STD_ROM_FN(cv_mpatrolp)

struct BurnDriver BurnDrvcv_mpatrolp = {
    "cv_mpatrolp", "cv_mpatrol", "cv_coleco", NULL, "1984",
    "Matt Patrol (USA, Prototype)\0", NULL, "Selma", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
    CVGetZipName, cv_mpatrolpRomInfo, cv_mpatrolpRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Moonsweeper (USA, Euro)
static struct BurnRomInfo cv_moonswprRomDesc[] = {
	{ "Moonsweeper (USA, Euro)(1983)(Imagic).rom",	16384, 0x8a303f5a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_moonswpr, cv_moonswpr, cv_coleco)
STD_ROM_FN(cv_moonswpr)

struct BurnDriver BurnDrvcv_moonswpr = {
	"cv_moonswpr", NULL, "cv_coleco", NULL, "1983",
	"Moonsweeper (USA, Euro)\0", NULL, "Imagic", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_moonswprRomInfo, cv_moonswprRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Moonsweeper (USA, Alt)
static struct BurnRomInfo cv_moonswpraRomDesc[] = {
	{ "Moonsweeper (USA, Alt)(1983)(Imagic).rom",	16384, 0xc521bcad, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_moonswpra, cv_moonswpra, cv_coleco)
STD_ROM_FN(cv_moonswpra)

struct BurnDriver BurnDrvcv_moonswpra = {
	"cv_moonswpra", "cv_moonswpr", "cv_coleco", NULL, "1983",
	"Moonsweeper (USA, Alt)\0", NULL, "Imagic", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_moonswpraRomInfo, cv_moonswpraRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Motocross Racer (USA)
static struct BurnRomInfo cv_mtcracerRomDesc[] = {
	{ "Motocross Racer (USA)(1984)(Xonox).rom",	16384, 0x935ebe62, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mtcracer, cv_mtcracer, cv_coleco)
STD_ROM_FN(cv_mtcracer)

struct BurnDriver BurnDrvcv_mtcracer = {
	"cv_mtcracer", NULL, "cv_coleco", NULL, "1984",
	"Motocross Racer (USA)\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_RACING, 0,
	CVGetZipName, cv_mtcracerRomInfo, cv_mtcracerRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Motocross Racer (USA, Alt)
static struct BurnRomInfo cv_mtcraceraRomDesc[] = {
	{ "Motocross Racer (USA, Alt)(1984)(Xonox).rom", 16384, 0x0f4d800a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mtcracera, cv_mtcracera, cv_coleco)
STD_ROM_FN(cv_mtcracera)

struct BurnDriver BurnDrvcv_mtcracera = {
	"cv_mtcracera", "cv_mtcracer", "cv_coleco", NULL, "1984",
	"Motocross Racer (USA, Alt)\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_COLECO, GBF_RACING, 0,
	CVGetZipName, cv_mtcraceraRomInfo, cv_mtcraceraRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mountain King (USA)
static struct BurnRomInfo cv_mkingRomDesc[] = {
	{ "Mountain King (USA)(1984)(Sunrise Software).rom",	16384, 0xd8d73246, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mking, cv_mking, cv_coleco)
STD_ROM_FN(cv_mking)

struct BurnDriver BurnDrvcv_mking = {
	"cv_mking", NULL, "cv_coleco", NULL, "1984",
	"Mountain King (USA)\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_mkingRomInfo, cv_mkingRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mountain King (USA, Alt)
static struct BurnRomInfo cv_mkingaRomDesc[] = {
	{ "Mountain King (USA, Alt)(1984)(Sunrise Software).rom",	16384, 0xc173bbec, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mkinga, cv_mkinga, cv_coleco)
STD_ROM_FN(cv_mkinga)

struct BurnDriver BurnDrvcv_mkinga = {
	"cv_mkinga", "cv_mking", "cv_coleco", NULL, "1984",
	"Mountain King (USA, Alt)\0", NULL, "Sunrise Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_mkingaRomInfo, cv_mkingaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mousetrap (USA)
static struct BurnRomInfo cv_mtrapRomDesc[] = {
	{ "Mousetrap (USA)(1982)(Coleco).rom",	12288, 0xde47c29f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mtrap, cv_mtrap, cv_coleco)
STD_ROM_FN(cv_mtrap)

struct BurnDriver BurnDrvcv_mtrap = {
	"cv_mtrap", NULL, "cv_coleco", NULL, "1982",
	"Mousetrap (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_mtrapRomInfo, cv_mtrapRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mousetrap (USA, Alt)
static struct BurnRomInfo cv_mtrapaRomDesc[] = {
	{ "Mousetrap (USA, Alt)(1982)(Coleco).rom",	16384, 0xfd8b79b3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mtrapa, cv_mtrapa, cv_coleco)
STD_ROM_FN(cv_mtrapa)

struct BurnDriver BurnDrvcv_mtrapa = {
	"cv_mtrapa", "cv_mtrap", "cv_coleco", NULL, "1982",
	"Mousetrap (USA, Alt)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_mtrapaRomInfo, cv_mtrapaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mr. Do! (USA, Euro)
static struct BurnRomInfo cv_mrdoRomDesc[] = {
	{ "Mr. Do! (USA, Euro)(1983)(Coleco).rom",	24576, 0x461ab6ad, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mrdo, cv_mrdo, cv_coleco)
STD_ROM_FN(cv_mrdo)

struct BurnDriver BurnDrvcv_mrdo = {
	"cv_mrdo", NULL, "cv_coleco", NULL, "1983",
	"Mr. Do! (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_mrdoRomInfo, cv_mrdoRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mr. Do! (USA, Alt)
static struct BurnRomInfo cv_mrdoaRomDesc[] = {
	{ "Mr. Do! (USA, Alt)(1983)(Coleco).rom",	24576, 0xd53e4bdf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mrdoa, cv_mrdoa, cv_coleco)
STD_ROM_FN(cv_mrdoa)

struct BurnDriver BurnDrvcv_mrdoa = {
	"cv_mrdoa", "cv_mrdo", "cv_coleco", NULL, "1983",
	"Mr. Do! (USA, Alt)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_mrdoaRomInfo, cv_mrdoaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mr. Do!'s Castle (USA)
static struct BurnRomInfo cv_docastleRomDesc[] = {
	{ "Mr. Do!'s Castle (USA)(1983)(Parker Brothers).rom",	12288, 0x546f2c54, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_docastle, cv_docastle, cv_coleco)
STD_ROM_FN(cv_docastle)

struct BurnDriver BurnDrvcv_docastle = {
	"cv_docastle", NULL, "cv_coleco", NULL, "1983",
	"Mr. Do!'s Castle (USA)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_docastleRomInfo, cv_docastleRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mr. Do!'s Castle (USA, Alt)
static struct BurnRomInfo cv_docastleaRomDesc[] = {
	{ "Mr. Do!'s Castle (USA, Alt)(1983)(Parker Brothers).rom",	16384, 0xef7e9562, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_docastlea, cv_docastlea, cv_coleco)
STD_ROM_FN(cv_docastlea)

struct BurnDriver BurnDrvcv_docastlea = {
	"cv_docastlea", "cv_docastle", "cv_coleco", NULL, "1983",
	"Mr. Do!'s Castle (USA, Alt)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_docastleaRomInfo, cv_docastleaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Oil's Well (USA)
static struct BurnRomInfo cv_oilswellRomDesc[] = {
	{ "Oil's Well (USA)(1984)(Sierra On-Line).rom",	16384, 0xadd10242, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_oilswell, cv_oilswell, cv_coleco)
STD_ROM_FN(cv_oilswell)

struct BurnDriver BurnDrvcv_oilswell = {
	"cv_oilswell", NULL, "cv_coleco", NULL, "1984",
	"Oil's Well (USA)\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_oilswellRomInfo, cv_oilswellRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Oil's Well (USA, Alt)
static struct BurnRomInfo cv_oilswellaRomDesc[] = {
	{ "Oil's Well (USA, Alt)(1984)(Sierra On-Line).rom",	16384, 0xd2256b6c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_oilswella, cv_oilswella, cv_coleco)
STD_ROM_FN(cv_oilswella)

struct BurnDriver BurnDrvcv_oilswella = {
	"cv_oilswella", "cv_oilswell", "cv_coleco", NULL, "1984",
	"Oil's Well (USA, Alt)\0", NULL, "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_oilswellaRomInfo, cv_oilswellaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Omega Race (USA, Euro)
static struct BurnRomInfo cv_omegraceRomDesc[] = {
	{ "Omega Race (USA, Euro)(1983)(Coleco).rom",	16384, 0x0a0511c7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_omegrace, cv_omegrace, cv_coleco)
STD_ROM_FN(cv_omegrace)

struct BurnDriver BurnDrvcv_omegrace = {
	"cv_omegrace", NULL, "cv_coleco", NULL, "1983",
	"Omega Race (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_omegraceRomInfo, cv_omegraceRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// One on One (USA)
static struct BurnRomInfo cv_1on1RomDesc[] = {
	{ "One on One (USA)(1984)(Micro Fun).rom",	24576, 0x17c77c3a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_1on1, cv_1on1, cv_coleco)
STD_ROM_FN(cv_1on1)

struct BurnDriver BurnDrvcv_1on1 = {
	"cv_1on1", NULL, "cv_coleco", NULL, "1984",
	"One on One (USA)\0", NULL, "Micro Fun", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SPORTSMISC, 0,
	CVGetZipName, cv_1on1RomInfo, cv_1on1RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Pac-Man (USA, Prototype)
static struct BurnRomInfo cv_pacmanRomDesc[] = {
	{ "Pac-Man (USA, Proto)(1983)(Atarisoft).rom",	16384, 0xa40a07e8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pacman, cv_pacman, cv_coleco)
STD_ROM_FN(cv_pacman)

struct BurnDriver BurnDrvcv_pacman = {
	"cv_pacman", NULL, "cv_coleco", NULL, "1983",
	"Pac-Man (USA, Prototype)\0", NULL, "Atarisoft", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_pacmanRomInfo, cv_pacmanRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Pitfall! (USA)
static struct BurnRomInfo cv_pitfallRomDesc[] = {
	{ "Pitfall! (USA)(1983)(Activision).rom",	16384, 0xd4f180df, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pitfall, cv_pitfall, cv_coleco)
STD_ROM_FN(cv_pitfall)

struct BurnDriver BurnDrvcv_pitfall = {
	"cv_pitfall", NULL, "cv_coleco", NULL, "1983",
	"Pitfall! (USA)\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_pitfallRomInfo, cv_pitfallRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Pitfall II: Lost Caverns (USA)
static struct BurnRomInfo cv_pitfall2RomDesc[] = {
	{ "Pitfall II - Lost Caverns (USA)(1984)(Activision).rom",	16384, 0x851a455f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pitfall2, cv_pitfall2, cv_coleco)
STD_ROM_FN(cv_pitfall2)

struct BurnDriver BurnDrvcv_pitfall2 = {
	"cv_pitfall2", NULL, "cv_coleco", NULL, "1984",
	"Pitfall II: Lost Caverns (USA)\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_pitfall2RomInfo, cv_pitfall2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Pitstop (USA)
static struct BurnRomInfo cv_pitstopRomDesc[] = {
	{ "Pitstop (USA)(1983)(Epyx).rom",	16384, 0xe627fcf0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pitstop, cv_pitstop, cv_coleco)
STD_ROM_FN(cv_pitstop)

struct BurnDriver BurnDrvcv_pitstop = {
	"cv_pitstop", NULL, "cv_coleco", NULL, "1983",
	"Pitstop (USA)\0", NULL, "Epyx", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_RACING, 0,
	CVGetZipName, cv_pitstopRomInfo, cv_pitstopRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Pitstop (USA, Euro)
static struct BurnRomInfo cv_pitstopaRomDesc[] = {
	{ "Pitstop (USA, Euro)(1983)(Epyx).rom",	16384, 0x81be4f55, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pitstopa, cv_pitstopa, cv_coleco)
STD_ROM_FN(cv_pitstopa)

struct BurnDriver BurnDrvcv_pitstopa = {
	"cv_pitstopa", "cv_pitstop", "cv_coleco", NULL, "1983",
	"Pitstop (USA, Euro)\0", NULL, "Epyx", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_COLECO, GBF_RACING, 0,
	CVGetZipName, cv_pitstopaRomInfo, cv_pitstopaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Popeye (USA)
static struct BurnRomInfo cv_popeyeRomDesc[] = {
	{ "Popeye (USA)(1983)(Parker Brothers).rom",	16384, 0xa095454d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_popeye, cv_popeye, cv_coleco)
STD_ROM_FN(cv_popeye)

struct BurnDriver BurnDrvcv_popeye = {
	"cv_popeye", NULL, "cv_coleco", NULL, "1983",
	"Popeye (USA)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_popeyeRomInfo, cv_popeyeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Popeye (USA, Alt)
static struct BurnRomInfo cv_popeyeaRomDesc[] = {
	{ "Popeye (USA, Alt)(1983)(Parker Brothers).rom",	16384, 0x3ef1d0ee, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_popeyea, cv_popeyea, cv_coleco)
STD_ROM_FN(cv_popeyea)

struct BurnDriver BurnDrvcv_popeyea = {
	"cv_popeyea", "cv_popeye", "cv_coleco", NULL, "1983",
	"Popeye (USA, Alt)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_popeyeaRomInfo, cv_popeyeaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Porky's (USA, Prototype)
static struct BurnRomInfo cv_porkysRomDesc[] = {
	{ "Porky's (USA, Proto)(1983)(Fox Video Games).rom",	16384, 0x46fc1c00, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_porkys, cv_porkys, cv_coleco)
STD_ROM_FN(cv_porkys)

struct BurnDriver BurnDrvcv_porkys = {
	"cv_porkys", NULL, "cv_coleco", NULL, "1983",
	"Porky's (USA, Prototype)\0", NULL, "Fox Video Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_porkysRomInfo, cv_porkysRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Q*bert (USA)
static struct BurnRomInfo cv_qbertRomDesc[] = {
	{ "Qbert (USA)(1983)(Parker Brothers).rom",	8192, 0x532f61ba, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_qbert, cv_qbert, cv_coleco)
STD_ROM_FN(cv_qbert)

struct BurnDriver BurnDrvcv_qbert = {
	"cv_qbert", NULL, "cv_coleco", NULL, "1983",
	"Q*bert (USA)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_qbertRomInfo, cv_qbertRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Q*bert (USA, Alt)
static struct BurnRomInfo cv_qbertaRomDesc[] = {
	{ "Qbert (USA, Alt)(1983)(Parker Brothers).rom",	8192, 0x13f06adc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_qberta, cv_qberta, cv_coleco)
STD_ROM_FN(cv_qberta)

struct BurnDriver BurnDrvcv_qberta = {
	"cv_qberta", "cv_qbert", "cv_coleco", NULL, "1983",
	"Q*bert (USA, Alt)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_qbertaRomInfo, cv_qbertaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Q*bert Qubes (USA)
static struct BurnRomInfo cv_qbertqubRomDesc[] = {
	{ "Qbert's Qubes (USA)(1984)(Parker Brothers).rom",	12288, 0x79d90d26, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_qbertqub, cv_qbertqub, cv_coleco)
STD_ROM_FN(cv_qbertqub)

struct BurnDriver BurnDrvcv_qbertqub = {
	"cv_qbertqub", NULL, "cv_coleco", NULL, "1984",
	"Q*bert Qubes (USA)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_qbertqubRomInfo, cv_qbertqubRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// River Raid (USA)
static struct BurnRomInfo cv_riveraidRomDesc[] = {
	{ "River Raid (USA)(1984)(Activision).rom",	16384, 0xfe897f9c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_riveraid, cv_riveraid, cv_coleco)
STD_ROM_FN(cv_riveraid)

struct BurnDriver BurnDrvcv_riveraid = {
	"cv_riveraid", NULL, "cv_coleco", NULL, "1984",
	"River Raid (USA)\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_riveraidRomInfo, cv_riveraidRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Robin Hood (USA)
static struct BurnRomInfo cv_robinhRomDesc[] = {
	{ "Robin Hood (USA)(1984)(Xonox).rom",	16384, 0xcdb8d3cf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_robinh, cv_robinh, cv_coleco)
STD_ROM_FN(cv_robinh)

struct BurnDriver BurnDrvcv_robinh = {
	"cv_robinh", NULL, "cv_coleco", NULL, "1984",
	"Robin Hood (USA)\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_RUNGUN, 0,
	CVGetZipName, cv_robinhRomInfo, cv_robinhRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Robin Hood (USA, Alt)
static struct BurnRomInfo cv_robinhaRomDesc[] = {
	{ "Robin Hood (USA, Alt)(1984)(Xonox).rom",	16384, 0x0eb45d6e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_robinha, cv_robinha, cv_coleco)
STD_ROM_FN(cv_robinha)

struct BurnDriver BurnDrvcv_robinha = {
	"cv_robinha", "cv_robinh", "cv_coleco", NULL, "1984",
	"Robin Hood (USA, Alt)\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_RUNGUN, 0,
	CVGetZipName, cv_robinhaRomInfo, cv_robinhaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Rock n' Bolt (USA)
static struct BurnRomInfo cv_rockboltRomDesc[] = {
	{ "Rock n' Bolt (USA)(1984)(Telegames).rom",	16384, 0x522b59b9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rockbolt, cv_rockbolt, cv_coleco)
STD_ROM_FN(cv_rockbolt)

struct BurnDriver BurnDrvcv_rockbolt = {
	"cv_rockbolt", NULL, "cv_coleco", NULL, "1984",
	"Rock n' Bolt (USA)\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_rockboltRomInfo, cv_rockboltRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Rock n' Bolt (USA, Alt)
static struct BurnRomInfo cv_rockboltaRomDesc[] = {
	{ "Rock n' Bolt (USA, Alt)(1984)(Telegames).rom",	16384, 0xaf49a0c3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rockbolta, cv_rockbolta, cv_coleco)
STD_ROM_FN(cv_rockbolta)

struct BurnDriver BurnDrvcv_rockbolta = {
	"cv_rockbolta", "cv_rockbolt", "cv_coleco", NULL, "1984",
	"Rock n' Bolt (USA, Alt)\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_rockboltaRomInfo, cv_rockboltaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Roc 'n Rope (USA, Euro)
static struct BurnRomInfo cv_rocnropeRomDesc[] = {
	{ "Roc 'n Rope (USA, Euro)(1984)(Coleco - Konami).rom",	24576, 0xaf885d76, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rocnrope, cv_rocnrope, cv_coleco)
STD_ROM_FN(cv_rocnrope)

struct BurnDriver BurnDrvcv_rocnrope = {
	"cv_rocnrope", NULL, "cv_coleco", NULL, "1984",
	"Roc 'n Rope (USA, Euro)\0", NULL, "Coleco - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_rocnropeRomInfo, cv_rocnropeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Sir Lancelot (USA)
static struct BurnRomInfo cv_lancelotRomDesc[] = {
	{ "Sir Lancelot (USA)(1983)(Xonox).rom",	16384, 0xdf84ffb5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_lancelot, cv_lancelot, cv_coleco)
STD_ROM_FN(cv_lancelot)

struct BurnDriver BurnDrvcv_lancelot = {
	"cv_lancelot", NULL, "cv_coleco", NULL, "1983",
	"Sir Lancelot (USA)\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_lancelotRomInfo, cv_lancelotRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Skiing (USA)
static struct BurnRomInfo cv_skiingRomDesc[] = {
	{ "Skiing (USA)(1986)(Telegames).rom",	8192, 0xb37d48bd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_skiing, cv_skiing, cv_coleco)
STD_ROM_FN(cv_skiing)

struct BurnDriver BurnDrvcv_skiing = {
	"cv_skiing", NULL, "cv_coleco", NULL, "1986",
	"Skiing (USA)\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_COLECO, GBF_SPORTSMISC, 0,
	CVGetZipName, cv_skiingRomInfo, cv_skiingRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Slither (USA, Euro)
static struct BurnRomInfo cv_slitherRomDesc[] = {
	{ "Slither (USA, Euro)(1983)(Coleco).rom",	16384, 0x53d2651c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_slither, cv_slither, cv_coleco)
STD_ROM_FN(cv_slither)

struct BurnDriver BurnDrvcv_slither = {
	"cv_slither", NULL, "cv_coleco", NULL, "1983",
	"Slither (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_slitherRomInfo, cv_slitherRomName, NULL, NULL, NULL, NULL, ROLLERInputInfo, ROLLERDIPInfo,
	DrvInitROLLER, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Slurpy (USA)
static struct BurnRomInfo cv_slurpyRomDesc[] = {
	{ "Slurpy (USA)(1984)(Xonox).rom",	24576, 0x27f5c0ad, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_slurpy, cv_slurpy, cv_coleco)
STD_ROM_FN(cv_slurpy)

struct BurnDriver BurnDrvcv_slurpy = {
	"cv_slurpy", NULL, "cv_coleco", NULL, "1984",
	"Slurpy (USA)\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_slurpyRomInfo, cv_slurpyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Smurf: Rescue in Gargamel's Castle (USA, Euro)
static struct BurnRomInfo cv_smurfRomDesc[] = {
	{ "Smurf - Rescue in Gargamel's Castle (USA, Euro)(1982)(Coleco).rom",	16384, 0x42850379, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_smurf, cv_smurf, cv_coleco)
STD_ROM_FN(cv_smurf)

struct BurnDriver BurnDrvcv_smurf = {
	"cv_smurf", NULL, "cv_coleco", NULL, "1982",
	"Smurf: Rescue in Gargamel's Castle (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_smurfRomInfo, cv_smurfRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Smurf: Rescue in Gargamel's Castle (USA, Alt)
static struct BurnRomInfo cv_smurfaRomDesc[] = {
	{ "Smurf - Rescue in Gargamel's Castle (USA, Alt)(1982)(Coleco).rom",	16384, 0xd1a1fe0b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_smurfa, cv_smurfa, cv_coleco)
STD_ROM_FN(cv_smurfa)

struct BurnDriver BurnDrvcv_smurfa = {
	"cv_smurfa", "cv_smurf", "cv_coleco", NULL, "1982",
	"Smurf: Rescue in Gargamel's Castle (USA, Alt)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_smurfaRomInfo, cv_smurfaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Spy Hunter (USA)
static struct BurnRomInfo cv_spyhuntRomDesc[] = {
	{ "Spy Hunter (USA)(1984)(Coleco).rom",	32768, 0x36478923, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spyhunt, cv_spyhunt, cv_coleco)
STD_ROM_FN(cv_spyhunt)

struct BurnDriver BurnDrvcv_spyhunt = {
	"cv_spyhunt", NULL, "cv_coleco", NULL, "1984",
	"Spy Hunter (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION | GBF_VERSHOOT, 0,
	CVGetZipName, cv_spyhuntRomInfo, cv_spyhuntRomName, NULL, NULL, NULL, NULL, SACInputInfo, SACDIPInfo,
	DrvInitSAC, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Spy Hunter (USA, Prototype)
static struct BurnRomInfo cv_spyhuntpRomDesc[] = {
	{ "Spy Hunter (USA, Proto)(1984)(Coleco).rom",	32768, 0x1969b9b7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spyhuntp, cv_spyhuntp, cv_coleco)
STD_ROM_FN(cv_spyhuntp)

struct BurnDriver BurnDrvcv_spyhuntp = {
	"cv_spyhuntp", "cv_spyhunt", "cv_coleco", NULL, "1984",
	"Spy Hunter (USA, Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION | GBF_VERSHOOT, 0,
	CVGetZipName, cv_spyhuntpRomInfo, cv_spyhuntpRomName, NULL, NULL, NULL, NULL, SACInputInfo, SACDIPInfo,
	DrvInitSAC, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Star Wars: The Arcade Game (USA)
static struct BurnRomInfo cv_starwarsRomDesc[] = {
	{ "Star Wars - The Arcade Game (USA)(1984)(Parker Brothers).rom",	16384, 0x0e75b3bf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_starwars, cv_starwars, cv_coleco)
STD_ROM_FN(cv_starwars)

struct BurnDriver BurnDrvcv_starwars = {
	"cv_starwars", NULL, "cv_coleco", NULL, "1984",
	"Star Wars: The Arcade Game (USA)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_starwarsRomInfo, cv_starwarsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Star Wars: The Arcade Game (USA, Prototype)
static struct BurnRomInfo cv_starwarspRomDesc[] = {
	{ "Star Wars - The Arcade Game (USA, Proto)(1984)(Parker Brothers).rom",	16384, 0xd7febc67, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_starwarsp, cv_starwarsp, cv_coleco)
STD_ROM_FN(cv_starwarsp)

struct BurnDriver BurnDrvcv_starwarsp = {
	"cv_starwarsp", "cv_starwars", "cv_coleco", NULL, "1984",
	"Star Wars: The Arcade Game (USA, Prototype)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_starwarspRomInfo, cv_starwarspRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Subroc (USA, Euro)
static struct BurnRomInfo cv_subrocRomDesc[] = {
	{ "Subroc (USA, Euro)(1983)(Coleco).rom",	20480, 0xa417f25f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_subroc, cv_subroc, cv_coleco)
STD_ROM_FN(cv_subroc)

struct BurnDriver BurnDrvcv_subroc = {
	"cv_subroc", NULL, "cv_coleco", NULL, "1983",
	"Subroc (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_subrocRomInfo, cv_subrocRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Super Action Baseball (USA)
static struct BurnRomInfo cv_sabasebRomDesc[] = {
	{ "Super Action Baseball (USA)(1983)(Coleco).rom",	20480, 0xb3bcf3f9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sabaseb, cv_sabaseb, cv_coleco)
STD_ROM_FN(cv_sabaseb)

struct BurnDriver BurnDrvcv_sabaseb = {
	"cv_sabaseb", NULL, "cv_coleco", NULL, "1983",
	"Super Action Baseball (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SPORTSMISC, 0,
	CVGetZipName, cv_sabasebRomInfo, cv_sabasebRomName, NULL, NULL, NULL, NULL, SACInputInfo, SACDIPInfo,
	DrvInitSAC, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Super Action Football (USA)
static struct BurnRomInfo cv_safootbRomDesc[] = {
	{ "Super Action Football (USA)(1984)(Coleco).rom",	32768, 0x62619dc0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_safootb, cv_safootb, cv_coleco)
STD_ROM_FN(cv_safootb)

struct BurnDriver BurnDrvcv_safootb = {
	"cv_safootb", NULL, "cv_coleco", NULL, "1984",
	"Super Action Football (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SPORTSMISC, 0,
	CVGetZipName, cv_safootbRomInfo, cv_safootbRomName, NULL, NULL, NULL, NULL, SACInputInfo, SACDIPInfo,
	DrvInitSAC, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Super Action Football (Euro)
static struct BurnRomInfo cv_saftsoccRomDesc[] = {
	{ "Super Action Football (Euro)(1984)(Coleco).rom",	32768, 0x4d6c0458, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_saftsocc, cv_saftsocc, cv_coleco)
STD_ROM_FN(cv_saftsocc)

struct BurnDriver BurnDrvcv_saftsocc = {
	"cv_saftsocc", "cv_sasoccer", "cv_coleco", NULL, "1984",
	"Super Action Football (Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_COLECO, GBF_SPORTSFOOTBALL, 0,
	CVGetZipName, cv_saftsoccRomInfo, cv_saftsoccRomName, NULL, NULL, NULL, NULL, SACInputInfo, SACDIPInfo,
	DrvInitSAC, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Super Action Soccer (USA)
static struct BurnRomInfo cv_sasoccerRomDesc[] = {
	{ "Super Action Soccer (USA)(1984)(Coleco).rom",	32768, 0xacac69dd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sasoccer, cv_sasoccer, cv_coleco)
STD_ROM_FN(cv_sasoccer)

struct BurnDriver BurnDrvcv_sasoccer = {
	"cv_sasoccer", NULL, "cv_coleco", NULL, "1984",
	"Super Action Soccer (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SPORTSFOOTBALL, 0,
	CVGetZipName, cv_sasoccerRomInfo, cv_sasoccerRomName, NULL, NULL, NULL, NULL, SACInputInfo, SACDIPInfo,
	DrvInitSAC, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Super Cobra (USA)
static struct BurnRomInfo cv_scobraRomDesc[] = {
	{ "Super Cobra (USA)(1983)(Parker Brothers).rom",	8192, 0x6cb5cb8f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_scobra, cv_scobra, cv_coleco)
STD_ROM_FN(cv_scobra)

struct BurnDriver BurnDrvcv_scobra = {
	"cv_scobra", NULL, "cv_coleco", NULL, "1983",
	"Super Cobra (USA)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_scobraRomInfo, cv_scobraRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Super Cobra (USA, Alt)
static struct BurnRomInfo cv_scobraaRomDesc[] = {
	{ "Super Cobra (USA, Alt)(1983)(Parker Brothers).rom",	8192, 0xf84622d2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_scobraa, cv_scobraa, cv_coleco)
STD_ROM_FN(cv_scobraa)

struct BurnDriver BurnDrvcv_scobraa = {
	"cv_scobraa", "cv_scobra", "cv_coleco", NULL, "1983",
	"Super Cobra (USA, Alt)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_scobraaRomInfo, cv_scobraaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Tank Wars (Euro)
static struct BurnRomInfo cv_tankwarsRomDesc[] = {
	{ "Tank Wars (Euro)(1983)(Coleco).rom",	12288, 0xaf6bbc7e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tankwars, cv_tankwars, cv_coleco)
STD_ROM_FN(cv_tankwars)

struct BurnDriver BurnDrvcv_tankwars = {
	"cv_tankwars", NULL, "cv_coleco", NULL, "1983",
	"Tank Wars (Euro)\0", NULL, "Telegames", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
	CVGetZipName, cv_tankwarsRomInfo, cv_tankwarsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Tapper (USA)
static struct BurnRomInfo cv_tapperRomDesc[] = {
	{ "Tapper (USA)(1984)(Coleco).rom",	32768, 0x8afd7db2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tapper, cv_tapper, cv_coleco)
STD_ROM_FN(cv_tapper)

struct BurnDriver BurnDrvcv_tapper = {
	"cv_tapper", NULL, "cv_coleco", NULL, "1984",
	"Tapper (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_tapperRomInfo, cv_tapperRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Tarzan: from out of the Jungle (USA, Euro)
static struct BurnRomInfo cv_tarzanRomDesc[] = {
	{ "Tarzan (USA, Euro)(1982)(Coleco).rom",	24576, 0x7cd7a702, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tarzan, cv_tarzan, cv_coleco)
STD_ROM_FN(cv_tarzan)

struct BurnDriver BurnDrvcv_tarzan = {
	"cv_tarzan", NULL, "cv_coleco", NULL, "1982",
	"Tarzan: from out of the Jungle (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_SCRFIGHT, 0,
	CVGetZipName, cv_tarzanRomInfo, cv_tarzanRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Time Pilot (USA, Euro)
static struct BurnRomInfo cv_timepltRomDesc[] = {
	{ "Time Pilot (USA, Euro)(1983)(Coleco - Konami).rom",	16384, 0xb3a1eacb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_timeplt, cv_timeplt, cv_coleco)
STD_ROM_FN(cv_timeplt)

struct BurnDriver BurnDrvcv_timeplt = {
	"cv_timeplt", NULL, "cv_coleco", NULL, "1983",
	"Time Pilot (USA, Euro)\0", NULL, "Coleco - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_timepltRomInfo, cv_timepltRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Tomarc the Barbarian (USA)
static struct BurnRomInfo cv_tomarcRomDesc[] = {
	{ "Tomarc the Barbarian (USA)(1984)(Xonox).rom",	16384, 0x1e14397e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tomarc, cv_tomarc, cv_coleco)
STD_ROM_FN(cv_tomarc)

struct BurnDriver BurnDrvcv_tomarc = {
	"cv_tomarc", NULL, "cv_coleco", NULL, "1984",
	"Tomarc the Barbarian (USA)\0", NULL, "Xonox", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_tomarcRomInfo, cv_tomarcRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Tournament Tennis (USA)
static struct BurnRomInfo cv_ttennisRomDesc[] = {
	{ "Tournament Tennis (USA)(1984)(Coleco).rom",	16384, 0xc1d5a702, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ttennis, cv_ttennis, cv_coleco)
STD_ROM_FN(cv_ttennis)

struct BurnDriver BurnDrvcv_ttennis = {
	"cv_ttennis", NULL, "cv_coleco", NULL, "1984",
	"Tournament Tennis (USA)\0", NULL, "Coleco - D&L Research Imagic", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_SPORTSMISC, 0,
	CVGetZipName, cv_ttennisRomInfo, cv_ttennisRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Turbo (USA, Euro)
static struct BurnRomInfo cv_turboRomDesc[] = {
	{ "Turbo (USA, Europe)(1982)(Coleco).rom",	16384, 0xbd6ab02a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_turbo, cv_turbo, cv_coleco)
STD_ROM_FN(cv_turbo)

struct BurnDriver BurnDrvcv_turbo = {
	"cv_turbo", NULL, "cv_coleco", NULL, "1982",
	"Turbo (USA, Euro)\0", "NB: select options with Controller #2", "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_RACING, 0,
	CVGetZipName, cv_turboRomInfo, cv_turboRomName, NULL, NULL, NULL, NULL, ROLLERInputInfo, ROLLERDIPInfo,
	DrvInitROLLER, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Tutankham (USA)
static struct BurnRomInfo cv_tutankhmRomDesc[] = {
	{ "Tutankham (USA)(1983)(Parker Brothers).rom",	12288, 0x0408f58c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tutankhm, cv_tutankhm, cv_coleco)
STD_ROM_FN(cv_tutankhm)

struct BurnDriver BurnDrvcv_tutankhm = {
	"cv_tutankhm", NULL, "cv_coleco", NULL, "1983",
	"Tutankham (USA)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
	CVGetZipName, cv_tutankhmRomInfo, cv_tutankhmRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Tutankham (USA, Alt)
static struct BurnRomInfo cv_tutankhmaRomDesc[] = {
	{ "Tutankham (USA, Alt)(1983)(Parker Brothers).rom",	16384, 0x10cc33a3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tutankhma, cv_tutankhma, cv_coleco)
STD_ROM_FN(cv_tutankhma)

struct BurnDriver BurnDrvcv_tutankhma = {
	"cv_tutankhma", "cv_tutankhm", "cv_coleco", NULL, "1983",
	"Tutankham (USA, Alt)\0", NULL, "Parker Brothers", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
	CVGetZipName, cv_tutankhmaRomInfo, cv_tutankhmaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Up'n Down (USA)
static struct BurnRomInfo cv_upndownRomDesc[] = {
	{ "Up'n Down (USA)(1984)(Sega).rom",	16384, 0xfdd52ca0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_upndown, cv_upndown, cv_coleco)
STD_ROM_FN(cv_upndown)

struct BurnDriver BurnDrvcv_upndown = {
	"cv_upndown", NULL, "cv_coleco", NULL, "1984",
	"Up'n Down (USA)\0", NULL, "Sega", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_upndownRomInfo, cv_upndownRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Venture (USA, Euro)
static struct BurnRomInfo cv_ventureRomDesc[] = {
	{ "Venture (USA, Euro)(1982)(Coleco).rom",	16384, 0x8e5a4aa3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_venture, cv_venture, cv_coleco)
STD_ROM_FN(cv_venture)

struct BurnDriver BurnDrvcv_venture = {
	"cv_venture", NULL, "cv_coleco", NULL, "1982",
	"Venture (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
	CVGetZipName, cv_ventureRomInfo, cv_ventureRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Victory (USA)
static struct BurnRomInfo cv_victoryRomDesc[] = {
	{ "Victory (USA)(1983)(Coleco).rom",	20480, 0x70142655, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_victory, cv_victory, cv_coleco)
STD_ROM_FN(cv_victory)

struct BurnDriver BurnDrvcv_victory = {
	"cv_victory", NULL, "cv_coleco", NULL, "1983",
	"Victory (USA)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_victoryRomInfo, cv_victoryRomName, NULL, NULL, NULL, NULL, ROLLERInputInfo, ROLLERDIPInfo,
	DrvInitROLLER, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Video Hustler (USA, Prototype 1)
static struct BurnRomInfo cv_hustlerRomDesc[] = {
	{ "Video Hustler (USA, Proto 1)(1984)(Coleco).rom",	8192, 0xa9177b20, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_hustler, cv_hustler, cv_coleco)
STD_ROM_FN(cv_hustler)

struct BurnDriver BurnDrvcv_hustler = {
	"cv_hustler", NULL, "cv_coleco", NULL, "1984",
	"Video Hustler (USA, Prototype 1)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_SPORTSMISC, 0,
	CVGetZipName, cv_hustlerRomInfo, cv_hustlerRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Video Hustler (USA, Prototype 2)
static struct BurnRomInfo cv_hustler1RomDesc[] = {
	{ "Video Hustler (USA, Proto 2)(1984)(Konami).rom",	8192, 0x841d168f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_hustler1, cv_hustler1, cv_coleco)
STD_ROM_FN(cv_hustler1)

struct BurnDriver BurnDrvcv_hustler1 = {
	"cv_hustler1", "cv_hustler", "cv_coleco", NULL, "1984",
	"Video Hustler (USA, Prototype 2)\0", NULL, "Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_SPORTSMISC, 0,
	CVGetZipName, cv_hustler1RomInfo, cv_hustler1RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// War Games (USA, Euro)
static struct BurnRomInfo cv_wargamesRomDesc[] = {
	{ "War Games (USA, Euro)(1984)(Coleco).rom",	24576, 0x6e0150c1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wargames, cv_wargames, cv_coleco)
STD_ROM_FN(cv_wargames)

struct BurnDriver BurnDrvcv_wargames = {
	"cv_wargames", NULL, "cv_coleco", NULL, "1984",
	"War Games (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_STRATEGY, 0,
	CVGetZipName, cv_wargamesRomInfo, cv_wargamesRomName, NULL, NULL, NULL, NULL, ROLLERInputInfo, ROLLERDIPInfo,
	DrvInitROLLER, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// War Games (USA, Prototype)
static struct BurnRomInfo cv_wargamespRomDesc[] = {
	{ "War Games (USA, Proto)(1984)(Coleco).rom",	24576, 0xfd25adb3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wargamesp, cv_wargamesp, cv_coleco)
STD_ROM_FN(cv_wargamesp)

struct BurnDriver BurnDrvcv_wargamesp = {
	"cv_wargamesp", "cv_wargames", "cv_coleco", NULL, "1984",
	"War Games (USA, Prototype)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_STRATEGY, 0,
	CVGetZipName, cv_wargamespRomInfo, cv_wargamespRomName, NULL, NULL, NULL, NULL, ROLLERInputInfo, ROLLERDIPInfo,
	DrvInitROLLER, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// WarRoom (USA)
static struct BurnRomInfo cv_warroomRomDesc[] = {
	{ "WarRoom (USA)(1983)(NAP Consumer Electronics).rom",	24576, 0x261b7d56, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_warroom, cv_warroom, cv_coleco)
STD_ROM_FN(cv_warroom)

struct BurnDriver BurnDrvcv_warroom = {
	"cv_warroom", NULL, "cv_coleco", NULL, "1983",
	"WarRoom (USA)\0", NULL, "NAP Consumer Electronics", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_warroomRomInfo, cv_warroomRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Wing War (USA)
static struct BurnRomInfo cv_wingwarRomDesc[] = {
	{ "Wing War (USA)(1983)(Imagic).rom",	16384, 0x29ec00c9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wingwar, cv_wingwar, cv_coleco)
STD_ROM_FN(cv_wingwar)

struct BurnDriver BurnDrvcv_wingwar = {
	"cv_wingwar", NULL, "cv_coleco", NULL, "1983",
	"Wing War (USA)\0", NULL, "Imagic", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_wingwarRomInfo, cv_wingwarRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Word Feud (USA)
static struct BurnRomInfo cv_wordfeudRomDesc[] = {
	{ "Word Feud (USA)(1984)(K-Tel).rom",	8192, 0xf7a29c01, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wordfeud, cv_wordfeud, cv_coleco)
STD_ROM_FN(cv_wordfeud)

struct BurnDriver BurnDrvcv_wordfeud = {
	"cv_wordfeud", NULL, "cv_coleco", NULL, "1984",
	"Word Feud (USA)\0", NULL, "K-Tel", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_wordfeudRomInfo, cv_wordfeudRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Yolks on You, The (Euro, Prototype)
static struct BurnRomInfo cv_yolkRomDesc[] = {
	{ "Yolk's on you, The (Euro, Proto)(1983)(Fox Video Games).rom",	16384, 0x997ad8de, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_yolk, cv_yolk, cv_coleco)
STD_ROM_FN(cv_yolk)

struct BurnDriver BurnDrvcv_yolk = {
	"cv_yolk", NULL, "cv_coleco", NULL, "1983",
	"Yolks on You, The (Euro, Prototype)\0", NULL, "Fox Video Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_yolkRomInfo, cv_yolkRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Zaxxon (USA, Euro)
static struct BurnRomInfo cv_zaxxonRomDesc[] = {
	{ "Zaxxon (USA, Euro)(1982)(Coleco).rom",	24576, 0x8cb0891a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zaxxon, cv_zaxxon, cv_coleco)
STD_ROM_FN(cv_zaxxon)

struct BurnDriver BurnDrvcv_zaxxon = {
	"cv_zaxxon", NULL, "cv_coleco", NULL, "1982",
	"Zaxxon (USA, Euro)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_zaxxonRomInfo, cv_zaxxonRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Zenji (USA)
static struct BurnRomInfo cv_zenjiRomDesc[] = {
	{ "Zenji (USA)(1984)(Activision).rom",	16384, 0x6e523e50, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zenji, cv_zenji, cv_coleco)
STD_ROM_FN(cv_zenji)

struct BurnDriver BurnDrvcv_zenji = {
	"cv_zenji", NULL, "cv_coleco", NULL, "1984",
	"Zenji (USA)\0", NULL, "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_zenjiRomInfo, cv_zenjiRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// CV Joystick Test
static struct BurnRomInfo cv_cvjoytestRomDesc[] = {
	{ "Super Action Controller Test Cartridge (1983)(Nuvatec).rom",	0x004000, 0xeb6cb4d8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cvjoytest, cv_cvjoytest, cv_coleco)
STD_ROM_FN(cv_cvjoytest)

struct BurnDriver BurnDrvcv_cvjoytest = {
	"cv_cvjoytest", NULL, "cv_coleco", NULL, "1983",
	"ColecoVision Joystick Test\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_cvjoytestRomInfo, cv_cvjoytestRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// CV SGM Test
static struct BurnRomInfo cv_sgmtestRomDesc[] = {
	{ "ColecoVision SuperGame Module (200x).rom",	0x002000, 0xabc4763d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sgmtest, cv_sgmtest, cv_coleco)
STD_ROM_FN(cv_sgmtest)

struct BurnDriver BurnDrvcv_sgmtest = {
	"cv_sgmtest", NULL, "cv_coleco", NULL, "200x",
	"ColecoVision SuperGame Module (SGM) Test\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_sgmtestRomInfo, cv_sgmtestRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};


// -------------------------------
// Aftermarket/Hack/Homebrew Games
// -------------------------------


// 421 (HB)
static struct BurnRomInfo cv_421RomDesc[] = {
    { "421 (2002)(Mathieu Proulx).rom",	0x6400, 0xb6254fc5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_421, cv_421, cv_coleco)
STD_ROM_FN(cv_421)

struct BurnDriver BurnDrvcv_421 = {
    "cv_421", NULL, "cv_coleco", NULL, "2002",
    "421 (HB)\0", NULL, "Mathieu Proulx", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_CASINO, 0,
    CVGetZipName, cv_421RomInfo, cv_421RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// 1942 (SGM) (HB)
static struct BurnRomInfo cv_1942sgmRomDesc[] = {
    { "1942 SGM (2020)(Team Pixelboy).rom",	131072, 0x851efe57, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_1942sgm, cv_1942sgm, cv_coleco)
STD_ROM_FN(cv_1942sgm)

struct BurnDriver BurnDrvcv_1942sgm = {
    "cv_1942sgm", NULL, "cv_coleco", NULL, "1985-2020",
    "1942 (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Capcom", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_1942sgmRomInfo, cv_1942sgmRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// A.E.:Anti-Environment Encounter (HB)
static struct BurnRomInfo cv_aeRomDesc[] = {
    { "AE (2012)(Team Pixelboy).rom",	0x8000, 0x85f2c3ff, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ae, cv_ae, cv_coleco)
STD_ROM_FN(cv_ae)

struct BurnDriver BurnDrvcv_ae = {
    "cv_ae", NULL, "cv_coleco", NULL, "1984-2012",
    "A.E.:Anti-Environment Encounter (HB)\0", "Published by Team Pixelboy", "Broderbund Software", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_aeRomInfo, cv_aeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Aerial (HB)
static struct BurnRomInfo cv_aerialRomDesc[] = {
	{ "Aerial (2022)(Inufuto).rom",	10614, 0x2d9d8e6f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_aerial, cv_aerial, cv_coleco)
STD_ROM_FN(cv_aerial)

struct BurnDriver BurnDrvcv_aerial = {
	"cv_aerial", NULL, "cv_coleco", NULL, "2022",
	"Aerial (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_aerialRomInfo, cv_aerialRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Adventure Island (SGM) (HB)
static struct BurnRomInfo cv_advislandRomDesc[] = {
    { "Adventure Island SGM (20XX)(unknown).rom",	131072, 0x91636750, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_advisland, cv_advisland, cv_coleco)
STD_ROM_FN(cv_advisland)

struct BurnDriver BurnDrvcv_advisland = {
    "cv_advisland", NULL, "cv_coleco", NULL, "20??",
    "Adventure Island (SGM) (HB)\0", "SGM - Super Game Module", "<unknown>", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_advislandRomInfo, cv_advislandRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Alpharoid (SGM) (HB)
static struct BurnRomInfo cv_alpharoidRomDesc[] = {
    { "Alpharoid SGM (2018)(Opcode Games).rom",	131072, 0x6068db13, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_alpharoid, cv_alpharoid, cv_coleco)
STD_ROM_FN(cv_alpharoid)

struct BurnDriver BurnDrvcv_alpharoid = {
    "cv_alpharoid", NULL, "cv_coleco", NULL, "1986-2018",
    "Alpharoid (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Pony Canyon", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
    CVGetZipName, cv_alpharoidRomInfo, cv_alpharoidRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Amazing Snake (HB, v0.9)
static struct BurnRomInfo cv_amzsnakeRomDesc[] = {
	{ "Amazing Snake v0.9 (2001)(Serge-Eric Tremblay).rom",	31744, 0x5ba1a6c8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_amzsnake, cv_amzsnake, cv_coleco)
STD_ROM_FN(cv_amzsnake)

struct BurnDriver BurnDrvcv_amzsnake = {
	"cv_amzsnake", NULL, "cv_coleco", NULL, "2001",
	"Amazing Snake (HB, v0.9)\0", NULL, "Serge-Eric Tremblay", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_amzsnakeRomInfo, cv_amzsnakeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// AntiAir (HB)
static struct BurnRomInfo cv_antiairRomDesc[] = {
	{ "AntiAir (2024)(Inufuto).rom",	7941, 0x8fabe383, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_antiair, cv_antiair, cv_coleco)
STD_ROM_FN(cv_antiair)

struct BurnDriver BurnDrvcv_antiair = {
	"cv_antiair", NULL, "cv_coleco", NULL, "2024",
	"AntiAir (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_antiairRomInfo, cv_antiairRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Arabian (HB)
static struct BurnRomInfo cv_arabianRomDesc[] = {
    { "Arabian (1983-2022)(Team Pixelboy).rom",	32768, 0xaeb8d084, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_arabian, cv_arabian, cv_coleco)
STD_ROM_FN(cv_arabian)

struct BurnDriver BurnDrvcv_arabian = {
    "cv_arabian", NULL, "cv_coleco", NULL, "1983-2022",
    "Arabian (HB)\0", "Published by Pixelboy", "Sun Electronics Corp.", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_arabianRomInfo, cv_arabianRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Arkadion (HB)
static struct BurnRomInfo cv_ArkadionRomDesc[] = {
	{ "Arkadion (2022)(8 bit Milli Games).rom", 32768, 0xd876a9b6, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Arkadion, cv_Arkadion, cv_coleco)
STD_ROM_FN(cv_Arkadion)

struct BurnDriver BurnDrvcv_Arkadion = {
	"cv_arkadion", NULL, "cv_coleco", NULL, "2022",
	"Arkadion (HB)\0", NULL, "8 bit Milli Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_ArkadionRomInfo, cv_ArkadionRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Arkanoid (SGM) (HB)
static struct BurnRomInfo cv_arkanoidRomDesc[] = {
    { "Arkanoid SGM (2020)(CollectorVision).rom",	32768, 0x28b61e06, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_arkanoid, cv_arkanoid, cv_coleco)
STD_ROM_FN(cv_arkanoid)

struct BurnDriver BurnDrvcv_arkanoid = {
    "cv_arkanoid", NULL, "cv_coleco", NULL, "1986-2020",
    "Arkanoid (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Taito", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_BREAKOUT, 0,
    CVGetZipName, cv_arkanoidRomInfo, cv_arkanoidRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Armageddon (HB)
static struct BurnRomInfo cv_armagednRomDesc[] = {
    { "Armageddon (2011)(CollectorVision).rom",	0x08000, 0x55147097, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_armagedn, cv_armagedn, cv_coleco)
STD_ROM_FN(cv_armagedn)

struct BurnDriver BurnDrvcv_armagedn = {
    "cv_armagedn", NULL, "cv_coleco", NULL, "2011",
    "Armageddon (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_SHOOT, 0,
    CVGetZipName, cv_armagednRomInfo, cv_armagednRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Arno Dash (HB)
static struct BurnRomInfo cv_arnodashRomDesc[] = {
	{ "Arno Dash (2021)(Under4Mhz).rom",	32768, 0xead5e824, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_arnodash, cv_arnodash, cv_coleco)
STD_ROM_FN(cv_arnodash)

struct BurnDriver BurnDrvcv_arnodash = {
	"cv_arnodash", NULL, "cv_coleco", NULL, "2021",
	"Arno Dash (HB)\0", NULL, "Under4Mhz", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_arnodashRomInfo, cv_arnodashRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Ascend (HB)
static struct BurnRomInfo cv_ascendRomDesc[] = {
	{ "Ascend (2022)(Inufuto).rom",	9371, 0xc7f84f5f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ascend, cv_ascend, cv_coleco)
STD_ROM_FN(cv_ascend)

struct BurnDriver BurnDrvcv_ascend = {
	"cv_ascend", NULL, "cv_coleco", NULL, "2022",
	"Ascend (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_ascendRomInfo, cv_ascendRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Asteroids (SGM) (HB)
static struct BurnRomInfo cv_AsteroidsRomDesc[] = {
	{ "Asteroids (2021)(Team Pixelboy).rom", 32768, 0x94bcf1d1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Asteroids, cv_Asteroids, cv_coleco)
STD_ROM_FN(cv_Asteroids)

struct BurnDriver BurnDrvcv_Asteroids = {
	"cv_asteroids", NULL, "cv_coleco", NULL, "2021",
	"Asteroids (SGM) (HB)\0", "SGM - Super Game Module", "Team Pixelboy", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_AsteroidsRomInfo, cv_AsteroidsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Astrododge (HB)
static struct BurnRomInfo cv_astrododgeRomDesc[] = {
    { "Astrododge (2012)(Revival Studios).rom",	0x8000, 0x1a29dfd1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_astrododge, cv_astrododge, cv_coleco)
STD_ROM_FN(cv_astrododge)

struct BurnDriver BurnDrvcv_astrododge = {
    "cv_astrododge", NULL, "cv_coleco", NULL, "2012",
    "Astrododge (HB)\0", NULL, "Revival Studios", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_astrododgeRomInfo, cv_astrododgeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Astro Invader (HB)
static struct BurnRomInfo cv_astroinvRomDesc[] = {
    { "Astro Invader (2005)(Scott Huggins).rom",	0x8000, 0x70ccc945, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_astroinv, cv_astroinv, cv_coleco)
STD_ROM_FN(cv_astroinv)

struct BurnDriver BurnDrvcv_astroinv = {
    "cv_astroinv", NULL, "cv_coleco", NULL, "1980-2005",
    "Astro Invader (HB)\0", NULL, "Scott Huggins - Stern", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_astroinvRomInfo, cv_astroinvRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Astrostorm (HB)
static struct BurnRomInfo cv_astormRomDesc[] = {
	{ "Astrostorm (2021)(AnalogKid).rom",	28787, 0x3b27ed05, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_astorm, cv_astorm, cv_coleco)
STD_ROM_FN(cv_astorm)

struct BurnDriver BurnDrvcv_astorm = {
	"cv_astorm", NULL, "cv_coleco", NULL, "2021",
	"Astrostorm (HB)\0", NULL, "AlalogKid", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_astormRomInfo, cv_astormRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Aztec Challenge (HB)
static struct BurnRomInfo cv_AztecchalRomDesc[] = {
	{ "Aztec Challenge (2024)(Dragonfly Amusement).rom", 32768, 0xb931416a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Aztecchal, cv_Aztecchal, cv_coleco)
STD_ROM_FN(cv_Aztecchal)

struct BurnDriver BurnDrvcv_Aztecchal = {
	"cv_aztecchal", NULL, "cv_coleco", NULL, "2024",
	"Aztec Challenge (HB)\0", NULL, "Dragonfly Amusement", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_AztecchalRomInfo, cv_AztecchalRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bagman (HB)
static struct BurnRomInfo cv_bagmanRomDesc[] = {
	{ "Bagman (2015)(CollectorVision).rom",	32767, 0x673bdb9d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bagman, cv_bagman, cv_coleco)
STD_ROM_FN(cv_bagman)

struct BurnDriver BurnDrvcv_bagman = {
	"cv_bagman", NULL, "cv_coleco", NULL, "1982-2015",
	"Bagman (HB)\0", "Remake by Alekmaul", "CollectorVision Games - Stern", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_bagmanRomInfo, cv_bagmanRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bank Panic (HB)
static struct BurnRomInfo cv_bankpanicRomDesc[] = {
    { "Bank Panic (2011)(Team Pixelboy).rom",	0x8000, 0x223e7ddc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bankpanic, cv_bankpanic, cv_coleco)
STD_ROM_FN(cv_bankpanic)

struct BurnDriver BurnDrvcv_bankpanic = {
    "cv_bankpanic", NULL, "cv_coleco", NULL, "2011",
    "Bank Panic (HB)\0", "Published by Team Pixelboy", "Eduardo Mello - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
    CVGetZipName, cv_bankpanicRomInfo, cv_bankpanicRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Bankruptcy Builder (HB)
static struct BurnRomInfo cv_bankbuildRomDesc[] = {
    { "Bankruptcy Builder (2013)(Good Deal Games).rom",	0x8000, 0xb153c849, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bankbuild, cv_bankbuild, cv_coleco)
STD_ROM_FN(cv_bankbuild)

struct BurnDriver BurnDrvcv_bankbuild = {
    "cv_bankbuild", NULL, "cv_coleco", NULL, "2013",
    "Bankruptcy Builder (HB)\0", NULL, "Good Deal Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_STRATEGY, 0,
    CVGetZipName, cv_bankbuildRomInfo, cv_bankbuildRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Battle of Hoth (HB)
static struct BurnRomInfo cv_bofhothRomDesc[] = {
    { "Battle of Hoth (2013)(Team Pixelboy).rom",	0x8000, 0xb2ef6386, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bofhoth, cv_bofhoth, cv_coleco)
STD_ROM_FN(cv_bofhoth)

struct BurnDriver BurnDrvcv_bofhoth = {
    "cv_bofhoth", NULL, "cv_coleco", NULL, "2013",
    "Battle of Hoth (HB)\0", NULL, "Team Pixelboy", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
    CVGetZipName, cv_bofhothRomInfo, cv_bofhothRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Beach Head (HB, v0.93)
static struct BurnRomInfo cv_beacheadRomDesc[] = {
	{ "Beach Head v0.93 (2025)(CollectorVision).rom",	131072, 0x86ab5d25, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_beachead, cv_beachead, cv_coleco)
STD_ROM_FN(cv_beachead)

struct BurnDriver BurnDrvcv_beachead = {
	"cv_beachead", NULL, "cv_coleco", NULL, "2025",
	"Beach Head (HB, v0.93)\0", "Published by CollectorVision Games", "Electric Adventures", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MINIGAMES | GBF_SHOOT, 0,
	CVGetZipName, cv_beacheadRomInfo, cv_beacheadRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Berzerk (HB)
static struct BurnRomInfo cv_berzerkRomDesc[] = {
	{ "Berzerk (2022)(CollectorVision).rom",	32768, 0x6328ffc1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_berzerk, cv_berzerk, cv_coleco)
STD_ROM_FN(cv_berzerk)

struct BurnDriver BurnDrvcv_berzerk = {
	"cv_berzerk", NULL, "cv_coleco", NULL, "2022",
	"Berzerk (HB)\0", "Published by CollectorVision Games", "Electric Adventures", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
	CVGetZipName, cv_berzerkRomInfo, cv_berzerkRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bejeweled (HB)
static struct BurnRomInfo cv_bejeweledRomDesc[] = {
    { "Bejeweled (2002)(Daniel Bienvenu).rom",	0x6480, 0x39c5a7e9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bejeweled, cv_bejeweled, cv_coleco)
STD_ROM_FN(cv_bejeweled)

struct BurnDriver BurnDrvcv_bejeweled = {
    "cv_bejeweled", NULL, "cv_coleco", NULL, "2002",
    "Bejeweled (HB)\0", NULL, "Daniel Bienvenu", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_bejeweledRomInfo, cv_bejeweledRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Black Onyx, The (HB)
static struct BurnRomInfo cv_blackonyxRomDesc[] = {
    { "Black Onyx, the (2013)(Team Pixelboy).rom",	0x10000, 0xdddd1396, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_blackonyx, cv_blackonyx, cv_coleco)
STD_ROM_FN(cv_blackonyx)

struct BurnDriver BurnDrvcv_blackonyx = {
    "cv_blackonyx", NULL, "cv_coleco", NULL, "1987-2013",
    "Black Onyx, The (HB)\0", "Published by Team Pixelboy", "Mystery Man - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MAZE | GBF_RPG, 0,
    CVGetZipName, cv_blackonyxRomInfo, cv_blackonyxRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitI2C, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Bloktris (HB, v1.03)
static struct BurnRomInfo cv_bloktrisRomDesc[] = {
	{ "Bloktris v1.03 (2023)(Under4Mhz).rom",	32768, 0xd1bc1d23, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bloktris, cv_bloktris, cv_coleco)
STD_ROM_FN(cv_bloktris)

struct BurnDriver BurnDrvcv_bloktris = {
	"cv_bloktris", NULL, "cv_coleco", NULL, "2023",
	"Bloktris (HB, v1.03)\0", NULL, "Under4Mhz", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_bloktrisRomInfo, cv_bloktrisRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Boggy '84 (SGM) (HB)
static struct BurnRomInfo cv_boggy84RomDesc[] = {
    { "Boggy '84 SGM (2017)(Opcode Games).rom",	32768, 0xef3473dd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_boggy84, cv_boggy84, cv_coleco)
STD_ROM_FN(cv_boggy84)

struct BurnDriver BurnDrvcv_boggy84 = {
    "cv_boggy84", NULL, "cv_coleco", NULL, "1984-2017",
    "Boggy '84 (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Colpax", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_boggy84RomInfo, cv_boggy84RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Bomb Jack (HB)
static struct BurnRomInfo cv_bombjackRomDesc[] = {
    { "Bomb Jack (2015)(CollectorVision).rom",	0x08000, 0x765d1f78, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bombjack, cv_bombjack, cv_coleco)
STD_ROM_FN(cv_bombjack)

struct BurnDriver BurnDrvcv_bombjack = {
    "cv_bombjack", NULL, "cv_coleco", NULL, "2015",
    "Bomb Jack (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_bombjackRomInfo, cv_bombjackRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Bomb'n Blast (HB)
static struct BurnRomInfo cv_bomblastRomDesc[] = {
    { "Bomb'n Blast (2011)(CollectorVision).rom",	0x08000, 0xb06d9de7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bomblast, cv_bomblast, cv_coleco)
STD_ROM_FN(cv_bomblast)

struct BurnDriver BurnDrvcv_bomblast = {
    "cv_bomblast", NULL, "cv_coleco", NULL, "2011",
    "Bomb'n Blast (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_bomblastRomInfo, cv_bomblastRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Bomb'n Blast 2 (HB)
static struct BurnRomInfo cv_bomblast2RomDesc[] = {
    { "Bomb'n Blast 2 (2020)(Cote Gamers).rom",	32768, 0x4218dbb8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bomblast2, cv_bomblast2, cv_coleco)
STD_ROM_FN(cv_bomblast2)

struct BurnDriver BurnDrvcv_bomblast2 = {
    "cv_bomblast2", NULL, "cv_coleco", NULL, "2020",
    "Bomb'n Blast 2 (HB)\0", NULL, "Cote Gamers", "ColecoVision",
    NULL, NULL, L"C\u00f4t\u00e9 Gamers", NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_bomblast2RomInfo, cv_bomblast2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Booming Boy (SGM) (HB)
static struct BurnRomInfo cv_boomingboyRomDesc[] = {
    { "Booming Boy SGM (2019)(Team Pixelboy).rom",	32768, 0x924a7d57, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_boomingboy, cv_boomingboy, cv_coleco)
STD_ROM_FN(cv_boomingboy)

struct BurnDriver BurnDrvcv_boomingboy = {
    "cv_boomingboy", NULL, "cv_coleco", NULL, "2019",
    "Booming Boy (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Armando Perez Abad & Nene Franz", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_boomingboyRomInfo, cv_boomingboyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Boot Hill (HB)
static struct BurnRomInfo cv_boothillRomDesc[] = {
	{ "Boot Hill (2018)(Leo Brophy).rom",	32768, 0x59e22170, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_boothill, cv_boothill, cv_coleco)
STD_ROM_FN(cv_boothill)

struct BurnDriver BurnDrvcv_boothill = {
	"cv_boothill", NULL, "cv_coleco", NULL, "2018",
	"Boot Hill (HB)\0", NULL, "Leo Brophy", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_boothillRomInfo, cv_boothillRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Bosconian (SGM) (HB)
static struct BurnRomInfo cv_bosconianRomDesc[] = {
    { "Bosconian SGM (2016)(Opcode Games).rom",	32768, 0x91326103, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bosconian, cv_bosconian, cv_coleco)
STD_ROM_FN(cv_bosconian)

struct BurnDriver BurnDrvcv_bosconian = {
    "cv_bosconian", NULL, "cv_coleco", NULL, "1984-2016",
    "Bosconian (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Namcot", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
    CVGetZipName, cv_bosconianRomInfo, cv_bosconianRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Boxxle (HB)
static struct BurnRomInfo cv_boxxleRomDesc[] = {
	{ "Boxxle (2015)(Team Pixelboy).rom",	0x10000, 0x62dacf07, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_boxxle, cv_boxxle, cv_coleco)
STD_ROM_FN(cv_boxxle)

struct BurnDriver BurnDrvcv_boxxle = {
	"cv_boxxle", NULL, "cv_coleco", NULL, "2015",
	"Boxxle (HB)\0", "Published by Team Pixelboy", "Thinking Rabbit", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_boxxleRomInfo, cv_boxxleRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitI2CBoxxle, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Breakout (HB)
static struct BurnRomInfo cv_breakoutRomDesc[] = {
    { "Breakout (1999) (Daniel Bienvenu).rom",	0x3580, 0xb05fb3ec, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_breakout, cv_breakout, cv_coleco)
STD_ROM_FN(cv_breakout)

struct BurnDriver BurnDrvcv_breakout = {
    "cv_breakout", NULL, "cv_coleco", NULL, "1999",
    "Breakout (HB)\0", NULL, "Daniel Bienvenu", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_BREAKOUT, 0,
    CVGetZipName, cv_breakoutRomInfo, cv_breakoutRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Buck Rogers (SGM) (HB)
static struct BurnRomInfo cv_buckrogsgmRomDesc[] = {
    { "Buck Rogers SGM (2013)(Team Pixelboy).rom",	0x40000, 0xc4f1a85a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_buckrogsgm, cv_buckrogsgm, cv_coleco)
STD_ROM_FN(cv_buckrogsgm)

struct BurnDriver BurnDrvcv_buckrogsgm = {
    "cv_buckrogsgm", NULL, "cv_coleco", NULL, "1983-2013",
    "Buck Rogers Super Game (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Coleco", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
    CVGetZipName, cv_buckrogsgmRomInfo, cv_buckrogsgmRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Bugs'n Bots (HB)
static struct BurnRomInfo cv_bugsbotsRomDesc[] = {
	{ "Bugs'n Bots (2012)(Nicam Shilova).rom",	32768, 0x542a5c61, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_bugsbots, cv_bugsbots, cv_coleco)
STD_ROM_FN(cv_bugsbots)

struct BurnDriver BurnDrvcv_bugsbots = {
	"cv_bugsbots", NULL, "cv_coleco", NULL, "2012",
	"Bugs'n Bots (HB)\0", NULL, "Nicam Shilova", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_bugsbotsRomInfo, cv_bugsbotsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Burn Rubber (HB)
static struct BurnRomInfo cv_brubberRomDesc[] = {
	{ "Burn Rubber (2010)(Dvik & Joyrex).rom",	0x08000, 0xb278ebde, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_brubber, cv_brubber, cv_coleco)
STD_ROM_FN(cv_brubber)

struct BurnDriver BurnDrvcv_brubber = {
	"cv_brubber", NULL, "cv_coleco", NULL, "2010",
	"Burn Rubber (HB)\0", "Published by CollectorVision Games", "Dvik & Joyrex", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_RACING, 0,
	CVGetZipName, cv_brubberRomInfo, cv_brubberRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cacorm (HB)
static struct BurnRomInfo cv_cacormRomDesc[] = {
	{ "Cacorm (2022)(Inufuto).rom",	8423, 0x60000902, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cacorm, cv_cacorm, cv_coleco)
STD_ROM_FN(cv_cacorm)

struct BurnDriver BurnDrvcv_cacorm = {
	"cv_cacorm", NULL, "cv_coleco", NULL, "2022",
	"Cacorm (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_cacormRomInfo, cv_cacormRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Camelot Knights (HB)
static struct BurnRomInfo cv_camknightsRomDesc[] = {
    { "Camelot Knights (2024)(Nanochess).rom",	22589, 0xb551402c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_camknights, cv_camknights, cv_coleco)
STD_ROM_FN(cv_camknights)

struct BurnDriver BurnDrvcv_camknights = {
    "cv_camknights", NULL, "cv_coleco", NULL, "2024",
    "Camelot Knights (HB)\0", NULL, "Nanochess", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_camknightsRomInfo, cv_camknightsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Canadian Minigames (HB)
static struct BurnRomInfo cv_canaminiRomDesc[] = {
    { "Canadian Minigames (2008)(Wick-Foster-Bienvenu).rom",	0x8000, 0x99f3cbc9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_canamini, cv_canamini, cv_coleco)
STD_ROM_FN(cv_canamini)

struct BurnDriver BurnDrvcv_canamini = {
    "cv_canamini", NULL, "cv_coleco", NULL, "2008",
    "Canadian Minigames (HB)\0", NULL, "Wick-Foster-Bienvenu", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MINIGAMES, 0,
    CVGetZipName, cv_canaminiRomInfo, cv_canaminiRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Caos Begins (SGM) (HB)
static struct BurnRomInfo cv_caosbgnsRomDesc[] = {
    { "Caos Begins SGM (2015)(Team Pixelboy).rom",	0x08000, 0x53672097, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_caosbgns, cv_caosbgns, cv_coleco)
STD_ROM_FN(cv_caosbgns)

struct BurnDriver BurnDrvcv_caosbgns = {
    "cv_caosbgns", NULL, "cv_coleco", NULL, "2015",
    "Caos Begins (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Hikaru Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ADV | GBF_PLATFORM, 0,
    CVGetZipName, cv_caosbgnsRomInfo, cv_caosbgnsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Car Fighter (SGM) (HB)
static struct BurnRomInfo cv_carfightRomDesc[] = {
    { "Car Fighter SGM (2018)(CollectorVision).rom",	27340, 0x66357edf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_carfight, cv_carfight, cv_coleco)
STD_ROM_FN(cv_carfight)

struct BurnDriver BurnDrvcv_carfight = {
    "cv_carfight", NULL, "cv_coleco", NULL, "1985-2018",
    "Car Fighter (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Casio", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_RACING | GBF_VERSHOOT, 0,
    CVGetZipName, cv_carfightRomInfo, cv_carfightRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Castle Excellent (SGM) (HB)
static struct BurnRomInfo cv_castleexRomDesc[] = {
    { "Castle Excellent SGM (2018)(CollectorVision).rom",	32678, 0x674c0fec, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_castleex, cv_castleex, cv_coleco)
STD_ROM_FN(cv_castleex)

struct BurnDriver BurnDrvcv_castleex = {
    "cv_castleex", NULL, "cv_coleco", NULL, "1986-2018",
    "Castle Excellent (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "ASCII - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_castleexRomInfo, cv_castleexRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Castle, The (SGM) (HB)
static struct BurnRomInfo cv_castleRomDesc[] = {
    { "Castle, The SGM (2018)(CollectorVision).rom",	32399, 0x8aee473c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_castle, cv_castle, cv_coleco)
STD_ROM_FN(cv_castle)

struct BurnDriver BurnDrvcv_castle = {
    "cv_castle", "cv_castleex", "cv_coleco", NULL, "1986-2018",
    "Castle, The (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "ASCII - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_castleRomInfo, cv_castleRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Caterpillar S.O.S. (HB)
static struct BurnRomInfo cv_catsosRomDesc[] = {
    { "Cat SOS Game (2011)(CollectorVision).rom",	0x8000, 0x10e6e6de, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_catsos, cv_catsos, cv_coleco)
STD_ROM_FN(cv_catsos)

struct BurnDriver BurnDrvcv_catsos = {
    "cv_catsos", NULL, "cv_coleco", NULL, "1983-2011",
    "Caterpillar S.O.S. (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MISC, 0,
    CVGetZipName, cv_catsosRomInfo, cv_catsosRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Cavern Fighter (HB)
static struct BurnRomInfo cv_cavfightRomDesc[] = {
    { "Cavern Fighter (2021)(Electric Adventures).rom",	23680, 0x846faf14, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cavfight, cv_cavfight, cv_coleco)
STD_ROM_FN(cv_cavfight)

struct BurnDriver BurnDrvcv_cavfight = {
    "cv_cavfight", NULL, "cv_coleco", NULL, "2021",
    "Cavern Fighter (HB)\0", NULL, "Electric Adventures", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
    CVGetZipName, cv_cavfightRomInfo, cv_cavfightRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Caverns of Titan (SGM) (HB)
static struct BurnRomInfo cv_cavernsRomDesc[] = {
    { "Caverns of Titan SGM (2017)(Team Pixelboy).rom",	0x08000, 0x4928b5f5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_caverns, cv_caverns, cv_coleco)
STD_ROM_FN(cv_caverns)

struct BurnDriver BurnDrvcv_caverns = {
    "cv_caverns", NULL, "cv_coleco", NULL, "2005-2017",
    "Caverns of Titan (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Jltursan", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_cavernsRomInfo, cv_cavernsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Chack'n Pop (HB)
static struct BurnRomInfo cv_chackpopRomDesc[] = {
	{ "Chack'n Pop (2011)(CollectorVision).rom",	32768, 0xc1366313, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_chackpop, cv_chackpop, cv_coleco)
STD_ROM_FN(cv_chackpop)

struct BurnDriver BurnDrvcv_chackpop = {
	"cv_chackpop", NULL, "cv_coleco", NULL, "1985-2011",
	"Chack'n Pop (HB)\0", "Published by CollectorVision Games", "Sega", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_chackpopRomInfo, cv_chackpopRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Champion Pro Wrestling (HB)
static struct BurnRomInfo cv_chprowresRomDesc[] = {
    { "Champion Pro Wrestling (2020)(Team Pixelboy).rom",	32768, 0x045c54d6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_chprowres, cv_chprowres, cv_coleco)
STD_ROM_FN(cv_chprowres)

struct BurnDriver BurnDrvcv_chprowres = {
    "cv_chprowres", NULL, "cv_coleco", NULL, "1985-2020",
    "Champion Pro Wrestling (HB)\0", "Published by Team Pixelboy", "Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VSFIGHT, 0,
    CVGetZipName, cv_chprowresRomInfo, cv_chprowresRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Chess Challenge (HB)
static struct BurnRomInfo cv_chesschalRomDesc[] = {
    { "Chess Challenger (2011)(CollectorVision).rom",	32768, 0x998c1bbd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_chesschal, cv_chesschal, cv_coleco)
STD_ROM_FN(cv_chesschal)

struct BurnDriver BurnDrvcv_chesschal = {
    "cv_chesschal", NULL, "cv_coleco", NULL, "2011",
    "Chess Challenge (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_BOARD | GBF_STRATEGY, 0,
    CVGetZipName, cv_chesschalRomInfo, cv_chesschalRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Children of the Night (SGM) (HB)
static struct BurnRomInfo cv_cotnRomDesc[] = {
    { "Children of the Night SGM (2017)(Team Pixelboy).rom",	0x20000, 0x55b36d53, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cotn, cv_cotn, cv_coleco)
STD_ROM_FN(cv_cotn)

struct BurnDriver BurnDrvcv_cotn = {
    "cv_cotn", NULL, "cv_coleco", NULL, "2017",
    "Children of the Night (HB)\0", "SGM - Published by Team Pixelboy", "Hikaru Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ADV | GBF_MAZE, 0,
    CVGetZipName, cv_cotnRomInfo, cv_cotnRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Circus Charlie (HB)
static struct BurnRomInfo cv_ccharlieRomDesc[] = {
    { "Circus Charlie (2011)(Team Pixelboy).rom",	0x8000, 0xbe3ef785, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ccharlie, cv_ccharlie, cv_coleco)
STD_ROM_FN(cv_ccharlie)

struct BurnDriver BurnDrvcv_ccharlie = {
    "cv_ccharlie", NULL, "cv_coleco", NULL, "1984-2011",
    "Circus Charlie (HB)\0", "Published by Team Pixelboy", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_ccharlieRomInfo, cv_ccharlieRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Cix (HB)
static struct BurnRomInfo cv_cixRomDesc[] = {
    { "Cix (2013)(Good Deal Games).rom",	32768, 0xfea0da6d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cix, cv_cix, cv_coleco)
STD_ROM_FN(cv_cix)

struct BurnDriver BurnDrvcv_cix = {
    "cv_cix", NULL, "cv_coleco", NULL, "2013",
    "Cix (HB)\0", NULL, "Good Deal Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_cixRomInfo, cv_cixRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Clock (HB)
static struct BurnRomInfo cv_ClockRomDesc[] = {
	{ "Clock (2021)(Team Pixelboy).rom", 8192, 0xb2f28b90, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Clock, cv_Clock, cv_coleco)
STD_ROM_FN(cv_Clock)

struct BurnDriver BurnDrvcv_Clock = {
	"cv_clock", NULL, "cv_coleco", NULL, "2021",
	"Clock (HB)\0", NULL, "Team Pixelboy", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MISC, 0,
	CVGetZipName, cv_ClockRomInfo, cv_ClockRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// CoCo and the Evil Robots (HB, v1.01)
static struct BurnRomInfo cv_cocoRomDesc[] = {
	{ "CoCo and the Evil Robots v1.01 (2020)(Robin Gravel).rom",	16374, 0xd6b28d19, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_coco, cv_coco, cv_coleco)
STD_ROM_FN(cv_coco)

struct BurnDriver BurnDrvcv_coco = {
	"cv_coco", NULL, "cv_coleco", NULL, "2020",
	"CoCo and the Evil Robots (HB, v1.01)\0", NULL, "Robin Gravel", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MAZE, 0,
	CVGetZipName, cv_cocoRomInfo, cv_cocoRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cold Blood (SGM) (HB)
static struct BurnRomInfo cv_coldbloodRomDesc[] = {
	{ "Cold Blood SGM (2017)(Team Pixelboy).rom",	0x08000, 0x960f7086, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_coldblood, cv_coldblood, cv_coleco)
STD_ROM_FN(cv_coldblood)

struct BurnDriver BurnDrvcv_coldblood = {
	"cv_coldblood", NULL, "cv_coleco", NULL, "2017",
	"Cold Blood (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Paxanga", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_coldbloodRomInfo, cv_coldbloodRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Comic Bakery (HB)
static struct BurnRomInfo cv_comicbakeryRomDesc[] = {
	{ "Comic Bakery (2014)(CollectorVision).rom",	27925, 0x8884bac2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_comicbakery, cv_comicbakery, cv_coleco)
STD_ROM_FN(cv_comicbakery)

struct BurnDriver BurnDrvcv_comicbakery = {
	"cv_comicbakery", NULL, "cv_coleco", NULL, "1984-2014",
	"Comic Bakery (HB)\0", "Published by CollectorVision Games", "Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_comicbakeryRomInfo, cv_comicbakeryRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Computer Space (HB)
static struct BurnRomInfo cv_comspaceRomDesc[] = {
	{ "Computer Space (2014)(CollectorVision).rom",	32768, 0x4ae11870, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_comspace, cv_comspace, cv_coleco)
STD_ROM_FN(cv_comspace)

struct BurnDriver BurnDrvcv_comspace = {
	"cv_comspace", NULL, "cv_coleco", NULL, "2014",
	"Computer Space (HB)\0", "Published by CollectorVision Games", "Kiwi Production", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_comspaceRomInfo, cv_comspaceRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Cracky (HB)
static struct BurnRomInfo cv_crackyRomDesc[] = {
	{ "Cracky (2023)(Inufuto).rom",	7332, 0xbb13bc66, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cracky, cv_cracky, cv_coleco)
STD_ROM_FN(cv_cracky)

struct BurnDriver BurnDrvcv_cracky = {
	"cv_cracky", NULL, "cv_coleco", NULL, "2023",
	"Cracky (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_crackyRomInfo, cv_crackyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Crazy Chicky Junior (HB)
static struct BurnRomInfo cv_CrazychickyjrRomDesc[] = {
	{ "Crazy Chicky Junior (2023)(8 bit Milli Games).rom", 32768, 0x1bdca585, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Crazychickyjr, cv_Crazychickyjr, cv_coleco)
STD_ROM_FN(cv_Crazychickyjr)

struct BurnDriver BurnDrvcv_Crazychickyjr = {
	"cv_crazychickyjr", NULL, "cv_coleco", NULL, "2023",
	"Crazy Chicky Junior (HB)\0", NULL, "8 bit Milli Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_CrazychickyjrRomInfo, cv_CrazychickyjrRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Crazy Climber Redux (HB)
static struct BurnRomInfo cv_ccreduxRomDesc[] = {
	{ "Crazy Climber Redux (2021)(8bit Milli Games).rom",	32768, 0x2c630f28, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ccredux, cv_ccredux, cv_coleco)
STD_ROM_FN(cv_ccredux)

struct BurnDriver BurnDrvcv_ccredux = {
	"cv_ccredux", NULL, "cv_coleco", NULL, "2021",
	"Crazy Climber Redux (HB)\0", NULL, "8 bit Milli Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_ccreduxRomInfo, cv_ccreduxRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// C_So! (HB)
static struct BurnRomInfo cv_csoRomDesc[] = {
    { "C-So (2018)(Team Pixelboy).rom",	0x8000, 0xd182812a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_cso, cv_cso, cv_coleco)
STD_ROM_FN(cv_cso)

struct BurnDriver BurnDrvcv_cso = {
    "cv_cso", NULL, "cv_coleco", NULL, "1985-2018",
    "C_So! (HB)\0", "Published by Team Pixelboy", "Claus Baekkel - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_csoRomInfo, cv_csoRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Dacman (HB)
static struct BurnRomInfo cv_dacmanRomDesc[] = {
	{ "Dacman (2000)(Daniel Bienvenu).rom",	24576, 0x9a46ef8d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dacman, cv_dacman, cv_coleco)
STD_ROM_FN(cv_dacman)

struct BurnDriver BurnDrvcv_dacman = {
	"cv_dacman", NULL, "cv_coleco", NULL, "2000",
	"Dacman (HB)\0", NULL, "Daniel Bienvenu", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_dacmanRomInfo, cv_dacmanRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Danger Tower (SGM) (HB)
static struct BurnRomInfo cv_dangrtowerRomDesc[] = {
    { "Danger Tower SGM (2017)(Team Pixelboy).rom",	0x8000, 0x0246bdb1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dangrtower, cv_dangrtower, cv_coleco)
STD_ROM_FN(cv_dangrtower)

struct BurnDriver BurnDrvcv_dangrtower = {
    "cv_dangrtower", NULL, "cv_coleco", NULL, "2009-2017",
    "Danger Tower (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Danger Team", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_dangrtowerRomInfo, cv_dangrtowerRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Deep Dungeon Adventure (SGM) (HB)
static struct BurnRomInfo cv_deepdngadvRomDesc[] = {
    { "Deep Dungeon Adventure SGM (2017)(Team Pixelboy).rom",	0x20000, 0x77900970, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_deepdngadv, cv_deepdngadv, cv_coleco)
STD_ROM_FN(cv_deepdngadv)

struct BurnDriver BurnDrvcv_deepdngadv = {
    "cv_deepdngadv", NULL, "cv_coleco", NULL, "2017",
    "Deep Dungeon Adventure (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Trilobyte", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
    CVGetZipName, cv_deepdngadvRomInfo, cv_deepdngadvRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Deflektor Kollection (HB)
static struct BurnRomInfo cv_deflektorRomDesc[] = {
    { "Deflektor Kollection (2005)(Daniel Bienvenu).rom",	0x7C80, 0x1b3a8639, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_deflektor, cv_deflektor, cv_coleco)
STD_ROM_FN(cv_deflektor)

struct BurnDriver BurnDrvcv_deflektor = {
    "cv_deflektor", NULL, "cv_coleco", NULL, "2005",
    "Deflektor Kollection (HB)\0", "Published by Atariage", "Daniel Bienvenu", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_deflektorRomInfo, cv_deflektorRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Demon Attack & New Atlantis (HB, v1.2)
static struct BurnRomInfo cv_DemoattacknaRomDesc[] = {
	{ "Demon Attack & New Atlantis (2022)(8 bit Milli Games).rom", 32768, 0xe86fcfe5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Demoattackna, cv_Demoattackna, cv_coleco)
STD_ROM_FN(cv_Demoattackna)

struct BurnDriver BurnDrvcv_Demoattackna = {
	"cv_demoattackna", NULL, "cv_coleco", NULL, "2022",
	"Demon Attack & New Atlantis (HB, v1.2)\0", NULL, "8 bit Milli Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_DemoattacknaRomInfo, cv_DemoattacknaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Desolator (SGM) (HB)
static struct BurnRomInfo cv_desolatorRomDesc[] = {
    { "Desolator SGM (2023)(8 bit Milli Games).rom",	29287, 0xad01c8b1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_desolator, cv_desolator, cv_coleco)
STD_ROM_FN(cv_desolator)

struct BurnDriver BurnDrvcv_desolator = {
    "cv_desolator", NULL, "cv_coleco", NULL, "2023",
    "Desolator (SGM) (HB)\0", "SGM - Super Game Module", "8 bit Milli Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
    CVGetZipName, cv_desolatorRomInfo, cv_desolatorRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Destructor S.C.E. (Hack)
static struct BurnRomInfo cv_destructsceRomDesc[] = {
	{ "Destructor SCE (2010)(Team Pixelboy).rom",	32768, 0x2a9fcbfa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_destructsce, cv_destructsce, cv_coleco)
STD_ROM_FN(cv_destructsce)

struct BurnDriver BurnDrvcv_destructsce = {
	"cv_destructsce", "cv_destruct", "cv_coleco", NULL, "2010",
	"Destructor S.C.E. (Hack, Standard Controller Edition)\0", "Published by Team Pixelboy", "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_destructsceRomInfo, cv_destructsceRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Diamond Dash (HB)
static struct BurnRomInfo cv_ddashRomDesc[] = {
	{ "Diamond Dash (2011)(NewColeco).rom",	3858, 0xe386b1b7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ddash, cv_ddash, cv_coleco)
STD_ROM_FN(cv_ddash)

struct BurnDriver BurnDrvcv_ddash = {
	"cv_ddash", NULL, "cv_coleco", NULL, "2011",
	"Diamond Dash (HB)\0", NULL, "NewColeco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_ddashRomInfo, cv_ddashRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Diamond Dash 2 (HB)
static struct BurnRomInfo cv_ddash2RomDesc[] = {
	{ "Diamond Dash 2 (2021)(Under4Mhz).rom",	32768, 0x5576dec3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ddash2, cv_ddash2, cv_coleco)
STD_ROM_FN(cv_ddash2)

struct BurnDriver BurnDrvcv_ddash2 = {
	"cv_ddash2", NULL, "cv_coleco", NULL, "2021",
	"Diamond Dash 2 (HB)\0", NULL, "Under4Mhz", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_ddash2RomInfo, cv_ddash2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Digger (HB)
static struct BurnRomInfo cv_diggerRomDesc[] = {
	{ "Digger (2015)(CollectorVision).rom",	0x06000, 0x77088cab, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_digger, cv_digger, cv_coleco)
STD_ROM_FN(cv_digger)

struct BurnDriver BurnDrvcv_digger = {
	"cv_digger", NULL, "cv_coleco", NULL, "2015",
	"Digger (HB)\0", "Published by CollectorVision Games", "Windmill Software", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_diggerRomInfo, cv_diggerRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Donuts Man (HB)
static struct BurnRomInfo cv_donutsRomDesc[] = {
	{ "Donuts Man (2021)(Cote Gamers).rom",	32768, 0x517314b1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_donuts, cv_donuts, cv_coleco)
STD_ROM_FN(cv_donuts)

struct BurnDriver BurnDrvcv_donuts = {
	"cv_donuts", NULL, "cv_coleco", NULL, "2021",
	"Donuts Man (HB)\0", NULL, "Cote Gamers", "ColecoVision",
	NULL, NULL, L"C\u00f4t\u00e9 Gamers", NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_donutsRomInfo, cv_donutsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Donkey Kong (SGM) (HB)
static struct BurnRomInfo cv_dkongsgmRomDesc[] = {
    { "Donkey Kong SGM (2020)(Team Pixelboy).rom",	0x20000, 0xb3e62471, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dkongsgm, cv_dkongsgm, cv_coleco)
STD_ROM_FN(cv_dkongsgm)

struct BurnDriver BurnDrvcv_dkongsgm = {
    "cv_dkongsgm", NULL, "cv_coleco", NULL, "1984-2020",
    "Donkey Kong (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Nintendo / Coleco", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_dkongsgmRomInfo, cv_dkongsgmRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Donkey Kong 3 (HB)
static struct BurnRomInfo cv_dkong3RomDesc[] = {
	{ "Donkey Kong 3 (2012)(CollectorVision).rom",	131072, 0x3b434ec2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dkong3, cv_dkong3, cv_coleco)
STD_ROM_FN(cv_dkong3)

struct BurnDriver BurnDrvcv_dkong3 = {
	"cv_dkong3", NULL, "cv_coleco", NULL, "2012",
	"Donkey Kong 3 (HB)\0", "Press '/' to add coins, '1' for P1 mode, '2' for P2 mode", "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_dkong3RomInfo, cv_dkong3RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Donkey Kong Arcade (SGM) (HB)
static struct BurnRomInfo cv_dkongarcadeRomDesc[] = {
    { "Donkey Kong Arcade SGM (2018)(Opcode Games).rom",	131072, 0x45345709, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dkongarcade, cv_dkongarcade, cv_coleco)
STD_ROM_FN(cv_dkongarcade)

struct BurnDriver BurnDrvcv_dkongarcade = {
    "cv_dkongarcade", NULL, "cv_coleco", NULL, "2018",
    "Donkey Kong Arcade (SGM) (HB)\0", "Press '/' to add coins, '1' for P1 mode, '2' for P2 mode", "Opcode Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_dkongarcadeRomInfo, cv_dkongarcadeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Donkey Kong Jr. (SGM) (HB)
static struct BurnRomInfo cv_dkongjrsgmRomDesc[] = {
    { "Donkey Kong Jr SGM (2020)(Team Pixelboy).rom",	0x20000, 0x644124f6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dkongjrsgm, cv_dkongjrsgm, cv_coleco)
STD_ROM_FN(cv_dkongjrsgm)

struct BurnDriver BurnDrvcv_dkongjrsgm = {
    "cv_dkongjrsgm", NULL, "cv_coleco", NULL, "1983-2020",
    "Donkey Kong Jr. (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Nintendo / Coleco", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_dkongjrsgmRomInfo, cv_dkongjrsgmRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Dragon's Lair (SGM) (HB)
static struct BurnRomInfo cv_dlairsgmRomDesc[] = {
	{ "Dragon's Lair SGM (2012)(Team Pixelboy).rom",	0x040000, 0x12ceee08, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_dlairsgm, cv_dlairsgm, cv_coleco)
STD_ROM_FN(cv_dlairsgm)

struct BurnDriver BurnDrvcv_dlairsgm = {
	"cv_dlairsgm", NULL, "cv_coleco", NULL, "1984-2012",
	"Dragon's Lair (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_dlairsgmRomInfo, cv_dlairsgmRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Drol (HB)
static struct BurnRomInfo cv_drolRomDesc[] = {
	{ "Drol (2016)(CollectorVision Games).col",	32768, 0x8331d147, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_drol, cv_drol, cv_coleco)
STD_ROM_FN(cv_drol)

struct BurnDriver BurnDrvcv_drol = {
	"cv_drol", NULL, "cv_coleco", NULL, "1985-2016",
	"Drol (HB)\0", "Published by CollectorVision Games", "Sega", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_drolRomInfo, cv_drolRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Dungeons and Trolls (HB)
static struct BurnRomInfo cv_datrollsRomDesc[] = {
    { "Dungeons and Trolls (2014)(CollectorVision).rom",	30187, 0xa227fdaa, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_datrolls, cv_datrolls, cv_coleco)
STD_ROM_FN(cv_datrolls)

struct BurnDriver BurnDrvcv_datrolls = {
    "cv_datrolls", NULL, "cv_coleco", NULL, "2014",
    "Dungeons and Trolls (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MAZE | GBF_RPG, 0,
    CVGetZipName, cv_datrollsRomInfo, cv_datrollsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Earth Defend 2083 (HB)
static struct BurnRomInfo cv_ed2083RomDesc[] = {
	{ "Earth Defend 2083 (2013)(CollectorVision).rom",	32768, 0x6498372c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ed2083, cv_ed2083, cv_coleco)
STD_ROM_FN(cv_ed2083)

struct BurnDriver BurnDrvcv_ed2083 = {
	"cv_ed2083", NULL, "cv_coleco", NULL, "2013",
	"Earth Defend 2083 (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_ed2083RomInfo, cv_ed2083RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Easter Bunny (HB)
static struct BurnRomInfo cv_easterbunnyRomDesc[] = {
	{ "Easter Bunny (2007)(Daniel Bienvenu).rom",	5888, 0x5c494be5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_easterbunny, cv_easterbunny, cv_coleco)
STD_ROM_FN(cv_easterbunny)

struct BurnDriver BurnDrvcv_easterbunny = {
	"cv_easterbunny", NULL, "cv_coleco", NULL, "2007",
	"Easter Bunny (HB)\0", NULL, "Daniel Bienvenu", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_easterbunnyRomInfo, cv_easterbunnyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Eggerland Mystery (SGM) (HB)
static struct BurnRomInfo cv_eggerlandRomDesc[] = {
    { "Eggerland Mystery SGM (2018)(CollectorVision).rom",	131072, 0x2488ca1a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_eggerland, cv_eggerland, cv_coleco)
STD_ROM_FN(cv_eggerland)

struct BurnDriver BurnDrvcv_eggerland = {
    "cv_eggerland", NULL, "cv_coleco", NULL, "1985-2018",
    "Eggerland Mystery (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "HAL Laboratory", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_eggerlandRomInfo, cv_eggerlandRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Elevator Action (HB)
static struct BurnRomInfo cv_elevactionRomDesc[] = {
	{ "Elevator Action (2012)(CollectorVision).rom",	32768, 0x4bb8dc2a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_elevaction, cv_elevaction, cv_coleco)
STD_ROM_FN(cv_elevaction)

struct BurnDriver BurnDrvcv_elevaction = {
	"cv_elevaction", NULL, "cv_coleco", NULL, "1985-2012",
	"Elevator Action (HB)\0", "Published by CollectorVision Games", "Sega - Taito", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_elevactionRomInfo, cv_elevactionRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Elevator Action (SGM) (HB)
static struct BurnRomInfo cv_elevactionsgmRomDesc[] = {
	{ "Elevator Action SGM (2021)(Opcode Games).rom",	32768, 0xcf4bdc75, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_elevactionsgm, cv_elevactionsgm, cv_coleco)
STD_ROM_FN(cv_elevactionsgm)

struct BurnDriver BurnDrvcv_elevactionsgm = {
	"cv_elevactionsgm", NULL, "cv_coleco", NULL, "2021",
	"Elevator Action (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_elevactionsgmRomInfo, cv_elevactionsgmRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Flapee Byrd (HB)
static struct BurnRomInfo cv_flapbrdRomDesc[] = {
    { "Flapee Byrd (2014)(CollectorVision).rom",	0x7FF4, 0x5ac80811, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_flapbrd, cv_flapbrd, cv_coleco)
STD_ROM_FN(cv_flapbrd)

struct BurnDriver BurnDrvcv_flapbrd = {
    "cv_flapbrd", NULL, "cv_coleco", NULL, "2014",
    "Flapee Byrd (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_flapbrdRomInfo, cv_flapbrdRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Flicky (HB)
static struct BurnRomInfo cv_flickyRomDesc[] = {
    { "Flicky (2018)(Team Pixelboy).rom",	0x8000, 0x89875c52, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_flicky, cv_flicky, cv_coleco)
STD_ROM_FN(cv_flicky)

struct BurnDriver BurnDrvcv_flicky = {
    "cv_flicky", NULL, "cv_coleco", NULL, "1984-2018",
    "Flicky (HB)\0", "Published by Team Pixelboy", "Mystery Man - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_flickyRomInfo, cv_flickyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Flora and the Ghost Mirror (SGM) (HB)
static struct BurnRomInfo cv_floraRomDesc[] = {
    { "Flora and the Ghost Mirror SGM (2013)(NewColeco).rom",	0x8000, 0xfd69012b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_flora, cv_flora, cv_coleco)
STD_ROM_FN(cv_flora)

struct BurnDriver BurnDrvcv_flora = {
    "cv_flora", NULL, "cv_coleco", NULL, "2013",
    "Flora and the Ghost Mirror (SGM) (HB)\0", "SGM - Super Game Module", "NewColeco", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_floraRomInfo, cv_floraRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Frantic (HB)
static struct BurnRomInfo cv_franticRomDesc[] = {
    { "Frantic (2008)(Scott Huggins).rom",	32640, 0xf43b0b28, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frantic, cv_frantic, cv_coleco)
STD_ROM_FN(cv_frantic)

struct BurnDriver BurnDrvcv_frantic = {
    "cv_frantic", NULL, "cv_coleco", NULL, "2008",
    "Frantic (HB)\0", NULL, "Scott Huggins", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_franticRomInfo, cv_franticRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Frog Feast (HB)
static struct BurnRomInfo cv_frogfeastRomDesc[] = {
	{ "Frog Feast (2007).rom",	21760, 0xcb941eb4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frogfeast, cv_frogfeast, cv_coleco)
STD_ROM_FN(cv_frogfeast)

struct BurnDriver BurnDrvcv_frogfeast = {
	"cv_frogfeast", NULL, "cv_coleco", NULL, "2007",
	"Frog Feast (HB)\0", NULL, "<unknown>", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_frogfeastRomInfo, cv_frogfeastRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Front Line S.C.E. (Hack)
static struct BurnRomInfo cv_frntlnsceRomDesc[] = {
	{ "Front Line S.C.E. (2015)(Team Pixelboy).rom",	32768, 0x832586bf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frntlnsce, cv_frntlnsce, cv_coleco)
STD_ROM_FN(cv_frntlnsce)

struct BurnDriver BurnDrvcv_frntlnsce = {
	"cv_frntlnsce", "cv_frontlin", "cv_coleco", NULL, "2015",
	"Front Line S.C.E. (Hack, Standard Controller Edition)\0", "Published by Team Pixelboy", "Coleco - Taito", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_COLECO, GBF_RUNGUN, 0,
	CVGetZipName, cv_frntlnsceRomInfo, cv_frntlnsceRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Frost Bite (HB)
static struct BurnRomInfo cv_frostbiteRomDesc[] = {
	{ "Frostbite (2016)(Team Pixelboy).rom",	0x08000, 0xb3212318, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_frostbite, cv_frostbite, cv_coleco)
STD_ROM_FN(cv_frostbite)

struct BurnDriver BurnDrvcv_frostbite = {
	"cv_frostbite", NULL, "cv_coleco", NULL, "1983-2016",
	"Frost Bite (HB)\0", "Published by Team Pixelboy", "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_frostbiteRomInfo, cv_frostbiteRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Galaga (SGM) (HB)
static struct BurnRomInfo cv_galagaRomDesc[] = {
    { "Galaga SGM (2014)(CollectorVision).rom",	32768, 0x9f1045e6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_galaga, cv_galaga, cv_coleco)
STD_ROM_FN(cv_galaga)

struct BurnDriver BurnDrvcv_galaga = {
    "cv_galaga", NULL, "cv_coleco", NULL, "1984-2014",
    "Galaga (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Namcot", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_SHOOT, 0,
    CVGetZipName, cv_galagaRomInfo, cv_galagaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// GamePack Vic-20 (HB)
static struct BurnRomInfo cv_gpvic20RomDesc[] = {
	{ "GamePack Vic-20 (2003)(Daniel Bienvenu).rom",	8192, 0x5b4ad168, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gpvic20, cv_gpvic20, cv_coleco)
STD_ROM_FN(cv_gpvic20)

struct BurnDriver BurnDrvcv_gpvic20 = {
	"cv_gpvic20", NULL, "cv_coleco", NULL, "2003",
	"GamePack Vic-20 (HB)\0", NULL, "Daniel Bienvenu", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MINIGAMES, 0,
	CVGetZipName, cv_gpvic20RomInfo, cv_gpvic20RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Gamester 81 - The Video Game (HB)
static struct BurnRomInfo cv_gamester81RomDesc[] = {
    { "Gamester 81 - The Video Game (2013)(Atari2600.com).rom",	32700, 0xcf4a26ff, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gamester81, cv_gamester81, cv_coleco)
STD_ROM_FN(cv_gamester81)

struct BurnDriver BurnDrvcv_gamester81 = {
    "cv_gamester81", NULL, "cv_coleco", NULL, "2013",
    "Gamester 81 - The Video Game (HB)\0", NULL, "Atari2600.com", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_gamester81RomInfo, cv_gamester81RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Gauntlet (SGM) (HB)
static struct BurnRomInfo cv_gauntletRomDesc[] = {
    { "Gauntlet SGM (2019)(Team Pixelboy).rom",	262144, 0x652d533e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gauntlet, cv_gauntlet, cv_coleco)
STD_ROM_FN(cv_gauntlet)

struct BurnDriver BurnDrvcv_gauntlet = {
    "cv_gauntlet", NULL, "cv_coleco", NULL, "1985-2019",
    "Gauntlet (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Atari Games Ltd.", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
    CVGetZipName, cv_gauntletRomInfo, cv_gauntletRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Ghost (SGM) (HB)
static struct BurnRomInfo cv_ghostRomDesc[] = {
    { "Ghost SGM (2019)(Unepic Fran).rom",	131072, 0xd55bbb66, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ghost, cv_ghost, cv_coleco)
STD_ROM_FN(cv_ghost)

struct BurnDriver BurnDrvcv_ghost= {
    "cv_ghost", NULL, "cv_coleco", NULL, "2019",
    "Ghost (SGM) (HB)\0", "SGM - Super Game Module", "Unepic Fran", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_ghostRomInfo, cv_ghostRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Ghost Blaster (HB)
static struct BurnRomInfo cv_gblasterRomDesc[] = {
    { "Ghostblaster (2006)(Daniel Bienvenu).rom",	0x8000, 0x7e93e6f3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gblaster, cv_gblaster, cv_coleco)
STD_ROM_FN(cv_gblaster)

struct BurnDriver BurnDrvcv_gblaster = {
    "cv_gblaster", NULL, "cv_coleco", NULL, "2006",
    "Ghost Blaster (HB)\0", NULL, "Daniel Bienvenu", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_gblasterRomInfo, cv_gblasterRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Ghostbusters (SGM) (HB)
static struct BurnRomInfo cv_ghostbstRomDesc[] = {
    { "Ghostbusters SGM (2018)(Team Pixelboy).rom",	0x10000, 0xfc935cdd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ghostbst, cv_ghostbst, cv_coleco)
STD_ROM_FN(cv_ghostbst)

struct BurnDriver BurnDrvcv_ghostbst = {
    "cv_ghostbst", NULL, "cv_coleco", NULL, "1984-2018",
    "Ghostbusters (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Activision", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_ghostbstRomInfo, cv_ghostbstRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Ghosts'n Zombies (HB)
static struct BurnRomInfo cv_gnzRomDesc[] = {
	{ "Ghosts'n Zombies (2009)(CollectorVision).rom",	32768, 0xcc63ca4b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gnz, cv_gnz, cv_coleco)
STD_ROM_FN(cv_gnz)

struct BurnDriver BurnDrvcv_gnz = {
	"cv_gnz", NULL, "cv_coleco", NULL, "2009",
	"Ghosts'n Zombies (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM | GBF_RUNGUN, 0,
	CVGetZipName, cv_gnzRomInfo, cv_gnzRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Ghost Trap (HB)
static struct BurnRomInfo cv_ghostrapRomDesc[] = {
	{ "Ghost Trap (2009)(Coleco).rom",	12288, 0x26269393, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ghostrap, cv_ghostrap, cv_coleco)
STD_ROM_FN(cv_ghostrap)

struct BurnDriver BurnDrvcv_ghostrap = {
	"cv_ghostrap", NULL, "cv_coleco", NULL, "2009",
	"Ghost Trap (HB)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_ghostrapRomInfo, cv_ghostrapRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Girl's Garden (HB)
static struct BurnRomInfo cv_ggardenRomDesc[] = {
    { "Girl's Garden (2010)(Team Pixelboy).rom",	0x8000, 0x305b74c0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ggarden, cv_ggarden, cv_coleco)
STD_ROM_FN(cv_ggarden)

struct BurnDriver BurnDrvcv_ggarden = {
    "cv_ggarden", NULL, "cv_coleco", NULL, "1984-2010",
    "Girl's Garden (HB)\0", "Published by Team Pixelboy", "Bruce Tomlin - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_ggardenRomInfo, cv_ggardenRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Golgo 13 (HB)
static struct BurnRomInfo cv_golgo13RomDesc[] = {
    { "Golgo 13 (2011)(Team Pixelboy).rom",	0x8000, 0x8acf1bcf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_golgo13, cv_golgo13, cv_coleco)
STD_ROM_FN(cv_golgo13)

struct BurnDriver BurnDrvcv_golgo13 = {
    "cv_golgo13", NULL, "cv_coleco", NULL, "1984-2011",
    "Golgo 13 (HB)\0", "Published by Team Pixelboy", "Eduardo Mello - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_SHOOT, 0,
    CVGetZipName, cv_golgo13RomInfo, cv_golgo13RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Goonies, The (Team Pixelboy) (SGM) (HB)
static struct BurnRomInfo cv_gooniesRomDesc[] = {
    { "Goonies, The SGM (2012)(Team Pixelboy).rom",	0x20000, 0x01581fa8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_goonies, cv_goonies, cv_coleco)
STD_ROM_FN(cv_goonies)

struct BurnDriver BurnDrvcv_goonies = {
    "cv_goonies", "cv_gooniesoc", "cv_coleco", NULL, "1986-2012",
    "Goonies, The (Team Pixelboy) (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_gooniesRomInfo, cv_gooniesRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Goonies, The (Opcode) (SGM) (HB)
static struct BurnRomInfo cv_GooniesocRomDesc[] = {
	{ "Goonies, The - SGM (2023)(Opcode Games).rom", 131072, 0x6C8113C1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Gooniesoc, cv_Gooniesoc, cv_coleco)
STD_ROM_FN(cv_Gooniesoc)

struct BurnDriver BurnDrvcv_Gooniesoc = {
	"cv_gooniesoc", NULL, "cv_coleco", NULL, "1986-2023",
	"Goonies, The (Opcode) (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_GooniesocRomInfo, cv_GooniesocRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitOCM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// GP World (HB)
static struct BurnRomInfo cv_gpworldRomDesc[] = {
    { "GP World (2021)(Team Pixelboy).rom",	32768, 0x3412cf1a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gpworld, cv_gpworld, cv_coleco)
STD_ROM_FN(cv_gpworld)

struct BurnDriver BurnDrvcv_gpworld = {
    "cv_gpworld", NULL, "cv_coleco", NULL, "1985-2021",
    "GP World (HB)\0", "Published by Team Pixelboy", "Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_RACING, 0,
    CVGetZipName, cv_gpworldRomInfo, cv_gpworldRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Gradius (SGM) (HB)
static struct BurnRomInfo cv_GradiusRomDesc[] = {
	{ "Gradius - SGM (2016)(Opcode Games).rom", 131072, 0x30d337e4, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Gradius, cv_Gradius, cv_coleco)
STD_ROM_FN(cv_Gradius)

struct BurnDriver BurnDrvcv_Gradius = {
	"cv_gradius", NULL, "cv_coleco", NULL, "1986-2016",
	"Gradius (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_GradiusRomInfo, cv_GradiusRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitOCM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Gradius (SGM) (HB, Alt)
static struct BurnRomInfo cv_GradiusaRomDesc[] = {
	{ "Gradius - SGM (Alt)(2016)(Opcode Games).rom", 131072, 0x2426C300, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Gradiusa, cv_Gradiusa, cv_coleco)
STD_ROM_FN(cv_Gradiusa)

struct BurnDriver BurnDrvcv_Gradiusa = {
	"cv_gradiusa", "cv_gradius", "cv_coleco", NULL, "1986-2016",
	"Gradius (SGM) (HB, Alt)\0", "SGM - Super Game Module", "Opcode Games - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_GradiusaRomInfo, cv_GradiusaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitOCM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Guardic (SGM) (HB)
static struct BurnRomInfo cv_guardicRomDesc[] = {
    { "Guardic SGM (2018)(Opcode Games).rom",	131072, 0x2b0bb712, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_guardic, cv_guardic, cv_coleco)
STD_ROM_FN(cv_guardic)

struct BurnDriver BurnDrvcv_guardic = {
    "cv_guardic", NULL, "cv_coleco", NULL, "1986-2018",
    "Guardic (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Compile", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_guardicRomInfo, cv_guardicRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Gulkave (HB)
static struct BurnRomInfo cv_gulkaveRomDesc[] = {
    { "Gulkave (2012)(Team Pixelboy).rom",	0x8000, 0x81413ff6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gulkave, cv_gulkave, cv_coleco)
STD_ROM_FN(cv_gulkave)

struct BurnDriver BurnDrvcv_gulkave = {
    "cv_gulkave", NULL, "cv_coleco", NULL, "1986-2012",
    "Gulkave (HB)\0", "Published by Team Pixelboy", "Eduardo Mello - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
    CVGetZipName, cv_gulkaveRomInfo, cv_gulkaveRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Gun Fright (SGM) (HB)
static struct BurnRomInfo cv_gunfrightRomDesc[] = {
    { "Gun Fright SGM (2019)(Opcode Games).rom",	131072, 0x083d13ee, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_gunfright, cv_gunfright, cv_coleco)
STD_ROM_FN(cv_gunfright)

struct BurnDriver BurnDrvcv_gunfright = {
    "cv_gunfright", NULL, "cv_coleco", NULL, "1986-2019",
    "Gun Fright (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Jaleco", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION | GBF_ADV, 0,
    CVGetZipName, cv_gunfrightRomInfo, cv_gunfrightRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Hamster with Wire (HB)
static struct BurnRomInfo cv_hamsterwwRomDesc[] = {
	{ "Hamster with Wire (2022)(Nicolas Campion).rom",	27673, 0xb852affe, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_hamsterww, cv_hamsterww, cv_coleco)
STD_ROM_FN(cv_hamsterww)

struct BurnDriver BurnDrvcv_hamsterww = {
	"cv_hamsterww", NULL, "cv_coleco", NULL, "2022",
	"Hamster with Wire (HB)\0", NULL, "Nicam Shilova", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_hamsterwwRomInfo, cv_hamsterwwRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// HeliFire (HB)
static struct BurnRomInfo cv_helifireRomDesc[] = {
    { "HeliFire (2013)(CollectorVision).rom",	0x08000, 0x29d8c5f4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_helifire, cv_helifire, cv_coleco)
STD_ROM_FN(cv_helifire)

struct BurnDriver BurnDrvcv_helifire = {
    "cv_helifire", NULL, "cv_coleco", NULL, "1980-2013",
    "HeliFire (HB)\0", "Published by CollectorVision Games", "Nintendo", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_helifireRomInfo, cv_helifireRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Heroes Arena (SGM) (HB)
static struct BurnRomInfo cv_heroesarRomDesc[] = {
    { "Heroes Arena SGM (2017)(Team Pixelboy).rom",	0x08000, 0x87a49761, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_heroesar, cv_heroesar, cv_coleco)
STD_ROM_FN(cv_heroesar)

struct BurnDriver BurnDrvcv_heroesar = {
    "cv_heroesar", NULL, "cv_coleco", NULL, "2010-2017",
    "Heroes Arena (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Imanok", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_BALLPADDLE, 0,
    CVGetZipName, cv_heroesarRomInfo, cv_heroesarRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Hole in One (SGM) (HB)
static struct BurnRomInfo cv_holeoneRomDesc[] = {
    { "Hole in One SGM (2017)(CollectorVision).rom",	24520, 0x7270ae16, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_holeone, cv_holeone, cv_coleco)
STD_ROM_FN(cv_holeone)

struct BurnDriver BurnDrvcv_holeone = {
    "cv_holeone", NULL, "cv_coleco", NULL, "1984-2017",
    "Hole in One (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "HAL Laboratory", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_SPORTSMISC, 0,
    CVGetZipName, cv_holeoneRomInfo, cv_holeoneRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Hopman (HB)
static struct BurnRomInfo cv_hopmanRomDesc[] = {
	{ "Hopman (2023)(Inufuto).rom",	9834, 0x856fc7b8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_hopman, cv_hopman, cv_coleco)
STD_ROM_FN(cv_hopman)

struct BurnDriver BurnDrvcv_hopman = {
	"cv_hopman", NULL, "cv_coleco", NULL, "2023",
	"Hopman (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_hopmanRomInfo, cv_hopmanRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Hustle Chumy (HB)
static struct BurnRomInfo cv_hchumyRomDesc[] = {
	{ "Hustle Chumy (2019)(CollectorVision).rom",	18416, 0x5de1fa04, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_hchumy, cv_hchumy, cv_coleco)
STD_ROM_FN(cv_hchumy)

struct BurnDriver BurnDrvcv_hchumy = {
	"cv_hchumy", NULL, "cv_coleco", NULL, "1984-2019",
	"Hustle Chumy (HB)\0", "Published by CollectorVision Games", "Compile", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_hchumyRomInfo, cv_hchumyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Hyper Rally (SGM) (HB, Prototype)
static struct BurnRomInfo cv_hrallyRomDesc[] = {
    { "Hyper Rally SGM (Proto)(2011)(Team Pixelboy).rom",	0x8000, 0xf8b08810, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_hrally, cv_hrally, cv_coleco)
STD_ROM_FN(cv_hrally)

struct BurnDriver BurnDrvcv_hrally = {
    "cv_hrally", NULL, "cv_coleco", NULL, "1985-2011",
    "Hyper Rally (SGM) (HB, Prototype)\0", "SGM - Published by Team Pixelboy", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW | BDF_PROTOTYPE, 1, HARDWARE_COLECO, GBF_RACING, 0,
    CVGetZipName, cv_hrallyRomInfo, cv_hrallyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Ice Muncher (HB)
static struct BurnRomInfo cv_icemuncherRomDesc[] = {
	{ "Ice Muncher (2023)(Nicam Shilova).rom",	29615, 0x8cf16f2f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_icemuncher, cv_icemuncher, cv_coleco)
STD_ROM_FN(cv_icemuncher)

struct BurnDriver BurnDrvcv_icemuncher = {
	"cv_icemuncher", NULL, "cv_coleco", NULL, "2023",
	"Ice Muncher (HB)\0", NULL, "Nicam Shilova", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_icemuncherRomInfo, cv_icemuncherRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Ice World (SGM) (HB)
static struct BurnRomInfo cv_iceworldRomDesc[] = {
    { "Ice World SGM (2020)(CollectorVision).rom",	25470, 0x4f95ee2b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_iceworld, cv_iceworld, cv_coleco)
STD_ROM_FN(cv_iceworld)

struct BurnDriver BurnDrvcv_iceworld = {
    "cv_iceworld", NULL, "cv_coleco", NULL, "1985-2020",
    "Ice World (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Casio", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_iceworldRomInfo, cv_iceworldRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Impetus (HB)
static struct BurnRomInfo cv_impetusRomDesc[] = {
	{ "Impetus (2022)(Inufuto).rom",	16262, 0xe30a446e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_impetus, cv_impetus, cv_coleco)
STD_ROM_FN(cv_impetus)

struct BurnDriver BurnDrvcv_impetus = {
	"cv_impetus", NULL, "cv_coleco", NULL, "2022",
	"Impetus (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_impetusRomInfo, cv_impetusRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Insane Pickin' Sticks VIII (HB)
static struct BurnRomInfo cv_insanepickRomDesc[] = {
    { "Insane Pickin' Sticks VIII (2010)(Daniel Bienvenu).rom",	0x7890, 0x5a6c2d2f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_insanepick, cv_insanepick, cv_coleco)
STD_ROM_FN(cv_insanepick)

struct BurnDriver BurnDrvcv_insanepick = {
    "cv_insanepick", NULL, "cv_coleco", NULL, "2010",
    "Insane Pickin' Sticks (HB)\0", NULL, "Daniel Bienvenu", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_insanepickRomInfo, cv_insanepickRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// J.E.T.P.A.C. (SGM) (HB)
static struct BurnRomInfo cv_jetpacRomDesc[] = {
    { "JETPAC SGM (2017)(Team Pixelboy).rom",	0x8000, 0xae7614c3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jetpac, cv_jetpac, cv_coleco)
STD_ROM_FN(cv_jetpac)

struct BurnDriver BurnDrvcv_jetpac = {
    "cv_jetpac", NULL, "cv_coleco", NULL, "2009-2017",
    "J.E.T.P.A.C. (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Imanok", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
    CVGetZipName, cv_jetpacRomInfo, cv_jetpacRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Jawbreaker I & II (HB)
static struct BurnRomInfo cv_jawbreakerRomDesc[] = {
	{ "Jawbreaker I&II SGM (2017)(CollectorVision).rom",	21749, 0xed9bb76f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jawbreaker, cv_jawbreaker, cv_coleco)
STD_ROM_FN(cv_jawbreaker)

struct BurnDriver BurnDrvcv_jawbreaker = {
	"cv_jawbreaker", NULL, "cv_coleco", NULL, "1983-2017",
	"Jawbreaker I & II (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Sierra On-Line", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_jawbreakerRomInfo, cv_jawbreakerRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Jeepers Creepers (HB)
static struct BurnRomInfo cv_jeepcrepRomDesc[] = {
	{ "Jeepers Creepers (2012)(Daniel Bienvenu).rom",	0x08000, 0x69e3c673, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jeepcrep, cv_jeepcrep, cv_coleco)
STD_ROM_FN(cv_jeepcrep)

struct BurnDriver BurnDrvcv_jeepcrep = {
	"cv_jeepcrep", NULL, "cv_coleco", NULL, "2007",
	"Jeepers Creepers (HB)\0", NULL, "Daniel Bienvenu", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_jeepcrepRomInfo, cv_jeepcrepRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Joust (SGM) (HB)
static struct BurnRomInfo cv_joustsgmRomDesc[] = {
    { "Joust SGM (2014)(Team Pixelboy).rom",	0x8000, 0x62f325b3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_joustsgm, cv_joustsgm, cv_coleco)
STD_ROM_FN(cv_joustsgm)

struct BurnDriver BurnDrvcv_joustsgm = {
    "cv_joustsgm", NULL, "cv_coleco", NULL, "2014",
    "Joust (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Atarisoft", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_joustsgmRomInfo, cv_joustsgmRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Jump Land (SGM) (HB)
static struct BurnRomInfo cv_jumplandRomDesc[] = {
    { "Jump Land SGM (2015)(CollectorVision).rom",	14815, 0xc48db4ce, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_jumpland, cv_jumpland, cv_coleco)
STD_ROM_FN(cv_jumpland)

struct BurnDriver BurnDrvcv_jumpland = {
    "cv_jumpland", NULL, "cv_coleco", NULL, "1985-2015",
    "Jump Land (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Colpax", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_jumplandRomInfo, cv_jumplandRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Junkwall (HB)
static struct BurnRomInfo cv_junkwallRomDesc[] = {
    { "Junkwall (2016)(Silicon Sex).rom",	0x08000, 0x28257c4a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_junkwall, cv_junkwall, cv_coleco)
STD_ROM_FN(cv_junkwall)

struct BurnDriver BurnDrvcv_junkwall = {
    "cv_junkwall", NULL, "cv_coleco", NULL, "2016",
    "Junkwall (HB)\0", NULL, "Silicon Sex", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_junkwallRomInfo, cv_junkwallRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Juno First (SGM) (HB)
static struct BurnRomInfo cv_junofirstRomDesc[] = {
    { "Juno First SGM (2017)(CollectorVision).rom",	32768, 0x1cde6497, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_junofirst, cv_junofirst, cv_coleco)
STD_ROM_FN(cv_junofirst)

struct BurnDriver BurnDrvcv_junofirst = {
    "cv_junofirst", NULL, "cv_coleco", NULL, "1983-2017",
    "Juno First (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
    CVGetZipName, cv_junofirstRomInfo, cv_junofirstRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Kaboom! (HB)
static struct BurnRomInfo cv_kaboomRomDesc[] = {
	{ "Kaboom (2017)(Team Pixelboy).rom",	0x04000, 0x24a2b61b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_kaboom, cv_kaboom, cv_coleco)
STD_ROM_FN(cv_kaboom)

struct BurnDriver BurnDrvcv_kaboom = {
	"cv_kaboom", NULL, "cv_coleco", NULL, "1981-2017",
	"Kaboom! (HB)\0", "Published by Team Pixelboy", "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_kaboomRomInfo, cv_kaboomRomName, NULL, NULL, NULL, NULL, ROLLERInputInfo, ROLLERDIPInfo,
	DrvInitROLLER, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Karateka 2 (HB)
static struct BurnRomInfo cv_karateka2RomDesc[] = {
	{ "Karateka 2 (2021)(Nanochess).rom",	32768, 0x59281c7f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_karateka2, cv_karateka2, cv_coleco)
STD_ROM_FN(cv_karateka2)

struct BurnDriver BurnDrvcv_karateka2 = {
	"cv_karateka2", NULL, "cv_coleco", NULL, "2021",
	"Karateka 2 (HB)\0", NULL, "Nanochess", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VSFIGHT, 0,
	CVGetZipName, cv_karateka2RomInfo, cv_karateka2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Kevtris (HB)
static struct BurnRomInfo cv_kevtrisRomDesc[] = {
    { "Kevtris (1996)(Kevin Horton).rom",	0x4000, 0x819a06e5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_kevtris, cv_kevtris, cv_coleco)
STD_ROM_FN(cv_kevtris)

struct BurnDriver BurnDrvcv_kevtris = {
    "cv_kevtris", NULL, "cv_coleco", NULL, "1996",
    "Kevtris (HB)\0", NULL, "Kevin Horton", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_kevtrisRomInfo, cv_kevtrisRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// King's Valley (SGM) (HB)
static struct BurnRomInfo cv_kingvalleyRomDesc[] = {
    { "King's Valley SGM (2012)(Team Pixelboy).rom",	0x8000, 0xa2ac883, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_kingvalley, cv_kingvalley, cv_coleco)
STD_ROM_FN(cv_kingvalley)

struct BurnDriver BurnDrvcv_kingvalley = {
    "cv_kingvalley", NULL, "cv_coleco", NULL, "1985-2012",
    "King's Valley (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_kingvalleyRomInfo, cv_kingvalleyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// King & Balloon (SGM) (HB)
static struct BurnRomInfo cv_kingballRomDesc[] = {
    { "King and balloon SGM (2018)(Team Pixelboy).rom",	0x8000, 0x9ce3b912, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_kingball, cv_kingball, cv_coleco)
STD_ROM_FN(cv_kingball)

struct BurnDriver BurnDrvcv_kingball = {
    "cv_kingball", NULL, "cv_coleco", NULL, "1984-2018",
    "King & Balloon (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Namco", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_kingballRomInfo, cv_kingballRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Klondike Solitaire (HB, v1.04)
static struct BurnRomInfo cv_ksolitaireRomDesc[] = {
    { "Klondike Solitaire v1.04 (2023)(Under4Mhz).rom",	32768, 0xedc43cd0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ksolitaire, cv_ksolitaire, cv_coleco)
STD_ROM_FN(cv_ksolitaire)

struct BurnDriver BurnDrvcv_ksolitaire = {
    "cv_ksolitaire", NULL, "cv_coleco", NULL, "2021",
    "Klondike Solitaire (HB, v1.04)\0", NULL, "Under4Mhz", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_CARD, 0,
    CVGetZipName, cv_ksolitaireRomInfo, cv_ksolitaireRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Knight Lore (SGM) (HB)
static struct BurnRomInfo cv_kngtloreRomDesc[] = {
    { "Knight Lore SGM (2018)(Team Pixelboy).rom",	0x8000, 0x1523bfbf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_kngtlore, cv_kngtlore, cv_coleco)
STD_ROM_FN(cv_kngtlore)

struct BurnDriver BurnDrvcv_kngtlore = {
    "cv_kngtlore", NULL, "cv_coleco", NULL, "1984-2018",
    "Knight Lore (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Ultimate", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ADV | GBF_MAZE, 0,
    CVGetZipName, cv_kngtloreRomInfo, cv_kngtloreRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Knightmare (Team Pixelboy) (SGM) (HB)
static struct BurnRomInfo cv_kngtmareRomDesc[] = {
    { "Knightmare SGM (fix)(2015)(Team Pixelboy).rom",	0x20000, 0x01cacd0d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_kngtmare, cv_kngtmare, cv_coleco)
STD_ROM_FN(cv_kngtmare)

struct BurnDriver BurnDrvcv_kngtmare = {
    "cv_kngtmare", "cv_kngtmareoc", "cv_coleco", NULL, "1986-2015",
    "Knightmare (Team Pixelboy) (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_kngtmareRomInfo, cv_kngtmareRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Knightmare (Opcode) (SGM)
static struct BurnRomInfo cv_KngtmareocRomDesc[] = {
	{ "Knightmare SGM (2023)(Opcode Games).rom", 131072, 0x80586CC5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Kngtmareoc, cv_Kngtmareoc, cv_coleco)
STD_ROM_FN(cv_Kngtmareoc)

struct BurnDriver BurnDrvcv_Kngtmareoc = {
	"cv_kngtmareoc", NULL, "cv_coleco", NULL, "1986-2023",
	"Knightmare (Opcode) (SGM)\0", "SGM - Super Game Module", "Opcode Games - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_KngtmareocRomInfo, cv_KngtmareocRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitOCM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Knight'n More (HB)
static struct BurnRomInfo cv_knightmoreRomDesc[] = {
	{ "Knight'n More (2016)(Cote Gamers).rom",	32768, 0xc6c33170, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_knightmore, cv_knightmore, cv_coleco)
STD_ROM_FN(cv_knightmore)

struct BurnDriver BurnDrvcv_knightmore = {
	"cv_knightmore", NULL, "cv_coleco", NULL, "2016",
	"Knight'n More (HB)\0", NULL, "Cote Gamers", "ColecoVision",
	NULL, NULL, L"C\u00f4t\u00e9 Gamers", NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM | GBF_RUNGUN, 0,
	CVGetZipName, cv_knightmoreRomInfo, cv_knightmoreRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Kobashi (HB)
static struct BurnRomInfo cv_kobashiRomDesc[] = {
    { "Kobashi (2010)(CollectorVision).rom",	0x08000, 0x9fc4d7c7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_kobashi, cv_kobashi, cv_coleco)
STD_ROM_FN(cv_kobashi)

struct BurnDriver BurnDrvcv_kobashi = {
    "cv_kobashi", NULL, "cv_coleco", NULL, "2010",
    "Kobashi (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_kobashiRomInfo, cv_kobashiRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Konami's Ping-Pong (HB)
static struct BurnRomInfo cv_pingpongRomDesc[] = {
	{ "Konami's Ping-Pong (2011)(Team Pixelboy).rom",	32768, 0x344cb482, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pingpong, cv_pingpong, cv_coleco)
STD_ROM_FN(cv_pingpong)

struct BurnDriver BurnDrvcv_pingpong = {
	"cv_pingpong", NULL, "cv_coleco", NULL, "2011",
	"Konami's Ping-Pong (HB)\0", "Published by Team Pixelboy", "Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_SPORTSMISC, 0,
	CVGetZipName, cv_pingpongRomInfo, cv_pingpongRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Kralizec Tetris (HB)
static struct BurnRomInfo cv_krtetrisRomDesc[] = {
    { "Kralizec Tetris (2017)(Team Pixelboy).rom",	0x8000, 0xe8a91349, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_krtetris, cv_krtetris, cv_coleco)
STD_ROM_FN(cv_krtetris)

struct BurnDriver BurnDrvcv_krtetris = {
    "cv_krtetris", NULL, "cv_coleco", NULL, "2016-17",
    "Kralizec Tetris (HB)\0", "Published by Team Pixelboy", "Kralizec", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_krtetrisRomInfo, cv_krtetrisRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Kung-Fu Master (SGM) (HB)
static struct BurnRomInfo cv_kungfumstrRomDesc[] = {
    { "Kung-Fu Master SGM (2016)(CollectorVision).rom",	0x20000, 0x441693fd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_kungfumstr, cv_kungfumstr, cv_coleco)
STD_ROM_FN(cv_kungfumstr)

struct BurnDriver BurnDrvcv_kungfumstr = {
    "cv_kungfumstr", NULL, "cv_coleco", NULL, "1985-2016",
    "Kung-Fu Master (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Irem", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_SCRFIGHT, 0,
    CVGetZipName, cv_kungfumstrRomInfo, cv_kungfumstrRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// L'Abbaye des Morts (HB)
static struct BurnRomInfo cv_labbayeRomDesc[] = {
	{ "L'Abbaye des Morts (2019)(CollectorVision).rom",	131072, 0xb11a6d23, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_labbaye, cv_labbaye, cv_coleco)
STD_ROM_FN(cv_labbaye)

struct BurnDriver BurnDrvcv_labbaye = {
	"cv_labbaye", NULL, "cv_coleco", NULL, "2019",
	"L'Abbaye des Morts (HB)\0", "Published by CollectorVision Games", "Exidy - Alekmaul", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ADV | GBF_PLATFORM, 0,
	CVGetZipName, cv_labbayeRomInfo, cv_labbayeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Lift (HB)
static struct BurnRomInfo cv_liftRomDesc[] = {
	{ "Lift (2022)(Inufuto).rom",	8166, 0x7a4d5d1b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_lift, cv_lift, cv_coleco)
STD_ROM_FN(cv_lift)

struct BurnDriver BurnDrvcv_lift = {
	"cv_lift", NULL, "cv_coleco", NULL, "2022",
	"Lift (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_liftRomInfo, cv_liftRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Light Grid Racing (HB)
static struct BurnRomInfo cv_lgracingRomDesc[] = {
	{ "Light Grid Racing (2015)(CollectorVision).rom",	32768, 0x1e37ec6a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_lgracing, cv_lgracing, cv_coleco)
STD_ROM_FN(cv_lgracing)

struct BurnDriver BurnDrvcv_lgracing = {
	"cv_lgracing", NULL, "cv_coleco", NULL, "2015",
	"Light Grid Racing (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_lgracingRomInfo, cv_lgracingRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Lock'n Chase (HB)
static struct BurnRomInfo cv_locknchaseRomDesc[] = {
    { "Lock'n Chase (2011)(CollectorVision).rom",	31873, 0x6bc9a350, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_locknchase, cv_locknchase, cv_coleco)
STD_ROM_FN(cv_locknchase)

struct BurnDriver BurnDrvcv_locknchase = {
    "cv_locknchase", NULL, "cv_coleco", NULL, "2011",
    "Lock'n Chase (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
    CVGetZipName, cv_locknchaseRomInfo, cv_locknchaseRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Loco-Motion (SGM) (HB)
static struct BurnRomInfo cv_locomotionRomDesc[] = {
    { "Loco-Motion SGM (2017)(CollectorVision).rom",	32768, 0x5b69d078, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_locomotion, cv_locomotion, cv_coleco)
STD_ROM_FN(cv_locomotion)

struct BurnDriver BurnDrvcv_locomotion = {
    "cv_locomotion", NULL, "cv_coleco", NULL, "1983-2017",
    "Loco-Motion (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_locomotionRomInfo, cv_locomotionRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Lode Runner (HB)
static struct BurnRomInfo cv_loderunnerRomDesc[] = {
	{ "Lode Runner (2002)(Steve Begin).rom",	32768, 0xa2128f74, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_loderunner, cv_loderunner, cv_coleco)
STD_ROM_FN(cv_loderunner)

struct BurnDriver BurnDrvcv_loderunner = {
	"cv_loderunner", NULL, "cv_coleco", NULL, "2009",
	"Lode Runner (HB)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_loderunnerRomInfo, cv_loderunnerRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Lode Runner 2013 (HB)
static struct BurnRomInfo cv_loder2013RomDesc[] = {
	{ "Lode Runner (2013)(Collectorvision).rom",	32438, 0xd679ac52, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_loder2013, cv_loder2013, cv_coleco)
STD_ROM_FN(cv_loder2013)

struct BurnDriver BurnDrvcv_loder2013 = {
	"cv_loder2013", NULL, "cv_coleco", NULL, "2012-13",
	"Lode Runner 2013 (HB)\0", NULL, "Collectorvision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_loder2013RomInfo, cv_loder2013RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Magical Tree (HB)
static struct BurnRomInfo cv_magtreeRomDesc[] = {
    { "Magical Tree (2006)(Opcode Games).rom",	32768, 0xf4314bb1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_magtree, cv_magtree, cv_coleco)
STD_ROM_FN(cv_magtree)

struct BurnDriver BurnDrvcv_magtree = {
    "cv_magtree", NULL, "cv_coleco", NULL, "2006",
    "Magical Tree (HB)\0", "Published by Opcode Games", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_magtreeRomInfo, cv_magtreeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Mahjong Solitaire (HB, v1.16)
static struct BurnRomInfo cv_msolitaireRomDesc[] = {
	{ "Mahjong Solitaire v1.16 (2023)(Under4Mhz).rom",	32768, 0x119af363, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_msolitaire, cv_msolitaire, cv_coleco)
STD_ROM_FN(cv_msolitaire)

struct BurnDriver BurnDrvcv_msolitaire = {
	"cv_msolitaire", NULL, "cv_coleco", NULL, "2021",
	"Mahjong Solitaire (HB, v1.16)\0", NULL, "Under4Mhz", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_msolitaireRomInfo, cv_msolitaireRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Majikazo (SGM) (HB)
static struct BurnRomInfo cv_majikazoRomDesc[] = {
    { "Majikazo SGM (2016)(Team Pixelboy).rom",	0x8000, 0x2a1438c0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_majikazo, cv_majikazo, cv_coleco)
STD_ROM_FN(cv_majikazo)

struct BurnDriver BurnDrvcv_majikazo = {
    "cv_majikazo", NULL, "cv_coleco", NULL, "2015-16",
    "Majikazo (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Lemonize", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_majikazoRomInfo, cv_majikazoRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Mappy (SGM) (HB)
static struct BurnRomInfo cv_mappyRomDesc[] = {
    { "Mappy SGM (2015)(Team Pixelboy).rom",	0x8000, 0x00d30431, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mappy, cv_mappy, cv_coleco)
STD_ROM_FN(cv_mappy)

struct BurnDriver BurnDrvcv_mappy = {
    "cv_mappy", NULL, "cv_coleco", NULL, "1984-2015",
    "Mappy (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Namco", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_mappyRomInfo, cv_mappyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Mario Brothers Final Edition (HB, Rev. D)
static struct BurnRomInfo cv_marioRomDesc[] = {
    { "Mario Brothers Final Edition (2009)(Rev D)(CollectorVision).rom",	0x20000, 0x2e09af0a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mario, cv_mario, cv_coleco)
STD_ROM_FN(cv_mario)

struct BurnDriver BurnDrvcv_mario = {
    "cv_mario", NULL, "cv_coleco", NULL, "1983-2009",
    "Mario Brothers Final Edition (HB, Rev. D)\0", "Published by CollectorVision Games", "Nintendo", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_marioRomInfo, cv_marioRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Mario Brothers (HB)
static struct BurnRomInfo cv_mariobRomDesc[] = {
    { "Mario Brothers (2009)(CollectorVision).rom",	0x10000, 0x11777b27, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mariob, cv_mariob, cv_coleco)
STD_ROM_FN(cv_mariob)

struct BurnDriver BurnDrvcv_mariob = {
    "cv_mariob", "cv_mario", "cv_coleco", NULL, "1983-2009",
    "Mario Brothers (HB)\0", "Published by CollectorVision Games", "Nintendo", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_mariobRomInfo, cv_mariobRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Maze Maniac (HB)
static struct BurnRomInfo cv_mazemaniacRomDesc[] = {
	{ "Maze Maniac (2006)(Charles-Mathieu Boyer).rom",	32640, 0x44bfaf23, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mazemaniac, cv_mazemaniac, cv_coleco)
STD_ROM_FN(cv_mazemaniac)

struct BurnDriver BurnDrvcv_mazemaniac = {
	"cv_mazemaniac", NULL, "cv_coleco", NULL, "2006",
	"Maze Maniac (HB)\0", NULL, "Charles-Mathieu Boyer", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MAZE, 0,
	CVGetZipName, cv_mazemaniacRomInfo, cv_mazemaniacRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// MazezaM (HB)
static struct BurnRomInfo cv_mazezamRomDesc[] = {
    { "MazezaM (2004)(Ventzislav Tzvetkov).rom",	16384, 0x150006fc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mazezam, cv_mazezam, cv_coleco)
STD_ROM_FN(cv_mazezam)

struct BurnDriver BurnDrvcv_mazezam = {
    "cv_mazezam", NULL, "cv_coleco", NULL, "2004",
    "MazezaM (HB)\0", NULL, "Ventzislav Tzvetkov", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_mazezamRomInfo, cv_mazezamRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// MazezaM Challenge (HB)
static struct BurnRomInfo cv_mazezamchRomDesc[] = {
    { "MazezaM Challenge (2016)(Collectorvision).rom",	32768, 0x8eded0e2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mazezamch, cv_mazezamch, cv_coleco)
STD_ROM_FN(cv_mazezamch)

struct BurnDriver BurnDrvcv_mazezamch = {
    "cv_mazezamch", NULL, "cv_coleco", NULL, "2016",
    "MazezaM Challenge (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_mazezamchRomInfo, cv_mazezamchRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Mazy (HB)
static struct BurnRomInfo cv_mazyRomDesc[] = {
	{ "Mazy (2022)(Inufuto).rom",	6967, 0x02ef2836, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mazy, cv_mazy, cv_coleco)
STD_ROM_FN(cv_mazy)

struct BurnDriver BurnDrvcv_mazy = {
	"cv_mazy", NULL, "cv_coleco", NULL, "2022",
	"Mazy (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_mazyRomInfo, cv_mazyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mazy 2 (HB)
static struct BurnRomInfo cv_mazy2RomDesc[] = {
	{ "Mazy 2 (2025)(Inufuto).rom",	9595, 0x5d9d95dc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mazy2, cv_mazy2, cv_coleco)
STD_ROM_FN(cv_mazy2)

struct BurnDriver BurnDrvcv_mazy2 = {
	"cv_mazy2", NULL, "cv_coleco", NULL, "2025",
	"Mazy 2 (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_mazy2RomInfo, cv_mazy2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mecha 8 (SGM) (HB)
static struct BurnRomInfo cv_mecha8RomDesc[] = {
	{ "Mecha 8 SGM (2013)(Team Pixelboy).rom",	0x020000, 0x53da40bc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mecha8, cv_mecha8, cv_coleco)
STD_ROM_FN(cv_mecha8)

struct BurnDriver BurnDrvcv_mecha8 = {
	"cv_mecha8", NULL, "cv_coleco", NULL, "2013",
	"Mecha-8 (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Oscar Toledo G.", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_mecha8RomInfo, cv_mecha8RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mecha 9 (SGM) (HB)
static struct BurnRomInfo cv_mecha9RomDesc[] = {
	{ "Mecha 9 SGM (2015)(Team Pixelboy).rom",	0x040000, 0xb405591a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mecha9, cv_mecha9, cv_coleco)
STD_ROM_FN(cv_mecha9)

struct BurnDriver BurnDrvcv_mecha9 = {
	"cv_mecha9", NULL, "cv_coleco", NULL, "2015",
	"Mecha-9 (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Oscar Toledo G.", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_mecha9RomInfo, cv_mecha9RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Memotech MTX Series Vol.1 (HB)
static struct BurnRomInfo cv_mmtxvol1RomDesc[] = {
	{ "Memotech MTX Series Vol 1 (2013).rom",	31448, 0xf8b5bb4f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mmtxvol1, cv_mmtxvol1, cv_coleco)
STD_ROM_FN(cv_mmtxvol1)

struct BurnDriver BurnDrvcv_mmtxvol1 = {
	"cv_mmtxvol1", NULL, "cv_coleco", NULL, "2013",
	"Memotech MTX Series Vol.1 (HB)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_mmtxvol1RomInfo, cv_mmtxvol1RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Memotech MTX Series Vol.2 (HB)
static struct BurnRomInfo cv_mmtxvol2RomDesc[] = {
	{ "Memotech MTX Series Vol 2 (2013).rom",	27839, 0x06ff3853, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mmtxvol2, cv_mmtxvol2, cv_coleco)
STD_ROM_FN(cv_mmtxvol2)

struct BurnDriver BurnDrvcv_mmtxvol2 = {
	"cv_mmtxvol2", NULL, "cv_coleco", NULL, "2013",
	"Memotech MTX Series Vol.2 (HB)\0", NULL, "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_mmtxvol2RomInfo, cv_mmtxvol2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mikie (HB)
static struct BurnRomInfo cv_mikieRomDesc[] = {
	{ "Mikie (2021)(Team Pixelboy).rom",	32768, 0xad434a6d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mikie, cv_mikie, cv_coleco)
STD_ROM_FN(cv_mikie)

struct BurnDriver BurnDrvcv_mikie = {
	"cv_mikie", NULL, "cv_coleco", NULL, "1984-2021",
	"Mikie (HB)\0", "Published by Team Pixelboy", "Konami - Sega", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_mikieRomInfo, cv_mikieRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mindwalls (HB)
static struct BurnRomInfo cv_mindwallsRomDesc[] = {
    { "Mindwalls (2013)(CollectorVision Games).rom",	32755, 0x7b61b12e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mindwalls, cv_mindwalls, cv_coleco)
STD_ROM_FN(cv_mindwalls)

struct BurnDriver BurnDrvcv_mindwalls = {
    "cv_mindwalls", NULL, "cv_coleco", NULL, "2013",
    "Mindwalls (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_BREAKOUT, 0,
    CVGetZipName, cv_mindwallsRomInfo, cv_mindwallsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Missile Strike (HB)
static struct BurnRomInfo cv_misstrikeRomDesc[] = {
	{ "Missile Strike (2021)(AnalogKid).rom",	27164, 0xdd730dbd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_misstrike, cv_misstrike, cv_coleco)
STD_ROM_FN(cv_misstrike)

struct BurnDriver BurnDrvcv_misstrike = {
	"cv_misstrike", NULL, "cv_coleco", NULL, "2021",
	"Missile Strike (HB)\0", NULL, "AlalogKid", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_misstrikeRomInfo, cv_misstrikeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mobile Planet Styllus (SGM) (HB)
static struct BurnRomInfo cv_mobplanstyRomDesc[] = {
    { "Mobile Planet Styllus SGM (20XX)(unknown).rom",	131072, 0x318d6bcb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mobplansty, cv_mobplansty, cv_coleco)
STD_ROM_FN(cv_mobplansty)

struct BurnDriver BurnDrvcv_mobplansty = {
    "cv_mobplansty", NULL, "cv_coleco", NULL, "20??",
    "Mobile Planet Styllus (SGM) (HB)\0", "SGM - Super Game Module", "<unknown>", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_RUNGUN, 0,
    CVGetZipName, cv_mobplanstyRomInfo, cv_mobplanstyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Module Man (HB)
static struct BurnRomInfo cv_modulemanRomDesc[] = {
	{ "Module Man (2013)(Team Pixelboy).rom",	0x08000, 0xeb6be8ec, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_moduleman, cv_moduleman, cv_coleco)
STD_ROM_FN(cv_moduleman)

struct BurnDriver BurnDrvcv_moduleman = {
	"cv_moduleman", NULL, "cv_coleco", NULL, "2013",
	"Module Man (HB)\0", NULL, "Team Pixelboy", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_modulemanRomInfo, cv_modulemanRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Monaco GP (HB)
static struct BurnRomInfo cv_monacogpRomDesc[] = {
	{ "Monaco GP (2021)(Mystery Man).rom",	32768, 0x5d21678e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_monacogp, cv_monacogp, cv_coleco)
STD_ROM_FN(cv_monacogp)

struct BurnDriver BurnDrvcv_monacogp = {
	"cv_monacogp", NULL, "cv_coleco", NULL, "1983-2021",
	"Monaco GP (HB)\0", "Published by Team Pixelboy", "Mystery Man - Sega", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_RACING, 0,
	CVGetZipName, cv_monacogpRomInfo, cv_monacogpRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Monster House (SGM) (HB)
static struct BurnRomInfo cv_monsthouseRomDesc[] = {
    { "Monster House SGM (2017)(CollectorVision).rom",	32768, 0x50f02dc9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_monsthouse, cv_monsthouse, cv_coleco)
STD_ROM_FN(cv_monsthouse)

struct BurnDriver BurnDrvcv_monsthouse = {
    "cv_monsthouse", NULL, "cv_coleco", NULL, "1986-2017",
    "Monster House (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Casio", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_monsthouseRomInfo, cv_monsthouseRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Mooncresta (SGM) (HB)
static struct BurnRomInfo cv_MooncrestaRomDesc[] = {
	{ "Mooncresta - SGM (2023)(Opcode Games).rom", 131072, 0xBDAE4248, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Mooncresta, cv_Mooncresta, cv_coleco)
STD_ROM_FN(cv_Mooncresta)

struct BurnDriver BurnDrvcv_Mooncresta = {
	"cv_mooncresta", NULL, "cv_coleco", NULL, "1980-2023",
	"Mooncresta (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Nichibutsu", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_MooncrestaRomInfo, cv_MooncrestaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitOCM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mopiranger (HB)
static struct BurnRomInfo cv_mopirangRomDesc[] = {
    { "Mopiranger (2012) (Team Pixelboy).rom",	32768, 0xa60be082, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mopirang, cv_mopirang, cv_coleco)
STD_ROM_FN(cv_mopirang)

struct BurnDriver BurnDrvcv_mopirang = {
    "cv_mopirang", NULL, "cv_coleco", NULL, "1985-2012",
    "Mopiranger (HB)\0", "Published by Team Pixelboy", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_mopirangRomInfo, cv_mopirangRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Mr. Chin (HB)
static struct BurnRomInfo cv_mrchinRomDesc[] = {
	{ "Mr. Chin (2008)(Collectorvision Games).rom",	32768, 0x9ab11795, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mrchin, cv_mrchin, cv_coleco)
STD_ROM_FN(cv_mrchin)

struct BurnDriver BurnDrvcv_mrchin = {
	"cv_mrchin", NULL, "cv_coleco", NULL, "1984-2008",
	"Mr. Chin (HB)\0", "Published by CollectorVision Games", "HAL Laboratory", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_mrchinRomInfo, cv_mrchinRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Mr. Do! Run Run (SGM) (HB)
static struct BurnRomInfo cv_mrdorunrunRomDesc[] = {
    { "Mr. Do! Run Run SGM (2022)(CollectorVision).rom",	131072, 0x13d53b3c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mrdorunrun, cv_mrdorunrun, cv_coleco)
STD_ROM_FN(cv_mrdorunrun)

struct BurnDriver BurnDrvcv_mrdorunrun = {
    "cv_mrdorunrun", NULL, "cv_coleco", NULL, "1987-2022",
    "Mr. Do! Run Run (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Universal", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_mrdorunrunRomInfo, cv_mrdorunrunRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Mr. Do's Wild Ride (HB)
static struct BurnRomInfo cv_mrdowrRomDesc[] = {
	{ "Mr. Do's Wild Ride (2021)(CollectorVision).rom",	131072, 0xd3ea5876, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mrdowr, cv_mrdowr, cv_coleco)
STD_ROM_FN(cv_mrdowr)

struct BurnDriver BurnDrvcv_mrdowr = {
	"cv_mrdowr", NULL, "cv_coleco", NULL, "1984-2021",
	"Mr. Do's Wild Ride (HB)\0", "Published by CollectorVision Games", "Universal", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_mrdowrRomInfo, cv_mrdowrRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Ms. Space Fury (HB)
static struct BurnRomInfo cv_msspacefuryRomDesc[] = {
    { "Ms Space Fury (Daniel Bienvenu)(2001).rom",	0x08000, 0xd055a446, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_msspacefury, cv_msspacefury, cv_coleco)
STD_ROM_FN(cv_msspacefury)

struct BurnDriver BurnDrvcv_msspacefury = {
    "cv_msspacefury", NULL, "cv_coleco", NULL, "2001",
    "Ms. Space Fury (HB)\0", NULL, "Daniel Bienvenu", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_msspacefuryRomInfo, cv_msspacefuryRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Multiverse (HB)
static struct BurnRomInfo cv_multiverseRomDesc[] = {
    { "Multiverse (2019)(Team Pixelboy).rom",	32768, 0xd45475ac, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_multiverse, cv_multiverse, cv_coleco)
STD_ROM_FN(cv_multiverse)

struct BurnDriver BurnDrvcv_multiverse = {
    "cv_multiverse", NULL, "cv_coleco", NULL, "2019",
    "Multiverse (HB)\0", "Published by Team Pixelboy", "Hikaru Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_multiverseRomInfo, cv_multiverseRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Mummy Maze (HB)
static struct BurnRomInfo cv_mummymazeRomDesc[] = {
	{ "Mummy Maze (2024)(Nicam Shilova).rom",	30454, 0x7724857a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_mummymaze, cv_mummymaze, cv_coleco)
STD_ROM_FN(cv_mummymaze)

struct BurnDriver BurnDrvcv_mummymaze = {
	"cv_mummymaze", NULL, "cv_coleco", NULL, "2024",
	"Mummy Maze (HB)\0", NULL, "Nicam Shilova", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MAZE, 0,
	CVGetZipName, cv_mummymazeRomInfo, cv_mummymazeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Muncher Mouse (HB)
static struct BurnRomInfo cv_munchmouseRomDesc[] = {
    { "Muncher Mouse (2018)(Cote Gamers).rom",	32768, 0x440b9ab6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_munchmouse, cv_munchmouse, cv_coleco)
STD_ROM_FN(cv_munchmouse)

struct BurnDriver BurnDrvcv_munchmouse = {
    "cv_munchmouse", NULL, "cv_coleco", NULL, "2018",
    "Muncher Mouse (HB)\0", NULL, "Cote Gamers", "ColecoVision",
    NULL, NULL, L"C\u00f4t\u00e9 Gamers", NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
    CVGetZipName, cv_munchmouseRomInfo, cv_munchmouseRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Nether Dungeon (HB)
static struct BurnRomInfo cv_nethdungeonRomDesc[] = {
	{ "Nether Dungeon (2018)(CollectorVision).rom",	131072, 0x4657bb8c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_nethdungeon, cv_nethdungeon, cv_coleco)
STD_ROM_FN(cv_nethdungeon)

struct BurnDriver BurnDrvcv_nethdungeon = {
	"cv_nethdungeon", "cv_sydneyhunt", "cv_coleco", NULL, "2018",
	"Nether Dungeon (HB)\0", "Hacked version of 'Sydney Hunter and the Sacred Tribe'", "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HOMEBREW | BDF_HACK, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_nethdungeonRomInfo, cv_nethdungeonRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Neuras (HB)
static struct BurnRomInfo cv_neurasRomDesc[] = {
	{ "Neuras (2022)(Inufuto).rom",	8243, 0xa2be9635, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_neuras, cv_neuras, cv_coleco)
STD_ROM_FN(cv_neuras)

struct BurnDriver BurnDrvcv_neuras = {
	"cv_neuras", NULL, "cv_coleco", NULL, "2022",
	"Neuras (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_neurasRomInfo, cv_neurasRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Nibbli: Son of Nibbler (HB)
static struct BurnRomInfo cv_nibbliRomDesc[] = {
    { "Nibbli (2014)(CollectorVision).rom",	0x08000, 0x41555f66, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_nibbli, cv_nibbli, cv_coleco)
STD_ROM_FN(cv_nibbli)

struct BurnDriver BurnDrvcv_nibbli = {
    "cv_nibbli", NULL, "cv_coleco", NULL, "2014",
    "Nibbli: Son of Nibbler (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MAZE, 0,
    CVGetZipName, cv_nibbliRomInfo, cv_nibbliRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Nim (HB)
static struct BurnRomInfo cv_nimRomDesc[] = {
	{ "Nim (2000)(Daniel Bienvenu).rom",	6912, 0x9c059a51, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_nim, cv_nim, cv_coleco)
STD_ROM_FN(cv_nim)

struct BurnDriver BurnDrvcv_nim = {
	"cv_nim", NULL, "cv_coleco", NULL, "2000",
	"Nim (HB)\0", NULL, "Daniel Bienvenu", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_nimRomInfo, cv_nimRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Ninja Princess (SGM) (HB)
static struct BurnRomInfo cv_ninjaprincessRomDesc[] = {
    { "Ninja Princess SGM (2011)(Team PixelBoy).rom",	0x8000, 0x5fb0ed62, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ninjaprincess, cv_ninjaprincess, cv_coleco)
STD_ROM_FN(cv_ninjaprincess)

struct BurnDriver BurnDrvcv_ninjaprincess = {
    "cv_ninjaprincess", NULL, "cv_coleco", NULL, "1986-2011",
    "Ninja Princess (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Eduardo Mello - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_RUNGUN, 0,
    CVGetZipName, cv_ninjaprincessRomInfo, cv_ninjaprincessRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// N-Sub (HB)
static struct BurnRomInfo cv_nsubRomDesc[] = {
    { "N-Sub (2021)(Team Pixelboy).rom",	16384, 0xc73cd5dc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_nsub, cv_nsub, cv_coleco)
STD_ROM_FN(cv_nsub)

struct BurnDriver BurnDrvcv_nsub = {
    "cv_nsub", NULL, "cv_coleco", NULL, "1988-2021",
    "N-Sub (HB)\0", "Published by Team Pixelboy", "Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_nsubRomInfo, cv_nsubRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Operation Hibernation (HB)
static struct BurnRomInfo cv_ophibernRomDesc[] = {
	{ "Operation Hibernation (2025)(Jess Creations).rom",	32768, 0x028809f5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ophibern, cv_ophibern, cv_coleco)
STD_ROM_FN(cv_ophibern)

struct BurnDriver BurnDrvcv_ophibern = {
	"cv_ophibern", NULL, "cv_coleco", NULL, "2025",
	"Operation Hibernation (HB)\0", NULL, "Jess Creations", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_ophibernRomInfo, cv_ophibernRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Operation Wolf (SGM) (HB)
static struct BurnRomInfo cv_opwolfRomDesc[] = {
    { "Operation Wolf SGM (2016)(Team Pixelboy).rom",	0x8000, 0xc177bfd4, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_opwolf, cv_opwolf, cv_coleco)
STD_ROM_FN(cv_opwolf)

struct BurnDriver BurnDrvcv_opwolf = {
    "cv_opwolf", NULL, "cv_coleco", NULL, "2016",
    "Operation Wolf (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Lemonize", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_SHOOT, 0,
    CVGetZipName, cv_opwolfRomInfo, cv_opwolfRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Ozma Wars (HB)
static struct BurnRomInfo cv_ozmawarRomDesc[] = {
	{ "Ozma Wars (2011)(CollectorVision).rom",	32768, 0x2424627f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ozmawar, cv_ozmawar, cv_coleco)
STD_ROM_FN(cv_ozmawar)

struct BurnDriver BurnDrvcv_ozmawar = {
	"cv_ozmawar", NULL, "cv_coleco", NULL, "2011",
	"Ozma Wars (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_ozmawarRomInfo, cv_ozmawarRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Pacar (HB)
static struct BurnRomInfo cv_pacarRomDesc[] = {
    { "Pacar (2017)(Team Pixelboy).rom",	32768, 0xa82c9593, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pacar, cv_pacar, cv_coleco)
STD_ROM_FN(cv_pacar)

struct BurnDriver BurnDrvcv_pacar = {
    "cv_pacar", NULL, "cv_coleco", NULL, "1983-2017",
    "Pacar (HB)\0", "Published by Team Pixelboy", "Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
    CVGetZipName, cv_pacarRomInfo, cv_pacarRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Pac-Man Collection (HB)
static struct BurnRomInfo cv_pacmancolRomDesc[] = {
	{ "Pac-Man Collection (2008)(Opcode Games).rom",	0x20000, 0xf3ccacb3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pacmancol, cv_pacmancol, cv_coleco)
STD_ROM_FN(cv_pacmancol)

struct BurnDriver BurnDrvcv_pacmancol = {
	"cv_pacmancol", NULL, "cv_coleco", NULL, "2008",
	"Pac-Man Collection (HB)\0", NULL, "Opcode Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_pacmancolRomInfo, cv_pacmancolRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Pang (HB)
static struct BurnRomInfo cv_pangRomDesc[] = {
	{ "Pang (2012)(CollectorVision).rom",	32558, 0x18aced43, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pang, cv_pang, cv_coleco)
STD_ROM_FN(cv_pang)

struct BurnDriver BurnDrvcv_pang = {
	"cv_pang", NULL, "cv_coleco", NULL, "2012",
	"Pang (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_pangRomInfo, cv_pangRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Peek-A-Boo Elmo (HB)
static struct BurnRomInfo cv_peekabooRomDesc[] = {
    { "Peek-A-Boo (2010)(Team Pixelboy).rom",	0x8000, 0x32c52187, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_peekaboo, cv_peekaboo, cv_coleco)
STD_ROM_FN(cv_peekaboo)

struct BurnDriver BurnDrvcv_peekaboo = {
    "cv_peekaboo", NULL, "cv_coleco", NULL, "2010",
    "Peek-A-Boo (HB)\0", "Published by Team Pixelboy", "Dvik & Joyrex", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MISC, 0,
    CVGetZipName, cv_peekabooRomInfo, cv_peekabooRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Pegged (HB)
static struct BurnRomInfo cv_peggedRomDesc[] = {
	{ "Pegged (2021)(Under4MHz).rom",	32768, 0x1abf1b9e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pegged, cv_pegged, cv_coleco)
STD_ROM_FN(cv_pegged)

struct BurnDriver BurnDrvcv_pegged = {
	"cv_pegged", NULL, "cv_coleco", NULL, "2021",
	"Pegged (HB)\0", NULL, "Under4Mhz", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_peggedRomInfo, cv_peggedRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Penguin Adventure (SGM) (HB)
static struct BurnRomInfo cv_PenguinadvRomDesc[] = {
	{ "Penguin Adventure SGM (2016)(Opcode Games).rom", 131072, 0x6831ad48, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Penguinadv, cv_Penguinadv, cv_coleco)
STD_ROM_FN(cv_Penguinadv)

struct BurnDriver BurnDrvcv_Penguinadv = {
	"cv_penguinadv", NULL, "cv_coleco", NULL, "1986-2016",
	"Penguin Adventure (SGM) (HB)\0", NULL, "Opcode Games - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_PenguinadvRomInfo, cv_PenguinadvRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitOCM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Penguin Land (HB)
static struct BurnRomInfo cv_penguinlandRomDesc[] = {
    { "Penguin Land (2010)(CollectorVision).rom",	31788, 0x2357c3e3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_penguinland, cv_penguinland, cv_coleco)
STD_ROM_FN(cv_penguinland)

struct BurnDriver BurnDrvcv_penguinland = {
    "cv_penguinland", NULL, "cv_coleco", NULL, "2003-2010",
    "Penguin Land (HB)\0", "Published by CollectorVision Games", "Steve Bégin", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_penguinlandRomInfo, cv_penguinlandRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Pengy (HB)
static struct BurnRomInfo cv_pengyRomDesc[] = {
    { "Pengy (2020)(Cote Gamers).rom",	32768, 0x3dd459ee, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pengy, cv_pengy, cv_coleco)
STD_ROM_FN(cv_pengy)

struct BurnDriver BurnDrvcv_pengy = {
    "cv_pengy", NULL, "cv_coleco", NULL, "1992-2020",
    "Pengy (HB)\0", NULL, "Cote Gamers", "ColecoVision",
    NULL, NULL, L"C\u00f4t\u00e9 Gamers", NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_pengyRomInfo, cv_pengyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Pillars (HB)
static struct BurnRomInfo cv_pillarsRomDesc[] = {
	{ "Pillars (2021)(Under4Mhz).rom",	32768, 0x5a49b249, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pillars, cv_pillars, cv_coleco)
STD_ROM_FN(cv_pillars)

struct BurnDriver BurnDrvcv_pillars = {
	"cv_pillars", NULL, "cv_coleco", NULL, "2021",
	"Pillars (HB)\0", NULL, "Under4Mhz", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_pillarsRomInfo, cv_pillarsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Pippols (HB)
static struct BurnRomInfo cv_pippolsRomDesc[] = {
	{ "Pippols (2014)(CollectorVision).rom",	32768, 0x28d04775, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pippols, cv_pippols, cv_coleco)
STD_ROM_FN(cv_pippols)

struct BurnDriver BurnDrvcv_pippols = {
	"cv_pippols", NULL, "cv_coleco", NULL, "1985-2014",
	"Pippols (HB)\0", "Published by CollectorVision Games", "Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_pippolsRomInfo, cv_pippolsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Pitfall II Arcade (HB)
static struct BurnRomInfo cv_pitfall2aRomDesc[] = {
    { "Pitfall II Arcade (2010)(Team Pixelboy).rom",	0x8000, 0x519bfe40, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pitfall2a, cv_pitfall2a, cv_coleco)
STD_ROM_FN(cv_pitfall2a)

struct BurnDriver BurnDrvcv_pitfall2a = {
    "cv_pitfall2a", NULL, "cv_coleco", NULL, "1985-2010",
    "Pitfall II Arcade (HB)\0", "Published by Team Pixelboy", "Stephen Seehorn - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_pitfall2aRomInfo, cv_pitfall2aRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Pitman (HB)
static struct BurnRomInfo cv_pitmanRomDesc[] = {
	{ "Pitman (2021)(Under4Mhz).rom",	32768, 0x50998610, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pitman, cv_pitman, cv_coleco)
STD_ROM_FN(cv_pitman)

struct BurnDriver BurnDrvcv_pitman = {
	"cv_pitman", NULL, "cv_coleco", NULL, "2021",
	"Pitman (HB)\0", NULL, "Under4Mhz", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_pitmanRomInfo, cv_pitmanRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Pooyan (HB)
static struct BurnRomInfo cv_pooyanRomDesc[] = {
    { "Pooyan (2009)(Dwik & JoyRex).rom",	0x08000, 0x0e25ebd7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pooyan, cv_pooyan, cv_coleco)
STD_ROM_FN(cv_pooyan)

struct BurnDriver BurnDrvcv_pooyan = {
    "cv_pooyan", NULL, "cv_coleco", NULL, "2010",
    "Pooyan (HB)\0", "Published by CollectorVision Games", "Dvik & Joyrex", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_pooyanRomInfo, cv_pooyanRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Princess Quest (SGM) (HB)
static struct BurnRomInfo cv_princessquestRomDesc[] = {
	{ "Princess Quest SGM (2012)(Team Pixelboy).rom",	0x40000, 0xa59eaa2b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_princessquest, cv_princessquest, cv_coleco)
STD_ROM_FN(cv_princessquest)

struct BurnDriver BurnDrvcv_princessquest = {
	"cv_pquest", NULL, "cv_coleco", NULL, "2012",
	"Princess Quest (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Oscar Toledo G.", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_princessquestRomInfo, cv_princessquestRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Prisoner of War (SGM) (HB)
static struct BurnRomInfo cv_prisonofwarRomDesc[] = {
    { "Prisoner of War SGM (2019)(Team Pixelboy).rom",	131072, 0xf998b515, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_prisonofwar, cv_prisonofwar, cv_coleco)
STD_ROM_FN(cv_prisonofwar)

struct BurnDriver BurnDrvcv_prisonofwar = {
    "cv_prisonofwar", NULL, "cv_coleco", NULL, "2018-19",
    "Prisoner of War (SGM) (HB)\0", "SGM - Super Game Module", "Team Pixelboy", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_ADV, 0,
    CVGetZipName, cv_prisonofwarRomInfo, cv_prisonofwarRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Purple Dinosaur Massacre (HB)
static struct BurnRomInfo cv_purpdinoRomDesc[] = {
    { "Purple Dinosaur Massacre (1996)(John Dondzilla).rom",	9494, 0x137d6c4c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_purpdino, cv_purpdino, cv_coleco)
STD_ROM_FN(cv_purpdino)

struct BurnDriver BurnDrvcv_purpdino = {
    "cv_purpdino", NULL, "cv_coleco", NULL, "1996",
    "Purple Dinosaur Massacre (HB)\0", NULL, "John Dondzilla", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
    CVGetZipName, cv_purpdinoRomInfo, cv_purpdinoRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Puzzli (HB)
static struct BurnRomInfo cv_puzzliRomDesc[] = {
    { "Puzzli (CollectorVision)(2011).rom",	0x08000, 0x4236e57e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_puzzli, cv_puzzli, cv_coleco)
STD_ROM_FN(cv_puzzli)

struct BurnDriver BurnDrvcv_puzzli = {
    "cv_puzzli", NULL, "cv_coleco", NULL, "2011",
    "Puzzli (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_puzzliRomInfo, cv_puzzliRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Pyramid Warp & Battleship Clapton 2 (HB)
static struct BurnRomInfo cv_pyrawbc2RomDesc[] = {
    { "Pyramid Warp & Battle Ship Clapton II (2009)(Collectorvision Games).rom",	32768, 0xf7052b06, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_pyrawbc2, cv_pyrawbc2, cv_coleco)
STD_ROM_FN(cv_pyrawbc2)

struct BurnDriver BurnDrvcv_pyrawbc2 = {
    "cv_pyrawbc2", NULL, "cv_coleco", NULL, "2008",
    "Pyramid Warp & Battleship Clapton 2 (HB)\0", "Published by CollectorVision Games", "Dvik & Joyrex", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MAZE | GBF_VERSHOOT, 0,
    CVGetZipName, cv_pyrawbc2RomInfo, cv_pyrawbc2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// qbIqS (SGM) (HB)
static struct BurnRomInfo cv_qbiqsRomDesc[] = {
    { "qbIqS SGM (2010)(Team Pixelboy).rom",	131072, 0xee530ad2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_qbiqs, cv_qbiqs, cv_coleco)
STD_ROM_FN(cv_qbiqs)

struct BurnDriver BurnDrvcv_qbiqs = {
    "cv_qbiqs", NULL, "cv_coleco", NULL, "2010",
    "qbIqS (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Z80ST Software", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PUZZLE | GBF_VERSHOOT, 0,
    CVGetZipName, cv_qbiqsRomInfo, cv_qbiqsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Quest for the Golden Chalice (HB)
static struct BurnRomInfo cv_questgcRomDesc[] = {
	{ "Quest for Golden Chalice (2012)(Team Pixelboy).rom",	0x08000, 0x6da37da8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_questgc, cv_questgc, cv_coleco)
STD_ROM_FN(cv_questgc)

struct BurnDriver BurnDrvcv_questgc = {
	"cv_questgc", NULL, "cv_coleco", NULL, "2012",
	"Quest for the Golden Chalice (HB)\0", NULL, "Team Pixelboy", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_ADV, 0,
	CVGetZipName, cv_questgcRomInfo, cv_questgcRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// R-Type (SGM) (HB)
static struct BurnRomInfo cv_rtypeRomDesc[] = {
    { "R-Type SGM (2020)(Team Pixelboy).rom",	524288, 0xb4785b43, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rtype, cv_rtype, cv_coleco)
STD_ROM_FN(cv_rtype)

struct BurnDriver BurnDrvcv_rtype = {
    "cv_rtype", NULL, "cv_coleco", NULL, "1988-2020",
    "R-Type (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Irem", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
    CVGetZipName, cv_rtypeRomInfo, cv_rtypeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Raid on Bungeling Bay (SGM) (HB)
static struct BurnRomInfo cv_raidbungbayRomDesc[] = {
    { "Raid on Bungeling Bay SGM (2019)(Opcode Games).rom",	131072, 0xc3588f0f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_raidbungbay, cv_raidbungbay, cv_coleco)
STD_ROM_FN(cv_raidbungbay)

struct BurnDriver BurnDrvcv_raidbungbay = {
    "cv_raidbungbay", NULL, "cv_coleco", NULL, "1984-2019",
    "Raid on Bungeling Bay (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Broderbund", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
    CVGetZipName, cv_raidbungbayRomInfo, cv_raidbungbayRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Rally-X (SGM) (HB)
static struct BurnRomInfo cv_rallyxRomDesc[] = {
    { "Rally-X SGM (2016)(Team Pixelboy).rom",	0x8000, 0xfb5dd80d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rallyx, cv_rallyx, cv_coleco)
STD_ROM_FN(cv_rallyx)

struct BurnDriver BurnDrvcv_rallyx = {
    "cv_rallyx", NULL, "cv_coleco", NULL, "1984-2016",
    "Rally-X (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Namco", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
    CVGetZipName, cv_rallyxRomInfo, cv_rallyxRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Recon Rescue (HB)
static struct BurnRomInfo cv_recrescueRomDesc[] = {
	{ "Recon Rescue (2021)(Nicam Shilova).rom",	17746, 0x85aeb133, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_recrescue, cv_recrescue, cv_coleco)
STD_ROM_FN(cv_recrescue)

struct BurnDriver BurnDrvcv_recrescue = {
	"cv_recrescue", NULL, "cv_coleco", NULL, "2021",
	"Recon Rescue (HB)\0", NULL, "Nicam Shilova", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_recrescueRomInfo, cv_recrescueRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Remember the Flag (HB)
static struct BurnRomInfo cv_rtheflagRomDesc[] = {
    { "Remember The Flag (2017)(Team Pixelboy).rom",	0x8000, 0x7e090dfb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rtheflag, cv_rtheflag, cv_coleco)
STD_ROM_FN(cv_rtheflag)

struct BurnDriver BurnDrvcv_rtheflag = {
    "cv_rtheflag", NULL, "cv_coleco", NULL, "2016-17",
    "Remember the Flag (HB)\0", NULL, "Team Pixelboy", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_rtheflagRomInfo, cv_rtheflagRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Reversi and Diamond Dash (HB)
static struct BurnRomInfo cv_reversddRomDesc[] = {
    { "Reversi and Diamond Dash (2004)(Daniel Bienvenu).rom",	29184, 0x2331b6f6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_reversdd, cv_reversdd, cv_coleco)
STD_ROM_FN(cv_reversdd)

struct BurnDriver BurnDrvcv_reversdd = {
    "cv_reversdd", NULL, "cv_coleco", NULL, "2004",
    "Reversi and Diamond Dash (HB)\0", NULL, "Daniel Bienvenu", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION | GBF_BOARD, 0,
    CVGetZipName, cv_reversddRomInfo, cv_reversddRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Reversi (HB)
static struct BurnRomInfo cv_reversiRomDesc[] = {
    { "Reversi (2001)(Daniel Bienvenu).rom",	23296, 0x7d102fe7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_reversi, cv_reversi, cv_coleco)
STD_ROM_FN(cv_reversi)

struct BurnDriver BurnDrvcv_reversi = {
    "cv_reversi", NULL, "cv_coleco", NULL, "2004",
    "Reversi (HB)\0", NULL, "Daniel Bienvenu", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_BOARD, 0,
    CVGetZipName, cv_reversiRomInfo, cv_reversiRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Rip Cord (HB)
static struct BurnRomInfo cv_ripcordRomDesc[] = {
    { "Rip Cord (2017)(CollectorVision).rom",	0x08000, 0x60312b4e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_ripcord, cv_ripcord, cv_coleco)
STD_ROM_FN(cv_ripcord)

struct BurnDriver BurnDrvcv_ripcord = {
    "cv_ripcord", NULL, "cv_coleco", NULL, "1979-2017",
    "Rip Cord (HB)\0", "Published by CollectorVision Games", "Exidy", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_ripcordRomInfo, cv_ripcordRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Risky Rick (HB)
static struct BurnRomInfo cv_RiskyrickRomDesc[] = {
	{ "Risky Rick (2018)(ArcadeVision).rom", 32768, 0x68fdb91b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Riskyrick, cv_Riskyrick, cv_coleco)
STD_ROM_FN(cv_Riskyrick)

struct BurnDriver BurnDrvcv_Riskyrick = {
	"cv_riskyrick", NULL, "cv_coleco", NULL, "2018",
	"Risky Rick (HB)\0", NULL, "ArcadeVision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_RiskyrickRomInfo, cv_RiskyrickRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Road Fighter (HB)
static struct BurnRomInfo cv_roadfghtRomDesc[] = {
    { "Road Fighter (2007)(Opcode Games).rom",	32768, 0x570b9935, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_roadfght, cv_roadfght, cv_coleco)
STD_ROM_FN(cv_roadfght)

struct BurnDriver BurnDrvcv_roadfght = {
    "cv_roadfght", NULL, "cv_coleco", NULL, "2007",
    "Road Fighter (HB)\0", NULL, "Opcode Games - Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_RACING, 0,
    CVGetZipName, cv_roadfghtRomInfo, cv_roadfghtRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Robee Blaster (HB)
static struct BurnRomInfo cv_robeeblrRomDesc[] = {
    { "Robee Blaster (2017)(Alekmaul).rom",	32767, 0x9e15db3f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_robeeblr, cv_robeeblr, cv_coleco)
STD_ROM_FN(cv_robeeblr)

struct BurnDriver BurnDrvcv_robeeblr = {
    "cv_robeeblr", NULL, "cv_coleco", NULL, "2017",
    "Robee Blaster (HB)\0", "Published by CollectorVision Games", "Alekmaul", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_robeeblrRomInfo, cv_robeeblrRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Robotron 2084 (HB)
static struct BurnRomInfo cv_robo2084RomDesc[] = {
    { "Robotron 2084 (2022)(CollectorVision).rom",	32768, 0x7d34387d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_robo2084, cv_robo2084, cv_coleco)
STD_ROM_FN(cv_robo2084)

struct BurnDriver BurnDrvcv_robo2084 = {
    "cv_robo2084", NULL, "cv_coleco", NULL, "2022",
    "Robotron 2084 (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
    CVGetZipName, cv_robo2084RomInfo, cv_robo2084RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Rollerball (SGM) (HB)
static struct BurnRomInfo cv_rollerballRomDesc[] = {
    { "Rollerball SGM (2013)(CollectorVision).rom",	32768, 0xaefdba29, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_rollerball, cv_rollerball, cv_coleco)
STD_ROM_FN(cv_rollerball)

struct BurnDriver BurnDrvcv_rollerball = {
    "cv_rollerball", NULL, "cv_coleco", NULL, "1984-2013",
    "Rollerball (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "HAL Laboratory", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PINBALL, 0,
    CVGetZipName, cv_rollerballRomInfo, cv_rollerballRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Runaway Train (SGM) (HB)
static struct BurnRomInfo cv_runawayRomDesc[] = {
    { "Runaway Train SGM (2023)(8 bit Milli Games).rom",	20218, 0x08fef448, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_runaway, cv_runaway, cv_coleco)
STD_ROM_FN(cv_runaway)

struct BurnDriver BurnDrvcv_runaway = {
    "cv_runaway", NULL, "cv_coleco", NULL, "2023",
    "Runaway Train (SGM) (HB)\0", "SGM - Super Game Module", "8 bit Milli Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_runawayRomInfo, cv_runawayRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Saguaro City (HB)
static struct BurnRomInfo cv_saguaroRomDesc[] = {
    { "Saguaro City (2019)(CollectorVision).rom",	0x08000, 0xc6cead71, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_saguaro, cv_saguaro, cv_coleco)
STD_ROM_FN(cv_saguaro)

struct BurnDriver BurnDrvcv_saguaro = {
    "cv_saguaro", NULL, "cv_coleco", NULL, "1981-2019",
    "Saguaro City (HB)\0", "Published by CollectorVision Games", "Exidy", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_saguaroRomInfo, cv_saguaroRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Sasa (SGM) (HB)
static struct BurnRomInfo cv_sasaRomDesc[] = {
    { "Sasa SGM (2015)(CollectorVision).rom",	23894, 0x9c48816c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sasa, cv_sasa, cv_coleco)
STD_ROM_FN(cv_sasa)

struct BurnDriver BurnDrvcv_sasa = {
    "cv_sasa", NULL, "cv_coleco", NULL, "1983-2015",
    "Sasa (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Mass Tael Ltd.", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_sasaRomInfo, cv_sasaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Search For The Stolen Crown Jewels (HB)
static struct BurnRomInfo cv_sftscjRomDesc[] = {
    { "Search for the Stolen Crown Jewels (2006)(Philipp Klaus Krause).rom",	32768, 0x889f5b98, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sftscj, cv_sftscj, cv_coleco)
STD_ROM_FN(cv_sftscj)

struct BurnDriver BurnDrvcv_sftscj = {
    "cv_sftscj", NULL, "cv_coleco", NULL, "2006",
    "Search For The Stolen Crown Jewels (HB)\0", NULL, "Philipp Klaus Krause", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_sftscjRomInfo, cv_sftscjRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Search For The Stolen Crown Jewels 2 (HB)
static struct BurnRomInfo cv_sftscj2RomDesc[] = {
    { "Search for the Stolen Crown Jewels 2 (2007)(Philipp Klaus Krause).rom",	32768, 0xa883fa9a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sftscj2, cv_sftscj2, cv_coleco)
STD_ROM_FN(cv_sftscj2)

struct BurnDriver BurnDrvcv_sftscj2 = {
    "cv_sftscj2", NULL, "cv_coleco", NULL, "2007",
    "Search For The Stolen Crown Jewels 2 (HB)\0", NULL, "Philipp Klaus Krause", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_sftscj2RomInfo, cv_sftscj2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Search For The Stolen Crown Jewels 3 (HB)
static struct BurnRomInfo cv_sftscj3RomDesc[] = {
    { "Search for the Stolen Crown Jewels 3 (2013)(Phillipp Klaus Krause).rom",	32768, 0x1f3981fc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sftscj3, cv_sftscj3, cv_coleco)
STD_ROM_FN(cv_sftscj3)

struct BurnDriver BurnDrvcv_sftscj3 = {
    "cv_sftscj3", NULL, "cv_coleco", NULL, "2013",
    "Search For The Stolen Crown Jewels 3 (HB)\0", NULL, "Phillipp Klaus Krause", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
    CVGetZipName, cv_sftscj3RomInfo, cv_sftscj3RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Secret of the Moai (SGM) (HB)
static struct BurnRomInfo cv_moaiRomDesc[] = {
    { "Secret of the Moai SGM (2017)(Team Pixelboy).rom",	0x20000, 0xb753a8ca, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_moai, cv_moai, cv_coleco)
STD_ROM_FN(cv_moai)

struct BurnDriver BurnDrvcv_moai = {
    "cv_moai", NULL, "cv_coleco", NULL, "1986-2017",
    "Secret of the Moai (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Casio", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM | GBF_PUZZLE, 0,
    CVGetZipName, cv_moaiRomInfo, cv_moaiRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Sega Flipper (HB)
static struct BurnRomInfo cv_segaflipperRomDesc[] = {
	{ "Sega Flipper (2021)(Team Pixelboy).rom",	16384, 0xe7739f82, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_segaflipper, cv_segaflipper, cv_coleco)
STD_ROM_FN(cv_segaflipper)

struct BurnDriver BurnDrvcv_segaflipper = {
	"cv_segaflipper", NULL, "cv_coleco", NULL, "1983-2021",
	"Sega Flipper (HB)\0", "Published by Team Pixelboy", "Sega", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PINBALL, 0,
	CVGetZipName, cv_segaflipperRomInfo, cv_segaflipperRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Shamus (HB)
static struct BurnRomInfo cv_shamusRomDesc[] = {
	{ "Shamus (2025)(Nanochess).rom",	24576, 0xbae53ace, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_shamus, cv_shamus, cv_coleco)
STD_ROM_FN(cv_shamus)

struct BurnDriver BurnDrvcv_shamus = {
	"cv_shamus", NULL, "cv_coleco", NULL, "2025",
	"Shamus (HB)\0", NULL, "Nanochess", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
	CVGetZipName, cv_shamusRomInfo, cv_shamusRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Shmup! (SGM) (HB)
static struct BurnRomInfo cv_shmupRomDesc[] = {
    { "Shmup SGM (2017)(Team Pixelboy).rom",	0x08000, 0x10e8dd09, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_shmup, cv_shmup, cv_coleco)
STD_ROM_FN(cv_shmup)

struct BurnDriver BurnDrvcv_shmup = {
    "cv_shmup", NULL, "cv_coleco", NULL, "2017",
    "Shmup! (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Imanok", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_shmupRomInfo, cv_shmupRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Shouganai (SGM) (HB)
static struct BurnRomInfo cv_shouganaiRomDesc[] = {
	{ "Shouganai SGM (2017)(Team Pixelboy).rom",	0x08000, 0x89c2b16f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_shouganai, cv_shouganai, cv_coleco)
STD_ROM_FN(cv_shouganai)

struct BurnDriver BurnDrvcv_shouganai = {
	"cv_shouganai", NULL, "cv_coleco", NULL, "2017",
	"Shouganai (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Paxanga", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_shouganaiRomInfo, cv_shouganaiRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Side Trak (HB)
static struct BurnRomInfo cv_sidetrakRomDesc[] = {
    { "Side Trak (2012)(CollectorVision).rom",	0x08000, 0x2e4c28e2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sidetrak, cv_sidetrak, cv_coleco)
STD_ROM_FN(cv_sidetrak)

struct BurnDriver BurnDrvcv_sidetrak = {
    "cv_sidetrak", NULL, "cv_coleco", NULL, "1979-2011",
    "Side Trak (HB)\0", "Published by CollectorVision Games", "Exidy", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_sidetrakRomInfo, cv_sidetrakRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Sindbad Mystery (HB)
static struct BurnRomInfo cv_sindmystRomDesc[] = {
	{ "Sindbad Mystery (2021)(Team Pixelboy).rom",	32768, 0x1ed11001, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sindmyst, cv_sindmyst, cv_coleco)
STD_ROM_FN(cv_sindmyst)

struct BurnDriver BurnDrvcv_sindmyst = {
	"cv_sindmyst", NULL, "cv_coleco", NULL, "1983-2021",
	"Sindbad Mystery (HB)\0", "Published by Team Pixelboy", "Sega", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_sindmystRomInfo, cv_sindmystRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Sir Ababol (HB, v1.1)
static struct BurnRomInfo cv_sirababolRomDesc[] = {
	{ "Sir Ababol v1.1 (2020)(Alekmaul).rom",	0x07fff, 0x690e8dd5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sirababol, cv_sirababol, cv_coleco)
STD_ROM_FN(cv_sirababol)

struct BurnDriver BurnDrvcv_sirababol = {
	"cv_sirababol", NULL, "cv_coleco", NULL, "2020",
	"Sir Ababol (HB, v1.1)\0", NULL, "Alekmaul", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_sirababolRomInfo, cv_sirababolRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Sky Jaguar (HB)
static struct BurnRomInfo cv_skyjagRomDesc[] = {
    { "Sky Jaguar (2004)(Opcode Games).rom",	32768, 0x54d54968, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_skyjag, cv_skyjag, cv_coleco)
STD_ROM_FN(cv_skyjag)

struct BurnDriver BurnDrvcv_skyjag = {
    "cv_skyjag", NULL, "cv_coleco", NULL, "2004",
    "Sky Jaguar (HB)\0", "Published by Opcode Games", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_skyjagRomInfo, cv_skyjagRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Slither (Hack, Joystick Version)
static struct BurnRomInfo cv_danslitherRomDesc[] = {
	{ "Slither (Hack, Joy-Version)(2010)(Daniel Bienvenu).rom",	0x0402a, 0x92624cff, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_danslither, cv_danslither, cv_coleco)
STD_ROM_FN(cv_danslither)

struct BurnDriver BurnDrvcv_danslither = {
	"cv_danslither", "cv_slither", "cv_coleco", NULL, "2010",
	"Slither (Hack, Joystick Version)\0", NULL, "Daniel Bienvenu", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_danslitherRomInfo, cv_danslitherRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Smurf Challenge (HB)
static struct BurnRomInfo cv_smurfchalRomDesc[] = {
	{ "Smurf Challenge (2010)(CollectorVision).bin",	32768, 0x807e1811, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_smurfchal, cv_smurfchal, cv_coleco)
STD_ROM_FN(cv_smurfchal)

struct BurnDriver BurnDrvcv_smurfchal = {
	"cv_smurfchal", NULL, "cv_coleco", NULL, "2010",
	"Smurf Challenge (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MINIGAMES, 0,
	CVGetZipName, cv_smurfchalRomInfo, cv_smurfchalRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Snake (HB)
static struct BurnRomInfo cv_snakeRomDesc[] = {
	{ "Snake (2021)(Under4Mhz).rom",	14114, 0xedd4c690, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_snake, cv_snake, cv_coleco)
STD_ROM_FN(cv_snake)

struct BurnDriver BurnDrvcv_snake = {
	"cv_snake", NULL, "cv_coleco", NULL, "2021",
	"Snake (HB)\0", NULL, "Under4Mhz", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_snakeRomInfo, cv_snakeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Solar Fox II (HB)
static struct BurnRomInfo cv_solarfox2RomDesc[] = {
    { "Solar Fox II (2025)(Jess Creations).rom",	32768, 0xff92d927, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_solarfox2, cv_solarfox2, cv_coleco)
STD_ROM_FN(cv_solarfox2)

struct BurnDriver BurnDrvcv_solarfox2 = {
    "cv_solarfox2", NULL, "cv_coleco", NULL, "2025",
    "Solar Fox II (HB)\0", NULL, "Jess Creations", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_solarfox2RomInfo, cv_solarfox2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// So, you want to be a Knight? (HB)
static struct BurnRomInfo cv_soknightRomDesc[] = {
    { "So, you want to be a Knight (2015)(CollectorVision).rom",	0x08000, 0x72f34a55, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_soknight, cv_soknight, cv_coleco)
STD_ROM_FN(cv_soknight)

struct BurnDriver BurnDrvcv_soknight = {
    "cv_soknight", NULL, "cv_coleco", NULL, "2015",
    "So, you want to be a Knight? (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
    CVGetZipName, cv_soknightRomInfo, cv_soknightRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Space Hunter (HB)
static struct BurnRomInfo cv_spacehunterRomDesc[] = {
    { "Space Hunter (2005)(Guy Foster).rom",	4147, 0x9badaa20, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spacehunter, cv_spacehunter, cv_coleco)
STD_ROM_FN(cv_spacehunter)

struct BurnDriver BurnDrvcv_spacehunter = {
    "cv_spacehunter", NULL, "cv_coleco", NULL, "2005",
    "Space Hunter (HB)\0", NULL, "Guy Foster", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_spacehunterRomInfo, cv_spacehunterRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Space Invaders Collection (HB)
static struct BurnRomInfo cv_spaceinvcolRomDesc[] = {
    { "Space Invaders Collection (2003)(Opcode Games).rom",	0x8000, 0x3098bd4b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spaceinvcol, cv_spaceinvcol, cv_coleco)
STD_ROM_FN(cv_spaceinvcol)

struct BurnDriver BurnDrvcv_spaceinvcol = {
    "cv_spaceinvcol", NULL, "cv_coleco", NULL, "2003",
    "Space Invaders Collection (HB)\0", NULL, "Opcode Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_spaceinvcolRomInfo, cv_spaceinvcolRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Space Invasion (HB)
static struct BurnRomInfo cv_spaceinvasionRomDesc[] = {
	{ "Space Invasion (1998)(John Dondzilla).rom",	16384, 0xaf9b178c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spaceinvasion, cv_spaceinvasion, cv_coleco)
STD_ROM_FN(cv_spaceinvasion)

struct BurnDriver BurnDrvcv_spaceinvasion = {
	"cv_spaceinvasion", NULL, "cv_coleco", NULL, "1998",
	"Space Invasion (HB)\0", NULL, "John Dondzilla", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_spaceinvasionRomInfo, cv_spaceinvasionRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Space Shuttle (SGM + EEPROM) (HB)
static struct BurnRomInfo cv_spaceshuttleRomDesc[] = {
	{ "Space Shuttle SGM_EEPROM (2023)(Team Pixelboy).rom",	65536, 0xbb0f6678, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spaceshuttle, cv_spaceshuttle, cv_coleco)
STD_ROM_FN(cv_spaceshuttle)

static INT32 DrvInitSGMEEPROM() // Space Shuttle
{
    use_I2C = 1;
    use_SGM = 1;
    return DrvInit();
}

struct BurnDriver BurnDrvcv_spaceshuttle = {
	"cv_spaceshuttle", NULL, "cv_coleco", NULL, "1986-2023",
	"Space Shuttle (SGM + EEPROM) (HB)\0", "SGM - Published by Team Pixelboy", "Activision", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_SIM, 0,
	CVGetZipName, cv_spaceshuttleRomInfo, cv_spaceshuttleRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitSGMEEPROM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Space Venture (Hack, v3.1)
static struct BurnRomInfo cv_spaceventureRomDesc[] = {
	{ "Space Venture v3.1 (2019)(TIX).rom",	32768, 0xf7ed24e9, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spaceventure, cv_spaceventure, cv_coleco)
STD_ROM_FN(cv_spaceventure)

struct BurnDriver BurnDrvcv_spaceventure = {
	"cv_spaceventure", "cv_venture", "cv_coleco", NULL, "1982",
	"Space Venture (Hack, v3.1)\0", NULL, "TIX", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
	CVGetZipName, cv_spaceventureRomInfo, cv_spaceventureRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Sparkie (SGM) (HB)
static struct BurnRomInfo cv_sparkieRomDesc[] = {
    { "Sparkie SGM (2017)(CollectorVision).rom",	32768, 0xeb19c7a3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sparkie, cv_sparkie, cv_coleco)
STD_ROM_FN(cv_sparkie)

struct BurnDriver BurnDrvcv_sparkie = {
    "cv_sparkie", NULL, "cv_coleco", NULL, "1983-2013",
    "Sparkie (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
    CVGetZipName, cv_sparkieRomInfo, cv_sparkieRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Spectar (HB)
static struct BurnRomInfo cv_spectarRomDesc[] = {
    { "Spectar (2002)(AtariAge).rom",	0x08000, 0xa3aaae47, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spectar, cv_spectar, cv_coleco)
STD_ROM_FN(cv_spectar)

struct BurnDriver BurnDrvcv_spectar = {
    "cv_spectar", NULL, "cv_coleco", NULL, "2005",
    "Spectar (HB)\0", NULL, "AtariAge", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MAZE, 0,
    CVGetZipName, cv_spectarRomInfo, cv_spectarRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Spelunker (SGM) (HB)
static struct BurnRomInfo cv_spelunkerRomDesc[] = {
    { "Spelunker SGM (2015)(Team Pixelboy).rom",	0x20000, 0x75f84889, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spelunker, cv_spelunker, cv_coleco)
STD_ROM_FN(cv_spelunker)

struct BurnDriver BurnDrvcv_spelunker = {
    "cv_spelunker", NULL, "cv_coleco", NULL, "1986-2015",
    "Spelunker (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Broderbund", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_spelunkerRomInfo, cv_spelunkerRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// SpinBall (HB)
static struct BurnRomInfo cv_spinballRomDesc[] = {
	{ "SpinBall (2020)(Nicam Shilova).rom",	17047, 0xe592805e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spinball, cv_spinball, cv_coleco)
STD_ROM_FN(cv_spinball)

struct BurnDriver BurnDrvcv_spinball = {
	"cv_spinball", NULL, "cv_coleco", NULL, "2020",
	"SpinBall (HB)\0", NULL, "Nicam Shilova", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_spinballRomInfo, cv_spinballRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Spunky's Super Car! (HB)
static struct BurnRomInfo cv_spunkyscRomDesc[] = {
    { "Spunky's Super Car! (2014)(Collectorvision Games).rom",	32697, 0xdeed811e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_spunkysc, cv_spunkysc, cv_coleco)
STD_ROM_FN(cv_spunkysc)

struct BurnDriver BurnDrvcv_spunkysc = {
    "cv_spunkysc", NULL, "cv_coleco", NULL, "2014",
    "Spunky's Super Car! (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_RACING, 0,
    CVGetZipName, cv_spunkyscRomInfo, cv_spunkyscRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Squares! (HB)
static struct BurnRomInfo cv_squaresRomDesc[] = {
    { "Squares! (2007)(Harvey deKleine).rom",	16384, 0x37198a63, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_squares, cv_squares, cv_coleco)
STD_ROM_FN(cv_squares)

struct BurnDriver BurnDrvcv_squares = {
    "cv_squares", NULL, "cv_coleco", NULL, "2007",
    "Squares! (HB)\0", NULL, "Harvey deKleine", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_squaresRomInfo, cv_squaresRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Star Castle (HB)
static struct BurnRomInfo cv_StarcastleRomDesc[] = {
	{ "Star Castle (2021)(Team Pixelboy).rom", 32768, 0xb48b9d51, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_Starcastle, cv_Starcastle, cv_coleco)
STD_ROM_FN(cv_Starcastle)

struct BurnDriver BurnDrvcv_Starcastle = {
	"cv_starcastle", NULL, "cv_coleco", NULL, "2021",
	"Star Castle (HB)\0", NULL, "Team Pixelboy", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_StarcastleRomInfo, cv_StarcastleRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Starcom (HB)
static struct BurnRomInfo cv_starcomRomDesc[] = {
	{ "Starcom (2012)(CollectorVision).rom",	32768, 0x61c98628, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_starcom, cv_starcom, cv_coleco)
STD_ROM_FN(cv_starcom)

struct BurnDriver BurnDrvcv_starcom = {
	"cv_starcom", NULL, "cv_coleco", NULL, "1986-2012",
	"Starcom (HB)\0", "Published by CollectorVision Games", "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_starcomRomInfo, cv_starcomRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Star Fire (HB)
static struct BurnRomInfo cv_starfireRomDesc[] = {
	{ "Star Fire (2021)(Chris Oberth).rom",	18688, 0xea3aa29e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_starfire, cv_starfire, cv_coleco)
STD_ROM_FN(cv_starfire)

struct BurnDriver BurnDrvcv_starfire = {
	"cv_starfire", NULL, "cv_coleco", NULL, "2021",
	"Star Fire (HB)\0", NULL, "Chris Oberth", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_starfireRomInfo, cv_starfireRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Star Force (HB)
static struct BurnRomInfo cv_starforceRomDesc[] = {
    { "Star Force (2011)(Team Pixelboy).rom",	0x8000, 0xcbc58796, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_starforce, cv_starforce, cv_coleco)
STD_ROM_FN(cv_starforce)

struct BurnDriver BurnDrvcv_starforce = {
    "cv_starforce", NULL, "cv_coleco", NULL, "2011",
    "Star Force (HB)\0", "Published by Team Pixelboy", "Eduardo Mello - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_starforceRomInfo, cv_starforceRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Star Fortress (HB)
static struct BurnRomInfo cv_starfortressRomDesc[] = {
	{ "Star Fortress (1997)(John Dondzila).rom",	0x04000, 0xf7f18e6f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_starfortress, cv_starfortress, cv_coleco)
STD_ROM_FN(cv_starfortress)

struct BurnDriver BurnDrvcv_starfortress = {
	"cv_starfortress", NULL, "cv_coleco", NULL, "1997",
	"Star Fortress (HB)\0", NULL, "John Dondzila", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_starfortressRomInfo, cv_starfortressRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Star Jacker (HB)
static struct BurnRomInfo cv_starjackerRomDesc[] = {
    { "Star Jacker (2021)(Team Pixelboy).rom",	32768, 0x929c6ded, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_starjacker, cv_starjacker, cv_coleco)
STD_ROM_FN(cv_starjacker)

struct BurnDriver BurnDrvcv_starjacker = {
    "cv_starjacker", NULL, "cv_coleco", NULL, "1983-2021",
    "Star Jacker (HB)\0", "Published by Team Pixelboy", "Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_starjackerRomInfo, cv_starjackerRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Starship Defence Force (HB)
static struct BurnRomInfo cv_starshipdfRomDesc[] = {
    { "Starship Defence Force (2015)(CollectorVision).rom",	0x08000, 0x160dda58, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_starshipdf, cv_starshipdf, cv_coleco)
STD_ROM_FN(cv_starshipdf)

struct BurnDriver BurnDrvcv_starshipdf = {
    "cv_starshipdf", NULL, "cv_coleco", NULL, "2014",
    "Starship Defence Force (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
    CVGetZipName, cv_starshipdfRomInfo, cv_starshipdfRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Star Soldier (SGM) (HB)
static struct BurnRomInfo cv_starsoldierRomDesc[] = {
	{ "Star Soldier SGM (2016)(CollectorVision).rom",	131072, 0x3e7d0520, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_starsoldier, cv_starsoldier, cv_coleco)
STD_ROM_FN(cv_starsoldier)

static INT32 starsoldierInit()
{
	INT32 rc = DrvInitSGM();

	if (!rc) {
		AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	}
	return rc;
}

struct BurnDriver BurnDrvcv_starsoldier = {
	"cv_starsoldier", NULL, "cv_coleco", NULL, "2015",
	"Star Soldier (SGM) (HB)\0", "SGM - Super Game Module", "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_starsoldierRomInfo, cv_starsoldierRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	starsoldierInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Stone of Wisdom, The (SGM) (HB)
static struct BurnRomInfo cv_stonewRomDesc[] = {
    { "Stone of Wisdom SGM (2015)(Team Pixelboy).rom",	0x20000, 0xba2e3fea, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_stonew, cv_stonew, cv_coleco)
STD_ROM_FN(cv_stonew)

struct BurnDriver BurnDrvcv_stonew = {
    "cv_stonew", NULL, "cv_coleco", NULL, "1986-2015",
    "Stone of Wisdom, The (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Casio", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ADV | GBF_MAZE, 0,
    CVGetZipName, cv_stonewRomInfo, cv_stonewRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Stray Cat (SGM) (HB)
static struct BurnRomInfo cv_straycatRomDesc[] = {
    { "Stray Cat SGM (2017)(Team Pixelboy).rom",	0x08000, 0xf55f499e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_straycat, cv_straycat, cv_coleco)
STD_ROM_FN(cv_straycat)

struct BurnDriver BurnDrvcv_straycat = {
    "cv_straycat", NULL, "cv_coleco", NULL, "2017",
    "Stray Cat (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Imanok", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_straycatRomInfo, cv_straycatRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Strip Poker (HB)
static struct BurnRomInfo cv_strippokerRomDesc[] = {
	{ "Strip Poker (2014)(NewColeco).rom",	32768, 0xd80b6cff, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_strippoker, cv_strippoker, cv_coleco)
STD_ROM_FN(cv_strippoker)

struct BurnDriver BurnDrvcv_strippoker = {
	"cv_strippoker", NULL, "cv_coleco", NULL, "2014",
	"Strip Poker (HB)\0", NULL, "NewColeco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_CARD, 0,
	CVGetZipName, cv_strippokerRomInfo, cv_strippokerRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Subroc (SGM) (HB)
static struct BurnRomInfo cv_subrocsgmRomDesc[] = {
    { "Subroc SGM (2014)(Team Pixelboy).rom",	0x20000, 0xeac71b43, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_subrocsgm, cv_subrocsgm, cv_coleco)
STD_ROM_FN(cv_subrocsgm)

struct BurnDriver BurnDrvcv_subrocsgm = {
    "cv_subrocsgm", NULL, "cv_coleco", NULL, "1984-2014",
    "Subroc (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Coleco", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_SHOOT, 0,
    CVGetZipName, cv_subrocsgmRomInfo, cv_subrocsgmRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Sudoku (HB)
static struct BurnRomInfo cv_sudokuRomDesc[] = {
    { "Sudoku (2017)(Team Pixelboy).rom",	0x8000, 0x85237b85, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sudoku, cv_sudoku, cv_coleco)
STD_ROM_FN(cv_sudoku)

struct BurnDriver BurnDrvcv_sudoku = {
    "cv_sudoku", NULL, "cv_coleco", NULL, "2016-17",
    "Sudoku (HB)\0", NULL, "Team Pixelboy", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_sudokuRomInfo, cv_sudokuRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Super Pac-Man (SGM) (HB)
static struct BurnRomInfo cv_superpacRomDesc[] = {
    { "Super Pac-Man SGM (2016)(Team Pixelboy).rom",	0x8000, 0x260cdf98, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_superpac, cv_superpac, cv_coleco)
STD_ROM_FN(cv_superpac)

struct BurnDriver BurnDrvcv_superpac = {
    "cv_superpac", NULL, "cv_coleco", NULL, "1982-2015",
    "Super Pac-Man (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Coleco", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MAZE | GBF_ACTION, 0,
    CVGetZipName, cv_superpacRomInfo, cv_superpacRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Super Space Acer (HB)
static struct BurnRomInfo cv_suprspacRomDesc[] = {
    { "Super Space Acer (2011)(Mike Brent).rom",	0x20000, 0xae209065, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_suprspac, cv_suprspac, cv_coleco)
STD_ROM_FN(cv_suprspac)

struct BurnDriver BurnDrvcv_suprspac = {
    "cv_suprspac", NULL, "cv_coleco", NULL, "2011",
    "Super Space Acer (HB)\0", NULL, "Mike Brent", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_suprspacRomInfo, cv_suprspacRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Super Tank (HB)
static struct BurnRomInfo cv_suprtankRomDesc[] = {
    { "Super Tank (2011)(Team Pixelboy).rom",	0x08000, 0xe9b7ff6c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_suprtank, cv_suprtank, cv_coleco)
STD_ROM_FN(cv_suprtank)

struct BurnDriver BurnDrvcv_suprtank = {
    "cv_suprtank", NULL, "cv_coleco", NULL, "1986-2011",
    "Super Tank (HB)\0", "Published by Team Pixelboy", "Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_suprtankRomInfo, cv_suprtankRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Svellas (HB)
static struct BurnRomInfo cv_svellasRomDesc[] = {
	{ "Svellas (2025)(Inufuto).rom",	9259, 0x3244546f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_svellas, cv_svellas, cv_coleco)
STD_ROM_FN(cv_svellas)

struct BurnDriver BurnDrvcv_svellas = {
	"cv_svellas", NULL, "cv_coleco", NULL, "2025",
	"Svellas (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
	CVGetZipName, cv_svellasRomInfo, cv_svellasRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Sweet Acorn (SGM) (HB)
static struct BurnRomInfo cv_swtacornRomDesc[] = {
    { "Sweet Acorn SGM (2019)(Opcode Games).rom",	32768, 0x19ceb69d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_swtacorn, cv_swtacorn, cv_coleco)
STD_ROM_FN(cv_swtacorn)

struct BurnDriver BurnDrvcv_swtacorn = {
    "cv_swtacorn", NULL, "cv_coleco", NULL, "1984-2019",
    "Sweet Acorn (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Taito Corp.", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_swtacornRomInfo, cv_swtacornRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Sydney Hunter and the Caverns of Death (HB)
static struct BurnRomInfo cv_sydneyhunt2RomDesc[] = {
	{ "Sydney Hunter and the Caverns of Death (2019)(CollectorVision).rom",	131072, 0x81bfb02d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sydneyhunt2, cv_sydneyhunt2, cv_coleco)
STD_ROM_FN(cv_sydneyhunt2)

struct BurnDriver BurnDrvcv_sydneyhunt2 = {
	"cv_sydneyhunt2", NULL, "cv_coleco", NULL, "2019",
	"Sydney Hunter and the Caverns of Death (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_sydneyhunt2RomInfo, cv_sydneyhunt2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Sydney Hunter and the Sacred Tribe (HB)
static struct BurnRomInfo cv_sydneyhuntRomDesc[] = {
	{ "Sydney Hunter and the Sacred Tribe (2017)(CollectorVision).rom",	131072, 0xb5c92637, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_sydneyhunt, cv_sydneyhunt, cv_coleco)
STD_ROM_FN(cv_sydneyhunt)

struct BurnDriver BurnDrvcv_sydneyhunt = {
	"cv_sydneyhunt", NULL, "cv_coleco", NULL, "2017",
	"Sydney Hunter and the Sacred Tribe (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_sydneyhuntRomInfo, cv_sydneyhuntRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Tank Battalion (SGM) (HB)
static struct BurnRomInfo cv_tankbtlnRomDesc[] = {
    { "Tank Battalion SGM (2016)(Opcode Games).rom",	32768, 0x3e4f3045, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tankbtln, cv_tankbtln, cv_coleco)
STD_ROM_FN(cv_tankbtln)

struct BurnDriver BurnDrvcv_tankbtln = {
    "cv_tankbtln", NULL, "cv_coleco", NULL, "1984-2019",
    "Tank Battalion (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Namcot", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
    CVGetZipName, cv_tankbtlnRomInfo, cv_tankbtlnRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Tank Challenge (HB)
static struct BurnRomInfo cv_tankchalRomDesc[] = {
    { "Tank Challenge (2016)(Todd Spangler).rom",	32538, 0xf8eb77d8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tankchal, cv_tankchal, cv_coleco)
STD_ROM_FN(cv_tankchal)

struct BurnDriver BurnDrvcv_tankchal = {
    "cv_tankchal", NULL, "cv_coleco", NULL, "2016",
    "Tank Challenge (HB)\0", "Two players mode only", "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_tankchalRomInfo, cv_tankchalRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Tank Mission (HB)
static struct BurnRomInfo cv_tankmissionRomDesc[] = {
    { "Tank Mission (2017)(CollectorVision).rom",	0x40000, 0xd0f37969, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_tankmission, cv_tankmission, cv_coleco)
STD_ROM_FN(cv_tankmission)

struct BurnDriver BurnDrvcv_tankmission = {
    "cv_tankmission", NULL, "cv_coleco", NULL, "2016-17",
    "Tank Mission (HB)\0", "Use option 1 - Normal", "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
    CVGetZipName, cv_tankmissionRomInfo, cv_tankmissionRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Telebunny (HB)
static struct BurnRomInfo cv_telebunnyRomDesc[] = {
    { "Telebunny (2014)(CollectorVision).rom",	0x08000, 0x26e2a8ec, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_telebunny, cv_telebunny, cv_coleco)
STD_ROM_FN(cv_telebunny)

struct BurnDriver BurnDrvcv_telebunny = {
    "cv_telebunny", NULL, "cv_coleco", NULL, "2014",
    "Telebunny (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MAZE, 0,
    CVGetZipName, cv_telebunnyRomInfo, cv_telebunnyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Terra Attack (HB)
static struct BurnRomInfo cv_terrattakRomDesc[] = {
    { "Terra Attack (2007)(Scott Huggins).rom",	32768, 0x44d527e1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_terrattak, cv_terrattak, cv_coleco)
STD_ROM_FN(cv_terrattak)

struct BurnDriver BurnDrvcv_terrattak = {
    "cv_terrattak", NULL, "cv_coleco", NULL, "2007",
    "Terra Attack (HB)\0", NULL, "Scott Huggins", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_terrattakRomInfo, cv_terrattakRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Thexder (SGM) (HB)
static struct BurnRomInfo cv_thexderRomDesc[] = {
    { "Thexder SGM (2012)(Team Pixelboy).rom",	0x20000, 0x09e3fdda, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_thexder, cv_thexder, cv_coleco)
STD_ROM_FN(cv_thexder)

struct BurnDriver BurnDrvcv_thexder = {
    "cv_thexder", NULL, "cv_coleco", NULL, "1986-2012",
    "Thexder (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Game Arts", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM | GBF_RUNGUN, 0,
    CVGetZipName, cv_thexderRomInfo, cv_thexderRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Time Pilot (SGM) (HB)
static struct BurnRomInfo cv_TimePilotRomDesc[] = {
	{ "Time Pilot SGM (2024)(Opcode Games).rom", 131072, 0xCF803DDC, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(cv_TimePilot, cv_TimePilot, cv_coleco)
STD_ROM_FN(cv_TimePilot)

struct BurnDriver BurnDrvcv_TimePilot = {
	"cv_timepilot", NULL, "cv_coleco", NULL, "1982-2024",
	"Time Pilot (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Konami", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MULTISHOOT, 0,
	CVGetZipName, cv_TimePilotRomInfo, cv_TimePilotRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitOCM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Track & Field (HB)
static struct BurnRomInfo cv_trackfldRomDesc[] = {
    { "Track and Field (2010)(Team Pixelboy).rom",	0x8000, 0x96dd114a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_trackfld, cv_trackfld, cv_coleco)
STD_ROM_FN(cv_trackfld)

struct BurnDriver BurnDrvcv_trackfld = {
    "cv_trackfld", NULL, "cv_coleco", NULL, "1984-2010",
    "Track & Field (HB)\0", "Published by Team Pixelboy", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_SPORTSMISC, 0,
    CVGetZipName, cv_trackfldRomInfo, cv_trackfldRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Traffic Jam (SGM) (HB)
static struct BurnRomInfo cv_trafcjamRomDesc[] = {
    { "Traffic Jam SGM (2017)(Team Pixelboy).rom",	0x08000, 0xaad0f224, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_trafcjam, cv_trafcjam, cv_coleco)
STD_ROM_FN(cv_trafcjam)

struct BurnDriver BurnDrvcv_trafcjam = {
    "cv_trafcjam", NULL, "cv_coleco", NULL, "2016",
    "Traffic Jam (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Imanok", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
    CVGetZipName, cv_trafcjamRomInfo, cv_trafcjamRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Turmoil 2022 (HB)
static struct BurnRomInfo cv_turmoil2022RomDesc[] = {
	{ "Turmoil 2022 (2021)(8bit Milli Games).rom",	32768, 0x3b4af650, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_turmoil2022, cv_turmoil2022, cv_coleco)
STD_ROM_FN(cv_turmoil2022)

struct BurnDriver BurnDrvcv_turmoil2022 = {
	"cv_turmoil2022", NULL, "cv_coleco", NULL, "2021",
	"Turmoil 2022 (HB)\0", NULL, "8 bit Milli Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_turmoil2022RomInfo, cv_turmoil2022RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// TwinBee (SGM) (HB)
static struct BurnRomInfo cv_twinbeeRomDesc[] = {
    { "TwinBee SGM (2014)(Team Pixelboy).rom",	0x20000, 0xe7e07a70, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_twinbee, cv_twinbee, cv_coleco)
STD_ROM_FN(cv_twinbee)

struct BurnDriver BurnDrvcv_twinbee = {
    "cv_twinbee", NULL, "cv_coleco", NULL, "1986-2014",
    "TwinBee (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_twinbeeRomInfo, cv_twinbeeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Txupinazo! (SGM) (HB)
static struct BurnRomInfo cv_TxupinazoRomDesc[] = {
    { "Txupinazo SGM (2017)(Team Pixelboy).rom",	0x8000, 0x28bdf665, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_Txupinazo, cv_Txupinazo, cv_coleco)
STD_ROM_FN(cv_Txupinazo)

struct BurnDriver BurnDrvcv_Txupinazo = {
    "cv_txupinazo", NULL, "cv_coleco", NULL, "2007-2017",
    "Txupinazo! (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Imanok", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_TxupinazoRomInfo, cv_TxupinazoRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Uridium (SGM) (HB)
static struct BurnRomInfo cv_uridiumRomDesc[] = {
    { "Uridium SGM (2019)(Trilobyte).rom",	131072, 0xbc8320a0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_uridium, cv_uridium, cv_coleco)
STD_ROM_FN(cv_uridium)

struct BurnDriver BurnDrvcv_uridium = {
    "cv_uridium", NULL, "cv_coleco", NULL, "2019",
    "Uridium (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Trilobyte", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
    CVGetZipName, cv_uridiumRomInfo, cv_uridiumRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Utopia (HB)
static struct BurnRomInfo cv_utopiaRomDesc[] = {
	{ "Utopia (2023)(Team Pixelboy).rom",	32768, 0x0a90ba65, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_utopia, cv_utopia, cv_coleco)
STD_ROM_FN(cv_utopia)

struct BurnDriver BurnDrvcv_utopia = {
	"cv_utopia", NULL, "cv_coleco", NULL, "1981-2023",
	"Utopia (HB)\0", "Published by Team Pixelboy", "Intellivision Prod.", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_STRATEGY, 0,
	CVGetZipName, cv_utopiaRomInfo, cv_utopiaRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Uwol Quest for Money (HB)
static struct BurnRomInfo cv_uwolRomDesc[] = {
	{ "Uwol Quest for Money (2020)(Cote Gamers).rom",	32768, 0x5f533a05, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_uwol, cv_uwol, cv_coleco)
STD_ROM_FN(cv_uwol)

struct BurnDriver BurnDrvcv_uwol = {
	"cv_uwol", NULL, "cv_coleco", NULL, "2006-2020",
	"Uwol Quest for Money (HB)\0", "Published by Cote Gamers", "The Mojon Twins - Alekmaul", "ColecoVision",
	NULL, L"Published by C\u00f4t\u00e9 Gamers", NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
	CVGetZipName, cv_uwolRomInfo, cv_uwolRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Vanguard (HB)
static struct BurnRomInfo cv_vanguardRomDesc[] = {
	{ "Vanguard (2019)(CollectorVision).rom",	131072, 0xa7a8d25e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_vanguard, cv_vanguard, cv_coleco)
STD_ROM_FN(cv_vanguard)

struct BurnDriver BurnDrvcv_vanguard = {
	"cv_vanguard", NULL, "cv_coleco", NULL, "2019",
	"Vanguard (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_vanguardRomInfo, cv_vanguardRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Vexed (HB, v1.15)
static struct BurnRomInfo cv_vexedRomDesc[] = {
	{ "Vexed v1.15 (2023)(Under4Mhz).rom",	32768, 0x865bae31, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_vexed, cv_vexed, cv_coleco)
STD_ROM_FN(cv_vexed)

struct BurnDriver BurnDrvcv_vexed = {
	"cv_vexed", NULL, "cv_coleco", NULL, "2023",
	"Vexed (HB, v1.15)\0", NULL, "Under4Mhz", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PUZZLE, 0,
	CVGetZipName, cv_vexedRomInfo, cv_vexedRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Void, The (HB)
static struct BurnRomInfo cv_voidRomDesc[] = {
	{ "Void, The (2010)(Atari2600.com).rom",	32768, 0x67311b7e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_void, cv_void, cv_coleco)
STD_ROM_FN(cv_void)

struct BurnDriver BurnDrvcv_void = {
	"cv_void", NULL, "cv_coleco", NULL, "2010",
	"Void, The (HB)\0", NULL, "Atari2600.com", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VERSHOOT, 0,
	CVGetZipName, cv_voidRomInfo, cv_voidRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// War (HB)
static struct BurnRomInfo cv_warRomDesc[] = {
	{ "War (2014)(Gerry Brophy).rom",	31933, 0x7f06e25c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_war, cv_war, cv_coleco)
STD_ROM_FN(cv_war)

struct BurnDriver BurnDrvcv_war = {
	"cv_war", NULL, "cv_coleco", NULL, "2014",
	"War (HB)\0", NULL, "Gerry Brophy", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION, 0,
	CVGetZipName, cv_warRomInfo, cv_warRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Warp & Warp (SGM) (HB)
static struct BurnRomInfo cv_warpwarpRomDesc[] = {
    { "Warp & Warp SGM (2016)(Opcode Games).rom",	32768, 0xb90c2062, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_warpwarp, cv_warpwarp, cv_coleco)
STD_ROM_FN(cv_warpwarp)

struct BurnDriver BurnDrvcv_warpwarp = {
    "cv_warpwarp", NULL, "cv_coleco", NULL, "1984-2016",
    "Warp & Warp (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Namcot", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_warpwarpRomInfo, cv_warpwarpRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Waterville Rescue (HB)
static struct BurnRomInfo cv_watervilRomDesc[] = {
    { "Waterville Rescue (2009)(Mike Brent).rom",	32768, 0xd642fb9e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_watervil, cv_watervil, cv_coleco)
STD_ROM_FN(cv_watervil)

struct BurnDriver BurnDrvcv_watervil = {
    "cv_watervil", NULL, "cv_coleco", NULL, "2009",
    "Waterville Rescue (HB)\0", NULL, "Mike Brent", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_HORSHOOT, 0,
    CVGetZipName, cv_watervilRomInfo, cv_watervilRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Way of the Exploding Foot, The (HB)
static struct BurnRomInfo cv_explodingfootRomDesc[] = {
    { "Way of the Exploding Foot (2011)(CollectorVision).rom",	0x08000, 0x28e07beb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_explodingfoot, cv_explodingfoot, cv_coleco)
STD_ROM_FN(cv_explodingfoot)

struct BurnDriver BurnDrvcv_explodingfoot = {
    "cv_explodingfoot", NULL, "cv_coleco", NULL, "2011",
    "Way of the Exploding Foot, The (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VSFIGHT, 0,
    CVGetZipName, cv_explodingfootRomInfo, cv_explodingfootRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Whack 'Em Smack 'Em Byrons (HB)
static struct BurnRomInfo cv_wsbyronsRomDesc[] = {
    { "Whack 'Em Smack 'Em Byrons (2024)(Jess Creations).rom",	29800, 0xa7bb34df, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wsbyrons, cv_wsbyrons, cv_coleco)
STD_ROM_FN(cv_wsbyrons)

struct BurnDriver BurnDrvcv_wsbyrons = {
    "cv_wsbyrons", NULL, "cv_coleco", NULL, "2024",
    "Whack 'Em Smack 'Em Byrons (HB)\0", NULL, "Jess Creations", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_wsbyronsRomInfo, cv_wsbyronsRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Winky Trap (HB)
static struct BurnRomInfo cv_winktrapRomDesc[] = {
    { "Winky Trap (2007)(Collectorvision Games).rom",	16384, 0xeaa5f606, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_winktrap, cv_winktrap, cv_coleco)
STD_ROM_FN(cv_winktrap)

struct BurnDriver BurnDrvcv_winktrap = {
    "cv_winktrap", NULL, "cv_coleco", NULL, "1997-2007",
    "Winky Trap (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ACTION | GBF_MAZE, 0,
    CVGetZipName, cv_winktrapRomInfo, cv_winktrapRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Wizard of Wor (SGM) (HB)
static struct BurnRomInfo cv_wowRomDesc[] = {
    { "Wizard of Wor SGM (2017)(Team Pixelboy).rom",	0x80000, 0xd9207f30, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wow, cv_wow, cv_coleco)
STD_ROM_FN(cv_wow)

struct BurnDriver BurnDrvcv_wow = {
    "cv_wow", NULL, "cv_coleco", NULL, "1980-2017",
    "Wizard of Wor (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Midway", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_MAZE | GBF_RUNGUN, 0,
    CVGetZipName, cv_wowRomInfo, cv_wowRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Wonder Boy (SGM) (HB)
static struct BurnRomInfo cv_wonderboyRomDesc[] = {
    { "Wonder Boy SGM (2012)(Team PixelBoy).rom",	0x8000, 0x43505be0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_wonderboy, cv_wonderboy, cv_coleco)
STD_ROM_FN(cv_wonderboy)

struct BurnDriver BurnDrvcv_wonderboy = {
    "cv_wonderboy", NULL, "cv_coleco", NULL, "1986-2012",
    "Wonder Boy (SGM) (HB)\0", "Published by Team Pixelboy", "Eduardo Mello - Sega", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_wonderboyRomInfo, cv_wonderboyRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Yar's Revenge (HB)
static struct BurnRomInfo cv_yarsrevengeRomDesc[] = {
    { "Yar's Revenge (2018)(CollectorVision).rom",	32727, 0xf0bae4a1, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_yarsrevenge, cv_yarsrevenge, cv_coleco)
STD_ROM_FN(cv_yarsrevenge)

struct BurnDriver BurnDrvcv_yarsrevenge = {
    "cv_yarsrevenge", NULL, "cv_coleco", NULL, "2018",
    "Yar's Revenge (HB)\0", NULL, "CollectorVision Games", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_SHOOT, 0,
    CVGetZipName, cv_yarsrevengeRomInfo, cv_yarsrevengeRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Yewdow (HB)
static struct BurnRomInfo cv_yewdowRomDesc[] = {
	{ "Yewdow (2023)(Inufuto).rom",	16384, 0x706895db, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_yewdow, cv_yewdow, cv_coleco)
STD_ROM_FN(cv_yewdow)

struct BurnDriver BurnDrvcv_yewdow = {
	"cv_yewdow", NULL, "cv_coleco", NULL, "2023",
	"Yewdow (HB)\0", NULL, "Inufuto", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_MAZE | GBF_ACTION, 0,
	CVGetZipName, cv_yewdowRomInfo, cv_yewdowRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Yie Ar Kung Fu (HB)
static struct BurnRomInfo cv_yiearRomDesc[] = {
    { "Yie Ar Kung Fu (2005)(Opcode Games).rom",	32768, 0x471240bb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_yiear, cv_yiear, cv_coleco)
STD_ROM_FN(cv_yiear)

struct BurnDriver BurnDrvcv_yiear = {
    "cv_yiear", NULL, "cv_coleco", NULL, "1985-2005",
    "Yie Ar Kung Fu (HB)\0", NULL, "Opcode Games - Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_VSFIGHT, 0,
    CVGetZipName, cv_yiearRomInfo, cv_yiearRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Yie Ar Kung Fu II (SGM) (HB)
static struct BurnRomInfo cv_yieariiRomDesc[] = {
    { "Yie Ar Kung Fu II SGM (2018)(Opcode Games).rom",	131072, 0x3143c6dd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_yiearii, cv_yiearii, cv_coleco)
STD_ROM_FN(cv_yiearii)

struct BurnDriver BurnDrvcv_yiearii = {
    "cv_yiearii", NULL, "cv_coleco", NULL, "1985-2018",
    "Yie Ar Kung Fu II (SGM) (HB)\0", "SGM - NB: the game resets randomly!", "Opcode Games - Konami", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_NOT_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_SCRFIGHT | GBF_VSFIGHT, 0,
    CVGetZipName, cv_yieariiRomInfo, cv_yieariiRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Xyzolog (SGM) (HB)
static struct BurnRomInfo cv_xyzologRomDesc[] = {
    { "Xyzolog SGM (2019)(Opcode Games).rom",	32768, 0x95ff75e8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_xyzolog, cv_xyzolog, cv_coleco)
STD_ROM_FN(cv_xyzolog)

struct BurnDriver BurnDrvcv_xyzolog = {
    "cv_xyzolog", NULL, "cv_coleco", NULL, "1984-2019",
    "Xyzolog (SGM) (HB)\0", "SGM - Super Game Module", "Opcode Games - Taito Corp.", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_ACTION, 0,
    CVGetZipName, cv_xyzologRomInfo, cv_xyzologRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Zanac (SGM) (HB)
static struct BurnRomInfo cv_zanacRomDesc[] = {
    { "Zanac SGM (2015)(CollectorVision).rom",	131072, 0xe290a941, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zanac, cv_zanac, cv_coleco)
STD_ROM_FN(cv_zanac)

struct BurnDriver BurnDrvcv_zanac = {
    "cv_zanac", NULL, "cv_coleco", NULL, "1986-2015",
    "Zanac (SGM) (HB)\0", "SGM - Published by CollectorVision Games", "Compile", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_VERSHOOT, 0,
    CVGetZipName, cv_zanacRomInfo, cv_zanacRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Zaxxon II (HB)
static struct BurnRomInfo cv_zaxxon2RomDesc[] = {
	{ "Zaxxon II (2021)(Team Pixelboy).rom",	32768, 0x762e9a2b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zaxxon2, cv_zaxxon2, cv_coleco)
STD_ROM_FN(cv_zaxxon2)

struct BurnDriver BurnDrvcv_zaxxon2 = {
	"cv_zaxxon2", NULL, "cv_coleco", NULL, "1985-2021",
	"Zaxxon II (HB)\0", "Published by Team Pixelboy", "Mystery Man - Sega", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_zaxxon2RomInfo, cv_zaxxon2RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Zaxxon Super Game (SGM) (HB)
static struct BurnRomInfo cv_zaxxonsgmRomDesc[] = {
	{ "Zaxxon Super Game SGM (2012)(Team Pixelboy).rom",	0x020000, 0xa5a90f63, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zaxxonsgm, cv_zaxxonsgm, cv_coleco)
STD_ROM_FN(cv_zaxxonsgm)

struct BurnDriver BurnDrvcv_zaxxonsgm = {
	"cv_zaxxonsgm", NULL, "cv_coleco", NULL, "1984-2012",
	"Zaxxon Super Game (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "Coleco", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_HORSHOOT, 0,
	CVGetZipName, cv_zaxxonsgmRomInfo, cv_zaxxonsgmRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Zippy Race (HB)
static struct BurnRomInfo cv_zippyracRomDesc[] = {
    { "Zippy Race (2009)(Opcode Games).rom",	32768, 0x44e6948c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zippyrac, cv_zippyrac, cv_coleco)
STD_ROM_FN(cv_zippyrac)

struct BurnDriver BurnDrvcv_zippyrac = {
    "cv_zippyrac", NULL, "cv_coleco", NULL, "1983-2009",
    "Zippy Race (HB)\0", "Published by CollectorVision Games", "Dvik & Joyrex", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_RACING, 0,
    CVGetZipName, cv_zippyracRomInfo, cv_zippyracRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Zombie Calavera Prologue (HB)
static struct BurnRomInfo cv_zcalaveraRomDesc[] = {
	{ "Zombie Calavera Prologue (2021)(Collectorvision).rom",	32768, 0x91645814, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zcalavera, cv_zcalavera, cv_coleco)
STD_ROM_FN(cv_zcalavera)

struct BurnDriver BurnDrvcv_zcalavera = {
	"cv_zcalavera", NULL, "cv_coleco", NULL, "2021",
	"Zombie Calavera Prologue (HB)\0", "Original game by 'The Mojon Twins'", "CollectorVision Games", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_RUNGUN | GBF_PLATFORM, 0,
	CVGetZipName, cv_zcalaveraRomInfo, cv_zcalaveraRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

// Zombie Incident (SGM) (HB)
static struct BurnRomInfo cv_zombieincRomDesc[] = {
    { "Zombie Incident (2018)(Team Pixelboy).rom",	0x20000, 0x8027dad7, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zombieinc, cv_zombieinc, cv_coleco)
STD_ROM_FN(cv_zombieinc)

struct BurnDriver BurnDrvcv_zombieinc = {
    "cv_zombieinc", NULL, "cv_coleco", NULL, "2012-2018",
    "Zombie Incident (SGM) (HB)\0", "SGM - Published by Team Pixelboy", "NeneFranz", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_COLECO, GBF_PLATFORM, 0,
    CVGetZipName, cv_zombieincRomInfo, cv_zombieincRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInitSGM, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Zombie Near (HB)
static struct BurnRomInfo cv_zombnearRomDesc[] = {
    { "Zombie Near (2012)(CollectorVision).rom",	0x8000, 0xc36c017e, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zombnear, cv_zombnear, cv_coleco)
STD_ROM_FN(cv_zombnear)

struct BurnDriver BurnDrvcv_zombnear = {
    "cv_zombnear", NULL, "cv_coleco", NULL, "2012",
    "Zombie Near (HB)\0", "Published by CollectorVision Games", "Oscar Toledo G.", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ADV | GBF_RUNGUN, 0,
    CVGetZipName, cv_zombnearRomInfo, cv_zombnearRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Zombie Near (Alt) (HB)
// see year and http://www.colecovision.dk/oscar-toledo.htm
// different title screen, different palette, character is faster, etc.
static struct BurnRomInfo cv_zombnearbRomDesc[] = {
    { "Zombie Near (2011)(CollectorVision).rom",	0x20000, 0xc89d281d, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zombnearb, cv_zombnearb, cv_coleco)
STD_ROM_FN(cv_zombnearb)

struct BurnDriver BurnDrvcv_zombnearb = {
    "cv_zombnearb", "cv_zombnear", "cv_coleco", NULL, "2011",
    "Zombie Near (Alt) (HB)\0", "Published by CollectorVision Games", "Oscar Toledo G.", "ColecoVision",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_ADV | GBF_RUNGUN, 0,
    CVGetZipName, cv_zombnearbRomInfo, cv_zombnearbRomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
    DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
    272, 228, 4, 3
};

// Zoom 909 (HB)
static struct BurnRomInfo cv_zoom909RomDesc[] = {
	{ "Zoom 909 (2021)(Team Pixelboy).rom",	32768, 0x7dc7a049, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(cv_zoom909, cv_zoom909, cv_coleco)
STD_ROM_FN(cv_zoom909)

struct BurnDriver BurnDrvcv_zoom909 = {
	"cv_zoom909", NULL, "cv_coleco", NULL, "1985-2021",
	"Zoom 909 (HB)\0", "Published by Team Pixelboy", "Mystery Man - Sega", "ColecoVision",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_COLECO, GBF_SHOOT, 0,
	CVGetZipName, cv_zoom909RomInfo, cv_zoom909RomName, NULL, NULL, NULL, NULL, ColecoInputInfo, ColecoDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 228, 4, 3
};

