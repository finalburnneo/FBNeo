// FB Neo Gridlee driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "samples.h"
#include "watchdog.h"
#include "burn_gun.h" // trackball

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 flipscreen;
static INT32 palettebank_buffer[256];
static INT32 palettebank;

static UINT8 *soundregs;
static UINT32 tone_step;
static UINT32 tone_pos;
static UINT8 tone_vol;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT16 Analog[4];

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo GridleeInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog[1],		"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog[0],		"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 start"	},
	A("P2 Trackball X", BIT_ANALOG_REL, &Analog[3],		"p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &Analog[2],		"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Gridlee)

static struct BurnDIPInfo GridleeDIPList[]=
{
	DIP_OFFSET(0x0c)
	{0x00, 0xff, 0xff, 0x00, NULL						},
	{0x01, 0xff, 0xff, 0x05, NULL						},
	{0x02, 0xff, 0xff, 0x20, NULL						},

	{0   , 0xfe, 0   ,    3, "Coinage"					},
	{0x00, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x00, 0x01, 0x30, 0x00, "1 Coin  1 Credits"		},
	{0x00, 0x01, 0x30, 0x10, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"				},
	{0x01, 0x01, 0x03, 0x00, "8000 points"				},
	{0x01, 0x01, 0x03, 0x01, "10000 points"				},
	{0x01, 0x01, 0x03, 0x02, "12000 points"				},
	{0x01, 0x01, 0x03, 0x03, "14000 points"				},

	{0   , 0xfe, 0   ,    4, "Lives"					},
	{0x01, 0x01, 0x0c, 0x00, "2"						},
	{0x01, 0x01, 0x0c, 0x04, "3"						},
	{0x01, 0x01, 0x0c, 0x08, "4"						},
	{0x01, 0x01, 0x0c, 0x0c, "5"						},

	{0   , 0xfe, 0   ,    2, "Free Play"				},
	{0x01, 0x01, 0x10, 0x00, "Off"						},
	{0x01, 0x01, 0x10, 0x10, "On"						},

	{0   , 0xfe, 0   ,    2, "Cabinet"					},
	{0x01, 0x01, 0x20, 0x00, "Upright"					},
	{0x01, 0x01, 0x20, 0x20, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Reset Hall of Fame"		},
	{0x01, 0x01, 0x40, 0x00, "No"						},
	{0x01, 0x01, 0x40, 0x40, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Reset Game Data"			},
	{0x01, 0x01, 0x80, 0x00, "No"						},
	{0x01, 0x01, 0x80, 0x80, "Yes"						},

	{0   , 0xfe, 0   ,    2, "Service Mode"				},
	{0x02, 0x01, 0x20, 0x00, "On"						},
	{0x02, 0x01, 0x20, 0x20, "Off"						},
};

STDDIPINFO(Gridlee)

#include "stream.h"
static Stream stream;

static void sound_tone_render(INT16 **streams, INT32 len)
{
	INT16 *dest = streams[0];

	memset(dest, 0, len*2);

	if (tone_step) {
		while (len-- > 0) {
			INT16 square = (tone_pos & 0x800000) ? (tone_vol << 6) : 0;
			*dest++ = square;

			tone_pos += tone_step;
		}
	}
}

static void gridlee_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff80) == 0x9000) {
		switch ((address >> 4) & 7)
		{
			case 0: // led0
			case 1: // led1
			case 2: // coin counter
			case 6: // unknown
			return;

			case 7:
				flipscreen = data;
			return;
		}

		return;
	}

	if (address >= 0x9828 && address <= 0x993f) {
		address -= 0x9828;

		switch (address) {
			case 0x04:
				if (data == 0xef && soundregs[address] != 0xef) {
					BurnSampleChannelPlay(4, 1);
				} else if (data != 0xef && soundregs[address] == 0xef) {
					BurnSampleChannelStop(4);
				}
				break;
			case 0x0c:
			case 0x0d:
			case 0x0e:
			case 0x0f:
				if (data & 1 && ~soundregs[address] & 1) {
					BurnSampleChannelPlay(address - 0x0c, 1 - soundregs[address - 4]);
				} else if (~data & 1 && soundregs[address] & 1) {
					BurnSampleChannelStop(address - 0x0c);
				}
				break;
			case 0x10:
				stream.update();
				tone_step = (data) ? (((double)(1<<24) / nBurnSoundRate) * (data * 5)) : 0;
				break;
			case 0x11:
				stream.update();
				tone_vol = data;
				break;
		}

		soundregs[address] = data;

		return;
	}

	switch (address)
	{
		case 0x9200:
			palettebank = data & 0x3f;
		return;

		case 0x9380:
			BurnWatchdogWrite();
		return;

		case 0x9700:
		return; // nop
	}
}

