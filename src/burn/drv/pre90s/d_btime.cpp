// BurgerTime emu-layer for FB Alpha by dink & iq_132, based on the MAME driver by Zsolt Vasvari.

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "bitswap.h"
#include "driver.h"
#include "flt_rc.h"
extern "C" {
    #include "ay8910.h"
}
#include "lowpass2.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvMainROMdec;
static UINT8 *DrvSoundROM;

static UINT8 *DrvMainRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSoundRAM;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *DrvBgMapROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static INT32 gfx0len;
static INT32 gfx1len;

static INT16 *pAY8910Buffer[6];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static UINT8 vblank;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 audio_nmi_type;
static UINT8 audio_nmi_enable;
static UINT8 audio_nmi_state;
static UINT8 bnj_scroll1;
static UINT8 bnj_scroll2;

static UINT8 btime3mode = 0;

enum
{
	AUDIO_ENABLE_NONE,
	AUDIO_ENABLE_DIRECT,        /* via direct address in memory map */
	AUDIO_ENABLE_AY8910         /* via ay-8910 port A */
};

static class LowPass2 *LP1 = NULL, *LP2 = NULL;
#define SampleFreq 44100.0
#define CutFreq 3000.0-4440
#define Q 0.4
#define Gain 0.8
#define CutFreq2 3000.0-1880
#define Q2 0.3
#define Gain2 1.05


static struct BurnInputInfo BtimeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 2,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Btime)


