// FB Alpha Mad Motor driver module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "h6280_intf.h"
#include "burn_ym2203.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "deco16ic.h"
#include "decobac06.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvHucROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPfRAM0;
static UINT8 *DrvPfRAM1;
static UINT8 *DrvPfRAM2;
static UINT8 *DrvHucRAM;
static UINT8 *DrvColScroll;
static UINT8 *DrvRowScroll;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 pf_control[3][2][4];

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static struct BurnInputInfo MadmotorInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Madmotor)

static struct BurnDIPInfo MadmotorDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0x7f, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x11, 0x01, 0x07, 0x00, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x01, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin B"			},
	{0x11, 0x01, 0x38, 0x00, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x08, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x38, 0x20, "1 Coin  4 Credits"		},
	{0x11, 0x01, 0x38, 0x18, "1 Coin  5 Credits"		},
	{0x11, 0x01, 0x38, 0x10, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x11, 0x01, 0x40, 0x40, "Off"				},
	{0x11, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x03, 0x00, "2"				},
	{0x12, 0x01, 0x03, 0x03, "3"				},
	{0x12, 0x01, 0x03, 0x02, "4"				},
	{0x12, 0x01, 0x03, 0x01, "5"				},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0x0c, 0x08, "Easy"				},
	{0x12, 0x01, 0x0c, 0x0c, "Normal"			},
	{0x12, 0x01, 0x0c, 0x04, "Hard"				},
	{0x12, 0x01, 0x0c, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x12, 0x01, 0x40, 0x00, "No"				},
	{0x12, 0x01, 0x40, 0x40, "Yes"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Madmotor)

static void __fastcall madmotor_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfcffe9) == 0x180000) {
		pf_control[(address / 0x10000) & 3][(address / 0x10) & 1][(address / 2) & 3] = data;
		return;
	}

	switch (address)
	{
		case 0x18c000:
		case 0x30c012:
		return; // nop

		case 0x3fc004:
			deco16_soundlatch = data & 0xff;
			h6280SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}

	bprintf (0, _T("MWW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall madmotor_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("MWB: %5.5x, %2.2x\n"), address, data);
}

static UINT16 __fastcall madmotor_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x18c000:
		case 0x19c000: // nop
			return 0;

		case 0x3f8002:
			return DrvInputs[0];

		case 0x3f8004:
			return (DrvDips[1] * 256) + DrvDips[0];

		case 0x3f8006:
			return (DrvInputs[1] & ~8) + (vblank ? 0 : 8);
	}

	bprintf (0, _T("MRW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall madmotor_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x3f8002:
			return DrvInputs[0] >> 8;

		case 0x3f8003:
			return DrvInputs[0];

		case 0x3f8004:
			return DrvDips[1];

		case 0x3f8005:
			return DrvDips[0];

		case 0x3f8006:
			return 0xff;

		case 0x3f8007:
			return (DrvInputs[1] & ~8) + (vblank ? 0 : 8);
	}

	bprintf (0, _T("MRB: %5.5x\n"), address);

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	deco16SoundReset();

	memset (pf_control, 0, sizeof(pf_control));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x080000;
	DrvHucROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x040000;
	DrvGfxROM1	= Next; Next += 0x080000;
	DrvGfxROM2	= Next; Next += 0x100000;
	DrvGfxROM3	= Next; Next += 0x200000;

	MSM6295ROM	= Next;
	DrvSndROM0	= Next; Next += 0x100000;
	DrvSndROM1	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x004000;
	DrvPalRAM	= Next; Next += 0x000800;
	DrvSprRAM	= Next; Next += 0x000800;
	DrvPfRAM0	= Next; Next += 0x002000;
	DrvPfRAM1	= Next; Next += 0x002000;
	DrvPfRAM2	= Next; Next += 0x001000;

	DrvHucRAM	= Next; Next += 0x002000;

	DrvColScroll	= Next; Next += 0x000400;
	DrvRowScroll	= Next; Next += 0x000400;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode(UINT8 *src, INT32 len, INT32 type)
{
	INT32 Plane[4]  = { (len/4)*8*3, (len/4)*8*1, (len/4)*8*2, (len/4)*8*0 };
	INT32 XOffs[16] = { STEP8(128,1), STEP8(0,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, src, len);

	if (type == 1) {
		GfxDecode((len*2)/(16*16), 4, 16, 16, Plane, XOffs + 0, YOffs, 0x100, tmp, src);
	} else {
		GfxDecode((len*2)/( 8*8 ), 4,  8,  8, Plane, XOffs + 8, YOffs, 0x040, tmp, src);
	}

	BurnFree (tmp);

	return 0;
}

static void DrvProgDecode()
{
	for (INT32 i = 0; i < 0x80000; i++)
	{
		Drv68KROM[i] = BITSWAP08(Drv68KROM[i], 0, 6, 2, 4, 3, 5, 1, 7);
	}
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
		if (BurnLoadRom(Drv68KROM  + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x00000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x40001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x40000,  3, 2)) return 1;

		if (BurnLoadRom(DrvHucROM  + 0x00000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x60000, 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x20000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x80000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0xa0000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x40000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x60000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0xc0000, 19, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0xe0000, 20, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x00000, 21, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x00000, 22, 1)) return 1;

		DrvProgDecode();
		DrvGfxDecode(DrvGfxROM0, 0x020000, 0); // 000, 16 - bg 8x8
		DrvGfxDecode(DrvGfxROM1, 0x040000, 1); // 200, 16 - bg 16x16
		DrvGfxDecode(DrvGfxROM2, 0x080000, 1); // 300, 16 - bg 16x16
		DrvGfxDecode(DrvGfxROM3, 0x100000, 1); // 100, 16 - sprite
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvColScroll,	0x184000, 0x1843ff, MAP_RAM);
	SekMapMemory(DrvRowScroll,	0x184400, 0x1847ff, MAP_RAM);
	SekMapMemory(DrvPfRAM0,		0x188000, 0x189fff, MAP_RAM);
	SekMapMemory(DrvPfRAM1,		0x198000, 0x199fff, MAP_RAM);
	SekMapMemory(DrvPfRAM2,		0x1a4000, 0x1a4fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0x3e0000, 0x3e3fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x3e8000, 0x3e87ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x3f0000, 0x3f07ff, MAP_RAM);
	SekSetWriteWordHandler(0,	madmotor_main_write_word);
	SekSetWriteByteHandler(0,	madmotor_main_write_byte);
	SekSetReadWordHandler(0,	madmotor_main_read_word);
	SekSetReadByteHandler(0,	madmotor_main_read_byte);
 	SekClose();

	deco16SoundInit(DrvHucROM, DrvHucRAM, 4026500, 1, NULL,0.45, 1023924, 0.50, 2047848, 0.25);
//	deco16_music_tempofix = 1;
	MSM6295SetBank(0, DrvSndROM0, 0, 0x3ffff);
	MSM6295SetBank(1, DrvSndROM1, 0, 0x3ffff);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	deco16SoundExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x800/2; i++)
	{
		UINT8 r = (p[i] >> 0) & 0xf;
		UINT8 g = (p[i] >> 4) & 0xf;
		UINT8 b = (p[i] >> 8) & 0xf;

		DrvPalette[i] = BurnHighCol(r*16+r,g*16+g,b*16+b,0);
	}
}