static INT32 tb_last[4];
static INT32 tb_last_dir[4];
static INT32 tb_accu[4];
static INT32 tb_target[4];
static INT32 tb_chunk[4];

static UINT8 tb_read(INT32 offset)
{
	INT32 ID = (2 * flipscreen) + offset;

	INT32 direction = BurnTrackballGetDirection(ID);
	UINT8 tb = BurnTrackballRead(ID);

	if (tb_last_dir[ID] != direction) {
		// direction change?
		// cancel remains of previous movement
		tb_target[ID] = tb_accu[ID];
	}

	if (tb_last[ID] != tb) {
		// we have movement!
		// game expects a 4-bit accumulator (positive counting) & direction bit
		// ...d aaaa	d.direction	a.accu
		tb_target[ID] += BurnTrackballGetVelocity(ID);

		// divide and spread value over 4 reads (if possible)
		// game makes 4 reads per axis
		tb_chunk[ID] = BurnTrackballGetVelocity(ID) / 4;
		if (!tb_chunk[ID]) tb_chunk[ID] = BurnTrackballGetVelocity(ID) / 2;
		if (!tb_chunk[ID]) tb_chunk[ID] = BurnTrackballGetVelocity(ID);
	}

	{
		// move accumulator towards target
		if ((tb_accu[ID]+tb_chunk[ID]) <= tb_target[ID]) tb_accu[ID] += tb_chunk[ID];
		else if (tb_accu[ID] < tb_target[ID]) tb_accu[ID]++;
	}

	UINT8 res = (tb_accu[ID]&0xf) | ((direction < 0) ? 0x10 : 0x00);

	tb_last[ID] = tb;
	tb_last_dir[ID] = direction;

	//bprintf(0, _T("d_res%d =  %x\tdirection %d\taccu  %d\ttarget  %d\tchunk  %d\n"), offset, res, direction, tb_accu[ID], tb_target[ID], tb_chunk[ID]);
	return res;
}

