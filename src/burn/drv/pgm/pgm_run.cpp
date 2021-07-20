
#include "pgm.h"
#include "v3021.h"
#include "ics2115.h"
#include "timer.h"

UINT8 PgmJoy1[8] = {0,0,0,0,0,0,0,0};
UINT8 PgmJoy2[8] = {0,0,0,0,0,0,0,0};
UINT8 PgmJoy3[8] = {0,0,0,0,0,0,0,0};
UINT8 PgmJoy4[8] = {0,0,0,0,0,0,0,0};
UINT8 PgmBtn1[8] = {0,0,0,0,0,0,0,0};
UINT8 PgmBtn2[8] = {0,0,0,0,0,0,0,0};
UINT8 PgmInput[9] = {0,0,0,0,0,0,0,0,0};
UINT8 PgmCoins = 0; // coin inputs (for hold logic)
UINT8 PgmReset = 0;
static INT32 hold_coin[4];

INT32 nPGM68KROMLen = 0;
INT32 nPGMTileROMLen = 0;
INT32 nPGMSPRColROMLen = 0;
INT32 nPGMSPRMaskROMLen = 0;
INT32 nPGMSNDROMLen = 0;
INT32 nPGMSPRColMaskLen = 0;
INT32 nPGMSPRMaskMaskLen = 0;
INT32 nPGMExternalARMLen = 0;

UINT32 *PGMBgRAM;
UINT32 *PGMTxtRAM;
UINT32 *RamCurPal;
UINT16 *PGMRowRAM;
UINT16 *PGMPalRAM;
UINT16 *PGMVidReg;
UINT16 *PGMSprBuf;
static UINT8 *RamZ80;
UINT8 *PGM68KRAM;

static UINT16 nSoundlatch[3] = {0, 0, 0};
static UINT8 bSoundlatchRead[3] = {0, 0, 0};

static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;

UINT8 *PGM68KBIOS, *PGM68KROM, *PGMTileROM, *PGMTileROMExp, *PGMSPRColROM, *PGMSPRMaskROM, *PGMARMROM;
UINT8 *PGMARMRAM0, *PGMUSER0, *PGMARMRAM1, *PGMARMRAM2, *PGMARMShareRAM, *PGMARMShareRAM2, *PGMProtROM;

UINT8 *ICSSNDROM;

UINT8 nPgmPalRecalc = 0;
static INT32 nPgmCurrentBios = -1;

void (*pPgmResetCallback)() = NULL;
void (*pPgmInitCallback)() = NULL;
void (*pPgmTileDecryptCallback)(UINT8 *gfx, INT32 len) = NULL;
void (*pPgmColorDataDecryptcallback)(UINT8 *gfx, INT32 len) = NULL;
void (*pPgmProtCallback)() = NULL;
INT32 (*pPgmScanCallback)(INT32, INT32*) = NULL;

static INT32 nEnableArm7 = 0;
INT32 nPGMDisableIRQ4 = 0;
INT32 nPGMArm7Type = 0;
UINT32 nPgmAsicRegionHackAddress = 0;

INT32 pgm_cave_refresh = 0;

#define M68K_FREQ  20000000
#define Z80_FREQ   8468000

#define M68K_CYCS_PER_FRAME	((M68K_FREQ * 100) / nBurnFPS)
#define ARM7_CYCS_PER_FRAME	((20000000 * 100) / nBurnFPS)
#define Z80_CYCS_PER_FRAME	(( Z80_FREQ * 100) / nBurnFPS)

#define	PGM_INTER_LEAVE	200

#define M68K_CYCS_PER_INTER	(M68K_CYCS_PER_FRAME / PGM_INTER_LEAVE)
#define ARM7_CYCS_PER_INTER	(ARM7_CYCS_PER_FRAME / PGM_INTER_LEAVE)
#define Z80_CYCS_PER_INTER	(Z80_CYCS_PER_FRAME  / PGM_INTER_LEAVE)

static INT32 nCyclesDone[3];
static INT32 nCyclesTotal[3];

