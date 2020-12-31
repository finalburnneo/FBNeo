// FinalBurn Neo Kaneko Gals Panic 3 driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "sknsspr.h"
#include "ymz280b.h"
#include "watchdog.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvSprROM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprRegs;
static UINT8 *DrvMCURAM;
static UINT8 *DrvNVRAM;
static UINT16 *DrvPrioBuffer;
static UINT16 *DrvFrameBuffer[3];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles;

static INT32 scrollx[3];
static INT32 scrolly[3];
static INT32 enable[3];
static INT32 fbbright1[3];
static INT32 fbbright2[3];
static INT32 regs1_address[4][2];
static INT32 prio_scrollx;
static INT32 prio_scrolly;
static INT32 regs1[3];
static UINT16 toybox_mcu_com[4];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[3];

static struct BurnInputInfo Galpani3InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 10,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 8,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 9,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 10,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 11,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 8,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 9,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 14,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy3 + 12,	"diag"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 13,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Galpani3)

static struct BurnDIPInfo Galpani3DIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "Test Mode"			},
	{0x12, 0x01, 0x01, 0x01, "Off"					},
	{0x12, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x02, 0x02, "Off"					},
	{0x12, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x04, 0x00, "Off"					},
	{0x12, 0x01, 0x04, 0x04, "On"					},
};

STDDIPINFO(Galpani3)

static void toybox_handle_04_subcommand(UINT8 mcu_subcmd, UINT16* mcu_ram)
{
	UINT8 *src = DrvMCUROM+0x10000;
	INT32 offs = (mcu_subcmd&0x3f)*8;

	UINT16 romstart = src[offs+2] | (src[offs+3]<<8);
	UINT16 romlength = src[offs+4] | (src[offs+5]<<8);
	UINT16 ramdest = BURN_ENDIAN_SWAP_INT16(mcu_ram[0x0012/2]);

	memcpy (DrvMCURAM + ramdest, src + romstart, romlength);
}

static void toybox_mcu_run()
{
	UINT16 *kaneko16_mcu_ram = (UINT16*)DrvMCURAM;
	UINT16 mcu_command  =   BURN_ENDIAN_SWAP_INT16(kaneko16_mcu_ram[0x0010/2]);
	UINT16 mcu_offset   =   BURN_ENDIAN_SWAP_INT16(kaneko16_mcu_ram[0x0012/2]) / 2;
	UINT16 mcu_data     =   BURN_ENDIAN_SWAP_INT16(kaneko16_mcu_ram[0x0014/2]);

	switch (mcu_command >> 8)
	{
		case 0x02:
			memcpy (kaneko16_mcu_ram + mcu_offset, DrvNVRAM, 0x80);
		break;

		case 0x42:
			memcpy (DrvNVRAM, kaneko16_mcu_ram + mcu_offset, 0x80);
		break;

		case 0x03:  // DSW
			kaneko16_mcu_ram[mcu_offset] = BURN_ENDIAN_SWAP_INT16((DrvDips[0] << 8) | 0);
		break;

		case 0x04:  // Protection
			toybox_handle_04_subcommand(mcu_data, kaneko16_mcu_ram);
		break;
	}
}

static void toybox_mcu_com_write(UINT16 data, INT32 select)
{
	toybox_mcu_com[select] = data;
	if (toybox_mcu_com[0] != 0xffff)  return;
	if (toybox_mcu_com[1] != 0xffff)  return;
	if (toybox_mcu_com[2] != 0xffff)  return;
	if (toybox_mcu_com[3] != 0xffff)  return;

	memset (toybox_mcu_com, 0, sizeof(toybox_mcu_com));

	toybox_mcu_run();
}

