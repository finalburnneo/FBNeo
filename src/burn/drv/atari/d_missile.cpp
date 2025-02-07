// FinalBurn Neo Atari Missile Command driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "burn_gun.h"
#include "watchdog.h"
#include "pokey.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvWritePROM;
static UINT8 *DrvVideoRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 flipscreen;
static INT32 ctrld;
static INT32 irq_state;
static UINT32 madsel_lastcycles;
static INT32 last_pokey_6_write;
static INT32 system_scanline;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT32 nExtraCycles[1];

static INT16 Analog0PortX = 0;
static INT16 Analog0PortY = 0;
static INT16 Analog1PortX = 0;
static INT16 Analog1PortY = 0;

static ButtonToggle service;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo MissileInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"		},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 3"		},
	A("P1 Trackball X", BIT_ANALOG_REL, &Analog0PortX,  "p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &Analog0PortY,  "p1 y-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 2"		},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 fire 3"		},
	A("P2 Trackball X", BIT_ANALOG_REL, &Analog1PortX,  "p2 x-axis"),
	A("P2 Trackball Y", BIT_ANALOG_REL, &Analog1PortY,  "p2 y-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 6,	"diag"			},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 5,	"tilt"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
};
#undef A
STDINPUTINFO(Missile)

static struct BurnDIPInfo MissileDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x80, NULL							},
	{0x01, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Coinage"						},
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0x03, 0x00, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  2 Credits"			},
	{0x00, 0x01, 0x03, 0x02, "Free Play"					},

	{0   , 0xfe, 0   ,    4, "Right Coin"					},
	{0x00, 0x01, 0x0c, 0x00, "*1"							},
	{0x00, 0x01, 0x0c, 0x04, "*4"							},
	{0x00, 0x01, 0x0c, 0x08, "*5"							},
	{0x00, 0x01, 0x0c, 0x0c, "*6"							},

	{0   , 0xfe, 0   ,    2, "Center Coin"					},
	{0x00, 0x01, 0x10, 0x00, "*1"							},
	{0x00, 0x01, 0x10, 0x10, "*2"							},

	{0   , 0xfe, 0   ,    4, "Language"						},
	{0x00, 0x01, 0x60, 0x00, "English"						},
	{0x00, 0x01, 0x60, 0x20, "French"						},
	{0x00, 0x01, 0x60, 0x40, "German"						},
	{0x00, 0x01, 0x60, 0x60, "Spanish"						},

	{0   , 0xfe, 0   ,    2, "Unknown"						},
	{0x00, 0x01, 0x80, 0x80, "Off"							},
	{0x00, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Cities"						},
	{0x01, 0x01, 0x03, 0x02, "4"							},
	{0x01, 0x01, 0x03, 0x01, "5"							},
	{0x01, 0x01, 0x03, 0x03, "6"							},
	{0x01, 0x01, 0x03, 0x00, "7"							},

	{0   , 0xfe, 0   ,    2, "Bonus Credit for 4 Coins"		},
	{0x01, 0x01, 0x04, 0x04, "No"							},
	{0x01, 0x01, 0x04, 0x00, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Trackball Size"				},
	{0x01, 0x01, 0x08, 0x00, "Mini"							},
	{0x01, 0x01, 0x08, 0x08, "Large"						},

	{0   , 0xfe, 0   ,    8, "Bonus City"					},
	{0x01, 0x01, 0x70, 0x10, "8000"							},
	{0x01, 0x01, 0x70, 0x70, "10000"						},
	{0x01, 0x01, 0x70, 0x60, "12000"						},
	{0x01, 0x01, 0x70, 0x50, "14000"						},
	{0x01, 0x01, 0x70, 0x40, "15000"						},
	{0x01, 0x01, 0x70, 0x30, "18000"						},
	{0x01, 0x01, 0x70, 0x20, "20000"						},
	{0x01, 0x01, 0x70, 0x00, "None"							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x01, 0x01, 0x80, 0x00, "Upright"						},
	{0x01, 0x01, 0x80, 0x80, "Cocktail"						},
};

STDDIPINFO(Missile)

static struct BurnDIPInfo SuprmatkDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x80, NULL							},
	{0x01, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    4, "Coinage"						},
	{0x00, 0x01, 0x03, 0x01, "2 Coins 1 Credits"			},
	{0x00, 0x01, 0x03, 0x00, "1 Coin  1 Credits"			},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  2 Credits"			},
	{0x00, 0x01, 0x03, 0x02, "Free Play"					},

	{0   , 0xfe, 0   ,    4, "Right Coin"					},
	{0x00, 0x01, 0x0c, 0x00, "*1"							},
	{0x00, 0x01, 0x0c, 0x04, "*4"							},
	{0x00, 0x01, 0x0c, 0x08, "*5"							},
	{0x00, 0x01, 0x0c, 0x0c, "*6"							},

	{0   , 0xfe, 0   ,    2, "Center Coin"					},
	{0x00, 0x01, 0x10, 0x00, "*1"							},
	{0x00, 0x01, 0x10, 0x10, "*2"							},

	{0   , 0xfe, 0   ,    2, "Unknown"						},
	{0x00, 0x01, 0x20, 0x20, "Off"							},
	{0x00, 0x01, 0x20, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Game"							},
	{0x00, 0x01, 0xc0, 0x00, "Missile Command"				},
	{0x00, 0x01, 0xc0, 0x40, "Easy Super Missile Attack"	},
	{0x00, 0x01, 0xc0, 0x80, "Reg. Super Missile Attack"	},
	{0x00, 0x01, 0xc0, 0xc0, "Hard Super Missile Attack"	},

	{0   , 0xfe, 0   ,    4, "Cities"						},
	{0x01, 0x01, 0x03, 0x02, "4"							},
	{0x01, 0x01, 0x03, 0x01, "5"							},
	{0x01, 0x01, 0x03, 0x03, "6"							},
	{0x01, 0x01, 0x03, 0x00, "7"							},

	{0   , 0xfe, 0   ,    2, "Bonus Credit for 4 Coins"		},
	{0x01, 0x01, 0x04, 0x04, "No"							},
	{0x01, 0x01, 0x04, 0x00, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Trackball Size"				},
	{0x01, 0x01, 0x08, 0x00, "Mini"							},
	{0x01, 0x01, 0x08, 0x08, "Large"						},

	{0   , 0xfe, 0   ,    8, "Bonus City"					},
	{0x01, 0x01, 0x70, 0x10, "8000"							},
	{0x01, 0x01, 0x70, 0x70, "10000"						},
	{0x01, 0x01, 0x70, 0x60, "12000"						},
	{0x01, 0x01, 0x70, 0x50, "14000"						},
	{0x01, 0x01, 0x70, 0x40, "15000"						},
	{0x01, 0x01, 0x70, 0x30, "18000"						},
	{0x01, 0x01, 0x70, 0x20, "20000"						},
	{0x01, 0x01, 0x70, 0x00, "None"							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x01, 0x01, 0x80, 0x00, "Upright"						},
	{0x01, 0x01, 0x80, 0x80, "Cocktail"						},
};

STDDIPINFO(Suprmatk)

#include "rescap.h"

struct rcfilt {
	double pokey_filt;
	double pokey_capL;
	double pokey_capR;

	void sam(INT16 &samr, INT16 &saml)
	{
		if (pokey_filt == 0.0) {
			pokey_filt = 1.0 - exp(-1.0 / (RES_K(10) * CAP_U(0.1) * nBurnSoundRate));
			pokey_capL = 0.0;
			pokey_capR = 0.0;
		}

		pokey_capL += (saml - pokey_capL) * pokey_filt;
		saml = pokey_capL;

		pokey_capR += (samr - pokey_capR) * pokey_filt;
		samr = pokey_capR;
	}

	void filt(INT16 *buf, INT32 len) {
		for (INT32 i = 0; i < len; i++) {
			sam(buf[i * 2 + 0], buf[i * 2 + 1]);
		}
	}

	void reset() {
		pokey_filt = 0.0;
	}
};

static rcfilt DINK;

static INT32 get_madsel()
{
	if (madsel_lastcycles && (M6502TotalCycles() - madsel_lastcycles) == 5) {
		madsel_lastcycles = 0;

		return 1;
	}

	return 0;
}

static inline INT32 get_bit3_addr(INT32 i)
{
	return ((i & 0x0800) >> 1) | ((~i & 0x0800) >> 2) | ((i & 0x07f8) >> 2) | ((i & 0x1000) >> 12);
}

static void write_vram(UINT16 address, UINT8 data)
{
	INT32 vramaddr = address >> 2;
	UINT8 vramdata = 0xfff00f00 >> ((data >> 6) * 8);
	UINT8 vrammask = DrvWritePROM[(address & 7) | 0x10];
	DrvVideoRAM[vramaddr] = (DrvVideoRAM[vramaddr] & vrammask) | (vramdata & ~vrammask);

	if ((address & 0xe000) == 0xe000)
	{
		vramaddr = get_bit3_addr(address);
		vramdata = -((data >> 5) & 1);
		vrammask = DrvWritePROM[(address & 7) | 0x18];
		DrvVideoRAM[vramaddr] = (DrvVideoRAM[vramaddr] & vrammask) | (vramdata & ~vrammask);

		M6502Idle(-1);
	}
}

static UINT8 read_vram(UINT16 address)
{
	UINT8 result = 0xff;
	INT32 vramaddr = address >> 2;
	UINT8 vrammask = 0x11 << (address & 3);
	UINT8 vramdata = DrvVideoRAM[vramaddr] & vrammask;
	if ((vramdata & 0xf0) == 0) result &= ~0x80;
	if ((vramdata & 0x0f) == 0) result &= ~0x40;

	if ((address & 0xe000) == 0xe000)
	{
		vramaddr = get_bit3_addr(address);
		vrammask = 1 << (address & 7);
		vramdata = DrvVideoRAM[vramaddr] & vrammask;
		if (vramdata == 0) result &= ~0x20;

		M6502Idle(-1);
	}

	return result;
}

static void missile_write(UINT16 address, UINT8 data)
{
	if (get_madsel()) {
		write_vram(address, data);
		return;
	}

	address &= 0x7fff;

	if ((address & 0x4000) == 0x0000) {
		DrvVideoRAM[address] = data;
		return;
	}

	if ((address & 0x7800) == 0x4000) {
		// when ufo or plane leaves the screen, we're left with a nasty sound
		if ((address & 0x0f) == 6 && data != 0) {
			// pokey chan4 freq activity?
			last_pokey_6_write = M6502TotalCycles();
		}
		if ((address & 0x0f) == 7 && data == 0xa4 && (M6502TotalCycles() - last_pokey_6_write) > 20000) {
			// no chan4 freq activity for ~1 frame, silence chan4 if it was 0xa4 (the ufo/plane sfx volume)
			data = 0;
		}

		pokey_write(0, address, data);
		return;
	}

	if ((address & 0x7f00) == 0x4800) {
		flipscreen = (~data & 0x40) >> 6;
		// coin counter = data & 0x38
		// leds = ~data & 0x06;
		ctrld = data & 0x01;
		return;
	}

	if ((address & 0x7f00) == 0x4b00) {
		DrvPalRAM[address & 7] = data;
		DrvPalette[address & 7] = BurnHighCol((data & 8) ? 0 : 0xff, (data & 4) ? 0 : 0xff, (data & 2) ? 0 : 0xff, 0);
		return;
	}

	if ((address & 0x7f00) == 0x4c00) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0x7f00) == 0x4d00) {
		if (irq_state) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			irq_state = 0;
		}
		return;
	}
	bprintf (0, _T("Missed write! %4.4x, %2.2x\n"), address, data);
}

