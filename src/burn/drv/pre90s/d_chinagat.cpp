// FB Alpha China Gate driver module
// Based on MAME driver by Paul Hampson

#include "tiles_generic.h"
#include "hd6309_intf.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "burn_ym2203.h"
#include "msm6295.h"
#include "msm5205.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSubROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bankdata[2];
static UINT8 soundlatch;
static INT32 flipscreen;
static UINT16 scrollx;
static UINT16 scrolly;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 is_bootleg = 0;
static INT32 vblank;

static struct BurnInputInfo DrvInputList[] = {
	{"Coin 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"Coin 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 coin"	},
	{"Coin 3",		BIT_DIGITAL,	DrvJoy1 + 3,	"p3 coin"	},

	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},
	{"Reset",		BIT_DIGITAL,	&DrvReset,      "reset"		},

	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x14, 0xff, 0xff, 0xbf, NULL			},
	{0x15, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x14, 0x01, 0x07, 0x00, "4 Coins 1 Credit"	},
	{0x14, 0x01, 0x07, 0x01, "3 Coins 1 Credit"	},
	{0x14, 0x01, 0x07, 0x02, "2 Coin  1 Credit"	},
	{0x14, 0x01, 0x07, 0x07, "1 Coin  1 Credit"	},
	{0x14, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x07, 0x05, "1 Coins 3 Credits"	},
	{0x14, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x14, 0x01, 0x38, 0x00, "4 Coins 1 Credit"	},
	{0x14, 0x01, 0x38, 0x08, "3 Coins 1 Credit"	},
	{0x14, 0x01, 0x38, 0x10, "2 Coin  1 Credit"	},
	{0x14, 0x01, 0x38, 0x30, "1 Coin  1 Credit"	},
	{0x14, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x38, 0x28, "1 Coins 3 Credits"	},
	{0x14, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  5 Credits"	},

//	{0   , 0xfe, 0   ,    2, "Cabinet"		},
//	{0x14, 0x01, 0x40, 0x40, "Cocktail"		},
//	{0x14, 0x01, 0x40, 0x00, "Upright"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x03, 0x01, "Easy"			},
	{0x15, 0x01, 0x03, 0x03, "Normal"		},
	{0x15, 0x01, 0x03, 0x02, "Hard"			},
	{0x15, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x04, 0x00, "Off"			},
	{0x15, 0x01, 0x04, 0x04, "On"			},

	{0   , 0xfe, 0   ,    4, "Timer"		},
	{0x15, 0x01, 0x30, 0x00, "50"			},
	{0x15, 0x01, 0x30, 0x20, "55"			},
	{0x15, 0x01, 0x30, 0x30, "60"			},
	{0x15, 0x01, 0x30, 0x10, "70"			},

	{0   , 0xfe, 0   ,    4, "Lifes"		},
	{0x15, 0x01, 0xc0, 0x00, "1"			},
	{0x15, 0x01, 0xc0, 0xc0, "2"			},
	{0x15, 0x01, 0xc0, 0x80, "3"			},
	{0x15, 0x01, 0xc0, 0x40, "4"			},
};

STDDIPINFO(Drv)

static inline void palette_write(INT32 offset)
{
	INT32 r = DrvPalRAM[offset + 0x000] & 0xf;
	INT32 g = DrvPalRAM[offset + 0x000] >> 4;
	INT32 b = DrvPalRAM[offset + 0x200] & 0xf;

	DrvPalette[offset] = BurnHighCol(r+r*16, g+g*16, b+b*16,0);
}

static void bankswitch1(INT32 data)
{
	bankdata[0] = data;

	HD6309MapMemory(DrvMainROM + (data & 0x07) * 0x4000, 0x4000, 0x7fff, MAP_ROM);
}

