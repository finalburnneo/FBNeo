// FB Alpha Knuckle Joe driver module
// Based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "m6800_intf.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvM6803ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvM6803RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[3];

static UINT8 *soundlatch;
static UINT8 *flipscreen;
static UINT8 *sprite_bank;
static UINT8 *tile_bank;
static UINT16 *scrollx;

static UINT8 m6803_port1_data;
static UINT8 m6803_port2_data;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo KncljoeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Kncljoe)

static struct BurnDIPInfo KncljoeDIPList[]=
{
	{0x11, 0xff, 0xff, 0xff, NULL				},
	{0x12, 0xff, 0xff, 0x7f, NULL				},

	{0   , 0xfe, 0   ,    8, "Coin A"			},
	{0x11, 0x01, 0x07, 0x00, "5 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x04, "4 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x06, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x07, 0x03, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x07, 0x01, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x11, 0x01, 0x18, 0x00, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x18, 0x10, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x18, 0x18, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x18, 0x08, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Infinite Energy (Cheat)"	},
	{0x11, 0x01, 0x20, 0x20, "Off"				},
	{0x11, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Free Play (Not Working)"	},
	{0x11, 0x01, 0x40, 0x40, "Off"				},
	{0x11, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x11, 0x01, 0x80, 0x80, "Off"				},
	{0x11, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x12, 0x01, 0x02, 0x02, "Upright"			},
	{0x12, 0x01, 0x02, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x12, 0x01, 0x04, 0x04, "3"				},
	{0x12, 0x01, 0x04, 0x00, "5"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x12, 0x01, 0x18, 0x18, "10k and every 20k"		},
	{0x12, 0x01, 0x18, 0x10, "20k and every 40k"		},
	{0x12, 0x01, 0x18, 0x08, "30k and every 60k"		},
	{0x12, 0x01, 0x18, 0x00, "40k and every 80k"		},

	{0   , 0xfe, 0   ,    4, "Difficulty?"			},
	{0x12, 0x01, 0x60, 0x60, "Easy"				},
	{0x12, 0x01, 0x60, 0x40, "Medium"			},
	{0x12, 0x01, 0x60, 0x20, "Hard"				},
	{0x12, 0x01, 0x60, 0x00, "Hardest"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x80, 0x80, "Off"				},
	{0x12, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Kncljoe)

static void __fastcall kncljoe_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd000:
			*scrollx = (*scrollx & 0x0100) | ((data & 0xff) << 0);
		return;

		case 0xd001:
			*scrollx = (*scrollx & 0x00ff) | ((data & 0x01) << 8);
		return;

		case 0xd800:
			if (data & 0x80) {
				M6803SetIRQLine(0, CPU_IRQSTATUS_ACK);
			} else {
				*soundlatch = data;
			}
		return;

		case 0xd801: {
			*flipscreen = (data & 0x01) >> 0;

			if (((data & 0x04) >> 2) != *sprite_bank) {
				memset (DrvZ80RAM + 0x0100, 0, 0x180);
			}

			*sprite_bank= (data & 0x04) >> 2;
			*tile_bank  = (data & 0x10) >> 4;
		}
		return;

		case 0xd802:
		case 0xd803:
			SN76496Write(address & 1, data);
		return;
	}
}

static UINT8 __fastcall kncljoe_main_read(UINT16 address)
{
	switch (address)
	{
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

		case 0xd807: // nop
		case 0xd817:
			return 0;
	}

	return 0;
}

static UINT8 kncljoe_sound_read(UINT16 address)
{
	address &= 0x7fff;

	if (address < 0x20) {
		return m6803_internal_registers_r(address & 0x1f);
	}

	if (address < 0x80) {
		return 0;
	}

	if ((address & 0xff80) == 0x80) {
		return DrvM6803RAM[address & 0x7f];
	}

	return 0;
}	

static void kncljoe_sound_write(UINT16 address, UINT8 data)
{
	address &= 0x7fff;

	if (address < 0x20) {
		m6803_internal_registers_w(address & 0x1f, data);
		return;
	}

	if (address < 0x80) {
		return;
	}

	if ((address & 0xff80) == 0x80) {
		DrvM6803RAM[address & 0x7f] = data;
		return;
	}

	if ((address & 0x7000) == 0x1000) {
		M6803SetIRQLine(0, CPU_IRQSTATUS_NONE);
	}
}

static void kncljoe_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0x1ff)
	{
		case 0x100:
			m6803_port1_data = data;
		return;

		case 0x101:
			if ((m6803_port2_data & 0x09) == 0x09 && (data & 0x01) == 0x00) {
				AY8910Write(0, (~m6803_port2_data >> 2) & 1, m6803_port1_data);
			}
			m6803_port2_data = data;
		return;
	}
}

