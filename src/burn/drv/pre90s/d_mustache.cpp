// FB Alpha Mustache Boy driver module
// Based on MAME drver by Tomasz Slanina

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "t5182.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvZ80ROMDec;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 scroll = 0;
static UINT8 video_control = 0;
static UINT8 flipscreen = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[2];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static struct BurnInputInfo MustacheInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 start"}	,
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Mustache)

static struct BurnDIPInfo MustacheDIPList[]=
{
	{0x11, 0xff, 0xff, 0x1d, NULL			},
	{0x12, 0xff, 0xff, 0x7f, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x01, 0x01, "Upright"		},
	{0x11, 0x01, 0x01, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x06, 0x06, "Easy"			},
	{0x11, 0x01, 0x06, 0x04, "Normal"		},
	{0x11, 0x01, 0x06, 0x02, "Hard"			},
	{0x11, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x11, 0x01, 0x18, 0x10, "1"			},
	{0x11, 0x01, 0x18, 0x18, "3"			},
	{0x11, 0x01, 0x18, 0x08, "4"			},
	{0x11, 0x01, 0x18, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x20, 0x20, "Off"			},
	{0x11, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x12, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x04, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x07, 0x03, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x07, 0x01, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x18, 0x10, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x18, 0x18, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x18, 0x00, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x18, 0x08, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x20, 0x20, "Off"			},
	{0x12, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x12, 0x01, 0x40, 0x40, "Off"			},
	{0x12, 0x01, 0x40, 0x00, "On"			},
};

STDDIPINFO(Mustache)

static UINT8 __fastcall mustache_main_read(UINT16 a)
{
	switch (a)
	{
		case 0xd001:
			return t5182_semaphore_snd;

		case 0xd800:
			return DrvInputs[0];

		case 0xd801:
			return DrvInputs[1];

		case 0xd802:
			return DrvInputs[2];

		case 0xd803:
			return DrvDips[0];

		case 0xd804:
			return DrvDips[1];
	}

	return 0;
}

static void __fastcall mustache_main_write(UINT16 a, UINT8 d)
{
	switch (a)
	{
		case 0xd000:
			ZetClose();
			ZetOpen(1);
			t5182_setirq_callback(CPU_ASSERT);
			ZetClose();
			ZetOpen(0);
		return;

		case 0xd002:
		case 0xd003:
			t5182_semaphore_main = ~a & 1;
		return;

		case 0xd806:
			scroll = d;
		return;

		case 0xd807:
			flipscreen = d & 0x01;
			video_control = d;
		return;
	}
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM	= Next; Next += 0x010000;
	DrvZ80ROMDec	= Next; Next += 0x008000;

	t5182ROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x020000;
	DrvGfxROM1	= Next; Next += 0x040000;

	DrvColPROM	= Next; Next += 0x000300;

	DrvPalette	= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM	= Next; Next += 0x001000;
	DrvVidRAM	= Next; Next += 0x001000;
	DrvSprRAM	= Next; Next += 0x000800;

	t5182SharedRAM	= Next; Next += 0x000100;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	t5182Reset();

	scroll = 0;
	video_control = 0;
	flipscreen = 0;

	HiscoreReset();

	return 0;
}

static UINT8 decrypt_byte(INT32 op, INT32 a, INT32 src)
{
	if ( BIT(a,9)  &  BIT(a,8))             src ^= 0x80;
	if ( BIT(a,11) &  BIT(a,4) &  BIT(a,1)) src ^= 0x40;

	if (op) // opcode only
	{
		if (~BIT(a,13) & BIT(a,12))             src ^= 0x20;
		if (~BIT(a,6)  &  BIT(a,1))             src ^= 0x10;
		if (~BIT(a,12) &  BIT(a,2))             src ^= 0x08;
	}

	if ( BIT(a,11) & ~BIT(a,8) &  BIT(a,1)) src ^= 0x04;
	if ( BIT(a,13) & ~BIT(a,6) &  BIT(a,4)) src ^= 0x02;
	if (~BIT(a,11) &  BIT(a,9) &  BIT(a,2)) src ^= 0x01;

	if (BIT(a,13) &  BIT(a,4)) src = BITSWAP08(src,7,6,5,4,3,2,0,1);
	if (BIT(a, 8) &  BIT(a,4)) src = BITSWAP08(src,7,6,5,4,2,3,1,0);

	if (op) // opcode only
	{
		if (BIT(a,12) &  BIT(a,9)) src = BITSWAP08(src,7,6,4,5,3,2,1,0);
		if (BIT(a,11) & ~BIT(a,6)) src = BITSWAP08(src,6,7,5,4,3,2,1,0);
	}

	return src;
}

