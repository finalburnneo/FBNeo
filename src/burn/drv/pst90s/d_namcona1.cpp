// FinalBurn Neo Namco System NA-1/NA-2 driver module
// Based on MAME (0.103) driver by Phil Stroffolino

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m377_intf.h"
#include "c140.h"
#include "burn_pal.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvNVRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvMcuRAM;
static UINT16 *mcu_mailbox;
static UINT8 *DrvPaletteRAM;
static UINT8 *DrvGfxRAM;
static UINT8 *DrvVideoRAM;
static UINT8 *DrvScrollRAM;
static UINT8 *DrvSpriteRAM;
static UINT8 *DrvShapeRAM;
static UINT16 *DrvVRegs;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles[2];

static INT32 interrupt_enable;

static INT32 namcona1_gametype = 0;
static INT32 (*keycus_callback)(INT32 offset) = NULL;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvJoy5[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static UINT8 port4_data;
static UINT8 port5_data;
static UINT8 port6_data;
static UINT8 port7_data;
static UINT8 port8_data;

static UINT16 last_rand;
static UINT8 tinklpit_key;

static struct BurnInputInfo Namcona1InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p3 fire 3"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy4 + 3,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy4 + 2,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy4 + 1,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 0,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p4 fire 3"	},

	{"Service",			BIT_DIGITAL,	DrvJoy5 + 7,	"service"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Namcona1)

static struct BurnInputInfo Namcona1_quizInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy5 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy5 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 4"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy5 + 3,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p3 start"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 2,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 1,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy3 + 0,	"p3 fire 4"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy5 + 2,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p4 start"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 2,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 1,	"p4 fire 3"	},
	{"P4 Button 4",		BIT_DIGITAL,	DrvJoy4 + 0,	"p4 fire 4"	},

	{"Service",			BIT_DIGITAL,	DrvJoy5 + 7,	"service"	},
	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Namcona1_quiz)

static struct BurnDIPInfo Namcona1DIPList[]=
{
	DIP_OFFSET(0x26)
	{0x00, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "DIP2 (Freeze)"		},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "DIP1 (Test)"			},
	{0x00, 0x01, 0x02, 0x02, "Off"					},
	{0x00, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "SERVICE Mode"			},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},
};

STDDIPINFO(Namcona1)

static struct BurnDIPInfo Namcona1_quizDIPList[]=
{
	DIP_OFFSET(0x1a)
	{0x00, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    2, "DIP2 (Freeze)"		},
	{0x00, 0x01, 0x01, 0x01, "Off"					},
	{0x00, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "DIP1 (Test)"			},
	{0x00, 0x01, 0x02, 0x02, "Off"					},
	{0x00, 0x01, 0x02, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "SERVICE Mode"			},
	{0x00, 0x01, 0x40, 0x40, "Off"					},
	{0x00, 0x01, 0x40, 0x00, "On"					},
};

STDDIPINFO(Namcona1_quiz)

static void blit_setup(INT32 format, INT32 *bytes_per_row, INT32 *pitch, INT32 mode)
{
	if (mode == 3)
	{
		switch (format)
		{
			case 0x0001:
				*bytes_per_row = 0x1000;
				*pitch = 0x1000;
			break;

			case 0x0081:
				*bytes_per_row = 0x20;
				*pitch = 36*8;
			break;

			default:
				*bytes_per_row = (64 - (format>>2))*0x08;
				*pitch = 0x200;
			break;
		}
	}
	else
	{
		switch (format)
		{
			case 0x0000: /* Numan (used to clear spriteram) */
				*bytes_per_row = 16;
				*pitch = 0;
			break;

			case 0x0001:
				*bytes_per_row = 0x1000;
				*pitch = 0x1000;
			break;

			case 0x008d: /* Numan Athletics */
				*bytes_per_row = 8;
				*pitch = 0x120;
			break;

			case 0x00bd: /* Numan Athletics */
				*bytes_per_row = 4;
				*pitch = 0x120;
			break;

			case 0x0401: /* F/A */
				*bytes_per_row = 0x100;
				*pitch = 36*0x40;
			break;

			default:
				*bytes_per_row = (64 - (format>>5))*0x40;
				*pitch = 0x1000;
			break;
		}
	}
}

static void blit()
{
	UINT16 *namcona1_vreg = DrvVRegs;

	int src1 = BURN_ENDIAN_SWAP_INT16(namcona1_vreg[0x1]);
	int dst1 = BURN_ENDIAN_SWAP_INT16(namcona1_vreg[0x4]);

	int gfxbank = BURN_ENDIAN_SWAP_INT16(namcona1_vreg[0x6]);

	UINT32 src_baseaddr	= 2*((BURN_ENDIAN_SWAP_INT16(namcona1_vreg[0x7])<<16)|BURN_ENDIAN_SWAP_INT16(namcona1_vreg[0x8]));
	UINT32 dst_baseaddr	= 2*((BURN_ENDIAN_SWAP_INT16(namcona1_vreg[0x9])<<16)|BURN_ENDIAN_SWAP_INT16(namcona1_vreg[0xa]));

	int num_bytes = BURN_ENDIAN_SWAP_INT16(namcona1_vreg[0xb]);

	int dest_offset, source_offset;
	int dest_bytes_per_row, dst_pitch;
	int source_bytes_per_row, src_pitch;

	blit_setup( dst1, &dest_bytes_per_row, &dst_pitch, gfxbank);
	blit_setup( src1, &source_bytes_per_row, &src_pitch, gfxbank );

	if (num_bytes & 1) num_bytes++;
	if (dst_baseaddr < 0xf00000) dst_baseaddr += 0xf40000;

	if (dst_baseaddr >= 0x1e00000 && dst_baseaddr < 0x1e04000) {
		dst_baseaddr = 0xf00000 | (dst_baseaddr & 0x3fff); // palette mirror
	}

	dest_offset		= 0;
	source_offset	= 0;

	while (num_bytes > 0)
	{
		SekWriteWord(dst_baseaddr + dest_offset, SekReadWord(src_baseaddr + source_offset));

		num_bytes -= 2;
		dest_offset+=2;

		if( dest_offset >= dest_bytes_per_row )
		{
			dst_baseaddr += dst_pitch;
			dest_offset = 0;
		}

		source_offset+=2;

		if( source_offset >= source_bytes_per_row )
		{
			src_baseaddr += src_pitch;
			source_offset = 0;
		}
	}
}

static void bankswitch()
{
	INT32 bank = BURN_ENDIAN_SWAP_INT16(DrvVRegs[0x0c/2]);

	SekMapHandler(0,					0xf40000, 0xf7ffff, MAP_RAM);

	if (bank == 3) {
		SekMapMemory(DrvShapeRAM,		0xf40000, 0xf47fff, MAP_RAM);
	} else if (bank == 2) {
		SekMapMemory(DrvGfxRAM,			0xf40000, 0xf7ffff, MAP_RAM);
	}
}

static void sync_mcu()
{
	INT32 cyc = (SekTotalCycles() / 2) - M377TotalCycles();
	if (cyc > 0) {
		M377Run(cyc);
	}
}

static void __fastcall namcona1_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff000) == 0xe00000) {
		DrvNVRAM[(address / 2) & 0x7ff] = data & 0xff;
		return;
	}

	if ((address & 0xffff00) == 0xefff00) {
		DrvVRegs[((address & 0xff) / 2) & 0x7f] = BURN_ENDIAN_SWAP_INT16(data);
		switch (address & 0xfe) {
			case 0x0c: bankswitch(); break;
			case 0x18: blit(); break;
			case 0x1a: interrupt_enable = 1; break;
		}
		return;
	}

	if (address >= 0x3f8000 && address <= 0x3fffff) {
		sync_mcu();
		address = (address & 0x7fff) / 2;

		mcu_mailbox[address & 7] = data;

		if (address == 4) {
			M377SetIRQLine(M37710_LINE_IRQ0, CPU_IRQSTATUS_HOLD);
		}

		// numan and knckhead fail to write sequence to shared ram.
		UINT16 *ram = (UINT16*)Drv68KRAM;
		if ((BURN_ENDIAN_SWAP_INT16(ram[0xf72/2]) & 0xff00) == 0x0700 && namcona1_gametype == 0xed) {
			const UINT16 source[0x8] = { 0x534e, 0x2d41, 0x4942, 0x534f, 0x7620, 0x7265, 0x2e31, 0x3133 };

			for (INT32 i = 0; i < 8; i++) {
				ram[0x1000/2+i] = BURN_ENDIAN_SWAP_INT16(source[i]);
			}
		}

		return;
	}

	//bprintf(0, _T("main ww %x  %x\n"), address, data);
}

