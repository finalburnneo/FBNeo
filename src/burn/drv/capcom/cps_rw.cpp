#include "cps.h"
#include "xmvsf3v3.h"
// CPS - Read/Write

// Input bits
#define INP(nnn) UINT8 CpsInp##nnn[8];
CPSINPSET
#undef  INP

// Bytes to return from ports
#define INP(nnn) UINT8 Inp##nnn;
CPSINPSET
#undef  INP

// forgottn, ecofght dials
UINT16 CpsInp055 = 0;
UINT16 CpsInp05d = 0;
INT32 nDial055, nDial05d;
INT32 nDial055_dir, nDial05d_dir;
// forgottn digital rotate
UINT8 CpsDigUD[4] = {0, 0, 0, 0};

// puzloop paddles
INT16 CpsInpPaddle1 = 0;
INT16 CpsInpPaddle2 = 0;
static INT32 ReadPaddle = 0;
INT32 CpsPaddle1Value = 0;
INT32 CpsPaddle2Value = 0;
INT32 CpsPaddle1 = 0;
INT32 CpsPaddle2 = 0;

// ghouls 4-way
static UINT8 nPrevInp000, nPrevInp001;

UINT8 fFakeDip = 0;

static INT32 nRasterLine;

INT32 PangEEP = 0;
INT32 Forgottn = 0;
INT32 Cps1QsHack = 0;
INT32 Kodh = 0;
INT32 Cawingb = 0;
INT32 Sf2thndr = 0;
INT32 Pzloop2 = 0;
INT32 Hkittymp = 0;
INT32 Ssf2tb = 0;
INT32 Dinohunt = 0;
INT32 Port6SoundWrite = 0;
INT32 CpsBootlegEEPROM = 0;
INT32 Cps2Turbo = 0;
INT32 Jurassic99 = 0;
INT32 Dinoh = 0;
INT32 Wofhfh = 0;
INT32 Wofsgzb = 0;
INT32 Wof3js = 0;
INT32 Knightsh = 0;
INT32 Ecofght = 0;

ClearOpposite<4, UINT8> clear_opposite;

CpsRWSoundCommandCallback CpsRWSoundCommandCallbackFunction = NULL;

static INT32 nCalc[2] = {0, 0};

static const bool nCPSExtraNVRAM = false;
static INT32 n664001;

#define INP(nnnn) UINT8 CpsInp##nnnn[8];
CPSINPEX
#undef  INP

#define INP(nnnn) static UINT8 Inp##nnnn;
CPSINPEX
#undef  INP

void CpsRwScan()
{
	if (Hkittymp) {
		SCAN_VAR(CpsPaddle1Value);
		SCAN_VAR(CpsPaddle1);
	}

	if (Pzloop2) {
		SCAN_VAR(ReadPaddle);
		SCAN_VAR(CpsPaddle1Value);
		SCAN_VAR(CpsPaddle2Value);
		SCAN_VAR(CpsPaddle1);
		SCAN_VAR(CpsPaddle2);
	}

	if (Forgottn) {
		SCAN_VAR(nDial055);
		SCAN_VAR(nDial05d);
	}

	if (Ecofght) {
		SCAN_VAR(nDial055);
		SCAN_VAR(nDial05d);
		SCAN_VAR(nDial055_dir);
		SCAN_VAR(nDial05d_dir);
	}

	if (Ghouls) {
		SCAN_VAR(nPrevInp000);
		SCAN_VAR(nPrevInp001);
	}

	clear_opposite.scan();

	SCAN_VAR(n664001);
	SCAN_VAR(nCalc);
	SCAN_VAR(nRasterLine);
}