static void seibu_decrypt()
{
	for (INT32 i = 0; i < 0x8000; i++)
	{
		DrvZ80ROMDec[i] = decrypt_byte(1,i,DrvZ80ROM[i]);
		DrvZ80ROM[i]    = decrypt_byte(0,i,DrvZ80ROM[i]);
	}
}

static void decode_gfx()
{
	INT32 i;
	INT32 G1 = 0xc000/3;
	INT32 G2 = 0x20000/2;
	UINT8 *gfx1 = DrvGfxROM0;
	UINT8 *gfx2 = DrvGfxROM1;
	UINT8 *buf = (UINT8*)BurnMalloc(0x20000);

	/* BG data lines */
	for (i=0;i<G1; i++)
	{
		UINT16 w;

		buf[i] = BITSWAP08(gfx1[i], 0,5,2,6,4,1,7,3);

		w = (gfx1[i+G1] << 8) | gfx1[i+G1*2];
		w = BITSWAP16(w, 14,1,13,5,9,2,10,6, 3,8,4,15,0,11,12,7);

		buf[i+G1]   = w >> 8;
		buf[i+G1*2] = w & 0xff;
	}

	/* BG address lines */
	for (i = 0; i < 3*G1; i++)
		gfx1[i] = buf[BITSWAP16(i,15,14,13,2,1,0,12,11,10,9,8,7,6,5,4,3)];

	/* SPR data lines */
	for (i=0;i<G2; i++)
	{
		UINT16 w;

		w = (gfx2[i] << 8) | gfx2[i+G2];
		w = BITSWAP16(w, 5,7,11,4,15,10,3,14, 9,2,13,8,1,12,0,6 );

		buf[i]    = w >> 8;
		buf[i+G2] = w & 0xff;
	}

	/* SPR address lines */
	for (i = 0; i < 2*G2; i++)
		gfx2[i] = buf[BITSWAP24(i,23,22,21,20,19,18,17,16,15,12,11,10,9,8,7,6,5,4,13,14,3,2,1,0)];

	BurnFree(buf);
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[3]  = { 0x00000, 0x20000, 0x40000 };
	INT32 Plane1[4]  = { 0x40000, 0xc0000, 0x00000, 0x80000 };
	INT32 XOffs0[8]  = { STEP8(7,-1) };
	INT32 XOffs1[16] = { STEP16(15,-1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };
	INT32 YOffs1[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x0c000);

	GfxDecode(0x0800, 3,  8,  8, Plane0, XOffs0, YOffs0, 0x40, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x20000);

	GfxDecode(0x0400, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0;i < 256;i++)
	{
		INT32 bit0,bit1,bit2,bit3,r,g,b;

		/* red component */
		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		bit3 = (DrvColPROM[i] >> 3) & 0x01;
		r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		/* green component */
		bit0 = (DrvColPROM[i + 256] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 256] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 256] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 256] >> 3) & 0x01;
		g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		/* blue component */
		bit0 = (DrvColPROM[i + 512] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 512] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 512] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 512] >> 3) & 0x01;
		b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(56.747);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x08000,  1, 1)) return 1;

		if (BurnLoadRom(t5182ROM   + 0x00000,  2, 1)) return 1;
		if (BurnLoadRom(t5182ROM   + 0x08000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x08000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x18000, 10, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 12, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 13, 1)) return 1;

		seibu_decrypt();
		decode_gfx();
		DrvGfxDecode();
		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80ROMDec,	0x0000, 0x7fff, MAP_FETCHOP);
	ZetMapMemory(DrvVidRAM,		0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(t5182SharedRAM,	0xd400, 0xd4ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM,		0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(mustache_main_write);
	ZetSetReadHandler(mustache_main_read);
	ZetClose();

	t5182Init(1, 14318180/4);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	t5182Exit();

	BurnFree (AllMem);

	return 0;
}

static void draw_layer(INT32 rows_start, INT32 rows_end)
{
	INT32 scrollx[4] = { 0x100 - scroll, 0x100 - scroll, 0x100 - scroll, 0x100 };

	for (INT32 offs = rows_start * 64; offs < 64 * rows_end; offs++)
	{
		INT32 sx = (~offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		sx -= scrollx[(sy / 0x40) & 3] + 8;
		if (sx < -7) sx += 512;
		sy -= 8;

		if (sx >= nScreenWidth || sy >= nScreenHeight || sy <= -7) continue;

		INT32 attr  = DrvVidRAM[offs * 2 + 1];
		INT32 code  = DrvVidRAM[offs * 2 + 0] + ((attr & 0x60) << 3) + ((video_control & 0x08) << 7);
		INT32 color = attr & 0x0f;

		INT32 flipx = attr & 0x10;
		INT32 flipy = attr & 0x80;

		if (flipscreen) {
			sx = 232 - sx;
			sy = 240 - sy;

			flipx ^= 0x10;
			flipy ^= 0x80;
		}

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 3, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x0800; offs += 4)
	{
		INT32 sy = (240-DrvSprRAM[offs]) ;
		INT32 sx = (240-DrvSprRAM[offs+3]) - 8;
		INT32 attr = DrvSprRAM[offs+1];
		INT32 code = DrvSprRAM[offs+2] + ((attr & 0x0c) << 6);
		INT32 color = (attr & 0xe0)>>5;

		if (flipscreen)
		{
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, (232 - sx) - 8, (232 - sy) + 8, color, 4, 0, 0x80, DrvGfxROM1);
		}
		else
		{
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 8, color, 4, 0, 0x80, DrvGfxROM1);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalc = 0;
		DrvPaletteInit();
	}

	// Hack-ish - should clip sprites rather than just draw over them
	INT32 all_rows = (video_control & 0x0a) ? 1 : 0;

	draw_layer(0, (all_rows) ? 32: 24);
	draw_sprites();
	if (all_rows == 0) draw_layer(24,32);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		t5182_coin_input = (DrvJoy4[0] << 0) | (DrvJoy4[1] << 1);
	}

	INT32 nSegment;
	INT32 nInterleave = 16;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 105732, 63079 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);

		nSegment = ((nCyclesTotal[0] / nInterleave) * (i + 1)) - ZetTotalCycles();

		nCyclesDone[0] += ZetRun(nSegment);

		if (i == 0) {
			ZetSetVector(0x08);
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}

		if (i == 12) {
			ZetSetVector(0x10);
			ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		}

		ZetClose();

		ZetOpen(1); // t5182

		nSegment = ((nCyclesTotal[1] / nInterleave) * (i + 1)) - ZetTotalCycles();

		nCyclesDone[1] += ZetRun(nSegment);

		if (pBurnSoundOut) {
			nSegment = nBurnSoundLen / nInterleave;

			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);

			nSoundBufferPos += nSegment;
		}

		ZetClose();
	}

	ZetOpen(1); // t5182

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029729;
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

		t5182Scan(nAction);

		SCAN_VAR(scroll);
		SCAN_VAR(video_control);
		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Mustache Boy (Japan)

