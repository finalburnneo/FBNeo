// Mouser for FBA, ported by vbt with help from dink
// Based on MAME driver by Frank Palazzolo
#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
#include "bitswap.h"
extern "C" {
	#include "ay8910.h"
}
static INT16 *pAY8910Buffer[6];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDip[2];
static UINT8 DrvReset;
static UINT8 DrvRecalc;

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvRAM;
static UINT8 *DrvMainROM;
static UINT8 *DrvDecROM;
static UINT8 *DrvSubROM;
static UINT8 *DrvSubRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT32 *DrvPalette;

static UINT8 sound_byte;
static UINT8 nmi_enable;
static void DrvPaletteInit();

static struct BurnInputInfo MouserInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 7,	"p1 right"},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 left"},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy3 + 7,	"p2 right"},
	{"P2 Button 1",	BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvJoy1 + 5,"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};

STDINPUTINFO(Mouser)


static struct BurnDIPInfo MouserDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x20, NULL		},
	{0x10, 0xff, 0xff, 0x80, NULL		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x0f, 0x01, 0x20, 0x00, "Normal"		},
	{0x0f, 0x01, 0x20, 0x20, "Hard"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x03, 0x00, "3"		},
	{0x10, 0x01, 0x03, 0x01, "4"		},
	{0x10, 0x01, 0x03, 0x02, "5"		},
	{0x10, 0x01, 0x03, 0x03, "6"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x10, 0x01, 0x0c, 0x00, "20000"		},
	{0x10, 0x01, 0x0c, 0x04, "40000"		},
	{0x10, 0x01, 0x0c, 0x08, "60000"		},
	{0x10, 0x01, 0x0c, 0x0c, "80000"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x10, 0x01, 0x70, 0x70, "5 Coins 1 Credit"		},
	{0x10, 0x01, 0x70, 0x50, "4 Coins 1 Credit"		},
	{0x10, 0x01, 0x70, 0x30, "3 Coins 1 Credit"		},
	{0x10, 0x01, 0x70, 0x10, "2 Coins 1 Credit"		},
	{0x10, 0x01, 0x70, 0x00, "1 Coin  1 Credit"		},
	{0x10, 0x01, 0x70, 0x20, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x70, 0x40, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x70, 0x60, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x80, 0x80, "Upright"		},
	{0x10, 0x01, 0x80, 0x00, "Cocktail"		},
};

STDDIPINFO(Mouser)

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	sound_byte = 0;
	nmi_enable = 0;
	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	return 0;
}

static void DrawSprite(INT32 offs)
{
	INT32 sx = DrvSprRAM[offs + 3];
	INT32 sy = 0xef - DrvSprRAM[offs + 2];

	INT32 flipx = BIT(DrvSprRAM[offs], 6);
	INT32 flipy = BIT(DrvSprRAM[offs], 7);
	sy-=16; // screen offset
	if (BIT(DrvSprRAM[offs+1], 4)) {
		INT32 code  = DrvSprRAM[offs] & 0x3f;
		INT32 color =  DrvSprRAM[offs + 1] % 16;

		if (sx < -7 || sy < -7 || sx >= nScreenWidth || sy >= nScreenHeight) return;

		if (flipy) 
		{
			if (flipx) 
			{
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1+(((DrvSprRAM[offs+1]&0x20)>>5)*16384));
			}
			else
			{
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1+(((DrvSprRAM[offs+1]&0x20)>>5)*16384));
			}
		}
		else
		{
			if (flipx)
			{
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1+(((DrvSprRAM[offs+1]&0x20)>>5)*16384));
			} 
			else 
			{
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 0, 0, DrvGfxROM1+(((DrvSprRAM[offs+1]&0x20)>>5)*16384));
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc)
	{
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	/* for every character in the Video RAM, check if it has been modified */
	/* since last time and update it accordingly. */
	for (INT32 offs = 0x3ff;offs >= 0;offs--)
	{
		INT32 sx = (offs % 32)*8;
		INT32 sy = (offs / 32)*8;

		sy = (256 + sy - DrvSprRAM[offs%32])%256;
		sy -= 16; // screen offset
		INT32 color_offs = offs % 32 + ((256 + 8 * (offs / 32) - DrvSprRAM[offs % 32] )% 256) / 8 * 32;
		INT32 code  = DrvVidRAM[offs] | (DrvColRAM[color_offs] >> 5) * 256 | ((DrvColRAM[color_offs] >> 4) & 1) * 512;
		INT32 color = DrvColRAM[color_offs]%16;
		Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 2, 0, DrvGfxROM0);
	}

	for(INT32 offs = 0x0084; offs < 0x00A0; offs += 4)
	{
		DrawSprite(offs);
	}

	/* This is the second set of 8 sprites */
	for(INT32 offs = 0x00C4; offs < 0x00E4; offs += 4)
	{
		DrawSprite(offs);
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static UINT8 __fastcall mouser_sub_read(UINT16 address)
{
	if(address == 0x3000) {
		ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return sound_byte;
	}

	return 0;
}

static void __fastcall mouser_sub_write(UINT16 address, UINT8 /*data*/)
{
	if(address == 0x4000)
		ZetSetIRQLine(Z80_INPUT_LINE_NMI, CPU_IRQSTATUS_NONE);
}

static UINT8 __fastcall mouser_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return DrvInputs[1];
		case 0xa800:
			return DrvInputs[0];
		case 0xb000:
			return DrvDip[1];
		case 0xb800:
			return DrvInputs[2];
		break;
	}

	return 0;
}

