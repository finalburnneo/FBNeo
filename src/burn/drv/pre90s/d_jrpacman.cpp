// FB Alpha JR. Pacman driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "namco_snd.h"
#include "driver.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvColPROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprRAM2;
static UINT8 *DrvVidRAM;
static UINT32 *DrvPalette;
static UINT32 *Palette;

static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT8 DrvInputs[2];

static INT32 interrupt_enable = 0;
static INT32 palettebank = 0;
static INT32 colortablebank = 0;
static INT32 flipscreen = 0;
static INT32 bgpriority = 0;
static INT32 spritebank = 0;
static INT32 charbank = 0;
static INT32 scrolly = 0;

static struct BurnInputInfo JrpacmanInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip c",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Jrpacman)


static struct BurnDIPInfo JrpacmanDIPList[]=
{
	// Default Values
	{0x0d, 0xff, 0xff, 0x10, NULL			},
	{0x0e, 0xff, 0xff, 0x90, NULL			},
	{0x0f, 0xff, 0xff, 0x69, NULL			},

	{0   , 0xfe, 0   ,    2, "Rack Test (Cheat)"	},
	{0x0d, 0x01, 0x10, 0x10, "Off"			},
	{0x0d, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service"		},
	{0x0e, 0x01, 0x10, 0x10, "Off"		},
	{0x0e, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0e, 0x01, 0x80, 0x80, "Upright"		},
	{0x0e, 0x01, 0x80, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x0f, 0x01, 0x03, 0x03, "2 Coins 1 Credits "	},
	{0x0f, 0x01, 0x03, 0x01, "1 Coin 1 Credits "	},
	{0x0f, 0x01, 0x03, 0x02, "1 Coin 2 Credits "	},
	{0x0f, 0x01, 0x03, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0f, 0x01, 0x0c, 0x00, "1"			},
	{0x0f, 0x01, 0x0c, 0x04, "2"			},
	{0x0f, 0x01, 0x0c, 0x08, "3"			},
	{0x0f, 0x01, 0x0c, 0x0c, "5"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0f, 0x01, 0x30, 0x00, "10000"		},
	{0x0f, 0x01, 0x30, 0x10, "15000"		},
	{0x0f, 0x01, 0x30, 0x20, "20000"		},
	{0x0f, 0x01, 0x30, 0x30, "30000"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0f, 0x01, 0x40, 0x40, "Normal"		},
	{0x0f, 0x01, 0x40, 0x00, "Hard"			},
};

STDDIPINFO(Jrpacman)

void __fastcall jrpacman_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x5000:
			interrupt_enable = data;
		return;

		case 0x5001:
			// pacman_sound_enable_w
		return;

		case 0x5003:
			flipscreen = data & 1;
		return;

		case 0x5070:
			palettebank = data;
		return;

		case 0x5071:
			colortablebank = data;
		return;

		case 0x5073:
			bgpriority = data & 1;
		return;

		case 0x5074:
			charbank = data & 1;
		return;

		case 0x5075:
			spritebank = data & 1;
		return;

		case 0x5080:
			scrolly = data;
		return;

		case 0x50c0:
			// nop
		return;
	}

	if (address >= 0x5040 && address <= 0x505f) {
		NamcoSoundWrite(address & 0x1f, data);
		return;
	}

	if ((address & 0xfff0) == 0x5060) {
		DrvSprRAM2[address & 0x0f] = data;
		return;
	}
}

UINT8 __fastcall jrpacman_read(UINT16 address)
{
	if ((address & 0xff00) == 0x5000) address &= 0xffc0;

	switch (address)
	{
		case 0x5000:
			return (DrvInputs[0] & 0xef) | (DrvDips[0] & 0x10);

		case 0x5040:
			return (DrvInputs[1] & 0x6f) | (DrvDips[1] & 0x90);

		case 0x5080:
			return DrvDips[2];
	}

	return 0;
}