static struct BurnRomInfo mustacheRomDesc[] = {
	{ "mustache.h18",	0x8000, 0x123bd9b8, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "mustache.h16",	0x4000, 0x62552beb, 1 | BRF_PRG | BRF_ESS }, //  1

#if !defined (ROM_VERIFY)
	{ "t5182.rom",		0x2000, 0xd354c8fc, 2 | BRF_PRG | BRF_ESS }, //  2 t5182 (Z80 #1) Code
#else
	{ "",				0x0000, 0x00000000, 2 | BRF_PRG | BRF_ESS }, //  2 t5182 (Z80 #1) Code
#endif
	{ "mustache.e5",	0x8000, 0xefbb1943, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "mustache.a13",	0x4000, 0x9baee4a7, 3 | BRF_GRA },           //  4 Background Tiles
	{ "mustache.a14",	0x4000, 0x8155387d, 3 | BRF_GRA },           //  5
	{ "mustache.a16",	0x4000, 0x4db4448d, 3 | BRF_GRA },           //  6

	{ "mustache.a4",	0x8000, 0xd5c3bbbf, 4 | BRF_GRA },           //  7 Sprites
	{ "mustache.a7",	0x8000, 0xe2a6012d, 4 | BRF_GRA },           //  8
	{ "mustache.a5",	0x8000, 0xc975fb06, 4 | BRF_GRA },           //  9
	{ "mustache.a8",	0x8000, 0x2e180ee4, 4 | BRF_GRA },           // 10

