// FB Alpha Formation Z driver module
// Based on MAME driver by Carlos A. Lozano

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvM6809RAM0;
static UINT8 *DrvM6809RAM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvScrRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *soundlatch;
static UINT8 counter201;
static UINT8 disable_irq;
static UINT8 starx;
static UINT8 stary;
static UINT8 scrolly;
static UINT8 bgcolor;
static UINT8 flipscreen;
static UINT8 characterbank;
static UINT8 stardisable;
static INT32 m_sx, m_sy, m_ox, m_oy;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static HoldCoin<1> hold_coin;

static struct BurnInputInfo FormatzInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvInputs + 3,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvInputs + 4,	"dip"		},
};

STDINPUTINFO(Formatz)

static struct BurnDIPInfo FormatzDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0x6c, NULL					},
	{0x01, 0xff, 0xff, 0x08, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x03, 0x00, "3"					},
	{0x00, 0x01, 0x03, 0x01, "4"					},
	{0x00, 0x01, 0x03, 0x02, "5"					},
	{0x00, 0x01, 0x03, 0x03, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x00, 0x01, 0x0c, 0x0c, "30000"				},
	{0x00, 0x01, 0x0c, 0x08, "40000"				},
	{0x00, 0x01, 0x0c, 0x04, "70000"				},
	{0x00, 0x01, 0x0c, 0x00, "100000"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x00, 0x01, 0x20, 0x00, "Off"					},
	{0x00, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x00, 0x01, 0x40, 0x40, "Upright"				},
	{0x00, 0x01, 0x40, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x01, 0x01, 0x07, 0x07, "5 Coins 1 Credits"	},
	{0x01, 0x01, 0x07, 0x05, "4 Coins 1 Credits"	},
	{0x01, 0x01, 0x07, 0x03, "3 Coins 1 Credits"	},
	{0x01, 0x01, 0x07, 0x01, "2 Coins 1 Credits"	},
	{0x01, 0x01, 0x07, 0x00, "1 Coin  1 Credits"	},
	{0x01, 0x01, 0x07, 0x02, "1 Coin  2 Credits"	},
	{0x01, 0x01, 0x07, 0x04, "1 Coin  3 Credits"	},
	{0x01, 0x01, 0x07, 0x06, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x01, 0x01, 0x18, 0x00, "Easy"					},
	{0x01, 0x01, 0x18, 0x08, "Normal"				},
	{0x01, 0x01, 0x18, 0x10, "Medium"				},
	{0x01, 0x01, 0x18, 0x18, "Hard"					},
};

STDDIPINFO(Formatz)

static UINT8 aeroboto_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x2973:
			DrvM6809RAM0[0x02be] = 0;
			return 0xff;

		case 0x3000:
			return DrvInputs[flipscreen];

		case 0x3001:
			return (DrvInputs[2] & 0x80) | (DrvInputs[3] & 0x7f);

		case 0x3002:
			return DrvInputs[4];

		case 0x3004:
			return (0x031b9fff >> (((counter201++) & 3) * 8)) & 0xff;

		case 0x3800:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return 0xff;
	}

	return 0;
}

static void aeroboto_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff00) == 0x0100)
	{
		if (address == 0x01a2 && data)
		{
			disable_irq = 1;
		}

		DrvM6809RAM0[address] = data;
		return;
	}

	if (address < 0x3000) return; // 0x900, 0x1800, 0x2000

	switch (address)
	{
		case 0x3000:
			flipscreen = data & 0x01;
			characterbank = (data & 0x02) >> 1;
			stardisable = data & 0x04;
		return;

		case 0x3001:
			soundlatch[0] = data;
		return;

		case 0x3002:
			soundlatch[1] = data;
		return;

		case 0x3003:
			scrolly = data;
		return;

		case 0x3004:
			starx = data;
		return;

		case 0x3005:
			stary = data;
		return;

		case 0x3006:
			bgcolor = data * 4;
		return;
	}
}