static INT32 pgmMemIndex()
{
	UINT8 *Next; Next = Mem;
	PGM68KBIOS			= Next; Next += 0x0080000;
	PGM68KROM			= Next; Next += nPGM68KROMLen;

	PGMUSER0			= Next; Next += nPGMExternalARMLen;

	PGMProtROM			= PGMUSER0 + 0x10000; // Olds, Killbld, drgw3

	if (BurnDrvGetHardwareCode() & HARDWARE_IGS_USE_ARM_CPU) {
		PGMARMROM		= Next; Next += 0x0004000;
	}

	RamStart			= Next;

	PGM68KRAM			= Next; Next += 0x0020000;
	RamZ80				= Next; Next += 0x0010000;

	if (BurnDrvGetHardwareCode() & HARDWARE_IGS_USE_ARM_CPU) {
		PGMARMShareRAM	= Next; Next += 0x0010000;
		PGMARMShareRAM2	= Next; Next += 0x0010000;
		PGMARMRAM0		= Next; Next += 0x0001000; // minimum map is 0x1000 - should be 0x400
		PGMARMRAM1		= Next; Next += 0x0040000;
		PGMARMRAM2		= Next; Next += 0x0001000; // minimum map is 0x1000 - should be 0x400
	}

	PGMBgRAM			= (UINT32 *) Next; Next += 0x0001000;
	PGMTxtRAM			= (UINT32 *) Next; Next += 0x0002000;

	PGMRowRAM			= (UINT16 *) Next; Next += 0x0001000;	// Row Scroll
	PGMPalRAM			= (UINT16 *) Next; Next += 0x0001400;	// Palette R5G5B5
	PGMVidReg			= (UINT16 *) Next; Next += 0x0010000;	// Video Regs inc. Zoom Table
	PGMSprBuf			= (UINT16 *) Next; Next += 0x0000a00;

	RamEnd				= Next;

	RamCurPal			= (UINT32 *) Next; Next += (0x0001204 / 2) * sizeof(UINT32);

	MemEnd				= Next;

	return 0;
}

static INT32 pgmGetRoms(bool bLoad)
{
	INT32 kov2 = (strncmp(BurnDrvGetTextA(DRV_NAME), "kov2", 4) == 0) ? 1 : 0;

	char* pRomName;
	struct BurnRomInfo ri;
	struct BurnRomInfo pi;

	UINT8 *PGM68KROMLoad = PGM68KROM;
	UINT8 *PGMUSER0Load = PGMUSER0;
	UINT8 *PGMTileROMLoad = PGMTileROM + 0x180000;
	UINT8 *PGMSPRMaskROMLoad = PGMSPRMaskROM;
	UINT8 *PGMSNDROMLoad = ICSSNDROM + (kov2 ? 0x800000 : 0x400000);

	if (bLoad && nPGM68KROMLen == 0x80000 && nPGMSNDROMLen == 0x600000) { // dw2001 & dwpc
		PGMSNDROMLoad -= 0x200000;
	}

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {

	//	bprintf (0, _T("Loading ROM #%d\n"), i);

		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 1)
		{
			if (bLoad) {
				BurnDrvGetRomInfo(&pi, i+1);

				if (ri.nLen == 0x80000 && pi.nLen == 0x80000)
				{
					BurnLoadRom(PGM68KROMLoad + 0, i + 0, 2);
					BurnLoadRom(PGM68KROMLoad + 1, i + 1, 2);
					PGM68KROMLoad += pi.nLen;
					i += 1;
				}
				else
				{
					BurnLoadRom(PGM68KROMLoad, i, 1);
				}
			}
			PGM68KROMLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 2)
		{
			if (bLoad) BurnLoadRom(PGMTileROMLoad, i, 1);
			PGMTileROMLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 3)
		{
			if (!bLoad) nPGMSPRColROMLen += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 4)
		{
			if ((PGMSPRMaskROMLoad - PGMSPRMaskROM) == 0x1000000 && ri.nLen == 0x200000) { // pgm3in1
				PGMSPRMaskROMLoad -= 0x100000;
			}

			if (bLoad) BurnLoadRom(PGMSPRMaskROMLoad, i, 1);
			PGMSPRMaskROMLoad += ri.nLen;

			if ((PGMSPRMaskROMLoad - PGMSPRMaskROM) == 0x1000000 && ri.nLen == 0x200000) { // pgm3in1
				PGMSPRMaskROMLoad -= 0x100000;
			}

			continue;
		}

		if ((ri.nType & BRF_SND) && (ri.nType & 0x0f) == 5)
		{
			if (bLoad) BurnLoadRom(PGMSNDROMLoad, i, 1);
			PGMSNDROMLoad += ri.nLen;
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 7)
		{
			if (bLoad) {
				if (BurnDrvGetHardwareCode() & HARDWARE_IGS_USE_ARM_CPU) {
					BurnLoadRom(PGMARMROM + ((ri.nLen == 0x3e78) ? 0x188 : 0), i, 1);
				}
			}
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 8)
		{
			if (BurnDrvGetHardwareCode() & HARDWARE_IGS_USE_ARM_CPU) {
				if (bLoad) BurnLoadRom(PGMUSER0Load, i, 1);
				PGMUSER0Load += ri.nLen;
			}
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 9)
		{
			if (bLoad) {
				BurnLoadRom(PGMProtROM, i, 1);
			}
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 0xa)
		{ // nvram
			if (bLoad) {
				BurnLoadRom(PGM68KRAM, i, 1);
			}
		}
	}

	if (!bLoad) {
		nPGM68KROMLen = PGM68KROMLoad - PGM68KROM;

		nPGMTileROMLen = PGMTileROMLoad - PGMTileROM;
		if (nPGMTileROMLen < 0x400000) nPGMTileROMLen = 0x400000;

		nPGMSPRMaskROMLen = PGMSPRMaskROMLoad - PGMSPRMaskROM;

		nPGMSNDROMLen = (((PGMSNDROMLoad - ICSSNDROM) - 1) | 0xfffff) + 1;

		// Round the soundrom length to the next power of 2 so the soundcore can make a proper mask from it.
		UINT32 Pages = 0;
		for (Pages = 1; Pages < nPGMSNDROMLen; Pages <<= 1); // Calculate nearest power of 2 of len
		//bprintf(0, _T("pgm_run: sndlen %x  pow2 %x\n"), nPGMSNDROMLen, Pages);
		nPGMSNDROMLen = Pages;

		nPGMExternalARMLen = (PGMUSER0Load - PGMUSER0) + 0x100000;

	//	bprintf (0, _T("68k: %x, tile: %x, sprmask: %x, sndrom: %x, arm7: %x\n"), nPGM68KROMLen, nPGMTileROMLen, nPGMSPRMaskROMLen, nPGMSNDROMLen, nPGMExternalARMLen);
	}

	return 0;
}