static void __fastcall mouser_main_write(UINT16 address, UINT8 data)
{
	if(address >= 0x8800 && address <= 0x88ff)
	{
		return;
	}

	switch (address)
	{
		case 0xa000:
			nmi_enable = data;
			return;
		case 0xa001:
			//mouser_flip_screen_x_w(); //AM_WRITE(mouser_flip_screen_x_w)
			return;
		case 0xa002:
			//mouser_flip_screen_y_w(); //AM_WRITE(mouser_flip_screen_y_w)
			return;
		case 0xb800:
			sound_byte = data;
			ZetClose();
			ZetOpen(1);
			ZetSetIRQLine(0x00, CPU_IRQSTATUS_ACK);
			ZetClose();
			ZetOpen(0);
			return;
	}
}

void __fastcall mouser_sub_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			AY8910Write(0, 1, data);
		break;

		case 0x01:
			AY8910Write(0, 0, data);
		break;

		case 0x80:
			AY8910Write(1, 1, data);
		break;

		case 0x81:
			AY8910Write(1, 0, data);
		break;
	}
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	 	= Next; Next += 0x10000;
	DrvDecROM	 	= Next; Next += 0x10000;
	DrvSubROM	 	= Next; Next += 0x10000;
	DrvGfxROM0		= Next; Next += 0x10000;
	DrvGfxROM1		= Next; Next += 0x10000;
	DrvColPROM		= Next; Next += 0x040;
	DrvPalette		= (UINT32*)Next; Next += 0x040 * sizeof(UINT32);
	AllRam			= Next;
	DrvRAM			= Next; Next += 0x3000;
	DrvSubRAM		= Next; Next += 0x2000;
	DrvVidRAM		= Next; Next += 0x800;
	DrvSprRAM		= Next; Next += 0x4FF;
	DrvColRAM		= Next; Next += 0x400;

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