void __fastcall jrpacman_out(UINT16 port, UINT8 data)
{
	if ((port & 0xff) == 0) {
		ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		ZetSetVector(data);
		return;
	}
}

static void DrvGfxDecode()
{
	INT32 Planes[2]  = { 0, 4 };
	INT32 XOffs8x8[8] = { STEP4(8*8,1), STEP4(0*8,1) };
	INT32 YOffs8x8[8] = { STEP8(0*8,8) };
	INT32 XOffs16x16[16] = { STEP4(8*8,1), STEP4(16*8,1), STEP4(24*8,1), STEP4(0*8,1) };
	INT32 YOffs16x16[16] = { STEP8(0*8,8), STEP8(32*8,8) };

	UINT8 *tmp = (UINT8*)malloc( 0x2000 );

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x200, 2,  8,  8, Planes, XOffs8x8, YOffs8x8, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x2000);

	GfxDecode(0x080, 2, 16, 16, Planes, XOffs16x16, YOffs16x16, 0x200, tmp, DrvGfxROM1);

	free (tmp);
}

static void DrvPaletteInit()
{
	UINT32 tmp[32];

	for (INT32 i = 0; i < 32; i++)
	{
		INT32 bit0, bit1, bit2;
		INT32 r, g, b;

		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		r = (bit0 * 33) + (bit1 * 71) + (bit2 * 151);

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		g = (bit0 * 33) + (bit1 * 71) + (bit2 * 151);

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		b = (bit0 * 81) + (bit1 * 171);

		tmp[i] = (r << 16) | (g << 8) | b;
	}

	for (INT32 i = 0; i < 256; i++)
	{
		UINT8 ctabentry = DrvColPROM[0x20 + i] & 0x0f;

		Palette[0x000 | i] = tmp[ctabentry | 0x00];
		Palette[0x100 + i] = tmp[ctabentry + 0x10];
	}
}