static inline void pgmSynchroniseZ80(INT32 extra_cycles)
{
	INT32 cycles = (UINT64)(SekTotalCycles()) * nCyclesTotal[1] / nCyclesTotal[0] + extra_cycles;

	if (cycles <= ZetTotalCycles())
		return;

	INT32 i = 0;
	while (ZetTotalCycles() < cycles && i++ < 5)
		BurnTimerUpdate(cycles);
}

static UINT16 ics2115_soundlatch_r(INT32 i)
{
	bSoundlatchRead[i] = 1;
	return nSoundlatch[i];
}

static void ics2115_soundlatch_w(INT32 i, UINT16 d)
{
	nSoundlatch[i] = d;
	bSoundlatchRead[i] = 0;
}

static UINT8 __fastcall PgmReadByte(UINT32 sekAddress)
{
	switch (sekAddress)
	{
		case 0xC00007:
			return v3021Read();

		case 0xC08007: // dipswitches - (ddp2)
			return ~(PgmInput[6]);// | 0xe0;

	//	default:
	//		bprintf(PRINT_NORMAL, _T("Attempt to read byte value of location %x (PC: %5.5x)\n"), sekAddress, SekGetPC(-1));
	}

	return 0;
}

static UINT16 __fastcall PgmReadWord(UINT32 sekAddress)
{
	switch (sekAddress)
	{
		case 0xC00004:
			pgmSynchroniseZ80(0);

			return ics2115_soundlatch_r(1);

		case 0xC00006:	// ketsui wants this
			return v3021Read();

		case 0xC08000:	// p1+p2 controls
			return ~(PgmInput[0] | (PgmInput[1] << 8));

		case 0xC08002:  // p3+p4 controls
			return ~(PgmInput[2] | (PgmInput[3] << 8));

		case 0xC08004:  // extra controls
			return ~(PgmInput[4] | (PgmInput[5] << 8));

		case 0xC08006: // dipswitches
			return ~(PgmInput[6]) | 0xff00; // 0xffe0;

	//	default:
	//		bprintf(PRINT_NORMAL, _T("Attempt to read word value of location %x (PC: %5.5x)\n"), sekAddress, SekGetPC(-1));
	}

	return 0;
}

static void __fastcall PgmWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	byteValue=byteValue; // fix warning

	switch (sekAddress)
	{
	//	default:
	//		bprintf(PRINT_NORMAL, _T("Attempt to write byte value %x to location %x (PC: %5.5x)\n"), byteValue, sekAddress, SekGetPC(-1));
	}
}

static void __fastcall PgmWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	static INT32 coin_counter_previous;

	switch (sekAddress)
	{
		case 0x700006:	// Watchdog?
			break;
			
		case 0xC00002:
			pgmSynchroniseZ80(0);

			ics2115_soundlatch_w(0, wordValue);
			ZetNmi();
			break;

		case 0xC00004:
			pgmSynchroniseZ80(0);

			ics2115_soundlatch_w(1, wordValue);
			break;

		case 0xC00006:
			v3021Write(wordValue);
			break;

		case 0xC00008:
			pgmSynchroniseZ80(0);

			if (wordValue == 0x5050)
			{
				ics2115_reset();
				ZetSetBUSREQLine(0);
				ZetReset();
			} else {
				ZetSetBUSREQLine(1);
			}

			break;

		case 0xC0000A:	// z80_ctrl_w
			break;

		case 0xC0000C:
			pgmSynchroniseZ80(0);

			ics2115_soundlatch_w(2, wordValue);
			break;

		case 0xC08006: // coin counter
			if (coin_counter_previous == 0xf && wordValue == 0) {
			//	bprintf (0, _T("increment coin counter!\n"));
			}
			coin_counter_previous = wordValue & 0x0f;
			break;

	//	default:
	//		bprintf(PRINT_NORMAL, _T("Attempt to write word value %x to location %x (PC: %5.5x)\n"), wordValue, sekAddress, SekGetPC(-1));
	}
}