static void DrvGfxDecode()
{
	INT32 Planes0[2] = { 1*8192*8, 0*8192*8 };
	INT32 XOffs0[8] = {0, 1, 2, 3, 4, 5, 6, 7};
	INT32 YOffs0[8] = {STEP8(0, 8)};//{8*0, 8*1, 8*2, 8*3, 8*4, 8*5, 8*6, 8*7};

	INT32 Planes1[2] = { 1*8192*8, 0*8192*8 };
	INT32 XOffs1[16] = {0, 1, 2, 3, 4, 5, 6, 7, 64+0, 64+1, 64+2, 64+3, 64+4, 64+5, 64+6, 64+7};
	INT32 YOffs1[16] = {STEP8(0, 8), 128+8*0, 128+8*1, 128+8*2, 128+8*3, 128+8*4, 128+8*5, 128+8*6, 128+8*7};

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM0, 0x4000);

	GfxDecode(0x0400, 2,  8,  8, Planes0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0); // modulo 0x040 to verify !!!
	GfxDecode(0x0040, 2, 16, 16, Planes1, XOffs1, YOffs1, 0x100, tmp+0x1000, DrvGfxROM1); // modulo 0x040 to verify !!!
	GfxDecode(0x0040, 2, 16, 16, Planes1, XOffs1, YOffs1, 0x100, tmp+0x1800, DrvGfxROM1+(0x800*8)); // modulo 0x100 to verify !!!

	BurnFree (tmp);
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x40; i++)
	{
		INT32 bit0,bit1,bit2,r,g,b;

		/* red component */
		bit0 = (DrvColPROM[i] >> 0) & 0x01;
		bit1 = (DrvColPROM[i] >> 1) & 0x01;
		bit2 = (DrvColPROM[i] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* green component */
		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
		/* blue component */
		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		b = 0x4f * bit0 + 0xa8 * bit1;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 MouserDecode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x200);

	if (BurnLoadRom(tmp + 0x00000, 6, 2)) return 1; // { "bprom.4b",	0x0100, 0xdd233851, 4 }, //  6 user1
	if (BurnLoadRom(tmp + 0x00001, 7, 2)) return 1; // { "bprom.4c",	0x0100, 0x60aaa686, 4 }, //  7

	for (INT32 l = 0; l < 0x200; l+=2) 
	{
		tmp[l/2] = (tmp[l+1] & 0x0f) | (tmp[l+0] << 4);
	}

	for (INT32 i = 0; i < 0x10000; i++) 
	{
		DrvDecROM[i] = tmp[DrvMainROM[i]];
	}

	BurnFree (tmp);
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
		if (BurnLoadRom(DrvMainROM + 0x00000,  0, 1)) return 1;	// { "m0.5e",	0x2000, 0xb56e00bc, 1 }, //  0 maincpu
		if (BurnLoadRom(DrvMainROM + 0x02000,  1, 1)) return 1;	// {"m1.5f",	0x2000, 0xae375d49, 1 }
		if (BurnLoadRom(DrvMainROM + 0x04000,  2, 1)) return 1;	// { "m2.5j",	0x2000, 0xef5817e4, 1 }, //  2
		if (BurnLoadRom(DrvSubROM  + 0x00000,  3, 1)) return 1;	// { "m5.3v",	0x1000, 0x50705eec, 2 }, //  3 audiocpu
		// Gfx char + sprite
		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  4, 1)) return 1; // { "m3.11h",	0x2000, 0xaca2834e, 3 }, //  4 gfx1
		if (BurnLoadRom(DrvGfxROM0  + 0x02000,  5, 1)) return 1; // { "m4.11k",	0x2000, 0x943ab2e2, 3 }, //  5 
		// Opcode Decryption PROMS
		MouserDecode();
		// Palette
		if (BurnLoadRom(DrvColPROM + 0x000000, 8, 1)) return 1; // { "bprom.5v",	0x0020, 0x7f8930b2, 5 }, //  8 proms
		if (BurnLoadRom(DrvColPROM + 0x000020, 9, 1)) return 1;	// { "bprom.5u",	0x0020, 0x0086feed, 5 }, //  9
		DrvPaletteInit();
		DrvGfxDecode();
	}
	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x5fff, 0, DrvMainROM);
	ZetMapArea(0x0000, 0x5fff, 2, DrvDecROM, DrvMainROM);
	ZetMapMemory(DrvRAM,			0x6000, 0x6bff, MAP_RAM);//AM_RANGE(0x6000, 0x6bff) AM_RAM
	ZetMapMemory(DrvVidRAM,		0x9000, 0x93ff, MAP_RAM);//AM_RANGE(0x9000, 0x93ff) AM_RAM AM_SHARE("videoram")
	ZetMapMemory(DrvSprRAM,		0x9800, 0x9cff, MAP_RAM);//AM_RANGE(0x9800, 0x9cff) AM_RAM AM_SHARE("spriteram")
	ZetMapMemory(DrvColRAM,		0x9c00, 0x9fff, MAP_RAM);//AM_RANGE(0x9c00, 0x9fff) AM_RAM AM_SHARE("colorram")
	ZetSetWriteHandler(mouser_main_write);
	ZetSetReadHandler(mouser_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvSubROM,	0x0000, 0x1fff, MAP_ROM);//AM_RANGE(0x0000, 0x1fff) AM_ROM
	ZetMapMemory(DrvSubRAM,		0x2000, 0x23ff, MAP_RAM);//AM_RANGE(0x2000, 0x23ff) AM_RAM
	ZetSetWriteHandler(mouser_sub_write);
	ZetSetReadHandler(mouser_sub_read);
	ZetSetOutHandler(mouser_sub_out);
	ZetClose();

	AY8910Init(0, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);

	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);

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

	BurnFree (AllMem);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	memset (DrvInputs, 0x00, 3); // DrvJoy1 = active low, 2, 3 = active high
	memset (DrvInputs, 0xff, 1); // DrvJoy1 = active low, 2, 3 = active high

	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;

	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = 4000000 / 60;

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		ZetRun(nCyclesTotal / nInterleave);

		if(i == 240 && nmi_enable&1) {
			ZetNmi();
		}
		ZetClose();

		ZetOpen(1);
		ZetRun(nCyclesTotal / nInterleave);
		if (i % (nInterleave / 5) == (nInterleave / 5) - 1) {
			if(nmi_enable & 1)
				ZetSetIRQLine(Z80_INPUT_LINE_NMI, CPU_IRQSTATUS_ACK);
		}
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
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
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

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(sound_byte);
		SCAN_VAR(nmi_enable);
	}

	return 0;
}


