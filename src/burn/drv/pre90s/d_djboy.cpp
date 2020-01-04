// FB Alpha DJ Boy driver module
// Based on MAME driver by Phil Stroffolino

#include "tiles_generic.h"
#include "z80_intf.h"
#include "mermaid.h"
#include "msm6295.h"
#include "burn_ym2203.h"
#include "pandora.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvShareRAM0;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPandoraRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;

static UINT8 *soundlatch;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bankxor;

static UINT8 nBankAddress0;
static UINT8 nBankAddress1;
static UINT8 nBankAddress2;

static INT32 videoreg;
static UINT8 scrollx;
static UINT8 scrolly;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[6];
static UINT8 DrvReset;

static struct BurnInputInfo DjboyInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvInputs + 4,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvInputs + 5,	"dip"		},
};

STDINPUTINFO(Djboy)

static struct BurnDIPInfo DjboyDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL							},
	{0x15, 0xff, 0xff, 0xff, NULL							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x14, 0x01, 0x02, 0x02, "Off"							},
	{0x14, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x14, 0x01, 0x04, 0x04, "Off"							},
	{0x14, 0x01, 0x04, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Coin A"						},
	{0x14, 0x01, 0x30, 0x10, "2 Coins 1 Credits"			},
	{0x14, 0x01, 0x30, 0x30, "1 Coin  1 Credits"			},
	{0x14, 0x01, 0x30, 0x00, "2 Coins 3 Credits"			},
	{0x14, 0x01, 0x30, 0x20, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin B"						},
	{0x14, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"			},
	{0x14, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"			},
	{0x14, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"			},
	{0x14, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x15, 0x01, 0x03, 0x02, "Easy"							},
	{0x15, 0x01, 0x03, 0x03, "Normal"						},
	{0x15, 0x01, 0x03, 0x01, "Hard"							},
	{0x15, 0x01, 0x03, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Bonus Levels (in thousands)"	},
	{0x15, 0x01, 0x0c, 0x0c, "10,30,50,70,90"				},
	{0x15, 0x01, 0x0c, 0x08, "10,20,30,40,50,60,70,80,90"	},
	{0x15, 0x01, 0x0c, 0x04, "20,50"						},
	{0x15, 0x01, 0x0c, 0x00, "None"							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x15, 0x01, 0x30, 0x20, "3"							},
	{0x15, 0x01, 0x30, 0x30, "5"							},
	{0x15, 0x01, 0x30, 0x10, "7"							},
	{0x15, 0x01, 0x30, 0x00, "9"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x15, 0x01, 0x40, 0x00, "Off"							},
	{0x15, 0x01, 0x40, 0x40, "On"							},

	{0   , 0xfe, 0   ,    2, "Stereo Sound"					},
	{0x15, 0x01, 0x80, 0x00, "Off"							},
	{0x15, 0x01, 0x80, 0x80, "On"							},
};

STDDIPINFO(Djboy)

static void cpu0_bankswitch(INT32 data)
{
	nBankAddress0 = data;

	ZetMapMemory(DrvZ80ROM0 + ((nBankAddress0 ^ bankxor) * 0x2000), 0xc000, 0xdfff, MAP_ROM);
}

static void __fastcall djboy_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0xb000) {
		DrvSprRAM[address & 0xfff] = data;
		address = (address & 0x800) | ((address & 0xff) << 3) | ((address & 0x700) >> 8);
		DrvPandoraRAM[address & 0xfff] = data;
		return;
	}
}

static void __fastcall djboy_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			cpu0_bankswitch(data);
		return;
	}
}

static inline void palette_update(INT32 offs)
{
	INT32 p = (DrvPalRAM[offs+1] + (DrvPalRAM[offs+0] * 256));

	INT32 r = (p >> 8) & 0x0f;
	INT32 g = (p >> 4) & 0x0f;
	INT32 b = (p >> 0) & 0x0f;

	DrvPalette[offs/2] = BurnHighCol((r*16)+r,(g*16)+g,(b*16)+b,0);
}

static void __fastcall djboy_cpu1_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0xd000) {
		DrvPalRAM[address & 0x3ff] = data;
		if (address & 1) palette_update(address & 0x3fe);
		return;
	}
}