static void __fastcall namcona1_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff001) == 0xe00001) {
		DrvNVRAM[(address / 2) & 0x7ff] = data;
		return;
	}

	if ((address & 0xffff00) == 0xefff00) {
		UINT8 *ram = (UINT8*)DrvVRegs;
		ram[(address & 0xff) ^ 1] = data;
		switch (address & 0xfe) {
			case 0x0c: bankswitch(); break;
			case 0x18: blit(); break;
			case 0x1a: interrupt_enable = 1; break;
		}
		return;
	}

	bprintf(0, _T("main wb %x  %x\n"), address, data);

}

static UINT16 __fastcall namcona1_read_word(UINT32 address)
{
	if ((address & 0xfff000) == 0xe00000) {
		return DrvNVRAM[(address / 2) & 0x7ff];
	}

	if ((address & 0xfffff0) == 0xe40000) {
		return keycus_callback(((address & 0xf) / 2) & 7);
	}

	if ((address & 0xffff00) == 0xefff00) {
		return BURN_ENDIAN_SWAP_INT16(DrvVRegs[((address & 0xff) / 2) & 0x7f]);
	}

	if (address >= 0x3f8000 && address <= 0x3fffff) {
		sync_mcu();
		address = (address & 0x7fff) / 2;

		return mcu_mailbox[address & 7];
	}

	bprintf(0, _T("main rw %x\n"), address);

	return 0;
}

static UINT8 __fastcall namcona1_read_byte(UINT32 address)
{
	if ((address & 0xfff001) == 0xe00001) {
		return DrvNVRAM[(address / 2) & 0x7ff];
	}

	if ((address & 0xfffff0) == 0xe40000) {
		return keycus_callback(((address & 0xf) / 2) & 7) >> ((~address & 1) * 8);
	}

	if ((address & 0xffff00) == 0xefff00) {
		UINT8 *ram = (UINT8*)DrvVRegs;
		return ram[(address & 0xff) ^ 1];
	}

	bprintf(0, _T("main rb %x\n"), address);

	return 0;
}

static inline void palette_update_entry(INT32 i)
{
	UINT16 *ram = (UINT16*)DrvPaletteRAM;

	UINT16 c = BURN_ENDIAN_SWAP_INT16(ram[i]);
	int r = (((c & 0x00e0) >> 5) + ((c & 0xe000) >> 13) * 2) * 0xff / (0x7 * 3);
	int g = (((c & 0x001c) >> 2) + ((c & 0x1c00) >> 10) * 2) * 0xff / (0x7 * 3);
	int b = (((c & 0x0003) >> 0) + ((c & 0x0300) >>  8) * 2) * 0xff / (0x3 * 3);

	DrvPalette[0x1000 + i] = BurnHighCol(r,g,b,0);
	DrvPalette[0x3000 + i] = BurnHighCol(r/2,g/2,b/2,0); // shadow

	r = pal5bit(c >> 10);
	g = pal5bit(c >> 5);
	b = pal5bit(c);

	DrvPalette[0x0000 + i] = BurnHighCol(r,g,b,0);
	DrvPalette[0x2000 + i] = BurnHighCol(r/2,g/2,b/2,0); // shadow
	DrvPalette[0x4000] = 0; // black
}

static void __fastcall namcona1_palette_write_word(UINT32 address, UINT16 data)
{
	address = address & 0x1ffe;
	*((UINT16*)(DrvPaletteRAM + address)) = BURN_ENDIAN_SWAP_INT16(data);
	palette_update_entry(address/2);
}

static void __fastcall namcona1_palette_write_byte(UINT32 address, UINT8 data)
{
	address = address & 0x1fff;
	DrvPaletteRAM[address ^ 1] = data;
	palette_update_entry(address/2);
}

static void mcu_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff000) == 0x1000) {
		address &= 0x1ff;
		c140_write(address ^ 1, data);
		return;
	}

	bprintf(0, _T("mcu wb %x  %x\n"), address, data);
}

static void mcu_write_word(UINT32 address, UINT16 data)
{
	if (address >= 0x000800 && address <= 0x000fff) {
		address &= 0x7ff;
		mcu_mailbox[(address / 2) & 7] = data;
		return;
	}

	if ((address & 0xfff000) == 0x1000) {
		address &= 0x1fe;
		c140_write(address + 1, data & 0xff);
		c140_write(address + 0, (data >> 8) & 0xff);
		return;
	}
	bprintf(0, _T("mcu ww %x  %x\n"), address, data);
}

static UINT8 mcu_read_byte(UINT32 address)
{
	if ((address & 0xfff000) == 0x1000) {
		address &= 0x1ff;
		return c140_read(address^1);
	}

	bprintf(0, _T("mcu rb %x\n"), address);

	return 0xff;
}

static UINT16 mcu_read_word(UINT32 address)
{
	if (address >= 0x000800 && address <= 0x000fff) {
		address &= 0x7ff;
		return mcu_mailbox[(address / 2) & 7];
	}

	if ((address & 0xfff000) == 0x1000) {
		address &= 0x1fe;
		UINT16 ret = c140_read(address + 1) | (c140_read(address + 0) << 8);
		return ret;
	}

	bprintf(0, _T("mcu rw %x\n"), address);
	return 0xffff;
}

static void mcu_write_port(UINT32 address, UINT8 data)
{
	switch (address) {
		case M37710_PORT4: {
			if (data & 0x08 && ~port4_data & 0x08) {
				SekSetRESETLine(0);
				bprintf(0, _T("Turning on 68k!\n"));
			}
			port4_data = data;
		}
		break;
		case M37710_PORT5: {
			port5_data = (data & 0xfe) | ((data & 0x02) >> 1);
		}
		break;
		case M37710_PORT6: {
			port6_data = data;
		}
		break;
		case M37710_PORT8: {
			port8_data = data;
		}
		break;
	}
}

