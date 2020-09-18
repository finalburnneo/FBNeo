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
static UINT8 SpecDIP[2]         = {0, 0};
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

static INT32 ay_table[312]; // special stuff for syncing ay across the frame
static INT32 ay_table_initted = 0;

static INT16 *dacbuf; // for dc offset removal & sinc filter
static INT16 dac_lastin;
static INT16 dac_lastout;

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
	{"Dip 1"				, BIT_DIPSWITCH, SpecDIP + 0       , "dip"               },
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
	{72, 0xff, 0xff, 0x80, NULL                     }, // Blinky's Scary School requires issue 3
	
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

	if (pszGameName == NULL || i > 2) {
		*pszName = NULL;
		return 1;
	}
	
	// remove leader
	memset(szFilename, 0, MAX_PATH);
	for (UINT32 j = 0; j < (strlen(pszGameName) - 5); j++) {
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

	RamStart                = Next;
	SpecZ80Ram              = Next; Next += 0x20000;
	RamEnd                  = Next;
	
	SpecPalette             = (UINT32*)Next; Next += 0x00010 * sizeof(UINT32);
	dacbuf                  = (INT16*)Next; Next += 0x800 * 2 * sizeof(INT16);
	
	MemEnd                  = Next;

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
	
	dac_lastin = 0;
	dac_lastout = 0;
	
	return 0;
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
	if (SpecDIP[0] & 0x80) data ^= (0x40);
	
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
			
			if ((Changed & (1 << 4)) != 0) {
				DACWrite(0, BIT(d, 4) * 0x80);
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
	if (SpecDIP[0] & 0x80) data ^= (0x40);
	
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
		
		if ((Changed & (1 << 4)) != 0) {
			DACWrite(0, BIT(d, 4) * 0x80);
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
		
		case 0xfefd:
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
	
	BurnSetRefreshRate(50.0);

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
	
	DACInit(0, 0, 0, ZetTotalCycles, 3500000);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

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
	
	BurnSetRefreshRate(50.0);

	DACInit(0, 0, 0, ZetTotalCycles, 3500000);
	DACSetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	
	AY8910Init(0, 17734475 / 10, 0);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	
	SpecFrameInvertCount = 16;
	SpecFrameNumber = 0;
	SpecFlashInvert = 0;
	SpecNumScanlines = 311;
	SpecNumCylesPerScanline = 224;
	SpecVBlankScanline = 296;
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

/*static INT32 SNASnapshotInit()
{
	return SpectrumInit(SPEC_SNAPSHOT_SNA);
}*/

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

	for (INT32 i = 0; i < SpecNumScanlines; i++) {
		ay_table[i] = ((i * nBurnSoundLen) / SpecNumScanlines) - p;
		p = (i * nBurnSoundLen) / SpecNumScanlines;
	}

	ay_table_initted = 1;
}

static void sinc_flt_plus_dcblock(INT16 *inbuf, INT16 *outbuf, INT32 sample_nums)
{
	static INT16 delayLine[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	const double filterCoef[8] = { 0.841471, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
	double result, ampsum;
	INT16 out;

	for (int sample = 0; sample < sample_nums; sample++)
	{
		// sinc filter
		for (INT32 i = 6; i >= 0; i--) {
			delayLine[i + 1] = delayLine[i];
		}
		delayLine[0] = inbuf[sample * 2 + 0];

		result = ampsum = 0;
		for (INT32 i = 0; i < 8; i++) {
			result += delayLine[i] * filterCoef[i];
			ampsum += filterCoef[i];
		}
		result /= ampsum;

		// dc block
		out = result - dac_lastin + 0.995 * dac_lastout;
		dac_lastin = result;
		dac_lastout = out;

		// add to stream (+include ay if Spec128)
		outbuf[sample * 2 + 0] = (SpecIsSpec128) ? BURN_SND_CLIP(outbuf[sample * 2 + 0] + out) : BURN_SND_CLIP(out);
		outbuf[sample * 2 + 1] = (SpecIsSpec128) ? BURN_SND_CLIP(outbuf[sample * 2 + 1] + out) : BURN_SND_CLIP(out);
	}
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
		if (SpecIsSpec128) {
			INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			if (nSegmentLength) {
				AY8910Render(pSoundBuf, nSegmentLength);
			}
		}

		DACUpdate(dacbuf, nBurnSoundLen);
		sinc_flt_plus_dcblock(dacbuf, pBurnSoundOut, nBurnSoundLen);
	}
	
	ZetClose();

	return 0;
}

static INT32 SpecScan(INT32 nAction, INT32* pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
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
		DACScan(nAction, pnMin);

		if (SpecIsSpec128) {
			AY8910Scan(nAction, pnMin);
		}

		SCAN_VAR(nPortFEData);
		SCAN_VAR(nPort7FFDData);
	}

	if (nAction & ACB_WRITE) {      // Updating banking (128k)
		if (SpecIsSpec128) {
			ZetOpen(0);
			spectrum_128_update_memory();
			ZetClose();
		}
	}

	return 0;
}

struct BurnDriver BurnSpecSpectrumBIOS = {
	"spec_spectrum", NULL, NULL, NULL, "1984",
	"ZX Spectrum\0", "BIOS Only", "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_SPECTRUM, GBF_BIOS, 0,
	SpectrumGetZipName, SpectrumRomInfo, SpectrumRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	SpecInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

struct BurnDriver BurnSpecSpectrum = {
	"spec_spec48k", NULL, NULL, NULL, "1984",
	"ZX Spectrum\0", NULL, "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectrumRomInfo, SpectrumRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	SpecInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

struct BurnDriver BurnSpecSpec128BIOS = {
	"spec_spec128", NULL, NULL, NULL, "1984",
	"ZX Spectrum 128\0", "BIOS Only", "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_SPECTRUM, GBF_BIOS, 0,
	SpectrumGetZipName, Spec128RomInfo, Spec128RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	SpecSpec128Init, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

struct BurnDriver BurnSpecSpec128 = {
	"spec_spec128k", NULL, NULL, NULL, "1984",
	"ZX Spectrum 128\0", NULL, "Sinclair Research Limited", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spec128RomInfo, Spec128RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	SpecSpec128Init, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// games

// 180 (48K)

static struct BurnRomInfo Spec180_48RomDesc[] = {
	{ "180 (1986)(Mastertronic Added Dimension).z80", 0x08d96, 0x8cba8fcd, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spec180_48, Spec180_48, Spectrum)
STD_ROM_FN(Spec180_48)

struct BurnDriver BurnSpec180_48 = {
	"spec_180_48", "spec_180", "spec_spectrum", NULL, "1986",
	"180 (48K)\0", NULL, "Mastertronic Added Dimension", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spec180_48RomInfo, Spec180_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// 180 (128K)

static struct BurnRomInfo Spec180RomDesc[] = {
	{ "180 (1986)(Mastertronic Added Dimension)[128K].z80", 0x0d536, 0xc4937cba, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spec180, Spec180, Spec128)
STD_ROM_FN(Spec180)

struct BurnDriver BurnSpec180 = {
	"spec_180", NULL, "spec_spec128", NULL, "1986",
	"180 (128K)\0", NULL, "Mastertronic Added Dimension", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spec180RomInfo, Spec180RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// 1942 (48K)

static struct BurnRomInfo Spec1942RomDesc[] = {
	{ "1942 (1986)(Elite Systems).z80", 0x08a78, 0x82b77807, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spec1942, Spec1942, Spectrum)
STD_ROM_FN(Spec1942)

struct BurnDriver BurnSpec1942 = {
	"spec_1942", NULL, "spec_spectrum", NULL, "1986",
	"1942 (48K)\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spec1942RomInfo, Spec1942RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// 1943 - The Battle of Midway (48K)

static struct BurnRomInfo Spec1943RomDesc[] = {
	{ "1943 - The Battle of Midway (1988)(Go!).z80", 0x0996f, 0xc97f9144, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spec1943, Spec1943, Spectrum)
STD_ROM_FN(Spec1943)

struct BurnDriver BurnSpec1943 = {
	"spec_1943", NULL, "spec_spectrum", NULL, "1988",
	"1943 - The Battle of Midway (48K)\0", NULL, "Go!", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spec1943RomInfo, Spec1943RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// 720 Degrees (48K)

static struct BurnRomInfo Spec720degRomDesc[] = {
	{ "720 Degrees (1986)(U.S. Gold).z80", 0x096c6, 0x25a4c45f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spec720deg, Spec720deg, Spectrum)
STD_ROM_FN(Spec720deg)

struct BurnDriver BurnSpec720deg = {
	"spec_720deg", NULL, "spec_spectrum", NULL, "1986",
	"720 Degrees (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spec720degRomInfo, Spec720degRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Action Fighter (48K)

static struct BurnRomInfo Specafighter_48RomDesc[] = {
	{ "Action Fighter (1989)(Firebird Software).z80", 0x09384, 0xd61df17c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specafighter_48, Specafighter_48, Spectrum)
STD_ROM_FN(Specafighter_48)

struct BurnDriver BurnSpecafighter_48 = {
	"spec_afighter_48", "spec_afighter", "spec_spectrum", NULL, "1989",
	"Action Fighter (48K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specafighter_48RomInfo, Specafighter_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Action Fighter (128K)

static struct BurnRomInfo SpecafighterRomDesc[] = {
	{ "Action Fighter (1989)(Firebird Software)[128K].z80", 0x0a81a, 0x55f30b2a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specafighter, Specafighter, Spec128)
STD_ROM_FN(Specafighter)

struct BurnDriver BurnSpecafighter = {
	"spec_afighter", NULL, "spec_spec128", NULL, "1989",
	"Action Fighter (128K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecafighterRomInfo, SpecafighterRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Adidas Championship Football (128K)

static struct BurnRomInfo SpecadichafoRomDesc[] = {
	{ "Adidas Championship Football (1990)(Ocean)[128K].z80", 0x17369, 0x89364845, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specadichafo, Specadichafo, Spec128)
STD_ROM_FN(Specadichafo)

struct BurnDriver BurnSpecadichafo = {
	"spec_adichafo", NULL, "spec_spec128", NULL, "1990",
	"Adidas Championship Football (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecadichafoRomInfo, SpecadichafoRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Advanced Lawnmower Simulator II (128K)

static struct BurnRomInfo Specadvlawn2RomDesc[] = {
	{ "Advanced Lawnmower Simulator II (1990)(JA Software)[128K].z80", 0x021e0, 0x9f4e38e2, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specadvlawn2, Specadvlawn2, Spec128)
STD_ROM_FN(Specadvlawn2)

struct BurnDriver BurnSpecadvlawn2 = {
	"spec_advlawn2", NULL, "spec_spec128", NULL, "1990",
	"Advanced Lawnmower Simulator II (128K)\0", NULL, "JA Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specadvlawn2RomInfo, Specadvlawn2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Adventures of Buratino, The (48K).z80

static struct BurnRomInfo SpecburatinoRomDesc[] = {
	{ "Adventures of Buratino, The (1993)(Copper Feet)(48k).z80", 0x096ec, 0x3a0cb189, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specburatino, Specburatino, Spectrum)
STD_ROM_FN(Specburatino)

struct BurnDriver BurnSpecburatino = {
	"spec_buratino", NULL, "spec_spectrum", NULL, "1993",
	"Adventures of Buratino, The (48K)\0", NULL, "Copper Feet", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecburatinoRomInfo, SpecburatinoRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Afterburner (48K)

static struct BurnRomInfo SpecaburnerRomDesc[] = {
	{ "Afterburner (1988)(Activision).z80", 0x0ab31, 0x1d647b67, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specaburner, Specaburner, Spectrum)
STD_ROM_FN(Specaburner)

struct BurnDriver BurnSpecaburner = {
	"spec_aburner", NULL, "spec_spectrum", NULL, "1988",
	"Afterburner (48K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecaburnerRomInfo, SpecaburnerRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Airwolf (48K)

static struct BurnRomInfo SpecairwolfRomDesc[] = {
	{ "Airwolf (1984)(Elite Systems).z80", 0x0af11, 0xf322ce6a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specairwolf, Specairwolf, Spectrum)
STD_ROM_FN(Specairwolf)

struct BurnDriver BurnSpecairwolf = {
	"spec_airwolf", NULL, "spec_spectrum", NULL, "1984",
	"Airwolf (48K)\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecairwolfRomInfo, SpecairwolfRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Airwolf II (48K)

static struct BurnRomInfo Specairwolf2RomDesc[] = {
	{ "Airwolf II (1987)(Encore).z80", 0x09ae0, 0x8f6671ec, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specairwolf2, Specairwolf2, Spectrum)
STD_ROM_FN(Specairwolf2)

struct BurnDriver BurnSpecairwolf2 = {
	"spec_airwolf2", NULL, "spec_spectrum", NULL, "1987",
	"Airwolf II (48K)\0", NULL, "Encore", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specairwolf2RomInfo, Specairwolf2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Alien Syndrome (128K)

static struct BurnRomInfo SpecaliensynRomDesc[] = {
	{ "Alien Syndrome (1987)(ACE Software)[t +2][128K].z80", 0x0f7bc, 0x11c4832e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specaliensyn, Specaliensyn, Spec128)
STD_ROM_FN(Specaliensyn)

struct BurnDriver BurnSpecaliensyn = {
	"spec_aliensyn", NULL, "spec_spec128", NULL, "1987",
	"Alien Syndrome (128K)\0", NULL, "ACE Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecaliensynRomInfo, SpecaliensynRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Altered Beast (128K)

static struct BurnRomInfo SpecaltbeastRomDesc[] = {
	{ "Altered Beast (1988)(Activision)[128K].z80", 0x0b8b5, 0x8c27eb15, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specaltbeast, Specaltbeast, Spec128)
STD_ROM_FN(Specaltbeast)

struct BurnDriver BurnSpecaltbeast = {
	"spec_altbeast", NULL, "spec_spec128", NULL, "1988",
	"Altered Beast (128K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecaltbeastRomInfo, SpecaltbeastRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Andy Capp (128K)

static struct BurnRomInfo SpecandycappRomDesc[] = {
	{ "Andy Capp (1988)(Mirrorsoft)[128K].z80", 0x0ad39, 0x77f3f490, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specandycapp, Specandycapp, Spec128)
STD_ROM_FN(Specandycapp)

struct BurnDriver BurnSpecandycapp = {
	"spec_andycapp", NULL, "spec_spec128", NULL, "1988",
	"Andy Capp (128K)\0", NULL, "Mirrorsoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecandycappRomInfo, SpecandycappRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// APB - All Points Bulletin (128K)

static struct BurnRomInfo SpecapbRomDesc[] = {
	{ "APB - All Points Bulletin (1989)(Domark)[128K].z80", 0x12d7d, 0x6fb0235f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specapb, Specapb, Spec128)
STD_ROM_FN(Specapb)

struct BurnDriver BurnSpecapb = {
	"spec_apb", NULL, "spec_spec128", NULL, "1989",
	"APB - All Points Bulletin (128K)\0", NULL, "Domark", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecapbRomInfo, SpecapbRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Arkanoid (48K)

static struct BurnRomInfo SpecarkanoidRomDesc[] = {
	{ "Arkanoid (1987)(Imagine Software).z80", 0x08ad3, 0x6fa4f00f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specarkanoid, Specarkanoid, Spectrum)
STD_ROM_FN(Specarkanoid)

struct BurnDriver BurnSpecarkanoid = {
	"spec_arkanoid", NULL, "spec_spectrum", NULL, "1987",
	"Arkanoid (48K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecarkanoidRomInfo, SpecarkanoidRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Arkanoid II - Revenge of Doh (48K)

static struct BurnRomInfo Specarkanoid2_48RomDesc[] = {
	{ "Arkanoid II - Revenge of Doh (1988)(Imagine Software).z80", 0x0c01e, 0xaa06fc9e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specarkanoid2_48, Specarkanoid2_48, Spectrum)
STD_ROM_FN(Specarkanoid2_48)

struct BurnDriver BurnSpecarkanoid2_48 = {
	"spec_arkanoid2_48", "spec_arkanoid2", "spec_spectrum", NULL, "1988",
	"Arkanoid II - Revenge of Doh (48K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specarkanoid2_48RomInfo, Specarkanoid2_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Arkanoid II - Revenge of Doh (128K)

static struct BurnRomInfo Specarkanoid2RomDesc[] = {
	{ "Arkanoid II - Revenge of Doh (1988)(Imagine Software)[128K].z80", 0x0cd16, 0x0fa0ffb5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specarkanoid2, Specarkanoid2, Spec128)
STD_ROM_FN(Specarkanoid2)

struct BurnDriver BurnSpecarkanoid2 = {
	"spec_arkanoid2", NULL, "spec_spec128", NULL, "1988",
	"Arkanoid II - Revenge of Doh (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specarkanoid2RomInfo, Specarkanoid2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Atic Atac (48K)

static struct BurnRomInfo SpecaticatacRomDesc[] = {
	{ "Atic Atac (1983)(Ultimate Play The Game).z80", 0x08994, 0x56520bdf, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specaticatac, Specaticatac, Spectrum)
STD_ROM_FN(Specaticatac)

struct BurnDriver BurnSpecaticatac = {
	"spec_aticatac", NULL, "spec_spectrum", NULL, "1983",
	"Atic Atac (48K)\0", NULL, "Ultimate Play The Game", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecaticatacRomInfo, SpecaticatacRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// ATV Simulator - All Terrain Vehicle (48K)

static struct BurnRomInfo Specatvsim_48RomDesc[] = {
	{ "ATV Simulator - All Terrain Vehicle (1987)(Codemasters).z80", 0x0a24d, 0x80171771, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specatvsim_48, Specatvsim_48, Spectrum)
STD_ROM_FN(Specatvsim_48)

struct BurnDriver BurnSpecatvsim_48 = {
	"spec_atvsim_48", "spec_atvsim", "spec_spectrum", NULL, "1987",
	"ATV Simulator - All Terrain Vehicle (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specatvsim_48RomInfo, Specatvsim_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// ATV Simulator - All Terrain Vehicle (128K)

static struct BurnRomInfo SpecatvsimRomDesc[] = {
	{ "ATV Simulator - All Terrain Vehicle (1987)(Codemasters)[128K].z80", 0x0a8ef, 0xe1fc4bb9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specatvsim, Specatvsim, Spec128)
STD_ROM_FN(Specatvsim)

struct BurnDriver BurnSpecatvsim = {
	"spec_atvsim", NULL, "spec_spec128", NULL, "1987",
	"ATV Simulator - All Terrain Vehicle (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecatvsimRomInfo, SpecatvsimRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Auf Wiedersehen Monty (48K)

static struct BurnRomInfo Specaufwiemo_48RomDesc[] = {
	{ "Auf Wiedersehen Monty (1987)(Gremlin Graphics)[a4].z80", 0x0c01e, 0x1851b7fa, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specaufwiemo_48, Specaufwiemo_48, Spectrum)
STD_ROM_FN(Specaufwiemo_48)

struct BurnDriver BurnSpecaufwiemo_48 = {
	"spec_aufwiemo_48", "spec_aufwiemo", "spec_spectrum", NULL, "1987",
	"Auf Wiedersehen Monty (48K)\0", NULL, "Gremlin Graphics Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specaufwiemo_48RomInfo, Specaufwiemo_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Auf Wiedersehen Monty (128K)

static struct BurnRomInfo SpecaufwiemoRomDesc[] = {
	{ "Auf Wiedersehen Monty (1987)(Gremlin Graphics)(128k).z80", 0x10eff, 0x49580b2d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specaufwiemo, Specaufwiemo, Spectrum)
STD_ROM_FN(Specaufwiemo)

struct BurnDriver BurnSpecaufwiemo = {
	"spec_aufwiemo", NULL, "spec_spec128", NULL, "1987",
	"Auf Wiedersehen Monty (128K)\0", NULL, "Gremlin Graphics Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecaufwiemoRomInfo, SpecaufwiemoRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Back to Skool (48K)

static struct BurnRomInfo SpecbackskooRomDesc[] = {
	{ "Back to Skool (1985)(Microsphere).z80", 0x0afb3, 0x6bf68f3d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbackskoo, Specbackskoo, Spectrum)
STD_ROM_FN(Specbackskoo)

struct BurnDriver BurnSpecbackskoo = {
	"spec_backskoo", NULL, "spec_spectrum", NULL, "1985",
	"Back to Skool (48K)\0", NULL, "Microsphere", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbackskooRomInfo, SpecbackskooRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Back to the Future (48K)

static struct BurnRomInfo SpecbackfutuRomDesc[] = {
	{ "Back to the Future (1985)(Electric Dreams Software).z80", 0x08d76, 0x9d8d8fa7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbackfutu, Specbackfutu, Spectrum)
STD_ROM_FN(Specbackfutu)

struct BurnDriver BurnSpecbackfutu = {
	"spec_backfutu", NULL, "spec_spectrum", NULL, "1985",
	"Back to the Future (48K)\0", NULL, "Electric Dreams Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbackfutuRomInfo, SpecbackfutuRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Back to the Future III (48K)

static struct BurnRomInfo Specbackfut3RomDesc[] = {
	{ "Back to the Future III (1991)(Image Works)[incomplete].z80", 0x09ad2, 0xad5a7442, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbackfut3, Specbackfut3, Spectrum)
STD_ROM_FN(Specbackfut3)

struct BurnDriver BurnSpecbackfut3 = {
	"spec_backfut3", NULL, "spec_spectrum", NULL, "1991",
	"Back to the Future III (48K)\0", NULL, "Image Works", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbackfut3RomInfo, Specbackfut3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Badlands (Domark) (48K)

static struct BurnRomInfo SpecbadlandsRomDesc[] = {
	{ "Badlands (1990)(Domark).z80", 0x08b3e, 0x93b1febc, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbadlands, Specbadlands, Spectrum)
STD_ROM_FN(Specbadlands)

struct BurnDriver BurnSpecbadlands = {
	"spec_badlands", NULL, "spec_spectrum", NULL, "1990",
	"Badlands (Domark) (48K)\0", NULL, "Domark", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbadlandsRomInfo, SpecbadlandsRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Badlands (Erbe Software) (48K)

static struct BurnRomInfo SpecbadlandseRomDesc[] = {
	{ "Badlands (1990)(Erbe Software)[re-release].z80", 0x0c01e, 0x13128782, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbadlandse, Specbadlandse, Spectrum)
STD_ROM_FN(Specbadlandse)

struct BurnDriver BurnSpecbadlandse = {
	"spec_badlandse", "spec_badlands", "spec_spectrum", NULL, "1990",
	"Badlands (Erbe Software) (48K)\0", NULL, "Erbe Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbadlandseRomInfo, SpecbadlandseRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Barbarian - 1 Player (48K)

static struct BurnRomInfo Specbarbply1RomDesc[] = {
	{ "Barbarian - 1 Player (1987)(Palace Software).z80", 0x0a596, 0x5f06bc26, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbarbply1, Specbarbply1, Spectrum)
STD_ROM_FN(Specbarbply1)

struct BurnDriver BurnSpecbarbply1 = {
	"spec_barbply1", "spec_barbarn", "spec_spectrum", NULL, "1987",
	"Barbarian - 1 Player (48K)\0", NULL, "Palace Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbarbply1RomInfo, Specbarbply1RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Barbarian - 2 Players (48K)

static struct BurnRomInfo Specbarbply2RomDesc[] = {
	{ "Barbarian - 2 Players (1987)(Palace Software).z80", 0x0a2b2, 0xa856967f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbarbply2, Specbarbply2, Spectrum)
STD_ROM_FN(Specbarbply2)

struct BurnDriver BurnSpecbarbply2 = {
	"spec_barbply2", "spec_barbarn", "spec_spectrum", NULL, "1987",
	"Barbarian - 2 Players (48K)\0", NULL, "Palace Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbarbply2RomInfo, Specbarbply2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Barbarian (48K)

static struct BurnRomInfo Specbarbarn_48RomDesc[] = {
	{ "Barbarian (1988)(Melbourne House).z80", 0x090d1, 0xf386a7ff, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbarbarn_48, Specbarbarn_48, Spectrum)
STD_ROM_FN(Specbarbarn_48)

struct BurnDriver BurnSpecbarbarn_48 = {
	"spec_barbarn_48", "spec_barbarn", "spec_spectrum", NULL, "1988",
	"Barbarian (48K)\0", NULL, "Melbourne House", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbarbarn_48RomInfo, Specbarbarn_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Barbarian (128K)

static struct BurnRomInfo SpecbarbarnRomDesc[] = {
	{ "Barbarian (1988)(Melbourne House)[128K].z80", 0x0a624, 0x4d0607de, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbarbarn, Specbarbarn, Spec128)
STD_ROM_FN(Specbarbarn)

struct BurnDriver BurnSpecbarbarn = {
	"spec_barbarn", NULL, "spec_spec128", NULL, "1988",
	"Barbarian (128K)\0", NULL, "Melbourne House", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbarbarnRomInfo, SpecbarbarnRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Barbarian II - The Dungeon of Drax (128K)

static struct BurnRomInfo Specbarbarn2RomDesc[] = {
	{ "Barbarian II - The Dungeon of Drax (1988)(Palace Software)[128K].z80", 0x1ac6c, 0x2215c3b7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbarbarn2, Specbarbarn2, Spec128)
STD_ROM_FN(Specbarbarn2)

struct BurnDriver BurnSpecbarbarn2 = {
	"spec_barbarn2", NULL, "spec_spec128", NULL, "1988",
	"Barbarian II - The Dungeon of Drax (128K)\0", NULL, "Palace Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbarbarn2RomInfo, Specbarbarn2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Batman - The Caped Crusader - Part 1 - A Bird in the Hand (48K)

static struct BurnRomInfo SpecbatmanccRomDesc[] = {
	{ "Batman - The Caped Crusader - Part 1 - A Bird in the Hand (1988)(Ocean).z80", 0x09dc8, 0x2d3ebd28, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbatmancc, Specbatmancc, Spectrum)
STD_ROM_FN(Specbatmancc)

struct BurnDriver BurnSpecbatmancc = {
	"spec_batmancc", NULL, "spec_spectrum", NULL, "1988",
	"Batman - The Caped Crusader - Part 1 - A Bird in the Hand (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbatmanccRomInfo, SpecbatmanccRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Batman - The Caped Crusader - Part 2 - A Fete Worse than Death (48K)

static struct BurnRomInfo Specbatmancc2RomDesc[] = {
	{ "Batman - The Caped Crusader - Part 2 - A Fete Worse than Death (1988)(Ocean).z80", 0x0a1fb, 0x9a27094c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbatmancc2, Specbatmancc2, Spectrum)
STD_ROM_FN(Specbatmancc2)

struct BurnDriver BurnSpecbatmancc2 = {
	"spec_batmancc2", "spec_batmancc", "spec_spectrum", NULL, "1988",
	"Batman - The Caped Crusader - Part 2 - A Fete Worse than Death (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbatmancc2RomInfo, Specbatmancc2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Batman - The Movie (128K)

static struct BurnRomInfo SpecbatmanmvRomDesc[] = {
	{ "Batman - The Movie (1989)(Ocean)[128K].z80", 0x1b872, 0x17ed3d84, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbatmanmv, Specbatmanmv, Spec128)
STD_ROM_FN(Specbatmanmv)

struct BurnDriver BurnSpecbatmanmv = {
	"spec_batmanmv", NULL, "spec_spec128", NULL, "1989",
	"Batman - The Movie (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbatmanmvRomInfo, SpecbatmanmvRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Batman - The Puaj Edition (48K)

static struct BurnRomInfo SpecbatmanpeRomDesc[] = {
	{ "Batman - The Puaj Edition (1989)(Micro House).z80", 0x086f6, 0x1ec85d88, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbatmanpe, Specbatmanpe, Spectrum)
STD_ROM_FN(Specbatmanpe)

struct BurnDriver BurnSpecbatmanpe = {
	"spec_batmanpe", NULL, "spec_spectrum", NULL, "1989",
	"Batman - The Puaj Edition (48K)\0", NULL, "Micro House", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbatmanpeRomInfo, SpecbatmanpeRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Batman (48K)

static struct BurnRomInfo Specbatman_48RomDesc[] = {
	{ "Batman (1986)(Ocean).z80", 0x0a6bb, 0x0f909c21, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbatman_48, Specbatman_48, Spectrum)
STD_ROM_FN(Specbatman_48)

struct BurnDriver BurnSpecbatman_48 = {
	"spec_batman_48", "spec_batman", "spec_spectrum", NULL, "1986",
	"Batman (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbatman_48RomInfo, Specbatman_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Batman (128K)

static struct BurnRomInfo SpecbatmanRomDesc[] = {
	{ "Batman (1986)(Ocean)[128K].z80", 0x0b90a, 0x48ec8253, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbatman, Specbatman, Spec128)
STD_ROM_FN(Specbatman)

struct BurnDriver BurnSpecbatman = {
	"spec_batman", NULL, "spec_spec128", NULL, "1986",
	"Batman (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbatmanRomInfo, SpecbatmanRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Beach Buggy Simulator (48K)

static struct BurnRomInfo SpecbeabugsiRomDesc[] = {
	{ "Beach Buggy Simulator (1988)(Silverbird Software).z80", 0x096de, 0x900b90e9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbeabugsi, Specbeabugsi, Spectrum)
STD_ROM_FN(Specbeabugsi)

struct BurnDriver BurnSpecbeabugsi = {
	"spec_beabugsi", NULL, "spec_spectrum", NULL, "1988",
	"Beach Buggy Simulator (48K)\0", NULL, "Silverbird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbeabugsiRomInfo, SpecbeabugsiRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Beyond the Ice Palace (48K)

static struct BurnRomInfo Specbeyicepa_48RomDesc[] = {
	{ "Beyond the Ice Palace (1988)(Elite Systems).z80", 0x09e04, 0xf039a5d9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbeyicepa_48, Specbeyicepa_48, Spectrum)
STD_ROM_FN(Specbeyicepa_48)

struct BurnDriver BurnSpecbeyicepa_48 = {
	"spec_beyicepa_48", "spec_beyicepa", "spec_spectrum", NULL, "1988",
	"Beyond the Ice Palace (48K)\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbeyicepa_48RomInfo, Specbeyicepa_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Beyond the Ice Palace (128K)

static struct BurnRomInfo SpecbeyicepaRomDesc[] = {
	{ "Beyond the Ice Palace (1988)(Elite Systems)(128k).z80", 0x0bc09, 0x3bb06772, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbeyicepa, Specbeyicepa, Spectrum)
STD_ROM_FN(Specbeyicepa)

struct BurnDriver BurnSpecbeyicepa = {
	"spec_beyicepa", NULL, "spec_spec128", NULL, "1988",
	"Beyond the Ice Palace (128K)\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbeyicepaRomInfo, SpecbeyicepaRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bionic Commando (48K)

static struct BurnRomInfo Specbionicc_48RomDesc[] = {
	{ "Bionic Commando (1988)(Go!).z80", 0x09ba6, 0x69a1c19d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbionicc_48, Specbionicc_48, Spectrum)
STD_ROM_FN(Specbionicc_48)

struct BurnDriver BurnSpecbionicc_48 = {
	"spec_bionicc_48", "spec_bionicc", "spec_spectrum", NULL, "1988",
	"Bionic Commando (48K)\0", NULL, "Go!", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbionicc_48RomInfo, Specbionicc_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bionic Commando (128K)

static struct BurnRomInfo SpecbioniccRomDesc[] = {
	{ "Bionic Commando (1988)(Go!)[128K].z80", 0x166f5, 0x8eb507eb, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbionicc, Specbionicc, Spec128)
STD_ROM_FN(Specbionicc)

struct BurnDriver BurnSpecbionicc = {
	"spec_bionicc", NULL, "spec_spec128", NULL, "1988",
	"Bionic Commando (128K)\0", NULL, "Go!", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbioniccRomInfo, SpecbioniccRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Black Tiger (48K)

static struct BurnRomInfo Specblktiger_48RomDesc[] = {
	{ "Black Tiger (1989)(U.S. Gold).z80", 0x086a4, 0x22b42c5d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specblktiger_48, Specblktiger_48, Spectrum)
STD_ROM_FN(Specblktiger_48)

struct BurnDriver BurnSpecblktiger_48 = {
	"spec_blktiger_48", "spec_blktiger", "spec_spectrum", NULL, "1989",
	"Black Tiger (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specblktiger_48RomInfo, Specblktiger_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Black Tiger (128K)

static struct BurnRomInfo SpecblktigerRomDesc[] = {
	{ "Black Tiger (1989)(U.S. Gold)[128K].z80", 0x0a1a1, 0xc9b3da9b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specblktiger, Specblktiger, Spec128)
STD_ROM_FN(Specblktiger)

struct BurnDriver BurnSpecblktiger = {
	"spec_blktiger", NULL, "spec_spec128", NULL, "1989",
	"Black Tiger (128K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecblktigerRomInfo, SpecblktigerRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Blinky's Scary School (48K)

static struct BurnRomInfo SpecblinkysRomDesc[] = {
	{ "Blinky's Scary School (1990)(Zeppelin Games).z80", 0x09cdf, 0x65b0cd8e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specblinkys, Specblinkys, Spectrum)
STD_ROM_FN(Specblinkys)

struct BurnDriver BurnSpecblinkys = {
	"spec_blinkys", NULL, "spec_spectrum", NULL, "1990",
	"Blinky's Scary School (48K)\0", NULL, "Zeppelin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecblinkysRomInfo, SpecblinkysRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// BMX Freestyle Simulator (48K)

static struct BurnRomInfo Specbmxfrees_48RomDesc[] = {
	{ "BMX Freestyle Simulator (1989)(Codemasters).z80", 0x09c65, 0x10749c38, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbmxfrees_48, Specbmxfrees_48, Spectrum)
STD_ROM_FN(Specbmxfrees_48)

struct BurnDriver BurnSpecbmxfrees_48 = {
	"spec_bmxfrees_48", "spec_bmxfrees", "spec_spectrum", NULL, "1989",
	"BMX Freestyle Simulator (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbmxfrees_48RomInfo, Specbmxfrees_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// BMX Freestyle Simulator (128K)

static struct BurnRomInfo SpecbmxfreesRomDesc[] = {
	{ "BMX Freestyle Simulator (1989)(Codemasters)[128K].z80", 0x0b736, 0x58d28848, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbmxfrees, Specbmxfrees, Spec128)
STD_ROM_FN(Specbmxfrees)

struct BurnDriver BurnSpecbmxfrees = {
	"spec_bmxfrees", NULL, "spec_spec128", NULL, "1989",
	"BMX Freestyle Simulator (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbmxfreesRomInfo, SpecbmxfreesRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bomb Jack (48K)

static struct BurnRomInfo Specbombjack_48RomDesc[] = {
	{ "Bomb Jack (1986)(Elite Systems).z80", 0x0a30a, 0x00e95211, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbombjack_48, Specbombjack_48, Spectrum)
STD_ROM_FN(Specbombjack_48)

struct BurnDriver BurnSpecbombjack_48 = {
	"spec_bombjack_48", "spec_bombjack", "spec_spectrum", NULL, "1986",
	"Bomb Jack (48K)\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbombjack_48RomInfo, Specbombjack_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bomb Jack (128K)

static struct BurnRomInfo SpecbombjackRomDesc[] = {
	{ "Bomb Jack (1986)(Elite Systems)[128K].z80", 0x0a345, 0x71a84d84, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbombjack, Specbombjack, Spec128)
STD_ROM_FN(Specbombjack)

struct BurnDriver BurnSpecbombjack = {
	"spec_bombjack", NULL, "spec_spec128", NULL, "1986",
	"Bomb Jack (128K)\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbombjackRomInfo, SpecbombjackRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bomb Jack II (48K)

static struct BurnRomInfo Specbmbjack2RomDesc[] = {
	{ "Bomb Jack II (1987)(Elite Systems).z80", 0x0b1b2, 0x6327f471, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbmbjack2, Specbmbjack2, Spectrum)
STD_ROM_FN(Specbmbjack2)

struct BurnDriver BurnSpecbmbjack2 = {
	"spec_bmbjack2", NULL, "spec_spectrum", NULL, "1987",
	"Bomb Jack II (48K)\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbmbjack2RomInfo, Specbmbjack2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Boulder Dash (48K)

static struct BurnRomInfo SpecbouldashRomDesc[] = {
	{ "Boulder Dash (1984)(Front Runner).z80", 0x04849, 0x5d71133f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbouldash, Specbouldash, Spectrum)
STD_ROM_FN(Specbouldash)

struct BurnDriver BurnSpecbouldash = {
	"spec_bouldash", NULL, "spec_spectrum", NULL, "1984",
	"Boulder Dash (48K)\0", NULL, "Front Runner", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbouldashRomInfo, SpecbouldashRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Boulder Dash II - Rockford's Riot (48K)

static struct BurnRomInfo Specbouldsh2RomDesc[] = {
	{ "Boulder Dash II - Rockford's Riot (1985)(Prism Leisure).z80", 0x057a4, 0xbcab1101, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbouldsh2, Specbouldsh2, Spectrum)
STD_ROM_FN(Specbouldsh2)

struct BurnDriver BurnSpecbouldsh2 = {
	"spec_bouldsh2", NULL, "spec_spectrum", NULL, "1985",
	"Boulder Dash II - Rockford's Riot (48K)\0", NULL, "Prism Leisure", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbouldsh2RomInfo, Specbouldsh2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Boulder Dash III (48K)

static struct BurnRomInfo Specbouldsh3RomDesc[] = {
	{ "Boulder Dash III (1986)(Prism Leisure).z80", 0x04982, 0xb61f2fae, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbouldsh3, Specbouldsh3, Spectrum)
STD_ROM_FN(Specbouldsh3)

struct BurnDriver BurnSpecbouldsh3 = {
	"spec_bouldsh3", NULL, "spec_spectrum", NULL, "1986",
	"Boulder Dash III (48K)\0", NULL, "Prism Leisure", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbouldsh3RomInfo, Specbouldsh3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Boulder Dash V (48K)

static struct BurnRomInfo Specbouldsh4RomDesc[] = {
	{ "Boulder Dash V (1992)(Too Trek Moscow S.N.G.).z80", 0x08721, 0x6e9b68bf, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbouldsh4, Specbouldsh4, Spectrum)
STD_ROM_FN(Specbouldsh4)

struct BurnDriver BurnSpecbouldsh4 = {
	"spec_bouldsh4", NULL, "spec_spectrum", NULL, "1992",
	"Boulder Dash V (48K)\0", NULL, "Too Trek Moscow S.N.G.", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbouldsh4RomInfo, Specbouldsh4RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Boulder Dash VI (48K)

static struct BurnRomInfo Specbouldsh5RomDesc[] = {
	{ "Boulder Dash VI (1992)(Too Trek Moscow S.N.G.).z80", 0x08792, 0x6abfc981, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbouldsh5, Specbouldsh5, Spectrum)
STD_ROM_FN(Specbouldsh5)

struct BurnDriver BurnSpecbouldsh5 = {
	"spec_bouldsh5", NULL, "spec_spectrum", NULL, "1992",
	"Boulder Dash VI (48K)\0", NULL, "Too Trek Moscow S.N.G.", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbouldsh5RomInfo, Specbouldsh5RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bruce Lee (48K)

static struct BurnRomInfo SpecbruceleeRomDesc[] = {
	{ "Bruce Lee (1984)(U.S. Gold).z80", 0x08ceb, 0x8298df22, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbrucelee, Specbrucelee, Spectrum)
STD_ROM_FN(Specbrucelee)

struct BurnDriver BurnSpecbrucelee = {
	"spec_brucelee", NULL, "spec_spectrum", NULL, "1984",
	"Bruce Lee (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbruceleeRomInfo, SpecbruceleeRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Boss, The (48K)

static struct BurnRomInfo SpecbosstheRomDesc[] = {
	{ "Boss, The (1984)(Peaksoft).z80", 0x09035, 0x9b7242b2, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbossthe, Specbossthe, Spectrum)
STD_ROM_FN(Specbossthe)

struct BurnDriver BurnSpecbossthe = {
	"spec_bossthe", NULL, "spec_spectrum", NULL, "1984",
	"Boss, The (48K)\0", NULL, "Peaksoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbosstheRomInfo, SpecbosstheRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bubble Bobble - The Adventure (128K)

static struct BurnRomInfo SpecbubothadRomDesc[] = {
	{ "Bubble Bobble - The Adventure (1993)(AP's Adventures)[128K].z80", 0x0d387, 0xcfa03eec, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbubothad, Specbubothad, Spec128)
STD_ROM_FN(Specbubothad)

struct BurnDriver BurnSpecbubothad = {
	"spec_bubothad", NULL, "spec_spec128", NULL, "1993",
	"Bubble Bobble - The Adventure (128K)\0", NULL, "AP's Adventures", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbubothadRomInfo, SpecbubothadRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bubble Bobble (48K)

static struct BurnRomInfo Specbublbobl_48RomDesc[] = {
	{ "Bubble Bobble (1987)(Firebird Software).z80", 0x09e39, 0x77c240b3, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbublbobl_48, Specbublbobl_48, Spectrum)
STD_ROM_FN(Specbublbobl_48)

struct BurnDriver BurnSpecbublbobl_48 = {
	"spec_bublbobl_48", "spec_bublbobl", "spec_spectrum", NULL, "1987",
	"Bubble Bobble (48K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbublbobl_48RomInfo, Specbublbobl_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bubble Bobble (128K)

static struct BurnRomInfo SpecbublboblRomDesc[] = {
	{ "Bubble Bobble (1987)(Firebird Software)[128K].z80", 0x0b5c8, 0x7919a50e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbublbobl, Specbublbobl, Spec128)
STD_ROM_FN(Specbublbobl)

struct BurnDriver BurnSpecbublbobl = {
	"spec_bublbobl", NULL, "spec_spec128", NULL, "1987",
	"Bubble Bobble (128K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbublboblRomInfo, SpecbublboblRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bubble Dizzy (48K)

static struct BurnRomInfo Specbubbdizz_48RomDesc[] = {
	{ "Bubble Dizzy (1991)(Codemasters).z80", 0x09214, 0xfa662366, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbubbdizz_48, Specbubbdizz_48, Spectrum)
STD_ROM_FN(Specbubbdizz_48)

struct BurnDriver BurnSpecbubbdizz_48 = {
	"spec_bubbdizz_48", "spec_bubbdizz", "spec_spectrum", NULL, "1991",
	"Bubble Dizzy (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbubbdizz_48RomInfo, Specbubbdizz_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bubble Dizzy (128K)

static struct BurnRomInfo SpecbubbdizzRomDesc[] = {
	{ "Bubble Dizzy (1991)(Codemasters)[128K].z80", 0x0d9eb, 0xb9086f44, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbubbdizz, Specbubbdizz, Spec128)
STD_ROM_FN(Specbubbdizz)

struct BurnDriver BurnSpecbubbdizz = {
	"spec_bubbdizz", NULL, "spec_spec128", NULL, "1991",
	"Bubble Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbubbdizzRomInfo, SpecbubbdizzRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bulls Eye (48K)

static struct BurnRomInfo SpecbulleyeRomDesc[] = {
	{ "Bulls Eye (1984)(Macsen Software).z80", 0x09944, 0xce764dd1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbulleye, Specbulleye, Spectrum)
STD_ROM_FN(Specbulleye)

struct BurnDriver BurnSpecbulleye = {
	"spec_bulleye", NULL, "spec_spectrum", NULL, "1984",
	"Bulls Eye (48K)\0", NULL, "Macsen Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbulleyeRomInfo, SpecbulleyeRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bullseye (Mastertronic) (48K)

static struct BurnRomInfo SpecbullseymRomDesc[] = {
	{ "Bullseye (1982)(Mastertronic)[aka Darts].z80", 0x0935f, 0x1caf68ca, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbullseym, Specbullseym, Spectrum)
STD_ROM_FN(Specbullseym)

struct BurnDriver BurnSpecbullseym = {
	"spec_bullseym", NULL, "spec_spectrum", NULL, "1982",
	"Bullseye (Mastertronic) (48K)\0", NULL, "Mastertronic", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbullseymRomInfo, SpecbullseymRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Bullseye (GSSCGC) (48K)

static struct BurnRomInfo SpecbullseygRomDesc[] = {
	{ "Bullseye (1997)(CSSCGC).z80", 0x00f78, 0xf985d118, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbullseyg, Specbullseyg, Spectrum)
STD_ROM_FN(Specbullseyg)

struct BurnDriver BurnSpecbullseyg = {
	"spec_bullseyg", NULL, "spec_spectrum", NULL, "1997",
	"Bullseye (GSSCGC) (48K)\0", NULL, "CSSCGC", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbullseygRomInfo, SpecbullseygRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Cabal (128K)

static struct BurnRomInfo SpeccabalRomDesc[] = {
	{ "Cabal (1988)(Ocean)[128K].z80", 0x1757e, 0x6174d654, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccabal, Speccabal, Spec128)
STD_ROM_FN(Speccabal)

struct BurnDriver BurnSpeccabal = {
	"spec_cabal", NULL, "spec_spec128", NULL, "1988",
	"Cabal (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccabalRomInfo, SpeccabalRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Cashdash (48K)

static struct BurnRomInfo SpeccashdashRomDesc[] = {
	{ "Cashdash (19xx)(Tynesoft).z80", 0x083cf, 0x8d84e2b6, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccashdash, Speccashdash, Spectrum)
STD_ROM_FN(Speccashdash)

struct BurnDriver BurnSpeccashdash = {
	"spec_cashdash", NULL, "spec_spectrum", NULL, "19xx",
	"Cashdash (48K)\0", NULL, "Tynesoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccashdashRomInfo, SpeccashdashRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Cauldron (48K)

static struct BurnRomInfo SpeccauldronRomDesc[] = {
	{ "Cauldron (1985)(Palace).z80", 0x09f7b, 0xadea0ad1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccauldron, Speccauldron, Spectrum)
STD_ROM_FN(Speccauldron)

struct BurnDriver BurnSpeccauldron = {
	"spec_cauldron", NULL, "spec_spectrum", NULL, "1985",
	"Cauldron (48K)\0", NULL, "Palace Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccauldronRomInfo, SpeccauldronRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Cauldron II - The Pumpkin Strikes Back (48K)

static struct BurnRomInfo Speccauldrn2RomDesc[] = {
	{ "Cauldron II (1986)(Palace).z80", 0x09349, 0xc73307f0, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccauldrn2, Speccauldrn2, Spectrum)
STD_ROM_FN(Speccauldrn2)

struct BurnDriver BurnSpeccauldrn2 = {
	"spec_cauldrn2", NULL, "spec_spectrum", NULL, "1986",
	"Cauldron II - The Pumpkin Strikes Back (48K)\0", NULL, "Palace Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Speccauldrn2RomInfo, Speccauldrn2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Canyon Warrior (48K)

static struct BurnRomInfo SpeccanywarrRomDesc[] = {
	{ "Canyon Warrior (1989)(Mastertronic Plus).z80", 0x0860a, 0x086df73a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccanywarr, Speccanywarr, Spectrum)
STD_ROM_FN(Speccanywarr)

struct BurnDriver BurnSpeccanywarr = {
	"spec_canywarr", NULL, "spec_spectrum", NULL, "1989",
	"Canyon Warrior (48K)\0", NULL, "Mastertronic Plus", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccanywarrRomInfo, SpeccanywarrRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Championship Jet Ski Simulator - Easy (48K)

static struct BurnRomInfo SpecchjesksiRomDesc[] = {
	{ "Championship Jet Ski Simulator - Easy (1989)(Codemasters).z80", 0x08e58, 0xff247fe4, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specchjesksi, Specchjesksi, Spectrum)
STD_ROM_FN(Specchjesksi)

struct BurnDriver BurnSpecchjesksi = {
	"spec_chjesksi", NULL, "spec_spectrum", NULL, "1989",
	"Championship Jet Ski Simulator - Easy (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecchjesksiRomInfo, SpecchjesksiRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Championship Jet Ski Simulator - Hard (48K)

static struct BurnRomInfo SpecchjesksihRomDesc[] = {
	{ "Championship Jet Ski Simulator - Hard (1989)(Codemasters).z80", 0x08f27, 0xe54f4b7a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specchjesksih, Specchjesksih, Spectrum)
STD_ROM_FN(Specchjesksih)

struct BurnDriver BurnSpecchjesksih = {
	"spec_chjesksih", "spec_chjesksi", "spec_spectrum", NULL, "1989",
	"Championship Jet Ski Simulator - Hard (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecchjesksihRomInfo, SpecchjesksihRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Championship Sprint (48K)

static struct BurnRomInfo SpecchamspriRomDesc[] = {
	{ "Championship Sprint (1988)(Electric Dreams Software).z80", 0x07073, 0xf557d7f7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specchamspri, Specchamspri, Spectrum)
STD_ROM_FN(Specchamspri)

struct BurnDriver BurnSpecchamspri = {
	"spec_chamspri", NULL, "spec_spectrum", NULL, "1988",
	"Championship Sprint (48K)\0", NULL, "Electric Dreams Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecchamspriRomInfo, SpecchamspriRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Chase H.Q. (48K)

static struct BurnRomInfo Specchasehq_48RomDesc[] = {
	{ "Chase H.Q. (1989)(Ocean).z80", 0x0a6f7, 0x5e684c1f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specchasehq_48, Specchasehq_48, Spectrum)
STD_ROM_FN(Specchasehq_48)

struct BurnDriver BurnSpecchasehq_48 = {
	"spec_chasehq_48", "spec_chasehq", "spec_spectrum", NULL, "1989",
	"Chase H.Q. (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specchasehq_48RomInfo, Specchasehq_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Chase H.Q. (128K)

static struct BurnRomInfo SpecchasehqRomDesc[] = {
	{ "Chase H.Q. (1989)(Ocean)[128K].z80", 0x1c04f, 0xbb5ae933, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specchasehq, Specchasehq, Spec128)
STD_ROM_FN(Specchasehq)

struct BurnDriver BurnSpecchasehq = {
	"spec_chasehq", NULL, "spec_spec128", NULL, "1989",
	"Chase H.Q. (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecchasehqRomInfo, SpecchasehqRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Chase H.Q. II - Special Criminal Investigations (128K)

static struct BurnRomInfo Specchasehq2RomDesc[] = {
	{ "Chase H.Q. II - Special Criminal Investigations (1990)(Ocean)[128K].z80", 0x19035, 0x83ac9fea, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specchasehq2, Specchasehq2, Spec128)
STD_ROM_FN(Specchasehq2)

struct BurnDriver BurnSpecchasehq2 = {
	"spec_chasehq2", NULL, "spec_spec128", NULL, "1990",
	"Chase H.Q. II - Special Criminal Investigations (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specchasehq2RomInfo, Specchasehq2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Chequered Flag (48K)

static struct BurnRomInfo SpeccheqflagRomDesc[] = {
	{ "Chequered Flag (1982)(Sinclair Research).z80", 0x08e6f, 0xbe6e657f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccheqflag, Speccheqflag, Spectrum)
STD_ROM_FN(Speccheqflag)

struct BurnDriver BurnSpeccheqflag = {
	"spec_cheqflag", NULL, "spec_spectrum", NULL, "1982",
	"Chequered Flag (48K)\0", NULL, "Sinclair Research", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccheqflagRomInfo, SpeccheqflagRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Chuckie Egg (48K)

static struct BurnRomInfo SpecchuckeggRomDesc[] = {
	{ "Chuckie Egg (1983)(A & F Software).z80", 0x04c8b, 0xf274304f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specchuckegg, Specchuckegg, Spectrum)
STD_ROM_FN(Specchuckegg)

struct BurnDriver BurnSpecchuckegg = {
	"spec_chuckegg", NULL, "spec_spectrum", NULL, "1983",
	"Chuckie Egg (48K)\0", NULL, "A & F Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecchuckeggRomInfo, SpecchuckeggRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Chuckie Egg 2 (48K)

static struct BurnRomInfo Specchuckeg2RomDesc[] = {
	{ "Chuckie Egg 2 (1985)(A & F Software).z80", 0x0a5d9, 0xd5aa2184, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specchuckeg2, Specchuckeg2, Spectrum)
STD_ROM_FN(Specchuckeg2)

struct BurnDriver BurnSpecchuckeg2 = {
	"spec_chuckeg2", NULL, "spec_spectrum", NULL, "1985",
	"Chuckie Egg 2 (48K)\0", NULL, "A & F Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specchuckeg2RomInfo, Specchuckeg2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// CJ's Elephant Antics (48K)

static struct BurnRomInfo SpeccjselephRomDesc[] = {
	{ "CJ's Elephant Antics (1991)(Codemasters).z80", 0x09f76, 0xc249538d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccjseleph, Speccjseleph, Spectrum)
STD_ROM_FN(Speccjseleph)

struct BurnDriver BurnSpeccjseleph = {
	"spec_cjseleph", NULL, "spec_spectrum", NULL, "1991",
	"CJ's Elephant Antics (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccjselephRomInfo, SpeccjselephRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// CJ In the USA (128K)

static struct BurnRomInfo SpeccjiiiusaRomDesc[] = {
	{ "CJ In the USA (1991)(Codemasters)(128k).z80", 0x0d966, 0xc8e7d99e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccjiiiusa, Speccjiiiusa, Spectrum)
STD_ROM_FN(Speccjiiiusa)

struct BurnDriver BurnSpeccjiiiusa = {
	"spec_cjiiiusa", NULL, "spec_spec128", NULL, "1991",
	"CJ In the USA (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccjiiiusaRomInfo, SpeccjiiiusaRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Combat School (48K)

static struct BurnRomInfo Speccschool_48RomDesc[] = {
	{ "Combat School (1987)(Ocean).z80", 0x0a7c9, 0x5b7421a6, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccschool_48, Speccschool_48, Spectrum)
STD_ROM_FN(Speccschool_48)

struct BurnDriver BurnSpeccschool_48 = {
	"spec_cschool_48", "spec_cschool", "spec_spectrum", NULL, "1987",
	"Combat School (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Speccschool_48RomInfo, Speccschool_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Combat School (128K)

static struct BurnRomInfo SpeccschoolRomDesc[] = {
	{ "Combat School (1987)(Ocean)[128K].z80", 0x19dd1, 0xe66e39b5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccschool, Speccschool, Spec128)
STD_ROM_FN(Speccschool)

struct BurnDriver BurnSpeccschool = {
	"spec_cschool", NULL, "spec_spec128", NULL, "1987",
	"Combat School (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccschoolRomInfo, SpeccschoolRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Commando (fr) (48K)

static struct BurnRomInfo SpeccommandofRomDesc[] = {
	{ "Commando (1984)(Loriciels)(fr).z80", 0x0a352, 0xc2b0cbfe, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccommandof, Speccommandof, Spectrum)
STD_ROM_FN(Speccommandof)

struct BurnDriver BurnSpeccommandof = {
	"spec_commandof", "spec_commando", "spec_spectrum", NULL, "1984",
	"Commando (fr) (48K)\0", NULL, "Loriciels", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccommandofRomInfo, SpeccommandofRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Commando (Pocket Money Software) (48K)

static struct BurnRomInfo SpeccommandopRomDesc[] = {
	{ "Commando (1984)(Pocket Money Software).z80", 0x04924, 0xeaac033a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccommandop, Speccommandop, Spectrum)
STD_ROM_FN(Speccommandop)

struct BurnDriver BurnSpeccommandop = {
	"spec_commandop", "spec_commando", "spec_spectrum", NULL, "1984",
	"Commando (Pocket Money Software) (48K)\0", NULL, "Pocket Money Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccommandopRomInfo, SpeccommandopRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Commando (Elite Systems) (48K)

static struct BurnRomInfo SpeccommandoRomDesc[] = {
	{ "Commando (1985)(Elite Systems).z80", 0x0a0dc, 0x6cabf85d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccommando, Speccommando, Spectrum)
STD_ROM_FN(Speccommando)

struct BurnDriver BurnSpeccommando = {
	"spec_commando", NULL, "spec_spectrum", NULL, "1985",
	"Commando (Elite Systems) (48K)\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccommandoRomInfo, SpeccommandoRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Continental Circus (128K)

static struct BurnRomInfo SpeccontcircRomDesc[] = {
	{ "Continental Circus (1989)(Virgin Mastertronic)[128K].z80", 0x0fcb0, 0xf95c5332, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccontcirc, Speccontcirc, Spec128)
STD_ROM_FN(Speccontcirc)

struct BurnDriver BurnSpeccontcirc = {
	"spec_contcirc", NULL, "spec_spec128", NULL, "1989",
	"Continental Circus (128K)\0", NULL, "Virgin Mastertronic", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccontcircRomInfo, SpeccontcircRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Crystal Castles (48K)

static struct BurnRomInfo Speccryscast_48RomDesc[] = {
	{ "Crystal Castles (1986)(U.S. Gold).z80", 0x0c01e, 0xff19b90c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccryscast_48, Speccryscast_48, Spectrum)
STD_ROM_FN(Speccryscast_48)

struct BurnDriver BurnSpeccryscast_48 = {
	"spec_cryscast_48", "spec_cryscast", "spec_spectrum", NULL, "1986",
	"Crystal Castles (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Speccryscast_48RomInfo, Speccryscast_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Crystal Castles (128K)

static struct BurnRomInfo SpeccryscastRomDesc[] = {
	{ "Crystal Castles (1986)(U.S. Gold)[128K].z80", 0x0a752, 0xff640ca8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccryscast, Speccryscast, Spec128)
STD_ROM_FN(Speccryscast)

struct BurnDriver BurnSpeccryscast = {
	"spec_cryscast", NULL, "spec_spec128", NULL, "1986",
	"Crystal Castles (128K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccryscastRomInfo, SpeccryscastRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Cybernoid - The Fighting Machine (48K)

static struct BurnRomInfo Speccythfima_48RomDesc[] = {
	{ "Cybernoid (1988)(Hewson Consultants).z80", 0x0a4fb, 0x539a8179, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccythfima_48, Speccythfima_48, Spectrum)
STD_ROM_FN(Speccythfima_48)

struct BurnDriver BurnSpeccythfima_48 = {
	"spec_cythfima_48", "spec_cythfima", "spec_spectrum", NULL, "1988",
	"Cybernoid - The Fighting Machine (48K)\0", NULL, "Hewson Consultants", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Speccythfima_48RomInfo, Speccythfima_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Cybernoid - The Fighting Machine (128K)

static struct BurnRomInfo SpeccythfimaRomDesc[] = {
	{ "Cybernoid (1988)(Hewson Consultants)(128k).z80", 0x0aac0, 0x4e00cf3d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccythfima, Speccythfima, Spectrum)
STD_ROM_FN(Speccythfima)

struct BurnDriver BurnSpeccythfima = {
	"spec_cythfima", NULL, "spec_spec128", NULL, "1988",
	"Cybernoid - The Fighting Machine (128K)\0", NULL, "Hewson Consultants", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeccythfimaRomInfo, SpeccythfimaRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Cybernoid II - The Revenge (48K)

static struct BurnRomInfo Speccybrnd2_48RomDesc[] = {
	{ "Cybernoid II (1988)(Hewson Consultants).z80", 0x0a67b, 0xb7018e24, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccybrnd2_48, Speccybrnd2_48, Spectrum)
STD_ROM_FN(Speccybrnd2_48)

struct BurnDriver BurnSpeccybrnd2_48 = {
	"spec_cybrnd2_48", "spec_cybrnd2", "spec_spectrum", NULL, "1988",
	"Cybernoid II - The Revenge (48K)\0", NULL, "Hewson Consultants", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Speccybrnd2_48RomInfo, Speccybrnd2_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Cybernoid II - The Revenge (128K)

static struct BurnRomInfo Speccybrnd2RomDesc[] = {
	{ "Cybernoid II (1988)(Hewson Consultants)(128k).z80", 0x0bb35, 0x773b8e31, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speccybrnd2, Speccybrnd2, Spectrum)
STD_ROM_FN(Speccybrnd2)

struct BurnDriver BurnSpeccybrnd2 = {
	"spec_cybrnd2", NULL, "spec_spec128", NULL, "1988",
	"Cybernoid II - The Revenge (128K)\0", NULL, "Hewson Consultants", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Speccybrnd2RomInfo, Speccybrnd2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Daley Thompson's Decathlon - Day 1 (48K)

static struct BurnRomInfo SpecdtdecthnRomDesc[] = {
	{ "Daley Thompson's Decathlon - Day 1 (1984)(Ocean).z80", 0x08bbe, 0xf31094d1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdtdecthn, Specdtdecthn, Spectrum)
STD_ROM_FN(Specdtdecthn)

struct BurnDriver BurnSpecdtdecthn = {
	"spec_dtdecthn", NULL, "spec_spectrum", NULL, "1984",
	"Daley Thompson's Decathlon - Day 1 (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdtdecthnRomInfo, SpecdtdecthnRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Daley Thompson's Decathlon - Day 2 (48K)

static struct BurnRomInfo Specdtdecthn2RomDesc[] = {
	{ "Daley Thompson's Decathlon - Day 2 (1984)(Ocean).z80", 0x08a49, 0x500ca1a5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdtdecthn2, Specdtdecthn2, Spectrum)
STD_ROM_FN(Specdtdecthn2)

struct BurnDriver BurnSpecdtdecthn2 = {
	"spec_dtdecthn2", "spec_dtdecthn", "spec_spectrum", NULL, "1984",
	"Daley Thompson's Decathlon - Day 2 (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdtdecthn2RomInfo, Specdtdecthn2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Daley Thompson's Olympic Challenge (128K)

static struct BurnRomInfo SpecdatholchRomDesc[] = {
	{ "Daley Thompson's Olympic Challenge (1988)(Ocean)[128K].z80", 0x1d5dc, 0xfc2d513f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdatholch, Specdatholch, Spec128)
STD_ROM_FN(Specdatholch)

struct BurnDriver BurnSpecdatholch = {
	"spec_datholch", NULL, "spec_spec128", NULL, "1988",
	"Daley Thompson's Olympic Challenge (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdatholchRomInfo, SpecdatholchRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Daley Thompson's Supertest - Day 1 (48K)

static struct BurnRomInfo Specdtsprtst_48RomDesc[] = {
	{ "Daley Thompson's Supertest - Day 1 (1985)(Ocean).z80", 0x09808, 0xe68ca3de, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdtsprtst_48, Specdtsprtst_48, Spectrum)
STD_ROM_FN(Specdtsprtst_48)

struct BurnDriver BurnSpecdtsprtst_48 = {
	"spec_dtsprtst_48", "spec_dtsprtst", "spec_spectrum", NULL, "1985",
	"Daley Thompson's Supertest - Day 1 (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdtsprtst_48RomInfo, Specdtsprtst_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Daley Thompson's Supertest - Day 2 (48K)

static struct BurnRomInfo Specdtsprtst2_48RomDesc[] = {
	{ "Daley Thompson's Supertest - Day 2 (1985)(Ocean).z80", 0x0a230, 0xba051953, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdtsprtst2_48, Specdtsprtst2_48, Spectrum)
STD_ROM_FN(Specdtsprtst2_48)

struct BurnDriver BurnSpecdtsprtst2_48 = {
	"spec_dtsprtst2_48", "spec_dtsprtst", "spec_spectrum", NULL, "1985",
	"Daley Thompson's Supertest - Day 2 (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdtsprtst2_48RomInfo, Specdtsprtst2_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Daley Thompson's Supertest (128K)

static struct BurnRomInfo SpecdtsprtstRomDesc[] = {
	{ "Daley Thompson's Supertest (1985)(Ocean)[128K].z80", 0x190ba, 0xc6bbb38c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdtsprtst, Specdtsprtst, Spec128)
STD_ROM_FN(Specdtsprtst)

struct BurnDriver BurnSpecdtsprtst = {
	"spec_dtsprtst", NULL, "spec_spec128", NULL, "1985",
	"Daley Thompson's Supertest (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdtsprtstRomInfo, SpecdtsprtstRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dan Dare - Pilot of the Future (48K)

static struct BurnRomInfo SpecdandareRomDesc[] = {
	{ "Dan Dare - Pilot of the Future (1986)(Virgin Games).z80", 0x0a5ac, 0x9378e2c3, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdandare, Specdandare, Spectrum)
STD_ROM_FN(Specdandare)

struct BurnDriver BurnSpecdandare = {
	"spec_dandare", NULL, "spec_spectrum", NULL, "1986",
	"Dan Dare - Pilot of the Future (48K)\0", NULL, "Virgin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdandareRomInfo, SpecdandareRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dan Dare II - Mekon's Revenge (48K)

static struct BurnRomInfo Specdandare2RomDesc[] = {
	{ "Dan Dare II - Mekon's Revenge (1988)(Virgin Games).z80", 0x09f88, 0xcbd5a032, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdandare2, Specdandare2, Spectrum)
STD_ROM_FN(Specdandare2)

struct BurnDriver BurnSpecdandare2 = {
	"spec_dandare2", NULL, "spec_spectrum", NULL, "1988",
	"Dan Dare II - Mekon's Revenge (48K)\0", NULL, "Virgin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdandare2RomInfo, Specdandare2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dan Dare III - The Escape (48K)

static struct BurnRomInfo Specdandare3RomDesc[] = {
	{ "Dan Dare III - The Escape (1990)(Virgin Games).z80", 0x0a6a4, 0xad0767cb, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdandare3, Specdandare3, Spectrum)
STD_ROM_FN(Specdandare3)

struct BurnDriver BurnSpecdandare3 = {
	"spec_dandare3", NULL, "spec_spectrum", NULL, "1990",
	"Dan Dare III - The Escape (48K)\0", NULL, "Virgin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdandare3RomInfo, Specdandare3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Darius (48K)

static struct BurnRomInfo SpecdariusRomDesc[] = {
	{ "Darius (1990)(The Edge Software)[t].z80", 0x0777d, 0x9f6c19d6, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdarius, Specdarius, Spectrum)
STD_ROM_FN(Specdarius)

struct BurnDriver BurnSpecdarius = {
	"spec_darius", NULL, "spec_spectrum", NULL, "1990",
	"Darius (48K)\0", NULL, "The Edge Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdariusRomInfo, SpecdariusRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy - Down the Rapids (48K)

static struct BurnRomInfo Specdizdowra_48RomDesc[] = {
	{ "Dizzy - Down the Rapids (1991)(Codemasters).z80", 0x08372, 0xfc6302d6, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizdowra_48, Specdizdowra_48, Spectrum)
STD_ROM_FN(Specdizdowra_48)

struct BurnDriver BurnSpecdizdowra_48 = {
	"spec_dizdowra_48", "spec_dizdowra", "spec_spectrum", NULL, "1991",
	"Dizzy - Down the Rapids (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizdowra_48RomInfo, Specdizdowra_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy - Down the Rapids (128K)

static struct BurnRomInfo SpecdizdowraRomDesc[] = {
	{ "Dizzy - Down the Rapids (1991)(Codemasters)[128K].z80", 0x0a17e, 0x426abaa3, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizdowra, Specdizdowra, Spec128)
STD_ROM_FN(Specdizdowra)

struct BurnDriver BurnSpecdizdowra = {
	"spec_dizdowra", NULL, "spec_spec128", NULL, "1991",
	"Dizzy - Down the Rapids (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdizdowraRomInfo, SpecdizdowraRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy - The Ultimate Cartoon Adventure (Russian) (128K)

static struct BurnRomInfo SpecdizzyrRomDesc[] = {
	{ "Dizzy - The Ultimate Cartoon Adventure (1987)(Codemasters)(ru)[128K].z80", 0x0c83c, 0x27b9e86c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzyr, Specdizzyr, Spec128)
STD_ROM_FN(Specdizzyr)

struct BurnDriver BurnSpecdizzyr = {
	"spec_dizzyr", "spec_dizzy", "spec_spec128", NULL, "1987",
	"Dizzy - The Ultimate Cartoon Adventure (Russian) (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdizzyrRomInfo, SpecdizzyrRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy - The Ultimate Cartoon Adventure (48K)

static struct BurnRomInfo Specdizzy_48RomDesc[] = {
	{ "Dizzy - The Ultimate Cartoon Adventure (1987)(Codemasters).z80", 0x0b21b, 0xfe79e93a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy_48, Specdizzy_48, Spectrum)
STD_ROM_FN(Specdizzy_48)

struct BurnDriver BurnSpecdizzy_48 = {
	"spec_dizzy_48", "spec_dizzy", "spec_spectrum", NULL, "1987",
	"Dizzy - The Ultimate Cartoon Adventure (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy_48RomInfo, Specdizzy_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy - The Ultimate Cartoon Adventure (128K)

static struct BurnRomInfo SpecdizzyRomDesc[] = {
	{ "Dizzy - The Ultimate Cartoon Adventure (1987)(Codemasters)[128K].z80", 0x0b82d, 0x30bb57e1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy, Specdizzy, Spec128)
STD_ROM_FN(Specdizzy)

struct BurnDriver BurnSpecdizzy = {
	"spec_dizzy", NULL, "spec_spec128", NULL, "1987",
	"Dizzy - The Ultimate Cartoon Adventure (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdizzyRomInfo, SpecdizzyRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy II - Treasure Island Dizzy (Russian) (128K)

static struct BurnRomInfo Specdizzy2rRomDesc[] = {
	{ "Dizzy II - Treasure Island Dizzy (1988)(Codemasters)(ru)[128K].z80", 0x0e0a4, 0xccc7f01b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy2r, Specdizzy2r, Spec128)
STD_ROM_FN(Specdizzy2r)

struct BurnDriver BurnSpecdizzy2r = {
	"spec_dizzy2r", NULL, "spec_spec128", NULL, "1988",
	"Dizzy II - Treasure Island Dizzy (Russian) (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy2rRomInfo, Specdizzy2rRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy II - Treasure Island Dizzy (48K)

static struct BurnRomInfo Specdizzy2_48RomDesc[] = {
	{ "Dizzy II - Treasure Island Dizzy (1988)(Codemasters).z80", 0x0b571, 0x63c7c2a9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy2_48, Specdizzy2_48, Spectrum)
STD_ROM_FN(Specdizzy2_48)

struct BurnDriver BurnSpecdizzy2_48 = {
	"spec_dizzy2_48", "spec_dizzy2", "spec_spectrum", NULL, "1988",
	"Dizzy II - Treasure Island Dizzy (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy2_48RomInfo, Specdizzy2_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy II - Treasure Island Dizzy (128K)

static struct BurnRomInfo Specdizzy2RomDesc[] = {
	{ "Dizzy II - Treasure Island Dizzy (1988)(Codemasters)[128K].z80", 0x0da56, 0x3d2e194d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy2, Specdizzy2, Spec128)
STD_ROM_FN(Specdizzy2)

struct BurnDriver BurnSpecdizzy2 = {
	"spec_dizzy2", NULL, "spec_spec128", NULL, "1988",
	"Dizzy II - Treasure Island Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy2RomInfo, Specdizzy2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy III - Fantasy World Dizzy (Russian) (128K)

static struct BurnRomInfo Specdizzy3rRomDesc[] = {
	{ "Dizzy III - Fantasy World Dizzy (1989)(Codemasters)(ru)[128K].z80", 0x0f2bd, 0x1ae2c460, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy3r, Specdizzy3r, Spec128)
STD_ROM_FN(Specdizzy3r)

struct BurnDriver BurnSpecdizzy3r = {
	"spec_dizzy3r", "spec_dizzy3", "spec_spec128", NULL, "1989",
	"Dizzy III - Fantasy World Dizzy (Russian) (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy3rRomInfo, Specdizzy3rRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy III - Fantasy World Dizzy (48K)

static struct BurnRomInfo Specdizzy3_48RomDesc[] = {
	{ "Dizzy III - Fantasy World Dizzy (1989)(Codemasters).z80", 0x0b391, 0xb4c2f20b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy3_48, Specdizzy3_48, Spectrum)
STD_ROM_FN(Specdizzy3_48)

struct BurnDriver BurnSpecdizzy3_48 = {
	"spec_dizzy3_48", "spec_dizzy3", "spec_spectrum", NULL, "1989",
	"Dizzy III - Fantasy World Dizzy (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy3_48RomInfo, Specdizzy3_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy III - Fantasy World Dizzy (128K)

static struct BurnRomInfo Specdizzy3RomDesc[] = {
	{ "Dizzy III - Fantasy World Dizzy (1989)(Codemasters)[128K].z80", 0x0f172, 0xef716059, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy3, Specdizzy3, Spec128)
STD_ROM_FN(Specdizzy3)

struct BurnDriver BurnSpecdizzy3 = {
	"spec_dizzy3", NULL, "spec_spec128", NULL, "1989",
	"Dizzy III - Fantasy World Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy3RomInfo, Specdizzy3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy IV - Magicland Dizzy (48K)

static struct BurnRomInfo Specdizzy4_48RomDesc[] = {
	{ "Dizzy IV - Magicland Dizzy (1989)(Codemasters).z80", 0x0b54a, 0x19009d03, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy4_48, Specdizzy4_48, Spectrum)
STD_ROM_FN(Specdizzy4_48)

struct BurnDriver BurnSpecdizzy4_48 = {
	"spec_dizzy4_48", "spec_dizzy4", "spec_spectrum", NULL, "1989",
	"Dizzy IV - Magicland Dizzy (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy4_48RomInfo, Specdizzy4_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy IV - Magicland Dizzy (128K)

static struct BurnRomInfo Specdizzy4RomDesc[] = {
	{ "Dizzy IV - Magicland Dizzy (1989)(Codemasters)[128K].z80", 0x0e107, 0x94e8903f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy4, Specdizzy4, Spec128)
STD_ROM_FN(Specdizzy4)

struct BurnDriver BurnSpecdizzy4 = {
	"spec_dizzy4", NULL, "spec_spec128", NULL, "1989",
	"Dizzy IV - Magicland Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy4RomInfo, Specdizzy4RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy V - Spellbound Dizzy (48K)

static struct BurnRomInfo Specdizzy5_48RomDesc[] = {
	{ "Dizzy V - Spellbound Dizzy (1991)(Codemasters).z80", 0x0b9c1, 0xdf6849fe, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy5_48, Specdizzy5_48, Spectrum)
STD_ROM_FN(Specdizzy5_48)

struct BurnDriver BurnSpecdizzy5_48 = {
	"spec_dizzy5_48", "spec_dizzy5", "spec_spectrum", NULL, "1991",
	"Dizzy V - Spellbound Dizzy (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy5_48RomInfo, Specdizzy5_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy V - Spellbound Dizzy (128K)

static struct BurnRomInfo Specdizzy5RomDesc[] = {
	{ "Dizzy V - Spellbound Dizzy (1991)(Codemasters)[128K].z80", 0x15e6f, 0x6769bc2e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy5, Specdizzy5, Spec128)
STD_ROM_FN(Specdizzy5)

struct BurnDriver BurnSpecdizzy5 = {
	"spec_dizzy5", NULL, "spec_spec128", NULL, "1991",
	"Dizzy V - Spellbound Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy5RomInfo, Specdizzy5RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy VI - Prince of the Yolkfolk (48K)

static struct BurnRomInfo Specdizzy6_48RomDesc[] = {
	{ "Dizzy VI - Prince of the Yolkfolk (1991)(Codemasters).z80", 0x0a17a, 0xd3791a7a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy6_48, Specdizzy6_48, Spectrum)
STD_ROM_FN(Specdizzy6_48)

struct BurnDriver BurnSpecdizzy6_48 = {
	"spec_dizzy6_48", "spec_dizzy6", "spec_spectrum", NULL, "1991",
	"Dizzy VI - Prince of the Yolkfolk (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy6_48RomInfo, Specdizzy6_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy VI - Prince of the Yolkfolk (128K)

static struct BurnRomInfo Specdizzy6RomDesc[] = {
	{ "Dizzy VI - Prince of the Yolkfolk (1991)(Codemasters)[128K].z80", 0x0b6d2, 0x84574abf, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy6, Specdizzy6, Spec128)
STD_ROM_FN(Specdizzy6)

struct BurnDriver BurnSpecdizzy6 = {
	"spec_dizzy6", NULL, "spec_spec128", NULL, "1991",
	"Dizzy VI - Prince of the Yolkfolk (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy6RomInfo, Specdizzy6RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy VII - Crystal Kingdom Dizzy (48K)

static struct BurnRomInfo Specdizzy7_48RomDesc[] = {
	{ "Dizzy VII - Crystal Kingdom Dizzy (1992)(Codemasters).z80", 0x0a58f, 0x9ca9af5e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy7_48, Specdizzy7_48, Spectrum)
STD_ROM_FN(Specdizzy7_48)

struct BurnDriver BurnSpecdizzy7_48 = {
	"spec_dizzy7_48", "spec_dizzy7", "spec_spectrum", NULL, "1992",
	"Dizzy VII - Crystal Kingdom Dizzy (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy7_48RomInfo, Specdizzy7_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy VII - Crystal Kingdom Dizzy (128K)

static struct BurnRomInfo Specdizzy7RomDesc[] = {
	{ "Dizzy VII - Crystal Kingdom Dizzy (1992)(Codemasters)[128K].z80", 0x0b91d, 0x16fb82f0, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy7, Specdizzy7, Spec128)
STD_ROM_FN(Specdizzy7)

struct BurnDriver BurnSpecdizzy7 = {
	"spec_dizzy7", NULL, "spec_spec128", NULL, "1992",
	"Dizzy VII - Crystal Kingdom Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy7RomInfo, Specdizzy7RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy VII - Crystal Kingdom Dizzy (Russian) (128K)

static struct BurnRomInfo Specdizzy7rRomDesc[] = {
	{ "Dizzy VII - Crystal Kingdom Dizzy (1993)(Codemasters)(ru)[128K].z80", 0x0f132, 0xd6b0801d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy7r, Specdizzy7r, Spec128)
STD_ROM_FN(Specdizzy7r)

struct BurnDriver BurnSpecdizzy7r = {
	"spec_dizzy7r", "spec_dizzy7", "spec_spec128", NULL, "1993",
	"Dizzy VII - Crystal Kingdom Dizzy (Russian) (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy7rRomInfo, Specdizzy7rRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Donkey Kong (48K)

static struct BurnRomInfo SpecdkongRomDesc[] = {
	{ "Donkey Kong (1986)(Ocean).z80", 0x09871, 0x4840171d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdkong, Specdkong, Spectrum)
STD_ROM_FN(Specdkong)

struct BurnDriver BurnSpecdkong = {
	"spec_dkong", NULL, "spec_spectrum", NULL, "1986",
	"Donkey Kong (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdkongRomInfo, SpecdkongRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Donkey Kong Jr. (48K)

static struct BurnRomInfo SpecdkongjrRomDesc[] = {
	{ "Donkey Kong Jr. (19xx)(Sir Clive and Mr ZX).z80", 0x02a1d, 0x91569bef, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdkongjr, Specdkongjr, Spectrum)
STD_ROM_FN(Specdkongjr)

struct BurnDriver BurnSpecdkongjr = {
	"spec_dkongjr", NULL, "spec_spectrum", NULL, "19xx",
	"Donkey Kong Jr. (48K)\0", NULL, "Sir Clive and Mr ZX", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdkongjrRomInfo, SpecdkongjrRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Doom (demo) (128K)

static struct BurnRomInfo SpecdoomdemoRomDesc[] = {
	{ "Doom (demo) (1996)(Digital Reality)[128K].z80", 0x11d9a, 0xb310b6f1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdoomdemo, Specdoomdemo, Spec128)
STD_ROM_FN(Specdoomdemo)

struct BurnDriver BurnSpecdoomdemo = {
	"spec_doomdemo", NULL, "spec_spec128", NULL, "1996",
	"Doom (demo) (128K)\0", NULL, "Digital Reality", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdoomdemoRomInfo, SpecdoomdemoRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Double Dragon III - The Rosetta Stone (128K)

static struct BurnRomInfo Specddragon3RomDesc[] = {
	{ "Double Dragon III - The Rosetta Stone (1991)(Storm Software)[128K].z80", 0x1366a, 0x63257333, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specddragon3, Specddragon3, Spec128)
STD_ROM_FN(Specddragon3)

struct BurnDriver BurnSpecddragon3 = {
	"spec_ddragon3", NULL, "spec_spec128", NULL, "1991",
	"Double Dragon III - The Rosetta Stone (128K)\0", NULL, "Storm Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specddragon3RomInfo, Specddragon3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dragon Breed (48K)

static struct BurnRomInfo SpecdrgbreedRomDesc[] = {
	{ "Dragon Breed (1989)(Activision).z80", 0x0894c, 0x9cfece69, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdrgbreed, Specdrgbreed, Spectrum)
STD_ROM_FN(Specdrgbreed)

struct BurnDriver BurnSpecdrgbreed = {
	"spec_drgbreed", NULL, "spec_spectrum", NULL, "1989",
	"Dragon Breed (48K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdrgbreedRomInfo, SpecdrgbreedRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dragon Ninja (128K)

static struct BurnRomInfo SpecdninjaRomDesc[] = {
	{ "Dragon Ninja (1988)(Imagine Software)[128K].z80", 0x186ff, 0xf7171c64, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdninja, Specdninja, Spec128)
STD_ROM_FN(Specdninja)

struct BurnDriver BurnSpecdninja = {
	"spec_dninja", NULL, "spec_spec128", NULL, "1988",
	"Dragon Ninja (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdninjaRomInfo, SpecdninjaRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dynamite Dan (48K)

static struct BurnRomInfo SpecdynadanRomDesc[] = {
	{ "Dynamite Dan (1985)(Mirrorsoft).z80", 0x0a6a8, 0x218460b1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdynadan, Specdynadan, Spectrum)
STD_ROM_FN(Specdynadan)

struct BurnDriver BurnSpecdynadan = {
	"spec_dynadan", NULL, "spec_spectrum", NULL, "1985",
	"Dynamite Dan (48K)\0", NULL, "Mirrorsoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdynadanRomInfo, SpecdynadanRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dynamite Dan II - Dr. Blitzen and the Islands of Arcanum (48K)

static struct BurnRomInfo Specdynadan2RomDesc[] = {
	{ "Dynamite Dan II - Dr Blitzen And The Islands Of Arcanum (1985)(Mirrorsoft).z80", 0x0aaee, 0xdf00027d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdynadan2, Specdynadan2, Spectrum)
STD_ROM_FN(Specdynadan2)

struct BurnDriver BurnSpecdynadan2 = {
	"spec_dynadan2", NULL, "spec_spectrum", NULL, "1986",
	"Dynamite Dan II - Dr. Blitzen and the Islands of Arcanum (48K)\0", NULL, "Mirrorsoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdynadan2RomInfo, Specdynadan2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Elite (48K)

static struct BurnRomInfo SpeceliteRomDesc[] = {
	{ "Elite (1985)(Firebird Software).z80", 0x0c01e, 0xcbe63a0e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specelite, Specelite, Spectrum)
STD_ROM_FN(Specelite)

struct BurnDriver BurnSpecelite = {
	"spec_elite", NULL, "spec_spectrum", NULL, "1985",
	"Elite (48K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeceliteRomInfo, SpeceliteRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Elven Warrior (48K)

static struct BurnRomInfo SpecelvewarrRomDesc[] = {
	{ "Elven Warrior (1989)(Players Premier).z80", 0x08e1a, 0x53401159, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specelvewarr, Specelvewarr, Spectrum)
STD_ROM_FN(Specelvewarr)

struct BurnDriver BurnSpecelvewarr = {
	"spec_elvewarr", NULL, "spec_spectrum", NULL, "1989",
	"Elven Warrior (48K)\0", NULL, "Players Premier Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecelvewarrRomInfo, SpecelvewarrRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Emulator Speed Test (48K)

static struct BurnRomInfo SpecemuspdtRomDesc[] = {
	{ "Emulator Speed Test (19xx)(-).z80", 0x00ff4, 0xff1bd89e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specemuspdt, Specemuspdt, Spectrum)
STD_ROM_FN(Specemuspdt)

struct BurnDriver BurnSpecemuspdt = {
	"spec_emuspdt", NULL, "spec_spectrum", NULL, "19xx",
	"Emulator Speed Test (48K)\0", NULL, "Unknown", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecemuspdtRomInfo, SpecemuspdtRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Enduro Racer (128K)

static struct BurnRomInfo SpecenduroRomDesc[] = {
	{ "Enduro Racer (1987)(Activision)[128K].z80", 0x0a738, 0xf9d78fc5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specenduro, Specenduro, Spec128)
STD_ROM_FN(Specenduro)

struct BurnDriver BurnSpecenduro = {
	"spec_enduro", NULL, "spec_spec128", NULL, "1987",
	"Enduro Racer (128K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecenduroRomInfo, SpecenduroRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Exolon (48K)

static struct BurnRomInfo Specexolon_48RomDesc[] = {
	{ "Exolon (1987)(Hewson Consultants).z80", 0x0928e, 0xc6b22a79, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specexolon_48, Specexolon_48, Spectrum)
STD_ROM_FN(Specexolon_48)

struct BurnDriver BurnSpecexolon_48 = {
	"spec_exolon_48", "spec_exolon", "spec_spectrum", NULL, "1987",
	"Exolon (48K)\0", NULL, "Hewson Consultants", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specexolon_48RomInfo, Specexolon_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Exolon (128K)

static struct BurnRomInfo SpecexolonRomDesc[] = {
	{ "Exolon (1987)(Hewson Consultants)(128k).z80", 0x0cd94, 0xab5a464b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specexolon, Specexolon, Spectrum)
STD_ROM_FN(Specexolon)

struct BurnDriver BurnSpecexolon = {
	"spec_exolon", NULL, "spec_spec128", NULL, "1987",
	"Exolon (128K)\0", NULL, "Hewson Consultants", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecexolonRomInfo, SpecexolonRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Fantasy Zone 2, The (128K)

static struct BurnRomInfo Specfntzone2RomDesc[] = {
	{ "Fantasy Zone 2, The (19xx)(-)[128K].z80", 0x05c6c, 0xe09d79d8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfntzone2, Specfntzone2, Spec128)
STD_ROM_FN(Specfntzone2)

struct BurnDriver BurnSpecfntzone2 = {
	"spec_fntzone2", NULL, "spec_spec128", NULL, "19xx",
	"Fantasy Zone 2, The (128K)\0", NULL, "Unknown", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specfntzone2RomInfo, Specfntzone2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Fast Food Dizzy (48K)

static struct BurnRomInfo Specffdizzy_48RomDesc[] = {
	{ "Fast Food Dizzy (1989)(Codemasters).z80", 0x0a6e6, 0x40b68f49, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specffdizzy_48, Specffdizzy_48, Spectrum)
STD_ROM_FN(Specffdizzy_48)

struct BurnDriver BurnSpecffdizzy_48 = {
	"spec_ffdizzy_48", "spec_ffdizzy", "spec_spectrum", NULL, "1989",
	"Fast Food Dizzy (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specffdizzy_48RomInfo, Specffdizzy_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Fast Food Dizzy (128K)

static struct BurnRomInfo SpecffdizzyRomDesc[] = {
	{ "Fast Food Dizzy (1989)(Codemasters)[128K].z80", 0x0bb12, 0x83608e22, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specffdizzy, Specffdizzy, Spec128)
STD_ROM_FN(Specffdizzy)

struct BurnDriver BurnSpecffdizzy = {
	"spec_ffdizzy", NULL, "spec_spec128", NULL, "1989",
	"Fast Food Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecffdizzyRomInfo, SpecffdizzyRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Feud (48K)

static struct BurnRomInfo Specfeud_48RomDesc[] = {
	{ "Feud (1987)(Bulldog).z80", 0x096cf, 0xe9d169a7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfeud_48, Specfeud_48, Spectrum)
STD_ROM_FN(Specfeud_48)

struct BurnDriver BurnSpecfeud_48 = {
	"spec_feud_48", "spec_feud", "spec_spectrum", NULL, "1987",
	"Feud (48K)\0", NULL, "Bulldog", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specfeud_48RomInfo, Specfeud_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Feud (128K)

static struct BurnRomInfo SpecfeudRomDesc[] = {
	{ "Feud (1987)(Bulldog)[128K].z80", 0x09d6f, 0x71ccae18, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfeud, Specfeud, Spec128)
STD_ROM_FN(Specfeud)

struct BurnDriver BurnSpecfeud = {
	"spec_feud", NULL, "spec_spec128", NULL, "1987",
	"Feud (128K)\0", NULL, "Bulldog", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecfeudRomInfo, SpecfeudRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Flying Shark (48K)

static struct BurnRomInfo SpecfsharkRomDesc[] = {
	{ "Flying Shark (1987)(Firebird Software).z80", 0x095b8, 0xefc98de4, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfshark, Specfshark, Spectrum)
STD_ROM_FN(Specfshark)

struct BurnDriver BurnSpecfshark = {
	"spec_fshark", NULL, "spec_spectrum", NULL, "1987",
	"Flying Shark (48K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecfsharkRomInfo, SpecfsharkRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Football Director (48K)

static struct BurnRomInfo SpecfootdireRomDesc[] = {
	{ "Football Director (1986)(D&H Games).z80", 0x09abb, 0x6fdbf2bd, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfootdire, Specfootdire, Spectrum)
STD_ROM_FN(Specfootdire)

struct BurnDriver BurnSpecfootdire = {
	"spec_footdire", NULL, "spec_spectrum", NULL, "1986",
	"Football Director (48K)\0", NULL, "D&H Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecfootdireRomInfo, SpecfootdireRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Football Manager - Players (48K)

static struct BurnRomInfo SpecftmanpRomDesc[] = {
	{ "Football Manager - Players (1982)(Addictive Games).z80", 0x09026, 0x0691e14c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specftmanp, Specftmanp, Spectrum)
STD_ROM_FN(Specftmanp)

struct BurnDriver BurnSpecftmanp = {
	"spec_ftmanp", NULL, "spec_spectrum", NULL, "1982",
	"Football Manager - Players (48K)\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecftmanpRomInfo, SpecftmanpRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Football Manager - World Cup Edition (Spanish) (48K)

static struct BurnRomInfo SpecftmanwcRomDesc[] = {
	{ "Football Manager - World Cup Edition (1990)(Addictive Games)(es).z80", 0x0b031, 0x8f0527c8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specftmanwc, Specftmanwc, Spectrum)
STD_ROM_FN(Specftmanwc)

struct BurnDriver BurnSpecftmanwc = {
	"spec_ftmanwc", NULL, "spec_spectrum", NULL, "1990",
	"Football Manager - World Cup Edition (Spanish) (48K)\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecftmanwcRomInfo, SpecftmanwcRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Football Manager (Addictive Games) (48K)

static struct BurnRomInfo SpecftmanRomDesc[] = {
	{ "Football Manager (1982)(Addictive Games).z80", 0x0824d, 0x54fd204c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specftman, Specftman, Spectrum)
STD_ROM_FN(Specftman)

struct BurnDriver BurnSpecftman = {
	"spec_ftman", NULL, "spec_spectrum", NULL, "1982",
	"Football Manager (Addictive Games) (48K)\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecftmanRomInfo, SpecftmanRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Football Manager (Unknown) (48K)

static struct BurnRomInfo SpecftmanaRomDesc[] = {
	{ "Football Manager (19xx)(-).z80", 0x09369, 0xd190faf5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specftmana, Specftmana, Spectrum)
STD_ROM_FN(Specftmana)

struct BurnDriver BurnSpecftmana = {
	"spec_ftmana", "spec_ftman", "spec_spectrum", NULL, "19xx",
	"Football Manager (Unknown) (48K)\0", NULL, "Unknown", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecftmanaRomInfo, SpecftmanaRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Football Manager 2 - Expansion Kit (48K)

static struct BurnRomInfo Specfootmn2eRomDesc[] = {
	{ "Football Manager 2 - Expansion Kit (1989)(Addictive Games).z80", 0x09efc, 0x155b7053, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfootmn2e, Specfootmn2e, Spectrum)
STD_ROM_FN(Specfootmn2e)

struct BurnDriver BurnSpecfootmn2e = {
	"spec_footmn2e", NULL, "spec_spectrum", NULL, "1989",
	"Football Manager 2 - Expansion Kit (48K)\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specfootmn2eRomInfo, Specfootmn2eRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Football Manager 2 (48K)

static struct BurnRomInfo Specfootman2RomDesc[] = {
	{ "Football Manager 2 (1988)(Addictive Games).z80", 0x0aad0, 0xb305dce4, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfootman2, Specfootman2, Spectrum)
STD_ROM_FN(Specfootman2)

struct BurnDriver BurnSpecfootman2 = {
	"spec_footman2", NULL, "spec_spectrum", NULL, "1988",
	"Football Manager 2 (48K)\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specfootman2RomInfo, Specfootman2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Football Manager 3 (48K)

static struct BurnRomInfo Specfootman3RomDesc[] = {
	{ "Football Manager 3 (1992)(Addictive Games).z80", 0x069f2, 0x33db96d5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfootman3, Specfootman3, Spectrum)
STD_ROM_FN(Specfootman3)

struct BurnDriver BurnSpecfootman3 = {
	"spec_footman3", NULL, "spec_spectrum", NULL, "1992",
	"Football Manager 3 (48K)\0", NULL, "Addictive Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specfootman3RomInfo, Specfootman3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Footballer of the Year (48K)

static struct BurnRomInfo SpecfotyRomDesc[] = {
	{ "Footballer of the Year (1986)(Gremlin Graphics Software).z80", 0x08137, 0x2af522d0, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfoty, Specfoty, Spectrum)
STD_ROM_FN(Specfoty)

struct BurnDriver BurnSpecfoty = {
	"spec_foty", NULL, "spec_spectrum", NULL, "1986",
	"Footballer of the Year (48K)\0", NULL, "Gremlin Graphics Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecfotyRomInfo, SpecfotyRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Footballer of the Year 2 (48K)

static struct BurnRomInfo Specfoty2RomDesc[] = {
	{ "Footballer of the Year 2 (1987)(Gremlin Graphics Software).z80", 0x08d8f, 0x3722f534, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfoty2, Specfoty2, Spectrum)
STD_ROM_FN(Specfoty2)

struct BurnDriver BurnSpecfoty2 = {
	"spec_foty2", NULL, "spec_spectrum", NULL, "1987",
	"Footballer of the Year 2 (48K)\0", NULL, "Gremlin Graphics Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specfoty2RomInfo, Specfoty2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Forgotten Worlds (128K)

static struct BurnRomInfo SpecforgottnRomDesc[] = {
	{ "Forgotten Worlds (1989)(U.S. Gold)[128K].z80", 0x0e045, 0x33ef767e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specforgottn, Specforgottn, Spec128)
STD_ROM_FN(Specforgottn)

struct BurnDriver BurnSpecforgottn = {
	"spec_forgottn", NULL, "spec_spec128", NULL, "1989",
	"Forgotten Worlds (128K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecforgottnRomInfo, SpecforgottnRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Fred (48K)

static struct BurnRomInfo SpecfredRomDesc[] = {
	{ "Fred (1984)(Quicksilva).z80", 0x05d9f, 0xb653c769, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfred, Specfred, Spectrum)
STD_ROM_FN(Specfred)

struct BurnDriver BurnSpecfred = {
	"spec_fred", NULL, "spec_spectrum", NULL, "1984",
	"Fred (48K)\0", NULL, "Quicksilva", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecfredRomInfo, SpecfredRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Frogger (48K)

static struct BurnRomInfo SpecfroggerRomDesc[] = {
	{ "Frogger (1983)(A & F Software).z80", 0x03aa0, 0x52075e8c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfrogger, Specfrogger, Spectrum)
STD_ROM_FN(Specfrogger)

struct BurnDriver BurnSpecfrogger = {
	"spec_frogger", NULL, "spec_spectrum", NULL, "1983",
	"Frogger (48K)\0", NULL, "A & F Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecfroggerRomInfo, SpecfroggerRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Galaxian (48K)

static struct BurnRomInfo SpecgalaxianRomDesc[] = {
	{ "Galaxian (1984)(Atarisoft).z80", 0x03d8c, 0x8990baef, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specgalaxian, Specgalaxian, Spectrum)
STD_ROM_FN(Specgalaxian)

struct BurnDriver BurnSpecgalaxian = {
	"spec_galaxian", NULL, "spec_spectrum", NULL, "1984",
	"Galaxian (48K)\0", NULL, "Atarisoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecgalaxianRomInfo, SpecgalaxianRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Game Over (128K)

static struct BurnRomInfo SpecgameoverRomDesc[] = {
	{ "Game Over (1987)(Dinamic Software - Imagine Software)(128k).z80", 0x13750, 0x3dcd6f8e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specgameover, Specgameover, Spectrum)
STD_ROM_FN(Specgameover)

struct BurnDriver BurnSpecgameover = {
	"spec_gameover", NULL, "spec_spec128", NULL, "1987",
	"Game Over (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecgameoverRomInfo, SpecgameoverRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Gary Lineker's Hot-Shot! (48K)

static struct BurnRomInfo SpecglinhtRomDesc[] = {
	{ "Gary Lineker's Hot-Shot! (1988)(Gremlin Graphics Software).z80", 0x08a0a, 0x18e1d943, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specglinht, Specglinht, Spectrum)
STD_ROM_FN(Specglinht)

struct BurnDriver BurnSpecglinht = {
	"spec_glinht", NULL, "spec_spectrum", NULL, "1988",
	"Gary Lineker's Hot-Shot! (48K)\0", NULL, "Gremlin Graphics Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecglinhtRomInfo, SpecglinhtRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Gary Lineker's Super Skills (128K)

static struct BurnRomInfo SpecglssRomDesc[] = {
	{ "Gary Lineker's Super Skills (1988)(Gremlin Graphics Software)[128K].z80", 0x12e6f, 0xfcb98fd1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specglss, Specglss, Spec128)
STD_ROM_FN(Specglss)

struct BurnDriver BurnSpecglss = {
	"spec_glss", NULL, "spec_spec128", NULL, "1988",
	"Gary Lineker's Super Skills (128K)\0", NULL, "Gremlin Graphics Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecglssRomInfo, SpecglssRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Gary Lineker's Super Star Soccer (48K)

static struct BurnRomInfo SpecglsssRomDesc[] = {
	{ "Gary Lineker's Super Star Soccer (1987)(Gremlin Graphics Software).z80", 0x09c44, 0x700248d9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specglsss, Specglsss, Spectrum)
STD_ROM_FN(Specglsss)

struct BurnDriver BurnSpecglsss = {
	"spec_glsss", NULL, "spec_spectrum", NULL, "1987",
	"Gary Lineker's Super Star Soccer (48K)\0", NULL, "Gremlin Graphics Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecglsssRomInfo, SpecglsssRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Ghosts 'n' Goblins (48K)

static struct BurnRomInfo Specgng_48RomDesc[] = {
	{ "Ghosts 'n' Goblins (1986)(Elite Systems).z80", 0x0b805, 0xdc252529, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specgng_48, Specgng_48, Spectrum)
STD_ROM_FN(Specgng_48)

struct BurnDriver BurnSpecgng_48 = {
	"spec_gng_48", "spec_gng", "spec_spectrum", NULL, "1986",
	"Ghosts 'n' Goblins (48K)\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specgng_48RomInfo, Specgng_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Ghostbusters (48K)

static struct BurnRomInfo Specghostb_48RomDesc[] = {
	{ "Ghostbusters (1984)(Activision).z80", 0x07940, 0x8002ad90, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specghostb_48, Specghostb_48, Spectrum)
STD_ROM_FN(Specghostb_48)

struct BurnDriver BurnSpecghostb_48 = {
	"spec_ghostb_48", "spec_ghostb", "spec_spectrum", NULL, "1984",
	"Ghostbusters (48K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specghostb_48RomInfo, Specghostb_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Ghostbusters (128K)

static struct BurnRomInfo SpecghostbRomDesc[] = {
	{ "Ghostbusters (1984)(Activision)[128K].z80", 0x110f7, 0x2b3f6071, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specghostb, Specghostb, Spec128)
STD_ROM_FN(Specghostb)

struct BurnDriver BurnSpecghostb = {
	"spec_ghostb", NULL, "spec_spec128", NULL, "1984",
	"Ghostbusters (128K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecghostbRomInfo, SpecghostbRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Ghostbusters II (48K)

static struct BurnRomInfo Specghostb2RomDesc[] = {
	{ "Ghostbusters II (1989)(Activision).z80", 0x08f74, 0x018c57e9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specghostb2, Specghostb2, Spectrum)
STD_ROM_FN(Specghostb2)

struct BurnDriver BurnSpecghostb2 = {
	"spec_ghostb2", NULL, "spec_spectrum", NULL, "1989",
	"Ghostbusters II (48K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specghostb2RomInfo, Specghostb2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Ghouls 'n' Ghosts (128K)

static struct BurnRomInfo SpecgngRomDesc[] = {
	{ "Ghouls 'n' Ghosts (1989)(U.S. Gold)[128K].z80", 0x1a8d2, 0x1b626fe8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specgng, Specgng, Spec128)
STD_ROM_FN(Specgng)

struct BurnDriver BurnSpecgng = {
	"spec_gng", NULL, "spec_spec128", NULL, "1989",
	"Ghouls 'n' Ghosts (128K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecgngRomInfo, SpecgngRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// G-LOC (128K)

static struct BurnRomInfo SpecglocRomDesc[] = {
	{ "G-LOC (1992)(U.S. Gold)[128K].z80", 0x16f39, 0xeeae7278, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specgloc, Specgloc, Spec128)
STD_ROM_FN(Specgloc)

struct BurnDriver BurnSpecgloc = {
	"spec_gloc", NULL, "spec_spec128", NULL, "1992",
	"G-LOC (128K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecglocRomInfo, SpecglocRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Great Escape, The (48K)

static struct BurnRomInfo SpecgreatescRomDesc[] = {
	{ "Great Escape, The (1986)(Ocean).z80", 0x0aada, 0x7af1372a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specgreatesc, Specgreatesc, Spectrum)
STD_ROM_FN(Specgreatesc)

struct BurnDriver BurnSpecgreatesc = {
	"spec_greatesc", NULL, "spec_spectrum", NULL, "1986",
	"Great Escape, The (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecgreatescRomInfo, SpecgreatescRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Green Beret (48K)

static struct BurnRomInfo Specgberet_48RomDesc[] = {
	{ "Green Beret (1986)(Imagine Software).z80", 0x0ad53, 0x55f36544, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specgberet_48, Specgberet_48, Spectrum)
STD_ROM_FN(Specgberet_48)

struct BurnDriver BurnSpecgberet_48 = {
	"spec_gberet_48", "spec_gberet", "spec_spectrum", NULL, "1986",
	"Green Beret (48K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specgberet_48RomInfo, Specgberet_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Green Beret (128K)

static struct BurnRomInfo SpecgberetRomDesc[] = {
	{ "Green Beret (1986)(Imagine Software)[128K].z80", 0x0b5b3, 0x1b61d4ab, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specgberet, Specgberet, Spec128)
STD_ROM_FN(Specgberet)

struct BurnDriver BurnSpecgberet = {
	"spec_gberet", NULL, "spec_spec128", NULL, "1986",
	"Green Beret (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecgberetRomInfo, SpecgberetRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Gryzor (48K)

static struct BurnRomInfo Specgryzor_48RomDesc[] = {
	{ "Gryzor (1987)(Ocean).z80", 0x08fb8, 0x8024e81b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specgryzor_48, Specgryzor_48, Spectrum)
STD_ROM_FN(Specgryzor_48)

struct BurnDriver BurnSpecgryzor_48 = {
	"spec_gryzor_48", "spec_gryzor", "spec_spectrum", NULL, "1987",
	"Gryzor (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specgryzor_48RomInfo, Specgryzor_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Gryzor (128K)

static struct BurnRomInfo SpecgryzorRomDesc[] = {
	{ "Gryzor (1987)(Ocean)[128K].z80", 0x19b7d, 0x1c8c9d01, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specgryzor, Specgryzor, Spec128)
STD_ROM_FN(Specgryzor)

struct BurnDriver BurnSpecgryzor = {
	"spec_gryzor", NULL, "spec_spec128", NULL, "1987",
	"Gryzor (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecgryzorRomInfo, SpecgryzorRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// H.A.T.E. - Hostile All Terrain Encounter (48K)

static struct BurnRomInfo SpechatehateRomDesc[] = {
	{ "H.A.T.E. - Hostile All Terrain Encounter (1989)(Vortex Software).z80", 0x0a176, 0x21ff36ab, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spechatehate, Spechatehate, Spectrum)
STD_ROM_FN(Spechatehate)

struct BurnDriver BurnSpechatehate = {
	"spec_hatehate", NULL, "spec_spectrum", NULL, "1989",
	"H.A.T.E. - Hostile All Terrain Encounter (48K)\0", NULL, "Gremlin Graphics Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpechatehateRomInfo, SpechatehateRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Hard Drivin' (128K)

static struct BurnRomInfo SpecharddrivRomDesc[] = {
	{ "Hard Drivin' (1989)(Domark)[128K].z80", 0x0adaa, 0xafe65244, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specharddriv, Specharddriv, Spec128)
STD_ROM_FN(Specharddriv)

struct BurnDriver BurnSpecharddriv = {
	"spec_harddriv", NULL, "spec_spec128", NULL, "1989",
	"Hard Drivin' (128K)\0", NULL, "Domark", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecharddrivRomInfo, SpecharddrivRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Head over Heels (48K)

static struct BurnRomInfo Specheadheel_48RomDesc[] = {
	{ "Head over Heels (1987)(Ocean).z80", 0x0a88d, 0x0e74c53b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specheadheel_48, Specheadheel_48, Spectrum)
STD_ROM_FN(Specheadheel_48)

struct BurnDriver BurnSpecheadheel_48 = {
	"spec_headheel_48", "spec_headheel", "spec_spectrum", NULL, "1987",
	"Head over Heels (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specheadheel_48RomInfo, Specheadheel_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Head over Heels (128K)

static struct BurnRomInfo SpecheadheelRomDesc[] = {
	{ "Head over Heels (1987)(Ocean)[128K].z80", 0x0c5a5, 0x9f148037, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specheadheel, Specheadheel, Spec128)
STD_ROM_FN(Specheadheel)

struct BurnDriver BurnSpecheadheel = {
	"spec_headheel", NULL, "spec_spec128", NULL, "1987",
	"Head over Heels (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecheadheelRomInfo, SpecheadheelRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Heart Broken (48K)

static struct BurnRomInfo SpechrtbbrknRomDesc[] = {
	{ "Heart Broken (1989)(Atlantis Software).z80", 0x0a391, 0xed8b78f5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spechrtbbrkn, Spechrtbbrkn, Spectrum)
STD_ROM_FN(Spechrtbbrkn)

struct BurnDriver BurnSpechrtbbrkn = {
	"spec_hrtbbrkn", NULL, "spec_spectrum", NULL, "1989",
	"Heart Broken (48K)\0", NULL, "Atlantis Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpechrtbbrknRomInfo, SpechrtbbrknRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Heartland (48K)

static struct BurnRomInfo SpecheartlanRomDesc[] = {
	{ "Heartland (1986)(Odin Computer Graphics).z80", 0x0989a, 0x746a07be, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specheartlan, Specheartlan, Spectrum)
STD_ROM_FN(Specheartlan)

struct BurnDriver BurnSpecheartlan = {
	"spec_heartlan", NULL, "spec_spectrum", NULL, "1986",
	"Heartland (48K)\0", NULL, "Odin Computer Graphics", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecheartlanRomInfo, SpecheartlanRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Highway Encounter (48K)

static struct BurnRomInfo SpechighencoRomDesc[] = {
	{ "Highway Encounter (1985)(Vortex Software).z80", 0x0a36c, 0x7946eec7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spechighenco, Spechighenco, Spectrum)
STD_ROM_FN(Spechighenco)

struct BurnDriver BurnSpechighenco = {
	"spec_highenco", NULL, "spec_spectrum", NULL, "1985",
	"Highway Encounter (48K)\0", NULL, "Vortex Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpechighencoRomInfo, SpechighencoRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Hobbit, The v1.0 (48K)

static struct BurnRomInfo Spechobbit2RomDesc[] = {
	{ "Hobbit, The v1.0 (1982)(Melbourne House).z80", 0x0a4c3, 0x46c20d35, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spechobbit2, Spechobbit2, Spectrum)
STD_ROM_FN(Spechobbit2)

struct BurnDriver BurnSpechobbit2 = {
	"spec_hobbit2", "spec_hobbit", "spec_spectrum", NULL, "1982",
	"Hobbit, The v1.0 (48K)\0", NULL, "Melbourne House", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spechobbit2RomInfo, Spechobbit2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Hobbit, The v1.2 (48K)

static struct BurnRomInfo SpechobbitRomDesc[] = {
	{ "Hobbit, The v1.2 (1982)(Melbourne House).z80", 0x0b1ee, 0x10231c84, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spechobbit, Spechobbit, Spectrum)
STD_ROM_FN(Spechobbit)

struct BurnDriver BurnSpechobbit = {
	"spec_hobbit", NULL, "spec_spectrum", NULL, "1982",
	"Hobbit, The v1.2 (48K)\0", NULL, "Melbourne House", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpechobbitRomInfo, SpechobbitRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Horace Goes Skiing (48K)

static struct BurnRomInfo SpechoraceskRomDesc[] = {
	{ "Horace Goes Skiing (1982)(Sinclair Research).z80", 0x0c01e, 0x02cd124b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spechoracesk, Spechoracesk, Spectrum)
STD_ROM_FN(Spechoracesk)

struct BurnDriver BurnSpechoracesk = {
	"spec_horacesk", NULL, "spec_spectrum", NULL, "1982",
	"Horace Goes Skiing (48K)\0", NULL, "Sinclair Research", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpechoraceskRomInfo, SpechoraceskRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// How to be a Complete Bastard (48K)

static struct BurnRomInfo Spechowbast_48RomDesc[] = {
	{ "How to be a Complete Bastard (1987)(Virgin Games).z80", 0x0a37f, 0x559834ba, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spechowbast_48, Spechowbast_48, Spectrum)
STD_ROM_FN(Spechowbast_48)

struct BurnDriver BurnSpechowbast_48 = {
	"spec_howbast_48", "spec_howbast", "spec_spectrum", NULL, "1987",
	"How to be a Complete Bastard (48K)\0", NULL, "Virgin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spechowbast_48RomInfo, Spechowbast_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// How to be a Complete Bastard (128K)

static struct BurnRomInfo SpechowbastRomDesc[] = {
	{ "How to be a Complete Bastard (1987)(Virgin Games)[128K].z80", 0x0e728, 0x7460da43, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spechowbast, Spechowbast, Spec128)
STD_ROM_FN(Spechowbast)

struct BurnDriver BurnSpechowbast = {
	"spec_howbast", NULL, "spec_spec128", NULL, "1987",
	"How to be a Complete Bastard (128K)\0", NULL, "Virgin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpechowbastRomInfo, SpechowbastRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Hunchback (48K)

static struct BurnRomInfo SpechunchbacRomDesc[] = {
	{ "Hunchback (1984)(Ocean).z80", 0x07c76, 0xa7dde347, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spechunchbac, Spechunchbac, Spectrum)
STD_ROM_FN(Spechunchbac)

struct BurnDriver BurnSpechunchbac = {
	"spec_hunchbac", NULL, "spec_spectrum", NULL, "1984",
	"Hunchback (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpechunchbacRomInfo, SpechunchbacRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Hunchback II - Quasimodo's Revenge (48K)

static struct BurnRomInfo Spechnchbac2RomDesc[] = {
	{ "Hunchback II - Quasimodo's Revenge (1985)(Ocean).z80", 0x09254, 0x36eb410c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spechnchbac2, Spechnchbac2, Spectrum)
STD_ROM_FN(Spechnchbac2)

struct BurnDriver BurnSpechnchbac2 = {
	"spec_hnchbac2", NULL, "spec_spectrum", NULL, "1985",
	"Hunchback II - Quasimodo's Revenge (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spechnchbac2RomInfo, Spechnchbac2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Incredible Shrinking Fireman, The (48K)

static struct BurnRomInfo SpecincshrfRomDesc[] = {
	{ "Incredible Shrinking Fireman, The (1986)(Mastertronic).z80", 0x08d68, 0x7061d0e3, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specincshrf, Specincshrf, Spectrum)
STD_ROM_FN(Specincshrf)

struct BurnDriver BurnSpecincshrf = {
	"spec_incshrf", NULL, "spec_spectrum", NULL, "1986",
	"Incredible Shrinking Fireman, The (48K)\0", NULL, "Mastertronic", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecincshrfRomInfo, SpecincshrfRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Indoor Soccer (48K)

static struct BurnRomInfo SpecindrsoccrRomDesc[] = {
	{ "Indoor Soccer (1986)(Magnificent 7 Software).z80", 0x070ed, 0x85a1a21f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specindrsoccr, Specindrsoccr, Spectrum)
STD_ROM_FN(Specindrsoccr)

struct BurnDriver BurnSpecindrsoccr = {
	"spec_indrsoccr", NULL, "spec_spectrum", NULL, "1986",
	"Indoor Soccer (48K)\0", NULL, "Magnificent 7 Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecindrsoccrRomInfo, SpecindrsoccrRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Into the Eagle's Nest (48K)

static struct BurnRomInfo Specinteagn_48RomDesc[] = {
	{ "Into the Eagle's Nest (1987)(Pandora).z80", 0x07635, 0xcdba827b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specinteagn_48, Specinteagn_48, Spectrum)
STD_ROM_FN(Specinteagn_48)

struct BurnDriver BurnSpecinteagn_48 = {
	"spec_inteagn_48", "spec_inteagn", "spec_spectrum", NULL, "1987",
	"Into the Eagle's Nest (48K)\0", NULL, "Pandora", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specinteagn_48RomInfo, Specinteagn_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Into the Eagle's Nest (128K)

static struct BurnRomInfo SpecinteagnRomDesc[] = {
	{ "Into the Eagle's Nest (1987)(Pandora)(128k).z80", 0x097cf, 0xbc1ea176, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specinteagn, Specinteagn, Spectrum)
STD_ROM_FN(Specinteagn)

struct BurnDriver BurnSpecinteagn = {
	"spec_inteagn", NULL, "spec_spec128", NULL, "1987",
	"Into the Eagle's Nest (128K)\0", NULL, "Pandora", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecinteagnRomInfo, SpecinteagnRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Ivan 'Ironman' Stewart's Super Off Road Racer (48K)

static struct BurnRomInfo Specironman_48RomDesc[] = {
	{ "Ivan 'Ironman' Stewart's Super Off Road Racer (1990)(Virgin Games).z80", 0x08cbf, 0x85841cb9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specironman_48, Specironman_48, Spectrum)
STD_ROM_FN(Specironman_48)

struct BurnDriver BurnSpecironman_48 = {
	"spec_ironman_48", "spec_ironman", "spec_spectrum", NULL, "1990",
	"Ivan 'Ironman' Stewart's Super Off Road Racer (48K)\0", NULL, "Virgin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specironman_48RomInfo, Specironman_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Ivan 'Ironman' Stewart's Super Off Road Racer (128K)

static struct BurnRomInfo SpecironmanRomDesc[] = {
	{ "Ivan 'Ironman' Stewart's Super Off Road Racer (1990)(Virgin Games)[128K].z80", 0x0932d, 0x1b98e2aa, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specironman, Specironman, Spec128)
STD_ROM_FN(Specironman)

struct BurnDriver BurnSpecironman = {
	"spec_ironman", NULL, "spec_spec128", NULL, "1990",
	"Ivan 'Ironman' Stewart's Super Off Road Racer (128K)\0", NULL, "Virgin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecironmanRomInfo, SpecironmanRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Jet Set Willy (48K)

static struct BurnRomInfo SpecjswillyRomDesc[] = {
	{ "Jet Set Willy (1984)(Software Projects).z80", 0x0c01e, 0x06cfad72, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specjswilly, Specjswilly, Spectrum)
STD_ROM_FN(Specjswilly)

struct BurnDriver BurnSpecjswilly = {
	"spec_jswilly", NULL, "spec_spectrum", NULL, "1984",
	"Jet Set Willy (48K)\0", NULL, "Software Projects", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecjswillyRomInfo, SpecjswillyRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Jet Set Willy II - The Final Frontier (48K)

static struct BurnRomInfo Specjswilly2RomDesc[] = {
	{ "Jet Set Willy II - The Final Frontier (1985)(Software Projects).z80", 0x0a5d6, 0xfdf35d51, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specjswilly2, Specjswilly2, Spectrum)
STD_ROM_FN(Specjswilly2)

struct BurnDriver BurnSpecjswilly2 = {
	"spec_jswilly2", NULL, "spec_spectrum", NULL, "1985",
	"Jet Set Willy II - The Final Frontier (48K)\0", NULL, "Software Projects", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specjswilly2RomInfo, Specjswilly2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Jet Set Willy II - The Final Frontier (end) (48K)

static struct BurnRomInfo Specjswilly2eRomDesc[] = {
	{ "Jet Set Willy II - The Final Frontier (1985)(Software Projects)[end].z80", 0x09f58, 0x27f2f88b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specjswilly2e, Specjswilly2e, Spectrum)
STD_ROM_FN(Specjswilly2e)

struct BurnDriver BurnSpecjswilly2e = {
	"spec_jswilly2e", "spec_jswilly2", "spec_spectrum", NULL, "1985",
	"Jet Set Willy II - The Final Frontier (end) (48K)\0", NULL, "Software Projects", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specjswilly2eRomInfo, Specjswilly2eRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Jet Set Willy III (48K)

static struct BurnRomInfo Specjswilly3RomDesc[] = {
	{ "Jet Set Willy III (1985)(MB - APG Software).z80", 0x08061, 0xbde7b5ae, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specjswilly3, Specjswilly3, Spectrum)
STD_ROM_FN(Specjswilly3)

struct BurnDriver BurnSpecjswilly3 = {
	"spec_jswilly3", NULL, "spec_spectrum", NULL, "1985",
	"Jet Set Willy III (48K)\0", NULL, "MB - APG Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specjswilly3RomInfo, Specjswilly3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Jetpac

static struct BurnRomInfo SpecjetpacRomDesc[] = {
	{ "jetpac.z80",     0x02aad, 0x4f96d444, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specjetpac, Specjetpac, Spectrum)
STD_ROM_FN(Specjetpac)

struct BurnDriver BurnSpecjetpac = {
	"spec_jetpac", NULL, "spec_spectrum", NULL, "1983",
	"Jetpac (48K)\0", NULL, "Ultimate Play the Game", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecjetpacRomInfo, SpecjetpacRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Joe Blade (48K)

static struct BurnRomInfo SpecjoebldRomDesc[] = {
	{ "Joe Blade (1987)(Players Premier).z80", 0x097f4, 0xea391957, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specjoebld, Specjoebld, Spectrum)
STD_ROM_FN(Specjoebld)

struct BurnDriver BurnSpecjoebld = {
	"spec_joebld", NULL, "spec_spectrum", NULL, "1987",
	"Joe Blade (48K)\0", NULL, "Players Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecjoebldRomInfo, SpecjoebldRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Joe Blade II (48K)

static struct BurnRomInfo Specjoebld2_48RomDesc[] = {
	{ "Joe Blade 2 (1988)(Players Premier).z80", 0x09b61, 0x382b3651, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specjoebld2_48, Specjoebld2_48, Spectrum)
STD_ROM_FN(Specjoebld2_48)

struct BurnDriver BurnSpecjoebld2_48 = {
	"spec_joebld2_48", "spec_joebld2", "spec_spectrum", NULL, "1988",
	"Joe Blade II (48K)\0", NULL, "Players Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specjoebld2_48RomInfo, Specjoebld2_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Joe Blade II (128K)

static struct BurnRomInfo Specjoebld2RomDesc[] = {
	{ "Joe Blade 2 (1988)(Players Premier)(128k).z80", 0x0bb13, 0x20a19599, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specjoebld2, Specjoebld2, Spectrum)
STD_ROM_FN(Specjoebld2)

struct BurnDriver BurnSpecjoebld2 = {
	"spec_joebld2", NULL, "spec_spec128", NULL, "1988",
	"Joe Blade II (128K)\0", NULL, "Players Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specjoebld2RomInfo, Specjoebld2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Joe Blade III (48K)

static struct BurnRomInfo Specjoebld3RomDesc[] = {
	{ "Joe Blade 3 (1989)(Players Premier)[t].z80", 0x0930d, 0x15c34926, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specjoebld3, Specjoebld3, Spectrum)
STD_ROM_FN(Specjoebld3)

struct BurnDriver BurnSpecjoebld3 = {
	"spec_joebld3", NULL, "spec_spectrum", NULL, "1989",
	"Joe Blade III (48K)\0", NULL, "Players Premier Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specjoebld3RomInfo, Specjoebld3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Kick Off (48K)

static struct BurnRomInfo SpeckickoffRomDesc[] = {
	{ "Kick Off (1989)(Anco Software).z80", 0x08b95, 0x65b432a1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speckickoff, Speckickoff, Spectrum)
STD_ROM_FN(Speckickoff)

struct BurnDriver BurnSpeckickoff = {
	"spec_kickoff", NULL, "spec_spectrum", NULL, "1989",
	"Kick Off (48K)\0", NULL, "Anco Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeckickoffRomInfo, SpeckickoffRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Kick Off 2 (128K)

static struct BurnRomInfo Speckickoff2RomDesc[] = {
	{ "Kick Off 2 (1990)(Anco Software)[128K].z80", 0x0be06, 0xc6367c82, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speckickoff2, Speckickoff2, Spec128)
STD_ROM_FN(Speckickoff2)

struct BurnDriver BurnSpeckickoff2 = {
	"spec_kickoff2", NULL, "spec_spec128", NULL, "1990",
	"Kick Off 2 (128K)\0", NULL, "Anco Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Speckickoff2RomInfo, Speckickoff2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Kick Off World Cup Edition (128K)

static struct BurnRomInfo SpeckickoffwRomDesc[] = {
	{ "Kick Off World Cup Edition (1990)(Anco Software)[128K].z80", 0x09ba6, 0x2a01e70e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speckickoffw, Speckickoffw, Spec128)
STD_ROM_FN(Speckickoffw)

struct BurnDriver BurnSpeckickoffw = {
	"spec_kickoffw", NULL, "spec_spec128", NULL, "1990",
	"Kick Off World Cup Edition (128K)\0", NULL, "Anco Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeckickoffwRomInfo, SpeckickoffwRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Kikstart 2 (48K)

static struct BurnRomInfo Speckikstrt2RomDesc[] = {
	{ "Kikstart 2 (1988)(Mastertronic).z80", 0x08d2d, 0xdb516489, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speckikstrt2, Speckikstrt2, Spectrum)
STD_ROM_FN(Speckikstrt2)

struct BurnDriver BurnSpeckikstrt2 = {
	"spec_kikstrt2", NULL, "spec_spectrum", NULL, "1988",
	"Kikstart 2 (48K)\0", NULL, "Mastertronic", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Speckikstrt2RomInfo, Speckikstrt2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Knight Lore (48K)

static struct BurnRomInfo SpecknigloreRomDesc[] = {
	{ "Knight Lore (1984)(Ultimate Play The Game).z80", 0x08c4c, 0x137ffdb2, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speckniglore, Speckniglore, Spectrum)
STD_ROM_FN(Speckniglore)

struct BurnDriver BurnSpeckniglore = {
	"spec_kniglore", NULL, "spec_spectrum", NULL, "1984",
	"Knight Lore (48K)\0", NULL, "Ultimate Play The Game", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecknigloreRomInfo, SpecknigloreRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Kong (Ocean) (48K)

static struct BurnRomInfo SpeckongRomDesc[] = {
	{ "Kong (1984)(Ocean).z80", 0x0551d, 0x0b3fcf53, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speckong, Speckong, Spectrum)
STD_ROM_FN(Speckong)

struct BurnDriver BurnSpeckong = {
	"spec_kong", NULL, "spec_spectrum", NULL, "1984",
	"Kong (Ocean) (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeckongRomInfo, SpeckongRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Kong 2 - Kong Strikes Back (48K)

static struct BurnRomInfo Speckong2RomDesc[] = {
	{ "Kong 2 - Kong Strikes Back (1985)(Ocean).z80", 0x06fe3, 0x9f2f534a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speckong2, Speckong2, Spectrum)
STD_ROM_FN(Speckong2)

struct BurnDriver BurnSpeckong2 = {
	"spec_kong2", NULL, "spec_spectrum", NULL, "1985",
	"Kong 2 - Kong Strikes Back (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Speckong2RomInfo, Speckong2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Kung-Fu Master (48K)

static struct BurnRomInfo SpeckungfumRomDesc[] = {
	{ "Kung-Fu Master (1986)(U.S. Gold).z80", 0x0a7a1, 0x7a375d9e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speckungfum, Speckungfum, Spectrum)
STD_ROM_FN(Speckungfum)

struct BurnDriver BurnSpeckungfum = {
	"spec_kungfum", NULL, "spec_spectrum", NULL, "1986",
	"Kung-Fu Master (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeckungfumRomInfo, SpeckungfumRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Kwik Snax Dizzy (48K)

static struct BurnRomInfo Specksdizzy_48RomDesc[] = {
	{ "Kwik Snax Dizzy (1990)(Codemasters).z80", 0x095a5, 0xff50b072, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specksdizzy_48, Specksdizzy_48, Spectrum)
STD_ROM_FN(Specksdizzy_48)

struct BurnDriver BurnSpecksdizzy_48 = {
	"spec_ksdizzy_48", "spec_ksdizzy", "spec_spectrum", NULL, "1990",
	"Kwik Snax Dizzy (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specksdizzy_48RomInfo, Specksdizzy_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Kwik Snax Dizzy (128K)

static struct BurnRomInfo SpecksdizzyRomDesc[] = {
	{ "Kwik Snax Dizzy (1990)(Codemasters)[128K].z80", 0x1508b, 0x7e358203, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specksdizzy, Specksdizzy, Spec128)
STD_ROM_FN(Specksdizzy)

struct BurnDriver BurnSpecksdizzy = {
	"spec_ksdizzy", NULL, "spec_spec128", NULL, "1990",
	"Kwik Snax Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecksdizzyRomInfo, SpecksdizzyRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Last Ninja 2 (128K)

static struct BurnRomInfo Speclninja2RomDesc[] = {
	{ "Last Ninja 2 (1988)(System 3 Software)[128K].z80", 0x0aac9, 0x35afe78a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speclninja2, Speclninja2, Spec128)
STD_ROM_FN(Speclninja2)

struct BurnDriver BurnSpeclninja2 = {
	"spec_lninja2", NULL, "spec_spec128", NULL, "1988",
	"Last Ninja 2 (128K)\0", NULL, "System 3 Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Speclninja2RomInfo, Speclninja2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Line of Fire (48K)

static struct BurnRomInfo SpeclinefireRomDesc[] = {
	{ "Line of Fire (1990)(U.S. Gold).z80", 0x09661, 0x9c212b34, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speclinefire, Speclinefire, Spectrum)
STD_ROM_FN(Speclinefire)

struct BurnDriver BurnSpeclinefire = {
	"spec_linefire", NULL, "spec_spectrum", NULL, "1990",
	"Line of Fire (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeclinefireRomInfo, SpeclinefireRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Lode Runner (Part 1 of 2) (48K)

static struct BurnRomInfo Specloderunn_48RomDesc[] = {
	{ "Lode Runner (1984)(Software Projects)(Part 1 of 2).z80", 0x07db5, 0x60fd844e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specloderunn_48, Specloderunn_48, Spectrum)
STD_ROM_FN(Specloderunn_48)

struct BurnDriver BurnSpecloderunn_48 = {
	"spec_loderunn_48", "spec_loderunn", "spec_spectrum", NULL, "1984",
	"Lode Runner (Part 1 of 2) (48K)\0", NULL, "Software Projects", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specloderunn_48RomInfo, Specloderunn_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Lode Runner (Part 2 of 2) (48K)

static struct BurnRomInfo Specloderunn2_48RomDesc[] = {
	{ "Lode Runner (1984)(Software Projects)(Part 2 of 2).z80", 0x07c15, 0x94d3a6c5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specloderunn2_48, Specloderunn2_48, Spectrum)
STD_ROM_FN(Specloderunn2_48)

struct BurnDriver BurnSpecloderunn2_48 = {
	"spec_loderunn2_48", "spec_loderunn", "spec_spectrum", NULL, "1984",
	"Lode Runner (Part 2 of 2) (48K)\0", NULL, "Software Projects", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specloderunn2_48RomInfo, Specloderunn2_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Lode Runner (128K)

static struct BurnRomInfo SpecloderunnRomDesc[] = {
	{ "Lode Runner (1984)(Software Projects)[128K].z80", 0x0fab1, 0x937fee1b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specloderunn, Specloderunn, Spec128)
STD_ROM_FN(Specloderunn)

struct BurnDriver BurnSpecloderunn = {
	"spec_loderunn", NULL, "spec_spec128", NULL, "1984",
	"Lode Runner (128K)\0", NULL, "Software Projects", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecloderunnRomInfo, SpecloderunnRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Lode Runner v2 (48K)

static struct BurnRomInfo Specloderun2RomDesc[] = {
	{ "Lode Runner v2 (1984)(Software Projects).z80", 0x07bf5, 0x4a3ca5b1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specloderun2, Specloderun2, Spectrum)
STD_ROM_FN(Specloderun2)

struct BurnDriver BurnSpecloderun2 = {
	"spec_loderun2", NULL, "spec_spectrum", NULL, "1984",
	"Lode Runner v2 (48K)\0", NULL, "Software Projects", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specloderun2RomInfo, Specloderun2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Lunar Jetman (48K)

static struct BurnRomInfo SpecljetmanRomDesc[] = {
	{ "Lunar Jetman (1983)(Ultimate Play The Game).z80", 0x08e6a, 0x914bc877, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specljetman, Specljetman, Spectrum)
STD_ROM_FN(Specljetman)

struct BurnDriver BurnSpecljetman = {
	"spec_ljetman", NULL, "spec_spectrum", NULL, "1983",
	"Lunar Jetman (48K)\0", NULL, "Ultimate Play The Game", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecljetmanRomInfo, SpecljetmanRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Lunar Rescue (48K)

static struct BurnRomInfo SpeclrescueRomDesc[] = {
	{ "Lunar Rescue (1984)(Lyversoft).z80", 0x046c2, 0x6aac917a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speclrescue, Speclrescue, Spectrum)
STD_ROM_FN(Speclrescue)

struct BurnDriver BurnSpeclrescue = {
	"spec_lrescue", NULL, "spec_spectrum", NULL, "1984",
	"Lunar Rescue (48K)\0", NULL, "Lyversoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeclrescueRomInfo, SpeclrescueRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Manchester United Europe (128K)

static struct BurnRomInfo SpecmanutdeuRomDesc[] = {
	{ "Manchester United Europe (1991)(Krisalis Software)(M5)[128K].z80", 0x0fb26, 0xb4146de0, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmanutdeu, Specmanutdeu, Spec128)
STD_ROM_FN(Specmanutdeu)

struct BurnDriver BurnSpecmanutdeu = {
	"spec_manutdeu", NULL, "spec_spec128", NULL, "1991",
	"Manchester United Europe (128K)\0", NULL, "Krisalis Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmanutdeuRomInfo, SpecmanutdeuRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Manic Miner - Eugene - Lord of the Bathroom (48K)

static struct BurnRomInfo SpecmminrelbRomDesc[] = {
	{ "Manic Miner - Eugene - Lord of the Bathroom (1999)(Manic Miner Technologies).z80", 0x07792, 0x3062e7d8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmminrelb, Specmminrelb, Spectrum)
STD_ROM_FN(Specmminrelb)

struct BurnDriver BurnSpecmminrelb = {
	"spec_mminrelb", NULL, "spec_spectrum", NULL, "1999",
	"Manic Miner - Eugene - Lord of the Bathroom (48K)\0", NULL, "Manic Miner Technologies", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmminrelbRomInfo, SpecmminrelbRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Manic Miner (Bug-Byte Software) (48K)

static struct BurnRomInfo SpecmminerRomDesc[] = {
	{ "Manic Miner (1983)(Bug-Byte Software).z80", 0x06834, 0x024b1971, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmminer, Specmminer, Spectrum)
STD_ROM_FN(Specmminer)

struct BurnDriver BurnSpecmminer = {
	"spec_mminer", NULL, "spec_spectrum", NULL, "1983",
	"Manic Miner (Bug-Byte Software) (48K)\0", NULL, "Bug-Byte Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmminerRomInfo, SpecmminerRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Manic Miner (Software Projects) (48K)

static struct BurnRomInfo SpecmminerspRomDesc[] = {
	{ "Manic Miner (1983)(Software Projects).z80", 0x05ff0, 0x2187b9dd, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmminersp, Specmminersp, Spectrum)
STD_ROM_FN(Specmminersp)

struct BurnDriver BurnSpecmminersp = {
	"spec_mminersp", NULL, "spec_spectrum", NULL, "1983",
	"Manic Miner (Software Projects) (48K)\0", NULL, "Software Projects", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmminerspRomInfo, SpecmminerspRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Marble Madness - Deluxe Edition (48K)

static struct BurnRomInfo SpecmarblemRomDesc[] = {
	{ "Marble Madness - Deluxe Edition (1987)(Melbourne House).z80", 0x0793d, 0x3d08e9ec, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmarblem, Specmarblem, Spectrum)
STD_ROM_FN(Specmarblem)

struct BurnDriver BurnSpecmarblem = {
	"spec_marblem", NULL, "spec_spectrum", NULL, "1987",
	"Marble Madness - Deluxe Edition (48K)\0", NULL, "Melbourne House", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmarblemRomInfo, SpecmarblemRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Mario Bros (48K)

static struct BurnRomInfo SpecmaribrosRomDesc[] = {
	{ "Mario Bros (1987)(Ocean).z80", 0x08f38, 0xe42c245c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmaribros, Specmaribros, Spectrum)
STD_ROM_FN(Specmaribros)

struct BurnDriver BurnSpecmaribros = {
	"spec_maribros", NULL, "spec_spectrum", NULL, "1987",
	"Mario Bros (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmaribrosRomInfo, SpecmaribrosRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Match Day (48K)

static struct BurnRomInfo SpecmatchdayRomDesc[] = {
	{ "Match Day (1985)(Ocean).z80", 0x0a809, 0x59d3bc21, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmatchday, Specmatchday, Spectrum)
STD_ROM_FN(Specmatchday)

struct BurnDriver BurnSpecmatchday = {
	"spec_matchday", NULL, "spec_spectrum", NULL, "1985",
	"Match Day (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmatchdayRomInfo, SpecmatchdayRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Match Day II (48K)

static struct BurnRomInfo Specmatchdy2RomDesc[] = {
	{ "Match Day II (1987)(Ocean).z80", 0x09469, 0x910131c2, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmatchdy2, Specmatchdy2, Spectrum)
STD_ROM_FN(Specmatchdy2)

struct BurnDriver BurnSpecmatchdy2 = {
	"spec_matchdy2", NULL, "spec_spectrum", NULL, "1987",
	"Match Day II (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specmatchdy2RomInfo, Specmatchdy2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Match of the Day (128K)

static struct BurnRomInfo SpecmtchotdRomDesc[] = {
	{ "Match of the Day (1992)(Zeppelin Games)[128K].z80", 0x0cd0a, 0xeb11c05c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmtchotd, Specmtchotd, Spec128)
STD_ROM_FN(Specmtchotd)

struct BurnDriver BurnSpecmtchotd = {
	"spec_mtchotd", NULL, "spec_spec128", NULL, "1992",
	"Match of the Day (128K)\0", NULL, "Zeppelin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmtchotdRomInfo, SpecmtchotdRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Merlin (48K)

static struct BurnRomInfo SpecmerlinRomDesc[] = {
	{ "Merlin (1987)(Firebird).z80", 0x096d1, 0x1c2945a1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmerlin, Specmerlin, Spectrum)
STD_ROM_FN(Specmerlin)

struct BurnDriver BurnSpecmerlin = {
	"spec_merlin", NULL, "spec_spectrum", NULL, "1987",
	"Merlin (48K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmerlinRomInfo, SpecmerlinRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Metal Army (48K)

static struct BurnRomInfo SpecmetaarmyRomDesc[] = {
	{ "Metal Army (1988)(Players Premier).z80", 0x08694, 0xb8b894c5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmetaarmy, Specmetaarmy, Spectrum)
STD_ROM_FN(Specmetaarmy)

struct BurnDriver BurnSpecmetaarmy = {
	"spec_metaarmy", NULL, "spec_spectrum", NULL, "1988",
	"Metal Army (48K)\0", NULL, "Players Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmetaarmyRomInfo, SpecmetaarmyRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Microprose Soccer (48K)

static struct BurnRomInfo Specmicrsocc_48RomDesc[] = {
	{ "Microprose Soccer (1989)(Microprose Software).z80", 0x07ed8, 0x5125611a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmicrsocc_48, Specmicrsocc_48, Spectrum)
STD_ROM_FN(Specmicrsocc_48)

struct BurnDriver BurnSpecmicrsocc_48 = {
	"spec_micrsocc_48", "spec_micrsocc", "spec_spectrum", NULL, "1989",
	"Microprose Soccer (48K)\0", NULL, "Microprose Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specmicrsocc_48RomInfo, Specmicrsocc_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Microprose Soccer (128K)

static struct BurnRomInfo SpecmicrsoccRomDesc[] = {
	{ "Microprose Soccer (1989)(Microprose Software)[128K].z80", 0x09c19, 0x432ea0b4, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmicrsocc, Specmicrsocc, Spec128)
STD_ROM_FN(Specmicrsocc)

struct BurnDriver BurnSpecmicrsocc = {
	"spec_micrsocc", NULL, "spec_spec128", NULL, "1989",
	"Microprose Soccer (128K)\0", NULL, "Microprose Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmicrsoccRomInfo, SpecmicrsoccRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Midnight Resistance (48K)

static struct BurnRomInfo Specmresist_48RomDesc[] = {
	{ "Midnight Resistance (1990)(Ocean).z80", 0x0a2e3, 0x70b26b8c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmresist_48, Specmresist_48, Spectrum)
STD_ROM_FN(Specmresist_48)

struct BurnDriver BurnSpecmresist_48 = {
	"spec_mresist_48", "spec_mresist", "spec_spectrum", NULL, "1990",
	"Midnight Resistance (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specmresist_48RomInfo, Specmresist_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Midnight Resistance (128K)

static struct BurnRomInfo SpecmresistRomDesc[] = {
	{ "Midnight Resistance (1990)(Ocean)[128K].z80", 0x1b641, 0x614f8f38, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmresist, Specmresist, Spec128)
STD_ROM_FN(Specmresist)

struct BurnDriver BurnSpecmresist = {
	"spec_mresist", NULL, "spec_spec128", NULL, "1990",
	"Midnight Resistance (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmresistRomInfo, SpecmresistRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Monty on the Run (48K)

static struct BurnRomInfo SpecmontrunRomDesc[] = {
	{ "Monty on the Run (1985)(Gremlin Graphics).z80", 0x099c9, 0x8d143cdd, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmontrun, Specmontrun, Spectrum)
STD_ROM_FN(Specmontrun)

struct BurnDriver BurnSpecmontrun = {
	"spec_montrun", NULL, "spec_spectrum", NULL, "1985",
	"Monty on the Run (48K)\0", NULL, "Gremlin Graphics Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmontrunRomInfo, SpecmontrunRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Moon Cresta - Review (48K)

static struct BurnRomInfo SpecmoocrereRomDesc[] = {
	{ "Moon Cresta - Review (1985)(Incentive Software).z80", 0x0147f, 0x5db616b9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmoocrere, Specmoocrere, Spectrum)
STD_ROM_FN(Specmoocrere)

struct BurnDriver BurnSpecmoocrere = {
	"spec_moocrere", NULL, "spec_spectrum", NULL, "1985",
	"Moon Cresta - Review (48K)\0", NULL, "Incentive Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmoocrereRomInfo, SpecmoocrereRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Moon Cresta (48K)

static struct BurnRomInfo SpecmooncrstRomDesc[] = {
	{ "Moon Cresta (1985)(Incentive Software).z80", 0x0979b, 0xaf817ac8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmooncrst, Specmooncrst, Spectrum)
STD_ROM_FN(Specmooncrst)

struct BurnDriver BurnSpecmooncrst = {
	"spec_mooncrst", NULL, "spec_spectrum", NULL, "1985",
	"Moon Cresta (48K)\0", NULL, "Incentive Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmooncrstRomInfo, SpecmooncrstRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Moon Torc (48K)

static struct BurnRomInfo SpecmoontorcRomDesc[] = {
	{ "Moon Torc (1991)(Atlantis Software).z80", 0x0a150, 0x8c9406a0, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmoontorc, Specmoontorc, Spectrum)
STD_ROM_FN(Specmoontorc)

struct BurnDriver BurnSpecmoontorc = {
	"spec_moontorc", NULL, "spec_spectrum", NULL, "1991",
	"Moon Torc (48K)\0", NULL, "Atlantis Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmoontorcRomInfo, SpecmoontorcRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Moonwalker (Part 1 of 3) (48K)

static struct BurnRomInfo SpecmoonwalkRomDesc[] = {
	{ "Moonwalker (1989)(U.S. Gold)[level 1 of 3].z80", 0x09894, 0x20d806f4, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmoonwalk, Specmoonwalk, Spectrum)
STD_ROM_FN(Specmoonwalk)

struct BurnDriver BurnSpecmoonwalk = {
	"spec_moonwalk", NULL, "spec_spectrum", NULL, "1989",
	"Moonwalker (Part 1 of 3) (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmoonwalkRomInfo, SpecmoonwalkRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Moonwalker (Part 2 of 3) (48K)

static struct BurnRomInfo Specmoonwalk2RomDesc[] = {
	{ "Moonwalker (1989)(U.S. Gold)[level 2 of 3].z80", 0x08ac1, 0xd7158141, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmoonwalk2, Specmoonwalk2, Spectrum)
STD_ROM_FN(Specmoonwalk2)

struct BurnDriver BurnSpecmoonwalk2 = {
	"spec_moonwalk2", "spec_moonwalk", "spec_spectrum", NULL, "1989",
	"Moonwalker (Part 2 of 3) (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specmoonwalk2RomInfo, Specmoonwalk2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Moonwalker (Part 3 of 3) (48K)

static struct BurnRomInfo Specmoonwalk3RomDesc[] = {
	{ "Moonwalker (1989)(U.S. Gold)[level 3 of 3].z80", 0x09cff, 0xa3a6d994, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmoonwalk3, Specmoonwalk3, Spectrum)
STD_ROM_FN(Specmoonwalk3)

struct BurnDriver BurnSpecmoonwalk3 = {
	"spec_moonwalk3", "spec_moonwalk", "spec_spectrum", NULL, "1989",
	"Moonwalker (Part 3 of 3) (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specmoonwalk3RomInfo, Specmoonwalk3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Ms. Pac-Man (48K)

static struct BurnRomInfo SpecmspacmanRomDesc[] = {
	{ "Ms. Pac-Man (1984)(Atarisoft).z80", 0x08e51, 0x168677eb, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmspacman, Specmspacman, Spectrum)
STD_ROM_FN(Specmspacman)

struct BurnDriver BurnSpecmspacman = {
	"spec_mspacman", NULL, "spec_spectrum", NULL, "1984",
	"Ms. Pac-Man (48K)\0", NULL, "Atarisoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmspacmanRomInfo, SpecmspacmanRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Navy SEALs (Part 1 of 2) (128K)

static struct BurnRomInfo SpecnvysealsRomDesc[] = {
	{ "Navy SEALs (1991)(Ocean)(Part 1 of 2)[128K].z80", 0x18df5, 0x40156d33, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specnvyseals, Specnvyseals, Spec128)
STD_ROM_FN(Specnvyseals)

struct BurnDriver BurnSpecnvyseals = {
	"spec_nvyseals", NULL, "spec_spec128", NULL, "1991",
	"Navy SEALs (Part 1 of 2) (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecnvysealsRomInfo, SpecnvysealsRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Navy SEALs (Part 2 of 2) (128K)

static struct BurnRomInfo Specnvyseals2RomDesc[] = {
	{ "Navy SEALs (1991)(Ocean)(Part 2 of 2)[128K].z80", 0x1820a, 0x78ab2c33, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specnvyseals2, Specnvyseals2, Spec128)
STD_ROM_FN(Specnvyseals2)

struct BurnDriver BurnSpecnvyseals2 = {
	"spec_nvyseals2", "spec_nvyseals", "spec_spec128", NULL, "1991",
	"Navy SEALs (Part 2 of 2) (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specnvyseals2RomInfo, Specnvyseals2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Nebulus (48K)

static struct BurnRomInfo SpecnebulusRomDesc[] = {
	{ "Nebulus (1987)(Hewson Consultants).z80", 0x082f5, 0xa66c873b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specnebulus, Specnebulus, Spectrum)
STD_ROM_FN(Specnebulus)

struct BurnDriver BurnSpecnebulus = {
	"spec_nebulus", NULL, "spec_spectrum", NULL, "1987",
	"Nebulus (48K)\0", NULL, "Hewson Consultants", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecnebulusRomInfo, SpecnebulusRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Nemesis (Konami) (48K)

static struct BurnRomInfo SpecnemesisRomDesc[] = {
	{ "Nemesis (1987)(Konami).z80", 0x09e00, 0xc6c2c9c6, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specnemesis, Specnemesis, Spectrum)
STD_ROM_FN(Specnemesis)

struct BurnDriver BurnSpecnemesis = {
	"spec_nemesis", NULL, "spec_spectrum", NULL, "1987",
	"Nemesis (Konami) (48K)\0", NULL, "Konami", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecnemesisRomInfo, SpecnemesisRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Nemesis (The Hit Squad) (48K)

static struct BurnRomInfo SpecnemesishsRomDesc[] = {
	{ "Nemesis (1987)(The Hit Squad)[re-release].z80", 0x0a71d, 0xad42684d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specnemesishs, Specnemesishs, Spectrum)
STD_ROM_FN(Specnemesishs)

struct BurnDriver BurnSpecnemesishs = {
	"spec_nemesishs", "spec_nemesis", "spec_spectrum", NULL, "1987",
	"Nemesis (The Hit Squad) (48K)\0", NULL, "The Hit Squad", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecnemesishsRomInfo, SpecnemesishsRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};


// Night Breed (48K)

static struct BurnRomInfo Specnbreed_48RomDesc[] = {
	{ "Night Breed (1990)(Ocean).z80", 0x096a0, 0x521f143e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specnbreed_48, Specnbreed_48, Spectrum)
STD_ROM_FN(Specnbreed_48)

struct BurnDriver BurnSpecnbreed_48 = {
	"spec_nbreed_48", "spec_nbreed", "spec_spectrum", NULL, "1990",
	"Night Breed (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specnbreed_48RomInfo, Specnbreed_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Night Breed (128K)

static struct BurnRomInfo SpecnbreedRomDesc[] = {
	{ "Night Breed (1990)(Ocean)[128K].z80", 0x05f72, 0x95a8d043, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specnbreed, Specnbreed, Spec128)
STD_ROM_FN(Specnbreed)

struct BurnDriver BurnSpecnbreed = {
	"spec_nbreed", NULL, "spec_spec128", NULL, "1990",
	"Night Breed (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecnbreedRomInfo, SpecnbreedRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Olli & Lissa - The Ghost of Shilmoore Castle (48K)

static struct BurnRomInfo SpecollilsaRomDesc[] = {
	{ "Olli & Lissa - The Ghost Of Shilmoore Castle (1986)(Firebird).z80", 0x09f58, 0xed38d8ad, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specollilsa, Specollilsa, Spectrum)
STD_ROM_FN(Specollilsa)

struct BurnDriver BurnSpecollilsa = {
	"spec_ollilsa", NULL, "spec_spectrum", NULL, "1986",
	"Olli & Lissa - The Ghost of Shilmoore Castle (48K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecollilsaRomInfo, SpecollilsaRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Olli & Lissa II - Halloween (48K)

static struct BurnRomInfo Specollilsa2RomDesc[] = {
	{ "Olli & Lissa II - Halloween (1987)(Firebird).z80", 0x0a6b1, 0x99e59116, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specollilsa2, Specollilsa2, Spectrum)
STD_ROM_FN(Specollilsa2)

struct BurnDriver BurnSpecollilsa2 = {
	"spec_ollilsa2", NULL, "spec_spectrum", NULL, "1987",
	"Olli & Lissa II - Halloween (48K)\0", NULL, "Silverbird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specollilsa2RomInfo, Specollilsa2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Olli & Lissa III - The Candlelight Adventure (48K)

static struct BurnRomInfo Specollilsa3RomDesc[] = {
	{ "Olli & Lissa III - The Candlelight Adventure (1989)(Codemasters).z80", 0x0a356, 0x7b22d37c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specollilsa3, Specollilsa3, Spectrum)
STD_ROM_FN(Specollilsa3)

struct BurnDriver BurnSpecollilsa3 = {
	"spec_ollilsa3", NULL, "spec_spectrum", NULL, "1989",
	"Olli & Lissa III - The Candlelight Adventure (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specollilsa3RomInfo, Specollilsa3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Operation Thunderbolt (48K)

static struct BurnRomInfo Specothunder_48RomDesc[] = {
	{ "Operation Thunderbolt (1989)(Ocean).z80", 0x09cf8, 0xd1037a4d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specothunder_48, Specothunder_48, Spectrum)
STD_ROM_FN(Specothunder_48)

struct BurnDriver BurnSpecothunder_48 = {
	"spec_othunder_48", "spec_othunder", "spec_spectrum", NULL, "1989",
	"Operation Thunderbolt (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specothunder_48RomInfo, Specothunder_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Operation Thunderbolt (128K)

static struct BurnRomInfo SpecothunderRomDesc[] = {
	{ "Operation Thunderbolt (1989)(Ocean)[128K].z80", 0x1c4e4, 0xcb2c92d8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specothunder, Specothunder, Spec128)
STD_ROM_FN(Specothunder)

struct BurnDriver BurnSpecothunder = {
	"spec_othunder", NULL, "spec_spec128", NULL, "1989",
	"Operation Thunderbolt (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecothunderRomInfo, SpecothunderRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Operation Wolf (48K)

static struct BurnRomInfo Specopwolf_48RomDesc[] = {
	{ "Operation Wolf (1988)(Ocean).z80", 0x0980c, 0x75b79e29, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specopwolf_48, Specopwolf_48, Spectrum)
STD_ROM_FN(Specopwolf_48)

struct BurnDriver BurnSpecopwolf_48 = {
	"spec_opwolf_48", "spec_opwolf", "spec_spectrum", NULL, "1988",
	"Operation Wolf (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specopwolf_48RomInfo, Specopwolf_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Operation Wolf (128K)

static struct BurnRomInfo SpecopwolfRomDesc[] = {
	{ "Operation Wolf (1988)(Ocean)[128K].z80", 0x1da84, 0xa7cd683f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specopwolf, Specopwolf, Spec128)
STD_ROM_FN(Specopwolf)

struct BurnDriver BurnSpecopwolf = {
	"spec_opwolf", NULL, "spec_spec128", NULL, "1988",
	"Operation Wolf (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecopwolfRomInfo, SpecopwolfRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Out Run (128K)

static struct BurnRomInfo SpecoutrunRomDesc[] = {
	{ "Out Run (1988)(U.S. Gold)[128K].z80", 0x07bcc, 0x7cc211cf, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specoutrun, Specoutrun, Spec128)
STD_ROM_FN(Specoutrun)

struct BurnDriver BurnSpecoutrun = {
	"spec_outrun", NULL, "spec_spec128", NULL, "1988",
	"Out Run (128K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecoutrunRomInfo, SpecoutrunRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Pac-Land (48K)

static struct BurnRomInfo Specpacland_48RomDesc[] = {
	{ "Pac-Land (1989)(Grandslam Entertainments).z80", 0x0940e, 0x67075079, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpacland_48, Specpacland_48, Spectrum)
STD_ROM_FN(Specpacland_48)

struct BurnDriver BurnSpecpacland_48 = {
	"spec_pacland_48", "spec_pacland", "spec_spectrum", NULL, "1989",
	"Pac-Land (48K)\0", NULL, "Grandslam Entertainments", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specpacland_48RomInfo, Specpacland_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Pac-Land (128K)

static struct BurnRomInfo SpecpaclandRomDesc[] = {
	{ "Pac-Land (1989)(Grandslam Entertainments)[128K].z80", 0x0b9d7, 0x78fe5d59, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpacland, Specpacland, Spec128)
STD_ROM_FN(Specpacland)

struct BurnDriver BurnSpecpacland = {
	"spec_pacland", NULL, "spec_spec128", NULL, "1989",
	"Pac-Land (128K)\0", NULL, "Grandslam Entertainments", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpaclandRomInfo, SpecpaclandRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Pac-Man (Atarisoft) (48K)

static struct BurnRomInfo SpecpacmanRomDesc[] = {
	{ "Pac-Man (1983)(Atarisoft).z80", 0x035fc, 0xe3c56f6b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpacman, Specpacman, Spectrum)
STD_ROM_FN(Specpacman)

struct BurnDriver BurnSpecpacman = {
	"spec_pacman", NULL, "spec_spectrum", NULL, "1983",
	"Pac-Man (Atarisoft) (48K)\0", NULL, "Atarisoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpacmanRomInfo, SpecpacmanRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};
// Pac-Mania (48K)

static struct BurnRomInfo Specpacmania_48RomDesc[] = {
	{ "Pac-Mania (1988)(Grandslam Entertainments).z80", 0x07cb3, 0xb7706dc6, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpacmania_48, Specpacmania_48, Spectrum)
STD_ROM_FN(Specpacmania_48)

struct BurnDriver BurnSpecpacmania_48 = {
	"spec_pacmania_48", "spec_pacmania", "spec_spectrum", NULL, "1988",
	"Pac-Mania (48K)\0", NULL, "Grandslam Entertainments", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specpacmania_48RomInfo, Specpacmania_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Pac-Mania (128K)

static struct BurnRomInfo SpecpacmaniaRomDesc[] = {
	{ "Pac-Mania (1988)(Grandslam Entertainments)[128K].z80", 0x0a813, 0x938c60ff, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpacmania, Specpacmania, Spec128)
STD_ROM_FN(Specpacmania)

struct BurnDriver BurnSpecpacmania = {
	"spec_pacmania", NULL, "spec_spec128", NULL, "1988",
	"Pac-Mania (128K)\0", NULL, "Grandslam Entertainments", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpacmaniaRomInfo, SpecpacmaniaRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Pang (128K)

static struct BurnRomInfo SpecpangRomDesc[] = {
	{ "Pang (1990)(Ocean)[128K][incomplete].z80", 0x12130, 0xc7016b68, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpang, Specpang, Spec128)
STD_ROM_FN(Specpang)

struct BurnDriver BurnSpecpang = {
	"spec_pang", NULL, "spec_spec128", NULL, "1990",
	"Pang (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpangRomInfo, SpecpangRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Panic Dizzy (48K)

static struct BurnRomInfo Specpandizzy_48RomDesc[] = {
	{ "Panic Dizzy (1991)(Codemasters).z80", 0x094f5, 0xce5c5125, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpandizzy_48, Specpandizzy_48, Spectrum)
STD_ROM_FN(Specpandizzy_48)

struct BurnDriver BurnSpecpandizzy_48 = {
	"spec_pandizzy_48", "spec_pandizzy", "spec_spectrum", NULL, "1991",
	"Panic Dizzy (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specpandizzy_48RomInfo, Specpandizzy_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Panic Dizzy (128K)

static struct BurnRomInfo SpecpandizzyRomDesc[] = {
	{ "Panic Dizzy (1991)(Codemasters)[128K].z80", 0x0b637, 0x4b087391, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpandizzy, Specpandizzy, Spec128)
STD_ROM_FN(Specpandizzy)

struct BurnDriver BurnSpecpandizzy = {
	"spec_pandizzy", NULL, "spec_spec128", NULL, "1991",
	"Panic Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpandizzyRomInfo, SpecpandizzyRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Paperboy (48K)

static struct BurnRomInfo SpecpaperboyRomDesc[] = {
	{ "Paperboy (1986)(Elite Systems).z80", 0x0a0ba, 0xa1465284, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpaperboy, Specpaperboy, Spectrum)
STD_ROM_FN(Specpaperboy)

struct BurnDriver BurnSpecpaperboy = {
	"spec_paperboy", NULL, "spec_spectrum", NULL, "1986",
	"Paperboy (48K)\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpaperboyRomInfo, SpecpaperboyRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Paperboy 2 (48K)

static struct BurnRomInfo Specpaperby2RomDesc[] = {
	{ "Paperboy 2 (1992)(Mindscape International).z80", 0x09bbe, 0x523b2b3b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpaperby2, Specpaperby2, Spectrum)
STD_ROM_FN(Specpaperby2)

struct BurnDriver BurnSpecpaperby2 = {
	"spec_paperby2", NULL, "spec_spectrum", NULL, "1992",
	"Paperboy 2 (48K)\0", NULL, "Mindscape International", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specpaperby2RomInfo, Specpaperby2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Passing Shot (48K)

static struct BurnRomInfo Specpassshot_48RomDesc[] = {
	{ "Passing Shot (1989)(Image Works).z80", 0x07b0f, 0x183331fc, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpassshot_48, Specpassshot_48, Spectrum)
STD_ROM_FN(Specpassshot_48)

struct BurnDriver BurnSpecpassshot_48 = {
	"spec_passshot_48", "spec_passshot", "spec_spectrum", NULL, "1989",
	"Passing Shot (48K)\0", NULL, "Image Works", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specpassshot_48RomInfo, Specpassshot_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Passing Shot (128K)

static struct BurnRomInfo SpecpassshotRomDesc[] = {
	{ "Passing Shot (1989)(Image Works)[128K].z80", 0x098ac, 0xf34e8359, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpassshot, Specpassshot, Spec128)
STD_ROM_FN(Specpassshot)

struct BurnDriver BurnSpecpassshot = {
	"spec_passshot", NULL, "spec_spec128", NULL, "1989",
	"Passing Shot (128K)\0", NULL, "Image Works", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpassshotRomInfo, SpecpassshotRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Peter Shilton's Handball Maradona (48K)

static struct BurnRomInfo SpecpshandmRomDesc[] = {
	{ "Peter Shilton's Handball Maradona (1986)(Grandslam Entertainments).z80", 0x09dff, 0x5d4a8e4d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpshandm, Specpshandm, Spectrum)
STD_ROM_FN(Specpshandm)

struct BurnDriver BurnSpecpshandm = {
	"spec_pshandm", NULL, "spec_spectrum", NULL, "1986",
	"Peter Shilton's Handball Maradona (48K)\0", NULL, "Grandslam Entertainments", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpshandmRomInfo, SpecpshandmRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Phoenix (48K)

static struct BurnRomInfo SpecphoenixRomDesc[] = {
	{ "Phoenix (1991)(Zenobi Software).z80", 0x092d3, 0xb2218446, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specphoenix, Specphoenix, Spectrum)
STD_ROM_FN(Specphoenix)

struct BurnDriver BurnSpecphoenix = {
	"spec_phoenix", NULL, "spec_spectrum", NULL, "1991",
	"Phoenix (48K)\0", NULL, "Zenobi Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecphoenixRomInfo, SpecphoenixRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Pippo (48K)

static struct BurnRomInfo SpecpippoRomDesc[] = {
	{ "Pippo (1986)(Mastertronic).z80", 0x0b03e, 0xf63e41be, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpippo, Specpippo, Spectrum)
STD_ROM_FN(Specpippo)

struct BurnDriver BurnSpecpippo = {
	"spec_pippo", NULL, "spec_spectrum", NULL, "1986",
	"Pippo (48K)\0", NULL, "Mastertronic", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpippoRomInfo, SpecpippoRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Platoon (48K)

static struct BurnRomInfo Specplatoon_48RomDesc[] = {
	{ "Platoon (1988)(Ocean).z80", 0x09ae3, 0x06b39aa4, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specplatoon_48, Specplatoon_48, Spectrum)
STD_ROM_FN(Specplatoon_48)

struct BurnDriver BurnSpecplatoon_48 = {
	"spec_platoon_48", "spec_platoon", "spec_spectrum", NULL, "1988",
	"Platoon (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specplatoon_48RomInfo, Specplatoon_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Platoon (128K)

static struct BurnRomInfo SpecplatoonRomDesc[] = {
	{ "Platoon (1988)(Ocean)[128K].z80", 0x1a121, 0xaa4d4d13, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specplatoon, Specplatoon, Spec128)
STD_ROM_FN(Specplatoon)

struct BurnDriver BurnSpecplatoon = {
	"spec_platoon", NULL, "spec_spec128", NULL, "1988",
	"Platoon (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecplatoonRomInfo, SpecplatoonRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Plotting (48K)

static struct BurnRomInfo Specplotting_48RomDesc[] = {
	{ "Plotting (1990)(Ocean).z80", 0x09950, 0xce3f07c5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specplotting_48, Specplotting_48, Spectrum)
STD_ROM_FN(Specplotting_48)

struct BurnDriver BurnSpecplotting_48 = {
	"spec_plotting_48", "spec_plotting", "spec_spectrum", NULL, "1990",
	"Plotting (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specplotting_48RomInfo, Specplotting_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Plotting (128K)

static struct BurnRomInfo SpecplottingRomDesc[] = {
	{ "Plotting (1990)(Ocean)[128K].z80", 0x09fb3, 0xdd65a0b5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specplotting, Specplotting, Spec128)
STD_ROM_FN(Specplotting)

struct BurnDriver BurnSpecplotting = {
	"spec_plotting", NULL, "spec_spec128", NULL, "1990",
	"Plotting (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecplottingRomInfo, SpecplottingRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Pole Position (Atarisoft) (48K)

static struct BurnRomInfo SpecpoleposaRomDesc[] = {
	{ "Pole Position (1984)(Atarisoft).z80", 0x0850b, 0x620ff870, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpoleposa, Specpoleposa, Spectrum)
STD_ROM_FN(Specpoleposa)

struct BurnDriver BurnSpecpoleposa = {
	"spec_poleposa", NULL, "spec_spectrum", NULL, "1984",
	"Pole Position (Atarisoft) (48K)\0", NULL, "Atarisoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpoleposaRomInfo, SpecpoleposaRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Pole Position (U.S. Gold) (48K)

static struct BurnRomInfo SpecpoleposuRomDesc[] = {
	{ "Pole Position (1984)(U.S. Gold).z80", 0x08241, 0x979c15f6, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpoleposu, Specpoleposu, Spectrum)
STD_ROM_FN(Specpoleposu)

struct BurnDriver BurnSpecpoleposu = {
	"spec_poleposu", NULL, "spec_spectrum", NULL, "1984",
	"Pole Position (U.S. Gold) (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpoleposuRomInfo, SpecpoleposuRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Power Drift (128K)

static struct BurnRomInfo SpecpdriftRomDesc[] = {
	{ "Power Drift (1989)(Activision)[128K].z80", 0x160f6, 0x34a7f74a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpdrift, Specpdrift, Spec128)
STD_ROM_FN(Specpdrift)

struct BurnDriver BurnSpecpdrift = {
	"spec_pdrift", NULL, "spec_spec128", NULL, "1989",
	"Power Drift (128K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpdriftRomInfo, SpecpdriftRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Puzznic (48K)

static struct BurnRomInfo Specpuzznic_48RomDesc[] = {
	{ "Puzznic (1990)(Ocean).z80", 0x06bc9, 0x1b6a8858, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpuzznic_48, Specpuzznic_48, Spectrum)
STD_ROM_FN(Specpuzznic_48)

struct BurnDriver BurnSpecpuzznic_48 = {
	"spec_puzznic_48", "spec_puzznic", "spec_spectrum", NULL, "1990",
	"Puzznic (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specpuzznic_48RomInfo, Specpuzznic_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Puzznic (128K)

static struct BurnRomInfo SpecpuzznicRomDesc[] = {
	{ "Puzznic (1990)(Ocean)[128K].z80", 0x11c44, 0x138fe09d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specpuzznic, Specpuzznic, Spec128)
STD_ROM_FN(Specpuzznic)

struct BurnDriver BurnSpecpuzznic = {
	"spec_puzznic", NULL, "spec_spec128", NULL, "1990",
	"Puzznic (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecpuzznicRomInfo, SpecpuzznicRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Quartet (48K)

static struct BurnRomInfo SpecquartetRomDesc[] = {
	{ "Quartet (1987)(Activision).z80", 0x08a9e, 0x45711e73, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specquartet, Specquartet, Spectrum)
STD_ROM_FN(Specquartet)

struct BurnDriver BurnSpecquartet = {
	"spec_quartet", NULL, "spec_spectrum", NULL, "1987",
	"Quartet (48K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecquartetRomInfo, SpecquartetRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Quazatron (48K)

static struct BurnRomInfo SpecquazatroRomDesc[] = {
	{ "Quazatron (1986)(Hewson Consultants).z80", 0x07e39, 0xdf931658, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specquazatro, Specquazatro, Spectrum)
STD_ROM_FN(Specquazatro)

struct BurnDriver BurnSpecquazatro = {
	"spec_quazatro", NULL, "spec_spectrum", NULL, "1986",
	"Quazatron (48K)\0", NULL, "Hewson Consultants", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecquazatroRomInfo, SpecquazatroRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rainbow Islands - The Story of Bubble Bobble 2 (48K)

static struct BurnRomInfo Specrbisland_48RomDesc[] = {
	{ "Rainbow Islands - The Story of Bubble Bobble 2 (1990)(Ocean).z80", 0x07da0, 0xad2c841f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrbisland_48, Specrbisland_48, Spectrum)
STD_ROM_FN(Specrbisland_48)

struct BurnDriver BurnSpecrbisland_48 = {
	"spec_rbisland_48", "spec_rbisland", "spec_spectrum", NULL, "1990",
	"Rainbow Islands - The Story of Bubble Bobble 2 (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrbisland_48RomInfo, Specrbisland_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rainbow Islands - The Story of Bubble Bobble 2 (128K)

static struct BurnRomInfo SpecrbislandRomDesc[] = {
	{ "Rainbow Islands - The Story of Bubble Bobble 2 (1990)(Ocean)[128K].z80", 0x161bb, 0x0211cd1d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrbisland, Specrbisland, Spec128)
STD_ROM_FN(Specrbisland)

struct BurnDriver BurnSpecrbisland = {
	"spec_rbisland", NULL, "spec_spec128", NULL, "1990",
	"Rainbow Islands - The Story of Bubble Bobble 2 (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrbislandRomInfo, SpecrbislandRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rambo (48K)

static struct BurnRomInfo SpecramboRomDesc[] = {
	{ "Rambo (1985)(Ocean).z80", 0x09bd0, 0x655b6fa1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrambo, Specrambo, Spectrum)
STD_ROM_FN(Specrambo)

struct BurnDriver BurnSpecrambo = {
	"spec_rambo", NULL, "spec_spectrum", NULL, "1985",
	"Rambo (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecramboRomInfo, SpecramboRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rambo III (128K)

static struct BurnRomInfo Specrambo3RomDesc[] = {
	{ "Rambo III (1988)(Ocean)[128K].z80", 0x1b487, 0xf2b6d24f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrambo3, Specrambo3, Spec128)
STD_ROM_FN(Specrambo3)

struct BurnDriver BurnSpecrambo3 = {
	"spec_rambo3", NULL, "spec_spec128", NULL, "1988",
	"Rambo III (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrambo3RomInfo, Specrambo3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rampage (48K)

static struct BurnRomInfo SpecrampageRomDesc[] = {
	{ "Rampage (1988)(Activision).z80", 0x094eb, 0x3735beaf, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrampage, Specrampage, Spectrum)
STD_ROM_FN(Specrampage)

struct BurnDriver BurnSpecrampage = {
	"spec_rampage", NULL, "spec_spectrum", NULL, "1988",
	"Rampage (48K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrampageRomInfo, SpecrampageRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rastan (128K)

static struct BurnRomInfo SpecrastanRomDesc[] = {
	{ "Rastan (1988)(Imagine Software)[128K].z80", 0x1441f, 0x0440b5e8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrastan, Specrastan, Spec128)
STD_ROM_FN(Specrastan)

struct BurnDriver BurnSpecrastan = {
	"spec_rastan", NULL, "spec_spec128", NULL, "1988",
	"Rastan (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrastanRomInfo, SpecrastanRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Renegade (48K)

static struct BurnRomInfo Specrenegade_48RomDesc[] = {
	{ "Renegade (1987)(Imagine Software).z80", 0x0a2d7, 0x9faf0d9e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrenegade_48, Specrenegade_48, Spectrum)
STD_ROM_FN(Specrenegade_48)

struct BurnDriver BurnSpecrenegade_48 = {
	"spec_renegade_48", "spec_renegade", "spec_spectrum", NULL, "1987",
	"Renegade (48K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrenegade_48RomInfo, Specrenegade_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Renegade (128K)

static struct BurnRomInfo SpecrenegadeRomDesc[] = {
	{ "Renegade (1987)(Imagine Software)[128K].z80", 0x16f0d, 0xcd930d9a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrenegade, Specrenegade, Spec128)
STD_ROM_FN(Specrenegade)

struct BurnDriver BurnSpecrenegade = {
	"spec_renegade", NULL, "spec_spec128", NULL, "1987",
	"Renegade (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrenegadeRomInfo, SpecrenegadeRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Renegade II - Target Renegade (128K)

static struct BurnRomInfo Specrenegad2RomDesc[] = {
	{ "Renegade II - Target Renegade (1988)(Imagine Software)[128K].z80", 0x1a950, 0x25d57e2c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrenegad2, Specrenegad2, Spec128)
STD_ROM_FN(Specrenegad2)

struct BurnDriver BurnSpecrenegad2 = {
	"spec_renegad2", NULL, "spec_spec128", NULL, "1988",
	"Renegade II - Target Renegade (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrenegad2RomInfo, Specrenegad2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Renegade III - The Final Chapter (128K)

static struct BurnRomInfo Specrenegad3RomDesc[] = {
	{ "Renegade III - The Final Chapter (1989)(Imagine Software)[128K].z80", 0x18519, 0x45f783f9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrenegad3, Specrenegad3, Spec128)
STD_ROM_FN(Specrenegad3)

struct BurnDriver BurnSpecrenegad3 = {
	"spec_renegad3", NULL, "spec_spec128", NULL, "1989",
	"Renegade III - The Final Chapter (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrenegad3RomInfo, Specrenegad3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rex (Part 1 of 2) (48K)

static struct BurnRomInfo Specrex_48RomDesc[] = {
	{ "Rex (1988)(Martech Games)[Part 1 of 2].z80", 0x0aab7, 0xecb7d3dc, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrex_48, Specrex_48, Spectrum)
STD_ROM_FN(Specrex_48)

struct BurnDriver BurnSpecrex_48 = {
	"spec_rex_48", "spec_rex", "spec_spectrum", NULL, "1988",
	"Rex (Part 1 of 2) (48K)\0", NULL, "Martech Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrex_48RomInfo, Specrex_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rex (Part 2 of 2) (48K)

static struct BurnRomInfo Specrex2_48RomDesc[] = {
	{ "Rex (1988)(Martech Games)[Part 2 of 2].z80", 0x09f3a, 0x51130dc0, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrex2_48, Specrex2_48, Spectrum)
STD_ROM_FN(Specrex2_48)

struct BurnDriver BurnSpecrex2_48 = {
	"spec_rex2_48", "spec_rex", "spec_spectrum", NULL, "1988",
	"Rex (Part 2 of 2) (48K)\0", NULL, "Martech Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrex2_48RomInfo, Specrex2_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rex (128K)

static struct BurnRomInfo SpecrexRomDesc[] = {
	{ "Rex (1988)(Martech Games)(128k).z80", 0x0d833, 0xf5fc8541, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrex, Specrex, Spectrum)
STD_ROM_FN(Specrex)

struct BurnDriver BurnSpecrex = {
	"spec_rex", NULL, "spec_spec128", NULL, "1988",
	"Rex (128K)\0", NULL, "Martech Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrexRomInfo, SpecrexRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rick Dangerous (48K)

static struct BurnRomInfo SpecrickdangRomDesc[] = {
	{ "Rick Dangerous (1989)(Firebird Software).z80", 0x09cb3, 0x556a8928, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrickdang, Specrickdang, Spectrum)
STD_ROM_FN(Specrickdang)

struct BurnDriver BurnSpecrickdang = {
	"spec_rickdang", NULL, "spec_spectrum", NULL, "1989",
	"Rick Dangerous (48K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrickdangRomInfo, SpecrickdangRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Robocop (128K)

static struct BurnRomInfo SpecrobocopRomDesc[] = {
	{ "Robocop (1988)(Ocean)[128K].z80", 0x1cbf8, 0xdcc4bf16, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrobocop, Specrobocop, Spec128)
STD_ROM_FN(Specrobocop)

struct BurnDriver BurnSpecrobocop = {
	"spec_robocop", NULL, "spec_spec128", NULL, "1988",
	"Robocop (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrobocopRomInfo, SpecrobocopRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Robocop 2 (128K)

static struct BurnRomInfo Specrobocop2RomDesc[] = {
	{ "Robocop 2 (1990)(Ocean)[128K].z80", 0x1c73e, 0xe9b44bc7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrobocop2, Specrobocop2, Spec128)
STD_ROM_FN(Specrobocop2)

struct BurnDriver BurnSpecrobocop2 = {
	"spec_robocop2", NULL, "spec_spec128", NULL, "1990",
	"Robocop 2 (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrobocop2RomInfo, Specrobocop2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Robocop 3 (128K)

static struct BurnRomInfo Specrobocop3RomDesc[] = {
	{ "Robocop 3 (1992)(Ocean)[128K].z80", 0x1ac8a, 0x21b5c6b7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrobocop3, Specrobocop3, Spec128)
STD_ROM_FN(Specrobocop3)

struct BurnDriver BurnSpecrobocop3 = {
	"spec_robocop3", NULL, "spec_spec128", NULL, "1992",
	"Robocop 3 (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrobocop3RomInfo, Specrobocop3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rock Star Ate my Hamster (128K)

static struct BurnRomInfo SpecrockshamRomDesc[] = {
	{ "Rock Star Ate my Hamster (1989)(Codemasters)[t][128K].z80", 0x0ffbd, 0xcf617748, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrocksham, Specrocksham, Spec128)
STD_ROM_FN(Specrocksham)

struct BurnDriver BurnSpecrocksham = {
	"spec_rocksham", NULL, "spec_spec128", NULL, "1989",
	"Rock Star Ate my Hamster (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrockshamRomInfo, SpecrockshamRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rolling Thunder (48K)

static struct BurnRomInfo SpecrollthunRomDesc[] = {
	{ "Rolling Thunder (1988)(U.S. Gold).z80", 0x09e57, 0x41bc00e3, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrollthun, Specrollthun, Spectrum)
STD_ROM_FN(Specrollthun)

struct BurnDriver BurnSpecrollthun = {
	"spec_rollthun", NULL, "spec_spectrum", NULL, "1988",
	"Rolling Thunder (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrollthunRomInfo, SpecrollthunRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// R-Type (48K)

static struct BurnRomInfo SpecrtypeRomDesc[] = {
	{ "R-Type (1988)(Electric Dreams Software)[b].z80", 0x07cc1, 0x7db5c98f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrtype, Specrtype, Spectrum)
STD_ROM_FN(Specrtype)

struct BurnDriver BurnSpecrtype = {
	"spec_rtype", NULL, "spec_spectrum", NULL, "1988",
	"R-Type (48K)\0", NULL, "Electric Dreams Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrtypeRomInfo, SpecrtypeRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Run the Gauntlet (128K)

static struct BurnRomInfo SpecrungauntRomDesc[] = {
	{ "Run the Gauntlet (1989)(Ocean)[128K].z80", 0x18211, 0xc3aef7d9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrungaunt, Specrungaunt, Spec128)
STD_ROM_FN(Specrungaunt)

struct BurnDriver BurnSpecrungaunt = {
	"spec_rungaunt", NULL, "spec_spec128", NULL, "1989",
	"Run the Gauntlet (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrungauntRomInfo, SpecrungauntRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rygar (48K)

static struct BurnRomInfo Specrygar_48RomDesc[] = {
	{ "Rygar (1987)(U.S. Gold).z80", 0x0713e, 0xaaef2c33, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrygar_48, Specrygar_48, Spectrum)
STD_ROM_FN(Specrygar_48)

struct BurnDriver BurnSpecrygar_48 = {
	"spec_rygar_48", "spec_rygar", "spec_spectrum", NULL, "1987",
	"Rygar (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrygar_48RomInfo, Specrygar_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rygar (128K)

static struct BurnRomInfo SpecrygarRomDesc[] = {
	{ "Rygar (1987)(U.S. Gold)[128K].z80", 0x076cc, 0x260afb11, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrygar, Specrygar, Spec128)
STD_ROM_FN(Specrygar)

struct BurnDriver BurnSpecrygar = {
	"spec_rygar", NULL, "spec_spec128", NULL, "1987",
	"Rygar (128K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrygarRomInfo, SpecrygarRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// S.D.I. - Strategic Defence Initiative (48K)

static struct BurnRomInfo SpecsdisdiRomDesc[] = {
	{ "S.D.I. - Strategic Defence Initiative (1988)(Activision).z80", 0x08bad, 0xe0faaf57, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsdisdi, Specsdisdi, Spectrum)
STD_ROM_FN(Specsdisdi)

struct BurnDriver BurnSpecsdisdi = {
	"spec_sdisdi", NULL, "spec_spectrum", NULL, "1988",
	"S.D.I. - Strategic Defence Initiative (48K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsdisdiRomInfo, SpecsdisdiRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Saboteur (Durell Software) (48K)

static struct BurnRomInfo Specsabot_48RomDesc[] = {
	{ "Saboteur (1986)(Durell).z80", 0x095bb, 0xd08cf864, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsabot_48, Specsabot_48, Spectrum)
STD_ROM_FN(Specsabot_48)

struct BurnDriver BurnSpecsabot_48 = {
	"spec_sabot_48", NULL, "spec_spectrum", NULL, "1986",
	"Saboteur (Durell Software) (48K)\0", NULL, "Durell Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specsabot_48RomInfo, Specsabot_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Saboteur (Durell Software) (128K)

static struct BurnRomInfo SpecsaboteurRomDesc[] = {
	{ "Saboteur (1986)(Durell)(128k).z80", 0x0a8e5, 0x2908e774, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsaboteur, Specsaboteur, Spectrum)
STD_ROM_FN(Specsaboteur)

struct BurnDriver BurnSpecsaboteur = {
	"spec_saboteur", NULL, "spec_spec128", NULL, "1987",
	"Saboteur (Durell Software) (128K)\0", NULL, "Durell Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsaboteurRomInfo, SpecsaboteurRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Saboteur II - Avenging Angel (48K)

static struct BurnRomInfo Specsabotur2_48RomDesc[] = {
	{ "Saboteur II - Avenging Angel (1987)(Durell).z80", 0x0a996, 0x4f0d8f73, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsabotur2_48, Specsabotur2_48, Spectrum)
STD_ROM_FN(Specsabotur2_48)

struct BurnDriver BurnSpecsabotur2_48 = {
	"spec_sabotur2_48", "spec_sabotur2", "spec_spectrum", NULL, "1987",
	"Saboteur II - Avenging Angel (48K)\0", NULL, "Durell Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specsabotur2_48RomInfo, Specsabotur2_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Saboteur II - Avenging Angel (128K)

static struct BurnRomInfo Specsabotur2RomDesc[] = {
	{ "Saboteur II - Avenging Angel (1987)(Durell)(128k).z80", 0x0b790, 0x7aad77db, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsabotur2, Specsabotur2, Spectrum)
STD_ROM_FN(Specsabotur2)

struct BurnDriver BurnSpecsabotur2 = {
	"spec_sabotur2", NULL, "spec_spec128", NULL, "1987",
	"Saboteur II - Avenging Angel (128K)\0", NULL, "Durell Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specsabotur2RomInfo, Specsabotur2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Salamander (48K)

static struct BurnRomInfo SpecsalamandRomDesc[] = {
	{ "Salamander (1987)(Imagine Software).z80", 0x0a680, 0x5ae35d91, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsalamand, Specsalamand, Spectrum)
STD_ROM_FN(Specsalamand)

struct BurnDriver BurnSpecsalamand = {
	"spec_salamand", NULL, "spec_spectrum", NULL, "1987",
	"Salamander (48K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsalamandRomInfo, SpecsalamandRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Savage (Part 1 of 3) (48K)

static struct BurnRomInfo Specsavage1RomDesc[] = {
	{ "Savage, The (1988)(Firebird)[Part 1 of 3].z80", 0x0921c, 0xf8071892, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsavage1, Specsavage1, Spectrum)
STD_ROM_FN(Specsavage1)

struct BurnDriver BurnSpecsavage1 = {
	"spec_savage1", "spec_savage", "spec_spectrum", NULL, "1988",
	"Savage (Part 1 of 3) (48K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specsavage1RomInfo, Specsavage1RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Savage (Part 2 of 3) (48K)

static struct BurnRomInfo Specsavage2RomDesc[] = {
	{ "Savage, The (1988)(Firebird)[Part 2 of 3].z80", 0x09a20, 0x4f8ddec1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsavage2, Specsavage2, Spectrum)
STD_ROM_FN(Specsavage2)

struct BurnDriver BurnSpecsavage2 = {
	"spec_savage2", "spec_savage", "spec_spectrum", NULL, "1988",
	"Savage (Part 2 of 3) (48K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specsavage2RomInfo, Specsavage2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Savage (Part 3 of 3) (48K)

static struct BurnRomInfo Specsavage3RomDesc[] = {
	{ "Savage, The (1988)(Firebird)[Part 3 of 3].z80", 0x08475, 0xe994f627, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsavage3, Specsavage3, Spectrum)
STD_ROM_FN(Specsavage3)

struct BurnDriver BurnSpecsavage3 = {
	"spec_savage3", "spec_savage", "spec_spectrum", NULL, "1988",
	"Savage (Part 3 of 3) (48K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specsavage3RomInfo, Specsavage3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Savage (128K)

static struct BurnRomInfo SpecsavageRomDesc[] = {
	{ "Savage (1988)(Firebird)(128k).z80", 0x091a9, 0x09aef12f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsavage, Specsavage, Spectrum)
STD_ROM_FN(Specsavage)

struct BurnDriver BurnSpecsavage = {
	"spec_savage", NULL, "spec_spec128", NULL, "1988",
	"Savage (128K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsavageRomInfo, SpecsavageRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Scramble Spirits (48K)

static struct BurnRomInfo Specscrspirt_48RomDesc[] = {
	{ "Scramble Spirits (1990)(Grandslam Entertainments).z80", 0x092c7, 0xf292ffc7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specscrspirt_48, Specscrspirt_48, Spectrum)
STD_ROM_FN(Specscrspirt_48)

struct BurnDriver BurnSpecscrspirt_48 = {
	"spec_scrspirt_48", "spec_scrspirt", "spec_spectrum", NULL, "1990",
	"Scramble Spirits (48K)\0", NULL, "Grandslam Entertainments", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specscrspirt_48RomInfo, Specscrspirt_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Scramble Spirits (128K)

static struct BurnRomInfo SpecscrspirtRomDesc[] = {
	{ "Scramble Spirits (1990)(Grandslam Entertainments)[128K].z80", 0x14309, 0xf293694e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specscrspirt, Specscrspirt, Spec128)
STD_ROM_FN(Specscrspirt)

struct BurnDriver BurnSpecscrspirt = {
	"spec_scrspirt", NULL, "spec_spec128", NULL, "1990",
	"Scramble Spirits (128K)\0", NULL, "Grandslam Entertainments", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecscrspirtRomInfo, SpecscrspirtRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Seymour at the Movies (48K)

static struct BurnRomInfo SpecseymmoviRomDesc[] = {
	{ "Seymour At The Movies (1991)(Codemasters).z80", 0x0b3c7, 0xc91a5fa5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specseymmovi, Specseymmovi, Spectrum)
STD_ROM_FN(Specseymmovi)

struct BurnDriver BurnSpecseymmovi = {
	"spec_seymmovi", NULL, "spec_spectrum", NULL, "1991",
	"Seymour at the Movies (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecseymmoviRomInfo, SpecseymmoviRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Seymour Take One (48K)

static struct BurnRomInfo SpecseytakonRomDesc[] = {
	{ "Seymour Take One (1991)(Codemasters).z80", 0x0858a, 0x21645f38, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specseytakon, Specseytakon, Spectrum)
STD_ROM_FN(Specseytakon)

struct BurnDriver BurnSpecseytakon = {
	"spec_seytakon", NULL, "spec_spectrum", NULL, "1991",
	"Seymour Take One (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecseytakonRomInfo, SpecseytakonRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Shadow Warriors (128K)

static struct BurnRomInfo SpecshadwarrRomDesc[] = {
	{ "Shadow Warriors (1990)(Ocean)[128K].z80", 0x1c463, 0x8a034e94, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specshadwarr, Specshadwarr, Spec128)
STD_ROM_FN(Specshadwarr)

struct BurnDriver BurnSpecshadwarr = {
	"spec_shadwarr", NULL, "spec_spec128", NULL, "1990",
	"Shadow Warriors (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecshadwarrRomInfo, SpecshadwarrRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Shinobi (128K)

static struct BurnRomInfo SpecshinobiRomDesc[] = {
	{ "Shinobi (1989)(Virgin Games)[128K].z80", 0x0d8b0, 0x3ca7a9e7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specshinobi, Specshinobi, Spec128)
STD_ROM_FN(Specshinobi)

struct BurnDriver BurnSpecshinobi = {
	"spec_shinobi", NULL, "spec_spec128", NULL, "1989",
	"Shinobi (128K)\0", NULL, "Virgin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecshinobiRomInfo, SpecshinobiRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Silkworm (128K)

static struct BurnRomInfo SpecsilkwormRomDesc[] = {
	{ "Silkworm (1989)(Virgin Games)[128K].z80", 0x174d9, 0xeb973e1c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsilkworm, Specsilkworm, Spec128)
STD_ROM_FN(Specsilkworm)

struct BurnDriver BurnSpecsilkworm = {
	"spec_silkworm", NULL, "spec_spec128", NULL, "1989",
	"Silkworm (128K)\0", NULL, "Virgin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsilkwormRomInfo, SpecsilkwormRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Sim City (48K)

static struct BurnRomInfo SpecsimcityRomDesc[] = {
	{ "Sim City (1989)(Infogrames).z80", 0x066f0, 0x83ec2144, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsimcity, Specsimcity, Spectrum)
STD_ROM_FN(Specsimcity)

struct BurnDriver BurnSpecsimcity = {
	"spec_simcity", NULL, "spec_spectrum", NULL, "1989",
	"Sim City (48K)\0", NULL, "Infogrames", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsimcityRomInfo, SpecsimcityRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Skool Daze (48K)

static struct BurnRomInfo SpecskoldazeRomDesc[] = {
	{ "Skool Daze (1985)(Microsphere).z80", 0x0a5f5, 0x4034c78b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specskoldaze, Specskoldaze, Spectrum)
STD_ROM_FN(Specskoldaze)

struct BurnDriver BurnSpecskoldaze = {
	"spec_skoldaze", NULL, "spec_spectrum", NULL, "1985",
	"Skool Daze (48K)\0", NULL, "Microsphere", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecskoldazeRomInfo, SpecskoldazeRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Slap Fight (48K)

static struct BurnRomInfo Specslapfigh_48RomDesc[] = {
	{ "Slap Fight (1987)(Imagine Software).z80", 0x09dca, 0xfc50dded, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specslapfigh_48, Specslapfigh_48, Spectrum)
STD_ROM_FN(Specslapfigh_48)

struct BurnDriver BurnSpecslapfigh_48 = {
	"spec_slapfigh_48", "spec_slapfigh", "spec_spectrum", NULL, "1987",
	"Slap Fight (48K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specslapfigh_48RomInfo, Specslapfigh_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Slap Fight (128K)

static struct BurnRomInfo SpecslapfighRomDesc[] = {
	{ "Slap Fight (1987)(Imagine Software)[128K].z80", 0x0a08e, 0x4b9c236b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specslapfigh, Specslapfigh, Spec128)
STD_ROM_FN(Specslapfigh)

struct BurnDriver BurnSpecslapfigh = {
	"spec_slapfigh", NULL, "spec_spec128", NULL, "1987",
	"Slap Fight (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecslapfighRomInfo, SpecslapfighRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Slightly Magic (48K)

static struct BurnRomInfo Specslightmg_48RomDesc[] = {
	{ "Slightly Magic (1990)(Codemasters).z80", 0x0a76b, 0xd89a3a98, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specslightmg_48, Specslightmg_48, Spectrum)
STD_ROM_FN(Specslightmg_48)

struct BurnDriver BurnSpecslightmg_48 = {
	"spec_slightmg_48", "spec_slightmg", "spec_spectrum", NULL, "1990",
	"Slightly Magic (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specslightmg_48RomInfo, Specslightmg_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Slightly Magic (128K)

static struct BurnRomInfo SpecslightmgRomDesc[] = {
	{ "Slightly Magic (1990)(Codemasters)[128K].z80", 0x11bed, 0x12aaa197, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specslightmg, Specslightmg, Spec128)
STD_ROM_FN(Specslightmg)

struct BurnDriver BurnSpecslightmg = {
	"spec_slightmg", NULL, "spec_spec128", NULL, "1990",
	"Slightly Magic (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecslightmgRomInfo, SpecslightmgRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Smash TV (128K)

static struct BurnRomInfo SpecsmashtvRomDesc[] = {
	{ "Smash TV (1991)(Ocean)[128K].z80", 0x0e6c1, 0x2f90973d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsmashtv, Specsmashtv, Spec128)
STD_ROM_FN(Specsmashtv)

struct BurnDriver BurnSpecsmashtv = {
	"spec_smashtv", NULL, "spec_spec128", NULL, "1991",
	"Smash TV (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsmashtvRomInfo, SpecsmashtvRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Soldier of Fortune (48K)

static struct BurnRomInfo SpecsoldfortRomDesc[] = {
	{ "Soldier Of Fortune (1988)(Firebird).z80", 0x0adff, 0xc3dc26df, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsoldfort, Specsoldfort, Spectrum)
STD_ROM_FN(Specsoldfort)

struct BurnDriver BurnSpecsoldfort = {
	"spec_soldfort", NULL, "spec_spectrum", NULL, "1988",
	"Soldier of Fortune (48K)\0", NULL, "Firebird Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsoldfortRomInfo, SpecsoldfortRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Solomon's Key (48K)

static struct BurnRomInfo SpecsolomonRomDesc[] = {
	{ "Solomon's Key (1987)(U.S. Gold).z80", 0x09608, 0xe9a42bde, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsolomon, Specsolomon, Spectrum)
STD_ROM_FN(Specsolomon)

struct BurnDriver BurnSpecsolomon = {
	"spec_solomon", NULL, "spec_spectrum", NULL, "1987",
	"Solomon's Key (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsolomonRomInfo, SpecsolomonRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Space Harrier (48K)

static struct BurnRomInfo SpecsharrierRomDesc[] = {
	{ "Space Harrier (1986)(Elite Systems).z80", 0x0b439, 0xd33b7f51, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsharrier, Specsharrier, Spectrum)
STD_ROM_FN(Specsharrier)

struct BurnDriver BurnSpecsharrier = {
	"spec_sharrier", NULL, "spec_spectrum", NULL, "1986",
	"Space Harrier (48K)\0", NULL, "Elite Systems", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsharrierRomInfo, SpecsharrierRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Space Harrier II (48K)

static struct BurnRomInfo Specsharrir2RomDesc[] = {
	{ "Space Harrier II (1990)(Grandslam Entertainments).z80", 0x0a2e4, 0x2f556b72, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsharrir2, Specsharrir2, Spectrum)
STD_ROM_FN(Specsharrir2)

struct BurnDriver BurnSpecsharrir2 = {
	"spec_sharrir2", NULL, "spec_spectrum", NULL, "1990",
	"Space Harrier II (48K)\0", NULL, "Grandslam Entertainments", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specsharrir2RomInfo, Specsharrir2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Spy Hunter (48K)

static struct BurnRomInfo SpecspyhuntRomDesc[] = {
	{ "Spy Hunter (1985)(U.S. Gold).z80", 0x086af, 0x7c1b3220, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specspyhunt, Specspyhunt, Spectrum)
STD_ROM_FN(Specspyhunt)

struct BurnDriver BurnSpecspyhunt = {
	"spec_spyhunt", NULL, "spec_spectrum", NULL, "1985",
	"Spy Hunter (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecspyhuntRomInfo, SpecspyhuntRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Spy vs Spy (48K)

static struct BurnRomInfo SpecspyvspyRomDesc[] = {
	{ "Spy vs Spy (1985)(Beyond Software).z80", 0x09f9a, 0xa5fc636b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specspyvspy, Specspyvspy, Spectrum)
STD_ROM_FN(Specspyvspy)

struct BurnDriver BurnSpecspyvspy = {
	"spec_spyvspy", NULL, "spec_spectrum", NULL, "1985",
	"Spy vs Spy (48K)\0", NULL, "Beyond Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecspyvspyRomInfo, SpecspyvspyRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Spy vs Spy II - The Island Caper (48K)

static struct BurnRomInfo Specspyvspy2RomDesc[] = {
	{ "Spy vs Spy II - The Island Caper (1987)(Databyte).z80", 0x09350, 0xe5133176, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specspyvspy2, Specspyvspy2, Spectrum)
STD_ROM_FN(Specspyvspy2)

struct BurnDriver BurnSpecspyvspy2 = {
	"spec_spyvspy2", NULL, "spec_spectrum", NULL, "1987",
	"Spy vs Spy II - The Island Caper (48K)\0", NULL, "Databyte", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specspyvspy2RomInfo, Specspyvspy2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Spy vs Spy III - Arctic Antics (48K)

static struct BurnRomInfo Specspyvspy3RomDesc[] = {
	{ "Spy vs Spy III - Arctic Antics (1988)(Databyte).z80", 0x07a46, 0x9bf7db2a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specspyvspy3, Specspyvspy3, Spectrum)
STD_ROM_FN(Specspyvspy3)

struct BurnDriver BurnSpecspyvspy3 = {
	"spec_spyvspy3", NULL, "spec_spectrum", NULL, "1988",
	"Spy vs Spy III - Arctic Antics (48K)\0", NULL, "Databyte", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specspyvspy3RomInfo, Specspyvspy3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Star Farce (48K)

static struct BurnRomInfo SpecstarfarcRomDesc[] = {
	{ "Star Farce (1988)(Mastertronic).z80", 0x0a888, 0x91817feb, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specstarfarc, Specstarfarc, Spectrum)
STD_ROM_FN(Specstarfarc)

struct BurnDriver BurnSpecstarfarc = {
	"spec_starfarc", NULL, "spec_spectrum", NULL, "1988",
	"Star Farce (48K)\0", NULL, "Mastertronic", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecstarfarcRomInfo, SpecstarfarcRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Stop the Express (48K)

static struct BurnRomInfo SpecstopexprRomDesc[] = {
	{ "Stop The Express (1983)(Hudson Soft).z80", 0x054c4, 0x56a42e2e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specstopexpr, Specstopexpr, Spectrum)
STD_ROM_FN(Specstopexpr)

struct BurnDriver BurnSpecstopexpr = {
	"spec_stopexpr", NULL, "spec_spectrum", NULL, "1983",
	"Stop the Express (48K)\0", NULL, "Sinclair Research", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecstopexprRomInfo, SpecstopexprRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Stormlord (48K)

static struct BurnRomInfo SpecstormlorRomDesc[] = {
	{ "Stormlord (1989)(Hewson Consultants)[t].z80", 0x09146, 0x95529c30, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specstormlor, Specstormlor, Spectrum)
STD_ROM_FN(Specstormlor)

struct BurnDriver BurnSpecstormlor = {
	"spec_stormlor", NULL, "spec_spectrum", NULL, "1989",
	"Stormlord (48K)\0", NULL, "Hewson Consultants", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecstormlorRomInfo, SpecstormlorRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Street Fighter (48K)

static struct BurnRomInfo SpecsfightRomDesc[] = {
	{ "Street Fighter (1983)(Shards Software).z80", 0x04787, 0xd15c56f4, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsfight, Specsfight, Spectrum)
STD_ROM_FN(Specsfight)

struct BurnDriver BurnSpecsfight = {
	"spec_sfight", NULL, "spec_spectrum", NULL, "1983",
	"Street Fighter (48K)\0", NULL, "Shards Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsfightRomInfo, SpecsfightRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Strider (128K)

static struct BurnRomInfo SpecstriderRomDesc[] = {
	{ "Strider (1989)(U.S. Gold)[128K].z80", 0x199f5, 0xf7390779, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specstrider, Specstrider, Spec128)
STD_ROM_FN(Specstrider)

struct BurnDriver BurnSpecstrider = {
	"spec_strider", NULL, "spec_spec128", NULL, "1989",
	"Strider (128K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecstriderRomInfo, SpecstriderRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Strider II (48K)

static struct BurnRomInfo Specstrider2RomDesc[] = {
	{ "Strider II (1990)(U.S. Gold)[needs tape load].z80", 0x07147, 0x63f5426d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specstrider2, Specstrider2, Spectrum)
STD_ROM_FN(Specstrider2)

struct BurnDriver BurnSpecstrider2 = {
	"spec_strider2", NULL, "spec_spectrum", NULL, "1990",
	"Strider II (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specstrider2RomInfo, Specstrider2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Striker (128K)

static struct BurnRomInfo SpecstrikerRomDesc[] = {
	{ "Striker (1989)(Cult Games)[128K].z80", 0x090e9, 0xf7144c4d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specstriker, Specstriker, Spec128)
STD_ROM_FN(Specstriker)

struct BurnDriver BurnSpecstriker = {
	"spec_striker", NULL, "spec_spec128", NULL, "1989",
	"Striker (128K)\0", NULL, "Cult Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecstrikerRomInfo, SpecstrikerRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Stunt Car Racer (48K)

static struct BurnRomInfo Specstuntcar_48RomDesc[] = {
	{ "Stunt Car Racer (1989)(Micro Style).z80", 0x08f77, 0xc02a3c1f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specstuntcar_48, Specstuntcar_48, Spectrum)
STD_ROM_FN(Specstuntcar_48)

struct BurnDriver BurnSpecstuntcar_48 = {
	"spec_stuntcar_48", "spec_stuntcar", "spec_spectrum", NULL, "1989",
	"Stunt Car Racer (48K)\0", NULL, "Micro Style", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specstuntcar_48RomInfo, Specstuntcar_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Stunt Car Racer (128K)

static struct BurnRomInfo SpecstuntcarRomDesc[] = {
	{ "Stunt Car Racer (1989)(Micro Style)[128K].z80", 0x0ae97, 0xb3abb3a6, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specstuntcar, Specstuntcar, Spec128)
STD_ROM_FN(Specstuntcar)

struct BurnDriver BurnSpecstuntcar = {
	"spec_stuntcar", NULL, "spec_spec128", NULL, "1989",
	"Stunt Car Racer (128K)\0", NULL, "Micro Style", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecstuntcarRomInfo, SpecstuntcarRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Subbuteo - The Computer Game (48K)

static struct BurnRomInfo Specsubbueto_48RomDesc[] = {
	{ "Subbuteo - The Computer Game (1990)(Electronic Zoo).z80", 0x083de, 0x31346a92, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsubbueto_48, Specsubbueto_48, Spectrum)
STD_ROM_FN(Specsubbueto_48)

struct BurnDriver BurnSpecsubbueto_48 = {
	"spec_subbueto_48", "spec_subbueto", "spec_spectrum", NULL, "1990",
	"Subbuteo - The Computer Game (48K)\0", NULL, "Electronic Zoo", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specsubbueto_48RomInfo, Specsubbueto_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Subbuteo - The Computer Game (128K)

static struct BurnRomInfo SpecsubbuetoRomDesc[] = {
	{ "Subbuteo - The Computer Game (1990)(Electronic Zoo)[128K].z80", 0x0eb1e, 0x37c72d4c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsubbueto, Specsubbueto, Spec128)
STD_ROM_FN(Specsubbueto)

struct BurnDriver BurnSpecsubbueto = {
	"spec_subbueto", NULL, "spec_spec128", NULL, "1990",
	"Subbuteo - The Computer Game (128K)\0", NULL, "Electronic Zoo", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsubbuetoRomInfo, SpecsubbuetoRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Super Hang-On - Part 1 - Beginner (48K)

static struct BurnRomInfo SpecshangonRomDesc[] = {
	{ "Super Hang-On - Part 1 - Beginner (1986)(Electric Dreams Software).z80", 0x08818, 0x6021b420, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specshangon, Specshangon, Spectrum)
STD_ROM_FN(Specshangon)

struct BurnDriver BurnSpecshangon = {
	"spec_shangon", NULL, "spec_spectrum", NULL, "1986",
	"Super Hang-On - Part 1 - Beginner (48K)\0", NULL, "Electric Dreams Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecshangonRomInfo, SpecshangonRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Super Hang-On - Part 2 - Junior (48K)

static struct BurnRomInfo Specshangon2RomDesc[] = {
	{ "Super Hang-On - Part 2 - Junior (1986)(Electric Dreams Software).z80", 0x08690, 0xd8180d70, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specshangon2, Specshangon2, Spectrum)
STD_ROM_FN(Specshangon2)

struct BurnDriver BurnSpecshangon2 = {
	"spec_shangon2", "spec_shangon", "spec_spectrum", NULL, "1986",
	"Super Hang-On - Part 2 - Junior (48K)\0", NULL, "Electric Dreams Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specshangon2RomInfo, Specshangon2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Super Hang-On - Part 3 - Senior (48K)

static struct BurnRomInfo Specshangon3RomDesc[] = {
	{ "Super Hang-On - Part 3 - Senior (1986)(Electric Dreams Software).z80", 0x08adc, 0x363567ec, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specshangon3, Specshangon3, Spectrum)
STD_ROM_FN(Specshangon3)

struct BurnDriver BurnSpecshangon3 = {
	"spec_shangon3", "spec_shangon", "spec_spectrum", NULL, "1986",
	"Super Hang-On - Part 3 - Senior (48K)\0", NULL, "Electric Dreams Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specshangon3RomInfo, Specshangon3RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Super Hang-On - Part 4 - Expert (48K)

static struct BurnRomInfo Specshangon4RomDesc[] = {
	{ "Super Hang-On - Part 4 - Expert (1986)(Electric Dreams Software).z80", 0x0868a, 0x8cb2ac52, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specshangon4, Specshangon4, Spectrum)
STD_ROM_FN(Specshangon4)

struct BurnDriver BurnSpecshangon4 = {
	"spec_shangon4", "spec_shangon", "spec_spectrum", NULL, "1986",
	"Super Hang-On - Part 4 - Expert (48K)\0", NULL, "Electric Dreams Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specshangon4RomInfo, Specshangon4RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Super Monaco GP (128K)

static struct BurnRomInfo SpecsmgpRomDesc[] = {
	{ "Super Monaco GP (1991)(U.S. Gold)[128K].z80", 0x0e08c, 0x6a1dcc87, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsmgp, Specsmgp, Spec128)
STD_ROM_FN(Specsmgp)

struct BurnDriver BurnSpecsmgp = {
	"spec_smgp", NULL, "spec_spec128", NULL, "1991",
	"Super Monaco GP (128K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsmgpRomInfo, SpecsmgpRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Super Sprint (48K)

static struct BurnRomInfo SpecsupespriRomDesc[] = {
	{ "Super Sprint (1987)(Activision).z80", 0x07a7e, 0x52ee2754, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specsupespri, Specsupespri, Spectrum)
STD_ROM_FN(Specsupespri)

struct BurnDriver BurnSpecsupespri = {
	"spec_supespri", NULL, "spec_spectrum", NULL, "1987",
	"Super Sprint (48K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecsupespriRomInfo, SpecsupespriRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// SWIV (128K)

static struct BurnRomInfo SpecswivRomDesc[] = {
	{ "SWIV (1991)(Storm Software)[128K].z80", 0x1949b, 0xedc341d5, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specswiv, Specswiv, Spec128)
STD_ROM_FN(Specswiv)

struct BurnDriver BurnSpecswiv = {
	"spec_swiv", NULL, "spec_spec128", NULL, "1991",
	"SWIV (128K)\0", NULL, "Storm Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecswivRomInfo, SpecswivRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Teenage Mutant Hero Turtles - The Coin-Op (48K)

static struct BurnRomInfo SpectmhtarcRomDesc[] = {
	{ "Teenage Mutant Hero Turtles - The Coin-Op (1991)(Image Works).z80", 0x0a173, 0x2adc23b7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectmhtarc, Spectmhtarc, Spectrum)
STD_ROM_FN(Spectmhtarc)

struct BurnDriver BurnSpectmhtarc = {
	"spec_tmhtarc", NULL, "spec_spectrum", NULL, "1991",
	"Teenage Mutant Hero Turtles - The Coin-Op (48K)\0", NULL, "Image Works", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectmhtarcRomInfo, SpectmhtarcRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Teenage Mutant Hero Turtles (48K)

static struct BurnRomInfo Spectmht_48RomDesc[] = {
	{ "Teenage Mutant Hero Turtles (1990)(Image Works).z80", 0x0b51c, 0xfa454654, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectmht_48, Spectmht_48, Spectrum)
STD_ROM_FN(Spectmht_48)

struct BurnDriver BurnSpectmht_48 = {
	"spec_tmht_48", "spec_tmht", "spec_spectrum", NULL, "1990",
	"Teenage Mutant Hero Turtles (48K)\0", NULL, "Image Works", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spectmht_48RomInfo, Spectmht_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Teenage Mutant Hero Turtles (128K)

static struct BurnRomInfo SpectmhtRomDesc[] = {
	{ "Teenage Mutant Hero Turtles (1990)(Image Works)[128K].z80", 0x0c0b6, 0x66d86001, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectmht, Spectmht, Spec128)
STD_ROM_FN(Spectmht)

struct BurnDriver BurnSpectmht = {
	"spec_tmht", NULL, "spec_spec128", NULL, "1990",
	"Teenage Mutant Hero Turtles (128K)\0", NULL, "Image Works", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectmhtRomInfo, SpectmhtRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Terra Cresta (48K)

static struct BurnRomInfo Specterracrs_48RomDesc[] = {
	{ "Terra Cresta (1986)(Imagine Software).z80", 0x08ff2, 0xa28b1755, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specterracrs_48, Specterracrs_48, Spectrum)
STD_ROM_FN(Specterracrs_48)

struct BurnDriver BurnSpecterracrs_48 = {
	"spec_terracrs_48", "spec_terracrs", "spec_spectrum", NULL, "1986",
	"Terra Cresta (48K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specterracrs_48RomInfo, Specterracrs_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Terra Cresta (128K)

static struct BurnRomInfo SpecterracrsRomDesc[] = {
	{ "Terra Cresta (1986)(Imagine Software)[128K].z80", 0x09496, 0x6f15fad6, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specterracrs, Specterracrs, Spec128)
STD_ROM_FN(Specterracrs)

struct BurnDriver BurnSpecterracrs = {
	"spec_terracrs", NULL, "spec_spec128", NULL, "1986",
	"Terra Cresta (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecterracrsRomInfo, SpecterracrsRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Tetris (Mirrorsoft) (48K)

static struct BurnRomInfo Spectetris_48RomDesc[] = {
	{ "Tetris (1988)(Mirrorsoft).z80", 0x06c7a, 0x1226181e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectetris_48, Spectetris_48, Spectrum)
STD_ROM_FN(Spectetris_48)

struct BurnDriver BurnSpectetris_48 = {
	"spec_tetris_48", "spec_tetris", "spec_spectrum", NULL, "1988",
	"Tetris (Mirrorsoft) (48K)\0", NULL, "Mirrorsoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spectetris_48RomInfo, Spectetris_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Tetris (Mirrorsoft) (128K)

static struct BurnRomInfo SpectetrisRomDesc[] = {
	{ "Tetris (1988)(Mirrorsoft)(128k).z80", 0x075b1, 0xca758c04, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectetris, Spectetris, Spectrum)
STD_ROM_FN(Spectetris)

struct BurnDriver BurnSpectetris = {
	"spec_tetris", NULL, "spec_spec128", NULL, "1988",
	"Tetris (Mirrorsoft) (128K)\0", NULL, "Mirrorsoft", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectetrisRomInfo, SpectetrisRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Three Weeks in Paradise (48K)

static struct BurnRomInfo Spec3weekspr_48RomDesc[] = {
	{ "Three Weeks In Paradise (1986)(Mikro-Gen).z80", 0x0a49e, 0x3c7ac0a9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spec3weekspr_48, Spec3weekspr_48, Spectrum)
STD_ROM_FN(Spec3weekspr_48)

struct BurnDriver BurnSpec3weekspr_48 = {
	"spec_3weekspr_48", "spec_3weekspr", "spec_spectrum", NULL, "1985",
	"Three Weeks in Paradise (48K)\0", NULL, "Mikro-Gen", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spec3weekspr_48RomInfo, Spec3weekspr_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Three Weeks in Paradise (128K)

static struct BurnRomInfo Spec3weeksprRomDesc[] = {
	{ "Three Weeks In Paradise (1986)(Mikro-Gen)(128k).z80", 0x0e06c, 0xf21d8b5d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spec3weekspr, Spec3weekspr, Spectrum)
STD_ROM_FN(Spec3weekspr)

struct BurnDriver BurnSpec3weekspr = {
	"spec_3weekspr", NULL, "spec_spec128", NULL, "1985",
	"Three Weeks in Paradise (128K)\0", NULL, "Mikro-Gen", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spec3weeksprRomInfo, Spec3weeksprRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Thunder Blade (128K)

static struct BurnRomInfo SpecthndrbldRomDesc[] = {
	{ "Thunder Blade (1988)(U.S. Gold)[128K].z80", 0x09abe, 0xb6773249, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specthndrbld, Specthndrbld, Spec128)
STD_ROM_FN(Specthndrbld)

struct BurnDriver BurnSpecthndrbld = {
	"spec_thndrbld", NULL, "spec_spec128", NULL, "1988",
	"Thunder Blade (128K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecthndrbldRomInfo, SpecthndrbldRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Tiger Road (48K)

static struct BurnRomInfo Spectigeroad_48RomDesc[] = {
	{ "Tiger Road (1988)(Go!).z80", 0x09f9b, 0x04767cb9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectigeroad_48, Spectigeroad_48, Spectrum)
STD_ROM_FN(Spectigeroad_48)

struct BurnDriver BurnSpectigeroad_48 = {
	"spec_tigeroad_48", "spec_tigeroad", "spec_spectrum", NULL, "1988",
	"Tiger Road (48K)\0", NULL, "Go!", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spectigeroad_48RomInfo, Spectigeroad_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Tiger Road (128K)

static struct BurnRomInfo SpectigeroadRomDesc[] = {
	{ "Tiger Road (1988)(Go!)[128K].z80", 0x0b726, 0xede04afd, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectigeroad, Spectigeroad, Spec128)
STD_ROM_FN(Spectigeroad)

struct BurnDriver BurnSpectigeroad = {
	"spec_tigeroad", NULL, "spec_spec128", NULL, "1988",
	"Tiger Road (128K)\0", NULL, "Go!", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectigeroadRomInfo, SpectigeroadRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Time Scanner (48K)

static struct BurnRomInfo SpectimescanRomDesc[] = {
	{ "Time Scanner (1989)(Activision).z80", 0x0a46a, 0x06983d6e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectimescan, Spectimescan, Spectrum)
STD_ROM_FN(Spectimescan)

struct BurnDriver BurnSpectimescan = {
	"spec_timescan", NULL, "spec_spectrum", NULL, "1989",
	"Time Scanner (48K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectimescanRomInfo, SpectimescanRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Total Recall (128K)

static struct BurnRomInfo SpectotrcallRomDesc[] = {
	{ "Total Recall (1991)(Ocean)[128K].z80", 0x17197, 0xab3503be, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectotrcall, Spectotrcall, Spec128)
STD_ROM_FN(Spectotrcall)

struct BurnDriver BurnSpectotrcall = {
	"spec_totrcall", NULL, "spec_spec128", NULL, "1991",
	"Total Recall (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectotrcallRomInfo, SpectotrcallRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Track and Field (48K)

static struct BurnRomInfo SpectracfielRomDesc[] = {
	{ "Track and Field (1988)(Ocean).z80", 0x05787, 0x7ddee010, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectracfiel, Spectracfiel, Spectrum)
STD_ROM_FN(Spectracfiel)

struct BurnDriver BurnSpectracfiel = {
	"spec_tracfiel", NULL, "spec_spectrum", NULL, "1988",
	"Track and Field (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectracfielRomInfo, SpectracfielRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Trantor - The Last Stormtrooper (48K)

static struct BurnRomInfo SpectrthlastRomDesc[] = {
	{ "Trantor The Last Stormtrooper (1987)(Go!).z80", 0x09b69, 0xbad682da, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectrthlast, Spectrthlast, Spectrum)
STD_ROM_FN(Spectrthlast)

struct BurnDriver BurnSpectrthlast = {
	"spec_trthlast", NULL, "spec_spectrum", NULL, "1987",
	"Trantor - The Last Stormtrooper (48K)\0", NULL, "Go!", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectrthlastRomInfo, SpectrthlastRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Trap Door, The (48K)

static struct BurnRomInfo SpectradoothRomDesc[] = {
	{ "Trap Door, The (1986)(Piranha).z80", 0x0a73f, 0x27e0667f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectradooth, Spectradooth, Spectrum)
STD_ROM_FN(Spectradooth)

struct BurnDriver BurnSpectradooth = {
	"spec_tradooth", NULL, "spec_spectrum", NULL, "1986",
	"Trap Door, The (48K)\0", NULL, "Piranha", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectradoothRomInfo, SpectradoothRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Untouchables, The (48K)

static struct BurnRomInfo SpecuntouchbRomDesc[] = {
	{ "Untouchables, The (1989)(Ocean)[needs tape load].z80", 0x09cea, 0x98cc8b4f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specuntouchb, Specuntouchb, Spectrum)
STD_ROM_FN(Specuntouchb)

struct BurnDriver BurnSpecuntouchb = {
	"spec_untouchb", NULL, "spec_spectrum", NULL, "1989",
	"Untouchables, The (48K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecuntouchbRomInfo, SpecuntouchbRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Vindicator, The (128K)

static struct BurnRomInfo SpecvindtheRomDesc[] = {
	{ "Vindicator, The (1988)(Imagine Software)[128K].z80", 0x1b692, 0x57c8a81d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specvindthe, Specvindthe, Spec128)
STD_ROM_FN(Specvindthe)

struct BurnDriver BurnSpecvindthe = {
	"spec_vindthe", NULL, "spec_spec128", NULL, "1988",
	"Vindicator, The (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecvindtheRomInfo, SpecvindtheRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Vindicators (128K)

static struct BurnRomInfo SpecvindicatRomDesc[] = {
	{ "Vindicators (1989)(Domark)[128K].z80", 0x0aa60, 0x6c4dbbe3, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specvindicat, Specvindicat, Spec128)
STD_ROM_FN(Specvindicat)

struct BurnDriver BurnSpecvindicat = {
	"spec_vindicat", NULL, "spec_spec128", NULL, "1989",
	"Vindicators (128K)\0", NULL, "Domark", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecvindicatRomInfo, SpecvindicatRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Wacky Darts (48K)

static struct BurnRomInfo SpecwackdartRomDesc[] = {
	{ "Wacky Darts (1991)(Codemasters).z80", 0x0ae9d, 0x6214a4ce, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specwackdart, Specwackdart, Spectrum)
STD_ROM_FN(Specwackdart)

struct BurnDriver BurnSpecwackdart = {
	"spec_wackdart", NULL, "spec_spectrum", NULL, "1991",
	"Wacky Darts (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecwackdartRomInfo, SpecwackdartRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Wacky Races (128K)

static struct BurnRomInfo SpecwackraceRomDesc[] = {
	{ "Wacky Races (1992)(Hi-Tec Software)(128k).z80", 0x16523, 0xee205ebd, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specwackrace, Specwackrace, Spectrum)
STD_ROM_FN(Specwackrace)

struct BurnDriver BurnSpecwackrace = {
	"spec_wackrace", NULL, "spec_spec128", NULL, "1992",
	"Wacky Races (128K)\0", NULL, "Hi-Tec Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecwackraceRomInfo, SpecwackraceRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Wanted! Monty Mole (48K)

static struct BurnRomInfo SpecwanmonmoRomDesc[] = {
	{ "Monty Mole (1984)(Gremlin Graphics).z80", 0x09c78, 0x2e7a94b2, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specwanmonmo, Specwanmonmo, Spectrum)
STD_ROM_FN(Specwanmonmo)

struct BurnDriver BurnSpecwanmonmo = {
	"spec_wanmonmo", NULL, "spec_spectrum", NULL, "1984",
	"Wanted! Monty Mole (48K)\0", NULL, "Gremlin Graphics Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecwanmonmoRomInfo, SpecwanmonmoRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Way of the Exploding Fist, The (48K)

static struct BurnRomInfo SpecwayexplfRomDesc[] = {
	{ "Way of the Exploding Fist, The (1985)(Melbourne House).z80", 0x0a6de, 0xf73f3022, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specwayexplf, Specwayexplf, Spectrum)
STD_ROM_FN(Specwayexplf)

struct BurnDriver BurnSpecwayexplf = {
	"spec_wayexplf", NULL, "spec_spectrum", NULL, "1985",
	"Way of the Exploding Fist, The (48K)\0", NULL, "Melbourne House", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecwayexplfRomInfo, SpecwayexplfRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// WEC Le Mans (48K)

static struct BurnRomInfo Specwecleman_48RomDesc[] = {
	{ "WEC Le Mans (1988)(Imagine Software).z80", 0x0a23e, 0x83a8e1f7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specwecleman_48, Specwecleman_48, Spectrum)
STD_ROM_FN(Specwecleman_48)

struct BurnDriver BurnSpecwecleman_48 = {
	"spec_wecleman_48", "spec_wecleman", "spec_spectrum", NULL, "1988",
	"WEC Le Mans (48K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specwecleman_48RomInfo, Specwecleman_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// WEC Le Mans (128K)

static struct BurnRomInfo SpecweclemanRomDesc[] = {
	{ "WEC Le Mans (1988)(Imagine Software)[128K].z80", 0x0d90e, 0xa77096e2, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specwecleman, Specwecleman, Spec128)
STD_ROM_FN(Specwecleman)

struct BurnDriver BurnSpecwecleman = {
	"spec_wecleman", NULL, "spec_spec128", NULL, "1988",
	"WEC Le Mans (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecweclemanRomInfo, SpecweclemanRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Wild West Seymour (48K)

static struct BurnRomInfo Specwwseymr_48RomDesc[] = {
	{ "Wild West Seymour (1992)(Codemasters).z80", 0x0b4ab, 0xbf324d91, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specwwseymr_48, Specwwseymr_48, Spectrum)
STD_ROM_FN(Specwwseymr_48)

struct BurnDriver BurnSpecwwseymr_48 = {
	"spec_wwseymr_48", "spec_wwseymr", "spec_spectrum", NULL, "1992",
	"Wild West Seymour (48K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specwwseymr_48RomInfo, Specwwseymr_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Wild West Seymour (128K)

static struct BurnRomInfo SpecwwseymrRomDesc[] = {
	{ "Wild West Seymour (1992)(Codemasters)(128k).z80", 0x141c6, 0x01dc3cee, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specwwseymr, Specwwseymr, Spectrum)
STD_ROM_FN(Specwwseymr)

struct BurnDriver BurnSpecwwseymr = {
	"spec_wwseymr", NULL, "spec_spec128", NULL, "1992",
	"Wild West Seymour (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecwwseymrRomInfo, SpecwwseymrRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Wonder Boy (128K)

static struct BurnRomInfo SpecwboyRomDesc[] = {
	{ "Wonder Boy (1987)(Activision)[128K].z80", 0x11193, 0xb492a055, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specwboy, Specwboy, Spec128)
STD_ROM_FN(Specwboy)

struct BurnDriver BurnSpecwboy = {
	"spec_wboy", NULL, "spec_spec128", NULL, "1987",
	"Wonder Boy (128K)\0", NULL, "Activision", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecwboyRomInfo, SpecwboyRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Xenon (48K)

static struct BurnRomInfo SpecxenonRomDesc[] = {
	{ "Xenon (1988)(Melbourne House).z80", 0x09577, 0x889e59ec, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specxenon, Specxenon, Spectrum)
STD_ROM_FN(Specxenon)

struct BurnDriver BurnSpecxenon = {
	"spec_xenon", NULL, "spec_spectrum", NULL, "1988",
	"Xenon (48K)\0", NULL, "Melbourne House", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecxenonRomInfo, SpecxenonRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Xevious (48K)

static struct BurnRomInfo SpecxeviousRomDesc[] = {
	{ "Xevious (1987)(U.S. Gold).z80", 0x05ae7, 0xac2c0235, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specxevious, Specxevious, Spectrum)
STD_ROM_FN(Specxevious)

struct BurnDriver BurnSpecxevious = {
	"spec_xevious", NULL, "spec_spectrum", NULL, "1987",
	"Xevious (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecxeviousRomInfo, SpecxeviousRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Yie Ar Kung-Fu (48K)

static struct BurnRomInfo Specyiarkufu_48RomDesc[] = {
	{ "Yie Ar Kung-Fu (1985)(Imagine Software).z80", 0x0a130, 0x23da9f6d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specyiarkufu_48, Specyiarkufu_48, Spectrum)
STD_ROM_FN(Specyiarkufu_48)

struct BurnDriver BurnSpecyiarkufu_48 = {
	"spec_yiarkufu_48", "spec_yiarkufu", "spec_spectrum", NULL, "1985",
	"Yie Ar Kung-Fu (48K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specyiarkufu_48RomInfo, Specyiarkufu_48RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Yie Ar Kung-Fu (128K)

static struct BurnRomInfo SpecyiarkufuRomDesc[] = {
	{ "Yie Ar Kung-Fu (1985)(Imagine Software)[128K].z80", 0x157f7, 0xf7c52002, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specyiarkufu, Specyiarkufu, Spec128)
STD_ROM_FN(Specyiarkufu)

struct BurnDriver BurnSpecyiarkufu = {
	"spec_yiarkufu", NULL, "spec_spec128", NULL, "1985",
	"Yie Ar Kung-Fu (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecyiarkufuRomInfo, SpecyiarkufuRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Yie Ar Kung-Fu II (48K)

static struct BurnRomInfo Specyiarkuf2RomDesc[] = {
	{ "Yie Ar Kung-Fu II (1986)(Imagine Software).z80", 0x08f6e, 0xef420fe9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specyiarkuf2, Specyiarkuf2, Spectrum)
STD_ROM_FN(Specyiarkuf2)

struct BurnDriver BurnSpecyiarkuf2 = {
	"spec_yiarkuf2", NULL, "spec_spectrum", NULL, "1986",
	"Yie Ar Kung-Fu II (48K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specyiarkuf2RomInfo, Specyiarkuf2RomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Zaxxon (48K)

static struct BurnRomInfo SpeczaxxonRomDesc[] = {
	{ "Zaxxon (1985)(U.S. Gold).z80", 0x092e6, 0xe89d7896, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speczaxxon, Speczaxxon, Spectrum)
STD_ROM_FN(Speczaxxon)

struct BurnDriver BurnSpeczaxxon = {
	"spec_zaxxon", NULL, "spec_spectrum", NULL, "1985",
	"Zaxxon (48K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeczaxxonRomInfo, SpeczaxxonRomName, NULL, NULL, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};
