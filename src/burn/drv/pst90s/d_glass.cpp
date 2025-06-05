// FB Alpha G hardware driver module
// Based on MAME driver by Manuel Abadia

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "msm6295.h"
#include "burn_pal.h"
#include "mcs51.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvBltROM;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRegs;
static UINT8 *DrvMCURAM;

static UINT8 DrvRecalc;

static UINT8 blitter_serial_buffer;
static UINT8 current_bit;
static UINT8 current_command;
static INT32 interrupt_enable;
static UINT8 oki_bank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[2];
static UINT8 DrvReset;

static INT32 has_mcu = 0;

static struct BurnInputInfo GlassInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 8,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Glass)

static struct BurnDIPInfo GlassDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x07, 0x00, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0x07, 0x01, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0x07, 0x02, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x13, 0x01, 0x38, 0x10, "6 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x18, "5 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x20, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x28, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x30, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x38, 0x08, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0x38, 0x00, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Credit configuration"	},
	{0x13, 0x01, 0x40, 0x40, "Start 1C"		},
	{0x13, 0x01, 0x40, 0x00, "Start 2C"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Normal"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x0c, 0x0c, "3"			},
	{0x14, 0x01, 0x0c, 0x08, "1"			},
	{0x14, 0x01, 0x0c, 0x04, "2"			},
	{0x14, 0x01, 0x0c, 0x00, "4"			},

	{0   , 0xfe, 0   ,    2, "Version"		},
	{0x14, 0x01, 0x10, 0x10, "Normal"		},
	{0x14, 0x01, 0x10, 0x00, "Light"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x20, 0x00, "Off"			},
	{0x14, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Glass)

static void bankswitch(INT32 bank)
{
	oki_bank = bank & 0xf;

	MSM6295SetBank(0, DrvSndROM + (oki_bank * 0x10000), 0x30000, 0x3ffff);
}	

static void blitter_write(INT32 data)
{
	blitter_serial_buffer <<= 1;
	blitter_serial_buffer |= data & 0x01;
	current_bit++;

	if (current_bit == 5)
	{
		current_command = blitter_serial_buffer;
		current_bit = 0;
	}
}

static void __fastcall glass_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x108000:
		case 0x108002:
		case 0x108004:
		case 0x108006:
			*((UINT16*)(DrvVidRegs + (address & 0x06))) = BURN_ENDIAN_SWAP_INT16(data);
		return;

		case 0x700008:
			blitter_write(data);
		return;

		case 0x108008:
			interrupt_enable = 1;
		return;
	}
}

static void __fastcall glass_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x108008:
		case 0x108009:
			interrupt_enable = 1;
		return;

		case 0x70000d:
			bankswitch(data);
		return;

		case 0x70000f:
			MSM6295Write(0, data);
		return;
	}

	// coin counter/lockout (not used in FBA)
	if (address >= 0x70000a && address <= 0x70004b) {
		return;
	}
}

static UINT8 __fastcall glass_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x700001:
			return DrvDips[1];

		case 0x700003:
			return DrvDips[0];

		case 0x700005:
			return DrvInputs[0];

		case 0x700006:
			return DrvInputs[1] >> 8;

		case 0x700007:
			return DrvInputs[1];

		case 0x70000f:
			return MSM6295Read(0);
	}

	return 0;
}

static void dallas_sharedram_write(INT32 address, UINT8 data)
{
	if (address >= MCS51_PORT_P0) return;

	if (address <= 0xffff)
		Drv68KRAM[(address & 0x3fff) ^ 1] = data;

	if (address >= 0x10000 && address <= 0x17fff)
		DrvMCURAM[address & 0x7fff] = data;
}

static UINT8 dallas_sharedram_read(INT32 address)
{
	if (address >= MCS51_PORT_P0) return 0;

	if (address <= 0xffff)
		return Drv68KRAM[(address & 0x3fff) ^ 1];

	if (address >= 0x10000 && address <= 0x17fff)
		return DrvMCURAM[address & 0x7fff];

	return 0;
}