static struct BurnDIPInfo BtimeDIPList[]=
{
	{0x10, 0xff, 0xff, 0x3f, NULL		},
	{0x11, 0xff, 0xff, 0x0b, NULL		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x10, 0x01, 0x03, 0x00, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x03, 0x01, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x10, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x10, 0x10, "Off"		},
	{0x10, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cross Hatch Pattern"		},
	{0x10, 0x01, 0x20, 0x20, "Off"		},
	{0x10, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Control Panel"		},
	{0x10, 0x01, 0x40, 0x00, "Upright"		},
	{0x10, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x11, 0x01, 0x01, 0x01, "3"		},
	{0x11, 0x01, 0x01, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x11, 0x01, 0x06, 0x06, "10000"		},
	{0x11, 0x01, 0x06, 0x04, "15000"		},
	{0x11, 0x01, 0x06, 0x02, "20000"		},
	{0x11, 0x01, 0x06, 0x00, "30000"		},

	{0   , 0xfe, 0   ,    2, "Enemies"		},
	{0x11, 0x01, 0x08, 0x08, "4"		},
	{0x11, 0x01, 0x08, 0x00, "6"		},

	{0   , 0xfe, 0   ,    2, "End of Level Pepper"		},
	{0x11, 0x01, 0x10, 0x10, "No"		},
	{0x11, 0x01, 0x10, 0x00, "Yes"		},
};

STDDIPINFO(Btime)


#define WB(a1,a2,dest) if (address >= a1 && address <= a2) { \
		dest[address - a1] = data; \
		return; \
	}
#define RB(a1,a2,dest) if (address >= a1 && address <= a2) \
		return dest[address - a1];

static void btimepalettewrite(int offset, int data)
{
	INT32 bit0, bit1, bit2;

	bit0 = (data >> 0) & 0x01;
	bit1 = (data >> 1) & 0x01;
	bit2 = (data >> 2) & 0x01;
	INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	bit0 = (data >> 3) & 0x01;
	bit1 = (data >> 4) & 0x01;
	bit2 = (data >> 5) & 0x01;
	INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	bit0 = 0;
	bit1 = (data >> 6) & 0x01;
	bit2 = (data >> 7) & 0x01;
	INT32 b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

	DrvPalette[offset] = BurnHighCol(r,g,b,0);
}

static UINT8 btime_main_read(UINT16 address)
{
	RB(0x0000, 0x07ff, DrvMainRAM);
	RB(0x0c00, 0x0c1f, DrvPalRAM);
	RB(0x1000, 0x13ff, DrvVidRAM);
	RB(0x1400, 0x17ff, DrvColRAM);

	if (address >= 0xb000 && address <= 0xffff) {
		return DrvMainROMdec[address];
	}

	if (address >= 0x1800 && address <= 0x1bff) {
		INT32 offset = address & 0x3ff;
		return DrvVidRAM[(((offset % 32) * 32) + (offset / 32))];
	}

	if (address >= 0x1c00 && address <= 0x1fff) {
		INT32 offset = address & 0x3ff;
		return DrvColRAM[(((offset % 32) * 32) + (offset / 32))];
	}

	switch (address)
	{
		case 0x4000:
			return DrvInputs[0];

		case 0x4001:
			return DrvInputs[1];

		case 0x4002:
			return DrvInputs[2];

		case 0x4003:
			return (DrvDips[0] & 0x7f) | vblank;

		case 0x4004:
			return DrvDips[1];
	}
	return 0;
}

static void btime_decrypt()
{
	// d-
	UINT16 A = M6502GetPC(0);
	UINT16 A1 = M6502GetPrevPC(0);

	if (DrvMainROMdec[A1] == 0x20)	/* JSR $xxxx */
		A = btime_main_read(A1+1) + 256 * btime_main_read(A1+2);

	/* If the address of the next instruction is xxxx xxx1 xxxx x1xx, decode it. */
	if ((A & 0x0104) == 0x0104)
	{
		/* 76543210 -> 65342710 bit rotation */
		DrvMainROMdec[A] = (DrvMainROM[A] & 0x13) | ((DrvMainROM[A] & 0x80) >> 5) | ((DrvMainROM[A] & 0x64) << 1)
			   | ((DrvMainROM[A] & 0x08) << 2);
	}
}


static void btime_main_write(UINT16 address, UINT8 data)
{
	btime_decrypt();

	WB(0x0000, 0x07ff, DrvMainRAM);
	WB(0x1000, 0x13ff, DrvVidRAM);
	WB(0x1400, 0x17ff, DrvColRAM);

	if (address >= 0xc00 && address <= 0xc1f) {
		DrvPalRAM[address - 0xc00] = data;
		if (address <= 0xc0f) btimepalettewrite(address - 0xc00, ~data);
		return;
	}

	if (address >= 0x1800 && address <= 0x1bff) {
		INT32 offset = address & 0x3ff;
		DrvVidRAM[(((offset % 32) * 32) + (offset / 32))] = data;
		return;
	}

	if (address >= 0x1c00 && address <= 0x1fff) {
		INT32 offset = address & 0x3ff;
		DrvColRAM[(((offset % 32) * 32) + (offset / 32))] = data;
		return;
	}

	//if (address != 0x4000) bprintf(0, _T("mw %X %X, "), address, data);

	switch (address)
	{
		case 0x4000:
		return; // nop

		case 0x4002:
			// video control (flipscreen)
		return;

		case 0x4003:
			soundlatch = data;
			M6502Close();
			M6502Open(1);
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6502Close();
			M6502Open(0);
		return;

		case 0x4004:
			bnj_scroll1 = data;
		return;
	}
}

static UINT8 btime_sound_read(UINT16 address)
{
	if (address >= 0x0000 && address <= 0x1fff) {
		return DrvSoundRAM[address & 0x3ff];
	}

	RB(0xe000, 0xefff, DrvSoundROM);
	RB(0xf000, 0xffff, DrvSoundROM);

	switch (address >> 13)
	{
		case 0x05:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static void btime_sound_write(UINT16 address, UINT8 data)
{
	if (address >= 0x0000 && address <= 0x1fff) {
		DrvSoundRAM[address & 0x3ff] = data;
	}
	switch (address >> 13)
	{
		case 0x01:
			AY8910Write(0, ~((address>>13)-1) & 1, data);
			return;
		case 0x02:
			AY8910Write(0, ~((address>>13)-1) & 1, data);
			return;
		case 0x03:
			AY8910Write(1, ~((address>>13)-1) & 1, data);
			return;
		case 0x04:
			AY8910Write(1, ~((address>>13)-1) & 1, data);
			return;

		case 0x06: {
			if (audio_nmi_type == AUDIO_ENABLE_DIRECT) {
				audio_nmi_enable = data & 1;
				M6502SetIRQLine(0x20, (audio_nmi_enable && audio_nmi_state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			}
		}
		return;
	}
}

static void ay8910_0_portA_write(UINT32, UINT32 data)
{
	if (data == 0xff) return; // init/reset. (usually)

	if (audio_nmi_type == AUDIO_ENABLE_AY8910)
	{
		audio_nmi_enable = ~data & 1;
		M6502SetIRQLine(0x20, (audio_nmi_enable && audio_nmi_state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	}

}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	= Next; Next += 0x010000;
	DrvMainROMdec	= Next; Next += 0x010000;
	DrvSoundROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x020000;
	DrvGfxROM1	= Next; Next += 0x020000;
	DrvGfxROM2	= Next; Next += 0x020000;
	DrvGfxROM3	= Next; Next += 0x020000;
	DrvBgMapROM = Next; Next += 0x020000;

	DrvPalette	= (UINT32*)Next; Next += 0x0200 * sizeof(INT32);

	pAY8910Buffer[0] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5] = (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	AllRam		= Next;

	DrvMainRAM	= Next; Next += 0x001000;
	DrvPalRAM	= Next; Next += 0x001000;
	DrvVidRAM	= Next; Next += 0x001000;
	DrvColRAM	= Next; Next += 0x001000;
	DrvSoundRAM	= Next; Next += 0x001000;

	RamEnd		= Next;
	MemEnd		= Next;

	return 0;
}


static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	M6502Close();

	AY8910Reset(0);
	AY8910Reset(1);

	flipscreen = 0;
	soundlatch = 0;
	bnj_scroll1 = 0;
	bnj_scroll2 = 0;

	audio_nmi_enable = 0;
	audio_nmi_state = 0;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane_t8[3] = { RGN_FRAC(gfx0len, 2,3), RGN_FRAC(gfx0len, 1,3), RGN_FRAC(gfx0len, 0,3) };
	INT32 Plane_t16spr[3] = { RGN_FRAC(gfx0len, 2,3), RGN_FRAC(gfx0len, 1,3), RGN_FRAC(gfx0len, 0,3) };
	INT32 Plane_t16[3] = { RGN_FRAC(gfx1len, 2,3), RGN_FRAC(gfx1len, 1,3), RGN_FRAC(gfx1len, 0,3) };

	INT32 t8XOffs[8] = { STEP8(0,1) };
	INT32 t8YOffs[8] = { STEP8(0,8) };

	INT32 t16XOffs[16] = { STEP8(16*8,1), STEP8(0,1) };
	INT32 t16YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)malloc(gfx0len + gfx1len); // more = more happy-fun. yay!
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, gfx0len);

	GfxDecode(0x0400, 3, 8, 8, Plane_t8, t8XOffs, t8YOffs, 0x40, tmp, DrvGfxROM0);
	GfxDecode(0x00ff, 3, 16, 16, Plane_t16spr, t16XOffs, t16YOffs, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, gfx1len);

	GfxDecode(0x0040, 3, 16, 16, Plane_t16, t16XOffs, t16YOffs, 0x100, tmp, DrvGfxROM2);

	free (tmp);

	return 0;
}


static INT32 BtimeInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 base = (btime3mode) ? 0xb000 : 0xc000;

		if (BurnLoadRom(DrvMainROM + base + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + base + 0x01000,  1, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + base + 0x02000,  2, 1)) return 1;
		if (BurnLoadRom(DrvMainROM + base + 0x03000,  3, 1)) return 1;

		if (btime3mode)
			if (BurnLoadRom(DrvMainROM + base + 0x04000,  4, 1)) return 1; // btime3mode

		if (BurnLoadRom(DrvSoundROM + 0x0000,  4+btime3mode, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  5+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x001000,  6+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x002000,  7+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x003000,  8+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x004000,  9+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x005000,  10+btime3mode, 1)) return 1;
		gfx0len = 0x6000;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  11+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000800,  12+btime3mode, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x001000,  13+btime3mode, 1)) return 1;
		gfx1len = 0x1800;

