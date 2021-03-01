// Based on MAME driver by Aaron Giles

// coin up doesn't work - forcing freeplay dip for now
// the hdd image get corrupted over time
// the games are also known for crashing, freezing, and having visual glitchs

#include "tiles_generic.h"
#include "mips3_intf.h"
#include "ide.h"
#include "dcs2k.h"

#define IDE_IRQ     1
#define VBLANK_IRQ  0

#define KINST   0x100000
#define KINST2  0x200000

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
static UINT8 DrvDSW[2];
static UINT32 DrvVRAMBase;
static UINT32 DrvInputs[3];
static UINT32 nSoundData;
static UINT32 nSoundCtrl;
static ide::ide_disk *DrvDisk;
static UINT8 DrvReset = 0;

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
	{ "Volume Down",	BIT_DIGITAL,	DrvJoy3 + 2,	"p1 fire 7" },
	{ "Volume Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 fire 8" },
	{ "Dip A",			BIT_DIPSWITCH,	DrvDSW + 0,		"dip"       },
	{ "Dip B",			BIT_DIPSWITCH,	DrvDSW + 1,		"dip"       },
};

STDINPUTINFO(kinst)

static struct BurnDIPInfo kinstDIPList[] =
{
	{0x20, 0xff, 0xff, 0xff, NULL                },
	{0x21, 0xff, 0xff, 0xff, NULL                },
	
	{0   , 0xfe, 0   ,    4, "Blood Level"       },
	{0x20, 0x01, 0x03, 0x03, "High"              },
	{0x20, 0x01, 0x03, 0x02, "Medium"            },
	{0x20, 0x01, 0x03, 0x01, "Low"               },
	{0x20, 0x01, 0x03, 0x00, "None"              },
	
