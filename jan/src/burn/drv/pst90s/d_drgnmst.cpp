// FB Alpha Dragon Master driver module
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
static UINT8 *DrvGfxROM3;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *Drv68KRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvMidRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvRowScroll;
static UINT8 *DrvVidRegs;

static UINT8 DrvReset;
static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[2];
static UINT16 DrvInps[3];

static UINT32  *DrvPalette;
static UINT32  *Palette;

static UINT8 DrvRecalc;

static UINT16 *priority_control;
static UINT8  *coin_lockout;

static UINT8 pic16c5x_port0 = 0;
static UINT8 drgnmst_oki_control = 0;
static UINT8 drgnmst_snd_command = 0;
static UINT8 drgnmst_snd_flag = 0;
static UINT8 drgnmst_oki0_bank = 0;
static UINT8 drgnmst_oki1_bank = 0;
static UINT8 drgnmst_oki_command = 0;

static INT32  nCyclesDone[2] = { 0, 0 };

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy3 + 8,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy3 + 9,	"p1 fire 6"	},
	{"P1 Button 7",		BIT_DIGITAL,	DrvJoy3 + 10,	"p1 fire 7"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy3 + 12,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	DrvJoy3 + 13,	"p2 fire 6"	},
	{"P2 Button 7",		BIT_DIGITAL,	DrvJoy3 + 14,	"p2 fire 7"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	// Default Values
	{0x1b, 0xff, 0xff, 0xa7, NULL			},
	{0x1c, 0xff, 0xff, 0xfc, NULL			},

	{0   , 0xfe, 0   ,    7, "Coinage"		},
	{0x1b, 0x01, 0x07, 0x00, "4 Coins 1 Credits "	},
	{0x1b, 0x01, 0x07, 0x01, "3 Coins 1 Credits "	},
	{0x1b, 0x01, 0x07, 0x02, "2 Coins 1 Credits "	},
	{0x1b, 0x01, 0x07, 0x07, "1 Coin 1 Credits "	},
	{0x1b, 0x01, 0x07, 0x06, "1 Coin 2 Credits "	},
	{0x1b, 0x01, 0x07, 0x05, "1 Coin 3 Credits "	},
	{0x1b, 0x01, 0x07, 0x04, "1 Coin 4 Credits "	},

	{0   , 0xfe, 0   ,    2, "Continue"		},
	{0x1b, 0x01, 0x08, 0x08, "Off"			},
	{0x1b, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo_Sounds"		},
	{0x1b, 0x01, 0x10, 0x10, "Off"			},
	{0x1b, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Two credits to start"	},
	{0x1b, 0x01, 0x20, 0x20, "Off"			},
	{0x1b, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x1b, 0x01, 0x40, 0x40, "Off"			},
	{0x1b, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Game Pause"		},
	{0x1b, 0x01, 0x80, 0x80, "Off"			},
	{0x1b, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    8, "Difficulty"		},
	{0x1c, 0x01, 0x07, 0x07, "Easiest"		},
	{0x1c, 0x01, 0x07, 0x06, "Easier"		},
	{0x1c, 0x01, 0x07, 0x05, "Easy"			},
	{0x1c, 0x01, 0x07, 0x04, "Normal"		},
	{0x1c, 0x01, 0x07, 0x03, "Medium"		},
	{0x1c, 0x01, 0x07, 0x02, "Hard"			},
	{0x1c, 0x01, 0x07, 0x01, "Harder"		},
	{0x1c, 0x01, 0x07, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x1c, 0x01, 0x08, 0x08, "English"		},
	{0x1c, 0x01, 0x08, 0x00, "Korean"		},

	{0   , 0xfe, 0   ,    2, "Game Time"		},
	{0x1c, 0x01, 0x10, 0x10, "Normal"		},
	{0x1c, 0x01, 0x10, 0x00, "Short"		},

	{0   , 0xfe, 0   ,    2, "Stage Skip"		},
	{0x1c, 0x01, 0x20, 0x20, "Off"			},
	{0x1c, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Spit Color"		},
	{0x1c, 0x01, 0x40, 0x40, "Grey"			},
	{0x1c, 0x01, 0x40, 0x00, "Red"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x1c, 0x01, 0x80, 0x80, "Off"			},
	{0x1c, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Drv)

static void palette_write(INT32 offset)
{
	UINT8 r, g, b;
	UINT16 data = *((UINT16*)(DrvPalRAM + offset));	

	r = (data >> 8) & 0x0f;
	r |= r << 4;

	g = (data >> 4) & 0x0f;
	g |= g << 4;

	b = (data >> 0) & 0x0f;
	b |= b << 4;

	Palette[offset >> 1] = (r << 16) | (g << 8) | b;
	DrvPalette[offset >> 1] = BurnHighCol(r, g, b, 0);
}

void __fastcall drgnmst_write_byte(UINT32 address, UINT8 data)
{

	if ((address & 0xffc000) == 0x900000) {
		DrvPalRAM[(address & 0x3fff)] = data;

		palette_write(address & 0x3ffe);

		return;
	}

	if (address >= 0x800100 && address <= 0x80011f) {
		DrvVidRegs[address & 0x1f] = data;
		return;
	}


	switch (address)
	{
		case 0x800030:
		case 0x800031:
			*coin_lockout = (~data >> 2) & 3;
		return;

		case 0x800154:
		case 0x800155:
		// priority control
		return;

		case 0x800181:
	//bprintf (PRINT_NORMAL, _T("%5.5x %2.2x, wb\n"), address, data);
			drgnmst_snd_command = data;
		//	hack = 1;
		//	pic16c5xRun(1000);
			SekRunEnd();//?
			// cpu_yield();
		return;

		case 0x800188:
		case 0x800189:
			drgnmst_snd_flag = 1;
		return;
	}
}

void __fastcall drgnmst_write_word(UINT32 address, UINT16 data)
{

	if ((address & 0xffc000) == 0x900000) {
		*((UINT16 *)(DrvPalRAM + (address & 0x3ffe))) = data;

		palette_write(address & 0x3ffe);

		return;
	}


	if (address >= 0x800100 && address <= 0x80011f) {
		*((UINT16*)(DrvVidRegs + (address & 0x1e))) = data;
		return;
	}

	switch (address)
	{
		case 0x800154:
			*priority_control = data;
		return;

		case 0x800180:
		case 0x800181:
		{
	bprintf (PRINT_NORMAL, _T("%5.5x %4.4x, ww\n"), address, data);

			drgnmst_snd_command = (data & 0xff);
			INT32 cycles = (SekTotalCycles() / 3) - nCyclesDone[1];
			nCyclesDone[1] += pic16c5xRun(cycles);
			//SekRunEnd();
			// cpu_yield();
		}
		return;

		case 0x800188:
		case 0x800189:
			drgnmst_snd_flag = 1;
		return;
	}
}

UINT8 __fastcall drgnmst_read_byte(UINT32 address)
{
//	bprintf (PRINT_NORMAL, _T("%5.5x rb\n"), address);

	switch (address)
	{
		case 0x800000:
		case 0x800001:
			return (DrvInps[0] >> ((~address & 1) << 3));

		case 0x800018:
		case 0x800019:
			return (DrvInps[1] >> ((~address & 1) << 3));

		case 0x80001a:
			return DrvDips[0];

		case 0x80001c:
			return DrvDips[1];

		case 0x80001b:
		case 0x80001d:
			return 0xff;

		case 0x800176:
		case 0x800177:
			return (DrvInps[2] >> ((~address & 1) << 3));;

	}

	return 0;
}

UINT16 __fastcall drgnmst_read_word(UINT32 address)
{
//	bprintf (PRINT_NORMAL, _T("%5.5x rw\n"), address);

	switch (address)
	{
		case 0x800000:
			return DrvInps[0];

		case 0x800018:
			return DrvInps[1];

		case 0x80001a:
			return 0xff | (DrvDips[0] << 8);

		case 0x80001c:
			return 0xff | (DrvDips[1] << 8);

		case 0x800176:
			return DrvInps[2];
	}

	return 0;
}

static void set_oki_bank0(INT32 bank)
{
	INT32 nBank = 0x40000 * (bank & 3);

	memcpy (MSM6295ROM + 0x000000, DrvSndROM0 + nBank, 0x40000);
}

static void set_oki_bank1(INT32 bank)
{
	INT32 nBank = 0x40000 * (bank & 7);

	memcpy (MSM6295ROM + 0x100000, DrvSndROM1 + nBank, 0x40000);
}

static UINT8 drgnmst_snd_command_r()
{
	INT32 data = 0;

	switch (drgnmst_oki_control & 0x1f)
	{
		case 0x12:	data = MSM6295ReadStatus(1) & 0x0f; break;//(okim6295_status_1_r(machine, 0) & 0x0f); break;
		case 0x16:	data = MSM6295ReadStatus(0) & 0x0f; break;//(okim6295_status_0_r(machine, 0) & 0x0f); break;
		case 0x0b:
		case 0x0f:      data = drgnmst_snd_command; break;
		default:	break;
	}

	return data;
}

static void drgnmst_snd_control_w(INT32 data)
{
	INT32 oki_new_bank;
	drgnmst_oki_control = data;

	oki_new_bank = ((pic16c5x_port0 & 0xc) >> 2) | ((drgnmst_oki_control & 0x80) >> 5);
	if (oki_new_bank != drgnmst_oki0_bank) {
		drgnmst_oki0_bank = oki_new_bank;
		if (drgnmst_oki0_bank) oki_new_bank--;
			bprintf (PRINT_NORMAL, _T("bank0, %2.2x\n"), oki_new_bank);
		set_oki_bank0(oki_new_bank);
	}
	oki_new_bank = ((pic16c5x_port0 & 0x3) >> 0) | ((drgnmst_oki_control & 0x20) >> 3);
	if (oki_new_bank != drgnmst_oki1_bank) {
		drgnmst_oki1_bank = oki_new_bank;
			bprintf (PRINT_NORMAL, _T("bank1, %2.2x\n"), oki_new_bank);
		set_oki_bank1(oki_new_bank);
	}

	switch (drgnmst_oki_control & 0x1f)
	{
		case 0x15:
			bprintf (PRINT_NORMAL, _T("0, %2.2x\n"), drgnmst_oki_command);
			MSM6295Command(0, drgnmst_oki_command);
			break;

		case 0x11:
			bprintf (PRINT_NORMAL, _T("1, %2.2x\n"), drgnmst_oki_command);
			MSM6295Command(1, drgnmst_oki_command);
			break;
	}
}


UINT8 drgnmst_sound_readport(UINT16 port)
{
//	if (port != 0x10)bprintf (PRINT_NORMAL, _T("%4.4x rp\n"), port);

	switch (port)
	{
		case 0x00:
			return pic16c5x_port0;

		case 0x01:
			return drgnmst_snd_command_r();

		case 0x02:
			if (drgnmst_snd_flag) {
				drgnmst_snd_flag = 0;
				return 0x40;
			}
			return 0;


		case 0x10:
			return 0;
	}

	return 0;
}

void drgnmst_sound_writeport(UINT16 port, UINT8 data)
{
//	bprintf (PRINT_NORMAL, _T("%4.4x %2.2x wp\n"), port, data);

	switch (port & 0xff)
	{
		case 0x00:
			pic16c5x_port0 = data;
		return;

		case 0x01:
			drgnmst_oki_command = data;
		return;

		case 0x02:
			drgnmst_snd_control_w(data);
		return;
	}
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { 24, 8, 16, 0 };
	INT32 XOffs0[32] = { 0x800000,0x800001,0x800002,0x800003,0x800004,0x800005,0x800006,0x800007,
			   0x000000,0x000001,0x000002,0x000003,0x000004,0x000005,0x000006,0x000007,
	    		   0x800020,0x800021,0x800022,0x800023,0x800024,0x800025,0x800026,0x800027,
			   0x000020,0x000021,0x000022,0x000023,0x000024,0x000025,0x000026,0x000027 };
	INT32 YOffs0[16] = { 0*32,1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32,
	 		   8*32,9*32,10*32,11*32,12*32,13*32,14*32,15*32 };


	INT32 XOffs1[16] = { 0x2000000,0x2000001,0x2000002,0x2000003,0x2000004,0x2000005,0x2000006,0x2000007,
			   0x0000000,0x0000001,0x0000002,0x0000003,0x0000004,0x0000005,0x0000006,0x0000007 };

	INT32 YOffs1[32] = { 0*64, 1*64, 2*64, 3*64, 4*64, 5*64, 6*64, 7*64,
		 	   8*64, 9*64,10*64,11*64,12*64,13*64,14*64,15*64,
	 		  16*64,17*64,18*64,19*64,20*64,21*64,22*64,23*64,
	 		  24*64,25*64,26*64,27*64,28*64,29*64,30*64,31*64 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x800000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x800000);

	GfxDecode(0x10000, 4, 16, 16, Plane, XOffs1 + 0, YOffs0, 0x200, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x200000);

	GfxDecode(0x10000, 4,  8,  8, Plane, XOffs0 + 8, YOffs0, 0x100, tmp, DrvGfxROM1);
	GfxDecode(0x04000, 4, 16, 16, Plane, XOffs0 + 0, YOffs0, 0x200, tmp, DrvGfxROM2);
	GfxDecode(0x01000, 4, 32, 32, Plane, XOffs0 + 0, YOffs1, 0x800, tmp, DrvGfxROM3);

	BurnFree (tmp);

	return 0;
}

static INT32 DrvDoReset()
{
	DrvReset = 0;

	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	pic16c5xReset();

	set_oki_bank0(0);
	set_oki_bank1(0);

	MSM6295Reset(0);
	MSM6295Reset(1);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x100000;

	DrvPicROM	= Next; Next += 0x000400;

	MSM6295ROM	= Next; Next += 0x140000;
	DrvSndROM0	= Next; Next += 0x100000;
	DrvSndROM1	= Next; Next += 0x200000;

	DrvGfxROM0	= Next; Next += 0x1000000;
	DrvGfxROM1	= Next; Next += 0x400000;
	DrvGfxROM2	= Next; Next += 0x400000;
	DrvGfxROM3	= Next; Next += 0x400000;

	DrvPalette	= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x010000;
	DrvPalRAM	= Next; Next += 0x004000;
	DrvSprRAM	= Next; Next += 0x000800;

	DrvBgRAM	= Next; Next += 0x004000;
	DrvMidRAM	= Next; Next += 0x004000;
	DrvFgRAM	= Next; Next += 0x004000;
	DrvRowScroll	= Next; Next += 0x004000;

	DrvVidRegs	= Next; Next += 0x000020;
	priority_control= (UINT16*)Next; Next += 0x0002 /*1*/ * sizeof(UINT16);

	coin_lockout	= Next; Next += 0x000004; // 1

	Palette		= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);

	RamEnd		= Next;

	MemEnd		= Next;

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
		UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);
		if (tmp == NULL) {
			return 1;
		}

		if (BurnLoadRom(Drv68KROM + 1,	 0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0,	 1, 2)) return 1;

		if (BurnLoadRom(tmp + 0,	 2, 2)) return 1;
		if (BurnLoadRom(tmp + 1,	 3, 2)) return 1;
		memcpy (DrvGfxROM0 + 0x000000, tmp + 0x000000, 0x100000);
		memcpy (DrvGfxROM0 + 0x400000, tmp + 0x100000, 0x100000);
		memcpy (DrvGfxROM0 + 0x100000, tmp + 0x200000, 0x100000);
		memcpy (DrvGfxROM0 + 0x500000, tmp + 0x300000, 0x100000);
		if (BurnLoadRom(tmp + 0,	 4, 2)) return 1;
		if (BurnLoadRom(tmp + 1,	 5, 2)) return 1;
		memcpy (DrvGfxROM0 + 0x200000, tmp + 0x000000, 0x080000);
		memcpy (DrvGfxROM0 + 0x600000, tmp + 0x080000, 0x080000);

		if (BurnLoadRom(DrvGfxROM1 + 1,	 6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0,	 7, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0,		 9, 1)) return 1;
		if (BurnLoadRom(DrvSndROM1,	10, 1)) return 1;
		BurnFree (tmp);

		if (BurnLoadPicROM(DrvPicROM, 8, 0xb7b)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,		0x900000, 0x903fff, MAP_ROM);
	SekMapMemory(DrvMidRAM,		0x904000, 0x907fff, MAP_RAM);
	SekMapMemory(DrvBgRAM,		0x908000, 0x90bfff, MAP_RAM);
	SekMapMemory(DrvFgRAM,		0x90c000, 0x90ffff, MAP_RAM);
	SekMapMemory(DrvRowScroll,	0x920000, 0x923fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,		0x930000, 0x9307ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,		0xff0000, 0xffffff, MAP_RAM);
	SekSetWriteByteHandler(0,	drgnmst_write_byte);
	SekSetWriteWordHandler(0,	drgnmst_write_word);
	SekSetReadByteHandler(0,	drgnmst_read_byte);
	SekSetReadWordHandler(0,	drgnmst_read_word);
	SekClose();

	pic16c5xInit(0, 0x16C55, DrvPicROM);
	pic16c5xSetReadPortHandler(drgnmst_sound_readport);
	pic16c5xSetWritePortHandler(drgnmst_sound_writeport);

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295Init(1, 1000000 / 132, 0);
	MSM6295SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	pic16c5xExit();
	MSM6295Exit(0);
	MSM6295Exit(1);

	BurnFree (AllMem);

	return 0;
}

static void draw_background()
{
	UINT16 *ram = (UINT16*)DrvBgRAM;
	INT32 scrollx = *((UINT16*)(DrvVidRegs + 0x14)) - 18;
	INT32 scrolly = *((UINT16*)(DrvVidRegs + 0x16));

	scrollx = (scrollx & 0x7ff) + 64;
	scrolly = (scrolly & 0x7ff) + 16;

	for (INT32 offs = 0; offs < 0x1000; offs++)
	{
		INT32 sy = (offs & 0x3f) << 5;
		INT32 sx = (offs >> 6) << 5;

		INT32 ofst = ((sx>>2)+((sy>>5)&7)+((sy>>8)<<9))<<1;

		INT32 code = (ram[ofst] & 0x1fff) + 0x800;
		INT32 color = ram[ofst+1];
		INT32 flipy = color & 0x40;
		INT32 flipx = color & 0x20;

		color &= 0x3f;

		sy -= scrolly;
		if (sy < -31) sy += 0x800;
		sx -= scrollx;
		if (sx < -31) sx += 0x800;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		if (flipy) {
			if (flipx) {
				Render32x32Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x600, DrvGfxROM3);
			} else {
				Render32x32Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x600, DrvGfxROM3);
			}
		} else {
			if (flipx) {
				Render32x32Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x600, DrvGfxROM3);
			} else {
				Render32x32Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x600, DrvGfxROM3);
			}
		}
	}
}

