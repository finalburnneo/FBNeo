// FB Alpha Black Tiger driver module
// Based on MAME driver by Paul Leaman

#include "tiles_generic.h"
#include "z80_intf.h"
#include "mcs51.h"
#include "burn_ym2203.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvPalRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvTxRAM;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *DrvScreenLayout;
static UINT8 *DrvBgEnable;
static UINT8 *DrvFgEnable;
static UINT8 *DrvSprEnable;
static UINT8 *DrvVidBank;
static UINT8 *DrvRomBank;

static UINT8 *DrvZ80Latch;
static UINT8 *DrvMCULatch;
static UINT8 *soundlatch;
static UINT8 *flipscreen;
static UINT8 *coin_lockout;
static INT32 watchdog;

static UINT16 *DrvScrollx;
static UINT16 *DrvScrolly;

static INT32 nCyclesExtra[3];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 use_mcu = 0;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin"       	  , BIT_DIGITAL  , DrvJoy1 + 6,	 "p1 coin"  },
	{"P1 Start"     	  , BIT_DIGITAL  , DrvJoy1 + 0,	 "p1 start" },
	{"P1 Up"        	  , BIT_DIGITAL  , DrvJoy2 + 3,  "p1 up"    },
	{"P1 Down"      	  , BIT_DIGITAL  , DrvJoy2 + 2,  "p1 down"  },
	{"P1 Left"      	  , BIT_DIGITAL  , DrvJoy2 + 1,  "p1 left"  },
	{"P1 Right"     	  , BIT_DIGITAL  , DrvJoy2 + 0,  "p1 right" },
	{"P1 Button 1"  	  , BIT_DIGITAL  , DrvJoy2 + 4,  "p1 fire 1"},
	{"P1 Button 2"  	  , BIT_DIGITAL  , DrvJoy2 + 5,  "p1 fire 2"},

	{"P2 Coin"       	  , BIT_DIGITAL  , DrvJoy1 + 7,	 "p2 coin"  },
	{"P2 Start"     	  , BIT_DIGITAL  , DrvJoy1 + 1,	 "p2 start" },
	{"P2 Up"        	  , BIT_DIGITAL  , DrvJoy3 + 3,  "p2 up"    },
	{"P2 Down"      	  , BIT_DIGITAL  , DrvJoy3 + 2,  "p2 down"  },
	{"P2 Left"      	  , BIT_DIGITAL  , DrvJoy3 + 1,  "p2 left"  },
	{"P2 Right"     	  , BIT_DIGITAL  , DrvJoy3 + 0,  "p2 right" },
	{"P2 Button 1"  	  , BIT_DIGITAL  , DrvJoy3 + 4,  "p2 fire 1"},
	{"P2 Button 2"  	  , BIT_DIGITAL  , DrvJoy3 + 5,  "p2 fire 2"},

	{"Reset",		    BIT_DIGITAL  , &DrvReset,	 "reset"    },
	{"Service",		    BIT_DIGITAL  , DrvJoy1 + 5,  "service"  },
	{"Dip 1",		    BIT_DIPSWITCH, DrvDips + 0,	 "dip"	    },
	{"Dip 2",		    BIT_DIPSWITCH, DrvDips + 1,	 "dip"	    },
	{"Dip 3",		    BIT_DIPSWITCH, DrvDips + 2,	 "dip"	    },
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
 	{0x13, 0xff, 0xff, 0xff, NULL			},
 	{0x14, 0xff, 0x01, 0x01, NULL			},

 	{0x12, 0xfe,    0,    8, "Coin A"		},
 	{0x12, 0x01, 0x07, 0x00, "4 Coin 1 Credits"	},
 	{0x12, 0x01, 0x07, 0x01, "3 Coin 1 Credits"	},
 	{0x12, 0x01, 0x07, 0x02, "2 Coin 1 Credits"	},
 	{0x12, 0x01, 0x07, 0x07, "1 Coin 1 Credits"	},
 	{0x12, 0x01, 0x07, 0x06, "1 Coin 2 Credits"	},
 	{0x12, 0x01, 0x07, 0x05, "1 Coin 3 Credits"	},
 	{0x12, 0x01, 0x07, 0x04, "1 Coin 4 Credits"	},
 	{0x12, 0x01, 0x07, 0x03, "1 Coin 5 Credits"	},

 	{0x12, 0xfe,    0,    8, "Coin B"		},
 	{0x12, 0x01, 0x38, 0x00, "4 Coin 1 Credits"	},
 	{0x12, 0x01, 0x38, 0x08, "3 Coin 1 Credits"	},
 	{0x12, 0x01, 0x38, 0x10, "2 Coin 1 Credits"	},
 	{0x12, 0x01, 0x38, 0x38, "1 Coin 1 Credits"	},
 	{0x12, 0x01, 0x38, 0x30, "1 Coin 2 Credits"	},
 	{0x12, 0x01, 0x38, 0x28, "1 Coin 3 Credits"	},
 	{0x12, 0x01, 0x38, 0x20, "1 Coin 4 Credits"	},
 	{0x12, 0x01, 0x38, 0x18, "1 Coin 5 Credits"	},

 	{0x12, 0xfe,    0,    2, "Flip Screen"		},
 	{0x12, 0x01, 0x40, 0x40, "Off"			},
 	{0x12, 0x01, 0x40, 0x00, "On"			},

 	{0x12, 0xfe,    0,    2, "Test"			},
 	{0x12, 0x01, 0x80, 0x80, "Off"			},
 	{0x12, 0x01, 0x80, 0x00, "On"			},

 	{0x13, 0xfe,    0,    4, "Lives"		},
 	{0x13, 0x01, 0x03, 0x02, "2"			},
 	{0x13, 0x01, 0x03, 0x03, "3"			},
 	{0x13, 0x01, 0x03, 0x01, "5"			},
 	{0x13, 0x01, 0x03, 0x00, "7"			},

 	{0x13, 0xfe,    0,    8, "Difficulty"		},
 	{0x13, 0x01, 0x1c, 0x1c, "1 (Easiest)"		},
 	{0x13, 0x01, 0x1c, 0x18, "2"			},
 	{0x13, 0x01, 0x1c, 0x14, "3"			},
 	{0x13, 0x01, 0x1c, 0x10, "4"			},
 	{0x13, 0x01, 0x1c, 0x0c, "5 (Normal)"		},
 	{0x13, 0x01, 0x1c, 0x08, "6"			},
 	{0x13, 0x01, 0x1c, 0x04, "7"			},
 	{0x13, 0x01, 0x1c, 0x00, "8 (Hardest)"		},

 	{0x13, 0xfe,    0,    2, "Demo Sounds"		},
 	{0x13, 0x01, 0x20, 0x00, "Off"			},
 	{0x13, 0x01, 0x20, 0x20, "On"			},

 	{0x13, 0xfe,    0,    2, "Allow Continue"	},
 	{0x13, 0x01, 0x40, 0x00, "No"			},
 	{0x13, 0x01, 0x40, 0x40, "Yes"			},

 	{0x13, 0xfe,    0,    2, "Cabinet"		},
 	{0x13, 0x01, 0x80, 0x00, "Upright"		},
 	{0x13, 0x01, 0x80, 0x80, "Cocktail"		},

 	{0x14, 0xfe,    0,    2, "Coin Lockout Present?"},
 	{0x14, 0x01, 0x01, 0x01, "Yes"			},
 	{0x14, 0x01, 0x01, 0x00, "No"			},
};

