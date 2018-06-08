// ZX Spectrum driver
// Snapshot formats supported - .SNA, .Z80
// joysticks supported - Kempston (1 stick), Inteface2/Sinclair (2 sticks)

#include "tiles_generic.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "dac.h"
#include "ay8910.h"

#define SPEC_NO_SNAPSHOT			0
#define SPEC_SNAPSHOT_SNA			1
#define SPEC_SNAPSHOT_Z80			2

#define SPEC_SCREEN_XSIZE			256
#define SPEC_SCREEN_YSIZE			192
#define SPEC_BORDER_LEFT			48
#define SPEC_BORDER_TOP				48
#define SPEC_BITMAP_WIDTH			448
#define SPEC_BITMAP_HEIGHT			312

#define SPEC_Z80_SNAPSHOT_INVALID	0
#define SPEC_Z80_SNAPSHOT_48K_OLD	1
#define SPEC_Z80_SNAPSHOT_48K		2
#define SPEC_Z80_SNAPSHOT_SAMRAM	3
#define SPEC_Z80_SNAPSHOT_128K		4
#define SPEC_Z80_SNAPSHOT_TS2068	5

static struct BurnRomInfo emptyRomDesc[] = {
	{ "", 0, 0, 0 },
};

static UINT8 DrvInputPort0[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - Caps - V
static UINT8 DrvInputPort1[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - A - G
static UINT8 DrvInputPort2[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - Q - T
static UINT8 DrvInputPort3[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - 1 - 5
static UINT8 DrvInputPort4[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - 6 - 0
static UINT8 DrvInputPort5[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - Y - P
static UINT8 DrvInputPort6[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - H - Enter
static UINT8 DrvInputPort7[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - B - Space
static UINT8 DrvInputPort8[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // kempston
static UINT8 DrvInputPort9[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // interface 2 - joystick 1
static UINT8 DrvInputPort10[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // interface 2 - joystick 2
static UINT8 DrvInputPort11[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard (128K) Extra Keys
static UINT8 DrvInputPort12[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard (128K) Extra Keys
static UINT8 DrvInputPort13[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard (128K) Extra Keys
static UINT8 DrvInputPort14[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard (128K) Extra Keys
static UINT8 DrvInputPort15[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard (128K) Extra Keys
static UINT8 DrvDip[2]         = {0, 0};
static UINT8 DrvInput[16]      = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static UINT8 DrvReset          = 0;

static UINT8 *Mem              = NULL;
static UINT8 *MemEnd           = NULL;
static UINT8 *RamStart         = NULL;
static UINT8 *RamEnd           = NULL;
static UINT8 *DrvZ80Rom        = NULL;
static UINT8 *DrvVideoRam      = NULL;
static UINT8 *DrvSnapshotData  = NULL;
static UINT8 *DrvZ80Ram        = NULL;
static UINT32 *DrvPalette      = NULL;
static UINT8 DrvRecalc;

static INT32 nActiveSnapshotType = SPEC_NO_SNAPSHOT;
static INT32 nCyclesDone = 0;
static INT32 nScanline = 0;
static INT32 DrvFrameNumber = 0;
static INT32 DrvFrameInvertCount = 0;
static INT32 DrvFlashInvert = 0;
static INT32 DrvNumScanlines = 312;
static INT32 DrvNumCylesPerScanline = 224;
static INT32 DrvVBlankScanline = 310;
static UINT32 DrvHorStartCycles = 0;

static UINT32 nPreviousScreenX = 0;
static UINT32 nPreviousScreenY = 0;
static UINT32 nPreviousBorderX = 0;
static UINT32 nPreviousBorderY = 0;
static UINT8 nPortFEData = 0;

static INT32 DrvIsSpec128 = 0;
static INT32 nPort7FFDData = -1;

static INT32 ay_table[312];
static INT32 ay_table_initted = 0;

static void spectrum_UpdateScreenBitmap(bool);
static void spectrum_UpdateBorderBitmap();
static void spectrum_128_update_memory();

static struct BurnInputInfo DrvInputList[] =
{
	{"Kempston Up"          , BIT_DIGITAL  , DrvInputPort8 + 3 , "p1 up"            },
	{"Kempston Down"        , BIT_DIGITAL  , DrvInputPort8 + 2 , "p1 down"          },
	{"Kempston Left"        , BIT_DIGITAL  , DrvInputPort8 + 1 , "p1 left"          },
	{"Kempston Right"       , BIT_DIGITAL  , DrvInputPort8 + 0 , "p1 right"         },
	{"Kempston Fire"        , BIT_DIGITAL  , DrvInputPort8 + 4 , "p1 fire 1"        },
	
	{"Intf2 Stick 1 Up"     , BIT_DIGITAL  , DrvInputPort9 + 1 , "p1 up"            },
	{"Intf2 Stick 1 Down"   , BIT_DIGITAL  , DrvInputPort9 + 2 , "p1 down"          },
	{"Intf2 Stick 1 Left"   , BIT_DIGITAL  , DrvInputPort9 + 4 , "p1 left"          },
	{"Intf2 Stick 1 Right"  , BIT_DIGITAL  , DrvInputPort9 + 3 , "p1 right"         },
	{"Intf2 Stick 1 Fire"   , BIT_DIGITAL  , DrvInputPort9 + 0 , "p1 fire 1"        },
	
	{"Intf2 Stick 2 Up"     , BIT_DIGITAL  , DrvInputPort10 + 3, "p2 up"            },
	{"Intf2 Stick 2 Down"   , BIT_DIGITAL  , DrvInputPort10 + 2, "p2 down"          },
	{"Intf2 Stick 2 Left"   , BIT_DIGITAL  , DrvInputPort10 + 0, "p2 left"          },
	{"Intf2 Stick 2 Right"  , BIT_DIGITAL  , DrvInputPort10 + 1, "p2 right"         },
	{"Intf2 Stick 2 Fire"   , BIT_DIGITAL  , DrvInputPort10 + 4, "p2 fire 1"        },
	
	{"ENTER"				, BIT_DIGITAL  , DrvInputPort6 + 0, "keyb_enter"        },
	{"SPACE"				, BIT_DIGITAL  , DrvInputPort7 + 0, "keyb_space"        },
	{"CAPS SHIFT"			, BIT_DIGITAL  , DrvInputPort0 + 0, "keyb_left_shift"   },
	{"SYMBOL SHIFT"			, BIT_DIGITAL  , DrvInputPort7 + 1, "keyb_right_shift"  },
	{"Q"					, BIT_DIGITAL  , DrvInputPort2 + 0, "keyb_Q"            },
	{"W"					, BIT_DIGITAL  , DrvInputPort2 + 1, "keyb_W"            },
	{"E"					, BIT_DIGITAL  , DrvInputPort2 + 2, "keyb_E"            },
	{"R"					, BIT_DIGITAL  , DrvInputPort2 + 3, "keyb_R"            },
	{"T"					, BIT_DIGITAL  , DrvInputPort2 + 4, "keyb_T"            },
	{"Y"					, BIT_DIGITAL  , DrvInputPort5 + 4, "keyb_Y"            },
	{"U"					, BIT_DIGITAL  , DrvInputPort5 + 3, "keyb_U"            },
	{"I"					, BIT_DIGITAL  , DrvInputPort5 + 2, "keyb_I"            },
	{"O"					, BIT_DIGITAL  , DrvInputPort5 + 1, "keyb_O"            },
	{"P"					, BIT_DIGITAL  , DrvInputPort5 + 0, "keyb_P"            },
	{"A"					, BIT_DIGITAL  , DrvInputPort1 + 0, "keyb_A"            },
	{"S"					, BIT_DIGITAL  , DrvInputPort1 + 1, "keyb_S"            },
	{"D"					, BIT_DIGITAL  , DrvInputPort1 + 2, "keyb_D"            },
	{"F"					, BIT_DIGITAL  , DrvInputPort1 + 3, "keyb_F"            },
	{"G"					, BIT_DIGITAL  , DrvInputPort1 + 4, "keyb_G"            },
	{"H"					, BIT_DIGITAL  , DrvInputPort6 + 4, "keyb_H"            },
	{"J"					, BIT_DIGITAL  , DrvInputPort6 + 3, "keyb_J"            },
	{"K"					, BIT_DIGITAL  , DrvInputPort6 + 2, "keyb_K"            },
	{"L"					, BIT_DIGITAL  , DrvInputPort6 + 1, "keyb_L"            },
	{"Z"					, BIT_DIGITAL  , DrvInputPort0 + 1, "keyb_Z"            },
	{"X"					, BIT_DIGITAL  , DrvInputPort0 + 2, "keyb_X"            },
	{"C"					, BIT_DIGITAL  , DrvInputPort0 + 3, "keyb_C"            },
	{"V"					, BIT_DIGITAL  , DrvInputPort0 + 4, "keyb_V"            },
	{"B"					, BIT_DIGITAL  , DrvInputPort7 + 4, "keyb_B"            },
	{"N"					, BIT_DIGITAL  , DrvInputPort7 + 3, "keyb_N"            },
	{"M"					, BIT_DIGITAL  , DrvInputPort7 + 2, "keyb_M"            },
	{"1"					, BIT_DIGITAL  , DrvInputPort3 + 0, "keyb_1"            },
	{"2"					, BIT_DIGITAL  , DrvInputPort3 + 1, "keyb_2"            },
	{"3"					, BIT_DIGITAL  , DrvInputPort3 + 2, "keyb_3"            },
	{"4"					, BIT_DIGITAL  , DrvInputPort3 + 3, "keyb_4"            },
	{"5"					, BIT_DIGITAL  , DrvInputPort3 + 4, "keyb_5"            },
	{"6"					, BIT_DIGITAL  , DrvInputPort4 + 4, "keyb_6"            },
	{"7"					, BIT_DIGITAL  , DrvInputPort4 + 3, "keyb_7"            },
	{"8"					, BIT_DIGITAL  , DrvInputPort4 + 2, "keyb_8"            },
	{"9"					, BIT_DIGITAL  , DrvInputPort4 + 1, "keyb_9"            },
	{"0"					, BIT_DIGITAL  , DrvInputPort4 + 0, "keyb_0"            },
	
	{"EDIT"					, BIT_DIGITAL  , DrvInputPort11 + 0, "keyb_insert"      },
	{"CAPS LOCK"			, BIT_DIGITAL  , DrvInputPort11 + 1, "keyb_caps_lock"   },
	{"TRUE VID"				, BIT_DIGITAL  , DrvInputPort11 + 2, "keyb_home"        },
	{"INV VID"				, BIT_DIGITAL  , DrvInputPort11 + 3, "keyb_end"         },
	{"Cursor Left"			, BIT_DIGITAL  , DrvInputPort11 + 4, "keyb_left"        },
	
	{"DEL"					, BIT_DIGITAL  , DrvInputPort12 + 0, "keyb_backspace"   },
	{"GRAPH"				, BIT_DIGITAL  , DrvInputPort12 + 1, "keyb_left_alt"    },
	{"Cursor Right"			, BIT_DIGITAL  , DrvInputPort12 + 2, "keyb_right"       },
	{"Cursor Up"			, BIT_DIGITAL  , DrvInputPort12 + 3, "keyb_up"          },
	{"Cursor Down"			, BIT_DIGITAL  , DrvInputPort12 + 4, "keyb_down"        },
	
	{"BREAK"				, BIT_DIGITAL  , DrvInputPort13 + 0, "keyb_pause"       },
	{"EXT MODE"				, BIT_DIGITAL  , DrvInputPort13 + 1, "keyb_left_ctrl"   },
	
	{"\""					, BIT_DIGITAL  , DrvInputPort14 + 0, "keyb_apost"       },
	{";"					, BIT_DIGITAL  , DrvInputPort14 + 1, "keyb_colon"       },
	
	{"."					, BIT_DIGITAL  , DrvInputPort15 + 0, "keyb_stop"        },
	{","					, BIT_DIGITAL  , DrvInputPort15 + 1, "keyb_comma"       },
	
	{"Reset"				, BIT_DIGITAL  , &DrvReset        , "reset"             },
	{"Dip 1"				, BIT_DIPSWITCH, DrvDip + 0       , "dip"               },
};

STDINPUTINFO(Drv)

static inline void DrvMakeInputs()
{
	DrvInput[0] = DrvInput[1] = DrvInput[2] = DrvInput[3] = DrvInput[4] = DrvInput[5] = DrvInput[6] = DrvInput[7] = DrvInput[9] = DrvInput[10] = DrvInput[11] = DrvInput[12] = DrvInput[13] = DrvInput[14] = DrvInput[15] = 0x1f;
	
	DrvInput[8] = 0x00;

	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0]  -= (DrvInputPort0[i] & 1) << i;
		DrvInput[1]  -= (DrvInputPort1[i] & 1) << i;
		DrvInput[2]  -= (DrvInputPort2[i] & 1) << i;
		DrvInput[3]  -= (DrvInputPort3[i] & 1) << i;
		DrvInput[4]  -= (DrvInputPort4[i] & 1) << i;
		DrvInput[5]  -= (DrvInputPort5[i] & 1) << i;
		DrvInput[6]  -= (DrvInputPort6[i] & 1) << i;
		DrvInput[7]  -= (DrvInputPort7[i] & 1) << i;
		DrvInput[8]  |= (DrvInputPort8[i] & 1) << i;
		DrvInput[9]  -= (DrvInputPort9[i] & 1) << i;
		DrvInput[10] -= (DrvInputPort10[i] & 1) << i;
		DrvInput[11] -= (DrvInputPort11[i] & 1) << i;
		DrvInput[12] -= (DrvInputPort12[i] & 1) << i;
		DrvInput[13] -= (DrvInputPort13[i] & 1) << i;
		DrvInput[14] -= (DrvInputPort14[i] & 1) << i;
		DrvInput[15] -= (DrvInputPort15[i] & 1) << i;
	}
}

static struct BurnDIPInfo DrvDIPList[]=
{
	// Default Values
	{72, 0xff, 0xff, 0x00, NULL                     },
	
	{0 , 0xfe, 0   , 2   , "Hardware Version"       },
	{72, 0x01, 0x80, 0x00, "Issue 2"                },
	{72, 0x01, 0x80, 0x80, "Issue 3"                },
};

STDDIPINFO(Drv)
TCHAR* ANSIToTCHAR(const char* pszString, TCHAR* pszOutString, int nOutSize);
INT32 SpectrumGetZipName(char** pszName, UINT32 i)
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

	if (pszGameName == NULL) {
		*pszName = NULL;
		return 1;
	}
	
	// remove leader
	memset(szFilename, 0, MAX_PATH);
	for (UINT32 j = 0; j < strlen(pszGameName); j++) {
		szFilename[j] = pszGameName[j + 5];
	}
	
	*pszName = szFilename;

	return 0;
}

static struct BurnRomInfo SpectrumRomDesc[] = {
	{ "spectrum.rom",  0x04000, 0xddee531f, BRF_BIOS },
};

STD_ROM_PICK(Spectrum)
STD_ROM_FN(Spectrum)

static struct BurnRomInfo Spec128RomDesc[] = {
	{ "zx128_0.rom",   0x04000, 0xe76799d2, BRF_BIOS },
	{ "zx128_1.rom",   0x04000, 0xb96a36be, BRF_BIOS },
};

STD_ROM_PICK(Spec128)
STD_ROM_FN(Spec128)

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	DrvZ80Rom              = Next; Next += 0x08000;
	DrvSnapshotData        = Next; Next += 0x20000;

	RamStart               = Next;
	DrvZ80Ram              = Next; Next += 0x20000;
	RamEnd                 = Next;
	
	DrvPalette             = (UINT32*)Next; Next += 0x00010 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

static void DrvLoadSNASnapshot()
{
	UINT16 data;
	UINT16 addr;
	
	data = (DrvSnapshotData[22] << 8) | DrvSnapshotData[21];
	ZetSetAF(0, data);
	
	data = (DrvSnapshotData[14] << 8) | DrvSnapshotData[13];
	ZetSetBC(0, data);
	
	data = (DrvSnapshotData[12] << 8) | DrvSnapshotData[11];
	ZetSetDE(0, data);
	
	data = (DrvSnapshotData[10] << 8) | DrvSnapshotData[9];
	ZetSetHL(0, data);

	data = (DrvSnapshotData[8] << 8) | DrvSnapshotData[7];
	ZetSetAF2(0, data);

	data = (DrvSnapshotData[6] << 8) | DrvSnapshotData[5];
	ZetSetBC2(0, data);

	data = (DrvSnapshotData[4] << 8) | DrvSnapshotData[3];
	ZetSetDE2(0, data);

	data = (DrvSnapshotData[2] << 8) | DrvSnapshotData[1];
	ZetSetHL2(0, data);
	
	data = (DrvSnapshotData[18] << 8) | DrvSnapshotData[17];
	ZetSetIX(0, data);

	data = (DrvSnapshotData[16] << 8) | DrvSnapshotData[15];
	ZetSetIY(0, data);

	data = DrvSnapshotData[20];
	ZetSetR(0, data);

	data = DrvSnapshotData[0];
	ZetSetI(0, data);
	
	data = (DrvSnapshotData[24] << 8) | DrvSnapshotData[23];
	ZetSetSP(0, data);

	data = DrvSnapshotData[25] & 0x03;
	if (data == 3) data = 2;
	ZetSetIM(0, data);
	
	data = DrvSnapshotData[19];
	ZetSetIFF1(0, BIT(data, 0));
	ZetSetIFF2(0, BIT(data, 2));
	
	UINT8 intr = BIT(data, 0) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK;
	if (intr == 0) bprintf(PRINT_IMPORTANT, _T("State load INTR=0\n"));
//	ZetSetIRQLine(0, intr);
	//m_maincpu->set_input_line(INPUT_LINE_HALT, CLEAR_LINE);
		
	/*if (snapsize == SNA48_SIZE)
		// 48K Snapshot
		page_basicrom();
	else
	{
		// 128K Snapshot
		m_port_7ffd_data = snapdata[SNA128_OFFSET + 2];
		logerror ("Port 7FFD:%02X\n", m_port_7ffd_data);
		if (snapdata[SNA128_OFFSET + 3])
		{
			// TODO: page TR-DOS ROM when supported
		}
		update_paging();
	}*/
	
//	if (snapsize == SNA48_SIZE) {
		for (INT32 i = 0; i < 0xc000; i++) DrvZ80Ram[i] = DrvSnapshotData[27 + i];

		addr = ZetSP(0);
		if (addr < 0x4000) {
			data = (DrvZ80Rom[addr + 1] << 8) | DrvZ80Rom[addr + 0];
		} else {
			data = (DrvZ80Ram[addr - 0x4000 + 1] << 8) | DrvZ80Ram[addr - 0x4000 + 0];
		}
		ZetSetPC(0, data);
		
#if 0
		space.write_byte(addr + 0, 0); // It's been reported that zeroing these locations fixes the loading
		space.write_byte(addr + 1, 0); // of a few images that were snapshot at a "wrong" instant
#endif

		addr += 2;
		ZetSetSP(0, addr);

		// Set border color
		data = DrvSnapshotData[26] & 0x07;
		nPortFEData = (nPortFEData & 0xf8) | data;
//	} else {
		// 128K
//	}
}

static INT32 spectrum_identify_z80(UINT32 SnapshotSize)
{
	UINT8 lo, hi, data;
	
	if (SnapshotSize < 30) return SPEC_Z80_SNAPSHOT_INVALID;
	
	lo = DrvSnapshotData[6] & 0x0ff;
	hi = DrvSnapshotData[7] & 0x0ff;
	if (hi || lo) return SPEC_Z80_SNAPSHOT_48K_OLD;

	lo = DrvSnapshotData[30] & 0x0ff;
	hi = DrvSnapshotData[31] & 0x0ff;
	data = DrvSnapshotData[34] & 0x0ff;

	if ((hi == 0) && (lo == 23)) {
		switch (data) {
			case 0:
			case 1: return SPEC_Z80_SNAPSHOT_48K;
			case 2: return SPEC_Z80_SNAPSHOT_SAMRAM;
			case 3:
			case 4: return SPEC_Z80_SNAPSHOT_128K;
			case 128: return SPEC_Z80_SNAPSHOT_TS2068;
		}
	}

	if ((hi == 0) && (lo == 54)) {
		switch (data) {
			case 0:
			case 1:
			case 3: return SPEC_Z80_SNAPSHOT_48K;
			case 2: return SPEC_Z80_SNAPSHOT_SAMRAM;
			case 4:
			case 5:
			case 6: return SPEC_Z80_SNAPSHOT_128K;
			case 128: return SPEC_Z80_SNAPSHOT_TS2068;
		}
	}

	return SPEC_Z80_SNAPSHOT_INVALID;
}

static void DrvWriteByte(UINT32 addr, UINT8 data)
{
	if (addr >= 0x4000) {
		DrvZ80Ram[addr - 0x4000] = data;
	}
}

static void update_paging()
{
	// this will be different for +2/+3
	ZetOpen(0);
	spectrum_128_update_memory();
	ZetClose();
}

static void z80_decompress_block(UINT8 *source, UINT32 dest, UINT16 size)
{
	UINT8 ch;
	INT32 i;
	
	do {
		ch = source[0];

		// either start 0f 0x0ed, 0x0ed, xx yy or single 0x0ed
		if (ch == 0x0ed) {
			if (source[1] == 0x0ed)	{
				// 0x0ed, 0x0ed, xx yy - repetition
				UINT8 count;
				UINT8 data;

				count = source[2];

				if (count == 0)	return;

				data = source[3];

				source += 4;

				if (count > size) count = size;

				size -= count;

				for (i = 0; i < count; i++)	{
					DrvWriteByte(dest, data);
					dest++;
				}
			} else {
				// single 0x0ed
				DrvWriteByte(dest, ch);
				dest++;
				source++;
				size--;
			}
		} else {
			// not 0x0ed
			DrvWriteByte(dest, ch);
			dest++;
			source++;
			size--;
		}
	}
	while (size > 0);
}

static void DrvLoadZ80Snapshot()
{
	INT32 z80_type = 0;
	UINT8 lo, hi, data;
	
	struct BurnRomInfo ri;
	ri.nType = 0;
	ri.nLen = 0;
	BurnDrvGetRomInfo(&ri, 0);
	UINT32 SnapshotSize = ri.nLen;
	
	z80_type = spectrum_identify_z80(SnapshotSize);
	
	bprintf(PRINT_IMPORTANT, _T("Snapshot Type %i\n"), z80_type);
	
	switch (z80_type) {
		case SPEC_Z80_SNAPSHOT_INVALID:	return;
		
		case SPEC_Z80_SNAPSHOT_48K_OLD:
		case SPEC_Z80_SNAPSHOT_48K: {
			//if (!strcmp(machine().system().name,"ts2068")) logerror("48K .Z80 file in TS2068\n");
			break;
		}
		
		case SPEC_Z80_SNAPSHOT_128K: {
			/*if (m_port_7ffd_data == -1)
			{
				logerror("Not a 48K .Z80 file\n");
				return;
			}
			if (!strcmp(machine().system().name,"ts2068"))
			{
				logerror("Not a TS2068 .Z80 file\n");
				return;
			}*/
			break;
		}
		
		case SPEC_Z80_SNAPSHOT_TS2068: {
			//if (strcmp(machine().system().name,"ts2068"))	logerror("Not a TS2068 machine\n");
			break;
		}
		case SPEC_Z80_SNAPSHOT_SAMRAM: {
			return;
		}
	}
	
	hi = DrvSnapshotData[0] & 0x0ff;
	lo = DrvSnapshotData[1] & 0x0ff;
	ZetSetAF(0, (hi << 8) | lo);
	
	lo = DrvSnapshotData[2] & 0x0ff;
	hi = DrvSnapshotData[3] & 0x0ff;
	ZetSetBC(0, (hi << 8) | lo);
	
	lo = DrvSnapshotData[4] & 0x0ff;
	hi = DrvSnapshotData[5] & 0x0ff;
	ZetSetHL(0, (hi << 8) | lo);

	lo = DrvSnapshotData[8] & 0x0ff;
	hi = DrvSnapshotData[9] & 0x0ff;
	ZetSetSP(0, (hi << 8) | lo);

	ZetSetI(0, (DrvSnapshotData[10] & 0x0ff));

	data = (DrvSnapshotData[11] & 0x07f) | ((DrvSnapshotData[12] & 0x01) << 7);
	ZetSetR(0, data);

	nPortFEData = (nPortFEData & 0xf8) | ((DrvSnapshotData[12] & 0x0e) >> 1);

	lo = DrvSnapshotData[13] & 0x0ff;
	hi = DrvSnapshotData[14] & 0x0ff;
	ZetSetDE(0, (hi << 8) | lo);

	lo = DrvSnapshotData[15] & 0x0ff;
	hi = DrvSnapshotData[16] & 0x0ff;
	ZetSetBC2(0, (hi << 8) | lo);

	lo = DrvSnapshotData[17] & 0x0ff;
	hi = DrvSnapshotData[18] & 0x0ff;
	ZetSetDE2(0, (hi << 8) | lo);

	lo = DrvSnapshotData[19] & 0x0ff;
	hi = DrvSnapshotData[20] & 0x0ff;
	ZetSetHL2(0, (hi << 8) | lo);

	hi = DrvSnapshotData[21] & 0x0ff;
	lo = DrvSnapshotData[22] & 0x0ff;
	ZetSetAF2(0, (hi << 8) | lo);

	lo = DrvSnapshotData[23] & 0x0ff;
	hi = DrvSnapshotData[24] & 0x0ff;
	ZetSetIY(0, (hi << 8) | lo);

	lo = DrvSnapshotData[25] & 0x0ff;
	hi = DrvSnapshotData[26] & 0x0ff;
	ZetSetIX(0, (hi << 8) | lo);
	
	if (DrvSnapshotData[27] == 0)	{
		ZetSetIFF1(0, 0);
	} else {
		ZetSetIFF1(0, 1);
	}

	//ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
	//m_maincpu->set_input_line(INPUT_LINE_HALT, CLEAR_LINE);
	
	if (DrvSnapshotData[28] != 0) {
		data = 1;
	} else {
		data = 0;
	}
	ZetSetIFF2(0, data);

	ZetSetIM(0, (DrvSnapshotData[29] & 0x03));
	
	if (z80_type == SPEC_Z80_SNAPSHOT_48K_OLD) {
		lo = DrvSnapshotData[6] & 0x0ff;
		hi = DrvSnapshotData[7] & 0x0ff;
		ZetSetPC(0, (hi << 8) | lo);

		//page_basicrom();

		if ((DrvSnapshotData[12] & 0x020) == 0)	{
			bprintf(PRINT_NORMAL, _T("Not compressed\n"));
			//for (i = 0; i < 49152; i++)	space.write_byte(i + 16384, DrvSnapshotData[30 + i]);
		} else {
			z80_decompress_block(DrvSnapshotData + 30, 16384, 49152);
		}
	} else {
		UINT8 *pSource;
		INT32 header_size;

		header_size = 30 + 2 + ((DrvSnapshotData[30] & 0x0ff) | ((DrvSnapshotData[31] & 0x0ff) << 8));

		lo = DrvSnapshotData[32] & 0x0ff;
		hi = DrvSnapshotData[33] & 0x0ff;
		ZetSetPC(0, (hi << 8) | lo);

//		if ((z80_type == SPEC_Z80_SNAPSHOT_128K) || ((z80_type == SPEC_Z80_SNAPSHOT_TS2068) && !strcmp(machine().system().name,"ts2068")))
		if (z80_type == SPEC_Z80_SNAPSHOT_128K) {
			for (INT32 i = 0; i < 16; i++) {
				AY8910Write(0, 1, i);
				AY8910Write(0, 0, DrvSnapshotData[39 + i]);
			}
			AY8910Write(0, 1, DrvSnapshotData[38]);
		}

		pSource = DrvSnapshotData + header_size;

		if (z80_type == SPEC_Z80_SNAPSHOT_48K) {
			// Ensure 48K Basic ROM is used
			//page_basicrom();
		}

		do {
			UINT16 length;
			UINT8 page;
			INT32 Dest = 0;

			length = (pSource[0] & 0x0ff) | ((pSource[1] & 0x0ff) << 8);
			page = pSource[2];
			
			if (z80_type == SPEC_Z80_SNAPSHOT_48K || z80_type == SPEC_Z80_SNAPSHOT_TS2068) {
				switch (page) {
					case 4: Dest = 0x08000; break;
					case 5: Dest = 0x0c000; break;
					case 8: Dest = 0x04000; break;
					default: Dest = 0; break;
				}
			} else {
				if ((page >= 3) && (page <= 10)) {
					Dest = ((page - 3) * 0x4000) + 0x4000;
				} else {
					// Other values correspond to ROM pages
					Dest = 0x0;
				}
			}

			if (Dest != 0) {
				if (length == 0x0ffff) {
					bprintf(PRINT_NORMAL, _T("Not compressed\n"));

					//for (i = 0; i < 16384; i++) space.write_byte(i + Dest, pSource[i]);
				} else {
					z80_decompress_block(&pSource[3], Dest, 16384);
				}
			}

			// go to next block
			pSource += (3 + length);
		}
		while ((pSource - DrvSnapshotData) < SnapshotSize);

		if ((nPort7FFDData != -1) && (z80_type != SPEC_Z80_SNAPSHOT_48K)) {
			// Set up paging
			nPort7FFDData = (DrvSnapshotData[35] & 0x0ff);
			update_paging();
		}
		/*if ((z80_type == SPEC_Z80_SNAPSHOT_48K) && !strcmp(machine().system().name,"ts2068"))
		{
			m_port_f4_data = 0x03;
			m_port_ff_data = 0x00;
			ts2068_update_memory();
		}
		if (z80_type == SPEC_Z80_SNAPSHOT_TS2068 && !strcmp(machine().system().name,"ts2068"))
		{
			m_port_f4_data = snapdata[35];
			m_port_ff_data = snapdata[36];
			ts2068_update_memory();
		}*/
	}
}

static INT32 DrvDoReset()
{
	ZetOpen(0);
	ZetReset();
	ZetClose();
	
	DACReset();
	
	if (DrvIsSpec128) AY8910Reset(0);
	
	nPreviousScreenX = 0;
	nPreviousScreenY = 0;
	nPreviousBorderX = 0;
	nPreviousBorderY = 0;
	nPort7FFDData = 0;
	nPortFEData = 0;
	
	if (DrvIsSpec128) {
		ZetOpen(0);
		spectrum_128_update_memory();
		ZetClose();
	}
	
	if (nActiveSnapshotType == SPEC_SNAPSHOT_SNA) DrvLoadSNASnapshot();
	if (nActiveSnapshotType == SPEC_SNAPSHOT_Z80) DrvLoadZ80Snapshot();
	
	ay_table_initted = 0;
	
	return 0;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (3500000.0000 / (nBurnFPS / 100.0000))));
}

static UINT8 __fastcall DrvZ80Read(UINT16 a)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall DrvZ80Write(UINT16 a, UINT8 d)
{
	if (a < 0x4000) return;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write => %04X, %02X\n"), a, d);
		}
	}
}

static UINT8 DrvIntf2PortFERead(UINT16 offset)
{
	UINT8 data = 0xff;
	UINT8 lines = offset >> 8;

	if ((lines & 8) == 0) data = DrvInput[10] | (0xff ^ 0x1f);

	if ((lines & 16) == 0) data = DrvInput[9] | (0xff ^ 0x1f);
	
	return data;
}

static UINT8 __fastcall DrvZ80PortRead(UINT16 a)
{
	if ((a & 0xff) != 0xfe) {
		if ((a & 0xff) == 0x1f) {
			// kempston
			return DrvInput[8] & 0x1f;
		}
		
		bprintf(PRINT_NORMAL, _T("Read Port %x\n"), a);
		return (nScanline < 193) ? DrvVideoRam[(nScanline & 0xf8) << 2] : 0xff;
	}
	
	UINT8 lines = a >> 8;
	UINT8 data = 0xff;
	
	INT32 cs_extra1 = 0x1f;//m_io_plus0.read_safe(0x1f) & 0x1f;
	INT32 cs_extra2 = 0x1f;//m_io_plus1.read_safe(0x1f) & 0x1f;
	INT32 cs_extra3 = 0x1f;//m_io_plus2.read_safe(0x1f) & 0x1f;
	INT32 ss_extra1 = 0x1f;//m_io_plus3.read_safe(0x1f) & 0x1f;
	INT32 ss_extra2 = 0x1f;//m_io_plus4.read_safe(0x1f) & 0x1f;
	INT32 joy1 = 0x1f;//m_io_joy1.read_safe(0x1f) & 0x1f;
	INT32 joy2 = 0x1f;//m_io_joy2.read_safe(0x1f) & 0x1f;
	
	// keys: Caps - V
	if ((lines & 1) == 0) {
		data &= DrvInput[0];
		
		// CAPS for extra keys
		if (cs_extra1 != 0x1f || cs_extra2 != 0x1f || cs_extra3 != 0x1f) data &= ~0x01;
	}
	
	// keys: A - G
	if ((lines & 2) == 0) data &= DrvInput[1];

	// keys: Q - T
	if ((lines & 4) == 0) data &= DrvInput[2];

	// keys 1 - 5
	if ((lines & 8) == 0) data &= DrvInput[3] & cs_extra1 & joy2;

	// keys: 6 - 0
	if ((lines & 16) == 0) data &= DrvInput[4] & cs_extra2 & joy1;

	// keys: Y - P
	if ((lines & 32) == 0) data &= DrvInput[5] & ss_extra1;

	// keys: H - Enter
	if ((lines & 64) == 0) data &= DrvInput[6];

	// keys: B - Space
	if ((lines & 128) == 0) {
		data &= DrvInput[7] & cs_extra3 & ss_extra2;
		
		// SYMBOL SHIFT for extra keys
		if (ss_extra1 != 0x1f || ss_extra2 != 0x1f) data &= ~0x02;
	}
	
	data |= (0xe0); // Set bits 5-7 - as reset above
	
	// cassette input from wav
	/*if (m_cassette->input() > 0.0038) {
		data &= ~0x40;
	}*/
	
	// expansion port
	data &= DrvIntf2PortFERead(a);
	
	// Issue 2 Spectrums default to having bits 5, 6 & 7 set. Issue 3 Spectrums default to having bits 5 & 7 set and bit 6 reset.
	if (DrvDip[0] & 0x80) data ^= (0x40);
	
	return data;
}

static void __fastcall DrvZ80PortWrite(UINT16 a, UINT8 d)
{
	a &= 0xff;
	
	switch (a) {
		case 0xfe: {
			UINT8 Changed = nPortFEData ^ d;
			
			if ((Changed & 0x07) != 0) {
				spectrum_UpdateBorderBitmap();
			}
			
			if ((Changed & (1 << 4)) !=0 ) {
				DACSignedWrite(0, BIT(d, 4) * 0x10);
			}

			if ((Changed & (1 << 3)) != 0) {
				// write cassette data
				//m_cassette->output((data & (1<<3)) ? -1.0 : +1.0);
				bprintf(PRINT_IMPORTANT, _T("Write Cassette Data %x\n"), d);
			}

			nPortFEData = d;
			
			break;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Port Write => %02X, %02X\n"), a, d);
		}
	}
}

static void spectrum_128_update_memory()
{
	INT32 ram_page = nPort7FFDData & 0x07;
	ZetMapArea(0xc000, 0xffff, 0, DrvZ80Ram + (ram_page << 14));
	ZetMapArea(0xc000, 0xffff, 1, DrvZ80Ram + (ram_page << 14));
	ZetMapArea(0xc000, 0xffff, 2, DrvZ80Ram + (ram_page << 14));
	
	if (BIT(nPort7FFDData, 3)) {
		DrvVideoRam = DrvZ80Ram + (7 << 14);
	} else {
		DrvVideoRam = DrvZ80Ram + (5 << 14);
	}
}

static UINT8 __fastcall DrvSpec128Z80Read(UINT16 a)
{
	if (a < 0x4000) {
		INT32 ROMSelection = BIT(nPort7FFDData, 4);		
		return DrvZ80Rom[(ROMSelection << 14) + a];
	}
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall DrvSpec128Z80Write(UINT16 a, UINT8 d)
{
	if (a <= 0x4000) return;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write => %04X, %02X\n"), a, d);
		}
	}
}

static UINT8 __fastcall DrvSpec128Z80PortRead(UINT16 a)
{
	if ((a & 0xff) != 0xfe) {
		if ((a & 0xff) == 0x1f) {
			// kempston
			return DrvInput[8] & 0x1f;
		}
		
		if (a == 0xfffd) {
			return AY8910Read(0);
		}
		
		bprintf(PRINT_NORMAL, _T("Read Port %x\n"), a);
		return (nScanline < 193) ? DrvVideoRam[0x1800 | ((nScanline & 0xf8) << 2)] : 0xff;
	}
	
	UINT8 lines = a >> 8;
	UINT8 data = 0xff;
	
	INT32 cs_extra1 = DrvInput[11] & 0x1f;
	INT32 cs_extra2 = DrvInput[12] & 0x1f;
	INT32 cs_extra3 = DrvInput[13] & 0x1f;
	INT32 ss_extra1 = DrvInput[14] & 0x1f;
	INT32 ss_extra2 = DrvInput[15] & 0x1f;
	
	INT32 joy1 = 0x1f;//m_io_joy1.read_safe(0x1f) & 0x1f;
	INT32 joy2 = 0x1f;//m_io_joy2.read_safe(0x1f) & 0x1f;
	
	// keys: Caps - V
	if ((lines & 1) == 0) {
		data &= DrvInput[0];
		
		// CAPS for extra keys
		if (cs_extra1 != 0x1f || cs_extra2 != 0x1f || cs_extra3 != 0x1f) data &= ~0x01;
	}
	
	// keys: A - G
	if ((lines & 2) == 0) data &= DrvInput[1];

	// keys: Q - T
	if ((lines & 4) == 0) data &= DrvInput[2];

	// keys 1 - 5
	if ((lines & 8) == 0) data &= DrvInput[3] & cs_extra1 & joy2;

	// keys: 6 - 0
	if ((lines & 16) == 0) data &= DrvInput[4] & cs_extra2 & joy1;

	// keys: Y - P
	if ((lines & 32) == 0) data &= DrvInput[5] & ss_extra1;

	// keys: H - Enter
	if ((lines & 64) == 0) data &= DrvInput[6];

	// keys: B - Space
	if ((lines & 128) == 0) {
		data &= DrvInput[7] & cs_extra3 & ss_extra2;
		
		// SYMBOL SHIFT for extra keys
		if (ss_extra1 != 0x1f || ss_extra2 != 0x1f) data &= ~0x02;
	}
	
	data |= (0xe0); // Set bits 5-7 - as reset above
	
	// cassette input from wav
	/*if (m_cassette->input() > 0.0038) {
		data &= ~0x40;
	}*/
	
	// expansion port
	data &= DrvIntf2PortFERead(a);
	
	// Issue 2 Spectrums default to having bits 5, 6 & 7 set. Issue 3 Spectrums default to having bits 5 & 7 set and bit 6 reset.
	if (DrvDip[0] & 0x80) data ^= (0x40);
	
	return data;
}

static void __fastcall DrvSpec128Z80PortWrite(UINT16 a, UINT8 d)
{
	if (a == 0x7ffd) {
		if (nPort7FFDData & 0x20) return;
			
		if ((nPort7FFDData ^ d) & 0x08) spectrum_UpdateScreenBitmap(false);
		
		nPort7FFDData = d;
		
		spectrum_128_update_memory();
		return;
	}
	
	if ((a & 0xff) == 0xfe) {
		UINT8 Changed = nPortFEData ^ d;
			
		if ((Changed & 0x07) != 0) {
			spectrum_UpdateBorderBitmap();
		}
		
		if ((Changed & (1 << 4)) !=0 ) {
			DACSignedWrite(0, BIT(d, 4) * 0x10);
		}

		if ((Changed & (1 << 3)) != 0) {
			// write cassette data
			//m_cassette->output((data & (1<<3)) ? -1.0 : +1.0);
			bprintf(PRINT_IMPORTANT, _T("Write Cassette Data %x\n"), d);
		}

		nPortFEData = d;
		return;
	}
	
	switch (a) {
		case 0xbefd:
		case 0xbffd: {
			AY8910Write(0, 1, d);
			break;
		}
		
		case 0xfffd: {
			AY8910Write(0, 0, d);
			break;
		}
	
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Port Write => %02X, %04X\n"), a, d);
		}
	}
}

static INT32 SpectrumInit(INT32 nSnapshotType)
{
	nActiveSnapshotType = nSnapshotType;
	
	INT32 nRet = 0, nLen;
	
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	if (nSnapshotType == SPEC_NO_SNAPSHOT) {
		nRet = BurnLoadRom(DrvZ80Rom + 0x00000, 0, 1); if (nRet != 0) return 1;
	} else {
		nRet = BurnLoadRom(DrvSnapshotData + 0x00000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvZ80Rom + 0x00000, 0x80, 1); if (nRet != 0) return 1;
	}
	
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(DrvZ80Read);
	ZetSetWriteHandler(DrvZ80Write);
	ZetSetInHandler(DrvZ80PortRead);
	ZetSetOutHandler(DrvZ80PortWrite);
	ZetMapArea(0x0000, 0x3fff, 0, DrvZ80Rom             );
	ZetMapArea(0x0000, 0x3fff, 2, DrvZ80Rom             );
	ZetMapArea(0x4000, 0xffff, 0, DrvZ80Ram             );
	ZetMapArea(0x4000, 0xffff, 1, DrvZ80Ram             );
	ZetMapArea(0x4000, 0xffff, 2, DrvZ80Ram             );
	ZetClose();
	
	DACInit(0, 0, 0, DrvSyncDAC);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	
	BurnSetRefreshRate(50.0);

	GenericTilesInit();
	
	DrvFrameInvertCount = 16;
	DrvFrameNumber = 0;
	DrvFlashInvert = 0;
	DrvNumScanlines = 312;
	DrvNumCylesPerScanline = 224;
	DrvVBlankScanline = 310;
	nPort7FFDData = -1;
	DrvVideoRam = DrvZ80Ram;
	
	DrvDoReset();
	
	return 0;
}

static INT32 Spectrum128Init(INT32 nSnapshotType)
{
	nActiveSnapshotType = nSnapshotType;
	
	INT32 nRet = 0, nLen;
	
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	if (nSnapshotType == SPEC_NO_SNAPSHOT) {
		nRet = BurnLoadRom(DrvZ80Rom + 0x00000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvZ80Rom + 0x04000, 1, 1); if (nRet != 0) return 1;
	} else {
		nRet = BurnLoadRom(DrvSnapshotData + 0x00000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvZ80Rom + 0x00000, 0x80, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(DrvZ80Rom + 0x04000, 0x81, 1); if (nRet != 0) return 1;
	}
	
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(DrvSpec128Z80Read);
	ZetSetWriteHandler(DrvSpec128Z80Write);
	ZetSetInHandler(DrvSpec128Z80PortRead);
	ZetSetOutHandler(DrvSpec128Z80PortWrite);
	ZetMapArea(0x4000, 0x7fff, 0, DrvZ80Ram + (5 << 14) );
	ZetMapArea(0x4000, 0x7fff, 1, DrvZ80Ram + (5 << 14) );
	ZetMapArea(0x4000, 0x7fff, 2, DrvZ80Ram + (5 << 14) );
	ZetMapArea(0x8000, 0xbfff, 0, DrvZ80Ram + (2 << 14) );
	ZetMapArea(0x8000, 0xbfff, 1, DrvZ80Ram + (2 << 14) );
	ZetMapArea(0x8000, 0xbfff, 2, DrvZ80Ram + (2 << 14) );
	ZetClose();
	
	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	
	AY8910Init(0, 17734475 / 10, 0);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	
	BurnSetRefreshRate(50.0);

	GenericTilesInit();
	
	DrvFrameInvertCount = 16;
	DrvFrameNumber = 0;
	DrvFlashInvert = 0;
	DrvNumScanlines = 312;
	DrvNumCylesPerScanline = 224;
	DrvVBlankScanline = 310;
	DrvIsSpec128 = 1;
	nPort7FFDData = 0;
	DrvVideoRam = DrvZ80Ram;
	
	DrvDoReset();
	
	return 0;
}

static INT32 DrvInit()
{
	return SpectrumInit(SPEC_NO_SNAPSHOT);
}

static INT32 SNASnapshotInit()
{
	return SpectrumInit(SPEC_SNAPSHOT_SNA);
}

static INT32 Z80SnapshotInit()
{
	return SpectrumInit(SPEC_SNAPSHOT_Z80);
}

static INT32 DrvSpec128Init()
{
	return Spectrum128Init(SPEC_NO_SNAPSHOT);
}

static INT32 Z80128KSnapshotInit()
{
	return Spectrum128Init(SPEC_SNAPSHOT_Z80);
}

static INT32 DrvExit()
{
	ZetExit();
	
	DACExit();
	
	if (DrvIsSpec128) AY8910Exit(0);

	GenericTilesExit();

	BurnFree(Mem);
	
	DrvVideoRam = NULL;
	
	nActiveSnapshotType = SPEC_NO_SNAPSHOT;
	nCyclesDone = 0;
	nScanline = 0;
	DrvFrameNumber = 0;
	DrvFrameInvertCount = 0;
	DrvFlashInvert = 0;
	DrvNumScanlines = 312;
	DrvNumCylesPerScanline = 224;
	DrvVBlankScanline = 310;
	DrvIsSpec128 = 0;
	nPort7FFDData = 1;

	return 0;
}

static void DrvCalcPalette()
{
	DrvPalette[0x00] = BurnHighCol(0x00, 0x00, 0x00, 0);
	DrvPalette[0x01] = BurnHighCol(0x00, 0x00, 0xbf, 0);
	DrvPalette[0x02] = BurnHighCol(0xbf, 0x00, 0x00, 0);
	DrvPalette[0x03] = BurnHighCol(0xbf, 0x00, 0xbf, 0);
	DrvPalette[0x04] = BurnHighCol(0x00, 0xbf, 0x00, 0);
	DrvPalette[0x05] = BurnHighCol(0x00, 0xbf, 0xbf, 0);
	DrvPalette[0x06] = BurnHighCol(0xbf, 0xbf, 0x00, 0);
	DrvPalette[0x07] = BurnHighCol(0xbf, 0xbf, 0xbf, 0);
	DrvPalette[0x08] = BurnHighCol(0x00, 0x00, 0x00, 0);
	DrvPalette[0x09] = BurnHighCol(0x00, 0x00, 0xff, 0);
	DrvPalette[0x0a] = BurnHighCol(0xff, 0x00, 0x00, 0);
	DrvPalette[0x0b] = BurnHighCol(0xff, 0x00, 0xff, 0);
	DrvPalette[0x0c] = BurnHighCol(0x00, 0xff, 0x00, 0);
	DrvPalette[0x0d] = BurnHighCol(0x00, 0xff, 0xff, 0);
	DrvPalette[0x0e] = BurnHighCol(0xff, 0xff, 0x00, 0);
	DrvPalette[0x0f] = BurnHighCol(0xff, 0xff, 0xff, 0);
}

static INT32 DrvDraw()
{
	return 0;
}

static void spectrum_UpdateScreenBitmap(bool eof)
{
	UINT32 x = 0; //hpos
	UINT32 y = nScanline;
	
	INT32 width = SPEC_BITMAP_WIDTH;
	INT32 height = SPEC_BITMAP_HEIGHT;
	
	if ((nPreviousScreenX == x) && (nPreviousScreenY == y) && !eof) return;
	
	do {
		UINT16 xSrc = nPreviousScreenX - SPEC_BORDER_LEFT;
		UINT16 ySrc = nPreviousScreenY - SPEC_BORDER_TOP;
		
		if (xSrc < SPEC_SCREEN_XSIZE && ySrc < SPEC_SCREEN_YSIZE) {
			if ((xSrc & 7) == 0) {
				UINT16 *bm = pTransDraw + (nPreviousScreenY * nScreenWidth) + nPreviousScreenX;
				UINT8 attr = *(DrvVideoRam + ((ySrc & 0xF8) << 2) + (xSrc >> 3) + 0x1800);
				UINT8 scr = *(DrvVideoRam + ((ySrc & 7) << 8) + ((ySrc & 0x38) << 2) + ((ySrc & 0xC0) << 5) + (xSrc >> 3));
				UINT16 ink = (attr & 0x07) + ((attr >> 3) & 0x08);
				UINT16 pap = (attr >> 3) & 0x0f;
				
				if (DrvFlashInvert && (attr & 0x80)) scr = ~scr;
						
				for (UINT8 b = 0x80; b != 0; b >>= 1) {
					*bm++ = (scr & b) ? ink : pap;
				}
			}
		}
			
		nPreviousScreenX++;
		
		if (nPreviousScreenX >= width) {
			nPreviousScreenX = 0;
			nPreviousScreenY++;
			
			if (nPreviousScreenY >= height) {
				nPreviousScreenY = 0;
			}
		}
	} while (!((nPreviousScreenX == x) && (nPreviousScreenY == y)));
}

static void spectrum_UpdateBorderBitmap()
{
	UINT32 x = ((ZetTotalCycles() - DrvHorStartCycles) * 2) + 88;
	UINT32 y = nScanline;
	
	if (x > SPEC_BITMAP_WIDTH) {
		x -= SPEC_BITMAP_WIDTH;
		y++;
	}
	if (x > SPEC_BITMAP_WIDTH) return;
	
	INT32 width = SPEC_BITMAP_WIDTH;
	INT32 height = SPEC_BITMAP_HEIGHT;
	
	UINT16 border = nPortFEData & 0x07;
	
	do {
		if ((nPreviousBorderX < SPEC_BORDER_LEFT) || (nPreviousBorderX >= (SPEC_BORDER_LEFT + SPEC_SCREEN_XSIZE)) || (nPreviousBorderY < SPEC_BORDER_TOP) || (nPreviousBorderY >= (SPEC_BORDER_TOP + SPEC_SCREEN_YSIZE))) {
			if (nPreviousBorderX > 0 && nPreviousBorderX < nScreenWidth && nPreviousBorderY > 0 && nPreviousBorderY < nScreenHeight) {
				pTransDraw[(nPreviousBorderY * nScreenWidth) + nPreviousBorderX] = border;
			}
		}

		nPreviousBorderX++;

		if (nPreviousBorderX >= width) {
			nPreviousBorderX = 0;
			nPreviousBorderY++;

			if (nPreviousBorderY >= height) {
				nPreviousBorderY = 0;
			}
		}
	}
	while (!((nPreviousBorderX == x) && (nPreviousBorderY == y)));
}

static void DrvMakeAYUpdateTable()
{ // sample update table for ay-dac at any soundrate, must be called in DrvFrame()!
	if (ay_table_initted) return;

	if (DrvNumScanlines > 312) {
		bprintf(PRINT_ERROR, _T(" !!! make_ay_updatetable(): uhoh, ay_table[] too small!\n"));
		return;
	}

	INT32 p = 0;
	memset(&ay_table, 0, sizeof(ay_table));

	for (INT32 i = 0; i < 312; i++) {
		ay_table[i] = ((i * nBurnSoundLen) / 312) - p;
		p = (i * nBurnSoundLen) / 312;
	}

	ay_table_initted = 1;
}

static INT32 DrvFrame()
{
	if (DrvReset) DrvDoReset();
	
	DrvMakeInputs();
	
	if (pBurnDraw) DrvCalcPalette();
	
	nCyclesDone = 0;
	INT32 nSoundBufferPos = 0;
	DrvMakeAYUpdateTable(); // _must_ be called in-frame since nBurnSoundLen is pre-calculated @ 60hz in DrvDoReset() @ init.
	
	ZetNewFrame();
	ZetOpen(0);
	
	for (nScanline = 0; nScanline < DrvNumScanlines; nScanline++) {
		DrvHorStartCycles = nCyclesDone;
		
		if (nScanline == DrvVBlankScanline) {
			// VBlank
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			nCyclesDone += ZetRun(32);
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			nCyclesDone += ZetRun(DrvNumCylesPerScanline - 32);
			
			spectrum_UpdateBorderBitmap();
			spectrum_UpdateScreenBitmap(true);
			
			DrvFrameNumber++;

			if (DrvFrameNumber >= DrvFrameInvertCount) {
				DrvFrameNumber = 0;
				DrvFlashInvert = !DrvFlashInvert;
			}
		} else {
			nCyclesDone += ZetRun(DrvNumCylesPerScanline);
		}
		
		spectrum_UpdateScreenBitmap(false);
		
		if (DrvIsSpec128 && pBurnSoundOut) {
			INT32 nSegmentLength = ay_table[nScanline];
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	if (pBurnDraw) BurnTransferCopy(DrvPalette);
	
	if (pBurnSoundOut) {
		if (DrvIsSpec128 && pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			if (nSegmentLength) {
				AY8910Render(pSoundBuf, nSegmentLength);
			}
		}

		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}
	
	ZetClose();

	return 0;
}

static INT32 DrvScan(INT32 /*nAction*/, INT32* /*pnMin*/)
{
	//struct BurnArea ba;
	
	/*if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029672;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);			// Scan Z80
	}*/
	
	return 0;
}

struct BurnDriver BurnDrvSpectrumBIOS = {
	"spec_spectrum", NULL, NULL, NULL, "1984",
	"ZX Spectrum\0", "BIOS Only", "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_SPECTRUM, GBF_BIOS, 0,
	SpectrumGetZipName, SpectrumRomInfo, SpectrumRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

struct BurnDriver BurnDrvSpectrum = {
	"spec_spectrum", NULL, NULL, NULL, "1984",
	"ZX Spectrum\0", NULL, "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectrumRomInfo, SpectrumRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

struct BurnDriver BurnDrvSpec128BIOS = {
	"spec_spec128", NULL, NULL, NULL, "1984",
	"ZX Spectrum 128\0", "BIOS Only", "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_SPECTRUM, GBF_BIOS, 0,
	SpectrumGetZipName, Spec128RomInfo, Spec128RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvSpec128Init, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

struct BurnDriver BurnDrvSpec128 = {
	"spec_spec128", NULL, NULL, NULL, "1984",
	"ZX Spectrum 128\0", NULL, "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spec128RomInfo, Spec128RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvSpec128Init, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

// games
static struct BurnRomInfo S48180RomDesc[] = {
	{ "180.z80",     0x08d96, 0x8cba8fcd, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48180, S48180, Spectrum)
STD_ROM_FN(S48180)

struct BurnDriver BurnDrvS48180 = {
	"spec_180", NULL, "spec_spectrum", NULL, "1986",
	"180 (1986)(Mastertronic)\0", NULL, "Mastertronic", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48180RomInfo, S48180RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48ATVSimRomDesc[] = {
	{ "atvsim.z80",     0x0a24d, 0x80171771, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48ATVSim, S48ATVSim, Spectrum)
STD_ROM_FN(S48ATVSim)

struct BurnDriver BurnDrvS48ATVSimRomDesc = {
	"spec_atvsim", NULL, "spec_spectrum", NULL, "1987",
	"ATV Simulator - All Terrain Vehicle\0", NULL, "Code Masters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48ATVSimRomInfo, S48ATVSimRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48BlinkysRomDesc[] = {
	{ "blinkys.z80",     0x09cdf, 0x65b0cd8e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Blinkys, S48Blinkys, Spectrum)
STD_ROM_FN(S48Blinkys)

struct BurnDriver BurnDrvS48Blinkys = {
	"spec_blinkys", NULL, "spec_spectrum", NULL, "1990",
	"Blinky's Scary School\0", NULL, "Zeppelin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48BlinkysRomInfo, S48BlinkysRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48BruceLeeRomDesc[] = {
	{ "brucelee.z80",     0x08ceb, 0x8298df22, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48BruceLee, S48BruceLee, Spectrum)
STD_ROM_FN(S48BruceLee)

struct BurnDriver BurnDrvS48BruceLee = {
	"spec_brucelee", NULL, "spec_spectrum", NULL, "1984",
	"Bruce Lee\0", NULL, "US Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48BruceLeeRomInfo, S48BruceLeeRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S128ChasehqRomDesc[] = {
	{ "chasehq.z80",     0x1c04f, 0xbb5ae933, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S128Chasehq, S128Chasehq, Spec128)
STD_ROM_FN(S128Chasehq)

struct BurnDriver BurnDrvS128Chasehq = {
	"spec_chasehq", NULL, "spec_spec128", NULL, "1989",
	"Chase HQ (128K)\0", NULL, "Ocean Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S128ChasehqRomInfo, S128ChasehqRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80128KSnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48ChasehqRomDesc[] = {
	{ "chasehq_48.z80",     0x0a6f7, 0x5e684c1f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Chasehq, S48Chasehq, Spectrum)
STD_ROM_FN(S48Chasehq)

struct BurnDriver BurnDrvS48Chasehq = {
	"spec_chasehq48", "spec_chasehq", "spec_spectrum", NULL, "1989",
	"Chase HQ (48K)\0", NULL, "Ocean Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48ChasehqRomInfo, S48ChasehqRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48ChuckieRomDesc[] = {
	{ "chuckie.z80",     0x04c8b, 0xf274304f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Chuckie, S48Chuckie, Spectrum)
STD_ROM_FN(S48Chuckie)

struct BurnDriver BurnDrvS48Chuckie = {
	"spec_chuckie", NULL, "spec_spectrum", NULL, "1983",
	"Chuckie Egg\0", NULL, "A & F Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48ChuckieRomInfo, S48ChuckieRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Chuckie2RomDesc[] = {
	{ "chuckie2",     0x0a5d9, 0xd5aa2184, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Chuckie2, S48Chuckie2, Spectrum)
STD_ROM_FN(S48Chuckie2)

struct BurnDriver BurnDrvS48Chuckie2 = {
	"spec_chuckie2", NULL, "spec_spectrum", NULL, "1985",
	"Chuckie Egg 2 - Choccy Egg\0", NULL, "A & F Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Chuckie2RomInfo, S48Chuckie2RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48CommandoRomDesc[] = {
	{ "comando.z80",     0x0a0dc, 0x6cabf85d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Commando, S48Commando, Spectrum)
STD_ROM_FN(S48Commando)

struct BurnDriver BurnDrvS48Commando = {
	"spec_commando", NULL, "spec_spectrum", NULL, "1985",
	"Commando\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48CommandoRomInfo, S48CommandoRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48DizzyRomDesc[] = {
	{ "dizzy.z80",     0x0b21b, 0xfe79e93a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy, S48Dizzy, Spectrum)
STD_ROM_FN(S48Dizzy)

struct BurnDriver BurnDrvS48Dizzy = {
	"spec_dizzy", NULL, "spec_spectrum", NULL, "1987",
	"Dizzy - The Ultimate Cartoon Adventure\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48DizzyRomInfo, S48DizzyRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Dizzy2RomDesc[] = {
	{ "dizzy2.z80",     0x0b571, 0x63c7c2a9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy2, S48Dizzy2, Spectrum)
STD_ROM_FN(S48Dizzy2)

struct BurnDriver BurnDrvS48Dizzy2 = {
	"spec_dizzy2", NULL, "spec_spectrum", NULL, "1988",
	"Dizzy II - Treasure Island Dizzy\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Dizzy2RomInfo, S48Dizzy2RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Dizzy3RomDesc[] = {
	{ "dizzy3.z80",     0x0b391, 0xb4c2f20b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy3, S48Dizzy3, Spectrum)
STD_ROM_FN(S48Dizzy3)

struct BurnDriver BurnDrvS48Dizzy3 = {
	"spec_dizzy3", NULL, "spec_spectrum", NULL, "1989",
	"Dizzy III - Fantasy World Dizzy\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Dizzy3RomInfo, S48Dizzy3RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Dizzy4RomDesc[] = {
	{ "dizzy4.z80",     0x0b54a, 0x19009d03, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy4, S48Dizzy4, Spectrum)
STD_ROM_FN(S48Dizzy4)

struct BurnDriver BurnDrvS48Dizzy4 = {
	"spec_dizzy4", NULL, "spec_spectrum", NULL, "1989",
	"Dizzy IV - Magicland Dizzy\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Dizzy4RomInfo, S48Dizzy4RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Dizzy5RomDesc[] = {
	{ "dizzy5.z80",     0x0b9c1, 0xdf6849fe, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy5, S48Dizzy5, Spectrum)
STD_ROM_FN(S48Dizzy5)

struct BurnDriver BurnDrvS48Dizzy5 = {
	"spec_dizzy5", NULL, "spec_spectrum", NULL, "1991",
	"Dizzy V - Spellbound Dizzy\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Dizzy5RomInfo, S48Dizzy5RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Dizzy6RomDesc[] = {
	{ "dizzy6.z80",     0x0a17a, 0xd3791a7a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy6, S48Dizzy6, Spectrum)
STD_ROM_FN(S48Dizzy6)

struct BurnDriver BurnDrvS48Dizzy6 = {
	"spec_dizzy6", NULL, "spec_spectrum", NULL, "1991",
	"Dizzy VI - Prince Of The Yolkfolk\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Dizzy6RomInfo, S48Dizzy6RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Dizzy7RomDesc[] = {
	{ "dizzy7.z80",     0x0a58f, 0x9ca9af5e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy7, S48Dizzy7, Spectrum)
STD_ROM_FN(S48Dizzy7)

struct BurnDriver BurnDrvS48Dizzy7 = {
	"spec_dizzy7", NULL, "spec_spectrum", NULL, "1992",
	"Dizzy VII - Crystal Kingdom Dizzy\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Dizzy7RomInfo, S48Dizzy7RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48FeudRomDesc[] = {
	{ "feud.z80",     0x096cf, 0xe9d169a7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Feud, S48Feud, Spectrum)
STD_ROM_FN(S48Feud)

struct BurnDriver BurnDrvS48Feud = {
	"spec_feud", NULL, "spec_spectrum", NULL, "1987",
	"Feud\0", NULL, "Mastertronic", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48FeudRomInfo, S48FeudRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48FootDirRomDesc[] = {
	{ "footdir.z80",     0x09abb, 0x6fdbf2bd, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48FootDir, S48FootDir, Spectrum)
STD_ROM_FN(S48FootDir)

struct BurnDriver BurnDrvS48FootDir = {
	"spec_footdir", NULL, "spec_spectrum", NULL, "1989",
	"Football Director\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48FootDirRomInfo, S48FootDirRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48FootmanRomDesc[] = {
	{ "footman.z80",     0x0824d, 0x54fd204c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Footman, S48Footman, Spectrum)
STD_ROM_FN(S48Footman)

struct BurnDriver BurnDrvS48Footman = {
	"spec_footman", NULL, "spec_spectrum", NULL, "1982",
	"Football Manager\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48FootmanRomInfo, S48FootmanRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Footman2RomDesc[] = {
	{ "footman2.z80",     0x0aad0, 0xb305dce4, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Footman2, S48Footman2, Spectrum)
STD_ROM_FN(S48Footman2)

struct BurnDriver BurnDrvS48Footman2 = {
	"spec_footman2", NULL, "spec_spectrum", NULL, "1988",
	"Football Manager 2\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Footman2RomInfo, S48Footman2RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Footman3RomDesc[] = {
	{ "footman3.z80",     0x069f2, 0x33db96d5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Footman3, S48Footman3, Spectrum)
STD_ROM_FN(S48Footman3)

struct BurnDriver BurnDrvS48Footman3 = {
	"spec_footman3", NULL, "spec_spectrum", NULL, "1991",
	"Football Manager 3\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Footman3RomInfo, S48Footman3RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48FotyRomDesc[] = {
	{ "foty.z80",     0x08137, 0x2af522d0, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Foty, S48Foty, Spectrum)
STD_ROM_FN(S48Foty)

struct BurnDriver BurnDrvS48Foty = {
	"spec_foty", NULL, "spec_spectrum", NULL, "1986",
	"Footballer Of The Year\0", NULL, "Gremlin Graphics", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48FotyRomInfo, S48FotyRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Foty2RomDesc[] = {
	{ "foty2.z80",     0x08d52, 0x94a14ed8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Foty2, S48Foty2, Spectrum)
STD_ROM_FN(S48Foty2)

struct BurnDriver BurnDrvS48Foty2 = {
	"spec_foty2", NULL, "spec_spectrum", NULL, "1986",
	"Footballer Of The Year 2\0", NULL, "Gremlin Graphics", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Foty2RomInfo, S48Foty2RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48GlhotshtRomDesc[] = {
	{ "glhotsht.z80",     0x08a0a, 0x18e1d943, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Glhotsht, S48Glhotsht, Spectrum)
STD_ROM_FN(S48Glhotsht)

struct BurnDriver BurnDrvS48Glhotsht = {
	"spec_glhotsht", NULL, "spec_spectrum", NULL, "1988",
	"Gary Lineker's Hot Shot\0", NULL, "Gremlin Graphics", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48GlhotshtRomInfo, S48GlhotshtRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48GlssoccRomDesc[] = {
	{ "glssocc.z80",     0x09c44, 0x700248d9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Glssocc, S48Glssocc, Spectrum)
STD_ROM_FN(S48Glssocc)

struct BurnDriver BurnDrvS48Glssocc = {
	"spec_glssocc", NULL, "spec_spectrum", NULL, "1987",
	"Gary Lineker's Superstar Soccer\0", NULL, "Gremlin Graphics", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48GlssoccRomInfo, S48GlssoccRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48HowbstdRomDesc[] = {
	{ "howbstd.z80",     0x0a37f, 0x559834ba, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Howbstd, S48Howbstd, Spectrum)
STD_ROM_FN(S48Howbstd)

struct BurnDriver BurnDrvS48Howbstd = {
	"spec_howbstd", NULL, "spec_spectrum", NULL, "1987",
	"How To Be A Complete Bastard\0", NULL, "Virgin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48HowbstdRomInfo, S48HowbstdRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48HunchbkRomDesc[] = {
	{ "hunchbk.z80",     0x07c76, 0xa7dde347, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Hunchbk, S48Hunchbk, Spectrum)
STD_ROM_FN(S48Hunchbk)

struct BurnDriver BurnDrvS48Hunchbk = {
	"spec_hunchbk", NULL, "spec_spectrum", NULL, "1983",
	"Hunchback\0", NULL, "Ocean Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48HunchbkRomInfo, S48HunchbkRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48JetpacRomDesc[] = {
	{ "jetpac.z80",     0x02aad, 0x4f96d444, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Jetpac, S48Jetpac, Spectrum)
STD_ROM_FN(S48Jetpac)

struct BurnDriver BurnDrvS48Jetpac = {
	"spec_jetpac", NULL, "spec_spectrum", NULL, "1983",
	"Jetpac\0", NULL, "Ultimate Play the Game", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48JetpacRomInfo, S48JetpacRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48MatchdayRomDesc[] = {
	{ "matchday.z80",     0x0a809, 0x59d3bc21, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Matchday, S48Matchday, Spectrum)
STD_ROM_FN(S48Matchday)

struct BurnDriver BurnDrvS48Matchday = {
	"spec_matchday", NULL, "spec_spectrum", NULL, "1987",
	"Match Day\0", NULL, "Ocean Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48MatchdayRomInfo, S48MatchdayRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Matchdy2RomDesc[] = {
	{ "matchdy2.z80", 0x09ea5, 0x0e8fc511, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Matchdy2, S48Matchdy2, Spectrum)
STD_ROM_FN(S48Matchdy2)

struct BurnDriver BurnDrvS48Matchday2 = {
	"spec_matchdy2", NULL, "spec_spectrum", NULL, "1987",
	"Match Day 2\0", NULL, "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Matchdy2RomInfo, S48Matchdy2RomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48MicrsoccRomDesc[] = {
	{ "micrsocc.z80",     0x07ed8, 0x5125611a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Micrsocc, S48Micrsocc, Spectrum)
STD_ROM_FN(S48Micrsocc)

struct BurnDriver BurnDrvS48Micrsocc = {
	"spec_micrsocc", NULL, "spec_spectrum", NULL, "1989",
	"Microprose Soccer\0", NULL, "Microprose", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48MicrsoccRomInfo, S48MicrsoccRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48HandbmarRomDesc[] = {
	{ "handbmar.z80",     0x08e08, 0x37a3f14c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Handbmar, S48Handbmar, Spectrum)
STD_ROM_FN(S48Handbmar)

struct BurnDriver BurnDrvS48Handbmar = {
	"spec_handbmar", NULL, "spec_spectrum", NULL, "1986",
	"Peter Shilton's Handball Maradona\0", NULL, "Grandslam Entertainment", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48HandbmarRomInfo, S48HandbmarRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48PippoRomDesc[] = {
	{ "pippo.sna",     0x0c01b, 0xcdd49a9f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Pippo, S48Pippo, Spectrum)
STD_ROM_FN(S48Pippo)

struct BurnDriver BurnDrvS48Pippo = {
	"spec_pippo", NULL, "spec_spectrum", NULL, "1986",
	"Pippo\0", NULL, "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48PippoRomInfo, S48PippoRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	SNASnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48RenegadeRomDesc[] = {
	{ "renegade.z80",     0x0a2d7, 0x9faf0d9e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Renegade, S48Renegade, Spectrum)
STD_ROM_FN(S48Renegade)

struct BurnDriver BurnDrvS48Renegade = {
	"spec_renegade", NULL, "spec_spectrum", NULL, "1987",
	"Renegade\0", NULL, "Ocean Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48RenegadeRomInfo, S48RenegadeRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S128RobocopRomDesc[] = {
	{ "robocop.z80",     0x1bc85, 0xe5ae03c8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S128Robocop, S128Robocop, Spec128)
STD_ROM_FN(S128Robocop)

struct BurnDriver BurnDrvS128Robocop = {
	"spec_robocop", NULL, "spec_spec128", NULL, "1988",
	"Robocop (128K)\0", NULL, "Ocean Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S128RobocopRomInfo, S128RobocopRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80128KSnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48ThebossRomDesc[] = {
	{ "theboss.z80",     0x093f2, 0xafbdbe2a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Theboss, S48Theboss, Spectrum)
STD_ROM_FN(S48Theboss)

struct BurnDriver BurnDrvS48Theboss = {
	"spec_theboss", NULL, "spec_spectrum", NULL, "1984",
	"The Boss\0", NULL, "Peaksoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48ThebossRomInfo, S48ThebossRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	Z80SnapshotInit, DrvExit, DrvFrame, DrvDraw, DrvScan,
	&DrvRecalc, 0x10, 352, 296, 4, 3
};