static tilemap_callback( screen0 )
{
	UINT16 *ram = (UINT16*)DrvVidRAM;

	UINT16 data = BURN_ENDIAN_SWAP_INT16(ram[(offs << 1) + 0]);
	UINT16 attr = BURN_ENDIAN_SWAP_INT16(ram[(offs << 1) + 1]);
	UINT32 code = ((data & 0x03) << 14) | ((data & 0x0fffc) >> 2);

	TILE_SET_INFO(0, code, attr, TILE_FLIPYX(attr >> 6));
}

static tilemap_callback( screen1 )
{
	UINT16 *ram = (UINT16*)(DrvVidRAM + 0x1000);

	UINT16 data = BURN_ENDIAN_SWAP_INT16(ram[(offs << 1) + 0]);
	UINT16 attr = BURN_ENDIAN_SWAP_INT16(ram[(offs << 1) + 1]);
	UINT32 code = ((data & 0x03) << 14) | ((data & 0x0fffc) >> 2);

	TILE_SET_INFO(0, code, attr, TILE_FLIPYX(attr >> 6));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	mcs51_reset();

	MSM6295Reset(0);

	bankswitch(3);

	interrupt_enable = 1;
	blitter_serial_buffer = 0;
	current_bit = 0;
	current_command = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x100000;

	DrvMCUROM	= Next; Next += 0x008000;

	DrvGfxROM	= Next; Next += 0x800000;
	DrvBltROM	= Next; Next += 0x100000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x100000;

	BurnPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	BurnPalRAM	= Next; Next += 0x000800;
	Drv68KRAM	= Next; Next += 0x004000;
	DrvVidRAM	= Next; Next += 0x003000;
	DrvSprRAM	= Next; Next += 0x001000;

	DrvVidRegs	= Next; Next += 0x000008;

	RamEnd		= Next;

	DrvMCURAM	= Next; Next += 0x008000;

	MemEnd		= Next;

	return 0;
}

static void ROM16_split_gfx(UINT8 *src, UINT8 *dst, int start, int length, int dest1, int dest2)
{
	for (INT32 i = 0; i < length / 2; i++)
	{
		dst[dest1 + i] = src[start + i * 2 + 0];
		dst[dest2 + i] = src[start + i * 2 + 1];
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { (0x100000*8*3),(0x100000*8*2),(0x100000*8*1),(0x100000*8*0) };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);

	ROM16_split_gfx(DrvGfxROM, tmp, 0x0000000, 0x0200000, 0x0000000, 0x0100000);
	ROM16_split_gfx(DrvGfxROM, tmp, 0x0200000, 0x0200000, 0x0200000, 0x0300000);

	GfxDecode(0x8000, 4, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 LoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad = Drv68KROM;
	UINT8 *gLoad = DrvGfxROM;
	UINT8 *bLoad = DrvBltROM;
	UINT8 *sLoad = DrvSndROM;
	UINT8 *mLoad = DrvMCUROM;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 0xf) == 1) {
			if (BurnLoadRom(pLoad + 1, i+0, 2)) return 1;
			if (BurnLoadRom(pLoad + 0, i+1, 2)) return 1;
			pLoad += ri.nLen * 2; i++;
			continue;
		}

		if ((ri.nType & 0xf) == 2) {
			if (BurnLoadRom(gLoad, i, 1)) return 1;
			gLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 3) {
			if (BurnLoadRom(gLoad + 0, i+0, 2)) return 1;
			if (BurnLoadRom(gLoad + 1, i+1, 2)) return 1;
			gLoad += ri.nLen * 2; i++;
			continue;
		}

		if ((ri.nType & 0xf) == 4) {
			if (BurnLoadRom(bLoad, i, 1)) return 1;
			bLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 5) {
			if (BurnLoadRom(sLoad, i, 1)) return 1;
			sLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 6) {
			if (BurnLoadRom(mLoad, i, 1)) return 1;
			mLoad += ri.nLen;
			has_mcu = 1;
			continue;
		}
	}

	if (has_mcu) memcpy(DrvMCURAM, DrvMCUROM, 0x8000);

	DrvGfxDecode();

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (LoadRoms()) return 1;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvVidRAM,		0x100000, 0x102fff, MAP_RAM);
	SekMapMemory(BurnPalRAM,	0x200000, 0x2007ff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x440000, 0x440fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xfec000, 0xfeffff, MAP_RAM);
	SekSetWriteWordHandler(0,	glass_write_word);
	SekSetWriteByteHandler(0,	glass_write_byte);
	SekSetReadByteHandler(0,	glass_read_byte);
	SekClose();

	ds5002fp_init(0x29, 0x00, 0x80);
	mcs51_set_program_data(DrvMCUROM);
	mcs51_set_write_handler(dallas_sharedram_write);
	mcs51_set_read_handler(dallas_sharedram_read);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, screen0_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, screen1_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 4, 16, 16, 0x400000*2, 0x200, 0x1f);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);
	MSM6295ROM = NULL;

	mcs51_exit();

	SekExit();

	BurnFree (AllMem);

	has_mcu = 0;

	return 0;
}