static UINT8 mcu_read_port(UINT32 address)
{
	if (address >= 0x10 && address <= 0x1f) {
		const UINT8 scramble[8] = { 0x40, 0x20, 0x10, 0x01, 0x02, 0x04, 0x08, 0x80 };

		return (DrvInputs[2] & scramble[(address & 0x0f) >> 1]) ? 0xff : 0x00;
	}

	switch (address) {
		case M37710_PORT4:
			return port4_data;

		case M37710_PORT5:
			return port5_data;

		case M37710_PORT6:
			return 0x00;

		case M37710_PORT7:
			if (~port6_data & 0x80) {
				switch (port6_data >> 5) {
					case 0: return DrvInputs[3];
					case 1: return (DrvDips[0] & 0x43) | (DrvInputs[4] & 0xbc);
					case 2: return DrvInputs[0];
					case 3: return DrvInputs[1];
				}
			}
			return 0xff;

		case M37710_PORT8:
			return port8_data;
	}

	bprintf(0, _T("mcu rp(unmapped) %x\n"), address);
	return 0x00;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekSetRESETLine(1); // mcu controls 68k
	bankswitch();
	SekClose();

	c140_reset();

	M377Open(0);
	M377Reset();
	M377Close();

	port4_data = 0;
	port5_data = 1;
	port6_data = 0;
	port7_data = 0;
	port8_data = 0;

	interrupt_enable = 0;

	BurnRandomSetSeed(0x313808303ULL);
	last_rand = 0;
	tinklpit_key = 0;

	nExtraCycles[0] = nExtraCycles[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0xa00000;
	DrvMCUROM		= Next; Next += 0x004000;

	DrvPalette		= (UINT32*)Next; Next += 0x4001 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000800;

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x080000;
	DrvMcuRAM       = Next; Next += 0x008000;
	mcu_mailbox     = (UINT16*)Next; Next += 0x000008 * sizeof(UINT16);

	DrvPaletteRAM	= Next; Next += 0x002000;
	DrvGfxRAM		= Next; Next += 0x040000;
	DrvVideoRAM		= Next; Next += 0x00e000;
	DrvScrollRAM	= Next; Next += 0x001000;
	DrvSpriteRAM	= Next; Next += 0x001000;
	DrvShapeRAM		= Next; Next += 0x008000;

	DrvVRegs		= (UINT16*)Next; Next += 0x00080 * sizeof(UINT16);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit(INT32 (*key_custom)(INT32))
{
	BurnAllocMemIndex();

	{
		char* pRomName;
		struct BurnRomInfo ri;
		UINT8 *pLoad[2] = { Drv68KROM + 0x800000, Drv68KROM };

		for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
		{
			BurnDrvGetRomInfo(&ri, i);

			if ((ri.nType & 7) == 1||(ri.nType & 7) == 2) {
				if (BurnLoadRom(pLoad[(ri.nType - 1) & 3] + 0, i+0, 2)) return 1;
				if (BurnLoadRom(pLoad[(ri.nType - 1) & 3] + 1, i+1, 2)) return 1;
				pLoad[(ri.nType - 1) & 3] += ri.nLen * 2;
				i++;
				continue;
			}

			if ((ri.nType & 7) == 3) {
				if (BurnLoadRom(DrvNVRAM, i, 1)) return 1;
				continue;
			}

			if ((ri.nType & 7) == 4) {
				if (BurnLoadRom(DrvMCUROM, i, 1)) return 1;
				continue;
			}
		}
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KRAM,			0x000000, 0x07ffff, MAP_RAM);
	SekMapMemory(Drv68KROM,			0x400000, 0xdfffff, MAP_ROM);
	SekMapMemory(DrvPaletteRAM,		0xf00000, 0xf01fff, MAP_RAM);
	SekMapMemory(DrvGfxRAM,			0xf40000, 0xf7ffff, MAP_RAM);
	SekMapMemory(DrvVideoRAM,		0xff0000, 0xffdfff, MAP_RAM);
	SekMapMemory(DrvScrollRAM,		0xffe000, 0xffefff, MAP_RAM);
	SekMapMemory(DrvSpriteRAM,		0xfff000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,		namcona1_write_word);
	SekSetWriteByteHandler(0,		namcona1_write_byte);
	SekSetReadWordHandler(0,		namcona1_read_word);
	SekSetReadByteHandler(0,		namcona1_read_byte);

	SekMapHandler(1,				0xf00000, 0xf01fff, MAP_WRITE);
	SekSetWriteWordHandler(1,		namcona1_palette_write_word);
	SekSetWriteByteHandler(1,		namcona1_palette_write_byte);
	SekClose();

	M377Init(0, M37702);
	M377Open(0);
	M377SetWritePortHandler(mcu_write_port);
	M377SetReadPortHandler(mcu_read_port);
	M377SetWriteByteHandler(mcu_write_byte);
	M377SetWriteWordHandler(mcu_write_word);
	M377SetReadByteHandler(mcu_read_byte);
	M377SetReadWordHandler(mcu_read_word);

	M377MapMemory(DrvMcuRAM,	0x003000, 0x00afff, MAP_RAM);
	M377MapMemory(Drv68KRAM,	0x002000, 0x002fff, MAP_RAM | M377_MEM_ENDISWAP);
	M377MapMemory(Drv68KRAM,	0x200000, 0x27ffff, MAP_RAM | M377_MEM_ENDISWAP);
	M377MapMemory(DrvMCUROM,	0x00c000, 0x00ffff, MAP_ROM);
	M377Reset();
	M377Close();

	c140_init(44100, C140_TYPE_ASIC219, Drv68KRAM);
	c140_set_sync(M377TotalCycles, 12528250 / 2);

	keycus_callback = key_custom;

	GenericTilesInit();

	BurnBitmapAllocate(1, 512, 512, false);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	M377Exit();

	c140_exit();

	BurnFreeMemIndex();

	namcona1_gametype = 0;

	return 0;
}

static void pdraw_tile(clip_struct clip, UINT32 code, UINT32 color, int sx, int sy, bool flipx, bool flipy, UINT8 priority, bool bShadow, bool bOpaque, UINT8 gfx_region)
{
	INT32 pixel_mask = gfx_region ? 0xf : 0xff;
	UINT32 shadow_color = gfx_region ? 0xff : 0xf;
	INT32 is_shadow = (color == shadow_color);
	const UINT16 pal_base = (color << (gfx_region ? 4 : 8)) & 0x1fff;
	const UINT8 *source_base = DrvGfxRAM + ((code & 0xfff) * 0x40); //gfx->get_data((code % gfx->elements()));
	const UINT8 *mask_base = DrvShapeRAM + ((code & 0xfff) * 8); //mask->get_data((code % mask->elements()));

	INT32 max_x = clip.nMaxx - 1;
	INT32 max_y = clip.nMaxy - 1;

	INT32 dx, dy;

	INT32 ex = sx + 8;
	INT32 ey = sy + 8;

	INT32 x_index_base;
	INT32 y_index;

	if (flipx)
	{
		x_index_base = 8 - 1;
		dx = -1;
	}
	else
	{
		x_index_base = 0;
		dx = 1;
	}

	if (flipy)
	{
		y_index = 8 - 1;
		dy = -1;
	}
	else
	{
		y_index = 0;
		dy = 1;
	}

	if (sx < clip.nMinx)
	{
		int pixels = clip.nMinx - sx;
		sx += pixels;
		x_index_base += pixels * dx;
	}
	if (sy < clip.nMiny)
	{
		int pixels = clip.nMiny - sy;
		sy += pixels;
		y_index += pixels * dy;
	}

	if (ex > max_x + 1)
	{
		int pixels = ex - max_x - 1;
		ex -= pixels;
	}
	if (ey > max_y + 1)
	{
		int pixels = ey - max_y - 1;
		ey -= pixels;
	}

	sy -= 32; // clip top
	ey -= 32; // clip bottom

	if (ex > sx)
	{
		for (INT32 y = sy; y < ey; y++)
		{
			UINT8 const *const source = source_base + (y_index) * 8;
			UINT8 mask_addr = mask_base[y_index ^ 1];
			UINT16 *dest = pTransDraw + y * nScreenWidth;
			UINT8 *pri = pPrioDraw + y * nScreenWidth;

			INT32 x_index = x_index_base;
			for (INT32 x = sx; x < ex; x++)
			{
				if (bOpaque)
				{
					if (pri[x] <= priority)
					{
						dest[x] = pal_base + (source[x_index ^ 1] & pixel_mask);
					}
					pri[x] = 0xff;
				}
				else
				{
					if (mask_addr & (0x80 >> (x_index & 7)))
					{
						if (pri[x] <= priority)
						{
							if (bShadow)
							{
								if (is_shadow)
								{
									dest[x] |= 0x2000;
								}
								else
								{
									dest[x] = pal_base + 0x1000 + (source[x_index ^ 1] & pixel_mask);
								}
							}
							else
							{
								dest[x] = pal_base + (source[x_index ^ 1] & pixel_mask);
							}
						}
						pri[x] = 0xff;
					}
				}
				x_index += dx;
			}
			y_index += dy;
		}
	}
}

static void draw_sprites(clip_struct clip)
{
	const UINT16 *source = (UINT16*)DrvSpriteRAM;

	const UINT16 sprite_control = BURN_ENDIAN_SWAP_INT16(DrvVRegs[0x22 / 2]);
	if (sprite_control & 1) source += 0x400;

	for (INT32 which = 0; which < 0x100; which++)
	{
		int bpp4,palbase;
		const UINT16 ypos  = BURN_ENDIAN_SWAP_INT16(source[0]);
		const UINT16 tile  = BURN_ENDIAN_SWAP_INT16(source[1]);
		const UINT16 color = BURN_ENDIAN_SWAP_INT16(source[2]);
		const UINT16 xpos  = BURN_ENDIAN_SWAP_INT16(source[3]);

		const UINT8 priority = color & 0x7;
		const UINT16 width = ((color >> 12) & 0x3) + 1;
		const UINT16 height = ((ypos >> 12) & 0x7) + 1;
		INT32 flipy = ypos & 0x8000;
		INT32 flipx = color & 0x8000;

		if (color & 8)
		{
			palbase = (color & 0xf0) | ((color & 0xf00) >> 8);
			bpp4 = 1;
		}
		else
		{
			palbase = (color & 0xf0) >> 4;
			bpp4 = 0;
		}

		for (INT32 row = 0; row < height; row++)
		{
			int sy = (ypos & 0x1ff) - 30 + 32;
			if (flipy)
			{
				sy += (height - 1 - row) << 3;
			}
			else
			{
				sy += row << 3;
			}
			sy = ((sy + 8) & 0x1ff) - 8;

			for (INT32 col = 0; col < width; col++)
			{
				int sx = (xpos & 0x1ff) - 10;
				if (flipx)
				{
					sx += (width - 1 - col) << 3;
				}
				else
				{
					sx += col << 3;
				}
				sx = ((sx + 16) & 0x1ff) - 8;

				pdraw_tile(clip, (tile & 0xfff) + (row << 6) + col,
					palbase,
					sx,sy,flipx,flipy,
					priority,
					color & 0x4000, /* shadow */
					tile & 0x8000, /* opaque */
					bpp4);

			}
		}

		source += 4;
	}
}

static void draw_pixel_line(clip_struct clip, UINT16 *pDest, UINT8 *pPri, UINT16 *pSource)
{
	pSource += clip.nMinx / 2;

	for (INT32 x = clip.nMinx; x < clip.nMaxx; x += 2)
	{
		UINT16 data = *pSource++;
		pPri[x + 0] = 0xff;
		pPri[x + 1] = 0xff;
		if (x >= clip.nMinx && x < clip.nMaxx)
			pDest[x+0] = data >> 8;
		if (x+1 >= clip.nMinx && x+1 < clip.nMaxx)
			pDest[x+1] = data & 0xff;
	}
}

static void draw_layer(clip_struct clip, INT32 layer, INT32 priority)
{
	UINT16 *vram = (UINT16*)DrvVideoRAM;
	UINT16 *ram = vram + (layer * 0x1000);
	UINT16 *scroll = (UINT16*)(DrvScrollRAM + (layer * 0x0400));

	INT32 depth = (BURN_ENDIAN_SWAP_INT16(DrvVRegs[0xbc/2]) >> layer) & 1;
	INT32 pixel_mask = depth ? 0xf : 0xff;
	INT32 color_mask = depth ? 0x7000 : 0;
	INT32 color_bank = (BURN_ENDIAN_SWAP_INT16(DrvVRegs[(0xb0/2)+layer]) & 0xf) << 8;

	INT32 xadjust = 0x3a - layer * 2;
	INT32 scrollx = xadjust;
	INT32 scrolly = 0;

	for (INT32 y = 0; y < 256; y++)
	{
		INT32 xdata = BURN_ENDIAN_SWAP_INT16(scroll[y]);
		INT32 ydata = BURN_ENDIAN_SWAP_INT16(scroll[y + 0x100]);

		scrollx = (xdata & 0x4000) ? (xadjust + xdata) : (scrollx + (xdata & 0x1ff));
		if (ydata & 0x4000) scrolly = (ydata - y) & 0x1ff;

		if (y < clip.nMiny || y >= clip.nMaxy) continue;

		INT32 sy = (scrolly + y) & 0x1ff; // right?

		UINT16 *dest = pTransDraw + (y - 32) * nScreenWidth;
		UINT8 *prio = pPrioDraw + (y - 32) * nScreenWidth;

		if (xdata == 0xc001) {
			draw_pixel_line(clip, dest, prio, vram + ydata + 25);
			continue;
		}

		for (INT32 x = clip.nMinx; x < clip.nMaxx + 7; x+=8)
		{
			INT32 sx = (scrollx + x) & 0x1ff;
			INT32 data = BURN_ENDIAN_SWAP_INT16(ram[(sx >> 3) | ((sy >> 3) << 6)]);
			INT32 color = ((data & color_mask) >> 8) + color_bank;

			UINT8 *pgfx = DrvGfxRAM + ((data & 0xfff) * 0x40) + ((sy & 7) * 8);
			UINT8 sgfx = (data & 0x8000) ? 0xff : DrvShapeRAM[((data & 0xfff) * 8) + ((sy & 7) ^ 1)];

			for (INT32 xx = 0; xx < 8; xx++)
			{
				INT32 xxx = (x + xx) - (sx & 7);
				if (xxx >= clip.nMinx && xxx < clip.nMaxx)
				{
					if (sgfx & (0x80 >> xx)) {
						dest[xxx] = color | (pgfx[xx ^ 1] & pixel_mask);
						prio[xxx] = priority;
					}
				}
			}
		}
	}
}

static void predraw_roz()
{
	INT32 depth = (BURN_ENDIAN_SWAP_INT16(DrvVRegs[0xbc/2]) >> 4) & 1;
	UINT16 *ram = (UINT16*)(DrvVideoRAM + 0x8000);
	INT32 pixel_mask = depth ? 0xf : 0xff;
	INT32 color_mask = depth ? 0x7000 : 0;
	INT32 color_bank = (BURN_ENDIAN_SWAP_INT16(DrvVRegs[(0xba/2)]) & 0xf) << 8;

	for (INT32 sy = 0; sy < 64; sy++)
	{
		for (INT32 sx = 0; sx < 64; sx++)
		{
			UINT16 data = BURN_ENDIAN_SWAP_INT16(ram[((sy >> 2) << 6) + (sx >> 2)]);
			UINT32 tile = (data & 0xfbf) + (sx & 3) + ((sy & 3) << 6);
			UINT16 color = ((data & color_mask) >> 8) + color_bank;

			UINT8 *pgfx = DrvGfxRAM + (tile * 0x40);
			UINT8 *sgfx = DrvShapeRAM + (tile * 8);

			for (INT32 yy = 0; yy < 8; yy++, pgfx+=8)
			{
				UINT16 *dest = BurnBitmapGetBitmap(1) + (yy + (sy * 8)) * 512 + (sx * 8);
				INT32 mask = (data & 0x8000) ? 0xff : sgfx[yy^1];

				for (INT32 xx = 0; xx < 8; xx++)
				{
					dest[xx] = (pgfx[xx ^ 1] & pixel_mask) | color;
					if ((mask & (0x80 >> xx)) == 0) dest[xx] |= 0x8000;
				}
			}
		}
	}
}

static void draw_roz(clip_struct clip, UINT32 startx, UINT32 starty, INT32 incxx, INT32 incxy, INT32 incyx, INT32 incyy, INT32 priority)
{
	UINT16 *src = BurnBitmapGetBitmap(1);

	starty += 32 * incyy; // clipy

	for (INT32 sy = clip.nMiny; sy < clip.nMaxy; sy++, startx+=incyx, starty+=incyy)
	{
		UINT32 cx = startx;
		UINT32 cy = starty;

		UINT16 *dst = pTransDraw + (sy - 32) * nScreenWidth;
		UINT8 *pri = pPrioDraw + (sy - 32) * nScreenWidth;

		for (INT32 x = clip.nMinx; x < clip.nMaxx; x++, cx+=incxx, cy+=incxy)
		{
			INT32 yy = cy >> 16;
			INT32 xx = cx >> 16;

			if (yy >= 512 || xx >= 512) continue;

			INT32 pxl = src[(yy << 9) | xx];

			if (pxl < 0x8000) {
				dst[x] = pxl;
				pri[x] = priority;
			}
		}
	}
}

static void draw_background(clip_struct clip, INT32 which, INT32 primask)
{
	if (which == 4)
	{
		INT32 incxx = ((INT16)BURN_ENDIAN_SWAP_INT16(DrvVRegs[0xc0 / 2]))<<8;
		INT32 incxy = ((INT16)BURN_ENDIAN_SWAP_INT16(DrvVRegs[0xc2 / 2]))<<8;
		INT32 incyx = ((INT16)BURN_ENDIAN_SWAP_INT16(DrvVRegs[0xc4 / 2]))<<8;
		INT32 incyy = ((INT16)BURN_ENDIAN_SWAP_INT16(DrvVRegs[0xc6 / 2]))<<8;
		INT16 xoffset = BURN_ENDIAN_SWAP_INT16(DrvVRegs[0xc8 / 2]);
		INT16 yoffset = BURN_ENDIAN_SWAP_INT16(DrvVRegs[0xca / 2]);
		INT32 dx = 46; // horizontal adjust
		INT32 dy = -8; // vertical adjust
		UINT32 startx = (xoffset<<12) + incxx * dx + incyx * dy;
		UINT32 starty = (yoffset<<12) + incxy * dx + incyy * dy;
		draw_roz(clip, startx, starty, incxx, incxy, incyx, incyy, primask);
	}
	else
	{
		draw_layer(clip, which, primask); // missing line mode!
	}
}

static INT32 enable_screen_clipped(clip_struct clip)
{
	if (clip.nMinx < 0) return 0;
	if (clip.nMaxx < 0) return 0;
	if (clip.nMinx >= clip.nMaxx) return 0;
	if (clip.nMiny >= clip.nMaxy) return 0;

	return 1;
}

static INT32 drawn;
static clip_struct clip;

static void DrvDrawBegin()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x2000/2; i++) {
			palette_update_entry(i);
		}
		DrvRecalc = 0;
	}

	clip.nMinx = BURN_ENDIAN_SWAP_INT16(DrvVRegs[0x80/2]) - 0x48;
	clip.nMaxx = BURN_ENDIAN_SWAP_INT16(DrvVRegs[0x82/2]) - 0x48;
	if (namcona1_gametype == 0xfa) {
		clip.nMaxx += 2; // fix clipping in Fighter & Attacker
	}
	clip.nMiny = BURN_ENDIAN_SWAP_INT16(DrvVRegs[0x84/2]);
	clip.nMaxy = BURN_ENDIAN_SWAP_INT16(DrvVRegs[0x86/2]);

	if (clip.nMinx < 0) clip.nMinx = 0;
	if (clip.nMaxx > nScreenWidth) clip.nMaxx = nScreenWidth;
	if (clip.nMiny < 32) clip.nMiny = 32;
	if (clip.nMaxy > (nScreenHeight + 32)) clip.nMaxy = nScreenHeight + 32;

	if (namcona1_gametype == 2) { // xday2
		BurnTransferClear((BURN_ENDIAN_SWAP_INT16(DrvVRegs[0xba / 2]) & 0xf) << 8);
	} else { // everyone else
		BurnTransferClear(0x4000);
	}

	drawn = 0;

	predraw_roz();
}

