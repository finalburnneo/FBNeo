// FB Alpha Kick & Run driver module
// Based on MAME driver by Ernesto Corvi

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2203.h"
#include "bitswap.h"
#include "taito_m68705.h"
#include "m6800_intf.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvShareRAM0;
static UINT8 *DrvShareRAM1;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvMCURAM;
static UINT8 *DrvPrtRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles;

static INT32 nBankData;
static INT32 nCharBank;
static INT32 nSoundCPUHalted;
static INT32 nSubCPUHalted;
static INT32 mcu_running;
static INT32 mcu_initialised;
static bool coin_last[2];
static INT32 coin_fract;
static INT32 mcu_address;
static INT32 mcu_latch;

static INT32 has_mcu = 0; // 1 = taito-style 68705 (bootleg mcu), 2 = 6801u4 (real mcu)
static INT32 has_sub = 0;

static void (*screen_update)();

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvJoy6[8];
static UINT8 DrvJoy7[8];
static UINT8 DrvJoy8[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[8];
static UINT8 DrvReset;

// 6801u4 mcu: kicknrun, kicknrunu, kikikai (when dumped)
static UINT8 ddr1, ddr2, ddr3, ddr4;
static UINT8 port1_in, port2_in, port3_in, port4_in;
static UINT8 port1_out, port2_out, port3_out, port4_out;

static struct BurnInputInfo KikikaiInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy4 + 2,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Kikikai)

static struct BurnInputInfo Mexico86InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4 + 3,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy7 + 0,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy7 + 2,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy5 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy5 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy5 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy5 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy5 + 5,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy5 + 4,	"p3 fire 2"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy8 + 0,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy8 + 2,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy6 + 0,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy6 + 1,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy6 + 2,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy6 + 3,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy6 + 5,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy6 + 4,	"p4 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy4 + 0,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy4 + 2,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Mexico86)

static struct BurnDIPInfo KikikaiDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL							},
	{0x14, 0xff, 0xff, 0x7f, NULL							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x13, 0x01, 0x01, 0x00, "Upright"						},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x13, 0x01, 0x02, 0x02, "Off"							},
	{0x13, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x13, 0x01, 0x04, 0x04, "Off"							},
	{0x13, 0x01, 0x04, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Unused"						},
	{0x13, 0x01, 0x08, 0x08, "Off"							},
	{0x13, 0x01, 0x08, 0x00, "On"							},

	{0   , 0xfe, 0   ,    4, "Coin A"						},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"			},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"			},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"			},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin B"						},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"			},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"			},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"			},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x14, 0x01, 0x03, 0x02, "Easy"							},
	{0x14, 0x01, 0x03, 0x03, "Normal"						},
	{0x14, 0x01, 0x03, 0x01, "Hard"							},
	{0x14, 0x01, 0x03, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Bonus Life"					},
	{0x14, 0x01, 0x0c, 0x00, "50000 100000"					},
	{0x14, 0x01, 0x0c, 0x0c, "70000 150000"					},
	{0x14, 0x01, 0x0c, 0x08, "70000 200000"					},
	{0x14, 0x01, 0x0c, 0x04, "100000 300000"				},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x14, 0x01, 0x30, 0x00, "2"							},
	{0x14, 0x01, 0x30, 0x30, "3"							},
	{0x14, 0x01, 0x30, 0x20, "4"							},
	{0x14, 0x01, 0x30, 0x10, "5"							},

	{0   , 0xfe, 0   ,    2, "Coinage"						},
	{0x14, 0x01, 0x40, 0x40, "A"							},
	{0x14, 0x01, 0x40, 0x00, "B"							},

	{0   , 0xfe, 0   ,    2, "Number Match"					},
	{0x14, 0x01, 0x80, 0x80, "Off"							},
	{0x14, 0x01, 0x80, 0x00, "On"							},
};

STDDIPINFO(Kikikai)

static struct BurnDIPInfo Mexico86DIPList[]=
{
	{0x23, 0xff, 0xff, 0xff, NULL							},
	{0x24, 0xff, 0xff, 0xfb, NULL							},

