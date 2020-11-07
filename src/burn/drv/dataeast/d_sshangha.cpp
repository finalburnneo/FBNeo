// FinalBurn Neo Super Shanghai Dragon's Eye driver module
// Based on MAME driver by Bryan McPhail, Charles MacDonald, David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "msm6295.h"
#include "deco16ic.h"
#include "deco146.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvUnkRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvPalRAMFixed;
static UINT8 *DrvSprRAM[3];
static UINT8 *DrvShareRAM;
static UINT8 *DrvBootRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 video_control;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static struct BurnInputInfo SshanghaInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sshangha)

static struct BurnDIPInfo SshanghaDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL							},
	{0x15, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    2, "Coin Mode"					},
	{0x14, 0x01, 0x10, 0x10, "Mode 1"						},
	{0x14, 0x01, 0x10, 0x00, "Mode 2"						},

	{0   , 0xfe, 0   ,    8, "Coin B"						},
	{0x14, 0x01, 0x03, 0x02, "2 Coins 1 Credits (Mode 1)"	},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  1 Credits (Mode 1)"	},
	{0x14, 0x01, 0x03, 0x00, "2 Coins 3 Credits (Mode 1)"	},
	{0x14, 0x01, 0x03, 0x01, "1 Coin  2 Credits (Mode 1)"	},
	{0x14, 0x01, 0x03, 0x00, "4 Coins 1 Credits (Mode 2)"	},
	{0x14, 0x01, 0x03, 0x02, "3 Coins 1 Credits (Mode 2)"	},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  1 Credits (Mode 2)"	},
	{0x14, 0x01, 0x03, 0x01, "1 Coin  4 Credits (Mode 2"	},

	{0   , 0xfe, 0   ,    8, "Coin A"						},
	{0x14, 0x01, 0x0c, 0x08, "2 Coins 1 Credits (Mode 1)"	},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits (Mode 1)"	},
	{0x14, 0x01, 0x0c, 0x00, "2 Coins 3 Credits (Mode 1)"	},
	{0x14, 0x01, 0x0c, 0x04, "1 Coin  2 Credits (Mode 1)"	},
	{0x14, 0x01, 0x0c, 0x00, "4 Coins 1 Credits (Mode 2)"	},
	{0x14, 0x01, 0x0c, 0x08, "3 Coins 1 Credits (Mode 2)"	},
	{0x14, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits (Mode 2)"	},
	{0x14, 0x01, 0x0c, 0x04, "1 Coin  4 Credits (Mode 2)"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x14, 0x01, 0x20, 0x20, "Off"							},
	{0x14, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x14, 0x01, 0x40, 0x40, "Off"							},
	{0x14, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x14, 0x01, 0x80, 0x00, "Off"							},
	{0x14, 0x01, 0x80, 0x80, "On"							},

	{0   , 0xfe, 0   ,    2, "Quest Course"					},
	{0x15, 0x01, 0x01, 0x00, "No"							},
	{0x15, 0x01, 0x01, 0x01, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Paradise (Adult) Course"		},
	{0x15, 0x01, 0x02, 0x00, "No"							},
	{0x15, 0x01, 0x02, 0x02, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Use Mahjong Tiles"			},
	{0x15, 0x01, 0x04, 0x00, "No"							},
	{0x15, 0x01, 0x04, 0x04, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Tile Animation"				},
	{0x15, 0x01, 0x08, 0x00, "No"							},
	{0x15, 0x01, 0x08, 0x08, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Debug Mode"					},
	{0x15, 0x01, 0x20, 0x20, "Off"							},
	{0x15, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x15, 0x01, 0xc0, 0x40, "Easy"							},
	{0x15, 0x01, 0xc0, 0xc0, "Normal"						},
	{0x15, 0x01, 0xc0, 0x80, "Hard"							},
	{0x15, 0x01, 0xc0, 0x00, "Hardest"						},
};

STDDIPINFO(Sshangha)

static void __fastcall sshangha_main_write_word(UINT32 address, UINT16 data)
{
	if (address & 0xc00000) {
		SekWriteWord(address & 0x3fffff, data);
		return;
	}
	
	if ((address & 0x3fc000) == 0x3e0000 || (address & 0x3fc000) == 0x3f4000) {
		deco146_104_prot_ww(0, address, data);
		return;
	}

	switch (address)
	{
		case 0x320000:
		case 0x320001:
			video_control = data;
		return;
	}
}

static void __fastcall sshangha_main_write_byte(UINT32 address, UINT8 data)
{
	if (address & 0xc00000) {
		SekWriteByte(address & 0x3fffff, data);
		return;
	}
	
	if ((address & 0x3fc000) == 0x3e0000 || (address & 0x3fc000) == 0x3f4000) {
		deco146_104_prot_wb(0, address, data);
		return;
	}

	switch (address)
	{
		case 0x320000:
		case 0x320001:
			video_control = data;
		return;
	}
}

static UINT16 __fastcall sshangha_main_read_word(UINT32 address)
{
	if (address & 0xc00000) {
		return SekReadWord(address & 0x3fffff);
	}
	
	if ((address & 0x3fc000) == 0x3e0000 || (address & 0x3fc000) == 0x3f4000) {
		return deco146_104_prot_rw(0, address);
	}

	switch (address)
	{
		case 0x084050: // bootleg
			return DrvInputs[0];

		case 0x08476a: // bootleg
			return (DrvInputs[1] & ~8) | (deco16_vblank ? 8 : 0);

		case 0x0840ac: // bootleg
			return (DrvDips[0] + (DrvDips[1] * 256));

		case 0x320006:
			return 0; // irq ack?

		case 0x350000:
		case 0x370000:
			return 0xffff;
	}
	
	return 0;
}

static UINT8 __fastcall sshangha_main_read_byte(UINT32 address)
{
	if ((address & 0x3fc000) == 0x3e0000 || (address & 0x3fc000) == 0x3f4000) {
		return deco146_104_prot_rb(0, address);
	}

	return SekReadWord(address & ~1) >> ((~address & 1) * 8);
}

static void __fastcall sshangha_write_palette_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff000) == 0x380000) {
		*((UINT16*)(DrvPalRAM + (address & 0xffe))) = BURN_ENDIAN_SWAP_INT16(data);

		INT32 offset = address & 0x3fe;
		switch (address & 0xc00)
		{
			case 0x000: offset |= 0x400; break;
			case 0x400: offset |= 0xc00; break;
			case 0x800: offset |= 0x000; break;
			case 0xc00: offset |= 0x800; break;
		}

		*((UINT16*)(DrvPalRAMFixed + offset)) = BURN_ENDIAN_SWAP_INT16(data);
	}
}

static void __fastcall sshangha_write_palette_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff000) == 0x380000) {
		DrvPalRAM[(address & 0xfff) ^ 1] = data;

		INT32 offset = address & 0x3ff;
		switch (address & 0xc00)
		{
			case 0x000: offset |= 0x400; break;
			case 0x400: offset |= 0xc00; break;
			case 0x800: offset |= 0x000; break;
			case 0xc00: offset |= 0x800; break;
		}

		DrvPalRAMFixed[offset ^ 1] = data;
	}
}


