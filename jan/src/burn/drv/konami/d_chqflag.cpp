// FB Alpha Chequered Flag driver module
// Based on MAME driver by Nicola Salmoria and Manuel Abadia

#include "tiles_generic.h"
#include "z80_intf.h"
#include "konami_intf.h"
#include "konamiic.h"
#include "burn_ym2151.h"
#include "k007232.h"
#include "burn_shift.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvKonROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROMExp0;
static UINT8 *DrvGfxROMExp1;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvKonRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *soundlatch;
static UINT8 *soundlatch1;

static UINT8 DrvInputs[3];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvReset;

static INT32 nNmiEnable;
static INT32 nDrvRomBank;
static INT32 nDrvRamBank;
static INT32 nBackgroundBrightness;
static INT32 k051316_readroms;
static INT32 analog_ctrl;
static INT32 DrvAnalogPort0 = 0;
static INT32 DrvAnalogPort1 = 0;
static UINT8 accelerator;
static UINT8 steeringwheel;

static INT32 watchdog;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo ChqflagInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},

	A("Wheel",          BIT_ANALOG_REL, &DrvAnalogPort0 , "mouse x-axis"),
	A("Accelerator",    BIT_ANALOG_REL, &DrvAnalogPort1 , "p1 fire 1"),

	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Chqflag)
#undef A

static struct BurnDIPInfo ChqflagDIPList[]=
{
	{0x08, 0xff, 0xff, 0xff, NULL			},
	{0x09, 0xff, 0xff, 0x5f, NULL			},
	{0x0a, 0xff, 0xff, 0xe0, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x08, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x08, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x08, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x08, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x08, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x08, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x08, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x08, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x08, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x08, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x08, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x08, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x08, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x08, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x08, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x08, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x08, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x08, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x08, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x08, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x08, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x08, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x08, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x08, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x08, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x08, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x08, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x08, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x08, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x08, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x08, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x08, 0x01, 0xf0, 0x00, "Invalid"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x09, 0x01, 0x60, 0x60, "Easy"			},
	{0x09, 0x01, 0x60, 0x40, "Normal"		},
	{0x09, 0x01, 0x60, 0x20, "Difficult"		},
	{0x09, 0x01, 0x60, 0x00, "Very Difficult"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x09, 0x01, 0x80, 0x80, "Off"			},
	{0x09, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Title"		},
	{0x0a, 0x01, 0x40, 0x40, "Chequered Flag"	},
	{0x0a, 0x01, 0x40, 0x00, "Checkered Flag"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0a, 0x01, 0x80, 0x80, "Off"			},
	{0x0a, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Chqflag)

static void bankswitch(INT32 data)
{
	nDrvRomBank = data & 0x1f;

	if (nDrvRomBank < (0x50000 / 0x4000))
		konamiMapMemory(DrvKonROM + (nDrvRomBank * 0x4000), 0x4000, 0x7fff, MAP_ROM);
}

static void chqflag_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x1000) {
		if (nDrvRamBank) {
			if (address & 0x800) {
				DrvPalRAM[address & 0x7ff] = data;
				return;
			} else {
				K051316Write(0, address & 0x7ff, data);
				return;
			}
		} else {
			DrvKonRAM[address] = data;
			return;
		}
	}

	if ((address & 0xfff8) == 0x2000) {
		if (address == 0x2000) {
			if (data & 0x01) konamiSetIrqLine(0, CPU_IRQSTATUS_NONE);
			if (data & 0x04) konamiSetIrqLine(0x20, CPU_IRQSTATUS_NONE);
			nNmiEnable = data & 0x04;
		}

		K051937Write(address & 7, data);
		return;
	}

	if ((address & 0xfc00) == 0x2400) {
		K051960Write(address & 0x3ff, data);
		return;
	}

	if ((address & 0xf800) == 0x2800) {
		K051316Write(1, address & 0x7ff, data);
		return;
	}

	if ((address & 0xffe0) == 0x3400) {
		K051733Write(address & 0x1f, data);
		return;
	}