static UINT8 gridlee_read(UINT16 address)
{
	switch (address)
	{
		case 0x9500:
		case 0x9501:
			return tb_read(address & 1);

		case 0x9502:
			return DrvInputs[0];

		case 0x9503:
			return (DrvInputs[1] & 0xcf) | (DrvDips[0] & 0x30);

		case 0x9600:
			return DrvDips[1];

		case 0x9700:
			return (vblank ? 0x80 : 0) | (DrvInputs[2] & 0x5f) | (DrvDips[2] & 0x20); // in2

		case 0x9820:
			return BurnRandom(); // random_num_r
	}

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	BurnWatchdogReset();

	BurnSampleReset();

	palettebank = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x010000;

	DrvGfxROM		= Next; Next += 0x004000;

	DrvColPROM		= Next; Next += 0x001800;

	DrvPalette		= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000100;

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x008000;

	soundregs       = Next; Next += 0x000024;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvM6809ROM + 0xa000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xb000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xc000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xd000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xe000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0xf000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM   + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x1000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x3000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x0800, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x1000, k++, 1)) return 1;
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvSprRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x0800, 0x7fff, MAP_RAM);
	M6809MapMemory(DrvNVRAM,		0x9c00, 0x9cff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0xa000,	0xa000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(gridlee_write);
	M6809SetReadHandler(gridlee_read);
	M6809Close();

	BurnWatchdogInit(DrvDoReset, 180);
	BurnSampleInit(0);
	BurnSampleSetAllRoutesAllSamples(0.25, BURN_SND_ROUTE_BOTH);
	BurnSampleSetBuffered(M6809TotalCycles, 1250000);

	// init custom stream/resampler
	stream.init(nBurnSoundRate, nBurnSoundRate, 1, 1, sound_tone_render);
    stream.set_volume(1.00);
	stream.set_buffered(M6809TotalCycles, 1250000);

	BurnTrackballInit(2);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	BurnTrackballExit();

	GenericTilesExit();

	M6809Exit();

	BurnSampleExit();
	stream.exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x800; i++)
	{
		INT32 r = DrvColPROM[i + 0x0000] & 0xf;
		INT32 g = DrvColPROM[i + 0x0800] & 0xf;
		INT32 b = DrvColPROM[i + 0x1000] & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static void draw_layer()
{
	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		INT32 color = palettebank_buffer[y] * 32 + 16;
		UINT8 *src = DrvVidRAM + (y << 7);

		for (INT32 x = 0; x < nScreenWidth; x+=2)
		{
			pTransDraw[(y * nScreenWidth) + x + 0] = (src[x/2] >> 4) + color;
			pTransDraw[(y * nScreenWidth) + x + 1] = (src[x/2] & 0xf) + color;
		}
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x20; i++) {
		UINT8 *sprite = DrvSprRAM + (i << 2);
		INT32 code = sprite[0];
		INT32 sy = sprite[2] + 0x21;
		INT32 sx = sprite[3];

		UINT8 *src = &DrvGfxROM[code << 6];

		for (INT32 y = 0; y < 16; y++, sy = (sy + 1) & 0xff) {
			if (sy >= 16 && sy < nScreenHeight) {
				INT32 color = palettebank_buffer[sy] << 5;
				INT32 cx = sx;

				for (INT32 x = 0; x < 4; x++, src++) {
					if (*src & 0xf0 && cx >= 0 && cx < 256)
						pTransDraw[(sy - 16) * nScreenWidth + cx] = (*src >> 4) + color;
					cx++;

					if (*src & 0x0f && cx >= 0 && cx < 256)
						pTransDraw[(sy - 16) * nScreenWidth + cx] = (*src & 0xf) + color;
					cx++;
				}
			} else {
				src += 4;
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
	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	M6809NewFrame();

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		BurnTrackballConfig(0, AXIS_NORMAL, AXIS_REVERSED);
		BurnTrackballFrame(0, Analog[0]/2, Analog[1]/2, 0x01, 0x3f);
		BurnTrackballUpdate(0);

		BurnTrackballConfig(1, AXIS_NORMAL, AXIS_REVERSED);
		BurnTrackballFrame(1, Analog[2]/2, Analog[3]/2, 0x01, 0x3f);
		BurnTrackballUpdate(1);
	}

	INT32 nInterleave = 264;
	INT32 nCyclesTotal[1] = { 1250000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	M6809Open(0);

	vblank = 1;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if ((i & 0x3f) == 0 && i > 0) {
			M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		if (i == 92) {
			M6809SetIRQLine(1, CPU_IRQSTATUS_HOLD);
		}

		if (i < 256) palettebank_buffer[i] = palettebank;

		vblank = (i >= 16 && i < 256) ? 0 : 1;

		CPU_RUN(0, M6809);
	}

	M6809Close();

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
		stream.render(pBurnSoundOut, nBurnSoundLen);
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

		M6809Scan(nAction);
		BurnSampleScan(nAction, pnMin);

		BurnTrackballScan();
		BurnRandomScan(nAction);

		SCAN_VAR(palettebank);
		SCAN_VAR(palettebank_buffer);
		SCAN_VAR(flipscreen);

		SCAN_VAR(tone_pos);
		SCAN_VAR(tone_step);
		SCAN_VAR(tone_vol);

		SCAN_VAR(tb_last);
		SCAN_VAR(tb_last_dir);
		SCAN_VAR(tb_accu);
		SCAN_VAR(tb_target);
		SCAN_VAR(tb_chunk);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00100;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}

static struct BurnSampleInfo GridleeSampleDesc[] = {
#if !defined (ROM_VERIFY)
	{ "bounce1", SAMPLE_NOLOOP },
	{ "bounce2", SAMPLE_NOLOOP },
#endif
	{ "", 0 }
};

STD_SAMPLE_PICK(Gridlee)
STD_SAMPLE_FN(Gridlee)


// Gridlee

static struct BurnRomInfo gridleeRomDesc[] = {
	{ "gridfnla.bin",	0x1000, 0x1c43539e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "gridfnlb.bin",	0x1000, 0xc48b91b8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gridfnlc.bin",	0x1000, 0x6ad436dd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gridfnld.bin",	0x1000, 0xf7188ddb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "gridfnle.bin",	0x1000, 0xd5330bee, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "gridfnlf.bin",	0x1000, 0x695d16a3, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "gridpix0.bin",	0x1000, 0xe6ea15ae, 2 | BRF_GRA },           //  6 Sprite Data
	{ "gridpix1.bin",	0x1000, 0xd722f459, 2 | BRF_GRA },           //  7
	{ "gridpix2.bin",	0x1000, 0x1e99143c, 2 | BRF_GRA },           //  8
	{ "gridpix3.bin",	0x1000, 0x274342a0, 2 | BRF_GRA },           //  9

	{ "grdrprom.bin",	0x0800, 0xf28f87ed, 3 | BRF_GRA },           // 10 Color Data
	{ "grdgprom.bin",	0x0800, 0x921b0328, 3 | BRF_GRA },           // 11
	{ "grdbprom.bin",	0x0800, 0x04350348, 3 | BRF_GRA },           // 12
};

STD_ROM_PICK(gridlee)
STD_ROM_FN(gridlee)

struct BurnDriver BurnDrvGridlee = {
	"gridlee", NULL, NULL, "gridlee", "1983",
	"Gridlee\0", NULL, "Videa", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, gridleeRomInfo, gridleeRomName, NULL, NULL, GridleeSampleInfo, GridleeSampleName, GridleeInputInfo, GridleeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 240, 4, 3
};

