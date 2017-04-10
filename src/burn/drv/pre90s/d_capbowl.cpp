// FB Alpha Coors Light Bowling/Bowl-O-Rama driver module
// Based on MAME driver by Zsolt Vasvari

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "dac.h"
#include "burn_ym2203.h"
#include "tms34061.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSoundROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvSoundRAM;

static UINT8 *rowaddress;
static UINT8 *soundlatch;
static UINT8 *bankselect;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT32 blitter_addr;

static INT32 watchdog;
static INT32 game_select;
static INT32 previous_scanline;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT32 DrvAnalogPort0 = 0;
static INT32 DrvAnalogPort1 = 0;
static INT32 track_x_last = 0;
static INT32 track_y_last = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo CapbowlInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};
#undef A

STDINPUTINFO(Capbowl)

static struct BurnDIPInfo CapbowlDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x40, NULL		},
	{0x0b, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    2, "Cabinet"	},
	{0x0a, 0x01, 0x40, 0x40, "Upright"	},
	{0x0a, 0x01, 0x40, 0x00, "Cocktail"	},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x0b, 0x01, 0x01, 0x00, "Off"		},
	{0x0b, 0x01, 0x01, 0x01, "On"		},
};

STDDIPINFO(Capbowl)

static void bankswitch(INT32 d)
{
	if (game_select != 0) return; // capbowl only

	*bankselect = d;

	int bank = 0x08000 + (((d & 0x0c) >> 1) | (d & 1)) * 0x4000;

	M6809MapMemory(DrvMainROM + bank, 0x0000, 0x3fff, MAP_ROM);
}

static void TrackReset()
{
	track_x_last = 0;
	track_y_last = 0;
}

static UINT8 ananice(INT16 anaval)
{
	if (anaval > 1024) anaval = 1024;
	if (anaval < -1024) anaval = -1024; // clamp huge values so don't overflow INT8 conversion (mouse)

	return (anaval >> 4) & 0xff;
}

static UINT8 ProcessTrack(UINT8 pad)
{
	if ((pad & 0xf0) == 0xf0 || pad < 0x10) pad = 0;
	pad = (pad>>4);
	if (pad & 0x10) pad = 0x15-pad;

	return pad;
}

static UINT8 TrackY()
{
	UINT8 pad = ananice(DrvAnalogPort1);

	pad = ProcessTrack(0xff - pad) & 0xf; // reversed
	if (pad) track_y_last = pad;

	return ((pad) ? pad : track_y_last);
}

static UINT8 TrackX()
{
	UINT8 pad = ananice(DrvAnalogPort0);

	pad = ProcessTrack(pad) & 0xf;
	if (pad) track_x_last = pad;

	return ((pad) ? pad : track_x_last);
}

static void TrackTick()
{ // executes once every 8th frame to simulate linear deceleration
	if (!(nCurrentFrame & 8)) return;
	if (track_y_last !=0) {
		if (track_y_last>0 && track_y_last < 9) track_y_last--;
		else if (track_y_last > 9) { track_y_last++; if (track_y_last>0xf) track_y_last = 0; }
	}

	if (track_x_last !=0) {
		if (track_x_last>0 && track_x_last < 9) track_x_last--;
		else if (track_x_last > 9) { track_x_last++; if (track_x_last>0xf) track_x_last = 0; }
	}
}

static void main_write(UINT16 a, UINT8 d)
{
	if ((a & 0xf800) == 0x5800) {
		tms34061_write((a & 0xff) ^ ((~a & 0x100) >> 7), *rowaddress, (a >> 8) & 3, d);
		return;
	}

	switch (a)
	{
		case 0x0008:
			blitter_addr = (blitter_addr & 0x00ffff) | (d << 16);
		return;

		case 0x0017:
			blitter_addr = (blitter_addr & 0xff00ff) | (d << 8);
		return;

		case 0x0018:
			blitter_addr = (blitter_addr & 0xffff00) | (d << 0);
		return;

		case 0x4000:
			*rowaddress = d;
		return;

		case 0x4800:
			bankswitch(d);
		return;

		case 0x6000:
		{
			*soundlatch = d;
			M6809Close();
			M6809Open(1);
			M6809SetIRQLine(M6809_IRQ_LINE, CPU_IRQSTATUS_AUTO);
			M6809Close();
			M6809Open(0);
		}
		return;

		case 0x6800:
			watchdog = 0;
			TrackReset();
		return;
	}
}