static void draw_16x16_rowscroll(INT32 code, INT32 sx, INT32 sy, INT32 color, INT32 flipx, INT32 flipy, INT32 scrolly, INT32 scroll_x, UINT16 *rowscroll)
{
	UINT8 *src = DrvGfxROM2 + code * 0x100;
	INT32 flip = (flipx ? 0x0f : 0) | (flipy ? 0xf0 : 0);

	color = (color << 4) | 0x400;

	for (INT32 y = 0, off = 0; y < 16; y++)
	{
		INT32 yy = y + sy;

		INT32 px = sx - ((scroll_x + rowscroll[yy]) & 0x3ff);

		yy -= scrolly + 16;

		if (yy < -15) yy += 0x400;
		if (yy >= nScreenHeight) break;
	
		for (INT32 x = 0; x < 16; x++, off++)
		{
			INT32 xx = (x + px);

			if (xx < -15) xx += 0x400;

			if (xx < 0 || yy < 0 || xx >= nScreenWidth) continue;

			if (src[off ^ flip] == 0x0f) continue;

			pTransDraw[(yy * nScreenWidth) + xx] = src[off ^ flip] | color;
		}
	}
}

static void draw_midground()
{
	UINT16 *ram = (UINT16*)DrvMidRAM;
	INT32 scrollx = *((UINT16*)(DrvVidRegs + 0x10)) - 16;
	INT32 scrolly = *((UINT16*)(DrvVidRegs + 0x12)) & 0x3ff;
	UINT16 *rowscroll = (UINT16*)(DrvRowScroll + ((DrvVidRegs[0x13] & 0x30) << 8));

	for (INT32 offs = 0; offs < 0x1000; offs++)
	{
		INT32 sy = (offs & 0x3f) << 4;
		INT32 sx = (offs >> 6) << 4;

		INT32 ofst = (sx+((sy>>4)&0x0f)+(((sy>>4)&0xf0)<<6))<<1;

		INT32 code = (ram[ofst] & 0x7fff) - 0x2000;
		if (code == 0x800) continue;
		INT32 color = ram[ofst+1];
		INT32 flipy = color & 0x40;
		INT32 flipx = color & 0x20;

		color &= 0x1f;

		draw_16x16_rowscroll(code, sx - 64, sy, color, flipx, flipy, scrolly, scrollx, rowscroll);
	}
}

