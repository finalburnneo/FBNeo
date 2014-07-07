// Fb Alpha Mighty Warrior / Steel Force / Twin Brats driver module
// Based on MAME drivers by Pierpaolo Prazzoli, David Haywood, and stephh

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "eeprom.h"

/*
	To do:
		why do some times get clipped on the left side in steel force?
		is the terrible sound normal?
*/

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvMloRAM;
static UINT8 *DrvMhiRAM;
static UINT8 *DrvVidAttrRAM;
static UINT8 *DrvMhiScrollRAM;
static UINT8 *DrvMloScrollRAM;
static UINT8 *DrvBgScrollRAM;
static UINT8 *DrvUnkRAM0;
static UINT8 *DrvUnkRAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;
static UINT16 *pPrioDraw;

static INT32 sprite_command_switch;
static INT32 nSoundBank[2];

static INT32 game_select;
static INT32 DrvSpriteBpp;
static INT32 vblank;
static INT32 global_x_offset;

static UINT32 bright;

static UINT8 DrvReset;
static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT16 DrvInps[2];

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo MwarrDIPList[]=
{
	{0x13, 0xff, 0xff, 0xbb, NULL			},
	{0x14, 0xff, 0xff, 0xbf, NULL			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x03, 0x03, "Very Easy"		},
	{0x13, 0x01, 0x03, 0x02, "Easy"			},
	{0x13, 0x01, 0x03, 0x01, "Hard"			},
	{0x13, 0x01, 0x03, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Mutant"		},
	{0x13, 0x01, 0x40, 0x40, "Off"			},
	{0x13, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x14, 0x01, 0x07, 0x00, "5 Coin 1 Credit"	},
	{0x14, 0x01, 0x07, 0x01, "4 Coin 1 Credit"	},
	{0x14, 0x01, 0x07, 0x02, "3 Coin 1 Credit"	},
	{0x14, 0x01, 0x07, 0x03, "2 Coin 1 Credit"	},
	{0x14, 0x01, 0x07, 0x07, "1 Coin 1 Credit"	},
	{0x14, 0x01, 0x07, 0x06, "1 Coin 2 Credits"	},
	{0x14, 0x01, 0x07, 0x05, "1 Coin 3 Credits"	},
	{0x14, 0x01, 0x07, 0x04, "1 Coin 4 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x14, 0x01, 0x38, 0x00, "5 Coin 1 Credit"	},
	{0x14, 0x01, 0x38, 0x08, "4 Coin 1 Credit"	},
	{0x14, 0x01, 0x38, 0x10, "3 Coin 1 Credit"	},
	{0x14, 0x01, 0x38, 0x18, "2 Coin 1 Credit"	},
	{0x14, 0x01, 0x38, 0x38, "1 Coin 1 Credit"	},
	{0x14, 0x01, 0x38, 0x30, "1 Coin 2 Credits"	},
	{0x14, 0x01, 0x38, 0x28, "1 Coin 3 Credits"	},
	{0x14, 0x01, 0x38, 0x20, "1 Coin 4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Allow_Continue"	},
	{0x14, 0x01, 0x40, 0x40, "No"			},
	{0x14, 0x01, 0x40, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Mwarr)

static inline void palette_write(INT32 offset)
{
	UINT16 p = *((UINT16*)(DrvPalRAM + offset));	

	UINT16 b = (p >> 10) & 0x1f;
	b = (((b << 3) | (b >> 2)) * bright) / 256;

	UINT16 g = (p >>  5) & 0x1f;
	g = (((g << 3) | (g >> 2)) * bright) / 256;

	UINT16 r = (p >>  0) & 0x1f;
	r = (((r << 3) | (r >> 2)) * bright) / 256;

	DrvPalette[offset / 2] = BurnHighCol(r, g, b, 0);
}

static void set_brightness(UINT16 data)
{
	bright = (data * 256) / 255;

	for (int i = 0; i < 0x1000; i+=2) {
		palette_write(i);
	}
}

static void sprite_buffer_command(INT32 data)
{
	if (sprite_command_switch)
	{
		switch (data & 0x0f)
		{
			case 0x00: // clear sprites
				memset (DrvSprBuf, 0, 0x1000);
				sprite_command_switch = 0; // otherwise no sprites for mwarr
			break;

			case 0x0d: // keep sprites
			break;

			case 0x07: // mighty warrior / twin brats / steel force
			case 0x09: // twin brats
			case 0x0a: // mighty warrior
			case 0x0b: // mighty warrior
			case 0x0e: // mighty warrior
			case 0x0f: // mighty warrior
			default:
				memcpy (DrvSprBuf, DrvSprRAM, 0x1000);
			break;
		}
	}

	sprite_command_switch ^= 1;
}

static void set_sound_bank(INT32 chip, INT32 data)
{
	if (nSoundBank[chip] == (data & 3)) return;

	nSoundBank[chip] = data & 0x03;

	INT32 nBank = (data & 0x03) * 0x20000;

	memcpy (MSM6295ROM + (chip * 0x100000) + 0x20000, ((chip) ? DrvSndROM1 : DrvSndROM0) + nBank, 0x20000);
}

static void __fastcall mwarr_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff000) == 0x104000) {
		DrvPalRAM[(address & 0xfff)^1] = data;
		palette_write(address & 0xffe);
		return;
	}

	if (address >= 0x110020 && address <= 0x11ffff) {
		Drv68KRAM[address & 0xffff] = data;
		return;
	}

	switch (address)
	{
		case 0x110011:
			set_sound_bank(1, data);
		return;

		case 0x180001:
			MSM6295Command(0, data);
		return;

		case 0x190001:
			MSM6295Command(1, data);
		return;
	}

	if (address >= 0x110000 && address <= 0x11ffff) {
		Drv68KRAM[address & 0xffff] = data;
		return;
	}
}

