// FB Alpha Black Touch '96 driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "pic16c5x_intf.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvPicROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *Drv68KRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;
static UINT8 txt_bank;
static UINT8 soundlatch;
static UINT8 soundready;
static UINT8 port_b_data;
static UINT8 port_c_data;
static UINT8 oki_selected;
static UINT8 okibank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[3];

static struct BurnInputInfo Blackt96InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 1,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Blackt96)

static struct BurnDIPInfo Blackt96DIPList[]=
{
	{0x14, 0xff, 0xff, 0x70, NULL							},
	{0x15, 0xff, 0xff, 0xc4, NULL							},

	{0   , 0xfe, 0   ,    4, "Coin B"						},
	{0x14, 0x01, 0x03, 0x00, "1 Coin  1 Credits"			},
	{0x14, 0x01, 0x03, 0x02, "1 Coin  2 Credits"			},
	{0x14, 0x01, 0x03, 0x01, "1 Coin  3 Credits"			},
	{0x14, 0x01, 0x03, 0x03, "1 Coin  4 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin A"						},
	{0x14, 0x01, 0x0c, 0x0c, "4 Coins 1 Credits"			},
	{0x14, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"			},
	{0x14, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"			},
	{0x14, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"			},

	{0   , 0xfe, 0   ,    2, "Lives"						},
	{0x14, 0x01, 0x10, 0x00, "2"							},
	{0x14, 0x01, 0x10, 0x10, "3"							},

	{0   , 0xfe, 0   ,    2, "Bonus Life Type"				},
	{0x14, 0x01, 0x20, 0x20, "Every"						},
	{0x14, 0x01, 0x20, 0x00, "Second Only"					},
	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x14, 0x01, 0x80, 0x00, "No"							},
	{0x14, 0x01, 0x80, 0x80, "Yes"							},

	{0   , 0xfe, 0   ,    2, "Continue"						},
	{0x15, 0x01, 0x02, 0x02, "Off"							},
	{0x15, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x15, 0x01, 0x0c, 0x00, "20000 / 50000"				},
	{0x15, 0x01, 0x0c, 0x04, "60000 / 150000"				},
	{0x15, 0x01, 0x0c, 0x08, "40000 / 100000"				},
	{0x15, 0x01, 0x0c, 0x0c, "No Bonus"						},

	{0   , 0xfe, 0   ,    4, "Demo Sound / Video Freeze"	},
	{0x15, 0x01, 0x30, 0x00, "Demo Sound On"				},
	{0x15, 0x01, 0x30, 0x10, "Never Finish"					},
	{0x15, 0x01, 0x30, 0x20, "Demo Sound Off"				},
	{0x15, 0x01, 0x30, 0x30, "Stop Video"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x15, 0x01, 0xc0, 0x80, "1"							},
	{0x15, 0x01, 0xc0, 0x00, "2"							},
	{0x15, 0x01, 0xc0, 0x40, "3"							},
	{0x15, 0x01, 0xc0, 0xc0, "4"							},
};

STDDIPINFO(Blackt96)

static UINT16 __fastcall blackt96_main_read_word(UINT32 address)
{	
	switch (address)
	{
		case 0x080000:
			return (DrvInputs[0] << 8) | DrvInputs[1];

		case 0x0c0000:
			return DrvInputs[2];

		case 0x0e0000:
		case 0x0e8000:
			return rand();

		case 0x0f0000:
			return DrvDips[0] << 8;


		case 0x0f0008:
		case 0x0f0009:
			return DrvDips[1] << 8;
	}

	return 0;
}

static UINT8 __fastcall blackt96_main_read_byte(UINT32 address)
{	
	switch (address)
	{
		case 0x080000:
		case 0x080001:
			return DrvInputs[~address & 1];

		case 0x0c0000:
		case 0x0c0001:
			return DrvInputs[2];

		case 0x0e0000:
		case 0x0e0001:
		case 0x0e8000:
		case 0x0e8001:
			return rand();

		case 0x0f0000:
		case 0x0f0001:
			return DrvDips[0];


		case 0x0f0008:
		case 0x0f0009:
			return (DrvDips[1] & ~0x30) | (rand() & 0x30);
	}

	return 0;
}

static void __fastcall blackt96_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x080000:
		case 0x080001:
			soundlatch = data;
			soundready = 1;
		return;

		case 0x0c0000:
		case 0x0c0001:
			txt_bank = (data >> 4) & 7;
			flipscreen = data & 0x08;
		return;

		case 0x0f0008:
		case 0x0f0009:
			// nop
		return;
	}
}

static void __fastcall blackt96_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x080000:
			soundlatch = data & 0xff;
		return;

		case 0x0c0000:
			txt_bank = (data >> 4) & 7;
			flipscreen = data & 0x08;
		return;

		case 0x0f0008:
			// nop
		return;
	}
}

static void oki_bank(INT32 data)
{
	okibank = data & 3;
	
	MSM6295SetBank(0, DrvSndROM0 + okibank * 0x10000, 0x30000, 0x3ffff);
}

