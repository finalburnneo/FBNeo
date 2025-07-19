
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
UINT8 PgmReset = 0;
static HoldCoin<4> hold_coin;
static ClearOpposite<4, UINT8> clear_opposite;

INT32 nPGM68KROMLen = 0;
INT32 nPGMTileROMLen = 0;
INT32 nPGMSPRColROMLen = 0;
INT32 nPGMSPRMaskROMLen = 0;
INT32 nPGMSNDROMLen = 0;
INT32 nPGMSPRColMaskLen = 0;
INT32 nPGMSPRMaskMaskLen = 0;
INT32 nPGMExternalARMLen = 0;

UINT16 pgm_bg_scrollx;
UINT16 pgm_bg_scrolly;
UINT16 pgm_fg_scrollx;
UINT16 pgm_fg_scrolly;
UINT16 pgm_video_control;
static UINT16 pgm_unk_video_flags;
static INT32 pgm_z80_connect_bus;

UINT16 *PGMZoomRAM;
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
INT32 nPGMSpriteBufferHack = 0;
INT32 nPGMMapperHack = 0;
INT32 OldCodeMode = 0;

#define Z80_FREQ	8467200

#define M68K_CYCS_PER_FRAME	((20000000 * 100) / nBurnFPS)
#define ARM7_CYCS_PER_FRAME	((20000000 * 100) / nBurnFPS)
#define Z80_CYCS_PER_FRAME	((Z80_FREQ * 100) / nBurnFPS)

static INT32 nCyclesDone[3];
static INT32 nCyclesTotal[3];

