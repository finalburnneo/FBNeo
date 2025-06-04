// FB Alpha Target Hits driver module
// Based on MAME driver by Manuel Abadia and David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "burn_pal.h"
#include "burn_gun.h"
#include "watchdog.h"
#include "mcs51.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvMCURAM;
static UINT8 *DrvMCUiRAM; // ds5002fp internal/scratch ram default.
static UINT16 *DrvVidRegs;
static UINT8 DrvRecalc;

static UINT8 *DrvTransTab;

static UINT8 oki_bank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT16 DrvAnalog[4];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT16 DrvGun0;
static INT16 DrvGun1;
static INT16 DrvGun2;
static INT16 DrvGun3;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo TargethInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	A("P1 Gun X",    	BIT_ANALOG_REL, &DrvGun0,       "mouse x-axis"	),
	A("P1 Gun Y",    	BIT_ANALOG_REL, &DrvGun1,       "mouse y-axis"	),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 2"	},

	{"P2 Coin",		    BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	A("P2 Gun X",    	BIT_ANALOG_REL, &DrvGun2,       "p2 x-axis"	),
	A("P2 Gun Y",    	BIT_ANALOG_REL, &DrvGun3,       "p2 y-axis"	),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Targeth)
#undef A

static struct BurnDIPInfo TargethDIPList[]=
{
	{0x0e, 0xff, 0xff, 0xff, NULL			},
	{0x0f, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x0e, 0x01, 0x07, 0x02, "6 Coins 1 Credits"	},
	{0x0e, 0x01, 0x07, 0x03, "5 Coins 1 Credits"	},
	{0x0e, 0x01, 0x07, 0x04, "4 Coins 1 Credits"	},
	{0x0e, 0x01, 0x07, 0x05, "3 Coins 1 Credits"	},
	{0x0e, 0x01, 0x07, 0x06, "2 Coins 1 Credits"	},
	{0x0e, 0x01, 0x07, 0x01, "3 Coins 2 Credits"	},
	{0x0e, 0x01, 0x07, 0x00, "4 Coins 3 Credits"	},
	{0x0e, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x0e, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x0e, 0x01, 0x38, 0x00, "3 Coins 4 Credits"	},
	{0x0e, 0x01, 0x38, 0x08, "2 Coins 3 Credits"	},
	{0x0e, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x0e, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x0e, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x0e, 0x01, 0x38, 0x18, "1 Coin  5 Credits"	},
	{0x0e, 0x01, 0x38, 0x10, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Credit configuration"	},
	{0x0e, 0x01, 0x40, 0x40, "Start 1C/Continue 1C"	},
	{0x0e, 0x01, 0x40, 0x00, "Start 2C/Continue 1C"	},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x0e, 0x01, 0x80, 0x80, "Off"			},
	{0x0e, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0f, 0x01, 0x03, 0x01, "Easy"			},
	{0x0f, 0x01, 0x03, 0x03, "Normal"		},
	{0x0f, 0x01, 0x03, 0x02, "Hard"			},
	{0x0f, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0f, 0x01, 0x20, 0x00, "Off"			},
	{0x0f, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Gun alarm"		},
	{0x0f, 0x01, 0x40, 0x40, "Off"			},
	{0x0f, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0f, 0x01, 0x80, 0x80, "Off"			},
	{0x0f, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Targeth)

static inline void set_oki_bank(INT32 data)
{
	oki_bank = data & 0xf;
	MSM6295SetBank(0, DrvSndROM + oki_bank * 0x10000, 0x30000, 0x3ffff);
}

static void __fastcall targeth_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x108000:
		case 0x108002:
		case 0x108004:
		case 0x108006:
			DrvVidRegs[(address & 0x06)/2] = data;
		return;

		case 0x10800c:
			BurnWatchdogWrite();
		return;
	}
}

