// FB Alpha Star Wars driver module
// Based on MAME driver by Steve Baine and Frank Palazzolo
// based on mame 0.111/0.119

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "watchdog.h"
#include "slapstic.h"
#include "pokey.h"
#include "tms5220.h"
#include "avgdvg.h"
#include "vector.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvVectorROM;
static UINT8 *DrvM6809ROM0;
static UINT8 *DrvM6809ROM1;
static UINT8 *DrvMathPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvNVRAMBuf;
static UINT8 *DrvM6809RAM0;
static UINT8 *DrvM6809RAM1A;
static UINT8 *DrvM6809RAM1B;
static UINT8 *DrvMathRAM;
static UINT8 *DrvVectorRAM;
static UINT8 *DrvAVGPROM;
static UINT8 *DrvStrPROM;
static UINT8 *DrvMasPROM;
static UINT8 *DrvAmPROM;
static UINT8 *DrvColRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bankdata;
static UINT8 control_num;
static UINT8 port_A;
static UINT8 port_B;
static UINT8 port_A_ddr;
static UINT8 port_B_ddr;
static UINT8 sound_data;
static UINT8 main_data;
static UINT8 sound_irq_enable;
static UINT8 irq_flag;
static UINT8 avgletsgo;
static INT32 timer_counter;

static INT32 mathing;
static INT32 mathing_cyc;
static UINT16 quotient_shift;
static UINT16 dvd_shift;
static UINT16 divisor;
static UINT16 dividend;
static INT32 MPA;
static INT32 BIC;

static UINT8 *slapstic_source;
static UINT8 *slapstic_base;
static INT32 current_bank;
static INT32 is_esb = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo StarwarsInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p2 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 4"	},

	A("P1 Stick X",     BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Stick Y",     BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Diag. Step",		BIT_DIGITAL,	DrvJoy2 + 2,	"service2"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};
#undef A

STDINPUTINFO(Starwars)

#define DO 0xc

static struct BurnDIPInfo StarwarsDIPList[]=
{
	{DO+0, 0xff, 0xff, 0x96, NULL					},
	{DO+1, 0xff, 0xff, 0x02, NULL					},
	{DO+2, 0xff, 0xff, 0x10, NULL                   },