static void __fastcall sshangha_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0xf800) {
		*((UINT16*)(DrvShareRAM + (address & 7) * 2)) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (address >= 0xf800) {
		DrvZ80RAM[address & 0x7ff] = data;
		return;
	}

	switch (address)
	{
		case 0xc000:
		case 0xc001:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0xc200:
		case 0xc201:
			MSM6295Write(0, data & 0xff);
		return;
	}
}

static UINT8 __fastcall sshangha_sound_read(UINT16 address)
{
	if ((address & 0xfff8) == 0xf800) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvShareRAM + (address & 7) * 2))) & 0xff;
	}

	if (address >= 0xf800) {
		return DrvZ80RAM[address & 0x7ff];
	}

	switch (address)
	{
		case 0xc000:
		case 0xc001:
			return BurnYM2203Read(0, address & 1);

		case 0xc200:
		case 0xc201:
			return MSM6295Read(0);
	}

	return 0;
}

static UINT16 inputs_read()
{
	return DrvInputs[0];
}

static UINT16 system_read()
{
	return (DrvInputs[1] & ~8) | (deco16_vblank ? 8 : 0);
}

static UINT16 dips_read()
{
	return DrvDips[0] + (DrvDips[1] * 256);
}

static INT32 sshangha_bank_callback(const INT32 bank)
{
	return (bank >> 4) * 0x1000;
}

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	deco_146_104_reset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM2203Reset();
	MSM6295Reset();
	ZetClose();

	video_control = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x040000;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM[0]	= Next; Next += 0x400000;
	DrvGfxROM[1]	= Next; Next += 0x400000;
	DrvGfxROM[2]	= Next; Next += 0x400000;

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x040000;

	DrvPalette		= (UINT32*)Next; Next += 0x401 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x008000;
	DrvZ80RAM		= Next; Next += 0x000800;
	DrvUnkRAM		= Next; Next += 0x001800;
	DrvSprRAM[0]	= Next; Next += 0x000800;
	DrvSprRAM[1]	= Next; Next += 0x000800;
	DrvSprRAM[2]	= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x004000;
	DrvPalRAMFixed	= Next; Next += 0x001000;
	DrvShareRAM		= Next; Next += 0x000400;
	DrvBootRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM    + 0x000001,  k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x000000,  k++, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x000000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x100000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x000000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x100000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x000000,  k++, 1)) return 1;

		k += 2; // don't bother loading sprite roms again

		if (BurnLoadRom(DrvSndROM    + 0x000000,  k++, 1)) return 1;

		deco16_tile_decode(DrvGfxROM[1], DrvGfxROM[0], 0x200000, 1);
		deco16_tile_decode(DrvGfxROM[1], DrvGfxROM[1], 0x200000, 0);
		deco16_tile_decode(DrvGfxROM[2], DrvGfxROM[2], 0x200000, 0);
	}

	deco16Init(1, 0, 1);
	deco16_set_graphics(DrvGfxROM[0], 0x200000 * 2, DrvGfxROM[1], 0x200000 * 2, NULL, 0);
	deco16_set_color_base(0, 0x300);
	deco16_set_color_base(1, 0x200);
	deco16_set_global_offsets(0, 8);
	deco16_set_bank_callback(0, sshangha_bank_callback);
	deco16_set_bank_callback(1, sshangha_bank_callback);

	deco_146_init();
	deco_146_104_set_port_a_cb(inputs_read);
	deco_146_104_set_port_b_cb(system_read);
	deco_146_104_set_port_c_cb(dips_read);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,						0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM,					0x100000, 0x1003ff, MAP_RAM); // 0-f
	SekMapMemory(DrvShareRAM,					0x101000, 0x1013ff, MAP_RAM); // bootleg
	SekMapMemory(deco16_pf_ram[0],				0x200000, 0x201fff, MAP_RAM);
	SekMapMemory(deco16_pf_ram[1],				0x202000, 0x203fff, MAP_RAM);
	SekMapMemory(deco16_pf_rowscroll[0],		0x204000, 0x2047ff, MAP_RAM);
	SekMapMemory(deco16_pf_rowscroll[1],		0x206000, 0x2067ff, MAP_RAM);
	SekMapMemory(DrvUnkRAM,						0x206800, 0x207fff, MAP_RAM);
	SekMapMemory((UINT8*)deco16_pf_control[0],	0x300000, 0x3003ff, MAP_RAM); // 0-f
	SekMapMemory(DrvPalRAM,						0x380000, 0x383fff, MAP_RAM);
	SekMapHandler(1,							0x380000, 0x380fff, MAP_WRITE);

	if (BurnDrvGetFlags() & BDF_BOOTLEG)
	{
		SekMapMemory(DrvSprRAM[2],				0x340000, 0x340fff, MAP_RAM);
		SekMapMemory(DrvSprRAM[0],				0x3c0000, 0x3c07ff, MAP_RAM);
		SekMapMemory(DrvSprRAM[1],				0x3c0800, 0x3c0fff, MAP_RAM);
		SekMapMemory(Drv68KRAM,					0xfec000, 0xff3fff, MAP_RAM);
		SekMapMemory(DrvBootRAM,				0xff4000, 0xff47ff, MAP_RAM);
	}
	else
	{
		SekMapMemory(DrvSprRAM[1],				0x340000, 0x3407ff, MAP_RAM);
		SekMapMemory(DrvSprRAM[1],				0x340800, 0x340Fff, MAP_RAM);
		SekMapMemory(DrvSprRAM[0],				0x360000, 0x3607ff, MAP_RAM);
		SekMapMemory(DrvSprRAM[0],				0x360800, 0x360Fff, MAP_RAM);
		SekMapMemory(Drv68KRAM,					0x3ec000, 0x3f3fff, MAP_RAM);
		SekMapMemory(Drv68KRAM,					0xfec000, 0xff3fff, MAP_RAM); // mirror
	}

	SekSetWriteWordHandler(0,					sshangha_main_write_word);
	SekSetWriteByteHandler(0,					sshangha_main_write_byte);
	SekSetReadWordHandler(0,					sshangha_main_read_word);
	SekSetReadByteHandler(0,					sshangha_main_read_byte);
	SekSetWriteWordHandler(1,					sshangha_write_palette_word);
	SekSetWriteByteHandler(1,					sshangha_write_palette_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,						0x0000, 0x7fff, MAP_ROM);
	ZetSetWriteHandler(sshangha_sound_write);
	ZetSetReadHandler(sshangha_sound_read);
	ZetClose();

	BurnYM2203Init(1, 4000000, &DrvYM2203IRQHandler, 0);
	BurnTimerAttach(&ZetConfig, 4000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.35, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1023924 / 132, 1);
	MSM6295SetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	// for sprite and background mixing
	BurnBitmapAllocate(1, nScreenWidth, nScreenHeight, 0);
	BurnBitmapAllocate(2, nScreenWidth, nScreenHeight, 0);
	BurnBitmapAllocate(3, nScreenWidth, nScreenHeight, 0);
	BurnBitmapAllocate(4, nScreenWidth, nScreenHeight, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	deco16Exit();

	SekExit();
	ZetExit();
	BurnYM2203Exit();
	MSM6295Exit(0);

	BurnFreeMemIndex();

	return 0;
}