static void DrvDrawTo(INT32 lineto)
{
	clip.nMiny = BURN_ENDIAN_SWAP_INT16(DrvVRegs[0x84/2]);
	clip.nMaxy = BURN_ENDIAN_SWAP_INT16(DrvVRegs[0x86/2]);
	if (clip.nMiny < 32) clip.nMiny = 32;
	if (clip.nMaxy > (nScreenHeight + 32)) clip.nMaxy = nScreenHeight + 32;

	if (drawn+32 >= clip.nMiny)
		clip.nMiny = drawn + 32;
	if (lineto+32 <= clip.nMaxy)
		clip.nMaxy = lineto + 32;

	drawn = lineto;

	if (BURN_ENDIAN_SWAP_INT16(DrvVRegs[0x8e / 2]) && enable_screen_clipped(clip))
	{
		for (INT32 priority = 0; priority < 8; priority++)
		{
			for (INT32 which = 4; which >= 0; which--)
			{
				INT32 pri;
				if (which == 4)
				{
					pri = BURN_ENDIAN_SWAP_INT16(DrvVRegs[0xa0 / 2 + 5]) & 0x7;
				}
				else
				{
					pri = BURN_ENDIAN_SWAP_INT16(DrvVRegs[0xa0 / 2 + which]) & 0x7;
				}

				if (pri == priority)
				{
					if (nSpriteEnable & (1 << which)) draw_background(clip, which, priority);
				}
			}
		}

		if (nBurnLayer & 1) draw_sprites(clip);
	}
}

