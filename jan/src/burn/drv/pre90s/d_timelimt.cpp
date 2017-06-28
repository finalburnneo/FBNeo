// FB Alpha Time Limit driver module
// Based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvColPROM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8  DrvJoy1[8];
static UINT8  DrvJoy2[8];
static UINT8  DrvDips[1];
static UINT8  DrvInputs[2];
static UINT8  DrvReset;

static UINT8 soundlatch;
static UINT8 nmi_enable;
static UINT8 scrolly;
static UINT16 scrollx;
static INT32 watchdog;

static INT32 TimelimtMode = 0;

static struct BurnInputInfo TimelimtInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dips",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Timelimt)

static struct BurnInputInfo ProgressInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dips",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Progress)

static struct BurnDIPInfo TimelimtDIPList[]=
{
	{0x08, 0xff, 0xff, 0x01, NULL				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x08, 0x01, 0x07, 0x00, "2 Coins 1 Credits"		},
	{0x08, 0x01, 0x07, 0x01, "1 Coin  1 Credits"		},
	{0x08, 0x01, 0x07, 0x02, "1 Coin  2 Credits"		},
	{0x08, 0x01, 0x07, 0x03, "1 Coin  3 Credits"		},
	{0x08, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x08, 0x01, 0x07, 0x05, "1 Coin  5 Credits"		},
	{0x08, 0x01, 0x07, 0x06, "1 Coin  6 Credits"		},
	{0x08, 0x01, 0x07, 0x07, "1 Coin  7 Credits"		},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x08, 0x01, 0x10, 0x00, "3"				},
	{0x08, 0x01, 0x10, 0x10, "5"				},

	{0   , 0xfe, 0   ,    2, "Invincibility (Cheat)"	},
	{0x08, 0x01, 0x80, 0x00, "Off"				},
	{0x08, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Timelimt)

static struct BurnDIPInfo ProgressDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x01, NULL				},

	{0   , 0xfe, 0   ,    8, "Coinage"			},
	{0x0a, 0x01, 0x07, 0x00, "2 Coins 1 Credits"		},
	{0x0a, 0x01, 0x07, 0x01, "1 Coin  1 Credits"		},
	{0x0a, 0x01, 0x07, 0x02, "1 Coin  2 Credits"		},
	{0x0a, 0x01, 0x07, 0x03, "1 Coin  3 Credits"		},
	{0x0a, 0x01, 0x07, 0x04, "1 Coin  4 Credits"		},
	{0x0a, 0x01, 0x07, 0x05, "1 Coin  5 Credits"		},
	{0x0a, 0x01, 0x07, 0x06, "1 Coin  6 Credits"		},
	{0x0a, 0x01, 0x07, 0x07, "1 Coin  7 Credits"		},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x0a, 0x01, 0x10, 0x00, "3"				},
	{0x0a, 0x01, 0x10, 0x10, "5"				},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x0a, 0x01, 0x20, 0x00, "20,000 & 100,000"		},
	{0x0a, 0x01, 0x20, 0x20, "50,000 & 200,000"		},

	{0   , 0xfe, 0   ,    2, "Invincibility (Cheat)"	},
	{0x0a, 0x01, 0x80, 0x00, "Off"				},
	{0x0a, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Progress)

static void __fastcall timelimt_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xb000:
			nmi_enable = data & 1;
		return;

		case 0xb003:
			if (data & 1) {
				ZetClose();
				ZetOpen(1);
				ZetReset();
				ZetClose();
				ZetOpen(0);
			}
		return;

		case 0xb800:
			soundlatch = data;
		return;

		case 0xc800:
			scrollx = (scrollx & 0xff00) + data;
		return;

		case 0xc801:
			scrollx = (scrollx & 0x00ff) + ((data & 1) << 8);
		return;

		case 0xc802:
			scrolly = data;
		return;

		case 0xc803: // nop
		case 0xc804:
		return;
	}
}