static void draw_sprites(INT32 sprite_chip)
{
	UINT16 *bitmap = BurnBitmapGetBitmap(3 + sprite_chip);
	BurnBitmapFill(3 + sprite_chip, 0);

	INT32 flipscreen = 1;
	UINT16 *spriteram = (UINT16*)DrvSprRAM[sprite_chip];

	for (INT32 offs = 0; offs < 0x400; offs += 4)
	{
		INT32 inc, mult;
		INT32 sprite = BURN_ENDIAN_SWAP_INT16(spriteram[offs + 1]);
		INT32 y = BURN_ENDIAN_SWAP_INT16(spriteram[offs]);
		INT32 flash = y & 0x1000;
		INT32 w = y & 0x0800;

		if (!(flash && (nCurrentFrame & 1)))
		{
			INT32 x = BURN_ENDIAN_SWAP_INT16(spriteram[offs + 2]);
			INT32 color = (x >> 9) & 0x7f;
			INT32 fx = y & 0x2000;
			INT32 fy = y & 0x4000;
			INT32 multi = (1 << ((y & 0x0600) >> 9)) - 1;

			x &= 0x01ff;
			y &= 0x01ff;
			if (x >= 320) x -= 512;
			if (y >= 256) y -= 512;
			y = 240 - y;
			x = 304 - x;

			sprite &= ~multi;

			if (fy)
				inc = -1;
			else
			{
				sprite += multi;
				inc = 1;
			}

			if (flipscreen)
			{
				y = 240 - y;
				x = 304 - x;
				if (fy) fy = 0; else fy = 1;
				if (fx) fx = 0; else fx = 1;
				mult = 16;
			}
			else
				mult = -16;

			INT32 mult2 = multi + 1;

			while (multi >= 0)
			{
				INT32 ypos = (y + mult * multi) - 8;
				Draw16x16MaskTile(bitmap, (sprite - multi * inc) & 0x3fff, x, ypos, fx, fy, color, 4, 0, 0, DrvGfxROM[2]);

				if (w)
				{
					Draw16x16MaskTile(bitmap, ((sprite - multi * inc) - mult2) & 0x3fff, x - 16, ypos, fx, fy, color, 4, 0, 0, DrvGfxROM[2]);
				}

				multi--;
			}
		}
	}
}

