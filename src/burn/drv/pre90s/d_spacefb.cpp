// FB Alpha Space Firebird driver module
// Based on MAME driver by Chris Hardy

#include "tiles_generic.h"
#include "z80_intf.h"
#include "i8039.h"
#include "samples.h"
#include "dac.h"
#include "resnet.h"

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
static UINT8 *DrvObjMAP;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 soundlatch;
static UINT8 port0_data;
static UINT8 port2_data;
static UINT32 star_shift_reg;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static double color_weights_rg[3];
static double color_weights_b[2];

static struct BurnInputInfo SpacefbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Spacefb)

static struct BurnDIPInfo SpacefbDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xe0, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0c, 0x01, 0x03, 0x00, "3"					},
	{0x0c, 0x01, 0x03, 0x01, "4"					},
	{0x0c, 0x01, 0x03, 0x02, "5"					},
	{0x0c, 0x01, 0x03, 0x03, "6"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0c, 0x01, 0x0c, 0x08, "3 Coins 1 Credits"	},
	{0x0c, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0x0c, 0x0c, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x0c, 0x01, 0x10, 0x00, "5000"					},
	{0x0c, 0x01, 0x10, 0x10, "8000"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0c, 0x01, 0x20, 0x20, "Upright"				},
	{0x0c, 0x01, 0x20, 0x00, "Cocktail"				},
};

STDDIPINFO(Spacefb)

static struct BurnDIPInfo SpacedemDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xe0, NULL					},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x0c, 0x01, 0x01, 0x00, "3"					},
	{0x0c, 0x01, 0x01, 0x01, "4"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x0c, 0x01, 0x02, 0x00, "Easy"					},
	{0x0c, 0x01, 0x02, 0x02, "Hard"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x0c, 0x01, 0x0c, 0x04, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0x0c, 0x0c, "1 Coin  2 Credits"	},
	{0x0c, 0x01, 0x0c, 0x08, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x0c, 0x01, 0x10, 0x00, "5000"					},
	{0x0c, 0x01, 0x10, 0x10, "8000"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0c, 0x01, 0x20, 0x20, "Upright"				},
	{0x0c, 0x01, 0x20, 0x00, "Cocktail"				},
};

STDDIPINFO(Spacedem)

static void port1_write(UINT8 data)
{
	I8039SetIrqState((data & 2) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);

	if (!(data & 0x01) && (soundlatch & 0x01)) BurnSamplePlay(0);
	if (!(data & 0x40) && (soundlatch & 0x40)) BurnSamplePlay(1);

	if ((data & 0x80) != (soundlatch & 0x80))
	{
		if (data & 0x80)
			BurnSamplePlay(3);
		else
			BurnSamplePlay(2);
	}

	soundlatch = data;
}

static void __fastcall spacefb_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 3)
	{
		case 0x00:
			port0_data = data;
		return;

		case 0x01:
			port1_write(data);
		return;

		case 0x02:
			port2_data = data;
		return;

		case 0x03:
			// nop
		return;
	}
}

static UINT8 __fastcall spacefb_main_read_port(UINT16 port)
{
	switch (port & 7)
	{
		case 0x00:
			return DrvInputs[0];

		case 0x01:
			return DrvInputs[1];

		case 0x02:
			return DrvInputs[2];

		case 0x03:
			return DrvDips[0];

		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			return 0; // nop
	}

	return 0;
}

static UINT8 __fastcall spacefb_i8035_read(UINT32 address)
{
	return DrvSndROM[address & 0x03ff];
}

static void __fastcall spacefb_i8035_write_port(UINT32 port, UINT8 data)
{
	switch (port & 0x1ff)
	{
		case I8039_p1:
			DACWrite(0, data);
		return;
	}
}