	if ((address & 0xfff0) == 0x3500) {
		K051316WriteCtrl(0, address & 0x0f, data);
		return;
	}

	if ((address & 0xfff0) == 0x3600) {
		K051316WriteCtrl(1, address & 0x0f, data);
		return;
	}

	switch (address)
	{
		case 0x3000:
			*soundlatch = data;
		return;

		case 0x3001:
			*soundlatch1 = data;
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x3002:
			nDrvRamBank = data & 0x20;
			bankswitch(data);
		return;

		case 0x3003:
		{
			nBackgroundBrightness = (data & 0x80) ? 60 : 100;

			konami_set_highlight_mode((data & 0x08) ? 1 : 0);

			k051316_readroms = data & 0x10;
		}
		return;

		case 0x3300:
			watchdog = 0;
		return;

		case 0x3700:
		case 0x3702:
			analog_ctrl = data & 3;
		return;
	}
}

static inline UINT8 analog_port_read()
{
	switch (analog_ctrl)
	{
		case 0x00: {
			accelerator = DrvAnalogPort1;
			return accelerator;
		}
		case 0x01: {
			steeringwheel = 0x7f + (DrvAnalogPort0 >> 4);
			return steeringwheel;
		}
		case 0x02: return accelerator; // 0x02,0x03: previous reads
		case 0x03: return steeringwheel;
	}

	return 0xff;
}

static UINT8 chqflag_main_read(UINT16 address)
{
	if ((address & 0xf000) == 0x1000) {
		if (nDrvRamBank) {
			if (address & 0x800) {
				return DrvPalRAM[address & 0x7ff];
			} else {
				if (k051316_readroms) {
					return K051316ReadRom(0, address & 0x7ff);
				} else {
					return K051316Read(0, address & 0x7ff);
				}
			}
		} else {
			return DrvKonRAM[address];
		}
	}

	if ((address & 0xffe0) == 0x3400) {
		return K051733Read(address & 0x1f);
	}

	if ((address & 0xfff8) == 0x2000) {
		return K051937Read(address & 7);
	}

	if ((address & 0xfc00) == 0x2400) {
		return K051960Read(address & 0x3ff);
	}

	if ((address & 0xf800) == 0x2800) {
		if (k051316_readroms) {
			return K051316ReadRom(1, address & 0x7ff);
		} else {
			return K051316Read(1, address & 0x7ff);
		}
	}

	switch (address)
	{
		case 0x3100:
			return DrvDips[0];

		case 0x3200:
			return (DrvInputs[0] & 0x1f) | (DrvDips[2] & 0xe0);

		case 0x3201:
			return 0xff;

		case 0x3203:
			return DrvDips[1];

		case 0x3701:
			return DrvInputs[1] & 0x0f;

		case 0x3702:
			return analog_port_read();
	}

	return 0;
}

static void __fastcall chqflag_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0xa000) {
		K007232WriteReg(0, address & 0x0f, data);
		return;
	}

	if ((address & 0xfff0) == 0xb000) {
		K007232WriteReg(1, address & 0x0f, data);
		return;
	}

	switch (address)
	{
		case 0x9000:
			k007232_set_bank(0, (data >> 4) & 3, (data >> 6) & 3 );
			k007232_set_bank(1, (data >> 0) & 3, (data >> 2) & 3 );
		return;

		case 0xa01c:
			K007232SetVolume(0, 1, (data & 0x0f) * 0x11/2, (data >> 4) * 0x11/2);
		return;

		case 0xc000:
			BurnYM2151SelectRegister(data);
		return;

		case 0xc001:
			BurnYM2151WriteRegister(data);
		return;

		case 0xf000:
		return; // nop
	}
}

