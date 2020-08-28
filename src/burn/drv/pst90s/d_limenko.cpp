// FinalBurn Neo Limenko Power System 2 hardware driver module
// Based on MAME driver by Pierpaolo Prazzoli, Tomasz Slanina

#include "tiles_generic.h"
#include "e132xs_intf.h"
#include "mcs51.h"
#include "qs1000.h"
#include "msm6295.h"
#include "eeprom.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvBootROM;
static UINT8 *DrvQSROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvMdRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvRegRAM;

static UINT8 DrvRecalc;

static UINT32 *video_regs;

static INT32 audiocpu_data[4];
static INT32 soundlatch;
static INT32 spriteram_bit;
static UINT32 prev_sprites_count;

static UINT32 security_bit_config	= 0x00400000;
static UINT32 eeprom_bit_config		= 0x00800000;
static UINT32 spriteram_bit_config	= 0x80000000;
static INT32 sound_type;
static INT32 graphicsrom_len;
static UINT32 speedhack_address = 0xffffffff;
static UINT32 speedhack_pc = 0;
static INT32 cpu_clock;

void limenko_draw_sprites();

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[1];
static UINT8 DrvReset;
static UINT16 DrvInputs[3];

static struct BurnInputInfo LegendohInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy3 + 10,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 8,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy1 + 15,	"p3 fire 4"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy3 + 11,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy3 + 9,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy2 + 13,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy2 + 14,	"p4 fire 3"	},
	{"P4 Button 4",		BIT_DIGITAL,	DrvJoy2 + 15,	"p4 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 5,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Legendoh)

static struct BurnInputInfo Sb2003InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 5,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Sb2003)

static struct BurnInputInfo SpottyInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 6"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 5,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Spotty)

static struct BurnDIPInfo LegendohDIPList[]=
{
	{0x2b, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    2, "Sound Enable"		},
	{0x2b, 0x01, 0x20, 0x20, "Off"				},
	{0x2b, 0x01, 0x20, 0x00, "On"				},
};

STDDIPINFO(Legendoh)

static struct BurnDIPInfo Sb2003DIPList[]=
{
	{0x17, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    2, "Sound Enable"		},
	{0x17, 0x01, 0x20, 0x20, "Off"				},
	{0x17, 0x01, 0x20, 0x00, "On"				},
};

STDDIPINFO(Sb2003)

static struct BurnDIPInfo SpottyDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x20, NULL				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0e, 0x01, 0x20, 0x00, "Off"				},
	{0x0e, 0x01, 0x20, 0x20, "On"				},
};

STDDIPINFO(Spotty)

static void sprite_buffer()
{
	spriteram_bit ^= 1;
	limenko_draw_sprites();
	prev_sprites_count = (BURN_ENDIAN_SWAP_INT32(video_regs[0]) >> 0) & 0x1ff; // reg0
}

static void limenko_write_long(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x8003e000:
			sprite_buffer();
		return;
	}
}

static void limenko_write_word(UINT32 address, UINT16 data)
{
	switch (address & ~3)
	{
		case 0x8003e000:
			sprite_buffer();
		return;
	}
}

static void limenko_write_byte(UINT32 address, UINT8 data)
{
	switch (address & ~3)
	{
		case 0x8003e000:
			sprite_buffer();
		return;
	}
}

static inline void speedhack_callback(UINT32 address)
{
	if (address == speedhack_address) {
		if (E132XSGetPC(0) == speedhack_pc) {
			E132XSBurnCycles(50);
		}
	}
}

static UINT32 limenko_read_long(UINT32 address)
{
	if (address < 0x200000) {
		speedhack_callback(address);
		UINT32 ret = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvMainRAM + address)));
		return (ret << 16) | (ret >> 16);
	}

	return 0;
}

static UINT16 limenko_read_word(UINT32 address)
{
	if (address < 0x200000) {
		speedhack_callback(address);
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvMainRAM + address)));
	}

	return 0;
}

static UINT8 limenko_read_byte(UINT32 address)
{
	if (address < 0x200000) {
		speedhack_callback(address);
		return DrvMainRAM[address^1];
	}

	return 0;
}

static inline void DrvMCUSync()
{
	INT32 todo = ((double)E132XSTotalCycles() * (24000000/12) / 80000000) - mcs51TotalCycles();

	if (todo > 0) mcs51Run(todo);
}

