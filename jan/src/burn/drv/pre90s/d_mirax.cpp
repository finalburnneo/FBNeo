// Mirax emu-layer for FB Alpha by dink, based on the MAME driver by Angelo Salese, Tomasz Slanina, Olivier Galibert.

#include "tiles_generic.h"
#include "driver.h"
#include "z80_intf.h"
#include "bitswap.h"

extern "C" {
	#include "ay8910.h"
}
static INT16 *pAY8910Buffer[6];

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColorRAM;
static UINT8 *DrvSpriteRAM;
static UINT8 *DrvColorPROM;
static UINT8 *DrvCharGFX;
static UINT8 *DrvSpriteGFX;

static UINT8 *nAyCtrl;
static UINT8 *nmi_mask;
static UINT8 *flipscreen_x;
static UINT8 *flipscreen_y;
static UINT8 *soundlatch;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDip[2] = { 0, 0 };
static UINT8 DrvInput[5];
static UINT8 DrvReset;

static struct BurnInputInfo MiraxInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"   },
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"  },
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"     },
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 down"   },
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"   },
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"  },
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1" },

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"   },
	{"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"  },
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 up"     },
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 down"   },
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"   },
	{"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"  },
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"     },
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	    "dip"       },
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	    "dip"       },
};

STDINPUTINFO(Mirax)


static struct BurnDIPInfo MiraxDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x00, NULL		            },
	{0x10, 0xff, 0xff, 0x0c, NULL		            },

	{0   , 0xfe, 0   ,    4, "Coinage"		        },
	{0x0f, 0x01, 0x03, 0x03, "2 Coins 1 Credit"	},
	{0x0f, 0x01, 0x03, 0x00, "1 Coin  1 Credit"	},
	{0x0f, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0x03, 0x02, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		        },
	{0x0f, 0x01, 0x04, 0x00, "Upright"		        },
	{0x0f, 0x01, 0x04, 0x04, "Cocktail"		        },

	{0   , 0xfe, 0   ,    2, "Flip Screen"		    },
	{0x0f, 0x01, 0x08, 0x00, "Off"		            },
	{0x0f, 0x01, 0x08, 0x08, "On"		            },

	{0   , 0xfe, 0   ,    4, "Lives"		        },
	{0x0f, 0x01, 0x30, 0x30, "2"		            },
	{0x0f, 0x01, 0x30, 0x00, "3"		            },
	{0x0f, 0x01, 0x30, 0x10, "4"		            },
	{0x0f, 0x01, 0x30, 0x20, "5"		            },

	{0   , 0xfe, 0   ,    2, "Bonus Life"		    },
	{0x10, 0x01, 0x01, 0x00, "30k 80k 150k"		    },
	{0x10, 0x01, 0x01, 0x01, "900k 950k 990k"		},

	{0   , 0xfe, 0   ,    2, "Flags for Extra Life"	},
	{0x10, 0x01, 0x02, 0x00, "5"		            },
	{0x10, 0x01, 0x02, 0x02, "8"		            },

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		    },
	{0x10, 0x01, 0x04, 0x00, "Off"		            },
	{0x10, 0x01, 0x04, 0x04, "On"		            },

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x10, 0x01, 0x08, 0x00, "No"		            },
	{0x10, 0x01, 0x08, 0x08, "Yes"		            },

	{0   , 0xfe, 0   ,    2, "AutoPlay Mode (Debug)"},
	{0x10, 0x01, 0x10, 0x00, "No"		            },
	{0x10, 0x01, 0x10, 0x10, "Yes"		            },

	{0   , 0xfe, 0   ,    2, "Difficulty"		    },
	{0x10, 0x01, 0x20, 0x00, "Easy"		            },
	{0x10, 0x01, 0x20, 0x20, "Hard"		            },
};

STDDIPINFO(Mirax)

