// Based on MAME driver by Aaron Giles

// the hdd is a MAME CHD (a raw dump also works), never written: the game's writes (audits, settings)
// go to <romset>.diff in the EEPROM path
// the games are also known for crashing, freezing, and having visual glitchs

#include "tiles_generic.h"
#include "mips3_intf.h"
#include "ide.h"
#include "dcs2k.h"

#define IDE_IRQ     1
#define VBLANK_IRQ  0

#define KINST   0x100000
#define KINST2  0x200000
#define KINST2UK 0x300000

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvBootROM;
static UINT8 *DrvRAM0;
static UINT8 *DrvRAM1;
static UINT8 *DrvSoundROM;
static UINT8 DrvRecalc = 1;
static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDSW[3];
static UINT32 DrvVRAMBase;
static UINT32 DrvInputs[3];
static UINT32 nSoundData;
static UINT32 nSoundCtrl;
static UINT8 DrvDcsHeld;	// DCS held in reset while the reset bit is low
static ide::ide_disk *DrvDisk;
static UINT8 DrvReset = 0;
static INT32 nKinstVersion;
static INT32 nBiosCount;
static INT32 nBiosLoaded;

static UINT8 nProtSel;
static UINT8 nProtCnt;
static UINT8 nProtRegA;
static UINT8 nProtRegB;

// Fast conversion from BGR555 to RGB565
static UINT32 *DrvColorLUT;

