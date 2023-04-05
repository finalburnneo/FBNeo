// FB Neo Pacific Novelty games driver module
// Based on MAME driver by Victor Trucco, Mike Balfour, and Phil Stroffolino

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "samples.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvCopROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 tms_reset;
static INT32 read_mask;
static INT32 write_mask;
static INT32 video_control;
static INT32 input_select;

struct coprocessor_t {
	UINT8 *context_ram;
	UINT8 *image_ram;
	UINT8 bank;
	UINT8 param[0x9];
};

static coprocessor_t coprocessor;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo SharkattInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sharkatt)

static struct BurnInputInfo ThiefInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Thief)

static struct BurnInputInfo NatodefInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Natodef)

static struct BurnDIPInfo SharkattDIPList[]=
{
	{0x11, 0xff, 0xff, 0x7f, NULL					},
	{0x12, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Coinage"				},
	{0x11, 0x01, 0x7f, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x7f, 0x7f, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    3, "Lives"				},
	{0x12, 0x01, 0x03, 0x00, "3"					},
	{0x12, 0x01, 0x03, 0x01, "4"					},
	{0x12, 0x01, 0x03, 0x02, "5"					},
};

STDDIPINFO(Sharkatt)

static struct BurnDIPInfo ThiefDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x00, NULL					},
	{0x0e, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0d, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x0d, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0d, 0x01, 0x03, 0x03, "2 Coins 3 Credits"	},
	{0x0d, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0d, 0x01, 0x0c, 0x00, "3"					},
	{0x0d, 0x01, 0x0c, 0x04, "4"					},
	{0x0d, 0x01, 0x0c, 0x08, "5"					},
	{0x0d, 0x01, 0x0c, 0x0c, "7"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0d, 0x01, 0x40, 0x00, "Upright"				},
	{0x0d, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Unused"				},
	{0x0d, 0x01, 0x80, 0x00, "No"					},
	{0x0d, 0x01, 0x80, 0x80, "Yes"					},

	{0   , 0xfe, 0   ,    9, "Bonus Life"			},
	{0x0e, 0x01, 0x0f, 0x0c, "10K"					},
	{0x0e, 0x01, 0x0f, 0x0d, "20K"					},
	{0x0e, 0x01, 0x0f, 0x0e, "30K"					},
	{0x0e, 0x01, 0x0f, 0x0f, "40K"					},
	{0x0e, 0x01, 0x0f, 0x08, "10K 10K"				},
	{0x0e, 0x01, 0x0f, 0x09, "20K 20K"				},
	{0x0e, 0x01, 0x0f, 0x0a, "30K 30K"				},
	{0x0e, 0x01, 0x0f, 0x0b, "40K 40K"				},
	{0x0e, 0x01, 0x0f, 0x00, "None"					},

	{0   , 0xfe, 0   ,    10, "Mode"				},
	{0x0e, 0x01, 0xf0, 0x00, "Normal"				},
	{0x0e, 0x01, 0xf0, 0x70, "Display Options"		},
	{0x0e, 0x01, 0xf0, 0x80, "Burn-in Test"			},
	{0x0e, 0x01, 0xf0, 0x90, "Color Bar Test"		},
	{0x0e, 0x01, 0xf0, 0xa0, "Cross Hatch"			},
	{0x0e, 0x01, 0xf0, 0xb0, "Color Map"			},
	{0x0e, 0x01, 0xf0, 0xc0, "VIDSEL Test"			},
	{0x0e, 0x01, 0xf0, 0xd0, "VIDBIT Test"			},
	{0x0e, 0x01, 0xf0, 0xe0, "I/O Board Test"		},
	{0x0e, 0x01, 0xf0, 0xf0, "Reserved"				},
};

STDDIPINFO(Thief)