static UINT8 main_read(UINT16 a)
{
	if ((a & 0xf800) == 0x5800) {
		return tms34061_read((a & 0xff) ^ ((~a & 0x100) >> 7), *rowaddress, (a >> 8) & 3);
	}

	switch (a)
	{
		case 0x0000:
		{
			UINT8 data = DrvGfxROM[blitter_addr];
			if ((data & 0xf0) == 0x00) data |= 0xf0;
			if ((data & 0x0f) == 0x00) data |= 0x0f;
			return data;
		}

		case 0x0004:
		{
			UINT8 data = DrvGfxROM[(blitter_addr & 0x3ffff)];
			blitter_addr = (blitter_addr + 1) & 0x3ffff;
			return data;
		}

		case 0x6800: // nop
			return 0;

		case 0x7000:
			return (DrvInputs[0] & 0xb0) | (DrvDips[0] & 0x40) | TrackY(); // track Y

		case 0x7800:
			return (DrvInputs[1] & 0xf0) | TrackX(); // track X
	}

	return 0;
}

static void sound_write(UINT16 a, UINT8 d)
{
	switch (a)
	{
		case 0x1000:
		case 0x1001:
			BurnYM2203Write(0, a & 1, d);
		return;

		case 0x2000:
		return; // nop

		case 0x6000:
			DACSignedWrite(0, d);
		return;
	}
}

static UINT8 sound_read(UINT16 a)
{
	switch (a)
	{
		case 0x1000:
		case 0x1001:
			return BurnYM2203Read(0, a & 1);

		case 0x7000:
			return *soundlatch;
	}

	return 0;
}

static UINT8 capbowl_ym2203_portA(UINT32)
{
	// ticket handling
	return 0;
}

static void capbowl_ym2203_write_portB(UINT32, UINT32 )
{
	// ticket handling
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (M6809TotalCycles() / (2000000.000 / (nBurnFPS / 100.000))));
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)M6809TotalCycles() * nSoundRate / 2000000;
}

static double DrvGetTime()
{
	return (double)M6809TotalCycles() / 2000000;
}