static UINT8 missile_read(UINT16 address)
{
	if (get_madsel()) {
		return read_vram(address);
	}

	address &= 0x7fff;
	UINT8 ret = 0xff;

	if ((address & 0x4000) == 0x0000) {
		ret = DrvVideoRAM[address];
	}

	if (address >= 0x5000) {
		ret = DrvMainROM[address];

		if (irq_state == 0 && (ret & 0x1f) == 0x01 && M6502GetFetchStatus()) {
			madsel_lastcycles = M6502TotalCycles();
		}
	}

	if ((address & 0x7800) == 0x4000) {
		ret = pokey_read(0, address & 0xf);
	}

	if ((address & 0x7f00) == 0x4800) {
		if (ctrld) {
			if (!flipscreen) {
				ret = ((BurnTrackballRead(0, 1) << 4) & 0xf0) | (BurnTrackballRead(0, 0) & 0x0f);
			} else {
				ret = ((BurnTrackballRead(1, 1) << 4) & 0xf0) | (BurnTrackballRead(1, 0) & 0x0f);
			}
		} else {
			ret = DrvInputs[0]; // buttons
		}
	}

	if ((address & 0x7f00) == 0x4900) {
		ret  = DrvInputs[1] & ~0x98; // in1
		ret |= (system_scanline < 24) ? 0x80 : 0x00;
	}

	if ((address & 0x7f00) == 0x4a00) {
		ret = DrvDips[0]; // r10
	}

	return ret;
}

