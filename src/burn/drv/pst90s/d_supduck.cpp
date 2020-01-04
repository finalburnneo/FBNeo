// FB Alpha Super Duck driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvZ80RAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvForRAM;
static UINT8 *DrvBakRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;

static UINT16 *DrvScroll;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 oki_bank;
static UINT8 soundlatch;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static INT32 vblank;

static struct BurnInputInfo SupduckInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 14,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 15,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 13,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Supduck)

static struct BurnDIPInfo SupduckDIPList[]=
{
	{0x16, 0xff, 0xff, 0xff, NULL					},
	{0x17, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x16, 0x01, 0x07, 0x00, "5 Coins 1 Credits"	},
	{0x16, 0x01, 0x07, 0x01, "4 Coins 1 Credits"	},
	{0x16, 0x01, 0x07, 0x02, "3 Coins 1 Credits"	},
	{0x16, 0x01, 0x07, 0x03, "2 Coins 1 Credits"	},
	{0x16, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x16, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x16, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x16, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x16, 0x01, 0x10, 0x00, "Off"					},
	{0x16, 0x01, 0x10, 0x10, "On"					},

	{0   , 0xfe, 0   ,    2, "Game Sound"			},
	{0x16, 0x01, 0x20, 0x00, "Off"					},
	{0x16, 0x01, 0x20, 0x20, "On"					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x16, 0x01, 0xc0, 0xc0, "2"					},
	{0x16, 0x01, 0xc0, 0x80, "3"					},
	{0x16, 0x01, 0xc0, 0x40, "4"					},
	{0x16, 0x01, 0xc0, 0x00, "5"					},

	{0   , 0xfe, 0   ,    2, "Character Test"		},
	{0x17, 0x01, 0x40, 0x40, "Off"					},
	{0x17, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x17, 0x01, 0x80, 0x80, "Off"					},
	{0x17, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Supduck)

static void __fastcall supduck_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0xfe4000:
		case 0xfe4001:
		return; // nop

		case 0xfe4002:
		case 0xfe4003:
			soundlatch = data >> 8;
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		return;

		case 0xfe8000:
		case 0xfe8001:
		case 0xfe8002:
		case 0xfe8003:
		case 0xfe8004:
		case 0xfe8005:
		case 0xfe8006:
		case 0xfe8007:
			DrvScroll[(address/2) & 3] = data;
		return;

		case 0xfe800e:
		case 0xfe800f:
			// watchdog?
		return;
	}
}

static void __fastcall supduck_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0xfe4000:
		case 0xfe4001:
		return; // nop

		case 0xfe4002:
		case 0xfe4003:
			soundlatch = data;
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		return;

		case 0xfe8000:
		case 0xfe8001:
		case 0xfe8002:
		case 0xfe8003:
		case 0xfe8004:
		case 0xfe8005:
		case 0xfe8006:
		case 0xfe8007:
			DrvScroll[(address/2) & 3] = data;
		return;

		case 0xfe800e:
		case 0xfe800f:
			// watchdog?
		return;
	}
}

static UINT16 __fastcall supduck_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xfe4000:
			return DrvInputs[0];

		case 0xfe4002:
			return (DrvInputs[1] & ~0x0400) | (vblank ? 0 : 0x0400);

		case 0xfe4004:
			return (DrvDips[1] * 256) + DrvDips[0];
	}

	return 0;
}

static UINT8 __fastcall supduck_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xfe4000:
			return DrvInputs[0] >> 8;

		case 0xfe4001:
			return DrvInputs[0];

		case 0xfe4002:
			return ((DrvInputs[1] >> 8) & ~0x04) | (vblank ? 0 : 0x04);

		case 0xfe4003:
			return 0xff;

		case 0xfe4004:
			return DrvDips[1];
			
		case 0xfe4005:
			return DrvDips[0];
	}

	return 0;
}

static void __fastcall supduck_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9000:
			oki_bank = data;
			MSM6295SetBank(0, MSM6295ROM + 0x20000 + (data & 3) * 0x20000, 0x20000, 0x3ffff);
		return;

		case 0x9800:
			MSM6295Write(0, data);
		return;
	}
}

static UINT8 __fastcall supduck_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x9800:
			return MSM6295Read(0);

		case 0xa000:
			return soundlatch;
	}

	return 0;
}

static tilemap_scan( supduck )
{
	return (((row ^ 0x38) / 8) * 0x400) + ((col / 8) * 64) + ((~row & 7) * 8) + (col & 7);
}

static tilemap_callback( text )
{
	UINT16 attr = *((UINT16*)(DrvTxtRAM + offs * 2));
	UINT16 code = (attr & 0xff) | ((attr & 0xc000) >> 6) | ((attr & 0x2000) >> 3);

	TILE_SET_INFO(0, code, (attr >> 8) & 0xf, TILE_FLIPXY((attr >> 12) & 1));
}