static UINT8 __fastcall spacefb_i8035_read_port(UINT32 port)
{
	switch (port & 0x1ff)
	{
		case I8039_p2:
			return (soundlatch & 0x18) << 1;

		case I8039_t0:
			return (soundlatch >> 5) & 1;

		case I8039_t1:
			return (soundlatch >> 2) & 1;
	}

	return 0;
}
#if 0
static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (I8039TotalCycles() / (400000.0000 / (nBurnFPS / 100.0000))));
}
#endif

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	I8039Open(0);
	I8039Reset();
	DACReset();
	I8039Close();

	BurnSampleReset();

	port0_data = 0;
	port2_data = 0;
	star_shift_reg = 0x18f89;
	soundlatch = 0;

	{ // init resnet for colors
		const INT32 resistances_rg[] = { 1000, 470, 220 };
		const INT32 resistances_b [] = {       470, 220 };

		compute_resistor_weights(0, 0xff, -1.0,
								 3, resistances_rg, color_weights_rg, 470, 0,
								 2, resistances_b,  color_weights_b,  470, 0,
								 0, NULL, NULL, 0, 0);
	}

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x004000;
	DrvSndROM		= Next; Next += 0x000400;

	DrvGfxROM0		= Next; Next += 0x001000;
	DrvGfxROM1		= Next; Next += 0x000100;

	DrvColPROM		= Next; Next += 0x000020;
	
	DrvObjMAP		= Next; Next += 512 * 256;

	DrvPalette		= (UINT32*)Next; Next += 0x0081 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

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

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x0800,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1800,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2800,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x3000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x3800,  7, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x0000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0800, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000, 11, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 12, 1)) return 1;
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x3fff, MAP_ROM);
	for (INT32 i = 0; i < 0x4000; i+= 0x400) {
		ZetMapMemory(DrvVidRAM,		0x8000 + i, 0x83ff + i, MAP_RAM);
	}
	for (INT32 i = 0; i < 0x4000; i+= 0x1000) {
		ZetMapMemory(DrvZ80RAM,		0xc000 + i, 0xc7ff + i, MAP_RAM);
	}
	ZetSetOutHandler(spacefb_main_write_port);
	ZetSetInHandler(spacefb_main_read_port);
	ZetClose();
	
	I8035Init(0);
	I8039Open(0);
	I8039SetProgramReadHandler(spacefb_i8035_read);
	I8039SetCPUOpReadHandler(spacefb_i8035_read);
	I8039SetCPUOpReadArgHandler(spacefb_i8035_read);
	I8039SetIOReadHandler(spacefb_i8035_read_port);
	I8039SetIOWriteHandler(spacefb_i8035_write_port);
	I8039Close();

	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(0.25, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, I8039TotalCycles, 400000);
	//DACInit(0, 0, 1, DrvSyncDAC); // for gamezfan
	DACSetRoute(0, 0.65, BURN_SND_ROUTE_BOTH);
	
	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ZetExit();
	I8039Exit();

	BurnSampleExit();
	DACExit();

	GenericTilesExit();

	BurnFree(AllMem);

	return 0;
}