static void mirax_palette()
{
	for (INT32 i = 0; i < 0x40; i++) {
		INT32 bit0, bit1, bit2, r, g, b;

		/* red component */
		bit0 = (DrvColorPROM[i] >> 0) & 0x01;
		bit1 = (DrvColorPROM[i] >> 1) & 0x01;
		bit2 = (DrvColorPROM[i] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = (DrvColorPROM[i] >> 3) & 0x01;
		bit1 = (DrvColorPROM[i] >> 4) & 0x01;
		bit2 = (DrvColorPROM[i] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = (DrvColorPROM[i] >> 6) & 0x01;
		bit1 = (DrvColorPROM[i] >> 7) & 0x01;
		b = 0x4f * bit0 + 0xa8 * bit1;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);;
	}
}

static void __fastcall main_write(UINT16 address, UINT8 data)
{
	if (address >= 0xea00 && address <= 0xea3f) {
		DrvColorRAM[address - 0xea00] = data;
		return;
	}

	switch (address)
	{
		case 0xF501: *nmi_mask = data & 1; return;
		case 0xF506: *flipscreen_x = data & 0x01; return;
		case 0xF507: *flipscreen_y = data & 0x01; return;
		case 0xF800: {
			*soundlatch = data & 0xff;
			ZetClose();
			ZetOpen(1);
			ZetNmi();
			ZetClose();
			ZetOpen(0);
			return;
		}
	}
}

static UINT8 __fastcall main_read(UINT16 address)
{
	if (address >= 0xea00 && address <= 0xea3f)
		return DrvColorRAM[address - 0xea00];

	switch (address)
	{
		case 0xF000: return DrvInput[0];
		case 0xF100: return DrvInput[1];
		case 0xF200: return DrvDip[0];
		case 0xF400: return DrvDip[1];
	}

	return 0;
}

static void __fastcall audio_write(UINT16 address, UINT8 data)
{
	if (address >= 0xf900 && address <= 0xf9ff) {
		*nAyCtrl = address - 0xf900;
		return;
	}

	switch (address)
	{
		case 0xE003: {
			AY8910Write(0, 0, *nAyCtrl);
			AY8910Write(0, 1, data);
			return;
		}
		case 0xE403: {
			AY8910Write(1, 0, *nAyCtrl);
			AY8910Write(1, 1, data);
			return;
		}
	}
}

static UINT8 __fastcall audio_read(UINT16 address)
{
	switch (address)
	{
		case 0xA000: return *soundlatch;
	}

	return 0;
}


static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);
	*nAyCtrl = 0x00;
	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x10000;
	DrvZ80ROM1		= Next; Next += 0x10000;

	DrvPalette		= (UINT32*)Next; Next += 0x040 * sizeof(UINT32);
	DrvCharGFX      = Next; Next += 0x40000;
	DrvSpriteGFX    = Next; Next += 0x40000;
	DrvColorPROM    = Next; Next += 0x00400;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x01000;
	DrvZ80RAM1		= Next; Next += 0x01000;
	DrvVidRAM		= Next; Next += 0x00400;
	DrvColorRAM		= Next; Next += 0x00400;
	DrvSpriteRAM	= Next; Next += 0x00300;

	nAyCtrl         = Next; Next += 0x00001;
	nmi_mask        = Next; Next += 0x00001;
	flipscreen_x    = Next; Next += 0x00001;
	flipscreen_y    = Next; Next += 0x00001;
	soundlatch      = Next; Next += 0x00001;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	INT32 c8PlaneOffsets[3] = { RGN_FRAC(0xc000, 2, 3), RGN_FRAC(0xc000, 1, 3), RGN_FRAC(0xc000, 0, 3) };
	INT32 c8XOffsets[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	INT32 c8YOffsets[8] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 };

	INT32 c16PlaneOffsets[3] = { RGN_FRAC(0x18000, 2, 3), RGN_FRAC(0x18000, 1, 3), RGN_FRAC(0x18000, 0, 3) };
	INT32 c16XOffsets[16] = { 0, 1, 2, 3, 4, 5, 6, 7 , 0+8*8, 1+8*8, 2+8*8, 3+8*8, 4+8*8, 5+8*8, 6+8*8, 7+8*8 };
	INT32 c16YOffsets[16] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 0*8+8*8*2, 1*8+8*8*2, 2*8+8*8*2, 3*8+8*8*2, 4*8+8*8*2, 5*8+8*8*2, 6*8+8*8*2, 7*8+8*8*2 };


	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{   // Load ROMS parse GFX
		UINT8 *DrvTempRom = (UINT8 *)BurnMalloc(0x40000);
		memset(DrvTempRom, 0, 0x40000);

		{
			if (BurnLoadRom(DrvTempRom + 0x0000, 0, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x4000, 1, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x8000, 2, 1)) return 1;

			/* decrypt the program ROMs */
			UINT8 *DATA = DrvTempRom;
			UINT8 *ROM = DrvZ80ROM;

			for(INT32 i=0x0000;i<0x4000;i++)
				ROM[BITSWAP16(i, 15,14,13,12,11,10,9, 5,7,6,8, 4,3,2,1,0)] = (BITSWAP08(DATA[i], 1, 3, 7, 0, 5, 6, 4, 2) ^ 0xff);

			for(INT32 i=0x4000;i<0x8000;i++)
				ROM[BITSWAP16(i, 15,14,13,12,11,10,9, 5,7,6,8, 4,3,2,1,0)] = (BITSWAP08(DATA[i], 2, 1, 0, 6, 7, 5, 3, 4) ^ 0xff);

			for(INT32 i=0x8000;i<0xc000;i++)
				ROM[BITSWAP16(i, 15,14,13,12,11,10,9, 5,7,6,8, 4,3,2,1,0)] = (BITSWAP08(DATA[i], 1, 3, 7, 0, 5, 6, 4, 2) ^ 0xff);

			if (BurnLoadRom(DrvZ80ROM1 + 0x0000, 3, 1)) return 1;

			// load & decode 8x8 tiles
			memset(DrvTempRom, 0, 0x40000);
			if (BurnLoadRom(DrvTempRom         , 4, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x4000, 5, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x8000, 6, 1)) return 1;
			GfxDecode(0x800, 3, 8, 8, c8PlaneOffsets, c8XOffsets, c8YOffsets, 0x40, DrvTempRom, DrvCharGFX);

			// load & decode 16x16 tiles
			memset(DrvTempRom, 0, 0x40000);
			if (BurnLoadRom(DrvTempRom + 0x04000, 7, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x0c000, 8, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x14000, 9, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x00000,10, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x08000,11, 1)) return 1;
			if (BurnLoadRom(DrvTempRom + 0x10000,12, 1)) return 1;
			GfxDecode(0x400 /*((0x18000*8)/3)/(16*16))*/, 3, 16, 16, c16PlaneOffsets, c16XOffsets, c16YOffsets, 0x100, DrvTempRom, DrvSpriteGFX);

			if (BurnLoadRom(DrvColorPROM + 0x0000, 13, 1)) return 1;
			if (BurnLoadRom(DrvColorPROM + 0x0020, 14, 1)) return 1;
		}
		BurnFree(DrvTempRom);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xc800, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,		0xe000, 0xe3ff, MAP_RAM);
	ZetMapMemory(DrvSpriteRAM,	0xe800, 0xe9ff, MAP_RAM);
	ZetSetWriteHandler(main_write);
	ZetSetReadHandler(main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x8000, 0x8fff, MAP_RAM);
	ZetSetWriteHandler(audio_write);
	ZetSetReadHandler(audio_read);
	ZetClose();

	AY8910Init(0, 3000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
	AY8910Init(1, 3000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}


static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	return 0;
}