static INT32 allpot(INT32 )
{
	return DrvDips[1];
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	M6502Reset();
	M6502Close();

	BurnWatchdogReset();

	flipscreen = 0;
	ctrld = 0;
	irq_state = 0;
	madsel_lastcycles = 0;
	last_pokey_6_write = 0;

	nExtraCycles[0] = 0;

	DINK.reset();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x010000;

	DrvWritePROM	= Next; Next += 0x000020;

	DrvPalette		= (UINT32*)Next; Next += 0x0008 * sizeof(UINT32);

	AllRam			= Next;

	DrvVideoRAM		= Next; Next += 0x010000;
	DrvPalRAM		= Next; Next += 0x000008;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvLoadRoms(bool bLoad)
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[1] = { DrvMainROM + 0x5000 };
	UINT8 *gLoad[1] = { DrvWritePROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		INT32 id = ri.nType & 7;

		if ((ri.nType & BRF_PRG) && id == 1) // PRG
		{
			if (bLoad) {
				bprintf (0, _T("PRG%d: %5.5x, %d\n"), id, pLoad[id-1] - (DrvMainROM + 0x5000), i);
				if (BurnLoadRom(pLoad[id-1], i, 1)) return 1;
			}
			pLoad[id-1] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_GRA) && id == 1) // PROMS
		{
			if (bLoad) {
				bprintf (0, _T("GFX PROMS%d: %5.5x, %d\n"), id, gLoad[id-1] - DrvWritePROM, i);
				if (BurnLoadRom(gLoad[id-1], i, 1)) return 1;
			}
			gLoad[id-1] += ri.nLen;
			continue;
		}
	}

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	if (DrvLoadRoms(1)) return 1;

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetReadHandler(missile_read);
	M6502SetWriteHandler(missile_write);
	M6502Close();

    PokeyInit(1250000, 1, 3.00, 0);
    PokeyAllPotCallback(0, allpot);

	BurnWatchdogInit(DrvDoReset, 8);

	BurnTrackballInit(2);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();

	PokeyExit();

	BurnTrackballExit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_layer()
{
	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		INT32 effy = flipscreen ? ((255 - y) & 0xff) : y + 25;
		UINT8 *src = &DrvVideoRAM[effy * 64];
		UINT8 *src3 = (effy >= 224) ? &DrvVideoRAM[get_bit3_addr(effy << 8)] : NULL;

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			UINT8 pix = src[x / 4] >> (x & 3);
			pix = ((pix >> 2) & 4) | ((pix << 1) & 2);

			if (src3)
				pix |= (src3[(x / 8) * 2] >> (x & 7)) & 1;

			pTransDraw[y * nScreenWidth + x] = pix;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 8; i++) {
			INT32 data = DrvPalRAM[i];
			DrvPalette[i] = BurnHighCol((data & 8) ? 0 : 0xff, (data & 4) ? 0 : 0xff, (data & 2) ? 0 : 0xff, 0);
		}

		DrvRecalc = 0;
	}

	draw_layer();

	BurnTransferFlip(flipscreen, flipscreen);
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		service.Toggle(DrvJoy2[6]);

		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
		BurnTrackballConfig(0, AXIS_NORMAL, AXIS_REVERSED);
		BurnTrackballFrame(0, Analog0PortX, Analog0PortY, 1, 7);
		BurnTrackballUpdate(0);

		BurnTrackballConfig(1, AXIS_REVERSED, AXIS_REVERSED);
		BurnTrackballFrame(1, Analog1PortX, Analog1PortY, 1, 7);
		BurnTrackballUpdate(1);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { (1250000 * 100) / 6103 };
	INT32 nCyclesDone[1] = { nExtraCycles[0] };
	INT32 nSoundBufferPos = 0;

	INT32 irq_lines[8] = { 0, 32, 64, 96, 128, 160, 192, 224 };
	INT32 irq_select = 0;

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		system_scanline = i;

		if (system_scanline > 223) {
			// line 224+ runs at half-speed!
			// convince CPU_RUN() we've already ran 1/2 line,
			nCyclesDone[0] += nCyclesTotal[0] / nInterleave / 2;
		}

		CPU_RUN(0, M6502);

		if (i == irq_lines[irq_select]) {
			irq_state = (~irq_lines[irq_select] >> 5) & 1;
			irq_select = (irq_select + 1) & 7;
			M6502SetIRQLine(0, (irq_state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
		}

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			pokey_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	M6502Close();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			pokey_update(pSoundBuf, nSegmentLength);
		}
		DINK.filt(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();
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
		*pnMin = 0x029727;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		pokey_scan(nAction, pnMin);
		BurnTrackballScan();

		SCAN_VAR(flipscreen);
		SCAN_VAR(ctrld);
		SCAN_VAR(madsel_lastcycles);
		SCAN_VAR(irq_state);
		SCAN_VAR(last_pokey_6_write);

		SCAN_VAR(nExtraCycles);

		service.Scan();
	}

	return 0;
}