static void __fastcall targeth_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x70000d:
			set_oki_bank(data);
		return;

		case 0x70000f:
			MSM6295Write(0,data);
		return;
	}
}

static UINT16 __fastcall targeth_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x108000:
		case 0x108002:
		case 0x108004:
		case 0x108006:
			return DrvAnalog[(address / 2) & 3];

		case 0x700000:
			return DrvDips[1];

		case 0x700002:
			return DrvDips[0];

		case 0x700006:
			return DrvInputs[0];

		case 0x700008:
			return DrvInputs[1];

		case 0x70000e:
			return MSM6295Read(0);
	}

	return 0;
}

static UINT8 __fastcall targeth_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x108000:
		case 0x108001:
		case 0x108002:
		case 0x108003:
		case 0x108004:
		case 0x108005:
		case 0x108006:
		case 0x108007:
			return DrvAnalog[(address / 2) & 3] >> ((~address & 1) * 8);

		case 0x700000:
		case 0x700001:
			return DrvDips[1];

		case 0x700002:
		case 0x700003:
			return DrvDips[0];

		case 0x700006:
		case 0x700007:
			return DrvInputs[0];

		case 0x700008:
		case 0x700009:
		case 0x70000a:
		case 0x70000b:
			return DrvInputs[1];

		case 0x70000e:
		case 0x70000f:
			return MSM6295Read(0);
	}

	return 0;
}

static void __fastcall targeth_palette_write_word(UINT32 address, UINT16 data)
{
	address &= 0x7fe;

	*((UINT16*)(BurnPalRAM + address)) = data;

	BurnPaletteWrite_xBBBBBGGGGGRRRRR(address);
}

static void __fastcall targeth_palette_write_byte(UINT32 address, UINT8 data)
{
	address &= 0x7ff;

	BurnPalRAM[address ^ 1] = data;

	BurnPaletteWrite_xBBBBBGGGGGRRRRR(address);
}

static void dallas_sharedram_write(INT32 address, UINT8 data)
{
	if (address >= MCS51_PORT_P0) return;

	if (address >= 0x8000 && address <= 0xffff)
		DrvShareRAM[(address & 0x7fff) ^ 1] = data;

	if (address >= 0x10000 && address <= 0x17fff)
		DrvMCURAM[address & 0x7fff] = data;
}

static UINT8 dallas_sharedram_read(INT32 address)
{
	if (address >= MCS51_PORT_P0) return 0;

	if (address >= 0x8000 && address <= 0xffff)
		return DrvShareRAM[(address & 0x7fff) ^ 1];

	if (address >= 0x10000 && address <= 0x17fff)
		return DrvMCURAM[address & 0x7fff];

	return 0;
}

static tilemap_callback( layer0 )
{
	UINT16 *ram = (UINT16*)(DrvVidRAM + (offs * 4));

	INT32 skip = DrvTransTab[ram[0] & 0x3fff] ? TILE_SKIP : 0;

	TILE_SET_INFO(0, ram[0], ram[1], TILE_FLIPXY(ram[1] >> 5) | skip);
}