static UINT8 __fastcall chqflag_sound_read(UINT16 address)
{
	if ((address & 0xfff0) == 0xa000) {
		return K007232ReadReg(0, address & 0x0f);
	}

	if ((address & 0xfff0) == 0xb000) {
		return K007232ReadReg(1, address & 0x0f);
	}

	switch (address)
	{
		case 0xc000:
		case 0xc001:
			return BurnYM2151ReadStatus();

		case 0xd000:
			return *soundlatch;

		case 0xe000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch1;
	}

	return 0;
}

static void DrvYM2151IrqHandler(INT32 state)
{
	if (state) ZetNmi();
}

static void DrvK007232VolCallback0(INT32 v)
{
	K007232SetVolume(0, 0, (v & 0x0f) * 0x11/2, (v & 0x0f) * 0x11/2);
}

static void DrvK007232VolCallback1(INT32 v)
{
	K007232SetVolume(1, 0, (v >> 0x4) * 0x11, 0);
	K007232SetVolume(1, 1, 0, (v & 0x0f) * 0x11);
}

static void K051316Callback0(INT32 *code,INT32 *color,INT32 *)
{
	*code |= ((*color & 0x03) << 8);
	*color = (256 / 16) + ((*color & 0x3c) >> 2);
}

static void K051316Callback1(INT32 *code,INT32 *color,INT32 *flags)
{
	*flags = (*color & 0xc0) >> 6;
	*code |= ((*color & 0x0f) << 8);
	*color = (512 / 256) + ((*color & 0x10) >> 4);
}

static void K051960Callback(INT32 *, INT32 *color, INT32 *priority, INT32 *)
{
	*priority = (*color & 0x10) ? 0 : 0xaaaa;
	*color = (*color & 0x0f);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	konamiOpen(0);
	konamiReset();
	bankswitch(0);
	konamiClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	BurnYM2151Reset();

	KonamiICReset();
	K051316WrapEnable(1, 1);

	nDrvRamBank = 0;
	k051316_readroms = 0;
	analog_ctrl = 0;
	nNmiEnable = 0;

	watchdog = 0;
	BurnShiftReset();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvKonROM		= Next; Next += 0x050000;
	DrvZ80ROM		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x100000;
	DrvGfxROM1		= Next; Next += 0x020000;
	DrvGfxROM2		= Next; Next += 0x100000;
	DrvGfxROMExp0		= Next; Next += 0x200000;
	DrvGfxROMExp1		= Next; Next += 0x040000;

	DrvSndROM0		= Next; Next += 0x080000;
	DrvSndROM1		= Next; Next += 0x080000;

	DrvPalette		= (UINT32*)Next; Next += 0x401 * sizeof(UINT32);

	AllRam			= Next;

	DrvKonRAM		= Next; Next += 0x002000;
	DrvPalRAM		= Next; Next += 0x000800;

	DrvZ80RAM		= Next; Next += 0x000800;

	soundlatch		= Next; Next += 0x000001;
	soundlatch1		= Next; Next += 0x000001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvKonROM  + 0x000000,  0, 1)) return 1;
		if (BurnLoadRom(DrvKonROM  + 0x040000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000, 3, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002, 4, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x040000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x080000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x0c0000,  9, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 10, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 11, 1)) return 1;

		K051960GfxDecode(DrvGfxROM0, DrvGfxROMExp0, 0x100000);
	}

	konamiInit(0);
	konamiOpen(0);
	konamiMapMemory(DrvKonRAM,		0x0000, 0x0fff, MAP_RAM);
	konamiMapMemory(DrvKonROM + 0x00000, 	0x4000, 0x7fff, MAP_ROM);
	konamiMapMemory(DrvKonROM + 0x48000,	0x8000, 0xffff, MAP_ROM);
	konamiSetWriteHandler(chqflag_main_write);
	konamiSetReadHandler(chqflag_main_read);
	konamiClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(chqflag_sound_write);
	ZetSetReadHandler(chqflag_sound_read);
	ZetClose();

	BurnYM2151Init(3579545);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	K007232Init(0, 3579545, DrvSndROM0, 0x80000);
	K007232SetPortWriteHandler(0, DrvK007232VolCallback0);
	K007232PCMSetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);

	K007232Init(1, 3579545, DrvSndROM1, 0x80000);
	K007232SetPortWriteHandler(1, DrvK007232VolCallback1);
	K007232PCMSetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);

	K051960Init(DrvGfxROM0, DrvGfxROMExp0, 0xfffff);
	K051960SetCallback(K051960Callback);
	K051960SetSpriteOffset(-8, 0);

	K051316Init(0, DrvGfxROM1, DrvGfxROMExp1, 0x1ffff, K051316Callback0, 4, 0);
	K051316SetOffset(0, -89, -16);

	K051316Init(1, DrvGfxROM2, DrvGfxROM2, 0xfffff, K051316Callback1, 8, 0xc0 | 0x200);
	K051316SetOffset(1, -96, -16);

	konami_set_highlight_over_sprites_mode(1);

	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_GREEN, 80);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	KonamiICExit();

	konamiExit();
	ZetExit();

	K007232Exit();
	BurnYM2151Exit();

	BurnShiftExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	konami_palette32 = DrvPalette;

	UINT8 r,g,b;
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x800 / 2; i++) {
		UINT16 d = BURN_ENDIAN_SWAP_INT16((p[i] << 8) | (p[i] >> 8));

		r = (d >>  0) & 0x1f;
		g = (d >>  5) & 0x1f;
		b = (d >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		if (i >= 0x200) {
			r = (r * nBackgroundBrightness) / 100;
			g = (g * nBackgroundBrightness) / 100;
			b = (b * nBackgroundBrightness) / 100;
		}

		DrvPalette[i] = (r << 16) | (g << 8) | b; // 32-bit colors
	}
}

