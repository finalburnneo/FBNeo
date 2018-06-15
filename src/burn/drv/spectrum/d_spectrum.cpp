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
	dacbuf                  = (INT16*)Next; Next += 0x800 * 2 * sizeof(INT16);
	
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
	
	DACInit(0, 0, 0, ZetTotalCycles, 3500000);
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
	
	DACInit(0, 0, 0, ZetTotalCycles, 3500000);
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

	for (INT32 i = 0; i < 312; i++) {
		ay_table[i] = ((i * nBurnSoundLen) / 312) - p;
		p = (i * nBurnSoundLen) / 312;
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

// 180 (Micro-Gen) (48K)

static struct BurnRomInfo Spec180mgen_48RomDesc[] = {
	{ "180 (1983)(Mikro-Gen).z80", 0x03a00, 0x03cad5e0, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spec180mgen_48, Spec180mgen_48, Spectrum)
STD_ROM_FN(Spec180mgen_48)

struct BurnDriver BurnSpec180mgen_48 = {
	"spec_180mgen_48", "spec_180", "spec_spectrum", NULL, "1983",
	"180 (Micro-Gen) (48K)\0", NULL, "Mikro-Gen", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spec180mgen_48RomInfo, Spec180mgen_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

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
	SpectrumGetZipName, Spec180_48RomInfo, Spec180_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// 180 (128K)

static struct BurnRomInfo Spec180RomDesc[] = {
	{ "180 (1986)(Mastertronic Added Dimension)[128K].z80", 0x0d536, 0xc4937cba, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spec180, Spec180, Spectrum)
STD_ROM_FN(Spec180)

struct BurnDriver BurnSpec180 = {
	"spec_180", NULL, "spec_spec128", NULL, "1986",
	"180 (128K)\0", NULL, "Mastertronic Added Dimension", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Spec180RomInfo, Spec180RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Adidas Championship Football (128K)

static struct BurnRomInfo SpecadichafoRomDesc[] = {
	{ "Adidas Championship Football (1990)(Ocean)[128K].z80", 0x17369, 0x89364845, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specadichafo, Specadichafo, Spectrum)
STD_ROM_FN(Specadichafo)

struct BurnDriver BurnSpecadichafo = {
	"spec_adichafo", NULL, "spec_spec128", NULL, "1990",
	"Adidas Championship Football (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecadichafoRomInfo, SpecadichafoRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
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
	SpectrumGetZipName, Specatvsim_48RomInfo, Specatvsim_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// ATV Simulator - All Terrain Vehicle (128K)

static struct BurnRomInfo SpecatvsimRomDesc[] = {
	{ "ATV Simulator - All Terrain Vehicle (1987)(Codemasters)[128K].z80", 0x0a8ef, 0xe1fc4bb9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specatvsim, Specatvsim, Spectrum)
STD_ROM_FN(Specatvsim)

struct BurnDriver BurnSpecatvsim = {
	"spec_atvsim", NULL, "spec_spec128", NULL, "1987",
	"ATV Simulator - All Terrain Vehicle (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecatvsimRomInfo, SpecatvsimRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
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
	SpectrumGetZipName, Specbarbply1RomInfo, Specbarbply1RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specbarbply2RomInfo, Specbarbply2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specbarbarn_48RomInfo, Specbarbarn_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Barbarian (128K)

static struct BurnRomInfo SpecbarbarnRomDesc[] = {
	{ "Barbarian (1988)(Melbourne House)[128K].z80", 0x0a624, 0x4d0607de, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbarbarn, Specbarbarn, Spectrum)
STD_ROM_FN(Specbarbarn)

struct BurnDriver BurnSpecbarbarn = {
	"spec_barbarn", NULL, "spec_spec128", NULL, "1988",
	"Barbarian (128K)\0", NULL, "Melbourne House", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbarbarnRomInfo, SpecbarbarnRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Barbarian II - The Dungeon of Drax (128K)

static struct BurnRomInfo Specbarbarn2RomDesc[] = {
	{ "Barbarian II - The Dungeon of Drax (1988)(Palace Software)[128K].z80", 0x1ac6c, 0x2215c3b7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbarbarn2, Specbarbarn2, Spectrum)
STD_ROM_FN(Specbarbarn2)

struct BurnDriver BurnSpecbarbarn2 = {
	"spec_barbarn2", NULL, "spec_spec128", NULL, "1988",
	"Barbarian II - The Dungeon of Drax (128K)\0", NULL, "Palace Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specbarbarn2RomInfo, Specbarbarn2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecbatmanccRomInfo, SpecbatmanccRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specbatmancc2RomInfo, Specbatmancc2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Batman - The Movie (128K)

static struct BurnRomInfo SpecbatmanmvRomDesc[] = {
	{ "Batman - The Movie (1989)(Ocean)[128K].z80", 0x1b872, 0x17ed3d84, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbatmanmv, Specbatmanmv, Spectrum)
STD_ROM_FN(Specbatmanmv)

struct BurnDriver BurnSpecbatmanmv = {
	"spec_batmanmv", NULL, "spec_spec128", NULL, "1989",
	"Batman - The Movie (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbatmanmvRomInfo, SpecbatmanmvRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecbatmanpeRomInfo, SpecbatmanpeRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specbatman_48RomInfo, Specbatman_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Batman (128K)

static struct BurnRomInfo SpecbatmanRomDesc[] = {
	{ "Batman (1986)(Ocean)[128K].z80", 0x0b90a, 0x48ec8253, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specbatman, Specbatman, Spectrum)
STD_ROM_FN(Specbatman)

struct BurnDriver BurnSpecbatman = {
	"spec_batman", NULL, "spec_spec128", NULL, "1986",
	"Batman (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecbatmanRomInfo, SpecbatmanRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecblinkysRomInfo, SpecblinkysRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecbruceleeRomInfo, SpecbruceleeRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecbosstheRomInfo, SpecbosstheRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpeccanywarrRomInfo, SpeccanywarrRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specchasehq_48RomInfo, Specchasehq_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Chase H.Q. (128K)

static struct BurnRomInfo SpecchasehqRomDesc[] = {
	{ "Chase H.Q. (1989)(Ocean)[128K].z80", 0x1c04f, 0xbb5ae933, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specchasehq, Specchasehq, Spectrum)
STD_ROM_FN(Specchasehq)

struct BurnDriver BurnSpecchasehq = {
	"spec_chasehq", NULL, "spec_spec128", NULL, "1989",
	"Chase H.Q. (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecchasehqRomInfo, SpecchasehqRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Chase H.Q. II - Special Criminal Investigations (128K)

static struct BurnRomInfo Specchasehq2RomDesc[] = {
	{ "Chase H.Q. II - Special Criminal Investigations (1990)(Ocean)[128K].z80", 0x19035, 0x83ac9fea, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specchasehq2, Specchasehq2, Spectrum)
STD_ROM_FN(Specchasehq2)

struct BurnDriver BurnSpecchasehq2 = {
	"spec_chasehq2", NULL, "spec_spec128", NULL, "1990",
	"Chase H.Q. II - Special Criminal Investigations (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specchasehq2RomInfo, Specchasehq2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
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
	SpectrumGetZipName, SpecchuckeggRomInfo, SpecchuckeggRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specchuckeg2RomInfo, Specchuckeg2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
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
	SpectrumGetZipName, SpeccommandofRomInfo, SpeccommandofRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpeccommandopRomInfo, SpeccommandopRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpeccommandoRomInfo, SpeccommandoRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specdizdowra_48RomInfo, Specdizdowra_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy - Down the Rapids (128K)

static struct BurnRomInfo SpecdizdowraRomDesc[] = {
	{ "Dizzy - Down the Rapids (1991)(Codemasters)[128K].z80", 0x0a17e, 0x426abaa3, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizdowra, Specdizdowra, Spectrum)
STD_ROM_FN(Specdizdowra)

struct BurnDriver BurnSpecdizdowra = {
	"spec_dizdowra", NULL, "spec_spec128", NULL, "1991",
	"Dizzy - Down the Rapids (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdizdowraRomInfo, SpecdizdowraRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy - The Ultimate Cartoon Adventure (Russian) (128K)

static struct BurnRomInfo SpecdizzyrRomDesc[] = {
	{ "Dizzy - The Ultimate Cartoon Adventure (1987)(Codemasters)(ru)[128K].z80", 0x0c83c, 0x27b9e86c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzyr, Specdizzyr, Spectrum)
STD_ROM_FN(Specdizzyr)

struct BurnDriver BurnSpecdizzyr = {
	"spec_dizzyr", "spec_dizzy", "spec_spec128", NULL, "1987",
	"Dizzy - The Ultimate Cartoon Adventure (Russian) (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdizzyrRomInfo, SpecdizzyrRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specdizzy_48RomInfo, Specdizzy_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy - The Ultimate Cartoon Adventure (128K)

static struct BurnRomInfo SpecdizzyRomDesc[] = {
	{ "Dizzy - The Ultimate Cartoon Adventure (1987)(Codemasters)[128K].z80", 0x0b82d, 0x30bb57e1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy, Specdizzy, Spectrum)
STD_ROM_FN(Specdizzy)

struct BurnDriver BurnSpecdizzy = {
	"spec_dizzy", NULL, "spec_spec128", NULL, "1987",
	"Dizzy - The Ultimate Cartoon Adventure (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecdizzyRomInfo, SpecdizzyRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy II - Treasure Island Dizzy (Russian) (128K)

static struct BurnRomInfo Specdizzy2rRomDesc[] = {
	{ "Dizzy II - Treasure Island Dizzy (1988)(Codemasters)(ru)[128K].z80", 0x0e0a4, 0xccc7f01b, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy2r, Specdizzy2r, Spectrum)
STD_ROM_FN(Specdizzy2r)

struct BurnDriver BurnSpecdizzy2r = {
	"spec_dizzy2r", NULL, "spec_spec128", NULL, "1988",
	"Dizzy II - Treasure Island Dizzy (Russian) (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy2rRomInfo, Specdizzy2rRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specdizzy2_48RomInfo, Specdizzy2_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy II - Treasure Island Dizzy (128K)

static struct BurnRomInfo Specdizzy2RomDesc[] = {
	{ "Dizzy II - Treasure Island Dizzy (1988)(Codemasters)[128K].z80", 0x0da56, 0x3d2e194d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy2, Specdizzy2, Spectrum)
STD_ROM_FN(Specdizzy2)

struct BurnDriver BurnSpecdizzy2 = {
	"spec_dizzy2", NULL, "spec_spec128", NULL, "1988",
	"Dizzy II - Treasure Island Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy2RomInfo, Specdizzy2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy III - Fantasy World Dizzy (Russian) (128K)

static struct BurnRomInfo Specdizzy3rRomDesc[] = {
	{ "Dizzy III - Fantasy World Dizzy (1989)(Codemasters)(ru)[128K].z80", 0x0f2bd, 0x1ae2c460, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy3r, Specdizzy3r, Spectrum)
STD_ROM_FN(Specdizzy3r)

struct BurnDriver BurnSpecdizzy3r = {
	"spec_dizzy3r", "spec_dizzy3", "spec_spec128", NULL, "1989",
	"Dizzy III - Fantasy World Dizzy (Russian) (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy3rRomInfo, Specdizzy3rRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specdizzy3_48RomInfo, Specdizzy3_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy III - Fantasy World Dizzy (128K)

static struct BurnRomInfo Specdizzy3RomDesc[] = {
	{ "Dizzy III - Fantasy World Dizzy (1989)(Codemasters)[128K].z80", 0x0f172, 0xef716059, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy3, Specdizzy3, Spectrum)
STD_ROM_FN(Specdizzy3)

struct BurnDriver BurnSpecdizzy3 = {
	"spec_dizzy3", NULL, "spec_spec128", NULL, "1989",
	"Dizzy III - Fantasy World Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy3RomInfo, Specdizzy3RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specdizzy4_48RomInfo, Specdizzy4_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy IV - Magicland Dizzy (128K)

static struct BurnRomInfo Specdizzy4RomDesc[] = {
	{ "Dizzy IV - Magicland Dizzy (1989)(Codemasters)[128K].z80", 0x0e107, 0x94e8903f, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy4, Specdizzy4, Spectrum)
STD_ROM_FN(Specdizzy4)

struct BurnDriver BurnSpecdizzy4 = {
	"spec_dizzy4", NULL, "spec_spec128", NULL, "1989",
	"Dizzy IV - Magicland Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy4RomInfo, Specdizzy4RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specdizzy5_48RomInfo, Specdizzy5_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy V - Spellbound Dizzy (128K)

static struct BurnRomInfo Specdizzy5RomDesc[] = {
	{ "Dizzy V - Spellbound Dizzy (1991)(Codemasters)[128K].z80", 0x15e6f, 0x6769bc2e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy5, Specdizzy5, Spectrum)
STD_ROM_FN(Specdizzy5)

struct BurnDriver BurnSpecdizzy5 = {
	"spec_dizzy5", NULL, "spec_spec128", NULL, "1991",
	"Dizzy V - Spellbound Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy5RomInfo, Specdizzy5RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specdizzy6_48RomInfo, Specdizzy6_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy VI - Prince of the Yolkfolk (128K)

static struct BurnRomInfo Specdizzy6RomDesc[] = {
	{ "Dizzy VI - Prince of the Yolkfolk (1991)(Codemasters)[128K].z80", 0x0b6d2, 0x84574abf, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy6, Specdizzy6, Spectrum)
STD_ROM_FN(Specdizzy6)

struct BurnDriver BurnSpecdizzy6 = {
	"spec_dizzy6", NULL, "spec_spec128", NULL, "1991",
	"Dizzy VI - Prince of the Yolkfolk (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy6RomInfo, Specdizzy6RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specdizzy7_48RomInfo, Specdizzy7_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy VII - Crystal Kingdom Dizzy (128K)

static struct BurnRomInfo Specdizzy7RomDesc[] = {
	{ "Dizzy VII - Crystal Kingdom Dizzy (1992)(Codemasters)[128K].z80", 0x0b91d, 0x16fb82f0, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy7, Specdizzy7, Spectrum)
STD_ROM_FN(Specdizzy7)

struct BurnDriver BurnSpecdizzy7 = {
	"spec_dizzy7", NULL, "spec_spec128", NULL, "1992",
	"Dizzy VII - Crystal Kingdom Dizzy (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy7RomInfo, Specdizzy7RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Dizzy VII - Crystal Kingdom Dizzy (Russian) (128K)

static struct BurnRomInfo Specdizzy7rRomDesc[] = {
	{ "Dizzy VII - Crystal Kingdom Dizzy (1993)(Codemasters)(ru)[128K].z80", 0x0f132, 0xd6b0801d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specdizzy7r, Specdizzy7r, Spectrum)
STD_ROM_FN(Specdizzy7r)

struct BurnDriver BurnSpecdizzy7r = {
	"spec_dizzy7r", "spec_dizzy7", "spec_spec128", NULL, "1993",
	"Dizzy VII - Crystal Kingdom Dizzy (Russian) (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specdizzy7rRomInfo, Specdizzy7rRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specfeud_48RomInfo, Specfeud_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Feud (128K)

static struct BurnRomInfo SpecfeudRomDesc[] = {
	{ "Feud (1987)(Bulldog)[128K].z80", 0x09d6f, 0x71ccae18, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specfeud, Specfeud, Spectrum)
STD_ROM_FN(Specfeud)

struct BurnDriver BurnSpecfeud = {
	"spec_feud", NULL, "spec_spec128", NULL, "1987",
	"Feud (128K)\0", NULL, "Bulldog", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecfeudRomInfo, SpecfeudRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
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
	SpectrumGetZipName, SpecfootdireRomInfo, SpecfootdireRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecftmanpRomInfo, SpecftmanpRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecftmanwcRomInfo, SpecftmanwcRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecftmanRomInfo, SpecftmanRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecftmanaRomInfo, SpecftmanaRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specfootmn2eRomInfo, Specfootmn2eRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specfootman2RomInfo, Specfootman2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specfootman3RomInfo, Specfootman3RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecfotyRomInfo, SpecfotyRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specfoty2RomInfo, Specfoty2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
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
	SpectrumGetZipName, SpecglinhtRomInfo, SpecglinhtRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Gary Lineker's Super Skills (128K)

static struct BurnRomInfo SpecglssRomDesc[] = {
	{ "Gary Lineker's Super Skills (1988)(Gremlin Graphics Software)[128K].z80", 0x12e6f, 0xfcb98fd1, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specglss, Specglss, Spectrum)
STD_ROM_FN(Specglss)

struct BurnDriver BurnSpecglss = {
	"spec_glss", NULL, "spec_spec128", NULL, "1988",
	"Gary Lineker's Super Skills (128K)\0", NULL, "Gremlin Graphics Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecglssRomInfo, SpecglssRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecglsssRomInfo, SpecglsssRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specgng_48RomInfo, Specgng_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Ghouls 'n' Ghosts (128K)

static struct BurnRomInfo SpecgngRomDesc[] = {
	{ "Ghouls 'n' Ghosts (1989)(U.S. Gold)[128K].z80", 0x1a8d2, 0x1b626fe8, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specgng, Specgng, Spectrum)
STD_ROM_FN(Specgng)

struct BurnDriver BurnSpecgng = {
	"spec_gng", NULL, "spec_spec128", NULL, "1989",
	"Ghouls 'n' Ghosts (128K)\0", NULL, "U.S. Gold", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecgngRomInfo, SpecgngRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecgreatescRomInfo, SpecgreatescRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Spechowbast_48RomInfo, Spechowbast_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// How to be a Complete Bastard (128K)

static struct BurnRomInfo SpechowbastRomDesc[] = {
	{ "How to be a Complete Bastard (1987)(Virgin Games)[128K].z80", 0x0e728, 0x7460da43, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spechowbast, Spechowbast, Spectrum)
STD_ROM_FN(Spechowbast)

struct BurnDriver BurnSpechowbast = {
	"spec_howbast", NULL, "spec_spec128", NULL, "1987",
	"How to be a Complete Bastard (128K)\0", NULL, "Virgin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpechowbastRomInfo, SpechowbastRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpechunchbacRomInfo, SpechunchbacRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecjetpacRomInfo, SpecjetpacRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpeckickoffRomInfo, SpeckickoffRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Kick Off 2 (128K)

static struct BurnRomInfo Speckickoff2RomDesc[] = {
	{ "Kick Off 2 (1990)(Anco Software)[128K].z80", 0x0be06, 0xc6367c82, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speckickoff2, Speckickoff2, Spectrum)
STD_ROM_FN(Speckickoff2)

struct BurnDriver BurnSpeckickoff2 = {
	"spec_kickoff2", NULL, "spec_spec128", NULL, "1990",
	"Kick Off 2 (128K)\0", NULL, "Anco Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Speckickoff2RomInfo, Speckickoff2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Kick Off World Cup Edition (128K)

static struct BurnRomInfo SpeckickoffwRomDesc[] = {
	{ "Kick Off World Cup Edition (1990)(Anco Software)[128K].z80", 0x09ba6, 0x2a01e70e, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Speckickoffw, Speckickoffw, Spectrum)
STD_ROM_FN(Speckickoffw)

struct BurnDriver BurnSpeckickoffw = {
	"spec_kickoffw", NULL, "spec_spec128", NULL, "1990",
	"Kick Off World Cup Edition (128K)\0", NULL, "Anco Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpeckickoffwRomInfo, SpeckickoffwRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Manchester United Europe (128K)

static struct BurnRomInfo SpecmanutdeuRomDesc[] = {
	{ "Manchester United Europe (1991)(Krisalis Software)(M5)[128K].z80", 0x0fb26, 0xb4146de0, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmanutdeu, Specmanutdeu, Spectrum)
STD_ROM_FN(Specmanutdeu)

struct BurnDriver BurnSpecmanutdeu = {
	"spec_manutdeu", NULL, "spec_spec128", NULL, "1991",
	"Manchester United Europe (128K)\0", NULL, "Krisalis Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmanutdeuRomInfo, SpecmanutdeuRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
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
	SpectrumGetZipName, SpecmatchdayRomInfo, SpecmatchdayRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specmatchdy2RomInfo, Specmatchdy2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Match of the Day (128K)

static struct BurnRomInfo SpecmtchotdRomDesc[] = {
	{ "Match of the Day (1992)(Zeppelin Games)[128K].z80", 0x0cd0a, 0xeb11c05c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmtchotd, Specmtchotd, Spectrum)
STD_ROM_FN(Specmtchotd)

struct BurnDriver BurnSpecmtchotd = {
	"spec_mtchotd", NULL, "spec_spec128", NULL, "1992",
	"Match of the Day (128K)\0", NULL, "Zeppelin Games", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmtchotdRomInfo, SpecmtchotdRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
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
	SpectrumGetZipName, Specmicrsocc_48RomInfo, Specmicrsocc_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Microprose Soccer (128K)

static struct BurnRomInfo SpecmicrsoccRomDesc[] = {
	{ "Microprose Soccer (1989)(Microprose Software)[128K].z80", 0x09c19, 0x432ea0b4, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specmicrsocc, Specmicrsocc, Spectrum)
STD_ROM_FN(Specmicrsocc)

struct BurnDriver BurnSpecmicrsocc = {
	"spec_micrsocc", NULL, "spec_spec128", NULL, "1989",
	"Microprose Soccer (128K)\0", NULL, "Microprose Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecmicrsoccRomInfo, SpecmicrsoccRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecpshandmRomInfo, SpecpshandmRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecpippoRomInfo, SpecpippoRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specrbisland_48RomInfo, Specrbisland_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rainbow Islands - The Story of Bubble Bobble 2 (128K)

static struct BurnRomInfo SpecrbislandRomDesc[] = {
	{ "Rainbow Islands - The Story of Bubble Bobble 2 (1990)(Ocean)[128K].z80", 0x161bb, 0x0211cd1d, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrbisland, Specrbisland, Spectrum)
STD_ROM_FN(Specrbisland)

struct BurnDriver BurnSpecrbisland = {
	"spec_rbisland", NULL, "spec_spec128", NULL, "1990",
	"Rainbow Islands - The Story of Bubble Bobble 2 (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrbislandRomInfo, SpecrbislandRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Specrenegade_48RomInfo, Specrenegade_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Renegade (128K)

static struct BurnRomInfo SpecrenegadeRomDesc[] = {
	{ "Renegade (1987)(Imagine Software)[128K].z80", 0x16f0d, 0xcd930d9a, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrenegade, Specrenegade, Spectrum)
STD_ROM_FN(Specrenegade)

struct BurnDriver BurnSpecrenegade = {
	"spec_renegade", NULL, "spec_spec128", NULL, "1987",
	"Renegade (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrenegadeRomInfo, SpecrenegadeRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Renegade II - Target Renegade (128K)

static struct BurnRomInfo Specrenegad2RomDesc[] = {
	{ "Renegade II - Target Renegade (1988)(Imagine Software)[128K].z80", 0x1a950, 0x25d57e2c, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrenegad2, Specrenegad2, Spectrum)
STD_ROM_FN(Specrenegad2)

struct BurnDriver BurnSpecrenegad2 = {
	"spec_renegad2", NULL, "spec_spec128", NULL, "1988",
	"Renegade II - Target Renegade (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrenegad2RomInfo, Specrenegad2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Renegade III - The Final Chapter (128K)

static struct BurnRomInfo Specrenegad3RomDesc[] = {
	{ "Renegade III - The Final Chapter (1989)(Imagine Software)[128K].z80", 0x18519, 0x45f783f9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrenegad3, Specrenegad3, Spectrum)
STD_ROM_FN(Specrenegad3)

struct BurnDriver BurnSpecrenegad3 = {
	"spec_renegad3", NULL, "spec_spec128", NULL, "1989",
	"Renegade III - The Final Chapter (128K)\0", NULL, "Imagine Software", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrenegad3RomInfo, Specrenegad3RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Robocop (128K)

static struct BurnRomInfo SpecrobocopRomDesc[] = {
	{ "Robocop (1988)(Ocean)[128K].z80", 0x1cbf8, 0xdcc4bf16, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrobocop, Specrobocop, Spectrum)
STD_ROM_FN(Specrobocop)

struct BurnDriver BurnSpecrobocop = {
	"spec_robocop", NULL, "spec_spec128", NULL, "1988",
	"Robocop (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrobocopRomInfo, SpecrobocopRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Robocop 2 (128K)

static struct BurnRomInfo Specrobocop2RomDesc[] = {
	{ "Robocop 2 (1990)(Ocean)[128K].z80", 0x1c73e, 0xe9b44bc7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrobocop2, Specrobocop2, Spectrum)
STD_ROM_FN(Specrobocop2)

struct BurnDriver BurnSpecrobocop2 = {
	"spec_robocop2", NULL, "spec_spec128", NULL, "1990",
	"Robocop 2 (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrobocop2RomInfo, Specrobocop2RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Robocop 3 (128K)

static struct BurnRomInfo Specrobocop3RomDesc[] = {
	{ "Robocop 3 (1992)(Ocean)[128K].z80", 0x1ac8a, 0x21b5c6b7, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrobocop3, Specrobocop3, Spectrum)
STD_ROM_FN(Specrobocop3)

struct BurnDriver BurnSpecrobocop3 = {
	"spec_robocop3", NULL, "spec_spec128", NULL, "1992",
	"Robocop 3 (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, Specrobocop3RomInfo, Specrobocop3RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Rock Star Ate my Hamster (128K)

static struct BurnRomInfo SpecrockshamRomDesc[] = {
	{ "Rock Star Ate my Hamster (1989)(Codemasters)[t][128K].z80", 0x0ffbd, 0xcf617748, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrocksham, Specrocksham, Spectrum)
STD_ROM_FN(Specrocksham)

struct BurnDriver BurnSpecrocksham = {
	"spec_rocksham", NULL, "spec_spec128", NULL, "1989",
	"Rock Star Ate my Hamster (128K)\0", NULL, "Codemasters", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrockshamRomInfo, SpecrockshamRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Run the Gauntlet (128K)

static struct BurnRomInfo SpecrungauntRomDesc[] = {
	{ "Run the Gauntlet (1989)(Ocean)[128K].z80", 0x18211, 0xc3aef7d9, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Specrungaunt, Specrungaunt, Spectrum)
STD_ROM_FN(Specrungaunt)

struct BurnDriver BurnSpecrungaunt = {
	"spec_rungaunt", NULL, "spec_spec128", NULL, "1989",
	"Run the Gauntlet (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpecrungauntRomInfo, SpecrungauntRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
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
	SpectrumGetZipName, SpecskoldazeRomInfo, SpecskoldazeRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, SpecstarfarcRomInfo, SpecstarfarcRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
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
	SpectrumGetZipName, SpectmhtarcRomInfo, SpectmhtarcRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
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
	SpectrumGetZipName, Spectmht_48RomInfo, Spectmht_48RomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80SnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Teenage Mutant Hero Turtles (128K)

static struct BurnRomInfo SpectmhtRomDesc[] = {
	{ "Teenage Mutant Hero Turtles (1990)(Image Works)[128K].z80", 0x0c0b6, 0x66d86001, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectmht, Spectmht, Spectrum)
STD_ROM_FN(Spectmht)

struct BurnDriver BurnSpectmht = {
	"spec_tmht", NULL, "spec_spec128", NULL, "1990",
	"Teenage Mutant Hero Turtles (128K)\0", NULL, "Image Works", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectmhtRomInfo, SpectmhtRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};

// Total Recall (128K)

static struct BurnRomInfo SpectotrcallRomDesc[] = {
	{ "Total Recall (1991)(Ocean)[128K].z80", 0x17197, 0xab3503be, BRF_ESS | BRF_PRG },
};

STDROMPICKEXT(Spectotrcall, Spectotrcall, Spectrum)
STD_ROM_FN(Spectotrcall)

struct BurnDriver BurnSpectotrcall = {
	"spec_totrcall", NULL, "spec_spec128", NULL, "1991",
	"Total Recall (128K)\0", NULL, "Ocean", "ZX Spectrum",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SPECTRUM, GBF_MISC, 0,
	SpectrumGetZipName, SpectotrcallRomInfo, SpectotrcallRomName, NULL, NULL, SpecInputInfo, SpecDIPInfo,
	Z80128KSnapshotInit, SpecExit, SpecFrame, SpecDraw, SpecScan,
	&SpecRecalc, 0x10, 352, 296, 4, 3
};