static tilemap_callback( fore )
{
	UINT16 attr = *((UINT16*)(DrvForRAM + offs * 2));
	UINT16 code = (attr & 0xff) | ((attr & 0xc000) >> 6);

	TILE_SET_INFO(1, code, (attr >> 8) & 0xf, TILE_FLIPXY(attr >> 12));
}

static tilemap_callback( back )
{
	UINT16 attr = *((UINT16*)(DrvBakRAM + offs * 2));
	UINT16 code = (attr & 0xff) | ((attr & 0xc000) >> 6);

	TILE_SET_INFO(2, code, (attr >> 8) & 0xf, TILE_FLIPXY(attr >> 12));
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	MSM6295Reset(0);
	supduck_sound_write(0x9000, 0);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x040000;
	DrvZ80ROM	= Next; Next += 0x008000;

	DrvGfxROM0	= Next; Next += 0x020000;
	DrvGfxROM1	= Next; Next += 0x100000;
	DrvGfxROM2	= Next; Next += 0x100000;
	DrvGfxROM3	= Next; Next += 0x100000;

	MSM6295ROM	= Next;
	DrvSndROM	= Next; Next += 0x0a0000;

	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x004000;
	DrvPalRAM	= Next; Next += 0x000800;
	DrvBakRAM	= Next; Next += 0x004000;
	DrvForRAM	= Next; Next += 0x004000;
	DrvTxtRAM	= Next; Next += 0x001000;
	DrvSprRAM	= Next; Next += 0x002000;
	DrvSprBuf	= Next; Next += 0x002000;

	DrvZ80RAM	= Next; Next += 0x000800;

	DrvScroll	= (UINT16*)Next; Next += 0x000004 * sizeof(UINT16);

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0x40000*8+4, 0x40000*8+0, 4, 0 };
	INT32 XOffs0[32] = { STEP4(0,1), STEP4(8,1), STEP4(64*8,1), STEP4(64*8+8,1), STEP4(2*64*8,1), STEP4(2*64*8+8,1), STEP4(3*64*8,1), STEP4(3*64*8+8,1) };
	INT32 YOffs0[32] = { STEP32(0,16) };

	INT32 Plane1[4]  = { 0x20000*8*3, 0x20000*8*2, 0x20000*8*1, 0x20000*8*0 };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8(16*8, 1) };
	INT32 YOffs1[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return;
	}
	
	memcpy (tmp, DrvGfxROM0, 0x008000);

	GfxDecode(0x0800, 2,  8,  8, Plane0 + 2, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x080000);

	GfxDecode(0x0400, 4, 32, 32, Plane0 + 0, XOffs0, YOffs0, 0x800, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x080000);

	GfxDecode(0x0400, 4, 32, 32, Plane0 + 0, XOffs0, YOffs0, 0x800, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x080000);

	GfxDecode(0x1000, 4, 16, 16, Plane1 + 0, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM3);

	BurnFree (tmp);
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
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x060000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x020000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x040000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x060000, 11, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x060000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x040000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x020000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x000000, 15, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 16, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x020000, 17, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,		0xfe0000, 0xfe1fff, MAP_RAM);
	SekMapMemory(DrvTxtRAM,		0xfec000, 0xfecfff, MAP_RAM);
	SekMapMemory(DrvBakRAM,		0xff0000, 0xff3fff, MAP_RAM);
	SekMapMemory(DrvForRAM,		0xff4000, 0xff7fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0xff8000, 0xff87ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xffc000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,	supduck_main_write_word);
	SekSetWriteByteHandler(0,	supduck_main_write_byte);
	SekSetReadWordHandler(0,	supduck_main_read_word);
	SekSetReadByteHandler(0,	supduck_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(supduck_sound_write);
	ZetSetReadHandler(supduck_sound_read);
	ZetClose();

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, text_map_callback, 8,   8,  32, 32);
	GenericTilemapInit(1, supduck_map_scan,  fore_map_callback, 32, 32, 128, 64);
	GenericTilemapInit(2, supduck_map_scan,  back_map_callback, 32, 32, 128, 64);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2,  8,  8, 0x020000, 0x300, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 32, 32, 0x100000, 0x000, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM2, 4, 32, 32, 0x100000, 0x100, 0x0f);
	GenericTilemapSetTransparent(0, 0x03);
	GenericTilemapSetTransparent(1, 0x0f);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	MSM6295Exit(0);
	SekExit();
	ZetExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *ram = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x800/2; i++)
	{
		UINT16 p = ram[i];

		UINT8 r = (p >>  8) & 0xf;
		UINT8 g = (p >>  4) & 0xf;
		UINT8 b = (p >>  0) & 0xf;

		r |= (p >> 10) & 0x10;
		g |= (p >>  9) & 0x10;
		b |= (p >>  8) & 0x10;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprBuf;

	for (INT32 i = 0x1000 - 4; i >= 0; i -= 4)
	{
		INT32 code = ram[i + 0];
		INT32 attr = ram[i + 1];
		INT32 sy   = ram[i + 2] & 0x1ff;
		INT32 sx   = ram[i + 3] & 0x1ff;

		INT32 flipx = attr & 0x02;
		INT32 flipy = attr & 0x01;
		INT32 color = (attr >> 2) & 0x0f;

		if (sx > 0x100) sx -= 0x200;
		if (sy > 0x100) sy -= 0x200;

		Draw16x16MaskTile(pTransDraw, code, sx, (240 - sy) - 16, flipx, flipy, color, 4, 0xf, 0x200, DrvGfxROM3);
	}
}