		if (BurnLoadRom(DrvBgMapROM + 0x000000,  14+btime3mode, 1)) return 1;

		DrvGfxDecode();

	}

	memcpy(DrvMainROMdec, DrvMainROM, 0x10000);

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502SetWriteHandler(btime_main_write);
	M6502SetReadHandler(btime_main_read);
	M6502SetWriteMemIndexHandler(btime_main_write);
	M6502SetReadMemIndexHandler(btime_main_read);
	M6502SetReadOpArgHandler(btime_main_read);
	M6502SetReadOpHandler(btime_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502SetWriteHandler(btime_sound_write);
	M6502SetReadHandler(btime_sound_read);
	M6502SetWriteMemIndexHandler(btime_sound_write);
	M6502SetReadMemIndexHandler(btime_sound_read);
	M6502SetReadOpArgHandler(btime_sound_read);
	M6502SetReadOpHandler(btime_sound_read);
	M6502Close();

	AY8910Init(0, 1500000, nBurnSoundRate, NULL, NULL, &ay8910_0_portA_write, NULL);
	AY8910Init(1, 1500000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);
	audio_nmi_type = AUDIO_ENABLE_DIRECT;

	GenericTilesInit();

	LP1 = new LowPass2(CutFreq, SampleFreq, Q, Gain, // these 2 filters are used to get
					   CutFreq2, Q2, Gain2);         // rid of the bg hissing noise on the first ay
	LP2 = new LowPass2(CutFreq, SampleFreq, Q, Gain, // they work a lot better than flt_rc, so they are used instead.
					   CutFreq2, Q2, Gain2);

	// first ay, pass-thru mixer only (CAP_P(0) = passthru)
	filter_rc_init(0, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 0);
	filter_rc_init(1, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);
	filter_rc_init(2, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_P(0), 1);

	// second ay, apply a slight bit of lpf to match
	filter_rc_init(3, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x15), 1);
	filter_rc_init(4, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10), 1);
	filter_rc_init(5, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10), 1);

	filter_rc_set_route(0, 0.25, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(1, 0.25, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(2, 0.25, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(3, 0.15, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(4, 0.15, BURN_SND_ROUTE_BOTH);
	filter_rc_set_route(5, 0.15, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();

	AY8910Exit(0);
	AY8910Exit(1);

	filter_rc_exit();

	BurnFree(AllMem);
	btime3mode = 0;

	delete LP1;
	delete LP2;
	LP1 = NULL;
	LP2 = NULL;

	return 0;
}

static void BtimePaletteRecalc()
{
	for (INT32 i = 0; i < 0x10; i++) {
		btimepalettewrite(i, ~DrvPalRAM[i]);
	}
}

static void draw_background(INT32 color)
{
	const UINT8 *gfx = DrvBgMapROM;
	int scroll = -(bnj_scroll2 | ((bnj_scroll1 & 0x03) << 8));

	INT32 start = 0;
	UINT8 tmap[5];

	if (flipscreen)
		start = 0;
	else
		start = 1;

	for (INT32 i = 0; i < 4; i++)
	{
		tmap[i] = start | (bnj_scroll1 & 0x04);
		start = (start + 1) & 0x03;
	}

	for (INT32 i = 0; i < 5; i++, scroll += 256)
	{
		INT32 tileoffset = tmap[i & 3] * 0x100;

		if (scroll > 256) break;
		if (scroll < -256) continue;

		for (INT32 offs = 0; offs < 0x100; offs++)
		{
			int x = 240 - (16 * (offs / 16) + scroll) - 1;
			int y = 16 * (offs % 16);

			if (flipscreen)
			{
				x = 240 - x;
				y = 240 - y;
			}

			y -= 8;
			x -= 8;

			if (x < -15 || x >= 256 || y < -15 || y >= 256) continue;

			INT32 code = gfx[tileoffset + offs];

			Render16x16Tile_Clip(pTransDraw, code, x, y, color, 3, 8, DrvGfxROM2);
		}
	}
}

static void draw_chars(UINT8 transparency, UINT8 color, int priority)
{
	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 x = 31 - (offs / 32);
		INT32 y = offs % 32;

		UINT16 code = DrvVidRAM[offs] + 256 * (DrvColRAM[offs] & 3);

		if ((priority != -1) && (priority != ((code >> 7) & 0x01)))
			continue;

		if (flipscreen)
		{
			x = 31 - x;
			y = 31 - y;
		}

		y -= 1;
		x -= 1;

		if (transparency) {
			Render8x8Tile_Mask_Clip(pTransDraw, code, x * 8, y * 8, color, 3, 0, 0, DrvGfxROM0);
		} else {
			Render8x8Tile_Clip(pTransDraw, code, x * 8, y * 8, color, 3, 0, DrvGfxROM0);
		}
	}
}

static void draw_sprites(UINT8 color, UINT8 sprite_y_adjust, UINT8 sprite_y_adjust_flip_screen, UINT8 *sprite_ram, INT32 interleave)
{
	int i;
	INT32 offs;

	for (i = 0, offs = 0; i < 8; i++, offs += 4 * interleave)
	{
		int x, y;
		UINT8 flipx, flipy;

		if (!(sprite_ram[offs + 0] & 0x01)) continue;

		x = 240 - sprite_ram[offs + 3 * interleave];
		y = 240 - sprite_ram[offs + 2 * interleave];

		flipx = sprite_ram[offs + 0] & 0x04;
		flipy = sprite_ram[offs + 0] & 0x02;

		if (flipscreen)
		{
			x = 240 - x;
			y = 240 - y + sprite_y_adjust_flip_screen;

			flipx = !flipx;
			flipy = !flipy;
		}


		INT32 code = sprite_ram[offs + interleave];

		y -= 8;
		x -= 8;

		y = y - sprite_y_adjust;

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			}
		}

		y = y + (flipscreen ? -256 : 256);

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, x, y, color, 3, 0, 0, DrvGfxROM1);
			}
		}
	}
}