static UINT8 kncljoe_sound_read_port(UINT16 port)
{
	switch (port & 0x1ff)
	{
		case 0x100:
			if (m6803_port2_data & 0x08) {
				return AY8910Read(0);
			}
			return 0xff;
	}

	return 0;
}

static UINT8 ay8910_port_A_read(unsigned int)
{
	return *soundlatch;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

//	M6803Open(0);
	M6803Reset();
//	M6803Close();

	AY8910Reset(0);

	m6803_port1_data = 0;
	m6803_port2_data = 0;

	return 0;
}

static INT32 DrvGfxDecode(UINT8 *gfx, INT32 len, INT32 size)
{
	INT32 Planes[3] = { ((len / 3) * 8) * 2, ((len / 3) * 8) * 1, ((len / 3) * 8) * 0 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)malloc(len);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, gfx, len);

	GfxDecode(((len / 3) * 8) / (size * size), 3, size, size, Planes, XOffs, YOffs, size * size, tmp, gfx);

	free (tmp);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[0x10];

	for (INT32 i = 0; i < 0x10; i++)
	{
		INT32 bit0 = 0;
		INT32 bit1 = (DrvColPROM[i + 0x300] >> 6) & 0x01;
		INT32 bit2 = (DrvColPROM[i + 0x300] >> 7) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i + 0x300] >> 3) & 0x01;
		bit1 = (DrvColPROM[i + 0x300] >> 4) & 0x01;
		bit2 = (DrvColPROM[i + 0x300] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i + 0x300] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x300] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x300] >> 2) & 0x01;
		INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		pal[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x80; i++)
	{
		INT32 r = DrvColPROM[i + 0x000] & 0x0f;
		INT32 g = DrvColPROM[i + 0x100] & 0x0f;
		INT32 b = DrvColPROM[i + 0x200] & 0x0f;

		DrvPalette[0x00 + i] = BurnHighCol((r << 4) | r,  (g << 4) | g, (b << 4) | b, 0);

		DrvPalette[0x80 + i] = pal[DrvColPROM[i + 0x320] & 0x0f];
	}

	DrvRecalc = 1;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000;
	DrvM6803ROM		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x080000;

	DrvColPROM		= Next; Next += 0x000500;

	DrvPalette		= (unsigned int*)Next; Next += 0x0100 * sizeof(INT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x000800;

	DrvM6803RAM		= Next; Next += 0x000080;

	soundlatch		= Next; Next += 0x000001;
	flipscreen		= Next; Next += 0x000001;
	sprite_bank		= Next; Next += 0x000001;
	tile_bank		= Next; Next += 0x000001;

	scrollx			= (UINT16*)Next; Next += 0x000002;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
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
		if (BurnLoadRom(DrvZ80ROM  + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x04000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x08000,  2, 1)) return 1;

		if (BurnLoadRom(DrvM6803ROM + 0x00000, 3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x08000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x18000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x28000, 12, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00300, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00320, 17, 1)) return 1;

		DrvGfxDecode(DrvGfxROM0, 0x0c000,  8);
		DrvGfxDecode(DrvGfxROM1, 0x30000, 16);
		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0xbfff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0xbfff, 2, DrvZ80ROM);
	ZetMapArea(0xc000, 0xcfff, 0, DrvVidRAM);
	ZetMapArea(0xc000, 0xcfff, 1, DrvVidRAM);
	ZetMapArea(0xc000, 0xcfff, 2, DrvVidRAM);
	ZetMapArea(0xe800, 0xefff, 0, DrvSprRAM);
	ZetMapArea(0xe800, 0xefff, 1, DrvSprRAM);
	ZetMapArea(0xe800, 0xefff, 2, DrvSprRAM);
	ZetMapArea(0xf000, 0xffff, 0, DrvZ80RAM);
	ZetMapArea(0xf000, 0xffff, 1, DrvZ80RAM);
	ZetMapArea(0xf000, 0xffff, 2, DrvZ80RAM);
	ZetSetWriteHandler(kncljoe_main_write);
	ZetSetReadHandler(kncljoe_main_read);
	ZetClose();

	M6803Init(1);