static void draw_bitmap()
{
	if (current_command & 0x18)
	{		
		UINT8 *gfx = DrvBltROM + (current_command & 0x0f) * 0x10000 + 320;

		for (INT32 j = 0; j < 200; j++)
		{
			UINT16 *dst = pTransDraw + ((j + 20) * nScreenWidth) + 24;

			for (INT32 i = 0; i < 320; i++, gfx++)
			{
				dst[i] = *gfx;
			}
		}
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 i = 3; i < (0x1000 - 6) / 2; i += 4)
	{
		INT32 sx    =(BURN_ENDIAN_SWAP_INT16(ram[i + 2]) & 0x01ff) - 15;
		INT32 sy    = ((240 - (BURN_ENDIAN_SWAP_INT16(ram[i]) & 0x00ff)) & 0x00ff) - 16;
		INT32 code  = BURN_ENDIAN_SWAP_INT16(ram[i + 3]);
		INT32 color =(BURN_ENDIAN_SWAP_INT16(ram[i + 2]) & 0x1e00) >> 9;
		INT32 attr  = BURN_ENDIAN_SWAP_INT16(ram[i]);

		INT32 flipx = attr & 0x4000;
		INT32 flipy = attr & 0x8000;

		code = ((code & 0x03) << 14) | ((code & 0x0fffc) >> 2);

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code & 0x7fff, sx, sy, color, 4, 0, 0x100, DrvGfxROM);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code & 0x7fff, sx, sy, color, 4, 0, 0x100, DrvGfxROM);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code & 0x7fff, sx, sy, color, 4, 0, 0x100, DrvGfxROM);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code & 0x7fff, sx, sy, color, 4, 0, 0x100, DrvGfxROM);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_xBBBBBGGGGGRRRRR();
		DrvRecalc = 1;
	}

	UINT16 *vreg = (UINT16*)DrvVidRegs;

	GenericTilemapSetScrollY(0, BURN_ENDIAN_SWAP_INT16(vreg[0]));
	GenericTilemapSetScrollX(0, BURN_ENDIAN_SWAP_INT16(vreg[1]) + 4);
	GenericTilemapSetScrollY(1, BURN_ENDIAN_SWAP_INT16(vreg[2]));
	GenericTilemapSetScrollX(1, BURN_ENDIAN_SWAP_INT16(vreg[3]));

	BurnTransferClear();

	if (nBurnLayer & 1) draw_bitmap();

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);
	if (nBurnLayer & 4) GenericTilemapDraw(0, pTransDraw, 0);

	if (nBurnLayer & 8) draw_sprites();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, 2 * sizeof(UINT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 12000000 / 60, 12000000 / 12 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += SekRun(nNext - nCyclesDone[0]);

		if (i == 240 && interrupt_enable) {
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
			interrupt_enable = 0;
		}

		if (has_mcu) {
			nCyclesDone[1] += mcs51Run((SekTotalCycles() / 12) - nCyclesDone[1]);
		}
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
		ba.Data		= DrvVidRAM;
		ba.nLen		= 0x003000;
		ba.nAddress	= 0x100000;
		ba.szName	= "Video RAM";
		BurnAcb(&ba);

		ba.Data		= BurnPalRAM;
		ba.nLen		= 0x000800;
		ba.nAddress	= 0x200000;
		ba.szName	= "Palette RAM";
		BurnAcb(&ba);

		ba.Data		= DrvSprRAM;
		ba.nLen		= 0x001000;
		ba.nAddress	= 0x440000;
		ba.szName	= "Sprite RAM";
		BurnAcb(&ba);

		ba.Data		= Drv68KRAM;
		ba.nLen		= 0x004000;
		ba.nAddress	= 0xfec000;
		ba.szName	= "68K RAM";
		BurnAcb(&ba);

		ba.Data		= DrvVidRegs;
		ba.nLen		= 8;
		ba.nAddress	= 0x108000;
		ba.szName	= "Regs";
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

		mcs51_scan(nAction);

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(oki_bank);
		SCAN_VAR(interrupt_enable);
		SCAN_VAR(current_command);
		SCAN_VAR(current_bit);
		SCAN_VAR(blitter_serial_buffer);
	}

	if (nAction & ACB_WRITE) {
		bankswitch(oki_bank);
	}

	return 0;
}