	{ "mustache.c3",	0x0100, 0x68575300, 5 | BRF_GRA },           // 11 Color PROMs
	{ "mustache.c2",	0x0100, 0xeb008d62, 5 | BRF_GRA },           // 12
	{ "mustache.c1",	0x0100, 0x65da3604, 5 | BRF_GRA },           // 13

	{ "mustache.b6",	0x1000, 0x5f83fa35, 5 | BRF_GRA | BRF_OPT }, // 14
};

STD_ROM_PICK(mustache)
STD_ROM_FN(mustache)

struct BurnDriver BurnDrvMustache = {
	"mustache", NULL, NULL, NULL, "1987",
	"Mustache Boy (Japan)\0", NULL, "Seibu Kaihatsu (March license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mustacheRomInfo, mustacheRomName, NULL, NULL, MustacheInputInfo, MustacheDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	248, 240, 3, 4
};


// Mustache Boy (Italy)

static struct BurnRomInfo mustacheiRomDesc[] = {
	{ "1.h18",			0x8000, 0x22893fbc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "2.h16",			0x4000, 0xec70cfd3, 1 | BRF_PRG | BRF_ESS }, //  1

#if !defined (ROM_VERIFY)
	{ "t5182.rom",		0x2000, 0xd354c8fc, 2 | BRF_PRG | BRF_ESS }, //  2 t5182 (Z80 #1) Code
#else
	{ "",				0x0000, 0x00000000, 2 | BRF_PRG | BRF_ESS }, //  2 t5182 (Z80 #1) Code
#endif
	{ "10.e5",			0x8000, 0xefbb1943, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "5.a13",			0x4000, 0x9baee4a7, 3 | BRF_GRA },           //  4 Background Tiles
	{ "4.a15",			0x4000, 0x8155387d, 3 | BRF_GRA },           //  5
	{ "3.a16",			0x4000, 0x4db4448d, 3 | BRF_GRA },           //  6

	{ "6.a4",			0x8000, 0x4a95a89c, 4 | BRF_GRA },           //  7 Sprites
	{ "8.a7",			0x8000, 0x3e6be0fb, 4 | BRF_GRA },           //  8
	{ "7.a5",			0x8000, 0x8ad38884, 4 | BRF_GRA },           //  9
	{ "9.a8",			0x8000, 0x3568c158, 4 | BRF_GRA },           // 10

	{ "d.c3",			0x0100, 0x68575300, 5 | BRF_GRA },           // 11 Color PROMs
	{ "c.c2",			0x0100, 0xeb008d62, 5 | BRF_GRA },           // 12
	{ "b.c1",			0x0100, 0x65da3604, 5 | BRF_GRA },           // 13

	{ "a.b6",			0x1000, 0x5f83fa35, 5 | BRF_GRA | BRF_OPT }, // 14
};

STD_ROM_PICK(mustachei)
STD_ROM_FN(mustachei)

struct BurnDriver BurnDrvMustachei = {
	"mustachei", "mustache", NULL, NULL, "1987",
	"Mustache Boy (Italy)\0", NULL, "Seibu Kaihatsu (IG SPA licence)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE, 0,
	NULL, mustacheiRomInfo, mustacheiRomName, NULL, NULL, MustacheInputInfo, MustacheDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	248, 240, 3, 4
};
