// FinalBurn Neo Mahjong Kyou Jidai driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "msm5205.h"
#include "8255ppi.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[2];
static UINT8 *DrvColPROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 display_enable;
static INT32 nmi_enable;
static INT32 flipscreen;
static INT32 bankdata;
static INT32 keyb;
static INT32 adpcm_pos;
static INT32 adpcm_end;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvJoy7[8];
static UINT8 DrvJoy8[8];
static UINT8 DrvJoy9[8];
static UINT8 DrvJoy10[8];
static UINT8 DrvJoy11[8];
static UINT8 DrvJoy12[8];
static UINT8 DrvJoy13[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[14];
static UINT8 DrvReset;

static struct BurnInputInfo MjkjidaiInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy13 + 7,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy6 + 0,	"p1 start"	},
	{"P1 A",				BIT_DIGITAL,	DrvJoy1 + 0,	"mah a"		},
	{"P1 B",				BIT_DIGITAL,	DrvJoy1 + 1,	"mah b"		},
	{"P1 C",				BIT_DIGITAL,	DrvJoy1 + 2,	"mah c"		},
	{"P1 D",				BIT_DIGITAL,	DrvJoy1 + 3,	"mah d"		},
	{"P1 E",				BIT_DIGITAL,	DrvJoy2 + 0,	"mah e"		},
	{"P1 F",				BIT_DIGITAL,	DrvJoy2 + 1,	"mah f"		},
	{"P1 G",				BIT_DIGITAL,	DrvJoy2 + 2,	"mah g"		},
	{"P1 H",				BIT_DIGITAL,	DrvJoy2 + 3,	"mah h"		},
	{"P1 I",				BIT_DIGITAL,	DrvJoy3 + 0,	"mah i"		},
	{"P1 J",				BIT_DIGITAL,	DrvJoy3 + 1,	"mah j"		},
	{"P1 K",				BIT_DIGITAL,	DrvJoy3 + 2,	"mah k"		},
	{"P1 L",				BIT_DIGITAL,	DrvJoy3 + 3,	"mah l"		},
	{"P1 M",				BIT_DIGITAL,	DrvJoy4 + 0,	"mah m"		},
	{"P1 N",				BIT_DIGITAL,	DrvJoy4 + 1,	"mah n"		},
	{"P1 Pon",				BIT_DIGITAL,	DrvJoy4 + 3,	"mah pon"	},
	{"P1 Chi",				BIT_DIGITAL,	DrvJoy4 + 2,	"mah chi"	},
	{"P1 Kan",				BIT_DIGITAL,	DrvJoy5 + 0,	"mah kan"	},
	{"P1 Ron",				BIT_DIGITAL,	DrvJoy5 + 2,	"mah ron"	},
	{"P1 Reach",			BIT_DIGITAL,	DrvJoy5 + 1,	"mah reach"	},

	{"P2 Start",			BIT_DIGITAL,	DrvJoy12 + 0,	"p2 start"	},
	{"P2 A",				BIT_DIGITAL,	DrvJoy7 + 0,	"mah a"		},
	{"P2 B",				BIT_DIGITAL,	DrvJoy7 + 1,	"mah b"		},
	{"P2 C",				BIT_DIGITAL,	DrvJoy7 + 2,	"mah c"		},
	{"P2 D",				BIT_DIGITAL,	DrvJoy7 + 3,	"mah d"		},
	{"P2 E",				BIT_DIGITAL,	DrvJoy8 + 0,	"mah e"		},
	{"P2 F",				BIT_DIGITAL,	DrvJoy8 + 1,	"mah f"		},
	{"P2 G",				BIT_DIGITAL,	DrvJoy8 + 2,	"mah g"		},
	{"P2 H",				BIT_DIGITAL,	DrvJoy8 + 3,	"mah h"		},
	{"P2 I",				BIT_DIGITAL,	DrvJoy9 + 0,	"mah i"		},
	{"P2 J",				BIT_DIGITAL,	DrvJoy9 + 1,	"mah j"		},
	{"P2 K",				BIT_DIGITAL,	DrvJoy9 + 2,	"mah k"		},
	{"P2 L",				BIT_DIGITAL,	DrvJoy9 + 3,	"mah l"		},
	{"P2 M",				BIT_DIGITAL,	DrvJoy10 + 0,	"mah m"		},
	{"P2 N",				BIT_DIGITAL,	DrvJoy10 + 1,	"mah n"		},
	{"P2 Pon",				BIT_DIGITAL,	DrvJoy10 + 3,	"mah pon"	},
	{"P2 Chi",				BIT_DIGITAL,	DrvJoy10 + 2,	"mah chi"	},
	{"P2 Kan",				BIT_DIGITAL,	DrvJoy11 + 0,	"mah kan"	},
	{"P2 Ron",				BIT_DIGITAL,	DrvJoy11 + 2,	"mah ron"	},
	{"P2 Reach",			BIT_DIGITAL,	DrvJoy11 + 1,	"mah reach"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Clear Settings",		BIT_DIGITAL,	DrvJoy13 + 6,	"service"	},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Mjkjidai)

static struct BurnDIPInfo MjkjidaiDIPList[]=
{
	{0x2b, 0xff, 0xff, 0xff, NULL				},
	{0x2c, 0xff, 0xff, 0xff, NULL				},
	{0x2d, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    2, "Test Mode"		},
	{0x2b, 0x01, 0x04, 0x04, "Off"				},
	{0x2b, 0x01, 0x04, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x2b, 0x01, 0x80, 0x80, "Off"				},
	{0x2b, 0x01, 0x80, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Statistics"		},
	{0x2c, 0x01, 0x20, 0x20, "Off"				},
	{0x2c, 0x01, 0x20, 0x00, "On"				},
};

STDDIPINFO(Mjkjidai)

static void __fastcall mjkjidai_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			ppi8255_w((port/0x10) & 1, port & 3, data);
		return;

		case 0x20:
		case 0x30:
			SN76496Write((port / 0x10) & 1, data);
		return;

		case 0x40:
			adpcm_pos = (data & 0x07) * 0x2000;
			adpcm_end = adpcm_pos + 0x2000;
			MSM5205ResetWrite(0, 0);
		return;
	}
}