static INT32 DrvDrawEnd()
{
	DrvDrawTo(nScreenHeight);
	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDraw() // called by redraw only!
{
	DrvDrawBegin();
	DrvDrawTo(nScreenHeight);
	DrvDrawEnd();

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	M377NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 12528250 / 2 / 60, 12528250 / 60 };
	INT32 nCyclesDone[2] = { nExtraCycles[0], nExtraCycles[1] };

	SekOpen(0);
	M377Open(0);
	M377Idle(nExtraCycles[0]); // since we use CPU_RUN_SYNCINT

	DrvDrawBegin();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN_SYNCINT(0, M377);

		CPU_RUN(1, Sek);

		{
			INT32 i_en = (interrupt_enable) ? (~BURN_ENDIAN_SWAP_INT16(DrvVRegs[0x1a/2])) : 0;

			if (i_en & 4 && i == (BURN_ENDIAN_SWAP_INT16(DrvVRegs[0x8a/2]) & 0xff)) {
				DrvDrawTo(i);
				SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
			}

			if (i == 240) {
				*(UINT16*)(Drv68KRAM + 0xf60) = 0x0000; // "mcu ready"

				M377SetIRQLine(M37710_LINE_IRQ1, CPU_IRQSTATUS_HOLD);

				if (i_en & 0x08) {
					SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
				}

				if (pBurnDraw) {
					DrvDrawEnd();
				}
			}
		}
	}

	if (pBurnSoundOut) {
		BurnSoundClear();
	    c140_update(pBurnSoundOut, nBurnSoundLen);
	}

	nExtraCycles[0] = M377TotalCycles() - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

	SekClose();
	M377Close();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin =  0x029702;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x000800;
		ba.nAddress	= 0xe00000;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		M377Scan(nAction);

		c140_scan(nAction, pnMin);

		BurnRandomScan(nAction);
		SCAN_VAR(last_rand);

		SCAN_VAR(port4_data);
		SCAN_VAR(port5_data);
		SCAN_VAR(port6_data);
		SCAN_VAR(port7_data);
		SCAN_VAR(port8_data);

		SCAN_VAR(interrupt_enable);
		SCAN_VAR(tinklpit_key);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_WRITE) {
		SekOpen(0);
		bankswitch();
		SekClose();
	}

 	return 0;
}

static UINT16 guaranteed_rand()
{
	INT32 trand;

	do { trand = BurnRandom() & 0xffff; } while (trand == last_rand);
	last_rand = trand;

	return trand;
}

static struct BurnRomInfo emptyRomDesc[] = { { "", 0, 0, 0 }, }; // for bios handling

static struct BurnRomInfo namcoc69RomDesc[] = {
	{ "c69.bin",		0x004000, 0x349134D9, 4 | BRF_BIOS | BRF_PRG | BRF_ESS }, // C69 Internal ROM
};

STD_ROM_PICK(namcoc69)
STD_ROM_FN(namcoc69)

static INT32 namcocInit() {
	return 1;
}

struct BurnDriver BurnDrvnamcoc69 = {
	"namcoc69", NULL, NULL, NULL, "1994",
	"Namco C69 (M37702) (Bios)\0", "BIOS only", "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_MISC_POST90S, GBF_BIOS, 0,
	NULL, namcoc69RomInfo, namcoc69RomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	namcocInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};

static struct BurnRomInfo namcoc70RomDesc[] = {
	{ "c70.bin",		0x004000, 0xB4015F23, 4 | BRF_BIOS | BRF_PRG | BRF_ESS }, // C70 Internal ROM
};

STD_ROM_PICK(namcoc70)
STD_ROM_FN(namcoc70)

struct BurnDriver BurnDrvnamcoc70 = {
	"namcoc70", NULL, NULL, NULL, "1994",
	"Namco C70 (M37702) (Bios)\0", "BIOS only", "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_MISC_POST90S, GBF_BIOS, 0,
	NULL, namcoc70RomInfo, namcoc70RomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	namcocInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	224, 288, 3, 4
};

// Bakuretsu Quiz Ma-Q Dai Bouken (Japan)

static struct BurnRomInfo bkrtmaqRomDesc[] = {
	{ "mq1-ep0l.bin",			0x080000, 0xf029bc57, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "mq1-ep0u.bin",			0x080000, 0x4cff62b8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "mq1-ep1l.bin",			0x080000, 0xe3be6f4b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "mq1-ep1u.bin",			0x080000, 0xb44e31b2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "mq1-ma0l.bin",			0x100000, 0x11fed35f, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "mq1-ma0u.bin",			0x100000, 0x23442ac0, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "mq1-ma1l.bin",			0x100000, 0xfe82205f, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "mq1-ma1u.bin",			0x100000, 0x0cdb6bd0, 2 | BRF_PRG | BRF_ESS }, //  7
};

STDROMPICKEXT(bkrtmaq, bkrtmaq, namcoc69)
STD_ROM_FN(bkrtmaq)

static INT32 bkrtmaq_keycus_read(INT32 offset)
{
	if (offset == 2) return 0x15c;
	return guaranteed_rand();
}

static INT32 BkrtmaqInit()
{
	return DrvInit(bkrtmaq_keycus_read);
}

