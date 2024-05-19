// FinalBurn Neo Sand Scorpion driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "burn_ym2203.h"
#include "kaneko_tmap.h"
#include "kaneko_hit.h"
#include "watchdog.h"
#include "pandora.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvTransTab;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvPandoraRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVideoRAM;
static UINT8 *DrvVidRegs;

static UINT8 nDrvZ80Bank;
static UINT8 soundlatch;
static UINT8 soundlatch2;
static INT32 vblank_irq;
static INT32 sprite_irq;
static INT32 unknown_irq;
static INT32 latch1_full;
static INT32 latch2_full;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[3];
static UINT8 DrvReset;

static INT32 nExtraCycles;

static struct BurnInputInfo SandscrpInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 6,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 4,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Sandscrp)

static struct BurnDIPInfo SandscrpDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL					},
	{0x14, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x13, 0x01, 0x03, 0x02, "1"					},
	{0x13, 0x01, 0x03, 0x01, "2"					},
	{0x13, 0x01, 0x03, 0x03, "3"					},
	{0x13, 0x01, 0x03, 0x00, "5"					},

	{0   , 0xfe, 0   ,    4, "Bombs"				},
	{0x13, 0x01, 0x0c, 0x08, "1"					},
	{0x13, 0x01, 0x0c, 0x04, "2"					},
	{0x13, 0x01, 0x0c, 0x0c, "3"					},
	{0x13, 0x01, 0x0c, 0x00, "5"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x30, 0x30, "Easy"					},
	{0x13, 0x01, 0x30, 0x20, "Normal"				},
	{0x13, 0x01, 0x30, 0x10, "Hard"					},
	{0x13, 0x01, 0x30, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x13, 0x01, 0xc0, 0x80, "100K, 300K"			},
	{0x13, 0x01, 0xc0, 0xc0, "200K, 500K"			},
	{0x13, 0x01, 0xc0, 0x40, "500K, 1000K"			},
	{0x13, 0x01, 0xc0, 0x00, "1000K, 3000K"			},

	{0   , 0xfe, 0   ,    16, "Coinage"				},
	{0x14, 0x01, 0x0f, 0x0a, "6 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x0b, "5 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x0c, "4 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x0d, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x01, "8 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x0e, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x0f, 0x02, "5 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x03, "3 Coins 2 Credits"	},
	{0x14, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x0f, 0x04, "2 Coins 3 Credits"	},
	{0x14, 0x01, 0x0f, 0x09, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x0f, 0x08, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x0f, 0x07, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x0f, 0x06, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x14, 0x01, 0x0f, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x10, 0x10, "Off"					},
	{0x14, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x20, 0x00, "Off"					},
	{0x14, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x14, 0x01, 0x40, 0x00, "Off"					},
	{0x14, 0x01, 0x40, 0x40, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x80, 0x80, "Off"					},
	{0x14, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Sandscrp)

static void update_irq_state()
{
	INT32 irq = (vblank_irq || sprite_irq || unknown_irq) ? 1 : 0;

	SekSetIRQLine(1, irq ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void sound_sync()
{
	BurnTimerUpdate(SekTotalCycles() / 5);
}

static void __fastcall sandscrp_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x100000:
			if (data & 0x08) sprite_irq = 0;
			if (data & 0x10) unknown_irq = 0;
			if (data & 0x20) vblank_irq = 0;
			update_irq_state();
		return;

		case 0xa00000:
		return; // coin counter

		case 0xe00000:
			sound_sync();
			latch1_full = 1;
			soundlatch = data & 0xff;
			ZetNmi();
			BurnTimerUpdate((SekTotalCycles() / 5) + 133);
		return;

		case 0xe40000:
			sound_sync();
			latch1_full = data & 0x80;
			latch2_full = data & 0x40;
		return;
	}
}

static void __fastcall sandscrp_main_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("MWB %5.5x %2.2x\n"), address, data);
}

static UINT16 __fastcall sandscrp_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x800000:
			return ((sprite_irq << 3) | (unknown_irq << 4) | (vblank_irq << 5));

		case 0xb00000:
			return DrvInputs[0];

		case 0xb00002:
			return DrvInputs[1];

		case 0xb00004:
			return DrvInputs[2];

		case 0xb00006:
			return 0xffff;

		case 0xe00000:
			sound_sync();
			latch2_full = 0;
			return soundlatch2;

		case 0xe40000:
			sound_sync();
			return (latch1_full ? 0x80 : 0) | (latch2_full ? 0x40 : 0);

		case 0xec0000:
			BurnWatchdogRead();
			return 0;
	}

	return 0;
}