static void chinagat_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0x3000 && address <= 0x317f) {
		DrvPalRAM[(address & 0x1ff) + 0x000] = data;
		palette_write(address & 0x1ff);
		return;
	}

	if (address >= 0x3400 && address <= 0x357f) {
		DrvPalRAM[(address & 0x1ff) + 0x200] = data;
		palette_write(address & 0x1ff);
		return;
	}

	switch (address)
	{
		case 0x3e00:
			soundlatch = data;
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		return;

		case 0x3e01: // clear NMI
			HD6309SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0x3e02: // clear FIRQ
			HD6309SetIRQLine(0x01, CPU_IRQSTATUS_NONE);
		return;

		case 0x3e03: // clear IRQ
			HD6309SetIRQLine(0x00, CPU_IRQSTATUS_NONE);
		return;

		case 0x3e04: // set sub irq
			HD6309Close();
			HD6309Open(1);
			HD6309SetIRQLine(0x00, CPU_IRQSTATUS_AUTO);
			HD6309Close();
			HD6309Open(0);
		return;

		case 0x3e06:
			scrolly = (scrolly & 0x0100) | data;
		return;

		case 0x3e07:
			scrollx = (scrollx & 0x0100) | data;
		return;

		case 0x3f00:
			scrolly = (scrolly & 0x00ff) | ((data & 0x02) << 7);
			scrollx = (scrollx & 0x00ff) | ((data & 0x01) << 8);
			flipscreen = ~data & 0x04;
		return;

		case 0x3f01:
			bankswitch1(data);
		return;
	}
}

static UINT8 chinagat_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x3f00:
			return (DrvInputs[0] & 0x0e) | (vblank ? 0x01 : 0);

		case 0x3f01:
			return DrvDips[0];

		case 0x3f02:
			return DrvDips[1];

		case 0x3f03:
			return DrvInputs[1];

		case 0x3f04:
			return DrvInputs[2];
	}

	return 0;
}

static void bankswitch2(INT32 data)
{
	bankdata[1] = data;

	HD6309MapMemory(DrvSubROM + (data & 0x07) * 0x4000, 0x4000, 0x7fff, MAP_ROM);
}

static void chinagat_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
			bankswitch2(data);
		return;

		case 0x2800:
		return;		// nop
	}
}

static void __fastcall chinagat_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8800:
			BurnYM2151SelectRegister(data);
		return;
		
		case 0x8801:
			BurnYM2151WriteRegister(data);
		return;
		
		case 0x9800:
			if (BurnDrvGetFlags() & BDF_BOOTLEG) {
			//	bprintf (0, _T("Bootleg MCU Command: %2.2x\n"), data);
			} else {
				MSM6295Command(0, data);
			}
		return;
	}
}

static UINT8 __fastcall chinagat_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8800:
		case 0x8801:
			return BurnYM2151ReadStatus();

		case 0x9800:
			return MSM6295ReadStatus(0);

		case 0xa000:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static void __fastcall chinagat_bootleg2_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8800:
		case 0x8801:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0x8804:
		case 0x8805:
			BurnYM2203Write(1, address & 1, data);
		return;
	}
}

static UINT8 __fastcall chinagat_bootleg2_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8800:
		case 0x8801:
			return BurnYM2203Read(0, address & 1);

		case 0x8804:
		case 0x8805:
			return BurnYM2203Read(1, address & 1);

		case 0xa000:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}


