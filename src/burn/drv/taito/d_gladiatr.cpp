// FinalBurn Neo Gladiator driver module
// Based on MAME driver by Victor Trucco, Steve Ellenoff, Phil Stroffolino, Tatsuyuki Satoh, Tomasz Slanina, Nicola Salmoria, Vas Crabb

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6809_intf.h"
#include "mcs48.h"
#include "burn_ym2203.h"
#include "msm5205.h"
#include "bitswap.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvM6809ROM;
static UINT8 *DrvMCUROM[4];
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvZ80RAM[2];

static INT32 main_bank;
static INT32 sound_bank;
static INT32 sprite_bank;
static INT32 sprite_buffer;
static INT32 soundlatch;
static INT32 flipscreen;
static INT32 scrolly[2];
static INT32 scrollx[2];
static INT32 fg_tile_bank;
static INT32 bg_tile_bank;
static INT32 video_attributes;
static UINT8 previous_inputs[4];
static INT32 cctl_p1;
static INT32 cctl_p2;
static INT32 ucpu_p1;
static INT32 csnd_p1;
static INT32 tclk_val;

static UINT8 last_portA;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

// heavy syncing needed: mcu's & subcpu sync to main - lots of bi-directional data
static INT32 in_sub = 0; // prevent recursion

// mcs48 cpu indexes
enum {
	CCTL = 0,
	CCPU = 1,
	UCPU = 2,
	CSND = 3
};

static struct BurnInputInfo GladiatrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Gladiatr)

static struct BurnDIPInfo GladiatrDIPList[]=
{
	DIP_OFFSET(0x14)
	{0x00, 0xff, 0xff, 0x5a, NULL						},
	{0x01, 0xff, 0xff, 0xbf, NULL						},
	{0x02, 0xff, 0xff, 0xff, NULL						},

	{0   , 0xfe, 0   ,    4, "Difficulty"				},
	{0x00, 0x01, 0x03, 0x03, "Easy"						},
	{0x00, 0x01, 0x03, 0x02, "Medium"					},
	{0x00, 0x01, 0x03, 0x01, "Hard"						},
	{0x00, 0x01, 0x03, 0x00, "Hardest"					},

	{0   , 0xfe, 0   ,    2, "After 4 Stages"			},
	{0x00, 0x01, 0x04, 0x00, "Continues"				},
	{0x00, 0x01, 0x04, 0x04, "Ends"						},

	{0   , 0xfe, 0   ,    2, "Bonus Life"				},
	{0x00, 0x01, 0x08, 0x08, "Only at 100000"			},
	{0x00, 0x01, 0x08, 0x00, "Every 100000"				},
	
	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x00, 0x01, 0x30, 0x30, "1"						},
	{0x00, 0x01, 0x30, 0x20, "2"						},
	{0x00, 0x01, 0x30, 0x10, "3"						},
	{0x00, 0x01, 0x30, 0x00, "4"						},

	{0   , 0xfe, 0   ,    2, "Allow Continue"			},
	{0x00, 0x01, 0x40, 0x00, "No"						},
	{0x00, 0x01, 0x40, 0x40, "Yes"						},
		
	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x00, 0x01, 0x80, 0x80, "Off"						},
	{0x00, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x01, 0x01, 0x03, 0x03, "1 Coin  1 Credit"			},
	{0x01, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x01, 0x01, 0x03, 0x01, "1 Coin  4 Credits"		},
	{0x01, 0x01, 0x03, 0x00, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x01, 0x01, 0x0c, 0x00, "5 Coins 1 Credit"			},
	{0x01, 0x01, 0x0c, 0x04, "4 Coins 1 Credit"			},
	{0x01, 0x01, 0x0c, 0x08, "3 Coins 1 Credit"			},
	{0x01, 0x01, 0x0c, 0x0c, "2 Coins 1 Credit"			},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x01, 0x01, 0x10, 0x10, "Off"						},
	{0x01, 0x01, 0x10, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x40, 0x00, "Upright"					},
	{0x01, 0x01, 0x40, 0x40, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"				},
	{0x01, 0x01, 0x80, 0x80, "Off"						},
	{0x01, 0x01, 0x80, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x02, 0x01, 0x01, 0x01, "Off"						},
	{0x02, 0x01, 0x01, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Memory Backup"			},
	{0x02, 0x01, 0x02, 0x02, "Normal"					},
	{0x02, 0x01, 0x02, 0x00, "Clear"					},

	{0   , 0xfe, 0   ,    4, "Starting Stage"			},
	{0x02, 0x01, 0x0c, 0x0c, "1"						},
	{0x02, 0x01, 0x0c, 0x08, "2"						},
	{0x02, 0x01, 0x0c, 0x04, "3"						},
	{0x02, 0x01, 0x0c, 0x00, "4"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x02, 0x01, 0x80, 0x80, "Off"						},
	{0x02, 0x01, 0x80, 0x00, "On"						},
};

STDDIPINFO(Gladiatr)

static void palette_write(INT32 i)
{
	i &= 0x3ff;
	INT32 x = BurnPalRAM[i + 0x400];
	INT32 r = ((BurnPalRAM[i] & 0xf) << 1) | ((x & 0x10) >> 4);
	INT32 g = ((BurnPalRAM[i] >> 4) << 1) | ((x & 0x20) >> 5);
	INT32 b = ((x & 0xf) << 1) | ((x & 0x40) >> 6);

	BurnPalette[i] = BurnHighCol(pal5bit(r), pal5bit(g), pal5bit(b), 0);
}

static void __fastcall gladiatr_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xd000) {
		BurnPalRAM[address & 0x7ff] = data;
		palette_write(address);
		return;
	}

	switch (address & 0xff80)
	{
		case 0xcc00:
			scrolly[1] = data;
		return;

		case 0xcc80:
			fg_tile_bank = data & 0x03;
			bg_tile_bank = (data >> 4) & 1;
			video_attributes = data;
		return;

		case 0xcd00:
			scrollx[1] = data;
		return;

		case 0xce00:
			scrolly[0] = data;
		return;

		case 0xcf00:
			scrollx[0] = data;
		return;
	}
}