static INT32 BtimeDraw()
{
	if (DrvRecalc) {
		BtimePaletteRecalc();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (bnj_scroll1 & 0x10) {
		if (nBurnLayer & 1) draw_background(0);
		if (nBurnLayer & 2) draw_chars(1, 0, -1);
	} else {
		if (nBurnLayer & 2) draw_chars(0, 0, -1);
	}

	if (nBurnLayer & 4) draw_sprites(0, 1, 0, DrvVidRAM, 0x20);

	BurnTransferCopy(DrvPalette);

	return 0;
}

#if 0
extern int counter;
int lastctr = 0;
#endif

static INT32 BtimeFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();

	{
		static UINT8 prevcoin = 0;

		memset (DrvInputs, 0xff, 2);
		DrvInputs[2] = 0x3f;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
		if ((DrvInputs[2] & 0xc0) && (prevcoin != (DrvInputs[2] & 0xc0))) {
			M6502Open(0);
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
			M6502Close();
		}
		prevcoin = DrvInputs[2] & 0xc0;
	}

	INT32 nInterleave = 272;
	INT32 nCyclesTotal[2] = { 1500000 / 60, 500000 / 60 };
	INT32 nCyclesDone[2]  = { 0, 0 };

	vblank = 0x80;

#if 0
	if (counter != lastctr) { // save this for tweaking filters etc.
		lastctr = counter;

		//LP1->SetParam(CutFreq + (counter * 500), SampleFreq, Q, Gain, CutFreq2+ (counter * 500), Q2, Gain2);
		//LP2->SetParam(CutFreq + (counter * 500), SampleFreq, Q, Gain, CutFreq2+ (counter * 500), Q2, Gain2);
		filter_rc_set_RC(3, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10+ (counter * 50)));
		filter_rc_set_RC(4, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10));
		filter_rc_set_RC(5, FLT_RC_LOWPASS, 1000, 5100, 0, CAP_N(10*0x10));
	}