static UINT8 __fastcall PgmZ80ReadByte(UINT32 sekAddress)
{
	switch (sekAddress)
	{
		default:
			bprintf(PRINT_NORMAL, _T("Attempt to read byte value of location %x\n"), sekAddress);
	}

	return 0;
}

static UINT16 __fastcall PgmZ80ReadWord(UINT32 sekAddress)
{
	pgmSynchroniseZ80(0);

	sekAddress &= 0xffff;
	return (RamZ80[sekAddress] << 8) | RamZ80[sekAddress + 1];
}

static void __fastcall PgmZ80WriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress)
	{
		default:
			bprintf(PRINT_NORMAL, _T("Attempt to write byte value (%2.2x) of location %x\n"), byteValue, sekAddress);
	}
}

static void __fastcall PgmZ80WriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	pgmSynchroniseZ80(0);

	sekAddress &= 0xffff;
	RamZ80[sekAddress    ] = wordValue >> 8;
	RamZ80[sekAddress + 1] = wordValue & 0xFF;
}

inline static UINT32 CalcCol(UINT16 nColour)
{
	INT32 r, g, b;

	r = (nColour & 0x7C00) >> 7;	// Red
	r |= r >> 5;
	g = (nColour & 0x03E0) >> 2;	// Green
	g |= g >> 5;
	b = (nColour & 0x001F) << 3;	// Blue
	b |= b >> 5;

	return BurnHighCol(r, g, b, 0);
}

static void __fastcall PgmPaletteWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	sekAddress = (sekAddress - 0xa00000) >> 1;
	PGMPalRAM[sekAddress] = BURN_ENDIAN_SWAP_INT16(wordValue);
	RamCurPal[sekAddress] = CalcCol(wordValue);
}

static void __fastcall PgmPaletteWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	sekAddress -= 0xa00000;
	UINT8 *pal = (UINT8*)PGMPalRAM;
	pal[sekAddress ^ 1] = byteValue;

	RamCurPal[sekAddress >> 1] = CalcCol(PGMPalRAM[sekAddress >> 1]);
}

static UINT8 __fastcall PgmZ80PortRead(UINT16 port)
{
	switch (port >> 8)
	{
		case 0x80:
			return ics2115read(port & 0xff);

		case 0x81:
			return ics2115_soundlatch_r(2) & 0xff;

		case 0x82:
			return ics2115_soundlatch_r(0) & 0xff;

		case 0x84:
			return ics2115_soundlatch_r(1) & 0xff;

//		default:
//			bprintf(PRINT_NORMAL, _T("Z80 Attempt to read port %04x\n"), port);
	}
	return 0;
}

static void __fastcall PgmZ80PortWrite(UINT16 port, UINT8 data)
{
	switch (port >> 8)
	{
		case 0x80:
			ics2115write(port & 0xff, data);
			break;

		case 0x81:
			ics2115_soundlatch_w(2, data);
			break;

		case 0x82:
			ics2115_soundlatch_w(0, data);
			break;	

		case 0x84:
			ics2115_soundlatch_w(1, data);
			break;

//		default:
//			bprintf(PRINT_NORMAL, _T("Z80 Attempt to write %02x to port %04x\n"), data, port);
	}
}

static INT32 PgmDoReset()
{
	if (nPgmCurrentBios != PgmInput[8]) {	// Load the 68k bios
		if (!(BurnDrvGetHardwareCode() & HARDWARE_IGS_JAMMAPCB)) {
			nPgmCurrentBios = PgmInput[8];
			BurnLoadRom(PGM68KBIOS, 0x00082 + nPgmCurrentBios, 1);	// 68k bios
		}
	}

	SekReset(0);

	if (nEnableArm7) {
		Arm7Open(0);
		Arm7Reset();
		Arm7Close();
	}

	ZetOpen(0);
	ZetSetBUSREQLine(0);
	ZetReset();
	ZetClose();

	ics2115_reset();

	if (pPgmResetCallback) {
		pPgmResetCallback();
	}

    memset (hold_coin, 0, sizeof(hold_coin));

	nCyclesDone[0] = nCyclesDone[1] = nCyclesDone[2] = 0;

	HiscoreReset();

	return 0;
}

