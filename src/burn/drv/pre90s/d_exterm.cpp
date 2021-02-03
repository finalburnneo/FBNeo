// FinalBurn Neo Gottlieb Exterminator hardware driver module
// Based on MAME driver by Alex Pasadyn, Zsolt Vasvari, Aaron Giles

#include "tiles_generic.h"
#include "tms34_intf.h"
#include "m6502_intf.h"
#include "burn_ym2151.h"
#include "dac.h"
#include "watchdog.h"
#include "burn_pal.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSndROM[2];
static UINT8 *DrvNVRAM;
static UINT8 *DrvVidRAM[2];
static UINT8 *DrvSndRAM[2];
static UINT8 *DrvMainRAM;
static UINT8 *DrvSubRAM;

static INT32 output_last;
static INT32 aimpos[2];
static UINT8 trackball_old[2];
static INT32 soundlatch[2];
static INT32 sound_control;
static UINT8 dac[2];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;
static INT16 Analog[2];

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo ExtermInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},
	A("P1 Spinner",     BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"),

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 4,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},
	A("P2 Spinner",     BIT_ANALOG_REL, &Analog[1],		"p2 x-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};
#undef A
STDINPUTINFO(Exterm)

static struct BurnDIPInfo ExtermDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0xff, NULL					},
	{0x01, 0xff, 0xff, 0x01, NULL                   },

	{0   , 0xfe, 0   ,    2, "Unused"				},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x00, 0x01, 0x06, 0x06, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x06, 0x02, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x06, 0x04, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x06, 0x00, "1 Coin  4 Credits"	},
	
	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x00, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x38, 0x18, "1 Coin  2 Credits"	},
	{0x00, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x00, 0x01, 0x38, 0x08, "1 Coin  4 Credits"	},
	{0x00, 0x01, 0x38, 0x30, "1 Coin  5 Credits"	},
	{0x00, 0x01, 0x38, 0x10, "1 Coin  6 Credits"	},
	{0x00, 0x01, 0x38, 0x20, "1 Coin  7 Credits"	},
	{0x00, 0x01, 0x38, 0x00, "1 Coin  8 Credits"	},

	{0   , 0xfe, 0   ,    2, "Memory Test"			},
	{0x00, 0x01, 0x40, 0x40, "Once"					},
	{0x00, 0x01, 0x40, 0x00, "Continuous"			},

	{0   , 0xfe, 0   ,    2, "Free Play"			},
	{0x00, 0x01, 0x80, 0x80, "Off"					},
	{0x00, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},
};

STDDIPINFO(Exterm)

static INT32 nmi_timer_cb(INT32 n, INT32 c)
{
	if (sound_control & 1) M6502SetIRQLine(0, 0x20, CPU_IRQSTATUS_AUTO);
	return 0;
}

static void output_port_write(UINT16 data)
{
	if ((data & 0x0001) && !(output_last & 0x0001)) aimpos[0] = 0;
	if ((data & 0x0002) && !(output_last & 0x0002)) aimpos[1] = 0;

	if ((data & 0x2000) && !(output_last & 0x2000)) {
		TMS34010Close();
		TMS34010Open(1);
		TMS34010Reset(); // pulse sub cpu reset line
		TMS34010Close();
		TMS34010Open(0);

		// coin counters = data & 0xc000
	}

	output_last = data;
}

static UINT16 trackball_port_read(INT32 select)
{
	UINT8 trackball_pos = BurnTrackballRead(0, select);

	UINT8 trackball_diff = trackball_old[select] - trackball_pos;

	trackball_old[select] = trackball_pos;

	if (trackball_diff & 0x80) trackball_diff |= 0x20;

	aimpos[select] = (aimpos[select] + trackball_diff) & 0x3f;

	return (DrvInputs[select] & 0xc0ff) | (aimpos[select] << 8);
}

