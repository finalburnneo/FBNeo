// FB Neo Dragon Master driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "pic16c5x_intf.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvPicROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvMidRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvRowScroll;
static UINT8 *DrvVidRegs;

static UINT32  *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 priority_control;
static UINT8 coin_lockout;
static UINT8 pic16c5x_port0;
static UINT8 oki_control;
static UINT8 snd_command;
static UINT8 snd_flag;
static UINT8 oki_bank0;
static UINT8 oki_bank1;
static UINT8 oki_command;

static UINT8 DrvReset;
static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[3];

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy3 + 8,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy3 + 9,	"p1 fire 6"	},
	{"P1 Button 7",		BIT_DIGITAL,	DrvJoy3 + 10,	"p1 fire 7"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy3 + 12,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	DrvJoy3 + 13,	"p2 fire 6"	},
	{"P2 Button 7",		BIT_DIGITAL,	DrvJoy3 + 14,	"p2 fire 7"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x1b, 0xff, 0xff, 0xe7, NULL					},
	{0x1c, 0xff, 0xff, 0xbc, NULL					},

	{0   , 0xfe, 0   ,    7, "Coinage"				},
	{0x1b, 0x01, 0x07, 0x00, "4 Coins 1 Credits "	},
	{0x1b, 0x01, 0x07, 0x01, "3 Coins 1 Credits "	},
	{0x1b, 0x01, 0x07, 0x02, "2 Coins 1 Credits "	},
	{0x1b, 0x01, 0x07, 0x07, "1 Coin 1 Credits "	},
	{0x1b, 0x01, 0x07, 0x06, "1 Coin 2 Credits "	},
	{0x1b, 0x01, 0x07, 0x05, "1 Coin 3 Credits "	},
	{0x1b, 0x01, 0x07, 0x04, "1 Coin 4 Credits "	},

	{0   , 0xfe, 0   ,    2, "Continue"				},
	{0x1b, 0x01, 0x08, 0x08, "Off"					},
	{0x1b, 0x01, 0x08, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo_Sounds"			},
	{0x1b, 0x01, 0x10, 0x10, "Off"					},
	{0x1b, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Two credits to start"	},
	{0x1b, 0x01, 0x20, 0x20, "Off"					},
	{0x1b, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x1b, 0x01, 0x40, 0x40, "Off"					},
	{0x1b, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Game Pause"			},
	{0x1b, 0x01, 0x80, 0x80, "Off"					},
	{0x1b, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x1c, 0x01, 0x07, 0x07, "Easiest"				},
	{0x1c, 0x01, 0x07, 0x06, "Easier"				},
	{0x1c, 0x01, 0x07, 0x05, "Easy"					},
	{0x1c, 0x01, 0x07, 0x04, "Normal"				},
	{0x1c, 0x01, 0x07, 0x03, "Medium"				},
	{0x1c, 0x01, 0x07, 0x02, "Hard"					},
	{0x1c, 0x01, 0x07, 0x01, "Harder"				},
	{0x1c, 0x01, 0x07, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Language"				},
	{0x1c, 0x01, 0x08, 0x08, "English"				},
	{0x1c, 0x01, 0x08, 0x00, "Korean"				},

	{0   , 0xfe, 0   ,    2, "Game Time"			},
	{0x1c, 0x01, 0x10, 0x10, "Normal"				},
	{0x1c, 0x01, 0x10, 0x00, "Short"				},

	{0   , 0xfe, 0   ,    2, "Stage Skip"			},
	{0x1c, 0x01, 0x20, 0x20, "Off"					},
	{0x1c, 0x01, 0x20, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Spit Color"			},
	{0x1c, 0x01, 0x40, 0x40, "Grey"					},
	{0x1c, 0x01, 0x40, 0x00, "Red"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x1c, 0x01, 0x80, 0x80, "Off"					},
	{0x1c, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Drv)

static inline void palette_write(INT32 offset)
{
	UINT16 data = *((UINT16*)(DrvPalRAM + offset));	

	INT32 i = (data >> 12) + 5;
	INT32 r = (data >> 8) & 0x0f;
	INT32 g = (data >> 4) & 0x0f;
	INT32 b = (data >> 0) & 0x0f;
	
	r = ((r + r * 16) * i) / 20;
	g = ((g + g * 16) * i) / 20;
	b = ((b + b * 16) * i) / 20;

	if (r > 0xff) r = 0xff; // clamp
	if (g > 0xff) g = 0xff;
	if (b > 0xff) b = 0xff;

	DrvPalette[offset >> 1] = BurnHighCol(r, g, b, 0);
}

static void __fastcall drgnmst_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffc000) == 0x900000) {
		DrvPalRAM[(address & 0x3fff)] = data;
		palette_write(address & 0x3ffe);
		return;
	}

	switch (address)
	{
		case 0x800030:
			coin_lockout = (~data >> 2) & 3;
		return;

		case 0x800181:
			snd_command = data;
			SekRunEnd();
		return;

		case 0x800189:
			snd_flag = 1;
		return;
	}
}

static void __fastcall drgnmst_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffc000) == 0x900000) {
		*((UINT16 *)(DrvPalRAM + (address & 0x3ffe))) = data;
		palette_write(address & 0x3ffe);
		return;
	}

	if (address >= 0x800100 && address <= 0x80011f) {
		*((UINT16*)(DrvVidRegs + (address & 0x1e))) = data;
		return;
	}

	switch (address)
	{
		case 0x800154:
			priority_control = data;
		return;
	}
}