static void do_rle(INT32 which)
{
	INT32 rle_count = 0;
	INT32 normal_count = 0;
	UINT32 dstaddress = 0;

	UINT8 thebyte;
	UINT32 address = regs1_address[which][1] | (regs1_address[which][0]<<16);
	UINT16 *framebuffer = DrvFrameBuffer[which];

	while (dstaddress<0x40000)
	{
		if (rle_count==0 && normal_count==0)
		{
			thebyte = DrvGfxROM[address & 0xffffff];

			if (thebyte & 0x80)
			{
				normal_count = (thebyte & 0x7f)+1;
				address++;
			}
			else
			{
				rle_count = (thebyte & 0x7f)+1;
				address++;
			}
		}
		else if (rle_count)
		{
			thebyte = DrvGfxROM[address & 0xffffff];
			framebuffer[dstaddress] = BURN_ENDIAN_SWAP_INT16(thebyte);
			dstaddress++;
			rle_count--;

			if (rle_count==0)
			{
				address++;
			}
		}
		else if (normal_count)
		{
			thebyte = DrvGfxROM[address & 0xffffff];
			framebuffer[dstaddress] = BURN_ENDIAN_SWAP_INT16(thebyte);
			dstaddress++;
			normal_count--;
			address++;
		}
	}
}

static void __fastcall galpani3_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x580000:
			toybox_mcu_com_write(data, 0);
		return;

		case 0x600000:
			toybox_mcu_com_write(data, 1);
		return;

		case 0x680000:
			toybox_mcu_com_write(data, 2);
		return;

		case 0x700000:
			toybox_mcu_com_write(data, 3);
		return;

		case 0x800400:
		case 0xa00400:
		case 0xc00400:
			scrollx[(address / 0x200000) & 3] = data;
		return;

		case 0x800c00:
		case 0xa00c00:
		case 0xc00c00:
			scrolly[(address / 0x200000) & 3] = data;
		return;

		case 0x800c02:
		case 0xa00c02:
		case 0xc00c02:
			enable[(address / 0x200000) & 3] = data;
		return;

		case 0x800c06:
		case 0xa00c06:
		case 0xc00c06:
			*((UINT16*)(DrvPalRAM + 0x8600 + ((address / 0x200000) & 3) * 2)) = BURN_ENDIAN_SWAP_INT16(data);
		return;

		case 0x800c10:
		case 0xa00c10:
		case 0xc00c10:
			fbbright1[(address / 0x200000) & 3] = data;
		return;

		case 0x800c12:
		case 0xa00c12:
		case 0xc00c12:
			fbbright2[(address / 0x200000) & 3] = data;
		return;

		case 0x800c18:
		case 0x800c1a:
		case 0xa00c18:
		case 0xa00c1a:
		case 0xc00c18:
		case 0xc00c1a:
			regs1_address[(address / 0x200000) & 3][(address / 2) & 1] = data;
		return;

		case 0x800c1e:
		case 0xa00c1e:
		case 0xc00c1e:
			if ((data & 0xefff) == 0x2000) do_rle((address / 0x200000) & 3);
		return;

		case 0xe80000:
			prio_scrollx = data;
		return;

		case 0xe80002:
			prio_scrolly = data;
		return;

		case 0xf00020:
			YMZ280BSelectRegister(data & 0xff);
		return;

		case 0xf00022:
			YMZ280BWriteRegister(data & 0xff);
		return;

		case 0xf00040:
			BurnWatchdogWrite();
		return;
	}
}

static void __fastcall galpani3_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0xf00021:
			YMZ280BSelectRegister(data);
		return;

		case 0xf00023:
			YMZ280BWriteRegister(data);
		return;
	}
}

static UINT16 __fastcall galpani3_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x780000:
			return 0; // mcu status

		case 0x800c02:
		case 0xa00c04:
		case 0xc00c04:
			return enable[(address / 0x200000) & 3];

		case 0x800c10:
		case 0xa00c10:
		case 0xc00c10:
			return fbbright1[(address / 0x200000) & 3];

		case 0x800c12:
		case 0xa00c12:
		case 0xc00c12:
			return fbbright2[(address / 0x200000) & 3];

		case 0x800c16:
		case 0xa00c16:
		case 0xc00c16:
			regs1[(address / 0x200000) & 3] ^= 1;
			return 0xffff ^ regs1[(address / 0x200000) & 3];

		case 0xf00010:
			return DrvInputs[0];

		case 0xf00012:
			return DrvInputs[1];

		case 0xf00014:
			return DrvInputs[2];

		case 0xf00040:
			return BurnWatchdogRead();
	}

	return 0xff;
}