static void limenko_io_write(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x4000:
			// coin counter / hopper?
		return;

		case 0x4800:
			EEPROMWriteBit((data & 0x40000));
			EEPROMSetCSLine((data & 0x10000) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE );
			EEPROMSetClockLine((data & 0x20000) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE );
		return;

		case 0x5000:
			soundlatch = (data >> 16) & 0xff;
			switch (sound_type)
			{
				case 0: DrvMCUSync(); qs1000_set_irq(1); break;
				case 1: soundlatch |= 0x100; break; // spotty
			}
		return;
	}
}

static UINT32 limenko_io_read(UINT32 address)
{
	switch (address)
	{
		case 0x0000:
			return (DrvInputs[0] << 16) | 0xffff;

		case 0x0800:
			return (DrvInputs[1] << 16) | 0xffff;

		case 0x1000:
		{
			UINT32 ret = (DrvInputs[2] << 16) | 0xffff;

			ret &= security_bit_config ^ 0xffffffff;
			ret &= eeprom_bit_config ^ 0xffffffff;
			ret &= 0x20000000 ^ 0xffffffff;
			ret &= spriteram_bit_config ^ 0xffffffff;

			if (spriteram_bit) ret |= spriteram_bit_config;
			if (EEPROMRead()) ret |= eeprom_bit_config;
			ret |= (DrvDips[0] & 0x20) << 24;

			return ret;
		}
	}

	return 0;
}

static tilemap_callback( bg )
{
	UINT32 attr = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvBgRAM + offs * 4)));
	attr = (attr << 16) | (attr >> 16);

	TILE_SET_INFO(0, attr & 0x7ffff, attr >> 28, 0);
}

static tilemap_callback( md )
{
	UINT32 attr = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvMdRAM + offs * 4)));
	attr = (attr << 16) | (attr >> 16);

	TILE_SET_INFO(0, attr & 0x7ffff, attr >> 28, 0);
}

static tilemap_callback( fg )
{
	UINT32 attr = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvFgRAM + offs * 4)));
	attr = (attr << 16) | (attr >> 16);

	TILE_SET_INFO(0, attr & 0x7ffff, attr >> 28, 0);
}

static void qs1000_p3_write(UINT8 data)
{
	qs1000_set_bankedrom(DrvQSROM + (data & 7) * 0x10000);

	if (~data & 0x20)
		qs1000_set_irq(0);
}

static void spotty_sound_write(INT32 port, UINT8 data)
{
	switch (port)
	{
		case MCS51_PORT_P1:
			audiocpu_data[0] = data;
		return;

		case MCS51_PORT_P3:
		{
			if ((audiocpu_data[3] & 0x01) == 0x01 && (data & 0x81) == 0) audiocpu_data[0] = MSM6295Read(0);
			if ((audiocpu_data[3] & 0x02) == 0x02 && (data & 0x82) == 0) MSM6295Write(0,  audiocpu_data[0]);
			if ((audiocpu_data[3] & 0x08) == 0x08 && (data & 0x08) == 0) { audiocpu_data[0] = soundlatch; soundlatch &= 0xff; }
			audiocpu_data[3] = data;
		}
		return;
	}
}

static UINT8 spotty_sound_read(INT32 port)
{
	switch (port)
	{
		case MCS51_PORT_P1:
			return audiocpu_data[0];

		case MCS51_PORT_P3:
			return (soundlatch & 0x100) ? 0 : 4;
	}

	return 0;
}