static struct BurnDIPInfo NatodefDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x00, NULL					},
	{0x10, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0f, 0x01, 0x03, 0x01, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x03, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x03, 0x03, "2 Coins 3 Credits"	},
	{0x0f, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0f, 0x01, 0x0c, 0x00, "3"					},
	{0x0f, 0x01, 0x0c, 0x04, "4"					},
	{0x0f, 0x01, 0x0c, 0x08, "5"					},
	{0x0f, 0x01, 0x0c, 0x0c, "7"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x0f, 0x01, 0x30, 0x00, "Easy"					},
	{0x0f, 0x01, 0x30, 0x10, "Medium"				},
	{0x0f, 0x01, 0x30, 0x20, "Hard"					},
	{0x0f, 0x01, 0x30, 0x30, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0f, 0x01, 0x40, 0x00, "Upright"				},
	{0x0f, 0x01, 0x40, 0x40, "Cocktail"				},

	{0   , 0xfe, 0   ,    2, "Add a Coin?"			},
	{0x0f, 0x01, 0x80, 0x00, "No"					},
	{0x0f, 0x01, 0x80, 0x80, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x10, 0x01, 0x04, 0x04, "Off"					},
	{0x10, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    5, "Bonus Life"			},
	{0x10, 0x01, 0x0b, 0x08, "10K"					},
	{0x10, 0x01, 0x0b, 0x09, "20K"					},
	{0x10, 0x01, 0x0b, 0x0a, "30K"					},
	{0x10, 0x01, 0x0b, 0x0b, "40K"					},
	{0x10, 0x01, 0x0b, 0x00, "None"					},

	{0   , 0xfe, 0   ,    10, "Mode"				},
	{0x10, 0x01, 0xf0, 0x00, "Normal"				},
	{0x10, 0x01, 0xf0, 0x70, "Display Options"		},
	{0x10, 0x01, 0xf0, 0x80, "Burn-in Test"			},
	{0x10, 0x01, 0xf0, 0x90, "Color Bar Test"		},
	{0x10, 0x01, 0xf0, 0xa0, "Cross Hatch"			},
	{0x10, 0x01, 0xf0, 0xb0, "Color Map"			},
	{0x10, 0x01, 0xf0, 0xc0, "VIDSEL Test"			},
	{0x10, 0x01, 0xf0, 0xd0, "VIDBIT Test"			},
	{0x10, 0x01, 0xf0, 0xe0, "I/O Board Test"		},
	{0x10, 0x01, 0xf0, 0xf0, "Reserved"				},
};

STDDIPINFO(Natodef)

enum {
	IMAGE_ADDR_LO,      //0xe000
	IMAGE_ADDR_HI,      //0xe001
	SCREEN_XPOS,        //0xe002
	SCREEN_YPOS,        //0xe003
	BLIT_WIDTH,         //0xe004
	BLIT_HEIGHT,        //0xe005
	GFX_PORT,           //0xe006
	BARL_PORT,          //0xe007
	BLIT_ATTRIBUTES     //0xe008
};

static UINT16 fetch_image_addr(coprocessor_t &thief_coprocessor)
{
	INT32 addr = thief_coprocessor.param[IMAGE_ADDR_LO]+256*thief_coprocessor.param[IMAGE_ADDR_HI];

	thief_coprocessor.param[IMAGE_ADDR_LO]++;

	if (thief_coprocessor.param[IMAGE_ADDR_LO] == 0x00)
	{
		thief_coprocessor.param[IMAGE_ADDR_HI]++;
	}

	return addr;
}

static void blit_write(UINT8 data)
{
	INT32 offs, xoffset, dy;
	coprocessor_t &thief_coprocessor = coprocessor;
	UINT8 x = thief_coprocessor.param[SCREEN_XPOS];
	UINT8 y = thief_coprocessor.param[SCREEN_YPOS];
	UINT8 width = thief_coprocessor.param[BLIT_WIDTH];
	UINT8 height = thief_coprocessor.param[BLIT_HEIGHT];
	UINT8 attributes = thief_coprocessor.param[BLIT_ATTRIBUTES];

	UINT8 old_data;
	INT32 xor_blit = data;

	x -= width*8;
	xoffset = x&7;

	if (attributes & 0x10)
	{
		y += 7-height;
		dy = 1;
	}
	else
	{
		dy = -1;
	}

	height++;

	while (height--)
	{
		for (INT32 i = 0; i <= width; i++)
		{
			INT32 addr = fetch_image_addr(thief_coprocessor);

			if (addr < 0x2000)
			{
				data = thief_coprocessor.image_ram[addr];
			}
			else
			{
				addr -= 0x2000;
				if (addr<0x2000*3) data = DrvGfxROM[addr];
			}

			offs = (y*32+x/8+i)&0x1fff;
			old_data = ZetReadByte(0xc000 + offs);

			if (xor_blit)
			{
				ZetWriteByte(0xc000 + offs, old_data^(data>>xoffset));
			}
			else
			{
				ZetWriteByte(0xc000 + offs, (old_data&(0xff00>>xoffset)) | (data>>xoffset));
			}

			offs = (offs+1)&0x1fff;
			old_data = ZetReadByte(0xc000 + offs);

			if (xor_blit)
			{
				ZetWriteByte(0xc000 + offs, old_data^((data<<(8-xoffset))&0xff) );
			}
			else
			{
				ZetWriteByte(0xc000 + offs, (old_data&(0xff>>xoffset)) | ((data<<(8-xoffset))&0xff));
			}
		}

		y+=dy;
	}
}

