// FinalBurn Neo TCH Kick Goal / Action Hollywood driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "eeprom.h"
#include "pic16c5x_intf.h"
#include "msm6295.h"

#define SIMULATE_PIC

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvPICROM;
static UINT8 *DrvGfxROM[4];
static UINT8 *DrvSndROM;
static UINT8 *DrvEEPROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM[3];
static UINT8 *DrvSprRAM;
static UINT8 *DrvScrollRegs;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvReset;
static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT16 DrvInputs[2];

// emulation
static INT32 soundlatch;
static INT32 soundbank;
static INT32 pic_portb;
static INT32 pic_portc;

#ifdef SIMULATE_PIC
static UINT16 sound_sample[4];
static UINT16 sound_new;
static INT32 has_mcu = 0;
#endif

static INT32 global_x_adjust = 0;
static INT32 global_y_adjust = 0;

static INT32 actionhw_mode = 0;

static struct BurnInputInfo KickgoalInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 12,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 13,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 11,	"diag"		},
};

STDINPUTINFO(Kickgoal)

static void oki_bankswitch(INT32 bank)
{
	soundbank = bank;
	MSM6295SetBank(0, DrvSndROM + bank * 0x20000, 0x20000, 0x3ffff);
}

#ifdef SIMULATE_PIC
static void actionhw_sound_write(INT32 data)
{
	if (has_mcu) return;

	if ((data & 0xfc) == 0xfc) // switch sample banks
	{
		switch (data) {
			case 0xfc: oki_bankswitch(0); break;
			case 0xfd: oki_bankswitch(2); break;
			case 0xfe: oki_bankswitch(1); break;
			case 0xff: oki_bankswitch(3); break;
		}
	}
	else if (data == 0x78) // reset (?)
	{
		MSM6295Write(0, data);
		memset (sound_sample, 0, sizeof(sound_sample));
	}
	else if (sound_new) // Play new sample
	{
		INT32 snddata = MSM6295Read(0) ^ 0xff;

		for (INT32 i = 0; i < 4; i++) {
			if ((data & (0x80 >> i)) && (sound_sample[3 - i] != sound_new)) {
				if (snddata & (8 >> i)) {
					MSM6295Write(0, sound_new);
					MSM6295Write(0, data);
				}
				sound_new = 0;
			}
		}
	}
	else if (data > 0x80) // New sample command
	{
		sound_new = data;
	}
	else // Turn a channel off
	{
		MSM6295Write(0, data);
		if (data & 0x40) sound_sample[3] = 00;
		if (data & 0x20) sound_sample[2] = 00;
		if (data & 0x10) sound_sample[1] = 00;
		if (data & 0x08) sound_sample[0] = 00;
		sound_new = 0;
	}
}
#endif

static void __fastcall kickgoal_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x800004:
			soundlatch = data | 0x200000;
#ifdef SIMULATE_PIC
			actionhw_sound_write(data >> 8);
#endif
		return;

		case 0x900000:
			EEPROMSetCSLine((data & 1) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
		return;

		case 0x900002:
			EEPROMSetClockLine((data & 1) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x900004:
			EEPROMWriteBit(data & 1);
		return;
	}
}

static void __fastcall kickgoal_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x800004:
			soundlatch = data | 0x200000;
#ifdef SIMULATE_PIC
			actionhw_sound_write(data);
#endif
		return;

		case 0x900001:
			EEPROMSetCSLine((data & 1) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
		return;

		case 0x900003:
			EEPROMSetClockLine((data & 1) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x900005:
			EEPROMWriteBit(data & 1);
		return;
	}
}

static UINT16 __fastcall kickgoal_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x800000:
			return DrvInputs[0];

		case 0x800002:
			return DrvInputs[1];

		case 0x900006:
			return EEPROMRead();
	}

	return 0;
}

static UINT8 __fastcall kickgoal_main_read_byte(UINT32 address)
{
	switch (address) {
		case 0x900006: return 0;
		case 0x900007: return EEPROMRead();
	}
	return SekReadWord(address & ~1) >> ((~address & 1) * 8);
}

static UINT8 kickgoal_sound_readport(UINT16 port)
{
	switch (port)
	{
		case 0x01:
			return pic_portb;

		case 0x02:
			return (pic_portc & ~0x20) | (soundlatch >> 16);
	}

	return 0;
}

