// FinalBurn Neo Seibu SPI System driver module
// Based on MAME driver by Ville Linde, hap, Nicola Salmoria

/*
	Notes:
		Raiden Fighters 2 has a problem with flip screen handling *fixed with hack, read on*
		- even with it disabled, it doesn't seem to respect the setting
		- force it by going into the diagnostics and exiting, then resetting the game
		- right now use default NVRAM data - only needs the first 32 bytes is necessary to correct the issue
		- works with single board versions, use default EEPROM data

	to do:
		test extensively
*/

#include "tiles_generic.h"
#include "i386_intf.h"
#include "z80_intf.h"
#include "msm6295.h"
#include "burn_ymf271.h"
#include "ymz280b.h"
#include "ds2404.h"
#include "intelfsh.h"
#include "eeprom.h"
#include "burn_pal.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvZ80WorkRAM;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvSndROM[2];
static UINT8 *DefaultEEPROM;
static UINT8 *DrvMainRAM;
static UINT32 *mainram;
static UINT32 *tilemap_ram;
static UINT16 *tilemap_ram16; // for tilemap_callbacks
static UINT32 *palette_ram;
static UINT32 *sprite_ram;
static UINT8 *DrvCRTCRAM;
static const UINT8 *DefaultNVRAM = NULL;
static UINT8 *DrvAlphaTable;
static UINT32 *bitmap32;
static UINT16 *tempdraw;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles;

static UINT32 video_dma_length;
static UINT32 video_dma_address;
static INT32 rowscroll_enable;
static INT32 rf2_layer_bank;
static INT32 text_layer_offset;
static INT32 fore_layer_offset;
static INT32 midl_layer_offset;
static INT32 fore_layer_d13;
static INT32 fore_layer_d14;
static INT32 back_layer_d14;
static INT32 midl_layer_d14;

#define FIFO_SIZE 512
static INT32 fifoin_rpos;
static INT32 fifoin_wpos;
static INT32 fifoout_rpos;
static INT32 fifoout_wpos;
static UINT8 fifoin_data[FIFO_SIZE];
static UINT8 fifoout_data[FIFO_SIZE];
static INT32 fifoin_read_request;
static INT32 fifoout_read_request;
static INT32 z80_prog_xfer_pos;
static INT32 z80_bank;
static INT32 oki_bank;
static INT32 coin_latch;

static INT32 input_select;

// configuration variables
static INT32 layer_enable;
static INT32 sprite_ram_size;
static INT32 bg_fore_layer_position;
enum { DECRYPT_SEI252 = 0, DECRYPT_RISE10, DECRYPT_RISE11 };

static INT32 graphics_len[3];
static INT32 sound_system = 0; 			// 0 = msm6295, 1 = z80 + ymf271, 2 - ymz280b
static INT32 rom_based_z80 = 0;

static INT32 has_eeprom = 0;

static UINT32 speedhack_address = ~0;
static UINT32 speedhack_pc = 0;

static UINT8 DrvJoy1[32];
static UINT8 DrvJoy2[32];
static UINT8 DrvJoy3[32];
static UINT8 DrvJoy4[32];
static UINT8 DrvJoy5[16];
static UINT8 DrvJoy6[16];
static UINT8 DrvJoy7[16];
static UINT8 DrvJoy8[16];
static UINT8 DrvJoy9[16];
static UINT32 DrvInputs[10];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static struct BurnInputInfo Spi_3buttonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Spi_3button)

static struct BurnInputInfo Spi_2buttonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Spi_2button)

static struct BurnInputInfo Sys386iInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
};

STDINPUTINFO(Sys386i)

static struct BurnInputInfo Spi_ejanhsInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy8 + 5,	"p1 start"	},

	{"P1 Pon",			BIT_DIGITAL,	DrvJoy5 + 1,	"mah pon"	},
	{"P1 L",			BIT_DIGITAL,	DrvJoy5 + 2,	"mah l"		},
	{"P1 H",			BIT_DIGITAL,	DrvJoy5 + 3,	"mah h"		},
	{"P1 D",			BIT_DIGITAL,	DrvJoy5 + 4,	"mah d"		},

	{"P1 Big",			BIT_DIGITAL,	DrvJoy6 + 0,	"mah big"	},
	{"P1 Flip Flip",	BIT_DIGITAL,	DrvJoy6 + 1,	"mah ff"	},
	{"P1 Double Up",	BIT_DIGITAL,	DrvJoy6 + 2,	"mah wup"	},
	{"P1 Score",		BIT_DIGITAL,	DrvJoy6 + 3,	"mah score"	},
	{"P1 Last Chance",	BIT_DIGITAL,	DrvJoy6 + 4,	"mah lc"	},
	{"P1 Small",		BIT_DIGITAL,	DrvJoy6 + 5,	"mah small"	},

	{"P1 Ron",			BIT_DIGITAL,	DrvJoy7 + 0,	"mah ron"	},
	{"P1 Chi",			BIT_DIGITAL,	DrvJoy7 + 1,	"mah chi"	},
	{"P1 K",			BIT_DIGITAL,	DrvJoy7 + 2,	"mah k"		},
	{"P1 G",			BIT_DIGITAL,	DrvJoy7 + 3,	"mah g"		},
	{"P1 C",			BIT_DIGITAL,	DrvJoy7 + 4,	"mah c"		},

	{"P1 Kan",			BIT_DIGITAL,	DrvJoy8 + 0,	"mah kan"	},
	{"P1 M",			BIT_DIGITAL,	DrvJoy8 + 1,	"mah m"		},
	{"P1 I",			BIT_DIGITAL,	DrvJoy8 + 2,	"mah i"		},
	{"P1 E",			BIT_DIGITAL,	DrvJoy8 + 3,	"mah e"		},
	{"P1 A",			BIT_DIGITAL,	DrvJoy8 + 4,	"mah a"		},

	{"P1 Reach",		BIT_DIGITAL,	DrvJoy9 + 0,	"mah reach"	},
	{"P1 N",			BIT_DIGITAL,	DrvJoy9 + 1,	"mah n"		},
	{"P1 J",			BIT_DIGITAL,	DrvJoy9 + 2,	"mah j"		},
	{"P1 F",			BIT_DIGITAL,	DrvJoy9 + 3,	"mah f"		},
	{"P1 B",			BIT_DIGITAL,	DrvJoy9 + 4,	"mah b"		},
	{"P1 Bet",			BIT_DIGITAL,	DrvJoy9 + 5,	"mah bet"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy2 + 2,	"diag"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Spi_ejanhs)

static struct BurnInputInfo EjsakuraInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy8 + 5,	"p1 start"	},

	{"P1 Pon",			BIT_DIGITAL,	DrvJoy5 + 1,	"mah pon"	},
	{"P1 L",			BIT_DIGITAL,	DrvJoy5 + 2,	"mah l"		},
	{"P1 H",			BIT_DIGITAL,	DrvJoy5 + 3,	"mah h"		},
	{"P1 D",			BIT_DIGITAL,	DrvJoy5 + 4,	"mah d"		},

	{"P1 Big",			BIT_DIGITAL,	DrvJoy6 + 0,	"mah big"	},
	{"P1 Flip Flip",	BIT_DIGITAL,	DrvJoy6 + 1,	"mah ff"	},
	{"P1 Double Up",	BIT_DIGITAL,	DrvJoy6 + 2,	"mah wup"	},
	{"P1 Score",		BIT_DIGITAL,	DrvJoy6 + 3,	"mah score"	},
	{"P1 Last Chance",	BIT_DIGITAL,	DrvJoy6 + 4,	"mah lc"	},
	{"P1 Small",		BIT_DIGITAL,	DrvJoy6 + 5,	"mah small"	},

	{"P1 Ron",			BIT_DIGITAL,	DrvJoy7 + 0,	"mah ron"	},
	{"P1 Chi",			BIT_DIGITAL,	DrvJoy7 + 1,	"mah chi"	},
	{"P1 K",			BIT_DIGITAL,	DrvJoy7 + 2,	"mah k"		},
	{"P1 G",			BIT_DIGITAL,	DrvJoy7 + 3,	"mah g"		},
	{"P1 C",			BIT_DIGITAL,	DrvJoy7 + 4,	"mah c"		},

	{"P1 Kan",			BIT_DIGITAL,	DrvJoy8 + 0,	"mah kan"	},
	{"P1 M",			BIT_DIGITAL,	DrvJoy8 + 1,	"mah m"		},
	{"P1 I",			BIT_DIGITAL,	DrvJoy8 + 2,	"mah i"		},
	{"P1 E",			BIT_DIGITAL,	DrvJoy8 + 3,	"mah e"		},
	{"P1 A",			BIT_DIGITAL,	DrvJoy8 + 4,	"mah a"		},

	{"P1 Reach",		BIT_DIGITAL,	DrvJoy9 + 0,	"mah reach"	},
	{"P1 N",			BIT_DIGITAL,	DrvJoy9 + 1,	"mah n"		},
	{"P1 J",			BIT_DIGITAL,	DrvJoy9 + 2,	"mah j"		},
	{"P1 F",			BIT_DIGITAL,	DrvJoy9 + 3,	"mah f"		},
	{"P1 B",			BIT_DIGITAL,	DrvJoy9 + 4,	"mah b"		},
	{"P1 Bet",			BIT_DIGITAL,	DrvJoy9 + 5,	"mah bet"	},
	{"P1 Payout",		BIT_DIGITAL,	DrvJoy9 + 11,	"payout"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy9 + 9,	"diag"		},
};

STDINPUTINFO(Ejsakura)

static struct BurnDIPInfo DefaultDIPList[]=
{
	{0   , 0xfe, 0   ,    3, "JP1"			},
	{0x00, 0x01, 0x03, 0x03, "Update"		},
	{0x00, 0x01, 0x03, 0x01, "Off"			},
	{0x00, 0x01, 0x03, 0x00, "On"			},
};

static struct BurnDIPInfo Spi_3buttonDIPList[]=
{
	DIP_OFFSET(0x15)
	{0x00, 0xff, 0xff, 0xff, NULL			},
};

static struct BurnDIPInfo Spi_2buttonDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0xff, NULL			},
};

static struct BurnDIPInfo Spi_ejanhsDIPList[]=
{
	DIP_OFFSET(0x1F)
	{0x00, 0xff, 0xff, 0xff, NULL			},
};

STDDIPINFOEXT(Spi_3button, Spi_3button, Default)
STDDIPINFOEXT(Spi_2button, Spi_2button, Default)
STDDIPINFOEXT(Spi_ejanhs, Spi_ejanhs, Default)

static void crtc_write()
{
	UINT16 *ram = (UINT16*)DrvCRTCRAM;

	UINT16 layer_bank = BURN_ENDIAN_SWAP_INT16(ram[0x1a / 2]);

	rowscroll_enable = (layer_bank >> 15) & 1;

	fore_layer_offset = 0x1000 / 4;
	midl_layer_offset = 0x2000 / 4;
	text_layer_offset = 0x3000 / 4;

	if (rowscroll_enable == 0)
	{
		fore_layer_offset /= 2;
		midl_layer_offset /= 2;
		text_layer_offset /= 2;
	}

	fore_layer_d13 = (layer_bank << 2) & 0x2000;
	back_layer_d14 = (rf2_layer_bank << 14) & 0x4000;
	midl_layer_d14 = (rf2_layer_bank << 13) & 0x4000;
	fore_layer_d14 = (rf2_layer_bank << 12) & 0x4000;
}

static void tilemap_dma_start_write()
{
	INT32 index = video_dma_address / 4;
	INT32 offsets[7] = { 0, 0x800/4, fore_layer_offset, 0x2800/4, midl_layer_offset, 0x1800/4, text_layer_offset };

	for (INT32 i = 0; i < 7; i++)
	{
		if ((i & 1) && rowscroll_enable == 0) continue; // copy rowscroll data.. or not

		memmove(&tilemap_ram[offsets[i]], &mainram[index], i == 6 ? 0x1000 : 0x800); // text is 1000
		index += 0x800/4;
	}
}

static void palette_dma_start_write()
{
	const INT32 dma_length = (video_dma_length + 1) * 2;

	for (INT32 i = 0; i < dma_length / 4; i++)
	{
		UINT32 color = BURN_ENDIAN_SWAP_INT32(mainram[video_dma_address / 4 + i]);

		if (BURN_ENDIAN_SWAP_INT32(palette_ram[i]) != color)
		{
			palette_ram[i] = BURN_ENDIAN_SWAP_INT32(color);

			DrvPalette[(i * 2) + 0] = (pal5bit(color >>  0) << 16) | (pal5bit(color >>  5) << 8) | pal5bit(color >> 10);
			DrvPalette[(i * 2) + 1] = (pal5bit(color >> 16) << 16) | (pal5bit(color >> 21) << 8) | pal5bit(color >> 26);
		}
	}
}

static void sprite_dma_start_write()
{
	memmove(&sprite_ram[0], &mainram[video_dma_address / 4], sprite_ram_size);
}

static void oki_bankswitch(INT32 data)
{
	oki_bank = data & 0x04;

	MSM6295SetBank(0, DrvSndROM[0], 0, 0x3ffff);
	MSM6295SetBank(1, DrvSndROM[1] + ((oki_bank) ? 0x40000 : 0), 0, 0x3ffff);
}

static void layerbanks_eeprom_write(UINT16 data)
{
	rf2_layer_bank = data;
	crtc_write();

	EEPROMWriteBit((data & 0x80) ? 1 : 0);
	EEPROMSetClockLine((data & 0x40) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
	EEPROMSetCSLine((data & 0x20) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
}

static void common_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffffffc0) == 0x00000400) {
		DrvCRTCRAM[(address & 0x3f)] = data;
		if ((address & 0x3e) == 0x1a) crtc_write();
		return;
	}

	if (address < 0x40000) {
		DrvMainRAM[address] = data;
		return;
	}
}

static void common_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffffc0) == 0x00000400) {
		UINT16 *crtc = (UINT16*)DrvCRTCRAM;
		crtc[(address / 2) & 0x1f] = BURN_ENDIAN_SWAP_INT16(data);
		if ((address & 0x3e) == 0x1a) crtc_write();
		return;
	}

	switch (address)
	{
		case 0x480:
			tilemap_dma_start_write();
		return;

		case 0x484:
			palette_dma_start_write();
		return;

		case 0x490:
			video_dma_length = (video_dma_length & 0xffff0000) | data;
			//bprintf(0, _T("ww video_dma_len %x   data %x\n"),video_dma_length, data);
		return;

		case 0x494:
			video_dma_address = data;
			//bprintf(0, _T("ww video_dma_address %x\n"),video_dma_address);
		return;

		case 0x498:
		break; //nop

//		case 0x50e: // (50e) -sei252
//		case 0x562: // (562) -rise
//			sprite_dma_start_write();
//		break;
	}

	if (address < 0x40000) {
		UINT16 *ram = (UINT16*)DrvMainRAM;
		ram[address/2] = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}
}

static void common_write_dword(UINT32 address, UINT32 data)
{
	if ((address & 0xffffffc0) == 0x00000400) {
		UINT32 *crtc = (UINT32*)DrvCRTCRAM;
		crtc[(address / 4) & 0xf] = BURN_ENDIAN_SWAP_INT32(data);
		if ((address & 0x3c) == 0x18) crtc_write();
		return;
	}

	switch (address)
	{
		case 0x480:
			tilemap_dma_start_write();
		return;

		case 0x484:
			palette_dma_start_write();
		return;

		case 0x490:
			video_dma_length = data;
		return;

		case 0x494:
			video_dma_address = data;
		return;

		case 0x498:
		return; //nop
	}

	if (address < 0x40000) {
		UINT32 *ram = (UINT32*)DrvMainRAM;
		ram[address/4] = BURN_ENDIAN_SWAP_INT32(data);
		return;
	}
}

static INT32 ejanhs_encode(INT32 N)
{
	static const UINT8 encoding[6] = { 6, 5, 4, 3, 2, 7 };
	UINT8 state = ~DrvInputs[4 + N];

	for (INT32 bit = 0; bit < 6; bit++)
		if (state & (1 << bit))
			return encoding[bit];

	return 0;
}

static UINT32 common_read_dword(UINT32 address)
{
	if ((address & 0xffffffc0) == 0x00000400) {
		UINT32 *crtc = (UINT32*)DrvCRTCRAM;
		return BURN_ENDIAN_SWAP_INT32(crtc[(address / 4) & 0xf]);
	}

	switch (address)
	{
		case 0x600:
			return 1; // spi status

		case 0x604:
		{
			UINT32 ret = DrvInputs[0]; // FLIPSCREEN & ~0x8000;

			if (BurnDrvGetGenreFlags() & GBF_MAHJONG) { // ejanhs
				UINT32 ej = 0;
				ret &= 0xffff4000;
				ej |= ejanhs_encode(3) << 0;
				ej |= ejanhs_encode(4) << 3;
				ej |= ejanhs_encode(2) << 8;
				ej |= ejanhs_encode(0) << 11;
				ej = ~ej; // active low
				ret |= (ej & 0x3f3f); // add bits from encoder
			}

			return ret; // inputs
		}

		case 0x608:
			return DrvInputs[2]; // exch

		case 0x60c:
		{
			UINT32 ret = DrvInputs[1];

			if (has_eeprom) ret = (ret & ~0x40) | (EEPROMRead() ? 0x40 : 0);

			return ret; // system
		}

		case 0x688: return 0; // rdft2us
	}

	if (address < 0x40000) {
		if (speedhack_address == address) {
			if (speedhack_pc == i386GetPC(-1)) {
				i386RunEnd();
				i386HaltUntilInterrupt(1);
			}
		//	if (speedhack_pc == 1) { // speedhack finder
		//		bprintf (0, _T("SPEEDHACK: %5.5x\n"), i386GetPC(-1));
		//	}
		}

		UINT32 *ram = (UINT32*)DrvMainRAM;
		return BURN_ENDIAN_SWAP_INT32(ram[address/4]);
	}

	return 0;
}

static UINT16 common_read_word(UINT32 address)
{
	return i386ReadLong(address & ~3) >> ((address & 2) * 8);
}

static UINT8 common_read_byte(UINT32 address)
{
	return i386ReadLong(address & ~3) >> ((address & 3) * 8);
}

static void sync_cpu()
{
	// main is 25000 000
	// sound is 7159 000

	UINT32 cycles = (i386TotalCycles() * 7159) / 25000;

	if (cycles > (UINT32)ZetTotalCycles())
		BurnTimerUpdate(cycles);
}

static UINT8 z80_fifoout_pop()
{
	UINT8 r = fifoout_data[fifoout_rpos++];
	if (fifoout_rpos == FIFO_SIZE) fifoout_rpos = 0;
	if (fifoout_wpos == fifoout_rpos) fifoout_read_request = 0;
	return r;
}

static void z80_fifoout_push(UINT8 data)
{
	fifoout_data[fifoout_wpos++] = data;
	if (fifoout_wpos == FIFO_SIZE) fifoout_wpos = 0;
	fifoout_read_request = 1;
}

static UINT8 z80_fifoin_pop()
{
	UINT8 r = fifoin_data[fifoin_rpos++];
	if (fifoin_rpos == FIFO_SIZE) fifoin_rpos = 0;
	if (fifoin_wpos == fifoin_rpos) fifoin_read_request = 0;
	return r;
}

static void z80_fifoin_push(UINT8 data)
{
	fifoin_data[fifoin_wpos++] = data;
	if (fifoin_wpos == FIFO_SIZE) fifoin_wpos = 0;
	fifoin_read_request = 1;
}

static void spi_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x68e:
			rf2_layer_bank = (rf2_layer_bank & 0xff00) | data;
			crtc_write();
			if (has_eeprom) {
				EEPROMWriteBit((data & 0x80) ? 1 : 0);
				EEPROMSetClockLine((data & 0x40) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
				EEPROMSetCSLine((data & 0x20) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
			}
		return;

		case 0x68f:
			rf2_layer_bank = (rf2_layer_bank & 0x00ff) | (data << 8);
			crtc_write();
		return;

		case 0x690:
		case 0x691:
		return;
	}

	common_write_byte(address, data);
}

static void spi_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x50e:
		case 0x562: // rdft2 (can these co-exist peacefully??)
			sprite_dma_start_write();
		return;

		case 0x600:
		return; // nop?

		case 0x680:
			sync_cpu();
			z80_fifoin_push(data & 0xff);
		return;

		case 0x688:
			if (rom_based_z80 == 0) {
				if (z80_prog_xfer_pos < 0x40000) {
					DrvZ80RAM[z80_prog_xfer_pos] = data & 0xff;
					z80_prog_xfer_pos++;
				}
			}
		return;

		case 0x68c:
			if (rom_based_z80 == 0) {
				sync_cpu();
				z80_prog_xfer_pos = 0;
				ZetSetRESETLine((data & 1) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
			}
		return;

		case 0x68e:
			rf2_layer_bank = data;
			crtc_write();
			if (has_eeprom) {
				EEPROMWriteBit((data & 0x80) ? 1 : 0);
				EEPROMSetClockLine((data & 0x40) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
				EEPROMSetCSLine((data & 0x20) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
			}
		return;
	}

	common_write_word(address, data);
}

static void spi_write_dword(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x524: // rise decryption key
		case 0x528: // unknown
		case 0x530: // rise decryption table key
		case 0x534: // unknown
		case 0x53c: // rise decryption table index
		return;

		case 0x6d0:
			ds2404_1w_reset_write(data);
		return;

		case 0x6d4:
			ds2404_data_write(data);
		return;

		case 0x6d8:
			ds2404_clk_write(data);
		return;
	}

	common_write_dword(address, data);
}

static UINT32 spi_read_dword(UINT32 address)
{
	switch (address)
	{
		case 0x680:
		{
			if (rom_based_z80 == 0) return z80_fifoout_pop();

			INT32 ret = coin_latch;
			coin_latch = 0;
			return ret;
		}

		case 0x684:
			return fifoout_read_request ? 3 : 1;

		case 0x6dc:
			return ds2404_data_read() | (0x00 << 8); // must be clear on this byte
	}

	return common_read_dword(address);
}

static void spi_i386_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x562:
			sprite_dma_start_write();
		return;

		case 0x68e:
			layerbanks_eeprom_write(data);
			oki_bankswitch(data >> 8);
		return;
	}

	common_write_word(address, data);
}

static void spi_i386_write_dword(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x1200000:
		case 0x1200004:
			MSM6295Write((address / 4) & 1, data);
		return;
	}

	common_write_dword(address, data);
}

static UINT32 spi_i386_read_dword(UINT32 address)
{
	switch (address)
	{
		case 0x60c:
		{
			UINT32 ret = DrvInputs[1] & ~0x40;
			if (EEPROMRead()) ret |= 0x40;
			return ret; // system
		}

		case 0x1200000:
		case 0x1200004:
			return MSM6295Read((address / 4) & 1);
	}

	return common_read_dword(address);
}

static UINT32 ejsakura_keyboard_read()
{
	// coins/eeprom data
	UINT32 ret = DrvInputs[0] & ~0x4000;
	if (EEPROMRead()) ret |= 0x4000;

	for (INT32 i = 0; i < 5; i++)
		if ((input_select >> i) & 1)
			ret &= DrvInputs[4 + i];

	return ret;
}

static UINT32 sys386f_read_dword(UINT32 address)
{
	switch (address)
	{
		case 0x010:
			return 1; // spi status

		case 0x400:
			return ~0; // system (not used)

		case 0x600:
		case 0x604:
			return YMZ280BReadStatus();

		case 0x60c:
			return ejsakura_keyboard_read();
	}

	return common_read_dword(address);
}

static void sys386f_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x400:
			input_select = data;
		return;

		case 0x404:
			EEPROMWriteBit((data & 0x80) ? 1 : 0);
			EEPROMSetClockLine((data & 0x40) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetCSLine((data & 0x20) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
		return;

		case 0x408:
			YMZ280BSelectRegister(data);
		return;

		case 0x40c:
			YMZ280BWriteRegister(data);
		return;

		case 0x562: // (562) -rise
			sprite_dma_start_write();
		return;
	}

	common_write_word(address, data);
}

static void sound_bankswitch(INT32 data)
{
	z80_bank = data & 7;

	ZetMapMemory(DrvZ80RAM + z80_bank * 0x8000, 0x8000, 0xffff, MAP_ROM);
}

static void __fastcall spi_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff0) == 0x6000) {
		BurnYMF271Write(address & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0x4002:
		case 0x4003:
		case 0x400b:
		return; // nop

		case 0x4004:
			coin_latch = data ? (data | 0xa0) : 0;
			// coin counter = data & 3
		return;

		case 0x4008:
			z80_fifoout_push(data);
		return;

		case 0x401b:
			sound_bankswitch(data);
		return;
	}
}

static UINT8 __fastcall spi_sound_read(UINT16 address)
{
	if ((address & 0xfff0) == 0x6000) {
		return BurnYMF271Read(address & 0xf);
	}

	switch (address)
	{
		case 0x4008:
			return z80_fifoin_pop();

		case 0x4009:
			return fifoin_read_request ? 3 : 1;

		case 0x4013:
			return DrvInputs[3]; // coin

		case 0x400a:
			return DrvDips[0]; // jumpers
	}

	return 0;
}

static void ymf271_external_write(UINT32 address, UINT8 data)
{
	intelflash_write((address >> 20) & 1, address & 0xfffff, data);
}