STDDIPINFO(Drv)

static void palette_write(INT32 offset)
{
	UINT8 r,g,b;
	UINT16 data = (DrvPalRAM[offset]) | (DrvPalRAM[offset | 0x400] << 8);

	r = (data >> 4) & 0x0f;
	g = (data >> 0) & 0x0f;
	b = (data >> 8) & 0x0f;

	r |= r << 4;
	g |= g << 4;
	b |= b << 4;

	DrvPalette[offset] = BurnHighCol(r, g, b, 0);
}

static void DrvRomBankswitch(INT32 bank)
{
	*DrvRomBank = bank & 0x0f;

	INT32 nBank = 0x10000 + (bank & 0x0f) * 0x4000;

	ZetMapMemory(DrvZ80ROM0 + nBank, 0x8000, 0xbfff, MAP_ROM);
}

static void DrvVidRamBankswitch(INT32 bank)
{
	*DrvVidBank = bank & 0x03;

	INT32 nBank = (bank & 3) * 0x1000;

	ZetMapMemory(DrvBgRAM + nBank, 0xc000, 0xcfff, MAP_RAM);
}

static void __fastcall blacktiger_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0xd800) {
		DrvPalRAM[address & 0x7ff] = data;

		palette_write(address & 0x3ff);
		return;
	}

	return;
}

static UINT8 __fastcall blacktiger_read(UINT16 /*address*/)
{
	return 0;
}

static void __fastcall blacktiger_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			*soundlatch = data;
		return;

		case 0x01:
			DrvRomBankswitch(data);
		return;

		case 0x03:
			if (DrvDips[2] & 1) {
				*coin_lockout = ~data << 6;
			}
		return;

		case 0x04:
			if (data & 0x20) {
				ZetReset(1);
			}

			*flipscreen  =  0; //data & 0x40; // ignore flipscreen
			*DrvFgEnable = ~data & 0x80;

		return;

		case 0x06:
			watchdog = 0;
		return;

		case 0x07:
			{
				if (use_mcu) {
					mcs51_set_irq_line(MCS51_INT1_LINE, CPU_IRQSTATUS_ACK);
					*DrvZ80Latch = data;
				} else {
					// do nothing
				}
			}
		return;

		case 0x08:
			*DrvScrollx = (*DrvScrollx & 0xff00) | data;
		return;

		case 0x09:
			*DrvScrollx = (*DrvScrollx & 0x00ff) | (data << 8);
		return;

		case 0x0a:
			*DrvScrolly = (*DrvScrolly & 0xff00) | data;
		return;

		case 0x0b:
			*DrvScrolly = (*DrvScrolly & 0x00ff) | (data << 8);
		return;

		case 0x0c:
			*DrvBgEnable  = ~data & 0x02;
			*DrvSprEnable = ~data & 0x04;
		return;

		case 0x0d:
			DrvVidRamBankswitch(data);
		return;

		case 0x0e:
			*DrvScreenLayout = data ? 1 : 0;
		return;
	}
}