//	M6803Open(0);
	M6803MapMemory(DrvM6803ROM,	0x6000, 0x7fff, MAP_ROM);
	M6803MapMemory(DrvM6803ROM,	0xe000, 0xffff, MAP_ROM);
	M6803SetReadHandler(kncljoe_sound_read);
	M6803SetWriteHandler(kncljoe_sound_write);
	M6803SetWritePortHandler(kncljoe_sound_write_port);
	M6803SetReadPortHandler(kncljoe_sound_read_port);
//	M6803Close();

	AY8910Init(0, 894886, nBurnSoundRate, &ay8910_port_A_read, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	SN76489Init(0, 3579545, 1);
	SN76489Init(1, 3579545, 1);
	SN76496SetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6803Exit();
	ZetExit();

	AY8910Exit(0);
	SN76496Exit();

	free (AllMem);
	AllMem = NULL;

	return 0;
}

static void draw_layer()
{
	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) << 3;
		INT32 sy = (offs >> 6) << 3;

		if (sy < 0xc0) {
			sx -= (*scrollx + 8) & 0x1ff;
			if (sx < -7) sx += 512;
		} else {
			sx -= 8;
		}

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = DrvVidRAM[offs * 2 + 1];
		INT32 code  = DrvVidRAM[offs * 2 + 0] | ((attr & 0xc0) << 2) | (*tile_bank << 10);

		INT32 color = attr & 0x0f;
		INT32 flipx = attr & 0x20;
		INT32 flipy = attr & 0x10;

		if (*flipscreen) {
			flipx = !flipx;
			sx = 218 - sx;
		} else {
			flipy = !flipy;
			sy ^= 0xf8;
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
	UINT16 *pDraw = pTransDraw;

	if (*flipscreen == 0) {
		pDraw += 64 * nScreenWidth;
	}
	nScreenHeight -= 64;

	for (INT32 i = 0; i < 4; i++)
	{
		for (INT32 j = 0x7c; j >= 0; j -= 4)
		{
			INT32 offs = ((~i & 1) << 8) | ((~i & 2) << 6) | j;

			INT32 sy    = DrvSprRAM[offs + 0] - 64;
			INT32 attr  = DrvSprRAM[offs + 1];
			INT32 code  = DrvSprRAM[offs + 2] | ((attr & 0x10) << 5) | ((attr & 0x20) << 3) | (*sprite_bank << 10);
			INT32 sx    = DrvSprRAM[offs + 3];

			INT32 flipx =  attr & 0x40;
			INT32 flipy = ~attr & 0x80;
			INT32 color =  attr & 0x0f;

			if (*flipscreen)
			{
				flipx = !flipx;
				flipy = !flipy;
				sx = 240 - sx;
				sy = 240 - sy;
			}

			if (sx >= 248) sx -= 256;
			if (code >= 0x600) code &= 0x5ff;

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pDraw, code, sx - 8, sy, color, 3, 0, 0x80, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pDraw, code, sx - 8, sy, color, 3, 0, 0x80, DrvGfxROM1);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pDraw, code, sx - 8, sy, color, 3, 0, 0x80, DrvGfxROM1);
				} else {
					Render16x16Tile_Mask_Clip(pDraw, code, sx - 8, sy, color, 3, 0, 0x80, DrvGfxROM1);
				}
			}
		}
	}

	nScreenHeight += 64;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_layer();

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 66; // for sound nmi
	INT32 nCyclesTotal[2] = { 6000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetOpen(0);
//	M6803Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment;

		nSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == 60) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);

		nSegment = nCyclesTotal[1] / nInterleave;
		nCyclesDone[1] += M6803Run(nSegment);
		M6803SetIRQLine(M6803_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
	}