static void __fastcall mwarr_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff000) == 0x104000) {
		*((UINT16 *)(DrvPalRAM + (address & 0xffe))) = data;
		palette_write(address & 0xffe);
		return;
	}

	if (address >= 0x110020 && address <= 0x11ffff) {
		*((UINT16*)(Drv68KRAM + (address & 0xfffe))) = data;
		return;
	}

	switch (address)
	{
		case 0x110010:
			set_sound_bank(1, data);
		break;

		case 0x110014:
			set_brightness(data);
		break;

		case 0x110016:
			sprite_buffer_command(data);
		break;
	}

	if (address >= 0x110000 && address <= 0x11ffff) {
		*((UINT16*)(Drv68KRAM + (address & 0xfffe))) = data;
		return;
	}

}

static UINT8 __fastcall mwarr_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x180001:
			return MSM6295ReadStatus(0);

		case 0x190001:
			return MSM6295ReadStatus(1);
	}

	return 0;
}

static void __fastcall stlforce_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff000) == 0x104000) {
		DrvPalRAM[(address & 0xfff)^1] = data;
		palette_write(address & 0xffe);
		return;
	}
	switch (address)
	{
		case 0x400011:
			EEPROMWrite(data & 0x04, data & 0x02, data & 0x01);
		return;

		case 0x410001:
			MSM6295Command(0, data);
		return;

		case 0x400012:
			set_sound_bank(0, data);
		return;
	}
}

static void __fastcall stlforce_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff000) == 0x104000) {
		*((UINT16 *)(DrvPalRAM + (address & 0xffe))) = data;
		palette_write(address & 0xffe);
		return;
	}

	switch (address)
	{
		case 0x400012:
	//		set_sound_bank(0, data);
		return;

		case 0x40001e:
			sprite_buffer_command(data);
		return;
	}
}

static UINT8 __fastcall stlforce_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
			return DrvInps[0];

		case 0x400001:
			return DrvInps[0] >> 8;

		case 0x400003:
			return (DrvInps[1] & ~0x50) | (EEPROMRead() ? 0x40 : 0) | vblank;

		case 0x410001:
			return MSM6295ReadStatus(0);
	}

	return 0;
}