static UINT8 __fastcall drgnmst_read_byte(UINT32 address)
{	
	switch (address)
	{
		case 0x800000:
			return DrvInputs[0] >> 8;

		case 0x800001:
			return DrvInputs[0];

		case 0x800019:
			return DrvInputs[1];

		case 0x80001a:
			return DrvDips[0];

		case 0x80001c:
			return DrvDips[1];

		case 0x800176:
			return DrvInputs[2] >> 8;
	}

	return 0;
}

static UINT16 __fastcall drgnmst_read_word(UINT32 address)
{
	bprintf (0, _T("RW: %5.5x\n"), address);

	return 0;
}

static void set_oki_bank0(INT32 bank)
{
	MSM6295SetBank(0, DrvSndROM0 + 0x20000 + ((bank & 7) * 0x20000), 0x20000, 0x3ffff);
}

static void set_oki_bank1(INT32 bank)
{
	MSM6295SetBank(1, DrvSndROM1 + 0x00000 + ((bank & 7) * 0x40000), 0x00000, 0x3ffff);
}

static UINT8 snd_command_read()
{
	INT32 data = 0;

	switch (oki_control & 0x1f)
	{
		case 0x12: data = MSM6295Read(1) & 0x0f; break;
		case 0x16: data = MSM6295Read(0) & 0x0f; break;
		case 0x0b:
		case 0x0f: data = snd_command; break;
	}

	return data;
}

static void snd_control_write(INT32 data)
{
	INT32 oki_new_bank;
	oki_control = data;

	oki_new_bank = ((pic16c5x_port0 & 0xc) >> 2) | ((oki_control & 0x80) >> 5);
	if (oki_new_bank != oki_bank0) {
		oki_bank0 = oki_new_bank;
		if (oki_bank0) oki_new_bank--;
		set_oki_bank0(oki_new_bank);
	}
	oki_new_bank = ((pic16c5x_port0 & 0x3) >> 0) | ((oki_control & 0x20) >> 3);
	if (oki_new_bank != oki_bank1) {
		oki_bank1 = oki_new_bank;
		set_oki_bank1(oki_new_bank);
	}

	switch (oki_control & 0x1f)
	{
		case 0x15:
			MSM6295Write(0, oki_command);
			break;

		case 0x11:
			MSM6295Write(1, oki_command);
			break;
	}
}

static UINT8 drgnmst_sound_readport(UINT16 port)
{
	switch (port)
	{
		case 0x00:
			return pic16c5x_port0;

		case 0x01:
			return snd_command_read();

		case 0x02:
			if (snd_flag) {
				snd_flag = 0;
				return 0x40;
			}
			return 0;

		case 0x10:
			return 0;
	}

	return 0;
}

static void drgnmst_sound_writeport(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			pic16c5x_port0 = data;
		return;

		case 0x01:
			oki_command = data;
		return;

		case 0x02:
			snd_control_write(data);
		return;
	}
}

static tilemap_callback( bg )
{
	UINT16 *ram = (UINT16*)(DrvBgRAM + offs * 4);
	
	TILE_SET_INFO(0, (ram[0] & 0x1fff) + 0x0800, ram[1], TILE_FLIPYX(ram[1] >> 5));
}

static tilemap_callback( mg )
{
	UINT16 *ram = (UINT16*)(DrvMidRAM + offs * 4);
	
	TILE_SET_INFO(1, (ram[0] & 0x7fff) - 0x2000, ram[1], TILE_FLIPYX(ram[1] >> 5));
}

