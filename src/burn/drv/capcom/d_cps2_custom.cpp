#include "cps.h"
#include "xmvsf3v3.h"

// CPS2 hardware extensions used by custom games.

struct CpsDisplayState {
	char* szPalette;
	char* szRegisters;
	char* szFrg;
	char* szObjects;
	char* szEffectivePalette;
	bool bRawPalette;
};
static const CpsDisplayState* pCpsDisplayState = NULL;
static bool CpsPaletteRestored = false;
static UINT16 Cps2PlusPalette[0xc00];

// Palette RAM can already contain the next DMA's colors. Save the effective
// palette instead; hardware words can be converted to the current pixel format.
static INT32 CpsPalVisualScan(INT32 nAction)
{
	if (!pCpsDisplayState || !(nAction & ACB_DRIVER_DATA)) return 0;

	struct BurnArea ba;
	memset(&ba, 0, sizeof(ba));
	if (!pCpsDisplayState->bRawPalette) {
		// Converted colors use the frontend's current pixel format.
		ba.Data = CpsPal;
		ba.nLen = 0xc00 * sizeof(UINT32);
	} else {
		ba.Data = Cps2PlusPalette;
		ba.nLen = 0xc00 * sizeof(UINT16);
	}
	ba.szName = pCpsDisplayState->szEffectivePalette;
	BurnAcb(&ba);

	if (nAction & ACB_WRITE) {
		if (pCpsDisplayState->bRawPalette) {
			CpsPalRecalc(Cps2PlusPalette);
		}
		CpsPaletteRestored = true;
	}

	return 0;
}

// Preserve latched display state so a paused redraw does not read the next frame's RAM.
static INT32 CpsVisualScan(INT32 nAction)
{
	if (!pCpsDisplayState || !(nAction & ACB_DRIVER_DATA)) return 0;

	struct BurnArea ba;
	memset(&ba, 0, sizeof(ba));
	ba.Data = CpsSavePal;		ba.nLen = 0x2000;								ba.szName = pCpsDisplayState->szPalette;
	BurnAcb(&ba);
	ba.Data = CpsSaveReg[0];	ba.nLen = 0x100 * (MAX_RASTER + 1);				ba.szName = pCpsDisplayState->szRegisters;
	BurnAcb(&ba);
	ba.Data = CpsSaveFrg[0];	ba.nLen = 0x20;									ba.szName = pCpsDisplayState->szFrg;
	BurnAcb(&ba);

	if (CpsObjScan(nAction, pCpsDisplayState->szObjects)) return 1;

	return CpsPalVisualScan(nAction);
}

static INT32 CpsPalConsumeRestoredCache()
{
	if (!pCpsDisplayState || !CpsPaletteRestored) return 0;

	CpsPaletteRestored = false;

	return 1;
}

static INT32 CpsDisplayInit()
{
	CpsObjReset();
	memset(CpsPal, 0, 0xc00 * sizeof(UINT32));
	CpsPaletteRestored = false;
	return 0;
}

static void CpsReplaySoundScan(INT32 nAction, INT32*)
{
	QsndScan(nAction, 1);
}

static INT32 DrvExit()
{
	pCpsDisplayState = NULL;
	CpsPaletteRestored = false;
	Cps2Volume = 39;
	Cps2DisableDigitalVolume = 0;
	return CpsExit();
}

static const CpsDisplayState Cps2PlusDisplayState = {
	"Latched palette", "Latched registers", "Latched frg",
	"Sprite lag buffers", "Effective palette", true
};

static UINT8* Cps2PlusObjectBanks = NULL;

static void Cps2PlusPaletteUpdate(INT32 nPage, const UINT16* pPalette)
{
	// Disabled DMA destinations retain their previous hardware colors.
	memcpy(Cps2PlusPalette + 0x200 * nPage, pPalette, 0x200 * sizeof(UINT16));
}

static void Cps2PlusObjectGet(UINT8* pExtra, INT32 nEntry, INT32 nSource)
{
	// Blank entries are skipped, so latch the selector with the compacted list.
	pExtra[nEntry] = Cps2PlusObjectBanks[((nCpsObjectBank ^ 1) << 10) + nSource];
}

static INT32 Cps2PlusObjectTile(UINT16* pObject, INT32 nTile, const UINT8* pExtra, INT32 nEntry)
{
	// Row arithmetic wraps within the selected latched tile bank.
	return (nTile & 0xffff) | ((BURN_ENDIAN_SWAP_INT16(pObject[1]) & 0x6000) << 3) | (pExtra[nEntry] << 18);
}

static INT32 Cps2PlusScroll(INT32 nAddress, INT32 nAttributes, INT32 nLayer)
{
	return ((nAddress + nCpsGfxScroll[nLayer]) & 0x1ffffff) | ((nAttributes & 0x200) << 16);
}

static UINT32 Cps2PlusObjectOffset(UINT32 a)
{
	// A15 and the bank register choose physical RAM; A13 and A14 mirror its entries.
	return (((a >> 15) ^ nCpsObjectBank) & 1) * 0x8000 + (a & 0x1fff);
}

static UINT8 __fastcall Cps2PlusObjectReadByte(UINT32 a)
{
	return CpsRam708[Cps2PlusObjectOffset(a) ^ 1];
}

static UINT16 __fastcall Cps2PlusObjectReadWord(UINT32 a)
{
	return BURN_ENDIAN_SWAP_INT16(*(UINT16*)(CpsRam708 + Cps2PlusObjectOffset(a)));
}

static void Cps2PlusObjectWrite(UINT32 a)
{
	// A14 is a write selector, not an extra address line in object RAM.
	UINT32 nOffset = Cps2PlusObjectOffset(a);
	Cps2PlusObjectBanks[(nOffset >> 15) * 0x400 + ((a & 0x1fff) >> 3)] = (a >> 14) & 1;
}

static void __fastcall Cps2PlusObjectWriteByte(UINT32 a, UINT8 d)
{
	CpsRam708[Cps2PlusObjectOffset(a) ^ 1] = d;
	Cps2PlusObjectWrite(a);
}

static void __fastcall Cps2PlusObjectWriteWord(UINT32 a, UINT16 d)
{
	*(UINT16*)(CpsRam708 + Cps2PlusObjectOffset(a)) = BURN_ENDIAN_SWAP_INT16(d);
	Cps2PlusObjectWrite(a);
}

static void Cps2PlusMapMemory()
{
	SekMapMemory(CpsRom + 0x400000, 0xa00000, 0xdfffff, MAP_ROM);
	// Both banks and their mirrors use the same handlers; the register selects physical RAM.
	SekMapHandler(4, 0x700000, 0x70ffff, MAP_RAM);
	SekSetReadByteHandler(4, Cps2PlusObjectReadByte);
	SekSetReadWordHandler(4, Cps2PlusObjectReadWord);
	SekSetWriteByteHandler(4, Cps2PlusObjectWriteByte);
	SekSetWriteWordHandler(4, Cps2PlusObjectWriteWord);
}

static UINT8 __fastcall Cps2PlusSoundRead(UINT16 a)
{
	if (a == 0xd007) return (QscRead() & 0x80) | 0x60 | QsndZGetBank();
	return QsndZRead(a);
}

static void __fastcall Cps2PlusSoundWrite(UINT16 a, UINT8 d)
{
	if (a == 0xd003) {
		INT32 nBank = d & 0x1f;
		QsndZSetBank(nBank < 30 ? nBank : 0);
		return;
	}
	QsndZWrite(a, d);
}

static void Cps2PlusSoundInit()
{
	if (nCpsZRomLen == 0x80000) {
		ZetOpen(0);
		ZetSetReadHandler(Cps2PlusSoundRead);
		ZetSetWriteHandler(Cps2PlusSoundWrite);
		ZetClose();
	}
}

static INT32 Cps2PlusReset()
{
	memset(Cps2PlusObjectBanks, 0, 0x800);
	CpsObjReset();
	ZetOpen(0);
	QsndZReset();
	QscReset();
	ZetClose();
	return 0;
}

static INT32 Cps2PlusExit()
{
	BurnFree(Cps2PlusObjectBanks);
	return DrvExit();
}

static INT32 Cps2PlusMemoryScan(INT32 nAction, INT32*)
{
	if (nAction & ACB_DRIVER_DATA) {
		struct BurnArea ba;
		memset(&ba, 0, sizeof(ba));
		ba.Data = Cps2PlusObjectBanks;
		ba.nLen = 0x800;
		ba.szName = "CPS2+ object banks";
		BurnAcb(&ba);
		if (nAction & ACB_WRITE) {
			for (INT32 i = 0; i < 0x800; i++) {
				if (Cps2PlusObjectBanks[i] > 1) return 1;
			}
		}
	}
	return 0;
}

static INT32 Cps2PlusScan(INT32 nAction, INT32* pnMin)
{
	INT32 nRet = CpsAreaScan(nAction, pnMin);
	return nRet ? nRet : CpsVisualScan(nAction);
}

static void Cps2PlusFrameStart()
{
	// The restored palette is already current; subsequent DMA updates enabled pages.
	if (CpsPalConsumeRestoredCache()) CpsRecalcPal = 0;
}

