// FB Alpha Xain'd Sleena driver module
// Based on MAME driver by Carlos A. Lozano, Rob Rosenbrock, Phil Stroffolino, and Bryan McPhail

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "m6805_intf.h"
#include "burn_ym2203.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvSubROM;
static UINT8 *DrvSoundROM;
static UINT8 *DrvMcuROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvShareRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvBgRAM0;
static UINT8 *DrvBgRAM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSoundRAM;
static UINT8 *DrvMcuRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 nBankAData;
static UINT8 nBankBData;
static UINT16 scrollxp1;
static UINT16 scrollyp1;
static UINT16 scrollxp0;
static UINT16 scrollyp0;
static INT32 vblank;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 xain_pri;

static UINT8 port_a_out;
static UINT8 port_b_out;
static UINT8 port_c_out;
static UINT8 port_a_in;
static UINT8 port_b_in;
static UINT8 port_c_in;
static UINT8 ddr_a;
static UINT8 ddr_b;
static UINT8 ddr_c;
static UINT8 from_mcu;
static UINT8 from_main;
static UINT8 mcu_ready;
static UINT8 mcu_accept;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 is_bootleg;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 coin"	},
	{"P3 Coin",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},

	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x12, 0xff, 0xff, 0x3f, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x03, 0x00, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x10, 0x00, "Off"			},
	{0x12, 0x01, 0x10, 0x10, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x12, 0x01, 0x20, 0x00, "No"			},
	{0x12, 0x01, 0x20, 0x20, "Yes"			},

//	{0   , 0xfe, 0   ,    2, "Cabinet"		},
//	{0x12, 0x01, 0x40, 0x00, "Upright"		},
//	{0x12, 0x01, 0x40, 0x40, "Cocktail"		},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x12, 0x01, 0x80, 0x00, "Off"			},
//	{0x12, 0x01, 0x80, 0x80, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x03, 0x03, "Easy"			},
	{0x13, 0x01, 0x03, 0x02, "Normal"		},
	{0x13, 0x01, 0x03, 0x01, "Hard"			},
	{0x13, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Game Time"		},
	{0x13, 0x01, 0x0c, 0x0c, "Slow"			},
	{0x13, 0x01, 0x0c, 0x08, "Normal"		},
	{0x13, 0x01, 0x0c, 0x04, "Fast"			},
	{0x13, 0x01, 0x0c, 0x00, "Very Fast"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x30, 0x30, "20k 70k and every 70k"},
	{0x13, 0x01, 0x30, 0x20, "30k 80k and every 80k"},
	{0x13, 0x01, 0x30, 0x10, "20k and 80k"		},
	{0x13, 0x01, 0x30, 0x00, "30k and 80k"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0xc0, 0xc0, "3"			},
	{0x13, 0x01, 0xc0, 0x80, "4"			},
	{0x13, 0x01, 0xc0, 0x40, "6"			},
	{0x13, 0x01, 0xc0, 0x00, "Infinite (Cheat)"	},
};

STDDIPINFO(Drv)

static inline void palette_update(INT32 offset)
{
	INT32 b = DrvPalRAM[offset + 0x200] & 0x0f;
	INT32 g = DrvPalRAM[offset] >> 4;
	INT32 r = DrvPalRAM[offset] & 0x0f;

	DrvPalette[offset] = BurnHighCol(r+(r*16),g+(g*16),b+(b*16),0);
}

static void bankswitchA(INT32 data)
{
	nBankAData = data;
	M6809MapMemory(DrvMainROM + ((data & 0x08) ? 0x14000 : 0x10000), 0x4000, 0x7fff, MAP_ROM);
}

static void xain_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x3c00) {
		DrvPalRAM[address & 0x3ff] = data;
		palette_update(address & 0x1ff);
		return;
	}

	switch (address)
	{
		case 0x3a00:
		case 0x3a01:
			scrollxp1 = ((data << ((address & 1) * 8)) | (scrollxp1 & (0xff00 >> ((address & 1) * 8)))) & 0x1ff;
		return;

		case 0x3a02:
		case 0x3a03:
			scrollyp1 = ((data << ((address & 1) * 8)) | (scrollyp1 & (0xff00 >> ((address & 1) * 8)))) & 0x1ff;
		return;

		case 0x3a04:
		case 0x3a05:
			scrollxp0 = ((data << ((address & 1) * 8)) | (scrollxp0 & (0xff00 >> ((address & 1) * 8)))) & 0x1ff;
		return;

		case 0x3a06:
		case 0x3a07:
			scrollyp0 = ((data << ((address & 1) * 8)) | (scrollyp0 & (0xff00 >> ((address & 1) * 8)))) & 0x1ff;
		return;

		case 0x3a08:
		{
			INT32 cycles = M6809TotalCycles();
			M6809Close();
			M6809Open(2);
			BurnTimerUpdate(cycles);
			soundlatch = data;
		//	M6809SetIRQLine(0, CPU_IRQSTATUS_AUTO);
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK); // hold line until soundlatch is read to ensure commands are read
			M6809Close();
			M6809Open(0);
		}
		return;

		case 0x3a09:
			M6809SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0x3a0a:
			M6809SetIRQLine(0x01, CPU_IRQSTATUS_NONE);
		return;

		case 0x3a0b:
			M6809SetIRQLine(0x00, CPU_IRQSTATUS_NONE);
		return;

		case 0x3a0c:
		{
			M6809Close();
			M6809Open(1);
			M6809SetIRQLine(0x00, CPU_IRQSTATUS_ACK); 
			M6809Close();
			M6809Open(0);
		}
		return;

		case 0x3a0d:
			flipscreen = data & 0x01;
		return;

		case 0x3a0e:
		{
			from_main = data;
			mcu_accept = 0;

			if (is_bootleg == 0)
			{
				m6805Run((M6809TotalCycles() * 2) - m6805TotalCycles());
				m68705SetIrqLine(0, 1 /*ASSERT_LINE*/);
			}
		}
		return;

		case 0x3a0f:
			xain_pri = data & 0x07;
			bankswitchA(data);
		return;
	}
}