static tilemap_callback( fg )
{
	UINT16 *ram = (UINT16*)(DrvFgRAM + offs * 4);
	
	TILE_SET_INFO(2, (ram[0] & 0xfff) | ((offs & 0x20) << 10), ram[1], TILE_FLIPYX(ram[1] >> 5));
}

static tilemap_scan( fg )
{
	return ((col & 0x3f) << 5) | (row & 0x1f) | ((row & 0x20) << 6);
}

static tilemap_scan( mg )
{
	return ((col & 0x3f) << 4) | (row & 0x0f) | ((row & 0x30) << 6);
}

static tilemap_scan( bg )
{
	return ((col & 0x3f) << 3) | (row & 0x07) | ((row & 0x38) << 6);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	pic16c5xReset();

	set_oki_bank0(0);
	set_oki_bank1(0);

	MSM6295Reset();
	
	priority_control = 0;
	coin_lockout = 0;
	
	pic16c5x_port0 = 0;
	oki_control = 0;
	snd_command = 0;
	snd_flag = 0;
	oki_command = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x100000;

	DrvPicROM			= Next; Next += 0x000400;

	MSM6295ROM			= Next;
	DrvSndROM0			= Next; Next += 0x120000;
	DrvSndROM1			= Next; Next += 0x200000;

	DrvGfxROM0			= Next; Next += 0x1000000;
	DrvGfxROM1			= Next; Next += 0x400000;
	DrvGfxROM2			= Next; Next += 0x400000;
	DrvGfxROM3			= Next; Next += 0x400000;

	DrvPalette			= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);

	AllRam				= Next;

	Drv68KRAM			= Next; Next += 0x010000;
	DrvPalRAM			= Next; Next += 0x004000;
	DrvSprRAM			= Next; Next += 0x000800;

	DrvBgRAM			= Next; Next += 0x004000;
	DrvMidRAM			= Next; Next += 0x004000;
	DrvFgRAM			= Next; Next += 0x004000;
	DrvRowScroll		= Next; Next += 0x004000;

	DrvVidRegs			= Next; Next += 0x000020;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]   = { 24, 8, 16, 0 };
	INT32 XOffs0[32] = { STEP8(0x800000,1), STEP8(0,1), STEP8(0x800020,1), STEP8(0x20,1) };
	INT32 YOffs0[16] = { STEP16(0,32) };
	INT32 XOffs1[16] = { STEP8(0x2000000,1), STEP8(0,1) };
	INT32 YOffs1[32] = { STEP32(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x800000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x800000);

	GfxDecode(0x10000, 4, 16, 16, Plane, XOffs1 + 0, YOffs0, 0x200, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x200000);

	GfxDecode(0x10000, 4,  8,  8, Plane, XOffs0 + 8, YOffs0, 0x100, tmp, DrvGfxROM1);
	GfxDecode(0x04000, 4, 16, 16, Plane, XOffs0 + 0, YOffs0, 0x200, tmp, DrvGfxROM2);
	GfxDecode(0x01000, 4, 32, 32, Plane, XOffs0 + 0, YOffs1, 0x800, tmp, DrvGfxROM3);

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
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, k++, 2)) return 1;
		memcpy (DrvGfxROM0 + 0x000000, DrvGfxROM1 + 0x000000, 0x100000);
		memcpy (DrvGfxROM0 + 0x400000, DrvGfxROM1 + 0x100000, 0x100000);
		memcpy (DrvGfxROM0 + 0x100000, DrvGfxROM1 + 0x200000, 0x100000);
		memcpy (DrvGfxROM0 + 0x500000, DrvGfxROM1 + 0x300000, 0x100000);
		memcpy (DrvGfxROM0 + 0x200000, DrvGfxROM2 + 0x000000, 0x080000);
		memcpy (DrvGfxROM0 + 0x600000, DrvGfxROM2 + 0x080000, 0x080000);

		if (BurnLoadRom(DrvGfxROM1 + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 2)) return 1;

		if (BurnLoadPicROM(DrvPicROM, k++, 0xb7b)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM1 + 0x000000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x900000, 0x903fff, MAP_ROM); // handler
	SekMapMemory(DrvMidRAM,		0x904000, 0x907fff, MAP_RAM);
	SekMapMemory(DrvBgRAM,		0x908000, 0x90bfff, MAP_RAM);
	SekMapMemory(DrvFgRAM,		0x90c000, 0x90ffff, MAP_RAM);
	SekMapMemory(DrvRowScroll,	0x920000, 0x923fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x930000, 0x9307ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xff0000, 0xffffff, MAP_RAM);
	SekSetWriteByteHandler(0,	drgnmst_write_byte);
	SekSetWriteWordHandler(0,	drgnmst_write_word);
	SekSetReadByteHandler(0,	drgnmst_read_byte);
	SekSetReadWordHandler(0,	drgnmst_read_word);
	SekClose();

	pic16c5xInit(0, 0x16C55, DrvPicROM);
	pic16c5xSetReadPortHandler(drgnmst_sound_readport);
	pic16c5xSetWritePortHandler(drgnmst_sound_writeport);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295Init(1, 1000000 / 132, 0);
	MSM6295SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, bg_map_scan, bg_map_callback, 32, 32, 64, 64);
	GenericTilemapInit(1, mg_map_scan, mg_map_callback, 16, 16, 64, 64);
	GenericTilemapInit(2, fg_map_scan, fg_map_callback,  8,  8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM3, 4, 32, 32, 0x400000, 0x600, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM2, 4, 16, 16, 0x400000, 0x400, 0x1f);
	GenericTilemapSetGfx(2, DrvGfxROM1, 4,  8,  8, 0x400000, 0x200, 0x1f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -64, -16);
	GenericTilemapSetOffsets(1, -64, 0); // line scrolling in generic tilemaps needs this fixed!!
	GenericTilemapSetScrollRows(1, 1024);
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetTransparent(2, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	pic16c5xExit();
	MSM6295Exit();

	BurnFree (AllMem);

	return 0;
}