static UINT8 __fastcall galpani3_read_byte(UINT32 address)
{
	return SekReadWord(address & ~1) >> ((~address & 1) * 8);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	YMZ280BReset();

	BurnWatchdogReset();

	memset (scrollx,  		0, sizeof(scrollx));
	memset (scrolly,  		0, sizeof(scrolly));
	memset (enable,    		0, sizeof(enable));
	memset (fbbright1,		0, sizeof(fbbright1));
	memset (fbbright2, 		0, sizeof(fbbright2));
	memset (regs1_address,	0, sizeof(regs1_address));
	memset (regs1,     		0, sizeof(regs1));
	memset (toybox_mcu_com, 0, sizeof(toybox_mcu_com));
	prio_scrollx = 0;
	prio_scrolly = 0;

	nExtraCycles = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x100000;
	DrvMCUROM			= Next; Next += 0x020000;

	DrvGfxROM			= Next; Next += 0x1000000;
	DrvSprROM			= Next; Next += 0x200000;

	YMZ280BROM			= Next;
	DrvSndROM           = Next; Next += 0x300000;

	DrvPalette			= (UINT32*)Next; Next += 0x4304 * sizeof(UINT32);

	DrvNVRAM			= Next; Next += 0x000080;

	AllRam				= Next;

	Drv68KRAM			= Next; Next += 0x010000;
	DrvPalRAM			= Next; Next += 0x008800;
	DrvSprRAM			= Next; Next += 0x004000;
	DrvSprRegs			= Next; Next += 0x000400;
	DrvMCURAM			= Next; Next += 0x010000;

	DrvPrioBuffer		= (UINT16*)Next; Next += 0x080000;
	DrvFrameBuffer[0]	= (UINT16*)Next; Next += 0x080000;
	DrvFrameBuffer[1]	= (UINT16*)Next; Next += 0x080000;
	DrvFrameBuffer[2]	= (UINT16*)Next; Next += 0x080000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static void toybox_decrypt_rom()
{
	static const UINT8 toybox_mcu_decryption_table[0x100] = {
		0x7b, 0x82, 0xf0, 0xbc, 0x7f, 0x1d, 0xa2, 0xc5, 0x2a, 0xfa, 0x55, 0xee, 0x1a, 0xd0, 0x59, 0x76,
		0x5e, 0x75, 0x79, 0x16, 0xa5, 0xf6, 0x84, 0xed, 0x0f, 0x2e, 0xf2, 0x36, 0x61, 0xac, 0xcd, 0xab,
		0x01, 0x3b, 0x01, 0x87, 0x73, 0xab, 0xce, 0x5d, 0xd4, 0x1d, 0x68, 0x2a, 0x35, 0xea, 0x13, 0x27,
		0x00, 0xaa, 0x46, 0x36, 0x6e, 0x65, 0x80, 0x7e, 0x19, 0xe2, 0x96, 0xab, 0xac, 0xa5, 0x6c, 0x63,
		0x4a, 0x6f, 0x87, 0xf6, 0x6a, 0xac, 0x38, 0xe2, 0x1f, 0x87, 0xf9, 0xaa, 0xf5, 0x41, 0x60, 0xa6,
		0x42, 0xb9, 0x30, 0xf2, 0xc3, 0x1c, 0x4e, 0x4b, 0x08, 0x10, 0x42, 0x32, 0xbf, 0xb2, 0xc5, 0x0f,
		0x7a, 0xab, 0x97, 0xf6, 0xe7, 0xb3, 0x46, 0xf8, 0xec, 0x2b, 0x7d, 0x5f, 0xb1, 0x10, 0x03, 0xe4,
		0x0f, 0x22, 0xdf, 0x8d, 0x10, 0x66, 0xa7, 0x7e, 0x96, 0xbd, 0x5a, 0xaf, 0xaa, 0x43, 0xdf, 0x10,
		0x7c, 0x04, 0xe2, 0x9d, 0x66, 0xd7, 0xf0, 0x02, 0x58, 0x8a, 0x55, 0x17, 0x16, 0xe2, 0xe2, 0x52,
		0xaf, 0xd9, 0xf9, 0x0d, 0x59, 0x70, 0x86, 0x3c, 0x05, 0xd1, 0x52, 0xa7, 0xf0, 0xbf, 0x17, 0xd0,
		0x23, 0x15, 0xfe, 0x23, 0xf2, 0x80, 0x60, 0x6f, 0x95, 0x89, 0x67, 0x65, 0xc9, 0x0e, 0xfc, 0x16,
		0xd6, 0x8a, 0x9f, 0x25, 0x2c, 0x0f, 0x2d, 0xe4, 0x51, 0xb2, 0xa8, 0x18, 0x3a, 0x5d, 0x66, 0xa0,
		0x9f, 0xb0, 0x58, 0xea, 0x78, 0x72, 0x08, 0x6a, 0x90, 0xb6, 0xa4, 0xf5, 0x08, 0x19, 0x60, 0x4e,
		0x92, 0xbd, 0xf1, 0x05, 0x67, 0x4f, 0x24, 0x99, 0x69, 0x1d, 0x0c, 0x6d, 0xe7, 0x74, 0x88, 0x22,
		0x2d, 0x15, 0x7a, 0xa2, 0x37, 0xa9, 0xa0, 0xb0, 0x2c, 0xfb, 0x27, 0xe5, 0x4f, 0xb6, 0xcd, 0x75,
		0xdc, 0x39, 0xce, 0x6f, 0x1f, 0xfe, 0xcc, 0xb5, 0xe6, 0xda, 0xd8, 0xee, 0x85, 0xee, 0x2f, 0x04,
	};

	for (INT32 i = 0; i < 0x20000; i++) {
		DrvMCUROM[i] = DrvMCUROM[i] + toybox_mcu_decryption_table[(i ^ 1) & 0xff];
	}
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvSprROM + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x200000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x400000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x600000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0xe00000, k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0xe00001, k++, 2)) return 1;

		if (BurnLoadRom(DrvSndROM + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM + 0x200000, k++, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM + 0x000000, k++, 1)) return 1;
		BurnByteswap (DrvMCUROM, 0x20000);
		toybox_decrypt_rom();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,						0x000000, 0x17ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,						0x200000, 0x20ffff, MAP_RAM);

	SekMapMemory(DrvPalRAM,						0x280000, 0x287fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,						0x300000, 0x303fff, MAP_RAM);
	SekMapMemory(DrvSprRegs,					0x380000, 0x38003f | 0x3ff, MAP_RAM); // page size is 3ff
	SekMapMemory(DrvMCURAM,						0x400000, 0x40ffff, MAP_RAM);

	SekMapMemory(DrvPalRAM + 0x8000,			0x880000, 0x8803ff, MAP_RAM); // 0-1ff
	SekMapMemory((UINT8*)DrvFrameBuffer[0],		0x900000, 0x97ffff, MAP_RAM);

	SekMapMemory(DrvPalRAM + 0x8200,			0xa80000, 0xa803ff, MAP_RAM); // 0-1ff
	SekMapMemory((UINT8*)DrvFrameBuffer[1],		0xb00000, 0xb7ffff, MAP_RAM);

	SekMapMemory(DrvPalRAM + 0x8400,			0xc80000, 0xc803ff, MAP_RAM); // 0-1ff
	SekMapMemory((UINT8*)DrvFrameBuffer[2],		0xd00000, 0xd7ffff, MAP_RAM);

	SekMapMemory((UINT8*)DrvPrioBuffer,			0xe00000, 0xe7ffff, MAP_RAM);
	SekSetWriteWordHandler(0,					galpani3_write_word);
	SekSetWriteByteHandler(0,					galpani3_write_byte);
	SekSetReadWordHandler(0,					galpani3_read_word);
	SekSetReadByteHandler(0,					galpani3_read_byte);
	SekClose();

	skns_init();

	memset(DrvNVRAM, 0xff, 0x80);

	BurnWatchdogInit(DrvDoReset, 180);

	YMZ280BInit(33333000/2, NULL, 0x300000);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_1, 0.80, BURN_SND_ROUTE_LEFT);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_2, 0.80, BURN_SND_ROUTE_RIGHT);

	GenericTilesInit();
	BurnBitmapAllocate(1, nScreenWidth, nScreenHeight, 0);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	YMZ280BExit();
	YMZ280BROM = NULL;

	skns_exit();
	SekExit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x8608/2; i++)
	{
		UINT8 g = pal5bit(BURN_ENDIAN_SWAP_INT16(p[i]) >> 10);
		UINT8 r = pal5bit(BURN_ENDIAN_SWAP_INT16(p[i]) >> 5);
		UINT8 b = pal5bit(BURN_ENDIAN_SWAP_INT16(p[i]) >> 0);

		DrvPalette[i] = (r << 16) | (g << 8) | b; // 32-bit!!
	}
}