static UINT8 __fastcall mjkjidai_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
			return ppi8255_r((port/0x10) & 1, port & 3);
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr  = DrvVidRAM[offs + 0x0800];
	INT32 code  = DrvVidRAM[offs + 0x0000] | (attr << 8);
	INT32 color = DrvVidRAM[offs + 0x1000];

	TILE_SET_INFO(0, code, color >> 3, 0);
}

static UINT8 ppi8255_0_portA_r()
{
	UINT8 ret = DrvInputs[12];

	for (INT32 i = 0; i < 12; i++) {
		if ((keyb & (0x800 >> i)) == 0) {
			return (ret & 0xc0) | (DrvInputs[i] & 0x3f);
		}
	}

	return ret;
}

static UINT8 ppi8255_0_portC_r() { return DrvDips[2]; }

static void ppi8255_0_portB_w(UINT8 data)
{
	keyb = (keyb & 0xff00) | (data);
}

static void ppi8255_0_portC_w(UINT8 data)
{
	keyb = (keyb & 0x00ff) | (data << 8);
}

static UINT8 ppi8255_1_portB_r() { return DrvDips[0]; }
static UINT8 ppi8255_1_portC_r() { return DrvDips[1]; }

static void bankswitch(INT32 data)
{
	bankdata = data;
	ZetMapMemory(DrvZ80ROM + 0x8000 + (bankdata & 3) * 0x4000, 0x8000, 0xbfff, MAP_ROM);
}

static void ppi8255_1_portA_w(UINT8 data)
{
	nmi_enable = data & 1;
	if (nmi_enable == 0) ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);

	flipscreen = data & 0x02;
	display_enable = data & 0x04;
	// coin counter = data & 0x20

	bankswitch((data & 0xc0) >> 6);
}

static void mjkjidai_adpcm_interrupt()
{
	if (adpcm_pos >= adpcm_end)
	{
		MSM5205ResetWrite(0, 1);
	}
	else
	{
		MSM5205DataWrite(0, (DrvSndROM[adpcm_pos / 2] >> ((~adpcm_pos & 1) * 4)) & 0xf);
		adpcm_pos++;
	}
}