static UINT8 ymf271_external_read(UINT32 address)
{
	return intelflash_read((address >> 20) & 1, address & 0xfffff);
}

static void spiZ80IRQCallback(INT32, INT32 state)
{
	if (state) ZetSetVector(0xd7);
	ZetSetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static tilemap_callback( text )
{
	UINT16 tile = BURN_ENDIAN_SWAP_INT16(tilemap_ram16[offs + (text_layer_offset * 2)]);

	TILE_SET_INFO(2, tile, tile >> 12, 0);
}

static tilemap_callback( back )
{
	UINT16 tile = BURN_ENDIAN_SWAP_INT16(tilemap_ram16[offs]);

	TILE_SET_INFO(1, (tile & 0x1fff) | back_layer_d14, tile >> 13, 0);
}

static tilemap_callback( midl )
{
	UINT16 tile = BURN_ENDIAN_SWAP_INT16(tilemap_ram16[offs + (midl_layer_offset * 2)]);

	TILE_SET_INFO(1, (tile & 0x1fff) | 0x2000 | midl_layer_d14, (tile >> 13) + 0x10, 0);
}

static tilemap_callback( fore )
{
	UINT16 tile = BURN_ENDIAN_SWAP_INT16(tilemap_ram16[offs + (fore_layer_offset * 2)]);

	TILE_SET_INFO(1, (tile & 0x1fff) | bg_fore_layer_position | fore_layer_d13 | fore_layer_d14, (tile >> 13) + 8, 0);
}

static INT32 SeibuspiIRQCallback(INT32)
{
	return 0x20;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	DrvRecalc = 1; // re-cache the palette (fixes seibu logo fade-in on reset rdftua etc)

	i386Open(0);
	i386Reset();
	i386Close();

	// if flash update gets interrupted, reset will fix hw.err.81
	DrvSndROM[0][0xa00000] = DrvMainROM[0x1ffffc]; // HACK! -set flash region...

	if (sound_system == 0)
	{
		MSM6295Reset(0);
		MSM6295Reset(1);
	}
	else if (sound_system == 1)
	{
		ZetOpen(0);
		ZetReset();
		ZetSetRESETLine(rom_based_z80 ? 0 : 1); // RAM-based starts in reset
		sound_bankswitch(0);
		BurnYMF271Reset();
		ZetClose();

		z80_prog_xfer_pos = 0;

		ds2404Init((UINT8*)DefaultNVRAM, 1995, 1, 1);
	}
	else if (sound_system == 2)
	{
		YMZ280BReset();
	}

	if (has_eeprom) {
		EEPROMReset();

		if (EEPROMAvailable() == 0) {
			EEPROMFill(DefaultEEPROM, 0, 0x80);
		}
	}

	coin_latch = 0;
	input_select = 0;

	video_dma_length = 0;
	video_dma_address = 0;
	rowscroll_enable = 0;
	rf2_layer_bank = 0;
	text_layer_offset = 0;
	fore_layer_offset = 0;
	midl_layer_offset = 0;
	fore_layer_d13 = 0;
	fore_layer_d14 = 0;
	back_layer_d14 = 0;
	midl_layer_d14 = 0;

	fifoin_rpos = 0;
	fifoin_wpos = 0;
	fifoout_rpos = 0;
	fifoout_wpos = 0;
	memset(fifoin_data, 0, sizeof(fifoin_data));
	memset(fifoout_data, 0, sizeof(fifoout_data));
	fifoin_read_request = 0;
	fifoout_read_request = 0;
	z80_prog_xfer_pos = 0;

	nExtraCycles = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM			= Next; Next += 0x200000;

	if (rom_based_z80) {
		DrvZ80RAM		= Next; Next += 0x040000;
	}

	DrvGfxROM[0]		= Next; Next += 0x100000; // must be 1mb or glitches
	DrvGfxROM[1]		= Next; Next += 0x1000000;
	DrvGfxROM[2]		= Next; Next += 0x2000000;

	YMZ280BROM			= Next;
	MSM6295ROM			= Next;
	DrvSndROM[0]		= Next; Next += 0x100000; // allow overflow into DrvSndROM[1] (spi sound is 0xa00000 in size) + 0x100000 for flash, YMZ280B needs 16mb
	DrvSndROM[1]		= Next; Next += 0xf00000;

	DefaultEEPROM		= Next; Next += 0x000080;

	DrvPalette			= (UINT32*)Next; Next += 0x2001 * sizeof(UINT32);

	bitmap32			= (UINT32*)Next; Next += 320 * 256 * sizeof(UINT32);
	DrvAlphaTable       = Next; Next += 0x002000;

	tempdraw            = (UINT16*)Next; Next += 320 * 256 * sizeof(UINT16);

	AllRam				= Next;

	DrvMainRAM			= Next;
	mainram				= (UINT32*)Next; Next += 0x040000;
	palette_ram			= (UINT32*)Next; Next += 0x004000;
	sprite_ram			= (UINT32*)Next; Next += 0x002000;
	tilemap_ram16		= (UINT16*)Next;
	tilemap_ram			= (UINT32*)Next; Next += 0x004000;

	DrvCRTCRAM			= Next; Next += 0x000040;

	if (rom_based_z80 == 0) {
		DrvZ80RAM		= Next; Next += 0x040000;
	}

	DrvZ80WorkRAM		= Next; Next += 0x002000;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static UINT32 partial_carry_sum(UINT32 add1,UINT32 add2,UINT32 carry_mask,INT32 bits)
{
	INT32 res = 0, carry = 0;
	for (INT32 i = 0; i < bits; i++)
	{
		INT32 bit = BIT(add1,i) + BIT(add2,i) + carry;

		res += (bit & 1) << i;

		if (BIT(carry_mask,i))
			carry = bit >> 1;
		else
			carry = 0;
	}

	if (carry) res ^=1;

	return res;
}

static inline UINT32 decrypt_tile(UINT32 val, int tileno, UINT32 key1, UINT32 key2, UINT32 key3)
{
	return partial_carry_sum(BITSWAP24(val, 18,19,9,5, 10,17,16,20, 21,22,6,11, 15,14,4,23, 0,1,7,8, 13,12,3,2), tileno + key1, key2, 24 ) ^ key3;
}

static void decrypt_text(UINT8 *rom, INT32 length, UINT32 key1, UINT32 key2, UINT32 key3)
{
	for (INT32 i = 0; i < length; i++)
	{
		UINT32 w = (rom[(i * 3) + 0] << 16) | (rom[(i * 3) + 1] << 8) | (rom[(i * 3) + 2]);

		w = decrypt_tile(w, i >> 4, key1, key2, key3);

		rom[(i * 3) + 0] = (w >> 16) & 0xff;
		rom[(i * 3) + 1] = (w >> 8) & 0xff;
		rom[(i * 3) + 2] = w & 0xff;
	}
}

static void decrypt_bg(UINT8 *rom, INT32 length, UINT32 key1, UINT32 key2, UINT32 key3)
{
	for (INT32 j = 0; j < length; j += 0xc0000)
	{
		for (INT32 i = 0; i < 0x40000; i++)
		{
			UINT32 w = (rom[j + (i * 3) + 0] << 16) | (rom[j + (i * 3) + 1] << 8) | (rom[j + (i * 3) + 2]);

			w = decrypt_tile(w, i >> 6, key1, key2, key3);

			rom[j + (i * 3) + 0] = (w >> 16) & 0xff;
			rom[j + (i * 3) + 1] = (w >> 8) & 0xff;
			rom[j + (i * 3) + 2] = w & 0xff;
		}
	}
}

static inline void sprite_reorder(UINT8 *buffer)
{
	UINT8 temp[64];

	for (INT32 j = 0; j < 16; j++)
	{
		temp[2 * (j * 2) + 0] = buffer[2 * j + 0];
		temp[2 * (j * 2) + 1] = buffer[2 * j + 1];
		temp[2 * (j * 2) + 2] = buffer[2 * j + 32];
		temp[2 * (j * 2) + 3] = buffer[2 * j + 33];
	}

	memcpy(buffer, temp, 64);
}

static void seibuspi_rise10_sprite_decrypt(UINT8 *rom, INT32 size)
{
	for (INT32 i = 0; i < size / 2; i++)
	{
		UINT32 plane54 = rom[0 * size + 2 * i] + (rom[0 * size + 2 * i + 1] << 8);
		UINT32 plane3210 = BITSWAP32(rom[2 * size + 2 * i] + (rom[2 * size + 2 * i + 1] << 8) + (rom[1 * size + 2 * i] << 16) + (rom[1 * size + 2 * i + 1] << 24),
				23,13,24,4,16,12,25,30,3,5,29,17,14,22,2,11,27,6,15,21,1,28,10,20,7,31,26,0,18,9,19,8);

		plane54   = partial_carry_sum( plane54, 0xabcb, 0x55aa, 16 ) ^ 0x6699;
		plane3210 = partial_carry_sum( plane3210, 0x654321d9 ^ 0x42, 0x1d463748, 32 ) ^ 0x0ca352a9;

		rom[0 * size + 2 * i]     = plane54   >>  8;
		rom[0 * size + 2 * i + 1] = plane54   >>  0;
		rom[1 * size + 2 * i]     = plane3210 >> 24;
		rom[1 * size + 2 * i + 1] = plane3210 >> 16;
		rom[2 * size + 2 * i]     = plane3210 >>  8;
		rom[2 * size + 2 * i + 1] = plane3210 >>  0;
	}

	for (INT32 i = 0; i < size / 2; i += 32)
	{
		sprite_reorder(&rom[0 * size + 2 * i]);
		sprite_reorder(&rom[1 * size + 2 * i]);
		sprite_reorder(&rom[2 * size + 2 * i]);
	}
}

static void seibuspi_rise11_sprite_decrypt(UINT8 *rom, int size, UINT32 k1, UINT32 k2, UINT32 k3, UINT32 k4, UINT32 k5)
{
	for (INT32 i = 0; i < size/2; i++)
	{
		const UINT16 b1 = rom[0 * size + 2 * i] + (rom[0 * size + 2 * i + 1] << 8);
		const UINT16 b2 = rom[1 * size + 2 * i] + (rom[1 * size + 2 * i + 1] << 8);
		const UINT16 b3 = rom[2 * size + 2 * i] + (rom[2 * size + 2 * i + 1] << 8);

		UINT32 plane543 = (BIT(b2,11)<< 0) | (BIT(b1, 6)<< 1) | (BIT(b3,12)<< 2) | (BIT(b3, 3)<< 3) | (BIT(b2,12)<< 4) | (BIT(b3,14)<< 5) | (BIT(b3, 4)<< 6) | (BIT(b1,11)<< 7) |
					      (BIT(b1,12)<< 8) | (BIT(b1, 2)<< 9) | (BIT(b2, 5)<<10) | (BIT(b1, 9)<<11) | (BIT(b3, 1)<<12) | (BIT(b2, 2)<<13) | (BIT(b2,10)<<14) | (BIT(b3, 5)<<15) |
					      (BIT(b1, 3)<<16) | (BIT(b2, 7)<<17) | (BIT(b1,15)<<18) | (BIT(b3, 9)<<19) | (BIT(b2,13)<<20) | (BIT(b1, 4)<<21) | (BIT(b3, 2)<<22) | (BIT(b2, 0)<<23);

		UINT32 plane210 = (BIT(b1,14)<< 0) | (BIT(b1, 1)<< 1) | (BIT(b1,13)<< 2) | (BIT(b3, 0)<< 3) | (BIT(b1, 7)<< 4) | (BIT(b2,14)<< 5) | (BIT(b2, 4)<< 6) | (BIT(b2, 9)<< 7) |
					      (BIT(b3, 8)<< 8) | (BIT(b2, 1)<< 9) | (BIT(b3, 7)<<10) | (BIT(b2, 6)<<11) | (BIT(b1, 0)<<12) | (BIT(b3,11)<<13) | (BIT(b2, 8)<<14) | (BIT(b3,13)<<15) |
					      (BIT(b1, 8)<<16) | (BIT(b3,10)<<17) | (BIT(b3, 6)<<18) | (BIT(b1,10)<<19) | (BIT(b2,15)<<20) | (BIT(b2, 3)<<21) | (BIT(b1, 5)<<22) | (BIT(b3,15)<<23);

		plane543 = partial_carry_sum( plane543, k1, k2, 32 ) ^ k3;
		plane210 = partial_carry_sum( plane210,  i, k4, 24 ) ^ k5;

		rom[0 * size + 2 * i]     = plane543 >> 16;
		rom[0 * size + 2 * i + 1] = plane543 >>  8;
		rom[1 * size + 2 * i]     = plane543 >>  0;
		rom[1 * size + 2 * i + 1] = plane210 >> 16;
		rom[2 * size + 2 * i]     = plane210 >>  8;
		rom[2 * size + 2 * i + 1] = plane210 >>  0;
	}

	for (INT32 i = 0; i < size / 2; i += 32)
	{
		sprite_reorder(&rom[0 * size + 2 * i]);
		sprite_reorder(&rom[1 * size + 2 * i]);
		sprite_reorder(&rom[2 * size + 2 * i]);
	}
}

static const UINT16 key_table[256] = {
	0x3ad7,0x54b1,0x2d41,0x8ca0,0xa69b,0x9018,0x9db9,0x6559,0xe9a7,0xb087,0x8a5e,0x821c,0xaafc,0x2ae7,0x557b,0xcd80,
	0xcfee,0x653e,0x9b31,0x7ab5,0x8b2a,0xbda8,0x707a,0x3c83,0xcbb7,0x7157,0x8226,0x5c4a,0x8bf2,0x6397,0x13e2,0x3102,
	0x8093,0x44cd,0x5f2d,0x7639,0xa7a4,0x9974,0x5263,0x8318,0xb78c,0xa120,0xafb4,0x615f,0x6e0b,0x1d7d,0x8c29,0x4466,
	0x3f35,0x794e,0xaea6,0x601c,0xe478,0xcf6e,0x4ee3,0xa009,0x4b99,0x51d3,0x3474,0x3e4d,0xe5b7,0x9088,0xb5c0,0xba9f,
	0x5646,0xa0af,0x970b,0xb14f,0x8216,0x2386,0x496d,0x9245,0x7e4c,0xad5f,0x89d9,0xb801,0xdf64,0x8ca8,0xe019,0xde9b,
	0x6836,0x70e2,0x7dcd,0x7ac1,0x98ef,0x71aa,0x7d6f,0x70bd,0x9e14,0x75b6,0x8153,0xab6c,0x1f85,0x79cd,0xb2a1,0x934a,
	0x6f74,0x37d7,0xa05a,0x6563,0x1972,0x2dcd,0x7e59,0x6a60,0x5163,0x84c4,0xc451,0x8d80,0x4287,0x57e8,0xacc9,0x539d,
	0xbe71,0xdb7c,0x9424,0xb224,0xcc0f,0xe3dd,0xb79c,0x461e,0x96a9,0x4c7c,0x5443,0x6b2b,0x3cdc,0xbee8,0x2602,0x3282,
	0x7f9c,0x59c3,0xc69a,0x39f4,0x5138,0xb7ca,0x6ca7,0x62e7,0xc455,0x56cf,0x8a9a,0x695c,0x5af2,0xdebf,0x4dbb,0xdaec,
	0xb564,0xc89c,0x7d2d,0x6dc3,0xa15a,0x6584,0xb8ea,0xb7ac,0x88d8,0xc5aa,0x98c5,0xc506,0xc13c,0x7f59,0xab65,0x8fc8,
	0x3a3c,0xd5f6,0x554d,0x5682,0x8ce7,0x40fc,0x8fd7,0x535c,0x6aa0,0x52fe,0x8834,0x5316,0x6c27,0x80a9,0x9e6f,0x2c08,
	0x4092,0xc7c1,0xc468,0x9520,0xbc4d,0xb621,0x3cdb,0xdce8,0x481f,0xd0bd,0x3a57,0x807e,0x3025,0x5aa0,0x5e49,0xa29b,
	0xd2d6,0x7bee,0x97f0,0xe28e,0x2fff,0x48e4,0x6367,0x933f,0x57c5,0x28d4,0x68a0,0xd22e,0x39a6,0x9d2b,0x7a64,0x7e72,
	0x5379,0xe86c,0x7554,0x8fbb,0xc06a,0x9533,0x7eec,0x4d52,0xa800,0x5d35,0xa47d,0xe515,0x8d19,0x703b,0x5a2e,0x627c,
	0x7cea,0x1b2c,0x5a05,0x8598,0x9e00,0xcf01,0x62d9,0x7a10,0x1f42,0x87ce,0x575d,0x6e23,0x86ef,0x93c2,0x3d1a,0x89aa,
	0xe199,0xba1d,0x1b72,0x4513,0x5131,0xc23c,0xba9f,0xa069,0xfbfb,0xda92,0x42b2,0x3a48,0xdb96,0x5fad,0xba96,0xc6eb,
};

static const UINT8 spi_bitswap[16][16] = {
	{ 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, },
	{  7, 6, 5,14, 0,15, 4, 3, 2, 8, 9,10,11,12,13, 1, },
	{  9,15,14,13,12, 0, 1, 2,10, 8, 7, 6, 5, 4, 3,11, },
	{  5, 4, 3, 2, 9,14,13,12,11, 6, 7, 8, 1,15, 0,10, },
	{ 12,11, 0, 7, 8, 5, 6,15,10,13,14, 1, 2, 3, 4, 9, },
	{ 14, 0, 1, 2, 3, 9, 8, 7,15, 5, 6,13,12,11,10, 4, },
	{ 13,12,11,10, 2, 7, 8, 9, 0,14,15, 3, 4, 5, 6, 1, },
	{  2, 9,10,11,12, 7, 6, 5,14, 3, 4, 0,15, 1, 8,13, },
	{  8, 7, 4, 3, 2,13,12,11, 0, 9,10,14,15, 6, 5, 1, },
	{  3, 2,10,11,12, 5,14, 0, 1, 4,15, 6, 7, 8, 9,13, },
	{  2,10, 6, 5, 4,14,13,12,11, 1, 0,15, 9, 8, 7, 3, },
	{ 12,11, 8, 1,15, 3, 2, 9,10,13,14, 4, 5, 6, 7, 0, },
	{  8, 7, 0,11,12, 5, 6,15,14, 9,10, 1, 2, 3, 4,13, },
	{  3, 2, 1, 0,14, 9, 8, 7, 6, 4,15,13,12,11,10, 5, },
	{  2,10,11,12,13, 7, 8, 9,15, 1, 0, 3, 4, 5, 6,14, },
	{ 12,11,10, 9, 2, 7, 6, 5, 4,13,14, 0,15, 1, 8, 3, },
};

static inline INT32 key(INT32 table, INT32 addr)
{
	return BIT(key_table[addr & 0xff] >> 4, table) ^ BIT(addr,(8 + ((table & 0xc) >> 2)));
}

void seibuspi_sprite_decrypt(UINT8 *src, int rom_size)
{
	for (int i = 0; i < rom_size/2; i++)
	{
		const INT32 addr = i >> 8;
		const UINT16 y1 = src[2 * i + 0 * rom_size + 0] + (src[2 * i + 0 * rom_size + 1] << 8);
		const UINT16 y2 = src[2 * i + 1 * rom_size + 0] + (src[2 * i + 1 * rom_size + 1] << 8);
		UINT16 y3       = src[2 * i + 2 * rom_size + 0] + (src[2 * i + 2 * rom_size + 1] << 8);

		const UINT8 *bs = spi_bitswap[key_table[addr & 0xff] & 0xf];
		y3 = BITSWAP16(y3, bs[0],bs[1],bs[2],bs[3],bs[4],bs[5],bs[6],bs[7],
							bs[8],bs[9],bs[10],bs[11],bs[12],bs[13],bs[14],bs[15]);

		UINT32 s1 = (BIT(y1, 4) <<  0) | (BIT(y3, 7) <<  1) | (BIT(y3, 6) <<  2) | (BIT(y2,12) <<  3) | (BIT(y2, 3) <<  4) | (BIT(y1,10) <<  5) | (BIT(y1, 1) <<  6) | (BIT(y3,14) <<  7) |
					(BIT(y3, 2) <<  8) | (BIT(y2, 9) <<  9) | (BIT(y2, 0) << 10) | (BIT(y1, 7) << 11) | (BIT(y3,12) << 12) | (BIT(y2,15) << 13) | (BIT(y2, 6) << 14) | (BIT(y1,13) << 15);

		UINT32 add1 = (BIT(addr,11) <<  0) | (BIT(addr,10) <<  1) | (key(10,addr) <<  2) | (key( 5,addr) <<  3) | (key( 4,addr) <<  4) | (BIT(addr,11) <<  5) | (BIT(addr,11) <<  6) | (key( 7,addr) <<  7) |
				   	  (key( 6,addr) <<  8) | (key( 1,addr) <<  9) | (key( 0,addr) << 10) | (BIT(addr,11) << 11) | (key( 9,addr) << 12) | (key( 8,addr) << 13) | (key( 3,addr) << 14) | (key( 2,addr) << 15);

		UINT32 s2 = (BIT(y1, 5) <<  0) | (BIT(y3, 0) <<  1) | (BIT(y3, 5) <<  2) | (BIT(y2,13) <<  3) | (BIT(y2, 4) <<  4) | (BIT(y1,11) <<  5) | (BIT(y1, 2) <<  6) | (BIT(y3, 9) <<  7) |
				    (BIT(y3, 3) <<  8) | (BIT(y2, 8) <<  9) | (BIT(y1,15) << 10) | (BIT(y1, 6) << 11) | (BIT(y3,11) << 12) | (BIT(y2,14) << 13) | (BIT(y2, 5) << 14) | (BIT(y1,12) << 15) |
				    (BIT(y1, 3) << 16) | (BIT(y3, 8) << 17) | (BIT(y3,15) << 18) | (BIT(y2,11) << 19) | (BIT(y2, 2) << 20) | (BIT(y1, 9) << 21) | (BIT(y1, 0) << 22) | (BIT(y3,10) << 23) |
					(BIT(y3, 1) << 24) | (BIT(y2,10) << 25) | (BIT(y2, 1) << 26) | (BIT(y1, 8) << 27) | (BIT(y3,13) << 28) | (BIT(y3, 4) << 29) | (BIT(y2, 7) << 30) | (BIT(y1,14) << 31);

		UINT32 add2 = (key( 0,addr) <<  0) | (key( 1,addr) <<  1) | (key( 2,addr) <<  2) | (key( 3,addr) <<  3) | (key( 4,addr) <<  4) | (key( 5,addr) <<  5) | (key( 6,addr) <<  6) | (key( 7,addr) <<  7) |
				      (key( 8,addr) <<  8) | (key( 9,addr) <<  9) | (key(10,addr) << 10) | (BIT(addr,10) << 11) | (BIT(addr,11) << 12) | (BIT(addr,11) << 13) | (BIT(addr,11) << 14) | (BIT(addr,11) << 15) |
				      (BIT(addr,11) << 16) | (key( 7,addr) << 17) | (BIT(addr,11) << 18) | (key( 6,addr) << 19) | (BIT(addr,11) << 20) | (key( 5,addr) << 21) | (BIT(addr,11) << 22) | (key( 4,addr) << 23) |
				      (BIT(addr,10) << 24) | (key( 3,addr) << 25) | (key(10,addr) << 26) | (key( 2,addr) << 27) | (key( 9,addr) << 28) | (key( 1,addr) << 29) | (key( 8,addr) << 30) | (key( 0,addr) << 31);

		s1 = partial_carry_sum( s1, add1, 0x3a59, 16 ) ^ 0x843a;
		s2 = partial_carry_sum( s2, add2, 0x28d49cac, 32 ) ^ 0xc8e29f84;

		UINT8 plane0 = 0, plane1 = 0, plane2 = 0, plane3 = 0, plane4 = 0, plane5 = 0;
		for (INT32 j = 0; j < 8; j++)
		{
			plane5 |= (BIT(s1, 2 * j + 1) << j);
			plane4 |= (BIT(s1, 2 * j + 0) << j);
			plane3 |= (BIT(s2, 4 * j + 3) << j);
			plane2 |= (BIT(s2, 4 * j + 2) << j);
			plane1 |= (BIT(s2, 4 * j + 1) << j);
			plane0 |= (BIT(s2, 4 * j + 0) << j);
		}

		src[2 * i + 0 * rom_size + 0] = plane5;
		src[2 * i + 0 * rom_size + 1] = plane4;
		src[2 * i + 1 * rom_size + 0] = plane3;
		src[2 * i + 1 * rom_size + 1] = plane2;
		src[2 * i + 2 * rom_size + 0] = plane1;
		src[2 * i + 2 * rom_size + 1] = plane0;
	}
}

static INT32 DrvGfxDecode(INT32 len0, INT32 len1, INT32 len2)
{
	INT32 Plane0[5]  = { 4, 8, 12, 16, 20 };
	INT32 Plane1[6]  = { 0, 4, 8, 12, 16, 20 };
	INT32 Plane2[6]  = { 0, 8, (((len2 * 8) / 3) * 1) + 0, (((len2 * 8) / 3) * 1) + 8, (((len2 * 8) / 3) * 2) + 0, (((len2 * 8) / 3) * 2) + 8 };
	INT32 XOffs0[8]  = { STEP4(3,-1), STEP4(4*6+3,-1) };
	INT32 XOffs1[16] = { STEP4(3,-1), STEP4(4*6+3,-1), STEP4(4*6*2+3,-1), STEP4(4*6*3+3,-1) };
	INT32 XOffs2[16] = { STEP8(7,-1), STEP8(8*2+7,-1) };
	INT32 YOffs0[8]  = { STEP8(0,4*6*2) };
	INT32 YOffs1[16] = { STEP16(0,4*6*4) };
	INT32 YOffs2[16] = { STEP16(0,8*4) };

	UINT8 *tmp = (UINT8*)BurnMalloc(len2);
	if (tmp == NULL) {
		return 1;
	}

	memcpy(tmp, DrvGfxROM[0], len0);

	GfxDecode(((len0 * 8) / 5) /   (8 * 8), 5,  8,  8, Plane0, XOffs0, YOffs0, 0x180, tmp, DrvGfxROM[0]);

	memcpy(tmp, DrvGfxROM[1], len1);

	GfxDecode(((len1 * 8) / 6) / (16 * 16), 6, 16, 16, Plane1, XOffs1, YOffs1, 0x600, tmp, DrvGfxROM[1]);

	memcpy(tmp, DrvGfxROM[2], len2);

	GfxDecode(((len2 * 8) / 6) / (16 * 16), 6, 16, 16, Plane2, XOffs2, YOffs2, 0x200, tmp, DrvGfxROM[2]);

	BurnFree (tmp);

	return 0;
}

static void sys386fGfxDecode()
{
	INT32 fraq = (0x1000000 * 8) / 4;
	INT32 Planes[8] = { 0, 8, (fraq * 1) + 0, (fraq * 1) + 8, (fraq * 2) + 0, (fraq * 2) + 8, (fraq * 3) + 0, (fraq * 3) + 8 };
	INT32 XOffs[16] = { STEP8(7,-1), STEP8(8*2+7,-1) };
	INT32 YOffs[16] = { STEP16(0,8*4) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x1000000);
	if (tmp == NULL) {
		return;
	}

	BurnByteswap(DrvGfxROM[2], 0x1000000);

	for (INT32 i = 0; i < 0x1000000; i++) {
		tmp[i] = DrvGfxROM[2][(i & 0xffffffc1) | ((i & 2) << 4) | ((i & 0x3c) >> 1)];
	}

	GfxDecode(0x10000, 8, 16, 16, Planes, XOffs, YOffs, 0x200, tmp, DrvGfxROM[2]);

	BurnFree(tmp);
}

static const eeprom_interface seibuspi_eeprom =
{
	6,				// address bits
	16,				// data bits
	"*110",			// read command
	"*101",			// write command
	"*111",			// erase command
	"*10000xxxx",	// lock command
	"*10011xxxx",	// unlock command
	1,				// enable_multi_read
	1				// reset_delay
};

static void graphics_init(INT32 decrypt_type, INT32 graphics_len0, INT32 graphics_len1, INT32 graphics_len2)
{
	switch (decrypt_type)
	{
		case DECRYPT_SEI252: // Senkyu / Viprph1 / Rdft / Rdfts
			decrypt_text(DrvGfxROM[0], graphics_len0, 0x5a3845, 0x77cf5b, 0x1378df);
			decrypt_bg(DrvGfxROM[1], graphics_len1, 0x5a3845, 0x77cf5b, 0x1378df);
			seibuspi_sprite_decrypt(DrvGfxROM[2], 0x400000);
		break;

		case DECRYPT_RISE10: // Rdft2
			decrypt_text(DrvGfxROM[0], graphics_len0, 0x823146, 0x4de2f8, 0x157adc);
			decrypt_bg(DrvGfxROM[1], graphics_len1, 0x823146, 0x4de2f8, 0x157adc);
			seibuspi_rise10_sprite_decrypt(DrvGfxROM[2], 0x600000);
		break;

		case DECRYPT_RISE11: // Rfjet
			decrypt_text(DrvGfxROM[0], graphics_len0, 0xaea754, 0xfe8530, 0xccb666);
			decrypt_bg(DrvGfxROM[1], graphics_len1, 0xaea754, 0xfe8530, 0xccb666);
			seibuspi_rise11_sprite_decrypt(DrvGfxROM[2], 0x800000, 0xabcb64, 0x55aadd, 0xab6a4c, 0xd6375b, 0x8bf23b);
		break;
	}

	DrvGfxDecode(graphics_len0, graphics_len1, graphics_len2);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, text_map_callback,  8,  8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_COLS, back_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_COLS, midl_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(3, TILEMAP_SCAN_COLS, fore_map_callback, 16, 16, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[2], 6, 16, 16, (graphics_len2 * 8) / 6, 0x0000, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 6, 16, 16, (graphics_len1 * 8) / 6, 0x1000, 0x3f);
	GenericTilemapSetGfx(2, DrvGfxROM[0], 5,  8,  8, 0x0040000, 0x1600, 0x3f);
	GenericTilemapSetTransparent(0, 0x1f);
	GenericTilemapSetTransparent(1, 0x3f);
	GenericTilemapSetTransparent(2, 0x3f);
	GenericTilemapSetTransparent(3, 0x3f);
	GenericTilemapSetScrollRows(1, 512);
	GenericTilemapSetScrollRows(2, 512);
	GenericTilemapSetScrollRows(3, 512);
	GenericTilemapBuildSkipTable(0, 2, 0x1f);
	GenericTilemapBuildSkipTable(1, 1, 0x3f);
	GenericTilemapBuildSkipTable(2, 1, 0x3f);
	GenericTilemapBuildSkipTable(3, 1, 0x3f);

	bg_fore_layer_position = (graphics_len1 <= 0x300000) ? 0x2000 : ((graphics_len1 <= 0x600000) ? 0x4000 : 0x8000);
	sprite_ram_size = 0x1000; // always 1000 unless hardware only has sprites...

	memset(DrvAlphaTable, 0, 0x2000);
	memset(DrvAlphaTable + 0x730, 1, 0x10);
	memset(DrvAlphaTable + 0x780, 1, 0x20);
	memset(DrvAlphaTable + 0xfc0, 1, 0x40);
	memset(DrvAlphaTable + 0x1200 + 0x160, 1, 0x20);
	memset(DrvAlphaTable + 0x1200 + 0x1b0, 1, 0x10);
	memset(DrvAlphaTable + 0x1200 + 0x1f0, 1, 0x10);
	memset(DrvAlphaTable + 0x1400 + 0x1b0, 1, 0x10);
	memset(DrvAlphaTable + 0x1400 + 0x1f0, 1, 0x10);
	memset(DrvAlphaTable + 0x1600 + 0x170, 1, 0x10);
	memset(DrvAlphaTable + 0x1600 + 0x1f0, 1, 0x10);
}

static void install_speedhack(UINT32 address, UINT32 pc)
{
	if (address >= 0x40000) return;

	speedhack_address = address;
	speedhack_pc = pc;

	i386Open(0);
	i386MapMemory(NULL,					address & ~0xfff, address | 0xfff, MAP_ROM);
	i386Close();
}

static INT32 DrvLoadRom(bool bLoad)
{
	char *pRomName;
	struct BurnRomInfo ri, ci, di, ei;
	UINT8 *pLoad[4] = { DrvGfxROM[1], DrvGfxROM[2], DrvSndROM[0], DrvGfxROM[0] };
	INT32 sampletype = 0; // 1 = flash, 2 = ymf271/ymz280b, 3 = msm6295
	INT32 samplelength = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);
		BurnDrvGetRomInfo(&ci, i+1);
		BurnDrvGetRomInfo(&di, i+2);
		BurnDrvGetRomInfo(&ei, i+3);

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 1)
		{
			if (ei.nType == ri.nType) // 4x program roms
			{
				if (bLoad) {
					UINT8 *dest = DrvMainROM + ((ri.nLen < 0x80000) ? ((0x80000 - ri.nLen) * 4) : 0); // program roms need to inhabit end of rom space
					if (BurnLoadRomExt(dest + 0, i+0, 4, LD_GROUP(1))) return 1;
					if (BurnLoadRomExt(dest + 1, i+1, 4, LD_GROUP(1))) return 1;
					if (BurnLoadRomExt(dest + 2, i+2, 4, LD_GROUP(1))) return 1;
					if (BurnLoadRomExt(dest + 3, i+3, 4, LD_GROUP(1))) return 1;
				}
				i += 3;
			}
			else if (di.nType == ri.nType) // 3x program roms
			{
				if (bLoad) {
					if (BurnLoadRomExt(DrvMainROM + 0, i+0, 4, LD_GROUP(1))) return 1;
					if (BurnLoadRomExt(DrvMainROM + 1, i+1, 4, LD_GROUP(1))) return 1;
					if (BurnLoadRomExt(DrvMainROM + 2, i+2, 4, LD_GROUP(2))) return 1;
				}
				i += 2;
			}
			else if (ci.nType == ri.nType) // 2x program roms
			{
				if (bLoad) {
					if (BurnLoadRomExt(DrvMainROM + 0, i+0, 4, LD_GROUP(2))) return 1;
					if (BurnLoadRomExt(DrvMainROM + 2, i+1, 4, LD_GROUP(2))) return 1;
				}
				i += 1;
			}

			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 2) { // z80 rom
			if (bLoad) {
				if (BurnLoadRomExt(DrvZ80RAM, i, 1, LD_GROUP(1))) return 1;
			}
			rom_based_z80 = 1;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 7) { // default eeprom
			if (bLoad) {
				if (BurnLoadRomExt(DefaultEEPROM, i, 1, LD_GROUP(1))) return 1;
			}
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 7) == 1) // character roms
		{
			pLoad[3] += 0x30000;
			if (ri.nType == di.nType) // 3x character roms
			{
				if (bLoad) {
					if (BurnLoadRomExt(DrvGfxROM[0] + 1, i+0, 3, LD_GROUP(1))) return 1; // most loaded this way!
					if (BurnLoadRomExt(DrvGfxROM[0] + 0, i+1, 3, LD_GROUP(1))) return 1;
					if (BurnLoadRomExt(DrvGfxROM[0] + 2, i+2, 3, LD_GROUP(1))) return 1;
				}
				i += 2;
			}
			else if (ri.nType == ci.nType) // 2x character roms
			{
				if (bLoad) {
					if (BurnLoadRomExt(DrvGfxROM[0] + 0, i+0, 3, LD_GROUP(2) | LD_BYTESWAP)) return 1;
					if (BurnLoadRomExt(DrvGfxROM[0] + 2, i+1, 3, LD_GROUP(1))) return 1;
				}
				i += 1;
			}

			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 7) == 2) // background tiles
		{
			if (bLoad) {
				if (BurnLoadRomExt(pLoad[0] + 0, i+0, 3, LD_GROUP(2) | LD_BYTESWAP)) return 1;
				if (BurnLoadRomExt(pLoad[0] + 2, i+1, 3, LD_GROUP(1))) return 1;
			}
			pLoad[0] += ri.nLen + ci.nLen;
			i += 1;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 7) == 3) // sprites
		{
			if (bLoad) {
				if (BurnLoadRomExt(pLoad[1], i, 1, LD_GROUP(1))) return 1;
			}
			pLoad[1] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 7) == 3) // ymf samples (flash rom)
		{
			sampletype = 1;
			samplelength += ri.nLen;
			if (bLoad) {
				if (BurnLoadRomExt(DrvSndROM[0] + 0x0000000, i+0, 4, LD_GROUP((ri.nLen <= 0x100000) ? 1 : 2))) return 1;

				memcpy(DrvSndROM[0] + 0x400000, DrvSndROM[0] + 0x200000, 0x200000);
				memset(DrvSndROM[0] + 0x200000, 0, 0x200000);

				if (ci.nType == ri.nType) {
					if (BurnLoadRomExt(DrvSndROM[0] + 0x0800000, i+1, 4, LD_GROUP(1))) return 1;
				}
			}

			if (ci.nType == ri.nType) {
				i += 1;
				samplelength += ci.nLen;
			}

			continue;
		}

		if ((ri.nType & BRF_SND) && (ri.nType & 7) == 1) // ymf samples (no flash rom) / ymz280b samples
		{
			sampletype = 2;
			if (bLoad) {
				if (BurnLoadRomExt(pLoad[2], i, 1, LD_GROUP(1))) return 1;
			}
			pLoad[2] += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_SND) && (ri.nType & 6) == 4) // MSM6295 samples
		{
			sampletype = 3;
			samplelength += ri.nLen;

			if (bLoad) {
				if (BurnLoadRomExt(DrvSndROM[ri.nType & 1], i, 1, LD_GROUP(1))) return 1;
			}
			continue;
		}
	}

	if (!bLoad) {
		graphics_len[0] = pLoad[3] - DrvGfxROM[0];
		graphics_len[1] = pLoad[0] - DrvGfxROM[1];
		graphics_len[2] = pLoad[1] - DrvGfxROM[2];
		bprintf (0, _T("gfx0: %x, gfx1: %x, gfx2: %x\n"), graphics_len[0], graphics_len[1], graphics_len[2]);
		switch (sampletype) {
			case 0: bprintf(0, _T("no samples.\n")); break;
			case 1: bprintf(0, _T("ymf271 flash samples: %x\n"), samplelength); break;
			case 2: bprintf(0, _T("ymf271/ymz280b samples: %x\n"), pLoad[2] - DrvSndROM[0]); break;
			case 3: bprintf(0, _T("msm6295 samples: %x\n"), samplelength); break;
		}
		if (rom_based_z80) bprintf (0, _T("Has ROM-based Z80\n"));
	}

	return 0;
}