static tilemap_callback( layer1 )
{
	UINT16 *ram = (UINT16*)(DrvVidRAM + 0x2000 + (offs * 4));

	TILE_SET_INFO(0, ram[0], ram[1], TILE_FLIPXY(ram[1] >> 5));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	mcs51_reset();
	ds5002fp_iram_fill(DrvMCUiRAM, 0x80);

	MSM6295Reset(0);

	set_oki_bank(3);

	BurnWatchdogReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x080000;

	DrvMCUROM	= Next; Next += 0x008000;

	DrvGfxROM	= Next; Next += 0x400000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x100000;

	DrvTransTab	= Next; Next += 0x400000 / 0x100;

	BurnPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	BurnPalRAM	= Next; Next += 0x000800;
	DrvVidRAM	= Next; Next += 0x004000;
	Drv68KRAM	= Next; Next += 0x008000;
	DrvSprRAM	= Next; Next += 0x001000;
	DrvShareRAM	= Next; Next += 0x008000;

	DrvVidRegs	= (UINT16*)Next; Next += 0x000004 * sizeof(UINT16);

	RamEnd		= Next;

	DrvMCURAM	= Next; Next += 0x008000; // NV RAM
	DrvMCUiRAM  = Next; Next += 0x0000ff; // default Internal Ram of DS5002fp, loaded from romset

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { 3*0x80000*8, 2*0x80000*8, 1*0x80000*8, 0*0x80000*8 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);

	memcpy (tmp, DrvGfxROM, 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static void DrvTransTableInit()
{
	INT32 tile_size = 16 * 16;

	for (INT32 i = 0; i < 0x400000; i+= tile_size)
	{
		for (INT32 k = 0; k < 3; k++)
		{
			DrvTransTab[i/tile_size] = 1;

			for (INT32 j = 0; j < tile_size; j++)
			{
				if (DrvGfxROM[i+j] != 0) {
					DrvTransTab[i / tile_size] = 0;
					break;
				}
			}
		}
	}
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
		if (BurnLoadRom(Drv68KROM + 0x0000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvMCUROM + 0x0000000,  2, 1)) return 1;
		memcpy (DrvMCURAM, DrvMCUROM, 0x8000);

		if (BurnLoadRom(DrvMCUiRAM + 0x0000000,  3, 1)) return 1; // SCRATCH RAM...

		if (BurnLoadRom(DrvGfxROM + 0x0000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0080000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0100000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x0180000,  7, 1)) return 1;

		if (BurnLoadRom(DrvSndROM + 0x0000000,  8, 1)) return 1;
		if (BurnLoadRom(DrvSndROM + 0x0080000,  9, 1)) return 1;

		DrvGfxDecode();
		DrvTransTableInit();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvVidRAM,		0x100000, 0x103fff, MAP_RAM);
	SekMapMemory(BurnPalRAM,	0x200000, 0x2007ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x440000, 0x440fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xfe0000, 0xfe7fff, MAP_RAM);
	SekMapMemory(DrvShareRAM,	0xfe8000, 0xfeffff, MAP_RAM);
	SekSetWriteWordHandler(0,	targeth_main_write_word);
	SekSetWriteByteHandler(0,	targeth_main_write_byte);
	SekSetReadWordHandler(0,	targeth_main_read_word);
	SekSetReadByteHandler(0,	targeth_main_read_byte);

	SekMapHandler(1,		0x200000, 0x2007ff, MAP_WRITE);
	SekSetWriteWordHandler(1,	targeth_palette_write_word);
	SekSetWriteByteHandler(1,	targeth_palette_write_byte);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	ds5002fp_init(0x49, 0x00, 0x80);
	mcs51_set_program_data(DrvMCUROM);
	mcs51_set_write_handler(dallas_sharedram_write);
	mcs51_set_read_handler(dallas_sharedram_read);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 16, 16, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 16, 16, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 16, 16, 0x400000, 0, 0x1f);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetOffsets(0, -24, -16);
	GenericTilemapSetOffsets(1, -24, -16);

	BurnGunInit(2, true);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);

	SekExit();
	mcs51_exit();

	BurnGunExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void draw_sprites()
{
	UINT16 *m_spriteram = (UINT16*)DrvSprRAM;

	for (int i = 3; i < (0x1000 - 6)/2; i += 4){
		int sx = m_spriteram[i+2] & 0x03ff;
		int sy = (240 - (m_spriteram[i] & 0x00ff)) & 0x00ff;
		int code = m_spriteram[i+3] & 0x3fff;
		int color = (m_spriteram[i+2] & 0x7c00) >> 10;
		int attr = (m_spriteram[i] & 0xfe00) >> 9;

		int flipx = attr & 0x20;
		int flipy = attr & 0x40;

		if (DrvTransTab[code]) continue;

		Draw16x16MaskTile(pTransDraw, code, sx - 40, sy - 16, flipx, flipy, color, 4, 0, 0x200, DrvGfxROM);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_xBBBBBGGGGGRRRRR();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollY(0, DrvVidRegs[0]);
	GenericTilemapSetScrollX(0, DrvVidRegs[1] + 4);
	GenericTilemapSetScrollY(1, DrvVidRegs[2]);
	GenericTilemapSetScrollX(1, DrvVidRegs[3]);

	if ((nBurnLayer & 1) == 0) BurnTransferClear();
	
	if ((nBurnLayer & 1) == 1) GenericTilemapDraw(1, pTransDraw, 0);
	if ((nBurnLayer & 2) == 2) GenericTilemapDraw(0, pTransDraw, 0);

	if ((nBurnLayer & 4) == 4) draw_sprites();

	BurnTransferCopy(BurnPalette);

	BurnGunDrawTargets();

	return 0;
}

static INT32 scale_gun(INT32 gun, double scale)
{
	INT32 x = 0;
	if (scale > 0.0)
		x = gun * scale;
	else
		x = -(1.0 - gun) * scale;

	return gun + x;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
		BurnGunMakeInputs(0, DrvGun0, DrvGun1);
		BurnGunMakeInputs(1, DrvGun2, DrvGun3);

		DrvAnalog[0] = scale_gun(BurnGunReturnX(0) * 404 / 255, -0.146) + 0x29;
		DrvAnalog[1] = scale_gun(BurnGunReturnY(0), -0.062) + 4 + (log((double)(0x100 - BurnGunReturnY(0))) + 0.7);
		DrvAnalog[2] = scale_gun(BurnGunReturnX(1) * 404 / 255, -0.146) + 0x29;
		DrvAnalog[3] = scale_gun(BurnGunReturnY(1), -0.062) + 4 + (log((double)(0x100 - BurnGunReturnY(1))) + 0.7);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 12000000 / 60, 12000000 / 12 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += SekRun(nNext - nCyclesDone[0]);

		if (i == 128) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		if (i == 160) SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
		if (i == 232) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);

		nCyclesDone[1] += mcs51Run((SekTotalCycles() / 12) - nCyclesDone[1]);
	}

	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ba.Data		= AllRam;
		ba.nLen		= RamEnd - AllRam;
		ba.nAddress	= 0;
		ba.szName	= "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvMCURAM;
		ba.nLen		= 0x008000;
		ba.nAddress	= 0;
		ba.szName	= "MCU RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		MSM6295Scan(nAction, pnMin);

		mcs51_scan(nAction);

		BurnWatchdogScan(nAction);

		BurnGunScan();

		SCAN_VAR(oki_bank);
	}

	if (nAction & ACB_WRITE) {
		set_oki_bank(oki_bank);
	}

	return 0;
}