static UINT8 qs1000_p1_read()
{
	return soundlatch;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	E132XSOpen(0);
	E132XSReset();
	E132XSClose();

	switch (sound_type)
	{
		case 0: qs1000_reset(); break;

		case 1: // spotty
		{
			mcs51Open(0);
			mcs51_reset();
			mcs51Close();
			MSM6295Reset();
		}
		break;
	}

	EEPROMReset();

	soundlatch = 0;
	spriteram_bit = 1;
	prev_sprites_count = 0;
	memset (audiocpu_data, 0, sizeof(audiocpu_data)); // spotty

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	= Next; Next += 0x400000;
	DrvBootROM	= Next; Next += 0x200000;

	DrvQSROM	= Next; Next += 0x080000;

	DrvGfxROM	= Next; Next += graphicsrom_len;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x400000;

	BurnPalette	= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam		= Next;

	DrvMainRAM	= Next; Next += 0x200000;
	DrvFgRAM	= Next; Next += 0x008000;
	DrvMdRAM	= Next; Next += 0x008000;
	DrvBgRAM	= Next; Next += 0x008000;
	DrvSprRAM	= Next; Next += 0x002000;
	BurnPalRAM	= Next; Next += 0x002000;
	DrvRegRAM	= Next; Next += 0x002000;

	video_regs = (UINT32*)(DrvRegRAM + 0x1fec);

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 LimenkoCommonInit(INT32 cputype, INT32 cpuclock, INT32 (*pLoadRoms)(), INT32 graphics_size, INT32 soundtype)
{
	graphicsrom_len = graphics_size;

	BurnAllocMemIndex();

	memset (DrvMainROM, 0xff, 0x400000);
	memset (DrvQSROM, 0xff, 0x80000);

	if (pLoadRoms) {
		if (pLoadRoms()) return 1;
	}
	
	cpu_clock = cpuclock;

	E132XSInit(0, cputype, cpu_clock);
	E132XSOpen(0);
	E132XSMapMemory(DrvMainRAM,			0x00000000, 0x001fffff, MAP_RAM);
	E132XSMapMemory(DrvMainROM,			0x40000000, 0x403fffff, MAP_ROM);
	E132XSMapMemory(DrvFgRAM,			0x80000000, 0x80007fff, MAP_RAM);
	E132XSMapMemory(DrvMdRAM,			0x80008000, 0x8000ffff, MAP_RAM);
	E132XSMapMemory(DrvBgRAM,			0x80010000, 0x80017fff, MAP_RAM);
	E132XSMapMemory(DrvSprRAM,			0x80018000, 0x80019fff, MAP_RAM);
	E132XSMapMemory(BurnPalRAM,			0x8001c000, 0x8001dfff, MAP_RAM);
	E132XSMapMemory(DrvRegRAM,			0x8001e000, 0x8001ffff, MAP_RAM);
	E132XSMapMemory(DrvBootROM,			0xffe00000, 0xffffffff, MAP_ROM);
	E132XSSetWriteLongHandler(limenko_write_long);
	E132XSSetWriteWordHandler(limenko_write_word);
	E132XSSetWriteByteHandler(limenko_write_byte);
	E132XSSetIOWriteHandler(limenko_io_write);
	E132XSSetIOReadHandler(limenko_io_read);

	if (speedhack_pc)
	{
		E132XSMapMemory(NULL,			speedhack_address & ~0xfff, speedhack_address | 0xfff, MAP_ROM);
		E132XSSetReadLongHandler(limenko_read_long);
		E132XSSetReadWordHandler(limenko_read_word);
		E132XSSetReadByteHandler(limenko_read_byte);
	}
	E132XSClose();

	EEPROMInit(&eeprom_interface_93C46);

	switch(soundtype)
	{
		case 0:
		{
			qs1000_init(DrvQSROM, DrvSndROM, 0x400000);
			qs1000_set_write_handler(3, qs1000_p3_write);
			qs1000_set_read_handler(1, qs1000_p1_read);
			qs1000_set_volume(3.00);

			sound_type = 0;
		}
		break;

		case 1:
		{
			i80c51_init();
			mcs51Open(0);
			mcs51_set_program_data(DrvQSROM); // not really qs!
			mcs51_set_write_handler(spotty_sound_write);
			mcs51_set_read_handler(spotty_sound_read);
			mcs51Close();

			MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, 0);
			MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

			sound_type = 1;
		}
		break;
	}

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 128, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, md_map_callback, 8, 8, 128, 64);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 128, 64);
	GenericTilemapSetGfx(0, DrvGfxROM, 8, 8, 8, graphicsrom_len, 0, 0xf);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
	BurnBitmapAllocate(1, 512, 512, true);

	DrvDoReset();

	return 0;
}

