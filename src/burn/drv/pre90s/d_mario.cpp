// FB Alpha Mario Bros driver module
// Based on MAME driver by Mirko Buffoni

#include "tiles_generic.h"
#include "z80_intf.h"
#include "i8039.h"
#include "samples.h"
#include "dac.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSndRAM;

static UINT8 *i8039_p;
static UINT8 *i8039_t;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *sample_data;

static UINT8 *soundlatch;
static UINT8 *flipscreen;
static UINT8 *interrupt_enable;
static UINT8 *gfxbank;
static UINT8 *palbank;
static UINT8 *scroll;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT32 masao = 0;

static INT32 monitor = 0; // monitor type (unused atm)

static struct BurnInputInfo MarioInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Mario)

static struct BurnInputInfo MariooInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Marioo)

static struct BurnDIPInfo MarioDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x00, NULL			},
	{0x0c, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0b, 0x01, 0x80, 0x00, "Off"			},
	{0x0b, 0x01, 0x80, 0x01, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x03, 0x00, "3"			},
	{0x0c, 0x01, 0x03, 0x01, "4"			},
	{0x0c, 0x01, 0x03, 0x02, "5"			},
	{0x0c, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0c, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x0c, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0c, 0x01, 0x30, 0x00, "20k only"		},
	{0x0c, 0x01, 0x30, 0x10, "30k only"		},
	{0x0c, 0x01, 0x30, 0x20, "40k only"		},
	{0x0c, 0x01, 0x30, 0x30, "None"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0c, 0x01, 0xc0, 0x00, "Easy"			},
	{0x0c, 0x01, 0xc0, 0x80, "Medium"		},
	{0x0c, 0x01, 0xc0, 0x40, "Hard"			},
	{0x0c, 0x01, 0xc0, 0xc0, "Hardest"		},
};

STDDIPINFO(Mario)

static struct BurnDIPInfo MariofDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x00, NULL			},
	{0x0c, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0b, 0x01, 0x80, 0x00, "Off"			},
	{0x0b, 0x01, 0x80, 0x01, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x03, 0x00, "3"			},
	{0x0c, 0x01, 0x03, 0x01, "4"			},
	{0x0c, 0x01, 0x03, 0x02, "5"			},
	{0x0c, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0c, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x0c, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0c, 0x01, 0x30, 0x00, "20k 40k 20k+"			},
	{0x0c, 0x01, 0x30, 0x10, "30k 50k 20k+"			},
	{0x0c, 0x01, 0x30, 0x20, "40k 60k 20k+"			},
	{0x0c, 0x01, 0x30, 0x30, "None"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0c, 0x01, 0xc0, 0x00, "Easy"			},
	{0x0c, 0x01, 0xc0, 0x80, "Medium"		},
	{0x0c, 0x01, 0xc0, 0x40, "Hard"			},
	{0x0c, 0x01, 0xc0, 0xc0, "Hardest"		},
};

STDDIPINFO(Mariof)

static struct BurnDIPInfo MariojDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x00, NULL			},
	{0x0c, 0xff, 0xff, 0x20, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0b, 0x01, 0x80, 0x00, "Off"			},
	{0x0b, 0x01, 0x80, 0x01, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0c, 0x01, 0x03, 0x00, "3"			},
	{0x0c, 0x01, 0x03, 0x01, "4"			},
	{0x0c, 0x01, 0x03, 0x02, "5"			},
	{0x0c, 0x01, 0x03, 0x03, "6"			},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x0c, 0x01, 0x1c, 0x08, "3 Coins 1 Credits"	},
	{0x0c, 0x01, 0x1c, 0x10, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x1c, 0x00, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0x1c, 0x18, "1 Coin  2 Credits"	},
	{0x0c, 0x01, 0x1c, 0x04, "1 Coin  3 Credits"	},
	{0x0c, 0x01, 0x1c, 0x0c, "1 Coin  4 Credits"	},
	{0x0c, 0x01, 0x1c, 0x14, "1 Coin  5 Credits"	},
	{0x0c, 0x01, 0x1c, 0x1c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "2 Players Game"	},
	{0x0c, 0x01, 0x20, 0x00, "1 Credit"		},
	{0x0c, 0x01, 0x20, 0x20, "2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0c, 0x01, 0xc0, 0x00, "20k 50k 30k+"		},
	{0x0c, 0x01, 0xc0, 0x40, "30k 60k 30k+"		},
	{0x0c, 0x01, 0xc0, 0x80, "40k 70k 30k+"		},
	{0x0c, 0x01, 0xc0, 0xc0, "None"			},
};