static UINT8 __fastcall sandscrp_main_read_byte(UINT32 address)
{
	bprintf (0, _T("MRB %5.5x\n"), address);

	return 0;
}

static void __fastcall sandscrp_pandora_write_word(UINT32 address, UINT16 data)
{
	address &= 0x1ffe;

	data &= 0xff;

	DrvSprRAM[address + 0] = data;
	DrvSprRAM[address + 1] = data;

	DrvPandoraRAM[address/2] = data;
}

static void __fastcall sandscrp_pandora_write_byte(UINT32 address, UINT8 data)
{
	address &= 0x1ffe;

	DrvSprRAM[address + 0] = data;
	DrvSprRAM[address + 1] = data;

	DrvPandoraRAM[address/2] = data;
}

static void __fastcall sandscrp_palette_write_word(UINT32 address, UINT16 data)
{
	*((UINT16*)(BurnPalRAM + (address & 0xffe))) = BURN_ENDIAN_SWAP_INT16(data);
	BurnPaletteWrite_xGGGGGRRRRRBBBBB(address & 0xffe);
}

static void __fastcall sandscrp_palette_write_byte(UINT32 address, UINT8 data)
{
	BurnPalRAM[(address & 0xfff) ^ 1] = data;
	BurnPaletteWrite_xGGGGGRRRRRBBBBB(address & 0xffe);
}

static void bankswitch(INT32 bank)
{
	nDrvZ80Bank = bank & 7;

	ZetMapMemory(DrvZ80ROM + ((bank & 0x07) * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall sandscrp_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			bankswitch(data);
		return;

		case 0x02:
		case 0x03:
			BurnYM2203Write(0, port & 1, data);
		return;

		case 0x04:
			MSM6295Write(0, data);
		return;

		case 0x06:
			latch2_full = 1;
			soundlatch2 = data;
		return;
	}
}

static UINT8 __fastcall sandscrp_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x02:
		case 0x03:
			return BurnYM2203Read(0, port & 1);

		case 0x07:
			latch1_full = 0;
			return soundlatch;

		case 0x08:
			return (latch2_full ? 0x80 : 0) | (latch1_full ? 0x40 : 0);
	}

	return 0;
}

static UINT8 DrvYM2203PortA(UINT32)
{
	return DrvDips[0];
}

static UINT8 DrvYM2203PortB(UINT32)
{
	return DrvDips[1];
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	MSM6295Reset(0);

	kaneko_hit_calc_reset();
	BurnWatchdogResetEnable();

	nDrvZ80Bank = 0;
	vblank_irq = 0;
	sprite_irq = 0;
	unknown_irq = 0;
	soundlatch = 0;
	soundlatch2 = 0;
	latch1_full = 0;
	latch2_full = 0;

	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;
	DrvZ80ROM		= Next; Next += 0x020000;

	DrvGfxROM0		= Next; Next += 0x200000;
	DrvGfxROM1		= Next; Next += 0x200000;

	DrvTransTab		= Next; Next += 0x400000 / 0x100;

	MSM6295ROM		= Next; Next += 0x040000;

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x002000;

	Drv68KRAM		= Next; Next += 0x010000;
	DrvPandoraRAM	= Next; Next += 0x002000;
	DrvSprRAM		= Next; Next += 0x002000;
	BurnPalRAM		= Next; Next += 0x001000;

	DrvVideoRAM		= Next; Next += 0x004000;
	DrvVidRegs		= Next; Next += 0x000400;

	RamEnd			= Next;

	BurnPalette		= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	MemEnd			= Next;

	return 0;
}

static void DrvFillTransTable()
{
	memset (DrvTransTab, 0, 0x4000);

	for (INT32 i = 0; i < 0x400000; i+= 0x100) {
		DrvTransTab[i/0x100] = 1; // transparent

		for (INT32 j = 0; j < 0x100; j++) {
			if (DrvGfxROM0[j + i]) {
				DrvTransTab[i/0x100] = 0;
				break;
			}
		}
	}
}

