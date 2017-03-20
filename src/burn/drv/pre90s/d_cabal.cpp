// FB Alpha Cabal driver module
// Based on MAME driver by Carlos A. Lozano

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "seibusnd.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvMainRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[5];
static UINT16 DrvPrvInputs[5];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p1 start"	}, 
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 13,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0xdf, NULL			},

	{0   , 0xfe, 0   ,   16, "Coinage (Mode 1)"	},
	{0x13, 0x01, 0x0f, 0x0a, "6 Coins 1 Credit"	},
	{0x13, 0x01, 0x0f, 0x0b, "5 Coins 1 Credit"	},
	{0x13, 0x01, 0x0f, 0x0c, "4 Coins 1 Credit"	},
	{0x13, 0x01, 0x0f, 0x0d, "3 Coin  1 Credit"	},
	{0x13, 0x01, 0x0f, 0x01, "8 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0e, "2 Coin  1 Credit"	},
	{0x13, 0x01, 0x0f, 0x02, "5 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x03, "3 Coin  2 Credits"	},
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credit"	},
	{0x13, 0x01, 0x0f, 0x04, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x09, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0f, 0x08, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x0f, 0x07, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x0f, 0x06, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Coin A (Mode 2)"	},
	{0x13, 0x01, 0x03, 0x00, "5 Coins 1 Credit"	},
	{0x13, 0x01, 0x03, 0x01, "3 Coins 1 Credit"	},
	{0x13, 0x01, 0x03, 0x02, "2 Coins 1 Credit"	},
	{0x13, 0x01, 0x03, 0x03, "1 Coin  1 Credit"	},

	{0   , 0xfe, 0   ,    4, "Coin B (Mode 2)"	},
	{0x13, 0x01, 0x0c, 0x00, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0c, 0x0c, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x0c, 0x04, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0x0c, 0x00, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Coin Mode"		},
	{0x13, 0x01, 0x10, 0x10, "Mode 1"		},
	{0x13, 0x01, 0x10, 0x00, "Mode 2"		},

	{0   , 0xfe, 0   ,    2, "Invert Buttons"	},
	{0x13, 0x01, 0x20, 0x20, "Off"			},
	{0x13, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x40, 0x40, "Off"			},
	{0x13, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Trackball"		},
	{0x13, 0x01, 0x80, 0x80, "Small"		},
	{0x13, 0x01, 0x80, 0x00, "Large"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x02, "2"			},
	{0x14, 0x01, 0x03, 0x03, "3"			},
	{0x14, 0x01, 0x03, 0x01, "5"			},
	{0x14, 0x01, 0x03, 0x00, "121 (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x0c, "150k 650k 500k+"	},
	{0x14, 0x01, 0x0c, 0x08, "200k 800k 600k+"	},
	{0x14, 0x01, 0x0c, 0x04, "300k 1000k 700k+"	},
	{0x14, 0x01, 0x0c, 0x00, "300k Only"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x30, 0x30, "Easy"			},
	{0x14, 0x01, 0x30, 0x20, "Normal"		},
	{0x14, 0x01, 0x30, 0x10, "Hard"			},
	{0x14, 0x01, 0x30, 0x00, "Very Hard"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x80, 0x80, "On"			},
	{0x14, 0x01, 0x80, 0x00, "Off"			},
};

STDDIPINFO(Drv)

static void cabal_audiocpu_irq_trigger()
{
	//ZetIdle(100);
}

static void __fastcall cabal_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x0c0000:
		case 0x0c0001:
			memcpy (DrvPrvInputs, DrvInputs, 4 * sizeof(UINT16));
		return;

		case 0x0c0080:
		case 0x0c0081:
			flipscreen = data & 0x20;
		return;

		case 0x0e8008:
		case 0x0e8009:
			cabal_audiocpu_irq_trigger(); // no break!
		case 0x0e8000:
		case 0x0e8001:
		case 0x0e8002:
		case 0x0e8003:
		case 0x0e8004:
		case 0x0e8005:
		case 0x0e8006:
		case 0x0e8007:
		case 0x0e800a:
		case 0x0e800b:
		case 0x0e800c:
		case 0x0e800d:
			seibu_main_word_write(address & 0xf, data);
		return;
	}
}

static void __fastcall cabal_main_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x0c0000:
		case 0x0c0001:
			memcpy (DrvPrvInputs, DrvInputs, 4 * sizeof(UINT16));
		return;

		case 0x0c0040:
		case 0x0c0041:
		return; // nop

		case 0x0c0080:
		case 0x0c0081:
			flipscreen = data & 0x20;
		return;

		case 0x0e8008:
		case 0x0e8009:
			cabal_audiocpu_irq_trigger(); // no break!
		case 0x0e8000:
		case 0x0e8001:
		case 0x0e8002:
		case 0x0e8003:
		case 0x0e8004:
		case 0x0e8005:
		case 0x0e8006:
		case 0x0e8007:
		case 0x0e800a:
		case 0x0e800b:
		case 0x0e800c:
		case 0x0e800d:
			seibu_main_word_write(address & 0xf, data);
		return;
	}
}

static UINT16 trackball_read(INT32 offset)
{
	switch (offset & 3)
	{
		case 0:
			return (( DrvInputs[0] - DrvPrvInputs[0]) & 0x00ff)       | (((DrvInputs[2] - DrvPrvInputs[2]) & 0x00ff) << 8);
		case 1:
			return (((DrvInputs[0] - DrvPrvInputs[0]) & 0xff00) >> 8) | (( DrvInputs[2] - DrvPrvInputs[2]) & 0xff00);
		case 2:
			return (( DrvInputs[0] - DrvPrvInputs[1]) & 0x00ff)       | (((DrvInputs[3] - DrvPrvInputs[3]) & 0x00ff) << 8);
		case 3:
			return (((DrvInputs[0] - DrvPrvInputs[1]) & 0xff00) >> 8) | (( DrvInputs[3] - DrvPrvInputs[3]) & 0xff00);
	}

	return 0;
}

static UINT16 __fastcall cabal_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x0a0000:
		case 0x0a0001:
			return (DrvDips[0] | (DrvDips[1] << 8));

		case 0x0a0008:
		case 0x0a0009:
		case 0x0a000a:
		case 0x0a000b:
		case 0x0a000c:
		case 0x0a000d:
		case 0x0a000e:
		case 0x0a000f:
			return trackball_read((address/2)&3);

		case 0x0a0010:
		case 0x0a0011:
			return DrvInputs[4];

		case 0x0e8000:
		case 0x0e8001:
		case 0x0e8002:
		case 0x0e8003:
		case 0x0e8004:
		case 0x0e8005:
		case 0x0e8006:
		case 0x0e8007:
		case 0x0e8008:
		case 0x0e8009:
		case 0x0e800a:
		case 0x0e800b:
		case 0x0e800c:
		case 0x0e800d:
			return seibu_main_word_read(address & 0xf);
	}
	return 0;
}