static void draw_foreground()
{
	UINT16 *ram = (UINT16*)DrvFgRAM;
	INT32 scrollx = *((UINT16*)(DrvVidRegs + 0x0c)) - 18;
	INT32 scrolly = *((UINT16*)(DrvVidRegs + 0x0e));

	scrollx = (scrollx & 0x1ff) + 64;
	scrolly = (scrolly & 0x1ff) + 16;

	for (INT32 offs = 0; offs < 64 * 64; offs++)
	{
		INT32 sy = (offs & 0x3f) << 3;
		INT32 sx = (offs >> 6) << 3;

		INT32 ofst = ((sx<<2)+((sy>>3)&0x1f)+((sy&0x300)<<3))<<1;

		INT32 code = ram[ofst] & 0x0fff;
		if (code == 0x0020) continue;

		INT32 color = ram[ofst+1];
		INT32 flipy = color & 0x40;
		INT32 flipx = color & 0x20;

		color &= 0x1f;

		sy -= scrolly;
		if (sy < -7) sy += 0x200;
		sx -= scrollx;
		if (sx < -7) sx += 0x200;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x200, DrvGfxROM1);
			} else {
				Render8x8Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x200, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x200, DrvGfxROM1);
			} else {
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 15, 0x200, DrvGfxROM1);
			}
		}
	}
}

