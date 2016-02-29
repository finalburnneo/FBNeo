// FB Alpha Sky Army driver module
// Based on MAME driver by Ryan Holtz


// This particular driver is a tutorial to hopefully help others
// develop drivers for fba using mame's source as a starting point
// I have tried to document as many lines as possible, and given hopefully enough details
// 
// original source:
//	src\mame\drivers\skyarmy.c
//

#include "tiles_generic.h" // either this or burnint.h is required, but not both
#include "z80_intf.h"
#include "bitswap.h" // using bitswap functions in this driver...

#include "driver.h" // all part of the ay8910 header
extern "C" {
#include "ay8910.h"
}

// define variables, most of these can be gotten from memindex

static UINT8 *AllMem;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *AllRam;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static UINT32 *Palette;
static UINT32 *DrvPalette;

static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[3];

static UINT8 DrvInputs[3];
static UINT8 DrvDips[1];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvReset;

static UINT8 nmi_enable;

// static INPUT_PORTS_START( skyarmy )
static struct BurnInputInfo SkyarmyInputList[] = {
//	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_COIN1 )
	{"Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"},

// DrvJoy3 + 0 -> 0x01 is (0x01 << 0)
// DrvJoy3 + 1 -> 0x02 is (0x01 << 1) ( * 2)
// DrvJoy3 + 6 -> 0x40 is (0x01 << 6) ( * 64)

//	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START1 )
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 start"},
//	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_PLAYER(1)
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
//	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_PLAYER(1)
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
//	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_PLAYER(1)
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
//	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(1)
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
//	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_PLAYER(1)
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
//	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_PLAYER(1)
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},
//	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON3 )
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 3"},
//	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON4 )
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 4"},
//	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON5 )
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 5"},
//	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_BUTTON6 )
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 6"},

//	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START2 )
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 start"},
//	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_PLAYER(2)
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
//	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_PLAYER(2)
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
//	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
//	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
//	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_PLAYER(2)
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
//	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_PLAYER(2)
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},

//	PORT_START("DSW")
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Skyarmy)

static struct BurnDIPInfo SkyarmyDIPList[]=
{
	{0x14, 0xff, 0xff, 0x02, NULL		},

#if 0

	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x02, "4" )
#endif

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x00, "2"		},
	{0x14, 0x01, 0x03, 0x01, "3"		},
	{0x14, 0x01, 0x03, 0x02, "4"		},
	{0x14, 0x01, 0x03, 0x03, "Free Play"		},

#if 0
	PORT_DIPSETTING(    0x03, DEF_STR (Free_Play ))
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 1C_2C ) )
#endif

	{0   , 0xfe, 0   ,    2, "Coinage"		},
	{0x14, 0x01, 0x08, 0x00, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x08, 0x01, "1 Coin  2 Credits"		},
};

STDDIPINFO(Skyarmy)
// end input setup...


/*
	memory handlers
	these take care of everything that isn't ram or rom
*/

void __fastcall skyarmy_write(UINT16 address, UINT8 data)// handle writes for skyarmy_map
{
	switch (address)
	{
		case 0xa004: // nmi_enable_w
			nmi_enable = data & 1; // state->nmi=data&1;
		return;

		case 0xa005:
		case 0xa006:
		case 0xa007: // AM_WRITENOP <- ignore these writes
		return;
	}
}

UINT8 __fastcall skyarmy_read(UINT16 address)// handle reads for skyarmy_map
{
	switch (address)
	{
		case 0xa000: // AM_READ_PORT("DSW")
			return DrvDips[0];

		case 0xa001: // AM_READ_PORT("P1")
			return DrvInputs[0];

		case 0xa002: // AM_READ_PORT("P2")
			return DrvInputs[1];

		case 0xa003: // AM_READ_PORT("SYSTEM")
			return DrvInputs[2];
	}

	return 0;
}