// Validate the expanded board before the common CPS2 loader allocates or decodes ROMs.
static INT32 Cps2PlusCheckROMs(UINT32 nSampleMask)
{
	UINT32 nSize[16] = { 0 };
	UINT32 nGfxLane = 0, nGfxSize = 0;
	struct BurnRomInfo ri;

	for (INT32 i = 0; !BurnDrvGetRomInfo(&ri, i); i++) {
		if (!ri.nLen) continue;
		UINT32 nType = ri.nType & 0x0f;
		if ((nGfxLane & 3) && nType != CPS2_GFX) return 1;
		UINT32 nLimit;
		switch (nType) {
			case CPS2_PRG_68K:        nLimit = 0x800000; break;
			case CPS2_GFX:            nLimit = 0x4000000; break;
			case CPS2_PRG_Z80:        nLimit = 0x80000; break;
			case CPS2_QSND:           nLimit = 0x2000000; break;
			case CPS2_ENCRYPTION_KEY: nLimit = 0x14; break;
			default: return 1;
		}
		if (ri.nLen > nLimit - nSize[nType]) return 1;
		if ((nType == CPS2_PRG_68K || nType == CPS2_QSND) && (ri.nLen & 1)) return 1;
		if (nType == CPS2_ENCRYPTION_KEY && ri.nLen != 0x14) return 1;
		if (nType == CPS2_GFX) {
			if ((nGfxLane & 3) == 0) {
				// The common loader packs smaller graphics groups after larger ones.
				if (nGfxSize && ri.nLen > nGfxSize) return 1;
				nGfxSize = ri.nLen;
			}
			if (ri.nLen < 0x80000 || (ri.nLen & 0x7ffff) || ri.nLen != nGfxSize) return 1;
			nGfxLane++;
		}
		nSize[nType] += ri.nLen;
	}

	if (nSize[CPS2_PRG_68K] != 0x800000 || nSize[CPS2_GFX] < 0x2000000 ||
		(nSize[CPS2_PRG_Z80] != 0x40000 && nSize[CPS2_PRG_Z80] != 0x80000) ||
		nSize[CPS2_ENCRYPTION_KEY] != 0x14 || (nGfxLane & 3)) return 1;
	UINT32 nSamples = nSize[CPS2_QSND];
	if (nSamples != 0x800000 && nSamples != 0x1000000 && nSamples != 0x2000000) return 1;
	return nSamples <= nSampleMask;
}

static INT32 Cps2PlusInit(INT32 nSampleBits)
{
	Cps = 2;
	nCpsQSamMask = 0;
	if (nSampleBits != 23 && nSampleBits != 24 && nSampleBits != 25) return 1;

	UINT32 nSampleMask = (1U << nSampleBits) - 1;
	if (Cps2PlusCheckROMs(nSampleMask)) return 1;

	Cps2PlusObjectBanks = (UINT8*)BurnMalloc(0x800);
	if (Cps2PlusObjectBanks == NULL) return 1;
	memset(Cps2PlusObjectBanks, 0, 0x800);

	pCpsDisplayState = &Cps2PlusDisplayState;
	memset(Cps2PlusPalette, 0, sizeof(Cps2PlusPalette));
	CpsPalUpdateCallbackFunction = Cps2PlusPaletteUpdate;
	nCpsObjExtraLen = 0x400;
	Cps2ObjGetCallbackFunction = Cps2PlusObjectGet;
	Cps2ObjTileCallbackFunction = Cps2PlusObjectTile;
	CpsRunInitCallbackFunction = CpsDisplayInit;
	CpsDrawPaletteCallbackFunction = CpsPalConsumeRestoredCache;
	CpsSoundScanCallbackFunction = CpsReplaySoundScan;
	bCpsObjectBankHandlers = 1;
	CpsMemScanExtraCallbackFunction = Cps2PlusMemoryScan;
	Cps2ScrollCallbackFunction = Cps2PlusScroll;
	CpsRunResetCallbackFunction = Cps2PlusReset;
	CpsRunFrameStartCallbackFunction = Cps2PlusFrameStart;
	nCpsQSamMask = nSampleMask;
	INT32 nRet = Cps2Init(0x400000, 0x4000000, 0);
	if (!nRet) {
		SekOpen(0);
		Cps2PlusMapMemory();
		SekClose();
		Cps2PlusSoundInit();
	}
	return nRet;
}

// SFZ3 Mix: linear ROM dumps, expanded sprite addresses and external music samples.
static UINT8 AspectDIP;
static UINT8 AspectDIPLast;

// samples:
// empty + 0x3f L, channel 0, sample + 0x00
// empty + 0x3f R, channel 1, sample + 0x40

static void samples_l_start(int channel, int sample, int loop)
{
	BurnSampleChannelPlay(channel + 0x00, sample + 0x00, loop);
}

static int samples_l_playing()
{
	return BurnSampleGetChannelStatus(0x00) == SAMPLE_PLAYING;
}
static int samples_r_playing()
{
	return BurnSampleGetChannelStatus(0x01) == SAMPLE_PLAYING;
}

static void samples_r_start(int channel, int sample, int loop)
{
	BurnSampleChannelPlay(channel + 0x01, sample + 0x40, loop);
}

// ----------------------------------------------------------------------------
static void qsound_cps2turbo_write(UINT32 offset, UINT8 data) //Zero800 allows you to write and signal when a new song is played
{
	UINT8 *m_qsound_sharedram1 = CpsZRamC0;
	UINT8 *m_qsound_sharedram2 = CpsZRamF0;
	if (offset == 0x0001 && (data & 0xff) == 0x00 && m_qsound_sharedram1[0x00] == 0xff) //stop samples sound
	{
		samples_l_start(0, 0, 0);
		samples_r_start(0, 0, 0);
		bprintf(0, _T("Stop: %02x to %x\n"), offset, offset);
	}

	if (offset == 0xffe)
	{
		if (m_qsound_sharedram1[0x80] == 0x77) //stop music condition in audiocpu = C080 (z80 hacked)
		{
			samples_l_start(0, 0, 0); //init L samples
			samples_r_start(0, 0, 0); //init R samples
			m_qsound_sharedram2[0x27] = 0xc9; //Enable Qsound Volume
			m_qsound_sharedram1[0x80] = 0xff; //Reset music id flag
			m_qsound_sharedram1[0x81] = 0xff; //Reset music id flag
			bprintf(0, _T("Clear: %02x to %x\n"), data, offset);
		}
		else
			if (m_qsound_sharedram1[0x81] > 0x00 && m_qsound_sharedram1[0x81] < 0x40) //play music condition in audiocpu = C081 (z80 hacked)
			{
				int data_byte = m_qsound_sharedram1[0x81];
				m_qsound_sharedram1[0x80] = 0xff; //Reset music id flag
				m_qsound_sharedram1[0x81] = 0xff; //Reset music id flag
				bprintf(0, _T("Music: %02x to %x\n"), data_byte, offset);
				samples_l_start(0, data_byte, 1);
				if (samples_l_playing() == 1)
				{
					m_qsound_sharedram2[0x27] = 0x00; //Qsound music volume = 0
				}
				else
					samples_l_start(0, 0, 0); //init L samples

				samples_r_start(0, data_byte, 1);
				if (samples_r_playing() == 1)
				{
					m_qsound_sharedram2[0x27] = 0x00; //Qsound music volume = 0
				}
				else
					samples_r_start(0, 0, 0); //init R samples
			}
			else
				if (m_qsound_sharedram2[0x27] == 0x00 && samples_l_playing() != 1) //if loadstate and qsound volume = 0 && samples stopped
				{
					samples_l_start(0, 0, 0); //init L samples
					samples_r_start(0, 0, 0); //init R samples
					m_qsound_sharedram2[0x27] = 0xc9; //Enable Qsound Volume
					m_qsound_sharedram1[0x80] = 0xff; //Reset music id flag
					m_qsound_sharedram1[0x81] = 0xff; //Reset music id flag
					bprintf(0, _T("Volume0: %02x to %x\n"), data, offset);
				}
				else
					if (m_qsound_sharedram2[0x27] != 0x00 && samples_l_playing() == 1) //if loadstate and qsound volume != 0 && samples playing
					{
						samples_l_start(0, 0, 0); //init L samples
						samples_r_start(0, 0, 0); //init R samples
						m_qsound_sharedram2[0x27] = 0xc9; //Enable Qsound Volume
						m_qsound_sharedram1[0x80] = 0xff; //Reset music id flag
						m_qsound_sharedram1[0x81] = 0xff; //Reset music id flag
						bprintf(0, _T("Volume0: %02x to %x\n"), data, offset);
					}
					else
						if ((data & 0xff) == 0x55) //init samples in rom start
						{
							samples_l_start(0, 0, 0);
							samples_r_start(0, 0, 0);
						}
	}

	m_qsound_sharedram1[offset] = data;	//Play data Qsound
}

static void check_aspect()
{
	if ((AspectDIP & 0x03) != AspectDIPLast) {
		INT32 aspects[3][2] = { { 16, 9 }, { 4, 3 }, { 112, 81 } };
		AspectDIPLast = AspectDIP & 0x03;

		INT32 nAspectX, nAspectY;
		BurnDrvGetAspect(&nAspectX, &nAspectY);

		if (nAspectX != aspects[AspectDIPLast][0] || nAspectY != aspects[AspectDIPLast][1]) {
			bprintf(0, _T("*  CPS-2: Changing to %d:%d aspect\n"), aspects[AspectDIPLast][0], aspects[AspectDIPLast][1]);
			BurnDrvSetAspect(aspects[AspectDIPLast][0], aspects[AspectDIPLast][1]);
			Reinitialise();
		}
	}
}

static void __fastcall Cps2MixWriteByte(UINT32 a, UINT8 d)
{
	if ((a & 0xfffff0) == 0x665000 || (a & 0xfffff0) == 0xfffff0) {
		CpsFrg[a & 0x0f] = d;
		return;
	}
	if ((a & 0xfffff0) != 0x400000) CpsWriteByte(a, d);
}

void __fastcall CPSQSoundC0WriteByte(UINT32 a, UINT8 d);

static void __fastcall Cps2MixSoundWrite(UINT32 a, UINT8 d)
{
	if (a & 1) qsound_cps2turbo_write((a & 0x1fff) >> 1, d);
	CPSQSoundC0WriteByte(a, d);
}