static void copy_sprite_bitmap(INT32 sprite_chip, UINT16 priority, UINT16 priority_mask, INT32 colbase, INT32 palmask)
{
	UINT16 *bitmap = BurnBitmapGetBitmap(3 + sprite_chip);

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++)
	{
		INT32 pxl = bitmap[i];
		if (pxl & 0xf)
		{
			if ((pxl & priority_mask) == priority) {
				pTransDraw[i] = (pxl & palmask) + colbase;
			}
		}
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		deco16_palette_recalculate(DrvPalette, DrvPalRAMFixed);
		DrvRecalc = 1;
//	}

//	INT32 flipscreen = deco16_pf_control[0][0] & 0x80;
	INT32 tilemap_8bpp = ~video_control & 0x04;

	deco16_pf12_update();
	draw_sprites(0);
	draw_sprites(1);

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x400;
	}

	if (tilemap_8bpp)
	{
		if (nBurnLayer & 2)
		{
			UINT16 *bitmap0 = BurnBitmapGetBitmap(1);
			UINT16 *bitmap1 = BurnBitmapGetBitmap(2);

			deco16_draw_layer(0, bitmap0, DECO16_LAYER_OPAQUE);
			deco16_draw_layer(1, bitmap1, DECO16_LAYER_OPAQUE);

			// combine to 8bpp
			for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++)
			{
				INT32 pixel = (BURN_ENDIAN_SWAP_INT16(bitmap0[i]) & 0xf) | ((BURN_ENDIAN_SWAP_INT16(bitmap1[i]) & 0xf) << 4);

				if ((pixel & 0xff) != 0xff) pTransDraw[i] = pixel + 0x200; 
			}
		}
	}
	else
	{
		if (nBurnLayer & 1) deco16_draw_layer(1, pTransDraw, 0);
	}

	if (nSpriteEnable & 1) copy_sprite_bitmap(0, 0x000, 0x000,  0x000,  0x0ff);
	if (nSpriteEnable & 2) copy_sprite_bitmap(1, 0x200, 0x200,  0x100,  0x0ff);

	if ((tilemap_8bpp == 0) && (nBurnLayer & 4)) deco16_draw_layer(0, pTransDraw, 0);

	if (nSpriteEnable & 4) copy_sprite_bitmap(1, 0x000, 0x200,  0x100,  0x0ff);

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
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 16000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	
	SekOpen(0);
	ZetOpen(0);

	deco16_vblank = 1;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));

		if (i == 7) deco16_vblank ^= 1;

		if (i == 247) {
			deco16_vblank ^= 1;
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
		}
	}

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029722;
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
		ZetScan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		deco_146_104_scan();

		deco16Scan();

		SCAN_VAR(video_control);
	}

	return 0;
}


