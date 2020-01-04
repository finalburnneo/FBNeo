// FB Alpha Metal Soldier Isaac II driver module
// Based on MAME driver by Jarek Burczynski

#include "tiles_generic.h"
#include "z80_intf.h"
#include "msm5232.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvBgRAM0;
static UINT8 *DrvBgRAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 pending_nmi;
static UINT8 nmi_enable;
static UINT8 sound_control[2];
static UINT8 soundlatch;
static UINT8 mcu_value;
static UINT8 direction;
static UINT8 bg1_textbank;
static UINT8 scrollx[3];
static UINT8 scrolly[3];

static INT32 m_vol_ctrl[16];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo MsisaacInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Msisaac)

static struct BurnDIPInfo MsisaacDIPList[]=
{
	{0x12, 0xff, 0xff, 0x14, NULL				},
	{0x13, 0xff, 0xff, 0x00, NULL				},
	{0x14, 0xff, 0xff, 0x84, NULL				},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x12, 0x01, 0x04, 0x04, "Off"				},
	{0x12, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x12, 0x01, 0x18, 0x00, "1"				},
	{0x12, 0x01, 0x18, 0x08, "2"				},
	{0x12, 0x01, 0x18, 0x10, "3"				},
	{0x12, 0x01, 0x18, 0x18, "4"				},

	{0   , 0xfe, 0   ,    16, "Coin A"			},
	{0x13, 0x01, 0x0f, 0x0f, "9 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x0e, "8 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x0d, "7 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x0c, "6 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x0b, "5 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x0a, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x09, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"		},
	{0x13, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"		},
	{0x13, 0x01, 0x0f, 0x06, "1 Coin  7 Credits"		},
	{0x13, 0x01, 0x0f, 0x07, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    16, "Coin B"			},
	{0x13, 0x01, 0xf0, 0xf0, "9 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0xe0, "8 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0xd0, "7 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0xc0, "6 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0xb0, "5 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0xa0, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0x90, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"		},
	{0x13, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"		},
	{0x13, 0x01, 0xf0, 0x60, "1 Coin  7 Credits"		},
	{0x13, 0x01, 0xf0, 0x70, "1 Coin  8 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x01, 0x01, "Off"				},
	{0x14, 0x01, 0x01, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "DSW3 Unknown 1"		},
	{0x14, 0x01, 0x02, 0x00, "00"				},
	{0x14, 0x01, 0x02, 0x02, "02"				},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"	},
	{0x14, 0x01, 0x04, 0x04, "Off"				},
	{0x14, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "DSW3 Unknown 3"		},
	{0x14, 0x01, 0x08, 0x00, "00"				},
	{0x14, 0x01, 0x08, 0x08, "08"				},

	{0   , 0xfe, 0   ,    4, "Copyright Notice"		},
	{0x14, 0x01, 0x30, 0x00, "(C) 1985 Taito Corporation"	},
	{0x14, 0x01, 0x30, 0x10, "(C) Taito Corporation"	},
	{0x14, 0x01, 0x30, 0x20, "(C) Taito Corp. MCMLXXXV"	},
	{0x14, 0x01, 0x30, 0x30, "(C) Taito Corporation"	},

	{0   , 0xfe, 0   ,    2, "Coinage Display"		},
	{0x14, 0x01, 0x40, 0x40, "Insert Coin"			},
	{0x14, 0x01, 0x40, 0x00, "Coins/Credits"		},

	{0   , 0xfe, 0   ,    2, "Coinage"			},
	{0x14, 0x01, 0x80, 0x80, "A and B"			},
	{0x14, 0x01, 0x80, 0x00, "A only"			},
};

STDDIPINFO(Msisaac)

static void ta7630_init()
{
	double db 	= 0.0;
	double db_step 	= 0.50;
	for (INT32 i = 0; i < 16; i++)
	{
		double max = 100.0 / pow(10.0, db/20.0);
		m_vol_ctrl[15 - i] = (INT32)max;

		db += db_step;
		db_step += 0.275;
	}
}

static void __fastcall msisaac_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf000:
			bg1_textbank = (data >> 3) & 1;
		return;

		case 0xf001: // nop
		case 0xf002: // nop
		case 0xf061: // sound reset?
		case 0xf0a3: // ?
		return;

		case 0xf060:
			soundlatch = data;
			if (nmi_enable) {
				ZetNmi(1);
			} else {
				pending_nmi = 1;
			}
		return;

		case 0xf0c0:
			scrollx[0] = data;
		return;

		case 0xf0c1:
			scrolly[0] = data;
		return;

		case 0xf0c2:
			scrollx[2] = data;
		return;

		case 0xf0c3:
			scrolly[2] = data;
		return;

		case 0xf0c4:
			scrollx[1] = data;
		return;

		case 0xf0c5:
			scrolly[1] = data;
		return;

		case 0xf0e0:
			mcu_value = data;
		return;
	}
}