static UINT8 xain_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x3a00:
			return DrvInputs[0];

		case 0x3a01:
			return DrvInputs[1];

		case 0x3a02:
			return DrvDips[0];

		case 0x3a03:
			return DrvDips[1];

		case 0x3a04:
			m6805Run((M6809TotalCycles() * 2) - m6805TotalCycles());
			mcu_ready = 1;
			return from_mcu;

		case 0x3a05:
		{
			UINT8 ret = DrvInputs[2] & ~0x38;

			ret |= vblank ? 0x20 : 0;

			if (is_bootleg)
			{
				ret |= 0x18;
			}
			else
			{
				m6805Run((M6809TotalCycles() * 2) - m6805TotalCycles());
				if (mcu_ready == 1) ret |= 0x08;
				if (mcu_accept == 1) ret |= 0x10;
			}

			return ret;
		}

		case 0x3a06:
		{
			if (is_bootleg == 0)
			{
				m6805Run((M6809TotalCycles() * 2) - m6805TotalCycles());

				mcu_ready = 1;
				mcu_accept = 1;
				m68705SetIrqLine(0, 0 /*CLEAR_LINE*/);
			}
			return 0xff;
		}
	}

	return 0;
}

static void bankswitchB(INT32 data)
{
	nBankBData = data;
	M6809MapMemory(DrvSubROM + ((data & 0x01) ? 0x14000 : 0x10000), 0x4000, 0x7fff, MAP_ROM);
}

static void xain_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
		{
			M6809Close();
			M6809Open(0);
			M6809SetIRQLine(0x00, CPU_IRQSTATUS_ACK); // maincpu irq
			M6809Close();
			M6809Open(1);
		}
		return;

		case 0x2800:
			M6809SetIRQLine(0x00, CPU_IRQSTATUS_NONE);
		return;

		case 0x3000:
			bankswitchB(data);
		return;
	}
}

static UINT8 xain_sub_read(UINT16 /*address*/)
{
	return 0;
}

static void xain_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2800:
		case 0x2801:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0x3000:
		case 0x3001:
			BurnYM2203Write(1, address & 1, data);
		return;
	}
}

static UINT8 xain_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x1000:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static void xain_68705_port_b_write(UINT8 data) // ripped directly from MAME
{
	if ((ddr_b & 0x02) && (~data & 0x02))
	{
		port_a_in = from_main;
	}
	else if ((ddr_b & 0x02) && (~port_b_out & 0x02) && (data & 0x02))
	{
		mcu_accept = 1;
		m68705SetIrqLine(0, 0 /*CLEAR_LINE*/);
	}

	if ((ddr_b & 0x04) && (~port_b_out & 0x04) && (data & 0x04))
	{
		mcu_ready = 0;
		from_mcu = port_a_out;
	}

	port_b_out = data;
}

static void xain_68705_write_ports(UINT16 address, UINT8 data)
{
	switch (address & 0x7ff)
	{
		case 0:
			port_a_out = data;
		return;

		case 1:
			xain_68705_port_b_write(data);
		return;

		case 2:
			port_c_out = data;
		return;

		case 4:
			ddr_a = data;
		return;

		case 5:
			ddr_b = data;
		return;

		case 6:
			ddr_c = data;
		return;
	}
}

static UINT8 xain_68705_read_ports(UINT16 address)
{
	switch (address & 0x7ff)
	{
		case 0:
			return (port_a_out & ddr_a) | (port_a_in & ~ddr_a);

		case 1:
			return (port_b_out & ddr_b) | (port_b_in & ~ddr_b);

		case 2:
		{
			port_c_in = 0;
		
			if (!mcu_accept) port_c_in |= 0x01;
			if (mcu_ready)   port_c_in |= 0x02;
		
			return (port_c_out & ddr_c) | (port_c_in & ~ddr_c);
		}
	}

	return 0;
}

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	M6809SetIRQLine(0x01, ((nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE));
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(M6809TotalCycles() * nSoundRate / 1500000);
}