static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	NamcoSoundReset();

	interrupt_enable = 0;
	palettebank = 0;
	colortablebank = 0;
	flipscreen = 0;
	bgpriority = 0;
	spritebank = 0;
	charbank = 0;
	scrolly = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x0080000;
	DrvGfxROM1	= Next; Next += 0x0080000;

	DrvColPROM	= Next; Next += 0x000120;

	NamcoSoundProm		= Next; Next += 0x000200;

	DrvPalette	= (UINT32*)Next; Next += 0x0200 * sizeof(int);
	Palette		= (UINT32*)Next; Next += 0x0200 * sizeof(int);

	AllRam		= Next;

	DrvSprRAM2	= Next; Next += 0x000010;
	DrvVidRAM	= Next; Next += 0x000800;
	DrvZ80RAM	= Next; Next += 0x000800;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void jrpacman_decode()
{
	static const struct {
	    INT32 count;
	    INT32 value;
	} table[] =
	{
		{ 0x00C1, 0x00 }, { 0x0002, 0x80 }, { 0x0004, 0x00 }, { 0x0006, 0x80 },
		{ 0x0003, 0x00 }, { 0x0002, 0x80 }, { 0x0009, 0x00 }, { 0x0004, 0x80 },
		{ 0x9968, 0x00 }, { 0x0001, 0x80 }, { 0x0002, 0x00 }, { 0x0001, 0x80 },
		{ 0x0009, 0x00 }, { 0x0002, 0x80 }, { 0x0009, 0x00 }, { 0x0001, 0x80 },
		{ 0x00AF, 0x00 }, { 0x000E, 0x04 }, { 0x0002, 0x00 }, { 0x0004, 0x04 },
		{ 0x001E, 0x00 }, { 0x0001, 0x80 }, { 0x0002, 0x00 }, { 0x0001, 0x80 },
		{ 0x0002, 0x00 }, { 0x0002, 0x80 }, { 0x0009, 0x00 }, { 0x0002, 0x80 },
		{ 0x0009, 0x00 }, { 0x0002, 0x80 }, { 0x0083, 0x00 }, { 0x0001, 0x04 },
		{ 0x0001, 0x01 }, { 0x0001, 0x00 }, { 0x0002, 0x05 }, { 0x0001, 0x00 },
		{ 0x0003, 0x04 }, { 0x0003, 0x01 }, { 0x0002, 0x00 }, { 0x0001, 0x04 },
		{ 0x0003, 0x01 }, { 0x0003, 0x00 }, { 0x0003, 0x04 }, { 0x0001, 0x01 },
		{ 0x002E, 0x00 }, { 0x0078, 0x01 }, { 0x0001, 0x04 }, { 0x0001, 0x05 },
		{ 0x0001, 0x00 }, { 0x0001, 0x01 }, { 0x0001, 0x04 }, { 0x0002, 0x00 },
		{ 0x0001, 0x01 }, { 0x0001, 0x04 }, { 0x0002, 0x00 }, { 0x0001, 0x01 },
		{ 0x0001, 0x04 }, { 0x0002, 0x00 }, { 0x0001, 0x01 }, { 0x0001, 0x04 },
		{ 0x0001, 0x05 }, { 0x0001, 0x00 }, { 0x0001, 0x01 }, { 0x0001, 0x04 },
		{ 0x0002, 0x00 }, { 0x0001, 0x01 }, { 0x0001, 0x04 }, { 0x0002, 0x00 },
		{ 0x0001, 0x01 }, { 0x0001, 0x04 }, { 0x0001, 0x05 }, { 0x0001, 0x00 },
		{ 0x01B0, 0x01 }, { 0x0001, 0x00 }, { 0x0002, 0x01 }, { 0x00AD, 0x00 },
		{ 0x0031, 0x01 }, { 0x005C, 0x00 }, { 0x0005, 0x01 }, { 0x604E, 0x00 },
	        { 0,      0    }
	};

	for (INT32 i = 0, A = 0; table[i].count; i++)
		for (INT32 j = 0; j < table[i].count; j++)
			DrvZ80ROM[A++] ^= table[i].value;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM + 0x0000,	0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x2000,	1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x8000,	2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0xa000,	3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0xc000,	4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,	5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0000,	6, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x000,	7, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x020,	8, 1)) return 1;
		for (INT32 i = 0; i < 0x20; i++) {
			DrvColPROM[i] = (DrvColPROM[i] & 0x0f) | (DrvColPROM[i + 0x020] << 4);
		}
		if (BurnLoadRom(DrvColPROM + 0x020,	9, 1)) return 1;

		if (BurnLoadRom(NamcoSoundProm + 0x0000,    10, 1)) return 1;
		if (BurnLoadRom(NamcoSoundProm + 0x0100,    11, 1)) return 1;

		DrvGfxDecode();
		jrpacman_decode();
		DrvPaletteInit();
	}

	DrvSprRAM = DrvZ80RAM + 0x7f0;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		    0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,		    0x4000, 0x47ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		    0x4800, 0x4fff, MAP_RAM); //+ 0x7f0 = sprite ram
	ZetMapMemory(DrvZ80ROM + 0x8000,0x8000, 0xdfff, MAP_ROM);
	ZetSetWriteHandler(jrpacman_write);
	ZetSetReadHandler(jrpacman_read);
	ZetSetOutHandler(jrpacman_out);
	ZetClose();

	NamcoSoundInit(18432000 / 6 / 32, 3, 0);
	NacmoSoundSetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	NamcoSoundExit();

	free (AllMem);
	AllMem = NULL;

	return 0;
}