static INT32 pgmMemIndex()
{
	UINT8 *Next; Next = Mem;
	PGM68KBIOS			= Next;	Next += 0x0080000;
	PGM68KROM			= Next;	Next += nPGM68KROMLen;

	PGMUSER0			= Next;	Next += nPGMExternalARMLen;

	PGMProtROM			= PGMUSER0 + 0x10000; // Olds, Killbld, drgw3

	PGMARMROM			= Next;	Next += (bDoIpsPatch || (0 != nPGMSpriteBufferHack)) ? 0x0008000 : 0x0004000;	// Just always allocate this - only 16kb

	RamCurPal			= (UINT32 *) Next;
	if (OldCodeMode)			Next += (0x0001204 / 2) * sizeof(UINT32);
	else						Next += (0x0002004 / 2) * sizeof(UINT32);

	RamStart			= Next;

	PGM68KRAM			= Next;	Next += 0x0020000;
	RamZ80				= Next;	Next += 0x0010000;

	if (nEnableArm7) {
		PGMARMShareRAM	= Next;	Next += 0x0020000;
		if (OldCodeMode)		Next -= 0x0010000;
		PGMARMShareRAM2	= Next;	Next += 0x0020000;
		if (OldCodeMode)		Next -= 0x0010000;
		PGMARMRAM0		= Next;	Next += 0x0001000; // minimum page size in arm7 is 0x1000
		PGMARMRAM1		= Next;	Next += 0x0040000;
		PGMARMRAM2		= Next;	Next += 0x0001000; // minimum page size in arm7 is 0x1000
	}

	PGMZoomRAM			= (UINT16 *) Next; 	Next += 0x0000040;

	PGMBgRAM			= (UINT32 *) Next;	Next += 0x0001000;
	PGMTxtRAM			= (UINT32 *) Next;	Next += 0x0002000;

	PGMRowRAM			= (UINT16 *) Next;	Next += 0x0001000;	// Row Scroll
	PGMPalRAM			= (UINT16 *) Next;	Next += 0x0002000;	// Palette R5G5B5
	if (OldCodeMode)						Next -= 0x0000c00;
	PGMVidReg			= (UINT16 *) Next;	Next += 0x0010000;	// Video Regs inc. Zoom Table
	PGMSprBuf			= (UINT16 *) Next;	Next += 0x0001000;

	RamEnd				= Next;

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

	if (bLoad) {
		if (nPGM68KROMLen == 0x80000 && nPGMSNDROMLen == 0x600000) { // dw2001 & dwpc
			PGMSNDROMLoad -= 0x200000;
		} else if ((!bDoIpsPatch && ((0 == strcmp(BurnDrvGetTextA(DRV_NAME), "kov2pshjz")) || (0 == strcmp(BurnDrvGetTextA(DRV_NAME), "kov2dzxx")))) || (nIpsDrvDefine & IPS_PGM_SNDOFFS)) { // kov2pshjz, kov2dzxx
			PGMSNDROMLoad -= 0x600000;
		}
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
				} else {
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
				if (nEnableArm7) {
					BurnLoadRom(PGMARMROM + ((ri.nLen == 0x3e78) ? 0x188 : 0), i, 1);
				}
			}
			continue;
		}

		if ((ri.nType & BRF_PRG) && (ri.nType & 0x0f) == 8)
		{
			if (nEnableArm7) {
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
		for (Pages = 1; Pages < (UINT32)nPGMSNDROMLen; Pages <<= 1); // Calculate nearest power of 2 of len
		//bprintf(0, _T("pgm_run: sndlen %x  pow2 %x\n"), nPGMSNDROMLen, Pages);
		nPGMSNDROMLen = Pages;

		nPGMExternalARMLen = (PGMUSER0Load - PGMUSER0) + 0x100000;

		if (bDoIpsPatch) {
			nPGM68KROMLen		+= nIpsMemExpLen[PRG1_ROM];
			nPGMExternalARMLen	+= nIpsMemExpLen[PRG2_ROM];
			nPGMTileROMLen		+= nIpsMemExpLen[GRA1_ROM];
			nPGMSNDROMLen		+= nIpsMemExpLen[SND1_ROM];
			nPGMSPRColROMLen	+= nIpsMemExpLen[GRA2_ROM];
			nPGMSPRMaskROMLen	+= nIpsMemExpLen[GRA3_ROM];
		}

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

static inline INT32 get_current_scanline()
{
	UINT32 ret = (SekTotalCycles() * 264) / (M68K_CYCS_PER_FRAME);

	return (ret > 263) ? 263 : ret;
}

static void __fastcall PgmVideoControllerWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress & 0x0f000)
	{
		case 0x0000: PGMSprBuf[(sekAddress >> 1) & 0x7ff] = wordValue; bprintf(0, _T("VideoController write word: %5.5x, %4.4x\n"), sekAddress, wordValue); break; // Sprite buffer is not writeable by the 68K, but the BIOS tries anyway
		case 0x1000: PGMZoomRAM[(sekAddress >> 1) & 0x1f] = wordValue; break; // size is guessed
		case 0x2000: pgm_bg_scrolly = wordValue; break;
		case 0x3000: pgm_bg_scrollx = wordValue; break;
		case 0x4000: /*bprintf (0, _T("VideoController write word: %5.5x, %4.4x\n"), sekAddress, wordValue);*/ pgm_unk_video_flags = wordValue; break; // 0610 is always written, but changing this seems to have no effect
		case 0x5000: pgm_fg_scrolly = wordValue; break;
		case 0x6000: pgm_fg_scrollx = wordValue; break;
		case 0x7000: bprintf (0, _T("VideoController write word: %5.5x, %4.4x\n"), sekAddress, wordValue); break; // ?
		case 0x8000: bprintf (0, _T("VideoController write word: %5.5x, %4.4x\n"), sekAddress, wordValue); break; // ?
		case 0x9000: bprintf (0, _T("VideoController write word: %5.5x, %4.4x\n"), sekAddress, wordValue); break; // ?
		case 0xa000: bprintf (0, _T("VideoController write word: %5.5x, %4.4x\n"), sekAddress, wordValue); break; // ?
		case 0xb000: bprintf (0, _T("VideoController write word: %5.5x, %4.4x\n"), sekAddress, wordValue); break; // ?
		case 0xc000: bprintf (0, _T("VideoController write word: %5.5x, %4.4x\n"), sekAddress, wordValue); break; // ?
		case 0xd000: bprintf (0, _T("VideoController write word: %5.5x, %4.4x\n"), sekAddress, wordValue); break; // ?
		case 0xe000: pgm_video_control = wordValue; break;
		case 0xf000: bprintf (0, _T("VideoController write word: %5.5x, %4.4x\n"), sekAddress, wordValue); break; // ?
	}
}