static UINT8 __fastcall gladiatr_main_read(UINT16 address)
{
	return 0;
}

static void sync_mcu()
{
	INT32 todo = ((INT64)ZetTotalCycles(0) * (6000000 / 15) / 6000000) - mcs48TotalCycles();
	if (todo < 1) todo = 10; // need to run some cycles even if MCU is up-to-date, to process incoming/outgoing messages.
	if (todo > 0) {
		mcs48Run(todo);
	}
}

static void sync_sub()
{
	if (in_sub) return;
	INT32 todo = ((INT64)ZetTotalCycles(0) * 3000000 / 6000000) - ZetTotalCycles(1);
	if (todo > 0) {
		in_sub = 1;
		ZetRun(1, todo);
		in_sub = 0;
	}
}

static UINT8 read_master(INT32 cpu, INT32 offset)
{
	// SYNC CPUS
	sync_sub();
	mcs48Open(cpu);
	sync_mcu();
	UINT8 ret = mcs48_master_r(offset & 1);
	mcs48Close();
	return ret;
}

static void write_master(INT32 cpu, INT32 offset, UINT8 data)
{
	sync_sub();
	mcs48Open(cpu);
	sync_mcu();
	mcs48_master_w(offset & 1, data);
	mcs48Close();
}

static void bankswitch(INT32 bank)
{
	main_bank = bank;

	ZetMapMemory(DrvZ80ROM[0] + 0x10000 + (main_bank ? 0x6000 : 0), 0x6000, 0xbfff, MAP_ROM);
}

static void __fastcall gladiatr_main_write_port(UINT16 port, UINT8 data)
{
	switch (port)
	{
		case 0xc000:
			sprite_buffer = data & 1;
		return;

		case 0xc001:
			sprite_bank = (data & 1) ? 4 : 2;
		return;

		case 0xc002:
			bankswitch(data & 1);
		return;

		case 0xc003:
		return; // nc?

		case 0xc004:
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_HOLD); // 1x frame irq for SUB
		return;

		case 0xc005:
		case 0xc006:
		return; // nc?

		case 0xc007:
			flipscreen = data & 1;
		return;

		case 0xc09e:
		case 0xc09f:
			write_master(UCPU, port & 1, data);
		return;

		case 0xc0bf:
		return; // watchdog (inop)
	}
}

static UINT8 __fastcall gladiatr_main_read_port(UINT16 port)
{
	switch (port)
	{
		case 0xc09e:
		case 0xc09f:
			return read_master(UCPU, port & 1);

		case 0xc0bf:
			return 0; // watchdog (inop)
	}

	return 0;
}

static void __fastcall gladiatr_sub_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			BurnYM2203Write(0, port & 1, data);
		return;

		case 0x20:
		case 0x21:
			write_master(CSND, port & 1, data);
		return;

		case 0x40:
		return; // nop

		case 0x60:
		case 0x61:
			write_master(CCTL, port & 1, data);
		return;

		case 0x80:
		case 0x81:
			write_master(CCPU, port & 1, data);
		return;

		case 0xa0:
		case 0xa1:
		case 0xa2:
		case 0xa3:
		case 0xa4:
		case 0xa5:
		case 0xa6:
		case 0xa7:
			// ls259 d0 (filtlatch)
		return;

		case 0xe0:
			soundlatch = data;
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_ACK); // nmi!
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 __fastcall gladiatr_sub_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM2203Read(0, port & 1);

		case 0x20:
		case 0x21:
			return read_master(CSND, port);

		case 0x40:
			return 0xff; // nop

		case 0x60:
		case 0x61:
			return read_master(CCTL, port);

		case 0x80:
		case 0x81:
			return read_master(CCPU, port);
	}

	return 0;
}