	{0   , 0xfe, 0   ,    2, "Master/Slave Mode"			},
	{0x23, 0x01, 0x01, 0x01, "Off"							},
	{0x23, 0x01, 0x01, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Unknown"						},
	{0x23, 0x01, 0x02, 0x02, "Off"							},
	{0x23, 0x01, 0x02, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Service Mode"					},
	{0x23, 0x01, 0x04, 0x04, "Off"							},
	{0x23, 0x01, 0x04, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x23, 0x01, 0x08, 0x00, "Off"							},
	{0x23, 0x01, 0x08, 0x08, "On"							},

	{0   , 0xfe, 0   ,    4, "Coin A"						},
	{0x23, 0x01, 0x30, 0x10, "2 Coins 1 Credits"			},
	{0x23, 0x01, 0x30, 0x30, "1 Coin  1 Credits"			},
	{0x23, 0x01, 0x30, 0x00, "2 Coins 3 Credits"			},
	{0x23, 0x01, 0x30, 0x20, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Coin B"						},
	{0x23, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"			},
	{0x23, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"			},
	{0x23, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"			},
	{0x23, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"					},
	{0x24, 0x01, 0x03, 0x03, "Easy"							},
	{0x24, 0x01, 0x03, 0x02, "Normal"						},
	{0x24, 0x01, 0x03, 0x01, "Hard"							},
	{0x24, 0x01, 0x03, 0x00, "Hardest"						},

	{0   , 0xfe, 0   ,    4, "Playing Time"					},
	{0x24, 0x01, 0x0c, 0x00, "40 Seconds"					},
	{0x24, 0x01, 0x0c, 0x0c, "One Minute"					},
	{0x24, 0x01, 0x0c, 0x08, "One Minute and 20 Sec."		},
	{0x24, 0x01, 0x0c, 0x04, "One Minute and 40 Sec."		},

	{0   , 0xfe, 0   ,    2, "Unknown"						},
	{0x24, 0x01, 0x10, 0x10, "Off"							},
	{0x24, 0x01, 0x10, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Board ID"						},
	{0x24, 0x01, 0x20, 0x20, "Master"						},
	{0x24, 0x01, 0x20, 0x00, "Slave"						},

	{0   , 0xfe, 0   ,    2, "Number of Matches"			},
	{0x24, 0x01, 0x40, 0x00, "2"							},
	{0x24, 0x01, 0x40, 0x40, "6"							},

	{0   , 0xfe, 0   ,    2, "Single board 4 Players Mode"	},
	{0x24, 0x01, 0x80, 0x80, "Off"							},
	{0x24, 0x01, 0x80, 0x00, "On"							},
};

STDDIPINFO(Mexico86)

static void bankswitch(INT32 data)
{
	nBankData = data;
	nCharBank = (data & 0x20) >> 5;

	INT32 bank = 0x8000 + (data & 7) * 0x4000;

	ZetMapMemory(DrvZ80ROM0 + bank, 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall mexico86_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf000:
			bankswitch(data);
		return;

		case 0xf008:
			{
				mcu_running = data & 2;

				if (!mcu_running)
					mcu_initialised = 0;

				if (!mcu_running) {
					switch (has_mcu) {
						case 1:
							m6805Open(0);
							m6805Reset();
							m6805Close();
							break;
						case 2:
							M6801Open(0);
							M6801Reset();
							M6801Close();
							break;
					}
				}

				nSoundCPUHalted = ~data & 0x04;
				if (nSoundCPUHalted) {
					ZetReset(1);
				}
			}
			return;

		case 0xf018:
			return; // wdt?
	}
}

static UINT8 __fastcall mexico86_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xf010:
			return DrvInputs[3];
	}

	return 0;
}

static void __fastcall mexico86_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
		case 0xc001:
			BurnYM2203Write(0, address & 1, data);
		return;
	}
}

static UINT8 __fastcall mexico86_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			return BurnYM2203Read(0,0) & 0x7f; // ignore busy status

		case 0xc001:
			return BurnYM2203Read(0,1);
	}

	return 0;
}

static void __fastcall mexico86_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc004:
			// coin lockout & counters
		return;
	}
}

static UINT8 __fastcall mexico86_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			return DrvInputs[4];

		case 0xc001:
			return DrvInputs[5];

		case 0xc002:
			return DrvInputs[6];

		case 0xc003:
			return DrvInputs[7];
	}

	return 0;
}

// bootleg mcu
static void mexico86_m68705_portB_out(UINT8 *data)
{
	if ((ddrB & 0x01) && (~*data & 0x01) && (portB_out & 0x01))
	{
		portA_in = mcu_latch;
	}

	if ((ddrB & 0x02) && (*data & 0x02) && (~portB_out & 0x02))
	{
		mcu_address = portA_out;
	}

	if ((ddrB & 0x08) && (~*data & 0x08) && (portB_out & 0x08))
	{
		if (*data & 0x10) // read
		{
			if (*data & 0x04)
			{
				mcu_latch = DrvPrtRAM[mcu_address];
			}
			else
			{
				mcu_latch = DrvInputs[(mcu_address & 1) + 1];
			}
		}
		else
		{
			DrvPrtRAM[mcu_address] = portA_out;
		}
	}
	if ((ddrB & 0x20) && (*data & 0x20) && (~portB_out & 0x20))
	{
		ZetSetVector(0, DrvPrtRAM[0]);
		ZetSetIRQLine(0, 0, CPU_IRQSTATUS_HOLD);
		m68705SetIrqLine(M68705_IRQ_LINE, CPU_IRQSTATUS_NONE);
	}

	portB_out = *data;
}

static void mexico86_m68705_portC_in()
{
	portC_in = DrvInputs[0];
}

static m68705_interface mexico86_m68705_interface = {
	NULL /* portA */, mexico86_m68705_portB_out /* portB */, NULL /* portC */,
	NULL /* ddrA  */, NULL                      /* ddrB  */, NULL /* ddrC  */,
	NULL /* portA */, NULL                      /* portB */, mexico86_m68705_portC_in /* portC */
};

// real mcu - M6801U4
static UINT8 mcu_read(UINT16 address)
{
	if (address >= 0x0080 && address <= 0x00ff) {
		return DrvMCURAM[address & 0x7f];
	}

	if (address >= 0x0008 && address <= 0x001f) {
		return m6803_internal_registers_r(address);
	}

	switch (address) {
		case 0x00: {
			return ddr1;
		}

		case 0x01: {
			return ddr2;
		}

		case 0x02: {
			port1_in = DrvInputs[0];
			return (port1_out & ddr1) | (port1_in & ~ddr1);
		}

		case 0x03: {
			return (port2_out & ddr2) | (port2_in & ~ddr2);
		}

		case 0x04: {
			return ddr3;
		}

		case 0x05: {
			return ddr4;
		}

		case 0x06: {
			return (port3_out & ddr3) | (port3_in & ~ddr3);
		}

		case 0x07: {
			return (port4_out & ddr4) | (port4_in & ~ddr4);
		}
	}

	bprintf(PRINT_NORMAL, _T("M6801 Read Byte -> %04X\n"), address);

	return 0;
}