static void draw_sprites()
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0; offs < 0x800/2; offs+=4)
	{
		if ((spriteram[offs] & 0x8000) == 0) {
			continue;
		}

		INT32 sy    = spriteram[offs];
		INT32 sx    = spriteram[offs + 2];
		INT32 color = sx >> 12;
		INT32 flash = sx & 0x0800;
		INT32 flipx = sy & 0x2000;
		INT32 flipy = sy & 0x4000;
		INT32 high  = 1 << ((sy & 0x1800) >> 11);
		INT32 wide  = 1 << ((sy & 0x0600) >>  9);

		sx = sx & 0x01ff;
		sy = sy & 0x01ff;
		if (sx >= 256) sx -= 512;
		if (sy >= 256) sy -= 512;
		sx = 240 - sx;
		sy =(240 - sy) - 8;

		if ((flash == 0) || (nCurrentFrame & 1))
		{
			for (INT32 x = 0; x < wide; x++)
			{
				INT32 incy;
				INT32 code = (spriteram[offs + 1] & 0x1fff) & (~(high - 1));

				if (flipy)
					incy = -1;
				else
				{
					code += high - 1;
					incy = 1;
				}

				for (INT32 y = 0; y < high; y++)
				{
					if (flipy) {
						if (flipx) {
							Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code - y * incy, sx + (-16 * x), sy + (-16 * y), color, 4, 0, 0x100, DrvGfxROM3);
						} else {
							Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code - y * incy, sx + (-16 * x), sy + (-16 * y), color, 4, 0, 0x100, DrvGfxROM3);
						}
					} else {
						if (flipx) {
							Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code - y * incy, sx + (-16 * x), sy + (-16 * y), color, 4, 0, 0x100, DrvGfxROM3);
						} else {
							Render16x16Tile_Mask_Clip(pTransDraw, code - y * incy, sx + (-16 * x), sy + (-16 * y), color, 4, 0, 0x100, DrvGfxROM3);
						}
					}
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force
	}

	BurnTransferClear();

	bac06_depth = 4;
	bac06_yadjust = 8;

	if (nBurnLayer & 4) bac06_draw_layer(DrvPfRAM2, pf_control[2], NULL, NULL, DrvGfxROM0, 0x000, 0xfff, DrvGfxROM2, 0x300, 0xfff, 1, 1);
	if (nBurnLayer & 2) bac06_draw_layer(DrvPfRAM1, pf_control[1], NULL, NULL, DrvGfxROM0, 0x000, 0xfff, DrvGfxROM1, 0x200, 0x7ff, 0, 0);

	draw_sprites();

	if (nBurnLayer & 1) bac06_draw_layer(DrvPfRAM0, pf_control[0], DrvRowScroll, DrvColScroll, DrvGfxROM0, 0x000, 0xfff, DrvGfxROM0, 0x000, 0x000, 0, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	h6280NewFrame();

	{
		memset (DrvInputs, 0xff, 2 * sizeof(UINT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 12000000 / 60, 4026500 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;
	
	SekOpen(0);
	h6280Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += SekRun(nCyclesTotal[0] / nInterleave);
		if (i == 248) {
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
			vblank = 1;
		}

		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));

		if (pBurnSoundOut && i%4 == 3) { // this fixes small tempo fluxuations in cninja
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 4);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			deco16SoundUpdate(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			deco16SoundUpdate(pSoundBuf, nSegmentLength);
		}
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	h6280Close();
	SekClose();

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
		SekScan(nAction);
		deco16SoundScan(nAction, pnMin);

		SCAN_VAR(pf_control);
	}

	return 0;
}