static UINT8 __fastcall cabal_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x0a0000:
		case 0x0a0001:
			return (DrvDips[0] | (DrvDips[1] << 8)) >> ((~address & 1) * 8);

		case 0x0a0008:
		case 0x0a0009:
		case 0x0a000a:
		case 0x0a000b:
		case 0x0a000c:
		case 0x0a000d:
		case 0x0a000e:
		case 0x0a000f:
			return trackball_read((address/2)&3);

		case 0x0a0010:
		case 0x0a0011:
			return DrvInputs[4] >> ((~address & 1) * 8);

		case 0x0e8000:
		case 0x0e8001:
		case 0x0e8002:
		case 0x0e8003:
		case 0x0e8004:
		case 0x0e8005:
		case 0x0e8006:
		case 0x0e8007:
		case 0x0e8008:
		case 0x0e8009:
		case 0x0e800a:
		case 0x0e800b:
		case 0x0e800c:
		case 0x0e800d:
			return seibu_main_word_read(address & 0xf);
	}
	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	seibu_sound_reset();

	flipscreen = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x050000;
	SeibuZ80DecROM		= Next; Next += 0x010000;
	SeibuZ80ROM		= Next;
	DrvZ80ROM		= Next; Next += 0x020000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x100000;
	DrvGfxROM2		= Next; Next += 0x100000;

	SeibuADPCMData[0] 	= Next;
	DrvSndROM0		= Next; Next += 0x010000;
	SeibuADPCMData[1] 	= Next;
	DrvSndROM1		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x010000;
	
	DrvPalRAM		= Next; Next += 0x000800;
	DrvColRAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;

	SeibuZ80RAM		= Next;
	DrvZ80RAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[2]  = { 0, 4 };
	INT32 XOffs0[8]  = { 3, 2, 1, 0, 8+3, 8+2, 8+1, 8+0 };
	INT32 YOffs0[8]  = { STEP8(0,16) };

	INT32 Plane1[4]  = { 8, 12, 0, 4 };
	INT32 XOffs1[16] = { STEP4(3,-1), STEP4(16+3,-1), STEP4(32*16+3,-1), STEP4(33*16+3,-1) };
	INT32 YOffs1[16] = { STEP16(0,32) };

	INT32 XOffs2[16] = { STEP4(3,-1), STEP4(16+3,-1), STEP4(32+3,-1), STEP4(48+3,-1) };
	INT32 YOffs2[16] = { 30*32, 28*32, 26*32, 24*32, 22*32, 20*32, 18*32, 16*32, 14*32, 12*32, 10*32,  8*32,  6*32,  4*32,  2*32,  0*32 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x008000);

	GfxDecode(0x0400, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x080000);

	GfxDecode(0x1000, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x080000);

	GfxDecode(0x1000, 4, 16, 16, Plane1, XOffs2, YOffs2, 0x400, tmp, DrvGfxROM2);

	BurnFree (tmp);

	return 0;
}