static void DrvYM2151IrqHandler(INT32 nStatus)
{
	ZetSetIRQLine(0, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 3579545;
}

static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 3579545;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	HD6309Open(0);
	HD6309Reset();
	HD6309Close();

	HD6309Open(1);
	HD6309Reset();
	HD6309Close();

	ZetOpen(0);
	ZetReset();

	if (is_bootleg == 2) {
		BurnYM2203Reset();
	} else {
		MSM6295Reset(0);
		BurnYM2151Reset();
	}

	ZetClose();

	bankdata[0] = 0;
	bankdata[1] = 0;
	scrollx = 0;
	scrolly = 0;
	soundlatch = 0;
	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x020000;
	DrvSubROM		= Next; Next += 0x020000;
	DrvZ80ROM		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x100000;
	DrvGfxROM2		= Next; Next += 0x080000;

	MSM6295ROM        	= Next;
	DrvSndROM		= Next; Next += 0x040000;

	DrvPalette       	= (UINT32*)Next; Next += 0x0180 * sizeof(UINT32);

	AllRam            	= Next;

	DrvShareRAM		= Next; Next += 0x002000;
	DrvBgRAM		= Next; Next += 0x000800;
	DrvFgRAM		= Next; Next += 0x000800;
 	DrvSprRAM		= Next; Next += 0x000200;
	DrvPalRAM		= Next; Next += 0x000400;
	DrvZ80RAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Plane0[4]  = { STEP4(0,2) };
	INT32 XOffs0[8]  = { 1, 0, 65, 64, 129, 128, 193, 192 };
	INT32 YOffs[16]  = { STEP16(0,8) };

	INT32 Plane1[4]  = { 0x40000*8+0, 0x40000*8+4, 0, 4 };
	INT32 XOffs1[16] = { STEP4(3,-1), STEP4(128+3,-1), STEP4(256+3,-1), STEP4(384+3,-1) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM0, 0x20000);

	GfxDecode(0x1000, 4,  8,  8, Plane0, XOffs0, YOffs, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x80000);

	GfxDecode(0x1000, 4, 16, 16, Plane1, XOffs1, YOffs, 0x200, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x80000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs, 0x200, tmp, DrvGfxROM2);

	BurnFree(tmp);
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
		if (BurnLoadRom(DrvMainROM + 0x00000,    0, 1)) return 1;

		if (BurnLoadRom(DrvSubROM  + 0x00000,    1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x00000,    2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,    3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,    4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000,    5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40000,    6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x60000,    7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,    8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000,    9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x40000,   10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x50000,   11, 1)) return 1;

		if (BurnDrvGetFlags() & BDF_BOOTLEG) {
			if (BurnLoadRom(DrvSndROM  + 0x00000,   12, 1)) return 1;
			if (BurnLoadRom(DrvSndROM  + 0x10000,   13, 1)) return 1;
			if (BurnLoadRom(DrvSndROM  + 0x20000,   14, 1)) return 1;
			if (BurnLoadRom(DrvSndROM  + 0x30000,   15, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x80000, 18, 1)) return 1; // check for mcu
			is_bootleg = DrvGfxROM1[0x80000] ? 1 : 2;
		} else {
			if (BurnLoadRom(DrvSndROM  + 0x00000,   12, 1)) return 1;
			if (BurnLoadRom(DrvSndROM  + 0x20000,   13, 1)) return 1;
		}

		DrvGfxDecode();
	}

	HD6309Init(0);
	HD6309Open(0);
	HD6309MapMemory(DrvShareRAM,		0x0000, 0x1fff, MAP_RAM);
	HD6309MapMemory(DrvFgRAM,		0x2000, 0x27ff, MAP_RAM);
	HD6309MapMemory(DrvBgRAM,		0x2800, 0x2fff, MAP_RAM);
	HD6309MapMemory(DrvPalRAM,		0x3000, 0x31ff, MAP_ROM);
	HD6309MapMemory(DrvPalRAM   + 0x00200,	0x3400, 0x35ff, MAP_ROM);
	HD6309MapMemory(DrvSprRAM,		0x3800, 0x39ff, MAP_RAM);
	HD6309MapMemory(DrvMainROM + 0x00000,	0x4000, 0x7fff, MAP_ROM);
	HD6309MapMemory(DrvMainROM + 0x18000,	0x8000, 0xffff, MAP_ROM);
	HD6309SetWriteHandler(chinagat_main_write);
	HD6309SetReadHandler(chinagat_main_read);
	HD6309Close();

	HD6309Init(1);
	HD6309Open(1);
	HD6309MapMemory(DrvShareRAM,            0x0000, 0x1fff, MAP_RAM);
	HD6309MapMemory(DrvSubROM + 0x00000,	0x4000, 0x7fff, MAP_ROM);
	HD6309MapMemory(DrvSubROM + 0x18000,	0x8000, 0xffff, MAP_ROM);
	HD6309SetWriteHandler(chinagat_sub_write);
	HD6309Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(chinagat_sound_write);
	ZetSetReadHandler(chinagat_sound_read);
	ZetClose();

	if (is_bootleg == 2) {
		ZetOpen(0);
		ZetSetWriteHandler(chinagat_bootleg2_sound_write);
		ZetSetReadHandler(chinagat_bootleg2_sound_read);
		ZetClose();

		BurnYM2203Init(2, 3579545, &DrvYM2203IRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
		BurnTimerAttachZet(3579545);
		BurnYM2203SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);
		BurnYM2203SetAllRoutes(1, 0.50, BURN_SND_ROUTE_BOTH);
	}
	else
	{
		BurnYM2151Init(3579545);
		BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.80, BURN_SND_ROUTE_LEFT);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.80, BURN_SND_ROUTE_RIGHT);

		MSM6295Init(0, 1065000 / 132, 1);
		MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);
	}

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	HD6309Exit();
	ZetExit();

	if (is_bootleg == 2) {
		BurnYM2203Exit();
	} else {
		BurnYM2151Exit();
		MSM6295Exit(0);
	}
	
	GenericTilesExit();
	
	BurnFree(AllMem);

	is_bootleg = 0;

	return 0;
}

