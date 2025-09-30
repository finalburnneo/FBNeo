// FB Neo NeoGeo Pocket[color] driver module
// Based on MESS driver by Wilbert Pol

#include "tiles_generic.h"
#include "tlcs900_intf.h"
#include "z80_intf.h"
#include "burn_pal.h"
#include "k1ge.h"
#include "t6w28.h"
#include "dac.h"
#include <stddef.h>

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvBiosROM;
static UINT8 *DrvCartROM;
static UINT8 *DrvCartBak;
static UINT8 *DrvCartTmp;
static UINT8 *DrvMainRAM;
static UINT8 *DrvShareRAM;

static UINT8 io_reg[0x40];
static UINT8 old_to3;
static INT32 timer_time;
static INT32 previous_start;
static INT32 color_mode;
static INT32 blind_mode; // for power-down.
static INT32 system_ok; // ok to power-down / write nvram?

struct flash_struct {
	INT32   present;
	UINT8   manufacturer_id;
	UINT8   device_id;
	UINT8   org_data[16];
	UINT8   command[2];
	INT32   state;

	UINT8   *data;
} m_flash_chip[2];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvInputs[2];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static struct BurnInputInfo NgpInputList[] = {
	{"Power",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"Option",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Config",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Ngp)


static struct BurnDIPInfo NgpDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0x00, NULL		},
};

STDDIPINFO(Ngp)

static struct BurnDIPInfo NgpcDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    2, "Pocket Color"			},
	{0x00, 0x01, 0x10, 0x10, "Force B&W"			},
	{0x00, 0x01, 0x10, 0x00, "Normal"			    },
};

STDDIPINFO(Ngpc)

static UINT8 ngp_io_r(UINT8 offset)
{
	offset &= 0x3f;

	switch (offset)
	{
		case 0x30:
			return DrvInputs[0];

		case 0x31:
			return (DrvInputs[1] & 0x01) | 0x02;
	}

	return io_reg[offset];
}

static void ngp_io_w(UINT8 offset, UINT8 data)
{
	offset &= 0x3f;

	switch (offset)
	{
		case 0x20:
		case 0x21:
			if (io_reg[0x38] == 0x55 && io_reg[0x39] == 0xaa) {
				t6w28Write(offset & 1, data);
			}
		break;

		case 0x22:
			DACWrite(1, data);
		break;

		case 0x23:
			DACWrite(0, data);
		break;

		case 0x36:
		case 0x37:
		break;

		case 0x38: // sound enable
			switch (data) {
				case 0x55:
					t6w28Enable(1);
				break;

				case 0xaa:
					t6w28Enable(0);
				break;
			}
		break;

		case 0x39: // soundcpu enable
			switch (data) {
				case 0x55:
					ZetSetRESETLine(0);
				break;

				case 0xaa:
					ZetSetRESETLine(1);
				break;
			}
		break;

		case 0x3a:
			ZetNmi();
		break;
	}

	io_reg[offset] = data;
}

enum flash_state
{
	F_READ,                     /* xxxx F0 or 5555 AA 2AAA 55 5555 F0 */
	F_PROG1,
	F_PROG2,
	F_COMMAND,
	F_ID_READ,                  /* 5555 AA 2AAA 55 5555 90 */
	F_AUTO_PROGRAM,             /* 5555 AA 2AAA 55 5555 A0 address data */
	F_AUTO_CHIP_ERASE,          /* 5555 AA 2AAA 55 5555 80 5555 AA 2AAA 55 5555 10 */
	F_AUTO_BLOCK_ERASE,         /* 5555 AA 2AAA 55 5555 80 5555 AA 2AAA 55 block_address 30 */
	F_BLOCK_PROTECT             /* 5555 AA 2AAA 55 5555 9A 5555 AA 2AAA 55 5555 9A */
};

static void flash_w( int which, UINT32 offset, UINT8 data )
{
	if ( ! m_flash_chip[which].present )
		return;

	offset &= 0x1fffff;

	switch( m_flash_chip[which].state )
	{
	case F_READ:
		if ( offset == 0x5555 && data == 0xaa )
			m_flash_chip[which].state = F_PROG1;
		m_flash_chip[which].command[0] = 0;
		break;
	case F_PROG1:
		if ( offset == 0x2aaa && data == 0x55 )
			m_flash_chip[which].state = F_PROG2;
		else
			m_flash_chip[which].state = F_READ;
		break;
	case F_PROG2:
		if ( data == 0x30 )
		{
			if ( m_flash_chip[which].command[0] == 0x80 )
			{
				int size = 0x10000;
				UINT8 *block = m_flash_chip[which].data;

				m_flash_chip[which].state = F_AUTO_BLOCK_ERASE;
				switch( m_flash_chip[which].device_id )
				{
				case 0xab:
					if ( offset < 0x70000 )
						block = m_flash_chip[which].data + ( offset & 0x70000 );
					else
					{
						if ( offset & 0x8000 )
						{
							if ( offset & 0x4000 )
							{
								block = m_flash_chip[which].data + ( offset & 0x7c000 );
								size = 0x4000;
							}
							else
							{
								block = m_flash_chip[which].data + ( offset & 0x7e000 );
								size = 0x2000;
							}
						}
						else
						{
							block = m_flash_chip[which].data + ( offset & 0x78000 );
							size = 0x8000;
						}
					}
					break;
				case 0x2c:
					if ( offset < 0xf0000 )
						block = m_flash_chip[which].data + ( offset & 0xf0000 );
					else
					{
						if ( offset & 0x8000 )
						{
							if ( offset & 0x4000 )
							{
								block = m_flash_chip[which].data + ( offset & 0xfc000 );
								size = 0x4000;
							}
							else
							{
								block = m_flash_chip[which].data + ( offset & 0xfe000 );
								size = 0x2000;
							}
						}
						else
						{
							block = m_flash_chip[which].data + ( offset & 0xf8000 );
							size = 0x8000;
						}
					}
					break;
				case 0x2f:
					if ( offset < 0x1f0000 )
						block = m_flash_chip[which].data + ( offset & 0x1f0000 );
					else
					{
						if ( offset & 0x8000 )
						{
							if ( offset & 0x4000 )
							{
								block = m_flash_chip[which].data + ( offset & 0x1fc000 );
								size = 0x4000;
							}
							else
							{
								block = m_flash_chip[which].data + ( offset & 0x1fe000 );
								size = 0x2000;
							}
						}
						else
						{
							block = m_flash_chip[which].data + ( offset & 0x1f8000 );
							size = 0x8000;
						}
					}
					break;
				}
				memset( block, 0xFF, size );
			}
			else
				m_flash_chip[which].state = F_READ;
		}
		else if ( offset == 0x5555 )
		{
			switch( data )
			{
			case 0x80:
				m_flash_chip[which].command[0] = 0x80;
				m_flash_chip[which].state = F_COMMAND;
				break;
			case 0x90:
				m_flash_chip[which].data[0x1fc000] = m_flash_chip[which].manufacturer_id;
				m_flash_chip[which].data[0xfc000] = m_flash_chip[which].manufacturer_id;
				m_flash_chip[which].data[0x7c000] = m_flash_chip[which].manufacturer_id;
				m_flash_chip[which].data[0] = m_flash_chip[which].manufacturer_id;
				m_flash_chip[which].data[0x1fc001] = m_flash_chip[which].device_id;
				m_flash_chip[which].data[0xfc001] = m_flash_chip[which].device_id;
				m_flash_chip[which].data[0x7c001] = m_flash_chip[which].device_id;
				m_flash_chip[which].data[1] = m_flash_chip[which].device_id;
				m_flash_chip[which].data[0x1fc002] = 0x02;
				m_flash_chip[which].data[0xfc002] = 0x02;
				m_flash_chip[which].data[0x7c002] = 0x02;
				m_flash_chip[which].data[2] = 0x02;
				m_flash_chip[which].data[0x1fc003] = 0x80;
				m_flash_chip[which].data[0xfc003] = 0x80;
				m_flash_chip[which].data[0x7c003] = 0x80;
				m_flash_chip[which].data[3] = 0x80;
				m_flash_chip[which].state = F_ID_READ;
				break;
			case 0x9a:
				if ( m_flash_chip[which].command[0] == 0x9a )
					m_flash_chip[which].state = F_BLOCK_PROTECT;
				else
				{
					m_flash_chip[which].command[0] = 0x9a;
					m_flash_chip[which].state = F_COMMAND;
				}
				break;
			case 0xa0:
				m_flash_chip[which].state = F_AUTO_PROGRAM;
				break;
			case 0xf0:
			default:
				m_flash_chip[which].state = F_READ;
				break;
			}
		}
		else
			m_flash_chip[which].state = F_READ;
		break;
	case F_COMMAND:
		if ( offset == 0x5555 && data == 0xaa )
			m_flash_chip[which].state = F_PROG1;
		else
			m_flash_chip[which].state = F_READ;
		break;
	case F_ID_READ:
		if ( offset == 0x5555 && data == 0xaa )
			m_flash_chip[which].state = F_PROG1;
		else
			m_flash_chip[which].state = F_READ;
		m_flash_chip[which].command[0] = 0;
		break;
	case F_AUTO_PROGRAM:
		/* Only 1 -> 0 changes can be programmed */
		m_flash_chip[which].data[offset] = m_flash_chip[which].data[offset] & data;
		m_flash_chip[which].state = F_READ;
		break;
	case F_AUTO_CHIP_ERASE:
		m_flash_chip[which].state = F_READ;
		break;
	case F_AUTO_BLOCK_ERASE:
		m_flash_chip[which].state = F_READ;
		break;
	case F_BLOCK_PROTECT:
		m_flash_chip[which].state = F_READ;
		break;
	}

	if ( m_flash_chip[which].state == F_READ )
	{
		/* Exit command/back to normal operation*/
		m_flash_chip[which].data[0] = m_flash_chip[which].org_data[0];
		m_flash_chip[which].data[1] = m_flash_chip[which].org_data[1];
		m_flash_chip[which].data[2] = m_flash_chip[which].org_data[2];
		m_flash_chip[which].data[3] = m_flash_chip[which].org_data[3];
		m_flash_chip[which].data[0x7c000] = m_flash_chip[which].org_data[4];
		m_flash_chip[which].data[0x7c001] = m_flash_chip[which].org_data[5];
		m_flash_chip[which].data[0x7c002] = m_flash_chip[which].org_data[6];
		m_flash_chip[which].data[0x7c003] = m_flash_chip[which].org_data[7];
		m_flash_chip[which].data[0xfc000] = m_flash_chip[which].org_data[8];
		m_flash_chip[which].data[0xfc001] = m_flash_chip[which].org_data[9];
		m_flash_chip[which].data[0xfc002] = m_flash_chip[which].org_data[10];
		m_flash_chip[which].data[0xfc003] = m_flash_chip[which].org_data[11];
		m_flash_chip[which].data[0x1fc000] = m_flash_chip[which].org_data[12];
		m_flash_chip[which].data[0x1fc001] = m_flash_chip[which].org_data[13];
		m_flash_chip[which].data[0x1fc002] = m_flash_chip[which].org_data[14];
		m_flash_chip[which].data[0x1fc003] = m_flash_chip[which].org_data[15];
		m_flash_chip[which].command[0] = 0;
	}
}

static void ngp_seconds_callback()
{
	io_reg[0x16] += 1;
	if ((io_reg[0x16] & 0x0f) == 0x0a) {
		io_reg[0x16] += 0x06;
	}

	if (io_reg[0x16] >= 0x60) {
		io_reg[0x16] = 0;
		io_reg[0x15] += 1;
		if ((io_reg[0x15] & 0x0f) == 0x0a) {
			io_reg[0x15] += 0x06;
		}

		if (io_reg[0x15] >= 0x60)	{
			io_reg[0x15] = 0;
			io_reg[0x14] += 1;
			if ((io_reg[0x14] & 0x0f) == 0x0a) {
				io_reg[0x14] += 0x06;
			}

			if (io_reg[0x14] == 0x24)	{
				io_reg[0x14] = 0;
			}
		}
	}
}

static void syncsnd()
{
	INT32 cyc = (tlcs900TotalCycles() / 2) - ZetTotalCycles();
	if (cyc > 0) {
		ZetRun(cyc);
	}
}

static void ngp_main_write(UINT32 address, UINT8 data)
{
	if ((address & 0xffffc0) == 0x000080) {
		// dac and t6w28 are synced to the z80, so we must sync z80 to main
		// before accessing sound chips from main.
		syncsnd();
		ngp_io_w(address,data);
		return;
	}

	if ((address & 0xffc000) == 0x008000) {
		k1ge_w(address, data);
		return;
	}

	if ((address & 0xe00000) == 0x200000) {
		flash_w(0, address, data);
		return;
	}

	if ((address & 0xe00000) == 0x800000) {
		flash_w(1, address, data);
		return;
	}
}

static UINT8 ngp_main_read(UINT32 address)
{
	if ((address & 0xffffc0) == 0x000080) {
		return ngp_io_r(address);
	}

	if ((address & 0xffc000) == 0x008000) {
		return k1ge_r(address);
	}

	return 0;
}

static void ngp_tlcs900_to3(UINT32, UINT8 data)
{
	if (data && !old_to3)
		ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);

	old_to3 = data;
}

static void __fastcall ngp_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x4000:
		case 0x4001:
			t6w28Write(address & 1, data);
		return;

		case 0x8000:
			io_reg[0x3c] = data;
		return;

		case 0xc000:
			tlcs900SetIRQLine(TLCS900_INT5, CPU_IRQSTATUS_ACK);
		return;
	}
}

static UINT8 __fastcall ngp_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x8000:
			return io_reg[0x3c];
	}

	return 0;
}

static void __fastcall ngp_sound_write_port(UINT16 , UINT8 )
{
	ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);

	tlcs900SetIRQLine(TLCS900_INT5, CPU_IRQSTATUS_NONE);
}

static void ngp_vblank_pin_w(INT32, INT32 data)
{
	tlcs900SetIRQLine(TLCS900_INT4, (data) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE );
}

static void ngp_hblank_pin_w(INT32, INT32 data)
{
	tlcs900SetIRQLine(TLCS900_TIO, (data) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE );
}

static UINT32 round_pow2(UINT32 in) {
    if (in == 0) {
        return 1;
    }

	UINT32 r = 1;

	while (r < in) {
		r <<= 1;
	}

    return r;
}

static void initialize_flash_config()
{
	struct BurnRomInfo ri;
	BurnDrvGetRomInfo(&ri, 0); // cart rom

	bprintf (0, _T("Cart size: %5.5x, rounded: %x\n"), ri.nLen, round_pow2(ri.nLen));

	memset(m_flash_chip, 0, sizeof(m_flash_chip));

	m_flash_chip[0].present = 1;
	m_flash_chip[0].state = F_READ;
	m_flash_chip[0].manufacturer_id = 0x98;

	ri.nLen = round_pow2(ri.nLen);

	switch ( ri.nLen )
	{
		case 0x08000:
		case 0x40000:
		case 0x80000:
			m_flash_chip[0].device_id = 0xab;
		break;

		case 0x100000:
			m_flash_chip[0].device_id = 0x2c;
		break;

		case 0x200000:
			m_flash_chip[0].device_id = 0x2f;
		break;

		case 0x400000:
			m_flash_chip[0].device_id = 0x2f;
			m_flash_chip[1].device_id = 0x2f;
			m_flash_chip[1].manufacturer_id = 0x98;
			m_flash_chip[1].present = 1;
			m_flash_chip[1].state = F_READ;
		break;
	}

	for (INT32 i = 0; i < 2; i++)
	{
		m_flash_chip[i].data = DrvCartROM + i * 0x200000;
		m_flash_chip[i].org_data[0] = m_flash_chip[i].data[0];
		m_flash_chip[i].org_data[1] = m_flash_chip[i].data[1];
		m_flash_chip[i].org_data[2] = m_flash_chip[i].data[2];
		m_flash_chip[i].org_data[3] = m_flash_chip[i].data[3];
		m_flash_chip[i].org_data[4] = m_flash_chip[i].data[0x7c000];
		m_flash_chip[i].org_data[5] = m_flash_chip[i].data[0x7c001];
		m_flash_chip[i].org_data[6] = m_flash_chip[i].data[0x7c002];
		m_flash_chip[i].org_data[7] = m_flash_chip[i].data[0x7c003];
		m_flash_chip[i].org_data[8] = m_flash_chip[i].data[0xfc000];
		m_flash_chip[i].org_data[9] = m_flash_chip[i].data[0xfc001];
		m_flash_chip[i].org_data[10] = m_flash_chip[i].data[0xfc002];
		m_flash_chip[i].org_data[11] = m_flash_chip[i].data[0xfc003];
		m_flash_chip[i].org_data[12] = m_flash_chip[i].data[0x1fc000];
		m_flash_chip[i].org_data[13] = m_flash_chip[i].data[0x1fc001];
		m_flash_chip[i].org_data[14] = m_flash_chip[i].data[0x1fc002];
		m_flash_chip[i].org_data[15] = m_flash_chip[i].data[0x1fc003];
	}
}