//	M6803Close();
	ZetClose();

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
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
		M6803Scan(nAction);

		AY8910Scan(nAction, pnMin);
		SN76496Scan(nAction, pnMin);

		SCAN_VAR(m6803_port1_data);
		SCAN_VAR(m6803_port2_data);
	}

	return 0;
}


// Knuckle Joe (set 1)

static struct BurnRomInfo kncljoeRomDesc[] = {
	{ "kj-1.bin",		0x4000, 0x4e4f5ff2, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "kj-2.bin",		0x4000, 0xcb11514b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kj-3.bin",		0x4000, 0x0f50697b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "kj-13.bin",		0x2000, 0x0a0be3f5, 2 | BRF_PRG | BRF_ESS }, //  3 M6803 Code

	{ "kj-10.bin",		0x4000, 0x74d3ba33, 3 | BRF_GRA           }, //  4 Tiles
	{ "kj-11.bin",		0x4000, 0x8ea01455, 3 | BRF_GRA           }, //  5
	{ "kj-12.bin",		0x4000, 0x33367c41, 3 | BRF_GRA           }, //  6

	{ "kj-4.bin",		0x8000, 0xa499ea10, 4 | BRF_GRA           }, //  7 Sprites
	{ "kj-6.bin",		0x8000, 0x815f5c0a, 4 | BRF_GRA           }, //  8
	{ "kj-5.bin",		0x8000, 0x11111759, 4 | BRF_GRA           }, //  9
	{ "kj-7.bin",		0x4000, 0x121fcccb, 4 | BRF_GRA           }, // 10
	{ "kj-9.bin",		0x4000, 0xaffbe3eb, 4 | BRF_GRA           }, // 11
	{ "kj-8.bin",		0x4000, 0xe057e72a, 4 | BRF_GRA           }, // 12

	{ "kjclr1.bin",		0x0100, 0xc3378ac2, 5 | BRF_GRA           }, // 13 Color PROMs
	{ "kjclr2.bin",		0x0100, 0x2126da97, 5 | BRF_GRA           }, // 14
	{ "kjclr3.bin",		0x0100, 0xfde62164, 5 | BRF_GRA           }, // 15
	{ "kjprom5.bin",	0x0020, 0x5a81dd9f, 5 | BRF_GRA           }, // 16
	{ "kjprom4.bin",	0x0100, 0x48dc2066, 5 | BRF_GRA           }, // 17
};

STD_ROM_PICK(kncljoe)
STD_ROM_FN(kncljoe)

struct BurnDriver BurnDrvKncljoe = {
	"kncljoe", NULL, NULL, NULL, "1985",
	"Knuckle Joe (set 1)\0", NULL, "[Seibu Kaihatsu] (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, kncljoeRomInfo, kncljoeRomName, NULL, NULL, KncljoeInputInfo, KncljoeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 256, 3, 4
};


// Knuckle Joe (set 2)

static struct BurnRomInfo kncljoeaRomDesc[] = {
	{ "kj01.bin",		0x4000, 0xf251019e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "kj-2.bin",		0x4000, 0xcb11514b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kj-3.bin",		0x4000, 0x0f50697b, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "kj-13.bin",		0x2000, 0x0a0be3f5, 2 | BRF_PRG | BRF_ESS }, //  3 M6803 Code

	{ "kj-10.bin",		0x4000, 0x74d3ba33, 3 | BRF_GRA           }, //  4 Tiles
	{ "kj-11.bin",		0x4000, 0x8ea01455, 3 | BRF_GRA           }, //  5
	{ "kj-12.bin",		0x4000, 0x33367c41, 3 | BRF_GRA           }, //  6