static void cpu1_bankswitch(INT32 data)
{
	INT32 bankdata[16] = { 0,1,2,3,-1,-1,-1,-1,4,5,6,7,8,9,10,11 };

	if (bankdata[data & 0x0f] == -1) return;

	nBankAddress1 = data;

	ZetMapMemory(DrvZ80ROM1 + (bankdata[data & 0x0f] * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void sync_main_to_sub()
{
	INT32 subcyc = ZetTotalCycles();
	ZetClose();
	ZetOpen(0);
	INT32 maincyc = ZetTotalCycles();
	INT32 nowcyc = subcyc - maincyc;

	if (nowcyc>0) {
		ZetRun(nowcyc);
	}
	ZetClose();
	ZetOpen(1);
}

static void sync_sound_to_sub()
{
	INT32 subcyc = ZetTotalCycles();
	ZetClose();
	ZetOpen(2);
	INT32 soundcyc = ZetTotalCycles();
	INT32 nowcyc = subcyc - soundcyc;
	if (nowcyc > 0) {
		BurnTimerUpdate(subcyc);
	}
	ZetClose();
	ZetOpen(1);
}

static void __fastcall djboy_cpu1_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			videoreg = data;
			cpu1_bankswitch(data);
		return;

		case 0x02:
			sync_sound_to_sub();
			*soundlatch = data;
			ZetNmi(2);
		return;

		case 0x04:
			mermaidWrite(data);
		return;

		case 0x06:
			scrolly = data;
		return;

		case 0x08:
			scrollx = data;
		return;

		case 0x0a:
			sync_main_to_sub();
			ZetNmi(0);
		return;

		case 0x0e:
			// coin counter
		return;
	}
}

static UINT8 __fastcall djboy_cpu1_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x04:
			return mermaidRead();

		case 0x0c:
			return mermaidStatus();
	}

	return 0;
}