// Super Shanghai Dragon's Eye (World)

static struct BurnRomInfo sshanghaRomDesc[] = {
	{ "ss007e.u28",		0x020000, 0x5f275f6e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ss006e.u27",		0x020000, 0x111327fe, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ss008-1.u82",	0x010000, 0xff128b54, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ss001.u8",		0x100000, 0xebeca5b7, 3 | BRF_GRA },           //  3 Characters & Background Tiles
	{ "ss002.u7",		0x100000, 0x67659f29, 3 | BRF_GRA },           //  4

	{ "ss003.u39",		0x100000, 0xfbecde72, 4 | BRF_GRA },           //  5 Background Tiles
	{ "ss004.u37",		0x100000, 0x98b82c5e, 4 | BRF_GRA },           //  6

	{ "ss003.u47",		0x100000, 0xfbecde72, 5 | BRF_GRA },           //  7 Sprites
	{ "ss004.u46",		0x100000, 0x98b82c5e, 5 | BRF_GRA },           //  8

	{ "ss005.u86",		0x040000, 0xc53a82ad, 6 | BRF_SND },           //  9 Samples
};

STD_ROM_PICK(sshangha)
STD_ROM_FN(sshangha)

struct BurnDriver BurnDrvSshangha = {
	"sshangha", NULL, NULL, NULL, "1992",
	"Super Shanghai Dragon's Eye (World)\0", NULL, "Hot-B Co., Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_PUZZLE, 0,
	NULL, sshanghaRomInfo, sshanghaRomName, NULL, NULL, NULL, NULL, SshanghaInputInfo, SshanghaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Super Shanghai Dragon's Eye (Japan)

static struct BurnRomInfo sshanghajRomDesc[] = {
	{ "ss007-1.u28",	0x020000, 0xbc466edf, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ss006-1.u27",	0x020000, 0x872a2a2d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ss008.u82",		0x010000, 0x04dc3647, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ss001.u8",		0x100000, 0xebeca5b7, 3 | BRF_GRA },           //  3 Characters & Background Tiles
	{ "ss002.u7",		0x100000, 0x67659f29, 3 | BRF_GRA },           //  4