static INT32 DrvDoReset()
{
	tlcs900Open(0);
	tlcs900Reset();
	tlcs900Close();

	ZetOpen(0);
	ZetReset();
	ZetSetRESETLine(1);
	DACReset();
	ZetClose();

	initialize_flash_config();
	k1geReset();

	t6w28Reset();

	old_to3 = 0;
	timer_time = 0;
	previous_start = 0;
	memset(io_reg, 0, sizeof(io_reg));

	tlcs900SetPC(0xff1800); // boot-up addr

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvBiosROM		= Next; Next += 0x010000;
	DrvCartROM		= Next; Next += 0x400000;
	DrvCartBak		= Next; Next += 0x400000;
	DrvCartTmp		= Next; Next += 0x400000;

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x003000;
	DrvShareRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 nvram_load_save(INT32 save)
{
	TCHAR szFilename[MAX_PATH];
	_stprintf(szFilename, _T("%s%s.nvram"), szAppEEPROMPath, (color_mode) ? _T("ngpc") : _T("ngp"));

	if (save) {
		FILE *fp = _tfopen(szFilename, _T("wb"));
		if (!fp) return -1;

		fwrite(DrvMainRAM, 1, 0x3000, fp);
		fclose(fp);
		bprintf(0, _T("*   NeoGeo Pocket: nvram save OK!\n"));
	} else {
		FILE *fp = _tfopen(szFilename, _T("rb"));
		if (!fp) return -1;

		fread(DrvMainRAM, 1, 0x3000, fp);
		fclose(fp);
		bprintf(0, _T("*   NeoGeo Pocket: nvram load OK!\n"));
	}

	return 0;
}

static INT32 DrvInit()
{
	system_ok = 0;

	BurnAllocMemIndex();

	color_mode = (BurnDrvGetHardwareCode() & HARDWARE_SNK_NGPC) == HARDWARE_SNK_NGPC;
	if (DrvDips[0] & 0x10) color_mode = 0; // ngpc - force b&w

	memset (DrvCartROM, 0xff, 0x400000);

	{
		if (BurnLoadRom(DrvBiosROM,	color_mode ? 0x81 : 0x80, 1)) return 1;
		if (BurnLoadRom(DrvCartROM,	0, 1)) return 1;
		memcpy (DrvCartBak, DrvCartROM, 0x400000);
	}

	tlcs900Init(0);
	tlcs900Open(0);
	tlcs900MapMemory(DrvMainRAM,			0x004000, 0x006fff, MAP_RAM);
	tlcs900MapMemory(DrvShareRAM,			0x007000, 0x007fff, MAP_RAM);
	tlcs900MapMemory(DrvCartROM,			0x200000, 0x3fffff, MAP_ROM);
	tlcs900MapMemory(DrvCartROM + 0x200000,	0x800000, 0x9fffff, MAP_ROM);
	tlcs900MapMemory(DrvBiosROM,			0xff0000, 0xffffff, MAP_ROM);
	tlcs900SetWriteHandler(ngp_main_write);
	tlcs900SetReadHandler(ngp_main_read);
	tlcs900SetToxHandler(3, ngp_tlcs900_to3);
	tlcs900Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvShareRAM,				0x0000, 0x0fff, MAP_RAM);
	ZetSetWriteHandler(ngp_sound_write);
	ZetSetReadHandler(ngp_sound_read);
	ZetSetOutHandler(ngp_sound_write_port);
	ZetClose();

	t6w28Init(6144200/2, ZetTotalCycles, 6144200/2, 1);
	t6w28SetVolume(0.50);

	DACInit(0, 0, 0, ZetTotalCycles, 6144200/2); // left
	DACInit(1, 0, 0, ZetTotalCycles, 6144200/2); // right
	DACSetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);
	DACSetRoute(1, 0.75, BURN_SND_ROUTE_BOTH);

	k1geInit(color_mode, ngp_vblank_pin_w, ngp_hblank_pin_w);

	GenericTilesInit();

	DrvDoReset();

	nvram_load_save(0 /* load */);

	blind_mode = 0;
	system_ok = 1;

	return 0;
}

static INT32 DrvFrame(); // forward use.

static void power_down_system()
{
	if (io_reg[0] & 4) {
		bprintf(0, _T("*   NeoGeo Pocket: Machine already powered off at exit.\n"));
		return; // already off.
	}

	blind_mode = 1;

	INT32 break_out = 0;
	INT32 frames_off = 0;
	do {
		switch (break_out) {
			case 0: DrvJoy2[0] = 1; break;
			case 4: DrvJoy2[0] = 0; break;
		}
		DrvFrame();
		break_out++;
		if (io_reg[0] & 4) { frames_off++; if (frames_off >= 10) break; }
	} while (break_out < 240);
	bprintf(0, _T("*   NeoGeo Pocket: System shut-down in %d frames, %d frames off.\n"), break_out, frames_off);
}

static INT32 DrvExit()
{
	if (system_ok) {
		power_down_system();
		nvram_load_save(1 /* save */);
	}

	GenericTilesExit();

	k1geExit();
	DACExit();
	t6w28Exit();
	tlcs900Exit();
	ZetExit();

	BurnFreeMemIndex();

	system_ok = 0;

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset && !blind_mode) {
		DrvDoReset();
	}

	tlcs900NewFrame();
	ZetNewFrame();

	{
		previous_start = DrvInputs[1];
		DrvInputs[0] = 0;
		DrvInputs[1] = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 199; // scanlines
	INT32 nCyclesTotal[2] = { (INT32)(6144000 / 59.95), (INT32)(6144000 / 2 / 59.95) }; // 59.95hz
	INT32 nCyclesDone[2] = { 0, 0 };

	tlcs900Open(0);
	ZetOpen(0);

	if (DrvInputs[1] != previous_start)
	{
		if (io_reg[0x33] & 0x04) // power up, or power down.  1 or the other. - Carl(ATHF)
		{
			tlcs900SetIRQLine(TLCS900_NMI, (DrvInputs[1] & 0x01) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK );
		}
	}

	for (INT32 i = 0; i < nInterleave; i++)
	{
		k1ge_scanline_timer_callback(i);

		nCyclesDone[0] += tlcs900Run(480);
		// hblank starts
		k1ge_hblank_on_timer_callback();

		nCyclesDone[0] += tlcs900Run(38);
		// hblank ends

		CPU_RUN(1, Zet);
	}

	if (pBurnSoundOut && !blind_mode) {
		DACUpdate(pBurnSoundOut, nBurnSoundLen); // dac w/super dc offset, fix.
		BurnSoundDCFilter();
		t6w28Update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw && !blind_mode) {
		BurnDrvRedraw();
	}

	timer_time++;
	if (timer_time == 60) {
		ngp_seconds_callback();
		timer_time = 0;
	}

	ZetClose();
	tlcs900Close();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));  // Also saved globally @ DrvExit (ngp[c].nvram)
		ba.Data	  = DrvMainRAM;
		ba.nLen	  = 0x3000;
		ba.szName = "Main Ram";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvShareRAM;
		ba.nLen	  = 0x1000;
		ba.szName = "Shared Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		tlcs900Scan(nAction);
		ZetScan(nAction);
		k1geScan(nAction, pnMin);
		t6w28Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		// scan flash structures, up to but not including the pointer (*data)
		ScanVar(&m_flash_chip[0], STRUCT_SIZE_HELPER(flash_struct, state), "flash0");
		ScanVar(&m_flash_chip[1], STRUCT_SIZE_HELPER(flash_struct, state), "flash1");

		SCAN_VAR(previous_start);
		SCAN_VAR(timer_time);
		SCAN_VAR(old_to3);
		SCAN_VAR(io_reg);
	}

	if (nAction & ACB_NVRAM && ~nAction & ACB_RUNAHEAD)
	{
		INT32 size = 0;

		if (nAction & ACB_READ) // read from game -> write to state
		{
			for (INT32 i = 0; i < 0x400000; i++) {
				if (DrvCartROM[i] != DrvCartBak[i]) {
					DrvCartTmp[size + 0] = DrvCartROM[i];
					DrvCartTmp[size + 1] = (i >> 0) & 0xff;
					DrvCartTmp[size + 2] = (i >> 8) & 0xff;
					DrvCartTmp[size + 3] = (i >> 16) & 0xff;
					size += 4;
				}
			}

			SCAN_VAR(size);

			memset(&ba, 0, sizeof(ba));
			ba.Data	  = DrvCartTmp;
			ba.nLen	  = size;
			ba.szName = "Flash ROM Diff";
			BurnAcb(&ba);
		}

		if (nAction & ACB_WRITE && ~nAction & ACB_RUNAHEAD) // write to game <- read from state
		{
			SCAN_VAR(size);

			memset(&ba, 0, sizeof(ba));
			ba.Data	  = DrvCartTmp;
			ba.nLen	  = size;
			ba.szName = "Flash ROM Diff";
			BurnAcb(&ba);

			for (INT32 i = 0; i < size; i+=4) {
				INT32 offset = (DrvCartTmp[i + 1] << 0) | (DrvCartTmp[i + 2] << 8) | (DrvCartTmp[i + 3] << 16);

				DrvCartROM[offset] = DrvCartTmp[i + 0];
			}
		}
	}

	return 0;
}


static INT32 NgpGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		if (i == 1 && BurnDrvGetTextA(DRV_BOARDROM)) {
			pszGameName = BurnDrvGetTextA(DRV_BOARDROM);
		} else {
			pszGameName = BurnDrvGetTextA(DRV_PARENT);
		}
	}

	if (pszGameName == NULL || i > 2) {
		*pszName = NULL;
		return 1;
	}

	szFilename[0] = '\0';
	if (pszGameName[3] == '_') { strcpy(szFilename, pszGameName + 4); }

	*pszName = szFilename;

	return 0;
}

// NeoGeo Pocket (Bios)
static struct BurnRomInfo emptyRomDesc[] = { { "", 0, 0, 0 }, }; // for bios handling

static struct BurnRomInfo ngpc_ngpRomDesc[] = {
	{ "SNK Neo-Geo Pocket BIOS (1998)(SNK)(en-ja).bin",			0x10000, 0x6232df8d, BRF_PRG | BRF_BIOS },	// 0x80 - Neo-Geo Pocket (b&w) bios
	{ "SNK Neo-Geo Pocket Color BIOS (1999)(SNK)(en-ja).bin",	0x10000, 0x6EEB6F40, BRF_PRG | BRF_BIOS },	// 0x81 - Neo-Geo Pocket Color bios
};

STD_ROM_PICK(ngpc_ngp)
STD_ROM_FN(ngpc_ngp)