static UINT8 __fastcall timelimt_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return DrvInputs[0];

		case 0xa800:
			return DrvInputs[1];

		case 0xb000:
			return DrvDips[0];
	}

	return 0;
}

static UINT8 __fastcall timelimt_main_read_port(UINT16 port)
{
	if ((port & 0xff) == 0) {
		watchdog = 0;
		return 0;
	}

	return 0;
}

static void __fastcall timelimt_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			soundlatch = 0;
		return;

		case 0x8c:
		case 0x8d:
		case 0x8e:
		case 0x8f:
			AY8910Write((port & 2) ? 1 : 0, port & 1, data);
		return;
	}
}

static UINT8 __fastcall timelimt_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x8c:
		case 0x8d:
		case 0x8e:
		case 0x8f:
			return AY8910Read((port & 2) ? 1 : 0);
	}

	return 0;
}

static UINT8 timelimt_ay8910_1_portA_read(UINT32)
{
	return soundlatch;
}

static INT32 DrvDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	soundlatch = 0;
	nmi_enable = 0;
	scrollx = 0;
	scrolly = 0;
	watchdog = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x004000;
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000060;

	DrvPalette		= (UINT32*)Next; Next += 0x0060 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x000c00;

	DrvVidRAM0		= Next; Next += 0x000400;
	DrvVidRAM1		= Next; Next += 0x000800;

	DrvSprRAM		= Next; Next += 0x000100;

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

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x60; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x4f * bit0 + 0xa8 * bit1;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void DrvGfxDecode()
{
	static INT32 Planes0[4] = { 0, 4, (0x1000*8)+0, (0x1000*8)+4 };
	static INT32 Planes1[3] = { 0, (0x2000*8), (0x4000*8) };
	static INT32 XOffs0[8]  = { STEP4(0,1), STEP4(8,1) };
	static INT32 YOffs0[8]  = { STEP8(0,16) };
	static INT32 XOffs1[16] = { STEP8(0,1), STEP8(64,1) };
	static INT32 YOffs1[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8 *)BurnMalloc(0x6000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM0, 0x4000);

	GfxDecode(0x0100, 4,  8,  8, Planes0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x0100, 4,  8,  8, Planes0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x6000);

	GfxDecode(0x0100, 3, 16, 16, Planes1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM2);

	BurnFree (tmp);
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
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x4000, 10, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020, 12, 1)) return 1;

		if (!TimelimtMode)
			if (BurnLoadRom(DrvColPROM + 0x0040, 13, 1)) return 1;

		if (TimelimtMode)
		{ // fill in missing timelimt prom with color values, values (c) 2017 -dink
	            const UINT8 color_data[32] = {
		        //0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f
				0x00, 0x00, 0xA4, 0xF6, 0xC0, 0x2F, 0x07, 0xFF, 0x00, 0x99, 0x99, 0xF6, 0x0A, 0x1F, 0x58, 0xFF,
				0x00, 0x0F, 0xB5, 0x54, 0xE1, 0x50, 0x5F, 0x64, 0x00, 0x0B, 0x53, 0x0F, 0x80, 0x08, 0x0D, 0xAE
			};

			memcpy (DrvColPROM + 0x40, color_data, 0x20);
		}

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000, 14, 1)) return 1;

		if (TimelimtMode)
			if (BurnLoadRom(DrvZ80ROM1 + 0x1000, 15, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,	0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,	0x9000, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0x9800, 0x98ff, MAP_RAM);
	ZetSetWriteHandler(timelimt_main_write);
	ZetSetReadHandler(timelimt_main_read);
	ZetSetInHandler(timelimt_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,	0x3000, 0x3bff, MAP_RAM);
	ZetSetOutHandler(timelimt_sound_write_port);
	ZetSetInHandler(timelimt_sound_read_port);
	ZetClose();

	AY8910Init(0, 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);

	AY8910Init(1, 1536000, nBurnSoundRate, &timelimt_ay8910_1_portA_read, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree(AllMem);

	TimelimtMode = 0;

	return 0;
}

static void draw_bg_layer()
{
	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		sx -= scrollx;
		if (sx < -7) sx += 512;
		if (sx >= nScreenWidth) continue;

		sy -= (scrolly + 16) & 0xff;
		if (sy < -7) sy += 256;
		if (sy >= nScreenHeight) continue;

		INT32 code = DrvVidRAM1[offs];

		Render8x8Tile_Clip(pTransDraw, code, sx, sy, 0, 4, 0, DrvGfxROM1);
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = ((offs & 0x1f) * 8);
		INT32 sy = ((offs / 0x20) * 8) - 16;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 code = DrvVidRAM0[offs];

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, 0, 4, 0, 0x20, DrvGfxROM0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x100-4; offs >= 0; offs -= 4 )
	{
		INT32 sy    = (240 - DrvSprRAM[offs]) - 16;
		INT32 sx    = DrvSprRAM[offs+3];
		INT32 attr  = DrvSprRAM[offs+2];
		INT32 code  =(DrvSprRAM[offs+1] & 0x3f) | ((attr & 0x40) << 1) | ((attr & 0x80) >> 1);
		INT32 flipy = DrvSprRAM[offs+1] & 0x80;
		INT32 flipx = DrvSprRAM[offs+1] & 0x40;
		INT32 color = attr & 0x07;

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
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

	if (nBurnLayer & 1) draw_bg_layer();
	if (nBurnLayer & 2) draw_sprites();
	if (nBurnLayer & 4) draw_fg_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	/*  -- save until 100% sure colors jive well. --
	static UINT8 oldj = 0;
	static INT32 selektor = 0;

	if (DrvJoy1[4] && oldj == 0) {
		selektor++;
	}
	if (counter!=0) DrvColPROM[0x40 + selektor] = counter;
	bprintf(0, _T("DrvColPROM[0x40 + %X] = %X.\n"), selektor, counter);
	oldj = DrvJoy1[4];

	//    0x40 + 2 = windows in police car
	//         + 3 = tires
	//           5 = hubcaps
	//           6 = headlights
	//           7 = body
	//           8
	//           9 = police pants 0x99
	//           A = police coat    ""
	//           B = shoes/hair
	//           C = Holster/belt 0xa ?
	//           D = Face       0x1f
	//			 E = gun        0x58
	//           F = cuff       0x65
	//           10 = nothing
	//           11 = gang suit 0xf
	//           12 = gang face 0xb5
	//           13 = gang shoes 0x54
	//           14 = gang tie/knife 0xe1
	//           15 = killr shirt 0x50
	//           16 = gang shirt 0x5f
	//           17 = "" pants 0x64
	//           18 = ?
	//           19 = bomb clock 0xb
	//           1a = skull border 0x53
	//           1b = bomb wrapper 0xf
	//           1c = bomb border/wire, 0x80
	//           1d = clock hands 0x8
	//           1e = bomb wrapper 2 0xd
	//           1f = skull 0xae
	*/

	{
		DrvInputs[0] = 0;
		DrvInputs[1] = 0x03;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}



	INT32 nInterleave = 50; // 3000 hz
	INT32 nCyclesTotal[2] = { 5000000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == (nInterleave - 1) && nmi_enable && nCurrentFrame & 1) ZetNmi();
		ZetClose();

		nSegment = nCyclesTotal[1] / nInterleave;

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nSegment);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
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

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
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

		AY8910Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
	}

	return 0;
}