static INT32 LegendohLoadCallback()
{
	INT32 k = 0;
	if (BurnLoadRom(DrvBootROM + 0x0180000, k++, 1)) return 1;

	if (BurnLoadRom(DrvMainROM + 0x0000000, k++, 1)) return 1;
	if (BurnLoadRom(DrvMainROM + 0x0200000, k++, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM  + 0x0000000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0000001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0000002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0000003, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0800000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0800001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0800002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0800003, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x1000000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x1000001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x1000002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x1000003, k++, 4)) return 1;

	if (BurnLoadRom(DrvQSROM   + 0x0000000, k++, 1)) return 1;

	if (BurnLoadRom(DrvSndROM  + 0x0000000, k++, 1)) return 1;
	if (BurnLoadRom(DrvSndROM  + 0x0080000, k++, 1)) return 1;
	if (BurnLoadRom(DrvSndROM  + 0x0200000, k++, 1)) return 1;

	security_bit_config	= ~0 & 0x00400000; // = bit is high

	return 0;
}

static INT32 DynabombLoadCallback()
{
	INT32 k = 0;
	if (BurnLoadRom(DrvBootROM + 0x0000000, k++, 1)) return 1;

	if (BurnLoadRom(DrvMainROM + 0x0000000, k++, 1)) return 1;

	if (BurnLoadRom(DrvQSROM   + 0x0000000, k++, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM  + 0x0000000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0000001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0000002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0000003, k++, 4)) return 1;

	if (BurnLoadRom(DrvSndROM  + 0x0000000, k++, 1)) return 1;
	if (BurnLoadRom(DrvSndROM  + 0x0080000, k++, 1)) return 1;
	if (BurnLoadRom(DrvSndROM  + 0x0200000, k++, 1)) return 1;

	security_bit_config	= 0 & 0x00400000; // = bit is low

	return 0;
}

static INT32 Sb2003LoadCallback()
{
	INT32 k = 0;
	if (BurnLoadRom(DrvBootROM + 0x0000000, k++, 1)) return 1;

	if (BurnLoadRom(DrvQSROM   + 0x0000000, k++, 1)) return 1;

	if (BurnLoadRom(DrvGfxROM  + 0x0000000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0000001, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0000002, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM  + 0x0000003, k++, 4)) return 1;

	if (BurnLoadRom(DrvSndROM  + 0x0000000, k++, 1)) return 1;
	if (BurnLoadRom(DrvSndROM  + 0x0200000, k++, 1)) return 1;

	security_bit_config	= 0 & 0x00400000; // = bit is low

	return 0;
}

static INT32 SpottyLoadCallback()
{
	INT32 k = 0;
	if (BurnLoadRom(DrvBootROM + 0x0180000, k++, 1)) return 1;

	if (BurnLoadRom(DrvQSROM   + 0x0000000, k++, 1)) return 1; // not qs
 
	if (BurnLoadRom(DrvGfxROM + 0x0000000, k++, 4)) return 1;
	if (BurnLoadRom(DrvGfxROM + 0x0000002, k++, 4)) return 1;

	if (BurnLoadRom(DrvSndROM  + 0x0000000, k++, 1)) return 1;

	{
		for (INT32 x = 0; x < 0x200000; x += 2)
		{
			DrvGfxROM[x+1] = (DrvGfxROM[x]&0xf0) >> 4;
			DrvGfxROM[x+0] = (DrvGfxROM[x]&0x0f) >> 0;
		}
	}

	security_bit_config		= 0 & 0x00400000; // = bit is low
	eeprom_bit_config		= 0x00800000;
	spriteram_bit_config	= 0x00080000; // different

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	E132XSExit();
	EEPROMExit();

	switch (sound_type)
	{
		case 0: qs1000_exit(); break;
		case 1: // spotty
		{
			mcs51_exit();
			MSM6295Exit();
		}
		break;
	}

	BurnFreeMemIndex();

	security_bit_config		= 0x00400000;
	eeprom_bit_config		= 0x00800000;
	spriteram_bit_config	= 0x80000000;
	sound_type = 0;

	speedhack_address = 0xffffffff;
	speedhack_pc = 0;

	MSM6295ROM = NULL;

	return 0;
}