static int DrvGfxDecode(INT32 sprite_length, INT32 sprite_bpp)
{
	INT32 fract = (sprite_length / sprite_bpp) * 8;

	INT32 Plane0[6]  = { fract * 5, fract * 4, fract * 3, fract * 2, fract * 1, fract * 0 };
	INT32 XOffs0[16] = { STEP8(128+7, -1), STEP8(7, -1) };
	INT32 YOffs0[16] = { STEP16(0, 8) };

	INT32 Plane1[4]  = { STEP4(0,1) };
	INT32 XOffs1[16] = { 12,8,4,0,28,24,20,16,16*32+12,16*32+8,16*32+4,16*32+0,16*32+28,16*32+24,16*32+20,16*32+16 };
	INT32 YOffs1[16] = { STEP16(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(sprite_length);

	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, sprite_length);

	GfxDecode(fract / (16 * 16), sprite_bpp, 16, 16, Plane0 + (6 - sprite_bpp), XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x040000);

	GfxDecode(0x2000, 4,  8,  8, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM3);

	memcpy (tmp, DrvGfxROM4, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM4);

	DrvSpriteBpp = sprite_bpp;

	BurnFree (tmp);

	return 0;
}

static INT32 MemIndex(INT32 mwarr)
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x100000;

	DrvGfxROM0	= Next; Next += (mwarr) ? 0xc00000 : 0x200000;
	DrvGfxROM1	= Next; Next += 0x100000;
	DrvGfxROM2 	= Next; Next += 0x200000;
	DrvGfxROM3 	= Next; Next += 0x200000;
	DrvGfxROM4	= Next; Next += 0x200000;

	MSM6295ROM	= Next; Next += 0x140000;
	DrvSndROM0	= Next; Next += 0x100000;
	DrvSndROM1	= Next; Next += 0x080000;

	DrvPalette	= (UINT32*)Next; Next += 0x0800 * sizeof(INT32);

	pPrioDraw	= (UINT16*)Next; Next += 512 * 256 * sizeof(UINT16);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x018000;
	DrvPalRAM	= Next; Next += 0x001000;
	DrvSprRAM	= Next; Next += 0x001000;
	DrvSprBuf	= Next; Next += 0x001000;

	DrvVidAttrRAM	= Next; Next += 0x000400;
	DrvMhiScrollRAM	= Next; Next += 0x000400;
	DrvMloScrollRAM	= Next; Next += 0x000400;
	DrvBgScrollRAM	= Next; Next += 0x000400;

	DrvTxtRAM	= Next; Next += 0x001000;
	DrvBgRAM	= Next; Next += 0x000800;
	DrvMloRAM	= Next; Next += 0x000800;
	DrvMhiRAM	= Next; Next += 0x000800;

	DrvUnkRAM0	= Next; Next += 0x000800;
	DrvUnkRAM1	= Next; Next += 0x003000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	MSM6295Reset(0);

	if (game_select == 0)
	{
		MSM6295Reset(1);
	}
	else
	{
		EEPROMReset();

		if (!EEPROMAvailable())
		{
			UINT8 eeprom[128];
			BurnLoadRom(eeprom, 11, 1);
			EEPROMFill(eeprom, 0, 128);
		}
	}

	memcpy (MSM6295ROM + 0x000000, DrvSndROM0, 0x20000);
	memcpy (MSM6295ROM + 0x100000, DrvSndROM1, 0x20000);

	nSoundBank[0] = nSoundBank[1] = 0xff;
	set_sound_bank(0, 1);
	set_sound_bank(1, 0);

	sprite_command_switch = 0;

	bright = 0xff; // start off at full brightness

	return 0;
}

static INT32 MwarrInit()
{
	game_select = 0;

	BurnSetRefreshRate(54.0);

	AllMem = NULL;
	MemIndex(1);
	int nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(1);

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,	0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,	1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x180000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x200000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x280000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x300000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x380000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x400000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x480000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x500000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x580000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x600000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x680000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x700000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x780000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x800000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x880000, 19, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000001, 20, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000000, 21, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000001, 22, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 23, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000001, 24, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x000000, 25, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x000001, 26, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x000000, 27, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 28, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 29, 1)) return 1;

		DrvGfxDecode(0x900000, 6);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, SM_ROM);
	SekMapMemory(DrvBgRAM,		0x100000, 0x1007ff, SM_RAM);
	SekMapMemory(DrvMloRAM,		0x100800, 0x100fff, SM_RAM);
	SekMapMemory(DrvMhiRAM,		0x101000, 0x1017ff, SM_RAM);
	SekMapMemory(DrvTxtRAM,		0x101800, 0x1027ff, SM_RAM);
	SekMapMemory(DrvUnkRAM0,	0x102800, 0x102fff, SM_RAM);
	SekMapMemory(DrvBgScrollRAM,	0x103000, 0x1033ff, SM_RAM);
	SekMapMemory(DrvMloScrollRAM,	0x103400, 0x1037ff, SM_RAM);
	SekMapMemory(DrvMhiScrollRAM,	0x103800, 0x103bff, SM_RAM);
	SekMapMemory(DrvVidAttrRAM,	0x103c00, 0x103fff, SM_RAM);
	SekMapMemory(DrvPalRAM,		0x104000, 0x104fff, SM_ROM); // handler
	SekMapMemory(DrvUnkRAM1,	0x105000, 0x107fff, SM_RAM);
	SekMapMemory(DrvSprRAM,		0x108000, 0x108fff, SM_RAM);
	SekMapMemory(Drv68KRAM,		0x110000, 0x1103ff, SM_ROM);
	SekMapMemory(Drv68KRAM + 0x400,	0x110400, 0x11ffff, SM_RAM);
	SekSetWriteByteHandler(0,	mwarr_write_byte);
	SekSetWriteWordHandler(0,	mwarr_write_word);
	SekSetReadByteHandler(0,	mwarr_read_byte);