	{0   , 0xfe, 0   ,    2, "Demo Sounds"       },
	{0x20, 0x01, 0x04, 0x00, "Off"               },
	{0x20, 0x01, 0x04, 0x04, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Finishing Moves"   },
	{0x20, 0x01, 0x08, 0x00, "Off"               },
	{0x20, 0x01, 0x08, 0x08, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Display Warning"   },
	{0x20, 0x01, 0x10, 0x00, "Off"               },
	{0x20, 0x01, 0x10, 0x10, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Blood"             },
	{0x20, 0x01, 0x20, 0x20, "Red"               },
	{0x20, 0x01, 0x20, 0x00, "White"             },
	
	{0   , 0xfe, 0   ,    2, "Coinage Source"    },
	{0x21, 0x01, 0x01, 0x01, "Dip Switch"        },
	{0x21, 0x01, 0x01, 0x00, "Disk"              },
	
	{0   , 0xfe, 0   ,   20, "Coinage"           },
	{0x21, 0x01, 0x3e, 0x3e, "USA-1"             },
	{0x21, 0x01, 0x3e, 0x3c, "USA-2"             },
	{0x21, 0x01, 0x3e, 0x3a, "USA-3"             },
	{0x21, 0x01, 0x3e, 0x38, "USA-4"             },
	{0x21, 0x01, 0x3e, 0x34, "USA-9"             },
	{0x21, 0x01, 0x3e, 0x32, "USA-10"            },
	{0x21, 0x01, 0x3e, 0x36, "USA-ECA"           },
	{0x21, 0x01, 0x3e, 0x30, "USA-Free Play"     },
	{0x21, 0x01, 0x3e, 0x2e, "German-1"          },
	{0x21, 0x01, 0x3e, 0x2c, "German-2"          },
	{0x21, 0x01, 0x3e, 0x2a, "German-3"          },
	{0x21, 0x01, 0x3e, 0x28, "German-4"          },
	{0x21, 0x01, 0x3e, 0x26, "German-ECA"        },
	{0x21, 0x01, 0x3e, 0x20, "German-Free Play"  },
	{0x21, 0x01, 0x3e, 0x1e, "French-1"          },
	{0x21, 0x01, 0x3e, 0x1c, "French-2"          },
	{0x21, 0x01, 0x3e, 0x1a, "French-3"          },
	{0x21, 0x01, 0x3e, 0x18, "French-4"          },
	{0x21, 0x01, 0x3e, 0x16, "French-ECA"        },
	{0x21, 0x01, 0x3e, 0x10, "French-Free Play"  },
	
	{0   , 0xfe, 0   ,    2, "Coin Counters"     },
	{0x21, 0x01, 0x40, 0x40, "1"                 },
	{0x21, 0x01, 0x40, 0x00, "2"                 },
	
	{0   , 0xfe, 0   ,    2, "Test Switch"       },
	{0x21, 0x01, 0x80, 0x80, "Off"               },
	{0x21, 0x01, 0x80, 0x00, "On"                },
};

STDDIPINFO(kinst)

static struct BurnDIPInfo kinst2DIPList[] =
{
	{0x20, 0xff, 0xff, 0xff, NULL                },
	{0x21, 0xff, 0xff, 0xff, NULL                },
	
	{0   , 0xfe, 0   ,    4, "Blood Level"       },
	{0x20, 0x01, 0x03, 0x03, "High"              },
	{0x20, 0x01, 0x03, 0x02, "Medium"            },
	{0x20, 0x01, 0x03, 0x01, "Low"               },
	{0x20, 0x01, 0x03, 0x00, "None"              },
	
	{0   , 0xfe, 0   ,    2, "Demo Sounds"       },
	{0x20, 0x01, 0x04, 0x00, "Off"               },
	{0x20, 0x01, 0x04, 0x04, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Finishing Moves"   },
	{0x20, 0x01, 0x08, 0x00, "Off"               },
	{0x20, 0x01, 0x08, 0x08, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Display Warning"   },
	{0x20, 0x01, 0x10, 0x00, "Off"               },
	{0x20, 0x01, 0x10, 0x10, "On"                },
	
	{0   , 0xfe, 0   ,    2, "Coinage Source"    },
	{0x21, 0x01, 0x01, 0x01, "Dip Switch"        },
	{0x21, 0x01, 0x01, 0x00, "Disk"              },
	
	{0   , 0xfe, 0   ,   20, "Coinage"           },
	{0x21, 0x01, 0x3e, 0x3e, "USA-1"             },
	{0x21, 0x01, 0x3e, 0x3c, "USA-2"             },
	{0x21, 0x01, 0x3e, 0x3a, "USA-3"             },
	{0x21, 0x01, 0x3e, 0x38, "USA-4"             },
	{0x21, 0x01, 0x3e, 0x34, "USA-9"             },
	{0x21, 0x01, 0x3e, 0x32, "USA-10"            },
	{0x21, 0x01, 0x3e, 0x36, "USA-ECA"           },
	{0x21, 0x01, 0x3e, 0x30, "USA-Free Play"     },
	{0x21, 0x01, 0x3e, 0x2e, "German-1"          },
	{0x21, 0x01, 0x3e, 0x2c, "German-2"          },
	{0x21, 0x01, 0x3e, 0x2a, "German-3"          },
	{0x21, 0x01, 0x3e, 0x28, "German-4"          },
	{0x21, 0x01, 0x3e, 0x26, "German-ECA"        },
	{0x21, 0x01, 0x3e, 0x20, "German-Free Play"  },
	{0x21, 0x01, 0x3e, 0x1e, "French-1"          },
	{0x21, 0x01, 0x3e, 0x1c, "French-2"          },
	{0x21, 0x01, 0x3e, 0x1a, "French-3"          },
	{0x21, 0x01, 0x3e, 0x18, "French-4"          },
	{0x21, 0x01, 0x3e, 0x16, "French-ECA"        },
	{0x21, 0x01, 0x3e, 0x10, "French-Free Play"  },
	
	{0   , 0xfe, 0   ,    2, "Coin Counters"     },
	{0x21, 0x01, 0x40, 0x40, "1"                 },
	{0x21, 0x01, 0x40, 0x00, "2"                 },
	
	{0   , 0xfe, 0   ,    2, "Test Switch"       },
	{0x21, 0x01, 0x80, 0x80, "Off"               },
	{0x21, 0x01, 0x80, 0x00, "On"                },
};

STDDIPINFO(kinst2)

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

static void DrvDoReset()
{
	Mips3Reset();
	DrvDisk->reset();
	
	DrvRecalc = 1;
	nSoundData = 0;
	nSoundCtrl = 0;
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
				tmp &= ~0x3e00; // coins don't work - force free play option
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
				tmp &= ~0x3e00; // coins don't work - force free play option
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
    if (BurnLoadRom(DrvSoundROM + 0x000000, 1, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0x200000, 2, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0x400000, 3, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0x600000, 4, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0x800000, 5, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0xA00000, 6, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0xC00000, 7, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0xE00000, 8, 2)) return 1;
    return 0;
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

static INT32 DrvInit(INT32 version)
{
    MemIndex();
    INT32 nLen = MemEnd - (UINT8 *)0;

    if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;

    DrvDisk = new ide::ide_disk();
    DrvDisk->set_irq_callback(IDESetIRQState);

    MemIndex();

    UINT32 nRet = BurnLoadRom(DrvBootROM, 0, 0);
    if (nRet != 0) return 1;

    nRet = LoadSoundBanks();
    if (nRet != 0) return 1;
	
	nRet = DrvDisk->load_hdd_image(0);
	if (nRet != 0) return 1;

    Dcs2kInit(DCS_2K, MHz(10));

#ifdef MIPS3_X64_DRC
    Mips3UseRecompiler(true);
#endif
    Mips3Init();
    
    DrvVRAMBase = 0x30000;

    Mips3MapMemory(DrvBootROM,  0x1FC00000, 0x1FC7FFFF, MAP_READ);
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
    }

    Dcs2kMapSoundROM(DrvSoundROM, 0x1000000);
    Dcs2kBoot();
	
	GenericTilesInit();
	
	DrvDoReset();

    return 0;
}

static INT32 kinstDrvInit() { return DrvInit(KINST); }
static INT32 kinst2DrvInit() { return DrvInit(KINST2); }

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
            *dst = *src & 0x7FFF;
            dst++;
            src++;
        }
    }
	
	BurnTransferCopy(DrvColorLUT);
	
    return 0;
}

// R4600: 100 MHz
// VIDEO:  60  Hz
// VBLANK: 20 kHz, 50us (from MAME))
//                 50us = 20kHz
//
static INT32 DrvFrame()
{
    if (DrvReset) DrvDoReset();
	
	if (DrvRecalc) {
		GenerateColorLUT();
		DrvRecalc = 0;
	}
	
	MakeInputs();

    const UINT64 FPS = 60;
    const UINT64 nMipsCycPerFrame = MHz(100) / FPS;
    const UINT64 nMipsVblankCyc = nMipsCycPerFrame - kHz(20);
    const UINT64 nDcsCycPerFrame = MHz(10) / FPS;

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
	bool dcsIrq = false;

    while ((nMipsTotalCyc < nMipsCycPerFrame) || (nDcsTotalCyc < nDcsCycPerFrame)) {
		// @ 60hz, dcs needs 2 IRQs/frame
		if (nDcsTotalCyc == 0) DcsCheckIRQ();
		
		if (nDcsTotalCyc >= (nDcsCycPerFrame / 2) && !dcsIrq) {
			DcsCheckIRQ();
			dcsIrq = true;
		}

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
				DcsCheckIRQ();
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
            Dcs2kRun(nNextDcsSegment);
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
    return 0;
}

static struct BurnHDDInfo kinstHDDDesc[] = {
	{ "kinst.img",		0x7d01200, 0x2b9b6c0d }
};

STD_HDD_PICK(kinst)
STD_HDD_FN(kinst)

static struct BurnRomInfo kinstRomDesc[] = {
    { "ki-l15d.u98",		0x80000, 0x7b65ca3d, 1 | BRF_PRG | BRF_ESS }, //  0 MIPS R4600 Code

    { "u10-l1",             0x80000, 0xb6cc155f, 2 | BRF_SND | BRF_ESS }, //  1 DCS sound banks
    { "u11-l1",             0x80000, 0x0b5e05df, 2 | BRF_SND | BRF_ESS }, //  2
    { "u12-l1",             0x80000, 0xd05ce6ad, 2 | BRF_SND | BRF_ESS }, //  3
    { "u13-l1",             0x80000, 0x7d0954ea, 2 | BRF_SND | BRF_ESS }, //  4
    { "u33-l1",             0x80000, 0x8bbe4f0c, 2 | BRF_SND | BRF_ESS }, //  5
    { "u34-l1",             0x80000, 0xb2e73603, 2 | BRF_SND | BRF_ESS }, //  6
    { "u35-l1",             0x80000, 0x0aaef4fc, 2 | BRF_SND | BRF_ESS }, //  7
    { "u36-l1",             0x80000, 0x0577bb60, 2 | BRF_SND | BRF_ESS }, //  8

#if defined ROM_VERIFY
	{ "ki-p47.u98",			0x80000, 0x05e67bcb, 0 | BRF_OPT },
	{ "ki_l15di.u98",		0x80000, 0x230f55fb, 0 | BRF_OPT },
	{ "ki-l13.u98",			0x80000, 0x65f7ea31, 0 | BRF_OPT },
	{ "ki-l14.u98",			0x80000, 0xafedb75f, 0 | BRF_OPT },
#endif
};

STD_ROM_PICK(kinst)
STD_ROM_FN(kinst)

struct BurnDriver BurnDrvKinst = {
    "kinst", NULL, NULL, NULL, "1994",
    "Killer Instinct (ROM ver. 1.5d)\0", "Works best in 64-bit build", "Rare/Nintendo", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_NOT_WORKING, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinstRomInfo, kinstRomName, kinstHDDInfo, kinstHDDName, NULL, NULL, kinstInputInfo, kinstDIPInfo,
    kinstDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};

static struct BurnRomInfo kinst14RomDesc[] = {
    { "ki-l14.u98",			0x80000, 0xafedb75f, 1 | BRF_PRG | BRF_ESS }, //  0 MIPS R4600 Code

    { "u10-l1",             0x80000, 0xb6cc155f, 2 | BRF_SND | BRF_ESS }, //  1 DCS sound banks
    { "u11-l1",             0x80000, 0x0b5e05df, 2 | BRF_SND | BRF_ESS }, //  2
    { "u12-l1",             0x80000, 0xd05ce6ad, 2 | BRF_SND | BRF_ESS }, //  3
    { "u13-l1",             0x80000, 0x7d0954ea, 2 | BRF_SND | BRF_ESS }, //  4
    { "u33-l1",             0x80000, 0x8bbe4f0c, 2 | BRF_SND | BRF_ESS }, //  5
    { "u34-l1",             0x80000, 0xb2e73603, 2 | BRF_SND | BRF_ESS }, //  6
    { "u35-l1",             0x80000, 0x0aaef4fc, 2 | BRF_SND | BRF_ESS }, //  7
    { "u36-l1",             0x80000, 0x0577bb60, 2 | BRF_SND | BRF_ESS }, //  8
};

STD_ROM_PICK(kinst14)
STD_ROM_FN(kinst14)

struct BurnDriver BurnDrvKinst14 = {
    "kinst14", "kinst", NULL, NULL, "1994",
    "Killer Instinct (ROM ver. 1.4)\0", "Works best in 64-bit build", "Rare/Nintendo", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinst14RomInfo, kinst14RomName, kinstHDDInfo, kinstHDDName, NULL, NULL, kinstInputInfo, kinstDIPInfo,
    kinstDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};

static struct BurnRomInfo kinst13RomDesc[] = {
    { "ki-l13.u98",			0x80000, 0x65f7ea31, 1 | BRF_PRG | BRF_ESS }, //  0 MIPS R4600 Code

    { "u10-l1",             0x80000, 0xb6cc155f, 2 | BRF_SND | BRF_ESS }, //  1 DCS sound banks
    { "u11-l1",             0x80000, 0x0b5e05df, 2 | BRF_SND | BRF_ESS }, //  2
    { "u12-l1",             0x80000, 0xd05ce6ad, 2 | BRF_SND | BRF_ESS }, //  3
    { "u13-l1",             0x80000, 0x7d0954ea, 2 | BRF_SND | BRF_ESS }, //  4
    { "u33-l1",             0x80000, 0x8bbe4f0c, 2 | BRF_SND | BRF_ESS }, //  5
    { "u34-l1",             0x80000, 0xb2e73603, 2 | BRF_SND | BRF_ESS }, //  6
    { "u35-l1",             0x80000, 0x0aaef4fc, 2 | BRF_SND | BRF_ESS }, //  7
    { "u36-l1",             0x80000, 0x0577bb60, 2 | BRF_SND | BRF_ESS }, //  8
};

STD_ROM_PICK(kinst13)
STD_ROM_FN(kinst13)

struct BurnDriver BurnDrvKinst13 = {
    "kinst13", "kinst", NULL, NULL, "1994",
    "Killer Instinct (ROM ver. 1.3)\0", "Works best in 64-bit build", "Rare/Nintendo", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinst13RomInfo, kinst13RomName, kinstHDDInfo, kinstHDDName, NULL, NULL, kinstInputInfo, kinstDIPInfo,
    kinstDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};

static struct BurnRomInfo kinstp47RomDesc[] = {
    { "ki-p47.u98",			0x80000, 0x05e67bcb, 1 | BRF_PRG | BRF_ESS }, //  0 MIPS R4600 Code

    { "u10-l1",             0x80000, 0xb6cc155f, 2 | BRF_SND | BRF_ESS }, //  1 DCS sound banks
    { "u11-l1",             0x80000, 0x0b5e05df, 2 | BRF_SND | BRF_ESS }, //  2
    { "u12-l1",             0x80000, 0xd05ce6ad, 2 | BRF_SND | BRF_ESS }, //  3
    { "u13-l1",             0x80000, 0x7d0954ea, 2 | BRF_SND | BRF_ESS }, //  4
    { "u33-l1",             0x80000, 0x8bbe4f0c, 2 | BRF_SND | BRF_ESS }, //  5
    { "u34-l1",             0x80000, 0xb2e73603, 2 | BRF_SND | BRF_ESS }, //  6
    { "u35-l1",             0x80000, 0x0aaef4fc, 2 | BRF_SND | BRF_ESS }, //  7
    { "u36-l1",             0x80000, 0x0577bb60, 2 | BRF_SND | BRF_ESS }, //  8
};

STD_ROM_PICK(kinstp47)
STD_ROM_FN(kinstp47)

struct BurnDriverD BurnDrvKinstp47 = {
    "kinstp47", "kinst", NULL, NULL, "1994",
    "Killer Instinct (ROM proto ver. 4.7)\0", "Works best in 64-bit build", "Rare/Nintendo", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinstp47RomInfo, kinstp47RomName, kinstHDDInfo, kinstHDDName, NULL, NULL, kinstInputInfo, kinstDIPInfo,
    kinstDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};

static struct BurnRomInfo kinst15aiRomDesc[] = {
    { "ki_l15di.u98",		0x80000, 0x230f55fb, 1 | BRF_PRG | BRF_ESS }, //  0 MIPS R4600 Code

    { "u10-l1",             0x80000, 0xb6cc155f, 2 | BRF_SND | BRF_ESS }, //  1 DCS sound banks
    { "u11-l1",             0x80000, 0x0b5e05df, 2 | BRF_SND | BRF_ESS }, //  2
    { "u12-l1",             0x80000, 0xd05ce6ad, 2 | BRF_SND | BRF_ESS }, //  3
    { "u13-l1",             0x80000, 0x7d0954ea, 2 | BRF_SND | BRF_ESS }, //  4
    { "u33-l1",             0x80000, 0x8bbe4f0c, 2 | BRF_SND | BRF_ESS }, //  5
    { "u34-l1",             0x80000, 0xb2e73603, 2 | BRF_SND | BRF_ESS }, //  6
    { "u35-l1",             0x80000, 0x0aaef4fc, 2 | BRF_SND | BRF_ESS }, //  7
    { "u36-l1",             0x80000, 0x0577bb60, 2 | BRF_SND | BRF_ESS }, //  8
};

STD_ROM_PICK(kinst15ai)
STD_ROM_FN(kinst15ai)

struct BurnDriver BurnDrvKinst15ai = {
    "kinst15ai", "kinst", NULL, NULL, "1994",
    "Killer Instinct (ROM ver. 1.5 AnyIDE)\0", "Works best in 64-bit build", "Rare/Nintendo", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_NOT_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinst15aiRomInfo, kinst15aiRomName, kinstHDDInfo, kinstHDDName, NULL, NULL, kinstInputInfo, kinstDIPInfo,
    kinstDrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};

static struct BurnHDDInfo kinst2HDDDesc[] = {
	{ "kinst2.img",		0x1b478a00, 0x63bc7789 }
};

STD_HDD_PICK(kinst2)
STD_HDD_FN(kinst2)

static struct BurnRomInfo kinst2RomDesc[] = {
    { "ki2-l14.u98",		0x80000, 0x27d0285e, 1 | BRF_PRG | BRF_ESS }, //  0 MIPS R4600 Code
	
    { "ki2_l1.u10",			0x80000, 0xfdf6ed51, 2 | BRF_SND | BRF_ESS }, //  1 DCS sound banks
    { "ki2_l1.u11",			0x80000, 0xf9e70024, 2 | BRF_SND | BRF_ESS }, //  2
    { "ki2_l1.u12",			0x80000, 0x2994c199, 2 | BRF_SND | BRF_ESS }, //  3
    { "ki2_l1.u13",			0x80000, 0x3fe6327b, 2 | BRF_SND | BRF_ESS }, //  4
    { "ki2_l1.u33",			0x80000, 0x6f4dcdcf, 2 | BRF_SND | BRF_ESS }, //  5
    { "ki2_l1.u34",			0x80000, 0x5db48206, 2 | BRF_SND | BRF_ESS }, //  6
    { "ki2_l1.u35",			0x80000, 0x7245ce69, 2 | BRF_SND | BRF_ESS }, //  7
    { "ki2_l1.u36",			0x80000, 0x8920acbb, 2 | BRF_SND | BRF_ESS }, //  8
	
#if defined ROM_VERIFY
	{ "ki2-l11.u98",		0x80000, 0x0cb8de1e, 0 | BRF_OPT },
	{ "ki2-l13.u98",		0x80000, 0x25ebde3b, 0 | BRF_OPT },
	{ "ki2-l10.u98",		0x80000, 0xb17b4b3d, 0 | BRF_OPT },
	{ "ki2_l14p.u98",		0x80000, 0xd80c937a, 0 | BRF_OPT },
#endif
};

STD_ROM_PICK(kinst2)
STD_ROM_FN(kinst2)

struct BurnDriver BurnDrvKinst2 = {
    "kinst2", NULL, NULL, NULL, "1995",
    "Killer Instinct II (ROM ver. 1.4)\0", "Works best in 64-bit build", "Rare/Nintendo", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_NOT_WORKING, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinst2RomInfo, kinst2RomName, kinst2HDDInfo, kinst2HDDName, NULL, NULL, kinstInputInfo, kinst2DIPInfo,
    kinst2DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};

static struct BurnRomInfo kinst213RomDesc[] = {
    { "ki2-l13.u98",		0x80000, 0x25ebde3b, 1 | BRF_PRG | BRF_ESS }, //  0 MIPS R4600 Code
	
    { "ki2_l1.u10",			0x80000, 0xfdf6ed51, 2 | BRF_SND | BRF_ESS }, //  1 DCS sound banks
    { "ki2_l1.u11",			0x80000, 0xf9e70024, 2 | BRF_SND | BRF_ESS }, //  2
    { "ki2_l1.u12",			0x80000, 0x2994c199, 2 | BRF_SND | BRF_ESS }, //  3
    { "ki2_l1.u13",			0x80000, 0x3fe6327b, 2 | BRF_SND | BRF_ESS }, //  4
    { "ki2_l1.u33",			0x80000, 0x6f4dcdcf, 2 | BRF_SND | BRF_ESS }, //  5
    { "ki2_l1.u34",			0x80000, 0x5db48206, 2 | BRF_SND | BRF_ESS }, //  6
    { "ki2_l1.u35",			0x80000, 0x7245ce69, 2 | BRF_SND | BRF_ESS }, //  7
    { "ki2_l1.u36",			0x80000, 0x8920acbb, 2 | BRF_SND | BRF_ESS }, //  8
};

STD_ROM_PICK(kinst213)
STD_ROM_FN(kinst213)

struct BurnDriver BurnDrvKinst213 = {
    "kinst213", "kinst2", NULL, NULL, "1995",
    "Killer Instinct II (ROM ver. 1.3)\0", "Works best in 64-bit build", "Rare/Nintendo", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinst213RomInfo, kinst213RomName, kinst2HDDInfo, kinst2HDDName, NULL, NULL, kinstInputInfo, kinst2DIPInfo,
    kinst2DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};

static struct BurnRomInfo kinst211RomDesc[] = {
    { "ki2-l11.u98",		0x80000, 0x0cb8de1e, 1 | BRF_PRG | BRF_ESS }, //  0 MIPS R4600 Code
	
    { "ki2_l1.u10",			0x80000, 0xfdf6ed51, 2 | BRF_SND | BRF_ESS }, //  1 DCS sound banks
    { "ki2_l1.u11",			0x80000, 0xf9e70024, 2 | BRF_SND | BRF_ESS }, //  2
    { "ki2_l1.u12",			0x80000, 0x2994c199, 2 | BRF_SND | BRF_ESS }, //  3
    { "ki2_l1.u13",			0x80000, 0x3fe6327b, 2 | BRF_SND | BRF_ESS }, //  4
    { "ki2_l1.u33",			0x80000, 0x6f4dcdcf, 2 | BRF_SND | BRF_ESS }, //  5
    { "ki2_l1.u34",			0x80000, 0x5db48206, 2 | BRF_SND | BRF_ESS }, //  6
    { "ki2_l1.u35",			0x80000, 0x7245ce69, 2 | BRF_SND | BRF_ESS }, //  7
    { "ki2_l1.u36",			0x80000, 0x8920acbb, 2 | BRF_SND | BRF_ESS }, //  8
};

STD_ROM_PICK(kinst211)
STD_ROM_FN(kinst211)

struct BurnDriver BurnDrvKinst211 = {
    "kinst211", "kinst2", NULL, NULL, "1995",
    "Killer Instinct II (ROM ver. 1.1)\0", "Works best in 64-bit build", "Rare/Nintendo", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinst211RomInfo, kinst211RomName, kinst2HDDInfo, kinst2HDDName, NULL, NULL, kinstInputInfo, kinst2DIPInfo,
    kinst2DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};

static struct BurnRomInfo kinst210RomDesc[] = {
    { "ki2-l10.u98",		0x80000, 0xb17b4b3d, 1 | BRF_PRG | BRF_ESS }, //  0 MIPS R4600 Code
	
    { "ki2_l1.u10",			0x80000, 0xfdf6ed51, 2 | BRF_SND | BRF_ESS }, //  1 DCS sound banks
    { "ki2_l1.u11",			0x80000, 0xf9e70024, 2 | BRF_SND | BRF_ESS }, //  2
    { "ki2_l1.u12",			0x80000, 0x2994c199, 2 | BRF_SND | BRF_ESS }, //  3
    { "ki2_l1.u13",			0x80000, 0x3fe6327b, 2 | BRF_SND | BRF_ESS }, //  4
    { "ki2_l1.u33",			0x80000, 0x6f4dcdcf, 2 | BRF_SND | BRF_ESS }, //  5
    { "ki2_l1.u34",			0x80000, 0x5db48206, 2 | BRF_SND | BRF_ESS }, //  6
    { "ki2_l1.u35",			0x80000, 0x7245ce69, 2 | BRF_SND | BRF_ESS }, //  7
    { "ki2_l1.u36",			0x80000, 0x8920acbb, 2 | BRF_SND | BRF_ESS }, //  8
};

STD_ROM_PICK(kinst210)
STD_ROM_FN(kinst210)

struct BurnDriver BurnDrvKinst210 = {
    "kinst210", "kinst2", NULL, NULL, "1995",
    "Killer Instinct II (ROM ver. 1.0)\0", "Works best in 64-bit build", "Rare/Nintendo", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinst210RomInfo, kinst210RomName, kinst2HDDInfo, kinst2HDDName, NULL, NULL, kinstInputInfo, kinst2DIPInfo,
    kinst2DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};

static struct BurnRomInfo kinst214aiRomDesc[] = {
    { "ki2_l14p.u98",		0x80000, 0xd80c937a, 1 | BRF_PRG | BRF_ESS }, //  0 MIPS R4600 Code
	
    { "ki2_l1.u10",			0x80000, 0xfdf6ed51, 2 | BRF_SND | BRF_ESS }, //  1 DCS sound banks
    { "ki2_l1.u11",			0x80000, 0xf9e70024, 2 | BRF_SND | BRF_ESS }, //  2
    { "ki2_l1.u12",			0x80000, 0x2994c199, 2 | BRF_SND | BRF_ESS }, //  3
    { "ki2_l1.u13",			0x80000, 0x3fe6327b, 2 | BRF_SND | BRF_ESS }, //  4
    { "ki2_l1.u33",			0x80000, 0x6f4dcdcf, 2 | BRF_SND | BRF_ESS }, //  5
    { "ki2_l1.u34",			0x80000, 0x5db48206, 2 | BRF_SND | BRF_ESS }, //  6
    { "ki2_l1.u35",			0x80000, 0x7245ce69, 2 | BRF_SND | BRF_ESS }, //  7
    { "ki2_l1.u36",			0x80000, 0x8920acbb, 2 | BRF_SND | BRF_ESS }, //  8
};

STD_ROM_PICK(kinst214ai)
STD_ROM_FN(kinst214ai)

struct BurnDriver BurnDrvKinst214ai = {
    "kinst214ai", "kinst2", NULL, NULL, "1995",
    "Killer Instinct II (ROM ver. 1.4 AnyIDE)\0", "Works best in 64-bit build", "Rare/Nintendo", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MIDWAY_KINST, GBF_VSFIGHT, 0,
    NULL, kinst214aiRomInfo, kinst214aiRomName, kinst2HDDInfo, kinst2HDDName, NULL, NULL, kinstInputInfo, kinst2DIPInfo,
    kinst2DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};