static void draw_sprites()
{
	UINT16 *spriteram16 = (UINT16*)DrvSprRAM;
	UINT16 *source = spriteram16;
	UINT16 *finish = source + 0x800/2;

	while (source < finish)
	{
		INT32 incx,incy;

		INT32 number = source[2];
		INT32 xpos   = source[0]-64;
		INT32 ypos   = source[1]-16;
		INT32 flipx  = source[3] & 0x0020;
		INT32 flipy  = source[3] & 0x0040;
		INT32 wide  = (source[3] & 0x0f00)>>8;
		INT32 high  = (source[3] & 0xf000)>>12;
		INT32 colr  = (source[3] & 0x001f);

		if ((source[3] & 0xff00) == 0xff00) break;

		if (!flipx) { incx = 16;} else { incx = -16; xpos += 16*wide; }
		if (!flipy) { incy = 16;} else { incy = -16; ypos += 16*high; }

		for (INT32 y=0;y<=high;y++) {
			for (INT32 x=0;x<=wide;x++) {
				INT32 realx, realy, realnumber;

				realx = (xpos+incx*x);
				realy = (ypos+incy*y);
				realnumber = number+x+y*16;

				if (realy < -15 || realx < -15 || realy >= nScreenHeight || realx >= nScreenWidth) continue;

				if (flipy) {
					if (flipx) {
						Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, realnumber, realx, realy, colr, 4, 15, 0, DrvGfxROM0);
					} else {
						Render16x16Tile_Mask_FlipY_Clip(pTransDraw, realnumber, realx, realy, colr, 4, 15, 0, DrvGfxROM0);
					}
				} else {
					if (flipx) {
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, realnumber, realx, realy, colr, 4, 15, 0, DrvGfxROM0);
					} else {
						Render16x16Tile_Mask_Clip(pTransDraw, realnumber, realx, realy, colr, 4, 15, 0, DrvGfxROM0);
					}
				}
			}
		}

		source+=4;
	}
}