// Target Hits (ver 1.1, checksum 5152)

static struct BurnRomInfo targethRomDesc[] = {
	{ "th2_b_c_23.c23",           0x40000, 0x840887d6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "th2_b_c_22.c22",           0x40000, 0xd2435eb8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "targeth_ds5002fp.bin",     0x08000, 0xabcdfee4, 2 | BRF_PRG | BRF_ESS }, //  2 DS5002fp SRAM

	{ "targeth_ds5002fp_scratch", 0x00080, 0xc927bcb1, 3 | BRF_PRG | BRF_ESS }, //  3 DS5002fp Internal RAM

	{ "targeth.i13",              0x80000, 0xb892be24, 4 | BRF_GRA },           //  4 Graphics
	{ "targeth.i11",              0x80000, 0x6797faf9, 4 | BRF_GRA },           //  5
	{ "targeth.i9",               0x80000, 0x0e922c1c, 4 | BRF_GRA },           //  6
	{ "targeth.i7",               0x80000, 0xd8b41000, 4 | BRF_GRA },           //  7

	{ "targeth.c1",               0x80000, 0xd6c9dfbc, 5 | BRF_SND },           //  8 Samples
	{ "targeth.c3",               0x80000, 0xd4c771df, 5 | BRF_SND },           //  9
};