static void draw_sprites()
{
	UINT16 *source = (UINT16*)DrvSprRAM;

	for (INT32 i = 0; i < 0x800/2; i+=4)
	{
		INT32 incx, incy;
		INT32 xpos   = source[i+0] - 64;
		INT32 ypos   = source[i+1] - 16;
		INT32 number = source[i+2];
		INT32 attr   = source[i+3];
		INT32 flipx  = attr & 0x0020;
		INT32 flipy  = attr & 0x0040;
		INT32 wide  = (attr & 0x0f00) >> 8;
		INT32 high  = (attr & 0xf000) >> 12;
		INT32 color = (attr & 0x001f);

		if ((source[i+3] & 0xff00) == 0xff00) break;

		if (!flipx) { incx = 16; } else { incx = -16; xpos += 16*wide; }
		if (!flipy) { incy = 16; } else { incy = -16; ypos += 16*high; }

		for (INT32 y = 0; y <= high; y++)
		{
			for (INT32 x = 0; x <= wide; x++)
			{
				INT32 sx = (xpos+incx*x);
				INT32 sy = (ypos+incy*y);
				INT32 code = number+x+y*16;

				Draw16x16MaskTile(pTransDraw, code & 0xffff, sx, sy, flipx, flipy, color, 4, 0xf, 0, DrvGfxROM0);
			}
		}
	}
}

static INT32 DrvDraw()
{	
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x4000; i+=2) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear(0xf);

	UINT16 *vreg = (UINT16*)DrvVidRegs;

	GenericTilemapSetScrollX(2, vreg[0x0c/2] - 18);
	GenericTilemapSetScrollY(2, vreg[0x0e/2]);
//	GenericTilemapSetScrollX(1, vreg[0x10/2] - 16);
	GenericTilemapSetScrollY(1, vreg[0x12/2] + 16);
	GenericTilemapSetScrollX(0, vreg[0x14/2] - 18);
	GenericTilemapSetScrollY(0, vreg[0x16/2]);

	{
		UINT16 *rs = (UINT16*)(DrvRowScroll + ((vreg[0x08/2] & 0x30) << 8));

		for (INT32 y = 0; y < 1024; y++) {
			GenericTilemapSetScrollRow(1, y, (vreg[0x10/2] - 16) + rs[y]);
		}
	}
	
	GenericTilemapSetEnable(0, (nBurnLayer >> 0) & 1);
	GenericTilemapSetEnable(1, (nBurnLayer >> 1) & 1);
	GenericTilemapSetEnable(2, (nBurnLayer >> 2) & 1);
	
	switch (priority_control)
	{
		case 0x2451:
		case 0x2d9a:
		case 0x2440:
		case 0x245a:
			GenericTilemapDraw(2, pTransDraw, 0);
			GenericTilemapDraw(1, pTransDraw, 0);
			GenericTilemapDraw(0, pTransDraw, 0);
		break;

		case 0x23c0:
			GenericTilemapDraw(0, pTransDraw, 0);
			GenericTilemapDraw(2, pTransDraw, 0);
			GenericTilemapDraw(1, pTransDraw, 0);
		break;

		case 0x38da:
		case 0x215a:
		case 0x2140:
			GenericTilemapDraw(2, pTransDraw, 0);
			GenericTilemapDraw(0, pTransDraw, 0);
			GenericTilemapDraw(1, pTransDraw, 0);
		break;

		case 0x2d80:
			GenericTilemapDraw(1, pTransDraw, 0);
			GenericTilemapDraw(0, pTransDraw, 0);
			GenericTilemapDraw(2, pTransDraw, 0);
		break;
	}

	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		DrvInputs[1] |= coin_lockout;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 12000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == 240) {
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		CPU_RUN(1, pic16c5x);
	}

	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029697;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		pic16c5xScan(nAction);

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(pic16c5x_port0);
		SCAN_VAR(oki_control);
		SCAN_VAR(snd_command);
		SCAN_VAR(snd_flag);
		SCAN_VAR(oki_bank0);
		SCAN_VAR(oki_bank1);
		SCAN_VAR(oki_command);
		SCAN_VAR(priority_control);
		SCAN_VAR(coin_lockout);
	}
	
	if (nAction & ACB_WRITE) {
		set_oki_bank0(oki_bank0);
		set_oki_bank1(oki_bank1);
	}

	return 0;
}