void __fastcall skyarmy_write_port(UINT16 port, UINT8 data)// handle writes to skyarmy_io_map
{
	switch (port & 0xff) // ADDRESS_MAP_GLOBAL_MASK(0xff)
	{
		case 0x04: // AM_DEVWRITE("aysnd", ay8910_address_data_w)
			AY8910Write(0 /* for multiple ay8910 */, 0 /* ay8910 has 2 write ports*/, data /* write actual data */);
		return;

		case 0x05: // AM_DEVWRITE("aysnd", ay8910_address_data_w)
			AY8910Write(0, 1, data);
		return;
	}
}

UINT8 __fastcall skyarmy_read_port(UINT16 port)// handle reads from skyarmy_io_map
{
	switch (port & 0xff) // ADDRESS_MAP_GLOBAL_MASK(0xff)
	{
		case 0x06: // AM_DEVREAD("aysnd", ay8910_r)
			return AY8910Read(0);
	}

	return 0;
}

/*
	reset the machine
	this is required or lots of bad things happen
*/

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam); // clear all ram!

	ZetOpen(0); // open cpu
	ZetReset(); // reset it
	ZetClose(); // close it

	AY8910Reset(0); // reset ay8910 sound cpu

	HiscoreReset();

	nmi_enable = 0; // disable nmi by default

	return 0;
}

// convert palette rom into a format fba can use (24-bit color)