static void kickgoal_sound_writeport(UINT16 port, UINT8 data)
{
	switch (port)
	{
		case 0x00:
			if (data == 1) oki_bankswitch(3);
			if (data == 2) oki_bankswitch(1);
		return;

		case 0x01:
			pic_portb = data;
		return;

		case 0x02:
		{
			if (!(data & 0x10) && (pic_portc & 0x10))
			{
				soundlatch &= 0xffff;
				pic_portb = soundlatch;
			}

			if (!(data & 0x01) && (pic_portc & 0x01))
			{
				pic_portb = MSM6295Read(0);
			}

			if (!(data & 0x02) && (pic_portc & 0x02))
			{
				MSM6295Write(0, pic_portb);
			}

			pic_portc = data;
		}
		return;
	}
}

static tilemap_scan( _8x8 )
{
	return (row & 0x1f) | ((col & 0x3f) << 5) | ((row & 0x20) << 6);
}

static tilemap_scan( _16x16 )
{
	return (row & 0xf) | ((col & 0x3f) << 4) | ((row & 0x30) << 6);
}

static tilemap_scan( _32x32 )
{
	return (row & 0x7) | ((col & 0x3f) << 3) | ((row & 0x38) << 6);
}

static tilemap_callback( fg )
{
	INT32 code  = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM[0] + offs * 4 + 0)));
	INT32 color = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM[0] + offs * 4 + 2)));

	TILE_SET_INFO((offs & 0x20) ? 4 : 0, code, color, 0);
}

static tilemap_callback ( bg0 )
{
	INT32 code  = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM[1] + offs * 4 + 0)));
	INT32 color = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM[1] + offs * 4 + 2)));

	TILE_SET_INFO(1, code, color, TILE_FLIPYX(color >> 5));
}

static tilemap_callback ( bg1 )
{
	INT32 code  = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM[2] + offs * 4 + 0)));
	INT32 color = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvVidRAM[2] + offs * 4 + 2)));

	TILE_SET_INFO(2, code, color, TILE_FLIPYX(color >> 5));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	pic16c5xReset();

	EEPROMReset();
	if (EEPROMAvailable() == 0) {
		UINT8 *data = BurnMalloc(0x80);
		memset(data, 0, 0x80);
		if (actionhw_mode) {
			// default 1c/1c
			data[0] = data[1] = 0x05;
			data[25] = 0x01;
		}
		EEPROMFill(data, 0, 0x80);
		BurnFree(data);
	}

	MSM6295Reset(0);

	soundlatch = 0;
	soundbank = 0;
	pic_portb = 0;
	pic_portc = 0;
#ifdef SIMULATE_PIC
	memset(sound_sample, 0, sizeof(sound_sample));
	sound_new = 0;