// Dragon Master (set 1)

static struct BurnRomInfo drgnmstRomDesc[] = {
	{ "dm1000e",		0x080000, 0x29467dac, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "dm1000o",		0x080000, 0xba48e9cf, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "dm1003",			0x200000, 0x0ca10e81, 2 | BRF_GRA },           //  2 Sprites
	{ "dm1005",			0x200000, 0x4c2b1db5, 2 | BRF_GRA },           //  3
	{ "dm1004",			0x080000, 0x1a9ac249, 2 | BRF_GRA },           //  4
	{ "dm1006",			0x080000, 0xc46da6fc, 2 | BRF_GRA },           //  5

	{ "dm1007",			0x100000, 0xd5ad81c4, 3 | BRF_GRA },           //  6 Background Tiles
	{ "dm1008",			0x100000, 0xb8572be3, 3 | BRF_GRA },           //  7

	{ "pic16c55.hex",	0x000b7b, 0xf17011e7, 4 | BRF_PRG | BRF_ESS }, //  8 pic16c55 Hex data

	{ "dm1001",			0x100000, 0x63566f7f, 5 | BRF_SND },           //  9 OKI6295 #0 Samples

	{ "dm1002",			0x200000, 0x0f1a874e, 6 | BRF_SND },           // 10 OKI6295 #1 Samples
};

STD_ROM_PICK(drgnmst)
STD_ROM_FN(drgnmst)

struct BurnDriver BurnDrvDrgnmst = {
	"drgnmst", NULL, NULL, NULL, "1994",
	"Dragon Master (set 1)\0", NULL, "Unico", "Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, drgnmstRomInfo, drgnmstRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 224, 4, 3
};


// Dragon Master (set 2)

static struct BurnRomInfo drgnmst2RomDesc[] = {
	{ "even",			0x080000, 0x63eae56a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "odd",			0x080000, 0x35734a49, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "dm1003",			0x200000, 0x0ca10e81, 2 | BRF_GRA },           //  2 Sprites
	{ "dm1005",			0x200000, 0x4c2b1db5, 2 | BRF_GRA },           //  3
	{ "dm1004",			0x080000, 0x1a9ac249, 2 | BRF_GRA },           //  4
	{ "dm1006",			0x080000, 0xc46da6fc, 2 | BRF_GRA },           //  5

	{ "dm1007",			0x100000, 0xd5ad81c4, 3 | BRF_GRA },           //  6 Background Tiles
	{ "dm1008",			0x100000, 0xb8572be3, 3 | BRF_GRA },           //  7

	{ "pic16c55.hex",	0x000b7b, 0xf17011e7, 4 | BRF_PRG | BRF_ESS }, //  8 pic16c55 Hex data

	{ "dm1001",			0x100000, 0x63566f7f, 5 | BRF_SND },           //  9 OKI6295 #0 Samples

	{ "dm1002",			0x200000, 0x0f1a874e, 6 | BRF_SND },           // 10 OKI6295 #1 Samples
};

STD_ROM_PICK(drgnmst2)
STD_ROM_FN(drgnmst2)

struct BurnDriver BurnDrvDrgnmst2 = {
	"drgnmst2", "drgnmst", NULL, NULL, "1994",
	"Dragon Master (set 2)\0", NULL, "Unico", "Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, drgnmst2RomInfo, drgnmst2RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	384, 224, 4, 3
};