//	SekSetReadWordHandler(0,	mwarr_read_word);
	SekClose();

	MSM6295Init(0, 937500 / 132, 1);
	MSM6295Init(1, 937500 / 132, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);

	global_x_offset = 8;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 CommonInit(INT32 select)
{
	game_select = select;

	BurnSetRefreshRate(58.0);

	AllMem = NULL;
	MemIndex(0);
	int nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(0);

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  2, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040000,  3, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c0000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM4 + 0x000000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM4 + 0x000001,  7, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001,  9, 2)) return 1;

		memcpy (DrvGfxROM1 + 0x000000, DrvGfxROM2 + 0x080000, 0x040000);
		memcpy (DrvGfxROM3 + 0x000000, DrvGfxROM4 + 0x080000, 0x080000);
		memcpy (DrvGfxROM3 + 0x080000, DrvGfxROM4 + 0x080000, 0x080000);
		memcpy (DrvGfxROM4 + 0x080000, DrvGfxROM4 + 0x000000, 0x080000);
		memcpy (DrvGfxROM2 + 0x080000, DrvGfxROM2 + 0x000000, 0x080000);

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 10, 1)) return 1;

		DrvGfxDecode(0x100000, 4);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, SM_ROM);
	SekMapMemory(DrvBgRAM,		0x100000, 0x1007ff, SM_RAM);
	SekMapMemory(DrvMloRAM,		0x100800, 0x100fff, SM_RAM);
	SekMapMemory(DrvMhiRAM,		0x101000, 0x1017ff, SM_RAM);
	SekMapMemory(DrvTxtRAM,		0x101800, 0x1027ff, SM_RAM);
	SekMapMemory(DrvUnkRAM0,	0x102800, 0x102fff, SM_RAM);
	SekMapMemory(DrvBgScrollRAM,	0x103000, 0x1033ff, SM_RAM);
	SekMapMemory(DrvMloScrollRAM,	0x103400, 0x1037ff, SM_RAM);
	SekMapMemory(DrvMhiScrollRAM,	0x103800, 0x103bff, SM_RAM);
	SekMapMemory(DrvVidAttrRAM,	0x103c00, 0x103fff, SM_RAM);
	SekMapMemory(DrvPalRAM,		0x104000, 0x104fff, SM_ROM); // handler
	SekMapMemory(DrvUnkRAM1,	0x105000, 0x107fff, SM_RAM);
	SekMapMemory(DrvSprRAM,		0x108000, 0x108fff, SM_RAM);
	SekMapMemory(Drv68KRAM,		0x109000, 0x11ffff, SM_RAM);
	SekSetWriteByteHandler(0,	stlforce_write_byte);
	SekSetWriteWordHandler(0,	stlforce_write_word);
	SekSetReadByteHandler(0,	stlforce_read_byte);
//	SekSetReadWordHandler(0,	stlforce_read_word);
	SekClose();

	MSM6295Init(0, 937500 / 132, 0);
	MSM6295SetRoute(0, 0.70, BURN_SND_ROUTE_BOTH); // should be 1.00, way too loud.

	EEPROMInit(&eeprom_interface_93C46);

	global_x_offset = 8;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 StlforceInit()
{
	return CommonInit(1);
}

static INT32 TwinbratInit()
{
	return CommonInit(2);
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	MSM6295Exit(0);
	if (game_select == 0)
		MSM6295Exit(1);
	else
		EEPROMExit();

	free (AllMem);
	AllMem = NULL;

	MSM6295ROM = NULL;

	game_select = 0;
	DrvSpriteBpp = 0;

	return 0;
}

static inline void draw16x16_prio_tile(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 priority)
{
	UINT16 *dst = pTransDraw + (sy * nScreenWidth) + sx;
	UINT16 *pri = pPrioDraw + (sy * nScreenWidth) + sx;

	UINT8 *gfx = DrvGfxROM0 + code * (16*16);

	if (flipx) flipx = 0x0f;

	for (INT32 y = 0; y < 16; y++)
	{
		if ((sy + y) >= 0 && (sy + y) < nScreenHeight)
		{
			for (INT32 x = 0; x < 16; x++)
			{
				if ((sx + x) >= 0 && (sx + x) < nScreenWidth)
				{
					if (gfx[x^flipx])
					{
						if ((priority & (1 << pri[x])) == 0 && pri[x] < 0x80)
						{
							dst[x] = gfx[x^flipx] + color;
							pri[x] |= 0x80;
						}
					}
				}
			}
		}

		dst += nScreenWidth;
		pri += nScreenWidth;
		gfx += 16;
	}
}