STDDIPINFO(Marioj)

static UINT8 __fastcall mario_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x7c00:
			return DrvInputs[0];

		case 0x7c80:
			return DrvInputs[1];

		case 0x7f80:
			return DrvDips[0];
	}

	return 0;
}

static void play_sample(INT32 sample, INT32 checkstatus, UINT8 data)
{
	if (sample_data[sample] != data && !masao) {
		sample_data[sample] = data;
		if (!data) return;

		if (checkstatus) {
			if (BurnSampleGetStatus(sample) == 0) BurnSamplePlay(sample);
		} else {
			BurnSamplePlay(sample);
		}
	}
}

#define ACTIVEHIGH_PORT_BIT(P,A,D)   ((P & (~(1 << A))) | (D << A))

static void __fastcall mario_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x7c00:
			play_sample(3, 1, data);
		return;

		case 0x7c80:
			play_sample(4, 1, data);
		return;

		case 0x7f00:
			if (!masao) { // mario
				I8039SetIrqState((data) ? 1 : 0);
			} else { // masao
				ZetSetVector(1, 0xff);
				ZetSetIRQLine(1, 0, CPU_IRQSTATUS_ACK);
			}
		return;

		case 0x7f01:
			i8039_t[0] = data; // mario
		return;

		case 0x7f02:
			play_sample(0, 0, data);
		return;

		case 0x7f03:
			i8039_p[1] = ACTIVEHIGH_PORT_BIT(i8039_p[1], 0, data);
		return;

		case 0x7f04:
			i8039_p[1] = ACTIVEHIGH_PORT_BIT(i8039_p[1], 1, data);
		return;

		case 0x7f05:
			i8039_p[1] = ACTIVEHIGH_PORT_BIT(i8039_p[1], 2, data);
		return;

		case 0x7f06:
			play_sample(1, 0, data);
		return;

		case 0x7f07:
			play_sample(2, 0, data);
		return;

		case 0x7d00:
			*scroll = data + 0x11;
		return;

		case 0x7e00:
			*soundlatch = data;
		return;

		case 0x7e80:
			*gfxbank = data & 1;
		return;

		case 0x7e82:
			*flipscreen = data & 1;
		return;

		case 0x7e83:
			*palbank = data & 1;
		return;

		case 0x7e84:
			*interrupt_enable = data & 1;
		return;

		case 0x7e85:
			// z80dma_rdy_w(device, data & 0x01);
		return;
	}
}

static void __fastcall mario_main_write_port(UINT16 port, UINT8 /*data*/)
{
	switch (port & 0xff)
	{
		case 0x00:
			// dma
		return;
	}
}

static UINT8 __fastcall mario_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			return 0; // dma
	}

	return 0;
}

static void __fastcall masao_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
			AY8910Write(0, 1, data);
		return;

		case 0x6000:
			AY8910Write(0, 0, data);
		return;
	}
}

static UINT8 __fastcall masao_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x4000:
			return AY8910Read(0);
	}

	return 0;
}

static UINT8 masao_ay8910_read_port_A(UINT32)
{
	ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
	return *soundlatch;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1); // masao

	I8039Open(0);
	I8039Reset();
	DACReset();
	I8039Close();

	i8039_p[1] = 0xf0; // mario sound, start active low top bits

	BurnSampleReset();
	AY8910Reset(0);

	return 0;
}

static UINT8 __fastcall mario_i8039_read(UINT32 address)
{
	return DrvSndROM[address & 0x0fff];
}

static void __fastcall mario_i8039_write_port(UINT32 port, UINT8 data)
{
	if ((port & 0x1ff) <= 0xff) {
		DACSignedWrite(0, data);
	}

	switch (port & 0x1ff)
	{
		case I8039_p1:
			i8039_p[1] = data;
		return;

		case I8039_p2:
			i8039_p[2] = data;
		return;
	}
}