static void aeroboto_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9000:
		case 0x9001:
		case 0xa000:
		case 0xa001:
			AY8910Write((address / 0x2000) & 1, address & 1, data);
		return;
	}
}

static UINT8 aeroboto_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x9002:
		case 0xa002:
			return AY8910Read((address / 0x2000) & 1);
	}

	return 0;
}

static UINT8 aeroboto_AY8910_0_portA(UINT32)
{
	return soundlatch[0];
}

static UINT8 aeroboto_AY8910_0_portB(UINT32)
{
	return soundlatch[1];
}

static INT32 DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	M6809Close();

	AY8910Reset(0);
	AY8910Reset(1);

	counter201 = 0;
	disable_irq = 0;
	starx = 0;
	stary = 0;
	scrolly = 0;
	bgcolor = 0;
	flipscreen = 0;
	characterbank = 0;
	stardisable = 0;
	m_sx = m_sy = m_ox = m_oy = 0;

	hold_coin.reset();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0	= Next; Next += 0x010000;
	DrvM6809ROM1	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x008000;
	DrvGfxROM1	= Next; Next += 0x002000;
	DrvGfxROM2	= Next; Next += 0x008000;

	DrvColPROM	= Next; Next += 0x000300;

	DrvPalette	= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam		= Next;

	DrvM6809RAM0	= Next; Next += 0x000900;
	DrvM6809RAM1	= Next; Next += 0x001000;
	DrvSprRAM	= Next; Next += 0x000100;
	DrvColRAM	= Next; Next += 0x000100;
	DrvScrRAM	= Next; Next += 0x000100;
	DrvVidRAM	= Next; Next += 0x000800;

	soundlatch	= Next; Next += 0x000002;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void DrvGfxDecode()
{
	static INT32 Plane0[2]  = { 4, 0 };
	static INT32 Plane1[3]  = { 0x2000*8, 0x1000*8, 0 };
	static INT32 XOffs0[8]  = { STEP4(0,1), STEP4((0x2000/2)*8,1) };
	static INT32 XOffs1[16] = { STEP8(0,1) };
	static INT32 YOffs[16]  = { STEP16(0,8) };

	UINT8 *tmp = (UINT8 *)BurnMalloc(0x3000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane0, XOffs0, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x2000);

	for (INT32 i = 0; i < 0x2000; i++) {
		DrvGfxROM1[(i & ~0xff) + ((i << 5) & 0xe0) + ((i >> 3) & 0x1f)] = tmp[i];
	}

	memcpy (tmp, DrvGfxROM2, 0x3000);

	GfxDecode(0x0100, 3,  8, 16, Plane1, XOffs1, YOffs, 0x080, tmp, DrvGfxROM2);

	BurnFree (tmp);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvM6809ROM0 + 0x4000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x8000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0xc000,  2, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0xf000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x0000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2   + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x1000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x2000,  8, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x0000,  9, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0100, 10, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x0200, 11, 1)) return 1;

		DrvGfxDecode();
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM0,			0x0000, 0x00ff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM0 + 0x0100,	0x0100, 0x01ff, MAP_ROM); // handler
	M6809MapMemory(DrvM6809RAM0 + 0x0200,	0x0200, 0x08ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x1000, 0x17ff, MAP_RAM);
	M6809MapMemory(DrvScrRAM,		0x1800, 0x18ff, MAP_RAM);
	M6809MapMemory(DrvColRAM,		0x2000, 0x20ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x2800, 0x28ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0 + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(aeroboto_main_write);
	M6809SetReadHandler(aeroboto_main_read);
	M6809Close();

	M6809Init(1);
	M6809Open(1);
	M6809MapMemory(DrvM6809RAM1,			0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM1 + 0xf000,	0xf000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(aeroboto_sound_write);
	M6809SetReadHandler(aeroboto_sound_read);
	M6809Close();

	AY8910Init(0, 1250000, 0);
	AY8910Init(1,  625000, 1);
	AY8910SetPorts(0, &aeroboto_AY8910_0_portA, &aeroboto_AY8910_0_portB, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(M6809TotalCycles, 1250000 / 2);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_layer(INT32 miny, INT32 maxy, INT32 yscroll)
{
	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		sx -= DrvScrRAM[sy/8];
		if (sx < -7) sx += 256;

		sy -= yscroll;
		if (sy < -7) sy += 512;

		INT32 code  = DrvVidRAM[offs]  + (characterbank * 256);
		INT32 color = DrvColRAM[code&0xff] & 0x3f;

		INT32 category = (color >= 0x33) ? 0 : 0xff;

		if (sy < miny || sy >= maxy) continue;

		if (flipscreen)
		{
			Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, (248 - sx) - 8, (248 - sy) - 16, color, 2, category, 0, DrvGfxROM0);
		}
		else
		{
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy - 16, color, 2, category, 0, DrvGfxROM0);
		}
	}
}