// Mouser

static struct BurnRomInfo mouserRomDesc[] = {
	{ "m0.5e",	0x2000, 0xb56e00bc, 1 }, //  0 maincpu
	{ "m1.5f",	0x2000, 0xae375d49, 1 }, //  1
	{ "m2.5j",	0x2000, 0xef5817e4, 1 }, //  2

	{ "m5.3v",	0x1000, 0x50705eec, 2 }, //  3 audiocpu

	{ "m3.11h",	0x2000, 0xaca2834e, 3 }, //  4 gfx1
	{ "m4.11k",	0x2000, 0x943ab2e2, 3 }, //  5

	{ "bprom.4b",	0x0100, 0xdd233851, 4 }, //  6 user1
	{ "bprom.4c",	0x0100, 0x60aaa686, 4 }, //  7

	{ "bprom.5v",	0x0020, 0x7f8930b2, 5 }, //  8 proms
	{ "bprom.5u",	0x0020, 0x0086feed, 5 }, //  9
};

STD_ROM_PICK(mouser)
STD_ROM_FN(mouser)

struct BurnDriver BurnDrvMouser = {
	"mouser", NULL, NULL, NULL, "1983",
	"Mouser\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, mouserRomInfo, mouserRomName, NULL, NULL, MouserInputInfo, MouserDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};

// Mouser (Cosmos)

static struct BurnRomInfo mousercRomDesc[] = {
	{ "83001.0",	0x2000, 0xe20f9601, 1 }, //  0 maincpu
	{ "m1.5f",	0x2000, 0xae375d49, 1 }, //  1
	{ "m2.5j",	0x2000, 0xef5817e4, 1 }, //  2

	{ "m5.3v",	0x1000, 0x50705eec, 2 }, //  3 audiocpu

	{ "m3.11h",	0x2000, 0xaca2834e, 3 }, //  4 gfx1
	{ "m4.11k",	0x2000, 0x943ab2e2, 3 }, //  5

	{ "bprom.4b",	0x0100, 0xdd233851, 4 }, //  6 user1
	{ "bprom.4c",	0x0100, 0x60aaa686, 4 }, //  7

	{ "bprom.5v",	0x0020, 0x7f8930b2, 5 }, //  8 proms
	{ "bprom.5u",	0x0020, 0x0086feed, 5 }, //  9
};

STD_ROM_PICK(mouserc)
STD_ROM_FN(mouserc)

struct BurnDriver BurnDrvMouserc = {
	"mouserc", "mouser", NULL, NULL, "1983",
	"Mouser (Cosmos)\0", NULL, "UPL (Cosmos license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, mousercRomInfo, mousercRomName, NULL, NULL, MouserInputInfo, MouserDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	224, 256, 3, 4
};