inline static double DrvGetTime()
{
	return (double)M6809TotalCycles() / 1500000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6809Open(0);
	M6809Reset();
	bankswitchA(0);
	M6809Close();

	M6809Open(1);
	M6809Reset();
	bankswitchB(0);
	M6809Close();

	M6809Open(2);
	M6809Reset();
	BurnYM2203Reset();
	M6809Close();

	m6805Open(0);
	m68705Reset();
	m6805Close();

	scrollxp1 = 0;
	scrollyp1 = 0;
	scrollxp0 = 0;
	scrollyp0 = 0;
	vblank = 0;
	soundlatch = 0;
	flipscreen = 0;
	xain_pri = 0;

	port_a_out = 0;
	port_b_out = 0;
	port_c_out = 0;
	port_a_in = 0;
	port_b_in = 0;
	port_c_in = 0;
	ddr_a = 0;
	ddr_b = 0;
	ddr_c = 0;
	from_mcu = 0;
	from_main = 0;
	mcu_ready = 0;
	mcu_accept = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x018000;
	DrvSubROM		= Next; Next += 0x018000;
	DrvSoundROM		= Next; Next += 0x010000;
	DrvMcuROM		= Next; Next += 0x000800;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x080000;
	DrvGfxROM2		= Next; Next += 0x080000;
	DrvGfxROM3		= Next; Next += 0x080000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvShareRAM		= Next; Next += 0x002000;
	DrvCharRAM		= Next; Next += 0x000800;
	DrvBgRAM0		= Next; Next += 0x000800;
	DrvBgRAM1		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000200;
	DrvPalRAM		= Next; Next += 0x000400;
	DrvSoundRAM		= Next; Next += 0x000800;
	DrvMcuRAM		= Next; Next += 0x000080;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0, 2, 4, 6 };
	INT32 Plane1[4]  = { RGN_FRAC(0x40000, 1,2)+0, RGN_FRAC(0x40000, 1,2)+4, 0, 4 };
	INT32 XOffs0[8]  = { STEP2(0*8+1,-1), STEP2(8*8+1,-1), STEP2(16*8+1,-1), STEP2(24*8+1,-1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };
	INT32 XOffs1[16] = { STEP4(0*8+3,-1), STEP4(16*8+3,-1), STEP4(32*8+3,-1), STEP4(48*8+3,-1) };
	INT32 YOffs1[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x08000);

	GfxDecode(0x0400, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM2);

	memcpy (tmp, DrvGfxROM3, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM3);

	BurnFree(tmp);

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
		if (BurnLoadRom(DrvMainROM  + 0x08000,  0, 1)) return 1;
		if (BurnLoadRom(DrvMainROM  + 0x10000,  1, 1)) return 1;

		if (BurnLoadRom(DrvSubROM   + 0x08000,  2, 1)) return 1;
		if (BurnLoadRom(DrvSubROM   + 0x10000,  3, 1)) return 1;

		if (BurnLoadRom(DrvSoundROM + 0x08000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x008000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x010000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x018000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x028000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x030000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x038000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x008000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x010000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x020000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x028000, 18, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x030000, 19, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x000000, 20, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x008000, 21, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x010000, 22, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x018000, 23, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x020000, 24, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x028000, 25, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x030000, 26, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x038000, 27, 1)) return 1;

		is_bootleg = (BurnLoadRom(DrvMcuROM, 29, 1)) ? 1 : 0;

		DrvGfxDecode();
	}

	M6809Init(3);
	M6809Open(0);
	M6809MapMemory(DrvShareRAM,		0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvCharRAM,		0x2000, 0x27ff, MAP_RAM);
	M6809MapMemory(DrvBgRAM1,		0x2800, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvBgRAM0,		0x3000, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x3800, 0x39ff, MAP_RAM);
	M6809MapMemory(DrvPalRAM,		0x3c00, 0x3fff, MAP_ROM);
	M6809MapMemory(DrvMainROM + 0x08000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(xain_main_write);
	M6809SetReadHandler(xain_main_read);
	M6809Close();

	M6809Open(1);
	M6809MapMemory(DrvShareRAM,		0x0000, 0x1fff, MAP_RAM);
	M6809MapMemory(DrvSubROM + 0x08000,	0x8000, 0xffff, MAP_ROM); 
	M6809SetWriteHandler(xain_sub_write);
	M6809SetReadHandler(xain_sub_read);
	M6809Close();

	M6809Open(2);
	M6809MapMemory(DrvSoundRAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvSoundROM + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(xain_sound_write);
	M6809SetReadHandler(xain_sound_read);
	M6809Close();

	m6805Init(1, 0x800);
	m6805Open(0);
	m6805MapMemory(DrvMcuRAM,		0x0010, 0x007f, MAP_RAM);
	m6805MapMemory(DrvMcuROM + 0x80,	0x0080, 0x07ff, MAP_ROM);
	m6805SetWriteHandler(xain_68705_write_ports);
	m6805SetReadHandler(xain_68705_read_ports);
	m6805Close();

	BurnYM2203Init(2,  6000000/2, &DrvYM2203IRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachM6809(1500000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.40, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.40, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	m6805Exit();

	BurnYM2203Exit();

	BurnFree (AllMem);

	return 0;
}

static inline INT32 scanline_to_vcount(INT32 scanline) // ripped directly from MAME
{
	INT32 vcount = scanline + 8;

	if (vcount < 0x100)
		return vcount;
	else
		return (vcount - 0x18) | 0x100;
}
				
static void xain_scanline(INT32 scanline) // ripped directly from MAME
{
	INT32 screen_height = 240;
	INT32 vcount_old = scanline_to_vcount((scanline == 0) ? screen_height - 1 : scanline - 1);
	INT32 vcount = scanline_to_vcount(scanline);

	if (!(vcount_old & 8) && (vcount & 8))
	{
		M6809SetIRQLine(0x01, CPU_IRQSTATUS_ACK);
	}

	if (vcount == 0xf8)
	{
		M6809SetIRQLine(0x20, CPU_IRQSTATUS_ACK);
	}

	vblank = (vcount >= (248 - 1)) ? 1 : 0; // -1 is a hack
}

static void draw_layer_8x8(INT32 transparent)
{
	if ((nBurnLayer & 0x01) == 0) return;

	for (INT32 offs = 32; offs < 32 * 31; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 attr = DrvCharRAM[offs + 0x400];
		INT32 code = DrvCharRAM[offs + 0x000] + ((attr & 0x03) * 256);
		INT32 color = (attr & 0xe0) >> 5;

		if (transparent) {
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy - 8, color, 4, 0, 0, DrvGfxROM0);
		} else {
			Render8x8Tile_Clip(pTransDraw, code, sx, sy - 8, color, 4, 0, DrvGfxROM0);
		}
	}
}

static void draw_layer_16x16(UINT8 *ram, UINT8 *rom, INT32 color_offset, INT32 scrollx, INT32 scrolly, INT32 transparent)
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f);
		INT32 sy = (offs / 0x20);

		INT32 ofst = (sx & 0x0f) + (((sy & 0x0f) + (sx & 0x10)) << 4) + ((sy & 0x10) << 5);

		sx = (sx * 16) - scrollx;
		if (sx < -15) sx += 0x200;
		sy = ((sy * 16) - scrolly) - 8;
		if (sy < -15) sy += 0x200;

		if (sx >= nScreenWidth || sy >= nScreenHeight || sx < -15 || sy < -15) continue;

		INT32 attr = ram[ofst + 0x400];
		INT32 code = ram[ofst + 0x000] + ((attr & 0x07) * 256);
		INT32 flipx  = attr & 0x80;
		INT32 color  = (attr & 0x70) >> 4;

		if (transparent) {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, color_offset, rom);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 4, 0, color_offset, rom);
			}
		} else {
			if (flipx) {
				Render16x16Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, color_offset, rom);
			} else {
				Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, color_offset, rom);
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x180; offs += 4)
	{
		INT32 attr = DrvSprRAM[offs+1];
		INT32 code = DrvSprRAM[offs+2] | ((attr & 7) << 8);
		INT32 color = (attr & 0x38) >> 3;

		INT32 sx = 238 - DrvSprRAM[offs+3];
		if (sx <= -7) sx += 256;
		INT32 sy = 240 - DrvSprRAM[offs];
		if (sy <= -7) sy += 256;
		INT32 flipx = attr & 0x40;
		INT32 flipy = 0;

		if (attr & 0x80)
		{
			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, (flipy ? sy+16:sy-16)-8, color, 4, 0, 0x80, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, (flipy ? sy+16:sy-16)-8, color, 4, 0, 0x80, DrvGfxROM3);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, (flipy ? sy+16:sy-16)-8, color, 4, 0, 0x80, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, (flipy ? sy+16:sy-16)-8, color, 4, 0, 0x80, DrvGfxROM3);
				}
			}

			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code+1, sx, sy-8, color, 4, 0, 0x80, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code+1, sx, sy-8, color, 4, 0, 0x80, DrvGfxROM3);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code+1, sx, sy-8, color, 4, 0, 0x80, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code+1, sx, sy-8, color, 4, 0, 0x80, DrvGfxROM3);
				}
			}
		}
		else
		{
			if (flipy) {
				if (flipx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy - 8, color, 4, 0, 0x80, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy - 8, color, 4, 0, 0x80, DrvGfxROM3);
				}
			} else {
				if (flipx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy - 8, color, 4, 0, 0x80, DrvGfxROM3);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy - 8, color, 4, 0, 0x80, DrvGfxROM3);
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x200; i++) {
			palette_update(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	switch (xain_pri & 0x07)
	{
		case 0:
			if (nBurnLayer & 1) draw_layer_16x16(DrvBgRAM0, DrvGfxROM2, 0x180, scrollxp0, scrollyp0, 0);
			if (nBurnLayer & 2) draw_layer_16x16(DrvBgRAM1, DrvGfxROM1, 0x100, scrollxp1, scrollyp1, 1);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 4) draw_layer_8x8(1);
		break;

		case 1:
			if (nBurnLayer & 2) draw_layer_16x16(DrvBgRAM1, DrvGfxROM1, 0x100, scrollxp1, scrollyp1, 0);
			if (nBurnLayer & 1) draw_layer_16x16(DrvBgRAM0, DrvGfxROM2, 0x180, scrollxp0, scrollyp0, 1);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 4) draw_layer_8x8(1);
		break;

		case 2:
			if (nBurnLayer & 4) draw_layer_8x8(0);
			if (nBurnLayer & 1) draw_layer_16x16(DrvBgRAM0, DrvGfxROM2, 0x180, scrollxp0, scrollyp0, 1);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 2) draw_layer_16x16(DrvBgRAM1, DrvGfxROM1, 0x100, scrollxp1, scrollyp1, 1);
		break;

		case 3:
			if (nBurnLayer & 4) draw_layer_8x8(0);
			if (nBurnLayer & 2) draw_layer_16x16(DrvBgRAM1, DrvGfxROM1, 0x100, scrollxp1, scrollyp1, 1);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 1) draw_layer_16x16(DrvBgRAM0, DrvGfxROM2, 0x180, scrollxp0, scrollyp0, 1);
		break;

		case 4:
			if (nBurnLayer & 1) draw_layer_16x16(DrvBgRAM0, DrvGfxROM2, 0x180, scrollxp0, scrollyp0, 0);
			if (nBurnLayer & 4) draw_layer_8x8(1);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 2) draw_layer_16x16(DrvBgRAM1, DrvGfxROM1, 0x100, scrollxp1, scrollyp1, 1);
		break;

		case 5:
			if (nBurnLayer & 2) draw_layer_16x16(DrvBgRAM1, DrvGfxROM1, 0x100, scrollxp1, scrollyp1, 0);
			if (nBurnLayer & 4) draw_layer_8x8(1);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 1) draw_layer_16x16(DrvBgRAM0, DrvGfxROM2, 0x180, scrollxp0, scrollyp0, 1);
		break;

		case 6:
			if (nBurnLayer & 1) draw_layer_16x16(DrvBgRAM0, DrvGfxROM2, 0x180, scrollxp0, scrollyp0, 0);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 2) draw_layer_16x16(DrvBgRAM1, DrvGfxROM1, 0x100, scrollxp1, scrollyp1, 1);
			if (nBurnLayer & 4) draw_layer_8x8(1);
		break;

		case 7:
			if (nBurnLayer & 2) draw_layer_16x16(DrvBgRAM1, DrvGfxROM1, 0x100, scrollxp1, scrollyp1, 0);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 1) draw_layer_16x16(DrvBgRAM0, DrvGfxROM2, 0x180, scrollxp0, scrollyp0, 1);
			if (nBurnLayer & 4) draw_layer_8x8(1);
		break;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6809NewFrame();
	m6805NewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 272*8;
	INT32 nCyclesTotal[4] = { (INT32)((double)1500000 / 57.44), (INT32)((double)1500000 / 57.44), (INT32)((double)1500000 / 57.44), (INT32)((double)3000000 / 57.44) };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	m6805Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		if ((i%8) == 7)
			xain_scanline(i/8);
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);
		M6809Close();

		M6809Open(1);
		nCyclesDone[1] += M6809Run(nCyclesTotal[1] / nInterleave);
		M6809Close();

		m6805Run(nCyclesTotal[3] / nInterleave);

		M6809Open(2);
		BurnTimerUpdate((i + 1) * (nCyclesTotal[2] / nInterleave));
		M6809Close();
	}

	m6805Close();

	M6809Open(2);
	BurnTimerEndFrame(nCyclesTotal[2]);
	
	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	M6809Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029722;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);
		m6805Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(nBankAData);
		SCAN_VAR(nBankBData);
		SCAN_VAR(scrollxp1);
		SCAN_VAR(scrollyp1);
		SCAN_VAR(scrollxp0);
		SCAN_VAR(scrollyp0);
		SCAN_VAR(vblank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(xain_pri);
		SCAN_VAR(port_a_out);
		SCAN_VAR(port_b_out);
		SCAN_VAR(port_c_out);
		SCAN_VAR(port_a_in);
		SCAN_VAR(port_b_in);
		SCAN_VAR(port_c_in);
		SCAN_VAR(ddr_a);
		SCAN_VAR(ddr_b);
		SCAN_VAR(ddr_c);
		SCAN_VAR(from_mcu);
		SCAN_VAR(from_main);
		SCAN_VAR(mcu_ready);
		SCAN_VAR(mcu_accept);
	}

	if (nAction & ACB_WRITE)
	{
		M6809Open(0);
		bankswitchA(nBankAData);
		M6809Close();

		M6809Open(1);
		bankswitchB(nBankBData);
		M6809Close();

		DrvRecalc = 1;
	}

	return 0;
}


