// FB Neo Super Dodge Ball driver module
// Based on MAME driver by Paul Hampson, Nicola Salmoria

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "m6809_intf.h"
#include "m6800_intf.h"
#include "burn_ym3812.h"
#include "msm5205.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvMCURAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 soundlatch;
static INT32 scrollx;
static INT32 flipscreen;
static INT32 tilebank;
static INT32 spritebank;
static INT32 bankdata;

static UINT8 mcu_inputs[5];
static INT32 mcu_status;
static UINT8 mcu_latch;

static INT32 adpcm_pos[2];
static INT32 adpcm_end[2];
static INT32 adpcm_data[2];

static INT32 vblank;
static INT32 lastline;

static struct BurnInputInfo SpdodgebInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Spdodgeb)

static struct BurnDIPInfo SpdodgebDIPList[]=
{
	{0x12, 0xff, 0xff, 0xc0, NULL					},
	{0x13, 0xff, 0xff, 0xff, NULL					},
	{0x14, 0xff, 0xff, 0x3f, NULL					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x12, 0x01, 0xc0, 0x80, "Easy"					},
	{0x12, 0x01, 0xc0, 0xc0, "Normal"				},
	{0x12, 0x01, 0xc0, 0x40, "Hard"					},
	{0x12, 0x01, 0xc0, 0x00, "Very Hard"			},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x13, 0x01, 0x07, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x01, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x02, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x13, 0x01, 0x38, 0x00, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x08, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x13, 0x01, 0x40, 0x40, "Off"					},
	{0x13, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x13, 0x01, 0x80, 0x00, "Off"					},
	{0x13, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x14, 0x01, 0x02, 0x02, "Off"					},
	{0x14, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x04, 0x00, "No"					},
	{0x14, 0x01, 0x04, 0x04, "Yes"					},
};

STDDIPINFO(Spdodgeb)

static void bankswitch(INT32 bank)
{
	bankdata = bank;

	M6502MapMemory(DrvM6502ROM + (bank * 0x4000), 0x4000, 0x7fff, MAP_ROM);
}

static void mcu_sync()
{
	INT32 cyc = ((M6502TotalCycles() * 4) / 2) - HD63701TotalCycles();

	if (cyc > 0) {
		HD63701Run(cyc);
	}
}

static void spdodgeb_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x3000:
		case 0x3001:
		case 0x3003:
		return; // nop

		case 0x3005:
			mcu_sync();
			HD63701SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		return;

		case 0x3002:
			soundlatch = data;
			M6809SetIRQLine(0, CPU_IRQSTATUS_HOLD);
			BurnTimerUpdateYM3812(M6502TotalCycles());
		return;

		case 0x3004:
			scrollx = (scrollx & 0xff00) | data;
		return;

		case 0x3006:
			flipscreen = data & 0x01;
			bankswitch(((data >> 1) & 1) ^ 1);
			scrollx = (scrollx & 0x00ff) | ((data & 0x04) << 6);
			tilebank = (data & 0x30) >> 4;
			spritebank = (data & 0xc0) >> 6;
		return;

		case 0x3800:
			mcu_sync();
			mcu_latch = data;
		return;
	}
}

static UINT8 spdodgeb_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
		{
			UINT8 ret = DrvInputs[0] & 0x3c;
			ret ^= vblank ? 1 : 0;
			mcu_sync();
			ret |= (mcu_status & 0x80) ? 2 : 0;
			return ret;
		}

		case 0x3001:
			return DrvDips[1];

		case 0x3801:
		case 0x3802:
		case 0x3803:
		case 0x3804:
		case 0x3805:
			return mcu_inputs[(address & 7) - 1];
	}

	return 0;
}

static void adpcm_write(UINT16 offset, UINT8 data)
{
	INT32 chip = offset & 1;

	switch (offset/2)
	{
		case 3:
			MSM5205ResetWrite(chip, 1);
		break;

		case 2:
			adpcm_pos[chip] = (data & 0x7f) * 0x200;
		break;

		case 1:
			adpcm_end[chip] = (data & 0x7f) * 0x200;
		break;

		case 0:
			MSM5205ResetWrite(chip, 0);
		break;
	}
}

static void spdodgeb_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2800:
		case 0x2801:
			BurnYM3812Write(0, address & 1, data); // right?
		return;

		case 0x3800:
		case 0x3801:
		case 0x3802:
		case 0x3803:
		case 0x3804:
		case 0x3805:
		case 0x3806:
		case 0x3807:
			adpcm_write(address & 7, data);
		return;
	}
}