static UINT8 __fastcall blacktiger_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
			return DrvInputs[port & 3];

		case 0x03:
		case 0x04:
			return DrvDips[~port & 1];

		case 0x05:
			return 0x01;

		case 0x07:
			return (use_mcu) ? *DrvMCULatch : (ZetDe(-1) >> 8);
	}

	return 0;
}

static void __fastcall blacktiger_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			BurnYM2203Write(0, 0, data);
			return;

		case 0xe001:
			BurnYM2203Write(0, 1, data);
			return;

		case 0xe002:
			BurnYM2203Write(1, 0, data);
			return;

		case 0xe003:
			BurnYM2203Write(1, 1, data);
			return;
	}
}

static UINT8 __fastcall blacktiger_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xc800:
			return *soundlatch;

		case 0xe000:
			return BurnYM2203Read(0, 0);

		case 0xe001:
			return BurnYM2203Read(0, 1);

		case 0xe002:
			return BurnYM2203Read(1, 0);

		case 0xe003:
			return BurnYM2203Read(1, 1);
	}

	return 0;
}

static UINT8 mcu_read_port(INT32 port)
{
	if (port != MCS51_PORT_P0) return 0;

	mcs51_set_irq_line(MCS51_INT1_LINE, CPU_IRQSTATUS_NONE);
	return *DrvZ80Latch;
}

static void mcu_write_port(INT32 port, UINT8 data)
{
	if (port != MCS51_PORT_P0) return;

	*DrvMCULatch = data;
}


static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x050000;
	DrvZ80ROM1	= Next; Next += 0x008000;
	DrvMCUROM   = Next; Next += 0x001000;

	DrvGfxROM0	= Next; Next += 0x020000;
	DrvGfxROM1	= Next; Next += 0x080000;
	DrvGfxROM2	= Next; Next += 0x080000;

	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM0	= Next; Next += 0x001e00;
	DrvZ80RAM1	= Next; Next += 0x000800;

	DrvPalRAM	= Next; Next += 0x000800;
	DrvTxRAM	= Next; Next += 0x000800;
	DrvBgRAM	= Next; Next += 0x004000;
	DrvSprRAM	= Next; Next += 0x000200;
	DrvSprBuf	= Next; Next += 0x000200;

	DrvScreenLayout	= Next; Next += 0x000001;
	DrvBgEnable	= Next; Next += 0x000001;
	DrvFgEnable	= Next; Next += 0x000001;
	DrvSprEnable	= Next; Next += 0x000001;

	DrvVidBank	= Next; Next += 0x000001;
	DrvRomBank	= Next; Next += 0x000001;

	DrvScrollx	= (UINT16*)Next; Next += 0x0001 * sizeof (UINT16);
	DrvScrolly	= (UINT16*)Next; Next += 0x0001 * sizeof (UINT16);

	soundlatch	= Next; Next += 0x000001;
	flipscreen	= Next; Next += 0x000001;
	coin_lockout	= Next; Next += 0x000001;

	DrvZ80Latch = Next; Next += 0x000001;
	DrvMCULatch = Next; Next += 0x000001;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	DrvRomBankswitch(1);
	DrvVidRamBankswitch(1);
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	if (use_mcu) {
		mcs51_reset();
	}

	watchdog = 0;

	nCyclesExtra[0] = nCyclesExtra[1] = nCyclesExtra[2] = 0;

	HiscoreReset();

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4] = { ((0x40000 * 8) / 2) + 4, ((0x40000 * 8) / 2) + 0, 4, 0 };
	INT32 XOffs[16] = { 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3,
			16*16+0, 16*16+1, 16*16+2, 16*16+3, 16*16+8+0, 16*16+8+1, 16*16+8+2, 16*16+8+3 };
	INT32 YOffs[16] = { 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,
			8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x08000);

	GfxDecode(0x0800, 2,  8,  8, Plane + 2, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane + 0, XOffs, YOffs, 0x200, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane + 0, XOffs, YOffs, 0x200, tmp, DrvGfxROM2);

	BurnFree (tmp);
	
	return 0;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	ZetSetIRQLine(0, (nStatus & 1) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		for (INT32 i = 0; i < 5; i++) {
			if (BurnLoadRom(DrvZ80ROM0 + i * 0x10000, 0  + i, 1)) return 1;
		}

		if (BurnLoadRom(DrvZ80ROM1, 5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0, 6, 1)) return 1;

		for (INT32 i = 0; i < 4; i++) {
			if (BurnLoadRom(DrvGfxROM1 + i * 0x10000, 7  + i, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM2 + i * 0x10000, 11 + i, 1)) return 1;
		}

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvTxRAM,   0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,  0xd800, 0xdfff, MAP_ROM); // write in handler
	ZetMapMemory(DrvZ80RAM0, 0xe000, 0xfdff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,  0xfe00, 0xffff, MAP_RAM);
	ZetSetWriteHandler(blacktiger_write);
	ZetSetReadHandler(blacktiger_read);
	ZetSetInHandler(blacktiger_in);
	ZetSetOutHandler(blacktiger_out);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1, 0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(blacktiger_sound_write);
	ZetSetReadHandler(blacktiger_sound_read);
	ZetClose();

	if (use_mcu) {
		bprintf(0, _T("Using i8751 Protection MCU.\n"));
		if (BurnLoadRom(DrvMCUROM + 0x00000, 19, 1)) return 1;

		mcs51_init();
		mcs51_set_program_data(DrvMCUROM);
		mcs51_set_write_handler(mcu_write_port);
		mcs51_set_read_handler(mcu_read_port);
	}

	GenericTilesInit();

	BurnYM2203Init(2, 3579545, &DrvFMIRQHandler, 0);
	BurnTimerAttachZet(3579545);
	BurnYM2203SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.05);
	BurnYM2203SetPSGVolume(1, 0.05);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvInitMCU()
{
	use_mcu = 1;

	return DrvInit();
}