static void sound_bankswitch(INT32 bank)
{
	sound_bank = bank;
	M6809MapMemory(DrvM6809ROM + 0x10000 + (sound_bank ? 0xc000 : 0), 0x4000, 0xffff, MAP_ROM);
}

static void gladiatr_adpcmcpu_write(UINT16 address, UINT8 data)
{
	switch (address & 0xf000)
	{
		case 0x1000:
			sound_bankswitch(data & 0x40);
			MSM5205DataWrite(0, data & 0x0f);
			MSM5205ResetWrite(0, (data & 0x20) >> 5);
			MSM5205VCLKWrite(0, (data & 0x10) >> 4);
		return;
	}
}

static UINT8 gladiatr_adpcmcpu_read(UINT16 address)
{
	switch (address & 0xf000)
	{
		case 0x2000:
			return soundlatch;
	}

	return 0;
}

static UINT8 gladiatr_cctl_read_port(UINT32 port)
{
	switch (port)
	{
		case MCS48_T0:
			return (DrvInputs[3] >> 3) & 1;

		case MCS48_T1:
			return (DrvInputs[3] >> 2) & 1;

		case MCS48_P1:
			return cctl_p1 & DrvInputs[2];

		case MCS48_P2:
			return cctl_p2;
	}

	return 0xff;
}

static void gladiatr_ccpu_write_port(UINT32 port, UINT8 data)
{
	switch (port)
	{
		case MCS48_P2:
			// coin counters
			break;
	}
}

static UINT8 gladiatr_ccpu_read_port(UINT32 port)
{
	switch (port)
	{
		case MCS48_T0:
			return (DrvInputs[3] >> 0) & 1;

		case MCS48_T1:
			return (DrvInputs[3] >> 1) & 1;

		case MCS48_P1:
			return DrvInputs[0];

		case MCS48_P2:
			return DrvInputs[1];
	}

	return 0xff;
}

static void gladiatr_ucpu_write_port(UINT32 port, UINT8 data)
{
	switch (port)
	{
	    case MCS48_P1:
			ucpu_p1 = data;
			return;
		break;
	}
}

static UINT8 gladiatr_ucpu_read_port(UINT32 port)
{
	switch (port)
	{
		case MCS48_T0:
			return tclk_val;

		case MCS48_T1:
			return BIT(csnd_p1, 1);

		case MCS48_P1:
			return csnd_p1 |= 0xfe;

		case MCS48_P2:
			return BITSWAP08(DrvDips[0], 0,1,2,3,4,5,6,7);
	}

	return 0xff;
}

static void gladiatr_csnd_write_port(UINT32 port, UINT8 data)
{
	switch (port)
	{
		case MCS48_P1:
			csnd_p1 = data;
			return;

		default:
			break;
	}
}

static UINT8 gladiatr_csnd_read_port(UINT32 port)
{
	switch (port)
	{
		case MCS48_T0:
			return tclk_val;

		case MCS48_T1:
			return BIT(ucpu_p1, 1);

		case MCS48_P1:
			return ucpu_p1 |= 0xfe;

		case MCS48_P2:
		return BITSWAP08(DrvDips[1], 2,3,4,5,6,7,1,0);
	}

	return 0xff;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvColRAM[offs];

	TILE_SET_INFO(1, (DrvVidRAM[offs]) | ((attr & 0x07) << 8) | (bg_tile_bank << 11), (attr >> 3) ^ 0x1f, 0);
}

static tilemap_callback( fg )
{
	TILE_SET_INFO(0, DrvTxtRAM[offs] | (fg_tile_bank << 8), 0, 0);
}

static UINT8 YM2203_read_portB(UINT32)
{
	return DrvDips[2];
}

static void YM2203_write_portA(UINT32, UINT32 data)
{
	if (~last_portA & 0x80 && data & 0x80) {
		mcs48Open(CCPU);
		mcs48Reset();
		mcs48Close();

		mcs48Open(CCTL);
		mcs48Reset();
		mcs48Close();
	}
	last_portA = data;
}

static void DrvIRQHandler(INT32, INT32 nStatus)
{
	// empty!
}