static UINT8 spdodgeb_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x1000:
			return soundlatch;

		case 0x2800:
		case 0x2801:
			return BurnYM3812Read(0, address & 1);
	}

	return 0;
}

static void spdodgeb_mcu_write(UINT16 address, UINT8 data)
{
	if (address >= 0x0000 && address <= 0x0027) {
		hd63xy_internal_registers_w(address, data);
	}

	if (address >= 0x0040 && address <= 0x013f) {
		DrvMCURAM[address - 0x40] = data;
	}

	if (address >= 0x8081 && address <= 0x8085) {
		mcu_inputs[address - 0x8081] = data;
	}
}

static UINT8 spdodgeb_mcu_read(UINT16 address)
{
	if (address >= 0x0000 && address <= 0x0027) {
		return hd63xy_internal_registers_r(address);
	}

	if (address >= 0x0040 && address <= 0x013f) {
		return DrvMCURAM[address - 0x40];
	}

	if (address == 0x8080) {
		return mcu_latch;
	}

	return 0xff;
}

static void spdodgeb_mcu_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0x1ff)
	{
		case HD63701_PORT5:
			mcu_status = data & 0xc0;
		return;
	}
}

static UINT8 spdodgeb_mcu_read_port(UINT16 port)
{
	switch (port & 0x1ff)
	{
		case HD63701_PORT2:
			return DrvInputs[1];

		case HD63701_PORT6:
			return DrvInputs[2];

		case HD63701_PORT5:
			return DrvDips[2];
	}

	return 0;
}

static void spd_adpcm_int(INT32 chip)
{
	if (adpcm_pos[chip] >= adpcm_end[chip] || adpcm_pos[chip] >= 0x10000)
	{
		MSM5205ResetWrite(chip, 1);
	}
	else if (adpcm_data[chip] != -1)
	{
		MSM5205DataWrite(chip, adpcm_data[chip] & 0x0f);
		adpcm_data[chip] = -1;
	}
	else
	{
		UINT8 *ROM = DrvSndROM + 0x10000 * chip;

		adpcm_data[chip] = ROM[adpcm_pos[chip]++ & 0xffff];
		MSM5205DataWrite(chip, adpcm_data[chip] >> 4);
	}
}

static void adpcm_int_0()
{
	spd_adpcm_int(0);
}

static void adpcm_int_1()
{
	spd_adpcm_int(1);
}

static void DrvFMIRQHandler(int, INT32 nStatus)
{
	M6809SetIRQLine(1/*FIRQ*/, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)M6809TotalCycles() * nSoundRate / 2000000);
}