static INT32 DrvExit()
{
	BurnYM2203Exit();
	ZetExit();

	if (use_mcu)
		mcs51_exit();

	GenericTilesExit();

	BurnFreeMemIndex();

	use_mcu = 0;

	return 0;
}

static void draw_bg(INT32 type, INT32 layer)
{
	UINT16 masks[2][4] = { { 0xffff, 0xfff0, 0xff00, 0xf000 }, { 0x8000, 0x800f, 0x80ff, 0x8fff } };
	INT32 scrollx = (*DrvScrollx)      & (0x3ff | (0x200 << type));
	INT32 scrolly = ((*DrvScrolly)+16) & (0x7ff >> type);

	for (INT32 offs = 0; offs < (128*64 | 64*128); offs++)
	{
		INT32 sx, sy, ofst;

		if (type) {		// 1 = 128x64, 0 = 64x128
			sx = (offs & 0x7f);
			sy = (offs >> 7);

			ofst = (sx & 0x0f) + ((sy & 0x0f) << 4) + ((sx & 0x70) << 4) + ((sy & 0x30) << 7);
		} else {
			sx = (offs & 0x3f);
			sy = (offs >> 6);

			ofst = (sx & 0x0f) + ((sy & 0x0f) << 4) + ((sx & 0x30) << 4) + ((sy & 0x70) << 6);
		}

		sx = (sx * 16) - scrollx;
		sy = (sy * 16) - scrolly;

		if (sx < -15) sx += (0x400 << type);
		if (sy < -15) sy += (0x800 >> type);
		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		INT32 attr  = DrvBgRAM[(ofst << 1) + 1];
		INT32 color = (attr >> 3) & 0x0f;
		INT32 code  = DrvBgRAM[ofst << 1] + ((attr & 0x07) << 8);
		INT32 flipx = attr & 0x80;
		INT32 flipy = 0;

		if (*flipscreen) {
			flipx ^= 0x80;
			flipy = 1;

			sx = 240 - sx;
			sy = 208 - sy;
		}

		UINT8 coltab[8] = { 3, 2, 1, 0, 0, 0, 0, 0 };
		INT32 colmask = masks[layer][coltab[color >> 1]];

		{
			UINT8 *gfx = DrvGfxROM1 + (code * 0x100);
			color <<= 4;

			INT32 flip = (flipx ? 0x0f : 0) + (flipy ? 0xf0 : 0);

			for (INT32 y = 0; y < 16; y++, sy++, sx-=16) {
				for (INT32 x = 0; x < 16; x++, sx++) {
					if (sx < 0 || sx >= nScreenWidth || sy < 0 || sy >= nScreenHeight) continue;

					INT32 pxl = gfx[(y*16+x)^flip];

					if (colmask & (1 << pxl)) continue;

					pTransDraw[sy * nScreenWidth + sx] = pxl + color;
				}
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x200 - 4; offs >= 0; offs -= 4)
	{
		INT32 attr = DrvSprBuf[offs+1];
		INT32 sx = DrvSprBuf[offs + 3] - ((attr & 0x10) << 4);
		INT32 sy = DrvSprBuf[offs + 2];
		INT32 code = DrvSprBuf[offs] | ((attr & 0xe0) << 3);
		INT32 color = attr & 0x07;
		INT32 flipx = attr & 0x08;

		if (*flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
		}

		sy -= 16;

		if (sy < -15 || sy >= nScreenHeight || sx < -15 || sx >= nScreenWidth) continue;

		if (*flipscreen) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0x0f, 0x200, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0x0f, 0x200, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0x0f, 0x200, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0x0f, 0x200, DrvGfxROM2);
			}
		}
	}
}