static void draw_stars() // copied directly from MAME
{
#define SCROLL_SPEED	1

	INT32 sky_color = bgcolor, star_color = bgcolor;

	// the star field is supposed to be seen through tile pen 0 when active
	if (!stardisable)
	{
		if (star_color < 0xd0)
		{
			star_color = 0xd0;
			sky_color = 0;
		}

		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
			pTransDraw[i] = sky_color;
		}

		star_color += 2;

		// actual scroll speed is unknown but it can be adjusted by changing the SCROLL_SPEED constant
		m_sx += (char)(starx - m_ox);
		m_ox = starx;
		INT32 x = m_sx / SCROLL_SPEED;

		if (scrolly != 0xff)
			m_sy += (char)(stary - m_oy);
		m_oy = stary;
		INT32 y = m_sy / SCROLL_SPEED;

		UINT8 *src_base = DrvGfxROM1;

		for (INT32 i = 0; i < 248; i++)
		{
			INT32 src_offsx = (x + i) & 0xff;
			INT32 src_colmask = 1 << (src_offsx & 7);
			src_offsx >>= 3;
			UINT8 *src_colptr = src_base + src_offsx;
			INT32 pen = star_color + ((i + 8) >> 4 & 1);

			for (INT32 j = 16; j < 240; j++)
			{
				UINT8 *src_rowptr = src_colptr + (((y + j) & 0xff) << 5 );
				if (!((unsigned)*src_rowptr & src_colmask))
					pTransDraw[((j-16)*nScreenWidth)+i] = pen;
			}
		}
	}
	else
	{
		m_sx = m_ox = starx;
		m_sy = m_oy = stary;

		BurnTransferClear(bgcolor);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		INT32 sx = DrvSprRAM[offs + 3];
		INT32 sy = 240 - DrvSprRAM[offs];

		INT32 code  = DrvSprRAM[offs + 1];
		INT32 color = DrvSprRAM[offs + 2] & 0x07;

		if (flipscreen)
		{
			RenderCustomTile_Mask_FlipXY_Clip(pTransDraw, 8, 16, code, (((248 - sx) + 8) & 0xff) - 8, (240 - sy) - 16, color, 3, 0, 0, DrvGfxROM2);
		}
		else
		{
			RenderCustomTile_Mask_Clip(pTransDraw, 8, 16, code, ((sx + 8) & 0xff) - 8, sy - 16, color, 3, 0, 0, DrvGfxROM2);
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

	if (nBurnLayer & 1) draw_stars();
	if (nBurnLayer & 2) draw_layer(40, 255, scrolly);
	if (nSpriteEnable & 1) draw_sprites();
	if (nBurnLayer & 4) draw_layer(0,  39, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		hold_coin.checklow(0, DrvInputs[2], 1 << 7, 3);
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 1250000 / 60, 1250000 / 2 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		CPU_RUN(0, M6809);
		if (i == (nInterleave - 1))
		{
			if (disable_irq == 0)
			{
				M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
			}
			else
			{
				disable_irq = 0;
			}
		}
		M6809Close();

		M6809Open(1);
		CPU_RUN(1, M6809);
		if (i == (nInterleave - 1)) M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);
		M6809Close();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
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

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(counter201);
		SCAN_VAR(disable_irq);
		SCAN_VAR(starx);
		SCAN_VAR(stary);
		SCAN_VAR(scrolly);
		SCAN_VAR(bgcolor);
		SCAN_VAR(flipscreen);
		SCAN_VAR(characterbank);
		SCAN_VAR(stardisable);
		SCAN_VAR(m_sx);
		SCAN_VAR(m_sy);
		SCAN_VAR(m_ox);
		SCAN_VAR(m_oy);

		hold_coin.scan();
	}

	return 0;
}