static INT32 DrvGfxDecode()
{
	static INT32 Plane[4]   = { STEP4(0,1) };
	static INT32 XOffs0[16] = { STEP4(12, -4), STEP4(28, -4), STEP4(268, -4),STEP4(284, -4) };
	static INT32 XOffs1[16] = { STEP8(0,4), STEP8(256,4) };
	static INT32 YOffs[16]  = { STEP8(0, 32), STEP8(512, 32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x200000);

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs0, YOffs, 0x400, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x200000);

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs1, YOffs, 0x400, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit(INT32 type)
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (type == 0) // early revisions
		{
			if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM0 + 0x000001,  4, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM1 + 0x000000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1 + 0x080000,  6, 1)) return 1;

			if (BurnLoadRom(MSM6295ROM + 0x000000,  7, 1)) return 1;
		}
		else
		{
			if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;
			BurnByteswap(DrvGfxROM0, 0x200000);

			if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;

			if (BurnLoadRom(MSM6295ROM + 0x000000,  5, 1)) return 1;
		}

		DrvGfxDecode();
		DrvFillTransTable();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvVidRegs,	0x300000, 0x30000f | 0x3ff, MAP_RAM);
	SekMapMemory(DrvVideoRAM,	0x400000, 0x403fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x500000, 0x501fff, MAP_ROM);
	SekMapMemory(BurnPalRAM,	0x600000, 0x600fff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x700000, 0x70ffff, MAP_RAM);
	SekSetWriteWordHandler(0,	sandscrp_main_write_word);
	SekSetWriteByteHandler(0,	sandscrp_main_write_byte);
	SekSetReadWordHandler(0,	sandscrp_main_read_word);
	SekSetReadByteHandler(0,	sandscrp_main_read_byte);

	SekMapHandler(1,			0x500000, 0x501fff, MAP_WRITE);
	SekSetWriteWordHandler(1,	sandscrp_pandora_write_word);
	SekSetWriteByteHandler(1,	sandscrp_pandora_write_byte);

	SekMapHandler(2,			0x600000, 0x600fff, MAP_WRITE);
	SekSetWriteWordHandler(2,	sandscrp_palette_write_word);
	SekSetWriteByteHandler(2,	sandscrp_palette_write_byte);

	kaneko_hit_calc_init(3, 	0x200000);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,	0xc000, 0xdfff, MAP_RAM);
	ZetSetOutHandler(sandscrp_sound_write_port);
	ZetSetInHandler(sandscrp_sound_read_port);
	ZetClose();

	BurnWatchdogInit(DrvDoReset, 180);

	BurnYM2203Init(1, 4000000, &DrvFMIRQHandler, 0);
	BurnYM2203SetPorts(0, &DrvYM2203PortA, &DrvYM2203PortB, NULL, NULL);
	BurnTimerAttachZet(4000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.25, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 2000000 / 132, 1);
	MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	kaneko_view2_init(0, DrvVideoRAM, DrvVidRegs, DrvGfxROM0, 0x400, DrvTransTab, 91, 5);
	pandora_init(DrvPandoraRAM, DrvGfxROM1, (0x200000/0x100)-1, 0x000, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	BurnYM2203Exit();

	MSM6295Exit(0);
	MSM6295ROM = NULL;

	SekExit();
	ZetExit();

	pandora_exit();
	kaneko_view2_exit();

	GenericTilesExit();

	BurnFreeMemIndex();

	return 0;
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		BurnPaletteUpdate_xGGGGGRRRRRBBBBB();
		BurnRecalc = 0;
	}

	BurnTransferClear();

	for (INT32 i = 0; i < 4; i++) {
		kaneko_view2_draw_layer(0, 0, i);
		kaneko_view2_draw_layer(0, 1, i);
	}

	pandora_update(pTransDraw);

	for (INT32 i = 4; i < 8; i++) {
		kaneko_view2_draw_layer(0, 0, i);
		kaneko_view2_draw_layer(0, 1, i);
	}

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	SekNewFrame();
	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3 * sizeof(UINT16));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] =  { 20000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {

		CPU_RUN(0, Sek);

		if (i == 240) {
			vblank_irq = 1;
			update_irq_state();
		}

		if (i == 255) {
			sprite_irq = 1;
			update_irq_state();
		}

		CPU_RUN_TIMER(1);
	}

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	ZetClose();
	SekClose();

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	pandora_buffer_sprites();

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

		SekScan(nAction);
		ZetScan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		kaneko_hit_calc_scan(nAction);

		SCAN_VAR(vblank_irq);
		SCAN_VAR(sprite_irq);
		SCAN_VAR(unknown_irq);
		SCAN_VAR(soundlatch);
		SCAN_VAR(soundlatch2);
		SCAN_VAR(latch1_full);
		SCAN_VAR(latch2_full);
		SCAN_VAR(nDrvZ80Bank);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(nDrvZ80Bank);
		ZetClose();
	}

	return 0;
}