static UINT8 __fastcall mario_i8039_read_port(UINT32 port)
{
	if ((port & 0x1ff) <= 0xff) {
		if (i8039_p[2] & 0x80)
			return *soundlatch;
		else
			return (DrvSndROM[((i8039_p[2] & 0xf) * 256 + (port & 0xff)) & 0xfff]);
	}

	switch (port & 0x1ff)
	{
		case I8039_p1:
			return i8039_p[1];

		case I8039_p2:
			return i8039_p[2] & 0xef;

		case I8039_t0:
			return i8039_t[0];

		case I8039_t1:
			return i8039_t[1];
	}

	return 0;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (I8039TotalCycles() / (730000.0000 / (nBurnFPS / 100.0000))));
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;

	DrvSndROM		= Next; Next += 0x001000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000400;

	DrvSndRAM		= Next; Next += 0x000400;

	soundlatch		= Next; Next += 0x000001;
	i8039_p         = Next; Next += 0x000004;
	i8039_t         = Next; Next += 0x000004;

	interrupt_enable	= Next; Next += 0x000001;
	gfxbank			= Next; Next += 0x000001;
	palbank			= Next; Next += 0x000001;
	flipscreen		= Next; Next += 0x000001;
	scroll			= Next; Next += 0x000001;

	sample_data		= Next; Next += 0x000010;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2] = { 1*512*8*8, 0*512*8*8 };
	INT32 Plane1[3] = { 2*256*256, 1*256*256, 0*256*256 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8((256*16*8), 1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane0, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x6000);

	GfxDecode(0x0100, 3, 16, 16, Plane1, XOffs, YOffs, 0x080, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static void DrvPaletteInit()
{
	INT32 tab0[8] = { 0x00, 0x20, 0x46, 0x67, 0x8d, 0xb3, 0xd4, 0xfc };
	INT32 tab1[4] = { 0x00, 0x0b, 0x66, 0xff };

	for (INT32 i = 0; i < 0x100; i++) {
		UINT8 c = DrvColPROM[i];

		INT32 r = tab0[(c >> 5) & 0x07] + ((c & 0x1c) ? 0x07 : 0) + ((c & 0x03) ? 0x07 : 0);
		INT32 g = tab0[(c >> 2) & 0x07] + ((c & 0xe0) ? 0x07 : 0) + ((c & 0x03) ? 0x07 : 0);
		INT32 b = tab1[(c >> 0) & 0x03];

		if (r > 0x100) r = 0xff;
		if (g > 0x100) g = 0xff;
		if (b > 0x100) b = 0xff;

		DrvPalette[i] = BurnHighCol(r^0xfc, g^0xfc, b^0xff, 0);
	}
}

static struct BurnSampleInfo MarioSampleDesc[] = {
#if !defined (ROM_VERIFY)
	{ "ice",		SAMPLE_NOLOOP },
	{ "coin",		SAMPLE_NOLOOP },
	{ "skid",		SAMPLE_NOLOOP },
	{ "run",		SAMPLE_NOLOOP },
	{ "luigirun",	SAMPLE_NOLOOP },
#endif
	{ "", 0 }
};

STD_SAMPLE_PICK(Mario)
STD_SAMPLE_FN(Mario)

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0xf000,  3, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x0000,  4, 1)) return 1;
		if (!masao) DrvSndROM[1] = 0x01; // mario onry i8039 patch
		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x3000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x4000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x5000, 12, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 13, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM, 	0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM, 	0x6000, 0x6fff, MAP_RAM);
	ZetMapMemory(DrvSprRAM, 	0x7000, 0x73ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM, 	0x7400, 0x77ff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM + 0xf000,0xf000, 0xffff, MAP_ROM);
	ZetSetWriteHandler(mario_main_write);
	ZetSetReadHandler(mario_main_read);
	ZetSetOutHandler(mario_main_write_port);
	ZetSetInHandler(mario_main_read_port);
	ZetClose();

	I8039Init(0);
	I8039Open(0);
	I8039SetProgramReadHandler(mario_i8039_read);
	I8039SetCPUOpReadHandler(mario_i8039_read);
	I8039SetCPUOpReadArgHandler(mario_i8039_read);
	I8039SetIOReadHandler(mario_i8039_read_port);
	I8039SetIOWriteHandler(mario_i8039_write_port);
	I8039Close();

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.65, BURN_SND_ROUTE_BOTH);

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(0.25, BURN_SND_ROUTE_BOTH);

	{ // masao

		ZetInit(1);
		ZetOpen(1);
		ZetMapMemory(DrvSndROM,	0x0000, 0x0fff, MAP_ROM);
		ZetMapMemory(DrvSndRAM, 0x2000, 0x23ff, MAP_RAM);
		ZetSetWriteHandler(masao_sound_write);
		ZetSetReadHandler(masao_sound_read);
		ZetClose();

		AY8910Init(0, 2386333, 0);
		AY8910SetPorts(0, &masao_ay8910_read_port_A, NULL, NULL, NULL);
		AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
	}

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	BurnSampleExit();
	AY8910Exit(0);	// masao
	DACExit();

	GenericTilesExit();

	ZetExit();

	I8039Exit();

	BurnFree (AllMem);

	masao = 0;

	return 0;
}