// Mad Motor (prototype)

static struct BurnRomInfo madmotorRomDesc[] = {
	{ "02-2.b4",	0x20000, 0x50b554e0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "00-2.b1",	0x20000, 0x2d6a1b3f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "03-2.b6",	0x20000, 0x442a0a52, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "01-2.b3",	0x20000, 0xe246876e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "14.l7",	0x10000, 0x1c28a7e5, 2 | BRF_PRG | BRF_ESS }, //  4 H6280 Code

	{ "04.a9",	0x10000, 0x833ca3ab, 3 | BRF_GRA },           //  5 Layer 0 Tiles (8x8)
	{ "05.a11",	0x10000, 0xa691fbfe, 3 | BRF_GRA },           //  6

	{ "10.a19",	0x20000, 0x9dbf482b, 4 | BRF_GRA },           //  7 Layer 1 Tiles (16x16)
	{ "11.a21",	0x20000, 0x593c48a9, 4 | BRF_GRA },           //  8

	{ "06.a13",	0x20000, 0x448850e5, 5 | BRF_GRA },           //  9 Layer 2 Tiles (16x16)
	{ "07.a14",	0x20000, 0xede4d141, 5 | BRF_GRA },           // 10
	{ "08.a16",	0x20000, 0xc380e5e5, 5 | BRF_GRA },           // 11
	{ "09.a18",	0x20000, 0x1ee3326a, 5 | BRF_GRA },           // 12

	{ "15.h11",	0x20000, 0x90ae9f74, 6 | BRF_GRA },           // 13 Sprites
	{ "16.h13",	0x20000, 0xe96ac815, 6 | BRF_GRA },           // 14
	{ "17.h14",	0x20000, 0xabad9a1b, 6 | BRF_GRA },           // 15
	{ "18.h16",	0x20000, 0x96d8d64b, 6 | BRF_GRA },           // 16
	{ "19.j13",	0x20000, 0xcbd8c9b8, 6 | BRF_GRA },           // 17
	{ "20.j14",	0x20000, 0x47f706a8, 6 | BRF_GRA },           // 18
	{ "21.j16",	0x20000, 0x9c72d364, 6 | BRF_GRA },           // 19
	{ "22.j18",	0x20000, 0x1e78aa60, 6 | BRF_GRA },           // 20

	{ "12.h1",	0x20000, 0xc202d200, 7 | BRF_SND },           // 21 OKI #0 Samples

	{ "13.h3",	0x20000, 0xcc4d65e9, 8 | BRF_SND },           // 22 OKI #1 Samples
};

STD_ROM_PICK(madmotor)
STD_ROM_FN(madmotor)

struct BurnDriver BurnDrvMadmotor = {
	"madmotor", NULL, NULL, NULL, "1989",
	"Mad Motor (prototype)\0", NULL, "Mitchell", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, madmotorRomInfo, madmotorRomName, NULL, NULL, NULL, NULL, MadmotorInputInfo, MadmotorDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};