static void cpu2_bankswitch(INT32 data)
{
	nBankAddress2 = data;

	ZetMapMemory(DrvZ80ROM2 + (data * 0x04000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall djboy_cpu2_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			cpu2_bankswitch(data);
		return;

		case 0x02:
		case 0x03:
			BurnYM2203Write(0, port & 1, data);
		return;

		case 0x06:
			MSM6295Write(0, data);
		return;

		case 0x07:
			MSM6295Write(1, data);
		return;
	}
}

static UINT8 __fastcall djboy_cpu2_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x02:
		case 0x03:
			return BurnYM2203Read(0, port & 1);

		case 0x04:
			return *soundlatch;

		case 0x06:
			return MSM6295Read(0);

		case 0x07:
			return MSM6295Read(1);
	}

	return 0;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[offs + 0x800];
	INT32 code = DrvVidRAM[offs + 0x000] + ((attr & 0x0f) * 256) + ((attr & 0x80) * 0x20);

	TILE_SET_INFO(0, code, attr >> 4, 0);
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	cpu0_bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	cpu1_bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(2);
	cpu2_bankswitch(0);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	mermaidReset();

	MSM6295Reset();

	videoreg = 0;
	scrollx = 0;
	scrolly = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x040000;
	DrvZ80ROM1		= Next; Next += 0x030000;
	DrvZ80ROM2		= Next; Next += 0x020000;
	DrvMCUROM		= Next; Next += 0x001000;

	DrvGfxROM0		= Next; Next += 0x400000;
	DrvGfxROM1		= Next; Next += 0x200000;

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x040000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x001000;

	DrvShareRAM0	= Next; Next += 0x002000;

	DrvPandoraRAM	= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x000400;
	DrvZ80RAM1		= Next; Next += 0x000500;
	DrvZ80RAM2		= Next; Next += 0x002000;

	soundlatch		= Next; Next += 0x000001;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	static INT32 Plane[4]  = { STEP4(0,1) };
	static INT32 XOffs[16] = { STEP8(0,4), STEP8(256, 4) };
	static INT32 YOffs[16] = { STEP8(0,32), STEP8(512, 32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(57.50);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x000000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x020000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x000000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x010000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM  + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x180000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1f0000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000, 12, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 13, 1)) return 1;
		    BurnLoadRom(DrvSndROM  + 0x020000, 14, 1); // djboyja

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0xafff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,			0xb000, 0xbfff, MAP_ROM); // handler...
	ZetMapMemory(DrvShareRAM0,		0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(djboy_main_write);
	ZetSetOutHandler(djboy_main_write_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,			0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,			0xd000, 0xd3ff, MAP_ROM); // handler
	ZetMapMemory(DrvZ80RAM1,        0xd400, 0xd8ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM0,		0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(djboy_cpu1_write);
	ZetSetOutHandler(djboy_cpu1_write_port);
	ZetSetInHandler(djboy_cpu1_read_port);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,		0xc000, 0xdfff, MAP_RAM);
	ZetSetOutHandler(djboy_cpu2_write_port);
	ZetSetInHandler(djboy_cpu2_read_port);
	ZetClose();

	mermaidInit(DrvMCUROM, DrvInputs);

	BurnYM2203Init(1, 3000000, NULL, 0);
	BurnTimerAttachZet(6000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.50, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1500000 / 165, 1);
	MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_LEFT);
	MSM6295SetBank(0, DrvSndROM, 0, 0x3ffff);

	MSM6295Init(1, 1500000 / 165, 1);
	MSM6295SetRoute(1, 0.80, BURN_SND_ROUTE_RIGHT);
	MSM6295SetBank(1, DrvSndROM, 0, 0x3ffff);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 16, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM1, 4, 16, 16, 0x200000, 0, 0x0f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	pandora_init(DrvPandoraRAM, DrvGfxROM0, (0x400000/0x100)-1, 0x100, -1, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	pandora_exit();

	GenericTilesExit();

	ZetExit();
	mermaidExit();

	MSM6295Exit();
	BurnYM2203Exit();

	BurnFree(AllMem);

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 offs = 0; offs < 0x400; offs+=2) {
			palette_update(offs);
		}
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	GenericTilemapSetScrollX(0, scrollx + ((videoreg & 0xc0) << 2) - 0x391);
	GenericTilemapSetScrollY(0, scrolly + ((videoreg & 0x20) << 3));

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) pandora_update(pTransDraw);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 4);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[2] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[0] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[4] =  { (6000000 * 10) / 575, (6000000 * 10) / 575, (6000000 * 10) / 575, (6000000 * 10) / 575 / 12 }; // 57.5 fps
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {

		ZetOpen(0);
		nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		if (i == 64 || i == 240) {
			if (i ==  64) ZetSetVector(0xff);
			if (i == 240) ZetSetVector(0xfd);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[2] / nInterleave));
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		nCyclesDone[3] += mermaidRun(((i + 1) * nCyclesTotal[3] / nInterleave) - nCyclesDone[3]);

		if (i == 240) {
			pandora_buffer_sprites();

			if (pBurnDraw) {
				DrvDraw();
			}
		}

	}

	ZetOpen(2);

	BurnTimerEndFrame(nCyclesTotal[2]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	
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

		ZetScan(nAction);
		mermaidScan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(nBankAddress0);
		SCAN_VAR(nBankAddress1);
		SCAN_VAR(nBankAddress2);
		SCAN_VAR(videoreg);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		cpu0_bankswitch(nBankAddress0);
		ZetClose();

		ZetOpen(1);
		cpu1_bankswitch(nBankAddress1);
		ZetClose();

		ZetOpen(2);
		cpu2_bankswitch(nBankAddress2);
		ZetClose();
	}

	return 0;
}


// DJ Boy (World)

static struct BurnRomInfo djboyRomDesc[] = {
	{ "djboy.4b",	0x20000, 0x354531ec, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "bs100.4d",	0x20000, 0x081e8af8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "djboy.5y",	0x10000, 0x91eb189a, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "bs101.6w",	0x20000, 0xa7c85577, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "bs200.8c",	0x20000, 0xf6c19e51, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "beast.9s",	0x01000, 0xebe0f5f3, 4 | BRF_PRG | BRF_ESS }, //  5 Kaneko Beast MCU

	{ "bs000.1h",	0x80000, 0xbe4bf805, 5 | BRF_GRA },           //  6 Sprites
	{ "bs001.1f",	0x80000, 0xfdf36e6b, 5 | BRF_GRA },           //  7
	{ "bs002.1d",	0x80000, 0xc52fee7f, 5 | BRF_GRA },           //  8
	{ "bs003.1k",	0x80000, 0xed89acb4, 5 | BRF_GRA },           //  9
	{ "bs06.1b",	0x10000, 0x22c8aa08, 5 | BRF_GRA },           // 10

	{ "bs004.1s",	0x80000, 0x2f1392c3, 6 | BRF_GRA },           // 11 Tiles
	{ "bs005.1u",	0x80000, 0x46b400c4, 6 | BRF_GRA },           // 12

	{ "bs203.5j",	0x40000, 0x805341fb, 7 | BRF_SND },           // 13 OKI Samples
};