struct BurnDriver BurnDrvBkrtmaq = {
	"bkrtmaq", NULL, "namcoc69", NULL, "1992",
	"Bakuretsu Quiz Ma-Q Dai Bouken (Japan)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, bkrtmaqRomInfo, bkrtmaqRomName, NULL, NULL, NULL, NULL, Namcona1_quizInputInfo, Namcona1_quizDIPInfo,
	BkrtmaqInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Cosmo Gang the Puzzle (US)

static struct BurnRomInfo cgangpzlRomDesc[] = {
	{ "cp2-ep0l.4c",			0x080000, 0x8f5cdcc5, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "cp2-ep0u.4f",			0x080000, 0x3a816140, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "eeprom_cgangpzl",		0x000800, 0x5f8dfe9e, 3 | BRF_PRG | BRF_ESS }, //  2 Default NV RAM
};

STDROMPICKEXT(cgangpzl, cgangpzl, namcoc69)
STD_ROM_FN(cgangpzl)

static INT32 cgangpzl_keycus_read(INT32 offset)
{
	if (offset == 1) return 0x164;
	return guaranteed_rand();
}

static INT32 CgangpzlInit()
{
	return DrvInit(cgangpzl_keycus_read);
}

struct BurnDriver BurnDrvCgangpzl = {
	"cgangpzl", NULL, "namcoc69", NULL, "1992",
	"Cosmo Gang the Puzzle (US)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, cgangpzlRomInfo, cgangpzlRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	CgangpzlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Cosmo Gang the Puzzle (Japan)

static struct BurnRomInfo cgangpzljRomDesc[] = {
	{ "cp1-ep0l.4c",			0x080000, 0x2825f7ba, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "cp1-ep0u.4f",			0x080000, 0x94d7d6fc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "eeprom_cgangpzlj",		0x000800, 0xef5ddff2, 3 | BRF_PRG | BRF_ESS }, //  2 Default NV RAM
};

STDROMPICKEXT(cgangpzlj, cgangpzlj, namcoc69)
STD_ROM_FN(cgangpzlj)

struct BurnDriver BurnDrvCgangpzlj = {
	"cgangpzlj", "cgangpzl", "namcoc69", NULL, "1992",
	"Cosmo Gang the Puzzle (Japan)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, cgangpzljRomInfo, cgangpzljRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	CgangpzlInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Emeraldia (Japan Version B)

static struct BurnRomInfo emeraldajbRomDesc[] = {
	{ "em1-ep0lb.6c",			0x080000, 0xfcd55293, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "em1-ep0ub.6f",			0x080000, 0xa52f00d5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "em1-ep1l.7c",			0x080000, 0x373c1c59, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "em1-ep1u.7f",			0x080000, 0x4e969152, 1 | BRF_PRG | BRF_ESS }, //  3
};

STDROMPICKEXT(emeraldajb, emeraldajb, namcoc69)
STD_ROM_FN(emeraldajb)

static INT32 emeralda_keycus_read(INT32 offset)
{
	if (offset == 1) return 0x166;
	return guaranteed_rand();
}

static INT32 EmeraldaInit()
{
	return DrvInit(emeralda_keycus_read);
}

struct BurnDriver BurnDrvEmeraldajb = {
	"emeraldajb", "emeralda", "namcoc69", NULL, "1993",
	"Emeraldia (Japan Version B)\0", "Slight GFX Issues", "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, emeraldajbRomInfo, emeraldajbRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	EmeraldaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Emeraldia (Japan)

static struct BurnRomInfo emeraldajRomDesc[] = {
	{ "em1-ep0l.6c",			0x080000, 0x443f3fce, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "em1-ep0u.6f",			0x080000, 0x484a2a81, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "em1-ep1l.7c",			0x080000, 0x373c1c59, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "em1-ep1u.7c",			0x080000, 0x4e969152, 1 | BRF_PRG | BRF_ESS }, //  3
};

STDROMPICKEXT(emeraldaj, emeraldaj, namcoc69)
STD_ROM_FN(emeraldaj)

struct BurnDriver BurnDrvEmeraldaj = {
	"emeraldaj", "emeralda", "namcoc69", NULL, "1993",
	"Emeraldia (Japan)\0", "Slight GFX Issues", "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, emeraldajRomInfo, emeraldajRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	EmeraldaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Emeraldia (Bankbank New Rotate Hack)

static struct BurnRomInfo emeraldahRomDesc[] = {
	{ "em1-ep0l.6c",			0x080000, 0x9b60320f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "em1-ep0u.6f",			0x080000, 0x5411cbec, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "em1-ep1l.7c",			0x080000, 0x6c3e5b53, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "em1-ep1u.7f",			0x080000, 0xdee15a81, 1 | BRF_PRG | BRF_ESS }, //  3
};

STDROMPICKEXT(emeraldah, emeraldah, namcoc70)
STD_ROM_FN(emeraldah)

struct BurnDriver BurnDrvEmeraldah = {
	"emeraldah", "emeralda", "namcoc70", NULL, "2022",
	"Emeraldia (Bankbank New Rotate Hack)\0", "Slight GFX Issues", "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HACK | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, emeraldahRomInfo, emeraldahRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	EmeraldaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Exvania (World)

static struct BurnRomInfo exvaniaRomDesc[] = {
	{ "ex2-ep0l.4c",			0x080000, 0xccf46677, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ex2-ep0u.4f",			0x080000, 0x37b8d890, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ex1-ma0l.2c",			0x100000, 0x17922cd4, 2 | BRF_PRG | BRF_ESS }, //  2 68K Code (Mask ROM)
	{ "ex1-ma0u.2f",			0x100000, 0x93d66106, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "ex1-ma1l.3c",			0x100000, 0xe4bba6ed, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "ex1-ma1u.3f",			0x100000, 0x04e7c4b0, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "eeprom",					0x000800, 0x0f46389d, 3 | BRF_PRG | BRF_ESS }, //  6 Default NV RAM
};

STDROMPICKEXT(exvania, exvania, namcoc69)
STD_ROM_FN(exvania)

static INT32 exvania_keycus_read(INT32 offset)
{
	if (offset == 2) return 0x15e;
	return guaranteed_rand();
}

static INT32 ExvaniaInit()
{
	return DrvInit(exvania_keycus_read);
}

struct BurnDriver BurnDrvExvania = {
	"exvania", NULL, "namcoc69", NULL, "1992",
	"Exvania (World)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, exvaniaRomInfo, exvaniaRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	ExvaniaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Exvania (Japan)

static struct BurnRomInfo exvaniajRomDesc[] = {
	{ "ex1-ep0l.4c",			0x080000, 0x18c12015, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "ex1-ep0u.4f",			0x080000, 0x07d054d1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ex1-ma0l.2c",			0x100000, 0x17922cd4, 2 | BRF_PRG | BRF_ESS }, //  2 68K Code (Mask ROM)
	{ "ex1-ma0u.2f",			0x100000, 0x93d66106, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "ex1-ma1l.3c",			0x100000, 0xe4bba6ed, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "ex1-ma1u.3f",			0x100000, 0x04e7c4b0, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "eeprom",					0x000800, 0x0f46389d, 3 | BRF_PRG | BRF_ESS }, //  6 Default NV RAM
};

STDROMPICKEXT(exvaniaj, exvaniaj, namcoc69)
STD_ROM_FN(exvaniaj)

struct BurnDriver BurnDrvExvaniaj = {
	"exvaniaj", "exvania", "namcoc69", NULL, "1992",
	"Exvania (Japan)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, exvaniajRomInfo, exvaniajRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	ExvaniaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Fighter & Attacker (US)

static struct BurnRomInfo fghtatckRomDesc[] = {
	{ "fa2-ep0l.4c",			0x080000, 0x8996db9c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "fa2-ep0u.4f",			0x080000, 0x58d5e090, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fa1-ep1l.5c",			0x080000, 0xb23a5b01, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fa1-ep1u.5f",			0x080000, 0xde2eb129, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fa1-ma0l.2c",			0x100000, 0xa0a95e54, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "fa1-ma0u.2f",			0x100000, 0x1d0135bd, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "fa1-ma1l.3c",			0x100000, 0xc4adf0a2, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "fa1-ma1u.3f",			0x100000, 0x900297be, 2 | BRF_PRG | BRF_ESS }, //  7
};

STDROMPICKEXT(fghtatck, fghtatck, namcoc69)
STD_ROM_FN(fghtatck)

static INT32 fghtatck_keycus_read(INT32 offset)
{
	if (offset == 2) return 0x15d;
	return guaranteed_rand();
}

static INT32 FghtatckInit()
{
	namcona1_gametype = 0xfa;

	return DrvInit(fghtatck_keycus_read);
}

struct BurnDriver BurnDrvFghtatck = {
	"fghtatck", NULL, "namcoc69", NULL, "1992",
	"Fighter & Attacker (US)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, fghtatckRomInfo, fghtatckRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	FghtatckInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 304, 3, 4
};


// F/A (Japan)

static struct BurnRomInfo faRomDesc[] = {
	{ "fa1-ep0l.4c",			0x080000, 0x182eee5c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "fa1-ep0u.4f",			0x080000, 0x7ea7830e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "fa1-ep1l.5c",			0x080000, 0xb23a5b01, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "fa1-ep1u.5f",			0x080000, 0xde2eb129, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "fa1-ma0l.2c",			0x100000, 0xa0a95e54, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "fa1-ma0u.2f",			0x100000, 0x1d0135bd, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "fa1-ma1l.3c",			0x100000, 0xc4adf0a2, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "fa1-ma1u.3f",			0x100000, 0x900297be, 2 | BRF_PRG | BRF_ESS }, //  7
};

STDROMPICKEXT(fa, fa, namcoc69)
STD_ROM_FN(fa)

struct BurnDriver BurnDrvFa = {
	"fa", "fghtatck", "namcoc69", NULL, "1992",
	"F/A (Japan)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_VERSHOOT, 0,
	NULL, faRomInfo, faRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	FghtatckInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	224, 304, 3, 4
};


// Super World Court (World)

static struct BurnRomInfo swcourtRomDesc[] = {
	{ "sc2-ep0l.4c",			0x080000, 0x5053a02e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sc2-ep0u.4f",			0x080000, 0x7b3fc7fa, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sc1-ep1l.5c",			0x080000, 0xfb45cf5f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sc1-ep1u.5f",			0x080000, 0x1ce07b15, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sc1-ma0l.2c",			0x100000, 0x3e531f5e, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "sc1-ma0u.2f",			0x100000, 0x31e76a45, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "sc1-ma1l.3c",			0x100000, 0x8ba3a4ec, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "sc1-ma1u.3f",			0x100000, 0x252dc4b7, 2 | BRF_PRG | BRF_ESS }, //  7
};

STDROMPICKEXT(swcourt, swcourt, namcoc69)
STD_ROM_FN(swcourt)

static INT32 swcourt_keycus_read(INT32 offset)
{
	if (offset == 1) return 0x165;
	return guaranteed_rand();
}

static INT32 SwcourtInit()
{
	return DrvInit(swcourt_keycus_read);
}

struct BurnDriver BurnDrvSwcourt = {
	"swcourt", NULL, "namcoc69", NULL, "1992",
	"Super World Court (World)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, swcourtRomInfo, swcourtRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	SwcourtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Super World Court (Japan)

static struct BurnRomInfo swcourtjRomDesc[] = {
	{ "sc1-ep0l.4c",			0x080000, 0x145111dd, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "sc1-ep0u.4f",			0x080000, 0xc721c138, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "sc1-ep1l.5c",			0x080000, 0xfb45cf5f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "sc1-ep1u.5f",			0x080000, 0x1ce07b15, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "sc1-ma0l.2c",			0x100000, 0x3e531f5e, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "sc1-ma0u.2f",			0x100000, 0x31e76a45, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "sc1-ma1l.3c",			0x100000, 0x8ba3a4ec, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "sc1-ma1u.3f",			0x100000, 0x252dc4b7, 2 | BRF_PRG | BRF_ESS }, //  7
};

STDROMPICKEXT(swcourtj, swcourtj, namcoc69)
STD_ROM_FN(swcourtj)

struct BurnDriver BurnDrvSwcourtj = {
	"swcourtj", "swcourt", "namcoc69", NULL, "1992",
	"Super World Court (Japan)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, swcourtjRomInfo, swcourtjRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	SwcourtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Super World Court (World, bootleg)

static struct BurnRomInfo swcourtbRomDesc[] = {
	{ "0l0l.4c",				0x080000, 0x669c9b10, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "0u0u.4f",				0x080000, 0x742f3da1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "1l1l.5c",				0x080000, 0xfb45cf5f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "1u.1u.5f",				0x080000, 0x1ce07b15, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "oll.ol.2c",				0x080000, 0xdf0920ef, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "oul.ou.2f",				0x080000, 0x844c6a1c, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "olh.ol.2c",				0x080000, 0x0a21abea, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "ouh.ou.2f",				0x080000, 0x6b7c93f9, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "1ll.1l.3c",				0x080000, 0xf7e30277, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "1ul.1u.3f",				0x080000, 0x5f03c51a, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "1lh.1l.3c",				0x080000, 0x6531236e, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "1uh.1u.3f",				0x080000, 0xacae6746, 2 | BRF_PRG | BRF_ESS }, // 11
};

STDROMPICKEXT(swcourtb, swcourtb, namcoc69)
STD_ROM_FN(swcourtb)

static INT32 swcourtb_keycus_read(INT32 offset)
{
	if (offset == 1) return 0x8061;
	return guaranteed_rand();
}

static INT32 SwcourtbInit()
{
	return DrvInit(swcourtb_keycus_read);
}

struct BurnDriver BurnDrvSwcourtb = {
	"swcourtb", "swcourt", "namcoc69", NULL, "1994",
	"Super World Court (World, bootleg)\0", NULL, "bootleg (Playmark?)", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, swcourtbRomInfo, swcourtbRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	SwcourtbInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Tinkle Pit (Japan)

static struct BurnRomInfo tinklpitRomDesc[] = {
	{ "tk1-ep0l.6c",			0x080000, 0xfdccae42, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "tk1-ep0u.6f",			0x080000, 0x62cdb48c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tk1-ep1l.7c",			0x080000, 0x7e90f104, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tk1-ep1u.7f",			0x080000, 0x9c0b70d6, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tk1-ma0l.2c",			0x100000, 0xc6b4e15d, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "tk1-ma0u.2f",			0x100000, 0xa3ad6f67, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "tk1-ma1l.3c",			0x100000, 0x61cfb92a, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "tk1-ma1u.3f",			0x100000, 0x54b77816, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "tk1-ma2l.4c",			0x100000, 0x087311d2, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "tk1-ma2u.4f",			0x100000, 0x5ce20c2c, 2 | BRF_PRG | BRF_ESS }, //  9
};

STDROMPICKEXT(tinklpit, tinklpit, namcoc69)
STD_ROM_FN(tinklpit)

static INT32 tinklpit_keycus_read(INT32 offset)
{
	const UINT16 data[0x20] = {
		0x0000, 0x2000, 0x2100, 0x2104, 0x0106, 0x0007, 0x4003, 0x6021,
		0x61a0, 0x31a4, 0x9186, 0x9047, 0xc443, 0x6471, 0x6db0, 0x39bc,
		0x9b8e, 0x924f, 0xc643, 0x6471, 0x6db0, 0x19bc, 0xba8e, 0xb34b,
		0xe745, 0x4576, 0x0cb7, 0x789b, 0xdb29, 0xc2ec, 0x16e2, 0xb491
	};

	switch (offset) {
		case 3:	return data[(tinklpit_key++) & 0x1f];
		case 4: tinklpit_key = 0; break;
		case 7: return 0x016f;
	}

	return guaranteed_rand();
}

static INT32 TinklpitInit()
{
	namcona1_gametype = 1;

	return DrvInit(tinklpit_keycus_read);
}

struct BurnDriver BurnDrvTinklpit = {
	"tinklpit", NULL, "namcoc69", NULL, "1993",
	"Tinkle Pit (Japan)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, tinklpitRomInfo, tinklpitRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	TinklpitInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Emeraldia (World)

static struct BurnRomInfo emeraldaRomDesc[] = {
	{ "em2-ep0l.6c",			0x080000, 0xff1479dc, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "em2-ep0u.6f",			0x080000, 0xffe750a2, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "em2-ep1l.7c",			0x080000, 0x6c3e5b53, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "em2-ep1u.7f",			0x080000, 0xdee15a81, 1 | BRF_PRG | BRF_ESS }, //  3
};

STDROMPICKEXT(emeralda, emeralda, namcoc70)
STD_ROM_FN(emeralda)

struct BurnDriver BurnDrvEmeralda = {
	"emeralda", NULL, "namcoc70", NULL, "1993",
	"Emeraldia (World)\0", "Slight GFX Issues", "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, emeraldaRomInfo, emeraldaRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	EmeraldaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Emeraldia (Japan Version D)

static struct BurnRomInfo emeraldajdRomDesc[] = {
	{ "em1-ep0ld.6c",			0x080000, 0x988eb1cb, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "em1-ep0ud.6f",			0x080000, 0x6a169b8c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "em1-ep1l.7c",			0x080000, 0x373c1c59, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "em1-ep1u.7c",			0x080000, 0x4e969152, 1 | BRF_PRG | BRF_ESS }, //  3
};

STDROMPICKEXT(emeraldajd, emeraldajd, namcoc69)
STD_ROM_FN(emeraldajd)

struct BurnDriver BurnDrvEmeraldajd = {
	"emeraldajd", "emeralda", "namcoc69", NULL, "1993",
	"Emeraldia (Japan Version D)\0", "Slight GFX Issues", "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, emeraldajdRomInfo, emeraldajdRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	EmeraldaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Knuckle Heads (World)

static struct BurnRomInfo knckheadRomDesc[] = {
	{ "kh2-ep0l.6c",			0x080000, 0xb4b88077, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "kh2-ep0u.6f",			0x080000, 0xa578d97e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kh1-ep1l.7c",			0x080000, 0x27e6ab4e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kh1-ep1u.7f",			0x080000, 0x487b2434, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "kh1-ma0l.2c",			0x100000, 0x7b2db5df, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "kh1-ma0u.2f",			0x100000, 0x6983228b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "kh1-ma1l.3c",			0x100000, 0xb24f93e6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "kh1-ma1u.3f",			0x100000, 0x18a60348, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "kh1-ma2l.4c",			0x100000, 0x82064ee9, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "kh1-ma2u.4f",			0x100000, 0x17fe8c3d, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "kh1-ma3l.5c",			0x100000, 0xad9a7807, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "kh1-ma3u.5f",			0x100000, 0xefeb768d, 2 | BRF_PRG | BRF_ESS }, // 11
};

STDROMPICKEXT(knckhead, knckhead, namcoc70)
STD_ROM_FN(knckhead)

static INT32 knckhead_keycus_read(INT32 offset)
{
	if (offset == 1) return 0x168;
	if (offset == 2) return guaranteed_rand();
	return BurnRandom() & 0xffff;
}

static INT32 KnckheadInit()
{
	namcona1_gametype = 0xed; // write version back

	return DrvInit(knckhead_keycus_read);
}

struct BurnDriver BurnDrvKnckhead = {
	"knckhead", NULL, "namcoc70", NULL, "1992",
	"Knuckle Heads (World)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, knckheadRomInfo, knckheadRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	KnckheadInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Knuckle Heads (Japan)

static struct BurnRomInfo knckheadjRomDesc[] = {
	{ "kh1-ep0l.6c",			0x080000, 0x94660bec, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "kh1-ep0u.6f",			0x080000, 0xad640d69, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "kh1-ep1l.7c",			0x080000, 0x27e6ab4e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "kh1-ep1u.7f",			0x080000, 0x487b2434, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "kh1-ma0l.2c",			0x100000, 0x7b2db5df, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "kh1-ma0u.2f",			0x100000, 0x6983228b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "kh1-ma1l.3c",			0x100000, 0xb24f93e6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "kh1-ma1u.3f",			0x100000, 0x18a60348, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "kh1-ma2l.4c",			0x100000, 0x82064ee9, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "kh1-ma2u.4f",			0x100000, 0x17fe8c3d, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "kh1-ma3l.5c",			0x100000, 0xad9a7807, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "kh1-ma3u.5f",			0x100000, 0xefeb768d, 2 | BRF_PRG | BRF_ESS }, // 11
};

STDROMPICKEXT(knckheadj, knckheadj, namcoc70)
STD_ROM_FN(knckheadj)

struct BurnDriver BurnDrvKnckheadj = {
	"knckheadj", "knckhead", "namcoc70", NULL, "1992",
	"Knuckle Heads (Japan)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, knckheadjRomInfo, knckheadjRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	KnckheadInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Knuckle Heads (Japan, Prototype?)

static struct BurnRomInfo knckheadjpRomDesc[] = {
	{ "2-10_9o.6c",				0x080000, 0x600faf17, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "2-10_9e.6f",				0x080000, 0x16ccc0b0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "2-10_8o.7c",				0x080000, 0x27e6ab4e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "2-10_8e.7f",				0x080000, 0x487b2434, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "kh1-ma0l.2c",			0x100000, 0x7b2db5df, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "kh1-ma0u.2f",			0x100000, 0x6983228b, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "kh1-ma1l.3c",			0x100000, 0xb24f93e6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "kh1-ma1u.3f",			0x100000, 0x18a60348, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "kh1-ma2l.4c",			0x100000, 0x82064ee9, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "kh1-ma2u.4f",			0x100000, 0x17fe8c3d, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "kh1-ma3l.5c",			0x100000, 0xad9a7807, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "kh1-ma3u.5f",			0x100000, 0xefeb768d, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "eeprom",					0x000800, 0x98875a23, 3 | BRF_PRG | BRF_ESS }, // 12 Default NV RAM
};

STDROMPICKEXT(knckheadjp, knckheadjp, namcoc70)
STD_ROM_FN(knckheadjp)

struct BurnDriver BurnDrvKnckheadjp = {
	"knckheadjp", "knckhead", "namcoc70", NULL, "1992",
	"Knuckle Heads (Japan, Prototype?)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 4, HARDWARE_MISC_POST90S, GBF_VSFIGHT, 0,
	NULL, knckheadjpRomInfo, knckheadjpRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	KnckheadInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Numan Athletics (World)

static struct BurnRomInfo numanathRomDesc[] = {
	{ "nm2-ep0l.6c",			0x080000, 0xf24414bb, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "nm2-ep0u.6f",			0x080000, 0x25c41616, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nm1-ep1l.7c",			0x080000, 0x4581dcb4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nm1-ep1u.7f",			0x080000, 0x30cd589a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "nm1-ma0l.2c",			0x100000, 0x20faaa57, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "nm1-ma0u.2f",			0x100000, 0xed7c37f2, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "nm1-ma1l.3c",			0x100000, 0x2232e3b4, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "nm1-ma1u.3f",			0x100000, 0x6cc9675c, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "nm1-ma2l.4c",			0x100000, 0x208abb39, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "nm1-ma2u.4f",			0x100000, 0x03a3f204, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "nm1-ma3l.5c",			0x100000, 0x42a539e9, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "nm1-ma3u.5f",			0x100000, 0xf79e2112, 2 | BRF_PRG | BRF_ESS }, // 11
};

STDROMPICKEXT(numanath, numanath, namcoc70)
STD_ROM_FN(numanath)

static INT32 numanath_keycus_read(INT32 offset)
{
	if (offset == 1) return 0x167;
	return guaranteed_rand();
}

static INT32 NumanathInit()
{
	namcona1_gametype = 0xed; // write version back

	return DrvInit(numanath_keycus_read);
}

struct BurnDriver BurnDrvNumanath = {
	"numanath", NULL, "namcoc70", NULL, "1993",
	"Numan Athletics (World)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, numanathRomInfo, numanathRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	NumanathInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Numan Athletics (Japan)

static struct BurnRomInfo numanathjRomDesc[] = {
	{ "nm1-ep0l.6c",			0x080000, 0x4398b898, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "nm1-ep0u.6f",			0x080000, 0xbe90aa79, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "nm1-ep1l.7c",			0x080000, 0x4581dcb4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "nm1-ep1u.7f",			0x080000, 0x30cd589a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "nm1-ma0l.2c",			0x100000, 0x20faaa57, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "nm1-ma0u.2f",			0x100000, 0xed7c37f2, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "nm1-ma1l.3c",			0x100000, 0x2232e3b4, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "nm1-ma1u.3f",			0x100000, 0x6cc9675c, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "nm1-ma2l.4c",			0x100000, 0x208abb39, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "nm1-ma2u.4f",			0x100000, 0x03a3f204, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "nm1-ma3l.5c",			0x100000, 0x42a539e9, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "nm1-ma3u.5f",			0x100000, 0xf79e2112, 2 | BRF_PRG | BRF_ESS }, // 11
};

STDROMPICKEXT(numanathj, numanathj, namcoc70)
STD_ROM_FN(numanathj)

struct BurnDriver BurnDrvNumanathj = {
	"numanathj", "numanath", "namcoc70", NULL, "1993",
	"Numan Athletics (Japan)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, numanathjRomInfo, numanathjRomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	NumanathInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// Nettou! Gekitou! Quiztou!! (Japan)

static struct BurnRomInfo quiztouRomDesc[] = {
	{ "qt1ep0l.6c",				0x080000, 0xb680e543, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "qt1ep0u.6f",				0x080000, 0x143c5e4d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "qt1ep1l.7c",				0x080000, 0x33a72242, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "qt1ep1u.7f",				0x080000, 0x69f876cb, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "qt1ma0l.2c",				0x100000, 0x5597f2b9, 2 | BRF_PRG | BRF_ESS }, //  4 68K Code (Mask ROM)
	{ "qt1ma0u.2f",				0x100000, 0xf0a4cb7d, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "qt1ma1l.3c",				0x100000, 0x1b9ce7a6, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "qt1ma1u.3f",				0x100000, 0x58910872, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "qt1ma2l.4c",				0x100000, 0x94739917, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "qt1ma2u.4f",				0x100000, 0x6ba5b893, 2 | BRF_PRG | BRF_ESS }, //  9
	{ "qt1ma3l.5c",				0x100000, 0xaa9dc6ff, 2 | BRF_PRG | BRF_ESS }, // 10
	{ "qt1ma3u.5f",				0x100000, 0x14a5a163, 2 | BRF_PRG | BRF_ESS }, // 11