static void sync_snd_both()
{
	INT32 cyc0 = (TMS34010TotalCycles() * 2000000 / (40000000 / 8)) - M6502TotalCycles(0);
	INT32 cyc1 = (TMS34010TotalCycles() * 2000000 / (40000000 / 8)) - M6502TotalCycles(1);
	if (cyc0 > 0) {
		M6502Open(0);
		BurnTimerUpdate(M6502TotalCycles() + cyc0);
		M6502Close();
	}
	if (cyc1 > 0) {
		M6502Run(1, cyc1);
	}
}

static void exterm_main_write(UINT32 address, UINT16 data)
{
	// 1800000 - 1807fff, 2800000 - 2807fff
	if (((address & ~(0xfc7f8000|0x7fff)) == 0x1800000) || ((address & ~(0xfc7f8000|0x7fff)) == 0x2800000)) {
		TMS34010WriteWord((address & ~0xfc7f8000) >> 3, data);
		return;
	}

	// 0 - fffff
	if ((address & ~(0xfc700000|0xfffff)) == 0x00000) {
		TMS34010WriteWord((address & ~0xfc700000) >> 3, data);
		return;
	}

	if ((address & ~(0xfc400000|0x3fffff)) == 0x800000) {
		TMS34010WriteWord((address & ~0xfc400000) >> 3, data);
		return;
	}

	if (address & 0xfc000000) { // top 6 address lines not connected? Doesn't cover all mirrors...
		TMS34010WriteWord((address & 0x3ffffff) >> 3, data);
		return;
	}

	if ((address & 0x3c00000) == 0x1000000) {
		TMS34010Close();
		TMS34010Open(1);
		TMS34010HostWrite((address / 0x100000) & 3, data);
		TMS34010Close();
		TMS34010Open(0);
		return;
	}

	if ((address & 0x3fc0000) == 0x1500000) {
		output_port_write(data);
		return;
	}

	if ((address & 0x3fc0000) == 0x1580000) {
		sync_snd_both();
		soundlatch[0] = soundlatch[1] = data & 0xff;
		M6502SetIRQLine(0, 0, CPU_IRQSTATUS_AUTO);
		M6502SetIRQLine(1, 0, CPU_IRQSTATUS_AUTO);
		return;
	}

	if ((address & 0x3fc0000) == 0x15c0000) {
		BurnWatchdogWrite();
		return;
	}
}

static UINT16 exterm_main_read(UINT32 address)
{
	// 1800000 - 1807fff, 2800000 - 2807fff
	if (((address & ~(0xfc7f8000|0x7fff)) == 0x1800000) || ((address & ~(0xfc7f8000|0x7fff)) == 0x2800000)) {
		return TMS34010ReadWord((address & ~0xfc7f8000) >> 3);
	}

	// 0 - fffff
	if ((address & ~(0xfc700000|0xfffff)) == 0x00000) {
		return TMS34010ReadWord((address & ~0xfc700000) >> 3);
	}

	// 800000 - bfffff
	if ((address & ~(0xfc400000|0x3fffff)) == 0x800000) {
		return TMS34010ReadWord((address & ~0xfc400000) >> 3);
	}

	if (address & 0xfc000000) { // top 6 address lines not connected? Doesn't cover all mirrors...
		return TMS34010ReadWord((address & 0x3ffffff) >> 3);
	}

	if ((address & 0x3c00000) == 0x1000000) {
		TMS34010Close();
		TMS34010Open(1);
		UINT16 ret = TMS34010HostRead((address / 0x100000) & 3);
		TMS34010Close();
		TMS34010Open(0);
		return ret;
	}

	if ((address & 0x3f80000) == 0x1400000) {
		return trackball_port_read((address >> 18) & 1);
	}

	if ((address & 0x3fc0000) == 0x1480000) {
		return (DrvDips[0] | 0xff00);
	}

	return 0;
}

static void exterm_sub_write(UINT32 address, UINT16 data)
{
	//	0x047fffff
	if (address & 0x4000000) { // vram
		TMS34010WriteWord((address & 0x047fffff) >> 3, data);
		return;
	}
	if ((address & 0x4000000) == 0x00000000) { // ram
		TMS34010WriteWord((address & 0xfffff) >> 3, data);
		return;
	}
}

