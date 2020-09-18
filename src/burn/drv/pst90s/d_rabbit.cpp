// FinalBurn Neo Electronic Arts Rabbit driver module
// Based on MAME driver by David Haywood

// to do:
//	sound - add volume adjust, etc

// notes:
//	graphics banking for the main cpu is only used to test the rom checksums
//	-this adds complexity and is not worth the time investment
//  -banking in first 512k of graphics0 to keep the logger happy on reads
//
//	possible mame over-flow bugs
//	- blitter tilemap select can select 7, only has 4 tilemaps
//	- blitter column select can be 0xff, later masks to 0x7f
//	- sprite start offset can be 0xfff*2 for sprites (32-bit), this is 1ffe*4 (0x7ff8) ram is only 0x4000

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "i5000.h"
#include "eeprom.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM[10];
static UINT8 *DrvSndROM;
static UINT8 *DrvEEPROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM[4];
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT32 *tilemapregs[4];
static UINT32 *spriteregs;
static UINT32 *blitterregs;
static INT32 blitter_irq;

static INT32 update_tilemap[4];

static UINT8 DrvJoy1[32];
static UINT32 DrvInputs[1];
static UINT8 DrvReset;

static struct BurnInputInfo RabbitInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 15,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 16,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 17,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 18,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 19,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 20,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 21,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 22,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 23,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 4,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 5,	"diag"		},
};

STDINPUTINFO(Rabbit)

static void blitter_write()
{	
	UINT32 reg0 = (blitterregs[0]<<16)|(blitterregs[0]>>16);
	UINT32 reg1 = (blitterregs[1]<<16)|(blitterregs[1]>>16);
	UINT32 reg2 = (blitterregs[2]<<16)|(blitterregs[2]>>16);

	INT32 blt_source = (reg0&0x000fffff) >>  0;
	INT32 blt_column = (reg1&0x00ff0000) >> 16; // should this be 0x7f?
	INT32 blt_line  =  (reg1&0x000000ff) >>  0;
	INT32 blt_tilemp = (reg2&0x00006000) >> 13; // MAME has e...
	INT32 blt_oddflg = (reg2&0x00000001) >>  0;
	INT32 mask,shift;
	UINT32 *tilemap_ram[4] = { (UINT32*)DrvVidRAM[0], (UINT32*)DrvVidRAM[1], (UINT32*)DrvVidRAM[2], (UINT32*)DrvVidRAM[3] };

	if (!blt_oddflg) // inverted vs MAME
	{
		mask = 0xffff0000;
		shift= 0;
	}
	else
	{
		mask = 0x0000ffff;
		shift= 16;
	}

	blt_oddflg = 0x80 * blt_line;
	blt_source <<= 1;

	while (1)
	{
		INT32 blt_amount = DrvGfxROM[0][blt_source++];
		INT32 blt_commnd = DrvGfxROM[0][blt_source++];
		
		switch (blt_commnd)
		{
			case 0x00: // copy nn bytes
			{
				if (!blt_amount)
				{
					blitter_irq = 1; //	irq line 4 after 500us
					return;
				}

				for (INT32 loopcount = 0; loopcount < blt_amount; loopcount++)
				{
					INT32 blt_value = ((DrvGfxROM[0][blt_source^1]<<8)|(DrvGfxROM[0][blt_source^0]));
					blt_source+=2;
					INT32 writeoffs=blt_oddflg+blt_column;
					tilemap_ram[blt_tilemp][writeoffs]= BURN_ENDIAN_SWAP_INT32((BURN_ENDIAN_SWAP_INT32(tilemap_ram[blt_tilemp][writeoffs])&mask)|(blt_value<<shift));
					GenericTilemapSetTileDirty(blt_tilemp, writeoffs);
					update_tilemap[blt_tilemp] = 1;
					blt_column++;
					blt_column&=0x7f;
				}
			}
			break;

			case 0x02: // fill nn bytes
			{
				INT32 blt_value = ((DrvGfxROM[0][blt_source^1]<<8)|(DrvGfxROM[0][blt_source^0]));
				blt_source+=2;

				for (INT32 loopcount = 0; loopcount < blt_amount; loopcount++)
				{
					INT32 writeoffs=blt_oddflg+blt_column;

					tilemap_ram[blt_tilemp][writeoffs]= BURN_ENDIAN_SWAP_INT32((BURN_ENDIAN_SWAP_INT32(tilemap_ram[blt_tilemp][writeoffs])&mask)|(blt_value<<shift));
					GenericTilemapSetTileDirty(blt_tilemp, writeoffs);
					update_tilemap[blt_tilemp] = 1;
					blt_column++;
					blt_column&=0x7f;
				}
			}
			break;

			case 0x03: // next line
				blt_column = (reg1&0x00ff0000)>>16;
				blt_oddflg+=128;
				break;
				
			default:
				bprintf (0, _T("BLIT ERROR! %x. %x\n"), blt_commnd, blt_amount);
		}
	}
}