static UINT8 coprocessor_read(UINT8 offset)
{
	coprocessor_t &thief_coprocessor = coprocessor;

	switch (offset)
	{
		case SCREEN_XPOS: /* xpos */
		case SCREEN_YPOS: /* ypos */
		{
			INT32 addr = thief_coprocessor.param[SCREEN_XPOS]+256*thief_coprocessor.param[SCREEN_YPOS];
			INT32 result = 0xc000 | (addr >> 3);
			return (offset==0x03) ? (result >> 8) : result;
		}
		break;

		case GFX_PORT:
		{
			INT32 addr = fetch_image_addr(thief_coprocessor);
			if (addr < 0x2000)
			{
				return thief_coprocessor.image_ram[addr];
			}
			else
			{
				addr -= 0x2000;
				if( addr<0x6000 ) return DrvGfxROM[addr];
			}
		}
		break;

		case BARL_PORT:
		{
			INT32 dx = thief_coprocessor.param[SCREEN_XPOS] & 0x7;

			if (thief_coprocessor.param[BLIT_ATTRIBUTES] & 0x01)
			{
				return 0x01<<dx; // flipx
			}
			else
			{
				return 0x80>>dx; // no flip
			}
		}
		break;
	}

	return thief_coprocessor.param[offset];
}

static void coprocessor_write(UINT8 offset, UINT8 data)
{
	coprocessor_t &thief_coprocessor = coprocessor;

	switch (offset)
	{
		case GFX_PORT:
		{
			INT32 addr = fetch_image_addr(thief_coprocessor);

			if (addr < 0x2000)
			{
				thief_coprocessor.image_ram[addr] = data;
			}
		}
		break;

		default:
			thief_coprocessor.param[offset] = data;
		break;
	}
}

static inline void tape_set_audio(INT32 track, INT32 bOn)
{
	BurnSampleSetAllRoutes(track, bOn ? 0.65 : 0.0, BURN_SND_ROUTE_BOTH);
}

static inline void tape_set_motor(INT32 data)
{
	if (data)
	{
		for (INT32 i = 0; i < 2; i++)
		{
			switch (BurnSampleGetStatus(i)) {
				case SAMPLE_PAUSED:
					BurnSampleResume(i);
					break;
				case SAMPLE_STOPPED:
					BurnSamplePlay(i);
					break;
			}
		}
	}
	else
	{
		BurnSamplePause(0);
		BurnSamplePause(1);
	}
}

static void tape_control_write(UINT8 data)
{
	switch (data)
	{
		case 0x02: // coin meter on
		case 0x03: // nop
		case 0x04: // coin meter off
		break;

		case 0x08: // talk track on
		case 0x09: // talk track off
			tape_set_audio(0, ~data & 1 );
		break;

		case 0x0a: // motor on
		case 0x0b: // motor off
			tape_set_motor(~data & 1);
		break;

		case 0x0c: // crash track on
		case 0x0d: // crash track off
			tape_set_audio(1, ~data & 1 );
		break;
	}
}