static UINT16 exterm_sub_read(UINT32 address)
{
	if (address & 0x4000000) { // vram
		return TMS34010ReadWord((address & 0x047fffff) >> 3);
	}
	if ((address & 0x4000000) == 0x00000000) { // ram
		return TMS34010ReadWord((address & 0xfffff) >> 3);
	}

	return 0xffff;
}

static void sync_sub()
{
	INT32 cyc = M6502TotalCycles() - M6502TotalCycles(1);
	if (cyc > 0) {
		M6502Run(1, cyc);
	}
}

static void exterm_sound_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe000) == 0x4000) {
		BurnYM2151Write(sound_control >> 7, data);
		return;
	}

	if ((address & 0xf800) == 0x6000) {
		double period = (1.0 / 4000000) * (4096 * (256 - data));
		BurnTimerSetRetrig(0, period);
		return;
	}

	if ((address & 0xe000) == 0xa000) {
		sound_control = data;
		return;
	}
}

static UINT8 exterm_sound_main_read(UINT16 address)
{
	if ((address & 0xf800) == 0x6800) {
		return soundlatch[0];
	}

	if ((address & 0xf800) == 0x7000) {
		sync_sub();
		M6502SetIRQLine(1, 0x20, CPU_IRQSTATUS_AUTO); // set NMI on slave
		return 0xff;
	}

	return 0;
}

static void exterm_sound_sub_write(UINT16 address, UINT8 data)
{
	if ((address & 0xc000) == 0x8000) {
		dac[address & 1] = data;
		DACWrite16Signed(0, (dac[0] /*^ 0xff*/) * dac[1]);
	}
}

static UINT8 exterm_sound_sub_read(UINT16 address)
{
	if ((address & 0xe000) == 0x4000) {
		return soundlatch[1];
	}

	return 0;
}

static void main_to_shift(UINT32 address, UINT16 *dst)
{
	UINT16 *ram = (UINT16*)DrvVidRAM[0];

	memcpy(dst, &ram[address >> 4], 256 * sizeof(UINT16));
}

static void main_from_shift(UINT32 address, UINT16 *src)
{
	UINT16 *ram = (UINT16*)DrvVidRAM[0];

	memcpy(&ram[address >> 4], src, 256 * sizeof(UINT16));
}

static void sub_to_shift(UINT32 address, UINT16 *dst)
{
	UINT16 *ram = (UINT16*)DrvVidRAM[1];

	memcpy(dst, &ram[address >> 4], 256 * sizeof(UINT16));
}

static void sub_from_shift(UINT32 address, UINT16 *src)
{
	UINT16 *ram = (UINT16*)DrvVidRAM[1];

	memcpy(&ram[address >> 4], src, 256 * sizeof(UINT16));
}