// Time Limit

static struct BurnRomInfo timelimtRomDesc[] = {
	{ "t8",		0x2000, 0x006767ca, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 code
	{ "t7",		0x2000, 0xcbe7cd86, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "t6",		0x2000, 0xf5f17e39, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "t9",		0x2000, 0x2d72ab45, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tl11",	0x1000, 0x46676307, 2 | BRF_GRA },           //  4 Foreground Tiles
	{ "tl10",	0x1000, 0x2336908a, 2 | BRF_GRA },           //  5

	{ "tl13",	0x1000, 0x072e4053, 3 | BRF_GRA },           //  6 Background Tiles
	{ "tl12",	0x1000, 0xce960389, 3 | BRF_GRA },           //  7

	{ "tl3",	0x2000, 0x01a9fd95, 4 | BRF_GRA },           //  8 Sprites
	{ "tl2",	0x2000, 0x4693b849, 4 | BRF_GRA },           //  9
	{ "tl1",	0x2000, 0xc4007caf, 4 | BRF_GRA },           // 10

	{ "clr.35",	0x0020, 0x9c9e6073, 5 | BRF_GRA },           // 11 Color proms
	{ "clr.48",	0x0020, 0xa0bcac59, 5 | BRF_GRA },           // 12
	{ "clr.57",	0x0020, 0x00000000, 5 | BRF_NODUMP | BRF_GRA },           // 13

	{ "tl5",	0x1000, 0x5b782e4a, 6 | BRF_PRG | BRF_ESS }, // 14 Z80 #1 code
	{ "tl4",	0x1000, 0xa32883a9, 6 | BRF_PRG | BRF_ESS }, // 15
};

STD_ROM_PICK(timelimt)
STD_ROM_FN(timelimt)

static INT32 TimelimtInit()
{
	TimelimtMode = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvTimelimt = {
	"timelimt", NULL, NULL, NULL, "1983",
	"Time Limit\0", NULL, "Chuo Co. Ltd", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 1, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, timelimtRomInfo, timelimtRomName, NULL, NULL, TimelimtInputInfo, TimelimtDIPInfo,
	TimelimtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};


// Progress

static struct BurnRomInfo progressRomDesc[] = {
	{ "pg8.bin",	0x2000, 0xe8779658, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 code
	{ "pg7.bin",	0x2000, 0x5dcf6b6f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pg6.bin",	0x2000, 0xf21d2a08, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pg9.bin",	0x2000, 0x052ab4ac, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "pg11.bin",	0x1000, 0xbd8462e4, 2 | BRF_GRA },           //  4 Foreground Tiles
	{ "pg10.bin",	0x1000, 0xc4bbf0b8, 2 | BRF_GRA },           //  5

	{ "pg13.bin",	0x1000, 0x25ec45be, 3 | BRF_GRA },           //  6 Background Tiles
	{ "pg12.bin",	0x1000, 0xc837c5f5, 3 | BRF_GRA },           //  7

	{ "pg1.bin",	0x2000, 0x155c8f7f, 4 | BRF_GRA },           //  8 Sprites
	{ "pg2.bin",	0x2000, 0xa6ca4dfc, 4 | BRF_GRA },           //  9
	{ "pg3.bin",	0x2000, 0x2b21c2fb, 4 | BRF_GRA },           // 10

	{ "35.bin",	0x0020, 0x8c5ca730, 5 | BRF_GRA },           // 11 Color proms
	{ "48.bin",	0x0020, 0x12dd62cd, 5 | BRF_GRA },           // 12
	{ "57.bin",	0x0020, 0x18455a79, 5 | BRF_GRA },           // 13

	{ "pg4.bin",	0x1000, 0xb1cc2fe8, 6 | BRF_PRG | BRF_ESS }, // 14 Z80 #1 code
};

STD_ROM_PICK(progress)
STD_ROM_FN(progress)

struct BurnDriver BurnDrvProgress = {
	"progress", NULL, NULL, NULL, "1984",
	"Progress\0", NULL, "Chuo Co. Ltd", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 1, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, progressRomInfo, progressRomName, NULL, NULL, ProgressInputInfo, ProgressDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};