static void adpcm_decode(UINT8 *rom)
{
	for (INT32 i = 0; i < 0x10000; i++) {
		rom[i] = BITSWAP08(rom[i], 7, 5, 3, 1, 6, 4, 2, 0);
	}
}

static INT32 DrvInit(INT32 select)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	// joystick version
	if (select == 0)
	{
		if (BurnLoadRom(DrvMainROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x020001,  2, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x020000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x010000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020001, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040000, 11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040001, 12, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x060000, 13, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x060001, 14, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 15, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001, 16, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x020000, 17, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x020001, 18, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x040000, 19, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x040001, 20, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x060000, 21, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x060001, 22, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 23, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 24, 1)) return 1;
	}

	if (select == 1 || select == 2)
	{
		if (BurnLoadRom(DrvMainROM + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x020001,  2, 2)) return 1;
		if (BurnLoadRom(DrvMainROM + 0x020000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x010000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  8, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000,  9, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 10, 1)) return 1;

		if (select == 1) {
			memcpy (DrvMainROM + 0x30000, DrvMainROM + 0x20000, 0x10000);
		}
	}

	DrvGfxDecode();
	adpcm_decode(DrvSndROM0);
	adpcm_decode(DrvSndROM1);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(DrvMainROM,	0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvMainRAM,	0x040000, 0x04ffff, MAP_RAM);
	SekMapMemory(DrvColRAM,		0x060000, 0x0607ff, MAP_RAM);
	SekMapMemory(DrvVidRAM,		0x080000, 0x0803ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x0e0000, 0x0e07ff, MAP_RAM);
	SekSetWriteWordHandler(0,	cabal_main_write_word);
	SekSetWriteByteHandler(0,	cabal_main_write_byte);
	SekSetReadWordHandler(0,	cabal_main_read_word);
	SekSetReadByteHandler(0,	cabal_main_read_byte);
	SekClose();

	SeibuADPCMDataLen[0] = 0x10000;
	SeibuADPCMDataLen[1] = 0x10000;
	seibu_sound_init(1|8, 0x2000, 3579545, 3579545, 8000);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	seibu_sound_exit();

	BurnFree (AllMem);

	return 0;
}

static inline void DrvRecalcPalette()
{
	UINT16 *ram = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x800/2; i++)
	{
		INT32 c = BURN_ENDIAN_SWAP_INT16(ram[i]);

		UINT8 r = c & 0x0f;
		UINT8 g = c & 0xf0;
		UINT8 b = (c >> 8) & 0x0f;

		DrvPalette[i] = BurnHighCol(r+(r*16), g+(g/16), b+(b*16), 0);
	}
}