STD_ROM_PICK(targeth)
STD_ROM_FN(targeth)

struct BurnDriver BurnDrvTargeth = {
	"targeth", NULL, NULL, NULL, "1994",
	"Target Hits (ver 1.1, checksum 5152)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, targethRomInfo, targethRomName, NULL, NULL, NULL, NULL, TargethInputInfo, TargethDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 232, 4, 3
};

// Target Hits (ver 1.1, checksum 86E1)

static struct BurnRomInfo targethaRomDesc[] = {
	{ "th2_n_c_23.c23",           0x40000, 0xb99b25dc, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "th2_n_c_22.c22",           0x40000, 0x6d34f0cf, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "targeth_ds5002fp.bin",     0x08000, 0xabcdfee4, 2 | BRF_PRG | BRF_ESS }, //  2 DS5002fp SRAM

	{ "targeth_ds5002fp_scratch", 0x00080, 0xc927bcb1, 3 | BRF_PRG | BRF_ESS }, //  3 DS5002fp Internal RAM

	{ "targeth.i13",              0x80000, 0xb892be24, 4 | BRF_GRA },           //  4 Graphics
	{ "targeth.i11",              0x80000, 0x6797faf9, 4 | BRF_GRA },           //  5
	{ "targeth.i9",               0x80000, 0x0e922c1c, 4 | BRF_GRA },           //  6
	{ "targeth.i7",               0x80000, 0xd8b41000, 4 | BRF_GRA },           //  7

	{ "targeth.c1",               0x80000, 0xd6c9dfbc, 5 | BRF_SND },           //  8 Samples
	{ "targeth.c3",               0x80000, 0xd4c771df, 5 | BRF_SND },           //  9
};

STD_ROM_PICK(targetha)
STD_ROM_FN(targetha)

struct BurnDriver BurnDrvTargetha = {
	"targetha", "targeth", NULL, NULL, "1994",
	"Target Hits (ver 1.1, checksum 86E1)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, targethaRomInfo, targethaRomName, NULL, NULL, NULL, NULL, TargethInputInfo, TargethDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 232, 4, 3
};

// Target Hits (ver 1.1, checksum B1F7)

static struct BurnRomInfo targethbRomDesc[] = {
	{ "zigurat_e_esc_25-oct-94_f616_27c020.bin", 0x40000, 0x296085a6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "zigurat_o_esc_25-oct-94_405d_27c020.bin", 0x40000, 0xef93a1cc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "targeth_ds5002fp.bin",                    0x08000, 0xabcdfee4, 2 | BRF_PRG | BRF_ESS }, //  2 DS5002fp SRAM

	{ "targeth_ds5002fp_scratch",                0x00080, 0xc927bcb1, 3 | BRF_PRG | BRF_ESS }, //  3 DS5002fp Internal RAM

	{ "targeth.i13",                             0x80000, 0xb892be24, 4 | BRF_GRA },           //  4 Graphics
	{ "targeth.i11",                             0x80000, 0x6797faf9, 4 | BRF_GRA },           //  5
	{ "targeth.i9",                              0x80000, 0x0e922c1c, 4 | BRF_GRA },           //  6
	{ "targeth.i7",                              0x80000, 0xd8b41000, 4 | BRF_GRA },           //  7

	{ "targeth.c1",                              0x80000, 0xd6c9dfbc, 5 | BRF_SND },           //  8 Samples
	{ "targeth.c3",                              0x80000, 0xd4c771df, 5 | BRF_SND },           //  9
};

STD_ROM_PICK(targethb)
STD_ROM_FN(targethb)

