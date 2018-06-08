// ZX Spectrum driver
// Snapshot formats supported - .SNA (48K only for now), .Z80
// joysticks supported - Kempston (1 stick), Inteface2/Sinclair (2 sticks)

#include "spectrum.h"

#define SPEC_SCREEN_XSIZE			256
#define SPEC_SCREEN_YSIZE			192
#define SPEC_BORDER_LEFT			48
#define SPEC_BORDER_TOP				48
#define SPEC_BITMAP_WIDTH			448
#define SPEC_BITMAP_HEIGHT			312

static struct BurnRomInfo emptyRomDesc[] = {
	{ "", 0, 0, 0 },
};

static UINT8 SpecInputPort0[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - Caps - V
static UINT8 SpecInputPort1[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - A - G
static UINT8 SpecInputPort2[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - Q - T
static UINT8 SpecInputPort3[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - 1 - 5
static UINT8 SpecInputPort4[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - 6 - 0
static UINT8 SpecInputPort5[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - Y - P
static UINT8 SpecInputPort6[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - H - Enter
static UINT8 SpecInputPort7[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard - B - Space
static UINT8 SpecInputPort8[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // kempston
static UINT8 SpecInputPort9[8]  = {0, 0, 0, 0, 0, 0, 0, 0}; // interface 2 - joystick 1
static UINT8 SpecInputPort10[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // interface 2 - joystick 2
static UINT8 SpecInputPort11[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard (128K) Extra Keys
static UINT8 SpecInputPort12[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard (128K) Extra Keys
static UINT8 SpecInputPort13[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard (128K) Extra Keys
static UINT8 SpecInputPort14[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard (128K) Extra Keys
static UINT8 SpecInputPort15[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // keyboard (128K) Extra Keys
static UINT8 SpecDip[2]         = {0, 0};
static UINT8 SpecInput[16]      = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static UINT8 SpecReset          = 0;

static UINT8 *Mem               = NULL;
static UINT8 *MemEnd            = NULL;
static UINT8 *RamStart          = NULL;
static UINT8 *RamEnd            = NULL;
UINT8 *SpecZ80Rom               = NULL;
UINT8 *SpecVideoRam             = NULL;
UINT8 *SpecSnapshotData         = NULL;
UINT8 *SpecZ80Ram               = NULL;
static UINT32 *SpecPalette      = NULL;
static UINT8 SpecRecalc;

static INT32 nActiveSnapshotType = SPEC_NO_SNAPSHOT;
static INT32 nCyclesDone = 0;
static INT32 nScanline = 0;
static INT32 SpecFrameNumber = 0;
static INT32 SpecFrameInvertCount = 0;
static INT32 SpecFlashInvert = 0;
static INT32 SpecNumScanlines = 312;
static INT32 SpecNumCylesPerScanline = 224;
static INT32 SpecVBlankScanline = 310;
static UINT32 SpecHorStartCycles = 0;

static UINT32 nPreviousScreenX = 0;
static UINT32 nPreviousScreenY = 0;
static UINT32 nPreviousBorderX = 0;
static UINT32 nPreviousBorderY = 0;
UINT8 nPortFEData = 0;
INT32 nPort7FFDData = -1;

static INT32 SpecIsSpec128 = 0;

static INT32 ay_table[312];
static INT32 ay_table_initted = 0;

static struct BurnInputInfo SpecInputList[] =
{
	{"Kempston Up"          , BIT_DIGITAL  , SpecInputPort8 + 3 , "p1 up"            },
	{"Kempston Down"        , BIT_DIGITAL  , SpecInputPort8 + 2 , "p1 down"          },
	{"Kempston Left"        , BIT_DIGITAL  , SpecInputPort8 + 1 , "p1 left"          },
	{"Kempston Right"       , BIT_DIGITAL  , SpecInputPort8 + 0 , "p1 right"         },
	{"Kempston Fire"        , BIT_DIGITAL  , SpecInputPort8 + 4 , "p1 fire 1"        },
	
	{"Intf2 Stick 1 Up"     , BIT_DIGITAL  , SpecInputPort9 + 1 , "p1 up"            },
	{"Intf2 Stick 1 Down"   , BIT_DIGITAL  , SpecInputPort9 + 2 , "p1 down"          },
	{"Intf2 Stick 1 Left"   , BIT_DIGITAL  , SpecInputPort9 + 4 , "p1 left"          },
	{"Intf2 Stick 1 Right"  , BIT_DIGITAL  , SpecInputPort9 + 3 , "p1 right"         },
	{"Intf2 Stick 1 Fire"   , BIT_DIGITAL  , SpecInputPort9 + 0 , "p1 fire 1"        },
	
	{"Intf2 Stick 2 Up"     , BIT_DIGITAL  , SpecInputPort10 + 3, "p2 up"            },
	{"Intf2 Stick 2 Down"   , BIT_DIGITAL  , SpecInputPort10 + 2, "p2 down"          },
	{"Intf2 Stick 2 Left"   , BIT_DIGITAL  , SpecInputPort10 + 0, "p2 left"          },
	{"Intf2 Stick 2 Right"  , BIT_DIGITAL  , SpecInputPort10 + 1, "p2 right"         },
	{"Intf2 Stick 2 Fire"   , BIT_DIGITAL  , SpecInputPort10 + 4, "p2 fire 1"        },
	
	{"ENTER"				, BIT_DIGITAL  , SpecInputPort6 + 0, "keyb_enter"        },
	{"SPACE"				, BIT_DIGITAL  , SpecInputPort7 + 0, "keyb_space"        },
	{"CAPS SHIFT"			, BIT_DIGITAL  , SpecInputPort0 + 0, "keyb_left_shift"   },
	{"SYMBOL SHIFT"			, BIT_DIGITAL  , SpecInputPort7 + 1, "keyb_right_shift"  },
	{"Q"					, BIT_DIGITAL  , SpecInputPort2 + 0, "keyb_Q"            },
	{"W"					, BIT_DIGITAL  , SpecInputPort2 + 1, "keyb_W"            },
	{"E"					, BIT_DIGITAL  , SpecInputPort2 + 2, "keyb_E"            },
	{"R"					, BIT_DIGITAL  , SpecInputPort2 + 3, "keyb_R"            },
	{"T"					, BIT_DIGITAL  , SpecInputPort2 + 4, "keyb_T"            },
	{"Y"					, BIT_DIGITAL  , SpecInputPort5 + 4, "keyb_Y"            },
	{"U"					, BIT_DIGITAL  , SpecInputPort5 + 3, "keyb_U"            },
	{"I"					, BIT_DIGITAL  , SpecInputPort5 + 2, "keyb_I"            },
	{"O"					, BIT_DIGITAL  , SpecInputPort5 + 1, "keyb_O"            },
	{"P"					, BIT_DIGITAL  , SpecInputPort5 + 0, "keyb_P"            },
	{"A"					, BIT_DIGITAL  , SpecInputPort1 + 0, "keyb_A"            },
	{"S"					, BIT_DIGITAL  , SpecInputPort1 + 1, "keyb_S"            },
	{"D"					, BIT_DIGITAL  , SpecInputPort1 + 2, "keyb_D"            },
	{"F"					, BIT_DIGITAL  , SpecInputPort1 + 3, "keyb_F"            },
	{"G"					, BIT_DIGITAL  , SpecInputPort1 + 4, "keyb_G"            },
	{"H"					, BIT_DIGITAL  , SpecInputPort6 + 4, "keyb_H"            },
	{"J"					, BIT_DIGITAL  , SpecInputPort6 + 3, "keyb_J"            },
	{"K"					, BIT_DIGITAL  , SpecInputPort6 + 2, "keyb_K"            },
	{"L"					, BIT_DIGITAL  , SpecInputPort6 + 1, "keyb_L"            },
	{"Z"					, BIT_DIGITAL  , SpecInputPort0 + 1, "keyb_Z"            },
	{"X"					, BIT_DIGITAL  , SpecInputPort0 + 2, "keyb_X"            },
	{"C"					, BIT_DIGITAL  , SpecInputPort0 + 3, "keyb_C"            },
	{"V"					, BIT_DIGITAL  , SpecInputPort0 + 4, "keyb_V"            },
	{"B"					, BIT_DIGITAL  , SpecInputPort7 + 4, "keyb_B"            },
	{"N"					, BIT_DIGITAL  , SpecInputPort7 + 3, "keyb_N"            },
	{"M"					, BIT_DIGITAL  , SpecInputPort7 + 2, "keyb_M"            },
	{"1"					, BIT_DIGITAL  , SpecInputPort3 + 0, "keyb_1"            },
	{"2"					, BIT_DIGITAL  , SpecInputPort3 + 1, "keyb_2"            },
	{"3"					, BIT_DIGITAL  , SpecInputPort3 + 2, "keyb_3"            },
	{"4"					, BIT_DIGITAL  , SpecInputPort3 + 3, "keyb_4"            },
	{"5"					, BIT_DIGITAL  , SpecInputPort3 + 4, "keyb_5"            },
	{"6"					, BIT_DIGITAL  , SpecInputPort4 + 4, "keyb_6"            },
	{"7"					, BIT_DIGITAL  , SpecInputPort4 + 3, "keyb_7"            },
	{"8"					, BIT_DIGITAL  , SpecInputPort4 + 2, "keyb_8"            },
	{"9"					, BIT_DIGITAL  , SpecInputPort4 + 1, "keyb_9"            },
	{"0"					, BIT_DIGITAL  , SpecInputPort4 + 0, "keyb_0"            },
	
	{"EDIT"					, BIT_DIGITAL  , SpecInputPort11 + 0, "keyb_insert"      },
	{"CAPS LOCK"			, BIT_DIGITAL  , SpecInputPort11 + 1, "keyb_caps_lock"   },
	{"TRUE VID"				, BIT_DIGITAL  , SpecInputPort11 + 2, "keyb_home"        },
	{"INV VID"				, BIT_DIGITAL  , SpecInputPort11 + 3, "keyb_end"         },
	{"Cursor Left"			, BIT_DIGITAL  , SpecInputPort11 + 4, "keyb_left"        },
	
	{"DEL"					, BIT_DIGITAL  , SpecInputPort12 + 0, "keyb_backspace"   },
	{"GRAPH"				, BIT_DIGITAL  , SpecInputPort12 + 1, "keyb_left_alt"    },
	{"Cursor Right"			, BIT_DIGITAL  , SpecInputPort12 + 2, "keyb_right"       },
	{"Cursor Up"			, BIT_DIGITAL  , SpecInputPort12 + 3, "keyb_up"          },
	{"Cursor Down"			, BIT_DIGITAL  , SpecInputPort12 + 4, "keyb_down"        },
	
	{"BREAK"				, BIT_DIGITAL  , SpecInputPort13 + 0, "keyb_pause"       },
	{"EXT MODE"				, BIT_DIGITAL  , SpecInputPort13 + 1, "keyb_left_ctrl"   },
	
	{"\""					, BIT_DIGITAL  , SpecInputPort14 + 0, "keyb_apost"       },
	{";"					, BIT_DIGITAL  , SpecInputPort14 + 1, "keyb_colon"       },
	
	{"."					, BIT_DIGITAL  , SpecInputPort15 + 0, "keyb_stop"        },
	{","					, BIT_DIGITAL  , SpecInputPort15 + 1, "keyb_comma"       },
	
	{"Reset"				, BIT_DIGITAL  , &SpecReset        , "reset"             },
	{"Dip 1"				, BIT_DIPSWITCH, SpecDip + 0       , "dip"               },
};

STDINPUTINFO(Spec)

static inline void SpecMakeInputs()
{
	SpecInput[0] = SpecInput[1] = SpecInput[2] = SpecInput[3] = SpecInput[4] = SpecInput[5] = SpecInput[6] = SpecInput[7] = SpecInput[9] = SpecInput[10] = SpecInput[11] = SpecInput[12] = SpecInput[13] = SpecInput[14] = SpecInput[15] = 0x1f;
	
	SpecInput[8] = 0x00;

	for (INT32 i = 0; i < 8; i++) {
		SpecInput[0]  -= (SpecInputPort0[i] & 1) << i;
		SpecInput[1]  -= (SpecInputPort1[i] & 1) << i;
		SpecInput[2]  -= (SpecInputPort2[i] & 1) << i;
		SpecInput[3]  -= (SpecInputPort3[i] & 1) << i;
		SpecInput[4]  -= (SpecInputPort4[i] & 1) << i;
		SpecInput[5]  -= (SpecInputPort5[i] & 1) << i;
		SpecInput[6]  -= (SpecInputPort6[i] & 1) << i;
		SpecInput[7]  -= (SpecInputPort7[i] & 1) << i;
		SpecInput[8]  |= (SpecInputPort8[i] & 1) << i;
		SpecInput[9]  -= (SpecInputPort9[i] & 1) << i;
		SpecInput[10] -= (SpecInputPort10[i] & 1) << i;
		SpecInput[11] -= (SpecInputPort11[i] & 1) << i;
		SpecInput[12] -= (SpecInputPort12[i] & 1) << i;
		SpecInput[13] -= (SpecInputPort13[i] & 1) << i;
		SpecInput[14] -= (SpecInputPort14[i] & 1) << i;
		SpecInput[15] -= (SpecInputPort15[i] & 1) << i;
	}
}

static struct BurnDIPInfo SpecDIPList[]=
{
	// Default Values
	{72, 0xff, 0xff, 0x00, NULL                     },
	
	{0 , 0xfe, 0   , 2   , "Hardware Version"       },
	{72, 0x01, 0x80, 0x00, "Issue 2"                },
	{72, 0x01, 0x80, 0x80, "Issue 3"                },
};

STDDIPINFO(Spec)

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

	SpecZ80Rom              = Next; Next += 0x08000;
	SpecSnapshotData        = Next; Next += 0x20000;

	RamStart               = Next;
	SpecZ80Ram              = Next; Next += 0x20000;
	RamEnd                 = Next;
	
	SpecPalette             = (UINT32*)Next; Next += 0x00010 * sizeof(UINT32);

	MemEnd                 = Next;

	return 0;
}

static INT32 SpecDoReset()
{
	ZetOpen(0);
	ZetReset();
	ZetClose();
	
	DACReset();
	
	if (SpecIsSpec128) AY8910Reset(0);
	
	nPreviousScreenX = 0;
	nPreviousScreenY = 0;
	nPreviousBorderX = 0;
	nPreviousBorderY = 0;
	nPort7FFDData = 0;
	nPortFEData = 0;
	
	if (SpecIsSpec128) {
		ZetOpen(0);
		spectrum_128_update_memory();
		ZetClose();
	}
	
	if (nActiveSnapshotType == SPEC_SNAPSHOT_SNA) SpecLoadSNASnapshot();
	if (nActiveSnapshotType == SPEC_SNAPSHOT_Z80) SpecLoadZ80Snapshot();
	
	ay_table_initted = 0;
	
	return 0;
}

static INT32 SpecSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (3500000.0000 / (nBurnFPS / 100.0000))));
}

static UINT8 __fastcall SpecZ80Read(UINT16 a)
{
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall SpecZ80Write(UINT16 a, UINT8 d)
{
	if (a < 0x4000) return;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write => %04X, %02X\n"), a, d);
		}
	}
}

static UINT8 SpecIntf2PortFERead(UINT16 offset)
{
	UINT8 data = 0xff;
	UINT8 lines = offset >> 8;

	if ((lines & 8) == 0) data = SpecInput[10] | (0xff ^ 0x1f);

	if ((lines & 16) == 0) data = SpecInput[9] | (0xff ^ 0x1f);
	
	return data;
}

static UINT8 __fastcall SpecZ80PortRead(UINT16 a)
{
	if ((a & 0xff) != 0xfe) {
		if ((a & 0xff) == 0x1f) {
			// kempston
			return SpecInput[8] & 0x1f;
		}
		
		bprintf(PRINT_NORMAL, _T("Read Port %x\n"), a);
		return (nScanline < 193) ? SpecVideoRam[(nScanline & 0xf8) << 2] : 0xff;
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
		data &= SpecInput[0];
		
		// CAPS for extra keys
		if (cs_extra1 != 0x1f || cs_extra2 != 0x1f || cs_extra3 != 0x1f) data &= ~0x01;
	}
	
	// keys: A - G
	if ((lines & 2) == 0) data &= SpecInput[1];

	// keys: Q - T
	if ((lines & 4) == 0) data &= SpecInput[2];

	// keys 1 - 5
	if ((lines & 8) == 0) data &= SpecInput[3] & cs_extra1 & joy2;

	// keys: 6 - 0
	if ((lines & 16) == 0) data &= SpecInput[4] & cs_extra2 & joy1;

	// keys: Y - P
	if ((lines & 32) == 0) data &= SpecInput[5] & ss_extra1;

	// keys: H - Enter
	if ((lines & 64) == 0) data &= SpecInput[6];

	// keys: B - Space
	if ((lines & 128) == 0) {
		data &= SpecInput[7] & cs_extra3 & ss_extra2;
		
		// SYMBOL SHIFT for extra keys
		if (ss_extra1 != 0x1f || ss_extra2 != 0x1f) data &= ~0x02;
	}
	
	data |= (0xe0); // Set bits 5-7 - as reset above
	
	// cassette input from wav
	/*if (m_cassette->input() > 0.0038) {
		data &= ~0x40;
	}*/
	
	// expansion port
	data &= SpecIntf2PortFERead(a);
	
	// Issue 2 Spectrums default to having bits 5, 6 & 7 set. Issue 3 Spectrums default to having bits 5 & 7 set and bit 6 reset.
	if (SpecDip[0] & 0x80) data ^= (0x40);
	
	return data;
}

static void __fastcall SpecZ80PortWrite(UINT16 a, UINT8 d)
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

void spectrum_128_update_memory()
{
	INT32 ram_page = nPort7FFDData & 0x07;
	ZetMapArea(0xc000, 0xffff, 0, SpecZ80Ram + (ram_page << 14));
	ZetMapArea(0xc000, 0xffff, 1, SpecZ80Ram + (ram_page << 14));
	ZetMapArea(0xc000, 0xffff, 2, SpecZ80Ram + (ram_page << 14));
	
	if (BIT(nPort7FFDData, 3)) {
		SpecVideoRam = SpecZ80Ram + (7 << 14);
	} else {
		SpecVideoRam = SpecZ80Ram + (5 << 14);
	}
}

static UINT8 __fastcall SpecSpec128Z80Read(UINT16 a)
{
	if (a < 0x4000) {
		INT32 ROMSelection = BIT(nPort7FFDData, 4);		
		return SpecZ80Rom[(ROMSelection << 14) + a];
	}
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Read => %04X\n"), a);
		}
	}

	return 0;
}

static void __fastcall SpecSpec128Z80Write(UINT16 a, UINT8 d)
{
	if (a <= 0x4000) return;
	
	switch (a) {
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 Write => %04X, %02X\n"), a, d);
		}
	}
}

static UINT8 __fastcall SpecSpec128Z80PortRead(UINT16 a)
{
	if ((a & 0xff) != 0xfe) {
		if ((a & 0xff) == 0x1f) {
			// kempston
			return SpecInput[8] & 0x1f;
		}
		
		if (a == 0xfffd) {
			return AY8910Read(0);
		}
		
		bprintf(PRINT_NORMAL, _T("Read Port %x\n"), a);
		return (nScanline < 193) ? SpecVideoRam[0x1800 | ((nScanline & 0xf8) << 2)] : 0xff;
	}
	
	UINT8 lines = a >> 8;
	UINT8 data = 0xff;
	
	INT32 cs_extra1 = SpecInput[11] & 0x1f;
	INT32 cs_extra2 = SpecInput[12] & 0x1f;
	INT32 cs_extra3 = SpecInput[13] & 0x1f;
	INT32 ss_extra1 = SpecInput[14] & 0x1f;
	INT32 ss_extra2 = SpecInput[15] & 0x1f;
	
	INT32 joy1 = 0x1f;//m_io_joy1.read_safe(0x1f) & 0x1f;
	INT32 joy2 = 0x1f;//m_io_joy2.read_safe(0x1f) & 0x1f;
	
	// keys: Caps - V
	if ((lines & 1) == 0) {
		data &= SpecInput[0];
		
		// CAPS for extra keys
		if (cs_extra1 != 0x1f || cs_extra2 != 0x1f || cs_extra3 != 0x1f) data &= ~0x01;
	}
	
	// keys: A - G
	if ((lines & 2) == 0) data &= SpecInput[1];

	// keys: Q - T
	if ((lines & 4) == 0) data &= SpecInput[2];

	// keys 1 - 5
	if ((lines & 8) == 0) data &= SpecInput[3] & cs_extra1 & joy2;

	// keys: 6 - 0
	if ((lines & 16) == 0) data &= SpecInput[4] & cs_extra2 & joy1;

	// keys: Y - P
	if ((lines & 32) == 0) data &= SpecInput[5] & ss_extra1;

	// keys: H - Enter
	if ((lines & 64) == 0) data &= SpecInput[6];

	// keys: B - Space
	if ((lines & 128) == 0) {
		data &= SpecInput[7] & cs_extra3 & ss_extra2;
		
		// SYMBOL SHIFT for extra keys
		if (ss_extra1 != 0x1f || ss_extra2 != 0x1f) data &= ~0x02;
	}
	
	data |= (0xe0); // Set bits 5-7 - as reset above
	
	// cassette input from wav
	/*if (m_cassette->input() > 0.0038) {
		data &= ~0x40;
	}*/
	
	// expansion port
	data &= SpecIntf2PortFERead(a);
	
	// Issue 2 Spectrums default to having bits 5, 6 & 7 set. Issue 3 Spectrums default to having bits 5 & 7 set and bit 6 reset.
	if (SpecDip[0] & 0x80) data ^= (0x40);
	
	return data;
}

static void __fastcall SpecSpec128Z80PortWrite(UINT16 a, UINT8 d)
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
		nRet = BurnLoadRom(SpecZ80Rom + 0x00000, 0, 1); if (nRet != 0) return 1;
	} else {
		nRet = BurnLoadRom(SpecSnapshotData + 0x00000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(SpecZ80Rom + 0x00000, 0x80, 1); if (nRet != 0) return 1;
	}
	
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(SpecZ80Read);
	ZetSetWriteHandler(SpecZ80Write);
	ZetSetInHandler(SpecZ80PortRead);
	ZetSetOutHandler(SpecZ80PortWrite);
	ZetMapArea(0x0000, 0x3fff, 0, SpecZ80Rom             );
	ZetMapArea(0x0000, 0x3fff, 2, SpecZ80Rom             );
	ZetMapArea(0x4000, 0xffff, 0, SpecZ80Ram             );
	ZetMapArea(0x4000, 0xffff, 1, SpecZ80Ram             );
	ZetMapArea(0x4000, 0xffff, 2, SpecZ80Ram             );
	ZetClose();
	
	DACInit(0, 0, 0, SpecSyncDAC);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	
	BurnSetRefreshRate(50.0);

	GenericTilesInit();
	
	SpecFrameInvertCount = 16;
	SpecFrameNumber = 0;
	SpecFlashInvert = 0;
	SpecNumScanlines = 312;
	SpecNumCylesPerScanline = 224;
	SpecVBlankScanline = 310;
	nPort7FFDData = -1;
	SpecVideoRam = SpecZ80Ram;
	
	SpecDoReset();
	
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
		nRet = BurnLoadRom(SpecZ80Rom + 0x00000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(SpecZ80Rom + 0x04000, 1, 1); if (nRet != 0) return 1;
	} else {
		nRet = BurnLoadRom(SpecSnapshotData + 0x00000, 0, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(SpecZ80Rom + 0x00000, 0x80, 1); if (nRet != 0) return 1;
		nRet = BurnLoadRom(SpecZ80Rom + 0x04000, 0x81, 1); if (nRet != 0) return 1;
	}
	
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(SpecSpec128Z80Read);
	ZetSetWriteHandler(SpecSpec128Z80Write);
	ZetSetInHandler(SpecSpec128Z80PortRead);
	ZetSetOutHandler(SpecSpec128Z80PortWrite);
	ZetMapArea(0x4000, 0x7fff, 0, SpecZ80Ram + (5 << 14) );
	ZetMapArea(0x4000, 0x7fff, 1, SpecZ80Ram + (5 << 14) );
	ZetMapArea(0x4000, 0x7fff, 2, SpecZ80Ram + (5 << 14) );
	ZetMapArea(0x8000, 0xbfff, 0, SpecZ80Ram + (2 << 14) );
	ZetMapArea(0x8000, 0xbfff, 1, SpecZ80Ram + (2 << 14) );
	ZetMapArea(0x8000, 0xbfff, 2, SpecZ80Ram + (2 << 14) );
	ZetClose();
	
	DACInit(0, 0, 1, SpecSyncDAC);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	
	AY8910Init(0, 17734475 / 10, 0);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);
	
	BurnSetRefreshRate(50.0);

	GenericTilesInit();
	
	SpecFrameInvertCount = 16;
	SpecFrameNumber = 0;
	SpecFlashInvert = 0;
	SpecNumScanlines = 312;
	SpecNumCylesPerScanline = 224;
	SpecVBlankScanline = 310;
	SpecIsSpec128 = 1;
	nPort7FFDData = 0;
	SpecVideoRam = SpecZ80Ram;
	
	SpecDoReset();
	
	return 0;
}

static INT32 SpecInit()
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

static INT32 SpecSpec128Init()
{
	return Spectrum128Init(SPEC_NO_SNAPSHOT);
}

static INT32 Z80128KSnapshotInit()
{
	return Spectrum128Init(SPEC_SNAPSHOT_Z80);
}

static INT32 SpecExit()
{
	ZetExit();
	
	DACExit();
	
	if (SpecIsSpec128) AY8910Exit(0);

	GenericTilesExit();

	BurnFree(Mem);
	
	SpecVideoRam = NULL;
	
	nActiveSnapshotType = SPEC_NO_SNAPSHOT;
	nCyclesDone = 0;
	nScanline = 0;
	SpecFrameNumber = 0;
	SpecFrameInvertCount = 0;
	SpecFlashInvert = 0;
	SpecNumScanlines = 312;
	SpecNumCylesPerScanline = 224;
	SpecVBlankScanline = 310;
	SpecIsSpec128 = 0;
	nPort7FFDData = 1;

	return 0;
}

static void SpecCalcPalette()
{
	SpecPalette[0x00] = BurnHighCol(0x00, 0x00, 0x00, 0);
	SpecPalette[0x01] = BurnHighCol(0x00, 0x00, 0xbf, 0);
	SpecPalette[0x02] = BurnHighCol(0xbf, 0x00, 0x00, 0);
	SpecPalette[0x03] = BurnHighCol(0xbf, 0x00, 0xbf, 0);
	SpecPalette[0x04] = BurnHighCol(0x00, 0xbf, 0x00, 0);
	SpecPalette[0x05] = BurnHighCol(0x00, 0xbf, 0xbf, 0);
	SpecPalette[0x06] = BurnHighCol(0xbf, 0xbf, 0x00, 0);
	SpecPalette[0x07] = BurnHighCol(0xbf, 0xbf, 0xbf, 0);
	SpecPalette[0x08] = BurnHighCol(0x00, 0x00, 0x00, 0);
	SpecPalette[0x09] = BurnHighCol(0x00, 0x00, 0xff, 0);
	SpecPalette[0x0a] = BurnHighCol(0xff, 0x00, 0x00, 0);
	SpecPalette[0x0b] = BurnHighCol(0xff, 0x00, 0xff, 0);
	SpecPalette[0x0c] = BurnHighCol(0x00, 0xff, 0x00, 0);
	SpecPalette[0x0d] = BurnHighCol(0x00, 0xff, 0xff, 0);
	SpecPalette[0x0e] = BurnHighCol(0xff, 0xff, 0x00, 0);
	SpecPalette[0x0f] = BurnHighCol(0xff, 0xff, 0xff, 0);
}

static INT32 SpecDraw()
{
	return 0;
}

void spectrum_UpdateScreenBitmap(bool eof)
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
				UINT8 attr = *(SpecVideoRam + ((ySrc & 0xF8) << 2) + (xSrc >> 3) + 0x1800);
				UINT8 scr = *(SpecVideoRam + ((ySrc & 7) << 8) + ((ySrc & 0x38) << 2) + ((ySrc & 0xC0) << 5) + (xSrc >> 3));
				UINT16 ink = (attr & 0x07) + ((attr >> 3) & 0x08);
				UINT16 pap = (attr >> 3) & 0x0f;
				
				if (SpecFlashInvert && (attr & 0x80)) scr = ~scr;
						
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

void spectrum_UpdateBorderBitmap()
{
	UINT32 x = ((ZetTotalCycles() - SpecHorStartCycles) * 2) + 88;
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

static void SpecMakeAYUpdateTable()
{ // sample update table for ay-dac at any soundrate, must be called in SpecFrame()!
	if (ay_table_initted) return;

	if (SpecNumScanlines > 312) {
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

static INT32 SpecFrame()
{
	if (SpecReset) SpecDoReset();
	
	SpecMakeInputs();
	
	if (pBurnDraw) SpecCalcPalette();
	
	nCyclesDone = 0;
	INT32 nSoundBufferPos = 0;
	SpecMakeAYUpdateTable(); // _must_ be called in-frame since nBurnSoundLen is pre-calculated @ 60hz in SpecDoReset() @ init.
	
	ZetNewFrame();
	ZetOpen(0);
	
	for (nScanline = 0; nScanline < SpecNumScanlines; nScanline++) {
		SpecHorStartCycles = nCyclesDone;
		
		if (nScanline == SpecVBlankScanline) {
			// VBlank
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			nCyclesDone += ZetRun(32);
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			nCyclesDone += ZetRun(SpecNumCylesPerScanline - 32);
			
			spectrum_UpdateBorderBitmap();
			spectrum_UpdateScreenBitmap(true);
			
			SpecFrameNumber++;

			if (SpecFrameNumber >= SpecFrameInvertCount) {
				SpecFrameNumber = 0;
				SpecFlashInvert = !SpecFlashInvert;
			}
		} else {
			nCyclesDone += ZetRun(SpecNumCylesPerScanline);
		}
		
		spectrum_UpdateScreenBitmap(false);
		
		if (SpecIsSpec128 && pBurnSoundOut) {
			INT32 nSegmentLength = ay_table[nScanline];
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	if (pBurnDraw) BurnTransferCopy(SpecPalette);
	
	if (pBurnSoundOut) {
		if (SpecIsSpec128 && pBurnSoundOut) {
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

static INT32 SpecScan(INT32 /*nAction*/, INT32* /*pnMin*/)
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

struct BurnDriver BurnSpecSpectrumBIOS = {
	"spec_spectrum", NULL, NULL, NULL, "1984",
	"ZX Spectrum\0", "BIOS Only", "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_SPECTRUM, GBF_BIOS, 0,
	SpectrumGetZipName, SpectrumRomInfo, SpectrumRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	SpecInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

struct BurnDriver BurnSpecSpectrum = {
	"spec_spec48k", NULL, NULL, NULL, "1984",
	"ZX Spectrum\0", NULL, "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectrumRomInfo, SpectrumRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	SpecInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

struct BurnDriver BurnSpecSpec128BIOS = {
	"spec_spec128", NULL, NULL, NULL, "1984",
	"ZX Spectrum 128\0", "BIOS Only", "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_SPECTRUM, GBF_BIOS, 0,
	SpectrumGetZipName, Spec128RomInfo, Spec128RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	SpecSpec128Init, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

struct BurnDriver BurnSpecSpec128 = {
	"spec_spec128k", NULL, NULL, NULL, "1984",
	"ZX Spectrum 128\0", NULL, "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spec128RomInfo, Spec128RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	SpecSpec128Init, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// games
static struct BurnRomInfo S48180RomDesc[] = {
	{ "180.z80",     0x08d96, 0x8cba8fcd, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48180, S48180, Spectrum)
STD_ROM_FN(S48180)

struct BurnDriver BurnSpecS48180 = {
	"spec_180", NULL, "spec_spectrum", NULL, "1986",
	"180 (1986)(Mastertronic)\0", NULL, "Mastertronic", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48180RomInfo, S48180RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48ATVSimRomDesc[] = {
	{ "atvsim.z80",     0x0a24d, 0x80171771, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48ATVSim, S48ATVSim, Spectrum)
STD_ROM_FN(S48ATVSim)

struct BurnDriver BurnSpecS48ATVSimRomDesc = {
	"spec_atvsim", NULL, "spec_spectrum", NULL, "1987",
	"ATV Simulator - All Terrain Vehicle\0", NULL, "Code Masters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48ATVSimRomInfo, S48ATVSimRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48BlinkysRomDesc[] = {
	{ "blinkys.z80",     0x09cdf, 0x65b0cd8e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Blinkys, S48Blinkys, Spectrum)
STD_ROM_FN(S48Blinkys)

struct BurnDriver BurnSpecS48Blinkys = {
	"spec_blinkys", NULL, "spec_spectrum", NULL, "1990",
	"Blinky's Scary School\0", NULL, "Zeppelin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48BlinkysRomInfo, S48BlinkysRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48BruceLeeRomDesc[] = {
	{ "brucelee.z80",     0x08ceb, 0x8298df22, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48BruceLee, S48BruceLee, Spectrum)
STD_ROM_FN(S48BruceLee)

struct BurnDriver BurnSpecS48BruceLee = {
	"spec_brucelee", NULL, "spec_spectrum", NULL, "1984",
	"Bruce Lee\0", NULL, "US Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48BruceLeeRomInfo, S48BruceLeeRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S128ChasehqRomDesc[] = {
	{ "chasehq.z80",     0x1c04f, 0xbb5ae933, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S128Chasehq, S128Chasehq, Spec128)
STD_ROM_FN(S128Chasehq)

struct BurnDriver BurnSpecS128Chasehq = {
	"spec_chasehq", NULL, "spec_spec128", NULL, "1989",
	"Chase HQ (128K)\0", NULL, "Ocean Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S128ChasehqRomInfo, S128ChasehqRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48ChasehqRomDesc[] = {
	{ "chasehq_48.z80",     0x0a6f7, 0x5e684c1f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Chasehq, S48Chasehq, Spectrum)
STD_ROM_FN(S48Chasehq)

struct BurnDriver BurnSpecS48Chasehq = {
	"spec_chasehq48", "spec_chasehq", "spec_spectrum", NULL, "1989",
	"Chase HQ (48K)\0", NULL, "Ocean Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48ChasehqRomInfo, S48ChasehqRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48ChuckieRomDesc[] = {
	{ "chuckie.z80",     0x04c8b, 0xf274304f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Chuckie, S48Chuckie, Spectrum)
STD_ROM_FN(S48Chuckie)

struct BurnDriver BurnSpecS48Chuckie = {
	"spec_chuckie", NULL, "spec_spectrum", NULL, "1983",
	"Chuckie Egg\0", NULL, "A & F Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48ChuckieRomInfo, S48ChuckieRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Chuckie2RomDesc[] = {
	{ "chuckie2",     0x0a5d9, 0xd5aa2184, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Chuckie2, S48Chuckie2, Spectrum)
STD_ROM_FN(S48Chuckie2)

struct BurnDriver BurnSpecS48Chuckie2 = {
	"spec_chuckie2", NULL, "spec_spectrum", NULL, "1985",
	"Chuckie Egg 2 - Choccy Egg\0", NULL, "A & F Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Chuckie2RomInfo, S48Chuckie2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48CommandoRomDesc[] = {
	{ "comando.z80",     0x0a0dc, 0x6cabf85d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Commando, S48Commando, Spectrum)
STD_ROM_FN(S48Commando)

struct BurnDriver BurnSpecS48Commando = {
	"spec_commando", NULL, "spec_spectrum", NULL, "1985",
	"Commando\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48CommandoRomInfo, S48CommandoRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48DizzyRomDesc[] = {
	{ "dizzy.z80",     0x0b21b, 0xfe79e93a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy, S48Dizzy, Spectrum)
STD_ROM_FN(S48Dizzy)

struct BurnDriver BurnSpecS48Dizzy = {
	"spec_dizzy", NULL, "spec_spectrum", NULL, "1987",
	"Dizzy - The Ultimate Cartoon Adventure\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48DizzyRomInfo, S48DizzyRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Dizzy2RomDesc[] = {
	{ "dizzy2.z80",     0x0b571, 0x63c7c2a9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy2, S48Dizzy2, Spectrum)
STD_ROM_FN(S48Dizzy2)

struct BurnDriver BurnSpecS48Dizzy2 = {
	"spec_dizzy2", NULL, "spec_spectrum", NULL, "1988",
	"Dizzy II - Treasure Island Dizzy\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Dizzy2RomInfo, S48Dizzy2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Dizzy3RomDesc[] = {
	{ "dizzy3.z80",     0x0b391, 0xb4c2f20b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy3, S48Dizzy3, Spectrum)
STD_ROM_FN(S48Dizzy3)

struct BurnDriver BurnSpecS48Dizzy3 = {
	"spec_dizzy3", NULL, "spec_spectrum", NULL, "1989",
	"Dizzy III - Fantasy World Dizzy\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Dizzy3RomInfo, S48Dizzy3RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Dizzy4RomDesc[] = {
	{ "dizzy4.z80",     0x0b54a, 0x19009d03, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy4, S48Dizzy4, Spectrum)
STD_ROM_FN(S48Dizzy4)

struct BurnDriver BurnSpecS48Dizzy4 = {
	"spec_dizzy4", NULL, "spec_spectrum", NULL, "1989",
	"Dizzy IV - Magicland Dizzy\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Dizzy4RomInfo, S48Dizzy4RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Dizzy5RomDesc[] = {
	{ "dizzy5.z80",     0x0b9c1, 0xdf6849fe, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy5, S48Dizzy5, Spectrum)
STD_ROM_FN(S48Dizzy5)

struct BurnDriver BurnSpecS48Dizzy5 = {
	"spec_dizzy5", NULL, "spec_spectrum", NULL, "1991",
	"Dizzy V - Spellbound Dizzy\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Dizzy5RomInfo, S48Dizzy5RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Dizzy6RomDesc[] = {
	{ "dizzy6.z80",     0x0a17a, 0xd3791a7a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy6, S48Dizzy6, Spectrum)
STD_ROM_FN(S48Dizzy6)

struct BurnDriver BurnSpecS48Dizzy6 = {
	"spec_dizzy6", NULL, "spec_spectrum", NULL, "1991",
	"Dizzy VI - Prince Of The Yolkfolk\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Dizzy6RomInfo, S48Dizzy6RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Dizzy7RomDesc[] = {
	{ "dizzy7.z80",     0x0a58f, 0x9ca9af5e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Dizzy7, S48Dizzy7, Spectrum)
STD_ROM_FN(S48Dizzy7)

struct BurnDriver BurnSpecS48Dizzy7 = {
	"spec_dizzy7", NULL, "spec_spectrum", NULL, "1992",
	"Dizzy VII - Crystal Kingdom Dizzy\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Dizzy7RomInfo, S48Dizzy7RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48FeudRomDesc[] = {
	{ "feud.z80",     0x096cf, 0xe9d169a7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Feud, S48Feud, Spectrum)
STD_ROM_FN(S48Feud)

struct BurnDriver BurnSpecS48Feud = {
	"spec_feud", NULL, "spec_spectrum", NULL, "1987",
	"Feud\0", NULL, "Mastertronic", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48FeudRomInfo, S48FeudRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48FootDirRomDesc[] = {
	{ "footdir.z80",     0x09abb, 0x6fdbf2bd, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48FootDir, S48FootDir, Spectrum)
STD_ROM_FN(S48FootDir)

struct BurnDriver BurnSpecS48FootDir = {
	"spec_footdir", NULL, "spec_spectrum", NULL, "1989",
	"Football Director\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48FootDirRomInfo, S48FootDirRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48FootmanRomDesc[] = {
	{ "footman.z80",     0x0824d, 0x54fd204c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Footman, S48Footman, Spectrum)
STD_ROM_FN(S48Footman)

struct BurnDriver BurnSpecS48Footman = {
	"spec_footman", NULL, "spec_spectrum", NULL, "1982",
	"Football Manager\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48FootmanRomInfo, S48FootmanRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Footman2RomDesc[] = {
	{ "footman2.z80",     0x0aad0, 0xb305dce4, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Footman2, S48Footman2, Spectrum)
STD_ROM_FN(S48Footman2)

struct BurnDriver BurnSpecS48Footman2 = {
	"spec_footman2", NULL, "spec_spectrum", NULL, "1988",
	"Football Manager 2\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Footman2RomInfo, S48Footman2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Footman3RomDesc[] = {
	{ "footman3.z80",     0x069f2, 0x33db96d5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Footman3, S48Footman3, Spectrum)
STD_ROM_FN(S48Footman3)

struct BurnDriver BurnSpecS48Footman3 = {
	"spec_footman3", NULL, "spec_spectrum", NULL, "1991",
	"Football Manager 3\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Footman3RomInfo, S48Footman3RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48FotyRomDesc[] = {
	{ "foty.z80",     0x08137, 0x2af522d0, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Foty, S48Foty, Spectrum)
STD_ROM_FN(S48Foty)

struct BurnDriver BurnSpecS48Foty = {
	"spec_foty", NULL, "spec_spectrum", NULL, "1986",
	"Footballer Of The Year\0", NULL, "Gremlin Graphics", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48FotyRomInfo, S48FotyRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Foty2RomDesc[] = {
	{ "foty2.z80",     0x08d52, 0x94a14ed8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Foty2, S48Foty2, Spectrum)
STD_ROM_FN(S48Foty2)

struct BurnDriver BurnSpecS48Foty2 = {
	"spec_foty2", NULL, "spec_spectrum", NULL, "1986",
	"Footballer Of The Year 2\0", NULL, "Gremlin Graphics", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Foty2RomInfo, S48Foty2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48GlhotshtRomDesc[] = {
	{ "glhotsht.z80",     0x08a0a, 0x18e1d943, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Glhotsht, S48Glhotsht, Spectrum)
STD_ROM_FN(S48Glhotsht)

struct BurnDriver BurnSpecS48Glhotsht = {
	"spec_glhotsht", NULL, "spec_spectrum", NULL, "1988",
	"Gary Lineker's Hot Shot\0", NULL, "Gremlin Graphics", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48GlhotshtRomInfo, S48GlhotshtRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48GlssoccRomDesc[] = {
	{ "glssocc.z80",     0x09c44, 0x700248d9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Glssocc, S48Glssocc, Spectrum)
STD_ROM_FN(S48Glssocc)

struct BurnDriver BurnSpecS48Glssocc = {
	"spec_glssocc", NULL, "spec_spectrum", NULL, "1987",
	"Gary Lineker's Superstar Soccer\0", NULL, "Gremlin Graphics", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48GlssoccRomInfo, S48GlssoccRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48HowbstdRomDesc[] = {
	{ "howbstd.z80",     0x0a37f, 0x559834ba, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Howbstd, S48Howbstd, Spectrum)
STD_ROM_FN(S48Howbstd)

struct BurnDriver BurnSpecS48Howbstd = {
	"spec_howbstd", NULL, "spec_spectrum", NULL, "1987",
	"How To Be A Complete Bastard\0", NULL, "Virgin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48HowbstdRomInfo, S48HowbstdRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48HunchbkRomDesc[] = {
	{ "hunchbk.z80",     0x07c76, 0xa7dde347, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Hunchbk, S48Hunchbk, Spectrum)
STD_ROM_FN(S48Hunchbk)

struct BurnDriver BurnSpecS48Hunchbk = {
	"spec_hunchbk", NULL, "spec_spectrum", NULL, "1983",
	"Hunchback\0", NULL, "Ocean Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48HunchbkRomInfo, S48HunchbkRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48JetpacRomDesc[] = {
	{ "jetpac.z80",     0x02aad, 0x4f96d444, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Jetpac, S48Jetpac, Spectrum)
STD_ROM_FN(S48Jetpac)

struct BurnDriver BurnSpecS48Jetpac = {
	"spec_jetpac", NULL, "spec_spectrum", NULL, "1983",
	"Jetpac\0", NULL, "Ultimate Play the Game", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48JetpacRomInfo, S48JetpacRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48MatchdayRomDesc[] = {
	{ "matchday.z80",     0x0a809, 0x59d3bc21, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Matchday, S48Matchday, Spectrum)
STD_ROM_FN(S48Matchday)

struct BurnDriver BurnSpecS48Matchday = {
	"spec_matchday", NULL, "spec_spectrum", NULL, "1987",
	"Match Day\0", NULL, "Ocean Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48MatchdayRomInfo, S48MatchdayRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48Matchdy2RomDesc[] = {
	{ "matchdy2.z80", 0x09ea5, 0x0e8fc511, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Matchdy2, S48Matchdy2, Spectrum)
STD_ROM_FN(S48Matchdy2)

struct BurnDriver BurnSpecS48Matchday2 = {
	"spec_matchdy2", NULL, "spec_spectrum", NULL, "1987",
	"Match Day 2\0", NULL, "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48Matchdy2RomInfo, S48Matchdy2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48MicrsoccRomDesc[] = {
	{ "micrsocc.z80",     0x07ed8, 0x5125611a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Micrsocc, S48Micrsocc, Spectrum)
STD_ROM_FN(S48Micrsocc)

struct BurnDriver BurnSpecS48Micrsocc = {
	"spec_micrsocc", NULL, "spec_spectrum", NULL, "1989",
	"Microprose Soccer\0", NULL, "Microprose", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48MicrsoccRomInfo, S48MicrsoccRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48HandbmarRomDesc[] = {
	{ "handbmar.z80",     0x08e08, 0x37a3f14c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Handbmar, S48Handbmar, Spectrum)
STD_ROM_FN(S48Handbmar)

struct BurnDriver BurnSpecS48Handbmar = {
	"spec_handbmar", NULL, "spec_spectrum", NULL, "1986",
	"Peter Shilton's Handball Maradona\0", NULL, "Grandslam Entertainment", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48HandbmarRomInfo, S48HandbmarRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48PippoRomDesc[] = {
	{ "pippo.sna",     0x0c01b, 0xcdd49a9f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Pippo, S48Pippo, Spectrum)
STD_ROM_FN(S48Pippo)

struct BurnDriver BurnSpecS48Pippo = {
	"spec_pippo", NULL, "spec_spectrum", NULL, "1986",
	"Pippo\0", NULL, "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48PippoRomInfo, S48PippoRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	SNASnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48RenegadeRomDesc[] = {
	{ "renegade.z80",     0x0a2d7, 0x9faf0d9e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Renegade, S48Renegade, Spectrum)
STD_ROM_FN(S48Renegade)

struct BurnDriver BurnSpecS48Renegade = {
	"spec_renegade", NULL, "spec_spectrum", NULL, "1987",
	"Renegade\0", NULL, "Ocean Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48RenegadeRomInfo, S48RenegadeRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S128RobocopRomDesc[] = {
	{ "robocop.z80",     0x1bc85, 0xe5ae03c8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S128Robocop, S128Robocop, Spec128)
STD_ROM_FN(S128Robocop)

struct BurnDriver BurnSpecS128Robocop = {
	"spec_robocop", NULL, "spec_spec128", NULL, "1988",
	"Robocop (128K)\0", NULL, "Ocean Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S128RobocopRomInfo, S128RobocopRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

static struct BurnRomInfo S48ThebossRomDesc[] = {
	{ "theboss.z80",     0x093f2, 0xafbdbe2a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(S48Theboss, S48Theboss, Spectrum)
STD_ROM_FN(S48Theboss)

struct BurnDriver BurnSpecS48Theboss = {
	"spec_theboss", NULL, "spec_spectrum", NULL, "1984",
	"The Boss\0", NULL, "Peaksoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, S48ThebossRomInfo, S48ThebossRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};