static void expand_tile_gfx()
{
	UINT8 *src = PGMTileROM;
	UINT8 *dst = PGMTileROMExp;

	if (pPgmTileDecryptCallback) {
		pPgmTileDecryptCallback(PGMTileROM + 0x180000, nPGMTileROMLen - 0x180000);
	}

	for (INT32 i = nPGMTileROMLen/5-1; i >= 0 ; i --) {
		dst[0+8*i] = ((src[0+5*i] >> 0) & 0x1f);
		dst[1+8*i] = ((src[0+5*i] >> 5) & 0x07) | ((src[1+5*i] << 3) & 0x18);
		dst[2+8*i] = ((src[1+5*i] >> 2) & 0x1f );
		dst[3+8*i] = ((src[1+5*i] >> 7) & 0x01) | ((src[2+5*i] << 1) & 0x1e);
		dst[4+8*i] = ((src[2+5*i] >> 4) & 0x0f) | ((src[3+5*i] << 4) & 0x10);
		dst[5+8*i] = ((src[3+5*i] >> 1) & 0x1f );
		dst[6+8*i] = ((src[3+5*i] >> 6) & 0x03) | ((src[4+5*i] << 2) & 0x1c);
		dst[7+8*i] = ((src[4+5*i] >> 3) & 0x1f );
	}

	for (INT32 i = 0x200000-1; i >= 0; i--) {
		INT32 d = PGMTileROM[i];
		PGMTileROM[i * 2 + 0] = d & 0x0f;
		PGMTileROM[i * 2 + 1] = d >> 4;
	}

	PGMTileROM = (UINT8*)BurnRealloc(PGMTileROM, 0x400000);
}

static void expand_colourdata()
{
	// allocate 
	{
		INT32 needed = (nPGMSPRColROMLen / 2) * 3;
		nPGMSPRColMaskLen = 1;
		while (nPGMSPRColMaskLen < needed)
			nPGMSPRColMaskLen <<= 1;

		needed = nPGMSPRMaskROMLen;
		nPGMSPRMaskMaskLen = 1;
		while (nPGMSPRMaskMaskLen < needed)
			nPGMSPRMaskMaskLen <<= 1;
		nPGMSPRMaskMaskLen-=1;

		PGMSPRColROM = (UINT8*)BurnMalloc(nPGMSPRColMaskLen);
		nPGMSPRColMaskLen -= 1;
	}

	UINT8 *tmp = (UINT8*)BurnMalloc(nPGMSPRColROMLen);
	if (tmp == NULL) return;

	// load sprite color roms
	{
		char* pRomName;
		struct BurnRomInfo ri;
	
		UINT8 *PGMSPRColROMLoad = tmp;
		INT32 prev_len = 0;
	
		for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
	
			BurnDrvGetRomInfo(&ri, i);
	
			if ((ri.nType & BRF_GRA) && (ri.nType & 0x0f) == 3)
			{
				// Fix for kovsh a0603 rom overlap
				if (ri.nLen == 0x400000 && prev_len == 0x400000 && nPGMSPRColROMLen == 0x2000000) {
					PGMSPRColROMLoad -= 0x200000;
				}

				BurnLoadRom(PGMSPRColROMLoad, i, 1);
				PGMSPRColROMLoad += ri.nLen;
				prev_len = ri.nLen;

				continue;
			}
		}
	}

	if (pPgmColorDataDecryptcallback) {
		pPgmColorDataDecryptcallback(tmp, nPGMSPRColROMLen);
	}

	// convert from 3bpp packed
	for (INT32 cnt = 0; cnt < nPGMSPRColROMLen / 2; cnt++)
	{
		UINT16 colpack = ((tmp[cnt*2]) | (tmp[cnt*2+1] << 8));
		PGMSPRColROM[cnt*3+0] = (colpack >> 0 ) & 0x1f;
		PGMSPRColROM[cnt*3+1] = (colpack >> 5 ) & 0x1f;
		PGMSPRColROM[cnt*3+2] = (colpack >> 10) & 0x1f;
	}

	BurnFree (tmp);
}

static void ics2115_sound_irq(INT32 nState)
{
	ZetSetIRQLine(0, nState);
}

