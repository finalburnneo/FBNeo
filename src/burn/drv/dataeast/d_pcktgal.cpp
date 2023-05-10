// FinalBurn Neo Pocket Gal driver module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "decobac06.h"
#include "burn_ym2203.h"
#include "burn_ym3812.h"
#include "msm5205.h"
#include "bitswap.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSoundROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvColPROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvSoundRAM;
static UINT8 *DrvPfRAM;
static UINT8 *DrvSprRAM;

static INT32 main_bank;
static INT32 sound_bank;
static INT32 soundlatch;
static INT32 soundtoggle;
static INT32 msm5205next;
static UINT16 pf_control[2][4];
static UINT16 dummy_control[2][4]; // bootleg

static INT32 is_bootleg = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo PcktgalInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Pcktgal)

static struct BurnDIPInfo PcktgalDIPList[]=
{
	{0x11, 0xff, 0xff, 0xbf, NULL					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{0x11, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x11, 0x01, 0x04, 0x04, "Off"					},
	{0x11, 0x01, 0x04, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow 2 Players Game"	},
	{0x11, 0x01, 0x08, 0x00, "No"					},
	{0x11, 0x01, 0x08, 0x08, "Yes"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x11, 0x01, 0x10, 0x00, "Off"					},
	{0x11, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Time"					},
	{0x11, 0x01, 0x20, 0x00, "100"					},
	{0x11, 0x01, 0x20, 0x20, "120"					},

	{0   , 0xfe, 0   ,    2, "Lives"				},
	{0x11, 0x01, 0x40, 0x00, "3"					},
	{0x11, 0x01, 0x40, 0x40, "4"					},
};

STDDIPINFO(Pcktgal)

static void main_bankswitch(INT32 data)
{
	main_bank = data;

	M6502MapMemory(DrvMainROM + 0x4000 + (~data & 1) * 0xc000, 0x4000, 0x5fff, MAP_ROM);
	M6502MapMemory(DrvMainROM + 0x6000 + (~data & 2) * 0x6000, 0x6000, 0x7fff, MAP_ROM);
}

static void sound_bankswitch(INT32 data)
{
	sound_bank = data;

	M6502MapMemory(DrvSoundROM + 0x10000 + (data & 4) * 0x1000, 0x4000, 0x7fff, MAP_ROM);
}

static void pcktgal_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0x800) {
		DrvPfRAM[(address & 0x7ff) ^ 1] = data;
		return;
	}

	if ((address & 0xfff8) == 0x1800) {
		pf_control[0][(address / 2) & 3] = data;
		return;
	}

	if ((address & 0xfff0) == 0x1810) {
		INT32 offset = address & 0xf;
		if (offset < 4) {
			UINT8 *pf = (UINT8*)pf_control[1];
			pf[offset] = data;
		} else {
		//	pf_control[1][offset / 2] = data;
		}
		return;
	}

	switch (address)
	{
		case 0x1a00:
			soundlatch = data;
			M6502SetIRQLine(1, CPU_IRQLINE_NMI, CPU_IRQSTATUS_AUTO);
		return;

		case 0x1c00:
			main_bankswitch(data);
		return;
	}
}

static UINT8 pcktgal_main_read(UINT16 address)
{
	if ((address & 0xf800) == 0x800) {
		return DrvPfRAM[(address & 0x7ff) ^ 1];
	}

	if ((address & 0xfff0) == 0x1810) {
		INT32 offset = address & 0xf;
		if (offset < 4) {
			UINT8 *pf = (UINT8*)pf_control[1];
			return pf[offset];
		} else {
		//	return pf_control[1][offset / 2];
		}
	}

	switch (address)
	{
		case 0x1800:
			return DrvInputs[0];

		case 0x1a00:
			return DrvInputs[1];

		case 0x1c00:
			return DrvDips[0];
	}

	return 0;
}

static void pcktgal_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x0800:
		case 0x0801:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0x1000:
		case 0x1001:
			BurnYM3812Write(0, address & 1, data);
		return;