#endif

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6502Open(0);
		INT32 nSegment = (nCyclesTotal[0] - nCyclesDone[0]) / (nInterleave - i);
		nCyclesDone[0] += M6502Run(nSegment);
		M6502Close();

		if (i == 248) vblank = 0x80;
		if (i == 8) vblank = 0x00;

		M6502Open(1);
		nSegment = (nCyclesTotal[1] - nCyclesDone[1]) / (nInterleave - i);
		nCyclesDone[1] += M6502Run(nSegment);

		audio_nmi_state = i & 8;
		M6502SetIRQLine(0x20, (audio_nmi_enable && audio_nmi_state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
		M6502Close();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);

		filter_rc_update(0, pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen);
		filter_rc_update(1, pAY8910Buffer[1], pBurnSoundOut, nBurnSoundLen);
		filter_rc_update(2, pAY8910Buffer[2], pBurnSoundOut, nBurnSoundLen);

		if (LP1 && LP2) {
			LP1->Filter(pBurnSoundOut, nBurnSoundLen);  // Left
			LP2->Filter(pBurnSoundOut+1, nBurnSoundLen); // Right
		}
		filter_rc_update(3, pAY8910Buffer[3], pBurnSoundOut, nBurnSoundLen);
		filter_rc_update(4, pAY8910Buffer[4], pBurnSoundOut, nBurnSoundLen);
		filter_rc_update(5, pAY8910Buffer[5], pBurnSoundOut, nBurnSoundLen);

	}

	if (pBurnDraw) {
		BtimeDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029719;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_DRIVER_DATA) {

		M6502Scan(nAction);

		SCAN_VAR(vblank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(audio_nmi_type);
		SCAN_VAR(audio_nmi_enable);
		SCAN_VAR(audio_nmi_state);
		SCAN_VAR(bnj_scroll1);
		SCAN_VAR(bnj_scroll2);
	}

	return 0;
}