// Missile Command (rev 3, A035467-02/04 PCBs)

static struct BurnRomInfo missileRomDesc[] = {
	{ "035820-02.h1",	0x0800, 0x7a62ce6a, 1 | BRF_PRG | BRF_ESS }, //  0 6502 Code
	{ "035821-02.jk1",	0x0800, 0xdf3bd57f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "035822-03e.kl1",	0x0800, 0x1a2f599a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "035823-02.lm1",	0x0800, 0x82e552bb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "035824-02.np1",	0x0800, 0x606e42e0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "035825-02.r1",	0x0800, 0xf752eaeb, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "035826-01.l6",	0x0020, 0x86a22140, 1 | BRF_GRA | BRF_ESS }, //  6 Write PROM
};

STD_ROM_PICK(missile)
STD_ROM_FN(missile)

struct BurnDriver BurnDrvMissile = {
	"missile", NULL, NULL, NULL, "1980",
	"Missile Command (rev 3, A035467-02/04 PCBs)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, missileRomInfo, missileRomName, NULL, NULL, NULL, NULL, MissileInputInfo, MissileDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 231, 4, 3
};


// Missile Command (rev 2, A035467-02/04 PCBs)

static struct BurnRomInfo missile2RomDesc[] = {
	{ "035820-02.h1",	0x0800, 0x7a62ce6a, 1 | BRF_PRG | BRF_ESS }, //  0 6502 Code
	{ "035821-02.jk1",	0x0800, 0xdf3bd57f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "035822-02.kl1",	0x0800, 0xa1cd384a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "035823-02.lm1",	0x0800, 0x82e552bb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "035824-02.np1",	0x0800, 0x606e42e0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "035825-02.r1",	0x0800, 0xf752eaeb, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "035826-01.l6",	0x0020, 0x86a22140, 1 | BRF_GRA },           //  6 Write PROM
};