#endif

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x100000;
	DrvPICROM		= Next; Next += 0x010000;

	DrvGfxROM[0]	= Next; Next += 0x400000;
	DrvGfxROM[1]	= Next; Next += 0x800000;
	DrvGfxROM[2]	= Next; Next += 0x800000;
	DrvGfxROM[3]	= Next; Next += 0x400000;

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x080000;

	DrvEEPROM		= Next; Next += 0x000080;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	DrvPalRAM		= Next; Next += 0x000800;
	DrvVidRAM[0]	= Next; Next += 0x004000;
	DrvVidRAM[1]	= Next; Next += 0x004000;
	DrvVidRAM[2]	= Next; Next += 0x008000;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvScrollRegs	= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 length, INT32 type)
{
	INT32 Plane[4]   = { RGN_FRAC(length, 3, 4), RGN_FRAC(length, 2, 4), RGN_FRAC(length, 1, 4), RGN_FRAC(length, 0, 4) };
	INT32 XOffs[32]  = { STEP32(0,1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };
	INT32 YOffs1[16] = { STEP16(0,16) };
	INT32 YOffs2[32] = { STEP32(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(length);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[1], length);

	if (type == 0) // kickgoal
	{
		GfxDecode(0x08000, 4,  8,  8, Plane, XOffs, YOffs1, 0x080, tmp + 0, DrvGfxROM[0]);
		GfxDecode(0x04000, 4, 16, 16, Plane, XOffs, YOffs1, 0x100, tmp + 0, DrvGfxROM[1]);
		GfxDecode(0x01000, 4, 32, 32, Plane, XOffs, YOffs2, 0x400, tmp + 0, DrvGfxROM[2]);
		GfxDecode(0x08000, 4,  8,  8, Plane, XOffs, YOffs1, 0x080, tmp + 1, DrvGfxROM[3]);
	}
	else // actionhw
	{
		GfxDecode(((length * 8)/4)/(8 * 8),   4,  8,  8, Plane, XOffs, YOffs0, 0x040, tmp, DrvGfxROM[0]);
		GfxDecode(((length * 8)/4)/(16 * 16), 4, 16, 16, Plane, XOffs, YOffs1, 0x100, tmp, DrvGfxROM[1]);
	}

	BurnFree (tmp);

	return 0;
}

static void CommonInit(INT32 mcu, INT32 msm_pin, INT32 bg1_size, INT32 x_adjust, INT32 y_adjust)
{
	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvVidRAM[0],		0xa00000, 0xa03fff, MAP_RAM);
	SekMapMemory(DrvVidRAM[1],		0xa04000, 0xa07fff, MAP_RAM);
	SekMapMemory(DrvVidRAM[2],		0xa08000, 0xa0ffff, MAP_RAM);
	SekMapMemory(DrvScrollRegs,		0xa10000, 0xa103ff, MAP_WRITE); // 0-f
	SekMapMemory(DrvSprRAM,			0xb00000, 0xb007ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xc00000, 0xc007ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xff0000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,		kickgoal_main_write_word);
	SekSetWriteByteHandler(0,		kickgoal_main_write_byte);
	SekSetReadWordHandler(0,		kickgoal_main_read_word);
	SekSetReadByteHandler(0,		kickgoal_main_read_byte);
	SekClose();

	EEPROMInit(&eeprom_interface_93C46);

	pic16c5xInit(0, 0x16C57, DrvPICROM);
	pic16c5xSetReadPortHandler(kickgoal_sound_readport);
	pic16c5xSetWritePortHandler(kickgoal_sound_writeport);

	MSM6295Init(0, 1000000 / msm_pin, 0);
	MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, _8x8_map_scan,   fg_map_callback,   8,  8, 64, 64);
	GenericTilemapInit(1, _16x16_map_scan, bg0_map_callback, 16, 16, 64, 64);
	if (bg1_size == 0)
		GenericTilemapInit(2, _16x16_map_scan, bg1_map_callback, 16, 16, 64, 64);
	else
		GenericTilemapInit(2, _32x32_map_scan, bg1_map_callback, 32, 32, 64, 64);
	GenericTilemapSetTransparent(0, 0xf);
	GenericTilemapSetTransparent(1, 0xf);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -x_adjust, -y_adjust);

	has_mcu = mcu;
	global_x_adjust = x_adjust;
	global_y_adjust = y_adjust;
}

static INT32 ActionhwInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM    + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x000000, k++, 2)) return 1;

		BurnLoadRom(DrvPICROM    + 0x000000, k++, 1);


		if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x100000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x180000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x200000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x280000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x300000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x380000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM    + 0x000000, k++, 1)) return 1;

		DrvGfxDecode(0x400000, 1);
	}

	actionhw_mode = 1;
	Drv68KROM[0x3e74] = 1; // rom-hack to enable eeprom saving

	CommonInit(0, MSM6295_PIN7_HIGH, 0, 82, 0);

	GenericTilemapSetGfx(0, DrvGfxROM[0] + 0x380000, 4,  8,  8, 0x040000, 0x000, 0x0f); // fg
	GenericTilemapSetGfx(1, DrvGfxROM[1] + 0x000000, 4, 16, 16, 0x200000, 0x100, 0x0f); // bg0
	GenericTilemapSetGfx(2, DrvGfxROM[1] + 0x200000, 4, 16, 16, 0x200000, 0x200, 0x0f); // bg1
	GenericTilemapSetGfx(3, DrvGfxROM[1] + 0x400000, 4, 16, 16, 0x400000, 0x300, 0x0f); // sprite
	GenericTilemapSetGfx(4, DrvGfxROM[0] + 0x380000, 4,  8,  8, 0x040000, 0x000, 0x0f); // fg (mirror 0)

	DrvDoReset();

	return 0;
}

static INT32 KickgoalCommonInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM    + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvPICROM    + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvEEPROM    + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x100000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x180000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM    + 0x000000, k++, 1)) return 1;

		DrvGfxDecode(0x200000, 0);
	}

	CommonInit(1, MSM6295_PIN7_LOW, 1, 72, 16);

	GenericTilemapSetGfx(0, DrvGfxROM[0] + 0x1c0000, 4,  8,  8, 0x040000, 0x000, 0x0f); // fg - bank0
	GenericTilemapSetGfx(1, DrvGfxROM[1] + 0x100000, 4, 16, 16, 0x100000, 0x100, 0x0f); // bg0
	GenericTilemapSetGfx(2, DrvGfxROM[2] + 0x200000, 4, 32, 32, 0x200000, 0x200, 0x0f); // bg1
	GenericTilemapSetGfx(3, DrvGfxROM[1] + 0x000000, 4, 16, 16, 0x400000, 0x300, 0x0f); // sprite
	GenericTilemapSetGfx(4, DrvGfxROM[3] + 0x1c0000, 4,  8,  8, 0x040000, 0x000, 0x0f); // fg - bank 1

	DrvDoReset();

	return 0;
}