static void __fastcall thief_write(UINT16 address, UINT8 data)
{
	if (address == 0x0000) {
		blit_write(data);
		return;
	}

	if ((address & 0xe000) == 0xc000) {
		UINT8 *ram = DrvVidRAM + ((video_control & 2) * 0x4000) + (address & 0x1fff);
		if (write_mask & 1) ram[0x0000] = data;
		if (write_mask & 2) ram[0x2000] = data;
		if (write_mask & 4) ram[0x4000] = data;
		if (write_mask & 8) ram[0x6000] = data;
		return;
	}

	if (address >= 0xe000 && address <= 0xe008) {
		coprocessor_write(address & 0xf, data);
		return;
	}

	if ((address & 0xffc0) == 0xe080) {
		coprocessor.context_ram[(0x40 * coprocessor.bank) + (address & 0x3f)] = data;
		return;
	}

	if (address == 0xe0c0) {
		coprocessor.bank = data & 0xf;
		return;
	}
}

static UINT8 __fastcall thief_read(UINT16 address)
{
	if ((address & 0xe000) == 0xc000) {
		UINT8 *ram = DrvVidRAM + ((video_control & 2) * 0x4000) + (address & 0x1fff);
		return ram[read_mask * 0x2000];
	}

	if (address >= 0xe010 && address <= 0xe02f) {
		return DrvZ80ROM[address];
	}

	if (address >= 0xe000 && address <= 0xe008) {
		return coprocessor_read(address & 0xf);
	}

	if ((address & 0xffc0) == 0xe080) {
		return coprocessor.context_ram[(0x40 * coprocessor.bank) + (address & 0x3f)];
	}

	return 0;
}

static void palette_write(UINT8 offset, UINT8 data)
{
	INT32 r = ((data >> 0) & 0x01) * 0x55 + ((data >> 1) & 0x01) * 0xAA;
	INT32 g = ((data >> 2) & 0x01) * 0x55 + ((data >> 3) & 0x01) * 0xAA;
	INT32 b = ((data >> 4) & 0x01) * 0x55 + ((data >> 5) & 0x01) * 0xAA;

	DrvPalette[offset] = BurnHighCol(r,g,b,0);
}

static void __fastcall thief_write_port(UINT16 port, UINT8 data)
{
	if ((port & 0xf0) == 0x60)
	{
		switch (port & 0xf)
		{
			case 0x07:
			case 0x0f:
				tms_reset = ~port & 8;
			return;

			case 0x0a:
				if (!tms_reset) tms_reset = 1;
			return;

			case 0x0e:
				if ( tms_reset) tms_reset = 0;
			return;
		}

		return;
	}

	if ((port & 0xf0) == 0x70) {
		DrvPalRAM[port & 0xf] = data;
		palette_write(port & 0xf, data);
		return;
	}

	switch (port & 0xff)
	{
		case 0x00:
			BurnWatchdogWrite();
		return;

		case 0x10:
			video_control = data;
		return;

		case 0x30:
			input_select = data;
		return;

		case 0x33:
			tape_control_write(data);
		return;

		case 0x40:
		case 0x41:
			AY8910Write(0, port & 1, data);
		return;

		case 0x42:
		case 0x43:
			AY8910Write(1, port & 1, data);
		return;

		case 0x50:
			write_mask = data & 0x0f;
			read_mask = (data & 0x30) >> 4;
		return;
	}
}

static UINT8 thief_io_read()
{
	switch (input_select)
	{
		case 0x01: return DrvDips[0];
		case 0x02: return DrvDips[1];
		case 0x04: return DrvInputs[0];
		case 0x08: return DrvInputs[1];
	}

	return 0;
}

static UINT8 __fastcall thief_read_port(UINT16 port)
{
	if ((port & 0xf0) == 0x60) {
		thief_write_port(port & 0xf, 0);
		return 0; // tms9927
	}

	switch (port & 0xff)
	{
		case 0x31:
			return thief_io_read();

		case 0x41:
			return AY8910Read(0);

		case 0x43:
			return AY8910Read(1);
	}

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	BurnSampleReset();
	ZetClose();

	BurnWatchdogReset();

	AY8910Reset(0);
	AY8910Reset(1);

	tms_reset = 1;
	read_mask = 0;
	write_mask = 0;
	video_control = 0;
	input_select = 0;

	coprocessor.bank = 0;
	memset (coprocessor.param, 0, sizeof(coprocessor.param));

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM				= Next; Next += 0x010000;
	DrvCopROM				= Next; Next += 0x000400;

	DrvGfxROM				= Next; Next += 0x006000;

	DrvPalette				= (UINT32*)Next; Next += 0x0011 * sizeof(UINT32);

	AllRam					= Next;

	DrvPalRAM				= Next; Next += 0x000010;
	DrvZ80RAM				= Next; Next += 0x001000;
	DrvVidRAM				= Next; Next += 0x010000;

	coprocessor.image_ram	= Next; Next += 0x002000;
	coprocessor.context_ram	= Next; Next += 0x000400;

	RamEnd					= Next;

	MemEnd					= Next;

	return 0;
}