static void __fastcall PgmVideoControllerWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	bprintf (0, _T("VideoController Write Byte: %5.5x, %2.2x PC(%5.5x)\n"), sekAddress, byteValue, SekGetPC(-1));
}

static UINT16 __fastcall PgmVideoControllerReadWord(UINT32 sekAddress)
{
	bprintf (0, _T("VideoController Read Word: %5.5x, PC(%5.5x)\n"), sekAddress, SekGetPC(-1));

	// ddp2 seems to read from the sprite buffer?
	switch (sekAddress & 0x0f000)
	{
		case 0x0000: return PGMSprBuf[(sekAddress >> 1) & 0x7ff];
		case 0x1000: return 0; // zoom ram is not readable by the 68K
		case 0x2000: return pgm_bg_scrolly;
		case 0x3000: return pgm_bg_scrollx;
		case 0x4000: return pgm_unk_video_flags;
		case 0x5000: return pgm_fg_scrolly;
		case 0x6000: return pgm_fg_scrollx;
		case 0x7000: return get_current_scanline(); // scanline counter? 0 - 107
		case 0x8000: return 0; // ?
		case 0x9000: return 0; // ?
		case 0xa000: return 0; // ?
		case 0xb000: return 0; // ?
		case 0xc000: return 0; // accesses here cause video to refresh?
		case 0xd000: return 0; // accesses here cause video to refresh?
		case 0xe000: return pgm_video_control;
		case 0xf000: return 0;
	}
	
	return 0;
}

static UINT8 __fastcall PgmVideoControllerReadByte(UINT32 sekAddress)
{
	switch (sekAddress & 0x0f000)
	{
		case 0x0000:
			return PGMSprBuf[(sekAddress >> 1) & 0x7ff] >> ((~sekAddress & 1) * 8);
	}

	bprintf (0, _T("VideoController Read Byte: %5.5x, PC(%5.5x)\n"), sekAddress, SekGetPC(-1));

	return 0;
}

static UINT8 __fastcall PgmReadByte(UINT32 sekAddress)
{
	UINT32 u32Addr = sekAddress;
	if (!OldCodeMode) u32Addr &= ~0xe7ff8;

	switch (u32Addr)
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
	UINT32 u32Addr = sekAddress;
	if (!OldCodeMode) u32Addr &= ~0xe7ff8;

	switch (u32Addr)
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
	}

	UINT32 u32Addr = sekAddress;
	if (!OldCodeMode) u32Addr &= ~0xe7ff0;
	
	switch (u32Addr)
	{
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

		case 0xC0000A:	// z80 controller
			if (!OldCodeMode) {
				if (wordValue == 0x45d3) pgm_z80_connect_bus = 1;
				if (wordValue == 0x0a0a) pgm_z80_connect_bus = 0;
			}
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

	if (!OldCodeMode)
		if (pgm_z80_connect_bus == 0) return 0;

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

	if (!OldCodeMode)
		if (pgm_z80_connect_bus == 0) return;

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
	sekAddress = (sekAddress & 0x001ffe) >> 1;

	PGMPalRAM[sekAddress] = BURN_ENDIAN_SWAP_INT16(wordValue);
	RamCurPal[sekAddress] = CalcCol(wordValue);
}

static void __fastcall PgmPaletteWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	sekAddress &= 0x001fff;

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