static UINT8 msisaac_mcu_read()
{
	switch (mcu_value)
	{
		case 0x5f:
			return mcu_value + 0x6b;

		case 0x02:
		{
			static const INT8 table[16] = { -1, 2, 6, -1, 0, 1, 7, 0, 4, 3, 5, 4, -1, 2, 6, -1 };

			INT32 val = table[(DrvInputs[1] >> 2) & 0x0f];

			if (val >= 0) direction = val;

			return direction;
		}

		case 0x07:
			return 0x45;
	}

	return 0;
}

static UINT8 __fastcall msisaac_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xf0e0:
			return msisaac_mcu_read();

		case 0xf0e1:
			return 0x03; // mcu status

		case 0xf080:
			return DrvDips[0];

		case 0xf081:
			return DrvDips[1];

		case 0xf082:
			return DrvDips[2];

		case 0xf083:
			return DrvInputs[0];

		case 0xf084:
			return DrvInputs[1];
	}

	return 0;
}

static void ta7630_set_channel_volume()
{
	double vol = m_vol_ctrl[sound_control[0] & 0xf] / 100.0;
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_0);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_1);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_2);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_3);

	vol = m_vol_ctrl[sound_control[0] >> 4] / 100.0;
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_4);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_5);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_6);
	MSM5232SetRoute(vol, BURN_SND_MSM5232_ROUTE_7);
}

static void __fastcall msisaac_sound_write(UINT16 address, UINT8 data)
{
	if (address >= 0x8010 && address <= 0x801d) {
		MSM5232Write(address & 0x1f, data);
		return;
	}

	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0x8002:
		case 0x8003:
			AY8910Write((address / 2) & 1, address & 1, data);
		return;

		case 0x8020:
			sound_control[0] = data;
			ta7630_set_channel_volume();
		return;

		case 0x8030:
			sound_control[1] = data;
		return;

		case 0xc001:
			nmi_enable = 1;
			if (pending_nmi) {
				ZetNmi();
				pending_nmi = 0;
			}
		return;

		case 0xc002:
			nmi_enable = 0;
		return;

		case 0xc003:
		return;	// nop
	}
}

static UINT8 __fastcall msisaac_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( fg )
{
	TILE_SET_INFO(0, DrvFgRAM[offs], 0x10, 0);
}

static tilemap_callback( bg0 )
{
	TILE_SET_INFO(0, DrvBgRAM0[offs] + 0x500, 0x30, 0);
}

static tilemap_callback( bg1 )
{
	TILE_SET_INFO(0, DrvBgRAM1[offs] + (bg1_textbank * 0x400), 0x20, 0);
}

static INT32 DrvDoReset()
{
	DrvReset = 0;
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1);

	AY8910Reset(0);
	AY8910Reset(1);
	MSM5232Reset();

	pending_nmi = 0;
	nmi_enable = 0;
	memset (sound_control, 0, 2);
	soundlatch = 0;
	mcu_value = 0;
	direction = 0;
	bg1_textbank = 0;
	memset (scrollx, 0, 3);
	memset (scrolly, 0, 3);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x00e000;
	DrvZ80ROM1		= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x020000;
	DrvGfxROM1		= Next; Next += 0x020000;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;
	DrvFgRAM		= Next; Next += 0x000400;
	DrvBgRAM0		= Next; Next += 0x000400;
	DrvBgRAM1		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { STEP4(0,0x4000*8) };
	INT32 XOffs[16] = { STEP8(7,-1), STEP8(64+7,-1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(16*8,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x10000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x10000);

	GfxDecode(0x0800, 4,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);
	GfxDecode(0x0200, 4, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x04000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x0c000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  4, 1)) return 1;

		// mcu rom (not used)

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x04000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x08000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x02000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x06000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0a000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0e000, 13, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0xdfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,			0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0xf100, 0xf1ff, MAP_RAM); // 0-7f
	ZetMapMemory(DrvFgRAM,			0xf400, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM1,			0xf800, 0xfbff, MAP_RAM);
	ZetMapMemory(DrvBgRAM0,			0xfc00, 0xffff, MAP_RAM);
	ZetSetWriteHandler(msisaac_main_write);
	ZetSetReadHandler(msisaac_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x4000, 0x47ff, MAP_RAM);
	ZetSetWriteHandler(msisaac_sound_write);
	ZetSetReadHandler(msisaac_sound_read);
	ZetClose();

	AY8910Init(0, 2000000, 0);
	AY8910Init(1, 2000000, 1);
	AY8910SetAllRoutes(0, 0.10, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.10, BURN_SND_ROUTE_BOTH);

	MSM5232Init(2000000, 1);
	MSM5232SetCapacitors(0.65e-6, 0.65e-6, 0.65e-6, 0.65e-6, 0.65e-6, 0.65e-6, 0.65e-6, 0.65e-6);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_0);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_1);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_2);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_3);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_4);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_5);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_6);
	MSM5232SetRoute(1.00, BURN_SND_MSM5232_ROUTE_7);

	ta7630_init();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback,  8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg0_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, bg1_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x20000, 0, 0x30);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -8);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(2, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	AY8910Exit(0);
	AY8910Exit(1);
	MSM5232Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x800; i+=2)
	{
		UINT8 r = DrvPalRAM[i + 1] & 0xf;
		UINT8 g = DrvPalRAM[i + 0] >> 4;
		UINT8 b = DrvPalRAM[i + 0] & 0xf;

		DrvPalette[i/2] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static void draw_single_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	if (flipy) {
		if (flipx) {
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM1);
		} else {
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM1);
		}
	} else {
		if (flipx) {
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM1);
		} else {
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0, DrvGfxROM1);
		}
	}
}