static UINT8 ecofght_readpaddle(UINT8 data, INT32 plr)
{
	if (ReadPaddle) {
		switch (plr) {
			case 0: return nDial055 & 0xff;
			case 1: return nDial05d & 0xff;
		}
	} else {
		data &= ~0x20;
		switch (plr) {
			case 0: data |= nDial055_dir << 5; break;
			case 1: data |= nDial05d_dir << 5; break;
		}
	}
	return data;
}
// Read input port 0x000-0x1ff
static UINT8 CpsReadPort(const UINT32 ia)
{
	UINT8 d = 0xFF;

	if (ia == 0x000) {
		d = (UINT8)~Inp000;
		if (Ecofght && (~fFakeDip & 0x20)) {
			d = ecofght_readpaddle(d, 1);
		}
		if (Pzloop2) {
			if (ReadPaddle) {
				d -= CpsPaddle2Value;
			} else {
				d = CpsPaddle2;
			}
		}
		return d;
	}
	if (ia == 0x001) {
		d = (UINT8)~Inp001;
		if (Ecofght && (~fFakeDip & 0x20)) {
			d = ecofght_readpaddle(d, 0);
		}
		if (Pzloop2) {
			if (ReadPaddle) {
				d -= CpsPaddle1Value;
			} else {
				d = CpsPaddle1;
			}
		}
		if (Hkittymp) {
			const UINT8 quadradic_sequence[4] = { 0, 1, 3, 2 };
			d = quadradic_sequence[(CpsPaddle1 / 8) & 3];

			d = (~d & 3) | (~Inp001 & ~3); // add bit 0,1 from quadradic encoder, rest from inputs
		}
		return d;
	}
	if (ia == 0x010) {
		d = (UINT8)~Inp010;
		return d;
	}
	if (ia == 0x011) {
		d = (UINT8)~Inp011;
		if (Ecofght && (~fFakeDip & 0x20)) d &= 0xef; // select analog spinner
		return d;
	}
	if (ia == 0x012) {
		d = (UINT8)~Inp012;
		return d;
	}
	if (ia == 0x018) {
		d = (UINT8)~Inp018;
		return d;
	}
	if (ia == 0x019) {
		d = (UINT8)~Inp019;
		return d;
	}
	if (ia == 0x01B) {
		d = (UINT8)~Inp01B;
		return d;
	}
	
	if (ia == 0x01A) {
		d = (UINT8)~Cpi01A;
		return d;
	}
	if (ia == 0x01C) {
		d = (UINT8)~Cpi01C;
		return d;
	}
	if (ia == 0x01E) {
		d = (UINT8)~Cpi01E;
		return d;
	}

	if (Cps == 2) {
		// Used on CPS2 only I think
		if (ia == 0x020) {
			d = (UINT8)~Inp020;
			return d;
		}
		if (ia == 0x021) {
			d = (UINT8)~Inp021;
			d &= 0xFE;
			d |= EEPROMRead();
			return d;
		}

		// CPS2 Volume control
		if (ia == 0x030) {
			if (Ssf2tb) {
				d = 0x20;
			} else {
				d = Cps2VolumeStates[Cps2Volume] >> 8;
				if (Cps2DisableDigitalVolume) d = 0xd0;
			}
			return d;
		}
		if (ia == 0x031) {
			d = Cps2VolumeStates[Cps2Volume] & 0xff;
			return d;
		}

		if (ia >= 0x0100 && ia < 0x0200) {
//			bprintf(PRINT_NORMAL, _T("  - port 0x%02X (%3i)\n"), ia & 255, SekCurrentScanline());

			// The linecounters seem to return the line at which the last IRQ triggered by this counter is scheduled minus the current line
			if ((ia & 0x0FE) == 0x50) {
				if ((ia & 1) == 0) {
					nRasterLine = nIrqLine50 - SekCurrentScanline();
					return nRasterLine >> 8;
				} else {
					return nRasterLine & 0xFF;
				}
			}
			if ((ia & 0x0FE) == 0x52) {
				if ((ia & 1) == 0) {
					nRasterLine = nIrqLine52 - SekCurrentScanline();
					return nRasterLine >> 8;
				} else {
					return nRasterLine & 0xFF;
				}
			}

		}
	} else {
		// Board ID
		if (ia == 0x100 + CpsBID[0]) {
			d = (UINT8)CpsBID[1];
			return d;
		}
		if (ia == 0x100 + (CpsBID[0] + 1)) {
			d = (UINT8)CpsBID[2];
			return d;
		}
		
		if (Sf2thndr) {
			// this reads the B-ID from here on startup as well as the normal location in-game
			if (ia == 0x1c8) {
				d = (UINT8)CpsBID[1];
				return d;
			}
		
			if (ia == 0x1c9) {
				d = (UINT8)CpsBID[2];
				return d;
			}
		}
		
		// CPS1 EEPROM read
		if (ia == 0xC007) {
			if (Cps1Qs || CpsBootlegEEPROM) {
				return EEPROMRead();
			} else {
				return 0;
			}
		}
		
		// Pang3 EEPROM
		if (PangEEP == 1) {
			if (ia == 0x17B) {
				return EEPROMRead();
			}
		}
		
		// Extra Input ports (move from game-to-game)
		if (ia == 0x006) {
			d = (UINT8)~Inp006;
			return d;
		}
		if (ia == 0x007) {
			d = (UINT8)~Inp007;
			return d;
		}
		if (ia == 0x008) {
			d = (UINT8)~Inp008;
			return d;
		}
		if (ia == 0x029) {
			d = (UINT8)~Inp029;
			return d;
		}		
		if (ia == 0x176) {
			d = (UINT8)~Inp176;
			return d;
		}
		if (ia == 0x177) {
			d = (UINT8)~Inp177;
			return d;
		}		
		if (ia == 0x179) {
			d = (UINT8)~Inp179;
			return d;
		}
		if (ia == 0x186) {
			d = (UINT8)~Inp186;
			return d;
		}		
		if (ia == 0x1fd) {
			d = (UINT8)~Inp1fd;
			return d;
		}		
		if (ia == 0xC000) {
			d = (UINT8)~Inpc000;
			return d;
		}
		if (ia == 0xC001) {
			d = (UINT8)~Inpc001;
			return d;
		}
		if (ia == 0xC002) {
			d = (UINT8)~Inpc002;
			return d;
		}
		if (ia == 0xC003) {
			d = (UINT8)~Inpc003;
			return d;
		}
		
		// Forgotten Worlds Dial
		if (Forgottn) {
			if (ia == 0x053) {
				return (nDial055 >> 0) & 0xff;
			}
			if (ia == 0x055) {
				return (nDial055 >> 8) & 0x0f;
			}
			if (ia == 0x05B) {
				return (nDial05d >> 0) & 0xff;
			}
			if (ia == 0x05D) {
				return (nDial05d >> 8) & 0x0f;
			}
		}
	}
	
//	bprintf(PRINT_NORMAL, _T("Read Port %x\n"), ia);

	return d;
}