//static PALETTE_INIT( skyarmy ) // mame
static void DrvPaletteInit()
{
	INT32 i;

	UINT8 *color_prom = DrvColPROM; // fba

	for (i = 0;i < 32;i++)
	{
		INT32 bit0,bit1,bit2,r,g,b;

		bit0 = (*color_prom >> 0) & 0x01;
		bit1 = (*color_prom >> 1) & 0x01;
		bit2 = (*color_prom >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (*color_prom >> 3) & 0x01;
		bit1 = (*color_prom >> 4) & 0x01;
		bit2 = (*color_prom >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0=0;
		bit1 = (*color_prom >> 6) & 0x01;
		bit2 = (*color_prom >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

//		palette_set_color(machine,i,MAKE_RGB(r,g,b)); // mame

		// assemble colors into 24-bit standard rgb format

		Palette[i] = (r << 16) | (g << 8) | (b << 0); // fba 

		color_prom++;
	}
}

/*
	convert the graphics into a format fba's generic tile handling can use
*/

static INT32 DrvGfxDecode()
{
#if 0
static const gfx_layout charlayout =
{
	8,8,
	256,
	2,
	{ 0, 256*8*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static const gfx_layout spritelayout =
{
	16,16,
	32*2,
	2,
	{ 0, 256*8*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
	  8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
	 16*8,17*8,18*8,19*8,20*8,21*8,22*8,23*8 },
	32*8
};

static GFXDECODE_START( skyarmy )
	GFXDECODE_ENTRY( "gfx1", 0, charlayout,   0, 8 )
	GFXDECODE_ENTRY( "gfx2", 0, spritelayout, 0, 8 )
GFXDECODE_END
#endif

	INT32 Plane[2] = { 0, 256*8*8 }; // <---------- same for both types!
	INT32 XOffs[16] = { 0, 1, 2, 3, 4, 5, 6, 7,	// <--- 0 - 7 is used for both types, so we can repeat this and save a few lines
	  8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 };
	INT32 YOffs[16] = { 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,// <--- 0 - 7 is used for both types, so we can repeat this and save a few lines
	 16*8,17*8,18*8,19*8,20*8,21*8,22*8,23*8 };

	UINT8 *tmp = (UINT8*)malloc(0x1000); // ROM_REGION( 0x1000, "gfx1", 0 ), ROM_REGION( 0x1000, "gfx2", 0 ) whichever is larger...
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x1000); // ROM_REGION( 0x1000, "gfx1", 0 )

// number of tiles, bits per pixel, width, height, pointer to plane offsets, x offsets, y offsets, modulo, source, destination

	GfxDecode(256 , 2,  8,  8, Plane, XOffs, YOffs, 8 * 8, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x1000); // ROM_REGION( 0x1000, "gfx2", 0 )

	GfxDecode(32*2, 2, 16, 16, Plane, XOffs, YOffs, 32 * 8, tmp, DrvGfxROM1);


	free (tmp);

	return 0;
}

/*
	MemIndex sets up memory for us

	Next += ... is the size for the previous region

	Values between AllRam and RamEnd are all RAM for the machine we are emulating
	we *should* reset these in DrvDoReset
*/

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x010000; // ROM_REGION( 0x10000, "maincpu", 0 )

	DrvGfxROM0		= Next; Next += 0x001000 * 8 / 2; // 8 bits per byte, and graphics are 2 bits per pixel
	DrvGfxROM1		= Next; Next += 0x001000 * 8 / 2;

	DrvColPROM		= Next; Next += 0x000020;

	Palette			= (UINT32 *)Next; Next += 0x0020 * sizeof(UINT32); // MDRV_PALETTE_LENGTH(32)
	DrvPalette		= (UINT32 *)Next; Next += 0x0020 * sizeof(UINT32); // MDRV_PALETTE_LENGTH(32)

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000800; // ZetMapArea(0x8000, 0x87ff, 0, DrvZ80RAM); <-- (0x87ff+1) - 0x8000
	DrvVidRAM		= Next; Next += 0x000800; // ZetMapArea(0x8800, 0x8fff, 0, DrvVidRAM); <-- (0x8fff+1) - 0x8800
	DrvColRAM		= Next; Next += 0x000400; // ZetMapArea(0x9000, 0x93ff, 2, DrvColRAM); <-- (0x93ff+1) - 0x9000
	DrvSprRAM		= Next; Next += 0x000100; // ZetMapArea(0x9800, 0x98ff, 0, DrvSprRAM); <-- (0x98ff+1) - 0x9800

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16 *)Next; Next += nBurnSoundLen * sizeof(INT16); // allocate ram for sound output
	pAY8910Buffer[1]	= (INT16 *)Next; Next += nBurnSoundLen * sizeof(INT16); // only really necessary for ay8910
	pAY8910Buffer[2]	= (INT16 *)Next; Next += nBurnSoundLen * sizeof(INT16);


	MemEnd			= Next;

	return 0;
}

/*
	DrvInit sets up the machine that we are emulating

	Here we:
		allocate the necessary ram
		load roms in to ram
		set up cpu(s)
		set up sound chips
		initialize fba's graphics routines
		reset the machine
*/

static INT32 DrvInit()
{
	// set up and allocate memory for roms & ram
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)malloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
//		ROM_REGION( 0x10000, "maincpu", 0 )

//		ROM_LOAD( "a1h.bin", 0x0000, 0x2000, CRC(e3fb9d70) SHA1(b8e3a6d7d6ef30c1397f9b741132c5257c16be2d) )
		if (BurnLoadRom(DrvZ80ROM  + 0x00000, 0, 1)) return 1;

//		ROM_LOAD( "a2h.bin", 0x2000, 0x2000, CRC(0417653e) SHA1(4f6ad7335b5b7e85b4e16cce3c127488c02401b2) )
		if (BurnLoadRom(DrvZ80ROM  + 0x02000, 1, 1)) return 1;

//		ROM_LOAD( "a3h.bin", 0x4000, 0x2000, CRC(95485e56) SHA1(c4cbcd31ba68769d2d0d0875e2a92982265339ae) )
		if (BurnLoadRom(DrvZ80ROM  + 0x04000, 2, 1)) return 1;

//		ROM_LOAD( "j4.bin",  0x6000, 0x2000, CRC(843783df) SHA1(256d8375a8af7de080d456dbc6290a22473d011b) )
		if (BurnLoadRom(DrvZ80ROM  + 0x06000, 3, 1)) return 1;

//		ROM_REGION( 0x1000, "gfx1", 0 )

//		ROM_LOAD( "13b.bin", 0x0000, 0x0800, CRC(3b0e0f7c) SHA1(2bbba10121d3e745146f50c14dc6df97de40fb96) )
		if (BurnLoadRom(DrvGfxROM0 + 0x00000, 4, 1)) return 1;

//		ROM_LOAD( "15b.bin", 0x0800, 0x0800, CRC(5ccfd782) SHA1(408406ae068e5578b8a742abed1c37dcd3720fe5) )
		if (BurnLoadRom(DrvGfxROM0 + 0x00800, 5, 1)) return 1;

//		ROM_REGION( 0x1000, "gfx2", 0 )

//		ROM_LOAD( "8b.bin",  0x0000, 0x0800, CRC(6ac6bd98) SHA1(e653d80ec1b0f8e07821ea781942dae3de7d238d) )
		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 6, 1)) return 1;