	{ "kj-4.bin",		0x8000, 0xa499ea10, 4 | BRF_GRA           }, //  7 Sprites
	{ "kj-6.bin",		0x8000, 0x815f5c0a, 4 | BRF_GRA           }, //  8
	{ "kj-5.bin",		0x8000, 0x11111759, 4 | BRF_GRA           }, //  9
	{ "kj-7.bin",		0x4000, 0x121fcccb, 4 | BRF_GRA           }, // 10
	{ "kj-9.bin",		0x4000, 0xaffbe3eb, 4 | BRF_GRA           }, // 11
	{ "kj-8.bin",		0x4000, 0xe057e72a, 4 | BRF_GRA           }, // 12

	{ "kjclr1.bin",		0x0100, 0xc3378ac2, 5 | BRF_GRA           }, // 13 Color PROMs
	{ "kjclr2.bin",		0x0100, 0x2126da97, 5 | BRF_GRA           }, // 14
	{ "kjclr3.bin",		0x0100, 0xfde62164, 5 | BRF_GRA           }, // 15
	{ "kjprom5.bin",	0x0020, 0x5a81dd9f, 5 | BRF_GRA           }, // 16
	{ "kjprom4.bin",	0x0100, 0x48dc2066, 5 | BRF_GRA           }, // 17
};

STD_ROM_PICK(kncljoea)
STD_ROM_FN(kncljoea)

struct BurnDriver BurnDrvKncljoea = {
	"kncljoea", "kncljoe", NULL, NULL, "1985",
	"Knuckle Joe (set 2)\0", NULL, "[Seibu Kaihatsu] (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, kncljoeaRomInfo, kncljoeaRomName, NULL, NULL, KncljoeInputInfo, KncljoeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 256, 3, 4
};


// Bone Crusher

static struct BurnRomInfo bcrusherRomDesc[] = {
	{ "bcrush1.bin",	0x4000, 0xe8979196, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "bcrush2.bin",	0x4000, 0x1be4c731, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bcrush3.bin",	0x4000, 0x0772d993, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "kj-13.bin",		0x2000, 0x0a0be3f5, 2 | BRF_PRG | BRF_ESS }, //  3 M6803 Code

	{ "bcrush10.bin",	0x4000, 0xa62f4572, 3 | BRF_GRA           }, //  4 Tiles
	{ "bcrush11.bin",	0x4000, 0x79cc5644, 3 | BRF_GRA           }, //  5
	{ "bcrush12.bin",	0x4000, 0x8f09641d, 3 | BRF_GRA           }, //  6

	{ "kj-4.bin",		0x8000, 0xa499ea10, 4 | BRF_GRA           }, //  7 Sprites
	{ "kj-6.bin",		0x8000, 0x815f5c0a, 4 | BRF_GRA           }, //  8
	{ "kj-5.bin",		0x8000, 0x11111759, 4 | BRF_GRA           }, //  9
	{ "kj-7.bin",		0x4000, 0x121fcccb, 4 | BRF_GRA           }, // 10
	{ "kj-9.bin",		0x4000, 0xaffbe3eb, 4 | BRF_GRA           }, // 11
	{ "kj-8.bin",		0x4000, 0xe057e72a, 4 | BRF_GRA           }, // 12

	{ "kjclr1.bin",		0x0100, 0xc3378ac2, 5 | BRF_GRA           }, // 13 Color PROMs
	{ "kjclr2.bin",		0x0100, 0x2126da97, 5 | BRF_GRA           }, // 14
	{ "kjclr3.bin",		0x0100, 0xfde62164, 5 | BRF_GRA           }, // 15
	{ "kjprom5.bin",	0x0020, 0x5a81dd9f, 5 | BRF_GRA           }, // 16
	{ "kjprom4.bin",	0x0100, 0x48dc2066, 5 | BRF_GRA           }, // 17
};

STD_ROM_PICK(bcrusher)
STD_ROM_FN(bcrusher)

struct BurnDriver BurnDrvBcrusher = {
	"bcrusher", "kncljoe", NULL, NULL, "1985",
	"Bone Crusher\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, bcrusherRomInfo, bcrusherRomName, NULL, NULL, KncljoeInputInfo, KncljoeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	240, 256, 3, 4
};