STD_ROM_PICK(missile2)
STD_ROM_FN(missile2)

struct BurnDriver BurnDrvMissile2 = {
	"missile2", "missile", NULL, NULL, "1980",
	"Missile Command (rev 2, A035467-02/04 PCBs)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, missile2RomInfo, missile2RomName, NULL, NULL, NULL, NULL, MissileInputInfo, MissileDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 231, 4, 3
};


// Missile Command (rev 1, A035467-02 PCBs)

static struct BurnRomInfo missile1RomDesc[] = {
	{ "35820-01.h1",	0x0800, 0x41cbb8f2, 1 | BRF_PRG | BRF_ESS }, //  0 6502 Code
	{ "35821-01.jk1",	0x0800, 0x728702c8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "35822-01.kl1",	0x0800, 0x28f0999f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "35823-01.lm1",	0x0800, 0xbcc93c94, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "35824-01.np1",	0x0800, 0x0ca089c8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "35825-01.r1",	0x0800, 0x428cf0d5, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "035826-01.l6",	0x0020, 0x86a22140, 1 | BRF_GRA },           //  6 Write PROM
};

STD_ROM_PICK(missile1)
STD_ROM_FN(missile1)

struct BurnDriver BurnDrvMissile1 = {
	"missile1", "missile", NULL, NULL, "1980",
	"Missile Command (rev 1, A035467-02 PCBs)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, missile1RomInfo, missile1RomName, NULL, NULL, NULL, NULL, MissileInputInfo, MissileDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 231, 4, 3
};