struct BurnDriver BurnDrvngpc_ngp = {
	"ngp_ngp", NULL, NULL, NULL, "1998",
	"NeoGeo Pocket (Bios)\0", "BIOS only", "SNK", "NeoGeo Pocket",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_SNK_NGP | HARDWARE_SNK_NGPC, GBF_BIOS, 0,
	NULL, ngpc_ngpRomInfo, ngpc_ngpRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Bakumatsu Rouman Tokubetsu Hen - Gekka no Kenshi - Tsuki ni Saku Hana, Chiri Yuku Hana (Japan)
static struct BurnRomInfo ngpc_bakumatsRomDesc[] = {
	{ "Bakumatsu Rouman Tokubetsu Hen (Japan)(2000)(SNK).ngp", 0x200000, 0x12afacb0, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_bakumats, ngpc_bakumats, ngpc_ngp)
STD_ROM_FN(ngpc_bakumats)

struct BurnDriver BurnDrvngpc_bakumats = {
	"ngp_bakumats", "ngp_lastblad", "ngp_ngp", NULL, "2000",
	"Bakumatsu Rouman Tokubetsu Hen (Japan)\0", NULL, "SNK", "NeoGeo Pocket Color",
	L"Bakumatsu Rouman Tokubetsu Hen (Japan)\0\u5e55\u672b\u6d6a\u6f2b\u7279\u5225\u7de8 \u6708\u83ef\u306e\u5263\u58eb\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_bakumatsRomInfo, ngpc_bakumatsRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Big Bang Pro Wrestling (Japan)
static struct BurnRomInfo ngpc_bigbangRomDesc[] = {
	{ "Big Bang Pro Wrestling (Japan)(2000)(SNK).ngp", 0x200000, 0xb783c372, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_bigbang, ngpc_bigbang, ngpc_ngp)
STD_ROM_FN(ngpc_bigbang)

struct BurnDriver BurnDrvngpc_bigbang = {
	"ngp_bigbang", "ngp_wrestmad", "ngp_ngp", NULL, "2000",
	"Big Bang Pro Wrestling (Japan)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_bigbangRomInfo, ngpc_bigbangRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Bikkuriman 2000: Viva! Pocket Festiva! (Japan)
static struct BurnRomInfo ngpc_bikkuriRomDesc[] = {
	{ "Bikkuriman 2000 - Viva! Pocket Festiva! (Japan)(2000)(Sega Toys).ngp", 0x100000, 0x7a2d7635, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_bikkuri, ngpc_bikkuri, ngpc_ngp)
STD_ROM_FN(ngpc_bikkuri)

struct BurnDriver BurnDrvngpc_bikkuri = {
	"ngp_bikkuri", NULL, "ngp_ngp", NULL, "2000",
	"Bikkuriman 2000: Viva! Pocket Festiva! (Japan)\0", NULL, "Sega Toys", "NeoGeo Pocket Color",
	L"Bikkuriman 2000: Viva! Pocket Festiva! (Japan)\0\u30d3\u30c3\u30af\u30ea\u30de\u30f32000 \u30d3\u30d0!\u30dd\u30b1\u30c3\u30c8\u30d5\u30a7\u30b9\u30c1\u30d0\u30a1!\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_ADV | GBF_CASINO, 0,
	NgpGetZipName, ngpc_bikkuriRomInfo, ngpc_bikkuriRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// BioMotor Unitron (Euro, USA)
static struct BurnRomInfo ngpc_biomotorRomDesc[] = {
	{ "BioMotor Unitron (Euro, USA)(1999)(SNK - Yumekobo).ngp", 0x100000, 0x3807df4f, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_biomotor, ngpc_biomotor, ngpc_ngp)
STD_ROM_FN(ngpc_biomotor)

struct BurnDriver BurnDrvngpc_biomotor = {
	"ngp_biomotor", NULL, "ngp_ngp", NULL, "1999",
	"BioMotor Unitron (Euro, USA)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_biomotorRomInfo, ngpc_biomotorRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// BioMotor Unitron (Japan)
static struct BurnRomInfo ngpc_biomotorjRomDesc[] = {
	{ "BioMotor Unitron (Japan)(1999)(SNK - Yumekobo).ngp", 0x100000, 0xecee99f3, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_biomotorj, ngpc_biomotorj, ngpc_ngp)
STD_ROM_FN(ngpc_biomotorj)

struct BurnDriver BurnDrvngpc_biomotorj = {
	"ngp_biomotorj", "ngp_biomotor", "ngp_ngp", NULL, "1999",
	"BioMotor Unitron (Japan)\0", NULL, "SNK - Yumekobo", "NeoGeo Pocket Color",
	L"BioMotor Unitron (Japan)\0\u30d0\u30a4\u30aa\u30e2\u30fc\u30bf\u30fc \u30e6\u30cb\u30c8\u30ed\u30f3\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_biomotorjRomInfo, ngpc_biomotorjRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Baseball Stars - Pocket Sports Series (Euro, Japan)
static struct BurnRomInfo ngp_bstarsRomDesc[] = {
	{ "Baseball Stars - Pocket Sports Series (Euro, Japan)(1998)(SNK).ngp", 0x100000, 0x4781ae84, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngp_bstars, ngp_bstars, ngpc_ngp)
STD_ROM_FN(ngp_bstars)

struct BurnDriver BurnDrvngp_bstars = {
	"ngp_bstars", NULL, "ngp_ngp", NULL, "1998",
	"Baseball Stars - Pocket Sports Series (Euro, Japan)\0", NULL, "SNK", "NeoGeo Pocket",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGP, GBF_SPORTSMISC, 0,
	NgpGetZipName, ngp_bstarsRomInfo, ngp_bstarsRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x8,
	160, 152, 4, 3
};

// Baseball Stars Color - Pocket Sports Series (World)
static struct BurnRomInfo ngpc_bstarscRomDesc[] = {
	{ "Baseball Stars Color - Pocket Sports Series (World)(1999)(SNK).ngp", 0x100000, 0xffa7e88d, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_bstarsc, ngpc_bstarsc, ngpc_ngp)
STD_ROM_FN(ngpc_bstarsc)

struct BurnDriver BurnDrvngpc_bstarsc = {
	"ngp_bstarsc", NULL, "ngp_ngp", NULL, "1999",
	"Baseball Stars Color - Pocket Sports Series (World)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_SPORTSMISC, 0,
	NgpGetZipName, ngpc_bstarscRomInfo, ngpc_bstarscRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Bust-A-Move Pocket (USA)
static struct BurnRomInfo ngpc_bamRomDesc[] = {
	{ "Bust-A-Move Pocket (USA)(1999)(SNK).ngp", 0x100000, 0x85fe6ae3, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_bam, ngpc_bam, ngpc_ngp)
STD_ROM_FN(ngpc_bam)

struct BurnDriver BurnDrvngpc_bam = {
	"ngp_bam", "ngp_pbobble", "ngp_ngp", NULL, "1999",
	"Bust-A-Move Pocket (USA)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_bamRomInfo, ngpc_bamRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Bust-A-Move Pocket (USA, Prototype)
static struct BurnRomInfo ngpc_bampRomDesc[] = {
	{ "Bust-A-Move Pocket (USA, Proto)(1999)(SNK).ngp", 0x100000, 0x78a1361d, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_bamp, ngpc_bamp, ngpc_ngp)
STD_ROM_FN(ngpc_bamp)

struct BurnDriver BurnDrvngpc_bamp = {
	"ngp_bamp", "ngp_pbobble", "ngp_ngp", NULL, "1999",
	"Bust-A-Move Pocket (USA, Prototype)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_bampRomInfo, ngpc_bampRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Cool Boarders Pocket (Euro, Japan)
static struct BurnRomInfo ngpc_coolboarRomDesc[] = {
	{ "Cool Boarders Pocket (Euro, Japan)(2000)(UEP Systems).ngp", 0x100000, 0x833f9c22, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_coolboar, ngpc_coolboar, ngpc_ngp)
STD_ROM_FN(ngpc_coolboar)

struct BurnDriver BurnDrvngpc_coolboar = {
	"ngp_coolboar", NULL, "ngp_ngp", NULL, "2000",
	"Cool Boarders Pocket (Euro, Japan)\0", NULL, "UEP Systems", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_SPORTSMISC, 0,
	NgpGetZipName, ngpc_coolboarRomInfo, ngpc_coolboarRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Cool Cool Jam (Euro, USA, Sample-Demo)
static struct BurnRomInfo ngpc_coolcoolRomDesc[] = {
	{ "Cool Cool Jam (Euro, USA, Sample-Demo)(2000)(SNK).ngp", 0x200000, 0x4d8d27f0, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_coolcool, ngpc_coolcool, ngpc_ngp)
STD_ROM_FN(ngpc_coolcool)

struct BurnDriver BurnDrvngpc_coolcool = {
	"ngp_coolcool", NULL, "ngp_ngp", NULL, "2000",
	"Cool Cool Jam (Euro, USA, Sample-Demo)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_ACTION | GBF_ADV, 0,
	NgpGetZipName, ngpc_coolcoolRomInfo, ngpc_coolcoolRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};


// Cool Cool Jam (Japan)
static struct BurnRomInfo ngpc_coolcooljRomDesc[] = {
	{ "Cool Cool Jam (Japan)(2000)(SNK).ngp", 0x200000, 0x4fda5d8a, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_coolcoolj, ngpc_coolcoolj, ngpc_ngp)
STD_ROM_FN(ngpc_coolcoolj)

struct BurnDriver BurnDrvngpc_coolcoolj = {
	"ngp_coolcoolj", "ngp_coolcool", "ngp_ngp", NULL, "2000",
	"Cool Cool Jam (Japan)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_ACTION | GBF_ADV, 0,
	NgpGetZipName, ngpc_coolcooljRomInfo, ngpc_coolcooljRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Fantastic Night Dreams Cotton (Euro)
static struct BurnRomInfo ngpc_cottonRomDesc[] = {
	{ "Fantastic Night Dreams Cotton (Euro)(2000)(Success)", 0x100000, 0x1bf412f5, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_cotton, ngpc_cotton, ngpc_ngp)
STD_ROM_FN(ngpc_cotton)

struct BurnDriver BurnDrvngpc_cotton = {
	"ngp_cotton", NULL, "ngp_ngp", NULL, "2000",
	"Fantastic Night Dreams Cotton (Euro)\0", NULL, "Success", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_HORSHOOT, 0,
	NgpGetZipName, ngpc_cottonRomInfo, ngpc_cottonRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Fantastic Night Dreams Cotton (Japan)
static struct BurnRomInfo ngpc_cottonjRomDesc[] = {
	{ "Fantastic Night Dreams Cotton (Japan)(2000)(Success).ngp", 0x100000, 0xb8a12409, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_cottonj, ngpc_cottonj, ngpc_ngp)
STD_ROM_FN(ngpc_cottonj)

struct BurnDriver BurnDrvngpc_cottonj = {
	"ngp_cottonj", "ngp_cotton", "ngp_ngp", NULL, "2000",
	"Fantastic Night Dreams Cotton (Japan)\0", NULL, "Success", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_HORSHOOT, 0,
	NgpGetZipName, ngpc_cottonjRomInfo, ngpc_cottonjRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Crush Roller (World)
static struct BurnRomInfo ngpc_crushrolRomDesc[] = {
	{ "Crush Roller (World)(1999)(SNK - ADK).ngp", 0x100000, 0xf20ff9f2, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_crushrol, ngpc_crushrol, ngpc_ngp)
STD_ROM_FN(ngpc_crushrol)

struct BurnDriver BurnDrvngpc_crushrol = {
	"ngp_crushrol", NULL, "ngp_ngp", NULL, "1999",
	"Crush Roller (World)\0", NULL, "SNK - ADK Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_ACTION | GBF_MAZE, 0,
	NgpGetZipName, ngpc_crushrolRomInfo, ngpc_crushrolRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Dark Arms: Beast Buster 1999 (World)
static struct BurnRomInfo ngpc_darkarmsRomDesc[] = {
	{ "Dark Arms - Beast Buster 1999 (World)(1999)(SNK).ngp", 0x200000, 0x6f353f34, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_darkarms, ngpc_darkarms, ngpc_ngp)
STD_ROM_FN(ngpc_darkarms)

struct BurnDriver BurnDrvngpc_darkarms = {
	"ngp_darkarms", NULL, "ngp_ngp", NULL, "1999",
	"Dark Arms: Beast Buster 1999 (World)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_darkarmsRomInfo, ngpc_darkarmsRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Dark Arms: Beast Buster 1999 (Euro, USA, Prototype)
static struct BurnRomInfo ngpc_darkarmspRomDesc[] = {
	{ "Dark Arms - Beast Buster 1999 (Euro, USA, Proto)(1999)(SNK).ngp", 0x200000, 0x77caa548, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_darkarmsp, ngpc_darkarmsp, ngpc_ngp)
STD_ROM_FN(ngpc_darkarmsp)

struct BurnDriver BurnDrvngpc_darkarmsp = {
	"ngp_darkarmsp", "ngp_darkarms", "ngp_ngp", NULL, "1999",
	"Dark Arms: Beast Buster 1999 (Euro, USA, Prototype)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_darkarmspRomInfo, ngpc_darkarmspRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Delta Warp (Japan)
static struct BurnRomInfo ngpc_deltawrpRomDesc[] = {
	{ "Delta Warp (Japan)(2000)(IOSYS).ngp", 0x80000, 0xadd4fdff, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_deltawrp, ngpc_deltawrp, ngpc_ngp)
STD_ROM_FN(ngpc_deltawrp)

struct BurnDriver BurnDrvngpc_deltawrp = {
	"ngp_deltawrp", NULL, "ngp_ngp", NULL, "2000",
	"Delta Warp (Japan)\0", NULL, "IOSYS Co.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_deltawrpRomInfo, ngpc_deltawrpRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Densha de GO! 2 on Neo Geo Pocket (Japan)
static struct BurnRomInfo ngpc_dendego2RomDesc[] = {
	{ "Densha de GO! 2 on Neo Geo Pocket (Japan)(1999)(SNK).ngp", 0x400000, 0xd5ced9e9, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_dendego2, ngpc_dendego2, ngpc_ngp)
STD_ROM_FN(ngpc_dendego2)

struct BurnDriver BurnDrvngpc_dendego2 = {
	"ngp_dendego2", NULL, "ngp_ngp", NULL, "1999",
	"Densha de GO! 2 on Neo Geo Pocket (Japan)\0", NULL, "SNK", "NeoGeo Pocket Color",
	L"Densha de GO! 2 on Neo Geo Pocket (Japan)\0\u96fb\u8eca\u3067GO! 2\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_SIM, 0,
	NgpGetZipName, ngpc_dendego2RomInfo, ngpc_dendego2RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Densha de Go! 2 on Neo Geo Pocket (Japan, Prototype)
static struct BurnRomInfo ngpc_dendego2pRomDesc[] = {
	{ "Densha de Go! 2 on Neo Geo Pocket (Japan, Proto)(1999)(SNK).ngp", 0x400000, 0x2fc4623e, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_dendego2p, ngpc_dendego2p, ngpc_ngp)
STD_ROM_FN(ngpc_dendego2p)

struct BurnDriver BurnDrvngpc_dendego2p = {
	"ngp_dendego2p", "ngp_dendego2", "ngp_ngp", NULL, "1999",
	"Densha de Go! 2 on Neo Geo Pocket (Japan, Prototype)\0", NULL, "SNK", "NeoGeo Pocket Color",
	L"Densha de GO! 2 on Neo Geo Pocket (Japan, Prototype)\0\u96fb\u8eca\u3067GO! 2\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_SNK_NGPC, GBF_SIM, 0,
	NgpGetZipName, ngpc_dendego2pRomInfo, ngpc_dendego2pRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Dive Alert: Matt's Version (Euro, USA)
static struct BurnRomInfo ngpc_divealrmRomDesc[] = {
	{ "Dive Alert - Matt's Version (Euro, USA)(1999)(SNK - Sacnoth).ngp", 0x200000, 0xef75081b, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_divealrm, ngpc_divealrm, ngpc_ngp)
STD_ROM_FN(ngpc_divealrm)

struct BurnDriver BurnDrvngpc_divealrm = {
	"ngp_divealrm", NULL, "ngp_ngp", NULL, "1999",
	"Dive Alert: Matt's Version (Euro, USA)\0", NULL, "SNK - Sacnoth", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_divealrmRomInfo, ngpc_divealrmRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Dive Alert: Becky's Version (Euro, USA)
static struct BurnRomInfo ngpc_divealrbRomDesc[] = {
	{ "Dive Alert - Becky's Version (Euro, USA)(1999)(SNK - Sacnoth).ngp", 0x200000, 0x83db5772, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_divealrb, ngpc_divealrb, ngpc_ngp)
STD_ROM_FN(ngpc_divealrb)

struct BurnDriver BurnDrvngpc_divealrb = {
	"ngp_divealrb", NULL, "ngp_ngp", NULL, "1999",
	"Dive Alert: Becky's Version (Euro, USA)\0", NULL, "SNK - Sacnoth", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_divealrbRomInfo, ngpc_divealrbRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Dive Alert: Barn Hen (Japan)
static struct BurnRomInfo ngpc_divealrmjRomDesc[] = {
	{ "Dive Alert - Barn Hen (Japan)(1999)(SNK - Sacnoth).ngp", 0x200000, 0xc213941d, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_divealrmj, ngpc_divealrmj, ngpc_ngp)
STD_ROM_FN(ngpc_divealrmj)

struct BurnDriver BurnDrvngpc_divealrmj = {
	"ngp_divealrmj", "ngp_divealrm", "ngp_ngp", NULL, "1999",
	"Dive Alert: Barn Hen (Japan)\0", NULL, "SNK - Sacnoth", "NeoGeo Pocket Color",
	L"Dive Alert: Barn Hen (Japan)\0\u30c0\u30a4\u30f4\u30a2\u30e9\u30fc\u30c8 \u30d0\u30fc\u30f3\u7de8\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_divealrmjRomInfo, ngpc_divealrmjRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Dive Alert: Barn Hen (Japan, Prototype)
static struct BurnRomInfo ngpc_divealrmjpRomDesc[] = {
	{ "Dive Alert - Barn Hen (Japan, Proto)(1999)(SNK - Sacnoth).ngp", 0x200000, 0xd9817a88, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_divealrmjp, ngpc_divealrmjp, ngpc_ngp)
STD_ROM_FN(ngpc_divealrmjp)

struct BurnDriver BurnDrvngpc_divealrmjp = {
	"ngp_divealrmjp", "ngp_divealrm", "ngp_ngp", NULL, "1999",
	"Dive Alert: Barn Hen (Japan, Prototype)\0", NULL, "SNK - Sacnoth", "NeoGeo Pocket Color",
	L"Dive Alert: Barn Hen (Japan, Prototype)\0\u30c0\u30a4\u30f4\u30a2\u30e9\u30fc\u30c8 \u30d0\u30fc\u30f3\u7de8\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_divealrmjpRomInfo, ngpc_divealrmjpRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Dive Alert: Rebecca Hen (Japan)
static struct BurnRomInfo ngpc_divealrbjRomDesc[] = {
	{ "Dive Alert - Rebecca Hen (Japan)(1999)(SNK - Sacnoth).ngp", 0x200000, 0x3c4af4f5, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_divealrbj, ngpc_divealrbj, ngpc_ngp)
STD_ROM_FN(ngpc_divealrbj)

struct BurnDriver BurnDrvngpc_divealrbj = {
	"ngp_divealrbj", "ngp_divealrb", "ngp_ngp", NULL, "1999",
	"Dive Alert: Rebecca Hen (Japan)\0", NULL, "SNK - Sacnoth", "NeoGeo Pocket Color",
	L"Dive Alert: Rebecca Hen (Japan)\0\u30c0\u30a4\u30f4\u30a2\u30e9\u30fc\u30c8 \u30ec\u30d9\u30c3\u30ab\u7de8\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_divealrbjRomInfo, ngpc_divealrbjRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Dokodemo Mahjong (Japan)
static struct BurnRomInfo ngpc_dokodemoRomDesc[] = {
	{ "Dokodemo Mahjong (Japan)(1999)(SNK - ADK).ngp", 0x80000, 0x78c21b5e, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_dokodemo, ngpc_dokodemo, ngpc_ngp)
STD_ROM_FN(ngpc_dokodemo)

struct BurnDriver BurnDrvngpc_dokodemo = {
	"ngp_dokodemo", NULL, "ngp_ngp", NULL, "1999",
	"Dokodemo Mahjong (Japan)\0", NULL, "SNK - ADK Corp.", "NeoGeo Pocket Color",
	L"Dokodemo Mahjong (Japan)\0\u3069\u3053\u3067\u3082 \u9ebb\u96c0\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_MAHJONG, 0,
	NgpGetZipName, ngpc_dokodemoRomInfo, ngpc_dokodemoRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Dynamite Slugger (Euro, Japan)
static struct BurnRomInfo ngpc_dynaslugRomDesc[] = {
	{ "Dynamite Slugger (Euro, Japan)(2000)(SNK - ADK).ngp", 0x100000, 0x7f1779cd, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_dynaslug, ngpc_dynaslug, ngpc_ngp)
STD_ROM_FN(ngpc_dynaslug)

struct BurnDriver BurnDrvngpc_dynaslug = {
	"ngp_dynaslug", NULL, "ngp_ngp", NULL, "2000",
	"Dynamite Slugger (Euro, Japan)\0", NULL, "SNK - ADK Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_SPORTSMISC, 0,
	NgpGetZipName, ngpc_dynaslugRomInfo, ngpc_dynaslugRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Evolution: Eternal Dungeons (Euro)
static struct BurnRomInfo ngpc_evolutnRomDesc[] = {
	{ "Evolution - Eternal Dungeons (Euro)(2000)(SNK - ESP.).ngp", 0x200000, 0xbe47e531, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_evolutn, ngpc_evolutn, ngpc_ngp)
STD_ROM_FN(ngpc_evolutn)

struct BurnDriver BurnDrvngpc_evolutn = {
	"ngp_evolutn", NULL, "ngp_ngp", NULL, "2000",
	"Evolution: Eternal Dungeons (Euro)\0", NULL, "SNK - ESP.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_STRATEGY, 0,
	NgpGetZipName, ngpc_evolutnRomInfo, ngpc_evolutnRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Shinki Sekai Evolution: Hateshinai Dungeon (Japan)
static struct BurnRomInfo ngpc_evolutnjRomDesc[] = {
	{ "Shinki Sekai Evolution - Hateshinai Dungeon (Japan)(2000)(SNK - ESP.).ngp", 0x200000, 0xe006f42f, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_evolutnj, ngpc_evolutnj, ngpc_ngp)
STD_ROM_FN(ngpc_evolutnj)

struct BurnDriver BurnDrvngpc_evolutnj = {
	"ngp_evolutnj", "ngp_evolutn", "ngp_ngp", NULL, "2000",
	"Shinki Sekai Evolution: Hateshinai Dungeon (Japan)\0", NULL, "SNK - ESP.", "NeoGeo Pocket Color",
	L"Shinki Sekai Evolution: Hateshinai Dungeon (Japan)\0\u795e\u6a5f\u4e16\u754c Evolution \u30a8\u30f4\u30a9\u30ea\u30e5\u30fc\u30b7\u30e7\u30f3\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_STRATEGY, 0,
	NgpGetZipName, ngpc_evolutnjRomInfo, ngpc_evolutnjRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Faselei! (Euro)
static struct BurnRomInfo ngpc_faseleiRomDesc[] = {
	{ "Faselei! (Euro)(2000)(SNK - Sacnoth).ngp", 0x200000, 0xe705e30e, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_faselei, ngpc_faselei, ngpc_ngp)
STD_ROM_FN(ngpc_faselei)

struct BurnDriver BurnDrvngpc_faselei = {
	"ngp_faselei", NULL, "ngp_ngp", NULL, "2000",
	"Faselei! (Euro)\0", NULL, "SNK - Sacnoth", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_RPG | GBF_STRATEGY, 0,
	NgpGetZipName, ngpc_faseleiRomInfo, ngpc_faseleiRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Faselei! (Japan)
static struct BurnRomInfo ngpc_faseleijRomDesc[] = {
	{ "Faselei! (Japan)(1999)(SNK - Sacnoth).ngp", 0x200000, 0x8f585838, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_faseleij, ngpc_faseleij, ngpc_ngp)
STD_ROM_FN(ngpc_faseleij)

struct BurnDriver BurnDrvngpc_faseleij = {
	"ngp_faseleij", "ngp_faselei", "ngp_ngp", NULL, "1999",
	"Faselei! (Japan)\0", NULL, "SNK - Sacnoth", "NeoGeo Pocket Color",
	L"Faselei! (Japan)\0\u30d5\u30a1\u30fc\u30bc\u30e9\u30a4!\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_RPG | GBF_STRATEGY, 0,
	NgpGetZipName, ngpc_faseleijRomInfo, ngpc_faseleijRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Fatal Fury F-Contact - Pocket Fighting Series (World)
static struct BurnRomInfo ngpc_fatfuryRomDesc[] = {
	{ "Fatal Fury F-Contact - Pocket Fighting Series (World)(1999)(SNK).ngp", 0x200000, 0xa9119b5a, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_fatfury, ngpc_fatfury, ngpc_ngp)
STD_ROM_FN(ngpc_fatfury)

struct BurnDriver BurnDrvngpc_fatfury = {
	"ngp_fatfury", NULL, "ngp_ngp", NULL, "1999",
	"Fatal Fury F-Contact - Pocket Fighting Series (World)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_fatfuryRomInfo, ngpc_fatfuryRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Fatal Fury F-Contact - Pocket Fighting Series (World, Demo)
static struct BurnRomInfo ngpc_fatfurydRomDesc[] = {
	{ "Fatal Fury F-Contact - Pocket Fighting Series (World, Demo)(1999)(SNK).ngp", 0x200000, 0xe7e5d6e8, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_fatfuryd, ngpc_fatfuryd, ngpc_ngp)
STD_ROM_FN(ngpc_fatfuryd)

struct BurnDriver BurnDrvngpc_fatfuryd = {
	"ngp_fatfuryd", "ngp_fatfury", "ngp_ngp", NULL, "1999",
	"Fatal Fury F-Contact - Pocket Fighting Series (World, Demo)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_fatfurydRomInfo, ngpc_fatfurydRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Ganbare Neo Poke-kun (Japan)
static struct BurnRomInfo ngpc_ganbarenRomDesc[] = {
	{ "Ganbare Neo Poke-kun (Japan)(2000)(SNK).ngp", 0x200000, 0x6df986a3, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_ganbaren, ngpc_ganbaren, ngpc_ngp)
STD_ROM_FN(ngpc_ganbaren)

struct BurnDriver BurnDrvngpc_ganbaren = {
	"ngp_ganbaren", NULL, "ngp_ngp", NULL, "2000",
	"Ganbare Neo Poke-kun (Japan)\0", NULL, "SNK", "NeoGeo Pocket Color",
	L"Ganbare Neo Poke-kun (Japan)\0\u30ac\u30f3\u30d0\u30ec \u306d\u304a\u307d\u3051\u304f\u3093\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_ACTION | GBF_ADV, 0,
	NgpGetZipName, ngpc_ganbarenRomInfo, ngpc_ganbarenRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Infinity Cure (Japan)
static struct BurnRomInfo ngpc_infinityRomDesc[] = {
	{ "Infinity Cure (Japan)(2000)(KID).ngp", 0x100000, 0x32dc2aa2, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_infinity, ngpc_infinity, ngpc_ngp)
STD_ROM_FN(ngpc_infinity)

struct BurnDriver BurnDrvngpc_infinity = {
	"ngp_infinity", NULL, "ngp_ngp", NULL, "2000",
	"Infinity Cure (Japan)\0", NULL, "KID", "NeoGeo Pocket Color",
	L"Infinity Cure (Japan)\0\u30a4\u30f3\u30d5\u30a3\u30cb\u30c6\u30a3\u30fb\u30ad\u30e5\u30a2\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_ADV, 0,
	NgpGetZipName, ngpc_infinityRomInfo, ngpc_infinityRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Kikou Seiki Unitron: Sono Tsuide. Hikari Umareru Chi Yori. (Japan)
static struct BurnRomInfo ngpc_kikouseiRomDesc[] = {
	{ "Kikou Seiki Unitron - Sono Tsuide. Hikari Umareru Chi Yori. (Japan)(2000)(SNK - Yumekobo).ngp", 0x200000, 0x84580d66, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_kikousei, ngpc_kikousei, ngpc_ngp)
STD_ROM_FN(ngpc_kikousei)

struct BurnDriver BurnDrvngpc_kikousei = {
	"ngp_kikousei", NULL, "ngp_ngp", NULL, "2000",
	"Kikou Seiki Unitron: Sono Tsuide. Hikari Umareru Chi Yori. (Japan)\0", NULL, "SNK - Yumekobo", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_kikouseiRomInfo, ngpc_kikouseiRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// King of Fighters R-1 & Melon-chan no Seichou Nikki (Japan, Prototype)
static struct BurnRomInfo ngp_kof_mlonRomDesc[] = {
	{ "King of Fighters R-1 & Melon-chan no Seichou Nikki (Japan, Proto)(1998)(SNK).ngp", 0x200000, 0x2660bfc4, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngp_kof_mlon, ngp_kof_mlon, ngpc_ngp)
STD_ROM_FN(ngp_kof_mlon)

struct BurnDriver BurnDrvngp_kof_mlon = {
	"ngp_kof_mlon", NULL, "ngp_ngp", NULL, "1998",
	"King of Fighters R-1 & Melon-chan no Seichou Nikki (Japan, Prototype)\0", NULL, "SNK", "NeoGeo Pocket",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_SNK_NGP, GBF_ADV | GBF_VSFIGHT, 0,
	NgpGetZipName, ngp_kof_mlonRomInfo, ngp_kof_mlonRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x8,
	160, 152, 4, 3
};

// King of Fighters, The: Battle de Paradise (Japan)
static struct BurnRomInfo ngpc_kofparaRomDesc[] = {
	{ "King of Fighters, The - Battle de Paradise (Japan)(2000)(SNK).ngp", 0x200000, 0x77e37bac, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_kofpara, ngpc_kofpara, ngpc_ngp)
STD_ROM_FN(ngpc_kofpara)

struct BurnDriver BurnDrvngpc_kofpara = {
	"ngp_kofpara", NULL, "ngp_ngp", NULL, "2000",
	"King of Fighters, The: Battle de Paradise (Japan)\0", "Force B&W mode to unlock the hidden game 'Yosaku'", "SNK", "NeoGeo Pocket Color",
	L"King of Fighters, The: Battle de Paradise (Japan)\0\u30d0\u30c8\u30eb DE \u30d1\u30e9\u30c0\u30a4\u30b9\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_BOARD, 0,
	NgpGetZipName, ngpc_kofparaRomInfo, ngpc_kofparaRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpcDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// King of Fighters R-1 - Pocket Fighting Series (Euro, Japan)
static struct BurnRomInfo ngp_kofr1RomDesc[] = {
	{ "King of Fighters R-1 - Pocket Fighting Series (Euro, Japan)(1998)(SNK).ngp", 0x200000, 0xdceb7e11, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngp_kofr1, ngp_kofr1, ngpc_ngp)
STD_ROM_FN(ngp_kofr1)

struct BurnDriver BurnDrvngp_kofr1 = {
	"ngp_kofr1", NULL, "ngp_ngp", NULL, "1998",
	"King of Fighters R-1 - Pocket Fighting Series (Euro, Japan)\0", NULL, "SNK", "NeoGeo Pocket",
	L"King of Fighters R-1 - Pocket Fighting Series (Euro, Japan)\0\u30ad\u30f3\u30b0\u30fb\u30aa\u30d6\u30fb\u30d5\u30a1\u30a4\u30bf\u30fc\u30ba R-1\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGP, GBF_VSFIGHT, 0,
	NgpGetZipName, ngp_kofr1RomInfo, ngp_kofr1RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x8,
	160, 152, 4, 3
};

// King of Fighters R-2 - Pocket Fighting Series (World)
static struct BurnRomInfo ngpc_kofr2RomDesc[] = {
	{ "King of Fighters R-2 - Pocket Fighting Series (World)(1999)(SNK).ngp", 0x200000, 0x47e490a2, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_kofr2, ngpc_kofr2, ngpc_ngp)
STD_ROM_FN(ngpc_kofr2)

struct BurnDriver BurnDrvngpc_kofr2 = {
	"ngp_kofr2", NULL, "ngp_ngp", NULL, "1999",
	"King of Fighters R-2 - Pocket Fighting Series (World)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_kofr2RomInfo, ngpc_kofr2RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// King of Fighters R-2 - Pocket Fighting Series (World, Demo)
static struct BurnRomInfo ngpc_kofr2dRomDesc[] = {
	{ "King of Fighters R-2 - Pocket Fighting Series (World, Demo)(1999)(SNK).npg", 0x200000, 0xe3ae79c0, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_kofr2d, ngpc_kofr2d, ngpc_ngp)
STD_ROM_FN(ngpc_kofr2d)

struct BurnDriver BurnDrvngpc_kofr2d = {
	"ngp_kofr2d", "ngp_kofr2", "ngp_ngp", NULL, "1999",
	"King of Fighters R-2 - Pocket Fighting Series (World, Demo)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_kofr2dRomInfo, ngpc_kofr2dRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// King of Fighters R-2 - Pocket Fighting Series (World, Demo v2)
static struct BurnRomInfo ngpc_kofr2d2RomDesc[] = {
	{ "King of Fighters R-2 - Pocket Fighting Series (World, Demo v2)(1999)(SNK).ngp", 0x200000, 0x76544c97, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_kofr2d2, ngpc_kofr2d2, ngpc_ngp)
STD_ROM_FN(ngpc_kofr2d2)

struct BurnDriver BurnDrvngpc_kofr2d2 = {
	"ngp_kofr2d2", "ngp_kofr2", "ngp_ngp", NULL, "1999",
	"King of Fighters R-2 - Pocket Fighting Series (World, Demo v2)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_kofr2d2RomInfo, ngpc_kofr2d2RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Koi Koi Mahjong (Japan)
static struct BurnRomInfo ngpc_koikoiRomDesc[] = {
	{ "Koi Koi Mahjong (Japan)(2000)(Visco Corp.).ngp", 0x80000, 0xb51c3eba, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_koikoi, ngpc_koikoi, ngpc_ngp)
STD_ROM_FN(ngpc_koikoi)

struct BurnDriver BurnDrvngpc_koikoi = {
	"ngp_koikoi", NULL, "ngp_ngp", NULL, "2000",
	"Koi Koi Mahjong (Japan)\0", NULL, "Visco Corp.", "NeoGeo Pocket Color",
	L"Koi Koi Mahjong (Japan)\0\u30b3\u30a4\u30b3\u30a4 \u9ebb\u96c0\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_MAHJONG, 0,
	NgpGetZipName, ngpc_koikoiRomInfo, ngpc_koikoiRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Last Blade, The: Beyond the Destiny (Euro)
static struct BurnRomInfo ngpc_lastbladRomDesc[] = {
	{ "Last Blade, The - Beyond the Destiny (Euro)(2000)(SNK).ngp", 0x200000, 0x94fcfd1e, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_lastblad, ngpc_lastblad, ngpc_ngp)
STD_ROM_FN(ngpc_lastblad)

struct BurnDriver BurnDrvngpc_lastblad = {
	"ngp_lastblad", NULL, "ngp_ngp", NULL, "2000",
	"Last Blade, The: Beyond the Destiny (Euro)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_lastbladRomInfo, ngpc_lastbladRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Magical Drop Pocket (Euro, USA)
static struct BurnRomInfo ngpc_magdropRomDesc[] = {
	{ "Magical Drop Pocket (Euro, USA)(1999)(Data East).ngp", 0x100000, 0x7712f82d, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_magdrop, ngpc_magdrop, ngpc_ngp)
STD_ROM_FN(ngpc_magdrop)

struct BurnDriver BurnDrvngpc_magdrop = {
	"ngp_magdrop", NULL, "ngp_ngp", NULL, "1999",
	"Magical Drop Pocket (Euro, USA)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_magdropRomInfo, ngpc_magdropRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Magical Drop Pocket (Japan)
static struct BurnRomInfo ngpc_magdropjRomDesc[] = {
	{ "Magical Drop Pocket (Japan)(1999)(Data East).ngp", 0x100000, 0x694ce2dc, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_magdropj, ngpc_magdropj, ngpc_ngp)
STD_ROM_FN(ngpc_magdropj)

struct BurnDriver BurnDrvngpc_magdropj = {
	"ngp_magdropj", "ngp_magdrop", "ngp_ngp", NULL, "1999",
	"Magical Drop Pocket (Japan)\0", NULL, "Data East", "NeoGeo Pocket Color",
	L"Magical Drop Pocket (Japan)\0\u30de\u30b8\u30ab\u30eb\u30c9\u30ed\u30c3\u30d7 \u30dd\u30b1\u30c3\u30c8\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_magdropjRomInfo, ngpc_magdropjRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Magical Drop Pocket (Japan, Demo)
static struct BurnRomInfo ngpc_magdropjdRomDesc[] = {
	{ "Magical Drop Pocket (Japan, Demo)(1999)(Data East).ngp", 0x100000, 0xd76f1a47, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_magdropjd, ngpc_magdropjd, ngpc_ngp)
STD_ROM_FN(ngpc_magdropjd)

struct BurnDriver BurnDrvngpc_magdropjd = {
	"ngp_magdropjd", "ngp_magdrop", "ngp_ngp", NULL, "1999",
	"Magical Drop Pocket (Japan, Demo)\0", NULL, "Data East", "NeoGeo Pocket Color",
	L"Magical Drop Pocket (Japan, Demo)\0\u30de\u30b8\u30ab\u30eb\u30c9\u30ed\u30c3\u30d7 \u30dd\u30b1\u30c3\u30c8\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_magdropjdRomInfo, ngpc_magdropjdRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Melon-chan no Seichou Nikki (Japan)
static struct BurnRomInfo ngp_melonchnRomDesc[] = {
	{ "Melon-chan no Seichou Nikki (Japan)(1998)(SNK - ADK).ngp", 0x100000, 0xcb42cfa4, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngp_melonchn, ngp_melonchn, ngpc_ngp)
STD_ROM_FN(ngp_melonchn)

struct BurnDriver BurnDrvngp_melonchn = {
	"ngp_melonchn", NULL, "ngp_ngp", NULL, "1998",
	"Melon-chan no Seichou Nikki (Japan)\0", NULL, "SNK - ADK Corp.", "NeoGeo Pocket",
	L"Melon-chan no Seichou Nikki (Japan)\0\u3081\u308d\u3093\u3061\u3083\u3093\u306e\u6210\u9577\u65e5\u8a18\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGP, GBF_ADV, 0,
	NgpGetZipName, ngp_melonchnRomInfo, ngp_melonchnRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x8,
	160, 152, 4, 3
};

// Memories Off: Pure (Japan)
static struct BurnRomInfo ngpc_memoriesRomDesc[] = {
	{ "Memories Off - Pure (Japan)(2000)(KID).ngp", 0x100000, 0xa7926e90, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_memories, ngpc_memories, ngpc_ngp)
STD_ROM_FN(ngpc_memories)

struct BurnDriver BurnDrvngpc_memories = {
	"ngp_memories", NULL, "ngp_ngp", NULL, "2000",
	"Memories Off: Pure (Japan)\0", NULL, "KID", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_memoriesRomInfo, ngpc_memoriesRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Mezase! Kanji Ou (Japan)
static struct BurnRomInfo ngpc_mezaseRomDesc[] = {
	{ "Mezase! Kanji Ou (Japan)(2000)(SNK).ngp", 0x200000, 0xa52e1c82, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_mezase, ngpc_mezase, ngpc_ngp)
STD_ROM_FN(ngpc_mezase)

struct BurnDriver BurnDrvngpc_mezase = {
	"ngp_mezase", NULL, "ngp_ngp", NULL, "2000",
	"Mezase! Kanji Ou (Japan)\0", NULL, "SNK", "NeoGeo Pocket Color",
	L"Mezase! Kanji Ou (Japan)\0\u3081\u3056\u305b! \u6f22\u5b57\u738b\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_QUIZ, 0,
	NgpGetZipName, ngpc_mezaseRomInfo, ngpc_mezaseRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Mizuki Shigeru no Youkai Shashinkan (Japan)
static struct BurnRomInfo ngpc_mizukiRomDesc[] = {
	{ "Mizuki Shigeru no Youkai Shashinkan (Japan)(1999)(SNK).ngp", 0x200000, 0xdde08335, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_mizuki, ngpc_mizuki, ngpc_ngp)
STD_ROM_FN(ngpc_mizuki)

struct BurnDriver BurnDrvngpc_mizuki = {
	"ngp_mizuki", NULL, "ngp_ngp", NULL, "1999",
	"Mizuki Shigeru no Youkai Shashinkan (Japan)\0", NULL, "SNK", "NeoGeo Pocket Color",
	L"Mizuki Shigeru no Youkai Shashinkan (Japan)\0\u6c34\u6728\u3057\u3052\u308b\u306e \u5996\u602a\u5199\u771f\u9928\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_mizukiRomInfo, ngpc_mizukiRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Metal Slug: 1st Mission (World)
static struct BurnRomInfo ngpc_mslug1stRomDesc[] = {
	{ "Metal Slug - 1st Mission (World)(1999)(SNK).ngp", 0x200000, 0x4ff91807, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_mslug1st, ngpc_mslug1st, ngpc_ngp)
STD_ROM_FN(ngpc_mslug1st)

struct BurnDriver BurnDrvngpc_mslug1st = {
	"ngp_mslug1st", NULL, "ngp_ngp", NULL, "1999",
	"Metal Slug: 1st Mission (World)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_PLATFORM | GBF_RUNGUN, 0,
	NgpGetZipName, ngpc_mslug1stRomInfo, ngpc_mslug1stRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Metal Slug: 2nd Mission (World)
static struct BurnRomInfo ngpc_mslug2ndRomDesc[] = {
	{ "Metal Slug - 2nd Mission (World)(2000)(SNK).ngp", 0x400000, 0xac549144, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_mslug2nd, ngpc_mslug2nd, ngpc_ngp)
STD_ROM_FN(ngpc_mslug2nd)

struct BurnDriver BurnDrvngpc_mslug2nd = {
	"ngp_mslug2nd", NULL, "ngp_ngp", NULL, "2000",
	"Metal Slug: 2nd Mission (World)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_PLATFORM | GBF_RUNGUN, 0,
	NgpGetZipName, ngpc_mslug2ndRomInfo, ngpc_mslug2ndRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Metal Slug: 2nd Mission (World, Demo)
static struct BurnRomInfo ngpc_mslug2nddRomDesc[] = {
	{ "Metal Slug - 2nd Mission (World, Demo)(2000)(SNK).ngp", 0x200000, 0x8b1647d4, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_mslug2ndd, ngpc_mslug2ndd, ngpc_ngp)
STD_ROM_FN(ngpc_mslug2ndd)

struct BurnDriver BurnDrvngpc_mslug2ndd = {
	"ngp_mslug2ndd", "ngp_mslug2nd", "ngp_ngp", NULL, "2000",
	"Metal Slug: 2nd Mission (World, Demo)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_PLATFORM | GBF_RUNGUN, 0,
	NgpGetZipName, ngpc_mslug2nddRomInfo, ngpc_mslug2nddRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Neo 21 - Real Casino Series (World)
static struct BurnRomInfo ngpc_neo21RomDesc[] = {
	{ "Neo 21 - Real Casino Series (World)(2000)(Dyna Corp.).ngp", 0x100000, 0x0a2a2f28, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_neo21, ngpc_neo21, ngpc_ngp)
STD_ROM_FN(ngpc_neo21)

struct BurnDriver BurnDrvngpc_neo21 = {
	"ngp_neo21", NULL, "ngp_ngp", NULL, "2000",
	"Neo 21 - Real Casino Series (World)\0", NULL, "Dyna Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CARD | GBF_CASINO, 0,
	NgpGetZipName, ngpc_neo21RomInfo, ngpc_neo21RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Neo Baccarat - Real Casino Series (World)
static struct BurnRomInfo ngpc_neobaccaRomDesc[] = {
	{ "Neo Baccarat - Real Casino Series (World)(2000)(Dyna Corp.).ngp", 0x100000, 0x22aab454, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_neobacca, ngpc_neobacca, ngpc_ngp)
STD_ROM_FN(ngpc_neobacca)

struct BurnDriver BurnDrvngpc_neobacca = {
	"ngp_neobacca", NULL, "ngp_ngp", NULL, "2000",
	"Neo Baccarat - Real Casino Series (World)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CARD | GBF_CASINO, 0,
	NgpGetZipName, ngpc_neobaccaRomInfo, ngpc_neobaccaRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Neo Baccarat - Real Casino Series (World, Prototype)
static struct BurnRomInfo ngpc_neobaccapRomDesc[] = {
	{ "Neo Baccarat - Real Casino Series (World, Proto)(2000)(Dyna Corp.).ngp", 0x100000, 0x80880498, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_neobaccap, ngpc_neobaccap, ngpc_ngp)
STD_ROM_FN(ngpc_neobaccap)

struct BurnDriver BurnDrvngpc_neobaccap = {
	"ngp_neobaccap", "ngp_neobacca", "ngp_ngp", NULL, "2000",
	"Neo Baccarat - Real Casino Series (World, Prototype)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_SNK_NGPC, GBF_CARD | GBF_CASINO, 0,
	NgpGetZipName, ngpc_neobaccapRomInfo, ngpc_neobaccapRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Neo Cherry Master - Real Casino Series (Euro, Japan)
static struct BurnRomInfo ngp_neocherRomDesc[] = {
	{ "Neo Cherry Master - Real Casino Series (Euro, Japan)(1998)(Dyna Corp.).ngp", 0x100000, 0x5c067579, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngp_neocher, ngp_neocher, ngpc_ngp)
STD_ROM_FN(ngp_neocher)

struct BurnDriver BurnDrvngp_neocher = {
	"ngp_neocher", NULL, "ngp_ngp", NULL, "1998",
	"Neo Cherry Master - Real Casino Series (Euro, Japan)\0", NULL, "Dyna Corp.", "NeoGeo Pocket",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGP, GBF_CASINO, 0,
	NgpGetZipName, ngp_neocherRomInfo, ngp_neocherRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x8,
	160, 152, 4, 3
};

// Neo Cherry Master Color - Real Casino Series (World)
static struct BurnRomInfo ngpc_neochercRomDesc[] = {
	{ "Neo Cherry Master Color - Real Casino Series (World)(1999)(Dyna Corp.).ngp", 0x100000, 0xa6dca584, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_neocherc, ngpc_neocherc, ngpc_ngp)
STD_ROM_FN(ngpc_neocherc)

struct BurnDriver BurnDrvngpc_neocherc = {
	"ngp_neocherc", NULL, "ngp_ngp", NULL, "1999",
	"Neo Cherry Master Color - Real Casino Series (World)\0", NULL, "Dyna Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_neochercRomInfo, ngpc_neochercRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Neo Cherry Master Color - Real Casino Series (World, Prototype)
static struct BurnRomInfo ngpc_neochercpRomDesc[] = {
	{ "Neo Cherry Master Color - Real Casino Series (World, Proto)(1999)(Dyna Corp.).ngp", 0x100000, 0x939a9912, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_neochercp, ngpc_neochercp, ngpc_ngp)
STD_ROM_FN(ngpc_neochercp)

struct BurnDriver BurnDrvngpc_neochercp = {
	"ngp_neochercp", "ngp_neocherc", "ngp_ngp", NULL, "1999",
	"Neo Cherry Master Color - Real Casino Series (World, Prototype)\0", NULL, "Dyna Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_neochercpRomInfo, ngpc_neochercpRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Neo Geo Cup '98 - Pocket Sports Series (Euro, Japan)
static struct BurnRomInfo ngp_neocup98RomDesc[] = {
	{ "Neo Geo Cup '98 - Pocket Sports Series (Euro, Japan)(1998)(SNK).ngp", 0x100000, 0x33add5bd, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngp_neocup98, ngp_neocup98, ngpc_ngp)
STD_ROM_FN(ngp_neocup98)

struct BurnDriver BurnDrvngp_neocup98 = {
	"ngp_neocup98", NULL, "ngp_ngp", NULL, "1998",
	"Neo Geo Cup '98 - Pocket Sports Series (Euro, Japan)\0", NULL, "SNK", "NeoGeo Pocket",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGP, GBF_SPORTSFOOTBALL, 0,
	NgpGetZipName, ngp_neocup98RomInfo, ngp_neocup98RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x8,
	160, 152, 4, 3
};

// Neo Geo Cup '98 Plus - Pocket Sports Series (World)
static struct BurnRomInfo ngpc_neocupplRomDesc[] = {
	{ "Neo Geo Cup '98 Plus - Pocket Sports Series (World)(1999)(SNK).ngp", 0x100000, 0xc0da4fe9, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_neocuppl, ngpc_neocuppl, ngpc_ngp)
STD_ROM_FN(ngpc_neocuppl)

struct BurnDriver BurnDrvngpc_neocuppl = {
	"ngp_neocuppl", NULL, "ngp_ngp", NULL, "1999",
	"Neo Geo Cup '98 Plus - Pocket Sports Series (World)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_SPORTSFOOTBALL, 0,
	NgpGetZipName, ngpc_neocupplRomInfo, ngpc_neocupplRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Neo Derby Champ Daiyosou (Japan)
static struct BurnRomInfo ngpc_neoderbyRomDesc[] = {
	{ "Neo Derby Champ Daiyosou (Japan)(1999)(Dyna Corp.).ngp", 0x200000, 0x5a053559, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_neoderby, ngpc_neoderby, ngpc_ngp)
STD_ROM_FN(ngpc_neoderby)

struct BurnDriver BurnDrvngpc_neoderby = {
	"ngp_neoderby", NULL, "ngp_ngp", NULL, "1999",
	"Neo Derby Champ Daiyosou (Japan)\0", NULL, "Dyna Corp.", "NeoGeo Pocket Color",
	L"Neo Derby Champ Daiyosou (Japan)\0Neo Derby Champ \u5927\u4e88\u60f3\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_SPORTSMISC, 0,
	NgpGetZipName, ngpc_neoderbyRomInfo, ngpc_neoderbyRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Neo Dragon's Wild - Real Casino Series (World, v1.13)
static struct BurnRomInfo ngpc_neodragRomDesc[] = {
	{ "Neo Dragon's Wild - Real Casino Series v1.13 (World)(1999)(Dyna Corp.).ngp", 0x100000, 0x5d028c99, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_neodrag, ngpc_neodrag, ngpc_ngp)
STD_ROM_FN(ngpc_neodrag)

struct BurnDriver BurnDrvngpc_neodrag = {
	"ngp_neodrag", NULL, "ngp_ngp", NULL, "1999",
	"Neo Dragon's Wild - Real Casino Series (World, v1.13)\0", NULL, "Dyna Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CARD | GBF_CASINO, 0,
	NgpGetZipName, ngpc_neodragRomInfo, ngpc_neodragRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Neo Dragon's Wild - Real Casino Series (World, v1.11)
static struct BurnRomInfo ngpc_neodragaRomDesc[] = {
	{ "Neo Dragon's Wild - Real Casino Series v1.11 (World)(1999)(Dyna Corp.).ngp", 0x100000, 0x225fd7f9, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_neodraga, ngpc_neodraga, ngpc_ngp)
STD_ROM_FN(ngpc_neodraga)

struct BurnDriver BurnDrvngpc_neodraga = {
	"ngp_neodraga", "ngp_neodrag", "ngp_ngp", NULL, "1999",
	"Neo Dragon's Wild - Real Casino Series (World, v1.11)\0", NULL, "Dyna Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_CARD | GBF_CASINO, 0,
	NgpGetZipName, ngpc_neodragaRomInfo, ngpc_neodragaRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Neo Mystery Bonus - Real Casino Series (World)
static struct BurnRomInfo ngpc_neomystrRomDesc[] = {
	{ "Neo Mystery Bonus - Real Casino Series (World)(1999)(Dyna Corp.).ngp", 0x100000, 0xe79dc5b3, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_neomystr, ngpc_neomystr, ngpc_ngp)
STD_ROM_FN(ngpc_neomystr)

struct BurnDriver BurnDrvngpc_neomystr = {
	"ngp_neomystr", NULL, "ngp_ngp", NULL, "1999",
	"Neo Mystery Bonus - Real Casino Series (World)\0", NULL, "Dyna Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_neomystrRomInfo, ngpc_neomystrRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Neo Poke Pro Yakyuu - Pocket Sport Series (Japan)
static struct BurnRomInfo ngpc_neoproykRomDesc[] = {
	{ "Neo Poke Pro Yakyuu - Pocket Sport Series (Japan)(1999)(SNK - ADK).ngp", 0x100000, 0xf4d78f12, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_neoproyk, ngpc_neoproyk, ngpc_ngp)
STD_ROM_FN(ngpc_neoproyk)

struct BurnDriver BurnDrvngpc_neoproyk = {
	"ngp_neoproyk", NULL, "ngp_ngp", NULL, "1999",
	"Neo Poke Pro Yakyuu - Pocket Sport Series (Japan)\0", NULL, "SNK - ADK Corp.", "NeoGeo Pocket Color",
	L"Neo Poke Pro Yakyuu - Pocket Sport Series (Japan)\0\u30cd\u30aa\u30dd\u30b1 \u30d7\u30ed\u91ce\u7403\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_SPORTSMISC, 0,
	NgpGetZipName, ngpc_neoproykRomInfo, ngpc_neoproykRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Neo Turf Masters - Pocket Sport Series (World)
static struct BurnRomInfo ngpc_neoturfmRomDesc[] = {
	{ "Neo Turf Masters - Pocket Sport Series (World)(1999)(SNK).ngp", 0x200000, 0x317a66d2, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_neoturfm, ngpc_neoturfm, ngpc_ngp)
STD_ROM_FN(ngpc_neoturfm)

struct BurnDriver BurnDrvngpc_neoturfm = {
	"ngp_neoturfm", NULL, "ngp_ngp", NULL, "1999",
	"Neo Turf Masters - Pocket Sport Series (World)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_SPORTSMISC, 0,
	NgpGetZipName, ngpc_neoturfmRomInfo, ngpc_neoturfmRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Nige-ron-pa (Japan)
static struct BurnRomInfo ngpc_nigeronpRomDesc[] = {
	{ "Nige-ron-pa (Japan)(2000)(Dennoi Eizo).ngp", 0x100000, 0x3cc3e269, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_nigeronp, ngpc_nigeronp, ngpc_ngp)
STD_ROM_FN(ngpc_nigeronp)

struct BurnDriver BurnDrvngpc_nigeronp = {
	"ngp_nigeronp", NULL, "ngp_ngp", NULL, "2000",
	"Nige-ron-pa (Japan)\0", NULL, "Dennou Eizo", "NeoGeo Pocket Color",
	L"Nige-ron-pa (Japan)\0\u306b\u3052\u30ed\u30f3\u30d1\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_nigeronpRomInfo, ngpc_nigeronpRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Nige-ron-pa (Hack, English v0.9)
// https://www.romhacking.net/translations/3804/
static struct BurnRomInfo ngpc_nigeronpeRomDesc[] = {
	{ "Nige-ron-pa T-Eng v0.9 (2018)(Loic).ngp", 0x200000, 0x6672d6e6, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_nigeronpe, ngpc_nigeronpe, ngpc_ngp)
STD_ROM_FN(ngpc_nigeronpe)

struct BurnDriver BurnDrvngpc_nigeronpe = {
	"ngp_nigeronpe", "ngp_nigeronp", "ngp_ngp", NULL, "2018",
	"Nige-ron-pa (Hack, English v0.9)\0", NULL, "Loic", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_SNK_NGPC, GBF_RPG, 0,
	NgpGetZipName, ngpc_nigeronpeRomInfo, ngpc_nigeronpeRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Oekaki Puzzle (Japan)
static struct BurnRomInfo ngpc_oekakipRomDesc[] = {
	{ "Oekaki Puzzle (Japan)(2000)(Success).ngp", 0x80000, 0x59ab1e0f, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_oekakip, ngpc_oekakip, ngpc_ngp)
STD_ROM_FN(ngpc_oekakip)

struct BurnDriver BurnDrvngpc_oekakip = {
	"ngp_oekakip", "ngp_picturep", "ngp_ngp", NULL, "2000",
	"Oekaki Puzzle (Japan)\0", NULL, "Success", "NeoGeo Pocket Color",
	L"Oekaki Puzzle (Japan)\0\u304a\u3048\u304b\u304d \u30d1\u30ba\u30eb\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_oekakipRomInfo, ngpc_oekakipRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Ogre Battle Gaiden: Prince of Zenobia (Japan)
static struct BurnRomInfo ngpc_ogrebatlRomDesc[] = {
	{ "Ogre Battle Gaiden - Prince of Zenobia (Japan)(2000)(SNK).ngp", 0x200000, 0xeeeefd6a, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_ogrebatl, ngpc_ogrebatl, ngpc_ngp)
STD_ROM_FN(ngpc_ogrebatl)

struct BurnDriver BurnDrvngpc_ogrebatl = {
	"ngp_ogrebatl", NULL, "ngp_ngp", NULL, "2000",
	"Ogre Battle Gaiden: Prince of Zenobia (Japan)\0", NULL, "SNK", "NeoGeo Pocket Color",
	L"Ogre Battle Gaiden: Prince of Zenobia (Japan)\0\u4f1d\u8aac\u306e\u30aa\u30a6\u30ac\u30d0\u30c8\u30eb\u5916\u4f1d \u30bc\u30ce\u30d3\u30a2\u306e\u7687\u5b50\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_RPG | GBF_STRATEGY, 0,
	NgpGetZipName, ngpc_ogrebatlRomInfo, ngpc_ogrebatlRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pachinko Hisshou Guide: Pocket Parlor (Japan)
static struct BurnRomInfo ngpc_pachinkoRomDesc[] = {
	{ "Pachinko Hisshou Guide - Pocket Parlor (Japan)(1999)(Japan Vistec).ngp", 0x100000, 0xc3d6b28b, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pachinko, ngpc_pachinko, ngpc_ngp)
STD_ROM_FN(ngpc_pachinko)

struct BurnDriver BurnDrvngpc_pachinko = {
	"ngp_pachinko", NULL, "ngp_ngp", NULL, "1999",
	"Pachinko Hisshou Guide: Pocket Parlor (Japan)\0", NULL, "Japan Vistec", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_pachinkoRomInfo, ngpc_pachinkoRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pac-Man (World)
static struct BurnRomInfo ngpc_pacmanRomDesc[] = {
	{ "Pac-Man (World)(1999)(SNK - Namco).ngp", 0x80000, 0x21e8cc15, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pacman, ngpc_pacman, ngpc_ngp)
STD_ROM_FN(ngpc_pacman)

struct BurnDriver BurnDrvngpc_pacman = {
	"ngp_pacman", NULL, "ngp_ngp", NULL, "1999",
	"Pac-Man (World)\0", NULL, "SNK - Namco", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_ACTION | GBF_MAZE, 0,
	NgpGetZipName, ngpc_pacmanRomInfo, ngpc_pacmanRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Puzzle Bobble Mini (Euro, Japan, v1.10)
static struct BurnRomInfo ngpc_pbobbleRomDesc[] = {
	{ "Puzzle Bobble Mini v1.10 (Euro, Japan)(1999)(SNK).ngp", 0x100000, 0xe69eae3e, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pbobble, ngpc_pbobble, ngpc_ngp)
STD_ROM_FN(ngpc_pbobble)

struct BurnDriver BurnDrvngpc_pbobble = {
	"ngp_pbobble", NULL, "ngp_ngp", NULL, "1999",
	"Puzzle Bobble Mini (Euro, Japan, v1.10)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_pbobbleRomInfo, ngpc_pbobbleRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Puzzle Bobble Mini (Euro, Japan, v1.09)
static struct BurnRomInfo ngpc_pbobbleaRomDesc[] = {
	{ "Puzzle Bobble Mini v1.09 (Euro, Japan)(1999)(SNK).ngp", 0x100000, 0xc94b173a, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pbobblea, ngpc_pbobblea, ngpc_ngp)
STD_ROM_FN(ngpc_pbobblea)

struct BurnDriver BurnDrvngpc_pbobblea = {
	"ngp_pbobblea", "ngp_pbobble", "ngp_ngp", NULL, "1999",
	"Puzzle Bobble Mini (Euro, Japan, v1.09)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_pbobbleaRomInfo, ngpc_pbobbleaRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Puzzle Bobble Mini (Euro, Japan, Prototype)
static struct BurnRomInfo ngpc_pbobblebRomDesc[] = {
	{ "Puzzle Bobble Mini (Euro, Japan, Proto)(1999)(SNK).ngp", 0x100000, 0xe3d9f38e, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pbobbleb, ngpc_pbobbleb, ngpc_ngp)
STD_ROM_FN(ngpc_pbobbleb)

struct BurnDriver BurnDrvngpc_pbobbleb = {
	"ngp_pbobbleb", "ngp_pbobble", "ngp_ngp", NULL, "1999",
	"Puzzle Bobble Mini (Euro, Japan, Prototype)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_pbobblebRomInfo, ngpc_pbobblebRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Picture Puzzle (Euro, USA)
static struct BurnRomInfo ngpc_picturepRomDesc[] = {
	{ "Picture Puzzle (Euro, USA)(2000)(Success).ngp", 0x100000, 0x67e6dc56, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_picturep, ngpc_picturep, ngpc_ngp)
STD_ROM_FN(ngpc_picturep)

struct BurnDriver BurnDrvngpc_picturep = {
	"ngp_picturep", NULL, "ngp_ngp", NULL, "2000",
	"Picture Puzzle (Euro, USA)\0", NULL, "Success", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_picturepRomInfo, ngpc_picturepRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pocket Love if (Japan)
static struct BurnRomInfo ngpc_poktloveRomDesc[] = {
	{ "Pocket Love if (Japan)(1999)(KID).ngp", 0x200000, 0xc2ee1ee5, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_poktlove, ngpc_poktlove, ngpc_ngp)
STD_ROM_FN(ngpc_poktlove)

struct BurnDriver BurnDrvngpc_poktlove = {
	"ngp_poktlove", NULL, "ngp_ngp", NULL, "1999",
	"Pocket Love if (Japan)\0", NULL, "KID", "NeoGeo Pocket Color",
	L"Pocket Love if (Japan)\0\u30dd\u30b1\u30c3\u30c8\u30e9\u30d6if\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_ADV, 0,
	NgpGetZipName, ngpc_poktloveRomInfo, ngpc_poktloveRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pocket Reversi (Euro)
static struct BurnRomInfo ngpc_pockrevRomDesc[] = {
	{ "Pocket Reversi (Euro)(2000)(Success).ngp", 0x100000, 0x20f09880, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pockrev, ngpc_pockrev, ngpc_ngp)
STD_ROM_FN(ngpc_pockrev)

struct BurnDriver BurnDrvngpc_pockrev = {
	"ngp_pockrev", NULL, "ngp_ngp", NULL, "2000",
	"Pocket Reversi (Euro)\0", NULL, "Success", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_BOARD, 0,
	NgpGetZipName, ngpc_pockrevRomInfo, ngpc_pockrevRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pocket Reversi (Japan)
static struct BurnRomInfo ngpc_pockrevjRomDesc[] = {
	{ "Pocket Reversi (Japan)(2000)(Success).ngp", 0x80000, 0xfb0b55b8, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pockrevj, ngpc_pockrevj, ngpc_ngp)
STD_ROM_FN(ngpc_pockrevj)

struct BurnDriver BurnDrvngpc_pockrevj = {
	"ngp_pockrevj", "ngp_pockrev", "ngp_ngp", NULL, "2000",
	"Pocket Reversi (Japan)\0", NULL, "Success", "NeoGeo Pocket Color",
	L"Pocket Reversi (Japan)\0Pocket \u30ea\u30d0\u30fc\u30b7\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_BOARD, 0,
	NgpGetZipName, ngpc_pockrevjRomInfo, ngpc_pockrevjRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Party Mail (Japan)
static struct BurnRomInfo ngpc_prtymailRomDesc[] = {
	{ "Party Mail (Japan)(1999)(SNK - ADK).ngp", 0x100000, 0x4da8a1c0, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_prtymail, ngpc_prtymail, ngpc_ngp)
STD_ROM_FN(ngpc_prtymail)

struct BurnDriver BurnDrvngpc_prtymail = {
	"ngp_prtymail", NULL, "ngp_ngp", NULL, "1999",
	"Party Mail (Japan)\0", NULL, "SNK - ADK Corp.", "NeoGeo Pocket Color",
	L"Party Mail (Japan)\0\u30d1\u30fc\u30c6\u30a3\u30fc\u30e1\u30fc\u30eb\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_MISC, 0,
	NgpGetZipName, ngpc_prtymailRomInfo, ngpc_prtymailRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Party Mail (Japan, Prototype)
static struct BurnRomInfo ngpc_prtymailpRomDesc[] = {
	{ "Party Mail (Japan, Proto)(1999)(SNK - ADK).ngp", 0x200000, 0xf2284a95, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_prtymailp, ngpc_prtymailp, ngpc_ngp)
STD_ROM_FN(ngpc_prtymailp)

struct BurnDriver BurnDrvngpc_prtymailp = {
	"ngp_prtymailp", "ngp_prtymail", "ngp_ngp", NULL, "1999",
	"Party Mail (Japan, Proto)\0", NULL, "SNK - ADK Corp.", "NeoGeo Pocket Color",
	L"Party Mail (Japan, Proto)\0\u30d1\u30fc\u30c6\u30a3\u30fc\u30e1\u30fc\u30eb\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_SNK_NGPC, GBF_MISC, 0,
	NgpGetZipName, ngpc_prtymailpRomInfo, ngpc_prtymailpRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pachi-Slot Aruze Oukoku Pocket: Azteca (Japan)
static struct BurnRomInfo ngpc_pslotaztRomDesc[] = {
	{ "Pachi-Slot Aruze Oukoku Pocket - Azteca (Japan)(2000)(Aruze).ngp", 0x80000, 0xc56ef8c3, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pslotazt, ngpc_pslotazt, ngpc_ngp)
STD_ROM_FN(ngpc_pslotazt)

struct BurnDriver BurnDrvngpc_pslotazt = {
	"ngp_pslotazt", NULL, "ngp_ngp", NULL, "2000",
	"Pachi-Slot Aruze Oukoku Pocket: Azteca (Japan)\0", NULL, "Aruze Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_pslotaztRomInfo, ngpc_pslotaztRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pachi-Slot Aruze Oukoku Pocket: Dekahel 2 (Japan)
static struct BurnRomInfo ngpc_pslotdk2RomDesc[] = {
	{ "Pachi-Slot Aruze Oukoku Pocket - Dekahel 2 (Japan)(2001)(Aruze).ngp", 0x100000, 0x346f3c46, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pslotdk2, ngpc_pslotdk2, ngpc_ngp)
STD_ROM_FN(ngpc_pslotdk2)

struct BurnDriver BurnDrvngpc_pslotdk2 = {
	"ngp_pslotdk2", NULL, "ngp_ngp", NULL, "2001",
	"Pachi-Slot Aruze Oukoku Pocket: Dekahel 2 (Japan)\0", NULL, "Aruze Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_pslotdk2RomInfo, ngpc_pslotdk2RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pachi-Slot Aruze Oukoku Pocket: Delsol 2 (Japan)
static struct BurnRomInfo ngpc_pslotds2RomDesc[] = {
	{ "Pachi-Slot Aruze Oukoku Pocket - Delsol 2 (Japan)(2000)(Aruze).ngp", 0x100000, 0x1ea69db1, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pslotds2, ngpc_pslotds2, ngpc_ngp)
STD_ROM_FN(ngpc_pslotds2)

struct BurnDriver BurnDrvngpc_pslotds2 = {
	"ngp_pslotds2", NULL, "ngp_ngp", NULL, "2000",
	"Pachi-Slot Aruze Oukoku Pocket: Delsol 2 (Japan)\0", NULL, "Aruze Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_pslotds2RomInfo, ngpc_pslotds2RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pachi-Slot Aruze Oukoku Pocket: e-Cup (Japan)
static struct BurnRomInfo ngpc_pslotecpRomDesc[] = {
	{ "Pachi-Slot Aruze Oukoku Pocket - e-Cup (Japan)(2001)(Aruze).ngp", 0x100000, 0x0a7c32df, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pslotecp, ngpc_pslotecp, ngpc_ngp)
STD_ROM_FN(ngpc_pslotecp)

struct BurnDriver BurnDrvngpc_pslotecp = {
	"ngp_pslotecp", NULL, "ngp_ngp", NULL, "2001",
	"Pachi-Slot Aruze Oukoku Pocket: e-Cup (Japan)\0", NULL, "Aruze Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_pslotecpRomInfo, ngpc_pslotecpRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pachi-Slot Aruze Oukoku Pocket: Hanabi (Japan, v1.04)
static struct BurnRomInfo ngpc_pslothanRomDesc[] = {
	{ "Pachi-Slot Aruze Oukoku Pocket - Hanabi v1.04 (Japan)(1999)(Aruze).ngp", 0x80000, 0xa0c26f9b, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pslothan, ngpc_pslothan, ngpc_ngp)
STD_ROM_FN(ngpc_pslothan)

struct BurnDriver BurnDrvngpc_pslothan = {
	"ngp_pslothan", NULL, "ngp_ngp", NULL, "1999",
	"Pachi-Slot Aruze Oukoku Pocket: Hanabi (Japan, v1.04)\0", NULL, "Aruze Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_pslothanRomInfo, ngpc_pslothanRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pachi-Slot Aruze Oukoku Pocket: Hanabi (Japan, v1.02)
static struct BurnRomInfo ngpc_pslothanaRomDesc[] = {
	{ "Pachi-Slot Aruze Oukoku Pocket - Hanabi v1.02 (Japan)(1999)(Aruze).ngp", 0x80000, 0xcfd1c8f2, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pslothana, ngpc_pslothana, ngpc_ngp)
STD_ROM_FN(ngpc_pslothana)

struct BurnDriver BurnDrvngpc_pslothana = {
	"ngp_pslothana", "ngp_pslothan", "ngp_ngp", NULL, "1999",
	"Pachi-Slot Aruze Oukoku Pocket: Hanabi (Japan, v1.02)\0", NULL, "Aruze Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_pslothanaRomInfo, ngpc_pslothanaRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pachi-Slot Aruze Oukoku Pocket: Oohanabi (Japan)
static struct BurnRomInfo ngpc_pslotoohRomDesc[] = {
	{ "Pachi-Slot Aruze Oukoku Pocket - Oohanabi (Japan)(2000)(Aruze).ngp", 0x100000, 0x8a88c50f, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pslotooh, ngpc_pslotooh, ngpc_ngp)
STD_ROM_FN(ngpc_pslotooh)

struct BurnDriver BurnDrvngpc_pslotooh = {
	"ngp_pslotooh", NULL, "ngp_ngp", NULL, "2000",
	"Pachi-Slot Aruze Oukoku Pocket: Oohanabi (Japan)\0", NULL, "Aruze Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_pslotoohRomInfo, ngpc_pslotoohRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pachi-Slot Aruze Oukoku Pocket: Porcano 2 (Japan)
static struct BurnRomInfo ngpc_pslotpc2RomDesc[] = {
	{ "Pachi-Slot Aruze Oukoku Pocket - Porcano 2 (Japan)(2000)(Aruze).ngp", 0x100000, 0x96ddfbe3, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pslotpc2, ngpc_pslotpc2, ngpc_ngp)
STD_ROM_FN(ngpc_pslotpc2)

struct BurnDriver BurnDrvngpc_pslotpc2 = {
	"ngp_pslotpc2", NULL, "ngp_ngp", NULL, "2000",
	"Pachi-Slot Aruze Oukoku Pocket: Porcano 2 (Japan)\0", NULL, "Aruze Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_pslotpc2RomInfo, ngpc_pslotpc2RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pachi-Slot Aruze Oukoku Pocket: Ward of Lights (Japan)
static struct BurnRomInfo ngpc_pslotwrdRomDesc[] = {
	{ "Pachi-Slot Aruze Oukoku Pocket - Ward of Lights (Japan)(2000)(Aruze).ngp", 0x80000, 0xd46cbc54, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_pslotwrd, ngpc_pslotwrd, ngpc_ngp)
STD_ROM_FN(ngpc_pslotwrd)

struct BurnDriver BurnDrvngpc_pslotwrd = {
	"ngp_pslotwrd", NULL, "ngp_ngp", NULL, "2000",
	"Pachi-Slot Aruze Oukoku Pocket: Ward of Lights (Japan)\0", NULL, "Aruze Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_CASINO, 0,
	NgpGetZipName, ngpc_pslotwrdRomInfo, ngpc_pslotwrdRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Pocket Tennis - Pocket Sports Series (Euro, Japan)
static struct BurnRomInfo ngp_ptennisRomDesc[] = {
	{ "Pocket Tennis - Pocket Sports Series (Euro, Japan)(1998)(SNK - Yumekobo).ngp", 0x80000, 0x4b1eed05, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngp_ptennis, ngp_ptennis, ngpc_ngp)
STD_ROM_FN(ngp_ptennis)

struct BurnDriver BurnDrvngp_ptennis = {
	"ngp_ptennis", NULL, "ngp_ngp", NULL, "1998",
	"Pocket Tennis - Pocket Sports Series (Euro, Japan)\0", NULL, "SNK - Yumekobo", "NeoGeo Pocket",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGP, GBF_SPORTSMISC, 0,
	NgpGetZipName, ngp_ptennisRomInfo, ngp_ptennisRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x8,
	160, 152, 4, 3
};

// Pocket Tennis Color - Pocket Sports Series (World)
static struct BurnRomInfo ngpc_ptenniscRomDesc[] = {
	{ "Pocket Tennis Color - Pocket Sports Series (World)(1999)(SNK - Yumekobo).ngp", 0x80000, 0xd8590b96, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_ptennisc, ngpc_ptennisc, ngpc_ngp)
STD_ROM_FN(ngpc_ptennisc)

struct BurnDriver BurnDrvngpc_ptennisc = {
	"ngp_ptennisc", NULL, "ngp_ngp", NULL, "1999",
	"Pocket Tennis Color - Pocket Sports Series (World)\0", NULL, "SNK - Yumekobo", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_SPORTSMISC, 0,
	NgpGetZipName, ngpc_ptenniscRomInfo, ngpc_ptenniscRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Puyo Pop ~ Puyo Pyuo Tsuu (World, v1.06)
static struct BurnRomInfo ngpc_puyopopRomDesc[] = {
	{ "Puyo Pop - Puyo Pyuo Tsuu v1.06 (World)(1999)(Sega).ngp", 0x100000, 0x5c649d1e, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_puyopop, ngpc_puyopop, ngpc_ngp)
STD_ROM_FN(ngpc_puyopop)

struct BurnDriver BurnDrvngpc_puyopop = {
	"ngp_puyopop", NULL, "ngp_ngp", NULL, "1999",
	"Puyo Pop ~ Puyo Pyuo Tsuu (World, v1.06)\0", NULL, "Sega", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_puyopopRomInfo, ngpc_puyopopRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Puyo Pop ~ Puyo Pyuo Tsuu (World, v1.05)
static struct BurnRomInfo ngpc_puyopopaRomDesc[] = {
	{ "Puyo Pop - Puyo Pyuo Tsuu v1.05 (World)(1999)(Sega).ngp", 0x100000, 0x3090b2fd, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_puyopopa, ngpc_puyopopa, ngpc_ngp)
STD_ROM_FN(ngpc_puyopopa)

struct BurnDriver BurnDrvngpc_puyopopa = {
	"ngp_puyopopa", "ngp_puyopop", "ngp_ngp", NULL, "1999",
	"Puyo Pop ~ Puyo Pyuo Tsuu (World, v1.05)\0", NULL, "Sega", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_puyopopaRomInfo, ngpc_puyopopaRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Puzzle Link (Euro)
static struct BurnRomInfo ngpc_puzzlinkRomDesc[] = {
	{ "Puzzle Link (Euro)(1999)(SNK - Yumekobo).ngp", 0x80000, 0x8d0610ac, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_puzzlink, ngpc_puzzlink, ngpc_ngp)
STD_ROM_FN(ngpc_puzzlink)

struct BurnDriver BurnDrvngpc_puzzlink = {
	"ngp_puzzlink", NULL, "ngp_ngp", NULL, "1999",
	"Puzzle Link (Euro)\0", NULL, "SNK - Yumekobo", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_puzzlinkRomInfo, ngpc_puzzlinkRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Puzzle Link 2 (Euro, USA)
static struct BurnRomInfo ngpc_puzzlnk2RomDesc[] = {
	{ "Puzzle Link 2 (Euro, USA)(1999)(SNK - Yumekobo).ngp", 0x100000, 0xba9ca21c, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_puzzlnk2, ngpc_puzzlnk2, ngpc_ngp)
STD_ROM_FN(ngpc_puzzlnk2)

struct BurnDriver BurnDrvngpc_puzzlnk2 = {
	"ngp_puzzlnk2", NULL, "ngp_ngp", NULL, "1999",
	"Puzzle Link 2 (Euro, USA)\0", NULL, "SNK - Yumekobo", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_puzzlnk2RomInfo, ngpc_puzzlnk2RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Renketsu Puzzle Tsunagete Pon! (Japan)
static struct BurnRomInfo ngp_tsunapnRomDesc[] = {
	{ "Renketsu Puzzle Tsunagete Pon! (Japan)(1998)(SNK - Yumekobo).ngp", 0x80000, 0xce09f534, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngp_tsunapn, ngp_tsunapn, ngpc_ngp)
STD_ROM_FN(ngp_tsunapn)

struct BurnDriver BurnDrvngp_tsunapn = {
	"ngp_tsunapn", NULL, "ngp_ngp", NULL, "1998",
	"Renketsu Puzzle Tsunagete Pon! (Japan)\0", NULL, "SNK - Yumekobo", "NeoGeo Pocket",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGP, GBF_PUZZLE, 0,
	NgpGetZipName, ngp_tsunapnRomInfo, ngp_tsunapnRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x8,
	160, 152, 4, 3
};

// Renketsu Puzzle Tsunagete Pon! Color (Japan)
static struct BurnRomInfo ngpc_tsunapncRomDesc[] = {
	{ "Renketsu Puzzle Tsunagete Pon! Color (Japan)(1999)(SNK - Yumekobo).ngp", 0x80000, 0xc3f3e83c, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_tsunapnc, ngpc_tsunapnc, ngpc_ngp)
STD_ROM_FN(ngpc_tsunapnc)

struct BurnDriver BurnDrvngpc_tsunapnc = {
	"ngp_tsunapnc", "ngp_puzzlink", "ngp_ngp", NULL, "1999",
	"Renketsu Puzzle Tsunagete Pon! Color (Japan)\0", NULL, "SNK - Yumekobo", "NeoGeo Pocket Color",
	L"Renketsu Puzzle Tsunagete Pon! Color (Japan)\0\u9023\u7d50\u30d1\u30ba\u30eb \u3064\u306a\u3052\u3066 \u30dd\u30f3\u30c3!\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_tsunapncRomInfo, ngpc_tsunapncRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Rockman: Battle & Fighters (Japan)
static struct BurnRomInfo ngpc_rockmanbRomDesc[] = {
	{ "Rockman - Battle & Fighters (Japan)(2000)(Capcom).ngp", 0x200000, 0x9c861e49, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_rockmanb, ngpc_rockmanb, ngpc_ngp)
STD_ROM_FN(ngpc_rockmanb)

struct BurnDriver BurnDrvngpc_rockmanb = {
	"ngp_rockmanb", NULL, "ngp_ngp", NULL, "2000",
	"Rockman: Battle & Fighters (Japan)\0", NULL, "Capcom", "NeoGeo Pocket Color",
	L"Rockman: Battle & Fighters (Japan)\0\u30ed\u30c3\u30af\u30de\u30f3 \u30d0\u30c8\u30eb & \u30d5\u30a1\u30a4\u30bf\u30fc\u30ba\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_PLATFORM, 0,
	NgpGetZipName, ngpc_rockmanbRomInfo, ngpc_rockmanbRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Rockman: Battle & Fighters (Japan, Demo)
static struct BurnRomInfo ngpc_rockmanbdRomDesc[] = {
	{ "Rockman - Battle & Fighters (Japan, Demo)(2000)(Capcom).ngp", 0x200000, 0x16a98ac4, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_rockmanbd, ngpc_rockmanbd, ngpc_ngp)
STD_ROM_FN(ngpc_rockmanbd)

struct BurnDriver BurnDrvngpc_rockmanbd = {
	"ngp_rockmanbd", "ngp_rockmanb", "ngp_ngp", NULL, "2000",
	"Rockman: Battle & Fighters (Japan, Demo)\0", NULL, "Capcom", "NeoGeo Pocket Color",
	L"Rockman: Battle & Fighters (Japan, Demo)\0\u30ed\u30c3\u30af\u30de\u30f3 \u30d0\u30c8\u30eb & \u30d5\u30a1\u30a4\u30bf\u30fc\u30ba\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_PLATFORM, 0,
	NgpGetZipName, ngpc_rockmanbdRomInfo, ngpc_rockmanbdRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Samurai Spirit! - Pocket Fighting Series (Japan)
static struct BurnRomInfo ngp_samshoRomDesc[] = {
	{ "Samurai Spirit! - Pocket Fighting Series (Euro, Japan)(1998)(SNK).ngp", 0x200000, 0x32e4696a, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngp_samsho, ngp_samsho, ngpc_ngp)
STD_ROM_FN(ngp_samsho)

struct BurnDriver BurnDrvngp_samsho = {
	"ngp_samsho", NULL, "ngp_ngp", NULL, "1998",
	"Samurai Spirit! - Pocket Fighting Series (Euro, Japan)\0", NULL, "SNK", "NeoGeo Pocket",
	L"Samurai Spirit! - Pocket Fighting Series (Euro, Japan)\0\u30b5\u30e0\u30e9\u30a4\u30b9\u30d4\u30ea\u30c3\u30c4!\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGP, GBF_VSFIGHT, 0,
	NgpGetZipName, ngp_samshoRomInfo, ngp_samshoRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x8,
	160, 152, 4, 3
};

// Samurai Shodown! 2 - Pocket Fighting Series (World)
static struct BurnRomInfo ngpc_samsho2RomDesc[] = {
	{ "Samurai Shodown! 2 - Pocket Fighting Series (World)(1999)(SNK).ngp", 0x200000, 0x4f7fb156, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_samsho2, ngpc_samsho2, ngpc_ngp)
STD_ROM_FN(ngpc_samsho2)

struct BurnDriver BurnDrvngpc_samsho2 = {
	"ngp_samsho2", NULL, "ngp_ngp", NULL, "1999",
	"Samurai Shodown! 2 - Pocket Fighting Series (World)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_samsho2RomInfo, ngpc_samsho2RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Shanghai Mini (World)
static struct BurnRomInfo ngpc_shanghaiRomDesc[] = {
	{ "Shanghai Mini (World)(1999)(SNK - Activision).ngp", 0x100000, 0x90732d53, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_shanghai, ngpc_shanghai, ngpc_ngp)
STD_ROM_FN(ngpc_shanghai)

struct BurnDriver BurnDrvngpc_shanghai = {
	"ngp_shanghai", NULL, "ngp_ngp", NULL, "1999",
	"Shanghai Mini (World)\0", NULL, "SNK - Activision", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_shanghaiRomInfo, ngpc_shanghaiRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Shanghai Mini (USA, Prototype)
static struct BurnRomInfo ngpc_shanghaipuRomDesc[] = {
	{ "Shanghai Mini (USA, Proto)(1999)(SNK - Activision).ngp", 0x100000, 0x920af0ed, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_shanghaipu, ngpc_shanghaipu, ngpc_ngp)
STD_ROM_FN(ngpc_shanghaipu)

struct BurnDriver BurnDrvngpc_shanghaipu = {
	"ngp_shanghaipu", "ngp_shanghai", "ngp_ngp", NULL, "1999",
	"Shanghai Mini (USA, Prototype)\0", NULL, "SNK - Activision", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_shanghaipuRomInfo, ngpc_shanghaipuRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Shanghai Mini (Japan, Prototype)
static struct BurnRomInfo ngpc_shanghaipjRomDesc[] = {
	{ "Shanghai Mini (Japan, Proto)(1999)(SNK - Activision).ngp", 0x100000, 0x79620c3f, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_shanghaipj, ngpc_shanghaipj, ngpc_ngp)
STD_ROM_FN(ngpc_shanghaipj)

struct BurnDriver BurnDrvngpc_shanghaipj = {
	"ngp_shanghaipj", "ngp_shanghai", "ngp_ngp", NULL, "1999",
	"Shanghai Mini (Japan, Prototype)\0", NULL, "SNK - Activision", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_shanghaipjRomInfo, ngpc_shanghaipjRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Shougi no Tatsujin (Japan)
static struct BurnRomInfo ngp_shougiRomDesc[] = {
	{ "Shougi no Tatsujin (Japan)(1998)(SNK - ADK).ngp", 0x80000, 0xf34d0c9b, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngp_shougi, ngp_shougi, ngpc_ngp)
STD_ROM_FN(ngp_shougi)

struct BurnDriver BurnDrvngp_shougi = {
	"ngp_shougi", NULL, "ngp_ngp", NULL, "1998",
	"Shougi no Tatsujin (Japan)\0", NULL, "SNK - ADK Corp.", "NeoGeo Pocket",
	L"Shougi no Tatsujin (Japan)\0\u5c06\u68cb\u306e\u9054\u4eba\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGP, GBF_BOARD | GBF_STRATEGY, 0,
	NgpGetZipName, ngp_shougiRomInfo, ngp_shougiRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x8,
	160, 152, 4, 3
};

// Shougi no Tatsujin Color (Japan)
static struct BurnRomInfo ngpc_shougicRomDesc[] = {
	{ "Shougi no Tatsujin Color (Japan)(1999)(SNK - ADK).ngp", 0x80000, 0x1f2872ed, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_shougic, ngpc_shougic, ngpc_ngp)
STD_ROM_FN(ngpc_shougic)

struct BurnDriver BurnDrvngpc_shougic = {
	"ngp_shougic", NULL, "ngp_ngp", NULL, "1999",
	"Shougi no Tatsujin Color (Japan)\0", NULL, "SNK - ADK Corp.", "NeoGeo Pocket Color",
	L"Shougi no Tatsujin Color (Japan)\0\u5c06\u68cb\u306e\u9054\u4eba Color\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_BOARD | GBF_STRATEGY, 0,
	NgpGetZipName, ngpc_shougicRomInfo, ngpc_shougicRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// SNK Gals' Fighters (Euro, USA)
static struct BurnRomInfo ngpc_snkgalsRomDesc[] = {
	{ "SNK Gals' Fighters (Euro, USA)(2000)(SNK).ngp", 0x200000, 0xb02c2be7, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_snkgals, ngpc_snkgals, ngpc_ngp)
STD_ROM_FN(ngpc_snkgals)

struct BurnDriver BurnDrvngpc_snkgals = {
	"ngp_snkgals", NULL, "ngp_ngp", NULL, "2000",
	"SNK Gals' Fighters (Euro, USA)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_snkgalsRomInfo, ngpc_snkgalsRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// SNK Gals' Fighters (Japan)
static struct BurnRomInfo ngpc_snkgalsjRomDesc[] = {
	{ "SNK Gals' Fighters (Japan)(2000)(SNK).ngp", 0x200000, 0x6a9ffa47, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_snkgalsj, ngpc_snkgalsj, ngpc_ngp)
STD_ROM_FN(ngpc_snkgalsj)

struct BurnDriver BurnDrvngpc_snkgalsj = {
	"ngp_snkgalsj", "ngp_snkgals", "ngp_ngp", NULL, "2000",
	"SNK Gals' Fighters (Japan)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_snkgalsjRomInfo, ngpc_snkgalsjRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Sonic the Hedgehog: Pocket Adventure (World)
static struct BurnRomInfo ngpc_sonicRomDesc[] = {
	{ "Sonic the Hedgehog - Pocket Adventure (World)(1999)(Sega).ngp", 0x200000, 0x356f0849, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_sonic, ngpc_sonic, ngpc_ngp)
STD_ROM_FN(ngpc_sonic)

struct BurnDriver BurnDrvngpc_sonic = {
	"ngp_sonic", NULL, "ngp_ngp", NULL, "1999",
	"Sonic the Hedgehog: Pocket Adventure (World)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_PLATFORM, 0,
	NgpGetZipName, ngpc_sonicRomInfo, ngpc_sonicRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Sonic the Hedgehog: Pocket Adventure (World, Prototype)
static struct BurnRomInfo ngpc_sonicpRomDesc[] = {
	{ "Sonic the Hedgehog: Pocket Adventure (World, Proto)(1999)(Sega).ngp", 0x200000, 0x622176ce, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_sonicp, ngpc_sonicp, ngpc_ngp)
STD_ROM_FN(ngpc_sonicp)

struct BurnDriver BurnDrvngpc_sonicp = {
	"ngp_sonicp", "ngp_sonic", "ngp_ngp", NULL, "1999",
	"Sonic the Hedgehog: Pocket Adventure (World, Prototype)\0", NULL, "Sega", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_SNK_NGPC, GBF_PLATFORM, 0,
	NgpGetZipName, ngpc_sonicpRomInfo, ngpc_sonicpRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Sonic the Hedgehog: Pocket Adventure (World, Prototype 2)
static struct BurnRomInfo ngpc_sonicp2RomDesc[] = {
	{ "Sonic the Hedgehog - Pocket Adventure (World, Proto 2)(1999)(Sega).ngp", 0x200000, 0x45fc3bca, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_sonicp2, ngpc_sonicp2, ngpc_ngp)
STD_ROM_FN(ngpc_sonicp2)

struct BurnDriver BurnDrvngpc_sonicp2 = {
	"ngp_sonicp2", "ngp_sonic", "ngp_ngp", NULL, "1999",
	"Sonic the Hedgehog: Pocket Adventure (World, Prototype 2)\0", NULL, "Sega", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_SNK_NGPC, GBF_PLATFORM, 0,
	NgpGetZipName, ngpc_sonicp2RomInfo, ngpc_sonicp2RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Soreike!! Hanafuda Doujou (Japan)
static struct BurnRomInfo ngpc_hanadojoRomDesc[] = {
	{ "Soreike!! Hanafuda Doujou (Japan)(1999(Dyna Corp.).ngp", 0x80000, 0x05fa3eb0, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_hanadojo, ngpc_hanadojo, ngpc_ngp)
STD_ROM_FN(ngpc_hanadojo)

struct BurnDriver BurnDrvngpc_hanadojo = {
	"ngp_hanadojo", NULL, "ngp_ngp", NULL, "1999",
	"Soreike!! Hanafuda Doujou (Japan)\0", NULL, "Dyna Corp.", "NeoGeo Pocket Color",
	L"Soreike!! Hanafuda Doujou (Japan)\0\u305d\u308c\u3044\u3051!! \u82b1\u672d\u9053\u5834\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_MAHJONG, 0,
	NgpGetZipName, ngpc_hanadojoRomInfo, ngpc_hanadojoRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Super Real Mahjong: Premium Collection (Japan)
static struct BurnRomInfo ngpc_srmpRomDesc[] = {
	{ "Super Real Mahjong: Premium Collection (Japan)(2001)(SNK - Seta Corp.).ngp", 0x200000, 0xc6e620c3, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_srmp, ngpc_srmp, ngpc_ngp)
STD_ROM_FN(ngpc_srmp)

struct BurnDriver BurnDrvngpc_srmp = {
	"ngp_srmp", NULL, "ngp_ngp", NULL, "2001",
	"Super Real Mahjong: Premium Collection (Japan)\0", NULL, "SNK - Seta Corp.", "NeoGeo Pocket Color",
	L"Super Real Mahjong: Premium Collection (Japan)\0\u30b9\u30fc\u30d1\u30fc\u30ea\u30a2\u30eb\u9ebb\u96c0\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_MAHJONG, 0,
	NgpGetZipName, ngpc_srmpRomInfo, ngpc_srmpRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// SNK vs. Capcom: The Match of the Millennium (World)
static struct BurnRomInfo ngpc_svcRomDesc[] = {
	{ "SNK vs. Capcom - The Match of the Millennium (World)(1999)(SNK).ngp", 0x400000, 0xb173030a, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_svc, ngpc_svc, ngpc_ngp)
STD_ROM_FN(ngpc_svc)

struct BurnDriver BurnDrvngpc_svc = {
	"ngp_svc", NULL, "ngp_ngp", NULL, "1999",
	"SNK vs. Capcom: The Match of the Millennium (World)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_svcRomInfo, ngpc_svcRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// SNK vs. Capcom: Card Fighters 2 - Expand Edition (Japan)
static struct BurnRomInfo ngpc_svccard2RomDesc[] = {
	{ "SNK vs. Capcom - Card Fighters 2 - Expand Edition (Japan)(2001)(SNK).ngp", 0x200000, 0xccbcfda7, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_svccard2, ngpc_svccard2, ngpc_ngp)
STD_ROM_FN(ngpc_svccard2)

struct BurnDriver BurnDrvngpc_svccard2 = {
	"ngp_svccard2", NULL, "ngp_ngp", NULL, "2001",
	"SNK vs. Capcom: Card Fighters 2 - Expand Edition (Japan)\0", NULL, "SNK", "NeoGeo Pocket Color",
	L"SNK vs. Capcom: Card Fighters 2 - Expand Edition (Japan)\0\u6fc0\u7a81 \u30ab\u30fc\u30c9 \u30d5\u30a1\u30a4\u30bf\u30fc\u30ba 2\0", NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_CARD | GBF_RPG, 0,
	NgpGetZipName, ngpc_svccard2RomInfo, ngpc_svccard2RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// SNK vs. Capcom: Card Fighters 2 - Expand Edition (Hack, English)
// https://www.romhacking.net/translations/82/
static struct BurnRomInfo ngpc_svccard2eRomDesc[] = {
	{ "SNK vs. Capcom - Card Fighters 2 - Expand Edition T-Eng (2017)(The CFC2 Translation Team).ngp", 0x200000, 0x0cc88d96, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_svccard2e, ngpc_svccard2e, ngpc_ngp)
STD_ROM_FN(ngpc_svccard2e)

struct BurnDriver BurnDrvngpc_svccard2e = {
	"ngp_svccard2e", "ngp_svccard2", "ngp_ngp", NULL, "2017",
	"SNK vs. Capcom: Card Fighters 2 - Expand Edition (Hack, English)\0", NULL, "The CFC2 Translation Team", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_SNK_NGPC, GBF_CARD | GBF_RPG, 0,
	NgpGetZipName, ngpc_svccard2eRomInfo, ngpc_svccard2eRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// SNK vs. Capcom: Card Fighters' Clash - Capcom Ver. (Euro, USA)
static struct BurnRomInfo ngpc_svccardcRomDesc[] = {
	{ "SNK vs. Capcom - Card Fighters' Clash - Capcom Ver. (Euro, USA)(1999)(SNK).ngp", 0x200000, 0x80ce137b, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_svccardc, ngpc_svccardc, ngpc_ngp)
STD_ROM_FN(ngpc_svccardc)

struct BurnDriver BurnDrvngpc_svccardc = {
	"ngp_svccardc", NULL, "ngp_ngp", NULL, "1999",
	"SNK vs. Capcom: Card Fighters' Clash - Capcom Ver. (Euro, USA)\0", "Capcom Supporters Version", "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_CARD | GBF_RPG, 0,
	NgpGetZipName, ngpc_svccardcRomInfo, ngpc_svccardcRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// SNK vs. Capcom: Gekitotsu Card Fighters - Capcom Ver. (Japan)
static struct BurnRomInfo ngpc_svccardcjRomDesc[] = {
	{ "SNK vs. Capcom - Gekitotsu Card Fighters - Capcom Ver. (Japan)(SNK)(1999).ngp", 0x200000, 0x9217159b, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_svccardcj, ngpc_svccardcj, ngpc_ngp)
STD_ROM_FN(ngpc_svccardcj)

struct BurnDriver BurnDrvngpc_svccardcj = {
	"ngp_svccardcj", "ngp_svccardc", "ngp_ngp", NULL, "1999",
	"SNK vs. Capcom: Gekitotsu Card Fighters - Capcom Ver. (Japan)\0", "Capcom Supporters Version", "SNK", "NeoGeo Pocket Color",
	L"SNK vs. Capcom: Gekitotsu Card Fighters - Capcom Ver. (Japan)\0\u6fc0\u7a81 \u30ab\u30fc\u30c9 \u30d5\u30a1\u30a4\u30bf\u30fc\u30ba\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_CARD | GBF_RPG, 0,
	NgpGetZipName, ngpc_svccardcjRomInfo, ngpc_svccardcjRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// SNK vs. Capcom: Gekitotsu Card Fighters (Japan, Demo)
static struct BurnRomInfo ngpc_svccardpRomDesc[] = {
	{ "SNK vs. Capcom: Gekitotsu Card Fighters (Japan, Demo)(1999)(SNK).ngp", 0x200000, 0x4a41a56e, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_svccardp, ngpc_svccardp, ngpc_ngp)
STD_ROM_FN(ngpc_svccardp)

struct BurnDriver BurnDrvngpc_svccardp = {
	"ngp_svccardp", "ngp_svccards", "ngp_ngp", NULL, "1999",
	"SNK vs. Capcom: Gekitotsu Card Fighters (Japan, Demo)\0", NULL, "SNK", "NeoGeo Pocket Color",
	L"SNK vs. Capcom: Gekitotsu Card Fighters (Japan, Demo)\0\u6fc0\u7a81 \u30ab\u30fc\u30c9 \u30d5\u30a1\u30a4\u30bf\u30fc\u30ba\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_CARD | GBF_RPG, 0,
	NgpGetZipName, ngpc_svccardpRomInfo, ngpc_svccardpRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// SNK vs. Capcom: Card Fighters' Clash - SNK Ver. (Euro, USA)
static struct BurnRomInfo ngpc_svccardsRomDesc[] = {
	{ "SNK vs. Capcom - Card Fighters' Clash - SNK Ver. (Euro, USA)(1999)(SNK).ngp", 0x200000, 0x94b63a97, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_svccards, ngpc_svccards, ngpc_ngp)
STD_ROM_FN(ngpc_svccards)

struct BurnDriver BurnDrvngpc_svccards = {
	"ngp_svccards", NULL, "ngp_ngp", NULL, "1999",
	"SNK vs. Capcom: Card Fighters' Clash - SNK Ver. (Euro, USA)\0", "SNK Cardfighter's Version", "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SNK_NGPC, GBF_CARD | GBF_RPG, 0,
	NgpGetZipName, ngpc_svccardsRomInfo, ngpc_svccardsRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// SNK vs. Capcom: Gekitotsu Card Fighters - SNK Ver. (Japan, v7)
static struct BurnRomInfo ngpc_svccardsjRomDesc[] = {
	{ "SNK vs. Capcom: Gekitotsu Card Fighters - SNK Ver. (Japan, v7)(1999)(SNK).ngp", 0x200000, 0x7065295c, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_svccardsj, ngpc_svccardsj, ngpc_ngp)
STD_ROM_FN(ngpc_svccardsj)

struct BurnDriver BurnDrvngpc_svccardsj = {
	"ngp_svccardsj", "ngp_svccards", "ngp_ngp", NULL, "1999",
	"SNK vs. Capcom: Gekitotsu Card Fighters - SNK Ver. (Japan, v7)\0", "SNK Cardfighter's Version", "SNK", "NeoGeo Pocket Color",
	L"SNK vs. Capcom: Gekitotsu Card Fighters - SNK Ver. (Japan, v7)\0\u6fc0\u7a81 \u30ab\u30fc\u30c9 \u30d5\u30a1\u30a4\u30bf\u30fc\u30ba\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_CARD | GBF_RPG, 0,
	NgpGetZipName, ngpc_svccardsjRomInfo, ngpc_svccardsjRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// SNK vs. Capcom: Gekitotsu Card Fighters - SNK Ver. (Japan, v6)
static struct BurnRomInfo ngpc_svccardsjaRomDesc[] = {
	{ "SNK vs. Capcom: Gekitotsu Card Fighters - SNK Ver. (Japan, v6)(1999)(SNK).ngp", 0x200000, 0xe70e3f1a, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_svccardsja, ngpc_svccardsja, ngpc_ngp)
STD_ROM_FN(ngpc_svccardsja)

struct BurnDriver BurnDrvngpc_svccardsja = {
	"ngp_svccardsja", "ngp_svccards", "ngp_ngp", NULL, "1999",
	"SNK vs. Capcom: Gekitotsu Card Fighters - SNK Ver. (Japan, v6)\0", "SNK Cardfighter's Version", "SNK", "NeoGeo Pocket Color",
	L"SNK vs. Capcom: Gekitotsu Card Fighters - SNK Ver. (Japan, v6)\0\u6fc0\u7a81 \u30ab\u30fc\u30c9 \u30d5\u30a1\u30a4\u30bf\u30fc\u30ba\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SNK_NGPC, GBF_CARD | GBF_RPG, 0,
	NgpGetZipName, ngpc_svccardsjaRomInfo, ngpc_svccardsjaRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Tsunagete Pon! 2 (Japan)
static struct BurnRomInfo ngpc_tsunapn2RomDesc[] = {
	{ "Tsunagete Pon 2 (Japan)(1999)(SNK - Yumekobo).ngp", 0x100000, 0x129091bb, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_tsunapn2, ngpc_tsunapn2, ngpc_ngp)
STD_ROM_FN(ngpc_tsunapn2)

struct BurnDriver BurnDrvngpc_tsunapn2 = {
	"ngp_tsunapn2", "ngp_puzzlnk2", "ngp_ngp", NULL, "1999",
	"Tsunagete Pon! 2 (Japan)\0", NULL, "SNK - Yumekobo", "NeoGeo Pocket Color",
	L"Tsunagete Pon! 2 (Japan)\0\u3064\u306a\u3052\u3066\u30dd\u30f3\u30c3!2\0", NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_tsunapn2RomInfo, ngpc_tsunapn2RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Wrestling Madness (Euro, USA, Prototype)
static struct BurnRomInfo ngpc_wrestmadRomDesc[] = {
	{ "Wrestling Madness (Euro, USA, Proto)(2000)(SNK).ngp", 0x200000, 0x16b0be9b, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_wrestmad, ngpc_wrestmad, ngpc_ngp)
STD_ROM_FN(ngpc_wrestmad)

struct BurnDriver BurnDrvngpc_wrestmad = {
	"ngp_wrestmad", NULL, "ngp_ngp", NULL, "2000",
	"Wrestling Madness (Euro, USA, Prototype)\0", NULL, "SNK", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_SNK_NGPC, GBF_VSFIGHT, 0,
	NgpGetZipName, ngpc_wrestmadRomInfo, ngpc_wrestmadRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Prize Game - PP-AA01 Pusher Program (Japan)
static struct BurnRomInfo ngpc_ppaa01RomDesc[] = {
	{ "Prize Game - PP-AA01 Pusher Program (Japan)(199x)(Aruze).ngp", 0x80000, 0x78acdbcb, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_ppaa01, ngpc_ppaa01, ngpc_ngp)
STD_ROM_FN(ngpc_ppaa01)

struct BurnDriverX BurnDrvngpc_ppaa01 = {
	"ngp_ppaa01", NULL, "ngp_ngp", NULL, "199?",
	"Prize Game - PP-AA01 Pusher Program (Japan)\0", NULL, "Aruze Corp.", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SNK_NGPC, GBF_MISC, 0,
	NgpGetZipName, ngpc_ppaa01RomInfo, ngpc_ppaa01RomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Don't Die, Mr. Robot! (HB)
static struct BurnRomInfo ngpc_ddmrRomDesc[] = {
	{ "Don't Die, Mr. Robot! (2025)(Infinite State Games).ngp", 289634, 0x86838a9d, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_ddmr, ngpc_ddmr, ngpc_ngp)
STD_ROM_FN(ngpc_ddmr)

struct BurnDriver BurnDrvngpc_ddmr = {
	"ngp_ddmr", NULL, "ngp_ngp", NULL, "2025",
	"Don't Die, Mr. Robot! (HB)\0", NULL, "Infinite State Games", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SNK_NGPC, GBF_ACTION, 0,
	NgpGetZipName, ngpc_ddmrRomInfo, ngpc_ddmrRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

// Gears of Fate (HB)
static struct BurnRomInfo ngpc_gearsoffateRomDesc[] = {
	{ "Gears of Fate (2009)(Thor).ngp", 1917732, 0x3c75807e, 1 | BRF_PRG | BRF_ESS }, // Cartridge
};

STDROMPICKEXT(ngpc_gearsoffate, ngpc_gearsoffate, ngpc_ngp)
STD_ROM_FN(ngpc_gearsoffate)

struct BurnDriver BurnDrvngpc_gearsoffate = {
	"ngp_gearsoffate", NULL, "ngp_ngp", NULL, "2009",
	"Gears of Fate (HB)\0", NULL, "Thor", "NeoGeo Pocket Color",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_SNK_NGPC, GBF_PUZZLE, 0,
	NgpGetZipName, ngpc_gearsoffateRomInfo, ngpc_gearsoffateRomName, NULL, NULL, NULL, NULL, NgpInputInfo, NgpDIPInfo,
	DrvInit, DrvExit, DrvFrame, k1geDraw, DrvScan, &BurnRecalc, 0x1000,
	160, 152, 4, 3
};