inline static INT32 DrvMSM5205SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)M6809TotalCycles() * nSoundRate / (3000000 / 4));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	BurnYM2203Reset();
	ZetReset();
	ZetClose();

	M6809Open(0);
	sound_bankswitch(0);
	M6809Reset();
	MSM5205Reset();
	M6809Close();

	mcs48Open(CCTL);
	mcs48Reset();
	mcs48Close();

	mcs48Open(CCPU);
	mcs48Reset();
	mcs48Close();

	mcs48Open(UCPU);
	mcs48Reset();
	mcs48Close();

	mcs48Open(CSND);
	mcs48Reset();
	mcs48Close();

	sprite_bank = 2; // gladiatr
	sprite_buffer = 0;
	soundlatch = 0;
	flipscreen = 0;
	scrolly[0] = 0;
	scrolly[1] = 0;
	scrollx[0] = 0;
	scrollx[1] = 0;
	fg_tile_bank = 0;
	bg_tile_bank = 0;
	video_attributes = 0;
	tclk_val = 0;
	cctl_p1 = 0xff;
	cctl_p2 = 0xff;
	ucpu_p1 = 0xff;
	csnd_p1 = 0xff;
	last_portA = 0xff;

	memset (previous_inputs, 0xff, sizeof(previous_inputs));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next 	= AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x120000;
	DrvZ80ROM[1]		= Next; Next += 0x120000;
	DrvM6809ROM			= Next; Next += 0x120000;

	DrvMCUROM[0]		= Next; Next += 0x000400;
	DrvMCUROM[1]		= Next; Next += 0x000400;
	DrvMCUROM[2]		= Next; Next += 0x000400;
	DrvMCUROM[3]		= Next; Next += 0x000800;

	DrvGfxROM[0]		= Next; Next += 0x110000;
	DrvGfxROM[1]		= Next; Next += 0x180000;
	DrvGfxROM[2]		= Next; Next += 0x180000;

	BurnPalette			= (UINT32*)Next; Next += 0x0401 * sizeof(UINT32);

	AllRam				= Next;

	DrvSprRAM			= Next; Next += 0x000c00;

	BurnPalRAM			= Next; Next += 0x000800;
	DrvVidRAM			= Next; Next += 0x000800;
	DrvColRAM			= Next; Next += 0x000800;
	DrvTxtRAM			= Next; Next += 0x000800;

	DrvZ80RAM[0]		= Next; Next += 0x000800;
	DrvZ80RAM[1]		= Next; Next += 0x000400;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[1]  = { 0 };
	INT32 XOffs0[8]  = { STEP8(0,1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };

	INT32 Plane1[3]  = { 4, (0x10000*8)+0, (0x10000*8)+4 };
	INT32 XOffs1[8]  = { STEP4(0,1), STEP4(64,1) };
	INT32 YOffs1[8]  = { STEP8(0,8) };

	INT32 Plane2[3]  = { 4, (0x18000*8)+0, (0x18000*8)+4 };
	INT32 XOffs2[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs2[16] = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x30000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x02000);

	GfxDecode(0x0400, 1,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x020000);

	GfxDecode(0x1000, 3,  8,  8, Plane1, XOffs1, YOffs1, 0x080, tmp, DrvGfxROM[1]); 

	memcpy (tmp, DrvGfxROM[2], 0x030000);

	GfxDecode(0x0600, 3, 16, 16, Plane2, XOffs2, YOffs2, 0x200, tmp, DrvGfxROM[2]);

	BurnFree (tmp);

	return 0;
}

static void swap_block(UINT8 *src1, UINT8 *src2, INT32 len)
{
	for (INT32 i = 0;i < len;i++)
	{
		INT32 t = src1[i];
		src1[i] = src2[i];
		src2[i] = t;
	}
}