INT32 pgmInit()
{
	BurnSetRefreshRate(((BurnDrvGetHardwareCode() & HARDWARE_IGS_JAMMAPCB) || pgm_cave_refresh) ? 59.17 : 60.00);

	Mem = NULL;

	pgmGetRoms(false);

	expand_colourdata();

	PGMTileROM      = (UINT8*)BurnMalloc(nPGMTileROMLen);		// 8x8 Text Tiles + 32x32 BG Tiles
	PGMTileROMExp   = (UINT8*)BurnMalloc((nPGMTileROMLen / 5) * 8);	// Expanded 8x8 Text Tiles and 32x32 BG Tiles
	PGMSPRMaskROM	= (UINT8*)BurnMalloc(nPGMSPRMaskROMLen);
	ICSSNDROM	= (UINT8*)BurnMalloc(nPGMSNDROMLen);

	pgmMemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	pgmMemIndex();

	// load bios roms (68k bios loaded in reset routine)
	if (BurnLoadRom(PGMTileROM, 0x80, 1)) return 1;	// Bios Text and Tiles
	    BurnLoadRom(ICSSNDROM,  0x81, 1);	     	// Bios Intro Sounds

	pgmGetRoms(true);

	expand_tile_gfx();	// expand graphics

	{
		SekInit(0, 0x68000);											// Allocate 68000
		SekOpen(0);

		// ketsui and espgaluda
		if (BurnDrvGetHardwareCode() & HARDWARE_IGS_JAMMAPCB)
		{
			SekMapMemory(PGM68KROM,				0x000000, (nPGM68KROMLen-1), MAP_ROM);			// 68000 ROM (no bios)
		}
		else
		{
			SekMapMemory(PGM68KBIOS,			0x000000, 0x07ffff, MAP_ROM);				// 68000 BIOS
			SekMapMemory(PGM68KROM,				0x100000, (nPGM68KROMLen-1)+0x100000, MAP_ROM);		// 68000 ROM
			if (strcmp(BurnDrvGetTextA(DRV_NAME), "kov2pfwll") == 0)
				SekMapMemory((PGM68KROM + 0x300000), 0x600000, 0x6FFFFF, MAP_ROM); // Many hacks from GOTVG, such as zerofx, kovshpd and kovplus2012 serials, use this as protection.
		}

        for (INT32 i = 0; i < 0x100000; i+=0x20000) {		// Main Ram + Mirrors...
			SekMapMemory(PGM68KRAM,            		0x800000 | i, 0x81ffff | i, MAP_RAM);
		}

		// Thanks to FBA Shuffle for this
		for (INT32 i = 0; i < 0x100000; i+=0x08000) {		// Video Ram + Mirrors...
			SekMapMemory((UINT8 *)PGMBgRAM,		0x900000 | i, 0x900fff | i, MAP_RAM);
			SekMapMemory((UINT8 *)PGMBgRAM,		0x901000 | i, 0x901fff | i, MAP_RAM); // mirror
 			SekMapMemory((UINT8 *)PGMBgRAM,		0x902000 | i, 0x902fff | i, MAP_RAM); // mirror
			SekMapMemory((UINT8 *)PGMBgRAM,		0x903000 | i, 0x904fff | i, MAP_RAM); // mirror
			SekMapMemory((UINT8 *)PGMTxtRAM,	0x904000 | i, 0x905fff | i, MAP_RAM);
			SekMapMemory((UINT8 *)PGMTxtRAM,	0x906000 | i, 0x906fff | i, MAP_RAM); // mirror
			SekMapMemory((UINT8 *)PGMRowRAM,	0x907000 | i, 0x907fff | i, MAP_RAM);
		}

		SekMapMemory((UINT8 *)PGMPalRAM,		0xa00000, 0xa013ff, MAP_ROM); // palette
		SekMapMemory((UINT8 *)PGMVidReg,		0xb00000, 0xb0ffff, MAP_RAM); // should be mirrored?

		SekMapHandler(1,					0xa00000, 0xa013ff, MAP_WRITE);
		SekMapHandler(2,					0xc10000, 0xc1ffff, MAP_READ | MAP_WRITE);

		SekSetReadWordHandler(0, PgmReadWord);
		SekSetReadByteHandler(0, PgmReadByte);
		SekSetWriteWordHandler(0, PgmWriteWord);
		SekSetWriteByteHandler(0, PgmWriteByte);

		SekSetWriteByteHandler(1, PgmPaletteWriteByte);
		SekSetWriteWordHandler(1, PgmPaletteWriteWord);

		SekSetReadWordHandler(2, PgmZ80ReadWord);
		SekSetReadByteHandler(2, PgmZ80ReadByte);
		SekSetWriteWordHandler(2, PgmZ80WriteWord);
		SekSetWriteByteHandler(2, PgmZ80WriteByte);
		SekClose();
	}

	{
		ZetInit(0);
		ZetOpen(0);
		ZetMapMemory(RamZ80, 0x0000, 0xffff, MAP_RAM);
		ZetSetOutHandler(PgmZ80PortWrite);
		ZetSetInHandler(PgmZ80PortRead);
		ZetClose();
	}

	if (BurnDrvGetHardwareCode() & HARDWARE_IGS_USE_ARM_CPU) {
		nEnableArm7 = 1;
	}

	pgmInitDraw();

	v3021Init();
	ics2115_init(ics2115_sound_irq, ICSSNDROM, nPGMSNDROMLen);
	BurnTimerAttachZet(Z80_FREQ);

	pBurnDrvPalette = (UINT32*)PGMPalRAM;
    
    if (strncmp(BurnDrvGetTextA(DRV_NAME), "pgm3in1", 7) == 0) {//load pgm3in1 mask rom and sound rom before pgm3in1's decrypt
        UINT8 *maskROM = (UINT8 *)malloc(0x200000);
        BurnLoadRom(maskROM,9,1);
        memcpy((void *)(PGMSPRMaskROM + 0xf00000),maskROM,0x100000);
        free(maskROM);
        BurnLoadRom(ICSSNDROM + 0x800000,0x0b,1);
    }

	if (pPgmInitCallback) {
		pPgmInitCallback();
	}

	if (pPgmProtCallback) {
		pPgmProtCallback();
	}

	PgmDoReset();

	return 0;
}