static void __fastcall rabbit_write_long(UINT32 address, UINT32 data)
{
	data = (data << 16) | (data >> 16);

	if ((address & 0xffffe0) == 0x400200) {
		spriteregs[(address / 4) & 7] = BURN_ENDIAN_SWAP_INT32(data);
		return;
	}

	if ((address & 0xfffff0) == 0x400700) {
		blitterregs[(address / 4) & 3] = data;
		return;
	}

//	bprintf (0, _T("WL: %5.5x, %8.8x\n"), address, data);
}

static void __fastcall rabbit_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffff80) == 0x400100) {
		UINT16 *regs = (UINT16*)tilemapregs[(address / 0x20) & 3];
		regs[(address / 2) & 0xf] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if ((address & 0xffffe0) == 0x400200) {
		UINT16 *regs = (UINT16*)spriteregs;
		regs[(address / 2) & 0xf] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if ((address & 0xfffffc) == 0x400300) {
		// rombank_w
		return;
	}

	if (address == 0x40070e) {
		blitter_write();
		return;
	}

	if ((address & 0xffff00) == 0x400900) {
		i5000sndWrite((address / 2) & 0x7f, data);
		return;
	}

//	bprintf (0, _T("WW: %5.5x, %2.2x\n"), address, data);
}

static void __fastcall rabbit_write_byte(UINT32 address, UINT8 data)
{
	if (address == 0x200000) {
		EEPROMWriteBit((data & 0x01));
		EEPROMSetCSLine((~data & 0x04) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		EEPROMSetClockLine((data & 0x02) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;
	}

//	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static UINT32 __fastcall rabbit_read_long(UINT32 address)
{
	if (address == 0x200000) return DrvInputs[0] | (EEPROMRead() ? 1 : 0);

//	bprintf (0, _T("RL: %5.5x\n"), address & 0xff);

	return 0;
}

static UINT16 __fastcall rabbit_read_word(UINT32 address)
{
	if (address == 0x400012) return BurnRandom();

	if ((address & 0xffff00) == 0x400900) return i5000sndRead((address / 2) & 0x7f);

//	bprintf (0, _T("RW: %8.8x\n"), address);

	return 0;
}

static UINT8 __fastcall rabbit_read_byte(UINT32 address)
{
	if (address == 0x200003) return (DrvInputs[0] & 0xfe) | (EEPROMRead() ? 1 : 0); // only this one ever read?

//	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static void __fastcall rabbit_videoram_write_long(UINT32 address, UINT32 data)
{
	data = (data << 16) | (data >> 16);

	INT32 offset = (address & 0x3ffc) / 4;
	INT32 select = (address / 0x4000) & 3;
	UINT32 *ram = (UINT32*)(DrvVidRAM[select]);
	if (BURN_ENDIAN_SWAP_INT32(ram[offset]) != data) {
		GenericTilemapSetTileDirty(select, offset);
		update_tilemap[select] = 1;
		ram[offset] = BURN_ENDIAN_SWAP_INT32(data);
	}
}

static void __fastcall rabbit_videoram_write_word(UINT32 address, UINT16 data)
{
	INT32 offset = (address & 0x3ffe) / 2;
	INT32 select = (address / 0x4000) & 3;
	UINT16 *ram = (UINT16*)(DrvVidRAM[select]);
	if (BURN_ENDIAN_SWAP_INT16(ram[offset]) != data) {
		GenericTilemapSetTileDirty(select, offset/2);
		update_tilemap[select] = 1;
		ram[offset] = BURN_ENDIAN_SWAP_INT16(data);
	}
}

static void __fastcall rabbit_videoram_write_byte(UINT32 address, UINT8 data)
{
	INT32 offset = (address & 0x3fff) ^ 1;
	INT32 select = (address / 0x4000) & 3;
	UINT8 *ram = (UINT8*)(DrvVidRAM[select]);
	if (ram[offset] != data) {
		GenericTilemapSetTileDirty(select, offset/4);
		update_tilemap[select] = 1;
		ram[offset] = data;
	}
}

static inline void palette_write(UINT16 i)
{
	DrvPalette[i/4] = BurnHighCol(DrvPalRAM[i+3],DrvPalRAM[i+0],DrvPalRAM[i+2],0);
}

static void __fastcall rabbit_paletteram_write_long(UINT32 address, UINT32 data)
{
	UINT32 *ram = (UINT32*)DrvPalRAM;
	ram[(address & 0xfffc)/4] = BURN_ENDIAN_SWAP_INT32((data << 16) | (data >> 16));
	palette_write(address & 0xfffc);
}

static void __fastcall rabbit_paletteram_write_word(UINT32 address, UINT16 data)
{
	UINT16 *ram = (UINT16*)DrvPalRAM;
	ram[(address & 0xfffe)/2] = BURN_ENDIAN_SWAP_INT16(data);
	palette_write(address & 0xfffc);
}

static void __fastcall rabbit_paletteram_write_byte(UINT32 address, UINT8 data)
{
	UINT8 *ram = (UINT8*)DrvPalRAM;
	ram[(address & 0xffff) ^ 1] = data;
	palette_write(address & 0xfffc);
}

#define	get_tilemap_info(which, size)									\
	UINT32 attr = BURN_ENDIAN_SWAP_INT32(*((UINT32*)(DrvVidRAM[which] + (offs * 4))));			\
	INT32 depth = (attr & 0x00001000) >> 12;							\
	INT32 code  = (attr & 0xffff0000) >> 16;							\
	INT32 bank  = (attr & 0x0000000f) >>  0;							\
	INT32 color = (attr & 0x00000ff0) >>  4;							\
	INT32 cmask = (attr & 0x00008000) >> 15;							\
	INT32 flip  = (attr & 0x00006000) >> 13;							\
																		\
	if (bank == 8) code += 0x10000;										\
	if (bank == 12) code += 0x20000;									\
																		\
	if (depth) {														\
		code >>= ((size*2) + 1);										\
	} else {															\
		code >>= size * 2;												\
		if (cmask) color &= 0x3f;										\
	}																	\
	TILE_SET_INFO((depth * 2) + size, code, color, TILE_FLIPXY(flip))

static tilemap_callback( layer0 )
{
	get_tilemap_info(0,1);
}

static tilemap_callback( layer1 )
{
	get_tilemap_info(1,1);
}

static tilemap_callback( layer2 )
{
	get_tilemap_info(2,1);
}

static tilemap_callback( layer3 )
{
	get_tilemap_info(3,0);
}

static INT32 DrvDoReset()
{
	SekOpen(0);
	SekReset();
	SekClose();

	i5000sndReset();

	BurnRandomSetSeed(0xb00b1e5);

	EEPROMReset();
	if (EEPROMAvailable() == 0) {
		EEPROMFill(DrvEEPROM, 0, 0x80);
	}

	blitter_irq = 0;
	for (INT32 i = 0; i < 4; i++) {
		GenericTilemapAllTilesDirty(i);	
		update_tilemap[i] = 1;
	}

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x0200000;

	DrvGfxROM[0]	= Next; Next += 0x0200000; // blitter data
	DrvGfxROM[1]	= Next; Next += 0x0600000; // 8x8x8 / 8x16x16 tiles
	DrvGfxROM[2]	= Next; Next += 0x2000000; // 4x16x16 sprites
	DrvGfxROM[3]	= Next; Next += 0x0c00000; // 4x8x8 / 4x16x16 tiles

	DrvSndROM		= Next; Next += 0x0400018; // sound data + 24 byte header

	DrvEEPROM		= Next; Next += 0x0000080;

	DrvPalette		= (UINT32*)Next; Next += 0x04001 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x0100000;
	DrvPalRAM		= Next; Next += 0x0100000;
	DrvVidRAM[0]	= Next; Next += 0x0200000; // only first 4000 accessible by cpu
	DrvVidRAM[1]	= Next; Next += 0x0200000; // ""
	DrvVidRAM[2]	= Next; Next += 0x0200000; // ""
	DrvVidRAM[3]	= Next; Next += 0x0200000; // ""
	DrvSprRAM		= Next; Next += 0x0040000;

	tilemapregs[0]	= (UINT32*)Next; Next += 0x000008 * sizeof(UINT32);
	tilemapregs[1]	= (UINT32*)Next; Next += 0x000008 * sizeof(UINT32);
	tilemapregs[2]	= (UINT32*)Next; Next += 0x000008 * sizeof(UINT32);
	tilemapregs[3]	= (UINT32*)Next; Next += 0x000008 * sizeof(UINT32);

	blitterregs		= (UINT32*)Next; Next += 0x000004 * sizeof(UINT32);
	spriteregs		= (UINT32*)Next; Next += 0x000008 * sizeof(UINT32);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;

		if (BurnLoadRom(Drv68KROM + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000003, k++, 4)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000002, k++, 4)) return 1;

		if (BurnLoadRomExt(DrvGfxROM[2] + 0x000000, k++, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x000002, k++, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x000004, k++, 8, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvGfxROM[2] + 0x000006, k++, 8, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x200000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x400000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM + 0x000000, k++, 1)) return 1;
		DrvSndROM += 0x18; // skip header
		
		if (BurnLoadRom(DrvEEPROM + 0x000000, k++, 1)) return 1;

		for (INT32 i = 0; i < 0x200000; i++) { // copy blitter data out of sprite graphics
			DrvGfxROM[0][i] = DrvGfxROM[2][((i & 3) ^ 2) | ((i & 0x1ffffc) << 1)];
		}

		// expand into 4bpp tiles
		BurnNibbleExpand(DrvGfxROM[1], DrvGfxROM[3], 0x0600000, 1, 0);
		BurnNibbleExpand(DrvGfxROM[2], DrvGfxROM[2], 0x1000000, 1, 0);
	}

	SekInit(0, 0x68ec020);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(DrvGfxROM[0],		0x440000, 0x47ffff, MAP_ROM); // bank graphics (don't bother)
	SekMapMemory(DrvVidRAM[0],		0x480000, 0x483fff, MAP_RAM);
	SekMapMemory(DrvVidRAM[1],		0x484000, 0x487fff, MAP_RAM);
	SekMapMemory(DrvVidRAM[2],		0x488000, 0x48bfff, MAP_RAM);
	SekMapMemory(DrvVidRAM[3],		0x48c000, 0x48ffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,			0x494000, 0x497fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x4a0000, 0x4affff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xff0000, 0xffffff, MAP_RAM);
	SekSetWriteLongHandler(0,		rabbit_write_long);
	SekSetWriteWordHandler(0,		rabbit_write_word);
	SekSetWriteByteHandler(0,		rabbit_write_byte);
	SekSetReadLongHandler(0,		rabbit_read_long);
	SekSetReadWordHandler(0,		rabbit_read_word);
	SekSetReadByteHandler(0,		rabbit_read_byte);

	SekMapHandler(1,				0x480000, 0x48ffff, MAP_WRITE);
	SekSetWriteLongHandler(1,		rabbit_videoram_write_long);
	SekSetWriteWordHandler(1,		rabbit_videoram_write_word);
	SekSetWriteByteHandler(1,		rabbit_videoram_write_byte);

	SekMapHandler(2,				0x4a0000, 0x4affff, MAP_WRITE);
	SekSetWriteLongHandler(2,		rabbit_paletteram_write_long);
	SekSetWriteWordHandler(2,		rabbit_paletteram_write_word);
	SekSetWriteByteHandler(2,		rabbit_paletteram_write_byte);
	SekClose();

	EEPROMInit(&eeprom_interface_93C46);

	i5000sndInit(DrvSndROM, 40000000, 0x400000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, layer0_map_callback, 16, 16, 128, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, layer1_map_callback, 16, 16, 128, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, layer2_map_callback, 16, 16, 128, 32);
	GenericTilemapInit(3, TILEMAP_SCAN_ROWS, layer3_map_callback,  8,  8, 128, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[3], 4,  8,  8, 0x0c00000, 0x2000, 0xff);
	GenericTilemapSetGfx(1, DrvGfxROM[3], 4, 16, 16, 0x0c00000, 0x2000, 0xff);
	GenericTilemapSetGfx(2, DrvGfxROM[1], 8,  8,  8, 0x0600000, 0x6000, 0x0f); // color offset | 0x4000 (masked in bitmap copy)
	GenericTilemapSetGfx(3, DrvGfxROM[1], 8, 16, 16, 0x0600000, 0x6000, 0x0f); // color offset | 0x4000 (masked in bitmap copy)
	GenericTilemapSetGfx(4, DrvGfxROM[2], 4, 16, 16, 0x2000000, 0x0000, 0xff); // sprites
	GenericTilemapUseDirtyTiles(0);
	GenericTilemapUseDirtyTiles(1);
	GenericTilemapUseDirtyTiles(2);
	GenericTilemapUseDirtyTiles(3);

	BurnBitmapAllocate(1, 2048,  512, true);
	BurnBitmapAllocate(2, 2048,  512, true);
	BurnBitmapAllocate(3, 2048,  512, true);
	BurnBitmapAllocate(4, 1024,  256, true);
	BurnBitmapAllocate(5, 4096, 4096, true);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	EEPROMExit();
	i5000sndExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x10000; i+=4) {
		DrvPalette[i/4] = BurnHighCol(DrvPalRAM[i+3],DrvPalRAM[i+0],DrvPalRAM[i+2],0);
	}
	
	DrvPalette[0x4000] = 0; // black
}

static void clearspritebitmap()
{
	INT32 starty = (BURN_ENDIAN_SWAP_INT32(spriteregs[1])&0x00000fff) - 200;
	INT32 startx =((BURN_ENDIAN_SWAP_INT32(spriteregs[0])&0x0fff0000)>>16) - 200;
	INT32 amountx = 650;
	INT32 amounty = 600;

	if (startx < 0) { amountx += startx; startx = 0; }
	if ((startx+amountx)>=0x1000) amountx-=(0x1000-(startx+amountx));

	for (INT32 y=0; y<amounty;y++)
	{
		UINT16 *dstline = BurnBitmapGetPosition(5, 0, (starty+y) & 0xfff);
		memset(dstline+startx,0x00,amountx*2);
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;
	
	for (INT32 offs = (((BURN_ENDIAN_SWAP_INT32(spriteregs[5]) & 0x0fff) - 1) * 4); offs >= 0; offs -= 4)
	{
		INT32 sy    = BURN_ENDIAN_SWAP_INT16(ram[offs + 0]) & 0x0fff;
		INT32 sx    = BURN_ENDIAN_SWAP_INT16(ram[offs + 1]) & 0x0fff;
		INT32 flipx = BURN_ENDIAN_SWAP_INT16(ram[offs + 1]) & 0x8000;
		INT32 flipy = BURN_ENDIAN_SWAP_INT16(ram[offs + 1]) & 0x4000;
		INT32 color =(BURN_ENDIAN_SWAP_INT16(ram[offs + 2]) >> 4) & 0xff;
		INT32 code  =(BURN_ENDIAN_SWAP_INT16(ram[offs + 3]) | (BURN_ENDIAN_SWAP_INT16(ram[offs + 2]) << 16)) & 0x1ffff;

		if (sx & 0x800) sx-=0x1000;

		DrawGfxMaskTile(5, 4, code, sx+0x20-8, sy-24, flipx, flipy, color, 0xf);
	}
}

static void draw_sprite_bitmap()
{
	INT32 startx = ((BURN_ENDIAN_SWAP_INT32(spriteregs[0])>>16)&0x00000fff) - (((BURN_ENDIAN_SWAP_INT32(spriteregs[1])>>16)&0x000001ff)>>1);
	INT32 starty = ((BURN_ENDIAN_SWAP_INT32(spriteregs[1])&0x0fff)) - (((BURN_ENDIAN_SWAP_INT32(spriteregs[1])>>16)&0x000001ff)>>1);
	UINT32 xsize = ((BURN_ENDIAN_SWAP_INT32(spriteregs[2])>>16)) + 0x80;
	UINT32 ysize = ((BURN_ENDIAN_SWAP_INT32(spriteregs[3])>>16)) + 0x80;
	UINT32 xstep = ((320*128)<<16) / xsize;
	UINT32 ystep = ((224*128)<<16) / ysize;

	for (UINT32 y=0;y<ysize;y+=0x80)
	{
		UINT32 ydrawpos = ((y>>7)*ystep) >> 16;

		if (ydrawpos < (UINT32)nScreenHeight)
		{
			UINT16 *srcline = BurnBitmapGetPosition(5, 0, (starty+(y>>7))&0xfff);
			UINT16 *dstline = BurnBitmapGetPosition(0, 0, ydrawpos);

			for (UINT32 x=0;x<xsize;x+=0x80)
			{
				UINT32 xdrawpos = ((x>>7)*xstep) >> 16;
				UINT16 pixdata = srcline[(startx+(x>>7))&0xfff];
				//if (pixdata) { fprintf(fp, "pixdata: 0x%x \n", pixdata); fflush(fp); }
				if (pixdata)
					if (xdrawpos < (UINT32)nScreenWidth)
						dstline[xdrawpos] = pixdata;
			}
		}
	}
}

static void drawtilemap(INT32 whichtilemap) // line zoom
{
	if ((nBurnLayer & (1 << whichtilemap)) == 0) return;

	UINT16 *dst = pTransDraw;

	INT32 minx, maxx, miny, maxy;
	BurnBitmapGetClipDims(whichtilemap + 1, &minx, &maxx, &miny, &maxy);
	UINT16 *src = BurnBitmapGetBitmap(1 + whichtilemap);

	INT32 width = maxx - minx;
	INT32 height = maxy - miny;
	INT32 wmask = width - 1;
	INT32 hmask = height - 1;

	INT32 starty=((BURN_ENDIAN_SWAP_INT32(tilemapregs[whichtilemap][1])&0x0000ffff)) << 12;
	INT32 startx=((BURN_ENDIAN_SWAP_INT32(tilemapregs[whichtilemap][1])&0xffff0000)>>16) << 12;
	INT32 incyy= ((BURN_ENDIAN_SWAP_INT32(tilemapregs[whichtilemap][4])&0x00000fff)) << 5;
	INT32 incxx= ((BURN_ENDIAN_SWAP_INT32(tilemapregs[whichtilemap][3])&0x0fff0000)>>16) << 5;

	if (update_tilemap[whichtilemap])
	{
		GenericTilemapDraw(whichtilemap, 1 + whichtilemap, 0);
		update_tilemap[whichtilemap] = 0;
	}

	for (INT32 sy = 0; sy < nScreenHeight; sy++, starty+=incyy)
	{
		UINT32 cx = startx;
		UINT16 *line = src + ((starty >> 16) & hmask) * width;

		for (INT32 x = 0; x < nScreenWidth; x++, cx+=incxx, dst++)
		{
			INT32 pxl = line[(cx >> 16) & wmask];

			if ((pxl & 0x40ff) != 0x40ff) {
				if ((pxl & 0x400f) != 0x000f) {
					*dst = pxl & 0x3fff;
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	BurnTransferClear(0x4000);

	for (UINT32 prilevel = 0xf00; prilevel > 0; prilevel-=0x100)
	{
		if (prilevel == (BURN_ENDIAN_SWAP_INT32(tilemapregs[3][0]) & 0x0f00)) drawtilemap(3);
		if (prilevel == (BURN_ENDIAN_SWAP_INT32(tilemapregs[2][0]) & 0x0f00)) drawtilemap(2);
		if (prilevel == (BURN_ENDIAN_SWAP_INT32(tilemapregs[1][0]) & 0x0f00)) drawtilemap(1);
		if (prilevel == (BURN_ENDIAN_SWAP_INT32(tilemapregs[0][0]) & 0x0f00)) drawtilemap(0);

		if ((prilevel == 0x0900) && (nSpriteEnable & 1))
		{
			clearspritebitmap();
			draw_sprites();
			draw_sprite_bitmap();
		}
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xfffffffe;

		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 32;
	INT32 nCyclesTotal = (INT32)((INT64)24000000 * nBurnCPUSpeedAdjust / (0x0100 * 60));

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekRun(nCyclesTotal / nInterleave);
		if (blitter_irq) { SekSetIRQLine(4, CPU_IRQSTATUS_AUTO); blitter_irq = 0; }
	}

	SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);

	SekClose();

	if (pBurnSoundOut) {
		i5000sndUpdate(pBurnSoundOut, nBurnSoundLen);
	}
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029682;
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
		BurnRandomScan(nAction);

		i5000sndScan(nAction, pnMin);

		SCAN_VAR(blitter_irq);
	}

	return 0;
}


// Rabbit (Asia 3/6)

static struct BurnRomInfo rabbitRomDesc[] = {
	{ "jpr0.0",				0x080000, 0x52bb18c0, 1 | BRF_PRG | BRF_ESS }, //  0 M68ec020 Code
	{ "jpr1.1",				0x080000, 0x38299d0d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "jpr2.2",				0x080000, 0xfa3fd91a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "jpr3.3",				0x080000, 0xd22727ca, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "jfv0.00",			0x400000, 0xb2a4d3d3, 2 | BRF_GRA },           //  4 Sprites
	{ "jfv1.01",			0x400000, 0x83f3926e, 2 | BRF_GRA },           //  5
	{ "jfv2.02",			0x400000, 0xb264bfb5, 2 | BRF_GRA },           //  6
	{ "jfv3.03",			0x400000, 0x3e1a9be2, 2 | BRF_GRA },           //  7

	{ "jbg0.40",			0x200000, 0x89662944, 3 | BRF_GRA },           //  8 Background Tiles
	{ "jbg1.50",			0x200000, 0x1fc7f6e0, 3 | BRF_GRA },           //  9
	{ "jbg2.60",			0x200000, 0xaee265fc, 3 | BRF_GRA },           // 10

	{ "jsn0.11",			0x400000, 0xe1f726e8, 4 | BRF_GRA },           // 11 i5000snd Samples

	{ "rabbit.nv",			0x000080, 0x73d471ed, 5 | BRF_PRG | BRF_ESS }, // 12 Default EEPROM
};

STD_ROM_PICK(rabbit)
STD_ROM_FN(rabbit)

struct BurnDriver BurnDrvRabbit = {
	"rabbit", NULL, NULL, NULL, "1997",
	"Rabbit (Asia 3/6)\0", NULL, "Aorn / Electronic Arts", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, rabbitRomInfo, rabbitRomName, NULL, NULL, NULL, NULL, RabbitInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	320, 224, 4, 3
};


// Rabbit (Japan, location test)

static struct BurnRomInfo rabbitjtRomDesc[] = {
	{ "pvo0_mst_1-28.u82",	0x080000, 0xa1c30c91, 1 | BRF_PRG | BRF_ESS }, //  0 M68ec020 Code
	{ "pvo1_mst_1-28.u84",	0x080000, 0x9b7697e6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pvo2_mst_1-28.u83",	0x080000, 0x9809b825, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pvo3_mst_1-28.u85",	0x080000, 0xce8ebb82, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "jfv0.00",			0x400000, 0xb2a4d3d3, 2 | BRF_GRA },           //  4 Sprites
	{ "jfv1.01",			0x400000, 0x83f3926e, 2 | BRF_GRA },           //  5
	{ "jfv2.02",			0x400000, 0xb264bfb5, 2 | BRF_GRA },           //  6
	{ "jfv3.03",			0x400000, 0x3e1a9be2, 2 | BRF_GRA },           //  7

	{ "jbg0.40",			0x200000, 0x89662944, 3 | BRF_GRA },           //  8 Background Tiles
	{ "jbg1.50",			0x200000, 0x1fc7f6e0, 3 | BRF_GRA },           //  9
	{ "jbg2.60",			0x200000, 0xaee265fc, 3 | BRF_GRA },           // 10

	{ "jsn0.11",			0x400000, 0xe1f726e8, 4 | BRF_GRA },           // 11 i5000snd Samples

	{ "rabbit.nv",			0x000080, 0x73d471ed, 5 | BRF_PRG | BRF_ESS }, // 12 Default EEPROM
};

STD_ROM_PICK(rabbitjt)
STD_ROM_FN(rabbitjt)

struct BurnDriver BurnDrvRabbitjt = {
	"rabbitjt", "rabbit", NULL, NULL, "1996",
	"Rabbit (Japan, location test)\0", NULL, "Aorn / Electronic Arts", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, rabbitjtRomInfo, rabbitjtRomName, NULL, NULL, NULL, NULL, RabbitInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	320, 224, 4, 3
};