static void OrlegendRegionHack()
{
	const char* ol_name[] = {
		"orlegend",
		"orlegende",
		"orlegendea",
		"orlegendc",
		"orlegendca",
		"orlegend105k",
		"orlegend105t",
		"orlegend111c",
		"orlegend111k",
		"orlegend111t"
	};

	UINT32 reg_addr[][5] = {
		{ 0x046ae4, 0x0d1725, 0x0d1749, 0x0d176d, 0x0d177f },	// orlegend
		{ 0x046af4, 0x0d1735, 0x0d1759, 0x0d177d, 0x0d178f },	// orlegende
		{ 0x046af4, 0x0d15ab, 0x0d15cf, 0x0d15f3, 0x0d1605 },	// orlegendea
		{ 0x046af4, 0x0d15ab, 0x0d15cf, 0x0d15f3, 0x0d1605 },	// orlegendc
		{ 0x046aba, 0x0459fc, 0x045a00, 0x000000, 0x000000 },	// orlegendca
		{ 0x046450, 0x0d0e47, 0x000000, 0x000000, 0x000000 },	// orlegend105k
		{ 0x046450, 0x0d0e11, 0x000000, 0x000000, 0x000000 },	// orlegend105t
		{ 0x0468b2, 0x0457f4, 0x0457f8, 0x000000, 0x000000 },	// orlegend111c
		{ 0x0468a8, 0x0d12bd, 0x000000, 0x000000, 0x000000 },	// orlegend111k
		{ 0x0468a8, 0x0d1287, 0x000000, 0x000000, 0x000000 },	// orlegend111t
	};

	for (INT32 nGame = 0; nGame < sizeof(ol_name) / sizeof(char*); nGame++) {
		if (0 == strcmp(BurnDrvGetTextA(DRV_NAME), ol_name[nGame])) {
			*((UINT16*)(PGM68KROM + reg_addr[nGame][0] + 0)) = BURN_ENDIAN_SWAP_INT16(0x4e71);
			*((UINT16*)(PGM68KROM + reg_addr[nGame][0] + 2)) = BURN_ENDIAN_SWAP_INT16(0x4e71);

			for (INT32 nAddr = 1; nAddr <= 4; nAddr++) {
				if (0x000000 != reg_addr[nGame][nAddr]) {
					PGM68KROM[reg_addr[nGame][nAddr]] = PgmInput[7];
				}
			}
		}
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

		// region hack
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
	}

	ZetOpen(0);
	ZetSetBUSREQLine(0);
	ZetReset();
	ZetClose();

	ics2115_reset();

	if (pPgmResetCallback) {
		pPgmResetCallback();

		// Orlegend series of regions hack
		if (0 == strncmp(BurnDrvGetTextA(DRV_NAME), "orlegend", 8)) {
			OrlegendRegionHack();
		}
	}

	hold_coin.reset();
	clear_opposite.reset();

	nCyclesDone[0] = nCyclesDone[1] = nCyclesDone[2] = 0;

	HiscoreReset();

	pgm_bg_scrollx = 0;
	pgm_bg_scrolly = 0;
	pgm_fg_scrollx = 0;
	pgm_fg_scrolly = 0;
	pgm_video_control = 0;
	pgm_unk_video_flags = 0;
	pgm_z80_connect_bus = 1;

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
				// Fix for kovsh & hacks a0613 rom overlap
				if ((ri.nLen >= 0x400000) && (prev_len == 0x400000) && (nPGMSPRColROMLen >= 0x2000000)) {
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
	BurnSetRefreshRate(59.185606);

	nEnableArm7 = (BurnDrvGetHardwareCode() / HARDWARE_IGS_USE_ARM_CPU) & 1;
	OldCodeMode = ((HackCodeDip & 1) || (bDoIpsPatch) || (NULL != pDataRomDesc)) ? 1 : 0;

	if (0 == nPGMSpriteBufferHack) {
		nPGMSpriteBufferHack = (nIpsDrvDefine & IPS_PGM_SPRHACK) ? 1 : 0;
	}

	Mem = NULL;

	pgmGetRoms(false);

	expand_colourdata();

	PGMTileROM      = (UINT8*)BurnMalloc(nPGMTileROMLen);			// 8x8 Text Tiles + 32x32 BG Tiles
	PGMTileROMExp   = (UINT8*)BurnMalloc((nPGMTileROMLen / 5) * 8);	// Expanded 8x8 Text Tiles and 32x32 BG Tiles
	PGMSPRMaskROM	= (UINT8*)BurnMalloc(nPGMSPRMaskROMLen);
	ICSSNDROM		= (UINT8*)BurnMalloc(nPGMSNDROMLen);

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
		} else {
			// if a cart is mapped at 100000+, the BIOS is mapped from 0-fffff, if no cart inserted, the BIOS is mapped to 7fffff!
			for (INT32 i = 0; i < 0x100000; i+= 0x20000) { // DDP3 bios is 512k in size, but >= 20000 is 0-filled!
				if ((!bDoIpsPatch && nPGMMapperHack) || (nIpsDrvDefine & IPS_PGM_MAPHACK)) {
					SekMapMemory(PGM68KBIOS, 0x000000, 0x07ffff, MAP_ROM); // kov2dzxx 68K BIOS, Mapped addresses other than 7fffff will fail.
					SekMapMemory((PGM68KROM + 0x300000), 0x600000, 0x6fffff, MAP_ROM); // Adds a mapping for the specified game/ips.

					break;
				}

				SekMapMemory(PGM68KBIOS,			0x000000 | i, 0x01ffff | i, MAP_ROM);			// 68000 BIOS
			}

			if (nPGM68KROMLen == 0x1000000) // banked 68k rom
				SekMapMemory(PGM68KROM,				0x100000, 0x4fffff, MAP_ROM);
			else
				SekMapMemory(PGM68KROM,				0x100000, (nPGM68KROMLen-1)+0x100000, MAP_ROM);		// 68000 ROM

			// from 0 to 7fffff is completely mappable by the cartridge (it can cover the bios!)
		}

        for (INT32 i = 0; i < 0x100000; i+=0x20000) {		// Main Ram + Mirrors...
			SekMapMemory(PGM68KRAM,            	0x800000 | i, 0x81ffff | i, MAP_RAM);
		}

		for (INT32 i = 0; i < 0x100000; i+=0x08000) {		// Video Ram + Mirrors...
			SekMapMemory((UINT8 *)PGMBgRAM,		0x900000 | i, 0x900fff | i, MAP_RAM);
			SekMapMemory((UINT8 *)PGMBgRAM,		0x901000 | i, 0x901fff | i, MAP_RAM); // mirror
 			SekMapMemory((UINT8 *)PGMBgRAM,		0x902000 | i, 0x902fff | i, MAP_RAM); // mirror
			SekMapMemory((UINT8 *)PGMBgRAM,		0x903000 | i, 0x904fff | i, MAP_RAM); // mirror
			SekMapMemory((UINT8 *)PGMTxtRAM,	0x904000 | i, 0x905fff | i, MAP_RAM);
			SekMapMemory((UINT8 *)PGMTxtRAM,	0x906000 | i, 0x906fff | i, MAP_RAM); // mirror
			SekMapMemory((UINT8 *)PGMRowRAM,	0x907000 | i, 0x907fff | i, MAP_RAM);
		}
		
		if (OldCodeMode) {
			SekMapMemory((UINT8 *)PGMPalRAM,	0xa00000, 0xa013ff, MAP_ROM); // palette
			SekMapMemory((UINT8 *)PGMVidReg,	0xb00000, 0xb0ffff, MAP_RAM); // should be mirrored?
			SekMapHandler(1,					0xa00000, 0xa013ff, MAP_WRITE);
			SekMapHandler(2,					0xc10000, 0xc1ffff, MAP_READ | MAP_WRITE);
		} else {
			for (INT32 i = 0; i < 0x100000; i+= 0x02000) { // mirror
				SekMapMemory((UINT8 *)PGMPalRAM,0xa00000 | i, 0xa01fff | i, MAP_ROM); // palette
			}
			SekMapHandler(1,					0xa00000, 0xafffff, MAP_WRITE);
			SekMapHandler(2,					0xb00000, 0xbfffff, MAP_READ | MAP_WRITE);
			for (INT32 i = 0; i < 0x100000; i += 0x20000) { // mirror
				SekMapHandler(3,				0xc10000 | i, 0xc1ffff | i, MAP_READ | MAP_WRITE);
			}
		}

		// from d00000 to ffffff is completely mappable by the cartridge

		SekSetReadWordHandler(0,		PgmReadWord);
		SekSetReadByteHandler(0,		PgmReadByte);
		SekSetWriteWordHandler(0,		PgmWriteWord);
		SekSetWriteByteHandler(0,		PgmWriteByte);

		SekSetWriteByteHandler(1,		PgmPaletteWriteByte);
		SekSetWriteWordHandler(1,		PgmPaletteWriteWord);
		
		if (OldCodeMode) {
			SekSetReadWordHandler(2,	PgmZ80ReadWord);
			SekSetReadByteHandler(2,	PgmZ80ReadByte);
			SekSetWriteWordHandler(2,	PgmZ80WriteWord);
			SekSetWriteByteHandler(2,	PgmZ80WriteByte);
		} else {
			SekSetReadWordHandler(2,	PgmVideoControllerReadWord);
			SekSetReadByteHandler(2,	PgmVideoControllerReadByte);
			SekSetWriteWordHandler(2,	PgmVideoControllerWriteWord);
			SekSetWriteByteHandler(2,	PgmVideoControllerWriteByte);

			SekSetReadWordHandler(3,	PgmZ80ReadWord);
			SekSetReadByteHandler(3,	PgmZ80ReadByte);
			SekSetWriteWordHandler(3,	PgmZ80WriteWord);
			SekSetWriteByteHandler(3,	PgmZ80WriteByte);
		}

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

	nPGMSpriteBufferHack = 0;
	nPGMMapperHack = 0;

	return 0;
}