// Write output port 0x000-0x1ff
void CpsWritePort(const UINT32 ia, UINT8 d)
{
	if ((Cps & 1) && Cps1Qs == 0) {
		if (!Cps1DisablePSnd) {
			// CPS1 sound code
			if (ia == 0x181 || (Port6SoundWrite && (ia == 0x006 || ia == 0x007))) {
				PsndSyncZ80((INT64)SekTotalCycles() * nCpsZ80Cycles / nCpsCycles);

				PsndCode = d;
				return;
			}

			// CPS1 sound fade
			if (ia == 0x189) {
				PsndSyncZ80((INT64)SekTotalCycles() * nCpsZ80Cycles / nCpsCycles);

				PsndFade = d;
				return;
			}
		} else {
			if (ia == 0x181 || (Port6SoundWrite && (ia == 0x006 || ia == 0x007))) {
				if (CpsRWSoundCommandCallbackFunction) {
					CpsRWSoundCommandCallbackFunction(d);
				}
			}
		}

		if (ia == 0x041) {
			nDial055 = 0;
		}
		if (ia == 0x049) {
			nDial05d = 0;
		}
	}

	if (Cps == 1 && Cps1QsHack == 1) {
		if (ia == 0x181) {
			// Pass the Sound Code to the Q-Sound Shared Ram
			CpsZRamC0[0x001] = d;
		}
	}

	// CPS registers
	if (ia >= 0x100 && ia < 0x200) {
		//Pang3 EEPROM
		if (PangEEP == 1 && ia == 0x17B) {
			EEPROMWrite(d & 0x40, d & 0x80, d & 0x01);
			return;
		}
		CpsReg[(ia ^ 1) & 0xFF] = d;
		
		if (ia == 0x10b) {
			GetPalette(0, 6);
			CpsPalUpdate(CpsSavePal);
		}
		return;
	}

	if (Cps == 2) {
		if (ia == 0x40) {
			EEPROMWrite(d & 0x20, d& 0x40, d & 0x10);

			if (Ecofght) {
				ReadPaddle = d & 0x01;
			}
			return;
		}

		// CPS2 object bank select
		if ((ia & 0x1FF) == 0x0E1) {
//			bprintf(PRINT_NORMAL, _T("  - %2i (%3i)\n"), d & 1, SekCurrentScanline());
//			CpsObjGet();
			CpsMapObjectBanks(d & 1);
			return;
		}
		
		if (ia == 0x41 && Pzloop2) {
			ReadPaddle = d & 0x02;
		}
	}

	if (Cps1Qs == 1 || CpsBootlegEEPROM) {
		//CPS1 EEPROM write
		if (ia == 0xc007) {
			EEPROMWrite(d & 0x40, d & 0x80, d & 0x01);
			return;
		}
	}
	
//	bprintf(PRINT_NORMAL, _T("Write Port %x, %x\n"), ia, d);
}