static void draw_text_layer()
{
	for (INT32 offs = 0x40; offs < 0x3c0; offs++)
	{
		INT32 attr  = DrvTxRAM[offs | 0x400];
		INT32 code  = DrvTxRAM[offs] | ((attr & 0xe0) << 3);

		INT32 sx = (offs & 0x1f) << 3;
		INT32 sy = (offs >> 5) << 3;

		if (*flipscreen) {
			Render8x8Tile_Mask_FlipXY(pTransDraw, code, 248 - sx, 216 - (sy - 16), (attr & 0x1f)/*color*/, 2, 3, 0x300, DrvGfxROM0);
		} else {
			Render8x8Tile_Mask(pTransDraw, code, sx, sy - 16, (attr & 0x1f)/*color*/, 2, 3, 0x300, DrvGfxROM0);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x400; i++) {
			palette_write(i);
		}
	}

	BurnTransferClear(0x3ff);

	if (*DrvBgEnable) {
		if (nBurnLayer & 1) draw_bg(*DrvScreenLayout, 1);
	}

	if (*DrvSprEnable) {
		if (nBurnLayer & 2) draw_sprites();
	}

	if (*DrvBgEnable) {
		if (nBurnLayer & 4) draw_bg(*DrvScreenLayout, 0);
	}

	if (*DrvFgEnable) {
		if (nBurnLayer & 8) draw_text_layer();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	if (watchdog >= 180) {
		DrvDoReset(0);
	}
	watchdog++;

	{
		DrvInputs[0] = DrvInputs[1] = DrvInputs[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		DrvInputs[0] |= *coin_lockout;
	}

	ZetNewFrame();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 6000000 / 60, 3579545 / 60, 6000000 / 12 / 60 };
	INT32 nCyclesDone[3] = { nCyclesExtra[0], 0, nCyclesExtra[2] };

	for (INT32 i = 0; i < nInterleave; i++) {
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == 240) {
			if (pBurnDraw) { // draw here gets rid of artefacts when starting game
				DrvDraw();
			}
			memcpy (DrvSprBuf, DrvSprRAM, 0x200); // buffer at rising vblank
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(1);
		CPU_RUN_TIMER(1);
		ZetClose();

		if (use_mcu) {
			CPU_RUN(2, mcs51);
		}
	}

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	// [1] = timer, not needed
	nCyclesExtra[2] = nCyclesDone[2] - nCyclesTotal[2];

	if (pBurnSoundOut) BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);

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
		ZetScan(nAction);
		if (use_mcu)
			mcs51_scan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(nCyclesExtra);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		DrvRomBankswitch(*DrvRomBank);
		DrvVidRamBankswitch(*DrvVidBank);
		ZetClose();
	}

	return 0;
}


// Black Tiger

static struct BurnRomInfo blktigerRomDesc[] = {
	{ "bdu-01a.5e",		0x08000, 0xa8f98f22, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "bdu-02a.6e",		0x10000, 0x7bef96e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bdu-03a.8e",		0x10000, 0x4089e157, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bd-04.9e",		0x10000, 0xed6af6ec, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bd-05.10e",		0x10000, 0xae59b72e, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "bd-06.1l",		0x08000, 0x2cf54274, 2 | BRF_PRG | BRF_ESS }, //  5 - Z80 #0 Code

	{ "bd-15.2n",		0x08000, 0x70175d78, 3 | BRF_GRA },           //  6 - Characters

	{ "bd-12.5b",		0x10000, 0xc4524993, 4 | BRF_GRA },           //  7 - Background Tiles
	{ "bd-11.4b",		0x10000, 0x7932c86f, 4 | BRF_GRA },           //  8
	{ "bd-14.9b",		0x10000, 0xdc49593a, 4 | BRF_GRA },           //  9
	{ "bd-13.8b",		0x10000, 0x7ed7a122, 4 | BRF_GRA },           // 10

	{ "bd-08.5a",		0x10000, 0xe2f17438, 5 | BRF_GRA },           // 11 - Sprites
	{ "bd-07.4a",		0x10000, 0x5fccbd27, 5 | BRF_GRA },           // 12
	{ "bd-10.9a",		0x10000, 0xfc33ccc6, 5 | BRF_GRA },           // 13
	{ "bd-09.8a",		0x10000, 0xf449de01, 5 | BRF_GRA },           // 14

	{ "bd01.8j",		0x00100, 0x29b459e5, 6 | BRF_OPT },           // 15 - Proms (not used)
	{ "bd02.9j",		0x00100, 0x8b741e66, 6 | BRF_OPT },           // 16
	{ "bd03.11k",		0x00100, 0x27201c75, 6 | BRF_OPT },           // 17
	{ "bd04.11l",		0x00100, 0xe5490b68, 6 | BRF_OPT },           // 18

	{ "bd.6k",  		0x01000, 0xac7d14f1, 7 | BRF_PRG },           // 19 I8751 Mcu Code
};

STD_ROM_PICK(blktiger)
STD_ROM_FN(blktiger)

struct BurnDriver BurnDrvBlktiger = {
	"blktiger", NULL, NULL, NULL, "1987",
	"Black Tiger\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_RUNGUN, 0,
	NULL, blktigerRomInfo, blktigerRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInitMCU, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Black Tiger (older)

static struct BurnRomInfo blktigeraRomDesc[] = {
	{ "bdu-01.5e",		0x08000, 0x47b13922, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "bdu-02.6e",		0x10000, 0x2e0daf1b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bdu-03.8e",		0x10000, 0x3b67dfec, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bd-04.9e",		0x10000, 0xed6af6ec, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bd-05.10e",		0x10000, 0xae59b72e, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "bd-06.1l",		0x08000, 0x2cf54274, 2 | BRF_PRG | BRF_ESS }, //  5 - Z80 #0 Code