static INT32 DrvDraw()
{
	DrvPaletteInit();

	BurnTransferClear();
	KonamiClearBitmaps(0);

	if (nBurnLayer & 1) K051316_zoom_draw(1, 0 | 0x200);

	if (nBurnLayer & 2) K051316_zoom_draw(1, 1);

	if (nSpriteEnable & 1) K051960SpritesRender(-1, -1);

	if (nBurnLayer & 4) K051316_zoom_draw(0, 0);

	KonamiBlendCopy(DrvPalette);

	BurnShiftRender();

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog > 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		{ // gear shifter stuff
			BurnShiftInputCheckToggle(DrvJoy2[0]);

			DrvInputs[1] &= ~1;
			DrvInputs[1] |= !bBurnShiftStatus;
		}
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 256/2;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetOpen(0);
	konamiOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = (nCyclesTotal[0] / nInterleave) * (i + 1);
		nCyclesDone[0] += konamiRun(nSegment - nCyclesDone[0]);

		nSegment = (nCyclesTotal[1] / nInterleave) * (i + 1);
		nCyclesDone[1] += ZetRun(nSegment - nCyclesDone[1]);

		if ((i&0xf)==0 && nNmiEnable) konamiSetIrqLine(0x20, CPU_IRQSTATUS_ACK); // iq_132 fix me!
		if (i == 120 && K051960_irq_enabled) konamiSetIrqLine(KONAMI_IRQ_LINE, CPU_IRQSTATUS_ACK);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K007232Update(0, pSoundBuf, nSegmentLength);
			K007232Update(1, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		if (nSegmentLength) {
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K007232Update(0, pSoundBuf, nSegmentLength);
			K007232Update(1, pSoundBuf, nSegmentLength);
		}
	}

	konamiClose();
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
		*pnMin = 0x029705;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		konamiCpuScan(nAction);
		ZetScan(nAction);

		BurnYM2151Scan(nAction);
		K007232Scan(nAction, pnMin);

		KonamiICScan(nAction);

		BurnShiftScan(nAction);

		SCAN_VAR(nDrvRomBank);
		SCAN_VAR(nDrvRamBank);
		SCAN_VAR(k051316_readroms);
		SCAN_VAR(analog_ctrl);
		SCAN_VAR(nNmiEnable);
		SCAN_VAR(nBackgroundBrightness);
		SCAN_VAR(accelerator);
		SCAN_VAR(steeringwheel);
	}

	if (nAction & ACB_WRITE) {
		konamiOpen(0);
		bankswitch(nDrvRomBank);
		konamiClose();

	}

	return 0;
}