static void draw_sprites(INT32 use_priority)
{
	const UINT16 *source = (UINT16*)(DrvSprBuf + 0x1000-8);
	const UINT16 *finish = (UINT16*)(DrvSprBuf);

	INT32 x_offset = global_x_offset;

	if (game_select == 2) x_offset -= 9;
	if (game_select == 1) x_offset += 0;
	if (game_select == 0) x_offset += 9;

	while (source >= finish)
	{
		if (source[0] & 0x0800)
		{
			INT32 y     = 512 - (source[0] & 0x01ff);
			INT32 x     = (source[3] & 0x3ff) - x_offset;
			INT32 color = ((source[1] & 0x000f) << DrvSpriteBpp) + 0x400;
			INT32 flipx =  source[1] & 0x0200;
			INT32 dy    = (source[0] & 0xf000) >> 12;

			INT32 pri_mask = ~((1 << (((source[1] & 0x3c00) >> 10) + 1)) - 1);
			if (use_priority == 0) pri_mask = ~0xffff; // over everything except text layer

			for (INT32 i = 0; i <= dy; i++)
			{
				INT32 yy = y + i * 16;

#if 1
				draw16x16_prio_tile(source[2]+i, color, x,	yy,	flipx, pri_mask);
				draw16x16_prio_tile(source[2]+i, color, x-1024, yy,	flipx, pri_mask);
				draw16x16_prio_tile(source[2]+i, color, x-1024, yy-512, flipx, pri_mask);
				draw16x16_prio_tile(source[2]+i, color, x,	yy-512, flipx, pri_mask);
#else
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, source[2]+i, x,      yy,     color, DrvSpriteBpp, 0, 0x400, DrvGfxROM0);
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, source[2]+i, x-1024, yy,     color, DrvSpriteBpp, 0, 0x400, DrvGfxROM0);
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, source[2]+i, x-1024, yy-512, color, DrvSpriteBpp, 0, 0x400, DrvGfxROM0);
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, source[2]+i, x,      yy-512, color, DrvSpriteBpp, 0, 0x400, DrvGfxROM0);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, source[2]+i, x,      yy,     color, DrvSpriteBpp, 0, 0x400, DrvGfxROM0);
					Render16x16Tile_Mask_Clip(pTransDraw, source[2]+i, x-1024, yy,     color, DrvSpriteBpp, 0, 0x400, DrvGfxROM0);
					Render16x16Tile_Mask_Clip(pTransDraw, source[2]+i, x-1024, yy-512, color, DrvSpriteBpp, 0, 0x400, DrvGfxROM0);
					Render16x16Tile_Mask_Clip(pTransDraw, source[2]+i, x,      yy-512, color, DrvSpriteBpp, 0, 0x400, DrvGfxROM0);
				}
#endif
			}
		}

		source -= 0x04;
	}
}

static void draw_layer(UINT8 *src, UINT8 *gfxbase, UINT8 *scrl, INT32 attand, INT32 gfxoffs, INT32 scrolloff, INT32 priority)
{
	UINT16 *vram = (UINT16*)src;
	UINT16 *attr = (UINT16 *)DrvVidAttrRAM;
	UINT16 *xscroll = (UINT16*)scrl;

	INT32 linescroll = attr[6] & attand;
	INT32 scrolly = attr[scrolloff] & 0xff;

	INT32 x_offset = ((game_select) ? 8 : 19) + global_x_offset;

	for (INT32 sy = 0; sy < nScreenHeight; sy++)
	{
		INT32 yy = (sy + scrolly) & 0xff;

		INT32 scrollx = (((linescroll) ? xscroll[yy] : xscroll[0]) + x_offset) & 0x3ff;

		UINT16 *dst = pTransDraw + sy * nScreenWidth;
		UINT16 *pri = pPrioDraw + sy * nScreenWidth;

		for (INT32 sx = 0; sx < nScreenWidth + 16; sx += 16)
		{
			INT32 sxx = sx - (scrollx & 0xf);

			INT32 offs = (yy / 16) + ((sx + scrollx) & 0x3f0);

			INT32 code  =  vram[offs] & 0x1fff;
			INT32 color =((vram[offs] & 0xe000) >> 9) + gfxoffs;

			UINT8 *gfx = gfxbase + (code * 256) + ((yy & 0x0f) * 16);

			for (INT32 x = 0; x < 16; x++, sxx++)
			{
				if (sxx >= 0 && sxx < nScreenWidth) {
					if (gfx[x]) {
						dst[sxx] = gfx[x] + color;
						pri[sxx] = priority;
					}
				}
			}
		}
	}
}