static void draw_single_sprite(UINT8 width, UINT8 height, UINT32 code, UINT32 color,bool flipx,bool flipy,int offsx,int offsy, UINT8 transparent_color, UINT8 priority)
{
	UINT16 pal = color << 8;
	UINT8 *source_base = DrvGfxROM + code;

	int xinc = flipx ? -1 : 1;
	int yinc = flipy ? -1 : 1;

	int x_index_base = flipx ? width - 1 : 0;
	int y_index = flipy ? height - 1 : 0;

	int sx = offsx;
	int sy = offsy;

	int ex = sx + width;
	int ey = sy + height;

	if (sx < 0)
	{
		int pixels = 0 - sx;
		sx += pixels;
		x_index_base += xinc * pixels;
	}

	if (sy < 0)
	{
		int pixels = 0 - sy;
		sy += pixels;
		y_index += yinc * pixels;
	}
	
	if (ex > 512) ey = 512;
	if (ex > 512) ex = 512;

	if (ex > sx)
	{
		for (int y = sy; y < ey; y++)
		{
			const UINT8 *source = source_base + y_index * width;
			UINT16 *dest = BurnBitmapGetPosition(1, 0, y);
			UINT8 *pri = BurnBitmapGetPrimapPosition(1, 0, y);
			int x_index = x_index_base;
			for (int x = sx; x < ex; x++)
			{
				const UINT8 c = source[x_index];
				if (c != transparent_color)
				{
						if (pri[x] < priority)
						{
							dest[x] = BURN_ENDIAN_SWAP_INT16(pal + c);
							pri[x] = priority;
						}
				}
				x_index += xinc;
			}
			y_index += yinc;
		}
	}
}

void limenko_draw_sprites()
{
	BurnBitmapFill(1, 0);
	BurnBitmapPrimapClear(1);

	UINT32 *sprites = (UINT32*)(DrvSprRAM + (0x1000  * spriteram_bit));

	for (UINT32 i = 0; i <= prev_sprites_count * 2; i += 2)
	{
		UINT32 sprites0 = BURN_ENDIAN_SWAP_INT32(sprites[i + 0]);
		UINT32 sprites1 = BURN_ENDIAN_SWAP_INT32(sprites[i + 1]);
		
		sprites0 = (sprites0 << 16) | (sprites0 >> 16);
		sprites1 = (sprites1 << 16) | (sprites1 >> 16);
		
		if (~sprites0 & 0x80000000) continue;

		int x      =  ((sprites0 & 0x01ff0000) >> 16);
		int width  = (((sprites0 & 0x0e000000) >> 25) + 1) * 8;
		bool flipx =    sprites0 & 0x10000000;
		int y      =    sprites0 & 0x000001ff;
		int height = (((sprites0 & 0x00000e00) >> 9) + 1) * 8;
		bool flipy =    sprites0 & 0x00001000;
		UINT32 code=   (sprites1 & 0x0007ffff) << 6;
		UINT32 color=  (sprites1 & 0xf0000000) >> 28;

		int pri = 0;
		if (sprites1 & 0x04000000)
		{
			pri = 1;
		}
		else
		{
			pri = 2;
		}

		/* Bounds checking */
		if ((code + (width * height)) > (UINT32)graphicsrom_len)
			continue;

		draw_single_sprite(width,height,code,color,flipx,flipy,x,y,0,pri);

		// wrap around x
		draw_single_sprite(width,height,code,color,flipx,flipy,x-512,y,0,pri);

		// wrap around y
		draw_single_sprite(width,height,code,color,flipx,flipy,x,y-512,0,pri);

		// wrap around x and y
		draw_single_sprite(width,height,code,color,flipx,flipy,x-512,y-512,0,pri);
	}
}