static INT32 CommonInit(INT32 decrypt_type, void (*pCallback)(), UINT32 speedhack_addr, UINT32 speedhack_pc_val)
{
	BurnSetRefreshRate(54.00);

	DrvLoadRom(false);
	BurnAllocMemIndex();
	DrvLoadRom(true);

	i386Init(0);
	i386Open(0);
	i386MapMemory(DrvMainRAM + 0x1000,	0x00001000, 0x0003ffff, MAP_RAM);
	i386MapMemory(DrvMainROM,			0x00200000, 0x003fffff, MAP_ROM);
	i386MapMemory(DrvSndROM[0],			0x00a00000, 0x013fffff, MAP_ROM);
	i386MapMemory(DrvMainROM,			0xffe00000, 0xffffffff, MAP_ROM);
	i386SetReadHandlers(common_read_byte, common_read_word, spi_read_dword);
	i386SetWriteHandlers(spi_write_byte, spi_write_word, spi_write_dword);
	i386SetIRQCallback(SeibuspiIRQCallback);
	i386Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80RAM,				0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80WorkRAM,			0x2000, 0x3fff, MAP_RAM);
	ZetSetWriteHandler(spi_sound_write);
	ZetSetReadHandler(spi_sound_read);
	ZetClose();

	intelflash_init(0, FLASH_INTEL_E28F008SA, DrvSndROM[0] + 0xa00000 );
	intelflash_init(1, FLASH_INTEL_E28F008SA, DrvSndROM[0] + 0xb00000 );

	DrvSndROM[0][0xa00000] = DrvMainROM[0x1ffffc]; // HACK! -set flash region...

	BurnYMF271Init(16934400, DrvSndROM[0], 0x280000, spiZ80IRQCallback, 0);
#if 0
	BurnYMF271SetRoute(0, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYMF271SetRoute(1, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnYMF271SetRoute(2, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYMF271SetRoute(3, 1.00, BURN_SND_ROUTE_RIGHT);
#endif
	BurnYMF271SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH); // sounds better MONO.
	BurnTimerAttachZet(7159090);

	ymf271_set_external_handlers(rom_based_z80 ? NULL : ymf271_external_read, rom_based_z80 ? NULL : ymf271_external_write);
	sound_system = 1;

	graphics_init(decrypt_type, graphics_len[0], graphics_len[1], graphics_len[2]);

	if (speedhack_addr) install_speedhack(speedhack_addr, speedhack_pc_val);

	if (pCallback) {
		pCallback();
	}

	DrvDoReset(1);

	return 0;
}

static INT32 Sys386fInit()
{
	BurnSetRefreshRate(54.00);

	sound_system = 2;

	DrvLoadRom(false);
	BurnAllocMemIndex();
	DrvLoadRom(true);

	sys386fGfxDecode();
	sprite_ram_size = 0x2000;

	i386Init(0);
	i386Open(0);
	i386MapMemory(DrvMainRAM + 0x1000,	0x00001000, 0x0003ffff, MAP_RAM);
	i386MapMemory(DrvMainROM,			0x00200000, 0x003fffff, MAP_ROM);
	i386MapMemory(DrvMainROM,			0xffe00000, 0xffffffff, MAP_ROM);
	i386SetReadHandlers(common_read_byte, common_read_word, sys386f_read_dword);
	i386SetWriteHandlers(common_write_byte, sys386f_write_word, common_write_dword);
	i386SetIRQCallback(SeibuspiIRQCallback);
	i386Close();

	EEPROMInit(&seibuspi_eeprom);
	has_eeprom = 1;

	YMZ280BInit(16934400, NULL);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_1, 0.80, BURN_SND_ROUTE_LEFT);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_2, 0.80, BURN_SND_ROUTE_RIGHT);

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM[2], 8, 16, 16, 0x1000000, 0x0000, 0x1f);

//	install_speedhack(0, 0);

	DrvDoReset(1);

	return 0;
}

static INT32 Sys368iCommonInit(INT32 decrypt_type, void (*pCallback)(), UINT32 speedhack_addr, UINT32 speedhack_pc_val)
{
	BurnSetRefreshRate(54.00);

	DrvLoadRom(false);
	BurnAllocMemIndex();
	DrvLoadRom(true);

	i386Init(0);
	i386Open(0);
	i386MapMemory(DrvMainRAM + 0x1000,	0x00001000, 0x0003ffff, MAP_RAM);
	i386MapMemory(DrvMainROM,			0x00200000, 0x003fffff, MAP_ROM);
	i386MapMemory(DrvMainROM,			0xffe00000, 0xffffffff, MAP_ROM);
	i386SetReadHandlers(common_read_byte, common_read_word, spi_i386_read_dword);
	i386SetWriteHandlers(common_write_byte, spi_i386_write_word, spi_i386_write_dword);
	i386SetIRQCallback(SeibuspiIRQCallback);
	i386Close();

	install_speedhack(speedhack_addr, speedhack_pc_val);

	EEPROMInit(&seibuspi_eeprom);
	has_eeprom = 1;

	MSM6295Init(0, 1431818 / MSM6295_PIN7_HIGH, 0);
	MSM6295Init(1, 1431818 / MSM6295_PIN7_HIGH, 0);
	MSM6295SetRoute(0, 0.47, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.47, BURN_SND_ROUTE_BOTH);

	sound_system = 0;

	if (pCallback) {
		pCallback();
	}

	graphics_init(decrypt_type, graphics_len[0], graphics_len[1], graphics_len[2]);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	i386Exit();

	if (sound_system == 0) {
		MSM6295Exit(0);
		MSM6295Exit(1);
	}
	else if (sound_system == 1)
	{
#if 0
		char name[128];
		sprintf (name, "%s-flash0.bin", BurnDrvGetTextA(DRV_NAME));
		FILE *fa = fopen(name, "wb");
		fwrite (DrvSndROM[0] + 0xa00000, 0x100000, 1, fa);
		fclose (fa);

		sprintf (name, "%s-flash1.bin", BurnDrvGetTextA(DRV_NAME));

		fa = fopen(name, "wb");
		fwrite (DrvSndROM[0] + 0xb00000, 0x100000, 1, fa);
		fclose (fa);
#endif
		ZetExit();
		BurnYMF271Exit();
		intelflash_exit();
	}
	else if (sound_system == 2)
	{
		YMZ280BExit();
		YMZ280BROM = NULL;
	}

	if (has_eeprom) EEPROMExit();

	sound_system = 0;
	rom_based_z80 = 0;
	has_eeprom = 0;
	DefaultNVRAM = NULL;

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < BurnDrvGetPaletteEntries()/2; i++)
	{
		UINT32 color = BURN_ENDIAN_SWAP_INT32(palette_ram[i]);

		DrvPalette[(i * 2) + 0] = (pal5bit(color >>  0) << 16) | (pal5bit(color >>  5) << 8) | pal5bit(color >> 10);
		DrvPalette[(i * 2) + 1] = (pal5bit(color >> 16) << 16) | (pal5bit(color >> 21) << 8) | pal5bit(color >> 26);
	}

	DrvPalette[0x2000] = 0; // black
}

static inline UINT32 alpha_blend(UINT32 d, UINT32 s)
{
	return (((((s & 0xff00ff) * 0x7f) + ((d & 0xff00ff) * 0x81)) & 0xff00ff00) +
		((((s & 0x00ff00) * 0x7f) + ((d & 0x00ff00) * 0x81)) & 0x00ff0000)) / 0x100;
}

static void drawgfx_blend(GenericTilesGfx *gfx, UINT32 code, UINT32 color, INT32 flipx, INT32 flipy, INT32 sx, INT32 sy, UINT8 primask)
{
	const INT32 width = gfx->width;
	const INT32 height = gfx->height;

	INT32 x1 = sx;
	INT32 x2 = sx + width - 1;
	INT32 y1 = sy;
	INT32 y2 = sy + height - 1;

	if (x1 > (nScreenWidth - 1) || x2 < 0) {
		return;
	}

	if (y1 > (nScreenHeight - 1) || y2 < 0) {
		return;
	}

	INT32 px = 0;
	INT32 py = 0;
	INT32 xd = 1;
	INT32 yd = 1;

	if (flipx)
	{
		xd = -xd;
		px = width - 1;
	}

	if (flipy)
	{
		yd = -yd;
		py = height - 1;
	}

	if (x1 < 0)
	{
		if (flipx)
		{
			px = width - (0 - x1) - 1;
		}
		else
		{
			px = (0 - x1);
		}
		x1 = 0;
	}

	if (x2 > (nScreenWidth - 1))
	{
		x2 = (nScreenWidth - 1);
	}

	if (y1 < 0)
	{
		if (flipy)
		{
			py = height - (0 - y1) - 1;
		}
		else
		{
			py = (0 - y1);
		}
		y1 = 0;
	}

	if (y2 > (nScreenHeight - 1))
	{
		y2 = (nScreenHeight - 1);
	}

	color = gfx->color_offset + ((color & gfx->color_mask) << gfx->depth);
	const UINT8 *src = gfx->gfxbase + ((code % gfx->code_mask) * gfx->width * gfx->height);
	const UINT8 trans_pen = (1 << gfx->depth) - 1;

	for (int y = y1; y <= y2; y++)
	{
		UINT32 *pal = DrvPalette;
		UINT32 *dest = bitmap32 + y * nScreenWidth;
		UINT8 *pri = pPrioDraw + y * nScreenWidth;
		int src_i = (py * width) + px;
		py += yd;

		for (int x = x1; x <= x2; x++)
		{
			const UINT8 pen = src[src_i];
			if (!(pri[x] & primask) && pen != trans_pen)
			{
				pri[x] |= primask;
				const UINT16 global_pen = pen + color;
				if (DrvAlphaTable[global_pen])
					dest[x] = alpha_blend(dest[x], pal[global_pen]);
				else
					dest[x] = pal[global_pen];
			}
			src_i += xd;
		}
	}
}

static void draw_sprites(UINT32 priority)
{
	if ((nSpriteEnable & 1) == 0 || (layer_enable & 0x10)) return;

	GenericTilesGfx *gfx = &GenericGfxData[0];

	for (INT32 a = 0; a < sprite_ram_size / 4; a += 2)
	{
		INT32 code = (BURN_ENDIAN_SWAP_INT32(sprite_ram[a + 0]) >> 16) | ((BURN_ENDIAN_SWAP_INT32(sprite_ram[a + 1]) & 0x1000) << 4);
		if ((code % gfx->code_mask) == 0) continue; // speed-up

		if (priority != ((BURN_ENDIAN_SWAP_INT32(sprite_ram[a + 0]) >> 6) & 0x3))
			continue;

		INT16 xpos  = BURN_ENDIAN_SWAP_INT32(sprite_ram[a + 1]) & 0x3ff;
		INT16 ypos  = BURN_ENDIAN_SWAP_INT32(sprite_ram[a + 1]) >> 16 & 0x1ff;
		INT32 color = BURN_ENDIAN_SWAP_INT32(sprite_ram[a + 0]) & 0x3f;
		if (xpos & 0x200) xpos |= 0xfc00;
		if (ypos & 0x100) ypos |= 0xfe00;

		INT32 width  =((BURN_ENDIAN_SWAP_INT32(sprite_ram[a + 0]) >>  8) & 7) + 1;
		INT32 height =((BURN_ENDIAN_SWAP_INT32(sprite_ram[a + 0]) >> 12) & 7) + 1;
		INT32 flip_x = (BURN_ENDIAN_SWAP_INT32(sprite_ram[a + 0]) >> 11) & 1;
		INT32 flip_y = (BURN_ENDIAN_SWAP_INT32(sprite_ram[a + 0]) >> 15) & 1;
		INT32 x1 = 0;
		INT32 y1 = 0;
		INT32 flip_y_pos = 0;
		INT32 flip_x_pos = 0;

		if (flip_x)
		{
			x1 = 8 - width;
			width = width + x1;
			flip_x_pos = 0x70;
		}

		if (flip_y)
		{
			y1 = 8 - height;
			height = height + y1;
			flip_y_pos = 0x70;
		}

		for (INT32 x = x1; x < width; x++)
		{
			for (INT32 y = y1; y < height; y++, code++)
			{
#if 0
// wrong, but good for testing...
				RenderPrioSprite(pTransDraw, gfx->gfxbase, code % gfx->code_mask, ((color & gfx->color_mask) << gfx->depth) + gfx->color_offset, (1 << gfx->depth) - 1, xpos + ((x ^ (flip_x * 7)) * 16), ypos + ((y ^ (flip_y * 7)) * 16), flip_x, flip_y, gfx->width, gfx->height, 1 << priority);

				if ((xpos + (16 * x) + 16) >= 512)
						RenderPrioSprite(pTransDraw, gfx->gfxbase, code % gfx->code_mask, ((color & gfx->color_mask) << gfx->depth) + gfx->color_offset, (1 << gfx->depth) - 1, xpos - 512 + ((x ^ (flip_x * 7)) * 16), ypos + ((y ^ (flip_y * 7)) * 16), flip_x, flip_y, gfx->width, gfx->height, 1 << priority);
#else
				drawgfx_blend(gfx, code, color, flip_x, flip_y, xpos + ((x * 16) ^ flip_x_pos), ypos + ((y * 16) ^ flip_y_pos), 1 << priority);

				if ((xpos + (16 * x) + 16) >= 512)
					drawgfx_blend(gfx, code, color, flip_x, flip_y, xpos - 512 + ((x * 16) ^ flip_x_pos), ypos + ((y * 16) ^ flip_y_pos), 1 << priority);
#endif
			}
		}
	}
}