static inline UINT32 alpha_blend(UINT32 d, UINT32 s, UINT32 p)
{
	if (p == 0) return d;

	INT32 a = 255 - p;

	return (((((s & 0xff00ff) * p) + ((d & 0xff00ff) * a)) & 0xff00ff00) |
	        ((((s & 0x00ff00) * p) + ((d & 0x00ff00) * a)) & 0x00ff0000)) >> 8;
}

#define COPY_SPRITE_PIXEL(prio)		\
	{ if ((sprpri == (prio)) && sprdat) dst = DrvPalette[sprline[drawx] & 0x3fff]; }

#define DRAW_BLIT_PIXEL(dat,fb)						\
{													\
	UINT16 pen = dat;								\
	UINT32 pal = DrvPalette[pen];					\
	INT32 alpha = 0xff;								\
	if (BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvPalRAM + pen * 2))) >> 15) {	\
		alpha = fbbright2[fb] & 0xff;				\
	} else {										\
		alpha = fbbright1[fb] & 0xff;				\
	}												\
	if (alpha != 0xff)								\
	{												\
		dst = alpha_blend(dst, pal, alpha);			\
	}												\
	else											\
	{												\
		dst = pal;									\
	}												\
}

#define DRAW_BLIT_PIXELTRANSPEN(dat,fb)				\
{													\
	UINT16 pen = dat;								\
	UINT32 pal = DrvPalette[pen];					\
	INT32 alpha = 0xff;								\
	if (BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvPalRAM + pen * 2))) >> 15) {	\
		alpha = fbbright2[fb] & 0xff;				\
	} else {										\
		alpha = fbbright1[fb] & 0xff;				\
	}												\
	if (alpha != 0xff)								\
	{												\
		if (pal) dst = alpha_blend(dst, pal, alpha);\
	}												\
	else											\
	{												\
	    if (pal) dst = pal;   						\
	}												\
}