//		ROM_LOAD( "10b.bin", 0x0800, 0x0800, CRC(cada7682) SHA1(83ce8336274cb8006a445ac17a179d9ffd4d6809) )
		if (BurnLoadRom(DrvGfxROM1 + 0x00800, 7, 1)) return 1;

//		ROM_REGION( 0x0020, "proms", 0 )
//		ROM_LOAD( "a6.bin",  0x0000, 0x0020, CRC(c721220b) SHA1(61b3320fb616c0600d56840cb6438616c7e0c6eb) )
		if (BurnLoadRom(DrvColPROM + 0x00000, 8, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit();
	}

	ZetInit(0); // initialize cpu

	ZetOpen(0); // open cpu #0 for modification

	ZetMapArea(0x0000, 0x7fff, 0, DrvZ80ROM); //	AM_RANGE(0x0000, 0x7fff) AM_ROM
	ZetMapArea(0x0000, 0x7fff, 2, DrvZ80ROM);

	ZetMapArea(0x8000, 0x87ff, 0, DrvZ80RAM); // 	AM_RANGE(0x8000, 0x87ff) AM_RAM
	ZetMapArea(0x8000, 0x87ff, 1, DrvZ80RAM);
	ZetMapArea(0x8000, 0x87ff, 2, DrvZ80RAM);

	ZetMapArea(0x8800, 0x8fff, 0, DrvVidRAM); //	AM_RANGE(0x8800, 0x8fff) AM_RAM_WRITE(skyarmy_videoram_w) < -- skyarmy_videoram_w does't do anything useful
	ZetMapArea(0x8800, 0x8fff, 1, DrvVidRAM);
	ZetMapArea(0x8800, 0x8fff, 2, DrvVidRAM);

	ZetMapArea(0x9000, 0x93ff, 0, DrvColRAM); //	AM_RANGE(0x9000, 0x93ff) AM_RAM_WRITE(skyarmy_colorram_w) < -- skyarmy_colorram_w doesn't do anything useful
	ZetMapArea(0x9000, 0x93ff, 1, DrvColRAM);
	ZetMapArea(0x9000, 0x93ff, 2, DrvColRAM);

	ZetMapArea(0x9800, 0x98ff, 0, DrvSprRAM); // 	AM_RANGE(0x9800, 0x983f) AM_RAM AM_BASE_MEMBER(skyarmy_state,spriteram)
	ZetMapArea(0x9800, 0x98ff, 1, DrvSprRAM); // 	AM_RANGE(0x9840, 0x985f) AM_RAM AM_BASE_MEMBER(skyarmy_state,scrollram)
	ZetMapArea(0x9800, 0x98ff, 2, DrvSprRAM); // ZetMapArea( can only handle 0-ff sized areas, so we must combine these two writes scrollram = sprram + 0x40

	ZetSetWriteHandler(skyarmy_write);  // handle writes for skyarmy_map that aren't ram / rom
	ZetSetReadHandler(skyarmy_read);    // handle reads for skyarmy_map that aren't ram / rom

	ZetSetOutHandler(skyarmy_write_port); // handle writes to skyarmy_io_map
	ZetSetInHandler(skyarmy_read_port); // handle reads from skyarmy_io_map

	ZetClose(); // close cpu to further modifications

	AY8910Init(0, 2500000, nBurnSoundRate, NULL, NULL, NULL, NULL); // MDRV_SOUND_ADD("aysnd", AY8910, 2500000)
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit(); // this must be called for generic tile handling

	DrvDoReset(); // reset the machine..

	return 0;
}