// X-Men Vs. Street Fighter 3v3 + DHC board additions (see xmvsf3v3.h), enabled by its drivers only
UINT8 CpsXmvSixButtons[XMV6_PLAYERS][13];
INT32 CpsXmvSixEnabled = 0;
INT32 CpsXmvWide = 0;
static UINT16 CpsXmvSixCapabilities = 0;
static XmvSixInputState CpsXmvSixState;
static UINT8 *CpsXmvSixWorkRam = NULL;

INT32 CpsXmvSixInit(INT32 bWide)
{
	CpsXmvSixWorkRam = (UINT8*)BurnMalloc(XMV6_WORK_RAM_BYTES);
	if (CpsXmvSixWorkRam == NULL) {
		return 1;
	}

	CpsXmvWide = bWide ? 1 : 0;
	CpsXmvSixCapabilities = XMV6_CAP_WORK_RAM | XMV6_CAP_PORTRAITS | XMV6_CAP_PORTRAITS_6 | XMV6_CAP_PORTRAITS_12 | XMV6_CAP_PORTRAITS_18;
	if (bWide) {
		CpsXmvSixCapabilities |= XMV6_CAP_WIDE | XMV6_CAP_WIDE_SCROLL1;
	}
	CpsXmvSixEnabled = 1;

	memset(CpsXmvSixButtons, 0, sizeof(CpsXmvSixButtons));
	CpsXmvSixReset();

	return 0;
}

void CpsXmvSixExit()
{
	CpsXmvSixEnabled = 0;
	CpsXmvWide = 0;
	BurnFree(CpsXmvSixWorkRam);
}

INT32 CpsXmvSixReset()
{
	if (!CpsXmvSixEnabled) return 0;

	XmvSixReset(&CpsXmvSixState);
	memset(CpsXmvSixWorkRam, 0, XMV6_WORK_RAM_BYTES);

	return 0;
}

INT32 CpsXmvSixScan(INT32 nAction, INT32* /*pnMin*/)
{
	if (!CpsXmvSixEnabled) return 0;

	if (nAction & ACB_MEMORY_RAM) {
		struct BurnArea ba;
		memset(&ba, 0, sizeof(ba));
		ba.Data		= CpsXmvSixWorkRam;
		ba.nLen		= XMV6_WORK_RAM_BYTES;
		ba.nAddress	= XMV6_WORK_RAM_BASE;
		ba.szName	= "XMvsSF 3v3 work RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(CpsXmvSixState);

		if (nAction & ACB_WRITE) {
			if (CpsXmvSixState.abi != XMV6_VERSION || CpsXmvSixState.count > XMV6_DEPTH) {
				// not a state of this board
				CpsXmvSixReset();
				return 1;
			}
		}
	}

	return CpsXmvVisualScan(nAction);
}

// Wide screen: while the ROM keeps 'XS1X' at 0x813ff8, scroll1 columns 64-127 come from 0x814000
const UINT8* CpsXmvScroll1Extension()
{
	if (!CpsXmvSixEnabled || !CpsXmvWide) return NULL;

	const UINT8* tag = CpsXmvSixWorkRam + 0x3ff8;
	if (tag[0] != 'X' || tag[1] != 'S' || tag[2] != '1' || tag[3] != 'X') return NULL;

	return CpsXmvSixWorkRam + 0x4000;
}