#define DRAW_BLITLAYER1()	DRAW_BLIT_PIXEL(dat1+0x4000, 0)
#define DRAW_BLITLAYER2()	DRAW_BLIT_PIXEL(dat2+0x4100, 1)
#define DRAW_BLITLAYER3()	DRAW_BLIT_PIXEL(dat3+0x4200, 2)
#define DRAW_BLITLAYER3TRANSPEN()	DRAW_BLIT_PIXELTRANSPEN(dat3+0x4200, 2)

static INT32 DrvDraw()
{
	DrvPaletteUpdate();

	// Clear sknsspr bitmap register (assumed only used by galpani3)
	UINT32 *sprite_regs = (UINT32*)DrvSprRegs;
	if (~BURN_ENDIAN_SWAP_INT32(sprite_regs[0x04/4]) & 0x04) {
		BurnBitmapFill(1, 0);
	}

	skns_draw_sprites(BurnBitmapGetBitmap(1), (UINT32*)DrvSprRAM, 0x4000, DrvSprROM, 0x200000, (UINT32*)DrvSprRegs, 0);

	for (INT32 drawy = 0; drawy < nScreenHeight; drawy++)
	{
		UINT16 *srcline1 = DrvFrameBuffer[0] + ((drawy+scrolly[0]+11)&0x1ff) * 0x200;
		UINT16 *srcline2 = DrvFrameBuffer[1] + ((drawy+scrolly[1]+11)&0x1ff) * 0x200;
		UINT16 *srcline3 = DrvFrameBuffer[2] + ((drawy+scrolly[2]+11)&0x1ff) * 0x200;

		UINT16 *priline  = DrvPrioBuffer     + ((drawy+prio_scrolly+11)&0x1ff) * 0x200;
		UINT16 *sprline  = BurnBitmapGetPosition(1, 0, drawy);

		UINT8 *bmp = pBurnDraw + drawy * nScreenWidth * nBurnBpp;

		for (INT32 drawx = 0; drawx < nScreenWidth; drawx++)
		{
			INT32 prioffs = (drawx+prio_scrollx+66) & 0x1ff;

			UINT8 dat1 = BURN_ENDIAN_SWAP_INT16(srcline1[(scrollx[0]+drawx+67)&0x1ff]);
			UINT8 dat2 = BURN_ENDIAN_SWAP_INT16(srcline2[(scrollx[1]+drawx+67)&0x1ff]);
			UINT8 dat3 = BURN_ENDIAN_SWAP_INT16(srcline3[(scrollx[2]+drawx+67)&0x1ff]);
			UINT8 pridat = BURN_ENDIAN_SWAP_INT16(priline[prioffs]);
			INT32 sprdat = sprline[drawx] & 0xff;
			INT32 sprpri = sprline[drawx] & 0xc000;
			UINT32 dst = 0;

			if (pridat==0x0f)
			{
				if (nSpriteEnable & 1) COPY_SPRITE_PIXEL(0x0000);
				if (nBurnLayer & 1 && enable[2]) DRAW_BLITLAYER3();
				if (nSpriteEnable & 2) COPY_SPRITE_PIXEL(0x4000);
				if (nBurnLayer & 2 && dat1 && enable[0]) DRAW_BLITLAYER1();
				if (nSpriteEnable & 4) COPY_SPRITE_PIXEL(0x8000);
				if (nBurnLayer & 4 && dat2 && enable[1]) DRAW_BLITLAYER2();
				if (nSpriteEnable & 8) COPY_SPRITE_PIXEL(0xc000);
			}
			else if (pridat==0xcf)
			{
				//   -all-: black lines left-behind (silhouette)   - dec.  23, 2020 (dink)
				if (nSpriteEnable & 1) COPY_SPRITE_PIXEL(0x0000);
				if (nBurnLayer & 1 && enable[0]) DRAW_BLIT_PIXEL(0x4300, 0);
				if (nSpriteEnable & 2) COPY_SPRITE_PIXEL(0x4000);
				if (nBurnLayer & 2 && enable[1]) DRAW_BLIT_PIXEL(0x4301, 1);
				if (nSpriteEnable & 4) COPY_SPRITE_PIXEL(0x8000);
				if (nBurnLayer & 4 && dat3 && enable[2]) DRAW_BLITLAYER3TRANSPEN();
				if (nSpriteEnable & 8) COPY_SPRITE_PIXEL(0xc000);
			}
			else if (pridat==0x30)
			{
				if (nSpriteEnable & 1) COPY_SPRITE_PIXEL(0x0000);
				if (nBurnLayer & 1 && enable[1]) DRAW_BLITLAYER2();
				if (nSpriteEnable & 2) COPY_SPRITE_PIXEL(0x4000);
				if (nBurnLayer & 2 && dat1 && enable[0]) DRAW_BLITLAYER1();
				if (nSpriteEnable & 4) COPY_SPRITE_PIXEL(0x8000);
				if (nBurnLayer & 4 && dat3 && enable[1]) DRAW_BLITLAYER3();
				if (nSpriteEnable & 8) COPY_SPRITE_PIXEL(0xc000);
			}
			else
			{
				// Usually pridat = 0x00.  Used @
				//   Bootup
				//   Minigame: Girl with ice-cubes infront   - april 16, 2020 (dink)
				//   -all-: black lines left-behind (girl)   - dec.  23, 2020 (dink)
				if (nSpriteEnable & 1) COPY_SPRITE_PIXEL(0x0000);
				if (nBurnLayer & 1 && enable[0]) DRAW_BLITLAYER1();
				if (nBurnLayer & 2 && dat2 && enable[1]) DRAW_BLITLAYER2();
				if (nSpriteEnable & 2) COPY_SPRITE_PIXEL(0x4000);
				if (nSpriteEnable & 4) COPY_SPRITE_PIXEL(0x8000);
				if (nBurnLayer & 4 && dat3 && enable[2]) DRAW_BLITLAYER3TRANSPEN();
				if (nSpriteEnable & 8) COPY_SPRITE_PIXEL(0xc000);
			}

			PutPix(bmp + drawx * nBurnBpp, BurnHighCol((dst >> 16) & 0xff, (dst >> 8) & 0xff, dst & 0xff, 0));
		}
	}

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 512;
	INT32 nCyclesTotal[1] = { 14318181 / 60 };
	INT32 nCyclesDone[1] = { nExtraCycles };

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i ==   0) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
		if (i == 128) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
		if (i == 240) {
			if (pBurnDraw) {
				BurnDrvRedraw();
			}
			SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		}
		CPU_RUN(0, Sek);
	}

	if (pBurnSoundOut) {
		YMZ280BRender(pBurnSoundOut, nBurnSoundLen);
	}

	SekClose();

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
    	ba.Data		= AllRam;
		ba.nLen		= RamEnd - AllRam;
		ba.szName	= "All RAM";
		BurnAcb(&ba);

		SekScan(nAction);

		YMZ280BScan(nAction, pnMin);

		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(enable);
		SCAN_VAR(fbbright1);
		SCAN_VAR(fbbright2);
		SCAN_VAR(regs1_address);
		SCAN_VAR(prio_scrollx);
		SCAN_VAR(prio_scrolly);
		SCAN_VAR(regs1);
		SCAN_VAR(toybox_mcu_com);
		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvNVRAM;
		ba.nLen	  = 0x80;
		ba.szName = "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Gals Panic 3 (Euro)