static void DrvTransferBitmap32()
{
	switch (nBurnBpp) {
		case 4:
			memcpy(pBurnDraw, bitmap32, nScreenHeight * nScreenWidth * sizeof(UINT32));
			break;

		default:
			for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
				PutPix(pBurnDraw + (i * nBurnBpp), BurnHighCol(bitmap32[i]>>16, (bitmap32[i]>>8)&0xff, bitmap32[i]&0xff, 0));
			}
			break;
	}
}

static void mix_in_tmap(INT32 layer, INT32 flags)
{
	// clear temp bitmap
	memset(tempdraw, 0, 320 * 256 * sizeof(UINT16));

	// draw tilemap
	GenericTilemapDraw(layer, tempdraw, flags, 0xff);

	// mix it in, applying alpha where needed
	UINT16 *src0 = tempdraw;
	UINT32 *dest = (UINT32*)bitmap32;

	for (INT32 y = 0; y < nScreenHeight; y++) {
		for (INT32 x = 0; x < nScreenWidth; x++) {
			UINT16 src = src0[x];
			if (src) {
				if (DrvAlphaTable[src])
					dest[x] = alpha_blend(dest[x], DrvPalette[src]);
				else
					dest[x] = DrvPalette[src];
			}
		}
		src0 += nScreenWidth;
		dest += nScreenWidth;
	}

	pBurnDrvPalette = DrvPalette;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	UINT16 *crtc = (UINT16*)DrvCRTCRAM;

	layer_enable = BURN_ENDIAN_SWAP_INT16(crtc[0x1c/2]);

	BurnPrioClear();

	GenericTilemapSetScrollY(1, BURN_ENDIAN_SWAP_INT16(crtc[0x22/2]));
	GenericTilemapSetScrollY(2, BURN_ENDIAN_SWAP_INT16(crtc[0x26/2]));
	GenericTilemapSetScrollY(3, BURN_ENDIAN_SWAP_INT16(crtc[0x2a/2]));

	if (rowscroll_enable)
	{
		INT16 *back_rowscroll = (INT16*)&tilemap_ram[0x200];
		INT16 *midl_rowscroll = (INT16*)&tilemap_ram[0x600];
		INT16 *fore_rowscroll = (INT16*)&tilemap_ram[0xa00];

		GenericTilemapSetScrollRows(1, 512);
		GenericTilemapSetScrollRows(2, 512);
		GenericTilemapSetScrollRows(3, 512);

		for (INT32 y = 0; y < 512; y++) {
			GenericTilemapSetScrollRow(1, y, BURN_ENDIAN_SWAP_INT16(crtc[0x20/2]) + BURN_ENDIAN_SWAP_INT16(back_rowscroll[(0x19 + y) & 0x1ff]));
			GenericTilemapSetScrollRow(2, y, BURN_ENDIAN_SWAP_INT16(crtc[0x24/2]) + BURN_ENDIAN_SWAP_INT16(midl_rowscroll[(0x19 + y) & 0x1ff]));
			GenericTilemapSetScrollRow(3, y, BURN_ENDIAN_SWAP_INT16(crtc[0x28/2]) + BURN_ENDIAN_SWAP_INT16(fore_rowscroll[(0x19 + y) & 0x1ff]));
		}
	} else {
		GenericTilemapSetScrollRows(1, 1);
		GenericTilemapSetScrollRows(2, 1);
		GenericTilemapSetScrollRows(3, 1);

		GenericTilemapSetScrollX(1, BURN_ENDIAN_SWAP_INT16(crtc[0x20 / 2]));
		GenericTilemapSetScrollX(2, BURN_ENDIAN_SWAP_INT16(crtc[0x24 / 2]));
		GenericTilemapSetScrollX(3, BURN_ENDIAN_SWAP_INT16(crtc[0x28/2]));
	}

	memset(bitmap32, 0x00, 320 * 256 * sizeof(UINT32));

	if (~layer_enable & 1 && nBurnLayer & 1)
		mix_in_tmap(1, TMAP_FORCEOPAQUE);

	draw_sprites(0);

	if ((layer_enable & 0x15) == 0 && nSpriteEnable & 1)
		mix_in_tmap(1, 0);

	if (~layer_enable & 4)
		draw_sprites(1);

	if (~layer_enable & 2 && nBurnLayer & 2)
		mix_in_tmap(2, 0);

	if (layer_enable & 4)
		draw_sprites(1);

	draw_sprites(2);

	if (~layer_enable & 4 && nBurnLayer & 4)
		mix_in_tmap(3, 0);

	draw_sprites(3);

	if (~layer_enable & 8 && nBurnLayer & 8)
		mix_in_tmap(0, 0);

	DrvTransferBitmap32();

	return 0;
}

static INT32 Sys386fDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
	}

	UINT16 *crtc = (UINT16*)DrvCRTCRAM;

	layer_enable = BURN_ENDIAN_SWAP_INT16(crtc[0x1c/2]);

	BurnPrioClear();

	memset(bitmap32, 0x00, 320 * 256 * sizeof(UINT32));

	for (INT32 i = 0; i < 4; i++) {
		draw_sprites(i);
	}

	DrvTransferBitmap32();

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	i386NewFrame();
	ZetNewFrame();

	{
		memset(DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (BurnDrvGetGenreFlags() & GBF_MAHJONG) {
			for (INT32 i = 0; i < 8; i++) {
				DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
				DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
				DrvInputs[6] ^= (DrvJoy7[i] & 1) << i;
				DrvInputs[7] ^= (DrvJoy8[i] & 1) << i;
				DrvInputs[8] ^= (DrvJoy9[i] & 1) << i;
			}
		}
	}

	INT32 nInterleave = 296; // vtotal
	INT32 nCyclesTotal[2] = { 25000000 / 54 /*(((28636363)/4)/448)/296*/, 7159090 / 54 };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	i386Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, i386);
		CPU_RUN_TIMER(1);

		ds2404_counter++;
		if (ds2404_counter == 63) { // ~256 / sec
			ds2404_counter = 0;
			ds2404_timer_update();
		}

		if (i == 239) {
			i386SetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
	}

	if (pBurnSoundOut) {
		BurnYMF271Update(nBurnSoundLen);
	}

	ZetClose();
	i386Close();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 Sys386Frame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	i386NewFrame();

	{
		memset(DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 296; // vtotal
	INT32 nCyclesTotal[1] = { 40000000 / 54 /*(((28636363)/4)/448)/296*/ }; // sys386i others are 25mhz
	INT32 nCyclesDone[1] = { nExtraCycles };

	i386Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, i386);

		if (i == 239) {
			i386SetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
	}

	i386Close();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 Sys386fFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	i386NewFrame();

	{
		memset(DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (BurnDrvGetGenreFlags() & GBF_MAHJONG) {
			for (INT32 i = 0; i < 16; i++) {
				DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
				DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
				DrvInputs[6] ^= (DrvJoy7[i] & 1) << i;
				DrvInputs[7] ^= (DrvJoy8[i] & 1) << i;
				DrvInputs[8] ^= (DrvJoy9[i] & 1) << i;
			}
		}
	}

	INT32 nInterleave = 296; // vtotal
	INT32 nCyclesTotal[1] = { 25000000 / 54 /*(((28636363)/4)/448)/296*/ };
	INT32 nCyclesDone[1] = { nExtraCycles };

	i386Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, i386);

		if (i == 239) {
			i386SetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
	}

	i386Close();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnSoundOut) {
		YMZ280BRender(pBurnSoundOut, nBurnSoundLen);
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

		i386Scan(nAction);

		if (sound_system == 0) MSM6295Scan(nAction, pnMin);

		if (sound_system == 1) ZetScan(nAction);
		if (sound_system == 1) BurnYMF271Scan(nAction, pnMin);
		if (sound_system == 1 && rom_based_z80 == 0 && has_eeprom == 0) intelflash_scan(nAction, pnMin);
		if (sound_system == 2) YMZ280BScan(nAction, pnMin);

		SCAN_VAR(video_dma_length);
		SCAN_VAR(video_dma_address);
		SCAN_VAR(rowscroll_enable);
		SCAN_VAR(rf2_layer_bank);
		SCAN_VAR(text_layer_offset);
		SCAN_VAR(fore_layer_offset);
		SCAN_VAR(midl_layer_offset);
		SCAN_VAR(fore_layer_d13);
		SCAN_VAR(fore_layer_d14);
		SCAN_VAR(back_layer_d14);
		SCAN_VAR(midl_layer_d14);
		SCAN_VAR(fifoin_rpos);
		SCAN_VAR(fifoin_wpos);
		SCAN_VAR(fifoout_rpos);
		SCAN_VAR(fifoout_wpos);
		SCAN_VAR(fifoin_data);
		SCAN_VAR(fifoout_data);
		SCAN_VAR(fifoin_read_request);
		SCAN_VAR(fifoout_read_request);
		SCAN_VAR(z80_prog_xfer_pos);
		SCAN_VAR(z80_bank);
		SCAN_VAR(oki_bank);
		SCAN_VAR(coin_latch);
		SCAN_VAR(input_select);

		SCAN_VAR(nExtraCycles);
	}

	if (has_eeprom)
		EEPROMScan(nAction, pnMin);
	else
		ds2404_scan(nAction, pnMin);

	if (nAction & ACB_NVRAM)
	{
		if (sound_system == 1 && rom_based_z80 == 0 && has_eeprom == 0) intelflash_scan(nAction, pnMin);
	}

	if (nAction & ACB_WRITE) {
		if (sound_system == 0) {
			oki_bankswitch(oki_bank);
		}
		else if (sound_system == 1) {
			ZetOpen(0);
			sound_bankswitch(z80_bank);
			ZetClose();
			// if flash update gets interrupted, this will fix it so it can be re-flased.
			DrvSndROM[0][0xa00000] = DrvMainROM[0x1ffffc]; // HACK! -set flash region...
		}
	}

	return 0;
}


// Senkyu (Japan, newer)

static struct BurnRomInfo senkyuRomDesc[] = {
	{ "fb_1.211",						0x040000, 0x20a3e5db, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "fb_2.212",						0x040000, 0x38e90619, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fb_3.210",						0x040000, 0x226f0429, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fb_4.29",						0x040000, 0xb46d66b7, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fb_6.413",						0x020000, 0xb57115c9, 1 | BRF_GRA },           //  4 Characters
	{ "fb_5.48",						0x010000, 0x440a9ae3, 1 | BRF_GRA },           //  5

	{ "fb_bg-1d.415",					0x200000, 0xeae7a1fc, 2 | BRF_GRA },           //  6 Background Tiles
	{ "fb_bg-1p.410",					0x100000, 0xb46e774e, 2 | BRF_GRA },           //  7

	{ "fb_obj-1.322",					0x400000, 0x29f86f68, 3 | BRF_GRA },           //  8 Sprites
	{ "fb_obj-2.324",					0x400000, 0xc9e3130b, 3 | BRF_GRA },           //  9
	{ "fb_obj-3.323",					0x400000, 0xf6c3bc49, 3 | BRF_GRA },           // 10

	{ "fb_pcm-1.215",					0x100000, 0x1d83891c, 3 | BRF_PRG | BRF_ESS }, // 11 Sample Data (Mapped to i386)
	{ "fb_7.216",						0x080000, 0x874d7b59, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 13 Intel Flash (Samples)
};

STD_ROM_PICK(senkyu)
STD_ROM_FN(senkyu)

static INT32 SenkyuInit()
{
	return CommonInit(DECRYPT_SEI252, NULL, 0x18cb4, 0x305bb2);
}

struct BurnDriver BurnDrvSenkyu = {
	"senkyu", NULL, NULL, NULL, "1995",
	"Senkyu (Japan, newer)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, senkyuRomInfo, senkyuRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	SenkyuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 240, 4, 3
};


// Senkyu (Japan, earlier)

static struct BurnRomInfo senkyuaRomDesc[] = {
	{ "1.bin",							0x040000, 0x6102c3fb, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "2.bin",							0x040000, 0xd5b8ce46, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",							0x040000, 0xe27ceccd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.bin",							0x040000, 0x7c6d4549, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fb_6.413",						0x020000, 0xb57115c9, 1 | BRF_GRA },           //  4 Characters
	{ "fb_5.48",						0x010000, 0x440a9ae3, 1 | BRF_GRA },           //  5

	{ "fb_bg-1d.415",					0x200000, 0xeae7a1fc, 2 | BRF_GRA },           //  6 Background Tiles
	{ "fb_bg-1p.410",					0x100000, 0xb46e774e, 2 | BRF_GRA },           //  7

	{ "fb_obj-1.322",					0x400000, 0x29f86f68, 3 | BRF_GRA },           //  8 Sprites
	{ "fb_obj-2.324",					0x400000, 0xc9e3130b, 3 | BRF_GRA },           //  9
	{ "fb_obj-3.323",					0x400000, 0xf6c3bc49, 3 | BRF_GRA },           // 10

	{ "fb_pcm-1.215",					0x100000, 0x1d83891c, 3 | BRF_PRG | BRF_ESS }, // 11 Sample Data (Mapped to i386)
	{ "fb_7.216",						0x080000, 0x874d7b59, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 13 Intel Flash (Samples)
};

STD_ROM_PICK(senkyua)
STD_ROM_FN(senkyua)

static INT32 SenkyuaInit()
{
	return CommonInit(DECRYPT_SEI252, NULL, 0x18c9c, 0x30582e);
}

struct BurnDriver BurnDrvSenkyua = {
	"senkyua", "senkyu", NULL, NULL, "1995",
	"Senkyu (Japan, earlier)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, senkyuaRomInfo, senkyuaRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	SenkyuaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 240, 4, 3
};


// Battle Balls (Germany, newer)

static struct BurnRomInfo batlballRomDesc[] = {
	{ "1.211",							0x040000, 0xd4e48f89, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "2.212",							0x040000, 0x3077720b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.210",							0x040000, 0x520d31e1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.029",							0x040000, 0x22419b78, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fb_6.413",						0x020000, 0xb57115c9, 1 | BRF_GRA },           //  4 Characters
	{ "fb_5.48",						0x010000, 0x440a9ae3, 1 | BRF_GRA },           //  5

	{ "fb_bg-1d.415",					0x200000, 0xeae7a1fc, 2 | BRF_GRA },           //  6 Background Tiles
	{ "fb_bg-1p.410",					0x100000, 0xb46e774e, 2 | BRF_GRA },           //  7

	{ "fb_obj-1.322",					0x400000, 0x29f86f68, 3 | BRF_GRA },           //  8 Sprites
	{ "fb_obj-2.324",					0x400000, 0xc9e3130b, 3 | BRF_GRA },           //  9
	{ "fb_obj-3.323",					0x400000, 0xf6c3bc49, 3 | BRF_GRA },           // 10

	{ "fb_pcm-1.215",					0x100000, 0x1d83891c, 3 | BRF_PRG | BRF_ESS }, // 11 Sample Data (Mapped to i386)
	{ "fb_7.216",						0x080000, 0x874d7b59, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "flash0_blank_region80.u1053",	0x100000, 0xe2adaff5, 0 | BRF_SND },           // 13 Intel Flash (Samples)
};

STD_ROM_PICK(batlball)
STD_ROM_FN(batlball)

static INT32 BatlballInit()
{
	return CommonInit(DECRYPT_SEI252, NULL, 0x18db4, 0x3058aa);
}

struct BurnDriver BurnDrvBatlball = {
	"batlball", "senkyu", NULL, NULL, "1995",
	"Battle Balls (Germany, newer)\0", NULL, "Seibu Kaihatsu (Tuning license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, batlballRomInfo, batlballRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	BatlballInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 240, 4, 3
};


// Battle Balls (Germany, earlier)

static struct BurnRomInfo batlballoRomDesc[] = {
	{ "seibu_1a.211",					0x040000, 0x90340e8c, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu_2a.212",					0x040000, 0xdb655d3e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu_3a.210",					0x040000, 0x659a54a2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu_4a.029",					0x040000, 0x51183421, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_6.413",					0x020000, 0x338556f9, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_5.48",						0x010000, 0x6ccfb72e, 1 | BRF_GRA },           //  5

	{ "fb_bg-1d.415",					0x200000, 0xeae7a1fc, 2 | BRF_GRA },           //  6 Background Tiles
	{ "fb_bg-1p.410",					0x100000, 0xb46e774e, 2 | BRF_GRA },           //  7

	{ "fb_obj-1.322",					0x400000, 0x29f86f68, 3 | BRF_GRA },           //  8 Sprites
	{ "fb_obj-2.324",					0x400000, 0xc9e3130b, 3 | BRF_GRA },           //  9
	{ "fb_obj-3.323",					0x400000, 0xf6c3bc49, 3 | BRF_GRA },           // 10

	{ "fb_pcm-1.215",					0x100000, 0x1d83891c, 3 | BRF_PRG | BRF_ESS }, // 11 Sample Data (Mapped to i386)
	{ "seibu_7.216",					0x080000, 0x874d7b59, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "flash0_blank_region80.u1053",	0x100000, 0xe2adaff5, 0 | BRF_SND },           // 13 Intel Flash (Samples)
};

STD_ROM_PICK(batlballo)
STD_ROM_FN(batlballo)

static INT32 BatlballoInit()
{
	return CommonInit(DECRYPT_SEI252, NULL, 0x18db4, 0x3058aa);
}

struct BurnDriver BurnDrvBatlballo = {
	"batlballo", "senkyu", NULL, NULL, "1995",
	"Battle Balls (Germany, earlier)\0", NULL, "Seibu Kaihatsu (Tuning license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, batlballoRomInfo, batlballoRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	BatlballoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 240, 4, 3
};


// Battle Balls (Hong Kong)

static struct BurnRomInfo batlballaRomDesc[] = {
	{ "senkyua1.bin",					0x040000, 0xec3c4d4d, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "2.212",							0x040000, 0x3077720b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.210",							0x040000, 0x520d31e1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.029",							0x040000, 0x22419b78, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fb_6.413",						0x020000, 0xb57115c9, 1 | BRF_GRA },           //  4 Characters
	{ "fb_5.48",						0x010000, 0x440a9ae3, 1 | BRF_GRA },           //  5

	{ "fb_bg-1d.415",					0x200000, 0xeae7a1fc, 2 | BRF_GRA },           //  6 Background Tiles
	{ "fb_bg-1p.410",					0x100000, 0xb46e774e, 2 | BRF_GRA },           //  7

	{ "fb_obj-1.322",					0x400000, 0x29f86f68, 3 | BRF_GRA },           //  8 Sprites
	{ "fb_obj-2.324",					0x400000, 0xc9e3130b, 3 | BRF_GRA },           //  9
	{ "fb_obj-3.323",					0x400000, 0xf6c3bc49, 3 | BRF_GRA },           // 10

	{ "fb_pcm-1.215",					0x100000, 0x1d83891c, 3 | BRF_PRG | BRF_ESS }, // 11 Sample Data (Mapped to i386)
	{ "fb_7.216",						0x080000, 0x874d7b59, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "flash0_blank_region22.u1053",	0x100000, 0x5fee8413, 0 | BRF_SND },           // 13 Intel Flash (Samples)
};

STD_ROM_PICK(batlballa)
STD_ROM_FN(batlballa)

static INT32 BatlballaInit()
{
	return CommonInit(DECRYPT_SEI252, NULL, 0x18db4, 0x3058aa);
}

struct BurnDriver BurnDrvBatlballa = {
	"batlballa", "senkyu", NULL, NULL, "1995",
	"Battle Balls (Hong Kong)\0", NULL, "Seibu Kaihatsu (Metrotainment license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, batlballaRomInfo, batlballaRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	BatlballaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 240, 4, 3
};


// Battle Balls (Hong Kong, earlier)

static struct BurnRomInfo batlballeRomDesc[] = {
	{ "1_10-16",						0x040000, 0x6b1baa07, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "2_10-16",						0x040000, 0x3c890639, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3_10-16",						0x040000, 0x8c30180e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4_10-16",						0x040000, 0x048c7aaa, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fb_6.413",						0x020000, 0xb57115c9, 1 | BRF_GRA },           //  4 Characters
	{ "fb_5.48",						0x010000, 0x440a9ae3, 1 | BRF_GRA },           //  5

	{ "fb_bg-1d.415",					0x200000, 0xeae7a1fc, 2 | BRF_GRA },           //  6 Background Tiles
	{ "fb_bg-1p.410",					0x100000, 0xb46e774e, 2 | BRF_GRA },           //  7

	{ "fb_obj-1.322",					0x400000, 0x29f86f68, 3 | BRF_GRA },           //  8 Sprites
	{ "fb_obj-2.324",					0x400000, 0xc9e3130b, 3 | BRF_GRA },           //  9
	{ "fb_obj-3.323",					0x400000, 0xf6c3bc49, 3 | BRF_GRA },           // 10

	{ "fb_pcm-1.215",					0x100000, 0x1d83891c, 3 | BRF_PRG | BRF_ESS }, // 11 Sample Data (Mapped to i386)
	{ "fb_7.216",						0x080000, 0x874d7b59, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "flash0_blank_region22.u1053",	0x100000, 0x5fee8413, 0 | BRF_SND },           // 13 Intel Flash (Samples)
};

STD_ROM_PICK(batlballe)
STD_ROM_FN(batlballe)

static INT32 BatlballeInit()
{
	return CommonInit(DECRYPT_SEI252, NULL, 0x18db4, 0x3058b2);
}

struct BurnDriver BurnDrvBatlballe = {
	"batlballe", "senkyu", NULL, NULL, "1995",
	"Battle Balls (Hong Kong, earlier)\0", NULL, "Seibu Kaihatsu (Metrotainment license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, batlballeRomInfo, batlballeRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	BatlballeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 240, 4, 3
};


// Battle Balls (US)

static struct BurnRomInfo batlballuRomDesc[] = {
	{ "sen1.bin",						0x040000, 0x13849bf0, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "sen2.bin",						0x040000, 0x2ae5f7e2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sen3.bin",						0x040000, 0x98e6f19f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sen4.bin",						0x040000, 0x1343ec56, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fb_6.413",						0x020000, 0xb57115c9, 1 | BRF_GRA },           //  4 Characters
	{ "fb_5.48",						0x010000, 0x440a9ae3, 1 | BRF_GRA },           //  5

	{ "fb_bg-1d.415",					0x200000, 0xeae7a1fc, 2 | BRF_GRA },           //  6 Background Tiles
	{ "fb_bg-1p.410",					0x100000, 0xb46e774e, 2 | BRF_GRA },           //  7

	{ "fb_obj-1.322",					0x400000, 0x29f86f68, 3 | BRF_GRA },           //  8 Sprites
	{ "fb_obj-2.324",					0x400000, 0xc9e3130b, 3 | BRF_GRA },           //  9
	{ "fb_obj-3.323",					0x400000, 0xf6c3bc49, 3 | BRF_GRA },           // 10

	{ "fb_pcm-1.215",					0x100000, 0x1d83891c, 3 | BRF_PRG | BRF_ESS }, // 11 Sample Data (Mapped to i386)
	{ "fb_7.216",						0x080000, 0x874d7b59, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "flash0_blank_region10.u1053",	0x100000, 0x4319d998, 0 | BRF_SND },           // 13 Intel Flash (Samples)
};

STD_ROM_PICK(batlballu)
STD_ROM_FN(batlballu)

static INT32 BatlballuInit()
{
	return CommonInit(DECRYPT_SEI252, NULL, 0x18db4, 0x305996);
}

struct BurnDriver BurnDrvBatlballu = {
	"batlballu", "senkyu", NULL, NULL, "1995",
	"Battle Balls (US)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, batlballuRomInfo, batlballuRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	BatlballuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 240, 4, 3
};


// Battle Balls (Portugal)

static struct BurnRomInfo batlballptRomDesc[] = {
	{ "senkyu_prog0_171195.u0211",					0x040000, 0xdcc227b6, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "2.u0212",						0x040000, 0x03ab203f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.u0210",						0x040000, 0x9eb9c8b4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.u029",							0x040000, 0xc37ae2a5, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fb_6.413",						0x020000, 0xb57115c9, 1 | BRF_GRA },           //  4 Characters
	{ "fb_5.48",						0x010000, 0x440a9ae3, 1 | BRF_GRA },           //  5