void draw_tiles(UINT8 draw_flag)
{
	for (INT32 y = 0; y < 32; y++) {
		for (INT32 x = 0; x < 32; x++) {
			INT32 color = DrvColorRAM[(x*2)+1];
			INT32 code = DrvVidRAM[32*y+x] | ((color & 0xe0)<<3);

			INT32 sx = (x * 8);
			if (sx < -7) sx += 256;
			INT32 sy = (y * 8) - (DrvColorRAM[x*2] + 8); // + 8 offset for smoothe scroll. -dink
			if (sy < -7) sy += 256;

			if (*flipscreen_x) sx = 248 - sx;
			if (*flipscreen_y) sy = 248 - sy;

			if ((x <= 1 || x >= 30) ^ draw_flag) {
				if (*flipscreen_y) { // we need a macro for this -dink
					if (*flipscreen_x) {
						Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color & 7, 3, 0, 0x00, DrvCharGFX);
					} else {
						Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color & 7, 3, 0, 0x00, DrvCharGFX);
					}
				} else {
					if (*flipscreen_x) {
						Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color & 7, 3, 0, 0x00, DrvCharGFX);
					} else {
						Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color & 7, 3, 0, 0x00, DrvCharGFX);
					}
				}
			}
		}
	}
}

static void RenderTileCPMP(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, INT32 width, INT32 height, INT32 offset, INT32 /*mode*/, UINT8 *gfxrom)
{
	UINT16 *dest = pTransDraw;
	UINT8 *gfx = gfxrom;

	INT32 flip = 0;
	if (flipy) flip |= (height - 1) * width;
	if (flipx) flip |= width - 1;

	gfx += code * width * height;

	for (INT32 y = 0; y < height; y++, sy++) {
		if (sy < 0 || sy >= nScreenHeight) continue;

		for (INT32 x = 0; x < width; x++, sx++) {
			if (sx < 16 || sx + 16 >= nScreenWidth) continue; // blank out the top and bottom 16 pixels for status

			INT32 pxl = gfx[((y * width) + x) ^ flip];

			if (!pxl) continue; // transparency

			dest[sy * nScreenWidth + sx] = pxl | (color << 3) | offset;
		}
		sx -= width;
	}
}