/*
	DrvExit exits the machine
*/

static INT32 DrvExit()
{
	GenericTilesExit(); // exit generic tile handling

	ZetExit(); // exit cpu

	AY8910Exit(0); // exit ay8910 cpu

	free (AllMem); // Free all RAM
	AllMem = NULL;

	return 0;
}


static void tilemap_draw()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++) // tilemap_create(machine, get_skyarmy_tile_info, tilemap_scan_rows, 8, 8, 32, 32);
	{
		INT32 sx = (offs % 32) * 8; // tilemap_create(machine, get_skyarmy_tile_info, tilemap_scan_rows, 8 (sx tile width), 8, 32 (number of tiles wide), 32);
		INT32 sy = (offs / 32) * 8; // tilemap_create(machine, get_skyarmy_tile_info, tilemap_scan_rows, 8, 8 (sy tile height), 32, 32 (number of tiles high));

//	for(i=0;i<0x20;i++)
//		tilemap_set_scrolly( state->tilemap,i,state->scrollram[i]);
//	tilemap_set_scroll_cols(state->tilemap,32);

		sy -= DrvSprRAM[0x40 + (offs % 32)] + 8; // (offs % 32) gives the column, scroll ram is 64 bytes after sprite data
		if (sy < -7) sy += 256; // (256 = 32 * 8), -7 or the screen shows garbage at the sides

		INT32 code = DrvVidRAM[offs];	// which tile are we drawing?
		INT32 attr = BITSWAP08(DrvColRAM[offs], 7, 6, 5, 4, 3, 0, 1, 2) & 7; // color

		// there is no transparency color (mask), can possibly be partially off the screen so use clip, and does not do flip
		Render8x8Tile_Clip(pTransDraw, code, sx, sy, attr /*color*/, 2 /* 2 bits - determine with gfx decode */, 0, DrvGfxROM0);
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x40; offs+=4)
	{
		INT32 pal = BITSWAP08(DrvSprRAM[offs+2], 7, 6, 5, 4, 3, 0, 1, 2) & 7; // color (this particular setup is unusual)

		INT32 sx    = DrvSprRAM[offs+3];		        // horizontal position -> start x
		INT32 sy    = 240-(DrvSprRAM[offs] + 1) - 8;		// vertical position -> start y
		INT32 flipy = (DrvSprRAM[offs+1]&0x80)>>7;	// flip tile vertically?
		INT32 flipx = (DrvSprRAM[offs+1]&0x40)>>6;	// flip tile horizontally?
		INT32 code  = DrvSprRAM[offs + 1] & 0x3f;		// which tile are we drawing?
		if (sy < -7) sy += 256; // (256 = 32 * 8), -7 or the screen shows garbage at the sides
		if (sx < -7) sx += 256; // same as above, but do sx too.  this handles sprite/tile wrapping.
		if (sy > 240) sy -= 256; // this kind of wrapping isn't as common or used in every game, but it's used here to fix the missing bridge heli when arriving at home base. -dink
#if 0
		drawgfx_transpen(bitmap,cliprect,screen->machine->gfx[1],
			spriteram[offs+1]&0x3f, /* code! */
			pal, /* color*/
			flipx,flipy,
			sx,sy,0 /*transparent color!*/);
#endif

	// this does have flip, can be partially off screen, and has a transparent color
	// so use flip, mask, clip
		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, pal /*color*/, 2 /*2 bits*/, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, pal /*color*/, 2 /*2 bits*/, 0, 0, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, pal /*color*/, 2 /*2 bits*/, 0, 0, DrvGfxROM1);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, pal /*color*/, 2 /*2 bits*/, 0, 0, DrvGfxROM1);
			}
		}
	}
}