static void copy_sprites()
{
	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		UINT16 *dest = BurnBitmapGetPosition(0, 0, y);
		UINT16 *source = BurnBitmapGetPosition(1, 0, y);
		UINT8 *dest_pri = BurnBitmapGetPrimapPosition(0, 0, y);
		UINT8 *source_pri = BurnBitmapGetPrimapPosition(1, 0, y);

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			if (source[x] != 0)
			{
				if (dest_pri[x] < source_pri[x])
					dest[x] = BURN_ENDIAN_SWAP_INT16(source[x]);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_xBBBBBGGGGGRRRRR();
		DrvRecalc = 1; // force update
	}

	GenericTilemapSetEnable(0, (BURN_ENDIAN_SWAP_INT32(video_regs[0]) >> 16) & 0x04);
	GenericTilemapSetEnable(1, (BURN_ENDIAN_SWAP_INT32(video_regs[0]) >> 16) & 0x02);
	GenericTilemapSetEnable(2, (BURN_ENDIAN_SWAP_INT32(video_regs[0]) >> 16) & 0x01);

	GenericTilemapSetScrollX(0, BURN_ENDIAN_SWAP_INT32(video_regs[3]) >> 0);
	GenericTilemapSetScrollX(1, BURN_ENDIAN_SWAP_INT32(video_regs[2]) >> 0);
	GenericTilemapSetScrollX(2, BURN_ENDIAN_SWAP_INT32(video_regs[1]) >> 0);
	GenericTilemapSetScrollY(0, BURN_ENDIAN_SWAP_INT32(video_regs[3]) >> 16);
	GenericTilemapSetScrollY(1, BURN_ENDIAN_SWAP_INT32(video_regs[2]) >> 16);
	GenericTilemapSetScrollY(2, BURN_ENDIAN_SWAP_INT32(video_regs[1]) >> 16);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);
	if (nBurnLayer & 4) GenericTilemapDraw(2, pTransDraw, 1);

	if ((BURN_ENDIAN_SWAP_INT32(video_regs[0]) >> 16) & 0x08)
		if (nSpriteEnable & 1) copy_sprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	E132XSNewFrame();
	mcs51NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { cpu_clock / 60, (sound_type ? 4000000 : 2000000) / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	E132XSOpen(0);
	mcs51Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, E132XS);
		if (i == (nInterleave - 1)) E132XSSetIRQLine(0, CPU_IRQSTATUS_HOLD);

		CPU_RUN_SYNCINT(1, mcs51);
	}

	if (pBurnSoundOut) {
		switch (sound_type)
		{
			case 0: qs1000_update(pBurnSoundOut, nBurnSoundLen); break;
			case 1: MSM6295Render(pBurnSoundOut, nBurnSoundLen); break;
		}
	}

	mcs51Close();
	E132XSClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
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
		E132XSScan(nAction);
		mcs51_scan(nAction);

		switch (sound_type)
		{
			case 0: qs1000_scan(nAction, pnMin); break;
			case 1: MSM6295Scan(nAction, pnMin); break;
		}

		SCAN_VAR(audiocpu_data);
		SCAN_VAR(soundlatch);
		SCAN_VAR(spriteram_bit);
		SCAN_VAR(prev_sprites_count);
	}

	if (nAction & ACB_NVRAM) {
		EEPROMScan(nAction, pnMin);
	}

	return 0;
}


// Dynamite Bomber (Korea, Rev 1.5)

static struct BurnRomInfo dynabombRomDesc[] = {
	{ "rom.u6",			0x200000, 0x457e015d, 1 | BRF_PRG | BRF_ESS }, //  0 Hyperstone Boot ROM

	{ "rom.u5",			0x200000, 0x7e837adf, 2 | BRF_PRG | BRF_ESS }, //  1 Hyperstone Main ROM

	{ "rom.u16",		0x020000, 0xf66d7e4d, 3 | BRF_PRG | BRF_ESS }, //  2 qs1000:cpu

	{ "rom.u1",			0x200000, 0xbf33eff6, 4 | BRF_GRA },           //  3 Graphics
	{ "rom.u2",			0x200000, 0x790bbcd5, 4 | BRF_GRA },           //  4
	{ "rom.u3",			0x200000, 0xec094b12, 4 | BRF_GRA },           //  5
	{ "rom.u4",			0x200000, 0x88b24e3c, 4 | BRF_GRA },           //  6

	{ "rom.u18",		0x080000, 0x50d76732, 5 | BRF_SND },           //  7 QS1000 Samples
	{ "rom.u17",		0x080000, 0x20f2417c, 5 | BRF_SND },           //  8
	{ "qs1003.u4",		0x200000, 0x19e4b469, 5 | BRF_SND },           //  9
};

STD_ROM_PICK(dynabomb)
STD_ROM_FN(dynabomb)

static INT32 DynabombInit()
{
	speedhack_address = 0xe2784;
	speedhack_pc = 0xc25b8;	

	security_bit_config		= 0x00400000 & 0; // low
	eeprom_bit_config		= 0x00800000;
	spriteram_bit_config	= 0x80000000;

	return LimenkoCommonInit(TYPE_E132XN, 80000000, DynabombLoadCallback, 0x800000, 0);
}

struct BurnDriver BurnDrvDynabomb = {
	"dynabomb", NULL, NULL, NULL, "2000",
	"Dynamite Bomber (Korea, Rev 1.5)\0", NULL, "Limenko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, dynabombRomInfo, dynabombRomName, NULL, NULL, NULL, NULL, Sb2003InputInfo, Sb2003DIPInfo,
	DynabombInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 240, 4, 3
};