	{ "bd-15.2n",		0x08000, 0x70175d78, 3 | BRF_GRA },           //  6 - Characters
	
	{ "bd-12.5b",		0x10000, 0xc4524993, 4 | BRF_GRA },           //  7 - Background Tiles
	{ "bd-11.4b",		0x10000, 0x7932c86f, 4 | BRF_GRA },           //  8
	{ "bd-14.9b",		0x10000, 0xdc49593a, 4 | BRF_GRA },           //  9
	{ "bd-13.8b",		0x10000, 0x7ed7a122, 4 | BRF_GRA },           // 10

	{ "bd-08.5a",		0x10000, 0xe2f17438, 5 | BRF_GRA },           // 11 - Sprites
	{ "bd-07.4a",		0x10000, 0x5fccbd27, 5 | BRF_GRA },           // 12
	{ "bd-10.9a",		0x10000, 0xfc33ccc6, 5 | BRF_GRA },           // 13
	{ "bd-09.8a",		0x10000, 0xf449de01, 5 | BRF_GRA },           // 14

	{ "bd01.8j",		0x00100, 0x29b459e5, 6 | BRF_OPT },           // 15 - Proms (not used)
	{ "bd02.9j",		0x00100, 0x8b741e66, 6 | BRF_OPT },           // 16
	{ "bd03.11k",		0x00100, 0x27201c75, 6 | BRF_OPT },           // 17
	{ "bd04.11l",		0x00100, 0xe5490b68, 6 | BRF_OPT },           // 18

	{ "bd.6k",  		0x01000, 0xac7d14f1, 7 | BRF_PRG },           // 19 I8751 Mcu Code
};

STD_ROM_PICK(blktigera)
STD_ROM_FN(blktigera)

struct BurnDriver BurnDrvBlktigera = {
	"blktigera", "blktiger", NULL, NULL, "1987",
	"Black Tiger (older)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_RUNGUN, 0,
	NULL, blktigeraRomInfo, blktigeraRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInitMCU, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Black Tiger (bootleg)

static struct BurnRomInfo blktigerb1RomDesc[] = {
	{ "btiger1.f6",		0x08000, 0x9d8464e8, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "bdu-02a.6e",		0x10000, 0x7bef96e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "btiger3.j6",		0x10000, 0x52c56ed1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bd-04.9e",		0x10000, 0xed6af6ec, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bd-05.10e",		0x10000, 0xae59b72e, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "bd-06.1l",		0x08000, 0x2cf54274, 2 | BRF_PRG | BRF_ESS }, //  5 - Z80 #0 Code

	{ "bd-15.2n",		0x08000, 0x70175d78, 3 | BRF_GRA },           //  6 - Characters

	{ "bd-12.5b",		0x10000, 0xc4524993, 4 | BRF_GRA },           //  7 - Background Tiles
	{ "bd-11.4b",		0x10000, 0x7932c86f, 4 | BRF_GRA },           //  8
	{ "bd-14.9b",		0x10000, 0xdc49593a, 4 | BRF_GRA },           //  9
	{ "bd-13.8b",		0x10000, 0x7ed7a122, 4 | BRF_GRA },           // 10

	{ "bd-08.5a",		0x10000, 0xe2f17438, 5 | BRF_GRA },           // 11 - Sprites
	{ "bd-07.4a",		0x10000, 0x5fccbd27, 5 | BRF_GRA },           // 12
	{ "bd-10.9a",		0x10000, 0xfc33ccc6, 5 | BRF_GRA },           // 13
	{ "bd-09.8a",		0x10000, 0xf449de01, 5 | BRF_GRA },           // 14

	{ "bd01.8j",		0x00100, 0x29b459e5, 6 | BRF_OPT },           // 15 - Proms (not used)
	{ "bd02.9j",		0x00100, 0x8b741e66, 6 | BRF_OPT },           // 16
	{ "bd03.11k",		0x00100, 0x27201c75, 6 | BRF_OPT },           // 17
	{ "bd04.11l",		0x00100, 0xe5490b68, 6 | BRF_OPT },           // 18
};

STD_ROM_PICK(blktigerb1)
STD_ROM_FN(blktigerb1)

struct BurnDriver BurnDrvBlktigerb1 = {
	"blktigerb1", "blktiger", NULL, NULL, "1987",
	"Black Tiger (bootleg set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_RUNGUN, 0,
	NULL, blktigerb1RomInfo, blktigerb1RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Black Tiger (bootleg set 2)

static struct BurnRomInfo blktigerb2RomDesc[] = {
	{ "1.bin",			0x08000, 0x47E2B21E, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "bdu-02a.6e",		0x10000, 0x7bef96e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.bin",			0x10000, 0x52c56ed1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bd-04.9e",		0x10000, 0xed6af6ec, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bd-05.10e",		0x10000, 0xae59b72e, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "bd-06.1l",		0x08000, 0x2cf54274, 2 | BRF_PRG | BRF_ESS }, //  5 - Z80 #0 Code