static INT32 scanline_cb(INT32 scanline, TMS34010Display *params)
{
	scanline -= params->veblnk; // top clipping
	if (scanline < 0 || scanline >= nScreenHeight) return 0;

	UINT16 *master_vram = (UINT16*)DrvVidRAM[0];
	UINT16 *slave_vram  = (UINT16*)DrvVidRAM[1];
	UINT16 *const bgsrc = &master_vram[(params->rowaddr << 8) & 0xff00];
	UINT16 *const dest = pTransDraw + scanline * nScreenWidth;
	TMS34010Display fgparams;
	INT32 coladdr = params->coladdr;
	INT32 fgcoladdr = 0;

	TMS34010Close();
	TMS34010Open(1);
	tms34010_get_display_params(&fgparams);
	TMS34010Close();
	TMS34010Open(0);

	UINT16 *fgsrc = NULL;
	if (fgparams.enabled && scanline >= fgparams.veblnk && scanline < fgparams.vsblnk && fgparams.heblnk < fgparams.hsblnk)
	{
		fgsrc = &slave_vram[((fgparams.rowaddr << 8) + (fgparams.yoffset << 7)) & 0xff80];
		fgcoladdr = (fgparams.coladdr >> 1);
	}

	for (INT32 x = params->heblnk; x < params->hsblnk; x += 2)
	{
		UINT16 bgdata, fgdata = 0;
		INT32 ex = x - params->heblnk;
		if (ex >= 0 && ex < nScreenWidth) {

			if (fgsrc != NULL)
				fgdata = fgsrc[fgcoladdr++ & 0x7f];

			bgdata = bgsrc[coladdr++ & 0xff];
			if ((bgdata & 0xe000) == 0xe000)
				dest[ex + 0] = bgdata & 0x7ff;
			else if ((fgdata & 0x00ff) != 0)
				dest[ex + 0] = fgdata & 0x00ff;
			else
				dest[ex + 0] = (bgdata & 0x8000) ? (bgdata & 0x7ff) : (bgdata + 0x800);

			bgdata = bgsrc[coladdr++ & 0xff];
			if ((bgdata & 0xe000) == 0xe000)
				dest[ex + 1] = bgdata & 0x7ff;
			else if ((fgdata & 0xff00) != 0)
				dest[ex + 1] = fgdata >> 8;
			else
				dest[ex + 1] = (bgdata & 0x8000) ? (bgdata & 0x7ff) : (bgdata + 0x800);
		}
	}
	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	TMS34010Open(0);
	TMS34010Reset();
	TMS34010Close();

	TMS34010Open(1);
	TMS34010Reset();
	TMS34010Close();

	M6502Open(0);
	M6502Reset();
	BurnYM2151Reset();
	BurnTimerReset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	M6502Close();

	BurnWatchdogReset();

	DACReset();

	output_last = 0;
	aimpos[0] = aimpos[1] = 0;
	trackball_old[0] = trackball_old[1] = 0;
	soundlatch[0] = soundlatch[1] = 0;
	sound_control = 0;
	dac[0] = dac[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM			= Next; Next += 0x200000;
	DrvSndROM[0]		= Next; Next += 0x010000;
	DrvSndROM[1]		= Next; Next += 0x010000;

	BurnPalette			= (UINT32*)Next; Next += (0x8000 + 0x800) * sizeof(UINT32);

	DrvNVRAM			= Next; Next += 0x001000;

	AllRam				= Next;

	DrvVidRAM[0]		= Next; Next += 0x020000;
	DrvVidRAM[1]		= Next; Next += 0x020000;

	DrvSndRAM[0]		= Next; Next += 0x000800;
	DrvSndRAM[1]		= Next; Next += 0x000800;

	BurnPalRAM			= Next; Next += 0x001000;

	DrvMainRAM			= Next; Next += 0x080000;
	DrvSubRAM			= Next; Next += 0x100000;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(59.55);

	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvSndROM[0] + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1] + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvMainROM + 0x000000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000001,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x020000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x020001,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x040000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x040001,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x060000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x060001,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x080000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x080001,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0a0000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x0a0001,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x180000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x180001,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x1a0000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x1a0001,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x1c0000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x1c0001,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x1e0000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x1e0001,  k++, 2)) return 1;
	}

	TMS34010Init(0);
	TMS34010Open(0);
	TMS34010MapMemory(DrvVidRAM[0],	0x00000000, 0x000fffff, MAP_RAM);
	TMS34010MapMemory(DrvMainRAM,	0x00800000, 0x00bfffff, MAP_RAM);
	TMS34010MapMemory(BurnPalRAM,	0x01800000, 0x01807fff, MAP_RAM);
	TMS34010MapMemory(DrvNVRAM,		0x02800000, 0x02807fff, MAP_RAM);
	TMS34010MapMemory(DrvMainROM,	0x03000000, 0x03ffffff, MAP_ROM);
	TMS34010SetHandlers(0,			exterm_main_read, exterm_main_write);
	TMS34010SetScanlineRender(scanline_cb);
	TMS34010SetToShift(main_to_shift);
	TMS34010SetFromShift(main_from_shift);
	TMS34010SetPixClock(4000000, 2);
	TMS34010SetCpuCyclesPerFrame(((40000000 / 8) * 100) / nBurnFPS);
	TMS34010SetHaltOnReset(0);
	TMS34010Close();

	TMS34010Init(1);
	TMS34010Open(1);
	TMS34010MapMemory(DrvVidRAM[1],	0x00000000, 0x000fffff, MAP_RAM);
	TMS34010MapMemory(DrvSubRAM,	0x04000000, 0x047fffff, MAP_RAM);
	TMS34010SetHandlers(0,			exterm_sub_read, exterm_sub_write);
	TMS34010SetToShift(sub_to_shift);
	TMS34010SetFromShift(sub_from_shift);
	TMS34010SetPixClock(4000000, 2);
	TMS34010SetCpuCyclesPerFrame(((40000000 / 8) * 100) / nBurnFPS);
	TMS34010SetHaltOnReset(1);
	TMS34010Close();

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	for (INT32 i = 0; i < 0x2000; i+= 0x0800) {
		M6502MapMemory(DrvSndRAM[0],	0x0000 | i, 0x07ff | i, MAP_RAM);
	}
	M6502MapMemory(DrvSndROM[0] + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(exterm_sound_main_write);
	M6502SetReadHandler(exterm_sound_main_read);
	M6502Close();

	BurnTimerInit(&nmi_timer_cb, NULL); // for high-freq dac&speech timer
	BurnTimerAttachM6502(2000000);

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	for (INT32 i = 0; i < 0x4000; i+= 0x0800) {
		M6502MapMemory(DrvSndRAM[1],	0x0000 | i, 0x07ff | i, MAP_RAM);
	}
	M6502MapMemory(DrvSndROM[1] + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(exterm_sound_sub_write);
	M6502SetReadHandler(exterm_sound_sub_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180); // ?

	BurnYM2151Init(4000000);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	DACInit(0, 0, 1, M6502TotalCycles, 2000000);
	DACSetRoute(0, 0.40, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);

	GenericTilesInit();

	BurnTrackballInit(2);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	TMS34010Exit();
	M6502Exit();
	BurnTimerExit();

	BurnWatchdogExit();
	BurnTrackballExit();

	BurnYM2151Exit();
	DACExit();

	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit() // static colors
{
	for (INT32 i = 0; i < 32768; i++) {
		BurnPalette[0x800 + i] = BurnHighCol(pal5bit(i >> 10), pal5bit(i >> 5), pal5bit(i >> 0), 0);
	}
}

static void DrvPaletteUpdate() // dynamic ram-based colors
{
	UINT16 *ram = (UINT16*)BurnPalRAM;

	for (INT32 i = 0; i < 0x1000/2; i++){
		BurnPalette[i] = BurnHighCol(pal5bit(ram[i] >> 10), pal5bit(ram[i] >> 5), pal5bit(ram[i] >> 0), 0);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteInit();
		BurnRecalc = 0;
	}

	DrvPaletteUpdate();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	TMS34010NewFrame();
	M6502NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
		DrvInputs[0] = (DrvInputs[0] & ~0x8000) | ((DrvDips[1] & 1) << 15); // Service dip

		BurnTrackballConfig(0, AXIS_REVERSED, AXIS_REVERSED);
		BurnTrackballFrame(0, Analog[0], Analog[1], 0x01, 0x03);
		BurnTrackballUpdate(0);
	}

	INT32 nSegment;
	INT32 nInterleave = 264;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[4] = { ((40000000 / 8) * 100) / nBurnFPS, ((40000000 / 8) * 100) / nBurnFPS, (2000000 * 100) / nBurnFPS, (2000000 * 100) / nBurnFPS };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		TMS34010Open(0);
		CPU_RUN(0, TMS34010);
		TMS34010GenerateScanline(i);
		TMS34010Close();

		TMS34010Open(1);
		CPU_RUN(1, TMS34010);
		TMS34010GenerateScanline(i);
		TMS34010Close();

		M6502Open(0);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[2] / nInterleave));
		if (i == nInterleave - 1) BurnTimerEndFrame(nCyclesTotal[2]);
		M6502Close();

		M6502Open(1);
		CPU_RUN(3, M6502);
		M6502Close();

		if (pBurnSoundOut && (i & 3) == 0) {
			nSegment = nBurnSoundLen / (nInterleave / 4);
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		TMS34010Scan(nAction);
		M6502Scan(nAction);

		BurnTimerScan(nAction, pnMin);

		BurnWatchdogScan(nAction);
		BurnTrackballScan();

		BurnYM2151Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(output_last);
		SCAN_VAR(sound_control);
		SCAN_VAR(trackball_old);
		SCAN_VAR(aimpos);
		SCAN_VAR(dac);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x1000;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Exterminator

static struct BurnRomInfo extermRomDesc[] = {
	{ "v101y1",		0x08000, 0xcbeaa837, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code

	{ "v101d1",		0x08000, 0x83268b7d, 2 | BRF_PRG | BRF_ESS }, //  1 M6502 #1 Code

	{ "v101bg0",	0x10000, 0x8c8e72cf, 3 | BRF_PRG | BRF_ESS }, //  2 TMS34010 #0 Code
	{ "v101bg1",	0x10000, 0xcc2da0d8, 3 | BRF_PRG | BRF_ESS }, //  3
	{ "v101bg2",	0x10000, 0x2dcb3653, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "v101bg3",	0x10000, 0x4aedbba0, 3 | BRF_PRG | BRF_ESS }, //  5
	{ "v101bg4",	0x10000, 0x576922d4, 3 | BRF_PRG | BRF_ESS }, //  6
	{ "v101bg5",	0x10000, 0xa54a4bc2, 3 | BRF_PRG | BRF_ESS }, //  7
	{ "v101bg6",	0x10000, 0x7584a676, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "v101bg7",	0x10000, 0xa4f24ff6, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "v101bg8",	0x10000, 0xfda165d6, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "v101bg9",	0x10000, 0xe112a4c4, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "v101bg10",	0x10000, 0xf1a5cf54, 3 | BRF_PRG | BRF_ESS }, // 12
	{ "v101bg11",	0x10000, 0x8677e754, 3 | BRF_PRG | BRF_ESS }, // 13
	{ "v101fg0",	0x10000, 0x38230d7d, 3 | BRF_PRG | BRF_ESS }, // 14
	{ "v101fg1",	0x10000, 0x22a2bd61, 3 | BRF_PRG | BRF_ESS }, // 15
	{ "v101fg2",	0x10000, 0x9420e718, 3 | BRF_PRG | BRF_ESS }, // 16
	{ "v101fg3",	0x10000, 0x84992aa2, 3 | BRF_PRG | BRF_ESS }, // 17
	{ "v101fg4",	0x10000, 0x38da606b, 3 | BRF_PRG | BRF_ESS }, // 18
	{ "v101fg5",	0x10000, 0x842de63a, 3 | BRF_PRG | BRF_ESS }, // 19
	{ "v101p0",		0x10000, 0x6c8ee79a, 3 | BRF_PRG | BRF_ESS }, // 20
	{ "v101p1",		0x10000, 0x557bfc84, 3 | BRF_PRG | BRF_ESS }, // 21
};

STD_ROM_PICK(exterm)
STD_ROM_FN(exterm)

struct BurnDriver BurnDrvExterm = {
	"exterm", NULL, NULL, NULL, "1989",
	"Exterminator\0", NULL, "Gottlieb / Premier Technology", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, extermRomInfo, extermRomName, NULL, NULL, NULL, NULL, ExtermInputInfo, ExtermDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x8800,
	256, 240, 4, 3
};