static INT32 DrvInit(INT32 game)
{
	BurnAllocMemIndex();

	INT32 k = 0;
	if (game == 0) // sharkatt
	{
		if (BurnLoadRom(DrvZ80ROM + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x0800, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1800, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2800, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x3000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x3800, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x4000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x4800, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x5000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x5800, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x6000, k++, 1)) return 1;
	}
	else if (game == 1) // thief
	{
		if (BurnLoadRom(DrvZ80ROM + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x3000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x4000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x5000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x6000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x7000, k++, 1)) return 1;

		if (BurnLoadRom(DrvCopROM + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvCopROM + 0x0200, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000, k++, 2)) return 1;
	}
	else if (game == 2) // natodef
	{
		if (BurnLoadRom(DrvZ80ROM + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x3000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x4000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x5000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x6000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x7000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0xa000, k++, 1)) return 1;

		if (BurnLoadRom(DrvCopROM + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvCopROM + 0x0200, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x0001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x2001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x2000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x4001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x4000, k++, 2)) return 1;
	}

	memcpy (DrvZ80ROM + 0xe010, DrvCopROM + 0x290, 0x20);

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0xa000,	0xa000, 0xafff, MAP_ROM);
//	ZetMapMemory(DrvVidRAM,			0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0xe000,	0xe000, 0xe0ff, MAP_FETCH);
	ZetSetWriteHandler(thief_write);
	ZetSetReadHandler(thief_read);
	ZetSetOutHandler(thief_write_port);
	ZetSetInHandler(thief_read_port);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, -1); // disable for now

	AY8910Init(0, 2000000, 1);
	AY8910Init(1, 2000000, 1);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 4000000);

	BurnSampleInit(0);
	BurnSampleSetBuffered(ZetTotalCycles, 4000000);
	BurnSampleSetAllRoutesAllSamples(0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);
	BurnSampleExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_bitmap()
{
	INT32 flipscreen = video_control & 1;

	UINT8 *source = DrvVidRAM + (video_control & 4) * 0x2000;

	if (tms_reset) { // fill w/black
		BurnTransferClear(0x10);
		return;
	}

	for (INT32 offs = 0; offs < 0x2000; offs++)
	{
		INT32 ypos = offs/32;
		INT32 xpos = (offs&0x1f)*8;
		INT32 p0 = source[0x2000*0+offs];
		INT32 p1 = source[0x2000*1+offs];
		INT32 p2 = source[0x2000*2+offs];
		INT32 p3 = source[0x2000*3+offs];

		if (flipscreen)
		{
			if (ypos < (256 - nScreenHeight)) {
				offs |= 0x1f;
				continue;
			}

			for (INT32 bit = 0; bit < 8; bit++)
			{
				INT32 pixel = (((p0<<bit)&0x80)>>7) | (((p1<<bit)&0x80)>>6) | (((p2<<bit)&0x80)>>5) | (((p3<<bit)&0x80)>>4);
				pTransDraw[(0xff - ypos) * nScreenWidth + (0xff - (xpos + bit))] = pixel;
			}
		}
		else
		{
			if (ypos >= nScreenHeight) break;

			for (INT32 bit = 0; bit < 8; bit++)
			{
				INT32 pixel = (((p0<<bit)&0x80)>>7) | (((p1<<bit)&0x80)>>6) | (((p2<<bit)&0x80)>>5) | (((p3<<bit)&0x80)>>4);
				pTransDraw[ypos * nScreenWidth + xpos + bit] = pixel;
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x10; i++) {
			palette_write(i, DrvPalRAM[i]);
		}
		DrvPalette[0x10] = 0; // black
		DrvRecalc = 0;
	}

	draw_bitmap();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 32;
	INT32 nCyclesTotal[1] = { 4000000 / 60 };
	INT32 nCyclesDone[1] =  { 0 };

	ZetOpen(0);
	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Zet);
	}
	if (DrvInputs[1] & 0x10)
		ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	else
		ZetNmi();
	ZetClose();

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

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

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
		BurnSampleScan(nAction, pnMin);

		SCAN_VAR(tms_reset);
		SCAN_VAR(read_mask);
		SCAN_VAR(write_mask);
		SCAN_VAR(video_control);
		SCAN_VAR(input_select);
		SCAN_VAR(coprocessor.bank);
		SCAN_VAR(coprocessor.param); // 9
	}

	return 0;
}