static void __fastcall Cps2MixRamWriteByte(UINT32 a, UINT8 d)
{
	if (a >= 0xfffff0 && a <= 0xfffffb) CpsFrg[a & 0x0f] = d;
	CpsRamFF[(a - 0xff0000) ^ 1] = d;
}

static void __fastcall Cps2MixRamWriteWord(UINT32 a, UINT16 d)
{
	SEK_DEF_WRITE_WORD(3, a, d);
}

static void __fastcall Cps2MixSpriteWriteByte(UINT32 a, UINT8 d)
{
	UINT32 nOffset = (a - 0x700000) ^ 1;
	CpsRam708[nOffset] = CpsRam708[nOffset + 0x8000] = d;
}

static void __fastcall Cps2MixSpriteWriteWord(UINT32 a, UINT16 d)
{
	UINT16* pRam = (UINT16*)CpsRam708;
	UINT32 nOffset = (a - 0x700000) >> 1;
	pRam[nOffset] = pRam[nOffset + 0x4000] = BURN_ENDIAN_SWAP_INT16(d);
}

static INT32 Cps2MixScroll(INT32 nAddress, INT32, INT32 nLayer)
{
	if (nLayer == 3) nAddress &= ~nCpsGfxScroll[3];
	return nAddress + nCpsGfxScroll[nLayer];
}

static INT32 Cps2MixObjectTile(UINT16* pObject, INT32 nTile, const UINT8*, INT32)
{
	if (BURN_ENDIAN_SWAP_INT16(pObject[1]) & 0x1000) pObject[1] |= BURN_ENDIAN_SWAP_INT16(0x8000);
	return nTile + ((BURN_ENDIAN_SWAP_INT16(pObject[1]) & 0x8000) << 3);
}

static void Cps2MixMapMemory()
{
	SekSetWriteByteHandler(0, Cps2MixWriteByte);
	SekSetWriteByteHandler(1, Cps2MixSoundWrite);
	SekMapHandler(3, 0xff0000, 0xffffff, MAP_WRITE);
	SekSetWriteByteHandler(3, Cps2MixRamWriteByte);
	SekSetWriteWordHandler(3, Cps2MixRamWriteWord);
	SekMapHandler(4, 0x700000, 0x701fff, MAP_WRITE);
	SekSetWriteByteHandler(4, Cps2MixSpriteWriteByte);
	SekSetWriteWordHandler(4, Cps2MixSpriteWriteWord);
}

static void Cps2MixSoundInit()
{
	BurnSampleInit(1);
	for (INT32 i = 0; i < 0x40; i++) {
		BurnSampleSetRoute(i + 0x00, BURN_SND_SAMPLE_ROUTE_1, 0.10, BURN_SND_ROUTE_LEFT);
		BurnSampleSetRoute(i + 0x00, BURN_SND_SAMPLE_ROUTE_2, 0.10, BURN_SND_ROUTE_NONE);
		BurnSampleSetRoute(i + 0x40, BURN_SND_SAMPLE_ROUTE_1, 0.10, BURN_SND_ROUTE_NONE);
		BurnSampleSetRoute(i + 0x40, BURN_SND_SAMPLE_ROUTE_2, 0.10, BURN_SND_ROUTE_RIGHT);
	}
}

static INT32 Cps2MixReset()
{
	AspectDIPLast = 0xff;
	check_aspect();
	return 0;
}

static INT32 Cps2MixExit()
{
	BurnSampleExit();
	return DrvExit();
}

static INT32 Cps2MixFrame()
{
	check_aspect();
	INT32 nRet = Cps2Frame();
	if (pBurnSoundOut) BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
	return nRet;
}

static void Cps2MixSoundScan(INT32 nAction, INT32* pnMin)
{
	if (nAction & ACB_DRIVER_DATA) BurnSampleScan(nAction, pnMin);
	QsndScan(nAction);
}

static INT32 Cps2MixLoadTiles()
{
	UINT8* Rom = (UINT8*)BurnMalloc(nCpsGfxLen);
	if (Rom == NULL) return 1;
	memcpy(Rom, CpsGfx, nCpsGfxLen);
	memset(CpsGfx, 0, nCpsGfxLen);
	Cps2LoadTilesInterleaved(CpsGfx, Rom, nCpsGfxLen);
	BurnFree(Rom);
	return 0;
}

static INT32 Cps2MixInit()
{
	bCpsUseParentBlend = 0;
	nQsndZ80Clock = 16000000;
	nQsndZBankOffset = 0x10000;
	Cps2ScrollCallbackFunction = Cps2MixScroll;
	Cps2ObjTileCallbackFunction = Cps2MixObjectTile;
	CpsSoundScanCallbackFunction = Cps2MixSoundScan;
	CpsRunResetCallbackFunction = Cps2MixReset;
	nCPS68KClockspeed = 32000000;
	INT32 nRet = Cps2Init(0x400000, 0, 1);
	if (!nRet) {
		if (Cps2MixLoadTiles()) return 1;
		SekOpen(0);
		Cps2MixMapMemory();
		SekClose();
		Cps2MixSoundInit();
	}
	return nRet;
}

// XMvsSF 3v3: an input device, extra work RAM and optional wide-screen graphics.
static const CpsDisplayState CpsXmvDisplayState = {
	"XMvsSF 3v3 latched palette", "XMvsSF 3v3 latched registers", "XMvsSF 3v3 latched frg",
	"XMvsSF 3v3 sprite lag buffers", "XMvsSF 3v3 effective palette", false
};

static UINT8 CpsXmvSixButtons[XMV6_PLAYERS][13];
static INT32 CpsXmvWide;
static UINT16 CpsXmvSixCapabilities = 0;
static XmvSixInputState CpsXmvSixState;
static UINT8 *CpsXmvSixWorkRam = NULL;

static INT32 CpsXmvSixExit()
{
	CpsXmvWide = 0;
	BurnFree(CpsXmvSixWorkRam);
	return DrvExit();
}

static INT32 CpsXmvSixReset()
{
	XmvSixReset(&CpsXmvSixState);
	memset(CpsXmvSixWorkRam, 0, XMV6_WORK_RAM_BYTES);

	return 0;
}

static INT32 CpsXmvSixScan(INT32 nAction, INT32* pnMin)
{
	INT32 nRet = CpsAreaScan(nAction, pnMin);
	if (nRet) return nRet;

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

	return CpsVisualScan(nAction);
}

// Wide screen: while the ROM keeps 'XS1X' at 0x813ff8, scroll1 columns 64-127 come from 0x814000
static const UINT8* CpsXmvScroll1Extension()
{
	const UINT8* tag = CpsXmvSixWorkRam + 0x3ff8;
	if (tag[0] != 'X' || tag[1] != 'S' || tag[2] != '1' || tag[3] != 'X') return NULL;

	return CpsXmvSixWorkRam + 0x4000;
}

// Once a frame, before the input ports are compiled
static INT32 CpsXmvSixSample()
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
	return 2;
}

static UINT8 __fastcall CpsXmvReadByte(UINT32 a)
{
	return XmvSixReadByte(&CpsXmvSixState, CpsXmvSixCapabilities, a - XMV6_BASE);
}

static UINT16 __fastcall CpsXmvReadWord(UINT32 a)
{
	return XmvSixReadWord(&CpsXmvSixState, CpsXmvSixCapabilities, a - XMV6_BASE);
}

static UINT8 __fastcall CpsXmvRamReadByte(UINT32 a)
{
	return CpsXmvSixWorkRam[a - XMV6_WORK_RAM_BASE];
}

static UINT16 __fastcall CpsXmvRamReadWord(UINT32 a)
{
	return XmvSixRamReadWord(CpsXmvSixWorkRam, a - XMV6_WORK_RAM_BASE);
}

static void __fastcall CpsXmvRamWriteByte(UINT32 a, UINT8 d)
{
	CpsXmvSixWorkRam[a - XMV6_WORK_RAM_BASE] = d;
}

static void __fastcall CpsXmvRamWriteWord(UINT32 a, UINT16 d)
{
	XmvSixRamWriteWord(CpsXmvSixWorkRam, a - XMV6_WORK_RAM_BASE, d);
}

static void CpsXmvMapMemory()
{
	// Instruction reads use the same device decode as data reads.
	SekMapHandler(5, XMV6_BASE, XMV6_BASE + XMV6_WINDOW_BYTES - 1, MAP_ROM);
	SekSetReadByteHandler(5, CpsXmvReadByte);
	SekSetReadWordHandler(5, CpsXmvReadWord);
	SekMapHandler(6, XMV6_WORK_RAM_BASE, XMV6_WORK_RAM_BASE + XMV6_WORK_RAM_BYTES - 1, MAP_RAM);
	SekSetReadByteHandler(6, CpsXmvRamReadByte);
	SekSetReadWordHandler(6, CpsXmvRamReadWord);
	SekSetWriteByteHandler(6, CpsXmvRamWriteByte);
	SekSetWriteWordHandler(6, CpsXmvRamWriteWord);
}

static INT32 CpsXmvObjectPalette(INT32 n, UINT16 x, UINT16 y, INT32 a, UINT32* pPalette)
{
	UINT32 nColor;
	if (!XmvPortraitObject(n, x, y, a, CpsXmvWide != 0, &nColor)) return 0;
	UINT16 nColors[16];
	if (!XmvPortraitColors(CpsRom, nCpsRomLen, n, nColor, nColors)) return -1;
	for (INT32 pen = 0; pen < 16; pen++) {
		UINT32 c = nColors[pen];
		pPalette[pen] = BurnHighCol(((c >> 8) & 15) * 17, ((c >> 4) & 15) * 17, (c & 15) * 17, 0);
	}
	return 1;
}