// Sand Scorpion

static struct BurnRomInfo sandscrpRomDesc[] = {
	{ "11.bin",		0x040000, 0x9b24ab40, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "12.bin",		0x040000, 0xad12caee, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "8.ic51",		0x020000, 0x6f3e9db1, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "4.ic32",		0x080000, 0xb9222ff2, 3 | BRF_GRA },           //  3 Tiles
	{ "3.ic33",		0x080000, 0xadf20fa0, 3 | BRF_GRA },           //  4

	{ "5.ic16",		0x080000, 0x9bb675f6, 4 | BRF_GRA },           //  5 Sprites
	{ "6.ic17",		0x080000, 0x7df2f219, 4 | BRF_GRA },           //  6

	{ "7.ic55",		0x040000, 0x9870ab12, 5 | BRF_SND },           //  7 Samples
};

STD_ROM_PICK(sandscrp)
STD_ROM_FN(sandscrp)

static INT32 sandscrpInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvSandscrp = {
	"sandscrp", NULL, NULL, NULL, "1992",
	"Sand Scorpion\0", NULL, "Face", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, sandscrpRomInfo, sandscrpRomName, NULL, NULL, NULL, NULL, SandscrpInputInfo, SandscrpDIPInfo,
	sandscrpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Sand Scorpion (Earlier)

static struct BurnRomInfo sandscrpaRomDesc[] = {
	{ "1.ic4",		0x040000, 0xc0943ae2, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "2.ic5",		0x040000, 0x6a8e0012, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "8.ic51",		0x020000, 0x6f3e9db1, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "4.ic32",		0x080000, 0xb9222ff2, 3 | BRF_GRA },           //  3 Tiles
	{ "3.ic33",		0x080000, 0xadf20fa0, 3 | BRF_GRA },           //  4

	{ "5.ic16",		0x080000, 0x9bb675f6, 4 | BRF_GRA },           //  5 Sprites
	{ "6.ic17",		0x080000, 0x7df2f219, 4 | BRF_GRA },           //  6

	{ "7.ic55",		0x040000, 0x9870ab12, 5 | BRF_SND },           //  7 Samples
};

STD_ROM_PICK(sandscrpa)
STD_ROM_FN(sandscrpa)

struct BurnDriver BurnDrvSandscrpa = {
	"sandscrpa", "sandscrp", NULL, NULL, "1992",
	"Sand Scorpion (Earlier)\0", NULL, "Face", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, sandscrpaRomInfo, sandscrpaRomName, NULL, NULL, NULL, NULL, SandscrpInputInfo, SandscrpDIPInfo,
	sandscrpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};


// Kuai Da Shizi Huangdi (China?, Revised Hardware)

static struct BurnRomInfo sandscrpbRomDesc[] = {
	{ "11.ic4",		0x040000, 0x80020cab, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "12.ic5",		0x040000, 0x8df1d42f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "8.ic51",		0x020000, 0x6f3e9db1, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "ss501.ic30",	0x100000, 0x0cf9f99d, 3 | BRF_GRA },       	   //  3 Tiles

	{ "ss502.ic16",	0x100000, 0xd8012ebb, 4 | BRF_GRA },           //  4 Sprites

	{ "7.ic55",		0x040000, 0x9870ab12, 5 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(sandscrpb)
STD_ROM_FN(sandscrpb)

static INT32 sandscrpbInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvSandscrpb = {
	"sandscrpb", "sandscrp", NULL, NULL, "1992",
	"Kuai Da Shizi Huangdi (China?, Revised Hardware)\0", NULL, "Face", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, sandscrpbRomInfo, sandscrpbRomName, NULL, NULL, NULL, NULL, SandscrpInputInfo, SandscrpDIPInfo,
	sandscrpbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x800,
	224, 256, 3, 4
};