static void draw_layer()
{
	INT32 scrolly = *scroll;// - (flipscreen[0] ? 8 : 0);

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sy = (offs / 32) * 8;
		INT32 sx = (offs & 31) * 8;

		if (flipscreen[0]) {
			sx ^= 0xf8;
			sy ^= 0xf8;
		}

		sy -= scrolly + 16;
		if (sy < -7) sy += 256;

		INT32 code  = DrvVidRAM[offs] | (*gfxbank << 8);
		INT32 color = ((DrvVidRAM[offs] & 0xe0) >> 4) | 0x10 | (*palbank << 5) | (monitor << 6);

		if (flipscreen[0]) {
			Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, DrvGfxROM0);
		} else {
			Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 2, 0, DrvGfxROM0);
		}
	}
}

static void draw_sprites()
{
	INT32 flip = *flipscreen ? 0xff : 0;

	memcpy (DrvSprRAM, DrvZ80RAM + 0x900, 0x180); // hack around missing dma functions

	for (INT32 offs = 0; offs < 0x400; offs += 4)
	{
		if (DrvSprRAM[offs])
		{
			INT32 sy    = (240 - ((DrvSprRAM[offs] + (flip ? 0xF7 : 0xF9) + 1) & 0xff)) ^ flip;
			INT32 sx    = DrvSprRAM[offs+3] ^ flip;

			INT32 flipx = DrvSprRAM[offs+1] & 0x80;
			INT32 flipy = DrvSprRAM[offs+1] & 0x40;

			INT32 code  = DrvSprRAM[offs+2];
			INT32 color = (DrvSprRAM[offs+1] & 0x0f) + (*palbank << 4) + (monitor << 5);

			if (flip) {
				sy -= 30;
				sx -= 7;
				flipx ^= 0x80;
				flipy ^= 0x40;
			} else {
				sy -= 15;
				sx -= 8;
			}

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0, DrvGfxROM1);
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer();
	if (nBurnLayer & 2) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0, 2);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 730000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	ZetNewFrame();
	I8039NewFrame();

	ZetOpen(0);
	I8039Open(0);
	
	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		if (i == 240 && *interrupt_enable) ZetNmi();

		nCyclesDone[1] += I8039Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);

		if (pBurnSoundOut && i&1) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 2);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnSampleRender(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			BurnSampleRender(pSoundBuf, nSegmentLength);
		}
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();
	}

	I8039Close();
	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 MasaoFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0, 2);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 1536000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0]/nInterleave);
		if (i == (nInterleave - 1) && *interrupt_enable) ZetNmi();
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1]/nInterleave);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029719;
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
		if (masao) {
			AY8910Scan(nAction, pnMin);
		} else { // mario
			I8039Scan(nAction, pnMin);

			DACScan(nAction, pnMin);
			BurnSampleScan(nAction, pnMin);
		}
	}

	return 0;
}


// Mario Bros. (US, Revision G)