	{ "fb_bg-1d.415",					0x200000, 0xeae7a1fc, 2 | BRF_GRA },           //  6 Background Tiles
	{ "fb_bg-1p.410",					0x100000, 0xb46e774e, 2 | BRF_GRA },           //  7

	{ "fb_obj-1.322",					0x400000, 0x29f86f68, 3 | BRF_GRA },           //  8 Sprites
	{ "fb_obj-2.324",					0x400000, 0xc9e3130b, 3 | BRF_GRA },           //  9
	{ "fb_obj-3.323",					0x400000, 0xf6c3bc49, 3 | BRF_GRA },           // 10

	{ "fb_pcm-1.215",					0x100000, 0x1d83891c, 3 | BRF_PRG | BRF_ESS }, // 11 Sample Data (Mapped to i386)
	{ "fb_7.216",						0x080000, 0x874d7b59, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "flash0_blank_region96.u1053",	0x100000, 0xa0ebae75, 0 | BRF_SND },           // 13 Intel Flash (Samples)
};

STD_ROM_PICK(batlballpt)
STD_ROM_FN(batlballpt)

static INT32 BatlballptInit()
{
	return CommonInit(DECRYPT_SEI252, NULL, 0x18db4, 0x3058aa);
}

struct BurnDriver BurnDrvBatlballpt = {
	"batlballpt", "senkyu", NULL, NULL, "1995",
	"Battle Balls (Portugal)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, batlballptRomInfo, batlballptRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	BatlballptInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 240, 4, 3
};


// E Jong High School (Japan)

static struct BurnRomInfo ejanhsRomDesc[] = {
	{ "ejan3_1.211",					0x040000, 0xe626d3d2, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "ejan3_2.212",					0x040000, 0x83c39da2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ejan3_3.210",					0x040000, 0x46897b7d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ejan3_4.29",						0x040000, 0xb3187a2b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "ejan3_6.413",					0x020000, 0x837e012c, 1 | BRF_GRA },           //  4 Characters
	{ "ejan3_5.48",						0x010000, 0xd62db7bf, 1 | BRF_GRA },           //  5

	{ "ej3_bg1d.415",					0x200000, 0xbcacabe0, 2 | BRF_GRA },           //  6 Background Tiles
	{ "ej3_bg1p.410",					0x100000, 0x1fd0eb5e, 2 | BRF_GRA },           //  7
	{ "ej3_bg2d.416",					0x100000, 0xea2acd69, 2 | BRF_GRA },           //  8
	{ "ej3_bg2p.49",					0x080000, 0xa4a9cb0f, 2 | BRF_GRA },           //  9

	{ "ej3_obj1.322",					0x400000, 0x852f180e, 3 | BRF_GRA },           // 10 Sprites
	{ "ej3_obj2.324",					0x400000, 0x1116ad08, 3 | BRF_GRA },           // 11
	{ "ej3_obj3.323",					0x400000, 0xccfe02b6, 3 | BRF_GRA },           // 12

	{ "ej3_pcm1.215",					0x100000, 0xa92a3a82, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)
	{ "ejan3_7.216",					0x080000, 0xc6fc6bcf, 3 | BRF_PRG | BRF_ESS }, // 14

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 15 Intel Flash (Samples)
};

STD_ROM_PICK(ejanhs)
STD_ROM_FN(ejanhs)

static INT32 EjanhsInit()
{
	return CommonInit(DECRYPT_SEI252, NULL, 0, 0);
}

struct BurnDriver BurnDrvEjanhs = {
	"ejanhs", NULL, NULL, NULL, "1996",
	"E Jong High School (Japan)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, ejanhsRomInfo, ejanhsRomName, NULL, NULL, NULL, NULL, Spi_ejanhsInputInfo, Spi_ejanhsDIPInfo,
	EjanhsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	320, 240, 4, 3
};


// Viper Phase 1 (New Version, World)

static struct BurnRomInfo viprp1RomDesc[] = {
	{ "seibu1.211",						0x080000, 0xe5caf4ff, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu2.212",						0x080000, 0x688a998e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu3.210",						0x080000, 0x990fa76a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu4.29",						0x080000, 0x13e3e343, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu5.u0413",					0x020000, 0x5ece677c, 1 | BRF_GRA },           //  4 Characters
	{ "seibu6.u048",					0x010000, 0x44844ef8, 1 | BRF_GRA },           //  5

	{ "v_bg-11.415",					0x200000, 0x6fc96736, 2 | BRF_GRA },           //  6 Background Tiles
	{ "v_bg-12.415",					0x100000, 0xd3c7281c, 2 | BRF_GRA },           //  7
	{ "v_bg-21.410",					0x100000, 0xd65b4318, 2 | BRF_GRA },           //  8
	{ "v_bg-22.416",					0x080000, 0x24a0a23a, 2 | BRF_GRA },           //  9

	{ "v_obj-1.322",					0x400000, 0x3be5b631, 3 | BRF_GRA },           // 10 Sprites
	{ "v_obj-2.324",					0x400000, 0x924153b4, 3 | BRF_GRA },           // 11
	{ "v_obj-3.323",					0x400000, 0xe9fb9062, 3 | BRF_GRA },           // 12

	{ "v_pcm.215",						0x100000, 0xe3111b60, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)

	{ "flash0_blank_regionbe.u1053",	0x100000, 0xa4c181d0, 0 | BRF_SND },           // 14 Intel Flash (Samples)
};

STD_ROM_PICK(viprp1)
STD_ROM_FN(viprp1)

static INT32 Viprp1Init()
{
	return CommonInit(DECRYPT_SEI252, NULL, 0x1e2e0, 0x0202769);
}

struct BurnDriver BurnDrvViprp1 = {
	"viprp1", NULL, NULL, NULL, "1995",
	"Viper Phase 1 (New Version, World)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, viprp1RomInfo, viprp1RomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	Viprp1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Viper Phase 1 (New Version, Korea)

static struct BurnRomInfo viprp1kRomDesc[] = {
	{ "seibu1.211",						0x080000, 0x5495c930, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu2.212",						0x080000, 0xe0ad22ae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu3.210",						0x080000, 0xdb7bcb90, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu4.29",						0x080000, 0xc6188bf9, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu5.u0413",					0x020000, 0x1a35f2d8, 1 | BRF_GRA },           //  4 Characters
	{ "seibu6.u048",					0x010000, 0xe88bf049, 1 | BRF_GRA },           //  5

	{ "v_bg-11.415",					0x200000, 0x6fc96736, 2 | BRF_GRA },           //  6 Background Tiles
	{ "v_bg-12.415",					0x100000, 0xd3c7281c, 2 | BRF_GRA },           //  7
	{ "v_bg-21.410",					0x100000, 0xd65b4318, 2 | BRF_GRA },           //  8
	{ "v_bg-22.416",					0x080000, 0x24a0a23a, 2 | BRF_GRA },           //  9

	{ "v_obj-1.322",					0x400000, 0x3be5b631, 3 | BRF_GRA },           // 10 Sprites
	{ "v_obj-2.324",					0x400000, 0x924153b4, 3 | BRF_GRA },           // 11
	{ "v_obj-3.323",					0x400000, 0xe9fb9062, 3 | BRF_GRA },           // 12

	{ "v_pcm.215",						0x100000, 0xe3111b60, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)

	{ "flash0_blank_region24.u1053",	0x100000, 0x72a33dc4, 0 | BRF_SND },           // 14 Intel Flash (Samples)
};

STD_ROM_PICK(viprp1k)
STD_ROM_FN(viprp1k)

static INT32 Viprp1kInit()
{
	return CommonInit(DECRYPT_SEI252, NULL, 0x1e2e0, 0x202769);
}

struct BurnDriver BurnDrvViprp1k = {
	"viprp1k", "viprp1", NULL, NULL, "1995",
	"Viper Phase 1 (New Version, Korea)\0", NULL, "Seibu Kaihatsu (Dream Island license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, viprp1kRomInfo, viprp1kRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	Viprp1kInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Viper Phase 1 (New Version, US set 1)

static struct BurnRomInfo viprp1uRomDesc[] = {
	{ "seibu1.u0211",					0x080000, 0x3f412b80, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu2.u0212",					0x080000, 0x2e6c2376, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu3.u0210",					0x080000, 0xc38f7b4e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu4.u029",					0x080000, 0x523cbef3, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu5.u0413",					0x020000, 0x5ece677c, 1 | BRF_GRA },           //  4 Characters
	{ "seibu6.u048",					0x010000, 0x44844ef8, 1 | BRF_GRA },           //  5

	{ "v_bg-11.415",					0x200000, 0x6fc96736, 2 | BRF_GRA },           //  6 Background Tiles
	{ "v_bg-12.415",					0x100000, 0xd3c7281c, 2 | BRF_GRA },           //  7
	{ "v_bg-21.410",					0x100000, 0xd65b4318, 2 | BRF_GRA },           //  8
	{ "v_bg-22.416",					0x080000, 0x24a0a23a, 2 | BRF_GRA },           //  9

	{ "v_obj-1.322",					0x400000, 0x3be5b631, 3 | BRF_GRA },           // 10 Sprites
	{ "v_obj-2.324",					0x400000, 0x924153b4, 3 | BRF_GRA },           // 11
	{ "v_obj-3.323",					0x400000, 0xe9fb9062, 3 | BRF_GRA },           // 12

	{ "v_pcm.215",						0x100000, 0xe3111b60, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)

	{ "flash0_blank_region10.u1053",	0x100000, 0x4319d998, 0 | BRF_SND },           // 14 Intel Flash (Samples)
};

STD_ROM_PICK(viprp1u)
STD_ROM_FN(viprp1u)

struct BurnDriver BurnDrvViprp1u = {
	"viprp1u", "viprp1", NULL, NULL, "1995",
	"Viper Phase 1 (New Version, US set 1)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, viprp1uRomInfo, viprp1uRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	Viprp1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Viper Phase 1 (New Version, US set 2)

static struct BurnRomInfo viprp1uaRomDesc[] = {
	{ "seibus_1",						0x080000, 0x882c299c, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibus_2",						0x080000, 0x6ce586e9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibus_3",						0x080000, 0xf9dd9128, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibus_4",						0x080000, 0xcb06440c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu5.u0413",					0x020000, 0x5ece677c, 1 | BRF_GRA },           //  4 Characters
	{ "seibu6.u048",					0x010000, 0x44844ef8, 1 | BRF_GRA },           //  5

	{ "v_bg-11.415",					0x200000, 0x6fc96736, 2 | BRF_GRA },           //  6 Background Tiles
	{ "v_bg-12.415",					0x100000, 0xd3c7281c, 2 | BRF_GRA },           //  7
	{ "v_bg-21.410",					0x100000, 0xd65b4318, 2 | BRF_GRA },           //  8
	{ "v_bg-22.416",					0x080000, 0x24a0a23a, 2 | BRF_GRA },           //  9

	{ "v_obj-1.322",					0x400000, 0x3be5b631, 3 | BRF_GRA },           // 10 Sprites
	{ "v_obj-2.324",					0x400000, 0x924153b4, 3 | BRF_GRA },           // 11
	{ "v_obj-3.323",					0x400000, 0xe9fb9062, 3 | BRF_GRA },           // 12

	{ "v_pcm.215",						0x100000, 0xe3111b60, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)

	{ "flash0_blank_region10.u1053",	0x100000, 0x4319d998, 0 | BRF_SND },           // 14 Intel Flash (Samples)
};

STD_ROM_PICK(viprp1ua)
STD_ROM_FN(viprp1ua)

struct BurnDriver BurnDrvViprp1ua = {
	"viprp1ua", "viprp1", NULL, NULL, "1995",
	"Viper Phase 1 (New Version, US set 2)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, viprp1uaRomInfo, viprp1uaRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	Viprp1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Viper Phase 1 (New Version, Japan)

static struct BurnRomInfo viprp1jRomDesc[] = {
	{ "v_1-n.211",						0x080000, 0x55f10b72, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "v_2-n.212",						0x080000, 0x0f888283, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "v_3-n.210",						0x080000, 0x842434ac, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "v_4-n.29",						0x080000, 0xa3948824, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu5.u0413",					0x020000, 0x5ece677c, 1 | BRF_GRA },           //  4 Characters
	{ "seibu6.u048",					0x010000, 0x44844ef8, 1 | BRF_GRA },           //  5

	{ "v_bg-11.415",					0x200000, 0x6fc96736, 2 | BRF_GRA },           //  6 Background Tiles
	{ "v_bg-12.415",					0x100000, 0xd3c7281c, 2 | BRF_GRA },           //  7
	{ "v_bg-21.410",					0x100000, 0xd65b4318, 2 | BRF_GRA },           //  8
	{ "v_bg-22.416",					0x080000, 0x24a0a23a, 2 | BRF_GRA },           //  9

	{ "v_obj-1.322",					0x400000, 0x3be5b631, 3 | BRF_GRA },           // 10 Sprites
	{ "v_obj-2.324",					0x400000, 0x924153b4, 3 | BRF_GRA },           // 11
	{ "v_obj-3.323",					0x400000, 0xe9fb9062, 3 | BRF_GRA },           // 12

	{ "v_pcm.215",						0x100000, 0xe3111b60, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 14 Intel Flash (Samples)
};

STD_ROM_PICK(viprp1j)
STD_ROM_FN(viprp1j)

struct BurnDriver BurnDrvViprp1j = {
	"viprp1j", "viprp1", NULL, NULL, "1995",
	"Viper Phase 1 (New Version, Japan)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, viprp1jRomInfo, viprp1jRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	Viprp1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Viper Phase 1 (New Version, Switzerland)

static struct BurnRomInfo viprp1sRomDesc[] = {
	{ "viper_prg0.bin",					0x080000, 0xed9980b8, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "viper_prg1.bin",					0x080000, 0x9d4d3486, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "viper_prg2.bin",					0x080000, 0xd7ea460b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "viper_prg3.bin",					0x080000, 0xca6df094, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu5.u0413",					0x020000, 0x5ece677c, 1 | BRF_GRA },           //  4 Characters
	{ "seibu6.u048",					0x010000, 0x44844ef8, 1 | BRF_GRA },           //  5

	{ "v_bg-11.415",					0x200000, 0x6fc96736, 2 | BRF_GRA },           //  6 Background Tiles
	{ "v_bg-12.415",					0x100000, 0xd3c7281c, 2 | BRF_GRA },           //  7
	{ "v_bg-21.410",					0x100000, 0xd65b4318, 2 | BRF_GRA },           //  8
	{ "v_bg-22.416",					0x080000, 0x24a0a23a, 2 | BRF_GRA },           //  9

	{ "v_obj-1.322",					0x400000, 0x3be5b631, 3 | BRF_GRA },           // 10 Sprites
	{ "v_obj-2.324",					0x400000, 0x924153b4, 3 | BRF_GRA },           // 11
	{ "v_obj-3.323",					0x400000, 0xe9fb9062, 3 | BRF_GRA },           // 12

	{ "v_pcm.215",						0x100000, 0xe3111b60, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)

	{ "flash0_blank_region9c.u1053",	0x100000, 0xd73d640c, 0 | BRF_SND },           // 14 Intel Flash (Samples)
};

STD_ROM_PICK(viprp1s)
STD_ROM_FN(viprp1s)

struct BurnDriver BurnDrvViprp1s = {
	"viprp1s", "viprp1", NULL, NULL, "1995",
	"Viper Phase 1 (New Version, Switzerland)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, viprp1sRomInfo, viprp1sRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	Viprp1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Viper Phase 1 (New Version, Holland)

static struct BurnRomInfo viprp1hRomDesc[] = {
	{ "viper_prg0_010995.u0211",		0x080000, 0xe42fcc93, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "viper_prg1_010995.u0212",		0x080000, 0x9d4d3486, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "viper_prg2_010995.u0210",		0x080000, 0xd7ea460b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "viper_prg3_010995.u029",			0x080000, 0xca6df094, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "viper_fix_010995.u0413",			0x020000, 0x5ece677c, 1 | BRF_GRA },           //  4 Characters
	{ "viper_fixp_010995.u048",			0x010000, 0x44844ef8, 1 | BRF_GRA },           //  5

	{ "v_bg-11.415",					0x200000, 0x6fc96736, 2 | BRF_GRA },           //  6 Background Tiles
	{ "v_bg-12.415",					0x100000, 0xd3c7281c, 2 | BRF_GRA },           //  7
	{ "v_bg-21.410",					0x100000, 0xd65b4318, 2 | BRF_GRA },           //  8
	{ "v_bg-22.416",					0x080000, 0x24a0a23a, 2 | BRF_GRA },           //  9

	{ "v_obj-1.322",					0x400000, 0x3be5b631, 3 | BRF_GRA },           // 10 Sprites
	{ "v_obj-2.324",					0x400000, 0x924153b4, 3 | BRF_GRA },           // 11
	{ "v_obj-3.323",					0x400000, 0xe9fb9062, 3 | BRF_GRA },           // 12

	{ "v_pcm.215",						0x100000, 0xe3111b60, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)

	{ "flash0_blank_region90.u1053",	0x100000, 0x8da617a2, 0 | BRF_SND },           // 14 Intel Flash (Samples)
};

STD_ROM_PICK(viprp1h)
STD_ROM_FN(viprp1h)

struct BurnDriver BurnDrvViprp1h = {
	"viprp1h", "viprp1", NULL, NULL, "1995",
	"Viper Phase 1 (New Version, Holland)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, viprp1hRomInfo, viprp1hRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	Viprp1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Viper Phase 1 (New Version, Germany)

static struct BurnRomInfo viprp1tRomDesc[] = {
	{ "viper_prg0_010995.u0211",		0x080000, 0xf998dcf7, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "viper_prg1_010995.u0212",		0x080000, 0x9d4d3486, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "viper_prg2_010995.u0210",		0x080000, 0xd7ea460b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "viper_prg3_010995.u029",			0x080000, 0xca6df094, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "viper_fix_010995.u0413",			0x020000, 0x5ece677c, 1 | BRF_GRA },           //  4 Characters
	{ "viper_fixp_010995.u048",			0x010000, 0x44844ef8, 1 | BRF_GRA },           //  5

	{ "v_bg-11.415",					0x200000, 0x6fc96736, 2 | BRF_GRA },           //  6 Background Tiles
	{ "v_bg-12.415",					0x100000, 0xd3c7281c, 2 | BRF_GRA },           //  7
	{ "v_bg-21.410",					0x100000, 0xd65b4318, 2 | BRF_GRA },           //  8
	{ "v_bg-22.416",					0x080000, 0x24a0a23a, 2 | BRF_GRA },           //  9

	{ "v_obj-1.322",					0x400000, 0x3be5b631, 3 | BRF_GRA },           // 10 Sprites
	{ "v_obj-2.324",					0x400000, 0x924153b4, 3 | BRF_GRA },           // 11
	{ "v_obj-3.323",					0x400000, 0xe9fb9062, 3 | BRF_GRA },           // 12

	{ "v_pcm.215",						0x100000, 0xe3111b60, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)

	{ "flash0_blank_region80.u1053",	0x100000, 0xe2adaff5, 0 | BRF_SND },           // 14 Intel Flash (Samples)
};

STD_ROM_PICK(viprp1t)
STD_ROM_FN(viprp1t)

struct BurnDriver BurnDrvViprp1t = {
	"viprp1t", "viprp1", NULL, NULL, "1995",
	"Viper Phase 1 (New Version, Germany)\0", NULL, "Seibu Kaihatsu (Tuning license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, viprp1tRomInfo, viprp1tRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	Viprp1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Viper Phase 1 (New Version, Portugal)

static struct BurnRomInfo viprp1ptRomDesc[] = {
	{ "viper_prg0_010995.u0211",		0x080000, 0x0d4c69a6, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "viper_prg1_010995.u0212",		0x080000, 0x9d4d3486, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "viper_prg2_010995.u0210",		0x080000, 0xd7ea460b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "viper_prg3_010995.u029",			0x080000, 0xca6df094, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "viper_fix_010995.u0413",			0x020000, 0x5ece677c, 1 | BRF_GRA },           //  4 Characters
	{ "viper_fixp_010995.u048",			0x010000, 0x44844ef8, 1 | BRF_GRA },           //  5

	{ "v_bg-11.415",					0x200000, 0x6fc96736, 2 | BRF_GRA },           //  6 Background Tiles
	{ "v_bg-12.415",					0x100000, 0xd3c7281c, 2 | BRF_GRA },           //  7
	{ "v_bg-21.410",					0x100000, 0xd65b4318, 2 | BRF_GRA },           //  8
	{ "v_bg-22.416",					0x080000, 0x24a0a23a, 2 | BRF_GRA },           //  9

	{ "v_obj-1.322",					0x400000, 0x3be5b631, 3 | BRF_GRA },           // 10 Sprites
	{ "v_obj-2.324",					0x400000, 0x924153b4, 3 | BRF_GRA },           // 11
	{ "v_obj-3.323",					0x400000, 0xe9fb9062, 3 | BRF_GRA },           // 12

	{ "v_pcm.215",						0x100000, 0xe3111b60, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)

	{ "flash0_blank_region96.u1053",	0x100000, 0xa0ebae75, 0 | BRF_SND },           // 14 Intel Flash (Samples)
};

STD_ROM_PICK(viprp1pt)
STD_ROM_FN(viprp1pt)

struct BurnDriver BurnDrvViprp1pt = {
	"viprp1pt", "viprp1", NULL, NULL, "1995",
	"Viper Phase 1 (New Version, Portugal)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, viprp1ptRomInfo, viprp1ptRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	Viprp1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Viper Phase 1 (Hong Kong)

static struct BurnRomInfo viprp1hkRomDesc[] = {
	{ "seibu_1",						0x080000, 0x283ba7b7, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu_2",						0x080000, 0x2c4db249, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu_3",						0x080000, 0x91989503, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu_4",						0x080000, 0x12c9582d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_5",						0x020000, 0x80920fed, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_6",						0x010000, 0xe71a8722, 1 | BRF_GRA },           //  5

	{ "v_bg-11.415",					0x200000, 0x6fc96736, 2 | BRF_GRA },           //  6 Background Tiles
	{ "v_bg-12.415",					0x100000, 0xd3c7281c, 2 | BRF_GRA },           //  7
	{ "v_bg-21.410",					0x100000, 0xd65b4318, 2 | BRF_GRA },           //  8
	{ "v_bg-22.416",					0x080000, 0x24a0a23a, 2 | BRF_GRA },           //  9

	{ "v_obj-1.322",					0x400000, 0x3be5b631, 3 | BRF_GRA },           // 10 Sprites
	{ "v_obj-2.324",					0x400000, 0x924153b4, 3 | BRF_GRA },           // 11
	{ "v_obj-3.323",					0x400000, 0xe9fb9062, 3 | BRF_GRA },           // 12

	{ "v_pcm.215",						0x100000, 0xe3111b60, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)

	{ "flash0_blank_region22.u1053",	0x100000, 0x5fee8413, 0 | BRF_SND },           // 14 Intel Flash (Samples)
};

STD_ROM_PICK(viprp1hk)
STD_ROM_FN(viprp1hk)

struct BurnDriver BurnDrvViprp1hk = {
	"viprp1hk", "viprp1", NULL, NULL, "1995",
	"Viper Phase 1 (Hong Kong)\0", NULL, "Seibu Kaihatsu (Metrotainment license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, viprp1hkRomInfo, viprp1hkRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	Viprp1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Viper Phase 1 (Japan)

static struct BurnRomInfo viprp1ojRomDesc[] = {
	{ "v_1-o.211",						0x080000, 0x4430be64, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "v_2-o.212",						0x080000, 0xffbd88f7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "v_3-o.210",						0x080000, 0x6146db39, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "v_4-o.29",						0x080000, 0xdc8dd2b6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "v_5-o.413",						0x020000, 0x6d863acc, 1 | BRF_GRA },           //  4 Characters
	{ "v_6-o.48",						0x010000, 0xfe7cb8f7, 1 | BRF_GRA },           //  5

	{ "v_bg-11.415",					0x200000, 0x6fc96736, 2 | BRF_GRA },           //  6 Background Tiles
	{ "v_bg-12.415",					0x100000, 0xd3c7281c, 2 | BRF_GRA },           //  7
	{ "v_bg-21.410",					0x100000, 0xd65b4318, 2 | BRF_GRA },           //  8
	{ "v_bg-22.416",					0x080000, 0x24a0a23a, 2 | BRF_GRA },           //  9

	{ "v_obj-1.322",					0x400000, 0x3be5b631, 3 | BRF_GRA },           // 10 Sprites
	{ "v_obj-2.324",					0x400000, 0x924153b4, 3 | BRF_GRA },           // 11
	{ "v_obj-3.323",					0x400000, 0xe9fb9062, 3 | BRF_GRA },           // 12