// Glass (Ver 1.1, Break Edition, checksum 49D5E66B, Version 1994, set 1)

static struct BurnRomInfo glassRomDesc[] = {
	{ "europa_c23_3-2-94_3fb0 27c020.bin",		0x040000, 0xf2932dc2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "europa_c22_3-2-94_a20c_27c020.bin",		0x040000, 0x165e2e01, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "h13.bin",								0x200000, 0x13ab7f31, 2 | BRF_GRA },           //  2 Graphics
	{ "h11.bin",								0x200000, 0xc6ac41c8, 2 | BRF_GRA },           //  3

	{ "h9.bin",									0x100000, 0xb9492557, 4 | BRF_GRA },           //  4 Blitter data

	{ "c1.bin",									0x100000, 0xd9f075a2, 5 | BRF_SND },           //  5 Samples

	{ "glass_ds5002fp_sram.bin",				0x008000, 0x47c9df4c, 6 | BRF_PRG | BRF_ESS }, //  6 Dallas MCU
};

STD_ROM_PICK(glass)
STD_ROM_FN(glass)

struct BurnDriver BurnDrvGlass = {
	"glass", NULL, NULL, NULL, "1994",
	"Glass (Ver 1.1, Break Edition, checksum 49D5E66B, Version 1994, set 1)\0", NULL, "OMK / Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, glassRomInfo, glassRomName, NULL, NULL, NULL, NULL, GlassInputInfo, GlassDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 240, 4, 3
};


// Super Splash (Ver 1.1, Break Edition, checksum 59D5E66B, Version 1994)

static struct BurnRomInfo ssplashRomDesc[] = {
	{ "c_23_splash_titulo_11-4_27c2001.bin",	0x040000, 0x563c4883, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "europa_c22_3-2-94_a20c_27c020.bin",		0x040000, 0x165e2e01, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "h13.bin",								0x200000, 0x13ab7f31, 2 | BRF_GRA },           //  2 Graphics
	{ "h11.bin",								0x200000, 0xc6ac41c8, 2 | BRF_GRA },           //  3

	{ "h9.bin",									0x100000, 0xb9492557, 4 | BRF_GRA },           //  4 Blitter data

	{ "c1.bin",									0x100000, 0xd9f075a2, 5 | BRF_SND },           //  5 Samples

	{ "glass_ds5002fp_sram.bin",				0x008000, 0x47c9df4c, 6 | BRF_PRG | BRF_ESS }, //  6 Dallas MCU
};

STD_ROM_PICK(ssplash)
STD_ROM_FN(ssplash)