static void mcu_write(UINT16 address, UINT8 data)
{
	if (address >= 0x0080 && address <= 0x00ff) {
		DrvMCURAM[address & 0x7f] = data;
		return;
	}

	if (address >= 0x0008 && address <= 0x001f) {
		m6803_internal_registers_w(address, data);
		return;
	}

	switch (address) {
		case 0x00: {
			ddr1 = data;
			return;
		}

		case 0x01: {
			ddr2 = data;
			return;
		}

		case 0x02: {
			// bits 0, 1 - coin counters (low -> high transition)
			// bits 4, 5 - coin lockouts (inverted)
			port1_out = data;
			return;
		}

		case 0x03: {
			if ((port2_out & 0x4) && (~data & 0x4)) { // clock
				if (data & 0x10) { // read (shared mem, inputs)
					if (data & 0x1) {
						port3_in = DrvPrtRAM[port4_out];
					} else {
						port3_in = DrvInputs[(port4_out & 1) + 1];
					}
				} else { // write shared mem
					DrvPrtRAM[port4_out] = port3_out;
				}
			}

			port2_out = data;
			return;
		}

		case 0x04: {
			ddr3 = data;
			return;
		}

		case 0x05: {
			ddr4 = data;
			return;
		}

		case 0x06: {
			port3_out = data;
			return;
		}

		case 0x07: {
			port4_out = data;
			return;
		}
	}

	bprintf(PRINT_NORMAL, _T("M6801 Write Byte -> %04X, %02X\n"), address, data);
}

static UINT8 YM2203_read_portA(UINT32)
{
	return DrvDips[0];
}

static UINT8 YM2203_read_portB(UINT32)
{
	return DrvDips[1];
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	ZetOpen(2);
	ZetReset();
	ZetClose();

	switch (has_mcu) {
		case 1:
			m67805_taito_reset();
			break;
		case 2:
			M6801Open(0);
			M6801Reset();
			M6801Close();
			port1_in = 0x00;
			port2_in = 0x00;
			port3_in = 0x00;
			port4_in = 0x00;
			port1_out = 0x00;
			port2_out = 0x00;
			port3_out = 0x00;
			port4_out = 0x00;
			ddr1 = 0x00;
			ddr2 = 0x00;
			ddr3 = 0x00;
			ddr4 = 0x00;
	}

	nExtraCycles = 0;
	nBankData = 0;
	nCharBank = 0;
	nSoundCPUHalted = 0;
	nSubCPUHalted = (has_sub && (DrvDips[1] & 0x80)); // 4-player board

	mcu_running = 0;
	mcu_initialised = 0;
	coin_last[0] = false;
	coin_last[1] = false;
	coin_fract = 0;

	mcu_address = 0;
	mcu_latch = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x020000;
	DrvZ80ROM1		= Next; Next += 0x008000;
	DrvZ80ROM2		= Next; Next += 0x004000;
	DrvMCUROM		= Next; Next += 0x001000;

	DrvGfxROM		= Next; Next += 0x080000;

	DrvColPROM		= Next; Next += 0x000300;

	DrvPalette		= (UINT32*)Next; Next += 0x0101 * sizeof(UINT32);

	AllRam			= Next;

	DrvShareRAM0	= Next; Next += 0x003000;
	DrvShareRAM1	= Next; Next += 0x000800;
	DrvZ80RAM1		= Next; Next += 0x001800;
	DrvZ80RAM2		= Next; Next += 0x000800;
	DrvMCURAM		= Next; Next += 0x000080;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

// forward
static void screen_update_mexico86();
static void screen_update_kikikai();

static INT32 DrvGfxDecode(INT32 swap)
{
	INT32 Plane[4] = { 0x20000*8, 0x20000*8+4, 0, 4 };
	INT32 XOffs[8] = { STEP4(3,-1), STEP4(11, -1) };
	INT32 YOffs[8] = { STEP8(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	if (swap) swap = 0x8000;

	for (INT32 i = 0; i < 0x40000; i++) tmp[i] = DrvGfxROM[i^swap] ^ 0xff; // invert

	GfxDecode(0x2000, 4, 8, 8, Plane, XOffs, YOffs, 0x80, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit(INT32 game)
{
	BurnAllocMemIndex();

	if (game == 0) // kiki
	{
		INT32 k = 0;
		if (BurnLoadRom(DrvGfxROM  + 0x00000, k++, 1)) return 1;
		memcpy(DrvZ80ROM0 + 0x00000, DrvGfxROM + 0x00000, 0x8000);
		memcpy(DrvZ80ROM0 + 0x18000, DrvGfxROM + 0x08000, 0x8000);
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000, k++, 1)) return 1;

//		if (BurnLoadRom(DrvMCUROM  + 0x00000, k++, 1)) return 1; // un-comment and remove next line when dump available!
		k++;

		if (BurnLoadRom(DrvGfxROM  + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x30000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, k++, 1)) return 1;

		DrvGfxDecode(0);

		has_mcu = 0; // set "has_mcu = 2" when mcu dump is available
		has_sub = 0;
	}
	else if (game == 1) // knightb
	{
		INT32 k = 0;
		if (BurnLoadRom(DrvGfxROM  + 0x00000, k++, 1)) return 1; // temp space to load segmented rom
		memcpy(DrvZ80ROM0 + 0x00000, DrvGfxROM + 0x00000, 0x8000);
		memcpy(DrvZ80ROM0 + 0x18000, DrvGfxROM + 0x08000, 0x8000);
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM  + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM  + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x30000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, k++, 1)) return 1;

		DrvGfxDecode(0);

		has_mcu = 1;
		has_sub = 0;
	}
	else if (game == 2 || game == 3) // mexico86 & kicknrun
	{
		INT32 k = 0;
		if (BurnLoadRom(DrvGfxROM  + 0x00000, k++, 1)) return 1;
		memcpy(DrvZ80ROM0 + 0x00000, DrvGfxROM + 0x00000, 0x8000);
		memcpy(DrvZ80ROM0 + 0x18000, DrvGfxROM + 0x08000, 0x8000);
		if (BurnLoadRom(DrvZ80ROM0 + 0x08000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000, k++, 1)) return 1;

		if (!strcmp(BurnDrvGetTextA(DRV_NAME), "mexico86a")) {
			if (BurnLoadRom(DrvZ80ROM1 + 0x00000, k++, 1)) return 1;
		}

		if (BurnLoadRom(DrvMCUROM  + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM  + 0x10000, k++, 1)) return 1; // temp space.
		memcpy(DrvGfxROM + 0x08000, DrvGfxROM + 0x10000, 0x8000);
		memcpy(DrvGfxROM + 0x00000, DrvGfxROM + 0x18000, 0x8000);

		if (BurnLoadRom(DrvGfxROM  + 0x10000, k  , 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x18000, k++, 1)) return 1; // reload

		if (BurnLoadRom(DrvGfxROM  + 0x28000, k++, 1)) return 1; // half @ 0x28000
		memcpy(DrvGfxROM + 0x20000, DrvGfxROM + 0x30000, 0x8000); // other half at 0x20000

		if (BurnLoadRom(DrvGfxROM  + 0x30000, k, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM  + 0x38000, k++, 1)) return 1; // reload

		if (BurnLoadRom(DrvColPROM + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00100, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, k++, 1)) return 1;

		DrvGfxDecode(0);

		switch (game) {
			case 2: has_mcu = 2; break; // M68001u4 real MCU / kicknrun
			case 3: has_mcu = 1; break; // m68705 (bootleg) / mexico86
		}
		has_sub = 1; // cpu for 4player mode
	}

	screen_update = (game >= 2) ? screen_update_mexico86 : screen_update_kikikai;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM0,		0xc000, 0xefff, MAP_RAM);

//	DrvVidRAM = DrvShareRAM0 + 0x0000; // 0xc000-0xd4ff
//	DrvSprRAM = DrvShareRAM0 + 0x1500; // 0xd500-0xd7ff
	DrvPrtRAM = DrvShareRAM0 + 0x2800; // 0xe800-0xe8ff

	ZetMapMemory(DrvShareRAM1,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(mexico86_main_write);
	ZetSetReadHandler(mexico86_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvShareRAM0,		0x8000, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1,		0xa800, 0xbfff, MAP_RAM);
	ZetSetWriteHandler(mexico86_sound_write);
	ZetSetReadHandler(mexico86_sound_read);
	ZetClose();

	ZetInit(2); // not on kiki / knightb
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,		0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,		0x4000, 0x47ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM1,		0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(mexico86_sub_write);
	ZetSetReadHandler(mexico86_sub_read);
	ZetClose();

	switch (has_mcu) {
		case 1:
			m67805_taito_init(DrvMCUROM, DrvMCURAM, &mexico86_m68705_interface); // not on kiki
			break;
		case 2:
			M6801Init(0);
			M6801Open(0);
			M6801MapMemory(DrvMCUROM, 0xf000, 0xffff, MAP_ROM);
			M6801SetReadHandler(mcu_read);
			M6801SetWriteHandler(mcu_write);
			M6801Close();
			break;
	}

	BurnYM2203Init(1,  3000000, NULL, 0);
	BurnYM2203SetPorts(0, &YM2203_read_portA, &YM2203_read_portB, NULL, NULL);
	BurnTimerAttachZet(6000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	switch (has_mcu) {
		case 1: m67805_taito_exit(); break;
		case 2: M6801Exit(); break;
	}

	BurnYM2203Exit();

	BurnFreeMemIndex();

	has_mcu = 0;
	has_sub = 0;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[i + 0x000] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i + 0x000] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i + 0x000] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i + 0x000] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x100] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x100] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x100] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x100] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x200] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x200] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x200] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x200] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}