static void port_c_write(UINT8 data)
{
	if ((data & 0x20) == 0x00 && (port_c_data & 0x20))
	{
		soundready = 0;
	}

	if ((data & 0x10) == 0x00 && (port_c_data & 0x10))
	{
		port_b_data = soundlatch;
	}

	if ((data & 0x08) == 0x00 && (port_c_data & 0x08))
	{
		oki_selected = 1;
	}

	if ((data & 0x04) == 0x00 && (port_c_data & 0x04))
	{
		oki_selected = 0;
	}

	if ((data & 0x02) == 0x00 && (port_c_data & 0x02))
	{
		MSM6295Write(oki_selected, port_b_data);
	}

	if ((data & 0x01) == 0x00 && (port_c_data & 0x01))
	{
		port_b_data = MSM6295Read(oki_selected);
	}

	port_c_data = data;
}

static UINT8 blackt96_sound_readport(UINT16 port)
{
	switch (port)
	{
		case 0x01:
			return port_b_data;

		case 0x02:
			return (soundready) ? 0x40 : 0x00;
	}

	return 0;
}

static void blackt96_sound_writeport(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			oki_bank(data);
		return;

		case 0x01:
			port_b_data = data;
		return;

		case 0x02:
			port_c_write(data);
		return;
	}
}

static tilemap_callback( text )
{
	UINT16 *ram = (UINT16*)DrvVidRAM;
	UINT16 code  = ram[offs * 2 + 0] & 0xff;
	UINT16 color = ram[offs * 2 + 1];

	TILE_SET_INFO(0, code + (txt_bank * 0x100), color, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	pic16c5xReset();
	
	MSM6295Reset();

	oki_bank(0);
	
	soundlatch = 0;
	soundready = 0;
	port_b_data = 0;
	port_c_data = 0;
	oki_selected = 0;
	flipscreen = 0;
	txt_bank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x080000;
	DrvPicROM	= Next; Next += 0x002000;

	DrvGfxROM0	= Next; Next += 0x200000; // sprites will break if this size changes!
	DrvGfxROM1	= Next; Next += 0x200000;
	DrvGfxROM2	= Next; Next += 0x020000;

	MSM6295ROM	= Next;
	DrvSndROM0	= Next; Next += 0x080000;
	DrvSndROM1	= Next; Next += 0x040000;

	DrvPalette	= (UINT32*)Next; Next += 0x0801 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x004000;
	DrvSprRAM	= Next; Next += 0x008000;
	DrvPalRAM	= Next; Next += 0x001000;

	DrvVidRAM	= Next; Next += 0x001000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[8]  = { STEP8(0,1) };
	INT32 Plane1[4]  = { 0, 24, 8, 16 };
	INT32 Plane2[4]  = { STEP4(0,4) };
	INT32 XOffs0[16] = { 1024+32, 1024+40, 1024+48, 1024+56, 1024+0, 1024+8, 1024+16, 1024+24, 32,40,48,56,0,8,16,24 };
	INT32 XOffs1[16] = { 519, 515, 518, 514,  517,513,  516,512, 7,3,6,2,5,1,4,0 };
	INT32 XOffs2[8]  = { STEP4(128+3,-1), STEP4(3,-1) };
	INT32 YOffs0[16] = { STEP16(0,64) };
	INT32 YOffs1[16] = { STEP16(0,32) };
	INT32 YOffs2[8]  = { STEP8(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x200000);

	GfxDecode(0x2000, 8, 16, 16, Plane0, XOffs0, YOffs0, 0x800, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2 + 0x10000, 0x10000);

	GfxDecode(0x0800, 4,  8,  8, Plane2, XOffs2, YOffs2, 0x100, tmp, DrvGfxROM2);

	BurnFree (tmp);

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

	{
		if (BurnLoadRom(Drv68KROM  + 0x000000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000001,  1, 2)) return 1;

		if (BurnLoadRom(DrvPicROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x100000,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100001,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x000001,  8, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  9, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001, 10, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000002, 11, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000003, 12, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 13, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, 14, 2)) return 1;
		
		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvVidRAM,		0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(DrvVidRAM,		0x101000, 0x101fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x200000, 0x207fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x400000, 0x400fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xc00000, 0xC03fff, MAP_RAM);
	SekSetWriteByteHandler(0,	blackt96_main_write_byte);
	SekSetWriteWordHandler(0,	blackt96_main_write_word);
	SekSetReadByteHandler(0,	blackt96_main_read_byte);
	SekSetReadWordHandler(0,	blackt96_main_read_word);
	SekClose();

	pic16c5xInit(0, 0x16C57, DrvPicROM);
	pic16c5xSetReadPortHandler(blackt96_sound_readport);
	pic16c5xSetWritePortHandler(blackt96_sound_writeport);
	
	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295Init(1, 1000000 / 132, 0);
	MSM6295SetBank(0, DrvSndROM0, 0, 0x3ffff);
	MSM6295SetBank(1, DrvSndROM1, 0, 0x3ffff);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.47, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, text_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM2, 4, 8, 8, 0x20000, 0, 0xf);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetOffsets(0, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit();

	SekExit();
	pic16c5xExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *ram = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x1000/2; i++)
	{
		UINT8 r = (ram[i] >> 8) & 0xf;
		UINT8 g = (ram[i] >> 4) & 0xf;
		UINT8 b = (ram[i] >> 0) & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16,g+g*16,b+b*16,0);
	}
}

static void draw_sprites(INT32 group)
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;
	UINT16 *tiledata = spriteram + (0x800 * group);

	for (INT32 offs = 0; offs < 0x800; offs += 0x40)
	{
		INT32 sx = (spriteram[offs + 2*group] & 0xff) << 4;
		INT32 sy = spriteram[offs + 2*group + 1];

		sx = sx | (sy >> 12);

		sx = ((sx + 16) & 0x1ff) - 16;
		sy = -sy;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
		}

		sy -= 16;

		for (INT32 i = 0; i < 0x20; ++i)
		{
			sy &= 0x1ff;

			if (sy < nScreenHeight && (sy + 15) >= 0)
			{
				INT32 color = *(tiledata++) & 0x7f;
				INT32 code = *(tiledata++);
				INT32 flipx = code & 0x4000;
				INT32 flipy = code & 0x8000;

				if (code & 0x2000)
				{
					color &= 0x70; // 8bpp
				}
				else
				{
					color &= 0x7f; // 4bpp
				}
			
				if (flipscreen)
				{
					flipx = !flipx;
					flipy = !flipy;
				}

				Draw16x16MaskTile(pTransDraw, (code ^ 0x2000) & 0x3fff, sx, sy, flipx, flipy, color, 4, 0, 0, DrvGfxROM0);
			}
			else
			{
				tiledata += 2;
			}

			if (flipscreen)
				sy -= 16;
			else
				sy += 16;
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	BurnTransferClear(0x800);

	if (nSpriteEnable & 1) draw_sprites(2);
	if (nSpriteEnable & 2) draw_sprites(3);
	if (nSpriteEnable & 4) draw_sprites(1);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		DrvInputs[2] = 0x33;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 9000000 / 60, 8000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0, };
	
	SekOpen(0);
	
	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += SekRun(nCyclesTotal[0] / nInterleave);
		if (i == 240) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);

		nCyclesDone[1] += pic16c5xRun(nCyclesTotal[1] / nInterleave);
	}
	
	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
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
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		pic16c5xScan(nAction);
		
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(port_b_data);
		SCAN_VAR(port_c_data);
		SCAN_VAR(oki_selected);
		SCAN_VAR(okibank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(soundready);
		SCAN_VAR(flipscreen);
		SCAN_VAR(txt_bank);
	}

	if (nAction & ACB_WRITE)
	{
		oki_bank(okibank);
	}

	return 0;
}