//static VIDEO_UPDATE( skyarmy )
static INT32 DrvDraw()
{
	// this will recalculate the colors if the screen color depth changes
	if (DrvRecalc) {
		for (INT32 i = 0; i < 32; i++) {
			INT32 d = Palette[i];
			DrvPalette[i] = BurnHighCol(d >> 16, (d >> 8) & 0xff, d & 0xff, 0);
		}
		DrvRecalc = 0; // ok, we've recalculated it, now disable it for the next frame or we waste a lot of time
	}

	tilemap_draw();
	draw_sprites(); // this was originally inside VIDEO_UPDATE, but move it to its own function to make things cleaner

	BurnTransferCopy(DrvPalette); // copy pixels so that they will work at any screen color depth

	return 0;
}


/*
	the frame function is where we actually run the machine
	here we:
		reset the machine if necessary
		assemble inputs in a way the machine can understand
		run the cpu
		set the irqs (interrupts [it's complicated])
		output sound
		draw video
*/

static INT32 DrvFrame()
{
	if (DrvReset) { // is the reset button pressed
		DrvDoReset(); // if so, reset!
	}

	// assemble inputs
	{
		memset (DrvInputs, 0, 3); // clear input storage bytes < if IP_ACTIVE_HIGH clear with 0, if IP_ACTIVE_LOW, clear with 0xff
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i; // pack bits for inputs in to respective bytes
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	// MDRV_SCREEN_REFRESH_RATE(60) ! 60!!

	INT32 nInterleave = 102/8; // MDRV_CPU_PERIODIC_INT(skyarmy_nmi_source,650) -> 4000000 / 60 -> 66666.67 / 650 -> 102 (add /8 to get the right timing -dink)
	INT32 nCyclesTotal = 4000000 / 60; // MDRV_CPU_ADD("maincpu", Z80,4000000)
	INT32 nCyclesDone  = 0;
	INT32 nSoundBufferPos = 0;

	ZetOpen(0); // open cpu for modification

	for (INT32 i = 0; i < nInterleave; i++) // split the amount of cpu the z80 is running in 1/60th of a second into slices
	{
		INT32 nSegment = nCyclesTotal / nInterleave;

		nCyclesDone += ZetRun(nSegment); // actually run the cpu

		if (i == (nInterleave - 1)) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD); // MDRV_CPU_VBLANK_INT("screen", irq0_line_hold)
		}
		// don't call if the irq above is triggered...
		// static INTERRUPT_GEN( skyarmy_nmi_source )
		if (i != (nInterleave - 1) && nmi_enable) ZetNmi(); //if(state->nmi) cpu_set_input_line(device,INPUT_LINE_NMI, PULSE_LINE);

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
			nSoundBufferPos += nSegmentLength;
		}
	}

	ZetClose(); // close the cpu to modifications

// output the sound -- ay8910 is more complex than most
// you can pretty much just copy and paste this from other drivers (this blob is from d_4enraya.cpp)

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
		}
	}

	// make sure the drawing surface is allocated and then draw!
	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

/*
	fba's save state function

	we check the lowest version of fba this should be compatible with
	and if it's not, we exit the state loading 

	we also save ram, data from the z80, ay8910, and other misc data
*/

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029708; // use current version number
	}

	if (nAction & ACB_VOLATILE) {		
		memset(&ba, 0, sizeof(ba));	// save all ram
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);		// save z80 data
		AY8910Scan(nAction, pnMin);	// save ay8910 data

		SCAN_VAR(nmi_enable); 	// save nmi_enable since it is not part of defined ram (in memindex)
	}

	return 0;
}	


// Sky Army