struct BurnDriver BurnDrvTargethb = {
	"targethb", "targeth", NULL, NULL, "1994",
	"Target Hits (ver 1.1, checksum B1F7)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, targethbRomInfo, targethbRomName, NULL, NULL, NULL, NULL, TargethInputInfo, TargethDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 232, 4, 3
};


// Target Hits (ver 1.0, checksum FBCB)

static struct BurnRomInfo targeth10RomDesc[] = {
	{ "c23.bin",                  0x40000, 0xe38a54e2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "c22.bin",                  0x40000, 0x24fe3efb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "targeth_ds5002fp.bin",     0x08000, 0xabcdfee4, 2 | BRF_PRG | BRF_ESS }, //  2 DS5002fp SRAM

	{ "targeth_ds5002fp_scratch", 0x00080, 0xc927bcb1, 3 | BRF_PRG | BRF_ESS }, //  3 DS5002fp Internal RAM

	{ "targeth.i13",              0x80000, 0xb892be24, 4 | BRF_GRA },           //  4 Graphics
	{ "targeth.i11",              0x80000, 0x6797faf9, 4 | BRF_GRA },           //  5
	{ "targeth.i9",               0x80000, 0x0e922c1c, 4 | BRF_GRA },           //  6
	{ "targeth.i7",               0x80000, 0xd8b41000, 4 | BRF_GRA },           //  7

	{ "targeth.c1",               0x80000, 0xd6c9dfbc, 5 | BRF_SND },           //  8 Samples
	{ "targeth.c3",               0x80000, 0xd4c771df, 5 | BRF_SND },           //  9
};

STD_ROM_PICK(targeth10)
STD_ROM_FN(targeth10)

struct BurnDriver BurnDrvTargeth10 = {
	"targeth10", "targeth", NULL, NULL, "1994",
	"Target Hits (ver 1.0, checksum FBCB)\0", NULL, "Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, targeth10RomInfo, targeth10RomName, NULL, NULL, NULL, NULL, TargethInputInfo, TargethDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 232, 4, 3
};


// Quick Shots (ver 1.0, checksum AD0C)

static struct BurnRomInfo quickshtsRomDesc[] = {
	{ "book_16-06_e_7fd9_27c020.bin", 0x40000, 0x74999de4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "book_16-06_o_6f77_27c020.bin", 0x40000, 0x9305509b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "targeth_ds5002fp.bin",         0x08000, 0xabcdfee4, 2 | BRF_PRG | BRF_ESS }, //  2 DS5002fp SRAM

	{ "targeth_ds5002fp_scratch",     0x00080, 0xc927bcb1, 3 | BRF_PRG | BRF_ESS }, //  3 DS5002fp Internal RAM

	{ "targeth.i13",                  0x80000, 0xb892be24, 4 | BRF_GRA },           //  4 Graphics
	{ "targeth.i11",                  0x80000, 0x6797faf9, 4 | BRF_GRA },           //  5
	{ "targeth.i9",                   0x80000, 0x0e922c1c, 4 | BRF_GRA },           //  6
	{ "targeth.i7",                   0x80000, 0xd8b41000, 4 | BRF_GRA },           //  7

	{ "targeth.c1",                   0x80000, 0xd6c9dfbc, 5 | BRF_SND },           //  8 Samples
	{ "targeth.c3",                   0x80000, 0xd4c771df, 5 | BRF_SND },           //  9
};

STD_ROM_PICK(quickshts)
STD_ROM_FN(quickshts)

struct BurnDriver BurnDrvQuickshts = {
	"quickshts", "targeth", NULL, NULL, "1994",
	"Quick Shots (ver 1.0, checksum AD0C)\0", NULL, "Zero / Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, quickshtsRomInfo, quickshtsRomName, NULL, NULL, NULL, NULL, TargethInputInfo, TargethDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 232, 4, 3
};