static void pgm_sprite_buffer()
{
	if (pgm_video_control & 0x0001) // verified
	{
		UINT16 *ram16 = (UINT16*)PGM68KRAM;
		
		UINT16 mask[2][5] = { 
			{ 0xffff, 0xfbff, 0x7fff, 0xffff, 0xffff }, // The sprite buffer hardware masks these bits!
			{ 0xffff, 0xffff, 0xffff, 0xffff, 0xffff }	// Some hacks rely on poor emulation
		};
		
		for (INT32 i = 0; i < 0xa00/2; i+= 10/2)
		{
			for (INT32 j = 0; j < 10 / 2; j++)
			{
				PGMSprBuf[(i / (10 / 2)) * (16 / 2) + j] = ram16[i + j] & mask[nPGMSpriteBufferHack][j];
			} 

			if ((ram16[i+4] & 0x7fff) == 0) break; // verified on hardware
		}
	}
}

INT32 pgmFrame()
{
	if (PgmReset) {
		PgmDoReset();
	}

	// compile inputs
	{
		memset (PgmInput, 0, 6); // 6 is correct! Regions are stored in 7!

		for (INT32 i = 0; i < sizeof(PgmJoy1); i++) {
			PgmInput[0] |= (PgmJoy1[i] & 1) << i;
			PgmInput[1] |= (PgmJoy2[i] & 1) << i;
			PgmInput[2] |= (PgmJoy3[i] & 1) << i;
			PgmInput[3] |= (PgmJoy4[i] & 1) << i;
			PgmInput[4] |= (PgmBtn1[i] & 1) << i;
			PgmInput[5] |= (PgmBtn2[i] & 1) << i;
		}

		// clear opposites & hold coin
		for (INT32 i = 0; i < 4; i++) {
			clear_opposite.check(i, PgmInput[i], 0x02, 0x4, 0x08, 0x10, nSocd[i]);
			hold_coin.check(i, PgmInput[4], 1 << i, 7);
		}
	}

	SekNewFrame();
	ZetNewFrame();

	SekOpen(0);
	ZetOpen(0);

	SekIdle(nCyclesDone[0]);
	ZetIdle(nCyclesDone[1]);

	if (OldCodeMode)
		SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);

	if (nEnableArm7)
	{
		Arm7NewFrame();
		Arm7Open(0);
		Arm7Idle(nCyclesDone[2]);

		if (OldCodeMode) {
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
	} else {
		if (OldCodeMode) {
			nCyclesTotal[0] = (UINT32)((UINT64)(20000000) * nBurnCPUSpeedAdjust * 100 / (0x0100 * nBurnFPS));
			nCyclesTotal[1] = Z80_CYCS_PER_FRAME;

			while (SekTotalCycles() < nCyclesTotal[0] / 2)
				SekRun(nCyclesTotal[0] / 2 - SekTotalCycles());

			if (!nPGMDisableIRQ4)
				SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

			while (SekTotalCycles() < nCyclesTotal[0])
				SekRun(nCyclesTotal[0] - SekTotalCycles());
		}
	}

	if (!OldCodeMode) {
		INT32 nInterleave = 262; // 262 scanlines
		nCyclesTotal[0] = M68K_CYCS_PER_FRAME;
		nCyclesTotal[1] = Z80_CYCS_PER_FRAME;
		nCyclesTotal[2] = ARM7_CYCS_PER_FRAME;

		for (INT32 i = 0; i < nInterleave; i++)
		{
			if (i == 224) {
				SekSetIRQLine(6, CPU_IRQSTATUS_AUTO); // vblank - cart-controlled!
				pgm_sprite_buffer();
			}
			if (i == 218 && !nPGMDisableIRQ4) SekSetIRQLine(4, CPU_IRQSTATUS_AUTO); // verified on Dragon World II cart - Cart-controlled! 

			CPU_RUN(0, Sek);
		//	CPU_IDLE_SYNCINT(1, Zet); // sync'd on reads and writes and at the end of the frame
			if (nEnableArm7) CPU_RUN_SYNCINT(2, Arm7);
		}
	}

	BurnTimerEndFrame(nCyclesTotal[1]);
	ics2115_update(nBurnSoundLen);

	nCyclesDone[0]			= SekTotalCycles() - nCyclesTotal[0];
	nCyclesDone[1]			= ZetTotalCycles() - nCyclesTotal[1];

	if (!OldCodeMode) {
		if (nEnableArm7) {
			nCyclesDone[2]	= Arm7TotalCycles() - nCyclesTotal[2];
			Arm7Close();
		}
	}
	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	if (OldCodeMode)
		memcpy(PGMSprBuf, PGM68KRAM /* Sprite RAM 0-bff */, 0xa00); // buffer sprites
	
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
		ba.Data			= PGMBgRAM;
		ba.nLen			= 0x004000;
		ba.nAddress		= 0x900000;
		ba.szName		= "Bg RAM";
		BurnAcb(&ba);

		ba.Data			= PGMTxtRAM;
		ba.nLen			= 0x003000;
		ba.nAddress		= 0x904000;
		ba.szName		= "Tx RAM";
		BurnAcb(&ba);

		ba.Data			= PGMRowRAM;
		ba.nLen			= 0x001000;
		ba.nAddress		= 0x907000;
		ba.szName		= "Row Scroll";
		BurnAcb(&ba);

		if (!OldCodeMode) {
			ba.Data		= PGMPalRAM;
			ba.nLen		= 0x002000;
			ba.nAddress	= 0xA00000;
			ba.szName	= "Palette RAM";
			BurnAcb(&ba);

			ba.Data		= PGMSprBuf;
			ba.nLen		= 0x001000;
			ba.nAddress	= 0xB00000;
			ba.szName	= "Sprite Buffer";
			BurnAcb(&ba);
		} else {
			ba.Data		= PGMPalRAM;
			ba.nLen		= 0x001400;
			ba.nAddress	= 0xA00000;
			ba.szName	= "Palette RAM";
			BurnAcb(&ba);

			ba.Data		= PGMVidReg;
			ba.nLen		= 0x010000;
			ba.nAddress	= 0xB00000;
			ba.szName	= "Video Regs";
			BurnAcb(&ba);
		}
		
		ba.Data			= PGMZoomRAM;
		ba.nLen			= 0x000040;
		ba.nAddress		= 0xB01000;
		ba.szName		= "Zoom Regs";
		BurnAcb(&ba);
		
		ba.Data			= RamZ80;
		ba.nLen			= 0x010000;
		ba.nAddress		= 0xC10000;
		ba.szName		= "Z80 RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data			= PGM68KRAM;
		ba.nLen			= 0x020000;
		ba.nAddress		= 0x800000;
		ba.szName		= "68K RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
	
		SekScan(nAction);
		ZetScan(nAction);

		v3021Scan();

		hold_coin.scan();
		clear_opposite.scan();

		SCAN_VAR(nPgmCurrentBios);

		SCAN_VAR(nSoundlatch);
		SCAN_VAR(bSoundlatchRead);

		SCAN_VAR(pgm_bg_scrollx);
		SCAN_VAR(pgm_bg_scrolly);
		SCAN_VAR(pgm_fg_scrollx);
		SCAN_VAR(pgm_fg_scrolly);
		SCAN_VAR(pgm_video_control);
		SCAN_VAR(pgm_unk_video_flags);
		SCAN_VAR(pgm_z80_connect_bus);

		ics2115_scan(nAction, pnMin);
	}

	if (pPgmScanCallback) {
		pPgmScanCallback(nAction, pnMin);
	}

 	return 0;
}