static struct BurnRomInfo skyarmyRomDesc[] = {
//	ROM_REGION( 0x10000, "maincpu", 0 )
//	ROM_LOAD( "a1h.bin", 0x0000, 0x2000, CRC(e3fb9d70) SHA1(b8e3a6d7d6ef30c1397f9b741132c5257c16be2d) )
	{ "a1h.bin",	0x2000, 0xe3fb9d70, 1 }, //  0 maincpu
//	ROM_LOAD( "a2h.bin", 0x2000, 0x2000, CRC(0417653e) SHA1(4f6ad7335b5b7e85b4e16cce3c127488c02401b2) )
	{ "a2h.bin",	0x2000, 0x0417653e, 1 }, //  1
//	ROM_LOAD( "a3h.bin", 0x4000, 0x2000, CRC(95485e56) SHA1(c4cbcd31ba68769d2d0d0875e2a92982265339ae) )
	{ "a3h.bin",	0x2000, 0x95485e56, 1 }, //  2
//	ROM_LOAD( "j4.bin",  0x6000, 0x2000, CRC(843783df) SHA1(256d8375a8af7de080d456dbc6290a22473d011b) )
	{ "j4.bin",	0x2000, 0x843783df, 1 }, //  3

//	ROM_REGION( 0x1000, "gfx1", 0 )
//	ROM_LOAD( "13b.bin", 0x0000, 0x0800, CRC(3b0e0f7c) SHA1(2bbba10121d3e745146f50c14dc6df97de40fb96) )
	{ "13b.bin",	0x0800, 0x3b0e0f7c, 2 }, //  4 gfx1
//	ROM_LOAD( "15b.bin", 0x0800, 0x0800, CRC(5ccfd782) SHA1(408406ae068e5578b8a742abed1c37dcd3720fe5) )
	{ "15b.bin",	0x0800, 0x5ccfd782, 2 }, //  5

//	ROM_REGION( 0x1000, "gfx2", 0 )
//	ROM_LOAD( "8b.bin",  0x0000, 0x0800, CRC(6ac6bd98) SHA1(e653d80ec1b0f8e07821ea781942dae3de7d238d) )
	{ "8b.bin",	0x0800, 0x6ac6bd98, 3 }, //  6 gfx2
//	ROM_LOAD( "10b.bin", 0x0800, 0x0800, CRC(cada7682) SHA1(83ce8336274cb8006a445ac17a179d9ffd4d6809) )
	{ "10b.bin",	0x0800, 0xcada7682, 3 }, //  7

//	ROM_REGION( 0x0020, "proms", 0 )
//	ROM_LOAD( "a6.bin",  0x0000, 0x0020, CRC(c721220b) SHA1(61b3320fb616c0600d56840cb6438616c7e0c6eb) )
	{ "a6.bin",	0x0020, 0xc721220b, 4 }, //  8 proms
};

STD_ROM_PICK(skyarmy)
STD_ROM_FN(skyarmy)

//GAME( 1982, skyarmy, 0, skyarmy, skyarmy, 0, ROT90, "Shoei", "Sky Army", 0 )

struct BurnDriver BurnDrvSkyarmy = {
	"skyarmy", NULL, NULL, NULL, "1982", // set name, parent name, bios name, publish date
	"Sky Army\0", NULL, "Shoei", "Miscellaneous", // title, notes, developer, hardware name 
	NULL, NULL, NULL, NULL, // unicode title, unicode notes, unicode developer, unicode hardware name
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0, // orientation vertical | flipped -> ROT90, number of players, hardware type, game category, series?
	NULL, skyarmyRomInfo, skyarmyRomName, NULL, NULL, SkyarmyInputInfo, SkyarmyDIPInfo, // extra rom load routine (usually not used), rom info, rom name, input def, dip def
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 32, // 32 = palette size // pointers to init, exit, frame, draw, savestate, jukebox, drvrecalc (palette), number of palette colors
	240, 256, 3, 4 // MDRV_SCREEN_VISIBLE_AREA(0*8,32*8-1,1*8,31*8-1) switch x & y because of ROT90 // screen width, screen width, aspect ration x, y
};