	{ "eeprom",					0x000800, 0x57a478a6, 3 | BRF_PRG | BRF_ESS }, // 12 Default NV RAM
};

STDROMPICKEXT(quiztou, quiztou, namcoc70)
STD_ROM_FN(quiztou)

static INT32 quiztou_keycus_read(INT32 offset)
{
	if (offset == 2) return 0x16d;
	return guaranteed_rand();
}

static INT32 QuiztouInit()
{
	return DrvInit(quiztou_keycus_read);
}

struct BurnDriver BurnDrvQuiztou = {
	"quiztou", NULL, "namcoc70", NULL, "1993",
	"Nettou! Gekitou! Quiztou!! (Japan)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, quiztouRomInfo, quiztouRomName, NULL, NULL, NULL, NULL, Namcona1_quizInputInfo, Namcona1_quizDIPInfo,
	QuiztouInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};


// X-Day 2 (Japan)

static struct BurnRomInfo xday2RomDesc[] = {
	{ "xds1-mpr0.4b",			0x080000, 0x83539aaa, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "xds1-mpr1.8b",			0x080000, 0x468b36de, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "xds1-dat0.4b",			0x200000, 0x42cecc8b, 2 | BRF_PRG | BRF_ESS }, //  2 68K Code (Mask ROM)
	{ "xds1-dat1.8b",			0x200000, 0xd250b7e8, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "xds1-dat2.4c",			0x200000, 0x99d72a08, 2 | BRF_PRG | BRF_ESS }, //  4
	{ "xds1-dat3.8c",			0x200000, 0x8980acc4, 2 | BRF_PRG | BRF_ESS }, //  5
};

STDROMPICKEXT(xday2, xday2, namcoc70)
STD_ROM_FN(xday2)

static INT32 xday2_keycus_read(INT32 offset)
{
	if (offset == 2) return 0x18a;
	return guaranteed_rand();
}

static INT32 Xday2Init()
{
	namcona1_gametype = 2;

	return DrvInit(xday2_keycus_read);
}

struct BurnDriver BurnDrvXday2 = {
	"xday2", NULL, "namcoc70", NULL, "1995",
	"X-Day 2 (Japan)\0", NULL, "Namco", "NA-1 / NA-2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, xday2RomInfo, xday2RomName, NULL, NULL, NULL, NULL, Namcona1InputInfo, Namcona1DIPInfo,
	Xday2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	304, 224, 4, 3
};