INT32 pgmExit()
{
	pgmExitDraw();

	SekExit();
	ZetExit();

	if (nEnableArm7) {
		Arm7Exit();
	}

	if (ICSSNDROM) {
		BurnFree(ICSSNDROM);
	}

	BurnFree(Mem);

	v3021Exit();
	ics2115_exit();

	BurnFree (PGMTileROM);
	BurnFree (PGMTileROMExp);
	BurnFree (PGMSPRColROM);
	BurnFree (PGMSPRMaskROM);

	nPGM68KROMLen = 0;
	nPGMTileROMLen = 0;
	nPGMSPRColROMLen = 0;
	nPGMSPRMaskROMLen = 0;
	nPGMSNDROMLen = 0;
	nPGMExternalARMLen = 0;

	pPgmInitCallback = NULL;
	pPgmTileDecryptCallback = NULL;
	pPgmColorDataDecryptcallback = NULL;
	pPgmProtCallback = NULL;
	pPgmScanCallback = NULL;
	pPgmResetCallback = NULL;

	nEnableArm7 = 0;
	nPGMDisableIRQ4 = 0;
	nPGMArm7Type = 0;
	nPgmAsicRegionHackAddress = 0;

	nPgmCurrentBios = -1;

	pgm_cave_refresh = 0;

	return 0;
}

INT32 pgmFrame()
{
	if (PgmReset) {
		PgmDoReset();
	}

	// compile inputs
	{
        INT32 previous_coin = PgmCoins & 0xf;
        PgmCoins = 0;

        memset (PgmInput, 0, 6);
		for (INT32 i = 0; i < 8; i++) {
			PgmInput[0] |= (PgmJoy1[i] & 1) << i;
			PgmInput[1] |= (PgmJoy2[i] & 1) << i;
			PgmInput[2] |= (PgmJoy3[i] & 1) << i;
			PgmInput[3] |= (PgmJoy4[i] & 1) << i;
			PgmCoins    |= (PgmBtn1[i] & 1) << i;
			PgmInput[5] |= (PgmBtn2[i] & 1) << i;
		}

		// clear opposites
		if ((PgmInput[0] & 0x06) == 0x06) PgmInput[0] &= 0xf9; // up/down
		if ((PgmInput[0] & 0x18) == 0x18) PgmInput[0] &= 0xe7; // left/right
		if ((PgmInput[1] & 0x06) == 0x06) PgmInput[1] &= 0xf9;
		if ((PgmInput[1] & 0x18) == 0x18) PgmInput[1] &= 0xe7;
		if ((PgmInput[2] & 0x06) == 0x06) PgmInput[2] &= 0xf9;
		if ((PgmInput[2] & 0x18) == 0x18) PgmInput[2] &= 0xe7;
		if ((PgmInput[3] & 0x06) == 0x06) PgmInput[3] &= 0xf9;
		if ((PgmInput[3] & 0x18) == 0x18) PgmInput[3] &= 0xe7;

        // silly hold coin logic
        for (INT32 i = 0; i < 4; i++) {
            if ((previous_coin != (PgmCoins & 0xf)) && PgmBtn1[i] && !hold_coin[i]) {
                hold_coin[i] = 7 + 1; // frames to hold coin + 1
            }
            if (hold_coin[i]) {
                hold_coin[i]--;
                PgmInput[4] |= 1<<i;
            }
            if (!hold_coin[i]) {
                PgmInput[4] &= ~(1<<i);
			}
		}

        PgmInput[4] |= PgmCoins & ~0xf; // add non-coin buttons
	}

	SekNewFrame();
	ZetNewFrame();

	SekOpen(0);
	ZetOpen(0);

	SekIdle(nCyclesDone[0]);
	ZetIdle(nCyclesDone[1]);

	SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);

	if (nEnableArm7)
	{
		Arm7NewFrame();
		Arm7Open(0);
		Arm7Idle(nCyclesDone[2]);

		// region hacks
		if (strncmp(BurnDrvGetTextA(DRV_NAME), "dmnfrnt", 7) == 0) {
			PGMARMShareRAM[0x158] = PgmInput[7];
			PGMARMShareRAM2[0x158] = PgmInput[7];

			// dmnfrntpcb - requires this - set internal rom version
			PGMARMShareRAM[0x164] = '1'; // S101KR (101 Korea) - $69be8 in ROM
			PGMARMShareRAM[0x165] = 'S';
			PGMARMShareRAM[0x166] = '1';
			PGMARMShareRAM[0x167] = '0';	
			PGMARMShareRAM[0x168] = 'R';
			PGMARMShareRAM[0x169] = 'K';
		} else if (nPgmAsicRegionHackAddress) {
			PGMARMROM[nPgmAsicRegionHackAddress] = PgmInput[7];
		}

		nCyclesTotal[0] = M68K_CYCS_PER_FRAME;
		nCyclesTotal[1] = Z80_CYCS_PER_FRAME;
		nCyclesTotal[2] = ARM7_CYCS_PER_FRAME;

		while (SekTotalCycles() < nCyclesTotal[0] / 2)
			SekRun(nCyclesTotal[0] / 2 - SekTotalCycles());

		if (!nPGMDisableIRQ4)
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		while (SekTotalCycles() < nCyclesTotal[0])
			SekRun(nCyclesTotal[0] - SekTotalCycles());

		while (Arm7TotalCycles() < nCyclesTotal[2])
			Arm7Run(nCyclesTotal[2] - Arm7TotalCycles());

		nCyclesDone[2] = Arm7TotalCycles() - nCyclesTotal[2];
		Arm7Close();
	}
	else
	{
		nCyclesTotal[0] = (UINT32)((UINT64)(M68K_FREQ) * nBurnCPUSpeedAdjust * 100 / (0x0100 * nBurnFPS));
		nCyclesTotal[1] = Z80_CYCS_PER_FRAME;

		while (SekTotalCycles() < nCyclesTotal[0] / 2)
			SekRun(nCyclesTotal[0] / 2 - SekTotalCycles());

		if (!nPGMDisableIRQ4)
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

		while (SekTotalCycles() < nCyclesTotal[0])
			SekRun(nCyclesTotal[0] - SekTotalCycles());
	}

	BurnTimerEndFrame(nCyclesTotal[1]);
	ics2115_update(nBurnSoundLen);

	nCyclesDone[0] = SekTotalCycles() - nCyclesTotal[0];
	nCyclesDone[1] = ZetTotalCycles() - nCyclesTotal[1];

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		pgmDraw();
	}

	memcpy (PGMSprBuf, PGM68KRAM /* Sprite RAM 0-bff */, 0xa00); // buffer sprites

	return 0;
}