static tilemap_scan( bg )
{
	return (col & 0x1f) + ((row & 0x1f) << 5) + ((col & 0x20) << 5);
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[offs + 0x800];

	TILE_SET_INFO(0, DrvVidRAM[offs] + (attr << 8), (attr >> 5) + (tilebank << 3), 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	bankswitch(0);
	M6502Reset();
	M6502Close();

	M6809Open(0);
	M6809Reset();
	BurnYM3812Reset();
	MSM5205Reset();
	M6809Close();

	HD63701Open(0);
	HD63701Reset();
	HD63701Close();

	soundlatch = 0;
	scrollx = 0;
	flipscreen = 0;
	tilebank = 0;
	spritebank = 0;

	mcu_latch = 0;
	mcu_status = 0;
	memset(mcu_inputs, 0, sizeof(mcu_inputs));

	adpcm_pos[0] = adpcm_pos[1] = 0;
	adpcm_end[0] = adpcm_end[1] = 0;
	adpcm_data[0] = adpcm_data[0] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x010000;
	DrvM6809ROM		= Next; Next += 0x008000;
	DrvMCUROM		= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x180000;
	DrvGfxROM1		= Next; Next += 0x180000;

	DrvSndROM		= Next; Next += 0x040000;

	DrvColPROM		= Next; Next += 0x000800;

	DrvPalette		= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x001000;
	DrvM6809RAM		= Next; Next += 0x001000;
	DrvMCURAM		= Next; Next += 0x000200;
	DrvVidRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x000100;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode() // 0, 200
{
	INT32 Plane0[4]  = { STEP4(0,2) };
	INT32 Plane1[4]  = { 0x20000*8+0, 0x20000*8+4, 0, 4 };
	INT32 XOffs0[8]  = { 1, 0, 64+1, 64+0, 128+1, 128+0, 192+1, 192+0 };
	INT32 XOffs1[16] = { STEP4(3,-1), STEP4(128+3,-1), STEP4(256+3,-1), STEP4(256+128+3,-1) };
	INT32 YOffs[16]  = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x40000);

	GfxDecode(0x2000, 4,  8,  8, Plane0, XOffs0, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs, 0x200, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	INT32 boot = BurnDrvGetFlags() & BDF_BOOTLEG;

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvM6502ROM + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM   + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000, k++, 1)) return 1;
		if (boot) if (BurnLoadRom(DrvGfxROM0  + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x20000, k++, 1)) return 1;
		if (boot) if (BurnLoadRom(DrvGfxROM0  + 0x30000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x00000, k++, 1)) return 1;
		if (boot) if (BurnLoadRom(DrvGfxROM1  + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x20000, k++, 1)) return 1;
		if (boot) if (BurnLoadRom(DrvGfxROM1  + 0x30000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM   + 0x10000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00400, k++, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,				0x0000, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,				0x1000, 0x10ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,				0x2000, 0x2fff, MAP_RAM);
	// bank 4000-7fff
	bankswitch(0);
	M6502MapMemory(DrvM6502ROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(spdodgeb_main_write);
	M6502SetReadHandler(spdodgeb_main_read);
	M6502Close();

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,			0x0000, 0x0fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM,			0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(spdodgeb_sound_write);
	M6809SetReadHandler(spdodgeb_sound_read);
	M6809Close();

	HD63701Init(0);
	HD63701Open(0);
	HD63701MapMemory(DrvMCUROM,			0xc000, 0xffff, MAP_ROM);
	HD63701SetReadHandler(spdodgeb_mcu_read);
	HD63701SetWriteHandler(spdodgeb_mcu_write);
	HD63701SetReadPortHandler(spdodgeb_mcu_read_port);
	HD63701SetWritePortHandler(spdodgeb_mcu_write_port);
	HD63701Close();

	BurnYM3812Init(1, 3000000, &DrvFMIRQHandler, 0);
	BurnTimerAttachYM3812(&M6809Config, 2000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 0.80, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, DrvSynchroniseStream, 384000, adpcm_int_0, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	MSM5205Init(1, DrvSynchroniseStream, 384000, adpcm_int_1, MSM5205_S48_4B, 1);
	MSM5205SetRoute(1, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, bg_map_scan, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x80000, 0, 0x1f);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	M6502Exit();
	M6809Exit();
	HD63701Exit();

	MSM5205Exit();
	BurnYM3812Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x400; i++)
	{
		INT32 bit0 = (DrvColPROM[i + 0x000] >> 0) & 1;
		INT32 bit1 = (DrvColPROM[i + 0x000] >> 1) & 1;
		INT32 bit2 = (DrvColPROM[i + 0x000] >> 2) & 1;
		INT32 bit3 = (DrvColPROM[i + 0x000] >> 3) & 1;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x000] >> 4) & 1;
		bit1 = (DrvColPROM[i + 0x000] >> 5) & 1;
		bit2 = (DrvColPROM[i + 0x000] >> 6) & 1;
		bit3 = (DrvColPROM[i + 0x000] >> 7) & 1;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x400] >> 0) & 1;
		bit1 = (DrvColPROM[i + 0x400] >> 1) & 1;
		bit2 = (DrvColPROM[i + 0x400] >> 2) & 1;
		bit3 = (DrvColPROM[i + 0x400] >> 3) & 1;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x100; i += 4)
	{
		INT32 attr = DrvSprRAM[i+1];
		INT32 which = DrvSprRAM[i+2]+((attr & 0x07)<<8);
		INT32 sx = DrvSprRAM[i+3];
		INT32 sy = 240 - DrvSprRAM[i];
		INT32 size = (attr & 0x80) >> 7;
		INT32 color = ((attr & 0x38) >> 3) + (spritebank << 3);
		INT32 flipx = ~attr & 0x40;
		INT32 flipy = 0;
		INT32 dy = -16;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
			dy = -dy;
		}

		if (sx < -8) sx += 256; else if (sx > 248) sx -= 256;

		switch (size)
		{
			case 0: /* normal */
			{
				if (sy < -8) sy += 256; else if (sy > 248) sy -= 256;

				Draw16x16MaskTile(pTransDraw, which, sx, sy, flipx, flipy, color, 4, 0, 0x200, DrvGfxROM1);
			}
			break;

			case 1: /* double y */
			{
				if (flipscreen) { if (sy > 240) sy -= 256; } else { if (sy < 0) sy += 256; }
				which &= ~1;
				Draw16x16MaskTile(pTransDraw, which + 0, sx, sy + dy, flipx, flipy, color, 4, 0, 0x200, DrvGfxROM1);
				Draw16x16MaskTile(pTransDraw, which + 1, sx, sy, flipx, flipy, color, 4, 0, 0x200, DrvGfxROM1);
			}
			break;
		}
	}
}