static void draw_text_layer()
{
	UINT16 *vram = (UINT16*)DrvTxtRAM;
	UINT16 *scroll = (UINT16 *)DrvVidAttrRAM;

	INT32 scrollx = (scroll[0] + (((game_select) ? 8 : 16) + global_x_offset)) & 0x1ff;
	INT32 scrolly = (scroll[4] + 1) & 0xff;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 code = vram[offs] & 0x1fff;

		INT32 sx = (offs & 0x3f) << 3;
		INT32 sy = (offs >> 6) << 3;

		sx -= scrollx;
		if (sx < -7) sx += 512;
		sy -= scrolly;
		if (sy < -7) sy += 256;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 color = vram[offs] >> 13;

		{
			color = (color * 16) + 0x180;
			code *= 8*8;
			UINT8 *gfx = DrvGfxROM1 + code;
			UINT16 *dst = pTransDraw + sy * nScreenWidth + sx;
			UINT16 *pri = pPrioDraw + sy * nScreenWidth + sx;

			for (INT32 y = 0; y < 8; y++)
			{
				if ((sy + y) >= 0 && (sy + y) < nScreenHeight)
				{
					for (INT32 x = 0; x < 8; x++)
					{
						if ((sx+x) >= 0 && (sx + x) < nScreenWidth)
						{
							if (gfx[x]) {
								dst[x] = gfx[x] + color;
								pri[x] = 0x10;
							}
						}
					}
				}

				gfx += 8;
				dst += nScreenWidth;
				pri += nScreenWidth;
			}
		}

	//	Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy - 1, color, 4, 0, 0x180, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x1000; i+=2) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	memset (pPrioDraw, 0, 512 * 256 * sizeof(UINT16));
	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer(DrvBgRAM,  DrvGfxROM4, DrvBgScrollRAM,  0x01, 0x000, 1, 0x01);
	if (nBurnLayer & 2) draw_layer(DrvMloRAM, DrvGfxROM3, DrvMloScrollRAM, 0x04, 0x080, 2, 0x02);
	if (nBurnLayer & 4) draw_layer(DrvMhiRAM, DrvGfxROM2, DrvMhiScrollRAM, 0x10, 0x100, 3, 0x04);
	if (nBurnLayer & 8) draw_text_layer();

	if (nSpriteEnable & 1) draw_sprites((game_select) ? 0 : 1);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 stlforceFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInps, 0xff, 4);

		for (INT32 i = 0; i < 16; i++) {
			DrvInps[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInps[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCycleSegment = 0;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal = (((game_select == 2) ? 14745600 : 15000000) / 58);
	INT32 nCyclesDone = 0;

	SekOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < 256; i++)
	{
		nCycleSegment = nCyclesTotal / nInterleave;
		nCycleSegment = (nCycleSegment * i) - nCyclesDone;
		nCyclesDone += SekRun(nCycleSegment);

		if (i == 240) {
			vblank = 0x10;
		}

		nCycleSegment = nBurnSoundLen / nInterleave;

		if (pBurnSoundOut) {
			MSM6295Render(0, pBurnSoundOut + nSoundBufferPos, nCycleSegment);
			nSoundBufferPos += (nCycleSegment << 1);
		}
	}

	SekSetIRQLine(4, SEK_IRQSTATUS_AUTO);

	nCycleSegment = nBurnSoundLen - (nSoundBufferPos>>1);
	if (pBurnSoundOut && nCycleSegment > 0) {
		MSM6295Render(0, pBurnSoundOut + nSoundBufferPos, nCycleSegment);
	}

	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (Drv68KRAM, 0xff, 4);
		UINT16 *inps = (UINT16*)Drv68KRAM;

		for (INT32 i = 0; i < 16; i++) {
			inps[0] ^= (DrvJoy1[i] & 1) << i;
			inps[1] ^= (DrvJoy2[i] & 1) << i;
		}

		Drv68KRAM[4] = DrvDips[0];
		Drv68KRAM[5] = DrvDips[1];
	}

	INT32 nInterleave = 256;
	INT32 nCycleSegment = 0;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal = 12000000 / 54;
	INT32 nCyclesDone = 0;

	SekOpen(0);

	Drv68KRAM[2] &= ~0x04; // vblank

	for (INT32 i = 0; i < 256; i++)
	{
		nCycleSegment = nCyclesTotal / nInterleave;
		nCycleSegment = (nCycleSegment * i) - nCyclesDone;
		nCyclesDone += SekRun(nCycleSegment);

		if (i == 240) {
			Drv68KRAM[2] |= 0x04; // vblank
		}

		nCycleSegment = nBurnSoundLen / nInterleave;

		if (pBurnSoundOut) {
			memset (pBurnSoundOut + nSoundBufferPos, 0, nCycleSegment * 2 * sizeof(INT16));
			MSM6295Render(0, pBurnSoundOut + nSoundBufferPos, nCycleSegment);
			MSM6295Render(1, pBurnSoundOut + nSoundBufferPos, nCycleSegment);
			nSoundBufferPos += (nCycleSegment << 1);
		}
	}

	SekSetIRQLine(4, SEK_IRQSTATUS_AUTO);

	nCycleSegment = nBurnSoundLen - (nSoundBufferPos>>1);
	if (pBurnSoundOut && nCycleSegment > 0) {
		memset (pBurnSoundOut + nSoundBufferPos, 0, nCycleSegment * 2 * sizeof(INT16));
		MSM6295Render(0, pBurnSoundOut + nSoundBufferPos, nCycleSegment);
		MSM6295Render(1, pBurnSoundOut + nSoundBufferPos, nCycleSegment);
	}

	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(int nAction,int *pnMin)
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

		MSM6295Scan(0, nAction);
		if (game_select == 0) MSM6295Scan(1, nAction);

		SCAN_VAR(sprite_command_switch);
		SCAN_VAR(bright);

		SCAN_VAR(nSoundBank[0]);
		SCAN_VAR(nSoundBank[1]);
	}

	if (nAction & ACB_WRITE) {
		INT32 t = nSoundBank[0];
		nSoundBank[0] = 0xff;
		set_sound_bank(0, t);

		t = nSoundBank[1];
		nSoundBank[1] = 0xff;
		set_sound_bank(1, t);

		DrvRecalc = 1;
	}

	return 0;
}