struct BurnDriver BurnDrvSsplash = {
	"ssplash", "glass", NULL, NULL, "1994",
	"Super Splash (Ver 1.1, Break Edition, checksum 59D5E66B, Version 1994)\0", NULL, "OMK / Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, ssplashRomInfo, ssplashRomName, NULL, NULL, NULL, NULL, GlassInputInfo, GlassDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 240, 4, 3
};


// Glass (Ver 1.1, Break Edition, checksum 49D5E66B, Version 1994, set 2)

static struct BurnRomInfo glassaRomDesc[] = {
	{ "1.c23",						0x040000, 0xaeebd4ed, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2.c22",						0x040000, 0x165e2e01, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "h13.bin",					0x200000, 0x13ab7f31, 2 | BRF_GRA },           //  2 Graphics
	{ "h11.bin",					0x200000, 0xc6ac41c8, 2 | BRF_GRA },           //  3

	{ "h9.bin",						0x100000, 0xb9492557, 4 | BRF_GRA },           //  4 Blitter data

	{ "c1.bin",						0x100000, 0xd9f075a2, 5 | BRF_SND },           //  5 Samples

	{ "glass_ds5002fp_sram.bin",	0x008000, 0x47c9df4c, 6 | BRF_PRG | BRF_ESS }, //  6 Dallas MCU
};

STD_ROM_PICK(glassa)
STD_ROM_FN(glassa)

struct BurnDriver BurnDrvGlassa = {
	"glassa", "glass", NULL, NULL, "1994",
	"Glass (Ver 1.1, Break Edition, checksum 49D5E66B, Version 1994, set 2)\0", NULL, "OMK / Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, glassaRomInfo, glassaRomName, NULL, NULL, NULL, NULL, GlassInputInfo, GlassDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 240, 4, 3
};


// Glass (Ver 1.1, Break Edition, checksum D7AF5496, Version 1994, US)

static struct BurnRomInfo glassatRomDesc[] = {
	{ "atari_c-23_3-2-94_27c020.bin",	0x040000, 0xff26f608, 1 | BRF_PRG | BRF_ESS },    //  0 68K Code
	{ "atari_c-22_3-2-94_27c020.bin",	0x040000, 0xad805bc3, 1 | BRF_PRG | BRF_ESS },    //  1

	{ "h13.bin",						0x200000, 0x13ab7f31, 2 | BRF_GRA },              //  2 Graphics
	{ "h11.bin",						0x200000, 0xc6ac41c8, 2 | BRF_GRA },              //  3

	{ "atari_el_4269_27c4001.bin",		0x080000, 0x514e50ea, 4 | BRF_GRA },              //  4 Blitter data
	{ "atari_eh",						0x080000, 0x00000000, 4 | BRF_NODUMP | BRF_GRA }, //  5

	{ "c1.bin",							0x100000, 0xd9f075a2, 5 | BRF_SND },              //  6 Samples

	{ "glass_ds5002fp_sram.bin",		0x008000, 0x47c9df4c, 6 | BRF_PRG | BRF_ESS },    //  7 Dallas MCU
};

STD_ROM_PICK(glassat)
STD_ROM_FN(glassat)

struct BurnDriver BurnDrvGlassat = {
	"glassat", "glass", NULL, NULL, "1994",
	"Glass (Ver 1.1, Break Edition, checksum D7AF5496, Version 1994, US)\0", NULL, "OMK / Gaelco (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, glassatRomInfo, glassatRomName, NULL, NULL, NULL, NULL, GlassInputInfo, GlassDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 240, 4, 3
};


// Glass (Ver 1.1, Break Edition, checksum D419AB69, Version 1994, unprotected, Korea)

static struct BurnRomInfo glasskrRomDesc[] = {
	{ "c_23_korea_4f3e_27c020.bin",		0x040000, 0x4d7749e4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "c_22_korea_7522_27c020.bin",		0x040000, 0xa23bba9e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "h13.bin",						0x200000, 0x13ab7f31, 2 | BRF_GRA },           //  2 Graphics
	{ "h11.bin",						0x200000, 0xc6ac41c8, 2 | BRF_GRA },           //  3