static INT32 SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 5000000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ppi8255_1_portA_w(0);
	MSM5205Reset();
	ZetClose();

	SN76496Reset();

	keyb = 0;
	adpcm_pos = 0;
	adpcm_end = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x018000;

	DrvGfxROM[0]	= Next; Next += 0x080000;
	DrvGfxROM[1]	= Next; Next += 0x080000;

	DrvColPROM		= Next; Next += 0x000300;

	DrvSndROM		= Next; Next += 0x008000;

	DrvPalette		= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x001000;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x001800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Planes[3] = { 0x10000*8*0, 0x10000*8*1, 0x10000*8*2 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(64,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(128,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x30000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x30000);

	GfxDecode(0x2000, 3,  8,  8, Planes, XOffs, YOffs, 0x040, tmp, DrvGfxROM[0]);
	GfxDecode(0x0800, 3, 16, 16, Planes, XOffs, YOffs, 0x100, tmp, DrvGfxROM[1]);

	BurnFree (tmp);
	
	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;

		if (BurnLoadRom(DrvZ80ROM    + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM    + 0x10000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x08000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x18000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x28000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00100, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00200, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM    + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvNVRAM     + 0x00000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvNVRAM,			0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvVidRAM,			0xe000, 0xf7ff, MAP_RAM);
	ZetSetOutHandler(mjkjidai_write_port);
	ZetSetInHandler(mjkjidai_read_port);
	ZetClose();

	ppi8255_init(2);
	ppi8255_set_read_ports(0, ppi8255_0_portA_r, NULL, ppi8255_0_portC_r);
	ppi8255_set_read_ports(1, NULL, ppi8255_1_portB_r, ppi8255_1_portC_r);
	ppi8255_set_write_ports(0, NULL, ppi8255_0_portB_w, ppi8255_0_portC_w);
	ppi8255_set_write_ports(1, ppi8255_1_portA_w, NULL, NULL);

	MSM5205Init(0, SynchroniseStream, 384000, mjkjidai_adpcm_interrupt, MSM5205_S64_4B, 0);
	MSM5205SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	SN76489Init(0, 10000000/4, 1);
	SN76489Init(1, 10000000/4, 1);
	SN76496SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 3, 8, 8, 0x80000, 0, 0x1f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, -24, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	SN76496Exit();;
	ZetExit();
	MSM5205Exit();
	ppi8255_exit();
	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x20-2; offs >= 0; offs -= 2)
	{
		INT32 attr  = DrvVidRAM[0x800 + offs];
		INT32 code  = DrvVidRAM[0x000 + offs] + ((attr & 0x1f) << 8);
		INT32 color = (DrvVidRAM[0x1000 + offs] & 0x78) >> 3;
		INT32 sx    = (DrvVidRAM[0x800 + offs+1] << 1) | ((attr & 0x20) >> 5);
		INT32 sy    = 240 - DrvVidRAM[offs+1];
		INT32 flipx = code & 1;
		INT32 flipy = code & 2;

		if (flipscreen)
		{
			sx = 496 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		Draw16x16MaskTile(pTransDraw, code >> 2, sx + 16 - 24, sy + 1 - 16, flipx, flipy, color, 3, 0, 0, DrvGfxROM[1]);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

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

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
			DrvInputs[6] ^= (DrvJoy7[i] & 1) << i;
			DrvInputs[7] ^= (DrvJoy8[i] & 1) << i;
			DrvInputs[8] ^= (DrvJoy9[i] & 1) << i;
			DrvInputs[9] ^= (DrvJoy10[i] & 1) << i;
			DrvInputs[10] ^= (DrvJoy11[i] & 1) << i;
			DrvInputs[11] ^= (DrvJoy12[i] & 1) << i;
			DrvInputs[12] ^= (DrvJoy13[i] & 1) << i;
		}
	}
	
	INT32 nInterleave = MSM5205CalcInterleave(0, 5000000);
	INT32 nCyclesTotal[1] = { 5000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Zet);
		if (nmi_enable && i == (nInterleave - 1)) ZetNmi();

		MSM5205Update();
	}

	if (pBurnSoundOut) {
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	
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

		ZetScan(nAction);
		SN76496Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);

		ppi8255_scan();

		SCAN_VAR(bankdata);
		SCAN_VAR(flipscreen);
		SCAN_VAR(display_enable);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(keyb);
		SCAN_VAR(adpcm_end);
		SCAN_VAR(adpcm_pos);

		if (nAction & ACB_WRITE) {
			ZetOpen(0);
			bankswitch(bankdata);
			ZetClose();
		}
	}

	return 0;
}


// Mahjong Kyou Jidai (Japan)

static struct BurnRomInfo mjkjidaiRomDesc[] = {
	{ "mkj-00.14g",		0x8000, 0x188a27e9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "mkj-01.15g",		0x8000, 0xa6a5e9c7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mkj-02.16g",		0x8000, 0xfb312927, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "mkj-20.4e",		0x8000, 0x8fc66bce, 2 | BRF_GRA },           //  3 Graphics
	{ "mkj-21.5e",		0x8000, 0x4dd41a9b, 2 | BRF_GRA },           //  4
	{ "mkj-22.6e",		0x8000, 0x70ac2bd7, 2 | BRF_GRA },           //  5
	{ "mkj-23.7e",		0x8000, 0xf9313dde, 2 | BRF_GRA },           //  6
	{ "mkj-24.8e",		0x8000, 0xaa5130d0, 2 | BRF_GRA },           //  7
	{ "mkj-25.9e",		0x8000, 0xc12c3fe0, 2 | BRF_GRA },           //  8

	{ "mkj-60.13a",		0x0100, 0x5dfaba60, 3 | BRF_GRA },           //  9 Color Data
	{ "mkj-61.14a",		0x0100, 0xe9e90d55, 3 | BRF_GRA },           // 10
	{ "mkj-62.15a",		0x0100, 0x934f1d53, 3 | BRF_GRA },           // 11

	{ "mkj-40.14c",		0x8000, 0x4d8fcc4a, 4 | BRF_SND },           // 12 ADPCM Data

	{ "default.nv",		0x1000, 0xeccc0263, 5 | BRF_PRG },           // 13 Default NV RAM
};

STD_ROM_PICK(mjkjidai)
STD_ROM_FN(mjkjidai)

struct BurnDriver BurnDrvMjkjidai = {
	"mjkjidai", NULL, NULL, NULL, "1986",
	"Mahjong Kyou Jidai (Japan)\0", NULL, "Sanritsu", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAHJONG, 0,
	NULL, mjkjidaiRomInfo, mjkjidaiRomName, NULL, NULL, NULL, NULL, MjkjidaiInputInfo, MjkjidaiDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	464, 224, 4, 3
};