static INT32 KickgoalInit()
{
	INT32 rc = KickgoalCommonInit();
	Drv68KROM[0x12b0] = 1; // rom-hack to enable eeprom saving

	return rc;
}

static INT32 KickgoalaInit()
{
	INT32 rc = KickgoalCommonInit();
	Drv68KROM[0x12bc] = 1; // rom-hack to enable eeprom saving

	return rc;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	EEPROMExit();
	MSM6295Exit(0);
	pic16c5xExit();
	SekExit();

	BurnFreeMemIndex();

	MSM6295ROM = NULL;
	has_mcu = 0;
	actionhw_mode = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x800/2; i++)
	{
		UINT8 r = (BURN_ENDIAN_SWAP_INT16(p[i]) & 0x00f);
		UINT8 g = (BURN_ENDIAN_SWAP_INT16(p[i]) & 0x0f0);
		UINT8 b = (BURN_ENDIAN_SWAP_INT16(p[i]) & 0xf00) >> 8;

		DrvPalette[i] = BurnHighCol(r + (r * 16), g + (g / 16), b + (b * 16), 0);
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0; offs < 0x800 / 2; offs += 4)
	{
		if (BURN_ENDIAN_SWAP_INT16(ram[offs + 0]) & 0x100) break; // end of list marker

		INT32 sy    = BURN_ENDIAN_SWAP_INT16(ram[offs + 0]) & 0x00ff;
		INT32 color = BURN_ENDIAN_SWAP_INT16(ram[offs + 1]) & 0x000f;
		INT32 flipx = BURN_ENDIAN_SWAP_INT16(ram[offs + 1]) & 0x0020;
		INT32 code  = BURN_ENDIAN_SWAP_INT16(ram[offs + 2]) & 0x3fff;
		INT32 sx    = BURN_ENDIAN_SWAP_INT16(ram[offs + 3]);

		DrawGfxMaskTile(0, 3, code, (sx - 16 + 4) - global_x_adjust, (272 - sy) - 32 - global_y_adjust, flipx, 0, color, 0xf);
	}
}

static INT32 DrvDraw()
{	
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	UINT16 *vreg = (UINT16*)DrvScrollRegs;

	BurnTransferClear();

	GenericTilemapSetScrollX(0, BURN_ENDIAN_SWAP_INT16(vreg[0]));
	GenericTilemapSetScrollY(0, BURN_ENDIAN_SWAP_INT16(vreg[1]));
	GenericTilemapSetScrollX(1, BURN_ENDIAN_SWAP_INT16(vreg[2]));
	GenericTilemapSetScrollY(1, BURN_ENDIAN_SWAP_INT16(vreg[3]));
	GenericTilemapSetScrollX(2, BURN_ENDIAN_SWAP_INT16(vreg[4]));
	GenericTilemapSetScrollY(2, BURN_ENDIAN_SWAP_INT16(vreg[5]));

	if (nBurnLayer & 1) GenericTilemapDraw(2, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

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
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 12000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);

		if (i == 240) {
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);

		}

		if (has_mcu) CPU_RUN(1, pic16c5x);
	}

	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
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
		EEPROMScan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(soundbank);
		SCAN_VAR(pic_portb);
		SCAN_VAR(pic_portc);
#ifdef SIMULATE_PIC
		SCAN_VAR(sound_sample);
		SCAN_VAR(sound_new);
#endif
	}
	
	if (nAction & ACB_WRITE) {
		oki_bankswitch(soundbank);
	}

	return 0;
}


// Kick Goal (set 1)

static struct BurnRomInfo kickgoalRomDesc[] = {
	{ "ic6",					0x40000, 0x498ca792, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ic5",					0x40000, 0xd528740a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pic16c57",				0x01007, 0x65dda03d, 2 | BRF_PRG | BRF_ESS }, //  2 PIC Code

	{ "93c46_16bit.ic12",		0x00080, 0x58f512ff, 5 | BRF_PRG | BRF_ESS }, //  3 Default EEPROM