static INT32 SuprmatkInit()
{
	INT32 rc = DrvInit();

	if (!rc) {
		const UINT16 table[64] = {
			0x7cc0, 0x5440, 0x5b00, 0x5740, 0x6000, 0x6540, 0x7500, 0x7100,
			0x7800, 0x5580, 0x5380, 0x6900, 0x6e00, 0x6cc0, 0x7dc0, 0x5b80,
			0x5000, 0x7240, 0x7040, 0x62c0, 0x6840, 0x7ec0, 0x7d40, 0x66c0,
			0x72c0, 0x7080, 0x7d00, 0x5f00, 0x55c0, 0x5a80, 0x6080, 0x7140,
			0x7000, 0x6100, 0x5400, 0x5bc0, 0x7e00, 0x71c0, 0x6040, 0x6e40,
			0x5800, 0x7d80, 0x7a80, 0x53c0, 0x6140, 0x6700, 0x7280, 0x7f00,
			0x5480, 0x70c0, 0x7f80, 0x5780, 0x6680, 0x7200, 0x7e40, 0x7ac0,
			0x6300, 0x7180, 0x7e80, 0x6280, 0x7f40, 0x6740, 0x74c0, 0x7fc0
		};

		for (INT32 i = 0; i < 64; i++) {
			memcpy (&DrvMainROM[table[i]], DrvMainROM + 0x8000 + i * 0x40, 0x40);
		}
	}

	return rc;
}

// Super Missile Attack (for rev 1)

static struct BurnRomInfo suprmatkRomDesc[] = {
	{ "035820-02.c1",	0x0800, 0x7a62ce6a, 1 | BRF_PRG | BRF_ESS }, //  0 6502 Code
	{ "035821-02.b1",	0x0800, 0xdf3bd57f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "035822-02.a1",	0x0800, 0xa1cd384a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "035823-02.a5",	0x0800, 0x82e552bb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "035824-02.b5",	0x0800, 0x606e42e0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "035825-02.c5",	0x0800, 0xf752eaeb, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "e0.d5",			0x0800, 0xd0b20179, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "e1.e5",			0x0800, 0xc6c818a3, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "035826-01.l6",	0x0020, 0x86a22140, 1 | BRF_GRA },           //  8 Write PROM

	{ "63s141.b2",		0x0100, 0x2de8ee4d, 2 | BRF_GRA },           //  9 Unknown PROMs
	{ "63s141.b4",		0x0100, 0x390fc532, 2 | BRF_GRA },           // 10
};

STD_ROM_PICK(suprmatk)
STD_ROM_FN(suprmatk)

struct BurnDriver BurnDrvSuprmatk = {
	"suprmatk", "missile", NULL, NULL, "1981",
	"Super Missile Attack (for rev 1)\0", NULL, "Atari / General Computer Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, suprmatkRomInfo, suprmatkRomName, NULL, NULL, NULL, NULL, MissileInputInfo, SuprmatkDIPInfo,
	SuprmatkInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 231, 4, 3
};


// Super Missile Attack (not encrypted)

static struct BurnRomInfo suprmatkdRomDesc[] = {
	{ "035820_sma.h1",		0x0800, 0x75f01b87, 1 | BRF_PRG | BRF_ESS }, //  0 6502 Code
	{ "035821_sma.jk1",		0x0800, 0x3320d67e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "035822_sma.kl1",		0x0800, 0xe6be5055, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "035823_sma.lm1",		0x0800, 0xa6069185, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "035824_sma.np1",		0x0800, 0x90a06be8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "035825_sma.r1",		0x0800, 0x1298213d, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "035826-01.l6",	0x0020, 0x86a22140, 1 | BRF_GRA },           //  6 Write PROM
};

STD_ROM_PICK(suprmatkd)
STD_ROM_FN(suprmatkd)

struct BurnDriver BurnDrvSuprmatkd = {
	"suprmatkd", "missile", NULL, NULL, "1981",
	"Super Missile Attack (not encrypted)\0", NULL, "Atari / General Computer Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, suprmatkdRomInfo, suprmatkdRomName, NULL, NULL, NULL, NULL, MissileInputInfo, SuprmatkDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	256, 231, 4, 3
};