static void draw_sprites()
{
	for(INT32 count = 0; count < 0x200; count += 4) {
		INT32 spr_offs, x, y, color, fx, fy;

		if(DrvSpriteRAM[count] == 0x00 || DrvSpriteRAM[count+3] == 0x00)
			continue;

		spr_offs = (DrvSpriteRAM[count+1] & 0x3f);
		color = DrvSpriteRAM[count+2] & 0x7;
		fx = (*flipscreen_x) ^ ((DrvSpriteRAM[count+1] & 0x40) >> 6); //<- guess
		fy = (*flipscreen_y) ^ ((DrvSpriteRAM[count+1] & 0x80) >> 7);

		spr_offs += (DrvSpriteRAM[count+2] & 0xe0)<<1;
		spr_offs += (DrvSpriteRAM[count+2] & 0x10)<<5;

		y = (*flipscreen_y) ? DrvSpriteRAM[count] : 0x100 - DrvSpriteRAM[count] - 16;
		x = (*flipscreen_x) ? 240 - DrvSpriteRAM[count+3] : DrvSpriteRAM[count+3];
		y -= 8;

		RenderTileCPMP(spr_offs, color, x, y, fx, fy, 16, 16, 0x000, 0, DrvSpriteGFX);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		mirax_palette();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 2) draw_tiles(1);
	if (nBurnLayer & 4) draw_sprites();
	if (nBurnLayer & 8) draw_tiles(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void DrvMakeInputs()
{
	// Reset Inputs (all active HIGH)
	DrvInput[0] = 0;
	DrvInput[1] = 0;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << i;
		DrvInput[1] |= (DrvJoy2[i] & 1) << i;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	DrvMakeInputs();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = 3000000 / 60;

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		ZetRun(nCyclesTotal / nInterleave);

		if (i == 248 && *nmi_mask)
			ZetNmi();

		ZetClose();

		ZetOpen(1);
		ZetRun(nCyclesTotal / nInterleave);

		if (i % (nInterleave / 4) == (nInterleave / 4) - 1)
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);

		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
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
		*pnMin = 0x029735;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		AY8910Scan(nAction, pnMin);
	}

	return 0;
}


// Mirax (set 1)

static struct BurnRomInfo miraxRomDesc[] = {
	{ "mxp5-42.rom",	0x4000, 0x716410a0, 1 | BRF_PRG | BRF_ESS }, //  0 main cpu
	{ "mxr5-4v.rom",	0x4000, 0xc9484fc3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mxs5-4v.rom",	0x4000, 0xe0085f91, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mxr2-4v.rom",	0x2000, 0xcd2d52dc, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "mxe3-4v.rom",	0x4000, 0x0cede01f, 3 | BRF_GRA }, //  4 gfx1
	{ "mxh3-4v.rom",	0x4000, 0x58221502, 3 | BRF_GRA }, //  5
	{ "mxk3-4v.rom",	0x4000, 0x6dbc2961, 3 | BRF_GRA }, //  6