static struct BurnInputInfo kinstInputList[] = {
    { "P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 coin"	},
    { "P1 Start",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 start"	},
    { "P1 Up",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 up"		},
    { "P1 Down",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 down"	},
    { "P1 Left",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 left"	},
    { "P1 Right",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 right"	},
    { "P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
    { "P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
    { "P1 Button 3",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
    { "P1 Button 4",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},
    { "P1 Button 5",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 5"	},
    { "P1 Button 6",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 6"	},
	
    { "P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 coin"	},
    { "P2 Start",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 start"	},
    { "P2 Up",			BIT_DIGITAL,	DrvJoy2 + 6,	"p2 up"		},
    { "P2 Down",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"	},
    { "P2 Left",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 left"	},
    { "P2 Right",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 right"	},
    { "P2 Button 1",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
    { "P2 Button 2",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
    { "P2 Button 3",	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},
    { "P2 Button 4",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 4"	},
    { "P2 Button 5",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 5"	},
    { "P2 Button 6",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 6"	},
	
	{ "P3 Coin",		BIT_DIGITAL,	DrvJoy1 + 13,	"p3 coin"	},
	{ "P4 Coin",		BIT_DIGITAL,	DrvJoy1 + 14,	"p4 coin"	},
    
	{ "Reset", 			BIT_DIGITAL,	&DrvReset,		"reset"     },
	{ "Service",		BIT_DIGITAL,	DrvJoy2 + 13,	"service"   },
	{ "Service Mode",   BIT_DIGITAL,    DrvJoy1 + 12,   "diag"      },
	{ "Tilt",			BIT_DIGITAL,	DrvJoy2 + 12,	"tilt"      },
	{ "Volume Down",	BIT_DIGITAL,	DrvJoy3 + 2,	"volumedown"},
	{ "Volume Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"volumeup"  },
	{ "Dip A",			BIT_DIPSWITCH,	DrvDSW + 0,		"dip"       },
	{ "Dip B",			BIT_DIPSWITCH,	DrvDSW + 1,		"dip"       },
	{ "Dip C",			BIT_DIPSWITCH,	DrvDSW + 2,		"dip"       },
};

STDINPUTINFO(kinst)

static struct BurnDIPInfo kinstBaseDIPList[] =
{
	DIP_OFFSET(0x20)
	{0x00, 0xff, 0xff, 0xff, NULL                },
	{0x01, 0xff, 0xff, 0xff, NULL                },
	
	{0   , 0xfe, 0   ,    4, "Blood Level"       },
	{0x00, 0x01, 0x03, 0x03, "High"              },
	{0x00, 0x01, 0x03, 0x02, "Medium"            },
	{0x00, 0x01, 0x03, 0x01, "Low"               },
	{0x00, 0x01, 0x03, 0x00, "None"              },
	
	{0   , 0xfe, 0   ,    2, "Demo Sounds"       },
	{0x00, 0x01, 0x04, 0x00, "Off"               },
	{0x00, 0x01, 0x04, 0x04, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Finishing Moves"   },
	{0x00, 0x01, 0x08, 0x00, "Off"               },
	{0x00, 0x01, 0x08, 0x08, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Display Warning"   },
	{0x00, 0x01, 0x10, 0x00, "Off"               },
	{0x00, 0x01, 0x10, 0x10, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Blood"             },
	{0x00, 0x01, 0x20, 0x20, "Red"               },
	{0x00, 0x01, 0x20, 0x00, "White"             },
	
	{0   , 0xfe, 0   ,    2, "Coinage Source"    },
	{0x01, 0x01, 0x01, 0x01, "Dip Switch"        },
	{0x01, 0x01, 0x01, 0x00, "Disk"              },
	
	{0   , 0xfe, 0   ,   20, "Coinage"           },
	{0x01, 0x01, 0x3e, 0x3e, "USA-1"             },
	{0x01, 0x01, 0x3e, 0x3c, "USA-2"             },
	{0x01, 0x01, 0x3e, 0x3a, "USA-3"             },
	{0x01, 0x01, 0x3e, 0x38, "USA-4"             },
	{0x01, 0x01, 0x3e, 0x34, "USA-9"             },
	{0x01, 0x01, 0x3e, 0x32, "USA-10"            },
	{0x01, 0x01, 0x3e, 0x36, "USA-ECA"           },
	{0x01, 0x01, 0x3e, 0x30, "USA-Free Play"     },
	{0x01, 0x01, 0x3e, 0x2e, "German-1"          },
	{0x01, 0x01, 0x3e, 0x2c, "German-2"          },
	{0x01, 0x01, 0x3e, 0x2a, "German-3"          },
	{0x01, 0x01, 0x3e, 0x28, "German-4"          },
	{0x01, 0x01, 0x3e, 0x26, "German-ECA"        },
	{0x01, 0x01, 0x3e, 0x20, "German-Free Play"  },
	{0x01, 0x01, 0x3e, 0x1e, "French-1"          },
	{0x01, 0x01, 0x3e, 0x1c, "French-2"          },
	{0x01, 0x01, 0x3e, 0x1a, "French-3"          },
	{0x01, 0x01, 0x3e, 0x18, "French-4"          },
	{0x01, 0x01, 0x3e, 0x16, "French-ECA"        },
	{0x01, 0x01, 0x3e, 0x10, "French-Free Play"  },
	
	{0   , 0xfe, 0   ,    2, "Coin Counters"     },
	{0x01, 0x01, 0x40, 0x40, "1"                 },
	{0x01, 0x01, 0x40, 0x00, "2"                 },
	
	{0   , 0xfe, 0   ,    2, "Test Switch"       },
	{0x01, 0x01, 0x80, 0x80, "Off"               },
	{0x01, 0x01, 0x80, 0x00, "On"                },
};

static struct BurnDIPInfo kinst2BaseDIPList[] =
{
	DIP_OFFSET(0x20)
	{0x00, 0xff, 0xff, 0xff, NULL                },
	{0x01, 0xff, 0xff, 0xff, NULL                },
	
	{0   , 0xfe, 0   ,    4, "Blood Level"       },
	{0x00, 0x01, 0x03, 0x03, "High"              },
	{0x00, 0x01, 0x03, 0x02, "Medium"            },
	{0x00, 0x01, 0x03, 0x01, "Low"               },
	{0x00, 0x01, 0x03, 0x00, "None"              },
	
	{0   , 0xfe, 0   ,    2, "Demo Sounds"       },
	{0x00, 0x01, 0x04, 0x00, "Off"               },
	{0x00, 0x01, 0x04, 0x04, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Finishing Moves"   },
	{0x00, 0x01, 0x08, 0x00, "Off"               },
	{0x00, 0x01, 0x08, 0x08, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Display Warning"   },
	{0x00, 0x01, 0x10, 0x00, "Off"               },
	{0x00, 0x01, 0x10, 0x10, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Coinage Source"    },
	{0x01, 0x01, 0x01, 0x01, "Dip Switch"        },
	{0x01, 0x01, 0x01, 0x00, "Disk"              },
	
	{0   , 0xfe, 0   ,   20, "Coinage"           },
	{0x01, 0x01, 0x3e, 0x3e, "USA-1"             },
	{0x01, 0x01, 0x3e, 0x3c, "USA-2"             },
	{0x01, 0x01, 0x3e, 0x3a, "USA-3"             },
	{0x01, 0x01, 0x3e, 0x38, "USA-4"             },
	{0x01, 0x01, 0x3e, 0x34, "USA-9"             },
	{0x01, 0x01, 0x3e, 0x32, "USA-10"            },
	{0x01, 0x01, 0x3e, 0x36, "USA-ECA"           },
	{0x01, 0x01, 0x3e, 0x30, "USA-Free Play"     },
	{0x01, 0x01, 0x3e, 0x2e, "German-1"          },
	{0x01, 0x01, 0x3e, 0x2c, "German-2"          },
	{0x01, 0x01, 0x3e, 0x2a, "German-3"          },
	{0x01, 0x01, 0x3e, 0x28, "German-4"          },
	{0x01, 0x01, 0x3e, 0x26, "German-ECA"        },
	{0x01, 0x01, 0x3e, 0x20, "German-Free Play"  },
	{0x01, 0x01, 0x3e, 0x1e, "French-1"          },
	{0x01, 0x01, 0x3e, 0x1c, "French-2"          },
	{0x01, 0x01, 0x3e, 0x1a, "French-3"          },
	{0x01, 0x01, 0x3e, 0x18, "French-4"          },
	{0x01, 0x01, 0x3e, 0x16, "French-ECA"        },
	{0x01, 0x01, 0x3e, 0x10, "French-Free Play"  },
	
	{0   , 0xfe, 0   ,    2, "Coin Counters"     },
	{0x01, 0x01, 0x40, 0x40, "1"                 },
	{0x01, 0x01, 0x40, 0x00, "2"                 },
	
	{0   , 0xfe, 0   ,    2, "Test Switch"       },
	{0x01, 0x01, 0x80, 0x80, "Off"               },
	{0x01, 0x01, 0x80, 0x00, "On"                },
};

static struct BurnDIPInfo kinstBiosDIPList[] =
{
	{0x02, 0xff, 0xff, 0x00, NULL                },

	{0   , 0xfd, 0   ,    5, "BIOS"              },
	{0x02, 0x01, 0x07, 0x00, "v1.5d"             },
	{0x02, 0x01, 0x07, 0x01, "v1.4"              },
	{0x02, 0x01, 0x07, 0x02, "v1.3"              },
	{0x02, 0x01, 0x07, 0x03, "proto v4.7"        },
	{0x02, 0x01, 0x07, 0x04, "v1.5d AnyIDE"      },
};

static struct BurnDIPInfo kinst2BiosDIPList[] =
{
	{0x02, 0xff, 0xff, 0x00, NULL                },

	{0   , 0xfd, 0   ,    5, "BIOS"              },
	{0x02, 0x01, 0x07, 0x00, "v1.4"              },
	{0x02, 0x01, 0x07, 0x01, "v1.3"              },
	{0x02, 0x01, 0x07, 0x02, "v1.1"              },
	{0x02, 0x01, 0x07, 0x03, "v1.0"              },
	{0x02, 0x01, 0x07, 0x04, "v1.4 AnyIDE"       },
};

static struct BurnDIPInfo kinst2ukBiosDIPList[] =
{
	{0x02, 0xff, 0xff, 0x00, NULL                },

	{0   , 0xfd, 0   ,    3, "BIOS"              },
	{0x02, 0x01, 0x07, 0x00, "v1.4k"             },
	{0x02, 0x01, 0x07, 0x01, "v1.3k"             },
	{0x02, 0x01, 0x07, 0x02, "v1.4k AnyIDE"      },
};

STDDIPINFOEXT(kinst, kinstBase, kinstBios)
STDDIPINFOEXT(kinst2, kinst2Base, kinst2Bios)
STDDIPINFOEXT(kinst2uk, kinst2Base, kinst2ukBios)

static INT32 MemIndex()
{
    UINT8 *Next; Next = AllMem;

    DrvBootROM 	= Next;             Next += 0x80000;
	DrvSoundROM = Next;             Next += 0x1000000;
	
    AllRam      = Next;
    DrvRAM0     = Next;             Next += 0x80000;
    DrvRAM1     = Next;             Next += 0x800000;
	RamEnd		= Next;
    
    DrvColorLUT = (UINT32*) Next;   Next += 0x8000 * sizeof(UINT32);
	
    MemEnd		= Next;
    return 0;
}

static void GenerateColorLUT()
{
    for (INT32 i = 0; i < 0x8000; i++) {
		INT32 r = (i >>  0) & 0x1f;
		INT32 g = (i >>  5) & 0x1f;
		INT32 b = (i >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);
 
		DrvColorLUT[i] = BurnHighCol(r, g, b, 0);
    }
}

// the selectable boot EPROMs come first in the rom list, the DCS sound banks follow
static INT32 LoadBootRom(INT32 nBios)
{
	if (nBios >= nBiosCount) nBios = 0;
	if (nBios == nBiosLoaded) return 0;

	if (BurnLoadRom(DrvBootROM, nBios, 1)) {
		if (nBios == 0 || BurnLoadRom(DrvBootROM, 0, 1)) return 1;
		nBios = 0;
	}

	nBiosLoaded = nBios;
	Mips3InvalidateRange(0x1FC00000, 0x1FC7FFFF);

	return 0;
}

static void DrvDoReset()
{
	LoadBootRom(DrvDSW[2]);

	Mips3Reset();
	DrvDisk->reset();
	Dcs2kReset();
	
	DrvRecalc = 1;
	nSoundData = 0;
	nSoundCtrl = 0;
	DrvDcsHeld = 0;

	nProtSel = 0;
	nProtCnt = 0;
	nProtRegA = 0;
	nProtRegB = 0;
}

static void IDESetIRQState(INT32 state)
{
    Mips3SetIRQLine(IDE_IRQ, state);
}

static UINT32 ideRead(UINT32 address)
{
    if (address >= 0x10000100 && address <= 0x1000013f) {
        return DrvDisk->read((address - 0x10000100) / 8);
	}

    if (address >= 0x10000170 && address <= 0x10000173) {
        return DrvDisk->read_alternate(6);
	}
	
    return 0;
}

static void ideWrite(UINT32 address, UINT32 value)
{
    if (address >= 0x10000100 && address <= 0x1000013f) {
        DrvDisk->write((address - 0x10000100) / 8, value);
        return;
    }

    if (address >= 0x10000170 && address <= 0x10000173) {
        DrvDisk->write_alternate(6, value);
        return;
    }
}

static UINT32 kinstRead(UINT32 address)
{
    UINT32 tmp;
	
    if (address >= 0x10000080 && address <= 0x100000ff) {
        switch (address & 0xFF) {
			case 0x90: {
				tmp = ~DrvInputs[2];
				tmp &= ~2;
				if (Dcs2kControlRead() & 0x800) tmp |= 2;
				return tmp;
			}
			
			case 0x98: return ~0;
			case 0x80: return ~DrvInputs[0];
			case 0x88: return ~DrvInputs[1];
			
			case 0xa0: {
				tmp = DrvDSW[0] | (DrvDSW[1] << 8);
				return tmp;
			}
		}
		
        return ~0;
    }

    if (address >= 0x10000100 && address <= 0x10000173) {
        return ideRead(address);
    }
	
    return ~0;
}

static UINT32 kinst2Read(UINT32 address)
{
    UINT32 tmp;
	
    if (address >= 0x10000080 && address <= 0x100000ff) {
        switch (address & 0xFF) {
			case 0x80: {
				tmp = ~DrvInputs[2];
				tmp &= ~2;
				if (Dcs2kControlRead() & 0x800) tmp |= 2;
				return tmp;
			}
			
			case 0xA0: return ~0;

			case 0x98: return ~DrvInputs[0];
			case 0x90: return ~DrvInputs[1];
			
			case 0x88: {
				tmp = DrvDSW[0] | (DrvDSW[1] << 8);
				return tmp;
			}
		}

        return ~0;
    }

    if (address >= 0x10000100 && address <= 0x10000173) {
        return ideRead(address);
    }
	
    return ~0;
}

static void kinstWrite(UINT32 address, UINT64 value)
{
    if (address >= 0x10000080 && address <= 0x100000ff) {
        switch (address & 0xFF) {
			case 0x80: {
				DrvVRAMBase = (value & 4) ? 0x58000 : 0x30000;
				break;
			}
			
			case 0x88: {
				DrvDcsHeld = ~value & 1;
				Dcs2kResetWrite(~value & 1);
				break;
			}
			
			case 0x90: {
				UINT32 old = nSoundCtrl;
				nSoundCtrl = value;
				if (!(old & 2) && (nSoundCtrl & 2)) {
					Dcs2kDataWrite(nSoundData);
				}
				break;
			}
			
			case 0x98: {
				nSoundData = value;
				break;
			}
        }
        return;
    }

    if (address >= 0x10000100 && address <= 0x10000173) {
        ideWrite(address, value);
        return;
    }
}

static void kinst2Write(UINT32 address, UINT64 value)
{
    if (address >= 0x10000080 && address <= 0x100000ff) {
        switch (address & 0xFF) {
			case 0x98: {
				DrvVRAMBase = (value & 4) ? 0x58000 : 0x30000;
				break;
			}
			
			case 0x90: {
				DrvDcsHeld = ~value & 1;
				Dcs2kResetWrite(~value & 1);
				break;
			}
			
			case 0x80: {
				UINT32 old = nSoundCtrl;
				nSoundCtrl = value;
				if (!(old & 2) && (nSoundCtrl & 2)) {
					Dcs2kDataWrite(nSoundData);
				}
				break;
			}
			
			case 0xA0: {
				nSoundData = value;
				break;
			}
        }
        return;
    }

    if (address >= 0x10000100 && address <= 0x10000173) {
        ideWrite(address, value);
        return;
    }
}

static void kinstWriteByte(UINT32 address, UINT8 value) { kinstWrite(address, value); }
static void kinstWriteHalf(UINT32 address, UINT16 value) { kinstWrite(address, value); }
static void kinstWriteWord(UINT32 address, UINT32 value) { kinstWrite(address, value); }
static void kinstWriteDouble(UINT32 address, UINT64 value) { kinstWrite(address, value); }

static UINT8 kinstReadByte(UINT32 address) { return kinstRead(address); }
static UINT16 kinstReadHalf(UINT32 address) { return kinstRead(address); }
static UINT32 kinstReadWord(UINT32 address) { return kinstRead(address); }
static UINT64 kinstReadDouble(UINT32 address) { return kinstRead(address); }

static void kinst2WriteByte(UINT32 address, UINT8 value) { kinst2Write(address, value); }
static void kinst2WriteHalf(UINT32 address, UINT16 value) { kinst2Write(address, value); }
static void kinst2WriteWord(UINT32 address, UINT32 value) { kinst2Write(address, value); }
static void kinst2WriteDouble(UINT32 address, UINT64 value) { kinst2Write(address, value); }

static UINT8 kinst2ReadByte(UINT32 address) { return kinst2Read(address); }
static UINT16 kinst2ReadHalf(UINT32 address) { return kinst2Read(address); }
static UINT32 kinst2ReadWord(UINT32 address) { return kinst2Read(address); }
static UINT64 kinst2ReadDouble(UINT32 address) { return kinst2Read(address); }

// upgrade kit: KI2 on KI1 hardware, with a protection CPLD sharing IDE register 6
static UINT32 kinst2ukRead(UINT32 address)
{
	if ((address & ~3) == 0x10000130 && nProtSel) {
		return (nProtSel << 4) | (nProtCnt ^ nProtRegA ^ nProtRegB);
	}

	return kinstRead(address);
}

static void kinst2ukWrite(UINT32 address, UINT64 value)
{
	if ((address & ~3) == 0x10000130) {
		if (value & 0x10) {
			if (value & 0x40) {
				nProtSel = 0x05;
				nProtRegA = value & 0x0f;
			} else {
				nProtSel = 0x01;
				nProtRegB = value & 0x0f;
				nProtCnt = (nProtCnt + 1) & 0x0f;
			}
		} else {
			nProtSel = 0;
		}
	}

	kinstWrite(address, value);
}

static void kinst2ukWriteByte(UINT32 address, UINT8 value) { kinst2ukWrite(address, value); }
static void kinst2ukWriteHalf(UINT32 address, UINT16 value) { kinst2ukWrite(address, value); }
static void kinst2ukWriteWord(UINT32 address, UINT32 value) { kinst2ukWrite(address, value); }
static void kinst2ukWriteDouble(UINT32 address, UINT64 value) { kinst2ukWrite(address, value); }

static UINT8 kinst2ukReadByte(UINT32 address) { return kinst2ukRead(address); }
static UINT16 kinst2ukReadHalf(UINT32 address) { return kinst2ukRead(address); }
static UINT32 kinst2ukReadWord(UINT32 address) { return kinst2ukRead(address); }
static UINT64 kinst2ukReadDouble(UINT32 address) { return kinst2ukRead(address); }

static void MakeInputs()
{
    DrvInputs[0] = 0;
    DrvInputs[1] = 0;
	DrvInputs[2] = 0;

    for (INT32 i = 0; i < 16; i++) {
        if (DrvJoy1[i] & 1) DrvInputs[0] |= (1 << i);
        if (DrvJoy2[i] & 1) DrvInputs[1] |= (1 << i);
		if (DrvJoy3[i] & 1) DrvInputs[2] |= (1 << i);
    }

    //if (DrvDSW[0] & 1) DrvInputs[2] ^= 0x08000;
}

static INT32 LoadSoundBanks()
{
    memset(DrvSoundROM, 0xFF, 0x1000000);
    for (INT32 i = 0; i < 8; i++) {
        if (BurnLoadRom(DrvSoundROM + i * 0x200000, nBiosCount + i, 2)) return 1;
    }
    return 0;
}

// EPROM accesses get RdRdy wait states: the boot sequence (blue screen) takes about 6 seconds,
// booting faster makes the attract mode music go missing (as MAME)
#define ROM_WAIT_CYCLES		128

static UINT8 RomReadByte(UINT32 address)
{
	Mips3BurnCycles(ROM_WAIT_CYCLES);
	return DrvBootROM[address & 0x7ffff];
}

static UINT16 RomReadHalf(UINT32 address)
{
	Mips3BurnCycles(ROM_WAIT_CYCLES);
	return BURN_ENDIAN_SWAP_INT16(*(UINT16*)(DrvBootROM + (address & 0x7fffe)));
}

static UINT32 RomReadWord(UINT32 address)
{
	Mips3BurnCycles(ROM_WAIT_CYCLES);
	return BURN_ENDIAN_SWAP_INT32(*(UINT32*)(DrvBootROM + (address & 0x7fffc)));
}

static UINT64 RomReadDouble(UINT32 address)
{
	Mips3BurnCycles(ROM_WAIT_CYCLES);
	UINT64 v;
	memcpy(&v, DrvBootROM + (address & 0x7fff8), 8);
	return BURN_ENDIAN_SWAP_INT64(v);
}

static INT32 kinstSetup()
{
    Mips3SetReadByteHandler(1, kinstReadByte);
    Mips3SetReadHalfHandler(1, kinstReadHalf);
    Mips3SetReadWordHandler(1, kinstReadWord);
    Mips3SetReadDoubleHandler(1, kinstReadDouble);

    Mips3SetWriteByteHandler(1, kinstWriteByte);
    Mips3SetWriteHalfHandler(1, kinstWriteHalf);
    Mips3SetWriteWordHandler(1, kinstWriteWord);
    Mips3SetWriteDoubleHandler(1, kinstWriteDouble);

    Mips3MapHandler(1, 0x10000000, 0x100001FF, MAP_READ | MAP_WRITE);

    return 0;
}

static INT32 kinst2Setup()
{
    Mips3SetReadByteHandler(1, kinst2ReadByte);
    Mips3SetReadHalfHandler(1, kinst2ReadHalf);
    Mips3SetReadWordHandler(1, kinst2ReadWord);
    Mips3SetReadDoubleHandler(1, kinst2ReadDouble);

    Mips3SetWriteByteHandler(1, kinst2WriteByte);
    Mips3SetWriteHalfHandler(1, kinst2WriteHalf);
    Mips3SetWriteWordHandler(1, kinst2WriteWord);
    Mips3SetWriteDoubleHandler(1, kinst2WriteDouble);

    Mips3MapHandler(1, 0x10000000, 0x100001FF, MAP_READ | MAP_WRITE);

    return 0;
}

static INT32 kinst2ukSetup()
{
    Mips3SetReadByteHandler(1, kinst2ukReadByte);
    Mips3SetReadHalfHandler(1, kinst2ukReadHalf);
    Mips3SetReadWordHandler(1, kinst2ukReadWord);
    Mips3SetReadDoubleHandler(1, kinst2ukReadDouble);

    Mips3SetWriteByteHandler(1, kinst2ukWriteByte);
    Mips3SetWriteHalfHandler(1, kinst2ukWriteHalf);
    Mips3SetWriteWordHandler(1, kinst2ukWriteWord);
    Mips3SetWriteDoubleHandler(1, kinst2ukWriteDouble);

    Mips3MapHandler(1, 0x10000000, 0x100001FF, MAP_READ | MAP_WRITE);

    return 0;
}

static INT32 DrvInit(INT32 version)
{
    nKinstVersion = version;

    MemIndex();
    INT32 nLen = MemEnd - (UINT8 *)0;

    if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;

	// 50MHz / 8 pixel clock, 406 x 261 total
	BurnSetRefreshRate(6250000.0 / (406 * 261));

    DrvDisk = new ide::ide_disk();
    DrvDisk->set_irq_callback(IDESetIRQState);

    MemIndex();

    struct BurnRomInfo ri;
    nBiosCount = 0;
    while (BurnDrvGetRomInfo(&ri, nBiosCount) == 0 && (ri.nType & BRF_BIOS)) nBiosCount++;

    UINT32 nRet = LoadSoundBanks();
    if (nRet != 0) return 1;
	
	nRet = DrvDisk->load_hdd_image(0);
	if (nRet != 0) return 1;

    Dcs2kInit(DCS_2K, MHz(10));

    Mips3Init();
    
    DrvVRAMBase = 0x30000;

    // code is fetched (and cached) from the EPROM directly, data reads go through the wait state handler
    Mips3MapMemory(DrvBootROM,  0x1FC00000, 0x1FC7FFFF, MAP_ROM);
    Mips3SetReadByteHandler(2, RomReadByte);
    Mips3SetReadHalfHandler(2, RomReadHalf);
    Mips3SetReadWordHandler(2, RomReadWord);
    Mips3SetReadDoubleHandler(2, RomReadDouble);
    Mips3MapHandler(2, 0x1FC00000, 0x1FC7FFFF, MAP_READ);
    Mips3MapMemory(DrvRAM0,     0x00000000, 0x0007FFFF, MAP_RAM);
    Mips3MapMemory(DrvRAM1,     0x08000000, 0x087FFFFF, MAP_RAM);

    switch (version) {
		case KINST: {
			kinstSetup();
			break;
		}
		
		case KINST2: {
			kinst2Setup();
			break;
		}

		case KINST2UK: {
			kinst2ukSetup();
			break;
		}
    }

    nBiosLoaded = -1;
    if (LoadBootRom(DrvDSW[2])) return 1;

    Dcs2kMapSoundROM(DrvSoundROM, 0x1000000);
    Dcs2kBoot();
	
	GenericTilesInit();
	
	DrvDoReset();

    return 0;
}

static INT32 kinstDrvInit() { return DrvInit(KINST); }
static INT32 kinst2DrvInit() { return DrvInit(KINST2); }
static INT32 kinst2ukDrvInit() { return DrvInit(KINST2UK); }

static INT32 DrvExit()
{
    GenericTilesExit();
	Dcs2kExit();
	Mips3Exit();
	
    delete DrvDisk;
    
    BurnFree(AllMem);
	
    return 0;
}

static INT32 DrvDraw()
{
    UINT16 *src = (UINT16*) &DrvRAM0[DrvVRAMBase];

    for (INT32 y = 0; y < nScreenHeight; y++) {
        UINT16 *dst = (UINT16*) pTransDraw + (y * 320);

        for (INT32 x = 0; x < nScreenWidth; x++) {
            *dst = BURN_ENDIAN_SWAP_INT16(*src) & 0x7FFF;
            dst++;
            src++;
        }
    }
	
	BurnTransferCopy(DrvColorLUT);
	
    return 0;
}

// R4600: 100 MHz
// VIDEO: 58.98 Hz, 261 lines, vblank on lines 240-260 (as MAME)
//
static INT32 DrvFrame()
{
    if (DrvReset) DrvDoReset();
	
	if (DrvRecalc) {
		GenerateColorLUT();
		DrvRecalc = 0;
	}
	
	MakeInputs();

    const UINT64 nMipsCycPerFrame = (UINT64)MHz(100) * 100 / nBurnFPS;
    const UINT64 nMipsVblankCyc = nMipsCycPerFrame * 240 / 261;
    const UINT64 nDcsCycPerFrame = (UINT64)MHz(10) * 100 / nBurnFPS;

    UINT64 nNextMipsSegment = 0;
    UINT64 nNextDcsSegment = 0;

    UINT64 nMipsTotalCyc = 0;
    UINT64 nDcsTotalCyc = 0;

    // mips 100MHz ->  dcs 10MHz => 10:1
    const UINT64 mQuantum = kHz(10);
    const UINT64 dQuantum = kHz(1);

    nNextMipsSegment = mQuantum;
    nNextDcsSegment = dQuantum;

    Mips3SetIRQLine(VBLANK_IRQ, 0);

    bool isVblank = false;

    while ((nMipsTotalCyc < nMipsCycPerFrame) || (nDcsTotalCyc < nDcsCycPerFrame)) {
        if ((nNextMipsSegment + nMipsTotalCyc) > nMipsCycPerFrame) {
			nNextMipsSegment -= (nNextMipsSegment + nMipsTotalCyc) - nMipsCycPerFrame;
		}

        if ((nNextDcsSegment + nDcsTotalCyc) > nDcsCycPerFrame) {
			nNextDcsSegment -= (nNextDcsSegment + nDcsTotalCyc) - nDcsCycPerFrame;
		}

        if (!isVblank) {
            if ((nNextMipsSegment + nMipsTotalCyc) >= nMipsVblankCyc) {
                nNextMipsSegment -= (nNextMipsSegment + nMipsTotalCyc) - nMipsVblankCyc;
            }
            if (nMipsTotalCyc == nMipsVblankCyc) {
                isVblank = true;
                Mips3SetIRQLine(VBLANK_IRQ, 1);
                if (pBurnDraw) {
                    DrvDraw();
                }
            }
        }
		
        if (nNextMipsSegment) {
            Mips3Run(nNextMipsSegment);
            nMipsTotalCyc += nNextMipsSegment;
        }
		
        if (nNextDcsSegment) {
            if (!DrvDcsHeld) Dcs2kRun(nNextDcsSegment);
            nDcsTotalCyc += nNextDcsSegment;
        }

        nNextDcsSegment = dQuantum;
        nNextMipsSegment = mQuantum;
    }
	
    if (pBurnSoundOut) {
        Dcs2kRender(pBurnSoundOut, nBurnSoundLen);
    }

    return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) *pnMin = 0x029702;

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		Mips3Scan(nAction);
		Dcs2kScan(nAction, pnMin);
		DrvDisk->scan(nAction);

		SCAN_VAR(DrvVRAMBase);
		SCAN_VAR(nSoundData);
		SCAN_VAR(nSoundCtrl);
		SCAN_VAR(DrvDcsHeld);

		INT32 nBios = nBiosLoaded;
		SCAN_VAR(nBios);
		if (nAction & ACB_WRITE) LoadBootRom(nBios);

		if (nKinstVersion == KINST2UK) {
			SCAN_VAR(nProtSel);
			SCAN_VAR(nProtCnt);
			SCAN_VAR(nProtRegA);
			SCAN_VAR(nProtRegB);
		}
	}

	return 0;
}

static struct BurnHDDInfo kinstHDDDesc[] = {
	{ "kinst", "81d833236e994528d1482979261401b198d1ca53" }
};

STD_HDD_PICK(kinst)
STD_HDD_FN(kinst)

static struct BurnRomInfo kinstRomDesc[] = {
    { "ki-l15d.u98",		0x80000, 0x7b65ca3d, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT }, //  0 MIPS R4600 Code (v1.5d)
    { "ki-l14.u98",			0x80000, 0xafedb75f, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT | BRF_OPT }, //  1 MIPS R4600 Code (v1.4)
    { "ki-l13.u98",			0x80000, 0x65f7ea31, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT | BRF_OPT }, //  2 MIPS R4600 Code (v1.3)
    { "ki-p47.u98",			0x80000, 0x05e67bcb, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT | BRF_OPT }, //  3 MIPS R4600 Code (proto v4.7)
    { "ki_l15di.u98",		0x80000, 0x230f55fb, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT | BRF_OPT }, //  4 MIPS R4600 Code (v1.5d AnyIDE)

    { "u10-l1",				0x80000, 0xb6cc155f, 2 | BRF_SND | BRF_ESS }, //  5 DCS sound banks
    { "u11-l1",				0x80000, 0x0b5e05df, 2 | BRF_SND | BRF_ESS }, //  6
    { "u12-l1",				0x80000, 0xd05ce6ad, 2 | BRF_SND | BRF_ESS }, //  7
    { "u13-l1",				0x80000, 0x7d0954ea, 2 | BRF_SND | BRF_ESS }, //  8
    { "u33-l1",				0x80000, 0x8bbe4f0c, 2 | BRF_SND | BRF_ESS }, //  9
    { "u34-l1",				0x80000, 0xb2e73603, 2 | BRF_SND | BRF_ESS }, // 10
    { "u35-l1",				0x80000, 0x0aaef4fc, 2 | BRF_SND | BRF_ESS }, // 11
    { "u36-l1",				0x80000, 0x0577bb60, 2 | BRF_SND | BRF_ESS }, // 12
};

STD_ROM_PICK(kinst)
STD_ROM_FN(kinst)

struct BurnDriver BurnDrvKinst = {
    "kinst", NULL, NULL, NULL, "1994",
    "Killer Instinct\0", NULL, "Rare / Nintendo (Midway license)", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinstRomInfo, kinstRomName, kinstHDDInfo, kinstHDDName, NULL, NULL, kinstInputInfo, kinstDIPInfo,
    kinstDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};

static struct BurnHDDInfo kinst2HDDDesc[] = {
	{ "kinst2", "e7c9291b4648eae0012ea0cc230731ed4987d1d5" }
};

STD_HDD_PICK(kinst2)
STD_HDD_FN(kinst2)

static struct BurnRomInfo kinst2RomDesc[] = {
    { "ki2-l14.u98",		0x80000, 0x27d0285e, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT }, //  0 MIPS R4600 Code (v1.4)
    { "ki2-l13.u98",		0x80000, 0x25ebde3b, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT | BRF_OPT }, //  1 MIPS R4600 Code (v1.3)
    { "ki2-l11.u98",		0x80000, 0x0cb8de1e, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT | BRF_OPT }, //  2 MIPS R4600 Code (v1.1)
    { "ki2-l10.u98",		0x80000, 0xb17b4b3d, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT | BRF_OPT }, //  3 MIPS R4600 Code (v1.0)
    { "ki2_l14p.u98",		0x80000, 0xd80c937a, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT | BRF_OPT }, //  4 MIPS R4600 Code (v1.4 AnyIDE)

    { "ki2_l1.u10",			0x80000, 0xfdf6ed51, 2 | BRF_SND | BRF_ESS }, //  5 DCS sound banks
    { "ki2_l1.u11",			0x80000, 0xf9e70024, 2 | BRF_SND | BRF_ESS }, //  6
    { "ki2_l1.u12",			0x80000, 0x2994c199, 2 | BRF_SND | BRF_ESS }, //  7
    { "ki2_l1.u13",			0x80000, 0x3fe6327b, 2 | BRF_SND | BRF_ESS }, //  8
    { "ki2_l1.u33",			0x80000, 0x6f4dcdcf, 2 | BRF_SND | BRF_ESS }, //  9
    { "ki2_l1.u34",			0x80000, 0x5db48206, 2 | BRF_SND | BRF_ESS }, // 10
    { "ki2_l1.u35",			0x80000, 0x7245ce69, 2 | BRF_SND | BRF_ESS }, // 11
    { "ki2_l1.u36",			0x80000, 0x8920acbb, 2 | BRF_SND | BRF_ESS }, // 12
};

STD_ROM_PICK(kinst2)
STD_ROM_FN(kinst2)

struct BurnDriver BurnDrvKinst2 = {
    "kinst2", NULL, NULL, NULL, "1996",
    "Killer Instinct 2\0", NULL, "Rare / Nintendo (Midway license)", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinst2RomInfo, kinst2RomName, kinst2HDDInfo, kinst2HDDName, NULL, NULL, kinstInputInfo, kinst2DIPInfo,
    kinst2DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};

static struct BurnRomInfo kinst2ukRomDesc[] = {
    { "ki2-l14k.u98",		0x80000, 0x9cbd00a8, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT }, //  0 MIPS R4600 Code (v1.4k)
    { "ki2-l13k.u98",		0x80000, 0x3b4f16fc, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT | BRF_OPT }, //  1 MIPS R4600 Code (v1.3k)
    { "ki2_d14p.u98",		0x80000, 0xd716d428, BRF_ESS | BRF_PRG | BRF_BIOS | BRF_SELECT | BRF_OPT }, //  2 MIPS R4600 Code (v1.4k AnyIDE)

    { "ki2_l1.u10",			0x80000, 0xfdf6ed51, 2 | BRF_SND | BRF_ESS }, //  3 DCS sound banks
    { "ki2_l1.u11",			0x80000, 0xf9e70024, 2 | BRF_SND | BRF_ESS }, //  4
    { "ki2_l1.u12",			0x80000, 0x2994c199, 2 | BRF_SND | BRF_ESS }, //  5
    { "ki2_l1.u13",			0x80000, 0x3fe6327b, 2 | BRF_SND | BRF_ESS }, //  6
    { "ki2_l1.u33",			0x80000, 0x6f4dcdcf, 2 | BRF_SND | BRF_ESS }, //  7
    { "ki2_l1.u34",			0x80000, 0x5db48206, 2 | BRF_SND | BRF_ESS }, //  8
    { "ki2_l1.u35",			0x80000, 0x7245ce69, 2 | BRF_SND | BRF_ESS }, //  9
    { "ki2_l1.u36",			0x80000, 0x8920acbb, 2 | BRF_SND | BRF_ESS }, // 10
};

STD_ROM_PICK(kinst2uk)
STD_ROM_FN(kinst2uk)

struct BurnDriver BurnDrvKinst2uk = {
    "kinst2uk", "kinst2", NULL, NULL, "1996",
    "Killer Instinct 2 (upgrade kit)\0", NULL, "Rare / Nintendo (Midway license)", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinst2ukRomInfo, kinst2ukRomName, kinst2HDDInfo, kinst2HDDName, NULL, NULL, kinstInputInfo, kinst2ukDIPInfo,
    kinst2ukDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};