static void region_swap()
{
	UINT8 *rom = DrvGfxROM[1];

	for (INT32 j = 3; j >= 0; j--)
	{
		for (INT32 i = 0; i < 0x2000; i++)
		{
			rom[i+(2*j+1)*0x2000] = rom[i+j*0x2000] >> 4;
			rom[i+2*j*0x2000] = rom[i+j*0x2000];
		}
	}

	swap_block(rom + 0x14000, rom + 0x18000, 0x4000);

	rom = DrvGfxROM[2];

	for (INT32 j = 5; j >= 0; j--)
	{
		for (INT32 i = 0; i < 0x2000; i++)
		{
			rom[i+(2*j+1)*0x2000] = rom[i+j*0x2000] >> 4;
			rom[i+2*j*0x2000] = rom[i+j*0x2000];
		}
	}

	swap_block(rom + 0x1a000, rom + 0x1c000, 0x2000);
	swap_block(rom + 0x22000, rom + 0x28000, 0x2000);
	swap_block(rom + 0x26000, rom + 0x2c000, 0x2000);
	swap_block(rom + 0x24000, rom + 0x28000, 0x4000);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x10000, k++, 1)) return 1;
		memcpy (DrvZ80ROM[0] + 0x16000, DrvZ80ROM[0] + 0x12000, 0x02000);

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
		memcpy (DrvZ80ROM[0] + 0x12000, DrvZ80ROM[1] + 0x00000, 0x04000);
		memcpy (DrvZ80ROM[0] + 0x18000, DrvZ80ROM[1] + 0x04000, 0x04000);

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
		
		if (BurnLoadRom(DrvM6809ROM  + 0x00000, k++, 1)) return 1;
		memcpy (DrvM6809ROM + 0x10000, DrvM6809ROM + 0x00000, 0x04000);
		memcpy (DrvM6809ROM + 0x1c000, DrvM6809ROM + 0x04000, 0x04000);
		if (BurnLoadRom(DrvM6809ROM  + 0x00000, k++, 1)) return 1;
		memcpy (DrvM6809ROM + 0x14000, DrvM6809ROM + 0x00000, 0x04000);
		memcpy (DrvM6809ROM + 0x20000, DrvM6809ROM + 0x04000, 0x04000);
		if (BurnLoadRom(DrvM6809ROM  + 0x00000, k++, 1)) return 1;
		memcpy (DrvM6809ROM + 0x18000, DrvM6809ROM + 0x00000, 0x04000);
		memcpy (DrvM6809ROM + 0x24000, DrvM6809ROM + 0x04000, 0x04000);

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x18000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x1c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x28000, k++, 1)) return 1;

	//	if (BurnLoadRom(DrvUnkPROM   + 0x00000, k++, 1)) return 1;
	//	if (BurnLoadRom(DrvUnkPROM   + 0x00000, k++, 1)) return 1;
		k++; k++;

		if (BurnLoadRom(DrvMCUROM[0] + 0x00000, k++, 1)) return 1; // cctl
		if (BurnLoadRom(DrvMCUROM[1] + 0x00000, k++, 1)) return 1; // ccpu
		if (BurnLoadRom(DrvMCUROM[2] + 0x00000, k++, 1)) return 1; // ucpu
		if (BurnLoadRom(DrvMCUROM[3] + 0x00000, k++, 1)) return 1; // csnd

		region_swap();
		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],		0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,			0xc000, 0xcbff, MAP_RAM);
	ZetMapMemory(BurnPalRAM,		0xd000, 0xd7ff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,			0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvColRAM,			0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,			0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM[0],		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(gladiatr_main_write);
	ZetSetReadHandler(gladiatr_main_read);
	ZetSetOutHandler(gladiatr_main_write_port);
	ZetSetInHandler(gladiatr_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],		0x8000, 0x83ff, MAP_RAM);
	ZetSetOutHandler(gladiatr_sub_write_port);
	ZetSetInHandler(gladiatr_sub_read_port);
	ZetClose();

	M6809Init(0); // MC6809 has a 4 divider(!)
	M6809Open(0);
	M6809MapMemory(DrvM6809ROM + 0x10000, 0x4000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(gladiatr_adpcmcpu_write);
	M6809SetReadHandler(gladiatr_adpcmcpu_read);
	M6809Close();

	mcs48Init(CCTL, 8041, DrvMCUROM[0]);
	mcs48Open(CCTL);
	mcs48_set_read_port(gladiatr_cctl_read_port);
	mcs48Close();

	mcs48Init(CCPU, 8041, DrvMCUROM[1]);
	mcs48Open(CCPU);
	mcs48_set_read_port(gladiatr_ccpu_read_port);
	mcs48_set_write_port(gladiatr_ccpu_write_port);
	mcs48Close();

	mcs48Init(UCPU, 8041, DrvMCUROM[2]);
	mcs48Open(UCPU);
	mcs48_set_read_port(gladiatr_ucpu_read_port);
	mcs48_set_write_port(gladiatr_ucpu_write_port);
	mcs48Close();

	mcs48Init(CSND, 8042, DrvMCUROM[3]);
	mcs48Open(CSND);
	mcs48_set_read_port(gladiatr_csnd_read_port);
	mcs48_set_write_port(gladiatr_csnd_write_port);
	mcs48Close();

	BurnYM2203Init(1, 1500000, &DrvIRQHandler, 0);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetPorts(0, NULL, &YM2203_read_portB, &YM2203_write_portA, NULL);
	BurnYM2203SetAllRoutes(0, 0.60, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, DrvMSM5205SynchroniseStream, 455000, NULL, MSM5205_SEX_4B, 1);
	MSM5205SetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 1,  8,  8, 0x10000, 0x200, 0x00);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 3,  8,  8, 0x40000, 0x000, 0x1f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 3, 16, 16, 0x60000, 0x100, 0x1f);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	BurnYM2203Exit();
	MSM5205Exit();

	mcs48Exit();
	M6809Exit();
	ZetExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x400; i++)
	{
		INT32 x = BurnPalRAM[i + 0x400];
		INT32 r = ((BurnPalRAM[i] & 0xf) << 1) | ((x & 0x10) >> 4);
		INT32 g = ((BurnPalRAM[i] >> 4) << 1) | ((x & 0x20) >> 5);
		INT32 b = ((x & 0xf) << 1) | ((x & 0x40) >> 6);

		BurnPalette[i] = BurnHighCol(pal5bit(r), pal5bit(g), pal5bit(b), 0);
	}

	BurnPalette[0x400] = BurnHighCol(0, 0, 0, 0); // set bg black color
}