static struct BurnRomInfo galpani3RomDesc[] = {
	{ "g3p0e0.u71",		0x080000, 0xfa681118, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "g3p1e0.u102",	0x080000, 0xf1150f1b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gp320000.1",		0x200000, 0xa0112827, 2 | BRF_GRA },           //  2 Sprites

	{ "gp340000.123",	0x200000, 0xa58a26b1, 3 | BRF_GRA },           //  3 Backgrounds
	{ "gp340100.122",	0x200000, 0x746fe4a8, 3 | BRF_GRA },           //  4
	{ "gp340200.121",	0x200000, 0xe9bc15c8, 3 | BRF_GRA },           //  5
	{ "gp340300.120",	0x200000, 0x59062eef, 3 | BRF_GRA },           //  6
	{ "g3g0j0.101",		0x040000, 0xfbb1e0dc, 3 | BRF_GRA },           //  7
	{ "g3g1j0.100",		0x040000, 0x18edb5f0, 3 | BRF_GRA },           //  8

	{ "gp310100.40",	0x200000, 0x6a0b1d12, 4 | BRF_SND },           //  9 Samples
	{ "gp310000.41",	0x100000, 0x641062ef, 4 | BRF_SND },           // 10

	{ "g3d0x0.134",		0x020000, 0x4ace10f9, 5 | BRF_PRG | BRF_ESS }, // 11 MCU Data
};

STD_ROM_PICK(galpani3)
STD_ROM_FN(galpani3)

struct BurnDriver BurnDrvGalpani3 = {
	"galpani3", NULL, NULL, NULL, "1995",
	"Gals Panic 3 (Euro)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galpani3RomInfo, galpani3RomName, NULL, NULL, NULL, NULL, Galpani3InputInfo, Galpani3DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4300,
	240, 320, 3, 4
};


// Gals Panic 3 (Hong Kong)

static struct BurnRomInfo galpani3hkRomDesc[] = {
	{ "gp3_hk.u71",		0x080000, 0xb8fc7826, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "gp3_hk.u102",	0x080000, 0x658f5fe8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gp320000.1",		0x200000, 0xa0112827, 2 | BRF_GRA },           //  2 Sprites