// Burger Time (Data East set 1)

static struct BurnRomInfo btimeRomDesc[] = {
	{ "aa04.9b",	0x1000, 0x368a25b5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "aa06.13b",	0x1000, 0xb4ba400d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "aa05.10b",	0x1000, 0x8005bffa, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "aa07.15b",	0x1000, 0x086440ad, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ab14.12h",	0x1000, 0xf55e5211, 2 | BRF_PRG | BRF_ESS }, //  4 audiocpu

	{ "aa12.7k",	0x1000, 0xc4617243, 3 | BRF_GRA },           //  5 gfx1
	{ "ab13.9k",	0x1000, 0xac01042f, 3 | BRF_GRA },           //  6
	{ "ab10.10k",	0x1000, 0x854a872a, 3 | BRF_GRA },           //  7
	{ "ab11.12k",	0x1000, 0xd4848014, 3 | BRF_GRA },           //  8
	{ "aa8.13k",	0x1000, 0x8650c788, 3 | BRF_GRA },           //  9
	{ "ab9.15k",	0x1000, 0x8dec15e6, 3 | BRF_GRA },           // 10

	{ "ab00.1b",	0x0800, 0xc7a14485, 4 | BRF_GRA },           // 11 gfx2
	{ "ab01.3b",	0x0800, 0x25b49078, 4 | BRF_GRA },           // 12
	{ "ab02.4b",	0x0800, 0xb8ef56c3, 4 | BRF_GRA },           // 13

	{ "ab03.6b",	0x0800, 0xd26bc1f3, 5 | BRF_GRA },           // 14 bg_map
};

STD_ROM_PICK(btime)
STD_ROM_FN(btime)

struct BurnDriver BurnDrvBtime = {
	"btime", NULL, NULL, NULL, "1982",
	"Burger Time (Data East set 1)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, btimeRomInfo, btimeRomName, NULL, NULL, BtimeInputInfo, BtimeDIPInfo,
	BtimeInit, DrvExit, BtimeFrame, BtimeDraw, DrvScan, &DrvRecalc, 16,
	240, 242, 3, 4
};

// Burger Time (Data East set 2)

static struct BurnRomInfo btime2RomDesc[] = {
	{ "aa04.9b2",	0x1000, 0xa041e25b, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "aa06.13b",	0x1000, 0xb4ba400d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "aa05.10b",	0x1000, 0x8005bffa, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "aa07.15b",	0x1000, 0x086440ad, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ab14.12h",	0x1000, 0xf55e5211, 2 | BRF_PRG | BRF_ESS }, //  4 audiocpu