static void draw_sprites()
{
	UINT8 *src = DrvSprRAM + (sprite_buffer * 0x80);

	for (INT32 offs = 0; offs < 0x80; offs += 2)
	{
		INT32 color = src[offs + 0x001] & 0x1f;
		INT32 attr = src[offs + 0x800];
		INT32 size = (attr & 0x10) >> 4;
		INT32 sy = 240 - src[offs + 0x400] - (size ? 16 : 0);
		INT32 sx = src[offs + 0x401] + 256*(src[offs + 0x801] & 1) - 0x38;
		INT32 bank = (attr & 0x01) + ((attr & 0x02) ? sprite_bank : 0);
		INT32 flipx = attr & 0x04;
		INT32 flipy = attr & 0x08;
		INT32 code = (src[offs + 0x000]+256*bank);

		if (flipscreen)
		{
			flipx = !flipx;
			flipy = !flipy;
		}

		for (INT32 y = 0; y <= size; y++)
		{
			for (INT32 x = 0; x <= size; x++)
			{
				INT32 tile = code + ((flipy ? (size - y) : y) * 2) + (flipx ? (size - x) : x);

				DrawGfxMaskTile(0, 2, tile, sx + x * 16, sy + y * 16 - 16, flipx, flipy, color, 0);
				DrawGfxMaskTile(0, 2, tile, sx + x * 16, sy + y * 16 + 256 - 16, flipx, flipy, color, 0); // wrap
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteUpdate();
		BurnRecalc = 0;
	}

	BurnTransferClear(0x400);

	if (video_attributes & 0x20)
	{
		const INT32 scrollx_offsets[2] = { 0x30, -0x12f};

		INT32 bgscrollx = scrollx_offsets[flipscreen & 1] + (scrollx[0] + ((video_attributes & 0x04) << 6));
		INT32 fgscrollx = scrollx_offsets[flipscreen & 1] + (scrollx[1] + ((video_attributes & 0x08) << 5));
		bgscrollx &= 0x1ff;
		fgscrollx &= 0x1ff;

		GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? (TMAP_FLIPX | TMAP_FLIPY) : 0);

		GenericTilemapSetScrollX(0, bgscrollx ^ (flipscreen ? 0xf : 0));
		GenericTilemapSetScrollY(0, scrolly[0] & 0xff);
		GenericTilemapSetScrollX(1, fgscrollx ^ (flipscreen ? 0xf : 0));
		GenericTilemapSetScrollY(1, scrolly[1] & 0xff);

		if (nBurnLayer & 1) GenericTilemapDraw(0, 0, 0);

		if (nSpriteEnable & 1) draw_sprites();

		if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);
	}

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();
	M6809NewFrame();
	mcs48NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
#if 0
		// this might be needed if the real mcu's get dumped
		if ((DrvInputs[0] ^ previous_inputs[0]) & 0x10) { // p1_s1
			if ((previous_inputs[0] & 0x10) && (~DrvInputs[0] & 0x10)) {
				cctl_p1 = (cctl_p1 & 0xfd) | ((cctl_p1 & 0x01) << 1);
			}
		}

		if ((DrvInputs[0] ^ previous_inputs[0]) & 0x20) { // p1_s2
			cctl_p1 = (cctl_p1 & 0xfe) | ((DrvInputs[0] & 0x20) ? 0x00 : 0x01);
		}

		if ((DrvInputs[1] ^ previous_inputs[1]) & 0x10) { // p2_s1
			if ((previous_inputs[1] & 0x10) && (~DrvInputs[1] & 0x10)) {
				cctl_p2 = (cctl_p2 & 0xfd) | ((cctl_p2 & 0x01) << 1);
			}
		}

		if ((DrvInputs[1] ^ previous_inputs[1]) & 0x20) { // p2_s2
			cctl_p2 = (cctl_p2 & 0xfe) | ((DrvInputs[1] & 0x20) ? 0x00 : 0x01);
		}

		memcpy (previous_inputs, DrvInputs, sizeof(previous_inputs));
#endif
	}

	INT32 nInterleave = 256*4;
	INT32 nCyclesTotal[7] = { 6000000 / 60, 3000000 / 60, 3000000 / 4 / 60, 6000000 / 15 / 60, 6000000 / 15 / 60, 6000000 / 15 / 60, 6000000 / 15 / 60 };
	INT32 nCyclesDone[7] = { 0, 0, 0, 0, 0, 0, 0 };

	MSM5205NewFrame(0, 3000000 / 4, nInterleave);
	M6809Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // should be vblank...		
		ZetClose();

		ZetOpen(1);
		in_sub = 1;
		BurnTimerUpdate((nCyclesTotal[1] / nInterleave) * (i + 1));
		in_sub = 0;
		ZetClose();

		CPU_RUN(2, M6809);
		MSM5205UpdateScanline(i);

		mcs48Open(0);
		CPU_RUN(3, mcs48);
		mcs48Close();

		mcs48Open(1);
		CPU_RUN(4, mcs48);
		mcs48Close();

		mcs48Open(2);
		CPU_RUN(5, mcs48);
		mcs48Close();

		mcs48Open(3);
		CPU_RUN(6, mcs48);
		mcs48Close();

		if ((i%10) == 9) tclk_val ^= 1;
	}

	ZetOpen(1);
	in_sub = 1;
	BurnTimerEndFrame(nCyclesTotal[1]);
	in_sub = 0;

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	M6809Close();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029672;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);
		M6809Scan(nAction);

		mcs48Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(main_bank);
		SCAN_VAR(sound_bank);
		SCAN_VAR(sprite_bank);
		SCAN_VAR(sprite_buffer);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(scrolly);
		SCAN_VAR(scrollx);
		SCAN_VAR(fg_tile_bank);
		SCAN_VAR(bg_tile_bank);
		SCAN_VAR(video_attributes);
		SCAN_VAR(previous_inputs);
		SCAN_VAR(cctl_p1);
		SCAN_VAR(cctl_p2);
		SCAN_VAR(ucpu_p1);
		SCAN_VAR(csnd_p1);
		SCAN_VAR(tclk_val);
		SCAN_VAR(last_portA);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(main_bank);
		ZetClose();

		M6809Open(0);
		sound_bankswitch(sound_bank);
		M6809Close();
	}

	return 0;
}