	{ "ss003.u39",		0x100000, 0xfbecde72, 4 | BRF_GRA },           //  5 Background Tiles
	{ "ss004.u37",		0x100000, 0x98b82c5e, 4 | BRF_GRA },           //  6

	{ "ss003.u47",		0x100000, 0xfbecde72, 5 | BRF_GRA },           //  7 Sprites
	{ "ss004.u46",		0x100000, 0x98b82c5e, 5 | BRF_GRA },           //  8

	{ "ss005.u86",		0x040000, 0xc53a82ad, 6 | BRF_SND },           //  9 Samples
};

STD_ROM_PICK(sshanghaj)
STD_ROM_FN(sshanghaj)

static INT32 SshanghajInit()
{
	INT32 nRet = DrvInit();

	if (nRet == 0)
	{
		// hack in debug dips
		*((UINT16*)(Drv68KROM + 0x384)) = BURN_ENDIAN_SWAP_INT16(0x4e71);
		*((UINT16*)(Drv68KROM + 0x386)) = BURN_ENDIAN_SWAP_INT16(0x4e71);
		*((UINT16*)(Drv68KROM + 0x388)) = BURN_ENDIAN_SWAP_INT16(0x4e71);
		*((UINT16*)(Drv68KROM + 0x38a)) = BURN_ENDIAN_SWAP_INT16(0x4e71);

		// fix checksum error
		*((UINT16*)(Drv68KROM + 0x428)) = BURN_ENDIAN_SWAP_INT16(0x4e71);
		*((UINT16*)(Drv68KROM + 0x42a)) = BURN_ENDIAN_SWAP_INT16(0x4e71);
	}

	return nRet;
}

struct BurnDriver BurnDrvSshanghaj = {
	"sshanghaj", "sshangha", NULL, NULL, "1992",
	"Super Shanghai Dragon's Eye (Japan)\0", NULL, "Hot-B Co., Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_PUZZLE, 0,
	NULL, sshanghajRomInfo, sshanghajRomName, NULL, NULL, NULL, NULL, SshanghaInputInfo, SshanghaDIPInfo,
	SshanghajInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Super Shanghai Dragon's Eye (Korea)

static struct BurnRomInfo sshanghakRomDesc[] = {
	{ "ss007k.u28",		0x020000, 0x90dbf11c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ss006k.u27",		0x020000, 0x07d94579, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ss008.u82",		0x010000, 0x04dc3647, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ss001.u8",		0x100000, 0xebeca5b7, 3 | BRF_GRA },           //  3 Characters & Background Tiles
	{ "ss002.u7",		0x100000, 0x67659f29, 3 | BRF_GRA },           //  4