// Once a frame, before the input ports are compiled
static void CpsXmvSixSample()
{
	UINT16 raw[XMV6_PLAYERS];
	for (INT32 p = 0; p < XMV6_PLAYERS; p++) {
		raw[p] = 0;
		for (INT32 bit = 0; bit < 13; bit++) {
			raw[p] |= (CpsXmvSixButtons[p][bit] & 1) << bit;
		}
	}
	XmvSixSample(&CpsXmvSixState, raw);

	// P1 and P2 also reach the stock input ports, for the game's own menus
	for (INT32 bit = 0; bit < 7; bit++) {
		CpsInp001[bit] = (CpsXmvSixState.held[0] >> bit) & 1;
		CpsInp000[bit] = (CpsXmvSixState.held[1] >> bit) & 1;
	}
	for (INT32 p = 0; p < 2; p++) {
		UINT16 value = CpsXmvSixState.held[p];
		CpsInp011[p * 4 + 0] = (value >>  8) & 1;	// weak kick
		CpsInp011[p * 4 + 1] = (value >>  9) & 1;	// medium kick
		CpsInp020[p + 0]     = (value >> 11) & 1;	// start
		CpsInp020[p + 4]     = (value >> 12) & 1;	// coin
	}
	CpsInp011[2] = (CpsXmvSixState.held[0] >> 10) & 1;	// P1 strong kick
	CpsInp020[6] = (CpsXmvSixState.held[1] >> 10) & 1;	// P2 strong kick
}

UINT8 __fastcall CpsReadByte(UINT32 a)
{
	if (CpsXmvSixEnabled) {
		if (a >= XMV6_BASE && a < XMV6_BASE + XMV6_WINDOW_BYTES) {
			return XmvSixReadByte(&CpsXmvSixState, CpsXmvSixCapabilities, a - XMV6_BASE);
		}
		if (a >= XMV6_WORK_RAM_BASE && a < XMV6_WORK_RAM_BASE + XMV6_WORK_RAM_BYTES) {
			return CpsXmvSixWorkRam[a - XMV6_WORK_RAM_BASE];
		}
	}

	// Input ports mirrored between 0x800000 and 0x807fff
	if ((a & 0xFF8000) == 0x800000) {
		return CpsReadPort(a & 0x1FF);
	}

	if (Cps == 2) {
		if ((a & 0xFF8000) == 0x660000) {
			if (a == 0x664001) {
				return n664001;
			}
		}

		return 0x00;
	}

	if (a >= 0xF1C000 && a <= 0xF1C007) {
		return CpsReadPort(a & 0xC00F);
	}
	
	if (Dinohunt && a == 0xfc0001) return (UINT8)~Inpc001;
	
//	bprintf(PRINT_NORMAL, _T("Read Byte %x\n"), a);
	
	return 0x00;
}

void __fastcall CpsWriteByte(UINT32 a,UINT8 d)
{
	if (CpsXmvSixEnabled && a >= XMV6_WORK_RAM_BASE && a < XMV6_WORK_RAM_BASE + XMV6_WORK_RAM_BYTES) {
		CpsXmvSixWorkRam[a - XMV6_WORK_RAM_BASE] = d;
		return;
	}

	// Output ports mirrored between 0x800000 and 0x807fff
	if ((a & 0xFF8000) == 0x800000) {
		CpsWritePort(a & 0x1FF, d);
		return;
	}
	
	if (Cps == 2) {
		// 0x400000 registers
		if (Cps2Turbo) {
			if ((a & 0xFFFFF0) == 0x665000)	{
				CpsFrg[a & 0x0F] = d;
				return;
			}
			if ((a & 0xFFFFF0) == 0xfffff0)	{
				CpsFrg[a & 0x0F] = d;
				return;
			}
		} else {
			if ((a & 0xFFFFF0) == 0x400000)	{
				CpsFrg[a & 0x0F] = d;
				return;
			}
		}
		if ((a & 0xFF8000) == 0x660000) {
			if (a == 0x664001) {
				// bit 1 toggled on/off each frame
				n664001 = d;
			}
			
			return;
		}

		return;
	}
	
	if (Cps1Qs == 1 || CpsBootlegEEPROM) {
		// CPS1 EEPROM
		if (a == 0xf1c007) {
			CpsWritePort(a & 0xC00F, d);
			return;
		}
	}
	
//	bprintf(PRINT_NORMAL, _T("Write Byte %x, %x\n"), a, d);
}