// Black Touch '96

static struct BurnRomInfo blackt96RomDesc[] = {
	{ "3",				0x40000, 0xfc2c1d79, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "4",				0x40000, 0xcaff5b4a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "pic16c57.bin",	0x02000, 0x6053ba2f, 2 | BRF_PRG | BRF_ESS }, //  2 PIC Code (not emulated?)

	{ "1",				0x80000, 0x6a934174, 3 | BRF_SND },           //  3 MSM6295 #0 Samples

	{ "2",				0x40000, 0x94009cd4, 4 | BRF_SND },           //  4 MSM6295 #1 Samples

	{ "5",				0x40000, 0x6e52c331, 5 | BRF_GRA },           //  5 Sprite Bank 0 (8 bpp)
	{ "6",				0x40000, 0x69637a5a, 5 | BRF_GRA },           //  6
	{ "7",				0x80000, 0x6b04e8a8, 5 | BRF_GRA },           //  7
	{ "8",				0x80000, 0x16c656be, 5 | BRF_GRA },           //  8

	{ "11",				0x40000, 0x9eb773a3, 6 | BRF_GRA },           //  9 Sprite Bank 1 (4 bpp)
	{ "12",				0x40000, 0x8894e608, 6 | BRF_GRA },           // 10
	{ "13",				0x40000, 0x0acceb9d, 6 | BRF_GRA },           // 11
	{ "14",				0x40000, 0xb5e3de25, 6 | BRF_GRA },           // 12

	{ "9",				0x10000, 0x81a4cf4c, 7 | BRF_GRA },           // 13 Characters
	{ "10",				0x10000, 0xb78232a2, 7 | BRF_GRA },           // 14
};

STD_ROM_PICK(blackt96)
STD_ROM_FN(blackt96)

struct BurnDriver BurnDrvBlackt96 = {
	"blackt96", NULL, NULL, NULL, "1996",
	"Black Touch '96\0", "Game is glitchy (very poor quality)! This is normal!", "D.G.R.M.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_SCRFIGHT, 0,
	NULL, blackt96RomInfo, blackt96RomName, NULL, NULL, NULL, NULL, Blackt96InputInfo, Blackt96DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	256, 208, 4, 3
};