// Mighty Warriors

static struct BurnRomInfo mwarrRomDesc[] = {
	{ "prg_ev",		0x80000, 0xd1d5e0a6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "prg_od",		0x80000, 0xe5217d91, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "obm-0",		0x80000, 0xb4707ba1, 2 | BRF_GRA },           //  2 Sprites
	{ "obm-6",		0x80000, 0xf9675acc, 2 | BRF_GRA },           //  3
	{ "obm-12",		0x80000, 0x6239c4dd, 2 | BRF_GRA },           //  4
	{ "obm-1",		0x80000, 0x817dcead, 2 | BRF_GRA },           //  5
	{ "obm-7",		0x80000, 0x3a93c499, 2 | BRF_GRA },           //  6
	{ "obm-13",		0x80000, 0xbac42f06, 2 | BRF_GRA },           //  7
	{ "obm-2",		0x80000, 0x68cd29b0, 2 | BRF_GRA },           //  8
	{ "obm-8",		0x80000, 0xf9482638, 2 | BRF_GRA },           //  9
	{ "obm-14",		0x80000, 0x79ed46b8, 2 | BRF_GRA },           // 10
	{ "obm-3",		0x80000, 0x6e924cb8, 2 | BRF_GRA },           // 11
	{ "obm-9",		0x80000, 0xbe1fb64e, 2 | BRF_GRA },           // 12
	{ "obm-15",		0x80000, 0x5e0efb71, 2 | BRF_GRA },           // 13
	{ "obm-4",		0x80000, 0xf34b67bd, 2 | BRF_GRA },           // 14
	{ "obm-10",		0x80000, 0x00c68a23, 2 | BRF_GRA },           // 15
	{ "obm-16",		0x80000, 0xe9516379, 2 | BRF_GRA },           // 16
	{ "obm-5",		0x80000, 0xb2b976f3, 2 | BRF_GRA },           // 17
	{ "obm-11",		0x80000, 0x7bf1e4da, 2 | BRF_GRA },           // 18
	{ "obm-17",		0x80000, 0x47bd56e8, 2 | BRF_GRA },           // 19

	{ "sf4-0",		0x80000, 0x25938b2d, 3 | BRF_GRA },           // 20 Characters
	{ "sf4-1",		0x80000, 0x2269ce5c, 3 | BRF_GRA },           // 21

	{ "sf3-0",		0x80000, 0x86cd162c, 4 | BRF_GRA },           // 22 Background High Tiles
	{ "sf3-1",		0x80000, 0x2e755e54, 4 | BRF_GRA },           // 23

	{ "sf2-0",		0x80000, 0x622a1816, 5 | BRF_GRA },           // 24 Background Low Tiles
	{ "sf2-1",		0x80000, 0x545f89e9, 5 | BRF_GRA },           // 25

	{ "dw-0",		0x80000, 0xb9b18d00, 6 | BRF_GRA },           // 26 Background Tiles
	{ "dw-1",		0x80000, 0x7aea0b12, 6 | BRF_GRA },           // 27

	{ "oki0",		0x40000, 0x005811ce, 7 | BRF_SND },           // 28 M6295 #0 Samples

	{ "oki1",		0x80000, 0xbcde2330, 8 | BRF_SND },           // 29 M6295 #1 Samples
};

STD_ROM_PICK(mwarr)
STD_ROM_FN(mwarr)

struct BurnDriver BurnDrvMwarr = {
	"mwarr", NULL, NULL, NULL, "199?",
	"Mighty Warriors\0", NULL, "Elettronica Video-Games S.R.L.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, mwarrRomInfo, mwarrRomName, NULL, NULL, DrvInputInfo, MwarrDIPInfo,
	MwarrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	366, 240, 4, 3
};


// Steel Force

static struct BurnRomInfo stlforceRomDesc[] = {
	{ "stlforce.105",	0x20000, 0x3ec804ca, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "stlforce.104",	0x20000, 0x69b5f429, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "stlforce.u36",	0x40000, 0x037dfa9f, 2 | BRF_GRA },           //  2 Sprites
	{ "stlforce.u31",	0x40000, 0x305a8eb5, 2 | BRF_GRA },           //  3
	{ "stlforce.u32",	0x40000, 0x760e8601, 2 | BRF_GRA },           //  4
	{ "stlforce.u33",	0x40000, 0x19415cf3, 2 | BRF_GRA },           //  5