// Xain'd Sleena (World)

static struct BurnRomInfo xsleenaRomDesc[] = {
	{ "p9-08.ic66",		0x8000, 0x5179ae3f, 1 | BRF_PRG | BRF_ESS }, //  0 Master M6809 Code
	{ "pa-09.ic65",		0x8000, 0x10a7c800, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p1-0.ic29",		0x8000, 0xa1a860e2, 2 | BRF_PRG | BRF_ESS }, //  2 Slave M6809 code
	{ "p0-0.ic15",		0x8000, 0x948b9757, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p2-0.ic49",		0x8000, 0xa5318cb8, 3 | BRF_PRG | BRF_ESS }, //  4 Sound M6809 Code

	{ "pb-01.ic24",		0x8000, 0x83c00dd8, 4 | BRF_GRA },           //  5 Character

	{ "pk-0.ic136",		0x8000, 0x11eb4247, 5 | BRF_GRA },           //  6 Background Layer 1 tiles
	{ "pl-0.ic135",		0x8000, 0x422b536e, 5 | BRF_GRA },           //  7
	{ "pm-0.ic134",		0x8000, 0x828c1b0c, 5 | BRF_GRA },           //  8
	{ "pn-0.ic133",		0x8000, 0xd37939e0, 5 | BRF_GRA },           //  9
	{ "pc-0.ic114",		0x8000, 0x8f0aa1a7, 5 | BRF_GRA },           // 10
	{ "pd-0.ic113",		0x8000, 0x45681910, 5 | BRF_GRA },           // 11
	{ "pe-0.ic112",		0x8000, 0xa8eeabc8, 5 | BRF_GRA },           // 12
	{ "pf-0.ic111",		0x8000, 0xe59a2f27, 5 | BRF_GRA },           // 13

	{ "p5-0.ic44",		0x8000, 0x5c6c453c, 6 | BRF_GRA },           // 14 Background Layer 0 tiles
	{ "p4-0.ic45",		0x8000, 0x59d87a9a, 6 | BRF_GRA },           // 15
	{ "p3-0.ic46",		0x8000, 0x84884a2e, 6 | BRF_GRA },           // 16
	{ "p6-0.ic43",		0x8000, 0x8d637639, 6 | BRF_GRA },           // 17
	{ "p7-0.ic42",		0x8000, 0x71eec4e6, 6 | BRF_GRA },           // 18
	{ "p8-0.ic41",		0x8000, 0x7fc9704f, 6 | BRF_GRA },           // 19

	{ "po-0.ic131",		0x8000, 0x252976ae, 7 | BRF_GRA },           // 20 Sprite tiles
	{ "pp-0.ic130",		0x8000, 0xe6f1e8d5, 7 | BRF_GRA },           // 21
	{ "pq-0.ic129",		0x8000, 0x785381ed, 7 | BRF_GRA },           // 22
	{ "pr-0.ic128",		0x8000, 0x59754e3d, 7 | BRF_GRA },           // 23
	{ "pg-0.ic109",		0x8000, 0x4d977f33, 7 | BRF_GRA },           // 24
	{ "ph-0.ic108",		0x8000, 0x3f3b62a0, 7 | BRF_GRA },           // 25
	{ "pi-0.ic107",		0x8000, 0x76641ee3, 7 | BRF_GRA },           // 26
	{ "pj-0.ic106",		0x8000, 0x37671f36, 7 | BRF_GRA },           // 27

	{ "pt-0.ic59",		0x0100, 0xfed32888, 8 | BRF_GRA | BRF_OPT }, // 28 Priority Prom

	{ "pz-0.113",		0x0800, 0xa432a907, 9 | BRF_PRG | BRF_ESS }, // 29 M68705 Code
};

STD_ROM_PICK(xsleena)
STD_ROM_FN(xsleena)

struct BurnDriver BurnDrvXsleena = {
	"xsleena", NULL, NULL, NULL, "1986",
	"Xain'd Sleena (World)\0", NULL, "Technos Japan (Taito license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TECHNOS, GBF_HORSHOOT, 0,
	NULL, xsleenaRomInfo, xsleenaRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 240, 4, 3
};


// Xain'd Sleena (Japan)

static struct BurnRomInfo xsleenajRomDesc[] = {
	{ "p9-01.ic66",		0x8000, 0x370164be, 1 | BRF_PRG | BRF_ESS }, //  0 Master M6809 Code
	{ "pa-0.ic65",		0x8000, 0xd22bf859, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p1-0.ic29",		0x8000, 0xa1a860e2, 2 | BRF_PRG | BRF_ESS }, //  2 Slave M6809 code
	{ "p0-0.ic15",		0x8000, 0x948b9757, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p2-0.ic49",		0x8000, 0xa5318cb8, 3 | BRF_PRG | BRF_ESS }, //  4 Sound M6809 Code

	{ "pb-01.ic24",		0x8000, 0x83c00dd8, 4 | BRF_GRA },           //  5 Character

	{ "pk-0.ic136",		0x8000, 0x11eb4247, 5 | BRF_GRA },           //  6 Background Layer 1 tiles
	{ "pl-0.ic135",		0x8000, 0x422b536e, 5 | BRF_GRA },           //  7
	{ "pm-0.ic134",		0x8000, 0x828c1b0c, 5 | BRF_GRA },           //  8
	{ "pn-0.ic133",		0x8000, 0xd37939e0, 5 | BRF_GRA },           //  9
	{ "pc-0.ic114",		0x8000, 0x8f0aa1a7, 5 | BRF_GRA },           // 10
	{ "pd-0.ic113",		0x8000, 0x45681910, 5 | BRF_GRA },           // 11
	{ "pe-0.ic112",		0x8000, 0xa8eeabc8, 5 | BRF_GRA },           // 12
	{ "pf-0.ic111",		0x8000, 0xe59a2f27, 5 | BRF_GRA },           // 13

	{ "p5-0.ic44",		0x8000, 0x5c6c453c, 6 | BRF_GRA },           // 14 Background Layer 0 tiles
	{ "p4-0.ic45",		0x8000, 0x59d87a9a, 6 | BRF_GRA },           // 15
	{ "p3-0.ic46",		0x8000, 0x84884a2e, 6 | BRF_GRA },           // 16
	{ "p6-0.ic43",		0x8000, 0x8d637639, 6 | BRF_GRA },           // 17
	{ "p7-0.ic42",		0x8000, 0x71eec4e6, 6 | BRF_GRA },           // 18
	{ "p8-0.ic41",		0x8000, 0x7fc9704f, 6 | BRF_GRA },           // 19

	{ "po-0.ic131",		0x8000, 0x252976ae, 7 | BRF_GRA },           // 20 Sprite tiles
	{ "pp-0.ic130",		0x8000, 0xe6f1e8d5, 7 | BRF_GRA },           // 21
	{ "pq-0.ic129",		0x8000, 0x785381ed, 7 | BRF_GRA },           // 22
	{ "pr-0.ic128",		0x8000, 0x59754e3d, 7 | BRF_GRA },           // 23
	{ "pg-0.ic109",		0x8000, 0x4d977f33, 7 | BRF_GRA },           // 24
	{ "ph-0.ic108",		0x8000, 0x3f3b62a0, 7 | BRF_GRA },           // 25
	{ "pi-0.ic107",		0x8000, 0x76641ee3, 7 | BRF_GRA },           // 26
	{ "pj-0.ic106",		0x8000, 0x37671f36, 7 | BRF_GRA },           // 27

	{ "pt-0.ic59",		0x0100, 0xfed32888, 8 | BRF_GRA | BRF_OPT }, // 28 Priority Prom

	{ "pz-0.113",		0x0800, 0xa432a907, 9 | BRF_PRG | BRF_ESS }, // 29 M68705 Code
};

STD_ROM_PICK(xsleenaj)
STD_ROM_FN(xsleenaj)

struct BurnDriver BurnDrvXsleenaj = {
	"xsleenaj", "xsleena", NULL, NULL, "1986",
	"Xain'd Sleena (Japan)\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TECHNOS, GBF_HORSHOOT, 0,
	NULL, xsleenajRomInfo, xsleenajRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 240, 4, 3
};


// Solar-Warrior (US)

static struct BurnRomInfo solrwarrRomDesc[] = {
	{ "p9-02.ic66",		0x8000, 0x8ff372a8, 1 | BRF_PRG | BRF_ESS }, //  0 Master M6809 code
	{ "pa-03.ic65",		0x8000, 0x154f946f, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p1-02.ic29",		0x8000, 0xf5f235a3, 2 | BRF_PRG | BRF_ESS }, //  2 Slave M6809 code
	{ "p0-02.ic133",	0x8000, 0x51ae95ae, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p2-0.ic49",		0x8000, 0xa5318cb8, 3 | BRF_PRG | BRF_ESS }, //  4 Sound M6809 Code

	{ "pb-01.ic24",		0x8000, 0x83c00dd8, 4 | BRF_GRA },           //  5 Character

	{ "pk-0.ic136",		0x8000, 0x11eb4247, 5 | BRF_GRA },           //  6 Background Layer 1 tiles
	{ "pl-0.ic135",		0x8000, 0x422b536e, 5 | BRF_GRA },           //  7
	{ "pm-0.ic134",		0x8000, 0x828c1b0c, 5 | BRF_GRA },           //  8
	{ "pn-02.ic133",	0x8000, 0xd2ed6f94, 5 | BRF_GRA },           //  9
	{ "pc-0.ic114",		0x8000, 0x8f0aa1a7, 5 | BRF_GRA },           // 10
	{ "pd-0.ic113",		0x8000, 0x45681910, 5 | BRF_GRA },           // 11
	{ "pe-0.ic112",		0x8000, 0xa8eeabc8, 5 | BRF_GRA },           // 12
	{ "pf-02.ic111",	0x8000, 0x6e627a77, 5 | BRF_GRA },           // 13

	{ "p5-0.ic44",		0x8000, 0x5c6c453c, 6 | BRF_GRA },           // 14 Background Layer 0 tiles
	{ "p4-0.ic45",		0x8000, 0x59d87a9a, 6 | BRF_GRA },           // 15
	{ "p3-0.ic46",		0x8000, 0x84884a2e, 6 | BRF_GRA },           // 16
	{ "p6-0.ic43",		0x8000, 0x8d637639, 6 | BRF_GRA },           // 17
	{ "p7-0.ic42",		0x8000, 0x71eec4e6, 6 | BRF_GRA },           // 18
	{ "p8-0.ic41",		0x8000, 0x7fc9704f, 6 | BRF_GRA },           // 19

	{ "po-0.ic131",		0x8000, 0x252976ae, 7 | BRF_GRA },           // 20 Sprite tiles
	{ "pp-0.ic130",		0x8000, 0xe6f1e8d5, 7 | BRF_GRA },           // 21
	{ "pq-0.ic129",		0x8000, 0x785381ed, 7 | BRF_GRA },           // 22
	{ "pr-0.ic128",		0x8000, 0x59754e3d, 7 | BRF_GRA },           // 23
	{ "pg-0.ic109",		0x8000, 0x4d977f33, 7 | BRF_GRA },           // 24
	{ "ph-0.ic108",		0x8000, 0x3f3b62a0, 7 | BRF_GRA },           // 25
	{ "pi-0.ic107",		0x8000, 0x76641ee3, 7 | BRF_GRA },           // 26
	{ "pj-0.ic106",		0x8000, 0x37671f36, 7 | BRF_GRA },           // 27

	{ "pt-0.ic59",		0x0100, 0xfed32888, 8 | BRF_GRA | BRF_OPT }, // 28 Priority Prom

	{ "pz-0.113",		0x0800, 0xa432a907, 9 | BRF_PRG | BRF_ESS }, // 29 M68705 Code
};

STD_ROM_PICK(solrwarr)
STD_ROM_FN(solrwarr)

struct BurnDriver BurnDrvSolrwarr = {
	"solrwarr", "xsleena", NULL, NULL, "1986",
	"Solar-Warrior (US)\0", NULL, "Technos Japan (Taito / Memetron license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TECHNOS, GBF_HORSHOOT, 0,
	NULL, solrwarrRomInfo, solrwarrRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 240, 4, 3
};


// Xain'd Sleena (bootleg)

static struct BurnRomInfo xsleenabRomDesc[] = {
	{ "1.rom",			0x8000, 0x79f515a7, 1 | BRF_PRG | BRF_ESS }, //  0 Master M6809 Code
	{ "pa-0.ic65",		0x8000, 0xd22bf859, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p1-0.ic29",		0x8000, 0xa1a860e2, 2 | BRF_PRG | BRF_ESS }, //  2 Slave M6809 code
	{ "p0-0.ic15",		0x8000, 0x948b9757, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p2-0.ic49",		0x8000, 0xa5318cb8, 3 | BRF_PRG | BRF_ESS }, //  4 Sound M6809 Code

	{ "pb-01.ic24",		0x8000, 0x83c00dd8, 4 | BRF_GRA },           //  5 Character

	{ "pk-0.ic136",		0x8000, 0x11eb4247, 5 | BRF_GRA },           //  6 Background Layer 1 tiles
	{ "pl-0.ic135",		0x8000, 0x422b536e, 5 | BRF_GRA },           //  7
	{ "pm-0.ic134",		0x8000, 0x828c1b0c, 5 | BRF_GRA },           //  8
	{ "pn-0.ic133",		0x8000, 0xd37939e0, 5 | BRF_GRA },           //  9
	{ "pc-0.ic114",		0x8000, 0x8f0aa1a7, 5 | BRF_GRA },           // 10
	{ "pd-0.ic113",		0x8000, 0x45681910, 5 | BRF_GRA },           // 11
	{ "pe-0.ic112",		0x8000, 0xa8eeabc8, 5 | BRF_GRA },           // 12
	{ "pf-0.ic111",		0x8000, 0xe59a2f27, 5 | BRF_GRA },           // 13

	{ "p5-0.ic44",		0x8000, 0x5c6c453c, 6 | BRF_GRA },           // 14 Background Layer 0 tiles
	{ "p4-0.ic45",		0x8000, 0x59d87a9a, 6 | BRF_GRA },           // 15
	{ "p3-0.ic46",		0x8000, 0x84884a2e, 6 | BRF_GRA },           // 16
	{ "p6-0.ic43",		0x8000, 0x8d637639, 6 | BRF_GRA },           // 17
	{ "p7-0.ic42",		0x8000, 0x71eec4e6, 6 | BRF_GRA },           // 18
	{ "p8-0.ic41",		0x8000, 0x7fc9704f, 6 | BRF_GRA },           // 19

	{ "po-0.ic131",		0x8000, 0x252976ae, 7 | BRF_GRA },           // 20 Sprite tiles
	{ "pp-0.ic130",		0x8000, 0xe6f1e8d5, 7 | BRF_GRA },           // 21
	{ "pq-0.ic129",		0x8000, 0x785381ed, 7 | BRF_GRA },           // 22
	{ "pr-0.ic128",		0x8000, 0x59754e3d, 7 | BRF_GRA },           // 23
	{ "pg-0.ic109",		0x8000, 0x4d977f33, 7 | BRF_GRA },           // 24
	{ "ph-0.ic108",		0x8000, 0x3f3b62a0, 7 | BRF_GRA },           // 25
	{ "pi-0.ic107",		0x8000, 0x76641ee3, 7 | BRF_GRA },           // 26
	{ "pj-0.ic106",		0x8000, 0x37671f36, 7 | BRF_GRA },           // 27

	{ "pt-0.ic59",		0x0100, 0xfed32888, 8 | BRF_GRA | BRF_OPT }, // 28 Priority Prom
};

STD_ROM_PICK(xsleenab)
STD_ROM_FN(xsleenab)

struct BurnDriverD BurnDrvXsleenab = {
	"xsleenab", "xsleena", NULL, NULL, "1986",
	"Xain'd Sleena (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_TECHNOS, GBF_HORSHOOT, 0,
	NULL, xsleenabRomInfo, xsleenabRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 240, 4, 3
};


// Xain'd Sleena (bootleg, bugfixed)
// newer bootleg, fixes some of the issues with the other one
// f205v id 1016

static struct BurnRomInfo xsleenabaRomDesc[] = {
	{ "xs87b-10.7d",	0x8000, 0x3d5f9fb4, 1 | BRF_PRG | BRF_ESS }, //  0 Master M6809 Code
	{ "xs87b-11.7c",	0x8000, 0x81c80d54, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "p1-0.ic29",		0x8000, 0xa1a860e2, 2 | BRF_PRG | BRF_ESS }, //  2 Slave M6809 code
	{ "p0-0.ic15",		0x8000, 0x948b9757, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "p2-0.ic49",		0x8000, 0xa5318cb8, 3 | BRF_PRG | BRF_ESS }, //  4 Sound M6809 Code

	{ "pb-01.ic24",		0x8000, 0x83c00dd8, 4 | BRF_GRA },           //  5 Character

	{ "pk-0.ic136",		0x8000, 0x11eb4247, 5 | BRF_GRA },           //  6 Background Layer 1 tiles
	{ "pl-0.ic135",		0x8000, 0x422b536e, 5 | BRF_GRA },           //  7
	{ "pm-0.ic134",		0x8000, 0x828c1b0c, 5 | BRF_GRA },           //  8
	{ "pn-0.ic133",		0x8000, 0xd37939e0, 5 | BRF_GRA },           //  9
	{ "pc-0.ic114",		0x8000, 0x8f0aa1a7, 5 | BRF_GRA },           // 10
	{ "pd-0.ic113",		0x8000, 0x45681910, 5 | BRF_GRA },           // 11
	{ "pe-0.ic112",		0x8000, 0xa8eeabc8, 5 | BRF_GRA },           // 12
	{ "pf-0.ic111",		0x8000, 0xe59a2f27, 5 | BRF_GRA },           // 13

	{ "p5-0.ic44",		0x8000, 0x5c6c453c, 6 | BRF_GRA },           // 14 Background Layer 0 tiles
	{ "p4-0.ic45",		0x8000, 0x59d87a9a, 6 | BRF_GRA },           // 15
	{ "p3-0.ic46",		0x8000, 0x84884a2e, 6 | BRF_GRA },           // 16
	{ "p6-0.ic43",		0x8000, 0x8d637639, 6 | BRF_GRA },           // 17
	{ "p7-0.ic42",		0x8000, 0x71eec4e6, 6 | BRF_GRA },           // 18
	{ "p8-0.ic41",		0x8000, 0x7fc9704f, 6 | BRF_GRA },           // 19

	{ "po-0.ic131",		0x8000, 0x252976ae, 7 | BRF_GRA },           // 20 Sprite tiles
	{ "pp-0.ic130",		0x8000, 0xe6f1e8d5, 7 | BRF_GRA },           // 21
	{ "pq-0.ic129",		0x8000, 0x785381ed, 7 | BRF_GRA },           // 22
	{ "pr-0.ic128",		0x8000, 0x59754e3d, 7 | BRF_GRA },           // 23
	{ "pg-0.ic109",		0x8000, 0x4d977f33, 7 | BRF_GRA },           // 24
	{ "ph-0.ic108",		0x8000, 0x3f3b62a0, 7 | BRF_GRA },           // 25
	{ "pi-0.ic107",		0x8000, 0x76641ee3, 7 | BRF_GRA },           // 26
	{ "pj-0.ic106",		0x8000, 0x37671f36, 7 | BRF_GRA },           // 27

	{ "pt-0.ic59",		0x0100, 0xfed32888, 8 | BRF_GRA | BRF_OPT }, // 28 Priority Prom
};

STD_ROM_PICK(xsleenaba)
STD_ROM_FN(xsleenaba)

struct BurnDriverD BurnDrvXsleenaba = {
	"xsleenaba", "xsleena", NULL, NULL, "1987",
	"Xain'd Sleena (bootleg, bugfixed)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_TECHNOS, GBF_HORSHOOT, 0,
	NULL, xsleenabaRomInfo, xsleenabaRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 240, 4, 3
};