	{0   , 0xfe, 0   ,    4, "Starting Shields"		},
	{DO+0, 0x01, 0x03, 0x00, "6"					},
	{DO+0, 0x01, 0x03, 0x01, "7"					},
	{DO+0, 0x01, 0x03, 0x02, "8"					},
	{DO+0, 0x01, 0x03, 0x03, "9"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{DO+0, 0x01, 0x0c, 0x00, "Easy"					},
	{DO+0, 0x01, 0x0c, 0x04, "Moderate"				},
	{DO+0, 0x01, 0x0c, 0x08, "Hard"					},
	{DO+0, 0x01, 0x0c, 0x0c, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Bonus Shields"		},
	{DO+0, 0x01, 0x30, 0x00, "0"					},
	{DO+0, 0x01, 0x30, 0x10, "1"					},
	{DO+0, 0x01, 0x30, 0x20, "2"					},
	{DO+0, 0x01, 0x30, 0x30, "3"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{DO+0, 0x01, 0x40, 0x40, "Off"					},
	{DO+0, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{DO+0, 0x01, 0x80, 0x80, "Off"					},
	{DO+0, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{DO+1, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{DO+1, 0x01, 0x03, 0x02, "1 Coin  1 Credits"	},
	{DO+1, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{DO+1, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{DO+1, 0x01, 0x0c, 0x00, "*1"					},
	{DO+1, 0x01, 0x0c, 0x04, "*4"					},
	{DO+1, 0x01, 0x0c, 0x08, "*5"					},
	{DO+1, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Coin A"				},
	{DO+1, 0x01, 0x10, 0x00, "*1"					},
	{DO+1, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    6, "Bonus Coin Adder"		},
	{DO+1, 0x01, 0xe0, 0x20, "2 gives 1"			},
	{DO+1, 0x01, 0xe0, 0x60, "4 gives 2"			},
	{DO+1, 0x01, 0xe0, 0xa0, "3 gives 1"			},
	{DO+1, 0x01, 0xe0, 0x40, "4 gives 1"			},
	{DO+1, 0x01, 0xe0, 0x80, "5 gives 1"			},
	{DO+1, 0x01, 0xe0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{DO+2, 0x01, 0x10, 0x10, "Off"					},
	{DO+2, 0x01, 0x10, 0x00, "On"					},
};

STDDIPINFO(Starwars)

static struct BurnDIPInfo EsbDIPList[]=
{
	{DO+0, 0xff, 0xff, 0xb7, NULL					},
	{DO+1, 0xff, 0xff, 0x02, NULL					},
	{DO+2, 0xff, 0xff, 0x10, NULL                   },

	{0   , 0xfe, 0   ,    4, "Starting Shields"		},
	{DO+0, 0x01, 0x03, 0x00, "3"					},
	{DO+0, 0x01, 0x03, 0x01, "2"					},
	{DO+0, 0x01, 0x03, 0x02, "5"					},
	{DO+0, 0x01, 0x03, 0x03, "4"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{DO+0, 0x01, 0x0c, 0x00, "Easy"					},
	{DO+0, 0x01, 0x0c, 0x04, "Moderate"				},
	{DO+0, 0x01, 0x0c, 0x08, "Hard"					},
	{DO+0, 0x01, 0x0c, 0x0c, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Jedi-Letter Mode"		},
	{DO+0, 0x01, 0x30, 0x00, "Level Only"			},
	{DO+0, 0x01, 0x30, 0x10, "lEVEL"				},
	{DO+0, 0x01, 0x30, 0x20, "Increment Only"		},
	{DO+0, 0x01, 0x30, 0x30, "Increment"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{DO+0, 0x01, 0x40, 0x40, "Off"					},
	{DO+0, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Freeze"				},
	{DO+0, 0x01, 0x80, 0x80, "Off"					},
	{DO+0, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Coinage"				},
	{DO+1, 0x01, 0x03, 0x03, "2 Coins 1 Credits"	},
	{DO+1, 0x01, 0x03, 0x02, "1 Coin  1 Credits"	},
	{DO+1, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},
	{DO+1, 0x01, 0x03, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{DO+1, 0x01, 0x0c, 0x00, "*1"					},
	{DO+1, 0x01, 0x0c, 0x04, "*4"					},
	{DO+1, 0x01, 0x0c, 0x08, "*5"					},
	{DO+1, 0x01, 0x0c, 0x0c, "*6"					},

	{0   , 0xfe, 0   ,    2, "Coin A"				},
	{DO+1, 0x01, 0x10, 0x00, "*1"					},
	{DO+1, 0x01, 0x10, 0x10, "*2"					},

	{0   , 0xfe, 0   ,    6, "Bonus Coin Adder"		},
	{DO+1, 0x01, 0xe0, 0x20, "2 gives 1"			},
	{DO+1, 0x01, 0xe0, 0x60, "4 gives 2"			},
	{DO+1, 0x01, 0xe0, 0xa0, "3 gives 1"			},
	{DO+1, 0x01, 0xe0, 0x40, "4 gives 1"			},
	{DO+1, 0x01, 0xe0, 0x80, "5 gives 1"			},
	{DO+1, 0x01, 0xe0, 0x00, "None"					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{DO+2, 0x01, 0x10, 0x10, "Off"					},
	{DO+2, 0x01, 0x10, 0x00, "On"					},
};

STDDIPINFO(Esb)

static void check_mbox_timer()
{
	if (mathing && M6809TotalCycles() - mathing_cyc >= mathing) {
		mathing = 0;
	}
}

static void run_mbox()
{
	static INT16 A, B, C;
	static INT32 ACC;

	int RAMWORD = 0;
	int MA_byte;
	int tmp;
	int M_STOP = 100000;
	int MA;
	int IP15_8, IP7, IP6_0;

	mathing = 0; // start at 0 cycles.
	mathing_cyc = M6809TotalCycles();

	while (M_STOP > 0)
	{
		mathing += 5;

		IP15_8 = DrvStrPROM[MPA];
		IP7    = DrvAmPROM[MPA];
		IP6_0  = DrvMasPROM[MPA];

		if (IP7 == 0)
			MA = (IP6_0 & 3) | ((BIC & 0x01ff) << 2);
		else
			MA = IP6_0;

		MA_byte = MA << 1;
		RAMWORD = (DrvMathRAM[MA_byte + 1] & 0x00ff) | ((DrvMathRAM[MA_byte] & 0x00ff) << 8);

		if (IP15_8 & 0x10) // clear_acc
		{
			ACC = 0;
		}

		if (IP15_8 & 0x01) // & LAC
			ACC = (RAMWORD << 16);

		if (IP15_8 & 0x02) // read_acc
		{
			DrvMathRAM[MA_byte+1] = ((ACC >> 16) & 0xff);
			DrvMathRAM[MA_byte  ] = ((ACC >> 24) & 0xff);
		}

		if (IP15_8 & 0x04) // m_halt
			M_STOP = 0;

		if (IP15_8 & 0x08) // inc_bic
			BIC = (BIC + 1) & 0x1ff;

		if (IP15_8 & 0x20) // LDC
		{
			C = RAMWORD;

			ACC += (((INT32)(A - B) << 1) * C) << 1;
			A = (A & 0x8000)? 0xffff: 0;
			B = (B & 0x8000)? 0xffff: 0;
			mathing += 33;
		}

		if (IP15_8 & 0x40) // LDB
			B = RAMWORD;

		if (IP15_8 & 0x80) // LDA
			A = RAMWORD;

		tmp = MPA + 1;
		MPA = (MPA & 0x0300) | (tmp & 0x00ff);

		M_STOP--;
	}
}

static void swmathbx_write(UINT8 offset, UINT8 data)
{
	switch (offset & 7)
	{
		case 0:
			MPA = data << 2;
			run_mbox();
		break;

		case 1:
			BIC = (BIC & 0x00ff) | ((data & 0x01) << 8);
		break;

		case 2:
			BIC = (BIC & 0x0100) | data;
		break;

		case 4:
			divisor = (divisor & 0x00ff) | (data << 8);
			dvd_shift = dividend;
			quotient_shift = 0;
		break;

		case 5:
			divisor = (divisor & 0xff00) | data;
			for (INT32 i = 1; i < 16; i++)
			{
				quotient_shift <<= 1;
				if (((INT32)dvd_shift + (divisor ^ 0xffff) + 1) & 0x10000)
				{
					quotient_shift |= 1;
					dvd_shift = (dvd_shift + (divisor ^ 0xffff) + 1) << 1;
				}
				else
				{
					dvd_shift <<= 1;
				}
			}
		break;

		case 6:
			dividend = (dividend & 0x00ff) | (data << 8);
		break;

		case 7:
			dividend = (dividend & 0xff00) | (data);
		break;
	}
}

static UINT8 esb_slapstic_read(UINT16 offset)
{
	int result = slapstic_base[offset];
	int new_bank = SlapsticTweak(offset);

	if (new_bank != current_bank)
	{
		current_bank = new_bank;
		memcpy(slapstic_base, &slapstic_source[current_bank * 0x2000], 0x2000);
	}

	return result;
}

static void esb_slapstic_write(UINT16 offset, UINT8 data)
{
	int new_bank = SlapsticTweak(offset);

	if (new_bank != current_bank)
	{
		current_bank = new_bank;
		memcpy(slapstic_base, &slapstic_source[current_bank * 0x2000], 0x2000);
	}
}

#if 0 // iq_132!!!! how should we handle this nightmare?
static UINT8 esb_setopbase(UINT16 address)
{
	int prevpc = activecpu_get_previouspc(); // iq_132

	if ((address & 0xe000) == 0x8000)
	{
		esb_slapstic_read(address & 0x1fff);

		catch_nextBranch(); // iq_132
		return -1;
	}
	else if ((prevpc & 0xe000) == 0x8000)
	{
		if (prevpc != 0x8080 && prevpc != 0x8090 && prevpc != 0x80a0 && prevpc != 0x80b0)
			esb_slapstic_read(prevpc & 0x1fff);
	}

	return address;
}
#endif

static void bankswitch(INT32 data)
{
	bankdata = data;
	bprintf(0, _T("bank sw.  %X\n"), data);
	if (is_esb) {
		M6809MapMemory(DrvM6809ROM0 + 0x06000 + (bankdata * 0x0a000), 0x6000, 0x7fff, MAP_ROM);
		M6809MapMemory(DrvM6809ROM0 + 0x0a000 + (bankdata * 0x12000), 0xa000, 0xffff, MAP_ROM);
	} else {
		M6809MapMemory(DrvM6809ROM0 + 0x10000 + (bankdata * 0x02000), 0x6000, 0x7fff, MAP_ROM);
	}
}

static void starwars_main_write(UINT16 address, UINT8 data)
{
	check_mbox_timer();

	if ((address & 0xe000) == 0x8000 && is_esb) {
		esb_slapstic_write(address & 0x1fff, data);
		return;
	}

	if (address == 0x4400) {
		if (port_A & 0x80) bprintf(0, _T("soundlatch overrun!\n"));
		port_A |= 0x80;
		sound_data = data;

		if (sound_irq_enable) {
			M6809Close();
			M6809Open(1);
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
			M6809Close();
			M6809Open(0);
		}
		return;
	}
	
	if ((address & 0xffe0) == 0x4600) {
		avgdvg_go();
		avgletsgo = 1;
		return;
	}
		
	if ((address & 0xffe0) == 0x4620) {
		avgdvg_reset();
		return;
	}
		
	if ((address & 0xffe0) == 0x4640) {
		BurnWatchogWrite();
		return;
	}

	if ((address & 0xffe0) == 0x4660) {
		M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}	
		
	if ((address & 0xffe0) == 0x4680)
	{
		address &= 7;
		// 0 -> coin counter 0
		// 1 -> coin counter 1
		// 2 -> led status 3
		// 3 -> led status 2
		if (address == 4) {
			bankswitch(data >> 7);
			return;
		}
		// 5 -> reset PRNG
		// 6 -> led status 1
		if (address == 7) {
			memcpy (DrvNVRAMBuf, DrvNVRAM, 0x100);
		}
		return;
	}		

	if ((address & 0xffe0) == 0x46a0) {
		memcpy (DrvNVRAM, DrvNVRAMBuf, 0x100);
		return;
	}

	if ((address & 0xfffc) == 0x46c0) {
		control_num = address & 0x03;
		return;
	}

	if ((address & 0xffe0) == 0x46e0) {
		port_A &= 0x3f;
		M6809Close();
		M6809Open(1);
		M6809Reset();
		M6809Close();
		M6809Open(0);
		return;
	}

	if ((address & 0xfff8) == 0x4700) {
		swmathbx_write(address, data);
		return;
	}
}

static UINT8 starwars_main_read(UINT16 address)
{
	check_mbox_timer();

	if ((address & 0xe000) == 0x8000 && is_esb) {
		return esb_slapstic_read(address & 0x1fff);
	}

	if ((address & 0xffe0) == 0x4300) {
		return DrvInputs[0] & ~0x20;
	}
	
	if ((address & 0xffe0) == 0x4320) {
		UINT8 ret = DrvInputs[1] & ~0xc0;
		if (mathing) ret |= 0x80;
		if (avgdvg_done()) ret |= 0x40;
		return ret;
	}
	
	if ((address & 0xffe0) == 0x4340) {
		return DrvDips[0];
	}
	
	if ((address & 0xffe0) == 0x4360) {
		return DrvDips[1];
	}
	
	if ((address & 0xffe0) == 0x4380) {
		switch (control_num) {
			case 0: return ProcessAnalog(DrvAnalogPort1, 0, 1, 0x00, 0xff);
			case 1: return ProcessAnalog(DrvAnalogPort0, 0, 1, 0x00, 0xff);
			default: return 0;
		}
		return 0;
	}

	switch (address)
	{
		case 0x4400:
			port_A &= 0xbf;
			return main_data;
		
		case 0x4401:
			return port_A & 0xc0;
			
		case 0x4700:
			return (quotient_shift & 0xff00) >> 8;//quotient_shift >> 8;
		
		case 0x4701:
			return quotient_shift & 0x00ff;//quotient_shift;
		
		case 0x4703:
			return BurnRandom();
	}

	return 0;
}
	
static void starwars_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0x0000) {
		port_A |= 0x40;
		main_data = data;
//		cpu_boost_interleave(0, TIME_IN_USEC(100));		// iq_132!!!!!!! (run end with high interleave??)
		M6809RunEnd(); // iq_132, good?
		return;
	}
	
	if ((address & 0xff80) == 0x1000) {
		DrvM6809RAM1A[(address & 0x7f)] = data;
		return;
	}
	
	if ((address & 0xffe0) == 0x1080)
	{
		switch (address&0x1f)
		{
			case 0:
				{
					INT32 prev_port_A = port_A;
					port_A = (port_A & (~port_A_ddr)) | (data & port_A_ddr);
					if (prev_port_A & 1)
					{
						if ((port_A & 1) == 0)
							tms5220_write(port_B);
					}
				}
			return;

		case 1:
			port_A_ddr = data;
			return;

		case 2:
			port_B = data;
			return;

		case 3:
			port_B_ddr = data;
			return;

		case 7:
			sound_irq_enable = data;
			return;

		case 0x1f: bprintf(0, _T("timer_counter loadding.. %X!\n"), data);
			timer_counter = data * 1024;
			return;
		}
	}
	
	if ((address & 0xffc0) == 0x1800) {
		quad_pokey_w(address & 0x3f, data);
		return;
	}
}

static UINT8 starwars_sound_read(UINT16 address)
{
	if ((address & 0xf800) == 0x0800) {
		port_A &= 0x7f;
		if (sound_irq_enable)
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return sound_data;
	}
	
	if ((address & 0xffe0) == 0x1080)
	{
		switch (address & 0x1f)
		{
			case 0: 
				return port_A | 0x10 | (!tms5220_ready() << 2);

			case 1:
				return port_A_ddr;

			case 2:
				return port_B;

			case 3:
				return port_B_ddr;

			case 5:
				{
				if (irq_flag)
					M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
				INT32 tmp = irq_flag;
				irq_flag = 0;
				return tmp;
				}
		}
	}

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	bankswitch(0);
	M6809Reset();
	M6809Close();

	M6809Open(1);
	tms5220_reset();
	M6809Reset();
	M6809Close();

	if (is_esb)
		SlapsticReset();
	BurnWatchdogReset();
	BurnRandomSetSeed(0x132132132132132ull);

	vector_reset();
	avgdvg_reset();

	control_num = 0;
	port_A = 0;
	port_A_ddr = 0;
	port_B = 0;
	port_B_ddr = 0;
	sound_data = 0;
	main_data = 0;
	sound_irq_enable = 0;
	irq_flag = 0;
	avgletsgo = 0;
	// MATHBOX
	mathing = 0;
	MPA = 0;
	BIC = 0;
	dvd_shift = 0;
	quotient_shift = 0;
	divisor = 0;
	dividend = 0;

	if (is_esb) {
		current_bank = SlapsticBank();
		memcpy(slapstic_base, &slapstic_source[current_bank * 0x2000], 0x2000);
	}

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM0		= Next; Next += 0x022000;
	DrvM6809ROM1		= Next; Next += 0x010000;

	DrvMathPROM			= Next; Next += 0x001000;

	DrvAVGPROM			= Next; Next += 0x000100;

	DrvStrPROM			= Next; Next += 0x000400;
	DrvMasPROM			= Next; Next += 0x000400;
	DrvAmPROM			= Next; Next += 0x000400;

	DrvPalette			= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);
	
	DrvNVRAM			= Next; Next += 0x000100;
	DrvNVRAMBuf			= Next; Next += 0x000100;

	AllRam				= Next;

	DrvM6809RAM0		= Next; Next += 0x001000;
	DrvM6809RAM1A		= Next; Next += 0x000080;
	DrvM6809RAM1B		= Next; Next += 0x000800;
	DrvMathRAM			= Next; Next += 0x001000;

	// leave these two alone! We need to overflow into rom!
	DrvVectorRAM		= Next; Next += 0x003000;
	RamEnd				= Next;
	DrvVectorROM		= Next; Next += 0x001000;

	DrvColRAM			= Next; Next += 0x000020; // iq_132

	MemEnd				= Next;

	return 0;
}

static void swmathbox_init()
{
	UINT16 val;

	for (INT32 cnt = 0; cnt < 1024; cnt++)
	{
		val  = (DrvMathPROM[0x0c00 + cnt]      ) & 0x000f;
		val |= (DrvMathPROM[0x0800 + cnt] <<  4) & 0x00f0;
		val |= (DrvMathPROM[0x0400 + cnt] <<  8) & 0x0f00;
		val |= (DrvMathPROM[0x0000 + cnt] << 12) & 0xf000;

		DrvStrPROM[cnt] = (val >> 8) & 0xff;
		DrvMasPROM[cnt] = val & 0x7f;
		DrvAmPROM[cnt]  = (val >> 7) & 0x0001;
	}
}

static INT32 DrvInit(INT32 game_select)
{
	BurnSetRefreshRate(40.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();
	
	if (game_select == 0) // starwars
	{
		if (BurnLoadRom(DrvVectorROM + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM0 + 0x10000,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM0 + 0x08000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x0a000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x0c000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x0e000,  5, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x04000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x0c000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x06000,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x0e000,  7, 1)) return 1;

		if (BurnLoadRom(DrvMathPROM  + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvMathPROM  + 0x00400,  9, 1)) return 1;
		if (BurnLoadRom(DrvMathPROM  + 0x00800, 10, 1)) return 1;
		if (BurnLoadRom(DrvMathPROM  + 0x00c00, 11, 1)) return 1;

		if (BurnLoadRom(DrvAVGPROM   + 0x00000, 12, 1)) return 1;
	}
	else if (game_select == 1) // tomcat
	{
		if (BurnLoadRom(DrvVectorROM + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x10000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x08000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x0a000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x0e000,  4, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x04000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x0c000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x06000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x0e000,  6, 1)) return 1;

		if (BurnLoadRom(DrvMathPROM  + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvMathPROM  + 0x00400,  8, 1)) return 1;
		if (BurnLoadRom(DrvMathPROM  + 0x00800,  9, 1)) return 1;
		if (BurnLoadRom(DrvMathPROM  + 0x00c00, 10, 1)) return 1;

		if (BurnLoadRom(DrvAVGPROM   + 0x00000, 11, 1)) return 1;

	}
	else if (game_select == 2) // esb
	{
		if (BurnLoadRom(DrvVectorROM + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x06000,  1, 1)) return 1;
		memcpy (DrvM6809ROM0 + 0x10000, DrvM6809ROM0 + 0x08000, 0x02000);
		if (BurnLoadRom(DrvM6809ROM0 + 0x0a000,  2, 1)) return 1;
		memcpy (DrvM6809ROM0 + 0x1c000, DrvM6809ROM0 + 0x0c000, 0x02000);
		if (BurnLoadRom(DrvM6809ROM0 + 0x0c000,  3, 1)) return 1;
		memcpy (DrvM6809ROM0 + 0x1e000, DrvM6809ROM0 + 0x0e000, 0x02000);
		if (BurnLoadRom(DrvM6809ROM1 + 0x00000,  4, 1)) return 1; // temp buffer
		memcpy (DrvM6809ROM0 + 0x0e000, DrvM6809ROM1 + 0x00000, 0x02000);
		memcpy (DrvM6809ROM0 + 0x20000, DrvM6809ROM1 + 0x02000, 0x02000);
		if (BurnLoadRom(DrvM6809ROM0 + 0x14000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM0 + 0x18000,  6, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM1 + 0x04000,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x0c000,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x06000,  8, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM1 + 0x0e000,  8, 1)) return 1;

		if (BurnLoadRom(DrvMathPROM  + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvMathPROM  + 0x00400, 10, 1)) return 1;
		if (BurnLoadRom(DrvMathPROM  + 0x00800, 11, 1)) return 1;
		if (BurnLoadRom(DrvMathPROM  + 0x00c00, 12, 1)) return 1;

		if (BurnLoadRom(DrvAVGPROM   + 0x00000, 13, 1)) return 1;

		is_esb = 1;
	}
		
	M6809Init(2);
	M6809Open(0);
	M6809MapMemory(DrvVectorRAM,			0x0000, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvVectorROM,			0x3000, 0x3fff, MAP_ROM);
	M6809MapMemory(DrvNVRAM,				0x4500, 0x45ff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM0,			0x4800, 0x4fff, MAP_RAM);
	M6809MapMemory(DrvMathRAM,				0x5000, 0x5fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM0 + 0x8000,	0x8000, 0xffff, MAP_ROM);
	if (is_esb) M6809UnmapMemory(0x8000, 0x9fff, MAP_RAM);
	M6809SetWriteHandler(starwars_main_write);
	M6809SetReadHandler(starwars_main_read);
	M6809Close();

	M6809Open(1);
	M6809MapMemory(DrvM6809RAM1B,			0x2000, 0x27ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM1 + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(starwars_sound_write);
	M6809SetReadHandler(starwars_sound_read);
	M6809Close();

	SlapsticInit(101);
	slapstic_source = DrvM6809ROM0 + 0x14000;
	slapstic_base = DrvM6809ROM0 + 0x08000;

	BurnWatchdogInit(DrvDoReset, 180 /*NOT REALLY*/);
	BurnRandomInit();

	vector_init();
	vector_set_scale(250, 280);
	avg_starwars_start(DrvVectorRAM, M6809TotalCycles);

	PokeyInit(1500000, 4, 2.40, 0);
	PokeySetTotalCyclesCB(M6809TotalCycles);
	
	tms5220_init();
	tms5220_set_frequency(640000);
	
	swmathbox_init();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	vector_exit();

	SlapsticExit();
	tms5220_exit();
	PokeyExit();
	M6809Exit();

	BurnFree(AllMem);

	is_esb = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 32; i++) // color
	{
		for (INT32 j = 0; j < 256; j++) // intensity
		{
			INT32 r = (i & 4) ? 0xff : 0;	
			INT32 g = (i & 2) ? 0xff : 0;	
			INT32 b = (i & 1) ? 0xff : 0;

			r = (r * j) / 255;
			g = (g * j) / 255;
			b = (b * j) / 255;
	
			DrvPalette[i * 256 + j] = (r << 16) | (g << 8) | b;
		}
	}
}

static INT32 DrvDraw()
{
	DrvPaletteInit();

	if (avgletsgo) avgdvg_go();

	draw_vector(DrvPalette);

	return 0;
}
	
static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	//M6809NewFrame(); no!
	
	{
		DrvInputs[0] = 0xff & ~0x20;
		DrvInputs[1] = 0xff & ~(1+2+8);
		
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		DrvInputs[0] = (DrvInputs[0] & ~0x10) | (DrvDips[2] & 0x10);
	}

	INT32 nInterleave = 256;
	INT32 nTotalCycles[2] = { 1512000 / 40, 1512000 / 40 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6809Open(0);
		nCyclesDone[0] += M6809Run(nTotalCycles[0] / nInterleave);
		if ((i % 40) == 39) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK); // 6x (really 6.15)
		M6809Close();

		M6809Open(1);
		INT32 done = M6809Run(nTotalCycles[1] / nInterleave);
		nCyclesDone[1] += done;
		if (timer_counter >= 0) {		// should this be a one-shot or repeat??
			timer_counter -= done;
			if (timer_counter <= 0) {
					irq_flag |= 0x80;
					M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
			}
		}
		M6809Close();
	}

	if (pBurnSoundOut) {
		pokey_update(pBurnSoundOut, nBurnSoundLen);
		tms5220_update(pBurnSoundOut, nBurnSoundLen);
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

		M6809Scan(nAction);
		SlapsticScan(nAction);
		pokey_scan(nAction, pnMin);
		tms5220_scan(nAction, pnMin);

		SCAN_VAR(bankdata);
		SCAN_VAR(control_num);
		SCAN_VAR(port_A);
		SCAN_VAR(port_A_ddr);
		SCAN_VAR(port_B);
		SCAN_VAR(port_B_ddr);
		SCAN_VAR(sound_data);
		SCAN_VAR(main_data);
		SCAN_VAR(sound_irq_enable);
		SCAN_VAR(irq_flag);
		SCAN_VAR(avgletsgo);
		SCAN_VAR(timer_counter);

		SCAN_VAR(MPA);
		SCAN_VAR(BIC);
		SCAN_VAR(dvd_shift);
		SCAN_VAR(quotient_shift);
		SCAN_VAR(divisor);
		SCAN_VAR(dividend);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00100;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);

		ba.Data		= DrvNVRAMBuf;
		ba.nLen		= 0x00100;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM Buffer";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE)
	{
		M6809Open(0);
		bankswitch(bankdata << 7);
		M6809Close();
	}

	return 0;
}


// Star Wars (rev 1)

static struct BurnRomInfo starwarsRomDesc[] = {
	{ "136021.105",		0x1000, 0x538e7d2f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "136021.214",		0x4000, 0x04f1876e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136021.102",		0x2000, 0xf725e344, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136021.203",		0x2000, 0xf6da0a00, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136021.104",		0x2000, 0x7e406703, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136021.206",		0x2000, 0xc7e51237, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136021.107",		0x2000, 0xdbf3aea2, 2 | BRF_PRG | BRF_ESS }, //  6 M6809 #1 Code
	{ "136021.208",		0x2000, 0xe38070a8, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "136021.110",		0x0400, 0x01061762, 3 | BRF_PRG | BRF_ESS }, //  8 Math PROMs
	{ "136021.111",		0x0400, 0x2e619b70, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "136021.112",		0x0400, 0x6cfa3544, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "136021.113",		0x0400, 0x03f6acb2, 3 | BRF_PRG | BRF_ESS }, // 11

	{ "136021-105.1l",	0x0100, 0x82fc3eb2, 4 | BRF_GRA },           // 12 Video PROM
};

STD_ROM_PICK(starwars)
STD_ROM_FN(starwars)

static INT32 StarwarsInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvStarwars = {
	"starwars", NULL, NULL, NULL, "1983",
	"Star Wars (rev 1)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, starwarsRomInfo, starwarsRomName, NULL, NULL, StarwarsInputInfo, StarwarsDIPInfo,
	StarwarsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	400, 300, 3, 4
};


// Star Wars (set 2)

static struct BurnRomInfo starwars1RomDesc[] = {
	{ "136021-105.1l",	0x1000, 0x538e7d2f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "136021.114.1f",	0x4000, 0xe75ff867, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136021.102.1hj",	0x2000, 0xf725e344, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136021.203.1jk",	0x2000, 0xf6da0a00, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136021.104.1kl",	0x2000, 0x7e406703, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136021.206.1m",	0x2000, 0xc7e51237, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136021-107.1jk",	0x2000, 0xdbf3aea2, 2 | BRF_PRG | BRF_ESS }, //  6 M6809 #1 Code
	{ "136021-208.1h",	0x2000, 0xe38070a8, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "136021-110.7h",	0x0400, 0x810e040e, 3 | BRF_PRG | BRF_ESS }, //  8 Math PROMs
	{ "136021-111.7j",	0x0400, 0xae69881c, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "136021-112.7k",	0x0400, 0xecf22628, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "136021-113.7l",	0x0400, 0x83febfde, 3 | BRF_PRG | BRF_ESS }, // 11

	{ "136021-109.4b",	0x0100, 0x82fc3eb2, 4 | BRF_GRA },           // 12 Video PROM
};

STD_ROM_PICK(starwars1)
STD_ROM_FN(starwars1)

struct BurnDriver BurnDrvStarwars1 = {
	"starwars1", "starwars", NULL, NULL, "1983",
	"Star Wars (set 2)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, starwars1RomInfo, starwars1RomName, NULL, NULL, StarwarsInputInfo, StarwarsDIPInfo,
	StarwarsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	400, 300, 3, 4
};


// Star Wars (set 3)

static struct BurnRomInfo starwarsoRomDesc[] = {
	{ "136021-105.1l",	0x1000, 0x538e7d2f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "136021-114.1f",	0x4000, 0xe75ff867, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136021-102.1hj",	0x2000, 0xf725e344, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136021-103.1jk",	0x2000, 0x3fde9ccb, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136021-104.1kl",	0x2000, 0x7e406703, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136021-206.1m",	0x2000, 0xc7e51237, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136021-107.1jk",	0x2000, 0xdbf3aea2, 2 | BRF_PRG | BRF_ESS }, //  6 M6809 #1 Code
	{ "136021-208.1h",	0x2000, 0xe38070a8, 2 | BRF_PRG | BRF_ESS }, //  7

	{ "136021-110.7h",	0x0400, 0x810e040e, 3 | BRF_PRG | BRF_ESS }, //  9 Math PROMs
	{ "136021-111.7j",	0x0400, 0xae69881c, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "136021-112.7k",	0x0400, 0xecf22628, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "136021-113.7l",	0x0400, 0x83febfde, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "136021-109.4b",	0x0100, 0x82fc3eb2, 4 | BRF_GRA },           //  8 Video PROM
};

STD_ROM_PICK(starwarso)
STD_ROM_FN(starwarso)

struct BurnDriver BurnDrvStarwarso = {
	"starwarso", "starwars", NULL, NULL, "1983",
	"Star Wars (set 3)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, starwarsoRomInfo, starwarsoRomName, NULL, NULL, StarwarsInputInfo, StarwarsDIPInfo,
	StarwarsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	250, 280, 3, 4
};


// TomCat (Star Wars hardware, prototype)

static struct BurnRomInfo tomcatswRomDesc[] = {
	{ "tcavg3.1l",		0x1000, 0x27188aa9, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "tc6.1f",			0x2000, 0x56e284ff, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tc8.1hj",		0x2000, 0x7b7575e3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tca.1jk",		0x2000, 0xa1020331, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tce.1m",			0x2000, 0x4a3de8a3, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "136021-107.1jk",	0x2000, 0x00000000, 2 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  5 M6809 #1 Code
	{ "136021-208.1h",	0x2000, 0x00000000, 2 | BRF_NODUMP | BRF_PRG | BRF_ESS }, //  6

	{ "136021-110.7h",	0x0400, 0x810e040e, 3 | BRF_PRG | BRF_ESS }, //  7 Math PROMs
	{ "136021-111.7j",	0x0400, 0xae69881c, 3 | BRF_PRG | BRF_ESS }, //  8
	{ "136021-112.7k",	0x0400, 0xecf22628, 3 | BRF_PRG | BRF_ESS }, //  9
	{ "136021-113.7l",	0x0400, 0x83febfde, 3 | BRF_PRG | BRF_ESS }, // 10

	{ "136021-109.4b",	0x0100, 0x82fc3eb2, 4 | BRF_GRA },           // 11 Video PROM
};

STD_ROM_PICK(tomcatsw)
STD_ROM_FN(tomcatsw)

static INT32 TomcatInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvTomcatsw = {
	"tomcatsw", "tomcat", NULL, NULL, "1983",
	"TomCat (Star Wars hardware, prototype)\0", "No sound!", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, tomcatswRomInfo, tomcatswRomName, NULL, NULL, StarwarsInputInfo, StarwarsDIPInfo,
	TomcatInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	250, 280, 3, 4
};


// The Empire Strikes Back

static struct BurnRomInfo esbRomDesc[] = {
	{ "136031-111.1l",	0x1000, 0xb1f9bd12, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 #0 Code
	{ "136031-101.1f",	0x4000, 0xef1e3ae5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136031-102.1jk",	0x4000, 0x62ce5c12, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136031-203.1kl",	0x4000, 0x27b0889b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136031-104.1m",	0x4000, 0xfd5c725e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136031-105.3u",	0x4000, 0xea9e4dce, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136031-106.2u",	0x4000, 0x76d07f59, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "136031-113.1jk",	0x4000, 0x24ae3815, 2 | BRF_PRG | BRF_ESS }, //  7 M6809 #1 Code
	{ "136031-112.1h",	0x4000, 0xca72d341, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "136031-110.7h",	0x0400, 0xb8d0f69d, 3 | BRF_PRG | BRF_ESS }, //  9 Math PROMs
	{ "136031-109.7j",	0x0400, 0x6a2a4d98, 3 | BRF_PRG | BRF_ESS }, // 10
	{ "136031-108.7k",	0x0400, 0x6a76138f, 3 | BRF_PRG | BRF_ESS }, // 11
	{ "136031-107.7l",	0x0400, 0xafbf6e01, 3 | BRF_PRG | BRF_ESS }, // 12

	{ "136021-109.4b",	0x0100, 0x82fc3eb2, 4 | BRF_GRA },           // 13 Video PROM
};

STD_ROM_PICK(esb)
STD_ROM_FN(esb)

static INT32 EsbInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvEsb = {
	"esb", NULL, NULL, NULL, "1985",
	"The Empire Strikes Back\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, esbRomInfo, esbRomName, NULL, NULL, StarwarsInputInfo, EsbDIPInfo,
	EsbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
	250, 280, 3, 4
};