	{ "mxe2-4v.rom",	0x4000, 0x2cf5d8b7, 4 | BRF_GRA }, //  7 gfx2
	{ "mxf2-4v.rom",	0x4000, 0x1f42c7fa, 4 | BRF_GRA }, //  8
	{ "mxh2-4v.rom",	0x4000, 0xcbaff4c6, 4 | BRF_GRA }, //  9
	{ "mxf3-4v.rom",	0x4000, 0x14b1ca85, 4 | BRF_GRA }, // 10
	{ "mxi3-4v.rom",	0x4000, 0x20fb2099, 4 | BRF_GRA }, // 11
	{ "mxl3-4v.rom",	0x4000, 0x918487aa, 4 | BRF_GRA }, // 12

	{ "mra3.prm",		0x0020, 0xae7e1a63, 5 | BRF_GRA }, // 13 proms
	{ "mrb3.prm",		0x0020, 0xe3f3d0f5, 5 | BRF_GRA }, // 14
	{ "mirax.prm",		0x0020, 0x00000000, 5 | BRF_NODUMP }, // 15
};

STD_ROM_PICK(mirax)
STD_ROM_FN(mirax)

struct BurnDriver BurnDrvMirax = {
	"mirax", NULL, NULL, NULL, "1985",
	"Mirax (set 1)\0", NULL, "Current Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, miraxRomInfo, miraxRomName, NULL, NULL, MiraxInputInfo, MiraxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};


// Mirax (set 2)

static struct BurnRomInfo miraxaRomDesc[] = {
	{ "mx_p5_43v.p5",	0x4000, 0x87664903, 1 | BRF_PRG | BRF_ESS }, //  0 main cpu
	{ "mx_r5_43v.r5",	0x4000, 0x1ba4cd8e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mx_s5_43v.s5",	0x4000, 0xc58cc151, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mxr2-4v.rom",	0x2000, 0xcd2d52dc, 2 | BRF_PRG | BRF_ESS }, //  3 audiocpu

	{ "mxe3-4v.rom",	0x4000, 0x0cede01f, 3 | BRF_GRA }, //  4 gfx1
	{ "mxh3-4v.rom",	0x4000, 0x58221502, 3 | BRF_GRA }, //  5
	{ "mxk3-4v.rom",	0x4000, 0x6dbc2961, 3 | BRF_GRA }, //  6

	{ "mxe2-4v.rom",	0x4000, 0x2cf5d8b7, 4 | BRF_GRA }, //  7 gfx2
	{ "mxf2-4v.rom",	0x4000, 0x1f42c7fa, 4 | BRF_GRA }, //  8
	{ "mxh2-4v.rom",	0x4000, 0xcbaff4c6, 4 | BRF_GRA }, //  9
	{ "mxf3-4v.rom",	0x4000, 0x14b1ca85, 4 | BRF_GRA }, // 10
	{ "mxi3-4v.rom",	0x4000, 0x20fb2099, 4 | BRF_GRA }, // 11
	{ "mxl3-4v.rom",	0x4000, 0x918487aa, 4 | BRF_GRA }, // 12

	{ "mra3.prm",		0x0020, 0xae7e1a63, 5 | BRF_GRA }, // 13 proms
	{ "mrb3.prm",		0x0020, 0xe3f3d0f5, 5 | BRF_GRA }, // 14
};

STD_ROM_PICK(miraxa)
STD_ROM_FN(miraxa)

struct BurnDriver BurnDrvMiraxa = {
	"miraxa", "mirax", NULL, NULL, "1985",
	"Mirax (set 2)\0", NULL, "Current Technologies", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, miraxaRomInfo, miraxaRomName, NULL, NULL, MiraxInputInfo, MiraxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	240, 256, 3, 4
};