static void draw_bg_layer()
{
	for (INT32 mx = 0; mx < 36+20; mx++) {
		for (INT32 my = 0; my < 54+20; my++) {

			INT32 offs;

			INT32 row = mx + 2;
			INT32 col = my - 2;
			if ((col & 0x20) && (row & 0x20))
				offs = 0;
			else if (col & 0x20)
				offs = row + (((col&0x3) | 0x38)<< 5);
			else
				offs = col + (row << 5);

			INT32 y = 8 * mx;
			INT32 x = 8 * my;

			if (x > 8 && x < 272) {
				y -= scrolly;
			}

			INT32 idx = (offs < 1792) ? (offs & 0x1f) : (offs + 0x80);

			INT32 code  = DrvVidRAM[offs] | (charbank << 8);
			INT32 color = (DrvVidRAM[idx] & 0x1f) | (colortablebank << 5) | (palettebank << 6);

			Render8x8Tile_Mask_Clip(pTransDraw, code, x, y, color, 2, 0, 0, DrvGfxROM0);
		}
	}
}

static void RenderTileCPMP(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height)
{
	UINT16 *dest = pTransDraw;
	UINT8 *gfx = DrvGfxROM1;

	INT32 flip = 0;
	if (flipy) flip |= (height - 1) * width;
	if (flipx) flip |= width - 1;

	gfx += code * width * height;

	for (INT32 y = 0; y < height; y++, sy++) {
		if (sy < 0 || sy >= nScreenHeight) continue;

		for (INT32 x = 0; x < width; x++, sx++) {
			if (sx < 0 || sx >= nScreenWidth) continue;

			INT32 pxl = gfx[((y * width) + x) ^ flip];

			if (DrvPalette[pxl | (color << 2)] == 0) continue;

			dest[sy * nScreenWidth + sx] = pxl | (color << 2);
		}
		sx -= width;
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x10 - 2; offs >= 0; offs -= 2)
	{
		INT32 code   = (DrvSprRAM[offs] >> 2) | (spritebank << 6);
		INT32 color  = (DrvSprRAM[offs + 1] & 0x1f ) | (colortablebank << 5) | (palettebank << 6);
		INT32 sx     = DrvSprRAM2[offs + 1];
		INT32 sy     = DrvSprRAM2[offs];
		INT32 flipx  = DrvSprRAM [offs] & 1;
		INT32 flipy  = DrvSprRAM [offs] & 2;

		if (flipscreen) {
			sy = (240 - sy) - 8;
			sx += 8;
			flipy = !flipy;
			flipx = !flipx;
		} else {
			sx = 272 - sx;
			sy = sy - 31;
		}

		if (offs <= 2*2) sy += 1; // first couple sprites are offset

		RenderTileCPMP(code, color, sx, sy, flipx, flipy, 16, 16);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x200; i++) {
			INT32 rgb = Palette[i];
			DrvPalette[i] = BurnHighCol(rgb >> 16, rgb >> 8, rgb, 0);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (bgpriority == 0)
		if (nBurnLayer & 1) draw_bg_layer();

	if (nBurnLayer & 2) draw_sprites();

	if (bgpriority != 0)
		if (nBurnLayer & 1) draw_bg_layer();

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

		ProcessJoystick(&DrvInputs[0], 0, 0,3,1,2, INPUT_4WAY | INPUT_CLEAROPPOSITES | INPUT_MAKEACTIVELOW );
		ProcessJoystick(&DrvInputs[1], 1, 0,3,1,2, INPUT_4WAY | INPUT_CLEAROPPOSITES | INPUT_MAKEACTIVELOW );
	}

	INT32 nInterleave = 264;
	INT32 nSoundBufferPos = 0;
	
	INT32 nCyclesTotal = (18432000 / 6) / 60;
	INT32 nCyclesDone = 0, nSegment = 0;

	ZetOpen(0);
	for (INT32 i = 0; i < nInterleave; i++) {
		nSegment = (nCyclesTotal - nCyclesDone) / (nInterleave - i);
		nCyclesDone += ZetRun(nSegment);

		if (i == 223 && interrupt_enable) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			
			if (nSegmentLength) {
				NamcoSoundUpdate(pSoundBuf, nSegmentLength);
			}
			nSoundBufferPos += nSegmentLength;
		}
	}
	ZetClose();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			NamcoSoundUpdate(pSoundBuf, nSegmentLength);
		}
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
		*pnMin = 0x029693;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		NamcoSoundScan(nAction, pnMin);

		SCAN_VAR(interrupt_enable);
		SCAN_VAR(palettebank);
		SCAN_VAR(colortablebank);
		SCAN_VAR(flipscreen);
		SCAN_VAR(bgpriority);
		SCAN_VAR(spritebank);
		SCAN_VAR(charbank);
		SCAN_VAR(scrolly);
	}

	return 0;
}