	{ "glassk.h9",						0x100000, 0xd499be4c, 4 | BRF_GRA },           //  4 Blitter data

	{ "c1.bin",							0x100000, 0xd9f075a2, 5 | BRF_SND },           //  5 Samples
};

STD_ROM_PICK(glasskr)
STD_ROM_FN(glasskr)

struct BurnDriver BurnDrvGlasskr = {
	"glasskr", "glass", NULL, NULL, "1994",
	"Glass (Ver 1.1, Break Edition, checksum D419AB69, Version 1994, unprotected, Korea)\0", NULL, "OMK / Gaelco (Promat license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, glasskrRomInfo, glasskrRomName, NULL, NULL, NULL, NULL, GlassInputInfo, GlassDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 240, 4, 3
};


// Glass (Ver 1.1, Break Edition, checksum 3D8A724F, Version 1994, unprotected, Korea)

static struct BurnRomInfo glasskraRomDesc[] = {
	{ "c_23_alt_korea_7ce8_27c020.bin",		0x040000, 0x6534fd44, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "c_22_alt_korea_8ce7_27c020.bin",		0x040000, 0xccde51eb, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "h13.bin",							0x200000, 0x13ab7f31, 2 | BRF_GRA },           //  2 Graphics
	{ "h11.bin",							0x200000, 0xc6ac41c8, 2 | BRF_GRA },           //  3

	{ "el_korea_d4f1_27c4001.bin",			0x080000, 0xb7b78b93, 4 | BRF_GRA },           //  4 Blitter data
	{ "eh_glass.bin",						0x080000, 0xea568c62, 4 | BRF_GRA },           //  5

	{ "c1.bin",								0x100000, 0xd9f075a2, 5 | BRF_SND },           //  6 Samples
};

STD_ROM_PICK(glasskra)
STD_ROM_FN(glasskra)

struct BurnDriver BurnDrvGlasskra = {
	"glasskra", "glass", NULL, NULL, "1994",
	"Glass (Ver 1.1, Break Edition, checksum 3D8A724F, Version 1994, unprotected, Korea)\0", NULL, "OMK / Gaelco (Promat license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, glasskraRomInfo, glasskraRomName, NULL, NULL, NULL, NULL, GlassInputInfo, GlassDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 240, 4, 3
};


// Glass (Ver 1.0, Break Edition, checksum C5513F3C)

static struct BurnRomInfo glass10RomDesc[] = {
	{ "c23.bin",					0x040000, 0x688cdf33, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "c22.bin",					0x040000, 0xab17c992, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "h13.bin",					0x200000, 0x13ab7f31, 2 | BRF_GRA },           //  2 Graphics
	{ "h11.bin",					0x200000, 0xc6ac41c8, 2 | BRF_GRA },           //  3

	{ "h9.bin",						0x100000, 0xb9492557, 4 | BRF_GRA },           //  4 Blitter data

	{ "c1.bin",						0x100000, 0xd9f075a2, 5 | BRF_SND },           //  5 Samples

	{ "glass_ds5002fp_sram.bin",	0x008000, 0x47c9df4c, 6 | BRF_PRG | BRF_ESS }, //  6 Dallas MCU
};

STD_ROM_PICK(glass10)
STD_ROM_FN(glass10)

struct BurnDriver BurnDrvGlass10 = {
	"glass10", "glass", NULL, NULL, "1993",
	"Glass (Ver 1.0, Break Edition, checksum C5513F3C)\0", NULL, "OMK / Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, glass10RomInfo, glass10RomName, NULL, NULL, NULL, NULL, GlassInputInfo, GlassDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 240, 4, 3
};


// Glass (Ver 1.0, Break Edition, checksum D3864FDB)

static struct BurnRomInfo glass10aRomDesc[] = {
	{ "spl-c23.bin",				0x040000, 0xc1393bea, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "spl-c22.bin",				0x040000, 0x0d6fa33e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "h13.bin",					0x200000, 0x13ab7f31, 2 | BRF_GRA },           //  2 Graphics
	{ "h11.bin",					0x200000, 0xc6ac41c8, 2 | BRF_GRA },           //  3