static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x2000; i++) {
			INT32 rgb = Palette[i];
			DrvPalette[i] = BurnHighCol(rgb >> 16, rgb >> 8, rgb, 0);
		}
	}

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x000f;

	}

	switch (*priority_control)
	{
		case 0x2451:
		case 0x2d9a:
		case 0x2440:
		case 0x245a:
			draw_foreground();
			draw_midground();
			draw_background();
		break;

		case 0x23c0:
			draw_background();
			draw_foreground();
			draw_midground();
		break;

		case 0x38da:
		case 0x215a:
		case 0x2140:
			draw_foreground();
			draw_background();
			draw_midground();
		break;

		case 0x2d80:
			draw_midground();
			draw_background();
			draw_foreground();
		break;
	}

	draw_sprites();

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
		memset (DrvInps, 0xff, 3 * sizeof(short));

		for (INT32 i = 0; i < 16; i++) {
			DrvInps[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInps[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInps[2] ^= (DrvJoy3[i] & 1) << i;
		}

		DrvInps[1] |= *coin_lockout;
	}

	INT32 nInterleave = 10;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 12000000 / 60, 4000000 / 60 };
//	INT32 nCyclesDone[2] = { 0, 0 };
	nCyclesDone[0] = 0;
	nCyclesDone[1] = 0;
	INT32 nCycleSegment;

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCycleSegment = (nCyclesTotal[0] - nCyclesDone[0]) / (nInterleave - i);
		nCyclesDone[0] += SekRun(nCycleSegment);

		nCycleSegment = (nCyclesTotal[1] - nCyclesDone[1]) / (nInterleave - i);
		nCyclesDone[1] += pic16c5xRun(nCycleSegment);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		//	memset (pSoundBuf, 0, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			MSM6295Render(1, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
	SekClose();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
		//	memset (pSoundBuf, 0, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			MSM6295Render(1, pSoundBuf, nSegmentLength);
		}
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029697;
	}

	if (nAction & ACB_MEMORY_RAM) {	
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		pic16c5xScan(nAction);

		MSM6295Scan(0, nAction);
		MSM6295Scan(1, nAction);

		SCAN_VAR(pic16c5x_port0);
		SCAN_VAR(drgnmst_oki_control);
		SCAN_VAR(drgnmst_snd_command);
		SCAN_VAR(drgnmst_snd_flag);
		SCAN_VAR(drgnmst_oki0_bank);
		SCAN_VAR(drgnmst_oki1_bank);
		SCAN_VAR(drgnmst_oki_command);

		set_oki_bank0(drgnmst_oki0_bank);
		set_oki_bank1(drgnmst_oki1_bank);
	}

	return 0;
}