// Super Bubble 2003 (World, Ver 1.0)

static struct BurnRomInfo sb2003RomDesc[] = {
	{ "sb2003_05.u6",	0x200000, 0x8aec4554, 1 | BRF_PRG | BRF_ESS }, //  0 Hyperstone Boot ROM

	{ "07.u16",			0x020000, 0x78acc607, 3 | BRF_PRG | BRF_ESS }, //  1 QS1000 Code

	{ "01.u1",			0x200000, 0xd2c7091a, 4 | BRF_GRA },           //  2 Graphics
	{ "02.u2",			0x200000, 0xa0734195, 4 | BRF_GRA },           //  3
	{ "03.u3",			0x200000, 0x0f020280, 4 | BRF_GRA },           //  4
	{ "04.u4",			0x200000, 0xfc2222b9, 4 | BRF_GRA },           //  5

	{ "06.u18",			0x200000, 0xb6ad0d32, 5 | BRF_SND },           //  6 QS1000 Samples
	{ "qs1003.u4",		0x200000, 0x19e4b469, 5 | BRF_SND },           //  7
};

STD_ROM_PICK(sb2003)
STD_ROM_FN(sb2003)

static INT32 Sb2003Init()
{
	speedhack_address = 0x135800;
	speedhack_pc = 0x26da4;

	security_bit_config		= 0x00400000 & 0; // low
	eeprom_bit_config		= 0x00800000;
	spriteram_bit_config	= 0x80000000;

	return LimenkoCommonInit(TYPE_E132XN, 80000000, Sb2003LoadCallback, 0x800000, 0);
}

struct BurnDriver BurnDrvSb2003 = {
	"sb2003", NULL, NULL, NULL, "2003",
	"Super Bubble 2003 (World, Ver 1.0)\0", NULL, "Limenko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, sb2003RomInfo, sb2003RomName, NULL, NULL, NULL, NULL, Sb2003InputInfo, Sb2003DIPInfo,
	Sb2003Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 240, 4, 3
};


// Super Bubble 2003 (Asia, Ver 1.0)

static struct BurnRomInfo sb2003aRomDesc[] = {
	{ "sb2003a_05.u6",	0x200000, 0x265e45a7, 1 | BRF_PRG | BRF_ESS }, //  0 Hyperstone Boot ROM

	{ "07.u16",			0x020000, 0x78acc607, 3 | BRF_PRG | BRF_ESS }, //  1 QS1000 Code

	{ "01.u1",			0x200000, 0xd2c7091a, 4 | BRF_GRA },           //  2 Graphics
	{ "02.u2",			0x200000, 0xa0734195, 4 | BRF_GRA },           //  3
	{ "03.u3",			0x200000, 0x0f020280, 4 | BRF_GRA },           //  4
	{ "04.u4",			0x200000, 0xfc2222b9, 4 | BRF_GRA },           //  5

	{ "06.u18",			0x200000, 0xb6ad0d32, 5 | BRF_SND },           //  6 QS1000 Samples
	{ "qs1003.u4",		0x200000, 0x19e4b469, 5 | BRF_SND },           //  7
};

STD_ROM_PICK(sb2003a)
STD_ROM_FN(sb2003a)

struct BurnDriver BurnDrvSb2003a = {
	"sb2003a", "sb2003", NULL, NULL, "2003",
	"Super Bubble 2003 (Asia, Ver 1.0)\0", NULL, "Limenko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, sb2003aRomInfo, sb2003aRomName, NULL, NULL, NULL, NULL, Sb2003InputInfo, Sb2003DIPInfo,
	Sb2003Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 240, 4, 3
};


// Legend of Heroes

static struct BurnRomInfo legendohRomDesc[] = {
	{ "01.sys_rom4",	0x080000, 0x49b4a91f, 1 | BRF_PRG | BRF_ESS }, //  0 Hyperstone Boot ROM

	{ "sys_rom6",		0x200000, 0x5c13d467, 2 | BRF_PRG | BRF_ESS }, //  1 Hyperstone Main ROM
	{ "sys_rom5",		0x200000, 0x19dc8d23, 2 | BRF_PRG | BRF_ESS }, //  2