static void get_starfield_pens()
{
	INT32 color_contrast_r   = port2_data & 0x01;
	INT32 color_contrast_g   = port2_data & 0x02;
	INT32 color_contrast_b   = port2_data & 0x04;
	INT32 background_red     = port2_data & 0x08;
	INT32 background_blue    = port2_data & 0x10;
	INT32 disable_star_field = port2_data & 0x80;

	for (INT32 i = 0; i < 0x40; i++)
	{
		UINT8 gb =  ((i >> 0) & 0x01) && color_contrast_g && !disable_star_field;
		UINT8 ga =  ((i >> 1) & 0x01) && !disable_star_field;
		UINT8 bb =  ((i >> 2) & 0x01) && color_contrast_b && !disable_star_field;
		UINT8 ba = (((i >> 3) & 0x01) || background_blue) && !disable_star_field;
		UINT8 ra = (((i >> 4) & 0x01) || background_red) && !disable_star_field;
		UINT8 rb =  ((i >> 5) & 0x01) && color_contrast_r && !disable_star_field;

		UINT8 r = combine_3_weights(color_weights_rg, 0, rb, ra);
		UINT8 g = combine_3_weights(color_weights_rg, 0, gb, ga);
		UINT8 b = combine_2_weights(color_weights_b,     bb, ba);

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void get_sprite_pens()
{
	static const double fade_weights[] = { 1.0, 1.5, 2.5, 4.0 };

	for (INT32 i = 0; i < 0x40; i++)
	{
		UINT8 data = DrvColPROM[((port0_data & 0x40) >> 2) | (i & 0x0f)];

		UINT8 r0 = (data >> 0) & 0x01;
		UINT8 r1 = (data >> 1) & 0x01;
		UINT8 r2 = (data >> 2) & 0x01;

		UINT8 g0 = (data >> 3) & 0x01;
		UINT8 g1 = (data >> 4) & 0x01;
		UINT8 g2 = (data >> 5) & 0x01;

		UINT8 b1 = (data >> 6) & 0x01;
		UINT8 b2 = (data >> 7) & 0x01;

	    UINT8 r = combine_3_weights(color_weights_rg, r0, r1, r2);
		UINT8 g = combine_3_weights(color_weights_rg, g0, g1, g2);
		UINT8 b = combine_2_weights(color_weights_b,      b1, b2);

		if (i >> 4)
		{
			double fade_weight = fade_weights[i >> 4];

			r = (UINT8)((r / fade_weight) + 0.5);
			g = (UINT8)((g / fade_weight) + 0.5);
			b = (UINT8)((b / fade_weight) + 0.5);
		}

		DrvPalette[i + 0x40] = BurnHighCol(r, g, b, 0);
	}
}

static inline void shift_star_generator()
{
	star_shift_reg = ((star_shift_reg << 1) | (((~star_shift_reg >> 16) & 0x01) ^ ((star_shift_reg >> 4) & 0x01))) & 0x1ffff;
}

static void draw_starfield()
{
	{
		INT32 clock_count = 0x200 * 0x10 - 1;

		for (INT32 i = 0; i < clock_count; i++)
			shift_star_generator();
	}

	for (INT32 y = 16; y < 240; y++)
	{
		UINT16 *bDst = pTransDraw + (y - 16) * 512;
		UINT8 *tDst = DrvObjMAP + (y * 512);
		
		for (INT32 x = 0; x < 512; x++)
		{
			if (tDst[x] == 0)
			{
				if (((star_shift_reg & 0x1c0ff) == 0x0c0b7) ||
					((star_shift_reg & 0x1c0ff) == 0x0c0d7) ||
					((star_shift_reg & 0x1c0ff) == 0x0c0bb) ||
					((star_shift_reg & 0x1c0ff) == 0x0c0db))
						bDst[x] = (star_shift_reg >> 8) & 0x3f;
				else
					bDst[x] = 0;
			}

			shift_star_generator();
		}
	}

	{
		INT32 clock_count = 0x200 * (0x100 - 0xf0);

		for (INT32 i = 0; i < clock_count; i++)
			shift_star_generator();
	}
}

static void draw_bullet(INT32 offs, INT32 flip)
{
	UINT8 *gfx = DrvGfxROM1;

	UINT8 code = DrvVidRAM[offs + 0x0200] & 0x3f;
	UINT8 y = ~DrvVidRAM[offs + 0x0100] - 2;

	for (UINT8 sy = 0; sy < 4; sy++)
	{
		UINT8 dy;

		UINT8 data = gfx[(code << 2) | sy];
		UINT8 x = DrvVidRAM[offs + 0x0000];

		if (flip)
			dy = ~y;
		else
			dy = y;

		if (dy >= 16 && dy < 240)
		{
			UINT16 *bDst = pTransDraw + (dy - 16) * 512;
			UINT8 *tDst = DrvObjMAP + (dy * 512);
			
			for (UINT8 sx = 0; sx < 4; sx++)
			{
				if (data & 0x01)
				{
					UINT16 dx;

					if (flip)
						dx = (255 - x) * 2;
					else
						dx = x * 2;

					bDst[dx + 0] = 0x80;
					bDst[dx + 1] = 0x80;

					tDst[dx + 0] = 1;
					tDst[dx + 1] = 1;
				}

				x = x + 1;
				data = data >> 1;
			}
		}

		y = y + 1;
	}
}

static void draw_sprite(INT32 offs, INT32 flip)
{
	UINT8 *gfx = DrvGfxROM0;
	UINT8 code = ~DrvVidRAM[offs + 0x0200];
	UINT8 color_base = (~DrvVidRAM[offs + 0x0300] & 0x0f) << 2;
	UINT8 y = ~DrvVidRAM[offs + 0x0100] - 2;

	for (UINT8 sy = 0; sy < 8; sy++)
	{
		UINT8 sx, dy;

		UINT8 data1 = gfx[0x0000 | (code << 3) | (sy ^ 0x07)];
		UINT8 data2 = gfx[0x0800 | (code << 3) | (sy ^ 0x07)];

		UINT8 x = DrvVidRAM[offs + 0x0000] - 3;

		if (flip)
			dy = ~y;
		else
			dy = y;

		if ((INT32)dy >= 16 && dy < 240)
		{
			UINT16 *bDst = pTransDraw + (dy - 16) * 512;
			UINT8 *tDst = DrvObjMAP + (dy * 512);
			
			for (sx = 0; sx < 8; sx++)
			{
				UINT16 dx;
				UINT8 data;
				UINT8 pen;

				if (flip)
					dx = (255 - x) * 2;
				else
					dx = x * 2;

				data = ((data1 << 1) & 0x02) | (data2 & 0x01);
				pen = ((color_base | data) & 0x3f) | 0x40;

				bDst[dx + 0] = pen;
				bDst[dx + 1] = pen;

				tDst[dx + 0] = (data != 0);
				tDst[dx + 1] = (data != 0);

				x = x + 1;
				data1 = data1 >> 1;
				data2 = data2 >> 1;
			}
		}

		y = y + 1;
	}
}

static void draw_objects()
{
	INT32 offs = (port0_data & 0x20) ? 0x80 : 0x00;
	INT32 flip = port0_data & 0x01;

	memset(DrvObjMAP, 0, 512 * 256);

	while (1)
	{
		if (DrvVidRAM[offs + 0x0300] & 0x20)
			draw_bullet(offs, flip);
		else if (DrvVidRAM[offs + 0x0300] & 0x40)
			draw_sprite(offs, flip);

		offs = offs + 1;

		if ((offs & 0x7f) == 0)  break;
	}
}

static INT32 DrvDraw()
{
	get_starfield_pens();
	get_sprite_pens();
	DrvPalette[0x80] = BurnHighCol(0xff, 0, 0, 0); // red bullets

	draw_objects();
	draw_starfield();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void dcfilter_dac()
{
	// dc-blocking filter
	static INT16 dac_lastin_r  = 0;
	static INT16 dac_lastout_r = 0;
	static INT16 dac_lastin_l  = 0;
	static INT16 dac_lastout_l = 0;

	for (INT32 i = 0; i < nBurnSoundLen; i++) {
		INT16 r = pBurnSoundOut[i*2+0];
		INT16 l = pBurnSoundOut[i*2+1];

		INT16 outr = r - dac_lastin_r + 0.995 * dac_lastout_r;
		INT16 outl = l - dac_lastin_l + 0.995 * dac_lastout_l;

		dac_lastin_r = r;
		dac_lastout_r = outr;
		dac_lastin_l = l;
		dac_lastout_l = outl;
		pBurnSoundOut[i*2+0] = outr;
		pBurnSoundOut[i*2+1] = outl;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}
	
	ZetNewFrame();
	I8039NewFrame();

	{
		memset (DrvInputs, 0, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 400000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetOpen(0);
	I8039Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);

		if (i == 128) {
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		if (i == 240) {
			if (pBurnDraw) {
				DrvDraw();
			}

			ZetSetVector(0xcf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		nCyclesDone[1] += I8039Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
	}
	
	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
		dcfilter_dac();
	}

	I8039Close();
	ZetClose();

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
		I8039Scan(nAction, pnMin);

		DACScan(nAction, pnMin);
		BurnSampleScan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(star_shift_reg);
		SCAN_VAR(port0_data);
		SCAN_VAR(port2_data);
	}

	return 0;
}

static struct BurnSampleInfo SpacefbSampleDesc[] = {
	{ "ekilled",		SAMPLE_NOLOOP },
	{ "shipfire",		SAMPLE_NOLOOP },
	{ "explode1",		SAMPLE_NOLOOP },
	{ "explode2",		SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Spacefb)
STD_SAMPLE_FN(Spacefb)


// Space Firebird (rev. 04-u)

static struct BurnRomInfo spacefbRomDesc[] = {
	{ "tst-c-u.5e",		0x0800, 0x79c3527e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "tst-c-u.5f",		0x0800, 0xc0973965, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tst-c-u.5h",		0x0800, 0x02c60ec5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tst-c-u.5i",		0x0800, 0x76fd18c7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tst-c-u.5j",		0x0800, 0xdf52c97c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tst-c-u.5k",		0x0800, 0x1713300c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "tst-c-u.5m",		0x0800, 0x6286f534, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "tst-c-u.5n",		0x0800, 0x1c9f91ee, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic20.snd",		0x0400, 0x1c8670b3, 2 | BRF_PRG | BRF_ESS }, //  8 I8035 Code

	{ "tst-v-a.5k",		0x0800, 0x236e1ff7, 3 | BRF_GRA },           //  9 Sprites
	{ "tst-v-a.6k",		0x0800, 0xbf901a4e, 3 | BRF_GRA },           // 10

	{ "4i.vid",			0x0100, 0x528e8533, 4 | BRF_GRA },           // 11 Bullets

	{ "mb7051.3n",		0x0020, 0x465d07af, 5 | BRF_GRA },           // 12 Color data
};

STD_ROM_PICK(spacefb)
STD_ROM_FN(spacefb)

struct BurnDriver BurnDrvSpacefb = {
	"spacefb", NULL, NULL, "spacefb", "1980",
	"Space Firebird (rev. 04-u)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, spacefbRomInfo, spacefbRomName, NULL, NULL, SpacefbSampleInfo, SpacefbSampleName, SpacefbInputInfo, SpacefbDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x81,
	224, 512, 3, 4
};


// Space Firebird (rev. 03-e set 1)

static struct BurnRomInfo spacefbeRomDesc[] = {
	{ "tst-c-e.5e",		0x0800, 0x77dda05b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "tst-c-e.5f",		0x0800, 0x89f0c34a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tst-c-e.5h",		0x0800, 0xc4bcac3e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tst-c-e.5i",		0x0800, 0x61c00a65, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tst-c-e.5j",		0x0800, 0x598420b9, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tst-c-e.5k",		0x0800, 0x1713300c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "tst-c-e.5m",		0x0800, 0x6286f534, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "tst-c-e.5n",		0x0800, 0x1c9f91ee, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic20.snd",		0x0400, 0x1c8670b3, 2 | BRF_PRG | BRF_ESS }, //  8 I8035 Code

	{ "tst-v-a.5k",		0x0800, 0x236e1ff7, 3 | BRF_GRA },           //  9 Sprites
	{ "tst-v-a.6k",		0x0800, 0xbf901a4e, 3 | BRF_GRA },           // 10

	{ "4i.vid",			0x0100, 0x528e8533, 4 | BRF_GRA },           // 11 Bullets

	{ "mb7051.3n",		0x0020, 0x465d07af, 5 | BRF_GRA },           // 12 Color data
};

STD_ROM_PICK(spacefbe)
STD_ROM_FN(spacefbe)

struct BurnDriver BurnDrvSpacefbe = {
	"spacefbe", "spacefb", NULL, "spacefb", "1980",
	"Space Firebird (rev. 03-e set 1)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, spacefbeRomInfo, spacefbeRomName, NULL, NULL, SpacefbSampleInfo, SpacefbSampleName, SpacefbInputInfo, SpacefbDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x81,
	224, 512, 3, 4
};


// Space Firebird (rev. 03-e set 2)

static struct BurnRomInfo spacefbe2RomDesc[] = {
	{ "5e.cpu",			0x0800, 0x2d406678, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "tst-c-e.5f",		0x0800, 0x89f0c34a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tst-c-e.5h",		0x0800, 0xc4bcac3e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tst-c-e.5i",		0x0800, 0x61c00a65, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tst-c-e.5j",		0x0800, 0x598420b9, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tst-c-e.5k",		0x0800, 0x1713300c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "tst-c-e.5m",		0x0800, 0x6286f534, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "tst-c-e.5n",		0x0800, 0x1c9f91ee, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic20.snd",		0x0400, 0x1c8670b3, 2 | BRF_PRG | BRF_ESS }, //  8 I8035 Code

	{ "tst-v-a.5k",		0x0800, 0x236e1ff7, 3 | BRF_GRA },           //  9 Sprites
	{ "tst-v-a.6k",		0x0800, 0xbf901a4e, 3 | BRF_GRA },           // 10

	{ "4i.vid",			0x0100, 0x528e8533, 4 | BRF_GRA },           // 11 Bullets

	{ "mb7051.3n",		0x0020, 0x465d07af, 5 | BRF_GRA },           // 12 Color data
};

STD_ROM_PICK(spacefbe2)
STD_ROM_FN(spacefbe2)

struct BurnDriver BurnDrvSpacefbe2 = {
	"spacefbe2", "spacefb", NULL, "spacefb", "1980",
	"Space Firebird (rev. 03-e set 2)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, spacefbe2RomInfo, spacefbe2RomName, NULL, NULL, SpacefbSampleInfo, SpacefbSampleName, SpacefbInputInfo, SpacefbDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x81,
	224, 512, 3, 4
};


// Space Firebird (rev. 02-a)

static struct BurnRomInfo spacefbaRomDesc[] = {
	{ "tst-c-a.5e",		0x0800, 0x5657bd2f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "tst-c-a.5f",		0x0800, 0x303b0294, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tst-c-a.5h",		0x0800, 0x49a26fe5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tst-c-a.5i",		0x0800, 0xc23025da, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tst-c-a.5j",		0x0800, 0x946bee5d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tst-c-a.5k",		0x0800, 0x1713300c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "tst-c-a.5m",		0x0800, 0x4cbe92fc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "tst-c-a.5n",		0x0800, 0x1a798fbf, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "tst-e-20.bin",	0x0400, 0xf7a59492, 2 | BRF_PRG | BRF_ESS }, //  8 I8035 Code

	{ "tst-v-a.5k",		0x0800, 0x236e1ff7, 3 | BRF_GRA },           //  9 Sprites
	{ "tst-v-a.6k",		0x0800, 0xbf901a4e, 3 | BRF_GRA },           // 10

	{ "mb7052-a.4i",	0x0100, 0x528e8533, 4 | BRF_GRA },           // 11 Bullets

	{ "mb7051-a.3n",	0x0020, 0x465d07af, 5 | BRF_GRA },           // 12 Color data
};

STD_ROM_PICK(spacefba)
STD_ROM_FN(spacefba)

struct BurnDriver BurnDrvSpacefba = {
	"spacefba", "spacefb", NULL, "spacefb", "1980",
	"Space Firebird (rev. 02-a)\0", NULL, "Nintendo", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, spacefbaRomInfo, spacefbaRomName, NULL, NULL, SpacefbSampleInfo, SpacefbSampleName, SpacefbInputInfo, SpacefbDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x81,
	224, 512, 3, 4
};


// Space Firebird (Gremlin)

static struct BurnRomInfo spacefbgRomDesc[] = {
	{ "tst-c.5e",		0x0800, 0x07949110, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "tst-c.5f",		0x0800, 0xce591929, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tst-c.5h",		0x0800, 0x55d34ea5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tst-c.5i",		0x0800, 0xa11e2881, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tst-c.5j",		0x0800, 0xa6aff352, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tst-c.5k",		0x0800, 0xf4213603, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "5m.cpu",			0x0800, 0x6286f534, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "5n.cpu",			0x0800, 0x1c9f91ee, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic20.snd",		0x0400, 0x1c8670b3, 2 | BRF_PRG | BRF_ESS }, //  8 I8035 Code

	{ "tst-v.5k",		0x0800, 0xbacc780d, 3 | BRF_GRA },           //  9 Sprites
	{ "tst-v.6k",		0x0800, 0x1645ff26, 3 | BRF_GRA },           // 10

	{ "4i.vid",			0x0100, 0x528e8533, 4 | BRF_GRA },           // 11 Bullets

	{ "mb7051.3n",		0x0020, 0x465d07af, 5 | BRF_GRA },           // 12 Color data
};

STD_ROM_PICK(spacefbg)
STD_ROM_FN(spacefbg)

struct BurnDriver BurnDrvSpacefbg = {
	"spacefbg", "spacefb", NULL, "spacefb", "1980",
	"Space Firebird (Gremlin)\0", NULL, "Nintendo (Gremlin license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, spacefbgRomInfo, spacefbgRomName, NULL, NULL, SpacefbSampleInfo, SpacefbSampleName, SpacefbInputInfo, SpacefbDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x81,
	224, 512, 3, 4
};


// Space Bird (bootleg)

static struct BurnRomInfo spacebrdRomDesc[] = {
	{ "sb5e.cpu",		0x0800, 0x232d66b8, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "sb5f.cpu",		0x0800, 0x99504327, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sb5h.cpu",		0x0800, 0x49a26fe5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sb5i.cpu",		0x0800, 0xc23025da, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sb5j.cpu",		0x0800, 0x5e97baf0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "5k.cpu",			0x0800, 0x1713300c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sb5m.cpu",		0x0800, 0x4cbe92fc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sb5n.cpu",		0x0800, 0x1a798fbf, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "ic20.snd",		0x0400, 0x1c8670b3, 2 | BRF_PRG | BRF_ESS }, //  8 I8035 Code

	{ "5k.vid",			0x0800, 0x236e1ff7, 3 | BRF_GRA },           //  9 Sprites
	{ "6k.vid",			0x0800, 0xbf901a4e, 3 | BRF_GRA },           // 10

	{ "4i.vid",			0x0100, 0x528e8533, 4 | BRF_GRA },           // 11 Bullets

	{ "spcbird.clr",	0x0020, 0x25c79518, 5 | BRF_GRA },           // 12 Color data
};

STD_ROM_PICK(spacebrd)
STD_ROM_FN(spacebrd)

struct BurnDriver BurnDrvSpacebrd = {
	"spacebrd", "spacefb", NULL, "spacefb", "1980",
	"Space Bird (bootleg)\0", NULL, "bootleg (Karateco)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, spacebrdRomInfo, spacebrdRomName, NULL, NULL, SpacefbSampleInfo, SpacefbSampleName, SpacefbInputInfo, SpacefbDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x81,
	224, 512, 3, 4
};


// Space Firebird (bootleg)

static struct BurnRomInfo spacefbbRomDesc[] = {
	{ "fc51",			0x0800, 0x5657bd2f, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "fc52",			0x0800, 0x303b0294, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sb5h.cpu",		0x0800, 0x49a26fe5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sb5i.cpu",		0x0800, 0xc23025da, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "fc55",			0x0800, 0x946bee5d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "5k.cpu",			0x0800, 0x1713300c, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sb5m.cpu",		0x0800, 0x4cbe92fc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sb5n.cpu",		0x0800, 0x1a798fbf, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "fb.snd",			0x0400, 0xf7a59492, 2 | BRF_PRG | BRF_ESS }, //  8 I8035 Code

	{ "fc59",			0x0800, 0xa00ad16c, 3 | BRF_GRA },           //  9 Sprites
	{ "6k.vid",			0x0800, 0xbf901a4e, 3 | BRF_GRA },           // 10

	{ "4i.vid",			0x0100, 0x528e8533, 4 | BRF_GRA },           // 11 Bullets

	{ "mb7051.3n",		0x0020, 0x465d07af, 5 | BRF_GRA },           // 12 Color data
};

STD_ROM_PICK(spacefbb)
STD_ROM_FN(spacefbb)

struct BurnDriver BurnDrvSpacefbb = {
	"spacefbb", "spacefb", NULL, "spacefb", "1980",
	"Space Firebird (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, spacefbbRomInfo, spacefbbRomName, NULL, NULL, SpacefbSampleInfo, SpacefbSampleName, SpacefbInputInfo, SpacefbDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x81,
	224, 512, 3, 4
};


// Space Demon

static struct BurnRomInfo spacedemRomDesc[] = {
	{ "sdm-c-5e",		0x0800, 0xbe4b9cbb, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "sdm-c-5f",		0x0800, 0x0814f964, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sdm-c-5h",		0x0800, 0xebfff682, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sdm-c-5i",		0x0800, 0xdd7e1378, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sdm-c-5j",		0x0800, 0x98334fda, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sdm-c-5k",		0x0800, 0xba4933b2, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sdm-c-5m",		0x0800, 0x14d3c656, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sdm-c-5n",		0x0800, 0x7e0e41b0, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "sdm-e-20",		0x0400, 0x55f40a0b, 2 | BRF_PRG | BRF_ESS }, //  8 I8035 Code

	{ "sdm-v-5k",		0x0800, 0x55758e4d, 3 | BRF_GRA },           //  9 Sprites
	{ "sdm-v-6k",		0x0800, 0x3fcbb20c, 3 | BRF_GRA },           // 10

	{ "4i.vid",			0x0100, 0x528e8533, 4 | BRF_GRA },           // 11 Bullets

	{ "sdm-v-3n",		0x0020, 0x6d8ad169, 5 | BRF_GRA },           // 12 Color data
};

STD_ROM_PICK(spacedem)
STD_ROM_FN(spacedem)

struct BurnDriver BurnDrvSpacedem = {
	"spacedem", "spacefb", NULL, "spacefb", "1980",
	"Space Demon\0", NULL, "Nintendo (Fortrek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, spacedemRomInfo, spacedemRomName, NULL, NULL, SpacefbSampleInfo, SpacefbSampleName, SpacefbInputInfo, SpacedemDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x81,
	224, 512, 3, 4
};


// Star Warrior

static struct BurnRomInfo starwarrRomDesc[] = {
	{ "sw51.5e",		0x0800, 0xa0f5e690, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "sw52.5f",		0x0800, 0x303b0294, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sw53.5h",		0x0800, 0x49a26fe5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sw54.5i",		0x0800, 0xc23025da, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "sw55.5j",		0x0800, 0x946bee5d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "sw56.5k",		0x0800, 0x8a2de5f0, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "sw57.5m",		0x0800, 0x4cbe92fc, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "sw58.5n",		0x0800, 0x1a798fbf, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "sw00.snd",		0x0400, 0xf7a59492, 2 | BRF_PRG | BRF_ESS }, //  8 I8035 Code

	{ "sw59.5k",		0x0800, 0x236e1ff7, 3 | BRF_GRA },           //  9 Sprites
	{ "sw60.6k",		0x0800, 0xbf901a4e, 3 | BRF_GRA },           // 10

	{ "mb7052.4i",		0x0100, 0x528e8533, 4 | BRF_GRA },           // 11 Bullets

	{ "mb7051.3n",		0x0020, 0x465d07af, 5 | BRF_GRA },           // 12 Color data
};

STD_ROM_PICK(starwarr)
STD_ROM_FN(starwarr)

struct BurnDriver BurnDrvStarwarr = {
	"starwarr", "spacefb", NULL, "spacefb", "1980",
	"Star Warrior\0", NULL, "bootleg (Potomac Mortgage)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, starwarrRomInfo, starwarrRomName, NULL, NULL, SpacefbSampleInfo, SpacefbSampleName, SpacefbInputInfo, SpacefbDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x81,
	224, 512, 3, 4
};