// Gladiator (US)

static struct BurnRomInfo gladiatrRomDesc[] = {
	{ "qb0_5",			0x4000, 0x25b19efb, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "qb0_4",			0x2000, 0x347ec794, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qb0_1",			0x4000, 0x040c9839, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "qc0_3",			0x8000, 0x8d182326, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "qb0_17",			0x4000, 0xe78be010, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "qb0_20",			0x8000, 0x15916eda, 3 | BRF_PRG | BRF_ESS }, //  5 M6809 Code
	{ "qb0_19",			0x8000, 0x79caa7ed, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "qb0_18",			0x8000, 0xe9591260, 3 | BRF_PRG | BRF_ESS }, //  7

	{ "qc0_15",			0x2000, 0xa7efa340, 4 | BRF_GRA },           //  8 Characters

	{ "qb0_12",			0x8000, 0x0585d9ac, 5 | BRF_GRA },           //  9 Background Tiles
	{ "qb0_13",			0x8000, 0xa6bb797b, 5 | BRF_GRA },           // 10
	{ "qb0_14",			0x8000, 0x85b71211, 5 | BRF_GRA },           // 11

	{ "qc1_6",			0x4000, 0x651e6e44, 6 | BRF_GRA },           // 12 Sprites
	{ "qc2_7",			0x8000, 0xc992c4f7, 6 | BRF_GRA },           // 13
	{ "qc0_8",			0x4000, 0x1c7ffdad, 6 | BRF_GRA },           // 14
	{ "qc1_9",			0x4000, 0x01043e03, 6 | BRF_GRA },           // 15
	{ "qc1_10",			0x8000, 0x364cdb58, 6 | BRF_GRA },           // 16
	{ "qc2_11",			0x8000, 0xc9fecfff, 6 | BRF_GRA },           // 17

	{ "q3.2b",			0x0020, 0x6a7c3c60, 7 | BRF_GRA },           // 18 Unused PROMS
	{ "q4.5s",			0x0020, 0xe325808e, 7 | BRF_GRA },           // 19

	{ "aq_002.9b",		0x0400, 0xb30d225f, 8 | BRF_GRA },           // 20 CCTL MCU Code