	{ "v_pcm.215",						0x100000, 0xe3111b60, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 14 Intel Flash (Samples)
};

STD_ROM_PICK(viprp1oj)
STD_ROM_FN(viprp1oj)

struct BurnDriver BurnDrvViprp1oj = {
	"viprp1oj", "viprp1", NULL, NULL, "1995",
	"Viper Phase 1 (Japan)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, viprp1ojRomInfo, viprp1ojRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	Viprp1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Viper Phase 1 (Germany)

static struct BurnRomInfo viprp1otRomDesc[] = {
	{ "ov1.bin",						0x080000, 0xcbad0e28, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "ov2.bin",						0x080000, 0x0e2bbcb5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "ov3.bin",						0x080000, 0x0e86686b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "ov4.bin",						0x080000, 0x9d7dd325, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "v_5-o.413",						0x020000, 0x6d863acc, 1 | BRF_GRA },           //  4 Characters
	{ "v_6-o.48",						0x010000, 0xfe7cb8f7, 1 | BRF_GRA },           //  5

	{ "v_bg-11.415",					0x200000, 0x6fc96736, 2 | BRF_GRA },           //  6 Background Tiles
	{ "v_bg-12.415",					0x100000, 0xd3c7281c, 2 | BRF_GRA },           //  7
	{ "v_bg-21.410",					0x100000, 0xd65b4318, 2 | BRF_GRA },           //  8
	{ "v_bg-22.416",					0x080000, 0x24a0a23a, 2 | BRF_GRA },           //  9

	{ "v_obj-1.322",					0x400000, 0x3be5b631, 3 | BRF_GRA },           // 10 Sprites
	{ "v_obj-2.324",					0x400000, 0x924153b4, 3 | BRF_GRA },           // 11
	{ "v_obj-3.323",					0x400000, 0xe9fb9062, 3 | BRF_GRA },           // 12

	{ "v_pcm.215",						0x100000, 0xe3111b60, 3 | BRF_PRG | BRF_ESS }, // 13 Sample Data (Mapped to i386)

	{ "flash0_blank_region80.u1053",	0x100000, 0xe2adaff5, 0 | BRF_SND },           // 14 Intel Flash (Samples)
};

STD_ROM_PICK(viprp1ot)
STD_ROM_FN(viprp1ot)

struct BurnDriver BurnDrvViprp1ot = {
	"viprp1ot", "viprp1", NULL, NULL, "1995",
	"Viper Phase 1 (Germany)\0", NULL, "Seibu Kaihatsu (Tuning license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, viprp1otRomInfo, viprp1otRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	Viprp1Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Germany)

static struct BurnRomInfo rdftRomDesc[] = {
	{ "raiden-fi_prg0_121196.u0211",	0x080000, 0xadcb5dbc, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "raiden-fi_prg1_121196.u0212",	0x080000, 0x60c5b92e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "raiden-fi_prg2_121196.u0210",	0x080000, 0x44b86db5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "raiden-fi_prg3_121196.u029",		0x080000, 0xe70727ce, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_6.u0424",					0x010000, 0x6ac64968, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_5.u0423",					0x010000, 0x8f8d4e14, 1 | BRF_GRA },           //  5
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  6

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  7 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  8
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  9
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           // 10

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           // 11 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 12
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 13

	{ "gun_dogs_pcm.u0217",				0x200000, 0x31253ad7, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "seibu_8.u0216",					0x080000, 0xf88cb6e4, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region80.u1053",	0x100000, 0xe2adaff5, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rdft)
STD_ROM_FN(rdft)

// hack - build flash roms from sample data to allow for instant loading rather than wait for flash to populate for 999 seconds.
static void rdft_build_flash()
{
	INT32 i, j;
	char *pRomName;
	struct BurnRomInfo ri;

	for (i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)	{
		BurnDrvGetRomInfo(&ri, i);
		if ((ri.nType & 7) == 3 && (ri.nType & BRF_PRG)) break;
	}

	BurnLoadRomExt(DrvSndROM[0] + 0x0a00000, i+0, 1, LD_GROUP(1));

	BurnDrvGetRomInfo(&ri, i+1); // check for second rom, newer set does not have this

	if ((ri.nType & 7) == 3 && (ri.nType & BRF_PRG)) // has second rom, copy it in
	{
		UINT8 *tmp = (UINT8*)BurnMalloc(ri.nLen);

		BurnLoadRomExt(tmp, i+1, 1, LD_GROUP(1));

		j = 0x1fffff;
		while (DrvSndROM[0][0xa00000 + j - 1] == 0xff) j--;

		memcpy(DrvSndROM[0] + 0x0a00000 + j, tmp, 0x200000 - j);

		BurnFree(tmp);
	}

	DrvSndROM[0][0xa00000] = DrvMainROM[0x1ffffc]; // region
	DrvSndROM[0][0xa00001] = 0x4a; // checksums?
	DrvSndROM[0][0xa00002] = 0x4a;
	DrvSndROM[0][0xa00003] = 0x36;
}

static INT32 RdftInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x00298d0, 0x203f06);
}

struct BurnDriver BurnDrvRdft = {
	"rdft", NULL, NULL, NULL, "1996",
	"Raiden Fighters (Germany)\0", NULL, "Seibu Kaihatsu (Tuning license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftRomInfo, rdftRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (US, earlier)

static struct BurnRomInfo rdftuRomDesc[] = {
	{ "rdftu_gd_1.211",					0x080000, 0x47810c48, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "rdftu_gd_2.212",					0x080000, 0x13911750, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rdftu_gd_3.210",					0x080000, 0x10761b03, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rdftu_gd_4.29",					0x080000, 0xe5a3f01d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_6.u0424",					0x010000, 0x6ac64968, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_5.u0423",					0x010000, 0x8f8d4e14, 1 | BRF_GRA },           //  5
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  6

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  7 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  8
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  9
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           // 10

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           // 11 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 12
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 13

	{ "gun_dogs_pcm.u0217",				0x200000, 0x31253ad7, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "seibu_8.u0216",					0x080000, 0xf88cb6e4, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region10.u1053",	0x100000, 0x4319d998, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rdftu)
STD_ROM_FN(rdftu)

static INT32 RdftuInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x00298d0, 0x203f3a);
}

struct BurnDriver BurnDrvRdftu = {
	"rdftu", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (US, earlier)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftuRomInfo, rdftuRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Japan, earlier)

static struct BurnRomInfo rdftjRomDesc[] = {
	{ "gd_1.211",						0x080000, 0xf6b2cbdc, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "gd_2.212",						0x080000, 0x1982f812, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "gd_3.210",						0x080000, 0xb0f59f44, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "gd_4.29",						0x080000, 0xcd8705bd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_6.u0424",					0x010000, 0x6ac64968, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_5.u0423",					0x010000, 0x8f8d4e14, 1 | BRF_GRA },           //  5
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  6

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  7 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  8
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  9
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           // 10

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           // 11 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 12
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 13

	{ "gun_dogs_pcm.u0217",				0x200000, 0x31253ad7, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "seibu_8.u0216",					0x080000, 0xf88cb6e4, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rdftj)
STD_ROM_FN(rdftj)

static INT32 RdftjInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x00298d0, 0x203f0a);
}

struct BurnDriver BurnDrvRdftj = {
	"rdftj", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Japan, earlier)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftjRomInfo, rdftjRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Japan, earliest)

static struct BurnRomInfo rdftjaRomDesc[] = {
	{ "rf1.bin",						0x080000, 0x46861b75, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "rf2.bin",						0x080000, 0x6388ed11, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rf3.bin",						0x080000, 0xbeafcd24, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rf4.bin",						0x080000, 0x5236f45f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_6.u0424",					0x010000, 0x6ac64968, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_5.u0423",					0x010000, 0x8f8d4e14, 1 | BRF_GRA },           //  5
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  6

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  7 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  8
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  9
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           // 10

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           // 11 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 12
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 13

	{ "gun_dogs_pcm.u0217",				0x200000, 0x31253ad7, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "seibu_8.u0216",					0x080000, 0xf88cb6e4, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rdftja)
STD_ROM_FN(rdftja)

static INT32 RdftjaInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x00298d0, 0x203f45);
}

struct BurnDriver BurnDrvRdftja = {
	"rdftja", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Japan, earliest)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftjaRomInfo, rdftjaRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftjaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Australia)

static struct BurnRomInfo rdftauRomDesc[] = {
	{ "1.u0211",						0x080000, 0x6339c60d, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "2.u0212",						0x080000, 0xa88bda02, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.u0210",						0x080000, 0xa73e337e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.u029",							0x080000, 0x8cc628f0, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_6.u0424",					0x010000, 0x6ac64968, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_5.u0423",					0x010000, 0x8f8d4e14, 1 | BRF_GRA },           //  5
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  6

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  7 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  8
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  9
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           // 10

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           // 11 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 12
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 13

	{ "gun_dogs_pcm.u0217",				0x200000, 0x31253ad7, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "seibu_8.u0216",					0x080000, 0xf88cb6e4, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region9e.u1053",	0x100000, 0x7ad6f17e, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rdftau)
STD_ROM_FN(rdftau)

static INT32 RdftauInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x00298d0, 0x203f16);
}

struct BurnDriver BurnDrvRdftau = {
	"rdftau", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Australia)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftauRomInfo, rdftauRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftauInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Evaluation Software For Show, Germany)

static struct BurnRomInfo rdftaugeRomDesc[] = {
	{ "seibu.1.u0211",					0x080000, 0x3e79da3c, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu.2.u0212",					0x080000, 0xa6fbf98c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu.3.u0210",					0x080000, 0xad31cc17, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu.4.u029",					0x080000, 0x756d99ae, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_6.u0424",					0x010000, 0x6ac64968, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_5.u0423",					0x010000, 0x8f8d4e14, 1 | BRF_GRA },           //  5
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  6

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  7 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  8
	{ "gun_dogs_bg2-d.u0416",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  9
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           // 10

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           // 11 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 12
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 13

	{ "gun_dogs_pcm.u0217",				0x200000, 0x31253ad7, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "seibu_8.u0216",					0x080000, 0xf88cb6e4, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region9e.u1053",	0x100000, 0x7ad6f17e, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rdftauge)
STD_ROM_FN(rdftauge)

static INT32 RdftaugeInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x00298d0, 0x203f6e);
}

struct BurnDriver BurnDrvRdftauge = {
	"rdftauge", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Evaluation Software For Show, Germany)\0", NULL, "Seibu Kaihatsu (Tuning license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftaugeRomInfo, rdftaugeRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftaugeInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Austria)

static struct BurnRomInfo rdftaRomDesc[] = {
	{ "seibu_1.u0211",					0x080000, 0xc3bb2e58, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu_2.u0212",					0x080000, 0x58ccb10c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu_3.u0210",					0x080000, 0x47fc3c96, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu_4.u029",					0x080000, 0x271bdd4b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_6.u0424",					0x010000, 0x6ac64968, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_5.u0423",					0x010000, 0x8f8d4e14, 1 | BRF_GRA },           //  5
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  6

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  7 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  8
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  9
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           // 10

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           // 11 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 12
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 13

	{ "gun_dogs_pcm.u0217",				0x200000, 0x31253ad7, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "seibu_8.u0216",					0x080000, 0xf88cb6e4, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region82.u1053",	0x100000, 0x4f463a87, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rdfta)
STD_ROM_FN(rdfta)

static INT32 RdftaInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x00298d0, 0x203f46);
}

struct BurnDriver BurnDrvRdfta = {
	"rdfta", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Austria)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftaRomInfo, rdftaRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Italy)

static struct BurnRomInfo rdftitRomDesc[] = {
	{ "seibu_1.u0211",					0x080000, 0xde0c3e3c, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu_2.u0212",					0x080000, 0x58ccb10c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu_3.u0210",					0x080000, 0x47fc3c96, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu_4.u029",					0x080000, 0x271bdd4b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_6.u0424",					0x010000, 0x6ac64968, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_5.u0423",					0x010000, 0x8f8d4e14, 1 | BRF_GRA },           //  5
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  6

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  7 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  8
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  9
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           // 10

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           // 11 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 12
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 13

	{ "gun_dogs_pcm.u0217",				0x200000, 0x31253ad7, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "seibu_8.u0216",					0x080000, 0xf88cb6e4, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region92.u1053",	0x100000, 0x204d82d0, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rdftit)
STD_ROM_FN(rdftit)

static INT32 RdftitInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x00298d0, 0x203f46);
}

struct BurnDriver BurnDrvRdftit = {
	"rdftit", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Italy)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftitRomInfo, rdftitRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Great Britain)

static struct BurnRomInfo rdftgbRomDesc[] = {
	{ "seibu_1.u0211",					0x080000, 0x2403035f, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu_2.u0212",					0x080000, 0x58ccb10c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu_3.u0210",					0x080000, 0x47fc3c96, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu_4.u029",					0x080000, 0x271bdd4b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_6.u0424",					0x010000, 0x6ac64968, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_5.u0423",					0x010000, 0x8f8d4e14, 1 | BRF_GRA },           //  5
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  6

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  7 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  8
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  9
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           // 10

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           // 11 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 12
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 13

	{ "gun_dogs_pcm.u0217",				0x200000, 0x31253ad7, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "seibu_8.u0216",					0x080000, 0xf88cb6e4, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region8c.u1053",	0x100000, 0xb836dc5b, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rdftgb)
STD_ROM_FN(rdftgb)

static INT32 RdftgbInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x00298d0, 0x203f46); // 0x203f69?
}

struct BurnDriver BurnDrvRdftgb = {
	"rdftgb", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Great Britain)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftgbRomInfo, rdftgbRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftgbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Greece)

static struct BurnRomInfo rdftgrRomDesc[] = {
	{ "seibu_1.u0211",					0x080000, 0xca0d6273, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu_2.u0212",					0x080000, 0x58ccb10c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu_3.u0210",					0x080000, 0x47fc3c96, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu_4.u029",					0x080000, 0x271bdd4b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_6.u0424",					0x010000, 0x6ac64968, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_5.u0423",					0x010000, 0x8f8d4e14, 1 | BRF_GRA },           //  5
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  6

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  7 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  8
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  9
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           // 10

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           // 11 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 12
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 13

	{ "gun_dogs_pcm.u0217",				0x200000, 0x31253ad7, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "seibu_8.u0216",					0x080000, 0xf88cb6e4, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region8e.u1053",	0x100000, 0x15dd4929, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rdftgr)
STD_ROM_FN(rdftgr)

static INT32 RdftgrInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x00298d0, 0x203f46); // 0x203f69?
}

struct BurnDriver BurnDrvRdftgr = {
	"rdftgr", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Greece)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftgrRomInfo, rdftgrRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftgrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Japan, newer)

static struct BurnRomInfo rdftjbRomDesc[] = {
	{ "seibu_1.u0211",					0x080000, 0xb70afcc2, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "raiden-f_prg2.u0212",			0x080000, 0x58ccb10c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "raiden-f_prg34.u0219",			0x100000, 0x63f01d17, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "raiden-f_fix.u0425",				0x020000, 0x2be2936b, 1 | BRF_GRA },           //  3 Characters
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  4

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  5 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  6
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  7
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           //  8

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           //  9 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 10
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 11

	{ "raiden-f_pcm2.u0217",			0x200000, 0x3f8d4a48, 3 | BRF_PRG | BRF_ESS }, // 12 Sample Data (Mapped to i386)

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 13 Intel Flash (Samples)
};

STD_ROM_PICK(rdftjb)
STD_ROM_FN(rdftjb)

static INT32 RdftjbInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x298d0, 0x203f46); // 0x203f69?
}

struct BurnDriver BurnDrvRdftjb = {
	"rdftjb", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Japan, newer)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftjbRomInfo, rdftjbRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftjbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (US, newer)

static struct BurnRomInfo rdftuaRomDesc[] = {
	{ "seibu_1.u0211",					0x080000, 0xddbadc30, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "raiden-f_prg2.u0212",			0x080000, 0x58ccb10c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "raiden-f_prg34.u0219",			0x100000, 0x63f01d17, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "raiden-f_fix.u0425",				0x020000, 0x2be2936b, 1 | BRF_GRA },           //  3 Characters
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  4

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  5 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  6
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  7
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           //  8

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           //  9 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 10
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 11

	{ "raiden-f_pcm2.u0217",			0x200000, 0x3f8d4a48, 3 | BRF_PRG | BRF_ESS }, // 12 Sample Data (Mapped to i386)

	{ "flash0_blank_region10.u1053",	0x100000, 0x4319d998, 0 | BRF_SND },           // 13 Intel Flash (Samples)
};

STD_ROM_PICK(rdftua)
STD_ROM_FN(rdftua)

static INT32 RdftuaInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x298d0, 0x203f46); // 0x203f69?
}

struct BurnDriver BurnDrvRdftua = {
	"rdftua", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (US, newer)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftuaRomInfo, rdftuaRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftuaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Korea, SUB4 cart)

static struct BurnRomInfo rdftadiRomDesc[] = {
	{ "seibu_1.u0211",					0x080000, 0xfc0e2885, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "raiden-f_prg2.u0212",			0x080000, 0x58ccb10c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "raiden-f_prg34.u0219",			0x100000, 0x63f01d17, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "raiden-f_fix.u0425",				0x020000, 0x2be2936b, 1 | BRF_GRA },           //  3 Characters
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  4

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  5 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  6
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  7
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           //  8

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           //  9 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 10
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 11

	{ "raiden-f_pcm2.u0217",			0x200000, 0x3f8d4a48, 3 | BRF_PRG | BRF_ESS }, // 12 Sample Data (Mapped to i386)

	{ "flash0_blank_region24.u1053",	0x100000, 0x72a33dc4, 0 | BRF_SND },           // 13 Intel Flash (Samples)
};

STD_ROM_PICK(rdftadi)
STD_ROM_FN(rdftadi)

static INT32 RdftadiInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x298d0, 0x203f46); // 0x203f69?
}

struct BurnDriver BurnDrvRdftadi = {
	"rdftadi", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Korea, SUB4 cart)\0", NULL, "Seibu Kaihatsu (Dream Island license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftadiRomInfo, rdftadiRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftadiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Korea, SUB2 cart)

static struct BurnRomInfo rdftadiaRomDesc[] = {
	{ "seibu_1.u0211",					0x080000, 0xfc0e2885, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu_2.u0212",					0x080000, 0x58ccb10c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu_3.u0210",					0x080000, 0x47fc3c96, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu_4.u029",					0x080000, 0x271bdd4b, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu_6.u0424",					0x010000, 0x6ac64968, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_5.u0423",					0x010000, 0x8f8d4e14, 1 | BRF_GRA },           //  5
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  6

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  7 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  8
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  9
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           // 10

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           // 11 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 12
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 13