		case 0x1800:
			msm5205next = data;
		return;

		case 0x2000:
			sound_bankswitch(data);
			MSM5205ResetWrite(0, (data & 2) >> 1);
		return;
	}
}

static UINT8 pcktgal_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			return soundlatch;

		case 0x3400:
			return 0; // ?
	}

	return 0;
}

static void msm5205_interrupt()
{
	MSM5205DataWrite(0, msm5205next >> 4);
	msm5205next <<= 4;

	soundtoggle ^= 1;
	if (soundtoggle) M6502SetIRQLine(1, 0, CPU_IRQSTATUS_AUTO);
}

static INT32 SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)M6502TotalCycles() * nSoundRate / 1500000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	MSM5205Reset();
	M6502Close();

	BurnYM2203Reset();
	BurnYM3812Reset();

	soundlatch = 0;
	soundtoggle = 0;
	msm5205next = 0;
	memset (pf_control, 0, sizeof(pf_control));

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x020000;
	DrvSoundROM		= Next; Next += 0x020000;

	DrvGfxROM[0]	= Next; Next += 0x040000;
	DrvGfxROM[1]	= Next; Next += 0x020000;

	DrvColPROM		= Next; Next += 0x000400;

	BurnPalette		= (UINT32*)Next; Next += 0x200 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x000800;
	DrvSoundRAM		= Next; Next += 0x000800;
	DrvPfRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000200;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { 0x10000*8, 0, 0x18000*8, 0x8000*8 };
	INT32 Plane1[2] = { 0x8000*8, 0 };
	INT32 XOffs[16] = { STEP8(128,1), STEP8(0,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x20000; i++) {
		tmp[i] = DrvGfxROM[0][i ^ (is_bootleg ? 0x8000 : 0x10)];
	}

	GfxDecode(0x1000, 4, 8, 8, Plane0, XOffs + 8, YOffs, 0x040, tmp, DrvGfxROM[0]);

	for (INT32 i = 0; i < 0x10000; i++) {
		tmp[i] = (is_bootleg) ? BITSWAP08(DrvGfxROM[1][i], 0, 1, 2, 3, 4, 5, 6, 7) : DrvGfxROM[1][i];
	}

	GfxDecode(0x200, 2, 16, 16, Plane1, XOffs + 0, YOffs, 0x100, tmp, DrvGfxROM[1]);

	BurnFree(tmp);

	return 0;
}