UINT16 __fastcall CpsReadWord(UINT32 a)
{
	if (CpsXmvSixEnabled) {
		if (a >= XMV6_BASE && a < XMV6_BASE + XMV6_WINDOW_BYTES) {
			return XmvSixReadWord(&CpsXmvSixState, CpsXmvSixCapabilities, a - XMV6_BASE);
		}
		if (a >= XMV6_WORK_RAM_BASE && a < XMV6_WORK_RAM_BASE + XMV6_WORK_RAM_BYTES) {
			return XmvSixRamReadWord(CpsXmvSixWorkRam, a - XMV6_WORK_RAM_BASE);
		}
	}

	if ((a & 0xFF8FFF) == 0x800100 + CpsMProt[3]) {
		return (UINT16)((nCalc[0] * nCalc[1]) >> 16);
	}
	// ports mirrored between 0x800000 and 0x807fff
	if ((a & 0xFF8FFF) == 0x800100 + CpsMProt[2]) {
		return (UINT16)((nCalc[0] * nCalc[1]));
	}
	
//	bprintf(PRINT_NORMAL, _T("Read Word %x\n"), a);

	SEK_DEF_READ_WORD(0, a);
}

void __fastcall CpsWriteWord(UINT32 a, UINT16 d)
{
	if (CpsXmvSixEnabled && a >= XMV6_WORK_RAM_BASE && a < XMV6_WORK_RAM_BASE + XMV6_WORK_RAM_BYTES) {
		XmvSixRamWriteWord(CpsXmvSixWorkRam, a - XMV6_WORK_RAM_BASE, d);
		return;
	}

	// ports mirrored between 0x800000 and 0x807fff
	if ((a & 0xFF8FFF) == 0x800100 + CpsMProt[0])
		nCalc[0] = d;
	if ((a & 0xFF8FFF) == 0x800100 + CpsMProt[1])
		nCalc[1] = d;

	if (a == 0x804040) {
		if ((d & 0x0008) == 0) {
			if (!Cps2DisableQSnd) ZetReset();
		}
	}
	
//	bprintf(PRINT_NORMAL, _T("Write Word %x, %x\n"), a, d);
	
	SEK_DEF_WRITE_WORD(0, a, d);
}

// Reset all inputs to zero
static INT32 InpBlank()
{
#define INP(nnn) Inp##nnn = 0; memset(CpsInp##nnn, 0, sizeof(CpsInp##nnn));
	CPSINPSET
#undef INP

#define INP(nnnn) Inp##nnnn = 0; memset(CpsInp##nnnn, 0, sizeof(CpsInp##nnnn));
	CPSINPEX
#undef INP

	CpsInp055 = CpsInp05d = 0;
	memset(CpsDigUD, 0, sizeof(CpsDigUD));

	return 0;
}

INT32 CpsRwInit()
{
	InpBlank();
	return 0;
}

INT32 CpsRwExit()
{
	InpBlank();
	CpsRWSoundCommandCallbackFunction = NULL;
	return 0;
}