// Formation Z

static struct BurnRomInfo formatzRomDesc[] = {
	{ "format_z.8",	0x4000, 0x81a2416c, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "format_z.7",	0x4000, 0x986e6052, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "format_z.6",	0x4000, 0xbaa0d745, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "format_z.9",	0x1000, 0x6b9215ad, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code

	{ "format_z.5",	0x2000, 0xba50be57, 3 | BRF_GRA },           //  4 Background Tiles

	{ "format_z.4",	0x2000, 0x910375a0, 4 | BRF_GRA },           //  5 Star Data

	{ "format_z.1",	0x1000, 0x5739afd2, 5 | BRF_GRA },           //  6 Sprites
	{ "format_z.2",	0x1000, 0x3a821391, 5 | BRF_GRA },           //  7
	{ "format_z.3",	0x1000, 0x7d1aec79, 5 | BRF_GRA },           //  8

	{ "10c",		0x0100, 0xb756dd6d, 6 | BRF_GRA },           //  9 Color PROMs
	{ "10b",		0x0100, 0x00df8809, 6 | BRF_GRA },           // 10
	{ "10a",		0x0100, 0xe8733c8f, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(formatz)
STD_ROM_FN(formatz)

struct BurnDriver BurnDrvFormatz = {
	"formatz", NULL, NULL, NULL, "1984",
	"Formation Z\0", NULL, "Jaleco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, formatzRomInfo, formatzRomName, NULL, NULL, NULL, NULL, FormatzInputInfo, FormatzDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	248, 224, 4, 3
};


// Aeroboto

static struct BurnRomInfo aerobotoRomDesc[] = {
	{ "aeroboto.8",	0x4000, 0x4d3fc049, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "aeroboto.7",	0x4000, 0x522f51c1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "aeroboto.6",	0x4000, 0x1a295ffb, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "format_z.9",	0x1000, 0x6b9215ad, 2 | BRF_PRG | BRF_ESS }, //  3 M6809 #1 Code

	{ "aeroboto.5",	0x2000, 0x32fc00f9, 3 | BRF_GRA },           //  4 Background Tiles

	{ "format_z.4",	0x2000, 0x910375a0, 4 | BRF_GRA },           //  5 Star Data

	{ "aeroboto.1",	0x1000, 0x7820eeaf, 5 | BRF_GRA },           //  6 Sprites
	{ "aeroboto.2",	0x1000, 0xc7f81a3c, 5 | BRF_GRA },           //  7
	{ "aeroboto.3",	0x1000, 0x5203ad04, 5 | BRF_GRA },           //  8

	{ "10c",		0x0100, 0xb756dd6d, 6 | BRF_GRA },           //  9 Color PROMs
	{ "10b",		0x0100, 0x00df8809, 6 | BRF_GRA },           // 10
	{ "10a",		0x0100, 0xe8733c8f, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(aeroboto)
STD_ROM_FN(aeroboto)

struct BurnDriver BurnDrvAeroboto = {
	"aeroboto", "formatz", NULL, NULL, "1984",
	"Aeroboto\0", NULL, "Jaleco (Williams license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, aerobotoRomInfo, aerobotoRomName, NULL, NULL, NULL, NULL, FormatzInputInfo, FormatzDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	248, 224, 4, 3
};