	{ "cg_rom10",		0x200000, 0x93a48489, 4 | BRF_GRA },           //  3 Graphics
	{ "cg_rom20",		0x200000, 0x1a6c0258, 4 | BRF_GRA },           //  4
	{ "cg_rom30",		0x200000, 0xa0559ef4, 4 | BRF_GRA },           //  5
	{ "cg_rom40",		0x200000, 0xa607b2b5, 4 | BRF_GRA },           //  6
	{ "cg_rom11",		0x200000, 0xa9fd5a50, 4 | BRF_GRA },           //  7
	{ "cg_rom21",		0x200000, 0xb05cdeb2, 4 | BRF_GRA },           //  8
	{ "cg_rom31",		0x200000, 0xa9a0d386, 4 | BRF_GRA },           //  9
	{ "cg_rom41",		0x200000, 0x1c014f45, 4 | BRF_GRA },           // 10
	{ "02.cg_rom12",	0x080000, 0x8b2e8cbc, 4 | BRF_GRA },           // 11
	{ "03.cg_rom22",	0x080000, 0xa35960c8, 4 | BRF_GRA },           // 12
	{ "04.cg_rom32",	0x080000, 0x3f486cab, 4 | BRF_GRA },           // 13
	{ "05.cg_rom42",	0x080000, 0x5d807bec, 4 | BRF_GRA },           // 14

	{ "sou_prg.06",		0x080000, 0xbfafe7aa, 3 | BRF_PRG | BRF_ESS }, // 15 QS1000 Code

	{ "sou_rom.07",		0x080000, 0x4c6eb6d2, 5 | BRF_SND },           // 16 QS1000 Samples
	{ "sou_rom.08",		0x080000, 0x42c32dd5, 5 | BRF_SND },           // 17
	{ "qs1003.u4",		0x200000, 0x19e4b469, 5 | BRF_SND },           // 18
};

STD_ROM_PICK(legendoh)
STD_ROM_FN(legendoh)

static INT32 LegendohInit()
{
	speedhack_address = 0x32ab0;
	speedhack_pc = 0x23e32;

	security_bit_config		= 0x00400000; // high
	eeprom_bit_config		= 0x00800000;
	spriteram_bit_config	= 0x80000000;

	return LimenkoCommonInit(TYPE_E132XN, 80000000, LegendohLoadCallback, 0x1200000, 0);
}

struct BurnDriver BurnDrvLegendoh = {
	"legendoh", NULL, NULL, NULL, "2000",
	"Legend of Heroes\0", NULL, "Limenko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, legendohRomInfo, legendohRomName, NULL, NULL, NULL, NULL, LegendohInputInfo, LegendohDIPInfo,
	LegendohInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 240, 4, 3
};


// Spotty (Ver. 2.0.2)

static struct BurnRomInfo spottyRomDesc[] = {
	{ "sys_rom2",		0x80000, 0x6ded8d9b, 1 | BRF_PRG | BRF_ESS }, //  0 Hyperstone Boot ROM

	{ "at89c4051.mcu",	0x01000, 0x82ceab26, 3 | BRF_PRG | BRF_ESS }, //  1 at89c4051 Code

	{ "gc_rom1",		0x80000, 0xea03f9c5, 2 | BRF_PRG | BRF_ESS }, //  2 Hyperstone Main ROM / Graphics
	{ "gc_rom3",		0x80000, 0x0ddac0b9, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "sou_rom1",		0x40000, 0x5791195b, 4 | BRF_SND },           //  4 OKI Samples
};

STD_ROM_PICK(spotty)
STD_ROM_FN(spotty)

static INT32 SpottyInit()
{
	speedhack_address = 0x6626c;
	speedhack_pc = 0x8560;
	
	security_bit_config		= 0x00400000 & 0; // low
	eeprom_bit_config		= 0x00800000;
	spriteram_bit_config	= 0x00080000;

	return LimenkoCommonInit(TYPE_GMS30C2232, 20000000, SpottyLoadCallback, 0x200000, 1);
}

struct BurnDriver BurnDrvSpotty = {
	"spotty", NULL, NULL, NULL, "2001",
	"Spotty (Ver. 2.0.2)\0", NULL, "Prince Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_CASINO, 0,
	NULL, spottyRomInfo, spottyRomName, NULL, NULL, NULL, NULL, SpottyInputInfo, SpottyDIPInfo,
	SpottyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 240, 4, 3
};