static void DrvFMIRQCallback(INT32 , INT32 state)
{
	M6809SetIRQLine(M6809_FIRQ_LINE, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void tms34061_interrupt(INT32 state)
{
	M6809SetIRQLine(M6809_FIRQ_LINE, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void draw_layer()
{
	INT32 current_scanline = tms34061_current_scanline + 1;

	if (tms34061_display_blanked())
	{
		for (INT32 y = previous_scanline; y < current_scanline && y < nScreenHeight; y++) {
			memset (pTransDraw + y * nScreenWidth, 0, nScreenWidth * sizeof(UINT16));
		}
	}
	else
	{
		for (INT32 y = previous_scanline; y < current_scanline && y < nScreenHeight; y++)
		{
			UINT8 *src = tms34061_get_vram_pointer() + y * 256;
			UINT16 *dest = pTransDraw + y * nScreenWidth;

			for (INT32 x = 0; x < nScreenWidth; x += 2)
			{
				UINT8 pix = src[32 + (x / 2)];
				*dest++ = ((src[((pix >> 4) << 1) + 0] << 8) + src[((pix >> 4) << 1) + 1]) & 0xfff;
				*dest++ = ((src[((pix & 15) << 1) + 0] << 8) + src[((pix & 15) << 1) + 1]) & 0xfff;
			}
		}
	}

	previous_scanline = current_scanline;
	if (previous_scanline == 256) previous_scanline = 0;
}

static INT32 DrvDoReset(int clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	M6809Open(1);
	M6809Reset();
	BurnYM2203Reset();
	DACReset();
	M6809Close();

	tms34061_reset();

	memset (DrvNVRAM, 0x01, 0x800);

	watchdog = 0;
	blitter_addr = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x020000;
	DrvSoundROM		= Next; Next += 0x008000;

	DrvGfxROM		= Next; Next += 0x040000;

	DrvNVRAM		= Next; Next += 0x000800;

	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(INT32);

	AllRam			= Next;

	DrvSoundRAM		= Next; Next += 0x000800;

	rowaddress		= Next; Next += 0x000001;
	soundlatch		= Next; Next += 0x000001;
	bankselect		= Next; Next += 0x000001;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x1000; i++) {
		DrvPalette[i] = BurnHighCol((i>>8)|((i>>4)&0xf0), (i&0xf0)|((i>>4)&0x0f), (i&0x0f)|((i&0x0f)<<4), 0);
	}
}

static INT32 DrvInit(INT32 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		game_select = game;

		if (game == 0) // capbowl
		{
			if (BurnLoadRom(DrvMainROM + 0x00000,  0, 1)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x08000,  1, 1)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x10000,  2, 1)) return 1;
			if (BurnLoadRom(DrvMainROM + 0x18000,  3, 1)) return 1;

			if (BurnLoadRom(DrvSoundROM + 0x0000,  4, 1)) return 1;
		}
		else
		{
			if (BurnLoadRom(DrvMainROM  + 0x0000,  0, 1)) return 1;

			if (BurnLoadRom(DrvSoundROM + 0x0000,  1, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM   + 0x0000,  2, 1)) return 1;
		}

		DrvPaletteInit();
	}

	BurnSetRefreshRate(57.00);

	M6809Init(2);
	M6809Open(0);
	M6809MapMemory(DrvNVRAM,		0x5000, 0x57ff, MAP_RAM);
	M6809MapMemory(DrvMainROM,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(main_write);
	M6809SetReadHandler(main_read);
	M6809Close();

	M6809Open(1);
	M6809MapMemory(DrvSoundRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvSoundROM,		0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(sound_write);
	M6809SetReadHandler(sound_read);
	M6809Close();

	BurnYM2203Init(1, 4000000, DrvFMIRQCallback, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachM6809(2000000);
	BurnYM2203SetPorts(0, &capbowl_ym2203_portA, NULL, NULL, &capbowl_ym2203_write_portB);

	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);

	tms34061_init(8, 0x10000, draw_layer, tms34061_interrupt);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	DACExit();

	BurnYM2203Exit();

	M6809Exit();

	tms34061_exit();

	BurnFree(AllMem);

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog > 120) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xf0, 2);

		for (INT32 i = 4 ; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	TrackTick();

	M6809NewFrame();

	INT32 nInterleave = 256; // scanlines
	INT32 nCyclesSegment = 0;
	INT32 nCyclesTotal[2] =  { 4000000 / 57, 2000000 / 57 };
	INT32 nCyclesDone[2] =  { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		tms34061_current_scanline = i;

		nCurrentCPU = 0;
		M6809Open(nCurrentCPU);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += M6809Run(nCyclesSegment);

		tms34061_interrupt();

		if (((i + ((game_select) ? 0x10 : 0x00)) & 0x1f) == 0x1f) { // force draw every 32 scanlines
			draw_layer();
		}

		M6809Close();

		nCurrentCPU = 1;
		M6809Open(nCurrentCPU);
		BurnTimerUpdate((i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave);
		M6809Close();
	}

	M6809Open(0);
	if (DrvDips[1] & 0x01) {
		M6809SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // NMI
	}
	M6809Close();

	M6809Open(1);
	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}


static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029695;
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x000800;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);

		tms34061_scan(nAction, pnMin);

		BurnYM2203Scan(nAction, pnMin);
		BurnTimerScan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(blitter_addr);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		bankswitch(*bankselect);
		M6809Close();
	}

	return 0;
}



// Capcom Bowling (set 1)