// Jr. Pac-Man (11/9/83)

static struct BurnRomInfo jrpacmanRomDesc[] = {
	{ "jrp8d.8d",		0x2000, 0xe3fa972e, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "jrp8e.8e",		0x2000, 0xec889e94, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jrp8h.8h",		0x2000, 0x35f1fc6e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jrp8j.8j",		0x2000, 0x9737099e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "jrp8k.8k",		0x2000, 0x5252dd97, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "jrp2c.2c",		0x2000, 0x0527ff9b, 2 | BRF_GRA },           //  5 gfx1
	{ "jrp2e.2e",		0x2000, 0x73477193, 2 | BRF_GRA },           //  6

	{ "a290-27axv-bxhd.9e",	0x0100, 0x029d35c4, 3 | BRF_GRA },           //  7 proms
	{ "a290-27axv-cxhd.9f",	0x0100, 0xeee34a79, 3 | BRF_GRA },           //  8
	{ "a290-27axv-axhd.9p",	0x0100, 0x9f6ea9d8, 3 | BRF_GRA },           //  9

	{ "a290-27axv-dxhd.7p",	0x0100, 0xa9cc86bf, 4 | BRF_GRA },           // 10 namco
	{ "a290-27axv-exhd.5s",	0x0100, 0x77245b66, 4 | BRF_GRA },           // 11
};

STD_ROM_PICK(jrpacman)
STD_ROM_FN(jrpacman)

struct BurnDriver BurnDrvJrpacman = {
	"jrpacman", NULL, NULL, NULL, "1983",
	"Jr. Pac-Man (11/9/83)\0", NULL, "Bally Midway", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, jrpacmanRomInfo, jrpacmanRomName, NULL, NULL, JrpacmanInputInfo, JrpacmanDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


// Jr. Pac-Man (speedup hack)

static struct BurnRomInfo jrpacmanfRomDesc[] = {
	{ "fast_jr.8d",		0x2000, 0x461e8b57, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "jrp8e.8e",		0x2000, 0xec889e94, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jrp8h.8h",		0x2000, 0x35f1fc6e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jrp8j.8j",		0x2000, 0x9737099e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "jrp8k.8k",		0x2000, 0x5252dd97, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "jrp2c.2c",		0x2000, 0x0527ff9b, 2 | BRF_GRA },           //  5 gfx1
	{ "jrp2e.2e",		0x2000, 0x73477193, 2 | BRF_GRA },           //  6

	{ "a290-27axv-bxhd.9e",	0x0100, 0x029d35c4, 3 | BRF_GRA },           //  7 proms
	{ "a290-27axv-cxhd.9f",	0x0100, 0xeee34a79, 3 | BRF_GRA },           //  8
	{ "a290-27axv-axhd.9p",	0x0100, 0x9f6ea9d8, 3 | BRF_GRA },           //  9

	{ "a290-27axv-dxhd.7p",	0x0100, 0xa9cc86bf, 4 | BRF_GRA },           // 10 namco
	{ "a290-27axv-exhd.5s",	0x0100, 0x77245b66, 4 | BRF_GRA },           // 11
};

STD_ROM_PICK(jrpacmanf)
STD_ROM_FN(jrpacmanf)

struct BurnDriver BurnDrvJrpacmanf = {
	"jrpacmanf", "jrpacman", NULL, NULL, "1983",
	"Jr. Pac-Man (speedup hack)\0", NULL, "hack", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, jrpacmanfRomInfo, jrpacmanfRomName, NULL, NULL, JrpacmanInputInfo, JrpacmanDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 288, 3, 4
};