	{ "bd-15.2n",		0x08000, 0x70175d78, 3 | BRF_GRA },           //  6 - Characters

	{ "bd-12.5b",		0x10000, 0xc4524993, 4 | BRF_GRA },           //  7 - Background Tiles
	{ "bd-11.4b",		0x10000, 0x7932c86f, 4 | BRF_GRA },           //  8
	{ "bd-14.9b",		0x10000, 0xdc49593a, 4 | BRF_GRA },           //  9
	{ "bd-13.8b",		0x10000, 0x7ed7a122, 4 | BRF_GRA },           // 10

	{ "bd-08.5a",		0x10000, 0xe2f17438, 5 | BRF_GRA },           // 11 - Sprites
	{ "bd-07.4a",		0x10000, 0x5fccbd27, 5 | BRF_GRA },           // 12
	{ "bd-10.9a",		0x10000, 0xfc33ccc6, 5 | BRF_GRA },           // 13
	{ "bd-09.8a",		0x10000, 0xf449de01, 5 | BRF_GRA },           // 14

	{ "bd01.8j",		0x00100, 0x29b459e5, 6 | BRF_OPT },           // 15 - Proms (not used)
	{ "bd02.9j",		0x00100, 0x8b741e66, 6 | BRF_OPT },           // 16
	{ "bd03.11k",		0x00100, 0x27201c75, 6 | BRF_OPT },           // 17
	{ "bd04.11l",		0x00100, 0xe5490b68, 6 | BRF_OPT },           // 18
};

STD_ROM_PICK(blktigerb2)
STD_ROM_FN(blktigerb2)

struct BurnDriver BurnDrvblktigerb2 = {
	"blktigerb2", "blktiger", NULL, NULL, "1987",
	"Black Tiger (bootleg set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_RUNGUN, 0,
	NULL, blktigerb2RomInfo, blktigerb2RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Black Tiger / Black Dragon (mixed bootleg?)

static struct BurnRomInfo blktigerb3RomDesc[] = {
	{ "1.5e",			0x08000, 0x47e2b21e, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "2.6e",			0x10000, 0x7bef96e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.8e",			0x10000, 0x52c56ed1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.9e",			0x10000, 0xed6af6ec, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "5.10e",			0x10000, 0xae59b72e, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "6.1l",			0x08000, 0x6dfab115, 2 | BRF_PRG | BRF_ESS }, //  5 - Z80 #0 Code

	{ "15.2n",			0x08000, 0x3821ab29, 3 | BRF_GRA },           //  6 - Characters

	{ "12.5b",			0x10000, 0xc4524993, 4 | BRF_GRA },           //  7 - Background Tiles
	{ "11.4b",			0x10000, 0x7932c86f, 4 | BRF_GRA },           //  8
	{ "14.9b",			0x10000, 0xdc49593a, 4 | BRF_GRA },           //  9
	{ "13.8b",			0x10000, 0x7ed7a122, 4 | BRF_GRA },           // 10

	{ "8.5a",			0x10000, 0xe2f17438, 5 | BRF_GRA },           // 11 - Sprites
	{ "7.4a",			0x10000, 0x5fccbd27, 5 | BRF_GRA },           // 12
	{ "10.9a",			0x10000, 0xfc33ccc6, 5 | BRF_GRA },           // 13
	{ "9.8a",			0x10000, 0xf449de01, 5 | BRF_GRA },           // 14

	{ "bd01.8j",		0x00100, 0x29b459e5, 6 | BRF_OPT },           // 15 - Proms (not used)
	{ "bd02.9j",		0x00100, 0x8b741e66, 6 | BRF_OPT },           // 16
	{ "bd03.11k",		0x00100, 0x27201c75, 6 | BRF_OPT },           // 17
	{ "bd04.11l",		0x00100, 0xe5490b68, 6 | BRF_OPT },           // 18
};

STD_ROM_PICK(blktigerb3)
STD_ROM_FN(blktigerb3)

static void blktigerb3SoundDecode()
{
	UINT8 *buf = (UINT8*)BurnMalloc(0x8000);

	memcpy (buf, DrvZ80ROM1, 0x8000);

	for (INT32 i = 0; i < 0x8000; i++)
	{
		DrvZ80ROM1[i] = buf[BITSWAP16(i, 15,14,13,12,11,10,9,8, 3,4,5,6, 7,2,1,0)];
	}

	BurnFree(buf);
}

static INT32 blktigerb3Init()
{
	INT32 nRet = DrvInit();

	if (nRet == 0)
	{
		blktigerb3SoundDecode();
	}

	return nRet;
}

struct BurnDriver BurnDrvBlktigerb3 = {
	"blktigerb3", "blktiger", NULL, NULL, "1987",
	"Black Tiger / Black Dragon (mixed bootleg?)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_RUNGUN, 0,
	NULL, blktigerb3RomInfo, blktigerb3RomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	blktigerb3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Black Dragon (Japan)

static struct BurnRomInfo blkdrgonRomDesc[] = {
	{ "bd_01.5e",		0x08000, 0x27ccdfbc, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "bd_02.6e",		0x10000, 0x7d39c26f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bd_03.8e",		0x10000, 0xd1bf3757, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bd_04.9e",		0x10000, 0x4d1d6680, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "bd_05.10e",		0x10000, 0xc8d0c45e, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "bd_06.1l",		0x08000, 0x2cf54274, 2 | BRF_PRG | BRF_ESS }, //  5 - Z80 #0 Code