static INT32 CpsXmvSixInit(INT32 bWide)
{
	CpsXmvSixWorkRam = (UINT8*)BurnMalloc(XMV6_WORK_RAM_BYTES);
	if (CpsXmvSixWorkRam == NULL) return 1;

	CpsXmvWide = bWide ? 1 : 0;
	CpsXmvSixCapabilities = XMV6_CAP_WORK_RAM | XMV6_CAP_PORTRAITS | XMV6_CAP_PORTRAITS_6 | XMV6_CAP_PORTRAITS_12 | XMV6_CAP_PORTRAITS_18;
	if (bWide) CpsXmvSixCapabilities |= XMV6_CAP_WIDE | XMV6_CAP_WIDE_SCROLL1;
	memset(CpsXmvSixButtons, 0, sizeof(CpsXmvSixButtons));
	CpsXmvSixReset();

	pCpsDisplayState = &CpsXmvDisplayState;
	CpsRunInitCallbackFunction = CpsDisplayInit;
	CpsDrawPaletteCallbackFunction = CpsPalConsumeRestoredCache;
	CpsSoundScanCallbackFunction = CpsReplaySoundScan;
	CpsRwGetInpCallbackFunction = CpsXmvSixSample;
	Cps2Scroll1CallbackFunction = bWide ? CpsXmvScroll1Extension : NULL;
	Cps2ObjPaletteCallbackFunction = CpsXmvObjectPalette;
	CpsRunResetCallbackFunction = CpsXmvSixReset;
	INT32 nRet = Cps2Init();
	if (!nRet) {
		SekOpen(0);
		CpsXmvMapMemory();
		SekClose();
	}
	return nRet;
}

// Input definitions

static struct BurnInputInfo Cps2CustomInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Weak Punch"    , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Medium Punch"  , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Strong Punch"  , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },
	{"P1 Weak Kick"     , BIT_DIGITAL  , CpsInp011+0, "p1 fire 4" },
	{"P1 Medium Kick"   , BIT_DIGITAL  , CpsInp011+1, "p1 fire 5" },
	{"P1 Strong Kick"   , BIT_DIGITAL  , CpsInp011+2, "p1 fire 6" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Weak Punch"    , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Medium Punch"  , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Strong Punch"  , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },
	{"P2 Weak Kick"     , BIT_DIGITAL  , CpsInp011+4, "p2 fire 4" },
	{"P2 Medium Kick"   , BIT_DIGITAL  , CpsInp011+5, "p2 fire 5" },
	{"P2 Strong Kick"   , BIT_DIGITAL  , CpsInp020+6, "p2 fire 6" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "volumeup"  },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "volumedown"},
};

STDINPUTINFO(Cps2Custom)

static struct BurnInputInfo Sfz3mixInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsInp020+4, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsInp020+0, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsInp001+3, "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsInp001+2, "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsInp001+1, "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsInp001+0, "p1 right"  },
	{"P1 Weak Punch"    , BIT_DIGITAL  , CpsInp001+4, "p1 fire 1" },
	{"P1 Medium Punch"  , BIT_DIGITAL  , CpsInp001+5, "p1 fire 2" },
	{"P1 Strong Punch"  , BIT_DIGITAL  , CpsInp001+6, "p1 fire 3" },
	{"P1 Weak Kick"     , BIT_DIGITAL  , CpsInp011+0, "p1 fire 4" },
	{"P1 Medium Kick"   , BIT_DIGITAL  , CpsInp011+1, "p1 fire 5" },
	{"P1 Strong Kick"   , BIT_DIGITAL  , CpsInp011+2, "p1 fire 6" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsInp020+5, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsInp020+1, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsInp000+3, "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsInp000+2, "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsInp000+1, "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsInp000+0, "p2 right"  },
	{"P2 Weak Punch"    , BIT_DIGITAL  , CpsInp000+4, "p2 fire 1" },
	{"P2 Medium Punch"  , BIT_DIGITAL  , CpsInp000+5, "p2 fire 2" },
	{"P2 Strong Punch"  , BIT_DIGITAL  , CpsInp000+6, "p2 fire 3" },
	{"P2 Weak Kick"     , BIT_DIGITAL  , CpsInp011+4, "p2 fire 4" },
	{"P2 Medium Kick"   , BIT_DIGITAL  , CpsInp011+5, "p2 fire 5" },
	{"P2 Strong Kick"   , BIT_DIGITAL  , CpsInp020+6, "p2 fire 6" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset  , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1, "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2, "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp , "volumeup"  },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn, "volumedown"},
	{"Dip A"            , BIT_DIPSWITCH, &AspectDIP , "dip"       },
};

STDINPUTINFO(Sfz3mix)

// sfz3mix 0.20+ only!
static struct BurnDIPInfo Sfz3mixDIPList[] =
{
	DIP_OFFSET(0x1d)
	{0x00, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    3, "Aspect Ratio"	},
	{0x00, 0x01, 0x03, 0x00, "16:9"			},
	{0x00, 0x01, 0x03, 0x01, "4:3"			},
	{0x00, 0x01, 0x03, 0x02, "112:81"		},
};

STDDIPINFO(Sfz3mix)


// Street Fighter Zero 3 Mix v0.32 (hacked by Zero800)
// https://sfz3mix.github.io/