	{ "gun_dogs_pcm.u0217",				0x200000, 0x31253ad7, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "seibu_8.u0216",					0x080000, 0xf88cb6e4, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region24.u1053",	0x100000, 0x72a33dc4, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rdftadia)
STD_ROM_FN(rdftadia)

static INT32 RdftadiaInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x00298d0, 0x203f46);
}

struct BurnDriver BurnDrvRdftadia = {
	"rdftadia", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Korea, SUB2 cart)\0", NULL, "Seibu Kaihatsu (Dream Island license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftadiaRomInfo, rdftadiaRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftadiaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Hong Kong)

static struct BurnRomInfo rdftamRomDesc[] = {
	{ "seibu_1.u0211",					0x080000, 0x156d8db0, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "raiden-f_prg2.u0212",			0x080000, 0x58ccb10c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "raiden-f_prg34.u0219",			0x100000, 0x63f01d17, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "raiden-f_fix.u0425",				0x020000, 0x2be2936b, 1 | BRF_GRA },           //  3 Characters
	{ "seibu_7.u048",					0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  4

	{ "gun_dogs_bg1-d.u0415",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  5 Background Tiles
	{ "gun_dogs_bg1-p.u0410",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  6
	{ "gun_dogs_bg2-d.u0424",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  7
	{ "gun_dogs_bg2-p.u049",			0x100000, 0x502d5799, 2 | BRF_GRA },           //  8

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           //  9 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 10
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 11

	{ "raiden-f_pcm2.u0217",			0x200000, 0x3f8d4a48, 3 | BRF_PRG | BRF_ESS }, // 12 Sample Data (Mapped to i386)

	{ "flash0_blank_region22.u1053",	0x100000, 0x5fee8413, 0 | BRF_SND },           // 13 Intel Flash (Samples)
};

STD_ROM_PICK(rdftam)
STD_ROM_FN(rdftam)

static INT32 RdftamInit()
{
	return CommonInit(DECRYPT_SEI252, rdft_build_flash, 0x298d0, 0x203f46);
}

struct BurnDriver BurnDrvRdftam = {
	"rdftam", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Hong Kong)\0", NULL, "Seibu Kaihatsu (Metrotainment license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftamRomInfo, rdftamRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, Spi_3buttonDIPInfo,
	RdftamInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive (Germany)

static struct BurnRomInfo rdft2RomDesc[] = {
	{ "prg0.tun",						0x080000, 0x3cb3fdca, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg1.bin",						0x080000, 0xcab55d88, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.bin",						0x080000, 0x83758b0e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.bin",						0x080000, 0x084fb5e4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fix0.u0524",						0x010000, 0x6fdf4cf6, 1 | BRF_GRA },           //  4 Characters
	{ "fix1.u0518",						0x010000, 0x69b7899b, 1 | BRF_GRA },           //  5
	{ "fixp.u0514",						0x010000, 0x99a5fece, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0535",					0x400000, 0x6143f576, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0536",					0x400000, 0xc607a444, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0538",					0x200000, 0xf0830248, 2 | BRF_GRA },           // 10

	{ "obj3.u0434",						0x400000, 0xe08f42dc, 3 | BRF_GRA },           // 11 Sprites
	{ "obj3b.u0433",					0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 12
	{ "obj2.u0431",						0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 13
	{ "obj2b.u0432",					0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 14
	{ "obj1.u0429",						0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 15
	{ "obj1b.u0430",					0x200000, 0x5259321f, 3 | BRF_GRA },           // 16

	{ "pcm.u0217",						0x200000, 0x2edc30b5, 3 | BRF_PRG | BRF_ESS }, // 17 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xb7bd3703, 3 | BRF_PRG | BRF_ESS }, // 18
	
	{ "rm81.u0529.bin",					0x000117, 0xacd55c8e, 0 | BRF_OPT },           // 19 PLDs
	{ "rm82.u0330.bin",					0x000117, 0x64c71423, 0 | BRF_OPT },           // 20
	{ "rm83.u0331.bin",					0x000117, 0x6e10d66b, 0 | BRF_OPT },           // 21

	{ "flash0_blank_region80.u1053",	0x100000, 0xe2adaff5, 0 | BRF_SND },           // 22 Intel Flash (Samples)
};

STD_ROM_PICK(rdft2)
STD_ROM_FN(rdft2)

static const UINT8 Rdft2DefaultNVRAM[32] = {
	0x65, 0x4A, 0x4A, 0x37, 0x20, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x0B, 0x00, 0x00, 0x00,
	0x01, 0x03, 0x04, 0x02, 0x00, 0x00, 0x5B, 0x69, 0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x34, 0x02
};

static INT32 Rdft2Init()
{
	DefaultNVRAM = Rdft2DefaultNVRAM;

	return CommonInit(DECRYPT_RISE10, NULL, 0x00282ac, 0x0204372);
}

struct BurnDriver BurnDrvRdft2 = {
	"rdft2", NULL, NULL, NULL, "1997",
	"Raiden Fighters 2 - Operation Hell Dive (Germany)\0", NULL, "Seibu Kaihatsu (Tuning license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft2RomInfo, rdft2RomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	Rdft2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive (US)

static struct BurnRomInfo rdft2uRomDesc[] = {
	{ "1.bin",							0x080000, 0xb7d6c866, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "2.bin",							0x080000, 0xff7747c5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",							0x080000, 0x86e3d1a8, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.bin",							0x080000, 0x2e409a76, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fix0.u0524",						0x010000, 0x6fdf4cf6, 1 | BRF_GRA },           //  4 Characters
	{ "fix1.u0518",						0x010000, 0x69b7899b, 1 | BRF_GRA },           //  5
	{ "fixp.u0514",						0x010000, 0x99a5fece, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0535",					0x400000, 0x6143f576, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0536",					0x400000, 0xc607a444, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0538",					0x200000, 0xf0830248, 2 | BRF_GRA },           // 10

	{ "obj3.u0434",						0x400000, 0xe08f42dc, 3 | BRF_GRA },           // 11 Sprites
	{ "obj3b.u0433",					0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 12
	{ "obj2.u0431",						0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 13
	{ "obj2b.u0432",					0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 14
	{ "obj1.u0429",						0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 15
	{ "obj1b.u0430",					0x200000, 0x5259321f, 3 | BRF_GRA },           // 16

	{ "pcm.u0217",						0x200000, 0x2edc30b5, 3 | BRF_PRG | BRF_ESS }, // 17 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xb7bd3703, 3 | BRF_PRG | BRF_ESS }, // 18
	
	{ "rm81.u0529.bin",					0x000117, 0xacd55c8e, 0 | BRF_OPT },           // 19 PLDs
	{ "rm82.u0330.bin",					0x000117, 0x64c71423, 0 | BRF_OPT },           // 20
	{ "rm83.u0331.bin",					0x000117, 0x6e10d66b, 0 | BRF_OPT },           // 21

	{ "flash0_blank_region10.u1053",	0x100000, 0x4319d998, 0 | BRF_SND },           // 22 Intel Flash (Samples)
};

STD_ROM_PICK(rdft2u)
STD_ROM_FN(rdft2u)

static INT32 Rdft2uInit()
{
	DefaultNVRAM = Rdft2DefaultNVRAM;

	return CommonInit(DECRYPT_RISE10, NULL, 0x00282ac, 0x2044b6); // or 2044d9?
}

struct BurnDriver BurnDrvRdft2u = {
	"rdft2u", "rdft2", NULL, NULL, "1997",
	"Raiden Fighters 2 - Operation Hell Dive (US)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft2uRomInfo, rdft2uRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	Rdft2uInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive (Japan set 1)

static struct BurnRomInfo rdft2jRomDesc[] = {
	{ "prg0.sei",						0x080000, 0xa60c4e7c, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg1.bin",						0x080000, 0xcab55d88, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.bin",						0x080000, 0x83758b0e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.bin",						0x080000, 0x084fb5e4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fix0.u0524",						0x010000, 0x6fdf4cf6, 1 | BRF_GRA },           //  4 Characters
	{ "fix1.u0518",						0x010000, 0x69b7899b, 1 | BRF_GRA },           //  5
	{ "fixp.u0514",						0x010000, 0x99a5fece, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0535",					0x400000, 0x6143f576, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0536",					0x400000, 0xc607a444, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0538",					0x200000, 0xf0830248, 2 | BRF_GRA },           // 10

	{ "obj3.u0434",						0x400000, 0xe08f42dc, 3 | BRF_GRA },           // 11 Sprites
	{ "obj3b.u0433",					0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 12
	{ "obj2.u0431",						0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 13
	{ "obj2b.u0432",					0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 14
	{ "obj1.u0429",						0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 15
	{ "obj1b.u0430",					0x200000, 0x5259321f, 3 | BRF_GRA },           // 16

	{ "pcm.u0217",						0x200000, 0x2edc30b5, 3 | BRF_PRG | BRF_ESS }, // 17 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xb7bd3703, 3 | BRF_PRG | BRF_ESS }, // 18
	
	{ "rm81.u0529.bin",					0x000117, 0xacd55c8e, 0 | BRF_OPT },           // 19 PLDs
	{ "rm82.u0330.bin",					0x000117, 0x64c71423, 0 | BRF_OPT },           // 20
	{ "rm83.u0331.bin",					0x000117, 0x6e10d66b, 0 | BRF_OPT },           // 21

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 22 Intel Flash (Samples)
};

STD_ROM_PICK(rdft2j)
STD_ROM_FN(rdft2j)

struct BurnDriver BurnDrvRdft2j = {
	"rdft2j", "rdft2", NULL, NULL, "1997",
	"Raiden Fighters 2 - Operation Hell Dive (Japan set 1)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft2jRomInfo, rdft2jRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	Rdft2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive (Japan set 2)

static struct BurnRomInfo rdft2jaRomDesc[] = {
	{ "rf2.1",							0x080000, 0x391d5057, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "rf2_2.bin",						0x080000, 0xec73a767, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rf2_3.bin",						0x080000, 0xe66243b2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rf2_4.bin",						0x080000, 0x92b7b73e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rf2_5.bin",						0x010000, 0x377cac2f, 1 | BRF_GRA },           //  4 Characters
	{ "rf2_6.bin",						0x010000, 0x42bd5372, 1 | BRF_GRA },           //  5
	{ "rf2_7.bin",						0x010000, 0x1efaac7e, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0535",					0x400000, 0x6143f576, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0536",					0x400000, 0xc607a444, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0538",					0x200000, 0xf0830248, 2 | BRF_GRA },           // 10

	{ "obj3.u0434",						0x400000, 0xe08f42dc, 3 | BRF_GRA },           // 11 Sprites
	{ "obj3b.u0433",					0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 12
	{ "obj2.u0431",						0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 13
	{ "obj2b.u0432",					0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 14
	{ "obj1.u0429",						0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 15
	{ "obj1b.u0430",					0x200000, 0x5259321f, 3 | BRF_GRA },           // 16

	{ "pcm.u0217",						0x200000, 0x2edc30b5, 3 | BRF_PRG | BRF_ESS }, // 17 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xb7bd3703, 3 | BRF_PRG | BRF_ESS }, // 18
	
	{ "rm81.u0529.bin",					0x000117, 0xacd55c8e, 0 | BRF_OPT },           // 19 PLDs
	{ "rm82.u0330.bin",					0x000117, 0x64c71423, 0 | BRF_OPT },           // 20
	{ "rm83.u0331.bin",					0x000117, 0x6e10d66b, 0 | BRF_OPT },           // 21

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 22 Intel Flash (Samples)
};

STD_ROM_PICK(rdft2ja)
STD_ROM_FN(rdft2ja)

struct BurnDriver BurnDrvRdft2ja = {
	"rdft2ja", "rdft2", NULL, NULL, "1997",
	"Raiden Fighters 2 - Operation Hell Dive (Japan set 2)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft2jaRomInfo, rdft2jaRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	Rdft2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive (Japan set 3)

static struct BurnRomInfo rdft2jbRomDesc[] = {
	{ "prg0.rom",						0x080000, 0xfc42cab8, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg1.rom",						0x080000, 0xa0f09dc5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.rom",						0x080000, 0x368580e0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.rom",						0x080000, 0x7ad45c01, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rf2_5.bin",						0x010000, 0x377cac2f, 1 | BRF_GRA },           //  4 Characters
	{ "rf2_6.bin",						0x010000, 0x42bd5372, 1 | BRF_GRA },           //  5
	{ "rf2_7.bin",						0x010000, 0x1efaac7e, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0535",					0x400000, 0x6143f576, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0536",					0x400000, 0xc607a444, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0538",					0x200000, 0xf0830248, 2 | BRF_GRA },           // 10

	{ "obj3.u0434",						0x400000, 0xe08f42dc, 3 | BRF_GRA },           // 11 Sprites
	{ "obj3b.u0433",					0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 12
	{ "obj2.u0431",						0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 13
	{ "obj2b.u0432",					0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 14
	{ "obj1.u0429",						0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 15
	{ "obj1b.u0430",					0x200000, 0x5259321f, 3 | BRF_GRA },           // 16

	{ "pcm.u0217",						0x200000, 0x2edc30b5, 3 | BRF_PRG | BRF_ESS }, // 17 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xb7bd3703, 3 | BRF_PRG | BRF_ESS }, // 18
	
	{ "rm81.u0529.bin",					0x000117, 0xacd55c8e, 0 | BRF_OPT },           // 19 PLDs
	{ "rm82.u0330.bin",					0x000117, 0x64c71423, 0 | BRF_OPT },           // 20
	{ "rm83.u0331.bin",					0x000117, 0x6e10d66b, 0 | BRF_OPT },           // 21

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 22 Intel Flash (Samples)
};

STD_ROM_PICK(rdft2jb)
STD_ROM_FN(rdft2jb)

struct BurnDriver BurnDrvRdft2jb = {
	"rdft2jb", "rdft2", NULL, NULL, "1997",
	"Raiden Fighters 2 - Operation Hell Dive (Japan set 3)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft2jbRomInfo, rdft2jbRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	Rdft2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive (Japan set 4)

static struct BurnRomInfo rdft2jcRomDesc[] = {
	{ "seibu_1.u0211",					0x080000, 0x36b6407c, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu_2.u0212",					0x080000, 0x65ee556e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu_3.u0221",					0x080000, 0xd2458358, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu_4.u0220",					0x080000, 0x5c4412f9, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fix0.u0524",						0x010000, 0x6fdf4cf6, 1 | BRF_GRA },           //  4 Characters
	{ "fix1.u0518",						0x010000, 0x69b7899b, 1 | BRF_GRA },           //  5
	{ "fixp.u0514",						0x010000, 0x99a5fece, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0535",					0x400000, 0x6143f576, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0536",					0x400000, 0xc607a444, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0538",					0x200000, 0xf0830248, 2 | BRF_GRA },           // 10

	{ "obj3.u0434",						0x400000, 0xe08f42dc, 3 | BRF_GRA },           // 11 Sprites
	{ "obj3b.u0433",					0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 12
	{ "obj2.u0431",						0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 13
	{ "obj2b.u0432",					0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 14
	{ "obj1.u0429",						0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 15
	{ "obj1b.u0430",					0x200000, 0x5259321f, 3 | BRF_GRA },           // 16

	{ "pcm.u0217",						0x200000, 0x2edc30b5, 3 | BRF_PRG | BRF_ESS }, // 17 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xb7bd3703, 3 | BRF_PRG | BRF_ESS }, // 18
	
	{ "rm81.u0529.bin",					0x000117, 0xacd55c8e, 0 | BRF_OPT },           // 19 PLDs
	{ "rm82.u0330.bin",					0x000117, 0x64c71423, 0 | BRF_OPT },           // 20
	{ "rm83.u0331.bin",					0x000117, 0x6e10d66b, 0 | BRF_OPT },           // 21

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 22 Intel Flash (Samples)
};

STD_ROM_PICK(rdft2jc)
STD_ROM_FN(rdft2jc)

struct BurnDriver BurnDrvRdft2jc = {
	"rdft2jc", "rdft2", NULL, NULL, "1997",
	"Raiden Fighters 2 - Operation Hell Dive (Japan set 4)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft2jcRomInfo, rdft2jcRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	Rdft2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive (Italy)

static struct BurnRomInfo rdft2itRomDesc[] = {
	{ "seibu1.bin",						0x080000, 0x501b92a9, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu2.bin",						0x080000, 0xec73a767, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu3.bin",						0x080000, 0xe66243b2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu4.bin",						0x080000, 0x92b7b73e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu5.bin",						0x010000, 0x377cac2f, 1 | BRF_GRA },           //  4 Characters
	{ "seibu6.bin",						0x010000, 0x42bd5372, 1 | BRF_GRA },           //  5
	{ "seibu7.bin",						0x010000, 0x1efaac7e, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0535",					0x400000, 0x6143f576, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0536",					0x400000, 0xc607a444, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0538",					0x200000, 0xf0830248, 2 | BRF_GRA },           // 10

	{ "obj3.u0434",						0x400000, 0xe08f42dc, 3 | BRF_GRA },           // 11 Sprites
	{ "obj3b.u0433",					0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 12
	{ "obj2.u0431",						0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 13
	{ "obj2b.u0432",					0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 14
	{ "obj1.u0429",						0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 15
	{ "obj1b.u0430",					0x200000, 0x5259321f, 3 | BRF_GRA },           // 16

	{ "pcm.u0217",						0x200000, 0x2edc30b5, 3 | BRF_PRG | BRF_ESS }, // 17 Sample Data (Mapped to i386)
	{ "seibu8.bin",						0x080000, 0xb7bd3703, 3 | BRF_PRG | BRF_ESS }, // 18
	
	{ "rm81.u0529.bin",					0x000117, 0xacd55c8e, 0 | BRF_OPT },           // 19 PLDs
	{ "rm82.u0330.bin",					0x000117, 0x64c71423, 0 | BRF_OPT },           // 20
	{ "rm83.u0331.bin",					0x000117, 0x6e10d66b, 0 | BRF_OPT },           // 21

	{ "flash0_blank_region92.u1053",	0x100000, 0x204d82d0, 0 | BRF_SND },           // 22 Intel Flash (Samples)
};

STD_ROM_PICK(rdft2it)
STD_ROM_FN(rdft2it)

struct BurnDriver BurnDrvRdft2it = {
	"rdft2it", "rdft2", NULL, NULL, "1997",
	"Raiden Fighters 2 - Operation Hell Dive (Italy)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft2itRomInfo, rdft2itRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	Rdft2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive (Hong Kong)

static struct BurnRomInfo rdft2aRomDesc[] = {
	{ "seibu__1.u0211",					0x080000, 0x046b3f0e, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu__2.u0212",					0x080000, 0xcab55d88, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu__3.u0221",					0x080000, 0x83758b0e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu__4.u0220",					0x080000, 0x084fb5e4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu__5.u0524",					0x010000, 0x6fdf4cf6, 1 | BRF_GRA },           //  4 Characters
	{ "seibu__6.u0518",					0x010000, 0x69b7899b, 1 | BRF_GRA },           //  5
	{ "seibu__7.u0514",					0x010000, 0x99a5fece, 1 | BRF_GRA },           //  6

	{ "raiden-f2bg-1d.u0535",			0x400000, 0x6143f576, 2 | BRF_GRA },           //  7 Background Tiles
	{ "raiden-f2__bg-1p.u0537",			0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  8
	{ "raiden-f2bg-2d.u0536",			0x400000, 0xc607a444, 2 | BRF_GRA },           //  9
	{ "raiden-f2__bg-2p.u0538",			0x200000, 0xf0830248, 2 | BRF_GRA },           // 10

	{ "raiden-f2obj-3.u0434",			0x400000, 0xe08f42dc, 3 | BRF_GRA },           // 11 Sprites
	{ "raiden-f2obj-6.u0433",			0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 12
	{ "raiden-f2obj-2.u0431",			0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 13
	{ "raiden-f2obj-5.u0432",			0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 14
	{ "raiden-f2obj-1.u0429",			0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 15
	{ "raiden-f2obj-4.u0430",			0x200000, 0x5259321f, 3 | BRF_GRA },           // 16

	{ "raiden-f2__pcm.u0217",			0x200000, 0x2edc30b5, 3 | BRF_PRG | BRF_ESS }, // 17 Sample Data (Mapped to i386)
	{ "seibu__8.u0222",					0x080000, 0xb7bd3703, 3 | BRF_PRG | BRF_ESS }, // 18
	
	{ "rm81.u0529.bin",					0x000117, 0xacd55c8e, 0 | BRF_OPT },           // 19 PLDs
	{ "rm82.u0330.bin",					0x000117, 0x64c71423, 0 | BRF_OPT },           // 20
	{ "rm83.u0331.bin",					0x000117, 0x6e10d66b, 0 | BRF_OPT },           // 21

	{ "flash0_blank_region22.u1053",	0x100000, 0x5fee8413, 0 | BRF_SND },           // 22 Intel Flash (Samples)
};

STD_ROM_PICK(rdft2a)
STD_ROM_FN(rdft2a)

static INT32 Rdft2aInit()
{
	DefaultNVRAM = Rdft2DefaultNVRAM;

	return CommonInit(DECRYPT_RISE10, NULL, 0x00282ac, 0x204366);
}

struct BurnDriver BurnDrvRdft2a = {
	"rdft2a", "rdft2", NULL, NULL, "1997",
	"Raiden Fighters 2 - Operation Hell Dive (Hong Kong)\0", NULL, "Seibu Kaihatsu (Metrotainment license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft2aRomInfo, rdft2aRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	Rdft2aInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive (Korea)

static struct BurnRomInfo rdft2aaRomDesc[] = {
	{ "rf2_1.bin",						0x080000, 0x72198410, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "rf2_2.bin",						0x080000, 0xec73a767, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rf2_3.bin",						0x080000, 0xe66243b2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rf2_4.bin",						0x080000, 0x92b7b73e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rf2_5.bin",						0x010000, 0x377cac2f, 1 | BRF_GRA },           //  4 Characters
	{ "rf2_6.bin",						0x010000, 0x42bd5372, 1 | BRF_GRA },           //  5
	{ "rf2_7.bin",						0x010000, 0x1efaac7e, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0535",					0x400000, 0x6143f576, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0536",					0x400000, 0xc607a444, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0538",					0x200000, 0xf0830248, 2 | BRF_GRA },           // 10

	{ "obj3.u0434",						0x400000, 0xe08f42dc, 3 | BRF_GRA },           // 11 Sprites
	{ "obj3b.u0433",					0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 12
	{ "obj2.u0431",						0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 13
	{ "obj2b.u0432",					0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 14
	{ "obj1.u0429",						0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 15
	{ "obj1b.u0430",					0x200000, 0x5259321f, 3 | BRF_GRA },           // 16

	{ "pcm.u0217",						0x200000, 0x2edc30b5, 3 | BRF_PRG | BRF_ESS }, // 17 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xb7bd3703, 3 | BRF_PRG | BRF_ESS }, // 18
	
	{ "rm81.u0529.bin",					0x000117, 0xacd55c8e, 0 | BRF_OPT },           // 19 PLDs
	{ "rm82.u0330.bin",					0x000117, 0x64c71423, 0 | BRF_OPT },           // 20
	{ "rm83.u0331.bin",					0x000117, 0x6e10d66b, 0 | BRF_OPT },           // 21

	{ "flash0_blank_region24.u1053",	0x100000, 0x72a33dc4, 0 | BRF_SND },           // 22 Intel Flash (Samples)
};

STD_ROM_PICK(rdft2aa)
STD_ROM_FN(rdft2aa)

struct BurnDriver BurnDrvRdft2aa = {
	"rdft2aa", "rdft2", NULL, NULL, "1997",
	"Raiden Fighters 2 - Operation Hell Dive (Korea)\0", NULL, "Seibu Kaihatsu (Dream Island license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft2aaRomInfo, rdft2aaRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	Rdft2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive (Taiwan)

static struct BurnRomInfo rdft2tRomDesc[] = {
	{ "prg0",							0x080000, 0x7e8c3acc, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg1",							0x080000, 0x22cb5b68, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2",							0x080000, 0x3eca68dd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3",							0x080000, 0x4124daa4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rf2_5.bin",						0x010000, 0x377cac2f, 1 | BRF_GRA },           //  4 Characters
	{ "rf2_6.bin",						0x010000, 0x42bd5372, 1 | BRF_GRA },           //  5
	{ "rf2_7.bin",						0x010000, 0x1efaac7e, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0535",					0x400000, 0x6143f576, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0536",					0x400000, 0xc607a444, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0538",					0x200000, 0xf0830248, 2 | BRF_GRA },           // 10

	{ "obj3.u0434",						0x400000, 0xe08f42dc, 3 | BRF_GRA },           // 11 Sprites
	{ "obj3b.u0433",					0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 12
	{ "obj2.u0431",						0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 13
	{ "obj2b.u0432",					0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 14
	{ "obj1.u0429",						0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 15
	{ "obj1b.u0430",					0x200000, 0x5259321f, 3 | BRF_GRA },           // 16

	{ "pcm.u0217",						0x200000, 0x2edc30b5, 3 | BRF_PRG | BRF_ESS }, // 17 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xb7bd3703, 3 | BRF_PRG | BRF_ESS }, // 18
	
	{ "rm81.u0529.bin",					0x000117, 0xacd55c8e, 0 | BRF_OPT },           // 19 PLDs
	{ "rm82.u0330.bin",					0x000117, 0x64c71423, 0 | BRF_OPT },           // 20
	{ "rm83.u0331.bin",					0x000117, 0x6e10d66b, 0 | BRF_OPT },           // 21

	{ "flash0_blank_region20.u1053",	0x100000, 0xf2051161, 0 | BRF_SND },           // 22 Intel Flash (Samples)
};

STD_ROM_PICK(rdft2t)
STD_ROM_FN(rdft2t)

struct BurnDriver BurnDrvRdft2t = {
	"rdft2t", "rdft2", NULL, NULL, "1997",
	"Raiden Fighters 2 - Operation Hell Dive (Taiwan)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft2tRomInfo, rdft2tRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	Rdft2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive (Switzerland)

static struct BurnRomInfo rdft2sRomDesc[] = {
	{ "seibu__1.u0211",					0x080000, 0x28b2a185, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "seibu__2.u0212",					0x080000, 0xcab55d88, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "seibu__3.u0221",					0x080000, 0x83758b0e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "seibu__4.u0220",					0x080000, 0x084fb5e4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "seibu__5.u0524",					0x010000, 0x6fdf4cf6, 1 | BRF_GRA },           //  4 Characters
	{ "seibu__6.u0518",					0x010000, 0x69b7899b, 1 | BRF_GRA },           //  5
	{ "seibu__7.u0514",					0x010000, 0x99a5fece, 1 | BRF_GRA },           //  6

	{ "raiden-f2bg-1d.u0535",			0x400000, 0x6143f576, 2 | BRF_GRA },           //  7 Background Tiles
	{ "raiden-f2__bg-1p.u0537",			0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  8
	{ "raiden-f2bg-2d.u0536",			0x400000, 0xc607a444, 2 | BRF_GRA },           //  9
	{ "raiden-f2__bg-2p.u0538",			0x200000, 0xf0830248, 2 | BRF_GRA },           // 10

	{ "raiden-f2obj-3.u0434",			0x400000, 0xe08f42dc, 3 | BRF_GRA },           // 11 Sprites
	{ "raiden-f2obj-6.u0433",			0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 12
	{ "raiden-f2obj-2.u0431",			0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 13
	{ "raiden-f2obj-5.u0432",			0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 14
	{ "raiden-f2obj-1.u0429",			0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 15
	{ "raiden-f2obj-4.u0430",			0x200000, 0x5259321f, 3 | BRF_GRA },           // 16

	{ "raiden-f2__pcm.u0217",			0x200000, 0x2edc30b5, 3 | BRF_PRG | BRF_ESS }, // 17 Sample Data (Mapped to i386)
	{ "seibu__8.u0222",					0x080000, 0xb7bd3703, 3 | BRF_PRG | BRF_ESS }, // 18
	
	{ "rm81.u0529.bin",					0x000117, 0xacd55c8e, 0 | BRF_OPT },           // 19 PLDs
	{ "rm82.u0330.bin",					0x000117, 0x64c71423, 0 | BRF_OPT },           // 20
	{ "rm83.u0331.bin",					0x000117, 0x6e10d66b, 0 | BRF_OPT },           // 21

	{ "flash0_blank_region9c.u1053",	0x100000, 0xd73d640c, 0 | BRF_SND },           // 22 Intel Flash (Samples)
};

STD_ROM_PICK(rdft2s)
STD_ROM_FN(rdft2s)

struct BurnDriver BurnDrvRdft2s = {
	"rdft2s", "rdft2", NULL, NULL, "1997",
	"Raiden Fighters 2 - Operation Hell Dive (Switzerland)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft2sRomInfo, rdft2sRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	Rdft2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters Jet (Germany)

static struct BurnRomInfo rfjetRomDesc[] = {
	{ "prg0.u0211",						0x080000, 0xe5a3b304, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg1.u0212",						0x080000, 0x395e6da7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.u0221",						0x080000, 0x82f7a57e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.u0220",						0x080000, 0xcbdf100d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fix0.u0524",						0x010000, 0x8bc080be, 1 | BRF_GRA },           //  4 Characters
	{ "fix1.u0518",						0x010000, 0xbded85e7, 1 | BRF_GRA },           //  5
	{ "fixp.u0514",						0x010000, 0x015d0748, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0543",					0x400000, 0xedfd96da, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0544",					0x200000, 0xa4cc4631, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0545",					0x200000, 0x731fbb59, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0546",					0x100000, 0x03652c25, 2 | BRF_GRA },           // 10

	{ "obj-1.u0442",					0x800000, 0x58a59896, 3 | BRF_GRA },           // 11 Sprites
	{ "obj-2.u0443",					0x800000, 0xa121d1e3, 3 | BRF_GRA },           // 12
	{ "obj-3.u0444",					0x800000, 0xbc2c0c63, 3 | BRF_GRA },           // 13

	{ "pcm-d.u0227",					0x200000, 0x8ee3ff45, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xd4fc3da1, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region80.u1053",	0x100000, 0xe2adaff5, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rfjet)
STD_ROM_FN(rfjet)

static INT32 RfjetInit()
{
	return CommonInit(DECRYPT_RISE11, NULL, 0x2894c, 0x206082);
}

struct BurnDriver BurnDrvRfjet = {
	"rfjet", NULL, NULL, NULL, "1998",
	"Raiden Fighters Jet (Germany)\0", NULL, "Seibu Kaihatsu (Tuning license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rfjetRomInfo, rfjetRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	RfjetInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters Jet (Japan)

static struct BurnRomInfo rfjetjRomDesc[] = {
	{ "prg0.bin",						0x080000, 0xd82fb71f, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg1.bin",						0x080000, 0x7e21c669, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.bin",						0x080000, 0x2f402d55, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.bin",						0x080000, 0xd619e2ad, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fix0.u0524",						0x010000, 0x8bc080be, 1 | BRF_GRA },           //  4 Characters
	{ "fix1.u0518",						0x010000, 0xbded85e7, 1 | BRF_GRA },           //  5
	{ "fixp.u0514",						0x010000, 0x015d0748, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0543",					0x400000, 0xedfd96da, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0544",					0x200000, 0xa4cc4631, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0545",					0x200000, 0x731fbb59, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0546",					0x100000, 0x03652c25, 2 | BRF_GRA },           // 10

	{ "obj-1.u0442",					0x800000, 0x58a59896, 3 | BRF_GRA },           // 11 Sprites
	{ "obj-2.u0443",					0x800000, 0xa121d1e3, 3 | BRF_GRA },           // 12
	{ "obj-3.u0444",					0x800000, 0xbc2c0c63, 3 | BRF_GRA },           // 13

	{ "pcm-d.u0227",					0x200000, 0x8ee3ff45, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xd4fc3da1, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region01.u1053",	0x100000, 0x7ae7ab76, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rfjetj)
STD_ROM_FN(rfjetj)

static INT32 RfjetjInit()
{
	return CommonInit(DECRYPT_RISE11, NULL, 0x2894c, 0x205f2e);
}

struct BurnDriver BurnDrvRfjetj = {
	"rfjetj", "rfjet", NULL, NULL, "1998",
	"Raiden Fighters Jet (Japan)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rfjetjRomInfo, rfjetjRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	RfjetjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters Jet (US)

static struct BurnRomInfo rfjetuRomDesc[] = {
	{ "prg0u.u0211",					0x080000, 0x15ac2040, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg1.u0212",						0x080000, 0x395e6da7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.u0221",						0x080000, 0x82f7a57e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.u0220",						0x080000, 0xcbdf100d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fix0.u0524",						0x010000, 0x8bc080be, 1 | BRF_GRA },           //  4 Characters
	{ "fix1.u0518",						0x010000, 0xbded85e7, 1 | BRF_GRA },           //  5
	{ "fixp.u0514",						0x010000, 0x015d0748, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0543",					0x400000, 0xedfd96da, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0544",					0x200000, 0xa4cc4631, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0545",					0x200000, 0x731fbb59, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0546",					0x100000, 0x03652c25, 2 | BRF_GRA },           // 10

	{ "obj-1.u0442",					0x800000, 0x58a59896, 3 | BRF_GRA },           // 11 Sprites
	{ "obj-2.u0443",					0x800000, 0xa121d1e3, 3 | BRF_GRA },           // 12
	{ "obj-3.u0444",					0x800000, 0xbc2c0c63, 3 | BRF_GRA },           // 13

	{ "pcm-d.u0227",					0x200000, 0x8ee3ff45, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xd4fc3da1, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region10.u1053",	0x100000, 0x4319d998, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rfjetu)
STD_ROM_FN(rfjetu)

static INT32 RfjetuInit()
{
	return CommonInit(DECRYPT_RISE11, NULL, 0x2894c, 0x206082);
}

struct BurnDriver BurnDrvRfjetu = {
	"rfjetu", "rfjet", NULL, NULL, "1998",
	"Raiden Fighters Jet (US)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rfjetuRomInfo, rfjetuRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	RfjetuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters Jet (Korea)

static struct BurnRomInfo rfjetaRomDesc[] = {
	{ "prg0a.u0211",					0x080000, 0x3418d4f5, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg1.u0212",						0x080000, 0x395e6da7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.u0221",						0x080000, 0x82f7a57e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.u0220",						0x080000, 0xcbdf100d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fix0.u0524",						0x010000, 0x8bc080be, 1 | BRF_GRA },           //  4 Characters
	{ "fix1.u0518",						0x010000, 0xbded85e7, 1 | BRF_GRA },           //  5
	{ "fixp.u0514",						0x010000, 0x015d0748, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0543",					0x400000, 0xedfd96da, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0544",					0x200000, 0xa4cc4631, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0545",					0x200000, 0x731fbb59, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0546",					0x100000, 0x03652c25, 2 | BRF_GRA },           // 10

	{ "obj-1.u0442",					0x800000, 0x58a59896, 3 | BRF_GRA },           // 11 Sprites
	{ "obj-2.u0443",					0x800000, 0xa121d1e3, 3 | BRF_GRA },           // 12
	{ "obj-3.u0444",					0x800000, 0xbc2c0c63, 3 | BRF_GRA },           // 13

	{ "pcm-d.u0227",					0x200000, 0x8ee3ff45, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xd4fc3da1, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region24.u1053",	0x100000, 0x72a33dc4, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rfjeta)
STD_ROM_FN(rfjeta)

static INT32 RfjetaInit()
{
	return CommonInit(DECRYPT_RISE11, NULL, 0x2894c, 0x206082);
}

struct BurnDriver BurnDrvRfjeta = {
	"rfjeta", "rfjet", NULL, NULL, "1998",
	"Raiden Fighters Jet (Korea)\0", NULL, "Seibu Kaihatsu (Dream Island license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rfjetaRomInfo, rfjetaRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	RfjetaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters Jet (Taiwan)

static struct BurnRomInfo rfjettRomDesc[] = {
	{ "prg0.u0211",						0x080000, 0xa4734579, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg1.u0212",						0x080000, 0x5e4ad3a4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.u0221",						0x080000, 0x21c9942e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.u0220",						0x080000, 0xea3657f4, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fix0.u0524",						0x010000, 0x8bc080be, 1 | BRF_GRA },           //  4 Characters
	{ "fix1.u0518",						0x010000, 0xbded85e7, 1 | BRF_GRA },           //  5
	{ "fixp.u0514",						0x010000, 0x015d0748, 1 | BRF_GRA },           //  6

	{ "bg-1d.u0543",					0x400000, 0xedfd96da, 2 | BRF_GRA },           //  7 Background Tiles
	{ "bg-1p.u0544",					0x200000, 0xa4cc4631, 2 | BRF_GRA },           //  8
	{ "bg-2d.u0545",					0x200000, 0x731fbb59, 2 | BRF_GRA },           //  9
	{ "bg-2p.u0546",					0x100000, 0x03652c25, 2 | BRF_GRA },           // 10

	{ "obj-1.u0442",					0x800000, 0x58a59896, 3 | BRF_GRA },           // 11 Sprites
	{ "obj-2.u0443",					0x800000, 0xa121d1e3, 3 | BRF_GRA },           // 12
	{ "obj-3.u0444",					0x800000, 0xbc2c0c63, 3 | BRF_GRA },           // 13

	{ "pcm-d.u0227",					0x200000, 0x8ee3ff45, 3 | BRF_PRG | BRF_ESS }, // 14 Sample Data (Mapped to i386)
	{ "sound1.u0222",					0x080000, 0xd4fc3da1, 3 | BRF_PRG | BRF_ESS }, // 15

	{ "flash0_blank_region20.u1053",	0x100000, 0xf2051161, 0 | BRF_SND },           // 16 Intel Flash (Samples)
};

STD_ROM_PICK(rfjett)
STD_ROM_FN(rfjett)

static INT32 RfjettInit()
{
	return CommonInit(DECRYPT_RISE11, NULL, 0x2894c, 0x206082);
}

struct BurnDriver BurnDrvRfjett = {
	"rfjett", "rfjet", NULL, NULL, "1998",
	"Raiden Fighters Jet (Taiwan)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rfjettRomInfo, rfjettRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, Spi_2buttonDIPInfo,
	RfjettInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters (Taiwan, single board)

static struct BurnRomInfo rdftsRomDesc[] = {
	{ "seibu_1.u0259",					0x080000, 0xe278dddd, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "raiden-f_prg2.u0258",			0x080000, 0x58ccb10c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "raiden-f_prg34.u0262",			0x100000, 0x63f01d17, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "seibu_zprg.u1139",				0x020000, 0xc1fda3e8, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "raiden-f_fix.u0535",				0x020000, 0x2be2936b, 1 | BRF_GRA },           //  4 Characters
	{ "seibu_fix2.u0528",				0x010000, 0x4d87e1ea, 1 | BRF_GRA },           //  5

	{ "gun_dogs_bg1-d.u0526",			0x200000, 0x6a68054c, 2 | BRF_GRA },           //  6 Background Tiles
	{ "gun_dogs_bg1-p.u0531",			0x100000, 0x3400794a, 2 | BRF_GRA },           //  7
	{ "gun_dogs_bg2-d.u0534",			0x200000, 0x61cd2991, 2 | BRF_GRA },           //  8
	{ "gun_dogs_bg2-p.u0530",			0x100000, 0x502d5799, 2 | BRF_GRA },           //  9

	{ "gun_dogs_obj-1.u0322",			0x400000, 0x59d86c99, 3 | BRF_GRA },           // 10 Sprites
	{ "gun_dogs_obj-2.u0324",			0x400000, 0x1ceb0b6f, 3 | BRF_GRA },           // 11
	{ "gun_dogs_obj-3.u0323",			0x400000, 0x36e93234, 3 | BRF_GRA },           // 12

	{ "raiden-f_pcm2.u0975",			0x200000, 0x3f8d4a48, 1 | BRF_SND },           // 13 YMF271 Samples
};

STD_ROM_PICK(rdfts)
STD_ROM_FN(rdfts)

static INT32 RdftsInit()
{
	return CommonInit(DECRYPT_SEI252, NULL, 0x298d0, 0x203f46); // 0x203f69?
}

struct BurnDriver BurnDrvRdfts = {
	"rdfts", "rdft", NULL, NULL, "1996",
	"Raiden Fighters (Taiwan, single board)\0", NULL, "Seibu Kaihatsu (Explorer System Corp. license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdftsRomInfo, rdftsRomName, NULL, NULL, NULL, NULL, Spi_3buttonInputInfo, NULL,
	RdftsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive (US, single board)

static struct BurnRomInfo rdft2usRomDesc[] = {
	{ "prg0.u0259",						0x080000, 0xff3eeec1, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg1.u0258",						0x080000, 0xe2cf77d6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.u0265",						0x080000, 0xcae87e1f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.u0264",						0x080000, 0x83f4fb5f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "zprg.u091",						0x020000, 0xcc543c4f, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "fix0.u0524",						0x010000, 0x6fdf4cf6, 1 | BRF_GRA },           //  5 Characters
	{ "fix1.u0518",						0x010000, 0x69b7899b, 1 | BRF_GRA },           //  6
	{ "fixp.u0514",						0x010000, 0x99a5fece, 1 | BRF_GRA },           //  7

	{ "bg-1d.u0535",					0x400000, 0x6143f576, 2 | BRF_GRA },           //  8 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  9
	{ "bg-2d.u0536",					0x400000, 0xc607a444, 2 | BRF_GRA },           // 10
	{ "bg-2p.u0538",					0x200000, 0xf0830248, 2 | BRF_GRA },           // 11

	{ "obj3.u075",						0x400000, 0xe08f42dc, 3 | BRF_GRA },           // 12 Sprites
	{ "obj3b.u078",						0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 13
	{ "obj2.u074",						0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 14
	{ "obj2b.u077",						0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 15
	{ "obj1.u073",						0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 16
	{ "obj1b.u076",						0x200000, 0x5259321f, 3 | BRF_GRA },           // 17

	{ "pcm.u0103",						0x200000, 0x2edc30b5, 1 | BRF_SND },           // 18 YMF271 Samples
	{ "sound1.u0107",					0x080000, 0x20384b0e, 1 | BRF_SND },           // 19
};

STD_ROM_PICK(rdft2us)
STD_ROM_FN(rdft2us)

static void rdft2us_callback()
{
	static const UINT8 EEPROMData[32] = {
		0x4A, 0x68, 0x37, 0x4A, 0x01, 0x20, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02,	0x00, 0x09, 0x00, 0x00,
		0x03, 0x01, 0x03, 0x01, 0x00, 0x00, 0x07, 0x05,	0x7F, 0xFF, 0x00, 0x00, 0x01, 0x03, 0x02, 0xFA
	};

	memcpy(DefaultEEPROM, EEPROMData, 32);

	EEPROMInit(&seibuspi_eeprom);
	has_eeprom = 1;
}

static INT32 Rdft2usInit()
{
	return CommonInit(DECRYPT_RISE10, rdft2us_callback, 0x282ac, 0x20420e);
}

struct BurnDriver BurnDrvRdft2us = {
	"rdft2us", "rdft2", NULL, NULL, "1997",
	"Raiden Fighters 2 - Operation Hell Dive (US, single board)\0", NULL, "Seibu Kaihatsu (Fabtek license)", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft2usRomInfo, rdft2usRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, NULL,
	Rdft2usInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters Jet (US, single board)

static struct BurnRomInfo rfjetsRomDesc[] = {
	{ "rfj-06.u0259",					0x080000, 0xc835aa7a, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "rfj-07.u0258",					0x080000, 0x3b6ca1ca, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rfj-08.u0265",					0x080000, 0x1f5dd06c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rfj-09.u0264",					0x080000, 0xcc71c402, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rfj-05.u091",					0x040000, 0xa55e8799, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rfj-01.u0524",					0x010000, 0x8bc080be, 1 | BRF_GRA },           //  5 Characters
	{ "rfj-02.u0518",					0x010000, 0xbded85e7, 1 | BRF_GRA },           //  6
	{ "rfj-03.u0514",					0x010000, 0x015d0748, 1 | BRF_GRA },           //  7

	{ "bg-1d.u0535",					0x400000, 0xedfd96da, 2 | BRF_GRA },           //  8 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0xa4cc4631, 2 | BRF_GRA },           //  9
	{ "bg-2d.u0536",					0x200000, 0x731fbb59, 2 | BRF_GRA },           // 10
	{ "bg-2p.u0545",					0x100000, 0x03652c25, 2 | BRF_GRA },           // 11

	{ "obj-1.u073",						0x800000, 0x58a59896, 3 | BRF_GRA },           // 12 Sprites
	{ "obj-2.u074",						0x800000, 0xa121d1e3, 3 | BRF_GRA },           // 13
	{ "obj-3.u075",						0x800000, 0xbc2c0c63, 3 | BRF_GRA },           // 14

	{ "pcm-d.u0103",					0x200000, 0x8ee3ff45, 1 | BRF_SND },           // 15 YMF271 Samples
	{ "rfj-04.u0107",					0x080000, 0xc050da03, 1 | BRF_SND },           // 16

	{ "st93c46.bin",					0x000080, 0x8fe8063b, 7 | BRF_PRG | BRF_ESS }, // 17 Default EEPROM
};

STD_ROM_PICK(rfjets)
STD_ROM_FN(rfjets)

static INT32 RfjetsInit()
{
	has_eeprom = 1;
	EEPROMInit(&seibuspi_eeprom);

	return RfjetInit();
}

struct BurnDriver BurnDrvRfjets = {
	"rfjets", "rfjet", NULL, NULL, "1999",
	"Raiden Fighters Jet (US, single board)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rfjetsRomInfo, rfjetsRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, NULL,
	RfjetsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters Jet (US, single board, test version?)

static struct BurnRomInfo rfjetsaRomDesc[] = {
	{ "rfj-06.u0259",					0x080000, 0xb0c8d47e, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "rfj-07.u0258",					0x080000, 0x17189b39, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rfj-08.u0265",					0x080000, 0xab6d724b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rfj-09.u0264",					0x080000, 0xb119a67c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "rfj-05.u091",					0x040000, 0xa55e8799, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "rfj-01.u0524",					0x010000, 0x8bc080be, 1 | BRF_GRA },           //  5 Characters
	{ "rfj-02.u0518",					0x010000, 0xbded85e7, 1 | BRF_GRA },           //  6
	{ "rfj-03.u0514",					0x010000, 0x015d0748, 1 | BRF_GRA },           //  7

	{ "bg-1d.u0535",					0x400000, 0xedfd96da, 2 | BRF_GRA },           //  8 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0xa4cc4631, 2 | BRF_GRA },           //  9
	{ "bg-2d.u0536",					0x200000, 0x731fbb59, 2 | BRF_GRA },           // 10
	{ "bg-2p.u0545",					0x100000, 0x03652c25, 2 | BRF_GRA },           // 11

	{ "obj-1.u073",						0x800000, 0x58a59896, 3 | BRF_GRA },           // 12 Sprites
	{ "obj-2.u074",						0x800000, 0xa121d1e3, 3 | BRF_GRA },           // 13
	{ "obj-3.u075",						0x800000, 0xbc2c0c63, 3 | BRF_GRA },           // 14

	{ "pcm-d.u0103",					0x200000, 0x8ee3ff45, 1 | BRF_SND },           // 15 YMF271 Samples
	{ "rfj-04.u0107",					0x080000, 0xc050da03, 1 | BRF_SND },           // 16
};

STD_ROM_PICK(rfjetsa)
STD_ROM_FN(rfjetsa)

struct BurnDriver BurnDrvRfjetsa = {
	"rfjetsa", "rfjet", NULL, NULL, "1999",
	"Raiden Fighters Jet (US, single board, test version?)\0", NULL, "Seibu Kaihatsu", "SPI",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rfjetsaRomInfo, rfjetsaRomName, NULL, NULL, NULL, NULL, Spi_2buttonInputInfo, NULL,
	RfjetsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters 2 - Operation Hell Dive 2000 (China, SYS386I)

static struct BurnRomInfo rdft22kcRomDesc[] = {
	{ "prg0-1.267",						0x100000, 0x0d7d6eb8, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg2-3.266",						0x100000, 0xead53e69, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "fix0.524",						0x010000, 0xed11d043, 1 | BRF_GRA },           //  2 Characters
	{ "fix1.518",						0x010000, 0x7036d70a, 1 | BRF_GRA },           //  3
	{ "fix2.514",						0x010000, 0x29b465da, 1 | BRF_GRA },           //  4

	{ "bg-1d.535",						0x400000, 0x6143f576, 2 | BRF_GRA },           //  5 Background Tiles
	{ "bg-1p.544",						0x200000, 0x55e64ef7, 2 | BRF_GRA },           //  6
	{ "bg-2d.536",						0x400000, 0xc607a444, 2 | BRF_GRA },           //  7
	{ "bg-2p.545",						0x200000, 0xf0830248, 2 | BRF_GRA },           //  8

	{ "obj3.075",						0x400000, 0xe08f42dc, 3 | BRF_GRA },           //  9 Sprites
	{ "obj6.078",						0x200000, 0x1b6a523c, 3 | BRF_GRA },           // 10
	{ "obj2.074",						0x400000, 0x7aeadd8e, 3 | BRF_GRA },           // 11
	{ "obj5.077",						0x200000, 0x5d790a5d, 3 | BRF_GRA },           // 12
	{ "obj1.073",						0x400000, 0xc2c50f02, 3 | BRF_GRA },           // 13
	{ "obj4.076",						0x200000, 0x5259321f, 3 | BRF_GRA },           // 14

	{ "pcm0.1022",						0x080000, 0xfd599b35, 4 | BRF_SND },           // 15 MSM6295 #0 Samples

	{ "pcm1.1023",						0x080000, 0x8b716356, 5 | BRF_SND },           // 16 MSM6295 #1 Samples
};

STD_ROM_PICK(rdft22kc)
STD_ROM_FN(rdft22kc)

static void Rdft22kc_callback()
{
	static const UINT8 EEPROMData[32] = {
		0x4A, 0xDC, 0x37, 0x4A, 0x01, 0x20, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02,	0x00, 0x63, 0x00, 0x00,
		0x03, 0x01, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x03, 0x04, 0x3D
	};

	memcpy(DefaultEEPROM, EEPROMData, 32);
}

static INT32 Rdft22kcInit()
{
	return Sys368iCommonInit(DECRYPT_RISE10, Rdft22kc_callback, 0x00282ac, 0x0203926);
}

struct BurnDriver BurnDrvRdft22kc = {
	"rdft22kc", "rdft2", NULL, NULL, "2000",
	"Raiden Fighters 2 - Operation Hell Dive 2000 (China, SYS386I)\0", NULL, "Seibu Kaihatsu", "SYS386I",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rdft22kcRomInfo, rdft22kcRomName, NULL, NULL, NULL, NULL, Sys386iInputInfo, NULL,
	Rdft22kcInit, DrvExit, Sys386Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// Raiden Fighters Jet 2000 (China, SYS386I)

static struct BurnRomInfo rfjet2kcRomDesc[] = {
	{ "prg01.u267",						0x100000, 0x36019fa8, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg23.u266",						0x100000, 0x65695dde, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "rfj-01.524",						0x010000, 0xe9d53007, 1 | BRF_GRA },           //  2 Characters
	{ "rfj-02.518",						0x010000, 0xdd3eabd3, 1 | BRF_GRA },           //  3
	{ "rfj-03.514",						0x010000, 0x0daa8aac, 1 | BRF_GRA },           //  4

	{ "bg-1d.u0535",					0x400000, 0xedfd96da, 2 | BRF_GRA },           //  5 Background Tiles
	{ "bg-1p.u0537",					0x200000, 0xa4cc4631, 2 | BRF_GRA },           //  6
	{ "bg-2d.u0536",					0x200000, 0x731fbb59, 2 | BRF_GRA },           //  7
	{ "bg-2p.u0547",					0x100000, 0x03652c25, 2 | BRF_GRA },           //  8

	{ "obj-1.u073",						0x800000, 0x58a59896, 3 | BRF_GRA },           //  9 Sprites
	{ "obj-2.u074",						0x800000, 0xa121d1e3, 3 | BRF_GRA },           // 10
	{ "obj-3.u0749",					0x800000, 0xbc2c0c63, 3 | BRF_GRA },           // 11

	{ "rfj-05.u1022",					0x080000, 0xfd599b35, 4 | BRF_SND },           // 12 MSM6295 #0 Samples

	{ "rfj-04.u1023",					0x080000, 0x1d10cd08, 5 | BRF_SND },           // 13 MSM6295 #1 Samples
};

STD_ROM_PICK(rfjet2kc)
STD_ROM_FN(rfjet2kc)

static INT32 Rfjet2kcInit()
{
	return Sys368iCommonInit(DECRYPT_RISE11, NULL, 0x2894c, 0x205c9e); // or 0x205cc1
}

struct BurnDriver BurnDrvRfjet2kc = {
	"rfjet2kc", "rfjet", NULL, NULL, "2000",
	"Raiden Fighters Jet 2000 (China, SYS386I)\0", NULL, "Seibu Kaihatsu", "SYS386I",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, rfjet2kcRomInfo, rfjet2kcRomName, NULL, NULL, NULL, NULL, Sys386iInputInfo, NULL,
	Rfjet2kcInit, DrvExit, Sys386Frame, DrvDraw, DrvScan, &DrvRecalc, 0x1800,
	240, 320, 3, 4
};


// E-Jan Sakurasou (Japan, SYS386F V2.0)

static struct BurnRomInfo ejsakuraRomDesc[] = {
	{ "prg0.211",						0x040000, 0x199f2f08, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg1.212",						0x040000, 0x2cb636e6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2.221",						0x040000, 0x98a7b615, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3.220",						0x040000, 0x9c3c037a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "chr4.445",						0x400000, 0x40c6c238, 3 | BRF_GRA },           //  4 Sprites
	{ "chr3.444",						0x400000, 0x8e5d1de5, 3 | BRF_GRA },           //  5
	{ "chr2.443",						0x400000, 0x638dc9ae, 3 | BRF_GRA },           //  6
	{ "chr1.442",						0x400000, 0x177e3139, 3 | BRF_GRA },           //  7

	{ "sound1.83",						0x800000, 0x98783cfc, 1 | BRF_SND },           //  8 YMZ280b Samples
	{ "sound2.84",						0x800000, 0xff37e769, 1 | BRF_SND },           //  9
};

STD_ROM_PICK(ejsakura)
STD_ROM_FN(ejsakura)

struct BurnDriver BurnDrvEjsakura = {
	"ejsakura", NULL, NULL, NULL, "1999",
	"E-Jan Sakurasou (Japan, SYS386F V2.0)\0", NULL, "Seibu Kaihatsu", "SYS386F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, ejsakuraRomInfo, ejsakuraRomName, NULL, NULL, NULL, NULL, EjsakuraInputInfo, NULL,
	Sys386fInit, DrvExit, Sys386fFrame, Sys386fDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 240, 4, 3
};


// E-Jan Sakurasou (Japan, SYS386F V1.2)

static struct BurnRomInfo ejsakura12RomDesc[] = {
	{ "prg0v1.2.u0211",					0x040000, 0xc734fde6, 1 | BRF_PRG | BRF_ESS }, //  0 i386 Code
	{ "prg1v1.2.u0212",					0x040000, 0xfb7a9e38, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prg2v1.2.u0221",					0x040000, 0xe13098ad, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prg3v1.2.u0220",					0x040000, 0x29b5460f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "chr4.445",						0x400000, 0x40c6c238, 3 | BRF_GRA },           //  4 Sprites
	{ "chr3.444",						0x400000, 0x8e5d1de5, 3 | BRF_GRA },           //  5
	{ "chr2.443",						0x400000, 0x638dc9ae, 3 | BRF_GRA },           //  6
	{ "chr1.442",						0x400000, 0x177e3139, 3 | BRF_GRA },           //  7

	{ "sound1.83",						0x800000, 0x98783cfc, 1 | BRF_SND },           //  8 YMZ280b Samples
	{ "sound2.84",						0x800000, 0xff37e769, 1 | BRF_SND },           //  9
};

STD_ROM_PICK(ejsakura12)
STD_ROM_FN(ejsakura12)

struct BurnDriver BurnDrvEjsakura12 = {
	"ejsakura12", "ejsakura", NULL, NULL, "1999",
	"E-Jan Sakurasou (Japan, SYS386F V1.2)\0", NULL, "Seibu Kaihatsu", "SYS386F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_MISC_POST90S, GBF_MAHJONG, 0,
	NULL, ejsakura12RomInfo, ejsakura12RomName, NULL, NULL, NULL, NULL, EjsakuraInputInfo, NULL,
	Sys386fInit, DrvExit, Sys386fFrame, Sys386fDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 240, 4, 3
};