static struct BurnRomInfo capbowlRomDesc[] = {
	{ "u6",				0x8000, 0x14924c96, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gr0",			0x8000, 0xef53ca7a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gr1",			0x8000, 0x27ede6ce, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gr2",			0x8000, 0xe49238f4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sound.u30",			0x8000, 0x8c9c3b8a, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code
};

STD_ROM_PICK(capbowl)
STD_ROM_FN(capbowl)

static INT32 CapbowlInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvCapbowl = {
	"capbowl", NULL, NULL, NULL, "1988",
	"Capcom Bowling (set 1)\0", NULL, "Incredible Technologies / Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, capbowlRomInfo, capbowlRomName, NULL, NULL, CapbowlInputInfo, CapbowlDIPInfo,
	CapbowlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	244, 360, 3, 4
};


// Capcom Bowling (set 2)

static struct BurnRomInfo capbowl2RomDesc[] = {
	{ "program_rev_3_u6.u6",	0x8000, 0x9162934a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "gr0",			0x8000, 0xef53ca7a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gr1",			0x8000, 0x27ede6ce, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gr2",			0x8000, 0xe49238f4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sound.u30",			0x8000, 0x8c9c3b8a, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code
};

STD_ROM_PICK(capbowl2)
STD_ROM_FN(capbowl2)

struct BurnDriver BurnDrvCapbowl2 = {
	"capbowl2", "capbowl", NULL, NULL, "1988",
	"Capcom Bowling (set 2)\0", NULL, "Incredible Technologies / Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, capbowl2RomInfo, capbowl2RomName, NULL, NULL, CapbowlInputInfo, CapbowlDIPInfo,
	CapbowlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	244, 360, 3, 4
};


// Capcom Bowling (set 3)

static struct BurnRomInfo capbowl3RomDesc[] = {
	{ "3.0_bowl.u6",		0x8000, 0x32e30928, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "grom0-gr0.gr0",		0x8000, 0x2b5eb091, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "grom1-gr1.gr1",		0x8000, 0x880e4e1c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "grom2-gr2.gr2",		0x8000, 0xf3d2468d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sound_r2_u30.u30",		0x8000, 0x43ac1658, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code
};

STD_ROM_PICK(capbowl3)
STD_ROM_FN(capbowl3)

struct BurnDriver BurnDrvCapbowl3 = {
	"capbowl3", "capbowl", NULL, NULL, "1988",
	"Capcom Bowling (set 3)\0", NULL, "Incredible Technologies / Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, capbowl3RomInfo, capbowl3RomName, NULL, NULL, CapbowlInputInfo, CapbowlDIPInfo,
	CapbowlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	244, 360, 3, 4
};


// Capcom Bowling (set 4)

static struct BurnRomInfo capbowl4RomDesc[] = {
	{ "bfb.u6",			0x8000, 0x79f1d083, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "grom0-gr0.gr0",		0x8000, 0x2b5eb091, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "grom1-gr1.gr1",		0x8000, 0x880e4e1c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "grom2-gr2.gr2",		0x8000, 0xf3d2468d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bfb.u30",			0x8000, 0x6fe2c4ff, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code
};

STD_ROM_PICK(capbowl4)
STD_ROM_FN(capbowl4)

struct BurnDriver BurnDrvCapbowl4 = {
	"capbowl4", "capbowl", NULL, NULL, "1988",
	"Capcom Bowling (set 4)\0", NULL, "Incredible Technologies / Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, capbowl4RomInfo, capbowl4RomName, NULL, NULL, CapbowlInputInfo, CapbowlDIPInfo,
	CapbowlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	244, 360, 3, 4
};


// Coors Light Bowling

static struct BurnRomInfo clbowlRomDesc[] = {
	{ "cb8_prg.u6",			0x8000, 0x91e06bc4, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "coors_bowling_grom0.gr0",	0x8000, 0x899c8f15, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "coors_bowling_grom1.gr1",	0x8000, 0x0ac0dc4c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "coors_bowling_grom2.gr2",	0x8000, 0x251f5da5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "coors_bowling_sound.u30",	0x8000, 0x1eba501e, 2 | BRF_PRG | BRF_ESS }, //  4 M6809 #1 Code
};

STD_ROM_PICK(clbowl)
STD_ROM_FN(clbowl)

struct BurnDriver BurnDrvClbowl = {
	"clbowl", "capbowl", NULL, NULL, "1989",
	"Coors Light Bowling\0", NULL, "Incredible Technologies / Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, clbowlRomInfo, clbowlRomName, NULL, NULL, CapbowlInputInfo, CapbowlDIPInfo,
	CapbowlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	244, 360, 3, 4
};


// Bowl-O-Rama

static struct BurnRomInfo bowlramaRomDesc[] = {
	{ "bowl-o-rama_rev_1.0_u6.u6",		0x08000, 0x7103ad55, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code

	{ "bowl-o-rama_rev_1.0_u30.u30",	0x08000, 0xf3168834, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 #1 Code

	{ "bowl-o-rama_rev_1.0_ux7.ux7",	0x40000, 0x8727432a, 3 | BRF_GRA },           //  2 Graphics Data
};

STD_ROM_PICK(bowlrama)
STD_ROM_FN(bowlrama)

static INT32 BowlramaInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvBowlrama = {
	"bowlrama", NULL, NULL, NULL, "1991",
	"Bowl-O-Rama\0", NULL, "P&P Marketing", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, bowlramaRomInfo, bowlramaRomName, NULL, NULL, CapbowlInputInfo, CapbowlDIPInfo,
	BowlramaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	240, 360, 3, 4
};