	{ "tch__4.tms27c040.ic33",	0x80000, 0x5038f52a, 3 | BRF_GRA },           //  4 Graphics
	{ "tch__5.tms27c040.ic34",	0x80000, 0x06e7094f, 3 | BRF_GRA },           //  5
	{ "tch__6.tms27c040.ic35",	0x80000, 0xea010563, 3 | BRF_GRA },           //  6
	{ "tch__7.tms27c040.ic36",	0x80000, 0xb6a86860, 3 | BRF_GRA },           //  7

	{ "tch__3.tms27c040.ic13",	0x80000, 0x51272b0b, 4 | BRF_SND },           //  8 Samples
};

STD_ROM_PICK(kickgoal)
STD_ROM_FN(kickgoal)

struct BurnDriver BurnDrvKickgoal = {
	"kickgoal", NULL, NULL, NULL, "1995",
	"Kick Goal (set 1)\0", NULL, "TCH", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, kickgoalRomInfo, kickgoalRomName, NULL, NULL, NULL, NULL, KickgoalInputInfo, NULL,
	KickgoalInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 224, 4, 3
};


// Kick Goal (set 2)

static struct BurnRomInfo kickgoalaRomDesc[] = {
	{ "tch__2.mc27c2001.ic6",	0x40000, 0x3ce2743a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "tch__1.am27c020.ic5",	0x40000, 0xd7d7f83c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pic16c57",				0x01007, 0x65dda03d, 2 | BRF_PRG | BRF_ESS }, //  2 PIC Code

	{ "93c46_16bit.ic12",		0x00080, 0x58f512ff, 5 | BRF_PRG | BRF_ESS }, //  3 Default EEPROM

	{ "tch__4.tms27c040.ic33",	0x80000, 0x5038f52a, 3 | BRF_GRA },           //  4 Graphics
	{ "tch__5.tms27c040.ic34",	0x80000, 0x06e7094f, 3 | BRF_GRA },           //  5
	{ "tch__6.tms27c040.ic35",	0x80000, 0xea010563, 3 | BRF_GRA },           //  6
	{ "tch__7.tms27c040.ic36",	0x80000, 0xb6a86860, 3 | BRF_GRA },           //  7

	{ "tch__3.tms27c040.ic13",	0x80000, 0x51272b0b, 4 | BRF_SND },           //  8 Samples
};

STD_ROM_PICK(kickgoala)
STD_ROM_FN(kickgoala)

struct BurnDriver BurnDrvKickgoala = {
	"kickgoala", "kickgoal", NULL, NULL, "1995",
	"Kick Goal (set 2)\0", NULL, "TCH", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSFOOTBALL, 0,
	NULL, kickgoalaRomInfo, kickgoalaRomName, NULL, NULL, NULL, NULL, KickgoalInputInfo, NULL,
	KickgoalaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 224, 4, 3
};


// Action Hollywood

static struct BurnRomInfo actionhwRomDesc[] = {
	{ "2.ic6",					0x80000, 0x2b71d58c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "1.ic5",					0x80000, 0x136b9711, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pic16c57",				0x00800, 0x00000000, 2 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  2 PIC Code

	{ "4.ic29",					0x80000, 0xdf076744, 3 | BRF_GRA },           //  3 Graphics
	{ "5.ic33",					0x80000, 0x8551fdd4, 3 | BRF_GRA },           //  4
	{ "6.ic30",					0x80000, 0x5cb005a5, 3 | BRF_GRA },           //  5
	{ "7.ic34",					0x80000, 0xc2f7d284, 3 | BRF_GRA },           //  6
	{ "8.ic31",					0x80000, 0x50dffa47, 3 | BRF_GRA },           //  7
	{ "9.ic35",					0x80000, 0xc1ea0370, 3 | BRF_GRA },           //  8
	{ "10.ic32",				0x80000, 0x5ee5db3e, 3 | BRF_GRA },           //  9
	{ "11.ic36",				0x80000, 0x8d376b1e, 3 | BRF_GRA },           // 10

	{ "3.ic13",					0x80000, 0xb8f6705d, 4 | BRF_SND },           // 11 Samples
};

STD_ROM_PICK(actionhw)
STD_ROM_FN(actionhw)

struct BurnDriver BurnDrvActionhw = {
	"actionhw", NULL, NULL, NULL, "1995",
	"Action Hollywood\0", NULL, "TCH", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, actionhwRomInfo, actionhwRomName, NULL, NULL, NULL, NULL, KickgoalInputInfo, NULL,
	ActionhwInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	352, 240, 4, 3
};