	{ "aq_003.xx",		0x0400, 0x1d02cd5f, 9 | BRF_PRG | BRF_ESS }, // 21 CCPU MCU Code

	{ "aq_006.3a",		0x0400, 0x3c5ca4c6, 10 | BRF_PRG | BRF_ESS },// 22 UCPU MCU Code

	{ "aq_007.6c",		0x0800, 0xf19af04d, 11 | BRF_PRG | BRF_ESS },// 23 CSND MCU Code
};

STD_ROM_PICK(gladiatr)
STD_ROM_FN(gladiatr)

struct BurnDriver BurnDrvGladiatr = {
	"gladiatr", NULL, NULL, NULL, "1986",
	"Gladiator (US)\0", NULL, "Allumer / Taito America Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, gladiatrRomInfo, gladiatrRomName, NULL, NULL, NULL, NULL, GladiatrInputInfo, GladiatrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x401,
	256, 224, 4, 3
};


// Ougon no Shiro (Japan)

static struct BurnRomInfo ogonsiroRomDesc[] = {
	{ "qb0_5",			0x4000, 0x25b19efb, 1 | BRF_PRG | BRF_ESS },  //  0 Z80 #0 Code
	{ "qb0_4",			0x2000, 0x347ec794, 1 | BRF_PRG | BRF_ESS },  //  1
	{ "qb0_1",			0x4000, 0x040c9839, 1 | BRF_PRG | BRF_ESS },  //  2
	{ "qc0_3",			0x8000, 0xd6a342e7, 1 | BRF_PRG | BRF_ESS },  //  3

	{ "qb0_17",			0x4000, 0xe78be010, 2 | BRF_PRG | BRF_ESS },  //  4 Z80 #1 Code

	{ "qb0_20",			0x8000, 0x15916eda, 3 | BRF_PRG | BRF_ESS },  //  5 M6809 Code
	{ "qb0_19",			0x8000, 0x79caa7ed, 3 | BRF_PRG | BRF_ESS },  //  6
	{ "qb0_18",			0x8000, 0xe9591260, 3 | BRF_PRG | BRF_ESS },  //  7

	{ "qc0_15",			0x2000, 0x5e1332b8, 4 | BRF_GRA },            //  8 Characters

	{ "qb0_12",			0x8000, 0x0585d9ac, 5 | BRF_GRA },            //  9 Background Tiles
	{ "qb0_13",			0x8000, 0xa6bb797b, 5 | BRF_GRA },            // 10
	{ "qb0_14",			0x8000, 0x85b71211, 5 | BRF_GRA },            // 11

	{ "qc1_6",			0x4000, 0x1a2bc769, 6 | BRF_GRA },            // 12 Sprites
	{ "qc2_7",			0x8000, 0x4b677bd9, 6 | BRF_GRA },            // 13
	{ "qc0_8",			0x4000, 0x1c7ffdad, 6 | BRF_GRA },            // 14
	{ "qc1_9",			0x4000, 0x38f5152d, 6 | BRF_GRA },            // 15
	{ "qc1_10",			0x8000, 0x87ab6cc4, 6 | BRF_GRA },            // 16
	{ "qc2_11",			0x8000, 0x25eaa4ff, 6 | BRF_GRA },            // 17

	{ "q3.2b",			0x0020, 0x6a7c3c60, 7 | BRF_GRA },            // 18 Unused PROMS
	{ "q4.5s",			0x0020, 0xe325808e, 7 | BRF_GRA },            // 19

	{ "aq_002.9b",		0x0400, 0xb30d225f, 8 | BRF_GRA },            // 20 CCTL MCU Code

	{ "aq_003.xx",		0x0400, 0x1d02cd5f, 9 | BRF_PRG | BRF_ESS },  // 21 CCPU MCU Code

	{ "aq_006.3a",		0x0400, 0x3c5ca4c6, 10 | BRF_PRG | BRF_ESS }, // 22 UCPU MCU Code

	{ "aq_007.6c",		0x0800, 0xf19af04d, 11 | BRF_PRG | BRF_ESS }, // 23 CSND MCU Code
};

STD_ROM_PICK(ogonsiro)
STD_ROM_FN(ogonsiro)

struct BurnDriver BurnDrvOgonsiro = {
	"ogonsiro", "gladiatr", NULL, NULL, "1986",
	"Ougon no Shiro (Japan)\0", NULL, "Allumer / Taito Corporation", "Miscellaneous",
	L"Ougon no Shiro (Japan)\0\u9ec4\u91d1\u306e\u57ce\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, ogonsiroRomInfo, ogonsiroRomName, NULL, NULL, NULL, NULL, GladiatrInputInfo, GladiatrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x401,
	256, 224, 4, 3
};