static void draw_bg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 col = offs & 0x1f;
		INT32 row = offs / 32;
		INT32 ofst = (col & 0x0f) + ((row & 0x0f) << 4) + ((col & 0x10) << 4) + ((row & 0x10) << 5);

		INT32 sx = (col * 16) - scrollx;
		INT32 sy = ((row * 16) - scrolly) - 8;
		if (sx < -15) sx += 512;
		if (sy < -15) sy += 512;

		INT32 attr  = DrvBgRAM[ofst * 2 + 0];
		INT32 code  = DrvBgRAM[ofst * 2 + 1] + ((attr & 0x07) << 8);
		INT32 color = (attr >> 3) & 0x07;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;

		if (flipy) {
			if (flipx) {
				Render16x16Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM2);
			} else {
				Render16x16Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render16x16Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM2);
			} else {
				Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0x100, DrvGfxROM2);
			}
		}
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = ((offs & 0x1f) * 8);
		INT32 sy = ((offs / 0x20) * 8) - 8;

		INT32 attr  = DrvFgRAM[offs * 2 + 0];
		INT32 code  = DrvFgRAM[offs * 2 + 1] + ((attr & 0x0f) << 8);
		INT32 color = attr >> 4;

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, 0x000, DrvGfxROM0);
	}
}

#define DRAW_SPRITE( order, sx, sy )	\
	if (flipy) {	\
		if (flipx) {	\
			Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, which+order, sx, sy, color, 4, 0, 0x080, DrvGfxROM1);	\
		} else {	\
			Render16x16Tile_Mask_FlipY_Clip(pTransDraw, which+order, sx, sy, color, 4, 0, 0x080, DrvGfxROM1);	\
		}	\
	} else {	\
		if (flipx) {	\
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, which+order, sx, sy, color, 4, 0, 0x080, DrvGfxROM1);	\
		} else {	\
			Render16x16Tile_Mask_Clip(pTransDraw, which+order, sx, sy, color, 4, 0, 0x080, DrvGfxROM1);	\
		}	\
	}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x180; i += 5)
	{
		INT32 attr = DrvSprRAM[i + 1];

		if (attr & 0x80)
		{
			INT32 sx = 240 - DrvSprRAM[i + 4] + ((attr & 2) << 7);
			INT32 sy = 232 - DrvSprRAM[i + 0] + ((attr & 1) << 8);
			INT32 size = (attr & 0x30) >> 4;
			INT32 flipx = attr & 8;
			INT32 flipy = attr & 4;
			INT32 dx = -16, dy = -16;
			INT32 color = DrvSprRAM[i + 2] >> 4;
			INT32 which = DrvSprRAM[i + 3] + ((DrvSprRAM[i + 2] & 0x0f) << 8);

			if ((sx < -7) && (sx > -16)) sx += 256;
			if ((sy < -7) && (sy > -16)) sy += 256;

			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - 16 - sy;
				flipx = !flipx;
				flipy = !flipy;
				dx = -dx;
				dy = -dy;
			}

			which &= ~size;

			switch (size)
			{
				case 0: // 1x1
					DRAW_SPRITE(0, sx, sy);
				break;

				case 1: // 1x2
					DRAW_SPRITE(0, sx, sy + dy);
					DRAW_SPRITE(1, sx, sy);
				break;

				case 2: // 2x1
					DRAW_SPRITE(0, sx + dx, sy);
					DRAW_SPRITE(2, sx, sy);
				break;

				case 3: // 2x2
					DRAW_SPRITE(0, sx + dx, sy + dy);
					DRAW_SPRITE(1, sx + dx, sy);
					DRAW_SPRITE(2, sx, sy + dy);
					DRAW_SPRITE(3, sx, sy);
				break;
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x180; i++) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	//BurnTransferClear();

	if (nBurnLayer & 2) draw_bg_layer();
	if (nBurnLayer & 4) draw_sprites();
	if (nBurnLayer & 8) draw_fg_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static inline INT32 scanline_to_vcount(INT32 scanline) // ripped directly from MAME
{
	INT32 vcount = scanline + 8;

	if (vcount < 0x100)
		return vcount;
	else
		return (vcount - 0x18) | 0x100;
}

static void scanline_timer(INT32 param)
{
	int scanline = param;
	int screen_height = 240;
	int vcount_old = scanline_to_vcount((scanline == 0) ? screen_height - 1 : scanline - 1);
	int vcount = scanline_to_vcount(scanline);

	if (vcount == 0xf8) {
		HD6309SetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		vblank = 1;
	}

	if (!(vcount_old & 8) && (vcount & 8))
		HD6309SetIRQLine(0x01, CPU_IRQSTATUS_ACK);
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

	INT32 nSegment;
	INT32 nInterleave = 256;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[3] = { 1500000 * 4 / 60, 1500000 * 4 / 60, 3579545 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	HD6309NewFrame();
	ZetNewFrame();
	
	ZetOpen(0);
	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = nCyclesTotal[0] / nInterleave;

		HD6309Open(0);
		nCyclesDone[0] += HD6309Run(nSegment);
		scanline_timer(i);
		HD6309Close();

		nSegment = nCyclesTotal[1] / nInterleave;

		HD6309Open(1);
		nCyclesDone[1] += HD6309Run(nSegment);
		HD6309Close();

		nSegment = nCyclesTotal[2] / nInterleave;
		nCyclesDone[2] += ZetRun(nSegment);

		if (pBurnSoundOut) {
			nSegment = nBurnSoundLen / nInterleave;

			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			MSM6295Render(0, pBurnSoundOut + (nSoundBufferPos << 1), nSegment);

			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			MSM6295Render(0, pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvBoot2Frame()
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

	INT32 nSegment;
	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 1500000 * 4 / 60, 1500000 * 4 / 60, 3579545 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	HD6309NewFrame();
	ZetNewFrame();
	
	ZetOpen(0);
	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = nCyclesTotal[0] / nInterleave;

		HD6309Open(0);
		nCyclesDone[0] += HD6309Run(nSegment);
		scanline_timer(i);
		HD6309Close();

		nSegment = nCyclesTotal[1] / nInterleave;

		HD6309Open(1);
		nCyclesDone[1] += HD6309Run(nSegment);
		HD6309Close();

		BurnTimerUpdate(i * (nCyclesTotal[2] / nInterleave));
	}

	BurnTimerEndFrame(nCyclesTotal[2]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
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

		HD6309Scan(nAction);
		ZetScan(nAction);

		if (is_bootleg == 2) {
			BurnYM2203Scan(nAction, pnMin);
		} else {
			BurnYM2151Scan(nAction);
			MSM6295Scan(0, nAction);
		}

		SCAN_VAR(bankdata[0]);
		SCAN_VAR(bankdata[1]);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
	}

	if (nAction & ACB_WRITE)
	{
		HD6309Open(0);
		bankswitch1(bankdata[0]);
		HD6309Close();

		HD6309Open(1);
		bankswitch2(bankdata[1]);
		HD6309Close();
	}

	return 0;
}



// China Gate (US)

static struct BurnRomInfo chinagatRomDesc[] = {
	{ "cgate51.bin",	0x20000, 0x439a3b19, 0 | BRF_PRG | BRF_ESS }, //  0 HD6309 #0 Code

	{ "23j4-0.48",		0x20000, 0x2914af38, 1 | BRF_PRG | BRF_ESS }, //  1 HD6309 #1 Code

	{ "23j0-0.40", 		0x08000, 0x9ffcadb6, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "cgate18.bin",	0x20000, 0x8d88d64d, 3 | BRF_GRA },           //  3 Foreground Tiles

	{ "23j7-0.103",		0x20000, 0x2f445030, 4 | BRF_GRA },           //  4 Sprites
	{ "23j8-0.102",		0x20000, 0x237f725a, 4 | BRF_GRA },           //  5
	{ "23j9-0.101",		0x20000, 0x8caf6097, 4 | BRF_GRA },           //  6
	{ "23ja-0.100",		0x20000, 0xf678594f, 4 | BRF_GRA },           //  7

	{ "chinagat_a-13",	0x10000, 0xb745cac4, 5 | BRF_GRA },           //  8 Background Tiles
	{ "chinagat_a-12",	0x10000, 0x3c864299, 5 | BRF_GRA },           //  9
	{ "chinagat_a-15",	0x10000, 0x2f268f37, 5 | BRF_GRA },           // 10
	{ "chinagat_a-14",	0x10000, 0xaef814c8, 5 | BRF_GRA },           // 11

	{ "23j1-0.53",		0x20000, 0xf91f1001, 6 | BRF_SND },           // 12 MSM6295 Samples
	{ "23j2-0.52",		0x20000, 0x8b6f26e9, 6 | BRF_SND },           // 13 

	{ "23jb-0.16",		0x00200, 0x46339529, 7 | BRF_OPT },           // 14 Proms (unused)
	{ "23j5-0.45",		0X00100, 0xfdb130a9, 7 | BRF_OPT },           // 15
};

STD_ROM_PICK(chinagat)
STD_ROM_FN(chinagat)

struct BurnDriver BurnDrvChinagat = {
	"chinagat", NULL, NULL, NULL, "1988",
	"China Gate (US)\0", NULL, "Technos Japan (Taito Romstar license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, chinagatRomInfo, chinagatRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	256, 240, 4, 3
};


// Sai Yu Gou Ma Roku (Japan)

static struct BurnRomInfo saiyugouRomDesc[] = {
	{ "23j3-0.51",		0x20000, 0xaa8132a2, 0 | BRF_PRG | BRF_ESS }, //  0 HD6309 #0 Code

	{ "23j4-0.48",		0x20000, 0x2914af38, 1 | BRF_PRG | BRF_ESS }, //  1 HD6309 #1 Code
	
	{ "23j0-0.40",		0x08000, 0x9ffcadb6, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "23j6-0.18",		0x20000, 0x86d33df0, 3 | BRF_GRA },           //  3 Foreground Tiles

	{ "23j7-0.103",		0x20000, 0x2f445030, 4 | BRF_GRA },           //  4 Sprites
	{ "23j8-0.102",		0x20000, 0x237f725a, 4 | BRF_GRA },           //  5
	{ "23j9-0.101",		0x20000, 0x8caf6097, 4 | BRF_GRA },           //  6
	{ "23ja-0.100",		0x20000, 0xf678594f, 4 | BRF_GRA },           //  7

	{ "saiyugou_a-13",	0x10000, 0xb745cac4, 5 | BRF_GRA },           //  8 Background Tiles set instead
	{ "saiyugou_a-12",	0x10000, 0x3c864299, 5 | BRF_GRA },           //  9
	{ "saiyugou_a-15",	0x10000, 0x2f268f37, 5 | BRF_GRA },           // 10
	{ "saiyugou_a-14",	0x10000, 0xaef814c8, 5 | BRF_GRA },           // 11

	{ "23j1-0.53",		0x20000, 0xf91f1001, 6 | BRF_SND },           // 12 MSM6295 Samples
	{ "23j2-0.52",		0x20000, 0x8b6f26e9, 6 | BRF_SND },           // 13

	{ "23jb-0.16",		0x00200, 0x46339529, 7 | BRF_OPT },           // 14 Proms (unused)
	{ "23j5-0.45",		0x00100, 0xfdb130a9, 7 | BRF_OPT },           // 15
};

STD_ROM_PICK(saiyugou)
STD_ROM_FN(saiyugou)

struct BurnDriver BurnDrvSaiyugou = {
	"saiyugou", "chinagat", NULL, NULL, "1988",
	"Sai Yu Gou Ma Roku (Japan)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, saiyugouRomInfo, saiyugouRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	256, 240, 4, 3
};


// Sai Yu Gou Ma Roku (Japan bootleg 1)

static struct BurnRomInfo saiyugoub1RomDesc[] = {
	{ "23j3-0.51",		0x20000, 0xaa8132a2, 0 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "23j4-0.48",		0x20000, 0x2914af38, 1 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "a-1.bin",		0x08000, 0x46e5a6d4, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "23j6-0.18",		0x20000, 0x86d33df0, 3 | BRF_GRA },           //  3 Foreground Tiles

	{ "23j7-0.103",		0x20000, 0x2f445030, 4 | BRF_GRA },           //  4 Sprites
	{ "23j8-0.102",		0x20000, 0x237f725a, 4 | BRF_GRA },           //  5
	{ "23j9-0.101",		0x20000, 0x8caf6097, 4 | BRF_GRA },           //  6
	{ "23ja-0.100",		0x20000, 0xf678594f, 4 | BRF_GRA },           //  7

	{ "a-13",		0x10000, 0xb745cac4, 5 | BRF_GRA },           //  8 gfx3
	{ "a-12",		0x10000, 0x3c864299, 5 | BRF_GRA },           //  9
	{ "a-15",		0x10000, 0x2f268f37, 5 | BRF_GRA },           // 10
	{ "a-14",		0x10000, 0xaef814c8, 5 | BRF_GRA },           // 11

	{ "a-6.bin",		0x10000, 0x4da4e935, 6 | BRF_SND },           // 12 MSM5205 Samples
	{ "a-7.bin",		0x10000, 0x6284c254, 6 | BRF_SND },           // 13
	{ "a-10.bin",		0x10000, 0xb728ec6e, 6 | BRF_SND },           // 14
	{ "a-11.bin",		0x10000, 0xa50d1895, 6 | BRF_SND },           // 15

	{ "23jb-0.16",		0x00200, 0x46339529, 7 | BRF_OPT },           // 16 Proms (unused)
	{ "23j5-0.45",		0x00100, 0xfdb130a9, 7 | BRF_OPT },           // 17

	{ "mcu8748.bin",	0x00400, 0x6d28d6c5, 8 | BRF_PRG | BRF_ESS }, // 18 I8748 Code
};

STD_ROM_PICK(saiyugoub1)
STD_ROM_FN(saiyugoub1)

struct BurnDriver BurnDrvSaiyugoub1 = {
	"saiyugoub1", "chinagat", NULL, NULL, "1988",
	"Sai Yu Gou Ma Roku (Japan bootleg 1)\0", "missing some sounds", "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, saiyugoub1RomInfo, saiyugoub1RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	256, 240, 4, 3
};


// Sai Yu Gou Ma Roku (Japan bootleg 2)

static struct BurnRomInfo saiyugoub2RomDesc[] = {
	{ "23j3-0.51",		0x20000, 0xaa8132a2, 0 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "23j4-0.48",		0x20000, 0x2914af38, 1 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "sai-alt1.bin",	0x08000, 0x8d397a8d, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "23j6-0.18",		0x20000, 0x86d33df0, 3 | BRF_GRA },           //  3 Foreground Tiles

	{ "23j7-0.103",		0x20000, 0x2f445030, 4 | BRF_GRA },           //  4 Sprites
	{ "23j8-0.102",		0x20000, 0x237f725a, 4 | BRF_GRA },           //  5
	{ "23j9-0.101",		0x20000, 0x8caf6097, 4 | BRF_GRA },           //  6
	{ "23ja-0.100",		0x20000, 0xf678594f, 4 | BRF_GRA },           //  7

	{ "a-13",		0x10000, 0xb745cac4, 5 | BRF_GRA },           //  8 gfx3
	{ "a-12",		0x10000, 0x3c864299, 5 | BRF_GRA },           //  9
	{ "a-15",		0x10000, 0x2f268f37, 5 | BRF_GRA },           // 10
	{ "a-14",		0x10000, 0xaef814c8, 5 | BRF_GRA },           // 11

	{ "a-6.bin",		0x10000, 0x4da4e935, 6 | BRF_SND },           // 12 MSM5205 Samples
	{ "a-7.bin",		0x10000, 0x6284c254, 6 | BRF_SND },           // 13
	{ "a-10.bin",		0x10000, 0xb728ec6e, 6 | BRF_SND },           // 14
	{ "a-11.bin",		0x10000, 0xa50d1895, 6 | BRF_SND },           // 15

	{ "23jb-0.16",		0x00200, 0x46339529, 7 | BRF_OPT },           // 16 Proms (unused)
	{ "23j5-0.45",		0x00100, 0xfdb130a9, 7 | BRF_OPT },           // 17
};

STD_ROM_PICK(saiyugoub2)
STD_ROM_FN(saiyugoub2)

struct BurnDriver BurnDrvSaiyugoub2 = {
	"saiyugoub2", "chinagat", NULL, NULL, "1988",
	"Sai Yu Gou Ma Roku (Japan bootleg 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, saiyugoub2RomInfo, saiyugoub2RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvBoot2Frame, DrvDraw, DrvScan, &DrvRecalc, 0x180,
	256, 240, 4, 3
};