// all sets use same names for samples
static struct BurnSampleInfo DrvSampleDesc[] = {
	{ "talk",	SAMPLE_AUTOLOOP },
	{ "crash",	SAMPLE_AUTOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Drv)
STD_SAMPLE_FN(Drv)


// Shark Attack

static struct BurnRomInfo sharkattRomDesc[] = {
	{ "sharkatt.0",		0x0800, 0xc71505e9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "sharkatt.1",		0x0800, 0x3e3abf70, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sharkatt.2",		0x0800, 0x96ded944, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sharkatt.3",		0x0800, 0x007283ae, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sharkatt.4a",	0x0800, 0x5cb114a7, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sharkatt.5",		0x0800, 0x1d88aaad, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sharkatt.6",		0x0800, 0xc164bad4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sharkatt.7",		0x0800, 0xd78c4b8b, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "sharkatt.8",		0x0800, 0x5958476a, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "sharkatt.9",		0x0800, 0x4915eb37, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "sharkatt.10",	0x0800, 0x9d07cb68, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "sharkatt.11",	0x0800, 0x21edc962, 1 | BRF_PRG | BRF_ESS }, // 11
	{ "sharkatt.12a",	0x0800, 0x5dd8785a, 1 | BRF_PRG | BRF_ESS }, // 12
};

STD_ROM_PICK(sharkatt)
STD_ROM_FN(sharkatt)

static INT32 SharkattInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvSharkatt = {
	"sharkatt", NULL, NULL, "sharkatt", "1980",
	"Shark Attack\0", NULL, "Pacific Novelty", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, sharkattRomInfo, sharkattRomName, NULL, NULL, DrvSampleInfo, DrvSampleName, SharkattInputInfo, SharkattDIPInfo,
	SharkattInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 192, 4, 3
};


// Thief

static struct BurnRomInfo thiefRomDesc[] = {
	{ "t8a0ah0a",		0x1000, 0xedbbf71c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "t2662h2",		0x1000, 0x85b4f6ff, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tc162h4",		0x1000, 0x70478a82, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "t0cb4h6",		0x1000, 0x29de0425, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tc707h8",		0x1000, 0xea8dd847, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "t857bh10",		0x1000, 0x403c33b7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "t606bh12",		0x1000, 0x4ca2748b, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "tae4bh14",		0x1000, 0x22e7dcc3, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "b8",				0x0200, 0xfe865b2a, 2 | BRF_PRG | BRF_ESS }, //  8 Coprocessor Code
	{ "c8",				0x0200, 0x7ed5c923, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "t079ahd4",		0x1000, 0x928bd8ef, 3 | BRF_GRA },           // 10 Blitter Graphics
	{ "tdda7hh4",		0x1000, 0xb48f0862, 3 | BRF_GRA },           // 11
};

STD_ROM_PICK(thief)
STD_ROM_FN(thief)

static INT32 ThiefInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvThief = {
	"thief", NULL, NULL, "thief", "1981",
	"Thief\0", NULL, "Pacific Novelty", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, thiefRomInfo, thiefRomName, NULL, NULL, DrvSampleInfo, DrvSampleName, ThiefInputInfo, ThiefDIPInfo,
	ThiefInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 256, 4, 3
};


// NATO Defense

static struct BurnRomInfo natodefRomDesc[] = {
	{ "natodef.cp0",	0x1000, 0x8397c787, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "natodef.cp2",	0x1000, 0x8cfbf26f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "natodef.cp4",	0x1000, 0xb4c90fb2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "natodef.cp6",	0x1000, 0xc6d0d35e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "natodef.cp8",	0x1000, 0xe4b6c21e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "natodef.cpa",	0x1000, 0x888ecd42, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "natodef.cpc",	0x1000, 0xcf713bc9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "natodef.cpe",	0x1000, 0x4eef6bf4, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "natodef.cp5",	0x1000, 0x65c3601b, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "b8",				0x0200, 0xfe865b2a, 2 | BRF_PRG | BRF_ESS }, //  9 Coprocessor Code
	{ "c8",				0x0200, 0x7ed5c923, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "natodef.o4",		0x1000, 0x39a868f8, 3 | BRF_GRA },           // 11 Blitter Graphics
	{ "natodef.e1",		0x1000, 0xb6d1623d, 3 | BRF_GRA },           // 12
	{ "natodef.o2",		0x1000, 0x77cc9cfd, 3 | BRF_GRA },           // 13
	{ "natodef.e3",		0x1000, 0x5302410d, 3 | BRF_GRA },           // 14
	{ "natodef.o3",		0x1000, 0xb217909a, 3 | BRF_GRA },           // 15
	{ "natodef.e2",		0x1000, 0x886c3f05, 3 | BRF_GRA },           // 16
};

STD_ROM_PICK(natodef)
STD_ROM_FN(natodef)

static INT32 NatodefInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvNatodef = {
	"natodef", NULL, NULL, "natodef", "1982",
	"NATO Defense\0", NULL, "Pacific Novelty", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, natodefRomInfo, natodefRomName, NULL, NULL, DrvSampleInfo, DrvSampleName, NatodefInputInfo, NatodefDIPInfo,
	NatodefInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 256, 4, 3
};


// NATO Defense (alternate mazes)

static struct BurnRomInfo natodefaRomDesc[] = {
	{ "natodef.cp0",	0x1000, 0x8397c787, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "natodef.cp2",	0x1000, 0x8cfbf26f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "natodef.cp4",	0x1000, 0xb4c90fb2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "natodef.cp6",	0x1000, 0xc6d0d35e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "natodef.cp8",	0x1000, 0xe4b6c21e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "natodef.cpa",	0x1000, 0x888ecd42, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "natodef.cpc",	0x1000, 0xcf713bc9, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "natodef.cpe",	0x1000, 0x4eef6bf4, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "natodef.cp5",	0x1000, 0x65c3601b, 1 | BRF_PRG | BRF_ESS }, //  8