static void draw_bg_layer()
{
	UINT16 *ram = (UINT16*)DrvVidRAM;

	for (INT32 offs = 0; offs < 16 * 16; offs++)
	{
		INT32 sx = (offs & 0xf) * 16;
		INT32 sy = offs & 0xf0;

		INT32 code  = ram[offs] & 0xfff;
		INT32 color = ram[offs] >> 12;

		Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0x200, DrvGfxROM1);
	}
}

static void draw_tx_layer()
{
	UINT16 *ram = (UINT16*)DrvColRAM;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 code  = ram[offs] & 0x1ff;
		INT32 color = ram[offs] >> 10;

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 2, 3, 0, DrvGfxROM0);
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)(DrvMainRAM + 0x3800);

	for (INT32 offs = 0x800/2 - 4; offs >= 0; offs -= 4)
	{
		INT32 data0 = ram[offs+0];
		INT32 data1 = ram[offs+1];
		INT32 data2 = ram[offs+2];

		if (data0 & 0x0100)
		{
			int code        = data1 & 0x0fff;
			int color       =(data2 & 0x7800) >> 11;
			int sy          = data0 & 0x00ff;
			int sx          = data2 & 0x01ff;
			int flipx       = data2 & 0x0400;
			int flipy       = 0;

			if (sx > 256) sx -= 512;

			if (flipscreen)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				flipx = !flipx;
				flipy = !flipy;
			}

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x100, DrvGfxROM2);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x100, DrvGfxROM2);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x100, DrvGfxROM2);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x100, DrvGfxROM2);
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvRecalcPalette();
	}

	draw_bg_layer();
	draw_sprites();
	draw_tx_layer();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	ZetNewFrame();

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xff;
		DrvInputs[3] = 0xff;
		DrvInputs[4] = 0xffff;
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[2] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy1[i] & 1) << i;
		}

		seibu_coin_input = 0xfc | (DrvJoy3[1] << 1) | DrvJoy3[0];
	}

	INT32 nSegment;
	INT32 nInterleave = 256;
	INT32 nTotalCycles[2] = { 10000000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = nTotalCycles[0] / nInterleave;
		nCyclesDone[0] += SekRun(nSegment);

		nSegment = nTotalCycles[1] / nInterleave;
		nCyclesDone[1] += ZetRun(nSegment);

		if (i == 240)
			SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			seibu_sound_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}

	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength > 0) {
			seibu_sound_update(pSoundBuf, nSegmentLength);
		}
		seibu_sound_update_cabal(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029719;
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

		seibu_sound_scan(pnMin, nAction);

		DrvRecalc = 1;
	}

	return 0;
}


// Cabal (World, Joystick)

static struct BurnRomInfo cabalRomDesc[] = {
	{ "13.7h",    		0x10000, 0x00abbe0c, 1 | BRF_PRG | BRF_ESS }, //  0 M68k Codc
	{ "11.6h",			0x10000, 0x44736281, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "12.7j",			0x10000, 0xd763a47c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "10.6j",			0x10000, 0x96d5e8af, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "4-3n",			0x02000, 0x4038eff2, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "3-3p",			0x08000, 0xd9defcbf, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "5-6s",			0x04000, 0x6a76955a, 3 | BRF_GRA },           //  6 Characters

	{ "bg_rom1.bin",	0x10000, 0x1023319b, 4 | BRF_GRA },           //  7 Background Tiles
	{ "bg_rom2.bin",	0x10000, 0x3b6d2b09, 4 | BRF_GRA },           //  8
	{ "bg_rom3.bin",	0x10000, 0x420b0801, 4 | BRF_GRA },           //  9
	{ "bg_rom4.bin",	0x10000, 0x77bc7a60, 4 | BRF_GRA },           // 10
	{ "bg_rom5.bin",	0x10000, 0x543fcb37, 4 | BRF_GRA },           // 11
	{ "bg_rom6.bin",	0x10000, 0x0bc50075, 4 | BRF_GRA },           // 12
	{ "bg_rom7.bin",	0x10000, 0xd28d921e, 4 | BRF_GRA },           // 13
	{ "bg_rom8.bin",	0x10000, 0x67e4fe47, 4 | BRF_GRA },           // 14