static struct BurnRomInfo marioRomDesc[] = {
	{ "tma1-c-7f_g.7f",	0x2000, 0xc0c6e014, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tma1-c-7e_g.7e",	0x2000, 0x116b3856, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tma1-c-7d_g.7d",	0x2000, 0xdcceb6c1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tma1-c-7c_g.7c",	0x1000, 0x4a63d96b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tma1-c-6k_e.6k",	0x1000, 0x06b9ff85, 2 | BRF_PRG | BRF_ESS }, //  4 audiocpu

	{ "tma1-v-3f.3f",	0x1000, 0x28b0c42c, 3 | BRF_GRA },           //  5 gfx1
	{ "tma1-v-3j.3j",	0x1000, 0x0c8cc04d, 3 | BRF_GRA },           //  6

	{ "tma1-v-7m.7m",	0x1000, 0x22b7372e, 4 | BRF_GRA },           //  7 gfx2
	{ "tma1-v-7n.7n",	0x1000, 0x4f3a1f47, 4 | BRF_GRA },           //  8
	{ "tma1-v-7p.7p",	0x1000, 0x56be6ccd, 4 | BRF_GRA },           //  9
	{ "tma1-v-7s.7s",	0x1000, 0x56f1d613, 4 | BRF_GRA },           // 10
	{ "tma1-v-7t.7t",	0x1000, 0x641f0008, 4 | BRF_GRA },           // 11
	{ "tma1-v-7u.7u",	0x1000, 0x7baf5309, 4 | BRF_GRA },           // 12

	{ "tma1-c-4p.4p",	0x0200, 0xafc9bd41, 5 | BRF_GRA },           // 13 proms

	{ "tma1-c-5p.5p",	0x0020, 0x58d86098, 6 | BRF_GRA },           // 14 decoder_prom
};

STD_ROM_PICK(mario)
STD_ROM_FN(mario)

struct BurnDriver BurnDrvMario = {
	"mario", NULL, NULL, "mario", "1983",
	"Mario Bros. (US, Revision G)\0", NULL, "Nintendo of America", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, marioRomInfo, marioRomName, NULL, NULL, MarioSampleInfo, MarioSampleName, MarioInputInfo, MarioDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Mario Bros. (US, Revision F)

static struct BurnRomInfo mariofRomDesc[] = {
	{ "tma1-c-7f_f.7f",	0x2000, 0xc0c6e014, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "tma1-c-7e_f.7e",	0x2000, 0x94fb60d6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tma1-c-7d_f.7d",	0x2000, 0xdcceb6c1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tma1-c-7c_f.7c",	0x1000, 0x4a63d96b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tma1-c-6k_e.6k",	0x1000, 0x06b9ff85, 2 | BRF_PRG | BRF_ESS }, //  4 audiocpu

	{ "tma1-v-3f.3f",	0x1000, 0x28b0c42c, 3 | BRF_GRA },           //  5 gfx1
	{ "tma1-v-3j.3j",	0x1000, 0x0c8cc04d, 3 | BRF_GRA },           //  6

	{ "tma1-v-7m.7m",	0x1000, 0x22b7372e, 4 | BRF_GRA },           //  7 gfx2
	{ "tma1-v-7n.7n",	0x1000, 0x4f3a1f47, 4 | BRF_GRA },           //  8
	{ "tma1-v-7p.7p",	0x1000, 0x56be6ccd, 4 | BRF_GRA },           //  9
	{ "tma1-v-7s.7s",	0x1000, 0x56f1d613, 4 | BRF_GRA },           // 10
	{ "tma1-v-7t.7t",	0x1000, 0x641f0008, 4 | BRF_GRA },           // 11
	{ "tma1-v-7u.7u",	0x1000, 0x7baf5309, 4 | BRF_GRA },           // 12

	{ "tma1-c-4p_1.4p",	0x0200, 0x8187d286, 5 | BRF_GRA },           // 13 proms