// Dragon Master

static struct BurnRomInfo drgnmstRomDesc[] = {
	{ "dm1000e",		0x080000, 0x29467dac, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "dm1000o",		0x080000, 0xba48e9cf, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "dm1003",		0x200000, 0x0ca10e81, 2 | BRF_GRA },           //  2 Sprites
	{ "dm1005",		0x200000, 0x4c2b1db5, 2 | BRF_GRA },           //  3
	{ "dm1004",		0x080000, 0x1a9ac249, 2 | BRF_GRA },           //  4
	{ "dm1006",		0x080000, 0xc46da6fc, 2 | BRF_GRA },           //  5

	{ "dm1007",		0x100000, 0xd5ad81c4, 3 | BRF_GRA },           //  6 Background Tiles
	{ "dm1008",		0x100000, 0xb8572be3, 3 | BRF_GRA },           //  7

	{ "pic16c55.hex",	0x000b7b, 0xf17011e7, 4 | BRF_PRG | BRF_ESS }, //  8 pic16c55 Hex data

	{ "dm1001",		0x100000, 0x63566f7f, 5 | BRF_SND },           //  9 OKI6295 #0 Samples

	{ "dm1002",		0x200000, 0x0f1a874e, 6 | BRF_SND },           // 10 OKI6295 #1 Samples
};

STD_ROM_PICK(drgnmst)
STD_ROM_FN(drgnmst)

struct BurnDriver BurnDrvDrgnmst = {
	"drgnmst", NULL, NULL, NULL, "1994",
	"Dragon Master\0", NULL, "Unico", "Misc",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, drgnmstRomInfo, drgnmstRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc,	0x2000, 384, 224, 4, 3
};