static void DrvPartialDraw(INT32 scanline)
{
	if (!pBurnDraw) return;

	if (scanline < 0 || scanline > nScreenHeight || scanline == lastline || lastline > scanline) return;
	INT32 finish = scanline + 16;
	if (finish > nScreenHeight) finish = nScreenHeight;

	GenericTilesSetClip(-1, -1, scanline, finish);

	GenericTilemapSetScrollX(0, scrollx + 5);
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	GenericTilesClearClip();

	lastline = finish-1;
}

static INT32 DrvPartialDrawFinish()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollX(0, scrollx);
	GenericTilemapDraw(0, pTransDraw, 0);

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
		DrvInputs[0] = DrvDips[0] | 0x3c;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	M6809NewFrame();
	M6502NewFrame(); // this cpu is not using a timer, but this is needed for cpu synch.
	HD63701NewFrame();

	INT32 nInterleave = 272;
	INT32 nCyclesTotal[3] = { (INT32)(2000000 / 57.44853), (INT32)(2000000 / 57.44853), (INT32)(4000000 / 57.44853) };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	MSM5205NewFrame(0, 2000000, nInterleave);
	M6809Open(0);
	M6502Open(0);
	HD63701Open(0);

	vblank = 0;
	lastline = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, M6502);

		if (i == 256) {
			vblank = 1;
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
			if (pBurnDraw) DrvPartialDraw(i);
		} else if ((i % 8) == 0 && i < 240) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
			if (pBurnDraw) DrvPartialDraw(i);
		}

		BurnTimerUpdateYM3812((i + 1) * (nCyclesTotal[1] / nInterleave));
		MSM5205UpdateScanline(i);

		mcu_sync(); // HD63701
	}

	BurnTimerEndFrameYM3812(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	M6502Close();
	M6809Close();
	HD63701Close();

	if (pBurnDraw) {
		DrvPartialDrawFinish();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
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

		M6502Scan(nAction);
		M6809Scan(nAction);
		HD63701Scan(nAction);
		MSM5205Scan(nAction, pnMin);
		BurnYM3812Scan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(scrollx);
		SCAN_VAR(flipscreen);
		SCAN_VAR(tilebank);
		SCAN_VAR(spritebank);
		SCAN_VAR(bankdata);

		SCAN_VAR(mcu_latch);
		SCAN_VAR(mcu_status);
		SCAN_VAR(mcu_inputs);

		SCAN_VAR(adpcm_pos);
		SCAN_VAR(adpcm_end);
		SCAN_VAR(adpcm_data);
	}

	if (nAction & ACB_WRITE) {
		bankswitch(bankdata);
	}

	return 0;
}


// Super Dodge Ball (US)

static struct BurnRomInfo spdodgebRomDesc[] = {
	{ "22a-04.139",		0x10000, 0x66071fda, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code

	{ "22j5-0.33",		0x08000, 0xc31e264e, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "22ja-0.162",		0x04000, 0x7162a97b, 3 | BRF_PRG | BRF_ESS }, //  2 HD63701Y0 Code

	{ "22a-4.121",		0x20000, 0xacc26051, 4 | BRF_GRA },           //  3 Characters
	{ "22a-3.107",		0x20000, 0x10bb800d, 4 | BRF_GRA },           //  4

	{ "22a-1.2",		0x20000, 0x3bd1c3ec, 5 | BRF_GRA },           //  5 Sprites
	{ "22a-2.35",		0x20000, 0x409e1be1, 5 | BRF_GRA },           //  6

	{ "22j6-0.83",		0x10000, 0x744a26e3, 6 | BRF_SND },           //  7 MSM5205 Samples
	{ "22j7-0.82",		0x10000, 0x2fa1de21, 6 | BRF_SND },           //  8