//	DrvPalette[0x100] = 0x100; // ??????
}

static void screen_update_mexico86()
{
	INT32 sx = 0;

	for (INT32 offs = 0x1500; offs < 0x2000; offs += 4)
	{
		INT32 height;

		if (offs >= 0x1800 && offs < 0x1980)
			continue;

		if (offs >= 0x19c0)
			continue;

		if (*(UINT32 *)(&DrvShareRAM0[offs]) == 0)
			continue;

		INT32 gfx_num = DrvShareRAM0[offs + 1];
		INT32 gfx_attr = DrvShareRAM0[offs + 3];
		INT32 gfx_offs = 0;

		if (~gfx_num & 0x80)
		{
			gfx_offs = ((gfx_num & 0x1f) * 0x80) + ((gfx_num & 0x60) >> 1) + 12;
			height = 2;
		}
		else
		{
			gfx_offs = ((gfx_num & 0x3f) * 0x80);
			height = 32;
		}

		if ((gfx_num & 0xc0) == 0xc0)
			sx += 16;
		else
			sx = DrvShareRAM0[offs + 2];

		INT32 sy = 256 - height * 8 - (DrvShareRAM0[offs + 0]);

		for (INT32 xc = 0; xc < 2; xc++)
		{
			for (INT32 yc = 0; yc < height; yc++)
			{
				INT32 goffs = gfx_offs + xc * 0x40 + yc * 0x02;
				INT32 code = DrvShareRAM0[goffs] + ((DrvShareRAM0[goffs + 1] & 0x07) << 8)
						+ ((DrvShareRAM0[goffs + 1] & 0x80) << 4) + (nCharBank << 12);
				INT32 color = ((DrvShareRAM0[goffs + 1] & 0x38) >> 3) + ((gfx_attr & 0x02) << 2);
				INT32 flipx = DrvShareRAM0[goffs + 1] & 0x40;
				INT32 flipy = 0;

				INT32 x = (sx + xc * 8) & 0xff;
				INT32 y = (sy + yc * 8) & 0xff;

				Draw8x8MaskTile(pTransDraw, code, x, y - 16, flipx, flipy, color, 4, 0xf, 0, DrvGfxROM);
			}
		}
	}
}