// Chequered Flag

static struct BurnRomInfo chqflagRomDesc[] = {
	{ "717e10",	0x40000, 0x72fc56f6, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code
	{ "717h02",	0x10000, 0xf5bd4e78, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "717e01",	0x08000, 0x966b8ba8, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "717e04",	0x80000, 0x1a50a1cc, 3 | BRF_GRA },           //  3 k051960 Sprites
	{ "717e05",	0x80000, 0x46ccb506, 3 | BRF_GRA },           //  4

	{ "717e06.n16",	0x20000, 0x1ec26c7a, 4 | BRF_GRA },           //  5 k051316 #0 Zoom Tiles

	{ "717e07.l20",	0x40000, 0xb9a565a8, 5 | BRF_GRA },           //  6 k051316 #1 Zoom Tiles
	{ "717e08.l22",	0x40000, 0xb68a212e, 5 | BRF_GRA },           //  7
	{ "717e11.n20",	0x40000, 0xebb171ec, 5 | BRF_GRA },           //  8
	{ "717e12.n22",	0x40000, 0x9269335d, 5 | BRF_GRA },           //  9

	{ "717e03",	0x80000, 0xebe73c22, 6 | BRF_GRA },           // 10 k007232 #0 Samples

	{ "717e09",	0x80000, 0xd74e857d, 7 | BRF_GRA },           // 11 k007232 #1 Samples
};

STD_ROM_PICK(chqflag)
STD_ROM_FN(chqflag)

struct BurnDriver BurnDrvChqflag = {
	"chqflag", NULL, NULL, NULL, "1988",
	"Chequered Flag\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 1, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, chqflagRomInfo, chqflagRomName, NULL, NULL, ChqflagInputInfo, ChqflagDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 304, 3, 4
};


// Chequered Flag (Japan)

static struct BurnRomInfo chqflagjRomDesc[] = {
	{ "717e10",	0x40000, 0x72fc56f6, 1 | BRF_PRG | BRF_ESS }, //  0 Konami Custom Code
	{ "717j02.bin",	0x10000, 0x05355daa, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "717e01",	0x08000, 0x966b8ba8, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "717e04",	0x80000, 0x1a50a1cc, 3 | BRF_GRA },           //  3 k051960 Sprites
	{ "717e05",	0x80000, 0x46ccb506, 3 | BRF_GRA },           //  4

	{ "717e06.n16",	0x20000, 0x1ec26c7a, 4 | BRF_GRA },           //  5 k051316 #0 Zoom Tiles

	{ "717e07.l20",	0x40000, 0xb9a565a8, 5 | BRF_GRA },           //  6 k051316 #1 Zoom Tiles
	{ "717e08.l22",	0x40000, 0xb68a212e, 5 | BRF_GRA },           //  7
	{ "717e11.n20",	0x40000, 0xebb171ec, 5 | BRF_GRA },           //  8
	{ "717e12.n22",	0x40000, 0x9269335d, 5 | BRF_GRA },           //  9

	{ "717e03",	0x80000, 0xebe73c22, 6 | BRF_GRA },           // 10 k007232 #0 Samples

	{ "717e09",	0x80000, 0xd74e857d, 7 | BRF_GRA },           // 11 k007232 #1 Samples
};

STD_ROM_PICK(chqflagj)
STD_ROM_FN(chqflagj)

struct BurnDriver BurnDrvChqflagj = {
	"chqflagj", "chqflag", NULL, NULL, "1988",
	"Chequered Flag (Japan)\0", NULL, "Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 1, HARDWARE_PREFIX_KONAMI, GBF_RACING, 0,
	NULL, chqflagjRomInfo, chqflagjRomName, NULL, NULL, ChqflagInputInfo, ChqflagDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	224, 304, 3, 4
};