	{ "stlforce.u28",	0x80000, 0x6a4b7c98, 3 | BRF_GRA },           //  6 Background Tiles
	{ "stlforce.u27",	0x80000, 0xc42ef365, 3 | BRF_GRA },           //  7
	{ "stlforce.u30",	0x80000, 0xcf19d43a, 3 | BRF_GRA },           //  8
	{ "stlforce.u29",	0x80000, 0x30488f44, 3 | BRF_GRA },           //  9

	{ "stlforce.u1",	0x80000, 0x0a55edf1, 4 | BRF_SND },           // 10 M6295 #0 Samples

	{ "eeprom-stlforce.bin", 0x0080, 0x3fb83951, 5 | BRF_PRG | BRF_ESS }, // 11 Default Eeprom
};

STD_ROM_PICK(stlforce)
STD_ROM_FN(stlforce)

struct BurnDriver BurnDrvStlforce = {
	"stlforce", NULL, NULL, NULL, "1994",
	"Steel Force\0", NULL, "Electronic Devices Italy / Ecogames S.L. Spain", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, stlforceRomInfo, stlforceRomName, NULL, NULL, DrvInputInfo, NULL,
	StlforceInit, DrvExit, stlforceFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	368, 240, 4, 3
};



// Twin Brats (set 1)

static struct BurnRomInfo twinbratRomDesc[] = {
	{ "2.u105",		0x20000, 0x33a9bb82, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "3.u104",		0x20000, 0xb1186a67, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "11.bin",		0x40000, 0x00eecb03, 2 | BRF_GRA },           //  2 Sprites
	{ "10.bin",		0x40000, 0x7556bee9, 2 | BRF_GRA },           //  3
	{ "9.bin",		0x40000, 0x13194d89, 2 | BRF_GRA },           //  4
	{ "8.bin",		0x40000, 0x79f14528, 2 | BRF_GRA },           //  5

	{ "6.bin",		0x80000, 0xaf10ddfd, 3 | BRF_GRA },           //  6 Background Tiles
	{ "7.bin",		0x80000, 0x3696345a, 3 | BRF_GRA },           //  7
	{ "4.bin",		0x80000, 0x1ae8a751, 3 | BRF_GRA },           //  8
	{ "5.bin",		0x80000, 0xcf235eeb, 3 | BRF_GRA },           //  9

	{ "1.bin",		0x80000, 0x76296578, 4 | BRF_SND },           // 10 M6295 #0 Samples

	{ "eeprom-twinbrat.bin", 0x0080, 0x9366263d, 5 | BRF_PRG | BRF_ESS }, // 11 Default Eeprom
};

STD_ROM_PICK(twinbrat)
STD_ROM_FN(twinbrat)

struct BurnDriverD BurnDrvTwinbrat = {
	"twinbrat", NULL, NULL, NULL, "1995",
	"Twin Brats (set 1)\0", NULL, "Elettronica Video-Games S.R.L.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, twinbratRomInfo, twinbratRomName, NULL, NULL, DrvInputInfo, NULL,
	TwinbratInit, DrvExit, stlforceFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};


// Twin Brats (set 2)

static struct BurnRomInfo twinbrtaRomDesc[] = {
	{ "2.bin",		0x20000, 0x5e75f568, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "3.bin",		0x20000, 0x0e3fa9b0, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "11.bin",		0x40000, 0x00eecb03, 2 | BRF_GRA },           //  2 Sprites
	{ "10.bin",		0x40000, 0x7556bee9, 2 | BRF_GRA },           //  3
	{ "9.bin",		0x40000, 0x13194d89, 2 | BRF_GRA },           //  4
	{ "8.bin",		0x40000, 0x79f14528, 2 | BRF_GRA },           //  5

	{ "6.bin",		0x80000, 0xaf10ddfd, 3 | BRF_GRA },           //  6 Background Tiles
	{ "7.bin",		0x80000, 0x3696345a, 3 | BRF_GRA },           //  7
	{ "4.bin",		0x80000, 0x1ae8a751, 3 | BRF_GRA },           //  8
	{ "5.bin",		0x80000, 0xcf235eeb, 3 | BRF_GRA },           //  9

	{ "1.bin",		0x80000, 0x76296578, 4 | BRF_SND },           // 10 M6295 #0 Samples

	{ "eeprom-twinbrat.bin", 0x0080, 0x9366263d, 5 | BRF_PRG | BRF_ESS }, // 11 Default Eeprom
};

STD_ROM_PICK(twinbrta)
STD_ROM_FN(twinbrta)

struct BurnDriverD BurnDrvTwinbrta = {
	"twinbrta", "twinbrat", NULL, NULL, "1995",
	"Twin Brats (set 2)\0", NULL, "Elettronica Video-Games S.R.L.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, twinbrtaRomInfo, twinbrtaRomName, NULL, NULL, DrvInputInfo, NULL,
	TwinbratInit, DrvExit, stlforceFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};