INT32 pgmScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin =  0x029702;
	}

	nPgmPalRecalc = 1;

	if (nAction & ACB_MEMORY_ROM) {	
		if (BurnDrvGetHardwareCode() & HARDWARE_IGS_JAMMAPCB) {
			ba.Data		= PGM68KROM;
			ba.nLen		= nPGM68KROMLen;
			ba.nAddress	= 0;
			ba.szName	= "68K ROM";
			BurnAcb(&ba);
		} else {
			ba.Data		= PGM68KBIOS;
			ba.nLen		= 0x0020000;
			ba.nAddress	= 0;
			ba.szName	= "BIOS ROM";
			BurnAcb(&ba);

			ba.Data		= PGM68KROM;
			ba.nLen		= nPGM68KROMLen;
			ba.nAddress	= 0x100000;
			ba.szName	= "68K ROM";
			BurnAcb(&ba);
		}
	}

	if (nAction & ACB_MEMORY_RAM) {	
		ba.Data		= PGMBgRAM;
		ba.nLen		= 0x0004000;
		ba.nAddress	= 0x900000;
		ba.szName	= "Bg RAM";
		BurnAcb(&ba);

		ba.Data		= PGMTxtRAM;
		ba.nLen		= 0x0003000;
		ba.nAddress	= 0x904000;
		ba.szName	= "Tx RAM";
		BurnAcb(&ba);

		ba.Data		= PGMRowRAM;
		ba.nLen		= 0x0001000;
		ba.nAddress	= 0x907000;
		ba.szName	= "Row Scroll";
		BurnAcb(&ba);

		ba.Data		= PGMPalRAM;
		ba.nLen		= 0x0001400;
		ba.nAddress	= 0xA00000;
		ba.szName	= "Palette";
		BurnAcb(&ba);

		ba.Data		= PGMVidReg;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0xB00000;
		ba.szName	= "Video Regs";
		BurnAcb(&ba);
		
		ba.Data		= RamZ80;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0xC10000;
		ba.szName	= "Z80 RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= PGM68KRAM;
		ba.nLen		= 0x020000;
		ba.nAddress	= 0x800000;
		ba.szName	= "68K RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
	
		SekScan(nAction);
		ZetScan(nAction);

		v3021Scan();

		SCAN_VAR(hold_coin);

		SCAN_VAR(nPgmCurrentBios);

		SCAN_VAR(nSoundlatch);
		SCAN_VAR(bSoundlatchRead);

		ics2115_scan(nAction, pnMin);
	}

	if (pPgmScanCallback) {
		pPgmScanCallback(nAction, pnMin);
	}

 	return 0;
}