	{ "bd_15.2n",		0x08000, 0x3821ab29, 3 | BRF_GRA },           //  6 - Characters

	{ "bd_12.5b",		0x10000, 0x22d0a4b0, 4 | BRF_GRA },           //  7 - Background Tiles
	{ "bd_11.4b",		0x10000, 0xc8b5fc52, 4 | BRF_GRA },           //  8
	{ "bd_14.9b",		0x10000, 0x9498c378, 4 | BRF_GRA },           //  9
	{ "bd_13.8b",		0x10000, 0x5b0df8ce, 4 | BRF_GRA },           // 10

	{ "bd_08.5a",		0x10000, 0xe2f17438, 5 | BRF_GRA },           // 11 - Sprites
	{ "bd_07.4a",		0x10000, 0x5fccbd27, 5 | BRF_GRA },           // 12
	{ "bd_10.9a",		0x10000, 0xfc33ccc6, 5 | BRF_GRA },           // 13
	{ "bd_09.8a",		0x10000, 0xf449de01, 5 | BRF_GRA },           // 14

	{ "bd01.8j",		0x00100, 0x29b459e5, 6 | BRF_OPT },           // 15 - Proms (not used)
	{ "bd02.9j",		0x00100, 0x8b741e66, 6 | BRF_OPT },           // 16
	{ "bd03.11k",		0x00100, 0x27201c75, 6 | BRF_OPT },           // 17
	{ "bd04.11l",		0x00100, 0xe5490b68, 6 | BRF_OPT },           // 18
	
	{ "bd.6k",  		0x01000, 0xac7d14f1, 7 | BRF_PRG },           // 19 I8751 Mcu Code
};

STD_ROM_PICK(blkdrgon)
STD_ROM_FN(blkdrgon)

struct BurnDriver BurnDrvBlkdrgon = {
	"blkdrgon", "blktiger", NULL, NULL, "1987",
	"Black Dragon (Japan)\0", NULL, "Capcom", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_RUNGUN, 0,
	NULL, blkdrgonRomInfo, blkdrgonRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInitMCU, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Black Dragon (bootleg)

static struct BurnRomInfo blkdrgonbRomDesc[] = {
	{ "a1",				0x08000, 0x7caf2ba0, 1 | BRF_PRG | BRF_ESS }, //  0 - Z80 #0 Code
	{ "blkdrgon.6e",	0x10000, 0x7d39c26f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a3",				0x10000, 0xf4cd0f39, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "blkdrgon.9e",	0x10000, 0x4d1d6680, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "blkdrgon.10e",	0x10000, 0xc8d0c45e, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "bd-06.1l",		0x08000, 0x2cf54274, 2 | BRF_PRG | BRF_ESS }, //  5 - Z80 #0 Code

	{ "b5",				0x08000, 0x852ad2b7, 3 | BRF_GRA },           //  6 - Characters

	{ "blkdrgon.5b",	0x10000, 0x22d0a4b0, 4 | BRF_GRA },           //  7 - Background Tiles
	{ "b1",				0x10000, 0x053ab15c, 4 | BRF_GRA },           //  8
	{ "blkdrgon.9b",	0x10000, 0x9498c378, 4 | BRF_GRA },           //  9
	{ "b3",				0x10000, 0x9dc6e943, 4 | BRF_GRA },           // 10

	{ "bd-08.5a",		0x10000, 0xe2f17438, 5 | BRF_GRA },           // 11 - Sprites
	{ "bd-07.4a",		0x10000, 0x5fccbd27, 5 | BRF_GRA },           // 12
	{ "bd-10.9a",		0x10000, 0xfc33ccc6, 5 | BRF_GRA },           // 13
	{ "bd-09.8a",		0x10000, 0xf449de01, 5 | BRF_GRA },           // 14

	{ "bd01.8j",		0x00100, 0x29b459e5, 6 | BRF_OPT },           // 15 - Proms (not used)
	{ "bd02.9j",		0x00100, 0x8b741e66, 6 | BRF_OPT },           // 16
	{ "bd03.11k",		0x00100, 0x27201c75, 6 | BRF_OPT },           // 17
	{ "bd04.11l",		0x00100, 0xe5490b68, 6 | BRF_OPT },           // 18
};

STD_ROM_PICK(blkdrgonb)
STD_ROM_FN(blkdrgonb)

struct BurnDriver BurnDrvBlkdrgonb = {
	"blkdrgonb", "blktiger", NULL, NULL, "1987",
	"Black Dragon (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARWARE_CAPCOM_MISC, GBF_PLATFORM | GBF_RUNGUN, 0,
	NULL, blkdrgonbRomInfo, blkdrgonbRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};