	{ "ss003.u39",		0x100000, 0xfbecde72, 4 | BRF_GRA },           //  5 Background Tiles
	{ "ss004.u37",		0x100000, 0x98b82c5e, 4 | BRF_GRA },           //  6

	{ "ss003.u47",		0x100000, 0xfbecde72, 5 | BRF_GRA },           //  7 Sprites
	{ "ss004.u46",		0x100000, 0x98b82c5e, 5 | BRF_GRA },           //  8

	{ "ss005.u86",		0x040000, 0xc53a82ad, 6 | BRF_SND },           //  9 Samples
};

STD_ROM_PICK(sshanghak)
STD_ROM_FN(sshanghak)

struct BurnDriver BurnDrvSshanghak = {
	"sshanghak", "sshangha", NULL, NULL, "1992",
	"Super Shanghai Dragon's Eye (Korea)\0", NULL, "Hot-B Co., Ltd. (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_PUZZLE, 0,
	NULL, sshanghakRomInfo, sshanghakRomName, NULL, NULL, NULL, NULL, SshanghaInputInfo, SshanghaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Super Shanghai Dragon's Eye (World, bootleg)

static struct BurnRomInfo sshanghabRomDesc[] = {
	{ "sshanb_2.010",	0x020000, 0xbc7ed254, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sshanb_1.010",	0x020000, 0x7b049f49, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ss008.u82",		0x010000, 0x04dc3647, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ss001.u8",		0x100000, 0xebeca5b7, 3 | BRF_GRA },           //  3 Characters & Background Tiles
	{ "ss002.u7",		0x100000, 0x67659f29, 3 | BRF_GRA },           //  4

	{ "ss003.u39",		0x100000, 0xfbecde72, 4 | BRF_GRA },           //  5 Background Tiles
	{ "ss004.u37",		0x100000, 0x98b82c5e, 4 | BRF_GRA },           //  6

	{ "ss003.u47",		0x100000, 0xfbecde72, 5 | BRF_GRA },           //  7 Sprites
	{ "ss004.u46",		0x100000, 0x98b82c5e, 5 | BRF_GRA },           //  8

	{ "ss005.u86",		0x040000, 0xc53a82ad, 6 | BRF_SND },           //  9 Samples
};

STD_ROM_PICK(sshanghab)
STD_ROM_FN(sshanghab)

struct BurnDriver BurnDrvSshanghab = {
	"sshanghab", "sshangha", NULL, NULL, "1992",
	"Super Shanghai Dragon's Eye (World, bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_DATAEAST, GBF_PUZZLE, 0,
	NULL, sshanghabRomInfo, sshanghabRomName, NULL, NULL, NULL, NULL, SshanghaInputInfo, SshanghaDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};