static struct BurnSampleInfo Sfz3mixSampleDesc[] = {
	{ "empty", SAMPLE_NOLOOP },
	{ "L01",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L02",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L03",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L04",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L05",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L06",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L07",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L08",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L09",   SAMPLE_NOLOOP },
	{ "L0A",   SAMPLE_NOLOOP },
	{ "L0B",   SAMPLE_NOLOOP },
	{ "L0C",   SAMPLE_NOLOOP },
	{ "L0D",   SAMPLE_NOLOOP },
	{ "L0E",   SAMPLE_NOLOOP },
	{ "L0F",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L10",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L11",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L12",   SAMPLE_NOLOOP },
	{ "L13",   SAMPLE_NOLOOP },
	{ "L14",   SAMPLE_NOLOOP },
	{ "L15",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L16",   SAMPLE_NOLOOP },
	{ "L17",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L18",   SAMPLE_NOLOOP },
	{ "L19",   SAMPLE_NOLOOP },
	{ "L1A",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L1B",   SAMPLE_NOLOOP },
	{ "L1C",   SAMPLE_NOLOOP },
	{ "L1D",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L1E",   SAMPLE_NOLOOP },
	{ "L1F",   SAMPLE_NOLOOP },
	{ "L20",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L21",   SAMPLE_NOLOOP },
	{ "L22",   SAMPLE_NOLOOP },
	{ "L23",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L24",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L25",   SAMPLE_NOLOOP },
	{ "L26",   SAMPLE_NOLOOP },
	{ "L27",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L28",   SAMPLE_NOLOOP },
	{ "L29",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L2A",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L2B",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L2C",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L2D",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L2E",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L2F",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L30",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L31",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L32",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L33",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L34",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L35",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L36",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L37",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L38",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L39",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L3A",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L3B",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L3C",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L3D",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L3E",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "L3F",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "empty", SAMPLE_NOLOOP },
	{ "R01",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R02",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R03",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R04",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R05",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R06",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R07",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R08",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R09",   SAMPLE_NOLOOP },
	{ "R0A",   SAMPLE_NOLOOP },
	{ "R0B",   SAMPLE_NOLOOP },
	{ "R0C",   SAMPLE_NOLOOP },
	{ "R0D",   SAMPLE_NOLOOP },
	{ "R0E",   SAMPLE_NOLOOP },
	{ "R0F",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R10",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R11",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R12",   SAMPLE_NOLOOP },
	{ "R13",   SAMPLE_NOLOOP },
	{ "R14",   SAMPLE_NOLOOP },
	{ "R15",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R16",   SAMPLE_NOLOOP },
	{ "R17",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R18",   SAMPLE_NOLOOP },
	{ "R19",   SAMPLE_NOLOOP },
	{ "R1A",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R1B",   SAMPLE_NOLOOP },
	{ "R1C",   SAMPLE_NOLOOP },
	{ "R1D",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R1E",   SAMPLE_NOLOOP },
	{ "R1F",   SAMPLE_NOLOOP },
	{ "R20",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R21",   SAMPLE_NOLOOP },
	{ "R22",   SAMPLE_NOLOOP },
	{ "R23",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R24",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R25",   SAMPLE_NOLOOP },
	{ "R26",   SAMPLE_NOLOOP },
	{ "R27",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R28",   SAMPLE_NOLOOP },
	{ "R29",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R2A",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R2B",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R2C",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R2D",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R2E",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R2F",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R30",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R31",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R32",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R33",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R34",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R35",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R36",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R37",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R38",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R39",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R3A",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R3B",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R3C",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R3D",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R3E",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "R3F",   SAMPLE_NOLOOP | SAMPLE_NODUMP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Sfz3mix)
STD_SAMPLE_FN(Sfz3mix)

static struct BurnRomInfo Sfz3mixRomDesc[] = {
	{ "sz3mx.03",		0x0600000, 0x7a968da6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "sz3mx.13m",		0x2000000, 0x5389b651, CPS2_GFX | BRF_GRA },
	{ "sz3mx.21m",		0x2000000, 0x914c7031, CPS2_GFX | BRF_GRA },

	{ "sz3mx.01",		0x0080000, 0x52f98a5c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "sz3mx.11m",		0x1000000, 0x395edce2, CPS2_QSND | BRF_SND },

	{ "phoenix.key",	0x0000014, 0x2cf772b0, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Sfz3mix)
STD_ROM_FN(Sfz3mix)

struct BurnDriver BurnDrvCpsSfz3mix = {
	"sfz3mix", "sfa3", NULL, "sfz3mix", "2026",
	"Street Fighter Zero 3 Mix v0.32\0", NULL, "hack (zero800)", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK | BDF_HISCORE_SUPPORTED, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Sfz3mixRomInfo, Sfz3mixRomName, NULL, NULL, Sfz3mixSampleInfo, Sfz3mixSampleName, Sfz3mixInputInfo, Sfz3mixDIPInfo,
	Cps2MixInit, Cps2MixExit, Cps2MixFrame, CpsRedraw, CpsAreaScan,
	&CpsRecalcPal, 0x1000, 416, 234, 16, 9
};


// X-Men Vs. Street Fighter 3v3 + DHC (Hack, v1.1.0)
// Modified by Morphus56K
// 2026-10-11
// https://www.morphus56k.com/blog/xmvsf-three-on-three
// Three characters a team, Delayed Hyper Combos, snapbacks and eighteen colors per character.
// The 6 player sets put P1-P3 against P4-P6; the wide sets show a 512 pixel wide arena.
// All four need the board additions in xmvsf3v3.h (inputs for six players, extra work RAM,
// HUD portraits in the chosen color).

static struct BurnInputInfo Xmvsf3v3InputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsXmvSixButtons[0] + 12, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsXmvSixButtons[0] + 11, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsXmvSixButtons[0] + 3 , "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsXmvSixButtons[0] + 2 , "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsXmvSixButtons[0] + 1 , "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsXmvSixButtons[0] + 0 , "p1 right"  },
	{"P1 Weak Punch"    , BIT_DIGITAL  , CpsXmvSixButtons[0] + 4 , "p1 fire 1" },
	{"P1 Medium Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[0] + 5 , "p1 fire 2" },
	{"P1 Strong Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[0] + 6 , "p1 fire 3" },
	{"P1 Weak Kick"     , BIT_DIGITAL  , CpsXmvSixButtons[0] + 8 , "p1 fire 4" },
	{"P1 Medium Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[0] + 9 , "p1 fire 5" },
	{"P1 Strong Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[0] + 10, "p1 fire 6" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsXmvSixButtons[1] + 12, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsXmvSixButtons[1] + 11, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsXmvSixButtons[1] + 3 , "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsXmvSixButtons[1] + 2 , "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsXmvSixButtons[1] + 1 , "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsXmvSixButtons[1] + 0 , "p2 right"  },
	{"P2 Weak Punch"    , BIT_DIGITAL  , CpsXmvSixButtons[1] + 4 , "p2 fire 1" },
	{"P2 Medium Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[1] + 5 , "p2 fire 2" },
	{"P2 Strong Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[1] + 6 , "p2 fire 3" },
	{"P2 Weak Kick"     , BIT_DIGITAL  , CpsXmvSixButtons[1] + 8 , "p2 fire 4" },
	{"P2 Medium Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[1] + 9 , "p2 fire 5" },
	{"P2 Strong Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[1] + 10, "p2 fire 6" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset               , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1             , "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2             , "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp              , "volumeup"  },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn             , "volumedown"},
};

STDINPUTINFO(Xmvsf3v3)

static struct BurnInputInfo Xmvsf6pInputList[] = {
	{"P1 Coin"          , BIT_DIGITAL  , CpsXmvSixButtons[0] + 12, "p1 coin"   },
	{"P1 Start"         , BIT_DIGITAL  , CpsXmvSixButtons[0] + 11, "p1 start"  },
	{"P1 Up"            , BIT_DIGITAL  , CpsXmvSixButtons[0] + 3 , "p1 up"     },
	{"P1 Down"          , BIT_DIGITAL  , CpsXmvSixButtons[0] + 2 , "p1 down"   },
	{"P1 Left"          , BIT_DIGITAL  , CpsXmvSixButtons[0] + 1 , "p1 left"   },
	{"P1 Right"         , BIT_DIGITAL  , CpsXmvSixButtons[0] + 0 , "p1 right"  },
	{"P1 Weak Punch"    , BIT_DIGITAL  , CpsXmvSixButtons[0] + 4 , "p1 fire 1" },
	{"P1 Medium Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[0] + 5 , "p1 fire 2" },
	{"P1 Strong Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[0] + 6 , "p1 fire 3" },
	{"P1 Weak Kick"     , BIT_DIGITAL  , CpsXmvSixButtons[0] + 8 , "p1 fire 4" },
	{"P1 Medium Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[0] + 9 , "p1 fire 5" },
	{"P1 Strong Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[0] + 10, "p1 fire 6" },

	{"P2 Coin"          , BIT_DIGITAL  , CpsXmvSixButtons[1] + 12, "p2 coin"   },
	{"P2 Start"         , BIT_DIGITAL  , CpsXmvSixButtons[1] + 11, "p2 start"  },
	{"P2 Up"            , BIT_DIGITAL  , CpsXmvSixButtons[1] + 3 , "p2 up"     },
	{"P2 Down"          , BIT_DIGITAL  , CpsXmvSixButtons[1] + 2 , "p2 down"   },
	{"P2 Left"          , BIT_DIGITAL  , CpsXmvSixButtons[1] + 1 , "p2 left"   },
	{"P2 Right"         , BIT_DIGITAL  , CpsXmvSixButtons[1] + 0 , "p2 right"  },
	{"P2 Weak Punch"    , BIT_DIGITAL  , CpsXmvSixButtons[1] + 4 , "p2 fire 1" },
	{"P2 Medium Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[1] + 5 , "p2 fire 2" },
	{"P2 Strong Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[1] + 6 , "p2 fire 3" },
	{"P2 Weak Kick"     , BIT_DIGITAL  , CpsXmvSixButtons[1] + 8 , "p2 fire 4" },
	{"P2 Medium Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[1] + 9 , "p2 fire 5" },
	{"P2 Strong Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[1] + 10, "p2 fire 6" },

	{"P3 Coin"          , BIT_DIGITAL  , CpsXmvSixButtons[2] + 12, "p3 coin"   },
	{"P3 Start"         , BIT_DIGITAL  , CpsXmvSixButtons[2] + 11, "p3 start"  },
	{"P3 Up"            , BIT_DIGITAL  , CpsXmvSixButtons[2] + 3 , "p3 up"     },
	{"P3 Down"          , BIT_DIGITAL  , CpsXmvSixButtons[2] + 2 , "p3 down"   },
	{"P3 Left"          , BIT_DIGITAL  , CpsXmvSixButtons[2] + 1 , "p3 left"   },
	{"P3 Right"         , BIT_DIGITAL  , CpsXmvSixButtons[2] + 0 , "p3 right"  },
	{"P3 Weak Punch"    , BIT_DIGITAL  , CpsXmvSixButtons[2] + 4 , "p3 fire 1" },
	{"P3 Medium Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[2] + 5 , "p3 fire 2" },
	{"P3 Strong Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[2] + 6 , "p3 fire 3" },
	{"P3 Weak Kick"     , BIT_DIGITAL  , CpsXmvSixButtons[2] + 8 , "p3 fire 4" },
	{"P3 Medium Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[2] + 9 , "p3 fire 5" },
	{"P3 Strong Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[2] + 10, "p3 fire 6" },

	{"P4 Coin"          , BIT_DIGITAL  , CpsXmvSixButtons[3] + 12, "p4 coin"   },
	{"P4 Start"         , BIT_DIGITAL  , CpsXmvSixButtons[3] + 11, "p4 start"  },
	{"P4 Up"            , BIT_DIGITAL  , CpsXmvSixButtons[3] + 3 , "p4 up"     },
	{"P4 Down"          , BIT_DIGITAL  , CpsXmvSixButtons[3] + 2 , "p4 down"   },
	{"P4 Left"          , BIT_DIGITAL  , CpsXmvSixButtons[3] + 1 , "p4 left"   },
	{"P4 Right"         , BIT_DIGITAL  , CpsXmvSixButtons[3] + 0 , "p4 right"  },
	{"P4 Weak Punch"    , BIT_DIGITAL  , CpsXmvSixButtons[3] + 4 , "p4 fire 1" },
	{"P4 Medium Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[3] + 5 , "p4 fire 2" },
	{"P4 Strong Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[3] + 6 , "p4 fire 3" },
	{"P4 Weak Kick"     , BIT_DIGITAL  , CpsXmvSixButtons[3] + 8 , "p4 fire 4" },
	{"P4 Medium Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[3] + 9 , "p4 fire 5" },
	{"P4 Strong Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[3] + 10, "p4 fire 6" },

	{"P5 Coin"          , BIT_DIGITAL  , CpsXmvSixButtons[4] + 12, "p5 coin"   },
	{"P5 Start"         , BIT_DIGITAL  , CpsXmvSixButtons[4] + 11, "p5 start"  },
	{"P5 Up"            , BIT_DIGITAL  , CpsXmvSixButtons[4] + 3 , "p5 up"     },
	{"P5 Down"          , BIT_DIGITAL  , CpsXmvSixButtons[4] + 2 , "p5 down"   },
	{"P5 Left"          , BIT_DIGITAL  , CpsXmvSixButtons[4] + 1 , "p5 left"   },
	{"P5 Right"         , BIT_DIGITAL  , CpsXmvSixButtons[4] + 0 , "p5 right"  },
	{"P5 Weak Punch"    , BIT_DIGITAL  , CpsXmvSixButtons[4] + 4 , "p5 fire 1" },
	{"P5 Medium Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[4] + 5 , "p5 fire 2" },
	{"P5 Strong Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[4] + 6 , "p5 fire 3" },
	{"P5 Weak Kick"     , BIT_DIGITAL  , CpsXmvSixButtons[4] + 8 , "p5 fire 4" },
	{"P5 Medium Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[4] + 9 , "p5 fire 5" },
	{"P5 Strong Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[4] + 10, "p5 fire 6" },

	{"P6 Coin"          , BIT_DIGITAL  , CpsXmvSixButtons[5] + 12, "p6 coin"   },
	{"P6 Start"         , BIT_DIGITAL  , CpsXmvSixButtons[5] + 11, "p6 start"  },
	{"P6 Up"            , BIT_DIGITAL  , CpsXmvSixButtons[5] + 3 , "p6 up"     },
	{"P6 Down"          , BIT_DIGITAL  , CpsXmvSixButtons[5] + 2 , "p6 down"   },
	{"P6 Left"          , BIT_DIGITAL  , CpsXmvSixButtons[5] + 1 , "p6 left"   },
	{"P6 Right"         , BIT_DIGITAL  , CpsXmvSixButtons[5] + 0 , "p6 right"  },
	{"P6 Weak Punch"    , BIT_DIGITAL  , CpsXmvSixButtons[5] + 4 , "p6 fire 1" },
	{"P6 Medium Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[5] + 5 , "p6 fire 2" },
	{"P6 Strong Punch"  , BIT_DIGITAL  , CpsXmvSixButtons[5] + 6 , "p6 fire 3" },
	{"P6 Weak Kick"     , BIT_DIGITAL  , CpsXmvSixButtons[5] + 8 , "p6 fire 4" },
	{"P6 Medium Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[5] + 9 , "p6 fire 5" },
	{"P6 Strong Kick"   , BIT_DIGITAL  , CpsXmvSixButtons[5] + 10, "p6 fire 6" },

	{"Reset"            , BIT_DIGITAL  , &CpsReset               , "reset"     },
	{"Diagnostic"       , BIT_DIGITAL  , CpsInp021+1             , "diag"      },
	{"Service"          , BIT_DIGITAL  , CpsInp021+2             , "service"   },
	{"Volume Up"        , BIT_DIGITAL  , &Cps2VolUp              , "volumeup"  },
	{"Volume Down"      , BIT_DIGITAL  , &Cps2VolDwn             , "volumedown"},
};

STDINPUTINFO(Xmvsf6p)

static INT32 Xmvsf3v3Init()
{
	return CpsXmvSixInit(0);
}

static INT32 Xmvsf3v3WideInit()
{
	return CpsXmvSixInit(1);
}

static struct BurnRomInfo Xmvsf3v3RomDesc[] = {
	{ "xvs3v3.03f",   0x080000, 0x7b151acd, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvse.04f",     0x080000, 0xef015aef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",      0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",      0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",       0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs3v3.08",    0x080000, 0x477a3f86, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs3v3.09",    0x080000, 0x98e06515, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs3v3.13m",   0x400000, 0xc8b777f6, CPS2_GFX | BRF_GRA },
	{ "xvs3v3.15m",   0x400000, 0x8143c2b0, CPS2_GFX | BRF_GRA },
	{ "xvs3v3.17m",   0x400000, 0x10cceec9, CPS2_GFX | BRF_GRA },
	{ "xvs3v3.19m",   0x400000, 0xf458a1f8, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",      0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",      0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",      0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",      0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",       0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",       0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",      0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",      0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },

	{ "xmvsf.key",    0x000014, 0xd5c07311, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsf3v3)
STD_ROM_FN(Xmvsf3v3)

static struct BurnRomInfo Xmvsf3v3wRomDesc[] = {
	{ "xvs3v3w.03f",  0x080000, 0x54ed459e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvse.04f",     0x080000, 0xef015aef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",      0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",      0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",       0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs3v3.08",    0x080000, 0x477a3f86, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs3v3w.09",   0x080000, 0x145e92b6, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs3v3.13m",   0x400000, 0xc8b777f6, CPS2_GFX | BRF_GRA },
	{ "xvs3v3.15m",   0x400000, 0x8143c2b0, CPS2_GFX | BRF_GRA },
	{ "xvs3v3.17m",   0x400000, 0x10cceec9, CPS2_GFX | BRF_GRA },
	{ "xvs3v3.19m",   0x400000, 0xf458a1f8, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",      0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",      0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",      0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",      0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",       0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",       0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",      0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",      0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },

	{ "xmvsf.key",    0x000014, 0xd5c07311, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsf3v3w)
STD_ROM_FN(Xmvsf3v3w)

static struct BurnRomInfo Xmvsf6pRomDesc[] = {
	{ "xvs6p.03f",    0x080000, 0x8940699a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvse.04f",     0x080000, 0xef015aef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",      0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",      0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",       0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs6p.08",     0x080000, 0x14b6a689, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs6p.09",     0x080000, 0x6ea8fdea, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs3v3.13m",   0x400000, 0xc8b777f6, CPS2_GFX | BRF_GRA },
	{ "xvs3v3.15m",   0x400000, 0x8143c2b0, CPS2_GFX | BRF_GRA },
	{ "xvs3v3.17m",   0x400000, 0x10cceec9, CPS2_GFX | BRF_GRA },
	{ "xvs3v3.19m",   0x400000, 0xf458a1f8, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",      0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",      0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",      0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",      0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",       0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",       0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",      0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",      0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },

	{ "xmvsf.key",    0x000014, 0xd5c07311, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsf6p)
STD_ROM_FN(Xmvsf6p)

static struct BurnRomInfo Xmvsf6pwRomDesc[] = {
	{ "xvs6pw.03f",   0x080000, 0xa6b836c9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvse.04f",     0x080000, 0xef015aef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.05a",      0x080000, 0x7db6025d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.06a",      0x080000, 0xe8e2c75c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs.07",       0x080000, 0x08f0abed, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs6p.08",     0x080000, 0x14b6a689, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "xvs6pw.09",    0x080000, 0xe2160a49, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "xvs3v3.13m",   0x400000, 0xc8b777f6, CPS2_GFX | BRF_GRA },
	{ "xvs3v3.15m",   0x400000, 0x8143c2b0, CPS2_GFX | BRF_GRA },
	{ "xvs3v3.17m",   0x400000, 0x10cceec9, CPS2_GFX | BRF_GRA },
	{ "xvs3v3.19m",   0x400000, 0xf458a1f8, CPS2_GFX | BRF_GRA },
	{ "xvs.14m",      0x400000, 0xbcac2e41, CPS2_GFX | BRF_GRA },
	{ "xvs.16m",      0x400000, 0xea04a272, CPS2_GFX | BRF_GRA },
	{ "xvs.18m",      0x400000, 0xb0def86a, CPS2_GFX | BRF_GRA },
	{ "xvs.20m",      0x400000, 0x4b40ff9f, CPS2_GFX | BRF_GRA },

	{ "xvs.01",       0x020000, 0x3999e93a, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "xvs.02",       0x020000, 0x101bdee9, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "xvs.11m",      0x200000, 0x9cadcdbc, CPS2_QSND | BRF_SND },
	{ "xvs.12m",      0x200000, 0x7b11e460, CPS2_QSND | BRF_SND },

	{ "xmvsf.key",    0x000014, 0xd5c07311, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Xmvsf6pw)
STD_ROM_FN(Xmvsf6pw)

struct BurnDriver BurnDrvCpsXmvsf3v3 = {
	"xmvsf3v3", "xmvsf", NULL, NULL, "2026",
	"X-Men Vs. Street Fighter 3v3 + DHC (Hack, v1.1.0)\0", NULL, "hack (Morphus56K)", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsf3v3RomInfo, Xmvsf3v3RomName, NULL, NULL, NULL, NULL, Xmvsf3v3InputInfo, NULL,
	Xmvsf3v3Init, CpsXmvSixExit, Cps2Frame, CpsRedraw, CpsXmvSixScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsf3v3w = {
	"xmvsf3v3w", "xmvsf", NULL, NULL, "2026",
	"X-Men Vs. Street Fighter 3v3 + DHC (Hack, v1.1.0, wide)\0", NULL, "hack (Morphus56K)", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsf3v3wRomInfo, Xmvsf3v3wRomName, NULL, NULL, NULL, NULL, Xmvsf3v3InputInfo, NULL,
	Xmvsf3v3WideInit, CpsXmvSixExit, Cps2Frame, CpsRedraw, CpsXmvSixScan,
	&CpsRecalcPal, 0x1000, 512, 224, 16, 9
};

struct BurnDriver BurnDrvCpsXmvsf6p = {
	"xmvsf6p", "xmvsf", NULL, NULL, "2026",
	"X-Men Vs. Street Fighter 3v3 + DHC (Hack, v1.1.0, 6 players)\0", NULL, "hack (Morphus56K)", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 6, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsf6pRomInfo, Xmvsf6pRomName, NULL, NULL, NULL, NULL, Xmvsf6pInputInfo, NULL,
	Xmvsf3v3Init, CpsXmvSixExit, Cps2Frame, CpsRedraw, CpsXmvSixScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

struct BurnDriver BurnDrvCpsXmvsf6pw = {
	"xmvsf6pw", "xmvsf", NULL, NULL, "2026",
	"X-Men Vs. Street Fighter 3v3 + DHC (Hack, v1.1.0, 6 players, wide)\0", NULL, "hack (Morphus56K)", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 6, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_SF,
	NULL, Xmvsf6pwRomInfo, Xmvsf6pwRomName, NULL, NULL, NULL, NULL, Xmvsf6pInputInfo, NULL,
	Xmvsf3v3WideInit, CpsXmvSixExit, Cps2Frame, CpsRedraw, CpsXmvSixScan,
	&CpsRecalcPal, 0x1000, 512, 224, 16, 9
};


static INT32 VampireArrangeInit()
{
	return Cps2PlusInit(24);
}

// Vampire Savior Arrange (Hack, English RC4)

static struct BurnRomInfo Vsavs01RomDesc[] = {
	{ "c71s01.p1",		0x080000, 0x1f2f188a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c71s01.p2",		0x080000, 0x8926858a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c71s01.p3",		0x080000, 0x2b02f280, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3j.06b",		0x080000, 0x2c4297e0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c71s01.p5",		0x080000, 0x7854576e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3j.08a",		0x080000, 0x5773e5c9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c71s01.p7",		0x080000, 0x6428a025, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c71s01.p8",		0x080000, 0x29316daa, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c71s01.p9",		0x400000, 0x7399ea53, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "c71s01.c1",		0x400000, 0x6a3fe97b, CPS2_GFX | BRF_GRA },
	{ "c71s01.c2",		0x400000, 0xd64c3c03, CPS2_GFX | BRF_GRA },
	{ "c71s01.c3",		0x400000, 0x93423093, CPS2_GFX | BRF_GRA },
	{ "c71s01.c4",		0x400000, 0x76f89f2b, CPS2_GFX | BRF_GRA },
	{ "vm3.14m",		0x400000, 0xc1a28e6c, CPS2_GFX | BRF_GRA },
	{ "vm3.16m",		0x400000, 0x194a7304, CPS2_GFX | BRF_GRA },
	{ "vm3.18m",		0x400000, 0xdf9a9f47, CPS2_GFX | BRF_GRA },
	{ "vm3.20m",		0x400000, 0xc22fc3d9, CPS2_GFX | BRF_GRA },
	{ "c71s01.c9",		0x200000, 0xd015d692, CPS2_GFX | BRF_GRA },
	{ "c71s01.c10",		0x200000, 0x0fa94d8b, CPS2_GFX | BRF_GRA },
	{ "c71s01.c11",		0x200000, 0x07f100d0, CPS2_GFX | BRF_GRA },
	{ "c71s01.c12",		0x200000, 0x1ce74d45, CPS2_GFX | BRF_GRA },

	{ "c71s01.m1",		0x020000, 0xb866c03c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vm3.02",		0x020000, 0xcc09faa1, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "c71s01.m3",		0x040000, 0xad73c4e2, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vm3.11m",		0x400000, 0xe80e956e, CPS2_QSND | BRF_SND },
	{ "vm3.12m",		0x400000, 0x9cd71557, CPS2_QSND | BRF_SND },
	{ "c71s01.q3",		0x800000, 0xfd3a78ec, CPS2_QSND | BRF_SND },

	{ "vsavj.key",		0x000014, 0x36d28ab8, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vsavs01)
STD_ROM_FN(Vsavs01)

struct BurnDriver BurnDrvCpsVsavs01 = {
	"vsavs01", "vsav", NULL, NULL, "2026",
	"Vampire Savior Arrange (Hack, English RC4)\0", NULL, "Strygo", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vsavs01RomInfo, Vsavs01RomName, NULL, NULL, NULL, NULL, Cps2CustomInputInfo, NULL,
	VampireArrangeInit, Cps2PlusExit, Cps2Frame, CpsRedraw, Cps2PlusScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

// Vampire Savior Arrange (Hack, Japan RC4)

static struct BurnRomInfo Vsavs02RomDesc[] = {
	{ "c71s02.p1",		0x080000, 0x2924c56a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c71s02.p2",		0x080000, 0x326ca702, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c71s02.p3",		0x080000, 0x2b02f280, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3j.06b",		0x080000, 0x2c4297e0, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c71s02.p5",		0x080000, 0x8791d88c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vm3j.08a",		0x080000, 0x5773e5c9, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c71s02.p7",		0x080000, 0x6428a025, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c71s02.p8",		0x080000, 0x29316daa, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c71s02.p9",		0x400000, 0x3670226d, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "c71s02.c1",		0x400000, 0x6a3fe97b, CPS2_GFX | BRF_GRA },
	{ "c71s02.c2",		0x400000, 0xd64c3c03, CPS2_GFX | BRF_GRA },
	{ "c71s02.c3",		0x400000, 0x93423093, CPS2_GFX | BRF_GRA },
	{ "c71s02.c4",		0x400000, 0x76f89f2b, CPS2_GFX | BRF_GRA },
	{ "vm3.14m",		0x400000, 0xc1a28e6c, CPS2_GFX | BRF_GRA },
	{ "vm3.16m",		0x400000, 0x194a7304, CPS2_GFX | BRF_GRA },
	{ "vm3.18m",		0x400000, 0xdf9a9f47, CPS2_GFX | BRF_GRA },
	{ "vm3.20m",		0x400000, 0xc22fc3d9, CPS2_GFX | BRF_GRA },
	{ "c71s02.c9",		0x200000, 0xd015d692, CPS2_GFX | BRF_GRA },
	{ "c71s02.c10",		0x200000, 0x0fa94d8b, CPS2_GFX | BRF_GRA },
	{ "c71s02.c11",		0x200000, 0x07f100d0, CPS2_GFX | BRF_GRA },
	{ "c71s02.c12",		0x200000, 0x1ce74d45, CPS2_GFX | BRF_GRA },

	{ "c71s02.m1",		0x020000, 0xb866c03c, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vm3.02",		0x020000, 0xcc09faa1, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "c71s02.m3",		0x040000, 0xad73c4e2, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vm3.11m",		0x400000, 0xe80e956e, CPS2_QSND | BRF_SND },
	{ "vm3.12m",		0x400000, 0x9cd71557, CPS2_QSND | BRF_SND },
	{ "c71s02.q3",		0x800000, 0xfd3a78ec, CPS2_QSND | BRF_SND },

	{ "vsavj.key",		0x000014, 0x36d28ab8, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vsavs02)
STD_ROM_FN(Vsavs02)

struct BurnDriver BurnDrvCpsVsavs02 = {
	"vsavs02", "vsav", NULL, NULL, "2026",
	"Vampire Savior Arrange (Hack, Japan RC4)\0", NULL, "Strygo", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vsavs02RomInfo, Vsavs02RomName, NULL, NULL, NULL, NULL, Cps2CustomInputInfo, NULL,
	VampireArrangeInit, Cps2PlusExit, Cps2Frame, CpsRedraw, Cps2PlusScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

// Vampire Savior 2 Arrange (Hack, English RC3)

static struct BurnRomInfo Vsav2s05RomDesc[] = {
	{ "c76s05.p1",		0x080000, 0xeeefba15, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c76s05.p2",		0x080000, 0xe39be4dc, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c76s05.p3",		0x080000, 0x5fbed75f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c76s05.p4",		0x080000, 0x253b76ef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.07",		0x080000, 0x8f885809, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c76s05.p6",		0x080000, 0x4e1b8eae, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.09",		0x080000, 0xfac3c217, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c76s05.p8",		0x080000, 0x1e8a3df5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c76s05.p9",		0x400000, 0xd524fdaa, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "c76s05.c1",		0x400000, 0xb0c5665b, CPS2_GFX | BRF_GRA },
	{ "c76s05.c2",		0x400000, 0xc5b6049f, CPS2_GFX | BRF_GRA },
	{ "c76s05.c3",		0x400000, 0xa6448812, CPS2_GFX | BRF_GRA },
	{ "c76s05.c4",		0x400000, 0x84067f87, CPS2_GFX | BRF_GRA },
	{ "vs2.14m",		0x400000, 0xcd09bd63, CPS2_GFX | BRF_GRA },
	{ "vs2.16m",		0x400000, 0xe0182c15, CPS2_GFX | BRF_GRA },
	{ "vs2.18m",		0x400000, 0x778dc4f6, CPS2_GFX | BRF_GRA },
	{ "vs2.20m",		0x400000, 0x605d9d1d, CPS2_GFX | BRF_GRA },
	{ "c76s05.c9",		0x400000, 0xa45e1119, CPS2_GFX | BRF_GRA },
	{ "c76s05.c10",		0x400000, 0x8b6832d1, CPS2_GFX | BRF_GRA },
	{ "c76s05.c11",		0x400000, 0x58cacc2f, CPS2_GFX | BRF_GRA },
	{ "c76s05.c12",		0x400000, 0x3d1408fe, CPS2_GFX | BRF_GRA },
	{ "c76s05.c13",		0x400000, 0x7d5b6975, CPS2_GFX | BRF_GRA },
	{ "c76s05.c14",		0x400000, 0x7d5b6975, CPS2_GFX | BRF_GRA },
	{ "c76s05.c15",		0x400000, 0x7d5b6975, CPS2_GFX | BRF_GRA },
	{ "c76s05.c16",		0x400000, 0x7d5b6975, CPS2_GFX | BRF_GRA },

	{ "c76s05.m1",		0x020000, 0x880f16d1, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vs2.02",		0x020000, 0xc32dba09, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vs2.11m",		0x400000, 0xd67e47b7, CPS2_QSND | BRF_SND },
	{ "vs2.12m",		0x400000, 0x6d020a14, CPS2_QSND | BRF_SND },
	{ "c76s05.q3",		0x800000, 0xeb50211e, CPS2_QSND | BRF_SND },

	{ "vsav2.key",		0x000014, 0x289028ce, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vsav2s05)
STD_ROM_FN(Vsav2s05)

struct BurnDriver BurnDrvCpsVsav2s05 = {
	"vsav2s05", "vsav2", NULL, NULL, "2026",
	"Vampire Savior 2 Arrange (Hack, English RC3)\0", NULL, "Strygo", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vsav2s05RomInfo, Vsav2s05RomName, NULL, NULL, NULL, NULL, Cps2CustomInputInfo, NULL,
	VampireArrangeInit, Cps2PlusExit, Cps2Frame, CpsRedraw, Cps2PlusScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

// Vampire Savior 2 Arrange (Hack, Japan RC3)

static struct BurnRomInfo Vsav2s06RomDesc[] = {
	{ "c76s06.p1",		0x080000, 0x300870ae, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c76s06.p2",		0x080000, 0xcfe4a7b2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c76s06.p3",		0x080000, 0x5fbed75f, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c76s06.p4",		0x080000, 0x253b76ef, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.07",		0x080000, 0x8f885809, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c76s06.p6",		0x080000, 0x7bc795c2, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "vs2j.09",		0x080000, 0xfac3c217, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c76s06.p8",		0x080000, 0x1e8a3df5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c76s06.p9",		0x400000, 0x1421c60b, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "c76s06.c1",		0x400000, 0xb7751dee, CPS2_GFX | BRF_GRA },
	{ "c76s06.c2",		0x400000, 0xa6dcea63, CPS2_GFX | BRF_GRA },
	{ "c76s06.c3",		0x400000, 0x968eeeb0, CPS2_GFX | BRF_GRA },
	{ "c76s06.c4",		0x400000, 0x591944a1, CPS2_GFX | BRF_GRA },
	{ "vs2.14m",		0x400000, 0xcd09bd63, CPS2_GFX | BRF_GRA },
	{ "vs2.16m",		0x400000, 0xe0182c15, CPS2_GFX | BRF_GRA },
	{ "vs2.18m",		0x400000, 0x778dc4f6, CPS2_GFX | BRF_GRA },
	{ "vs2.20m",		0x400000, 0x605d9d1d, CPS2_GFX | BRF_GRA },
	{ "c76s06.c9",		0x400000, 0xa45e1119, CPS2_GFX | BRF_GRA },
	{ "c76s06.c10",		0x400000, 0x8b6832d1, CPS2_GFX | BRF_GRA },
	{ "c76s06.c11",		0x400000, 0x58cacc2f, CPS2_GFX | BRF_GRA },
	{ "c76s06.c12",		0x400000, 0x3d1408fe, CPS2_GFX | BRF_GRA },
	{ "c76s06.c13",		0x400000, 0x7d5b6975, CPS2_GFX | BRF_GRA },
	{ "c76s06.c14",		0x400000, 0x7d5b6975, CPS2_GFX | BRF_GRA },
	{ "c76s06.c15",		0x400000, 0x7d5b6975, CPS2_GFX | BRF_GRA },
	{ "c76s06.c16",		0x400000, 0x7d5b6975, CPS2_GFX | BRF_GRA },

	{ "c76s06.m1",		0x020000, 0x880f16d1, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "vs2.02",		0x020000, 0xc32dba09, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vs2.11m",		0x400000, 0xd67e47b7, CPS2_QSND | BRF_SND },
	{ "vs2.12m",		0x400000, 0x6d020a14, CPS2_QSND | BRF_SND },
	{ "c76s06.q3",		0x800000, 0xeb50211e, CPS2_QSND | BRF_SND },

	{ "vsav2.key",		0x000014, 0x289028ce, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vsav2s06)
STD_ROM_FN(Vsav2s06)

struct BurnDriver BurnDrvCpsVsav2s06 = {
	"vsav2s06", "vsav2", NULL, NULL, "2026",
	"Vampire Savior 2 Arrange (Hack, Japan RC3)\0", NULL, "Strygo", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vsav2s06RomInfo, Vsav2s06RomName, NULL, NULL, NULL, NULL, Cps2CustomInputInfo, NULL,
	VampireArrangeInit, Cps2PlusExit, Cps2Frame, CpsRedraw, Cps2PlusScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

// Night Warriors 2 Arrange (Hack, English RC3)

static struct BurnRomInfo Vhunt2s02RomDesc[] = {
	{ "c75s02.p1",		0x080000, 0x9354e632, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s02.p2",		0x080000, 0x7f10f35e, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s02.p3",		0x080000, 0x87edf306, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s02.p4",		0x080000, 0x94118a84, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s02.p5",		0x080000, 0xa324ffeb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s02.p6",		0x080000, 0x17b37f5a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s02.p7",		0x080000, 0xf3159a3a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s02.p8",		0x080000, 0xecd2d939, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s02.p9",		0x400000, 0x673d43b1, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "c75s02.c1",		0x400000, 0x82e825c6, CPS2_GFX | BRF_GRA },
	{ "c75s02.c2",		0x400000, 0x678feab2, CPS2_GFX | BRF_GRA },
	{ "c75s02.c3",		0x400000, 0xb9e1cbf8, CPS2_GFX | BRF_GRA },
	{ "c75s02.c4",		0x400000, 0x68d603b4, CPS2_GFX | BRF_GRA },
	{ "vh2.14m",		0x400000, 0xcd09bd63, CPS2_GFX | BRF_GRA },
	{ "vh2.16m",		0x400000, 0xe0182c15, CPS2_GFX | BRF_GRA },
	{ "vh2.18m",		0x400000, 0x778dc4f6, CPS2_GFX | BRF_GRA },
	{ "vh2.20m",		0x400000, 0x605d9d1d, CPS2_GFX | BRF_GRA },
	{ "c75s02.c9",		0x200000, 0xd7d4fecc, CPS2_GFX | BRF_GRA },
	{ "c75s02.c10",		0x200000, 0x95c0d576, CPS2_GFX | BRF_GRA },
	{ "c75s02.c11",		0x200000, 0x66a27497, CPS2_GFX | BRF_GRA },
	{ "c75s02.c12",		0x200000, 0x958494ec, CPS2_GFX | BRF_GRA },

	{ "c75s02.m1",		0x020000, 0xf6088d87, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "c75s02.m2",		0x020000, 0x7c4fae46, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "c75s02.m3",		0x040000, 0x04ef903d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vh2.11m",		0x400000, 0x38922efd, CPS2_QSND | BRF_SND },
	{ "vh2.12m",		0x400000, 0x6e2430af, CPS2_QSND | BRF_SND },
	{ "c75s02.q3",		0x800000, 0x24813909, CPS2_QSND | BRF_SND },

	{ "vhunt2.key",		0x000014, 0x61306b20, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vhunt2s02)
STD_ROM_FN(Vhunt2s02)

struct BurnDriver BurnDrvCpsVhunt2s02 = {
	"vhunt2s02", "vhunt2", NULL, NULL, "2026",
	"Night Warriors 2 Arrange (Hack, English RC3)\0", NULL, "Strygo", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vhunt2s02RomInfo, Vhunt2s02RomName, NULL, NULL, NULL, NULL, Cps2CustomInputInfo, NULL,
	VampireArrangeInit, Cps2PlusExit, Cps2Frame, CpsRedraw, Cps2PlusScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};

// Vampire Hunter 2 Arrange (Hack, Japan RC3)

static struct BurnRomInfo Vhunt2s03RomDesc[] = {
	{ "c75s03.p1",		0x080000, 0x7797e2a4, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s03.p2",		0x080000, 0xc0703475, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s03.p3",		0x080000, 0x87edf306, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s03.p4",		0x080000, 0x94118a84, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s03.p5",		0x080000, 0xa324ffeb, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s03.p6",		0x080000, 0x79dbd15c, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s03.p7",		0x080000, 0xf3159a3a, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s03.p8",		0x080000, 0xecd2d939, CPS2_PRG_68K | BRF_ESS | BRF_PRG },
	{ "c75s03.p9",		0x400000, 0x2d91d6a5, CPS2_PRG_68K | BRF_ESS | BRF_PRG },

	{ "c75s03.c1",		0x400000, 0x6a3d7b6a, CPS2_GFX | BRF_GRA },
	{ "c75s03.c2",		0x400000, 0xebcb10da, CPS2_GFX | BRF_GRA },
	{ "c75s03.c3",		0x400000, 0xda8cfcd4, CPS2_GFX | BRF_GRA },
	{ "c75s03.c4",		0x400000, 0x108fd9e0, CPS2_GFX | BRF_GRA },
	{ "vh2.14m",		0x400000, 0xcd09bd63, CPS2_GFX | BRF_GRA },
	{ "vh2.16m",		0x400000, 0xe0182c15, CPS2_GFX | BRF_GRA },
	{ "vh2.18m",		0x400000, 0x778dc4f6, CPS2_GFX | BRF_GRA },
	{ "vh2.20m",		0x400000, 0x605d9d1d, CPS2_GFX | BRF_GRA },
	{ "c75s03.c9",		0x200000, 0x069fd6cc, CPS2_GFX | BRF_GRA },
	{ "c75s03.c10",		0x200000, 0xdfde0582, CPS2_GFX | BRF_GRA },
	{ "c75s03.c11",		0x200000, 0x2438f71b, CPS2_GFX | BRF_GRA },
	{ "c75s03.c12",		0x200000, 0xdf587c66, CPS2_GFX | BRF_GRA },

	{ "c75s03.m1",		0x020000, 0xf6088d87, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "c75s03.m2",		0x020000, 0x7c4fae46, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },
	{ "c75s03.m3",		0x040000, 0x04ef903d, CPS2_PRG_Z80 | BRF_ESS | BRF_PRG },

	{ "vh2.11m",		0x400000, 0x38922efd, CPS2_QSND | BRF_SND },
	{ "vh2.12m",		0x400000, 0x6e2430af, CPS2_QSND | BRF_SND },
	{ "c75s03.q3",		0x800000, 0x24813909, CPS2_QSND | BRF_SND },

	{ "vhunt2.key",		0x000014, 0x61306b20, CPS2_ENCRYPTION_KEY },
};

STD_ROM_PICK(Vhunt2s03)
STD_ROM_FN(Vhunt2s03)

struct BurnDriver BurnDrvCpsVhunt2s03 = {
	"vhunt2s03", "vhunt2", NULL, NULL, "2026",
	"Vampire Hunter 2 Arrange (Hack, Japan RC3)\0", NULL, "Strygo", "CPS2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 2, HARDWARE_CAPCOM_CPS2, GBF_VSFIGHT, FBF_DSTLK,
	NULL, Vhunt2s03RomInfo, Vhunt2s03RomName, NULL, NULL, NULL, NULL, Cps2CustomInputInfo, NULL,
	VampireArrangeInit, Cps2PlusExit, Cps2Frame, CpsRedraw, Cps2PlusScan,
	&CpsRecalcPal, 0x1000, 384, 224, 4, 3
};