	{ "tma1-c-5p.5p",	0x0020, 0x58d86098, 6 | BRF_GRA },           // 14 decoder_prom
};

STD_ROM_PICK(mariof)
STD_ROM_FN(mariof)

struct BurnDriver BurnDrvMariof = {
	"mariof", "mario", NULL, "mario", "1983",
	"Mario Bros. (US, Revision F)\0", NULL, "Nintendo of America", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, mariofRomInfo, mariofRomName, NULL, NULL, MarioSampleInfo, MarioSampleName, MarioInputInfo, MariofDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Mario Bros. (US, Revision E)

static struct BurnRomInfo marioeRomDesc[] = {
	{ "tma1-c-7f_e-1.7f",	0x2000, 0xc0c6e014, 1 }, //  0 maincpu
	{ "tma1-c-7e_e-3.7e",	0x2000, 0xb09ab857, 1 }, //  1
	{ "tma1-c-7d_e-1.7d",	0x2000, 0xdcceb6c1, 1 }, //  2
	{ "tma1-c-7c_e-3.7c",	0x1000, 0x0d31bd1c, 1 }, //  3

	{ "tma1-c-6k_e.6k",	0x1000, 0x06b9ff85, 2 }, //  4 audiocpu

	{ "tma1-v-3f.3f",	0x1000, 0x28b0c42c, 3 }, //  5 gfx1
	{ "tma1-v-3j.3j",	0x1000, 0x0c8cc04d, 3 }, //  6

	{ "tma1-v.7m",		0x1000, 0xd01c0e2c, 4 }, //  7 gfx2
	{ "tma1-v-7n.7n",	0x1000, 0x4f3a1f47, 4 }, //  8
	{ "tma1-v-7p.7p",	0x1000, 0x56be6ccd, 4 }, //  9
	{ "tma1-v.7s",		0x1000, 0xff856e6f, 4 }, // 10
	{ "tma1-v-7t.7t",	0x1000, 0x641f0008, 4 }, // 11
	{ "tma1-v.7u",		0x1000, 0xd2dbeb75, 4 }, // 12

	{ "tma1-c-4p_1.4p",	0x0200, 0x8187d286, 5 }, // 13 proms

	{ "tma1-c-5p.5p",	0x0020, 0x58d86098, 6 }, // 14 unk_proms
};

STD_ROM_PICK(marioe)
STD_ROM_FN(marioe)

struct BurnDriver BurnDrvMarioe = {
	"marioe", "mario", NULL, "mario", "1983",
	"Mario Bros. (US, Revision E)\0", NULL, "Nintendo of America", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, marioeRomInfo, marioeRomName, NULL, NULL, MarioSampleInfo, MarioSampleName, MarioInputInfo, MarioDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Mario Bros. (US, Unknown Rev)

static struct BurnRomInfo mariooRomDesc[] = {
	{ "tma1-c-7f_.7f",	0x2000, 0xc0c6e014, 1 }, //  0 maincpu
	{ "tma1-c-7f_.7e",	0x2000, 0x116b3856, 1 }, //  1
	{ "tma1-c-7f_.7d",	0x2000, 0xdcceb6c1, 1 }, //  2
	{ "tma1-c-7f_.7c",	0x1000, 0x4a63d96b, 1 }, //  3

	{ "tma1-c-6k_e.6k",	0x1000, 0x06b9ff85, 2 }, //  4 audiocpu

	{ "tma1-v-3f.3f",	0x1000, 0x28b0c42c, 3 }, //  5 gfx1
	{ "tma1-v-3j.3j",	0x1000, 0x0c8cc04d, 3 }, //  6

	{ "tma1-v-7m.7m",	0x1000, 0x22b7372e, 4 }, //  7 gfx2
	{ "tma1-v-7n.7n",	0x1000, 0x4f3a1f47, 4 }, //  8
	{ "tma1-v-7p.7p",	0x1000, 0x56be6ccd, 4 }, //  9
	{ "tma1-v-7s.7s",	0x1000, 0x56f1d613, 4 }, // 10
	{ "tma1-v-7t.7t",	0x1000, 0x641f0008, 4 }, // 11
	{ "tma1-v-7u.7u",	0x1000, 0x7baf5309, 4 }, // 12

	{ "tma1-c-4p.4p",	0x0200, 0xafc9bd41, 5 }, // 13 proms