	{ "aa12.7k",	0x1000, 0xc4617243, 3 | BRF_GRA },           //  5 gfx1
	{ "ab13.9k",	0x1000, 0xac01042f, 3 | BRF_GRA },           //  6
	{ "ab10.10k",	0x1000, 0x854a872a, 3 | BRF_GRA },           //  7
	{ "ab11.12k",	0x1000, 0xd4848014, 3 | BRF_GRA },           //  8
	{ "aa8.13k",	0x1000, 0x8650c788, 3 | BRF_GRA },           //  9
	{ "ab9.15k",	0x1000, 0x8dec15e6, 3 | BRF_GRA },           // 10

	{ "ab00.1b",	0x0800, 0xc7a14485, 4 | BRF_GRA },           // 11 gfx2
	{ "ab01.3b",	0x0800, 0x25b49078, 4 | BRF_GRA },           // 12
	{ "ab02.4b",	0x0800, 0xb8ef56c3, 4 | BRF_GRA },           // 13

	{ "ab03.6b",	0x0800, 0xd26bc1f3, 5 | BRF_GRA },           // 14 bg_map
};

STD_ROM_PICK(btime2)
STD_ROM_FN(btime2)

struct BurnDriver BurnDrvBtime2 = {
	"btime2", "btime", NULL, NULL, "1982",
	"Burger Time (Data East set 2)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, btime2RomInfo, btime2RomName, NULL, NULL, BtimeInputInfo, BtimeDIPInfo,
	BtimeInit, DrvExit, BtimeFrame, BtimeDraw, DrvScan, &DrvRecalc, 16,
	240, 242, 3, 4
};

INT32 Btime3Init()
{
	btime3mode = 1;
	return BtimeInit();
}

// Burger Time (Data East USA)

static struct BurnRomInfo btime3RomDesc[] = {
	{ "ab05a-3.12b",	0x1000, 0x12e9f58c, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "ab04-3.9b",		0x1000, 0x5d90c696, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ab06-3.13b",		0x1000, 0xe0b993ad, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ab05-3.10b",		0x1000, 0xc2b44b7f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "ab07-3.15b",		0x1000, 0x91986594, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "ab14-1.12h",		0x1000, 0xf55e5211, 2 | BRF_PRG | BRF_ESS }, //  5 audiocpu

	{ "ab12-1.7k",		0x1000, 0x6c79f79f, 3 | BRF_GRA },           //  6 gfx1
	{ "ab13-1.9k",		0x1000, 0xac01042f, 3 | BRF_GRA },           //  7
	{ "ab10-1.10k",		0x1000, 0x854a872a, 3 | BRF_GRA },           //  8
	{ "ab11-1.12k",		0x1000, 0xd4848014, 3 | BRF_GRA },           //  9
	{ "ab8-1.13k",		0x1000, 0x70b35bbe, 3 | BRF_GRA },           // 10
	{ "ab9-1.15k",		0x1000, 0x8dec15e6, 3 | BRF_GRA },           // 11

	{ "ab00-1.1b",		0x0800, 0xc7a14485, 4 | BRF_GRA },           // 12 gfx2
	{ "ab01-1.3b",		0x0800, 0x25b49078, 4 | BRF_GRA },           // 13
	{ "ab02-1.4b",		0x0800, 0xb8ef56c3, 4 | BRF_GRA },           // 14

	{ "ab03-3.6b",		0x0800, 0xf699d797, 5 | BRF_GRA },           // 15 bg_map
};

STD_ROM_PICK(btime3)
STD_ROM_FN(btime3)

struct BurnDriver BurnDrvBtime3 = {
	"btime3", "btime", NULL, NULL, "1982",
	"Burger Time (Data East USA)\0", NULL, "Data East USA Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, btime3RomInfo, btime3RomName, NULL, NULL, BtimeInputInfo, BtimeDIPInfo,
	Btime3Init, DrvExit, BtimeFrame, BtimeDraw, DrvScan, &DrvRecalc, 16,
	240, 242, 3, 4
};