	{ "b8",				0x0200, 0xfe865b2a, 2 | BRF_PRG | BRF_ESS }, //  9 Coprocessor Code
	{ "c8",				0x0200, 0x7ed5c923, 2 | BRF_PRG | BRF_ESS }, // 10

	{ "natodef.o4",		0x1000, 0x39a868f8, 3 | BRF_GRA },           // 11 Blitter Graphics
	{ "natodef.e1",		0x1000, 0xb6d1623d, 3 | BRF_GRA },           // 12
	{ "natodef.o3",		0x1000, 0xb217909a, 3 | BRF_GRA },           // 13
	{ "natodef.e2",		0x1000, 0x886c3f05, 3 | BRF_GRA },           // 14
	{ "natodef.o2",		0x1000, 0x77cc9cfd, 3 | BRF_GRA },           // 15
	{ "natodef.e3",		0x1000, 0x5302410d, 3 | BRF_GRA },           // 16
};

STD_ROM_PICK(natodefa)
STD_ROM_FN(natodefa)

struct BurnDriver BurnDrvNatodefa = {
	"natodefa", "natodef", NULL, "natodef", "1982",
	"NATO Defense (alternate mazes)\0", NULL, "Pacific Novelty", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, natodefaRomInfo, natodefaRomName, NULL, NULL, DrvSampleInfo, DrvSampleName, NatodefInputInfo, NatodefDIPInfo,
	NatodefInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10,
	256, 256, 4, 3
};