	{ "gp340000.123",	0x200000, 0xa58a26b1, 3 | BRF_GRA },           //  3 Backgrounds
	{ "gp340100.122",	0x200000, 0x746fe4a8, 3 | BRF_GRA },           //  4
	{ "gp340200.121",	0x200000, 0xe9bc15c8, 3 | BRF_GRA },           //  5
	{ "gp340300.120",	0x200000, 0x59062eef, 3 | BRF_GRA },           //  6
	{ "g3g0h0.101",		0x040000, 0xdca3109a, 3 | BRF_GRA },           //  7
	{ "g3g1h0.100",		0x040000, 0x2ebe6ed0, 3 | BRF_GRA },           //  8

	{ "gp310100.40",	0x200000, 0x6a0b1d12, 4 | BRF_SND },           //  9 Samples
	{ "gp310000.41",	0x100000, 0x641062ef, 4 | BRF_SND },           // 10

	{ "g3d0x0.134",		0x020000, 0x4ace10f9, 5 | BRF_PRG | BRF_ESS }, // 11 MCU Data
};

STD_ROM_PICK(galpani3hk)
STD_ROM_FN(galpani3hk)

struct BurnDriver BurnDrvGalpani3hk = {
	"galpani3hk", "galpani3", NULL, NULL, "1995",
	"Gals Panic 3 (Hong Kong)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galpani3hkRomInfo, galpani3hkRomName, NULL, NULL, NULL, NULL, Galpani3InputInfo, Galpani3DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4300,
	240, 320, 3, 4
};


// Gals Panic 3 (Japan)

static struct BurnRomInfo galpani3jRomDesc[] = {
	{ "g3p0j1.71",		0x080000, 0x52893326, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "g3p1j1.102",		0x080000, 0x05f935b4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gp320000.1",		0x200000, 0xa0112827, 2 | BRF_GRA },           //  2 Sprites