static INT32 CommonInit(INT32 is_pcktgal)
{
	is_bootleg = BurnDrvGetFlags() & BDF_BOOTLEG;

	BurnAllocMemIndex();

	{
		INT32 t = 0;
		if (BurnLoadRom(DrvMainROM + 0x10000, t++, 1)) return 1;
		memcpy (DrvMainROM + 0x4000, DrvMainROM + 0x14000, 0xc000);

		if (BurnLoadRom(DrvSoundROM + 0x10000, t++, 1)) return 1;
		memcpy (DrvSoundROM + 0x8000, DrvSoundROM + 0x18000, 0x8000);

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, t++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x10000, t++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, t++, 1)) return 1;
		if (is_bootleg) if (BurnLoadRom(DrvGfxROM[1] + 0x08000, t++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, t++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, t++, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvMainRAM,				0x0000, 0x07ff, MAP_RAM);
//	M6502MapMemory(DrvPfRAM,				0x0800, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,				0x1000, 0x11ff, MAP_RAM);
	M6502MapMemory(DrvMainROM + 0x8000,		0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(pcktgal_main_write);
	M6502SetReadHandler(pcktgal_main_read);
	M6502Close();

	M6502Init(1, is_pcktgal ? TYPE_DECO222 : TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvSoundRAM,				0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvSoundROM + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(pcktgal_sound_write);
	M6502SetReadHandler(pcktgal_sound_read);
	M6502Close();

	BurnYM2203Init(1, 1500000, NULL, 0);
	BurnTimerAttachM6502(2000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.60, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);

	BurnYM3812Init(1, 3000000, NULL, 1);
	BurnTimerAttachYM3812(&M6502Config, 1500000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, SynchroniseStream, 384000, msm5205_interrupt, MSM5205_S48_4B, 1);
	MSM5205SetRoute(0, 0.70, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	bac06_yadjust = 16; // -16
	memset (dummy_control, 0, sizeof(dummy_control));
	dummy_control[0][0] = 1; // force 8x8

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	M6502Exit();

	BurnYM2203Exit();
	BurnYM3812Exit();
	MSM5205Exit();

	BurnFreeMemIndex();

	is_bootleg = 0;

	return 0;
}

static void BurnPaletteInit()
{
	for (INT32 i = 0; i < 0x200; i++)
	{
		INT32 bit0, bit1, bit2, bit3;
		bit0 = BIT(DrvColPROM[i], 0);
		bit1 = BIT(DrvColPROM[i], 1);
		bit2 = BIT(DrvColPROM[i], 2);
		bit3 = BIT(DrvColPROM[i], 3);
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = BIT(DrvColPROM[i], 4);
		bit1 = BIT(DrvColPROM[i], 5);
		bit2 = BIT(DrvColPROM[i], 6);
		bit3 = BIT(DrvColPROM[i], 7);
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;
		bit0 = BIT(DrvColPROM[i + 0x200], 0);
		bit1 = BIT(DrvColPROM[i + 0x200], 1);
		bit2 = BIT(DrvColPROM[i + 0x200], 2);
		bit3 = BIT(DrvColPROM[i + 0x200], 3);
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		BurnPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x200; offs += 4)
	{
		INT32 sx = 240 - DrvSprRAM[offs+2];
		INT32 sy = 240 - DrvSprRAM[offs+0];
		INT32 attr = DrvSprRAM[offs+1];
		INT32 code = DrvSprRAM[offs+3] + ((attr & 1) << 8);
		INT32 color = (attr & 0x70) >> 4;
		INT32 flipx = attr & 0x04;
		INT32 flipy = attr & 0x02;

		Draw16x16MaskTile(pTransDraw, code, sx, sy - 16, flipx, flipy, color, 2, 0, 0, DrvGfxROM[1]);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		BurnPaletteInit();
		BurnRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) bac06_draw_layer(DrvPfRAM, is_bootleg ? dummy_control : pf_control, NULL, NULL, DrvGfxROM[0], 0x100, 0xfff, DrvGfxROM[0], 0x100, 0, 0, 1);

	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		// clear opposites
		if ((DrvInputs[0] & 0x03) == 0) DrvInputs[0] |= 0x03;
		if ((DrvInputs[0] & 0x0c) == 0) DrvInputs[0] |= 0x0c;
		if ((DrvInputs[1] & 0x03) == 0) DrvInputs[1] |= 0x03;
		if ((DrvInputs[1] & 0x0c) == 0) DrvInputs[1] |= 0x0c;
	}

	INT32 nInterleave = MSM5205CalcInterleave(0, 1500000);
	INT32 nCyclesTotal[2] = { 2000000 / 60, 1500000 / 60 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6502Open(0);
		CPU_RUN_TIMER(0);
		if (i == (nInterleave - 1)) M6502SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_AUTO);
		M6502Close();

		M6502Open(1);
		CPU_RUN_TIMER_YM3812(1);
		MSM5205Update();
		M6502Close();
	}

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
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

		M6502Scan(nAction);
		BurnYM2203Scan(nAction, pnMin);
		BurnYM3812Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(main_bank);
		SCAN_VAR(sound_bank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(soundtoggle);
		SCAN_VAR(msm5205next);
		SCAN_VAR(pf_control);
	}

	if (nAction & ACB_WRITE)
	{
		M6502Open(0);
		main_bankswitch(main_bank);
		M6502Close();

		M6502Open(1);
		sound_bankswitch(sound_bank);
		M6502Close();
	}

	return 0;
}


// Pocket Gal (Japan)

static struct BurnRomInfo pcktgalRomDesc[] = {
	{ "eb04.j7",		0x10000, 0x8215d60d, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code

	{ "eb03.f2",		0x10000, 0xcb029b02, 2 | BRF_PRG | BRF_ESS }, //  1 DECO 222 Encrypted CPU

	{ "eb01.d11",		0x10000, 0x63542c3d, 3 | BRF_GRA },           //  2 Background Tiles
	{ "eb02.d12",		0x10000, 0xa9dcd339, 3 | BRF_GRA },           //  3

	{ "eb00.a1",		0x10000, 0x6c1a14a8, 4 | BRF_GRA },           //  4 Sprites

	{ "eb05.k14",		0x00200, 0x3b6198cb, 5 | BRF_GRA },           //  5 Color PROMs
	{ "eb06.k15",		0x00200, 0x1fbd4b59, 5 | BRF_GRA },           //  6
};

STD_ROM_PICK(pcktgal)
STD_ROM_FN(pcktgal)

static INT32 DrvInit()
{
	return CommonInit(1);
}

struct BurnDriver BurnDrvPcktgal = {
	"pcktgal", NULL, NULL, NULL, "1987",
	"Pocket Gal (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, pcktgalRomInfo, pcktgalRomName, NULL, NULL, NULL, NULL, PcktgalInputInfo, PcktgalDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 224, 4, 3
};


// Pocket Gal (Yada East bootleg)

static struct BurnRomInfo pcktgalbRomDesc[] = {
	{ "sexybill.001",	0x10000, 0x4acb3e84, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code

	{ "eb03.f2",		0x10000, 0xcb029b02, 2 | BRF_PRG | BRF_ESS }, //  1 DECO 222 Encrypted CPU

	{ "sexybill.005",	0x10000, 0x3128dc7b, 3 | BRF_GRA },           //  2 Background Tiles
	{ "sexybill.006",	0x10000, 0x0fc91eeb, 3 | BRF_GRA },           //  3

	{ "sexybill.003",	0x08000, 0x58182daa, 4 | BRF_GRA },           //  4 Sprites
	{ "sexybill.004",	0x08000, 0x33a67af6, 4 | BRF_GRA },           //  5

	{ "eb05.k14",		0x00200, 0x3b6198cb, 5 | BRF_GRA },           //  6 Color PROMs
	{ "eb06.k15",		0x00200, 0x1fbd4b59, 5 | BRF_GRA },           //  7

	{ "pal16l8",		0x00104, 0xb8d4b318, 6 | BRF_OPT },           //  8 PLDs
	{ "pal16r6",		0x00104, 0x43aad537, 6 | BRF_OPT },           //  9
};

STD_ROM_PICK(pcktgalb)
STD_ROM_FN(pcktgalb)

struct BurnDriver BurnDrvPcktgalb = {
	"pcktgalb", "pcktgal", NULL, NULL, "1987",
	"Pocket Gal (Yada East bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, pcktgalbRomInfo, pcktgalbRomName, NULL, NULL, NULL, NULL, PcktgalInputInfo, PcktgalDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 224, 4, 3
};


// Pocket Gal / unknown card game
// strange bootleg with 2 connected PCBs, one for the Pocket Gal bootleg and one for an unknown card game.
// Pocket Gal used as cover for a stealth gambling game?

static struct BurnRomInfo pcktgalbaRomDesc[] = {
	// Pocket Gal PCB: standard chips emulated in this driver, with no Data East customs
	{ "sex_v1.3",	0x10000, 0xe278da6b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code

	{ "2_sex",		0x10000, 0xcb029b02, 2 | BRF_PRG | BRF_ESS }, //  1 DECO 222 Encrypted CPU

	{ "5_sex",		0x10000, 0x3128dc7b, 3 | BRF_GRA },           //  2 Background Tiles
	{ "6_sex",		0x10000, 0x0fc91eeb, 3 | BRF_GRA },           //  3

	{ "3_sex",		0x08000, 0x58182daa, 4 | BRF_GRA },           //  4 Sprites
	{ "4_sex",		0x08000, 0x33a67af6, 4 | BRF_GRA },           //  5

	{ "prom1",		0x00200, 0x3b6198cb, 5 | BRF_GRA },           //  6 Color PROMs / BAD DUMP
	{ "prom2",		0x00200, 0x1fbd4b59, 5 | BRF_GRA },           //  7             / BAD DUMP

	// unknown card game PCB: Z84C00AB6 (Z80), 2 scratched off chips (possibly I8255?), AY38912A/P, 4 8-dip banks
	{ "7_sex.u45",	0x04000, 0x65b0b6d0, 7 | BRF_PRG | BRF_ESS }, //  8 Z80 Code

	{ "8_sex.u35",	0x02000, 0x36e450e5, 8 | BRF_GRA },           //  9 Graphics
	{ "9_sex.u36",	0x02000, 0xffcc1198, 8 | BRF_GRA },           // 10
	{ "10_sex.u37",	0x02000, 0x73cf56a0, 8 | BRF_GRA },           // 11
};

STD_ROM_PICK(pcktgalba)
STD_ROM_FN(pcktgalba)

struct BurnDriver BurnDrvPcktgalba = {
	"pcktgalba", "pcktgal", NULL, NULL, "1987",
	"Pocket Gal / unknown card game\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, pcktgalbaRomInfo, pcktgalbaRomName, NULL, NULL, NULL, NULL, PcktgalInputInfo, PcktgalDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 224, 4, 3
};


// Pocket Gal 2 (English)

static struct BurnRomInfo pcktgal2RomDesc[] = {
	{ "eb04-2.j7",		0x10000, 0x0c7f2905, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code

	{ "eb03-2.f2",		0x10000, 0x9408ffb4, 2 | BRF_PRG | BRF_ESS }, //  1 M6502 #1 Code

	{ "eb01-2.rom",		0x10000, 0xe52b1f97, 3 | BRF_GRA },           //  2 Background Tiles
	{ "eb02-2.rom",		0x10000, 0xf30d965d, 3 | BRF_GRA },           //  3

	{ "eb00.a1",		0x10000, 0x6c1a14a8, 4 | BRF_GRA },           //  4 Sprites

	{ "eb05.k14",		0x00200, 0x3b6198cb, 5 | BRF_GRA },           //  5 Color PROMs
	{ "eb06.k15",		0x00200, 0x1fbd4b59, 5 | BRF_GRA },           //  6
};

STD_ROM_PICK(pcktgal2)
STD_ROM_FN(pcktgal2)

static INT32 Drv2Init()
{
	return CommonInit(0);
}

struct BurnDriver BurnDrvPcktgal2 = {
	"pcktgal2", "pcktgal", NULL, NULL, "1989",
	"Pocket Gal 2 (English)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, pcktgal2RomInfo, pcktgal2RomName, NULL, NULL, NULL, NULL, PcktgalInputInfo, PcktgalDIPInfo,
	Drv2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 224, 4, 3
};


// Pocket Gal 2 (Japanese)

static struct BurnRomInfo pcktgal2jRomDesc[] = {
	{ "eb04-2.j7",		0x10000, 0x0c7f2905, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code

	{ "eb03-2.f2",		0x10000, 0x9408ffb4, 2 | BRF_PRG | BRF_ESS }, //  1 M6502 #1 Code

	{ "eb01-2.d11",		0x10000, 0x8f42ab1a, 3 | BRF_GRA },           //  2 Background Tiles
	{ "eb02-2.d12",		0x10000, 0xf394cb35, 3 | BRF_GRA },           //  3

	{ "eb00.a1",		0x10000, 0x6c1a14a8, 4 | BRF_GRA },           //  4 Sprites

	{ "eb05.k14",		0x00200, 0x3b6198cb, 5 | BRF_GRA },           //  5 Color PROMs
	{ "eb06.k15",		0x00200, 0x1fbd4b59, 5 | BRF_GRA },           //  6
};

STD_ROM_PICK(pcktgal2j)
STD_ROM_FN(pcktgal2j)

struct BurnDriver BurnDrvPcktgal2j = {
	"pcktgal2j", "pcktgal", NULL, NULL, "1989",
	"Pocket Gal 2 (Japanese)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, pcktgal2jRomInfo, pcktgal2jRomName, NULL, NULL, NULL, NULL, PcktgalInputInfo, PcktgalDIPInfo,
	Drv2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 224, 4, 3
};


// Super Pool III (English)

static struct BurnRomInfo spool3RomDesc[] = {
	{ "eb04-2.j7",		0x10000, 0x0c7f2905, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code

	{ "eb03-2.f2",		0x10000, 0x9408ffb4, 2 | BRF_PRG | BRF_ESS }, //  1 M6502 #1 Code

	{ "deco2.bin",		0x10000, 0x0a23f0cf, 3 | BRF_GRA },           //  2 Background Tiles
	{ "deco3.bin",		0x10000, 0x55ea7c45, 3 | BRF_GRA },           //  3

	{ "eb00.a1",		0x10000, 0x6c1a14a8, 4 | BRF_GRA },           //  4 Sprites

	{ "eb05.k14",		0x00200, 0x3b6198cb, 5 | BRF_GRA },           //  5 Color PROMs
	{ "eb06.k15",		0x00200, 0x1fbd4b59, 5 | BRF_GRA },           //  6
};

STD_ROM_PICK(spool3)
STD_ROM_FN(spool3)

struct BurnDriver BurnDrvSpool3 = {
	"spool3", "pcktgal", NULL, NULL, "1989",
	"Super Pool III (English)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, spool3RomInfo, spool3RomName, NULL, NULL, NULL, NULL, PcktgalInputInfo, PcktgalDIPInfo,
	Drv2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 224, 4, 3
};


// Super Pool III (I-Vics)

static struct BurnRomInfo spool3iRomDesc[] = {
	{ "de1.bin",		0x10000, 0xa59980fe, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code

	{ "eb03-2.f2",		0x10000, 0x9408ffb4, 2 | BRF_PRG | BRF_ESS }, //  1 M6502 #1 Code

	{ "deco2.bin",		0x10000, 0x0a23f0cf, 3 | BRF_GRA },           //  2 Background Tiles
	{ "deco3.bin",		0x10000, 0x55ea7c45, 3 | BRF_GRA },           //  3

	{ "eb00.a1",		0x10000, 0x6c1a14a8, 4 | BRF_GRA },           //  4 Sprites

	{ "eb05.k14",		0x00200, 0x3b6198cb, 5 | BRF_GRA },           //  5 Color PROMs
	{ "eb06.k15",		0x00200, 0x1fbd4b59, 5 | BRF_GRA },           //  6
};

STD_ROM_PICK(spool3i)
STD_ROM_FN(spool3i)

struct BurnDriver BurnDrvSpool3i = {
	"spool3i", "pcktgal", NULL, NULL, "1990",
	"Super Pool III (I-Vics)\0", NULL, "Data East Corporation (I-Vics license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, spool3iRomInfo, spool3iRomName, NULL, NULL, NULL, NULL, PcktgalInputInfo, PcktgalDIPInfo,
	Drv2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x200,
	256, 224, 4, 3
};