static INT32 DrvDraw()
{
	DrvPaletteUpdate();

	GenericTilemapSetScrollX(2, DrvScroll[0]);
	GenericTilemapSetScrollY(2, -DrvScroll[1] - 256);
	GenericTilemapSetScrollX(1, DrvScroll[2]);
	GenericTilemapSetScrollY(1, -DrvScroll[3] - 256);
	
	if ((nBurnLayer & 4) == 0) BurnTransferClear();

	if ((nBurnLayer & 4) == 4) GenericTilemapDraw(2, pTransDraw, 0);
	if ((nBurnLayer & 2) == 2) GenericTilemapDraw(1, pTransDraw, 0);

	if ((nSpriteEnable & 1) == 1) draw_sprites();

	if ((nBurnLayer & 1) == 1) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 2 * sizeof(UINT16));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 8000000 / 60, 2000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	
	SekOpen(0);
	ZetOpen(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += SekRun((nCyclesTotal[0] * (i + 1) / nInterleave) - nCyclesDone[0]);
		if (i == 240) {
			if (pBurnDraw) {
				DrvDraw();
			}

			memcpy (DrvSprBuf, DrvSprRAM, 0x2000);

			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
			vblank = 1;
		}

		nCyclesDone[1] += ZetRun((nCyclesTotal[1] * (i + 1) / nInterleave) - nCyclesDone[1]);
	}

	ZetClose();
	SekClose();

	if (pBurnSoundOut) {
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
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
		ZetScan(nAction);

		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(oki_bank);
		SCAN_VAR(soundlatch);
	}

	if (nAction & ACB_WRITE) {
		supduck_sound_write(0x9000, oki_bank);
	}

	return 0;
}


// Super Duck

static struct BurnRomInfo supduckRomDesc[] = {
	{ "5.u16n",		0x20000, 0x837a559a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "6.u16l",		0x20000, 0x508e9905, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "4.su6",		0x08000, 0xd75863ea, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "3.cu15",		0x08000, 0xb1cacca4, 3 | BRF_GRA },           //  3 Characters

	{ "7.uu29",		0x20000, 0xf3251b20, 4 | BRF_GRA },           //  4 Foreground Tiles
	{ "8.uu30",		0x20000, 0x03c60cbd, 4 | BRF_GRA },           //  5
	{ "9.uu31",		0x20000, 0x9b6d3430, 4 | BRF_GRA },           //  6
	{ "10.uu32",	0x20000, 0xbeed2616, 4 | BRF_GRA },           //  7

	{ "11.ul29",	0x20000, 0x1b6958a4, 5 | BRF_GRA },           //  8 Background Tiles
	{ "12.ul30",	0x20000, 0x3e6bd24b, 5 | BRF_GRA },           //  9
	{ "13.ul31",	0x20000, 0xbff7b7cd, 5 | BRF_GRA },           // 10
	{ "14.ul32",	0x20000, 0x97a7310b, 5 | BRF_GRA },           // 11

	{ "15.u1d",		0x20000, 0x81bf1f27, 6 | BRF_GRA },           // 12 Sprites
	{ "16.u2d",		0x20000, 0x9573d6ec, 6 | BRF_GRA },           // 13
	{ "17.u1c",		0x20000, 0x21ef14d4, 6 | BRF_GRA },           // 14
	{ "18.u2c",		0x20000, 0x33dd0674, 6 | BRF_GRA },           // 15

	{ "2.su12",		0x20000, 0x745d42fb, 7 | BRF_SND },           // 16 Samples
	{ "1.su13",		0x80000, 0x7fb1ed42, 7 | BRF_SND },           // 17
};

STD_ROM_PICK(supduck)
STD_ROM_FN(supduck)

struct BurnDriver BurnDrvSupduck = {
	"supduck", NULL, NULL, NULL, "1992",
	"Super Duck\0", NULL, "Comad", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, supduckRomInfo, supduckRomName, NULL, NULL, NULL, NULL, SupduckInputInfo, SupduckDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};