	{ "gp340000.123",	0x200000, 0xa58a26b1, 3 | BRF_GRA },           //  3 Backgrounds
	{ "gp340100.122",	0x200000, 0x746fe4a8, 3 | BRF_GRA },           //  4
	{ "gp340200.121",	0x200000, 0xe9bc15c8, 3 | BRF_GRA },           //  5
	{ "gp340300.120",	0x200000, 0x59062eef, 3 | BRF_GRA },           //  6
	{ "g3g0j0.101",		0x040000, 0xfbb1e0dc, 3 | BRF_GRA },           //  7
	{ "g3g1j0.100",		0x040000, 0x18edb5f0, 3 | BRF_GRA },           //  8

	{ "gp310100.40",	0x200000, 0x6a0b1d12, 4 | BRF_SND },           //  9 Samples
	{ "gp310000.41",	0x100000, 0x641062ef, 4 | BRF_SND },           // 10

	{ "g3d0x0.134",		0x020000, 0x4ace10f9, 5 | BRF_PRG | BRF_ESS }, // 11 MCU Data
};

STD_ROM_PICK(galpani3j)
STD_ROM_FN(galpani3j)

struct BurnDriver BurnDrvGalpani3j = {
	"galpani3j", "galpani3", NULL, NULL, "1995",
	"Gals Panic 3 (Japan)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galpani3jRomInfo, galpani3jRomName, NULL, NULL, NULL, NULL, Galpani3InputInfo, Galpani3DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4300,
	240, 320, 3, 4
};


// Gals Panic 3 (Korea)

static struct BurnRomInfo galpani3kRomDesc[] = {
	{ "g3p0k0.71",		0x080000, 0x98147760, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "g3p1k0.102",		0x080000, 0x27416b22, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "gp320000.1",		0x200000, 0xa0112827, 2 | BRF_GRA },           //  2 Sprites

	{ "gp340000.123",	0x200000, 0xa58a26b1, 3 | BRF_GRA },           //  3 Backgrounds
	{ "gp340100.122",	0x200000, 0x746fe4a8, 3 | BRF_GRA },           //  4
	{ "gp340200.121",	0x200000, 0xe9bc15c8, 3 | BRF_GRA },           //  5
	{ "gp340300.120",	0x200000, 0x59062eef, 3 | BRF_GRA },           //  6
	{ "g3g0k0.101",		0x080000, 0x23d895b0, 3 | BRF_GRA },           //  7
	{ "g3g1k0.100",		0x080000, 0x9b1eac6d, 3 | BRF_GRA },           //  8

	{ "gp310100.40",	0x200000, 0x6a0b1d12, 4 | BRF_SND },           //  9 Samples
	{ "gp310000.41",	0x100000, 0x641062ef, 4 | BRF_SND },           // 10

	{ "g3d0x0.134",		0x020000, 0x4ace10f9, 5 | BRF_PRG | BRF_ESS }, // 11 MCU Data
};

STD_ROM_PICK(galpani3k)
STD_ROM_FN(galpani3k)

struct BurnDriver BurnDrvGalpani3k = {
	"galpani3k", "galpani3", NULL, NULL, "1995",
	"Gals Panic 3 (Korea)\0", NULL, "Kaneko", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, galpani3kRomInfo, galpani3kRomName, NULL, NULL, NULL, NULL, Galpani3InputInfo, Galpani3DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4300,
	240, 320, 3, 4
};