	{ "tma1-c-5p.5p",	0x0020, 0x58d86098, 6 }, // 14 unk_proms
};

STD_ROM_PICK(marioo)
STD_ROM_FN(marioo)

struct BurnDriver BurnDrvMarioo = {
	"marioo", "mario", NULL, "mario", "1983",
	"Mario Bros. (US, Unknown Rev)\0", NULL, "Nintendo of America", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, mariooRomInfo, mariooRomName, NULL, NULL, MarioSampleInfo, MarioSampleName, MariooInputInfo, MarioDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


// Mario Bros. (Japan)

static struct BurnRomInfo mariojRomDesc[] = {
	{ "tma1c-a1.7f",	0x2000, 0xb64b6330, 1 }, //  0 maincpu
	{ "tma1c-a2.7e",	0x2000, 0x290c4977, 1 }, //  1
	{ "tma1c-a1.7d",	0x2000, 0xf8575f31, 1 }, //  2
	{ "tma1c-a2.7c",	0x1000, 0xa3c11e9e, 1 }, //  3

	{ "tma1c-a.6k",		0x1000, 0x06b9ff85, 2 }, //  4 audiocpu

	{ "tma1v-a.3f",		0x1000, 0xadf49ee0, 3 }, //  5 gfx1
	{ "tma1v-a.3j",		0x1000, 0xa5318f2d, 3 }, //  6

	{ "tma1v-a.7m",		0x1000, 0x186762f8, 4 }, //  7 gfx2
	{ "tma1v-a.7n",		0x1000, 0xe0e08bba, 4 }, //  8
	{ "tma1v-a.7p",		0x1000, 0x7b27c8c1, 4 }, //  9
	{ "tma1v-a.7s",		0x1000, 0x912ba80a, 4 }, // 10
	{ "tma1v-a.7t",		0x1000, 0x5cbb92a5, 4 }, // 11
	{ "tma1v-a.7u",		0x1000, 0x13afb9ed, 4 }, // 12

	{ "tma1-c-4p.4p",	0x0200, 0xafc9bd41, 5 }, // 13 proms

	{ "tma1-c-5p.5p",	0x0020, 0x58d86098, 6 }, // 14 unk_proms
};

STD_ROM_PICK(marioj)
STD_ROM_FN(marioj)

struct BurnDriver BurnDrvMarioj = {
	"marioj", "mario", NULL, "mario", "1983",
	"Mario Bros. (Japan)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, mariojRomInfo, mariojRomName, NULL, NULL, MarioSampleInfo, MarioSampleName, MarioInputInfo, MariojDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};


static INT32 DrvInitmasao()
{
	masao = 1;
	return DrvInit();
}

// Masao

static struct BurnRomInfo masaoRomDesc[] = {
	{ "masao-4.rom",	0x2000, 0x07a75745, 1 }, //  0 maincpu
	{ "masao-3.rom",	0x2000, 0x55c629b6, 1 }, //  1
	{ "masao-2.rom",	0x2000, 0x42e85240, 1 }, //  2
	{ "masao-1.rom",	0x1000, 0xb2817af9, 1 }, //  3

	{ "masao-5.rom",	0x1000, 0xbd437198, 2 }, //  4 audiocpu

	{ "masao-6.rom",	0x1000, 0x1c9e0be2, 3 }, //  5 gfx1
	{ "masao-7.rom",	0x1000, 0x747c1349, 3 }, //  6

	{ "tma1v-a.7m",		0x1000, 0x186762f8, 4 }, //  7 gfx2
	{ "masao-9.rom",	0x1000, 0x50be3918, 4 }, //  8
	{ "mario.7p",		0x1000, 0x56be6ccd, 4 }, //  9
	{ "tma1v-a.7s",		0x1000, 0x912ba80a, 4 }, // 10
	{ "tma1v-a.7t",		0x1000, 0x5cbb92a5, 4 }, // 11
	{ "tma1v-a.7u",		0x1000, 0x13afb9ed, 4 }, // 12

	{ "tma1-c-4p.4p",	0x0200, 0xafc9bd41, 5 }, // 13 proms
};

STD_ROM_PICK(masao)
STD_ROM_FN(masao)

struct BurnDriver BurnDrvMasao = {
	"masao", "mario", NULL, "mario", "1983",
	"Masao\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, masaoRomInfo, masaoRomName, NULL, NULL, MarioSampleInfo, MarioSampleName, MariooInputInfo, MarioDIPInfo,
	DrvInitmasao, DrvExit, MasaoFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};