static void draw_sprites()
{
	for (INT32 i = 128 - 4; i >= 0; i -= 4)
	{
		int attr  = DrvSprRAM[i + 2];
		int sx    = DrvSprRAM[i + 0];
		int sy    = (240 - DrvSprRAM[i + 1] - 1) - 8;
		int code  = DrvSprRAM[i + 3] + ((attr & 0x04) << 6);
		int color = attr >> 4;
		int flipx = attr & 0x01;
		int flipy = attr & 0x02;

		if (attr & 0x08)
		{
			if (flipy) {
				draw_single_sprite(code + 0, color, sx, sy - 16, flipx, flipy);
				draw_single_sprite(code + 1, color, sx, sy - 0,  flipx, flipy);
			} else {
				draw_single_sprite(code + 1, color, sx, sy - 16, flipx, flipy);
				draw_single_sprite(code + 0, color, sx, sy - 0,  flipx, flipy);
			}
		}
		else
		{
			draw_single_sprite(code + 0, color, sx, sy - 0,  flipx, flipy);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	GenericTilemapSetScrollX(0, scrollx[0] + 9);
	GenericTilemapSetScrollY(0, scrolly[0]);
	GenericTilemapSetScrollX(1, scrollx[1] + 9 + 4);
	GenericTilemapSetScrollY(1, scrolly[1]);
	GenericTilemapSetScrollX(2, scrollx[2] + 9 + 2);
	GenericTilemapSetScrollY(2, scrolly[2]);

	GenericTilemapDraw(1, pTransDraw, 0);
	GenericTilemapDraw(2, pTransDraw, 0);

	draw_sprites();

	GenericTilemapDraw(0, pTransDraw, 0);

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
		DrvInputs[0] = 0x08;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2]  = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == 248) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		if (i == 248) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		MSM5232Update(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
		MSM5232Scan(nAction, pnMin);

		SCAN_VAR(pending_nmi);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(sound_control);
		SCAN_VAR(soundlatch);
		SCAN_VAR(mcu_value);
		SCAN_VAR(direction);
		SCAN_VAR(bg1_textbank);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
	}

	if (nAction & ACB_WRITE) {
		ta7630_set_channel_volume();
	}

	return 0;
}


// Metal Soldier Isaac II

static struct BurnRomInfo msisaacRomDesc[] = {
	{ "a34_11.bin",		0x4000, 0x40819334, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a34_12.bin",		0x4000, 0x4c50b298, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a34_13.bin",		0x4000, 0x2e2b09b3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "a34_10.bin",		0x2000, 0xa2c53dc1, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "a34_01.bin",		0x4000, 0x545e45e7, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "a34.mcu",		0x0800, 0, 3 | BRF_NODUMP | BRF_OPT },       //  5 MCU (not dumped)

	{ "a34_02.bin",		0x2000, 0x50da1a81, 4 | BRF_GRA },           //  6 Tiles
	{ "a34_03.bin",		0x2000, 0x728a549e, 4 | BRF_GRA },           //  7
	{ "a34_04.bin",		0x2000, 0xe7d19f1c, 4 | BRF_GRA },           //  8
	{ "a34_05.bin",		0x2000, 0xbed2107d, 4 | BRF_GRA },           //  9

	{ "a34_06.bin",		0x2000, 0x4ec71687, 5 | BRF_GRA },           // 10 Sprites
	{ "a34_07.bin",		0x2000, 0x24922abf, 5 | BRF_GRA },           // 11
	{ "a34_08.bin",		0x2000, 0x3ddbf4c0, 5 | BRF_GRA },           // 12
	{ "a34_09.bin",		0x2000, 0x23eb089d, 5 | BRF_GRA },           // 13
};

STD_ROM_PICK(msisaac)
STD_ROM_FN(msisaac)

struct BurnDriver BurnDrvMsisaac = {
	"msisaac", NULL, NULL, NULL, "1985",
	"Metal Soldier Isaac II\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_TAITO, GBF_SHOOT, 0,
	NULL, msisaacRomInfo, msisaacRomName, NULL, NULL, NULL, NULL, MsisaacInputInfo, MsisaacDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 256, 3, 4
};