STD_ROM_PICK(djboy)
STD_ROM_FN(djboy)

static INT32 DjboyInit()
{
	bankxor = 0;

	return DrvInit();
}

struct BurnDriver BurnDrvDjboy = {
	"djboy", NULL, NULL, NULL, "1989",
	"DJ Boy (World)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_KANEKO_MISC, GBF_SCRFIGHT, 0,
	NULL, djboyRomInfo, djboyRomName, NULL, NULL, NULL, NULL, DjboyInputInfo, DjboyDIPInfo,
	DjboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// DJ Boy (US, set 1)

static struct BurnRomInfo djboyuRomDesc[] = {
	{ "bs64.4b",	0x20000, 0xb77aacc7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "bs100.4d",	0x20000, 0x081e8af8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bs65.5y",	0x10000, 0x0f1456eb, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "bs101.6w",	0x20000, 0xa7c85577, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "bs200.8c",	0x20000, 0xf6c19e51, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "beast.9s",	0x01000, 0xebe0f5f3, 4 | BRF_PRG | BRF_ESS }, //  5 Kaneko Beast MCU

	{ "bs000.1h",	0x80000, 0xbe4bf805, 5 | BRF_GRA },           //  6 Sprites
	{ "bs001.1f",	0x80000, 0xfdf36e6b, 5 | BRF_GRA },           //  7
	{ "bs002.1d",	0x80000, 0xc52fee7f, 5 | BRF_GRA },           //  8
	{ "bs003.1k",	0x80000, 0xed89acb4, 5 | BRF_GRA },           //  9
	{ "bs07s.1b",	0x10000, 0xd9b7a220, 5 | BRF_GRA },           // 10

	{ "bs004.1s",	0x80000, 0x2f1392c3, 6 | BRF_GRA },           // 11 Tiles
	{ "bs005.1u",	0x80000, 0x46b400c4, 6 | BRF_GRA },           // 12

	{ "bs203.5j",	0x40000, 0x805341fb, 7 | BRF_SND },           // 13 OKI Samples
};

STD_ROM_PICK(djboyu)
STD_ROM_FN(djboyu)

struct BurnDriver BurnDrvDjboyu = {
	"djboyu", "djboy", NULL, NULL, "1990",
	"DJ Boy (US, set 1)\0", NULL, "Kaneko (American Sammy license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_KANEKO_MISC, GBF_SCRFIGHT, 0,
	NULL, djboyuRomInfo, djboyuRomName, NULL, NULL, NULL, NULL, DjboyInputInfo, DjboyDIPInfo,
	DjboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// DJ Boy (US, set 2)

static struct BurnRomInfo djboyuaRomDesc[] = {
	{ "bs19s.4b",	0x20000, 0x17ce9f6c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "bs100.4d",	0x20000, 0x081e8af8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bs15s.5y",	0x10000, 0xe6f966b2, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "bs101.6w",	0x20000, 0xa7c85577, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "bs200.8c",	0x20000, 0xf6c19e51, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "beast.9s",	0x01000, 0xebe0f5f3, 4 | BRF_PRG | BRF_ESS }, //  5 Kaneko Beast MCU

	{ "bs000.1h",	0x80000, 0xbe4bf805, 5 | BRF_GRA },           //  6 Sprites
	{ "bs001.1f",	0x80000, 0xfdf36e6b, 5 | BRF_GRA },           //  7
	{ "bs002.1d",	0x80000, 0xc52fee7f, 5 | BRF_GRA },           //  8
	{ "bs003.1k",	0x80000, 0xed89acb4, 5 | BRF_GRA },           //  9
	{ "bs07s.1b",	0x10000, 0xd9b7a220, 5 | BRF_GRA },           // 10

	{ "bs004.1s",	0x80000, 0x2f1392c3, 6 | BRF_GRA },           // 11 Tiles
	{ "bs005.1u",	0x80000, 0x46b400c4, 6 | BRF_GRA },           // 12