	{ "mb7132e.158",	0x00400, 0x7e623722, 7 | BRF_GRA },           //  9 Color Data
	{ "mb7122e.159",	0x00400, 0x69706e8d, 7 | BRF_GRA },           // 10
};

STD_ROM_PICK(spdodgeb)
STD_ROM_FN(spdodgeb)

struct BurnDriver BurnDrvSpdodgeb = {
	"spdodgeb", NULL, NULL, NULL, "1987",
	"Super Dodge Ball (US)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, spdodgebRomInfo, spdodgebRomName, NULL, NULL, NULL, NULL, SpdodgebInputInfo, SpdodgebDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Nekketsu Koukou Dodgeball Bu (Japan)

static struct BurnRomInfo nkdodgeRomDesc[] = {
	{ "22j4-0.139",		0x10000, 0xaa674fd8, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code

	{ "22j5-0.33",		0x08000, 0xc31e264e, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "22ja-0.162",		0x04000, 0x7162a97b, 3 | BRF_PRG | BRF_ESS }, //  2 HD63701Y0 Code

	{ "tj22j4-0.121",	0x20000, 0xd2922b3f, 4 | BRF_GRA },           //  3 Characters
	{ "tj22j3-0.107",	0x20000, 0x79cd1315, 4 | BRF_GRA },           //  4

	{ "tj22j1-0.2",		0x20000, 0x9ed27a8d, 5 | BRF_GRA },           //  5 Sprites
	{ "tj22j2-0.35",	0x20000, 0x768934f9, 5 | BRF_GRA },           //  6

	{ "22j6-0.83",		0x10000, 0x744a26e3, 6 | BRF_SND },           //  7 MSM5205 Samples
	{ "22j7-0.82",		0x10000, 0x2fa1de21, 6 | BRF_SND },           //  8

	{ "22j8-0.158",		0x00400, 0xc368440f, 7 | BRF_GRA },           //  9 Color Data
	{ "22j9-0.159",		0x00400, 0x6059f401, 7 | BRF_GRA },           // 10
};

STD_ROM_PICK(nkdodge)
STD_ROM_FN(nkdodge)

struct BurnDriver BurnDrvNkdodge = {
	"nkdodge", "spdodgeb", NULL, NULL, "1987",
	"Nekketsu Koukou Dodgeball Bu (Japan)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, nkdodgeRomInfo, nkdodgeRomName, NULL, NULL, NULL, NULL, SpdodgebInputInfo, SpdodgebDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};


// Nekketsu Koukou Dodgeball Bu (Japan, bootleg)

static struct BurnRomInfo nkdodgebRomDesc[] = {
	{ "12.bin",			0x10000, 0xaa674fd8, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code

	{ "22j5-0.33",		0x08000, 0xc31e264e, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "hd63701y0p.n12",	0x04000, 0x7162a97b, 3 | BRF_PRG | BRF_ESS }, //  2 HD63701Y0 Code

	{ "10.bin",			0x10000, 0x442326fd, 4 | BRF_GRA },           //  3 Characters
	{ "11.bin",			0x10000, 0x2140b070, 4 | BRF_GRA },           //  4
	{ "9.bin",			0x10000, 0x18660ac1, 4 | BRF_GRA },           //  5
	{ "8.bin",			0x10000, 0x5caae3c9, 4 | BRF_GRA },           //  6

	{ "2.bin",			0x10000, 0x1271583e, 5 | BRF_GRA },           //  7 Sprites
	{ "1.bin",			0x10000, 0x5ae6cccf, 5 | BRF_GRA },           //  8
	{ "4.bin",			0x10000, 0xf5022822, 5 | BRF_GRA },           //  9
	{ "3.bin",			0x10000, 0x05a71179, 5 | BRF_GRA },           // 10

	{ "22j6-0.83",		0x10000, 0x744a26e3, 6 | BRF_SND },           // 11 MSM5205 Samples
	{ "22j7-0.82",		0x10000, 0x2fa1de21, 6 | BRF_SND },           // 12

	{ "27s191.bin",		0x00800, 0x317e42ea, 7 | BRF_GRA },           // 13 Color Data
	{ "82s137.bin",		0x00400, 0x6059f401, 7 | BRF_GRA },           // 14
};

STD_ROM_PICK(nkdodgeb)
STD_ROM_FN(nkdodgeb)

struct BurnDriver BurnDrvNkdodgeb = {
	"nkdodgeb", "spdodgeb", NULL, NULL, "1987",
	"Nekketsu Koukou Dodgeball Bu (Japan, bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, nkdodgebRomInfo, nkdodgebRomName, NULL, NULL, NULL, NULL, SpdodgebInputInfo, SpdodgebDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 240, 4, 3
};