	{ "sp_rom1.bin",	0x10000, 0x34d3cac8, 5 | BRF_GRA },           // 15 Sprites
	{ "sp_rom2.bin",	0x10000, 0x4e49c28e, 5 | BRF_GRA },           // 16
	{ "sp_rom3.bin",	0x10000, 0x7065e840, 5 | BRF_GRA },           // 17
	{ "sp_rom4.bin",	0x10000, 0x6a0e739d, 5 | BRF_GRA },           // 18
	{ "sp_rom5.bin",	0x10000, 0x0e1ec30e, 5 | BRF_GRA },           // 19
	{ "sp_rom6.bin",	0x10000, 0x581a50c1, 5 | BRF_GRA },           // 20
	{ "sp_rom7.bin",	0x10000, 0x55c44764, 5 | BRF_GRA },           // 21
	{ "sp_rom8.bin",	0x10000, 0x702735c9, 5 | BRF_GRA },           // 22

	{ "2-1s",			0x10000, 0x850406b4, 6 | BRF_SND },           // 23 ADPCM #0 Code

	{ "1-1u",			0x10000, 0x8b3e0789, 7 | BRF_SND },           // 24 ADPCM #1 Code
};

STD_ROM_PICK(cabal)
STD_ROM_FN(cabal)

static INT32 CabalInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvCabal = {
	"cabal", NULL, NULL, NULL, "1988",
	"Cabal (World, Joystick)\0", NULL, "TAD Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, cabalRomInfo, cabalRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	CabalInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 256, 4, 3
};


// Cabal (Korea?, Joystick)

static struct BurnRomInfo cabalaRomDesc[] = {
	{ "epr-a-9.7h",    	0x10000, 0x00abbe0c, 1 | BRF_PRG | BRF_ESS }, //  0 M68k Codc
	{ "epr-a-7.6h",		0x10000, 0xc89608db, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "epr-a-8.7k",		0x08000, 0xfe84788a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "epr-a-6.6k",		0x08000, 0x81eb1355, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "epr-a-4.3n",		0x02000, 0x4038eff2, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "epr-a-3.3p",		0x04000, 0xc0097c55, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "epr-a-5.6s",		0x08000, 0x189033fd, 3 | BRF_GRA },           //  6 Characters

	{ "tad-2.7s",		0x80000, 0x13ca7ae1, 4 | BRF_GRA },           //  7 Background Tiles
	
	{ "tad-1.5e",		0x80000, 0x8324a7fe, 5 | BRF_GRA },           // 15 Sprites
	
	{ "epr-a-2.1s",		0x10000, 0x850406b4, 6 | BRF_SND },           // 23 ADPCM #0 Code

	{ "epr-a-1.1u",		0x10000, 0x8b3e0789, 7 | BRF_SND },           // 24 ADPCM #1 Code
};

STD_ROM_PICK(cabala)
STD_ROM_FN(cabala)

static INT32 CabalaInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvCabala = {
	"cabala", "cabal", NULL, NULL, "1988",
	"Cabal (korea?, Joystick)\0", NULL, "TAD Corporation (Alpha Trading license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, cabalaRomInfo, cabalaRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	CabalaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 256, 4, 3
};


// Cabal (UK, Trackball)