	{ "h9.bin",						0x100000, 0xb9492557, 4 | BRF_GRA },           //  4 Blitter data

	{ "c1.bin",						0x100000, 0xd9f075a2, 5 | BRF_SND },           //  5 Samples

	{ "glass_ds5002fp_sram.bin",	0x008000, 0x47c9df4c, 6 | BRF_PRG | BRF_ESS }, //  6 Dallas MCU
};

STD_ROM_PICK(glass10a)
STD_ROM_FN(glass10a)

struct BurnDriver BurnDrvGlass10a = {
	"glass10a", "glass", NULL, NULL, "1993",
	"Glass (Ver 1.0, Break Edition, checksum D3864FDB)\0", NULL, "OMK / Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, glass10aRomInfo, glass10aRomName, NULL, NULL, NULL, NULL, GlassInputInfo, GlassDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 240, 4, 3
};


// Glass (Ver 1.0, Break Edition, checksum EBCB0BFE, 22 Nov 1993)

static struct BurnRomInfo glass10bRomDesc[] = {
	{ "glass 0 23 22-11 b041 27c020.bin",	0x040000, 0xd2e81d61, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "glass 1 22 22-11 90d9 27c020.bin",	0x040000, 0x8b631fb8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "al_d0-d7_27c4001.bin",				0x080000, 0xc668caad, 3 | BRF_GRA },           //  2 Graphics
	{ "bl_d8-d15_27c4001.bin",				0x080000, 0x7a6cb91f, 3 | BRF_GRA },           //  3
	{ "ah_d0-d7_27c4001.bin",				0x080000, 0xde27b785, 3 | BRF_GRA },           //  4
	{ "bh_d8-d15_27c4001.bin",				0x080000, 0xe9aa45d3, 3 | BRF_GRA },           //  5
	{ "cl_d0-d7_27c4001.bin",				0x080000, 0xe32639be, 3 | BRF_GRA },           //  6
	{ "dl_d8-d15_27c4001.bin",				0x080000, 0xb56eb8a4, 3 | BRF_GRA },           //  7
	{ "ch_d0-d7_27c4001.bin",				0x080000, 0x1b3f148d, 3 | BRF_GRA },           //  8
	{ "dh_d8-d15_27c4001.bin",				0x080000, 0xf3638123, 3 | BRF_GRA },           //  9

	{ "h9.bin",								0x100000, 0xb9492557, 4 | BRF_GRA },           // 10 Blitter data

	{ "c1.bin",								0x100000, 0xd9f075a2, 5 | BRF_SND },           // 11 Samples

	{ "glass_ds5002fp_sram.bin",			0x008000, 0x47c9df4c, 6 | BRF_PRG | BRF_ESS }, // 12 Dallas MCU
};

STD_ROM_PICK(glass10b)
STD_ROM_FN(glass10b)

struct BurnDriver BurnDrvGlass10b = {
	"glass10b", "glass", NULL, NULL, "1993",
	"Glass (Ver 1.0, Break Edition, checksum EBCB0BFE, 22 Nov 1993)\0", NULL, "OMK / Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, glass10bRomInfo, glass10bRomName, NULL, NULL, NULL, NULL, GlassInputInfo, GlassDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 240, 4, 3
};


// Glass (Ver 1.0, Break Edition, checksum 6241CD67, 16 Nov 1993)

static struct BurnRomInfo glass10cRomDesc[] = {
	{ "glass break 0 16-11 e419 27c020.bin",	0x040000, 0xdab83534, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "glass break 1 16-11 d948 27c020.bin",	0x040000, 0x35348cae, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "al_d0-d7_27c4001.bin",				0x080000, 0xc668caad, 3 | BRF_GRA },           //  2 Graphics
	{ "bl_d8-d15_27c4001.bin",				0x080000, 0x7a6cb91f, 3 | BRF_GRA },           //  3
	{ "ah_d0-d7_27c4001.bin",				0x080000, 0xde27b785, 3 | BRF_GRA },           //  4
	{ "bh_d8-d15_27c4001.bin",				0x080000, 0xe9aa45d3, 3 | BRF_GRA },           //  5
	{ "cl_d0-d7_27c4001.bin",				0x080000, 0xe32639be, 3 | BRF_GRA },           //  6
	{ "dl_d8-d15_27c4001.bin",				0x080000, 0xb56eb8a4, 3 | BRF_GRA },           //  7
	{ "ch_d0-d7_27c4001.bin",				0x080000, 0x1b3f148d, 3 | BRF_GRA },           //  8
	{ "dh_d8-d15_27c4001.bin",				0x080000, 0xf3638123, 3 | BRF_GRA },           //  9