static void screen_update_kikikai()
{
	INT32 sx = 0, sy, gfx_offs, height;

	for (INT32 offs = 0x1500; offs < 0x1800; offs += 4)
	{
		if (*(UINT32*)(DrvShareRAM0 + offs) == 0)
			continue;

		INT32 ty = DrvShareRAM0[offs];
		INT32 gfx_num = DrvShareRAM0[offs + 1];
		INT32 tx = DrvShareRAM0[offs + 2];

		if (gfx_num & 0x80)
		{
			gfx_offs = ((gfx_num & 0x3f) << 7);
			height = 32;
			if (gfx_num & 0x40) sx += 16;
			else sx = tx;
		}
		else
		{
			if (!(ty && tx)) continue;
			gfx_offs = ((gfx_num & 0x1f) << 7) + ((gfx_num & 0x60) >> 1) + 12;
			height = 2;
			sx = tx;
		}

		sy = 256 - (height << 3) - ty;

		height <<= 1;

		for (INT32 yc = 0; yc < height; yc += 2)
		{
			INT32 y = (sy + (yc << 2)) & 0xff;
			INT32 goffs = gfx_offs + yc;
			INT32 code = DrvShareRAM0[goffs] + ((DrvShareRAM0[goffs + 1] & 0x1f) << 8);
			INT32 color = (DrvShareRAM0[goffs + 1] & 0xe0) >> 5;
			goffs += 0x40;

			Draw8x8MaskTile(pTransDraw, code, (sx + 0) & 0xff, y - 16, 0, 0, color, 4, 0xf, 0, DrvGfxROM);

			code = DrvShareRAM0[goffs] + ((DrvShareRAM0[goffs + 1] & 0x1f) << 8);
			color = (DrvShareRAM0[goffs + 1] & 0xe0) >> 5;

			Draw8x8MaskTile(pTransDraw, code, (sx + 8) & 0xff, y - 16, 0, 0, color, 4, 0xf, 0, DrvGfxROM);
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear(0x100);

	screen_update();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void mcu_simulate() // kikikai
{
	UINT8 *protection_ram = DrvPrtRAM;

	if (!mcu_initialised)
	{
		if (protection_ram[0x01] == 0x00)
		{
			protection_ram[0x04] = 0xfc;   // coin inputs
			protection_ram[0x02] = 0xff;   // player 1
			protection_ram[0x03] = 0xff;   // player 2
			protection_ram[0x1b] = 0xff;   // active player
			protection_ram[0x06] = 0xff;   // must be FF otherwise PS4 ERROR
			protection_ram[0x07] = 0x03;   // must be 03 otherwise PS4 ERROR
			protection_ram[0x00] = 0x00;
			mcu_initialised = 1;
		}
	}

	if (mcu_initialised)
	{
		int i;
		bool coin_curr;
		UINT8 coin_in_read = DrvInputs[0] & 3;

		for (INT32 coin_idx = 0; coin_idx < 2; coin_idx++)
		{
			coin_curr = (coin_in_read & (1 << coin_idx)) == 0;
			if (coin_curr && coin_last[coin_idx] == false)
			{
				UINT8 coinage_setting = (DrvDips[0] >> (coin_idx*2 + 4)) & 3;

				// increase credits counter
				switch(coinage_setting)
				{
					case 0: // 2c / 3c
					case 1: // 2c / 1c
						if(coin_fract == 1)
						{
							protection_ram[0x01]+= (coinage_setting == 0) ? 3 : 1;
							coin_fract = 0;
						}
						else
							coin_fract ++;

						break;
					case 2: // 1c / 2c
					case 3: // 1c / 1c
						protection_ram[0x01]+= (coinage_setting == 2) ? 2 : 1;
						break;

				}

				protection_ram[0x0a] = 0x01;   // set flag (coin inserted sound is not played otherwise)
			}
			coin_last[coin_idx] = coin_curr;
		}
		// Purge any coin counter higher than 9 TODO: is this limit correct?
		if(protection_ram[0x01] > 9)
			protection_ram[0x01] = 9;

		protection_ram[0x04] = 0x3c | (coin_in_read ^ 3);   // coin inputs

		protection_ram[0x02] = BITSWAP08(DrvInputs[1], 7,6,5,4,2,3,1,0); // player 1
		protection_ram[0x03] = BITSWAP08(DrvInputs[2], 7,6,5,4,2,3,1,0); // player 2

		if (protection_ram[0x19] == 0xaa)  // player 2 active
			protection_ram[0x1b] = protection_ram[0x03];
		else
			protection_ram[0x1b] = protection_ram[0x02];

		for (i = 0; i < 0x10; i += 2)
			protection_ram[i + 0xb1] = protection_ram[i + 0xb0];

		for (i = 0; i < 0x0a; i++)
			protection_ram[i + 0xc0] = protection_ram[i + 0x90] + 1;

		if (protection_ram[0xd1] == 0xff)
		{
			if (protection_ram[0xd0] > 0 && protection_ram[0xd0] < 4)
			{
				protection_ram[0xd2] = 0x81;
				protection_ram[0xd0] = 0xff;
			}
		}

		if (protection_ram[0xe0] > 0 && protection_ram[0xe0] < 4)
		{
			static const UINT8 answers[3][16] =
			{
				{ 0x00,0x40,0x48,0x50,0x58,0x60,0x68,0x70,0x78,0x80,0x88,0x00,0x00,0x00,0x00,0x00 },
				{ 0x00,0x04,0x08,0x0C,0x10,0x14,0x18,0x1C,0x20,0x31,0x2B,0x35,0x00,0x00,0x00,0x00 },
				{ 0x00,0x0C,0x0D,0x0E,0x0F,0x10,0x11,0x12,0x03,0x0A,0x0B,0x14,0x00,0x00,0x00,0x00 },
			};
			int table = protection_ram[0xe0] - 1;

			for (i = 1; i < 0x10; i++)
				protection_ram[0xe0 + i] = answers[table][i];
			protection_ram[0xe0] = 0xff;
		}

		if (protection_ram[0xf0] > 0 && protection_ram[0xf0] < 4)
		{
			protection_ram[0xf1] = 0xb3;
			protection_ram[0xf0] = 0xff;
		}

		// The following is missing from Knight Boy
		// this should be equivalent to the obfuscated kiki_clogic() below
		{
			static const UINT8 db[16]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x10,0x18,0x00,0x00,0x00,0x00};
			UINT16 sy = protection_ram[0xa0] + ((0x18) >> 1);
			UINT16 sx = protection_ram[0xa1] + ((0x18) >> 1);

			for (i = 0; i < 0x38; i += 8)
			{
				UINT8 hw = db[protection_ram[0x20 + i] & 0xf];

				if (hw)
				{
					UINT16 xdiff = sx - ((UINT16)(protection_ram[0x20 + i + 6]) << 8 | protection_ram[0x20 + i + 7]);
					if (xdiff < hw)
					{
						UINT16 ydiff = sy - ((UINT16)(protection_ram[0x20 + i + 4]) << 8 | protection_ram[0x20 + i + 5]);
						if (ydiff < hw)
							protection_ram[0xa2] = 1; // we have a collision
					}
				}
			}
		}
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		DrvInputs[0] = (has_mcu == 2) ? 0x00 : 0xff; // 6801u4 mcu - active high coins!
		DrvInputs[1] = 0xff;
		DrvInputs[2] = 0xff;
		DrvInputs[3] = 0xff;
		DrvInputs[4] = 0xff;
		DrvInputs[5] = 0xff;
		DrvInputs[6] = 0xfe;
		DrvInputs[7] = 0xfe;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
			DrvInputs[6] ^= (DrvJoy7[i] & 1) << i;
			DrvInputs[7] ^= (DrvJoy8[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[4] = { 6000000 / 60, 6000000 / 60, 4000000 / 60, 1000000 / 60 };
	INT32 nCyclesDone[4] = { nExtraCycles, 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);

		if (i == 255 && (has_mcu == 0 || has_mcu == 2)) {
			if (mcu_running && has_mcu == 0) mcu_simulate(); // kikikai mcu sim
		    ZetSetVector(DrvPrtRAM[0]);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		ZetClose();

		ZetOpen(1);
		if (nSoundCPUHalted) {
			CPU_IDLE_SYNCINT(1, Zet);
		} else {
			CPU_RUN_TIMER(1);
		}
		if (i == 255) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		if (has_sub && !nSubCPUHalted)
		{
			ZetOpen(2);
			CPU_RUN(2, Zet);
			if (i == 255) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			ZetClose();
		}

		if (!mcu_running || !has_mcu)
		{
			CPU_IDLE_NULL(3);
		}
		else if (has_mcu == 1)
		{
			m6805Open(0);
			CPU_RUN(3, m6805);
			if (i == 255) m68705SetIrqLine(M68705_IRQ_LINE, CPU_IRQSTATUS_ACK);
			m6805Close();
		}
		else if (has_mcu == 2)
		{
			M6801Open(0);
			CPU_RUN(3, M6801);
			if (i == 255) M6801SetIRQLine(0, CPU_IRQSTATUS_HOLD);
			M6801Close();
		}
	}

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter(); // kicknrun intro, dc offset from ay
	}

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	if (pBurnDraw)
		DrvDraw();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029672;
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

		switch (has_mcu) {
			case 1:
				m68705_taito_scan(nAction);
				break;
			case 2:
				M6801Scan(nAction);
				break;
		}

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(mcu_address);
		SCAN_VAR(mcu_latch);

		SCAN_VAR(ddr1);
		SCAN_VAR(ddr2);
		SCAN_VAR(ddr3);
		SCAN_VAR(ddr4);
		SCAN_VAR(port1_in);
		SCAN_VAR(port2_in);
		SCAN_VAR(port3_in);
		SCAN_VAR(port4_in);
		SCAN_VAR(port1_out);
		SCAN_VAR(port2_out);
		SCAN_VAR(port3_out);
		SCAN_VAR(port4_out);

		SCAN_VAR(nExtraCycles);
		SCAN_VAR(nBankData);
		SCAN_VAR(nCharBank);
		SCAN_VAR(nSoundCPUHalted);
		SCAN_VAR(nSubCPUHalted);
		SCAN_VAR(mcu_running);
		SCAN_VAR(mcu_initialised);
		SCAN_VAR(coin_last);
		SCAN_VAR(coin_fract);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(nBankData);
		ZetClose();
	}

	return 0;
}


// KiKi KaiKai

static struct BurnRomInfo kikikaiRomDesc[] = {
	{ "a85-17.h16",			0x10000, 0xc141d5ab, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a85-16.h18",			0x10000, 0x4094d750, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "a85-11.f6",			0x08000, 0xcc3539db, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "a85-01.g8",			0x00800, 0x00000000, 3 | BRF_NODUMP | BRF_PRG }, //  3 MCU Code (undumped)

	{ "a85-15.a1",			0x10000, 0xaebc8c32, 4 | BRF_GRA },           //  4 Graphics
	{ "a85-14.a3",			0x10000, 0xa9df0453, 4 | BRF_GRA },           //  5
	{ "a85-13.a4",			0x10000, 0x3eeaf878, 4 | BRF_GRA },           //  6
	{ "a85-12.a6",			0x10000, 0x91e58067, 4 | BRF_GRA },           //  7

	{ "a85-08.g15",			0x00100, 0xd15f61a8, 5 | BRF_GRA },           //  8 Color Data
	{ "a85-10.g12",			0x00100, 0x8fc3fa86, 5 | BRF_GRA },           //  9
	{ "a85-09.g14",			0x00100, 0xb931c94d, 5 | BRF_GRA },           // 10
};

STD_ROM_PICK(kikikai)
STD_ROM_FN(kikikai)

static INT32 KikikaiInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvKikikai = {
	"kikikai", NULL, NULL, NULL, "1986",
	"KiKi KaiKai\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, kikikaiRomInfo, kikikaiRomName, NULL, NULL, NULL, NULL, KikikaiInputInfo, KikikaiDIPInfo,
	KikikaiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x101,
	224, 256, 3, 4
};


// Knight Boy

static struct BurnRomInfo knightbRomDesc[] = {
	{ "a85-17.h16",			0x10000, 0xc141d5ab, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a85-16.h18",			0x10000, 0x4094d750, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "a85-11.f6",			0x08000, 0xcc3539db, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "knightb.uc",			0x00800, 0x3cc2bbe4, 3 | BRF_PRG | BRF_ESS }, //  3 M68705 Code

	{ "knightb.d",			0x10000, 0x53ecdb3f, 4 | BRF_GRA },           //  4 Graphics
	{ "a85-14.a3",			0x10000, 0xa9df0453, 4 | BRF_GRA },           //  5
	{ "knightb.b",			0x10000, 0x63ad7df3, 4 | BRF_GRA },           //  6
	{ "a85-12.a6",			0x10000, 0x91e58067, 4 | BRF_GRA },           //  7

	{ "a85-08.g15",			0x00100, 0xd15f61a8, 5 | BRF_GRA },           //  8 Color Data
	{ "a85-10.g12",			0x00100, 0x8fc3fa86, 5 | BRF_GRA },           //  9
	{ "a85-09.g14",			0x00100, 0xb931c94d, 5 | BRF_GRA },           // 10
};

STD_ROM_PICK(knightb)
STD_ROM_FN(knightb)

static INT32 KnightbInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvKnightb = {
	"knightb", "kikikai", NULL, NULL, "1986",
	"Knight Boy\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, knightbRomInfo, knightbRomName, NULL, NULL, NULL, NULL, KikikaiInputInfo, KikikaiDIPInfo,
	KnightbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x101,
	224, 256, 3, 4
};


// Kick and Run (World)

static struct BurnRomInfo kicknrunRomDesc[] = {
	{ "a87-08.h16",			0x10000, 0x715e1b04, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a87-07.h18",			0x10000, 0x6cb6ebfe, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "a87-06.f6",			0x08000, 0x1625b587, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "a87-01_jph1021p.h8",	0x01000, 0x9451e880, 3 | BRF_PRG | BRF_ESS }, //  3 M6801u4 Code

	{ "a87-09-1",			0x04000, 0x6a2ad32f, 4 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "a87-05.a1",			0x10000, 0x4eee3a8a, 5 | BRF_GRA },           //  5 Graphics
	{ "a87-04.a3",			0x08000, 0x8b438d20, 5 | BRF_GRA },           //  6
	{ "a87-03.a4",			0x10000, 0xf42e8a88, 5 | BRF_GRA },           //  7
	{ "a87-02.a6",			0x08000, 0x64f1a85f, 5 | BRF_GRA },           //  8

	{ "a87-10.g15",			0x00100, 0xbe6eb1f0, 6 | BRF_GRA },           //  9 Color Data
	{ "a87-12.g12",			0x00100, 0x3e953444, 6 | BRF_GRA },           // 10
	{ "a87-11.g14",			0x00100, 0x14f6c28d, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(kicknrun)
STD_ROM_FN(kicknrun)

static INT32 KicknrunInit()
{
	return DrvInit(2);
}

struct BurnDriver BurnDrvKicknrun = {
	"kicknrun", NULL, NULL, NULL, "1986",
	"Kick and Run (World)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, kicknrunRomInfo, kicknrunRomName, NULL, NULL, NULL, NULL, Mexico86InputInfo, Mexico86DIPInfo,
	KicknrunInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x101,
	256, 224, 4, 3
};


// Kick and Run (US)

static struct BurnRomInfo kicknrunuRomDesc[] = {
	{ "a87-23.h16",			0x10000, 0x37182560, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "a87-22.h18",			0x10000, 0x3b5a8354, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "a87-06.f6",			0x08000, 0x1625b587, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "a87-01_jph1021p.h8",	0x01000, 0x9451e880, 3 | BRF_PRG | BRF_ESS }, //  3 M6801u4 Code

	{ "a87-09-1",			0x04000, 0x6a2ad32f, 4 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "a87-05.a1",			0x10000, 0x4eee3a8a, 5 | BRF_GRA },           //  5 Graphics
	{ "a87-04.a3",			0x08000, 0x8b438d20, 5 | BRF_GRA },           //  6
	{ "a87-03.a4",			0x10000, 0xf42e8a88, 5 | BRF_GRA },           //  7
	{ "a87-02.a6",			0x08000, 0x64f1a85f, 5 | BRF_GRA },           //  8

	{ "a87-10.g15",			0x00100, 0xbe6eb1f0, 6 | BRF_GRA },           //  9 Color Data
	{ "a87-12.g12",			0x00100, 0x3e953444, 6 | BRF_GRA },           // 10
	{ "a87-11.g14",			0x00100, 0x14f6c28d, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(kicknrunu)
STD_ROM_FN(kicknrunu)

struct BurnDriver BurnDrvKicknrunu = {
	"kicknrunu", "kicknrun", NULL, NULL, "1986",
	"Kick and Run (US)\0", NULL, "Taito America Corp", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, kicknrunuRomInfo, kicknrunuRomName, NULL, NULL, NULL, NULL, Mexico86InputInfo, Mexico86DIPInfo,
	KicknrunInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x101,
	256, 224, 4, 3
};


// Mexico 86 (bootleg of Kick and Run) (set 1)

static struct BurnRomInfo mexico86RomDesc[] = {
	{ "2_g.bin",			0x10000, 0x2bbfe0fb, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "1_f.bin",			0x10000, 0x0b93e68e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "a87-06.f6",			0x08000, 0x1625b587, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code

	{ "68_h.bin",			0x00800, 0xff92f816, 3 | BRF_PRG | BRF_ESS }, //  3 M68705 Code

	{ "a87-09-1",			0x04000, 0x6a2ad32f, 4 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "4_d.bin",			0x10000, 0x57cfdbca, 5 | BRF_GRA },           //  5 Graphics
	{ "5_c.bin",			0x08000, 0xe42fa143, 5 | BRF_GRA },           //  6
	{ "6_b.bin",			0x10000, 0xa4607989, 5 | BRF_GRA },           //  7
	{ "7_a.bin",			0x08000, 0x245036b1, 5 | BRF_GRA },           //  8

	{ "a87-10.g15",			0x00100, 0xbe6eb1f0, 6 | BRF_GRA },           //  9 Color Data
	{ "a87-12.g12",			0x00100, 0x3e953444, 6 | BRF_GRA },           // 10
	{ "a87-11.g14",			0x00100, 0x14f6c28d, 6 | BRF_GRA },           // 11
};

STD_ROM_PICK(mexico86)
STD_ROM_FN(mexico86)

static INT32 Mexico86Init()
{
	return DrvInit(3);
}

struct BurnDriver BurnDrvMexico86 = {
	"mexico86", "kicknrun", NULL, NULL, "1986",
	"Mexico 86 (bootleg of Kick and Run) (set 1)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, mexico86RomInfo, mexico86RomName, NULL, NULL, NULL, NULL, Mexico86InputInfo, Mexico86DIPInfo,
	Mexico86Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x101,
	256, 224, 4, 3
};


// Mexico 86 (bootleg of Kick and Run) (set 2)

static struct BurnRomInfo mexico86aRomDesc[] = {
	{ "2.bin",				0x10000, 0x397c93ad, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "1.bin",				0x10000, 0x0b93e68e, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "3x.bin",				0x08000, 0xabbbf6c4, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "3.bin",				0x08000, 0x1625b587, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "68_h.bin",			0x00800, 0xff92f816, 3 | BRF_PRG | BRF_ESS }, //  4 M68705 Code

	{ "8.bin",				0x04000, 0x6a2ad32f, 4 | BRF_PRG | BRF_ESS }, //  5 Z80 #2 Code

	{ "4.bin",				0x10000, 0x57cfdbca, 5 | BRF_GRA },           //  6 Graphics
	{ "5.bin",				0x08000, 0xe42fa143, 5 | BRF_GRA },           //  7
	{ "6.bin",				0x10000, 0xa4607989, 5 | BRF_GRA },           //  8
	{ "7.bin",				0x08000, 0x245036b1, 5 | BRF_GRA },           //  9

	{ "n82s129n.1.bin",		0x00100, 0xbe6eb1f0, 6 | BRF_GRA },           // 10 Color Data
	{ "n82s129n.3.bin",		0x00100, 0x3e953444, 6 | BRF_GRA },           // 11
	{ "n82s129n.2.bin",		0x00100, 0x14f6c28d, 6 | BRF_GRA },           // 12

	{ "ampal16l8pc.1.bin",	0x00104, 0x634f3a5b, 7 | BRF_OPT },           // 13 PLDs
	{ "ampal16l8pc.3.bin",	0x00104, 0xf9ce900a, 7 | BRF_OPT },           // 14
	{ "ampal16l8pc.4.bin",	0x00104, 0x39120b6f, 7 | BRF_OPT },           // 15
	{ "ampal16l8pc.5.bin",	0x00104, 0x1d27f7b9, 7 | BRF_OPT },           // 16
	{ "ampal16l8pc.6.bin",	0x00104, 0x9f941c8e, 7 | BRF_OPT },           // 17
	{ "ampal16l8pc.7.bin",	0x00104, 0x9f941c8e, 7 | BRF_OPT },           // 18
	{ "ampal16r4pc.2.bin",	0x00104, 0x213a71d1, 7 | BRF_OPT },           // 19
};

STD_ROM_PICK(mexico86a)
STD_ROM_FN(mexico86a)

struct BurnDriverD BurnDrvMexico86a = {
	"mexico86a", "kicknrun", NULL, NULL, "1986",
	"Mexico 86 (bootleg of Kick and Run) (set 2)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, mexico86aRomInfo, mexico86aRomName, NULL, NULL, NULL, NULL, Mexico86InputInfo, Mexico86DIPInfo,
	Mexico86Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x101,
	256, 224, 4, 3
};