static struct BurnRomInfo cabalukRomDesc[] = {
	{ "9-7H.BIN",    	0x10000, 0xf66378e5, 1 | BRF_PRG | BRF_ESS }, //  0 M68k Codc
	{ "7-6H.BIN",		0x10000, 0x960991ac, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "8-7K.BIN",		0x10000, 0x82160ab0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6-6K.BIN",		0x10000, 0x7ef2ecc7, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "4-3n",			0x02000, 0x4038eff2, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "3-3p",			0x08000, 0xd9defcbf, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "5-6s",			0x04000, 0x6a76955a, 3 | BRF_GRA },           //  6 Characters

	{ "tad-2.7s",		0x80000, 0x13ca7ae1, 4 | BRF_GRA },           //  7 Background Tiles
	
	{ "tad-1.5e",		0x80000, 0x8324a7fe, 5 | BRF_GRA },           // 15 Sprites
	
	{ "2-1s",			0x10000, 0x850406b4, 6 | BRF_SND },           // 23 ADPCM #0 Code

	{ "1-1u",			0x10000, 0x8b3e0789, 7 | BRF_SND },           // 24 ADPCM #1 Code
};

STD_ROM_PICK(cabaluk)
STD_ROM_FN(cabaluk)

static INT32 CabalukInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvCabaluk = {
	"cabaluk", "cabal", NULL, NULL, "1988",
	"Cabal (UK, Trackball)\0", "Needs Trackball Support", "TAD Corporation (Electrocoin license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, cabalukRomInfo, cabalukRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	CabalukInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 256, 4, 3
};


// Cabal (US set 1, Trackball)

static struct BurnRomInfo cabalusRomDesc[] = {
	{ "h7_512.bin",    	0x10000, 0x8fe16fb4, 1 | BRF_PRG | BRF_ESS }, //  0 M68k Codc
	{ "h6_512.bin",		0x10000, 0x6968101c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "k7_512.bin",		0x10000, 0x562031a2, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "k6_512.bin",		0x10000, 0x4fda2856, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "4-3n",			0x02000, 0x4038eff2, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "3-3p",			0x08000, 0xd9defcbf, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "t6_128.bin",		0x04000, 0x1ccee214, 3 | BRF_GRA },           //  6 Characters

	{ "tad-2.7s",		0x80000, 0x13ca7ae1, 4 | BRF_GRA },           //  7 Background Tiles
	
	{ "tad-1.5e",		0x80000, 0x8324a7fe, 5 | BRF_GRA },           // 15 Sprites
	
	{ "2-1s",			0x10000, 0x850406b4, 6 | BRF_SND },           // 23 ADPCM #0 Code

	{ "1-1u",			0x10000, 0x8b3e0789, 7 | BRF_SND },           // 24 ADPCM #1 Code
	
	{ "prom05.8e",		0x00100, 0xa94b18c2, 8 | BRF_OPT },           // 25 Proms
	{ "prom10.4j",		0x00100, 0x261c93bc, 8 | BRF_OPT },           // 26 
};

STD_ROM_PICK(cabalus)
STD_ROM_FN(cabalus)

static INT32 CabalusInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvCabalus = {
	"cabalus", "cabal", NULL, NULL, "1988",
	"Cabal (US set 1, Trackball)\0", "Needs Trackball Support", "TAD Corporation (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, cabalusRomInfo, cabalusRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	CabalusInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 256, 4, 3
};


// Cabal (US set 2, Trackball)

static struct BurnRomInfo cabalus2RomDesc[] = {
	{ "9-7h",    		0x10000, 0xebbb9484, 1 | BRF_PRG | BRF_ESS }, //  0 M68k Codc
	{ "7-6h",			0x10000, 0x51aeb49e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "8-7k",			0x10000, 0x4c24ed9a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "6-6k",			0x10000, 0x681620e8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "4-3n",			0x02000, 0x4038eff2, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code
	{ "3-3p",			0x08000, 0xd9defcbf, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "5-6s",			0x04000, 0x6a76955a, 3 | BRF_GRA },           //  6 Characters

	{ "tad-2.7s",		0x80000, 0x13ca7ae1, 4 | BRF_GRA },           //  7 Background Tiles
	
	{ "tad-1.5e",		0x80000, 0x8324a7fe, 5 | BRF_GRA },           // 15 Sprites
	
	{ "2-1s",			0x10000, 0x850406b4, 6 | BRF_SND },           // 23 ADPCM #0 Code

	{ "1-1u",			0x10000, 0x8b3e0789, 7 | BRF_SND },           // 24 ADPCM #1 Code
	
	{ "prom05.8e",		0x00100, 0xa94b18c2, 8 | BRF_OPT },           // 25 Proms
	{ "prom10.4j",		0x00100, 0x261c93bc, 8 | BRF_OPT },           // 26 
};

STD_ROM_PICK(cabalus2)
STD_ROM_FN(cabalus2)

static INT32 Cabalus2Init()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvCabalus2 = {
	"cabalus2", "cabal", NULL, NULL, "1988",
	"Cabal (US set 2, Trackball)\0", "Needs Trackball Support", "TAD Corporation (Fabtek license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, cabalus2RomInfo, cabalus2RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Cabalus2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 256, 4, 3
};