	{ "bs203.5j",	0x40000, 0x805341fb, 7 | BRF_SND },           // 13 OKI Samples
};

STD_ROM_PICK(djboyua)
STD_ROM_FN(djboyua)

struct BurnDriver BurnDrvDjboyua = {
	"djboyua", "djboy", NULL, NULL, "1990",
	"DJ Boy (US, set 2)\0", NULL, "Kaneko (American Sammy license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_KANEKO_MISC, GBF_SCRFIGHT, 0,
	NULL, djboyuaRomInfo, djboyuaRomName, NULL, NULL, NULL, NULL, DjboyInputInfo, DjboyDIPInfo,
	DjboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// DJ Boy (Japan, set 1)

static struct BurnRomInfo djboyjRomDesc[] = {
	{ "bs12.4b",	0x20000, 0x0971523e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "bs100.4d",	0x20000, 0x081e8af8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bs13.5y",	0x10000, 0x5c3f2f96, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "bs101.6w",	0x20000, 0xa7c85577, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "bs200.8c",	0x20000, 0xf6c19e51, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "beast.9s",	0x01000, 0xebe0f5f3, 4 | BRF_PRG | BRF_ESS }, //  5 Kaneko Beast MCU

	{ "bs000.1h",	0x80000, 0xbe4bf805, 5 | BRF_GRA },           //  6 Sprites
	{ "bs001.1f",	0x80000, 0xfdf36e6b, 5 | BRF_GRA },           //  7
	{ "bs002.1d",	0x80000, 0xc52fee7f, 5 | BRF_GRA },           //  8
	{ "bs003.1k",	0x80000, 0xed89acb4, 5 | BRF_GRA },           //  9
	{ "bs06.1b",	0x10000, 0x22c8aa08, 5 | BRF_GRA },           // 10

	{ "bs004.1s",	0x80000, 0x2f1392c3, 6 | BRF_GRA },           // 11 Tiles
	{ "bs005.1u",	0x80000, 0x46b400c4, 6 | BRF_GRA },           // 12

	{ "bs-204.5j",	0x40000, 0x510244f0, 7 | BRF_SND },           // 13 OKI Samples
};

STD_ROM_PICK(djboyj)
STD_ROM_FN(djboyj)

static INT32 DjboyjInit()
{
	bankxor = 0x1f;

	return DrvInit();
}

struct BurnDriver BurnDrvDjboyj = {
	"djboyj", "djboy", NULL, NULL, "1989",
	"DJ Boy (Japan, set 1)\0", NULL, "Kaneko (Sega license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_KANEKO_MISC, GBF_SCRFIGHT, 0,
	NULL, djboyjRomInfo, djboyjRomName, NULL, NULL, NULL, NULL, DjboyInputInfo, DjboyDIPInfo,
	DjboyjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// DJ Boy (Japan, set 2)

static struct BurnRomInfo djboyjaRomDesc[] = {
	{ "djboyja.4b",	0x20000, 0xf7ac20ca, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "bs100.4d",	0x20000, 0x081e8af8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bs13.5y",	0x10000, 0x5c3f2f96, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "bs101.6w",	0x20000, 0xa7c85577, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "bs200.8c",	0x20000, 0xf6c19e51, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "beast.9s",	0x01000, 0xebe0f5f3, 4 | BRF_PRG | BRF_ESS }, //  5 Kaneko Beast MCU

	{ "bs000.1h",	0x80000, 0xbe4bf805, 5 | BRF_GRA },           //  6 Sprites
	{ "bs001.1f",	0x80000, 0xfdf36e6b, 5 | BRF_GRA },           //  7
	{ "bs002.1d",	0x80000, 0xc52fee7f, 5 | BRF_GRA },           //  8
	{ "bs003.1k",	0x80000, 0xed89acb4, 5 | BRF_GRA },           //  9
	{ "bs06.1b",	0x10000, 0x22c8aa08, 5 | BRF_GRA },           // 10

	{ "bs004.1s",	0x80000, 0x2f1392c3, 6 | BRF_GRA },           // 11 Tiles
	{ "bs005.1u",	0x80000, 0x46b400c4, 6 | BRF_GRA },           // 12

	{ "bs24_l.5j",	0x20000, 0xb0e0a452, 7 | BRF_SND },           // 13 OKI Samples
	{ "bs24_h.5l",	0x20000, 0xd24988ad, 7 | BRF_SND },           // 14
};

STD_ROM_PICK(djboyja)
STD_ROM_FN(djboyja)

struct BurnDriver BurnDrvDjboyja = {
	"djboyja", "djboy", NULL, NULL, "1989",
	"DJ Boy (Japan, set 2)\0", NULL, "Kaneko (Sega license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_KANEKO_MISC, GBF_SCRFIGHT, 0,
	NULL, djboyjaRomInfo, djboyjaRomName, NULL, NULL, NULL, NULL, DjboyInputInfo, DjboyDIPInfo,
	DjboyjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};