	{ "h9.bin",								0x100000, 0xb9492557, 4 | BRF_GRA },           // 10 Blitter data

	{ "c1.bin",								0x100000, 0xd9f075a2, 5 | BRF_SND },           // 11 Samples

	{ "glass_ds5002fp_sram.bin",			0x008000, 0x47c9df4c, 6 | BRF_PRG | BRF_ESS }, // 12 Dallas MCU
};

STD_ROM_PICK(glass10c)
STD_ROM_FN(glass10c)

struct BurnDriver BurnDrvGlass10c = {
	"glass10c", "glass", NULL, NULL, "1993",
	"Glass (Ver 1.0, Break Edition, checksum 6241CD67, 16 Nov 1993)\0", NULL, "OMK / Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, glass10cRomInfo, glass10cRomName, NULL, NULL, NULL, NULL, GlassInputInfo, GlassDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 240, 4, 3
};


// Glass (Ver 1.0, Break Edition, checksum 2B43D337, 10 Nov 1993)

static struct BurnRomInfo glass10dRomDesc[] = {
	{ "glass spain 0 10-11-93 27c020.bin",	0x040000, 0x9e359418, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "glass spain 1 10-11-93 27c020.bin",	0x040000, 0x1f5ed48d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "al_d0-d7_27c4001.bin",				0x080000, 0xc668caad, 3 | BRF_GRA },           //  2 Graphics
	{ "bl_d8-d15_27c4001.bin",				0x080000, 0x7a6cb91f, 3 | BRF_GRA },           //  3
	{ "ah_d0-d7_27c4001.bin",				0x080000, 0xde27b785, 3 | BRF_GRA },           //  4
	{ "bh_d8-d15_27c4001.bin",				0x080000, 0xe9aa45d3, 3 | BRF_GRA },           //  5
	{ "cl_d0-d7_27c4001.bin",				0x080000, 0xe32639be, 3 | BRF_GRA },           //  6
	{ "dl_d8-d15_27c4001.bin",				0x080000, 0xb56eb8a4, 3 | BRF_GRA },           //  7
	{ "ch_d0-d7_27c4001.bin",				0x080000, 0x1b3f148d, 3 | BRF_GRA },           //  8
	{ "dh_d8-d15_27c4001.bin",				0x080000, 0xf3638123, 3 | BRF_GRA },           //  9

	{ "h9.bin",								0x100000, 0xb9492557, 4 | BRF_GRA },           // 10 Blitter data

	{ "c1.bin",								0x100000, 0xd9f075a2, 5 | BRF_SND },           // 11 Samples

	{ "glass_ds5002fp_sram.bin",			0x008000, 0x47c9df4c, 6 | BRF_PRG | BRF_ESS }, // 12 Dallas MCU
};

STD_ROM_PICK(glass10d)
STD_ROM_FN(glass10d)

struct BurnDriver BurnDrvGlass10d = {
	"glass10d", "glass", NULL, NULL, "1993",
	"Glass (Ver 1.0, Break Edition, checksum 2B43D337, 10 Nov 1993)\0", NULL, "OMK / Gaelco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_SHOOT, 0,
	NULL, glass10dRomInfo, glass10dRomName, NULL, NULL, NULL, NULL, GlassInputInfo, GlassDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	368, 240, 4, 3
};