INT32 CpsRwGetInp()
{
	if (CpsXmvSixEnabled) {
		CpsXmvSixSample();
	}

	// Compile separate buttons into Inpxxx
#define INP(nnn) \
  { INT32 i = 0; Inp##nnn = 0; \
    for (i = 0; i < 8; i++) { Inp##nnn |= (CpsInp##nnn[i] & 1) << i; }  }
	CPSINPSET
#undef INP


#define INP(nnnn) \
  { INT32 i = 0; Inp##nnnn = 0; \
    for (i = 0; i < 8; i++) { Inp##nnnn |= (CpsInp##nnnn[i] & 1) << i; }  }
	CPSINPEX
#undef INP

	if (Jurassic99)
		Inpc001 |= (CpsInp179[0] & 1) << 7;
	if (Wofsgzb)
		Inpc001 |= (CpsInp179[0] & 1) << 6;
	if (Dinoh)
		Inp177  |= (CpsInp179[0] & 1) << 7;
	if (Wofhfh || Wof3js || Knightsh)
		Inp177  |= (CpsInp179[0] & 1) << 6;

	if (Forgottn) {
		// Handle analog controls
		if (fFakeDip & 0x80) {
			if (CpsDigUD[0]) nDial055 += 0x40; // p1
			if (CpsDigUD[1]) nDial055 -= 0x40;
			if (CpsDigUD[2]) nDial05d += 0x40; // p2
			if (CpsDigUD[3]) nDial05d -= 0x40;
		} else {
			if (CpsDigUD[0]) nDial055 -= 0x40; // p1
			if (CpsDigUD[1]) nDial055 += 0x40;
			if (CpsDigUD[2]) nDial05d -= 0x40; // p2
			if (CpsDigUD[3]) nDial05d += 0x40;
		}
	}

	if (Pzloop2) {
		if (ReadPaddle) {
			CpsPaddle1Value = 0;
			CpsPaddle2Value = 0;
			if (CpsInpPaddle1) {
				if (CpsInpPaddle1 > 0x3fe) {
					CpsPaddle1Value = 2;
				}
	
				if (CpsInpPaddle1 < -0x3fe) {
					CpsPaddle1Value = 1;
				}
			}
			
			if (CpsInpPaddle2) {
				if (CpsInpPaddle2 > 0x3fe) {
					CpsPaddle2Value = 2;
				}
	
				if (CpsInpPaddle2 < -0x3fe) {
					CpsPaddle2Value = 1;
				}
			}
		}
		
		CpsPaddle1 += CpsInpPaddle1 / 0x100;
		CpsPaddle2 += CpsInpPaddle2 / 0x100;
	}

	if (Hkittymp) {
		// In new drivers I'd use my paddle device, but..
		// This crude analog handling was in fb for probably 20 years now,
		// Let's stay with it, just for fun! -dink

		// Clamp at +- 0x400 (-0x400 to +0x3ff, range of typical analog thumbstick)
		if (CpsInpPaddle1 >  0x3fe) CpsInpPaddle1 =  0x400;
		if (CpsInpPaddle1 < -0x3fe) CpsInpPaddle1 = -0x400;

		CpsPaddle1 += CpsInpPaddle1 / 0x80; // add +-8 maximum to paddle-accumulator
	}

	clear_opposite.check(0, Inp000, 0x08, 0x04, 0x02, 0x01, nSocd[0]);
	clear_opposite.check(1, Inp001, 0x08, 0x04, 0x02, 0x01, nSocd[1]);

	// Ghouls uses a 4-way stick
	if (Ghouls) {
		if (fFakeDip & 1) {
			if ((Inp000 & 0xf) & ((Inp000 & 0xf) - 1)) {
				Inp000 = (Inp000 & ~0xf) | (nPrevInp000 & 0xf);
			}
			nPrevInp000 = Inp000;

			if ((Inp001 & 0xf) & ((Inp001 & 0xf) - 1)) {
				Inp001 = (Inp001 & ~0xf) | (nPrevInp001 & 0xf);
			}
			nPrevInp001 = Inp001;
		} else {
			if ((Inp000 & 0x03) && (Inp000 & 0x0C)) {
				Inp000 ^= (nPrevInp000 & 0x0F);
			} else {
				nPrevInp000 = Inp000;
			}

			if ((Inp001 & 0x03) && (Inp001 & 0x0C)) {
				Inp001 ^= (nPrevInp001 & 0x0F);
			} else {
				nPrevInp001 = Inp001;
			}
		}
	}

	// xmvsf3v3: players 3-6 never reach the stock ports (Inp011 is P1/P2's kicks there), and
	// CpsXmvSixSample() has already cleaned every player's opposite directions
	if (nMaxPlayers > 2 && !CpsXmvSixEnabled) {
		if (Cps == 2) {
			clear_opposite.check(2, Inp011, 0x08, 0x04, 0x02, 0x01, nSocd[2]);
			if (nMaxPlayers == 4) {
				clear_opposite.check(3, Inp010, 0x08, 0x04, 0x02, 0x01, nSocd[3]);
			}
		} else {
			clear_opposite.check(2, Inp177, 0x08, 0x04, 0x02, 0x01, nSocd[2]);
			if (nMaxPlayers == 4) {
				clear_opposite.check(3, Inp179, 0x08, 0x04, 0x02, 0x01, nSocd[3]);
			}
			if (Cps1Qs) {
				clear_opposite.check(2, Inpc001, 0x08, 0x04, 0x02, 0x01, nSocd[2]);
				if (nMaxPlayers == 4) {
					clear_opposite.check(3, Inpc003, 0x08, 0x04, 0x02, 0x01, nSocd[3]);
				}
			}
		}
	}

	return 0;
}

void CpsSoundCmd(UINT16 sound_code) {
//	CpsWritePort(0x181, sound_code);
	PsndCode = sound_code;
}
